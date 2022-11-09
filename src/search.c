// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// search.c	Search functions for MightEMacs.
//
// These functions implement commands that search both forward and backward through a buffer.

#include "std.h"
#include "exec.h"
#include "var.h"
#if MMDebug & (Debug_SrchRepl | Debug_Regexp)
#include <ctype.h>
#endif

// Make selected global definitions local.
#define SearchData
#include "search.h"

#define max(a, b)	((a < b) ? b : a)

// Control object for Boyer-Moore (plain text pattern) buffer searches.
typedef struct {
	Point point;			// Current line and offset in buffer during scan.
	int lineBreakLimit;		// Number of line breaks which ends scan (or zero for no limit).
	ushort direct;			// Scanning direction -- Forward or Backward.
	uint scanCount;			// Number of characters scanned (for progress reporting).
	bool progMsgShown;		// Progress message displayed?
	} BMScan;

// Control object for RE buffer searches.
typedef struct {
	Point point;			// Current line and offset in buffer during scan.
	regoff_t offset;		// Current offset of scan point (>= 0) from starting position.
	int lineBreakLimit;		// Number of line breaks which ends scan (or zero for no limit).
	RegMatch regMatch;		// RE matching parameters.
	ushort matchFlags;		// Flags from Match object.
	uint scanCount;			// Number of characters scanned (for progress reporting).
	bool progMsgShown;		// Progress message displayed?
	} RegScan;

// Check if given search pattern has trailing option characters and return results.  Flags in *flags are initially cleared,
// then if any options were found, the appropriate flags are set and the source string is truncated at the OptCh_Begin
// character.  The final pattern length is returned.  The rule for interpreting a pattern is as follows: "If a pattern ends with
// a colon followed by one or more valid option letters, and the colon is not the first character of the pattern, then
// everything preceding the colon is the pattern.  In all other cases, the entire string is the pattern (with no options)."
int checkOpts(char *pat, ushort *flags) {
	char *options = strrchr(pat, OptCh_Begin);
	int patLen = strlen(pat);			// Assume no options exist.
	ushort searchFlags;

	*flags &= ~SOpt_All;				// Clear flags.
	if(options == NULL || options == pat)		// No option lead-in char or at beginning of pattern?
		return patLen;				// Yep, it's a no-go.

	pat = ++options;				// No, check for lower-case letters.
	while(isLowerCase(*pat))
		++pat;
	if(pat == options || *pat != '\0')		// No lowercase letters or other chars follow?
		return patLen;				// Yep, it's a no go.

	// Possible option string found.  Parse it.
	pat = options - 1;
	short c;
	struct FlagInfo {
		ushort optChar;		// Option character.
		ushort flag;		// Option flag.
		ushort xFlags;		// Duplicate and conflicting flags.
		} *pFlagInfo;
	static struct FlagInfo flagTable[] = {
		{OptCh_Ignore, SOpt_Ignore, SOpt_Ignore | SOpt_Exact},
		{OptCh_Exact, SOpt_Exact, SOpt_Exact | SOpt_Ignore},
#if FuzzySearch
		{OptCh_Fuzzy, SOpt_Fuzzy, SOpt_Fuzzy | SOpt_Plain},
		{OptCh_Plain, SOpt_Plain, SOpt_Plain | SOpt_Fuzzy | SOpt_Multi | SOpt_Regexp},
#else
		{OptCh_Plain, SOpt_Plain, SOpt_Plain | SOpt_Multi | SOpt_Regexp},
#endif
		{OptCh_Multi, SOpt_Multi, SOpt_Multi | SOpt_Plain},
		{OptCh_Regexp, SOpt_Regexp, SOpt_Regexp | SOpt_Plain},
		{0, 0, 0}};

	searchFlags = 0;
	do {
		c = *options++;
		pFlagInfo = flagTable;
		do {
			if(c == pFlagInfo->optChar) {
				if(searchFlags & pFlagInfo->xFlags)	// Duplicate or conflicting flags?
					break;			// Yes, bail out.
				searchFlags |= pFlagInfo->flag;
				goto Next;
				}
			} while((++pFlagInfo)->optChar != 0);

		// Invalid char found... assume it's part of the pattern.
		return patLen;
Next:;
		} while(*options != '\0');

#if FuzzySearch
	// No duplicate or conflicting flags found.  Enable Regexp if 'm' or 'f' specified.
	if(searchFlags & (SOpt_Fuzzy | SOpt_Multi))
#else
	// No duplicate or conflicting flags found.  Enable Regexp if 'm' specified.
	if(searchFlags & SOpt_Multi)
#endif
		searchFlags |= SOpt_Regexp;

	// Valid option string.  Truncate pattern, update flags, and return new length.
	*pat = '\0';
	*flags |= searchFlags;
	return patLen - (options - pat);
	}

// Return true if given Match record specifies Exact search mode.
bool exactSearch(Match *pMatch) {

	return (pMatch->flags & SOpt_Exact) ||
	 (!(pMatch->flags & SOpt_Ignore) && (pMatch != &bufSearch.match || modeSet(MdIdxExact, NULL)));
	}

// Return true if a Regexp buffer search is indicated by flags in Match record or Regexp global mode.
bool bufRESearch(void) {

	return (bufSearch.match.flags & SOpt_Regexp) || (modeSet(MdIdxRegexp, NULL) && !(bufSearch.match.flags & SOpt_Plain));
	}

// Return true if a plain text buffer search is indicated by flags in Match record or Regexp global mode, and presence of
// metacharacter(s) in search pattern if an RE.  It is assumed that any RE search pattern has already been compiled so that the
// SRegical flag is valid and can be checked.
bool bufPlainSearch(void) {

	return !bufRESearch() || !(bufSearch.match.flags & SRegical);
	}

// Compile search pattern for plain text buffer search and return status.
int compileBM(ushort flags) {
	unsigned compFlags;
	Match *pMatch = &bufSearch.match;

	// Remember type of compile.
	if(exactSearch(pMatch)) {
		pMatch->flags |= SCpl_PlainExact;
		compFlags = 0;
		}
	else {
		pMatch->flags &= ~SCpl_PlainExact;
		compFlags = PatIgnoreCase;
		}

	// Compile forward and backward patterns if requested.
	if(flags & SCpl_ForwardBM) {
#if MMDebug & Debug_Regexp
		fprintf(logfile, "compileBM(): compiling pattern '%s' with compFlags %.8x...\n",
		 pMatch->pat, compFlags);
#endif
		if(bmncomp(&bufSearch.forwBM, pMatch->pat, pMatch->patLen, compFlags) != 0)
			goto LibFail;
		}
	if(flags & SCpl_BackwardBM) {
		if(bmncomp(&bufSearch.backBM, pMatch->pat, pMatch->patLen, compFlags | PatReverse) != 0)
LibFail:
			return libfail();
		}
	return sess.rtn.status;
	}

