// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// nav.c	Basic movement (navigation) functions for MightEMacs.
//
// These routines move the cursor around on the screen.  They compute a new value for the cursor, then adjust dot.  The display
// code always updates the cursor location, so only moves between lines or functions that adjust the top line in the window and
// invalidate the framing are hard.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"

// Local variables.
static int targcol;			// Goal column for vertical line movements.

// Move the point backward by "n" characters.  ("n" is assumed to be >= 0.)  Return NOTFOUND (bypassing rcset()) if move would
// go out of the buffer.  Set the move flag if dot moves to a different line.
int backch(int n) {
	Line *lnp;
	Dot *dotp = &curwp->w_face.wf_dot;

	while(n-- > 0) {
		if(dotp->off == 0) {
			if((lnp = lback(dotp->lnp)) == curbp->b_hdrlnp)
				return NOTFOUND;
			dotp->lnp = lnp;
			dotp->off = lused(lnp);
			curwp->w_flags |= WFMOVE;
			}
		else
			--dotp->off;
		}
	return rc.status;
	}

// Move the point backward by "n" characters.  If "n" is negative, call "forwChar" to actually do the move.  Set rp to false and
// return NOTFOUND (bypassing rcset()) if move would go out of the buffer; otherwise, return true.
int backChar(Value *rp,int n) {
	int status;
	char *strp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwChar(rp,-n);

	strp = (status = backch(n)) == NOTFOUND ? val_false : val_true;
	return (vsetstr(strp,rp) != 0) ? vrcset() : status;
	}

// Move dot to the beginning of text (first non-whitespace character) on the current line.  Trivial.  No errors.
int begintxt() {
	int c,off,used;
	Dot *dotp;
	Line *lnp;

	used = lused(lnp = (dotp = &curwp->w_face.wf_dot)->lnp);
	for(off = 0; off < used; ++off) {
		c = lgetc(lnp,off);
		if(c != ' ' && c != '\t')
			break;
		}
	dotp->off = off;

	return rc.status;
	}

// Move dot to the [-]nth line and clear the "line move" flag (for subsequent dot move).  Set rp to nil and return NOTFOUND
// (bypassing rcset()) if move would go out of the buffer.
static int goline(Value *rp,int n) {
	int status = SUCCESS;

	if(n > 1)
		status = forwLine(rp,n - 1);
	else if(n < 0 && n != INT_MIN)
		status = backLine(rp,-n);
	kentry.thisflag &= ~CFVMOV;

	return (status == NOTFOUND) ? NOTFOUND : rc.status;
	}

// Move dot to the beginning of text (first non-whitespace character) on the [-]nth line.  Set rp to nil and return NOTFOUND
// (bypassing rcset()) if move would go out of the buffer.
int beginText(Value *rp,int n) {
	int status = goline(rp,n);

	(void) begintxt();
	return (status == NOTFOUND) ? NOTFOUND : rc.status;
	}

// Move dot to beginning ("end" is false) or end ("end" is true) of white space at the current position.  Trivial.  No errors.
int spanwhite(bool end) {
	int c,off,used;
	Dot *dotp;
	Line *lnp;

	used = lused(lnp = (dotp = &curwp->w_face.wf_dot)->lnp);
	off = dotp->off;
	if(end) {
		while(off < used && ((c = lgetc(lnp,off)) == ' ' || c == '\t'))
			++off;
		}
	else {
		int off0 = off;
		while(off >= 0 && ((c = lgetc(lnp,off)) == ' ' || c == '\t'))
			--off;
		if(off < off0)
			++off;
		}
	dotp->off = off;

	return rc.status;
	}

