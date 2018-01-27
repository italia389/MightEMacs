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

	if((lnp = (Line *) malloc(sizeof(Line) + used)) == NULL)
		return rcset(Panic,0,text94,"lalloc");
			// "%s(): Out of memory!"

	lnp->l_size = lnp->l_used = used;
	*lnpp = lnp;
	return rc.status;
	}

// Fix "window face" settings.  Return true if line found in face.
static bool fixfree(WindFace *wfp,Line *lnp) {
	bool found = false;

	if(wfp->wf_toplnp == lnp) {
		wfp->wf_toplnp = lnp->l_nextp;
		found = true;
		}
	if(wfp->wf_dot.lnp == lnp) {
		wfp->wf_dot.lnp = lnp->l_nextp;
		wfp->wf_dot.off = 0;
		found = true;
		}
	return found;
	}

// Delete line "lnp".  Fix all of the links that might point at it (they are moved to offset 0 of the next line.  Unlink
// the line from whatever buffer it might be in.  Release the memory and update the buffers.
void lfree(Line *lnp) {
	Buffer *bufp;
	EScreen *scrp;
	EWindow *winp;
	Mark *mkp;
	bool found;

	// In all screens...
	scrp = sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			(void) fixfree(&winp->w_face,lnp);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In all buffers...
	bufp = bheadp;
	do {
		found = fixfree(&bufp->b_face,lnp);
		mkp = &bufp->b_mroot;
		do {
			if(mkp->mk_dot.lnp == lnp) {
				mkp->mk_dot.lnp = lnp->l_nextp;
				mkp->mk_dot.off = 0;
				found = true;
				}
			} while((mkp = mkp->mk_nextp) != NULL);
		} while(!found && (bufp = bufp->b_nextp) != NULL);

	// Remove line from linked list and release its heap space.
	lnp->l_prevp->l_nextp = lnp->l_nextp;
	lnp->l_nextp->l_prevp = lnp->l_prevp;
	free((void *) lnp);
	}

// This routine gets called when a buffer is changed (edited) in any way.  It updates all of the required flags in the buffer
// and windowing system.  The minimal flag(s) are passed as an argument; if the buffer is being displayed in more than one
// window, we change WFEdit to WFHard.  Also WFMode is set if this is the first buffer change (the "*" has to be displayed) and
// any macro preprocessing storage is freed.
void bchange(Buffer *bufp,ushort flags) {

	if(bufp->b_nwind > 1)			// Hard update needed?
		flags = WFHard;			// Yes.
	if(!(bufp->b_flags & BFChanged)) {	// If first change...
		flags |= WFMode;		// need to update mode line.
		bufp->b_flags |= BFChanged;
		}
	if(bufp->b_mip != NULL)			// If precompiled macro...
		ppfree(bufp);			// force macro preprocessor redo.
	supd_wflags(bufp,flags);		// Flag any windows displaying this buffer.
	}

// Fix "window face" settings.  Return true if line found in face.
static bool fixins(int offset,int n,WindFace *wfp,Line *lnp1,Line *lnp2) {
	bool found = false;

	if(wfp->wf_toplnp == lnp1) {
		wfp->wf_toplnp = lnp2;
		found = true;
		}
	if(wfp->wf_dot.lnp == lnp1) {
		wfp->wf_dot.lnp = lnp2;
		if(wfp->wf_dot.off >= offset)
			wfp->wf_dot.off += n;
		found = true;
		}
	return found;
	}

// Insert "n" copies of the character "c" at the current point.  In the easy case all that happens is the text is stored in the
// line.  In the hard case, the line has to be reallocated.  When the window list is updated, need to (1), always update dot in
// the current window; and (2), update mark and dot in other windows if it is greater than the place where the insert was done.
// Return status.
int linsert(int n,short c) {
	char *str1,*str2;
	Line *lnp1,*lnp2,*lnp3;
	int offset;
	int i;
	EWindow *winp;
	EScreen *scrp;		// Screen to fix pointers in.
	Buffer *bufp;
	Mark *mkp;
	bool found;

	if(allowedit(true) != Success)				// Don't allow if in read-only mode.
		return rc.status;

	// A zero repeat count means do nothing!
	if(n == 0)
		return rc.status;

	// Negative repeat count is an error.
	if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"

	// Get current line and determine the type of insert.
	lnp1 = curwp->w_face.wf_dot.lnp;
	offset = curwp->w_face.wf_dot.off;

	if(lnp1 == curbp->b_hdrlnp) {				// Header line.

		// Allocate new line.
		if(lalloc(BSIZE(n),&lnp2) != Success)		// New line.
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
			lnp1->l_prevp->l_nextp = lnp2;		// Link in the new line.
			lnp2->l_nextp = lnp1->l_nextp;
			lnp1->l_nextp->l_prevp = lnp2;
			lnp2->l_prevp = lnp1->l_prevp;
			free((void *) lnp1);			// Get rid of the old one.
			}
		else {						// Easy: update in place.
			lnp2 = lnp1;				// Make gap in line for new character(s).
			lnp2->l_used += n;
			str2 = lnp1->l_text + lnp1->l_used;
			str1 = str2 - n;
			while(str1 != lnp1->l_text + offset)
				*--str2 = *--str1;
			}

		for(i = 0; i < n; ++i)				// Store the new character(s) in the "gap".
			lnp2->l_text[offset + i] = c;
		}

	// Set the "line change" flag in the current window.
	bchange(curbp,WFEdit);

	//  In all screens...
	scrp = sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			(void) fixins(offset,n,&winp->w_face,lnp1,lnp2);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In all buffers...
	bufp = bheadp;
	found = false;
	do {
		found = fixins(offset,n,&bufp->b_face,lnp1,lnp2);
		mkp = &bufp->b_mroot;
		do {
			if(mkp->mk_dot.lnp == lnp1) {
				mkp->mk_dot.lnp = lnp2;
				if(mkp->mk_dot.off > offset)
					mkp->mk_dot.off += n;
				found = true;
				}
			} while((mkp = mkp->mk_nextp) != NULL);
		} while(!found && (bufp = bufp->b_nextp) != NULL);

	return rc.status;
	}

// Fix "window face" settings.  Return true if line found in face.
static bool fixinsnl(int offset,WindFace *wfp,Line *lnp1,Line *lnp2) {
	bool found = false;

	if(wfp->wf_toplnp == lnp1) {
		wfp->wf_toplnp = lnp2;
		found = true;
		}
	if(wfp->wf_dot.lnp == lnp1) {
		if(wfp->wf_dot.off < offset)
			wfp->wf_dot.lnp = lnp2;
		else
			wfp->wf_dot.off -= offset;
		found = true;
		}
	return found;
	}

// Insert a newline at the current point.  Return status.  The funny ass-backward way it does things is not a botch; it just
// makes the last line in the buffer not a special case.  The update of dot and mark is a bit easier than in the above case,
// because the split forces more updating.
int lnewline(void) {
	char *str1,*str2;
	Line *lnp1,*lnp2;
	int offset;
	EScreen *scrp;
	EWindow *winp;
	Buffer *bufp;
	Mark *mkp;
	bool found;

	if(allowedit(true) != Success)		// Don't allow if in read-only mode.
		return rc.status;

	bchange(curbp,WFHard);
	lnp1 = curwp->w_face.wf_dot.lnp;	// Get the address and...
	offset = curwp->w_face.wf_dot.off;	// offset of dot.
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
	lnp2->l_prevp = lnp1->l_prevp;
	lnp1->l_prevp = lnp2;
	lnp2->l_prevp->l_nextp = lnp2;
	lnp2->l_nextp = lnp1;

	// In all screens...
	scrp = sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			(void) fixinsnl(offset,&winp->w_face,lnp1,lnp2);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In all buffers...
	bufp = bheadp;
	do {
		found = fixinsnl(offset,&bufp->b_face,lnp1,lnp2);
		mkp = &bufp->b_mroot;
		do {
			if(mkp->mk_dot.lnp == lnp1) {
				if(mkp->mk_dot.off < offset)
					mkp->mk_dot.lnp = lnp2;
				else
					mkp->mk_dot.off -= offset;
				found = true;
				}
			} while((mkp = mkp->mk_nextp) != NULL);
		} while(!found && (bufp = bufp->b_nextp) != NULL);

	return rc.status;
	}

