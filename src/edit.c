// (c) Copyright 2020 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// edit.c	Text editing functions for MightEMacs.
//
// These routines edit lines in the current window and are the only routines that touch the text.  They also touch the buffer
// and window structures to make sure that the necessary updating gets done.

#include "std.h"
#include <ctype.h>
#include "bind.h"
#include "exec.h"
#include "search.h"

// Local definitions.
#define BlockSize(a)	((a + LineBlockSize) & ~(LineBlockSize - 1))
#define min(a, b)	((b < a) ? b : a)

// Allocate a block of memory large enough to hold a Line containing "used" characters and set *ppLine to the new block.
// Return status.
int lalloc(int used, Line **ppLine) {
	Line *pLine;

	// Use byte in text member of Line structure if possible.
	if((pLine = (Line *) malloc(sizeof(Line) + (used > 0 ? used - 1 : 0))) == NULL)
		return rsset(Panic, 0, text94, "lalloc");
			// "%s(): Out of memory!"
	pLine->size = pLine->used = used;
	*ppLine = pLine;
	return sess.rtn.status;
	}

// This routine is called when a buffer is changed (edited) in any way.  It updates all of the required flags in the buffer and
// windowing system.  The minimal flag(s) are passed as an argument; if the buffer is being displayed in more than one window,
// we change WFEdit to WFHard.  Also WFMode is set if this is the first buffer change (the "*" has to be displayed) and any
// user command/function preprocessing storage is freed.
void bchange(Buffer *pBuf, ushort flags) {

	if(pBuf->windCount > 1)			// Hard update needed?
		flags = WFHard;			// Yes.
	if(!(pBuf->flags & BFChanged)) {	// If first change...
		flags |= WFMode;		// need to update mode line.
		pBuf->flags |= BFChanged;
		}
	if(pBuf->pCallInfo != NULL)		// If precompiled user command or function...
		preprocFree(pBuf);		// force preprocessor redo.
	supd_windFlags(pBuf, flags);		// Lastly, flag any windows displaying this buffer.
	}

// Link line pLine1 into given buffer.  If pLine2 is NULL, link line at end of buffer; otherwise, link line before pLine2.  If
// pBuf is NULL, use current buffer.
void llink(Line *pLine1, Buffer *pBuf, Line *pLine2) {
	Line *pLine0;

	if(pBuf == NULL)
		pBuf = sess.pCurBuf;
	if(pLine2 == NULL) {
		pLine2 = pBuf->pFirstLine->prev;
		pLine1->next = NULL;
		pLine1->prev = pLine2;
		pBuf->pFirstLine->prev = pLine2->next = pLine1;
		}
	else {
		pLine0 = pLine2->prev;
		pLine1->next = pLine2;
		pLine2->prev = pLine1;

		// Inserting before first line of buffer?
		if(pLine2 == pBuf->pFirstLine)
			pBuf->pFirstLine = pLine1;	// Yes.
		else
			pLine0->next = pLine1;		// No.

		pLine1->prev = pLine0;
		}
	}

// Unlink a line from given buffer and free it.  If pBuf is NULL, use current buffer.  It is assumed that at least two lines
// exist.
void lunlink(Line *pLine, Buffer *pBuf) {

	if(pBuf == NULL)
		pBuf = sess.pCurBuf;
	if(pLine == pBuf->pFirstLine)

		// Deleting first line of buffer.
		(pBuf->pFirstLine = pLine->next)->prev = pLine->prev;
	else if(pLine == pBuf->pFirstLine->prev)

		// Deleting last line of buffer.
		(pBuf->pFirstLine->prev = pLine->prev)->next = NULL;
	else {
		// Deleting line between two other lines.
		pLine->next->prev = pLine->prev;
		pLine->prev->next = pLine->next;
		}
	free((void *) pLine);
	}

// Replace line pLine1 in given buffer with pLine2 and free pLine1.  If pBuf is NULL, use current buffer.
void lreplace1(Line *pLine1, Buffer *pBuf, Line *pLine2) {

	if(pBuf == NULL)
		pBuf = sess.pCurBuf;
	pLine2->next = pLine1->next;
	if(pLine1 == pBuf->pFirstLine) {

		// Replacing first line of buffer.
		pBuf->pFirstLine = pLine2;
		if(pLine1->prev == pLine1)

			// First line is only line... point new line to itself.
			pLine2->prev = pLine2;
		else {
			pLine2->prev = pLine1->prev;
			pLine1->next->prev = pLine2;
			}
		}
	else {
		pLine1->prev->next = pLine2;
		pLine2->prev = pLine1->prev;
		if(pLine1->next == NULL)

			// Replacing last line of buffer.
			pBuf->pFirstLine->prev = pLine2;
		else
			pLine1->next->prev = pLine2;
		}
	free((void *) pLine1);
	}

// Replace lines pLine1 and pLine2 in the current buffer with pLine3 and free the first two.
static void lreplace2(Line *pLine1, Line *pLine2, Line *pLine3) {

	pLine3->next = pLine2->next;
	if(pLine1 == sess.pCurBuf->pFirstLine) {

		// pLine1 is first line of buffer.
		sess.pCurBuf->pFirstLine = pLine3;
		if(pLine2->next == NULL)

			// pLine2 is last line of buffer.
			pLine3->prev = pLine3;
		else {
			pLine3->prev = pLine1->prev;
			pLine2->next->prev = pLine3;
			}
		}
	else {
		pLine1->prev->next = pLine3;
		pLine3->prev = pLine1->prev;
		if(pLine2->next == NULL)

			// pLine2 is last line of buffer.
			sess.pCurBuf->pFirstLine->prev = pLine3;
		else
			pLine2->next->prev = pLine3;
		}
	free((void *) pLine1);
	free((void *) pLine2);
	}

// Fix "window face" line pointers and point offset after insert.
static void fixInsert(int offset, int n, WindFace *pWindFace, Line *pLine1, Line *pLine2) {

	if(pWindFace->pTopLine == pLine1)
		pWindFace->pTopLine = pLine2;
	if(pWindFace->point.pLine == pLine1) {
		pWindFace->point.pLine = pLine2;
		if(pWindFace->point.offset >= offset)
			pWindFace->point.offset += n;
		}
	}

