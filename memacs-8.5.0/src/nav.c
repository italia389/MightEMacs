// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// nav.c	Basic movement (navigation) functions for MightEMacs.
//
// These routines move the point around on the screen.  They compute a new value for the point, then adjust dot.  The display
// code always updates the point location, so only moves between lines or functions that adjust the top line in the window and
// invalidate the framing are hard.

#include "os.h"
#include "std.h"
#include "lang.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "main.h"
#include "search.h"

/*** Local declarations ***/

static int targcol;			// Goal column for vertical line movements.

// Move the point backward by "n" characters.  ("n" is assumed to be >= 0.)  Return NotFound (bypassing rcset()) if move would
// go out of the buffer.  Set the move flag if dot moves to a different line.
int backch(int n) {
	Line *lnp;
	Dot *dotp = &curwp->w_face.wf_dot;

	while(n-- > 0) {
		if(dotp->off == 0) {
			if((lnp = lback(dotp->lnp)) == curbp->b_hdrlnp)
				return NotFound;
			dotp->lnp = lnp;
			dotp->off = lused(lnp);
			curwp->w_flags |= WFMove;
			}
		else
			--dotp->off;
		}
	return rc.status;
	}

// Move the point backward by "n" characters.  If "n" is negative, call "forwChar" to actually do the move.  Set rp to false and
// return NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, return true.
int backChar(Datum *rp,int n,Datum **argpp) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwChar(rp,-n,NULL);

	dsetbool((status = backch(n)) != NotFound,rp);
	return status;
	}

// Move dot to the beginning of text (first non-whitespace character) on the current line.  Trivial.  No errors.
int begintxt(void) {
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

// Move dot to the [-]nth line and clear the "line move" flag (for subsequent dot move).  Set rp to nil and return NotFound
// (bypassing rcset()) if move would go out of the buffer.
static int goln(Datum *rp,int n) {
	int status = Success;

	if(n > 1)
		status = forwLine(rp,n - 1,NULL);
	else if(n < 0 && n != INT_MIN)
		status = backLine(rp,-n,NULL);
	kentry.thisflag &= ~CFVMove;

	return (status == NotFound) ? NotFound : rc.status;
	}

// Move dot to the beginning of text (first non-whitespace character) on the [-]nth line.  Set rp to nil and return NotFound
// (bypassing rcset()) if move would go out of the buffer.
int beginText(Datum *rp,int n,Datum **argpp) {
	int status = goln(rp,n);

	(void) begintxt();
	return (status == NotFound) ? NotFound : rc.status;
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

// Move the point forward by "n" characters.  ("n" is assumed to be >= 0.)  Return NotFound (bypassing rcset()) if move would
// go out of the buffer.  Set the move flag if dot moves to a different line.
int forwch(int n) {
	Dot *dotp = &curwp->w_face.wf_dot;

	while(n-- > 0) {
		if(dotp->off == lused(dotp->lnp)) {
			if(dotp->lnp == curbp->b_hdrlnp)
				return NotFound;
			dotp->lnp = lforw(dotp->lnp);
			dotp->off = 0;
			curwp->w_flags |= WFMove;
			}
		else
			++dotp->off;
		}
	return rc.status;
	}

// Move the point forward by "n" characters.  If "n" is negative, call "backChar" to actually do the move.  Set rp to false and
// return NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, return true.
int forwChar(Datum *rp,int n,Datum **argpp) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backChar(rp,-n,NULL);

	dsetbool((status = forwch(n)) != NotFound,rp);
	return status;
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
// NotFound (bypassing rcset()) if move would go out of the buffer.
int forwln(int n) {
	Line *lnp;
	int status = Success;
	Dot *dotp = &curwp->w_face.wf_dot;

	// If we are on the last line as we start, fail the command.
	if(dotp->lnp == curbp->b_hdrlnp)
		return NotFound;

	// If the last command was not a line move, reset the goal column.
	if(!(kentry.lastflag & CFVMove))
		targcol = getccol();

	// Flag this command as a line move...
	kentry.thisflag |= CFVMove;

	// and move the point down.
	lnp = dotp->lnp;
	while(n-- > 0) {
		if(lnp == curbp->b_hdrlnp) {
			status = NotFound;
			break;
			}
		lnp = lforw(lnp);
		}

	// Reset the current position.
	dotp->lnp = lnp;
	dotp->off = getgoal(lnp,targcol);
	curwp->w_flags |= WFMove;

	return (status == NotFound) ? status : rc.status;
	}

// Move forward by "n" full lines.  If the number of lines to move is negative, call the backward line function to actually do
// the move.  Set rp to false and return NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, return
// true.
int forwLine(Datum *rp,int n,Datum **argpp) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backLine(rp,-n,NULL);

	dsetbool((status = forwln(n)) != NotFound,rp);
	return status;
	}

// This function is like "forwln", but goes backward.  The scheme is exactly the same.
int backln(int n) {
	Line *lnp;
	int status = Success;
	Dot *dotp = &curwp->w_face.wf_dot;

	// If we are on the first line as we start, fail the command.
	if(lback(dotp->lnp) == curbp->b_hdrlnp)
		return NotFound;

	// If the last command was not a line move, reset the goal column.
	if(!(kentry.lastflag & CFVMove))
		targcol = getccol();

	// Flag this command as a line move...
	kentry.thisflag |= CFVMove;

	// and move the point up.
	lnp = dotp->lnp;
	while(n-- > 0) {
		if(lback(lnp) == curbp->b_hdrlnp) {
			status = NotFound;
			break;
			}
		lnp = lback(lnp);
		}

	// Reset the current position.
	dotp->lnp = lnp;
	dotp->off = getgoal(lnp,targcol);
	curwp->w_flags |= WFMove;

	return (status == NotFound) ? status : rc.status;
	}

// This function is like "forwLine", but goes backward.  The scheme is exactly the same.
int backLine(Datum *rp,int n,Datum **argpp) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwLine(rp,-n,NULL);

	dsetbool((status = backln(n)) != NotFound,rp);
	return status;
	}

