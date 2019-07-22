// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// display.c	High level display routines for MightEMacs.
//
// This file contains functions that call the lower level terminal display functions in vterm.c.

#include "os.h"
#include <stdarg.h>
#include <ctype.h>
#include "std.h"
#include "exec.h"

// Set given flags on all windows on current screen.  If buf is not NULL, mark only those windows.
void supd_wflags(Buffer *buf,ushort flags) {

	if(buf == NULL || buf->b_nwind > 0) {
		EWindow *win;

		// Only need to update windows on current screen because windows on background screens are automatically
		// updated when the screen to brought to the foreground.
		win = si.whead;
		do {
			if(buf == NULL || win->w_buf == buf)
				win->w_flags |= flags;
			} while((win = win->w_next) != NULL);
		}
	}

// Find window on current screen whose w_next matches given pointer and return it, or NULL if not found (win is top window).
EWindow *wnextis(EWindow *win) {
	EWindow *win1 = si.whead;

	if(win == win1)
		return NULL;			// No window above top window.
	while(win1->w_next != win)
		win1 = win1->w_next;
	return win1;
	}

// Find window on current screen displaying given buffer and return pointer to it, or NULL if not found.  Do not consider
// current window if skipCur is true.
EWindow *whasbuf(Buffer *buf,bool skipCur) {
	EWindow *win = si.whead;
	do {
		if(win->w_buf == buf && (win != si.curwin || !skipCur))
			return win;
		} while((win = win->w_next) != NULL);
	return NULL;
	}

// Flush message line to physical screen.  Return status.
int mlflush(void) {

	return wrefresh(term.mlwin) == ERR ? scallerr("mlflush","wrefresh",false) : rc.status;
	}

// Move cursor to column "col" in message line.  Return status.
int mlmove(int col) {

	return wmove(term.mlwin,0,term.mlcol = col) == ERR ? scallerr("mlmove","wmove",false) : rc.status;
	}

// Restore message line cursor position.  Return status.
int mlrestore(void) {

	if(mlmove(term.mlcol) == Success)
		(void) mlflush();
	return rc.status;
	}

// Erase to end of line on message line.
void mleeol(void) {

	// Ignore return code in case cursor is at right edge of screen, which causes an ERR return and a subsequent program
	// abort.
	(void) wclrtoeol(term.mlwin);
	}

// Erase the message line.  Return status.
int mlerase(void) {

	if(term.mlcol != 0) {

		// Home the cursor and erase to end of line.
		if(mlmove(0) == Success) {
			mleeol();
			(void) mlflush();
			}
		}
	return rc.status;
	}

// Write a character to the message line with invisible characters exposed, unless MLRaw flag is set.  Keep track of the
// physical cursor position so that a LineExt ($) can be displayed at the right edge of the screen if the cursor moves past it
// (unless MLNoEOL flag is set).  Return status.
void mlputc(ushort flags,short c) {

	// Nothing to do if past right edge of screen.
	if(term.mlcol >= term.t_ncol)
		return;

	// Raw or plain character?
	if((flags & MLRaw) || (c >= ' ' && c <= '~')) {
		int n;
		bool skipSpace = (c == ' ' && (flags & TAAltUL));

		// Yes, display it.
		if(c == '\b')
			n = -1;
		else {
			n = 1;
			if(skipSpace)
				ttul(true,false);
			if(term.mlcol == term.t_ncol - 1 && !(flags & MLNoEOL))
				c = LineExt;
			}
		(void) waddch(term.mlwin,c);
		term.mlcol += n;
		if(skipSpace)
			ttul(true,true);
		}
	else {
		char *str;

		// Not raw.  Display char literal (if any) and remember cursor position until it reaches the right edge.
		c = *(str = vizc(c,VBaseDef));
		do {
			(void) waddch(term.mlwin,term.mlcol == term.t_ncol - 1 && !(flags & MLNoEOL) ? LineExt : c);
			} while(++term.mlcol < term.t_ncol && (c = *++str) != '\0');
		}
	}

// Prepare for new message line message.  If MLHome flag is set, clear message line.  If MLWrap flag is set, write leading '['
// of message-wrap characters "[]".  Return status.
static int mlbegin(ushort flags) {

	// Position cursor and/or begin wrap, if applicable.
	if(((flags & MLHome) || term.mlcol < 0 || term.mlcol >= term.t_ncol) && mlerase() != Success)
		return rc.status;
	if((flags & (MLHome | MLWrap)) == (MLHome | MLWrap))
		mlputc(flags | MLRaw,'[');

	return rc.status;
	}

// Finish message line message.  Return status.
static int mlend(ushort flags) {

	// Finish wrap and flush message.
	if(flags & MLWrap)
		mlputc(flags | MLRaw,']');
	return (flags & MLFlush) ? mlflush() : rc.status;
	}

// Write a string to the message line, given flags and message.  If MLTermAttr flag is set, terminal attribute sequences
// (beginning with '~') in message are processed.  Return status.
int mlputs(ushort flags,char *str) {
	short c;
	char *strz = strchr(str,'\0');

	if(mlbegin(flags) != Success)
		return rc.status;

	// Copy string to message line.  If MLTermAttr flag is set, scan for attribute sequences and process them.
	while((c = *str++) != '\0') {
		if(!(flags & MLTermAttr) || c != AttrSpecBegin)
			mlputc(flags,c);
		else
			termattr(&str,strz,&flags,term.mlwin);
		}

	(void) wattrset(term.mlwin,0);
	return mlend(flags);
	}

// Write text into the message line, given flag word, format string and optional arguments.  Return status.  If the MLTermAttr
// flag is set, terminal attribute specifications are recognized in the string that is built (which includes any hard-coded ones
// in the fmt string).  Any format spec recognized by the vasprintf() function is allowed.
int mlprintf(ushort flags,char *fmt,...) {
	va_list ap;
	char *str;

	// Process arguments.
	va_start(ap,fmt);
	(void) vasprintf(&str,fmt,ap);
	va_end(ap);
	(void) mlputs(flags,str);
	free((void *) str);
	return rc.status;
	}

// Initialize point position, marks, and first column position of a face record, given line pointer.  If buf not NULL, clear
// its buffer marks.
void faceinit(WindFace *wfp,Line *lnp,Buffer *buf) {

	wfp->wf_toplnp = wfp->wf_point.lnp = lnp;
	wfp->wf_point.off = wfp->wf_firstcol = 0;

	// Clear mark(s).
	if(buf != NULL)
		mdelete(buf,MkOpt_Viz | MkOpt_Wind);
	}

// Copy buffer face record to a window.
void bftowf(Buffer *buf,EWindow *win) {

	win->w_face = buf->b_face;
	win->w_flags |= (WFHard | WFMode);
	}

// Copy window face record to a buffer and clear the "buffer face in unknown state" flag.
void wftobf(EWindow *win,Buffer *buf) {

	buf->b_face = win->w_face;
	}