// Move the cursor forward by "n" characters.  ("n" is assumed to be >= 0.)  Return NOTFOUND (bypassing rcset()) if move would
// go out of the buffer.  Set the move flag if dot moves to a different line.
int forwch(int n) {
	Dot *dotp = &curwp->w_face.wf_dot;

	while(n-- > 0) {
		if(dotp->off == lused(dotp->lnp)) {
			if(dotp->lnp == curbp->b_hdrlnp)
				return NOTFOUND;
			dotp->lnp = lforw(dotp->lnp);
			dotp->off = 0;
			curwp->w_flags |= WFMOVE;
			}
		else
			++dotp->off;
		}
	return rc.status;
	}

// Move the point forward by "n" characters.  If "n" is negative, call "backChar" to actually do the move.  Set rp to false and
// return NOTFOUND (bypassing rcset()) if move would go out of the buffer; otherwise, return true.
int forwChar(Value *rp,int n) {
	int status;
	char *strp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backChar(rp,-n);

	strp = (status = forwch(n)) == NOTFOUND ? val_false : val_true;
	return (vsetstr(strp,rp) != 0) ? vrcset() : status;
	}

// Get a required n argument (via prompt if interactive) and validate it.  Return true if valid; otherwise, set an error
// (unless nothing was entered) and return false.
bool getnum(char *prmtp,int *np) {

	// Error if in a script.
	if(opflags & OPSCRIPT)
		(void) rcset(FAILURE,0,text57);
			// "Argument expected"
	else {
		Value *vp;

		if(vnew(&vp,false) != 0)
			(void) vrcset();
		else if(termarg(vp,prmtp,NULL,CTRL | 'M',ARG_FIRST | ARG_NOTNULL) == SUCCESS && !vistfn(vp,VNIL) &&
		 toint(vp) == SUCCESS) {
			*np = vp->u.v_int;
			return true;
			}
		}
	return false;
	}

// Move to a particular line.  Argument n must be zero or greater for this to actually do anything.  Return status.
int gotoLine(Value *rp,int n) {

	// Get an argument if interactive and one doesn't exist.
	if(n == INT_MIN && !getnum(text7,&n))
				// "Go to line"
		return rc.status;

	if(n < 0)
		return rcset(FAILURE,0,text39,text143,n,0);
				// "%s (%d) must be %d or greater","Line number"

	// Go to end of buffer OR beginning of buffer and count lines.
	if(n == 0)
		return feval(rp,INT_MIN,cftab + cf_endBuf);
	curwp->w_face.wf_dot.lnp = lforw(curbp->b_hdrlnp);
	curwp->w_face.wf_dot.off = 0;
	return forwln(n - 1);
	}

// Return best choice for an offset in given line, considering given target column.
static int getgoal(Line *lnp,int targ) {
	int col,off;

	col = off = 0;

	// Find position in lnp which most closely matches goal column, or end of line if lnp is too short.
	while(off < lused(lnp)) {
		if((col = newcol(lgetc(lnp,off),col)) > targ)
			break;
		++off;
		}
	return off;
	}

// Move forward by "n" full lines.  ("n" is assumed to be >= 0.)  The last command controls how the goal column is set.  Return
// NOTFOUND (bypassing rcset()) if move would go out of the buffer.
int forwln(int n) {
	Line *lnp;
	int status = SUCCESS;
	Dot *dotp = &curwp->w_face.wf_dot;


	// If we are on the last line as we start, fail the command.
	if(dotp->lnp == curbp->b_hdrlnp)
		return NOTFOUND;

	// If the last command was not a line move, reset the goal column.
	if(!(kentry.lastflag & CFVMOV))
		targcol = getccol();

	// Flag this command as a line move ...
	kentry.thisflag |= CFVMOV;

	// and move the point down.
	lnp = dotp->lnp;
	while(n-- > 0) {
		if(lnp == curbp->b_hdrlnp) {
			status = NOTFOUND;
			break;
			}
		lnp = lforw(lnp);
		}

	// Reset the current position.
	dotp->lnp = lnp;
	dotp->off = getgoal(lnp,targcol);
	curwp->w_flags |= WFMOVE;

	return (status == NOTFOUND) ? status : rc.status;
	}