// Move the point to the beginning ("end" is false) or end ("end" is true) of the [-]nth line.  Set rp to nil and return
// NotFound (bypassing rcset()) if move would go out of the buffer.
int beline(Datum *rp,int n,bool end) {
	int status = goln(rp,n);

	curwp->w_face.wf_dot.off = end ? lused(curwp->w_face.wf_dot.lnp) : 0;
	return (status == NotFound) ? NotFound : rc.status;
	}

// Go to a line via bufop() call.  Return status.
int goline(Datum *datp,int n,int line) {

	if(line < 0)
		return rcset(Failure,0,text39,text143,line,0);
				// "%s (%d) must be %d or greater","Line number"
	if((opflags & OpScript) && n != INT_MIN && (!havesym(s_comma,true) || getsym() != Success))
		return rc.status;

	// Go to line.
	return bufop(datp,n,text229 + 2,BOpGotoLn,line);
		// ", in"
	}

// Move to a particular line, or end of buffer if line number is zero.  If n argument given, move dot in specified buffer;
// otherwise, current one.  Return status.
int gotoLine(Datum *rp,int n,Datum **argpp) {
	Datum *datp;

	// Get line number and validate it.
	if(dnewtrk(&datp) != 0)
		return drcset();
	char wkbuf[strlen(text7) + strlen(text205) + 2];
	sprintf(wkbuf,"%s %s",text7,text205);
			// "Go to","line"
	if(getarg(datp,wkbuf,NULL,RtnKey,0,Arg_First | CFInt1,0) != Success || datp->d_type == dat_nil ||
	 toint(datp) != Success)
		return rc.status;
	return goline(datp,n,datp->u.d_int);
	}

// Move the point in multi-char increments left or right on the current line.
int traverseLine(Datum *rp,int n,Datum **argpp) {
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
			if(kentry.lastflag & CFTrav)		// Last command was a line traversal (this routine)...
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

		// Move point and save results.
		(void) setccol(newCol);
		lastWasForw = moveForw;
		kentry.thisflag |= CFTrav;
		}
	return rc.status;
	}

