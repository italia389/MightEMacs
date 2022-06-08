// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// display.c	High level display routines for MightEMacs.
//
// This file contains functions that call the lower level terminal display functions in vterm.c.

#include "std.h"
#include <stdarg.h>
#include "exec.h"

// Find window on current screen whose 'next' pointer matches given pointer and return it, or NULL if not found (that is,
// pWind points to top window).
EWindow *windNextIs(EWindow *pWind) {
	EWindow *pWind1 = sess.windHead;

	if(pWind == pWind1)
		return NULL;			// No window above top window.
	while(pWind1->next != pWind)
		pWind1 = pWind1->next;
	return pWind1;
	}

// Find window on current screen displaying given buffer and return pointer to it, or NULL if not found.  Do not consider
// current window if skipCur is true.
EWindow *windDispBuf(Buffer *pBuf, bool skipCur) {
	EWindow *pWind = sess.windHead;
	do {
		if(pWind->pBuf == pBuf && (pWind != sess.cur.pWind || !skipCur))
			return pWind;
		} while((pWind = pWind->next) != NULL);
	return NULL;
	}

// Flush message line to physical screen.  Return status.
int mlflush(void) {

	return wrefresh(term.pMsgLineWind) == ERR ? sysCallError("mlflush", "wrefresh", false) : sess.rtn.status;
	}

// Move cursor to column "col" in message line.  Return status.
int mlmove(int col) {

	return wmove(term.pMsgLineWind, 0, term.msgLineCol = col) == ERR ? sysCallError("mlmove", "wmove", false) :
	 sess.rtn.status;
	}

// Restore message line cursor position.  Return status.
int mlrestore(void) {

	if(mlmove(term.msgLineCol) == Success)
		(void) mlflush();
	return sess.rtn.status;
	}

// Erase to end of line on message line.
void mleraseEOL(void) {

	// Ignore return code in case cursor is at right edge of screen, which causes an ERR return and a subsequent
	// program abort.
	(void) wclrtoeol(term.pMsgLineWind);
	}

// Erase the message line and return status.  If the MLForce flag is set, force output; otherwise, suppress output if not
// interactive.
int mlerase(ushort flags) {

	// Is anything on message line and interactive or a force?
	if(term.msgLineCol != 0 && (interactive() || (flags & MLForce))) {

		// Yes, home the cursor and erase to end of line.
		if(mlmove(0) == Success) {
			mleraseEOL();
			(void) mlflush();
			}
		}
	return sess.rtn.status;
	}

// Write a character to the message line with invisible characters exposed, unless MLRaw flag is set.  Keep track of the
// physical cursor position so that a LineExt ($) can be displayed at the right edge of the screen if the cursor moves past it
// (unless MLNoEOL flag is set).  Return status.
void mlputc(ushort flags, short c) {

	// Nothing to do if past right edge of screen.
	if(term.msgLineCol >= term.cols)
		return;

	// Raw or plain character?
	if((flags & MLRaw) || (c >= ' ' && c <= '~')) {
		int n;
		bool skipSpace = (c == ' ' && (flags & TA_AltUL));

		// Yes, display it.
		if(c == '\b')
			n = -1;
		else {
			n = 1;
			if(skipSpace)
				tul(true, false);
			if(term.msgLineCol == term.cols - 1 && !(flags & MLNoEOL))
				c = LineExt;
			}
		(void) waddch(term.pMsgLineWind, c);
		term.msgLineCol += n;
		if(skipSpace)
			tul(true, true);
		}
	else {
		char *str;

		// Not raw.  Display char literal (if any) and remember cursor position until it reaches the right edge.
		c = *(str = vizc(c, VizBaseDef));
		do {
			(void) waddch(term.pMsgLineWind, term.msgLineCol == term.cols - 1 && !(flags & MLNoEOL) ? LineExt : c);
			} while(++term.msgLineCol < term.cols && (c = *++str) != '\0');
		}
	}

// Write a string to the message line and return status.  If the MLForce flag is set, force output; otherwise, suppress output
// if not interactive.  If MLTermAttr flag is set, terminal attribute sequences (beginning with '~') in message are processed.
int mlputs(ushort flags, const char *msg) {

#if MMDebug & Debug_MsgLine
	fprintf(logfile, "mlputs(%.4hx, \"%s\")\n", flags, msg);
#endif
	if(interactive() || flags & MLForce) {
		short c;
		const char *msgEnd = strchr(msg, '\0');

#if MMDebug & Debug_MsgLine
		fprintf(logfile, "    -> displaying \"%s\" message...\n", msg);
#endif
		// Position cursor and/or begin wrap, if applicable.
		if(((flags & MLHome) || term.msgLineCol < 0 || term.msgLineCol >= term.cols) && mlerase(flags) != Success)
			return sess.rtn.status;
		if((flags & (MLHome | MLWrap)) == (MLHome | MLWrap))
			mlputc(flags | MLRaw, '[');

		// Copy string to message line.  If MLTermAttr flag is set, scan for attribute sequences and process them.
		while((c = *msg++) != '\0') {
			if(!(flags & MLTermAttr) || c != AttrSpecBegin)
				mlputc(flags, c);
			else
				msg = (const char *) putAttrStr(msg, msgEnd, &flags, (void *) term.pMsgLineWind);
			}

		(void) wattrset(term.pMsgLineWind, 0);

		// Finish wrap and flush message.
		if(flags & MLWrap)
			mlputc(flags | MLRaw, ']');
		if(flags & MLFlush)
			(void) mlflush();
		}
	return sess.rtn.status;
	}

// Write text into the message line, given flag word, format string and optional arguments, and return status.  If the MLForce
// flag is set, force output; otherwise, suppress output if not interactive.  If the MLTermAttr flag is set, terminal attribute
// specifications are recognized in the string that is built (which includes any hard-coded ones in the fmt string).  Any format
// spec recognized by the vasprintf() function is allowed.
int mlprintf(ushort flags, const char *fmt, ...) {

#if MMDebug & Debug_MsgLine
	fprintf(logfile, "mlprintf(%.4hx, \"%s\", ...)\n", flags, fmt);
#endif
	if(interactive() || flags & MLForce) {
		int r;
		va_list varArgList;
		char *str;

		// Process arguments.
		va_start(varArgList, fmt);
		r = vasprintf(&str, fmt, varArgList);
		va_end(varArgList);
		if(r < 0)
			return rsset(Panic, 0, text94, "mlprintf");
				// "%s(): Out of memory!"
		(void) mlputs(flags, str);
		free((void *) str);
		}

	return sess.rtn.status;
	}

// Initialize point position, marks, and first column position of a face record, given line pointer.  If pBuf not NULL, clear
// its buffer marks.
void faceInit(Face *pFace, Line *pLine, Buffer *pBuf) {

	pFace->pTopLine = pFace->point.pLine = pLine;
	pFace->point.offset = pFace->firstCol = 0;

	// Clear mark(s).
	if(pBuf != NULL)
		delBufMarks(pBuf, MKViz | MKWind);
	}

// Copy buffer face record to a window.
void bufFaceToWindFace(Buffer *pBuf, EWindow *pWind) {

	pWind->face = pBuf->face;
	pWind->flags |= (WFHard | WFMode);
	}

// Copy window face record to a buffer and clear the "buffer face in unknown state" flag.
void windFaceToBufFace(EWindow *pWind, Buffer *pBuf) {

	pBuf->face = pWind->face;
	}

// Get ordinal number of given window, beginning at 1.
int getWindNum(EWindow *pWind) {
	EWindow *pWind1 = sess.windHead;
	int windNum = 1;
	while(pWind1 != pWind) {
		pWind1 = pWind1->next;
		++windNum;
		}
	return windNum;
	}

