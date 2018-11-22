// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// edit.c	Text editing functions for MightEMacs.
//
// These routines edit lines in the current window and are the only routines that touch the text.  They also touch the buffer
// and window structures to make sure that the necessary updating gets done.

#include "os.h"
#include "std.h"
#include "bind.h"
#include "exec.h"
#include "search.h"

// Local definitions.
#define BSIZE(a)	(a + NBlock - 1) & (~(NBlock - 1))

// Allocate a block of memory large enough to hold a Line containing "used" characters and set *lnpp to the new block.
// Return status.
int lalloc(int used,Line **lnpp) {
	Line *lnp;

	// Use byte in l_text member of Line structure if possible.
	if((lnp = (Line *) malloc(sizeof(Line) + (used > 0 ? used - 1 : 0))) == NULL)
		return rcset(Panic,0,text94,"lalloc");
			// "%s(): Out of memory!"
	lnp->l_size = lnp->l_used = used;
	*lnpp = lnp;
	return rc.status;
	}

// This routine is called when a buffer is changed (edited) in any way.  It updates all of the required flags in the buffer and
// windowing system.  The minimal flag(s) are passed as an argument; if the buffer is being displayed in more than one window,
// we change WFEdit to WFHard.  Also WFMode is set if this is the first buffer change (the "*" has to be displayed) and any
// macro preprocessing storage is freed.
void bchange(Buffer *bufp,ushort flags) {

	if(bufp->b_nwind > 1)			// Hard update needed?
		flags = WFHard;			// Yes.
	if(!(bufp->b_flags & BFChanged)) {	// If first change...
		flags |= WFMode;		// need to update mode line.
		bufp->b_flags |= BFChanged;
		}
	if(bufp->b_mip != NULL)			// If precompiled macro...
		ppfree(bufp);			// force macro preprocessor redo.
	supd_wflags(bufp,flags);		// Lastly, flag any windows displaying this buffer.
	}

// Link line lnp1 into given buffer.  If lnp2 is NULL, link line at end of buffer; otherwise, link line before lnp2.  If bufp is
// NULL, use current buffer.
void llink(Line *lnp1,Buffer *bufp,Line *lnp2) {
	Line *lnp0;

	if(bufp == NULL)
		bufp = si.curbp;
	if(lnp2 == NULL) {
		lnp2 = bufp->b_lnp->l_prevp;
		lnp1->l_nextp = NULL;
		lnp1->l_prevp = lnp2;
		bufp->b_lnp->l_prevp = lnp2->l_nextp = lnp1;
		}
	else {
		lnp0 = lnp2->l_prevp;
		lnp1->l_nextp = lnp2;
		lnp2->l_prevp = lnp1;

		// Inserting before first line of buffer?
		if(lnp2 == bufp->b_lnp)
			bufp->b_lnp = lnp1;		// Yes.
		else
			lnp0->l_nextp = lnp1;		// No.

		lnp1->l_prevp = lnp0;
		}
	}

// Unlink a line from given buffer and free it.  If bufp is NULL, use current buffer.  It is assumed that at least two lines
// exist.
void lunlink(Line *lnp,Buffer *bufp) {

	if(bufp == NULL)
		bufp = si.curbp;
	if(lnp == bufp->b_lnp)

		// Deleting first line of buffer.
		(bufp->b_lnp = lnp->l_nextp)->l_prevp = lnp->l_prevp;
	else if(lnp == bufp->b_lnp->l_prevp)

		// Deleting last line of buffer.
		(bufp->b_lnp->l_prevp = lnp->l_prevp)->l_nextp = NULL;
	else {
		// Deleting line between two other lines.
		lnp->l_nextp->l_prevp = lnp->l_prevp;
		lnp->l_prevp->l_nextp = lnp->l_nextp;
		}
	free((void *) lnp);
	}

// Replace line lnp1 in given buffer with lnp2 and free lnp1.  If bufp is NULL, use current buffer.
void lreplace1(Line *lnp1,Buffer *bufp,Line *lnp2) {

	if(bufp == NULL)
		bufp = si.curbp;
	lnp2->l_nextp = lnp1->l_nextp;
	if(lnp1 == bufp->b_lnp) {

		// Replacing first line of buffer.
		bufp->b_lnp = lnp2;
		if(lnp1->l_prevp == lnp1)

			// First line is only line... point new line to itself.
			lnp2->l_prevp = lnp2;
		else {
			lnp2->l_prevp = lnp1->l_prevp;
			lnp1->l_nextp->l_prevp = lnp2;
			}
		}
	else {
		lnp1->l_prevp->l_nextp = lnp2;
		lnp2->l_prevp = lnp1->l_prevp;
		if(lnp1->l_nextp == NULL)

			// Replacing last line of buffer.
			bufp->b_lnp->l_prevp = lnp2;
		else
			lnp1->l_nextp->l_prevp = lnp2;
		}
	free((void *) lnp1);
	}

// Replace lines lnp1 and lnp2 in the current buffer with lnp3 and free the first two.
static void lreplace2(Line *lnp1,Line *lnp2,Line *lnp3) {

	lnp3->l_nextp = lnp2->l_nextp;
	if(lnp1 == si.curbp->b_lnp) {

		// lnp1 is first line of buffer.
		si.curbp->b_lnp = lnp3;
		if(lnp2->l_nextp == NULL)

			// lnp2 is last line of buffer.
			lnp3->l_prevp = lnp3;
		else {
			lnp3->l_prevp = lnp1->l_prevp;
			lnp2->l_nextp->l_prevp = lnp3;
			}
		}
	else {
		lnp1->l_prevp->l_nextp = lnp3;
		lnp3->l_prevp = lnp1->l_prevp;
		if(lnp2->l_nextp == NULL)

			// lnp2 is last line of buffer.
			si.curbp->b_lnp->l_prevp = lnp3;
		else
			lnp2->l_nextp->l_prevp = lnp3;
		}
	free((void *) lnp1);
	free((void *) lnp2);
	}

// Fix "window face" line pointers and dot offset after insert.
static void fixins(int offset,int n,WindFace *wfp,Line *lnp1,Line *lnp2) {

	if(wfp->wf_toplnp == lnp1)
		wfp->wf_toplnp = lnp2;
	if(wfp->wf_dot.lnp == lnp1) {
		wfp->wf_dot.lnp = lnp2;
		if(wfp->wf_dot.off >= offset)
			wfp->wf_dot.off += n;
		}
	}

// Scan all screens and windows and fix window pointers.
void fixwface(int offset,int n,Line *lnp1,Line *lnp2) {
	EScreen *scrp;
	EWindow *winp;

	//  In all screens...
	scrp = si.sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			fixins(offset,n,&winp->w_face,lnp1,lnp2);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);
	}

// Fix buffer pointers and marks after insert.
void fixbface(Buffer *bufp,int offset,int n,Line *lnp1,Line *lnp2) {
	Mark *mkp;

	fixins(offset,n,&bufp->b_face,lnp1,lnp2);
	mkp = &bufp->b_mroot;
	do {
		if(mkp->mk_dot.lnp == lnp1) {
			mkp->mk_dot.lnp = lnp2;

			// Note that unlike the point, marks are not "pushed forward" as text is inserted.  They remain anchored
			// where they are set.  Hence, want to use '>' here instead of '>='.
			if(mkp->mk_dot.off > offset)
				mkp->mk_dot.off += n;
			}
		} while((mkp = mkp->mk_nextp) != NULL);
	}

