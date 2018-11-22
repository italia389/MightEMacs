// (c) Copyright 2018 Richard W. Marinelli
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
int rcycle(Ring *ringp,int n,bool msg) {

	if(ringp->r_size == 0)
		(void) rcset(Failure,0,text202,ringp->r_name);
				// "%s ring is empty"
	else if(n != 0 && ringp->r_size > 1) {
		if(n == INT_MIN)
			n = 1;
		if(n < 0)
			do {
				ringp->r_entryp = ringp->r_entryp->re_nextp;
				} while(++n < 0);
		else
			do {
				ringp->r_entryp = ringp->r_entryp->re_prevp;
				} while(--n > 0);
		if(msg)
			(void) rcset(Success,0,text42,ringp->r_name);
				// "%s ring cycled"
		}
	return rc.status;
	}

// Create a new RingEntry object and insert it into the given ring at the current position if room; otherwise, clear the oldest
// entry and cycle the ring so it's current.  Return status.
static int rnew(Ring *ringp) {

	if(ringp->r_maxsize == 0 || ringp->r_size < ringp->r_maxsize) {
		RingEntry *rep = (RingEntry *) malloc(sizeof(RingEntry));
		if(rep == NULL)
			return rcset(Panic,0,text94,"rnew");
					// "%s(): Out of memory!"
		dinit(&rep->re_data);
		if(ringp->r_entryp == NULL)
			ringp->r_entryp = rep->re_nextp = rep->re_prevp = rep;
		else {
			rep->re_nextp = ringp->r_entryp->re_nextp;
			rep->re_prevp = ringp->r_entryp;
			ringp->r_entryp = ringp->r_entryp->re_nextp = ringp->r_entryp->re_nextp->re_prevp = rep;
			}
		++ringp->r_size;
		}
	else {
		(void) rcycle(ringp,-1,false);		// Can't fail.
		dclear(&ringp->r_entryp->re_data);
		}

	return rc.status;
	}

// Prepare for new kill or delete.  Return status.
int kprep(int kill) {
	ushort flag = kill ? CFKill : CFDel;

	if(!(kentry.lastflag & flag))				// If last command was not in "kill" or "delete" family...
		flag == CFKill ? (void) rnew(&kring) :		// cycle kill ring or reset undelete buffer and start fresh.
		 dclear(&undelbuf.re_data);			// (Don't append this kill or delete to the last one.)
	kentry.thisflag |= flag;				// Mark this command as a "kill" or "delete".
	return rc.status;
	}

#if MMDebug & Debug_RingDump
// Dump a ring to the log file.
void dumpring(Ring *ringp) {
	ushort size = 0;

	fprintf(logfile,"  RING DUMP -- %s (%hu entries)\n",ringp->r_name,ringp->r_size);
	if(ringp->r_size > 0) {
		int n = 0;
		RingEntry *rep1 = ringp->r_entryp;
		RingEntry *rep = rep1;
		do {
			fprintf(logfile,"%6d  %.80s\n",n--,rep->re_data.d_str);
			++size;
			} while((rep = rep->re_nextp) != rep1);
		}
	fprintf(logfile,"  %hu entries found.\n",size);
	}
#endif

// Save a pattern to a ring if not null.  If pattern matches an existing entry, move old entry to the top (without cycling);
// otherwise, create an entry.  Return status.
int rsetpat(char *pat,Ring *ringp) {

	if(*pat != '\0') {
		if(ringp->r_size > 0) {
			RingEntry *rep1 = ringp->r_entryp;
			RingEntry *rep = rep1;

			// Pattern already in ring?
			do {
				if(strcmp(rep->re_data.d_str,pat) == 0) {

					// Found it.  Move it to the top if not already there.
					if(rep != rep1) {

						// Ring contains at least two entries.  If only two, just change the header
						// pointer; otherwise, remove matched entry from its current slot and insert it
						// at the top.
						ringp->r_entryp = rep;
						if(ringp->r_size > 2) {
							rep->re_nextp->re_prevp = rep->re_prevp;
							rep->re_prevp->re_nextp = rep->re_nextp;

							rep->re_nextp = rep1->re_nextp;
							rep->re_prevp = rep1;
							rep1->re_nextp = rep1->re_nextp->re_prevp = rep;
							}
						}
					return rc.status;
					}
				} while((rep = rep->re_nextp) != rep1);
			}

		// Pattern not found... add it.
		if(rnew(ringp) == Success && dsetstr(pat,&ringp->r_entryp->re_data) != 0)
			(void) librcset(Failure);
		}
	return rc.status;
	}

