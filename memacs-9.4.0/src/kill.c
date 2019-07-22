// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// kill.c	Data and kill ring functions for MightEMacs.
//
// These routines manage the kill ring and undelete buffer.

#include "os.h"
#include "std.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"

static long lastysize = -1;	// Last number of bytes yanked (used by yankCycle to delete prior yank).

// Cycle a data ring forward or backward abs(n) times.  Return status.
int rcycle(Ring *ring,int n,bool msg) {

	if(ring->r_size == 0)
		(void) rcset(Failure,0,text202,ring->r_rname);
				// "%s ring is empty"
	else if(n != 0 && ring->r_size > 1) {
		if(n == INT_MIN)
			n = 1;
		if(n < 0)
			do {
				ring->r_entry = ring->r_entry->re_next;
				} while(++n < 0);
		else
			do {
				ring->r_entry = ring->r_entry->re_prev;
				} while(--n > 0);
		if(msg)
			(void) rcset(Success,0,text42,ring->r_rname);
				// "%s ring cycled"
		}
	return rc.status;
	}

// Create a new RingEntry object and insert it into the given ring at the current position if room; otherwise, clear the oldest
// entry and cycle the ring so it's current.  Return status.
static int rnew(Ring *ring) {

	if(ring->r_maxsize == 0 || ring->r_size < ring->r_maxsize) {
		RingEntry *rep = (RingEntry *) malloc(sizeof(RingEntry));
		if(rep == NULL)
			return rcset(Panic,0,text94,"rnew");
					// "%s(): Out of memory!"
		dinit(&rep->re_data);
		if(ring->r_entry == NULL)
			ring->r_entry = rep->re_next = rep->re_prev = rep;
		else {
			rep->re_next = ring->r_entry->re_next;
			rep->re_prev = ring->r_entry;
			ring->r_entry = ring->r_entry->re_next = ring->r_entry->re_next->re_prev = rep;
			}
		++ring->r_size;
		}
	else {
		(void) rcycle(ring,-1,false);		// Can't fail.
		dclear(&ring->r_entry->re_data);
		}

	return rc.status;
	}

// Prepare for new kill or delete.  Return status.
int kprep(int kill) {
	ushort flag = kill ? CFKill : CFDel;

	if(!(kentry.lastflag & flag))				// If last command was not in "kill" or "delete" family...
		(void) rnew(flag == CFKill ? &kring : &dring);	// cycle kill or delete ring and start fresh (don't append this
								// kill or delete to the last one).

	kentry.thisflag |= flag;				// Mark this command as a "kill" or "delete".
	return rc.status;
	}

#if MMDebug & Debug_RingDump
// Dump a ring to the log file.
void dumpring(Ring *ring) {
	ushort size = 0;

	fprintf(logfile,"  RING DUMP -- %s (%hu entries)\n",ring->r_rname,ring->r_size);
	if(ring->r_size > 0) {
		int n = 0;
		RingEntry *rep1 = ring->r_entry;
		RingEntry *rep = rep1;
		do {
			fprintf(logfile,"%6d  %.80s\n",n--,rep->re_data.d_str);
			++size;
			} while((rep = rep->re_next) != rep1);
		}
	fprintf(logfile,"  %hu entries found.\n",size);
	}
#endif

// Save a pattern to a ring if not null.  If pattern matches an existing entry, move old entry to the top (without cycling);
// otherwise, create an entry.  Return status.
int rsetpat(char *pat,Ring *ring) {

	if(*pat != '\0') {
		if(ring->r_size > 0) {
			RingEntry *rep1 = ring->r_entry;
			RingEntry *rep = rep1;

			// Pattern already in ring?
			do {
				if(strcmp(rep->re_data.d_str,pat) == 0) {

					// Found it.  Move it to the top if not already there.
					if(rep != rep1) {

						// Ring contains at least two entries.  If only two, just change the header
						// pointer; otherwise, remove matched entry from its current slot and insert it
						// at the top.
						ring->r_entry = rep;
						if(ring->r_size > 2) {
							rep->re_next->re_prev = rep->re_prev;
							rep->re_prev->re_next = rep->re_next;

							rep->re_next = rep1->re_next;
							rep->re_prev = rep1;
							rep1->re_next = rep1->re_next->re_prev = rep;
							}
						}
					return rc.status;
					}
				} while((rep = rep->re_next) != rep1);
			}

		// Pattern not found... add it.
		if(rnew(ring) == Success && dsetstr(pat,&ring->r_entry->re_data) != 0)
			(void) librcset(Failure);
		}
	return rc.status;
	}

