// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// replace.c	Search and replace functions for MightEMacs.

#include "std.h"
#include "bind.h"
#include "exec.h"
#include "search.h"

// Query replace control objects and flags.
typedef struct {
	Datum *pMatchStr;		// Last match string (for "undo" in query mode).
	int matchLen;			// Last match length.
	Mark replPoint;			// Point position after last replacement.
	int replLen;			// Length of replacement string.
	ushort flags;			// QR_LastWasNL flag or zero.
	} TextRepl;			// Text replacement information (for undo).
typedef struct {
	Datum *pReplPat;		// Plain text replacement string, or NULL if pattern.
	int numSub;			// Running count of substitutions made.
	Mark origPoint;			// Original point position and framing (for . query response).
	Match *pMatch;			// Match object pointer.
	Mark matchPoint;		// Point position of last match (for repeating match detection is non-query mode).
	Array replStack;		// Match stack (array of TextRepl objects, which may be empty).
	Datum **ppLastRepl;		// Last replacement made (TextRepl object), or NULL if stack is empty.
	ushort flags;			// Control flags.
	} QueryCtrl;

// Macro to access TextRepl object in a datum.
#define trptr(ppDatum)	((TextRepl *)(dgetmemptr((*ppDatum))))

// Query control flags.
#define QR_Forever	0x0001		// Do replacements until end of buffer is reached.
#define QR_GoBack	0x0002		// Return to starting position when done.
#define QR_LastWasNo	0x0004		// Last query response was 'n'.
#define QR_LastWasYes	0x0008		// Last query response was 'Y'.
#define QR_LastWasNL	0x0010		// Last replacement char was a newline.
#define QR_LastHitEOB	0x0020		// Last search matched newline at end of buffer.
#define QR_MatchFound	0x0040		// At least one match found.
#define QR_UseRMP	0x0080		// Using replacement meta-pattern.
#define QR_Abort	0x0100		// User pressed abort key (C-g).

// Free all heap space for RE replacement pattern in given Match object.
static void freeRepl(Match *pMatch) {

	if(pMatch->compReplPat != NULL) {
		ReplPat *pReplPat, *pReplPat1;

		pReplPat = pMatch->compReplPat;
		do {
			pReplPat1 = pReplPat->next;
			free((void *) pReplPat);
			} while((pReplPat = pReplPat1) != NULL);
		pMatch->compReplPat = NULL;
		}
	}

// Delete specified length from the current point, then either insert src string directly, or make use of replacement meta-array
// if pFlags not NULL and QR_UseRMP flag in *pFlags is true.  If pReplLen not NULL, assume pFlags not NULL and set *pReplLen to
// length of replacement string, and set *pFlags to QR_LastWasNL if last replacement character was a newline, otherwise, zero.
static int delInsert(int delLen, const char *src, ushort *pFlags, int *pReplLen) {
	const char *replStr;
	int replLen = 0;

#if MMDebug & Debug_SrchRepl
	Point *pPoint = &sess.cur.pFace->point;
	fprintf(logfile, "delInsert(delLen %d, src '%s', flags %.4x)... at offset %d on line:\n  \"%.*s\"\n",
	 delLen, src == NULL ? "NULL" : src, pFlags == NULL ? 0 : (uint) *pFlags, pPoint->offset, pPoint->pLine->used,
	 pPoint->pLine->text);
#endif
	// Zap the matched text, and insert its replacement.
	if(edelc(delLen, 0) != Success)
		return sess.rtn.status;
	if(pFlags != NULL && *pFlags & QR_UseRMP) {
		Match *pMatch = &bufSearch.match;
		ReplPat *pReplPat = pMatch->compReplPat;
		replStr = "";
		while(pReplPat != NULL) {
			replStr = (pReplPat->type == RPE_LitString) ? pReplPat->u.replStr :
			 pMatch->grpMatch.groups[pReplPat->u.grpNum].str;
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "  useRMP: inserting '%s'...\n", replStr == NULL ? "NULL" : replStr);
#endif
			if(einserts(replStr) != Success)
				return sess.rtn.status;
			replLen += strlen(replStr);
			pReplPat = pReplPat->next;
			}
		}
	else {
#if MMDebug & Debug_SrchRepl
		fprintf(logfile, "  useLIT: inserting '%s'...\n", src == NULL ? "NULL" : src);
#endif
		if(einserts(src) != Success)
			return sess.rtn.status;
		replLen = strlen(replStr = src);
		}

	// Update controls and return.
	if(pReplLen != NULL) {
		*pReplLen = replLen;
		*pFlags = (*replStr != '\0' && strchr(replStr, '\0')[-1] == '\n') ? QR_LastWasNL : 0;
		}

	return sess.rtn.status;
	}

