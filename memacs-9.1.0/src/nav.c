// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// nav.c	Basic movement (navigation) functions for MightEMacs.
//
// These routines move the point around on the screen.  The display code always updates the point location, so only functions
// that adjust the top line in the window and invalidate the framing pose any difficulty.

#include "os.h"
#include "std.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "search.h"

/*** Local declarations ***/

static int targcol;			// Goal column for vertical line movements.

// Return true if given dot position is at a boundary (beginning or end of buffer); otherwise, false.
int boundary(Dot *dotp,int dir) {

	return (dir == Forward) ? bufend(dotp) : bufbegin(dotp);
	}

// Return true if the character at dot is a character that is considered to be part of a word.
bool inword(void) {
	Dot *dotp = &si.curwp->w_face.wf_dot;
	return wordlist[(dotp->off == dotp->lnp->l_used ? '\n' : dotp->lnp->l_text[dotp->off])];
	}

// Move point forward (+n) or backward (-n) by abs(n) characters.  Return NotFound (bypassing rcset()) if move would go out of
// the buffer.  Set the move flag if dot moves to a different line.  Return status.
int movech(int n) {

	if(n != 0) {
		Dot *dotp = &si.curwp->w_face.wf_dot;

		if(n > 0) {
			do {
				if(dotp->off == dotp->lnp->l_used) {
					if(dotp->lnp->l_nextp == NULL)
						return NotFound;
					dotp->lnp = dotp->lnp->l_nextp;
					dotp->off = 0;
					si.curwp->w_flags |= WFMove;
					}
				else
					++dotp->off;
				} while(--n > 0);

			}
		else {
			do {
				if(dotp->off == 0) {
					if(dotp->lnp == si.curbp->b_lnp)
						return NotFound;
					dotp->lnp = dotp->lnp->l_prevp;
					dotp->off = dotp->lnp->l_used;
					si.curwp->w_flags |= WFMove;
					}
				else
					--dotp->off;
				} while(++n < 0);
			}
		}

	return rc.status;
	}

// Move point forward by n characters.  If n is negative, call backChar() to actually do the move.  Set rp to false and return
// NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, set rp to true and return status.
int forwChar(Datum *rp,int n,Datum **argpp) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backChar(rp,-n,NULL);

	dsetbool((status = movech(n)) != NotFound,rp);
	return status;
	}

// Move point backward by n characters.  If n is negative, call forwChar() to actually do the move.  Set rp to false and return
// NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, set rp to true and return status.
int backChar(Datum *rp,int n,Datum **argpp) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwChar(rp,-n,NULL);

	dsetbool((status = movech(-n)) != NotFound,rp);
	return status;
	}

// Move point forward (+n) or backward (-n) by abs(n) words.  All of the motion is performed by the movech() routine.  Return
// NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, return status.
int movewd(int n) {

	if(n != 0) {
		if(n > 0) {
			do {
				// Scan through the current word.
				while(inword())
					if(movech(1) == NotFound)
						return NotFound;

				// Scan through the white space.
				while(!inword())
					if(movech(1) == NotFound)
						return NotFound;
				} while(--n > 0);
			}
		else {
			if(movech(-1) == NotFound)
				return NotFound;
			do {
				// Back up through the white space.
				while(!inword())
					if(movech(-1) == NotFound)
						return NotFound;

				// Back up through the current word.
				while(inword())
					if(movech(-1) == NotFound) {
						if(n == -1)
							return rc.status;	// In word at beginning of buffer... no error.
						return NotFound;
						}
				} while(++n < 0);

			// Move to beginning of current word.
			return movech(1);
			}
		}

	return rc.status;
	}

// Move point forward one word on current line.  Return NotFound (bypassing rcset()) if move would go past end of line;
// otherwise, return status.
static int forwwd0(void) {
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Scan through the current word.
	while(inword())
		if(++dotp->off == dotp->lnp->l_used)
			return NotFound;

	// Scan through the white space.
	while(!inword())
		if(++dotp->off == dotp->lnp->l_used)
			return NotFound;

	return rc.status;
	}

