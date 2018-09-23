// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// misc.c	Miscellaneous functions for MightEMacs.
//
// This file contains command processing routines for a few random commands.  There is no functional grouping here, for sure.

#include "os.h"
#include <ctype.h>
#include "std.h"
#include "bind.h"
#include "exec.h"
#include "cmd.h"
#include "search.h"
#include "var.h"

// Is a character a letter?  We presume a letter must be either in the upper or lower case tables (even if it gets
// translated to itself).
bool isletter(short c) {

	return is_upper(c) || is_lower(c);
	}

// Is a character a lower case letter?
bool is_lower(short c) {

	return (c >= 'a' && c <= 'z');
	}

// Is a character an upper case letter?
bool is_upper(short c) {

	return (c >= 'A' && c <= 'Z');
	}

// Change the case of the current character.  First check lower and then upper.  If it is not a letter, it gets returned
// unchanged.
int chcase(short c) {

	// Translate lowercase.
	if(is_lower(c))
		return upcase[c];

	// Translate uppercase.
	if(is_upper(c))
		return lowcase[c];

	// Let the rest pass.
	return c;
	}

// Copy a string from src to dst, changing its case.  Return dst.
static char *trancase(char *dst,char *src,char *trantab) {
	char *str;

	str = dst;
	while(*src != '\0')
		*str++ = trantab[(int) *src++];
	*str = '\0';

	return dst;
	}

// Copy a string from src to dest, making it lower case.  Return dest.
char *mklower(char *dest,char *src) {

	return trancase(dest,src,lowcase);
	}

// Copy a string from src to dest, making it upper case.  Return dest.
char *mkupper(char *dest,char *src) {

	return trancase(dest,src,upcase);
	}

