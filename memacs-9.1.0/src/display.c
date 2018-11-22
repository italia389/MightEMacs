// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// display.c	High level display routines for MightEMacs.
//
// This file contains functions that generally call the lower level terminal display functions in vterm.c.

#include "os.h"
#include <stdarg.h>
#include <ctype.h>
#include "std.h"
#include "exec.h"

// Set given flags on all windows on current screen.  If bufp is not NULL, mark only those windows.
void supd_wflags(Buffer *bufp,ushort flags) {

	if(bufp == NULL || bufp->b_nwind > 0) {
		EWindow *winp;

		// Only need to update windows on current screen because windows on background screens are automatically
		// updated when the screen to brought to the foreground.
		winp = si.wheadp;
		do {
			if(bufp == NULL || winp->w_bufp == bufp)
				winp->w_flags |= flags;
			} while((winp = winp->w_nextp) != NULL);
		}
	}

// Find window on current screen whose w_nextp matches given pointer and return it, or NULL if not found (winp is top window).
EWindow *wnextis(EWindow *winp) {
	EWindow *winp1 = si.wheadp;

	if(winp == winp1)
		return NULL;			// No window above top window.
	while(winp1->w_nextp != winp)
		winp1 = winp1->w_nextp;
	return winp1;
	}

// Find window on current screen displaying given buffer and return pointer to it, or NULL if not found.  Do not consider
// current window if skipCur is true.
EWindow *whasbuf(Buffer *bufp,bool skipCur) {
	EWindow *winp = si.wheadp;
	do {
		if(winp->w_bufp == bufp && (winp != si.curwp || !skipCur))
			return winp;
		} while((winp = winp->w_nextp) != NULL);
	return NULL;
	}

// Move cursor to column "col" in message line.  Return status.
int mlmove(int col) {

	return movecursor(term.t_nrow - 1,col);
	}

// Restore message line cursor position.  Return status.
int mlrestore(void) {

	if(mlmove(si.mlcol) == Success)
		(void) TTflush();
	return rc.status;
	}

// Erase the message line.  Return status.
int mlerase(void) {
	// Home the cursor.
	int i = si.mlcol;
	if(movecursor(term.t_nrow - 1,0) != Success)
		return rc.status;

	// Erase line if needed.
	if(i != 0 && TTeeol() == Success)
		(void) TTflush();

	return rc.status;
	}

// Write a character to the message line with invisible characters exposed, unless MLRaw flag is set.  Keep track of the
// physical cursor position so that a LineExt ($) can be displayed at the right edge of the screen if the cursor moves past it
// (unless MLNoEOL flag is set).  Return status.
int mlputc(ushort flags,short c) {

	// Nothing to do if past right edge of screen.
	if(si.mlcol >= term.t_ncol)
		return rc.status;

	// Raw or plain character?
	if((flags & MLRaw) || (c >= ' ' && c <= '~')) {
		int n;
		bool skipSpace = (c == ' ' && (flags & MLNoSpace));

		// Yes, display it.
		if(c == '\b')
			n = -1;
		else {
			n = 1;
			if(skipSpace && TTul(false) != Success)
				return rc.status;
			if(si.mlcol == term.t_ncol - 1 && !(flags & MLNoEOL))
				c = LineExt;
			}
		if(TTputc(c) != Success)
			return rc.status;
		si.mlcol += n;
		if(skipSpace)
			(void) TTul(true);
		}
	else {
		char *str;

		// Not raw.  Display char literal (if any) and remember cursor position until it reaches the right edge.
		c = *(str = vizc(c,VBaseDef));
		do {
			if(TTputc(si.mlcol == term.t_ncol - 1 && !(flags & MLNoEOL) ? LineExt : c) != Success)
				break;
			} while(++si.mlcol < term.t_ncol && (c = *++str) != '\0');
		}

	return rc.status;
	}

// Prepare for new message line message.  If MLHome flag is set, clear message line.  If MLWrap flag is set, write leading '['
// of message-wrap characters "[]".  Return status.
static int mlbegin(ushort flags) {

	// Position cursor and/or begin wrap, if applicable.
	if(((flags & MLHome) || si.mlcol < 0 || si.mlcol >= term.t_ncol) && mlerase() != Success)
		return rc.status;
	if((flags & (MLHome | MLWrap)) == (MLHome | MLWrap))
		(void) mlputc(flags | MLRaw,'[');

	return rc.status;
	}

// Finish message line message.  Return status.
static int mlend(ushort flags) {

	// Finish wrap and flush message.
	if(flags & MLFlush)
		if(!(flags & MLWrap) || mlputc(flags | MLRaw,']') == Success)
			(void) TTflush();
	return rc.status;
	}

// Write a string to the message line, given flags and message.  Return status.
static int mlputstr(ushort flags,char *str) {
	short c;

	while((c = *str++) != '\0')
		if(mlputc(flags,c) != Success || si.mlcol >= term.t_ncol)
			break;

	return rc.status;
	}

// Process a terminal attribute specification.  Return status.
static int mlattr(ushort *flagsp,char **fmtp) {
	short c;
	bool attrOn;
	bool altForm = false;

	if((c = *(*fmtp)++) == AttrAlt) {
		altForm = true;
		c = *(*fmtp)++;
		}
	else if(c == AttrSeqBegin)
		return mlputc(*flagsp,c);

	switch(c) {
		case AttrAllOff:
			*flagsp &= ~MLNoSpace;
			(void) TTattroff();
			break;
		case AttrBoldOn:
#if !TT_Curses
			(void) TTbold();
#else
			attrOn = true;
			goto Bold;
		case AttrBoldOff:
			attrOn = false;
Bold:
			(void) TTbold(attrOn);
#endif
			break;
		case AttrRevOn:
#if !TT_Curses
			(void) TTrev();
#else
			attrOn = true;
			goto Rev;
		case AttrRevOff:
			attrOn = false;
Rev:
			(void) TTrev(attrOn);
#endif
			break;
		case AttrULOn:
			attrOn = true;
			if(altForm)
				*flagsp |= MLNoSpace;
			goto UL;
		case AttrULOff:
			attrOn = false;
			*flagsp &= ~MLNoSpace;
UL:
			(void) TTul(attrOn);
			break;
		default:
			(void) TTattroff();
			(void) rcset(Failure,0,text321,(int) AttrSeqBegin,c == '\0' ? "" : vizc(c,VBaseDef));
				// "Unknown format spec '%c%s'"
		}

	return rc.status;
	}

// Write a string to the message line, given flags and message.  If MLTermAttr flag is set, terminal attribute sequences
// (beginning with '~') in message are processed.  Return status.
int mlputs(ushort flags,char *str) {
	short c;

	if(mlbegin(flags) != Success)
		return rc.status;

	// Copy string to message line.  If MLTermAttr flag is set, scan for attribute sequences and process them.
	while((c = *str++) != '\0') {
		if((flags & MLTermAttr) && c == AttrSeqBegin) {
			if(mlattr(&flags,&str) != Success)
				return rc.status;
			}
		else if(mlputc(flags,c) != Success)
			return rc.status;
		}

	if(TTattroff() == Success)
		(void) mlend(flags);
	return rc.status;
	}