// Create ReplPat (replacement) node if *curNode is NULL and initialize it.  Return status.
static int makeReplNode(ReplPat **curNode, ReplPat **prev, int len) {
	ReplPat *pReplPat;

	if((pReplPat = (ReplPat *) malloc(sizeof(ReplPat) + ((size_t) len > sizeof(pReplPat->u) ? len - sizeof(pReplPat->u) :
	 0))) == NULL)
		return rsset(Panic, 0, text94, "makeReplNode");
			// "%s(): Out of memory!"
	*prev = *curNode = pReplPat;
	pReplPat->next = NULL;
	return sess.rtn.status;
	}

// Create a RPE_LitString object in the replacement RE array.  "len" is the length of the string literal, including the
// terminating null byte.
static int makeReplLit(ReplPat **curNode, ReplPat **prev, const char *src, int len) {

	if(makeReplNode(curNode, prev, len) == Success) {
		(*curNode)->type = RPE_LitString;
		stplcpy((*curNode)->u.replStr, src, len);
		}
	return sess.rtn.status;
	}

// Set up a replacement pattern linked list and return status.  If SRegical is set in Match object, all back reference sequences
// (\0..\9) are recognized in the pattern in addition to escaped characters, otherwise just \0 and escaped characters (\1..\9
// generate an error).  Note that if there are no meta-characters encountered in the replacement string (RRegical flag not set
// by this routine), the list is never actually used -- the find and replace routines will just use the character array
// pMatch->replPat verbatim as the replacement string.
int compileRepl(Match *pMatch) {
	ReplPat *pReplPat, **pNode;
	char *pat;
	short patChar;
	short litLen;

	pat = pMatch->replPat;
	pNode = &pMatch->compReplPat;
	pMatch->flags &= ~RRegical;
	litLen = 0;

	while(*pat != '\0') {
		if(*pat == '\\') {		// Escape character (backslash).

			// Peek at next char.
			patChar = *(pat + 1);
			if(patChar >= '0' && patChar <= '9') {

				// Note that \1..\9 will fail (below) if SRegical not set in Match object.
				if(litLen != 0) {
					if(makeReplLit(&pReplPat, pNode, pat - litLen, litLen + 1) != Success)
						return sess.rtn.status;
					pNode = &pReplPat->next;
					litLen = 0;
					}
				if(makeReplNode(&pReplPat, pNode, -1) != Success)
					return sess.rtn.status;
				pReplPat->type = RPE_GrpMatch;

				// Group number reference out of range?
				pReplPat->u.grpNum = patChar - '0';
				if(pReplPat->u.grpNum > pMatch->grpCount) {
					(void) rsset(Failure, 0, text302, pReplPat->u.grpNum, pMatch->grpCount,
						// "No such group (ref: %hu, have: %hu) in replacement pattern '%s'"
					 pMatch->replPat);
					freeRepl(pMatch);
					return sess.rtn.status;
					}
				pNode = &pReplPat->next;
				if(*pat == '\\')
					++pat;
				}
			else {
				// We use litLen + 2 here instead of 1, because we want to include the backslash (which will be
				// overwritten below).
				if(makeReplLit(&pReplPat, pNode, pat - litLen, litLen + 2) != Success)
					return sess.rtn.status;

				// If backslash is not the last character in the string, find out what it is escaping,
				// and overwrite the last character with it.
				if(patChar != '\0') {

					// Check for \c.
					switch(patChar) {
						case 't':
							patChar = '\t'; break;
						case 'r':
							patChar = '\r'; break;
						case 'n':
							patChar = '\n'; break;
						case 'f':
							patChar = '\f'; break;
						case 'e':
							patChar = '\e'; break;
						case 'v':
							patChar = '\v'; break;
						}
					pReplPat->u.replStr[litLen] = patChar;
					++pat;
					}
				pNode = &pReplPat->next;
				litLen = 0;
				}
			pMatch->flags |= RRegical;
			}
		else
			++litLen;
		++pat;
		}

	// Finish last literal if needed.
	if(pMatch->flags & RRegical && litLen > 0)
		(void) makeReplLit(&pReplPat, pNode, pat - litLen, litLen + 1);

#if MMDebug & Debug_SrchRepl
	// Dump compiled pattern to log file.
	fputs("compileRepl():\n", logfile);
	for(pReplPat = pMatch->compReplPat; pReplPat != NULL; pReplPat = pReplPat->next) {
		if(pReplPat->type == RPE_LitString)
			fprintf(logfile, "  '%s'\n", pReplPat->u.replStr);
		else
			fprintf(logfile, "  \\%hu\n", pReplPat->u.grpNum);
		}
#endif
	return sess.rtn.status;
	}

