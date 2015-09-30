// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// display.c	High level display routines for MightEMacs.
//
// This file contains functions that generally call the lower level terminal display functions in vterm.c.

#include "os.h"
#include <stdarg.h>
#include <ctype.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

// Flag all mode lines in the current screen for updating.  If bufp is not NULL, mark only those windows.
void upmode(Buffer *bufp) {
	EWindow *winp;

	winp = wheadp;
	do {
		if(bufp == NULL || winp->w_bufp == bufp)
			winp->w_flags |= WFMODE;
		} while((winp = winp->w_nextp) != NULL);
	}

// Force hard updates on all windows in the current screen.
void uphard(void) {
	EWindow *winp;

	winp = wheadp;
	do {
		winp->w_flags |= WFHARD | WFMODE;
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

// Find window on current screen displaying given buffer (pointer or name).  If found, set *winpp (if winpp not NULL) to pointer
// and return its ordinal number.  If not found, return zero (and ignore winpp).
int bufWind(Buffer *bufp,char *bname,EWindow **winpp) {
	EWindow *winp;
	int wnum = 0;

	// Scan the window list.
	winp = wheadp;
	do {
		++wnum;
		if(winp->w_bufp == bufp || (bname != NULL && strcmp(winp->w_bufp->b_bname,bname) == 0)) {
			if(winpp != NULL)
				*winpp = winp;
			return wnum;
			}
		} while((winp = winp->w_nextp) != NULL);

	return 0;
	}

// Restore message line cursor position.  Return status.
int mlrestore(void) {

	if(movecursor(term.t_nrow - 1,ml.ttcol) == SUCCESS)
		(void) TTflush();
	return rc.status;
	}

// Erase the message line.  Return status.
int mlerase(int f) {
	int i;

	// If we are not currently echoing on the command line and it's not a force, abort this.
	if((modetab[MDR_GLOBAL].flags & MDMSG) == 0 && !(f & MLFORCE))
		return rc.status;

	// Home the cursor.
	i = ml.ttcol;
	if(movecursor(term.t_nrow - 1,0) != SUCCESS)
		return rc.status;
#if COLOR && 0
	if(TTforg(gfcolor) != SUCCESS || TTbacg(gbcolor) != SUCCESS)
		return rc.status;
#endif
	// Erase line if needed.
	if(i != 0) {
		if(opflags & OPHAVEEOL) {
			if(TTeeol() != SUCCESS)
				return rc.status;
			}
		else {
			for(i = 0; i < (int) term.t_ncol - 1; ++i)
				if(TTputc(' ') != SUCCESS)
					return rc.status;

			// Reset cursor.
			if(movecursor(term.t_nrow - 1,0) != SUCCESS)
				return rc.status;
			}

		// Reset buffer pointer and update message line on screen.
		ml.spanptr = ml.span;
		(void) TTflush();
		}

	return rc.status;
	}

// Write a character into the message line with invisible characters exposed, unless v is vz_raw.  Keep track of the physical
// cursor position and number of characters output.  Return status.
int mlputc(int c,enum e_viz v) {

	// Raw character?
	if(v == vz_raw) {

		// Yes, backspace?
		if(c == '\b') {

			// Undo last.
			if(ml.ttcol > 0) {
				int len = *--ml.spanptr;

				ml.ttcol -= len;
				do {
					if(TTputc('\b') != SUCCESS || TTputc(' ') != SUCCESS || TTputc('\b') != SUCCESS)
						return rc.status;
					} while(--len > 0);
				}
			}

		// Not a backspace.  Display character unmodified (if room).
		else if(ml.ttcol < (int) term.t_ncol) {
			if(TTputc(c) != SUCCESS)
				return rc.status;
			++ml.ttcol;
			*ml.spanptr++ = 1;
			}
		}
	else {
		char *strp;
		int ttcol0 = ml.ttcol;

		// Not raw.  Display char literal (if any) and remember length.
		c = *(strp = chlit(c,v == vz_wrap));
		do {
			if(ml.ttcol < (int) term.t_ncol) {
				if(TTputc(c) != SUCCESS)
					return rc.status;
				++ml.ttcol;
				}
			} while((c = *++strp) != '\0');
		if(ml.ttcol > ttcol0)
			*ml.spanptr++ = ml.ttcol - ttcol0;
		}

	return rc.status;
	}

// Write out an integer, in the specified radix.  Update the physical cursor position.  Return status.
static int mlputi(int i,int r) {
	int q;
	static char hexdigits[] = "0123456789abcdef";

	if(i < 0) {
		i = -i;
		if(mlputc('-',vz_raw) != SUCCESS)
			return rc.status;
		}
	q = i / r;
	if(q != 0 && mlputi(q,r) != SUCCESS)
		return rc.status;
	return mlputc(hexdigits[i % r],vz_raw);
	}

// Do the same for a long integer.
static int mlputli(long l,int r) {
	long q;

	if(l < 0) {
		l = -l;
		if(mlputc('-',vz_raw) != SUCCESS)
			return rc.status;
		}
	q = l / r;
	if(q != 0 && mlputli(q,r) != SUCCESS)
		return rc.status;
	return mlputc((int)(l % r) + '0',vz_raw);
	}

#if MLSCALED
// Write out a scaled integer with two decimal places.  Return status.
static int mlputf(int s) {
	int i;		// Integer portion of number.
	int f;		// Fractional portion of number.

	// Break it up.
	i = s / 100;
	f = s % 100;

	// Send it out with a decimal point.
	if(mlputi(i,10) == SUCCESS && mlputc('.',vz_raw) == SUCCESS && mlputc((f / 10) + '0',vz_raw) == SUCCESS)
		(void) mlputc((f % 10) + '0',vz_raw);

	return rc.status;
	}
#endif

// Prepare for new message line message.  Return rc.status (SUCCESS) if successful; otherwise, NOTFOUND (bypassing rcset()).
static int mlbegin(int f) {

	// If we are not currently echoing on the command line and it's not a force, abort this.
	if((modetab[MDR_GLOBAL].flags & MDMSG) == 0 && (f & MLFORCE) == 0)
		return NOTFOUND;

	// Position cursor and/or begin wrap, if applicable.
	if((f & MLHOME) && mlerase(f | MLFORCE) != SUCCESS)
		return rc.status;
	if(f & MLWRAP)
		(void) mlputc('[',vz_raw);

	return rc.status;
	}

// Finish message line message.  Return status.
static int mlend(int f) {

	// Finish wrap and flush message.
	if(!(f & MLWRAP) || mlputc(']',vz_raw) == SUCCESS)
		(void) TTflush();
	return rc.status;
	}

// Write text into the message line, given flag word, format string and optional arguments.  If MLHOME is set in f, move cursor
// to bottom left corner of screen first.  If MLFORCE is set in f, write string regardless of the MDMSG global flag setting.  If
// MLWRAP is set in f, wrap message within '[' and ']' characters before displaying it.
//
// A small class of printf-like format items is handled.  Set the "message line" flag to true.  Don't write beyond the end of
// the current terminal width.  Return status.
int mlprintf(int f,char *fmt,...) {
	int c;			// Current char in format string.
	va_list ap;		// Pointer to current data field.
	int arg_int;		// Integer argument.
	long arg_long;		// Long argument.
	char *arg_str;		// String argument.

	// Bag it if not currently echoing and not a force.
	if(mlbegin(f) != SUCCESS)
		return rc.status;

	// Process arguments.
	va_start(ap,fmt);
	while((c = *fmt++) != '\0') {
		if(c != '%')
			(void) mlputc(c,vz_show);
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
					(void) mlputs(MLFORCE,arg_str,vz_show);
					break;
#if MLSCALED
				case 'f':
					arg_int = va_arg(ap,int);
					(void) mlputf(arg_int);
					break;
#endif
				case 'c':
					c = va_arg(ap,int);
					// Fall through.
				default:
					(void) mlputc(c,vz_show);
				}
			}
		if(rc.status != SUCCESS)
			return rc.status;
		}
	va_end(ap);
	return mlend(f);
	}

