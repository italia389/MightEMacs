// (c) Copyright 2020 Richard W. Marinelli
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

// This routine figures out the bounds of the region in the current window and fills in the fields of the Region structure
// pointed to by pRegion.  Because point and mark RegionMark are usually very close together, we scan outward from point in both
// directions at once, looking for the mark.  This should save time.  If RForceBegin flag is set, point in the Region object
// is forced to beginning of region; otherwise, it is left at original starting point and size is negated if region extends
// backward.  Return status.
int getRegion(Region *pRegion, ushort flags) {
	struct {
		Line *pLine;
		long size;
		int lineCount;
		} fwd, bwd;
	Point *pMarkPoint = &sess.pCurBuf->markHdr.point;
	Point *pPoint = &sess.pCurWind->face.point;

	if(pMarkPoint->offset < 0)
		return rsset(Failure, RSTermAttr, text11, RegionMark);
			// "No mark ~u%c~U in this buffer"

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
	while(fwd.pLine->next != NULL || bwd.pLine != sess.pCurBuf->pFirstLine) {
		if(fwd.pLine->next != NULL) {
			fwd.pLine = fwd.pLine->next;
			if(fwd.pLine == pMarkPoint->pLine) {
				pRegion->point = *pPoint;
				pRegion->size = fwd.size + pMarkPoint->offset;
				pRegion->lineCount = fwd.lineCount + (pMarkPoint->offset > 0 ? 1 : 0);
				return sess.rtn.status;
				}
			fwd.size += fwd.pLine->used + 1;
			++fwd.lineCount;
			}
		if(bwd.pLine != sess.pCurBuf->pFirstLine) {
			bwd.pLine = bwd.pLine->prev;
			bwd.size += bwd.pLine->used + 1;
			++bwd.lineCount;
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
	return rsset(FatalError, 0, text77, "getRegion", RegionMark, sess.pCurBuf->bufname);
		// "%s() bug: Mark '%c' not found in buffer '%s'!"
	}

// Create region from given point and n argument and return in *pRegion, using n as a text (not line) selector.  If n == 1 (the
// default), select text from point to the end of the line, unless point is at the end of the line, in which case select just
// the newline.  If n == 0, select text from point to the beginning of the line.  If n > 1, select text from point forward over
// that number of line breaks - 1 to the end of the last line and, if RInclDelim flag set, include its delimiter.  Lastly, if
// n < 0, select text backward over that number of line breaks to the beginning of the first line.  If RForceBegin flag set,
// point in the Region object is forced to beginning of region; otherwise, it is left at original starting point and size is
// negated if region extends backward.
void getTextRegion(Point *pPoint, int n, Region *pRegion, ushort flags) {
	Line *pLine;
	long chunk;

	pRegion->point = *pPoint;
	pRegion->lineCount = bempty(NULL) ? 0 : 1;
	if(n == INT_MIN || n == 1) {		// From point to end of line.
		if((chunk = pPoint->pLine->used - pPoint->offset) == 0)
			chunk = bufEnd(pPoint) ? 0 : 1;	// Select line delimiter, if any.
		else if((flags & RInclDelim) && pPoint->pLine->next != NULL)
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
				if((flags & RInclDelim) && pLine->next != NULL)
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
			if(pLine == sess.pCurBuf->pFirstLine)
				break;
			pLine = pLine->prev;
			chunk -= 1 + pLine->used;
			++pRegion->lineCount;
			if(flags & RForceBegin)
				pRegion->point.pLine = pLine;
			} while(++n < 0);
		}

	// Return results.
	if((pRegion->size = (flags & RForceBegin) ? labs(chunk) : chunk) == 0)
		pRegion->lineCount = 0;
	}

// Get a region bounded by a line block and return in *pRegion.  Force point in Region object to beginning of block.  If n == 0,
// call getRegion() to get initial region; otherwise, call getTextRegion().  Pass "flags" to the function in either case.  If
// RInclDelim flag set, include delimiter of last line of block; otherwise, omit it.  If REmptyOK flag set, allow empty
// region.  If RLineSelect flag set, include current line in line count if not a EOB.  Return status.
int getLineRegion(int n, Region *pRegion, ushort flags) {
	Point *pPoint = &sess.pCurWind->face.point;

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
		Point *pMarkPoint = &sess.pCurBuf->markHdr.point;

		// Bump line count in region if it ends at the beginning of a line which is not at EOB and region is not empty.
		if((flags & RLineSelect) && pRegion->size > 0) {
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
		if((flags & RInclDelim) && pPoint->pLine->next != NULL && pMarkPoint->pLine->next != NULL)
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
	if(n == INT_MIN) {
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
int regionLines(int *pLineCount) {
	Region region;

	// Get "line block" region.
	if(getLineRegion(0, &region, RInclDelim | REmptyOK | RLineSelect) != Success)
		return sess.rtn.status;

	// Move point to beginning of region and return result.
	movePoint(&region.point);
	*pLineCount = region.lineCount;
	return sess.rtn.status;
	}

// Delete or kill a region, depending on "kill" flag.  Return status.
int delKillRegion(int n, bool kill) {
	Region region;

	if(getRegion(&region, 0) == Success && killPrep(kill) == Success)
		(void) edelc(region.size, kill ? EditKill : EditDel);
	return sess.rtn.status;
	}

// Copy all of the characters in the given region to the kill ring without moving point.  Return status.
int regionToKill(Region *pRegion) {

	if(killPrep(true) == Success && pRegion->size != 0) {
		Line *pLine;
		int offset, chunk;
		DFab fab;

		if(dopenwith(&fab, &ringTable[RingIdxKill].pEntry->data, pRegion->size < 0 ? FabPrepend : FabAppend) != 0)
			goto LibFail;
		pLine = pRegion->point.pLine;					// Current line.
		offset = pRegion->point.offset;				// Current offset.
		if(pRegion->size > 0)					// Going forward?
			do {						// Yes, copy forward.
				if(offset == pLine->used) {		// End of line.
					if(dputc('\n', &fab) != 0)
						goto LibFail;
					pLine = pLine->next;
					offset = 0;
					--pRegion->size;
					}
				else {					// Beginning or middle of line.
					chunk = (pLine->used - offset > pRegion->size) ? pRegion->size : pLine->used - offset;
					if(dputmem((void *) (pLine->text + offset), chunk, &fab) != 0)
						goto LibFail;
					offset += chunk;
					pRegion->size -= chunk;
					}
				} while(pRegion->size > 0);
		else {							// Copy backward.
			char *str;
			do {
				if(offset == 0) {			// Beginning of line.
					if(dputc('\n', &fab) != 0)
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
					if(dputmem((void *) str, chunk, &fab) != 0)
						goto LibFail;
					offset -= chunk;
					pRegion->size += chunk;
					}
				} while(pRegion->size < 0);
			}
		if(dclose(&fab, Fab_String) != 0)
			goto LibFail;
		}
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Copy all of the characters in the region to the given string buffer and return pointer to terminating null.  It is assumed
// that the buffer size is at least region size + 1 and the region point is at the beginning of the region.
char *regionToStr(char *buf, Region *pRegion) {
	Line *pLine;
	int offset;
	long replTableSize, len;
	char *dest;

	dest = buf;
	pLine = pRegion->point.pLine;					// Current line.
	offset = pRegion->point.offset;				// Current offset.
	replTableSize = pRegion->size;

	while(replTableSize > 0) {
		if((len = pLine->used - offset) == 0) {		// End of line.
			*dest++ = '\n';
			pLine = pLine->next;
			--replTableSize;
			offset = 0;
			}
		else {						// Beginning or middle of line.
			if(len > replTableSize)
				len = replTableSize;
			dest = (char *) memstpcpy((void *) dest, (void *) (pLine->text + offset), len);
			offset += len;
			replTableSize -= len;
			}
		}
	*dest = '\0';
	return dest;
	}

// Indent a region n tab stops.
int indentRegion(Datum *pRtnVal, int n, Datum **args) {
	Line *pLine;
	int count;
	Point *pPoint;

	// Validate n and determine number of tab stops.
	if(n == INT_MIN)
		count = 1;
	else if(n < 0)
		return rsset(Failure, 0, text39, text137, n, 0);
			// "%s (%d) must be %d or greater", "Repeat count"
	else
		count = n;

	// Get number of lines and set mark at starting point.
	if(regionLines(&n) != Success)
		return sess.rtn.status;
	setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);

	// Loop through lines in block.
	pPoint = &sess.pCurWind->face.point;
	keyEntry.prevFlags &= ~SF_VertMove;
	do {
		pPoint->offset = 0;				// Start at the beginning.
		pLine = pPoint->pLine;

		// Shift current line using tabs.
		if(pLine->used > 0 && !isWhite(pLine, pLine->used)) {
			if(sess.softTabSize == 0)
				(void) einsertc(count, '\t');
			else {
				beginTxt();
				(void) edInsertTab(count);
				}
			if(sess.rtn.status != Success)
				return sess.rtn.status;
			}

		// Move to the next line.
		(void) moveLine(1);			// Can't fail.
		} while(--n > 0);

	if(!bufEnd(pPoint))
		pPoint->offset = 0;
	keyEntry.curFlags &= ~SF_VertMove;			// Flag that this resets the goal column...
	bchange(sess.pCurBuf, WFHard);			// and a line other than the current one was changed.
	return sess.rtn.status;
	}

// Outdent a region n tab stops.
int outdentRegion(Datum *pRtnVal, int n, Datum **args) {
	int count;
	Point *pPoint;

	// Validate n and determine number of tab stops.
	if(n == INT_MIN)
		count = 1;
	else if(n < 0)
		return rsset(Failure, 0, text39, text137, n, 0);
			// "%s (%d) must be %d or greater", "Repeat count"
	else
		count = n;

	// Get number of lines and set mark at starting point.
	if(regionLines(&n) != Success)
		return sess.rtn.status;
	setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);

	// Loop through lines in block.
	pPoint = &sess.pCurWind->face.point;
	keyEntry.prevFlags &= ~SF_VertMove;
	do {
		pPoint->offset = 0;
		if(delTab(count, false) != Success)
			return sess.rtn.status;

		// Move to the next line.
		(void) moveLine(1);			// Can't fail.
		} while(--n > 0);

	keyEntry.curFlags &= ~SF_VertMove;			// Flag that this resets the goal column...
	bchange(sess.pCurBuf, WFHard);			// and a line other than the current one was changed.
	return sess.rtn.status;
	}
