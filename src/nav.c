// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// nav.c	Basic movement (navigation) functions for MightEMacs.
//
// These routines move the point around on the screen.  The display code always updates the point location, so only functions
// that adjust the top line in the window and invalidate the framing pose any difficulty.

#include "std.h"
#include <ctype.h>
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "search.h"

/*** Local declarations ***/

static int targCol;			// Goal column for vertical line movements.

// Fence-matching parameters.
typedef struct {
	short fence;			// Source fence character.
	short otherFence;		// Other (matching) fence character.
	int scanDirec;			// Scan direction (-1 or +1).
	Point origPoint;		// Original point position.
	} FenceMatch;

// Return true if given point position is at a boundary (beginning or end of buffer), otherwise false.
int boundary(Point *pPoint, int dir) {

	return (dir == Forward) ? bufEnd(pPoint) : bufBegin(pPoint);
	}

// Return true if the character at point is a character that is considered to be part of a word.
bool inWord(void) {
	Point *pPoint = &sess.cur.pFace->point;
	return wordChar[(pPoint->offset == pPoint->pLine->used ? '\n' : pPoint->pLine->text[pPoint->offset])];
	}

// Move point forward (+n) or backward (-n) by abs(n) characters in current edit buffer.  Return NotFound (bypassing rsset()) if
// move would go out of the buffer.  Set the move flag if point moves to a different line.  Return status.
int moveChar(int n) {

	if(n != 0) {
		Point *pPoint = &sess.edit.pFace->point;

		if(n > 0) {
			do {
				if(pPoint->offset == pPoint->pLine->used) {
					if(pPoint->pLine->next == NULL)
						return NotFound;
					pPoint->pLine = pPoint->pLine->next;
					pPoint->offset = 0;
					if(sess.edit.pWind != NULL)
						sess.edit.pWind->flags |= WFMove;
					}
				else
					++pPoint->offset;
				} while(--n > 0);

			}
		else {
			do {
				if(pPoint->offset == 0) {
					if(pPoint->pLine == sess.edit.pBuf->pFirstLine)
						return NotFound;
					pPoint->pLine = pPoint->pLine->prev;
					pPoint->offset = pPoint->pLine->used;
					if(sess.edit.pWind != NULL)
						sess.edit.pWind->flags |= WFMove;
					}
				else
					--pPoint->offset;
				} while(++n < 0);
			}
		}

	return sess.rtn.status;
	}

// Move point forward by n characters.  If n is negative, call backChar() to actually do the move.  Set pRtnVal to false and
// return NotFound (bypassing rsset()) if move would go out of the buffer; otherwise, set pRtnVal to true and return status.
int forwChar(Datum *pRtnVal, int n, Datum **args) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backChar(pRtnVal, -n, NULL);

	dsetbool((status = moveChar(n)) != NotFound, pRtnVal);
	return status;
	}

// Move point backward by n characters.  If n is negative, call forwChar() to actually do the move.  Set pRtnVal to false and
// return NotFound (bypassing rsset()) if move would go out of the buffer; otherwise, set pRtnVal to true and return status.
int backChar(Datum *pRtnVal, int n, Datum **args) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwChar(pRtnVal, -n, NULL);

	dsetbool((status = moveChar(-n)) != NotFound, pRtnVal);
	return status;
	}

// Move point forward (+n) or backward (-n) by abs(n) words.  All of the motion is performed by the moveChar() routine.  Return
// NotFound (bypassing rsset()) if move would go out of the buffer; otherwise, return status.
int moveWord(int n) {

	if(n != 0) {
		if(n > 0) {
			do {
				// Scan through the current word.
				while(inWord())
					if(moveChar(1) == NotFound)
						return NotFound;

				// Scan through the white space.
				while(!inWord())
					if(moveChar(1) == NotFound)
						return NotFound;
				} while(--n > 0);
			}
		else {
			if(moveChar(-1) == NotFound)
				return NotFound;
			do {
				// Back up through the white space.
				while(!inWord())
					if(moveChar(-1) == NotFound)
						return NotFound;

				// Back up through the current word.
				while(inWord())
					if(moveChar(-1) == NotFound) {
						if(n == -1)
							return sess.rtn.status;	// In word at beginning of buffer... no error.
						return NotFound;
						}
				} while(++n < 0);

			// Move to beginning of current word.
			return moveChar(1);
			}
		}

	return sess.rtn.status;
	}

// Move point forward one word on current line.  Return NotFound (bypassing rsset()) if move would go past end of line;
// otherwise, return status.
static int forwWord0(void) {
	Point *pPoint = &sess.cur.pFace->point;

	// Scan through the current word.
	while(inWord())
		if(++pPoint->offset == pPoint->pLine->used)
			return NotFound;

	// Scan through the white space.
	while(!inWord())
		if(++pPoint->offset == pPoint->pLine->used)
			return NotFound;

	return sess.rtn.status;
	}