// Move point backward one word on current line.  Return NotFound (bypassing rcset()) if move would go past beginning of line;
// otherwise, return status.
static int backwd0(void) {
	Dot *dotp = &si.curwp->w_face.wf_dot;

	if(dotp->off == 0)
		return NotFound;
	--dotp->off;

	// Back up through the white space.
	while(!inword()) {
		if(dotp->off == 0)
			return NotFound;
		--dotp->off;
		}

	// Back up through the current word.
	while(inword()) {
		if(dotp->off == 0)
			return rc.status;	// In word at beginning of line... no error.
		--dotp->off;
		}

	// Move to beginning of current word.
	++dotp->off;
	return rc.status;
	}

// Move point forward by n words.  All of the motion is performed by movech().  Set rp to false if move would go out of the
// buffer; otherwise, true.  Return status.
int forwWord(Datum *rp,int n,Datum **argpp) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rp,-n,NULL);

	dsetbool(movewd(n) != NotFound,rp);
	return rc.status;
	}

// Move point backward by n words.  All of the motion is performed by the movech() routine.  Set rp to false if move would go
// out of the buffer; otherwise, true.  Return status.
int backWord(Datum *rp,int n,Datum **argpp) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwWord(rp,-n,NULL);

	dsetbool(movewd(-n) != NotFound,rp);
	return rc.status;
	}

// Move forward to the end of the nth next word.  Set rp to false if move would go out of the buffer; otherwise, true.  Return
// status.
int endWord(Datum *rp,int n,Datum **argpp) {
	bool b = true;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rp,-n,NULL);

	do {
		// Scan through the white space.
		while(!inword())
			if(movech(1) == NotFound)
				goto Fail;

		// Scan through the current word.
		while(inword())
			if(movech(1) == NotFound) {
				if(n == 1)
					goto Retn;
				goto Fail;
				}
		} while(--n > 0);
	goto Retn;
Fail:
	b = false;
Retn:
	dsetbool(b,rp);
	return rc.status;
	}

// Get a word from current line (or possibly an adjacent line) and return it, or nil if not found.  Return status.
int getWord(Datum *rp,int n,Datum **argpp) {
	Dot *dotp = &si.curwp->w_face.wf_dot;
	int off = dotp->off;						// Save current offset in case of error.

	if(n == INT_MIN)
		n = 0;
	if(inword()) {
		// Starting in a word...
		if(n <= 0) {
			// Move to beginning of current word if n is zero.
			if(n == 0)
				for(;;) {
					if(dotp->off == 0)
						break;			// We're there.
					--dotp->off;
					if(!inword()) {
						++dotp->off;
						break;
						}
					}
			if(n < 0) {
				// Move to beginning of previous word.
				if(n == -1) {
					if(backwd0() == NotFound)	// Hit BOL.
						goto RestorePt;
					}
				else if(movewd(-1) == NotFound)		// Hit BOB.
					goto RestorePt;
				}
			}
		else {
			// Move to beginning of next word.
			if(n == 1) {
				if(forwwd0() == NotFound)		// Hit EOL.
					goto RestorePt;
				}
			else if(movewd(1) == NotFound)			// Hit EOB.
				goto RestorePt;
			}
		}
	else {
		// Starting outside of a word...
		if(n == 0)
			goto NilRtn;					// Current word not found.
		if(n < 0) {
			// Move to beginning of previous word.
			if(n == -1) {
				if(backwd0() == NotFound)		// Hit BOL.
					goto RestorePt;
				}
			else if(movewd(-1) == NotFound)			// Hit BOB.
				goto RestorePt;
			}
		else {
			// Move to beginning of next word.
			if(n == 1) {
				if(forwwd0() == NotFound)		// Hit EOL.
					goto RestorePt;
				}
			else if(movewd(1) == NotFound)			// Hit EOB.
				goto RestorePt;
			}
		}

	// Found beginning of target word... save offset and move to end of it.
	off = dotp->off;
	do {
		if(++dotp->off == dotp->lnp->l_used)			// Hit EOL.
			break;
		} while(inword());

	// Save text and return.
	if(dsetsubstr(dotp->lnp->l_text + off,(dotp->lnp->l_text + dotp->off) - (dotp->lnp->l_text + off),rp) != 0)
		(void) librcset(Failure);
	else if(n < 0)
		dotp->off = off;
	goto Retn;
RestorePt:
	dotp->off = off;
NilRtn:
	dsetnil(rp);
Retn:
	return rc.status;
	}