// Initialize the character upper/lower case tables.
void initchars(void) {
	int index;

	// Set all of both tables to their indices.
	for(index = 0; index < HiChar; ++index)
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

// Reverse string in place and return it.
char *strrev(char *s) {
	char *strz = strchr(s,'\0');
	if(strz - s > 1) {
		int c;
		char *str = s;
		do {
			c = *--strz;
			*strz = *str;
			*str++ = c;
			} while(strz > str);
		}
	return s;
	}

// If default n, display the current line, column, and character position of the point in the current buffer, the fraction of
// the text that is before the point, and the character that is at point (in printable form and hex).  If n is not the
// default, display the point column and the character at point only.  The displayed column is not the current column,
// but the column on an infinite-width display.  (Interactive only)
int showPoint(Datum *rp,int n,Datum **argpp) {
	Line *lnp;			// Current line.
	ulong numchars;			// # of chars in file.
	ulong numlines;			// # of lines in file.
	ulong predchars;		// # chars preceding point.
	ulong predlines;		// # lines preceding point.
	short curchar;			// Character at point.
	double ratio;
	int col;
	int ecol;			// Column pos/end of current line.
	Dot dot;
	Dot *dotp = &curwp->w_face.wf_dot;
	char *str,wkbuf1[32],wkbuf2[32];

	// Skip this if not displaying messages.
	if(!(modetab[MdIdxGlobal].flags & MdMsg))
		return rc.status;

	if(n == INT_MIN) {

		// Starting at the beginning of the buffer.
		lnp = curbp->b_hdrlnp->l_nextp;
		curchar = '\n';

		// Start counting chars and lines.
		numchars = numlines = 0;
		while(lnp != curbp->b_hdrlnp) {

			// If we are on the current line, record it.
			if(lnp == dotp->lnp) {
				predlines = numlines;
				predchars = numchars + dotp->off;
				curchar = (dotp->off == lnp->l_used) ? '\n' : lnp->l_text[dotp->off];
				}

			// On to the next line.
			++numlines;
			numchars += lnp->l_used + 1;
			lnp = lnp->l_nextp;
			}

		// If dot is at end of buffer, record it.
		if(dotp->lnp == curbp->b_hdrlnp) {
			predlines = numlines;
			predchars = numchars;
			}

		ratio = 0.0;				// Ratio before dot.
		if(numchars > 0)
			ratio = (double) predchars / numchars * 100.0;
		sprintf(str = wkbuf2,"%.1f",ratio);
		if(numchars > 0) {

			// Fix rounding errors at buffer boundaries.
			if(predchars > 0 && strcmp(str,"0.0") == 0)
				str = "0.1";
			else if(predchars < numchars && strcmp(str,"100.0") == 0)
				str = "99.9";
			}
		}
	else
		curchar = (dotp->off == dotp->lnp->l_used) ? '\n' : dotp->lnp->l_text[dotp->off];

	// Get real column and end-of-line column.
	col = getccol(NULL);
	dot.lnp = dotp->lnp;
	dot.off = dotp->lnp->l_used;
	ecol = getccol(&dot);

	// Summarize and report the info.
	if(curchar >= ' ' && curchar < 0x7F)
		sprintf(wkbuf1,"'%c' 0x%.2hX",(int) curchar,curchar);
	else
		sprintf(wkbuf1,"0x%.2hX",curchar);
	return (n == INT_MIN) ? rcset(Success,0,text60,predlines + 1,numlines,col,ecol,predchars,numchars,str,wkbuf1) :
					// "Line %lu/%lu, Col %d/%d, Char %lu/%lu (%s%%), char = %s"
	 rcset(Success,0,text340,col,ecol,wkbuf1);
		// "Col %d/%d, char = %s"
	}

// Get line number, given buffer and line pointer.
long getlinenum(Buffer *bufp,Line *targlnp) {
	Line *lnp;
	long n;

	// Starting at the beginning of the buffer.
	lnp = bufp->b_hdrlnp->l_nextp;

	// Start counting lines.
	n = 0;
	while(lnp != bufp->b_hdrlnp) {

		// If we have reached the target line, stop...
		if(lnp == targlnp)
			break;
		++n;
		lnp = lnp->l_nextp;
		}

	// and return the count.
	return n + 1;
	}

// Return new column, given character c and old column.
int newcol(short c,int col) {

	if(c == '\t')
		col += -(col % htabsize) + (htabsize - 1);
	else if(c < 0x20 || c == 0x7F)
		++col;
	else if(c > 0x7F)
		col += 3;
	return col + 1;
	}

// Return current column of given dot position.  If dotp is NULL, use point.
int getccol(Dot *dotp) {
	int i,col;

	if(dotp == NULL)
		dotp = &curwp->w_face.wf_dot;

	col = 0;
	for(i = 0; i < dotp->off; ++i)
		col = newcol(dotp->lnp->l_text[i],col);
	return col;
	}

// Try to set current column to given position.  Return status.
int setccol(int pos) {
	int i;			// Index into current line.
	int col;		// Current point column.
	int llen;		// Length of line in bytes.
	Dot *dotp = &curwp->w_face.wf_dot;

	col = 0;
	llen = dotp->lnp->l_used;

	// Scan the line until we are at or past the target column.
	for(i = 0; i < llen; ++i) {

		// Upon reaching the target, drop out.
		if(col >= pos)
			break;

		// Advance one character.
		col = newcol(dotp->lnp->l_text[i],col);
		}

	// Set point to the new position and return.
	dotp->off = i;
	return rc.status;
	}

// Check if all white space from beginning of line, given length.  Return boolean result, including true if length is zero.
bool is_white(Line *lnp,int length) {
	short c;
	int i;

	for(i = 0; i < length; ++i) {
		c = lnp->l_text[i];
		if(c != ' ' && c != '\t')
			return false;
		}
	return true;
	}

// Match closing fences against their partners, and if on screen, briefly light the cursor there.
int fmatch(short c) {
	Dot odot;		// Original line pointer and offset.
	Line *toplp;		// Top line in current window.
	int count;		// Current fence level count.
	short opench;		// Open fence.
	short c1;		// Current character in scan.
	WindFace *wfp = &curwp->w_face;

	// Skip this if executing a script or a keyboard macro.
	if((opflags & OpScript) || kmacro.km_state == KMPlay)
		return rc.status;

	// First get the display update out there.
	if(update(INT_MIN) != Success)
		return rc.status;

	// Save the original point position.
	odot = wfp->wf_dot;

	// Set up proper open fence for passed close fence.
	switch(c) {
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
	toplp = wfp->wf_toplnp->l_prevp;
	count = 1;
	(void) backch(1);

	// Scan back until we find it, or move past the top of the window.
	while(count > 0 && wfp->wf_dot.lnp != toplp) {
		(void) backch(1);
		if(wfp->wf_dot.off == wfp->wf_dot.lnp->l_used)
			c1 = '\n';
		else
			c1 = wfp->wf_dot.lnp->l_text[wfp->wf_dot.off];
		if(c1 == c)
			++count;
		else if(c1 == opench)
			--count;
		if(wfp->wf_dot.lnp == curbp->b_hdrlnp->l_nextp && wfp->wf_dot.off == 0)
			break;
		}

	// If count is zero, we have a match -- display the sucker.
	if(count == 0) {
		if(update(INT_MIN) != Success)
			return rc.status;
		cpause(fencepause);
		}

	// Restore the previous position.
	wfp->wf_dot = odot;

	return rc.status;
	}

#if WordCount
// Count the number of words in the marked region, along with average word sizes, number of chars, etc, and report on them
// (interactive only).
int countWords(Datum *rp,int n,Datum **argpp) {
	Line *lnp;		// Current line to scan.
	int offset;		// Current char to scan.
	long size;		// Size of region left to count.
	short c;		// Current character to scan.
	bool wordflag;		// Is current character a word character?
	bool inword;		// Are we in a word now?
	long nwords;		// Total number of words.
	long nchars;		// Total number of word characters.
	int nlines;		// Total number of lines in region.
	Region region;		// Region to look at.

	// Skip this if not displaying messages.
	if(!(modetab[MdIdxGlobal].flags & MdMsg))
		return rc.status;

	// Make sure we have a region to count.
	if(getregion(&region,NULL) != Success)
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
		if(offset == lnp->l_used) {	// End of line.
			c = '\n';
			lnp = lnp->l_nextp;
			offset = 0;

			++nlines;
			}
		else {
			c = lnp->l_text[offset];
			++offset;
			}

		// and tabulate it.
		if((wordflag = isletter(c) || isdigit(c)))
			++nchars;
		if(wordflag && !inword)
			++nwords;
		inword = wordflag;
		}

	// and report on the info.
	return rcset(Success,0,text100,nwords,nchars,region.r_size,nlines + 1,(nwords > 0) ? nchars * 1.0 / nwords : 0.0);
		// "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f"
	}
#endif

// Set or display i-variable parameters.  If n < 0, display parameters on message line; if n >= 0; set first parameter to n
// (only); otherwise, get arguments.  Return status.
int seti(Datum *rp,int n,Datum **argpp) {
	int i = ivar.i;
	int inc = ivar.inc;
	bool newfmt = false;
	Datum *datp;

	// If script mode and not evaluating, nothing to do.
	if((opflags & (OpScript | OpEval)) == OpScript)
		return rc.status;

	// Not script mode or evaluating.  n argument?
	if(n != INT_MIN) {
		if(n >= 0) {
			ivar.i = n;
			return rcset(Success,0,text287,ivar.i);
					// "i variable set to %d"
			}
		return rcset(Success,RCNoWrap,text384,ivar.i,ivar.inc,ivar.format.d_str);
				// "i = %d, inc = %d, format = '%s'"
		}

	// Default n.  Get value(s).
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	if(opflags & OpScript) {

		// Get "i" argument.
		i = (*argpp++)->u.d_int;

		// Have "inc" argument?
		if(*argpp != NULL) {

			// Yes, get it.
			inc = (*argpp++)->u.d_int;

			// Have "format" argument?
			if(*argpp != NULL) {

				// Yes, get it.
				datxfer(datp,*argpp);
				newfmt = true;
				}
			}
		}
	else {
		TermInp ti = {"1",RtnKey,0,NULL};
		char nbuf[LongWidth];

		// Prompt for "i" value.
		if(terminp(datp,text102,0,0,&ti) != Success || toint(datp) != Success)
				// "Beginning value"
			return rc.status;
		i = datp->u.d_int;

		// Prompt for "inc" value.
		sprintf(nbuf,"%d",inc);
		ti.defval = nbuf;
		if(terminp(datp,text234,0,0,&ti) != Success || toint(datp) != Success)
				// "Increment"
			return rc.status;
		inc = datp->u.d_int;

		// Prompt for "format" value.
		ti.defval = ivar.format.d_str;
		ti.delim = Ctrl | '[';
		if(terminp(datp,text235,CFNotNull1,0,&ti) != Success)
				// "Format string"
			return rc.status;
		newfmt = true;
		}

	// Validate arguments.
	if(inc == 0)				// Zero increment.
		return rcset(Failure,RCNoFormat,text236);
			// "i increment cannot be zero!"

	// Validate format string if changed.
	if(newfmt) {
		if(strcmp(datp->d_str,ivar.format.d_str) == 0)
			newfmt = false;
		else {
			int c,icount,ocount;
			bool inspec = false;
			char *str = datp->d_str;

			icount = ocount = 0;
			do {
				c = *str++;
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
				} while(*str != '\0');

			if(icount != 1 || ocount > 0)		// Bad format string.
				return rcset(Failure,0,text237,datp->d_str);
					// "Invalid i format '%s' (must contain exactly one %%d, %%o, %%u, %%x, or %%X)"
			}
		}

	// Passed all edits... update ivar.
	ivar.i = i;
	ivar.inc = inc;
	if(newfmt)
		datxfer(&ivar.format,datp);

	return rc.status;
	}

// Return a pseudo-random integer in range 1..max.  If max <= 0, return zero.  This is a slight variation of the Xorshift
// pseudorandom number generator discovered by George Marsaglia.
long xorshift64star(long max) {
	if(max <= 0)
		return 0;
	else if(max == 1)
		return 1;
	else {
		randseed ^= randseed >> 12; // a
		randseed ^= randseed << 25; // b
		randseed ^= randseed >> 27; // c
		}
	return ((randseed * 0x2545F4914F6CDD1D) & LONG_MAX) % max + 1;
	}

// Get an apropos match string with a null default.  Convert a nil argument to null as well.  Return status.
static int getamatch(ShowCtrl *scp,char *prmt,Datum **argpp) {
	Datum *mstrp = &scp->sc_mstr;

	if(!(opflags & OpScript)) {
		char wkbuf[NWork + 1];

		sprintf(wkbuf,"%s %s",text20,prmt);
				// "Apropos"
		if(terminp(mstrp,wkbuf,CFNil1,0,NULL) != Success)
			return rc.status;
		if(mstrp->d_type == dat_nil)
			dsetnull(mstrp);
		}
	else if(argpp[0]->d_type == dat_nil)
		dsetnull(mstrp);
	else
		datxfer(mstrp,argpp[0]);

	// Set up match record if pattern given.  Force "ignore" if non-RE and non-Exact.
	if(mstrp->d_type != dat_nil && !disnull(mstrp) && newspat(mstrp->d_str,&scp->sc_match,NULL) == Success) {
		if(scp->sc_match.flags & SOpt_Regexp) {
			if(mccompile(&scp->sc_match) != Success)
				freespat(&scp->sc_match);
			}
		else if(!(scp->sc_match.flags & SOpt_Exact))
			scp->sc_match.flags |= SOpt_Ignore;
		}

	return rc.status;
	}

// Initialize a ShowCtrl object for a "show" listing.  If default n, assume a full list; otherwise, get a match string and save
// in control object for later.  Return status.
int showopen(ShowCtrl *scp,int n,char *plabel,Datum **argpp) {
	char *str,wkbuf[strlen(plabel) + 3];

	dinit(&scp->sc_name);
	dinit(&scp->sc_value);
	dinit(&scp->sc_mstr);
	minit(&scp->sc_match);

	// If not default n, get match string.
	if(n != INT_MIN && getamatch(scp,plabel,argpp) != Success)
		return rc.status;

	// Create a buffer name (plural of plabel) and get a buffer.
	str = stpcpy(wkbuf,plabel);
	wkbuf[0] = upcase[(int) wkbuf[0]];
	strcpy(str,str[-1] == 's' ? "es" : "s");
	return sysbuf(wkbuf,&scp->sc_listp,BFTermAttr);
	}

// Copy src to dest in upper case, inserting a space between each two characters.
static char *expand(char *dest,char *src) {

	do {
		*dest++ = upcase[(int) *src++];
		*dest++ = ' ';
		} while(*src != '\0');
	return dest;
	}

// Write header lines to an open string-fab object, given report title.  Return status.
static int showhdr(ShowCtrl *scp,char *title) {
	char *str;
	char titlebuf[strlen(title) * 2 + 5];
	char wkbuf[term.t_ncol + 1];

	// Write separator line and centered title line.
	dupchr(wkbuf,'=',term.t_ncol);
	if(dputs(wkbuf,&scp->sc_rpt) != 0 || dputc('\n',&scp->sc_rpt) != 0)
		return librcset(Failure);
	str = expand(titlebuf,title);
	str = expand(str,str[-2] == 'S' ? "es" : "s");
	str[-1] = '\0';
	str = stpcpy(dupchr(wkbuf,' ',(term.t_ncol - strlen(titlebuf)) / 2),"~b");
	strcpy(stpcpy(str,titlebuf),"~0");
	if(dputs(wkbuf,&scp->sc_rpt) != 0)
		(void) librcset(Failure);

	return rc.status;
	}

// Build a "show" listing in a report buffer, given ShowCtrl object, flags, section title (which may be NULL), and pointer to
// routine which sets the name + usage, value (if applicable), and description for the next list item in the ShowCtrl object.
// Return status.
int showbuild(ShowCtrl *scp,ushort flags,char *title,int (*fp)(ShowCtrl *scp,ushort req,char **namep)) {
	char *nametab[] = {NULL,NULL,NULL};
	char **namep,*str0,*str,*strz;
	bool firstItem = true;
	bool doApropos = scp->sc_mstr.d_type != dat_nil;
	Datum *indexp,*srcp;
	char sepline[term.t_ncol + 1];
	char wkbuf[NWork];

	// Initialize.
	scp->sc_itemp = NULL;
	if(flags & SHNoDesc)
		scp->sc_desc = NULL;
	if(doApropos && !(flags & SHExact)) {
		if(dnewtrk(&indexp) != 0 || dnewtrk(&srcp) != 0)
			return librcset(Failure);
		}

	// Open a string-fab object and write section header if applicable.
	if(dopentrk(&scp->sc_rpt) != 0)
		return librcset(Failure);
	if(title != NULL && showhdr(scp,title) != Success)
		return rc.status;

	// Loop through detail items.
	dupchr(sepline,'-',term.t_ncol);
	for(;;) {
		// Find next item and get its name.  Exit loop if no items left.
		if(fp(scp,SHReqNext,nametab) != Success)
			return rc.status;
		if(nametab[0] == NULL)
			break;

		// Skip if apropos in effect and item name doesn't match the search pattern.
		if(doApropos) {
			if(flags & SHExact) {
				if(strcmp(nametab[0],scp->sc_mstr.d_str) != 0)
					continue;
				}
			else if(!disnull(&scp->sc_mstr)) {
				namep = nametab;
				do {
					if(dsetstr(*namep++,srcp) != 0)
						return librcset(Failure);
					if(sindex(indexp,srcp,&scp->sc_mstr,&scp->sc_match,false) != Success)
						return rc.status;
					if(indexp->d_type != dat_nil)
						goto MatchFound;
					} while(*namep != NULL);
				continue;
				}
			}
MatchFound:
		// Get item usage and description, and "has value" flag (in nametab[0] slot).
		if(fp(scp,SHReqUsage,nametab) != Success)
			return rc.status;

		// Begin next line.
		if(((flags & SHSepLine) || firstItem) && (dputc('\n',&scp->sc_rpt) != 0 || dputs(sepline,&scp->sc_rpt) != 0))
			return librcset(Failure);
		firstItem = false;

		// Store item name and value, if any, in work buffer and add line to report.
		sprintf(wkbuf,"~b%s~0",scp->sc_name.d_str);
		if(nametab[0] != NULL) {
			str = pad(wkbuf,34);
			if(str[-2] != ' ')
				strcpy(str,str[-1] == ' ' ? " " : "  ");
			}
		if(dputc('\n',&scp->sc_rpt) != 0 || dputs(wkbuf,&scp->sc_rpt) != 0)
			return librcset(Failure);
		if(nametab[0] != NULL && fp(scp,SHReqValue,NULL) != Success)
			return rc.status;

		// Store indented description, if present and not blank.  Wrap into as many lines as needed.  May contain
		// terminal attribute sequences.
		if(scp->sc_desc != NULL) {
			int len;
			str0 = scp->sc_desc;
			for(;;) {						// Skip leading white space.
				if(*str0 != ' ')
					break;
				++str0;
				}
			strz = strchr(str0,'\0');
			for(;;) {						// Wrap loop.
				len = attrCount(str0,strz - str0,term.t_ncol - 4);
				if(strz - str0 - len <= term.t_ncol - 4)	// Remainder too long?
					str = strz;				// No.
				else {						// Yes, find space to break on.
					str = str0 + len + term.t_ncol - 4;
					for(;;) {
						if(*--str == ' ')
							break;
						if(str == str0) {
							str += len + term.t_ncol - 4;
							break;
							}
						}
					}
				if(dputc('\n',&scp->sc_rpt) != 0 || dputs("    ",&scp->sc_rpt) != 0 ||
				 dputmem((void *) str0,str - str0,&scp->sc_rpt) != 0)
					return librcset(Failure);
				if(str == strz)
					break;
				str0 = *str == ' ' ? str + 1 : str;
				}
			}
		}

	// Close string-fab object and append string to report buffer if any items were written.
	if(dclose(&scp->sc_rpt,sf_string) != 0)
		return librcset(Failure);
	if(!firstItem) {

		// Write blank line if title not NULL and buffer not empty.
		if(title != NULL && scp->sc_listp->b_hdrlnp->l_nextp != scp->sc_listp->b_hdrlnp &&
		 bappend(scp->sc_listp,"") != Success)
			return rc.status;

		// Append section detail.
		if(bappend(scp->sc_listp,scp->sc_rpt.sf_datp->d_str) != Success)
			return rc.status;
		}

	return rc.status;
	}

// Close a "show" listing.  Return status.
int showclose(Datum *rp,int n,ShowCtrl *scp) {

	dclear(&scp->sc_name);
	dclear(&scp->sc_value);
	dclear(&scp->sc_mstr);
	if(scp->sc_match.ssize > 0)
		freespat(&scp->sc_match);

	// Display the list.
	return render(rp,n,scp->sc_listp,RendNewBuf | RendReset);
	}

// Get name, usage, and key bindings (if any) for current list item (command, function, or macro) and save in report-control
// object.  Return status.
int findkeys(ShowCtrl *scp,uint ktype,void *tp) {
	KeyWalk kw = {NULL,NULL};
	KeyDesc *kdp;
	Buffer *bufp;
	CmdFunc *cfp;
	MacInfo *mip = NULL;
	char *name,*usage;
	char keybuf[16];

	// Set pointers and item description.
	if(ktype & PtrMacro) {
		bufp = (Buffer *) tp;
		name = bufp->b_bname + 1;
		mip = bufp->b_mip;
		if(mip != NULL) {
			usage = (mip->mi_usage.d_type != dat_nil) ? mip->mi_usage.d_str : NULL;
			scp->sc_desc = (mip->mi_desc.d_type != dat_nil) ? mip->mi_desc.d_str : NULL;
			}
		else
			scp->sc_desc = usage = NULL;
		}
	else {
		cfp = (CmdFunc *) tp;
		name = cfp->cf_name;
		usage = cfp->cf_usage;
		scp->sc_desc = cfp->cf_desc;
		}

	// Set item name and usage.
	if(usage == NULL) {
		if(dsetstr(name,&scp->sc_name) != 0)
			return librcset(Failure);
		}
	else {
		char wkbuf[strlen(name) + strlen(usage) + 1];
		sprintf(wkbuf,"%s %s",name,usage);
		if(dsetstr(wkbuf,&scp->sc_name) != 0)
			return librcset(Failure);
		}

	// Set key bindings, if any.
	if(ktype & PtrFunc)
		dclear(&scp->sc_value);
	else {
		DStrFab sf;
		char *sep = NULL;

		// Search for any keys bound to command or macro (buffer) "tp".
		if(dopenwith(&sf,&scp->sc_value,SFClear) != 0)
			return librcset(Failure);
		kdp = nextbind(&kw);
		do {
			if((kdp->k_cfab.p_type & ktype) && kdp->k_cfab.u.p_voidp == tp) {

				// Add the key sequence.
				ektos(kdp->k_code,keybuf,true);
				if((sep != NULL && dputs(sep,&sf) != 0) || dputf(&sf,"~#u%s~U",keybuf) != 0)
					return librcset(Failure);
				sep = ", ";
				}
			} while((kdp = nextbind(&kw)) != NULL);
		if(dclose(&sf,sf_string) != 0)
			(void) librcset(Failure);
		}

	return rc.status;
	}

// Get next command or function name or description and store in report-control object.  If req is SHReqNext, set *namep to NULL
// if no items left; otherwise, pointer to its name.  Return status.
static int nextCmdFunc(ShowCtrl *scp,ushort req,char **namep,ushort aflags) {
	CmdFunc *cfp;

	// First call?
	if(scp->sc_itemp == NULL)
		scp->sc_itemp = (void *) (cfp = cftab);
	else {
		cfp = (CmdFunc *) scp->sc_itemp;
		if(req == SHReqNext)
			++cfp;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; cfp->cf_name != NULL; ++cfp) {
	
				// Skip if wrong type.
				if((cfp->cf_aflags & CFFunc) != aflags)
					continue;

				// Found item... return its name.
				*namep = cfp->cf_name;
				scp->sc_itemp = (void *) cfp;
				goto Retn;
				}

			// End of table.
			*namep = NULL;
			break;
		case SHReqUsage:
			if(findkeys(scp,aflags ? PtrFunc : PtrCmdType,(void *) cfp) != Success)
				return rc.status;
			*namep = (scp->sc_value.d_type == dat_nil) ? NULL : cfp->cf_name;
			break;
		default: // SHReqValue
			if(dputs(scp->sc_value.d_str,&scp->sc_rpt) != 0)
				(void) librcset(Failure);
		}
Retn:
	return rc.status;
	}

