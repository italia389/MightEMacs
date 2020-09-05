// (c) Copyright 2020 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// misc.c	Miscellaneous functions for MightEMacs.
//
// This file contains command processing routines for some random commands.

#include "std.h"
#include "cxl/lib.h"
#include "bind.h"
#include "exec.h"
#include "cmd.h"
#include "search.h"
#include "var.h"

// Is a character a lower case letter?
bool isLowerCase(short c) {

	return (c >= 'a' && c <= 'z');
	}

// Is a character an upper case letter?
bool isUpperCase(short c) {

	return (c >= 'A' && c <= 'Z');
	}

// Is a character a letter?
bool is_letter(short c) {

	return isUpperCase(c) || isLowerCase(c);
	}

// Is a character a digit?
bool is_digit(short c) {

	return (c >= '0' && c <= '9');
	}

// Change the case of a letter.  First check lower and then upper.  If character is not a letter, it gets returned unchanged.
int chgCase(short c) {

	// Translate lowercase.
	if(isLowerCase(c))
		return upCase[c];

	// Translate uppercase.
	if(isUpperCase(c))
		return lowCase[c];

	// Let the rest pass.
	return c;
	}

// Copy a string from src to dest, changing its case.  Return dest.
static char *tranCase(char *dest, const char *src, char *tranTable) {
	char *str;

	str = dest;
	while(*src != '\0')
		*str++ = tranTable[(int) *src++];
	*str = '\0';

	return dest;
	}

// Copy a string from src to dest, making it lower case.  Return dest.
char *makeLower(char *dest, const char *src) {

	return tranCase(dest, src, lowCase);
	}

// Copy a string from src to dest, making it upper case.  Return dest.
char *makeUpper(char *dest, const char *src) {

	return tranCase(dest, src, upCase);
	}

// Initialize the character upper/lower case tables.
void initCharTables(void) {
	int i;

	// Set all of both tables to their indices.
	for(i = 0; i < 256; ++i)
		lowCase[i] = upCase[i] = i;

	// Set letter translations.
	for(i = 'a'; i <= 'z'; ++i) {
		upCase[i] = i ^ 0x20;
		lowCase[i ^ 0x20] = i;
		}

	// And those international characters also.
	for(i = (uchar) '\340'; i <= (uchar) '\375'; ++i) {
		upCase[i] = i ^ 0x20;
		lowCase[i ^ 0x20] = i;
		}
	}

// Reverse string in place and return it.
char *strrev(char *s) {
	char *strEnd = strchr(s, '\0');
	if(strEnd - s > 1) {
		short c;
		char *str = s;
		do {
			c = *--strEnd;
			*strEnd = *str;
			*str++ = c;
			} while(strEnd > str);
		}
	return s;
	}

// Get line number, given buffer and line pointer.
long getLineNum(Buffer *pBuf, Line *pTargLine) {
	Line *pLine;
	long n;

	// Start at the beginning of the buffer and count lines.
	pLine = pBuf->pFirstLine;
	n = 0;
	do {
		// If we have reached the target line, stop.
		if(pLine == pTargLine)
			break;
		++n;
		} while((pLine = pLine->next) != NULL);

	// Return result.
	return n + 1;
	}

// Return new column, given character in line and old column.
int newCol(short c, int col) {

	return col + (c == '\t' ? -(col % sess.hardTabSize) + sess.hardTabSize :
	 c < 0x20 || c == 0x7F ? 2 : c > 0x7F ? 4 : 1);
	}

// Return new column, given point and old column, adjusting for any terminal attribute specification if attribute enabled in
// current buffer.  *attrLen is set to length of specification, or zero if none.
int newColTermAttr(Point *pPoint, int col, int *attrLen) {
	short c = pPoint->pLine->text[pPoint->offset];
	if(c != AttrSpecBegin || !(sess.pCurBuf->flags & BFTermAttr)) {
		*attrLen = 0;
		return newCol(c, col);
		}

	// Found terminal attribute specification... process it.
	int inviz;
	char *str0, *str;
	ushort flags = TA_ScanOnly;
	str = (str0 = pPoint->pLine->text + pPoint->offset) + 1;
	(void) putAttrStr(str, pPoint->pLine->text + pPoint->pLine->used, &flags, (void *) &inviz);
	*attrLen = str - str0;
	return col + (*attrLen - inviz);
	}

// Return column of given point position.  If argument is NULL, use current point position.
int getCol(Point *pPoint, bool termAttr) {
	int i, col;

	if(pPoint == NULL)
		pPoint = &sess.pCurWind->face.point;

	col = 0;
	if(termAttr) {
		Point workPoint = *pPoint;
		int attrLen;
		for(workPoint.offset = 0; workPoint.offset < pPoint->offset; ++workPoint.offset) {
			col = newColTermAttr(&workPoint, col, &attrLen);
			if(attrLen > 0)
				workPoint.offset += attrLen - 1;
			}
		}
	else
		for(i = 0; i < pPoint->offset; ++i)
			col = newCol(pPoint->pLine->text[i], col);

	return col;
	}

// Try to set point to given column position.  Return status.
int setPointCol(int pos) {
	int i;			// Index into current line.
	int col;		// Current point column.
	int lineLen;		// Length of line in bytes.
	Point *pPoint = &sess.pCurWind->face.point;

	col = 0;
	lineLen = pPoint->pLine->used;

	// Scan the line until we are at or past the target column.
	for(i = 0; i < lineLen; ++i) {

		// Upon reaching the target, drop out.
		if(col >= pos)
			break;

		// Advance one character.
		col = newCol(pPoint->pLine->text[i], col);
		}

	// Set point to the new position and return.
	pPoint->offset = i;
	return sess.rtn.status;
	}

// Check if all white space from beginning of line, given length.  Return Boolean result, including true if length is zero.
bool isWhite(Line *pLine, int length) {
	short c;
	int i;

	for(i = 0; i < length; ++i) {
		c = pLine->text[i];
		if(c != ' ' && c != '\t')
			return false;
		}
	return true;
	}

