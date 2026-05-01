// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// region.c	Region-related functions for MightEMacs.
//
// These routines deal with the region; that is, the space between the point and mark RegionMark.  Some functions are commands
// and some are just for internal use.

#include "std.h"
#include "bind.h"
#include "exec.h"

// This routine figures out the bounds of the region in the current window and fills in the fields of the Region structure
// pointed to by pRegion.  Because point and mark RegionMark are usually very close together, we scan outward from point in both
// directions at once, looking for the mark.  This should save time.  If RForceBegin flag is set, point in the Region object
// is forced to beginning of region; otherwise, it is left at original starting point and size is negated if region extends
// backward.  Return status.
int getRegion(Region *pRegion, uint flags) {
	struct {
		Line *pLine;
		long size;
		int lineCount;
		} fwd, bwd;
	Point *pMarkPoint = &sess.cur.pBuf->markHdr.point;
	Point *pPoint = &sess.cur.pFace->point;

	if(pMarkPoint->offset < 0) {
		char regionMark[2];

		regionMark[0] = RegionMark;
		regionMark[1] = '\0';
		return rsset(Failure, RSTermAttr, text11, regionMark);
			// "No mark ~u%s~U in this buffer"
		}

	// Do special case where mark is on current line.  If region is empty, line count is zero.
	if(pMarkPoint->pLine == pPoint->pLine) {
		pRegion->point.pLine = pPoint->pLine;
		if(pMarkPoint->offset > pPoint->offset || !(flags & RForceBegin)) {
			pRegion->point.offset = pPoint->offset;
			pRegion->size = (long) (pMarkPoint->offset - pPoint->offset);
			}
		else {
			pRegion->size = (long) (pPoint->offset - pMarkPoint->offset);
			pRegion->point.offset = pMarkPoint->offset;
			}
		pRegion->lineCount = (pRegion->size == 0) ? 0 : 1;
		return sess.rtn.status;
		}

	// Do general case; hunt forward and backward looking for mark.
	fwd.pLine = bwd.pLine = pPoint->pLine;
	bwd.lineCount = (bwd.size = (long) pPoint->offset) == 0 ? 0 : 1;
	fwd.size = (long) (fwd.pLine->used - pPoint->offset + 1);
	fwd.lineCount = 1;
	while(fwd.pLine->next != NULL || bwd.pLine != sess.cur.pBuf->pFirstLine) {
		if(fwd.pLine->next != NULL) {
			fwd.pLine = fwd.pLine->next;
			if(fwd.pLine == pMarkPoint->pLine) {
				pRegion->point = *pPoint;
				pRegion->size = fwd.size + pMarkPoint->offset;
				pRegion->lineCount = fwd.lineCount + (pMarkPoint->offset > 0 ? 1 : 0);
				return sess.rtn.status;
				}
			fwd.size += fwd.pLine->used + 1;
			if(++fwd.lineCount == INT_MAX)
				goto ErrRtn;
			}
		if(bwd.pLine != sess.cur.pBuf->pFirstLine) {
			bwd.pLine = bwd.pLine->prev;
			bwd.size += bwd.pLine->used + 1;
			if(++bwd.lineCount == INT_MAX)
				goto ErrRtn;
			if(bwd.pLine == pMarkPoint->pLine) {
				if(flags & RForceBegin) {
					pRegion->point.pLine = bwd.pLine;
					pRegion->point.offset = pMarkPoint->offset;
					pRegion->size = bwd.size - pMarkPoint->offset;
					}
				else {
					pRegion->point = *pPoint;
					pRegion->size = -(bwd.size - pMarkPoint->offset);
					}
				pRegion->lineCount = bwd.lineCount;
				return sess.rtn.status;
				}
			}
		}

	// Huh?  Didn't find mark RegionMark!  This is a bug.
	return rsset(FatalError, 0, text77, "getRegion", RegionMark, sess.cur.pBuf->bufname);
		// "%s() bug: Mark '%c' not found in buffer '%s'!"
ErrRtn:
	return rsset(Failure, 0, text12, text105, (long) INT_MAX + 1, 0, (long) INT_MAX);
		// "%s (%'ld) must be in range %d - %'ld", "Region line count"
	}

