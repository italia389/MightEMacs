// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// region.c	Region-related functions for MightEMacs.
//
// These routines deal with the region; that is, the space between the point and mark RegMark.  Some functions are commands and
// some are just for internal use.

#include "os.h"
#include "std.h"
#include "bind.h"

// This routine figures out the bounds of the region in the current window and fills in the fields of the Region structure
// pointed to by regp.  Because point and mark RegMark are usually very close together, we scan outward from point in both
// directions at once, looking for the mark.  This should save time.  If RegForceBegin flag is set, dot in the Region object is
// forced to beginning of region; otherwise, it is left at original starting point and r_size is negated if region extends
// backward.  Return status.
int getregion(Region *regp,ushort flags) {
	struct {
		Line *lnp;
		long size;
		int linect;
		} fwd,bwd;
	Dot *mdotp = &si.curbp->b_mroot.mk_dot;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	if(mdotp->off < 0)
		return rcset(Failure,RCTermAttr,text11,RegMark);
			// "No mark ~u%c~U in this buffer"

	// Do special case where mark is on current line.  If region is empty, line count is zero.
	if(mdotp->lnp == dotp->lnp) {
		regp->r_dot.lnp = dotp->lnp;
		if(mdotp->off > dotp->off || !(flags & RegForceBegin)) {
			regp->r_dot.off = dotp->off;
			regp->r_size = (long) (mdotp->off - dotp->off);
			}
		else {
			regp->r_size = (long) (dotp->off - mdotp->off);
			regp->r_dot.off = mdotp->off;
			}
		regp->r_linect = (regp->r_size == 0) ? 0 : 1;
		return rc.status;
		}

	// Do general case; hunt forward and backward looking for mark.
	fwd.lnp = bwd.lnp = dotp->lnp;
	bwd.linect = (bwd.size = (long) dotp->off) == 0 ? 0 : 1;
	fwd.size = (long) (fwd.lnp->l_used - dotp->off + 1);
	fwd.linect = 1;
	while(fwd.lnp->l_nextp != NULL || bwd.lnp != si.curbp->b_lnp) {
		if(fwd.lnp->l_nextp != NULL) {
			fwd.lnp = fwd.lnp->l_nextp;
			if(fwd.lnp == mdotp->lnp) {
				regp->r_dot = *dotp;
				regp->r_size = fwd.size + mdotp->off;
				regp->r_linect = fwd.linect + (mdotp->off > 0 ? 1 : 0);
				return rc.status;
				}
			fwd.size += fwd.lnp->l_used + 1;
			++fwd.linect;
			}
		if(bwd.lnp != si.curbp->b_lnp) {
			bwd.lnp = bwd.lnp->l_prevp;
			bwd.size += bwd.lnp->l_used + 1;
			++bwd.linect;
			if(bwd.lnp == mdotp->lnp) {
				if(flags & RegForceBegin) {
					regp->r_dot.lnp = bwd.lnp;
					regp->r_dot.off = mdotp->off;
					regp->r_size = bwd.size - mdotp->off;
					}
				else {
					regp->r_dot = *dotp;
					regp->r_size = -(bwd.size - mdotp->off);
					}
				regp->r_linect = bwd.linect;
				return rc.status;
				}
			}
		}

	// Huh?  Didn't find mark RegMark!  This is a bug.
	return rcset(FatalError,0,text77,"getregion",RegMark,si.curbp->b_bname);
		// "%s() bug: Mark '%c' not found in buffer '%s'!"
	}