// Write out an integer, in the specified radix.  Update the physical cursor position.  Return status.
static int mlputi(ushort flags,int i,int r) {
	int q;
	long l = (uint) i;
	static char hexdigits[] = "0123456789abcdef";

	if(i < 0 && r == 10) {
		l = -i;
		if(mlputc(flags | MLRaw,'-') != Success)
			return rc.status;
		}
	q = l / r;
	if(q != 0 && mlputi(flags,q,r) != Success)
		return rc.status;
	return mlputc(flags | MLRaw,hexdigits[l % r]);
	}

#if MLExtend
// Do the same for a long integer.
static int mlputli(ushort flags,long l,int r) {
	long q;

	if(l < 0) {
		l = -l;
		if(mlputc(flags | MLRaw,'-') != Success)
			return rc.status;
		}
	q = l / r;
	if(q != 0 && mlputli(flags,q,r) != Success)
		return rc.status;
	return mlputc(flags | MLRaw,(l % r) + '0');
	}
#endif

// Write text into the message line, given flag word, format string and optional arguments.  Return status.
//
// A small number of printf format specifications are handled.  If the MLTermAttr flag is set, terminal attribute specifications
// are also recognized, which enable/disable bold, reverse video, underline, and disable all attributes at the location where
// they are encountered, without consuming arguments.
int mlprintf(ushort flags,char *fmt,...) {
	short c;		// Current char in format string.
	va_list ap;
	int arg_int;		// Integer argument.
#if MLExtend
	long arg_long;		// Long argument.
#endif
	char *arg_str;		// String argument.

	// Bag it if not currently echoing and not a force.
	if(mlbegin(flags) != Success)
		return rc.status;

	// Process arguments.
	va_start(ap,fmt);
	while((c = *fmt++) != '\0') {
		if((flags & MLTermAttr) && c == AttrSeqBegin)
			(void) mlattr(&flags,&fmt);
		else if(c != '%')
			(void) mlputc(flags,c);
		else {
			switch(c = *fmt++) {
				case 's':
					arg_str = va_arg(ap,char *);
					(void) mlputstr(flags,arg_str);
					break;
				case 'd':
					arg_int = va_arg(ap,int);
					(void) mlputi(flags,arg_int,10);
					break;
#if MLExtend
				case 'c':
#else
				default: // 'c'
#endif
					c = va_arg(ap,int);
					(void) mlputc(flags,c);
#if MLExtend
					break;
				case 'o':
					arg_int = va_arg(ap,int);
					(void) mlputi(flags,arg_int,8);
					break;
				case 'x':
					arg_int = va_arg(ap,int);
					(void) mlputi(flags,arg_int,16);
					break;
				default: // 'D'
					arg_long = va_arg(ap,long);
					(void) mlputli(flags,arg_long,10);
#endif
				}
			}
		if(rc.status != Success)
			return rc.status;
		}
	va_end(ap);
	return mlend(flags);
	}

// Initialize point position, marks, and first column position of a face record, given line pointer.  If bufp not NULL, clear
// its buffer marks.
void faceinit(WindFace *wfp,Line *lnp,Buffer *bufp) {

	wfp->wf_toplnp = wfp->wf_dot.lnp = lnp;
	wfp->wf_dot.off = wfp->wf_firstcol = 0;

	// Clear mark(s).
	if(bufp != NULL)
		mdelete(bufp,MkOpt_Viz | MkOpt_Wind);
	}

// Copy buffer face record to a window.
void bftowf(Buffer *bufp,EWindow *winp) {

	winp->w_face = bufp->b_face;
	winp->w_flags |= (WFHard | WFMode);
	}

// Copy window face record to a buffer and clear the "buffer face in unknown state" flag.
void wftobf(EWindow *winp,Buffer *bufp) {

	bufp->b_face = winp->w_face;
	}

// Get ordinal number of given window, beginning at 1.
int getwnum(EWindow *winp) {
	EWindow *winp1 = si.wheadp;
	int num = 1;
	while(winp1 != winp) {
		winp1 = winp1->w_nextp;
		++num;
		}
	return num;
	}

// Get number of windows on given screen.  If wnump not NULL, set *wnump to screen's current window number.
int wincount(EScreen *scrp,int *wnump) {
	EWindow *winp = scrp->s_wheadp;
	int count = 0;
	do {
		++count;
		if(wnump != NULL && winp == scrp->s_curwp)
			*wnump = count;
		} while((winp = winp->w_nextp) != NULL);
	return count;
	}

// Move up or down n lines (if possible) from given window line (or current top line of window if lnp is NULL) and set top line
// of window to result.  If n is negative, move up; if n is positive, move down.  Set hard update flag in window and return true
// if top line was changed; otherwise, false.
bool wnewtop(EWindow *winp,Line *lnp,int n) {
	Line *oldtoplnp = winp->w_face.wf_toplnp;
	if(lnp == NULL)
		lnp = oldtoplnp;

	if(n < 0) {
		while(lnp != winp->w_bufp->b_lnp) {
			lnp = lnp->l_prevp;
			if(++n == 0)
				break;
			}
		}
	else if(n > 0) {
		while(lnp->l_nextp != NULL) {
			lnp = lnp->l_nextp;
			if(--n == 0)
				break;
			}
		}

	if((winp->w_face.wf_toplnp = lnp) != oldtoplnp) {
		winp->w_flags |= (WFHard | WFMode);
		return true;
		}
	return false;
	}

// Reset terminal.  Get the current terminal dimensions, update the ETerm structure, flag all screens that have different
// dimensions for a "window resize", and flag current screen for a "redraw".  Force update if n > 0.  Return status.  Needs to
// be called when the size of the terminal window changes; for example, when switching from portrait to landscape viewing on a
// mobile device.
int resetTermc(Datum *rp,int n,Datum **argpp) {
	EScreen *scrp;		// Current screen being examined.
	ushort ncol,nrow;	// Current terminal dimensions.

	// Get current terminal size.
	if(gettermsize(&ncol,&nrow) != Success)
		return rc.status;

	// In all screens...
	scrp = si.sheadp;
	n = (n <= 0) ? 0 : 1;
	do {
		// Flag screen if its not the current terminal size.
		if(scrp->s_nrow != nrow || scrp->s_ncol != ncol) {
			scrp->s_flags |= EScrResize;
			n = 1;
			}
		} while((scrp = scrp->s_nextp) != NULL);

	// Perform update?
	if(n) {
		// Yes, update ETerm settings.
		settermsize(ncol,nrow);

		// Force full screen update.
		supd_wflags(NULL,WFHard | WFMode);
		si.opflags |= OpScrRedraw;
		(void) rcset(Success,0,text227,ncol,nrow);
				// "Terminal dimensions set to %hu x %hu"
		}

	return rc.status;
	}

// Switch to given window.
void wswitch(EWindow *winp) {

	si.curbp = (si.cursp->s_curwp = si.curwp = winp)->w_bufp;
	}

// Get a required screen or window number (via prompt if interactive) and save in *np.  Return status.
static int getnum(char *prmt,bool screen,int *np) {
	Datum *datp;

	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript) {
		if(getnarg(datp,NULL) != Success)
			return rc.status;
		}
	else {
		char wkbuf[NWork];

		// Build prompt with screen or window number range.
		sprintf(wkbuf,"%s %s (1-%d)",prmt,screen ? text380 : text331,screen ? scrcount() : wincount(si.cursp,NULL));
						// "screen","window"
		if(getnarg(datp,wkbuf) != Success || datp->d_type == dat_nil)
			return Cancelled;
		}

	// Return integer result.
	*np = datp->u.d_int;
	return rc.status;
	}