// Find pattern within source, using given Match record if pMatch not NULL; otherwise, global "matchRE" record.  Return status.
// If Idx_Char flag set, pattern is assumed to be a single character; otherwise, a plain text pattern or regular expression.  If
// Idx_Last flag set, last (rightmost) match is found; otherwise, first.  pRtnVal is set to 0-origin match position, or nil if
// no match.
int strIndex(Datum *pRtnVal, ushort flags, Datum *pSrc, Datum *pPat, Match *pMatch) {

	if(flags & Idx_Char) {

		// No match if source or character is null.
		if(isCharVal(pPat) && !disnull(pSrc)) {
			int i;
			char *str;

			if((i = pPat->u.intNum) != 0) {
				if(!(flags & Idx_Last)) {

					// Find first occurrence.
					if((str = strchr(pSrc->str, i)) != NULL)
						goto Found;
					}
				else {
					// Find last (rightmost) occurrence.
					char *str0 = pSrc->str;
					str = strchr(str0, '\0');
					do {
						if(*--str == i) {
Found:
							dsetint(str - pSrc->str, pRtnVal);
							return sess.rtn.status;
							}
						} while(str != str0);
					}
				}
			}
		}

	// No match if source or pattern is null.
	else if(isStrVal(pPat) && !disnull(pSrc) && !disnull(pPat)) {

		// If match is NULL, compile pattern and save in global "matchRE" record.
		if(pMatch == NULL) {
			if(newSearchPat(pPat->str, pMatch = &matchRE, NULL, false) != Success ||
			 ((pMatch->flags & SOpt_Regexp) && compileRE(pMatch, (flags & Idx_Last) ?
			  SCpl_ForwardRE | SCpl_BackwardRE : SCpl_ForwardRE) != Success))
				return sess.rtn.status;
			grpFree(pMatch);
			}

		// Check pattern type.
		if(pMatch->flags & SOpt_Regexp) {
			regmatch_t group0;

			// Have regular expression.  Perform operation...
			if(regcmp(pSrc, (flags & Idx_Last) ? -1 : 0, pMatch, &group0) != Success)
				return sess.rtn.status;

			// and return index if a match was found.
			if(group0.rm_so >= 0) {
				dsetint((long) group0.rm_so, pRtnVal);
				return sess.rtn.status;
				}
			}
		else {
			char *src1, *srcEnd;
			size_t srcLen;
			int incr;
			MatchLoc matchLoc;
			int (*ncmp)(const char *s1, const char *s2, size_t n) = (pMatch->flags & SOpt_Ignore) ?
			 strncasecmp : strncmp;

			// Have plain text pattern.  Scan through the source string.
			matchLoc.strLoc.len = strlen(pPat->str);
			srcLen = strlen(src1 = pSrc->str);
			if(flags & Idx_Last) {
				srcEnd = src1 - 1;
				src1 += srcLen + (incr = -1);
				}
			else {
				srcEnd = src1 + srcLen;
				incr = 1;
				}

			do {
				// Scan through the string.  If match found, save results and return.
				if(ncmp(src1, pPat->str, matchLoc.strLoc.len) == 0) {
					dsetint((long) (src1 - pSrc->str), pRtnVal);
					matchLoc.strLoc.strPoint = src1;
					return saveMatch(pMatch, &matchLoc, NULL);
					}
				} while((src1 += incr) != srcEnd);
			}
		}

	// No match.
	dsetnil(pRtnVal);
	return sess.rtn.status;
	}

// Do "index" function.  Return status.
int fIndex(Datum *pRtnVal, int n, Datum **args) {
	ushort flags = 0;
	static Option options[] = {
		{"^Char", NULL, 0, Idx_Char},
		{"^Last", NULL, 0, Idx_Last},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text451, false, options};
			// "function option"

	// Get options and set flags.
	if(n != INT_MIN) {
		if(parseOpts(&optHdr, NULL, args[2], NULL) != Success)
			return sess.rtn.status;
		flags = getFlagOpts(options);
		}

	return strIndex(pRtnVal, flags, args[0], args[1], NULL);
	}

// Set or display i-variable parameters.  If interactive and n < 0, display parameters on message line; if n >= 0; set first
// parameter to n (only); otherwise, get arguments.  Return status.
int seti(Datum *pRtnVal, int n, Datum **args) {
	int i = iVar.i;
	int incr = iVar.incr;
	bool newFmt = false;

	// Process n argument if interactive.
	if(!(sess.opFlags & OpScript) && n != INT_MIN) {
		if(n >= 0) {
			iVar.i = n;
			return rsset(Success, 0, text287, iVar.i);
					// "i variable set to %d"
			}
		return rsset(Success, RSNoWrap | RSTermAttr, text384, iVar.i, iVar.format.str, iVar.incr);
						// "~bi:~B %d, ~bformat:~B \"%s\", ~binc:~B %d"
		}

	// Script mode or default n.  Get value(s).
	if(sess.opFlags & OpScript) {

		// Get "i" argument.
		i = (*args++)->u.intNum;

		// Have "format" argument?
		if(*args != NULL) {

			// Yes, get it.
			datxfer(pRtnVal, *args++);
			newFmt = true;

			// Have "incr" argument?
			if(*args != NULL) {

				// Yes, get it.
				incr = (*args)->u.intNum;
				}
			}
		}
	else {
		Datum *pDatum;
		TermInpCtrl termInpCtrl = {"1", RtnKey, 0, NULL};
		char numBuf[LongWidth];

		if(dnewtrack(&pDatum) != 0)
			return librsset(Failure);

		// Prompt for "i" value.
		if(termInp(pDatum, text102, 0, 0, &termInpCtrl) != Success || toInt(pDatum) != Success)
				// "Beginning value"
			return sess.rtn.status;
		i = pDatum->u.intNum;

		// Prompt for "format" value.
		termInpCtrl.defVal = iVar.format.str;
		termInpCtrl.delimKey = Ctrl | '[';
		if(termInp(pRtnVal, text235, ArgNotNull1, 0, &termInpCtrl) != Success)
				// "Format string"
			return sess.rtn.status;
		newFmt = true;

		// Prompt for "incr" value.
		sprintf(numBuf, "%d", incr);
		termInpCtrl.defVal = numBuf;
		termInpCtrl.delimKey = RtnKey;
		if(termInp(pDatum, text234, 0, 0, &termInpCtrl) != Success || toInt(pDatum) != Success)
				// "Increment"
			return sess.rtn.status;
		incr = pDatum->u.intNum;
		}

	// Validate arguments.  pRtnVal contains format string.
	if(incr == 0)				// Zero increment.
		return rsset(Failure, RSNoFormat, text236);
			// "i increment cannot be zero!"

	// Validate format string if changed.
	if(newFmt) {
		if(strcmp(pRtnVal->str, iVar.format.str) == 0)
			newFmt = false;
		else {
			int c, intSpecCount, otherSpecCount;
			bool inSpec = false;
			char *str = pRtnVal->str;

			intSpecCount = otherSpecCount = 0;
			do {
				c = *str++;
				if(inSpec) {
					switch(c) {
						case 'd':
						case 'o':
						case 'u':
						case 'x':
						case 'X':
							++intSpecCount;
							inSpec = false;
							break;
						default:
							if(is_letter(c)) {
								++otherSpecCount;
								inSpec = false;
								}
						}
					}
				else if(c == '%') {
					if(*str == '%')
						++str;
					else
						inSpec = true;
					}
				} while(*str != '\0');

			if(intSpecCount != 1 || otherSpecCount > 0)		// Bad format string.
				return rsset(Failure, 0, text237, pRtnVal->str);
					// "Invalid i format '%s' (must contain exactly one %%d, %%o, %%u, %%x, or %%X)"
			}
		}

	// Passed all edits... update iVar and return status.
	iVar.i = i;
	iVar.incr = incr;
	if(newFmt)
		datxfer(&iVar.format, pRtnVal);
	dsetnil(pRtnVal);
	return sess.rtn.status;
	}

// Return a pseudo-random integer in range 1..max.  If max <= 0, return zero.  This is a slight variation of the Xorshift
// pseudorandom number generator discovered by George Marsaglia.
long xorShift64Star(long max) {
	if(max <= 0)
		return 0;
	else if(max == 1)
		return 1;
	else {
		sess.randSeed ^= sess.randSeed >> 12; // a
		sess.randSeed ^= sess.randSeed << 25; // b
		sess.randSeed ^= sess.randSeed >> 27; // c
		}
	return ((sess.randSeed * 0x2545F4914F6CDD1D) & LONG_MAX) % max + 1;
	}