// Get number of windows on given screen and return it.  If pWindNum not NULL, set *pWindNum to screen's current window number.
int getWindCount(EScreen *pScrn, int *pWindNum) {
	EWindow *pWind = pScrn->windHead;
	int count = 0;
	do {
		++count;
		if(pWindNum != NULL && pWind == pScrn->pCurWind)
			*pWindNum = count;
		} while((pWind = pWind->next) != NULL);
	return count;
	}

// Move up or down n lines (if possible) from given window line (or current top line of window if pLine is NULL) and set top
// line of window to result.  If n is negative, move up; otherwise, move down.  Set hard update flag in window and return true
// if top line was changed, otherwise false.
bool windNewTop(EWindow *pWind, Line *pLine, int n) {
	Line *pOldTopLine = pWind->face.pTopLine;
	if(pLine == NULL)
		pLine = pOldTopLine;

	if(n < 0) {
		while(pLine != pWind->pBuf->pFirstLine) {
			pLine = pLine->prev;
			if(++n == 0)
				break;
			}
		}
	else if(n > 0) {
		while(pLine->next != NULL) {
			pLine = pLine->next;
			if(--n == 0)
				break;
			}
		}

	if((pWind->face.pTopLine = pLine) != pOldTopLine) {
		pWind->flags |= (WFHard | WFMode);
		return true;
		}
	return false;
	}

// Switch to given window in current screen.  Return status.
int wswitch(EWindow *pWind, ushort flags) {
	Datum *pRtnVal = NULL;
	bool differentBuf = false;

	// Nothing to do if requested window is already current.
	if(pWind == sess.cur.pWind)
		return sess.rtn.status;

	// Run exit-buffer hook if applicable.
	if(!(flags & SWB_NoBufHooks)) {
		differentBuf = (pWind->pBuf != sess.cur.pBuf);
		if(differentBuf && runBufHook(&pRtnVal, SWB_ExitHook) != Success)
			return sess.rtn.status;
		}

	// Switch windows.
	sess.edit.pBuf = sess.cur.pBuf = (sess.cur.pScrn->pCurWind = sess.edit.pWind = sess.cur.pWind = pWind)->pBuf;
	sess.edit.pFace = sess.cur.pFace = &pWind->face;

	// Run enter-buffer hook if applicable.
	if(differentBuf)
		(void) runBufHook(&pRtnVal, 0);

	return sess.rtn.status;
	}

// Get a required screen or window number (via prompt if interactive) and save in *pNum.  Return status.
static int getNum(const char *prompt, int screen, int *pNum) {
	Datum *pDatum;

	if(dnewtrack(&pDatum) != 0)
		return librsset(Failure);
	if(sess.opFlags & OpScript) {
		if(getNArg(pDatum, NULL) != Success)
			return sess.rtn.status;
		}
	else {
		char workBuf[WorkBufSize];

		// Build prompt with screen or window number range.
		sprintf(workBuf, "%s %s (1-%d)%s", prompt, screen ? text380 : text331,
							// "screen", "window"
		 screen ? scrnCount() : getWindCount(sess.cur.pScrn, NULL), screen > 0 ? text445 : "");
									// " or 0 for new"
		if(getNArg(pDatum, workBuf) != Success || disnil(pDatum))
			return Cancelled;
		}

	// Return integer result.
	*pNum = pDatum->u.intNum;
	return sess.rtn.status;
	}

// Switch to another window per flags.  Return status.
int gotoWind(Datum *pRtnVal, int n, ushort flags) {

	if(n != 0 || !(flags & SWB_Repeat)) {
		EWindow *pWind;
		int windNum, count;
		int windCount = getWindCount(sess.cur.pScrn, &windNum);

		// Check if n is out of range.
		if(flags & SWB_Repeat) {
			if(n == INT_MIN)
				n = 1;
			else if(n < 0)
				return rsset(Failure, 0, text39, text137, n, 0);
						// "%s (%d) must be %d or greater", "Repeat count"

			// If only one window or repeat count equals window count, nothing to do.
			if(windCount == 1 || (n %= windCount) == 0)
				return sess.rtn.status;

			if(flags & SWB_Forw) {
				if((n += windNum) > windCount)
					n -= windNum;
				}
			else if((n = windNum - n) < 1)
				n += windCount;
			}
		else {
			if(n == 0 || abs(n) > windCount)
				return rsset(Failure, 0, text239, n);
						// "No such window '%d'"
			if(n < 0)				// Target window is nth from the bottom of the screen.
				n += windCount + 1;
			}

		// n is now the target window number.
		pWind = sess.windHead;					// Find the window...
		count = 0;
		while(++count != n)
			pWind = pWind->next;
		(void) wswitch(pWind, 0);				// and make new window current.
		supd_windFlags(NULL, WFMode);
		}
	return sess.rtn.status;
	}

// Switch to previous window n times.  Return status.
int prevWind(Datum *pRtnVal, int n, Datum **args) {

	return gotoWind(pRtnVal, n, SWB_Repeat);
	}

// Switch to next window n times.  Return status.
int nextWind(Datum *pRtnVal, int n, Datum **args) {

	return gotoWind(pRtnVal, n, SWB_Repeat | SWB_Forw);
	}

// Switch to given window N, or nth from bottom if N < 0.  Return status.
int selectWind(Datum *pRtnVal, int n, Datum **args) {

	// Get window number.
	if(sess.windHead->next == NULL && !(sess.opFlags & OpScript))
		return rsset(Failure, RSNoFormat, text294);
					// "Only one window"
	if(getNum(text113, 0, &n) == Success)
			// "Switch to"
		(void) gotoWind(pRtnVal, n, 0);

	return sess.rtn.status;
	}

// Check if given line is in given window and return Boolean result.
bool lineInWind(EWindow *pWind, Line *pLine) {
	Line *pLine1 = pWind->face.pTopLine;
	ushort i = 0;
	do {
		if(pLine1 == pLine)
			return true;
		if((pLine1 = pLine1->next) == NULL)
			break;
		} while(++i < pWind->rows);
	return false;
	}

// Return true if point is in current window, otherwise false.
bool pointInWind(void) {

	return lineInWind(sess.cur.pWind, sess.cur.pFace->point.pLine);
	}

// Move the current window up by "n" lines and compute the new top line of the window.
int moveWindUp(Datum *pRtnVal, int n, Datum **args) {

	// Nothing to do if buffer is empty.
	if(bempty(NULL))
		return sess.rtn.status;

	// Change top line.
	if(n == INT_MIN)
		n = 1;
	windNewTop(sess.cur.pWind, NULL, -n);

	// Is point still in the window?
	if(pointInWind())
		return sess.rtn.status;

	// Nope.  Move it to the center.
	Face *pFace = &sess.cur.pWind->face;
	Line *pLine = pFace->pTopLine;
	int i = sess.cur.pWind->rows / 2;
	while(i-- > 0 && pLine->next != NULL)
		pLine = pLine->next;
	pFace->point.pLine = pLine;
	pFace->point.offset = 0;

	return sess.rtn.status;
	}