#if MMDebug & Debug_SrchRepl
// Dump point in a Mark object to log file.
static void dumpPoint(Mark *pMark) {
	Point *pPoint = &pMark->point;

	if(pPoint->pLine == NULL)
		fputs("NULL", logfile);
	else
		fprintf(logfile, "point offset: %d \"%.*s\"", pPoint->offset, pPoint->pLine->used, pPoint->pLine->text);
	}

// Dump QueryCtrl object and most recent TextRepl object to log file.
static void qcDump(QueryCtrl *pQueryCtrl, const char *label) {

	fprintf(logfile, "---------- QueryCtrl: %s\n\
%-20s\"%s\"\n\
%-20s%d\n\
%-20s", label,
 "pReplPat", pQueryCtrl->pReplPat == NULL ? "NULL" : pQueryCtrl->pReplPat->str,
 "numSub", pQueryCtrl->numSub,
 "origPoint");
	dumpPoint(&pQueryCtrl->origPoint);
	fprintf(logfile, "\n\
%-20s\"%s\" -> \"%s\", group 0: \"%s\"\n\
%-20s",
 "pMatch", pQueryCtrl->pMatch->pat, pQueryCtrl->pMatch->replPat == NULL ? "NULL" : pQueryCtrl->pMatch->replPat,
 pQueryCtrl->pMatch->grpMatch.groups[0].str,
 "matchPoint");
	dumpPoint(&pQueryCtrl->matchPoint);
	fprintf(logfile, "\n%-20s%.4hx\n---------- TextRepl #", "flags", pQueryCtrl->flags);
	if(pQueryCtrl->ppLastRepl == NULL)
		fputs("-1\n\tNULL OBJECT\n", logfile);
	else {
		TextRepl *pTextRepl = trptr(pQueryCtrl->ppLastRepl);
		fprintf(logfile, "%u\n\
%-20s\"%s\"\n\
%-20s%d\n\
%-20s",
 (uint)(pQueryCtrl->ppLastRepl - pQueryCtrl->replStack.elements),
 "pMatchStr", dtypstr(pTextRepl->pMatchStr) ? pTextRepl->pMatchStr->str : "nil",
 "matchLen", pTextRepl->matchLen,
 "replPoint");
		dumpPoint(&pTextRepl->replPoint);
		fprintf(logfile, "\n\
%-20s%d\n\
%-20s%.4hu\n",
 "replLen", pTextRepl->replLen,
 "flags", pTextRepl->flags);
		}
	}
#endif

// Pop item from undo stack.
static void trPop(QueryCtrl *pQueryCtrl) {
	TextRepl *pTextRepl = trptr(pQueryCtrl->ppLastRepl);
	delMark1(&pTextRepl->replPoint, MKStatic);
	++sess.maxWindID;
	dfree(pTextRepl->pMatchStr);
	dfree(apop(&pQueryCtrl->replStack));
	if(pQueryCtrl->replStack.used == 0)
		pQueryCtrl->ppLastRepl = NULL;
	else
		--pQueryCtrl->ppLastRepl;
	}

// Pop all items from undo stack.
static void trPopAll(QueryCtrl *pQueryCtrl) {

	while(pQueryCtrl->ppLastRepl != NULL)
		trPop(pQueryCtrl);
	}

// Create TextRepl object and push onto stack.
static int trNew(QueryCtrl *pQueryCtrl, TextRepl **ppTextRepl) {
	TextRepl textRepl, *pTextRepl;
	Datum *pDatum;
	Mark *pMark;

	if((textRepl.pMatchStr = dnew()) == NULL || (pDatum = aget(&pQueryCtrl->replStack, pQueryCtrl->replStack.used,
	 OpGrow)) == NULL || dsetmem((void *) &textRepl, sizeof(TextRepl), pDatum) != 0)
		return libfail();
	pQueryCtrl->ppLastRepl = pQueryCtrl->replStack.elements + pQueryCtrl->replStack.used - 1;
	*ppTextRepl = pTextRepl = trptr(pQueryCtrl->ppLastRepl);
	pMark = &pTextRepl->replPoint;
	(void) findBufMark(sess.maxWindID--, &pMark, MKStatic);
	return sess.rtn.status;
	}