// Get ring entry specified by n and return pointer to it.  If not found, set an error and return NULL.
RingEntry *rget(Ring *ring,int n) {
	RingEntry *rep;

	// n in range?
	if(ring->r_size == 0) {
		(void) rcset(Failure,0,text202,ring->r_rname);
				// "%s ring is empty"
		return NULL;
		}
	if(n > 0 || n <= -ring->r_size) {
		(void) rcset(Failure,0,text19,n,-((int) ring->r_size - 1));
				// "No such ring entry %d (max %d)"
		return NULL;
		}

	// n is valid.  Find entry #n in ring...
	for(rep = ring->r_entry; n < 0; ++n)
		rep = rep->re_prev;

	// and return it.
	return rep;
	}

// Attempt to delete n entries from given ring, beginning at top.  It is assumed that ring->r_entry != NULL and n > 0.  Return
// number of entries actually deleted.
static ushort rtrim(Ring *ring,int n) {
	RingEntry *rep;
	RingEntry *repz = ring->r_entry->re_next;
	ushort sz0 = ring->r_size;

	do {
		rep = ring->r_entry;
		ring->r_entry = rep->re_prev;
		dclear(&rep->re_data);
		free((void *) rep);
		} while(--ring->r_size > 0 && --n > 0);
	if(ring->r_size == 0)
		ring->r_entry = NULL;
	else {
		ring->r_entry->re_next = repz;
		repz->re_prev = ring->r_entry;
		}

	return sz0 - ring->r_size;
	}

// Delete the most recent n entries (default 1) from given ring, kill number n if n < 0, or all entries if n == 0.  Return
// status.
int rdelete(Ring *ring,int n) {

	if(ring->r_size == 0)
		return rcset(Failure,0,text202,ring->r_rname);
				// "%s ring is empty"
	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		n = ring->r_size;
	else if(n < 0) {
		RingEntry *rep = rget(ring,n);
		if(rep == NULL)
			return rc.status;

		// Delete "rep" entry, which is not at top.  Kill ring contains at least two entries.
		rep->re_next->re_prev = rep->re_prev;
		rep->re_prev->re_next = rep->re_next;
		dclear(&rep->re_data);
		free((void *) rep);
		--ring->r_size;
		n = 1;
		goto NukeMsg;
		}

	// Delete the most recent n entries.
	int sz0 = ring->r_size;
	if((n = rtrim(ring,n)) == sz0)
		(void) rcset(Success,0,text228,ring->r_rname);
				// "%s ring cleared"
	else
NukeMsg:
		(void) rcset(Success,0,text357,ring->r_rname,ring->r_ename,n > 1 ? "s" : "");
				// "%s%s%s deleted"

	return rc.status;
	}

// Insert, overwrite, or replace a character at the current point.  Return status.
static int iorch(short c,ushort t) {

	// Newline?
	if(c == '\n' && !(t & Txt_LiteralNL))
		return lnewline();

	// Not a newline or processing literally.  Overwrite or replace and not at end of line?
	Point *point = &si.curwin->w_face.wf_point;
	if(!(t & Txt_Insert) && point->off < point->lnp->l_used) {

		// Yes.  Replace mode, not a tab, or at a tab stop?
		if((t & Txt_Replace) || point->lnp->l_text[point->off] != '\t' ||
		 getcol(NULL,false) % si.htabsize == (si.htabsize - 1)) {

			// Yes.  Delete before insert.
			if(ldelete(1L,0) != Success)
				return rc.status;
			}
		}

	// Insert the character! (amen).
	return linsert(1,c);
	}

