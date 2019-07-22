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
// pointed to by reg.  Because point and mark RegMark are usually very close together, we scan outward from point in both
// directions at once, looking for the mark.  This should save time.  If RegForceBegin flag is set, point in the Region object
// is forced to beginning of region; otherwise, it is left at original starting point and r_size is negated if region extends
// backward.  Return status.
int getregion(Region *reg,ushort flags) {
	struct {
		Line *lnp;
		long size;
		int linect;
		} fwd,bwd;
	Point *mpoint = &si.curbuf->b_mroot.mk_point;
	Point *point = &si.curwin->w_face.wf_point;

	if(mpoint->off < 0)
		return rcset(Failure,RCTermAttr,text11,RegMark);
			// "No mark ~u%c~U in this buffer"

	// Do special case where mark is on current line.  If region is empty, line count is zero.
	if(mpoint->lnp == point->lnp) {
		reg->r_point.lnp = point->lnp;
		if(mpoint->off > point->off || !(flags & RegForceBegin)) {
			reg->r_point.off = point->off;
			reg->r_size = (long) (mpoint->off - point->off);
			}
		else {
			reg->r_size = (long) (point->off - mpoint->off);
			reg->r_point.off = mpoint->off;
			}
		reg->r_linect = (reg->r_size == 0) ? 0 : 1;
		return rc.status;
		}

	// Do general case; hunt forward and backward looking for mark.
	fwd.lnp = bwd.lnp = point->lnp;
	bwd.linect = (bwd.size = (long) point->off) == 0 ? 0 : 1;
	fwd.size = (long) (fwd.lnp->l_used - point->off + 1);
	fwd.linect = 1;
	while(fwd.lnp->l_next != NULL || bwd.lnp != si.curbuf->b_lnp) {
		if(fwd.lnp->l_next != NULL) {
			fwd.lnp = fwd.lnp->l_next;
			if(fwd.lnp == mpoint->lnp) {
				reg->r_point = *point;
				reg->r_size = fwd.size + mpoint->off;
				reg->r_linect = fwd.linect + (mpoint->off > 0 ? 1 : 0);
				return rc.status;
				}
			fwd.size += fwd.lnp->l_used + 1;
			++fwd.linect;
			}
		if(bwd.lnp != si.curbuf->b_lnp) {
			bwd.lnp = bwd.lnp->l_prev;
			bwd.size += bwd.lnp->l_used + 1;
			++bwd.linect;
			if(bwd.lnp == mpoint->lnp) {
				if(flags & RegForceBegin) {
					reg->r_point.lnp = bwd.lnp;
					reg->r_point.off = mpoint->off;
					reg->r_size = bwd.size - mpoint->off;
					}
				else {
					reg->r_point = *point;
					reg->r_size = -(bwd.size - mpoint->off);
					}
				reg->r_linect = bwd.linect;
				return rc.status;
				}
			}
		}

	// Huh?  Didn't find mark RegMark!  This is a bug.
	return rcset(FatalError,0,text77,"getregion",RegMark,si.curbuf->b_bname);
		// "%s() bug: Mark '%c' not found in buffer '%s'!"
	}