// Scan all screens and windows and fix window pointers.
void fixWindFace(int offset, int n, Line *pLine1, Line *pLine2) {
	EScreen *pScrn;
	EWindow *pWind;

	//  In all screens...
	pScrn = sess.scrnHead;
	do {
		// In all windows...
		pWind = pScrn->windHead;
		do {
			fixInsert(offset, n, &pWind->face, pLine1, pLine2);
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);
	}

// Fix buffer pointers and marks after insert.
void fixBufFace(Buffer *pBuf, int offset, int n, Line *pLine1, Line *pLine2) {
	Mark *pMark;

	fixInsert(offset, n, &pBuf->face, pLine1, pLine2);
	pMark = &pBuf->markHdr;
	do {
		if(pMark->point.pLine == pLine1) {
			pMark->point.pLine = pLine2;

			// Note that unlike the point, marks are not "pushed forward" as text is inserted.  They remain anchored
			// where they are set.  Hence, want to use '>' here instead of '>='.
			if(pMark->point.offset > offset)
				pMark->point.offset += n;
			}
		} while((pMark = pMark->next) != NULL);
	}

// Insert "n" copies of the character "c" into the current buffer at point.  In the easy case, all that happens is the
// character(s) are stored in the line.  In the hard case, the line has to be reallocated.  When the window list is updated,
// need to (1), always update point in the current window; and (2), update mark and point in other windows if their buffer
// position is past the place where the insert was done.  Note that no validity checking is done, so if "c" is a newline, it
// will be inserted as a literal character.  Return status.
int einsertc(int n, short c) {
	char *str1, *str2;
	Line *pLine1, *pLine2;
	int offset;
	int i;

	// Don't allow if read-only buffer... and nothing to do if repeat count is zero.
	if(allowEdit(true) != Success || n == 0)
		return sess.rtn.status;

	// Negative repeat count is an error.
	if(n < 0)
		return rsset(Failure, 0, text39, text137, n, 0);
			// "%s (%d) must be %d or greater", "Repeat count"

	// n > 0.  Get current line and determine the type of insert.
	pLine1 = sess.pCurWind->face.point.pLine;
	offset = sess.pCurWind->face.point.offset;
	if(pLine1->used + n > pLine1->size) {			// Not enough room left in line: reallocate.
		if(lalloc(BlockSize(pLine1->used + n), &pLine2) != Success)
			return sess.rtn.status;			// Fatal error.
		pLine2->used = pLine1->used + n;		// Set new "used" length.
		str1 = pLine1->text;				// Copy old to new up to point.
		str2 = pLine2->text;
		while(str1 != pLine1->text + offset)
			*str2++ = *str1++;
		str2 += n;					// Make gap and copy remainder.
		while(str1 != pLine1->text + pLine1->used)
			*str2++ = *str1++;
		lreplace1(pLine1, NULL, pLine2);		// Link in the new line and get rid of the old one.
		}
	else {							// Easy: update in place.
		pLine2 = pLine1;				// Make gap in line for new character(s).
		pLine2->used += n;
		str2 = pLine1->text + pLine1->used;
		str1 = str2 - n;
		while(str1 != pLine1->text + offset)
			*--str2 = *--str1;
		}
	str1 = pLine2->text + offset;				// Store the new character(s) in the gap.
	i = n;
	do
		*str1++ = c;
		while(--i > 0);

	// Set the "line change" flag in the current window and update "face" settings.
	bchange(sess.pCurBuf, WFEdit);
	fixWindFace(offset, n, pLine1, pLine2);
	fixBufFace(sess.pCurBuf, offset, n, pLine1, pLine2);

	return sess.rtn.status;
	}

// Fix "window face" line pointers and point offset after newline was inserted.
static void fixInsertNL(int offset, WindFace *pWindFace, Line *pLine1, Line *pLine2) {

	if(pWindFace->pTopLine == pLine1)
		pWindFace->pTopLine = pLine2;
	if(pWindFace->point.pLine == pLine1) {
		if(pWindFace->point.offset < offset)
			pWindFace->point.pLine = pLine2;
		else
			pWindFace->point.offset -= offset;
		}
	}

// Insert a newline into the current buffer at point.  Return status.  The funny ass-backward way this is done is not a botch;
// it just makes the last line in the buffer not a special case.
int einsertNL(void) {
	char *str1, *str2;
	Line *pLine1, *pLine2;
	int offset;
	EScreen *pScrn;
	EWindow *pWind;
	Mark *pMark;

	if(allowEdit(true) != Success)			// Don't allow if read-only buffer.
		return sess.rtn.status;

	bchange(sess.pCurBuf, WFHard);
	pLine1 = sess.pCurWind->face.point.pLine;	// Get the address and offset of point.
	offset = sess.pCurWind->face.point.offset;
	if(lalloc(offset, &pLine2) != Success)		// New first half line.
		return sess.rtn.status;			// Fatal error.
	str1 = pLine1->text;				// Shuffle text around.
	str2 = pLine2->text;
	while(str1 != pLine1->text + offset)
		*str2++ = *str1++;
	str2 = pLine1->text;
	while(str1 != pLine1->text + pLine1->used)
		*str2++ = *str1++;
	pLine1->used -= offset;
	llink(pLine2, NULL, pLine1);

	// In all screens...
	pScrn = sess.scrnHead;
	do {
		// In all windows...
		pWind = pScrn->windHead;
		do {
			fixInsertNL(offset, &pWind->face, pLine1, pLine2);
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	// In current buffer...
	fixInsertNL(offset, &sess.pCurBuf->face, pLine1, pLine2);
	pMark = &sess.pCurBuf->markHdr;
	do {
		if(pMark->point.pLine == pLine1) {
			if(pMark->point.offset < offset)
				pMark->point.pLine = pLine2;
			else
				pMark->point.offset -= offset;
			}
		} while((pMark = pMark->next) != NULL);

#if MMDebug & Debug_Narrow
	dumpBuffer("einsertNL(): AFTER", NULL, true);
#endif
	return sess.rtn.status;
	}

// Insert a string at point.  "str" may be NULL.
int einserts(const char *str) {

	if(str != NULL) {
		while(*str != '\0') {
			if(((*str == '\n') ? einsertNL() : einsertc(1, *str)) != Success)
				break;
			++str;
			}
		}
	return sess.rtn.status;
	}

// Fix "window face" line pointers and point offset after a newline was deleted without line reallocation.
static void fixDelNL1(WindFace *pWindFace, Line *pLine1, Line *pLine2) {

	if(pWindFace->pTopLine == pLine2)
		pWindFace->pTopLine = pLine1;
	if(pWindFace->point.pLine == pLine2) {
		pWindFace->point.pLine = pLine1;
		pWindFace->point.offset += pLine1->used;
		}
	}

// Fix "window face" line pointers and point offset after a newline was deleted and a new line was allocated.
static void fixDelNL2(WindFace *pWindFace, Line *pLine1, Line *pLine2, Line *pLine3) {

	if(pWindFace->pTopLine == pLine1 || pWindFace->pTopLine == pLine2)
		pWindFace->pTopLine = pLine3;
	if(pWindFace->point.pLine == pLine1)
		pWindFace->point.pLine = pLine3;
	else if(pWindFace->point.pLine == pLine2) {
		pWindFace->point.pLine = pLine3;
		pWindFace->point.offset += pLine1->used;
		}
	}

// Delete a newline from current buffer -- join the current line with the next line.  Easy cases can be done by shuffling data
// around.  Hard cases require that lines be moved about in memory.  Called by edelc() only.  It is assumed that there will
// always be at least two lines in the current buffer when this routine is called and the current line will never be the last
// line of the buffer (which does not have a newline delimiter).
static int edelNL(void) {
	char *str1, *str2;
	Line *pLine1, *pLine2, *pLine3;
	EScreen *pScrn;
	EWindow *pWind;
	Mark *pMark;

	pLine1 = sess.pCurWind->face.point.pLine;
	pLine2 = pLine1->next;

	// Do simple join if room in current line for next line.
	if(pLine2->used <= pLine1->size - pLine1->used) {
		str1 = pLine1->text + pLine1->used;
		str2 = pLine2->text;
		while(str2 != pLine2->text + pLine2->used)
			*str1++ = *str2++;

		// In all screens...
		pScrn = sess.scrnHead;
		do {
			// In all windows...
			pWind = pScrn->windHead;
			do {
				fixDelNL1(&pWind->face, pLine1, pLine2);
				} while((pWind = pWind->next) != NULL);
			} while((pScrn = pScrn->next) != NULL);

		// In current buffer...
		fixDelNL1(&sess.pCurBuf->face, pLine1, pLine2);
		pMark = &sess.pCurBuf->markHdr;
		do {
			if(pMark->point.pLine == pLine2) {
				pMark->point.pLine = pLine1;
				pMark->point.offset += pLine1->used;
				}
			} while((pMark = pMark->next) != NULL);

		pLine1->used += pLine2->used;
		lunlink(pLine2, NULL);
		return sess.rtn.status;
		}

	// Simple join not possible, get more space.
	if(lalloc(pLine1->used + pLine2->used, &pLine3) != Success)
		return sess.rtn.status;	// Fatal error.
	str1 = pLine1->text;
	str2 = pLine3->text;
	while(str1 != pLine1->text + pLine1->used)
		*str2++ = *str1++;
	str1 = pLine2->text;
	while(str1 != pLine2->text + pLine2->used)
		*str2++ = *str1++;

	// Replace pLine1 and pLine2 with pLine3.
	lreplace2(pLine1, pLine2, pLine3);

	// In all screens...
	pScrn = sess.scrnHead;
	do {
		// In all windows...
		pWind = pScrn->windHead;
		do {
			fixDelNL2(&pWind->face, pLine1, pLine2, pLine3);
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	// In current buffer...
	fixDelNL2(&sess.pCurBuf->face, pLine1, pLine2, pLine3);
	pMark = &sess.pCurBuf->markHdr;
	do {
		if(pMark->point.pLine == pLine1)
			pMark->point.pLine = pLine3;
		else if(pMark->point.pLine == pLine2) {
			pMark->point.pLine = pLine3;
			pMark->point.offset += pLine1->used;
			}
		} while((pMark = pMark->next) != NULL);

	return sess.rtn.status;
	}

// Fix point offset after delete.
static void fixDotDel1(int offset, int chunk, Point *pPoint) {
	int delta;

	if(pPoint->offset > offset) {
		if(chunk >= 0) {
			delta = pPoint->offset - offset;
			pPoint->offset -= (chunk < delta ? chunk : delta);
			}
		else
			pPoint->offset += chunk;
		}
	else if(chunk < 0 && (delta = chunk + (offset - pPoint->offset)) < 0)
		pPoint->offset += delta;
	}

// Fix point after delete in other windows with the same text displayed.
static void fixDotDel(Line *pLine, int offset, int chunk) {
	EScreen *pScrn;
	EWindow *pWind;
	Mark *pMark;

	// In all screens...
	pScrn = sess.scrnHead;
	do {
		// In all windows...
		pWind = pScrn->windHead;
		do {
			if(pWind->face.point.pLine == pLine)
				fixDotDel1(offset, chunk, &pWind->face.point);
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	// In current buffer...
	if(sess.pCurBuf->face.point.pLine == pLine)
		fixDotDel1(offset, chunk, &sess.pCurBuf->face.point);
	pMark = &sess.pCurBuf->markHdr;
	do {
		if(pMark->point.pLine == pLine)
			fixDotDel1(offset, chunk, &pMark->point);
		} while((pMark = pMark->next) != NULL);
	}

// This function deletes up to n characters from the current buffer, starting at point.  The text that is deleted may include
// newlines.  Positive n deletes forward; negative n deletes backward.  Returns current status if all of the characters were
// deleted, NotFound (bypassing rsset()) if they were not (because point ran into a buffer boundary), or the appropriate status
// if an error occurred.  The deleted text is put in the kill ring if the EditKill flag is set, the delete ring if the EditDel
// flag is set, otherwise discarded.
int edelc(long n, ushort flags) {

	if(n == 0 || allowEdit(true) != Success)	// Don't allow if read-only buffer.
		return sess.rtn.status;

	ushort windFlags;
	char *str1, *str2;
	Line *pLine;
	int offset, chunk;
	RingEntry *pEntry;
	DFab fab;
	Point *pPoint = &sess.pCurWind->face.point;
	bool hitEOB = false;

	// Set kill buffer pointer.
	pEntry = (flags & EditKill) ? ringTable[RingIdxKill].pEntry : (flags & EditDel) ? ringTable[RingIdxDel].pEntry : NULL;
	windFlags = 0;

	// Going forward?
	if(n > 0) {
		if(pEntry != NULL && dopenwith(&fab, &pEntry->data, FabAppend) != 0)
			goto LibFail;
		do {
			// Get the current line.
			pLine = pPoint->pLine;
			offset = pPoint->offset;

			// Figure out how many characters to delete on this line.
			if((chunk = pLine->used - offset) > n)
				chunk = n;

			// If at the end of a line, merge with the next.
			if(chunk == 0) {

				// Can't delete past end of buffer.
				if(pLine->next == NULL)
					goto EOB;

				// Flag that we are making a hard change and delete newline.
				windFlags |= WFHard;
				if(edelNL() != Success)
					return sess.rtn.status;
				if(pEntry != NULL && dputc('\n', &fab) != 0)
					goto LibFail;
				--n;
				continue;
				}

			// Flag the fact we are changing the current line.
			windFlags |= WFEdit;

			// Find the limits of the kill.
			str1 = pLine->text + offset;
			str2 = str1 + chunk;

			// Save the text to the kill buffer.
			if(pEntry != NULL && chunk > 0 && dputmem((void *) str1, chunk, &fab) != 0)
				goto LibFail;

			// Shift what remains on the line leftward.
			while(str2 < pLine->text + pLine->used)
				*str1++ = *str2++;
			pLine->used -= chunk;

			// Fix any other windows with the same text displayed.
			fixDotDel(pLine, offset, chunk);

			// We have deleted "chunk" characters.  Update counter.
			n -= chunk;
			} while(n > 0);
		}
	else {
		if(pEntry != NULL && dopenwith(&fab, &pEntry->data, FabPrepend) != 0)
			goto LibFail;
		do {
			// Get the current line.
			pLine = pPoint->pLine;
			offset = pPoint->offset;

			// Figure out how many characters to delete on this line.
			if((chunk = offset) > -n)
				chunk = -n;

			// If at the beginning of a line, merge with the previous one.
			if(chunk == 0) {

				// Can't delete past beginning of buffer.
				if(pLine == sess.pCurBuf->pFirstLine)
					goto EOB;

				// Flag that we are making a hard change and delete the newline.
				windFlags |= WFHard;
				(void) moveChar(-1);
				if(edelNL() != Success)
					return sess.rtn.status;
				if(pEntry != NULL && dputc('\n', &fab) != 0)
					goto LibFail;
				++n;
				continue;
				}

			// Flag the fact we are changing the current line.
			windFlags |= WFEdit;

			// Find the limits of the kill.
			str1 = pLine->text + offset;
			str2 = str1 - chunk;

			// Save the text to the kill buffer.
			if(pEntry != NULL && dputmem((void *) str2, chunk, &fab) != 0)
				goto LibFail;

			// Shift what remains on the line leftward.
			while(str1 < pLine->text + pLine->used)
				*str2++ = *str1++;
			pLine->used -= chunk;
			pPoint->offset -= chunk;

			// Fix any other windows with the same text displayed.
			fixDotDel(pLine, offset, -chunk);

			// We have deleted "chunk" characters.  Update counter.
			n += chunk;
			} while(n < 0);
		}
	goto Retn;
EOB:
	hitEOB = true;
Retn:
	if(pEntry != NULL && dclose(&fab, Fab_String) != 0)
		goto LibFail;
	if(windFlags)
		bchange(sess.pCurBuf, windFlags);
	return hitEOB ? NotFound : sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Quote the next character, and insert it into the buffer abs(n) times, ignoring "Over" and "Repl" buffer modes if n < 0.  All
// characters are taken literally, including newline, which does not then have its line-splitting meaning.  The character is
// always read, even if it is inserted zero times, so that the command completes normally.  If a function key is pressed, its
// symbolic name is inserted.
int quoteChar(Datum *pRtnVal, int n, Datum **args) {
	ushort extKey;
	bool forceIns = false;

	// Move cursor from message line back to buffer if interactive.
	if(!(sess.opFlags & OpScript))
		if(tmove(sess.pCurScrn->cursorRow, sess.pCurScrn->cursorCol) != Success || tflush() != Success)
			return sess.rtn.status;

	// Set real value of n.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0) {
		n = -n;
		forceIns = true;
		}

	// Get the key.
	if(getkey(false, &extKey, true) != Success)
		return sess.rtn.status;

	if(n > 0) {
		// If this is a function or special key, put its name in.
		if(extKey & (FKey | Shift)) {
			int len;
			char keyBuf[16];

			len = strlen(ektos(extKey, keyBuf, false));
			do {
				if((!forceIns && overPrep(len) != Success) || einserts(keyBuf) != Success)
					break;
				} while(--n > 0);
			}

		// Otherwise, just insert the raw character n times.
		else if(forceIns || overPrep(n) == Success)
			(void) einsertc(n, ektoc(extKey, false));
		}

	return sess.rtn.status;
	}

// Set soft tab size to abs(n) if n <= 0; otherwise, insert a hard tab or spaces into the buffer n times.  If inserting soft
// tab(s), one or more spaces are inserted for each so that the first non-space character following point (or the newline) is
// pushed forward to the next tab stop.
int edInsertTab(int n) {

	// Non-positive n?
	if(n <= 0)
		// Set soft tab size.
		(void) setTabSize(abs(n), false);

	// Positive n.  Insert hard tab or spaces n times.
	else if(sess.softTabSize == 0)
		(void) einsertc(n, '\t');
	else {
		// Scan forward from point to next non-space character.
		Point workPoint = sess.pCurWind->face.point;
		int len = workPoint.pLine->used;
		while(workPoint.offset < len && workPoint.pLine->text[workPoint.offset] == ' ')
			++workPoint.offset;

		// Set len to size of first tab and loop n times.
		len = sess.softTabSize - getCol(&workPoint, false) % sess.softTabSize;
		do {
			if(einsertc(len, ' ') != Success)
				break;
			len = sess.softTabSize;
			} while(--n > 0);
		}

	return sess.rtn.status;
	}

// Set "line count" return message and return status.
static int lineMsg(ushort flags, int lineCount, const char *action) {

	return rsset(Success, flags, text307, lineCount, lineCount == 1 ? "" : "s", action);
					// "%d line%s %s"
	}

// Finish a routine that modifies a block of lines.
static int finishLineChange(Point *pOldPoint, bool backward, int count) {
	Mark *pMark;
	Point *pPoint = &sess.pCurWind->face.point;

	keyEntry.curFlags &= ~SF_VertMove;		// Flag that this resets the goal column.
	if(backward) {				// Set mark at beginning of line block and move point to end.
		pPoint->offset = 0;
		if(!bufBegin(pPoint))
			(void) moveLine(1);
		setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);
		*pPoint = *pOldPoint;
		pPoint->offset = 0;
		(void) moveLine(1);
		}

	// Scan marks in current buffer and adjust offset if needed.
	pMark = &sess.pCurBuf->markHdr;
	do {
		pPoint = &pMark->point;
		if(pPoint->offset > pPoint->pLine->used)
			pPoint->offset = pPoint->pLine->used;
		} while((pMark = pMark->next) != NULL);

	// Set window flags and return.
	if(count == 0)
		sess.pCurWind->flags |= WFMove;
	else
		bchange(sess.pCurBuf, WFHard);	// Flag that a line other than the current one was changed.

	return lineMsg(RSHigh, count, text260);
			// "changed"
	}

// Get (hard) tab size into *pTabSize for detabLine() and entabLine().
static int getTabSize(Datum **args, int *pTabSize) {
	int tabSize1;

	if(sess.opFlags & OpScript) {
		if((*args)->type == dat_nil) {
			*pTabSize = sess.hardTabSize;
			goto Retn;
			}
		tabSize1 = (*args)->u.intNum;
		}
	else {
		Datum *pDatum;
		long longVal;
		char workBuf[4];
		TermInpCtrl termInpCtrl = {workBuf, RtnKey, 0, NULL};

		if(dnewtrack(&pDatum) != 0)
			return librsset(Failure);
		sprintf(workBuf, "%d", sess.hardTabSize);
		if(termInp(pDatum, text393, ArgNotNull1 | ArgNil1, 0, &termInpCtrl) != Success)
				// "Tab size"
			goto Retn;
		if(pDatum->type == dat_nil)
			return Cancelled;
		if(ascToLong(pDatum->str, &longVal, false) != Success)
			goto Retn;
		tabSize1 = longVal;
		}
	if(checkTabSize(tabSize1, true) == Success)
		*pTabSize = tabSize1;
Retn:
	return sess.rtn.status;
	}

// Change tabs to spaces in a block of lines.  If n is zero, use lines in current region.  No error if attempt to move past end
// of buffer.  Always leave point at end of line block.
int detabLine(Datum *pRtnVal, int n, Datum **args) {
	int incr, len, tabSize1;
	int count;
	bool changed;
	Point *pPoint = &sess.pCurWind->face.point;
	Point origPoint = *pPoint;		// Original point position.

	// Get hard tab size.
	if(getTabSize(args, &tabSize1) != Success)
		return sess.rtn.status;

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && regionLines(&n) != Success)
		return sess.rtn.status;
	if(n < 0) {
		--n;
		incr = -1;
		}
	else
		incr = 1;

	pPoint->offset = 0;			// Start at the beginning.
	if(n > 0)
		setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);

	// Loop thru text, detabbing n lines.
	keyEntry.prevFlags &= ~SF_VertMove;
	for(count = 0; n != 0; n -= incr) {
		changed = false;

		// Detab the entire current line.
		while(pPoint->offset < pPoint->pLine->used) {

			// If we have a tab.
			if(pPoint->pLine->text[pPoint->offset] == '\t') {
				len = tabSize1 - (pPoint->offset % tabSize1);
				if(edelc(1, 0) != Success || insertNLSpace(-len, EditSpace | EditHoldPoint) != Success)
					return sess.rtn.status;
				pPoint->offset += len;
				changed = true;
				}
			else
				++pPoint->offset;
			}
		if(changed)
			++count;

		// Move forward or backward to the next line.
		pPoint->offset = 0;
		if(moveLine(incr) != Success)
			break;
		}

	return finishLineChange(&origPoint, incr == -1, count);
	}

// Define column-calculator macro.
#define nextTab(i, s)	(i - (i % s)) + s

// Change spaces to tabs (where possible) in a block of lines.  If n is zero, use lines in current region.  No error if attempt
// to move past end of buffer.  Always leave point at end of line block.  Return status.
int entabLine(Datum *pRtnVal, int n, Datum **args) {
	int incr;			// Increment to next line.
	int firstSpace;			// Index of first space if in a run.
	int curCol;			// Current point column.
	int tabSize1, len, count;
	bool changed;
	Point *pPoint = &sess.pCurWind->face.point;
	Point origPoint = *pPoint;		// Original point position.

	// Get hard tab size.
	if(getTabSize(args, &tabSize1) != Success)
		return sess.rtn.status;

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && regionLines(&n) != Success)
		return sess.rtn.status;
	if(n < 0) {
		--n;
		incr = -1;
		}
	else
		incr = 1;

	pPoint->offset = 0;		// Start at the beginning.
	if(n > 0)
		setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);

	// Loop thru text, entabbing n lines.
	keyEntry.prevFlags &= ~SF_VertMove;
	for(count = 0; n != 0; n -= incr) {

		// Entab the entire current line.
		curCol = 0;
		firstSpace = -1;
		changed = false;

		while(pPoint->offset <= pPoint->pLine->used) {

			// Time to compress?
			if(firstSpace >= 0 && nextTab(firstSpace, tabSize1) <= curCol) {

				// Yes.  Skip if just a single space; otherwise, chaos ensues.
				if((len = curCol - firstSpace) >= 2) {
					pPoint->offset -= len;
					if(edelc(len, 0) != Success || einsertc(1, '\t') != Success)
						return sess.rtn.status;
					changed = true;
					}
				firstSpace = -1;
				}
			if(pPoint->offset == pPoint->pLine->used)
				break;

			// Get the current character and check it.
			switch(pPoint->pLine->text[pPoint->offset]) {

				case '\t':	// A tab... expand it and fall through.
					if(edelc(1, 0) != Success ||
					 insertNLSpace(-(tabSize1 - (curCol % tabSize1)), EditSpace | EditHoldPoint) != Success)
						return sess.rtn.status;
					changed = true;

				case ' ':	// A space... beginning of run?
					if(firstSpace == -1)
						firstSpace = curCol;
					break;

				default:	// Any other char... reset.
					firstSpace = -1;
				}
			++curCol;
			++pPoint->offset;
			}
		if(changed)
			++count;

		// Move forward or backward to the next line.
		pPoint->offset = 0;
		if(moveLine(incr) != Success)
			break;
		}

	return finishLineChange(&origPoint, incr == -1, count);
	}

// Get indentation of given line.  Store indentation in *ppIndent if found; otherwise, set to NULL.  Return status.
static int getIndent(Datum **ppIndent, Line *pLine) {
	char *str, *strEnd;

	// Find end of indentation.
	strEnd = (str = pLine->text) + pLine->used;
	while(str < strEnd) {
		if(*str != ' ' && *str != '\t')
			break;
		++str;
		}

	// Save it if found.
	if(str == pLine->text)
		*ppIndent = NULL;
	else if(dnewtrack(ppIndent) != 0 || dsetsubstr(pLine->text, str - pLine->text, *ppIndent) != 0)
		return librsset(Failure);

	return sess.rtn.status;
	}

// Insert newline or space character abs(n) times and return status.  If EditSpace flag set, insert space(s); otherwise,
// newline(s).  If EditWrap flag set, do word wrapping if applicable.  If EditHoldPoint flag set, insert or replace character(s)
// ahead of point.  If n < 0, ignore "Over" and "Repl" buffer modes (force insert).  Commands that call this routine (+
// self-insert) and their actions per n argument are as follows:
//
// Command        defn                           n < 0                          n > 0
// -------------  -----------------------------  -----------------------------  -------------
// newline        langnl(), wrap, insert         Insert n times                 langnl() n times -OR- wrap, then insert n times
// openLine       Insert, hold point             Insert n times, move point to  Insert n times, hold point
//                                                first empty line if possible
// (self insert)  lang insert -OR- replace       Insert n times                 lang insert n times -OR- replace n times
// space          Wrap, replace                  Insert n times                 Wrap, then replace n times
// insertSpace    Replace, hold point            Insert n times, hold point     Replace n times, hold point
int insertNLSpace(int n, ushort flags) {

	if(n != 0) {
		if(n == INT_MIN)
			n = 1;

		// If EditWrap flag, positive n, "Wrap" mode is enabled, "Over" and "Repl" modes are disabled, wrap
		// column is defined, and we are now past wrap column, execute user-assigned wrap hook.
		if((flags & EditWrap) && n > 0 && modeSet(MdIdxWrap, NULL) &&
		 !isAnyBufModeSet(sess.pCurBuf, false, 2, modeInfo.cache[MdIdxOver], modeInfo.cache[MdIdxRepl]) &&
		 sess.wrapCol > 0 && getCol(NULL, true) > sess.wrapCol)
			if(execHook(NULL, INT_MIN, hookTable + HkWrap, 0) != Success)
				return sess.rtn.status;

		// If a space, positive n, and replace or overwrite mode, delete before insert.
		if((flags & EditSpace) && n > 0 && overPrep(n) != Success)
			return sess.rtn.status;

		// Insert some lines or spaces.
		if(!(flags & EditSpace)) {
			int count = abs(n);
			do {
				if(einsertNL() != Success)
					return sess.rtn.status;
				} while(--count > 0);
			}
		else if(einsertc(abs(n), ' ') != Success)
			return sess.rtn.status;
		if(flags & EditHoldPoint)
			(void) moveChar(-abs(n));
		}

	return sess.rtn.status;
	}

// Trim trailing whitespace from current line.  Return 1 if any whitespace was found; otherwise, 0.
static int ltrim(void) {
	char *str;
	int len;
	Line *pLine = sess.pCurWind->face.point.pLine;

	for(str = pLine->text + pLine->used; str > pLine->text; --str) {
		if(!isspace(str[-1]) && str[-1] != '\0')
			break;
		}
	if((len = str - pLine->text) == pLine->used)
		return 0;
	pLine->used = len;
	return 1;
	}

// Trim trailing whitespace from a block of lines.  If n is zero, use lines in current region.  No error if attempt to move past
// end of buffer.  Always leave point at end of line block.
int trimLine(Datum *pRtnVal, int n, Datum **args) {
	int incr, count;
	Point *pPoint = &sess.pCurWind->face.point;
	Point origPoint = *pPoint;		// Original point position.

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && regionLines(&n) != Success)
		return sess.rtn.status;
	if(n < 0) {
		--n;
		incr = -1;
		}
	else
		incr = 1;

	pPoint->offset = 0;			// Start at the beginning.
	if(n > 0)
		setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);

	// Loop through text, trimming n lines.
	keyEntry.prevFlags &= ~SF_VertMove;
	for(count = 0; n != 0; n -= incr) {
		count += ltrim();

		// Move forward or backward to the next line.
		pPoint->offset = 0;
		if(moveLine(incr) != Success)
			break;
		}

	return finishLineChange(&origPoint, incr == -1, count);
	}

