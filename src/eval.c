// (c) Copyright 2020 Richard W. Marinelli
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

// Forwards.
int dtosf(Datum *pDatum, const char *delim, uint flags, DFab *pFab);

// Return a pDatum object as a logical (Boolean) value.
bool toBool(Datum *pDatum) {

	// Check for numeric truth (!= 0).
	if(pDatum->type == dat_int)
		return pDatum->u.intNum != 0;

	// Check for logical false values (false and nil).  All other strings (including null strings) are true.
	return pDatum->type != dat_false && pDatum->type != dat_nil;
	}

// Check if given pDatum object is nil or null and return Boolean result.
bool isEmpty(Datum *pDatum) {

	return pDatum->type == dat_nil || disnull(pDatum);
	}

// Write an array to pFab (an active fabrication object) via calls to dtosf() and return status.  If CvtExpr flag is set,
// array is written in "[...]" form so that result can be subsequently evaluated as an expression; otherwise, it is written as
// data.  In the latter case, elements are separated by "delim" delimiters if not NULL.  A nil argument is included in result if
// CvtExpr or CvtKeepNil flag is set, and a null argument is included if CvtExpr or CvtKeepNull flag is set; otherwise, they are
// skipped.  In all cases, if the array includes itself, recursion stops and "[...]" is written for array if CvtForceArray flag
// is set; otherwise, an error is set.
static int atosf(Datum *pDatum, const char *delim, uint flags, DFab *pFab) {
	ArrayWrapper *pWrap = wrapPtr(pDatum);

	// Array includes self?
	if(pWrap->marked) {

		// Yes.  Store an ellipsis or set an error.
		if(!(flags & CvtForceArray))
			(void) rsset(Failure, RSNoFormat, text195);
				// "Endless recursion detected (array contains itself)"
		else if(dputs("[...]", pFab) != 0)
			goto LibFail;
		}
	else {
		Datum *pDatum;
		Array *pArray = pWrap->pArray;
		Datum **ppArrayEl = pArray->elements;
		ArraySize n = pArray->used;
		bool first = true;
		const char *realDelim = (flags & CvtExpr) ? ", " : delim;
		pWrap->marked = true;
		if(flags & CvtExpr) {
			flags |= CvtKeepAll;
			if(dputc('[', pFab) != 0)
				goto LibFail;
			}

		while(n-- > 0) {
			pDatum = *ppArrayEl++;

			// Skip nil or null string if appropriate.
			if(pDatum->type == dat_nil) {
				if(!(flags & CvtKeepNil))
					continue;
				}
			else if(disnull(pDatum) && !(flags & CvtKeepNull))
				continue;

			// Write optional delimiter and encoded value.
			if(!first && realDelim != NULL && dputs(realDelim, pFab) != 0)
				goto LibFail;
			if(dtosf(pDatum, delim, flags, pFab) != Success)
				return sess.rtn.status;
			first = false;
			}
		if((flags & CvtExpr) && dputc(']', pFab) != 0)
			goto LibFail;
		}

	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Add an array to wrapper list, clear all "marked" flags, and call atosf().
int atosfclr(Datum *pDatum, const char *delim, uint flags, DFab *pFab) {

	awGarbPush(pDatum);
	awClearMarks();
	return atosf(pDatum, delim, flags, pFab);
	}

// Write a pDatum object to pFab (an active fabrication object) in string form and return status.  If CvtExpr flag is set,
// quote strings and write nil values as keywords.  (Output is intended to be subsequently evaluated as an expression.)  If
// CvtExpr flag is not set, write strings in encoded (visible) form if CvtVizStr flag is set, and enclose in single (') or
// double (") quotes if CvtQuote1 or CvtQuote2 flag is set; otherwise, unmodified, and write nil values as a keyword if
// CvtShowNil flag is set; otherwise, a null string.  In all cases, write Boolean values as keywords, and call atosf() with
// delim and flag arguments to write arrays.
int dtosf(Datum *pDatum, const char *delim, uint flags, DFab *pFab) {

	// Determine type of pDatum object.
	if(pDatum->type & DStrMask) {
		Datum *pValue = pDatum;
		if(flags & CvtTermAttr) {
			DFab val;

			if(dopentrack(&val) != 0 || esctosf(pDatum->str, &val) != 0 || dclose(&val, Fab_String) != 0)
				return librsset(Failure);
			pValue = val.pDatum;
			}
		if(flags & CvtExpr)
			(void) quote(pFab, pValue->str, true);
		else {
			short qChar = flags & CvtQuote1 ? '\'' : flags & CvtQuote2 ? '"' : -1;
			if((flags & (CvtQuote1 | CvtQuote2)) && dputc(qChar, pFab) != 0)
				goto LibFail;
			if(flags & CvtVizStr) {
				if(dputvizmem(pValue->str, 0, VizBaseDef, pFab) != 0)
					goto LibFail;
				}
			else if(dputs(pValue->str, pFab) != 0)
				goto LibFail;
			if((flags & (CvtQuote1 | CvtQuote2)) && dputc(qChar, pFab) != 0)
				goto LibFail;
			}
		}
	else {
		const char *str;

		switch(pDatum->type) {
			case dat_int:
				if(dputf(pFab, "%ld", pDatum->u.intNum) != 0)
					goto LibFail;
				break;
			case dat_blobRef:	// Array
				(void) atosf(pDatum, delim, flags, pFab);
				break;
			case dat_nil:
				if(flags & (CvtExpr | CvtShowNil)) {
					str = viz_nil;
					goto Viz;
					}
				break;
			default:		// Boolean
				str = (pDatum->type == dat_false) ? viz_false : viz_true;
Viz:
				if(dputs(str, pFab) != 0)
					goto LibFail;
			}
		}

	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Call atosfclr() if array so that "marked" flags in wrapper list are cleared first; otherwise, call dtosf().
int dtosfchk(Datum *pDatum, const char *delim, uint flags, DFab *pFab) {
	int (*func)(Datum *pDatum, const char *delim, uint flags, DFab *pFab) =
	 (pDatum->type == dat_blobRef) ? atosfclr : dtosf;
	return func(pDatum, delim, flags, pFab);
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
		return librsset(Failure);
	if(awWrap(pRtnVal, pArray) != Success)
		return sess.rtn.status;

	// Create unique arrays if initializer is an array.
	if(len > 0 && pInitializer != NULL && pInitializer->type == dat_blobRef) {
		Datum **ppArrayEl = pArray->elements;
		do {
			if(arrayClone(*ppArrayEl++, pInitializer, 0) != Success)
				return sess.rtn.status;
			} while(--len > 0);
		}

	return sess.rtn.status;
	}

// Split a string into an array and save in pRtnVal, given delimiter and optional limit value.  Return status.
int strSplit(Datum *pRtnVal, int n, Datum **args) {
	Array *pArray;
	short delimChar;
	const char *str;
	int limit = 0;

	// Get delimiter, string, and optional limit.
	if(!isCharVal(*args))
		return sess.rtn.status;
	if((delimChar = (*args++)->u.intNum) == '\0')
		return rsset(Failure, 0, text187, text409);
			// "%s cannot be null", "Delimiter"
	str = (*args++)->str;
	if(*args != NULL)
		limit = (*args)->u.intNum;

	return (pArray = asplit(delimChar, str, limit)) == NULL ? librsset(Failure) : awWrap(pRtnVal, pArray);
	}

// Copy string from src to pFab (an active fabrication object), adding a double quote (") at beginning and end (if full is
// true) and escaping all control characters, backslashes, and characters that are escaped by parsesym().  Return status.
int quote(DFab *pFab, const char *src, bool full) {
        short c;
	bool isChar;
	char *str;
	char workBuf[8];

	if(full) {
		c = '"';
		goto LitChar;
		}

	while((c = *src++) != '\0') {
		isChar = false;
		switch(c) {
			case '"':				// Double quote
				if(!full)
					goto LitChar;
				str = "\\\""; break;
			case '\\':				// Backslash
				str = "\\\\"; break;
			case '\r':				// CR
				str = "\\r"; break;
			case '\n':				// NL
				str = "\\n"; break;
			case '\t':				// Tab
				str = "\\t"; break;
			case '\b':				// Backspace
				str = "\\b"; break;
			case '\f':				// Form feed
				str = "\\f"; break;
			case 033:				// Escape
				str = "\\e"; break;
			default:
				if(c < ' ' || c >= 0x7F)	// Non-printable character.
					sprintf(str = workBuf, "\\%.3o", c);
				else				// Literal character.
LitChar:
					isChar = true;
			}

		if((isChar ? dputc(c, pFab) : dputs(str, pFab)) != 0)
			return librsset(Failure);
		}
	if(full && dputc('"', pFab) != 0)
		(void) librsset(Failure);

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
			sess.hardTabSize = size;
		else
			sess.softTabSize = size;
		(void) rsset(Success, 0, text332, hard ? text49 : text50, size);
				// "%s tab size set to %d", "Hard", "Soft"
		}

	return sess.rtn.status;
	}

// Strip whitespace off the beginning (op == -1), the end (op == 1), or both ends (op == 0) of a string.
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
			 pMatch->grpMatch.groups[pReplPat->u.grpNum].str, result) != 0)
				return -1;
			}
	else if(dputs(pMatch->replPat, result) != 0)
		return -1;
	return 0;
	}

