// (c) Copyright 2017 Richard W. Marinelli
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
#include "lang.h"
#include "exec.h"
#include "main.h"
#include "search.h"

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
int whence(Datum *rp,int n,Datum **argpp) {
	Line *lnp;			// Current line.
	ulong numchars;			// # of chars in file.
	ulong numlines;			// # of lines in file.
	ulong predchars;		// # chars preceding point.
	ulong predlines;		// # lines preceding point.
	int curchar;			// Character at point.
	double ratio;
	int col;
	int savepos;			// Temp save for current offset.
	int ecol;			// Column pos/end of current line.
	Dot *dotp = &curwp->w_face.wf_dot;
	char *str,wkbuf1[32],wkbuf2[32];

	// Skip this if not displaying messages.
	if(!(modetab[MdRec_Global].flags & MdMsg))
		return rc.status;

	if(n == INT_MIN) {

		// Starting at the beginning of the buffer.
		lnp = lforw(curbp->b_hdrlnp);
		curchar = '\n';

		// Start counting chars and lines.
		numchars = numlines = 0;
		while(lnp != curbp->b_hdrlnp) {

			// If we are on the current line, record it.
			if(lnp == dotp->lnp) {
				predlines = numlines;
				predchars = numchars + dotp->off;
				curchar = (dotp->off == lused(lnp)) ? '\n' : lgetc(lnp,dotp->off);
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
		curchar = (dotp->off == lused(dotp->lnp)) ? '\n' : lgetc(dotp->lnp,dotp->off);

	// Get real column and end-of-line column.
	col = getccol();
	savepos = dotp->off;
	dotp->off = lused(dotp->lnp);
	ecol = getccol();
	dotp->off = savepos;

	// Summarize and report the info.
	if(curchar >= ' ' && curchar < 0x7f)
		sprintf(wkbuf1,"'%c' 0x%.2X",curchar,curchar);
	else
		sprintf(wkbuf1,"0x%.2X",curchar);
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
	else if(c > 0x7f && (modetab[MdRec_Global].flags & MdEsc8))
		col += 3;
	return col + 1;
	}

// Return current column of point.
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
	int col;		// Current point column.
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
int fmatch(ushort ch) {
	Dot odot;		// Original line pointer and offset.
	Line *toplp;		// Top line in current window.
	int count;		// Current fence level count.
	ushort opench;		// Open fence.
	ushort c;		// Current character in scan.
	WindFace *wfp = &curwp->w_face;

	// Skip this if executing a script or a keyboard macro.
	if((opflags & OpScript) || kmacro.km_state == KMPlay)
		return rc.status;

	// First get the display update out there.
	if(update(false) != Success)
		return rc.status;

	// Save the original point position.
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
			c = '\n';
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
		if(update(false) != Success)
			return rc.status;
		cpause(fencepause);
		}

	// Restore the previous position.
	wfp->wf_dot = odot;

	return rc.status;
	}


#if Color
// Look up color index, given name.
int lookup_color(char *str) {
	int i;			// Index into color list.

	// Test it against the colors we know.
	for(i = 0; i < NColorS; ++i) {
		if(strcmp(str,cname[i]) == 0)
			return i;
		}
	return -1;
	}
#endif

#if WordCount
// Count the number of words in the marked region, along with average word sizes, number of chars, etc, and report on them
// (interactive only).
int countWords(Datum *rp,int n,Datum **argpp) {
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
	if(!(modetab[MdRec_Global].flags & MdMsg))
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
		if(offset == lused(lnp)) {	// End of line.
			ch = '\n';
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
	return rcset(Success,0,text100,nwords,nchars,region.r_size,nlines + 1,(nwords > 0) ? nchars * 1.0 / nwords : 0.0);
		// "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f"
	}
#endif

// Set i variable.  Use numeric argument for first parameter if present; otherwise, get arguments.  Return status.
int seti(Datum *rp,int n,Datum **argpp) {
	int i = ivar.i;
	int inc = ivar.inc;
	bool newfmt = false;
	Datum *datp;

	// Numeric argument?
	if(n != INT_MIN) {
		ivar.i = n;
		return rcset(Success,0,text287,ivar.i);
				// "i variable set to %d"
		}

	// If script mode and not evaluating, nothing to do.
	if((opflags & (OpScript | OpEval)) == OpScript)
		return rc.status;

	// Not script mode or evaluating.  Get value(s).
	if(dnewtrk(&datp) != 0)
		return drcset();
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
		char nbuf[LongWidth];

		// Prompt for "i" value.
		if(terminp(datp,text102,"0",RtnKey,0,0,0) != Success || toint(datp) != Success)
				// "Beginning value"
			return rc.status;
		i = datp->u.d_int;

		// Prompt for "inc" value.
		sprintf(nbuf,"%d",inc);
		if(terminp(datp,text234,nbuf,RtnKey,0,0,0) != Success || toint(datp) != Success)
				// "Increment"
			return rc.status;
		inc = datp->u.d_int;

		// Prompt for "format" value.
		if(terminp(datp,text235,ivar.format.d_str,Ctrl | '[',0,CFNotNull1,0) != Success)
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

// Change a mode, given result pointer, action (n < 0: clear, n == 0 (default): toggle, n > 0: set), type (0: global, 1: show,
// 2: default, 3: buffer), and optional mode flags.  If rp is not NULL, set it to the state (-1 or 1) of the last mode altered.
// Return status.
int adjustmode(Datum *rp,int n,int type,Datum *datp) {
	int action;			// < 0 (clear), 0 (toggle), or > 0 (set).
	long formerState;		// -1 or 1.
	ModeSpec *msp;
	uint mask;
	ModeRec *mrp = (type == 3) ? NULL : modetab + type;
	uint *mp = mrp ? &mrp->flags : &curbp->b_modes;	// Pointer to mode-flags word to change.
	uint mflags = *mp;		// Old mode flag word.
	uint aflags = Arg_First | CFNotNull1;
	long oldflags[4],*ofp = oldflags;

	// Save current modes so they can be passed to mode hook, if any.
	mrp = modetab;
	do {
		*ofp++ = (mrp++)->flags;
		} while(mrp->cmdlabel != NULL);
	*ofp = curbp->b_modes;

	// If called from putvar(), decode the new flag word, validate it, and jump ahead.
	if(datp != NULL) {
		ushort u1,u2;
		int count;

		// Any unknown bits?
		if((uint) datp->u.d_int & (type <= 1 ? ~MdGlobal : ~MdBuffer))
			goto BadBit;

		// If nothing has changed, nothing to do.
		if((u1 = datp->u.d_int) == mflags)
			return rc.status;

		if(type > 1) {
			// MdOver and MdRepl both set?
			if((u1 & MdGrp_Over) == MdGrp_Over)
				goto BadBit;

			// More than one language mode bit set?
			for(count = 0,u2 = u1 & MdGrp_Lang; u2 != 0; u2 >>= 1)
				if((u2 & 1) && ++count > 1)
BadBit:
					return rcset(Failure,0,text298,(uint) datp->u.d_int);
						// "Unknown or conflicting bit(s) in mode word '0x%.8X'"
			}
		else if(type == 0 && (u1 & MdASave) && gasave == 0)
			goto NoASave;

		// Update flag word and do special processing for specific global modes that changed.
		*mp = u1;
		if((u1 & MdEsc8) != (mflags & MdEsc8))
			uphard();
		if((u1 & MdHScrl) != (mflags & MdHScrl)) {
			lbound = 0;
			if(!(u1 & MdHScrl) && curwp->w_face.wf_fcol > 0) {
				curwp->w_face.wf_fcol = 0;
				curwp->w_flags |= WFHard | WFMode;
				}
			}
		}
	else {
		Datum *datp;
		action = (n == INT_MIN) ? 0 : n;

		if(dnewtrk(&datp) != 0)
			return drcset();

		// If interactive mode, build the proper prompt string; e.g. "Toggle global mode: "
		if(!(opflags & OpScript)) {
			DStrFab prompt;

			// Build prompt.
			if(dopentrk(&prompt) != 0 ||
			 dputs(action < 0 ? text65 : action > 0 ? text64 : text231,&prompt) != 0)
						// "Clear","Set","Toggle"
				return drcset();
			if(type < 3) {
				if(dputc(' ',&prompt) != 0 ||
				 dputs(type == 0 ? text31 : type == 1 ? text296 : text62,&prompt) != 0)
						// "global","show","default"
					return drcset();
				}
			if(type > 1) {
				if(dputc(' ',&prompt) != 0 || dputs(text83,&prompt) != 0)
									// "buffer"
					return drcset();
				}
			if(dputs(text63,&prompt) != 0 || dclose(&prompt,sf_string) != 0)
					// " mode"
				return drcset();

			// Prompt the user and get an answer.
			if(terminp(datp,prompt.sf_datp->d_str,NULL,RtnKey,0,0,
			 type <= 1 ? Term_C_GMode : Term_C_BMode) != Success || datp->d_type == dat_nil)
				return rc.status;
			goto DoMode;
			}

		// Script mode: get one or more arguments.
		size_t len;
		do {
			if(aflags & Arg_First) {
				if(!havesym(s_any,true))
					return rc.status;	// Error.
				}
			else if(!havesym(s_comma,false))
				break;				// At least one argument retrieved and none left.
			if(funcarg(datp,aflags) != Success)
				return rc.status;
			aflags = CFNotNull1;
DoMode:
			// Make keyword lowercase and get length.
			mklower(datp->d_str,datp->d_str);
			len = strlen(datp->d_str);

			// Test it against the modes we know.
			msp = (type <= 1) ? gmodeinfo : bmodeinfo;
			mask = 0;
			do {
				if(strncmp(datp->d_str,msp->name,len) == 0) {

					// Exact match?
					if(msp->name[len] == '\0') {
						mask = msp->mask;
						goto Found;
						}

					// Error if not first match.
					if(mask != 0)
						goto BadMode;
					mask = msp->mask;
					}
				} while((++msp)->name != NULL);

			// Error if no match.
			if(mask == 0)
BadMode:
				return rcset(Failure,0,text66,datp->d_str);
					// "Unknown or ambiguous mode '%s'"
Found:
			// Match found... validate mode and process it.
			if(type == 0 && (mask & MdASave) && gasave == 0 && (action > 0 || (action == 0 && !(*mp & mask))))
NoASave:
				return rcset(Failure,RCNoFormat,text35);
					// "$autoSave not set"
			formerState = (*mp & mask) ? 1L : -1L;
			if(action < 0)
				*mp &= ~mask;
			else if(action > 0)
				*mp |= mask;
			else
				*mp ^= mask;

			// Ensure mutually-exclusive bits are not set.
			if(type > 1) {
				if((mask & MdGrp_Over) && (*mp & MdGrp_Over))
					*mp = (*mp & ~MdGrp_Over) | mask;
				else if((mask & MdGrp_Lang) && (*mp & MdGrp_Lang))
					*mp = (*mp & ~MdGrp_Lang) | mask;
				}

			// Do special processing for specific global modes that changed.
			if(type == 0 && (*mp & mask) != (mflags & mask)) {
				switch(mask) {
					case MdEsc8:
						uphard();
						break;
					case MdHScrl:
						lbound = 0;
						if(!(*mp & MdHScrl) && curwp->w_face.wf_fcol > 0) {
							curwp->w_face.wf_fcol = 0;
							curwp->w_flags |= WFHard | WFMode;
							}
					}
				}

			// Onward...
			} while(opflags & OpScript);
		}

	// Display new mode line.
	if(type != 2)
		upmode(type == 3 ? curbp : NULL);
	if(!(opflags & OpScript))
		mlerase(0);		// Erase the prompt.

	// Return former state of last mode that was changed.
	if(rp != NULL)
		dsetint(formerState,rp);

	// Run mode-change hook if any flag was changed.
	return *mp != mflags && !(curbp->b_flags & (BFHidden | BFMacro)) ?
	 exechook(NULL,INT_MIN,hooktab + HkMode,0x554,oldflags[0],oldflags[1],oldflags[2],oldflags[3]) : rc.status;
	}

// Build and pop up a buffer containing all the global and buffer modes.  Render buffer and return status.
int showModes(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	DStrFab rpt;
	ModeSpec *mtp;
	struct {
		char *hdr;
		ModeSpec *mtp;
		} mtab[] = {
			{text364,gmodeinfo},
				// "GLOBAL MODES"
			{text365,bmodeinfo},
				// "BUFFER MODES"
			{NULL,NULL}},*mtabp = mtab;
	char wkbuf[32];

	// Get buffer for the mode list.
	if(sysbuf(text363,&bufp) != Success)
			// "ModeList"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	// Write global modes, then buffer modes.
	do {
		// Construct the header lines.
		if(mtabp->mtp == bmodeinfo && dputs("\n\n",&rpt) != 0)
			return drcset();
		if(dputs(mtabp->hdr,&rpt) != 0 || dputc('\n',&rpt) != 0)
			return drcset();
		mtp = mtabp->mtp;
		do {
			wkbuf[0] = '\n';
			if(mtabp->mtp == gmodeinfo) {
				wkbuf[1] = modetab[MdRec_Global].flags & mtp->mask ? '*' : ' ';
				wkbuf[2] = modetab[MdRec_Show].flags & mtp->mask ? '+' : ' ';
				}
			else {
				wkbuf[1] = curbp->b_modes & mtp->mask ? '*' : ' ';
				wkbuf[2] = modetab[MdRec_Default].flags & mtp->mask ? '+' : ' ';
				}
			wkbuf[3] = ' ';
			sprintf(wkbuf + 4,"%-10s",mtp->mlname);
			if(dputs(wkbuf,&rpt) != 0 || dputs(mtp->desc,&rpt) != 0)
				return drcset();
			} while((++mtp)->mlname != NULL);
		} while((++mtabp)->hdr != NULL);

	// Write footnote.
	if(dputs(text366,&rpt) != 0)
			// "\n\n* Active global or buffer mode\n+ Active show or default mode"
		return drcset();

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(bappend(bufp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,bufp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}