// Move point backward one word on current line.  Return NotFound (bypassing rsset()) if move would go past beginning of line;
// otherwise, return status.
static int backWord0(void) {
	Point *pPoint = &sess.cur.pFace->point;

	if(pPoint->offset == 0)
		return NotFound;
	--pPoint->offset;

	// Back up through the white space.
	while(!inWord()) {
		if(pPoint->offset == 0)
			return NotFound;
		--pPoint->offset;
		}

	// Back up through the current word.
	while(inWord()) {
		if(pPoint->offset == 0)
			return sess.rtn.status;	// In word at beginning of line... no error.
		--pPoint->offset;
		}

	// Move to beginning of current word.
	++pPoint->offset;
	return sess.rtn.status;
	}

// Move point forward by n words.  All of the motion is performed by moveChar().  Set pRtnVal to false if move would go out of
// the buffer, otherwise true.  Return status.
int forwWord(Datum *pRtnVal, int n, Datum **args) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(pRtnVal, -n, NULL);

	dsetbool(moveWord(n) != NotFound, pRtnVal);
	return sess.rtn.status;
	}

// Move point backward by n words.  All of the motion is performed by the moveChar() routine.  Set pRtnVal to false if move
// would go out of the buffer, otherwise true.  Return status.
int backWord(Datum *pRtnVal, int n, Datum **args) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwWord(pRtnVal, -n, NULL);

	dsetbool(moveWord(-n) != NotFound, pRtnVal);
	return sess.rtn.status;
	}

// Move forward to the end of the nth next word.  Set pRtnVal to false if move would go out of the buffer, otherwise true.
// Return status.
int endWord(Datum *pRtnVal, int n, Datum **args) {
	bool b = true;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backWord(pRtnVal, -n, NULL);

	do {
		// Scan through the white space.
		while(!inWord())
			if(moveChar(1) == NotFound)
				goto Fail;

		// Scan through the current word.
		while(inWord())
			if(moveChar(1) == NotFound) {
				if(n == 1)
					goto Retn;
				goto Fail;
				}
		} while(--n > 0);
	goto Retn;
Fail:
	b = false;
Retn:
	dsetbool(b, pRtnVal);
	return sess.rtn.status;
	}

// Get a word from current line (or possibly an adjacent line) and return it, or nil if not found.  Return status.
int getWord(Datum *pRtnVal, int n, Datum **args) {
	Point *pPoint = &sess.cur.pFace->point;
	int offset = pPoint->offset;						// Save current offset in case of error.

	if(n == INT_MIN)
		n = 0;
	if(inWord()) {
		// Starting in a word...
		if(n <= 0) {
			// Move to beginning of current word if n is zero.
			if(n == 0)
				for(;;) {
					if(pPoint->offset == 0)
						break;			// We're there.
					--pPoint->offset;
					if(!inWord()) {
						++pPoint->offset;
						break;
						}
					}
			if(n < 0) {
				// Move to beginning of previous word.
				if(n == -1) {
					if(backWord0() == NotFound)	// Hit BOL.
						goto RestorePt;
					}
				else if(moveWord(-1) == NotFound)		// Hit BOB.
					goto RestorePt;
				}
			}
		else {
			// Move to beginning of next word.
			if(n == 1) {
				if(forwWord0() == NotFound)		// Hit EOL.
					goto RestorePt;
				}
			else if(moveWord(1) == NotFound)			// Hit EOB.
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
				if(backWord0() == NotFound)		// Hit BOL.
					goto RestorePt;
				}
			else if(moveWord(-1) == NotFound)			// Hit BOB.
				goto RestorePt;
			}
		else {
			// Move to beginning of next word.
			if(n == 1) {
				if(forwWord0() == NotFound)		// Hit EOL.
					goto RestorePt;
				}
			else if(moveWord(1) == NotFound)			// Hit EOB.
				goto RestorePt;
			}
		}

	// Found beginning of target word... save offset and move to end of it.
	offset = pPoint->offset;
	do {
		if(++pPoint->offset == pPoint->pLine->used)			// Hit EOL.
			break;
		} while(inWord());

	// Save text and return.
	if(dsetsubstr(pPoint->pLine->text + offset, (pPoint->pLine->text + pPoint->offset) - (pPoint->pLine->text + offset),
	 pRtnVal) != 0)
		(void) libfail();
	else if(n < 0)
		pPoint->offset = offset;
	goto Retn;
RestorePt:
	pPoint->offset = offset;
NilRtn:
	dsetnil(pRtnVal);
Retn:
	return sess.rtn.status;
	}

// Return best choice for an offset in given line, considering given target column.
static int getGoal(Line *pLine, int targ) {
	int col, offset;

	col = offset = 0;

	// Find position in pLine which most closely matches goal column, or end of line if pLine is too short.
	while(offset < pLine->used) {
		if((col = newCol(pLine->text[offset], col)) > targ)
			break;
		++offset;
		}
	return offset;
	}

