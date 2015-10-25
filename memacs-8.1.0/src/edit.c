// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// edit.c	Text editing functions for MightEMacs.
//
// These routines edit lines in the current window and are the only routines that touch the text.  They also touch the buffer
// and window structures to make sure that the necessary updating gets done.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"
#include <stdio.h>

#define BSIZE(a)	(a + NBLOCK - 1) & (~(NBLOCK - 1))

// Initialize dot position, marks, and first column position of a face record, given line pointer.
void faceinit(WindFace *wfp,Line *lnp) {
	Mark *mkp,*mkpz;

	wfp->wf_toplnp = wfp->wf_dot.lnp = lnp;
	wfp->wf_dot.off = wfp->wf_fcol = 0;

	// Clear all marks.
	mkpz = (mkp = wfp->wf_mark) + NMARKS;
	do {
		mkp->mk_dot.lnp = NULL;
		mkp->mk_dot.off = mkp->mk_force = 0;
		} while(++mkp < mkpz);
	}

// Allocate a block of memory large enough to hold a Line containing "used" characters and set *lnpp to the new block.
// Return status.
int lalloc(int used,Line **lnpp) {
	Line *lnp;

	if((lnp = (Line *) malloc(sizeof(Line) + used)) == NULL)
		return rcset(PANIC,0,text94,"lalloc");
			// "%s(): Out of memory!"

	lnp->l_size = lnp->l_used = used;
	*lnpp = lnp;
	return rc.status;
	}

// Fix "window face" settings.
static void fixfree(WindFace *wfp,Line *lnp) {
	Mark *mkp,*mkpz;

	if(wfp->wf_toplnp == lnp)
		wfp->wf_toplnp = lforw(lnp);
	if(wfp->wf_dot.lnp == lnp) {
		wfp->wf_dot.lnp = lforw(lnp);
		wfp->wf_dot.off = 0;
		}
	mkpz = (mkp = wfp->wf_mark) + NMARKS;
	do {
		if(mkp->mk_dot.lnp == lnp) {
			mkp->mk_dot.lnp = lforw(lnp);
			mkp->mk_dot.off = 0;
			}
		} while(++mkp < mkpz);
	}

