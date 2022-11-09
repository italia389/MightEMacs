// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// eval.c	High-level expression evaluation routines for MightEMacs.

#include "std.h"
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "cxl/lib.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "file.h"
#include "search.h"
#include "var.h"

// Definitions for fmtarg() and strfmt().
#define FMTBufSz	32		// Size of temporary buffer.

// Flags.
#define FMTleft		0x0001		// Left-justify.
#define	FMTplus		0x0002		// Force plus (+) sign.
#define	FMTspc		0x0004		// Use ' ' for plus sign.
#define FMThash		0x0008		// Alternate conversion.
#define	FMTlong		0x0010		// 'l' flag.
#define FMT0pad		0x0020		// '0' flag.
#define FMTprec		0x0040		// Precision was specified.
#define FMTxuc		0x0080		// Use upper case hex letters.

// Control record used to process array arguments.
typedef struct {
	Array *pArray;
	ArraySize i;
	} ArrayState;

// Definitions for isClass? system function.  The isxxx() facilities may be macros, so wrap them into functions.
bool isalnum_func(short c) { return isalnum(c); }
bool isalpha_func(short c) { return isalpha(c); }
bool isblank_func(short c) { return isblank(c); }
bool iscntrl_func(short c) { return iscntrl(c); }
bool isdigit_func(short c) { return isdigit(c); }
bool isgraph_func(short c) { return isgraph(c); }
bool islower_func(short c) { return islower(c); }
bool isprint_func(short c) { return isprint(c); }
bool ispunct_func(short c) { return ispunct(c); }
bool isspace_func(short c) { return isspace(c); }
bool isupper_func(short c) { return isupper(c); }
bool isword_func(short c) { return wordChar[c]; }
bool isxdigit_func(short c) { return isxdigit(c); }

struct CCMap {
	const char *name;
	bool (*func)(short);
	} ccMap[] = {
		{"alnum", &isalnum_func},
		{"alpha", &isalpha_func},
		{"blank", &isblank_func},
		{"cntrl", &iscntrl_func},
		{"digit", &isdigit_func},
		{"graph", &isgraph_func},
		{"lower", &islower_func},
		{"print", &isprint_func},
		{"punct", &ispunct_func},
		{"space", &isspace_func},
		{"upper", &isupper_func},
		{"word", &isword_func},
		{"xdigit", &isxdigit_func},
		{NULL, NULL}};

bool (*getCC(const char *name))(short) {
	struct CCMap *item = ccMap;
	do {
		if(strcmp(name, item->name) == 0)
			return item->func;
		} while((++item)->name != NULL);
	return NULL;
	}

// Return a Datum value as a logical (Boolean) value.
bool toBool(Datum *pDatum) {

	// Check for numeric truth (!= 0).
	if(pDatum->type == dat_int)
		return pDatum->u.intNum != 0;

	// Check for logical false values (false and nil).  All other datum types and values (including null strings) are true.
	return pDatum->type != dat_false && pDatum->type != dat_nil;
	}

// Check if given Datum object is nil or null and return Boolean result.
bool isNN(Datum *pDatum) {

	return disnil(pDatum) || disnull(pDatum);
	}

// Create an array in pRtnVal, given optional size and initializer.  Return status.
int array(Datum *pRtnVal, int n, Datum **args) {
	Array *pArray;
	ArraySize len = 0;
	Datum *pInitializer = NULL;

	// Get array size and initializer, if present.
	if(*args != NULL) {
		len = (*args++)->u.intNum;
		if(*args != NULL)
			pInitializer = *args;
		}
	if((pArray = anew(len, pInitializer)) == NULL)
		return libfail();
	agStash(pRtnVal, pArray);
	return sess.rtn.status;
	}

// Split a string into an array and save in pRtnVal, given delimiter and optional limit value.  If n argument, strip all array
// elements by passing n to stripStr().  Return status.
int strSplit(Datum *pRtnVal, int n, Datum **args) {
	Array *pArray;
	short delimChar;
	const char *str;
	int limit = 0;

	// Get delimiter, string, and optional limit.
	if(disnil(*args))
		delimChar = '\0';
	else if(!isCharVal(*args))
		return sess.rtn.status;
	else
		delimChar = (*args)->u.intNum;
	str = (*++args)->str;
	if(*++args != NULL)
		limit = (*args)->u.intNum;

	if((pArray = asplit(delimChar, str, limit)) == NULL)
		return libfail();
	agStash(pRtnVal, pArray);

	// Strip the elements if requested.
	if(n != INT_MIN) {
		Array *pArray1 = pArray;
		Datum *pDatum;

		while((pDatum = aeach(&pArray1)) != NULL)
			if(dsetstr(stripStr(pDatum->str, n), pDatum) != 0)
				return libfail();
		}

	return sess.rtn.status;
	}

// Force NULL pointer to null string.
char *fixNull(char *s) {

	return (s == NULL) ? "" : s;
	}

// Do range check for hard or soft tab size.
int checkTabSize(int size, bool hard) {

	return ((size != 0 || hard) && (size < 2 || size > MaxTabSize)) ?
	 rsset(Failure, 0, text256, hard ? text49 : text50, size, MaxTabSize) :
		// "%s tab size %d must be between 2 and %d", "Hard", "Soft"
	 sess.rtn.status;
	}

// Set hard or soft tab size and do range check.
int setTabSize(int size, bool hard) {

	// Check if new tab size is valid.
	if(checkTabSize(size, hard) == Success) {

		// Set new size.
		if(hard)
			sess.cur.pScrn->hardTabSize = size;
		else
			sess.cur.pScrn->softTabSize = size;
		(void) rsset(Success, 0, text332, hard ? text49 : text50, size);
				// "%s tab size set to %d", "Hard", "Soft"
		}

	return sess.rtn.status;
	}

// Strip white space off the beginning (op == -1), the end (op == 1), or both ends (op == 0) of a string.
char *stripStr(char *src, int op) {

	// Trim beginning, if applicable...
	if(op <= 0)
		src = nonWhite(src, false);

	// trim end, if applicable...
	if(op >= 0) {
		char *srcEnd = strchr(src, '\0');
		while(srcEnd-- > src)
			if(!isspace(*srcEnd))
				break;
		srcEnd[1] = '\0';
		}

	// and return result.
	return src;
	}

// Copy a replacement pattern to an open fabrication object.  Return -1 if error.
static int replCopy(Match *pMatch, DFab *result) {

	if(pMatch->flags & RRegical)
		for(ReplPat *pReplPat = pMatch->compReplPat; pReplPat != NULL; pReplPat = pReplPat->next) {
			if(dputs(pReplPat->type == RPE_LitString ? pReplPat->u.replStr :
			 pMatch->grpMatch.groups[pReplPat->u.grpNum].str, result, 0) != 0)
				return -1;
			}
	else if(dputs(pMatch->replPat, result, 0) != 0)
		return -1;
	return 0;
	}

// Substitute first occurrence (or all if n > 1) of "subPat" in "pSrc" with replacement pattern in "pMatch" and store results in
// "pRtnVal".  Ignore case in comparisons if flag set in "flags".  Return status.
static int strsub(Datum *pRtnVal, int n, Datum *pSrc, const char *subPat, Match *pMatch, ushort flags) {
	DFab result;
	char *(*strsrch)(const char *s1, const char *s2);
	char *str = pSrc->str;
	char *s;
	int srcLen, subPatLen;
	MatchLoc matchLoc;

	if(dopenwith(&result, pRtnVal, FabClear) != 0)
		goto LibFail;
	strsrch = flags & SOpt_Ignore ? strcasestr : strstr;
	matchLoc.strLoc.len = subPatLen = strlen(subPat);

	// Ready to roll... do substitution(s).
	for(;;) {
		// Find next occurrence.
		if((s = strsrch(str, subPat)) == NULL)
			break;

		// Compute offset and copy prefix.
		if((srcLen = s - str) > 0 && dputmem((void *) str, srcLen, &result, 0) != 0)
			goto LibFail;
		str = s + subPatLen;

		// If RRegical flag set, then no metacharacters were found in the search pattern (because this routine was
		// called), but at least one backslash was found in the replacement pattern.  In this case, we need to set up
		// the match in a MatchLoc object and save it so that replCopy() will do the replacement properly (from
		// group 0).
		if(pMatch->flags & RRegical) {
			matchLoc.strLoc.strPoint = s;
			if(saveMatch(pMatch, &matchLoc, NULL) != Success)
				return sess.rtn.status;
			}

		// Copy replacement pattern.
		if(replCopy(pMatch, &result) != 0)
			goto LibFail;

		// Bail out here unless n > 1.
		if(n <= 1)
			break;
		}

	// Copy remainder, if any.
	if((srcLen = strlen(str)) > 0 && dputmem((void *) str, srcLen, &result, 0) != 0)
		goto LibFail;
	if(dclose(&result, FabStr) == 0)
		return sess.rtn.status;
LibFail:
	return libfail();
	}

// Perform RE substitution(s) in string 'pSrc' using search and replacement patterns in given Match object, and save result in
// 'pRtnVal'.  Do all occurrences of the search pattern if n > 1, otherwise first only.  Return status.  'subPat' argument
// (needed for plain text substitutions) is not used.
static int regsub(Datum *pRtnVal, int n, Datum *pSrc, const char *subPat, Match *pMatch, ushort flags) {
	DFab result;
	regmatch_t group0;
	int scanOffset;
	size_t len, lastScanLen;
	ulong loopCount;

	// Begin scan loop.  For each match found in pSrc, perform substitution and use string result in pRtnVal as source
	// string (with an offset) for next iteration.  This is necessary for RE matching to work correctly.
	loopCount = lastScanLen = scanOffset = 0;
	for(;;) {
		// Find next occurrence.
		if(regcmp(pSrc, scanOffset, pMatch, &group0) != Success)
			break;
		if(group0.rm_so >= 0) {
			// Match found.  Error if we matched an empty string and scan position did not advance; otherwise, we'll
			// go into an infinite loop.
			if(++loopCount > 2 && group0.rm_eo == group0.rm_so && strlen(pSrc->str + scanOffset) == lastScanLen) {
				(void) rsset(Failure, RSNoFormat, text91);
					// "Repeating match at same position detected"
				break;
				}

			// Open fabrication object.
			if(dopenwith(&result, pRtnVal, FabClear) != 0)
				goto LibFail;

			// Copy any source text that is before the match location.
			len = scanOffset + group0.rm_so;
			if(len > 0 && dputmem((void *) pSrc->str, len, &result, 0) != 0)
				goto LibFail;

			// Copy replacement pattern.
			if(replCopy(pMatch, &result) != 0)
				goto LibFail;

			// Copy remaining source text to result if any, and close it.
			if(((len = strlen(pSrc->str + scanOffset + group0.rm_eo)) > 0 &&
			 dputs(pSrc->str + scanOffset + group0.rm_eo, &result, 0) != 0) ||
			 dclose(&result, FabStr) != 0) {
LibFail:
				(void) libfail();
				break;
				}

			// If no text remains or repeat count reached, we're done.
			if(len == 0 || n <= 1)
				break;

			// In "find all" mode... keep going.
			lastScanLen = strlen(pSrc->str) - scanOffset;
			scanOffset = strlen(pRtnVal->str) - len;
			dxfer(pSrc, pRtnVal);
			}
		else {
			// No match found.  Transfer input to result and bail out.
			dxfer(pRtnVal, pSrc);
			break;
			}
		}

	return sess.rtn.status;
	}