// Move forward (+n) or backward (-n) by abs(n) full lines.  The last command controls how the goal column is set.  Return
// NotFound (bypassing rsset()) if move would go out of the buffer.
int moveLine(int n) {
	int status = sess.rtn.status;

	if(n != 0) {
		Line *pLine;
		Point *pPoint = &sess.cur.pFace->point;

		if(n > 0) {
			// If we are on the last line as we start, move point to end of line and fail the command.
			if(pPoint->pLine->next == NULL) {
				pPoint->offset = pPoint->pLine->used;
				return NotFound;
				}

			// If the last command was not a line move, reset the goal column.
			if(!(keyEntry.prevFlags & SF_VertMove))
				targCol = getCol(NULL, false);

			// Flag this command as a line move...
			keyEntry.curFlags |= SF_VertMove;

			// and move point down.
			pLine = pPoint->pLine;
			do {
				if(pLine->next == NULL) {
					status = NotFound;
					pPoint->offset = pLine->used;
					goto BufBound;
					}
				pLine = pLine->next;
				} while(--n > 0);
			}
		else {
			// If we are on the first line as we start, move point to beginning of line and fail the command.
			if(pPoint->pLine == sess.cur.pBuf->pFirstLine) {
				pPoint->offset = 0;
				return NotFound;
				}

			// If the last command was not a line move, reset the goal column.
			if(!(keyEntry.prevFlags & SF_VertMove))
				targCol = getCol(NULL, false);

			// Flag this command as a line move...
			keyEntry.curFlags |= SF_VertMove;

			// and move point up.
			pLine = pPoint->pLine;
			do {
				if(pLine == sess.cur.pBuf->pFirstLine) {
					status = NotFound;
					pPoint->offset = 0;
					goto BufBound;
					}
				pLine = pLine->prev;
				} while(++n < 0);
			}

		// Reset the current position.
		pPoint->offset = getGoal(pLine, targCol);
BufBound:
		pPoint->pLine = pLine;
		sess.cur.pWind->flags |= WFMove;
		}

	return status;
	}

// Move forward by n full lines.  If the number of lines to move is negative, call the backward line function to actually do
// the move.  Set pRtnVal to false and return NotFound (bypassing rsset()) if move would go out of the buffer; otherwise, return
// true.
int forwLine(Datum *pRtnVal, int n, Datum **args) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return backLine(pRtnVal, -n, NULL);

	dsetbool((status = moveLine(n)) != NotFound, pRtnVal);
	return status;
	}

// This function is like "forwLine", but goes backward.  The scheme is exactly the same.
int backLine(Datum *pRtnVal, int n, Datum **args) {
	int status;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return forwLine(pRtnVal, -n, NULL);

	dsetbool((status = moveLine(-n)) != NotFound, pRtnVal);
	return status;
	}

// Move point to the [-]nth line and clear the "line move" flag (for subsequent point move).  Set pRtnVal to nil and return
// NotFound (bypassing rsset()) if move would go out of the buffer.
static int moveToLine(Datum *pRtnVal, int n) {
	int status = Success;

	if(n > 1)
		status = forwLine(pRtnVal, n - 1, NULL);
	else if(n < 0 && n != INT_MIN)
		status = backLine(pRtnVal, -n, NULL);
	keyEntry.curFlags &= ~SF_VertMove;

	return (status == NotFound) ? NotFound : sess.rtn.status;
	}

// Move point to the beginning of text (first non-whitespace character) on the current line.  Trivial.  No errors.
int beginTxt(void) {
	int c, offset, used;
	Point *pPoint;
	Line *pLine;

	used = (pLine = (pPoint = &sess.cur.pFace->point)->pLine)->used;
	for(offset = 0; offset < used; ++offset) {
		c = pLine->text[offset];
		if(c != ' ' && c != '\t')
			break;
		}
	pPoint->offset = offset;

	return sess.rtn.status;
	}

// Move point to the beginning of text (first non-whitespace character) on the [-]nth line.  Set pRtnVal to nil and return
// NotFound (bypassing rsset()) if move would go out of the buffer.
int beginText(Datum *pRtnVal, int n, Datum **args) {

	if(moveToLine(pRtnVal, n) == NotFound)
		return NotFound;
	return beginTxt();
	}

// Move point to beginning ("end" is false) or end ("end" is true) of white space at the current position.  Trivial.  No errors.
int spanWhite(bool end) {
	int offset;
	short c;
	Point *pPoint;
	Line *pLine;

	pLine = (pPoint = &sess.cur.pFace->point)->pLine;
	offset = pPoint->offset;
	if(end) {
		while(offset < pLine->used && (isspace(c = pLine->text[offset]) || c == '\0'))
			++offset;
		}
	else {
		while(offset > 0 && (isspace(c = pLine->text[offset - 1]) || c == '\0'))
			--offset;
		}
	pPoint->offset = offset;

	return sess.rtn.status;
	}

// Move point to the beginning ("end" is false) or end ("end" is true) of the [-]nth line.  Set pRtnVal to nil and return
// NotFound (bypassing rsset()) if move would go out of the buffer.
int beginEndLine(Datum *pRtnVal, int n, bool end) {

	if(moveToLine(pRtnVal, n) == NotFound)
		return NotFound;
	sess.cur.pFace->point.offset = end ? sess.cur.pFace->point.pLine->used : 0;
	return sess.rtn.status;
	}

// Go to a line via bufOp() call.  Return status.
int goLine(Datum *pDatum, int n, int line) {

	if(line < 0)
		return rsset(Failure, 0, text39, text143, line, 0);
				// "%s (%d) must be %d or greater", "Line number"
	if((sess.opFlags & OpScript) && n != INT_MIN && (!haveSym(s_comma, true) || getSym() != Success))
		return sess.rtn.status;

	// Go to line.
	return bufOp(pDatum, n, text229 + 2, BO_GotoLine, line);
		// ", in"
	}