// Insert "n" copies of the character "c" into the current buffer at point.  In the easy case, all that happens is the
// character(s) are stored in the line.  In the hard case, the line has to be reallocated.  When the window list is updated,
// need to (1), always update dot in the current window; and (2), update mark and dot in other windows if their buffer position
// is past the place where the insert was done.  Note that no validity checking is done, so if "c" is a newline, it will be
// inserted as a literal character.  Return status.
int linsert(int n,short c) {
	char *str1,*str2;
	Line *lnp1,*lnp2;
	int offset;
	int i;

	// Don't allow if in read-only mode... and nothing to do if repeat count is zero.
	if(allowedit(true) != Success || n == 0)
		return rc.status;

	// Negative repeat count is an error.
	if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"

	// n > 0.  Get current line and determine the type of insert.
	lnp1 = si.curwp->w_face.wf_dot.lnp;
	offset = si.curwp->w_face.wf_dot.off;
	if(lnp1->l_used + n > lnp1->l_size) {		// Not enough room left in line: reallocate.
		if(lalloc(BSIZE(lnp1->l_used + n),&lnp2) != Success)
			return rc.status;		// Fatal error.
		lnp2->l_used = lnp1->l_used + n;	// Set new "used" length.
		str1 = lnp1->l_text;			// Copy old to new up to dot.
		str2 = lnp2->l_text;
		while(str1 != lnp1->l_text + offset)
			*str2++ = *str1++;
		str2 += n;				// Make gap and copy remainder.
		while(str1 != lnp1->l_text + lnp1->l_used)
			*str2++ = *str1++;
		lreplace1(lnp1,NULL,lnp2);		// Link in the new line and get rid of the old one.
		}
	else {						// Easy: update in place.
		lnp2 = lnp1;				// Make gap in line for new character(s).
		lnp2->l_used += n;
		str2 = lnp1->l_text + lnp1->l_used;
		str1 = str2 - n;
		while(str1 != lnp1->l_text + offset)
			*--str2 = *--str1;
		}
	str1 = lnp2->l_text + offset;			// Store the new character(s) in the gap.
	i = n;
	do
		*str1++ = c;
		while(--i > 0);

	// Set the "line change" flag in the current window and update "face" settings.
	bchange(si.curbp,WFEdit);
	fixwface(offset,n,lnp1,lnp2);
	fixbface(si.curbp,offset,n,lnp1,lnp2);

	return rc.status;
	}

// Fix "window face" line pointers and dot offset after newline was inserted.
static void fixinsnl(int offset,WindFace *wfp,Line *lnp1,Line *lnp2) {

	if(wfp->wf_toplnp == lnp1)
		wfp->wf_toplnp = lnp2;
	if(wfp->wf_dot.lnp == lnp1) {
		if(wfp->wf_dot.off < offset)
			wfp->wf_dot.lnp = lnp2;
		else
			wfp->wf_dot.off -= offset;
		}
	}

// Insert a newline into the current buffer at point.  Return status.  The funny ass-backward way this is done is not a botch;
// it just makes the last line in the buffer not a special case.
int lnewline(void) {
	char *str1,*str2;
	Line *lnp1,*lnp2;
	int offset;
	EScreen *scrp;
	EWindow *winp;
	Mark *mkp;

	if(allowedit(true) != Success)		// Don't allow if in read-only mode.
		return rc.status;

	bchange(si.curbp,WFHard);
	lnp1 = si.curwp->w_face.wf_dot.lnp;	// Get the address and offset of point.
	offset = si.curwp->w_face.wf_dot.off;
	if(lalloc(offset,&lnp2) != Success)	// New first half line.
		return rc.status;		// Fatal error.
	str1 = lnp1->l_text;			// Shuffle text around.
	str2 = lnp2->l_text;
	while(str1 != lnp1->l_text + offset)
		*str2++ = *str1++;
	str2 = lnp1->l_text;
	while(str1 != lnp1->l_text + lnp1->l_used)
		*str2++ = *str1++;
	lnp1->l_used -= offset;
	llink(lnp2,NULL,lnp1);

	// In all screens...
	scrp = si.sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			fixinsnl(offset,&winp->w_face,lnp1,lnp2);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In current buffer...
	fixinsnl(offset,&si.curbp->b_face,lnp1,lnp2);
	mkp = &si.curbp->b_mroot;
	do {
		if(mkp->mk_dot.lnp == lnp1) {
			if(mkp->mk_dot.off < offset)
				mkp->mk_dot.lnp = lnp2;
			else
				mkp->mk_dot.off -= offset;
			}
		} while((mkp = mkp->mk_nextp) != NULL);

#if MMDebug & Debug_Narrow
	dumpbuffer("lnewline(): AFTER",NULL,true);
#endif
	return rc.status;
	}

// Insert a string at the current point.  "str" may be NULL.
int linstr(char *str) {

	if(str != NULL) {
		while(*str != '\0') {
			if(((*str == '\n') ? lnewline() : linsert(1,*str)) != Success)
				break;
			++str;
			}
		}
	return rc.status;
	}

// Fix "window face" line pointers and dot offset after a newline was deleted without line reallocation.
static void fixdelnl1(WindFace *wfp,Line *lnp1,Line *lnp2) {

	if(wfp->wf_toplnp == lnp2)
		wfp->wf_toplnp = lnp1;
	if(wfp->wf_dot.lnp == lnp2) {
		wfp->wf_dot.lnp = lnp1;
		wfp->wf_dot.off += lnp1->l_used;
		}
	}

// Fix "window face" line pointers and dot offset after a newline was deleted and a new line was allocated.
static void fixdelnl2(WindFace *wfp,Line *lnp1,Line *lnp2,Line *lnp3) {

	if(wfp->wf_toplnp == lnp1 || wfp->wf_toplnp == lnp2)
		wfp->wf_toplnp = lnp3;
	if(wfp->wf_dot.lnp == lnp1)
		wfp->wf_dot.lnp = lnp3;
	else if(wfp->wf_dot.lnp == lnp2) {
		wfp->wf_dot.lnp = lnp3;
		wfp->wf_dot.off += lnp1->l_used;
		}
	}

// Delete a newline from current buffer -- join the current line with the next line.  Easy cases can be done by shuffling data
// around.  Hard cases require that lines be moved about in memory.  Called by ldelete() only.  It is assumed that there will
// always be at least two lines in the current buffer when this routine is called and the current line will never be the last
// line of the buffer (which does not have a newline delimiter).
static int ldelnewline(void) {
	char *str1,*str2;
	Line *lnp1,*lnp2,*lnp3;
	EScreen *scrp;
	EWindow *winp;
	Mark *mkp;

	lnp1 = si.curwp->w_face.wf_dot.lnp;
	lnp2 = lnp1->l_nextp;

	// Do simple join if room in current line for next line.
	if(lnp2->l_used <= lnp1->l_size - lnp1->l_used) {
		str1 = lnp1->l_text + lnp1->l_used;
		str2 = lnp2->l_text;
		while(str2 != lnp2->l_text + lnp2->l_used)
			*str1++ = *str2++;

		// In all screens...
		scrp = si.sheadp;
		do {
			// In all windows...
			winp = scrp->s_wheadp;
			do {
				fixdelnl1(&winp->w_face,lnp1,lnp2);
				} while((winp = winp->w_nextp) != NULL);
			} while((scrp = scrp->s_nextp) != NULL);

		// In current buffer...
		fixdelnl1(&si.curbp->b_face,lnp1,lnp2);
		mkp = &si.curbp->b_mroot;
		do {
			if(mkp->mk_dot.lnp == lnp2) {
				mkp->mk_dot.lnp = lnp1;
				mkp->mk_dot.off += lnp1->l_used;
				}
			} while((mkp = mkp->mk_nextp) != NULL);

		lnp1->l_used += lnp2->l_used;
		lunlink(lnp2,NULL);
		return rc.status;
		}

	// Simple join not possible, get more space.
	if(lalloc(lnp1->l_used + lnp2->l_used,&lnp3) != Success)
		return rc.status;	// Fatal error.
	str1 = lnp1->l_text;
	str2 = lnp3->l_text;
	while(str1 != lnp1->l_text + lnp1->l_used)
		*str2++ = *str1++;
	str1 = lnp2->l_text;
	while(str1 != lnp2->l_text + lnp2->l_used)
		*str2++ = *str1++;

	// Replace lnp1 and lnp2 with lnp3.
	lreplace2(lnp1,lnp2,lnp3);

	// In all screens...
	scrp = si.sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			fixdelnl2(&winp->w_face,lnp1,lnp2,lnp3);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In current buffer...
	fixdelnl2(&si.curbp->b_face,lnp1,lnp2,lnp3);
	mkp = &si.curbp->b_mroot;
	do {
		if(mkp->mk_dot.lnp == lnp1)
			mkp->mk_dot.lnp = lnp3;
		else if(mkp->mk_dot.lnp == lnp2) {
			mkp->mk_dot.lnp = lnp3;
			mkp->mk_dot.off += lnp1->l_used;
			}
		} while((mkp = mkp->mk_nextp) != NULL);

	return rc.status;
	}

// Fix dot offset after delete.
static void fixdotdel1(int offset,int chunk,Dot *dotp) {
	int delta;

	if(dotp->off > offset) {
		if(chunk >= 0) {
			delta = dotp->off - offset;
			dotp->off -= (chunk < delta ? chunk : delta);
			}
		else
			dotp->off += chunk;
		}
	else if(chunk < 0 && (delta = chunk + (offset - dotp->off)) < 0)
		dotp->off += delta;
	}