// Get ordinal number of given window, beginning at 1.
int getwnum(EWindow *win) {
	EWindow *win1 = si.whead;
	int num = 1;
	while(win1 != win) {
		win1 = win1->w_next;
		++num;
		}
	return num;
	}

// Get number of windows on given screen.  If wnum not NULL, set *wnum to screen's current window number.
int wincount(EScreen *scr,int *wnum) {
	EWindow *win = scr->s_whead;
	int count = 0;
	do {
		++count;
		if(wnum != NULL && win == scr->s_curwin)
			*wnum = count;
		} while((win = win->w_next) != NULL);
	return count;
	}

// Move up or down n lines (if possible) from given window line (or current top line of window if lnp is NULL) and set top line
// of window to result.  If n is negative, move up; if n is positive, move down.  Set hard update flag in window and return true
// if top line was changed; otherwise, false.
bool wnewtop(EWindow *win,Line *lnp,int n) {
	Line *oldtoplnp = win->w_face.wf_toplnp;
	if(lnp == NULL)
		lnp = oldtoplnp;

	if(n < 0) {
		while(lnp != win->w_buf->b_lnp) {
			lnp = lnp->l_prev;
			if(++n == 0)
				break;
			}
		}
	else if(n > 0) {
		while(lnp->l_next != NULL) {
			lnp = lnp->l_next;
			if(--n == 0)
				break;
			}
		}

	if((win->w_face.wf_toplnp = lnp) != oldtoplnp) {
		win->w_flags |= (WFHard | WFMode);
		return true;
		}
	return false;
	}

// Switch to given window.  Return status.
int wswitch(EWindow *win,ushort flags) {
	Datum *rval = NULL;
	bool diffbuf;

	// Run exit-buffer hook if applicable.
	if(!(flags & SWB_NoHooks)) {
		diffbuf = (win->w_buf != si.curbuf);
		if(diffbuf && bhook(&rval,SWB_ExitHook) != Success)
			return rc.status;
		}

	// Switch windows.
	si.curbuf = (si.curscr->s_curwin = si.curwin = win)->w_buf;

	// Run enter-buffer hook if applicable.
	if(!(flags & SWB_NoHooks) && diffbuf)
		(void) bhook(&rval,0);

	return rc.status;
	}

// Get a required screen or window number (via prompt if interactive) and save in *np.  Return status.
static int getnum(char *prmt,int screen,int *np) {
	Datum *datum;

	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript) {
		if(getnarg(datum,NULL) != Success)
			return rc.status;
		}
	else {
		char wkbuf[NWork];

		// Build prompt with screen or window number range.
		sprintf(wkbuf,"%s %s (1-%d)%s",prmt,screen ? text380 : text331,screen ? scrcount() : wincount(si.curscr,NULL),
							// "screen","window"
		 screen > 0 ? text445 : "");
			// " or 0 for new"
		if(getnarg(datum,wkbuf) != Success || datum->d_type == dat_nil)
			return Cancelled;
		}

	// Return integer result.
	*np = datum->u.d_int;
	return rc.status;
	}

// Switch to another window per flags.  Return status.
int gotoWind(Datum *rval,int n,ushort flags) {

	if(n != 0 || !(flags & SWB_Repeat)) {
		EWindow *win;
		int wnum,count;
		int winct = wincount(si.curscr,&wnum);

		// Check if n is out of range.
		if(flags & SWB_Repeat) {
			if(n == INT_MIN)
				n = 1;
			else if(n < 0)
				return rcset(Failure,0,text39,text137,n,0);
						// "%s (%d) must be %d or greater","Repeat count"

			if(winct == 1 || (n %= winct) == 0)	// If only one window or repeat count equals window count...
				return rc.status;		// nothing to do.
			if(flags & SWB_Forw) {
				if((n += wnum) > winct)
					n -= wnum;
				}
			else if((n = wnum - n) < 1)
				n += winct;
			}
		else {
			if(n == 0 || abs(n) > winct)
				return rcset(Failure,0,text239,n);
						// "No such window '%d'"
			if(n < 0)				// Target window is nth from the bottom of the screen.
				n += winct + 1;
			}

		// n is now the target window number.
		win = si.whead;				// Find the window...
		count = 0;
		while(++count != n)
			win = win->w_next;
		(void) wswitch(win,0);				// and make new window current.
		supd_wflags(NULL,WFMode);
		}
	return rc.status;
	}

// Switch to previous window n times.  Return status.
int prevWind(Datum *rval,int n,Datum **argv) {

	return gotoWind(rval,n,SWB_Repeat);
	}

// Switch to next window n times.  Return status.
int nextWind(Datum *rval,int n,Datum **argv) {

	return gotoWind(rval,n,SWB_Repeat | SWB_Forw);
	}

// Switch to given window N, or nth from bottom if N < 0.  Return status.
int selectWind(Datum *rval,int n,Datum **argv) {

	// Get window number.
	if(si.whead->w_next == NULL && !(si.opflags & OpScript))
		return rcset(Failure,RCNoFormat,text294);
					// "Only one window"
	if(getnum(text113,0,&n) == Success)
			// "Switch to"
		(void) gotoWind(rval,n,0);

	return rc.status;
	}

// Check if given line is in given window and return Boolean result.
bool inwind(EWindow *win,Line *lnp) {
	Line *lnp1 = win->w_face.wf_toplnp;
	ushort i = 0;
	do {
		if(lnp1 == lnp)
			return true;
		if((lnp1 = lnp1->l_next) == NULL)
			break;
		} while(++i < win->w_nrows);
	return false;
	}

// Return true if point is in current window; otherwise, false.
bool ptinwind(void) {

	return inwind(si.curwin,si.curwin->w_face.wf_point.lnp);
	}

// Move the current window up by "n" lines and compute the new top line of the window.
int moveWindUp(Datum *rval,int n,Datum **argv) {

	// Nothing to do if buffer is empty.
	if(bempty(NULL))
		return rc.status;

	// Change top line.
	if(n == INT_MIN)
		n = 1;
	wnewtop(si.curwin,NULL,-n);

	// Is point still in the window?
	if(ptinwind())
		return rc.status;

	// Nope.  Move it to the center.
	WindFace *wfp = &si.curwin->w_face;
	Line *lnp = wfp->wf_toplnp;
	int i = si.curwin->w_nrows / 2;
	while(i-- > 0 && lnp->l_next != NULL)
		lnp = lnp->l_next;
	wfp->wf_point.lnp = lnp;
	wfp->wf_point.off = 0;

	return rc.status;
	}