// Return best choice for an offset in given line, considering given target column.
static int getgoal(Line *lnp,int targ) {
	int col,off;

	col = off = 0;

	// Find position in lnp which most closely matches goal column, or end of line if lnp is too short.
	while(off < lnp->l_used) {
		if((col = newcol(lnp->l_text[off],col)) > targ)
			break;
		++off;
		}
	return off;
	}

// Move forward (+n) or backward (-n) by abs(n) full lines.  The last command controls how the goal column is set.  Return
// NotFound (bypassing rcset()) if move would go out of the buffer.
int moveln(int n) {
	int status = rc.status;

	if(n != 0) {
		Line *lnp;
		Dot *dotp = &si.curwp->w_face.wf_dot;

		if(n > 0) {
			// If we are on the last line as we start, move point to end of line and fail the command.
			if(dotp->lnp->l_nextp == NULL) {
				dotp->off = dotp->lnp->l_used;
				return NotFound;
				}

			// If the last command was not a line move, reset the goal column.
			if(!(kentry.lastflag & CFVMove))
				targcol = getccol(NULL);

			// Flag this command as a line move...
			kentry.thisflag |= CFVMove;

			// and move point down.
			lnp = dotp->lnp;
			do {
				if(lnp->l_nextp == NULL) {
					status = NotFound;
					dotp->off = lnp->l_used;
					goto BufBound;
					}
				lnp = lnp->l_nextp;
				} while(--n > 0);
			}
		else {
			// If we are on the first line as we start, move point to beginning of line and fail the command.
			if(dotp->lnp == si.curbp->b_lnp) {
				dotp->off = 0;
				return NotFound;
				}

			// If the last command was not a line move, reset the goal column.
			if(!(kentry.lastflag & CFVMove))
				targcol = getccol(NULL);

			// Flag this command as a line move...
			kentry.thisflag |= CFVMove;

			// and move point up.
			lnp = dotp->lnp;
			do {
				if(lnp == si.curbp->b_lnp) {
					status = NotFound;
					dotp->off = 0;
					goto BufBound;
					}
				lnp = lnp->l_prevp;
				} while(++n < 0);
			}

		// Reset the current position.
		dotp->off = getgoal(lnp,targcol);
BufBound:
		dotp->lnp = lnp;
		si.curwp->w_flags |= WFMove;
		}

	return status;
	}

// Move forward by n full lines.  If the number of lines to move is negative, call the backward line function to actually do
// the move.  Set rp to false and return NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, return
// true.
int forwLine(Datum *rp,int n,Datum **argpp) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backLine(rp,-n,NULL);

	dsetbool((status = moveln(n)) != NotFound,rp);
	return status;
	}

// This function is like "forwLine", but goes backward.  The scheme is exactly the same.
int backLine(Datum *rp,int n,Datum **argpp) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwLine(rp,-n,NULL);

	dsetbool((status = moveln(-n)) != NotFound,rp);
	return status;
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

// Move dot to the beginning of text (first non-whitespace character) on the current line.  Trivial.  No errors.
int begintxt(void) {
	int c,off,used;
	Dot *dotp;
	Line *lnp;

	used = (lnp = (dotp = &si.curwp->w_face.wf_dot)->lnp)->l_used;
	for(off = 0; off < used; ++off) {
		c = lnp->l_text[off];
		if(c != ' ' && c != '\t')
			break;
		}
	dotp->off = off;

	return rc.status;
	}

// Move dot to the beginning of text (first non-whitespace character) on the [-]nth line.  Set rp to nil and return NotFound
// (bypassing rcset()) if move would go out of the buffer.
int beginText(Datum *rp,int n,Datum **argpp) {

	if(goln(rp,n) == NotFound)
		return NotFound;
	return begintxt();
	}

// Move dot to beginning ("end" is false) or end ("end" is true) of white space at the current position.  Trivial.  No errors.
int spanwhite(bool end) {
	int off;
	short c;
	Dot *dotp;
	Line *lnp;

	lnp = (dotp = &si.curwp->w_face.wf_dot)->lnp;
	off = dotp->off;
	if(end) {
		while(off < lnp->l_used && ((c = lnp->l_text[off]) == ' ' || c == '\t'))
			++off;
		}
	else {
		while(off > 0 && ((c = lnp->l_text[off - 1]) == ' ' || c == '\t'))
			--off;
		}
	dotp->off = off;

	return rc.status;
	}