// Free all heap space for plain text pattern compilation.
static void freeBM(void) {
	Match *pMatch = &bufSearch.match;

	if(pMatch->flags & SCpl_ForwardBM) {
		bmfree(&bufSearch.forwBM);
		if(pMatch->flags & SCpl_BackwardBM)
			bmfree(&bufSearch.backBM);
		pMatch->flags &= ~(SCpl_ForwardBM | SCpl_BackwardBM);
		}
	}

// Free all heap space for RE pattern in given Match object.
void freeRE(Match *pMatch) {

	if(pMatch->flags & SCpl_ForwardRE) {
		xregfree(&pMatch->regPat.compPat);
		if(pMatch->flags & SCpl_BackwardRE)
			xregfree(&pMatch->regPat.compBackPat);
		pMatch->flags &= ~(SCpl_ForwardRE | SCpl_BackwardRE);
		pMatch->grpCount = 0;
		}
	}

// Check if search pattern contains any regular expression metacharacters and set or clear SRegical flag accordingly.
static void metaCheck(Match *pMatch) {
	char *str = pMatch->pat;
	while(*str != '\0')
		if(strchr(Metacharacters, *str++) != NULL) {
			pMatch->flags |= SRegical;
			return;
			}
	pMatch->flags &= ~SRegical;
	}

// Set error message pertaining to code r returned from XRE library function and return Failure status.
static int regError(const char *pat, int r) {
	const char *text;
	const char *msg = xregmsg(r);

	// Possible errors.
// REG_BADPAT		// Invalid regular expression				" '%s'"
// REG_ECOLLATE		// Unknown collating element				" in RE pattern '%s'"
// REG_ECTYPE		// Unknown character class name.
// REG_EESCAPE		// Trailing backslash invalid.
// REG_ESUBREG		// Invalid back reference.
// REG_EBRACK		// Brackets '[ ]' not balanced
// REG_EPAREN		// Parentheses '( )' not balanced
// REG_EBRACE		// Braces '{ }' not balanced
// REG_BADBR		// Invalid repetition count(s) in '{ }'.
// REG_ERANGE		// Invalid character range in '[ ]'.
// REG_ESPACE		// Out of memory					", matching RE pattern '%s'"
// REG_BADRPT		// Invalid use of repetition operator.
// REG_EMPTY		// Empty (sub)expression.
// REG_EREGREV		// Cannot reverse back reference(s) in pattern		" '%s'"

	switch(r) {
		case REG_BADPAT:
		case REG_EREGREV:
			(void) rsset(Failure, 0, "%s '%s'", msg, pat);
			break;
		case REG_ESPACE:
			text = text255;
				// "%s, matching RE pattern '%s'"
			goto SetMsg;
		default:
			text = text221;
				// "%s in RE pattern '%s'"
SetMsg:
			(void) rsset(Failure, 0, text, msg, pat);
		}
	return sess.rtn.status;
	}

// Compile the RE string in the given Match object forward and/or backward per "flags", save in the "regPat" member, and return
// status.  The RE string is assumed to be non-empty, and memory used for any previous compile is assumed to have already been
// freed.  If SCpl_ForwardRE is set in flags, the forward pattern is compiled and the backward pattern is created.  If
// SCpl_BackwardRE is set in flags, the backward pattern is compiled (and is assumed to already exist).  In either case, if the
// buffer Match object is being processed and the pattern contains no metacharacters, compilation is skipped.
int compileRE(Match *pMatch, ushort flags) {

	// If compiling primary (forward) pattern, check if new pattern contains any metacharacters.
	if(flags & SCpl_ForwardRE)
		metaCheck(pMatch);

	// Proceed with compilation if non-buffer type or have metacharacter(s) in new pattern.
	if(pMatch != &bufSearch.match || (pMatch->flags & SRegical)) {
		int r, compFlags;

		// Set XRE compilation flags.
		compFlags = REG_ENHANCED | REG_NEWLINE | REG_APPROX;
		if(exactSearch(pMatch))
			pMatch->flags |= SCpl_RegExact;
		else {
			compFlags |= REG_ICASE;
			pMatch->flags &= ~SCpl_RegExact;
			}
		if(pMatch->flags & SOpt_Multi)
			compFlags |= REG_ANY;

		// Compile new forward pattern and/or backward pattern.
		if(flags & SCpl_ForwardRE) {
#if MMDebug & Debug_Regexp
			fprintf(logfile, "compileRE(): compiling pattern '%s' with compFlags %.8x...\n",
			 pMatch->pat, compFlags);
#endif
			if((r = xregcomp(&pMatch->regPat.compPat, pMatch->pat, compFlags)) == 0) {
				pMatch->flags |= SCpl_ForwardRE;
				pMatch->grpCount = pMatch->regPat.compPat.re_nsub;
				}
			else
				goto CompErr;
			}
		if(flags & SCpl_BackwardRE) {
			if((r = xregcomp(&pMatch->regPat.compBackPat, pMatch->pat, compFlags | REG_REVERSE)) == 0)
				pMatch->flags |= SCpl_BackwardRE;
			else
CompErr:
				return regError(pMatch->pat, r);
			}
		}

	return sess.rtn.status;
	}

// Free memory used for group matches in given Match object.  Note that pMatch->grpCount is not set to zero because it is the
// number of groups in the RE pattern, not the number of match strings in the group array.  Hence, it is zeroed in freeRE()
// instead.
void grpFree(Match *pMatch) {

	if(pMatch->grpMatch.size > 0) {
		GrpMatch *pGrpMatch = &pMatch->grpMatch;
		Datum *pDatum, *pDatumEnd;

		pDatumEnd = (pDatum = pGrpMatch->groups) + pGrpMatch->size;
		do {
			dclear(pDatum);
			} while(++pDatum < pDatumEnd);

		free((void *) pGrpMatch->groups);
		pGrpMatch->groups = NULL;
		pGrpMatch->size = 0;
		pLastMatch = NULL;
		}
	}

