// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// region.c	Region-related functions for MightEMacs.
//
// These routines deal with the region; that is, the space between the point and mark RMark.  Some functions are commands and
// some are just for internal use.

#include "os.h"
#include "std.h"
#include "bind.h"

// Set *np to the number of lines in the current region and *allp (if allp not NULL) to true if region includes all lines in
// the buffer.  Place point at beginning of region and return status.
int reglines(int *np,bool *allp) {
	Line *lnp;
	int n;
	Region region;

	// Check for a valid region first.
	if(getregion(&region,true,allp) != Success)
		return rc.status;

	// Start at the top of the region.
	lnp = region.r_dot.lnp;
	region.r_size += region.r_dot.off;
	n = 0;

	// Scan the region, counting lines.
	while(region.r_size >= 0) {
		region.r_size -= lnp->l_used + 1;
		lnp = lnp->l_nextp;
		++n;
		}

	// Place point at the beginning of the region and return result.
	curwp->w_face.wf_dot = region.r_dot;
	*np = n;
	return rc.status;
	}

// Delete or kill a region, depending on "kill" flag.  Return status.
int dkregion(int n,bool kill) {
	Region region;

	if(getregion(&region,false,NULL) == Success && kprep(kill) == Success)
		(void) ldelete(region.r_size,kill ? EditKill : EditDel);
	return rc.status;
	}

// Copy all of the characters in the given region to the kill ring without moving point.  Return status.
int copyreg(Region *regp) {

	if(kprep(true) == Success && regp->r_size != 0) {
		Line *lnp;
		int offset,chunk;
		DStrFab sf;

		if(dopenwith(&sf,&kring.r_entryp->re_data,regp->r_size < 0 ? SFPrepend : SFAppend) != 0)
			return librcset(Failure);
		lnp = regp->r_dot.lnp;					// Current line.
		offset = regp->r_dot.off;				// Current offset.
		if(regp->r_size > 0)					// Going forward?
			do {						// Yes, copy forward.
				if(offset == lnp->l_used) {		// End of line.
					if(dputc('\n',&sf) != 0)
						return librcset(Failure);
					lnp = lnp->l_nextp;
					offset = 0;
					--regp->r_size;
					}
				else {					// Beginning or middle of line.
					chunk = (lnp->l_used - offset > regp->r_size) ? regp->r_size : lnp->l_used - offset;
					if(dputmem((void *) (lnp->l_text + offset),chunk,&sf) != 0)
						return librcset(Failure);
					offset += chunk;
					regp->r_size -= chunk;
					}
				} while(regp->r_size > 0);
		else {							// Copy backward.
			char *str;
			do {
				if(offset == 0) {			// Beginning of line.
					if(dputc('\n',&sf) != 0)
						return librcset(Failure);
					lnp = lnp->l_prevp;
					offset = lnp->l_used;
					++regp->r_size;
					}
				else {					// End or middle of line.
					if(offset > -regp->r_size) {
						chunk = -regp->r_size;
						str = lnp->l_text + offset - chunk;
						}
					else {
						chunk = offset;
						str = lnp->l_text;
						}
					if(dputmem((void *) str,chunk,&sf) != 0)
						return librcset(Failure);
					offset -= chunk;
					regp->r_size += chunk;
					}
				} while(regp->r_size < 0);
			}
		if(dclose(&sf,sf_string) != 0)
			(void) librcset(Failure);
		}
	return rc.status;
	}

// This routine figures out the bounds of the region in the current window and fills in the fields of the Region structure
// pointed to by regp.  Because point and mark RMark are usually very close together, we scan outward from point in both
// directions at once, looking for the mark.  This should save time.  If forceBegin is true, dot in the Region object is forced
// to beginning of region; otherwise, it is left at original starting point (and r_size is negated if region extends backward).
// If wholebufp is not NULL, *wholebufp is set to true if region extends from first line of buffer to or past last line;
// otherwise, false.  Return status.
int getregion(Region *regp,bool forceBegin,bool *wholebufp) {
	long fsize,bsize;
	Line *flp,*blp;
	Dot *mdotp = &curbp->b_mroot.mk_dot;
	Dot *dotp = &curwp->w_face.wf_dot;

	if(mdotp->off < 0)
		return rcset(Failure,0,text11,RMark);
			// "No mark '%c' in this buffer",

	// Do special case where mark is on current line.
	if(mdotp->lnp == dotp->lnp) {
		regp->r_dot.lnp = dotp->lnp;
		if(mdotp->off > dotp->off || !forceBegin) {
			regp->r_dot.off = dotp->off;
			regp->r_size = (long) (mdotp->off - dotp->off);
			}
		else {
			regp->r_size = (long) (dotp->off - mdotp->off);
			regp->r_dot.off = mdotp->off;
			}
		if(wholebufp != NULL)
			*wholebufp = false;
		return rc.status;
		}

	// Do general case; hunt forward and backward looking for mark.
	flp = blp = dotp->lnp;
	bsize = (long) dotp->off;
	fsize = (long) (flp->l_used - dotp->off + 1);
	while(flp != curbp->b_hdrlnp || blp->l_prevp != curbp->b_hdrlnp) {
		if(flp != curbp->b_hdrlnp) {
			flp = flp->l_nextp;
			if(flp == mdotp->lnp) {
				regp->r_dot = *dotp;
				regp->r_size = fsize + mdotp->off;
				if(wholebufp != NULL)
					*wholebufp = (dotp->lnp->l_prevp == curbp->b_hdrlnp && (flp == curbp->b_hdrlnp ||
					 flp->l_nextp == curbp->b_hdrlnp));
				return rc.status;
				}
			fsize += flp->l_used + 1;
			}
		if(blp->l_prevp != curbp->b_hdrlnp) {
			blp = blp->l_prevp;
			bsize += blp->l_used + 1;
			if(blp == mdotp->lnp) {
				if(forceBegin) {
					regp->r_dot.lnp = blp;
					regp->r_dot.off = mdotp->off;
					regp->r_size = bsize - mdotp->off;
					}
				else {
					regp->r_dot = *dotp;
					regp->r_size = -(bsize - mdotp->off);
					}
				if(wholebufp != NULL)
					*wholebufp = (blp->l_prevp == curbp->b_hdrlnp && (dotp->lnp == curbp->b_hdrlnp ||
					 dotp->lnp->l_nextp == curbp->b_hdrlnp));
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
		if((len = lnp->l_used - offset) == 0) {	// End of line.
			*dest++ = '\n';
			lnp = lnp->l_nextp;
			--rsize;
			offset = 0;
			}
		else {					// Beginning or middle of line.
			if(len > rsize)
				len = rsize;
			dest = (char *) memzcpy((void *) dest,(void *) (lnp->l_text + offset),len);
			offset += len;
			rsize -= len;
			}
		}
	*dest = '\0';
	return buf;
	}

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
		if(lnp->l_used > 0 && !is_white(lnp,lnp->l_used) &&
		 !((curbp->b_modes & MdC) && lnp->l_text[dotp->off] == '#')) {
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
	kentry.thisflag &= ~CFVMove;			// Flag that this resets the goal column...
	bchange(curbp,WFHard);				// and a line other than the current one was changed.
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
		if(deltab(count,false) != Success)
			return rc.status;

		// Move to the next line.
		(void) forwln(1);			// Can't fail.
		} while(--n > 0);

	kentry.thisflag &= ~CFVMove;			// Flag that this resets the goal column...
	bchange(curbp,WFHard);				// and a line other than the current one was changed.
	return rc.status;
	}