// Open up some blank space.  The basic plan is to insert a bunch of newlines, and then back up over them.  Everything is done
// by the subcommand processors.  They even handle the looping.
int openLine(Datum *pRtnVal, int n, Datum **args) {
	int i;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rsset(Failure, 0, text39, text137, n, 0);
			// "%s (%d) must be %d or greater", "Repeat count"

	i = n;				// Insert newlines.
	do {
		if(einsertNL() != Success)
			return sess.rtn.status;
		} while(--i);
	(void) moveChar(-n);		// Backup over them all.
	return sess.rtn.status;
	}

// Open a line before [-]nth line with same indentation as the line preceding target line.
int openLineI(Datum *pRtnVal, int n, Datum **args) {

	if(n != 0) {
		// Move point to end of line preceding target line.
		(void) beginEndLine(pRtnVal, n == INT_MIN || n == 1 ? -1 : n - 1, true);

		// If hit beginning of buffer, just open a line; otherwise, call "newline and indent" routine.
		(void) (bufBegin(NULL) ? insertNLSpace(1, EditHoldPoint) : newlineI(pRtnVal, 1, NULL));
		}
	return sess.rtn.status;
	}

// Compute and insert "i" variable at point n times.
int inserti(Datum *pRtnVal, int n, Datum **args) {
	int i;
	char *str;
	static const char myName[] = "inserti";

	if(n == INT_MIN)
		n = 1;
	i = iVar.i;

	// Going forward?
	if(n > 0)
		do {
			asprintf(&str, iVar.format.str, i);
			if(str == NULL)
				return rsset(Panic, 0, text94, myName);
					// "%s(): Out of memory!"
			(void) einserts(str);
			free((void *) str);
			if(sess.rtn.status != Success)
				return sess.rtn.status;
			i += iVar.incr;
			} while(--n > 0);
	else {
		int len;

		do {
			len = asprintf(&str, iVar.format.str, i);
			if(str == NULL)
				return rsset(Panic, 0, text94, myName);
					// "%s(): Out of memory!"
			(void) einserts(str);
			free((void *) str);
			if(sess.rtn.status != Success)
				return sess.rtn.status;
			(void) moveChar(-len);
			i += iVar.incr;
			} while(++n < 0);
		}

	iVar.i = i;
	return sess.rtn.status;
	}

