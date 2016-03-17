// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// kill.c	Kill buffer functions for MightEMacs.
//
// These routines manage the kill ring and undelete buffer.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"

static long lastysize = -1;	// Last number of bytes yanked (used by yankPop to delete prior yank).

// Delete all of the text saved in the given kill buffer.  Called when a new kill context is being created.  No errors possible.
void kdelete(Kill *kp) {

	if(kp->kbufh != NULL) {
		KillBuf *kbp1,*kbp2;

		kbp1 = kp->kbufh;

		// First, delete all the chunks...
		do {
			kbp2 = kbp1->kl_next;
			free((void *) kbp1);
			} while((kbp1 = kbp2) != NULL);

		// and now reset all the kill buffer pointers.
		kp->kbufh = kp->kbufp = NULL;
		kp->kskip = 0;
		kp->kused = KBLOCK;
		}
	}

// Insert a character into the given kill buffer, given direction (FORWARD or BACKWARD).  Allocate new chunks as needed.
// Return status.
int kinsert(Kill *kp,int direct,int c) {
	KillBuf *kbp;
	static char myname[] = "kinsert";

	if(direct == FORWARD) {

		// Check to see if we need a new chunk...
		if(kp->kused >= KBLOCK) {
			if((kbp = (KillBuf *) malloc(sizeof(KillBuf))) == NULL)
				return rcset(PANIC,0,text94,myname);
						// "%s(): Out of memory!"
			if(kp->kbufh == NULL)		// Set head pointer if first insert.
				kp->kbufh = kp->kbufp = kbp;
			else
				kp->kbufp = kp->kbufp->kl_next = kbp;
			kbp->kl_next = NULL;
			kp->kused = 0;
			}

		// and now insert the character.
		kp->kbufp->kl_chunk[kp->kused++] = c;
		}
	else {	// BACKWARD.

		// Check to see if we need a new chunk...
		if(kp->kskip == 0) {
			if((kbp = (KillBuf *) malloc(sizeof(KillBuf))) == NULL)
				return rcset(PANIC,0,text94,myname);
						// "%s(): Out of memory!"
			if(kp->kbufh == NULL) {		// Set head pointer if first insert.
				kp->kbufh = kp->kbufp = kbp;
				kp->kskip = kp->kused = KBLOCK;
				kbp->kl_next = NULL;
				}
			else {
				kbp->kl_next = kp->kbufh;
				kp->kbufh = kbp;
				kp->kskip = KBLOCK;
				}
			}

		// and now insert the character.
		kp->kbufh->kl_chunk[--kp->kskip] = c;
		}

	return rc.status;
	}

// Advance to the next position in the kill ring, pushing the current kill buffer and clearing what will be the new kill
// buffer.
void kcycle(void) {

	// Advance to the next kill ring entry...
	if(++kringp == kringz)
		kringp = kring;

	// and clear it, so it is ready for use.
	kdelete(kringp);
	}

// Prepare for new kill or delete.
void kprep(bool kill) {
	uint flag = kill ? CFKILL : CFDEL;

	if(!(kentry.lastflag & flag))				// If last command was not in "kill" or "delete" family...
		flag == CFKILL ? kcycle() : kdelete(&undelbuf);	// cycle kill ring or reset undelete buffer and start fresh
								// (don't append this kill or delete to the last one).
	kentry.thisflag |= flag;				// Mark this command as a "kill" or "delete".
	}

// Insert, overwrite, or replace a character at the current point.  Return status.
static int iorch(int c,uint t) {

	// Newline?
	if(c == '\r' && !(t & TXT_LITCR))
		return lnewline();

	// Not a newline.  Overwrite or replace and not at end of line?
	Dot *dotp = &curwp->w_face.wf_dot;
	if(!(t & TXT_INSERT) && dotp->off < dotp->lnp->l_used) {

		// Yes.  Replace mode, not a tab, or at a tab stop?
		if((t & TXT_REPLACE) || lgetc(dotp->lnp,dotp->off) != '\t' || getccol() % htabsize == (htabsize - 1)) {

			// Yes.  Delete before insert.
			if(ldelete(1L,0) != SUCCESS)
				return rc.status;
			}
		}

	// Insert the character! (amen).
	return linsert(1,c);
	}