// Create and initialize Datum objects for group matches in given Match record.  Return status.
static int grpInit(Match *pMatch, ushort size) {
	GrpMatch *pGrpMatch = &pMatch->grpMatch;
	Datum *pDatum, *pDatumEnd;

	if((pGrpMatch->groups = (Datum *) malloc(sizeof(Datum) * size)) == NULL)
		return rsset(Panic, 0, text94, "grpInit");
				// "%s(): Out of memory!"
	pDatumEnd = (pDatum = pGrpMatch->groups) + (pGrpMatch->size = size);
	do {
		dinit(pDatum);
		} while(++pDatum < pDatumEnd);

	return sess.rtn.status;
	}

// Initialize a match object.
void minit(Match *pMatch) {
	pMatch->flags = pMatch->grpCount = pMatch->grpMatch.size = 0;
	pMatch->searchPatSize = pMatch->replPatSize = 0;
	pMatch->compReplPat = NULL;
	pMatch->grpMatch.groups = NULL;
	}

// Free all search pattern heap space in given Match object.
void freeSearchPat(Match *pMatch) {

	freeRE(pMatch);
	if(pMatch->searchPatSize > 0) {
		free((void *) pMatch->pat);
		pMatch->searchPatSize = 0;
		}
	}

// Make original search pattern from given Match record, store in dest, and return it.  *dest is assumed to be large enough to
// hold the result.
char *makePat(char *dest, Match *pMatch) {
	struct FlagInfo {
		ushort optChar;
		ushort flag;
		} *pFlagInfo;
	static struct FlagInfo flagTable[] = {
		{OptCh_Ignore, SOpt_Ignore},
		{OptCh_Exact, SOpt_Exact},
		{OptCh_Regexp, SOpt_Regexp},
		{OptCh_Plain, SOpt_Plain},
		{OptCh_Multi, SOpt_Multi},
#if FuzzySearch
		{OptCh_Fuzzy, SOpt_Fuzzy},
#endif
		{0, 0}};
	char *str = stpcpy(dest, pMatch->pat);
	if(pMatch->flags & SOpt_All) {
		*str++ = OptCh_Begin;
		pFlagInfo = flagTable;
		do {
			if(pMatch->flags & pFlagInfo->flag)
				*str++ = pFlagInfo->optChar;
			} while((++pFlagInfo)->optChar != 0);
		*str = '\0';
		}
	return dest;
	}

// Initialize parameters for new search pattern, which may be null.  If flags is NULL, check for pattern options via
// checkOpts(); otherwise, use *flags.  Return status.
int newSearchPat(char *pat, Match *pMatch, ushort *flags, bool addToRing) {

	// Get and strip search options, if any.
	if(flags == NULL)
		pMatch->patLen = checkOpts(pat, &pMatch->flags);
	else {
		pMatch->patLen = strlen(pat);
		pMatch->flags = (pMatch->flags & ~SOpt_All) | *flags;
		}

	// Free RE pattern.
	freeRE(pMatch);

	// Free up pattern arrays if too big or too small.
	if(pMatch->searchPatSize > PatSizeMax ||
	 (pMatch->searchPatSize > 0 && (uint) pMatch->patLen > pMatch->searchPatSize)) {
		free((void *) pMatch->pat);
		pMatch->searchPatSize = 0;
		}

	// Get heap space for arrays if needed.
	if(pMatch->searchPatSize == 0) {
		pMatch->searchPatSize = pMatch->patLen < PatSizeMin ? PatSizeMin : pMatch->patLen;
		if((pMatch->pat = (char *) malloc(pMatch->searchPatSize + 1)) == NULL)
			return rsset(Panic, 0, text94, "newSearchPat");
					// "%s(): Out of memory!"
		}

	// Save search pattern.
	strcpy(pMatch->pat, pat);
	if(pMatch == &bufSearch.match) {
		freeBM();
		if(addToRing) {
			// Add pattern to search ring.
			char patBuf[pMatch->patLen + OptCh_N + 1];
			(void) rsave(ringTable + RingIdxSearch, makePat(patBuf, pMatch), false);
			}
		}

	return sess.rtn.status;
	}

// Get a search or replacement pattern and stash it in global variable bufSearch.pat (searchPat is true) or bufSearch.replPat
// (searchPat is false) via newSearchPat() or newReplPat().  If script mode, *args contains the pattern; otherwise, get it
// interactively.  If the pattern is null, searchPat is true, and there is no old pattern, it is an error.  Return status.
int getPat(Datum **args, const char *prompt, bool searchPat) {
	Datum *pPat;

	if(dnewtrack(&pPat) != 0)
		return libfail();

	// Read a pattern.  Either we get one, or we just get the input delimiter and use the previous pattern.
	if(sess.opFlags & OpScript)
		dxfer(pPat, args[0]);
	else {
		TermInpCtrl termInpCtrl = {NULL, bufSearch.inpDelim, 0,
		 searchPat ? ringTable + RingIdxSearch : ringTable + RingIdxRepl};
		if(termInp(pPat, prompt, searchPat ? ArgNotNull1 | ArgNil1 : ArgNil1, 0, &termInpCtrl) != Success)
			return sess.rtn.status;
		if(disnil(pPat))
			dsetnull(pPat);
		}

	// Error if search pattern is null.
	if(searchPat) {
		if(disnull(pPat))
			return rsset(Failure, RSNoFormat, text80);
					// "No pattern set"
		// New search pattern?
		ushort flags = 0;
		(void) checkOpts(pPat->str, &flags);
		if(flags != (bufSearch.match.flags & SOpt_All) || strcmp(pPat->str, bufSearch.match.pat) != 0) {

			// Save the new search pattern.
			(void) newSearchPat(pPat->str, &bufSearch.match, &flags, true);
			}
		}
	else if(strcmp(pPat->str, bufSearch.match.replPat) != 0) {

		// Save the new replacement pattern.
		(void) newReplPat(pPat->str, &bufSearch.match, true);
		}

	return sess.rtn.status;
	}