// Fix dot after delete in other windows with the same text displayed.
static void fixdotdel(Line *lnp,int offset,int chunk) {
	EScreen *scrp;
	EWindow *winp;
	Mark *mkp;

	// In all screens...
	scrp = si.sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp->w_face.wf_dot.lnp == lnp)
				fixdotdel1(offset,chunk,&winp->w_face.wf_dot);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In current buffer...
	if(si.curbp->b_face.wf_dot.lnp == lnp)
		fixdotdel1(offset,chunk,&si.curbp->b_face.wf_dot);
	mkp = &si.curbp->b_mroot;
	do {
		if(mkp->mk_dot.lnp == lnp)
			fixdotdel1(offset,chunk,&mkp->mk_dot);
		} while((mkp = mkp->mk_nextp) != NULL);
	}

// This function deletes up to n characters from the current buffer, starting at point.  The text that is deleted may include
// newlines.  Positive n deletes forward; negative n deletes backward.  Returns current status if all of the characters were
// deleted, NotFound (bypassing rcset()) if they were not (because dot ran into a buffer boundary), or the appropriate status if
// an error occurred.  The deleted text is put in the kill ring if the EditKill flag is set, the undelete buffer if the EditDel
// flag is set, otherwise discarded.
int ldelete(long n,ushort flags) {

	if(n == 0 || allowedit(true) != Success)	// Don't allow if buffer in read-only mode.
		return rc.status;

	ushort wflags;
	char *str1,*str2;
	Line *lnp;
	int offset,chunk;
	RingEntry *rep;
	DStrFab sf;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	bool hitEOB = false;

	// Set kill buffer pointer.
	rep = (flags & EditKill) ? kring.r_entryp : (flags & EditDel) ? &undelbuf : NULL;
	wflags = 0;

	// Going forward?
	if(n > 0) {
		if(rep != NULL && dopenwith(&sf,&rep->re_data,SFAppend) != 0)
			goto LibFail;
		do {
			// Get the current line.
			lnp = dotp->lnp;
			offset = dotp->off;

			// Figure out how many characters to delete on this line.
			if((chunk = lnp->l_used - offset) > n)
				chunk = n;

			// If at the end of a line, merge with the next.
			if(chunk == 0) {

				// Can't delete past end of buffer.
				if(lnp->l_nextp == NULL)
					goto EOB;

				// Flag that we are making a hard change and delete newline.
				wflags |= WFHard;
				if(ldelnewline() != Success)
					return rc.status;
				if(rep != NULL && dputc('\n',&sf) != 0)
					goto LibFail;
				--n;
				continue;
				}

			// Flag the fact we are changing the current line.
			wflags |= WFEdit;

			// Find the limits of the kill.
			str1 = lnp->l_text + offset;
			str2 = str1 + chunk;

			// Save the text to the kill buffer.
			if(rep != NULL && chunk > 0 && dputmem((void *) str1,chunk,&sf) != 0)
				goto LibFail;

			// Shift what remains on the line leftward.
			while(str2 < lnp->l_text + lnp->l_used)
				*str1++ = *str2++;
			lnp->l_used -= chunk;

			// Fix any other windows with the same text displayed.
			fixdotdel(lnp,offset,chunk);

			// We have deleted "chunk" characters.  Update counter.
			n -= chunk;
			} while(n > 0);
		}
	else {
		if(rep != NULL && dopenwith(&sf,&rep->re_data,SFPrepend) != 0)
			goto LibFail;
		do {
			// Get the current line.
			lnp = dotp->lnp;
			offset = dotp->off;

			// Figure out how many characters to delete on this line.
			if((chunk = offset) > -n)
				chunk = -n;

			// If at the beginning of a line, merge with the previous one.
			if(chunk == 0) {

				// Can't delete past beginning of buffer.
				if(lnp == si.curbp->b_lnp)
					goto EOB;

				// Flag that we are making a hard change and delete the newline.
				wflags |= WFHard;
				(void) movech(-1);
				if(ldelnewline() != Success)
					return rc.status;
				if(rep != NULL && dputc('\n',&sf) != 0)
					goto LibFail;
				++n;
				continue;
				}

			// Flag the fact we are changing the current line.
			wflags |= WFEdit;

			// Find the limits of the kill.
			str1 = lnp->l_text + offset;
			str2 = str1 - chunk;

			// Save the text to the kill buffer.
			if(rep != NULL && dputmem((void *) str2,chunk,&sf) != 0)
				goto LibFail;

			// Shift what remains on the line leftward.
			while(str1 < lnp->l_text + lnp->l_used)
				*str2++ = *str1++;
			lnp->l_used -= chunk;
			dotp->off -= chunk;

			// Fix any other windows with the same text displayed.
			fixdotdel(lnp,offset,-chunk);

			// We have deleted "chunk" characters.  Update counter.
			n += chunk;
			} while(n < 0);
		}
	goto Retn;
EOB:
	hitEOB = true;
Retn:
	if(rep != NULL && dclose(&sf,sf_string) != 0)
		goto LibFail;
	if(wflags)
		bchange(si.curbp,wflags);
	return hitEOB ? NotFound : rc.status;
LibFail:
	return librcset(Failure);
	}

// Quote the next character, and insert it into the buffer.  All characters are taken literally, including newline, which does
// not then have its line-splitting meaning.  The character is always read, even if it is inserted zero times, so that the
// command completes normally.  If a function key is pressed, its symbolic name is inserted.
int quoteChar(Datum *rp,int n,Datum **argpp) {
	ushort ek;			// Key fetched.

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)			// Fail on a negative argument.
		return rcset(Failure,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"

	// Get the key.
	if(getkey(&ek,true) != Success)
		return rc.status;

	if(n > 0) {
		// If this is a function or special key, put its name in.
		if(ek & (FKey | Shft)) {
			int len;
			char keybuf[16];

			len = strlen(ektos(ek,keybuf,false));
			do {
				if(overprep(len) != Success || linstr(keybuf) != Success)
					break;
				} while(--n > 0);
			}

		// Otherwise, just insert the raw character n times.
		else if(overprep(n) == Success)
			(void) linsert(n,ektoc(ek,false));
		}

	return rc.status;
	}

// Set soft tab size to abs(n) if n <= 0; otherwise, insert a hard tab or spaces into the buffer n times.  If inserting soft
// tab(s), one or more spaces are inserted for each so that the first non-space character following point (or the newline) is
// pushed forward to the next tab stop.
int instab(int n) {

	// Non-positive n?
	if(n <= 0)
		// Set soft tab size.
		(void) settab(abs(n),false);

	// Positive n.  Insert hard tab or spaces n times.
	else if(si.stabsize == 0)
		(void) linsert(n,'\t');
	else {
		// Scan forward from point to next non-space character.
		Dot dot = si.curwp->w_face.wf_dot;
		int len = dot.lnp->l_used;
		while(dot.off < len && dot.lnp->l_text[dot.off] == ' ')
			++dot.off;

		// Set len to size of first tab and loop n times.
		len = si.stabsize - getccol(&dot) % si.stabsize;
		do {
			if(linsert(len,' ') != Success)
				break;
			len = si.stabsize;
			} while(--n > 0);
		}

	return rc.status;
	}

// Finish a routine that modifies a block of lines.
static int lchange(Dot *odotp,bool backward,int count) {
	Mark *mkp;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	kentry.thisflag &= ~CFVMove;		// Flag that this resets the goal column.
	if(backward) {				// Set mark at beginning of line block and move point to end.
		dotp->off = 0;
		if(!bufbegin(dotp))
			(void) moveln(1);
		mset(&si.curbp->b_mroot,si.curwp);
		*dotp = *odotp;
		dotp->off = 0;
		(void) moveln(1);
		}

	// Scan marks in current buffer and adjust offset if needed.
	mkp = &si.curbp->b_mroot;
	do {
		dotp = &mkp->mk_dot;
		if(dotp->off > dotp->lnp->l_used)
			dotp->off = dotp->lnp->l_used;
		} while((mkp = mkp->mk_nextp) != NULL);

	// Set window flags and return.
	if(count == 0)
		si.curwp->w_flags |= WFMove;
	else
		bchange(si.curbp,WFHard);	// Flag that a line other than the current one was changed.

	return rcset(Success,0,text307,count,count == 1 ? "" : "s",text260);
			// "%d line%s %s"			// "changed"
	}