// Move forward by "n" full lines.  If the number of lines to move is negative, call the backward line function to actually do
// the move.  Set rp to false and return NOTFOUND (bypassing rcset()) if move would go out of the buffer; otherwise, return
// true.
int forwLine(Value *rp,int n) {
	int status;
	char *strp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backLine(rp,-n);

	strp = (status = forwln(n)) == NOTFOUND ? val_false : val_true;
	return (vsetstr(strp,rp) != 0) ? vrcset() : status;
	}

// This function is like "forwln", but goes backward.  The scheme is exactly the same.
int backln(int n) {
	Line *lnp;
	int status = SUCCESS;
	Dot *dotp = &curwp->w_face.wf_dot;

	// If we are on the first line as we start, fail the command.
	if(lback(dotp->lnp) == curbp->b_hdrlnp)
		return NOTFOUND;

	// If the last command was not a line move, reset the goal column.
	if(!(kentry.lastflag & CFVMOV))
		targcol = getccol();

	// Flag this command as a line move ...
	kentry.thisflag |= CFVMOV;

	// and move the point up.
	lnp = dotp->lnp;
	while(n-- > 0) {
		if(lback(lnp) == curbp->b_hdrlnp) {
			status = NOTFOUND;
			break;
			}
		lnp = lback(lnp);
		}

	// Reset the current position.
	dotp->lnp = lnp;
	dotp->off = getgoal(lnp,targcol);
	curwp->w_flags |= WFMOVE;

	return (status == NOTFOUND) ? status : rc.status;
	}

// This function is like "forwLine", but goes backward.  The scheme is exactly the same.
int backLine(Value *rp,int n) {
	int status;
	char *strp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwLine(rp,-n);

	strp = (status = backln(n)) == NOTFOUND ? val_false : val_true;
	return (vsetstr(strp,rp) != 0) ? vrcset() : status;
	}

// Move the cursor to the beginning ("end" is false) or end ("end" is true) of the [-]nth line.  Set rp to nil and return
// NOTFOUND (bypassing rcset()) if move would go out of the buffer.
int beline(Value *rp,int n,bool end) {
	int status = goline(rp,n);

	curwp->w_face.wf_dot.off = end ? lused(curwp->w_face.wf_dot.lnp) : 0;
	return (status == NOTFOUND) ? NOTFOUND : rc.status;
	}

// Move the cursor in multi-char increments left or right on the current line.
int traverseLine(Value *rp,int n) {
	static int lastWasForw;			// true if last invocation was forward motion.
	int odot,curCol,endCol,newCol;
	int jump = tjump;
	int moveForw = true;
	int maxDisplayCol = term.t_ncol - 2;
	Dot *dotp = &curwp->w_face.wf_dot;

	if(lused(dotp->lnp) > 0) {				// If not blank line.
		odot = dotp->off;

		// Get column positions.
		curCol = getccol();
		dotp->off = lused(dotp->lnp);
		endCol = getccol();

		if(n == 0)					// Zero argument?
			newCol = maxDisplayCol;			// Yes, move to far right of display.
		else if(endCol <= tjump) {			// Line too short?
			dotp->off = odot;			// Yep, exit.
			return rc.status;
			}
		else {
			// Figure out initial direction to move (forward or backward), ignoring any argument (for now).
			if(kentry.lastflag & CFTRAV)		// Last command was a line traversal (this routine) ...
				moveForw = lastWasForw;		// So repeat that direction.
			else if(curCol > (int) (endCol * 0.57))	// If a bit past mid-line ...
				moveForw = false;		// Go backward.

			// Check bounds and reverse direction if needed.
			if(moveForw && curCol > endCol - tjump)
				moveForw = false;
			else if(!moveForw && curCol < tjump)
				moveForw = true;

			// Goose it or reverse if any non-zero argument.
			if(n != INT_MIN) {			// If argument ...
				if((n > 0) == moveForw)		// and same direction as calculated ...
					jump = tjump * 4;	// then boost distance (4x).
				else
					moveForw = !moveForw;	// Otherwise, reverse direction.
				}

			// Move "jump" columns.
			newCol = curCol + (moveForw ? jump : -jump);
			}

		// Move cursor and save results.
		(void) setccol(newCol);
		lastWasForw = moveForw;
		kentry.thisflag |= CFTRAV;
		}
	return rc.status;
	}