// Delete blank lines around point.  If point is on a blank line, delete all blank lines above and below the current line.
// If it is on a non-blank line, delete all blank lines above (n < 0) or below (otherwise) the current line.
int delBlankLines(Datum *pRtnVal, int n, Datum **args) {
	Line *pLine1, *pLine2;
	long count;
	Point *pPoint = &sess.pCurWind->face.point;

	if(n == INT_MIN)
		n = 0;

	// Only one line in buffer?
	if((pLine1 = sess.pCurBuf->pFirstLine)->next == NULL) {
		if(pLine1->used == 0 || !isWhite(pLine1, pLine1->used))
			return sess.rtn.status;
		count = pLine1->used;
		}
	else {
		// Two or more lines in buffer.  Check if current line is not blank and deleting prior blank lines.
		pLine1 = pPoint->pLine;
		if(n < 0 && !isWhite(pLine1, pLine1->used)) {

			// True.  Nothing to do if current line is first line in buffer.
			if(pLine1 == sess.pCurBuf->pFirstLine)
				return sess.rtn.status;

			// Not on first line.  Back up one line.
			pLine1 = pLine1->prev;
			}
		else
			pLine1 = pPoint->pLine;

		// Find first non-blank line going backward.
		while(isWhite(pLine1, pLine1->used)) {
			if(pLine1 == sess.pCurBuf->pFirstLine)
				break;
			pLine1 = pLine1->prev;
			}

		// Nothing to do if line is last line in buffer.
		if(pLine1->next == NULL)
			return sess.rtn.status;

		// pLine1 not last line.  Find first non-blank line going forward, counting characters as we go.
		pLine2 = pLine1;
		count = 0;
		for(;;) {
			pLine2 = pLine2->next;
			if(!isWhite(pLine2, pLine2->used))
				break;
			count += pLine2->used + 1;
			if(pLine2->next == NULL) {
				--count;
				break;
				}
			}

		// Handle special case where first buffer line is blank.
		if(isWhite(pLine1, pLine1->used)) {
			pPoint->pLine = pLine1;
			count += pLine1->used + 1;
			}
		else {
			if(count == 0)
				return sess.rtn.status;
			pPoint->pLine = pLine1->next;
			}
		}

	// Finally, move point to beginning of first blank line and delete all the white space.
	pPoint->offset = 0;
	return edelc(count, 0);
	}

// Insert a newline, then enough tabs and spaces to duplicate the indentation of the previous line.  Tabs are every
// sess.hardTabSize characters.  Quite simple.  Figure out the indentation of the current line.  Insert a newline by calling the
// standard routine.  Insert the indentation by inserting the right number of tabs and spaces.  Return status.  Normally bound
// to ^J.
int newlineI(Datum *pRtnVal, int n, Datum **args) {
	Datum *pIndent;
	Line *pLine = sess.pCurWind->face.point.pLine;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rsset(Failure, 0, text39, text137, n, 0);
				// "%s (%d) must be %d or greater", "Repeat count"

	// Get the indentation of the current line, if any.
	if(getIndent(&pIndent, pLine) != Success)
		return sess.rtn.status;

	// Insert lines with indentation in the final line.
	do {
		if(einsertNL() != Success || (pIndent != NULL && n == 1 && einserts(pIndent->str) != Success))
			break;
		} while(--n > 0);

	return sess.rtn.status;
	}