// Substitute first occurrence (or all if n > 1) of subPat in pSrc with replacement pattern in 'pMatch' and store results in
// pRtnVal.  Ignore case in comparisons if flag set in 'flags'.  Return status.
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
		if((srcLen = s - str) > 0 && dputmem((void *) str, srcLen, &result) != 0)
			goto LibFail;
		str = s + subPatLen;

		// If RRegical flag set, then no metacharacters were found in the search pattern (because this routine was
		// called), but at least one ampersand was found in the replacement pattern.  In this case, we need to set up
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
	if((srcLen = strlen(str)) > 0 && dputmem((void *) str, srcLen, &result) != 0)
		goto LibFail;
	if(dclose(&result, Fab_String) == 0)
		return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Perform RE substitution(s) in string 'pSrc' using search and replacement patterns in given Match object, and save result in
// 'pRtnVal'.  Do all occurrences of the search pattern if n > 1; otherwise, first only.  Return status.  'subPat' argument
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
			if(len > 0 && dputmem((void *) pSrc->str, len, &result) != 0)
				goto LibFail;

			// Copy replacement pattern.
			if(replCopy(pMatch, &result) != 0)
				goto LibFail;

			// Copy remaining source text to result if any, and close it.
			if(((len = strlen(pSrc->str + scanOffset + group0.rm_eo)) > 0 &&
			 dputs(pSrc->str + scanOffset + group0.rm_eo, &result) != 0) ||
			 dclose(&result, Fab_String) != 0) {
LibFail:
				(void) librsset(Failure);
				break;
				}

			// If no text remains or repeat count reached, we're done.
			if(len == 0 || n <= 1)
				break;

			// In "find all" mode... keep going.
			lastScanLen = strlen(pSrc->str) - scanOffset;
			scanOffset = strlen(pRtnVal->str) - len;
			datxfer(pSrc, pRtnVal);
			}
		else {
			// No match found.  Transfer input to result and bail out.
			datxfer(pRtnVal, pSrc);
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
		datxfer(pRtnVal, args[0]);
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
		if(newReplPat(args[2]->type == dat_nil ? "" : args[2]->str, &match, false) != Success)
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
					if(dputc(c1, pFab) != 0)
						goto LibFail;
				++str;
				break;
			case '\\':
				if(str[1] != '\0')
					c1 = *++str;
				// Fall through.
			default:
LitChar:
				if(dputc(c1, pFab) != 0)
					goto LibFail;
			}
		} while(*++str != '\0');

	if(dclose(pFab, Fab_String) != 0)