// Do query "undo".  It is assumed that undo stack is not empty.  Return status.
static int undo(QueryCtrl *pQueryCtrl, Point *pPoint) {
	TextRepl *pTextRepl = trptr(pQueryCtrl->ppLastRepl);

#if MMDebug & Debug_SrchRepl
	qcDump(pQueryCtrl, "Pre-UNDO");
#endif
	// Restore old position.
	*pPoint = pTextRepl->replPoint.point;
	(void) moveChar(-pTextRepl->replLen);

	// Delete the new string and restore the old match.
	if(delInsert(pTextRepl->replLen, pTextRepl->pMatchStr->str, NULL, NULL) != Success)
		return sess.rtn.status;

	// Decrement substitution counter, back up (to beginning of prior match), pop stack, and return.
	--pQueryCtrl->numSub;
	(void) moveChar(-pTextRepl->matchLen);
	trPop(pQueryCtrl);
	return sess.rtn.status;
	}

// Do non-query initialization.
static void replInit(QueryCtrl *pQueryCtrl, TextRepl *pTextRepl) {
	Mark *pMark = &pQueryCtrl->matchPoint;
	(void) findBufMark(sess.maxWindID--, &pMark, MKStatic);		// Can't fail.
	pMark = &pTextRepl->replPoint;
	(void) findBufMark(sess.maxWindID--, &pMark, MKStatic);		// Can't fail.

	// Prevent bogus "repeating match" error after initial scan.
	pQueryCtrl->matchPoint.point = pTextRepl->replPoint.point = (Point) {NULL};
	}

// Display the query-replace prompt.  If pPat is not NULL (constant replacement pattern), use it directly in the prompt;
// otherwise, build one.  Return status.
static int replQuery(Datum *pPat) {
	DFab msg;
	Datum *pDatum;
	Match *pMatch = &bufSearch.match;
	char lineBuf[TTY_MaxCols + 1];

	// Display the matched string in roughly half the terminal width.
	if((pDatum = dnewtrack()) == NULL || dopenwith(&msg, pDatum, FabClear) != 0 ||
	 dputs(pMatch->grpMatch.groups[0].str, &msg, CvtVizChar) != 0 || dclose(&msg, FabStr) != 0)
		goto LibFail;
	strfit(lineBuf, (size_t) term.cols / 2 - 8, pDatum->str, 0);
	if(mlputs(text87, MLHome | MLTermAttr | MLForce) != Success || mlputs(lineBuf, MLForce) != Success ||
		// "~bReplace~B \""
	 mlputs(text382, MLTermAttr | MLForce) != Success)
			// "\" ~bwith~B \""
		return sess.rtn.status;

	// Display the replacement string.  If not Regexp matching or replacement pattern contains no metacharacters, replPat
	// (from caller) will contain the replacement string (which is constant); otherwise, need to build it from the matched
	// string.
	if(pPat == NULL) {
		ReplPat *pReplPat = pMatch->compReplPat;

		// Save entire replacement string in a fabrication object, temporarily.
		if(dopenwith(&msg, pDatum, FabClear) != 0)
			goto LibFail;
		while(pReplPat != NULL) {
			if(dputs(pReplPat->type == RPE_LitString ? pReplPat->u.replStr :
			 pMatch->grpMatch.groups[pReplPat->u.grpNum].str, &msg, CvtVizChar) != 0)
				goto LibFail;
			pReplPat = pReplPat->next;
			}
		if(dclose(&msg, FabStr) != 0)
LibFail:
			return libfail();
		pPat = pDatum;
		}
	strfit(lineBuf, (size_t) term.cols - term.msgLineCol - 2, pPat->str, 0);
	if(mlputs(lineBuf, MLForce) == Success)
		(void) mlputs("\"?", MLNoEOL | MLFlush | MLForce);
	return sess.rtn.status;
	}

// Return true if point has moved, otherwise false.
static bool pointMoved(Point *pPoint, QueryCtrl *pQueryCtrl) {

	return pPoint->pLine != pQueryCtrl->origPoint.point.pLine || pPoint->offset != pQueryCtrl->origPoint.point.offset;
	}