// Do include? system function.  Return status.  First argument is option string if n argument; otherwise, it is skipped.
int doIncl(Datum *pRtnVal, int n, Datum **args) {
	bool result;
	Array *pArray;
	ArraySize elCount;
	Datum *pDatum, **ppArrayEl;
	bool aryMatch, elMatch;
	bool firstArg = true;
	uint argFlags = ArgFirst;
	static bool any;
	static bool ignore;
	static Option options[] = {
		{"^All", NULL, OptFalse, .u.ptr = (void *) &any},
		{"^Ignore", NULL, 0, .u.ptr = (void *) &ignore},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		ArgNotNull1, text451, false, options};
			// "function option"

	if(dnewtrack(&pDatum) != 0)
		return librsset(Failure);

	// Get options and set flags.
	initBoolOpts(options);
	if(n != INT_MIN) {
		if(funcArg(pDatum, ArgFirst) != Success || parseOpts(&optHdr, NULL, pDatum, NULL) != Success)
			return sess.rtn.status;
		setBoolOpts(options);
		argFlags = 0;
		}

	// Get array argument.
	if(funcArg(pDatum, argFlags | ArgArray1) != Success)
		return sess.rtn.status;
	pArray = wrapPtr(pDatum)->pArray;

	// Get remaining argument(s).
	result = !any;
	for(;;) {
		if(!firstArg && !haveSym(s_comma, false))
			break;					// At least one argument retrieved and none left.
		if(funcArg(pDatum, ArgNIS1 | ArgBool1 | ArgArray1) != Success)
			return sess.rtn.status;
		firstArg = false;

		// Loop through array elements and compare them to the argument if final result has not yet been
		// determined.
		if(result == !any) {
			elCount = pArray->used;
			ppArrayEl = pArray->elements;
			elMatch = false;
			while(elCount-- > 0) {
				if((*ppArrayEl)->type == dat_blobRef) {
					if(pDatum->type != dat_blobRef)
						goto NextEl;
					if(arrayEQ(pDatum, *ppArrayEl, ignore, &aryMatch) != Success)
						return sess.rtn.status;
					if(aryMatch)
						goto Match;
					goto NextEl;
					}
				else if(dateq(pDatum, *ppArrayEl, ignore)) {
Match:
					if(any)
						result = true;
					else
						elMatch = true;
					break;
					}
NextEl:
				++ppArrayEl;
				}

			// Match found or all array elements checked.  Fail if "all" mode and no match found.
			if(!any && !elMatch)
				result = false;
			}
		}

	dsetbool(result, pRtnVal);
	return sess.rtn.status;
	}

// Perform binary search on a sorted array of string elements (which may be empty) given key string, array pointer, number of
// elements, comparison function pointer, fetch function pointer, and index pointer.  Return true if match found; otherwise,
// false.  In either case, set *pIndex to index of matching element or slot where key should be.
bool binSearch(const char *key, const void *table, ssize_t n, int (*cmp)(const char *str1, const char *str2),
 const char *(*fetch)(const void *table, ssize_t i), ssize_t *pIndex) {
	int compResult;
	ssize_t lo, hi;
	ssize_t i = 0;
	bool result = false;

	// Set current search limit to entire array.
	i = lo = 0;
	hi = n - 1;

	// Loop until a match found or array shrinks to zero items.
	while(hi >= lo) {

		// Get the midpoint.
		i = (lo + hi) >> 1;

		// Do the comparison and check result.
		if((compResult = cmp(key, fetch(table, i))) == 0) {
			result = true;
			break;
			}
		if(compResult < 0)
			hi = i - 1;
		else
			lo = ++i;
		}

	// Return results.
	*pIndex = i;
	return result;
	}

// Create an array with len elements, stash into pRtnVal, and store pointer to it in *ppAry.  Return status.
int makeArray(Datum *pRtnVal, ArraySize len, Array **ppAry) {

	return (*ppAry = anew(len, NULL)) == NULL ? librsset(Failure) : awWrap(pRtnVal, *ppAry);
	}

// Mode and group attribute control flags.
#define MG_Nil		0x0001		// Nil value allowed.
#define MG_Bool		0x0002		// Boolean value allowed.
#define MG_ModeAttr	0x0004		// Valid mode attribute.
#define MG_GrpAttr	0x0008		// Valid group attribute.
#define MG_ScopeAttr	0x0010		// Scope attribute.

#define MG_Global	2		// Index of "global" table entry.
#define MG_Hidden	4		// Index of "hidden" table entry.

static struct AttrInfo {
	const char *name;	// Attribute name.
	const char *abbr;	// Abbreviated name (for small screens).
	short letterIndex;	// Name index of interactive prompt letter.
	ushort ctrlFlags;		// Control flags.
	} attrInfoTable[] = {
		{"buffer", "pBuf", 0, MG_Bool | MG_ScopeAttr | MG_ModeAttr},
		{"description", "desc", 0, MG_ModeAttr | MG_GrpAttr},
		{"global", "globObj", 0, MG_Bool | MG_ScopeAttr | MG_ModeAttr},
		{"group", "grp", 1, MG_Nil | MG_ModeAttr},
		{"hidden", "hid", 0, MG_Bool | MG_ModeAttr},
		{"modes", "modes", 0, MG_Nil | MG_GrpAttr},
		{"visible", "viz", 0, MG_Bool | MG_ModeAttr}};

// Check if a built-in mode is set, depending on its scope, and return Boolean result.  If pBuf is NULL and needs to be
// accessed, check current buffer.
bool modeSet(ushort id, Buffer *pBuf) {
	ModeSpec *pModeSpec = modeInfo.cache[id];
	if(pModeSpec->flags & MdGlobal)
		return pModeSpec->flags & MdEnabled;
	if(pBuf == NULL)
		pBuf = sess.pCurBuf;
	return isBufModeSet(pBuf, pModeSpec);
	}

// Set a built-in global mode and flag mode line update if needed.
void setGlobalMode(ModeSpec *pModeSpec) {

	if(!(pModeSpec->flags & MdEnabled)) {
		pModeSpec->flags |= MdEnabled;
		if((pModeSpec->flags & (MdHidden | MdInLine)) != MdHidden)		// If mode line update needed...
			windNextIs(NULL)->flags |= WFMode;			// update mode line of bottom window.
		}
	}

// Clear a built-in global mode and flag mode line update if needed.
void clearGlobalMode(ModeSpec *pModeSpec) {

	if(pModeSpec->flags & MdEnabled) {
		pModeSpec->flags &= ~MdEnabled;
		if((pModeSpec->flags & (MdHidden | MdInLine)) != MdHidden)		// If mode line update needed...
			windNextIs(NULL)->flags |= WFMode;			// update mode line of bottom window.
		}
	}

// Set new description in mode or group record.
static int setDesc(const char *newDesc, char **pDesc) {

	if(newDesc != NULL) {
		if(*pDesc != NULL)
			free((void *) *pDesc);
		if((*pDesc = (char *) malloc(strlen(newDesc) + 1)) == NULL)
			return rsset(Panic, 0, text94, "setdesc");
				// "%s(): Out of memory!"
		strcpy(*pDesc, newDesc);
		}
	return sess.rtn.status;
	}

// binSearch() helper function for returning a mode name, given table (array) and index.
static const char *modeName(const void *table, ssize_t i) {

	return modePtr(((Datum **) table)[i])->name;
	}

