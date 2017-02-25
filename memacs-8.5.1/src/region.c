// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// region.c	Region-related functions for MightEMacs.
//
// These routines deal with the region, that magic space between "." and mark.  Some functions are commands and some are just
// for internal use.

#include "os.h"
#include "std.h"
#include "lang.h"
#include "bind.h"
#include "cmd.h"
#include "main.h"

// Set *np to the number of lines in the current region and *allp (if allp not NULL) to true if region includes all lines in
// the buffer.  Place dot at beginning of region and return status.
int reglines(int *np,bool *allp) {
	Line *lnp;
	int n;
	Region region;

	// Check for a valid region first.
	if(getregion(&region,allp) != Success)
		return rc.status;

	// Start at the top of the region.
	lnp = region.r_dot.lnp;
	region.r_size += region.r_dot.off;
	n = 0;

	// Scan the region, counting lines.
	while(region.r_size >= 0) {
		region.r_size -= lused(lnp) + 1;
		lnp = lforw(lnp);
		++n;
		}

	// Place point at the beginning of the region and return result.
	curwp->w_face.wf_dot = region.r_dot;
	*np = n;
	return rc.status;
	}

// Delete or kill a region.  Ask "getregion" to figure out the bounds of the region.  Move dot to the start, and delete or
// kill the characters, depending on "kill" flag.
int dkregion(int n,bool kill) {
	Region region;

	if(getregion(&region,NULL) != Success)
		return rc.status;
	kprep(kill);
	curwp->w_face.wf_dot = region.r_dot;
	return ldelete(region.r_size,kill ? DFKill : DFDel);
	}

// Copy all of the characters in the given region to the kill ring without moving dot.  Return status.
int copyreg(Region *regp) {
	Line *lnp;
	int offset;

	kprep(1);
	lnp = regp->r_dot.lnp;			// Current line.
	offset = regp->r_dot.off;		// Current offset.
	while(regp->r_size-- > 0) {
		if(offset == lused(lnp)) {	// End of line.
			if(kinsert(kringp,Forward,'\n') != Success)
				return rc.status;
			lnp = lforw(lnp);
			offset = 0;
			}
		else {				// Beginning or middle of line.
			if(kinsert(kringp,Forward,lgetc(lnp,offset)) != Success)
				return rc.status;
			++offset;
			}
		}

	return rc.status;
	}

// Lower or upper case region.  Change the case of all the characters in the region.  Use the region code to set the
// limits.  Scan the buffer, doing the changes.  Call "lchange" to ensure that redisplay is done in all buffers.
int caseregion(int n,char *trantab) {
	Line *lnp;
	int offset;
	Region region;

	if(getregion(&region,NULL) != Success)
		return rc.status;
	lchange(curbp,WFHard);
	lnp = region.r_dot.lnp;
	offset = region.r_dot.off;
	while(region.r_size-- > 0) {
		if(offset == lused(lnp)) {
			lnp = lforw(lnp);
			offset = 0;
			}
		else {
			lputc(lnp,offset,trantab[(int) lgetc(lnp,offset)]);
			++offset;
			}
		}

	return rc.status;
	}

// This routine figures out the bounds of the region in the current window and fills in the fields of the Region structure
// pointed to by "regp".  Because the dot and mark are usually very close together, we scan outward from dot looking for mark.
// This should save time.  Set *wholebufp (if pointer not NULL) to true if region extends from first line of buffer to or past
// last line; otherwise, false.  Return status.
int getregion(Region *regp,bool *wholebufp) {
	Line *flp,*blp;
	Dot *dotp;
	long fsize,bsize;
	WindFace *wfp = &curwp->w_face;

	dotp = &curbp->b_mroot.mk_dot;
	if(dotp->off < 0)
		return rcset(Failure,0,text11,RMark);
			// "No mark '%c' in this buffer",

	// Do special case where mark 0 is on current line.
	if(dotp->lnp == wfp->wf_dot.lnp) {
		regp->r_dot.lnp = wfp->wf_dot.lnp;
		if(dotp->off > wfp->wf_dot.off) {
			regp->r_dot.off = wfp->wf_dot.off;
			regp->r_size = (long) (dotp->off - wfp->wf_dot.off);
			}
		else {
			regp->r_size = (long) (wfp->wf_dot.off - dotp->off);
			regp->r_dot.off = dotp->off;
			}
		if(wholebufp != NULL)
			*wholebufp = false;
		return rc.status;
		}

	// Do general case; begin hunt forward and backward looking for mark 0.
	blp = wfp->wf_dot.lnp;
	bsize = (long) wfp->wf_dot.off;
	flp = wfp->wf_dot.lnp;
	fsize = (long) (lused(flp) - wfp->wf_dot.off + 1);
	while(flp != curbp->b_hdrlnp || lback(blp) != curbp->b_hdrlnp) {
		if(flp != curbp->b_hdrlnp) {
			flp = lforw(flp);
			if(flp == dotp->lnp) {
				regp->r_dot = wfp->wf_dot;
				regp->r_size = fsize + dotp->off;
				if(wholebufp != NULL)
					*wholebufp = (lback(wfp->wf_dot.lnp) == curbp->b_hdrlnp && (flp == curbp->b_hdrlnp ||
					 lforw(flp) == curbp->b_hdrlnp));
				return rc.status;
				}
			fsize += lused(flp) + 1;
			}
		if(lback(blp) != curbp->b_hdrlnp) {
			blp = lback(blp);
			bsize += lused(blp) + 1;
			if(blp == dotp->lnp) {
				regp->r_dot.lnp = blp;
				regp->r_dot.off = dotp->off;
				regp->r_size = bsize - dotp->off;
				if(wholebufp != NULL)
					*wholebufp = (lback(blp) == curbp->b_hdrlnp && (wfp->wf_dot.lnp == curbp->b_hdrlnp ||
					 lforw(wfp->wf_dot.lnp) == curbp->b_hdrlnp));
				return rc.status;
				}
			}
		}

	// Huh?  Didn't find mark RMark!  This is a bug.
	return rcset(FatalError,0,text77,"getregion",RMark,curbp->b_bname);
		// "%s() bug: Mark '%c' not found in buffer '%s'!"
	}