// Scroll backward or forward n pages.
static int bfpage(Value *rp,int n) {
	WindFace *wfp = &curwp->w_face;
	int pagesize = curwp->w_nrows - overlap;	// Default scroll.
	if(pagesize <= 0)				// Tiny window or large overlap.
		pagesize = 1;
	n *= pagesize;
	wupd_newtop(curwp,wfp->wf_toplnp,n);
	wfp->wf_dot.lnp = wfp->wf_toplnp;
	wfp->wf_dot.off = 0;
	curwp->w_flags |= WFHARD;

	return rc.status;
	}

// Scroll forward by a specified number of pages (less the current overlap).
int forwPage(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backPage(rp,-n);
	return bfpage(rp,n);
	}

// This command is like "forwPage", but it goes backward.
int backPage(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwPage(rp,-n);
	return bfpage(rp,-n);
	}

// Get row offset of dot in current window and return it.  If dot is not in the window, return zero.
int getwpos(void) {
	int sline;		// Screen line from top of window.
	Line *lnp;
	WindFace *wfp = &curwp->w_face;

	// Search down to the line we want ...
	lnp = wfp->wf_toplnp;
	sline = 1;
	while(lnp != wfp->wf_dot.lnp) {
		if(sline++ == curwp->w_nrows)
			return 0;
		lnp = lforw(lnp);
		}

	// and return the value.
	return sline;
	}

// Set given mark number to dot.
static void setmk(int n) {
	Mark *mkp = curwp->w_face.wf_mark + n;
	mkp->mk_dot = curwp->w_face.wf_dot;
	mkp->mk_force = getwpos();
	}

// Get a mark number.  Return status.
static int getmark(int defn,int *np) {

	// Make sure it is in range.
	if(*np == INT_MIN)
		*np = defn;
	else if(*np < (defn >> 1) || *np >= NMARKS)
		(void) rcset(FAILURE,0,text76,*np,defn >> 1,NMARKS - 1);
			// "Mark (%d) must be between %d and %d"
	return rc.status;
	}

// Set the given mark in the current window to dot.
int setMark(Value *rp,int n) {

	// Make sure mark is in range.
	if(getmark(0,&n) != SUCCESS)
		return rc.status;

	setmk(n);
	return rcset(SUCCESS,0,text9,n);
		// "Mark %d set"
	}

// Remove the mark in the current window.
int clearMark(Value *rp,int n) {

	// Make sure mark is in range.
	if(getmark(0,&n) != SUCCESS)
		return rc.status;

	Mark *mkp = curwp->w_face.wf_mark + n;
	mkp->mk_dot.lnp = NULL;
	mkp->mk_dot.off = mkp->mk_force = 0;
	return rcset(SUCCESS,0,text10,n);
		// "Mark %d cleared"
	}

// Check if given line is in given window and return Boolean result.
bool inwind(EWindow *winp,Line *lnp) {
	Line *lnp1 = winp->w_face.wf_toplnp;
	ushort i = 0;
	do {
		if(lnp1 == lnp)
			return true;

		// If we are at the end of the buffer, bail out.
		if(lnp1 == winp->w_bufp->b_hdrlnp)
			break;

		// On to the next line.
		lnp1 = lforw(lnp1);
		} while(++i < winp->w_nrows);

	return false;
	}

// Goto given mark in current window, but don't force reframe if mark is already in the window.
static void gomark(Mark *mkp) {

	curwp->w_face.wf_dot = mkp->mk_dot;
	if(inwind(curwp,mkp->mk_dot.lnp))
		curwp->w_flags |= WFMOVE;
	else {
		curwp->w_force = mkp->mk_force;
		curwp->w_flags |= WFFORCE;
		}
	}

