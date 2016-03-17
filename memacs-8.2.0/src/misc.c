// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// misc.c	Miscellaneous functions for MightEMacs.
//
// This file contains command processing routines for a few random commands.  There is no functional grouping here, for sure.

#include "os.h"
#include <ctype.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

// Is a character a letter?  We presume a letter must be either in the upper or lower case tables (even if it gets
// translated to itself).
bool isletter(int ch) {

	return is_upper(ch) || is_lower(ch);
	}

// Is a character a lower case letter?
bool is_lower(int ch) {

	return (ch >= 'a' && ch <= 'z');
	}

// Is a character an upper case letter?
bool is_upper(int ch) {

	return (ch >= 'A' && ch <= 'Z');
	}

// Change the case of the current character.  First check lower and then upper.  If it is not a letter, it gets returned
// unchanged.
int chcase(int ch) {

	// Translate lowercase.
	if(is_lower(ch))
		return upcase[ch];

	// Translate uppercase.
	if(is_upper(ch))
		return lowcase[ch];

	// Let the rest pass.
	return ch;
	}

// Copy a string from src to dst, changing its case.  Return dst.
static char *trancase(char *dst,char *src,char *trantab) {
	char *strp;

	strp = dst;
	while(*src != '\0')
		*strp++ = trantab[(int) *src++];
	*strp = '\0';

	return dst;
	}

// Copy a string from src to dest, making it lower case.  Return dest.
char *mklower(char *destp,char *srcp) {

	return trancase(destp,srcp,lowcase);
	}

// Copy a string from src to dest, making it upper case.  Return dest.
char *mkupper(char *destp,char *srcp) {

	return trancase(destp,srcp,upcase);
	}

// Initialize the character upper/lower case tables.
void initchars(void) {
	int index;

	// Set all of both tables to their indices.
	for(index = 0; index < HICHAR; ++index)
		lowcase[index] = upcase[index] = index;

	// Set letter translations.
	for(index = 'a'; index <= 'z'; ++index) {
		upcase[index] = index ^ 0x20;
		lowcase[index ^ 0x20] = index;
		}

	// And those international characters also.
	for(index = (uchar) '\340'; index <= (uchar) '\375'; ++index) {
		upcase[index] = index ^ 0x20;
		lowcase[index ^ 0x20] = index;
		}
	}

// Reverse string in place.  Code here for those compilers that do not have the function in their own library.
char *strrev(char *our_str) {
	char *beg_str,*end_str;
	char the_char;

	end_str = beg_str = our_str;
	end_str += strlen(beg_str);

	do {
		the_char = *--end_str;
		*end_str = *beg_str;
		*beg_str++ = the_char;
		} while(end_str > beg_str);

	return our_str;
	}