// Insert, overwrite, or replace text from a string (srcp != NULL) or the kill or undelete buffer (srcp == NULL) n times.  In
// latter case, use kill buffer if "kill" is true; otherwise, the undelete buffer.  If n == 0, do one repetition and leave point
// before the new text; otherwise, after.  If n < 0 and srcp != NULL, treat CR characters literally.  If n < 0, srcp == NULL,
// and kill is true, get the -nth kill instead of the most recent one without cycling the kill ring.  Update kill state if srcp
// is NULL.  Return status.
int iortext(Value *srcp,int n,uint t,bool kill) {
	long size;
	int reps;
	int counter;		// Counter into kill buffer data.
	char *strp;		// Pointer into string to insert.
	Line *curline;
	int curoff;
	bool eob;		// true if starting at end of buffer.
	KillBuf *kbp;		// Pointer into kill buffer.
	Kill *kp;
	bool pretext;		// Leave point before inserted text?
	Dot *dotp = &curwp->w_face.wf_dot;

	// Check if we have something to insert.  Not an error if no text.
	if(srcp == NULL) {

		// Which kill?
		if(!kill) {
			kp = &undelbuf;
			if(n != 0)
				n = 1;
			}
		else if(n >= 0)
			kp = kringp;
		else if(n <= -NRING)
			return rcset(FAILURE,0,text19,n,-(NRING - 1));
					// "No such kill %d (max %d)"
		else {
			if((kp = kringp + n) < kring)
				kp += NRING;
			n = 1;
			}
		if(kp->kbufh == NULL) {
			if(kill)
				lastysize = 0;
			return rc.status;
			}
		}
	else if(visnull(srcp))
		return rc.status;

	eob = (dotp->lnp == curbp->b_hdrlnp);

	// If zero argument, find the *previous* line, since the line we are on may disappear due to re-allocation.  This
	// works even if we are on the first line of the file.
	if((pretext = n == 0)) {
		curline = lback(dotp->lnp);
		curoff = dotp->off;
		n = 1;
		}

	// For "reps" times...
	if(n < 0) {
		reps = 1;
		t |= TXT_LITCR;
		}
	else
		reps = n;
	while(reps-- > 0) {
		size = 0;
		if(srcp != NULL) {
			strp = srcp->v_strp;
			do {
				if(iorch(*strp++,t) != SUCCESS)
					return rc.status;
				++size;
				} while(*strp != '\0');
			}
		else {
			kbp = kp->kbufh;
			if(kp->kskip > 0) {
				strp = kbp->kl_chunk + kp->kskip;
				counter = kp->kskip;
				size += KBLOCK - counter;
				while(counter++ < KBLOCK) {
					if(iorch(*strp,t) != SUCCESS)
						return rc.status;
					++strp;
					}
				kbp = kbp->kl_next;
				}

			if(kbp != NULL) {
				while(kbp != kp->kbufp) {
					strp = kbp->kl_chunk;
					size += KBLOCK;
					for(counter = 0; counter < KBLOCK; ++counter) {
						if(iorch(*strp,t) != SUCCESS)
							return rc.status;
						++strp;
						}
					kbp = kbp->kl_next;
					}
				counter = kp->kused;
				strp = kbp->kl_chunk;
				size += counter;
				while(counter--) {
					if(iorch(*strp,t) != SUCCESS)
						return rc.status;
					++strp;
					}
				}
			}
		}

	// Delete last character if it was a newline, not solo, and we started at the end of the buffer.
	if(eob && size > 1 && strp[-1] == '\r') {
		if(ldelete(-1L,0) != SUCCESS)
			return rc.status;
		dotp->lnp = lforw(dotp->lnp);
		dotp->off = 0;
		}

	// If requested, set point back to the beginning of the new text.
	if(pretext) {
		dotp->lnp = lforw(curline);
		dotp->off = curoff;
		}

	// Update kill state.
	if(srcp == NULL && kill) {
		kentry.thisflag |= (pretext ? (CFYANK | CFNMOV) : CFYANK);
		lastysize = size;
		}

	return rc.status;
	}

// Cycle the kill ring forward or backward.
int cycle_ring(int n,bool msg) {
	int i;
	Kill *origp,*limitp,*resetp;

	// Cycle the kill index forward or backward.
	if(n == INT_MIN) {
		n = 1;
		goto notneg;
		}
	if(n < 0) {
		i = 1;
		limitp = kringz - 1;
		resetp = kring;
		}
	else {
notneg:
		i = -1;
		limitp = kring;
		resetp = kringz - 1;
		}

	while(n != 0) {

		// Skip unused slots.
		origp = kringp;
		do {
			kringp = (kringp == limitp) ? resetp : (kringp += i);
			} while(kringp != origp && kringp->kbufh == NULL);
		n += i;
		}

	return msg ? rcset(SUCCESS,0,text42) : rc.status;
			// "Kill ring cycled"
	}