LibFail:
		(void) librsset(Failure);
	return sess.rtn.status;
	}

// Prepare tr from and to strings.  Return status.
static int trPrep(Datum *pFrom, Datum *pTo) {
	DFab fab;

	// Expand "from" string.
	if(strExpand(&fab, pFrom->str) != Success)
		return sess.rtn.status;
	datxfer(pFrom, fab.pDatum);

	// Expand "to" string.
	if(pTo->type == dat_nil)
		dsetnull(pTo);
	else if(*pTo->str != '\0') {
		int lenFrom, lenTo;

		if(strExpand(&fab, pTo->str) != Success)
			return sess.rtn.status;
		datxfer(pTo, fab.pDatum);

		if((lenFrom = strlen(pFrom->str)) > (lenTo = strlen(pTo->str))) {
			short c = pTo->str[lenTo - 1];
			int n = lenFrom - lenTo;

			if(dopenwith(&fab, pTo, FabAppend) != 0)
				goto LibFail;
			do {
				if(dputc(c, &fab) != 0)
					goto LibFail;
				} while(--n > 0);
			if(dclose(&fab, Fab_String) != 0)
				goto LibFail;
			}
		}
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
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
				if(lenTo > 0 && dputc(pTo->str[fromStr - pFrom->str], &result) != 0)
					goto LibFail;
				goto Next;
				}
			} while(*++fromStr != '\0');

		// No match, copy the source char untranslated.
		if(dputc(*srcStr, &result) != 0)
			goto LibFail;
Next:
		++srcStr;
		}

	// Terminate and return the result.
	if(dclose(&result, Fab_String) != 0)
LibFail:
		(void) librsset(Failure);
	return sess.rtn.status;
	}

// Concatenate all function arguments into pRtnVal.  requiredCount is the number of required arguments.  ArgFirst flag is set on
// first argument.  Null and nil arguments are included in result if CvtKeepNil and/or CvtKeepNull flags are set; otherwise,
// they are skipped.  A nil argument is output as a keyword if CvtShowNil flag is set; otherwise, a null string.  Boolean
// arguments are always output as "false" and "true" and arrays are processed (recursively) as if each element was specified as
// an argument.  Return status.
int catArgs(Datum *pRtnVal, int requiredCount, Datum *pDelim, uint flags) {
	uint argFlags = ArgFirst | ArgBool1 | ArgArray1 | ArgNIS1;
	DFab fab;
	Datum *pDatum;		// For arguments.
	bool firstWrite = true;
	const char *delim = (pDelim != NULL && !isEmpty(pDelim)) ? pDelim->str : NULL;

	// Nothing to do if no arguments; for example, an "abort()" call.
	if((sess.opFlags & (OpScript | OpParens)) == (OpScript | OpParens) && haveSym(s_rightParen, false) &&
	 requiredCount == 0)
		return sess.rtn.status;

	if(dnewtrack(&pDatum) != 0 || dopenwith(&fab, pRtnVal, FabClear) != 0)
		goto LibFail;

	for(;;) {
		if(argFlags & ArgFirst) {
			if(!haveSym(s_any, requiredCount > 0))
				break;				// Error or no arguments.
			}
		else if(!haveSym(s_comma, false))
			break;					// No arguments left.
		if(funcArg(pDatum, argFlags) != Success)
			return sess.rtn.status;
		--requiredCount;

		// Skip nil or null string if appropriate.
		if(pDatum->type == dat_nil) {
			if(!(flags & CvtKeepNil))
				goto Onward;
			}
		else if(disnull(pDatum) && !(flags & CvtKeepNull))
			goto Onward;

		// Write optional delimiter and value.
		if(delim != NULL && !firstWrite && dputs(delim, &fab) != 0)
			goto LibFail;
		if(dtosfchk(pDatum, delim, flags, &fab) != Success)
			return sess.rtn.status;
		firstWrite = false;
Onward:
		argFlags = ArgBool1 | ArgArray1 | ArgNIS1;
		}

	// Return result.
	if(dclose(&fab, Fab_String) != 0)
LibFail:
		(void) librsset(Failure);
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
		if(pOldVarVal->type == dat_nil)
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
			if(pDelim->type == dat_nil)
				delimChar = '\0';
			else if(!isCharVal(pDelim))
				return sess.rtn.status;
			else if((delimChar = pDelim->u.intNum) == ' ')
				spaceDelim = true;
			}
		else if(pDelim->type == dat_nil)
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
			 dputs(str1, &fab) != 0)			// Copy initial portion of new var value to work buffer.
				goto LibFail;

			// Append a delimiter if pOldVarVal is not null, and value (if strPush) or var (if strUnshift).
			if((!disnull(pOldVarVal) && dputs(pDelim->str, &fab) != 0) || dputs(str2, &fab) != 0 ||
			 dclose(&fab, Fab_String) != 0)
				goto LibFail;
			pNewVar = pRtnVal;				// New var value.
			}
			break;
			}

	// Update variable and return status.
	(void) putVar(pNewVar, &varDesc);
Retn:
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