// Move point to the beginning ("end" is false) or end ("end" is true) of the [-]nth line.  Set rp to nil and return NotFound
// (bypassing rcset()) if move would go out of the buffer.
int beline(Datum *rp,int n,bool end) {

	if(goln(rp,n) == NotFound)
		return NotFound;
	si.curwp->w_face.wf_dot.off = end ? si.curwp->w_face.wf_dot.lnp->l_used : 0;
	return rc.status;
	}

// Go to a line via bufop() call.  Return status.
int goline(Datum *datp,int n,int line) {

	if(line < 0)
		return rcset(Failure,0,text39,text143,line,0);
				// "%s (%d) must be %d or greater","Line number"
	if((si.opflags & OpScript) && n != INT_MIN && (!havesym(s_comma,true) || getsym() != Success))
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
		return librcset(Failure);
	if(si.opflags & OpScript) {
		if(funcarg(datp,ArgFirst | ArgInt1) != Success)
			return rc.status;
		}
	else {
		char wkbuf[strlen(text7) + strlen(text205) + 2];
		sprintf(wkbuf,"%s %s",text7,text205);
				// "Go to","line"
		if(terminp(datp,wkbuf,ArgNil1,0,NULL) != Success || datp->d_type == dat_nil || toint(datp) != Success)
			return rc.status;
		}
	return goline(datp,n,datp->u.d_int);
	}