// Create region from given point and n argument and return in *reg, using n as a text (not line) selector.  If n == 1 (the
// default), select text from point to the end of the line, unless point is at the end of the line, in which case select just
// the newline.  If n == 0, select text from point to the beginning of the line.  If n > 1, select text from point forward over
// that number of line breaks - 1 to the end of the last line and, if RegInclDelim flag set, include its delimiter.  Lastly, if
// n < 0, select text backward over that number of line breaks to the beginning of the first line.  If RegForceBegin flag set,
// point in the Region object is forced to beginning of region; otherwise, it is left at original starting point and r_size is
// negated if region extends backward.
void gettregion(Point *point,int n,Region *reg,ushort flags) {
	Line *lnp;
	long chunk;

	reg->r_point = *point;
	reg->r_linect = bempty(NULL) ? 0 : 1;
	if(n == INT_MIN || n == 1) {		// From point to end of line.
		if((chunk = point->lnp->l_used - point->off) == 0)
			chunk = bufend(point) ? 0 : 1;	// Select line delimiter, if any.
		else if((flags & RegInclDelim) && point->lnp->l_next != NULL)
			++chunk;		// Include line delimiter.
		}
	else if(n == 0) {			// From point to beginning of line.
		if(flags & RegForceBegin)
			reg->r_point.off = 0;
		chunk = -point->off;
		}
	else if(n > 1) {			// From point forward through multiple lines to end of last line.
		chunk = point->lnp->l_used - point->off;
		for(lnp = point->lnp->l_next; lnp != NULL; lnp = lnp->l_next) {
			chunk += 1 + lnp->l_used;
			if(lnp->l_used > 0 || lnp->l_next != NULL)
				++reg->r_linect;
			if(--n == 1) {
				if((flags & RegInclDelim) && lnp->l_next != NULL)
					++chunk;
				break;
				}
			}
		}
	else {					// From point backward through multiple lines to beginning of first line.
		if(flags & RegForceBegin)
			reg->r_point.off = 0;
		if((chunk = -point->off) == 0)
			reg->r_linect = 0;
		lnp = point->lnp;
		do {
			if(lnp == si.curbuf->b_lnp)
				break;
			lnp = lnp->l_prev;
			chunk -= 1 + lnp->l_used;
			++reg->r_linect;
			if(flags & RegForceBegin)
				reg->r_point.lnp = lnp;
			} while(++n < 0);
		}

	// Return results.
	if((reg->r_size = (flags & RegForceBegin) ? labs(chunk) : chunk) == 0)
		reg->r_linect = 0;
	}