// Create region from given dot and n argument and return in *regp, using n as a text (not line) selector.  If n == 1 (the
// default), select text from dot to the end of the line, unless dot is at the end of the line, in which case select just the
// newline.  If n == 0, select text from dot to the beginning of the line.  If n > 1, select text from dot forward over that
// number of line breaks - 1 to the end of the last line and, if RegInclDelim flag set, include its delimiter.  Lastly, if
// n < 0, select text backward over that number of line breaks to the beginning of the first line.  If RegForceBegin flag set,
// dot in the Region object is forced to beginning of region; otherwise, it is left at original starting point and r_size is
// negated if region extends backward.
void gettregion(Dot *dotp,int n,Region *regp,ushort flags) {
	Line *lnp;
	long chunk;

	regp->r_dot = *dotp;
	regp->r_linect = bempty(NULL) ? 0 : 1;
	if(n == INT_MIN || n == 1) {			// From dot to end of line.
		if((chunk = dotp->lnp->l_used - dotp->off) == 0)
			chunk = bufend(dotp) ? 0 : 1;	// Select line delimiter, if any.
		else if((flags & RegInclDelim) && dotp->lnp->l_nextp != NULL)
			++chunk;			// Include line delimiter.
		}
	else if(n == 0) {				// From dot to beginning of line.
		if(flags & RegForceBegin)
			regp->r_dot.off = 0;
		chunk = -dotp->off;
		}
	else if(n > 1) {				// From dot forward through multiple lines to end of last line.
		chunk = dotp->lnp->l_used - dotp->off;
		for(lnp = dotp->lnp->l_nextp; lnp != NULL; lnp = lnp->l_nextp) {
			chunk += 1 + lnp->l_used;
			if(lnp->l_used > 0 || lnp->l_nextp != NULL)
				++regp->r_linect;
			if(--n == 1) {
				if((flags & RegInclDelim) && lnp->l_nextp != NULL)
					++chunk;
				break;
				}
			}
		}
	else {						// From dot backward through multiple lines to beginning of first line.
		if(flags & RegForceBegin)
			regp->r_dot.off = 0;
		if((chunk = -dotp->off) == 0)
			regp->r_linect = 0;
		lnp = dotp->lnp;
		do {
			if(lnp == si.curbp->b_lnp)
				break;
			lnp = lnp->l_prevp;
			chunk -= 1 + lnp->l_used;
			++regp->r_linect;
			if(flags & RegForceBegin)
				regp->r_dot.lnp = lnp;
			} while(++n < 0);
		}

	// Return results.
	if((regp->r_size = (flags & RegForceBegin) ? labs(chunk) : chunk) == 0)
		regp->r_linect = 0;
	}

// Get a region bounded by a line block and return in *regp.  Force dot in Region object to beginning of block.  If n == 0, call
// getregion() to get initial region; otherwise, call gettregion().  Pass "flags" to the function in either case.  If
// RegInclDelim flag set, include delimiter of last line of block; otherwise, omit it.  If RegEmptyOK flag set, allow empty
// region.  If RegLineSelect flag set, include current line in line count if not a EOB.  Return status.
int getlregion(int n,Region *regp,ushort flags) {
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Empty buffer?
	if(bempty(NULL)) {
		regp->r_linect = 0;
		goto NoText;
		}

	if(n == 0) {							// If zero argument...
		if(getregion(regp,flags | RegForceBegin) != Success)	// select all lines in region.
			return rc.status;
		if(regp->r_size == 0 && !(flags & RegEmptyOK))		// If empty region and not allowed...
			goto NoText;					// no lines were selected.
		Dot *mdotp = &si.curbp->b_mroot.mk_dot;

		// Bump line count in region if it ends at the beginning of a line which is not at EOB and region is not empty.
		if((flags & RegLineSelect) && regp->r_size > 0) {
			Dot *edotp = (regp->r_dot.lnp == dotp->lnp && regp->r_dot.off == dotp->off) ? mdotp : dotp;
			if(edotp->off == 0 && (edotp->lnp->l_nextp != NULL || edotp->lnp->l_used > 0))
				++regp->r_linect;
			}

		// Expand region to include text from dot or mark at top of region backward to beginning of that line, and text
		// from dot or mark at bottom of region forward to end of that line, plus its delimiter if RegInclDelim flag
		// set.  However, don't include a line delimiter at end of last line of buffer (which doesn't exist).
		regp->r_size += (regp->r_dot.lnp == dotp->lnp && regp->r_dot.off == dotp->off) ?
		 dotp->off + mdotp->lnp->l_used - mdotp->off : mdotp->off + dotp->lnp->l_used - dotp->off;
		if((flags & RegInclDelim) && dotp->lnp->l_nextp != NULL && mdotp->lnp->l_nextp != NULL)
			++regp->r_size;
		regp->r_dot.off = 0;

		// Empty region?
		if(regp->r_size == 0)
			goto NoText;
		if(regp->r_linect == 0)
			regp->r_linect = 1;
		return rc.status;
		}

	// Not selecting all lines in current region.  Get line block.
	Dot dot = *dotp;						// Use copy of dot so it's not changed.
	if(n == INT_MIN) {
		n = 1;
		goto Forw;
		}
	if(n < 0) {							// Going backward?
		if(dot.lnp->l_nextp == NULL || !(flags & RegInclDelim))	// Yes, last line of buffer or omitting delimiter?
			dot.off = dot.lnp->l_used;			// Yes, move to end of line.
		else {
			dot.lnp = dot.lnp->l_nextp;			// No, move to next line...
			--n;						// and bump the line count.
			goto Forw;
			}
		}
	else {
Forw:
		dot.off = 0;						// No, move to beginning of line.
		}

	// Convert line block to a region.
	gettregion(&dot,n,regp,flags | RegForceBegin);
	if(regp->r_size == 0) {
NoText:
		if(!(flags & RegEmptyOK))
			(void) rcset(Failure,RCNoFormat,text259);
					// "No text selected"
		}
	return rc.status;
	}