// Move to a particular line, or end of buffer if line number is zero.  If n argument given, move point in specified buffer,
// otherwise current one.  Return status.
int gotoLine(Datum *pRtnVal, int n, Datum **args) {
	Datum *pDatum;

	// Get line number and validate it.
	if(dnewtrack(&pDatum) != 0)
		return libfail();
	if(sess.opFlags & OpScript) {
		if(funcArg(pDatum, ArgFirst | ArgInt1) != Success)
			return sess.rtn.status;
		}
	else {
		char workBuf[strlen(text7) + strlen(text205) + 2];
		sprintf(workBuf, "%s %s", text7, text205);
				// "Go to", "line"
		if(termInp(pDatum, workBuf, ArgNil1, 0, NULL) != Success || disnil(pDatum) || toInt(pDatum) != Success)
			return sess.rtn.status;
		}
	return goLine(pDatum, n, pDatum->u.intNum);
	}

// Move point in multi-character increments left or right on the current line.
int traverseLine(Datum *pRtnVal, int n, Datum **args) {
	static bool lastWasForw;		// true if last invocation was forward motion.
	int newCol;
	bool moveForw = true;
	Point *pPoint = &sess.cur.pFace->point;

	if(pPoint->pLine->used > 0) {					// If not blank line...
		if(n == 0) {						// zero argument?
			newCol = term.cols - 2;			// Yes, move to right edge of unshifted display.
			if(modeSet(MdIdxHScrl, NULL)) {
				if(sess.cur.pFace->firstCol > 0) {
					sess.cur.pFace->firstCol = 0;
					sess.cur.pWind->flags |= (WFHard | WFMode);
					}
				}
			else if(sess.cur.pScrn->firstCol > 0) {
				sess.cur.pScrn->firstCol = 0;
				sess.cur.pWind->flags |= (WFHard | WFMode);
				}
			}
		else {							// No, save point offset and get column positions.
			int jump = sess.travJumpCols;
			int curCol = getCol(NULL, false);
			Point workPoint;
			workPoint.pLine = pPoint->pLine;
			workPoint.offset = pPoint->pLine->used;
			int endCol = getCol(&workPoint, false);
			if(endCol <= sess.travJumpCols)			// Line too short?
				return sess.rtn.status;			// Yep, exit.

			// Figure out initial direction to move (forward or backward), ignoring any argument (for now).
			if(keyEntry.prevFlags & SF_Trav)		// Last command was a line traversal (this routine)...
				moveForw = lastWasForw;			// so repeat that direction.
			else if(curCol > (int) (endCol * 0.57))		// If a bit past mid-line...
				moveForw = false;			// go backward.

			// Check bounds and reverse direction if needed.
			if(moveForw && curCol > endCol - sess.travJumpCols)
				moveForw = false;
			else if(!moveForw && curCol < sess.travJumpCols)
				moveForw = true;

			// Goose it or reverse direction if any non-zero argument.
			if(n != INT_MIN) {				// If argument...
				if((n > 0) == moveForw)			// and same direction as calculated...
					jump = sess.travJumpCols * 4;	// then boost distance (4x).
				else
					moveForw = !moveForw;		// Otherwise, reverse direction.
				}

			// Move "jump" columns.
			newCol = curCol + (moveForw ? jump : -jump);
			}

		// Move point and save results.
		(void) setPointCol(newCol);
		lastWasForw = moveForw;
		keyEntry.curFlags |= SF_Trav;
		}
	return sess.rtn.status;
	}

// Scroll current window backward or forward n pages, less the current sess.overlap.  If n is negative, scroll half a page.
static int backForwPage(Datum *pRtnVal, int n, int direc) {

	if(n != 0) {
		Face *pFace = &sess.cur.pWind->face;
		bool fullPage = true;
		int pageSize = 1;				// Full page.
		if(n == INT_MIN)
			n = 1;
		else if(n < 0) {
			pageSize = 2;				// Half page.
			n = 1;
			fullPage = false;
			}
		pageSize = sess.cur.pWind->rows / pageSize - sess.overlap;	// Default number of rows per page to scroll.
		if(pageSize <= 0)				// Tiny window or large sess.overlap.
			pageSize = 1;
		n *= pageSize * direc;
		windNewTop(sess.cur.pWind, NULL, n);
		if(fullPage || !pointInWind()) {		// If scrolled full page or point no longer in window...
			pFace->point.pLine = pFace->pTopLine;	// set point to top line.
			pFace->point.offset = 0;
			}
		sess.cur.pWind->flags |= WFMove;
		}

	return sess.rtn.status;
	}

// Scroll current window forward n pages.
int forwPage(Datum *pRtnVal, int n, Datum **args) {

	return backForwPage(pRtnVal, n, 1);
	}

// Scroll current window backward n pages.
int backPage(Datum *pRtnVal, int n, Datum **args) {

	return backForwPage(pRtnVal, n, -1);
	}