// Get ring entry specified by n and return pointer to it.  If not found, set an error and return NULL.
RingEntry *rget(Ring *ringp,int n) {
	RingEntry *rep;

	// n in range?
	if(ringp->r_size == 0) {
		(void) rcset(Failure,0,text202,ringp->r_name);
				// "%s ring is empty"
		return NULL;
		}
	if(n > 0 || n <= -ringp->r_size) {
		(void) rcset(Failure,0,text19,n,-((int) ringp->r_size - 1));
				// "No such ring entry %d (max %d)"
		return NULL;
		}

	// n is valid.  Find entry #n in ring...
	for(rep = ringp->r_entryp; n < 0; ++n)
		rep = rep->re_prevp;

	// and return it.
	return rep;
	}

// Attempt to delete n entries from given ring, beginning at top.  It is assumed that ringp->r_entryp != NULL and n > 0.  Return
// number of entries actually deleted.
static ushort rtrim(Ring *ringp,int n) {
	RingEntry *rep;
	RingEntry *repz = ringp->r_entryp->re_nextp;
	ushort sz0 = ringp->r_size;

	do {
		rep = ringp->r_entryp;
		ringp->r_entryp = rep->re_prevp;
		dclear(&rep->re_data);
		free((void *) rep);
		} while(--ringp->r_size > 0 && --n > 0);
	if(ringp->r_size == 0)
		ringp->r_entryp = NULL;
	else {
		ringp->r_entryp->re_nextp = repz;
		repz->re_prevp = ringp->r_entryp;
		}

	return sz0 - ringp->r_size;
	}

// Delete the most recent n entries (default 1) from given ring, kill number n if n < 0, or all entries if n == 0.  Return
// status.
int rdelete(Ring *ringp,int n) {

	if(ringp->r_size == 0)
		return rcset(Failure,0,text202,ringp->r_name);
				// "%s ring is empty"
	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		n = ringp->r_size;
	else if(n < 0) {
		RingEntry *rep = rget(ringp,n);
		if(rep == NULL)
			return rc.status;

		// Delete "rep" entry, which is not at top.  Kill ring contains at least two entries.
		rep->re_nextp->re_prevp = rep->re_prevp;
		rep->re_prevp->re_nextp = rep->re_nextp;
		dclear(&rep->re_data);
		free((void *) rep);
		--ringp->r_size;
		n = 1;
		goto NukeMsg;
		}

	// Delete the most recent n entries.
	int sz0 = ringp->r_size;
	if((n = rtrim(ringp,n)) == sz0)
		(void) rcset(Success,0,text228,ringp->r_name);
				// "%s ring cleared"
	else
NukeMsg:
		(void) rcset(Success,0,text357,ringp->r_name,ringp->r_ename,n > 1 ? "s" : "");
				// "%s%s%s deleted"

	return rc.status;
	}

// Insert, overwrite, or replace a character at the current point.  Return status.
static int iorch(short c,ushort t) {

	// Newline?
	if(c == '\n' && !(t & Txt_LiteralRtn))
		return lnewline();

	// Not a newline.  Overwrite or replace and not at end of line?
	Dot *dotp = &si.curwp->w_face.wf_dot;
	if(!(t & Txt_Insert) && dotp->off < dotp->lnp->l_used) {

		// Yes.  Replace mode, not a tab, or at a tab stop?
		if((t & Txt_Replace) || dotp->lnp->l_text[dotp->off] != '\t' ||
		 getccol(NULL) % si.htabsize == (si.htabsize - 1)) {

			// Yes.  Delete before insert.
			if(ldelete(1L,0) != Success)
				return rc.status;
			}
		}

	// Insert the character! (amen).
	return linsert(1,c);
	}

