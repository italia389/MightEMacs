// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// search.c	Search functions for MightEMacs.
//
// These functions implement commands that search in the forward and backward directions.

#include "std.h"
#include "exec.h"
#include "var.h"

// Make selected global definitions local.
#define SearchData
#include "search.h"

#define max(a, b)	((a < b) ? b : a)

// Control object for RE compilation and execution.
typedef struct {
	Point point;			// Current line and offset in buffer during scan.
	size_t offset;			// Current offset of scan point (>= 0) from starting position.
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
		{OptCh_Fuzzy, SOpt_Fuzzy, SOpt_Fuzzy | SOpt_Plain},
		{OptCh_Multi, SOpt_Multi, SOpt_Multi | SOpt_Plain},
		{OptCh_Plain, SOpt_Plain, SOpt_Plain | SOpt_Fuzzy | SOpt_Multi | SOpt_Regexp},
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

	// No duplicate or conflicting flags found.  Enable Regexp if 'm' or 'f' specified.
	if(searchFlags & (SOpt_Fuzzy | SOpt_Multi))
		searchFlags |= SOpt_Regexp;

	// Valid option string.  Truncate pattern, update flags, and return new length.
	*pat = '\0';
	*flags |= searchFlags;
	return patLen - (options - pat);
	}

// Return true if given Match record specifies Exact search mode.
bool exactSearch(Match *pMatch) {

	return (pMatch->flags & SOpt_Exact) ||
	 (!(pMatch->flags & SOpt_Ignore) && (pMatch != &searchCtrl.match || modeSet(MdIdxExact, NULL)));
	}

// Return true if a Regexp buffer search is indicated by flags in Match record or Regexp global mode.
bool bufRESearch(void) {

	return (searchCtrl.match.flags & SOpt_Regexp) || (modeSet(MdIdxRegexp, NULL) && !(searchCtrl.match.flags & SOpt_Plain));
	}

// Return true if a plain text buffer search is indicated by flags in Match record or Regexp global mode, and presence of
// metacharacter(s) in search pattern if an RE.  It is assumed that any RE search pattern has already been compiled so that the
// SRegical flag is valid and can be checked.
bool bufPlainSearch(void) {

	return !bufRESearch() || !(searchCtrl.match.flags & SRegical);
	}

// Delta1 table: delta1[c] contains the distance between the last character of pat and the rightmost occurrence of c in pat.  If
// c does not occur in pat, then delta1[c] = patLen.  If c is at string[i] and c != pat[patLen - 1], we can safely shift i over
// by delta1[c], which is the minimum distance needed to shift pat forward to get string[i] lined up with some character in pat.
// This algorithm runs in alphabet_len + patLen time.
static void makeDelta1(int *delta1, const char *pat, bool exact) {
	int i;
	int patLen1 = searchCtrl.match.patLen - 1;

	// First, set all characters to the maximum jump.
	for(i = 0; i < 256; ++i)
		delta1[i] = searchCtrl.match.patLen;

	// Now, set the characters in the pattern.  Set both cases of letters if "exact" mode is false.
	for(i = 0; i < patLen1; ++i) {
		delta1[(int) pat[i]] = patLen1 - i;
		if(!exact)
			delta1[chgCase(pat[i])] = patLen1 - i;
		}
	}
 
// Return true if the suffix of word starting from word[pos] is a prefix of word.
static bool isPrefix(const char *word, int wordLen, int pos) {
	int i;
	int sufLen = wordLen - pos;

	// Could also use the strncmp() library function here.
	for(i = 0; i < sufLen; ++i) {
		if(word[i] != word[pos + i])
			return false;
		}
	return true;
	}
 
// Return length of the longest suffix of word ending on word[pos]; for example, suffixLen("dddbcabc", 8, 4) = 2.
static int suffixLen(const char *word, int wordLen, int pos) {
	int i;

	// Increment suffix length i to the first mismatch or beginning of the word.
	for(i = 0; (word[pos - i] == word[wordLen - 1 - i]) && (i < pos); ++i);
	return i;
	}
 