// Get (hard) tab size into *tsp for detabLine() and entabLine().
int getTabSize(Datum **argpp,int *tsp) {
	int tabsz;

	if(si.opflags & OpScript) {
		if((*argpp)->d_type == dat_nil) {
			*tsp = si.htabsize;
			goto Retn;
			}
		tabsz = (*argpp)->u.d_int;
		}
	else {
		Datum *datp;
		long lval;
		char wkbuf[4];
		TermInp ti = {wkbuf,RtnKey,0,NULL};

		if(dnewtrk(&datp) != 0)
			return librcset(Failure);
		sprintf(wkbuf,"%d",si.htabsize);
		if(terminp(datp,text393,ArgNotNull1 | ArgNil1,0,&ti) != Success)
				// "Tab size"
			goto Retn;
		if(datp->d_type == dat_nil)
			return Cancelled;
		if(asc_long(datp->d_str,&lval,false) != Success)
			goto Retn;
		tabsz = lval;
		}
	if(chktab(tabsz,true) == Success)
		*tsp = tabsz;
Retn:
	return rc.status;
	}

// Change tabs to spaces in a block of lines.  If n is zero, use lines in current region.  No error if attempt to move past end
// of buffer.  Always leave point at end of line block.
int detabLine(Datum *rp,int n,Datum **argpp) {
	int inc,len,tabsz;
	int count;
	bool changed;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	Dot dot = *dotp;	// Original dot position.

	// Get hard tab size.
	if(getTabSize(argpp,&tabsz) != Success)
		return rc.status;

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n) != Success)
		return rc.status;
	if(n < 0) {
		--n;
		inc = -1;
		}
	else
		inc = 1;

	dotp->off = 0;		// Start at the beginning.
	if(n > 0)
		mset(&si.curbp->b_mroot,si.curwp);

	// Loop thru text, detabbing n lines.
	kentry.lastflag &= ~CFVMove;
	for(count = 0; n != 0; n -= inc) {
		changed = false;

		// Detab the entire current line.
		while(dotp->off < dotp->lnp->l_used) {

			// If we have a tab.
			if(dotp->lnp->l_text[dotp->off] == '\t') {
				len = tabsz - (dotp->off % tabsz);
				if(ldelete(1L,0) != Success || insnlspace(-len,EditSpace | EditHoldPt) != Success)
					return rc.status;
				dotp->off += len;
				changed = true;
				}
			else
				++dotp->off;
			}
		if(changed)
			++count;

		// Move forward or backward to the next line.
		dotp->off = 0;
		if(moveln(inc) != Success)
			break;
		}

	return lchange(&dot,inc == -1,count);
	}

// Define column-calculator macro.
#define nextab(a)	(a - (a % tabsz)) + tabsz

// Change spaces to tabs (where possible) in a block of lines.  If n is zero, use lines in current region.  No error if attempt
// to move past end of buffer.  Always leave point at end of line block.  Return status.
int entabLine(Datum *rp,int n,Datum **argpp) {
	int inc;		// Increment to next line.
	int fspace;		// Index of first space if in a run.
	int ccol;		// Current point column.
	int tabsz,len,count;
	bool changed;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	Dot dot = *dotp;	// Original dot position.

	// Get hard tab size.
	if(getTabSize(argpp,&tabsz) != Success)
		return rc.status;

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n) != Success)
		return rc.status;
	if(n < 0) {
		--n;
		inc = -1;
		}
	else
		inc = 1;

	dotp->off = 0;		// Start at the beginning.
	if(n > 0)
		mset(&si.curbp->b_mroot,si.curwp);

	// Loop thru text, entabbing n lines.
	kentry.lastflag &= ~CFVMove;
	for(count = 0; n != 0; n -= inc) {

		// Entab the entire current line.
		ccol = 0;
		fspace = -1;
		changed = false;

		while(dotp->off <= dotp->lnp->l_used) {

			// Time to compress?
			if(fspace >= 0 && nextab(fspace) <= ccol) {

				// Yes.  Skip if just a single space; otherwise, chaos ensues.
				if((len = ccol - fspace) >= 2) {
					dotp->off -= len;
					if(ldelete((long) len,0) != Success || linsert(1,'\t') != Success)
						return rc.status;
					changed = true;
					}
				fspace = -1;
				}
			if(dotp->off == dotp->lnp->l_used)
				break;

			// Get the current character and check it.
			switch(dotp->lnp->l_text[dotp->off]) {

				case '\t':	// A tab... expand it and fall through.
					if(ldelete(1L,0) != Success ||
					 insnlspace(-(tabsz - (ccol % tabsz)),EditSpace | EditHoldPt) != Success)
						return rc.status;
					changed = true;

				case ' ':	// A space... beginning of run?
					if(fspace == -1)
						fspace = ccol;
					break;

				default:	// Any other char... reset.
					fspace = -1;
				}
			++ccol;
			++dotp->off;
			}
		if(changed)
			++count;

		// Move forward or backward to the next line.
		dotp->off = 0;
		if(moveln(inc) != Success)
			break;
		}

	return lchange(&dot,inc == -1,count);
	}

// Get indentation of given line.  Store indentation in *dpp if found; otherwise, set to NULL.  Return status.
static int getindent(Datum **dpp,Line *lnp) {
	char *str,*strz;

	// Find end of indentation.
	strz = (str = lnp->l_text) + lnp->l_used;
	while(str < strz) {
		if(*str != ' ' && *str != '\t')
			break;
		++str;
		}

	// Save it if found.
	if(str == lnp->l_text)
		*dpp = NULL;
	else if(dnewtrk(dpp) != 0 || dsetsubstr(lnp->l_text,str - lnp->l_text,*dpp) != 0)
		return librcset(Failure);

	return rc.status;
	}

// Insert newline or space character abs(n) times.  If EditSpace flag set, insert space(s); otherwise, newline(s).  If EditWrap
// flag set, do word wrapping if applicable.  If EditHoldPt flag set, insert or replace character(s) ahead of point.  If n < 0,
// ignore "Over" and "Repl" buffer modes (force insert).  Commands that call this routine (+ self-insert) and their actions per
// n argument are as follows:
//
// Command        defn                           n < 0                          n > 0
// -------------  -----------------------------  -----------------------------  -------------
// newline        langnl(), wrap, insert         Insert n times                 langnl() n times -OR- wrap, then insert n times
// openLine       Insert, hold point             Insert n times, move point to  Insert n times, hold point
//                                                first empty line if possible
// (self insert)  lang insert -OR- replace       Insert n times                 lang insert n times -OR- replace n times
// space          Wrap, replace                  Insert n times                 Wrap, then replace n times
// insertSpace    Replace, hold point            Insert n times, hold point     Replace n times, hold point
int insnlspace(int n,ushort flags) {

	if(n != 0) {
		if(n == INT_MIN)
			n = 1;

		// If EditWrap flag, positive n, "Wrap" mode is enabled, "Over" and "Repl" modes are disabled, wrap
		// column is defined, and we are now past wrap column, execute user-assigned wrap hook.
		if((flags & EditWrap) && n > 0 && modeset(MdIdxWrap,NULL) &&
		 !bmsrch(si.curbp,false,2,mi.cache[MdIdxOver],mi.cache[MdIdxRepl]) &&
		 si.wrapcol > 0 && getccol(NULL) > si.wrapcol)
			if(exechook(NULL,INT_MIN,hooktab + HkWrap,0) != Success)
				return rc.status;

		// If a space, positive n, and replace or overwrite mode, delete before insert.
		if((flags & EditSpace) && n > 0 && overprep(n) != Success)
			return rc.status;

		// Insert some lines or spaces.
		if(!(flags & EditSpace)) {
			int count = abs(n);
			do {
				if(lnewline() != Success)
					return rc.status;
				} while(--count > 0);
			}
		else if(linsert(abs(n),' ') != Success)
			return rc.status;
		if(flags & EditHoldPt)
			(void) movech(-abs(n));
		}

	return rc.status;
	}

// Trim trailing whitespace from current line.  Return 1 if any whitespace was found; otherwise, 0.
static int trimln(void) {
	char *str;
	int len;
	Line *lnp = si.curwp->w_face.wf_dot.lnp;

	for(str = lnp->l_text + lnp->l_used; str > lnp->l_text; --str) {
		if(str[-1] != ' ' && str[-1] != '\t')
			break;
		}
	if((len = str - lnp->l_text) == lnp->l_used)
		return 0;
	lnp->l_used = len;
	return 1;
	}