// If default n, display the current line, column, and character position of the cursor in the current buffer, the fraction of
// the text that is before the cursor, and the character that is under the cursor (in printable form and hex).  If n is not the
// default, display the cursor column and the character under the cursor only.  The displayed column is not the current column,
// but the column on an infinite-width display.  (Interactive only)
int whence(Value *rp,int n) {
	Line *lnp;			// Current line.
	ulong numchars;			// # of chars in file.
	ulong numlines;			// # of lines in file.
	ulong predchars;		// # chars preceding point.
	ulong predlines;		// # lines preceding point.
	int curchar;			// Character under cursor.
	double ratio;
	int col;
	int savepos;			// Temp save for current offset.
	int ecol;			// Column pos/end of current line.
	Dot *dotp = &curwp->w_face.wf_dot;
	char *strp,wkbuf1[32],wkbuf2[32];

	// Skip this if not displaying messages.
	if(!(modetab[MDR_GLOBAL].flags & MDMSG))
		return rc.status;

	if(n == INT_MIN) {

		// Starting at the beginning of the buffer.
		lnp = lforw(curbp->b_hdrlnp);
		curchar = '\r';

		// Start counting chars and lines.
		numchars = numlines = 0;
		while(lnp != curbp->b_hdrlnp) {

			// If we are on the current line, record it.
			if(lnp == dotp->lnp) {
				predlines = numlines;
				predchars = numchars + dotp->off;
				curchar = (dotp->off == lused(lnp)) ? '\r' : lgetc(lnp,dotp->off);
				}

			// On to the next line.
			++numlines;
			numchars += lused(lnp) + 1;
			lnp = lforw(lnp);
			}

		// If dot is at end of buffer, record it.
		if(dotp->lnp == curbp->b_hdrlnp) {
			predlines = numlines;
			predchars = numchars;
			}

		ratio = 0.0;				// Ratio before dot.
		if(numchars > 0)
			ratio = (double) predchars / numchars * 100.0;
		sprintf(strp = wkbuf2,"%.1f",ratio);
		if(numchars > 0) {

			// Fix rounding errors at buffer boundaries.
			if(predchars > 0 && strcmp(strp,"0.0") == 0)
				strp = "0.1";
			else if(predchars < numchars && strcmp(strp,"100.0") == 0)
				strp = "99.9";
			}
		}
	else
		curchar = (dotp->off == lused(dotp->lnp)) ? '\r' : lgetc(dotp->lnp,dotp->off);

	// Get real column and end-of-line column.
	col = getccol();
	savepos = dotp->off;
	dotp->off = lused(dotp->lnp);
	ecol = getccol();
	dotp->off = savepos;

	// Summarize and report the info.
	if(curchar >= ' ' && curchar < 0x7f)
		sprintf(wkbuf1,"'%c' 0x%.2x",curchar,curchar);
	else
		sprintf(wkbuf1,"0x%.2x",curchar);
	return (n == INT_MIN) ? rcset(SUCCESS,0,text60,predlines + 1,numlines,col,ecol,predchars,numchars,strp,wkbuf1) :
					// "Line %lu/%lu, Col %d/%d, Char %lu/%lu (%s%%), char = %s"
	 rcset(SUCCESS,0,text340,col,ecol,wkbuf1);
		// "Col %d/%d, char = %s"
	}

// Get line number, given buffer and line pointer.
long getlinenum(Buffer *bufp,Line *targlnp) {
	Line *lnp;
	long n;

	// Starting at the beginning of the buffer.
	lnp = lforw(bufp->b_hdrlnp);

	// Start counting lines.
	n = 0;
	while(lnp != bufp->b_hdrlnp) {

		// If we have reached the target line, stop...
		if(lnp == targlnp)
			break;
		++n;
		lnp = lforw(lnp);
		}

	// and return the count.
	return n + 1;
	}

// Return new column, given character c and old column.
int newcol(int c,int col) {

	if(c == '\t')
		col += -(col % htabsize) + (htabsize - 1);
	else if(c < 0x20 || c == 0x7f)
		++col;
	else if(c > 0x7f && (modetab[MDR_GLOBAL].flags & MDESC8))
		col += 3;
	return col + 1;
	}

// Return current column of cursor.
int getccol(void) {
	int i,col;
	Dot *dotp = &curwp->w_face.wf_dot;

	col = 0;
	for(i = 0; i < dotp->off; ++i)
		col = newcol(lgetc(dotp->lnp,i),col);
	return col;
	}

// Try to set current column to given position.  Return status.
int setccol(int pos) {
	int i;			// Index into current line.
	int col;		// Current cursor column.
	int llen;		// Length of line in bytes.
	Dot *dotp = &curwp->w_face.wf_dot;

	col = 0;
	llen = lused(dotp->lnp);

	// Scan the line until we are at or past the target column.
	for(i = 0; i < llen; ++i) {

		// Upon reaching the target, drop out.
		if(col >= pos)
			break;

		// Advance one character.
		col = newcol(lgetc(dotp->lnp,i),col);
		}

	// Set point to the new position...
	dotp->off = i;

	// and return status.
	return rc.status;
	}

// Check if all white space from beginning of line, given length.  Return boolean result, including true if length is zero.
bool is_white(Line *lnp,int length) {
	int ch,i;

	for(i = 0; i < length; ++i) {
		ch = lgetc(lnp,i);
		if(ch != ' ' && ch != '\t')
			return false;
		}
	return true;
	}