// Copy all of the characters in the region to the given buffer and return it.  It is assumed that the buffer size is at least
// region size + 1.
char *regcpy(char *buf,Region *regp) {
	Line *lnp;
	int offset;
	long rsize,len;
	char *dest;

	dest = buf;
	lnp = regp->r_dot.lnp;			// Current line.
	offset = regp->r_dot.off;		// Current offset.
	rsize = regp->r_size;

	while(rsize > 0) {
		if((len = lused(lnp) - offset) == 0) {	// End of line.
			*dest++ = '\n';
			lnp = lforw(lnp);
			--rsize;
			offset = 0;
			}
		else {					// Beginning or middle of line.
			if(len > rsize)
				len = rsize;
			dest = (char *) memzcpy((void *) dest,(void *) (ltext(lnp) + offset),len);
			offset += len;
			rsize -= len;
			}
		}
	*dest = '\0';
	return buf;
	}

#if 0
// Copy the contents of the given region to dest.  Return status.
int regdatcpy(Datum *destp,Region *regp) {

	// Preallocate a string and copy.
	if(dsalloc(destp,regp->r_size + 1) != 0)
		return drcset();
	regcpy(destp->d_str,regp);

	return rc.status;
	}
#endif

// Indent a region n tab stops.
int indentRegion(Datum *rp,int n,Datum **argpp) {
	Line *lnp;
	int count;
	Dot *dotp;

	// Validate n and determine number of tab stops.
	if(n == INT_MIN)
		count = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"
	else
		count = n;

	// Get number of lines.
	if(reglines(&n,NULL) != Success)
		return rc.status;
	dotp = &curwp->w_face.wf_dot;

	// Loop through lines in block.
	kentry.lastflag &= ~CFVMove;
	do {
		dotp->off = 0;				// Start at the beginning.
		lnp = dotp->lnp;

		// Shift current line using tabs.
		if(lnp->l_used > 0 && !is_white(lnp,lnp->l_used) && !((curbp->b_modes & MdC) && lgetc(lnp,dotp->off) == '#')) {
			if(stabsize == 0)
				(void) linsert(count,'\t');
			else {
				begintxt();
				(void) instab(count);
				}
			if(rc.status != Success)
				return rc.status;
			}

		// Move to the next line.
		(void) forwln(1);			// Can't fail.
		} while(--n > 0);

	dotp->off = 0;
	kentry.thisflag &= ~CFVMove;			// Flag that this resets the goal column.
	lchange(curbp,WFEdit);
	return rc.status;
	}

// Outdent a region n tab stops.
int outdentRegion(Datum *rp,int n,Datum **argpp) {
	int count;
	Dot *dotp;

	// Validate n and determine number of tab stops.
	if(n == INT_MIN)
		count = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"
	else
		count = n;

	// Get number of lines.
	if(reglines(&n,NULL) != Success)
		return rc.status;
	dotp = &curwp->w_face.wf_dot;

	// Loop through lines in block.
	kentry.lastflag &= ~CFVMove;
	do {
		dotp->off = 0;
		if(deltab(count) != Success)
			return rc.status;

		// Move to the next line.
		(void) forwln(1);			// Can't fail.
		} while(--n > 0);

	kentry.thisflag &= ~CFVMove;			// Flag that this resets the goal column.
	lchange(curbp,WFEdit);				// Yes, we have made at least an edit.
	return rc.status;
	}
