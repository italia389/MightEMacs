// (c) Copyright 2026 Richard W. Marinelli
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
		if(pWind->pBuf == pBuf && (pWind != sess.pCurWind || !skipCur))
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
int mlerase(uint flags) {

	// Is anything on message line and interactive or a force?
	if(term.msgLineCol != 0 && (interactive() || flags & MLForce)) {

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
void mlputc(short c, uint flags) {

	// Nothing to do if past right edge of screen.
	if(term.msgLineCol >= term.cols)
		return;

	// Raw or plain character?
	if(flags & MLRaw || (c >= ' ' && c <= '~')) {
		int i;
		bool skipSpace = (c == ' ' && flags & TA_AltUL);

		// Yes, display it.
		if(c == '\b')
			i = -1;
		else {
			i = 1;
			if(skipSpace)
				tul(true, false);
			if(term.msgLineCol == term.cols - 1 && !(flags & MLNoEOL))
				c = LineExt;
			}
		(void) waddch(term.pMsgLineWind, c);
		term.msgLineCol += i;
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
int mlputs(const char *msg, uint flags) {

#if MMDebug & Debug_MsgLnRtn
	fprintf(logfile, "mlputs(\"%s\", %.4x)\n", msg, flags);
#endif
	if(interactive() || flags & MLForce) {
		short c;
		const char *msgEnd = strchr(msg, '\0');

#if MMDebug & Debug_MsgLnRtn
		fprintf(logfile, "    -> displaying \"%s\" message...\n", msg);
#endif
		// Position cursor and/or begin wrap, if applicable.
		if((flags & MLHome || term.msgLineCol < 0 || term.msgLineCol >= term.cols) && mlerase(flags) != Success)
			return sess.rtn.status;
		if((flags & (MLHome | MLWrap)) == (MLHome | MLWrap))
			mlputc('[', flags | MLRaw);

		// Copy string to message line.  If MLTermAttr flag is set, scan for attribute sequences and process them.
		while((c = *msg++) != '\0') {
			if(!(flags & MLTermAttr) || c != AttrSpecBegin)
				mlputc(c, flags);
			else
				msg = (const char *) putAttrStr(msg, msgEnd, &flags, (void *) term.pMsgLineWind);
			}

		(void) wattrset(term.pMsgLineWind, 0);

		// Finish wrap and flush message.
		if(flags & MLWrap)
			mlputc(']', flags | MLRaw);
		if(flags & MLFlush)
			(void) mlflush();
		}

	return sess.rtn.status;
	}

// Write text into the message line, given flag word, format string and optional arguments, and return status.  If the MLForce
// flag is set, force output; otherwise, suppress output if not interactive.  If the MLTermAttr flag is set, terminal attribute
// specifications are recognized in the string that is built (which includes any hard-coded ones in the fmt string).  Any format
// spec recognized by the vasprintf() function is allowed.
int mlprintf(uint flags, const char *fmt, ...) {

#if MMDebug & Debug_MsgLnRtn
	fprintf(logfile, "mlprintf(%.4x, \"%s\", ...)\n", flags, fmt);
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
		(void) mlputs(str, flags);
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
	pWind->face.wflags |= (WFHard | WFMode);
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
bool windNewTop(EWindow *pWind, Line *pLine, long n) {
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

	if(pLine != pOldTopLine) {
		pWind->face.pTopLine = pLine;
		pWind->face.wflags |= (WFHard | WFMode);
		return true;
		}
	return false;
	}

// Switch to given window in current screen.  Return status.
int wswitch(EWindow *pWind, uint flags) {
	Datum *pRtnVal = NULL;
	bool differentBuf = false;

	// Nothing to do if requested window is already current.
	if(pWind == sess.pCurWind)
		return sess.rtn.status;

	// Run exit-buffer hook if applicable.
	if(!(flags & SWB_NoBufHooks)) {
		differentBuf = (pWind->pBuf != sess.cur.pBuf);
		if(differentBuf && runBufHook(&pRtnVal, SWB_ExitHook) != Success)
			return sess.rtn.status;
		}

	// Switch windows.
	sess.edit.pBuf = sess.cur.pBuf = (sess.pCurScrn->pCurWind = sess.pCurWind = pWind)->pBuf;
	sess.edit.pFace = sess.cur.pFace = &pWind->face;

	// Run enter-buffer hook if applicable.
	if(differentBuf)
		(void) runBufHook(&pRtnVal, 0);

	return sess.rtn.status;
	}

// Get a required screen or window number (via prompt if interactive) and save in *pNum.  Prompt is built per "screen" value:
//	0	Build window prompt, otherwise screen.
//	1	Append " or 0 for new" to prompt.
//	> 1	Append " or 0 for none" to prompt.
// Return status.
int getNum(const char *prompt, int screen, int *pNum) {
	Datum *pDatum;

	if((pDatum = dnewtrack()) == NULL)
		return libfail();
	if(sess.opFlags & OpScript) {
		if(getNArg(pDatum, NULL) != Success)
			return sess.rtn.status;
		}
	else {
		char workBuf[WorkBufSize];

		// Build prompt with screen or window number range.
		sprintf(workBuf, "%s %s (1-%d)%s", prompt, screen ? text380 : text331,
								// "screen", "window"
		 screen ? scrnCount() : getWindCount(sess.pCurScrn, NULL), screen > 1 ? text411 : screen == 1 ? text445 : "");
										// " or 0 for none", " or 0 for new"
		if(getNArg(pDatum, workBuf) != Success || disnil(pDatum))
			return Cancelled;
		}

	// Return integer result.
	*pNum = dgetint(pDatum);
	return sess.rtn.status;
	}

// Switch to another window per flags.  Return status.
int gotoWind(Datum *pRtnVal, long n, uint flags) {

	if(n != 0 || !(flags & SWB_RepeatCount)) {
		EWindow *pWind;
		int windNum, count;
		int windCount = getWindCount(sess.pCurScrn, &windNum);

		// Check if n is out of range.
		if(flags & SWB_RepeatCount) {
			if(n == DefN)
				n = 1;
			else if(n < 0)
				return rsset(Failure, 0, text39, text137, n, 0);
						// "%s (%'ld) must be %d or greater", "Repeat count"

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
			if(n == 0 || labs(n) > windCount)
				return rsset(Failure, 0, text5, text331, n);
						// "No such %s '%'ld'", "window"
			if(n < 0)					// Target window is nth from the bottom of the screen.
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
int prevWind(Datum *pRtnVal, long n, Datum **args) {

	return gotoWind(pRtnVal, n, SWB_RepeatCount);
	}

// Switch to next window n times.  Return status.
int nextWind(Datum *pRtnVal, long n, Datum **args) {

	return gotoWind(pRtnVal, n, SWB_RepeatCount | SWB_Forw);
	}

// Switch to given window N, or nth from bottom if N < 0.  Return status.
int selectWind(Datum *pRtnVal, long n, Datum **args) {
	int windNum;

	// Get window number.
	if(sess.windHead->next == NULL && !(sess.opFlags & OpScript))
		return rsset(Failure, RSNoFormat, text294);
					// "Only one window"
	if(getNum(text113, 0, &windNum) == Success)
			// "Switch to"
		(void) gotoWind(pRtnVal, windNum, 0);

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

	return lineInWind(sess.pCurWind, sess.cur.pFace->point.pLine);
	}

// Move the current window up by "n" lines and compute the new top line of the window.
int moveWindUp(Datum *pRtnVal, long n, Datum **args) {

	// Nothing to do if buffer is empty.
	if(bempty(NULL))
		return sess.rtn.status;

	// Change top line.
	if(n == DefN)
		n = 1;
	windNewTop(sess.pCurWind, NULL, -n);

	// Is point still in the window?
	if(pointInWind())
		return sess.rtn.status;

	// Nope.  Move it to the center.
	Face *pFace = &sess.pCurWind->face;
	Line *pLine = pFace->pTopLine;
	int i = sess.pCurWind->rows / 2;
	while(i-- > 0 && pLine->next != NULL)
		pLine = pLine->next;
	pFace->point.pLine = pLine;
	pFace->point.offset = 0;

	return sess.rtn.status;
	}

// Make the current window the only window on the screen.  Try to set the framing so that point does not move on the screen.
int onlyWind(Datum *pRtnVal, long n, Datum **args) {
	EWindow *pWind;

	// If there is only one window, nothing to do.
	if(sess.windHead->next == NULL)
		return sess.rtn.status;

	// Nuke windows before current window.
	while(sess.windHead != sess.pCurWind) {
		pWind = sess.windHead;
		sess.pCurScrn->windHead = sess.windHead = pWind->next;
		--pWind->pBuf->windCount;
		windFaceToBufFace(pWind, pWind->pBuf);
		free((void *) pWind);
		}

	// Nuke windows after current window.
	while(sess.pCurWind->next != NULL) {
		pWind = sess.pCurWind->next;
		sess.pCurWind->next = pWind->next;
		--pWind->pBuf->windCount;
		windFaceToBufFace(pWind, pWind->pBuf);
		free((void *) pWind);
		}

	// Adjust window parameters.
	windNewTop(sess.pCurWind, NULL, -sess.pCurWind->topRow);
	sess.pCurWind->topRow = 0;
	sess.pCurWind->rows = term.rows - 2;
	sess.pCurWind->face.wflags |= (WFHard | WFMode);

	return sess.rtn.status;
	}

// Delete the current window ("join" is false) or an adjacent window ("join" is true), and place its space in another window.
// If interactive and n argument, or have argument in script mode, get one or more of the following options, which control the
// behavior of this function:
//	JoinDown	Place space from deleted window in window below it if possible, otherwise above it.
//	ForceJoin	Force join to target window, wrapping around if necessary.
//	DelBuf		Delete buffer in deleted window after join.
// Return status.
int delJoinWind(long n, CmdFunc *pCmdFunc) {
	EWindow *targWind;		// Window to receive deleted space.
	EWindow *pWind;			// Work pointer.
	Buffer *pOldBuf = sess.cur.pBuf;
	uint flags = pCmdFunc->pOptHdr->selFlags;
	bool join = (pCmdFunc - cmdFuncTable) == cf_joinWind;

	// If no options were selected interactively, bail out (user cancelled).
	if(n != DefN && !(sess.opFlags & OpScript) && flags == 0)
		return sess.rtn.status;

	// If there is only one window, bail out.
	if(sess.windHead->next == NULL)
		return rsset(Failure, RSNoFormat, text294);
			// "Only one window"

	// Find receiving window and transfer lines.  Check for special "wrap around" case first (which only applies if
	// we have at least three windows).
	if(sess.windHead->next->next != NULL &&
	 ((sess.pCurWind == sess.windHead && !(flags & Wind_JoinDown) && flags & Wind_ForceJoin) ||
	  (sess.pCurWind->next == NULL && flags & Wind_JoinDown && flags & Wind_ForceJoin))) {
		int rows;

		// Current window is top or bottom and need to transfer lines to window at opposite end of screen.
		if((sess.pCurWind == sess.windHead) != join) {
			if(join) {						// Switch to top window if join.
				if(wswitch(sess.windHead, 0) != Success)
					return sess.rtn.status;
				pOldBuf = sess.cur.pBuf;
				}
			targWind = windNextIs(NULL);				// Receiving window (bottom one).
			rows = -(sess.pCurWind->rows + 1);			// Top row adjustment for all windows.
			sess.pCurScrn->windHead = sess.windHead = sess.pCurWind->next; // Remove current window from list.
			}
		else {
			if(join) {						// Switch to bottom window if join.
				if(wswitch(windNextIs(NULL), 0) != Success)
					return sess.rtn.status;
				pOldBuf = sess.cur.pBuf;
				}
			targWind = sess.windHead;
			rows = sess.pCurWind->rows + 1;
			windNextIs(sess.pCurWind)->next = NULL;
			windNewTop(targWind, NULL, -rows);
			}

		// Adjust top rows of other windows and set update flags.
		pWind = sess.windHead;
		do {
			pWind->topRow += rows;
			pWind->face.wflags |= (WFHard | WFMode);
			} while((pWind = pWind->next) != NULL);
		sess.windHead->topRow = 0;

		// Adjust size of receiving window.
		targWind->rows += abs(rows);
		}
	else {
		// Set pWind to window before current one.
		pWind = windNextIs(sess.pCurWind);
		if((pWind == NULL ||
		 (flags & Wind_JoinDown && sess.pCurWind->next != NULL)) != join) {	// Next window down.
			if(join) {							// Switch upward one window if join.
				if(wswitch(pWind, 0) != Success)
					return sess.rtn.status;
				pWind = windNextIs(sess.pCurWind);
				pOldBuf = sess.cur.pBuf;
				}
			targWind = sess.pCurWind->next;
			targWind->topRow = sess.pCurWind->topRow;
			if(pWind == NULL)
				sess.pCurScrn->windHead = sess.windHead = targWind;
			else
				pWind->next = targWind;
			windNewTop(targWind, NULL, -(sess.pCurWind->rows + 1));
			}
		else {									// Next window up.
			if(join) {							// Switch downward one window if join.
				pWind = sess.pCurWind;
				if(wswitch(pWind->next, 0) != Success)
					return sess.rtn.status;
				pOldBuf = sess.cur.pBuf;
				}
			targWind = pWind;
			pWind->next = sess.pCurWind->next;
			}
		targWind->rows += sess.pCurWind->rows + 1;
		}

	// Get rid of the current window.
	--sess.cur.pBuf->windCount;
	windFaceToBufFace(sess.pCurWind, sess.cur.pBuf);
	free((void *) sess.pCurWind);

	(void) wswitch(targWind, 0);							// Can't fail.
	targWind->face.wflags |= (WFHard | WFMode);

	// Delete old buffer if requested.
	if(flags & Wind_DelBuf)
		(void) bdelete(pOldBuf, BD_RtnMsg);

	return sess.rtn.status;
	}


// Get a unique window id (a mark past the printable-character range for internal use) and return it in *pWindId.  Return
// status.
static int getWindId(ushort *pWindId) {
	EScreen *pScrn;
	EWindow *pWind;
	uint id = '~';

	// If no screen yet exists, use the first window id.
	if((pScrn = sess.scrnHead) == NULL)
		++id;
	else {
		// Get count of all windows in all screens and add it to the maximum user mark value (because all of those
		// windows already have ids).
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
			if(++id > sess.maxWindID)
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
int wsplit(long n, EWindow **ppWind) {
	EWindow *pWind;
	Line *pLine;
	ushort id;
	int upperRows, lowerRows, pointRow;
	bool pointInUpper;				// Point in upper window after split?
	Face *pFace = &sess.pCurWind->face;

	// Make sure we have enough space and can obtain a unique id.  If so, create a new window.
	if(sess.pCurWind->rows < 3)
		return rsset(Failure, 0, text293, sess.pCurWind->rows);
			// "Cannot split a %hu-line window"
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
	pWind->face.wflags = WFHard | WFMode;
	pWind->id = id;

	// Calculate new window sizes.
	upperRows = (sess.pCurWind->rows - 1) / 2;		// Upper window (default).
	if(n != DefN) {
		if(n < 0)
			upperRows = pointRow <= 1 ? 1 : pointRow - 1;
		else if(n > 0)
			upperRows = n < sess.pCurWind->rows - 1 ? n : sess.pCurWind->rows - 2;
		}
	lowerRows = (sess.pCurWind->rows - 1) - upperRows;	// Lower window.

	// Make new window the bottom one.
	pWind->next = sess.pCurWind->next;
	sess.pCurWind->next = pWind;
	sess.pCurWind->rows = upperRows;
	pWind->rows = lowerRows;
	pWind->topRow = sess.pCurWind->topRow + upperRows + 1;

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

	// Set top line and point line of each window as needed, keeping in mind that buffer may be empty (so top line and point
	// line point to the first line) or have just a few lines in it.  In the latter case, set top in the bottom window to
	// the last line of the buffer and point to same line, except for special case described below.
	if(pointRow < sess.pCurWind->rows) {

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
		upperRows = sess.pCurWind->rows / 2;
		pLine = pFace->pTopLine;
		while(upperRows-- > 0)
			pLine = pLine->next;
		pFace->point.pLine = pLine;
		pFace->point.offset = 0;
		}

	// Both windows are now set up.  All that's left is to set the window-update flags, set the current window to the bottom
	// one if needed, and return the non-point window pointer.
	sess.pCurWind->face.wflags |= (WFHard | WFMode);
	if((n == 0 && pointInUpper) || ((n == DefN || n == -1 || n > 0) && !pointInUpper)) {
		sess.pCurScrn->pCurWind = pWind;
		pWind = sess.pCurWind;
		sess.edit.pFace = sess.cur.pFace = &(sess.pCurWind = sess.pCurScrn->pCurWind)->face;
		}
	*ppWind = pWind;

	return sess.rtn.status;
	}

// Grow or shrink the current window.  If how == 0, set window size to abs(n) lines; otherwise, shrink (how < 0) or grow
// (how > 0) by abs(n) lines.  Find the window that loses or gains space and make sure the window that shrinks is big enough.
// If n < 0, try to use upper window, otherwise lower.  If it's a go, set the window flags and let the redisplay system do all
// the hard work.  (Can't just set "force reframe" because point would move.)  Return status.
int chgWindSize(Datum *pRtnVal, long n, int how) {
	EWindow *adjWind;
	bool grow = (how > 0);

	if(n == DefN)
		n = 1;
	else if(n == 0)
		return sess.rtn.status;		// Nothing to do.

	if(sess.windHead->next == NULL)
		return rsset(Failure, RSNoFormat, text294);
			// "Only one window"

	// Figure out which window (next or previous) to steal lines from or give lines to.
	adjWind = sess.pCurWind->next;
	if(sess.pCurWind != sess.windHead && (n < 0 || adjWind == NULL))
		adjWind = windNextIs(sess.pCurWind);

	if(n < 0)
		n = -n;
	if(how == 0) {

		// Want n-line window.  Convert n to a size adjustment.
		if(n > sess.pCurWind->rows) {
			n -= sess.pCurWind->rows;
			grow = true;
			}
		else if(n == sess.pCurWind->rows)
			return sess.rtn.status;	// Nothing to do.
		else {
			n = sess.pCurWind->rows - n;
			grow = false;
			}
		}

	if(grow) {
		// Adjacent window big enough?
		if(adjWind->rows <= n)
			return rsset(Failure, 0, text207, n, n == 1 ? "" : "s");
				// "Cannot get %ld line%s from adjacent window"

		// Yes, proceed.
		if(sess.pCurWind->next == adjWind) {		// Shrink below.
			windNewTop(adjWind, NULL, n);
			adjWind->topRow += n;
			}
		else {					// Shrink above.
			windNewTop(sess.pCurWind, NULL, -n);
			sess.pCurWind->topRow -= n;
			}
		sess.pCurWind->rows += n;
		adjWind->rows -= n;
		}
	else {
		// Current window big enough?
		if(sess.pCurWind->rows <= n)
			return rsset(Failure, 0, text93, n, n == 1 ? "" : "s");
				// "Current window too small to shrink by %ld line%s"

		// Yes, proceed.
		if(sess.pCurWind->next == adjWind) {		// Grow below.
			windNewTop(adjWind, NULL, -n);
			adjWind->topRow -= n;
			}
		else {					// Grow above.
			windNewTop(sess.pCurWind, NULL, n);
			sess.pCurWind->topRow += n;
			}
		sess.pCurWind->rows -= n;
		adjWind->rows += n;
		}

	sess.pCurWind->face.wflags |= (WFHard | WFMode);
	adjWind->face.wflags |= (WFHard | WFMode);

	return sess.rtn.status;
	}

// Resize the current window to the requested size.
int resizeWind(Datum *pRtnVal, long n, Datum **args) {

	// Ignore if no argument or already requested size.
	if(n == DefN || n == sess.pCurWind->rows)
		return sess.rtn.status;

	// Error if only one window.
	if(sess.windHead->next == NULL)
		return rsset(Failure, RSNoFormat, text294);
			// "Only one window"

	// Set all windows on current screen to same size if n == 0; otherwise, change current window only.
	if(n == 0) {
		int count = getWindCount(sess.pCurScrn, NULL);
		int size = (term.rows - 1) / count - 1;	// Minimum size of each window, excluding mode line.
		int leftover0 = (term.rows - 1) % count;

		// Remember current window, then step through all windows repeatedly, setting new size of each if possible,
		// until all windows have been set successfully or become deadlocked (which can happen if there are numerous
		// tiny windows).  Give up in the latter case and call it good (a very rare occurrence).
		EWindow *pOldWind = sess.pCurWind;
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
int render(Datum *pRtnVal, long n, Buffer *pBuf, uint flags) {
	EWindow *pWind = NULL;
	bool newWind = false;

	if(n == DefN)
		n = -1;

	// Displaying buffer?
	if(n != 0) {
		// Yes.  Popping buffer?
		if(n == -1) {
			// Yes.  Activate buffer if needed and do a real pop-up.
			if((!(pBuf->flags & BFActive) && bactivate(pBuf) != Success) || bpop(pBuf, flags | RendWait) != Success)
				return sess.rtn.status;
			}

		// Not popping buffer.  Switch to it?
		else if(n == 1) {
			if(pBuf != sess.cur.pBuf && bswitch(pBuf, 0) != Success)
				return sess.rtn.status;
			pWind = sess.pCurWind;
			}

		// No, displaying buffer in another window.
		else {
			EWindow *pOldWind;
			ushort flags;

			if(n > 1 || sess.windHead->next == NULL) {	// If force-creating or only one window...
				if(wsplit(DefN, &pWind) != Success)	// split current window and get new one into pWind.
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
			else if((pWind = windNextIs(sess.pCurWind)) == NULL)	// or the window above, if none found.
				pWind = sess.pCurWind->next;
			pOldWind = sess.pCurWind;			// save old window...
			flags = (labs(n) == 2) ? SWB_NoBufHooks : 0;	// set wswitch() flag...
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
			astash(pRtnVal, pArray);
			}

		if(flags & RendNotify || (n == 0 && flags & RendNewBuf))
			(void) rsset(Success, RSHigh, text381, pBuf->bufname);
				// "Buffer '%s' created"
		}
	if(pBuf != NULL && flags & RendRewind) {
		pBuf->flags &= ~BFHidden;
		bcount(pBuf, BFCmdFunc);
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Scroll the previous or next window up (backward) or down (forward) a page.
int wscroll(Datum *pRtnVal, long n, int (*windFunc)(Datum *pRtnVal, long n, Datum **args),
 int (*pageFunc)(Datum *pRtnVal, long n, Datum **args)) {

	(void) windFunc(pRtnVal, DefN, NULL);
	(void) pageFunc(pRtnVal, n, NULL);
	(void) (*(windFunc == prevWind ? nextWind : prevWind))(pRtnVal, DefN, NULL);

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

	if(pBuf->pCallHdr == NULL)
		strcpy(workBuf, "NULL");
	else
		sprintf(workBuf, "%hu", pBuf->pCallHdr->execCount);
	fprintf(logfile,
	 "\tb_face.firstCol: %d\n\tb_nwind: %hu\n\tb_nexec: %s\n\tb_nalias: %hu\n\tb_flags: %.4x\n\tb_modes:",
	 pBuf->face.firstCol, pBuf->windCount, workBuf, pBuf->aliasCount, (uint) pBuf->flags);
	if(pBuf->modes == NULL)
		fputs(" NONE", logfile);
	else {
		BufMode *pBufMode = pBuf->modes;
		do {
			fprintf(logfile, " %s", pBufMode->pModeSpec->name);
			} while((pBufMode = pBufMode->next) != NULL);
		}
	if(pBuf->filename == NULL)
		strcpy(workBuf, "NULL");
	else
		sprintf(workBuf, "'%s'", pBuf->filename);
	fprintf(logfile, "\n\tinpDelim: %s\n\tfilename: %s\n", delimSymToStr(pBuf->inpDelimSym), workBuf);

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
	 (uint) pWind->face.wflags, pWind->face.firstCol);
	}

// Write screen, window, and buffer information to log file -- for debugging.
void dumpScreens(const char *msg) {
	EScreen *pScrn;
	EWindow *pWind;
	HashNode *pNode;
	HashEach each;
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
	heachinit(&each, &buf.table);
	while((pNode = heach(&each)) != NULL)
		dumpBuffer(NULL, bufPtr(pNode->pVal), false);
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
// pointer to result pointer.  If the screen is not found and "pBuf" is not NULL, create new screen and return status;
// otherwise, return false, ignoring ppScrn.
int sfind(ushort scrnNum, Buffer *pBuf, EScreen **ppScrn) {
	EScreen *pScrn;
	ushort count = 0;
	static const char myName[] = "sfind";

	// Scan the screen list.  Note that the screen list is empty at program launch.
	for(pScrn = sess.scrnHead; pScrn != NULL; pScrn = pScrn->next) {
		if(pScrn->num == scrnNum) {
			if(ppScrn)
				*ppScrn = pScrn;
			return (pBuf == NULL) ? true : sess.rtn.status;
			}
		++count;
		}

	// No such screen exists, create one?
	if(pBuf != NULL) {
		ushort id;
		EWindow *pWind;				// Pointer to first window of new screen.

		// Get unique window id.
		if(getWindId(&id) != Success)
			return sess.rtn.status;

		// Allocate memory for screen.
		if((pScrn = (EScreen *) malloc(sizeof(EScreen))) == NULL)
			return rsset(Panic, 0, text94, myName);
				// "%s(): Out of memory!"

		// Set up screen fields.
		pScrn->pLastBuf = NULL;
		pScrn->num = count + 1;
		pScrn->flags = 0;
		pScrn->workDir = NULL;
		if(setWorkDir(pScrn) != Success)
			return sess.rtn.status;		// Fatal error.
		pScrn->rows = term.rows;
		pScrn->wrapCol = (pScrn->cols = term.cols) - 4;
		pScrn->prevWrapCol = -1;
		pScrn->hardTabSize = 8;
		pScrn->softTabSize = pScrn->cursorRow = pScrn->cursorCol = pScrn->firstCol = 0;

		// Allocate its first window...
		if((pWind = (EWindow *) malloc(sizeof(EWindow))) == NULL)
			return rsset(Panic, 0, text94, myName);
				// "%s(): Out of memory!"
		pScrn->windHead = pScrn->pCurWind = pWind;

		// and set up the window's info.
		pWind->next = NULL;
		pWind->pBuf = pBuf;
		++pBuf->windCount;
		bufFaceToWindFace(pBuf, pWind);
		pWind->id = id;
		pWind->topRow = 0;
		pWind->rows = term.rows - 2;		// "-2" for message and mode lines.
		pWind->reframeRow = 0;

		// Insert new screen at beginning of screen list...
		pScrn->next = sess.scrnHead;
		sess.scrnHead = pScrn;

		// and return the new screen pointer.
		if(ppScrn)
			*ppScrn = pScrn;
		return sess.rtn.status;
		}

	// Screen not found and pBuf is NULL.
	return false;
	}

// Switch to given screen.  Return status.
int sswitch(EScreen *pScrn, uint flags) {

	// Nothing to do if requested screen is already current.
	if(pScrn == sess.pCurScrn)
		return sess.rtn.status;

	// Save the current screen's concept of current window.
	sess.pCurScrn->pCurWind = sess.pCurWind;
	sess.pCurScrn->rows = term.rows;
	sess.pCurScrn->cols = term.cols;

	// Run exit-buffer user hook on current (old) buffer if the new screen's buffer is different.
	Datum *pRtnVal = NULL;
	bool differentBuf = false;
	if(!(flags & SWB_NoBufHooks)) {
		differentBuf = (pScrn->pCurWind->pBuf != sess.cur.pBuf);
		if(differentBuf && runBufHook(&pRtnVal, SWB_ExitHook) != Success)
			return sess.rtn.status;
		}

	// Save "last screen" and change current directory if needed.
	sess.pLastScrn = sess.pCurScrn;
	if(pScrn->workDir != sess.pCurScrn->workDir)
		(void) chgdir(pScrn->workDir);

	// Change the current screen, window and buffer.
	sess.windHead = (sess.pCurScrn = pScrn)->windHead;
	sess.edit.pBuf = sess.cur.pBuf = (sess.pCurWind = pScrn->pCurWind)->pBuf;
	sess.edit.pFace = sess.cur.pFace = &pScrn->pCurWind->face;

	// Let the display driver know we need a full screen update.
	sess.opFlags |= OpScrnRedraw;

	// Run enter-buffer user hook on current (new) buffer.
	if(differentBuf && sess.rtn.status == Success)
		(void) runBufHook(&pRtnVal, 0);

	return sess.rtn.status;
	}

// Put a new screen's working directory in return message if 'WkDir' global mode not enabled and directory changed.
void newWorkDir(EScreen *pOldScrn) {

	if(!(mode.cache[MdIdxWkDir]->flags & MdActive) && sess.pCurScrn->workDir != pOldScrn->workDir)
		rsset(Success, RSHigh | RSNoWrap, text409, sess.pCurScrn->workDir);
						// "CWD: %s"
	}

// Bring a screen to the front per flags.  Return status.
int gotoScreen(long n, uint flags) {

	if(n != 0 || !(flags & SWB_RepeatCount)) {
		EScreen *pScrn, *pOldScrn;
		int scrnTotal = scrnCount();				// Total number of screens.

		// Check if n is out of range.
		if(flags & SWB_RepeatCount) {
			if(n == DefN)
				n = 1;
			else if(n < 0)
				return rsset(Failure, 0, text39, text137, n, 0);
						// "%s (%'ld) must be %d or greater", "Repeat count"

			if(scrnTotal == 1 || (n %= scrnTotal) == 0)	// If only one screen or repeat count == screen count...
				return sess.rtn.status;			// nothing to do.
			if(flags & SWB_Forw) {
				if((n += sess.pCurScrn->num) > scrnTotal)
					n -= sess.pCurScrn->num;
				}
			else if((n = sess.pCurScrn->num - n) < 1)
				n += scrnTotal;
			}
		else if(n <= 0 || labs(n) > scrnTotal)
			return rsset(Failure, 0, text5, text380, n);
					// "No such %s '%'ld'", "screen"

		// n is now the target screen number.
		pScrn = sess.scrnHead;					// Find the screen...
		while(pScrn->num != n)
			pScrn = pScrn->next;
		pOldScrn = sess.pCurScrn;				// save current screen...
		if(sswitch(pScrn, 0) == Success)			// make new screen current...
			newWorkDir(pOldScrn);				// and display its working directory if appropriate.

		}
	return sess.rtn.status;
	}

// Set user-customizable default values in a new screen and return status.
static int setdef(void) {
	int *pParam = &sess.pCurScrn->hardTabSize;
	Option *pOpt = defOptions;

	// Set hard tab size;
	if(pOpt->u.svalue != -1) {
		if(checkTabSize(pOpt->u.svalue, true) == Success)
			*pParam = pOpt->u.svalue;
		else {
			pOpt->u.svalue = -1;
			goto Retn;
			}
		}
	++pOpt; ++pParam;

	// Set soft tab size;
	if(pOpt->u.svalue != -1) {
		if(checkTabSize(pOpt->u.svalue, false) == Success)
			*pParam = pOpt->u.svalue;
		else {
			pOpt->u.svalue = -1;
			goto Retn;
			}
		}
	++pOpt; ++pParam;

	// Set wrap column.
	if(pOpt->u.svalue != -1) {
		if(checkWrapCol(pOpt->u.svalue) == Success)
			*pParam = pOpt->u.svalue;
		else
			pOpt->u.svalue = -1;
		}
Retn:
	return sess.rtn.status;
	}

// Switch to given screen, or create one if screen number is zero.  Return status.
int selectScreen(Datum *pRtnVal, long n, Datum **args) {
	int scrnNum;

	// Get screen number.
	if(getNum(text113, 1, &scrnNum) != Success)
			// "Switch to"
		return sess.rtn.status;

	// Create screen?
	if(scrnNum == 0) {
		EScreen *pScrn;

		// Yes.  Save current screen number and current window's settings.
		windFaceToBufFace(sess.pCurWind, sess.cur.pBuf);

		// Find screen "0" to force-create one and make it current.
		if(sfind(0, sess.cur.pBuf, &pScrn) != Success || sswitch(pScrn, 0) != Success)
			return sess.rtn.status;
		if(setdef() == Success)
			(void) rsset(Success, 0, text174, pScrn->num);
					// "Created screen %hu"
		}
	else
		// Not creating... switch to one specified.
		(void) gotoScreen(scrnNum, 0);

	return sess.rtn.status;
	}

// Swap numbers in two screens and return status.
int swapScreens(Datum *pRtnVal, long n, Datum **args) {
	EScreen *pScrn1, *pScrn2;
	int scrnNum1, scrnNum2;

	// Get screen numbers.  Error if only one exists.
	if(sess.scrnHead->next == NULL)
		return rsset(Failure, RSNoFormat, text57);
					// "Only one screen"
	if(getNum(text531, -1, &scrnNum1) != Success || getNum(text86, -1, &scrnNum2) != Success)
			// "Swap", 			"with"
		return sess.rtn.status;

	// Get screen pointers.
	pScrn1 = NULL;
	if(!sfind((ushort) scrnNum1, NULL, &pScrn1) || !sfind((ushort) scrnNum2, NULL, &pScrn2))
		return rsset(Failure, 0, text5, text380, (long)(pScrn1 ? scrnNum2 : scrnNum1));
				// "No such %s '%'ld'", "screen"

	// Nothing to do if screen numbers are the same.
	if(scrnNum1 == scrnNum2)
		return sess.rtn.status;

	// We're good... swap screen numbers.
	pScrn1->num = scrnNum2;
	pScrn2->num = scrnNum1;

	// Flag mode line at bottom of screen (last window) for an update if either screen is current one.
	if(sess.pCurScrn->num == scrnNum1 || sess.pCurScrn->num == scrnNum2)
		windNextIs(NULL)->face.wflags |= WFMode;

	return rsset(Success, RSNoFormat, text532);
		// "Screen numbers swapped"
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

// Remove given screen from the list, renumber remaining ones, and update mode line of bottom window.  Return status.
static int unlistScrn(EScreen *pScrn) {
	EScreen *pScrn1;

	if(pScrn == sess.scrnHead)
		sess.scrnHead = sess.scrnHead->next;
	else {
		pScrn1 = sess.scrnHead;
		do {
			if(pScrn1->next == pScrn)
				goto Found;
			} while((pScrn1 = pScrn1->next) != NULL);

		// Huh?  Screen not found... this is a bug.
		return rsset(FatalError, 0, text177, "unlistScrn", (int) pScrn->num);
				// "%s(): Screen number %d not found in screen list!"
Found:
		pScrn1->next = pScrn->next;
		}

	// Renumber higher-numbered screens.
	pScrn1 = sess.scrnHead;
	do {
		if(pScrn1->num > pScrn->num)
			--pScrn1->num;
		} while((pScrn1 = pScrn1->next) != NULL);

	// Flag mode line at bottom of screen (last window) for an update.
	windNextIs(NULL)->face.wflags |= WFMode;

	return sess.rtn.status;
	}

// Delete given screen and its homed buffers if "delBufs" is true.
int sdelete(EScreen *pScrn, bool delBufs) {
	const char *workDir;
	uint count;
	int scrnNum = pScrn->num;

	// Grab screen's working directory...
	workDir = pScrn->workDir;

	// and nuke it.
	if(unlistScrn(pScrn) != Success)
		return sess.rtn.status;
	freeScrn(pScrn);
	if(sess.pLastScrn == pScrn)
		sess.pLastScrn = NULL;

	// Also delete buffers (including hidden) that are homed to nuked screen, if requested.
	if(delBufs)
		return bnuke(BD_Homed | BD_Hidden, workDir, &count) != Success ? sess.rtn.status :
		 rsset(Success, RSHigh, text446, scrnNum, count, count == 1 ? "" : "s");
			// "Screen %d and %u homed buffer%s deleted"
	else
		return rsset(Success, RSHigh, text178, scrnNum);
				// "Screen %d deleted"
	}

// Delete a screen.  Return status.
int delScreen(Datum *pRtnVal, long n, Datum **args) {
	EScreen *pScrn;
	int scrnNum;

	// Get the number of the screen to delete.  Error if only one exists.
	if(sess.scrnHead->next == NULL)
		return rsset(Failure, RSNoFormat, text57);
					// "Only one screen"
	if(getNum(text26, -1, &scrnNum) != Success)
			// "Delete"
		return sess.rtn.status;

	// Make sure it exists.
	if(!sfind((ushort) scrnNum, NULL, &pScrn))
		return rsset(Failure, 0, text5, text380, (long) scrnNum);
				// "No such %s '%'ld'", "screen"

	// Switch screens if deleting current one.
	if(pScrn == sess.pCurScrn)
		if(sswitch(pScrn == sess.scrnHead ? pScrn->next : sess.scrnHead, 0) != Success)
			return sess.rtn.status;

	// Everything's cool... nuke it.
	return sdelete(pScrn, n < 0 && n != DefN);
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
int setColorPair(Datum *pRtnVal, long n, Datum **args) {

	if(!haveColor())
		return sess.rtn.status;

	short pair, colors[2], *colorPair = colors, *colorPairEnd = colors + 2;
	long c;

	// Get arguments and validate them.
	if((c = dgetint((*args++))) != 0)
		if(c < 1 || c > term.maxWorkPair)
			return rsset(Failure, 0, text12, text425, c, 0, (long) term.maxWorkPair);
				// "%s (%'ld) must be in range %d - %'ld", "Color pair"
	pair = c;
	do {
		if((c = dgetint((*args++))) < -1 || c > term.maxColor)
			return rsset(Failure, 0, text12, text426, c, -1, (long) term.maxColor);
				// "%s (%'ld) must be in range %d - %'ld", "Color no."
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
int setDispColor(Datum *pRtnVal, long n, Datum **args) {
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
		Array *pArray = dgetarrayptr(args[1]);

		if(pArray->used != 2)
			goto Err;
		ppArrayElEnd = (ppArrayEl = pArray->elements) + 2;
		do {
			pArrayEl = *ppArrayEl++;
			if(pArrayEl->type != dat_int || (c = dgetint(pArrayEl)) < -1 || c > term.maxColor)
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