// Move point to given position in current window and set "move" flag if needed.
void movePoint(Point *pPoint) {
	Point *pPoint1 = &sess.cur.pFace->point;
	if(pPoint->pLine != pPoint1->pLine)
		sess.cur.pWind->flags |= WFMove;
	*pPoint1 = *pPoint;
	}

// Get row offset of point in given window and return it.  If point is not in the window, return zero.
int getWindPos(EWindow *pWind) {
	Line *pLine;
	int row = 1;
	Face *pFace = &pWind->face;

	// Search down to the line we want...
	pLine = pFace->pTopLine;
	while(pLine != pFace->point.pLine)
		if(row++ == pWind->rows || (pLine = pLine->next) == NULL)
			return 0;

	// and return the value.
	return row;
	}

// Find given mark in current buffer and return in *ppMark.  If found, return it if it's active (visible) or MKViz not set;
// otherwise, if MKQuery is set, set *ppMark to NULL and return; otherwise, return an error.  If not found, create the mark
// (and assume that the caller will set the mark's point) if MKCreate set; otherwise, set *ppMark to NULL and return if
// MKQuery or MKWind set or script mode and MKExist set (called from delMark() function... no error); otherwise,
// return an error.  Return status.
int findBufMark(ushort id, Mark **ppMark, ushort flags) {
	Mark *pMark0 = NULL;
	Mark *pMark1 = &sess.cur.pBuf->markHdr;
	do {
		if(pMark1->id == id) {

			// Found it.  Return it if appropriate.
			if(pMark1->point.offset >= 0 || !(flags & MKViz))
				goto Found;
			if(flags & MKQuery) {
				pMark1 = NULL;
				goto Found;
				}
			goto ErrRetn;
			}
		pMark0 = pMark1;
		} while((pMark1 = pMark1->next) != NULL);

	// Not found.  Error if required.
	if(!(flags & MKCreate)) {
		if((flags & (MKQuery | MKWind)) || ((flags & MKExist) && (sess.opFlags & OpScript)))
			goto Found;
ErrRetn:
		return rsset(Failure, RSTermAttr, text11, id);
				// "No mark ~u%c~U in this buffer"
		}

	// Mark was not required to already exist... create it.
	if((pMark1 = (Mark *) malloc(sizeof(Mark))) == NULL)
		return rsset(Panic, 0, text94, "findBufMark");
			// "%s(): Out of memory!"
	pMark1->next = NULL;
	pMark1->id = id;
	pMark0->next = pMark1;
Found:
	*ppMark = pMark1;
	return sess.rtn.status;
	}

// Set given mark to point in given window.
void setWindMark(Mark *pMark, EWindow *pWind) {

	pMark->point = pWind->face.point;
	pMark->reframeRow = getWindPos(pWind);
	}

// Check if given character is a valid mark.  Return true if so; otherwise, set an error and return false.
bool validMark(Datum *pDatum) {

	if(pDatum->u.intNum >= '!' && pDatum->u.intNum < '~')
		return true;
	(void) rsset(Failure, 0, "%s'%s'%s", text9, vizc(pDatum->u.intNum, VizSpace), text345);
					//  "Mark ", " invalid"
	return false;
	}

// Get a mark and return it in *ppMark.  If default n and MKAutoR or MKAutoW flag set, return mark RegionMark or WorkMark
// (creating the latter if necessary); otherwise, get a key with no default (and set *ppMark to NULL if nothing was entered
// interactively).  Return status.
static int getMark(const char *basePrompt, int n, ushort flags, Mark **ppMark) {

	// Check n.
	if(n == INT_MIN && (flags & (MKAutoR | MKAutoW)))
		return findBufMark((flags & MKAutoW) ? WorkMark : RegionMark, ppMark, flags);

	// Get a key.
	ushort id;
	Datum *pDatum;
	if(dnewtrack(&pDatum) != 0)
		goto LibFail;
	if(sess.opFlags & OpScript) {
		if(funcArg(pDatum, ArgFirst | ArgInt1) != Success || !validMark(pDatum))
			return sess.rtn.status;
		id = pDatum->u.intNum;
		}
	else {
		char promptBuf[term.cols];

		if(flags & (MKViz | MKExist)) {
			Mark *pMark;
			char *delim;
			DFab prompt;
			char workBuf[5];

			// Build prompt with existing marks in parentheses.
			if(dopentrack(&prompt) != 0 || dputf(&prompt, 0, text346, basePrompt) != 0 ||
									// "%s mark"
			 dputc(' ', &prompt, 0) != 0)
				goto LibFail;
			strcpy(delim = workBuf, "(~#u");
			pMark = &sess.cur.pBuf->markHdr;
			do {
				if(pMark->id < '~' && (pMark->point.offset >= 0 || (flags & MKExist)) &&
				 (pMark->id != RegionMark || !(flags & MKExist))) {
					if(dputs(delim, &prompt, 0) != 0 || dputc(pMark->id, &prompt, 0) != 0)
						goto LibFail;
					delim = " ";
					}
				} while((pMark = pMark->next) != NULL);

			// Error if delMark call and no mark other than RegionMark found (which can't be deleted).
			if(*delim == '(')
				return rsset(Failure, RSNoFormat, text361);
					// "No mark found to delete"

			if(dputs("~U)", &prompt, 0) != 0 || dclose(&prompt, FabStr) != 0)
				goto LibFail;

			// Fit the prompt in roughly 90% of the terminal width.
			strfit(promptBuf, (size_t) term.cols * 90 / 100, prompt.pDatum->str, 0);
			}
		else {
			// Build basic prompt.
			sprintf(promptBuf, text346, basePrompt);
				// "%s mark"
			}
		if(termInp(pDatum, promptBuf, ArgNotNull1 | ArgNil1, Term_OneChar | Term_Attr, NULL) != Success)
			return sess.rtn.status;
		if(disnil(pDatum)) {
			*ppMark = NULL;
			return sess.rtn.status;
			}
		if(pDatum->u.intNum < '!' || pDatum->u.intNum >= '~')
			return rsset(Failure, 0, "%s'%s'%s", text9, vizc(pDatum->u.intNum, VizSpace), text345);
						//  "Mark ", " invalid"
		id = pDatum->u.intNum;
		}

	// Success.  Return mark.
	return findBufMark(id, ppMark, flags);
LibFail:
	return libfail();
	}

