// (c) Copyright 2016 Richard W. Marinelli
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

/*** Local declarations ***/

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
static int goln(Value *rp,int n) {
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
	int status = goln(rp,n);

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
		else if(terminp(vp,prmtp,NULL,RTNKEY,0,ARG_NOTNULL) == SUCCESS && !vistfn(vp,VNIL) &&
		 toint(vp) == SUCCESS) {
			*np = vp->u.v_int;
			return true;
			}
		}
	return false;
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

	// Flag this command as a line move...
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

	// Flag this command as a line move...
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
	int status = goln(rp,n);

	curwp->w_face.wf_dot.off = end ? lused(curwp->w_face.wf_dot.lnp) : 0;
	return (status == NOTFOUND) ? NOTFOUND : rc.status;
	}

// Go to a line via bufop() call.  Return status.
int goline(Value *vp,int n,int line) {

	if(line < 0)
		return rcset(FAILURE,0,text39,text143,line,0);
				// "%s (%d) must be %d or greater","Line number"
	if((opflags & OPSCRIPT) && n != INT_MIN && (!havesym(s_comma,true) || getsym() != SUCCESS))
		return rc.status;

	// Go to line.
	return bufop(vp,n,text229 + 2,BOPGOTOLN,line);
		// ", in"
	}

// Move to a particular line, or end of buffer if line number is zero.  If n argument given, move dot in specified buffer;
// otherwise, current one.  Return status.
int gotoLine(Value *rp,int n) {
	Value *vp;

	// Get line number and validate it.
	if(vnew(&vp,false) != 0)
		return vrcset();
	char wkbuf[strlen(text7) + strlen(text205) + 2];
	sprintf(wkbuf,"%s %s",text7,text205);
			// "Go to","line"
	if(getarg(vp,wkbuf,NULL,RTNKEY,0,ARG_FIRST | ARG_NOTNULL) != SUCCESS ||
	 (!(opflags & OPSCRIPT) && vistfn(vp,VNIL)) || toint(vp) != SUCCESS)
		return rc.status;
	return goline(vp,n,vp->u.v_int);
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
			if(kentry.lastflag & CFTRAV)		// Last command was a line traversal (this routine)...
				moveForw = lastWasForw;		// So repeat that direction.
			else if(curCol > (int) (endCol * 0.57))	// If a bit past mid-line...
				moveForw = false;		// Go backward.

			// Check bounds and reverse direction if needed.
			if(moveForw && curCol > endCol - tjump)
				moveForw = false;
			else if(!moveForw && curCol < tjump)
				moveForw = true;

			// Goose it or reverse if any non-zero argument.
			if(n != INT_MIN) {			// If argument...
				if((n > 0) == moveForw)		// and same direction as calculated...
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

// Get row offset of dot in given window and return it.  If dot is not in the window, return zero.
int getwpos(EWindow *winp) {
	int sline;		// Screen line from top of window.
	Line *lnp;
	WindFace *wfp = &winp->w_face;

	// Search down to the line we want...
	lnp = wfp->wf_toplnp;
	sline = 1;
	while(lnp != wfp->wf_dot.lnp) {
		if(sline++ == winp->w_nrows)
			return 0;
		lnp = lforw(lnp);
		}

	// and return the value.
	return sline;
	}

// Find given mark in current buffer and return in *mkpp.  If found, return it if it's active (visible) or MKOPT_VIZ not set;
// otherwise, if MKOPT_QUERY is set, set *mkpp to NULL and return; otherwise, return an error.  If not found, create the mark
// (and assume that the caller will set the mark's dot) if MKOPT_CREATE set; otherwise, set *mkpp to NULL and return if
// MKOPT_QUERY or MKOPT_WIND set; otherwise,  return an error.  Return status.
int mfind(ushort id,Mark **mkpp,uint flags) {
	Mark *mkp0 = NULL;
	Mark *mkp1 = &curbp->b_mroot;
	do {
		if(mkp1->mk_id == id) {

			// Found it.  Return it if appropriate.
			if(mkp1->mk_dot.off >= 0 || !(flags & MKOPT_VIZ))
				goto found;
			if(flags & MKOPT_QUERY) {
				mkp1 = NULL;
				goto found;
				}
			goto err;
			}
		mkp0 = mkp1;
		} while((mkp1 = mkp1->mk_nextp) != NULL);

	// Not found.  Error if required.
	if(!(flags & MKOPT_CREATE)) {
		if(flags & (MKOPT_QUERY | MKOPT_WIND))
			goto found;
err:
		return rcset(FAILURE,0,text11,id);
				// "No mark '%c' in this buffer"
		}

	// Mark was not required to already exist ... create it.
	if((mkp1 = (Mark *) malloc(sizeof(Mark))) == NULL)
		return rcset(PANIC,0,text94,"mfind");
			// "%s(): Out of memory!"
	mkp1->mk_nextp = NULL;
	mkp1->mk_id = id;
	mkp0->mk_nextp = mkp1;
found:
	*mkpp = mkp1;
	return rc.status;
	}

// Set given mark to dot in given window.
void mset(Mark *mkp,EWindow *winp) {

	mkp->mk_dot = winp->w_face.wf_dot;
	mkp->mk_force = getwpos(winp);
	}

// Get a mark and return it in *mrkpp.  If default n and MKOPT_AUTOR or MKOPT_AUTOW flag set, return mark RMARK or WMARK
// (creating the latter if necessary); otherwise, if n < 0 and MKOPT_AUTOR or MKOPT_AUTOW flag set, return mark WMARK (creating
// it if necessary and if MKOPT_CREATE flag set); otherwise, get a key with no default (and set *mkpp to NULL if nothing was
// entered interactively).  Return status.
static int getmark(char *prmptp,int n,uint flags,Mark **mkpp) {

	// Check n.
	if(n < 0 && (flags & (MKOPT_AUTOR | MKOPT_AUTOW)))
		return mfind((flags & MKOPT_AUTOW) || (n < 0 && n != INT_MIN) ? WMARK : RMARK,mkpp,flags);

	// Get a key.
	Value *vp;
	if(vnew(&vp,false) != 0)
		return vrcset();
	if(opflags & OPSCRIPT) {
		if(funcarg(vp,ARG_FIRST | ARG_NOTNULL | ARG_STR | ARG_PRINT) != SUCCESS)
			return rc.status;
		}
	else {
		char pbuf[term.t_ncol];

		if(flags & (MKOPT_VIZ | MKOPT_EXIST)) {
			Mark *mkp;
			ushort delim;
			StrList prmpt;

			// Build prompt with existing marks in parentheses.
			if(vopen(&prmpt,NULL,false) != 0 || vputf(&prmpt,text346,prmptp) != 0 || vputc(' ',&prmpt) != 0)
									// "%s mark"
				return vrcset();
			delim = '(';
			mkp = &curbp->b_mroot;
			do {
				if(mkp->mk_id <= '~' && (mkp->mk_dot.off >= 0 || (flags & MKOPT_EXIST)) &&
				 (mkp->mk_id != ' ' || !(flags & MKOPT_EXIST))) {
					if(vputc(delim,&prmpt) != 0)
						return vrcset();
					if(mkp->mk_id == RMARK) {
						if(vputc('\'',&prmpt) != 0 || vputc(RMARK,&prmpt) != 0 ||
						 vputc('\'',&prmpt) != 0)
							return vrcset();
						}
					else if(vputc(mkp->mk_id,&prmpt) != 0)
						return vrcset();
					delim = ' ';
					}
				} while((mkp = mkp->mk_nextp) != NULL);

			// Error if deleteMark call and no mark other than ' ' found (which can't be deleted).
			if(delim == '(')
				return rcset(FAILURE,0,text361);
					// "No mark found to delete"

			if(vputc(')',&prmpt) != 0 || vclose(&prmpt) != 0)
				return vrcset();

			// Fit the prompt in roughly 90% of the terminal width.
			strfit(pbuf,(size_t) term.t_ncol * 90 / 100,prmpt.sl_vp->v_strp,0);
			}
		else {
			// Build basic prompt.
			sprintf(pbuf,text346,prmptp);
				// "%s mark"
			}
		if(terminp(vp,pbuf,NULL,RTNKEY,0,ARG_NOTNULL | ARG_PRINT | TERM_ONEKEY) != SUCCESS)
			return rc.status;
		if(vistfn(vp,VNIL)) {
			*mkpp = NULL;
			return rc.status;
			}
		}

	// Success.  Return mark.
	return mfind(*vp->v_strp,mkpp,flags);
	}

// Set a mark in the current buffer to dot.  If default n, use RMARK; otherwise, if n < 0, use mark WMARK; otherwise, get a mark
// with no default.  Return status.
int setMark(Value *rp,int n) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text64,n,MKOPT_AUTOR | MKOPT_CREATE,&mkp) != SUCCESS || mkp == NULL)
			// "Set"
		return rc.status;

	// Set mark to dot and return.
	mset(mkp,curwp);
	return rcset(SUCCESS,0,text9,mkp->mk_id,text350);
		// "Mark '%c' %s","set"
	}

