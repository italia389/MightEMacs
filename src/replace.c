// (c) Copyright 2022 Richard W. Marinelli
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
	Datum *pMatchStr;		// Last match string.
	Point matchPoint;		// Point position of last match.
	int matchLen;			// Last match length.
	Point replPoint;		// Point position after last replacement.
	int replLen;			// Length of replacement string.
	} LastMatch;			// Info about last replacement (for undo).
typedef struct {
	Datum *pReplPat;		// Plain text replacement string, or NULL if pattern.
	int numSub;			// Running count of substitutions made.
	Mark origPoint;			// Original point position and offset (for . query response).
	Match *pMatch;			// Match object pointer.
	LastMatch lastMatch;		// Match information.
	ushort flags;			// Control flags.
	} QueryCtrl;

#define QR_Forever	0x0001		// Do replacements until end of buffer is reached.
#define QR_GoBack	0x0002		// Return to starting position when done.
#define QR_LastWasNo	0x0004		// Last query response was 'n'.
#define QR_LastWasYes	0x0008		// Last query response was 'Y'.
#define QR_LastWasNL	0x0010		// Last replacement char was a newline.
#define QR_LastHitEOB	0x0020		// Last search matched newline at end of buffer.
#define QR_Regexp	0x0040		// Performing RE search.
#define QR_UseRMP	0x0080		// Using replacement meta-pattern.

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

// Delete a specified length from the current point, then either insert the string directly, or make use of replacement
// meta-array if available.  If pQueryCtrl not NULL, set replLen in LastMatch record to length of replacement string and set or
// clear QR_LastWasNL flag if last replacement character was or was not a newline.
static int delInsert(int delLen, const char *src, bool useRMP, QueryCtrl *pQueryCtrl) {
	const char *replStr;
	int replLen = 0;

#if MMDebug & Debug_SrchRepl
	fprintf(logfile, "delInsert(delLen %d, src '%s', useRMP %d, pQueryCtrl %.8x)...\n",
	 delLen, src == NULL ? "NULL" : src, useRMP, (uint) pQueryCtrl);
#endif
	// Zap the matched text, and insert its replacement.
	if(edelc(delLen, 0) != Success)
		return sess.rtn.status;
	if(useRMP) {
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
	if(pQueryCtrl != NULL) {
		pQueryCtrl->lastMatch.replLen = replLen;
		if(*replStr != '\0' && strchr(replStr, '\0')[-1] == '\n')
			pQueryCtrl->flags |= QR_LastWasNL;
		else
			pQueryCtrl->flags &= ~QR_LastWasNL;
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
		if(*pat == MC_Escape) {		// \	(backslash)

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
				if(*pat == MC_Escape)
					++pat;
				}
			else {
				// We use litLen + 2 here instead of 1, because we want to include the backslash (which will be
				// overwritten below).
				if(makeReplLit(&pReplPat, pNode, pat - litLen, litLen + 2) != Success)
					return sess.rtn.status;

				// If MC_Escape is not the last character in the string, find out what it is escaping,
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
	if((pMatch->flags & RRegical) && litLen > 0)
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

// Display the query-replace prompt.  If pPat is not NULL (constant replacement pattern), use it directly in the prompt;
// otherwise, build one.  Return status.
static int replQuery(Datum *pPat) {
	DFab msg;
	Datum *pDatum;
	Match *pMatch = &bufSearch.match;
	char lineBuf[TTY_MaxCols + 1];

	// Display the matched string in roughly half the terminal width.
	if(dnewtrack(&pDatum) != 0 || dopenwith(&msg, pDatum, FabClear) != 0 ||
	 dputs(pMatch->grpMatch.groups[0].str, &msg, DCvtVizChar) != 0 || dclose(&msg, FabStr) != 0)
		goto LibFail;
	strfit(lineBuf, (size_t) term.cols / 2 - 9, pDatum->str, 0);
	if(mlputs(MLHome | MLTermAttr | MLForce, text87) != Success || mlputs(MLForce, lineBuf) != Success ||
				// "~bReplace~B \""
	 mlputs(MLTermAttr | MLForce, text382) != Success)
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
			 pMatch->grpMatch.groups[pReplPat->u.grpNum].str, &msg, DCvtVizChar) != 0)
				goto LibFail;
			pReplPat = pReplPat->next;
			}
		if(dclose(&msg, FabStr) != 0)
LibFail:
			return libfail();
		pPat = pDatum;
		}
	strfit(lineBuf, (size_t) term.cols - term.msgLineCol - 3, pPat->str, 0);
	if(mlputs(MLForce, lineBuf) == Success)
		(void) mlputs(MLNoEOL | MLFlush | MLForce, "\"?");
	return sess.rtn.status;
	}