// Do "sub" function and return status.  Any of its three arguments may be null and the third may also be nil.
int substitute(Datum *pRtnVal, int n, Datum **args) {

	// Return null string if empty source string, and return source string if empty "from" string.
	if(disnull(args[0]))
		dsetnull(pRtnVal);
	else if(disnull(args[1]))
		dxfer(pRtnVal, args[0]);
	else {
		ushort flags = 0;
		int (*sub)(Datum *pRtnVal, int n, Datum *pSrc, const char *subPat, Match *pMatch, ushort flags);
		Match match;

		// Check if real RE pattern given.
		minit(&match);
		(void) checkOpts(args[1]->str, &flags);
		if(flags & SOpt_Regexp) {
			if(newSearchPat(args[1]->str, &match, &flags, false) != Success)
				goto Retn;
			if(compileRE(&match, SCpl_ForwardRE) != Success)
				goto SFree;
			if(match.flags & SRegical) {
				sub = regsub;
				goto PrepRepl;
				}
			}

		// Have plain text pattern or non-SRegical RE pattern.
		sub = strsub;
PrepRepl:
		// Save and compile replacement pattern.
		if(newReplPat(disnil(args[2]) ? "" : args[2]->str, &match, false) != Success)
			goto Retn;
		if(compileRepl(&match) != Success)
			goto RFree;

		// Call strsub() or regsub() function.
		(void) sub(pRtnVal, n <= 1 ? 1 : n, args[0], args[1]->str, &match, flags);

		// Finis.  Free pattern space and return.
RFree:
		freeReplPat(&match);
SFree:
		freeSearchPat(&match);
		grpFree(&match);
		}
Retn:
	return sess.rtn.status;
	}

// Expand character ranges and escaped characters (if any) in a string.  Return status.
int strExpand(DFab *pFab, const char *src) {
	short c1, c2;
	const char *str;

	if(dopentrack(pFab) != 0)
		goto LibFail;
	str = src;
	do {
		switch(c1 = *str) {
			case '-':
				if(str == src || (c2 = str[1]) == '\0')
					goto LitChar;
				if(c2 < (c1 = str[-1]))
					return rsset(Failure, 0, text2, str - 1, src);
						// "Invalid character range '%.3s' in string '%s'"
				while(++c1 <= c2)
					if(dputc(c1, pFab, 0) != 0)
						goto LibFail;
				++str;
				break;
			case '\\':
				if(str[1] != '\0')
					c1 = *++str;
				// Fall through.
			default:
LitChar:
				if(dputc(c1, pFab, 0) != 0)
					goto LibFail;
			}
		} while(*++str != '\0');

	if(dclose(pFab, FabStr) != 0)
LibFail:
		(void) libfail();
	return sess.rtn.status;
	}