// Insert, overwrite, or replace a string from a Datum object (srcp != NULL) or the kill or undelete buffer (srcp == NULL) at
// point n times.  In the latter case, use kill ring if "kill" is true and mark text as a region; otherwise, use the undelete
// buffer.  If n == 0, do one repetition and leave point before the new text; otherwise, after.  If n < 0 and srcp != NULL,
// treat newline characters literally.  If n < 0, srcp == NULL, and kill is true, get the -nth kill instead of the most recent
// one without cycling the kill ring.  Update kill state if srcp is NULL.  Return status.
int iorstr(Datum *srcp,int n,ushort style,bool kill) {
	long size;
	int reps;
	char *str;		// Pointer into string to insert.
	Line *curline;
	int curoff;
	RingEntry *rep;
	bool pretext;		// Leave point before inserted text?
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Check if we have something to insert.  Not an error if no text.
	if(srcp == NULL) {

		// Which kill?
		if(!kill) {
			rep = &undelbuf;
			if(rep->re_data.d_type == dat_nil)
				return rc.status;
			if(n != 0)
				n = 1;
			}
		else if(kring.r_size == 0)
			return rcset(Failure,0,text202,kring.r_name);
					// "%s ring is empty"
		else if(n >= 0)
			rep = kring.r_entryp;
		else {
			if((rep = rget(&kring,n)) == NULL)
				return rc.status;
			n = 1;
			}

		if(disnn(&rep->re_data)) {
			if(kill)
				lastysize = 0;
			return rc.status;
			}

		// Save dot in RegMark if a kill.
		if(kill)
			mset(&si.curbp->b_mroot,si.curwp);
		}
	else if(disnull(srcp))
		return rc.status;

	// If n == 0, remember point.  However, must save the *previous* line (if possible), since the line we are on may
	// disappear due to re-allocation.
	if((pretext = n == 0)) {
		curline = (dotp->lnp == si.curbp->b_lnp) ? NULL : dotp->lnp->l_prevp;
		curoff = dotp->off;
		n = 1;
		}

	// For "reps" times...
	if(n < 0) {
		reps = 1;
		style |= Txt_LiteralRtn;
		}
	else
		reps = n;
	while(reps-- > 0) {
		size = 0;
		str = (srcp == NULL) ? rep->re_data.d_str : srcp->d_str;
		do {
			if(iorch(*str++,style) != Success)
				return rc.status;
			++size;
			} while(*str != '\0');
		}

	// If requested, set point back to the beginning of the new text and move RegMark.
	if(pretext) {
		if(kill)
			mset(&si.curbp->b_mroot,si.curwp);
		dotp->lnp = (curline == NULL) ? si.curbp->b_lnp : curline->l_nextp;
		dotp->off = curoff;
		}

	// Update kill state.
	if(srcp == NULL && kill) {
		kentry.thisflag |= (pretext ? (CFYank | CFNMove) : CFYank);
		lastysize = size;
		}

	return rc.status;
	}

// Yank text, replacing the text from the last yank if not the first invocation.
int yankCycle(Datum *rp,int n,Datum **argpp) {

	// Cycle the kill ring per n.
	if(rcycle(&kring,n,false) != Success)
		return rc.status;

	// If not the first consecutive time, delete the last yank...
	if(kentry.lastflag & CFYank)
		if(ldelete(kentry.lastflag & CFNMove ? lastysize : -lastysize,0) != Success || n == 0)
			return rc.status;

	// and insert the current kill entry.
	return execCF(rp,1,cftab + cf_yank,0,0);
	}

// Build and pop up a buffer containing all the strings in given ring.  Render buffer and return status.
int showRing(Datum *rp,int n,Ring *ringp) {
	Buffer *bufp;
	size_t n1;
	DStrFab rpt;
	char wkbuf[16];

	// Get buffer for the ring list.
	sprintf(wkbuf,text305,ringp->r_name);
			// "%sRing"
	if(sysbuf(wkbuf,&bufp,BFTermAttr) != Success)
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;

	// Construct the header lines.
	mkupper(wkbuf,ringp->r_name);
	if(dputf(&rpt,text311,wkbuf) != 0 || dputs(text330,&rpt) != 0 || dputs("-----  ",&rpt) != 0)
			// "%s RING\n\n","~bEntry  Text~0\n"
		goto LibFail;
	n1 = term.t_ncol - 7;
	do {
		if(dputc('-',&rpt) != 0)
			goto LibFail;
		} while(--n1 > 0);

	// Loop through ring, beginning at current slot and continuing until we arrive back where we began.
	if(ringp->r_entryp != NULL) {
		RingEntry *rep;
		size_t max = term.t_ncol * 2;
		int inum = 0;
		char wkbuf[12];

		rep = ringp->r_entryp;
		do {
			sprintf(wkbuf,"\n%4d   ",inum--);
			if(dputs(wkbuf,&rpt) != 0)
				goto LibFail;
			if(rep->re_data.d_type != dat_nil && (n1 = strlen(rep->re_data.d_str)) > 0) {
				if(n1 > max)
					n1 = max;
				if(dvizs(rep->re_data.d_str,n1,VBaseDef,&rpt) != 0)
					goto LibFail;
				}

			// Back up to the next kill ring entry.
			} while((rep = rep->re_prevp) != ringp->r_entryp);
		}

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		goto LibFail;
	if(bappend(bufp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n,bufp,RendNewBuf | RendReset);
LibFail:
	return librcset(Failure);
	}

// Build and pop up a buffer containing all the strings in the kill ring.  Render buffer and return status.
int showKillRing(Datum *rp,int n,Datum **argpp) {

	return showRing(rp,n,&kring);
	}
