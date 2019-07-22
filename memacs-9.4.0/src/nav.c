// (c) Copyright 2019 Richard W. Marinelli
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

// Fence-matching parameters.
typedef struct {
	short fence;			// Source fence character.
	short ofence;			// Other (matching) fence character.
	int scanDirec;			// Scan direction (-1 or +1).
	Point opoint;			// Original point position.
	} FenceMatch;

// Return true if given point position is at a boundary (beginning or end of buffer); otherwise, false.
int boundary(Point *point,int dir) {

	return (dir == Forward) ? bufend(point) : bufbegin(point);
	}

// Return true if the character at point is a character that is considered to be part of a word.
bool inword(void) {
	Point *point = &si.curwin->w_face.wf_point;
	return wordlist[(point->off == point->lnp->l_used ? '\n' : point->lnp->l_text[point->off])];
	}

// Move point forward (+n) or backward (-n) by abs(n) characters.  Return NotFound (bypassing rcset()) if move would go out of
// the buffer.  Set the move flag if point moves to a different line.  Return status.
int movech(int n) {

	if(n != 0) {
		Point *point = &si.curwin->w_face.wf_point;

		if(n > 0) {
			do {
				if(point->off == point->lnp->l_used) {
					if(point->lnp->l_next == NULL)
						return NotFound;
					point->lnp = point->lnp->l_next;
					point->off = 0;
					si.curwin->w_flags |= WFMove;
					}
				else
					++point->off;
				} while(--n > 0);

			}
		else {
			do {
				if(point->off == 0) {
					if(point->lnp == si.curbuf->b_lnp)
						return NotFound;
					point->lnp = point->lnp->l_prev;
					point->off = point->lnp->l_used;
					si.curwin->w_flags |= WFMove;
					}
				else
					--point->off;
				} while(++n < 0);
			}
		}

	return rc.status;
	}

// Move point forward by n characters.  If n is negative, call backChar() to actually do the move.  Set rval to false and return
// NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, set rval to true and return status.
int forwChar(Datum *rval,int n,Datum **argv) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backChar(rval,-n,NULL);

	dsetbool((status = movech(n)) != NotFound,rval);
	return status;
	}

// Move point backward by n characters.  If n is negative, call forwChar() to actually do the move.  Set rval to false and
// return NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, set rval to true and return status.
int backChar(Datum *rval,int n,Datum **argv) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwChar(rval,-n,NULL);

	dsetbool((status = movech(-n)) != NotFound,rval);
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
	Point *point = &si.curwin->w_face.wf_point;

	// Scan through the current word.
	while(inword())
		if(++point->off == point->lnp->l_used)
			return NotFound;

	// Scan through the white space.
	while(!inword())
		if(++point->off == point->lnp->l_used)
			return NotFound;

	return rc.status;
	}

// Move point backward one word on current line.  Return NotFound (bypassing rcset()) if move would go past beginning of line;
// otherwise, return status.
static int backwd0(void) {
	Point *point = &si.curwin->w_face.wf_point;

	if(point->off == 0)
		return NotFound;
	--point->off;

	// Back up through the white space.
	while(!inword()) {
		if(point->off == 0)
			return NotFound;
		--point->off;
		}

	// Back up through the current word.
	while(inword()) {
		if(point->off == 0)
			return rc.status;	// In word at beginning of line... no error.
		--point->off;
		}

	// Move to beginning of current word.
	++point->off;
	return rc.status;
	}

// Move point forward by n words.  All of the motion is performed by movech().  Set rval to false if move would go out of the
// buffer; otherwise, true.  Return status.
int forwWord(Datum *rval,int n,Datum **argv) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rval,-n,NULL);

	dsetbool(movewd(n) != NotFound,rval);
	return rc.status;
	}

// Move point backward by n words.  All of the motion is performed by the movech() routine.  Set rval to false if move would go
// out of the buffer; otherwise, true.  Return status.
int backWord(Datum *rval,int n,Datum **argv) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwWord(rval,-n,NULL);

	dsetbool(movewd(-n) != NotFound,rval);
	return rc.status;
	}

