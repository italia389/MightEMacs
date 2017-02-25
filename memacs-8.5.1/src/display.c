// (c) Copyright 2017 Richard W. Marinelli
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
#include "lang.h"
#include "main.h"
#include "exec.h"

// Flag all mode lines in the current screen for updating.  If bufp is not NULL, mark only those windows.
void upmode(Buffer *bufp) {
	EWindow *winp;

	winp = wheadp;
	do {
		if(bufp == NULL || winp->w_bufp == bufp)
			winp->w_flags |= WFMode;
		} while((winp = winp->w_nextp) != NULL);
	}

// Force hard updates on all windows in the current screen.
void uphard(void) {
	EWindow *winp;

	winp = wheadp;
	do {
		winp->w_flags |= WFHard | WFMode;
		} while((winp = winp->w_nextp) != NULL);
	}

// Find window on current screen whose w_nextp matches given pointer and return it, or NULL if not found (winp is top window).
EWindow *wnextis(EWindow *winp) {
	EWindow *winp1 = wheadp;

	if(winp == winp1)
		return NULL;			// No window above top window.
	while(winp1->w_nextp != winp)
		winp1 = winp1->w_nextp;
	return winp1;
	}

// Restore message line cursor position.  Return status.
int mlrestore(void) {

	if(movecursor(term.t_nrow - 1,ml.ttcol) == Success)
		(void) TTflush();
	return rc.status;
	}

// Erase the message line.  Return status.
int mlerase(ushort flags) {

	// If we are not currently echoing on the command line and it's not a force, abort this.
	if(!(modetab[MdRec_Global].flags & MdMsg) && !(flags & MLForce))
		return rc.status;

	// Home the cursor.
	int i = ml.ttcol;
	if(movecursor(term.t_nrow - 1,0) != Success)
		return rc.status;
#if Color && 0
	if(TTforg(gfcolor) != Success || TTbacg(gbcolor) != Success)
		return rc.status;
#endif
	// Erase line if needed.
	if(i != 0) {
		if(opflags & OpHaveEOL) {
			if(TTeeol() != Success)
				return rc.status;
			}
		else {
			for(i = 0; i < term.t_ncol - 1; ++i)
				if(TTputc(' ') != Success)
					return rc.status;

			// Reset cursor.
			if(movecursor(term.t_nrow - 1,0) != Success)
				return rc.status;
			}

		// Reset buffer pointer and update message line on screen.
		ml.spanw = ml.span;
		(void) TTflush();
		}

	return rc.status;
	}

// Write a character to the message line with invisible characters exposed, unless MLRaw flag is set.  Keep track of the
// physical cursor position and actual number of characters output for each character so that backspacing can be done correctly.
// Return status.
int mlputc(ushort flags,int c) {

	// Nothing to do if past right edge of terminal and not tracking.
	if(ml.ttcol >= term.t_ncol && !(flags & MLTrack))
		return rc.status;

	// Raw character?
	if(flags & MLRaw) {

		// Yes, backspace?
		if(c == '\b') {

			// Undo (erase) last char literal.
			if(ml.ttcol > 0) {
				uint len = *--ml.spanw;
				do {
					if(ml.ttcol <= term.t_ncol) {
						if(ml.ttcol == term.t_ncol) {
							if(!(opflags & OpHaveEOL)) {

								// This is ugly, but it works.
								if(movecursor(term.t_nrow - 1,term.t_ncol - 1) != Success ||
								 TTputc(' ') != Success)
									return rc.status;
								++ml.ttcol;
								}
							else if(TTeeol() != Success)
								return rc.status;
							}
						else if(TTputc('\b') != Success || TTputc(' ') != Success ||
						 TTputc('\b') != Success)
							return rc.status;
						}
					--ml.ttcol;
					} while(--len > 0);
				}
			}

		// Not a backspace.  Display raw character (if room).
		else {
			if(ml.ttcol++ < term.t_ncol)
				if(TTputc(c) != Success)
					return rc.status;
			*ml.spanw++ = 1;
			}
		}
	else {
		char *str;
		int ttcol0 = ml.ttcol;

		// Not raw.  Display char literal (if any) and remember length, even if past right edge.
		c = *(str = vizc(c,VBaseDef));
		do {
			if(ml.ttcol++ < term.t_ncol)
				if(TTputc(c) != Success)
					return rc.status;
			} while((c = *++str) != '\0');
		*ml.spanw++ = ml.ttcol - ttcol0;
		}

	return rc.status;
	}

// Write out an integer, in the specified radix.  Update the physical cursor position.  Return status.
static int mlputi(int i,int r) {
	int q;
	static char hexdigits[] = "0123456789abcdef";

	if(i < 0) {
		i = -i;
		if(mlputc(MLRaw,'-') != Success)
			return rc.status;
		}
	q = i / r;
	if(q != 0 && mlputi(q,r) != Success)
		return rc.status;
	return mlputc(MLRaw,hexdigits[i % r]);
	}

// Do the same for a long integer.
static int mlputli(long l,int r) {
	long q;

	if(l < 0) {
		l = -l;
		if(mlputc(MLRaw,'-') != Success)
			return rc.status;
		}
	q = l / r;
	if(q != 0 && mlputli(q,r) != Success)
		return rc.status;
	return mlputc(MLRaw,(int)(l % r) + '0');
	}

#if MLScaled
// Write out a scaled integer with two decimal places.  Return status.
static int mlputf(int s) {
	int i;		// Integer portion of number.
	int f;		// Fractional portion of number.

	// Break it up.
	i = s / 100;
	f = s % 100;

	// Send it out with a decimal point.
	if(mlputi(i,10) == Success && mlputc(MLRaw,'.') == Success && mlputc(MLRaw,(f / 10) + '0') == Success)
		(void) mlputc(MLRaw,(f % 10) + '0');

	return rc.status;
	}
#endif

// Prepare for new message line message.  Return rc.status (Success) if successful; otherwise, NotFound (bypassing rcset()).
static int mlbegin(ushort flags) {

	// If we are not currently echoing on the command line and it's not a force, abort this.
	if(!(modetab[MdRec_Global].flags & MdMsg) && !(flags & MLForce))
		return NotFound;

	// Position cursor and/or begin wrap, if applicable.
	if((flags & MLHome) && mlerase(flags | MLForce) != Success)
		return rc.status;
	if(flags & MLWrap)
		(void) mlputc(MLRaw,'[');

	return rc.status;
	}