// Trim trailing whitespace from a block of lines.  If n is zero, use lines in current region.  No error if attempt to move past
// end of buffer.  Always leave point at end of line block.
int trimLine(Datum *rp,int n,Datum **argpp) {
	int inc,count;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	Dot dot = *dotp;	// Original dot position.

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n) != Success)
		return rc.status;
	if(n < 0) {
		--n;
		inc = -1;
		}
	else
		inc = 1;

	dotp->off = 0;		// Start at the beginning.
	if(n > 0)
		mset(&si.curbp->b_mroot,si.curwp);

	// Loop through text, trimming n lines.
	kentry.lastflag &= ~CFVMove;
	for(count = 0; n != 0; n -= inc) {
		count += trimln();

		// Move forward or backward to the next line.
		dotp->off = 0;
		if(moveln(inc) != Success)
			break;
		}

	return lchange(&dot,inc == -1,count);
	}

// Open up some blank space.  The basic plan is to insert a bunch of newlines, and then back up over them.  Everything is done
// by the subcommand processors.  They even handle the looping.
int openLine(Datum *rp,int n,Datum **argpp) {
	int i;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"

	i = n;				// Insert newlines.
	do {
		if(lnewline() != Success)
			return rc.status;
		} while(--i);
	(void) movech(-n);		// Backup over them all.
	return rc.status;
	}

// Insert a line at [-]nth line with same indentation.
int insertLineI(Datum *rp,int n,Datum **argpp) {

	// Non-empty buffer?
	if(!bempty(NULL)) {
		Datum *dsinkp,*indentp;
		Dot *dotp = &si.curwp->w_face.wf_dot;

		if(dnewtrk(&dsinkp) != 0)
			return librcset(Failure);
		(void) beline(dsinkp,n,false);			// Move to beginning of target line...
		if(getindent(&indentp,dotp->lnp) != Success ||	// get indentation...
		 lnewline() != Success)				// insert a newline...
			return rc.status;
		(void) movech(-1);				// back up over it...
		if(indentp != NULL)				// and insert saved indentation.
			(void) linstr(indentp->d_str);
		}

	return rc.status;
	}

// Compute and insert "i" variable at point n times.
int inserti(Datum *rp,int n,Datum **argpp) {
	int i;
	char *str;
	static char myname[] = "inserti";

	if(n == INT_MIN)
		n = 1;
	i = ivar.i;

	// Going forward?
	if(n > 0)
		do {
			asprintf(&str,ivar.format.d_str,i);
			if(str == NULL)
				return rcset(Panic,0,text94,myname);
					// "%s(): Out of memory!"
			(void) linstr(str);
			(void) free((void *) str);
			if(rc.status != Success)
				return rc.status;
			i += ivar.inc;
			} while(--n > 0);
	else {
		int len;

		do {
			len = asprintf(&str,ivar.format.d_str,i);
			if(str == NULL)
				return rcset(Panic,0,text94,myname);
					// "%s(): Out of memory!"
			(void) linstr(str);
			(void) free((void *) str);
			if(rc.status != Success)
				return rc.status;
			(void) movech(-len);
			i += ivar.inc;
			} while(++n < 0);
		}

	ivar.i = i;
	return rc.status;
	}

// Delete blank lines around point.  If point is on a blank line, this command deletes all the blank lines above and below the
// current line.  If it is on a non-blank line, then it deletes all of the blank lines after the line.
int deleteBlankLines(Datum *rp,int n,Datum **argpp) {
	Line *lnp1,*lnp2;
	long count;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Only one line in buffer?
	if((lnp1 = si.curbp->b_lnp)->l_nextp == NULL) {
		if(lnp1->l_used == 0 || !is_white(lnp1,lnp1->l_used))
			return rc.status;
		count = lnp1->l_used;
		}
	else {
		// Two or more lines in buffer.  Find first non-blank line going backward.
		for(lnp1 = dotp->lnp; is_white(lnp1,lnp1->l_used); lnp1 = lnp1->l_prevp)
			if(lnp1 == si.curbp->b_lnp)
				break;

		// Nothing to do if line is last line in buffer.
		if(lnp1->l_nextp == NULL)
			return rc.status;

		// lnp1 not last line.  Find first non-blank line going forward, counting characters as we go.
		lnp2 = lnp1;
		count = 0;
		for(;;) {
			lnp2 = lnp2->l_nextp;
			if(!is_white(lnp2,lnp2->l_used))
				break;
			count += lnp2->l_used + 1;
			if(lnp2->l_nextp == NULL) {
				--count;
				break;
				}
			}

		// Handle special case where first buffer line is blank.
		if(is_white(lnp1,lnp1->l_used)) {
			dotp->lnp = lnp1;
			count += lnp1->l_used + 1;
			}
		else {
			if(count == 0)
				return rc.status;
			dotp->lnp = lnp1->l_nextp;
			}
		}

	// Finally, move point to beginning of first blank line and delete all the white space.
	dotp->off = 0;
	return ldelete(count,0);
	}

// Insert a newline, then enough tabs and spaces to duplicate the indentation of the previous line.  Tabs are every si.htabsize
// characters.  Quite simple.  Figure out the indentation of the current line.  Insert a newline by calling the standard
// routine.  Insert the indentation by inserting the right number of tabs and spaces.  Return status.  Normally bound to ^J.
int newlineI(Datum *rp,int n,Datum **argpp) {
	Datum *indentp;
	Line *lnp = si.curwp->w_face.wf_dot.lnp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"

	// Get the indentation of the current line, if any.
	if(getindent(&indentp,lnp) != Success)
		return rc.status;

	// Insert lines with indentation in the final line.
	do {
		if(lnewline() != Success || (indentp != NULL && n == 1 && linstr(indentp->d_str) != Success))
			break;
		} while(--n > 0);

	return rc.status;
	}

// Delete hard tabs or "chunks" of spaces forward or backward.  Return status.  Soft tabs are deleted such that the next
// non-space character (or end of line) past dot is moved left until it reaches a tab stop, one or more times.  To accomplish
// this, spaces are deleted beginning at dot going forward (if n > 0) and dot does not move, or spaces are deleted prior to dot
// going backward (if n < 0) and dot follows.  If n < 0 and "force" is true (called by "backspace" command), deletions are done
// iteratively up to abs(n) times such that either one soft tab or one character (including a newline) is deleted in each
// iteration.
int deltab(int n,bool force) {
	int off,i,direc;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Check for "do nothing" cases.
	if(n == INT_MIN)
		n = 1;
	if((i = dotp->lnp->l_used) == 0 || ((off = dotp->off) == 0 && n < 0) || (off == i && n > 0))
		return rc.status;

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
	if(si.stabsize == 0) {
		n = abs(n);
		while((off = dotp->off + i) >= 0 && off < dotp->lnp->l_used && dotp->lnp->l_text[off] == '\t')
			if(ldelete(direc,0) != Success || --n == 0)
				break;
		}
	else {
		// Process soft tab(s).  Loop until n == 0 if force is true; otherwise, just do just one iteration.
		i = -direc;
		do {
			// Delete spaces until the next non-space character to the right of dot (if deleting forward) or at or
			// to the right of dot (if deleting backward) is positioned at the next tab stop to its left, assuming
			// that position is at or to the right of dot.  If deleting forward, begin at dot and go rightward; if
			// deleting backward; begin at dot - 1 and go leftward.  If this is not possible (because the current
			// run of spaces is too short), do nothing.  However, if deleting backward, dot is at a tab stop, and a
			// short run of spaces precedes it, delete the run.

			//  ....+....1....+....2....+....3....+....4....+....5....+....6....+....7....+....8
			//  ^   ^   ^   ^   ^   ^   ^   ^   ^   ^	Tab stops if tab size is 4.
			//  ab      x   The end is near    Weird.	Sample text.

			// Bail out if deleting backward, not a force, and at BOL or the character just prior to dot is not a
			// space; otherwise, go straight to char-delete if at BOL and a force.
			if(n < 0) {
				if(!force) {
					if(dotp->off == 0 || dotp->lnp->l_text[dotp->off - 1] != ' ')
						break;
					}
				else if(dotp->off == 0)
					goto Nuke1;
				}

			// Save column position of dot and scan forward to next non-space character.
			int dotcol = getccol(NULL);
			int len = dotp->lnp->l_used;
			for(off = dotp->off; dotp->off < len && dotp->lnp->l_text[dotp->off] == ' '; ++dotp->off);

			// Bail out if deleting forward and run length (len) is zero.
			if(n > 0 && (len = dotp->off - off) == 0)
				break;
			int col1,col2,chunk1;

			// Get column position of non-space character (col2), calculate position of prior tab stop (col1), and
			// calculate size of first "chunk" (distance from prior tab stop to non-space character).
			col1 = ((col2 = getccol(NULL)) - 1) / si.stabsize * si.stabsize;
			chunk1 = col2 - col1;

			// If deleting forward...
			if(n > 0) {
				// If prior tab stop is before dot, stop here if non-space character is not a tab stop or dot is
				// not at BOL and the character just prior to dot is a space.
				if(col1 < dotcol && (col2 % si.stabsize != 0 ||
				 (off > 0 && dotp->lnp->l_text[off - 1] == ' '))) {
					dotp->off = off;
					break;
					}
				goto Nuke;
				}

			// Deleting backward.  Scan backward to first non-space character.
			for(dotp->off = off; dotp->off > 0 && dotp->lnp->l_text[dotp->off - 1] == ' '; --dotp->off);

			// Continue only if run (len) is not too short or dot is at a tab stop.
			len = off - dotp->off;
			dotp->off = off;		// Restore original point position.
			if(len >= chunk1 || (len > 0 && dotcol % si.stabsize == 0)) {
Nuke:;
				// Delete one or more soft tabs.  Determine number of spaces to delete and nuke 'em.
				int max = (len - chunk1) / si.stabsize;
				int m = abs(n) - 1;
				m = (m > max ? max : m);
				if(ldelete((long) -(len < chunk1 ? len : (chunk1 + m * si.stabsize)),0) != Success)
					return rc.status;
				if(n > 0)
					dotp->off = off;
				if(!force)
					break;
				n += (m + 1) * i;
				}
			else if(!force)
				break;
			else {
Nuke1:
				// Force-deleting backward and no tab found.  Delete one character instead.
				if(ldelete(-1L,0) != Success)
					return rc.status;
				++n;
				}
			} while(n != 0);
		}

	return rc.status;
	}