// Move forward to the end of the nth next word.  Set rval to false if move would go out of the buffer; otherwise, true.  Return
// status.
int endWord(Datum *rval,int n,Datum **argv) {
	bool b = true;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(rval,-n,NULL);

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
	dsetbool(b,rval);
	return rc.status;
	}

// Get a word from current line (or possibly an adjacent line) and return it, or nil if not found.  Return status.
int getWord(Datum *rval,int n,Datum **argv) {
	Point *point = &si.curwin->w_face.wf_point;
	int off = point->off;						// Save current offset in case of error.

	if(n == INT_MIN)
		n = 0;
	if(inword()) {
		// Starting in a word...
		if(n <= 0) {
			// Move to beginning of current word if n is zero.
			if(n == 0)
				for(;;) {
					if(point->off == 0)
						break;			// We're there.
					--point->off;
					if(!inword()) {
						++point->off;
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
	off = point->off;
	do {
		if(++point->off == point->lnp->l_used)			// Hit EOL.
			break;
		} while(inword());

	// Save text and return.
	if(dsetsubstr(point->lnp->l_text + off,(point->lnp->l_text + point->off) - (point->lnp->l_text + off),rval) != 0)
		(void) librcset(Failure);
	else if(n < 0)
		point->off = off;
	goto Retn;
RestorePt:
	point->off = off;
NilRtn:
	dsetnil(rval);
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
		Point *point = &si.curwin->w_face.wf_point;

		if(n > 0) {
			// If we are on the last line as we start, move point to end of line and fail the command.
			if(point->lnp->l_next == NULL) {
				point->off = point->lnp->l_used;
				return NotFound;
				}

			// If the last command was not a line move, reset the goal column.
			if(!(kentry.lastflag & CFVMove))
				targcol = getcol(NULL,false);

			// Flag this command as a line move...
			kentry.thisflag |= CFVMove;

			// and move point down.
			lnp = point->lnp;
			do {
				if(lnp->l_next == NULL) {
					status = NotFound;
					point->off = lnp->l_used;
					goto BufBound;
					}
				lnp = lnp->l_next;
				} while(--n > 0);
			}
		else {
			// If we are on the first line as we start, move point to beginning of line and fail the command.
			if(point->lnp == si.curbuf->b_lnp) {
				point->off = 0;
				return NotFound;
				}

			// If the last command was not a line move, reset the goal column.
			if(!(kentry.lastflag & CFVMove))
				targcol = getcol(NULL,false);

			// Flag this command as a line move...
			kentry.thisflag |= CFVMove;

			// and move point up.
			lnp = point->lnp;
			do {
				if(lnp == si.curbuf->b_lnp) {
					status = NotFound;
					point->off = 0;
					goto BufBound;
					}
				lnp = lnp->l_prev;
				} while(++n < 0);
			}

		// Reset the current position.
		point->off = getgoal(lnp,targcol);
BufBound:
		point->lnp = lnp;
		si.curwin->w_flags |= WFMove;
		}

	return status;
	}

// Move forward by n full lines.  If the number of lines to move is negative, call the backward line function to actually do
// the move.  Set rval to false and return NotFound (bypassing rcset()) if move would go out of the buffer; otherwise, return
// true.
int forwLine(Datum *rval,int n,Datum **argv) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backLine(rval,-n,NULL);

	dsetbool((status = moveln(n)) != NotFound,rval);
	return status;
	}

// This function is like "forwLine", but goes backward.  The scheme is exactly the same.
int backLine(Datum *rval,int n,Datum **argv) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwLine(rval,-n,NULL);

	dsetbool((status = moveln(-n)) != NotFound,rval);
	return status;
	}

// Move point to the [-]nth line and clear the "line move" flag (for subsequent point move).  Set rval to nil and return
// NotFound (bypassing rcset()) if move would go out of the buffer.
static int goln(Datum *rval,int n) {
	int status = Success;

	if(n > 1)
		status = forwLine(rval,n - 1,NULL);
	else if(n < 0 && n != INT_MIN)
		status = backLine(rval,-n,NULL);
	kentry.thisflag &= ~CFVMove;

	return (status == NotFound) ? NotFound : rc.status;
	}

// Move point to the beginning of text (first non-whitespace character) on the current line.  Trivial.  No errors.
int begintxt(void) {
	int c,off,used;
	Point *point;
	Line *lnp;

	used = (lnp = (point = &si.curwin->w_face.wf_point)->lnp)->l_used;
	for(off = 0; off < used; ++off) {
		c = lnp->l_text[off];
		if(c != ' ' && c != '\t')
			break;
		}
	point->off = off;

	return rc.status;
	}

// Move point to the beginning of text (first non-whitespace character) on the [-]nth line.  Set rval to nil and return NotFound
// (bypassing rcset()) if move would go out of the buffer.
int beginText(Datum *rval,int n,Datum **argv) {

	if(goln(rval,n) == NotFound)
		return NotFound;
	return begintxt();
	}

// Move point to beginning ("end" is false) or end ("end" is true) of white space at the current position.  Trivial.  No errors.
int spanwhite(bool end) {
	int off;
	short c;
	Point *point;
	Line *lnp;

	lnp = (point = &si.curwin->w_face.wf_point)->lnp;
	off = point->off;
	if(end) {
		while(off < lnp->l_used && ((c = lnp->l_text[off]) == ' ' || c == '\t'))
			++off;
		}
	else {
		while(off > 0 && ((c = lnp->l_text[off - 1]) == ' ' || c == '\t'))
			--off;
		}
	point->off = off;

	return rc.status;
	}

// Move point to the beginning ("end" is false) or end ("end" is true) of the [-]nth line.  Set rval to nil and return NotFound
// (bypassing rcset()) if move would go out of the buffer.
int beline(Datum *rval,int n,bool end) {

	if(goln(rval,n) == NotFound)
		return NotFound;
	si.curwin->w_face.wf_point.off = end ? si.curwin->w_face.wf_point.lnp->l_used : 0;
	return rc.status;
	}

// Go to a line via bufop() call.  Return status.
int goline(Datum *datum,int n,int line) {

	if(line < 0)
		return rcset(Failure,0,text39,text143,line,0);
				// "%s (%d) must be %d or greater","Line number"
	if((si.opflags & OpScript) && n != INT_MIN && (!havesym(s_comma,true) || getsym() != Success))
		return rc.status;

	// Go to line.
	return bufop(datum,n,text229 + 2,BOpGotoLn,line);
		// ", in"
	}

// Move to a particular line, or end of buffer if line number is zero.  If n argument given, move point in specified buffer;
// otherwise, current one.  Return status.
int gotoLine(Datum *rval,int n,Datum **argv) {
	Datum *datum;

	// Get line number and validate it.
	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript) {
		if(funcarg(datum,ArgFirst | ArgInt1) != Success)
			return rc.status;
		}
	else {
		char wkbuf[strlen(text7) + strlen(text205) + 2];
		sprintf(wkbuf,"%s %s",text7,text205);
				// "Go to","line"
		if(terminp(datum,wkbuf,ArgNil1,0,NULL) != Success || datum->d_type == dat_nil || toint(datum) != Success)
			return rc.status;
		}
	return goline(datum,n,datum->u.d_int);
	}