// Create buffer search tables if needed.
static int makeSearchTables(void) {

	// Clear search groups.
	grpFree(&bufSearch.match);

	// Compile pattern as an RE if requested.
	if(bufRESearch() && !(bufSearch.match.flags & SCpl_ForwardRE) &&
	 compileRE(&bufSearch.match, SCpl_ForwardRE) != Success)
		return sess.rtn.status;

	// Compile as a plain-text pattern if not an RE request or not really an RE (SRegical not set).
	if(bufPlainSearch() && (!(bufSearch.match.flags & SCpl_ForwardBM) ||
	 (((bufSearch.match.flags & SCpl_PlainExact) != 0) != exactSearch(&bufSearch.match)))) {
		freeBM();
		(void) compileBM(SCpl_ForwardBM);
	 	}

	return sess.rtn.status;
	}

// Search forward or backward for text in current buffer matching a previously acquired search pattern.  If found, return the
// matched string, otherwise, false.
static int hunt(Datum *pRtnVal, int n, ushort direct) {

	if(n != 0 || direct == Backward) {	// Nothing to do if n is zero and searching forward.
		int lineBreakLimit;

		if(n == INT_MIN) {
			n = 1;
			lineBreakLimit = 0;
			}
		else if(n <= 0) {
			lineBreakLimit = abs(n) + direct;
			n = 1;
			}
		else
			lineBreakLimit = 0;			

		// Make sure a pattern exists, or that we didn't switch into Regexp mode after the pattern was entered.  Note
		// that a buffer search pattern always exists, including a null one if search ring is empty.
		if(bufSearch.match.pat[0] == '\0')
			return rsset(Failure, RSNoFormat, text80);
				// "No pattern set"

		// Create search tables if needed.
		if(makeSearchTables() == Success) {
			bool bufRegical = !bufPlainSearch();

			// If doing a backward scan, compile backward pattern if needed.
			if(direct == Backward) {
				if(bufRegical) {
					if(!(bufSearch.match.flags & SCpl_BackwardRE) &&
					 compileRE(&bufSearch.match, SCpl_BackwardRE) != Success)
						return sess.rtn.status;
					}
				else if(!(bufSearch.match.flags & SCpl_BackwardBM) &&
				 compileBM(SCpl_BackwardBM) != Success)
					return sess.rtn.status;
				}

			// Perform appropriate search and return result.
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "hunt(): calling %s() with n %d, line break limit %d, pat '%s'...\n",
			 bufRegical ? "regScan" : "scan", n, lineBreakLimit, bufSearch.match.pat);
#endif
			if((bufRegical ? regScan(n, &lineBreakLimit, direct, NULL) :
			  scan(n, &lineBreakLimit, direct, NULL)) == NotFound)
				dsetbool(false, pRtnVal);
			else if(dcpy(pRtnVal, bufSearch.match.grpMatch.groups) != 0)
				(void) libfail();
			}
		}

	return sess.rtn.status;
	}

// Search forward for a previously acquired search pattern, starting at point and proceeding toward the bottom of the buffer.
// If found, point is left at the character immediately following the matched string and the matched string is returned;
// otherwise, false is returned.
int huntForw(Datum *pRtnVal, int n, Datum **args) {

	return hunt(pRtnVal, n, Forward);
	}

// Search forward.  Get a search pattern and, starting at point, search toward the bottom of the buffer.  If found, point is
// left at the character immediately following the matched string and the matched string is returned; otherwise, false is
// returned.
int searchForw(Datum *pRtnVal, int n, Datum **args) {

	// Get the pattern and perform the search up to n times.
	if(getPat(args, text78, true) == Success && n != 0)
			// "Search"
		(void) huntForw(pRtnVal, n, NULL);

	return sess.rtn.status;
	}

// Search backward for a previously acquired search pattern, starting at point and proceeding toward the top of the buffer.  If
// found, point is left at the first character of the matched string (the last character that was matched) and the
// matched string is returned; otherwise, false is returned.
int huntBack(Datum *pRtnVal, int n, Datum **args) {

	return hunt(pRtnVal, n, Backward);
	}

// Search backward.  Get a search pattern and, starting at point, search toward the top of the buffer.  If found, point is left
// at the first character of the matched string (the last character that was matched) and the matched string is returned;
// otherwise, false is returned.
int searchBack(Datum *pRtnVal, int n, Datum **args) {

	// Get the pattern and perform the search up to n times.
	if(getPat(args, text81, true) == Success)
			// "Reverse search"
		(void) huntBack(pRtnVal, n, NULL);

	return sess.rtn.status;
	}

// Return the next or previous character in a buffer and, if "update" is true, advance or retreat the scanning point.  The order
// in which this is done is important and depends upon the direction of the search.  Forward searches fetch and advance; reverse
// searches back up and fetch.  If a buffer boundary is hit, -1 is returned.
static short nextChar(Point *pPoint, ushort direct, bool update) {
	short c;

	if(direct == Forward) {						// Going forward?
		if(bufEnd(pPoint))					// Yes.  At bottom of buffer?
			c = -1;						// Yes, return -1;
		else if(pPoint->offset == pPoint->pLine->used) {	// No.  At EOL?
			if(update) {					// Yes, skip to next line...
				pPoint->pLine = pPoint->pLine->next;
				pPoint->offset = 0;
				}
			c = '\n';					// and return a <NL>.
			}
		else {
			c = pPoint->pLine->text[pPoint->offset];	// Otherwise, return current char.
			if(update)
				++pPoint->offset;
			}
		}
	else if(pPoint->offset == 0) {					// Going backward.  At BOL?
		if(pPoint->pLine == sess.cur.pBuf->pFirstLine)		// Yes.  At top of buffer?
			c = -1;						// Yes, return -1.
		else {
			if(update) {					// No.  Skip to previous line...
				pPoint->pLine = pPoint->pLine->prev;
				pPoint->offset = pPoint->pLine->used;
				}
			c = '\n';					// and return a <NL>.
			}
		}
	else {
		int offset = pPoint->offset - 1;
		c = pPoint->pLine->text[offset];			// Not at BOL... return previous char.
		if(update)
			pPoint->offset = offset;
		}

	return c;
	}