// Kill, delete, or copy text for kdc values of -1, 0, or 1, respectively.  Save text to kill ring if kdc is non-zero.  If regp
// is not NULL, operate on the given region (assumed to not be empty); otherwise, operate on text selected by n via call to
// gettregion().  Return status.
int kdctext(int n,int kdc,Region *regp) {
	Region region;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Process region elsewhere if specified.
	if(regp != NULL) {
		if(kdc > 0)
			goto Kopy;
		if(kprep(kdc) != Success)
			return rc.status;
		*dotp = regp->r_dot;
		return ldelete(regp->r_size,kdc ? EditKill : EditDel);
		}

	// No region... process lines and make one.  Error if region is empty.
	gettregion(dotp,n,&region,0);
	if(region.r_size == 0)
		return rcset(Failure,RCNoFormat,text259);
				// "No text selected"

	// Kill, delete, or copy text.
	if(kdc <= 0) {

		// Kill or delete.
		if(kprep(kdc) == Success)
			(void) ldelete(region.r_size,kdc ? EditKill : EditDel);
		return rc.status;
		}

	// Copy.
	regp = &region;
Kopy:
	return copyreg(regp) != Success ? rc.status : rcset(Success,RCForce,"%s %s",text261,text262);
									// "Text","copied"
	}

// Kill, delete, or copy whole line(s) via kdctext() for kdc values of -1, 0, or 1, respectively.  Called from execCF() for
// killLine, deleteLine, and copyLine commands.
int kdcline(int n,int kdc) {
	Region region;

	// Convert line block to a region.
	if(getlregion(n,&region,RegInclDelim) != Success)
		return rc.status;

	// Nuke or copy line block (as a region) via kdctext().
	if(kdc <= 0)
		si.curwp->w_face.wf_dot = region.r_dot;
	return kdctext(0,kdc,&region) != Success || kdc <= 0 ? rc.status :
	 rcset(Success,RCForce,text307,region.r_linect,region.r_linect == 1 ? "" : "s",text262);
						// "%d line%s %s"		// "copied"
	}

// Duplicate whole line(s) in the current buffer, leaving point at beginning of second line block.  Return status.
int dupLine(Datum *rp,int n,Datum **argpp) {
	Region region;

	// Convert line block to a region.
	if(getlregion(n,&region,RegInclDelim) != Success)
		return rc.status;

	// Copy region to a local (stack) variable and append a newline if absent (a line block at EOB).
	char *strz,wkbuf[region.r_size + 2];
	if((strz = regcpy(wkbuf,&region))[-1] != '\n') {
		strz[0] = '\n';
		strz[1] = '\0';
		}

	// Move dot to beginning of line block and insert it.
	si.curwp->w_face.wf_dot = region.r_dot;
	if(linstr(wkbuf) == Success)
		(void) begintxt();
	return rc.status;
	}

// Select [-]N lines (default 1, region lines if N == 0) or text if n > 0 (where N is function argument), move point to
// first line of block, and return number of lines selected.  If n < 0, omit delimiter of last line selected.  Return status.
int selectLine(Datum *rp,int n,Datum **argpp) {
	Region region;

	// Get region.
	if(n > 0)
		gettregion(&si.curwp->w_face.wf_dot,argpp[0]->u.d_int,&region,RegForceBegin);
	else if(getlregion(argpp[0]->u.d_int,&region,n < 0 && n != INT_MIN ? RegEmptyOK | RegLineSelect :
	 RegInclDelim | RegEmptyOK | RegLineSelect) != Success)
		return rc.status;

	// If region not empty, move point to beginning of region and set region in buffer.
	if(region.r_size > 0) {
		movept(&region.r_dot);
		(void) movech(region.r_size);
		mset(&si.curbp->b_mroot,si.curwp);
		si.curwp->w_face.wf_dot = region.r_dot;
		}

	// Return line count.
	dsetint((long) region.r_linect,rp);
	return rc.status;
	}

// Delete white space at and forward from point (if any) on current line.  If n > 0, include non-word characters as well.
static int delwhitef(int n) {
	short c;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	for(;;) {
		if(dotp->off == dotp->lnp->l_used)
			break;
		c = dotp->lnp->l_text[dotp->off];
		if(c != ' ' && c != '\t' && (n <= 0 || wordlist[c]))
			break;
		if(ldelete(1L,0) != Success)
			break;
		}

	return rc.status;
	}

// Delete white space surrounding point on current line.  If prior is true, delete white space immediately before point also.
// If n > 0, include non-word characters as well.
int delwhite(int n,bool prior) {
	short c;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	Line *lnp = dotp->lnp;

	if(lnp->l_used > 0 && (prior || dotp->off == lnp->l_used || (c = lnp->l_text[dotp->off]) == ' ' || c == '\t' ||
	 (n > 0 && !wordlist[(int) c]))) {

		// Delete backward.
		for(;;) {
			if(dotp->off == 0)
				break;
			c = lnp->l_text[dotp->off - 1];
			if(c != ' ' && c != '\t' && (n <= 0 || wordlist[c]))
				break;
			if(ldelete(-1L,0) != Success)
				goto Retn;
			}

		// Delete forward.
		(void) delwhitef(n);
		}
Retn:
	return rc.status;
	}

// Join adjacent line(s), replacing all white space in between with (1), nothing if "delimp" is nil; or (2), a single space
// character (unless either line is blank or all white space) and insert extra space character if first of two adjacent lines
// ends with any character specified in "delimp".  Return status.
static int joinln(Datum *rp,int n,Datum *delimp) {
	int m,incr,newdot;
	bool insSpace = (delimp == NULL || delimp->d_type != dat_nil);
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Determine bounds of line block.
	if(n == INT_MIN)
		n = -1;						// Join with one line above by default.
	else if(n == 1 ||
	 (n == 0 && (reglines(&n) != Success || n == 1)))	// Join all lines in region.
		return rc.status;				// Nothing to do if n == 1 or region line count is 1.

	// Get ready.
	if(n > 0) {						// n is >= 2.
		incr = 1;
		(void) trimln();
		newdot = dotp->lnp->l_used;			// Save current end-of-line position.
		--n;
		}
	else
		incr = -1;

	// Join lines forward or backward.
	do {
		// Move to beginning of first line of pair if needed, trim white space, move to end, delete newline, delete
		// white space, and if delimp not nil and not at beginning or end of new line, insert one or two spaces.
		if(incr == -1) {
			if(dotp->lnp == si.curbp->b_lnp)
				break;
			dotp->lnp = dotp->lnp->l_prevp;
			}
		else if(dotp->lnp->l_nextp == NULL)
			break;
		(void) trimln();
		dotp->off = dotp->lnp->l_used;
		if(ldelete(1L,0) != Success || delwhitef(0) != Success)
			return rc.status;
		if(insSpace && dotp->off > 0 && dotp->off < dotp->lnp->l_used) {
			m = 1;
			if(delimp != NULL && index(delimp->d_str,dotp->lnp->l_text[dotp->off - 1]) != NULL)
				++m;
			if(linsert(m,' ') != Success)
				return rc.status;
			}
		} while((n -= incr) != 0);
	if(incr > 0)
		dotp->off = newdot;				// Set current position.

	return rc.status;
	}