// Search the mode table for given name and return pointer to ModeSpec object if found; otherwise, NULL.  In either case, set
// *pIndex to target array index if pIndex not NULL.
ModeSpec *msrch(const char *name, ssize_t *pIndex) {
	ssize_t i = 0;
	bool found = (modeInfo.modeTable.used == 0) ? false :
	 binSearch(name, (const void *) modeInfo.modeTable.elements, modeInfo.modeTable.used, strcasecmp, modeName, &i);
	if(pIndex != NULL)
		*pIndex = i;
	return found ? modePtr(modeInfo.modeTable.elements[i]) : NULL;
	}

// Check a proposed mode or group name for length and valid (printable) characters and return Boolean result.  It is assumed
// that name is not null.
static bool validMGName(const char *name, ushort type) {

	if(strlen(name) > MaxModeGrpName) {
		(void) rsset(Failure, 0, text280, type == MG_GrpAttr ? text388 : text387, MaxModeGrpName);
			// "%s name cannot be null or exceed %d characters", "Group", "Mode"
		return false;
		}
	const char *str = name;
	do {
		if(*str <= ' ' || *str > '~') {
			(void) rsset(Failure, 0, text397, type == MG_GrpAttr ? text390 : text389, text803, name);
					// "Invalid %s %s '%s'"		"group"		"mode", "name"
			return false;
			}
		} while(*++str != '\0');
	return true;
	}

// Create a user mode and, if ppModeSpec not NULL, set *ppModeSpec to ModeSpec object.  Return status.
int mcreate(const char *name, ssize_t index, const char *desc, ushort flags, ModeSpec **ppModeSpec) {
	ModeSpec *pModeSpec;
	Datum *pDatum;

	// Valid name?
	if(!validMGName(name, MG_ModeAttr))
		return sess.rtn.status;

	// All is well... create a mode record.
	if((pModeSpec = (ModeSpec *) malloc(sizeof(ModeSpec) + strlen(name))) == NULL)
		return rsset(Panic, 0, text94, "mcreate");
			// "%s(): Out of memory!"

	// Set the attributes.
	pModeSpec->pModeGrp = NULL;
	pModeSpec->flags = flags;
	strcpy(pModeSpec->name, name);
	pModeSpec->descrip = NULL;
	if(setDesc(desc, &pModeSpec->descrip) != Success)
		return sess.rtn.status;

	// Set pointer to object if requested.
	if(ppModeSpec != NULL)
		*ppModeSpec = pModeSpec;

	// Insert record into mode table array and return.
	if(dnew(&pDatum) != 0)
		goto LibFail;
	dsetblobref((void *) pModeSpec, 0, pDatum);
	if(ainsert(&modeInfo.modeTable, index, pDatum, false) != 0)
LibFail:
		(void) librsset(Failure);
	return sess.rtn.status;
	}

// Delete a user mode and return status.
static int mdelete(ModeSpec *pModeSpec, ssize_t index) {
	Datum *pDatum;

	// Built-in mode?
	if(!(pModeSpec->flags & MdUser))
		return rsset(Failure, 0, text396, text389, pModeSpec->name);
			// "Cannot delete built-in %s '%s'", "mode"

	// No, user mode.  Check usage.
	if(pModeSpec->flags & MdGlobal) {			// Global mode?
		if(pModeSpec->flags & MdEnabled)		// Yes, enabled?
			windNextIs(NULL)->flags |= WFMode;	// Yes, update mode line of bottom window.
		}
	else
		// Buffer mode.  Disable it in all buffers (if any).
		clearBufMode(pModeSpec);
	if(pModeSpec->pModeGrp != NULL)				// Is mode a group member?
		--pModeSpec->pModeGrp->useCount;		// Yes, decrement use count.

	// Usage check completed... nuke the mode.
	if(pModeSpec->descrip != NULL)				// If mode description present...
		free((void *) pModeSpec->descrip);			// free it.
	pDatum = adelete(&modeInfo.modeTable, index);		// Free the array element...
	free((void *) modePtr(pDatum));				// the ModeSpec object...
	ddelete(pDatum);					// and the Datum object.

	return rsset(Success, 0, "%s %s", text387, text10);
				// "Mode", "deleted"
	}

// Clear all global modes.  Return true if any mode was enabled; otherwise, false.
static bool globalClearModes(void) {
	Array *pArray = &modeInfo.modeTable;
	Datum *pArrayEl;
	ModeSpec *pModeSpec;
	bool modeWasChanged = false;

	while((pArrayEl = aeach(&pArray)) != NULL) {
		pModeSpec = modePtr(pArrayEl);
		if((pModeSpec->flags & (MdGlobal | MdEnabled)) == (MdGlobal | MdEnabled)) {
			clearGlobalMode(pModeSpec);
			modeWasChanged = true;
			}
		}
	return modeWasChanged;
	}

// Find a mode group by name and return Boolean result.
// If group is found:
//	If slot not NULL, *slot is set to prior node.
//	If ppModeGrp not NULL, *ppModeGrp is set to ModeGrp object.
// If group is not found:
//	If slot not NULL, *slot is set to list-insertion slot.
bool mgsrch(const char *name, ModeGrp **slot, ModeGrp **ppModeGrp) {
	int result;
	bool pRtnVal;

	// Scan the mode group list.
	ModeGrp *pModeGrp1 = NULL;
	ModeGrp *pModeGrp2 = modeInfo.grpHead;
	while(pModeGrp2 != NULL) {
		if((result = strcasecmp(pModeGrp2->name, name)) == 0) {

			// Found it.
			if(ppModeGrp != NULL)
				*ppModeGrp = pModeGrp2;
			pRtnVal = true;
			goto Retn;
			}
		if(result > 0)
			break;
		pModeGrp1 = pModeGrp2;
		pModeGrp2 = pModeGrp2->next;
		}

	// Group not found.
	pRtnVal = false;
Retn:
	if(slot != NULL)
		*slot = pModeGrp1;
	return pRtnVal;
	}

// Create a mode group and, if result not NULL, set *result to new object.  prior points to prior node in linked list.  Return
// status.
int mgcreate(const char *name, ModeGrp *prior, const char *desc, ModeGrp **result) {
	ModeGrp *pModeGrp;

	// Valid name?
	if(!validMGName(name, MG_GrpAttr))
		return sess.rtn.status;

	// All is well... create a mode group record.
	if((pModeGrp = (ModeGrp *) malloc(sizeof(ModeGrp) + strlen(name))) == NULL)
		return rsset(Panic, 0, text94, "mgcreate");
			// "%s(): Out of memory!"

	// Find the place in the list to insert this group.
	if(prior == NULL) {

		// Insert at the beginning.
		pModeGrp->next = modeInfo.grpHead;
		modeInfo.grpHead = pModeGrp;
		}
	else {
		// Insert after prior.
		pModeGrp->next = prior->next;
		prior->next = pModeGrp;
		}

	// Set the object attributes.
	pModeGrp->flags = MdUser;
	pModeGrp->useCount = 0;
	strcpy(pModeGrp->name, name);
	pModeGrp->descrip = NULL;
	(void) setDesc(desc, &pModeGrp->descrip);

	// Set pointer to object if requested and return.
	if(result != NULL)
		*result = pModeGrp;
	return sess.rtn.status;
	}

// Remove all members of a mode group, if any.
static void mgclear(ModeGrp *pModeGrp) {

	if(pModeGrp->useCount > 0) {
		ModeSpec *pModeSpec;
		Datum *pArrayEl;
		Array *pArray = &modeInfo.modeTable;
		while((pArrayEl = aeach(&pArray)) != NULL) {
			pModeSpec = modePtr(pArrayEl);
			if(pModeSpec->pModeGrp == pModeGrp) {
				pModeSpec->pModeGrp = NULL;
				if(--pModeGrp->useCount == 0)
					return;
				}
			}
		}
	}