// Return true if point has moved, otherwise false.
static bool pointMoved(Point *pPoint, QueryCtrl *pQueryCtrl) {

	return pPoint->pLine != pQueryCtrl->origPoint.point.pLine || pPoint->offset != pQueryCtrl->origPoint.point.offset;
	}

// Get response (key) from user and store in *extKey.  Return status.
static int response(ushort *extKey, QueryCtrl *pQueryCtrl) {

	// Loop until response not undo or valid undo...
	for(;;) {
		if(getkey(true, extKey, false) != Success)	// Get a key.
			return sess.rtn.status;
		if(*extKey != 'u' || pQueryCtrl->lastMatch.replPoint.pLine != NULL)
			break;
		tbeep();					// Nothing to undo.
		}

	// Clear the prompt and respond appropriately.
	if(mlerase(MLForce) == Success && *extKey == coreKeys[CK_Abort].extKey)
		return abortInp();
	return sess.rtn.status;
	}

// Search for a string in the current buffer and replace it with another string.  If script mode, args contains arguments;
// otherwise, get them interactively.  If qRepl is true, do query replace and, if script mode, set pRtnVal to false if search
// ends prematurely (failure), otherwise true (success).  If point has moved after search ends, set mark WorkMark to original
// position and notify user of such.  Return status.
int replStr(Datum *pRtnVal, int n, Datum **args, bool qRepl) {
	int status = NotFound;
	int lineBreakLimit;
	ushort extKey;
	QueryCtrl queryCtrl;
	long matchLen;
	Point *pPoint = &sess.cur.pFace->point;

	// Get ready.
	queryCtrl.pReplPat = NULL;
	queryCtrl.flags = QR_Forever;
	queryCtrl.pMatch = &bufSearch.match;

	// Get the pattern and replacement string.
	if(getPat(args, qRepl ? text85 : text84, true) != Success || getPat(args + 1, text86, false) != Success)
				// "Query replace" or "Replace"			"with"
		return sess.rtn.status;

	// Check numeric argument.
	if(n == INT_MIN || n == 0)
		lineBreakLimit = 0;
	else if(n < 0) {
		lineBreakLimit = -n;
		n = INT_MIN;
		}
	else {
		lineBreakLimit = 0;
		queryCtrl.flags = 0;		// QR_Forever flag not set.
		}

	// Clear search groups and create search tables if needed.
	grpFree(queryCtrl.pMatch);

	// Compile search pattern as an RE if requested.
	if(bufRESearch() && !(queryCtrl.pMatch->flags & SCpl_ForwardRE) &&
	 compileRE(queryCtrl.pMatch, SCpl_ForwardRE) != Success)
		return sess.rtn.status;

	// Compile as a plain-text pattern if not an RE request or not really an RE (SRegical not set).
	if(!bufPlainSearch())
		queryCtrl.flags |= QR_Regexp;
	else if(!(bufSearch.match.flags & SCpl_ForwardBM) ||
	 (((queryCtrl.pMatch->flags & SCpl_PlainExact) != 0) != exactSearch(queryCtrl.pMatch)))
		if(compileBM(SCpl_ForwardBM) != Success)
			return sess.rtn.status;

	// Compile replacement pattern.
	if(queryCtrl.pMatch->compReplPat == NULL && compileRepl(queryCtrl.pMatch) != Success)
		return sess.rtn.status;

	// Create visible form of replacement pattern if possible.
	if(!(queryCtrl.pMatch->flags & RRegical)) {
		DFab fab;

		if(dopentrack(&fab) != 0 || dputs(queryCtrl.pMatch->replPat, &fab, DCvtVizChar) != 0 ||
		 dclose(&fab, FabStr) != 0)
			goto LibFail;
		queryCtrl.pReplPat = fab.pDatum;
		}
	else
		queryCtrl.flags |= QR_UseRMP;

	// Save original point position and initialize counters.
	if(!qRepl)
		queryCtrl.lastMatch.matchPoint.pLine = NULL;
	else if(dnewtrack(&queryCtrl.lastMatch.pMatchStr) != 0)
		goto LibFail;
	queryCtrl.origPoint.point = *pPoint;
	queryCtrl.origPoint.reframeRow = getWindPos(sess.cur.pWind);
	queryCtrl.numSub = 0;
	queryCtrl.lastMatch.replPoint.pLine = NULL;	// Disable "undo".
	if(n == 0)
		goto Rpt;

	// Scan the buffer until we make the nth substitution or hit a buffer boundary.  If point has moved at that time and
	// interactive, prompt for action to take.
#if MMDebug & Debug_SrchRepl
	fputs("replStr(): Beginning scan loop...\n", logfile);
#endif
	for(;;) {
		// Scan the buffer.  The basic loop is to find the next match and process it, jump forward in the buffer past
		// the match (if it was not replaced), and repeat.
		for(;;) {
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "replStr(): Searching for pattern /%s/...\n", queryCtrl.pMatch->pat);
#endif
			// Search for the pattern.  The scanning routines will set matchLen to the true length of the
			// matched string.
			if(queryCtrl.flags & QR_Regexp) {
				if((status = regScan(1, &lineBreakLimit, Forward, &matchLen)) == NotFound)
					break;			// All done.
				}
			else if((status = scan(1, &lineBreakLimit, Forward, &matchLen)) == NotFound)
				break;				// All done.
			if(status != Success)
				return sess.rtn.status;

			// Match found.  Set flag if point is now at end-of-buffer (so we can force loop exit after this
			// iteration), save length, and move to beginning of match.
			if(bufEnd(pPoint))
				queryCtrl.flags |= QR_LastHitEOB;
			(void) moveChar(-matchLen);
#if MMDebug & Debug_SrchRepl
			fprintf(logfile, "replStr(): match found for /%s/, len %ld, at '%.*s'\n", queryCtrl.pMatch->pat,
			 matchLen, (int)(pPoint->pLine->used - pPoint->offset), pPoint->pLine->text + pPoint->offset);
#endif

			// If we are not doing query-replace and we have currently matched an empty string, make sure that we
			// are not at the same buffer position as the last match or replacement (possible in Regexp mode)
			// because we'll go into an infinite loop.
			if(!qRepl) {
				if(matchLen == 0 && ((pPoint->pLine == queryCtrl.lastMatch.matchPoint.pLine &&
				 pPoint->offset == queryCtrl.lastMatch.matchPoint.offset) ||
				 (pPoint->pLine == queryCtrl.lastMatch.replPoint.pLine &&
				 pPoint->offset == queryCtrl.lastMatch.replPoint.offset)))
					return rsset(Failure, RSNoFormat, text91);
						// "Repeating match at same position detected"
				queryCtrl.lastMatch.matchPoint = *pPoint;
				}
			else {
				// Build the query prompt and display it.
				if(replQuery(queryCtrl.pReplPat) != Success)
					return sess.rtn.status;

				queryCtrl.flags &= ~QR_LastWasNo;

				// Update the position on the mode line if needed.
				if(modeSet(MdIdxLine, NULL) || modeSet(MdIdxCol, NULL))
					sess.cur.pWind->flags |= WFMode;

				// Loop until get valid response.  Show the proposed place to change and set hard update flag so
				// that any previous replacement that is still visible will be updated on screen.
				sess.cur.pWind->flags |= WFHard;
				for(;;) {
					if(update(INT_MIN) != Success)
						return sess.rtn.status;
					if(response(&extKey, &queryCtrl) != Success)
						return sess.rtn.status;
					switch(extKey) {
						case 'Y':			// Yes, substitute and stop.
							queryCtrl.flags = (queryCtrl.flags & ~QR_Forever) | QR_LastWasYes;
							n = queryCtrl.numSub + 1;
							// Fall through.
						case 'y':			// Yes, substitute.
						case ' ':
							goto Replace;
						case 'n':			// No, onward.
							(void) moveChar(1);	// Skip past the current match.
							queryCtrl.flags |= QR_LastWasNo;
							goto Onward;
						case '!':			// Yes, stop asking.
							qRepl = false;
							goto Replace;
						case '.':			// Stop and return to original buffer position.
							queryCtrl.flags |= QR_GoBack;
							goto Retn;
						case 'q':			// Quit or escape key...
						case Ctrl | '[':
							goto Retn;		// stop and stay at current buffer position.
						case 'r':			// Restart: go back and start over.
							goto Restart;
						case 'u':			// Undo last and re-prompt.
Undo:
							// Restore old position.
							*pPoint = queryCtrl.lastMatch.replPoint;
							queryCtrl.lastMatch.replPoint.pLine = NULL;	// Disable "undo".
							(void) moveChar(-queryCtrl.lastMatch.replLen);

							// Delete the new string and restore the old match.  If current line is
							// the point origin line, set flag (and use queryCtrl.lastMatch.
							// replPoint.pLine temporarily) so we a can update the line pointer
							// after the substitution in case the line is reallocated by
							// delInsert().
							if(pPoint->pLine == queryCtrl.origPoint.point.pLine) {
								queryCtrl.origPoint.point.pLine = NULL;
								queryCtrl.lastMatch.replPoint.pLine = (pPoint->pLine ==
								 sess.cur.pBuf->pFirstLine) ? NULL : pPoint->pLine->prev;
								}
							if(delInsert(queryCtrl.lastMatch.replLen,
							 queryCtrl.lastMatch.pMatchStr->str, false, NULL) != Success)
								return sess.rtn.status;

							// Update origin line pointer if needed.
							if(queryCtrl.origPoint.point.pLine == NULL) {
								queryCtrl.origPoint.point.pLine =
								 (queryCtrl.lastMatch.replPoint.pLine == NULL) ?
								 sess.cur.pBuf->pFirstLine :
								 queryCtrl.lastMatch.replPoint.pLine->next;
								queryCtrl.lastMatch.replPoint.pLine = NULL;
								}

							// Decrement substitution counter, back up (to beginning of prior
							// match), and reprompt.
							--queryCtrl.numSub;
							(void) moveChar(-queryCtrl.lastMatch.matchLen);
							goto Onward;
						default:			// Complain and beep.
							tbeep();
							// Fall through.
						case '?':			// Display help.
							if(mlputs(MLHome | MLTermAttr | MLFlush | MLForce, text90) != Success)
					// "~uSPC~U|~uy~U ~bYes~B, ~un~U ~bNo~B, ~uY~U ~bYes and stop~B, ~u!~U ~bDo rest~B,
					// ~uu~U ~bUndo last~B, ~ur~U ~bRestart~B, ~uESC~U|~uq~U ~bStop here~B,
					// ~u.~U ~bStop and go back~B, ~u?~U ~bHelp~B",
								return sess.rtn.status;
						}	// End of switch.
					}	// End of loop.
				}	// End of "if(qRepl)".
Replace:
			// Do replacement.  If current line is the point origin line, set flag (and use
			// queryCtrl.lastMatch.replPoint.pLine temporarily) so we a can update the line pointer after the
			// substitution in case the line is reallocated by delInsert().
			if(pPoint->pLine == queryCtrl.origPoint.point.pLine) {
				queryCtrl.origPoint.point.pLine = NULL;
				queryCtrl.lastMatch.replPoint.pLine = (pPoint->pLine == sess.cur.pBuf->pFirstLine) ? NULL :
				 pPoint->pLine->prev;
				}

			// Delete the sucker, insert its replacement, and count it.
			if(delInsert(matchLen, queryCtrl.pMatch->replPat, queryCtrl.flags & QR_UseRMP, &queryCtrl) != Success)
				return sess.rtn.status;
			++queryCtrl.numSub;

			// Update origin line pointer if needed.
			if(queryCtrl.origPoint.point.pLine == NULL)
				queryCtrl.origPoint.point.pLine = (queryCtrl.lastMatch.replPoint.pLine == NULL) ?
				 sess.cur.pBuf->pFirstLine : queryCtrl.lastMatch.replPoint.pLine->next;

			// Save our position, the match length, and the string matched if we are query-replacing, so that we may
			// undo the replacement if requested.
			queryCtrl.lastMatch.replPoint = *pPoint;
			if(qRepl) {
				queryCtrl.lastMatch.matchLen = matchLen;
				if(dsetstr(queryCtrl.pMatch->grpMatch.groups[0].str, queryCtrl.lastMatch.pMatchStr) != 0)
					goto LibFail;
				}

			// If the last match included the newline at the end of the buffer, we're done.  Delete any extra line
			// and break out.
			if(queryCtrl.flags & QR_LastHitEOB) {
				if(queryCtrl.flags & QR_LastWasNL)
					(void) edelc(1, 0);
				break;
				}

			// n matches replaced?
			if(!(queryCtrl.flags & QR_Forever) && queryCtrl.numSub == n)
				break;
Onward:;
			}

		// Inner loop completed.  Prompt user for final action to take if applicable.
		if(!qRepl || (queryCtrl.flags & QR_LastWasYes) || !pointMoved(pPoint, &queryCtrl))
			break;
		if(mlputs(MLHome | MLTermAttr | MLFlush | MLForce, text304) != Success)
					// "Scan completed.  Press ~uq~U to quit or ~u.~U to go back."
			return sess.rtn.status;
		if(queryCtrl.flags & QR_LastWasNo) {
			(void) moveChar(-1);
			sess.cur.pWind->flags |= WFMove;
			queryCtrl.flags &= ~QR_LastWasNo;
			}

		// Loop until get valid response.
		for(;;) {
			if(update(INT_MIN) != Success)
				return sess.rtn.status;
			if(response(&extKey, &queryCtrl) != Success)
				return sess.rtn.status;
			switch(extKey) {
				case 'q':			// Stop here.
				case Ctrl | '[':
					goto Retn;
				case '.':			// Stop and return to original buffer position.
					queryCtrl.flags |= QR_GoBack;
					goto Retn;
				case 'r':			// Restart, go back and start over.
					goto Restart;
				case 'u':			// Undo last and re-prompt.
					goto Undo;
				default:			// Complain and beep.
					tbeep();
					// Fall through.
				case '?':			// Display help.
					if(mlputs(MLHome | MLTermAttr | MLFlush | MLForce, text258) != Success)
	// "~uESC~U|~uq~U ~bStop here~B, ~u.~U ~bStop and go back~B, ~uu~U ~bUndo last~B, ~ur~U ~bRestart~B, ~u?~U ~bHelp~B"
						return sess.rtn.status;
				}
			}
Restart:
		// Return to original buffer position and start over.
		*pPoint = queryCtrl.origPoint.point;
		queryCtrl.lastMatch.replPoint.pLine = NULL;
		}