// Yank text, replacing the text from the last yank if not the first invocation.
int yankPop(Value *rp,int n) {

	// Cycle the kill ring appropriately.
	if(cycle_ring(n,false) != SUCCESS)
		return rc.status;

	// If not the first consecutive time, delete the last yank...
	if(kentry.lastflag & CFYANK)
		if(ldelete(kentry.lastflag & CFNMOV ? lastysize : -lastysize,0) != SUCCESS)
			return rc.status;

	// and insert the current kill buffer.
	return feval(rp,kentry.lastflag & CFNMOV ? -1 : 1,cftab + cf_yank);
	}

// Build and pop up a buffer containing all the strings in the kill ring.  Render buffer and return status.
int showKillRing(Value *rp,int n) {
	int knum;
	Buffer *bufp;
	Kill *kp0,*kp;
	KillBuf *kbp;
	StrList rpt;
	size_t n1,n2;
	size_t max = term.t_ncol * 2;
	char wkbuf[12];

	// Get buffer for the kill ring list.
	if(sysbuf(text305,&bufp) != SUCCESS)
			// "KillList"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	// Construct the header lines.
	if(vputs(text330,&rpt) != 0 || vputc('\r',&rpt) != 0 || vputs("----  ",&rpt) != 0)
			// "Kill  Text"
		return vrcset();
	n1 = term.t_ncol - 6;
	do {
		if(vputc('-',&rpt) != 0)
			return vrcset();
		} while(--n1 > 0);

	// Loop through kill ring, beginning at current slot and continuing until we arrive back where we began.
	knum = 0;
	kp0 = kp = kringp;
	do {
		sprintf(wkbuf,"\r%3d   ",knum--);
		if(vputs(wkbuf,&rpt) != 0)
			return vrcset();
		n1 = 6;
		kbp = kp->kbufh;
		if(kp->kskip > 0) {
			n2 = KBLOCK - kp->kskip;
			if(vstrlit(&rpt,kbp->kl_chunk + kp->kskip,n2) != 0)
				return vrcset();
			if((n1 += n2) >= max)
				goto done;
			kbp = kbp->kl_next;
			}

		if(kbp != NULL) {
			while(kbp != kp->kbufp) {
				if(vstrlit(&rpt,kbp->kl_chunk,KBLOCK) != 0)
					return vrcset();
				if((n1 += KBLOCK) >= max)
					goto done;
				kbp = kbp->kl_next;
				}
			if(vstrlit(&rpt,kbp->kl_chunk,kp->kused) != 0)
				return vrcset();
			n1 += kp->kused;
			}
done:
		// Back up to the next kill ring entry.
		if(kp-- == kring)
			kp = kringz - 1;
		} while(kp != kp0);

	// Add the report to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(bappend(bufp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,bufp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}

#if MMDEBUG & DEBUG_KILLRING
// Display the kill ring (to line-oriented terminal) -- DEBUG ROUTINE.
void dispkill(void) {
	KillBuf *kbp;
	char *strp;
	int counter;

	if(kringp->kbufh == NULL) {
		fputs("<EMPTY>\n",stderr);
		return;
		}

	if(kringp->kskip > 0) {
		kbp = kringp->kbufh;
		fprintf(stderr,"kringp->kskip = %d\n",kringp->kskip);
		fprintf(stderr,"BLOCK %d <",(int) (kringz - kringp++));
		strp = kbp->kl_chunk + kringp->kskip;
		counter = kringp->kskip;
		while(counter++ < KBLOCK) {
			putc(*strp++,stderr);
			}
		fputs(">\n",stderr);
		kbp = kbp->kl_next;
		}
	else
		kbp = kringp->kbufh;

	if(kbp != NULL) {
		while(kbp != kringp->kbufp) {
			fprintf(stderr,"BLOCK %d <%255s>\n",(int) (kringz - kringp++),kbp->kl_chunk);
			kbp = kbp->kl_next;
			}
		fprintf(stderr,"BLOCK %d <",(int) (kringz - kringp++));
		counter = kringp->kused;
		strp = kbp->kl_chunk;
		while(counter--) {
			putc(*strp++,stderr);
			}
		fprintf(stderr,">\nkringp->kused = %d\n",kringp->kused);
		}
	}
#endif