// Swap the values of dot and a mark in the current window. This is pretty easy, bacause all of the hard work gets done by the
// standard routine that moves the mark about.  The only possible error is "no mark".
int swapMark(Value *rp,int n) {

	// Make sure n is in range.
	if(getmark(0,&n) != SUCCESS)
		return rc.status;

	Mark *mkp = curwp->w_face.wf_mark + n;
	if(mkp->mk_dot.lnp == NULL || mkp->mk_dot.off < 0)
		return rcset(FAILURE,0,text11,n);
			// "No mark %d in this window"

	Dot odot = curwp->w_face.wf_dot;
	short orow = getwpos();
	gomark(mkp);
	mkp->mk_dot = odot;
	mkp->mk_force = orow;
	return rc.status;
	}

// Goto a mark in the current window.  This is pretty easy, bacause all of the hard work gets done by the screen management
// routines.  The only possible error is "no mark".
int gotoMark(Value *rp,int n) {

	// Make sure mark is in range.
	if(getmark(0,&n) != SUCCESS)
		return rc.status;

	Mark *mkp = curwp->w_face.wf_mark + n;
	if(mkp->mk_dot.lnp == NULL || mkp->mk_dot.off < 0)
		return rcset(FAILURE,0,text11,n);
			// "No mark %d in this window"

	gomark(mkp);
	return rc.status;
	}

// Mark current buffer and preserve current position in given mark number, or #2 if no argument.
int markBuf(Value *rp,int n) {

	// Make sure mark is in range.
	if(getmark(2,&n) != SUCCESS)
		return rc.status;

	setmk(n);						// Preserve current position.
	(void) feval(rp,INT_MIN,cftab + cf_beginBuf);		// Move to beginning of buffer.
	setmk(0);						// Set to mark 0.
	(void) feval(rp,INT_MIN,cftab + cf_endBuf);		// Move to end of buffer.
	if(rc.status == SUCCESS)
		vnull(&rc.msg);
	return rcset(SUCCESS,0,text233,n);
		// "Mark %d set to previous position"
	}

// Move the cursor to a matching fence.  If the fence is found, set *regp to its position, number of characters traversed + 1,
// and return -1 (dot moved backward) or 1 (dot moved forward); otherwise, restore dot position and return 0.
int otherfence(Region *regp) {
	Dot odot;		// Original line pointer and offset.
	int sdir;		// Direction of search.
	int flevel;		// Current fence level.
	int ch;			// Fence type to match against.
	int ofence;		// Open fence.
	int c;			// Current character in scan.
	Dot *dotp = &curwp->w_face.wf_dot;

	// Save the original cursor position.
	odot = *dotp;

	// Get the current character.
	if(odot.off == lused(odot.lnp))
		ch = '\r';
	else
		ch = lgetc(odot.lnp,odot.off);

	// Setup proper matching fence.
	switch(ch) {
		case '(':
			ofence = ')';
			sdir = FORWARD;
			break;
		case '{':
			ofence = '}';
			sdir = FORWARD;
			break;
		case '[':
			ofence = ']';
			sdir = FORWARD;
			break;
		case '<':
			ofence = '>';
			sdir = FORWARD;
			break;
		case ')':
			ofence = '(';
			sdir = BACKWARD;
			break;
		case '}':
			ofence = '{';
			sdir = BACKWARD;
			break;
		case ']':
			ofence = '[';
			sdir = BACKWARD;
			break;
		case '>':
			ofence = '<';
			sdir = BACKWARD;
			break;
		default:
			goto nogo;
	}

	// Set up for scan.
	regp->r_size = 0;
	flevel = 1;

	// Scan until we find it, or reach a buffer boundary.
	while(flevel > 0) {
		(void) (sdir == FORWARD ? forwch(1) : backch(1));
		++regp->r_size;

		if(dotp->off == lused(dotp->lnp))
			c = '\r';
		else
			c = lgetc(dotp->lnp,dotp->off);
		if(c == ch)
			++flevel;
		else if(c == ofence)
			--flevel;
		if(boundary(dotp,sdir))
			break;
		}

	// If flevel is zero, we have a match ... move the sucker.
	if(flevel == 0) {
		curwp->w_flags |= WFMOVE;
		++regp->r_size;
		if(sdir == FORWARD) {
			regp->r_dot = odot;
			return 1;
			}
		regp->r_dot = *dotp;
		return -1;
		}

	// Matching fence not found: restore previous position.
	*dotp = odot;
nogo:
	(void) TTbeep();
	return 0;
	}