// Make the current window the only window on the screen.  Try to set the framing so that point does not move on the screen.
int onlyWind(Datum *pRtnVal, int n, Datum **args) {
	EWindow *pWind;

	// If there is only one window, nothing to do.
	if(sess.windHead->next == NULL)
		return sess.rtn.status;

	// Nuke windows before current window.
	while(sess.windHead != sess.cur.pWind) {
		pWind = sess.windHead;
		sess.cur.pScrn->windHead = sess.windHead = pWind->next;
		--pWind->pBuf->windCount;
		windFaceToBufFace(pWind, pWind->pBuf);
		free((void *) pWind);
		}

	// Nuke windows after current window.
	while(sess.cur.pWind->next != NULL) {
		pWind = sess.cur.pWind->next;
		sess.cur.pWind->next = pWind->next;
		--pWind->pBuf->windCount;
		windFaceToBufFace(pWind, pWind->pBuf);
		free((void *) pWind);
		}

	// Adjust window parameters.
	windNewTop(sess.cur.pWind, NULL, -sess.cur.pWind->topRow);
	sess.cur.pWind->topRow = 0;
	sess.cur.pWind->rows = term.rows - 2;
	sess.cur.pWind->flags |= (WFHard | WFMode);

	return sess.rtn.status;
	}

// Delete the current window ("join" is false) or an adjacent window ("join" is true), and place its space in another window.
// If n argument, get one or more of the following options, which control the behavior of this function:
//	JoinDown	Place space from deleted window in window below it if possible, otherwise above it.
//	ForceJoin	Force join to target window, wrapping around if necessary.
//	DelBuf		Delete buffer in deleted window after join.
// Return status.
static int delJoinWind(int n, Datum **args, bool join) {
	EWindow *targWind;		// Window to receive deleted space.
	EWindow *pWind;			// Work pointer.
	Buffer *pOldBuf = sess.cur.pBuf;
	static bool joinDown, force, delBuf;
	static Option options[] = {
		{"^JoinDown", "^JDwn", 0, .u.ptr = (void *) &joinDown},
		{"^ForceJoin", "^Frc", 0, .u.ptr = (void *) &force},
		{"^DelBuf", "^Del", 0, .u.ptr = (void *) &delBuf},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text410, false, options};
			// "command option"

	// Get options and set flags.
	initBoolOpts(options);
	if(n != INT_MIN) {
		int count;

		if(parseOpts(&optHdr, text448, args[0], &count) != Success)
				// "Options"
			return sess.rtn.status;
		if(count > 0)
			setBoolOpts(options);
		}

	// If there is only one window, bail out.
	if(sess.windHead->next == NULL)
		return rsset(Failure, RSNoFormat, text294);
			// "Only one window"

	// Find receiving window and transfer lines.  Check for special "wrap around" case first (which only applies if
	// we have at least three windows).
	if(sess.windHead->next->next != NULL &&
	 ((sess.cur.pWind == sess.windHead && !joinDown && force) || (sess.cur.pWind->next == NULL && joinDown && force))) {
		int rows;

		// Current window is top or bottom and need to transfer lines to window at opposite end of screen.
		if((sess.cur.pWind == sess.windHead) != join) {
			if(join) {						// Switch to top window if join.
				if(wswitch(sess.windHead, 0) != Success)
					return sess.rtn.status;
				pOldBuf = sess.cur.pBuf;
				}
			targWind = windNextIs(NULL);				// Receiving window (bottom one).
			rows = -(sess.cur.pWind->rows + 1);			// Top row adjustment for all windows.
			sess.cur.pScrn->windHead = sess.windHead = sess.cur.pWind->next; // Remove current window from list.
			}
		else {
			if(join) {						// Switch to bottom window if join.
				if(wswitch(windNextIs(NULL), 0) != Success)
					return sess.rtn.status;
				pOldBuf = sess.cur.pBuf;
				}
			targWind = sess.windHead;
			rows = sess.cur.pWind->rows + 1;
			windNextIs(sess.cur.pWind)->next = NULL;
			windNewTop(targWind, NULL, -rows);
			}

		// Adjust top rows of other windows and set update flags.
		pWind = sess.windHead;
		do {
			pWind->topRow += rows;
			pWind->flags |= (WFHard | WFMode);
			} while((pWind = pWind->next) != NULL);
		sess.windHead->topRow = 0;

		// Adjust size of receiving window.
		targWind->rows += abs(rows);
		}
	else {
		// Set pWind to window before current one.
		pWind = windNextIs(sess.cur.pWind);
		if((pWind == NULL || (joinDown && sess.cur.pWind->next != NULL)) != join) {	// Next window down.
			if(join) {							// Switch upward one window if join.
				if(wswitch(pWind, 0) != Success)
					return sess.rtn.status;
				pWind = windNextIs(sess.cur.pWind);
				pOldBuf = sess.cur.pBuf;
				}
			targWind = sess.cur.pWind->next;
			targWind->topRow = sess.cur.pWind->topRow;
			if(pWind == NULL)
				sess.cur.pScrn->windHead = sess.windHead = targWind;
			else
				pWind->next = targWind;
			windNewTop(targWind, NULL, -(sess.cur.pWind->rows + 1));
			}
		else {									// Next window up.
			if(join) {							// Switch downward one window if join.
				pWind = sess.cur.pWind;
				if(wswitch(pWind->next, 0) != Success)
					return sess.rtn.status;
				pOldBuf = sess.cur.pBuf;
				}
			targWind = pWind;
			pWind->next = sess.cur.pWind->next;
			}
		targWind->rows += sess.cur.pWind->rows + 1;
		}

	// Get rid of the current window.
	--sess.cur.pBuf->windCount;
	windFaceToBufFace(sess.cur.pWind, sess.cur.pBuf);
	free((void *) sess.cur.pWind);

	(void) wswitch(targWind, 0);							// Can't fail.
	targWind->flags |= (WFHard | WFMode);

	// Delete old buffer if requested.
	if(delBuf) {
		char bufname[MaxBufname + 1];
		strcpy(bufname, pOldBuf->bufname);
		if(bdelete(pOldBuf, 0) == Success)
			(void) rsset(Success, RSHigh, text372, bufname);
					// "Buffer '%s' deleted"
		}

	return sess.rtn.status;
	}

// Delete the current window, placing its space in another window, as described in delJoinWind().  Return status.
int delWind(Datum *pRtnVal, int n, Datum **args) {

	return delJoinWind(n, args, false);
	}

// Join the current window with another window, as described in delJoinWind().  Return status.
int joinWind(Datum *pRtnVal, int n, Datum **args) {

	return delJoinWind(n, args, true);
	}

// Get a unique window id (a mark past the printable-character range for internal use) and return it in *pWindId.  Return
// status.
int getWindId(ushort *pWindId) {
	EScreen *pScrn;
	EWindow *pWind;
	uint id = '~';

	// If no screen yet exists, use the first window id.
	if((pScrn = sess.scrnHead) == NULL)
		++id;
	else {
		// Get count of all windows in all screens and add it to the maximum user mark value.
		do {
			// In all windows...
			pWind = pScrn->windHead;
			do {
				++id;
				} while((pWind = pWind->next) != NULL);
			} while((pScrn = pScrn->next) != NULL);

		// Scan windows again and find an id that is unique.
		for(;;) {
NextId:
			if(++id > USHRT_MAX)
				return rsset(Failure, 0, text356, id - '~');
					// "Too many windows (%u)"

			// In all screens...
			pScrn = sess.scrnHead;
			do {
				// In all windows...
				pWind = pScrn->windHead;
				do {
					if(id == pWind->id)
						goto NextId;
					} while((pWind = pWind->next) != NULL);
				} while((pScrn = pScrn->next) != NULL);

			// Success!
			break;
			}
		}

	// Unique id found.  Return it.
	*pWindId = id;
	return sess.rtn.status;
	}	