// Delete line "lnp".  Fix all of the links that might point at it (they are moved to offset 0 of the next line.  Unlink
// the line from whatever buffer it might be in.  Release the memory and update the buffers.
void lfree(Line *lnp) {
	Buffer *bufp;
	EScreen *scrp;
	EWindow *winp;

	// In all screens ...
	scrp = sheadp;
	do {
		winp = scrp->s_wheadp;
		do {
			fixfree(&winp->w_face,lnp);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In all buffers ...
	bufp = bheadp;
	do {
		fixfree(&bufp->b_face,lnp);
		} while((bufp = bufp->b_nextp) != NULL);

	// Remove line from linked list and release its heap space.
	lnp->l_prevp->l_nextp = lnp->l_nextp;
	lnp->l_nextp->l_prevp = lnp->l_prevp;
	free((void *) lnp);
	}

// This routine gets called when a buffer is changed (edited) in any way.  It updates all of the required flags in the buffer
// and windowing system.  The minimal flag is passed as an argument; if the buffer is being displayed in more than one window we
// change EDIT to HARD.  Also set MODE if the mode line needs to be updated (the "*" has to be displayed) and free any macro
// preprocessing storage.
void lchange(Buffer *bufp,uint flags) {
	EWindow *winp;
	EScreen *scrp;

	if(bufp->b_nwind != 1)			// Hard update needed?
		flags = WFHARD;			// Yes.
	if((bufp->b_flags & BFCHGD) == 0) {	// If first change ...
		flags |= WFMODE;		// need to update mode lines also.
		bufp->b_flags |= BFCHGD;
		}
	ppfree(bufp);				// Force macro preprocessor redo.

	// In all screens ...
	scrp = sheadp;
	do {
		// Make sure all the needed windows get these flags.
		winp = scrp->s_wheadp;
		do {
			if(winp->w_bufp == bufp)
				winp->w_flags |= flags;
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);
	}

// Fix "window face" settings.
static void fixins(int offset,int n,WindFace *wfp,Line *lnp1,Line *lnp2) {
	Mark *mkp,*mkpz;

	if(wfp->wf_toplnp == lnp1)
		wfp->wf_toplnp = lnp2;
	if(wfp->wf_dot.lnp == lnp1) {
		wfp->wf_dot.lnp = lnp2;
		if(wfp->wf_dot.off >= offset)
			wfp->wf_dot.off += n;
		}
	mkpz = (mkp = wfp->wf_mark) + NMARKS;
	do {
		if(mkp->mk_dot.lnp == lnp1) {
			mkp->mk_dot.lnp = lnp2;
			if(mkp->mk_dot.off > offset)
				mkp->mk_dot.off += n;
			}
		} while(++mkp < mkpz);
	}

// Insert "n" copies of the character "c" at the current point.  In the easy case all that happens is the text is stored in the
// line.  In the hard case, the line has to be reallocated.  When the window list is updated, need to (1), always update dot in
// the current window; and (2), update mark and dot in other windows if it is greater than the place where the insert was done.
// Return status.
int linsert(int n,int c) {
	char *strp1,*strp2;
	Line *lnp1,*lnp2,*lnp3;
	int offset;
	int i;
	EWindow *winp;
	EScreen *scrp;		// Screen to fix pointers in.
	Buffer *bufp;
	bool eob;		// True if at end-of-buffer.

	if(allowedit(true) != SUCCESS)				// Don't allow if in read-only mode.
		return rc.status;

	// A zero repeat count means do nothing!
	if(n == 0)
		return rc.status;

	// Negative repeat count is an error.
	if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"

	// Mark the current window's buffer as changed.
	lchange(curbp,WFEDIT);

	// Get current line and determine the type of insert.
	lnp1 = curwp->w_face.wf_dot.lnp;
	offset = curwp->w_face.wf_dot.off;

	if(lnp1 == curbp->b_hdrlnp) {				// Header line.
		eob = true;

		// Allocate new line.
		if(lalloc(BSIZE(n),&lnp2) != SUCCESS)		// New line.
			return rc.status;			// Fatal error.
		lnp2->l_used = n;				// Set "used" length.
		lnp3 = lnp1->l_prevp;				// Previous line.
		lnp3->l_nextp = lnp2;				// Link in new line.
		lnp2->l_nextp = lnp1;
		lnp1->l_prevp = lnp2;
		lnp2->l_prevp = lnp3;
		i = 0;						// Store line data.
		do {
			lnp2->l_text[i++] = c;
			} while(i < n);
		}
	else {
		// Not at end of buffer.
		eob = false;
		if(lnp1->l_used + n > lnp1->l_size) {		// Not enough room left in line: reallocate.
			if(lalloc(BSIZE(lnp1->l_used + n),&lnp2) != SUCCESS)
				return rc.status;		// Fatal error.
			lnp2->l_used = lnp1->l_used + n;	// Set new "used" length.
			strp1 = lnp1->l_text;			// Copy old to new up to dot.
			strp2 = lnp2->l_text;
			while(strp1 != lnp1->l_text + offset)
				*strp2++ = *strp1++;
			strp2 += n;				// Make gap and copy remainder.
			while(strp1 != lnp1->l_text + lnp1->l_used)
				*strp2++ = *strp1++;
			lnp1->l_prevp->l_nextp = lnp2;		// Link in the new line.
			lnp2->l_nextp = lnp1->l_nextp;
			lnp1->l_nextp->l_prevp = lnp2;
			lnp2->l_prevp = lnp1->l_prevp;
			free((void *) lnp1);			// Get rid of the old one.
			}
		else {						// Easy: update in place.
			lnp2 = lnp1;				// Make gap in line for new character(s).
			lnp2->l_used += n;
			strp2 = lnp1->l_text + lnp1->l_used;
			strp1 = strp2 - n;
			while(strp1 != lnp1->l_text + offset)
				*--strp2 = *--strp1;
			}

		for(i = 0; i < n; ++i)				// Store the new character(s) in the "gap".
			lnp2->l_text[offset + i] = c;
		}

	//  In all screens ...
	scrp = sheadp;
	do {
		winp = scrp->s_wheadp;
		do {
			fixins(offset,n,&winp->w_face,lnp1,lnp2);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In all buffers ...
	bufp = bheadp;
	do {
		fixins(offset,n,&bufp->b_face,lnp1,lnp2);
		} while((bufp = bufp->b_nextp) != NULL);

	return rc.status;
	}

// Fix "window face" settings.
static void fixinsnl(int offset,WindFace *wfp,Line *lnp1,Line *lnp2) {
	Mark *mkp,*mkpz;

	if(wfp->wf_toplnp == lnp1)
		wfp->wf_toplnp = lnp2;
	if(wfp->wf_dot.lnp == lnp1) {
		if(wfp->wf_dot.off < offset)
			wfp->wf_dot.lnp = lnp2;
		else
			wfp->wf_dot.off -= offset;
		}
	mkpz = (mkp = wfp->wf_mark) + NMARKS;
	do {
		if(mkp->mk_dot.lnp == lnp1) {
			if(mkp->mk_dot.off < offset)
				mkp->mk_dot.lnp = lnp2;
			else
				mkp->mk_dot.off -= offset;
			}
		} while(++mkp < mkpz);
	}

// Insert a newline at the current point.  Return status.  The funny ass-backward way it does things is not a botch; it just
// makes the last line in the buffer not a special case.  The update of dot and mark is a bit easier than in the above case,
// because the split forces more updating.
int lnewline(void) {
	char *strp1,*strp2;
	Line *lnp1,*lnp2;
	int offset;
	EScreen *scrp;
	EWindow *winp;
	Buffer *bufp;

	if(allowedit(true) != SUCCESS)		// Don't allow if in read-only mode.
		return rc.status;

	lchange(curbp,WFHARD);
	lnp1 = curwp->w_face.wf_dot.lnp;		// Get the address and ...
	offset = curwp->w_face.wf_dot.off;		// offset of dot.
	if(lalloc(offset,&lnp2) != SUCCESS)	// New first half line.
		return rc.status;		// Fatal error.
	strp1 = lnp1->l_text;			// Shuffle text around.
	strp2 = lnp2->l_text;
	while(strp1 != lnp1->l_text + offset)
		*strp2++ = *strp1++;
	strp2 = lnp1->l_text;
	while(strp1 != lnp1->l_text + lnp1->l_used)
		*strp2++ = *strp1++;
	lnp1->l_used -= offset;
	lnp2->l_prevp = lnp1->l_prevp;
	lnp1->l_prevp = lnp2;
	lnp2->l_prevp->l_nextp = lnp2;
	lnp2->l_nextp = lnp1;

	// In all screens ...
	scrp = sheadp;
	do {
		winp = scrp->s_wheadp;
		do {
			fixinsnl(offset,&winp->w_face,lnp1,lnp2);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In all buffers ...
	bufp = bheadp;
	do {
		fixinsnl(offset,&bufp->b_face,lnp1,lnp2);
		} while((bufp = bufp->b_nextp) != NULL);

	return rc.status;
	}

// Insert a string at the current point.  "s" may be NULL.
int linstr(char *s) {

	if(s == NULL)
		return rc.status;
	while(*s != '\0') {
		if(((*s == '\r') ? lnewline() : linsert(1,*s)) != SUCCESS)
			return rc.status;
		++s;
		}
	return rc.status;
	}

// Fix "window face" settings.
static void fixdelnl1(WindFace *wfp,Line *lnp1,Line *lnp2) {
	Mark *mkp,*mkpz;

	if(wfp->wf_toplnp == lnp2)
		wfp->wf_toplnp = lnp1;
	if(wfp->wf_dot.lnp == lnp2) {
		wfp->wf_dot.lnp = lnp1;
		wfp->wf_dot.off += lnp1->l_used;
		}
	mkpz = (mkp = wfp->wf_mark) + NMARKS;
	do {
		if(mkp->mk_dot.lnp == lnp2) {
			mkp->mk_dot.lnp = lnp1;
			mkp->mk_dot.off += lnp1->l_used;
			}
		} while(++mkp < mkpz);
	}

// Fix "window face" settings.
static void fixdelnl2(WindFace *wfp,Line *lnp1,Line *lnp2,Line *lnp3) {
	Mark *mkp,*mkpz;

	if(wfp->wf_toplnp == lnp1 || wfp->wf_toplnp == lnp2)
		wfp->wf_toplnp = lnp3;
	if(wfp->wf_dot.lnp == lnp1)
		wfp->wf_dot.lnp = lnp3;
	else if(wfp->wf_dot.lnp == lnp2) {
		wfp->wf_dot.lnp = lnp3;
		wfp->wf_dot.off += lnp1->l_used;
		}
	mkpz = (mkp = wfp->wf_mark) + NMARKS;
	do {
		if(mkp->mk_dot.lnp == lnp1)
			mkp->mk_dot.lnp = lnp3;
		else if(mkp->mk_dot.lnp == lnp2) {
			mkp->mk_dot.lnp = lnp3;
			mkp->mk_dot.off += lnp1->l_used;
			}
		} while(++mkp < mkpz);
	}

// Delete a newline.  Join the current line with the next line.  If the next line is the magic header line, always return
// SUCCESS; merging the last line with the header line can be thought of as always being a successful operation, even if nothing
// is done, and this makes the kill buffer work "right".  Easy cases can be done by shuffling data around.  Hard cases require
// that lines be moved about in memory.  Called by ldelete() only.
static int ldelnewline(void) {
	char *strp1,*strp2;
	Line *lnp1,*lnp2,*lnp3;
	EScreen *scrp;
	EWindow *winp;
	Buffer *bufp;

	lnp1 = curwp->w_face.wf_dot.lnp;
	lnp2 = lnp1->l_nextp;
	if(lnp2 == curbp->b_hdrlnp) {		// At the buffer end.
		if(lnp1->l_used == 0)		// Blank line.
			lfree(lnp1);
		return rc.status;
		}

	// Do simple join if room in current line for next line.
	if(lnp2->l_used <= lnp1->l_size - lnp1->l_used) {
		strp1 = lnp1->l_text + lnp1->l_used;
		strp2 = lnp2->l_text;
		while(strp2 != lnp2->l_text + lnp2->l_used)
			*strp1++ = *strp2++;

		// In all screens ...
		scrp = sheadp;
		do {
			winp = scrp->s_wheadp;
			do {
				fixdelnl1(&winp->w_face,lnp1,lnp2);
				} while((winp = winp->w_nextp) != NULL);
			} while((scrp = scrp->s_nextp) != NULL);

		// In all buffers ...
		bufp = bheadp;
		do {
			fixdelnl1(&bufp->b_face,lnp1,lnp2);
			} while((bufp = bufp->b_nextp) != NULL);

		lnp1->l_used += lnp2->l_used;
		lnp1->l_nextp = lnp2->l_nextp;
		lnp2->l_nextp->l_prevp = lnp1;
		free((void *) lnp2);
		return rc.status;
		}

	// Simple join not possible, get more space.
	if(lalloc(lnp1->l_used + lnp2->l_used,&lnp3) != SUCCESS)
		return rc.status;	// Fatal error.
	strp1 = lnp1->l_text;
	strp2 = lnp3->l_text;
	while(strp1 != lnp1->l_text + lnp1->l_used)
		*strp2++ = *strp1++;
	strp1 = lnp2->l_text;
	while(strp1 != lnp2->l_text + lnp2->l_used)
		*strp2++ = *strp1++;
	lnp1->l_prevp->l_nextp = lnp3;
	lnp3->l_nextp = lnp2->l_nextp;
	lnp2->l_nextp->l_prevp = lnp3;
	lnp3->l_prevp = lnp1->l_prevp;

	// In all screens ...
	scrp = sheadp;
	do {
		winp = scrp->s_wheadp;
		do {
			fixdelnl2(&winp->w_face,lnp1,lnp2,lnp3);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In all buffers ...
	bufp = bheadp;
	do {
		fixdelnl2(&bufp->b_face,lnp1,lnp2,lnp3);
		} while((bufp = bufp->b_nextp) != NULL);

	free((void *) lnp1);
	free((void *) lnp2);
	return rc.status;
	}

// Fix dot offset after delete.
static void fixdotdel(int offset,int chunk,Dot *dotp) {
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

// Fix "window face" settings.
static void fixdel(int offset,int chunk,WindFace *wfp,Line *lnp) {
	Mark *mkp,*mkpz;

	if(wfp->wf_dot.lnp == lnp)
		fixdotdel(offset,chunk,&wfp->wf_dot);
	mkpz = (mkp = wfp->wf_mark) + NMARKS;
	do {
		if(mkp->mk_dot.lnp == lnp)
			fixdotdel(offset,chunk,&mkp->mk_dot);
		} while(++mkp < mkpz);
	}

// This function deletes "n" bytes, starting at dot.  Positive n deletes forward; negative n deletes backward.  It understands
// how to deal with end of lines, and with two-byte characters.  It returns current status if all of the characters were
// deleted, NOTFOUND (bypassing rcset()) if they were not (because dot ran into a buffer boundary), or the appropriate status if
// an error occurred.  The deleted text is put in the kill buffer if the DFKILL flag is set; otherwise, the undelete buffer if
// the DFDEL flag is set.
int ldelete(long n,uint flags) {
	char *strp1,*strp2;
	Line *lnp;
	int offset,chunk;
	EScreen *scrp;
	EWindow *winp;
	Buffer *bufp;
	Kill *kp;

	if(allowedit(true) != SUCCESS)			// Don't allow if in read-only mode.
		return rc.status;

	// Set delete buffer pointer.
	kp = (flags & DFKILL) ? kringp : (flags & DFDEL) ? &undelbuf : NULL;

	// Going forward?
	if(n >= 0) {
		while(n > 0) {

			// Get the current point.
			lnp = curwp->w_face.wf_dot.lnp;
			offset = curwp->w_face.wf_dot.off;

			// Can't delete past the end of the buffer.
			if(lnp == curbp->b_hdrlnp)
				return NOTFOUND;

			// Find out how many chars to delete on this line.
			chunk = lnp->l_used - offset;	// Size of chunk.
			if(chunk > n)
				chunk = n;

			// If at the end of a line, merge with the next.
			if(chunk == 0) {

				// Force loop exit if at end of last line in buffer.
				if(curwp->w_face.wf_dot.lnp->l_nextp == curbp->b_hdrlnp)
					n = 1;

				// Flag that we are making a hard change and delete newline.
				lchange(curbp,WFHARD);
				if(ldelnewline() != SUCCESS || ((kp != NULL) && kinsert(kp,FORWARD,'\r') != SUCCESS))
					return rc.status;
				--n;
				continue;
				}

			// Flag the fact we are changing the current line.
			lchange(curbp,WFEDIT);

			// Find the limits of the kill.
			strp1 = lnp->l_text + offset;
			strp2 = strp1 + chunk;

			// Save the text to the kill buffer.
			if((kp != NULL)) {
				while(strp1 != strp2) {
					if(kinsert(kp,FORWARD,*strp1) != SUCCESS)
						return rc.status;
					++strp1;
					}
				strp1 = lnp->l_text + offset;
				}

			// Copy what is left of the line upward.
			while(strp2 != lnp->l_text + lnp->l_used)
				*strp1++ = *strp2++;
			lnp->l_used -= chunk;

			// Fix any other windows with the same text displayed.  In all screens ...
			scrp = sheadp;
			do {
				winp = scrp->s_wheadp;
				do {
					fixdel(offset,chunk,&winp->w_face,lnp);

					// Onward to the next window.
					} while((winp = winp->w_nextp) != NULL);
				} while((scrp = scrp->s_nextp) != NULL);

			// In all buffers ...
			bufp = bheadp;
			do {
				fixdel(offset,chunk,&bufp->b_face,lnp);
				} while((bufp = bufp->b_nextp) != NULL);

			// Indicate we have deleted chunk characters.
			n -= chunk;
			}
		}
	else {
		while(n < 0) {

			// Get the current point.
			lnp = curwp->w_face.wf_dot.lnp;
			offset = curwp->w_face.wf_dot.off;

			// Can't delete past the beginning of the buffer.
			if(lnp == lforw(curbp->b_hdrlnp) && (offset == 0))
				return NOTFOUND;

			// Find out how many chars to delete on this line.
			chunk = offset;		// Size of chunk.
			if(chunk > -n)
				chunk = -n;

			// If at the beginning of a line, merge with the last.
			if(chunk == 0) {

				// Flag that we are making a hard change and delete newline.
				lchange(curbp,WFHARD);
				(void) backch(1);
				if(ldelnewline() != SUCCESS || ((kp != NULL) && kinsert(kp,BACKWARD,'\r') != SUCCESS))
					return rc.status;
				++n;
				continue;
				}

			// Flag the fact we are changing the current line.
			lchange(curbp,WFEDIT);

			// Find the limits of the kill.
			strp1 = lnp->l_text + offset;
			strp2 = strp1 - chunk;

			// Save the text to the kill buffer.
			if((kp != NULL)) {
				while(strp1 > strp2) {
					if(kinsert(kp,BACKWARD,*(--strp1)) != SUCCESS)
						return rc.status;
					}
				strp1 = lnp->l_text + offset;
				}

			// Copy what is left of the line downward.
			while(strp1 != lnp->l_text + lnp->l_used)
				*strp2++ = *strp1++;
			lnp->l_used -= chunk;
			curwp->w_face.wf_dot.off -= chunk;

			// Fix any other windows with the same text displayed.  In all screens ...
			scrp = sheadp;
			do {
				winp = scrp->s_wheadp;
				do {
					fixdel(offset,-chunk,&winp->w_face,lnp);

					// Onward to the next window.
					} while((winp = winp->w_nextp) != NULL);
				} while((scrp = scrp->s_nextp) != NULL);

			// In all buffers ...
			bufp = bheadp;
			do {
				fixdel(offset,-chunk,&bufp->b_face,lnp);
				} while((bufp = bufp->b_nextp) != NULL);

			// Update delete count.
			n += chunk;
			}
		}

	return rc.status;
	}

// Quote the next character, and insert it into the buffer.  All the characters are taken literally, including the newline,
// which does not then have its line splitting meaning.  The character is always read, even if it is inserted zero times, so
// that the command completes normally.  If a mouse action or function key is pressed, its symbolic name gets inserted.
int quoteChar(Value *rp,int n) {
	int c;				// Key fetched.
	char key_name[16];		// Name of a keystroke for quoting.

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)			// Fail on a negative argument.
		return rcset(FAILURE,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Command repeat count"

	// Get the key.
	if(getkey(&c) != SUCCESS)
		return rc.status;

	// If this is a function or special key, put its name in.
	if(c & (FKEY | SHFT)) {
		ectos(c,key_name,true);
		while(n-- > 0) {
			if(linstr(key_name) != SUCCESS)
				break;
			}
		return rc.status;
		}

	// Otherwise, just insert the raw character n times.
	return linsert(n,ectoc(c));
	}

// Set soft tab size to abs(n) if n <= 0; otherwise, insert a tab or spaces into the buffer n times.
int instab(int n) {

	// Non-positive n?
	if(n <= 0)
		// Set soft tab size.
		(void) settab(abs(n),false);

	// Positive n.  Insert tab or spaces n times.
	else if(stabsize == 0)
		(void) linsert(n,'\t');
	else
		do {
			if(linsert(stabsize - (getccol() % stabsize),' ') != SUCCESS)
				break;
			} while(--n > 0);

	return rc.status;
	}

// Change tabs to spaces.  If argument is zero, use the current region.  No error if attempt to move past the end of the buffer.
int detabLine(Value *rp,int n) {
	int (*nextln)(int n);
	int inc;		// Increment to next line.
	Dot *dotp = &curwp->w_face.wf_dot;

	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != SUCCESS)
		return rc.status;

	// Loop thru text, detabbing n lines.
	if(n < 0) {
		--n;
		inc = -1;
		nextln = backln;
		}
	else {
		inc = 1;
		nextln = forwln;
		}

	kentry.lastflag &= ~CFVMOV;
	while(n != 0) {
		dotp->off = 0;		// Start at the beginning.

		// Detab the entire current line.
		while(dotp->off < lused(dotp->lnp)) {

			// If we have a tab.
			if(lgetc(dotp->lnp,dotp->off) == '\t') {
				if(ldelete(1L,0) != SUCCESS ||
				 feval(rp,htabsize - (dotp->off % htabsize),cftab + cf_insertSpace) != SUCCESS)
					return rc.status;
				}
			++dotp->off;
			}

		// Move to the next line.
		dotp->off = 0;
		if(nextln(1) != SUCCESS)
			break;
		n -= inc;
		}

	if(nextln == backln)
		(void) forwln(1);
	kentry.thisflag &= ~CFVMOV;		// Flag that this resets the goal column.
	lchange(curbp,WFEDIT);			// Text changed.
	return rc.status;
	}

// Define column-calculator macro.
#define nextab(a)	(a - (a % htabsize)) + htabsize

// Change spaces to tabs where possible.  If argument is zero, use the current region.  No error if attempt to move past the
// end of the buffer.
int entabLine(Value *rp,int n) {
	int (*nextln)(int n);
	int inc;		// Increment to next line.
	int fspace;		// Index of first space if in a run.
	int ccol;		// Current cursor column.
	int cchar;		// Current character.
	int len;
	Dot *dotp = &curwp->w_face.wf_dot;

	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != SUCCESS)
		return rc.status;

	// Loop thru text, entabbing n lines.
	if(n < 0) {
		--n;
		inc = -1;
		nextln = backln;
		}
	else {
		inc = 1;
		nextln = forwln;
		}

	kentry.lastflag &= ~CFVMOV;
	while(n != 0) {
		// Entab the entire current line.
		ccol = dotp->off = 0;	// Start at the beginning.
		fspace = -1;

		while(dotp->off <= lused(dotp->lnp)) {

			// Time to compress?
			if(fspace >= 0 && nextab(fspace) <= ccol) {

				// Yes.  Skip if just a single space; otherwise, chaos ensues.
				if((len = ccol - fspace) >= 2) {
					dotp->off -= len;
					if(ldelete((long) len,0) != SUCCESS || linsert(1,'\t') != SUCCESS)
						return rc.status;
					}
				fspace = -1;
				}
			if(dotp->off == lused(dotp->lnp))
				break;

			// Get the current character and check it.
			switch(cchar = lgetc(dotp->lnp,dotp->off)) {

				case '\t':	// A tab ... expand it and fall through.
					if(ldelete(1L,0) != SUCCESS ||
					 feval(rp,htabsize - (ccol % htabsize),cftab + cf_insertSpace) != SUCCESS)
						return rc.status;

				case ' ':	// A space ... beginning of run?
					if(fspace == -1)
						fspace = ccol;
					break;

				default:	// Any other char ... reset.
					fspace = -1;
				}
			++ccol;
			++dotp->off;
			}

		// Advance/or back to the next line.
		dotp->off = 0;
		if(nextln(1) != SUCCESS)
			break;
		n -= inc;
		}

	if(nextln == backln)
		(void) forwln(1);
	kentry.thisflag &= ~CFVMOV;		// Flag that this resets the goal column.
	lchange(curbp,WFEDIT);			// Text changed.
	return rc.status;
	}

// Trim trailing whitespace from one or more lines.  If argument is zero, it trims all lines in the current region.  No error
// if attempt to move past the end of the buffer.
int trimLine(Value *rp,int n) {
	int (*nextln)(int n);
	Line *lnp;		// Current line pointer.
	int length;		// Current length.
	int inc;		// Increment to next line [sgn(n)].

	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != SUCCESS)
		return rc.status;

	// Loop thru text, trimming n lines.
	if(n < 0) {
		--n;
		inc = -1;
		nextln = backln;
		}
	else {
		inc = 1;
		nextln = forwln;
		}

	kentry.lastflag &= ~CFVMOV;
	while(n != 0) {
		lnp = curwp->w_face.wf_dot.lnp;
		curwp->w_face.wf_dot.off = 0;
		length = lused(lnp);

		// Trim the current line.
		while(length > 0) {
			if(lgetc(lnp,length - 1) != ' ' && lgetc(lnp,length - 1) != '\t')
				break;
			--length;
			}
		lnp->l_used = length;

		// Advance/or back to the next line.
		curwp->w_face.wf_dot.off = 0;
		if(nextln(1) != SUCCESS)
			break;
		n -= inc;
		}

	if(nextln == backln)
		(void) forwln(1);
	lchange(curbp,WFEDIT);
	kentry.thisflag &= ~CFVMOV;		// Flag that this resets the goal column.
	return rc.status;
	}

// Open up some blank space.  The basic plan is to insert a bunch of newlines, and then back up over them.  Everything is done
// by the subcommand processors.  They even handle the looping.
int openLine(Value *rp,int n) {
	int i;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"

	i = n;				// Insert newlines.
	do {
		if(lnewline() != SUCCESS)
			return rc.status;
		} while(--i);
	(void) backch(n);		// Backup over them all.
	return rc.status;
	}

// Get indentation of given line.  Store indentation in *vpp if found; otherwise, set to NULL.  Return status.
static int getindent(Value **vpp,Line *lnp) {
	char *strp,*strpz;

	// Find end of indentation.
	strpz = (strp = ltext(lnp)) + lused(lnp);
	while(strp < strpz) {
		if(*strp != ' ' && *strp != '\t')
			break;
		++strp;
		}

	// Save it if found.
	if(strp == ltext(lnp))
		*vpp = NULL;
	else if(vnew(vpp,false) != 0 || vsetfstr(ltext(lnp),strp - ltext(lnp),*vpp) != 0)
		return vrcset();

	return rc.status;
	}

// Insert line(s) above or below current line with same indentation.
int insertLineI(Value *rp,int n) {
	Line *lnp = curwp->w_face.wf_dot.lnp;	// Current line pointer.
	Value *indentp;				// Indentation of current line.

	if(n == INT_MIN)
		n = -1;					// Insert one line above by default.
	else if(n > 0) {				// Going forward?
		curwp->w_face.wf_dot.off = lused(lnp);	// Move to end of line ...
		return newlineI(rp,n);			// and call newlineI().
		}

	// Going backward: get indentation.
	if(getindent(&indentp,lnp) != SUCCESS)
		return rc.status;

	// Insert lines (backward) with indentation in the final line.
	do {
		curwp->w_face.wf_dot.off = 0;		// Move to beginning of line ...
		if(lnewline() != SUCCESS)		// insert a newline ...
			return rc.status;
		(void) backch(1);			// back up over it ...
		if(indentp != NULL && n == -1 &&
		 linstr(indentp->v_strp) != SUCCESS)	// and insert saved indentation if final line.
			return rc.status;
		} while(++n < 0);

	return rc.status;
	}

// Compute and insert "i" variable at point n times.
int inserti(Value *rp,int n) {
	int i;
	char *strp;
	static char myname[] = "inserti";

	if(n == INT_MIN)
		n = 1;
	i = ivar.i;

	// Going forward?
	if(n > 0)
		do {
			asprintf(&strp,ivar.format.v_strp,i);
			if(strp == NULL)
				return rcset(PANIC,0,text94,myname);
					// "%s(): Out of memory!"
			(void) linstr(strp);
			(void) free((void *) strp);
			if(rc.status != SUCCESS)
				return rc.status;
			i += ivar.inc;
			} while(--n > 0);
	else {
		int len;

		do {
			len = asprintf(&strp,ivar.format.v_strp,i);
			if(strp == NULL)
				return rcset(PANIC,0,text94,myname);
					// "%s(): Out of memory!"
			(void) linstr(strp);
			(void) free((void *) strp);
			if(rc.status != SUCCESS)
				return rc.status;
			(void) backch(len);
			i += ivar.inc;
			} while(++n < 0);
		}

	ivar.i = i;
	return rc.status;
	}

// Check for keywords or symbols at the beginning or end of the current line which indicate the beginning or end of a block. 
// Return 1 if beginning of block found, -1 if end, or 0 otherwise.  "len" is the line length minus any trailing white space and
// is assumed to be > 0.  This routine is called only when a language mode (C, MightEMacs, Perl, Ruby, or Shell) is active.
static int bebcheck(Line *lnp,int len) {
	int c,offset,txtlen;
	char *strp;

	//*** Check symbols or keywords at the end of the line. ***//

	// Left brace and not MightEMacs mode?
	if((c = lgetc(lnp,--len)) == '{')
		return (curbp->b_modes & MDMEMACS) == 0;

	// Pipe sign and Ruby mode?
	if(c == '|')
		return (curbp->b_modes & MDRUBY) != 0;

	// Right paren and Shell mode?
	if(c == ')' && (curbp->b_modes & MDSHELL))
		return 1;

	// Colon and C mode?
	if(c == ':' && len > 0 && lgetc(lnp,len - 1) != ' ')
		return (curbp->b_modes & MDC) != 0;

	// ";;" and Shell mode?
	if(len >= 1) {
		strp = ltext(lnp) + len - 1;
		if(memcmp(strp,";;",2) == 0)
			return (curbp->b_modes & MDSHELL) ? -1 : 0;
		}

	// Check four-letter keywords.
	// "else" and not MightEMacs mode, or "then" and Ruby or Shell mode?
	if(len == 3 || (len >= 4 && (lgetc(lnp,len - 4) == '\t' || lgetc(lnp,len - 4) == ' '))) {
		strp = ltext(lnp) + len - 3;
		if(memcmp(strp,"else",4) == 0)
			return (curbp->b_modes & MDMEMACS) == 0;
		if(memcmp(strp,"then",4) == 0)
			return (curbp->b_modes & (MDRUBY | MDSHELL)) != 0;
		}

	// Check five-letter keywords.
	// "!else" or "!loop" and MightEMacs mode, or "begin" and Ruby mode?
	if(len == 4 || (len >= 5 && (lgetc(lnp,len - 5) == '\t' || lgetc(lnp,len - 5) == ' '))) {
		strp = ltext(lnp) + len - 4;
		if(memcmp(strp,"!else",5) == 0 || memcmp(strp,"!loop",5) == 0)
			return (curbp->b_modes & MDMEMACS) != 0;
		if(memcmp(strp,"begin",5) == 0)
			return (curbp->b_modes & MDRUBY) != 0;
		}

	// Check other keywords.
	// "do" and C, Ruby, or Shell mode, or "rescue" and Ruby mode?
	if(((len == 1 ||
	 (len >= 2 && (lgetc(lnp,len - 2) == ' ' || lgetc(lnp,len - 2) == ';' || lgetc(lnp,len - 2) == '\t'))) &&
	 lgetc(lnp,len - 1) == 'd' && lgetc(lnp,len) == 'o'))
		return (curbp->b_modes & (MDC | MDSHELL | MDRUBY)) != 0;
	if((len == 5 || (len >= 6 && (lgetc(lnp,len - 6) == '\t' || lgetc(lnp,len - 6) == ' '))) &&
	 memcmp(ltext(lnp) + len - 5,"rescue",6) == 0)
		return (curbp->b_modes & MDRUBY) != 0;

	//*** Check keywords or symbols at the beginning of the line. ***//

	// Move to end of any line indentation.
	txtlen = curwp->w_face.wf_dot.off;
	(void) begintxt();
	offset = curwp->w_face.wf_dot.off;
	curwp->w_face.wf_dot.off = txtlen;
	txtlen = len - offset + 1;

	// ";;" and Shell mode?
	if(txtlen >= 2) {
		strp = ltext(lnp) + offset;
		if(memcmp(strp,";;",2) == 0)
			return (curbp->b_modes & MDSHELL) ? -1 : 0;
		}

	// Check two-letter keywords.
	// "if" and not MightEMacs mode?
	if(txtlen >= 3 && (lgetc(lnp,offset + 2) == '(' || lgetc(lnp,offset + 2) == ' ')) {
		strp = ltext(lnp) + offset;
		if(memcmp(strp,"if",2) == 0)
			return (curbp->b_modes & MDMEMACS) == 0;
		}

	// Check three-letter keywords.
	// "for" and not MightEMacs mode, "!if" and MightEMacs mode, or "def" and Ruby mode?
	if(txtlen >= 4 && (lgetc(lnp,offset + 3) == ' ' || lgetc(lnp,offset + 3) == '(')) {
		strp = ltext(lnp) + offset;
		if(memcmp(strp,"for",3) == 0)
			return (curbp->b_modes & MDMEMACS) == 0;
		if(memcmp(strp,"!if",3) == 0)
			return (curbp->b_modes & MDMEMACS) != 0;
		if(memcmp(strp,"def",3) == 0)
			return (curbp->b_modes & MDRUBY) != 0;
		}

	// Check four-letter keywords.
	// "elif" and Shell mode, "when" and Ruby mode?
	// Note: C "case" is handled by trailing ':'.  Ruby "next" may be "next if ..." so not checked.
	if(txtlen >= 5 && (lgetc(lnp,offset + 4) == '(' || lgetc(lnp,offset + 4) == ' ' || lgetc(lnp,offset + 4) == ';')) {
		strp = ltext(lnp) + offset;
		if(memcmp(strp,"elif",4) == 0)
			return (curbp->b_modes & MDSHELL) != 0;
		if(memcmp(strp,"when",4) == 0)
			return (curbp->b_modes & MDRUBY) != 0;
		}

	// Check five-letter keywords.
	// "while" and not MightEMacs mode, "elsif" or "until" and Perl or Ruby mode, "break" and C or Shell mode,
	// "!next" and MightEMacs mode, or "class" and Ruby mode?
	// Note: Ruby "break" may be "break if ..." so not checked.
	if(txtlen == 5 || (txtlen >= 6 && (lgetc(lnp,offset + 5) == '(' || lgetc(lnp,offset + 5) == ' ' ||
	 lgetc(lnp,offset + 5) == '\t' || lgetc(lnp,offset + 5) == ';'))) {
		strp = ltext(lnp) + offset;
		if(memcmp(strp,"while",5) == 0)
			return (curbp->b_modes & MDMEMACS) == 0;
		if(memcmp(strp,"elsif",5) == 0 || memcmp(strp,"until",5) == 0)
			return (curbp->b_modes & (MDPERL | MDRUBY)) != 0;
		if(memcmp(strp,"break",5) == 0)
			return (curbp->b_modes & (MDC | MDSHELL)) ? -1 : 0;
		if(memcmp(strp,"!next",5) == 0)
			return (curbp->b_modes & MDMEMACS) ? -1 : 0;
		if(memcmp(strp,"class",5) == 0)
			return (curbp->b_modes & MDRUBY) != 0;
		}

	// Check six-letter keywords.
	// "unless" and Perl or Ruby mode, "!macro", "!elsif", "!while", or "!until" or "!break" and MightEMacs mode,
	// "return" and not MightEMacs mode, or "module" and Ruby mode?
	if(txtlen == 6 || (txtlen >= 7 && (lgetc(lnp,offset + 6) == '(' || lgetc(lnp,offset + 6) == ' ' ||
	 lgetc(lnp,offset + 6) == '\t' || lgetc(lnp,offset + 6) == ';'))) {
		strp = ltext(lnp) + offset;
		if(memcmp(strp,"unless",6) == 0)
			return (curbp->b_modes & (MDPERL | MDRUBY)) != 0;
		if(memcmp(strp,"!macro",6) == 0 || memcmp(strp,"!elsif",6) == 0 || memcmp(strp,"!while",6) == 0 ||
		 memcmp(strp,"!until",6) == 0)
			return (curbp->b_modes & MDMEMACS) != 0;
		if(memcmp(strp,"!break",6) == 0)
			return (curbp->b_modes & MDMEMACS) ? -1 : 0;
		if(memcmp(strp,"return",6) == 0)
			return (curbp->b_modes & MDMEMACS) ? 0 : -1;
		if(memcmp(strp,"module",6) == 0)
			return (curbp->b_modes & MDRUBY) != 0;
		}

	// Check seven-letter keywords.
	// "else if" and C mode, or "!return" and MightEMacs mode?
	// Note: C "default" is handled by trailing ':'.
	if(txtlen == 7 || (txtlen >= 8 && (lgetc(lnp,offset + 7) == '(' || lgetc(lnp,offset + 7) == ':' ||
	 lgetc(lnp,offset + 7) == '\t' || lgetc(lnp,offset + 7) == ' '))) {
		strp = ltext(lnp) + offset;
		if(memcmp(strp,"else if",7) == 0)
			return (curbp->b_modes & MDC) != 0;
		if(memcmp(strp,"!return",7) == 0)
			return (curbp->b_modes & MDMEMACS) ? -1 : 0;
		}

	// Check eight-letter keywords.
	// "continue" and C or Shell mode?
	if(txtlen == 8 || (txtlen >= 9 && (lgetc(lnp,offset + 8) == ';' || lgetc(lnp,offset + 8) == ' '))) {
		strp = ltext(lnp) + offset;
		if(memcmp(strp,"continue",8) == 0)
			return (curbp->b_modes & (MDC | MDSHELL)) ? -1 : 0;
		}

	return false;
	}

// Insert given indentation before dot and remove any trailing spaces from it if hard tabs in effect.  Returns status.
static int insindent(Value *indentp) {
	Dot *dotp = &curwp->w_face.wf_dot;

	// Insert indentation.
	if(linstr(indentp->v_strp) != SUCCESS)
		return rc.status;

	// If hard tabs and indentation has trailing space(s) ...
	if(stabsize == 0 && lgetc(dotp->lnp,dotp->off -1) == ' ') {
		Line *lnp;
		int len,offset;

		// Entab indentation, then delete any trailing spaces from it.
		len = lused(dotp->lnp) - dotp->off;
		if(entabLine(indentp,1) != SUCCESS)
			return rc.status;
		(void) backch(len + 1);
		lnp = dotp->lnp;
		offset = dotp->off;
		while(offset > 0 && lgetc(lnp,offset - 1) == ' ') {
			if(ldelete(-1L,0) != SUCCESS)
				return rc.status;
			--offset;
			}
		}
	return rc.status;
	}

// Insert a newline and indentation when in a programming language mode.
static int langnewline(void) {
	int openClose;			// Was there a block open or close on current line?
	Line *lnp;
	int offset;
	Value *indentp;

	// Trim the whitespace before the point (following the left fence).
	lnp = curwp->w_face.wf_dot.lnp;
	offset = curwp->w_face.wf_dot.off;
	while(offset > 0 && (lgetc(lnp,offset - 1) == ' ' || lgetc(lnp,offset - 1) == '\t')) {
		if(ldelete(-1L,0) != SUCCESS)
			return rc.status;
		--offset;
		}

	// Check for a left brace, etc., depending on language.
	openClose = (offset == 0) ? 0 : bebcheck(lnp,offset);

	// Put in the newline.
	if(lnewline() != SUCCESS)
		return rc.status;

	// If the new line is not blank, don't indent it!
	lnp = curwp->w_face.wf_dot.lnp;
	if(lused(lnp) != 0)
		return rc.status;

	// Hunt for the last non-blank line to get indentation from.
	for(;;) {
		if((lnp = lback(lnp)) == curbp->b_hdrlnp || !is_white(lnp,lused(lnp)))
			break;
		}

	// Get the indentation, if any.
	if(getindent(&indentp,lnp) != SUCCESS)
		return rc.status;
	if(indentp != NULL) {

		// Insert it.
		if(insindent(indentp) != SUCCESS)
			return rc.status;

		// Delete one tab backward if block close.
		if(openClose < 0 && deleteTab(NULL,-1) != SUCCESS)
			return rc.status;
		}

	// Insert one more tab for a block begin.
	return openClose > 0 ? instab(1) : rc.status;
	}

// Insert a newline or space with auto-formatting.
int insnlspace(Value *rp,int n,bool nl) {

	// Negative argument not allowed.
	if(n < 0 && n != INT_MIN)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"

	if(n != 0) {
		// If we are in a language mode and this is a default NL ...
		if(nl && n == INT_MIN && (curbp->b_modes & MDGRP_LANG) && curwp->w_face.wf_dot.lnp != curbp->b_hdrlnp)
			return langnewline();

		// If wrap mode is enabled, wrap column is defined, and we are now past wrap column, execute user-assigned wrap
		// hook.
		if((curbp->b_modes & MDWRAP) && wrapcol > 0 && getccol() > wrapcol)
			if(exechook(NULL,INT_MIN,hooktab + HKWRAP,0) != SUCCESS)
				return rc.status;

		if(n == INT_MIN)
			n = 1;

		// If space char and replace or overwrite mode ...
		if(!nl && overprep(n) != SUCCESS)
			return rc.status;

		// Insert some lines or spaces.
		if(nl) {
			do {
				if(lnewline() != SUCCESS)
					break;
				} while(--n > 0);
			}
		else
			(void) linsert(n,' ');
		}

	return rc.status;
	}

// Insert a right fence or keyword into the text for current language mode, given right fence or last letter of keyword that
// was just entered (but not yet inserted into current line).  Possible keywords and languages are:
//	end, rescue					Ruby
//	else						C, Perl, Ruby, Shell
//	elsif						Perl, Ruby
//	elif, do, done, fi, esac			Shell
//	!else, !elsif, !endif, !endloop, !endmacro	MightEMacs
//	then						Ruby, Shell
int insrfence(int c) {
	int ch,count,kwlen,thendo;
	Line *lnp;
	Dot origdot;
	Value *indentp;
	Dot *dotp = &curwp->w_face.wf_dot;

	// If dot is at the beginning of the line and the character is not a right brace (which is the only possible "trigger"
	// character at column 0), bag it.
	if(dotp->off == 0) {
		if(c != '}')
			goto bagit;
		}
	else {
		char *strp1,*strp2;

		// Scan to see if we have all white space before right brace or keyword.  Count is number of characters thus.
		thendo = false;
		if(c == '}')
			count = dotp->off;
		else {
			// Keyword.  Set strp1 to beginning of line and strp2 to beginning of keyword.
			strp2 = (strp1 = ltext(dotp->lnp)) + dotp->off;
			while(is_lower(strp2[-1]))
				if(--strp2 == strp1)
					break;
			if((curbp->b_modes & MDMEMACS) && strp2 > strp1 && strp2[-1] == '!')
				--strp2;
			count = strp2 - strp1;
			}
		if(!is_white(dotp->lnp,count))
			goto bagit;

		// Now check if keyword is a "right fence" for current language mode.  kwlen is length of symbol or keyword - 1.
		if(c == '}')
			kwlen = 0;
		else {
			// Keyword length greater than maximum possible (!endmacro without the 'o')?
			if((kwlen = dotp->off - count) == 0 || kwlen > 8)
				goto bagit;

				// "end", "rescue"
			if(((curbp->b_modes & MDRUBY) && ((kwlen == 2 && c == 'd' && memcmp(strp2,"en",2) == 0) ||
			 (kwlen == 5 && c == 'e' && memcmp(strp2,"rescu",5) == 0))) ||
				// "else"
			 (!(curbp->b_modes & MDMEMACS) && kwlen == 3 && c == 'e' && memcmp(strp2,"els",3) == 0) ||
				// "elsif"
			 ((curbp->b_modes & (MDPERL | MDRUBY)) && kwlen == 4 && c == 'f' && memcmp(strp2,"elsi",4) == 0) ||
				// !else, !elsif, !endif, !endloop, !endmacro
			 ((curbp->b_modes & MDMEMACS) && ((kwlen == 4 && c == 'e' && memcmp(strp2,"!els",4) == 0) ||
			  (kwlen == 5 && c == 'f' && (memcmp(strp2,"!elsi",5) == 0 || memcmp(strp2,"!endi",5) == 0)) ||
			  (kwlen == 7 && c == 'p' && memcmp(strp2,"!endloo",7) == 0) ||
			  (kwlen == 8 && c == 'o' && memcmp(strp2,"!endmacr",8) == 0))) ||
			 	// "fi", "do", "done", "elif", "esac"
			 ((curbp->b_modes & MDSHELL) &&
			  ((kwlen == 1 && ((c == 'i' && *strp2 == 'f') || (thendo = (c == 'o' && *strp2 == 'd')))) ||
			   (kwlen == 3 && ((c == 'e' && memcmp(strp2,"don",3) == 0) || (c == 'f' && memcmp(strp2,"eli",3) == 0) ||
			   (c == 'c' && memcmp(strp2,"esa",3) == 0))))) ||
				// "then"
			 ((curbp->b_modes & (MDRUBY | MDSHELL)) &&
			  (kwlen == 3 && (thendo = (c == 'n' && memcmp(strp2,"the",3) == 0)))))
				;
			else
				goto bagit;
			}
		}

	// It's a go: white space (or no space) and matching keyword found.  Save current (original) position.
	origdot = *dotp;

	// If a right brace was entered, check if matching left brace at same nesting level exists (moving dot).
	if(c == '}') {
		count = 1;
		(void) backch(1);	// Begin search at character immediately preceding dot.

		while(count > 0) {
			ch = (dotp->off == lused(dotp->lnp)) ? '\r' : lgetc(dotp->lnp,dotp->off);
			if(ch == '}')
				++count;
			else if(ch == '{')
				--count;
			(void) backch(1);
			if(boundary(dotp,BACKWARD))
				break;
			}

		if(count != 0) {		// No match ... don't alter indentation.
			*dotp = origdot;
			goto bagit;
			}
		}
	else {
		// A letter was entered: hunt for the last non-blank line to get indentation from.
		lnp = dotp->lnp;
		for(;;) {
			if((lnp = lback(lnp)) == curbp->b_hdrlnp)
				goto bagit;
			if(!is_white(lnp,lused(lnp)))
				break;
			}
		dotp->lnp = lnp;
		}

	// Dot is now (somewhere) on line containing desired indentation.  Get it.
	if(getindent(&indentp,dotp->lnp) != SUCCESS)
		return rc.status;

	// Restore original position and set indentation of line, if applicable.
	*dotp = origdot;

	// Move to beginning of keyword and delete any white space before dot.
	if(kwlen > 0)
		(void) backch(kwlen);
	if(ldelete((long) -dotp->off,0) != SUCCESS)
		return rc.status;

	// If indentation was found, insert it.
	if(indentp != NULL && insindent(indentp) != SUCCESS)
		return rc.status;

	// Insert one extra tab if "extra indent" mode (which applies only to a right brace) ...
	if(c == '}') {
		if(curbp->b_modes & MDXINDT)
			(void) instab(1);
		}

	// delete one tab backward if indentation line did not contain "then" or "do" keyword by itself ...
	else if(!thendo && deleteTab(NULL,-1) != SUCCESS)
		return rc.status;

	// and return to starting position if needed.
	if(kwlen > 0)
		(void) forwch(kwlen);
bagit:
	// Indentation adjusted: now insert the "trigger" character.
	return linsert(1,c);
	}

// Insert "c" (# or =) into the text at point -- we are in C or RUBY mode.
int inspre(int c) {
	Dot *dotp = &curwp->w_face.wf_dot;

	// If we are at the beginning of the line, no go.
	if(dotp->off == 0)
		return linsert(1,c);

	// Check if all white space before this position.
	if(!is_white(dotp->lnp,dotp->off))
		return linsert(1,c);

	// Delete back first ...
	if(ldelete((long) -dotp->off,0) != SUCCESS)
		return rc.status;

	// and insert the required character.
	return linsert(1,c);
	}

// Delete blank lines around point.  If point is on a blank line, this command deletes all the blank lines above and below the
// current line.  If it is on a non-blank line, then it deletes all of the blank lines after the line.  Any argument is ignored.
int deleteBlankLines(Value *rp,int n) {
	Line *lnp1,*lnp2;
	long count;			// Characters to delete.
	Dot *dotp = &curwp->w_face.wf_dot;

	lnp1 = dotp->lnp;
	while(is_white(lnp1,lused(lnp1)) && (lnp2 = lback(lnp1)) != curbp->b_hdrlnp)
		lnp1 = lnp2;
	lnp2 = lnp1;
	count = 0;
	while((lnp2 = lforw(lnp2)) != curbp->b_hdrlnp && is_white(lnp2,lused(lnp2)))
		count += lused(lnp2) + 1;

	// Handle special case where first buffer line is blank.
	if(is_white(lnp1,lused(lnp1))) {
		dotp->lnp = lnp1;
		count += lused(lnp1) + 1;
		}
	else {
		if(count == 0)
			return rc.status;
		dotp->lnp = lforw(lnp1);
		}
	dotp->off = 0;
	return ldelete((long) count,0);
	}

// Insert a newline, then enough tabs and spaces to duplicate the indentation of the previous line.  Tabs are every htabsize
// characters.  Quite simple.  Figure out the indentation of the current line.  Insert a newline by calling the standard
// routine.  Insert the indentation by inserting the right number of tabs and spaces.  Return status.  Normally bound to ^J.
int newlineI(Value *rp,int n) {
	Value *indentp;
	Line *lnp = curwp->w_face.wf_dot.lnp;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Command repeat count"

	// Get the indentation of the current line, if any.
	if(getindent(&indentp,lnp) != SUCCESS)
		return rc.status;

	// Insert lines with indentation in the final line.
	do {
		if(lnewline() != SUCCESS || (indentp != NULL && n == 1 && linstr(indentp->v_strp) != SUCCESS))
			break;
		} while(--n > 0);

	return rc.status;
	}

// Delete hard tabs or "chunks" of spaces.  Return status.  Soft tabs are deleted such that the next non-space character (or end
// of line) past dot is moved left until it reaches a tab stop, one or more times.  To accomplish this, spaces are deleted
// beginning at dot going forward (if n > 0) and dot does not move, or spaces are deleted prior to dot going backward (if n < 0)
// and dot follows.
int deleteTab(Value *rp,int n) {
	int off,i;
	long direc;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Check for "do nothing" cases.
	if(n == INT_MIN)
		n = -1;
	if((i = lused(dotp->lnp)) == 0 || ((off = dotp->off) == 0 && n < 0) || (off == i && n > 0))
		return rc.status;

	// Set direction and loop increment.
	if(n > 0) {
		direc = 1L;
		i = 0;
		}
	else {
		direc = -1L;
		i = -1;
		}

	// Do hard tabs first ... simple.  Just delete up to n tab characters forward or backward.
	if(stabsize == 0) {
		n = abs(n);
		while((off = dotp->off + i) >= 0 && off < lused(dotp->lnp) && lgetc(dotp->lnp,off) == '\t')
			if(ldelete(direc,0) != SUCCESS || --n == 0)
				break;
		}
	else {
		// Process soft tab(s).  Delete spaces until the next non-space character to the right of dot (if deleting
		// forward) or at or to the right of dot (if deleting backward) is positioned at the next tab stop to its left,
		// assuming that position is at or to the right of dot.  If deleting forward, begin at dot and go rightward; if
		// deleting backward; begin at dot - 1 and go leftward.  If this is not possible (because the current run of
		// spaces is too short), do nothing.

		// ....+....1....+....2....+....3....+....4....+....5....+....6....+....7....+....8
		// ^   ^   ^   ^   ^   ^   ^   ^   ^   ^	Tab stops if tab size is 4.
		// ab      x   The end is near    Weird.	Sample text.

		// Proceed only if deleting forward or (deleting backward and) the character just prior to dot is a space.
		if(n > 0 || lgetc(dotp->lnp,dotp->off - 1) == ' ') {
			int dotcol,len;

			// Save column postion of dot and scan forward to next non-space character.
			dotcol = getccol();
			len = lused(dotp->lnp);
			for(off = dotp->off; dotp->off < len && lgetc(dotp->lnp,dotp->off) == ' '; ++dotp->off);

			// Continue only if deleting backward or run length > 0.
			if(n < 0 || (len = dotp->off - off) > 0) {
				int col1,col2,chunk1;

				// Get column position of non-space character, and calculate position of prior tab stop and
				// size of first "chunk".
				col1 = ((col2 = getccol()) - 1) / stabsize * stabsize;
				chunk1 = col2 - col1;

				// If deleting forward ...
				if(n > 0) {

					// Stop here if calculated position is before dot.
					if(col1 < dotcol)
						dotp->off = off;
					else
						goto nuke;
					}
				else {
					// Deleting backward.  Scan backward to next non-space character.
					for(dotp->off = off; dotp->off > 0 && lgetc(dotp->lnp,dotp->off - 1) == ' ';
					 --dotp->off);

					// Continue only if run is not too short.
					len = off - dotp->off;
					dotp->off = off;
					if(len >= chunk1) {
nuke:;
						// Determine number of spaces to delete and nuke 'em.
						int maxleft = (len - chunk1) / stabsize;
						int m = abs(n) - 1;
						(void) ldelete((long) -(chunk1 + (m > maxleft ? maxleft : m) * stabsize),0);
						if(n > 0)
							dotp->off = off;
						}
					}
				}
			}
		}

	return rc.status;
	}

// Kill, delete, or copy text if kdc is -1, 0, or 1, respectively.  Save text to kill ring if kdc is non-zero.  If regp is not
// NULL, operate on the region.  Otherwise, if called with an argument of 1 (the default), operate from dot to the end of the
// line, unless dot is at the end of the line, in which case operate on the newline.  If called with an argument of 0, operate
// from dot to the beginning of the line.  If called with a positive argument, operate from dot forward over that number of line
// breaks to the end of the last line.  If called with a negative argument, operate backward over that number of line breaks to
// the beginning of the first line.  Return status.
int kdctext(int n,int kdc,Region *regp) {
	Line *nextp;
	long chunk;
	Region region;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Process region elsewhere if specified.
	if(regp != NULL) {
		if(kdc > 0)
			goto kopy;
		kprep(kdc);
		*dotp = regp->r_dot;
		return ldelete(regp->r_size,kdc ? DFKILL : DFDEL);
		}

	// No region ... check if at end of buffer.
	if(dotp->lnp == curbp->b_hdrlnp && (n == INT_MIN || n > 0))
		return rcset(FAILURE,0,text259);
				// "No text selected"

	// Process lines and make a region.
	region.r_dot = *dotp;
	if(n == INT_MIN || n == 1) {
		chunk = lused(dotp->lnp) - dotp->off;
		if(chunk == 0)
			chunk = 1;
		}
	else if(n == 0) {
		region.r_dot.off = 0;
		chunk = -dotp->off;
		}
	else if(n > 1) {
		chunk = lused(dotp->lnp) - dotp->off;
		nextp = lforw(dotp->lnp);
		do {
			if(nextp == curbp->b_hdrlnp)
				break;
			chunk += 1 + lused(nextp);
			nextp = lforw(nextp);
			} while(--n > 1);
		}
	else {	// n < 0.
		region.r_dot.off = 0;
		chunk = -dotp->off;
		nextp = lback(dotp->lnp);
		do {
			if((region.r_dot.lnp = nextp) == curbp->b_hdrlnp)
				break;
			chunk -= lused(nextp) + 1;
			nextp = lback(nextp);
			} while(++n < 0);
		}

	// Kill, delete, or copy text.
#if NULREGERR
	if(chunk == 0)
		return rcset(FAILURE,0,text259);
				// "No text selected"
#endif
	if(kdc <= 0) {

		// Kill or delete.
		kprep(kdc);
		return ldelete(chunk,kdc ? DFKILL : DFDEL);
		}

	// Copy.
	if(chunk < 0)
		kentry.lastflag &= ~CFKILL;		// New kill if copying backward.
	region.r_size = labs(chunk);
	if(region.r_dot.lnp == curbp->b_hdrlnp)
		region.r_dot.lnp = lforw(curbp->b_hdrlnp);
	regp = &region;
kopy:
	if(copyreg(regp) != SUCCESS)
		return rc.status;
	return rcset(SUCCESS,0,"%s %s",text261,text262);
				// "Text","copied"
	}

// Kill, delete, or copy line(s) via kdctext() if kdc is -1, 0, or 1, respectively.
int kdcline(int n,int kdc) {
	int used1;
	bool oneline;
	Dot odot;
	Region region;
	WindFace *wfp = &curwp->w_face;
	Dot *dotp = &wfp->wf_dot;

	if(n == INT_MIN)
		n = 1;
	else if(n == 0) {				// If zero argument ...
		if(getregion(&region,NULL) != SUCCESS)	// select all lines in region.
			return rc.status;

		// If region begins at point, expand to include text from dot to beginning of line and text at end of last
		// (mark 0) line plus line terminator; otherwise, expand to include text at beginning of first (mark 0) line
		// and text from dot to beginning of next line.
		region.r_size += ((region.r_dot.lnp == dotp->lnp && region.r_dot.off == dotp->off) ?
		 dotp->off + lused(wfp->wf_mark[0].mk_dot.lnp) - wfp->wf_mark[0].mk_dot.off :
		 wfp->wf_mark[0].mk_dot.off + lused(dotp->lnp) - dotp->off) + 1;
		region.r_dot.off = 0;
		}
	odot = *dotp;
	used1 = lused(dotp->lnp);
	oneline = false;

	// Check if at end of buffer.
	if(dotp->lnp == curbp->b_hdrlnp) {
		if(n > 0)
			return rcset(FAILURE,0,text259);
					// "No text selected"
		oneline = (n == -1);
		}
	else if(n < 0) {				// Going backward?
		dotp->lnp = lforw(dotp->lnp);		// Yes, move to beginning of next line ...
		dotp->off = 0;
		--n;					// and bump the line count.
		}
	else {
		dotp->off = 0;				// No, move to beginning of line.
		oneline = (n == 1);
		}

	// Nuke or copy line(s).
	if(kdctext(n,kdc,n == 0 ? &region : NULL) != SUCCESS)
		return rc.status;

	// Nuke or copy one more line break if n > 1 or n == 1 and first line wasn't empty.
	if(kdc <= 0)
		return (n <= 0 || (n == 1 && used1 == 0)) ? rc.status : ldelete(1L,kdc ? DFKILL : DFDEL);
	if((n > 1 || (n == 1 && used1 > 0)) && kinsert(kringp,FORWARD,'\r') != SUCCESS)
		return rc.status;
	*dotp = odot;
	return rcset(SUCCESS,RCFORCE,"%s%s %s",text260,oneline ? "" : "s",text262);
				// "Line","copied"
	}

// Delete white space surrounding point on current line.
int delwhite(void) {
	int c,offset;
	Dot *dotp = &curwp->w_face.wf_dot;

	if(lused(dotp->lnp) > 0 && ((c = lgetc(dotp->lnp,dotp->off)) == ' ' || c == '\t')) {

		// Delete backward.
		for(;;) {
			if((offset = dotp->off) == 0)
				break;
			c = lgetc(dotp->lnp,offset - 1);
			if(c != ' ' && c != '\t')
				break;
			if(ldelete(-1L,0) != SUCCESS)
				return rc.status;
			}

		// Delete forward.
		for(;;) {
			if((offset = dotp->off) == lused(dotp->lnp))
				break;
			c = lgetc(dotp->lnp,offset);
			if(c != ' ' && c != '\t')
				break;
			if(ldelete(1L,0) != SUCCESS)
				return rc.status;
			}
		}
	return rc.status;
	}

// Join adjacent line(s), replacing all white space in between with (1), nothing if "delimp" is nil; or (2), a single space
// character (unless either line is blank or all white space) and insert extra space character if first of two adjacent lines
// ends with any character specified in "delimp".  Return status.
static int joinln(Value *rp,int n,Value *delimp) {
	int m,incr,newdot;
	bool insSpace = (delimp == NULL || !vistfn(delimp,VNIL));
	Dot *dotp = &curwp->w_face.wf_dot;

	// Determine bounds of line block.
	if(n == INT_MIN)
		n = -1;						// Join with one line above by default.
	else {
		if(n == 0 && reglines(&n,NULL) != SUCCESS)	// Join all lines in region.
			return rc.status;
		if(n == 1)					// 1 is invalid.
			return rcset(FAILURE,0,text35);
				// "Line count cannot be 1"
		}

	// Get ready.
	if(n > 0) {						// n is >= 2.
		incr = 1;
		newdot = lused(dotp->lnp);			// Save current end-of-line position.
		--n;
		}
	else
		incr = -1;

	// Join lines forward or backward.
	do {
		// Move to end or beginning of line, delete newline, delete white space, and if delimp not nil and not at
		// beginning or end of new line, insert one or two spaces.
		if(incr == 1) {
			if(lforw(dotp->lnp) == curbp->b_hdrlnp)
				break;
			dotp->off = lused(dotp->lnp);
			}
		else {
			if(lback(dotp->lnp) == curbp->b_hdrlnp)
				break;
			dotp->off = 0;
			}
		if(ldelete((long) incr,0) != SUCCESS || delwhite() != SUCCESS)
			return rc.status;
		if(insSpace && dotp->off > 0 && dotp->off < lused(dotp->lnp)) {
			m = 1;
			if(delimp != NULL && index(delimp->v_strp,lgetc(dotp->lnp,dotp->off - 1)) != NULL)
				++m;
			if(linsert(m,' ') != SUCCESS)
				return rc.status;
			}
		} while((n -= incr) != 0);
	if(incr > 0)
		dotp->off = newdot;				// Set current position.

	return rc.status;
	}

// Join adjacent line(s) via joinln(), passing argument value if script mode.
int joinLines(Value *rp,int n) {
	Value *delimp = NULL;

	// Get sentence-end characters if script mode.
	if(opflags & OPSCRIPT) {
		if(vnew(&delimp,false) != 0)
			return vrcset();
		if(macarg(delimp,ARG_FIRST) != SUCCESS)
			return rc.status;
		if(visnull(delimp))
			delimp = NULL;
		}

	// Call joinln().
	return joinln(rp,n,delimp);
	}


// Change a mode, given result pointer, action (n < 0: clear, n == 0 (default): toggle, n > 0: set), type (0: global, 1: show,
// 2: default, 3: buffer), and optional mode flags.  If rp is not NULL, set it to the state (-1 or 1) of the last mode altered.
// Return status.
int adjustmode(Value *rp,int n,int type,Value *valp) {
	int action;			// < 0 (clear), 0 (toggle), or > 0 (set).
	int i;				// Loop index.
	long formerState;		// -1 or 1.
	ModeSpec *msp;
	uint mask;
	ModeRec *mrp = (type == 3) ? NULL : modetab + type;
	uint *mp = mrp ? &mrp->flags : &curbp->b_modes;	// Pointer to mode-flags word to change.
	uint mflags = *mp;		// Old mode flag word.
	uint aflags = ARG_FIRST | ARG_STR;
	long oldflags[4],*ofp = oldflags;

	// Save current modes so they can be passed to mode hook, if any.
	mrp = modetab;
	do {
		*ofp++ = (mrp++)->flags;
		} while(mrp->cmdlabel != NULL);
	*ofp = curbp->b_modes;

	// If called from putvar(), decode the new flag word, validate it, and jump ahead.
	if(valp != NULL) {
		ushort u1,u2;
		int count;

		// Any unknown bits?
		if((uint) valp->u.v_int & (type <= 1 ? ~MDGLOBAL : ~MDBUFFER))
			goto badbit;

		// If nothing has changed, nothing to do.
		if((u1 = valp->u.v_int) == mflags)
			return rc.status;

		if(type > 1) {
			// MDOVER and MDREPL both set?
			if((u1 & MDGRP_OVER) == MDGRP_OVER)
				goto badbit;

			// More than one language mode bit set?
			for(count = 0,u2 = u1 & MDGRP_LANG; u2 != 0; u2 >>= 1)
				if((u2 & 1) && ++count > 1)
badbit:
					return rcset(FAILURE,0,text298,(uint) valp->u.v_int);
						// "Unknown or conflicting bit(s) in mode word '0x%.8x'"
			}

		// Update flag word and do special processing for specific global modes that changed.
		*mp = u1;
		if((u1 & MDESC8) != (mflags & MDESC8))
			uphard();
		if((u1 & MDEXACT) != (mflags & MDEXACT))
			srch.fdelta1[0] = -1;
		if((u1 & MDHSCRL) != (mflags & MDHSCRL))
			lbound = 0;
		}
	else {
		Value *vp;
		action = (n == INT_MIN) ? 0 : n;

		if(vnew(&vp,false) != 0)
			return vrcset();

		// If interactive mode, build the proper prompt string; e.g. "Toggle global mode (Asave eXact ... wkDir): "
		if((opflags & OPSCRIPT) == 0) {
			StrList prompt;		// Prompt string, built from bmodeinfo or gmodeinfo array.

			// Build prompt.
			if(vopen(&prompt,NULL,false) != 0 ||
			 vputs(action < 0 ? text65 : action > 0 ? text64 : text231,&prompt) != 0)
						// "Clear","Set","Toggle"
				return vrcset();
			if(type < 3) {
				if(vputc(' ',&prompt) != 0 ||
				 vputs(type == 0 ? text31 : type == 1 ? text296 : text62,&prompt) != 0)
						// "global","show","default"
					return vrcset();
				}
			if(type > 1) {
				if(vputc(' ',&prompt) != 0 || vputs(text83,&prompt) != 0)
									// "buffer"
					return vrcset();
				}
			if(vputs(text63,&prompt) != 0)
					// " mode"
				return vrcset();

			msp = (type <= 1) ? gmodeinfo : bmodeinfo;
			if(vputc(' ',&prompt) != 0)
				return vrcset();
			i = '(';
			do {
				if(vputc(i,&prompt) != 0 || vputs(msp->mlname,&prompt) != 0)
					return vrcset();
				i = ' ';
				} while((++msp)->name != NULL);
			if(vputc(')',&prompt) != 0 || vclose(&prompt) != 0)
				return vrcset();

			// Prompt the user and get an answer.
			if(termarg(vp,prompt.sl_vp->v_strp,NULL,CTRL | 'M',ARG_ONEKEY) != SUCCESS || vistfn(vp,VNIL))
				return rc.status;
			goto domode;
			}

		// Script mode: get one or more arguments.
		do {
			if(aflags & ARG_FIRST) {
				if(!havesym(s_any,true))
					return rc.status;	// Error.
				}
			else if(!havesym(s_comma,false))
				break;				// At least one argument retrieved and none left.
			if(macarg(vp,aflags) != SUCCESS)
				return rc.status;
			aflags = ARG_STR;
			if(visnull(vp) || vistfn(vp,VNIL))
				return rcset(FAILURE,0,text187,text285);
					// "%s cannot be null","Command argument"
domode:
			// Make keyword lowercase.
			mklower(vp->v_strp,vp->v_strp);

			// Test it against the modes we know.
			i = strlen(vp->v_strp);
			msp = (type <= 1) ? gmodeinfo : bmodeinfo;
			do {
				if(i == 1) {
					if(upcase[(int) *vp->v_strp] == msp->code)
						goto found;
					}
				else if(strcmp(vp->v_strp,msp->name) == 0)
					goto found;
				} while((++msp)->name != NULL);

			// No match ... return error message.
			return rcset(FAILURE,0,text66,vp->v_strp);
				// "No such mode '%s'"
found:
			// Match found ... process it.
			mask = msp->mask;
			formerState = (*mp & mask) ? 1L : -1L;
			if(action < 0)
				*mp &= ~mask;
			else if(action > 0)
				*mp |= mask;
			else
				*mp ^= mask;

			// Ensure mutually-exclusive bits are not set.
			if(type > 1) {
				if((mask & MDGRP_OVER) && (*mp & MDGRP_OVER))
					*mp = (*mp & ~MDGRP_OVER) | mask;
				else if((mask & MDGRP_LANG) && (*mp & MDGRP_LANG))
					*mp = (*mp & ~MDGRP_LANG) | mask;
				}

			// Do special processing for specific global modes that changed.
			if(type == 0 && (*mp & mask) != (mflags & mask)) {
				switch(mask) {
					case MDESC8:
						uphard();
						break;
					case MDEXACT:
						srch.fdelta1[0] = -1;
						break;
					case MDHSCRL:
						lbound = 0;
					}
				}

			// Onward ...
			} while(opflags & OPSCRIPT);
		}

	// Display new mode line.
	if(type != 2)
		upmode(type == 3 ? curbp : NULL);
	if((opflags & OPSCRIPT) == 0)
		mlerase(0);		// Erase the prompt.

	// Return former state of last mode that was changed.
	if(rp != NULL)
		vsetint(formerState,rp);

	// Run mode-change hook if any flag was changed.
	return *mp != mflags && !(curbp->b_flags & (BFHIDDEN | BFMACRO)) ?
	 exechook(NULL,INT_MIN,hooktab + HKMODE,0xf4,oldflags[0],oldflags[1],oldflags[2],oldflags[3]) : rc.status;
	}

// Kill, delete, or copy fenced region if kdc is -1, 0, or 1, respectively.  Return status.
bool kdcfencedreg(int kdc) {
	Region region;
	int result;
	long size;

	if((result = otherfence(&region)) == 0)
		return rcset(FAILURE,0,NULL);

	// Got region.  If kill or delete, let kdctext() do the rest.
	if(kdc <= 0)
		return kdctext(INT_MIN,kdc,&region);

	// Copy and restore dot.  Save size, call kdctext() ...
	size = region.r_size - 1;
	if(result < 0)
		kentry.lastflag &= ~CFKILL;		// New kill if copying backward.
	if(kdctext(INT_MIN,1,&region) != SUCCESS)
		return rc.status;

	// and move dot to original position.
	return result < 0 ? forwch(size) : backch(size);
	}

// Write text to a named buffer (for macro use only).
int writeBuf(Value *rp,int n) {
	Buffer *bufp;

	// Negative repeat count is an error.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"

	// Get the buffer name.
	if(macarg(rp,ARG_FIRST | ARG_STR | ARG_NOTNULL) != SUCCESS)
		return rc.status;
	if(!bfind(rp->v_strp,0,0,&bufp,NULL))
		return rcset(FAILURE,0,text118,rp->v_strp);
			// "No such buffer '%s'"
	if(!getcomma(true))
		return rc.status;

	return chgtext(rp,n,bufp,txt_insert);
	}

// Word wrap on white space.  Beginning at point going backward, stop on the first word break (space character) or column n
// (which must be >= 0).  If we reach column n, jump back to the point position and (i), if default n: start a new line; or
// (ii), if not default n: scan forward to a word break (and eat all white space) or end of line and start a new line. 
// Otherwise, break the line at the word break, eat all white space, and jump forward to the point position.  Make sure we force
// the display back to the left edge of the current window.
int wrapWord(Value *rp,int n) {
	int wordsz;		// Size of word wrapped to next line.
	int lmargin,origoff;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Determine left margin.
	if(n == INT_MIN)
		lmargin = 0;
	else if((lmargin = n) < 0)
		return rcset(FAILURE,0,text39,text322,n,0);
			// "%s (%d) must be %d or greater","Column number"

	// If blank line, do nothing.
	if(lused(dotp->lnp) == 0)
		return rc.status;

	// Scan backward to first space character.
	origoff = dotp->off;
	wordsz = -1;
	while(lgetc(dotp->lnp,dotp->off) != ' ') {

		// If we are at or past the left margin, start a new line.
		if(getccol() <= lmargin) {

			// Hunt forward for a break if non-default n.
			if(n == INT_MIN)
				dotp->off = origoff;
			else
				while(dotp->off < lused(dotp->lnp)) {
					(void) forwch(1);
					if(lgetc(dotp->lnp,dotp->off) == ' ') {
						if(delwhite() != SUCCESS)
							return rc.status;
						break;
						}
					}
			return lnewline();
			}

		// Back up one character.
		(void) backch(1);
		++wordsz;
		}

	// Found a space.  Replace it with a newline.
	if(delwhite() != SUCCESS || lnewline() != SUCCESS)
		return rc.status;

	// Move back to where we started.
	if(wordsz > 0)
		(void) forwch(wordsz);

	// Make sure the display is not horizontally scrolled.
	if(curwp->w_face.wf_fcol != 0) {
		curwp->w_face.wf_fcol = 0;
		curwp->w_flags |= WFHARD | WFMOVE | WFMODE;
		}

	return rc.status;
	}

// Move dot forward by the specified number of words.  As you move, convert any characters to upper case.  No error if attempt
// to move past the end of the buffer.
int ucWord(Value *rp,int n) {
	int c;
	Dot *dotp = &curwp->w_face.wf_dot;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"
	do {
		while(!inword()) {
			if(forwch(1) != SUCCESS)
				return rc.status;
			}
		while(inword()) {
			c = lgetc(dotp->lnp,dotp->off);
			if(is_lower(c)) {
				lputc(dotp->lnp,dotp->off,upcase[c]);
				lchange(curbp,WFHARD);
				}
			if(forwch(1) != SUCCESS)
				return rc.status;
			}
		} while(--n > 0);

	return rc.status;
	}

// Move dot forward by the specified number of words.  As you move convert characters to lower case.  No error if attempt to
// move past the end of the buffer.
int lcWord(Value *rp,int n) {
	int c;
	Dot *dotp = &curwp->w_face.wf_dot;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"
	do {
		while(!inword()) {
			if(forwch(1) != SUCCESS)
				return rc.status;
			}
		while(inword()) {
			c = lgetc(dotp->lnp,dotp->off);
			if(is_upper(c)) {
				lputc(dotp->lnp,dotp->off,lowcase[c]);
				lchange(curbp,WFHARD);
				}
			if(forwch(1) != SUCCESS)
				return rc.status;
			}
		} while(--n > 0);

	return rc.status;
	}

// Move dot forward by the specified number of words.  As you move, convert the first character of the word to upper case and
// subsequent characters to lower case.  No error if attempt to move past the end of the buffer.
int tcWord(Value *rp,int n) {
	int c,firstc;
	Dot *dotp = &curwp->w_face.wf_dot;

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"
	do {
		while(!inword()) {
			if(forwch(1) != SUCCESS)
				return rc.status;
			}
		firstc = true;
		while(inword()) {
			c = lgetc(dotp->lnp,dotp->off);
			if(firstc == is_lower(c)) {
				c = firstc ? upcase[c] : lowcase[c];
				lputc(dotp->lnp,dotp->off,c);
				lchange(curbp,WFHARD);
				}
			firstc = false;
			if(forwch(1) != SUCCESS)
				return rc.status;
			}
		} while(--n > 0);

	return rc.status;
	}

// Wrap line(s) in a block specified by n argument.  Duplicate indentation from first line in all subsequent lines.  If script
// mode, also add value of first argument after indentation (for example, "// " or "# "), and pass second argument to joinln().
// No error if attempt to move past a buffer boundary.
int wrapLine(Value *rp,int n) {
	Dot *dotp;
	Value *indentp = NULL;		// Leading white space on first line, if any.
	Value *prefixp = NULL;		// Value of first argument if script mode.
	Value *delimp = NULL;		// Value of second argument if script mode.
	int indentcol,lmargin,col;
	int prefixLen = 0;

	// Wrap column set?
	if(wrapcol == 0)
		return rcset(FAILURE,0,text98);
			// "Wrap column not set"

	// Get prefix and end-sentence delimiters if script mode.
	if(opflags & OPSCRIPT) {
		if(vnew(&prefixp,false) != 0 || vnew(&delimp,false) != 0)
			return vrcset();
		if(macarg(prefixp,ARG_FIRST) != SUCCESS || macarg(delimp,0) != SUCCESS)
			return rc.status;
		if(visnull(prefixp) || vistfn(prefixp,VNIL))
			prefixp = NULL;
		else if(*prefixp->v_strp == ' ' || *prefixp->v_strp == '\t')
			return rcset(FAILURE,0,text303,prefixp->v_strp);
				// "Invalid wrap prefix \"%s\""
		else
			prefixLen = strlen(prefixp->v_strp);
		if(visnull(delimp) || vistfn(delimp,VNIL))
			delimp = NULL;
		}

	// Determine bounds of line block.
	dotp = &curwp->w_face.wf_dot;
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != SUCCESS)			// Process all lines in region.
		return rc.status;
	else if(n < 0) {						// Back up to first line.
		int count = 1;
		while(lback(dotp->lnp) != curbp->b_hdrlnp) {
			dotp->lnp = lback(dotp->lnp);
			++count;
			if(++n == 0)
				break;
			}
		if((n = count) > 1)
			curwp->w_flags |= WFMOVE;
		}
	dotp->off = 0;							// Move to beginning of line.

	// Dot now at beginning of first line and n > 0.
	(void) begintxt();						// Move to beginning of text.
	if((indentcol = getccol()) + prefixLen >= wrapcol)		// Too much indentation?
		return rcset(FAILURE,0,text323,wrapcol);
			// "Indentation exceeds wrap column (%d)"
	if(dotp->off > 0) {						// Save any indentation (from first line of block) ...
		if(vnew(&indentp,false) != 0 || vsetfstr(ltext(dotp->lnp),dotp->off,indentp) != 0)
			return vrcset();
		if(ldelete(-dotp->off,0) != SUCCESS)			// and delete it.
			return rc.status;
		}

	// Remove any existing prefix string from each line of block.
	if(prefixLen > 0) {
		Dot odot = *dotp;
		int count = n;
		char *strp = prefixp->v_strp - 1;
		int striplen;

		// Get length of stripped prefix.
		for(striplen = prefixLen; striplen > 1 && (strp[striplen] == ' ' || strp[striplen] == '\t'); --striplen);
		do {
			(void) begintxt();
			if(lused(dotp->lnp) - dotp->off >= striplen && memcmp(ltext(dotp->lnp) + dotp->off,prefixp->v_strp,
			 striplen) == 0 && (ldelete(striplen,0) != SUCCESS || (count == n && delwhite() != SUCCESS)))
				return rc.status;
			} while((dotp->lnp = lforw(dotp->lnp)) != curbp->b_hdrlnp && --count > 0);
		*dotp = odot;
		}

	if(n > 1 && joinln(rp,n,delimp) != SUCCESS)			// Convert line block to single line.
		return rc.status;
	if(lused(dotp->lnp) > 0) {
		dotp->off = 0;
		lmargin = indentcol + prefixLen;

		// Wrap n lines.
		for(;;) {
			// Insert indentation and prefix string.
			if((indentp != NULL && linstr(indentp->v_strp) != SUCCESS) ||
			 (prefixp != NULL && linstr(prefixp->v_strp) != SUCCESS))
				return rc.status;
			col = lmargin;					// Move forward until hit end of line or wrap column.
			for(;;) {
				if(++dotp->off == lused(dotp->lnp))	// At end of line?
					goto retn;			// Yes, done.
				col = newcol(lgetc(dotp->lnp,dotp->off),col);
				if(col >= wrapcol) {			// At or past wrap column?
					if(wrapWord(rp,lmargin) != SUCCESS)	// Yes, wrap.
						return rc.status;
					break;
					}
				}
			if(lused(dotp->lnp) == 0)			// Blank line after wrap?
				return ldelete(1L,0);			// Yes, done.
			dotp->off = 0;					// No, move to beginning of line and continue.
			}
		}
retn:
	(void) forwch(1);					// Can't fail.
	return rc.status;
	}

// Lower or upper case line(s).  If argument is zero, use the current region.  No error if attempt to move past the end of the
// buffer.
int caseline(int n,char *trantab) {
	int (*nextln)(int n);
	int offset,inc;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Compute block size.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != SUCCESS)
		return rc.status;
	if(n < 0) {
		--n;
		inc = -1;
		nextln = backln;
		}
	else {
		inc = 1;
		nextln = forwln;
		}

	// Loop thru text, changing case of n lines.
	kentry.lastflag &= ~CFVMOV;
	while(n != 0) {

		// Process the current line from the beginning.
		offset = 0;

		while(offset < lused(dotp->lnp)) {
			lputc(dotp->lnp,offset,trantab[(int) lgetc(dotp->lnp,offset)]);
			++offset;
			}

		// Move to the next line.
		dotp->off = 0;
		if(nextln(1) != SUCCESS)
			break;
		n -= inc;
		}

	// Finish up.
	if(nextln == backln)
		(void) forwln(1);
	kentry.thisflag &= ~CFVMOV;		// Flag that this resets the goal column.
	lchange(curbp,WFHARD);

	return rc.status;
	}

// Kill, delete, or copy forward by "n" words (which is never INT_MIN).  Save text to kill ring if kdc is non-zero.  If zero
// argument, kill or copy just one word and no trailing whitespace.
int kdcfword(int n,int kdc) {
	bool oneword;
	Region region;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Check if at end of buffer.
	if(dotp->lnp == curbp->b_hdrlnp)
		return rcset(FAILURE,0,text259);
				// "No text selected"

	// Save the current cursor position.
	region.r_dot = *dotp;

	// Figure out how many characters to copy or give the axe.
	region.r_size = 0;

	// Get into a word ...
	while(!inword()) {
		if(forwch(1) != SUCCESS)
			break;			// At end of buffer.
		++region.r_size;
		}

	oneword = false;
	if(n == 0) {
		// Skip one word, no whitespace.
		while(inword()) {
			if(forwch(1) != SUCCESS)
				break;		// At end of buffer.
			++region.r_size;
			}
		oneword = true;
		}
	else {
		int c;

		// Skip n words ...
		oneword = (n == 1);
		while(n-- > 0) {

			// If we are at EOL; skip to the beginning of the next.
			while(dotp->off == lused(dotp->lnp)) {
				if(forwch(1) != SUCCESS)
					goto nuke;		// At end of buffer.
				++region.r_size;
				}

			// Move forward until we are at the end of the word.
			while(inword()) {
				if(forwch(1) != SUCCESS)
					goto nuke;		// At end of buffer.
				++region.r_size;
				}

			// If there are more words, skip the interword stuff.
			if(n != 0)
				while(!inword()) {
					if(forwch(1) != SUCCESS)
						goto nuke;	// At end of buffer.
					++region.r_size;
					}
			}

		// Skip whitespace and newlines.
		while(dotp->off == lused(dotp->lnp) || (c = lgetc(dotp->lnp,dotp->off)) == ' ' ||
		 c == '\t') {
			if(forwch(1) != SUCCESS)
				break;
			++region.r_size;
			}
		}
nuke:
#if NULREGERR
	// Anything found?
	if(region.r_size == 0)
		return rcset(FAILURE,0,text259);
				// "No text selected"
#endif
	// Have region ... restore original position and kill, delete, or copy it.
	*dotp = region.r_dot;
	if(kdc <= 0) {

		// Kill or delete the word(s).
		kprep(kdc);
		return ldelete(region.r_size,kdc ? DFKILL : DFDEL);
		}

	// Copy the word(s).
	if(copyreg(&region) != SUCCESS)
		return rc.status;
	return rcset(SUCCESS,0,"%s%s %s",text115,oneword ? "" : "s",text262);
				// "Word","copied"
	}

// Kill, delete, or copy backward by "n" words (which is always > 0).  Save text to kill ring if kdc is non-zero.
int kdcbword(int n,int kdc) {
	bool oneword;
	long size;
	Region region;

	// Check if at beginning of buffer.
	if(backch(1) != SUCCESS)
		return rcset(FAILURE,0,text259);
				// "No text selected"

	// Figure out how many characters to copy or give the axe.
	size = 0;

	// Skip back n words ...
	oneword = (n == 1);
	do {
		while(!inword()) {
			++size;
			if(backch(1) != SUCCESS)
				goto copynuke;	// At beginning of buffer.
			}
		while(inword()) {
			++size;
			if(backch(1) != SUCCESS)
				goto copynuke;	// At beginning of buffer.
			}
		} while(--n > 0);

	if(forwch(1) != SUCCESS)
		return rc.status;
copynuke:
#if NULREGERR
	// Anything found?
	if(size == 0)
		return rcset(FAILURE,0,text259);
				// "No text selected"
#endif
	// Have region ... kill, delete, or copy it.
	if(kdc <= 0) {

		// Restore original position.
		(void) forwch(size);		// Can't fail.

		// Kill or delete the word(s) backward.
		kprep(kdc);
		return ldelete(-size,kdc ? DFKILL : DFDEL);
		}

	// Copy the word(s) from the current position.
	kentry.lastflag &= ~CFKILL;		// New kill.
	region.r_dot = curwp->w_face.wf_dot;
	region.r_size = size;
	if(copyreg(&region) != SUCCESS)
		return rc.status;

	// Restore original position.
	(void) forwch(size);			// Can't fail.

	return rcset(SUCCESS,0,"%s%s %s",text115,oneword ? "" : "s",text262);
				// "Word","copied"
	}