// Create region from given point and n argument and return in *pRegion, using n as a text (not line) selector.  If n == 1 (the
// default), select text from point to the end of the line, unless point is at the end of the line, in which case select just
// the newline.  If n == 0, select text from point to the beginning of the line.  If n > 1, select text from point forward over
// that number of line breaks - 1 to the end of the last line and, if RInclDelim flag set, include its delimiter.  Lastly, if
// n < 0, select text backward over that number of line breaks to the beginning of the first line.  If RForceBegin flag set,
// point in the Region object is forced to beginning of region; otherwise, it is left at original starting point and size is
// negated if region extends backward.
void getTextRegion(Point *pPoint, long n, Region *pRegion, uint flags) {
	Line *pLine;
	long chunk;

	pRegion->point = *pPoint;
	pRegion->lineCount = bempty(NULL) ? 0 : 1;
	if(n == DefN || n == 1) {		// From point to end of line.
		if((chunk = pPoint->pLine->used - pPoint->offset) == 0)
			chunk = bufEnd(pPoint) ? 0 : 1;	// Select line delimiter, if any.
		else if(flags & RInclDelim && pPoint->pLine->next != NULL)
			++chunk;		// Include line delimiter.
		}
	else if(n == 0) {			// From point to beginning of line.
		if(flags & RForceBegin)
			pRegion->point.offset = 0;
		chunk = -pPoint->offset;
		}
	else if(n > 1) {			// From point forward through multiple lines to end of last line.
		chunk = pPoint->pLine->used - pPoint->offset;
		for(pLine = pPoint->pLine->next; pLine != NULL; pLine = pLine->next) {
			chunk += 1 + pLine->used;
			if(pLine->used > 0 || pLine->next != NULL)
				++pRegion->lineCount;
			if(--n == 1) {
				if(flags & RInclDelim && pLine->next != NULL)
					++chunk;
				break;
				}
			}
		}
	else {					// From point backward through multiple lines to beginning of first line.
		if(flags & RForceBegin)
			pRegion->point.offset = 0;
		if((chunk = -pPoint->offset) == 0)
			pRegion->lineCount = 0;
		pLine = pPoint->pLine;
		do {
			if(pLine == sess.cur.pBuf->pFirstLine)
				break;
			pLine = pLine->prev;
			chunk -= 1 + pLine->used;
			++pRegion->lineCount;
			if(flags & RForceBegin)
				pRegion->point.pLine = pLine;
			} while(++n < 0);
		}

	// Return results.
	if((pRegion->size = flags & RForceBegin ? labs(chunk) : chunk) == 0)
		pRegion->lineCount = 0;
	}