// Delete a mode group and return status.
static int mgdelete(ModeGrp *pModeGrp1, ModeGrp *pModeGrp2) {

	// Built-in group?
	if(!(pModeGrp2->flags & MdUser))
		return rsset(Failure, 0, text396, text390, pModeGrp2->name);
			// "Cannot delete built-in %s '%s'", "group"

	// It's a go.  Clear the group, delete it from the group list, and free the storage.
	mgclear(pModeGrp2);
	if(pModeGrp1 == NULL)
		modeInfo.grpHead = pModeGrp2->next;
	else
		pModeGrp1->next = pModeGrp2->next;
	if(pModeGrp2->descrip != NULL)
		free((void *) pModeGrp2->descrip);
	free((void *) pModeGrp2);

	return rsset(Success, 0, "%s %s", text388, text10);
				// "Group", "deleted"
	}

// Remove a mode's group membership, if any.
static void membClear(ModeSpec *pModeSpec) {

	if(pModeSpec->pModeGrp != NULL) {
		--pModeSpec->pModeGrp->useCount;
		pModeSpec->pModeGrp = NULL;
		}
	}

// Make a mode a member of a group.  Don't allow if group already has members and mode's scope doesn't match members'.  Also, if
// group already has an active member, disable mode being added if enabled (because modes are mutually exclusive).  Return
// status.
static int membSet(ModeSpec *pModeSpec, ModeGrp *pModeGrp) {

	// Check if scope mismatch or any mode in group is enabled.
	if(pModeGrp->useCount > 0) {
		ushort globalType = USHRT_MAX;
		ModeSpec *pModeSpec2;
		Datum *pArrayEl;
		Array *pArray = &modeInfo.modeTable;
		bool activeFound = false;
		while((pArrayEl = aeach(&pArray)) != NULL) {
			pModeSpec2 = modePtr(pArrayEl);
			if(pModeSpec2->pModeGrp == pModeGrp) {			// Mode in group?
				if(pModeSpec2 == pModeSpec)			// Yes.  Is it the one to be added?
					return sess.rtn.status;			// Yes, nothing to do.
				if((globalType = pModeSpec2->flags & MdGlobal)) { // No.  Remember type and "active" status.
					if(pModeSpec2->flags & MdEnabled)
						activeFound = true;
					}
				else if(isBufModeSet(sess.pCurBuf, pModeSpec2))
					activeFound = true;
				}
			}
		if(globalType != USHRT_MAX && (pModeSpec->flags & MdGlobal) != globalType)
			return rsset(Failure, 0, text398, text389, pModeSpec->name, text390, pModeGrp->name);
				// "Scope of %s '%s' must match %s '%s'", "mode", "group"

		// Disable mode if an enabled mode was found in group.
		if(activeFound)
			(pModeSpec->flags & MdGlobal) ? clearGlobalMode(pModeSpec) :
			 isAnyBufModeSet(sess.pCurBuf, true, 1, pModeSpec);
		}

	// Group is empty or scopes match... add mode.
	membClear(pModeSpec);
	pModeSpec->pModeGrp = pModeGrp;
	++pModeGrp->useCount;

	return sess.rtn.status;
	}

// Ensure mutually-exclusive modes (those in the same group) are not set.
static void checkGrp(ModeSpec *pModeSpec, Buffer *pBuf) {

	if(pModeSpec->pModeGrp != NULL && pModeSpec->pModeGrp->useCount > 1) {
		ModeSpec *pModeSpec2;
		Datum *pArrayEl;
		Array *pArray = &modeInfo.modeTable;
		while((pArrayEl = aeach(&pArray)) != NULL) {
			if((pModeSpec2 = modePtr(pArrayEl)) != pModeSpec && pModeSpec2->pModeGrp == pModeSpec->pModeGrp) {
				if(pModeSpec2->flags & MdGlobal)
					clearGlobalMode(pModeSpec2);
				else
					isAnyBufModeSet(pBuf, true, 1, pModeSpec2);
				}
			}			
		}
	}

// binSearch() helper function for returning a mode or group attribute keyword, given table (array) and index.
static const char *attrInfoName(const void *table, ssize_t i) {

	return ((struct AttrInfo *) table)[i].name;
	}

// Check if mode scope change is valid.  Return true if so; otherwise, set an error and return false.
static bool scopeMatch(ModeSpec *pModeSpec) {

	// If mode not in a group or only member, scope-change is allowed.
	if(pModeSpec->pModeGrp == NULL || pModeSpec->pModeGrp->useCount == 1)
		return true;

	// No go... mode is in a group with other members.
	(void) rsset(Failure, 0, text398, text389, pModeSpec->name, text390, pModeSpec->pModeGrp->name);
		// "Scope of %s '%s' must match %s '%s'", "mode", "group"
	return false;
	}