// Get next command name and description and store in report-control object via call to nextCmdFunc().
int nextCommand(ShowCtrl *scp,ushort req,char **namep) {

	return nextCmdFunc(scp,req,namep,0);
	}

// Create formatted list of commands via calls to "show" routines.  Return status.
int showCommands(Datum *rp,int n,Datum **argpp) {
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,n,text158,argpp) == Success && showbuild(&sc,SHSepLine,text158,nextCommand) == Success)
			// "command"
		(void) showclose(rp,n,&sc);
	return rc.status;
	}

// Get next function name and description and store in report-control object via call to nextCmdFunc().
static int nextFunction(ShowCtrl *scp,ushort req,char **namep) {

	return nextCmdFunc(scp,req,namep,CFFunc);
	}

// Create formatted list of system functions via calls to "show" routines.  Return status.
int showFunctions(Datum *rp,int n,Datum **argpp) {
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,n,text247,argpp) == Success && showbuild(&sc,SHSepLine,text247,nextFunction) == Success)
			// "function"
		(void) showclose(rp,n,&sc);
	return rc.status;
	}

// Get next macro name or description and store in report-control object.  If req is SHReqNext, set *namep to NULL if no items
// left; otherwise, pointer to its name.  Return status.
int nextMacro(ShowCtrl *scp,ushort req,char **namep) {
	Buffer *bufp;

	// First call?
	if(scp->sc_itemp == NULL)
		scp->sc_itemp = (void *) (bufp = bheadp);
	else {
		bufp = (Buffer *) scp->sc_itemp;
		if(req == SHReqNext)
			bufp = bufp->b_nextp;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; bufp != NULL; bufp = bufp->b_nextp) {

				// Skip if not a macro.
				if(!(bufp->b_flags & BFMacro))
					continue;

				// Found macro... return its name.
				*namep = bufp->b_bname + 1;
				scp->sc_itemp = (void *) bufp;
				goto Retn;
				}

			// End of list.
			*namep = NULL;
			break;
		case SHReqUsage:
			if(findkeys(scp,PtrMacro,(void *) bufp) != Success)
				return rc.status;
			*namep = (scp->sc_value.d_type == dat_nil) ? NULL : bufp->b_bname;
			break;
		default: // SHReqValue
			if(dputs(scp->sc_value.d_str,&scp->sc_rpt) != 0)
				(void) librcset(Failure);
		}