// Delta2 table: given a mismatch at pat[pos], we want to align with the next possible full match, based on what we know about
// pat[pos + 1] to pat[patLen - 1].
//
// In case 1:
// pat[pos + 1] to pat[patLen - 1] does not occur elsewhere in pat and the next plausible match starts at or after the mismatch.
// If, within the subString pat[pos + 1 .. patLen - 1], lies a prefix of pat, the next plausible match is here (if there are
// multiple prefixes in the subString, pick the longest).  Otherwise, the next plausible match starts past the character aligned
// with pat[patLen - 1].
//
// In case 2:
// pat[pos + 1] to pat[patLen - 1] does occur elsewhere in pat.  The mismatch tells us that we are not looking at the end of a
// match.  We may, however, be looking at the middle of a match.
//
// The first loop, which takes care of case 1, is analogous to the KMP table, adapted for a 'backwards' scan order with the
// additional restriction that the subStrings it considers as potential prefixes are all suffixes. In the worst case scenario,
// pat consists of the same letter repeated, so every suffix is a prefix.  This loop alone is not sufficient, however.  Suppose
// that pat is "ABYXCDEYX", and text is ".....ABYXCDEYX".  We will match X, Y, and find B != E.  There is no prefix of pat in
// the suffix "YX", so the first loop tells us to skip forward by 9 characters.  Although superficially similar to the KMP
// table, the KMP table relies on information about the beginning of the partial match that the BM algorithm does not have.
//
// The second loop addresses case 2.  Since suffixLen may not be unique, we want to take the minimum value, which will tell
// us how far away the closest potential match is.
static void makeDelta2(int *delta2, char *pat, bool exact) {
 
	// If "Exact" mode is false, set values in this table to the minimum jump for each pattern index which will advance the
	// search by one position -- it can't be used in the usual way.
	if(!exact) {
		int i = searchCtrl.match.patLen;

		// Set the first "patLen" entries to zero.
		do {
			*delta2++ = i--;
			} while(i > 0);
		}
	else {
		int i;
		int patLen1 = searchCtrl.match.patLen - 1;
		int lastPrefixIndex = patLen1;

		// First loop.
		i = searchCtrl.match.patLen;
		do {
			if(isPrefix(pat, searchCtrl.match.patLen, i))
				lastPrefixIndex = i;
			--i;
			delta2[i] = lastPrefixIndex + (patLen1 - i);
			} while(i > 0);

		// Second loop.
		for(i = 0; i < patLen1; ++i) {
			int sufLen = suffixLen(pat, searchCtrl.match.patLen, i);
			if(pat[i - sufLen] != pat[patLen1 - sufLen])
				delta2[patLen1 - sufLen] = patLen1 - i + sufLen;
			}
		}
	}

// Make delta tables for plain text buffer search.  Return status.
void makeDeltas(void) {
	bool exact;

	// Remember type of compile.
	if((exact = exactSearch(&searchCtrl.match)))
		searchCtrl.match.flags |= SCpl_PlainExact;
	else
		searchCtrl.match.flags &= ~SCpl_PlainExact;

	// Create delta tables.
	makeDelta1(searchCtrl.forwDelta1, searchCtrl.match.pat, exact);
	makeDelta2(searchCtrl.forwDelta2, searchCtrl.match.pat, exact);
	makeDelta1(searchCtrl.backDelta1, searchCtrl.backPat, exact);
	makeDelta2(searchCtrl.backDelta2, searchCtrl.backPat, exact);
	}