// Move point in multi-character increments left or right on the current line.
int traverseLine(Datum *rp,int n,Datum **argpp) {
	static bool lastWasForw;		// true if last invocation was forward motion.
	int newCol;
	bool moveForw = true;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	if(dotp->lnp->l_used > 0) {					// If not blank line...
		if(n == 0) {						// zero argument?
			newCol = term.t_ncol - 2;			// Yes, move to right edge of unshifted display.
			if(modeset(MdIdxHScrl,NULL)) {
				if(si.curwp->w_face.wf_firstcol > 0) {
					si.curwp->w_face.wf_firstcol = 0;
					si.curwp->w_flags |= (WFHard | WFMode);
					}
				}
			else if(si.cursp->s_firstcol > 0) {
				si.cursp->s_firstcol = 0;
				si.curwp->w_flags |= (WFHard | WFMode);
				}
			}
		else {							// No, save dot offset and get column positions.
			int jump = si.tjump;
			int curCol = getccol(NULL);
			Dot dot;
			dot.lnp = dotp->lnp;
			dot.off = dotp->lnp->l_used;
			int endCol = getccol(&dot);
			if(endCol <= si.tjump)				// Line too short?
				return rc.status;			// Yep, exit.

			// Figure out initial direction to move (forward or backward), ignoring any argument (for now).
			if(kentry.lastflag & CFTrav)			// Last command was a line traversal (this routine)...
				moveForw = lastWasForw;			// so repeat that direction.
			else if(curCol > (int) (endCol * 0.57))		// If a bit past mid-line...
				moveForw = false;			// go backward.

			// Check bounds and reverse direction if needed.
			if(moveForw && curCol > endCol - si.tjump)
				moveForw = false;
			else if(!moveForw && curCol < si.tjump)
				moveForw = true;

			// Goose it or reverse direction if any non-zero argument.
			if(n != INT_MIN) {				// If argument...
				if((n > 0) == moveForw)			// and same direction as calculated...
					jump = si.tjump * 4;		// then boost distance (4x).
				else
					moveForw = !moveForw;		// Otherwise, reverse direction.
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

// Scroll current window backward or forward n pages, less the current si.overlap.  If n is negative, scroll half a page.
static int bfpage(Datum *rp,int n,int direc) {

	if(n != 0) {
		WindFace *wfp = &si.curwp->w_face;
		bool fullpage = true;
		int pagesize = 1;				// Full page.
		if(n == INT_MIN)
			n = 1;
		else if(n < 0) {
			pagesize = 2;				// Half page.
			n = 1;
			fullpage = false;
			}
		pagesize = si.curwp->w_nrows / pagesize - si.overlap;	// Default number of rows per page to scroll.
		if(pagesize <= 0)				// Tiny window or large si.overlap.
			pagesize = 1;
		n *= pagesize * direc;
		wnewtop(si.curwp,NULL,n);
		if(fullpage || !ptinwind()) {			// If scrolled full page or dot no longer in window...
			wfp->wf_dot.lnp = wfp->wf_toplnp;	// set dot to top line.
			wfp->wf_dot.off = 0;
			}
		si.curwp->w_flags |= WFMove;
		}

	return rc.status;
	}

// Scroll current window forward n pages.
int forwPage(Datum *rp,int n,Datum **argpp) {

	return bfpage(rp,n,1);
	}

// Scroll current window backward n pages.
int backPage(Datum *rp,int n,Datum **argpp) {

	return bfpage(rp,n,-1);
	}

// Move point to given dot position and set window flag if needed.
void movept(Dot *dotp) {
	Dot *wdotp = &si.curwp->w_face.wf_dot;
	if(dotp->lnp != wdotp->lnp)
		si.curwp->w_flags |= WFMove;
	*wdotp = *dotp;
	}

// Get row offset of dot in given window and return it.  If dot is not in the window, return zero.
int getwpos(EWindow *winp) {
	int sline;		// Screen line from top of window.
	Line *lnp;
	WindFace *wfp = &winp->w_face;

	// Search down to the line we want...
	lnp = wfp->wf_toplnp;
	sline = 1;
	while(lnp != wfp->wf_dot.lnp)
		if(sline++ == winp->w_nrows || (lnp = lnp->l_nextp) == NULL)
			return 0;

	// and return the value.
	return sline;
	}

// Find given mark in current buffer and return in *mkpp.  If found, return it if it's active (visible) or MkOpt_Viz not set;
// otherwise, if MkOpt_Query is set, set *mkpp to NULL and return; otherwise, return an error.  If not found, create the mark
// (and assume that the caller will set the mark's dot) if MkOpt_Create set; otherwise, set *mkpp to NULL and return if
// MkOpt_Query or MkOpt_Wind set or script mode and MkOpt_Exist set; otherwise, return an error.  Return status.
int mfind(ushort id,Mark **mkpp,ushort flags) {
	Mark *mkp0 = NULL;
	Mark *mkp1 = &si.curbp->b_mroot;
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
		if((flags & (MkOpt_Query | MkOpt_Wind)) || ((flags & MkOpt_Exist) && (si.opflags & OpScript)))
			goto Found;
ErrRetn:
		return rcset(Failure,RCTermAttr,text11,id);
				// "No mark ~u%c~U in this buffer"
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
	mkp->mk_rfrow = getwpos(winp);
	}

// Check if given character is a valid mark.  Return true if so; otherwise, set an error and return false.
bool markval(Datum *datp) {

	if(datp->u.d_int >= '!' && datp->u.d_int < '~')
		return true;
	(void) rcset(Failure,0,"%s'%s'%s",text9,vizc(datp->u.d_int,VSpace),text345);
					//  "Mark "," invalid"
	return false;
	}

// Get a mark and return it in *mrkpp.  If default n and MkOpt_AutoR or MkOpt_AutoW flag set, return mark RegMark or WrkMark
// (creating the latter if necessary); otherwise, if n < 0 and MkOpt_AutoR or MkOpt_AutoW flag set, return mark WrkMark
// (creating it if necessary and if MkOpt_Create flag set); otherwise, get a key with no default (and set *mkpp to NULL if
// nothing was entered interactively).  Return status.
static int getmark(char *prmt,int n,ushort flags,Mark **mkpp) {

	// Check n.
	if(n < 0 && (flags & (MkOpt_AutoR | MkOpt_AutoW)))
		return mfind((flags & MkOpt_AutoW) || (n < 0 && n != INT_MIN) ? WrkMark : RegMark,mkpp,flags);

	// Get a key.
	ushort id;
	Datum *datp;
	if(dnewtrk(&datp) != 0)
		goto LibFail;
	if(si.opflags & OpScript) {
		if(funcarg(datp,ArgFirst | ArgInt1) != Success || !markval(datp))
			return rc.status;
		id = datp->u.d_int;
		}
	else {
		char pbuf[term.t_ncol];

		if(flags & (MkOpt_Viz | MkOpt_Exist)) {
			Mark *mkp;
			char *delim;
			DStrFab prompt;

			// Build prompt with existing marks in parentheses.
			if(dopentrk(&prompt) != 0 || dputf(&prompt,text346,prmt) != 0 || dputc(' ',&prompt) != 0)
									// "%s mark"
				goto LibFail;
			delim = "(~#u";
			mkp = &si.curbp->b_mroot;
			do {
				if(mkp->mk_id < '~' && (mkp->mk_dot.off >= 0 || (flags & MkOpt_Exist)) &&
				 (mkp->mk_id != RegMark || !(flags & MkOpt_Exist))) {
					if(dputs(delim,&prompt) != 0 || dputc(mkp->mk_id,&prompt) != 0)
						goto LibFail;
					delim = " ";
					}
				} while((mkp = mkp->mk_nextp) != NULL);

			// Error if deleteMark call and no mark other than RegMark found (which can't be deleted).
			if(*delim == '(')
				return rcset(Failure,RCNoFormat,text361);
					// "No mark found to delete"

			if(dputs("~U)",&prompt) != 0 || dclose(&prompt,sf_string) != 0)
				goto LibFail;

			// Fit the prompt in roughly 90% of the terminal width.
			strfit(pbuf,(size_t) term.t_ncol * 90 / 100,prompt.sf_datp->d_str,0);
			}
		else {
			// Build basic prompt.
			sprintf(pbuf,text346,prmt);
				// "%s mark"
			}
		if(terminp(datp,pbuf,ArgNotNull1 | ArgNil1,Term_OneChar | Term_Attr,NULL) != Success)
			return rc.status;
		if(datp->d_type == dat_nil) {
			*mkpp = NULL;
			return rc.status;
			}
		if(datp->u.d_int < '!' || datp->u.d_int >= '~')
			return rcset(Failure,0,"%s'%s'%s",text9,vizc(datp->u.d_int,VSpace),text345);
						//  "Mark "," invalid"
		id = datp->u.d_int;
		}

	// Success.  Return mark.
	return mfind(id,mkpp,flags);
LibFail:
	return librcset(Failure);
	}

// Set a mark in the current buffer to dot.  If default n, use RegMark; otherwise, if n < 0, use WrkMark; otherwise, get a mark
// with no default.  Return status.
int setMark(Datum *rp,int n,Datum **argpp) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text64,n,MkOpt_AutoR | MkOpt_Create,&mkp) != Success || mkp == NULL)
			// "Set"
		return rc.status;

	// Set mark to dot and return.
	mset(mkp,si.curwp);
	return rcset(Success,RCTermAttr,"%s~u%c~U %s",text9,mkp->mk_id,text350);
						// "Mark ","set"
	}

// Delete given mark.  Return status.
static int delmark(Mark *mkp) {

	if(mkp->mk_id == RegMark)
		return rcset(Failure,RCTermAttr,text352,RegMark);
			// "Cannot delete mark ~u%c~U"

	// It's a go... delete it.
	Mark *mkp0 = &si.curbp->b_mroot;
	ushort id = mkp->mk_id;
	for(;;) {
		if(mkp0->mk_nextp == mkp)
			break;
		mkp0 = mkp0->mk_nextp;
		}
	mkp0->mk_nextp = mkp->mk_nextp;
	(void) free((void *) mkp);
	return rcset(Success,RCTermAttr,"%s~u%c~U %s",text9,id,text10);
						// "Mark ","deleted"
	}

// Remove a mark in the current buffer.  If non-default n, remove all marks.  No error if script mode and mark not found.
int deleteMark(Datum *rp,int n,Datum **argpp) {

	// Delete all?
	if(n != INT_MIN) {
		mdelete(si.curbp,0);
		(void) rcset(Success,RCNoFormat,text351);
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

// Go to given mark in current buffer, but don't force reframe if mark is already in the window.
static void gomark(Mark *mkp) {
	Line *lnp = si.curwp->w_face.wf_dot.lnp;		// Remember current line.
	si.curwp->w_face.wf_dot = mkp->mk_dot;			// Set point to mark.
	if(mkp->mk_dot.lnp != lnp) {				// If still on same line, do nothing else.
		if(inwind(si.curwp,mkp->mk_dot.lnp))		// Otherwise, set minimal window-update flag.
			si.curwp->w_flags |= WFMove;
		else {
			si.curwp->w_rfrow = mkp->mk_rfrow;
			si.curwp->w_flags |= WFReframe;
			}
		}
	}

// Swap a mark with dot, given mark pointer.  Return status.
static int swapmkp(Mark *mkp) {
	Dot odot = si.curwp->w_face.wf_dot;
	short orow = getwpos(si.curwp);
	gomark(mkp);
	mkp->mk_dot = odot;
	mkp->mk_rfrow = orow;
	return rc.status;
	}

// Swap the values of dot and a mark in the current window.  If default n, use RegMark; otherwise, if n < 0, use mark WrkMark;
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

// Mark current buffer from beginning to end and preserve current position in a mark.  If default n or n < 0, use WrkMark;
// otherwise, get a mark with no default.  Return status.
int markBuf(Datum *rp,int n,Datum **argpp) {
	Mark *mkp;

	// Make sure mark is valid.
	if(getmark(text348,n,MkOpt_AutoW | MkOpt_Create,&mkp) != Success || mkp == NULL)
			// "Save point in"
		return rc.status;

	// Mark whole buffer.  If RegMark was specified for saving point, user is out of luck (it will be overwritten).
	mset(mkp,si.curwp);					// Preserve current position.
	(void) execCF(rp,INT_MIN,cftab + cf_beginBuf,0,0);	// Move to beginning of buffer.
	mset(&si.curbp->b_mroot,si.curwp);			// Set to mark RegMark.
	(void) execCF(rp,INT_MIN,cftab + cf_endBuf,0,0);	// Move to end of buffer.
	if(rc.status == Success)
		rcclear(0);
	return (mkp->mk_id == RegMark) ? rc.status : rcset(Success,RCTermAttr,text233,mkp->mk_id);
							// "Mark ~u%c~U set to previous position"
	}

// Find a matching fence and either move point to it, or create a region.  Scan for the matching fence forward or backward,
// depending on the source fence character.  Use "fence" as the source fence if it's value is > 0; otherwise, use character at
// point.  If the fence is found: if regp is not NULL, set dot in the Region object to original point, set line count to zero
// (not used), set size to the number of characters traversed + 1 as a positive (point moved forward) or negative (point moved
// backward) amount, and restore point position; otherwise, set point to the matching fence position.  If the fence is not
// found: restore point position.  Return status.
int otherfence(short fence,Region *regp) {
	int scanDirec;		// Direction of search.
	int flevel;		// Current fence level.
	short ofence;		// Other fence character.
	short c;		// Current character in scan.
	long chunk;
	int n;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	Dot odot = *dotp;	// Original point position.

	// Get the source fence character.
	if(fence == 0) {
		if(dotp->off == dotp->lnp->l_used)
			goto Bust;
		fence = dotp->lnp->l_text[dotp->off];
		}

	// Determine matching fence.
	switch(fence) {
		case '(':
			ofence = ')';
			goto Forw;
		case '{':
			ofence = '}';
			goto Forw;
		case '[':
			ofence = ']';
			goto Forw;
		case '<':
			ofence = '>';
Forw:
			scanDirec = Forward;
			break;
		case ')':
			ofence = '(';
			goto Back;
		case '}':
			ofence = '{';
			goto Back;
		case ']':
			ofence = '[';
			goto Back;
		case '>':
			ofence = '<';
Back:
			scanDirec = Backward;
			break;
		default:
			goto Bust;
	}

	// Set up for scan.
	chunk = 0;
	flevel = 1;

	// Scan until we find it, or reach a buffer boundary.
	n = (scanDirec == Forward) ? 1 : -1;
	while(flevel > 0) {
		(void) movech(n);
		++chunk;
		if(dotp->off == dotp->lnp->l_used)
			c = '\n';
		else
			c = dotp->lnp->l_text[dotp->off];
		if(c == fence)
			++flevel;
		else if(c == ofence)
			--flevel;
		if(boundary(dotp,scanDirec))
			break;
		}

	// If flevel is zero, we have a match.
	if(flevel == 0) {
		// Create region and restore point, or keep current point position.
		if(regp != NULL) {
			*dotp = odot;
			if(scanDirec == Forward) {
				regp->r_dot = *dotp;
				regp->r_size = chunk + 1;
				}
			else {
				(void) movech(1);
				regp->r_dot = *dotp;
				(void) movech(-1);
				regp->r_size = -(chunk + 1);
				}
			regp->r_linect = 0;
			}
		else
			si.curwp->w_flags |= WFMove;
		return rc.status;
		}

	// Matching fence not found: restore previous position.
	*dotp = odot;
Bust:
	// Beep if interactive.
	if(!(si.opflags & OpScript))
		(void) TTbeep();
	return rcset(Failure,0,NULL);
	}

// Move point to a matching fence if possible and return Boolean result.
int gotoFence(Datum *rp,int n,Datum **argpp) {
	short fence;
	bool result = true;

	// Get source fence character...
	if(n == INT_MIN)
		fence = 0;
	else if(si.opflags & OpScript) {
		if(!charval(argpp[0]))
			return rc.status;
		fence = argpp[0]->u.d_int;
		}
	else {
		if(terminp(rp,text179,ArgNotNull1 | ArgNil1,Term_OneChar,NULL) != Success || rp->d_type == dat_nil)
				// "Fence char"
			return rc.status;
		fence = rp->u.d_int;
		}

	// go to other fence...
	if(otherfence(fence,NULL) != Success) {
		if(rc.status < Failure)
			return rc.status;
		rcclear(0);
		result = false;
		}

	// and return result.	
	dsetbool(result,rp);
	return rc.status;
	}

// Move point backward or forward n tab stops.  Return -1 if invalid move; otherwise, new offset in current line.
int tabstop(int n) {
	int off,len,col,tabsize,curstop,targstop;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Check for "do nothing" cases.
	if(n == 0 || (len = dotp->lnp->l_used) == 0 || ((off = dotp->off) == 0 && n < 0) || (off == len && n > 0))
		return -1;

	// Calculate target tab stop column.
	tabsize = (si.stabsize == 0) ? si.htabsize : si.stabsize;
	curstop = (col = getccol(NULL)) / tabsize;
	if(col % tabsize != 0 && n < 0)
		++curstop;
	return (targstop = curstop + n) <= 0 ? 0 : getgoal(dotp->lnp,targstop * tabsize);
	}

// Move point backward or forward n tab stops.
int bftab(int n) {
	int off;

	if((off = tabstop(n)) >= 0)
		si.curwp->w_face.wf_dot.off = off;
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
	if(sysbuf(text353,&bufp,BFTermAttr) != Success)
			// "Marks"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;

	// Construct the header lines.
	if(dputs(text354,&rpt) != 0 || dputc('\n',&rpt) != 0 || dputs("----  ------  ",&rpt) != 0)
			// "~bMark  Offset  Line text~0"
		goto LibFail;
	n1 = term.t_ncol - 14;
	do {
		if(dputc('-',&rpt) != 0)
			goto LibFail;
		} while(--n1 > 0);

	// Loop through lines in current buffer, searching for mark matches.
	lnp = si.curbp->b_lnp;
	do {
		mkp = &si.curbp->b_mroot;
		do {
			if(mkp->mk_id < '~' && mkp->mk_dot.lnp == lnp) {
				(void) sprintf(wkbuf,"\n %c  %8d",mkp->mk_id,mkp->mk_dot.off);
				if(dputs(wkbuf,&rpt) != 0)
					goto LibFail;
				if(lnp->l_nextp == NULL && mkp->mk_dot.off == lnp->l_used) {
					if(dputs("  (EOB)",&rpt) != 0)
						goto LibFail;
					}
				else if(lnp->l_used > 0 && (dputs("  ",&rpt) != 0 ||
				 dvizs(lnp->l_text,lnp->l_used > max ? max : lnp->l_used,VBaseDef,&rpt) != 0))
					goto LibFail;
				}
			} while((mkp = mkp->mk_nextp) != NULL);
		} while((lnp = lnp->l_nextp) != NULL);

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(bufp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n,bufp,RendNewBuf | RendReset);
	}