// Join adjacent line(s) via joinln(), passing argument value if script mode.
int joinLines(Datum *rp,int n,Datum **argpp) {
	Datum *delimp = NULL;

	// Get sentence-end characters if script mode.
	if((si.opflags & OpScript) && !disnull(argpp[0]))
		delimp = argpp[0];

	// Call joinln().
	return joinln(rp,n,delimp);
	}

// Kill, delete, or copy fenced region if kdc is -1, 0, or 1, respectively.  Return status.
bool kdcFencedRegion(int kdc) {
	Region region;

	if(otherfence(0,&region) == Success)
		(void) kdctext(0,kdc,&region);
	return rc.status;
	}

// Write text to a named buffer.
int bprint(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;

	// Negative repeat count is an error.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"

	// Get the buffer name.
	if((bufp = bsrch(argpp[0]->d_str,NULL)) == NULL)
		return rcset(Failure,0,text118,argpp[0]->d_str);
			// "No such buffer '%s'"
	if(!needsym(s_comma,true))
		return rc.status;

	return chgtext(rp,n,Txt_Insert,bufp);
	}

// Wrap word on white space.  Beginning at point going backward, stop on the first word break (space character) or column n
// (which must be >= 0).  If we reach column n and n is before point, jump back to the point position and (i), if default n:
// start a new line; or (ii), if not default n: scan forward to a word break (and eat all white space) or end of line and start
// a new line.  Otherwise, break the line at the word break or point, eat all white space, and jump forward to the point
// position.  Make sure we force the display back to the left edge of the current window.
int wrapWord(Datum *rp,int n,Datum **argpp) {
	int wordsz;		// Size of word wrapped to next line.
	int lmargin,origoff;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Determine left margin.
	if(n == INT_MIN)
		lmargin = 0;
	else if((lmargin = n) < 0)
		return rcset(Failure,0,text39,text322,n,0);
			// "%s (%d) must be %d or greater","Column number"

	// If blank line, do nothing.
	if(dotp->lnp->l_used == 0)
		return rc.status;

	// Scan backward to first space character, if any.
	origoff = dotp->off;
	wordsz = -1;
	do {
		// If we are at or past the left margin, start a new line.
		if(getccol(NULL) <= lmargin) {

			// Hunt forward for a break if non-default n.
			if(n == INT_MIN)
				dotp->off = origoff;
			else
				while(dotp->off < dotp->lnp->l_used) {
					(void) movech(1);
					if(dotp->lnp->l_text[dotp->off] == ' ') {
						if(delwhite(0,false) != Success)
							return rc.status;
						break;
						}
					}
			return lnewline();
			}

		// Back up one character.
		(void) movech(-1);
		++wordsz;
		} while(dotp->lnp->l_text[dotp->off] != ' ');

	// Found a space.  Replace it with a newline.
	if(delwhite(0,false) != Success || lnewline() != Success)
		return rc.status;

	// Move back to where we started.
	if(wordsz > 0)
		(void) movech(wordsz);

	// Make sure the display is not horizontally scrolled.
	if(si.curwp->w_face.wf_firstcol > 0) {
		si.curwp->w_face.wf_firstcol = 0;
		si.curwp->w_flags |= (WFHard | WFMode);
		}

	return rc.status;
	}

// Wrap line(s) in a block specified by n argument.  Duplicate indentation from first line in all subsequent lines.  If script
// mode, also add value of first argument after indentation (for example, "// " or "# "), and pass second argument to joinln().
// No error if attempt to move past a buffer boundary.
int wrapLine(Datum *rp,int n,Datum **argpp) {
	Dot *dotp;
	Datum *indentp = NULL;		// Leading white space on first line, if any.
	Datum *prefixp = NULL;		// First argument if script mode.
	Datum *delimp = NULL;		// Second argument if script mode.
	int indentcol,lmargin,col;
	int prefixLen = 0;

	// Wrap column set?
	if(si.wrapcol == 0)
		return rcset(Failure,RCNoFormat,text98);
			// "Wrap column not set"

	// Get prefix and end-sentence delimiters if script mode.
	if(si.opflags & OpScript) {
		if(!disnn(argpp[0])) {
			prefixp = argpp[0];
			if(*prefixp->d_str == ' ' || *prefixp->d_str == '\t')
				return rcset(Failure,0,text303,prefixp->d_str);
					// "Invalid wrap prefix \"%s\""
			else
				prefixLen = strlen(prefixp->d_str);
			}
		if(!disnn(argpp[1]))
			delimp = argpp[1];
		}

	// Determine bounds of line block.
	dotp = &si.curwp->w_face.wf_dot;
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n) != Success)			// Process all lines in region.
		return rc.status;
	else if(n < 0) {						// Back up to first line.
		int count = 1;
		while(dotp->lnp != si.curbp->b_lnp) {
			dotp->lnp = dotp->lnp->l_prevp;
			++count;
			if(++n == 0)
				break;
			}
		if((n = count) > 1)
			si.curwp->w_flags |= WFMove;
		}
	dotp->off = 0;							// Move to beginning of line.

	// Dot now at beginning of first line and n > 0.
	(void) begintxt();						// Move to beginning of text.
	if((indentcol = getccol(NULL)) + prefixLen >= si.wrapcol)		// Too much indentation?
		return rcset(Failure,0,text323,si.wrapcol);
			// "Indentation exceeds wrap column (%d)"
	if(dotp->off > 0) {						// Save any indentation (from first line of block)...
		if(dnewtrk(&indentp) != 0 || dsetsubstr(dotp->lnp->l_text,dotp->off,indentp) != 0)
			return librcset(Failure);
		if(ldelete(-dotp->off,0) != Success)			// and delete it.
			return rc.status;
		}

	// Remove any existing prefix string from each line of block.
	if(prefixLen > 0) {
		Dot odot = *dotp;
		int count = n;
		char *str = prefixp->d_str - 1;
		int striplen;

		// Get length of stripped prefix.
		for(striplen = prefixLen; striplen > 1 && (str[striplen] == ' ' || str[striplen] == '\t'); --striplen);
		do {
			(void) begintxt();
			if(dotp->lnp->l_used - dotp->off >= striplen && memcmp(dotp->lnp->l_text + dotp->off,prefixp->d_str,
			 striplen) == 0 && (ldelete(striplen,0) != Success || (count == n && delwhite(0,false) != Success)))
				return rc.status;
			} while((dotp->lnp = dotp->lnp->l_nextp) != NULL && --count > 0);
		*dotp = odot;
		}

	if(n > 1 && joinln(rp,n,delimp) != Success)			// Convert line block to single line.
		return rc.status;
	(void) trimln();						// Delete any trailing white space.
	if(dotp->lnp->l_used > 0) {
		short c;
		dotp->off = 0;
		lmargin = indentcol + prefixLen;

		// Wrap current line until too short to wrap any further.
		for(;;) {
			// Insert indentation and prefix string.
			if((indentp != NULL && linstr(indentp->d_str) != Success) ||
			 (prefixp != NULL && linstr(prefixp->d_str) != Success))
				return rc.status;
			col = lmargin;					// Move forward until hit end of line or first non-
									// whitespace character at or past wrap column.
			for(;;) {
				if(++dotp->off == dotp->lnp->l_used)	// At end of line?
					goto Retn;			// Yes, done.
				c = dotp->lnp->l_text[dotp->off];
				col = newcol(c,col);
				if(col >= si.wrapcol && (c != ' ' && c != '\t')) {	// At or past wrap column and not space?
					if(wrapWord(rp,lmargin,argpp) != Success)	// Yes, wrap.
						return rc.status;
					break;
					}
				}
			if(dotp->lnp->l_used == 0)			// Blank line after wrap?
				return ldelete(1L,0);			// Yes, done.
			dotp->off = 0;					// No, move to beginning of line and continue.
			}
		}
Retn:
	(void) movech(1);						// Can't fail.
	return rc.status;
	}