// Switch to another window per flags.  Return status.
int gotoWind(Datum *rp,int n,ushort flags) {

	if(n != 0 || !(flags & EScrWinRepeat)) {
		EWindow *winp;
		int wnum,count;
		int winct = wincount(si.cursp,&wnum);

		// Check if n is out of range.
		if(flags & EScrWinRepeat) {
			if(n == INT_MIN)
				n = 1;
			else if(n < 0)
				return rcset(Failure,0,text39,text137,n,0);
						// "%s (%d) must be %d or greater","Repeat count"

			if(winct == 1 || (n %= winct) == 0)	// If only one window or repeat count equals window count...
				return rc.status;		// nothing to do.
			if(flags & EScrWinForw) {
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
		winp = si.wheadp;					// Find the window...
		count = 0;
		while(++count != n)
			winp = winp->w_nextp;
		wswitch(winp);					// and make new window current.
		supd_wflags(NULL,WFMode);
		}
	return rc.status;
	}

// Switch to previous window n times.  Return status.
int prevWind(Datum *rp,int n,Datum **argpp) {

	return gotoWind(rp,n,EScrWinRepeat);
	}

// Switch to next window n times.  Return status.
int nextWind(Datum *rp,int n,Datum **argpp) {

	return gotoWind(rp,n,EScrWinRepeat | EScrWinForw);
	}

// Switch to given window N, or nth from bottom if N < 0.  Return status.
int selectWind(Datum *rp,int n,Datum **argpp) {

	// Get window number.
	if(si.wheadp->w_nextp == NULL && !(si.opflags & OpScript))
		return rcset(Failure,RCNoFormat,text294);
					// "Only one window"
	if(getnum(text113,false,&n) == Success)
			// "Switch to"
		(void) gotoWind(rp,n,0);

	return rc.status;
	}

// Check if given line is in given window and return Boolean result.
bool inwind(EWindow *winp,Line *lnp) {
	Line *lnp1 = winp->w_face.wf_toplnp;
	ushort i = 0;
	do {
		if(lnp1 == lnp)
			return true;
		if((lnp1 = lnp1->l_nextp) == NULL)
			break;
		} while(++i < winp->w_nrows);
	return false;
	}

// Return true if point is in current window; otherwise, false.
bool ptinwind(void) {

	return inwind(si.curwp,si.curwp->w_face.wf_dot.lnp);
	}

// Move the current window up by "n" lines and compute the new top line of the window.
int moveWindUp(Datum *rp,int n,Datum **argpp) {

	// Nothing to do if buffer is empty.
	if(bempty(NULL))
		return rc.status;

	// Change top line.
	if(n == INT_MIN)
		n = 1;
	wnewtop(si.curwp,NULL,-n);

	// Is point still in the window?
	if(ptinwind())
		return rc.status;

	// Nope.  Move it to the center.
	WindFace *wfp = &si.curwp->w_face;
	Line *lnp = wfp->wf_toplnp;
	int i = si.curwp->w_nrows / 2;
	while(i-- > 0 && lnp->l_nextp != NULL)
		lnp = lnp->l_nextp;
	wfp->wf_dot.lnp = lnp;
	wfp->wf_dot.off = 0;

	return rc.status;
	}

// Make the current window the only window on the screen.  Try to set the framing so that point does not move on the screen.
int onlyWind(Datum *rp,int n,Datum **argpp) {
	EWindow *winp;

	// If there is only one window, nothing to do.
	if(si.wheadp->w_nextp == NULL)
		return rc.status;

	// Nuke windows before current window.
	while(si.wheadp != si.curwp) {
		winp = si.wheadp;
		si.cursp->s_wheadp = si.wheadp = winp->w_nextp;
		if(--winp->w_bufp->b_nwind == 0)
			winp->w_bufp->b_lastscrp = si.cursp;
		wftobf(winp,winp->w_bufp);
		free((void *) winp);
		}

	// Nuke windows after current window.
	while(si.curwp->w_nextp != NULL) {
		winp = si.curwp->w_nextp;
		si.curwp->w_nextp = winp->w_nextp;
		if(--winp->w_bufp->b_nwind == 0)
			winp->w_bufp->b_lastscrp = si.cursp;
		wftobf(winp,winp->w_bufp);
		free((void *) winp);
		}

	// Adjust window parameters.
	wnewtop(si.curwp,NULL,-si.curwp->w_toprow);
	si.curwp->w_toprow = 0;
	si.curwp->w_nrows = term.t_nrow - 2;
	si.curwp->w_flags |= (WFHard | WFMode);

	return rc.status;
	}

// Delete the current window, placing its space in the upper window if default n, n == 0, or n == -1.  If n == 1, place in lower
// window instead.  If n <= -2, force to upper window; if n >= 2, force to lower window.  If delbuf is true, also delete the
// buffer if possible.  If the current window is the top or bottom window, wrap around if necessary to do the force; otherwise,
// just transfer to the adjacent window.  It is assumed the current screen contains at least two windows.
static int delwind(int n,bool delbuf) {
	EWindow *targwinp;		// Window to receive deleted space.
	EWindow *winp;			// Work pointer.
	Buffer *oldbufp = si.curbp;

	// Find receiving window and transfer lines.  Check for special "wrap around" case first (which only applies if
	// we have at least three windows).
	if(si.wheadp->w_nextp->w_nextp != NULL && ((si.curwp == si.wheadp && n != INT_MIN && n < -1) ||
	 (si.curwp->w_nextp == NULL && n > 1))) {
		int rows;

		// Current window is top or bottom and need to transfer lines to window at opposite end of screen.
		if(si.curwp == si.wheadp) {
			targwinp = wnextis(NULL);			// Receiving window (bottom one).
			rows = -(si.curwp->w_nrows + 1);			// Top row adjustment for all windows.
			si.cursp->s_wheadp = si.wheadp = si.curwp->w_nextp;	// Remove current window from list.
			}
		else {
			targwinp = si.wheadp;
			rows = si.curwp->w_nrows + 1;
			wnextis(si.curwp)->w_nextp = NULL;
			wnewtop(targwinp,NULL,-rows);
			}

		// Adjust top rows of other windows and set update flags.
		winp = si.wheadp;
		do {
			winp->w_toprow += rows;
			winp->w_flags |= (WFHard | WFMode);
			} while((winp = winp->w_nextp) != NULL);
		si.wheadp->w_toprow = 0;

		// Adjust size of receiving window.
		targwinp->w_nrows += abs(rows);
		}
	else {
		// Set winp to window before current one.
		winp = wnextis(si.curwp);
		if(winp == NULL || (n > 0 && si.curwp->w_nextp != NULL)) {		// Next window down.
			targwinp = si.curwp->w_nextp;
			targwinp->w_toprow = si.curwp->w_toprow;
			if(winp == NULL)
				si.cursp->s_wheadp = si.wheadp = targwinp;
			else
				winp->w_nextp = targwinp;
			wnewtop(targwinp,NULL,-(si.curwp->w_nrows + 1));
			}
		else {								// Next window up.
			targwinp = winp;
			winp->w_nextp = si.curwp->w_nextp;
			}
		targwinp->w_nrows += si.curwp->w_nrows + 1;
		}

	// Get rid of the current window.
	if(--si.curbp->b_nwind == 0)
		si.curbp->b_lastscrp = si.cursp;
	wftobf(si.curwp,si.curbp);
	free((void *) si.curwp);

	wswitch(targwinp);
	targwinp->w_flags |= (WFHard | WFMode);

	// Delete old buffer if requested.
	if(delbuf) {
		char bname[MaxBufName + 1];
		strcpy(bname,oldbufp->b_bname);
		if(bdelete(oldbufp,0) == Success)
			(void) rcset(Success,0,text372,bname);
					// "Buffer '%s' deleted"
		}

	return rc.status;
	}

// Delete the current window, placing its space in another window per the n value, as described in delwind().  If n == -1, try
// to delete the current buffer also.
int deleteWind(Datum *rp,int n,Datum **argpp) {

	// If there is only one window, don't delete it.
	if(si.wheadp->w_nextp == NULL)
		(void) rcset(Failure,RCNoFormat,text294);
				// "Only one window"
	else
		(void) delwind(n,n == -1);

	return rc.status;
	}

// Join the current window with the upper window if default n or n == 0.  If n == 1, join with lower window instead.  If n <=
// -2, force join with upper window; if n >= 2, force join with lower window.  If n == -1, treat it as the default value, but
// also delete the other window's buffer if possible.  If the current window is the top or bottom window, wrap around if
// necessary to do the force; otherwise, just join with the adjacent window.
int joinWind(Datum *rp,int n,Datum **argpp) {
	EWindow *targwinp;		// Window to delete.
	bool delbuf = (n == -1);

	// If there is only one window, bail out.
	if(si.wheadp->w_nextp == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Find window to delete.  Check for special "wrap around" case first (which only applies if we have at least three
	// windows).
	if(si.wheadp->w_nextp->w_nextp != NULL && ((si.curwp == si.wheadp && n != INT_MIN && n < -1) ||
	 (si.curwp->w_nextp == NULL && n > 1))) {
		if(si.curwp == si.wheadp) {
			targwinp = wnextis(NULL);	// Nuke bottom window.
			n = 2;
			}
		else {
			targwinp = si.wheadp;		// Nuke top window.
			n = -2;
			}
		}
	else if(si.curwp == si.wheadp || (n > 0 && si.curwp->w_nextp != NULL)) {
		targwinp = si.curwp->w_nextp;		// Nuke next window down.
		n = -2;
		}
	else {
		targwinp = wnextis(si.curwp);		// Nuke next window up.
		n = 2;
		}

	wswitch(targwinp);				// Make target window the current window...
	return delwind(n,delbuf);			// and delete it.
	}

// Get a unique window id (a mark past the printable-character range for internal use) and return it in *widp.  Return status.
int getwid(ushort *widp) {
	EScreen *scrp;
	EWindow *winp;
	uint id = '~';

	// If no screen yet exists, use the first window id.
	if((scrp = si.sheadp) == NULL)
		++id;
	else {
		// Get count of all windows in all screens and add it to the maximum user mark value.
		do {
			// In all windows...
			winp = scrp->s_wheadp;
			do {
				++id;
				} while((winp = winp->w_nextp) != NULL);
			} while((scrp = scrp->s_nextp) != NULL);

		// Scan windows again and find an id that is unique.
		for(;;) {
NextId:
			if(++id > USHRT_MAX)
				return rcset(Failure,0,text356,id);
					// "Too many windows (%u)"

			// In all screens...
			scrp = si.sheadp;
			do {
				// In all windows...
				winp = scrp->s_wheadp;
				do {
					if(id == winp->w_id)
						goto NextId;
					} while((winp = winp->w_nextp) != NULL);
				} while((scrp = scrp->s_nextp) != NULL);

			// Success!
			break;
			}
		}

	// Unique id found.  Return it.
	*widp = id;
	return rc.status;
	}	

// Split the current window and return status.  The top or bottom line is dropped to make room for a new mode line, and the
// remaining lines are split into an upper and lower window.  A window smaller than three lines cannot be split.  The point
// remains in whichever window contains point after the split by default.  A line is pushed out of the other window and its
// point is moved to the center.  If n == 0, the point is forced to the opposite (non-default) window.  If n < 0, the size of
// the upper window is reduced by abs(n) lines; if n > 0, the upper window is set to n lines.  *winpp is set to the new window
// not containing point.
int wsplit(int n,EWindow **winpp) {
	EWindow *winp;
	Line *lnp;
	ushort id;
	int nrowu,nrowl,nrowpt;
	bool defupper;			// Default is upper window?
	WindFace *wfp = &si.curwp->w_face;

	// Make sure we have enough space and can obtain a unique id.  If so, create a new window.
	if(si.curwp->w_nrows < 3)
		return rcset(Failure,0,text293,si.curwp->w_nrows);
			// "Cannot split a %d-line window"
	if(getwid(&id) != Success)
		return rc.status;
	if((winp = (EWindow *) malloc(sizeof(EWindow))) == NULL)
		return rcset(Panic,0,text94,"wsplit");
			// "%s(): Out of memory!"

	// Find row containing point (which is assumed to be in the window).
	nrowpt = 0;
	for(lnp = wfp->wf_toplnp; lnp != wfp->wf_dot.lnp; lnp = lnp->l_nextp)
		++nrowpt;

	// Update some settings.
	++si.curbp->b_nwind;			// Now displayed twice (or more).
	winp->w_bufp = si.curbp;
	winp->w_face = *wfp;			// For now.
	winp->w_rfrow = 0;
	winp->w_flags = (WFHard | WFMode);
	winp->w_id = id;

	// Calculate new window sizes.
	nrowu = (si.curwp->w_nrows - 1) / 2;	// Upper window (default).
	if(n != INT_MIN) {
		if(n < 0) {
			if((nrowu += n) < 1)
				nrowu = 1;
			}
		else if(n > 0)
			nrowu = (n < si.curwp->w_nrows - 1) ? n : si.curwp->w_nrows - 2;
		}
	nrowl = (si.curwp->w_nrows - 1) - nrowu;	// Lower window.

	// Make new window the bottom one.
	winp->w_nextp = si.curwp->w_nextp;
	si.curwp->w_nextp = winp;
	si.curwp->w_nrows = nrowu;
	winp->w_nrows = nrowl;
	winp->w_toprow = si.curwp->w_toprow + nrowu + 1;

	// Adjust current window's top line if needed.
	if(nrowpt > nrowu)
		wfp->wf_toplnp = wfp->wf_toplnp->l_nextp;

	// Move down nrowu lines to find top line of lower window.  Stop if we slam into end-of-buffer.
	if(nrowpt != nrowu) {
		lnp = wfp->wf_toplnp;
		while(lnp->l_nextp != NULL) {
			lnp = lnp->l_nextp;
			if(--nrowu == 0)
				break;
			}
		}

	// Set top line and dot line of each window as needed, keeping in mind that buffer may be empty (so top and dot point
	// to the first line) or have just a few lines in it.  In the latter case, set top in the bottom window to the last
	// line of the buffer and dot to same line, except for special case described below.
	if(nrowpt < si.curwp->w_nrows) {

		// Point is in old (upper) window.  Fixup new (lower) window.
		defupper = true;

		// Hit end of buffer looking for top?
		if(lnp->l_nextp == NULL) {

			// Yes, lines in window being split do not extend past the middle.
			winp->w_face.wf_toplnp = lnp->l_prevp;

			// Set point to last line (unless it is already there) so that it will be visible in the lower window.
			if(wfp->wf_dot.lnp->l_nextp != NULL) {
				winp->w_face.wf_dot.lnp = si.curbp->b_lnp->l_prevp;
				winp->w_face.wf_dot.off = 0;
				}
			}
		else {
			// No, save current line as top and press onward to find spot to place point.
			winp->w_face.wf_toplnp = lnp;
			nrowu = nrowl / 2;
			while(nrowu-- > 0) {
				if((lnp = lnp->l_nextp)->l_nextp == NULL)
					break;
				}
	
			// Set point line to mid-point of lower window or last line of buffer.
			winp->w_face.wf_dot.lnp = lnp;
			winp->w_face.wf_dot.off = 0;
			}
		}
	else {
		// Point is in new (lower) window.  Fixup both windows.
		defupper = false;

		// Set top line of lower window (point is already correct).
		winp->w_face.wf_toplnp = lnp;

		// Set point in upper window to middle.
		nrowu = si.curwp->w_nrows / 2;
		lnp = wfp->wf_toplnp;
		while(nrowu-- > 0)
			lnp = lnp->l_nextp;
		wfp->wf_dot.lnp = lnp;
		wfp->wf_dot.off = 0;
		}

	// Both windows are now set up.  All that's left is to set the window-update flags, set the current window to the bottom
	// one if needed, and return the non-dot window pointer if requested.
	si.curwp->w_flags |= (WFHard | WFMode);
	if((n != 0 && !defupper) || (n == 0 && defupper)) {
		si.cursp->s_curwp = winp;
		winp = si.curwp;
		si.curwp = si.cursp->s_curwp;
		}
	*winpp = winp;

	return rc.status;
	}

// Grow or shrink the current window.  If how == 0, set window size to abs(n) lines; otherwise, shrink (how < 0) or grow
// (how > 0) by abs(n) lines.  Find the window that loses or gains space and make sure the window that shrinks is big enough.
// If n < 0, try to use upper window; otherwise, lower.  If it's a go, set the window flags and let the redisplay system do all
// the hard work.  (Can't just set "force reframe" because point would move.)  Return status.
int gswind(Datum *rp,int n,int how) {
	EWindow *adjwp;
	bool grow = (how > 0);

	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		return rc.status;		// Nothing to do.

	if(si.wheadp->w_nextp == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Figure out which window (next or previous) to steal lines from or give lines to.
	adjwp = si.curwp->w_nextp;
	if(si.curwp != si.wheadp && (n < 0 || adjwp == NULL))
		adjwp = wnextis(si.curwp);

	if(n < 0)
		n = -n;
	if(how == 0) {

		// Want n-line window.  Convert n to a size adjustment.
		if(n > si.curwp->w_nrows) {
			n -= si.curwp->w_nrows;
			grow = true;
			}
		else if(n == si.curwp->w_nrows)
			return rc.status;	// Nothing to do.
		else {
			n = si.curwp->w_nrows - n;
			grow = false;
			}
		}

	if(grow) {
		// Adjacent window big enough?
		if(adjwp->w_nrows <= n)
			return rcset(Failure,0,text207,n,n == 1 ? "" : "s");
				// "Cannot get %d line%s from adjacent window"

		// Yes, proceed.
		if(si.curwp->w_nextp == adjwp) {		// Shrink below.
			wnewtop(adjwp,NULL,n);
			adjwp->w_toprow += n;
			}
		else {					// Shrink above.
			wnewtop(si.curwp,NULL,-n);
			si.curwp->w_toprow -= n;
			}
		si.curwp->w_nrows += n;
		adjwp->w_nrows -= n;
		}
	else {
		// Current window big enough?
		if(si.curwp->w_nrows <= n)
			return rcset(Failure,0,text93,n,n == 1 ? "" : "s");
				// "Current window too small to shrink by %d line%s"

		// Yes, proceed.
		if(si.curwp->w_nextp == adjwp) {		// Grow below.
			wnewtop(adjwp,NULL,-n);
			adjwp->w_toprow -= n;
			}
		else {					// Grow above.
			wnewtop(si.curwp,NULL,n);
			si.curwp->w_toprow += n;
			}
		si.curwp->w_nrows -= n;
		adjwp->w_nrows += n;
		}

	si.curwp->w_flags |= (WFHard | WFMode);
	adjwp->w_flags |= (WFHard | WFMode);

	return rc.status;
	}

// Resize the current window to the requested size.
int resizeWind(Datum *rp,int n,Datum **argpp) {

	// Ignore if no argument or already requested size.
	if(n == INT_MIN || n == si.curwp->w_nrows)
		return rc.status;

	// Error if only one window.
	if(si.wheadp->w_nextp == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Set all windows on current screen to same size if n == 0; otherwise, change current window only.
	if(n == 0) {
		int count = wincount(si.cursp,NULL);
		int size = (term.t_nrow - 1) / count - 1;	// Minimum size of each window, excluding mode line.
		int leftover0 = (term.t_nrow - 1) % count;

		// Remember current window, then step through all windows repeatedly, setting new size of each if possible,
		// until all windows have been set successfully or become deadlocked (which can happen if there are numerous
		// tiny windows).  Give up in the latter case and call it good (a very rare occurrence).
		EWindow *oldwp = si.curwp;
		EWindow *winp;
		int leftover;
		bool success,changed;
		do {
			winp = si.wheadp;
			leftover = leftover0;
			success = true;
			changed = false;
			do {
				wswitch(winp);
				if(leftover > 0) {
					n = size + 1;
					--leftover;
					}
				else
					n = size;

				// Don't try to adjust bottom window.
				if(winp->w_nextp != NULL && winp->w_nrows != n) {
					if(winp->w_nrows < n && winp->w_nextp->w_nrows <= n - winp->w_nrows) {

						// Current window needs to grow but next window is too small.  Take as many
						// rows as possible.
						success = false;
						if(winp->w_nextp->w_nrows == 1)
							continue;
						n = winp->w_nrows + winp->w_nextp->w_nrows - 1;
						}
					if(gswind(rp,n,0) != Success)
						break;
					changed = true;
					}
				} while((winp = winp->w_nextp) != NULL);
			} while(!success && changed);

		// Adjustment loop completed... success or deadlocked.  Switch back to original window in either case.
		wswitch(oldwp);
		}
	else
		(void) gswind(rp,n,0);

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
int render(Datum *rp,int n,Buffer *bufp,ushort flags) {
	EWindow *winp = NULL;
	bool newWind = false;

	if(n == INT_MIN)
		n = -1;

	// Displaying buffer?
	if(n != 0) {
		// Yes.  Popping buffer?
		if(n == -1) {
			// Yes.  Activate buffer if needed and do a real pop-up.
			if(bactivate(bufp) != Success || bpop(bufp,flags | RendWait) != Success)
				return rc.status;
			}

		// Not popping buffer.  Switch to it?
		else if(n == 1) {
			if(bufp != si.curbp && bswitch(bufp) != Success)
				return rc.status;
			winp = si.curwp;
			}

		// No, displaying buffer in another window.
		else {
			EWindow *oldwinp;

			if(n > 1 || si.wheadp->w_nextp == NULL) {	// If force-creating or only one window...
				if(wsplit(INT_MIN,&winp) != Success)	// split current window and get new one into winp.
					return rc.status;
				newWind = true;
				}
			else if((winp = whasbuf(bufp,true)) != NULL) {	// Otherwise, find a different window, giving
				if(n == -2) {				// preference to one already displaying the buffer...
					(void) rcset(Success,0,text28,text58);
						// "%s is being displayed","Buffer"
					goto Retn;
					}
				}
			else if((winp = wnextis(si.curwp)) == NULL)	// or the window above, if none found.
				winp = si.curwp->w_nextp;
			oldwinp = si.curwp;				// save old window...
			wswitch(winp);					// make new one current...
			if(bswitch(bufp) != Success)			// and switch to new buffer.
				return rc.status;
			if(flags & RendReset)
				faceinit(&winp->w_face,bufp->b_lnp,NULL);
			if(abs(n) == 2)					// If not a force to new window...
				wswitch(oldwinp);			// switch back to previous one.
			}
		}
Retn:
	// Wrap up and set return value.
	if(n == -1) {
		if(flags & RendNewBuf) {
			if(bdelete(bufp,0) != Success)
				return rc.status;
			bufp = NULL;
			dsetnil(rp);
			}
		else {
			if(dsetstr(bufp->b_bname,rp) != 0)
				goto LibFail;
			}
		}
	else {
		Array *aryp;

		if((aryp = anew(n == 0 ? 2 : 4,NULL)) == NULL || dsetstr(bufp->b_bname,aryp->a_elpp[0]) != 0)
			goto LibFail;
		else {
			dsetbool(flags & RendNewBuf,aryp->a_elpp[1]);
			if(n != 0) {
				if(winp == NULL)
					dsetnil(aryp->a_elpp[2]);
				else
					dsetint((long) getwnum(winp),aryp->a_elpp[2]);
				dsetbool(newWind,aryp->a_elpp[3]);
				}
			if(awrap(rp,aryp) != Success)
				return rc.status;
			}

		if((flags & RendNotify) || (n == 0 && (flags & RendNewBuf)))
			(void) rcset(Success,0,text381,bufp->b_bname);
				// "Buffer '%s' created"
		}
	if(bufp != NULL && (flags & RendReset))
		bufp->b_flags &= ~BFHidden;
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Scroll the previous or next window up (backward) or down (forward) a page.
int wscroll(Datum *rp,int n,int (*winfunc)(Datum *rp,int n,Datum **argpp),int (*pagefunc)(Datum *rp,int n,Datum **argpp)) {

	(void) winfunc(rp,INT_MIN,NULL);
	(void) pagefunc(rp,n,NULL);
	(void) (*(winfunc == prevWind ? nextWind : prevWind))(rp,INT_MIN,NULL);

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
void dumpbuffer(char *label,Buffer *bufp,bool withData) {
	Mark *mkp;
	char wkbuf[16];
	static char s[] = "%s\n";

	if(bufp == NULL)
		bufp = si.curbp;
	if(label != NULL)
		fprintf(logfile,"*** In %s...\n",label);
	fprintf(logfile,"Buffer '%s' [%.08X]:\n",bufp->b_bname,(uint) bufp);
	fprintf(logfile,s,lninfo("\tb_lnp",bufp->b_lnp));
	fprintf(logfile,s,lninfo("\tb_lnp->l_prevp",bufp->b_lnp->l_prevp));
	fprintf(logfile,s,lninfo("\tb_lnp->l_prevp->l_nextp",bufp->b_lnp->l_prevp->l_nextp));
	fprintf(logfile,s,lninfo("\tb_ntoplnp",bufp->b_ntoplnp));
		if(bufp->b_ntoplnp != NULL)
			fprintf(logfile,s,lninfo("\t\tb_ntoplnp->l_prevp",bufp->b_ntoplnp->l_prevp));
	fprintf(logfile,s,lninfo("\tb_nbotlnp",bufp->b_nbotlnp));
		if(bufp->b_nbotlnp != NULL)
			fprintf(logfile,s,lninfo("\t\tb_nbotlnp->l_prevp",bufp->b_nbotlnp->l_prevp));

	fprintf(logfile,s,lninfo("\tb_face.wf_toplnp",bufp->b_face.wf_toplnp));
	fprintf(logfile,"\t%s\n\tb_face.wf_dot.off: %d\n",
	 lninfo("b_face.wf_dot.lnp",bufp->b_face.wf_dot.lnp),bufp->b_face.wf_dot.off);
	fputs("\tMarks:\n",logfile);
	mkp = &bufp->b_mroot;
	do {
		if(mkp->mk_id <= '~')
			sprintf(wkbuf,"%c",(int) mkp->mk_id);
		else
			sprintf(wkbuf,"%.4hX",mkp->mk_id);
		fprintf(logfile,"\t\t'%s': (%s), mk_dot.off %d, mk_rfrow %hd\n",
		 wkbuf,lninfo("mk_dot.lnp",mkp->mk_dot.lnp),mkp->mk_dot.off,mkp->mk_rfrow);
		} while((mkp = mkp->mk_nextp) != NULL);

	if(bufp->b_mip == NULL)
		strcpy(wkbuf,"NULL");
	else
		sprintf(wkbuf,"%hu",bufp->b_mip->mi_nexec);
	fprintf(logfile,
	 "\tb_face.wf_firstcol: %d\n\tb_nwind: %hu\n\tb_nexec: %s\n\tb_nalias: %hu\n\tb_flags: %.4X\n\tb_modes:",
	 bufp->b_face.wf_firstcol,bufp->b_nwind,wkbuf,bufp->b_nalias,(uint) bufp->b_flags);
	if(bufp->b_modes == NULL)
		fputs(" NONE",logfile);
	else {
		BufMode *bmp = bufp->b_modes;
		do
			fprintf(logfile," %s",bmp->bm_modep->ms_name);
			while((bmp = bmp->bm_nextp) != NULL);
		}
	if(bufp->b_fname == NULL)
		strcpy(wkbuf,"NULL");
	else
		sprintf(wkbuf,"'%s'",bufp->b_fname);
	fprintf(logfile,"\n\tb_inpdelim: %.2hX %.2hX (%d)\n\tb_fname: %s\n",
	 (ushort) bufp->b_inpdelim[0],(ushort) bufp->b_inpdelim[1],bufp->b_inpdelimlen,wkbuf);

	if(withData) {
		Line *lnp = bufp->b_lnp;
		uint n = 0;
		fputs("\tData:\n",logfile);
		do {
			sprintf(wkbuf,"\t\tL%.4u",++n);
			fprintf(logfile,s,lninfo(wkbuf,lnp));
			} while((lnp = lnp->l_nextp) != NULL);
		}
	}
#endif

#if MMDebug & Debug_ScrDump
// Write window information to log file.
static void dumpwindow(EWindow *winp,int windnum) {

	fprintf(logfile,"\tWindow %d [%.08x]:\n\t\tw_nextp: %.08x\n\t\tw_bufp: %.08x '%s'\n%s\n",
	 windnum,(uint) winp,(uint) winp->w_nextp,(uint) winp->w_bufp,winp->w_bufp->b_bname,
	 lninfo("\t\tw_face.wf_toplnp",winp->w_face.wf_toplnp));
	fprintf(logfile,"%s\n\t\tw_face.wf_dot.off: %d\n\t\tw_toprow: %hu\n\t\tw_nrows: %hu\n\t\tw_rfrow: %hu\n",
	 lninfo("\t\tw_face.wf_dot.lnp",winp->w_face.wf_dot.lnp),
	 winp->w_face.wf_dot.off,winp->w_toprow,winp->w_nrows,winp->w_rfrow);
	fprintf(logfile,"\t\tw_flags: %.04x\n\t\tw_face.wf_firstcol: %d\n",
	 (uint) winp->w_flags,winp->w_face.wf_firstcol);
	}

// Write screen, window, and buffer information to log file -- for debugging.
void dumpscreens(char *msg) {
	EScreen *scrp;
	EWindow *winp;
	Array *aryp;
	Datum *datp;
	int windnum;

	fprintf(logfile,"### %s ###\n\n*SCREENS\n\n",msg);

	// Dump screens and windows.
	scrp = si.sheadp;
	do {
		fprintf(logfile,"Screen %hu [%.08x]:\n\ts_flags: %.04x\n\ts_nrow: %hu\n\ts_ncol: %hu\n\ts_curwp: %.08x\n",
		 scrp->s_num,(uint) scrp,(uint) scrp->s_flags,scrp->s_nrow,scrp->s_ncol,(uint) scrp->s_curwp);
		winp = scrp->s_wheadp;
		windnum = 0;
		do {
			dumpwindow(winp,++windnum);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// Dump buffers.
	fputs("\n*BufferS\n\n",logfile);
	aryp = &buftab;
	while((datp = aeach(&aryp)) != NULL)
		dumpbuffer(NULL,bufptr(datp),false);
	}
#endif

// Get number of screens.
int scrcount(void) {
	EScreen *scrp = si.sheadp;
	int count = 0;
	do {
		++count;
		} while((scrp = scrp->s_nextp) != NULL);
	return count;
	}

// Find a screen, given number, (possibly NULL) pointer to buffer to attach to first window of screen, and (possibly NULL)
// pointer to result.  If the screen is not found and "bufp" is not NULL, create new screen and return status; otherwise, return
// false, ignoring spp.
int sfind(ushort scr_num,Buffer *bufp,EScreen **spp) {
	EScreen *scrp1;
	ushort snum;
	static char myname[] = "sfind";

	// Scan the screen list.  Note that the screen list is empty at program launch.
	for(snum = 0, scrp1 = si.sheadp; scrp1 != NULL; scrp1 = scrp1->s_nextp) {
		if((snum = scrp1->s_num) == scr_num) {
			if(spp)
				*spp = scrp1;
			return (bufp == NULL) ? true : rc.status;
			}
		}

	// No such screen exists, create new one?
	if(bufp != NULL) {
		ushort id;
		EScreen *scrp2;
		EWindow *winp;		// Pointer to first window of new screen.

		// Get unique window id.
		if(getwid(&id) != Success)
			return rc.status;

		// Allocate memory for screen.
		if((scrp1 = (EScreen *) malloc(sizeof(EScreen))) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"

		// Set up screen fields.
		scrp1->s_lastbufp = NULL;
		scrp1->s_num = snum + 1;
		scrp1->s_flags = 0;
		scrp1->s_wkdir = NULL;
		if(setwkdir(scrp1) != Success)
			return rc.status;			// Fatal error.
		scrp1->s_nrow = term.t_nrow;
		scrp1->s_ncol = term.t_ncol;
		scrp1->s_cursrow = scrp1->s_curscol = scrp1->s_firstcol = 0;

		// Allocate its first window...
		if((winp = (EWindow *) malloc(sizeof(EWindow))) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"
		scrp1->s_wheadp = scrp1->s_curwp = winp;

		// and set up the window's info.
		winp->w_nextp = NULL;
		winp->w_bufp = bufp;
		++bufp->b_nwind;
		bftowf(bufp,winp);
		winp->w_id = id;
		winp->w_toprow = 0;
		winp->w_nrows = term.t_nrow - 2;		// "-2" for message and mode lines.
		winp->w_rfrow = 0;

		// Insert new screen at end of screen list.
		scrp1->s_nextp = NULL;
		if((scrp2 = si.sheadp) == NULL)
			si.sheadp = scrp1;
		else {
			while(scrp2->s_nextp != NULL)
				scrp2 = scrp2->s_nextp;
			scrp2->s_nextp = scrp1;
			}

		// and return the new screen pointer.
		if(spp)
			*spp = scrp1;
		return rc.status;
		}

	// Screen not found and bufp is NULL.
	return false;
	}

// Switch to given screen.  Return status.
int sswitch(EScreen *scrp) {

	// Nothing to do if it is already current.
	if(scrp == si.cursp)
		return rc.status;

	// Save the current screen's concept of current window.
	si.cursp->s_curwp = si.curwp;
	si.cursp->s_nrow = term.t_nrow;
	si.cursp->s_ncol = term.t_ncol;

	// Run exit-buffer user hook on current (old) buffer if the new screen's buffer is different.
	Datum *rp;
	bool diffbuf = (scrp->s_curwp->w_bufp != si.curbp);
	if(diffbuf) {
		if(dnewtrk(&rp) != 0)
			return librcset(Failure);
		if(bhook(rp,true) != Success)
			return rc.status;
		}

	// Change current directory if needed.
	if(strcmp(scrp->s_wkdir,si.cursp->s_wkdir) != 0)
		(void) chgdir(scrp->s_wkdir);

	// Reset the current screen, window and buffer.
	si.wheadp = (si.cursp = scrp)->s_wheadp;
	si.curbp = (si.curwp = scrp->s_curwp)->w_bufp;

	// Let the display driver know we need a full screen update.
	supd_wflags(NULL,WFHard | WFMode);
	si.opflags |= OpScrRedraw;

	// Run enter-buffer user hook on current (new) buffer.
	if(diffbuf && rc.status == Success)
		(void) bhook(rp,false);

	return rc.status;
	}

// Bring a screen to the front per flags.  Return status.
int gotoScreen(int n,ushort flags) {

	if(n != 0 || !(flags & EScrWinRepeat)) {
		EScreen *scrp;
		int scrct = scrcount();		// Total number of screens.
		char olddir[strlen(si.cursp->s_wkdir) + 1];

		// Check if n is out of range.
		if(flags & EScrWinRepeat) {
			if(n == INT_MIN)
				n = 1;
			else if(n < 0)
				return rcset(Failure,0,text39,text137,n,0);
						// "%s (%d) must be %d or greater","Repeat count"

			if(scrct == 1 || (n %= scrct) == 0)	// If only one screen or repeat count equals screen count...
				return rc.status;		// nothing to do.
			if(flags & EScrWinForw) {
				if((n += si.cursp->s_num) > scrct)
					n -= si.cursp->s_num;
				}
			else if((n = si.cursp->s_num - n) < 1)
				n += scrct;
			}
		else if(n <= 0 || abs(n) > scrct)
			return rcset(Failure,0,text240,n);
					// "No such screen '%d'"

		// n is now the target screen number.
		scrp = si.sheadp;					// Find the screen...
		while(scrp->s_num != n)
			scrp = scrp->s_nextp;
		strcpy(olddir,si.cursp->s_wkdir);			// save current directory...
		if(sswitch(scrp) == Success)			// make new screen current...

			// and display its working directory if interactive, 'WkDir' global mode not enabled, and changed.
			if(!(si.opflags & OpScript) && !(mi.cache[MdIdxWkDir]->ms_flags & MdEnabled) &&
			 strcmp(si.cursp->s_wkdir,olddir) != 0)
				rcset(Success,RCForce | RCNoFormat | RCNoWrap,scrp->s_wkdir);
		}
	return rc.status;
	}

// Switch to given screen (default n), create screen (n > 0), or force redraw of current screen (n <= 0).  Return status.
int selectScreen(Datum *rp,int n,Datum **argpp) {

	if(n == INT_MIN) {

		// Switch screen.  Get number.
		if(si.sheadp->s_nextp == NULL && !(si.opflags & OpScript))
			return rcset(Failure,RCNoFormat,text57);
						// "Only one screen"
		if(getnum(text113,true,&n) != Success || gotoScreen(n,0) != Success)
				// "Switch to"
			return rc.status;
		}
	else if(n > 0) {
		EScreen *scrp;

		// Create screen.  Save current screen number and current window's settings.
		wftobf(si.curwp,si.curbp);

		// Find screen "0" to force-create one and make it current.
		if(sfind(0,si.curbp,&scrp) != Success || sswitch(scrp) != Success)
			return rc.status;
		(void) rcset(Success,0,text174,scrp->s_num);
				// "Created screen %hu"
		}
	else {
		// Force redraw of physical screen.
		si.opflags |= OpScrRedraw;
		(void) rcset(Success,RCNoFormat,text211);
				// "Screen refreshed"
		}

	return rc.status;
	}

// Free all resouces associated with a screen.
static void freescreen(EScreen *scrp) {
	EWindow *winp,*tp;

	// First, free the screen's windows...
	winp = scrp->s_wheadp;
	do {
		if(--winp->w_bufp->b_nwind == 0)
			winp->w_bufp->b_lastscrp = NULL;
		wftobf(winp,winp->w_bufp);

		// On to the next window; free this one.
		tp = winp;
		winp = winp->w_nextp;
		(void) free((void *) tp);
		} while(winp != NULL);

	// free the working directory...
	(void) free((void *) scrp->s_wkdir);

	// and lastly, free the screen itself.
	(void) free((void *) scrp);
	}

// Remove screen from the list and renumber remaining ones.  Update modeline of bottom window if only one left or current screen
// number changes.  Return status.
static int unlistscreen(EScreen *scrp) {
	ushort snum0,snum;
	EScreen *tp;

	snum0 = si.cursp->s_num;
	if(scrp == si.sheadp)
		si.sheadp = si.sheadp->s_nextp;
	else {
		tp = si.sheadp;
		do {
			if(tp->s_nextp == scrp)
				goto Found;
			} while((tp = tp->s_nextp) != NULL);

		// Huh?  Screen not found... this is a bug.
		return rcset(FatalError,0,text177,"unlistscreen",(int) scrp->s_num);
				// "%s(): Screen number %d not found in screen list!"
Found:
		tp->s_nextp = scrp->s_nextp;
		}

	// Renumber screens.
	snum = 0;
	tp = si.sheadp;
	do {
		tp->s_num = ++snum;
		} while((tp = tp->s_nextp) != NULL);

	// If only one screen left or current screen number changed, flag mode line at bottom of screen for update.
	if(snum == 1 || si.cursp->s_num != snum0) {

		// Flag last window.
		wnextis(NULL)->w_flags |= WFMode;
		}

	return rc.status;
	}

// Delete a screen.  Return status.
int deleteScreen(Datum *rp,int n,Datum **argpp) {
	EScreen *scrp;

	// Get the number of the screen to delete.  Error if only one exists.
	if(si.sheadp->s_nextp == NULL)
		return rcset(Failure,RCNoFormat,text57);
					// "Only one screen"
	if(getnum(text26,true,&n) != Success)
			// "Delete"
		return rc.status;

	// Make sure it exists.
	if(!sfind((ushort) n,NULL,&scrp))
		return rcset(Failure,0,text240,n);
			// "No such screen '%d'"

	// Switch screens if deleting current one.
	if(scrp == si.cursp && sswitch(scrp == si.sheadp ? scrp->s_nextp : si.sheadp) != Success)
		return rc.status;

	// Everything's cool... nuke it.
	nukebufsp(scrp);
	if(unlistscreen(scrp) != Success)
		return rc.status;
	freescreen(scrp);

	return rcset(Success,0,text178,n);
			// "Screen %d deleted"
	}

// Build and pop up a buffer containing a list of all screens and their associated buffers.  Render buffer and return status.
int showScreens(Datum *rp,int n,Datum **argpp) {
	Buffer *slistp;
	EScreen *scrp;			// Pointer to current screen to list.
	EWindow *winp;			// Pointer into current screens window list.
	uint wnum;
	DStrFab rpt;
	bool chg = false;		// Screen has a changed buffer?
	int windcol = 8;
	int filecol = 38;
	char sepline1[filecol + 33],*sepline;

	// Get a buffer and open a string-fab object.
	if(sysbuf(text160,&slistp,BFTermAttr) != Success)
			// "Screens"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;

	// Write first header line.
	if(dputs(text89,&rpt) != 0)
			// "~bScreen  Window    Buffer                File~0"
		goto LibFail;
	dupchr(sepline1,'-',filecol + 32);
	sepline = "======  ======  ====================  ================================";

	// For all screens...
	scrp = si.sheadp;
	do {
		// Write separator line.
		if(dputc('\n',&rpt) != 0 || dputs(sepline,&rpt) != 0)
			goto LibFail;
		sepline = sepline1;

		// Store the screen number and working directory.
		if(dputf(&rpt,"\n~b%4hu~0    WD: %s\n",scrp->s_num,scrp->s_wkdir) != 0)
			goto LibFail;

		// List screen's window numbers and buffer names.
		wnum = 0;
		winp = scrp->s_wheadp;
		do {
			Buffer *bufp = winp->w_bufp;

			// Store window number and "changed" marker.
			if(bufp->b_flags & BFChanged)
				chg = true;
			if(dputf(&rpt,"\n%*u   %c",windcol + 4,++wnum,(bufp->b_flags & BFChanged) ? '*' : ' ') != 0)
				goto LibFail;

			// Store buffer name and filename.
			if(bufp->b_fname != NULL) {
				if(dputf(&rpt,"%-*s%s",(MaxBufName - 2),bufp->b_bname,bufp->b_fname) != 0)
					goto LibFail;
				}
			else if(dputs(bufp->b_bname,&rpt) != 0)
				goto LibFail;

			// On to the next window.
			} while((winp = winp->w_nextp) != NULL);

		// On to the next screen.
		} while((scrp = scrp->s_nextp) != NULL);

	// Add footnote if applicable.
	if(chg && (dputc('\n',&rpt) != 0 || dputs(sepline,&rpt) != 0 || dputs(text243,&rpt) != 0))
								// "\n* Changed buffer"
		goto LibFail;

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(slistp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n,slistp,RendNewBuf | RendReset);
	}