// Process a mode or group attribute.  Return status.
static int doAttr(Datum *pAttr, ushort type, void *p) {
	ModeSpec *pModeSpec;
	ModeGrp *pModeGrp;
	struct AttrInfo *pAttrInfo;
	ssize_t i;
	ushort *flags;
	char **desc, *str0, *str1;
	char workBuf[strlen(pAttr->str) + 1];

	// Make copy of attribute string so unmodified original can be used for error reporting.
	strcpy(str0 = workBuf, pAttr->str);

	if(type == MG_ModeAttr) {					// Set control variables.
		pModeSpec = (ModeSpec *) p;
		flags = &pModeSpec->flags;
		desc = &pModeSpec->descrip;
		}
	else {
		pModeGrp = (ModeGrp *) p;
		flags = &pModeGrp->flags;
		desc = &pModeGrp->descrip;
		}

	str1 = strparse(&str0, ':');					// Parse keyword from attribute string.
	if(str1 == NULL || *str1 == '\0')
		goto ErrRtn;
	if(!binSearch(str1, (const void *) attrInfoTable,		// Keyword found.  Look it up.
	 elementsof(attrInfoTable), strcasecmp, attrInfoName, &i))
		goto ErrRtn;						// Not in attribute table.
	pAttrInfo = attrInfoTable + i;
	if(!(pAttrInfo->ctrlFlags & type))
		goto ErrRtn;						// Wrong type.

	if(*(str1 = strip(str0, 0)) == '\0')				// Valid keyword and type.  Get stripped value.
		goto ErrRtn;
	if(pAttrInfo->ctrlFlags & (MG_Nil | MG_Bool)) {			// If parameter accepts a nil or Boolean value...
		bool haveTrue;

		if(strcasecmp(str1, viz_true) == 0) {			// Check if true, false, or nil.
			haveTrue = true;
			goto DoBool;
			}
		if(strcasecmp(str1, viz_false) == 0) {
			haveTrue = false;
DoBool:
			if(!(pAttrInfo->ctrlFlags & MG_Bool))		// Have Boolean value... allowed?
				goto ErrRtn;				// No, error.
			if(pAttrInfo->ctrlFlags & MG_ScopeAttr) {	// Yes, setting mode's scope?
				if(pModeSpec->flags & MdLocked)		// Yes, error if mode is scope-locked.
					return rsset(Failure, 0, text399, text387, pModeSpec->name);
						// "%s '%s' has permanent scope", "Mode"
				if(i == MG_Global) {			// Determine ramifications and make changes.
					if(haveTrue)			// "global: true"
						goto BufToGlobal;
					goto GlobalToBuf;		// "global: false"
					}
				else if(haveTrue) {			// "buffer: true"
GlobalToBuf:								// Do global -> buffer.
					if(!(*flags & MdGlobal) || !scopeMatch(pModeSpec))
						return sess.rtn.status;	// Not global mode or scope mismatch.
					haveTrue = (*flags & MdEnabled);
					*flags &= ~(MdEnabled | MdGlobal);
					if(haveTrue)			// If mode was enabled, set it in current buffer.
						(void) setBufMode(sess.pCurBuf, pModeSpec, false, NULL);
					}
				else {					// "buffer: false"
BufToGlobal:								// Do buffer -> global.
					if((*flags & MdGlobal) || !scopeMatch(pModeSpec))
						return sess.rtn.status;	// Not buffer mode or scope mismatch.

					// If mode was enabled in current buffer, set it globally.
					haveTrue = isBufModeSet(sess.pCurBuf, pModeSpec);
					clearBufMode(pModeSpec);
					*flags |= haveTrue ? (MdGlobal | MdEnabled) : MdGlobal;
					}
				goto BFUpdate;
				}
			else {
				if(i == MG_Hidden) {
					if(haveTrue)			// "hidden: true"
						goto Hide;
					goto Expose;			// "hidden: false"
					}
				else if(haveTrue) {			// "visible: true"
Expose:
					if(*flags & MdHidden) {
						*flags &= ~MdHidden;
						goto FUpdate;
						}
					}
				else {					// "visible: false"
Hide:
					if(!(*flags & MdHidden)) {
						*flags |= MdHidden;
FUpdate:
						if(*flags & MdGlobal)
							windNextIs(NULL)->flags |= WFMode;
						else
BFUpdate:
							supd_windFlags(NULL, WFMode);
						}
					}
				}
			}
		else if(strcasecmp(str1, viz_nil) == 0) {
			if(!(pAttrInfo->ctrlFlags & MG_Nil))
				goto ErrRtn;
			if(pAttrInfo->ctrlFlags & MG_ModeAttr)		// "group: nil"
				membClear(pModeSpec);
			else						// "modes: nil"
				mgclear(pModeGrp);
			}
		else {
			if(pAttrInfo->ctrlFlags & MG_Nil)
				goto ProcString;
			goto ErrRtn;
			}
		}
	else {
ProcString:
		// str1 points to null-terminated value.  Process it.
		if((pAttrInfo->ctrlFlags & (MG_ModeAttr | MG_GrpAttr)) == (MG_ModeAttr | MG_GrpAttr))	// "description: ..."
			(void) setDesc(str1, desc);
		else if(pAttrInfo->ctrlFlags & MG_ModeAttr) {		// "group: ..."
			if(!mgsrch(str1, NULL, &pModeGrp))
				return rsset(Failure, 0, text395, text390, str1);
					// "No such %s '%s'", "group"
			(void) membSet(pModeSpec, pModeGrp);
			}
		else {							// "modes: ..."
			str0 = str1;
			do {
				str1 = strparse(&str0, ',');		// Get next mode name.
				if(str1 == NULL || *str1 == '\0')
					goto ErrRtn;
				if((pModeSpec = msrch(str1, NULL)) == NULL)
					return rsset(Failure, 0, text395, text389, str1);
						// "No such %s '%s'", "mode"
				if(membSet(pModeSpec, pModeGrp) != Success)
					break;
				} while(str0 != NULL);
			}
		}

	return sess.rtn.status;
ErrRtn:
	return rsset(Failure, 0, text397, type == MG_GrpAttr ? text390 : text389, text391, pAttr->str);
			// "Invalid %s %s '%s'"		"group"		"mode", "setting"
	}

// Prompt for mode or group attribute and value, convert to string (script) form, and save in pRtnVal.  Return status.
static int getAttr(Datum *pRtnVal, ushort type) {
	DFab prompt, result;
	Datum *pDatum;
	struct AttrInfo *pAttrInfo, *pAttrInfoEnd;
	const char *attrName;
	char *lead = " (";

	// Build prompt for attribute to set or change.
	if(dnewtrack(&pDatum) != 0 || dopenwith(&result, pRtnVal, FabClear) != 0 || dopenwith(&prompt, pDatum, FabClear) != 0 ||
	 dputc(upCase[(int) text186[0]], &prompt) != 0 || dputs(text186 + 1, &prompt) != 0)
					// "attribute"
		goto LibFail;
	pAttrInfoEnd = (pAttrInfo = attrInfoTable) + elementsof(attrInfoTable);
	do {
		if(pAttrInfo->ctrlFlags & type) {
			attrName = (term.cols >= 80) ? pAttrInfo->name : pAttrInfo->abbr;
			if(dputs(lead, &prompt) != 0 || (pAttrInfo->letterIndex == 1 && dputc(attrName[0], &prompt) != 0) ||
			 dputf(&prompt, "~u~b%c~Z%s", (int) attrName[pAttrInfo->letterIndex],
			  attrName + pAttrInfo->letterIndex + 1) != 0)
				goto LibFail;
			lead = ", ";
			}
		} while(++pAttrInfo < pAttrInfoEnd);
	if(dputc(')', &prompt) != 0 || dclose(&prompt, Fab_String) != 0)
		goto LibFail;

	// Get attribute from user and validate it.
	dclear(pRtnVal);
	if(termInp(pDatum, pDatum->str, ArgNotNull1 | ArgNil1, Term_LongPrmt | Term_Attr | Term_OneChar, NULL) != Success ||
	 pDatum->type == dat_nil)
		return sess.rtn.status;
	pAttrInfoEnd = (pAttrInfo = attrInfoTable) + elementsof(attrInfoTable);
	do {
		attrName = (term.cols >= 80) ? pAttrInfo->name : pAttrInfo->abbr;
		if(attrName[pAttrInfo->letterIndex] == pDatum->u.intNum)
			goto Found;
		} while(++pAttrInfo < pAttrInfoEnd);
	return rsset(Failure, 0, text397, type == MG_GrpAttr ? text390 : text389, text186, vizc(pDatum->u.intNum, VizBaseDef));
			// "Invalid %s %s '%s'"			"group", "mode", "attribute"
Found:
	// Return result if Boolean attribute; otherwise, get string value.
	if(dputf(&result, "%s: ", pAttrInfo->name) != 0)
		goto LibFail;
	if(pAttrInfo->ctrlFlags & MG_Bool) {
		if(dputs(viz_true, &result) != 0)
			goto LibFail;
		}
	else {
		if(termInp(pDatum, text53, (pAttrInfo->ctrlFlags & MG_Nil) ? ArgNotNull1 | ArgNil1 : ArgNotNull1,
				// "Value"
		 0, NULL) != Success || dtosf(pDatum, NULL, CvtShowNil, &result) != Success)
			return sess.rtn.status;
		}
	if(dclose(&result, Fab_String) != 0)
LibFail:
		(void) librsset(Failure);
	return sess.rtn.status;
	}