Retn:
	return rc.status;
	}

// Create formatted list of macros via calls to "show" routines.  Return status.
int showMacros(Datum *rp,int n,Datum **argpp) {
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,n,text336,argpp) == Success && showbuild(&sc,SHSepLine,text336,nextMacro) == Success)
			// "macro"
		(void) showclose(rp,n,&sc);
	return rc.status;
	}

// Get next alias name or description and store in report-control object.  If req is SHReqNext, set *namep to NULL if no items
// left; otherwise, pointer to its name.  Return status.
static int nextAlias(ShowCtrl *scp,ushort req,char **namep) {
	Alias *ap;

	// First call?
	if(scp->sc_itemp == NULL)
		scp->sc_itemp = (void *) (ap = aheadp);
	else {
		ap = (Alias *) scp->sc_itemp;
		if(req == SHReqNext)
			ap = ap->a_nextp;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; ap != NULL; ap = ap->a_nextp) {

				// Found alias... return its name and name it points to.
				*namep++ = ap->a_name;
				*namep = (ap->a_type == PtrAlias_M ? ap->a_cfab.u.p_bufp->b_bname :
				 ap->a_cfab.u.p_cfp->cf_name);
				scp->sc_itemp = (void *) ap;
				goto Retn;
				}

			// End of list.
			*namep = NULL;
			break;
		case SHReqUsage:
			if(dsetstr(ap->a_name,&scp->sc_name) != 0)
				return librcset(Failure);
			*namep = ap->a_name;
			break;
		default: // SHReqValue
			{char *name2 = (ap->a_cfab.p_type == PtrMacro) ? ap->a_cfab.u.p_bufp->b_bname :
			 ap->a_cfab.u.p_cfp->cf_name;
			if(dputs("-> ",&scp->sc_rpt) != 0 || dputs(name2,&scp->sc_rpt) != 0)
				return librcset(Failure);
			}
		}