// Scroll backward or forward n pages.
static int bfpage(Datum *rp,int n) {
	WindFace *wfp = &curwp->w_face;
	int pagesize = curwp->w_nrows - overlap;	// Default scroll.
	if(pagesize <= 0)				// Tiny window or large overlap.
		pagesize = 1;
	n *= pagesize;
	wupd_newtop(curwp,wfp->wf_toplnp,n);
	wfp->wf_dot.lnp = wfp->wf_toplnp;
	wfp->wf_dot.off = 0;
	curwp->w_flags |= WFHard;

	return rc.status;
	}

// Scroll forward by a specified number of pages (less the current overlap).
int forwPage(Datum *rp,int n,Datum **argpp) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backPage(rp,-n,NULL);
	return bfpage(rp,n);
	}

// This command is like "forwPage", but it goes backward.
int backPage(Datum *rp,int n,Datum **argpp) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwPage(rp,-n,NULL);
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

// Find given mark in current buffer and return in *mkpp.  If found, return it if it's active (visible) or MkOpt_Viz not set;
// otherwise, if MkOpt_Query is set, set *mkpp to NULL and return; otherwise, return an error.  If not found, create the mark
// (and assume that the caller will set the mark's dot) if MkOpt_Create set; otherwise, set *mkpp to NULL and return if
// MkOpt_Query or MkOpt_Wind set; otherwise,  return an error.  Return status.
int mfind(ushort id,Mark **mkpp,ushort flags) {
	Mark *mkp0 = NULL;
	Mark *mkp1 = &curbp->b_mroot;
	do {
		if(mkp1->mk_id == id) {

			// Found it.  Return it if appropriate.
			if(mkp1->mk_dot.off >= 0 || !(flags & MkOpt_Viz))
				goto Found;
			if(flags & MkOpt_Query) {
				mkp1 = NULL;
				goto Found;
				}
			goto ErrRetn;
			}
		mkp0 = mkp1;
		} while((mkp1 = mkp1->mk_nextp) != NULL);

	// Not found.  Error if required.
	if(!(flags & MkOpt_Create)) {
		if(flags & (MkOpt_Query | MkOpt_Wind))
			goto Found;
ErrRetn:
		return rcset(Failure,0,text11,id);
				// "No mark '%c' in this buffer"
		}

	// Mark was not required to already exist... create it.
	if((mkp1 = (Mark *) malloc(sizeof(Mark))) == NULL)
		return rcset(Panic,0,text94,"mfind");
			// "%s(): Out of memory!"
	mkp1->mk_nextp = NULL;
	mkp1->mk_id = id;
	mkp0->mk_nextp = mkp1;
Found:
	*mkpp = mkp1;
	return rc.status;
	}

// Set given mark to dot in given window.
void mset(Mark *mkp,EWindow *winp) {

	mkp->mk_dot = winp->w_face.wf_dot;
	mkp->mk_force = getwpos(winp);
	}