// Make the current window the only window on the screen.  Try to set the framing so that point does not move on the screen.
int onlyWind(Datum *rval,int n,Datum **argv) {
	EWindow *win;

	// If there is only one window, nothing to do.
	if(si.whead->w_next == NULL)
		return rc.status;

	// Nuke windows before current window.
	while(si.whead != si.curwin) {
		win = si.whead;
		si.curscr->s_whead = si.whead = win->w_next;
		if(--win->w_buf->b_nwind == 0)
			win->w_buf->b_lastscr = si.curscr;
		wftobf(win,win->w_buf);
		free((void *) win);
		}

	// Nuke windows after current window.
	while(si.curwin->w_next != NULL) {
		win = si.curwin->w_next;
		si.curwin->w_next = win->w_next;
		if(--win->w_buf->b_nwind == 0)
			win->w_buf->b_lastscr = si.curscr;
		wftobf(win,win->w_buf);
		free((void *) win);
		}

	// Adjust window parameters.
	wnewtop(si.curwin,NULL,-si.curwin->w_toprow);
	si.curwin->w_toprow = 0;
	si.curwin->w_nrows = term.t_nrow - 2;
	si.curwin->w_flags |= (WFHard | WFMode);

	return rc.status;
	}

// Delete the current window ("join" is false) or an adjacent window ("join" is true), and place its space in another window.
// If n argument, get one or more of the following options, which control the behavior of this function:
//	JoinDown	Place space from deleted window in window below it if possible; otherwise, above it.
//	ForceJoin	Force join to target window, wrapping around if necessary.
//	DelBuf		Delete buffer in deleted window after join.
// Return status.
static int djwind(int n,Datum **argv,bool join) {
	EWindow *targwin;		// Window to receive deleted space.
	EWindow *win;			// Work pointer.
	Buffer *oldbuf = si.curbuf;
	static bool joinDown;
	static bool force;
	static bool delBuf;
	static Option options[] = {
		{"^JoinDown","^JD",0,.u.ptr = (void *) &joinDown},
		{"^ForceJoin","^Frc",0,.u.ptr = (void *) &force},
		{"^DelBuf","^Del",0,.u.ptr = (void *) &delBuf},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		0,text410,false,options};
			// "command option"

	// Get options and set flags.
	initBoolOpts(options);
	if(n != INT_MIN) {
		int count;

		if(parseopts(&ohdr,text448,argv[0],&count) != Success)
				// "Options"
			return rc.status;
		if(count > 0)
			setBoolOpts(options);
		}

	// If there is only one window, bail out.
	if(si.whead->w_next == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Find receiving window and transfer lines.  Check for special "wrap around" case first (which only applies if
	// we have at least three windows).
	if(si.whead->w_next->w_next != NULL &&
	 ((si.curwin == si.whead && !joinDown && force) || (si.curwin->w_next == NULL && joinDown && force))) {
		int rows;

		// Current window is top or bottom and need to transfer lines to window at opposite end of screen.
		if((si.curwin == si.whead) != join) {
			if(join) {						// Switch to top window if join.
				if(wswitch(si.whead,0) != Success)
					return rc.status;
				oldbuf = si.curbuf;
				}
			targwin = wnextis(NULL);				// Receiving window (bottom one).
			rows = -(si.curwin->w_nrows + 1);			// Top row adjustment for all windows.
			si.curscr->s_whead = si.whead = si.curwin->w_next;	// Remove current window from list.
			}
		else {
			if(join) {						// Switch to bottom window if join.
				if(wswitch(wnextis(NULL),0) != Success)
					return rc.status;
				oldbuf = si.curbuf;
				}
			targwin = si.whead;
			rows = si.curwin->w_nrows + 1;
			wnextis(si.curwin)->w_next = NULL;
			wnewtop(targwin,NULL,-rows);
			}

		// Adjust top rows of other windows and set update flags.
		win = si.whead;
		do {
			win->w_toprow += rows;
			win->w_flags |= (WFHard | WFMode);
			} while((win = win->w_next) != NULL);
		si.whead->w_toprow = 0;

		// Adjust size of receiving window.
		targwin->w_nrows += abs(rows);
		}
	else {
		// Set win to window before current one.
		win = wnextis(si.curwin);
		if((win == NULL || (joinDown && si.curwin->w_next != NULL)) != join) {	// Next window down.
			if(join) {							// Switch upward one window if join.
				if(wswitch(win,0) != Success)
					return rc.status;
				win = wnextis(si.curwin);
				oldbuf = si.curbuf;
				}
			targwin = si.curwin->w_next;
			targwin->w_toprow = si.curwin->w_toprow;
			if(win == NULL)
				si.curscr->s_whead = si.whead = targwin;
			else
				win->w_next = targwin;
			wnewtop(targwin,NULL,-(si.curwin->w_nrows + 1));
			}
		else {									// Next window up.
			if(join) {							// Switch downward one window if join.
				win = si.curwin;
				if(wswitch(win->w_next,0) != Success)
					return rc.status;
				oldbuf = si.curbuf;
				}
			targwin = win;
			win->w_next = si.curwin->w_next;
			}
		targwin->w_nrows += si.curwin->w_nrows + 1;
		}

	// Get rid of the current window.
	if(--si.curbuf->b_nwind == 0)
		si.curbuf->b_lastscr = si.curscr;
	wftobf(si.curwin,si.curbuf);
	free((void *) si.curwin);

	(void) wswitch(targwin,SWB_NoHooks);						// Can't fail.
	targwin->w_flags |= (WFHard | WFMode);

	// Delete old buffer if requested.
	if(delBuf) {
		char bname[MaxBufName + 1];
		strcpy(bname,oldbuf->b_bname);
		if(bdelete(oldbuf,0) == Success)
			(void) rcset(Success,0,text372,bname);
					// "Buffer '%s' deleted"
		}

	return rc.status;
	}

// Delete the current window, placing its space in another window, as described in delwind().  Return status.
int delWind(Datum *rval,int n,Datum **argv) {

	return djwind(n,argv,false);
	}

// Join the current window with another window, as described in delwind().  Return status.
int joinWind(Datum *rval,int n,Datum **argv) {

	return djwind(n,argv,true);
	}

// Get a unique window id (a mark past the printable-character range for internal use) and return it in *wid.  Return status.
int getwid(ushort *wid) {
	EScreen *scr;
	EWindow *win;
	uint id = '~';

	// If no screen yet exists, use the first window id.
	if((scr = si.shead) == NULL)
		++id;
	else {
		// Get count of all windows in all screens and add it to the maximum user mark value.
		do {
			// In all windows...
			win = scr->s_whead;
			do {
				++id;
				} while((win = win->w_next) != NULL);
			} while((scr = scr->s_next) != NULL);

		// Scan windows again and find an id that is unique.
		for(;;) {
NextId:
			if(++id > USHRT_MAX)
				return rcset(Failure,0,text356,id);
					// "Too many windows (%u)"

			// In all screens...
			scr = si.shead;
			do {
				// In all windows...
				win = scr->s_whead;
				do {
					if(id == win->w_id)
						goto NextId;
					} while((win = win->w_next) != NULL);
				} while((scr = scr->s_next) != NULL);

			// Success!
			break;
			}
		}

	// Unique id found.  Return it.
	*wid = id;
	return rc.status;
	}	

// Split the current window and return status.  The top or bottom line is dropped to make room for a new mode line, and the
// remaining lines are split into an upper and lower window.  A window smaller than three lines cannot be split.  The point
// remains in whichever window contains point after the split by default.  A line is pushed out of the other window and its
// point is moved to the center.  If n == 0, the point is forced to the opposite (non-default) window.  If n < 0, the size of
// the upper window is reduced by abs(n) lines; if n > 0, the upper window is set to n lines.  *winp is set to the new window
// not containing point.
int wsplit(int n,EWindow **winp) {
	EWindow *win;
	Line *lnp;
	ushort id;
	int nrowu,nrowl,nrowpt;
	bool defupper;			// Default is upper window?
	WindFace *wfp = &si.curwin->w_face;

	// Make sure we have enough space and can obtain a unique id.  If so, create a new window.
	if(si.curwin->w_nrows < 3)
		return rcset(Failure,0,text293,si.curwin->w_nrows);
			// "Cannot split a %d-line window"
	if(getwid(&id) != Success)
		return rc.status;
	if((win = (EWindow *) malloc(sizeof(EWindow))) == NULL)
		return rcset(Panic,0,text94,"wsplit");
			// "%s(): Out of memory!"

	// Find row containing point (which is assumed to be in the window).
	nrowpt = 0;
	for(lnp = wfp->wf_toplnp; lnp != wfp->wf_point.lnp; lnp = lnp->l_next)
		++nrowpt;

	// Update some settings.
	++si.curbuf->b_nwind;			// Now displayed twice (or more).
	win->w_buf = si.curbuf;
	win->w_face = *wfp;			// For now.
	win->w_rfrow = 0;
	win->w_flags = (WFHard | WFMode);
	win->w_id = id;

	// Calculate new window sizes.
	nrowu = (si.curwin->w_nrows - 1) / 2;	// Upper window (default).
	if(n != INT_MIN) {
		if(n < 0) {
			if((nrowu += n) < 1)
				nrowu = 1;
			}
		else if(n > 0)
			nrowu = (n < si.curwin->w_nrows - 1) ? n : si.curwin->w_nrows - 2;
		}
	nrowl = (si.curwin->w_nrows - 1) - nrowu;	// Lower window.

	// Make new window the bottom one.
	win->w_next = si.curwin->w_next;
	si.curwin->w_next = win;
	si.curwin->w_nrows = nrowu;
	win->w_nrows = nrowl;
	win->w_toprow = si.curwin->w_toprow + nrowu + 1;

	// Adjust current window's top line if needed.
	if(nrowpt > nrowu)
		wfp->wf_toplnp = wfp->wf_toplnp->l_next;

	// Move down nrowu lines to find top line of lower window.  Stop if we slam into end-of-buffer.
	if(nrowpt != nrowu) {
		lnp = wfp->wf_toplnp;
		while(lnp->l_next != NULL) {
			lnp = lnp->l_next;
			if(--nrowu == 0)
				break;
			}
		}

	// Set top line and point line of each window as needed, keeping in mind that buffer may be empty (so top and point
	// point to the first line) or have just a few lines in it.  In the latter case, set top in the bottom window to the
	// last line of the buffer and point to same line, except for special case described below.
	if(nrowpt < si.curwin->w_nrows) {

		// Point is in old (upper) window.  Fixup new (lower) window.
		defupper = true;

		// Hit end of buffer looking for top?
		if(lnp->l_next == NULL) {

			// Yes, lines in window being split do not extend past the middle.
			win->w_face.wf_toplnp = lnp->l_prev;

			// Set point to last line (unless it is already there) so that it will be visible in the lower window.
			if(wfp->wf_point.lnp->l_next != NULL) {
				win->w_face.wf_point.lnp = si.curbuf->b_lnp->l_prev;
				win->w_face.wf_point.off = 0;
				}
			}
		else {
			// No, save current line as top and press onward to find spot to place point.
			win->w_face.wf_toplnp = lnp;
			nrowu = nrowl / 2;
			while(nrowu-- > 0) {
				if((lnp = lnp->l_next)->l_next == NULL)
					break;
				}
	
			// Set point line to mid-point of lower window or last line of buffer.
			win->w_face.wf_point.lnp = lnp;
			win->w_face.wf_point.off = 0;
			}
		}
	else {
		// Point is in new (lower) window.  Fixup both windows.
		defupper = false;

		// Set top line of lower window (point is already correct).
		win->w_face.wf_toplnp = lnp;

		// Set point in upper window to middle.
		nrowu = si.curwin->w_nrows / 2;
		lnp = wfp->wf_toplnp;
		while(nrowu-- > 0)
			lnp = lnp->l_next;
		wfp->wf_point.lnp = lnp;
		wfp->wf_point.off = 0;
		}

	// Both windows are now set up.  All that's left is to set the window-update flags, set the current window to the bottom
	// one if needed, and return the non-point window pointer if requested.
	si.curwin->w_flags |= (WFHard | WFMode);
	if((n != 0 && !defupper) || (n == 0 && defupper)) {
		si.curscr->s_curwin = win;
		win = si.curwin;
		si.curwin = si.curscr->s_curwin;
		}
	*winp = win;

	return rc.status;
	}

// Grow or shrink the current window.  If how == 0, set window size to abs(n) lines; otherwise, shrink (how < 0) or grow
// (how > 0) by abs(n) lines.  Find the window that loses or gains space and make sure the window that shrinks is big enough.
// If n < 0, try to use upper window; otherwise, lower.  If it's a go, set the window flags and let the redisplay system do all
// the hard work.  (Can't just set "force reframe" because point would move.)  Return status.
int gswind(Datum *rval,int n,int how) {
	EWindow *adjwp;
	bool grow = (how > 0);

	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		return rc.status;		// Nothing to do.

	if(si.whead->w_next == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Figure out which window (next or previous) to steal lines from or give lines to.
	adjwp = si.curwin->w_next;
	if(si.curwin != si.whead && (n < 0 || adjwp == NULL))
		adjwp = wnextis(si.curwin);

	if(n < 0)
		n = -n;
	if(how == 0) {

		// Want n-line window.  Convert n to a size adjustment.
		if(n > si.curwin->w_nrows) {
			n -= si.curwin->w_nrows;
			grow = true;
			}
		else if(n == si.curwin->w_nrows)
			return rc.status;	// Nothing to do.
		else {
			n = si.curwin->w_nrows - n;
			grow = false;
			}
		}

	if(grow) {
		// Adjacent window big enough?
		if(adjwp->w_nrows <= n)
			return rcset(Failure,0,text207,n,n == 1 ? "" : "s");
				// "Cannot get %d line%s from adjacent window"

		// Yes, proceed.
		if(si.curwin->w_next == adjwp) {		// Shrink below.
			wnewtop(adjwp,NULL,n);
			adjwp->w_toprow += n;
			}
		else {					// Shrink above.
			wnewtop(si.curwin,NULL,-n);
			si.curwin->w_toprow -= n;
			}
		si.curwin->w_nrows += n;
		adjwp->w_nrows -= n;
		}
	else {
		// Current window big enough?
		if(si.curwin->w_nrows <= n)
			return rcset(Failure,0,text93,n,n == 1 ? "" : "s");
				// "Current window too small to shrink by %d line%s"

		// Yes, proceed.
		if(si.curwin->w_next == adjwp) {		// Grow below.
			wnewtop(adjwp,NULL,-n);
			adjwp->w_toprow -= n;
			}
		else {					// Grow above.
			wnewtop(si.curwin,NULL,n);
			si.curwin->w_toprow += n;
			}
		si.curwin->w_nrows -= n;
		adjwp->w_nrows += n;
		}

	si.curwin->w_flags |= (WFHard | WFMode);
	adjwp->w_flags |= (WFHard | WFMode);

	return rc.status;
	}

// Resize the current window to the requested size.
int resizeWind(Datum *rval,int n,Datum **argv) {

	// Ignore if no argument or already requested size.
	if(n == INT_MIN || n == si.curwin->w_nrows)
		return rc.status;

	// Error if only one window.
	if(si.whead->w_next == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Set all windows on current screen to same size if n == 0; otherwise, change current window only.
	if(n == 0) {
		int count = wincount(si.curscr,NULL);
		int size = (term.t_nrow - 1) / count - 1;	// Minimum size of each window, excluding mode line.
		int leftover0 = (term.t_nrow - 1) % count;

		// Remember current window, then step through all windows repeatedly, setting new size of each if possible,
		// until all windows have been set successfully or become deadlocked (which can happen if there are numerous
		// tiny windows).  Give up in the latter case and call it good (a very rare occurrence).
		EWindow *oldwin = si.curwin;
		EWindow *win;
		int leftover;
		bool success,changed;
		do {
			win = si.whead;
			leftover = leftover0;
			success = true;
			changed = false;
			do {
				(void) wswitch(win,SWB_NoHooks);		// Can't fail.
				if(leftover > 0) {
					n = size + 1;
					--leftover;
					}
				else
					n = size;

				// Don't try to adjust bottom window.
				if(win->w_next != NULL && win->w_nrows != n) {
					if(win->w_nrows < n && win->w_next->w_nrows <= n - win->w_nrows) {

						// Current window needs to grow but next window is too small.  Take as many
						// rows as possible.
						success = false;
						if(win->w_next->w_nrows == 1)
							continue;
						n = win->w_nrows + win->w_next->w_nrows - 1;
						}
					if(gswind(rval,n,0) != Success)
						break;
					changed = true;
					}
				} while((win = win->w_next) != NULL);
			} while(!success && changed);

		// Adjustment loop completed... success or deadlocked.  Switch back to original window in either case.
		(void) wswitch(oldwin,0);
		}
	else
		(void) gswind(rval,n,0);

	return rc.status;
	}

// Determine the disposition of a buffer.  This routine is called by any command that creates or selects a buffer.  Once the
// command has the buffer (which may have just been created), it hands it off to this routine to figure out what to do with it.
// The action taken is determined by the value of "n" (which defaults to -1) and "flags".  Possible values of "n" are:
//
//	< -2		Display buffer in a different window (possibly new) and switch to that window.
//	-2		Display buffer in a different window (possibly new), but stay in current window.
//	-1		Pop buffer with RendAltML and RendShift options.
//	0		Leave buffer as is.
//	1		Switch to buffer in current window.
//	2		Display buffer in a new window, but stay in current window.
//	> 2		Display buffer in a new window and switch to that window.
//
// Flags are:
//	RendNewBuf	Buffer was just created.
//	RendReset	Move point to beginning of buffer and unhide it if displaying in another window ("show" command).
//	RendAltML	Display the alternate mode line when doing a real pop-up.
//	RendShift	Shift long lines left when doing a real pop-up.
//
// Possible return values are listed below.  The new-buf? value is true if RendNewBuf flag set; otherwise, false:
//	n == -1:		buf-name (or nil if buffer deleted)
//	n == 0:			[buf-name,new-buf?]
//	Other n:		[buf-name,new-buf?,targ-wind-num,new-wind?]
//
// Notes:
//	* A buffer may be in the background or displayed in a window when this routine is called.  In either case, it is left as
//	  is if n == 0.
//	* If n < -1:
//		- The buffer will be displayed in another window even if it is displayed in the current window.
//		- If the buffer is already being displayed in another window, that window will be used as the target window.
//		- If the buffer is not being displayed, the window immediately above the current window is the first choice.
int render(Datum *rval,int n,Buffer *buf,ushort flags) {
	EWindow *win = NULL;
	bool newWind = false;

	if(n == INT_MIN)
		n = -1;

	// Displaying buffer?
	if(n != 0) {
		// Yes.  Popping buffer?
		if(n == -1) {
			// Yes.  Activate buffer if needed and do a real pop-up.
			if(bactivate(buf) != Success || bpop(buf,flags | RendWait) != Success)
				return rc.status;
			}

		// Not popping buffer.  Switch to it?
		else if(n == 1) {
			if(buf != si.curbuf && bswitch(buf,0) != Success)
				return rc.status;
			win = si.curwin;
			}

		// No, displaying buffer in another window.
		else {
			EWindow *oldwin;
			ushort flags;

			if(n > 1 || si.whead->w_next == NULL) {	// If force-creating or only one window...
				if(wsplit(INT_MIN,&win) != Success)	// split current window and get new one into win.
					return rc.status;
				newWind = true;
				}
			else if((win = whasbuf(buf,true)) != NULL) {	// Otherwise, find a different window, giving
				if(n == -2) {				// preference to one already displaying the buffer...
					(void) rcset(Success,0,text28,text58);
						// "%s is being displayed","Buffer"
					goto Retn;
					}
				}
			else if((win = wnextis(si.curwin)) == NULL)	// or the window above, if none found.
				win = si.curwin->w_next;
			oldwin = si.curwin;				// save old window...
			flags = (abs(n) == 2) ? SWB_NoHooks : 0;	// set wswitch() flag...
			if(wswitch(win,flags) != Success ||		// make new one current...
			 bswitch(buf,flags) != Success)		// and switch to new buffer.
				return rc.status;
			if(flags & RendReset)
				faceinit(&win->w_face,buf->b_lnp,NULL);
			if(flags != 0)					// If not a force to new window...
				(void) wswitch(oldwin,flags);		// switch back to previous one (can't fail).
			}
		}
Retn:
	// Wrap up and set return value.
	if(n == -1) {
		if(flags & RendNewBuf) {
			if(bdelete(buf,0) != Success)
				return rc.status;
			buf = NULL;
			dsetnil(rval);
			}
		else {
			if(dsetstr(buf->b_bname,rval) != 0)
				goto LibFail;
			}
		}
	else {
		Array *ary;

		if((ary = anew(n == 0 ? 2 : 4,NULL)) == NULL || dsetstr(buf->b_bname,ary->a_elp[0]) != 0)
			goto LibFail;
		else {
			dsetbool(flags & RendNewBuf,ary->a_elp[1]);
			if(n != 0) {
				if(win == NULL)
					dsetnil(ary->a_elp[2]);
				else
					dsetint((long) getwnum(win),ary->a_elp[2]);
				dsetbool(newWind,ary->a_elp[3]);
				}
			if(awrap(rval,ary) != Success)
				return rc.status;
			}

		if((flags & RendNotify) || (n == 0 && (flags & RendNewBuf)))
			(void) rcset(Success,0,text381,buf->b_bname);
				// "Buffer '%s' created"
		}
	if(buf != NULL && (flags & RendReset))
		buf->b_flags &= ~BFHidden;
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Scroll the previous or next window up (backward) or down (forward) a page.
int wscroll(Datum *rval,int n,int (*winfunc)(Datum *rval,int n,Datum **argv),int (*pagefunc)(Datum *rval,int n,Datum **argv)) {

	(void) winfunc(rval,INT_MIN,NULL);
	(void) pagefunc(rval,n,NULL);
	(void) (*(winfunc == prevWind ? nextWind : prevWind))(rval,INT_MIN,NULL);

	return rc.status;
	}

#if MMDebug & (Debug_ScrDump | Debug_Narrow | Debug_Temp)
// Return line information.
static char *lninfo(char *label,Line *lnp) {
	static char wkbuf[96];

	if(lnp == NULL)
		sprintf(wkbuf,"%s [00000000]: NULL",label);
	else if(lnp->l_used <= 20)
		sprintf(wkbuf,"%s [%.08X]: '%.*s'",label,(uint) lnp,lnp->l_used,lnp->l_text);
	else
		sprintf(wkbuf,"%s [%.08X]: '%.20s...'",label,(uint) lnp,lnp->l_text);
	return wkbuf;
	}

// Write buffer information to log file.
void dumpbuffer(char *label,Buffer *buf,bool withData) {
	Mark *mark;
	char wkbuf[16];
	static char s[] = "%s\n";

	if(buf == NULL)
		buf = si.curbuf;
	if(label != NULL)
		fprintf(logfile,"*** In %s...\n",label);
	fprintf(logfile,"Buffer '%s' [%.08X]:\n",buf->b_bname,(uint) buf);
	fprintf(logfile,s,lninfo("\tb_lnp",buf->b_lnp));
	fprintf(logfile,s,lninfo("\tb_lnp->l_prev",buf->b_lnp->l_prev));
	fprintf(logfile,s,lninfo("\tb_lnp->l_prev->l_next",buf->b_lnp->l_prev->l_next));
	fprintf(logfile,s,lninfo("\tb_ntoplnp",buf->b_ntoplnp));
		if(buf->b_ntoplnp != NULL)
			fprintf(logfile,s,lninfo("\t\tb_ntoplnp->l_prev",buf->b_ntoplnp->l_prev));
	fprintf(logfile,s,lninfo("\tb_nbotlnp",buf->b_nbotlnp));
		if(buf->b_nbotlnp != NULL)
			fprintf(logfile,s,lninfo("\t\tb_nbotlnp->l_prev",buf->b_nbotlnp->l_prev));

	fprintf(logfile,s,lninfo("\tb_face.wf_toplnp",buf->b_face.wf_toplnp));
	fprintf(logfile,"\t%s\n\tb_face.wf_point.off: %d\n",
	 lninfo("b_face.wf_point.lnp",buf->b_face.wf_point.lnp),buf->b_face.wf_point.off);
	fputs("\tMarks:\n",logfile);
	mark = &buf->b_mroot;
	do {
		if(mark->mk_id <= '~')
			sprintf(wkbuf,"%c",(int) mark->mk_id);
		else
			sprintf(wkbuf,"%.4hX",mark->mk_id);
		fprintf(logfile,"\t\t'%s': (%s), mk_point.off %d, mk_rfrow %hd\n",
		 wkbuf,lninfo("mk_point.lnp",mark->mk_point.lnp),mark->mk_point.off,mark->mk_rfrow);
		} while((mark = mark->mk_next) != NULL);

	if(buf->b_mip == NULL)
		strcpy(wkbuf,"NULL");
	else
		sprintf(wkbuf,"%hu",buf->b_mip->mi_nexec);
	fprintf(logfile,
	 "\tb_face.wf_firstcol: %d\n\tb_nwind: %hu\n\tb_nexec: %s\n\tb_nalias: %hu\n\tb_flags: %.4X\n\tb_modes:",
	 buf->b_face.wf_firstcol,buf->b_nwind,wkbuf,buf->b_nalias,(uint) buf->b_flags);
	if(buf->b_modes == NULL)
		fputs(" NONE",logfile);
	else {
		BufMode *bmp = buf->b_modes;
		do
			fprintf(logfile," %s",bmp->bm_mode->ms_name);
			while((bmp = bmp->bm_next) != NULL);
		}
	if(buf->b_fname == NULL)
		strcpy(wkbuf,"NULL");
	else
		sprintf(wkbuf,"'%s'",buf->b_fname);
	fprintf(logfile,"\n\tb_inpdelim: %.2hX %.2hX (%d)\n\tb_fname: %s\n",
	 (ushort) buf->b_inpdelim[0],(ushort) buf->b_inpdelim[1],buf->b_inpdelimlen,wkbuf);

	if(withData) {
		Line *lnp = buf->b_lnp;
		uint n = 0;
		fputs("\tData:\n",logfile);
		do {
			sprintf(wkbuf,"\t\tL%.4u",++n);
			fprintf(logfile,s,lninfo(wkbuf,lnp));
			} while((lnp = lnp->l_next) != NULL);
		}
	}
#endif

#if MMDebug & Debug_ScrDump
// Write window information to log file.
static void dumpwindow(EWindow *win,int windnum) {

	fprintf(logfile,"\tWindow %d [%.08x]:\n\t\tw_next: %.08x\n\t\tw_buf: %.08x '%s'\n%s\n",
	 windnum,(uint) win,(uint) win->w_next,(uint) win->w_buf,win->w_buf->b_bname,
	 lninfo("\t\tw_face.wf_toplnp",win->w_face.wf_toplnp));
	fprintf(logfile,"%s\n\t\tw_face.wf_point.off: %d\n\t\tw_toprow: %hu\n\t\tw_nrows: %hu\n\t\tw_rfrow: %hu\n",
	 lninfo("\t\tw_face.wf_point.lnp",win->w_face.wf_point.lnp),
	 win->w_face.wf_point.off,win->w_toprow,win->w_nrows,win->w_rfrow);
	fprintf(logfile,"\t\tw_flags: %.04x\n\t\tw_face.wf_firstcol: %d\n",
	 (uint) win->w_flags,win->w_face.wf_firstcol);
	}

// Write screen, window, and buffer information to log file -- for debugging.
void dumpscreens(char *msg) {
	EScreen *scr;
	EWindow *win;
	Array *ary;
	Datum *el;
	int windnum;

	fprintf(logfile,"### %s ###\n\n*SCREENS\n\n",msg);

	// Dump screens and windows.
	scr = si.shead;
	do {
		fprintf(logfile,"Screen %hu [%.08x]:\n\ts_flags: %.04x\n\ts_nrow: %hu\n\ts_ncol: %hu\n\ts_curwin: %.08x\n",
		 scr->s_num,(uint) scr,(uint) scr->s_flags,scr->s_nrow,scr->s_ncol,(uint) scr->s_curwin);
		win = scr->s_whead;
		windnum = 0;
		do {
			dumpwindow(win,++windnum);
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);

	// Dump buffers.
	fputs("\n*BufferS\n\n",logfile);
	ary = &buftab;
	while((el = aeach(&ary)) != NULL)
		dumpbuffer(NULL,bufptr(el),false);
	}
#endif

// Get number of screens.
int scrcount(void) {
	EScreen *scr = si.shead;
	int count = 0;
	do {
		++count;
		} while((scr = scr->s_next) != NULL);
	return count;
	}

// Find a screen, given number, (possibly NULL) pointer to buffer to attach to first window of screen, and (possibly NULL)
// pointer to result.  If the screen is not found and "buf" is not NULL, create new screen and return status; otherwise, return
// false, ignoring spp.
int sfind(ushort scr_num,Buffer *buf,EScreen **spp) {
	EScreen *scr1;
	ushort snum;
	static char myname[] = "sfind";

	// Scan the screen list.  Note that the screen list is empty at program launch.
	for(snum = 0, scr1 = si.shead; scr1 != NULL; scr1 = scr1->s_next) {
		if((snum = scr1->s_num) == scr_num) {
			if(spp)
				*spp = scr1;
			return (buf == NULL) ? true : rc.status;
			}
		}

	// No such screen exists, create new one?
	if(buf != NULL) {
		ushort id;
		EScreen *scr2;
		EWindow *win;		// Pointer to first window of new screen.

		// Get unique window id.
		if(getwid(&id) != Success)
			return rc.status;

		// Allocate memory for screen.
		if((scr1 = (EScreen *) malloc(sizeof(EScreen))) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"

		// Set up screen fields.
		scr1->s_lastbuf = NULL;
		scr1->s_num = snum + 1;
		scr1->s_flags = 0;
		scr1->s_wkdir = NULL;
		if(setwkdir(scr1) != Success)
			return rc.status;			// Fatal error.
		scr1->s_nrow = term.t_nrow;
		scr1->s_ncol = term.t_ncol;
		scr1->s_cursrow = scr1->s_curscol = scr1->s_firstcol = 0;

		// Allocate its first window...
		if((win = (EWindow *) malloc(sizeof(EWindow))) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"
		scr1->s_whead = scr1->s_curwin = win;

		// and set up the window's info.
		win->w_next = NULL;
		win->w_buf = buf;
		++buf->b_nwind;
		bftowf(buf,win);
		win->w_id = id;
		win->w_toprow = 0;
		win->w_nrows = term.t_nrow - 2;		// "-2" for message and mode lines.
		win->w_rfrow = 0;

		// Insert new screen at end of screen list.
		scr1->s_next = NULL;
		if((scr2 = si.shead) == NULL)
			si.shead = scr1;
		else {
			while(scr2->s_next != NULL)
				scr2 = scr2->s_next;
			scr2->s_next = scr1;
			}

		// and return the new screen pointer.
		if(spp)
			*spp = scr1;
		return rc.status;
		}

	// Screen not found and buf is NULL.
	return false;
	}

// Switch to given screen.  Return status.
int sswitch(EScreen *scr,ushort flags) {

	// Nothing to do if it is already current.
	if(scr == si.curscr)
		return rc.status;

	// Save the current screen's concept of current window.
	si.curscr->s_curwin = si.curwin;
	si.curscr->s_nrow = term.t_nrow;
	si.curscr->s_ncol = term.t_ncol;

	// Run exit-buffer user hook on current (old) buffer if the new screen's buffer is different.
	Datum *rval = NULL;
	bool diffbuf;
	if(!(flags & SWB_NoHooks)) {
		diffbuf = (scr->s_curwin->w_buf != si.curbuf);
		if(diffbuf && bhook(&rval,SWB_ExitHook) != Success)
			return rc.status;
		}

	// Change current directory if needed.
	if(strcmp(scr->s_wkdir,si.curscr->s_wkdir) != 0)
		(void) chgdir(scr->s_wkdir);

	// Reset the current screen, window and buffer.
	si.whead = (si.curscr = scr)->s_whead;
	si.curbuf = (si.curwin = scr->s_curwin)->w_buf;

	// Let the display driver know we need a full screen update.
	si.opflags |= OpScrRedraw;

	// Run enter-buffer user hook on current (new) buffer.
	if(!(flags & SWB_NoHooks) && diffbuf && rc.status == Success)
		(void) bhook(&rval,0);

	return rc.status;
	}

// Bring a screen to the front per flags.  Return status.
int gotoScreen(int n,ushort flags) {

	if(n != 0 || !(flags & SWB_Repeat)) {
		EScreen *scr;
		int scrct = scrcount();		// Total number of screens.
		char olddir[strlen(si.curscr->s_wkdir) + 1];

		// Check if n is out of range.
		if(flags & SWB_Repeat) {
			if(n == INT_MIN)
				n = 1;
			else if(n < 0)
				return rcset(Failure,0,text39,text137,n,0);
						// "%s (%d) must be %d or greater","Repeat count"

			if(scrct == 1 || (n %= scrct) == 0)	// If only one screen or repeat count equals screen count...
				return rc.status;		// nothing to do.
			if(flags & SWB_Forw) {
				if((n += si.curscr->s_num) > scrct)
					n -= si.curscr->s_num;
				}
			else if((n = si.curscr->s_num - n) < 1)
				n += scrct;
			}
		else if(n <= 0 || abs(n) > scrct)
			return rcset(Failure,0,text240,n);
					// "No such screen '%d'"

		// n is now the target screen number.
		scr = si.shead;					// Find the screen...
		while(scr->s_num != n)
			scr = scr->s_next;
		strcpy(olddir,si.curscr->s_wkdir);			// save current directory...
		if(sswitch(scr,0) == Success)				// make new screen current...

			// and display its working directory if interactive, 'WkDir' global mode not enabled, and changed.
			if(!(si.opflags & OpScript) && !(mi.cache[MdIdxWkDir]->ms_flags & MdEnabled) &&
			 strcmp(si.curscr->s_wkdir,olddir) != 0)
				rcset(Success,RCHigh | RCNoFormat | RCNoWrap,scr->s_wkdir);
		}
	return rc.status;
	}

// Switch to given screen, or create one if screen number is zero.  Return status.
int selectScreen(Datum *rval,int n,Datum **argv) {

	// Get screen number.
	if(getnum(text113,1,&n) != Success)
			// "Switch to"
		return rc.status;

	// Create screen?
	if(n == 0) {
		EScreen *scr;

		// Yes.  Save current screen number and current window's settings.
		wftobf(si.curwin,si.curbuf);

		// Find screen "0" to force-create one and make it current.
		if(sfind(0,si.curbuf,&scr) != Success || sswitch(scr,0) != Success)
			return rc.status;
		(void) rcset(Success,0,text174,scr->s_num);
				// "Created screen %hu"
		}
	else
		// Not creating... switch to one specified.
		(void) gotoScreen(n,0);

	return rc.status;
	}

// Free all resouces associated with a screen.
static void freescreen(EScreen *scr) {
	EWindow *win,*tp;

	// First, free the screen's windows...
	win = scr->s_whead;
	do {
		if(--win->w_buf->b_nwind == 0)
			win->w_buf->b_lastscr = NULL;
		wftobf(win,win->w_buf);

		// On to the next window; free this one.
		tp = win;
		win = win->w_next;
		(void) free((void *) tp);
		} while(win != NULL);

	// free the working directory...
	(void) free((void *) scr->s_wkdir);

	// and lastly, free the screen itself.
	(void) free((void *) scr);
	}

// Remove screen from the list and renumber remaining ones.  Update modeline of bottom window if only one left or current screen
// number changes.  Return status.
static int unlistscreen(EScreen *scr) {
	ushort snum0,snum;
	EScreen *tp;

	snum0 = si.curscr->s_num;
	if(scr == si.shead)
		si.shead = si.shead->s_next;
	else {
		tp = si.shead;
		do {
			if(tp->s_next == scr)
				goto Found;
			} while((tp = tp->s_next) != NULL);

		// Huh?  Screen not found... this is a bug.
		return rcset(FatalError,0,text177,"unlistscreen",(int) scr->s_num);
				// "%s(): Screen number %d not found in screen list!"
Found:
		tp->s_next = scr->s_next;
		}

	// Renumber screens.
	snum = 0;
	tp = si.shead;
	do {
		tp->s_num = ++snum;
		} while((tp = tp->s_next) != NULL);

	// If only one screen left or current screen number changed, flag mode line at bottom of screen for update.
	if(snum == 1 || si.curscr->s_num != snum0) {

		// Flag last window.
		wnextis(NULL)->w_flags |= WFMode;
		}

	return rc.status;
	}

// Delete a screen.  Return status.
int delScreen(Datum *rval,int n,Datum **argv) {
	EScreen *scr;

	// Get the number of the screen to delete.  Error if only one exists.
	if(si.shead->s_next == NULL)
		return rcset(Failure,RCNoFormat,text57);
					// "Only one screen"
	if(getnum(text26,-1,&n) != Success)
			// "Delete"
		return rc.status;

	// Make sure it exists.
	if(!sfind((ushort) n,NULL,&scr))
		return rcset(Failure,0,text240,n);
			// "No such screen '%d'"

	// Switch screens if deleting current one.
	if(scr == si.curscr && sswitch(scr == si.shead ? scr->s_next : si.shead,0) != Success)
		return rc.status;

	// Everything's cool... nuke it.
	nukebufsp(scr);
	if(unlistscreen(scr) != Success)
		return rc.status;
	freescreen(scr);

	return rcset(Success,0,text178,n);
			// "Screen %d deleted"
	}

// Check if terminal supports color.  If yes, return true; otherwise, set an error and return false.
bool haveColor(void) {

	if(si.opflags & OpHaveColor)
		return true;
	(void) rcset(Failure,RCNoFormat,text427);
		// "Terminal does not support color"
	return false;
	}

// Assign a foreground and background color to a color pair number.  If pair number is zero, use next one available.  Set rval
// to pair number assigned and return status.
int setColorPair(Datum *rval,int n,Datum **argv) {

	if(!haveColor())
		return rc.status;

	short pair,colors[2],*cp = colors,*cpz = colors + 2;
	long c;

	// Get arguments and validate them.
	if((c = (*argv++)->u.d_int) != 0)
		if(c < 1 || c > term.maxWorkPair)
			return rcset(Failure,0,text425,c,term.maxWorkPair);
				// "Color pair %ld must be between 0 and %hd"
	pair = c;
	do {
		if((c = (*argv++)->u.d_int) < -1 || c > term.maxColor)
			return rcset(Failure,0,text426,c,term.maxColor);
				// "Color no. %ld must be between -1 and %hd"
		*cp++ = c;
		} while(cp < cpz);

	// Have arguments.  Get next pair number if needed.
	if(pair == 0) {
		pair = term.nextPair++;
		if(term.nextPair > term.maxWorkPair)
			term.nextPair = 1;
		}

	// Create color pair and return it.
	(void) init_pair(pair,colors[0],colors[1]);
	dsetint(pair,rval);
	return rc.status;
	}

// Set color for a display item.
int setDispColor(Datum *rval,int n,Datum **argv) {
	ItemColor *ctab,*ctabz;
	short colors[2];
	bool changed = false;

	// Check keyword (first argument).
	ctabz = (ctab = term.itemColor) + sizeof(term.itemColor) / sizeof(ItemColor);
	do {
		if(strcasecmp((*argv)->d_str,ctab->name) == 0)
			goto Found;
		} while(++ctab < ctabz);

	// Match not found.
	return rcset(Failure,0,text447,text443,(*argv)->d_str);
			// "Invalid %s '%s'","display item"
Found:
	// Match found.  Check color numbers.
	if(argv[1]->d_type == dat_nil) {
		if(ctab->colors[0] != -2) {
			ctab->colors[0] = ctab->colors[1] = -2;
			changed = true;
			}
		}
	else if(!haveColor())
		return rc.status;
	else {
		Datum **elp,**elpz,*el;
		long c;
		short *cp = colors;
		Array *ary = awptr(argv[1])->aw_ary;

		if(ary->a_used != 2)
			goto Err;
		elpz = (elp = ary->a_elp) + 2;
		do {
			el = *elp++;
			if(el->d_type != dat_int || (c = el->u.d_int) < -1 || c > term.maxColor)
				goto Err;
			*cp++ = c;
			} while(elp < elpz);
		if(colors[0] != ctab->colors[0] || colors[1] != ctab->colors[1]) {
			ctab->colors[0] = colors[0];
			ctab->colors[1] = colors[1];
			changed = true;
			}
		}

	// Success.  If a change was made, implement it.
	if(changed)
		switch(ctab - term.itemColor) {
			case ColorIdxML:
				supd_wflags(NULL,WFMode);

				// Last color pair is reserved for mode line.
				if(colors[0] >= -1)
					(void) init_pair(term.maxPair - ColorPairML,colors[0],colors[1]);
				break;
			case ColorIdxKMRI:

				// Second to last color pair is reserved for keyboard macro recording indicator.
				if(colors[0] >= -1)
					(void) init_pair(term.maxPair - ColorPairKMRI,colors[0],colors[1]);
			}
	return rc.status;
Err:
	return rcset(Failure,RCNoFormat,text422);
		// "Invalid color pair value(s)"
	}