// Create prompt for editMode or editModeGroup command.
static int editMGPrompt(ushort type, int n, Datum *pDatum) {
	DFab prompt;

	return (dopenwith(&prompt, pDatum, FabClear) != 0 ||
	 dputf(&prompt, "%s %s", n == INT_MIN ? text402 : n <= 0 ? text26 : text403, text389) != 0 ||
					// "Edit"		"Delete", "Create or edit", "mode"
	 (type == MG_GrpAttr && dputf(&prompt, " %s", text390) != 0) || dclose(&prompt, Fab_String) != 0) ?
						// "group"
	 librsset(Failure) : sess.rtn.status;
	}

// Edit a user mode or group.  Return status.
static int editMG(Datum *pRtnVal, int n, Datum **args, ushort type) {
	void *p;
	Datum *pDatum;
	char *name;
	bool created = false;

	if(dnewtrack(&pDatum) != 0)
		return librsset(Failure);

	// Get mode or group name and validate it.
	if(sess.opFlags & OpScript)
		name = args[0]->str;
	else {
		// Build prompt.
		if(editMGPrompt(type, n, pDatum) != Success)
			return sess.rtn.status;

		// Get mode or group name from user.
		if(termInp(pDatum, pDatum->str, ArgNil1 | ArgNotNull1,
		 type == MG_ModeAttr ? Term_C_Mode : 0, NULL) != Success || pDatum->type == dat_nil)
			return sess.rtn.status;
		name = pDatum->str;
		}
	if(type == MG_ModeAttr) {
		ssize_t index;
		ModeSpec *pModeSpec;

		if((pModeSpec = msrch(name, &index)) == NULL) {			// Mode not found.  Create it?
			if(n <= 0)						// No, error.
				return rsset(Failure, 0, text395, text389, name);
						// "No such %s '%s'", "mode"
			if(mcreate(name, index, NULL, MdUser, &pModeSpec) != Success)	// Yes, create it.
				return sess.rtn.status;
			created = true;
			}
		else if(n <= 0 && n != INT_MIN)					// Mode found.  Delete it?
			return mdelete(pModeSpec, index);			// Yes.
		p = (void *) pModeSpec;
		}
	else {
		ModeGrp *pModeGrp1, *pModeGrp2;

		if(!mgsrch(name, &pModeGrp1, &pModeGrp2)) {					// Group not found.  Create it?
			if(n <= 0)						// No, error.
				return rsset(Failure, 0, text395, text390, name);
						// "No such %s '%s'", "group"
			if(mgcreate(name, pModeGrp1, NULL, &pModeGrp2) != Success)		// Yes, create it.
				return sess.rtn.status;
			created = true;
			}
		else if(n <= 0 && n != INT_MIN)					// Group found.  Delete it?
			return mgdelete(pModeGrp1, pModeGrp2);				// Yes.
		p = (void *) pModeGrp2;
		}

	// If interactive mode, get an attribute; otherwise, loop through attribute arguments and process them.
	if(!(sess.opFlags & OpScript)) {
		if(getAttr(pDatum, type) == Success && pDatum->type != dat_nil && doAttr(pDatum, type, p) == Success)
			rsset(Success, 0, "%s %s", type == MG_ModeAttr ? text387 : text388, created ? text392 : text394);
								// "Mode", "Group", "created", "attribute changed"
		}
	else {
		// Process any remaining arguments.
		while(haveSym(s_comma, false)) {
			if(funcArg(pDatum, ArgNotNull1) != Success || doAttr(pDatum, type, p) != Success)
				break;
			}
		}

	return sess.rtn.status;
	}

// Edit a user mode.  Return status.
int editMode(Datum *pRtnVal, int n, Datum **args) {

	return editMG(pRtnVal, n, args, MG_ModeAttr);
	}

// Edit a user mode group.  Return status.
int editModeGroup(Datum *pRtnVal, int n, Datum **args) {

	return editMG(pRtnVal, n, args, MG_GrpAttr);
	}