// Delete hard tabs or "chunks" of spaces forward or backward.  Return status.  Soft tabs are deleted such that the next
// non-space character (or end of line) past point is moved left until it reaches a tab stop, one or more times.  To accomplish
// this, spaces are deleted beginning at point going forward (if n > 0) and point does not move, or spaces are deleted prior to
// point going backward (if n < 0) and point follows.  If n < 0 and "force" is true (called by "backspace" command), deletions
// are done iteratively up to abs(n) times such that either one soft tab or one character (including a newline) is deleted in
// each iteration.
int delTab(int n, bool force) {
	int offset, i, direc;
	Point *pPoint = &sess.pCurWind->face.point;

	// Check for "do nothing" cases.
	if(n == INT_MIN)
		n = 1;
	if((i = pPoint->pLine->used) == 0 || ((offset = pPoint->offset) == 0 && n < 0) || (offset == i && n > 0))
		return sess.rtn.status;

	// Set direction and loop increment.
	if(n > 0) {
		direc = 1;
		i = 0;
		}
	else {
		direc = -1;
		i = -1;
		}

	// Do hard tabs first... simple.  Just delete up to n tab characters forward or backward.
	if(sess.softTabSize == 0) {
		n = abs(n);
		while((offset = pPoint->offset + i) >= 0 && offset < pPoint->pLine->used && pPoint->pLine->text[offset] == '\t')
			if(edelc(direc, 0) != Success || --n == 0)
				break;
		}
	else {
		// Process soft tab(s).  Loop until n == 0 if force is true; otherwise, just do just one iteration.
		i = -direc;
		do {
			// Delete spaces until the next non-space character to the right of point (if deleting forward) or at or
			// to the right of point (if deleting backward) is positioned at the next tab stop to its left, assuming
			// that position is at or to the right of point.  If deleting forward, begin at point and go rightward;
			// if deleting backward; begin at point - 1 and go leftward.  If this is not possible (because the
			// current run of spaces is too short), do nothing.  However, if deleting backward, point is at a tab
			// stop, and a short run of spaces precedes it, delete the run.

			//  ....+....1....+....2....+....3....+....4....+....5....+....6....+....7....+....8
			//  ^   ^   ^   ^   ^   ^   ^   ^   ^   ^	Tab stops if tab size is 4.
			//  ab      x   The end is near    Weird.	Sample text.

			// Bail out if deleting backward, not a force, and at BOL or the character just prior to point is not a
			// space; otherwise, go straight to char-delete if at BOL and a force.
			if(n < 0) {
				if(!force) {
					if(pPoint->offset == 0 || pPoint->pLine->text[pPoint->offset - 1] != ' ')
						break;
					}
				else if(pPoint->offset == 0)
					goto Nuke1;
				}

			// Save column position of point and scan forward to next non-space character.
			int dotCol = getCol(NULL, false);
			int len = pPoint->pLine->used;
			for(offset = pPoint->offset; pPoint->offset < len && pPoint->pLine->text[pPoint->offset] == ' ';
			 ++pPoint->offset);

			// Bail out if deleting forward and run length (len) is zero.
			if(n > 0 && (len = pPoint->offset - offset) == 0)
				break;
			int col1, col2, chunk1;

			// Get column position of non-space character (col2), calculate position of prior tab stop (col1), and
			// calculate size of first "chunk" (distance from prior tab stop to non-space character).
			col1 = ((col2 = getCol(NULL, false)) - 1) / sess.softTabSize * sess.softTabSize;
			chunk1 = col2 - col1;

			// If deleting forward...
			if(n > 0) {
				// If prior tab stop is before point, stop here if non-space character is not a tab stop or
				// point is not at BOL and the character just prior to point is a space.
				if(col1 < dotCol && (col2 % sess.softTabSize != 0 ||
				 (offset > 0 && pPoint->pLine->text[offset - 1] == ' '))) {
					pPoint->offset = offset;
					break;
					}
				goto Nuke;
				}

			// Deleting backward.  Scan backward to first non-space character.
			for(pPoint->offset = offset; pPoint->offset > 0 && pPoint->pLine->text[pPoint->offset - 1] == ' ';
			 --pPoint->offset);

			// Continue only if run (len) is not too short or point is at a tab stop.
			len = offset - pPoint->offset;
			pPoint->offset = offset;		// Restore original point position.
			if(len >= chunk1 || (len > 0 && dotCol % sess.softTabSize == 0)) {
Nuke:;
				// Delete one or more soft tabs.  Determine number of spaces to delete and nuke 'em.
				int max = (len - chunk1) / sess.softTabSize;
				int m = abs(n) - 1;
				m = (m > max ? max : m);
				if(edelc(-(len < chunk1 ? len : (chunk1 + m * sess.softTabSize)), 0) != Success)
					return sess.rtn.status;
				if(n > 0)
					pPoint->offset = offset;
				if(!force)
					break;
				n += (m + 1) * i;
				}
			else if(!force)
				break;
			else {
Nuke1:
				// Force-deleting backward and no tab found.  Delete one character instead.
				if(edelc(-1, 0) != Success)
					return sess.rtn.status;
				++n;
				}
			} while(n != 0);
		}

	return sess.rtn.status;
	}

// Kill, delete, or copy text for kdc values of -1, 0, or 1, respectively.  Save text to kill ring if kdc is non-zero.  If
// pRegion is not NULL, operate on the given region (assumed to not be empty); otherwise, operate on text selected by n via call
// to getTextRegion().  Return status.
int kdcText(int n, int kdc, Region *pRegion) {
	Region region;
	Point *pPoint = &sess.pCurWind->face.point;

	// Process region elsewhere if specified.
	if(pRegion != NULL) {
		if(kdc > 0)
			goto Kopy;
		if(killPrep(kdc) != Success)
			return sess.rtn.status;
		*pPoint = pRegion->point;
		return edelc(pRegion->size, kdc ? EditKill : EditDel);
		}

	// No region... process lines and make one.  Error if region is empty.
	getTextRegion(pPoint, n, &region, 0);
	if(region.size == 0)
		return rsset(Failure, RSNoFormat, text259);
				// "No text selected"

	// Kill, delete, or copy text.
	if(kdc <= 0) {

		// Kill or delete.
		if(killPrep(kdc) == Success)
			(void) edelc(region.size, kdc ? EditKill : EditDel);
		return sess.rtn.status;
		}

	// Copy.
	pRegion = &region;
Kopy:
	return regionToKill(pRegion) != Success ? sess.rtn.status : rsset(Success, RSHigh, "%s %s", text261, text262);
										// "Text", "copied"
	}

// Kill, delete, or copy whole line(s) via kdcText() for kdc values of -1, 0, or 1, respectively.  Called from execCmdFunc() for
// killLine, delLine, and copyLine commands.
int kdcLine(int n, int kdc) {
	Region region;

	// Convert line block to a region.
	if(getLineRegion(n, &region, RInclDelim) != Success)
		return sess.rtn.status;

	// Nuke or copy line block (as a region) via kdcText().
	if(kdc <= 0)
		sess.pCurWind->face.point = region.point;
	return kdcText(0, kdc, &region) != Success || kdc <= 0 ? sess.rtn.status : lineMsg(RSForce, region.lineCount, text262);
													// "copied"
	}

// Duplicate whole line(s) in the current buffer, leaving point at beginning of second line block.  Return status.
int dupLine(Datum *pRtnVal, int n, Datum **args) {
	Region region;

	// Convert line block to a region.
	if(getLineRegion(n, &region, RInclDelim) != Success)
		return sess.rtn.status;

	// Copy region to a local (stack) variable and append a newline if absent (a line block at EOB).
	char *strEnd, workBuf[region.size + 2];
	if((strEnd = regionToStr(workBuf, &region))[-1] != '\n') {
		strEnd[0] = '\n';
		strEnd[1] = '\0';
		}

	// Move point to beginning of line block and insert it.
	sess.pCurWind->face.point = region.point;
	if(einserts(workBuf) == Success)
		(void) beginTxt();
	return sess.rtn.status;
	}

// Select [-]N lines (default 1, region lines if N == 0) or text if n > 0 (where N is function argument), move point to
// first line of block, and return number of lines selected.  If n < 0, omit delimiter of last line selected.  Return status.
int selectLine(Datum *pRtnVal, int n, Datum **args) {
	Region region;

	// Get region.
	if(n > 0)
		getTextRegion(&sess.pCurWind->face.point, args[0]->u.intNum, &region, RForceBegin);
	else if(getLineRegion(args[0]->u.intNum, &region, n < 0 && n != INT_MIN ? REmptyOK | RLineSelect :
	 RInclDelim | REmptyOK | RLineSelect) != Success)
		return sess.rtn.status;

	// If region not empty, move point to beginning of region and set region in buffer.
	if(region.size > 0) {
		movePoint(&region.point);
		(void) moveChar(region.size);
		setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);
		sess.pCurWind->face.point = region.point;
		}

	// Return line count.
	dsetint((long) region.lineCount, pRtnVal);
	return sess.rtn.status;
	}

// Delete white space at and forward from point (if any) on current line.  If n > 0, include non-word characters as well.
static int nukeWhiteForw(int n) {
	short c;
	Point *pPoint = &sess.pCurWind->face.point;

	for(;;) {
		if(pPoint->offset == pPoint->pLine->used)
			break;
		c = pPoint->pLine->text[pPoint->offset];
		if(!isspace(c) && c != '\0' && (n <= 0 || wordChar[c]))
			break;
		if(edelc(1, 0) != Success)
			break;
		}

	return sess.rtn.status;
	}

// Delete white space surrounding point on current line.  If prior is true, delete white space immediately before point also.
// If n > 0, include non-word characters as well.
int nukeWhite(int n, bool prior) {
	short c;
	Point *pPoint = &sess.pCurWind->face.point;
	Line *pLine = pPoint->pLine;

	if(pLine->used > 0 && (prior || pPoint->offset == pLine->used || isspace(c = pLine->text[pPoint->offset]) ||
	 c == '\0' || (n > 0 && !wordChar[(int) c]))) {

		// Delete backward.
		for(;;) {
			if(pPoint->offset == 0)
				break;
			c = pLine->text[pPoint->offset - 1];
			if(!isspace(c) && c != '\0' && (n <= 0 || wordChar[c]))
				break;
			if(edelc(-1, 0) != Success)
				goto Retn;
			}

		// Delete forward.
		(void) nukeWhiteForw(n);
		}
Retn:
	return sess.rtn.status;
	}