// Get a region bounded by a line block and return in *pRegion.  Set point in Region object to beginning of block.  If n == 0,
// call getRegion() to get initial region; otherwise, call getTextRegion().  Pass "flags" to the function in either case.  If
// RInclDelim flag set, include delimiter of last line of block; otherwise, omit it.  If REmptyOK flag set, allow empty
// region.  If ROmitEndLine flag set, skip line count bump when point at beginning of last line.  Return status.
int getLineRegion(long n, Region *pRegion, uint flags) {
	Point *pPoint = &sess.cur.pFace->point;

	// Empty buffer?
	if(bempty(NULL)) {
		pRegion->lineCount = 0;
		goto NoText;
		}

	if(n == 0) {							// If zero argument...
		if(getRegion(pRegion, flags | RForceBegin) != Success)	// select all lines in region.
			return sess.rtn.status;
		if(pRegion->size == 0 && !(flags & REmptyOK))		// If empty region and not allowed...
			goto NoText;					// no lines were selected.
		Point *pMarkPoint = &sess.cur.pBuf->markHdr.point;

		// Bump line count in region if it ends at the beginning of a line which is not at EOB and region is not empty.
		if(!(flags & ROmitEndLine) && pRegion->size > 0) {
			Point *pEndPoint = (pRegion->point.pLine == pPoint->pLine && pRegion->point.offset == pPoint->offset) ?
			 pMarkPoint : pPoint;
			if(pEndPoint->offset == 0 && (pEndPoint->pLine->next != NULL || pEndPoint->pLine->used > 0))
				++pRegion->lineCount;
			}

		// Expand region to include text from point or mark at top of region backward to beginning of that line, and
		// text from point or mark at bottom of region forward to end of that line, plus its delimiter if RInclDelim
		// flag set.  However, don't include a line delimiter at end of last line of buffer (which doesn't exist).
		pRegion->size += (pRegion->point.pLine == pPoint->pLine && pRegion->point.offset == pPoint->offset) ?
		 pPoint->offset + pMarkPoint->pLine->used - pMarkPoint->offset :
		 pMarkPoint->offset + pPoint->pLine->used - pPoint->offset;
		if(flags & RInclDelim && pPoint->pLine->next != NULL && pMarkPoint->pLine->next != NULL)
			++pRegion->size;
		pRegion->point.offset = 0;

		// Empty region?
		if(pRegion->size == 0)
			goto NoText;
		if(pRegion->lineCount == 0)
			pRegion->lineCount = 1;
		return sess.rtn.status;
		}

	// Not selecting all lines in current region.  Get line block.
	Point workPoint = *pPoint;					// Use copy of point so it's not changed.
	if(n == DefN) {
		n = 1;
		goto Forw;
		}
	if(n < 0) {							// Going backward?
		if(workPoint.pLine->next == NULL ||
		 !(flags & RInclDelim))					// Yes, last line of buffer or omitting delimiter?
			workPoint.offset = workPoint.pLine->used;	// Yes, move to end of line.
		else {
			workPoint.pLine = workPoint.pLine->next;	// No, move to next line...
			--n;						// and bump the line count.
			goto Forw;
			}
		}
	else {
Forw:
		workPoint.offset = 0;					// No, move to beginning of line.
		}

	// Convert line block to a region.
	getTextRegion(&workPoint, n, pRegion, flags | RForceBegin);
	if(pRegion->size == 0) {
NoText:
		if(!(flags & REmptyOK))
			(void) rsset(Failure, RSNoFormat, text259);
					// "No text selected"
		}
	return sess.rtn.status;
	}

// Set *pLineCount to the number of lines in the current region, place point at beginning of region, and return status.
int regionLines(long *pLineCount) {
	Region region;

	// Get "line block" region.
	if(getLineRegion(0, &region, RInclDelim | REmptyOK) != Success)
		return sess.rtn.status;

	// Move point to beginning of region and return result.
	movePoint(&region.point);
	*pLineCount = region.lineCount;
	return sess.rtn.status;
	}

// Delete or kill a region, depending on "kill" flag.  Error if region is empty.  Return status.
int delKillRegion(bool kill) {
	Region region;

	if(getRegion(&region, 0) == Success) {
		if(region.size == 0)
			(void) rsset(Failure, RSNoFormat, text259);
					// "No text selected"
		else if(killPrep(kill) == Success)
			(void) edelc(region.size, kill ? EditKill : EditDel);
		}
	return sess.rtn.status;
	}