// Write a string to the message line, given flag word, message, and processing mode.  If MLHOME is set in f, move cursor to
// bottom left corner of screen first.  If MLFORCE is set in f, write string regardless of the MDMSG global flag setting.  If
// MLWRAP is set in f, wrap message within '[' and ']' characters before displaying it.  Pass v to mlputc().  Return status.
int mlputs(int f,char *strp,enum e_viz v) {

	// Write string if currently echoing or a force.
	if(mlbegin(f) == SUCCESS) {
		int c;

		// Display the string.
		while((c = *strp++) != '\0')
			if(mlputc(c,v) != SUCCESS)
				return rc.status;

		// Finish wrap and flush message.
		(void) mlend(f);
		}

	return rc.status;
	}

#if 0
// Copy len bytes from a string to the message line in the same manner as mlputs(), given flag word, message, and length.  Pass
// v to mlputc().  Return status.
int mlcpy(int f,char *strp,size_t len,enum e_viz v) {

	// Write string if currently echoing or a force.
	if(mlbegin(f) == SUCCESS) {
		char *strpz;

		for(strpz = strp + len; strp < strpz; ++strp)
			if(mlputc(*strp,v) != SUCCESS)
				return rc.status;

		// Finish wrap and flush message.
		(void) mlend(f);
		}
	return rc.status;
	}