// Insert, overwrite, or replace a string from a string buffer (src != NULL) or a kill or delete ring entry (src == NULL) at
// point n times.  In the latter case, use given ring and and mark text as a region if kill ring.  If n == 0, do one repetition
// and leave point before the new text; otherwise, after.  If n < 0 and src != NULL, treat newline characters literally.  If
// n < 0 and src == NULL, get the -nth ring entry instead of the most recent one without cycling the ring.  Update kill state
// if src is NULL.  Return status.
int iorstr(char *src,int n,ushort style,Ring *ring) {
	long size;
	int reps;
	char *str;		// Pointer into string to insert.
	Line *curline;
	int curoff;
	RingEntry *rep;
	bool pretext;		// Leave point before inserted text?
	Point *point = &si.curwin->w_face.wf_point;

	// Check if we have something to insert.  Not an error if no text.
	if(src == NULL) {

		// Which kill?
		if(ring->r_size == 0)
			return rcset(Failure,0,text202,ring->r_rname);
					// "%s ring is empty"
		else if(n >= 0)
			rep = ring->r_entry;
		else {
			if((rep = rget(ring,n)) == NULL)
				return rc.status;
			n = 1;
			}

		if(disnn(&rep->re_data)) {
			lastysize = 0;
			return rc.status;
			}

		// Save point in RegMark.
		mset(&si.curbuf->b_mroot,si.curwin);
		}
	else if(*src == '\0')
		return rc.status;

	// If n == 0, remember point.  However, must save the *previous* line (if possible), since the line we are on may
	// disappear due to re-allocation.
	if((pretext = n == 0)) {
		curline = (point->lnp == si.curbuf->b_lnp) ? NULL : point->lnp->l_prev;
		curoff = point->off;
		n = 1;
		}

	// For "reps" times...
	if(n < 0) {
		reps = 1;
		style |= Txt_LiteralNL;
		}
	else
		reps = n;
	while(reps-- > 0) {
		size = 0;
		str = (src == NULL) ? rep->re_data.d_str : src;
		do {
			if(iorch(*str++,style) != Success)
				return rc.status;
			++size;
			} while(*str != '\0');
		}

	// If requested, set point back to the beginning of the new text and move RegMark.
	if(pretext) {
		mset(&si.curbuf->b_mroot,si.curwin);
		point->lnp = (curline == NULL) ? si.curbuf->b_lnp : curline->l_next;
		point->off = curoff;
		}

	// Update kill state.
	if(src == NULL) {
		reps = (ring == &kring) ? CFYank : CFUndel;
		kentry.thisflag |= (pretext ? (reps | CFNMove) : reps);
		lastysize = size;
		}

	return rc.status;
	}

// Revert yanked or undeleted text; that is, undo the insertion.  Nothing is done unless prior command was a yank or undelete
// per given flags.  Return status.
int yrevert(ushort flags) {

	if(kentry.lastflag & flags) {
		(void) ldelete(kentry.lastflag & CFNMove ? lastysize : -lastysize,0);
		lastysize = 0;
		}
	return rc.status;
	}

// Yank or undelete text, replacing the text from the last invocation if this not the first.
int ycycle(Datum *rval,int n,Ring *ring,ushort flag,ushort cmd) {

	// If not the first consecutive time...
	if((kentry.lastflag & flag) && n != 0) {
		if(yrevert(flag) == Success &&			// delete the last yank or undelete...
		 rcycle(ring,n,false) == Success)		// cycle the ring per n...
			(void) execCF(rval,1,cftab + cmd,0,0);	// and insert the current ring entry.
		}
	return rc.status;
	}