// Set *np to the number of lines in the current region, place point at beginning of region, and return status.
int reglines(int *np) {
	Region region;

	// Get "line block" region.
	if(getlregion(0,&region,RegInclDelim | RegEmptyOK | RegLineSelect) != Success)
		return rc.status;

	// Move point to beginning of region and return result.
	movept(&region.r_dot);
	*np = region.r_linect;
	return rc.status;
	}

// Delete or kill a region, depending on "kill" flag.  Return status.
int dkregion(int n,bool kill) {
	Region region;

	if(getregion(&region,0) == Success && kprep(kill) == Success)
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
			goto LibFail;
		lnp = regp->r_dot.lnp;					// Current line.
		offset = regp->r_dot.off;				// Current offset.
		if(regp->r_size > 0)					// Going forward?
			do {						// Yes, copy forward.
				if(offset == lnp->l_used) {		// End of line.
					if(dputc('\n',&sf) != 0)
						goto LibFail;
					lnp = lnp->l_nextp;
					offset = 0;
					--regp->r_size;
					}
				else {					// Beginning or middle of line.
					chunk = (lnp->l_used - offset > regp->r_size) ? regp->r_size : lnp->l_used - offset;
					if(dputmem((void *) (lnp->l_text + offset),chunk,&sf) != 0)
						goto LibFail;
					offset += chunk;
					regp->r_size -= chunk;
					}
				} while(regp->r_size > 0);
		else {							// Copy backward.
			char *str;
			do {
				if(offset == 0) {			// Beginning of line.
					if(dputc('\n',&sf) != 0)
						goto LibFail;
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
						goto LibFail;
					offset -= chunk;
					regp->r_size += chunk;
					}
				} while(regp->r_size < 0);
			}
		if(dclose(&sf,sf_string) != 0)
			goto LibFail;
		}
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Copy all of the characters in the region to the given buffer and return pointer to terminating null.  It is assumed that the
// buffer size is at least region size + 1.
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
	return dest;
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

	// Get number of lines and set mark at starting point.
	if(reglines(&n) != Success)
		return rc.status;
	mset(&si.curbp->b_mroot,si.curwp);
	dotp = &si.curwp->w_face.wf_dot;

	// Loop through lines in block.
	kentry.lastflag &= ~CFVMove;
	do {
		dotp->off = 0;				// Start at the beginning.
		lnp = dotp->lnp;

		// Shift current line using tabs.
		if(lnp->l_used > 0 && !is_white(lnp,lnp->l_used)) {
			if(si.stabsize == 0)
				(void) linsert(count,'\t');
			else {
				begintxt();
				(void) instab(count);
				}
			if(rc.status != Success)
				return rc.status;
			}

		// Move to the next line.
		(void) moveln(1);			// Can't fail.
		} while(--n > 0);

	if(!bufend(dotp))
		dotp->off = 0;
	kentry.thisflag &= ~CFVMove;			// Flag that this resets the goal column...
	bchange(si.curbp,WFHard);			// and a line other than the current one was changed.
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

	// Get number of lines and set mark at starting point.
	if(reglines(&n) != Success)
		return rc.status;
	mset(&si.curbp->b_mroot,si.curwp);
	dotp = &si.curwp->w_face.wf_dot;

	// Loop through lines in block.
	kentry.lastflag &= ~CFVMove;
	do {
		dotp->off = 0;
		if(deltab(count,false) != Success)
			return rc.status;

		// Move to the next line.
		(void) moveln(1);			// Can't fail.
		} while(--n > 0);

	kentry.thisflag &= ~CFVMove;			// Flag that this resets the goal column...
	bchange(si.curbp,WFHard);				// and a line other than the current one was changed.
	return rc.status;
	}