// Set a mark in the current buffer to point and return status.  If default n, use RegionMark; otherwise, get a mark with no
// default.
int setMark(Datum *pRtnVal, int n, Datum **args) {
	Mark *pMark;

	// Make sure mark is valid.
	if(getMark(text64, n, MKAutoR | MKCreate, &pMark) != Success || pMark == NULL)
			// "Set"
		return sess.rtn.status;

	// Set mark to point and return.
	setWindMark(pMark, sess.cur.pWind);
	return rsset(Success, RSTermAttr, "%s~u%c~U %s", text9, pMark->id, text350);
		// "Mark ", "set"
	}

// Delete given mark.  Return status.
static int delMark1(Mark *pMark) {

	if(pMark->id == RegionMark)
		return rsset(Failure, RSTermAttr, text352, RegionMark);
			// "Cannot delete mark ~u%c~U"

	// It's a go... delete it.
	Mark *pMark0 = &sess.cur.pBuf->markHdr;
	ushort id = pMark->id;
	for(;;) {
		if(pMark0->next == pMark)
			break;
		pMark0 = pMark0->next;
		}
	pMark0->next = pMark->next;
	free((void *) pMark);
	return rsset(Success, RSHigh | RSTermAttr, "%s~u%c~U %s", text9, id, text10);
							// "Mark ", "deleted"
	}

// Remove a mark in the current buffer.  If non-default n, remove all marks.  No error if script mode and mark not found.
int delMark(Datum *pRtnVal, int n, Datum **args) {

	// Delete all?
	if(n != INT_MIN) {
		delBufMarks(sess.cur.pBuf, 0);
		(void) rsset(Success, RSHigh | RSNoFormat, text351);
			// "All marks deleted"
		}
	else {
		Mark *pMark;

		// Make sure mark is valid.
		if(getMark(text26, n, MKHard | MKExist, &pMark) != Success || pMark == NULL)
				// "Delete"
			return sess.rtn.status;
		(void) delMark1(pMark);
		}

	return sess.rtn.status;
	}

// Go to given mark in current buffer, but don't force reframe if mark is already in the window unless forceReframe is true.
static void goMark(Mark *pMark, bool forceReframe) {
	Line *pLine = sess.cur.pFace->point.pLine;		// Remember current line.
	sess.cur.pFace->point = pMark->point;		// Set point to mark.
	if(forceReframe)
		goto Reframe;
	if(pMark->point.pLine != pLine) {				// If still on same line, do nothing else.
		if(lineInWind(sess.cur.pWind, pMark->point.pLine))	// Otherwise, set minimal window-update flag.
			sess.cur.pWind->flags |= WFMove;
		else {
Reframe:
			sess.cur.pWind->reframeRow = pMark->reframeRow;
			sess.cur.pWind->flags |= WFReframe;
			}
		}
	}

// Swap a mark with point, given mark pointer.  Force a window reframe if forceReframe is true.  Return status.
static int swapMrk(Mark *pMark, bool forceReframe) {
	Point oldPoint = sess.cur.pFace->point;
	short oldRow = getWindPos(sess.cur.pWind);
	goMark(pMark, forceReframe);
	pMark->point = oldPoint;
	pMark->reframeRow = oldRow;
	return sess.rtn.status;
	}

// Swap the values of point and a mark in the current window.  Get a mark with no default and force a window reframe if n >= 0.
// Return status.
int swapMark(Datum *pRtnVal, int n, Datum **args) {
	Mark *pMark;

	// Make sure mark is valid.
	if(getMark(text347, n, MKHard | MKViz, &pMark) != Success || pMark == NULL)
			// "Swap point with"
		return sess.rtn.status;

	// Swap point and the mark.
	return swapMrk(pMark, n >= 0);
	}

// Swap a mark with point, given mark id.  Return status.
int swapMarkId(ushort id) {
	Mark *pMark;

	if(findBufMark(id, &pMark, MKViz) == Success)
		(void) swapMrk(pMark, false);
	return sess.rtn.status;
	}