Retn:
	return rc.status;
	}

// Create formatted list of aliases via calls to "show" routines.  Return status.
int showAliases(Datum *rp,int n,Datum **argpp) {
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,n,text127,argpp) == Success && showbuild(&sc,SHNoDesc,text127,nextAlias) == Success)
			// "alias"
		(void) showclose(rp,n,&sc);
	return rc.status;
	}

// Create formatted list of commands, macros, functions, aliases, and variables which match a pattern via calls to "show"
// routines.  Return status.
int apropos(Datum *rp,int n,Datum **argpp) {
	ShowCtrl sc;

	// Open control object.
	if(n == INT_MIN)
		n = -1;
	if(showopen(&sc,n,Literal4,argpp) == Success) {
			// "name"

		// Call the various show routines and build the list.
		if(showbuild(&sc,SHSepLine,text158,nextCommand) == Success &&
						// "command"
		 showbuild(&sc,SHSepLine,text336,nextMacro) == Success &&
						// "macro"
		 showbuild(&sc,SHSepLine,text247,nextFunction) == Success &&
						// "function"
		 showbuild(&sc,SHNoDesc,text127,nextAlias) == Success &&
						// "alias"
		 showbuild(&sc,SHSepLine,text21,nextSysVar) == Success &&
						// "system variable"
		 showbuild(&sc,SHNoDesc,text56,nextGlobalVar) == Success &&
		 showbuild(&sc,SHNoDesc,NULL,nextLocalVar) == Success)
						// "user variable"
			(void) showclose(rp,n,&sc);
		}
	return rc.status;
	}