// Delete given mark.  Return status.
static int delmark(Mark *mkp) {

	if(mkp->mk_id == RMARK)
		return rcset(FAILURE,0,text352,RMARK);
			// "Cannot delete mark '%c'"

	// It's a go ... delete it.
	Mark *mkp0 = &curbp->b_mroot;
	ushort id = mkp->mk_id;
	for(;;) {
		if(mkp0->mk_nextp == mkp)
			break;
		mkp0 = mkp0->mk_nextp;
		}
	mkp0->mk_nextp = mkp->mk_nextp;
	(void) free((void *) mkp);
	return rcset(SUCCESS,0,text9,id,text10);
		// "Mark '%s' %s","deleted"
	}

// Remove a mark in the current buffer.  If non-default n, remove all marks.
int deleteMark(Value *rp,int n) {

	// Delete all?
	if(n != INT_MIN) {
		mdelete(curbp,0);
		(void) rcset(SUCCESS,0,text351);
			// "All marks deleted"
		}
	else {
		Mark *mkp;

		// Make sure mark is valid.
		if(getmark(text26,n,MKOPT_HARD | MKOPT_EXIST,&mkp) != SUCCESS || mkp == NULL)
				// "Delete"
			return rc.status;
		(void) delmark(mkp);
		}

	return rc.status;
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

// Swap a mark with dot, given mark pointer.  Return status.
static int swapmkp(Mark *mkp) {
	Dot odot = curwp->w_face.wf_dot;
	short orow = getwpos(curwp);
	gomark(mkp);
	mkp->mk_dot = odot;
	mkp->mk_force = orow;
	return rc.status;
	}

// Swap the values of dot and a mark in the current window.  If default n, use RMARK; otherwise, if n < 0, use mark WMARK;
// otherwise, get a mark with no default.  Return status.
int swapMark(Value *rp,int n) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text347,n,MKOPT_AUTOR | MKOPT_VIZ,&mkp) != SUCCESS || mkp == NULL)
			// "Swap dot with"
		return rc.status;

	// Swap dot and the mark.
	return swapmkp(mkp);
	}