// Go to a mark in the current window.  Get a mark with no default, move point, then delete mark if n <= 0.  Also force a window
// reframe if n >= 0.  Return status.
int gotoMark(Datum *pRtnVal, int n, Datum **args) {
	Mark *pMark;

	// Make sure mark is valid.
	if(getMark(text7, n, MKHard | MKViz, &pMark) != Success || pMark == NULL)
			// "Go to"
		return sess.rtn.status;

	// Set point to the mark.
	goMark(pMark, n >= 0);

	// Delete mark if applicable.
	if(n <= 0 && n != INT_MIN)
		(void) delMark1(pMark);

	return sess.rtn.status;
	}

// Mark current buffer from beginning to end and preserve current position in a mark.  If default n, use WorkMark; otherwise,
// get a mark with no default.  Return status.
int markBuf(Datum *pRtnVal, int n, Datum **args) {
	Mark *pMark;

	// Make sure mark is valid.
	if(getMark(text348, n, MKAutoW | MKCreate, &pMark) != Success || pMark == NULL)
			// "Save point in"
		return sess.rtn.status;

	// Mark whole buffer.  If RegionMark was specified for saving point, user is out of luck (it will be overwritten).
	setWindMark(pMark, sess.cur.pWind);					// Preserve current position.
	(void) execCmdFunc(pRtnVal, INT_MIN, cmdFuncTable + cf_beginBuf, 0, 0);	// Move to beginning of buffer.
	setWindMark(&sess.cur.pBuf->markHdr, sess.cur.pWind);			// Set to mark RegionMark.
	(void) execCmdFunc(pRtnVal, INT_MIN, cmdFuncTable + cf_endBuf, 0, 0);	// Move to end of buffer.
	if(sess.rtn.status == Success)
		rsclear(0);
	return (pMark->id == RegionMark) ? sess.rtn.status : rsset(Success, RSTermAttr, text233, pMark->id);
							// "Mark ~u%c~U set to previous position"
	}

// Prepare for an "other fence" scan, given source fence character and pointer to FenceMatch object.  If valid source fence
// found, set object parameters and return current point pointer (Point *); otherwise, return NULL.
static Point *fencePrep(short fence, FenceMatch *pFenceMatch) {
	Point *pPoint = &sess.cur.pFace->point;

	// Get the source fence character.
	if(fence == 0) {
		if(pPoint->offset == pPoint->pLine->used)
			return NULL;
		fence = pPoint->pLine->text[pPoint->offset];
		}
	pFenceMatch->fence = fence;
	pFenceMatch->origPoint = *pPoint;			// Original point position.

	// Determine matching fence.
	switch(fence) {
		case '(':
			pFenceMatch->otherFence = ')';
			goto Forw;
		case '{':
			pFenceMatch->otherFence = '}';
			goto Forw;
		case '[':
			pFenceMatch->otherFence = ']';
			goto Forw;
		case '<':
			pFenceMatch->otherFence = '>';
Forw:
			pFenceMatch->scanDirec = Forward;
			break;
		case ')':
			pFenceMatch->otherFence = '(';
			goto Back;
		case '}':
			pFenceMatch->otherFence = '{';
			goto Back;
		case ']':
			pFenceMatch->otherFence = '[';
			goto Back;
		case '>':
			pFenceMatch->otherFence = '<';
Back:
			pFenceMatch->scanDirec = Backward;
			break;
		default:
			return NULL;
		}
	return pPoint;
	}

// Find a matching fence and either move point to it, or create a region.  Scan for the matching fence forward or backward,
// depending on the source fence character.  Use "fence" as the source fence if it's value is > 0; otherwise, use character at
// point.  If the fence is found: if pRegion is not NULL, set point in the Region object to original point, set line count to
// zero (not used), set size to the number of characters traversed + 1 as a positive (point moved forward) or negative (point
// moved backward) amount, and restore point position; otherwise, set point to the matching fence position.  If the fence is not
// found: restore point position and return Failure, with no message.
int otherFence(short fence, Region *pRegion) {
	int fenceLevel;		// Current fence level.
	short c;		// Current character in scan.
	long chunk;
	int n;
	Point *pPoint;
	FenceMatch fenceMatch;

	// Set up control variables.
	if((pPoint = fencePrep(fence, &fenceMatch)) == NULL)
		goto Bust;

	// Scan until we find matching fence, or hit a buffer boundary.
	chunk = 0;
	fenceLevel = 1;
	n = (fenceMatch.scanDirec == Forward) ? 1 : -1;
	while(fenceLevel > 0) {
		(void) moveChar(n);
		++chunk;
		if(pPoint->offset < pPoint->pLine->used) {
			c = pPoint->pLine->text[pPoint->offset];
			if(c == fenceMatch.fence)
				++fenceLevel;
			else if(c == fenceMatch.otherFence)
				--fenceLevel;
			}
		if(boundary(pPoint, fenceMatch.scanDirec))
			break;
		}

	// If fenceLevel is zero, we have a match.
	if(fenceLevel == 0) {
		// Create region and restore point, or keep current point position.
		if(pRegion != NULL) {
			*pPoint = fenceMatch.origPoint;
			if(fenceMatch.scanDirec == Forward) {
				pRegion->point = *pPoint;
				pRegion->size = chunk + 1;
				}
			else {
				(void) moveChar(1);
				pRegion->point = *pPoint;
				(void) moveChar(-1);
				pRegion->size = -(chunk + 1);
				}
			pRegion->lineCount = 0;
			}
		else
			sess.cur.pWind->flags |= WFMove;
		return sess.rtn.status;
		}

	// Matching fence not found: restore previous position.
	*pPoint = fenceMatch.origPoint;
Bust:
	// Beep if interactive.
	if(interactive())
		tbeep();
	return rsset(Failure, 0, NULL);
	}