// Do include? system function.  Return status.
int doincl(Datum *rp,int n,Datum **argpp) {
	bool result;
	Array *aryp = awptr(argpp[0])->aw_aryp;
	ArraySize elct;
	Datum *datp,**elpp;
	bool aryMatch,allMatch;
	bool any = (n == INT_MIN || n == 0);
	bool ignore = (n <= 0 && n != INT_MIN);
	uint aflags = CFNIS1 | CFBool1 | CFArray1;

	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	result = !any;
	for(;;) {
		// Get next argument.
		if(!(aflags & Arg_First) && !havesym(s_comma,false))
			break;					// At least one argument retrieved and none left.
		if(funcarg(datp,aflags) != Success)
			return rc.status;

		// Loop through array elements and compare them to the argument if final result has not yet been
		// determined.
		if(result == !any) {
			elct = aryp->a_used;
			elpp = aryp->a_elpp;
			allMatch = false;
			while(elct-- > 0) {
				if((*elpp)->d_type == dat_blobRef) {
					if(datp->d_type != dat_blobRef)
						goto NextEl;
					if(aryeq(datp,*elpp,&aryMatch) != Success)
						return rc.status;
					if(aryMatch)
						goto Match;
					goto NextEl;
					}
				else if(dateq(datp,*elpp,ignore)) {
Match:
					if(any)
						result = true;
					else
						allMatch = true;
					break;
					}
NextEl:
				++elpp;
				}

			// Match found or all array elements checked.  Fail if "all" mode and no match found.
			if(!any && !allMatch)
				result = false;
			}
		}

	dsetbool(result,rp);
	return rc.status;
	}