Retn:
#if MMDebug & Debug_SrchRepl
	fputs("replStr(): END scan loop.\n", logfile);
#endif
	// Adjust point if needed.
	if(queryCtrl.flags & QR_GoBack) {
		*pPoint = queryCtrl.origPoint.point;
		sess.cur.pWind->reframeRow = queryCtrl.origPoint.reframeRow;
		sess.cur.pWind->flags |= WFReframe;
		}
	else if(qRepl && (queryCtrl.flags & QR_LastWasNo)) {
		(void) moveChar(-1);
		sess.cur.pWind->flags |= WFMove;
		}
Rpt:
	// Report the results.
	rsclear(0);
	(void) rsset(Success, RSTermAttr, text92, queryCtrl.numSub, queryCtrl.numSub == 1 ? "" : "s");
		// "%d substitution%s"
	if(pointMoved(pPoint, &queryCtrl)) {
		Mark *pMark;
		DFab msg;

		if(findBufMark(WorkMark, &pMark, MKCreate) != Success)
			return sess.rtn.status;
		pMark->point = queryCtrl.origPoint.point;
		pMark->reframeRow = queryCtrl.origPoint.reframeRow;

		// Append to return message if it was set successfully.
		if(!disnull(&sess.rtn.msg)) {
			if(dopenwith(&msg, &sess.rtn.msg, FabAppend) != 0 || dputs(", ", &msg, 0) != 0 ||
			 dputc(lowCase[(int) *text233], &msg, 0) != 0 || dputf(&msg, 0, text233 + 1, WorkMark) != 0 ||
			 dclose(&msg, FabStr) != 0)
					// "Mark ~u%c~U set to previous position"
LibFail:
				return libfail();
			}
		}

	// Return Boolean result.
	dsetbool((qRepl && (status == NotFound || queryCtrl.numSub == n)) ||
	 (!qRepl && (n == INT_MIN || queryCtrl.numSub >= n)), pRtnVal);
	return sess.rtn.status;
	}