// Move point forward by the specified number of words (wcount) or characters (ccount), converting characters to upper or lower
// case (ultab != NULL) or words to title case (ultab == NULL).  Processing stops when either wcount or ccount reaches zero,
// both of which are assumed to be greater than zero.  If changedp not NULL, set *changedp to "changed" status; otherwise, set
// window update flag.
static void casetext(int wcount,long ccount,char *ultab,bool *changedp) {
	short c;
	bool firstc;
	bool changed = false;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	bool (*isul)(short c) = (ultab == NULL) ? NULL : (ultab == upcase) ? is_lower : is_upper;

	do {
		while(!inword()) {
			if(movech(1) != Success || --ccount == 0)
				goto Retn;
			}
		firstc = true;
		while(inword()) {
			c = dotp->lnp->l_text[dotp->off];
			if(ultab == NULL) {
				if(firstc == is_lower(c)) {
					dotp->lnp->l_text[dotp->off] = firstc ? upcase[c] : lowcase[c];
					changed = true;
					}
				}
			else if(isul(c)) {
				dotp->lnp->l_text[dotp->off] = ultab[c];
				changed = true;
				}
			firstc = false;
			if(movech(1) != Success || --ccount == 0)
				goto Retn;
			}
		} while(--wcount > 0);
Retn:
	if(changedp != NULL)
		*changedp = changed;
	else if(changed)
		bchange(si.curbp,WFHard);
	}

// Convert a block of lines to lower, title, or upper case.  If n is zero, use lines in current region.  If trantab is NULL, do
// title case.  No error if attempt to move past end of buffer.  Always leave point at end of line block.
static int caseline(int n,char *trantab) {
	bool changed;
	int inc,count;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	Dot dot = *dotp;	// Original dot position.

	// Compute block size.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n) != Success)
		return rc.status;
	if(n < 0) {
		--n;
		inc = -1;
		}
	else
		inc = 1;

	dotp->off = 0;		// Start at the beginning.
	if(n > 0)
		mset(&si.curbp->b_mroot,si.curwp);

	// Loop through buffer text, changing case of n lines.
	kentry.lastflag &= ~CFVMove;
	for(count = 0; n != 0; n -= inc) {

		// Process the current line from the beginning.
		if(dotp->lnp->l_used > 0) {
			dotp->off = 0;
			casetext(INT_MAX,dotp->lnp->l_used,trantab,&changed);
			if(changed)
				++count;
			}

		// Move forward or backward to the next line.
		dotp->off = 0;
		if(moveln(inc) != Success)
			break;
		}

	return lchange(&dot,inc == -1,count);
	}

// Convert text objects per flags: n words (CaseWord), n lines (CaseLine), or a region (CaseRegion) to lower case (CaseLower),
// title case (CaseTitle), or upper case (CaseUpper).  If n == 0 and CaseLine flag set, convert lines in current region.  If
// n < 0, convert words and lines backward from point; othewise, forward.  No error if attempt to move past end of buffer.  If
// converting words, point is left at ending position (forward or backward from point); if converting lines, point is left at
// end of line block (forward from point always); if converting a region, point is left at opposite end of region (for visible
// effect).  Return status.
int cvtcase(int n,ushort flags) {
	char *trantab = flags & CaseUpper ? upcase : flags & CaseLower ? lowcase : NULL;

	// Process region.
	if(flags & CaseRegion) {
		Region region;

		if(getregion(&region,RegForceBegin) == Success && region.r_size > 0) {
			bool backward = false;
			Dot *dotp = &si.curwp->w_face.wf_dot;

			mset(&si.curbp->b_mroot,si.curwp);
			if(region.r_dot.lnp != dotp->lnp || region.r_dot.off != dotp->off) {
				*dotp = region.r_dot;
				backward = true;
				}
			casetext(INT_MAX,region.r_size,trantab,NULL);
			if(backward)
				*dotp = region.r_dot;
			}
		}

	// Process line(s).
	else if(flags & CaseLine)
		(void) caseline(n,trantab);

	// Process word(s).
	else if(n != 0) {

		// If going backward, move point to starting position and process a region.
		if(n == INT_MIN)
			n = 1;
		else if(n < 0) {
			Region region;

			mset(&si.curbp->b_mroot,si.curwp);
			(void) movewd(n);
			if(getregion(&region,RegForceBegin) == Success && region.r_size > 0) {
				casetext(INT_MAX,region.r_size,trantab,NULL);
				si.curwp->w_face.wf_dot = region.r_dot;
				}
			goto Retn;
			}
		casetext(n,LONG_MAX,trantab,NULL);
		}
Retn:
	return rc.status;
	}

// Kill, delete, or copy forward by "n" words (which is never INT_MIN).  Save text to kill ring if kdc is non-zero.  If zero
// argument, kill or copy just one word and no trailing whitespace.
int kdcfword(int n,int kdc) {
	bool oneword;
	Region region;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Check if at end of buffer.
	if(bufend(dotp))
		return rcset(Failure,RCNoFormat,text259);
				// "No text selected"

	// Save the current point position.
	region.r_dot = *dotp;
	region.r_linect = 0;					// Not used.

	// Figure out how many characters to copy or give the axe.
	region.r_size = 0;

	// Get into a word...
	while(!inword()) {
		if(movech(1) != Success)
			break;					// At end of buffer.
		++region.r_size;
		}

	oneword = false;
	if(n == 0) {
		// Skip one word, no whitespace.
		while(inword()) {
			if(movech(1) != Success)
				break;				// At end of buffer.
			++region.r_size;
			}
		oneword = true;
		}
	else {
		short c;

		// Skip n words...
		oneword = (n == 1);
		while(n-- > 0) {

			// If we are at EOL; skip to the beginning of the next.
			while(dotp->off == dotp->lnp->l_used) {
				if(movech(1) != Success)
					goto Nuke;		// At end of buffer.
				++region.r_size;
				}

			// Move forward until we are at the end of the word.
			while(inword()) {
				if(movech(1) != Success)
					goto Nuke;		// At end of buffer.
				++region.r_size;
				}

			// If there are more words, skip the interword stuff.
			if(n != 0)
				while(!inword()) {
					if(movech(1) != Success)
						goto Nuke;	// At end of buffer.
					++region.r_size;
					}
			}

		// Skip whitespace and newlines.
		while(dotp->off == dotp->lnp->l_used || (c = dotp->lnp->l_text[dotp->off]) == ' ' ||
		 c == '\t') {
			if(movech(1) != Success)
				break;
			++region.r_size;
			}
		}
Nuke:
	// Have region... restore original position and kill, delete, or copy it.
	*dotp = region.r_dot;
	if(kdc <= 0) {

		// Kill or delete the word(s).
		if(kprep(kdc) == Success)
			(void) ldelete(region.r_size,kdc ? EditKill : EditDel);
		return rc.status;
		}

	// Copy the word(s).
	if(copyreg(&region) != Success)
		return rc.status;
	return rcset(Success,RCForce,"%s%s %s",text115,oneword ? "" : "s",text262);
				// "Word","copied"
	}

// Kill, delete, or copy backward by "n" words (which is always > 0).  Save text to kill ring if kdc is non-zero.
int kdcbword(int n,int kdc) {
	bool oneword;
	long size;
	Region region;

	// Check if at beginning of buffer.
	if(movech(-1) != Success)
		return rcset(Failure,RCNoFormat,text259);
				// "No text selected"

	// Figure out how many characters to copy or give the axe.
	size = 0;

	// Skip back n words...
	oneword = (n == 1);
	do {
		while(!inword()) {
			++size;
			if(movech(-1) != Success)
				goto CopyNuke;		// At beginning of buffer.
			}
		while(inword()) {
			++size;
			if(movech(-1) != Success)
				goto CopyNuke;		// At beginning of buffer.
			}
		} while(--n > 0);

	if(movech(1) != Success)
		return rc.status;
CopyNuke:
	// Have region, restore original position...
	(void) movech(size);		// Can't fail.

	// and kill, delete, or copy region.
	if(kdc <= 0) {

		// Kill or delete the word(s) backward.
		if(kprep(kdc) == Success)
			(void) ldelete(-size,kdc ? EditKill : EditDel);
		return rc.status;
		}

	// Copy the word(s) backward.
	region.r_dot = si.curwp->w_face.wf_dot;
	region.r_size = -size;
	region.r_linect = 0;				// Not used.
	if(copyreg(&region) != Success)
		return rc.status;
	return rcset(Success,RCForce,"%s%s %s",text115,oneword ? "" : "s",text262);
				// "Word","copied"
	}
