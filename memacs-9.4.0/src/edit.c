// (c) Copyright 2019 Richard W. Marinelli
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
void bchange(Buffer *buf,ushort flags) {

	if(buf->b_nwind > 1)			// Hard update needed?
		flags = WFHard;			// Yes.
	if(!(buf->b_flags & BFChanged)) {	// If first change...
		flags |= WFMode;		// need to update mode line.
		buf->b_flags |= BFChanged;
		}
	if(buf->b_mip != NULL)			// If precompiled macro...
		ppfree(buf);			// force macro preprocessor redo.
	supd_wflags(buf,flags);		// Lastly, flag any windows displaying this buffer.
	}

// Link line lnp1 into given buffer.  If lnp2 is NULL, link line at end of buffer; otherwise, link line before lnp2.  If buf is
// NULL, use current buffer.
void llink(Line *lnp1,Buffer *buf,Line *lnp2) {
	Line *lnp0;

	if(buf == NULL)
		buf = si.curbuf;
	if(lnp2 == NULL) {
		lnp2 = buf->b_lnp->l_prev;
		lnp1->l_next = NULL;
		lnp1->l_prev = lnp2;
		buf->b_lnp->l_prev = lnp2->l_next = lnp1;
		}
	else {
		lnp0 = lnp2->l_prev;
		lnp1->l_next = lnp2;
		lnp2->l_prev = lnp1;

		// Inserting before first line of buffer?
		if(lnp2 == buf->b_lnp)
			buf->b_lnp = lnp1;		// Yes.
		else
			lnp0->l_next = lnp1;		// No.

		lnp1->l_prev = lnp0;
		}
	}

// Unlink a line from given buffer and free it.  If buf is NULL, use current buffer.  It is assumed that at least two lines
// exist.
void lunlink(Line *lnp,Buffer *buf) {

	if(buf == NULL)
		buf = si.curbuf;
	if(lnp == buf->b_lnp)

		// Deleting first line of buffer.
		(buf->b_lnp = lnp->l_next)->l_prev = lnp->l_prev;
	else if(lnp == buf->b_lnp->l_prev)

		// Deleting last line of buffer.
		(buf->b_lnp->l_prev = lnp->l_prev)->l_next = NULL;
	else {
		// Deleting line between two other lines.
		lnp->l_next->l_prev = lnp->l_prev;
		lnp->l_prev->l_next = lnp->l_next;
		}
	free((void *) lnp);
	}

// Replace line lnp1 in given buffer with lnp2 and free lnp1.  If buf is NULL, use current buffer.
void lreplace1(Line *lnp1,Buffer *buf,Line *lnp2) {

	if(buf == NULL)
		buf = si.curbuf;
	lnp2->l_next = lnp1->l_next;
	if(lnp1 == buf->b_lnp) {

		// Replacing first line of buffer.
		buf->b_lnp = lnp2;
		if(lnp1->l_prev == lnp1)

			// First line is only line... point new line to itself.
			lnp2->l_prev = lnp2;
		else {
			lnp2->l_prev = lnp1->l_prev;
			lnp1->l_next->l_prev = lnp2;
			}
		}
	else {
		lnp1->l_prev->l_next = lnp2;
		lnp2->l_prev = lnp1->l_prev;
		if(lnp1->l_next == NULL)

			// Replacing last line of buffer.
			buf->b_lnp->l_prev = lnp2;
		else
			lnp1->l_next->l_prev = lnp2;
		}
	free((void *) lnp1);
	}

// Replace lines lnp1 and lnp2 in the current buffer with lnp3 and free the first two.
static void lreplace2(Line *lnp1,Line *lnp2,Line *lnp3) {

	lnp3->l_next = lnp2->l_next;
	if(lnp1 == si.curbuf->b_lnp) {

		// lnp1 is first line of buffer.
		si.curbuf->b_lnp = lnp3;
		if(lnp2->l_next == NULL)

			// lnp2 is last line of buffer.
			lnp3->l_prev = lnp3;
		else {
			lnp3->l_prev = lnp1->l_prev;
			lnp2->l_next->l_prev = lnp3;
			}
		}
	else {
		lnp1->l_prev->l_next = lnp3;
		lnp3->l_prev = lnp1->l_prev;
		if(lnp2->l_next == NULL)

			// lnp2 is last line of buffer.
			si.curbuf->b_lnp->l_prev = lnp3;
		else
			lnp2->l_next->l_prev = lnp3;
		}
	free((void *) lnp1);
	free((void *) lnp2);
	}

// Fix "window face" line pointers and point offset after insert.
static void fixins(int offset,int n,WindFace *wfp,Line *lnp1,Line *lnp2) {

	if(wfp->wf_toplnp == lnp1)
		wfp->wf_toplnp = lnp2;
	if(wfp->wf_point.lnp == lnp1) {
		wfp->wf_point.lnp = lnp2;
		if(wfp->wf_point.off >= offset)
			wfp->wf_point.off += n;
		}
	}