// Match closing fences against their partners, and if on screen, briefly light the cursor there.
int fmatch(int ch) {
	Dot odot;		// Original line pointer and offset.
	Line *toplp;		// Top line in current window.
	int count;		// Current fence level count.
	int opench;		// Open fence.
	int c;			// Current character in scan.
	WindFace *wfp = &curwp->w_face;

	// Skip this if executing a script or a keyboard macro.
	if((opflags & OPSCRIPT) || kmacro.km_state == KMPLAY)
		return rc.status;

	// First get the display update out there.
	if(update(false) != SUCCESS)
		return rc.status;

	// Save the original cursor position.
	odot = wfp->wf_dot;

	// Set up proper open fence for passed close fence.
	switch(ch) {
		case ')':
			opench = '(';
			break;
		case '}':
			opench = '{';
			break;
		default:	// ']'
			opench = '[';
		}

	// Get top line of window and set up for scan.
	toplp = lback(wfp->wf_toplnp);
	count = 1;
	(void) backch(1);

	// Scan back until we find it, or move past the top of the window.
	while(count > 0 && wfp->wf_dot.lnp != toplp) {
		(void) backch(1);
		if(wfp->wf_dot.off == lused(wfp->wf_dot.lnp))
			c = '\r';
		else
			c = lgetc(wfp->wf_dot.lnp,wfp->wf_dot.off);
		if(c == ch)
			++count;
		else if(c == opench)
			--count;
		if(wfp->wf_dot.lnp == lforw(curbp->b_hdrlnp) && wfp->wf_dot.off == 0)
			break;
		}

	// If count is zero, we have a match -- display the sucker.
	if(count == 0) {
		if(update(false) != SUCCESS)
			return rc.status;
		cpause(fencepause);
		}

	// Restore the previous position.
	wfp->wf_dot = odot;

	return rc.status;
	}


#if COLOR
// Look up color index, given name.
int lookup_color(char *strp) {
	int i;			// Index into color list.

	// Test it against the colors we know.
	for(i = 0; i < NCOLORS; ++i) {
		if(strcmp(strp,cname[i]) == 0)
			return i;
		}
	return -1;
	}
#endif

#if WORDCOUNT
// Count the number of words in the marked region, along with average word sizes, number of chars, etc, and report on them
// (interactive only).
int countWords(Value *rp,int n) {
	Line *lnp;		// Current line to scan.
	int offset;		// Current char to scan.
	long size;		// Size of region left to count.
	int ch;			// Current character to scan.
	bool wordflag;		// Is current character a word character?
	bool inword;		// Are we in a word now?
	long nwords;		// Total number of words.
	long nchars;		// Total number of word characters.
	int nlines;		// Total number of lines in region.
	Region region;		// Region to look at.

	// Skip this if not displaying messages.
	if(!(modetab[MDR_GLOBAL].flags & MDMSG))
		return rc.status;

	// Make sure we have a region to count.
	if(getregion(&region,NULL) != SUCCESS)
		return rc.status;
	lnp = region.r_dot.lnp;
	offset = region.r_dot.off;
	size = region.r_size;

	// Count up things.
	inword = false;
	nchars = nwords = 0;
	nlines = 0;
	while(size--) {

		// Get the current character...
		if(offset == lused(lnp)) {	// End of line.
			ch = '\r';
			lnp = lforw(lnp);
			offset = 0;

			++nlines;
			}
		else {
			ch = lgetc(lnp,offset);
			++offset;
			}

		// and tabulate it.
		if((wordflag = isletter(ch) || isdigit(ch)))
			++nchars;
		if(wordflag && !inword)
			++nwords;
		inword = wordflag;
		}

	// and report on the info.
	return rcset(SUCCESS,0,text100,nwords,nchars,region.r_size,nlines + 1,(nwords > 0) ? nchars * 1.0 / nwords : 0.0);
		// "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f"
	}
#endif