// Swap a mark with dot, given mark id.  Return status.
int swapmid(ushort id) {
	Mark *mkp;

	if(mfind(id,&mkp,MKOPT_VIZ) == SUCCESS)
		(void) swapmkp(mkp);
	return rc.status;
	}

// Go to a mark in the current window.  Get a mark with no default, move dot, then delete mark if non-default n.  Return status.
int gotoMark(Value *rp,int n) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text7,n,MKOPT_HARD | MKOPT_VIZ,&mkp) != SUCCESS || mkp == NULL)
			// "Go to"
		return rc.status;

	// Set dot to the mark.
	gomark(mkp);

	// Delete mark if applicable.
	if(n != INT_MIN)
		(void) delmark(mkp);

	return rc.status;
	}

// Mark current buffer from beginning to end and preserve current position in a mark.  If default n or n < 0, use WMARK;
// otherwise, get a mark with no default.  Return status.
int markBuf(Value *rp,int n) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text348,n,MKOPT_AUTOW | MKOPT_CREATE,&mkp) != SUCCESS || mkp == NULL)
			// "Save dot in"
		return rc.status;

	// Mark whole buffer.  If RMARK was specified for saving dot, user is out of luck (it will be overwritten).
	mset(mkp,curwp);					// Preserve current position.
	(void) feval(rp,INT_MIN,cftab + cf_beginBuf);		// Move to beginning of buffer.
	mset(&curbp->b_mroot,curwp);				// Set to mark RMARK.
	(void) feval(rp,INT_MIN,cftab + cf_endBuf);		// Move to end of buffer.
	if(rc.status == SUCCESS)
		rcclear();
	return (mkp->mk_id == RMARK) ? rc.status : rcset(SUCCESS,0,text233,mkp->mk_id);
						// "Mark '%c' set to previous position"
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
		ch = '\n';
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
			c = '\n';
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