// Get a region bounded by a line block and return in *reg.  Force point in Region object to beginning of block.  If n == 0,
// call getregion() to get initial region; otherwise, call gettregion().  Pass "flags" to the function in either case.  If
// RegInclDelim flag set, include delimiter of last line of block; otherwise, omit it.  If RegEmptyOK flag set, allow empty
// region.  If RegLineSelect flag set, include current line in line count if not a EOB.  Return status.
int getlregion(int n,Region *reg,ushort flags) {
	Point *point = &si.curwin->w_face.wf_point;

	// Empty buffer?
	if(bempty(NULL)) {
		reg->r_linect = 0;
		goto NoText;
		}

	if(n == 0) {							// If zero argument...
		if(getregion(reg,flags | RegForceBegin) != Success)	// select all lines in region.
			return rc.status;
		if(reg->r_size == 0 && !(flags & RegEmptyOK))		// If empty region and not allowed...
			goto NoText;					// no lines were selected.
		Point *mpoint = &si.curbuf->b_mroot.mk_point;

		// Bump line count in region if it ends at the beginning of a line which is not at EOB and region is not empty.
		if((flags & RegLineSelect) && reg->r_size > 0) {
			Point *epoint = (reg->r_point.lnp == point->lnp && reg->r_point.off == point->off) ? mpoint : point;
			if(epoint->off == 0 && (epoint->lnp->l_next != NULL || epoint->lnp->l_used > 0))
				++reg->r_linect;
			}

		// Expand region to include text from point or mark at top of region backward to beginning of that line, and
		// text from point or mark at bottom of region forward to end of that line, plus its delimiter if RegInclDelim
		// flag set.  However, don't include a line delimiter at end of last line of buffer (which doesn't exist).
		reg->r_size += (reg->r_point.lnp == point->lnp && reg->r_point.off == point->off) ?
		 point->off + mpoint->lnp->l_used - mpoint->off : mpoint->off + point->lnp->l_used - point->off;
		if((flags & RegInclDelim) && point->lnp->l_next != NULL && mpoint->lnp->l_next != NULL)
			++reg->r_size;
		reg->r_point.off = 0;

		// Empty region?
		if(reg->r_size == 0)
			goto NoText;
		if(reg->r_linect == 0)
			reg->r_linect = 1;
		return rc.status;
		}

	// Not selecting all lines in current region.  Get line block.
	Point wkpoint = *point;						// Use copy of point so it's not changed.
	if(n == INT_MIN) {
		n = 1;
		goto Forw;
		}
	if(n < 0) {							// Going backward?
		if(wkpoint.lnp->l_next == NULL ||
		 !(flags & RegInclDelim))				// Yes, last line of buffer or omitting delimiter?
			wkpoint.off = wkpoint.lnp->l_used;		// Yes, move to end of line.
		else {
			wkpoint.lnp = wkpoint.lnp->l_next;		// No, move to next line...
			--n;						// and bump the line count.
			goto Forw;
			}
		}
	else {
Forw:
		wkpoint.off = 0;					// No, move to beginning of line.
		}

	// Convert line block to a region.
	gettregion(&wkpoint,n,reg,flags | RegForceBegin);
	if(reg->r_size == 0) {
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
	movept(&region.r_point);
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
int copyreg(Region *reg) {

	if(kprep(true) == Success && reg->r_size != 0) {
		Line *lnp;
		int offset,chunk;
		DStrFab sfab;

		if(dopenwith(&sfab,&kring.r_entry->re_data,reg->r_size < 0 ? SFPrepend : SFAppend) != 0)
			goto LibFail;
		lnp = reg->r_point.lnp;					// Current line.
		offset = reg->r_point.off;				// Current offset.
		if(reg->r_size > 0)					// Going forward?
			do {						// Yes, copy forward.
				if(offset == lnp->l_used) {		// End of line.
					if(dputc('\n',&sfab) != 0)
						goto LibFail;
					lnp = lnp->l_next;
					offset = 0;
					--reg->r_size;
					}
				else {					// Beginning or middle of line.
					chunk = (lnp->l_used - offset > reg->r_size) ? reg->r_size : lnp->l_used - offset;
					if(dputmem((void *) (lnp->l_text + offset),chunk,&sfab) != 0)
						goto LibFail;
					offset += chunk;
					reg->r_size -= chunk;
					}
				} while(reg->r_size > 0);
		else {							// Copy backward.
			char *str;
			do {
				if(offset == 0) {			// Beginning of line.
					if(dputc('\n',&sfab) != 0)
						goto LibFail;
					lnp = lnp->l_prev;
					offset = lnp->l_used;
					++reg->r_size;
					}
				else {					// End or middle of line.
					if(offset > -reg->r_size) {
						chunk = -reg->r_size;
						str = lnp->l_text + offset - chunk;
						}
					else {
						chunk = offset;
						str = lnp->l_text;
						}
					if(dputmem((void *) str,chunk,&sfab) != 0)
						goto LibFail;
					offset -= chunk;
					reg->r_size += chunk;
					}
				} while(reg->r_size < 0);
			}
		if(dclose(&sfab,sf_string) != 0)
			goto LibFail;
		}
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Copy all of the characters in the region to the given string buffer and return pointer to terminating null.  It is assumed
// that the buffer size is at least region size + 1 and the region point is at the beginning of the region.
char *regcpy(char *buf,Region *reg) {
	Line *lnp;
	int offset;
	long rsize,len;
	char *dest;

	dest = buf;
	lnp = reg->r_point.lnp;					// Current line.
	offset = reg->r_point.off;				// Current offset.
	rsize = reg->r_size;

	while(rsize > 0) {
		if((len = lnp->l_used - offset) == 0) {		// End of line.
			*dest++ = '\n';
			lnp = lnp->l_next;
			--rsize;
			offset = 0;
			}
		else {						// Beginning or middle of line.
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
int indentRegion(Datum *rval,int n,Datum **argv) {
	Line *lnp;
	int count;
	Point *point;

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
	mset(&si.curbuf->b_mroot,si.curwin);
	point = &si.curwin->w_face.wf_point;

	// Loop through lines in block.
	kentry.lastflag &= ~CFVMove;
	do {
		point->off = 0;				// Start at the beginning.
		lnp = point->lnp;

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

	if(!bufend(point))
		point->off = 0;
	kentry.thisflag &= ~CFVMove;			// Flag that this resets the goal column...
	bchange(si.curbuf,WFHard);			// and a line other than the current one was changed.
	return rc.status;
	}

// Outdent a region n tab stops.
int outdentRegion(Datum *rval,int n,Datum **argv) {
	int count;
	Point *point;

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
	mset(&si.curbuf->b_mroot,si.curwin);
	point = &si.curwin->w_face.wf_point;

	// Loop through lines in block.
	kentry.lastflag &= ~CFVMove;
	do {
		point->off = 0;
		if(deltab(count,false) != Success)
			return rc.status;

		// Move to the next line.
		(void) moveln(1);			// Can't fail.
		} while(--n > 0);

	kentry.thisflag &= ~CFVMove;			// Flag that this resets the goal column...
	bchange(si.curbuf,WFHard);				// and a line other than the current one was changed.
	return rc.status;
	}