// Move the cursor backward by "n" words.  All of the details of motion are performed by the "backChar" and "forwChar"
// routines.  No error if attempt to move past the beginning of the buffer.
int backWord(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwWord(rp,-n);

	if(backch(1) != SUCCESS)
		return rc.status;

	do {
		// Back up through the whitespace.
		while(!inword()) {
			if(backch(1) != SUCCESS)
				return rc.status;
			}

		// Back up through the current word.
		while(inword()) {
			if(backch(1) != SUCCESS)
				return rc.status;
			}
		} while(--n > 0);

	// Move to beginning of current word.
	(void) forwch(1);
	return rc.status;
	}

// Move the cursor forward by "n" words.  All of the motion is done by "forwChar".  No error if attempt to move past the end of
// the buffer.
int forwWord(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rp,-n);

	do {
		// Scan through the current word.
		while(inword()) {
			if(forwch(1) != SUCCESS)
				return rc.status;
			}

		// Scan through the whitespace.
		while(!inword()) {
			if(forwch(1) != SUCCESS)
				return rc.status;
			}
		} while(--n > 0);

	return rc.status;
	}

// Move forward to the end of the nth next word.  No error if attempt to move past the end of the buffer.
int endWord(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rp,-n);

	do {
		// Scan through the white space.
		while(!inword()) {
			if(forwch(1) != SUCCESS)
				return rc.status;
			}

		// Scan through the current word.
		while(inword()) {
			if(forwch(1) != SUCCESS)
				return rc.status;
			}
		} while(--n > 0);

	return rc.status;
	}

// Return true if the character at dot is a character that is considered to be part of a word.  The default word character
// list is hard coded.  If $wordChars has been set by the user, use that instead.
bool inword(void) {
	int c;
	Dot *dotp = &curwp->w_face.wf_dot;

	// The end of a line is never in a word.
	if(dotp->off == lused(dotp->lnp))
		return false;

	// Grab the word to check.
	c = lgetc(dotp->lnp,dotp->off);

	// If we are using the table ...
	if(opflags & OPWORDLST)
		return wordlist[c];

	// Otherwise, use the default hard coded check.
	return (isletter(c) || (c >= '0' && c <= '9') || c == '_') ? true : false;
	}

// Move the cursor backward or forward "n" tab stops.  Return -1 if invalid move; otherwise, new offset in current line.
int tabstop(int n) {
	int off,len,col,tabsize,curstop,targstop;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Check for "do nothing" cases.
	if(n == 0 || (len = lused(dotp->lnp)) == 0 || ((off = dotp->off) == 0 && n < 0) || (off == len && n > 0))
		return -1;

	// Calculate target tab stop column.
	tabsize = (stabsize == 0) ? htabsize : stabsize;
	curstop = (col = getccol()) / tabsize;
	if(col % tabsize != 0 && n < 0)
		++curstop;
	return (targstop = curstop + n) <= 0 ? 0 : getgoal(dotp->lnp,targstop * tabsize);
	}

// Move the cursor backward or forward "n" tab stops.
int bftab(int n) {
	int off;

	if((off = tabstop(n)) >= 0)
		curwp->w_face.wf_dot.off = off;
	return rc.status;
	}