// Move point in multi-character increments left or right on the current line.
int traverseLine(Datum *rval,int n,Datum **argv) {
	static bool lastWasForw;		// true if last invocation was forward motion.
	int newCol;
	bool moveForw = true;
	Point *point = &si.curwin->w_face.wf_point;

	if(point->lnp->l_used > 0) {					// If not blank line...
		if(n == 0) {						// zero argument?
			newCol = term.t_ncol - 2;			// Yes, move to right edge of unshifted display.
			if(modeset(MdIdxHScrl,NULL)) {
				if(si.curwin->w_face.wf_firstcol > 0) {
					si.curwin->w_face.wf_firstcol = 0;
					si.curwin->w_flags |= (WFHard | WFMode);
					}
				}
			else if(si.curscr->s_firstcol > 0) {
				si.curscr->s_firstcol = 0;
				si.curwin->w_flags |= (WFHard | WFMode);
				}
			}
		else {							// No, save point offset and get column positions.
			int jump = si.tjump;
			int curCol = getcol(NULL,false);
			Point wkpoint;
			wkpoint.lnp = point->lnp;
			wkpoint.off = point->lnp->l_used;
			int endCol = getcol(&wkpoint,false);
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
static int bfpage(Datum *rval,int n,int direc) {

	if(n != 0) {
		WindFace *wfp = &si.curwin->w_face;
		bool fullpage = true;
		int pagesize = 1;				// Full page.
		if(n == INT_MIN)
			n = 1;
		else if(n < 0) {
			pagesize = 2;				// Half page.
			n = 1;
			fullpage = false;
			}
		pagesize = si.curwin->w_nrows / pagesize - si.overlap;	// Default number of rows per page to scroll.
		if(pagesize <= 0)				// Tiny window or large si.overlap.
			pagesize = 1;
		n *= pagesize * direc;
		wnewtop(si.curwin,NULL,n);
		if(fullpage || !ptinwind()) {			// If scrolled full page or point no longer in window...
			wfp->wf_point.lnp = wfp->wf_toplnp;	// set point to top line.
			wfp->wf_point.off = 0;
			}
		si.curwin->w_flags |= WFMove;
		}

	return rc.status;
	}

// Scroll current window forward n pages.
int forwPage(Datum *rval,int n,Datum **argv) {

	return bfpage(rval,n,1);
	}

// Scroll current window backward n pages.
int backPage(Datum *rval,int n,Datum **argv) {

	return bfpage(rval,n,-1);
	}

// Move point to given point position and set window flag if needed.
void movept(Point *point) {
	Point *wkpoint = &si.curwin->w_face.wf_point;
	if(point->lnp != wkpoint->lnp)
		si.curwin->w_flags |= WFMove;
	*wkpoint = *point;
	}

// Get row offset of point in given window and return it.  If point is not in the window, return zero.
int getwpos(EWindow *win) {
	int sline;		// Screen line from top of window.
	Line *lnp;
	WindFace *wfp = &win->w_face;

	// Search down to the line we want...
	lnp = wfp->wf_toplnp;
	sline = 1;
	while(lnp != wfp->wf_point.lnp)
		if(sline++ == win->w_nrows || (lnp = lnp->l_next) == NULL)
			return 0;

	// and return the value.
	return sline;
	}

// Find given mark in current buffer and return in *markp.  If found, return it if it's active (visible) or MkOpt_Viz not set;
// otherwise, if MkOpt_Query is set, set *markp to NULL and return; otherwise, return an error.  If not found, create the mark
// (and assume that the caller will set the mark's point) if MkOpt_Create set; otherwise, set *markp to NULL and return if
// MkOpt_Query or MkOpt_Wind set or script mode and MkOpt_Exist set; otherwise, return an error.  Return status.
int mfind(ushort id,Mark **markp,ushort flags) {
	Mark *mkp0 = NULL;
	Mark *mkp1 = &si.curbuf->b_mroot;
	do {
		if(mkp1->mk_id == id) {

			// Found it.  Return it if appropriate.
			if(mkp1->mk_point.off >= 0 || !(flags & MkOpt_Viz))
				goto Found;
			if(flags & MkOpt_Query) {
				mkp1 = NULL;
				goto Found;
				}
			goto ErrRetn;
			}
		mkp0 = mkp1;
		} while((mkp1 = mkp1->mk_next) != NULL);

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
	mkp1->mk_next = NULL;
	mkp1->mk_id = id;
	mkp0->mk_next = mkp1;
Found:
	*markp = mkp1;
	return rc.status;
	}

// Set given mark to point in given window.
void mset(Mark *mark,EWindow *win) {

	mark->mk_point = win->w_face.wf_point;
	mark->mk_rfrow = getwpos(win);
	}

// Check if given character is a valid mark.  Return true if so; otherwise, set an error and return false.
bool markval(Datum *datum) {

	if(datum->u.d_int >= '!' && datum->u.d_int < '~')
		return true;
	(void) rcset(Failure,0,"%s'%s'%s",text9,vizc(datum->u.d_int,VSpace),text345);
					//  "Mark "," invalid"
	return false;
	}

// Get a mark and return it in *markp.  If default n and MkOpt_AutoR or MkOpt_AutoW flag set, return mark RegMark or WrkMark
// (creating the latter if necessary); otherwise, get a key with no default (and set *markp to NULL if nothing was entered
// interactively).  Return status.
static int getmark(char *prmt,int n,ushort flags,Mark **markp) {

	// Check n.
	if(n == INT_MIN && (flags & (MkOpt_AutoR | MkOpt_AutoW)))
		return mfind((flags & MkOpt_AutoW) ? WrkMark : RegMark,markp,flags);

	// Get a key.
	ushort id;
	Datum *datum;
	if(dnewtrk(&datum) != 0)
		goto LibFail;
	if(si.opflags & OpScript) {
		if(funcarg(datum,ArgFirst | ArgInt1) != Success || !markval(datum))
			return rc.status;
		id = datum->u.d_int;
		}
	else {
		char pbuf[term.t_ncol];

		if(flags & (MkOpt_Viz | MkOpt_Exist)) {
			Mark *mark;
			char *delim;
			DStrFab prompt;
			char wkbuf[5];

			// Build prompt with existing marks in parentheses.
			if(dopentrk(&prompt) != 0 || dputf(&prompt,text346,prmt) != 0 || dputc(' ',&prompt) != 0)
									// "%s mark"
				goto LibFail;
			sprintf(delim = wkbuf,"(%c%c%c",AttrSpecBegin,AttrAlt,AttrULOn);
			mark = &si.curbuf->b_mroot;
			do {
				if(mark->mk_id < '~' && (mark->mk_point.off >= 0 || (flags & MkOpt_Exist)) &&
				 (mark->mk_id != RegMark || !(flags & MkOpt_Exist))) {
					if(dputs(delim,&prompt) != 0 || dputc(mark->mk_id,&prompt) != 0)
						goto LibFail;
					delim = " ";
					}
				} while((mark = mark->mk_next) != NULL);

			// Error if delMark call and no mark other than RegMark found (which can't be deleted).
			if(*delim == '(')
				return rcset(Failure,RCNoFormat,text361);
					// "No mark found to delete"

			if(dputf(&prompt,"%c%c)",AttrSpecBegin,AttrULOff) != 0 || dclose(&prompt,sf_string) != 0)
				goto LibFail;

			// Fit the prompt in roughly 90% of the terminal width.
			strfit(pbuf,(size_t) term.t_ncol * 90 / 100,prompt.sf_datum->d_str,0);
			}
		else {
			// Build basic prompt.
			sprintf(pbuf,text346,prmt);
				// "%s mark"
			}
		if(terminp(datum,pbuf,ArgNotNull1 | ArgNil1,Term_OneChar | Term_Attr,NULL) != Success)
			return rc.status;
		if(datum->d_type == dat_nil) {
			*markp = NULL;
			return rc.status;
			}
		if(datum->u.d_int < '!' || datum->u.d_int >= '~')
			return rcset(Failure,0,"%s'%s'%s",text9,vizc(datum->u.d_int,VSpace),text345);
						//  "Mark "," invalid"
		id = datum->u.d_int;
		}

	// Success.  Return mark.
	return mfind(id,markp,flags);
LibFail:
	return librcset(Failure);
	}

// Set a mark in the current buffer to point.  If default n, use RegMark; otherwise, get a mark with no default.  Return status.
int setMark(Datum *rval,int n,Datum **argv) {
	Mark *mark;

	// Make sure mark is valid.
	if(getmark(text64,n,MkOpt_AutoR | MkOpt_Create,&mark) != Success || mark == NULL)
			// "Set"
		return rc.status;

	// Set mark to point and return.
	mset(mark,si.curwin);
	return rcset(Success,RCTermAttr,"%s%c%c%c%c%c %s",
	 text9,AttrSpecBegin,AttrULOn,mark->mk_id,AttrSpecBegin,AttrULOff,text350);
		// "Mark ","set"
	}

// Delete given mark.  Return status.
static int delmark(Mark *mark) {

	if(mark->mk_id == RegMark)
		return rcset(Failure,RCTermAttr,text352,RegMark);
			// "Cannot delete mark ~u%c~U"

	// It's a go... delete it.
	Mark *mkp0 = &si.curbuf->b_mroot;
	ushort id = mark->mk_id;
	for(;;) {
		if(mkp0->mk_next == mark)
			break;
		mkp0 = mkp0->mk_next;
		}
	mkp0->mk_next = mark->mk_next;
	(void) free((void *) mark);
	return rcset(Success,RCTermAttr,"%s%c%c%c%c%c %s",text9,AttrSpecBegin,AttrULOn,id,AttrSpecBegin,AttrULOff,text10);
							// "Mark ","deleted"
	}

// Remove a mark in the current buffer.  If non-default n, remove all marks.  No error if script mode and mark not found.
int delMark(Datum *rval,int n,Datum **argv) {

	// Delete all?
	if(n != INT_MIN) {
		mdelete(si.curbuf,0);
		(void) rcset(Success,RCNoFormat,text351);
			// "All marks deleted"
		}
	else {
		Mark *mark;

		// Make sure mark is valid.
		if(getmark(text26,n,MkOpt_Hard | MkOpt_Exist,&mark) != Success || mark == NULL)
				// "Delete"
			return rc.status;
		(void) delmark(mark);
		}

	return rc.status;
	}

// Go to given mark in current buffer, but don't force reframe if mark is already in the window.
static void gomark(Mark *mark) {
	Line *lnp = si.curwin->w_face.wf_point.lnp;		// Remember current line.
	si.curwin->w_face.wf_point = mark->mk_point;			// Set point to mark.
	if(mark->mk_point.lnp != lnp) {				// If still on same line, do nothing else.
		if(inwind(si.curwin,mark->mk_point.lnp))		// Otherwise, set minimal window-update flag.
			si.curwin->w_flags |= WFMove;
		else {
			si.curwin->w_rfrow = mark->mk_rfrow;
			si.curwin->w_flags |= WFReframe;
			}
		}
	}

// Swap a mark with point, given mark pointer.  Return status.
static int swapmrk(Mark *mark) {
	Point opoint = si.curwin->w_face.wf_point;
	short orow = getwpos(si.curwin);
	gomark(mark);
	mark->mk_point = opoint;
	mark->mk_rfrow = orow;
	return rc.status;
	}

// Swap the values of point and a mark in the current window.  Get a mark with no default and return status.
int swapMark(Datum *rval,int n,Datum **argv) {
	Mark *mark;

	// Make sure mark is valid.
	if(getmark(text347,n,MkOpt_Hard | MkOpt_Viz,&mark) != Success || mark == NULL)
			// "Swap point with"
		return rc.status;

	// Swap point and the mark.
	return swapmrk(mark);
	}

// Swap a mark with point, given mark id.  Return status.
int swapmid(ushort id) {
	Mark *mark;

	if(mfind(id,&mark,MkOpt_Viz) == Success)
		(void) swapmrk(mark);
	return rc.status;
	}

// Go to a mark in the current window.  Get a mark with no default, move point, then delete mark if n < 0.  Return status.
int gotoMark(Datum *rval,int n,Datum **argv) {
	Mark *mark;

	// Make sure mark is valid.
	if(getmark(text7,n,MkOpt_Hard | MkOpt_Viz,&mark) != Success || mark == NULL)
			// "Go to"
		return rc.status;

	// Set point to the mark.
	gomark(mark);

	// Delete mark if applicable.
	if(n < 0 && n != INT_MIN)
		(void) delmark(mark);

	return rc.status;
	}

// Mark current buffer from beginning to end and preserve current position in a mark.  If default n, use WrkMark; otherwise, get
// a mark with no default.  Return status.
int markBuf(Datum *rval,int n,Datum **argv) {
	Mark *mark;

	// Make sure mark is valid.
	if(getmark(text348,n,MkOpt_AutoW | MkOpt_Create,&mark) != Success || mark == NULL)
			// "Save point in"
		return rc.status;

	// Mark whole buffer.  If RegMark was specified for saving point, user is out of luck (it will be overwritten).
	mset(mark,si.curwin);					// Preserve current position.
	(void) execCF(rval,INT_MIN,cftab + cf_beginBuf,0,0);	// Move to beginning of buffer.
	mset(&si.curbuf->b_mroot,si.curwin);			// Set to mark RegMark.
	(void) execCF(rval,INT_MIN,cftab + cf_endBuf,0,0);	// Move to end of buffer.
	if(rc.status == Success)
		rcclear(0);
	return (mark->mk_id == RegMark) ? rc.status : rcset(Success,RCTermAttr,text233,mark->mk_id);
							// "Mark ~u%c~U set to previous position"
	}

// Prepare for an "other fence" scan, given source fence character and pointer to FenceMatch object.  If valid source fence
// found, set object parameters and return current point pointer (Point *); otherwise, return NULL.
static Point *fenceprep(short fence,FenceMatch *fmatch) {
	Point *point = &si.curwin->w_face.wf_point;

	// Get the source fence character.
	if(fence == 0) {
		if(point->off == point->lnp->l_used)
			return NULL;
		fence = point->lnp->l_text[point->off];
		}
	fmatch->fence = fence;
	fmatch->opoint = *point;			// Original point position.

	// Determine matching fence.
	switch(fence) {
		case '(':
			fmatch->ofence = ')';
			goto Forw;
		case '{':
			fmatch->ofence = '}';
			goto Forw;
		case '[':
			fmatch->ofence = ']';
			goto Forw;
		case '<':
			fmatch->ofence = '>';
Forw:
			fmatch->scanDirec = Forward;
			break;
		case ')':
			fmatch->ofence = '(';
			goto Back;
		case '}':
			fmatch->ofence = '{';
			goto Back;
		case ']':
			fmatch->ofence = '[';
			goto Back;
		case '>':
			fmatch->ofence = '<';
Back:
			fmatch->scanDirec = Backward;
			break;
		default:
			return NULL;
		}
	return point;
	}

// Find a matching fence and either move point to it, or create a region.  Scan for the matching fence forward or backward,
// depending on the source fence character.  Use "fence" as the source fence if it's value is > 0; otherwise, use character at
// point.  If the fence is found: if reg is not NULL, set point in the Region object to original point, set line count to zero
// (not used), set size to the number of characters traversed + 1 as a positive (point moved forward) or negative (point moved
// backward) amount, and restore point position; otherwise, set point to the matching fence position.  If the fence is not
// found: restore point position and return Failure, with no message.
int otherfence(short fence,Region *reg) {
	int flevel;		// Current fence level.
	short c;		// Current character in scan.
	long chunk;
	int n;
	Point *point;
	FenceMatch fmatch;

	// Set up control variables.
	if((point = fenceprep(fence,&fmatch)) == NULL)
		goto Bust;

	// Scan until we find matching fence, or hit a buffer boundary.
	chunk = 0;
	flevel = 1;
	n = (fmatch.scanDirec == Forward) ? 1 : -1;
	while(flevel > 0) {
		(void) movech(n);
		++chunk;
		if(point->off < point->lnp->l_used) {
			c = point->lnp->l_text[point->off];
			if(c == fmatch.fence)
				++flevel;
			else if(c == fmatch.ofence)
				--flevel;
			}
		if(boundary(point,fmatch.scanDirec))
			break;
		}

	// If flevel is zero, we have a match.
	if(flevel == 0) {
		// Create region and restore point, or keep current point position.
		if(reg != NULL) {
			*point = fmatch.opoint;
			if(fmatch.scanDirec == Forward) {
				reg->r_point = *point;
				reg->r_size = chunk + 1;
				}
			else {
				(void) movech(1);
				reg->r_point = *point;
				(void) movech(-1);
				reg->r_size = -(chunk + 1);
				}
			reg->r_linect = 0;
			}
		else
			si.curwin->w_flags |= WFMove;
		return rc.status;
		}

	// Matching fence not found: restore previous position.
	*point = fmatch.opoint;
Bust:
	// Beep if interactive.
	if(!(si.opflags & OpScript))
		ttbeep();
	return rcset(Failure,0,NULL);
	}

// Match opening or closing fence against its partner, and if in the current window, briefly light the cursor there.  If the
// fence is not found, return Failure with no message.
int fenceMatch(short fence,bool dobeep) {
	int flevel;		// Current fence level.
	short c;		// Current character in scan.
	int n,row,nrows;
	ushort mvflag = 0;
	Point *point;
	FenceMatch fmatch;

	// Skip this if executing a keyboard macro.
	if(kmacro.km_state == KMPlay)
		return rc.status;

	// Set up control variables.
	if((point = fenceprep(fence,&fmatch)) == NULL)
		goto Bust;
	row = getwpos(si.curwin);		// Row in window containing point.
	nrows = si.curwin->w_nrows;		// Number of rows in window.
	flevel = 1;
	n = (fmatch.scanDirec == Forward) ? 1 : -1;

	// Scan until we find matching fence, or move out of the window.
	while(flevel > 0) {
		if(movech(n) == NotFound)
			break;
		if(point->off < point->lnp->l_used) {
			c = point->lnp->l_text[point->off];
			if(c == fmatch.fence)
				++flevel;
			else if(c == fmatch.ofence)
				--flevel;
			}
		else if(fmatch.scanDirec == Backward) {
			if(--row == 0)
				break;
			}
		else if(row++ == nrows)
			break;
		}

	// If flevel is zero, we have a match -- display the sucker.
	if(flevel == 0) {
		mvflag = si.curwin->w_flags & WFMove;
		if(update(INT_MIN) != Success)
			return rc.status;
		cpause(si.fencepause);
		}

	// Restore the previous position.
	*point = fmatch.opoint;
	if(mvflag)
		si.curwin->w_flags |= WFMove;
	if(flevel == 0)
		return rc.status;
Bust:
	// Beep if interactive.
	if(dobeep && !(si.opflags & OpScript))
		ttbeep();
	return rcset(Failure,0,NULL);
	}

// Get source fence character from command line argument or user input if non-default n.  Return status.
static int getfence(Datum *rval,int n,Datum **argv,short *fence) {

	// Get source fence character...
	if(n == INT_MIN)
		*fence = 0;
	else if(si.opflags & OpScript) {
		if(!charval(argv[0]))
			return rc.status;
		*fence = argv[0]->u.d_int;
		}
	else if(terminp(rval,text179,ArgNotNull1 | ArgNil1,Term_OneChar,NULL) == Success && rval->d_type != dat_nil)
			// "Fence char"
		*fence = rval->u.d_int;

	return rc.status;
	}

// Move point to a matching fence if possible, and return Boolean result.
int gotoFence(Datum *rval,int n,Datum **argv) {
	short fence;
	bool result = true;

	// Get source fence character...
	if(getfence(rval,n,argv,&fence) != Success)
		return rc.status;

	// go to other fence...
	if(otherfence(fence,NULL) != Success) {
		if(rc.status < Failure)
			return rc.status;
		rcclear(0);
		result = false;
		}

	// and return result.	
	dsetbool(result,rval);
	return rc.status;
	}

// Find matching fence and, if in the current window, briefly light the cursor there.
int showFence(Datum *rval,int n,Datum **argv) {
	short fence;

	// Get source fence character...
	if(getfence(rval,n,argv,&fence) != Success)
		return rc.status;

	// and highlight other fence if possible.
	return fenceMatch(fence,true);
	}

// Move point backward or forward n tab stops.  Return -1 if invalid move; otherwise, new offset in current line.
int tabstop(int n) {
	int off,len,col,tabsize,curstop,targstop;
	Point *point = &si.curwin->w_face.wf_point;

	// Check for "do nothing" cases.
	if(n == 0 || (len = point->lnp->l_used) == 0 || ((off = point->off) == 0 && n < 0) || (off == len && n > 0))
		return -1;

	// Calculate target tab stop column.
	tabsize = (si.stabsize == 0) ? si.htabsize : si.stabsize;
	curstop = (col = getcol(NULL,false)) / tabsize;
	if(col % tabsize != 0 && n < 0)
		++curstop;
	return (targstop = curstop + n) <= 0 ? 0 : getgoal(point->lnp,targstop * tabsize);
	}

// Move point backward or forward n tab stops.
int bftab(int n) {
	int off;

	if((off = tabstop(n)) >= 0)
		si.curwin->w_face.wf_point.off = off;
	return rc.status;
	}