#if (MMDebug & Debug_Token) && 0
static void showSym(const char *name) {

	fprintf(logfile, "%s(): last is str \"%s\" (%d)\n", name, pLastParse->tok.str, pLastParse->sym);
	}
#endif

// Insert, overwrite, replace, or write the text in src to a buffer n times.  This routine's job is to make the given buffer
// the current buffer so that iorStr() can insert the text, then restore the current screen to its original form.  src, n, and
// style are passed to iorStr().  If pBuf is NULL, the current buffer is used.  Return status.
int iorText(const char *src, int n, ushort style, Buffer *pBuf) {
	EScreen *pOldScrn = NULL;
	EWindow *pOldWind = NULL;
	Buffer *pOldBuf = NULL;

	// If the target buffer is being displayed in another window, remember current window and move to the other one;
	// otherwise, switch to the buffer in the current window.
	if(pBuf != NULL && pBuf != sess.pCurBuf) {
		if(pBuf->windCount == 0) {

			// Target buffer is not being displayed in any window... switch to it in current window.
			pOldBuf = sess.pCurBuf;
			if(bswitch(pBuf, SWB_NoHooks) != Success)
				return sess.rtn.status;
			}
		else {
			// Target buffer is being displayed.  Get window and find screen.
			EWindow *pWind = findWind(pBuf);
			EScreen *pScrn = sess.scrnHead;
			EWindow *pWind2;
			pOldWind = sess.pCurWind;
			do {
				pWind2 = pScrn->windHead;
				do {
					if(pWind2 == pWind)
						goto Found;
					} while((pWind2 = pWind2->next) != NULL);
				} while((pScrn = pScrn->next) != NULL);
Found:
			// If screen is different, switch to it.
			if(pScrn != sess.pCurScrn) {
				pOldScrn = sess.pCurScrn;
				if(sswitch(pScrn, SWB_NoHooks) != Success)
					return sess.rtn.status;
				}

			// If window is different, switch to it.
			if(pWind != sess.pCurWind) {
				(void) wswitch(pWind, SWB_NoHooks);		// Can't fail.
				supd_windFlags(NULL, WFMode);
				}
			}
		}

	// Target buffer is current.  Now insert, overwrite, or replace the text in src n times.
	if(iorStr(src, n, style, NULL) == Success) {

		// Restore old screen, window, and/or buffer, if needed.
		if(pOldBuf != NULL)
			(void) bswitch(pOldBuf, SWB_NoHooks);
		else if(pOldScrn != NULL) {
			if(sswitch(pOldScrn, SWB_NoHooks) != Success)
				return sess.rtn.status;
			if(pOldWind != sess.pCurWind)
				goto RestoreWind;
			}
		else if(pOldWind != NULL) {
RestoreWind:
			(void) wswitch(pOldWind, SWB_NoHooks);		// Can't fail.
			supd_windFlags(NULL, WFMode);
			}
		}

	return sess.rtn.status;
	}

// Concatenate command-line arguments into pRtnVal and insert, overwrite, replace, or write the resulting text to a buffer n
// times, given text insertion style and buffer pointer.  If pBuf is NULL, use current buffer.  If n == 0, do one repetition and
// don't move point.  If n < 0, do one repetition and process all newline characters literally (don't create a new line).
// Return status.
int chgText(Datum *pRtnVal, int n, ushort style, Buffer *pBuf) {
	Datum *pArg;
	DFab text;
	uint argFlags = ArgFirst | ArgBool1 | ArgArray1 | ArgNIS1;

	if(n == INT_MIN)
		n = 1;

	if(dnewtrack(&pArg) != 0)
		goto LibFail;

	// Evaluate all the arguments and save in fabrication object (so that the text can be inserted more than once,
	// if requested).
	if(dopenwith(&text, pRtnVal, FabClear) != 0)
		goto LibFail;

	for(;;) {
		if(!(argFlags & ArgFirst) && !haveSym(s_comma, false))
			break;					// No arguments left.
		if(funcArg(pArg, argFlags) != Success)
			return sess.rtn.status;
		argFlags = ArgBool1 | ArgArray1 | ArgNIS1;
		if(isEmpty(pArg))
			continue;				// Ignore null and nil values.
		if(pArg->type == dat_blobRef || (pArg->type & DBoolMask)) {
			if(dtosfchk(pArg, NULL, 0, &text) != Success)
				return sess.rtn.status;
			}
		else if(dputd(pArg, &text) != 0)		// Add text chunk to fabrication object.
			goto LibFail;
		}
	if(dclose(&text, Fab_String) != 0)
		goto LibFail;

	// We have all the text in pRtnVal.  Now insert, overwrite, or replace it n times.
	return iorText(pRtnVal->str, n, style, pBuf);
LibFail:
	return librsset(Failure);
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

// Return next argument to strFormat(), "flattening" arrays in the process.  Return status.
static int formatArg(Datum *pRtnVal, uint argFlags, ArrayState *pArrayState) {

	for(;;) {
		if(pArrayState->pArray == NULL) {
			if(funcArg(pRtnVal, argFlags | ArgArray1 | ArgMay) != Success)
				return sess.rtn.status;
			if(pRtnVal->type != dat_blobRef)
				break;
			pArrayState->pArray = wrapPtr(pRtnVal)->pArray;
			pArrayState->i = 0;
			}
		else {
			Array *pArray = pArrayState->pArray;
			if(pArrayState->i == pArray->used)
				pArrayState->pArray = NULL;
			else {
				if(datcpy(pRtnVal, pArray->elements[pArrayState->i]) != 0)
					return librsset(Failure);
				++pArrayState->i;
				break;
				}
			}
		}

	if(argFlags == ArgInt1)
		(void) isIntVal(pRtnVal);
	else if(argFlags == ArgNil1 && pRtnVal->type != dat_nil)
		(void) isStrVal(pRtnVal);
	return sess.rtn.status;
	}

// Build string from "printf" format string (format) and argument(s).  If pArg is not NULL, process binary format (%)
// expression using pArg as the argument; otherwise, process printf, sprintf, or bprintf function.
// Return status.
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
			if(dputc(c, &result) != 0)
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
			if(pArg != NULL) {
				if(pArg->type != dat_nil) {
					if(!isStrVal(pArg))	// Check pArg type.
						return sess.rtn.status;
					if(++specCount > 1)	// Check spec count.
						goto TooMany;
					}
				pFmtArg = pArg;
				}
			else if(formatArg(pFmtArg, ArgNil1, &aryState) != Success)
				return sess.rtn.status;
			if(pFmtArg->type == dat_nil)
				dsetnull(pFmtArg);
			sLen = strlen(str = pFmtArg->str);		// Length of string.
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
			if(dputs(prefix, &result) != 0)
				goto LibFail;
			prefix = NULL;
			}

		// If right-justified and chars to pad, store prefix string.
		if(padding > 0) {
			if(!(flags & FMTleft)) {
				short c1 = (flags & FMT0pad) ? '0' : ' ';
				while(--padding >= 0)
					if(dputc(c1, &result) != 0)
						goto LibFail;
				}
			}

		// Store prefix (if any).
		if(prefix != NULL && dputs(prefix, &result) != 0)
			goto LibFail;

		// Store (fixed-length) string.
		if(dputmem((void *) str, sLen, &result) != 0)
			goto LibFail;

 		// Store right padding.
		if(flags & FMTleft) {
			while(--padding >= 0)
				if(dputc(' ', &result) != 0)
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
	if(dclose(&result, Fab_String) != 0)
LibFail:
		(void) librsset(Failure);

	return sess.rtn.status;
	}