// Check if a partial (or full) mode name is unique.  Set *mspp to modeinfo table entry if true; otherwise, NULL.
// Return status.
int modecmp(char *kw,int type,ModeSpec **mspp,bool errorIfNot) {
	ModeSpec *msp0 = NULL,*msp = modeinfo;
	size_t len = strlen(kw);
	uint modetype = (type == MdIdxGlobal) ? MdGlobal : 0;
	do {
		// Skip if wrong type of mode.
		if(type != MdIdxShow && (msp->mask & MdGlobal) != modetype)
			continue;

		if(strncasecmp(kw,msp->name,len) == 0) {

			// Exact match?
			if(msp->name[len] == '\0') {
				msp0 = msp;
				goto Retn;
				}

			// Error if not first match.
			if(msp0 != NULL)
				goto ERetn;
			msp0 = msp;
			}
		} while((++msp)->name != NULL);

	if(errorIfNot && msp0 == NULL)
ERetn:
		return rcset(Failure,0,text66,kw);
			// "Unknown or ambiguous mode '%s'"
Retn:
	*mspp = msp0;
	return rc.status;
	}

// Retrieve modes matching bit masks in "flags" and save as a list of keywords in *rp.  Return status.
int getmodes(Datum *rp,uint flags) {
	Datum *datp;
	Array *aryp;

	if((aryp = anew(0,NULL)) == NULL)
		(void) librcset(Failure);
	else if(awrap(rp,aryp) == Success) {
		if(dnewtrk(&datp) != 0)
			(void) librcset(Failure);
		else {
			ModeSpec *msp = modeinfo;
			do {
				if(flags & msp->mask & ~MdGlobal)
					if(dsetstr(msp->mlname,datp) != 0 || apush(aryp,datp) != 0)
						return librcset(Failure);
				} while((++msp)->name != NULL);
			}
		}
	return rc.status;
	}

// Change a mode, given keyword pointer, action (< 0: clear, 0: toggle, > 0: set), type (MdIdxGlobal=0: global, MdIdxShow=1:
// show, MdIdxBuffer=2: buffer), pointer to flag word to update, optional "former state" pointer, and optional result pointer
// (mode bit mask).  Return status.
int domode(char *kw,int action,int type,uint *flagp,long *formerStatep,uint *resultp) {
	ModeSpec *msp;
	uint mask;

	// Test it against the modes we know.
	if(modecmp(kw,type,&msp,true) != Success)
		return rc.status;
	mask = msp->mask & ~MdGlobal;

	// Match found... validate mode and process it.
	if(type == MdIdxGlobal && (mask & MdASave) && gasave == 0 && (action > 0 || (action == 0 && !(*flagp & mask))))
		return rcset(Failure,RCNoFormat,text35);
			// "$autoSave not set"
	if(formerStatep != NULL)
		*formerStatep = (*flagp & mask) ? 1L : -1L;
	if(action < 0)
		*flagp &= ~mask;
	else if(action > 0)
		*flagp |= mask;
	else
		*flagp ^= mask;

	// Ensure mutually-exclusive bits are not set.
	if(type == MdIdxBuffer) {
		if((mask & MdGrp_Repl) && (*flagp & MdGrp_Repl))
			*flagp = (*flagp & ~MdGrp_Repl) | mask;
		else if((mask & MdGrp_Lang) && (*flagp & MdGrp_Lang))
			*flagp = (*flagp & ~MdGrp_Lang) | mask;
		}
	if(resultp != NULL)
		*resultp = mask;
	return rc.status;
	}

// Build portion of mode-change prompt and write to active string-fab object; e.g., "buffer mode".  Return -1 if error;
// otherwise, 0.
static int modeprmt(DStrFab *sfp,int type,ModeRec *mrp) {

	if(type < MdIdxBuffer) {
		if(dputc(' ',sfp) != 0 || dputc(lowcase[(int) *mrp->cmdlabel],sfp) != 0 || dputs(mrp->cmdlabel + 1,sfp) != 0)
			return -1;
		}
	else if(dputc(' ',sfp) != 0 || dputs(text83,sfp) != 0)
					// "buffer"
		return -1;
	if(dputs(text63,sfp) != 0)
			// " mode"
		return -1;
	return 0;
	}