// Retrieve enabled global (pBuf is NULL) or buffer (pBuf not NULL) modes and save as an array of keywords in *pRtnVal.  Return
// status.
int getModes(Datum *pRtnVal, Buffer *pBuf) {
	Array *pArray0;

	if(makeArray(pRtnVal, 0, &pArray0) == Success) {
		Datum datum;

		dinit(&datum);
		if(pBuf == NULL) {
			Datum *pArrayEl;
			ModeSpec *pModeSpec;
			Array *pArray = &modeInfo.modeTable;
			while((pArrayEl = aeach(&pArray)) != NULL) {
				pModeSpec = modePtr(pArrayEl);
				if((pModeSpec->flags & (MdGlobal | MdEnabled)) == (MdGlobal | MdEnabled))
					if(dsetstr(pModeSpec->name, &datum) != 0 || apush(pArray0, &datum, true) != 0)
						goto LibFail;
				}
			}
		else {
			BufMode *pBufMode;

			for(pBufMode = pBuf->modes; pBufMode != NULL; pBufMode = pBufMode->next)
				if(dsetstr(pBufMode->pModeSpec->name, &datum) != 0 || apush(pArray0, &datum, true) != 0)
					goto LibFail;
			}
		dclear(&datum);
		}
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Check if a partial (or full) mode name is unique.  If "bufModeOnly" is true, consider buffer modes only; otherwise, global
// modes.  Set *ppModeSpec to mode table entry if true; otherwise, NULL.  Return status.
static int modeSearch(const char *name, bool bufModeOnly, ModeSpec **ppModeSpec) {
	Datum *pArrayEl;
	Array *pArray = &modeInfo.modeTable;
	ModeSpec *pModeSpec0 = NULL, *pModeSpec;
	size_t len = strlen(name);
	while((pArrayEl = aeach(&pArray)) != NULL) {
		pModeSpec = modePtr(pArrayEl);

		// Skip if wrong type of mode.
		if(bufModeOnly && (pModeSpec->flags & MdGlobal))
			continue;

		if(strncasecmp(name, pModeSpec->name, len) == 0) {

			// Exact match?
			if(pModeSpec->name[len] == '\0') {
				pModeSpec0 = pModeSpec;
				goto Retn;
				}

			// Error if not first match.
			if(pModeSpec0 != NULL)
				goto ERetn;
			pModeSpec0 = pModeSpec;
			}
		}

	if(pModeSpec0 == NULL)
ERetn:
		return rsset(Failure, 0, text66, name);
			// "Unknown or ambiguous mode '%s'"
Retn:
	*ppModeSpec = pModeSpec0;
	return sess.rtn.status;
	}

// Change a mode, given name, action (< 0: clear, 0: toggle, > 0: set), Buffer pointer if buffer mode (otherwise, NULL),
// optional "former state" pointer, and optional indirect ModeSpec pointer which is set to entry in mode table if mode was
// changed; otherwise, NULL.  If action < -1 or > 2, partial mode names are allowed.  Return status.
int chgMode1(const char *name, int action, Buffer *pBuf, long *formerStatep, ModeSpec **result) {
	ModeSpec *pModeSpec;
	bool modeWasSet, modeWasChanged;

	// Check if name in mode table.
	if(action < -1 || action > 2) {
		if(modeSearch(name, pBuf != NULL, &pModeSpec) != Success)
			return sess.rtn.status;
		}
	else if((pModeSpec = msrch(name, NULL)) == NULL)
		return rsset(Failure, 0, text395, text389, name);
				// "No such %s '%s'", "mode"

	// Match found... validate mode and process it.  Error if (1), wrong type; or (2), ASave mode, $autoSave is zero, and
	// setting mode or toggling it and ASave not currently set.
	if(((pModeSpec->flags & MdGlobal) != 0) != (pBuf == NULL))
		return rsset(Failure, 0, text66, name);
				// "Unknown or ambiguous mode '%s'"
	if(pModeSpec == modeInfo.cache[MdIdxASave] && sess.autoSaveTrig == 0 &&
	 (action > 0 || (action == 0 && !(pModeSpec->flags & MdEnabled))))
		return rsset(Failure, RSNoFormat, text35);
			// "$autoSave not set"

	if(pBuf != NULL) {

		// Change buffer mode.
		if(action <= 0) {
			modeWasSet = isAnyBufModeSet(pBuf, true, 1, pModeSpec);
			if(action == 0) {
				if(!modeWasSet && setBufMode(pBuf, pModeSpec, false, NULL) != Success)
					return sess.rtn.status;
				modeWasChanged = true;
				}
			else
				modeWasChanged = modeWasSet;
			}
		else {
			if(setBufMode(pBuf, pModeSpec, false, &modeWasSet) != Success)
				return sess.rtn.status;
			modeWasChanged = !modeWasSet;
			}
		}
	else {
		// Change global mode.
		modeWasSet = pModeSpec->flags & MdEnabled;
		if(action < 0 || (action == 0 && modeWasSet)) {
			modeWasChanged = modeWasSet;
			clearGlobalMode(pModeSpec);
			}
		else {
			modeWasChanged = !modeWasSet;
			setGlobalMode(pModeSpec);
			}
		}
	if(formerStatep != NULL)
		*formerStatep = modeWasSet ? 1L : -1L;

	// Ensure mutually-exclusive modes (those in the same group) are not set.
	if(modeWasChanged && !modeWasSet)
		checkGrp(pModeSpec, pBuf);

	if(result != NULL)
		*result = modeWasChanged ? pModeSpec : NULL;
	return sess.rtn.status;
	}

// Change a mode, given result pointer, action (clear: n < 0, toggle: n == 0 or default, set: n == 1, or clear all and set:
// n > 1), and argument vector.  Set pRtnVal to the former state (-1 or 1) of the last mode changed.  Return status.
int chgMode(Datum *pRtnVal, int n, Datum **args) {
	Datum *pDatum, *pArg, *pOldModes;
	ModeSpec *pModeSpec;
	Buffer *pBuf = NULL;
	long formerState = 0;			// Usually -1 or 1.
	int action = (n == INT_MIN) ? 0 : (n < 0) ? -1 : (n > 1) ? 2 : n;
	bool modeWasChanged = false;
	bool global;

	// Get ready.
	if(dnewtrack(&pDatum) != 0 || dnewtrack(&pOldModes) != 0)
		goto LibFail;

	// If interactive mode, build proper prompt string (e.g., "Toggle mode: ") and get buffer, if needed.
	if(!(sess.opFlags & OpScript)) {
		DFab prompt;

		// Build prompt.
		if(dopenwith(&prompt, pDatum, FabClear) != 0 ||
		 dputs(action < 0 ? text65 : action == 0 ? text231 : action == 1 ? text64 : text296, &prompt) != 0 ||
				// "Clear", "Toggle", "Set", "Clear all and set"
		 dputs(text63, &prompt) != 0 || dclose(&prompt, Fab_String) != 0)
				// " mode"
			goto LibFail;

		// Get mode name from user.  If action > 1 (clear all and set) and nothing entered, just return because there
		// is no way to know whether to clear buffer modes or global modes.
		if(termInp(pDatum, pDatum->str, ArgNil1, Term_C_Mode, NULL) != Success ||
		 pDatum->type == dat_nil)
			return sess.rtn.status;

		// Valid?
		if((pModeSpec = msrch(pDatum->str, NULL)) == NULL)
			return rsset(Failure, 0, text395, text389, pDatum->str);
					// "No such %s '%s'", "mode"

		// Get buffer, if needed.
		if(pModeSpec->flags & MdGlobal)
			global = true;
		else {
			if(n == INT_MIN)
				pBuf = sess.pCurBuf;
			else {
				pBuf = bdefault();
				if(getBufname(pRtnVal, text229 + 2, pBuf != NULL ? pBuf->bufname : NULL, OpDelete, &pBuf, NULL)
				 != Success || pBuf == NULL)
						// ", in"
					return sess.rtn.status;
				}
			global = false;
			}
		}
	else {
		// Script mode.  Check arguments.
		if((!(global = args[0]->type == dat_nil) && (pBuf = findBuf(args[0])) == NULL) ||
		 (n <= 1 && validateArg(args[1], ArgNotNull1 | ArgArray1 | ArgMay) != Success))
			return sess.rtn.status;
		}

	// Have buffer (if applicable) and, if interactive, one mode name in pDatum.  Save current modes so they can be passed
	// to mode hook, if set.
	if(getModes(pOldModes, pBuf) != 0)
		return sess.rtn.status;

	// Clear all modes initially, if applicable.
	if(action > 1)
		modeWasChanged = global ? globalClearModes() : bufClearModes(pBuf);

	// Process mode argument(s).
	if(!(sess.opFlags & OpScript)) {
		pArg = pDatum;
		goto DoMode;
		}
	else {
		char *optStr;
		Datum **ppArrayEl = NULL;
		ArraySize elCount;
		int status;

		// Script mode: get mode keywords.
		while((status = nextArg(&pArg, args + 1, pDatum, &optStr, &ppArrayEl, &elCount)) == Success) {
DoMode:
			if(chgMode1(pArg->str, action, pBuf, &formerState, &pModeSpec) != Success)
				return sess.rtn.status;

			// Do special processing for specific global modes that changed.
			if(pModeSpec != NULL) {
				if(!modeWasChanged)
					modeWasChanged = true;
				if(pModeSpec == modeInfo.cache[MdIdxHScrl]) {

					// If horizontal scrolling is now disabled, unshift any shifted windows on
					// current screen; otherwise, set shift of current window to line shift.
					if(!(pModeSpec->flags & MdEnabled)) {
						EWindow *pWind = sess.windHead;
						do {
							if(pWind->face.firstCol > 0) {
								pWind->face.firstCol = 0;
								pWind->flags |= (WFHard | WFMode);
								}
							} while((pWind = pWind->next) != NULL);
						}
					else if(sess.pCurScrn->firstCol > 0) {
						sess.pCurWind->face.firstCol = sess.pCurScrn->firstCol;
						sess.pCurWind->flags |= (WFHard | WFMode);
						}
					sess.pCurScrn->firstCol = 0;
					}
				}
			if(!(sess.opFlags & OpScript))
				break;
			}
		}

	// Display new mode line.
	if(!(sess.opFlags & OpScript) && mlerase() != Success)
		return sess.rtn.status;

	// Return former state of last mode that was changed.
	dsetint(formerState, pRtnVal);

	// Run mode-change hook if any mode was changed and either: (1), a global mode; or (2), target buffer is not hidden and
	// not a user command or function.  Hook arguments are either: (1), nil, old modes (if global mode was changed); or (2),
	// buffer name, old modes (if buffer mode was changed).
	if(modeWasChanged && (pBuf == NULL || !(pBuf->flags & (BFHidden | BFCommand | BFFunc)))) {
		if(global)
			dclear(pDatum);
		else if(dsetstr(pBuf->bufname, pDatum) != 0)
			goto LibFail;
		(void) execHook(NULL, INT_MIN, hookTable + HkMode, 0xA2, pDatum, pOldModes);
		}

	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}