// Move the cursor backward by "n" words.  All of the details of motion are performed by the "backChar" and "forwChar" routines.
// Set rp to false and return NOTFOUND (bypassing rcset()) if move would go out of the buffer; otherwise, return true.
int backWord(Value *rp,int n) {
	int status;
	char *strp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwWord(rp,-n);

	if((status = backch(1)) == NOTFOUND)
		goto fail;

	do {
		// Back up through the whitespace.
		while(!inword()) {
			if((status = backch(1)) == NOTFOUND)
				goto fail;
			}

		// Back up through the current word.
		while(inword()) {
			if((status = backch(1)) == NOTFOUND)
				goto retn;	// Hit word at beginning of buffer.
			}
		} while(--n > 0);

	// Move to beginning of current word.
	if((status = forwch(1)) == NOTFOUND)
fail:
		strp = val_false;
	else
retn:
		strp = val_true;
	return (vsetstr(strp,rp) != 0) ? vrcset() : status;
	}

// Move the cursor forward by "n" words.  All of the motion is done by "forwChar".  Set rp to false and return NOTFOUND
// (bypassing rcset()) if move would go out of the buffer; otherwise, return true.
int forwWord(Value *rp,int n) {
	int status;
	char *strp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rp,-n);

	do {
		// Scan through the current word.
		while(inword()) {
			if((status = forwch(1)) == NOTFOUND)
				goto fail;
			}

		// Scan through the whitespace.
		while(!inword()) {
			if((status = forwch(1)) == NOTFOUND)
				goto fail;
			}
		} while(--n > 0);
	strp = val_true;
	goto retn;
fail:
	strp = val_false;
retn:
	return (vsetstr(strp,rp) != 0) ? vrcset() : rc.status;
	}

// Move forward to the end of the nth next word.  Set rp to false and return NOTFOUND (bypassing rcset()) if move would go out
// of the buffer; otherwise, return true.
int endWord(Value *rp,int n) {
	int status;
	char *strp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rp,-n);

	do {
		// Scan through the white space.
		while(!inword()) {
			if((status = forwch(1)) == NOTFOUND)
				goto fail;
			}

		// Scan through the current word.
		while(inword()) {
			if((status = forwch(1)) == NOTFOUND)
				goto fail;
			}
		} while(--n > 0);
	strp = val_true;
	goto retn;
fail:
	strp = val_false;
retn:
	return (vsetstr(strp,rp) != 0) ? vrcset() : rc.status;
	}

// Return true if the character at dot is a character that is considered to be part of a word.
bool inword(void) {
	Dot *dotp = &curwp->w_face.wf_dot;
	return wordlist[(int) (dotp->off == lused(dotp->lnp) ? 015 : lgetc(dotp->lnp,dotp->off))];
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

// Build and pop up a buffer containing all marks which exist in the current buffer.  Render buffer and return status.
int showMarks(Value *rp,int n) {
	Buffer *bufp;
	Line *lnp;
	Mark *mkp;
	StrList rpt;
	int n1;
	int max = term.t_ncol * 2;
	char wkbuf[32];

	// Get buffer for the mark list.
	if(sysbuf(text353,&bufp) != SUCCESS)
			// "MarkList"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	// Construct the header lines.
	if(vputs(text354,&rpt) != 0 || vputc('\n',&rpt) != 0 || vputs("----  ------  ",&rpt) != 0)
			// "Mark  Offset  Line text"
		return vrcset();
	n1 = term.t_ncol - 14;
	do {
		if(vputc('-',&rpt) != 0)
			return vrcset();
		} while(--n1 > 0);

	// Loop through lines in current buffer, searching for mark matches.
	lnp = lforw(curbp->b_hdrlnp);
	for(;;) {
		n1 = 1;
		mkp = &curbp->b_mroot;
		do {
			if(mkp->mk_id <= '~' && mkp->mk_dot.lnp == lnp) {
				mkp->mk_id == ' ' ? (void) strcpy(wkbuf,"\n' '") : (void) sprintf(wkbuf,"\n %c ",mkp->mk_id);
				sprintf(strchr(wkbuf,'\0')," %8d",mkp->mk_dot.off);
				if(vputs(wkbuf,&rpt) != 0)
					return vrcset();
				if(n1 && lused(lnp) > 0 && (vputs("  ",&rpt) != 0 || vstrlit(&rpt,lnp == curbp->b_hdrlnp ?
				 "(EOB)" : ltext(lnp),lused(lnp) > max ? max : lused(lnp)) != 0))
					return vrcset();
				n1 = 0;
				}
			} while((mkp = mkp->mk_nextp) != NULL);
		if(lnp == curbp->b_hdrlnp)
			break;
		lnp = lforw(lnp);
		}

	// Add the report to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(bappend(bufp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,bufp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}