// Free all heap space for RE pattern in given Match object.
void freeRE(Match *pMatch) {

	if(pMatch->flags & SCpl_ForwardRE) {
		xregfree(&pMatch->regPat.compPat);
		free((void *) pMatch->regPat.backPat);
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
// REG_BADPAT,             // Invalid regular expression.			" '%s'"
// REG_ECOLLATE,           // Unknown collating element.			" in RE pattern '%s'"
// REG_ECTYPE,             // Unknown character class name.
// REG_EESCAPE,            // Trailing backslash invalid.
// REG_ESUBREG,            // Invalid back reference.
// REG_EBRACK,             // Brackets '[ ]' not balanced
// REG_EPAREN,             // Parentheses '( )' not balanced
// REG_EBRACE,             // Braces '{ }' not balanced
// REG_BADBR,              // Invalid repetition count(s) in '{ }'.
// REG_ERANGE,             // Invalid character range in '[ ]'.
// REG_ESPACE,             // Out of memory.					", matching RE pattern '%s'"
// REG_BADRPT,             // Invalid use of repetition operator.
// REG_EMPTY,              // Empty (sub)expression.
// REG_EREGREV             // Unsupported element(s) in reversed pattern.	" '%s'"

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

// Compile the RE string in the given Match record forward and/or backward per "flags", save in the "regPat" record, and return
// status.  The RE string is assumed to be non-empty, and memory used for any previous compile is assumed to have already been
// freed.  If SCpl_ForwardRE is set in flags, the forward pattern is compiled and the backward pattern is created.  If
// SCpl_BackwardRE is set in flags, the backward pattern is compiled (and is assumed to already exist).  In either case, if the
// buffer Match object is being processed and the pattern contains no metacharacters, compilation is skipped.
int compileRE(Match *pMatch, ushort flags) {

	// If compiling primary (forward) pattern, check if new pattern contains any metacharacters.
	if(flags & SCpl_ForwardRE)
		metaCheck(pMatch);

	// Non-buffer compile, metacharacter(s) in new pattern, or RE search?
	if(pMatch != &searchCtrl.match || (pMatch->flags & SRegical)) {
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
				if((pMatch->regPat.backPat = (char *) malloc(pMatch->patLen + 1)) == NULL)
					return rsset(Panic, 0, text94, "compileRE");
							// "%s(): Out of memory!"
				(void) xregrev(pMatch->regPat.backPat, pMatch->pat, REG_ENHANCED);
#if MMDebug & Debug_Regexp
				fprintf(logfile, "  reversed pattern is '%s'.\n", pMatch->regPat.backPat);
#endif
				}
			else
				return regError(pMatch->pat, r);
			}
		if(flags & SCpl_BackwardRE) {
			if((r = xregcomp(&pMatch->regPat.compBackPat, pMatch->regPat.backPat, compFlags | REG_REVERSED)) == 0)
				pMatch->flags |= SCpl_BackwardRE;
			else
				return regError(pMatch->regPat.backPat, r);
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
	pMatch->searchTableSize = pMatch->replTableSize = 0;
	pMatch->compReplPat = NULL;
	pMatch->grpMatch.groups = NULL;
	}

// Free all search pattern heap space in given Match object.
void freeSearchPat(Match *pMatch) {

	freeRE(pMatch);
	if(pMatch->searchTableSize > 0) {
		free((void *) pMatch->pat);
		pMatch->searchTableSize = 0;
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
		{OptCh_Fuzzy, SOpt_Fuzzy},
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

	// Free up arrays if too big or too small.
	if(pMatch->searchTableSize > PatSizeMax ||
	 (pMatch->searchTableSize > 0 && (uint) pMatch->patLen > pMatch->searchTableSize)) {
		free((void *) pMatch->pat);
		pMatch->searchTableSize = 0;
		if(pMatch == &searchCtrl.match) {
			free((void *) searchCtrl.backPat);
			free((void *) searchCtrl.forwDelta2);
			free((void *) searchCtrl.backDelta2);
			}
		}

	// Get heap space for arrays if needed.
	if(pMatch->searchTableSize == 0) {
		pMatch->searchTableSize = pMatch->patLen < PatSizeMin ? PatSizeMin : pMatch->patLen;
		if((pMatch->pat = (char *) malloc(pMatch->searchTableSize + 1)) == NULL ||
		 (pMatch == &searchCtrl.match &&
		 ((searchCtrl.backPat = (char *) malloc(pMatch->searchTableSize + 1)) == NULL ||
		 (searchCtrl.forwDelta2 = (int *) malloc(pMatch->searchTableSize * sizeof(int))) == NULL ||
		 (searchCtrl.backDelta2 = (int *) malloc(pMatch->searchTableSize * sizeof(int))) == NULL)))
			return rsset(Panic, 0, text94, "newSearchPat");
					// "%s(): Out of memory!"
		}

	// Save search pattern.
	strcpy(pMatch->pat, pat);
	if(pMatch == &searchCtrl.match) {
		strrev(strcpy(searchCtrl.backPat, pat));
		searchCtrl.forwDelta1[0] = -1;						// Clear delta tables.
		if(addToRing) {
			char patBuf[pMatch->patLen + OptCh_N + 1];
			(void) rsave(ringTable + RingIdxSearch, makePat(patBuf, pMatch), false); // Add pattern to search ring.
			}
		}

	return sess.rtn.status;
	}

// Get a search or replacement pattern and stash it in global variable searchCtrl.pat (searchPat is true) or searchCtrl.replPat
// (searchPat is false) via newSearchPat() or newReplPat().  If script mode, *args contains the pattern; otherwise, get it
// interactively.  If the pattern is null, searchPat is true, and there is no old pattern, it is an error.  Return status.
int getPat(Datum **args, const char *prompt, bool searchPat) {
	Datum *pPat;

	if(dnewtrack(&pPat) != 0)
		return librsset(Failure);

	// Read a pattern.  Either we get one, or we just get the input delimiter and use the previous pattern.
	if(sess.opFlags & OpScript)
		dxfer(pPat, args[0]);
	else {
		TermInpCtrl termInpCtrl = {NULL, searchCtrl.inpDelim, 0,
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
		if(flags != (searchCtrl.match.flags & SOpt_All) || strcmp(pPat->str, searchCtrl.match.pat) != 0) {

			// Save the new search pattern.
			(void) newSearchPat(pPat->str, &searchCtrl.match, &flags, true);
			}
		}
	else if(strcmp(pPat->str, searchCtrl.match.replPat) != 0) {

		// Save the new replacement pattern.
		(void) newReplPat(pPat->str, &searchCtrl.match, true);
		}

	return sess.rtn.status;
	}

// Create buffer search tables if needed.
static int makeSearchTables(void) {

	// Clear search groups.
	grpFree(&searchCtrl.match);

	// Compile pattern as an RE if requested.
	if(bufRESearch() && !(searchCtrl.match.flags & SCpl_ForwardRE) &&
	 compileRE(&searchCtrl.match, SCpl_ForwardRE) != Success)
		return sess.rtn.status;

	// Compile as a plain-text pattern if not an RE request or not really an RE (SRegical not set).
	if(bufPlainSearch() && (searchCtrl.forwDelta1[0] == -1 ||
	 (((searchCtrl.match.flags & SCpl_PlainExact) != 0) != exactSearch(&searchCtrl.match))))
		makeDeltas();

	return sess.rtn.status;
	}

// Search forward or backward for a previously acquired search pattern.  If found, the matched string is returned; otherwise,
// false is returned.
static int hunt(Datum *pRtnVal, int n, ushort direct, const char *pat) {

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

		// Make sure a pattern exists, or that we didn't switch into Regexp mode after the pattern was entered.
		if(pat[0] == '\0')
			return rsset(Failure, RSNoFormat, text80);
				// "No pattern set"

		// Create search tables if needed.
		if(makeSearchTables() == Success) {
			bool bufRegical = !bufPlainSearch();

			// If doing an RE scan backward, compile backward pattern if needed.
			if(bufRegical && direct == Backward && !(searchCtrl.match.flags & SCpl_BackwardRE) &&
			 compileRE(&searchCtrl.match, SCpl_BackwardRE) != Success)
				return sess.rtn.status;

			// Perform appropriate search and return result.
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "hunt(): calling %s() with n %d, line break limit %d, pat '%s'...\n",
			 bufRegical ? "regScan" : "scan", n, lineBreakLimit, pat);
#endif
			if((bufRegical ? regScan(n, &lineBreakLimit, direct, NULL) :
			  scan(n, &lineBreakLimit, direct, NULL)) == NotFound)
				dsetbool(false, pRtnVal);
			else if(dcpy(pRtnVal, searchCtrl.match.grpMatch.groups) != 0)
				(void) librsset(Failure);
			}
		}

	return sess.rtn.status;
	}

// Search forward for a previously acquired search pattern, starting at point and proceeding toward the bottom of the buffer.
// If found, point is left at the character immediately following the matched string and the matched string is returned;
// otherwise, false is returned.
int huntForw(Datum *pRtnVal, int n, Datum **args) {

	return hunt(pRtnVal, n, Forward, searchCtrl.match.pat);
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

	return hunt(pRtnVal, n, Backward, searchCtrl.backPat);
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

// Compare two characters ("bufChar" from the buffer, and "patChar" from the pattern) and return Boolean result.  If "exact" is
// false, fold the case.
static bool eq(ushort bufChar, ushort patChar, bool exact) {

	if(!exact) {
		bufChar = lowCase[bufChar];
		patChar = lowCase[patChar];
		}

	return bufChar == patChar;
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

// Save the text that was matched in given Match record and return status.  If pMatchLoc not NULL, process plain text result,
// otherwise RE result (using pRegMatch).  Called for plain text and RE scanning in buffer or string.
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
		if(pMatch == &searchCtrl.match) {
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
			fprintf(logfile, "### saveMatch(): saving group %d (rm_so %d, rm_eo %d) to pDatum of type %d...\n",
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
					(void) bufJump(&pRegMatch->startPoint.u.bufPoint, pRegMatch->direct == Forward ?
					 pGroup->rm_so : pGroup->rm_eo, NULL, pRegMatch->direct, &region.point);
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

	// Set new "last match" pointer.
	if(pMatch == &searchCtrl.match || pMatch == &matchRE)
		pLastMatch = pMatch->grpMatch.groups;
Retn:
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

#define SHOW_COMPARES	0

// Search current buffer for nth occurrence of text matching a fixed pattern in either direction.  (It is assumed that n > 0.)
// If match is found, position point at beginning (if scanning backward) or end (if scanning forward) of the matched text and
// set *pMatchLen to match length, if pMatchLen not NULL.  This is a fast algorithm using code from Boyer and Moore, Software
// Practice and Experience, vol. 10 (1980).  "n" is repeat count, "pLineBreakLimit" is pointer to number of line breaks that
// forces failure, and "direct" is Forward or Backward.  Return NotFound (bypassing rsset()) if search failure.
int scan(int n, int *pLineBreakLimit, ushort direct, long *pMatchLen) {
	Match *pMatch = &searchCtrl.match;
	MatchLoc matchLoc;
	short bufChar, patChar;		// Buffer char and pattern char.
	int patIndex;			// Pattern index.
	char *pat, *patEnd;		// Pattern pointers.
	char *pattern;			// String to scan for.
	Point point;			// Current line and offset during scanning.
	ushort scanDirect;		// Scan direction.
	int *delta1, *delta2;
	int jumpSize;			// Next offset.
	uint scanCount = 0;		// Number of characters scanned (for progress reporting).
	uint scanCountMax = CharScanCount * 6;
	bool progMsgShown = false;	// Progress message displayed?
	bool exact = pMatch->flags & SCpl_PlainExact;
#if SHOW_COMPARES
	int compares = 0;
#endif
	// Set scanning direction for pattern matching, which is always in the opposite direction of the search.
	scanDirect = direct ^ 1;

	// Another directional problem: if we are searching forward, use the backward (reversed) pattern in backPat[], and the
	// forward delta tables; otherwise, use the forward pattern in pat[], and the backward delta tables.  This is so the
	// pattern can always be scanned from left to right (until patLen characters have been examined) during comparisions.
	if(direct == Forward) {
		pattern = searchCtrl.backPat;
		delta1 = searchCtrl.forwDelta1;
		delta2 = searchCtrl.forwDelta2;
		}
	else {
		pattern = pMatch->pat;
		delta1 = searchCtrl.backDelta1;
		delta2 = searchCtrl.backDelta2;
		}

	// Set local scan variable to point.
	point = sess.cur.pFace->point;

	// Scan the buffer until we find the nth match or hit a buffer boundary.  The basic loop is to jump forward or backward
	// in the buffer (so that comparisons are done in reverse), check for a match, get the next jump size (delta) on a
	// failure, and repeat.  The initial jump is the full pattern length so that the first character returned by nextChar()
	// is at the far end of a possible match, depending on the search direction.
	matchLoc.region.size = jumpSize = pMatch->patLen;
	matchLoc.region.lineCount = 0;					// Not used.
	patEnd = pattern + pMatch->patLen;
	while(bufJump(&point, jumpSize, pLineBreakLimit, direct, NULL)) {
		if(!progMsgShown)
			scanCount += jumpSize;

		// Save the current position in case we match the search string at this point.
		matchLoc.region.point = point;

		// Scan through the pattern for a match.  Note that nextChar() will never hit a buffer boundary (and return -1)
		// because bufJump() checks for that.
		pat = pattern;
		patIndex = pMatch->patLen;
#if SHOW_COMPARES
		++compares;
#endif
		do {
			patChar = *pat;
			--patIndex;
			bufChar = nextChar(&point, scanDirect, true);

			// Adjust line break limit if passing over a newline during comparison.
			if(bufChar == '\n' && *pLineBreakLimit)
				++*pLineBreakLimit;

			if(!eq(patChar, bufChar, exact)) {

				// No match.  Jump forward or backward in buffer as far as possible and try again.
				// Use delta + 1 so that nextChar() begins at the right spot.
				jumpSize = max(delta1[bufChar], delta2[patIndex]) + 1;
				goto Fail;
				}
			} while(++pat < patEnd);

		// A SUCCESSFULL MATCH!  Flag that we have moved, update the point pointers, and save the match.  Need to do
		// this now because the nth match may not be found, in which case we want to leave the point at the last one.
		movePoint(&matchLoc.region.point);			// Leave point at end of string.

		// If we were heading forward, set the match pointer to the beginning of the string.
		if(direct == Forward)
			matchLoc.region.point = point;
		if(saveMatch(pMatch, &matchLoc, NULL) != Success)
			return sess.rtn.status;

		// Return if nth match found.
		if(--n == 0) {
			if(pMatchLen != NULL)
				*pMatchLen = pMatch->patLen;
#if SHOW_COMPARES
			return rsset(Success, 0, "%d comparisons", compares);
#else
			if(progMsgShown)
				(void) mlerase(0);
			return sess.rtn.status;
#endif
			}

		// Nth match not found.  Reset and continue scan.
		jumpSize = pMatch->patLen * 2;
Fail:
		// If search is taking awhile, let user know.
		if(!progMsgShown) {
			if(scanCount >= scanCountMax) {
				if(mlputs(MLHome | MLWrap | MLFlush, text203) != Success)
						// "Searching..."
					return sess.rtn.status;
				progMsgShown = true;
				}
			}
		}

	// No match found.
#if SHOW_COMPARES
	(void) rsset(Success, RSNoWrap, "Not found (%d comparisons)", compares);
#else
	(void) rsset(Success, RSNoFormat | RSNoWrap, text79);
			// "Not found"
#endif
	return NotFound;
	}

// Get next input character -- callback routine for xreg[a]uexec().  Set *pChar to the value of the next character in buffer,
// and set *pAdvBytes to number of bytes advanced.  Return true if end of buffer or line break limit reached, otherwise false.
static bool cbGetNext(xcint_t *pChar, int *pAdvBytes, void *context) {
	RegScan *pRegScan = (RegScan *) context;
	short c = nextChar(&pRegScan->point, pRegScan->regMatch.direct, true);
#if MMDebug & Debug_Regexp
	fprintf(logfile, "cbGetNext(): direct %hu, returned char %c (%.2hx)\n",
	 pRegScan->regMatch.direct, isprint(c) ? c : '?', c);
#endif
	*pChar = (xcint_t) c;
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
static void cbRewind(size_t pos, void *context) {
	RegScan *pRegScan = (RegScan *) context;
#if MMDebug & Debug_Regexp
	fprintf(logfile, "cbRewind(%lu, ...)\n", pos);
#endif
	(void) bufJump(&pRegScan->point, pRegScan->offset - pos, NULL, pRegScan->regMatch.direct ^ 1, NULL);
	pRegScan->offset = pos;
	}

// Compare strings -- callback routine for xreg[a]uexec().  Compare two substrings in the input and return zero if the
// substrings are equal, or a nonzero value if not.  It is assumed that neither pos1 nor pos2 is ever greater than the current
// scanning position.
static int cbCompare(size_t pos1, size_t pos2, size_t len, void *context) {
	RegScan *pRegScan = (RegScan *) context;
	Point point1;
	Point point2;
#if MMDebug & Debug_Regexp
	fprintf(logfile, "cbCompare(%lu, %lu, %lu, ...)\n", pos1, pos2, len);
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
	Match *pMatch = &searchCtrl.match;
	regex_t *pRegex = (direct == Forward) ? &pMatch->regPat.compPat : &pMatch->regPat.compBackPat;
	regmatch_t groups[pMatch->grpCount + 1];
	regamatch_t approxMatch = {pMatch->grpCount + 1, groups};
	regaparams_t params;
	regusource_t strSource = {
		cbGetNext, cbRewind, cbCompare, (void *) &regScan};
#if MMDebug & Debug_SrchRepl
	char patBuf[pMatch->patLen + OptCh_N + 1];
	makePat(patBuf, &searchCtrl.match);
	fprintf(logfile, "### regScan(): Scanning for /%s/ with line break limit %d...\n", patBuf, *pLineBreakLimit);
#endif

	// Set local scan variable to point and initialize scanning parameters.
	if(pMatch->flags & SOpt_Fuzzy)
		xregainit(&params, 0);
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
		fprintf(logfile, "*** regScan(): Calling xreg%suexec() with /%s/ at '%.*s'...\n",
		 (pMatch->flags & SOpt_Fuzzy) ? "a" : "", pMatch->pat, (int)(regScan.point.pLine->used - regScan.point.offset),
		 regScan.point.pLine->text + regScan.point.offset);
#endif
		// Ready to roll... scan the buffer for a match.
		if((c = nextChar(&regScan.point, direct ^ 1, false)) < 0)
			c = '\0';
		r = getExecFlags(direct, direct == Forward ? regScan.point.offset == 0 :
		 regScan.point.offset == regScan.point.pLine->used, c);
#if MMDebug & Debug_SrchRepl
		r = (pMatch->flags & SOpt_Fuzzy) ? xregauexec(pRegex, &strSource, &approxMatch, &params, r) :
		 xreguexec(pRegex, &strSource, pMatch->grpCount + 1, groups, r);
		fprintf(logfile, "  xreg%suexec() returned status %d\n", (pMatch->flags & SOpt_Fuzzy) ? "a" : "", r);
		if(r == 0) {	/*** } ***/
#else
		if((r = (pMatch->flags & SOpt_Fuzzy) ? xregauexec(pRegex, &strSource, &approxMatch, &params, r) :
		 xreguexec(pRegex, &strSource, pMatch->grpCount + 1, groups, r)) == 0) {
#endif
			// A SUCCESSFULL MATCH!  Flag that we have moved, update the point pointers, and save the match.
			(void) bufJump(&regScan.regMatch.startPoint.u.bufPoint, groups[0].rm_eo, NULL, direct, &regScan.point);
			movePoint(&regScan.point);				// Leave point at end of string.
			if(saveMatch(&searchCtrl.match, NULL, &regScan.regMatch) != Success)
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
			return regError(direct == Forward ? pMatch->pat : pMatch->regPat.backPat, r);

		// Found match, but not nth one.  Continue searching.
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
	regamatch_t approxMatch = {pMatch->grpCount + 1, groups};
	regaparams_t params;
	size_t srcLen = strlen(pSrc->str);
	char *str;
	char workBuf[srcLen + 1];		// For reversed string, if needed.

	// Get ready.  If scanning backward, create local copy of reversed string and use backward RE pattern.
	if(pMatch->flags & SOpt_Fuzzy)
		xregainit(&params, 0);
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
	if((r = (pMatch->flags & SOpt_Fuzzy) ? xregaexec(pRegex, str, &approxMatch, &params, r) :
	 xregexec(pRegex, str, pMatch->grpCount + 1, groups, r)) == 0) {

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
		(void) regError(regMatch.direct == Forward ? pMatch->pat : pMatch->regPat.backPat, r);

	return sess.rtn.status;
	}