// Get response (key) from user and store in *extKey.  Return status.
static int response(ExtKey *pExtKey, QueryCtrl *pQueryCtrl) {

	// Loop until response not undo or valid undo...
	for(;;) {
		if(getkey(true, pExtKey, false) != Success)	// Get a key.
			return sess.rtn.status;
		if(pQueryCtrl->ppLastRepl != NULL || (*pExtKey != 'u' && *pExtKey != 'U'))
			break;
		tbeep();					// Nothing to undo.
		}
	if(*pExtKey == coreKeys[CK_Abort].extKey)		// Abort key (C-g) pressed?
		pQueryCtrl->flags |= QR_Abort;			// Yes, set flag.

	// Clear the prompt and return.
	return mlerase(MLForce);
	}

// Search for a string in the current buffer and replace it with another string.  If script mode, args contains arguments;
// otherwise, get them interactively.  If qRepl is true, do query replace and, if script mode, set pRtnVal to false if search
// ends prematurely (failure), otherwise true (success).  If point has moved after search ends, set mark WorkMark to original
// position and notify user of such.  Return status.
int replStr(Datum *pRtnVal, long n, Datum **args, bool qRepl) {
	QueryCtrl queryCtrl;
	TextRepl textRepl, *pTextRepl;
	Mark *pMark;
	int (*srch)(int nth, int *pLineBreakLimit, uint direct, long *pMatchLen);
	long matchLen;
	Point *pPoint = &sess.cur.pFace->point;
	int lineBreakLimit0, lineBreakLimit;
	int status = NotFound;
	bool qResumePlainAll;		// Resume query-replace? OR Plain text pattern? OR Undo all?
	ExtKey extKey;
#if MMDebug & Debug_SrchRepl
	char keyBuf[16];

	fprintf(logfile, "replStr() BEGIN: bufSearch.match.flags: %.4hx\n", bufSearch.match.flags);
#endif

	// Get ready.
	queryCtrl.pReplPat = NULL;
	queryCtrl.flags = QR_Forever;
	queryCtrl.pMatch = &bufSearch.match;
	ainit(&queryCtrl.replStack);
	queryCtrl.ppLastRepl = NULL;

	// Set lineBreakLimit0 from numeric argument so that lineBreakLimit can be reinitialized from lineBreakLimit0 on a
	// restart.
	if(n < 0 && n != DefN) {
		lineBreakLimit0 = -n;
		n = DefN;
		}
	else {
		lineBreakLimit0 = 0;
		if(n > 0)
			queryCtrl.flags = 0;		// Clear QR_Forever flag.
		}

	// Get the search pattern and replacement string.
	qResumePlainAll = n == 0 && qRepl && !(sess.opFlags & OpScript);
	if(getPat(args, qRepl ? text85 : text84, qResumePlainAll ? GPQResume | GPSearchPat : GPSearchPat) != Success ||
			// "Query replace" or "Replace"
	 getPat(args + 1, text86, qResumePlainAll ? GPQResume : 0) != Success)
			// "with"
		goto Retn;

	// Create search tables if needed.
	if(makeSearchTables(false, &qResumePlainAll) != Success)
		goto Retn;
	srch = qResumePlainAll ? scan : regScan;

	// Compile replacement pattern.
	if(queryCtrl.pMatch->compReplPat == NULL)
		if(compileRepl(queryCtrl.pMatch) != Success)
			goto Retn;

	// Create visible form of replacement pattern if possible.
	if(!(queryCtrl.pMatch->flags & RRegical)) {
		DFab fab;

		if(dopentrack(&fab) != 0 || dputs(queryCtrl.pMatch->replPat, &fab, CvtVizChar) != 0 ||
		 dclose(&fab, FabStr) != 0)
			goto LibFail;
		queryCtrl.pReplPat = fab.pDatum;
		}
	else
		queryCtrl.flags |= QR_UseRMP;

	// Save original point position and initialize substitution counter.
	pMark = &queryCtrl.origPoint;
	pMark->point = *pPoint;
	queryCtrl.numSub = 0;

	// Skip search if buffer is empty or repeat count is zero and either "replace" command or ("queryReplace" and)
	// script mode.
	if(bempty(NULL) || (n == 0 && (!qRepl || sess.opFlags & OpScript)))
		goto Rpt;

	// Add original point "mark" to mark list.
	pMark->reframeRow = getWindPos(sess.pCurWind);
	(void) findBufMark(sess.maxWindID--, &pMark, MKStatic);		// Can't fail.

	// Initialize other Mark objects if not query-replacing.
	if(!qRepl)
		replInit(&queryCtrl, pTextRepl = &textRepl);

	// Scan the buffer until we make the nth substitution, reach line limit, or hit end of buffer.  If point has moved at
	// that time and interactive, prompt for final action to take.
#if MMDebug & Debug_SrchRepl
	if(qRepl) queryCtrl.matchPoint.point = (Point) {NULL};	// Not used for query-replace.
	fputs("replStr(): Beginning scan loop...\n", logfile);
#endif
	for(;;) {
		// Scan the buffer.  The basic loop is to find the next match and process it, jump forward in the buffer past
		// the match (if it was not replaced), and repeat.
		lineBreakLimit = lineBreakLimit0;
		for(;;) {
#if MMDebug & Debug_SrchRepl
			fputs("replStr(): Searching for pattern...\n", logfile);
#endif
			// Search for the pattern.  The scanning routines will set matchLen to the true length of the
			// matched string.
			queryCtrl.flags &= ~QR_LastHitEOB;
			if((status = srch(1, &lineBreakLimit, Forward, &matchLen)) == NotFound) {
#if MMDebug & Debug_SrchRepl
				fputs("\tNOT FOUND\n", logfile);
#endif
				break;			// All done.
				}
			if(status != Success)
				goto Retn;

			// Match found.  Set flag and also EOB flag if point is now at end of buffer (so we can force loop exit
			// after this iteration), save length, and move to beginning of match.
			queryCtrl.flags |= bufEnd(pPoint) ? QR_MatchFound | QR_LastHitEOB : QR_MatchFound;
			(void) moveChar(-matchLen);
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "replStr(): match found for /%s/, len %ld, at '%.*s'\n", queryCtrl.pMatch->pat,
			 matchLen, (int)(pPoint->pLine->used - pPoint->offset), pPoint->pLine->text + pPoint->offset);
			qcDump(&queryCtrl, "Match found");
#endif

			// If we are not doing query-replace and we have currently matched an empty string, make sure that we
			// are not at the same buffer position as the last match or replacement (possible in Regexp mode)
			// because we'll go into an infinite loop.
			if(!qRepl) {
				if(matchLen == 0 && ((pPoint->pLine == queryCtrl.matchPoint.point.pLine &&
				 pPoint->offset == queryCtrl.matchPoint.point.offset) ||
				 (pPoint->pLine == pTextRepl->replPoint.point.pLine &&
				 pPoint->offset == pTextRepl->replPoint.point.offset)))
					return rsset(Failure, RSNoFormat, text91);
						// "Repeating match at same position detected"
				queryCtrl.matchPoint.point = *pPoint;
				}
			else {
				// Build the query prompt and display it.
				if(replQuery(queryCtrl.pReplPat) != Success)
					goto Retn;

				queryCtrl.flags &= ~QR_LastWasNo;

				// Update the position on the mode line if needed.
				if(modeSet(MdIdxLine, NULL) || modeSet(MdIdxCol, NULL))
					sess.pCurWind->face.wflags |= WFMode;

				// Loop until get valid response.  Show the proposed place to change and set hard update flag so
				// that any previous replacement that is still visible will be updated on screen.
				sess.pCurWind->face.wflags |= WFHard;
				for(;;) {
					if(update(DefN) != Success)
						goto Retn;
					if(response(&extKey, &queryCtrl) != Success)
						goto Retn;
#if MMDebug & Debug_SrchRepl
					fprintf(logfile, "Got key: \"%s\"\n", ektos(keyBuf, extKey, false));
#endif

					// If user abort, simulate "quit" response and clean up before abort return.
					if(queryCtrl.flags & QR_Abort)
						goto EndScan;

					// Not abort.  Process key that was pressed.
					switch(extKey) {
						case 'Y':			// Yes: substitute and stop.
							queryCtrl.flags = (queryCtrl.flags & ~QR_Forever) | QR_LastWasYes;
							n = queryCtrl.numSub + 1;
							// Fall through.
						case 'y':			// Yes: substitute.
						case ' ':
							goto Replace;
						case 'n':			// No: onward.
							(void) moveChar(1);	// Skip past the current match.
							queryCtrl.flags |= QR_LastWasNo;
							goto Onward;
						case '!':			// Yes: stop asking.
							trPopAll(&queryCtrl);	// Switch to non-query mode.
							replInit(&queryCtrl, pTextRepl = &textRepl);
							queryCtrl.matchPoint.point = *pPoint;
							qRepl = false;
							goto Replace;
						case '.':			// Stop and return to original buffer position.
							queryCtrl.flags |= QR_GoBack;
							goto EndScan;
						case 'q':			// Quit or escape key...
						case Ctrl | '[':
							goto EndScan;		// stop and stay at current buffer position.
						case 'r':			// Restart: go back and start over.
							goto Restart;
						case 'u':			// Undo last and re-prompt.
Undo:
							qResumePlainAll = false;
							goto Undo1;
						case 'U':			// Undo all and re-prompt.
UndoAll:
							qResumePlainAll = true;
							do {
Undo1:
								if(undo(&queryCtrl, pPoint) != Success)
									goto Retn;
								} while(qResumePlainAll && queryCtrl.ppLastRepl != NULL);
							goto Onward;
						default:			// Complain and beep.
							tbeep();
							// Fall through.
						case '?':			// Display help.
							if(mlputs(text90, MLHome | MLTermAttr | MLFlush | MLForce) != Success)
		// "~uSPC~U|~uy~U ~bYes~B, ~un~U ~bNo~B, ~uY~U ~bYes & quit~B, ~u!~U ~bDo rest~B, ~uu~U|~uU~U ~bUndo last|all~B,
		// ~ur~U ~bRestart~B, ~uESC~U|~uq~U ~bQuit~B, ~u.~U ~bQuit & go back~B",
								goto Retn;
						}	// End of switch.
					}	// End of loop.
				}	// End of "if(qRepl)".
Replace:
			// Do replacement.  If query-replacing, get new TextRepl object and push onto stack.
			if(qRepl && trNew(&queryCtrl, &pTextRepl) != Success)
				goto Retn;

			// Delete the sucker, insert its replacement, and count it.
			pTextRepl->flags = queryCtrl.flags & QR_UseRMP;
			if(delInsert(matchLen, queryCtrl.pMatch->replPat, &pTextRepl->flags, &pTextRepl->replLen) != Success)
				goto Retn;
			++queryCtrl.numSub;

			// Save our position, the match length, and the string matched if we are query-replacing, so that we may
			// undo the replacement if requested.
			pTextRepl->replPoint.point = *pPoint;
			if(qRepl) {
				pTextRepl->matchLen = matchLen;
				if(dsetstr(queryCtrl.pMatch->grpMatch.groups[0].str, pTextRepl->pMatchStr) != 0)
					goto LibFail;
				}

#if MMDebug & Debug_SrchRepl
			qcDump(&queryCtrl, "Post-replacement");
#endif
			// If the last match reached the end of the buffer, we're done.  Delete any extra newline and break out.
			if(queryCtrl.flags & QR_LastHitEOB) {
				if(pTextRepl->flags == QR_LastWasNL)
					(void) edelc(1, 0);
				break;
				}

			// n or more matches replaced?
			if(!(queryCtrl.flags & QR_Forever) && queryCtrl.numSub >= n)
				break;
Onward:;
			}

		// Inner loop completed.  Prompt user for final action to take, if applicable.
		if(!qRepl || queryCtrl.flags & QR_LastWasYes || !(queryCtrl.flags & QR_MatchFound))
			break;
		if(mlputs(text304, MLHome | MLTermAttr | MLFlush | MLForce) != Success)
				// "Scan completed.  Press ~uq~U to quit or ~u.~U to go back."
			goto Retn;
		if(queryCtrl.flags & QR_LastWasNo) {
			(void) moveChar(-1);			// Move back to beginning of last match.
			sess.pCurWind->face.wflags |= WFMove;
			queryCtrl.flags &= ~QR_LastWasNo;
			}

		// Loop until get valid response.
		for(;;) {
			if(update(DefN) != Success)
				goto Retn;
			if(response(&extKey, &queryCtrl) != Success)
				goto Retn;
			if(queryCtrl.flags & QR_Abort)
				goto EndScan;
			switch(extKey) {
				case 'q':			// Stop here.
				case Ctrl | '[':
					goto EndScan;
				case '.':			// Stop and return to original buffer position.
					queryCtrl.flags |= QR_GoBack;
					goto EndScan;
				case 'r':			// Restart: go back and start over.
					goto Restart;
				case 'u':			// Undo last and re-prompt.
					goto Undo;
				case 'U':			// Undo all and re-prompt.
					goto UndoAll;
				default:			// Complain and beep.
					tbeep();
					// Fall through.
				case '?':			// Display help.
					if(mlputs(text258, MLHome | MLTermAttr | MLFlush | MLForce) != Success)
							// "~uESC~U|~uq~U ~bStop here~B, ~u.~U ~bStop and go back~B,
							// ~uu~U|~uU~U ~bUndo last|all~B, ~ur~U ~bRestart~B, ~u?~U ~bHelp~B"
						goto Retn;
				}
			}
Restart:
		// Return to original buffer position and start over.
		*pPoint = queryCtrl.origPoint.point;
		}