// Free all replacement pattern heap space in given Match object.
void freeReplPat(Match *pMatch) {

	freeRepl(pMatch);
	if(pMatch->replPatSize > 0) {
		free((void *) pMatch->replPat);
		pMatch->replPatSize = 0;
		}
	}

// Initialize parameters for new replacement pattern, which may be null.  If addToRing is true, add pattern to replace
// ring also.
int newReplPat(const char *pat, Match *pMatch, bool addToRing) {
	int patLen = strlen(pat);

	// Free RE pattern.
	freeRepl(pMatch);

	// Free up arrays if too big or too small.
	if(pMatch->replPatSize > PatSizeMax || (pMatch->replPatSize > 0 && (uint) patLen > pMatch->replPatSize))
		freeReplPat(pMatch);

	// Get heap space for arrays if needed.
	if(pMatch->replPatSize == 0) {
		pMatch->replPatSize = patLen < PatSizeMin ? PatSizeMin : patLen;
		if((pMatch->replPat = (char *) malloc(pMatch->replPatSize + 1)) == NULL)
			return rsset(Panic, 0, text94, "newReplPat");
					// "%s(): Out of memory!"
		}

	// Save replacement pattern.
	strcpy(pMatch->replPat, pat);
	if(addToRing)
		(void) rsave(ringTable + RingIdxRepl, pat, false);	// Add pattern to replacement ring.

	return sess.rtn.status;
	}