// Match opening or closing fence against its partner, and if in the current window, briefly light the cursor there.  If the
// fence is not found, return Failure with no message.
int fenceMatch(short fence, bool doBeep) {
	int fenceLevel;		// Current fence level.
	short c;		// Current character in scan.
	int n, row, rows;
	ushort moveFlag = 0;
	Point *pPoint;
	FenceMatch fenceMatch;

	// Skip this if executing a macro.
	if(curMacro.state == MacPlay)
		return sess.rtn.status;

	// Set up control variables.
	if((pPoint = fencePrep(fence, &fenceMatch)) == NULL)
		goto Bust;
	row = getWindPos(sess.cur.pWind);		// Row in window containing point.
	rows = sess.cur.pWind->rows;		// Number of rows in window.
	fenceLevel = 1;
	n = (fenceMatch.scanDirec == Forward) ? 1 : -1;

	// Scan until we find matching fence, or move out of the window.
	while(fenceLevel > 0) {
		if(moveChar(n) == NotFound)
			break;
		if(pPoint->offset < pPoint->pLine->used) {
			c = pPoint->pLine->text[pPoint->offset];
			if(c == fenceMatch.fence)
				++fenceLevel;
			else if(c == fenceMatch.otherFence)
				--fenceLevel;
			}
		else if(fenceMatch.scanDirec == Backward) {
			if(--row == 0)
				break;
			}
		else if(row++ == rows)
			break;
		}

	// If fenceLevel is zero, we have a match -- display the sucker.
	if(fenceLevel == 0) {
		moveFlag = sess.cur.pWind->flags & WFMove;
		if(update(INT_MIN) != Success)
			return sess.rtn.status;
		centiPause(sess.fencePause);
		}

	// Restore the previous position.
	*pPoint = fenceMatch.origPoint;
	if(moveFlag)
		sess.cur.pWind->flags |= WFMove;
	if(fenceLevel == 0)
		return sess.rtn.status;
Bust:
	// Beep if interactive.
	if(doBeep && interactive())
		tbeep();
	return rsset(Failure, 0, NULL);
	}

// Get source fence character from command line argument or user input if non-default n.  Return status.
static int getFence(Datum *pRtnVal, int n, Datum **args, short *fence) {

	// Get source fence character...
	if(n == INT_MIN)
		*fence = 0;
	else if(sess.opFlags & OpScript) {
		if(!isCharVal(args[0]))
			return sess.rtn.status;
		*fence = args[0]->u.intNum;
		}
	else if(termInp(pRtnVal, text179, ArgNotNull1 | ArgNil1, Term_OneChar, NULL) == Success && !disnil(pRtnVal))
			// "Fence char"
		*fence = pRtnVal->u.intNum;

	return sess.rtn.status;
	}

// Move point to a matching fence if possible, and return Boolean result.
int gotoFence(Datum *pRtnVal, int n, Datum **args) {
	short fence;
	bool result = true;

	// Get source fence character...
	if(getFence(pRtnVal, n, args, &fence) != Success)
		return sess.rtn.status;

	// go to other fence...
	if(otherFence(fence, NULL) != Success) {
		if(sess.rtn.status < Failure)
			return sess.rtn.status;
		rsclear(0);
		result = false;
		}

	// and return result.	
	dsetbool(result, pRtnVal);
	return sess.rtn.status;
	}

// Find matching fence and, if in the current window, briefly light the cursor there.
int showFence(Datum *pRtnVal, int n, Datum **args) {
	short fence;

	// Get source fence character...
	if(getFence(pRtnVal, n, args, &fence) != Success)
		return sess.rtn.status;

	// and highlight other fence if possible.
	return fenceMatch(fence, true);
	}

// Move point backward or forward n tab stops.  Return -1 if invalid move, otherwise new offset in current line.
int tabStop(int n) {
	int offset, len, col, tabSize, curStop, targStop;
	Point *pPoint = &sess.cur.pFace->point;

	// Check for "do nothing" cases.
	if(n == 0 || (len = pPoint->pLine->used) == 0 || ((offset = pPoint->offset) == 0 && n < 0) || (offset == len && n > 0))
		return -1;

	// Calculate target tab stop column.
	tabSize = (sess.cur.pScrn->softTabSize == 0) ? sess.cur.pScrn->hardTabSize : sess.cur.pScrn->softTabSize;
	curStop = (col = getCol(NULL, false)) / tabSize;
	if(col % tabSize != 0 && n < 0)
		++curStop;
	return (targStop = curStop + n) <= 0 ? 0 : getGoal(pPoint->pLine, targStop * tabSize);
	}