#endif

// Write a value object to the message line.  Return status.
int mlputv(int f,Value *vp,enum e_viz v) {

	// Write string if currently echoing or a force.
	if(mlbegin(f) == SUCCESS) {
		int c;
		char *strp;
		char wkbuf[LONGWIDTH];

		if(vp->v_type == VALINT)
			sprintf(strp = wkbuf,"%ld",vp->u.v_int);
		else
			strp = vp->v_strp;
		while((c = *strp++) != '\0')
			if(mlputc(c,v) != SUCCESS)
				return rc.status;

		// Finish wrap and flush message.
		(void) mlend(f);
		}
	return rc.status;
	}

// Get number of windows on current screen.  (Mainly for macro use.)
int wincount(void) {
	EWindow *winp;
	int count;

	count = 0;
	winp = wheadp;
	do {
		++count;
		} while((winp = winp->w_nextp) != NULL);
	return count;
	}

// Reset terminal.  Get the current terminal dimensions, update the ETerm structure, flag all screens that have different
// dimensions for a "window resize", and flag current screen for a "redraw".  Force update if n > 0.  Return status.  Needs to
// be called when the size of the terminal window changes; for example, when switching from portrait to landscape viewing on a
// mobile device.
int resetTermc(Value *rp,int n) {
	EScreen *scrp;		// Current screen being examined.
	ushort ncol,nrow;	// Current terminal dimensions.

	// Get current terminal size.
	if(gettermsize(&ncol,&nrow) != SUCCESS)
		return rc.status;

	// In all screens ...
	scrp = sheadp;
	n = (n <= 0) ? 0 : 1;
	do {
		// Flag screen if its not the current terminal size.
		if(scrp->s_nrow != nrow || scrp->s_ncol != ncol) {
			scrp->s_flags |= ESRESIZE;
			n = 1;
			}
		} while((scrp = scrp->s_nextp) != NULL);

	// Perform update?
	if(n > 0) {

		// Yes, update ETerm settings.
		settermsize(ncol,nrow);

		// Force full screen update.
		opflags |= OPSCREDRAW;
		uphard();
		(void) rcset(SUCCESS,0,text227,ncol,nrow);
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
int nextWind(Value *rp,int n) {
	EWindow *winp;
	int nwindows;		// Total number of windows.

	// Check if n is out of range.
	if(n == 0 || (n != INT_MIN && abs(n) > (nwindows = wincount())))
		return rcset(FAILURE,0,text239,n);
				// "No such window '%d'"

	// Argument given?
	if(n != INT_MIN) {

		// If the argument is negative, it is the nth window from the bottom of the screen ...
		if(n < 0)
			n = nwindows + n + 1;

		// otherwise, select that window from the top.
		winp = wheadp;
		while(--n)
			winp = winp->w_nextp;
		}

	// No argument ... get next window in list.
	else if((winp = curwp->w_nextp) == NULL)
		winp = wheadp;

	wswitch(winp);
	upmode(NULL);

	return rc.status;
	}

// This command makes the previous window (previous => up the screen) the current window.  There arn't any errors, although
// the command does not do a lot if there is only one window.
int prevWind(Value *rp,int n) {
	EWindow *winp1,*winp2;

	// If we have an argument, process the same way as nextWind(); otherwise, it's too confusing.
	if(n != INT_MIN)
		return nextWind(rp,n);

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
int moveWindUp(Value *rp,int n) {
	Line *lnp;
	int i;
	WindFace *wfp = &curwp->w_face;

	// Return if n is zero or buffer is empty.
	if(n == 0 || (lnp = wfp->wf_toplnp) == curbp->b_hdrlnp)
		return rc.status;

	if(n == INT_MIN)
		n = 1;

	wupd_newtop(curwp,wfp->wf_toplnp,-n);
	curwp->w_flags |= WFHARD;				// Mode line is still good.

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
int onlyWind(Value *rp,int n) {
	EWindow *winp;

	// If there is only one window, nothing to do.
	if(wheadp->w_nextp == NULL)
		return rc.status;

	// Nuke windows before current window.
	while(wheadp != curwp) {
		winp = wheadp;
		cursp->s_wheadp = wheadp = winp->w_nextp;
		--winp->w_bufp->b_nwind;
		winp->w_bufp->b_face = winp->w_face;
		free((void *) winp);
		}

	// Nuke windows after current window.
	while(curwp->w_nextp != NULL) {
		winp = curwp->w_nextp;
		curwp->w_nextp = winp->w_nextp;
		--winp->w_bufp->b_nwind;
		winp->w_bufp->b_face = winp->w_face;
		free((void *) winp);
		}

	// Adjust window parameters.
	wupd_newtop(curwp,curwp->w_face.wf_toplnp,-curwp->w_toprow);
	curwp->w_toprow = 0;
	curwp->w_nrows = term.t_nrow - 2;
	curwp->w_flags |= WFMODE | WFHARD;

	return rc.status;
	}

// Delete the current window, placing its space in the upper window by default.  If n < 0, force to upper window; if n > 0,
// force to lower window.  If the current window is the top or bottom window, wrap around if necessary to do the force;
// otherwise, just transfer to the adjacent window.
int deleteWind(Value *rp,int n) {
	EWindow *targwinp;		// Window to receive deleted space.
	EWindow *winp;			// Work pointer.

	// If there is only one window, don't delete it.
	if(wheadp->w_nextp == NULL)
		return rcset(FAILURE,0,text294);
			// "Only one window"

	// Find receiving window and transfer lines.  Check for special "wrap around" case first (which only applies if
	// we have at least three windows).
	if(wheadp->w_nextp->w_nextp != NULL && ((curwp == wheadp && n != INT_MIN && n < 0) ||
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
	curbp->b_face = curwp->w_face;
	free((void *) curwp);

	wswitch(targwinp);
	targwinp->w_flags |= WFMODE | WFHARD;

	return rc.status;
	}

// Join the current window with the lower window by default.  If n < 0, force join with upper window; if n > 0,
// force join with lower window.  If the current window is the top or bottom window, wrap around if necessary to do the force;
// otherwise, just join with the adjacent window.
int joinWind(Value *rp,int n) {
	EWindow *targwinp;		// Window to delete.

	// If there is only one window, bail out.
	if(wheadp->w_nextp == NULL)
		return rcset(FAILURE,0,text294);
			// "Only one window"

	// Find window to delete.  Check for special "wrap around" case first (which only applies if we have at least three
	// windows).
	if(wheadp->w_nextp->w_nextp != NULL && ((curwp == wheadp && n != INT_MIN && n < 0) ||
	 (curwp->w_nextp == NULL && n > 0))) {
		if(curwp == wheadp) {
			targwinp = wnextis(NULL);	// Nuke bottom window.
			n = 1;
			}
		else {
			targwinp = wheadp;		// Nuke top window.
			n = -1;
			}
		}
	else if(curwp->w_nextp == NULL || (n < 0 && n != INT_MIN && curwp != wheadp)) {
		targwinp = wnextis(curwp);		// Nuke next window up.
		n = 1;
		}
	else {
		targwinp = curwp->w_nextp;		// Nuke next window down.
		n = -1;
		}

	// Make target window the current window ...
	wswitch(targwinp);

	// and delete it.
	return deleteWind(rp,n);
	}

// Split the current window.  The top or bottom line is dropped (to make room for a new mode line) and the remaining lines are
// split into an upper and lower window.  A window smaller than three lines cannot be split.  The cursor remains in whichever
// window contains dot after the split by default.  A line is pushed out of the other window and its dot is moved to the center.
// If n == 0, the cursor is forced to the opposite (non-default) window.  If n < 0, the size of the upper window is reduced by
// abs(n) lines; if n > 0, the upper window is set to n lines.
int splitWind(Value *rp,int n) {
	EWindow *winp;
	Line *lnp;
	int nrowu,nrowl,nrowdot;
	bool defupper;			// Default is upper window?
	WindFace *wfp = &curwp->w_face;

	// Make sure we have enough space.  If so, create a new window.
	if(curwp->w_nrows < 3)
		return rcset(FAILURE,0,text293,curwp->w_nrows);
			// "Cannot split a %d-line window"
	if((winp = (EWindow *) malloc(sizeof(EWindow))) == NULL)
		return rcset(PANIC,0,text94,"splitWind");
			// "%s(): Out of memory!"

	// Find dot row.
	nrowdot = 0;
	for(lnp = wfp->wf_toplnp; lnp != wfp->wf_dot.lnp; lnp = lforw(lnp))
		++nrowdot;

	// Update some settings.
	++curbp->b_nwind;			// Now displayed twice (or more).
	winp->w_bufp = curbp;
	winp->w_face = curwp->w_face;		// For now.
	winp->w_flags = winp->w_force = 0;
#if COLOR
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
		winp->w_flags |= WFMODE;
	winp->w_flags |= WFHARD;
	curwp->w_flags |= WFMODE | WFHARD;

	return rc.status;
	}

// Enlarge or shrink the current window.  Find the window that loses or gains space and make sure the window that shrinks is big
// enough.  If it's a go, set the window flags and let the redisplay system do all the hard work.  (Can't just set "force
// reframe" because dot would move.)
int gswind(Value *rp,int n,bool grow) {
	EWindow *adjwp;

	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		return rc.status;		// Nothing to do.

	if(wheadp->w_nextp == NULL)
		return rcset(FAILURE,0,text294);
			// "Only one window"

	// Figure out which window (next or previous) to steal lines from.
	adjwp = curwp->w_nextp;
	if(curwp != wheadp && (n < 0 || adjwp == NULL))
		adjwp = wnextis(curwp);
	if(n < 0)
		n = -n;

	if(grow) {
		// Adjacent window big enough?
		if(adjwp->w_nrows <= n)
			return rcset(FAILURE,0,text207,n,n == 1 ? "" : "s");
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
			return rcset(FAILURE,0,text93,n,n == 1 ? "" : "s");
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

	curwp->w_flags |= WFMODE | WFHARD;
	adjwp->w_flags |= WFMODE | WFHARD;

	return rc.status;
	}

// Resize the current window to the requested size.
int resizeWind(Value *rp,int n) {

	// Ignore if no argument or already requested size.
	if(n == INT_MIN || n == 0 || n == curwp->w_nrows)
		return rc.status;

	// Error if negative.
	if(n < 0)
		return rcset(FAILURE,0,text39,text223,n,0);
			// "%s (%d) must be %d or greater","Size argument"

	return (n > curwp->w_nrows) ? gswind(rp,n - curwp->w_nrows,true) : gswind(rp,curwp->w_nrows - n,false);
	}

// Find a window other than the current window and return it in *winpp.  If only one window exists, split it.
static int getwind(Value *rp,EWindow **winpp) {
	EWindow *winp;

	if(wheadp->w_nextp == NULL &&			// Only one window ...
	 splitWind(rp,INT_MIN) != SUCCESS)		// and it won't split.
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
//	RENDRESET	Move dot to beginning of buffer when displaying the buffer in a new window.
//	RENDALTML	Display the alternate mode line when doing a real pop-up.
//	RENDBOOL	Return a tab and boolean argument in rp in addition to buffer name.
//	RENDTRUE	Return true boolean argument; otherwise, false.  (If RENDBOOL is set.)
//
// Use of this routine by these buffer-manipulation commands allows for consistent use of the n argument and eases the user's
// task of remembering command syntax.
//
// Notes:
//	* If pop is requested, but the buffer is already being displayed in a window on the current screen, dot is reset in its
//	  window to the beginning only (buffer is not popped).
//	* A buffer may be in the background or displayed in a window when this routine is called.  In either case, it is left as
//	  is if n == 0.
int render(Value *rp,int n,Buffer *bufp,uint flags) {
	EWindow *winp;

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
						faceinit(&winp->w_face,lforw(bufp->b_hdrlnp));
						winp->w_flags |= WFHARD;
						(void) rcset(SUCCESS,0,text28,text58);
							// "%s is being displayed","Buffer"
						goto retn;
						}
					} while((winp = winp->w_nextp) != NULL);
				}

			// Not on current screen (and "create window" not requested).  If buffer is inactive, switch to it first
			// (and read file), then switch back to current buffer.
			if((bufp->b_flags & BFACTIVE) == 0) {
				Buffer *oldbufp = curbp;
				if(bswitch(bufp) != SUCCESS || bswitch(oldbufp) != SUCCESS)
					return rc.status;
				}

			// Now do a real pop up and delete buffer if requested.
			if(bpop(bufp,(flags & RENDALTML) != 0,true) == SUCCESS && n < -1)
				(void) bdelete(bufp,0);
			}

		// Not popping buffer.  Switch to it?
		else if(n == 1) {
			if(curbp != bufp)
				(void) bswitch(bufp);
			}

		// Not switching.  Create window?
		else if(n > 1) {
			EWindow *oldwinp;

			if(getwind(rp,&winp) != SUCCESS)			// get one ...
				return rc.status;
			oldwinp = curwp;					// save old window ...
			wswitch(winp);						// make new one current ...
			if(bswitch(bufp) != SUCCESS)				// and switch to new buffer.
				return rc.status;
			if(flags & RENDRESET)
				faceinit(&winp->w_face,lforw(curbp->b_hdrlnp));
			if(n == 2)						// If not a force to new window ...
				wswitch(oldwinp);				// restore previous window.
			}
		}
retn:;
	// Return buffer name and optional boolean value.
	char *strp,rbuf[NBUFN + 12];

	strp = stpcpy(rbuf,bufp->b_bname);
	if(flags & RENDBOOL) {
		*strp++ = '\t';
		strcpy(strp,flags & RENDTRUE ? val_true : val_false);
		}
	return vsetstr(rbuf,rp) != 0 ? vrcset() : rc.status;
	}

// Scroll the previous or next window up (backward) or down (forward) a page.
int wscroll(Value *rp,int n,int (*winfunc)(Value *rp,int n),int (*pagefunc)(Value *rp,int n)) {

	(void) winfunc(rp,INT_MIN);
	(void) pagefunc(rp,n);
	(void) (*(winfunc == prevWind ? nextWind : prevWind))(rp,INT_MIN);

	return rc.status;
	}
