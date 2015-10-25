// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// region.c	Region-related functions for MightEMacs.
//
// These routines deal with the region, that magic space between "." and mark.  Some functions are commands and some are just
// for internal use.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

// Set *np to the number of lines in the current region and *allp (if allp not NULL) to true if region includes all lines in
// the buffer.  Place dot at beginning of region and return status.
int reglines(int *np,bool *allp) {
	Line *lnp;		// Position while scanning.
	int n;			// Number of lines in this current region.
	Region region;

	// Check for a valid region first.
	if(getregion(&region,allp) != SUCCESS)
		return rc.status;

	// Start at the top of the region ...
	lnp = region.r_dot.lnp;
	region.r_size += region.r_dot.off;
	n = 0;

	// Scan the region ... counting lines.
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

	if(getregion(&region,NULL) != SUCCESS)
		return rc.status;
	kprep(kill);
	curwp->w_face.wf_dot = region.r_dot;
	return ldelete(region.r_size,kill ? DFKILL : DFDEL);
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
			if(kinsert(kringp,FORWARD,'\r') != SUCCESS)
				return rc.status;
			lnp = lforw(lnp);
			offset = 0;
			}
		else {				// Beginning or middle of line.
			if(kinsert(kringp,FORWARD,lgetc(lnp,offset)) != SUCCESS)
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

	if(getregion(&region,NULL) != SUCCESS)
		return rc.status;
	lchange(curbp,WFHARD);
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

	dotp = &wfp->wf_mark[0].mk_dot;
	if(dotp->lnp == NULL || dotp->off < 0)
		return rcset(FAILURE,0,text11,0);
			// "No mark %d in this window"

	// Do special case where mark 0 is on current line.
	if(dotp->lnp == wfp->wf_dot.lnp) {
		regp->r_dot.lnp = wfp->wf_dot.lnp;
		if(dotp->off > wfp->wf_dot.off) {
			regp->r_dot.off = wfp->wf_dot.off;
			regp->r_size = (long) (dotp->off - wfp->wf_dot.off);
			}
		else {
#if NULREGERR
			if((regp->r_size = (long) (wfp->wf_dot.off - dotp->off)) == 0)
				return rcset(FAILURE,0,text258);
					// "Null region"
#else
			regp->r_size = (long) (wfp->wf_dot.off - dotp->off);
#endif
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

	// Huh?  Didn't find mark 0!  This is a bug.
	return rcset(FATALERROR,0,text77,"getregion");
		// "%s() bug: Lost mark 0!"
	}

// Copy all of the characters in the region to the given buffer and return it.  It is assumed that the buffer size is at least
// region size + 1.
char *regcpy(char *buf,Region *regp) {
	Line *lnp;
	int offset;
	long rsize;
	char *ptr;

	ptr = buf;
	lnp = regp->r_dot.lnp;			// Current line.
	offset = regp->r_dot.off;		// Current offset.
	rsize = regp->r_size;

	while(rsize-- > 0) {
		if(offset == lused(lnp)) {	// End of line.
			*ptr = '\r';
			lnp = lforw(lnp);
			offset = 0;
			}
		else {				// Middle of line.
			*ptr = lgetc(lnp,offset);
			++offset;
			}
		++ptr;
		}

	*ptr = '\0';
	return buf;
	}

// Copy the contents of the current region to dest.  Return status.
int getregtext(Value *destp) {
	Region region;

	// Get the region limits.
	if(getregion(&region,NULL) != SUCCESS)
		return rc.status;

	// Preallocate a string and copy.
	if(vsalloc(destp,region.r_size + 1) != 0)
		return vrcset();
	regcpy(destp->v_strp,&region);

	return rc.status;
	}

// Indent a region n tab stops.
int indentRegion(Value *rp,int n) {
	Line *lnp;
	int count;
	Dot *dotp;

	// Validate n and determine number of tab stops.
	if(n == INT_MIN)
		count = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"
	else
		count = n;

	// Get number of lines.
	if(reglines(&n,NULL) != SUCCESS)
		return rc.status;
	dotp = &curwp->w_face.wf_dot;

	// Loop through lines in block.
	kentry.lastflag &= ~CFVMOV;
	do {
		dotp->off = 0;				// Start at the beginning.
		lnp = dotp->lnp;

		// Shift current line using tabs.
		if(lnp->l_used > 0 && !is_white(lnp,lnp->l_used) && !((curbp->b_modes & MDC) && lgetc(lnp,dotp->off) == '#')) {
			if(stabsize == 0)
				(void) linsert(count,'\t');
			else {
				begintxt();
				(void) instab(count);
				}
			if(rc.status != SUCCESS)
				return rc.status;
			}

		// Move to the next line.
		(void) forwln(1);			// Can't fail.
		} while(--n > 0);

	dotp->off = 0;
	kentry.thisflag &= ~CFVMOV;			// Flag that this resets the goal column.
	lchange(curbp,WFEDIT);
	return rc.status;
	}

// Outdent a region n tab stops.
int outdentRegion(Value *rp,int n) {
	int count;
	Dot *dotp;

	// Validate n and determine number of tab stops.
	if(n == INT_MIN)
		count = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Command repeat count"
	else
		count = n;

	// Get number of lines.
	if(reglines(&n,NULL) != SUCCESS)
		return rc.status;
	dotp = &curwp->w_face.wf_dot;

	// Loop through lines in block.
	kentry.lastflag &= ~CFVMOV;
	do {
		dotp->off = 0;
		if(deleteTab(rp,count) != SUCCESS)
			return rc.status;

		// Move to the next line.
		(void) forwln(1);			// Can't fail.
		} while(--n > 0);

	kentry.thisflag &= ~CFVMOV;			// Flag that this resets the goal column.
	lchange(curbp,WFEDIT);				// Yes, we have made at least an edit.
	return rc.status;
	}