// Join adjacent line(s), replacing all white space in between with (1), nothing if "pDelim" is nil; or (2), a single space
// character (unless either line is blank or all white space) and insert extra space character if first of two adjacent lines
// ends with any character specified in "pDelim".  Return status.
static int ljoin(Datum *pRtnVal, int n, Datum *pDelim) {
	int m, incr, newDot;
	bool insSpace = (pDelim == NULL || pDelim->type != dat_nil);
	Point *pPoint = &sess.pCurWind->face.point;

	// Determine bounds of line block.
	if(n == INT_MIN)
		n = -1;						// Join with one line above by default.
	else if(n == 1 ||
	 (n == 0 && (regionLines(&n) != Success || n == 1)))	// Join all lines in region.
		return sess.rtn.status;				// Nothing to do if n == 1 or region line count is 1.

	// Get ready.
	if(n > 0) {						// n is >= 2.
		incr = 1;
		(void) ltrim();
		newDot = pPoint->pLine->used;			// Save current end-of-line position.
		--n;
		}
	else
		incr = -1;

	// Join lines forward or backward.
	do {
		// Move to beginning of first line of pair if needed, trim white space, move to end, delete newline, delete
		// white space, and if pDelim not nil and not at beginning or end of new line, insert one or two spaces.
		if(incr == -1) {
			if(pPoint->pLine == sess.pCurBuf->pFirstLine)
				break;
			pPoint->pLine = pPoint->pLine->prev;
			}
		else if(pPoint->pLine->next == NULL)
			break;
		(void) ltrim();
		pPoint->offset = pPoint->pLine->used;
		if(edelc(1, 0) != Success || nukeWhiteForw(0) != Success)
			return sess.rtn.status;
		if(insSpace && pPoint->offset > 0 && pPoint->offset < pPoint->pLine->used) {
			m = 1;
			if(pDelim != NULL && index(pDelim->str, pPoint->pLine->text[pPoint->offset - 1]) != NULL)
				++m;
			if(einsertc(m, ' ') != Success)
				return sess.rtn.status;
			}
		} while((n -= incr) != 0);
	if(incr > 0)
		pPoint->offset = newDot;			// Set current position.

	return sess.rtn.status;
	}

// Join adjacent line(s) via ljoin(), passing argument value if script mode.
int joinLines(Datum *pRtnVal, int n, Datum **args) {
	Datum *pDelim = NULL;

	// Get sentence-end characters if script mode.
	if((sess.opFlags & OpScript) && !disnull(args[0]))
		pDelim = args[0];

	// Call ljoin().
	return ljoin(pRtnVal, n, pDelim);
	}

// Flags for sorting a block of lines.
#define SortDescending	0x0001		// Sort in descending order; otherwise, ascending.
#define SortIgnore	0x0002		// Ignore case in comparisons.

// Swap two sequential lines in given buffer.  It is assumed that pLine1 and pLine2 are different lines and pLine2 immediately
// follows pLine1.  Hence, pLine1 will never be the last line and pLine2 will never be the first line.
static void lswap(Buffer *pBuf, Line *pLine1, Line *pLine2) {
	Line *last = pBuf->pFirstLine->prev;
	Line *prev1 = pLine1->prev;
	Line *next2 = pLine2->next;

	pLine1->next = pLine2->next;
	pLine1->prev = pLine2;
	pLine2->next = pLine1;
	pLine2->prev = (prev1 == pLine2) ? pLine1 : prev1;

	if(pLine1 == pBuf->pFirstLine)
		pBuf->pFirstLine = pLine2;
	else
		prev1->next = pLine2;
	if(pLine2 == last)
		pBuf->pFirstLine->prev = pLine1;
	else
		next2->prev = pLine1;
	}

// Compare two lines.
static int linecmp(const Line *pLine1, const Line *pLine2, ushort flags) {
	int (*cmp)(const void *s1, const void *s2, size_t n) = (flags & SortIgnore) ? memcasecmp : memcmp;
	if(pLine1->used == 0)
		return -1;
	if(pLine2->used == 0)
		return 1;
	if(pLine1->used == pLine2->used)
		return cmp(pLine1->text, pLine2->text, pLine1->used);
	int size = min(pLine1->used, pLine2->used);
	int result = cmp(pLine1->text, pLine2->text, size);
	if(result != 0)
		return result;
	return (pLine1->used < pLine2->used) ? -1 : 1;
	}

// Swap line pointers.
static void swap(const Line **ppLine1, const Line **ppLine2) {
	const Line *pLine = *ppLine1;
	*ppLine1 = *ppLine2;
	*ppLine2 = pLine;
	}

// Select last line pointer in ppLine array as sorting pPivotLine, place the pPivotLine line at its correct position in the
// array, place all lessor lines (or greater if descending) before pPivotLine, and all greater lines (or lessor if descending)
// after pPivotLine.
static int partition (const Line **ppLine, int low, int high, ushort flags) {
	const Line **ppHighLine = ppLine + high;
	const Line *pPivotLine = *ppHighLine;		// Pivot.
	const Line **ppLowLine = ppLine + low;
	const Line **ppLine1 = ppLowLine - 1;		// Pointer to lessor or greater element.
	int orderFlag = (flags & SortDescending) ? -1 : 1;

	for(const Line **ppArrayEl = ppLowLine; ppArrayEl < ppHighLine; ++ppArrayEl) {

		// If current element is less or greater than the pPivotLine...
		if(linecmp(*ppArrayEl, pPivotLine, flags) * orderFlag < 0) {
			++ppLine1;		// increment pointer to lessor or greater element...
			swap(ppLine1, ppArrayEl);	// and swap.
			}
		}

	swap(ppLine1 + 1, ppHighLine);
	return ppLine1 + 1 - ppLine;
	}

// Sort an array of Line pointers using the QuickSort algorithm -- from index low through high.
static void lsort(const Line **ppLine, int low, int high, ushort flags) {

	if(low < high) {
		// Partitioning index.
		int i = partition(ppLine, low, high, flags);

		// ppLine[i] is now at right place.  Finish up by sorting elements (recursively) before and after index.
		lsort(ppLine, low, i - 1, flags);
		lsort(ppLine, i + 1, high, flags);
		}
	}

// Sort lines in given buffer and return status, given buffer pointer, point (which points to first line to sort), line count,
// and flags.  If n == 0, all lines in buffer are sorted.  If buffer is current buffer, point is moved to beginning of line
// block and block is marked as a region after sort; otherwise, point is moved to end of buffer.
int bsort(Buffer *pBuf, Point *pPoint, int n, ushort flags) {
	Line *pTopLine = NULL;

	// Get number of lines and set mark at starting point.
	if(n == 0)
		(void) bufLength(pBuf, &n);
	if(pBuf == sess.pCurBuf) {
		setWindMark(&pBuf->markHdr, sess.pCurWind);
		pTopLine = sess.pCurWind->face.pTopLine;
		}

	// Handle special cases where line count is 1 or 2.
	if(n == 1) {
		if(pBuf == sess.pCurBuf)
			goto MovePt;
		}
	else if(n == 2) {
		int orderFlag = (flags & SortDescending) ? -1 : 1;
		if(linecmp(pPoint->pLine, pPoint->pLine->next, flags) * orderFlag < 0) {
			if(pBuf == sess.pCurBuf) {
MovePt:
				(void) moveLine(n);		// Can't fail.
				supd_windFlags(pBuf, WFMove);
				}
			}
		else {
			lswap(pBuf, pPoint->pLine, pPoint->pLine->next);
			if(pBuf == sess.pCurBuf) {
				if(pPoint->pLine == pTopLine || pPoint->pLine->prev == pTopLine)
					pTopLine = NULL;
				(void) moveLine(-1);		// Can't fail.
				setWindMark(&pBuf->markHdr, sess.pCurWind);
				(void) moveLine(2);		// Can't fail.
				bchange(pBuf, WFHard);
				}
			}
		}
	else {
		// Three or more line in block.  Create array of pointers to each line.
		Line *pLine0, *pLine, **ppLine0, **ppLine, **ppLineEnd;
		Line *lastNext, *firstPrev = pPoint->pLine->prev;
		bool last;					// Last line of block is last line of buffer?
		if((ppLine0 = malloc(sizeof(Line *) * n)) == NULL)
			return rsset(Panic, 0, text94, "bsort");
				// "%s(): Out of memory!"
		ppLineEnd = (ppLine = ppLine0) + n;
		pLine = pPoint->pLine;
		do {
			if(pLine == pTopLine)
				pTopLine = NULL;
			*ppLine++ = pLine;
			pLine = pLine->next;
			} while(ppLine < ppLineEnd);
		lastNext = pLine;
		last = (pLine == NULL);

		// Sort the array.
		lsort((const Line **) ppLine0, 0, n - 1, flags);

		// Relink the lines in the block to each other.
		ppLine = ppLine0;
		pLine0 = *ppLine++;
		do {
			pLine = *ppLine++;
			pLine0->next = pLine;
			pLine->prev = pLine0;
			pLine0 = pLine;
			} while(ppLine < ppLineEnd);

		// Graft first line of array back into buffer.
		pLine = *ppLine0;
		if(pPoint->pLine == pBuf->pFirstLine) {
			pLine->prev = firstPrev;
			pBuf->pFirstLine = pLine;
			}
		else {
			pLine->prev = firstPrev;
			firstPrev->next = pLine;
			}
		if(pBuf == sess.pCurBuf) {
			pPoint->pLine = pLine;
			pPoint->offset = 0;
			setWindMark(&pBuf->markHdr, sess.pCurWind);
			}

		// Graft last line of array back into buffer.
		pPoint->pLine = pLine = *(ppLineEnd - 1);
		if(last) {
			pLine->next = NULL;
			pBuf->pFirstLine->prev = pLine;
			}
		else {
			pLine->next = lastNext;
			lastNext->prev = pLine;
			}

		if(pBuf == sess.pCurBuf) {
			(void) moveLine(1);		// Can't fail.
			bchange(pBuf, WFHard);
			}

		// Clean up.
		free((void *) ppLine0);
		}

	if(pBuf == sess.pCurBuf) {
		if(pTopLine == NULL) {
			sess.pCurWind->reframeRow = n + 1;
			sess.pCurWind->flags |= WFReframe;
			}
		(void) rsset(Success, 0, "%s %d %s%s%s", text475, n, text205, n == 1 ? "" : "s", text355);
						// "Sorted", "line", " and marked as region"
		}
	else {
		pPoint->pLine = pBuf->pFirstLine->prev;
		pPoint->offset = 0;
		}

	return sess.rtn.status;
	}