// Scan all screens and windows and fix window pointers.
void fixwface(int offset,int n,Line *lnp1,Line *lnp2) {
	EScreen *scr;
	EWindow *win;

	//  In all screens...
	scr = si.shead;
	do {
		// In all windows...
		win = scr->s_whead;
		do {
			fixins(offset,n,&win->w_face,lnp1,lnp2);
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);
	}

// Fix buffer pointers and marks after insert.
void fixbface(Buffer *buf,int offset,int n,Line *lnp1,Line *lnp2) {
	Mark *mark;

	fixins(offset,n,&buf->b_face,lnp1,lnp2);
	mark = &buf->b_mroot;
	do {
		if(mark->mk_point.lnp == lnp1) {
			mark->mk_point.lnp = lnp2;

			// Note that unlike the point, marks are not "pushed forward" as text is inserted.  They remain anchored
			// where they are set.  Hence, want to use '>' here instead of '>='.
			if(mark->mk_point.off > offset)
				mark->mk_point.off += n;
			}
		} while((mark = mark->mk_next) != NULL);
	}

// Insert "n" copies of the character "c" into the current buffer at point.  In the easy case, all that happens is the
// character(s) are stored in the line.  In the hard case, the line has to be reallocated.  When the window list is updated,
// need to (1), always update point in the current window; and (2), update mark and point in other windows if their buffer
// position is past the place where the insert was done.  Note that no validity checking is done, so if "c" is a newline, it
// will be inserted as a literal character.  Return status.
int linsert(int n,short c) {
	char *str1,*str2;
	Line *lnp1,*lnp2;
	int offset;
	int i;

	// Don't allow if read-only buffer... and nothing to do if repeat count is zero.
	if(allowedit(true) != Success || n == 0)
		return rc.status;

	// Negative repeat count is an error.
	if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"

	// n > 0.  Get current line and determine the type of insert.
	lnp1 = si.curwin->w_face.wf_point.lnp;
	offset = si.curwin->w_face.wf_point.off;
	if(lnp1->l_used + n > lnp1->l_size) {		// Not enough room left in line: reallocate.
		if(lalloc(BSIZE(lnp1->l_used + n),&lnp2) != Success)
			return rc.status;		// Fatal error.
		lnp2->l_used = lnp1->l_used + n;	// Set new "used" length.
		str1 = lnp1->l_text;			// Copy old to new up to point.
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
	bchange(si.curbuf,WFEdit);
	fixwface(offset,n,lnp1,lnp2);
	fixbface(si.curbuf,offset,n,lnp1,lnp2);

	return rc.status;
	}

// Fix "window face" line pointers and point offset after newline was inserted.
static void fixinsnl(int offset,WindFace *wfp,Line *lnp1,Line *lnp2) {

	if(wfp->wf_toplnp == lnp1)
		wfp->wf_toplnp = lnp2;
	if(wfp->wf_point.lnp == lnp1) {
		if(wfp->wf_point.off < offset)
			wfp->wf_point.lnp = lnp2;
		else
			wfp->wf_point.off -= offset;
		}
	}

// Insert a newline into the current buffer at point.  Return status.  The funny ass-backward way this is done is not a botch;
// it just makes the last line in the buffer not a special case.
int lnewline(void) {
	char *str1,*str2;
	Line *lnp1,*lnp2;
	int offset;
	EScreen *scr;
	EWindow *win;
	Mark *mark;

	if(allowedit(true) != Success)		// Don't allow if read-only buffer.
		return rc.status;

	bchange(si.curbuf,WFHard);
	lnp1 = si.curwin->w_face.wf_point.lnp;	// Get the address and offset of point.
	offset = si.curwin->w_face.wf_point.off;
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
	scr = si.shead;
	do {
		// In all windows...
		win = scr->s_whead;
		do {
			fixinsnl(offset,&win->w_face,lnp1,lnp2);
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);

	// In current buffer...
	fixinsnl(offset,&si.curbuf->b_face,lnp1,lnp2);
	mark = &si.curbuf->b_mroot;
	do {
		if(mark->mk_point.lnp == lnp1) {
			if(mark->mk_point.off < offset)
				mark->mk_point.lnp = lnp2;
			else
				mark->mk_point.off -= offset;
			}
		} while((mark = mark->mk_next) != NULL);

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

// Fix "window face" line pointers and point offset after a newline was deleted without line reallocation.
static void fixdelnl1(WindFace *wfp,Line *lnp1,Line *lnp2) {

	if(wfp->wf_toplnp == lnp2)
		wfp->wf_toplnp = lnp1;
	if(wfp->wf_point.lnp == lnp2) {
		wfp->wf_point.lnp = lnp1;
		wfp->wf_point.off += lnp1->l_used;
		}
	}

// Fix "window face" line pointers and point offset after a newline was deleted and a new line was allocated.
static void fixdelnl2(WindFace *wfp,Line *lnp1,Line *lnp2,Line *lnp3) {

	if(wfp->wf_toplnp == lnp1 || wfp->wf_toplnp == lnp2)
		wfp->wf_toplnp = lnp3;
	if(wfp->wf_point.lnp == lnp1)
		wfp->wf_point.lnp = lnp3;
	else if(wfp->wf_point.lnp == lnp2) {
		wfp->wf_point.lnp = lnp3;
		wfp->wf_point.off += lnp1->l_used;
		}
	}

// Delete a newline from current buffer -- join the current line with the next line.  Easy cases can be done by shuffling data
// around.  Hard cases require that lines be moved about in memory.  Called by ldelete() only.  It is assumed that there will
// always be at least two lines in the current buffer when this routine is called and the current line will never be the last
// line of the buffer (which does not have a newline delimiter).
static int ldelnewline(void) {
	char *str1,*str2;
	Line *lnp1,*lnp2,*lnp3;
	EScreen *scr;
	EWindow *win;
	Mark *mark;

	lnp1 = si.curwin->w_face.wf_point.lnp;
	lnp2 = lnp1->l_next;

	// Do simple join if room in current line for next line.
	if(lnp2->l_used <= lnp1->l_size - lnp1->l_used) {
		str1 = lnp1->l_text + lnp1->l_used;
		str2 = lnp2->l_text;
		while(str2 != lnp2->l_text + lnp2->l_used)
			*str1++ = *str2++;

		// In all screens...
		scr = si.shead;
		do {
			// In all windows...
			win = scr->s_whead;
			do {
				fixdelnl1(&win->w_face,lnp1,lnp2);
				} while((win = win->w_next) != NULL);
			} while((scr = scr->s_next) != NULL);

		// In current buffer...
		fixdelnl1(&si.curbuf->b_face,lnp1,lnp2);
		mark = &si.curbuf->b_mroot;
		do {
			if(mark->mk_point.lnp == lnp2) {
				mark->mk_point.lnp = lnp1;
				mark->mk_point.off += lnp1->l_used;
				}
			} while((mark = mark->mk_next) != NULL);

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
	scr = si.shead;
	do {
		// In all windows...
		win = scr->s_whead;
		do {
			fixdelnl2(&win->w_face,lnp1,lnp2,lnp3);
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);

	// In current buffer...
	fixdelnl2(&si.curbuf->b_face,lnp1,lnp2,lnp3);
	mark = &si.curbuf->b_mroot;
	do {
		if(mark->mk_point.lnp == lnp1)
			mark->mk_point.lnp = lnp3;
		else if(mark->mk_point.lnp == lnp2) {
			mark->mk_point.lnp = lnp3;
			mark->mk_point.off += lnp1->l_used;
			}
		} while((mark = mark->mk_next) != NULL);

	return rc.status;
	}

// Fix point offset after delete.
static void fixdotdel1(int offset,int chunk,Point *point) {
	int delta;

	if(point->off > offset) {
		if(chunk >= 0) {
			delta = point->off - offset;
			point->off -= (chunk < delta ? chunk : delta);
			}
		else
			point->off += chunk;
		}
	else if(chunk < 0 && (delta = chunk + (offset - point->off)) < 0)
		point->off += delta;
	}

// Fix point after delete in other windows with the same text displayed.
static void fixdotdel(Line *lnp,int offset,int chunk) {
	EScreen *scr;
	EWindow *win;
	Mark *mark;

	// In all screens...
	scr = si.shead;
	do {
		// In all windows...
		win = scr->s_whead;
		do {
			if(win->w_face.wf_point.lnp == lnp)
				fixdotdel1(offset,chunk,&win->w_face.wf_point);
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);

	// In current buffer...
	if(si.curbuf->b_face.wf_point.lnp == lnp)
		fixdotdel1(offset,chunk,&si.curbuf->b_face.wf_point);
	mark = &si.curbuf->b_mroot;
	do {
		if(mark->mk_point.lnp == lnp)
			fixdotdel1(offset,chunk,&mark->mk_point);
		} while((mark = mark->mk_next) != NULL);
	}

// This function deletes up to n characters from the current buffer, starting at point.  The text that is deleted may include
// newlines.  Positive n deletes forward; negative n deletes backward.  Returns current status if all of the characters were
// deleted, NotFound (bypassing rcset()) if they were not (because point ran into a buffer boundary), or the appropriate status
// if an error occurred.  The deleted text is put in the kill ring if the EditKill flag is set, the delete ring if the EditDel
// flag is set, otherwise discarded.
int ldelete(long n,ushort flags) {

	if(n == 0 || allowedit(true) != Success)	// Don't allow if read-only buffer.
		return rc.status;

	ushort wflags;
	char *str1,*str2;
	Line *lnp;
	int offset,chunk;
	RingEntry *rep;
	DStrFab sfab;
	Point *point = &si.curwin->w_face.wf_point;
	bool hitEOB = false;

	// Set kill buffer pointer.
	rep = (flags & EditKill) ? kring.r_entry : (flags & EditDel) ? dring.r_entry : NULL;
	wflags = 0;

	// Going forward?
	if(n > 0) {
		if(rep != NULL && dopenwith(&sfab,&rep->re_data,SFAppend) != 0)
			goto LibFail;
		do {
			// Get the current line.
			lnp = point->lnp;
			offset = point->off;

			// Figure out how many characters to delete on this line.
			if((chunk = lnp->l_used - offset) > n)
				chunk = n;

			// If at the end of a line, merge with the next.
			if(chunk == 0) {

				// Can't delete past end of buffer.
				if(lnp->l_next == NULL)
					goto EOB;

				// Flag that we are making a hard change and delete newline.
				wflags |= WFHard;
				if(ldelnewline() != Success)
					return rc.status;
				if(rep != NULL && dputc('\n',&sfab) != 0)
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
			if(rep != NULL && chunk > 0 && dputmem((void *) str1,chunk,&sfab) != 0)
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
		if(rep != NULL && dopenwith(&sfab,&rep->re_data,SFPrepend) != 0)
			goto LibFail;
		do {
			// Get the current line.
			lnp = point->lnp;
			offset = point->off;

			// Figure out how many characters to delete on this line.
			if((chunk = offset) > -n)
				chunk = -n;

			// If at the beginning of a line, merge with the previous one.
			if(chunk == 0) {

				// Can't delete past beginning of buffer.
				if(lnp == si.curbuf->b_lnp)
					goto EOB;

				// Flag that we are making a hard change and delete the newline.
				wflags |= WFHard;
				(void) movech(-1);
				if(ldelnewline() != Success)
					return rc.status;
				if(rep != NULL && dputc('\n',&sfab) != 0)
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
			if(rep != NULL && dputmem((void *) str2,chunk,&sfab) != 0)
				goto LibFail;

			// Shift what remains on the line leftward.
			while(str1 < lnp->l_text + lnp->l_used)
				*str2++ = *str1++;
			lnp->l_used -= chunk;
			point->off -= chunk;

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
	if(rep != NULL && dclose(&sfab,sf_string) != 0)
		goto LibFail;
	if(wflags)
		bchange(si.curbuf,wflags);
	return hitEOB ? NotFound : rc.status;
LibFail:
	return librcset(Failure);
	}

// Quote the next character, and insert it into the buffer abs(n) times, ignoring "Over" and "Repl" buffer modes if n < 0.  All
// characters are taken literally, including newline, which does not then have its line-splitting meaning.  The character is
// always read, even if it is inserted zero times, so that the command completes normally.  If a function key is pressed, its
// symbolic name is inserted.
int quoteChar(Datum *rval,int n,Datum **argv) {
	ushort ek;
	bool forceIns = false;

	// Move cursor from message line back to buffer if interactive.
	if(!(si.opflags & OpScript))
		if(ttmove(si.curscr->s_cursrow,si.curscr->s_curscol) != Success || ttflush() != Success)
			return rc.status;

	// Set real value of n.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0) {
		n = -n;
		forceIns = true;
		}

	// Get the key.
	if(getkey(false,&ek,true) != Success)
		return rc.status;

	if(n > 0) {
		// If this is a function or special key, put its name in.
		if(ek & (FKey | Shft)) {
			int len;
			char keybuf[16];

			len = strlen(ektos(ek,keybuf,false));
			do {
				if((!forceIns && overprep(len) != Success) || linstr(keybuf) != Success)
					break;
				} while(--n > 0);
			}

		// Otherwise, just insert the raw character n times.
		else if(forceIns || overprep(n) == Success)
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
		Point wkpoint = si.curwin->w_face.wf_point;
		int len = wkpoint.lnp->l_used;
		while(wkpoint.off < len && wkpoint.lnp->l_text[wkpoint.off] == ' ')
			++wkpoint.off;

		// Set len to size of first tab and loop n times.
		len = si.stabsize - getcol(&wkpoint,false) % si.stabsize;
		do {
			if(linsert(len,' ') != Success)
				break;
			len = si.stabsize;
			} while(--n > 0);
		}

	return rc.status;
	}

// Set "line count" return message and return status.
static int lineMsg(ushort flags,int linect,char *action) {

	return (si.opflags & OpScript) ? rc.status : rcset(Success,flags,text307,linect,linect == 1 ? "" : "s",action);
								// "%d line%s %s"
	}

// Finish a routine that modifies a block of lines.
static int lchange(Point *opoint,bool backward,int count) {
	Mark *mark;
	Point *point = &si.curwin->w_face.wf_point;

	kentry.thisflag &= ~CFVMove;		// Flag that this resets the goal column.
	if(backward) {				// Set mark at beginning of line block and move point to end.
		point->off = 0;
		if(!bufbegin(point))
			(void) moveln(1);
		mset(&si.curbuf->b_mroot,si.curwin);
		*point = *opoint;
		point->off = 0;
		(void) moveln(1);
		}

	// Scan marks in current buffer and adjust offset if needed.
	mark = &si.curbuf->b_mroot;
	do {
		point = &mark->mk_point;
		if(point->off > point->lnp->l_used)
			point->off = point->lnp->l_used;
		} while((mark = mark->mk_next) != NULL);

	// Set window flags and return.
	if(count == 0)
		si.curwin->w_flags |= WFMove;
	else
		bchange(si.curbuf,WFHard);	// Flag that a line other than the current one was changed.

	return lineMsg(RCHigh,count,text260);
			// "changed"
	}

// Get (hard) tab size into *tsp for detabLine() and entabLine().
static int getTabSize(Datum **argv,int *tsp) {
	int tabsz;

	if(si.opflags & OpScript) {
		if((*argv)->d_type == dat_nil) {
			*tsp = si.htabsize;
			goto Retn;
			}
		tabsz = (*argv)->u.d_int;
		}
	else {
		Datum *datum;
		long lval;
		char wkbuf[4];
		TermInp ti = {wkbuf,RtnKey,0,NULL};

		if(dnewtrk(&datum) != 0)
			return librcset(Failure);
		sprintf(wkbuf,"%d",si.htabsize);
		if(terminp(datum,text393,ArgNotNull1 | ArgNil1,0,&ti) != Success)
				// "Tab size"
			goto Retn;
		if(datum->d_type == dat_nil)
			return Cancelled;
		if(asc_long(datum->d_str,&lval,false) != Success)
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
int detabLine(Datum *rval,int n,Datum **argv) {
	int inc,len,tabsz;
	int count;
	bool changed;
	Point *point = &si.curwin->w_face.wf_point;
	Point wkpoint = *point;		// Original point position.

	// Get hard tab size.
	if(getTabSize(argv,&tabsz) != Success)
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

	point->off = 0;			// Start at the beginning.
	if(n > 0)
		mset(&si.curbuf->b_mroot,si.curwin);

	// Loop thru text, detabbing n lines.
	kentry.lastflag &= ~CFVMove;
	for(count = 0; n != 0; n -= inc) {
		changed = false;

		// Detab the entire current line.
		while(point->off < point->lnp->l_used) {

			// If we have a tab.
			if(point->lnp->l_text[point->off] == '\t') {
				len = tabsz - (point->off % tabsz);
				if(ldelete(1L,0) != Success || insnlspace(-len,EditSpace | EditHoldPt) != Success)
					return rc.status;
				point->off += len;
				changed = true;
				}
			else
				++point->off;
			}
		if(changed)
			++count;

		// Move forward or backward to the next line.
		point->off = 0;
		if(moveln(inc) != Success)
			break;
		}

	return lchange(&wkpoint,inc == -1,count);
	}

// Define column-calculator macro.
#define nextab(a)	(a - (a % tabsz)) + tabsz

// Change spaces to tabs (where possible) in a block of lines.  If n is zero, use lines in current region.  No error if attempt
// to move past end of buffer.  Always leave point at end of line block.  Return status.
int entabLine(Datum *rval,int n,Datum **argv) {
	int inc;			// Increment to next line.
	int fspace;			// Index of first space if in a run.
	int ccol;			// Current point column.
	int tabsz,len,count;
	bool changed;
	Point *point = &si.curwin->w_face.wf_point;
	Point wkpoint = *point;		// Original point position.

	// Get hard tab size.
	if(getTabSize(argv,&tabsz) != Success)
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

	point->off = 0;		// Start at the beginning.
	if(n > 0)
		mset(&si.curbuf->b_mroot,si.curwin);

	// Loop thru text, entabbing n lines.
	kentry.lastflag &= ~CFVMove;
	for(count = 0; n != 0; n -= inc) {

		// Entab the entire current line.
		ccol = 0;
		fspace = -1;
		changed = false;

		while(point->off <= point->lnp->l_used) {

			// Time to compress?
			if(fspace >= 0 && nextab(fspace) <= ccol) {

				// Yes.  Skip if just a single space; otherwise, chaos ensues.
				if((len = ccol - fspace) >= 2) {
					point->off -= len;
					if(ldelete((long) len,0) != Success || linsert(1,'\t') != Success)
						return rc.status;
					changed = true;
					}
				fspace = -1;
				}
			if(point->off == point->lnp->l_used)
				break;

			// Get the current character and check it.
			switch(point->lnp->l_text[point->off]) {

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
			++point->off;
			}
		if(changed)
			++count;

		// Move forward or backward to the next line.
		point->off = 0;
		if(moveln(inc) != Success)
			break;
		}

	return lchange(&wkpoint,inc == -1,count);
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
		 !bmsrch(si.curbuf,false,2,mi.cache[MdIdxOver],mi.cache[MdIdxRepl]) &&
		 si.wrapcol > 0 && getcol(NULL,true) > si.wrapcol)
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
	Line *lnp = si.curwin->w_face.wf_point.lnp;

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
int trimLine(Datum *rval,int n,Datum **argv) {
	int inc,count;
	Point *point = &si.curwin->w_face.wf_point;
	Point wkpoint = *point;		// Original point position.

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

	point->off = 0;			// Start at the beginning.
	if(n > 0)
		mset(&si.curbuf->b_mroot,si.curwin);

	// Loop through text, trimming n lines.
	kentry.lastflag &= ~CFVMove;
	for(count = 0; n != 0; n -= inc) {
		count += trimln();

		// Move forward or backward to the next line.
		point->off = 0;
		if(moveln(inc) != Success)
			break;
		}

	return lchange(&wkpoint,inc == -1,count);
	}

// Open up some blank space.  The basic plan is to insert a bunch of newlines, and then back up over them.  Everything is done
// by the subcommand processors.  They even handle the looping.
int openLine(Datum *rval,int n,Datum **argv) {
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

// Open a line before [-]nth line with same indentation as the line preceding target line.
int openLineI(Datum *rval,int n,Datum **argv) {

	if(n != 0) {
		// Move point to end of line preceding target line.
		(void) beline(rval,n == INT_MIN || n == 1 ? -1 : n - 1,true);

		// If hit beginning of buffer, just open a line; otherwise, call "newline and indent" routine.
		(void) (bufbegin(NULL) ? insnlspace(1,EditHoldPt) : newlineI(rval,1,NULL));
		}
	return rc.status;
	}

// Compute and insert "i" variable at point n times.
int inserti(Datum *rval,int n,Datum **argv) {
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

// Delete blank lines around point.  If point is on a blank line, delete all blank lines above and below the current line.
// If it is on a non-blank line, delete all blank lines above (n < 0) or below (otherwise) the current line.
int delBlankLines(Datum *rval,int n,Datum **argv) {
	Line *lnp1,*lnp2;
	long count;
	Point *point = &si.curwin->w_face.wf_point;

	if(n == INT_MIN)
		n = 0;

	// Only one line in buffer?
	if((lnp1 = si.curbuf->b_lnp)->l_next == NULL) {
		if(lnp1->l_used == 0 || !is_white(lnp1,lnp1->l_used))
			return rc.status;
		count = lnp1->l_used;
		}
	else {
		// Two or more lines in buffer.  Check if current line is not blank and deleting prior blank lines.
		lnp1 = point->lnp;
		if(n < 0 && !is_white(lnp1,lnp1->l_used)) {

			// True.  Nothing to do if current line is first line in buffer.
			if(lnp1 == si.curbuf->b_lnp)
				return rc.status;

			// Not on first line.  Back up one line.
			lnp1 = lnp1->l_prev;
			}
		else
			lnp1 = point->lnp;

		// Find first non-blank line going backward.
		while(is_white(lnp1,lnp1->l_used)) {
			if(lnp1 == si.curbuf->b_lnp)
				break;
			lnp1 = lnp1->l_prev;
			}

		// Nothing to do if line is last line in buffer.
		if(lnp1->l_next == NULL)
			return rc.status;

		// lnp1 not last line.  Find first non-blank line going forward, counting characters as we go.
		lnp2 = lnp1;
		count = 0;
		for(;;) {
			lnp2 = lnp2->l_next;
			if(!is_white(lnp2,lnp2->l_used))
				break;
			count += lnp2->l_used + 1;
			if(lnp2->l_next == NULL) {
				--count;
				break;
				}
			}

		// Handle special case where first buffer line is blank.
		if(is_white(lnp1,lnp1->l_used)) {
			point->lnp = lnp1;
			count += lnp1->l_used + 1;
			}
		else {
			if(count == 0)
				return rc.status;
			point->lnp = lnp1->l_next;
			}
		}

	// Finally, move point to beginning of first blank line and delete all the white space.
	point->off = 0;
	return ldelete(count,0);
	}

// Insert a newline, then enough tabs and spaces to duplicate the indentation of the previous line.  Tabs are every si.htabsize
// characters.  Quite simple.  Figure out the indentation of the current line.  Insert a newline by calling the standard
// routine.  Insert the indentation by inserting the right number of tabs and spaces.  Return status.  Normally bound to ^J.
int newlineI(Datum *rval,int n,Datum **argv) {
	Datum *indent;
	Line *lnp = si.curwin->w_face.wf_point.lnp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"

	// Get the indentation of the current line, if any.
	if(getindent(&indent,lnp) != Success)
		return rc.status;

	// Insert lines with indentation in the final line.
	do {
		if(lnewline() != Success || (indent != NULL && n == 1 && linstr(indent->d_str) != Success))
			break;
		} while(--n > 0);

	return rc.status;
	}

// Delete hard tabs or "chunks" of spaces forward or backward.  Return status.  Soft tabs are deleted such that the next
// non-space character (or end of line) past point is moved left until it reaches a tab stop, one or more times.  To accomplish
// this, spaces are deleted beginning at point going forward (if n > 0) and point does not move, or spaces are deleted prior to
// point going backward (if n < 0) and point follows.  If n < 0 and "force" is true (called by "backspace" command), deletions
// are done iteratively up to abs(n) times such that either one soft tab or one character (including a newline) is deleted in
// each iteration.
int deltab(int n,bool force) {
	int off,i,direc;
	Point *point = &si.curwin->w_face.wf_point;

	// Check for "do nothing" cases.
	if(n == INT_MIN)
		n = 1;
	if((i = point->lnp->l_used) == 0 || ((off = point->off) == 0 && n < 0) || (off == i && n > 0))
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
		while((off = point->off + i) >= 0 && off < point->lnp->l_used && point->lnp->l_text[off] == '\t')
			if(ldelete(direc,0) != Success || --n == 0)
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
					if(point->off == 0 || point->lnp->l_text[point->off - 1] != ' ')
						break;
					}
				else if(point->off == 0)
					goto Nuke1;
				}

			// Save column position of point and scan forward to next non-space character.
			int dotcol = getcol(NULL,false);
			int len = point->lnp->l_used;
			for(off = point->off; point->off < len && point->lnp->l_text[point->off] == ' '; ++point->off);

			// Bail out if deleting forward and run length (len) is zero.
			if(n > 0 && (len = point->off - off) == 0)
				break;
			int col1,col2,chunk1;

			// Get column position of non-space character (col2), calculate position of prior tab stop (col1), and
			// calculate size of first "chunk" (distance from prior tab stop to non-space character).
			col1 = ((col2 = getcol(NULL,false)) - 1) / si.stabsize * si.stabsize;
			chunk1 = col2 - col1;

			// If deleting forward...
			if(n > 0) {
				// If prior tab stop is before point, stop here if non-space character is not a tab stop or
				// point is not at BOL and the character just prior to point is a space.
				if(col1 < dotcol && (col2 % si.stabsize != 0 ||
				 (off > 0 && point->lnp->l_text[off - 1] == ' '))) {
					point->off = off;
					break;
					}
				goto Nuke;
				}

			// Deleting backward.  Scan backward to first non-space character.
			for(point->off = off; point->off > 0 && point->lnp->l_text[point->off - 1] == ' '; --point->off);

			// Continue only if run (len) is not too short or point is at a tab stop.
			len = off - point->off;
			point->off = off;		// Restore original point position.
			if(len >= chunk1 || (len > 0 && dotcol % si.stabsize == 0)) {
Nuke:;
				// Delete one or more soft tabs.  Determine number of spaces to delete and nuke 'em.
				int max = (len - chunk1) / si.stabsize;
				int m = abs(n) - 1;
				m = (m > max ? max : m);
				if(ldelete((long) -(len < chunk1 ? len : (chunk1 + m * si.stabsize)),0) != Success)
					return rc.status;
				if(n > 0)
					point->off = off;
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

// Kill, delete, or copy text for kdc values of -1, 0, or 1, respectively.  Save text to kill ring if kdc is non-zero.  If reg
// is not NULL, operate on the given region (assumed to not be empty); otherwise, operate on text selected by n via call to
// gettregion().  Return status.
int kdctext(int n,int kdc,Region *reg) {
	Region region;
	Point *point = &si.curwin->w_face.wf_point;

	// Process region elsewhere if specified.
	if(reg != NULL) {
		if(kdc > 0)
			goto Kopy;
		if(kprep(kdc) != Success)
			return rc.status;
		*point = reg->r_point;
		return ldelete(reg->r_size,kdc ? EditKill : EditDel);
		}

	// No region... process lines and make one.  Error if region is empty.
	gettregion(point,n,&region,0);
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
	reg = &region;
Kopy:
	return copyreg(reg) != Success || (si.opflags & OpScript) ? rc.status : rcset(Success,RCHigh,"%s %s",text261,text262);
													// "Text","copied"
	}

// Kill, delete, or copy whole line(s) via kdctext() for kdc values of -1, 0, or 1, respectively.  Called from execCF() for
// killLine, delLine, and copyLine commands.
int kdcline(int n,int kdc) {
	Region region;

	// Convert line block to a region.
	if(getlregion(n,&region,RegInclDelim) != Success)
		return rc.status;

	// Nuke or copy line block (as a region) via kdctext().
	if(kdc <= 0)
		si.curwin->w_face.wf_point = region.r_point;
	return kdctext(0,kdc,&region) != Success || kdc <= 0 ? rc.status : lineMsg(RCForce,region.r_linect,text262);
													// "copied"
	}

// Duplicate whole line(s) in the current buffer, leaving point at beginning of second line block.  Return status.
int dupLine(Datum *rval,int n,Datum **argv) {
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

	// Move point to beginning of line block and insert it.
	si.curwin->w_face.wf_point = region.r_point;
	if(linstr(wkbuf) == Success)
		(void) begintxt();
	return rc.status;
	}

// Select [-]N lines (default 1, region lines if N == 0) or text if n > 0 (where N is function argument), move point to
// first line of block, and return number of lines selected.  If n < 0, omit delimiter of last line selected.  Return status.
int selectLine(Datum *rval,int n,Datum **argv) {
	Region region;

	// Get region.
	if(n > 0)
		gettregion(&si.curwin->w_face.wf_point,argv[0]->u.d_int,&region,RegForceBegin);
	else if(getlregion(argv[0]->u.d_int,&region,n < 0 && n != INT_MIN ? RegEmptyOK | RegLineSelect :
	 RegInclDelim | RegEmptyOK | RegLineSelect) != Success)
		return rc.status;

	// If region not empty, move point to beginning of region and set region in buffer.
	if(region.r_size > 0) {
		movept(&region.r_point);
		(void) movech(region.r_size);
		mset(&si.curbuf->b_mroot,si.curwin);
		si.curwin->w_face.wf_point = region.r_point;
		}

	// Return line count.
	dsetint((long) region.r_linect,rval);
	return rc.status;
	}

// Delete white space at and forward from point (if any) on current line.  If n > 0, include non-word characters as well.
static int delwhitef(int n) {
	short c;
	Point *point = &si.curwin->w_face.wf_point;

	for(;;) {
		if(point->off == point->lnp->l_used)
			break;
		c = point->lnp->l_text[point->off];
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
	Point *point = &si.curwin->w_face.wf_point;
	Line *lnp = point->lnp;

	if(lnp->l_used > 0 && (prior || point->off == lnp->l_used || (c = lnp->l_text[point->off]) == ' ' || c == '\t' ||
	 (n > 0 && !wordlist[(int) c]))) {

		// Delete backward.
		for(;;) {
			if(point->off == 0)
				break;
			c = lnp->l_text[point->off - 1];
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

// Join adjacent line(s), replacing all white space in between with (1), nothing if "delim" is nil; or (2), a single space
// character (unless either line is blank or all white space) and insert extra space character if first of two adjacent lines
// ends with any character specified in "delim".  Return status.
static int joinln(Datum *rval,int n,Datum *delim) {
	int m,incr,newdot;
	bool insSpace = (delim == NULL || delim->d_type != dat_nil);
	Point *point = &si.curwin->w_face.wf_point;

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
		newdot = point->lnp->l_used;			// Save current end-of-line position.
		--n;
		}
	else
		incr = -1;

	// Join lines forward or backward.
	do {
		// Move to beginning of first line of pair if needed, trim white space, move to end, delete newline, delete
		// white space, and if delim not nil and not at beginning or end of new line, insert one or two spaces.
		if(incr == -1) {
			if(point->lnp == si.curbuf->b_lnp)
				break;
			point->lnp = point->lnp->l_prev;
			}
		else if(point->lnp->l_next == NULL)
			break;
		(void) trimln();
		point->off = point->lnp->l_used;
		if(ldelete(1L,0) != Success || delwhitef(0) != Success)
			return rc.status;
		if(insSpace && point->off > 0 && point->off < point->lnp->l_used) {
			m = 1;
			if(delim != NULL && index(delim->d_str,point->lnp->l_text[point->off - 1]) != NULL)
				++m;
			if(linsert(m,' ') != Success)
				return rc.status;
			}
		} while((n -= incr) != 0);
	if(incr > 0)
		point->off = newdot;				// Set current position.

	return rc.status;
	}

// Join adjacent line(s) via joinln(), passing argument value if script mode.
int joinLines(Datum *rval,int n,Datum **argv) {
	Datum *delim = NULL;

	// Get sentence-end characters if script mode.
	if((si.opflags & OpScript) && !disnull(argv[0]))
		delim = argv[0];

	// Call joinln().
	return joinln(rval,n,delim);
	}

// Kill, delete, or copy fenced region if kdc is -1, 0, or 1, respectively.  Return status.
bool kdcFencedRegion(int kdc) {
	Region region;

	if(otherfence(0,&region) == Success)
		(void) kdctext(0,kdc,&region);
	return rc.status;
	}

// Write text to a named buffer.
int bprint(Datum *rval,int n,Datum **argv) {
	Buffer *buf;

	// Negative repeat count is an error.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"

	// Get the buffer name.
	if((buf = bsrch(argv[0]->d_str,NULL)) == NULL)
		return rcset(Failure,0,text118,argv[0]->d_str);
			// "No such buffer '%s'"
	if(!needsym(s_comma,true))
		return rc.status;

	return chgtext(rval,n,Txt_Insert,buf);
	}

// Write lines in region to a (different) buffer.  If n >= 0, write characters in region instead.  If n <= 0, also delete
// selected text after write.  Return status.
int writeBuf(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	Region region;

	// Get the destination buffer name.  Make sure it's not the current buffer.
	buf = bdefault();
	if(bcomplete(rval,text469,buf != NULL ? buf->b_bname : NULL,OpCreate,&buf,NULL) != Success || buf == NULL)
			// "Write to"
		return rc.status;
	if(buf == si.curbuf)
		return rcset(Failure,0,text471);
			// "Cannot write to current buffer"

	// Get region.
	if((n >= 0 ? getregion(&region,RegForceBegin) :
	 getlregion(0,&region,RegForceBegin | RegInclDelim | RegEmptyOK | RegLineSelect)) != Success)
		return rc.status;

	// Preallocate a string and copy region to it.
	if(dsalloc(rval,region.r_size + 1) != 0)
		return librcset(Failure);
	regcpy(rval->d_str,&region);

	// Insert the text into the buffer.
	if(iortext(rval->d_str,1,Txt_Insert,buf) != Success)
		return rc.status;
	if(!(si.opflags & OpScript)) {
		if(n >= 0)
			(void) rcset(Success,RCHigh,"%s %s",text261,text470);
							// "Text","written"
		else
			(void) lineMsg(RCHigh,region.r_linect,text470);
							// "written"
		}

	// If n <= 0, delete the region text.
	if(n <= 0 && n != INT_MIN) {
		si.curwin->w_face.wf_point = region.r_point;
		if(kprep(0) == Success)
			(void) ldelete(region.r_size,EditDel);
		}

	return rc.status;
	}

// Wrap word on white space.  Beginning at point going backward, stop on the first word break (space character) or column n
// (which must be >= 0).  If we reach column n and n is before point, jump back to the point position and (i), if default n:
// start a new line; or (ii), if not default n: scan forward to a word break (and delete all white space) or end of line and
// start a new line.  Otherwise, break the line at the word break or point, delete all white space, and jump forward to the
// point position.  Make sure we force the display back to the left edge of the current window.
int wrapWord(Datum *rval,int n,Datum **argv) {
	int wordsz;		// Size of word wrapped to next line.
	int lmargin,origoff;
	Point *point = &si.curwin->w_face.wf_point;

	// Determine left margin.
	if(n == INT_MIN)
		lmargin = 0;
	else if((lmargin = n) < 0)
		return rcset(Failure,0,text39,text322,n,0);
			// "%s (%d) must be %d or greater","Column number"

	// If blank line, do nothing.
	if(point->lnp->l_used == 0)
		return rc.status;

	// Scan backward to first space character, if any.
	origoff = point->off;
	wordsz = -1;
	while(point->off > 0) {

		// Back up one character.
		(void) movech(-1);
		++wordsz;
		if(point->lnp->l_text[point->off] == ' ')
			break;
		}

	// If we are now at or past the left margin, start a new line.
	if(point->off == 0 || getcol(NULL,true) <= lmargin) {

		// Go back to starting point and hunt forward for a break.
		point->off = origoff;
		while(point->off < point->lnp->l_used) {
			if(point->lnp->l_text[point->off] == ' ') {
				if(delwhite(0,false) != Success)
					return rc.status;
				break;
				}
			(void) movech(1);
			}
		return lnewline();
		}

	// Found a space.  Replace it with a newline.
	if(delwhite(0,false) != Success || lnewline() != Success)
		return rc.status;

	// Move back to where we started.
	if(wordsz > 0)
		(void) movech(wordsz);

	// Make sure the display is not horizontally scrolled.
	if(si.curwin->w_face.wf_firstcol > 0) {
		si.curwin->w_face.wf_firstcol = 0;
		si.curwin->w_flags |= (WFHard | WFMode);
		}

	return rc.status;
	}

// Wrap line(s) in a block specified by n argument.  Duplicate indentation from first line in all subsequent lines.  If script
// mode, also add value of first argument after indentation (for example, "// " or "# "), and pass second argument to joinln().
// No error if attempt to move past a buffer boundary.
int wrapLine(Datum *rval,int n,Datum **argv) {
	Point *point;
	Datum *indent = NULL;		// Leading white space on first line, if any.
	Datum *prefix = NULL;		// First argument if script mode.
	Datum *delim = NULL;		// Second argument if script mode.
	int indentcol,lmargin,col;
	int prefixLen = 0;

	// Wrap column set?
	if(si.wrapcol == 0)
		return rcset(Failure,RCNoFormat,text98);
			// "Wrap column not set"

	// Get prefix and end-sentence delimiters if script mode.
	if(si.opflags & OpScript) {
		if(!disnn(argv[0])) {
			prefix = argv[0];
			if(*prefix->d_str == ' ' || *prefix->d_str == '\t')
				return rcset(Failure,0,text303,prefix->d_str);
					// "Invalid wrap prefix \"%s\""
			else
				prefixLen = strlen(prefix->d_str);
			}
		if(!disnn(argv[1]))
			delim = argv[1];
		}

	// Determine bounds of line block.
	point = &si.curwin->w_face.wf_point;
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n) != Success)			// Process all lines in region.
		return rc.status;
	else if(n < 0) {						// Back up to first line.
		int count = 1;
		while(point->lnp != si.curbuf->b_lnp) {
			point->lnp = point->lnp->l_prev;
			++count;
			if(++n == 0)
				break;
			}
		if((n = count) > 1)
			si.curwin->w_flags |= WFMove;
		}
	point->off = 0;							// Move to beginning of line.

	// Point now at beginning of first line and n > 0.
	(void) begintxt();						// Move to beginning of text.
	if((indentcol = getcol(NULL,false)) + prefixLen >= si.wrapcol)	// Too much indentation?
		return rcset(Failure,0,text323,si.wrapcol);
			// "Indentation exceeds wrap column (%d)"
	if(point->off > 0) {						// Save any indentation (from first line of block)...
		if(dnewtrk(&indent) != 0 || dsetsubstr(point->lnp->l_text,point->off,indent) != 0)
			return librcset(Failure);
		if(ldelete(-point->off,0) != Success)			// and delete it.
			return rc.status;
		}

	// Remove any existing prefix string from each line of block.
	if(prefixLen > 0) {
		Point opoint = *point;
		int count = n;
		char *str = prefix->d_str - 1;
		int striplen;

		// Get length of stripped prefix.
		for(striplen = prefixLen; striplen > 1 && (str[striplen] == ' ' || str[striplen] == '\t'); --striplen);
		do {
			(void) begintxt();
			if(point->lnp->l_used - point->off >= striplen && memcmp(point->lnp->l_text + point->off,prefix->d_str,
			 striplen) == 0 && (ldelete(striplen,0) != Success || (count == n && delwhite(0,false) != Success)))
				return rc.status;
			} while((point->lnp = point->lnp->l_next) != NULL && --count > 0);
		*point = opoint;
		}

	if(n > 1 && joinln(rval,n,delim) != Success)			// Convert line block to single line.
		return rc.status;
	(void) trimln();						// Delete any trailing white space.
	if(point->lnp->l_used > 0) {
		short c;
		int attrLen;
		lmargin = indentcol + prefixLen;

		// Wrap current line until too short to wrap any further.
		for(;;) {
#if MMDebug & Debug_Wrap
			attrLen = point->lnp->l_used < term.t_ncol ? point->lnp->l_used : term.t_ncol;
			fprintf(logfile,"wrapLine(): wrapping line:\n%.*s\n",attrLen,point->lnp->l_text);
			attrLen = 1;
			do {
				fprintf(logfile,"....+....%d",attrLen % 10);
				} while(++attrLen <= term.t_ncol / 10);
			fputc('\n',logfile);
#endif
			point->off = 0;					// Move to beginning of line.

			// Insert indentation and prefix string.
			if((indent != NULL && linstr(indent->d_str) != Success) ||
			 (prefix != NULL && linstr(prefix->d_str) != Success))
				return rc.status;

			// Move forward until hit end of line or first non-whitespace character past wrap column.
			col = lmargin;
			for(;;) {
				// Get next character and skip over any terminal attribute specification.
#if MMDebug & Debug_Wrap
				c = point->lnp->l_text[point->off];
				fprintf(logfile,"At col: %3d  offset: %3d  char: %c ",col,point->off,c == 011 ? '*' : c);
#endif
				col = newcolTA(point,col,&attrLen);
				if(attrLen > 0)
					point->off += attrLen - 1;
#if MMDebug & Debug_Wrap
				c = point->lnp->l_text[point->off];
			fprintf(logfile," ->  new col: %3d  new offset: %3d  new char: %c\n",col,point->off,c == 011 ? '*' : c);
#endif
				// If now past wrap column and not on a whitespace character, wrap line.
				if(col > si.wrapcol && ((c = point->lnp->l_text[point->off]) != ' ' && c != '\t')) {
					if(wrapWord(rval,lmargin,argv) != Success)
						return rc.status;
					break;
					}

				// If at end of line, we're done.
				if(++point->off == point->lnp->l_used)
					goto Retn;
				}
			if(point->lnp->l_used == 0)			// Blank line after wrap?
				return ldelete(1L,0);			// Yes, done.
			}
		}
Retn:
	(void) movech(1);						// Can't fail.
	return rc.status;
	}

// Move point forward by the specified number of words (wcount) or characters (ccount), converting characters to upper or lower
// case (ultab != NULL) or words to title case (ultab == NULL).  Processing stops when either wcount or ccount reaches zero,
// both of which are assumed to be greater than zero.  If changed not NULL, set *changed to "changed" status; otherwise, set
// window update flag.
static void casetext(int wcount,long ccount,char *ultab,bool *changed) {
	short c;
	bool firstc;
	bool chgd = false;
	Point *point = &si.curwin->w_face.wf_point;
	bool (*isul)(short c) = (ultab == NULL) ? NULL : (ultab == upcase) ? is_lower : is_upper;

	do {
		while(!inword()) {
			if(movech(1) != Success || --ccount == 0)
				goto Retn;
			}
		firstc = true;
		while(inword()) {
			c = point->lnp->l_text[point->off];
			if(ultab == NULL) {
				if(firstc == is_lower(c)) {
					point->lnp->l_text[point->off] = firstc ? upcase[c] : lowcase[c];
					chgd = true;
					}
				}
			else if(isul(c)) {
				point->lnp->l_text[point->off] = ultab[c];
				chgd = true;
				}
			firstc = false;
			if(movech(1) != Success || --ccount == 0)
				goto Retn;
			}
		} while(--wcount > 0);
Retn:
	if(changed != NULL)
		*changed = chgd;
	else if(chgd)
		bchange(si.curbuf,WFHard);
	}

// Convert a block of lines to lower, title, or upper case.  If n is zero, use lines in current region.  If trantab is NULL, do
// title case.  No error if attempt to move past end of buffer.  Always leave point at end of line block.
static int caseline(int n,char *trantab) {
	bool changed;
	int inc,count;
	Point *point = &si.curwin->w_face.wf_point;
	Point wkpoint = *point;		// Original point position.

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

	point->off = 0;			// Start at the beginning.
	if(n > 0)
		mset(&si.curbuf->b_mroot,si.curwin);

	// Loop through buffer text, changing case of n lines.
	kentry.lastflag &= ~CFVMove;
	for(count = 0; n != 0; n -= inc) {

		// Process the current line from the beginning.
		if(point->lnp->l_used > 0) {
			point->off = 0;
			casetext(INT_MAX,point->lnp->l_used,trantab,&changed);
			if(changed)
				++count;
			}

		// Move forward or backward to the next line.
		point->off = 0;
		if(moveln(inc) != Success)
			break;
		}

	return lchange(&wkpoint,inc == -1,count);
	}

// Convert text objects per flags: n words (CaseWord), n lines (CaseLine), or a region (CaseRegion) to lower case (CaseLower),
// title case (CaseTitle), or upper case (CaseUpper).  If n == 0 and CaseLine flag set, convert lines in current region.  If
// n < 0, convert words and lines backward from point; othewise, forward.  No error if attempt to move past end of buffer.  If
// converting words, point is left at ending position (forward or backward from point); if converting lines or a region, point
// is left at end of line block or region (forward from point always).  Return status.
int cvtcase(int n,ushort flags) {
	char *trantab = flags & CaseUpper ? upcase : flags & CaseLower ? lowcase : NULL;

	// Process region.
	if(flags & CaseRegion) {
		Region region;

		if(getregion(&region,RegForceBegin) == Success && region.r_size > 0) {
			Point *point = &si.curwin->w_face.wf_point;

			// Move point or mark so that (1), point is at beginning of region before case conversion; (2), point
			// will be left at end of region after case conversion; and (3), region will still be marked.
			if(region.r_point.lnp != point->lnp || region.r_point.off != point->off)

				// Mark is before point... move point there and leave mark as is.
				*point = region.r_point;
			else
				// Mark is after point... set mark at point.
				mset(&si.curbuf->b_mroot,si.curwin);

			casetext(INT_MAX,region.r_size,trantab,NULL);
			if(!(si.opflags & OpScript))
				(void) rcset(Success,0,"%s %s",text466,text260);
							// "Case of text","changed"
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

			mset(&si.curbuf->b_mroot,si.curwin);
			(void) movewd(n);
			if(getregion(&region,RegForceBegin) == Success && region.r_size > 0) {
				casetext(INT_MAX,region.r_size,trantab,NULL);
				si.curwin->w_face.wf_point = region.r_point;
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
	Point *point = &si.curwin->w_face.wf_point;

	// Check if at end of buffer.
	if(bufend(point))
		return rcset(Failure,RCNoFormat,text259);
				// "No text selected"

	// Save the current point position.
	region.r_point = *point;
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
			while(point->off == point->lnp->l_used) {
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
		while(point->off == point->lnp->l_used || (c = point->lnp->l_text[point->off]) == ' ' ||
		 c == '\t') {
			if(movech(1) != Success)
				break;
			++region.r_size;
			}
		}
Nuke:
	// Have region... restore original position and kill, delete, or copy it.
	*point = region.r_point;
	if(kdc <= 0) {

		// Kill or delete the word(s).
		if(kprep(kdc) == Success)
			(void) ldelete(region.r_size,kdc ? EditKill : EditDel);
		return rc.status;
		}

	// Copy the word(s).
	return copyreg(&region) != Success || (si.opflags & OpScript) ? rc.status :
	 rcset(Success,RCHigh,"%s%s %s",text115,oneword ? "" : "s",text262);
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
	region.r_point = si.curwin->w_face.wf_point;
	region.r_size = -size;
	region.r_linect = 0;				// Not used.
	return copyreg(&region) != Success || (si.opflags & OpScript) ? rc.status :
	 rcset(Success,RCHigh,"%s%s %s",text115,oneword ? "" : "s",text262);
				// "Word","copied"
	}