// Get a mark and return it in *mrkpp.  If default n and MkOpt_AutoR or MkOpt_AutoW flag set, return mark RMark or WMark
// (creating the latter if necessary); otherwise, if n < 0 and MkOpt_AutoR or MkOpt_AutoW flag set, return mark WMark (creating
// it if necessary and if MkOpt_Create flag set); otherwise, get a key with no default (and set *mkpp to NULL if nothing was
// entered interactively).  Return status.
static int getmark(char *prmt,int n,ushort flags,Mark **mkpp) {

	// Check n.
	if(n < 0 && (flags & (MkOpt_AutoR | MkOpt_AutoW)))
		return mfind((flags & MkOpt_AutoW) || (n < 0 && n != INT_MIN) ? WMark : RMark,mkpp,flags);

	// Get a key.
	Datum *datp;
	if(dnewtrk(&datp) != 0)
		return drcset();
	if(opflags & OpScript) {
		if(funcarg(datp,Arg_First) != Success)
			return rc.status;
		if(strlen(datp->d_str) != 1 || *datp->d_str < ' ' || *datp->d_str > '~')
			return rcset(Failure,0,"%s '%s'%s",text285,datp->d_str,text345);
					//  "Call argument"," must be a printable character"
		}
	else {
		char pbuf[term.t_ncol];

		if(flags & (MkOpt_Viz | MkOpt_Exist)) {
			Mark *mkp;
			ushort delim;
			DStrFab prompt;

			// Build prompt with existing marks in parentheses.
			if(dopentrk(&prompt) != 0 || dputf(&prompt,text346,prmt) != 0 || dputc(' ',&prompt) != 0)
									// "%s mark"
				return drcset();
			delim = '(';
			mkp = &curbp->b_mroot;
			do {
				if(mkp->mk_id <= '~' && (mkp->mk_dot.off >= 0 || (flags & MkOpt_Exist)) &&
				 (mkp->mk_id != ' ' || !(flags & MkOpt_Exist))) {
					if(dputc(delim,&prompt) != 0)
						return drcset();
					if(mkp->mk_id == RMark) {
						if(dputc('\'',&prompt) != 0 || dputc(RMark,&prompt) != 0 ||
						 dputc('\'',&prompt) != 0)
							return drcset();
						}
					else if(dputc(mkp->mk_id,&prompt) != 0)
						return drcset();
					delim = ' ';
					}
				} while((mkp = mkp->mk_nextp) != NULL);

			// Error if deleteMark call and no mark other than ' ' found (which can't be deleted).
			if(delim == '(')
				return rcset(Failure,0,text361);
					// "No mark found to delete"

			if(dputc(')',&prompt) != 0 || dclose(&prompt,sf_string) != 0)
				return drcset();

			// Fit the prompt in roughly 90% of the terminal width.
			strfit(pbuf,(size_t) term.t_ncol * 90 / 100,prompt.sf_datp->d_str,0);
			}
		else {
			// Build basic prompt.
			sprintf(pbuf,text346,prmt);
				// "%s mark"
			}
		if(terminp(datp,pbuf,NULL,RtnKey,0,CFNotNull1,Term_OneKey) != Success)
			return rc.status;
		if(datp->d_type == dat_nil) {
			*mkpp = NULL;
			return rc.status;
			}
		if(*datp->d_str < ' ' || *datp->d_str > '~') {
			char wkbuf[16];
			return rcset(Failure,0,"%s%s%s",text349,ektos(*datp->d_str,wkbuf),text345);
					//  "Mark value '","' must be a printable character"
			}
		}

	// Success.  Return mark.
	return mfind(*datp->d_str,mkpp,flags);
	}

// Set a mark in the current buffer to dot.  If default n, use RMark; otherwise, if n < 0, use mark WMark; otherwise, get a mark
// with no default.  Return status.
int setMark(Datum *rp,int n,Datum **argpp) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text64,n,MkOpt_AutoR | MkOpt_Create,&mkp) != Success || mkp == NULL)
			// "Set"
		return rc.status;

	// Set mark to dot and return.
	mset(mkp,curwp);
	return rcset(Success,0,text9,mkp->mk_id,text350);
		// "Mark '%c' %s","set"
	}

// Delete given mark.  Return status.
static int delmark(Mark *mkp) {

	if(mkp->mk_id == RMark)
		return rcset(Failure,0,text352,RMark);
			// "Cannot delete mark '%c'"

	// It's a go... delete it.
	Mark *mkp0 = &curbp->b_mroot;
	ushort id = mkp->mk_id;
	for(;;) {
		if(mkp0->mk_nextp == mkp)
			break;
		mkp0 = mkp0->mk_nextp;
		}
	mkp0->mk_nextp = mkp->mk_nextp;
	(void) free((void *) mkp);
	return rcset(Success,0,text9,id,text10);
		// "Mark '%s' %s","deleted"
	}

// Remove a mark in the current buffer.  If non-default n, remove all marks.
int deleteMark(Datum *rp,int n,Datum **argpp) {

	// Delete all?
	if(n != INT_MIN) {
		mdelete(curbp,0);
		(void) rcset(Success,0,text351);
			// "All marks deleted"
		}
	else {
		Mark *mkp;

		// Make sure mark is valid.
		if(getmark(text26,n,MkOpt_Hard | MkOpt_Exist,&mkp) != Success || mkp == NULL)
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
		curwp->w_flags |= WFMove;
	else {
		curwp->w_force = mkp->mk_force;
		curwp->w_flags |= WFForce;
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

// Swap the values of dot and a mark in the current window.  If default n, use RMark; otherwise, if n < 0, use mark WMark;
// otherwise, get a mark with no default.  Return status.
int swapMark(Datum *rp,int n,Datum **argpp) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text347,n,MkOpt_AutoR | MkOpt_Viz,&mkp) != Success || mkp == NULL)
			// "Swap dot with"
		return rc.status;

	// Swap dot and the mark.
	return swapmkp(mkp);
	}