// Move a buffer scanning point by jumpSize characters forward or backward.  If "pEnd" not NULL, set it to new position and
// leave "pBegin" unchanged; otherwise, update "pBegin".  Return false if a boundary is hit or *pLineBreakLimit is reached (and
// we may therefore search no further); otherwise, return true.
static bool bufJump(Point *pBegin, int jumpSize, int *pLineBreakLimit, ushort direct, Point *pEnd) {
	Point *pPoint;

	if(pEnd == NULL)
		pPoint = pBegin;
	else {
		pPoint = pEnd;
		*pPoint = *pBegin;
		}

	if(direct == Forward) {							// Going forward.
		if(bufEnd(pPoint))
			return false;						// Hit end of buffer.
		pPoint->offset += jumpSize;
		while(pPoint->offset > pPoint->pLine->used) {			// Skip to next line if overflow.
			if(pLineBreakLimit != NULL && *pLineBreakLimit &&
			 --*pLineBreakLimit == 0)
				return false;					// Hit maximum number of line breaks.
			pPoint->offset -= pPoint->pLine->used + 1;
			if((pPoint->pLine = pPoint->pLine->next) == NULL)
				return false;					// Hit end of buffer.
			}
		}
	else {									// Going backward.
		pPoint->offset -= jumpSize;
		while(pPoint->offset < 0) {					// Skip back a line.
			if(pPoint->pLine == sess.cur.pBuf->pFirstLine ||
			 (pLineBreakLimit != NULL && *pLineBreakLimit &&
			 --*pLineBreakLimit == 0))
				return false;					// Hit beginning of buffer or maximum number
										// of line breaks.
			pPoint->pLine = pPoint->pLine->prev;
			pPoint->offset += pPoint->pLine->used + 1;
			}
		}

	return true;
	}

// Save the text that was matched in given Match record and return status.  Either pMatchLoc or pRegMatch assumed to be non-NULL
// (but not both).  If pMatchLoc not NULL, process plain text result, otherwise (pRegMatch not NULL) RE result.  Called for
// plain text and RE scanning in buffer or string.
int saveMatch(Match *pMatch, MatchLoc *pMatchLoc, RegMatch *pRegMatch) {

#if MMDebug & Debug_SrchRepl
	fprintf(logfile, "saveMatch(): grpMatch.size %hu...\n", pMatch->grpMatch.size);
#endif
	// Free all groups and allocate new ones.
	grpFree(pMatch);

	// If plain text search was performed, need only save group 0 (entire match) using "pMatchLoc" -- there are no other
	// groups.
	if(pMatchLoc != NULL) {
		if(grpInit(pMatch, 1) != Success)
			goto Retn;
		if(pMatch == &bufSearch.match) {

			// Buffer scan -- copy region.
			if(dsalloc(pMatch->grpMatch.groups, pMatchLoc->region.size + 1) != 0)
				goto LibFail;
			regionToStr(pMatch->grpMatch.groups->str, &pMatchLoc->region);
			}
		else {
			// String scan -- copy from StrLoc object.
			if(dsetsubstr((void *) pMatchLoc->strLoc.strPoint, pMatchLoc->strLoc.len, pMatch->grpMatch.groups) != 0)
				goto LibFail;
			}
		}
	else {
		// RE search in a buffer or string.  Save all groups using position offsets in RegMatch object.  If search
		// direction was backward, group 1 goes to end of array, group 2 to end - 1, etc.; however, group 0 always goes
		// to beginning.
		long len;
		Datum *pDatum, *pDatumEnd;
		Region region;
		char *str, *strEnd;
		int i = (pRegMatch->direct == Forward) ? 1 : -1;
		regmatch_t *pGroup = pRegMatch->grpList;
#if MMDebug & Debug_SrchRepl
		int grpNum = 0;
		fputs("### saveMatch(): Beginning RE group save(s)...\n", logfile);
#endif
		if(pRegMatch->startPoint.type == ScanPt_Str)
			strEnd = strchr(pRegMatch->startPoint.u.strPoint, '\0');
		if(grpInit(pMatch, pMatch->grpCount + 1) != Success)
			goto Retn;
		pDatumEnd = (pDatum = pMatch->grpMatch.groups) + pMatch->grpCount;

#if MMDebug & Debug_SrchRepl
		if(pRegMatch->startPoint.type == ScanPt_Str)
			fprintf(logfile, "### saveMatch(): source str '%s' [%.8x], strEnd [%.8x], pat '%s'\n",
			 pRegMatch->startPoint.u.strPoint, (uint) pRegMatch->startPoint.u.strPoint, (uint) strEnd, pMatch->pat);
#endif
		// Save the pattern match (group 0) and all the RE groups, if any.  The offsets will both be -1 if the group
		// never matched.  Set the group to null in that case.
		do {
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "### saveMatch(): saving group %d (rm_so %ld, rm_eo %ld) to pDatum of type %d...\n",
			 grpNum, pGroup->rm_so, pGroup->rm_eo, (int) pDatum->type);
#endif
			if(pGroup->rm_so < 0 || (len = pGroup->rm_eo - pGroup->rm_so) == 0) {
				dsetnull(pDatum);
#if MMDebug & Debug_SrchRepl
				fputs("  set pDatum to null.\n", logfile);
#endif
				}
			else {
#if MMDebug & Debug_SrchRepl
				fprintf(logfile, "### saveMatch(): Getting %ld bytes for group %d...\n", len + 1, grpNum);
#endif
				if(dsalloc(pDatum, len + 1) != 0)
					goto LibFail;
				if(pRegMatch->startPoint.type == ScanPt_Buf) {
					(void) bufJump(&pRegMatch->startPoint.u.bufPoint, pRegMatch->grpList->rm_eo -
					 pGroup->rm_eo + (pRegMatch->direct == Forward ? len : 0), NULL, pRegMatch->direct ^ 1,
					 &region.point);
					region.size = len;
					regionToStr(pDatum->str, &region);
					}
				else {
					str = (pRegMatch->direct == Forward) ? pRegMatch->startPoint.u.strPoint +
					 pGroup->rm_so : strEnd - pGroup->rm_eo;
#if MMDebug & Debug_SrchRepl
					fprintf(logfile,
					 "### saveMatch(): group %d: copying from '%s' [%.8x]: offset %lu, len %ld\n", grpNum,
					 pRegMatch->startPoint.u.strPoint, (uint) pRegMatch->startPoint.u.strPoint,
					 str - pRegMatch->startPoint.u.strPoint, len);
#endif
					stplcpy(pDatum->str, str, len + 1);
					}
				}
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "  set pDatum to str '%s'\n", pDatum->str);
#endif
			if((pGroup += i) < pRegMatch->grpList)
				pGroup = pRegMatch->grpList + pMatch->grpCount;
#if MMDebug & Debug_SrchRepl
			++grpNum;
#endif
			} while(++pDatum <= pDatumEnd);
		}

	// Set new "last match" pointer for $Match system variable.
	if(pMatch == &bufSearch.match || pMatch == &matchRE)
		pLastMatch = pMatch->grpMatch.groups;
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Get next input character -- callback routine for bmuexec().  Set *pChar to the value of the next character in buffer.
// Return true if end of buffer or line break limit reached, otherwise false.
static bool bmGetNext(short *pChar, void *context) {
	BMScan *pScan = (BMScan *) context;
	short c = nextChar(&pScan->point, pScan->direct, true);
#if MMDebug & Debug_SrchRepl && 0
	fprintf(logfile, "bmGetNext(): direct %hu, returned char %c (%.2hx)\n",
	 pScan->direct, isprint(c) ? (int) c : '?', c);
#endif
	*pChar = c;
	if(c == -1 || (c == '\n' && pScan->lineBreakLimit && --pScan->lineBreakLimit == 0))
		return true;

	// If search is taking awhile, let user know.
	if(!pScan->progMsgShown) {
		if(++pScan->scanCount >= CharScanCount) {
			(void) mlputs(MLHome | MLWrap | MLFlush, text203);
					// "Searching..."
			pScan->progMsgShown = true;
			}
		}

	return false;
	}