// Prepare tr from and to strings.  Return status.
static int trPrep(Datum *pFrom, Datum *pTo) {
	DFab fab;

	// Expand "from" string.
	if(strExpand(&fab, pFrom->str) != Success)
		return sess.rtn.status;
	dxfer(pFrom, fab.pDatum);

	// Expand "to" string.
	if(disnil(pTo))
		dsetnull(pTo);
	else if(*pTo->str != '\0') {
		int lenFrom, lenTo;

		if(strExpand(&fab, pTo->str) != Success)
			return sess.rtn.status;
		dxfer(pTo, fab.pDatum);

		if((lenFrom = strlen(pFrom->str)) > (lenTo = strlen(pTo->str))) {
			short c = pTo->str[lenTo - 1];
			int n = lenFrom - lenTo;

			if(dopenwith(&fab, pTo, FabAppend) != 0)
				goto LibFail;
			do {
				if(dputc(c, &fab, 0) != 0)
					goto LibFail;
				} while(--n > 0);
			if(dclose(&fab, FabStr) != 0)
				goto LibFail;
			}
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Translate a string, given result pointer, source pointer, translate-from string, and translate-to string.  Return status.
// The translate-to string may be null, in which case all translate-from characters are deleted from the result.  It may also
// be shorter than the translate-from string, in which case it is padded to the same length with its last character.
static int tr(Datum *pRtnVal, Datum *pSrc, Datum *pFrom, Datum *pTo) {
	int lenTo;
	char *fromStr, *srcStr;
	DFab result;

	// Validate arguments.
	if(strlen(pFrom->str) == 0)
		return rsset(Failure, 0, text187, text328);
				// "%s cannot be null", "tr \"from\" string"
	if(trPrep(pFrom, pTo) != Success)
		return sess.rtn.status;

	// Scan source string.
	if(dopenwith(&result, pRtnVal, FabClear) != 0)
		goto LibFail;
	srcStr = pSrc->str;
	lenTo = strlen(pTo->str);
	while(*srcStr != '\0') {

		// Scan lookup table for a match.
		fromStr = pFrom->str;
		do {
			if(*srcStr == *fromStr) {
				if(lenTo > 0 && dputc(pTo->str[fromStr - pFrom->str], &result, 0) != 0)
					goto LibFail;
				goto Next;
				}
			} while(*++fromStr != '\0');

		// No match, copy the source char untranslated.
		if(dputc(*srcStr, &result, 0) != 0)
			goto LibFail;
Next:
		++srcStr;
		}

	// Terminate and return the result.
	if(dclose(&result, FabStr) != 0)
LibFail:
		(void) libfail();
	return sess.rtn.status;
	}

// Concatenate all arguments on a script line into pRtnVal in string form and return status.  minRequired is the number of
// required arguments.  ArgFirst flag is set on first argument.  Nil arguments are skipped if DCvtSkipNil flag is set;
// otherwise, they are kept.  Boolean arguments are always output as "false" and "true" and arrays are processed (recursively)
// as if each element was specified as an argument.
int catArgs(Datum *pRtnVal, int minRequired, Datum *pDelim, ushort cflags) {
	int rtnCode;
	uint argFlags = ArgFirst | ArgBool1 | ArgArray1 | ArgNIS1;
	DFab fab;
	Datum *pDatum;		// For arguments.
	bool firstWrite = true;
	const char *delim = (pDelim != NULL && !isNN(pDelim)) ? pDelim->str : NULL;

	// Nothing to do if no arguments; for example, an "abort()" call.
	if((sess.opFlags & (OpScript | OpParens)) == (OpScript | OpParens) && haveSym(s_rightParen, false) &&
	 minRequired == 0)
		return sess.rtn.status;

	if(dnewtrack(&pDatum) != 0 || dopenwith(&fab, pRtnVal, FabClear) != 0)
		goto LibFail;

	for(;;) {
		if(argFlags & ArgFirst) {
			if(!haveSym(s_any, minRequired > 0))
				break;				// Error or no arguments.
			}
		else if(!haveSym(s_comma, false))
			break;					// No arguments left.
		if(funcArg(pDatum, argFlags) != Success)
			return sess.rtn.status;
		--minRequired;

		// Skip nil argument if appropriate.
		if(disnil(pDatum) && (cflags & ACvtSkipNil))
			goto Onward;

		// Write optional delimiter and value.
		if(delim != NULL && !firstWrite && dputs(delim, &fab, 0) != 0)
			goto LibFail;
		if((rtnCode = dputd(pDatum, &fab, delim, 0)) < 0)
			goto LibFail;
		if(endless(rtnCode) != Success)
			return sess.rtn.status;
		firstWrite = false;
Onward:
		argFlags = ArgBool1 | ArgArray1 | ArgNIS1;
		}

	// Return result.
	if(dclose(&fab, FabStr) != 0)
LibFail:
		(void) libfail();
	return sess.rtn.status;
	}

// Process a strPop, strPush, strShift, or strUnshift function and store result in pRtnVal if evaluating; otherwise, just
// "consume" arguments.  Set pRtnVal to nil if strShift or strPop and no items left.  Return status.
static int strFunc(Datum *pRtnVal, CmdFuncId cmdFuncId) {
	int status;
	VarDesc varDesc;
	char *str1, *str2, *newVarVal;
	short delimChar = 0;
	bool spaceDelim = false;
	Datum *pDelim, *pOldVarVal, *pArg, newVar, *pNewVar;

	// Syntax of functions:
	//	strPush var, delimStr, val    strPop var, delimChar    strShift var, delimChar    strUnshift var, delimStr, val

	// Get variable name from current symbol, find it and its value, and validate it.
	if(!haveSym(s_any, true))
		return sess.rtn.status;
	if(sess.opFlags & OpEval) {
		if(dnewtrack(&pOldVarVal) != 0)
			goto LibFail;
		if(findVar(str1 = pLastParse->tok.str, &varDesc, OpDelete) != Success)
			return sess.rtn.status;
		if((varDesc.type == VTyp_SysVar && (varDesc.p.pSysVar->flags & V_RdOnly)) ||
		 (varDesc.type == VTyp_NumVar && varDesc.i.argNum == 0))
			return rsset(Failure, RSTermAttr, text164, str1);
				// "Cannot modify read-only variable '~b%s~B'"
		if(vderefv(pOldVarVal, &varDesc) != Success)
			return sess.rtn.status;

		// Have var value in pOldVarVal.  Verify that it is nil or string.
		if(disnil(pOldVarVal))
			dsetnull(pOldVarVal);
		else if(!isStrVal(pOldVarVal))
			return sess.rtn.status;
		}

	// Get delimiter into pDelim and also delimChar if single byte (strShift and strPop).
	if(dnewtrack(&pDelim) != 0)
		goto LibFail;
	if(getSym() < NotFound || funcArg(pDelim, (cmdFuncId == cf_strShift || cmdFuncId == cf_strPop) ?
	 (ArgInt1 | ArgNil1 | ArgMay) : ArgNil1) != Success)
		return sess.rtn.status;
	if(sess.opFlags & OpEval) {
		if(cmdFuncId == cf_strShift || cmdFuncId == cf_strPop) {
			if(disnil(pDelim))
				delimChar = '\0';
			else if(!isCharVal(pDelim))
				return sess.rtn.status;
			else if((delimChar = pDelim->u.intNum) == ' ')
				spaceDelim = true;
			}
		else if(disnil(pDelim))
			dsetnull(pDelim);
		}

	// Get value argument into pArg for strPush and strUnshift functions.
	if(cmdFuncId == cf_strPush || cmdFuncId == cf_strUnshift) {
		if(dnewtrack(&pArg) != 0)
			goto LibFail;
		if(funcArg(pArg, ArgNIS1) != Success)
			return sess.rtn.status;
		}

	// If not evaluating, we're done (all arguments consumed).
	if(!(sess.opFlags & OpEval))
		return sess.rtn.status;

	// Evaluating.  Convert value argument to string.
	if((cmdFuncId == cf_strPush || cmdFuncId == cf_strUnshift) && toStr(pArg) != Success)
		return sess.rtn.status;

	// Function value is in pArg (if strPush or strUnshift) and "old" var value is in pOldVarVal.  Do function-specific
	// operation.  Copy parsed token to pRtnVal if strPop or strShift.  Set newVarVal to new value of var in all cases.
	switch(cmdFuncId) {
		case cf_strPop:					// Get last token from old var value into pRtnVal.

			// Value	str1		str2		str3		Correct result
			// :		:		null		null		"", ""
			// ::		:#2		null		null		"", "", ""
			// x:		x		null		null		"", "x"
			// x::		:#2		null		null		"", "", "x"

			// :x		x		null		null		"x", ""
			// ::x		:#2		x		null		"x", "", ""

			// x		x		null		null		"x"
			// x:y:z	z		null		null		"z", "y", "x"
			// x::z		z		null		null		"z", "", "x"

			// "x  y  z"	" z"		null		null

			if(*(newVarVal = pOldVarVal->str) == '\0')	// Null var value?
				status = NotFound;
			else {
				str1 = strchr(newVarVal, '\0');
				status = revParseTok(pRtnVal, &str1, newVarVal, spaceDelim ? -1 : delimChar);
				}
			goto Check;
		case cf_strShift:
			newVarVal = pOldVarVal->str;
			status = parseTok(pRtnVal, &newVarVal, spaceDelim ? -1 : delimChar);
Check:
			dinit(pNewVar = &newVar);
			dsetstrref(newVarVal, pNewVar);

			if(status != Success) {			// Any tokens left?
				if(sess.rtn.status != Success)
					return sess.rtn.status;	// Fatal error.

				// No tokens left.  Signal end of token list.
				dsetnil(pRtnVal);
				goto Retn;
				}

			// We have a token.
			if(cmdFuncId == cf_strPop) {
				if(str1 <= newVarVal)		// Just popped last token?
					*newVarVal = '\0';	// Yes, clear variable.
				else
					*str1 = '\0';		// Not last.  Chop old at current spot (delimiter).
				}
			break;
		case cf_strPush:
			str1 = pOldVarVal->str;		// First portion of new var value (old var value).
			str2 = pArg->str;			// Second portion (value to append).
			goto Paste;
		default:	// cf_strUnshift
			str1 = pArg->str;			// First portion of new var value (value to prepend).
			str2 = pOldVarVal->str;		// Second portion (old var value).
Paste:
			{DFab fab;

			if(dopenwith(&fab, pRtnVal, FabClear) != 0 ||
			 dputs(str1, &fab, 0) != 0)			// Copy initial portion of new var value to work buffer.
				goto LibFail;

			// Append a delimiter if pOldVarVal is not null, and value (if strPush) or var (if strUnshift).
			if((!disnull(pOldVarVal) && dputs(pDelim->str, &fab, 0) != 0) || dputs(str2, &fab, 0) != 0 ||
			 dclose(&fab, FabStr) != 0)
				goto LibFail;
			pNewVar = pRtnVal;				// New var value.
			}
			break;
			}

	// Update variable and return status.
	(void) setVar(pNewVar, &varDesc);
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

#if (MMDebug & Debug_Token) && 0
static void showSym(const char *name) {

	fprintf(logfile, "%s(): last is str \"%s\" (%d)\n", name, pLastParse->tok.str, pLastParse->sym);
	}
#endif

// Insert, overwrite, or replace the text in src to a buffer n times and return status.  This routine's job is to set the given
// buffer as the current edit buffer, set current edit face and window accordingly, call iorStr() with src, n, and style to
// insert the text (via the core editing routines), then restore the current edit buffer, face, and window to their previous
// settings.  If pBuf is NULL, the current buffer is used.
int iorText(const char *src, int n, ushort style, Buffer *pBuf) {
	BufCtrl oldEdit = {NULL};

	// If the target buffer is not the current buffer and is being displayed in another window, remember current window and
	// use face from other one; otherwise, use buffer's face.
	if(pBuf != NULL && pBuf != sess.cur.pBuf) {
		oldEdit = sess.edit;
		sess.edit.pBuf = pBuf;
		if(pBuf->windCount == 0) {

			// Target buffer is not being displayed in any window... use buffer's face for editing.
			sess.edit.pFace = &pBuf->face;
			sess.edit.pScrn = (EScreen *) (sess.edit.pWind = NULL);
			}
		else {
			// Target buffer is being displayed.  Get window and screen.
			EScreen *pScrn;
			EWindow *pWind = findWind(pBuf, &pScrn);

			// Use window's face for editing.
			sess.edit.pFace = &(sess.edit.pWind = pWind)->face;
			if(pScrn == sess.cur.pScrn)
				supd_windFlags(pBuf, WFMode);
			else
				sess.edit.pScrn = pScrn;
			}
		}

	// Edit pointers set.  Now insert, overwrite, or replace the text in src n times.
	if(iorStr(src, n, style, NULL) == Success) {

		// Restore old edit pointers, if needed.
		if(oldEdit.pBuf != NULL)
			sess.edit = oldEdit;
		}

	return sess.rtn.status;
	}

// Concatenate script arguments into pRtnVal and insert, overwrite, or replace the resulting text to a buffer n times, given
// text insertion style and buffer pointer.  If pBuf is NULL, use current buffer.  If n == 0, do one repetition and don't move
// point.  If n < 0, do one repetition and process all newline characters literally (don't create a new line).  Return status.
static int chgText(Datum *pRtnVal, int n, ushort style, Buffer *pBuf) {
	int rtnCode;
	Datum *pArg;
	DFab text;

	if(dnewtrack(&pArg) != 0)
		goto LibFail;

	// Get first argument.  Skip fab creation if its the only one and is a string.
	if(funcArg(pArg, ArgFirst | ArgBool1 | ArgArray1 | ArgNIS1) != Success)
		return sess.rtn.status;
	if(!haveSym(s_comma, false) && dtypstr(pArg))
		dxfer(pRtnVal, pArg);
	else {
		// Evaluate all the arguments and save in fabrication object (so that the text can be inserted more than once,
		// if requested).
		if(dopenwith(&text, pRtnVal, FabClear) != 0)
			goto LibFail;
		goto First;

		for(;;) {
			if(!haveSym(s_comma, false))
				break;						// No arguments left.
			if(funcArg(pArg, ArgBool1 | ArgArray1 | ArgNIS1) != Success)
				return sess.rtn.status;
First:
			if(disempty(pArg))
				continue;					// Ignore nil, null, and [] values.
			if((rtnCode = dputd(pArg, &text, NULL, 0)) < 0)		// Add text chunk to fabrication object.
				goto LibFail;
			else if(endless(rtnCode) != Success)
				return sess.rtn.status;
			}
		if(dclose(&text, FabStr) != 0)
			goto LibFail;
		}

	// We have all the text in pRtnVal.  Now insert, overwrite, or replace it n times.
	return iorText(pRtnVal->str, n, style, pBuf);
LibFail:
	return libfail();
	}

// Process stat? function.  Return status.
static int fileTest(Datum *pRtnVal, int n, Datum *pFilename, Datum *pTestCodes) {

	if(disnull(pTestCodes))
		(void) rsset(Failure, 0, text187, text335);
			// "%s cannot be null", "File test code(s)"
	else {
		struct stat s;
		bool result;
		char *str, tests[] = "defLlrswx";

		// Validate test code(s).
		for(str = pTestCodes->str; *str != '\0'; ++str) {
			if(strchr(tests, *str) == NULL)
				return rsset(Failure, 0, text362, *str);
					// "Unknown file test code '%c'"
			}

		// Get file status.
		if(lstat(pFilename->str, &s) != 0)
			result = false;
		else {
			int flag;
			bool match;
			result = (n != INT_MIN);

			// Loop through test codes.
			for(str = pTestCodes->str; *str != '\0'; ++str) {
				switch(*str) {
					case 'd':
						flag = S_IFDIR;
						goto TypChk;
					case 'e':
						match = true;
						break;
					case 'f':
						flag = S_IFREG;
						goto TypChk;
					case 'r':
					case 'w':
					case 'x':
						match = access(pFilename->str, *str == 'r' ? R_OK :
						 *str == 'w' ? W_OK : X_OK) == 0;
						break;
					case 's':
						match = s.st_size > 0;
						break;
					case 'L':
						flag = S_IFLNK;
TypChk:
						match = (s.st_mode & S_IFMT) == flag;
						break;
					default:	// 'l'
						match = (s.st_mode & S_IFMT) == S_IFREG && s.st_nlink > 1;
					}
				if(match) {
					if(n == INT_MIN) {
						result = true;
						break;
						}
					}
				else if(n != INT_MIN) {
					result = false;
					break;
					}
				}
			}
		dsetbool(result, pRtnVal);
		}

	return sess.rtn.status;
	}

// Return next argument to strFormat(), "flattening" arrays in the process.  "argFlags" will always be either ArgInt1 or
// ArgNil1.  Return status.
static int formatArg(Datum *pRtnVal, uint argFlags, ArrayState *pArrayState) {

	for(;;) {
		if(pArrayState->pArray == NULL) {
			if(funcArg(pRtnVal, argFlags | ArgArray1 | ArgMay) != Success)
				return sess.rtn.status;
			if(!dtyparray(pRtnVal))
				break;
			pArrayState->pArray = pRtnVal->u.pArray;
			pArrayState->i = 0;
			}
		else {
			Array *pArray = pArrayState->pArray;
			if(pArrayState->i == pArray->used)
				pArrayState->pArray = NULL;
			else {
				if(dcpy(pRtnVal, pArray->elements[pArrayState->i]) != 0)
					return libfail();
				++pArrayState->i;
				break;
				}
			}
		}

	if(argFlags == ArgInt1)
		(void) isIntVal(pRtnVal);
	else if(argFlags == ArgNil1 && !disnil(pRtnVal))
		(void) isStrVal(pRtnVal);
	return sess.rtn.status;
	}

// Build string from "printf" format string and argument(s).  If pArg is not NULL, process binary operator (%)
// expression using pArg as the argument; otherwise, process bprintf, insertf, printf, or sprintf function.  Return status.
int strFormat(Datum *pRtnVal, Datum *pFormat, Datum *pArg) {
	short c;
	int specCount;
	ulong ul;
	char *prefix, *fmt;
	int prefLen;		// Length of prefix string.
	int width;		// Field width.
	int padding;		// # of chars to pad (on left or right).
	int precision;
	char *str;
	int sLen;		// Length of formatted string s.
	int flags;		// FMTxxx.
	int base;		// Number base (decimal, octal, etc.).
	Datum *pFmtArg;
	DFab result;
	ArrayState aryState = {NULL, 0};
	char workBuf[FMTBufSz];

	fmt = pFormat->str;

	// Create fabrication object for result and work Datum object for formatArg() calls.
	if(dopenwith(&result, pRtnVal, FabClear) != 0 || (pArg == NULL && dnewtrack(&pFmtArg) != 0))
		goto LibFail;

	// Loop through format string.
	specCount = 0;
	while((c = *fmt++) != '\0') {
		if(c != '%') {
			if(dputc(c, &result, 0) != 0)
				goto LibFail;
			continue;
			}

		// Check for prefix(es).
		prefix = NULL;					// Assume no prefix.
		flags = 0;					// Reset.
		while((c = *fmt++) != '\0') {
			switch(c) {
				case '0':
					flags |= FMT0pad; 	// Pad with 0's.
					break;
				case '-':
					flags |= FMTleft; 	// Left-justify.
					break;
				case '+':
					flags |= FMTplus; 	// Do + or - sign.
					break;
				case ' ':
					flags |= FMTspc;	// Space flag.
					break;
				case '#':
					flags |= FMThash; 	// Alternate form.
					break;
				default:
					goto GetWidth;
				}
			}

		// Get width.
GetWidth:
		width = 0;
		if(c == '*') {
			if(pArg != NULL)			// Error if format op (%).
				goto TooMany;
			if(formatArg(pFmtArg, ArgInt1, &aryState) != Success)	// Get next (int) argument for width.
				return sess.rtn.status;
			if((width = pFmtArg->u.intNum) < 0) {		// Negative field width?
				flags |= FMTleft;		// Yes, left justify field.
				width = -width;
				}
			c = *fmt++;
			}
		else {
			while(is_digit(c)) {
				width = width * 10 + c - '0';
				c = *fmt++;
				}
			}

		// Get precision.
		precision = 0;
		if(c == '.') {
			c = *fmt++;
			if(c == '*') {
				if(pArg != NULL)		// Error if format op (%).
					goto TooMany;
				if(formatArg(pFmtArg, ArgInt1, &aryState) != Success)
					return sess.rtn.status;
				if((precision = pFmtArg->u.intNum) < 0)
					precision = 0;
				else
					flags |= FMTprec;
				c = *fmt++;
				}
			else if(is_digit(c)) {
				flags |= FMTprec;
				do {
					precision = precision * 10 + c - '0';
					} while(is_digit(c = *fmt++));
				}
			}

		// Get pArrayEl flag.
		if(c == 'l') {
			flags |= FMTlong;
			c = *fmt++;
			}

		// Get spec.
		switch(c) {
		case 's':
			if(pArg != NULL) {			// Check pArg type.
				if(!disnil(pArg) && !isStrVal(pArg))
					return sess.rtn.status;
				if(++specCount > 1)		// Check spec count.
					goto TooMany;
				pFmtArg = pArg;
				}
			else if(formatArg(pFmtArg, ArgNil1, &aryState) != Success)
				return sess.rtn.status;
			if(disnil(pFmtArg))
				dsetnull(pFmtArg);
			sLen = strlen(str = pFmtArg->str);	// Length of string.
			if(flags & FMTprec) {			// If there is a precision...
				if(precision < sLen)
					if((sLen = precision) < 0)
						sLen = 0;
				}
			break;
		case '%':
			workBuf[0] = '%';
			goto SaveCh;
		case 'c':
			if(pArg != NULL) {
				if(!isIntVal(pArg))		// Check pArg type.
					return sess.rtn.status;
				if(++specCount > 1)		// Check spec count.
					goto TooMany;
				pFmtArg = pArg;
				}
			else if(formatArg(pFmtArg, ArgInt1, &aryState) != Success)
				return sess.rtn.status;
			workBuf[0] = pFmtArg->u.intNum;
SaveCh:
			str = workBuf;
			sLen = 1;
			break;
		case 'd':
		case 'i':
			if(pArg != NULL) {
				if(!isIntVal(pArg))		// Check pArg type.
					return sess.rtn.status;
				if(++specCount > 1)		// Check spec count.
					goto TooMany;
				pFmtArg = pArg;
				}
			else if(formatArg(pFmtArg, ArgInt1, &aryState) != Success)
				return sess.rtn.status;
			base = 10;
			ul = labs(pFmtArg->u.intNum);
			prefix = pFmtArg->u.intNum < 0 ? "-" : (flags & FMTplus) ? "+" : (flags & FMTspc) ? " " : "";
			goto ULFmt;
		case 'b':
			base = 2;
			goto GetUns;
		case 'o':
			base = 8;
			goto GetUns;
		case 'u':
			base = 10;
GetUns:
			if(pArg != NULL) {
				if(!isIntVal(pArg))		// Check pArg type.
					return sess.rtn.status;
				if(++specCount > 1)		// Check spec count.
					goto TooMany;
				pFmtArg = pArg;
				}
			else if(formatArg(pFmtArg, ArgInt1, &aryState) != Success)
				return sess.rtn.status;
			ul = (ulong) pFmtArg->u.intNum;
			goto ULFmt;
		case 'X':
			flags |= FMTxuc;
		case 'x':
			if(pArg != NULL) {
				if(!isIntVal(pArg))		// Check pArg type.
					return sess.rtn.status;
				if(++specCount > 1)		// Check spec count.
TooMany:
					return rsset(Failure, RSNoFormat, text320);
						// "More than one spec in '%' format string"
				pFmtArg = pArg;
				}
			else if(formatArg(pFmtArg, ArgInt1, &aryState) != Success)
				return sess.rtn.status;
			base = 16;
			ul = (ulong) pFmtArg->u.intNum;
			if((flags & FMThash) && ul)
				prefix = (c == 'X') ? "0X" : "0x";

			// Fall through.
ULFmt:
			// Ignore '0' flag if precision specified.
			if((flags & (FMT0pad | FMTprec)) == (FMT0pad | FMTprec))
				flags &= ~FMT0pad;

			str = workBuf + (FMTBufSz - 1);
			if(ul) {
				for(;;) {
					*str = (ul % base) + '0';
					if(*str > '9')
						*str += (flags & FMTxuc) ? 'A' - '0' - 10 : 'a' - '0' - 10;
					if((ul /= base) == 0)
						break;
					--str;
					}
				sLen = workBuf + FMTBufSz - str;
				}
			else if((flags & FMTprec) && precision == 0)
				sLen = 0;
			else {
				*str = '0';
				sLen = 1;
				}

			if(sLen < precision) {
				if(precision > FMTBufSz)
					precision = FMTBufSz;
				do {
					*--str = '0';
					} while(++sLen < precision);
				}
			else if(sLen > 0 && c == 'o' && (flags & FMThash) && *str != '0') {
				*--str = '0';
				++sLen;
				}
			break;
		default:
			return rsset(Failure, 0, text321, c == '\0' ? "" : vizc(c, VizBaseDef));
				// "Unknown format spec '%%%s'"
		}

		// Concatenate the pieces, which are padding, prefix, more padding, the string, and trailing padding.
		prefLen = (prefix == NULL) ? 0 : strlen(prefix); // Length of prefix string.
		padding = width - (prefLen + sLen);		// # of chars to pad.

		// If 0 padding, store prefix (if any).
		if((flags & FMT0pad) && prefix != NULL) {
			if(dputs(prefix, &result, 0) != 0)
				goto LibFail;
			prefix = NULL;
			}

		// If right-justified and chars to pad, store prefix string.
		if(padding > 0) {
			if(!(flags & FMTleft)) {
				short c1 = (flags & FMT0pad) ? '0' : ' ';
				while(--padding >= 0)
					if(dputc(c1, &result, 0) != 0)
						goto LibFail;
				}
			}

		// Store prefix (if any).
		if(prefix != NULL && dputs(prefix, &result, 0) != 0)
			goto LibFail;

		// Store (fixed-length) string.
		if(dputmem((void *) str, sLen, &result, 0) != 0)
			goto LibFail;

 		// Store right padding.
		if(flags & FMTleft) {
			while(--padding >= 0)
				if(dputc(' ', &result, 0) != 0)
					goto LibFail;
			}
		}

	// End of format string.  Check for errors.
	if(specCount == 0 && pArg != NULL)
		return rsset(Failure, RSNoFormat, text281);
			// "Missing spec in '%' format string"
	if(aryState.pArray != NULL && aryState.i < aryState.pArray->used)
		return rsset(Failure, RSNoFormat, text204);
			// "Too many arguments for 'printf' or 'sprintf' function"
	if(dclose(&result, FabStr) != 0)
LibFail:
		(void) libfail();

	return sess.rtn.status;
	}

// Get a kill specified by n (of unlimited size) and save in pRtnVal.  May be a null string.  Return status.
static int getKill(Datum *pRtnVal, int n) {
	RingEntry *pRingEntry;

	// Which kill?
	if((pRingEntry = rget(ringTable + RingIdxKill, n)) != NULL && dcpy(pRtnVal, &pRingEntry->data) != 0)
		(void) libfail();
	return sess.rtn.status;
	}

// Resolve a buffer name argument.  Return status.
Buffer *findBuf(Datum *pBufname) {
	Buffer *pBuf;

	if((pBuf = bsrch(pBufname->str, NULL)) == NULL)
		(void) rsset(Failure, 0, text118, pBufname->str);
			// "No such buffer '%s'"
	return pBuf;
	}

//  Do mode? function (check if a mode is set) and return status.
int modeQ(Datum *pRtnVal, int n, Datum **args) {
	ModeSpec *pModeSpec;
	char *modeName;
	Buffer *pBuf;
	bool strict = n > 0;
	bool result = false;

	// Get buffer if specified.
	if(args[1] == NULL)
		pBuf = sess.cur.pBuf;
	else if((pBuf = findBuf(args[1])) == NULL)
		return sess.rtn.status;

	// Search mode table for a match.
	modeName = args[0]->str;
	if((pModeSpec = msrch(modeName, NULL)) != NULL) {

		// Match found.  Set result.
		result = (pModeSpec->flags & MdGlobal) ? pModeSpec->flags & MdEnabled : isBufModeSet(pBuf, pModeSpec);
		}
	else {
		// Unknown mode name.  Error if "strict" in effect.
		if(strict)
			return rsset(Failure, 0, text66, modeName);
				// "Unknown or ambiguous mode '%s'"
		}

	// Return result.
	dsetbool(result, pRtnVal);
	return sess.rtn.status;
	}

// Check if a mode in given group is set and return its name if so, otherwise nil.  Return status.
int groupModeQ(Datum *pRtnVal, int n, Datum **args) {
	ModeSpec *pModeSpec;
	ModeGrp *pModeGrp;
	Array *pArray;
	Datum *pArrayEl;
	ushort count;
	Buffer *pBuf;
	ModeSpec *pResultSpec = NULL;
	bool strict = n > 0;

	// Get buffer if specified.
	if(args[1] == NULL)
		pBuf = sess.cur.pBuf;
	else if((pBuf = findBuf(args[1])) == NULL)
		return sess.rtn.status;

	// Get mode group.
	if(!mgsrch(args[0]->str, NULL, &pModeGrp)) {
		if(!strict)
			goto SetNil;
		return rsset(Failure, 0, text395, text390, args[0]->str);
				// "No such %s '%s'", "group"
		}

	// If group has members, check them.
	if(pModeGrp->useCount > 0) {

		// Scan mode table and check if any mode in group is enabled.
		pArray = &modeInfo.modeTable;
		count = 0;
		while((pArrayEl = aeach(&pArray)) != NULL) {
			pModeSpec = modePtr(pArrayEl);
			if(pModeSpec->pModeGrp == pModeGrp) {

				// Found mode in group.  Enabled?
				if(pModeSpec->flags & MdGlobal) {
					if(pModeSpec->flags & MdEnabled)
						goto Enabled;
					}
				else if(isBufModeSet(pBuf, pModeSpec)) {
Enabled:
					pResultSpec = pModeSpec;
					break;
					}
				if(++count == pModeGrp->useCount)
					break;
				}
			}
		}

	// Return result.
	if(pResultSpec != NULL) {
		if(dsetstr(pResultSpec->name, pRtnVal) != 0)
			(void) libfail();
		}
	else
SetNil:
		dsetnil(pRtnVal);
	return sess.rtn.status;
	}

// Clone an array.  Return status.
int arrayClone(Datum *pDest, Datum *pSrc) {
	Array *pArray;

	if((pArray = aclone(pSrc->u.pArray)) == NULL)
		return libfail();
	agStash(pDest, pArray);

	return sess.rtn.status;
	}

// Check if proposed wrap column is in range.  Return status.
int checkWrapCol(int col) {

	return col < 0 ? rsset(Failure, 0, text39, text59, col, 0) : sess.rtn.status;
				// "%s (%d) must be %d or greater", "Wrap column"
	}

// Set wrap column to col.  Return status.
int setWrapCol(int col) {

	if(checkWrapCol(col) == Success && col != sess.cur.pScrn->wrapCol) {
		sess.cur.pScrn->prevWrapCol = sess.cur.pScrn->wrapCol;
		sess.cur.pScrn->wrapCol = col;
		}
	return sess.rtn.status;
	}

// Convert a string to title case.  Return status.
int titleCaseStr(Datum *pRtnVal, int n, Datum **args) {
	char *src, *dest;
	short c;
	bool inWord = false;

	if(dsalloc(pRtnVal, strlen(src = args[0]->str) + 1) != 0)
		return libfail();
	dest = pRtnVal->str;
	while((c = *src++) != '\0') {
		if(wordChar[c]) {
			*dest++ = inWord ? lowCase[c] : upCase[c];
			inWord = true;
			}
		else {
			*dest++ = c;
			inWord = false;
			}
		}
	*dest = '\0';
	return sess.rtn.status;
	}

// Move point backward or forward n tab stops.
static int backForwTab(int n) {
	int offset;

	if((offset = tabStop(n)) >= 0)
		sess.cur.pFace->point.offset = offset;
	return sess.rtn.status;
	}

// Check if a character or string is in a character class.  Return -1 (error), 0 (false), or 1 (true).
static int isCC(Datum *pClass, Datum *pVal) {
	bool (*func)(short) = getCC(pClass->str);
	if(func == NULL) {
		(void) rsset(Failure, 0, text136, pClass->str);
				// "No such character class '%s'"
		return -1;
		}
	if(dtypstr(pVal)) {
		char *str = pVal->str;
		do {
			if(!func((short) *str++))
				return 0;
			} while(*str != '\0');
		return 1;
		}
	else if(!isCharVal(pVal))
		return -1;
	else
		return func(pVal->u.intNum) ? 1 : 0;
	}

// Execute a system command or function, given result pointer, n argument, pointer into the command-function table
// (cmdFuncTable), and minimum and maximum number of arguments.  Return status.  This is the execution routine for all commands
// and functions.  When script mode is active, arguments (if any) are preloaded and validated per descriptors in the
// cmdFuncTable table, then made available to the code for the specific command or function (which is here or in a separate
// routine).  If not evaluating (OpEval not set), arguments are "consumed" only and the execution code is bypassed if the
// maximum number of arguments for the command or function has been loaded.  Note that (1), if not evaluating, this routine will
// only be called if the CFSpecArgs flag is set; and (2), any command or function with CFNCount flag set must handle a zero n so
// that execCmdFunc() calls from routines other than fcall() will always work (for any value of n).
int execCmdFunc(Datum *pRtnVal, int n, CmdFunc *pCmdFunc, int minArgs, int maxArgs) {
	Datum *args[CFMaxArgs + 1];	// Argument values plus NULL terminator.
	int argCount = 0;		// Number of arguments loaded into args array.

	// If script mode and CFNoLoad is not set, retrieve the minimum number of arguments specified in cmdFuncTable, up to the
	// maximum, and save in args.  If CFSpecArgs flag is set or the maximum is negative, use the minimum for the maximum;
	// otherwise, if CFShortLoad flag is set, decrease the maximum by one.
	args[0] = NULL;
	if((sess.opFlags & (OpScript | OpNoLoad)) == OpScript && !(pCmdFunc->attrFlags & CFNoLoad) &&
	 (!(sess.opFlags & OpParens) || !haveSym(s_rightParen, false))) {
		if(pCmdFunc->attrFlags & CFShortLoad)
			--minArgs;
		if((pCmdFunc->attrFlags & (CFSpecArgs | CFMinLoad)) || pCmdFunc->maxArgs < 0)
			maxArgs = minArgs;
		else if(pCmdFunc->attrFlags & CFShortLoad)
			--maxArgs;

		// Proceed if at least one argument is allowed and either have a symbol or at least one argument is required.
		if(maxArgs > 0 && (haveSym(s_any, false) || minArgs > 0)) {
			Datum **args1 = args;
			uint argFlags = ArgFirst | (pCmdFunc->argFlags & ArgPath);
			do {
				if(dnewtrack(args1) != 0)
					goto LibFail;
				if(funcArg(*args1++, argFlags | (((pCmdFunc->argFlags & (ArgNotNull1 << argCount)) |
				 (pCmdFunc->argFlags & (ArgNil1 << argCount)) | (pCmdFunc->argFlags & (ArgBool1 << argCount)) |
				 (pCmdFunc->argFlags & (ArgInt1 << argCount)) | (pCmdFunc->argFlags & (ArgArray1 << argCount)) |
				 (pCmdFunc->argFlags & (ArgNIS1 << argCount))) >> argCount) |
				 (pCmdFunc->argFlags & ArgMay)) != Success)
					return sess.rtn.status;
				argFlags = 0;
				} while(++argCount < minArgs || (argCount < maxArgs && haveSym(s_comma, false)));
			*args1 = NULL;
			}
		}

	// Evaluate the command or function.
	if(pCmdFunc->func != NULL) {
#if MMDebug & Debug_Expr
		fprintf(logfile, "execCmdFunc(): Calling %s() with n = %d, and %d arguments...\n", pCmdFunc->name, n, argCount);
#endif
		(void) pCmdFunc->func(pRtnVal, n, args);
		}
	else {
		int i;
		const char *prompt;
		long longVal;
		Array *pArray;
		Buffer *pBuf = NULL;
		int cmdFuncId = pCmdFunc - cmdFuncTable;

		switch(cmdFuncId) {
			case cf_abs:
				dsetint(labs(args[0]->u.intNum), pRtnVal);
				break;
			case cf_aclone:
				(void) arrayClone(pRtnVal, args[0]);
				break;
			case cf_acompact:
				i = n != INT_MIN ? AOpInPlace : 0;
				if((pArray = acompact(args[0]->u.pArray, i)) == NULL)
					goto LibFail;
				if(i == 0)
					agStash(pRtnVal, pArray);
				else
					dxfer(pRtnVal, args[0]);
				break;
			case cf_adelete:
				if(args[2] == NULL) {

					// Delete an element.
					Datum *pDatum = adelete(args[0]->u.pArray, args[1]->u.intNum);
					if(pDatum == NULL)
						goto LibFail;
					if(dtyparray(pDatum))
						agTrack(pDatum);	// Have array reference.
					dxfer(pRtnVal, pDatum);
					}
				else {
					// Cut a slice.
					if((pArray = aslice(args[0]->u.pArray, args[1]->u.intNum, args[2]->u.intNum,
					 AOpCut)) == NULL)
						goto LibFail;
					agStash(pRtnVal, pArray);
					}
				break;
			case cf_adeleteif:
				dsetint(adeleteif(args[0]->u.pArray, args[1], 0), pRtnVal);
				break;
			case cf_afill:
				(void) doFill(pRtnVal, args);
				break;
			case cf_aincludeQ:
				dsetbool(ainclude(args[0]->u.pArray, args[1], 0), pRtnVal);
				break;
			case cf_aindex:
				if((i = aindex(args[0]->u.pArray, args[1], n > 0 ? AOpLast : 0)) < 0)
					dsetnil(pRtnVal);
				else
					dsetint(i, pRtnVal);
				break;
			case cf_ainsert:
				i = args[2]->u.intNum;
				goto AInsert;
			case cf_apop:
			case cf_ashift:
				pArray = args[0]->u.pArray;
				if((args[1] = (cmdFuncId == cf_apop) ? apop(pArray) : ashift(pArray)) == NULL)
					dsetnil(pRtnVal);
				else {
					dxfer(pRtnVal, args[1]);
					if(dtyparray(pRtnVal))
						agTrack(pRtnVal);
					}
				break;
			case cf_apush:
				i = args[0]->u.pArray->used;
				goto AInsert;
			case cf_aunshift:
				i = 0;
AInsert:
				if(ainsert(args[0]->u.pArray, args[1], i, 0) != 0)
					goto LibFail;
				duntrack(args[1]);
				dxfer(pRtnVal, args[0]);
				break;
			case cf_appendFile:
				i = 'a';
				goto AWFile;
			case cf_backPageNext:
				// Scroll the next window up (backward) a page.
				(void) wscroll(pRtnVal, n, nextWind, backPage);
				break;
			case cf_backPagePrev:
				// Scroll the previous window up (backward) a page.
				(void) wscroll(pRtnVal, n, prevWind, backPage);
				break;
			case cf_backTab:
				// Move the point backward "n" tab stops.
				(void) backForwTab(n == INT_MIN ? -1 : -n);
				break;
			case cf_backspace:;
				// Delete char or soft tab backward.  Return status.
				Point *pPoint = &sess.cur.pFace->point;
				(void) (sess.cur.pScrn->softTabSize > 0 && pPoint->offset > 0 ?
				 delTab(n == INT_MIN ? -1 : -n, true) : edelc(n == INT_MIN ? -1 : -n, 0));
				break;
			case cf_basename:
				if(dsetstr(fbasename(args[0]->str, n == INT_MIN || n > 0), pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_beep:
				// Beep the beeper n times.
				if(n == INT_MIN)
					n = 1;
				else if(n < 0 || n > 10)
					return rsset(Failure, 0, text12, text137, n, 0, 10);
						// "%s (%d) must be between %d and %d", "Repeat count"
				while(n-- > 0)
					tbeep();
				break;
			case cf_beginBuf:
				// Go to the beginning of the buffer.
				prompt = text326;
					// "Begin"
				i = false;
				goto BEBuf;
			case cf_beginLine:
				// Move the point to the beginning of the [-]nth line.
				(void) beginEndLine(pRtnVal, n, false);
				break;
			case cf_beginWhite:
				// Move the point to the beginning of white space surrounding point.
				(void) spanWhite(false);
				break;
			case cf_bemptyQ:

				// Get buffer pointer.
				if(n == INT_MIN)
					pBuf = sess.cur.pBuf;
				else if((pBuf = bsrch(args[0]->str, NULL)) == NULL)
					return rsset(Failure, 0, text118, args[0]->str);
						// "No such buffer '%s'"

				// Return Boolean result.
				dsetbool(bempty(pBuf), pRtnVal);
				break;
			case cf_bprint:
			case cf_bprintf:

				// Get buffer pointer.
				if((pBuf = bsrch(args[0]->str, NULL)) == NULL)
					return rsset(Failure, 0, text118, args[0]->str);
						// "No such buffer '%s'"

				if(cmdFuncId == cf_bprint) {
					if(!needSym(s_comma, true))
						return sess.rtn.status;
					i = Txt_Insert;
					goto ChgText;
					}

				// Create formatted string in pRtnVal and insert into buffer.
				if(strFormat(pRtnVal, args[1], NULL) == Success)
					(void) iorText(pRtnVal->str, n, Txt_Insert, pBuf);
				break;
			case cf_bufBoundQ:
				if(n != INT_MIN)
					n = n > 0 ? 1 : n < 0 ? -1 : 0;
				i = bufEnd(NULL) ? 1 : bufBegin(NULL) ? -1 : 0;
				dsetbool((n == INT_MIN && i != 0) || i == n, pRtnVal);
				break;
			case cf_bufWind:
				{EWindow *pWind;

				if((pBuf = bsrch(args[0]->str, NULL)) != NULL &&
				 (pWind = windDispBuf(pBuf, n != INT_MIN)) != NULL)
					dsetint((long) getWindNum(pWind), pRtnVal);
				else
					dsetnil(pRtnVal);
				}
				break;
			case cf_chr:
				if(isCharVal(args[0]))
					dconvchr(args[0]->u.intNum, pRtnVal);
				break;
			case cf_clearMsgLine:
				(void) mlerase(MLForce);
				break;
			case cf_copyFencedRegion:
				// Copy text to kill ring.
				i = 1;
				goto KDCFenced;
			case cf_copyLine:
				// Copy line(s) to kill ring.
				i = 1;
				goto KDCLine;
			case cf_copyRegion:;
				// Copy all of the characters in the region to the kill ring without moving point.
				Region region;

				if(getRegion(&region, 0) != Success || regionToKill(&region) != Success)
					break;
				(void) rsset(Success, RSNoFormat, text70);
					// "Region copied"
				break;
			case cf_copyToBreak:
				// Copy text to kill ring.
				i = 1;
				goto KDCText;
			case cf_copyWord:
				// Copy word(s) to kill ring without moving point.
				(void) (n == INT_MIN ? kdcForwWord(1, 1) : n < 0 ? kdcBackWord(-n, 1) : kdcForwWord(n, 1));
				break;
			case cf_metaPrefix:
			case cf_negativeArg:
			case cf_prefix1:
			case cf_prefix2:
			case cf_prefix3:
			case cf_universalArg:
				break;
			case cf_delBackChar:
				// Delete char backward.  Return status.
				(void) edelc(n == INT_MIN ? -1 : -n, 0);
				break;
			case cf_delBackTab:
				// Delete tab backward.  Return status.
				(void) delTab(n == INT_MIN ? -1 : -n, false);
				break;
			case cf_delFencedRegion:
				// Delete text without saving in kill ring.
				i = 0;
				goto KDCFenced;
			case cf_delForwChar:
				// Delete char forward.  Return status.
				(void) edelc(n == INT_MIN ? 1 : n, 0);
				break;
			case cf_delForwTab:
				// Delete tab forward.  Return status.
				(void) delTab(n, false);
				break;
			case cf_delLine:
				// Delete line(s) without saving text in kill ring.
				i = 0;
				goto KDCLine;
			case cf_delRegion:
				// Delete region without saving text in kill ring.
				i = false;
				goto DKReg;
			case cf_delToBreak:
				// Delete text without saving in kill ring.
				i = 0;
				goto KDCText;
			case cf_delWhite:
				// Delete white space surrounding or immediately before point on current line.
				(void) nukeWhite(n, true);
				break;
			case cf_delWord:
				// Delete word(s) without saving text in kill ring.
				(void) (n == INT_MIN ? kdcForwWord(1, 0) : n < 0 ? kdcBackWord(-n, 0) : kdcForwWord(n, 0));
				break;
			case cf_dirname:
				if(dsetstr(fdirname(args[0]->str, n), pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_emptyQ:
				if(args[0]->type != dat_int || isCharVal(args[0]))
					dsetbool(disnil(args[0]) ? true :
					 args[0]->type == dat_int ? args[0]->u.intNum == 0 :
					 dtypstr(args[0]) ? *args[0]->str == '\0' :
					 args[0]->u.pArray->used == 0, pRtnVal);
				break;
			case cf_endBuf:
				// Move to the end of the buffer.
				prompt = text188;
					// "End"
				i = true;
BEBuf:
				(void) bufOp(pRtnVal, n, prompt, BO_BeginEnd, i);
				break;
			case cf_endLine:
				// Move the point to the end of the [-]nth line.
				(void) beginEndLine(pRtnVal, n, true);
				break;
			case cf_endWhite:
				// Move the point to the end of white space surrounding point.
				(void) spanWhite(true);
				break;
			case cf_env:
				if(dsetstr(fixNull(getenv(args[0]->str)), pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_expandPath:
				(void) expandPath(pRtnVal, args[0]);
				break;
			case cf_findFile:
				i = false;
				goto FVFile;
			case cf_forwPageNext:
				// Scroll the next window down (forward) a page.
				(void) wscroll(pRtnVal, n, nextWind, forwPage);
				break;
			case cf_forwPagePrev:
				// Scroll the previous window down (forward) a page.
				(void) wscroll(pRtnVal, n, prevWind, forwPage);
				break;
			case cf_forwTab:
				// Move the point forward "n" tab stops.
				(void) backForwTab(n == INT_MIN ? 1 : n);
				break;
			case cf_growWind:
				// Grow (enlarge) the current window.  If n is negative, take lines from the upper window;
				// otherwise, lower.
				i = 1;
				goto GSWind;
			case cf_insert:
				i = Txt_Insert;
				goto ChgText;
			case cf_insertf:
				if(strFormat(pRtnVal, args[0], NULL) == Success)
					(void) iorText(pRtnVal->str, n, Txt_Insert, NULL);
				break;
			case cf_insertPipe:
				prompt = text249;
					// "Insert pipe"
				i = PipeInsert;
				goto PipeCmd;
			case cf_insertSpace:
				// Insert space(s) forward into text without moving point.
				(void) insertNLSpace(n, EditSpace | EditHoldPoint);
				break;
			case cf_interactiveQ:
				dsetbool((sess.opFlags & (OpStartup | OpUserCmd)) == OpUserCmd, pRtnVal);
				break;
			case cf_isClassQ:
				if((i = isCC(args[0], args[1])) >= 0)
					dsetbool(i, pRtnVal);
				break;
			case cf_join:
				if(needSym(s_comma, true))
					(void) catArgs(pRtnVal, 1, args[0], n <= 0 && n != INT_MIN ? ACvtSkipNil : 0);
				break;
			case cf_keyPendingQ:
				if(typahead(&i) == Success)
					dsetbool(i > 0, pRtnVal);
				break;
			case cf_kill:
				(void) getKill(pRtnVal, (int) args[0]->u.intNum);
				break;
			case cf_killFencedRegion:
				// Delete text and save in kill ring.
				i = -1;
KDCFenced:
				(void) kdcFencedRegion(i);
				break;
			case cf_killLine:
				// Delete line(s) and save text in kill ring.
				i = -1;
KDCLine:
				(void) kdcLine(n, i);
				break;
			case cf_killRegion:
				// Delete region and save text in kill ring.
				i = true;
DKReg:
				(void) delKillRegion(n, i);
				break;
			case cf_killToBreak:
				// Delete text and save in kill ring.
				i = -1;
KDCText:
				(void) kdcText(n, i, NULL);
				break;
			case cf_killWord:
				// Delete word(s) and save text in kill ring.
				(void) (n == INT_MIN ? kdcForwWord(1, -1) : n < 0 ? kdcBackWord(-n, -1) : kdcForwWord(n, -1));
				break;
			case cf_lastBuf:
				if(sess.cur.pScrn->pLastBuf != NULL) {
					Buffer *pOldBuf = sess.cur.pBuf;
					if(render(pRtnVal, 1, sess.cur.pScrn->pLastBuf, 0) == Success && n < 0 &&
					 n != INT_MIN) {
						char bufname[MaxBufname + 1];

						strcpy(bufname, pOldBuf->bufname);
					 	if(bdelete(pOldBuf, 0) == Success)
							(void) rsset(Success, RSHigh, text372, bufname);
									// "Buffer '%s' deleted"
						}
					}
				break;
			case cf_length:
				dsetint(dtyparray(args[0]) ? (long) args[0]->u.pArray->used :
				 (long) strlen(args[0]->str), pRtnVal);
				break;
			case cf_lowerCaseLine:
				i = CaseLine | CaseLower;
				goto CvtCase;
			case cf_lowerCaseRegion:
				i = CaseRegion | CaseLower;
				goto CvtCase;
			case cf_lowerCaseStr:
				i = -1;
				goto CvtStr;
			case cf_lowerCaseWord:
				i = CaseWord | CaseLower;
				goto CvtCase;
			case cf_match:
				{Match *pMatch = (n == INT_MIN) ? &matchRE : &bufSearch.match;
				longVal = args[0]->u.intNum;
				if(longVal < 0)
					return rsset(Failure, 0, text5, longVal);
						// "No such group, %ld"
				if(longVal >= pMatch->grpMatch.size)
					dsetnil(pRtnVal);
				else if(dsetstr(pMatch->grpMatch.groups[longVal].str, pRtnVal) != 0)
					goto LibFail;
				}
				break;
			case cf_moveWindDown:
				// Move the current window down by "n" lines and compute the new top line in the window.
				(void) moveWindUp(pRtnVal, n == INT_MIN ? -1 : -n, args);
				break;
			case cf_newline:
				i = EditWrap;
				goto InsNLSPC;
			case cf_nextBuf:
				i = 0;
				goto PNBuf;
			case cf_nextScreen:
				i = SWB_Repeat | SWB_Forw;
				goto PNScreen;
			case cf_nilQ:
				dsetbool(disnil(args[0]), pRtnVal);
				break;
			case cf_nullQ:
				dsetbool(disnull(args[0]), pRtnVal);
				break;
			case cf_numericQ:
				dsetbool(ascToLong(args[0]->str, NULL, true), pRtnVal);
				break;
			case cf_openLine:
				if(insertNLSpace(n, EditHoldPoint) == Success && n < 0 && n != INT_MIN) {

					// Move point to first empty line if possible.
					Point *pPoint = &sess.cur.pFace->point;
					if(pPoint->pLine->used > 0 && pPoint->pLine->next->used == 0)
						(void) moveChar(1);
					}
				break;
			case cf_ord:
				dsetint((long) args[0]->str[0], pRtnVal);
				break;
			case cf_overwriteChar:
				i = Txt_Replace;
				goto ChgText;
			case cf_overwriteCol:
				i = Txt_Overwrite;
ChgText:
				(void) chgText(pRtnVal, n, i, pBuf);
				break;
			case cf_pause:
				if((i = args[0]->u.intNum) < 0)
					return rsset(Failure, 0, text39, text119, i, 0);
						// "%s (%d) must be %d or greater", "Pause duration"
				centiPause(n == INT_MIN ? i * 100 : i);
				break;
			case cf_pipeBuf:
				prompt = text306;
					// "Pipe command"
				i = PipeWrite;
				goto PipeCmd;
			case cf_popBuf:
				i = true;
				goto PopIt;
			case cf_popFile:
				i = false;
PopIt:
				(void) doPop(pRtnVal, n, i);
				break;
			case cf_prevBuf:
				i = BT_Backward;
PNBuf:
				// Switch to the previous or next buffer in the buffer list.  If n >= 0, consider buffers having
				// same directory as current screen (only) as candidates.  If n <= 0, delete current buffer
				// after switch.
				if(n != INT_MIN) {
					if(n >= 0)
						i |= BT_HomeDir;
					if(n <= 0)
						i |= BT_Delete;
					}
				(void) prevNextBuf(pRtnVal, i);
				break;
			case cf_prevScreen:
				i = SWB_Repeat;
PNScreen:
				(void) gotoScreen(n, i);
				break;
			case cf_print:
				// Concatenate all arguments into pRtnVal and write result to message line.
				if(catArgs(pRtnVal, 1, NULL, 0) == Success)
					goto PrintMsg;
				break;
			case cf_printf:
				// Build message in pRtnVal and write result to message line.
				if(strFormat(pRtnVal, args[0], NULL) == Success) {
PrintMsg:
					(void) mlputs(n == INT_MIN ? MLHome | MLFlush | MLForce :
					 MLHome | MLFlush | MLForce | MLTermAttr, pRtnVal->str);
					dsetnil(pRtnVal);
					}
				break;
			case cf_queryReplace:
				// Search and replace with query.
				i = true;
				goto Repl;
			case cf_quickExit:
				// Quick exit from MightEMacs.  If any buffer has changed, do a save on that buffer first.
				sess.exitNArg = n;
				if(saveBufs(1, BS_QuickExit) == Success)
					(void) rsset(UserExit, RSForce, NULL);
				break;
			case cf_quote:
				// Convert any value to a string form which will resolve to the original value.
				if((i = dtos(pRtnVal, args[0], NULL, DCvtLang)) < 0)
					(void) libfail();
				else if(n <= 0)
					(void) endless(i);
				break;
			case cf_rand:
				dsetint(urand(args[0]->u.intNum), pRtnVal);
				break;
			case cf_readPipe:
				prompt = text170;
					// "Read pipe"
				i = 0;
				goto PipeCmd;
			case cf_reframeWind:
				// Reposition point in current window per the standard redisplay subsystem.  Row defaults to
				// zero to center current line in window.  If in script mode, also do partial screen update so
				// that the window is repositioned.
				sess.cur.pWind->reframeRow = (n == INT_MIN) ? 0 : n;
				sess.cur.pWind->flags |= WFReframe;
				if(sess.opFlags & OpScript)
					(void) wupd_reframe(sess.cur.pWind);
				break;
			case cf_replace:
				// Search and replace.
				i = false;
Repl:
				(void) replStr(pRtnVal, n, args, i);
				break;
			case cf_restoreBuf:
				// Restore the saved buffer.
				if(sess.pSavedBuf == NULL)
					return rsset(Failure, 0, text208, text83);
							// "Saved %s not found", "buffer"
				if(bswitch(sess.pSavedBuf, 0) == Success && dsetstr(sess.cur.pBuf->bufname, pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_restoreScreen:;
				// Restore the saved screen.
				EScreen *pScrn;

				// Find the screen.
				pScrn = sess.scrnHead;
				do {
					if(pScrn == sess.pSavedScrn) {
						dsetint((long) pScrn->num, pRtnVal);
						return sswitch(pScrn, 0);
						}
					} while((pScrn = pScrn->next) != NULL);
				(void) rsset(Failure, 0, text208, text380);
					// "Saved %s not found", "screen"
				sess.pSavedScrn = NULL;
				break;
			case cf_restoreWind:
				// Restore the saved window.
				{EWindow *pWind;

				// Find the window.
				pWind = sess.windHead;
				do {
					if(pWind == sess.pSavedWind) {
						sess.cur.pWind->flags |= WFMode;
						(void) wswitch(pWind, 0);
						sess.cur.pWind->flags |= WFMode;
						dsetint((long) getWindNum(sess.cur.pWind), pRtnVal);
						return sess.rtn.status;
						}
					} while((pWind = pWind->next) != NULL);
				(void) rsset(Failure, 0, text208, text331);
					// "Saved %s not found", "window"
				}
				break;
			case cf_revertYank:
				(void) yrevert(SF_Yank | SF_Undel);
				break;
			case cf_saveBuf:
				// Save pointer to current buffer.
				sess.pSavedBuf = sess.cur.pBuf;
				if(dsetstr(sess.cur.pBuf->bufname, pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_saveFile:
				// Save the contents of the current buffer (or all buffers if n argument) to their associated
				// files.
				(void) saveBufs(n, 0);
				break;
			case cf_saveScreen:
				// Save pointer to current screen.
				sess.pSavedScrn = sess.cur.pScrn;
				break;
			case cf_saveWind:
				// Save pointer to current window.
				sess.pSavedWind = sess.cur.pWind;
				break;
			case cf_setWrapCol:
				// Set wrap column to N... or swap with previous value if n >= 0; display values if n < 0
				// and interactive.
				if(interactive() && n < 0 && n != INT_MIN)
					goto ShowWrapCol;
				if(n >= 0) {
					if(sess.cur.pScrn->prevWrapCol < 0)
						return rsset(Failure, RSNoFormat, text298);
							// "No previous wrap column set"
					n = sess.cur.pScrn->prevWrapCol;
					}
				else {
					if(getNArg(pRtnVal, text59) != Success || disnil(pRtnVal))
							// "Wrap column"
						return sess.rtn.status;
					if((n = pRtnVal->u.intNum) == sess.cur.pScrn->wrapCol)
ShowWrapCol:
						return rsset(Success, RSNoWrap | RSTermAttr, text496, sess.cur.pScrn->wrapCol,
								// "~bCurrent wrap col:~B %d, ~bprevious wrap col:~B %d"
						 sess.cur.pScrn->prevWrapCol);
					}
				if(setWrapCol(n) == Success)
					(void) rsset(Success, RSNoWrap, "%s %s %d (%s: %d)", text59, text278, n, text498,
							// "Wrap column", "changed to", "previous"
					 sess.cur.pScrn->prevWrapCol);
				break;
			case cf_shellCmd:
				prompt = "> ";
				i = PipePopOnly;
PipeCmd:
				(void) pipeCmd(pRtnVal, n, prompt, i);
				break;
			case cf_showDir:
				if(dsetstr(sess.cur.pScrn->workDir, pRtnVal) != 0)
					goto LibFail;
				(void) rsset(Success, RSNoFormat | RSNoWrap, sess.cur.pScrn->workDir);
				break;
			case cf_shQuote:
				if(toStr(args[0]) == Success && dshquote(args[0]->str, pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_shrinkWind:
				// Shrink the current window.  If n is negative, give lines to the upper window, otherwise
				// lower.
				i = -1;
GSWind:
				(void) chgWindSize(pRtnVal, n, i);
				break;
			case cf_space:
				i = EditSpace | EditWrap;
InsNLSPC:
				(void) insertNLSpace(n, i);
				break;
			case cf_splitWind:
				{EWindow *pWind;

				if(wsplit(n, &pWind) == Success)
					dsetint((long) getWindNum(pWind), pRtnVal);
				}
				break;
			case cf_sprintf:
				(void) strFormat(pRtnVal, args[0], NULL);
				break;
			case cf_statQ:
				(void) fileTest(pRtnVal, n, args[0], args[1]);
				break;
			case cf_strFit:
				if(args[1]->u.intNum < 0)
					(void) rsset(Failure, 0, text39, text290, (int) args[1]->u.intNum, 0);
							// "%s (%d) must be %d or greater", "Length argument"
				else {
					if(dsalloc(pRtnVal, args[1]->u.intNum + 1) != 0)
						goto LibFail;
					strfit(pRtnVal->str, args[1]->u.intNum, args[0]->str, 0);
					}
				break;
			case cf_strPop:
			case cf_strPush:
			case cf_strShift:
			case cf_strUnshift:
				(void) strFunc(pRtnVal, cmdFuncId);
				break;
			case cf_strip:
				if(dsetstr(stripStr(args[0]->str, n == INT_MIN ? 0 : n), pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_subline:
				{long longVal2 = argCount < 2 ? LONG_MAX : args[1]->u.intNum;
				longVal = args[0]->u.intNum;
				if(longVal2 != 0 && (i = sess.cur.pFace->point.pLine->used) > 0) {

					// Determine line offset and length and validate them.  Offset is position relative to
					// point.
					longVal += sess.cur.pFace->point.offset;
					if(longVal >= 0 && longVal < i &&
					 (longVal2 >= 0 || (longVal2 = i - longVal + longVal2) > 0)) {
						if(longVal2 > i - longVal)
							longVal2 = i - longVal;
						if(dsetsubstr(sess.cur.pFace->point.pLine->text + (int) longVal,
						 (size_t) longVal2, pRtnVal) != 0)
							goto LibFail;
						}
					else
						dsetnull(pRtnVal);
					}
				else
					dsetnull(pRtnVal);
				}
				break;
			case cf_substr:
				{long longVal2 = args[1]->u.intNum;
				long longVal3 = argCount < 3 ? LONG_MAX : args[2]->u.intNum;
				if(longVal3 != 0 && (longVal = strlen(args[0]->str)) > 0 && (longVal2 < 0 ? -longVal2 - 1 :
				 longVal2) < longVal) {
					if(longVal2 < 0)					// Negative offset.
						longVal2 += longVal;
					longVal -= longVal2;					// Maximum bytes can copy.
					if(longVal3 > 0 || (longVal3 += longVal) > 0) {
						if(dsetsubstr(args[0]->str + longVal2, (size_t) (longVal3 <= longVal ?
						 longVal3 : longVal), pRtnVal) != 0)
							goto LibFail;
						}
					else
						dsetnull(pRtnVal);
					}
				else
					dsetnull(pRtnVal);
				}
				break;
			case cf_tab:
				// Process a tab.  Normally bound to ^I.
				(void) edInsertTab(n == INT_MIN ? 1 : n);
				break;
			case cf_titleCaseLine:
				i = CaseLine | CaseTitle;
				goto CvtCase;
			case cf_titleCaseRegion:
				i = CaseRegion | CaseTitle;
				goto CvtCase;
			case cf_titleCaseWord:
				i = CaseWord | CaseTitle;
				goto CvtCase;
			case cf_toInt:
				dxfer(pRtnVal, args[0]);
				(void) toInt(pRtnVal);
				break;
			case cf_tr:
				(void) tr(pRtnVal, args[0], args[1], args[2]);
				break;
			case cf_truncBuf:
				// Truncate buffer.  Delete all text from current buffer position to beginning or end and save
				// in undelete buffer, without confirmation if not interactive or abs(n) >= 2.

				// No-op if currently at buffer boundary.
				{long bytes;
				bool yep;
				bool forw = n == INT_MIN || n > 0;
				bool confirm = n == INT_MIN || (n >= -1 && n <= 1);
				if(forw) {
					if(bufEnd(NULL))
						break;
					bytes = LONG_MAX;
					}
				else {
					if(bufBegin(NULL))
						break;
					bytes = LONG_MIN + 1;
					}

				// Get confirmation if no n-override and interactive.
				if(confirm && interactive()) {
					char workBuf[WorkBufSize];

					sprintf(workBuf, text463, forw ? text465 : text464);
						// "Truncate to %s of buffer", "end", "beginning"
					if(terminpYN(workBuf, &yep) != Success || !yep)
						break;
					}

				// Delete maximum possible, ignoring any end-of-buffer error.
				if(killPrep(false) == Success)
					(void) edelc(bytes, EditDel);
				}
				break;
			case cf_typeQ:
				(void) dsetstr(dtype(args[0], true), pRtnVal);
				break;
			case cf_undelete:
				// Yank text from the delete ring.
				(void) iorStr(NULL, n, Txt_Insert, ringTable + RingIdxDel);
				break;
			case cf_undeleteCycle:
				(void) ycycle(pRtnVal, n, ringTable + RingIdxDel, SF_Undel, cf_undelete);
				break;
			case cf_upperCaseLine:
				i = CaseLine | CaseUpper;
				goto CvtCase;
			case cf_upperCaseRegion:
				i = CaseRegion | CaseUpper;
				goto CvtCase;
			case cf_upperCaseStr:
				i = 1;
CvtStr:;
				char *(*chgCase)(char *dest, const char *src) = (i <= 0) ? makeLower : makeUpper;
				if(dsalloc(pRtnVal, strlen(args[0]->str) + 1) != 0)
					goto LibFail;
				chgCase(pRtnVal->str, args[0]->str);
				break;
			case cf_upperCaseWord:
				i = CaseWord | CaseUpper;
CvtCase:
				(void) convertCase(n, i);
				break;
			case cf_viewFile:
				i = true;
FVFile:
				(void) findViewFile(pRtnVal, n, i);
				break;
			case cf_writeFile:
				i = 'w';
AWFile:
				(void) appendWriteFile(pRtnVal, n, i == 'w' ? text144 : text218, i);
						// "Write to: ", "Append to"
				break;
			case cf_yank:
				// Yank text from the kill ring.
				(void) iorStr(NULL, n, Txt_Insert, ringTable + RingIdxKill);
				break;
			case cf_yankCycle:
				(void) ycycle(pRtnVal, n, ringTable + RingIdxKill, SF_Yank, cf_yank);
			}
		}

	// Command or function call completed.  Extra arguments in script mode are checked in fcall() so no need to do it here.
	return sess.rtn.status == Success ? rssave() : sess.rtn.status;
LibFail:
	return libfail();
	}

// Parse an escaped character sequence, given pointer to leading backslash (\) character.  A sequence includes numeric form \nn,
// where \0x... and \x... are hexadecimal, otherwise octal.  Source pointer is updated after parsing is completed.  If
// allowNull is true, a null (zero) value is allowed; otherwise, an error is returned.  If pChar is not NULL, result is returned
// in *pChar.  Return status.
int evalCharLit(char **pSrc, short *pChar, bool allowNull) {
	short c, c1;
	int base = 8;
	int maxLen = 3;
	char *src0 = *pSrc;
	char *src = src0 + 1;		// Past backslash.
	char *src1;

	// Parse \x and \0n... sequences.
	switch(*src++) {
		case 't':	// Tab
			c = '\t'; break;
		case 'r':	// CR
			c = '\r'; break;
		case 'n':	// NL
			c = '\n'; break;
		case 'e':	// Escape
			c = '\e'; break;
		case 's':	// Space
			c = 040; break;
		case 'f':	// Form feed
			c = '\f'; break;
		case 'v':	// Vertical tab
			c = '\v'; break;
		case 'x':
			goto IsNum;
		case '0':
			if(*src == 'x') {
				++src;
IsNum:
				base = 16;
				maxLen = 2;
				goto GetNum;
				}
			// Fall through.
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			--src;
GetNum:
			// \nn found.  src is at first digit (if any).  Decode it.
			c = 0;
			src1 = src;
			while((c1 = *src) != '\0' && maxLen > 0) {
				if(c1 >= '0' && (c1 <= '7' || (c1 <= '9' && base != 8)))
					c = c * base + (c1 - '0');
				else {
					c1 = lowCase[c1];
					if(base == 16 && (c1 >= 'a' && c1 <= 'f'))
						c = c * 16 + (c1 - ('a' - 10));
					else
						break;
					}
				++src;
				--maxLen;
				}

			// Any digits decoded?
			if(src == src1)
				goto LitChar;

			// Null character... return error if not allowed.
			if(c == 0 && !allowNull)
				return rsset(Failure, 0, text337, (int)(src - src0), src0);
					// "Invalid escape sequence \"%.*s\""
			// Valid sequence.
			break;
		default:	// Literal character.
LitChar:
			c = src[-1];
		}

	// Return results.
	if(pChar != NULL)
		*pChar = c;
	*pSrc = src;
	return sess.rtn.status;
	}

// Evaluate a string literal and return result.  src is assumed to begin and end with ' or ".  In single-quoted strings, escaped
// backslashes '\\' and apostrophes '\'' are recognized (only); in double-quoted strings, escaped backslashes "\\", double
// quotes "\"", special letters (like "\n" and "\t"), \nnn octal and hexadecimal sequences, and Ruby-style interpolated #{}
// expressions are recognized (and executed); e.g., "Values are #{sub "#{join ', ', values, final}", ', ', delim} [#{count}]".
// Return status.
int evalStrLit(Datum *pRtnVal, char *src) {
	short termChar, c;
	DFab result;
#if MMDebug & Debug_Token
	char *src0 = src;
#endif
	// Get ready.
	if(dopenwith(&result, pRtnVal, FabClear) != 0)
		goto LibFail;
	termChar = *src++;

	// Loop until null or string terminator hit.
	while((c = *src) != termChar) {
#if MMDebug & Debug_Token
		if(c == '\0')
			return rsset(Failure, 0, "String terminator %c not found in %s", (int) termChar, src0);
#endif
		// Process escaped characters.  Backslash cannot be followed by '\0'.
		if(c == '\\') {

			// Do single-quote processing.
			if(termChar == '\'') {
				++src;

				// Check for escaped backslash or apostrophe only.
				if(*src == '\\' || *src == '\'')
					c = *src++;
				}

			// Do double-quote processing.
			else if(evalCharLit(&src, &c, false) != Success)
				return sess.rtn.status;
			}

		// Not a backslash.  Check for beginning of interpolation.
		else if(termChar == '"' && c == TokC_Expr && src[1] == TokC_ExprBegin) {
			Datum *pDatum;

			// "#{" found.  Execute what follows to "}" as a command line.
			if(dnewtrack(&pDatum) != 0)
				goto LibFail;
			if(execExprStmt(pDatum, src + 2, ParseExprEnd | ParseStmtEnd, &src) != Success)
				return sess.rtn.status;

			// Append the result.
			if(!disnil(pDatum)) {
				if(toStr(pDatum) != Success)
					return sess.rtn.status;
				if(dputs(pDatum->str, &result, 0) != 0)
					goto LibFail;
				}

			// Success.
			++src;
			continue;
			}
		else
			// Not an interpolated expression.  Vanilla character.
			++src;

		// Save the character.
		if(dputc(c, &result, 0) != 0)
			goto LibFail;
		}

	// Terminate the result and return.
	if(dclose(&result, FabStr) != 0)
LibFail:
		(void) libfail();
	return sess.rtn.status;
	}