// Split the current window and return status.  The top or bottom line is dropped to make room for a new mode line, and the
// remaining lines are split into an upper and lower window.  A window smaller than three lines cannot be split.  The point
// remains in whichever window contains point after the split by default.  A line is pushed out of the other window and its
// point is moved to the center.  If n == 0, the point is forced to the opposite (non-default) window; if n < 0, the window is
// split just above the current line, and if n <= -2, the point is also moved to the other window; if n > 0, the upper window
// is set to n lines.  *ppWind is set to the new window not containing point.
int wsplit(int n, EWindow **ppWind) {
	EWindow *pWind;
	Line *pLine;
	ushort id;
	int upperRows, lowerRows, pointRow;
	bool pointInUpper;				// Point in upper window after split?
	Face *pFace = &sess.cur.pWind->face;

	// Make sure we have enough space and can obtain a unique id.  If so, create a new window.
	if(sess.cur.pWind->rows < 3)
		return rsset(Failure, 0, text293, sess.cur.pWind->rows);
			// "Cannot split a %d-line window"
	if(getWindId(&id) != Success)
		return sess.rtn.status;
	if((pWind = (EWindow *) malloc(sizeof(EWindow))) == NULL)
		return rsset(Panic, 0, text94, "wsplit");
			// "%s(): Out of memory!"

	// Find row containing point (which is assumed to be in the window).
	pointRow = 0;
	for(pLine = pFace->pTopLine; pLine != pFace->point.pLine; pLine = pLine->next)
		++pointRow;

	// Update some settings.
	++sess.cur.pBuf->windCount;				// Now displayed twice (or more).
	pWind->pBuf = sess.cur.pBuf;
	pWind->face = *pFace;					// For now.
	pWind->reframeRow = 0;
	pWind->flags = WFHard | WFMode;
	pWind->id = id;

	// Calculate new window sizes.
	upperRows = (sess.cur.pWind->rows - 1) / 2;		// Upper window (default).
	if(n != INT_MIN) {
		if(n < 0)
			upperRows = pointRow <= 1 ? 1 : pointRow - 1;
		else if(n > 0)
			upperRows = n < sess.cur.pWind->rows - 1 ? n : sess.cur.pWind->rows - 2;
		}
	lowerRows = (sess.cur.pWind->rows - 1) - upperRows;	// Lower window.

	// Make new window the bottom one.
	pWind->next = sess.cur.pWind->next;
	sess.cur.pWind->next = pWind;
	sess.cur.pWind->rows = upperRows;
	pWind->rows = lowerRows;
	pWind->topRow = sess.cur.pWind->topRow + upperRows + 1;

	// Adjust current window's top line if needed.
	if(pointRow > upperRows)
		pFace->pTopLine = pFace->pTopLine->next;

	// Move down upperRows lines to find top line of lower window.  Stop if we slam into end-of-buffer.
	if(pointRow != upperRows) {
		pLine = pFace->pTopLine;
		while(pLine->next != NULL) {
			pLine = pLine->next;
			if(--upperRows == 0)
				break;
			}
		}

	// Set top line and point line of each window as needed, keeping in mind that buffer may be empty (so top and point
	// point to the first line) or have just a few lines in it.  In the latter case, set top in the bottom window to the
	// last line of the buffer and point to same line, except for special case described below.
	if(pointRow < sess.cur.pWind->rows) {

		// Point is in old (upper) window.  Fixup new (lower) window.
		pointInUpper = true;

		// Hit end of buffer looking for top?
		if(pLine->next == NULL) {

			// Yes, lines in window being split do not extend past the middle.
			pWind->face.pTopLine = pLine->prev;

			// Set point to last line (unless it is already there) so that it will be visible in the lower window.
			if(pFace->point.pLine->next != NULL) {
				pWind->face.point.pLine = sess.cur.pBuf->pFirstLine->prev;
				pWind->face.point.offset = 0;
				}
			}
		else {
			// No, save current line as top and press onward to find spot to place point.
			pWind->face.pTopLine = pLine;
			upperRows = lowerRows / 2;
			while(upperRows-- > 0) {
				if((pLine = pLine->next)->next == NULL)
					break;
				}
	
			// Set point line to mid-point of lower window or last line of buffer.
			pWind->face.point.pLine = pLine;
			pWind->face.point.offset = 0;
			}
		}
	else {
		// Point is in new (lower) window.  Fixup both windows.
		pointInUpper = false;

		// Set top line of lower window (point is already correct).
		pWind->face.pTopLine = pLine;

		// Set point in upper window to middle.
		upperRows = sess.cur.pWind->rows / 2;
		pLine = pFace->pTopLine;
		while(upperRows-- > 0)
			pLine = pLine->next;
		pFace->point.pLine = pLine;
		pFace->point.offset = 0;
		}

	// Both windows are now set up.  All that's left is to set the window-update flags, set the current window to the bottom
	// one if needed, and return the non-point window pointer.
	sess.cur.pWind->flags |= (WFHard | WFMode);
	if((n == 0 && pointInUpper) || ((n == INT_MIN || n == -1 || n > 0) && !pointInUpper)) {
		sess.cur.pScrn->pCurWind = pWind;
		pWind = sess.cur.pWind;
		sess.edit.pFace = sess.cur.pFace = &(sess.edit.pWind = sess.cur.pWind = sess.cur.pScrn->pCurWind)->face;
		}
	*ppWind = pWind;

	return sess.rtn.status;
	}

// Grow or shrink the current window.  If how == 0, set window size to abs(n) lines; otherwise, shrink (how < 0) or grow
// (how > 0) by abs(n) lines.  Find the window that loses or gains space and make sure the window that shrinks is big enough.
// If n < 0, try to use upper window, otherwise lower.  If it's a go, set the window flags and let the redisplay system do all
// the hard work.  (Can't just set "force reframe" because point would move.)  Return status.
int chgWindSize(Datum *pRtnVal, int n, int how) {
	EWindow *adjWind;
	bool grow = (how > 0);

	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		return sess.rtn.status;		// Nothing to do.

	if(sess.windHead->next == NULL)
		return rsset(Failure, RSNoFormat, text294);
			// "Only one window"

	// Figure out which window (next or previous) to steal lines from or give lines to.
	adjWind = sess.cur.pWind->next;
	if(sess.cur.pWind != sess.windHead && (n < 0 || adjWind == NULL))
		adjWind = windNextIs(sess.cur.pWind);

	if(n < 0)
		n = -n;
	if(how == 0) {

		// Want n-line window.  Convert n to a size adjustment.
		if(n > sess.cur.pWind->rows) {
			n -= sess.cur.pWind->rows;
			grow = true;
			}
		else if(n == sess.cur.pWind->rows)
			return sess.rtn.status;	// Nothing to do.
		else {
			n = sess.cur.pWind->rows - n;
			grow = false;
			}
		}

	if(grow) {
		// Adjacent window big enough?
		if(adjWind->rows <= n)
			return rsset(Failure, 0, text207, n, n == 1 ? "" : "s");
				// "Cannot get %d line%s from adjacent window"

		// Yes, proceed.
		if(sess.cur.pWind->next == adjWind) {		// Shrink below.
			windNewTop(adjWind, NULL, n);
			adjWind->topRow += n;
			}
		else {					// Shrink above.
			windNewTop(sess.cur.pWind, NULL, -n);
			sess.cur.pWind->topRow -= n;
			}
		sess.cur.pWind->rows += n;
		adjWind->rows -= n;
		}
	else {
		// Current window big enough?
		if(sess.cur.pWind->rows <= n)
			return rsset(Failure, 0, text93, n, n == 1 ? "" : "s");
				// "Current window too small to shrink by %d line%s"

		// Yes, proceed.
		if(sess.cur.pWind->next == adjWind) {		// Grow below.
			windNewTop(adjWind, NULL, -n);
			adjWind->topRow -= n;
			}
		else {					// Grow above.
			windNewTop(sess.cur.pWind, NULL, n);
			sess.cur.pWind->topRow += n;
			}
		sess.cur.pWind->rows -= n;
		adjWind->rows += n;
		}

	sess.cur.pWind->flags |= (WFHard | WFMode);
	adjWind->flags |= (WFHard | WFMode);

	return sess.rtn.status;
	}