EndScan:
#if MMDebug & Debug_SrchRepl
	fputs("replStr(): END scan loop.\n", logfile);
#endif
	// Clear undo stack (if query-replace) or marks (otherwise).
	if(qRepl)
		trPopAll(&queryCtrl);
	else {
		delMark1(&pTextRepl->replPoint, MKStatic);
		delMark1(&queryCtrl.matchPoint, MKStatic);
		}
	delMark1(&queryCtrl.origPoint, MKStatic);
	sess.maxWindID = USHRT_MAX;

	// If abort key was pressed, return abort status.
	if(queryCtrl.flags & QR_Abort)
		return abortInp();

	// Adjust point if needed.
	if(qRepl) {
		if(queryCtrl.flags & QR_GoBack) {
			*pPoint = queryCtrl.origPoint.point;
			sess.pCurWind->reframeRow = queryCtrl.origPoint.reframeRow;
			sess.pCurWind->face.wflags |= WFReframe;
			}
		else if(queryCtrl.flags & QR_LastWasNo) {
			(void) moveChar(-1);			// Move back to beginning of last match.
			sess.pCurWind->face.wflags |= WFMove;
			}
		}
Rpt:
	// Report the results.
	rsclear(0);
	(void) rsset(Success, RSTermAttr, text92, queryCtrl.numSub, queryCtrl.numSub == 1 ? "" : "s");
				// "%'d substitution%s"
	if(pointMoved(pPoint, &queryCtrl)) {
		Mark *pMark;

		if(findBufMark(WorkMark, &pMark, MKCreate) != Success)
			goto Retn;
		pMark->point = queryCtrl.origPoint.point;
		pMark->reframeRow = queryCtrl.origPoint.reframeRow;

		// Append to return message if it was set successfully.
		if(!disnull(&sess.rtn.msg)) {
			char workBuf[WorkBufSize];

			sprintf(workBuf, text233 + 1, WorkMark);
				// "Mark ~u%c~U set to previous position"
			if(rscat(", %c%s", lowCase[(int) *text233], workBuf) != Success)
				goto Retn;
			}
		}

	// If query-replacing, return Boolean result (success or failure), otherwise number of substitutions made.
	if(qRepl)
		dsetbool(status == NotFound || queryCtrl.numSub >= n, pRtnVal);
	else
		dsetint(queryCtrl.numSub, pRtnVal);
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Free all replacement pattern heap space in given Match object.
static void freeReplPat(Match *pMatch) {

	freeRepl(pMatch);
	if(pMatch->replBufSize > 0) {
		free((void *) pMatch->replPat);
		pMatch->replBufSize = 0;
		}
	}

// Initialize parameters for new replacement pattern, which may be null.  If addToRing is true, add pattern to replace
// ring also.
int newReplPat(const char *pat, Match *pMatch, bool addToRing) {
	ushort patLen = strlen(pat);

	// Free RE pattern.
	freeRepl(pMatch);

	// Free up arrays if too big or too small.
	if(pMatch->replBufSize > PatSizeMax || (pMatch->replBufSize > 0 && patLen > pMatch->replBufSize))
		freeReplPat(pMatch);

	// Get heap space for arrays if needed.
	if(pMatch->replBufSize == 0) {
		pMatch->replBufSize = patLen < PatSizeMin ? PatSizeMin : patLen;
		if((pMatch->replPat = (char *) malloc(pMatch->replBufSize + 1)) == NULL)
			return rsset(Panic, 0, text94, "newReplPat");
					// "%s(): Out of memory!"
		}

	// Save replacement pattern.
	strcpy(pMatch->replPat, pat);
	if(addToRing)
		(void) rsave(ringTable + RingIdxRepl, pat, false);	// Add pattern to replacement ring.

	return sess.rtn.status;
	}