// Swap a mark with dot, given mark id.  Return status.
int swapmid(ushort id) {
	Mark *mkp;

	if(mfind(id,&mkp,MkOpt_Viz) == Success)
		(void) swapmkp(mkp);
	return rc.status;
	}

// Go to a mark in the current window.  Get a mark with no default, move dot, then delete mark if non-default n.  Return status.
int gotoMark(Datum *rp,int n,Datum **argpp) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text7,n,MkOpt_Hard | MkOpt_Viz,&mkp) != Success || mkp == NULL)
			// "Go to"
		return rc.status;

	// Set dot to the mark.
	gomark(mkp);

	// Delete mark if applicable.
	if(n != INT_MIN)
		(void) delmark(mkp);

	return rc.status;
	}

// Mark current buffer from beginning to end and preserve current position in a mark.  If default n or n < 0, use WMark;
// otherwise, get a mark with no default.  Return status.
int markBuf(Datum *rp,int n,Datum **argpp) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text348,n,MkOpt_AutoW | MkOpt_Create,&mkp) != Success || mkp == NULL)
			// "Save dot in"
		return rc.status;

	// Mark whole buffer.  If RMark was specified for saving dot, user is out of luck (it will be overwritten).
	mset(mkp,curwp);					// Preserve current position.
	(void) execCF(rp,INT_MIN,cftab + cf_beginBuf,0,0);	// Move to beginning of buffer.
	mset(&curbp->b_mroot,curwp);				// Set to mark RMark.
	(void) execCF(rp,INT_MIN,cftab + cf_endBuf,0,0);	// Move to end of buffer.
	if(rc.status == Success)
		rcclear();
	return (mkp->mk_id == RMark) ? rc.status : rcset(Success,0,text233,mkp->mk_id);
						// "Mark '%c' set to previous position"
	}

// Move the point to a matching fence.  If the fence is found, set *regp to its position, number of characters traversed + 1,
// and return -1 (dot moved backward) or 1 (dot moved forward); otherwise, restore dot position and return 0.
int otherfence(Region *regp) {
	Dot odot;		// Original line pointer and offset.
	int sdir;		// Direction of search.
	int flevel;		// Current fence level.
	int ch;			// Fence type to match against.
	int ofence;		// Open fence.
	int c;			// Current character in scan.
	Dot *dotp = &curwp->w_face.wf_dot;

	// Save the original point position.
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
			sdir = Forward;
			break;
		case '{':
			ofence = '}';
			sdir = Forward;
			break;
		case '[':
			ofence = ']';
			sdir = Forward;
			break;
		case '<':
			ofence = '>';
			sdir = Forward;
			break;
		case ')':
			ofence = '(';
			sdir = Backward;
			break;
		case '}':
			ofence = '{';
			sdir = Backward;
			break;
		case ']':
			ofence = '[';
			sdir = Backward;
			break;
		case '>':
			ofence = '<';
			sdir = Backward;
			break;
		default:
			goto NoGo;
	}

	// Set up for scan.
	regp->r_size = 0;
	flevel = 1;

	// Scan until we find it, or reach a buffer boundary.
	while(flevel > 0) {
		(void) (sdir == Forward ? forwch(1) : backch(1));
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

	// If flevel is zero, we have a match... move the sucker.
	if(flevel == 0) {
		curwp->w_flags |= WFMove;
		++regp->r_size;
		if(sdir == Forward) {
			regp->r_dot = odot;
			return 1;
			}
		regp->r_dot = *dotp;
		return -1;
		}

	// Matching fence not found: restore previous position.
	*dotp = odot;
NoGo:
	(void) TTbeep();
	return 0;
	}