// Resize the current window to the requested size.
int resizeWind(Datum *pRtnVal, int n, Datum **args) {

	// Ignore if no argument or already requested size.
	if(n == INT_MIN || n == sess.cur.pWind->rows)
		return sess.rtn.status;

	// Error if only one window.
	if(sess.windHead->next == NULL)
		return rsset(Failure, RSNoFormat, text294);
			// "Only one window"

	// Set all windows on current screen to same size if n == 0; otherwise, change current window only.
	if(n == 0) {
		int count = getWindCount(sess.cur.pScrn, NULL);
		int size = (term.rows - 1) / count - 1;	// Minimum size of each window, excluding mode line.
		int leftover0 = (term.rows - 1) % count;

		// Remember current window, then step through all windows repeatedly, setting new size of each if possible,
		// until all windows have been set successfully or become deadlocked (which can happen if there are numerous
		// tiny windows).  Give up in the latter case and call it good (a very rare occurrence).
		EWindow *pOldWind = sess.cur.pWind;
		EWindow *pWind;
		int leftover;
		bool success, changed;
		do {
			pWind = sess.windHead;
			leftover = leftover0;
			success = true;
			changed = false;
			do {
				(void) wswitch(pWind, SWB_NoBufHooks);		// Can't fail.
				if(leftover > 0) {
					n = size + 1;
					--leftover;
					}
				else
					n = size;

				// Don't try to adjust bottom window.
				if(pWind->next != NULL && pWind->rows != n) {
					if(pWind->rows < n && pWind->next->rows <= n - pWind->rows) {

						// Current window needs to grow but next window is too small.  Take as many
						// rows as possible.
						success = false;
						if(pWind->next->rows == 1)
							continue;
						n = pWind->rows + pWind->next->rows - 1;
						}
					if(chgWindSize(pRtnVal, n, 0) != Success)
						break;
					changed = true;
					}
				} while((pWind = pWind->next) != NULL);
			} while(!success && changed);

		// Adjustment loop completed... success or deadlocked.  Switch back to original window in either case.
		(void) wswitch(pOldWind, SWB_NoBufHooks);
		}
	else
		(void) chgWindSize(pRtnVal, n, 0);

	return sess.rtn.status;
	}