// Set i variable.  Use numeric argument for first parameter if present; otherwise, get arguments.  Return status.
int seti(Value *rp,int n) {
	int i = ivar.i;
	int inc = ivar.inc;
	bool newfmt = false;
	Value *vp;

	// Numeric argument?
	if(n != INT_MIN) {
		ivar.i = n;
		return rcset(SUCCESS,0,text287,ivar.i);
				// "i variable set to %d"
		}

	// Get value(s).
	if(vnew(&vp,false) != 0)
		return vrcset();
	if(opflags & OPSCRIPT) {

		// Get "i" argument and decode it.
		if(funcarg(vp,ARG_FIRST | ARG_NOTNULL | ARG_INT) != SUCCESS)
			return rc.status;
		if(opflags & OPEVAL)
			i = vp->u.v_int;

		// Have "inc" argument?
		if(havesym(s_comma,false)) {

			// Yes, get it and decode it.
			if(funcarg(vp,ARG_NOTNULL | ARG_INT) != SUCCESS)
				return rc.status;
			if(opflags & OPEVAL)
				inc = vp->u.v_int;

			// Have "format" argument?
			if(havesym(s_comma,false)) {

				// Yes, get it.
				if(funcarg(vp,ARG_NOTNULL | ARG_STR) != SUCCESS)
					return rc.status;
				newfmt = true;
				}
			}

		// Bail out here if not evaluating arguments.
		if(!(opflags & OPEVAL))
			return rc.status;
		}
	else {
		char nbuf[LONGWIDTH];

		// Prompt for "i" value.
		if(terminp(vp,text102,"0",CTRL | 'M',0,0) != SUCCESS || toint(vp) != SUCCESS)
				// "Beginning value"
			return rc.status;
		i = vp->u.v_int;

		// Prompt for "inc" value.
		sprintf(nbuf,"%d",inc);
		if(terminp(vp,text234,nbuf,CTRL | 'M',0,0) != SUCCESS || toint(vp) != SUCCESS)
				// "Increment"
			return rc.status;
		inc = vp->u.v_int;

		// Prompt for "format" value.
		if(terminp(vp,text235,ivar.format.v_strp,CTRL | '[',0,0) != SUCCESS)
				// "Format string"
			return rc.status;
		newfmt = true;
		}

	// Validate arguments.
	if(inc == 0)				// Zero increment.
		return rcset(FAILURE,0,text236);
			// "i increment cannot be zero!"

	// Validate format string if changed.
	if(newfmt) {
		if(strcmp(vp->v_strp,ivar.format.v_strp) == 0)
			newfmt = false;
		else {
			int c,icount,ocount;
			bool inspec = false;
			char *strp = vp->v_strp;

			icount = ocount = 0;
			do {
				c = *strp++;
				if(inspec) {
					switch(c) {
						case '%':
							inspec = false;
							break;
						case 'd':
						case 'o':
						case 'u':
						case 'x':
						case 'X':
							++icount;
							inspec = false;
							break;
						default:
							if(strchr("0123456789+- .",c) == NULL) {
								++ocount;
								inspec = false;
								}
							}
					}
				else if(c == '%')
					inspec = true;
				} while(*strp != '\0');

			if(icount != 1 || ocount > 0)		// Bad format string.
				return rcset(FAILURE,0,text237,vp->v_strp);
					// "Invalid i format '%s' (must contain exactly one %%d, %%o, %%u, %%x, or %%X)"
			}
		}

	// Passed all edits ... update ivar.
	ivar.i = i;
	ivar.inc = inc;
	if(newfmt)
		vxfer(&ivar.format,vp);

	return rc.status;
	}

// Return a random integer.  This function implements the "Minimal Standard, Integer Version 2" RNG from the paper "RNGs: Good
// Ones are Hard to Find" by Park and Miller, CACM, Volume 31, Number 10, October 1988.
int ernd(void) {
	int
		a = 16807,
		m = 2147483647,
		q = 127773,	// (m / a)
		r = 2836;	// (m % a)
	int lo,hi,test;

	hi = randseed / q;
	lo = randseed % q;
	test = a * lo - r * hi;
	return (randseed = (test > 0) ? test : test + m);
	}