// Move the point backward by "n" words.  All of the details of motion are performed by the "backChar" and "forwChar" routines.
// Set rp to false if move would go out of the buffer; otherwise, true.  Return status.
int backWord(Datum *rp,int n,Datum **argpp) {
	bool b;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwWord(rp,-n,NULL);

	if(backch(1) == NotFound)
		goto Fail;

	do {
		// Back up through the whitespace.
		while(!inword()) {
			if(backch(1) == NotFound)
				goto Fail;
			}

		// Back up through the current word.
		while(inword()) {
			if(backch(1) == NotFound)
				goto Retn;	// Hit word at beginning of buffer.
			}
		} while(--n > 0);

	// Move to beginning of current word.
	if(forwch(1) == NotFound)
Fail:
		b = false;
	else
Retn:
		b = true;
	dsetbool(b,rp);
	return rc.status;
	}

// Move the point forward by "n" words.  All of the motion is done by "forwChar".  Set rp to false if move would go out of the
// buffer; otherwise, true.  Return status.
int forwWord(Datum *rp,int n,Datum **argpp) {
	bool b;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rp,-n,NULL);

	do {
		// Scan through the current word.
		while(inword()) {
			if(forwch(1) == NotFound)
				goto Fail;
			}

		// Scan through the whitespace.
		while(!inword()) {
			if(forwch(1) == NotFound)
				goto Fail;
			}
		} while(--n > 0);
	b = true;
	goto Retn;
Fail:
	b = false;
Retn:
	dsetbool(b,rp);
	return rc.status;
	}

// Move forward to the end of the nth next word.  Set rp to false if move would go out of the buffer; otherwise, true.  Return
// status.
int endWord(Datum *rp,int n,Datum **argpp) {
	bool b;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rp,-n,NULL);

	do {
		// Scan through the white space.
		while(!inword()) {
			if(forwch(1) == NotFound)
				goto Fail;
			}

		// Scan through the current word.
		while(inword()) {
			if(forwch(1) == NotFound)
				goto Fail;
			}
		} while(--n > 0);
	b = true;
	goto Retn;
Fail:
	b = false;
Retn:
	dsetbool(b,rp);
	return rc.status;
	}

// Return true if the character at dot is a character that is considered to be part of a word.
bool inword(void) {
	Dot *dotp = &curwp->w_face.wf_dot;
	return wordlist[(int) (dotp->off == lused(dotp->lnp) ? 015 : lgetc(dotp->lnp,dotp->off))];
	}

// Move the point backward or forward "n" tab stops.  Return -1 if invalid move; otherwise, new offset in current line.
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

// Move the point backward or forward "n" tab stops.
int bftab(int n) {
	int off;

	if((off = tabstop(n)) >= 0)
		curwp->w_face.wf_dot.off = off;
	return rc.status;
	}

// Build and pop up a buffer containing all marks which exist in the current buffer.  Render buffer and return status.
int showMarks(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	Line *lnp;
	Mark *mkp;
	DStrFab rpt;
	int n1;
	int max = term.t_ncol * 2;
	char wkbuf[32];

	// Get buffer for the mark list.
	if(sysbuf(text353,&bufp) != Success)
			// "MarkList"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	// Construct the header lines.
	if(dputs(text354,&rpt) != 0 || dputc('\n',&rpt) != 0 || dputs("----  ------  ",&rpt) != 0)
			// "Mark  Offset  Line text"
		return drcset();
	n1 = term.t_ncol - 14;
	do {
		if(dputc('-',&rpt) != 0)
			return drcset();
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
				if(dputs(wkbuf,&rpt) != 0)
					return drcset();
				if(n1 && lused(lnp) > 0 && (dputs("  ",&rpt) != 0 || dvizs(lnp == curbp->b_hdrlnp ? "(EOB)" :
				 ltext(lnp),lused(lnp) > max ? max : lused(lnp),VBaseDef,&rpt) != 0))
					return drcset();
				n1 = 0;
				}
			} while((mkp = mkp->mk_nextp) != NULL);
		if(lnp == curbp->b_hdrlnp)
			break;
		lnp = lforw(lnp);
		}

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(bappend(bufp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,bufp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}