// Finish message line message.  Return status.
static int mlend(ushort flags) {

	// Finish wrap and flush message.
	if(!(flags & MLWrap) || mlputc(MLRaw,']') == Success)
		(void) TTflush();
	return rc.status;
	}

// Write text into the message line, given flag word, format string and optional arguments.  If MLHome is set in f, move cursor
// to bottom left corner of screen first.  If MLForce is set in f, write string regardless of the MdMsg global flag setting.  If
// MLWrap is set in f, wrap message within '[' and ']' characters before displaying it.
//
// A small class of printf-like format items is handled.  Set the "message line" flag to true.  Don't write beyond the end of
// the current terminal width.  Return status.
int mlprintf(ushort flags,char *fmt,...) {
	int c;			// Current char in format string.
	va_list ap;		// Pointer to current data field.
	int arg_int;		// Integer argument.
	long arg_long;		// Long argument.
	char *arg_str;		// String argument.

	// Bag it if not currently echoing and not a force.
	if(mlbegin(flags) != Success)
		return rc.status;

	// Process arguments.
	va_start(ap,fmt);
	while((c = *fmt++) != '\0') {
		if(c != '%')
			(void) mlputc(0,c);
		else {
			c = *fmt++;
			switch(c) {
				case 'd':
					arg_int = va_arg(ap,int);
					(void) mlputi(arg_int,10);
					break;
				case 'o':
					arg_int = va_arg(ap,int);
					(void) mlputi(arg_int,8);
					break;
				case 'x':
					arg_int = va_arg(ap,int);
					(void) mlputi(arg_int,16);
					break;
				case 'D':
					arg_long = va_arg(ap,long);
					(void) mlputli(arg_long,10);
					break;
				case 's':
					arg_str = va_arg(ap,char *);
					(void) mlputs(MLForce,arg_str);
					break;
#if MLScaled
				case 'f':
					arg_int = va_arg(ap,int);
					(void) mlputf(arg_int);
					break;
#endif
				case 'c':
					c = va_arg(ap,int);
					// Fall through.
				default:
					(void) mlputc(0,c);
				}
			}
		if(rc.status != Success)
			return rc.status;
		}
	va_end(ap);
	return mlend(flags);
	}

// Write a string to the message line, given flags and message.  If MLHome flag is set, move cursor to bottom left corner of
// screen first.  If MLForce flag is set, write string regardless of the 'msg' global flag setting.  If MLWrap flag is set, wrap
// message within '[' and ']' characters before displaying it.  Pass flags to mlputc().  Return status.
int mlputs(ushort flags,char *str) {

	// Write string if currently echoing or a force.
	if(mlbegin(flags) == Success) {
		int c;

		// Display the string.
		while((c = *str++) != '\0')
			if(mlputc(flags,c) != Success)
				return rc.status;

		// Finish wrap and flush message.
		(void) mlend(flags);
		}

	return rc.status;
	}

// Write a datum object to the message line.  Return status.
int mlputv(ushort flags,Datum *datp) {

	// Write string if currently echoing or a force.
	if(mlbegin(flags) == Success) {
		int c;
		char *str;
		char wkbuf[LongWidth];

		if(datp->d_type == dat_int)
			sprintf(str = wkbuf,"%ld",datp->u.d_int);
		else
			str = datp->d_str;
		while((c = *str++) != '\0')
			if(mlputc(flags,c) != Success)
				return rc.status;

		// Finish wrap and flush message.
		(void) mlend(flags);
		}
	return rc.status;
	}

// Initialize dot position, marks, and first column position of a face record, given line pointer.  If bufp is not NULL, reset
// its buffer marks.
void faceinit(WindFace *wfp,Line *lnp,Buffer *bufp) {

	wfp->wf_toplnp = wfp->wf_dot.lnp = lnp;
	wfp->wf_dot.off = wfp->wf_fcol = 0;

	// Clear mark(s).
	if(bufp != NULL)
		mdelete(bufp,MkOpt_Viz | MkOpt_Wind);
	}

// Copy buffer face record to a window.
void bftowf(Buffer *bufp,EWindow *winp) {

	winp->w_face = bufp->b_face;
	winp->w_flags |= WFMode | WFHard;
	}

// Copy window face record to a buffer and clear the "buffer face in unknown state" flag.
void wftobf(EWindow *winp,Buffer *bufp) {

	bufp->b_face = winp->w_face;
	}

// Get number of windows on current screen.
int wincount(void) {
	EWindow *winp = wheadp;
	int count = 0;
	do {
		++count;
		} while((winp = winp->w_nextp) != NULL);
	return count;
	}