// Determine the disposition of a buffer.  This routine is called by any command that creates or selects a buffer.  Once the
// command has the buffer (which may have just been created), it hands it off to this routine to figure out what to do with it.
// The action taken is determined by the value of "n" (which defaults to -1) and "flags".  Possible values of "n" are:
//
//	< -2		Display buffer in a different window (possibly new) and switch to that window.
//	-2		Display buffer in a different window (possibly new), but stay in current window.
//	-1		Pop buffer via bpop() with given flags + RendWait.
//	0		Leave buffer as is.
//	1		Switch to buffer in current window.
//	2		Display buffer in a new window, but stay in current window.
//	> 2		Display buffer in a new window and switch to that window.
//
// Flags are:
//	RendNewBuf	Buffer was just created.
//	RendRewind	Move point to beginning of buffer and unhide it if displaying in another window ("show" command).
//	RendAltML	Display the alternate mode line when doing a real pop-up.
//	RendShift	Shift long lines left when doing a real pop-up.
//
// Possible return values are listed below.  The new-buf? value is true if RendNewBuf flag set, otherwise false:
//	n == -1:		bufname (or nil if buffer deleted)
//	n == 0:			[bufname, new-buf?]
//	Other n:		[bufname, new-buf?, targ-wind-num, new-wind?]
//
// Notes:
//	* A buffer may be in the background or displayed in a window when this routine is called.  In either case, it is left as
//	  is if n == 0.
//	* If n < -1:
//		- The buffer will be displayed in another window even if it is displayed in the current window.
//		- If the buffer is already being displayed in another window, that window will be used as the target window.
//		- If the buffer is not being displayed, the window immediately above the current window is the first choice.
int render(Datum *pRtnVal, int n, Buffer *pBuf, ushort flags) {
	EWindow *pWind = NULL;
	bool newWind = false;

	if(n == INT_MIN)
		n = -1;

	// Displaying buffer?
	if(n != 0) {
		// Yes.  Popping buffer?
		if(n == -1) {
			// Yes.  Activate buffer if needed and do a real pop-up.
			if(bactivate(pBuf) != Success || bpop(pBuf, flags | RendWait) != Success)
				return sess.rtn.status;
			}

		// Not popping buffer.  Switch to it?
		else if(n == 1) {
			if(pBuf != sess.cur.pBuf && bswitch(pBuf, 0) != Success)
				return sess.rtn.status;
			pWind = sess.cur.pWind;
			}

		// No, displaying buffer in another window.
		else {
			EWindow *pOldWind;
			ushort flags;

			if(n > 1 || sess.windHead->next == NULL) {	// If force-creating or only one window...
				if(wsplit(INT_MIN, &pWind) != Success)	// split current window and get new one into pWind.
					return sess.rtn.status;
				newWind = true;
				}
			else if((pWind = windDispBuf(pBuf, true)) != NULL) {	// Otherwise, find a different window, giving
				if(n == -2) {				// preference to one already displaying the buffer...
					(void) rsset(Success, 0, text28, text58);
						// "%s is being displayed", "Buffer"
					goto Retn;
					}
				}
			else if((pWind = windNextIs(sess.cur.pWind)) == NULL)	// or the window above, if none found.
				pWind = sess.cur.pWind->next;
			pOldWind = sess.cur.pWind;			// save old window...
			flags = (abs(n) == 2) ? SWB_NoBufHooks : 0;	// set wswitch() flag...
			if(wswitch(pWind, flags) != Success ||		// make new one current...
			 bswitch(pBuf, flags) != Success)		// and switch to new buffer.
				return sess.rtn.status;
			if(flags & RendRewind)
				faceInit(&pWind->face, pBuf->pFirstLine, NULL);
			if(flags != 0)					// If not a force to new window...
				(void) wswitch(pOldWind, flags);	// switch back to previous one (can't fail).
			}
		}
Retn:
	// Wrap up and set return value.
	if(n == -1) {
		if(flags & RendNewBuf) {
			if(bdelete(pBuf, 0) != Success)
				return sess.rtn.status;
			pBuf = NULL;
			dsetnil(pRtnVal);
			}
		else {
			if(dsetstr(pBuf->bufname, pRtnVal) != 0)
				goto LibFail;
			}
		}
	else {
		Array *pArray;

		if((pArray = anew(n == 0 ? 2 : 4, NULL)) == NULL || dsetstr(pBuf->bufname, pArray->elements[0]) != 0)
			goto LibFail;
		else {
			dsetbool(flags & RendNewBuf, pArray->elements[1]);
			if(n != 0) {
				if(pWind == NULL)
					dsetnil(pArray->elements[2]);
				else
					dsetint((long) getWindNum(pWind), pArray->elements[2]);
				dsetbool(newWind, pArray->elements[3]);
				}
			agStash(pRtnVal, pArray);
			}

		if((flags & RendNotify) || (n == 0 && (flags & RendNewBuf)))
			(void) rsset(Success, RSHigh, text381, pBuf->bufname);
				// "Buffer '%s' created"
		}
	if(pBuf != NULL && (flags & RendRewind))
		pBuf->flags &= ~BFHidden;
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Scroll the previous or next window up (backward) or down (forward) a page.
int wscroll(Datum *pRtnVal, int n, int (*windFunc)(Datum *pRtnVal, int n, Datum **args), int (*pageFunc)(Datum *pRtnVal, int n,
 Datum **args)) {

	(void) windFunc(pRtnVal, INT_MIN, NULL);
	(void) pageFunc(pRtnVal, n, NULL);
	(void) (*(windFunc == prevWind ? nextWind : prevWind))(pRtnVal, INT_MIN, NULL);

	return sess.rtn.status;
	}

#if MMDebug & (Debug_ScrnDump | Debug_Narrow)
// Return line information.
static char *lineInfo(const char *label, Line *pLine) {
	static char workBuf[96];

	if(pLine == NULL)
		sprintf(workBuf, "%s [00000000]: NULL", label);
	else if(pLine->used <= 20)
		sprintf(workBuf, "%s [%.08X]: '%.*s'", label, (uint) pLine, pLine->used, pLine->text);
	else
		sprintf(workBuf, "%s [%.08X]: '%.20s...'", label, (uint) pLine, pLine->text);
	return workBuf;
	}

// Write buffer information to log file.
void dumpBuffer(const char *label, Buffer *pBuf, bool withData) {
	Mark *pMark;
	char workBuf[80];
	static char s[] = "%s\n";

	if(pBuf == NULL)
		pBuf = sess.cur.pBuf;
	if(label != NULL)
		fprintf(logfile, "*** In %s...\n", label);
	fprintf(logfile, "Buffer '%s' [%.08X]:\n", pBuf->bufname, (uint) pBuf);
	fprintf(logfile, s, lineInfo("\tb_lnp", pBuf->pFirstLine));
	fprintf(logfile, s, lineInfo("\tb_lnp->prev", pBuf->pFirstLine->prev));
	fprintf(logfile, s, lineInfo("\tb_lnp->prev->next", pBuf->pFirstLine->prev->next));
	fprintf(logfile, s, lineInfo("\tb_ntoplnp", pBuf->pNarTopLine));
		if(pBuf->pNarTopLine != NULL)
			fprintf(logfile, s, lineInfo("\t\tb_ntoplnp->prev", pBuf->pNarTopLine->prev));
	fprintf(logfile, s, lineInfo("\tb_nbotlnp", pBuf->pNarBotLine));
		if(pBuf->pNarBotLine != NULL)
			fprintf(logfile, s, lineInfo("\t\tb_nbotlnp->prev", pBuf->pNarBotLine->prev));

	fprintf(logfile, s, lineInfo("\tb_face.pTopLine", pBuf->face.pTopLine));
	fprintf(logfile, "\t%s\n\tb_face.point.offset: %d\n",
	 lineInfo("face.point.pLine", pBuf->face.point.pLine), pBuf->face.point.offset);
	fputs("\tMarks:\n", logfile);
	pMark = &pBuf->markHdr;
	do {
		if(pMark->id <= '~')
			sprintf(workBuf, "%c", (int) pMark->id);
		else
			sprintf(workBuf, "%.4hx", pMark->id);
		fprintf(logfile, "\t\t'%s': (%s), point.offset %d, reframeRow %hd\n",
		 workBuf, lineInfo("point.pLine", pMark->point.pLine), pMark->point.offset, pMark->reframeRow);
		} while((pMark = pMark->next) != NULL);

	if(pBuf->pCallInfo == NULL)
		strcpy(workBuf, "NULL");
	else
		sprintf(workBuf, "%hu", pBuf->pCallInfo->execCount);
	fprintf(logfile,
	 "\tb_face.firstCol: %d\n\tb_nwind: %hu\n\tb_nexec: %s\n\tb_nalias: %hu\n\tb_flags: %.4x\n\tb_modes:",
	 pBuf->face.firstCol, pBuf->windCount, workBuf, pBuf->aliasCount, (uint) pBuf->flags);
	if(pBuf->modes == NULL)
		fputs(" NONE", logfile);
	else {
		BufMode *pBufMode = pBuf->modes;
		do
			fprintf(logfile, " %s", pBufMode->pModeSpec->name);
			while((pBufMode = pBufMode->next) != NULL);
		}
	if(pBuf->filename == NULL)
		strcpy(workBuf, "NULL");
	else
		sprintf(workBuf, "'%s'", pBuf->filename);
	fprintf(logfile, "\n\tinpDelim.u.delim: %.2hx %.2hx (%d)\n\tfilename: %s\n",
	 (ushort) pBuf->inpDelim.u.delim[0], (ushort) pBuf->inpDelim.u.delim[1], pBuf->inpDelim.len, workBuf);

	if(withData) {
		Line *pLine = pBuf->pFirstLine;
		uint n = 0;
		fputs("\tData:\n", logfile);
		do {
			sprintf(workBuf, "\t\tL%.4u", ++n);
			fprintf(logfile, s, lineInfo(workBuf, pLine));
			} while((pLine = pLine->next) != NULL);
		}
	}
#endif

#if MMDebug & Debug_ScrnDump
// Write window information to log file.
static void dumpWindow(EWindow *pWind, int windNum) {

	fprintf(logfile, "\tWindow %d [%.08x]:\n\t\tw_next: %.08x\n\t\tw_buf: %.08x '%s'\n%s\n",
	 windNum, (uint) pWind, (uint) pWind->next, (uint) pWind->pBuf, pWind->pBuf->bufname,
	 lineInfo("\t\tw_face.pTopLine", pWind->face.pTopLine));
	fprintf(logfile, "%s\n\t\tw_face.point.offset: %d\n\t\tw_toprow: %hu\n\t\tw_nrows: %hu\n\t\tw_rfrow: %hu\n",
	 lineInfo("\t\tw_face.point.pLine", pWind->face.point.pLine),
	 pWind->face.point.offset, pWind->topRow, pWind->rows, pWind->reframeRow);
	fprintf(logfile, "\t\tw_flags: %.04x\n\t\tw_face.firstCol: %d\n",
	 (uint) pWind->flags, pWind->face.firstCol);
	}

// Write screen, window, and buffer information to log file -- for debugging.
void dumpScreens(const char *msg) {
	EScreen *pScrn;
	EWindow *pWind;
	Array *pArray;
	Datum *pArrayEl;
	int windNum;

	fprintf(logfile, "### %s ###\n\n*SCREENS\n\n", msg);

	// Dump screens and windows.
	pScrn = sess.scrnHead;
	do {
		fprintf(logfile, "Screen %hu [%.08x]:\n\ts_flags: %.04x\n\ts_nrow: %hu\n\ts_ncol: %hu\n\ts_curwin: %.08x\n",
		 pScrn->num, (uint) pScrn, (uint) pScrn->flags, pScrn->rows, pScrn->cols, (uint) pScrn->pCurWind);
		pWind = pScrn->windHead;
		windNum = 0;
		do {
			dumpWindow(pWind, ++windNum);
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	// Dump buffers.
	fputs("\n*BufferS\n\n", logfile);
	pArray = &bufTable;
	while((pArrayEl = aeach(&pArray)) != NULL)
		dumpBuffer(NULL, bufPtr(pArrayEl), false);
	}
#endif

// Get number of screens.
int scrnCount(void) {
	EScreen *pScrn = sess.scrnHead;
	int count = 0;
	do {
		++count;
		} while((pScrn = pScrn->next) != NULL);
	return count;
	}

// Find a screen, given number, (possibly NULL) pointer to buffer to attach to first window of screen, and (possibly NULL)
// pointer to result.  If the screen is not found and "pBuf" is not NULL, create new screen and return status; otherwise, return
// false, ignoring ppScrn.
int sfind(ushort scrNum, Buffer *pBuf, EScreen **ppScrn) {
	EScreen *pScrn1;
	ushort scrNum1;
	static const char myName[] = "sfind";

	// Scan the screen list.  Note that the screen list is empty at program launch.
	for(scrNum1 = 0, pScrn1 = sess.scrnHead; pScrn1 != NULL; pScrn1 = pScrn1->next) {
		if((scrNum1 = pScrn1->num) == scrNum) {
			if(ppScrn)
				*ppScrn = pScrn1;
			return (pBuf == NULL) ? true : sess.rtn.status;
			}
		}

	// No such screen exists, create one?
	if(pBuf != NULL) {
		ushort id;
		EScreen *pScrn2;
		EWindow *pWind;				// Pointer to first window of new screen.

		// Get unique window id.
		if(getWindId(&id) != Success)
			return sess.rtn.status;

		// Allocate memory for screen.
		if((pScrn1 = (EScreen *) malloc(sizeof(EScreen))) == NULL)
			return rsset(Panic, 0, text94, myName);
				// "%s(): Out of memory!"

		// Set up screen fields.
		pScrn1->pLastBuf = NULL;
		pScrn1->num = scrNum1 + 1;
		pScrn1->flags = 0;
		pScrn1->workDir = NULL;
		if(setWorkDir(pScrn1) != Success)
			return sess.rtn.status;		// Fatal error.
		pScrn1->rows = term.rows;
		pScrn1->wrapCol = (pScrn1->cols = term.cols) - 4;
		pScrn1->prevWrapCol = -1;
		pScrn1->hardTabSize = 8;
		pScrn1->softTabSize = pScrn1->cursorRow = pScrn1->cursorCol = pScrn1->firstCol = 0;

		// Allocate its first window...
		if((pWind = (EWindow *) malloc(sizeof(EWindow))) == NULL)
			return rsset(Panic, 0, text94, myName);
				// "%s(): Out of memory!"
		pScrn1->windHead = pScrn1->pCurWind = pWind;

		// and set up the window's info.
		pWind->next = NULL;
		pWind->pBuf = pBuf;
		++pBuf->windCount;
		bufFaceToWindFace(pBuf, pWind);
		pWind->id = id;
		pWind->topRow = 0;
		pWind->rows = term.rows - 2;		// "-2" for message and mode lines.
		pWind->reframeRow = 0;

		// Insert new screen at end of screen list.
		pScrn1->next = NULL;
		if((pScrn2 = sess.scrnHead) == NULL)
			sess.scrnHead = pScrn1;
		else {
			while(pScrn2->next != NULL)
				pScrn2 = pScrn2->next;
			pScrn2->next = pScrn1;
			}

		// and return the new screen pointer.
		if(ppScrn)
			*ppScrn = pScrn1;
		return sess.rtn.status;
		}

	// Screen not found and pBuf is NULL.
	return false;
	}

// Switch to given screen.  Return status.
int sswitch(EScreen *pScrn, ushort flags) {

	// Nothing to do if requested screen is already current.
	if(pScrn == sess.cur.pScrn)
		return sess.rtn.status;

	// Save the current screen's concept of current window.
	sess.cur.pScrn->pCurWind = sess.cur.pWind;
	sess.cur.pScrn->rows = term.rows;
	sess.cur.pScrn->cols = term.cols;

	// Run exit-buffer user hook on current (old) buffer if the new screen's buffer is different.
	Datum *pRtnVal = NULL;
	bool differentBuf = false;
	if(!(flags & SWB_NoBufHooks)) {
		differentBuf = (pScrn->pCurWind->pBuf != sess.cur.pBuf);
		if(differentBuf && runBufHook(&pRtnVal, SWB_ExitHook) != Success)
			return sess.rtn.status;
		}

	// Change current directory if needed.
	if(pScrn->workDir != sess.cur.pScrn->workDir)
		(void) chgdir(pScrn->workDir);

	// Change the current screen, window and buffer.
	sess.windHead = (sess.edit.pScrn = sess.cur.pScrn = pScrn)->windHead;
	sess.edit.pBuf = sess.cur.pBuf = (sess.edit.pWind = sess.cur.pWind = pScrn->pCurWind)->pBuf;
	sess.edit.pFace = sess.cur.pFace = &pScrn->pCurWind->face;

	// Let the display driver know we need a full screen update.
	sess.opFlags |= OpScrnRedraw;

	// Run enter-buffer user hook on current (new) buffer.
	if(differentBuf && sess.rtn.status == Success)
		(void) runBufHook(&pRtnVal, 0);

	return sess.rtn.status;
	}

// Bring a screen to the front per flags.  Return status.
int gotoScreen(int n, ushort flags) {

	if(n != 0 || !(flags & SWB_Repeat)) {
		EScreen *pScrn;
		int scrTotal = scrnCount();		// Total number of screens.
		const char *oldDir;

		// Check if n is out of range.
		if(flags & SWB_Repeat) {
			if(n == INT_MIN)
				n = 1;
			else if(n < 0)
				return rsset(Failure, 0, text39, text137, n, 0);
						// "%s (%d) must be %d or greater", "Repeat count"

			if(scrTotal == 1 || (n %= scrTotal) == 0)	// If only one screen or repeat count == screen count...
				return sess.rtn.status;			// nothing to do.
			if(flags & SWB_Forw) {
				if((n += sess.cur.pScrn->num) > scrTotal)
					n -= sess.cur.pScrn->num;
				}
			else if((n = sess.cur.pScrn->num - n) < 1)
				n += scrTotal;
			}
		else if(n <= 0 || abs(n) > scrTotal)
			return rsset(Failure, 0, text240, n);
					// "No such screen '%d'"

		// n is now the target screen number.
		pScrn = sess.scrnHead;					// Find the screen...
		while(pScrn->num != n)
			pScrn = pScrn->next;
		oldDir = sess.cur.pScrn->workDir;			// save current directory...
		if(sswitch(pScrn, 0) == Success)			// make new screen current...

			// and display its working directory if 'WkDir' global mode not enabled and directory changed.
			if(!(modeInfo.cache[MdIdxWkDir]->flags & MdEnabled) && sess.cur.pScrn->workDir != oldDir)
				rsset(Success, RSHigh | RSNoWrap, text409, pScrn->workDir);
								// "CWD: %s"
		}
	return sess.rtn.status;
	}

// Set user-customizable default values in a new screen and return status.
static int setdef(void) {
	int *pParam = &sess.cur.pScrn->hardTabSize;
	int *pDef = defParams;

	// Set hard tab size;
	if(*pDef != -1) {
		if(checkTabSize(*pDef, true) == Success)
			*pParam = *pDef;
		else {
			*pDef = -1;
			goto Retn;
			}
		}
	++pDef; ++pParam;

	// Set soft tab size;
	if(*pDef != -1) {
		if(checkTabSize(*pDef, false) == Success)
			*pParam = *pDef;
		else {
			*pDef = -1;
			goto Retn;
			}
		}
	++pDef; ++pParam;

	// Set wrap column.
	if(*pDef != -1) {
		if(checkWrapCol(*pDef) == Success)
			*pParam = *pDef;
		else
			*pDef = -1;
		}
Retn:
	return sess.rtn.status;
	}

// Switch to given screen, or create one if screen number is zero.  Return status.
int selectScreen(Datum *pRtnVal, int n, Datum **args) {

	// Get screen number.
	if(getNum(text113, 1, &n) != Success)
			// "Switch to"
		return sess.rtn.status;

	// Create screen?
	if(n == 0) {
		EScreen *pScrn;

		// Yes.  Save current screen number and current window's settings.
		windFaceToBufFace(sess.cur.pWind, sess.cur.pBuf);

		// Find screen "0" to force-create one and make it current.
		if(sfind(0, sess.cur.pBuf, &pScrn) != Success || sswitch(pScrn, 0) != Success)
			return sess.rtn.status;
		if(setdef() == Success)
			(void) rsset(Success, 0, text174, pScrn->num);
					// "Created screen %hu"
		}
	else
		// Not creating... switch to one specified.
		(void) gotoScreen(n, 0);

	return sess.rtn.status;
	}

// Free all resouces associated with a screen.
static void freeScrn(EScreen *pScrn) {
	EWindow *pWind, *pWindTemp;

	// First, free the screen's windows...
	pWind = pScrn->windHead;
	do {
		--pWind->pBuf->windCount;
		windFaceToBufFace(pWind, pWind->pBuf);

		// On to the next window; free this one.
		pWindTemp = pWind;
		pWind = pWind->next;
		free((void *) pWindTemp);
		} while(pWind != NULL);

	// then free the screen itself.
	free((void *) pScrn);
	}

// Remove screen from the list, renumber remaining ones, and update mode line of bottom window.  Return status.
static int unlistScrn(EScreen *pScrn) {
	ushort scrNum0, scrNum;
	EScreen *pScreenTemp;

	scrNum0 = sess.cur.pScrn->num;
	if(pScrn == sess.scrnHead)
		sess.scrnHead = sess.scrnHead->next;
	else {
		pScreenTemp = sess.scrnHead;
		do {
			if(pScreenTemp->next == pScrn)
				goto Found;
			} while((pScreenTemp = pScreenTemp->next) != NULL);

		// Huh?  Screen not found... this is a bug.
		return rsset(FatalError, 0, text177, "unlistScrn", (int) pScrn->num);
				// "%s(): Screen number %d not found in screen list!"
Found:
		pScreenTemp->next = pScrn->next;
		}

	// Renumber screens.
	scrNum = 0;
	pScreenTemp = sess.scrnHead;
	do {
		pScreenTemp->num = ++scrNum;
		} while((pScreenTemp = pScreenTemp->next) != NULL);

	// Flag mode line at bottom of screen (last window) for an update.
	windNextIs(NULL)->flags |= WFMode;

	return sess.rtn.status;
	}

// Delete a screen.  Return status.
int delScreen(Datum *pRtnVal, int n, Datum **args) {
	EScreen *pScrn;
	int screenNum;
	const char *workDir;
	int count;

	// Get the number of the screen to delete.  Error if only one exists.
	if(sess.scrnHead->next == NULL)
		return rsset(Failure, RSNoFormat, text57);
					// "Only one screen"
	if(getNum(text26, -1, &screenNum) != Success)
			// "Delete"
		return sess.rtn.status;

	// Make sure it exists.
	if(!sfind((ushort) screenNum, NULL, &pScrn))
		return rsset(Failure, 0, text240, screenNum);
			// "No such screen '%d'"

	// Grab its working directory.
	workDir = pScrn->workDir;

	// Switch screens if deleting current one.
	if(pScrn == sess.cur.pScrn && sswitch(pScrn == sess.scrnHead ? pScrn->next : sess.scrnHead, 0) != Success)
		return sess.rtn.status;

	// Everything's cool... nuke it.
	if(unlistScrn(pScrn) != Success)
		return sess.rtn.status;
	freeScrn(pScrn);

	// Also delete buffers homed to nuked screen, if requested.
	if(n < 0 && n != INT_MIN)
		return bnuke(BD_Homed, workDir, &count) != Success ? sess.rtn.status :
		 rsset(Success, RSHigh, text446, screenNum, count, count == 1 ? "" : "s");
			// "Screen %d and %d homed buffer%s deleted"
	else
		return rsset(Success, RSHigh, text178, screenNum);
				// "Screen %d deleted"
	}

// Check if terminal supports color.  If yes, return true; otherwise, set an error and return false.
bool haveColor(void) {

	if(sess.opFlags & OpHaveColor)
		return true;
	(void) rsset(Failure, RSNoFormat, text427);
		// "Terminal does not support color"
	return false;
	}

// Assign a foreground and background color to a color pair number.  If pair number is zero, use next one available.  Set
// pRtnVal to pair number assigned and return status.
int setColorPair(Datum *pRtnVal, int n, Datum **args) {

	if(!haveColor())
		return sess.rtn.status;

	short pair, colors[2], *colorPair = colors, *colorPairEnd = colors + 2;
	long c;

	// Get arguments and validate them.
	if((c = (*args++)->u.intNum) != 0)
		if(c < 1 || c > term.maxWorkPair)
			return rsset(Failure, 0, text425, c, term.maxWorkPair);
				// "Color pair %ld must be between 0 and %hd"
	pair = c;
	do {
		if((c = (*args++)->u.intNum) < -1 || c > term.maxColor)
			return rsset(Failure, 0, text426, c, term.maxColor);
				// "Color no. %ld must be between -1 and %hd"
		*colorPair++ = c;
		} while(colorPair < colorPairEnd);

	// Have arguments.  Get next pair number if needed.
	if(pair == 0) {
		pair = term.nextPair++;
		if(term.nextPair > term.maxWorkPair)
			term.nextPair = 1;
		}

	// Create color pair and return it.
	(void) init_pair(pair, colors[0], colors[1]);
	dsetint(pair, pRtnVal);
	return sess.rtn.status;
	}

// Set color for a display item.
int setDispColor(Datum *pRtnVal, int n, Datum **args) {
	ItemColor *pItemColor, *pItemColorEnd;
	short colors[2];
	bool changed = false;

	// Check keyword (first argument).
	pItemColorEnd = (pItemColor = term.itemColors) + elementsof(term.itemColors);
	do {
		if(strcasecmp((*args)->str, pItemColor->name) == 0)
			goto Found;
		} while(++pItemColor < pItemColorEnd);

	// Match not found.
	return rsset(Failure, 0, text447, text443, (*args)->str);
			// "Invalid %s '%s'", "display item"
Found:
	// Match found.  Check color numbers.
	if(disnil(args[1])) {
		if(pItemColor->colors[0] != -2) {
			pItemColor->colors[0] = pItemColor->colors[1] = -2;
			changed = true;
			}
		}
	else if(!haveColor())
		return sess.rtn.status;
	else {
		Datum **ppArrayEl, **ppArrayElEnd, *pArrayEl;
		long c;
		short *colorPair = colors;
		Array *pArray = args[1]->u.pArray;

		if(pArray->used != 2)
			goto Err;
		ppArrayElEnd = (ppArrayEl = pArray->elements) + 2;
		do {
			pArrayEl = *ppArrayEl++;
			if(pArrayEl->type != dat_int || (c = pArrayEl->u.intNum) < -1 || c > term.maxColor)
				goto Err;
			*colorPair++ = c;
			} while(ppArrayEl < ppArrayElEnd);
		if(colors[0] != pItemColor->colors[0] || colors[1] != pItemColor->colors[1]) {
			pItemColor->colors[0] = colors[0];
			pItemColor->colors[1] = colors[1];
			changed = true;
			}
		}

	// Success.  If a change was made, implement it.
	if(changed)
		switch(pItemColor - term.itemColors) {
			case ColorIdxModeLn:
				supd_windFlags(NULL, WFMode);

				// Last color pair is reserved for mode line.
				if(colors[0] >= -1)
					(void) init_pair(term.maxPair - ColorPairML, colors[0], colors[1]);
				break;
			case ColorIdxMRI:

				// Second to last color pair is reserved for macro recording indicator.
				if(colors[0] >= -1)
					(void) init_pair(term.maxPair - ColorPairMRI, colors[0], colors[1]);
			}
	return sess.rtn.status;
Err:
	return rsset(Failure, RSNoFormat, text422);
		// "Invalid color pair value(s)"
	}