// Copy all of the characters in the given region to the kill ring without moving point.  Return status.
int regionToKill(Region *pRegion) {

	if(pRegion->size == 0)
		(void) rsset(Failure, RSNoFormat, text259);
				// "No text selected"
	else if(killPrep(true) == Success) {
		Line *pLine;
		int offset, chunk;
		DFab fab;

		if(dopenwith(&fab, &ringTable[RingIdxKill].pEntry->data, pRegion->size < 0 ? FabPrepend : FabAppend) != 0)
			goto LibFail;
		pLine = pRegion->point.pLine;				// Current line.
		offset = pRegion->point.offset;				// Current offset.
		if(pRegion->size > 0)					// Going forward?
			do {						// Yes, copy forward.
				if(offset == pLine->used) {		// End of line.
					if(dputc('\n', &fab, 0) != 0)
						goto LibFail;
					pLine = pLine->next;
					offset = 0;
					--pRegion->size;
					}
				else {					// Beginning or middle of line.
					chunk = (pLine->used - offset > pRegion->size) ? pRegion->size : pLine->used - offset;
					if(dputmem((void *) (pLine->text + offset), chunk, &fab, 0) != 0)
						goto LibFail;
					offset += chunk;
					pRegion->size -= chunk;
					}
				} while(pRegion->size > 0);
		else {							// Copy backward.
			char *str;
			do {
				if(offset == 0) {			// Beginning of line.
					if(dputc('\n', &fab, 0) != 0)
						goto LibFail;
					pLine = pLine->prev;
					offset = pLine->used;
					++pRegion->size;
					}
				else {					// End or middle of line.
					if(offset > -pRegion->size) {
						chunk = -pRegion->size;
						str = pLine->text + offset - chunk;
						}
					else {
						chunk = offset;
						str = pLine->text;
						}
					if(dputmem((void *) str, chunk, &fab, 0) != 0)
						goto LibFail;
					offset -= chunk;
					pRegion->size += chunk;
					}
				} while(pRegion->size < 0);
			}
		if(dclose(&fab, FabAuto) != 0)
			goto LibFail;
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Copy all of the characters in the region to the given string buffer and return pointer to terminating null.  It is assumed
// that the buffer size is at least region size + 1 and the region point is at the beginning of the region.
char *regionToStr(char *regBuf, Region *pRegion) {
	Line *pLine;
	int offset;
	long size, len;

	pLine = pRegion->point.pLine;				// Current line.
	offset = pRegion->point.offset;				// Current offset.
	size = pRegion->size;

	while(size > 0) {
		if((len = pLine->used - offset) == 0) {		// End of line.
			*regBuf++ = '\n';
			pLine = pLine->next;
			--size;
			offset = 0;
			}
		else {						// Beginning or middle of line.
			if(len > size)
				len = size;
			regBuf = (char *) memstpcpy((void *) regBuf, (void *) (pLine->text + offset), len);
			offset += len;
			size -= len;
			}
		}
	*regBuf = '\0';
	return regBuf;
	}

// Indent or outdent a line block per n and flags.  Common routine for commands and flags:
//	indentRegion		ShRegion
//	outdentRegion		ShRegion | ShOutdent
//	indentLine		ShLineSelect
//	outdentLine		ShLineSelect | ShOutdent
//	shiftLine		ShHoldPoint
int shiftLineBlock(Datum *pRtnVal, long n, Datum **args, uint flags) {
	Line *pLine;
	Point *pPoint;
	int tabStops;

	if(flags & ShLineSelect) {
		Region region;

		// n is line block selector (indentLine and outdentLine).  Get number of tab stops.
		if(sess.opFlags & OpScript)
			tabStops = dgetint(args[0]);
		else {
			TermInpCtrl termInpCtrl = {"1", RtnKey, 0, NULL};

			if(termInp(pRtnVal, text525, ArgInt1 | ArgNil1, 0, &termInpCtrl) != Success || disnil(pRtnVal) ||
			 toInt(pRtnVal, 10) != Success)
					// "Tab stops"
				return sess.rtn.status;
			tabStops = dgetint(pRtnVal);
			}

		// Convert line block to a region.
		if(getLineRegion(n, &region, RInclDelim | REmptyOK) != Success)
			return sess.rtn.status;

		// Move point to beginning of region.
		sess.cur.pFace->point = region.point;
		n = region.lineCount;
		}
	else {
		// n is number of tab stops (indentRegion, outdentRegion, and shiftLine).  Validate it.
		if(n == DefN)
			tabStops = 1;
		else if(n < 0 && flags & ShRegion)
			return rsset(Failure, 0, text39, text137, n, 0);
				// "%s (%'ld) must be %d or greater", "Repeat count"
		else
			tabStops = n;
		if(flags & ShRegion) {

			// Set n to number of lines in region.
			if(regionLines(&n) != Success)
				return sess.rtn.status;
			}
		else {
			// shiftLine command.  Set mark at current position.
			setWindMark(&sess.cur.pBuf->markHdr, sess.pCurWind);

			// If n is zero, delete white space at beginning of line and return.
			if(n == 0) {
				sess.cur.pFace->point.offset = 0;
				(void) nukeWhite(0, false);
				goto RestorePoint;
				}

			// Not zero.  Simulate indentRegion or outdentRegion command.
			n = 1;
			if(tabStops < 0) {
				flags |= ShOutdent;
				tabStops = -tabStops;
				}
			}
		}

	if(!(flags & ShHoldPoint))
		setWindMark(&sess.cur.pBuf->markHdr, sess.pCurWind);

	// Loop through lines in block.
	pPoint = &sess.cur.pFace->point;
	keyEntry.prevFlags &= ~SF_VertMove;
	if(flags & ShOutdent) {

		// Outdent n lines.
		do {
			pPoint->offset = 0;
			if(delTab(tabStops, false) != Success)
				return sess.rtn.status;

			// Move to the next line.
			(void) moveLine(1L);		// Can't fail.
			} while(--n > 0);
		}
	else {
		// Indent n lines.
		do {
			pPoint->offset = 0;
			pLine = pPoint->pLine;

			// Shift current line using tabs.
			if(pLine->used > 0 && !whiteLine(pLine)) {
				if(sess.pCurScrn->softTabSize == 0)
					(void) einsertc(tabStops, '\t');
				else {
					(void) beginTxt();
					(void) edInsertTab(tabStops);
					}
				if(sess.rtn.status != Success)
					return sess.rtn.status;
				}

			// Move to the next line.
			(void) moveLine(1L);		// Can't fail.
			} while(--n > 0);

		if(!bufEnd(pPoint))
			pPoint->offset = 0;
		}

	// Line block shift completed.  Flag that this resets the goal column.  If shiftLine command, move point back to
	// original position (on same line); otherwise, flag that a line other than the current one was changed.
	keyEntry.curFlags &= ~SF_VertMove;
	if(flags & ShHoldPoint)
RestorePoint:
		sess.cur.pFace->point = sess.cur.pBuf->markHdr.point;
	else
		bchange(sess.cur.pBuf, WFHard);

	return sess.rtn.status;
	}

// Select [-]N lines (default 1, region lines if N == 0) or text if n > 0 (where N is function argument), move point to
// first line of block, and return number of lines selected.  If n < 0, omit delimiter of last line selected.  Return status.
int selectText(Datum *pRtnVal, long n, Datum **args) {
	Region region;

	// Get region.
	if(n > 0)
		getTextRegion(&sess.cur.pFace->point, dgetint(args[0]), &region, RForceBegin);
	else if(getLineRegion(dgetint(args[0]), &region, n < 0 && n != DefN ? REmptyOK : RInclDelim | REmptyOK) != Success)
		return sess.rtn.status;

	// If region not empty, move point to beginning of region and set region in buffer.
	if(region.size > 0) {
		movePoint(&region.point);
		(void) moveChar(region.size);
		setWindMark(&sess.cur.pBuf->markHdr, sess.pCurWind);
		sess.cur.pFace->point = region.point;
		}

	// Return line count.
	dsetint(region.lineCount, pRtnVal);
	return sess.rtn.status;
	}
