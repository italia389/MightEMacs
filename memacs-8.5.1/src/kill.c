// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// kill.c	Kill buffer functions for MightEMacs.
//
// These routines manage the kill ring and undelete buffer.

#include "os.h"
#include "std.h"
#include "lang.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "main.h"

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
		kp->kused = KBlock;
		}
	}

// Insert a character into the given kill buffer, given direction (Forward or Backward).  Allocate new chunks as needed.
// Return status.
int kinsert(Kill *kp,int direct,int c) {
	KillBuf *kbp;
	static char myname[] = "kinsert";

	if(direct == Forward) {

		// Check to see if we need a new chunk...
		if(kp->kused >= KBlock) {
			if((kbp = (KillBuf *) malloc(sizeof(KillBuf))) == NULL)
				return rcset(Panic,0,text94,myname);
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
	else {	// Backward.

		// Check to see if we need a new chunk...
		if(kp->kskip == 0) {
			if((kbp = (KillBuf *) malloc(sizeof(KillBuf))) == NULL)
				return rcset(Panic,0,text94,myname);
						// "%s(): Out of memory!"
			if(kp->kbufh == NULL) {		// Set head pointer if first insert.
				kp->kbufh = kp->kbufp = kbp;
				kp->kskip = kp->kused = KBlock;
				kbp->kl_next = NULL;
				}
			else {
				kbp->kl_next = kp->kbufh;
				kp->kbufh = kbp;
				kp->kskip = KBlock;
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
	uint flag = kill ? CFKill : CFDel;

	if(!(kentry.lastflag & flag))				// If last command was not in "kill" or "delete" family...
		flag == CFKill ? kcycle() : kdelete(&undelbuf);	// cycle kill ring or reset undelete buffer and start fresh
								// (don't append this kill or delete to the last one).
	kentry.thisflag |= flag;				// Mark this command as a "kill" or "delete".
	}

// Insert, overwrite, or replace a character at the current point.  Return status.
static int iorch(int c,uint t) {

	// Newline?
	if(c == '\n' && !(t & Txt_LitRtn))
		return lnewline();

	// Not a newline.  Overwrite or replace and not at end of line?
	Dot *dotp = &curwp->w_face.wf_dot;
	if(!(t & Txt_Insert) && dotp->off < dotp->lnp->l_used) {

		// Yes.  Replace mode, not a tab, or at a tab stop?
		if((t & Txt_Replace) || lgetc(dotp->lnp,dotp->off) != '\t' || getccol() % htabsize == (htabsize - 1)) {

			// Yes.  Delete before insert.
			if(ldelete(1L,0) != Success)
				return rc.status;
			}
		}

	// Insert the character! (amen).
	return linsert(1,c);
	}

// Insert, overwrite, or replace text from a string (srcp != NULL) or the kill or undelete buffer (srcp == NULL) n times.  In
// the latter case, use kill buffer if "kill" is true; otherwise, the undelete buffer.  If n == 0, do one repetition and leave
// point before the new text; otherwise, after.  If n < 0 and srcp != NULL, treat newline characters literally.  If n < 0, srcp
// == NULL, and kill is true, get the -nth kill instead of the most recent one without cycling the kill ring.  Update kill state
// if srcp is NULL.  Return status.
uint iortext(Datum *srcp,int n,uint t,bool kill) {
	long size;
	int reps;
	int counter;		// Counter into kill buffer data.
	char *str;		// Pointer into string to insert.
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
		else if(n <= -NRing)
			return rcset(Failure,0,text19,n,-(NRing - 1));
					// "No such kill %d (max %d)"
		else {
			if((kp = kringp + n) < kring)
				kp += NRing;
			n = 1;
			}
		if(kp->kbufh == NULL) {
			if(kill)
				lastysize = 0;
			return rc.status;
			}
		}
	else if(disnull(srcp))
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
		t |= Txt_LitRtn;
		}
	else
		reps = n;
	while(reps-- > 0) {
		size = 0;
		if(srcp != NULL) {
			str = srcp->d_str;
			do {
				if(iorch(*str++,t) != Success)
					return rc.status;
				++size;
				} while(*str != '\0');
			}
		else {
			kbp = kp->kbufh;
			if(kp->kskip > 0) {
				str = kbp->kl_chunk + kp->kskip;
				counter = kp->kskip;
				size += KBlock - counter;
				while(counter++ < KBlock) {
					if(iorch(*str,t) != Success)
						return rc.status;
					++str;
					}
				kbp = kbp->kl_next;
				}

			if(kbp != NULL) {
				while(kbp != kp->kbufp) {
					str = kbp->kl_chunk;
					size += KBlock;
					for(counter = 0; counter < KBlock; ++counter) {
						if(iorch(*str,t) != Success)
							return rc.status;
						++str;
						}
					kbp = kbp->kl_next;
					}
				counter = kp->kused;
				str = kbp->kl_chunk;
				size += counter;
				while(counter--) {
					if(iorch(*str,t) != Success)
						return rc.status;
					++str;
					}
				}
			}
		}

	// Delete last character if it was a newline, not solo, and we started at the end of the buffer.
	if(eob && size > 1 && str[-1] == '\n') {
		if(ldelete(-1L,0) != Success)
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
		kentry.thisflag |= (pretext ? (CFYank | CFNMove) : CFYank);
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
		goto NotNeg;
		}
	if(n < 0) {
		i = 1;
		limitp = kringz - 1;
		resetp = kring;
		}
	else {
NotNeg:
		i = -1;
		limitp = kring;
		resetp = kringz - 1;
		}

	while(n != 0) {

		// Skip unused slots.
		origp = kringp;
		do {
			kringp = (kringp == limitp) ? resetp : (kringp + i);
			} while(kringp != origp && kringp->kbufh == NULL);
		n += i;
		}

	return msg ? rcset(Success,RCNoFormat,text42) : rc.status;
			// "Kill ring cycled"
	}

// Yank text, replacing the text from the last yank if not the first invocation.
int yankPop(Datum *rp,int n,Datum **argpp) {

	// Cycle the kill ring appropriately.
	if(cycle_ring(n,false) != Success)
		return rc.status;

	// If not the first consecutive time, delete the last yank...
	if(kentry.lastflag & CFYank)
		if(ldelete(kentry.lastflag & CFNMove ? lastysize : -lastysize,0) != Success)
			return rc.status;

	// and insert the current kill buffer.
	return execCF(rp,kentry.lastflag & CFNMove ? -1 : 1,cftab + cf_yank,0,0);
	}

// Build and pop up a buffer containing all the strings in the kill ring.  Render buffer and return status.
int showKillRing(Datum *rp,int n,Datum **argpp) {
	int knum;
	Buffer *bufp;
	Kill *kp0,*kp;
	KillBuf *kbp;
	DStrFab rpt;
	size_t n1,n2;
	size_t max = term.t_ncol * 2;
	char wkbuf[12];

	// Get buffer for the kill ring list.
	if(sysbuf(text305,&bufp) != Success)
			// "KillList"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	// Construct the header lines.
	if(dputs(text330,&rpt) != 0 || dputc('\n',&rpt) != 0 || dputs("----  ",&rpt) != 0)
			// "Kill  Text"
		return drcset();
	n1 = term.t_ncol - 6;
	do {
		if(dputc('-',&rpt) != 0)
			return drcset();
		} while(--n1 > 0);

	// Loop through kill ring, beginning at current slot and continuing until we arrive back where we began.
	knum = 0;
	kp0 = kp = kringp;
	do {
		sprintf(wkbuf,"\n%3d   ",knum--);
		if(dputs(wkbuf,&rpt) != 0)
			return drcset();
		n1 = 6;
		kbp = kp->kbufh;
		if(kp->kskip > 0) {
			n2 = KBlock - kp->kskip;
			if(dvizs(kbp->kl_chunk + kp->kskip,n2,VBaseDef,&rpt) != 0)
				return drcset();
			if((n1 += n2) >= max)
				goto Done;
			kbp = kbp->kl_next;
			}

		if(kbp != NULL) {
			while(kbp != kp->kbufp) {
				if(dvizs(kbp->kl_chunk,KBlock,VBaseDef,&rpt) != 0)
					return drcset();
				if((n1 += KBlock) >= max)
					goto Done;
				kbp = kbp->kl_next;
				}
			if(dvizs(kbp->kl_chunk,kp->kused,VBaseDef,&rpt) != 0)
				return drcset();
			n1 += kp->kused;
			}
Done:
		// Back up to the next kill ring entry.
		if(kp-- == kring)
			kp = kringz - 1;
		} while(kp != kp0);

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(bappend(bufp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,bufp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}

#if MMDebug & Debug_KillRing
// Display the kill ring (to line-oriented terminal) -- DEBUG ROUTINE.
void dispkill(void) {
	KillBuf *kbp;
	char *str;
	int counter;

	if(kringp->kbufh == NULL) {
		fputs("<EMPTY>\n",stderr);
		return;
		}

	if(kringp->kskip > 0) {
		kbp = kringp->kbufh;
		fprintf(stderr,"kringp->kskip = %d\n",kringp->kskip);
		fprintf(stderr,"BLOCK %d <",(int) (kringz - kringp++));
		str = kbp->kl_chunk + kringp->kskip;
		counter = kringp->kskip;
		while(counter++ < KBlock) {
			putc(*str++,stderr);
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
		str = kbp->kl_chunk;
		while(counter--) {
			putc(*str++,stderr);
			}
		fprintf(stderr,">\nkringp->kused = %d\n",kringp->kused);
		}
	}
#endif