// Create array of [wind-num,buf-name] (default n) or [screen-num,wind-num,buf-name] (non-default n) for all existing windows
// and save result in rp.  Return status.
int getwindlist(Datum *rp,int n) {
	EScreen *scrp;
	EWindow *winp;
	Array *aryp0,*aryp1;
	Datum *datp,**elpp;
	long snum,wnum;

	if((aryp0 = anew(0,NULL)) == NULL)
		return drcset();
	if(awrap(rp,aryp0) != Success)
		return rc.status;

	// In all screens...
	snum = 0;
	scrp = sheadp;
	do {
		if(++snum == cursp->s_num || n != INT_MIN) {

			// In all windows...
			wnum = 0;
			winp = scrp->s_wheadp;
			do {
				++wnum;
				if((aryp1 = anew(n == INT_MIN ? 2 : 3,NULL)) == NULL)
					return drcset();
				elpp = aryp1->a_elpp;
				if(n != INT_MIN)
					dsetint(snum,*elpp++);
				dsetint(wnum,*elpp++);
				if(dsetstr(winp->w_bufp->b_bname,*elpp) != 0 || (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					return drcset();
				if(awrap(datp,aryp1) != Success)
					return rc.status;
				} while((winp = winp->w_nextp) != NULL);
			}
		} while((scrp = scrp->s_nextp) != NULL);

	return rc.status;
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
	scrp = sheadp;
	n = (n <= 0) ? 0 : 1;
	do {
		// Flag screen if its not the current terminal size.
		if(scrp->s_nrow != nrow || scrp->s_ncol != ncol) {
			scrp->s_flags |= EScrResize;
			n = 1;
			}
		} while((scrp = scrp->s_nextp) != NULL);

	// Perform update?
	if(n > 0) {

		// Yes, update ETerm settings.
		settermsize(ncol,nrow);

		// Force full screen update.
		opflags |= OpScrRedraw;
		uphard();
		(void) rcset(Success,0,text227,ncol,nrow);
				// "Terminal dimensions set to %hu x %hu"
		}

	return rc.status;
	}

// Switch to given window.
void wswitch(EWindow *winp) {

	curbp = (cursp->s_curwp = curwp = winp)->w_bufp;
	}

// Make the next window (next => down the screen) the current window.  There are no real errors, although the command does
// nothing if there is only one window on the screen.  With an argument, this command finds the <n>th window from the top.
int nextWind(Datum *rp,int n,Datum **argpp) {
	EWindow *winp;
	int nwindows;		// Total number of windows.

	// Check if n is out of range.
	if(n == 0 || (n != INT_MIN && abs(n) > (nwindows = wincount())))
		return rcset(Failure,0,text239,n);
				// "No such window '%d'"

	// Argument given?
	if(n != INT_MIN) {

		// If the argument is negative, it is the nth window from the bottom of the screen...
		if(n < 0)
			n = nwindows + n + 1;

		// otherwise, select that window from the top.
		winp = wheadp;
		while(--n)
			winp = winp->w_nextp;
		}

	// No argument... get next window in list.
	else if((winp = curwp->w_nextp) == NULL)
		winp = wheadp;

	wswitch(winp);
	upmode(NULL);

	return rc.status;
	}

// This command makes the previous window (previous => up the screen) the current window.  There arn't any errors, although
// the command does not do a lot if there is only one window.
int prevWind(Datum *rp,int n,Datum **argpp) {
	EWindow *winp1,*winp2;

	// If we have an argument, process the same way as nextWind(); otherwise, it's too confusing.
	if(n != INT_MIN)
		return nextWind(rp,n,argpp);

	winp1 = wheadp;
	winp2 = curwp;

	if(winp1 == winp2)
		winp2 = NULL;

	while(winp1->w_nextp != winp2)
		winp1 = winp1->w_nextp;

	wswitch(winp1);
	upmode(NULL);

	return rc.status;
	}

// Move the current window up by "n" lines and compute the new top line of the window.
int moveWindUp(Datum *rp,int n,Datum **argpp) {
	Line *lnp;
	int i;
	WindFace *wfp = &curwp->w_face;

	// Return if buffer is empty.
	if((lnp = wfp->wf_toplnp) == curbp->b_hdrlnp)
		return rc.status;

	if(n == INT_MIN)
		n = 1;

	wupd_newtop(curwp,wfp->wf_toplnp,-n);
	curwp->w_flags |= WFHard;				// Mode line is still good.

	// Is dot still in the window?
	lnp = wfp->wf_toplnp;
	i = 0;
	do {
		if(lnp == wfp->wf_dot.lnp)
			return rc.status;
		if(lnp == curbp->b_hdrlnp)
			break;
		lnp = lforw(lnp);
		} while(++i < curwp->w_nrows);

	// Nope.  Move it to the center.
	lnp = wfp->wf_toplnp;
	i = curwp->w_nrows / 2;
	while(i-- > 0 && lnp != curbp->b_hdrlnp)
		lnp = lforw(lnp);
	wfp->wf_dot.lnp = lnp;
	wfp->wf_dot.off = 0;

	return rc.status;
	}

// Make the current window the only window on the screen.  Try to set the framing so that dot does not move on the screen.
int onlyWind(Datum *rp,int n,Datum **argpp) {
	EWindow *winp;

	// If there is only one window, nothing to do.
	if(wheadp->w_nextp == NULL)
		return rc.status;

	// Nuke windows before current window.
	while(wheadp != curwp) {
		winp = wheadp;
		cursp->s_wheadp = wheadp = winp->w_nextp;
		--winp->w_bufp->b_nwind;
		wftobf(winp,winp->w_bufp);
		free((void *) winp);
		}

	// Nuke windows after current window.
	while(curwp->w_nextp != NULL) {
		winp = curwp->w_nextp;
		curwp->w_nextp = winp->w_nextp;
		--winp->w_bufp->b_nwind;
		wftobf(winp,winp->w_bufp);
		free((void *) winp);
		}

	// Adjust window parameters.
	wupd_newtop(curwp,curwp->w_face.wf_toplnp,-curwp->w_toprow);
	curwp->w_toprow = 0;
	curwp->w_nrows = term.t_nrow - 2;
	curwp->w_flags |= WFMode | WFHard;

	return rc.status;
	}

// Delete the current window, placing its space in the upper window by default.  If n < -1, force to upper window; if n > 0,
// force to lower window; otherwise, treat n as the default value.  If the current window is the top or bottom window, wrap
// around if necessary to do the force; otherwise, just transfer to the adjacent window.  It is assumed the current screen
// contains at least two windows.
static int delwind(int n) {
	EWindow *targwinp;		// Window to receive deleted space.
	EWindow *winp;			// Work pointer.

	// Find receiving window and transfer lines.  Check for special "wrap around" case first (which only applies if
	// we have at least three windows).
	if(wheadp->w_nextp->w_nextp != NULL && ((curwp == wheadp && n != INT_MIN && n < -1) ||
	 (curwp->w_nextp == NULL && n > 0))) {
		int n;

		// Current window is top or bottom and need to transfer lines to window at opposite end of screen.
		if(curwp == wheadp) {
			targwinp = wnextis(NULL);			// Receiving window (bottom one).
			n = -(curwp->w_nrows + 1);			// Top row adjustment for all windows.
			cursp->s_wheadp = wheadp = curwp->w_nextp;	// Remove current window from list.
			}
		else {
			targwinp = wheadp;
			n = curwp->w_nrows + 1;
			wnextis(curwp)->w_nextp = NULL;
			wupd_newtop(targwinp,targwinp->w_face.wf_toplnp,-n);
			}

		// Adjust top rows of other windows.
		winp = wheadp;
		do {
			winp->w_toprow += n;
			} while((winp = winp->w_nextp) != NULL);
		wheadp->w_toprow = 0;

		// Adjust size of receiving window.
		targwinp->w_nrows += abs(n);
		}
	else {
		// Set winp to window before current one.
		winp = wnextis(curwp);
		if(winp == NULL || (n > 0 && curwp->w_nextp != NULL)) {		// Next window down.
			targwinp = curwp->w_nextp;
			targwinp->w_toprow = curwp->w_toprow;
			if(winp == NULL)
				cursp->s_wheadp = wheadp = targwinp;
			else
				winp->w_nextp = targwinp;
			wupd_newtop(targwinp,targwinp->w_face.wf_toplnp,-(curwp->w_nrows + 1));
			}
		else {								// Next window up.
			targwinp = winp;
			winp->w_nextp = curwp->w_nextp;
			}
		targwinp->w_nrows += curwp->w_nrows + 1;
		}

	// Get rid of the current window.
	--curbp->b_nwind;
	wftobf(curwp,curbp);
	free((void *) curwp);

	wswitch(targwinp);
	targwinp->w_flags |= WFMode | WFHard;

	return rc.status;
	}

// Delete the current window, placing its space in another window per the n value, as described in delwind().  If n == -1, treat
// it as the default value, but also delete the window's buffer if possible.
int deleteWind(Datum *rp,int n,Datum **argpp) {

	// If there is only one window, don't delete it.
	if(wheadp->w_nextp == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	Buffer *oldbufp = curbp;
	if(delwind(n) == Success && n == -1) {
		char bname[NBufName + 1];
		strcpy(bname,oldbufp->b_bname);
		if(bdelete(oldbufp,0) == Success)
			(void) rcset(Success,0,text372,bname);
					// "Deleted buffer '%s'"
		}
	return rc.status;
	}

// Join the current window with the lower window by default.  If n < -1, force join with upper window; if n > 0, force join with
// lower window; otherwise, treat n as default value.  If the current window is the top or bottom window, wrap around if
// necessary to do the force; otherwise, just join with the adjacent window.
int joinWind(Datum *rp,int n,Datum **argpp) {
	EWindow *targwinp;		// Window to delete.

	// If there is only one window, bail out.
	if(wheadp->w_nextp == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Find window to delete.  Check for special "wrap around" case first (which only applies if we have at least three
	// windows).
	if(wheadp->w_nextp->w_nextp != NULL && ((curwp == wheadp && n != INT_MIN && n < -1) ||
	 (curwp->w_nextp == NULL && n > 0))) {
		if(curwp == wheadp) {
			targwinp = wnextis(NULL);	// Nuke bottom window.
			n = 1;
			}
		else {
			targwinp = wheadp;		// Nuke top window.
			n = -2;
			}
		}
	else if(curwp->w_nextp == NULL || (n < -1 && n != INT_MIN && curwp != wheadp)) {
		targwinp = wnextis(curwp);		// Nuke next window up.
		n = 1;
		}
	else {
		targwinp = curwp->w_nextp;		// Nuke next window down.
		n = -2;
		}

	wswitch(targwinp);				// Make target window the current window...
	return delwind(n);				// and delete it.
	}

// Get a unique window id (a mark past the printable-character range for internal use) and return it in *widp.  Return status.
int getwid(ushort *widp) {
	EScreen *scrp;
	EWindow *winp;
	uint id;

	// If no screen yet exists, use the first id.
	id = '~';
	if((scrp = sheadp) == NULL)
		++id;
	else {
		// Get count of all windows and add it to the last user mark value.
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
			scrp = sheadp;
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

// Split the current window.  The top or bottom line is dropped (to make room for a new mode line) and the remaining lines are
// split into an upper and lower window.  A window smaller than three lines cannot be split.  The point remains in whichever
// window contains dot after the split by default.  A line is pushed out of the other window and its dot is moved to the center.
// If n == 0, the point is forced to the opposite (non-default) window.  If n < 0, the size of the upper window is reduced by
// abs(n) lines; if n > 0, the upper window is set to n lines.
int splitWind(Datum *rp,int n,Datum **argpp) {
	EWindow *winp;
	Line *lnp;
	ushort id;
	int nrowu,nrowl,nrowdot;
	bool defupper;			// Default is upper window?
	WindFace *wfp = &curwp->w_face;

	// Make sure we have enough space and can obtain a unique id.  If so, create a new window.
	if(curwp->w_nrows < 3)
		return rcset(Failure,0,text293,curwp->w_nrows);
			// "Cannot split a %d-line window"
	if(getwid(&id) != Success)
		return rc.status;
	if((winp = (EWindow *) malloc(sizeof(EWindow))) == NULL)
		return rcset(Panic,0,text94,"splitWind");
			// "%s(): Out of memory!"

	// Find dot row.
	nrowdot = 0;
	for(lnp = wfp->wf_toplnp; lnp != wfp->wf_dot.lnp; lnp = lforw(lnp))
		++nrowdot;

	// Update some settings.
	++curbp->b_nwind;			// Now displayed twice (or more).
	winp->w_bufp = curbp;
	winp->w_face = *wfp;			// For now.
	winp->w_flags = winp->w_force = 0;
	winp->w_id = id;
#if Color
	// Set the colors of the new window.
	winp->w_face.wf_fcolor = gfcolor;
	winp->w_bcolor = gbcolor;
#endif
	// Calculate new window sizes.
	nrowu = (curwp->w_nrows - 1) / 2;	// Upper window (default).
	if(n != INT_MIN) {
		if(n < 0) {
			if((nrowu += n) < 1)
				nrowu = 1;
			}
		else if(n > 0)
			nrowu = (n < curwp->w_nrows - 1) ? n : curwp->w_nrows - 2;
		}
	nrowl = (curwp->w_nrows - 1) - nrowu;	// Lower window.

	// Make new window the bottom one.
	winp->w_nextp = curwp->w_nextp;
	curwp->w_nextp = winp;
	curwp->w_nrows = nrowu;
	winp->w_nrows = nrowl;
	winp->w_toprow = curwp->w_toprow + nrowu + 1;

	// Adjust current window's top line if needed.
	if(nrowdot > nrowu)
		wfp->wf_toplnp = lforw(wfp->wf_toplnp);

	// Move down nrowu lines to find top line of lower window.  Stop if we slam into end-of-buffer.
	if(nrowdot != nrowu) {
		lnp = wfp->wf_toplnp;
		while(lnp != curbp->b_hdrlnp) {
			lnp = lforw(lnp);
			if(--nrowu == 0)
				break;
			}
		}

	// Set top line and dot line of each window as needed, keeping in mind that buffer may be empty (so top and dot point
	// to the header line) or have just a few lines in it.  In the latter case, set top in the bottom window to the last
	// line of the buffer and dot to same line, except for special case described below.
	if(nrowdot < curwp->w_nrows) {

		// Dot is in old (upper) window.  Fixup new (lower) window.
		defupper = true;

		// Hit end of buffer looking for top?
		if(lnp == curbp->b_hdrlnp) {

			// Yes, lines in window being split do not extend past the middle.
			winp->w_face.wf_toplnp = lback(lnp);

			// Set dot to last line unless it is already there or at end of buffer, in which case it will be
			// visible in the lower window.
			if((lnp = wfp->wf_dot.lnp) != curbp->b_hdrlnp && lnp != lback(curbp->b_hdrlnp)) {
				winp->w_face.wf_dot.lnp = lback(curbp->b_hdrlnp);
				winp->w_face.wf_dot.off = 0;
				}
			}
		else {
			// No, save current line as top and press onward to find spot to place dot.
			winp->w_face.wf_toplnp = lnp;
			nrowu = nrowl / 2;
			while(nrowu-- > 0) {
				if((lnp = lforw(lnp)) == curbp->b_hdrlnp)
					break;
				}
	
			// Set dot line to mid-point of lower window or last line of buffer.
			winp->w_face.wf_dot.lnp = (lnp == curbp->b_hdrlnp) ? lback(lnp) : lnp;
			winp->w_face.wf_dot.off = 0;
			}
		}
	else {
		// Dot is in new (lower) window.  Fixup both windows.
		defupper = false;

		// Set top line of lower window (dot is already correct).
		winp->w_face.wf_toplnp = (lnp == curbp->b_hdrlnp) ? lback(lnp) : lnp;

		// Set dot in upper window to middle.
		nrowu = curwp->w_nrows / 2;
		lnp = wfp->wf_toplnp;
		while(nrowu-- > 0)
			lnp = lforw(lnp);
		wfp->wf_dot.lnp = lnp;
		wfp->wf_dot.off = 0;
		}

	// Both windows are now set up.  All that's left is to set the current window to the bottom one if needed and set
	// the window-update flags.
	if((n != 0 && !defupper) || (n == 0 && defupper))
		cursp->s_curwp = curwp = winp;
	else
		winp->w_flags |= WFMode;
	winp->w_flags |= WFHard;
	curwp->w_flags |= WFMode | WFHard;

	return rc.status;
	}

// Grow or shrink the current window.  If how == 0, set window size to n lines; otherwise, shrink (how < 0) or grow (how > 0) by
// abs(n) lines.  Find the window that loses or gains space and make sure the window that shrinks is big enough.  If n < 0, try
// to use upper window; otherwise, lower.  If it's a go, set the window flags and let the redisplay system do all the hard work.
// (Can't just set "force reframe" because dot would move.) Return status.
int gswind(Datum *rp,int n,int how) {
	EWindow *adjwp;
	bool grow = (how > 0);

	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		return rc.status;		// Nothing to do.

	if(wheadp->w_nextp == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Figure out which window (next or previous) to steal lines from or give lines to.
	adjwp = curwp->w_nextp;
	if(curwp != wheadp && (n < 0 || adjwp == NULL))
		adjwp = wnextis(curwp);
	if(n < 0)
		n = -n;
	else if(how == 0) {

		// Want n-line window.  Convert n to a size adjustment.
		if(n > curwp->w_nrows) {
			n -= curwp->w_nrows;
			grow = true;
			}
		else if(n == curwp->w_nrows)
			return rc.status;	// Nothing to do.
		else {
			n = curwp->w_nrows - n;
			grow = false;
			}
		}

	if(grow) {
		// Adjacent window big enough?
		if(adjwp->w_nrows <= n)
			return rcset(Failure,0,text207,n,n == 1 ? "" : "s");
				// "Cannot get %d line%s from adjacent window"

		// Yes, proceed.
		if(curwp->w_nextp == adjwp) {		// Shrink below.
			wupd_newtop(adjwp,adjwp->w_face.wf_toplnp,n);
			adjwp->w_toprow += n;
			}
		else {					// Shrink above.
			wupd_newtop(curwp,curwp->w_face.wf_toplnp,-n);
			curwp->w_toprow -= n;
			}
		curwp->w_nrows += n;
		adjwp->w_nrows -= n;
		}
	else {
		// Current window big enough?
		if(curwp->w_nrows <= n)
			return rcset(Failure,0,text93,n,n == 1 ? "" : "s");
				// "Current window too small to shrink by %d line%s"

		// Yes, proceed.
		if(curwp->w_nextp == adjwp) {		// Grow below.
			wupd_newtop(adjwp,adjwp->w_face.wf_toplnp,-n);
			adjwp->w_toprow -= n;
			}
		else {					// Grow above.
			wupd_newtop(curwp,curwp->w_face.wf_toplnp,n);
			curwp->w_toprow += n;
			}
		curwp->w_nrows -= n;
		adjwp->w_nrows += n;
		}

	curwp->w_flags |= WFMode | WFHard;
	adjwp->w_flags |= WFMode | WFHard;

	return rc.status;
	}

// Resize the current window to the requested size.
int resizeWind(Datum *rp,int n,Datum **argpp) {

	// Ignore if no argument or already requested size.
	if(n == INT_MIN || n == curwp->w_nrows)
		return rc.status;

	// Error if negative.
	if(n < 0)
		return rcset(Failure,0,text39,text223,n,0);
			// "%s (%d) must be %d or greater","Size argument"

	// Error if only one window.
	if(wheadp->w_nextp == NULL)
		return rcset(Failure,RCNoFormat,text294);
			// "Only one window"

	// Set all windows on current screen to same size if n == 0; otherwise, change current window only.
	if(n == 0) {
		int count = wincount();
		int size = (term.t_nrow - 1) / count - 1;	// Minimum size of each window, excluding mode line.
		int leftover0 = (term.t_nrow - 1) % count;

		// Remember current window, then step through all windows repeatedly, setting new size of each if possible,
		// until all windows have been set successfully or become deadlocked (which can happen if there are numerous
		// tiny windows).  Give up in the latter case and call it good (a very rare occurrence).
		EWindow *oldwp = curwp;
		EWindow *winp;
		int leftover;
		bool success,changed;
		do {
			winp = wheadp;
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

// Find a window other than the current window and return it in *winpp.  If only one window exists, split it.
static int getwind(Datum *rp,EWindow **winpp) {
	EWindow *winp;

	if(wheadp->w_nextp == NULL &&			// Only one window...
	 splitWind(rp,INT_MIN,NULL) != Success)		// and it won't split.
		return rc.status;

	// Find a window to use (any but current one).
	winp = wheadp;
	for(;;) {
		if(winp != curwp)
			break;
		winp = winp->w_nextp;
		}
	*winpp = winp;
	return rc.status;
	}

// Determine the disposition of a buffer.  This routine is called by any command that creates or potentially could create a
// buffer.  Once the command has the buffer (which may have just been created), it hands it off to this routine to figure out
// what to do with it.  The action taken is determined by the value of "n" (which is assumed to never be INT_MIN -- the caller
// will always pick one of the following as its default action).  Requested actions are:
//	< -1		Pop buffer and delete it.
//	-1		Pop buffer but don't delete it.
//	0		Leave buffer as is.
//	1		Switch to buffer in current window.
//	2		Display buffer in another window.
//	> 2		Display buffer in another window and switch to that window.
//
// Note that a requested action may not be fulfilled (see Notes below).  In all cases, the name of the buffer is saved in rp.
// Flags are:
//	RendReset	Move dot to beginning of buffer when displaying the buffer in a new window, and unhide buffer after it
//			is rendered (if not deleted).
//	RendAltML	Display the alternate mode line when doing a real pop-up.
//	RendBool	Return an array in rp containing the buffer name and a Boolean argument.
//	RendTrue	Return true Boolean argument; otherwise, false (if RendBool is set).
//
// Use of this routine by these buffer-manipulation commands allows for consistent use of the n argument and eases the user's
// task of remembering command syntax.
//
// Notes:
//	* If pop is requested, but the buffer is already being displayed in a window on the current screen, dot is reset in its
//	  window to the beginning only (buffer is not popped).
//	* A buffer may be in the background or displayed in a window when this routine is called.  In either case, it is left as
//	  is if n == 0.
int render(Datum *rp,int n,Buffer *bufp,ushort flags) {
	EWindow *winp;
	bool nukeBuf = false;

	// Displaying buffer?
	if(n != 0) {
		// Yes.  Popping buffer?
		if(n < 0) {
			// Yes.  Buffer on screen already?
			if(bufp->b_nwind > 0) {

				// Yes, check if its window is on the current screen.
				winp = wheadp;
				do {
					if(winp->w_bufp == bufp) {
						faceinit(&winp->w_face,lforw(bufp->b_hdrlnp),NULL);
						winp->w_flags |= WFHard;
						(void) rcset(Success,0,text28,text58);
							// "%s is being displayed","Buffer"
						goto Retn;
						}
					} while((winp = winp->w_nextp) != NULL);
				}

			// Not on current screen (and "create window" not requested).  Activate buffer if needed, do a real pop
			// up, and set delete flag if requested.
			if(bactivate(bufp) != Success || bpop(bufp,(flags & RendAltML) != 0,true) != Success)
				return rc.status;
			if(n < -1)
				nukeBuf = true;
			}

		// Not popping buffer.  Switch to it?
		else if(n == 1) {
			if(curbp != bufp && bswitch(bufp) != Success)
				return rc.status;
			}

		// Not switching.  Create window?
		else if(n > 1) {
			EWindow *oldwinp;

			if(getwind(rp,&winp) != Success)			// get one...
				return rc.status;
			oldwinp = curwp;					// save old window...
			wswitch(winp);						// make new one current...
			if(bswitch(bufp) != Success)				// and switch to new buffer.
				return rc.status;
			if(flags & RendReset)
				faceinit(&winp->w_face,lforw(curbp->b_hdrlnp),NULL);
			if(n == 2)						// If not a force to new window...
				wswitch(oldwinp);				// restore previous window.
			}
		}
Retn:;
	// Return buffer name and optional Boolean value.
	if(flags & RendBool) {
		Array *aryp;
		if((aryp = anew(2,NULL)) == NULL || dsetstr(bufp->b_bname,aryp->a_elpp[0]) != 0)
			return drcset();
		else {
			dsetbool(flags & RendTrue,aryp->a_elpp[1]);
			if(awrap(rp,aryp) != Success)
				return rc.status;
			}
		}
	else if(dsetstr(bufp->b_bname,rp) != 0)
		return drcset();
	if(nukeBuf)
		(void) bdelete(bufp,0);
	else if(flags & RendReset)
		bufp->b_flags &= ~BFHidden;
	return rc.status;
	}

// Scroll the previous or next window up (backward) or down (forward) a page.
int wscroll(Datum *rp,int n,int (*winfunc)(Datum *rp,int n,Datum **argpp),int (*pagefunc)(Datum *rp,int n,Datum **argpp)) {

	(void) winfunc(rp,INT_MIN,NULL);
	(void) pagefunc(rp,n,NULL);
	(void) (*(winfunc == prevWind ? nextWind : prevWind))(rp,INT_MIN,NULL);

	return rc.status;
	}

#if MMDebug & Debug_ScrDump
// Return line information.
static char *lninfo(Line *lnp) {
	int i;
	char *str;
	static char wkbuf[32];

	if(lnp == NULL) {
		str = "(NULL)";
		i = 12;
		}
	else {
		str = lnp->l_text;
		i = lnp->l_used < 12 ? lnp->l_used : 12;
		}
	sprintf(wkbuf,"%.08x '%.*s'",(uint) lnp,i,str);

	return wkbuf;
	}

// Write buffer information to log file.
static void dumpbuffer(Buffer *bufp) {
	Mark *mkp;

	fprintf(logfile,"Buffer '%s' [%.08x]:\n\tb_prevp: %.08x '%s'\n\tb_nextp: %.08x '%s'\n",
	 bufp->b_bname,(uint) bufp,(uint) bufp->b_prevp,bufp->b_prevp == NULL ? "" : bufp->b_prevp->b_bname,
	 (uint) bufp->b_nextp,bufp->b_nextp == NULL ? "" : bufp->b_nextp->b_bname);
	fprintf(logfile,"\tb_face.wf_toplnp: %s\n\tb_hdrlnp: %s\n\tb_ntoplnp: %s\n\tb_nbotlnp: %s\n",
	 lninfo(bufp->b_face.wf_toplnp),lninfo(bufp->b_hdrlnp),lninfo(bufp->b_ntoplnp),lninfo(bufp->b_nbotlnp));

	fprintf(logfile,"\tb_face.wf_dot.lnp: %s\n\tb_face.wf_dot.off: %d\n",
	 lninfo(bufp->b_face.wf_dot.lnp),bufp->b_face.wf_dot.off);
	fputs("\tMarks:\n",logfile);
	mkp = &bufp->b_mroot;
	do {
		fprintf(logfile,"\t\t'%s': [%s] %d %hd\n",mkp->mk_id,lninfo(mkp->mk_dot.lnp),mkp->mk_dot.off,mkp->mk_force);
		} while(++i < NMARKS);

fprintf(logfile,"\tb_face.wf_fcol: %d\n\tb_nwind: %hu\n\tb_nexec: %hu\n\tb_nalias: %hu\n\tb_flags: %.04x\n\tb_modes: %.04x\n",
	 bufp->b_face.wf_fcol,bufp->b_nwind,bufp->b_mip == NULL ? -1H : bufp->b_mip->mi_nexec,bufp->b_nalias,
	 (uint) bufp->b_flags,(uint) bufp->b_modes);
	fprintf(logfile,"\tb_inpdelimlen: %d\n\tb_inpdelim: '%s'\n\tb_fname: '%s'\n",
	 bufp->b_inpdelimlen,bufp->b_inpdelim,bufp->b_fname);
	}

// Write window information to log file.
static void dumpwindow(EWindow *winp,int windnum) {

	fprintf(logfile,"\tWindow %d [%.08x]:\n\t\tw_nextp: %.08x\n\t\tw_bufp: %.08x '%s'\n\t\tw_face.wf_toplnp: %s\n",
	 windnum,(uint) winp,(uint) winp->w_nextp,(uint) winp->w_bufp,winp->w_bufp->b_bname,lninfo(winp->w_face.wf_toplnp));
fprintf(logfile,"\t\tw_face.wf_dot.lnp: %s\n\t\tw_face.wf_dot.off: %d\n\t\tw_toprow: %hu\n\t\tw_nrows: %hu\n\t\tw_force: %hu\n",
	 lninfo(winp->w_face.wf_dot.lnp),winp->w_face.wf_dot.off,winp->w_toprow,winp->w_nrows,winp->w_force);
	fprintf(logfile,"\t\tw_flags: %.04x\n\t\tw_face.wf_fcol: %d\n",
	 (uint) winp->w_flags,winp->w_face.wf_fcol);
#if Color
	fprintf(logfile,"\t\tw_face.wf_fcolor: %hu\n\t\tw_bcolor: %hu\n",
	 (uint) winp->w_face.wf_fcolor,(uint) winp->w_bcolor);
#endif
	}

// Write screen, window, and buffer information to log file -- for debugging.
void dumpscreens(char *msg) {
	EScreen *scrp;
	EWindow *winp;
	Buffer *bufp;
	int windnum;

	fprintf(logfile,"### %s ###\n\n*SCREENS\n\n",msg);

	// Dump screens and windows.
	scrp = sheadp;
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
	bufp = bheadp;
	do {
		dumpbuffer(bufp);
		} while((bufp = bufp->b_nextp) != NULL);
	}
#endif

// Find a screen, given number, (possibly NULL) pointer to buffer to attach to first window of screen, and (possibly NULL)
// pointer to result.  If the screen is not found and "bufp" is not NULL, create new screen and return rc.status; otherwise,
// return false, ignoring spp.
int sfind(ushort scr_num,Buffer *bufp,EScreen **spp) {
	EScreen *scrp1;
	ushort snum;
	static char myname[] = "sfind";

	// Scan the screen list.  Note that the screen list is empty at program launch.
	for(snum = 0, scrp1 = sheadp; scrp1 != NULL; scrp1 = scrp1->s_nextp) {
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
		scrp1->s_num = snum + 1;
		scrp1->s_flags = 0;
		scrp1->s_nrow = term.t_nrow;
		scrp1->s_ncol = term.t_ncol;

		// Allocate its first window...
		if((winp = (EWindow *) malloc(sizeof(EWindow))) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"
		scrp1->s_wheadp = scrp1->s_curwp = winp;

		// and setup the window's info.
		winp->w_nextp = NULL;
		winp->w_bufp = bufp;
		++bufp->b_nwind;
		bftowf(bufp,winp);
		winp->w_id = id;
		winp->w_toprow = 0;
#if Color
		// Initalize colors to global defaults.
		winp->w_face.wf_fcolor = gfcolor;
		winp->w_bcolor = gbcolor;
#endif
		winp->w_nrows = term.t_nrow - 2;		// "-2" for message and mode lines.
		winp->w_force = 0;

		// Insert new screen at end of screen list.
		scrp1->s_nextp = NULL;
		if((scrp2 = sheadp) == NULL)
			sheadp = scrp1;
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
	if(scrp == cursp)
		return rc.status;

	// Save the current screen's concept of current window.
	cursp->s_curwp = curwp;
	cursp->s_nrow = term.t_nrow;
	cursp->s_ncol = term.t_ncol;

	// Run exit-buffer user hook on current (old) buffer if the new screen's buffer is different.
	Datum *rp;
	bool diffbuf = (scrp->s_curwp->w_bufp != curbp);
	if(diffbuf) {
		if(dnewtrk(&rp) != 0)
			return drcset();
		if(bhook(rp,true) != Success)
			return rc.status;
		}

	// Reset the current screen, window and buffer.
	wheadp = (cursp = scrp)->s_wheadp;
	curbp = (curwp = scrp->s_curwp)->w_bufp;

	// Let the display driver know we need a full screen update.
	opflags |= OpScrRedraw;
	uphard();

	// Run enter-buffer user hook on current (new) buffer.
	if(diffbuf)
		(void) bhook(rp,false);

	return rc.status;
	}

// Bring the next screen in the linked screen list to the front and return its number.  Return status.
int nextScreen(Datum *rp,int n,Datum **argpp) {
	EScreen *scrp;
	int nscreens;		// Total number of screens.

	// Check if n is out of range.
	nscreens = scrcount();
	if(n == 0 || (n != INT_MIN && abs(n) > nscreens))
		return rcset(Failure,0,text240,n);
				// "No such screen '%d'"
	// Argument given?
	if(n != INT_MIN) {

		// If the argument is negative, it is the nth screen from the end of the screen sequence.
		if(n < 0)
			n = nscreens + n + 1;
		}
	else if((n = cursp->s_num + 1) > nscreens)
		n = 1;

	// Find the screen...
	scrp = sheadp;
	while(scrp->s_num != n)
		scrp = scrp->s_nextp;

	// return its number...
	dsetint((long) scrp->s_num,rp);

	// and make it current.
	return sswitch(scrp);
	}

// Create new screen, switch to it, and return its number.  Return status.
int newScreen(Datum *rp,int n,Datum **argpp) {
	EScreen *scrp;

	// Save the current window's settings.
	wftobf(curwp,curbp);

	// Find screen "0" to force-create one and make it current.
	if(sfind(0,curbp,&scrp) != Success || sswitch(scrp) != Success)
		return rc.status;
	dsetint((long) scrp->s_num,rp);

	return rcset(Success,0,text174,scrp->s_num);
			// "Created screen %hu"
	}

// Free all resouces associated with a screen.
static void freescreen(EScreen *scrp) {
	EWindow *winp,*tp;

	// First, free the screen's windows.
	winp = scrp->s_wheadp;
	do {
		--winp->w_bufp->b_nwind;
		wftobf(winp,winp->w_bufp);

		// On to the next window; free this one.
		tp = winp;
		winp = winp->w_nextp;
		free((void *) tp);
		} while(winp != NULL);

	// And now, free the screen itself.
	free((void *) scrp);
	}

// Remove screen from the list and renumber remaining ones.  Update modeline of bottom window if only one left.  Return status.
static int unlistscreen(EScreen *scrp) {
	ushort snum;
	EScreen *tp;

	if(scrp == sheadp)
		sheadp = sheadp->s_nextp;
	else {
		tp = sheadp;
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
	tp = sheadp;
	do {
		tp->s_num = ++snum;
		} while((tp = tp->s_nextp) != NULL);

	// If only one screen left, flag mode line at bottom of screen for update.
	if(snum == 1) {

		// Go to last window and flag it.
		wnextis(NULL)->w_flags |= WFMode;
		}

	return rc.status;
	}

// Get a required n argument (via prompt if interactive) and validate it.  Return true if valid; otherwise, set an error
// (unless nothing was entered) and return false.
static bool getnum(char *prmt,int *np) {

	// Error if in a script.
	if(opflags & OpScript)
		(void) rcset(Failure,RCNoFormat,text57);
			// "Argument expected"
	else {
		Datum *datp;

		if(dnewtrk(&datp) != 0)
			(void) drcset();
		else if(terminp(datp,prmt,NULL,RtnKey,0,0,0) == Success && datp->d_type != dat_nil &&
		 toint(datp) == Success) {
			*np = datp->u.d_int;
			return true;
			}
		}
	return false;
	}

// Delete a screen.  Return status.
int deleteScreen(Datum *rp,int n,Datum **argpp) {
	EScreen *scrp;

	// Get the number of the screen to delete.
	if(n == INT_MIN && !getnum(text243,&n))
				// "Delete screen"
		return rc.status;

	// Make sure it exists.
	if(!sfind((ushort) n,NULL,&scrp))
		return rcset(Failure,0,text240,n);
			// "No such screen '%d'"

	// It can't be current.
	if(scrp == cursp)
		return rcset(Failure,RCNoFormat,text241);
			// "Screen is being displayed"

	// Everything's cool... nuke it.
	if(unlistscreen(scrp) != Success)
		return rc.status;
	freescreen(scrp);

	return rcset(Success,0,text178,n);
			// "Deleted screen %d"
	}

// Build and pop up a buffer containing a list of all screens and their associated buffers.  Render buffer and return status.
int showScreens(Datum *rp,int n,Datum **argpp) {
	Buffer *slistp;
	EScreen *scrp;			// Pointer to current screen to list.
	EWindow *winp;			// Pointer into current screens window list.
	uint wnum;
	DStrFab rpt;
	int windcol = 7;
	int filecol = 37;
	char *str,wkbuf[filecol + 16];

	// Get a buffer and open a string-fab object.
	if(sysbuf(text160,&slistp) != Success)
			// "Screens"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	// Construct the header lines.
	if(dputs(text89,&rpt) != 0 || dputc('\n',&rpt) != 0 ||
	    // "Screen Window      Buffer                File"
	 dputs("------ ------  --------------------  -------------------------------",&rpt) != 0)
		return drcset();

	// For all screens...
	scrp = sheadp;
	do {
		// Store the screen number.
		sprintf(wkbuf,"\n%4hu   ",scrp->s_num);
		str = strchr(wkbuf,'\0');

		// List screen's window numbers and buffer names.
		wnum = 0;
		winp = scrp->s_wheadp;
		do {
			Buffer *bufp = winp->w_bufp;

			// Indent if not first time through.
			if(wnum != 0) {
				wkbuf[0] = '\n';
				str = wkbuf + 1;
				do {
					*str++ = ' ';
					} while(str <= wkbuf + windcol);
				}

			// Store window number, buffer name, and filename.
			sprintf(str,"%4u   %c%s",++wnum,(bufp->b_flags & BFChgd) ? '*' : ' ',bufp->b_bname);
			str = strchr(str,'\0');
			if(bufp->b_fname != NULL)			// Pad if filename exists.
				do {
					*str++ = ' ';
					} while(str <= wkbuf + filecol);
			*str = '\0';					// Save buffer and add filename.
			if(dputs(wkbuf,&rpt) != 0 ||
			 (bufp->b_fname != NULL && dputs(bufp->b_fname,&rpt) != 0))
				return drcset();

			// On to the next window.
			} while((winp = winp->w_nextp) != NULL);

		// On to the next screen.
		} while((scrp = scrp->s_nextp) != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(bappend(slistp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,slistp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}

// Get number of screens.
int scrcount(void) {
	EScreen *scrp;
	int count;

	count = 0;
	scrp = sheadp;
	do {
		++count;
		} while((scrp = scrp->s_nextp) != NULL);

	return count;
	}