// Insert a string at the current point.  "s" may be NULL.
int linstr(char *str) {

	if(str == NULL)
		return rc.status;
	while(*str != '\0') {
		if(((*str == '\n') ? lnewline() : linsert(1,*str)) != Success)
			return rc.status;
		++str;
		}
	return rc.status;
	}

// Fix "window face" settings.  Return true if line found in face.
static bool fixdelnl1(WindFace *wfp,Line *lnp1,Line *lnp2) {
	bool found = false;

	if(wfp->wf_toplnp == lnp2) {
		wfp->wf_toplnp = lnp1;
		found = true;
		}
	if(wfp->wf_dot.lnp == lnp2) {
		wfp->wf_dot.lnp = lnp1;
		wfp->wf_dot.off += lnp1->l_used;
		found = true;
		}
	return found;
	}

// Fix "window face" settings.  Return true if line found in face.
static bool fixdelnl2(WindFace *wfp,Line *lnp1,Line *lnp2,Line *lnp3) {
	bool found = false;

	if(wfp->wf_toplnp == lnp1 || wfp->wf_toplnp == lnp2) {
		wfp->wf_toplnp = lnp3;
		found = true;
		}
	if(wfp->wf_dot.lnp == lnp1) {
		wfp->wf_dot.lnp = lnp3;
		found = true;
		}
	else if(wfp->wf_dot.lnp == lnp2) {
		wfp->wf_dot.lnp = lnp3;
		wfp->wf_dot.off += lnp1->l_used;
		found = true;
		}
	return found;
	}