// Sort lines in region and return status.
int sortRegion(Datum *pRtnVal, int n, Datum **args) {
	ushort flags = 0;
	static Option options[] = {
		{"^Descending", "^Desc", 0, SortDescending},
		{"^Ignore", "^Ign", 0, SortIgnore},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text474, false, options};
			// "sort option"

	// Get options and validate them.
	if(n != INT_MIN) {
		if(parseOpts(&optHdr, text448, args[0], NULL) != Success)
				// "Options"
			return sess.rtn.status;
		flags = getFlagOpts(options);
		}

	// Get number of lines in region and sort the line block.
	if(regionLines(&n) != Success)
		return sess.rtn.status;
	keyEntry.prevFlags &= ~SF_VertMove;			// Kill goal column.
	return bsort(sess.pCurBuf, &sess.pCurWind->face.point, n, flags);
	}

// Kill, delete, or copy fenced region if kdc is -1, 0, or 1, respectively.  Return status.
bool kdcFencedRegion(int kdc) {
	Region region;

	if(otherFence(0, &region) == Success)
		(void) kdcText(0, kdc, &region);
	return sess.rtn.status;
	}

// Write text to a named buffer.
int bprint(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;

	// Negative repeat count is an error.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rsset(Failure, 0, text39, text137, n, 0);
			// "%s (%d) must be %d or greater", "Repeat count"

	// Get the buffer name.
	if((pBuf = bsrch(args[0]->str, NULL)) == NULL)
		return rsset(Failure, 0, text118, args[0]->str);
			// "No such buffer '%s'"
	if(!needSym(s_comma, true))
		return sess.rtn.status;

	return chgText(pRtnVal, n, Txt_Insert, pBuf);
	}

// Write lines in region to a (different) buffer.  If n >= 0, write characters in region instead.  If n <= 0, also delete
// selected text after write.  Return status.
int writeBuf(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;
	Region region;

	// Get the destination buffer name.  Make sure it's not the current buffer.
	pBuf = bdefault();
	if(getBufname(pRtnVal, text144, pBuf != NULL ? pBuf->bufname : NULL, OpCreate, &pBuf, NULL) != Success || pBuf == NULL)
			// "Write to"
		return sess.rtn.status;
	if(pBuf == sess.pCurBuf)
		return rsset(Failure, 0, text246);
			// "Cannot write to current buffer"

	// Get region.
	if((n >= 0 ? getRegion(&region, RForceBegin) :
	 getLineRegion(0, &region, RForceBegin | RInclDelim | REmptyOK | RLineSelect)) != Success)
		return sess.rtn.status;

	// Preallocate a string and copy region to it.
	if(dsalloc(pRtnVal, region.size + 1) != 0)
		return librsset(Failure);
	regionToStr(pRtnVal->str, &region);

	// Insert the text into the buffer.
	if(iorText(pRtnVal->str, 1, Txt_Insert, pBuf) != Success)
		return sess.rtn.status;
	if(n >= 0)
		(void) rsset(Success, RSHigh, "%s %s", text261, text96);
						// "Text", "written"
	else
		(void) lineMsg(RSHigh, region.lineCount, text96);
						// "written"

	// If n <= 0, delete the region text.
	if(n <= 0 && n != INT_MIN) {
		sess.pCurWind->face.point = region.point;
		if(killPrep(0) == Success)
			(void) edelc(region.size, EditDel);
		}

	return sess.rtn.status;
	}

// Wrap word on white space.  Beginning at point going backward, stop on the first word break (space character) or column n
// (which must be >= 0).  If we reach column n and n is before point, jump back to the point position and (i), if default n:
// start a new line; or (ii), if not default n: scan forward to a word break (and delete all white space) or end of line and
// start a new line.  Otherwise, break the line at the word break or point, delete all white space, and jump forward to the
// point position.  Make sure we force the display back to the left edge of the current window.
int wrapWord(Datum *pRtnVal, int n, Datum **args) {
	int wordLen;		// Length of word wrapped to next line.
	int leftMargin, origOffset;
	Point *pPoint = &sess.pCurWind->face.point;

	// Determine left margin.
	if(n == INT_MIN)
		leftMargin = 0;
	else if((leftMargin = n) < 0)
		return rsset(Failure, 0, text39, text322, n, 0);
			// "%s (%d) must be %d or greater", "Column number"

	// If blank line, do nothing.
	if(pPoint->pLine->used == 0)
		return sess.rtn.status;

	// Scan backward to first space character, if any.
	origOffset = pPoint->offset;
	wordLen = -1;
	while(pPoint->offset > 0) {

		// Back up one character.
		(void) moveChar(-1);
		++wordLen;
		if(pPoint->pLine->text[pPoint->offset] == ' ')
			break;
		}

	// If we are now at or past the left margin, start a new line.
	if(pPoint->offset == 0 || getCol(NULL, true) <= leftMargin) {

		// Go back to starting point and hunt forward for a break.
		pPoint->offset = origOffset;
		while(pPoint->offset < pPoint->pLine->used) {
			if(pPoint->pLine->text[pPoint->offset] == ' ') {
				if(nukeWhite(0, false) != Success)
					return sess.rtn.status;
				break;
				}
			(void) moveChar(1);
			}
		return einsertNL();
		}

	// Found a space.  Replace it with a newline.
	if(nukeWhite(0, false) != Success || einsertNL() != Success)
		return sess.rtn.status;

	// Move back to where we started.
	if(wordLen > 0)
		(void) moveChar(wordLen);

	// Make sure the display is not horizontally scrolled.
	if(sess.pCurWind->face.firstCol > 0) {
		sess.pCurWind->face.firstCol = 0;
		sess.pCurWind->flags |= (WFHard | WFMode);
		}

	return sess.rtn.status;
	}

// Wrap line(s) in a block specified by n argument.  Duplicate indentation from first line in all subsequent lines.  If script
// mode, also add value of first argument after indentation (for example, "// " or "# "), and pass second argument to
// ljoin().  No error if attempt to move past a buffer boundary.
int wrapLine(Datum *pRtnVal, int n, Datum **args) {
	Point *pPoint;
	Datum *pIndent = NULL;		// Leading white space on first line, if any.
	Datum *pPrefix = NULL;		// First argument if script mode.
	Datum *pDelim = NULL;		// Second argument if script mode.
	int indentCol, leftMargin, col;
	int prefixLen = 0;

	// Wrap column set?
	if(sess.wrapCol == 0)
		return rsset(Failure, RSNoFormat, text98);
			// "Wrap column not set"

	// Get prefix and end-sentence delimiters if script mode.
	if(sess.opFlags & OpScript) {
		if(!isEmpty(args[0])) {
			pPrefix = args[0];
			if(*pPrefix->str == ' ' || *pPrefix->str == '\t')
				return rsset(Failure, 0, text303, pPrefix->str);
					// "Invalid wrap prefix \"%s\""
			else
				prefixLen = strlen(pPrefix->str);
			}
		if(!isEmpty(args[1]))
			pDelim = args[1];
		}

	// Determine bounds of line block.
	pPoint = &sess.pCurWind->face.point;
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && regionLines(&n) != Success)			// Process all lines in region.
		return sess.rtn.status;
	else if(n < 0) {						// Back up to first line.
		int count = 1;
		while(pPoint->pLine != sess.pCurBuf->pFirstLine) {
			pPoint->pLine = pPoint->pLine->prev;
			++count;
			if(++n == 0)
				break;
			}
		if((n = count) > 1)
			sess.pCurWind->flags |= WFMove;
		}
	pPoint->offset = 0;						// Move to beginning of line.

	// Point now at beginning of first line and n > 0.
	(void) beginTxt();						// Move to beginning of text.
	if((indentCol = getCol(NULL, false)) + prefixLen >= sess.wrapCol)	// Too much indentation?
		return rsset(Failure, 0, text323, sess.wrapCol);
			// "Indentation exceeds wrap column (%d)"
	if(pPoint->offset > 0) {					// Save any indentation (from first line of block)...
		if(dnewtrack(&pIndent) != 0 || dsetsubstr(pPoint->pLine->text, pPoint->offset, pIndent) != 0)
			return librsset(Failure);
		if(edelc(-pPoint->offset, 0) != Success)		// and delete it.
			return sess.rtn.status;
		}

	// Remove any existing prefix string from each line of block.
	if(prefixLen > 0) {
		Point oldPoint = *pPoint;
		int count = n;
		char *str = pPrefix->str - 1;
		int stripLen;

		// Get length of stripped prefix.
		for(stripLen = prefixLen; stripLen > 1 && (isspace(str[stripLen]) || str[stripLen] == '\0'); --stripLen);
		do {
			(void) beginTxt();
			if(pPoint->pLine->used - pPoint->offset >= stripLen && memcmp(pPoint->pLine->text + pPoint->offset,
			 pPrefix->str, stripLen) == 0 && (edelc(stripLen, 0) != Success ||
			 (count == n && nukeWhite(0, false) != Success)))
				return sess.rtn.status;
			} while((pPoint->pLine = pPoint->pLine->next) != NULL && --count > 0);
		*pPoint = oldPoint;
		}

	if(n > 1 && ljoin(pRtnVal, n, pDelim) != Success)		// Convert line block to single line.
		return sess.rtn.status;
	(void) ltrim();							// Delete any trailing white space.
	if(pPoint->pLine->used > 0) {
		short c;
		int attrLen;
		leftMargin = indentCol + prefixLen;

		// Wrap current line until too short to wrap any further.
		for(;;) {
#if MMDebug & Debug_Wrap
			attrLen = pPoint->pLine->used < term.cols ? pPoint->pLine->used : term.cols;
			fprintf(logfile, "wrapLine(): wrapping line:\n%.*s\n", attrLen, pPoint->pLine->text);
			attrLen = 1;
			do {
				fprintf(logfile, "....+....%d", attrLen % 10);
				} while(++attrLen <= term.cols / 10);
			fputc('\n', logfile);
#endif
			pPoint->offset = 0;					// Move to beginning of line.

			// Insert indentation and prefix string.
			if((pIndent != NULL && einserts(pIndent->str) != Success) ||
			 (pPrefix != NULL && einserts(pPrefix->str) != Success))
				return sess.rtn.status;

			// Move forward until hit end of line or first non-whitespace character past wrap column.
			col = leftMargin;
			for(;;) {
				// Get next character and skip over any terminal attribute specification.
#if MMDebug & Debug_Wrap
				c = pPoint->pLine->text[pPoint->offset];
				fprintf(logfile, "At col: %3d  offset: %3d  char: %c ", col, pPoint->offset,
				 c == 011 ? '*' : c);
#endif
				col = newColTermAttr(pPoint, col, &attrLen);
				if(attrLen > 0)
					pPoint->offset += attrLen - 1;
#if MMDebug & Debug_Wrap
				c = pPoint->pLine->text[pPoint->offset];
		fprintf(logfile, " ->  new col: %3d  new offset: %3d  new char: %c\n", col, pPoint->offset, c == 011 ? '*' : c);
#endif
				// If now past wrap column and not on a whitespace character, wrap line.
				if(col > sess.wrapCol && (!isspace(c = pPoint->pLine->text[pPoint->offset]) && c != '\0')) {
					if(wrapWord(pRtnVal, leftMargin, args) != Success)
						return sess.rtn.status;
					break;
					}

				// If at end of line, we're done.
				if(++pPoint->offset == pPoint->pLine->used)
					goto Retn;
				}
			if(pPoint->pLine->used == 0)			// Blank line after wrap?
				return edelc(1, 0);			// Yes, done.
			}
		}