// Search current buffer for the nth occurrence of text matching a regular expression in either direction.  (It is assumed that
// n > 0.)  If match found, position point at the beginning (if scanning backward) or end (if scanning forward) the matched text
// and set *pMatchLen to text length (if matchLen not NULL).  Uses fast Apostolico-Giancarlo search algorithm from CXL library,
// which is a variant of the Boyer-Moore algorithm.  "n" is repeat count, "pLineBreakLimit" is pointer to number of line breaks
// that forces failure, and "direct" is Forward or Backward.  Return NotFound (bypassing rsset()) if search failure.
int scan(int n, int *pLineBreakLimit, ushort direct, long *pMatchLen) {
#if MMDebug & Debug_SrchRepl
	CharOffset offset;
#endif
	BMScan bmScan;
	Match *pMatch = &bufSearch.match;
	MatchLoc matchLoc;
	BMPat *pPat = (direct == Forward) ? &bufSearch.forwBM : &bufSearch.backBM;
#if MMDebug & Debug_SrchRepl
	char patBuf[pMatch->patLen + OptCh_N + 1];
	makePat(patBuf, &bufSearch.match);
	fprintf(logfile, "### scan(): Scanning for pattern \"%s\" with line break limit %d...\n", patBuf, *pLineBreakLimit);
#endif

	// Set local scan variable to point and initialize scanning parameters.
	bmScan.point = sess.cur.pFace->point;
	bmScan.scanCount = 0;
	bmScan.progMsgShown = false;
	bmScan.lineBreakLimit = *pLineBreakLimit;
	bmScan.direct = direct;
	matchLoc.region.size = pMatch->patLen;
	matchLoc.region.lineCount = 0;					// Not used.

	// Scan the buffer until we find the nth match or hit a buffer boundary.
	for(;;) {
		// Save the current position (starting point) so we can find the exact match point and save group matches later
		// if match is successful.
#if MMDebug & Debug_SrchRepl
		fprintf(logfile, "*** scan(): Calling bmuexec() with pattern \"%s\" at '%.*s'...\n",
		 pMatch->pat, (int)(bmScan.point.pLine->used - bmScan.point.offset),
		 bmScan.point.pLine->text + bmScan.point.offset);
#endif
		// Ready to roll... scan the buffer for a match.
#if MMDebug & Debug_SrchRepl
		offset = bmuexec(pPat, bmGetNext, (void *) &bmScan);
		fprintf(logfile, "  bmuexec() returned offset %ld\n", offset);
		if(offset >= 0) {	/*** } ***/
#else
		if(bmuexec(pPat, bmGetNext, (void *) &bmScan) >= 0) {
#endif
			// A SUCCESSFULL MATCH!  Flag that we have moved, update the point pointers, and save the match.  Need
			// to do this now because the nth match may not be found, in which case we want to leave the point at
			// the last one.
			if(direct == Forward)
				(void) bufJump(&bmScan.point, pMatch->patLen, NULL, Backward, NULL);
			matchLoc.region.point = bmScan.point;
			if(direct == Forward)
				(void) bufJump(&bmScan.point, pMatch->patLen, NULL, Forward, NULL);
			movePoint(&bmScan.point);				// Leave point at end of string.
			if(saveMatch(&bufSearch.match, &matchLoc, NULL) != Success)
				return sess.rtn.status;

			// Return results if nth match found.
			if(--n == 0) {
				if(pMatchLen != NULL)
					*pMatchLen = pMatch->patLen;
				*pLineBreakLimit = bmScan.lineBreakLimit;
#if MMDebug & Debug_SrchRepl
				return rsset(Success, 0, "Match length %d, group 0: '%s'",
				 pMatch->patLen, pMatch->grpMatch.groups[0].str);
#else
				if(bmScan.progMsgShown)
					(void) mlerase(0);
				return sess.rtn.status;
#endif
				}
			}
		else
			// Not found.
			break;

		// Found match, but not nth one.  Continue searching.
		}

	// No match found.
	(void) rsset(Success, RSNoFormat | RSNoWrap, text79);
			// "Not found"
	return NotFound;
	}

// Get next input character -- callback routine for xreg[a]uexec().  Set *pChar to the value of the next character in buffer,
// and set *pAdvBytes to number of bytes advanced.  Return true if end of buffer or line break limit reached, otherwise false.
static bool regGetNext(xint_t *pChar, uint *pAdvBytes, void *context) {
	RegScan *pRegScan = (RegScan *) context;
	short c = nextChar(&pRegScan->point, pRegScan->regMatch.direct, true);
#if MMDebug & Debug_Regexp
	fprintf(logfile, "regGetNext(): direct %hu, returned char %c (%.2hx)\n",
	 pRegScan->regMatch.direct, isprint(c) ? (int) c : '?', c);
#endif
	*pChar = (xint_t) c;
	*pAdvBytes = 1;
	if(c == -1 || (c == '\n' && pRegScan->lineBreakLimit && --pRegScan->lineBreakLimit == 0))
		return true;
	++pRegScan->offset;

	// If search is taking awhile, let user know.
	if(!pRegScan->progMsgShown) {
		if(++pRegScan->scanCount >= CharScanCount) {
			(void) mlputs(MLHome | MLWrap | MLFlush, text203);
					// "Searching..."
			pRegScan->progMsgShown = true;
			}
		}

	return false;
	}

// Rewind input -- callback routine for xreg[a]uexec().  Reset the current position in the buffer or input string.  It is
// assumed that pos is never greater than the current scanning position.
static void regRewind(regoff_t pos, void *context) {
	RegScan *pRegScan = (RegScan *) context;
#if MMDebug & Debug_Regexp
	fprintf(logfile, "regRewind(%ld, ...)\n", pos);
#endif
	(void) bufJump(&pRegScan->point, pRegScan->offset - pos, NULL, pRegScan->regMatch.direct ^ 1, NULL);
	pRegScan->offset = pos;
	}

// Compare strings -- callback routine for xreg[a]uexec().  Compare two substrings in the input and return zero if the
// substrings are equal, or a nonzero value if not.  It is assumed that neither pos1 nor pos2 is ever greater than the current
// scanning position.
static int regCompare(regoff_t pos1, regoff_t pos2, size_t len, void *context) {
	RegScan *pRegScan = (RegScan *) context;
	Point point1;
	Point point2;
#if MMDebug & Debug_Regexp
	fprintf(logfile, "regCompare(%ld, %ld, %lu, ...)\n", pos1, pos2, len);
#endif
	(void) bufJump(&pRegScan->point, pRegScan->offset - pos1, NULL, pRegScan->regMatch.direct ^ 1, &point1);
	(void) bufJump(&pRegScan->point, pRegScan->offset - pos2, NULL, pRegScan->regMatch.direct ^ 1, &point2);

	// Compare bytes in buffer.
	short c1, c2;
	while(len > 0) {
		c1 = nextChar(&point1, pRegScan->regMatch.direct, true);
		c2 = nextChar(&point2, pRegScan->regMatch.direct, true);
		if(c1 != c2)
			return 1;
		--len;
		}
	return 0;
	}

// Return XRE execution flags for buffer search or string comparison, given (1), scanning direction; (2), "starting at beginning
// of line" flag (if scanning forward) or "starting at end of line" flag (if scanning backward); and (3), character preceding
// starting point (relative to scanning direction).  XRE flags:
//	REG_NOTBOL	Assertion ^ does not match beginning of string.
//	REG_NOTEOL	Assertion $ does not match end of string.
//	REG_WORDCHBOS	Assume a word character exists immediately before beginning of string (which is located at end of scan
//			if REG_REVERSED flag is set).
//	REG_WORDCHEOS	Assume a word character exists immediately after end of string (which is located at start of scan if
//			REG_REVERSED flag is set).
// "Beginning of string" is left end of string (which is most distant point if scanning backward) and "end of string" is right
// end of string (which is starting point if scanning backward).
static int getExecFlags(ushort direct, bool atBeginEnd, short c) {
	int execFlags, lineFlag, wordFlag;

	if(direct == Forward) {
		lineFlag = REG_NOTBOL;
		wordFlag = REG_WORDCHBOS;
		}
	else {
		lineFlag = REG_NOTEOL;
		wordFlag = REG_WORDCHEOS;
		}
	execFlags = 0;
	if(!atBeginEnd) {
		execFlags |= lineFlag;
		if(wordChar[c])
			execFlags |= wordFlag;
		}
#if MMDebug & Debug_Regexp
	fprintf(logfile, "getExecFlags(%hu,%d,%hd): returning %.8x.\n", direct, atBeginEnd, c, execFlags);
#endif
	return execFlags;
	}

// Search current buffer for the nth occurrence of text matching a regular expression in either direction.  (It is assumed that
// n > 0.)  If match found, position point at the beginning (if scanning backward) or end (if scanning forward) the matched text
// and set *pMatchLen to text length (if matchLen not NULL).  "n" is repeat count, "pLineBreakLimit" is pointer to number of
// line breaks that forces failure, and "direct" is Forward or Backward.  Return NotFound (bypassing rsset()) if search failure.
int regScan(int n, int *pLineBreakLimit, ushort direct, long *pMatchLen) {
	int r;
	short c;
	RegScan regScan;
	Match *pMatch = &bufSearch.match;
	regex_t *pRegex = (direct == Forward) ? &pMatch->regPat.compPat : &pMatch->regPat.compBackPat;
	regmatch_t groups[pMatch->grpCount + 1];
#if FuzzySearch
	regamatch_t approxMatch = {pMatch->grpCount + 1, groups};
	regaparams_t params;
#endif
	regusource_t strSource = {
		regGetNext, regRewind, regCompare, (void *) &regScan};
#if MMDebug & Debug_SrchRepl
	char patBuf[pMatch->patLen + OptCh_N + 1];
	makePat(patBuf, &bufSearch.match);
	fprintf(logfile, "### regScan(): Scanning for /%s/ with line break limit %d...\n", patBuf, *pLineBreakLimit);
#endif

	// Set local scan variable to point and initialize scanning parameters.
#if FuzzySearch
	if(pMatch->flags & SOpt_Fuzzy)
		xregainit(&params, 0);
#endif
	regScan.point = sess.cur.pFace->point;
	regScan.offset = regScan.scanCount = 0;
	regScan.progMsgShown = false;
	regScan.lineBreakLimit = *pLineBreakLimit;
	regScan.matchFlags = pMatch->flags;
	regScan.regMatch.grpList = groups;
	regScan.regMatch.direct = direct;
	regScan.regMatch.startPoint.type = ScanPt_Buf;

	// Scan the buffer until we find the nth match or hit a buffer boundary.
	for(;;) {
		// Save the current position (starting point) so we can find the exact match point and save group matches later
		// if match is successful.
		regScan.regMatch.startPoint.u.bufPoint = regScan.point;
#if MMDebug & Debug_SrchRepl
		fprintf(logfile, "*** regScan(): Calling xreg%suexec() %sward with /%s/ at '%.*s'...\n",
#if FuzzySearch
		 pMatch->flags & SOpt_Fuzzy ? "a" : "",
#else
		 "",
#endif
		 direct == Forward ? "for" : "back", pMatch->pat, (int)(regScan.point.pLine->used - regScan.point.offset),
		 regScan.point.pLine->text + regScan.point.offset);
#endif
		// Ready to roll... scan the buffer for a match.
		if((c = nextChar(&regScan.point, direct ^ 1, false)) < 0)
			c = '\0';
		r = getExecFlags(direct, direct == Forward ? regScan.point.offset == 0 :
		 regScan.point.offset == regScan.point.pLine->used, c);
#if FuzzySearch
#if MMDebug & Debug_SrchRepl
		r = (pMatch->flags & SOpt_Fuzzy) ? xregauexec(pRegex, &strSource, &approxMatch, &params, r) :
		 xreguexec(pRegex, &strSource, pMatch->grpCount + 1, groups, r);
		fprintf(logfile, "  xreg%suexec() returned status %d\n", (pMatch->flags & SOpt_Fuzzy) ? "a" : "", r);
		if(r == 0) {
#else
		if((r = (pMatch->flags & SOpt_Fuzzy) ? xregauexec(pRegex, &strSource, &approxMatch, &params, r) :
		 xreguexec(pRegex, &strSource, pMatch->grpCount + 1, groups, r)) == 0) {
#endif
#else
#if MMDebug & Debug_SrchRepl
		r = xreguexec(pRegex, &strSource, pMatch->grpCount + 1, groups, r);
		fprintf(logfile, "  xreguexec() returned status %d\n", r);
		if(r == 0) {
#else
		if((r = xreguexec(pRegex, &strSource, pMatch->grpCount + 1, groups, r)) == 0) {
#endif
#endif
			// A SUCCESSFULL MATCH!  Flag that we have moved, update the point pointers, and save the match.
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "  -> rm_so %ld, rm_eo %ld, regScan.offset %ld\n",
			 groups[0].rm_so, groups[0].rm_eo, regScan.offset);
#endif
			(void) bufJump(&regScan.point, regScan.offset - groups[0].rm_eo, NULL, direct ^ 1, NULL);
			movePoint(&regScan.point);				// Leave point at end of string.
			regScan.regMatch.startPoint.u.bufPoint = regScan.point;
			if(saveMatch(&bufSearch.match, NULL, &regScan.regMatch) != Success)
				return sess.rtn.status;

			// Return results if nth match found.
			if(--n == 0) {
				if(pMatchLen != NULL)
					*pMatchLen = groups[0].rm_eo - groups[0].rm_so;
				*pLineBreakLimit = regScan.lineBreakLimit;
#if MMDebug & Debug_SrchRepl
				return rsset(Success, 0, "Match length %d, group 0: '%s'",
				 groups[0].rm_eo - groups[0].rm_so, pMatch->grpMatch.groups[0].str);
#else
				if(regScan.progMsgShown)
					(void) mlerase(0);
				return sess.rtn.status;
#endif
				}
			}
		else if(r == REG_NOMATCH)

			// Not found.
			break;
		else
			// xreg[a]uexec() internal error.
			return regError(pMatch->pat, r);

		// Found match, but not nth one.  Continue searching.
		regScan.offset = 0;
		}

	// No match found.
	(void) rsset(Success, RSNoFormat | RSNoWrap, text79);
			// "Not found"
	return NotFound;
	}

// Compare given string in *pSrc with the (non-null) RE pattern in *pMatch.  If scanOffset < 0, begin comparison at end of
// string and scan backward; otherwise, begin at scanOffset and scan forward.  If a match is found, set *result to regmatch_t
// object (for group 0) and save groups in Match object; otherwise, set rm_so in *result to -1.  Return status.
int regcmp(Datum *pSrc, int scanOffset, Match *pMatch, regmatch_t *result) {
	int r;
	short c;
	RegMatch regMatch;
	regex_t *pRegex = (scanOffset >= 0) ? &pMatch->regPat.compPat : &pMatch->regPat.compBackPat;
	regmatch_t groups[pMatch->grpCount + 1];
#if FuzzySearch
	regamatch_t approxMatch = {pMatch->grpCount + 1, groups};
	regaparams_t params;
#endif
	size_t srcLen = strlen(pSrc->str);
	char *str;
	char workBuf[srcLen + 1];		// For reversed string, if needed.

	// Get ready.  If scanning backward, create local copy of reversed string and use backward RE pattern.
#if FuzzySearch
	if(pMatch->flags & SOpt_Fuzzy)
		xregainit(&params, 0);
#endif
	regMatch.grpList = groups;
	regMatch.startPoint.type = ScanPt_Str;
	regMatch.startPoint.u.strPoint = str = pSrc->str + (scanOffset >= 0 ? scanOffset : 0);
	if(scanOffset >= 0) {
		regMatch.direct = Forward;
		c = (str > pSrc->str) ? str[-1] : '\0';
		}
	else {
		str = strrev(strcpy(workBuf, pSrc->str));
		regMatch.direct = Backward;
		c = '\0';
		}

	// Ready to roll... scan the string for a match.
	//	Direc	scanOffset	NOTBOL	NOTEOL
	//	Fwd	0	-	-
	//		> 0	X	-
	//	Bwd	strend	X	X
	r = getExecFlags(regMatch.direct, regMatch.direct == Forward ? scanOffset == 0 || str[-1] == '\n' : true, c);
#if FuzzySearch
	if((r = (pMatch->flags & SOpt_Fuzzy) ? xregaexec(pRegex, str, &approxMatch, &params, r) :
	 xregexec(pRegex, str, pMatch->grpCount + 1, groups, r)) == 0) {
#else
	if((r = xregexec(pRegex, str, pMatch->grpCount + 1, groups, r)) == 0) {
#endif

		// A SUCCESSFUL MATCH!  Save groups and returns offsets.  If scan was backward, adjust group 0 offsets first.
		(void) saveMatch(pMatch, NULL, &regMatch);
		if(regMatch.direct == Backward) {
			size_t len = srcLen - groups[0].rm_eo;
			groups[0].rm_eo = srcLen - groups[0].rm_so;
			groups[0].rm_so = len;
			}
		*result = groups[0];
		}
	else if(r == REG_NOMATCH)

		// Not found.
		result->rm_so = -1;
	else
		// xreg[a]exec() internal error.
		(void) regError(pMatch->pat, r);

	return sess.rtn.status;
	}