// Delete a newline.  Join the current line with the next line.  If the next line is the magic header line, always return
// Success; merging the last line with the header line can be thought of as always being a successful operation, even if nothing
// is done, and this makes the kill buffer work "right".  Easy cases can be done by shuffling data around.  Hard cases require
// that lines be moved about in memory.  Called by ldelete() only.
static int ldelnewline(void) {
	char *str1,*str2;
	Line *lnp1,*lnp2,*lnp3;
	EScreen *scrp;
	EWindow *winp;
	Buffer *bufp;
	Mark *mkp;
	bool found;

	lnp1 = curwp->w_face.wf_dot.lnp;
	lnp2 = lnp1->l_nextp;
	if(lnp2 == curbp->b_hdrlnp) {		// At the buffer end.
		if(lnp1->l_used == 0)		// Blank line.
			lfree(lnp1);
		return rc.status;
		}

	// Do simple join if room in current line for next line.
	if(lnp2->l_used <= lnp1->l_size - lnp1->l_used) {
		str1 = lnp1->l_text + lnp1->l_used;
		str2 = lnp2->l_text;
		while(str2 != lnp2->l_text + lnp2->l_used)
			*str1++ = *str2++;

		// In all screens...
		scrp = sheadp;
		do {
			// In all windows...
			winp = scrp->s_wheadp;
			do {
				(void) fixdelnl1(&winp->w_face,lnp1,lnp2);
				} while((winp = winp->w_nextp) != NULL);
			} while((scrp = scrp->s_nextp) != NULL);

		// In all buffers...
		bufp = bheadp;
		do {
			found = fixdelnl1(&bufp->b_face,lnp1,lnp2);
			mkp = &bufp->b_mroot;
			do {
				if(mkp->mk_dot.lnp == lnp2) {
					mkp->mk_dot.lnp = lnp1;
					mkp->mk_dot.off += lnp1->l_used;
					found = true;
					}
				} while((mkp = mkp->mk_nextp) != NULL);
			} while(!found && (bufp = bufp->b_nextp) != NULL);

		lnp1->l_used += lnp2->l_used;
		lnp1->l_nextp = lnp2->l_nextp;
		lnp2->l_nextp->l_prevp = lnp1;
		free((void *) lnp2);
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
	lnp1->l_prevp->l_nextp = lnp3;
	lnp3->l_nextp = lnp2->l_nextp;
	lnp2->l_nextp->l_prevp = lnp3;
	lnp3->l_prevp = lnp1->l_prevp;

	// In all screens...
	scrp = sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			(void) fixdelnl2(&winp->w_face,lnp1,lnp2,lnp3);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In all buffers...
	bufp = bheadp;
	do {
		found = fixdelnl2(&bufp->b_face,lnp1,lnp2,lnp3);
		mkp = &bufp->b_mroot;
		do {
			if(mkp->mk_dot.lnp == lnp1) {
				mkp->mk_dot.lnp = lnp3;
				found = true;
				}
			else if(mkp->mk_dot.lnp == lnp2) {
				mkp->mk_dot.lnp = lnp3;
				mkp->mk_dot.off += lnp1->l_used;
				found = true;
				}
			} while((mkp = mkp->mk_nextp) != NULL);
		} while(!found && (bufp = bufp->b_nextp) != NULL);

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

// This function deletes up to n bytes, starting at point.  The text that is deleted may include newlines.  Positive n deletes
// forward; negative n deletes backward.  Returns current status if all of the characters were deleted, NotFound (bypassing
// rcset()) if they were not (because dot ran into a buffer boundary), or the appropriate status if an error occurred.  The
// deleted text is put in the kill ring if the EditKill flag is set; otherwise, the undelete buffer if the EditDel flag is set.
int ldelete(long n,ushort flags) {

	if(n == 0 || allowedit(true) != Success)	// Don't allow if buffer in read-only mode.
		return rc.status;

	ushort wflags;
	char *str1,*str2;
	Line *lnp;
	int offset,chunk;
	EScreen *scrp;
	EWindow *winp;
	Buffer *bufp;
	RingEntry *rep;
	Mark *mkp;
	bool found;
	DStrFab sf;
	bool hitEOB = false;

	// Set kill buffer pointer.
	rep = (flags & EditKill) ? kring.r_entryp : (flags & EditDel) ? &undelbuf : NULL;
	wflags = 0;

	// Going forward?
	if(n > 0) {
		if(rep != NULL && dopenwith(&sf,&rep->re_data,SFAppend) != 0)
			return librcset(Failure);
		do {
			// Get the current point.
			lnp = curwp->w_face.wf_dot.lnp;
			offset = curwp->w_face.wf_dot.off;

			// Can't delete past end of buffer.
			if(lnp == curbp->b_hdrlnp)
				goto EOB;

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
				wflags |= WFHard;
				if(ldelnewline() != Success)
					return rc.status;
				if(rep != NULL && dputc('\n',&sf) != 0)
					return librcset(Failure);
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
				return librcset(Failure);

			// Copy what is left of the line upward.
			while(str2 < lnp->l_text + lnp->l_used)
				*str1++ = *str2++;
			lnp->l_used -= chunk;

			// Fix any other windows with the same text displayed.  In all screens...
			scrp = sheadp;
			do {
				// In all windows...
				winp = scrp->s_wheadp;
				do {
					if(winp->w_face.wf_dot.lnp == lnp)
						fixdotdel(offset,chunk,&winp->w_face.wf_dot);

					// Onward to the next window.
					} while((winp = winp->w_nextp) != NULL);
				} while((scrp = scrp->s_nextp) != NULL);

			// In all buffers...
			bufp = bheadp;
			found = false;
			do {
				if(bufp->b_face.wf_dot.lnp == lnp) {
					fixdotdel(offset,chunk,&bufp->b_face.wf_dot);
					found = true;
					}
				mkp = &bufp->b_mroot;
				do {
					if(mkp->mk_dot.lnp == lnp) {
						fixdotdel(offset,chunk,&mkp->mk_dot);
						found = true;
						}
					} while((mkp = mkp->mk_nextp) != NULL);
				} while(!found && (bufp = bufp->b_nextp) != NULL);

			// Indicate we have deleted chunk characters.
			n -= chunk;
			} while(n > 0);
		}
	else {
		if(rep != NULL && dopenwith(&sf,&rep->re_data,SFPrepend) != 0)
			return librcset(Failure);
		do {
			// Get the current point.
			lnp = curwp->w_face.wf_dot.lnp;
			offset = curwp->w_face.wf_dot.off;

			// Can't delete past the beginning of the buffer.
			if(lnp == curbp->b_hdrlnp->l_nextp && (offset == 0))
				goto EOB;

			// Find out how many chars to delete on this line.
			chunk = offset;		// Size of chunk.
			if(chunk > -n)
				chunk = -n;

			// If at the beginning of a line, merge with the previous one.
			if(chunk == 0) {

				// Flag that we are making a hard change and delete newline.
				wflags |= WFHard;
				(void) backch(1);
				if(ldelnewline() != Success)
					return rc.status;
				if(rep != NULL && dputc('\n',&sf) != 0)
					return librcset(Failure);
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
				return librcset(Failure);

			// Copy what is left of the line downward.
			while(str1 < lnp->l_text + lnp->l_used)
				*str2++ = *str1++;
			lnp->l_used -= chunk;
			curwp->w_face.wf_dot.off -= chunk;

			// Fix any other windows with the same text displayed.  In all screens...
			scrp = sheadp;
			do {
				// In all windows...
				winp = scrp->s_wheadp;
				do {
					if(winp->w_face.wf_dot.lnp == lnp)
						fixdotdel(offset,-chunk,&winp->w_face.wf_dot);

					// Onward to the next window.
					} while((winp = winp->w_nextp) != NULL);
				} while((scrp = scrp->s_nextp) != NULL);

			// In all buffers...
			bufp = bheadp;
			found = false;
			do {
				if(bufp->b_face.wf_dot.lnp == lnp) {
					fixdotdel(offset,-chunk,&bufp->b_face.wf_dot);
					found = true;
					}
				mkp = &bufp->b_mroot;
				do {
					if(mkp->mk_dot.lnp == lnp) {
						fixdotdel(offset,-chunk,&mkp->mk_dot);
						found = true;
						}
					} while((mkp = mkp->mk_nextp) != NULL);
				} while(!found && (bufp = bufp->b_nextp) != NULL);

			// Update counter.
			n += chunk;
			} while(n < 0);
		}
	goto Retn;
EOB:
	hitEOB = true;
Retn:
	if(rep != NULL && dclose(&sf,sf_string) != 0)
		return librcset(Failure);
	if(wflags)
		bchange(curbp,wflags);
	return hitEOB ? NotFound : rc.status;
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
	if(getkey(&ek) != Success)
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
	else if(stabsize == 0)
		(void) linsert(n,'\t');
	else {
		// Scan forward from point to next non-space character.
		Dot dot = curwp->w_face.wf_dot;
		int len = dot.lnp->l_used;
		while(dot.off < len && dot.lnp->l_text[dot.off] == ' ')
			++dot.off;

		// Set len to size of first tab and loop n times.
		len = stabsize - getccol(&dot) % stabsize;
		do {
			if(linsert(len,' ') != Success)
				break;
			len = stabsize;
			} while(--n > 0);
		}

	return rc.status;
	}

// Finish a routine that modifies a block of lines.
static int lchange(Dot *odotp,bool backward,int count) {
	Dot *dotp = &curwp->w_face.wf_dot;

	kentry.thisflag &= ~CFVMove;		// Flag that this resets the goal column.
	if(backward) {				// Move point to end of line block.
		*dotp = *odotp;
		dotp->off = 0;
		(void) forwln(1);
		}
	if(count == 0 || count == -1)
		curwp->w_flags |= WFMove;
	else
		bchange(curbp,WFHard);		// Flag that a line other than the current one was changed.

	return count < 0 ? rc.status : rcset(Success,0,text307,count,count == 1 ? "" : "s");
						// "%d line%s changed"
	}

// Check for keywords or symbols at the beginning or end of the current line which indicate the beginning or end of a block. 
// Return 1 if beginning of block found, -1 if end, or 0 otherwise.  "len" is the line length minus any trailing white space and
// is assumed to be > 0.  This routine is called only when a language mode (C, MightEMacs, Perl, Ruby, or Shell) is active.
static int bebcheck(Line *lnp,int len) {
	int offset,txtlen;
	short c;
	char *str;

	//*** Check symbols or keywords at the end of the line. ***//

	// Left brace and not MightEMacs mode?
	if((c = lnp->l_text[--len]) == '{')
		return !(curbp->b_modes & MdMemacs);

	// Pipe sign and Ruby mode?
	if(c == '|')
		return (curbp->b_modes & MdRuby) != 0;

	// Right paren and Shell mode?
	if(c == ')' && (curbp->b_modes & MdShell))
		return 1;

	// Colon and C mode?
	if(c == ':' && len > 0 && lnp->l_text[len - 1] != ' ')
		return (curbp->b_modes & MdC) != 0;

	// ";;" and Shell mode?
	if(len >= 1) {
		str = lnp->l_text + len - 1;
		if(memcmp(str,";;",2) == 0)
			return (curbp->b_modes & MdShell) ? -1 : 0;
		}

	// Check four-letter keywords.
	// "else" and any mode, "loop" and MightEMacs mode, or "then" and Ruby or Shell mode?
	if(len == 3 || (len >= 4 && (lnp->l_text[len - 4] == '\t' || lnp->l_text[len - 4] == ' '))) {
		str = lnp->l_text + len - 3;
		if(memcmp(str,"else",4) == 0)
			return 1;
		if(memcmp(str,"loop",4) == 0)
			return (curbp->b_modes & MdMemacs) != 0;
		if(memcmp(str,"then",4) == 0)
			return (curbp->b_modes & (MdRuby | MdShell)) != 0;
		}

	// Check five-letter keywords.
	// "begin" and Ruby mode?
	if(len == 4 || (len >= 5 && (lnp->l_text[len - 5] == '\t' || lnp->l_text[len - 5] == ' '))) {
		str = lnp->l_text + len - 4;
		if(memcmp(str,"begin",5) == 0)
			return (curbp->b_modes & MdRuby) != 0;
		}

	// Check other keywords.
	// "do" and C, Ruby, or Shell mode, or "rescue" and Ruby mode?
	if(((len == 1 ||
	 (len >= 2 && (lnp->l_text[len - 2] == ' ' || lnp->l_text[len - 2] == ';' || lnp->l_text[len - 2] == '\t'))) &&
	 lnp->l_text[len - 1] == 'd' && lnp->l_text[len] == 'o'))
		return (curbp->b_modes & (MdC | MdShell | MdRuby)) != 0;
	if((len == 5 || (len >= 6 && (lnp->l_text[len - 6] == '\t' || lnp->l_text[len - 6] == ' '))) &&
	 memcmp(lnp->l_text + len - 5,"rescue",6) == 0)
		return (curbp->b_modes & MdRuby) != 0;

	//*** Check keywords or symbols at the beginning of the line. ***//

	// Move to end of any line indentation.
	txtlen = curwp->w_face.wf_dot.off;
	(void) begintxt();
	offset = curwp->w_face.wf_dot.off;
	curwp->w_face.wf_dot.off = txtlen;
	txtlen = len - offset + 1;

	// ";;" and Shell mode?
	if(txtlen >= 2) {
		str = lnp->l_text + offset;
		if(memcmp(str,";;",2) == 0)
			return (curbp->b_modes & MdShell) ? -1 : 0;
		}

	// Check two-letter keywords.
	// "if" and any mode?
	if(txtlen >= 3 && (lnp->l_text[offset + 2] == '(' || lnp->l_text[offset + 2] == ' ')) {
		str = lnp->l_text + offset;
		if(memcmp(str,"if",2) == 0)
			return 1;
		}

	// Check three-letter keywords.
	// "for" and any mode, or "def" and Ruby mode?
	if(txtlen >= 4 && (lnp->l_text[offset + 3] == ' ' ||
	 (!(curbp->b_modes & (MdMemacs | MdRuby)) && lnp->l_text[offset + 3] == '('))) {
		str = lnp->l_text + offset;
		if(memcmp(str,"for",3) == 0)
			return 1;
		if(memcmp(str,"def",3) == 0)
			return (curbp->b_modes & MdRuby) != 0;
		}

	// Check four-letter keywords.
	// "elif" and Shell mode, "next" and MightEMacs mode, or "when" and Ruby mode?
	// Note: C "case" is handled by trailing ':'.  Ruby "next" may be "next if..." so not checked.
	if(txtlen == 4 || (txtlen >= 5 && (lnp->l_text[offset + 4] == '(' || lnp->l_text[offset + 4] == ' ' ||
	 lnp->l_text[offset + 4] == ';'))) {
		str = lnp->l_text + offset;
		if(memcmp(str,"elif",4) == 0)
			return (curbp->b_modes & MdShell) != 0;
		if(memcmp(str,"next",4) == 0)
			return (curbp->b_modes & MdMemacs) ? -1 : 0;
		if(memcmp(str,"when",4) == 0)
			return (curbp->b_modes & MdRuby) != 0;
		}

	// Check five-letter keywords.
	// "while" and any mode, "elsif" or "until" and MightEMacs, Perl, or Ruby mode, "break" and C, MightEMacs, or Shell
	// mode, "macro" and MightEMacs mode, or "class" and Ruby mode?
	// Note: Ruby "break" may be "break if..." so not checked.
	if(txtlen == 5 || (txtlen >= 6 && (lnp->l_text[offset + 5] == '(' || lnp->l_text[offset + 5] == ' ' ||
	 lnp->l_text[offset + 5] == '\t' || lnp->l_text[offset + 5] == ';'))) {
		str = lnp->l_text + offset;
		if(memcmp(str,"while",5) == 0)
			return 1;
		if(memcmp(str,"elsif",5) == 0 || memcmp(str,"until",5) == 0)
			return (curbp->b_modes & (MdMemacs | MdPerl | MdRuby)) != 0;
		if(memcmp(str,"break",5) == 0)
			return (curbp->b_modes & (MdC | MdMemacs | MdShell)) ? -1 : 0;
		if(memcmp(str,"macro",5) == 0)
			return (curbp->b_modes & MdMemacs) != 0;
		if(memcmp(str,"class",5) == 0)
			return (curbp->b_modes & MdRuby) != 0;
		}

	// Check six-letter keywords.
	// "unless" and Perl or Ruby mode, "return" and any mode, or "module" and Ruby mode?
	if(txtlen == 6 || (txtlen >= 7 && (lnp->l_text[offset + 6] == '(' || lnp->l_text[offset + 6] == ' ' ||
	 lnp->l_text[offset + 6] == '\t' || lnp->l_text[offset + 6] == ';'))) {
		str = lnp->l_text + offset;
		if(memcmp(str,"unless",6) == 0)
			return (curbp->b_modes & (MdPerl | MdRuby)) != 0;
		if(memcmp(str,"return",6) == 0)
			return -1;
		if(memcmp(str,"module",6) == 0)
			return (curbp->b_modes & MdRuby) != 0;
		}

	// Check seven-letter keywords.
	// "else if" and C mode?
	// Note: C "default" is handled by trailing ':'.
	if(txtlen == 7 || (txtlen >= 8 && (lnp->l_text[offset + 7] == '(' || lnp->l_text[offset + 7] == ':' ||
	 lnp->l_text[offset + 7] == '\t' || lnp->l_text[offset + 7] == ' '))) {
		str = lnp->l_text + offset;
		if(memcmp(str,"else if",7) == 0)
			return (curbp->b_modes & MdC) != 0;
		}

	// Check eight-letter keywords.
	// "continue" and C or Shell mode?
	if(txtlen == 8 || (txtlen >= 9 && (lnp->l_text[offset + 8] == ';' || lnp->l_text[offset + 8] == ' '))) {
		str = lnp->l_text + offset;
		if(memcmp(str,"continue",8) == 0)
			return (curbp->b_modes & (MdC | MdShell)) ? -1 : 0;
		}

	return false;
	}

// Insert "c" (# or =) into the text at point -- we are in C or RUBY mode.
int inspre(short c) {
	Dot *dotp = &curwp->w_face.wf_dot;

	// If we are at the beginning of the line, no go.
	if(dotp->off == 0)
		return linsert(1,c);

	// Check if all white space before this position.
	if(!is_white(dotp->lnp,dotp->off))
		return linsert(1,c);

	// Delete back first...
	if(ldelete((long) -dotp->off,0) != Success)
		return rc.status;

	// and insert the required character.
	return linsert(1,c);
	}

// Change tabs to spaces in a block of lines.  If n is zero, use lines in current region.  No error if attempt to move past end
// of buffer.  Always leave point at end of line block.
int detabLine(Datum *rp,int n,Datum **argpp) {
	int (*nextln)(int n);
	int inc,len;
	int count = 0;
	bool changed;
	Dot *dotp = &curwp->w_face.wf_dot;
	Dot dot = *dotp;	// Original dot position.

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != Success)
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

	// Loop thru text, detabbing n lines.
	kentry.lastflag &= ~CFVMove;
	while(n != 0) {
		dotp->off = 0;		// Start at the beginning.
		changed = false;

		// Detab the entire current line.
		while(dotp->off < dotp->lnp->l_used) {

			// If we have a tab.
			if(dotp->lnp->l_text[dotp->off] == '\t') {
				len = htabsize - (dotp->off % htabsize);
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
		if(nextln(1) != Success)
			break;
		n -= inc;
		}

	return lchange(&dot,nextln == backln,count);
	}

// Define column-calculator macro.
#define nextab(a)	(a - (a % htabsize)) + htabsize

// Change spaces to tabs (where possible) in a block of lines.  If n is zero, use lines in current region.  No error if attempt
// to move past end of buffer.  Always leave point at end of line block.  Return status.
static int entabln(Datum *rp,int n,Dot *odotp,bool *backwardp,int *countp) {
	int (*nextln)(int n);
	int inc;		// Increment to next line.
	int fspace;		// Index of first space if in a run.
	int ccol;		// Current point column.
	int len,count = 0;
	bool changed;
	Dot *dotp = &curwp->w_face.wf_dot;

	*odotp = *dotp;		// Original dot position.

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != Success)
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

	// Loop thru text, entabbing n lines.
	kentry.lastflag &= ~CFVMove;
	while(n != 0) {
		// Entab the entire current line.
		ccol = dotp->off = 0;	// Start at the beginning.
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
					 insnlspace(-(htabsize - (ccol % htabsize)),EditSpace | EditHoldPt) != Success)
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
		if(nextln(1) != Success)
			break;
		n -= inc;
		}

	// Return results.
	*backwardp = (nextln == backln);
	*countp = count;
	return rc.status;
	}

// Insert given indentation before dot and remove any trailing spaces from it if hard tabs in effect.  Returns status.
static int insindent(Datum *indentp) {
	Dot *dotp = &curwp->w_face.wf_dot;

	// Insert indentation.
	if(linstr(indentp->d_str) != Success)
		return rc.status;

	// If hard tabs and indentation has trailing space(s)...
	if(stabsize == 0 && dotp->lnp->l_text[dotp->off -1] == ' ') {
		Line *lnp;
		int len,offset;
		Dot dot;
		bool backward;
		int count;

		// Entab indentation, then delete any trailing spaces from it.
		len = dotp->lnp->l_used - dotp->off;
		if(entabln(indentp,1,&dot,&backward,&count) != Success || lchange(&dot,backward,-(count + 1)) != Success)
			return rc.status;
		(void) backch(len + 1);
		lnp = dotp->lnp;
		offset = dotp->off;
		while(offset > 0 && lnp->l_text[offset - 1] == ' ') {
			if(ldelete(-1L,0) != Success)
				return rc.status;
			--offset;
			}
		}
	return rc.status;
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

// Insert a newline and indentation when in a programming language mode.
static int langnl(void) {
	int openClose;			// Was there a block open or close on current line?
	Line *lnp;
	int offset;
	Datum *indentp;

	// Trim the whitespace before the point (following the left fence).
	lnp = curwp->w_face.wf_dot.lnp;
	offset = curwp->w_face.wf_dot.off;
	while(offset > 0 && (lnp->l_text[offset - 1] == ' ' || lnp->l_text[offset - 1] == '\t')) {
		if(ldelete(-1L,0) != Success)
			return rc.status;
		--offset;
		}

	// Check for a left brace, etc., depending on language.
	openClose = (offset == 0) ? 0 : bebcheck(lnp,offset);

	// Put in the newline.
	if(lnewline() != Success)
		return rc.status;

	// If the new line is not blank, don't indent it!
	lnp = curwp->w_face.wf_dot.lnp;
	if(lnp->l_used != 0)
		return rc.status;

	// Hunt for the last non-blank line to get indentation from.
	for(;;) {
		if((lnp = lnp->l_prevp) == curbp->b_hdrlnp || !is_white(lnp,lnp->l_used))
			break;
		}

	// Get the indentation, if any.
	if(getindent(&indentp,lnp) != Success)
		return rc.status;
	if(indentp != NULL) {

		// Insert it.
		if(insindent(indentp) != Success)
			return rc.status;

		// Delete one tab backward if block close.
		if(openClose < 0 && deltab(-1,false) != Success)
			return rc.status;
		}

	// Insert one more tab for a block begin.
	return openClose > 0 ? instab(1) : rc.status;
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

		// If a newline, not holding point, and a language mode is active, auto-indent.
		if(!(flags & (EditSpace | EditHoldPt)) && n > 0 && (curbp->b_modes & MdGrp_Lang) &&
		 curwp->w_face.wf_dot.lnp != curbp->b_hdrlnp) {
			do {
				if(langnl() != Success)
					break;
				} while(--n > 0);
			}
		else {
			// If EditWrap flag, positive n, "Wrap" mode is enabled, "Over" and "Repl" modes are disabled, wrap
			// column is defined, and we are now past wrap column, execute user-assigned wrap hook.
			if((flags & EditWrap) && n > 0 && (curbp->b_modes & (MdWrap | MdGrp_Repl)) == MdWrap && wrapcol > 0 &&
			 getccol(NULL) > wrapcol)
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
				(void) backch(abs(n));
			}
		}

	return rc.status;
	}

// Insert a right fence or keyword into the text for current language mode, given right fence or last letter of keyword that
// was just entered (but not yet inserted into current line).  Possible keywords and languages are:
//	end, rescue					Ruby
//	else						C, Perl, Ruby, Shell
//	elsif						Perl, Ruby
//	elif, do, done, fi, esac			Shell
//	else, elsif, endif, endloop, endmacro		MightEMacs
//	then						Ruby, Shell
int insrfence(short c) {
	short ch;
	int count,thendo;
	Line *lnp;
	Dot origdot;
	Datum *indentp;
	int kwlen = 0;
	Dot *dotp = &curwp->w_face.wf_dot;

	// If dot is at the beginning of the line and the character is not a right brace (which is the only possible "trigger"
	// character at column 0), bag it.
	if(dotp->off == 0) {
		if(c != '}')
			goto BagIt;
		}
	else {
		char *str1,*str2;

		// Scan to see if we have all white space before right brace or keyword.  Count is number of characters thus.
		thendo = false;
		if(c == '}')
			count = dotp->off;
		else {
			// Keyword.  Set str1 to beginning of line and str2 to beginning of keyword.
			str2 = (str1 = dotp->lnp->l_text) + dotp->off;
			while(is_lower(str2[-1]))
				if(--str2 == str1)
					break;
			if((curbp->b_modes & MdMemacs) && str2 > str1 && str2[-1] == '!')
				--str2;
			count = str2 - str1;
			}
		if(!is_white(dotp->lnp,count))
			goto BagIt;

		// Now check if keyword is a "right fence" for current language mode.  kwlen is length of symbol or keyword - 1.
		if(c != '}') {

			// Keyword length greater than maximum possible (endmacro without the 'o')?
			if((kwlen = dotp->off - count) == 0 || kwlen > 7)
				goto BagIt;

				// "end", "rescue"
			if(((curbp->b_modes & MdRuby) && ((kwlen == 2 && c == 'd' && memcmp(str2,"en",2) == 0) ||
			 (kwlen == 5 && c == 'e' && memcmp(str2,"rescu",5) == 0))) ||
				// "else"
			 (kwlen == 3 && c == 'e' && memcmp(str2,"els",3) == 0) ||
				// "elsif"
			 ((curbp->b_modes & (MdMemacs | MdPerl | MdRuby)) && kwlen == 4 && c == 'f' &&
			  memcmp(str2,"elsi",4) == 0) ||
				// endif, endloop, endmacro
			 ((curbp->b_modes & MdMemacs) && ((kwlen == 4 && c == 'f' && memcmp(str2,"endi",4) == 0) ||
			  (kwlen == 6 && c == 'p' && memcmp(str2,"endloo",6) == 0) ||
			  (kwlen == 7 && c == 'o' && memcmp(str2,"endmacr",7) == 0))) ||
			 	// "fi", "do", "done", "elif", "esac"
			 ((curbp->b_modes & MdShell) &&
			  ((kwlen == 1 && ((c == 'i' && *str2 == 'f') || (thendo = (c == 'o' && *str2 == 'd')))) ||
			   (kwlen == 3 && ((c == 'e' && memcmp(str2,"don",3) == 0) || (c == 'f' && memcmp(str2,"eli",3) == 0) ||
			   (c == 'c' && memcmp(str2,"esa",3) == 0))))) ||
				// "then"
			 ((curbp->b_modes & (MdRuby | MdShell)) &&
			  (kwlen == 3 && (thendo = (c == 'n' && memcmp(str2,"the",3) == 0)))))
				;
			else
				goto BagIt;
			}
		}

	// It's a go: white space (or no space) and matching keyword found.  Save current (original) position.
	origdot = *dotp;

	// If a right brace was entered, check if matching left brace at same nesting level exists (moving dot).
	if(c == '}') {
		count = 1;
		(void) backch(1);	// Begin search at character immediately preceding dot.

		while(count > 0) {
			ch = (dotp->off == dotp->lnp->l_used) ? '\n' : dotp->lnp->l_text[dotp->off];
			if(ch == '}')
				++count;
			else if(ch == '{')
				--count;
			(void) backch(1);
			if(boundary(dotp,Backward))
				break;
			}

		if(count != 0) {		// No match... don't alter indentation.
			*dotp = origdot;
			goto BagIt;
			}
		}
	else {
		// A letter was entered: hunt for the last non-blank line to get indentation from.
		lnp = dotp->lnp;
		for(;;) {
			if((lnp = lnp->l_prevp) == curbp->b_hdrlnp)
				goto BagIt;
			if(!is_white(lnp,lnp->l_used))
				break;
			}
		dotp->lnp = lnp;
		}

	// Dot is now (somewhere) on line containing desired indentation.  Get it.
	if(getindent(&indentp,dotp->lnp) != Success)
		return rc.status;

	// Restore original position and set indentation of line, if applicable.
	*dotp = origdot;

	// Move to beginning of keyword and delete any white space before dot.
	if(kwlen > 0)
		(void) backch(kwlen);
	if(ldelete((long) -dotp->off,0) != Success)
		return rc.status;

	// If indentation was found, insert it.
	if(indentp != NULL && insindent(indentp) != Success)
		return rc.status;

	// Insert one extra tab if "extra indent" mode (which applies only to a right brace)...
	if(c == '}') {
		if(curbp->b_modes & MdXIndt)
			(void) instab(1);
		}

	// delete one tab backward if indentation line did not contain "then" or "do" keyword by itself...
	else if(!thendo && deltab(-1,false) != Success)
		return rc.status;

	// and return to starting position if needed.
	if(kwlen > 0)
		(void) forwch(kwlen);
BagIt:
	// Indentation adjusted: now insert the "trigger" character.
	return linsert(1,c);
	}

// Change spaces to tabs (where possible) in a block of lines via entabln().  Return status.
int entabLine(Datum *rp,int n,Datum **argpp) {
	Dot dot;
	bool backward;
	int count;

	if(entabln(rp,n,&dot,&backward,&count) == Success)
		(void) lchange(&dot,backward,count);
	return rc.status;
	}

// Trim trailing whitespace from current line.  Return 1 if any whitespace was found; otherwise, 0.
static int trimln(void) {
	Line *lnp = curwp->w_face.wf_dot.lnp;
	int len = lnp->l_used;
	while(len > 0) {
		if(lnp->l_text[len - 1] != ' ' && lnp->l_text[len - 1] != '\t')
			break;
		--len;
		}
	if(len == lnp->l_used)
		return 0;
	lnp->l_used = len;
	return 1;
	}

// Trim trailing whitespace from a block of lines.  If n is zero, use lines in current region.  No error if attempt to move past
// end of buffer.  Always leave point at end of line block.
int trimLine(Datum *rp,int n,Datum **argpp) {
	int (*nextln)(int n);
	int inc,count = 0;
	Dot *dotp = &curwp->w_face.wf_dot;
	Dot dot = *dotp;	// Original dot position.

	// Compute block size and set direction.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != Success)
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

	// Loop thru text, trimming n lines.
	kentry.lastflag &= ~CFVMove;
	while(n != 0) {
		count += trimln();

		// Move forward or backward to the next line.
		dotp->off = 0;
		if(nextln(1) != Success)
			break;
		n -= inc;
		}

	return lchange(&dot,nextln == backln,count);
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
	(void) backch(n);		// Backup over them all.
	return rc.status;
	}

// Insert a line at [-]nth line with same indentation.
int insertLineI(Datum *rp,int n,Datum **argpp) {

	// Non-empty buffer?
	if(curbp->b_hdrlnp->l_nextp != curbp->b_hdrlnp) {
		Datum *dsinkp,*indentp;
		Dot *dotp = &curwp->w_face.wf_dot;

		if(dnewtrk(&dsinkp) != 0)
			return librcset(Failure);
		(void) beline(dsinkp,n,false);			// Move to beginning of target line...
		if(getindent(&indentp,dotp->lnp) != Success ||	// get indentation...
		 lnewline() != Success)				// insert a newline...
			return rc.status;
		(void) backch(1);				// back up over it...
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
			(void) backch(len);
			i += ivar.inc;
			} while(++n < 0);
		}

	ivar.i = i;
	return rc.status;
	}

// Delete blank lines around point.  If point is on a blank line, this command deletes all the blank lines above and below the
// current line.  If it is on a non-blank line, then it deletes all of the blank lines after the line.  Any argument is ignored.
int deleteBlankLines(Datum *rp,int n,Datum **argpp) {
	Line *lnp1,*lnp2;
	long count;			// Characters to delete.
	Dot *dotp = &curwp->w_face.wf_dot;

	lnp1 = dotp->lnp;
	while(is_white(lnp1,lnp1->l_used) && (lnp2 = lnp1->l_prevp) != curbp->b_hdrlnp)
		lnp1 = lnp2;
	lnp2 = lnp1;
	count = 0;
	while((lnp2 = lnp2->l_nextp) != curbp->b_hdrlnp && is_white(lnp2,lnp2->l_used))
		count += lnp2->l_used + 1;

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
	dotp->off = 0;
	return ldelete((long) count,0);
	}

// Insert a newline, then enough tabs and spaces to duplicate the indentation of the previous line.  Tabs are every htabsize
// characters.  Quite simple.  Figure out the indentation of the current line.  Insert a newline by calling the standard
// routine.  Insert the indentation by inserting the right number of tabs and spaces.  Return status.  Normally bound to ^J.
int newlineI(Datum *rp,int n,Datum **argpp) {
	Datum *indentp;
	Line *lnp = curwp->w_face.wf_dot.lnp;

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
	Dot *dotp = &curwp->w_face.wf_dot;

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
	if(stabsize == 0) {
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
			col1 = ((col2 = getccol(NULL)) - 1) / stabsize * stabsize;
			chunk1 = col2 - col1;

			// If deleting forward...
			if(n > 0) {
				// If prior tab stop is before dot, stop here if non-space character is not a tab stop or dot is
				// not at BOL and the character just prior to dot is a space.
				if(col1 < dotcol && (col2 % stabsize != 0 || (off > 0 && dotp->lnp->l_text[off - 1] == ' '))) {
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
			if(len >= chunk1 || (len > 0 && dotcol % stabsize == 0)) {
Nuke:;
				// Delete one or more soft tabs.  Determine number of spaces to delete and nuke 'em.
				int max = (len - chunk1) / stabsize;
				int m = abs(n) - 1;
				m = (m > max ? max : m);
				if(ldelete((long) -(len < chunk1 ? len : (chunk1 + m * stabsize)),0) != Success)
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

// Create region from given dot and n argument, using n as a text (not line) selector.  If forceBegin is true, force dot in
// Region object to beginning of region; otherwise, leave dot at original starting point (and negate r_size if region extends
// backward).  Return signed region size.
static void gettregion(Dot *dotp,int n,Region *regp,bool forceBegin) {
	Line *nextp;
	long chunk;

	regp->r_dot = *dotp;
	if(n == INT_MIN || n == 1) {			// From dot to end of line.
		if((chunk = dotp->lnp->l_used - dotp->off) == 0)
			chunk = 1;			// Select line terminator.
		}
	else if(n == 0) {				// From dot to beginning of line.
		if(forceBegin)
			regp->r_dot.off = 0;
		chunk = -dotp->off;
		}
	else if(n > 1) {				// From dot forward through multiple lines to end of last line.
		chunk = dotp->lnp->l_used - dotp->off;
		nextp = dotp->lnp->l_nextp;
		do {
			if(nextp == curbp->b_hdrlnp)
				break;
			chunk += 1 + nextp->l_used;
			nextp = nextp->l_nextp;
			} while(--n > 1);
		}
	else {						// From dot backward through multiple lines to beginning of first line.
		if(forceBegin)
			regp->r_dot.off = 0;
		chunk = -dotp->off;
		nextp = dotp->lnp->l_prevp;
		do {
			if(nextp == curbp->b_hdrlnp)
				break;
			chunk -= nextp->l_used + 1;
			if(forceBegin)
				regp->r_dot.lnp = nextp;
			nextp = nextp->l_prevp;
			} while(++n < 0);
		}

	// Return results.
	regp->r_size = forceBegin ? labs(chunk) : chunk;
	}

// Kill, delete, or copy text if kdc is -1, 0, or 1, respectively.  Save text to kill ring if kdc is non-zero.  If regp is not
// NULL, operate on the region.  Otherwise, if called with an argument of 1 (the default), operate from dot to the end of the
// line, unless dot is at the end of the line, in which case operate on the newline.  If called with an argument of 0, operate
// from dot to the beginning of the line.  If called with a positive argument, operate from dot forward over that number of line
// breaks to the end of the last line.  If called with a negative argument, operate backward over that number of line breaks to
// the beginning of the first line.  Return status.
int kdctext(int n,int kdc,Region *regp) {
	Region region;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Process region elsewhere if specified.
	if(regp != NULL) {
		if(kdc > 0)
			goto Kopy;
		if(kprep(kdc) != Success)
			return rc.status;
		*dotp = regp->r_dot;
		return ldelete(regp->r_size,kdc ? EditKill : EditDel);
		}

	// No region... check if at end of buffer.
	if(dotp->lnp == curbp->b_hdrlnp && (n == INT_MIN || n > 0))
		return rcset(Failure,RCNoFormat,text259);
				// "No text selected"

	// Process lines and make a region.
	gettregion(dotp,n,&region,false);

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
	if(copyreg(regp) != Success)
		return rc.status;
	return rcset(Success,0,"%s %s",text261,text262);
				// "Text","copied"
	}

// Get a region bounded by a line block.  Force dot in Region object to beginning of block.  If onelinep not NULL, set *onelinep
// to true if block contains one line; otherwise, false.  Return status.
static int getlregion(int n,Region *regp,bool *onelinep) {
	Dot *dotp = &curwp->w_face.wf_dot;
	int used1 = dotp->lnp->l_used;
	if(onelinep != NULL)
		*onelinep = false;

	if(n == 0) {						// If zero argument...
		if(getregion(regp,true,NULL) != Success)	// select all lines in region.
			return rc.status;

		// If region begins at point, expand to include text from dot to beginning of line and text at end of last (mark
		// RMark) line plus line terminator; otherwise, expand to include text at beginning of first (mark RMark) line
		// and text from dot to beginning of next line.
		regp->r_size += ((regp->r_dot.lnp == dotp->lnp && regp->r_dot.off == dotp->off) ?
		 dotp->off + curbp->b_mroot.mk_dot.lnp->l_used - curbp->b_mroot.mk_dot.off :
		 curbp->b_mroot.mk_dot.off + dotp->lnp->l_used - dotp->off) + 1;
		regp->r_dot.off = 0;
		return rc.status;
		}

	// Not selecting all lines in current region.  Check if at end of buffer.
	Dot dot = *dotp;
	if(n == INT_MIN)
		n = 1;
	if(dotp->lnp == curbp->b_hdrlnp) {
		if(n > 0)
			return rcset(Failure,RCNoFormat,text259);
					// "No text selected"
		if(onelinep != NULL)
			*onelinep = (n == -1);
		}
	else if(n < 0) {					// Going backward?
		dot.lnp = dot.lnp->l_nextp;			// Yes, move to beginning of next line...
		dot.off = 0;
		--n;						// and bump the line count.
		}
	else {
		dot.off = 0;					// No, move to beginning of line.
		if(onelinep != NULL)
			*onelinep = (n == 1);
		}

	// Convert line block to a region.
	gettregion(&dot,n,regp,true);

	// Add one more line break if n > 1 or n == 1 and first line wasn't empty.
	if(n > 1 || (n == 1 && used1 > 0))
		++regp->r_size;

	return rc.status;
	}

// Kill, delete, or copy whole line(s) via kdctext() if kdc is -1, 0, or 1, respectively.
int kdcline(int n,int kdc) {
	bool oneline;
	Region region;

	// Convert line block to a region.
	if(getlregion(n,&region,&oneline) != Success)
		return rc.status;

	// Nuke or copy line block (as a region) via kdctext().
	if(kdc <= 0)
		curwp->w_face.wf_dot = region.r_dot;
	if(kdctext(0,kdc,&region) != Success)
		return rc.status;

	return (kdc <= 0) ? rc.status : rcset(Success,RCForce,"%s%s %s",text260,oneline ? "" : "s",text262);
							// "Line","copied"
	}

// Duplicate whole line(s) in the current buffer, leaving point at beginning of second line block.  Return status.
int dupLine(Datum *rp,int n,Datum **argpp) {
	Region region;

	// Convert line block to a region.
	if(getlregion(n,&region,NULL) != Success)
		return rc.status;

	// Copy region to a local (stack) variable.
	char wkbuf[region.r_size + 1];
	regcpy(wkbuf,&region);

	// Move dot to beginning of line block and insert it.
	curwp->w_face.wf_dot = region.r_dot;
	if(linstr(wkbuf) == Success)
		(void) begintxt();
	return rc.status;
	}

// Delete white space at and forward from point (if any) on current line.  If n > 0, include non-word characters as well.
static int delwhitef(int n) {
	short c;
	Dot *dotp = &curwp->w_face.wf_dot;

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
	Dot *dotp = &curwp->w_face.wf_dot;
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
	Dot *dotp = &curwp->w_face.wf_dot;

	// Determine bounds of line block.
	if(n == INT_MIN)
		n = -1;						// Join with one line above by default.
	else if(n == 1 ||
	 (n == 0 && (reglines(&n,NULL) != Success || n == 1)))	// Join all lines in region.
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
			if(dotp->lnp->l_prevp == curbp->b_hdrlnp)
				break;
			dotp->lnp = dotp->lnp->l_prevp;
			}
		else if(dotp->lnp->l_nextp == curbp->b_hdrlnp)
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
	if((opflags & OpScript) && !disnull(argpp[0]))
		delimp = argpp[0];

	// Call joinln().
	return joinln(rp,n,delimp);
	}

// Kill, delete, or copy fenced region if kdc is -1, 0, or 1, respectively.  Return status.
bool kdcFencedRegion(int kdc) {
	Region region;

	if(otherfence(&region) == Success)
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
	Dot *dotp = &curwp->w_face.wf_dot;

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
					(void) forwch(1);
					if(dotp->lnp->l_text[dotp->off] == ' ') {
						if(delwhite(0,false) != Success)
							return rc.status;
						break;
						}
					}
			return lnewline();
			}

		// Back up one character.
		(void) backch(1);
		++wordsz;
		} while(dotp->lnp->l_text[dotp->off] != ' ');

	// Found a space.  Replace it with a newline.
	if(delwhite(0,false) != Success || lnewline() != Success)
		return rc.status;

	// Move back to where we started.
	if(wordsz > 0)
		(void) forwch(wordsz);

	// Make sure the display is not horizontally scrolled.
	if(curwp->w_face.wf_firstcol > 0) {
		curwp->w_face.wf_firstcol = 0;
		curwp->w_flags |= (WFHard | WFMode);
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
	if(wrapcol == 0)
		return rcset(Failure,RCNoFormat,text98);
			// "Wrap column not set"

	// Get prefix and end-sentence delimiters if script mode.
	if(opflags & OpScript) {
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
	dotp = &curwp->w_face.wf_dot;
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != Success)			// Process all lines in region.
		return rc.status;
	else if(n < 0) {						// Back up to first line.
		int count = 1;
		while(dotp->lnp->l_prevp != curbp->b_hdrlnp) {
			dotp->lnp = dotp->lnp->l_prevp;
			++count;
			if(++n == 0)
				break;
			}
		if((n = count) > 1)
			curwp->w_flags |= WFMove;
		}
	dotp->off = 0;							// Move to beginning of line.

	// Dot now at beginning of first line and n > 0.
	(void) begintxt();						// Move to beginning of text.
	if((indentcol = getccol(NULL)) + prefixLen >= wrapcol)		// Too much indentation?
		return rcset(Failure,0,text323,wrapcol);
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
			} while((dotp->lnp = dotp->lnp->l_nextp) != curbp->b_hdrlnp && --count > 0);
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
				if(col >= wrapcol && (c != ' ' && c != '\t')) {		// At or past wrap column and not space?
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
	(void) forwch(1);						// Can't fail.
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
	Dot *dotp = &curwp->w_face.wf_dot;
	bool (*isul)(short c) = (ultab == NULL) ? NULL : (ultab == upcase) ? is_lower : is_upper;

	do {
		while(!inword()) {
			if(forwch(1) != Success || --ccount == 0)
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
			if(forwch(1) != Success || --ccount == 0)
				goto Retn;
			}
		} while(--wcount > 0);
Retn:
	if(changedp != NULL)
		*changedp = changed;
	else if(changed)
		bchange(curbp,WFHard);
	}

// Convert a block of lines to lower, title, or upper case.  If n is zero, use lines in current region.  If trantab is NULL, do
// title case.  No error if attempt to move past end of buffer.  Always leave point at end of line block.
static int caseline(int n,char *trantab) {
	int (*nextln)(int n);
	bool changed;
	int inc,count = 0;
	Dot *dotp = &curwp->w_face.wf_dot;
	Dot dot = *dotp;	// Original dot position.

	// Compute block size.
	if(n == INT_MIN)
		n = 1;
	else if(n == 0 && reglines(&n,NULL) != Success)
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

	// Loop through buffer text, changing case of n lines.
	kentry.lastflag &= ~CFVMove;
	while(n != 0) {

		// Process the current line from the beginning.
		if(dotp->lnp->l_used > 0) {
			dotp->off = 0;
			casetext(INT_MAX,dotp->lnp->l_used,trantab,&changed);
			if(changed)
				++count;
			}

		// Move forward or backward to the next line.
		dotp->off = 0;
		if(nextln(1) != Success)
			break;
		n -= inc;
		}

	return lchange(&dot,nextln == backln,count);
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

		if(getregion(&region,true,NULL) == Success && region.r_size > 0) {
			bool backward = false;
			Dot *dotp = &curwp->w_face.wf_dot;

			mset(&curbp->b_mroot,curwp);
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

			mset(&curbp->b_mroot,curwp);
			(void) backwd(-n);
			if(getregion(&region,true,NULL) == Success && region.r_size > 0) {
				casetext(INT_MAX,region.r_size,trantab,NULL);
				curwp->w_face.wf_dot = region.r_dot;
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
	Dot *dotp = &curwp->w_face.wf_dot;

	// Check if at end of buffer.
	if(dotp->lnp == curbp->b_hdrlnp)
		return rcset(Failure,RCNoFormat,text259);
				// "No text selected"

	// Save the current point position.
	region.r_dot = *dotp;

	// Figure out how many characters to copy or give the axe.
	region.r_size = 0;

	// Get into a word...
	while(!inword()) {
		if(forwch(1) != Success)
			break;					// At end of buffer.
		++region.r_size;
		}

	oneword = false;
	if(n == 0) {
		// Skip one word, no whitespace.
		while(inword()) {
			if(forwch(1) != Success)
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
				if(forwch(1) != Success)
					goto Nuke;		// At end of buffer.
				++region.r_size;
				}

			// Move forward until we are at the end of the word.
			while(inword()) {
				if(forwch(1) != Success)
					goto Nuke;		// At end of buffer.
				++region.r_size;
				}

			// If there are more words, skip the interword stuff.
			if(n != 0)
				while(!inword()) {
					if(forwch(1) != Success)
						goto Nuke;	// At end of buffer.
					++region.r_size;
					}
			}

		// Skip whitespace and newlines.
		while(dotp->off == dotp->lnp->l_used || (c = dotp->lnp->l_text[dotp->off]) == ' ' ||
		 c == '\t') {
			if(forwch(1) != Success)
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
	return rcset(Success,0,"%s%s %s",text115,oneword ? "" : "s",text262);
				// "Word","copied"
	}

// Kill, delete, or copy backward by "n" words (which is always > 0).  Save text to kill ring if kdc is non-zero.
int kdcbword(int n,int kdc) {
	bool oneword;
	long size;
	Region region;

	// Check if at beginning of buffer.
	if(backch(1) != Success)
		return rcset(Failure,RCNoFormat,text259);
				// "No text selected"

	// Figure out how many characters to copy or give the axe.
	size = 0;

	// Skip back n words...
	oneword = (n == 1);
	do {
		while(!inword()) {
			++size;
			if(backch(1) != Success)
				goto CopyNuke;		// At beginning of buffer.
			}
		while(inword()) {
			++size;
			if(backch(1) != Success)
				goto CopyNuke;		// At beginning of buffer.
			}
		} while(--n > 0);

	if(forwch(1) != Success)
		return rc.status;
CopyNuke:
	// Have region, restore original position...
	(void) forwch(size);		// Can't fail.

	// and kill, delete, or copy region.
	if(kdc <= 0) {

		// Kill or delete the word(s) backward.
		if(kprep(kdc) == Success)
			(void) ldelete(-size,kdc ? EditKill : EditDel);
		return rc.status;
		}

	// Copy the word(s) backward.
	region.r_dot = curwp->w_face.wf_dot;
	region.r_size = -size;
	if(copyreg(&region) != Success)
		return rc.status;
	return rcset(Success,0,"%s%s %s",text115,oneword ? "" : "s",text262);
				// "Word","copied"
	}