// Get a kill specified by n (of unlimited size) and save in pRtnVal.  May be a null string.  Return status.
static int getKill(Datum *pRtnVal, int n) {
	RingEntry *pRingEntry;

	// Which kill?
	if((pRingEntry = rget(ringTable + RingIdxKill, n)) != NULL && datcpy(pRtnVal, &pRingEntry->data) != 0)
		(void) librsset(Failure);
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

// Do mode? function.  Return status.
int modeQ(Datum *pRtnVal, int n, Datum **args) {
	int status;
	ModeSpec *pModeSpec;
	bool matchFound = false;
	Datum *pDatum, *pArg;
	char *optStr, *keyword;
	ArraySize elCount;
	Datum **ppArrayEl = NULL;
	Buffer *pBuf = NULL;
	bool result = false;
	static bool checkAll, ignore;
	static Option options[] = {
		{"^All", NULL, 0, .u.ptr = (void *) &checkAll},
		{"^Ignore", NULL, 0, .u.ptr = (void *) &ignore},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text451, false, options};
			// "function option"

	// Get buffer if specified.
	if(args[0]->type != dat_nil && (pBuf = findBuf(args[0])) == NULL)
		return sess.rtn.status;

	// Get options and set flags.
	initBoolOpts(options);
	if(n != INT_MIN) {
		if(parseOpts(&optHdr, NULL, args[2], NULL) != Success)
			return sess.rtn.status;
		setBoolOpts(options);
		if(checkAll)
			result = true;
		}

	// Get keyword(s).
	if(dnewtrack(&pDatum) != 0)
		return librsset(Failure);
	while((status = nextArg(&pArg, args + 1, pDatum, &optStr, &ppArrayEl, &elCount)) == Success) {
		keyword = pArg->str;

		// Search mode table for a match.
		matchFound = false;
		if((pModeSpec = msrch(keyword, NULL)) != NULL) {

			// Match found.  Mode enabled?
			matchFound = true;
			if(pBuf == NULL) {

				// Checking global modes.  Right type?
				if(!(pModeSpec->flags & MdGlobal))
					goto Wrong;
				if(pModeSpec->flags & MdEnabled)
					goto Good;
				goto Bad;
				}
			else {
				// Checking buffer modes.  Right type?
				if(pModeSpec->flags & MdGlobal)
Wrong:
					return rsset(Failure, 0, text456, keyword);
						// "Wrong type of mode '%s'"
				if(isBufModeSet(pBuf, pModeSpec)) {
Good:
					// Mode is enabled.  Done (success) if "any" check.
					if(!checkAll)
						result = true;
					}

				// Buffer mode not enabled.  Failure if "all" check.
				else {
Bad:
					if(checkAll)
						result = false;
					}
				}
			}

		// Unknown keyword.  Error if "Ignore" not specified.
		else if(!ignore)
			return rsset(Failure, 0, text66, keyword);
				// "Unknown or ambiguous mode '%s'"
		}

	// All arguments processed.  Return result.
	if(status == NotFound) {
		if(!matchFound)
			return rsset(Failure, 0, text446, text389);
				// "Valid %s not specified", "mode"
		dsetbool(result, pRtnVal);
		}
	return sess.rtn.status;
	}

// Check if a mode in given group is set and return its name if so; otherwise, nil.  Return status.
int groupModeQ(Datum *pRtnVal, int n, Datum **args) {
	ModeSpec *pModeSpec;
	ModeGrp *pModeGrp;
	Array *pArray;
	Datum *pArrayEl;
	ushort count;
	Buffer *pBuf = NULL;
	ModeSpec *pResultSpec = NULL;
	bool ignore = false;
	bool bufMode = false;
	static Option options[] = {
		{"^Ignore", NULL, 0, 0},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text451, false, options};
			// "function option"

	// Get buffer if specified.
	if(args[0]->type != dat_nil) {
		if((pBuf = findBuf(args[0])) == NULL)
			return sess.rtn.status;
		bufMode = true;
		}

	// Get options and set flags.
	if(n != INT_MIN) {
		if(parseOpts(&optHdr, NULL, args[2], NULL) != Success)
			return sess.rtn.status;
		if(options[0].ctrlFlags & OptSelected)
			ignore = true;
		}

	// Get mode group.
	if(!mgsrch(args[1]->str, NULL, &pModeGrp)) {
		if(ignore)
			goto SetNil;
		return rsset(Failure, 0, text395, text390, args[1]->str);
				// "No such %s '%s'", "group"
		}

	// If group has members, continue...
	if(pModeGrp->useCount > 0) {

		// Check for type mismatch; that is, a global mode specified for a buffer group, or vice versa.
		pArray = &modeInfo.modeTable;
		while((pArrayEl = aeach(&pArray)) != NULL) {
			pModeSpec = modePtr(pArrayEl);
			if(pModeSpec->pModeGrp == pModeGrp) {
				if(!(pModeSpec->flags & MdGlobal) != bufMode)
					return rsset(Failure, 0, text404, pModeGrp->name, bufMode ? text83 : text146);
						// "'%s' is not a %s group", "buffer", "global"
				break;
				}
			}

		// Have valid argument(s).  Check if any mode is enabled in group.
		pArray = &modeInfo.modeTable;
		count = 0;
		while((pArrayEl = aeach(&pArray)) != NULL) {
			pModeSpec = modePtr(pArrayEl);
			if(pModeSpec->pModeGrp == pModeGrp) {
				if(pBuf == NULL) {
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
			(void) librsset(Failure);
		}
	else
SetNil:
		dsetnil(pRtnVal);
	return sess.rtn.status;
	}

// Clone an array.  Return status.
int arrayClone(Datum *pDest, Datum *pSrc, int depth) {
	Array *pArray;

	if(maxArrayDepth > 0 && depth > maxArrayDepth)
		return rsset(Failure, 0, text319, text822, maxArrayDepth);
			// "Maximum %s recursion depth (%d) exceeded", "array"
	if((pArray = aclone(wrapPtr(pSrc)->pArray)) == NULL)
		return librsset(Failure);
	if(awWrap(pDest, pArray) == Success) {
		ArraySize n = pArray->used;
		Datum **ppArrayEl = pArray->elements;

		// Check for nested arrays.
		while(n-- > 0) {
			if((*ppArrayEl)->type == dat_blobRef && arrayClone(*ppArrayEl, *ppArrayEl, depth + 1) != Success)
				return sess.rtn.status;
			++ppArrayEl;
			}
		}
	return sess.rtn.status;
	}

// Convert any value to a string form which will resolve to the original value if subsequently evaluated as an expression
// (unless "force" is set (n > 0) and value is an array that includes itself).  Return status.
#if !(MMDebug & Debug_CallArg)
static
#endif
int quoteVal(Datum *pRtnVal, Datum *pDatum, uint flags) {
	DFab fab;

	if(dopenwith(&fab, pRtnVal, FabClear) != 0)
		goto LibFail;
	if(dtosfchk(pDatum, NULL, flags, &fab) == Success && dclose(&fab, Fab_String) != 0)
LibFail:
		(void) librsset(Failure);

	return sess.rtn.status;
	}

// Set wrap column to n.  Return status.
int setWrapCol(int n, bool msg) {

	if(n < 0)
		(void) rsset(Failure, 0, text39, text59, n, 0);
			// "%s (%d) must be %d or greater", "Wrap column"
	else {
		sess.prevWrapCol = sess.wrapCol;
		sess.wrapCol = n;
		if(msg)
			(void) rsset(Success, 0, "%s%s%d", text59, text278, n);
					// "Wrap column", " set to "
		}
	return sess.rtn.status;
	}

// Convert a string to title case.  Return status.
int titleCaseStr(Datum *pRtnVal, int n, Datum **args) {
	char *src, *dest;
	short c;
	bool inWord = false;

	if(dsalloc(pRtnVal, strlen(src = args[0]->str) + 1) != 0)
		return librsset(Failure);
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
		sess.pCurWind->face.point.offset = offset;
	return sess.rtn.status;
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
		if(maxArgs > 0) {
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
	if(pCmdFunc->func != NULL)
		(void) pCmdFunc->func(pRtnVal, n, args);
	else {
		int i;
		const char *prompt;
		long longVal;
		Buffer *pBuf;
		int cmdFuncId = pCmdFunc - cmdFuncTable;

		switch(cmdFuncId) {
			case cf_abs:
				dsetint(labs(args[0]->u.intNum), pRtnVal);
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
				Point *pPoint = &sess.pCurWind->face.point;
				(void) (sess.softTabSize > 0 && pPoint->offset > 0 ? delTab(n == INT_MIN ? -1 : -n, true) :
				 edelc(n == INT_MIN ? -1 : -n, 0));
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
					pBuf = sess.pCurBuf;
				else if((pBuf = bsrch(args[0]->str, NULL)) == NULL)
					return rsset(Failure, 0, text118, args[0]->str);
						// "No such buffer '%s'"

				// Return Boolean result.
				dsetbool(bempty(pBuf), pRtnVal);
				break;
			case cf_bprintf:

				// Get buffer pointer.
				if((pBuf = bsrch(args[0]->str, NULL)) == NULL)
					return rsset(Failure, 0, text118, args[0]->str);
						// "No such buffer '%s'"

				// Create formatted string in pRtnVal and insert into buffer.
				if(strFormat(pRtnVal, args[1], NULL) == Success)
					(void) iorText(pRtnVal->str, n == INT_MIN ? 1 : n, Txt_Insert, pBuf);
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
					dsetchr(args[0]->u.intNum, pRtnVal);
				break;
			case cf_clearMsgLine:
				(void) mlerase();
				break;
			case cf_clone:
				(void) arrayClone(pRtnVal, args[0], 0);
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
					dsetbool(args[0]->type == dat_nil ? true :
					 args[0]->type == dat_int ? args[0]->u.intNum == 0 :
					 args[0]->type & DStrMask ? *args[0]->str == '\0' :
					 wrapPtr(args[0])->pArray->used == 0, pRtnVal);
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
				(void) chgText(pRtnVal, n, Txt_Insert, NULL);
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
				dsetbool(!(sess.opFlags & OpStartup) && !(pLastParse->flags & OpScript), pRtnVal);
				break;
			case cf_join:
				if(needSym(s_comma, true))
					(void) catArgs(pRtnVal, 1, args[0], n == INT_MIN || n > 0 ? CvtKeepAll :
					 n == 0 ? CvtKeepNull : 0);
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
				if(sess.pCurScrn->pLastBuf != NULL) {
					Buffer *pOldBuf = sess.pCurBuf;
					if(render(pRtnVal, 1, sess.pCurScrn->pLastBuf, 0) == Success && n < 0 && n != INT_MIN) {
						char bufname[MaxBufname + 1];

						strcpy(bufname, pOldBuf->bufname);
					 	if(bdelete(pOldBuf, 0) == Success)
							(void) rsset(Success, 0, text372, bufname);
									// "Buffer '%s' deleted"
						}
					}
				break;
			case cf_length:
				dsetint(args[0]->type == dat_blobRef ? (long) wrapPtr(args[0])->pArray->used :
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
				{Match *pMatch = (n == INT_MIN) ? &matchRE : &searchCtrl.match;
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
				dsetbool(args[0]->type == dat_nil, pRtnVal);
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
					Point *pPoint = &sess.pCurWind->face.point;
					if(pPoint->pLine->used > 0 && pPoint->pLine->next->used == 0)
						(void) moveChar(1);
					}
				break;
			case cf_ord:
				dsetint((long) args[0]->str[0], pRtnVal);
				break;
			case cf_overwrite:
				(void) chgText(pRtnVal, n, Txt_Overwrite, NULL);
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
			case cf_pop:
			case cf_shift:
				{Array *pArray = wrapPtr(args[0])->pArray;
				if((args[1] = (cmdFuncId == cf_pop) ? apop(pArray) : ashift(pArray)) == NULL)
					dsetnil(pRtnVal);
				else {
					datxfer(pRtnVal, args[1]);
					if(pRtnVal->type == dat_blobRef)
						awGarbPush(pRtnVal);
					}
				}
				break;
			case cf_push:
			case cf_unshift:
				{Array *pArray = wrapPtr(args[0])->pArray;
				if(ainsert(pArray, cmdFuncId == cf_push ? pArray->used : 0, args[1], false) != 0)
					goto LibFail;
				else {
					duntrack(args[1]);
					datxfer(pRtnVal, args[0]);
					}
				}
				break;
			case cf_prevBuf:
				i = BT_Backward;
PNBuf:
				// Switch to the previous or next buffer in the buffer list.  If n == 0, switch once and include
				// buffers having same directory as current screen (only) as candidates.  If n < 0, switch once
				// and delete the current buffer.
				if(n <= 0 && n != INT_MIN) {
					i |= (n == 0 ? BT_HomeDir : BT_Delete);
					n = 1;
					}
				(void) prevNextBuf(pRtnVal, n, i);
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
					(void) mlputs(n == INT_MIN ? MLHome | MLFlush : MLHome | MLFlush | MLTermAttr,
					 pRtnVal->str);
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
				(void) quoteVal(pRtnVal, args[0], n > 0 ? CvtExpr | CvtForceArray : CvtExpr);
				break;
			case cf_rand:
				dsetint(xorShift64Star(args[0]->u.intNum), pRtnVal);
				break;
			case cf_readPipe:
				prompt = text170;
					// "Read pipe"
				i = 0;
				goto PipeCmd;
			case cf_reframeWind:
				// Reposition point in current window per the standard redisplay subsystem.  Row defaults to
				// zero to center current line in window.  If in script mode, also do partial screen update so
				// that the window is moved to new point location.
				sess.pCurWind->reframeRow = (n == INT_MIN) ? 0 : n;
				sess.pCurWind->flags |= WFReframe;
				if(sess.opFlags & OpScript)
					(void) wupd_reframe(sess.pCurWind);
				break;
			case cf_replace:
				// Search and replace.
				i = false;
Repl:
				(void) replStr(pRtnVal, n, args, i);
				break;
			case cf_replaceText:
				(void) chgText(pRtnVal, n, Txt_Replace, NULL);
				break;
			case cf_restoreBuf:
				// Restore the saved buffer.
				if(sess.pSavedBuf == NULL)
					return rsset(Failure, 0, text208, text83);
							// "Saved %s not found", "buffer"
				if(bswitch(sess.pSavedBuf, 0) == Success && dsetstr(sess.pCurBuf->bufname, pRtnVal) != 0)
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
						sess.pCurWind->flags |= WFMode;
						(void) wswitch(pWind, 0);
						sess.pCurWind->flags |= WFMode;
						dsetint((long) getWindNum(sess.pCurWind), pRtnVal);
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
				sess.pSavedBuf = sess.pCurBuf;
				if(dsetstr(sess.pCurBuf->bufname, pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_saveFile:
				// Save the contents of the current buffer (or all buffers if n argument) to their associated
				// files.
				(void) saveBufs(n, 0);
				break;
			case cf_saveScreen:
				// Save pointer to current screen.
				sess.pSavedScrn = sess.pCurScrn;
				break;
			case cf_saveWind:
				// Save pointer to current window.
				sess.pSavedWind = sess.pCurWind;
				break;
			case cf_setWrapCol:
				// Set wrap column to N, or previous value if n argument.
				if(n != INT_MIN) {
					if(sess.prevWrapCol < 0)
						return rsset(Failure, RSNoFormat, text298);
							// "No previous wrap column set"
					n = sess.prevWrapCol;
					}
				else {
					if(getNArg(pRtnVal, text59) != Success || pRtnVal->type == dat_nil)
							// "Wrap column"
						return sess.rtn.status;
					n = pRtnVal->u.intNum;
					}
				(void) setWrapCol(n, true);
				break;
			case cf_shellCmd:
				prompt = "> ";
				i = PipePopOnly;
PipeCmd:
				(void) pipeCmd(pRtnVal, n, prompt, i);
				break;
			case cf_showDir:
				if(dsetstr(sess.pCurScrn->workDir, pRtnVal) != 0)
					goto LibFail;
				(void) rsset(Success, RSNoFormat | RSNoWrap, sess.pCurScrn->workDir);
				break;
			case cf_shQuote:
				if(toStr(args[0]) == Success && dshquote(args[0]->str, pRtnVal) != 0)
					goto LibFail;
				break;
			case cf_shrinkWind:
				// Shrink the current window.  If n is negative, give lines to the upper window; otherwise,
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
				(void) wsplit(n, &pWind);
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
				if(longVal2 != 0 && (i = sess.pCurWind->face.point.pLine->used) > 0) {

					// Determine line offset and length and validate them.  Offset is position relative to
					// point.
					longVal += sess.pCurWind->face.point.offset;
					if(longVal >= 0 && longVal < i &&
					 (longVal2 >= 0 || (longVal2 = i - longVal + longVal2) > 0)) {
						if(longVal2 > i - longVal)
							longVal2 = i - longVal;
						if(dsetsubstr(sess.pCurWind->face.point.pLine->text + (int) longVal,
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
				datxfer(pRtnVal, args[0]);
				(void) toInt(pRtnVal);
				break;
			case cf_tr:
				(void) tr(pRtnVal, args[0], args[1], args[2]);
				break;
			case cf_truncBuf:;
				// Truncate buffer.  Delete all text from current buffer position to end (default or n > 0) or
				// beginning (n <= 0) and save in undelete buffer.

				// No-op if currently at buffer boundary.
				long bytes;
				bool yep;
				if(n == INT_MIN || n > 0) {
					if(bufEnd(NULL))
						break;
					bytes = LONG_MAX;
					yep = true;
					}
				else {
					if(bufBegin(NULL))
						break;
					bytes = LONG_MIN + 1;
					yep = false;
					}

				// Get confirmation if interactive.
				if(!(sess.opFlags & OpScript)) {
					char workBuf[WorkBufSize];

					sprintf(workBuf, text463, yep ? text465 : text464);
						// "Truncate to %s of buffer", "end", "beginning"
					if(terminpYN(workBuf, &yep) != Success || !yep)
						break;
					}

				// Delete maximum possible, ignoring any end-of-buffer error.
				if(killPrep(false) == Success)
					(void) edelc(bytes, EditDel);
				break;
			case cf_typeQ:
				(void) dsetstr(dtype(args[0], true), pRtnVal);
				break;
			case cf_undelete:
				// Yank text from the delete ring.
				if(n == INT_MIN)
					n = 1;
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
			case cf_wordCharQ:
				if(isCharVal(args[0]))
					dsetbool(wordChar[args[0]->u.intNum], pRtnVal);
				break;
			case cf_writeFile:
				i = 'w';
AWFile:
				(void) appendWriteFile(pRtnVal, n, i == 'w' ? text144 : text218, i);
						// "Write to: ", "Append to"
				break;
			case cf_yank:
				// Yank text from the kill ring.
				if(n == INT_MIN)
					n = 1;
				(void) iorStr(NULL, n, Txt_Insert, ringTable + RingIdxKill);
				break;
			case cf_yankCycle:
				(void) ycycle(pRtnVal, n, ringTable + RingIdxKill, SF_Yank, cf_yank);
			}
		}

	// Command or function call completed.  Extra arguments in script mode are checked in fcall() so no need to do it here.
	return sess.rtn.status == Success ? rssave() : sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Parse an escaped character sequence, given pointer to leading backslash (\) character.  A sequence includes numeric form \nn,
// where \0x... and \x... are hexadecimal; otherwise, octal.  Source pointer is updated after parsing is completed.  If
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
			c = 011; break;
		case 'r':	// CR
			c = 015; break;
		case 'n':	// NL
			c = 012; break;
		case 'e':	// Escape
			c = 033; break;
		case 's':	// Space
			c = 040; break;
		case 'f':	// Form feed
			c = 014; break;
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
			if(execExprStmt(pDatum, src + 2, TokC_ExprEnd, &src) != Success)
				return sess.rtn.status;

			// Append the result.
			if(pDatum->type != dat_nil) {
				if(toStr(pDatum) != Success)
					return sess.rtn.status;
				if(dputd(pDatum, &result) != 0)
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
		if(dputc(c, &result) != 0)
			goto LibFail;
		}

	// Terminate the result and return.
	if(dclose(&result, Fab_String) != 0)
LibFail:
		(void) librsset(Failure);
	return sess.rtn.status;
	}