// Change a mode, given result pointer, action (clear all: n < -1, clear: n == -1, toggle: n == 0 or default, set: n == 1, or
// clear all and set: n > 1), type (MdIdxGlobal=0: global, MdIdxShow=1: show, MdIdxBuffer=2: buffer), and optional buffer
// pointer (for type 2).  Set *rp it to the state (-1 or 1) of the last mode altered.  Return status.
int alterMode(Datum *rp,int n,int type,Buffer *bufp) {
	Datum *datp,*argp;
	long formerState = 0;			// Usually -1 or 1.
	int action = (n == INT_MIN) ? 0 : n;
	uint mask;
	ModeRec *mrp;
	uint *mp;				// Pointer to mode-flags word to change.
	uint oldmodes;				// Old mode flags.
	long oldflags[3],*ofp = oldflags;

	// Get ready.
	mrp = (type == MdIdxBuffer) ? NULL : modetab + type;
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);

	// If interactive mode, build the proper prompt string; e.g. "Toggle global mode: "
	if(!(opflags & OpScript)) {
		if(action >= -1) {
			DStrFab prompt;

			// Build prompt.
			if(dopenwith(&prompt,datp,SFClear) != 0 ||
			 dputs(action < 0 ? text65 : action == 0 ? text231 : action == 1 ? text64 : text296,&prompt) != 0 ||
					// "Clear","Toggle","Set","Clear all and set"
			 modeprmt(&prompt,type,mrp) != 0 || dclose(&prompt,sf_string) != 0)
				return librcset(Failure);

			// Get mode name from user.
			if(terminp(datp,datp->d_str,CFNil1,type == MdIdxGlobal ? Term_C_GMode :
			 type == MdIdxShow ? Term_C_GMode | Term_C_BMode : Term_C_BMode,NULL) != Success ||
			 datp->d_type == dat_nil)
				return rc.status;
			}

		// Get buffer, if needed.
		if(type == MdIdxBuffer) {
			if(n == INT_MIN)
				bufp = curbp;
			else {
				bufp = bdefault(BDefTwo);
				if(bcomplete(rp,action < -1 ? text146 : text229 + 2,bufp != NULL ? bufp->b_bname : NULL,
				 OpDelete,&bufp,NULL) != Success || bufp == NULL)
						// "Clear modes in",", in"
					return rc.status;
				}
			}
		}

	// Have buffer and, if interactive and action >= -1, one mode name in datp.  Prepare to process modes.
	mp = mrp ? &mrp->flags : &bufp->b_modes;
	oldmodes = *mp;

	// Save current modes so they can be passed to mode hook, if set.
	mrp = modetab;
	do {
		*ofp++ = (mrp++)->flags;
		} while(mrp->cmdlabel != NULL);
	*ofp = (bufp == NULL ? curbp : bufp)->b_modes;

	// Clear all modes initially, if applicable.
	if(action < -1 || action > 1)
		*mp = 0;

	// Do "clear all" special case.
	if(action < -1) {
		if(!(opflags & OpScript)) {
			DStrFab msg;

			if(dopenwith(&msg,datp,SFClear) != 0 || dputs(text31,&msg) != 0 || modeprmt(&msg,type,mrp) != 0 ||
								// "All"
			 dputs(text62,&msg) != 0 || dclose(&msg,sf_string) != 0)
					// "s cleared"
				return librcset(Failure);
			(void) rcset(Success,RCNoFormat,datp->d_str);
			}
		}
	else if(!(opflags & OpScript)) {
		argp = datp;
		goto DoMode;
		}
	else {
		Datum **elpp = NULL;
		int status;
		uint aflags = Arg_First | CFNotNull1 | CFArray1 | CFMay;

		// Script mode: get one or more arguments.
		do {
			if((status = nextarg(&argp,&aflags,datp,&elpp,&n)) == NotFound)
				break;					// No arguments left.
			if(status != Success)
				return rc.status;
DoMode:
			if(domode(argp->d_str,action,type,mp,&formerState,&mask) != Success)
				return rc.status;

			// Do special processing for specific global modes that changed.
			if(type == MdIdxGlobal && (*mp & mask) != (oldmodes & mask)) {
				if(mask == MdHScrl) {
					// If horizontal scrolling is now disabled, unshift any shifted windows on
					// current screen; otherwise, set shift of current window to line shift.
					if(!(*mp & MdHScrl)) {
						EWindow *winp = wheadp;
						do {
							if(winp->w_face.wf_firstcol > 0) {
								winp->w_face.wf_firstcol = 0;
								winp->w_flags |= (WFHard | WFMode);
								}
							} while((winp = winp->w_nextp) != NULL);
						}
					else if(cursp->s_firstcol > 0) {
						curwp->w_face.wf_firstcol = cursp->s_firstcol;
						curwp->w_flags |= (WFHard | WFMode);
						}
					cursp->s_firstcol = 0;
					}
				}

			// Onward...
			} while(opflags & OpScript);
		}

	// Display new mode line.
	supd_wflags(type == MdIdxBuffer ? bufp : NULL,WFMode);
	if(!(opflags & OpScript) && mlerase(0) != Success)
		return rc.status;

	// Return former state of last mode that was changed.
	dsetint(formerState,rp);

	// Run mode-change hook if any flag was changed and not a buffer mode or target buffer is not hidden or a macro.
	if(*mp != oldmodes && (bufp == NULL || !(bufp->b_flags & (BFHidden | BFMacro)))) {
		Datum *old[3],**oldp = old,**oldpz = old + 3;
		ofp = oldflags;
		do {
			if(dnewtrk(oldp) != 0)
				return librcset(Failure);
			if(getmodes(*oldp++,*ofp++) != Success)
				break;
			} while(oldp < oldpz);
		(void) exechook(NULL,INT_MIN,hooktab + HkMode,0x2A3,old[0],old[1],old[2]);
		}

	return rc.status;
	}

// Build and pop up a buffer containing all the global and buffer modes.  Render buffer and return status.
int showModes(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	DStrFab rpt;
	ModeSpec *msp;
	struct {
		char *hdr;
		uint mask;
		} mtab[] = {
			{text364,MdGlobal},
				// "GLOBAL MODES"
			{text365,0},
				// "BUFFER MODES"
			{NULL,0}},*mtabp = mtab;
	char wkbuf[32];

	// Get buffer for the mode list.
	if(sysbuf(text363,&bufp,BFTermAttr) != Success)
			// "Modes"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return librcset(Failure);

	// Write global modes, then buffer modes.
	do {
		// Construct the header lines.
		if(mtabp->mask == 0 && dputs("\n\n",&rpt) != 0)
			return librcset(Failure);
		if(dputs(mtabp->hdr,&rpt) != 0 || dputc('\n',&rpt) != 0)
			return librcset(Failure);
		msp = modeinfo;
		do {
			// Skip if wrong type of mode.
			if((msp->mask & MdGlobal) != mtabp->mask)
				continue;

			wkbuf[0] = '\n';
			if(mtabp->mask == 0)
				wkbuf[1] = curbp->b_modes & msp->mask ? '*' : ' ';
			else
				wkbuf[1] = modetab[MdIdxGlobal].flags & msp->mask ? '*' : ' ';
			wkbuf[2] = modetab[MdIdxShow].flags & msp->mask ? '+' : ' ';
			sprintf(wkbuf + 3," ~b%-10s~0",msp->mlname);
			if(dputs(wkbuf,&rpt) != 0 || dputs(msp->desc,&rpt) != 0)
				return librcset(Failure);
			} while((++msp)->mlname != NULL);
		} while((++mtabp)->hdr != NULL);

	// Write footnote.
	if(dputs("\n\n----------\n",&rpt) != 0 || dputs(text366,&rpt) != 0)
				// "* Active global or buffer mode\n+ Active show mode"
		return librcset(Failure);

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return librcset(Failure);
	if(bappend(bufp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n,bufp,RendNewBuf | RendReset);
	}