Retn:
	(void) moveChar(1);						// Can't fail.
	return sess.rtn.status;
	}

// Move point forward by the specified number of words (wordCount) or characters (charCount), converting characters to upper or
// lower case (tranTable != NULL) or words to title case (tranTable == NULL).  Processing stops when either wordCount or
// charCount reaches zero, both of which are assumed to be greater than zero.  If pWasChanged not NULL, set *pWasChanged to
// "changed" status; otherwise, set window update flag.
static void chgTextCase(int wordCount, long charCount, const char *tranTable, bool *pWasChanged) {
	short c;
	bool firstChar;
	bool changed = false;
	Point *pPoint = &sess.pCurWind->face.point;
	bool (*isCase)(short c) = (tranTable == NULL) ? NULL : (tranTable == upCase) ? isLowerCase : isUpperCase;

	do {
		while(!inWord()) {
			if(moveChar(1) != Success || --charCount == 0)
				goto Retn;
			}
		firstChar = true;
		while(inWord()) {
			c = pPoint->pLine->text[pPoint->offset];
			if(tranTable == NULL) {
				if(firstChar == isLowerCase(c)) {
					pPoint->pLine->text[pPoint->offset] = firstChar ? upCase[c] : lowCase[c];
					changed = true;
					}
				}
			else if(isCase(c)) {
				pPoint->pLine->text[pPoint->offset] = tranTable[c];
				changed = true;
				}
			firstChar = false;
			if(moveChar(1) != Success || --charCount == 0)
				goto Retn;
			}
		} while(--wordCount > 0);
Retn:
	if(pWasChanged != NULL)
		*pWasChanged = changed;
	else if(changed)
		bchange(sess.pCurBuf, WFHard);
	}

// Convert a block of lines to lower, title, or upper case.  If n is zero, use lines in current region.  If tranTable is NULL,
// do title case.  No error if attempt to move past end of buffer.  Always leave point at end of line block.
static int chgLineCase(int n, const char *tranTable) {
	bool changed;
	int incr, count;
	Point *pPoint = &sess.pCurWind->face.point;
	Point origPoint = *pPoint;		// Original point position.

	// Compute block size.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && regionLines(&n) != Success)
		return sess.rtn.status;
	if(n < 0) {
		--n;
		incr = -1;
		}
	else
		incr = 1;

	pPoint->offset = 0;			// Start at the beginning.
	if(n > 0)
		setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);

	// Loop through buffer text, changing case of n lines.
	keyEntry.prevFlags &= ~SF_VertMove;
	for(count = 0; n != 0; n -= incr) {

		// Process the current line from the beginning.
		if(pPoint->pLine->used > 0) {
			pPoint->offset = 0;
			chgTextCase(INT_MAX, pPoint->pLine->used, tranTable, &changed);
			if(changed)
				++count;
			}

		// Move forward or backward to the next line.
		pPoint->offset = 0;
		if(moveLine(incr) != Success)
			break;
		}

	return finishLineChange(&origPoint, incr == -1, count);
	}

// Convert text objects per flags: n words (CaseWord), n lines (CaseLine), or a region (CaseRegion) to lower case (CaseLower),
// title case (CaseTitle), or upper case (CaseUpper).  If n == 0 and CaseLine flag set, convert lines in current region.  If
// n < 0, convert words and lines backward from point; othewise, forward.  No error if attempt to move past end of buffer.  If
// converting words, point is left at ending position (forward or backward from point); if converting lines or a region, point
// is left at end of line block or region (forward from point always).  Return status.
int convertCase(int n, ushort flags) {
	const char *tranTable = flags & CaseUpper ? upCase : flags & CaseLower ? lowCase : NULL;

	// Process region.
	if(flags & CaseRegion) {
		Region region;

		if(getRegion(&region, RForceBegin) == Success && region.size > 0) {
			Point *pPoint = &sess.pCurWind->face.point;

			// Move point or mark so that (1), point is at beginning of region before case conversion; (2), point
			// will be left at end of region after case conversion; and (3), region will still be marked.
			if(region.point.pLine != pPoint->pLine || region.point.offset != pPoint->offset)

				// Mark is before point... move point there and leave mark as is.
				*pPoint = region.point;
			else
				// Mark is after point... set mark at point.
				setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);

			chgTextCase(INT_MAX, region.size, tranTable, NULL);
			(void) rsset(Success, 0, "%s %s", text222, text260);
						// "Case of text", "changed"
			}
		}

	// Process line(s).
	else if(flags & CaseLine)
		(void) chgLineCase(n, tranTable);

	// Process word(s).
	else if(n != 0) {

		// If going backward, move point to starting position and process a region.
		if(n == INT_MIN)
			n = 1;
		else if(n < 0) {
			Region region;

			setWindMark(&sess.pCurBuf->markHdr, sess.pCurWind);
			(void) moveWord(n);
			if(getRegion(&region, RForceBegin) == Success && region.size > 0) {
				chgTextCase(INT_MAX, region.size, tranTable, NULL);
				sess.pCurWind->face.point = region.point;
				}
			goto Retn;
			}
		chgTextCase(n, LONG_MAX, tranTable, NULL);
		}
Retn:
	return sess.rtn.status;
	}

// Kill, delete, or copy forward by "n" words (which is never INT_MIN).  Save text to kill ring if kdc is non-zero.  If zero
// argument, kill or copy just one word and no trailing whitespace.
int kdcForwWord(int n, int kdc) {
	bool oneWord;
	Region region;
	Point *pPoint = &sess.pCurWind->face.point;

	// Check if at end of buffer.
	if(bufEnd(pPoint))
		return rsset(Failure, RSNoFormat, text259);
				// "No text selected"

	// Save the current point position.
	region.point = *pPoint;
	region.lineCount = 0;					// Not used.

	// Figure out how many characters to copy or give the axe.
	region.size = 0;

	// Get into a word...
	while(!inWord()) {
		if(moveChar(1) != Success)
			break;					// At end of buffer.
		++region.size;
		}

	oneWord = false;
	if(n == 0) {
		// Skip one word, no white space.
		while(inWord()) {
			if(moveChar(1) != Success)
				break;				// At end of buffer.
			++region.size;
			}
		oneWord = true;
		}
	else {
		short c;

		// Skip n words...
		oneWord = (n == 1);
		while(n-- > 0) {

			// If we are at EOL; skip to the beginning of the next.
			while(pPoint->offset == pPoint->pLine->used) {
				if(moveChar(1) != Success)
					goto Nuke;		// At end of buffer.
				++region.size;
				}

			// Move forward until we are at the end of the word.
			while(inWord()) {
				if(moveChar(1) != Success)
					goto Nuke;		// At end of buffer.
				++region.size;
				}

			// If there are more words, skip the interword stuff.
			if(n != 0)
				while(!inWord()) {
					if(moveChar(1) != Success)
						goto Nuke;	// At end of buffer.
					++region.size;
					}
			}

		// Skip white space and newlines.
		while(pPoint->offset == pPoint->pLine->used || isspace(c = pPoint->pLine->text[pPoint->offset]) || c == '\0') {
			if(moveChar(1) != Success)
				break;
			++region.size;
			}
		}
Nuke:
	// Have region... restore original position and kill, delete, or copy it.
	*pPoint = region.point;
	if(kdc <= 0) {

		// Kill or delete the word(s).
		if(killPrep(kdc) == Success)
			(void) edelc(region.size, kdc ? EditKill : EditDel);
		return sess.rtn.status;
		}

	// Copy the word(s).
	return regionToKill(&region) != Success ? sess.rtn.status :
	 rsset(Success, RSHigh, "%s%s %s", text115, oneWord ? "" : "s", text262);
					// "Word"			"copied"
	}

// Kill, delete, or copy backward by "n" words (which is always > 0).  Save text to kill ring if kdc is non-zero.
int kdcBackWord(int n, int kdc) {
	bool oneWord;
	long size;
	Region region;

	// Check if at beginning of buffer.
	if(moveChar(-1) != Success)
		return rsset(Failure, RSNoFormat, text259);
				// "No text selected"

	// Figure out how many characters to copy or give the axe.
	size = 0;

	// Skip back n words...
	oneWord = (n == 1);
	do {
		while(!inWord()) {
			++size;
			if(moveChar(-1) != Success)
				goto CopyNuke;		// At beginning of buffer.
			}
		while(inWord()) {
			++size;
			if(moveChar(-1) != Success)
				goto CopyNuke;		// At beginning of buffer.
			}
		} while(--n > 0);

	if(moveChar(1) != Success)
		return sess.rtn.status;
CopyNuke:
	// Have region, restore original position...
	(void) moveChar(size);		// Can't fail.

	// and kill, delete, or copy region.
	if(kdc <= 0) {

		// Kill or delete the word(s) backward.
		if(killPrep(kdc) == Success)
			(void) edelc(-size, kdc ? EditKill : EditDel);
		return sess.rtn.status;
		}

	// Copy the word(s) backward.
	region.point = sess.pCurWind->face.point;
	region.size = -size;
	region.lineCount = 0;				// Not used.
	return regionToKill(&region) != Success ? sess.rtn.status :
	 rsset(Success, RSHigh, "%s%s %s", text115, oneWord ? "" : "s", text262);
					// "Word"			"copied"
	}
