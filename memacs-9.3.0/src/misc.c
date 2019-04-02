// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// misc.c	Miscellaneous functions for MightEMacs.
//
// This file contains command processing routines for some random commands.

#include "os.h"
#include <ctype.h>
#include "pllib.h"
#include "std.h"
#include "bind.h"
#include "exec.h"
#include "cmd.h"
#include "search.h"
#include "var.h"

// Is a character a letter?
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

// Change the case of a letter.  First check lower and then upper.  If character is not a letter, it gets returned unchanged.
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

// Copy a string from src to dest, changing its case.  Return dest.
static char *trancase(char *dest,char *src,char *trantab) {
	char *str;

	str = dest;
	while(*src != '\0')
		*str++ = trantab[(int) *src++];
	*str = '\0';

	return dest;
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
	int i;

	// Set all of both tables to their indices.
	for(i = 0; i < 256; ++i)
		lowcase[i] = upcase[i] = i;

	// Set letter translations.
	for(i = 'a'; i <= 'z'; ++i) {
		upcase[i] = i ^ 0x20;
		lowcase[i ^ 0x20] = i;
		}

	// And those international characters also.
	for(i = (uchar) '\340'; i <= (uchar) '\375'; ++i) {
		upcase[i] = i ^ 0x20;
		lowcase[i ^ 0x20] = i;
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
// but the column on an infinite-width display.  *Interactive only*
int showPoint(Datum *rp,int n,Datum **argpp) {
	Line *lnp;
	ulong dotline = 1;
	ulong numchars = 0;		// # of chars in file.
	ulong numlines = 0;		// # of lines in file.
	ulong prechars = 0;		// # chars preceding point.
	ulong prelines = 0;		// # lines preceding point.
	short curchar = '\0';		// Character at point.
	double ratio = 0.0;
	int col = 0;
	int ecol = 0;			// Column pos/end of current line.
	Dot dot;
	char *info;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	char *str,wkbuf1[32],wkbuf2[32];

	str = strcpy(wkbuf2,"0.0");
	if(!bempty(NULL)) {
		if(n == INT_MIN) {

			// Start at the beginning of the buffer.
			lnp = si.curbp->b_lnp;
			curchar = '\0';

			// Count characters and lines.
			numchars = numlines = 0;
			while(lnp->l_nextp != NULL || lnp->l_used > 0) {

				// If we are on the current line, save "pre-point" stats.
				if(lnp == dotp->lnp) {
					dotline = (prelines = numlines) + 1;
					prechars = numchars + dotp->off;
					if(lnp->l_nextp != NULL || dotp->off < lnp->l_used)
						curchar = (dotp->off < lnp->l_used) ? lnp->l_text[dotp->off] : '\n';
					}

				// On to the next line.
				++numlines;
				numchars += lnp->l_used + (lnp->l_nextp == NULL ? 0 : 1);
				if((lnp = lnp->l_nextp) == NULL)
					break;
				}

			// If point is at end of buffer, save "pre-point" stats.
			if(bufend(dotp)) {
				dotline = (prelines = numlines) + (dotp->lnp->l_used == 0 ? 1 : 0);
				prechars = numchars;
				}

			ratio = 0.0;				// Ratio before point.
			if(numchars > 0)
				ratio = (double) prechars / numchars * 100.0;
			sprintf(str = wkbuf2,"%.1f",ratio);
			if(numchars > 0) {

				// Fix rounding errors at buffer boundaries.
				if(prechars > 0 && strcmp(str,"0.0") == 0)
					str = "0.1";
				else if(prechars < numchars && strcmp(str,"100.0") == 0)
					str = "99.9";
				}
			}
		else if(!bempty(NULL) && (dotp->lnp->l_nextp != NULL || dotp->off < dotp->lnp->l_used))
			curchar = (dotp->off == dotp->lnp->l_used) ? '\n' : dotp->lnp->l_text[dotp->off];

		// Get real column and end-of-line column.
		col = getccol(NULL);
		dot.lnp = dotp->lnp;
		dot.off = dotp->lnp->l_used;
		ecol = getccol(&dot);
		}

	// Summarize and report the info.
	if(curchar >= ' ' && curchar < 0x7F)
		sprintf(wkbuf1,"'%c' 0x%.2hX",(int) curchar,curchar);
	else
		sprintf(wkbuf1,"0x%.2hX",curchar);
	if((n == INT_MIN ? asprintf(&info,text60,dotline,numlines,col,ecol,prechars,numchars,str,wkbuf1) :
				// "Line %lu/%lu, Col %d/%d, Char %lu/%lu (%s%%), char = %s"
	 asprintf(&info,text340,col,ecol,wkbuf1)) < 0)
			// "Col %d/%d, char = %s"
		return rcset(Panic,0,text94,"showPoint");
			// "%s(): Out of memory!"
	(void) mlputs(MLHome | MLFlush,info);
	free((void *) info);
	return rc.status;
	}

// Get line number, given buffer and line pointer.
long getlinenum(Buffer *bufp,Line *targlnp) {
	Line *lnp;
	long n;

	// Start at the beginning of the buffer and count lines.
	lnp = bufp->b_lnp;
	n = 0;
	do {
		// If we have reached the target line, stop.
		if(lnp == targlnp)
			break;
		++n;
		} while((lnp = lnp->l_nextp) != NULL);

	// Return result.
	return n + 1;
	}

// Return new column, given character c and old column.
int newcol(short c,int col) {

	if(c == '\t')
		col += -(col % si.htabsize) + (si.htabsize - 1);
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
		dotp = &si.curwp->w_face.wf_dot;

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
	Dot *dotp = &si.curwp->w_face.wf_dot;

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

// Check if all white space from beginning of line, given length.  Return Boolean result, including true if length is zero.
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

// Determine if given name is defined.  If default n, set rp to result: "alias", "buffer", "command", "pseudo-command",
// "function", "macro", "variable", or nil; otherwise, set rp to true if mark is defined in current buffer (n < 0), active mark
// is defined (n == 0), mode exists (n == 1), or mode group exists (n > 1); otherwise, false.
int checkdef(Datum *rp,int n,Datum **argpp) {
	Datum *namep = argpp[0];
	char *result = NULL;
	UnivPtr univ;

	// Null or nil string?
	if(!disnn(namep)) {

		// Mark?
		if(n <= 0 && n != INT_MIN) {
			if(intval(namep) && markval(namep)) {
				Mark *mkp;

				dsetbool(mfind(namep->u.d_int,&mkp,n == 0 ? (MkOpt_Query | MkOpt_Viz) :
				 MkOpt_Query) == Success && mkp != NULL,rp);
				}
			goto Retn;
			}

		if(!strval(namep))
			goto Retn;

		// Mode?
		if(n == 1) {
			dsetbool(mdsrch(namep->d_str,NULL) != NULL,rp);
			goto Retn;
			}

		// Mode group?
		if(n > 1) {
			dsetbool(mgsrch(namep->d_str,NULL,NULL),rp);
			goto Retn;
			}

		// Variable?
		if(findvar(namep->d_str,NULL,OpQuery))
			result = text292;
				// "variable"

		// Command, pseudo-command, function, alias, or macro?
		else if(execfind(namep->d_str,OpQuery,PtrAny,&univ))
			switch(univ.p_type) {
				case PtrCmd:
					result = text158;
						// "command"
					break;
				case PtrPseudo:
					result = text333;
						// "pseudo-command"
					break;
				case PtrFunc:
					result = text247;
						// "function"
					break;
				case PtrMacro_C:
				case PtrMacro_O:
					result = text336;
						// "macro"
					break;
				default:	// PtrAlias
					result = text127;
						// "alias"
				}
		else if(bsrch(namep->d_str,NULL) != NULL)
			result = text83;
				// "buffer"

		}

	// Return result.
	if(result == NULL)
		dsetnil(rp);
	else if(dsetstr(result,rp) != 0)
		(void) librcset(Failure);
Retn:
	return rc.status;
	}

// Match closing fences against their partners, and if on screen, briefly light the cursor there.
int fmatch(short c) {
	Dot odot;		// Original line pointer and offset.
	Line *toplnp;		// Top line in current window.
	int count;		// Current fence level count.
	short opench;		// Open fence.
	short c1;		// Current character in scan.
	ushort mvflag = 0;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Skip this if executing a script or a keyboard macro.
	if((si.opflags & OpScript) || kmacro.km_state == KMPlay)
		return rc.status;

	// First get the display update out there.
	if(update(INT_MIN) != Success)
		return rc.status;

	// Save the original point position.
	odot = *dotp;

	// Set up proper open fence for passed closed fence.
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
	toplnp = si.curwp->w_face.wf_toplnp;
	count = 1;
	(void) movech(-1);

	// Scan back until we find it, or move past the top of the window.
	while(count > 0 && (dotp->lnp != toplnp || dotp->off > 0)) {
		(void) movech(-1);
		if(dotp->off == dotp->lnp->l_used)
			c1 = '\n';
		else
			c1 = dotp->lnp->l_text[dotp->off];
		if(c1 == c)
			++count;
		else if(c1 == opench)
			--count;
		}

	// If count is zero, we have a match -- display the sucker.
	if(count == 0) {
		mvflag = si.curwp->w_flags & WFMove;
		if(update(INT_MIN) != Success)
			return rc.status;
		cpause(si.fencepause);
		}

	// Restore the previous position.
	si.curwp->w_face.wf_dot = odot;
	if(mvflag)
		si.curwp->w_flags |= WFMove;

	return rc.status;
	}

#if WordCount
// Count the number of words in the marked region, along with average word sizes, number of chars, etc, and report on them.
// *Interactive only*
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
	char *info;

	// Make sure we have a region to count.
	if(getregion(&region,0) != Success)
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
	if(asprintf(&info,text100,nwords,nchars,region.r_size,nlines + 1,(nwords > 0) ? nchars * 1.0 / nwords : 0.0) < 0)
			// "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f"
		return rcset(Panic,0,text94,"countWords");
			// "%s(): Out of memory!"
	(void) mlputs(MLHome | MLFlush,info);
	free((void *) info);
	return rc.status;
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
	if((si.opflags & (OpScript | OpEval)) == OpScript)
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
	if(si.opflags & OpScript) {

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
		if(terminp(datp,text235,ArgNotNull1,0,&ti) != Success)
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
		si.randseed ^= si.randseed >> 12; // a
		si.randseed ^= si.randseed << 25; // b
		si.randseed ^= si.randseed >> 27; // c
		}
	return ((si.randseed * 0x2545F4914F6CDD1D) & LONG_MAX) % max + 1;
	}

// Get an apropos match string with a null default.  Convert a nil argument to null as well.  Return status.
static int getamatch(ShowCtrl *scp,char *prmt,Datum **argpp) {
	Datum *mstrp = &scp->sc_mstr;

	if(!(si.opflags & OpScript)) {
		char wkbuf[NWork + 1];

		sprintf(wkbuf,"%s %s",text20,prmt);
				// "Apropos"
		if(terminp(mstrp,wkbuf,ArgNil1,0,NULL) != Success)
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

// Initialize color pairs for a "show" listing.
void initInfoColors(void) {

	if(si.opflags & OpHaveColor) {
		short *cp = term.itemColor[ColorIdxInfo].colors;
		if(cp[0] >= -1) {
			short fg,bg,line;

			fg = cp[0];
			bg = cp[1];
			line = bg >= 0 ? bg : fg;
			(void) init_pair(term.maxWorkPair - ColorPairIH,fg,bg);
			(void) init_pair(term.maxWorkPair - ColorPairISL,line,-1);
			}
		}
	}

// Initialize a ShowCtrl object for a "show" listing.  If default n, assume a full list; otherwise, get a match string and save
// in control object for later.  Return status.
int showopen(ShowCtrl *scp,int n,char *plabel,Datum **argpp) {
	char *str,wkbuf[strlen(plabel) + 3];

	dinit(&scp->sc_name);
	dinit(&scp->sc_value);
	dinit(&scp->sc_mstr);
	minit(&scp->sc_match);
	scp->sc_n = n;

	// If not default n, get match string.
	if(n != INT_MIN && getamatch(scp,plabel,argpp) != Success)
		return rc.status;

	// Create a buffer name (plural of plabel) and get a buffer.
	str = stpcpy(wkbuf,plabel);
	wkbuf[0] = upcase[(int) wkbuf[0]];
	strcpy(str,str[-1] == 's' ? "es" : "s");
	if(sysbuf(wkbuf,&scp->sc_listp,BFTermAttr) == Success)
		initInfoColors();

	return rc.status;
	}

// Copy src to dest in upper case, inserting a space between each two characters.  Return dest.
static char *expand1(char *dest,char *src) {

	do {
		*dest++ = upcase[(int) *src++];
		*dest++ = ' ';
		} while(*src != '\0');
	return dest;
	}

// Copy src to dest in upper case, inserting a space between each two characters.  If plural is true, append "s" or "es".
// Return dest.
char *expand(char *dest,char *src,bool plural) {

	dest = expand1(dest,src);
	if(plural)
		dest = expand1(dest,dest[-2] == 'S' ? "es" : "s");
	*--dest = '\0';
	return dest;
	}

// Write header lines to an open string-fab object, given object pointer, report title, plural flag, column headings line, and
// column widths.  Return status.  If wp is NULL, title (only) is written with bold and color attributes (if possible), centered
// in full terminal width and, if colhead not NULL, length of colhead string is used for colored length of title.  Otherwise,
// page width is calculated from wp array, title is written in bold, centered in calculated width, and headings are written in
// bold and color.  The minwidth member of the second-to-last element of the wp array may be -1, which indicates a width from
// the current column position to the right edge of the screen.  Spacing between columns is 1 if terminal width < 96; otherwise,
// 2.  *widthp is set to page width if widthp not NULL.
int rpthdr(DStrFab *rptp,char *title,bool plural,char *colhead,ColHdrWidth *wp,int *widthp) {
	ColHdrWidth *wp1;
	int leadin,whitespace,width;
	char *space = (term.t_ncol < 96) ? " " : "  ";
	int spacing = (term.t_ncol < 96) ? 1 : 2;
	short *cp = term.itemColor[ColorIdxInfo].colors;
	char wkbuf[term.t_ncol + 1];

	// Expand title, get page width, and begin color if applicable.
	expand(wkbuf,title,plural);
	if(wp == NULL) {
		if(colhead == NULL)
			width = term.t_ncol;
		else {
			width = strlen(colhead);
			if((leadin = (term.t_ncol - width) >> 1) > 0 && dputf(rptp,"%*s",leadin,"") != 0)
				goto LibFail;
			}
		if((cp[0] >= -1) && dputf(rptp,"%c%hd%c",AttrSpecBegin,term.maxWorkPair - ColorPairIH,AttrColorOn) != 0)
			goto LibFail;
		}
	else {
		width = -spacing;
		wp1 = wp;
		do {
			if(wp1->minwidth == -1) {
				width = term.t_ncol;
				break;
				}
			width += wp1->minwidth + spacing;
			++wp1;
			} while(wp1->minwidth != 0);
		}
	if(widthp != NULL)
		*widthp = width;

	// Write centered title line in bold.
	whitespace = width - strlen(wkbuf);
	leadin = whitespace >> 1;
	if(dputf(rptp,"%*s%c%c%s%c%c",leadin,"",AttrSpecBegin,AttrBoldOn,wkbuf,AttrSpecBegin,AttrBoldOff) != 0)
		goto LibFail;

	// Finish title line and write column headings if requested.
	if(wp != NULL) {
		char *str = colhead;
		int curcol = 0;

		if(dputs("\n\n",rptp) != 0)
			goto LibFail;

		// Write column headings in bold, and in color if available.
		if(dputf(rptp,"%c%c",AttrSpecBegin,AttrBoldOn) != 0)
			goto LibFail;
		wp1 = wp;
		do {
			if(wp1 != wp) {
				if(dputs(space,rptp) != 0)
					goto LibFail;
				curcol += spacing;
				}
			width = (wp1->minwidth == -1) ? term.t_ncol - curcol : (int) wp1->minwidth;
			if(cp[0] >= -1) {
				if(dputf(rptp,"%c%hd%c%-*.*s%c%c",AttrSpecBegin,term.maxWorkPair - ColorPairIH,AttrColorOn,
				 width,(int) wp1->maxwidth,str,AttrSpecBegin,AttrColorOff) != 0)
					goto LibFail;
				}
			else if(dputf(rptp,"%-*.*s",width,(int) wp1->maxwidth,str) != 0)
				goto LibFail;
			curcol += width;
			str += wp1->maxwidth;
			++wp1;
			} while(wp1->minwidth != 0);
		if(dputf(rptp,"%c%c",AttrSpecBegin,AttrBoldOff) != 0)
			goto LibFail;

		// Underline each column header if no color.
		if(!(si.opflags & OpHaveColor)) {
			if(dputc('\n',rptp) != 0)
				goto LibFail;
			dupchr(str = wkbuf,'-',term.t_ncol);
			curcol = 0;
			wp1 = wp;
			do {
				if(wp1 != wp) {
					if(dputs(space,rptp) != 0)
						goto LibFail;
					curcol += spacing;
					}
				width = (wp1->minwidth == -1) ? term.t_ncol - curcol : (int) wp1->minwidth;
				if(dputf(rptp,"%.*s",width,str) != 0)
					goto LibFail;
				curcol += width;
				str += wp1->minwidth;
				++wp1;
				} while(wp1->minwidth != 0);
			}
		}
	else if((si.opflags & OpHaveColor) && dputf(rptp,"%*s%c%c",whitespace - leadin,"",AttrSpecBegin,AttrColorOff) != 0)
		goto LibFail;
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Write header lines to an open string-fab object, given report title.  Return status.
static int showhdr(ShowCtrl *scp,char *title) {

	// If not using color, write separator line.
	if(!(si.opflags & OpHaveColor)) {
		char wkbuf[term.t_ncol + 1];
		dupchr(wkbuf,'=',term.t_ncol);
		if(dputs(wkbuf,&scp->sc_rpt) != 0 || dputc('\n',&scp->sc_rpt) != 0)
			return librcset(Failure);
		}

	// Write title.
	return rpthdr(&scp->sc_rpt,title,true,NULL,NULL,NULL);
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
	short *cp = term.itemColor[ColorIdxInfo].colors;
	char sepline[term.t_ncol + 1];
	char wkbuf[NWork];

	// Initialize.
	scp->sc_itemp = NULL;
	if(flags & SHNoDesc)
		scp->sc_desc = NULL;
	if(doApropos && !(flags & SHExact)) {
		if(dnewtrk(&indexp) != 0 || dnewtrk(&srcp) != 0)
			goto LibFail;
		}

	// Open a string-fab object and write section header if applicable.
	if(dopentrk(&scp->sc_rpt) != 0)
		goto LibFail;
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
						goto LibFail;
					if(sindex(indexp,INT_MIN,srcp,&scp->sc_mstr,&scp->sc_match) != Success)
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
		if((flags & SHSepLine) || firstItem) {
			if(dputc('\n',&scp->sc_rpt) != 0)
				goto LibFail;
			if((cp[0] >= -1) && dputf(&scp->sc_rpt,"%c%hd%c",AttrSpecBegin,
			 term.maxWorkPair - ColorPairISL,AttrColorOn) != 0)
				goto LibFail;
			if(dputs(sepline,&scp->sc_rpt) != 0)
				goto LibFail;
			if((cp[0] >= -1) && dputf(&scp->sc_rpt,"%c%c",AttrSpecBegin,AttrColorOff) != 0)
				goto LibFail;
			}
		firstItem = false;

		// Store item name and value, if any, in work buffer and add line to report.
		sprintf(wkbuf,"%c%c%s%c%c",AttrSpecBegin,AttrBoldOn,scp->sc_name.d_str,AttrSpecBegin,AttrBoldOff);
		if(nametab[0] != NULL) {
			str = pad(wkbuf,34);
			if(str[-2] != ' ')
				strcpy(str,str[-1] == ' ' ? " " : "  ");
			}
		if(dputc('\n',&scp->sc_rpt) != 0 || dputs(wkbuf,&scp->sc_rpt) != 0)
			goto LibFail;
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
					goto LibFail;
				if(str == strz)
					break;
				str0 = *str == ' ' ? str + 1 : str;
				}
			}
		}

	// Close string-fab object and append string to report buffer if any items were written.
	if(dclose(&scp->sc_rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(!firstItem) {

		// Write blank line if title not NULL and buffer not empty.
		if(title != NULL && !bempty(scp->sc_listp))
			if(bappend(scp->sc_listp,"") != Success)
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
	KeyBind *kbp;
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
			goto LibFail;
		}
	else {
		char wkbuf[strlen(name) + strlen(usage) + 1];
		sprintf(wkbuf,"%s %s",name,usage);
		if(dsetstr(wkbuf,&scp->sc_name) != 0)
			goto LibFail;
		}

	// Set key bindings, if any.
	if(ktype & PtrFunc)
		dclear(&scp->sc_value);
	else {
		DStrFab sf;
		char *sep = NULL;

		// Search for any keys bound to command or macro (buffer) "tp".
		if(dopenwith(&sf,&scp->sc_value,SFClear) != 0)
			goto LibFail;
		kbp = nextbind(&kw);
		do {
			if((kbp->k_targ.p_type & ktype) && kbp->k_targ.u.p_voidp == tp) {

				// Add the key sequence.
				ektos(kbp->k_code,keybuf,true);
				if((sep != NULL && dputs(sep,&sf) != 0) ||
				 dputf(&sf,"%c%c%c%s%c%c",AttrSpecBegin,AttrAlt,AttrULOn,keybuf,AttrSpecBegin,AttrULOff) != 0)
					goto LibFail;
				sep = ", ";
				}
			} while((kbp = nextbind(&kw)) != NULL);
		if(dclose(&sf,sf_string) != 0)
			goto LibFail;
		}

	return rc.status;
LibFail:
	return librcset(Failure);
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
	Datum **blp;
	Datum **blpz = buftab.a_elpp + buftab.a_used;

	// First call?
	if(scp->sc_itemp == NULL) {
		scp->sc_itemp = (void *) (blp = buftab.a_elpp);
		bufp = bufptr(buftab.a_elpp[0]);
		}
	else {
		blp = (Datum **) scp->sc_itemp;
		bufp = (req == SHReqNext && ++blp == blpz) ? NULL : bufptr(*blp);
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			while(bufp != NULL) {

				// Select if macro buffer and (not constrained or bound to a hook or non-default n).
				if((bufp->b_flags & BFMacro) && (!(bufp->b_flags & BFConstrain) || ishook(bufp,false) ||
				 scp->sc_n != INT_MIN)) {

					// Found macro... return its name.
					*namep = bufp->b_bname + 1;
					scp->sc_itemp = (void *) blp;
					goto Retn;
					}
				bufp = (++blp == blpz) ? NULL : bufptr(*blp);
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
				*namep = (ap->a_type == PtrAlias_M ? ap->a_targ.u.p_bufp->b_bname :
				 ap->a_targ.u.p_cfp->cf_name);
				scp->sc_itemp = (void *) ap;
				goto Retn;
				}

			// End of list.
			*namep = NULL;
			break;
		case SHReqUsage:
			if(dsetstr(ap->a_name,&scp->sc_name) != 0)
				goto LibFail;
			*namep = ap->a_name;
			break;
		default: // SHReqValue
			{char *name2 = (ap->a_targ.p_type & PtrMacro) ? ap->a_targ.u.p_bufp->b_bname :
			 ap->a_targ.u.p_cfp->cf_name;
			if(dputs("-> ",&scp->sc_rpt) != 0 || dputs(name2,&scp->sc_rpt) != 0)
				goto LibFail;
			}
		}
Retn:
	return rc.status;
LibFail:
	return librcset(Failure);
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
	uint aflags = ArgNIS1 | ArgBool1 | ArgArray1;

	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	result = !any;
	for(;;) {
		// Get next argument.
		if(!(aflags & ArgFirst) && !havesym(s_comma,false))
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

// Create an empty array, stash into rp, and store pointer to it in *arypp.  Return status.
int mkarray(Datum *rp,Array **arypp) {

	return (*arypp = anew(0,NULL)) == NULL ? librcset(Failure) : awrap(rp,*arypp);
	}

// Mode and group attribute control flags.
#define MG_Nil		0x0001		// Nil value allowed.
#define MG_Bool		0x0002		// Boolean value allowed.
#define MG_ModeAttr	0x0004		// Valid mode attribute.
#define MG_GrpAttr	0x0008		// Valid group attribute.
#define MG_ScopeAttr	0x0010		// Scope attribute.

#define MG_Global	2		// Index of "global" table entry.
#define MG_Hidden	4		// Index of "hidden" table entry.

static struct attrinfo {
	char *name;		// Attribute name.
	char *abbr;		// Abbreviated name (for small screens).
	short pindex;		// Name index of interactive prompt letter.
	ushort cflags;		// Control flags.
	ushort mflags;		// Mode flags.
	} ai[] = {		// MdEnabled indicates that associated flag is to be set when value is "true".
		{"buffer","buf",0,MG_Bool | MG_ScopeAttr | MG_ModeAttr,MdGlobal},
		{"description","desc",0,MG_ModeAttr | MG_GrpAttr,0},
		{"global","glb",0,MG_Bool | MG_ScopeAttr | MG_ModeAttr,MdGlobal | MdEnabled},
		{"group","grp",1,MG_Nil | MG_ModeAttr,0},
		{"hidden","hid",0,MG_Bool | MG_ModeAttr,MdHidden | MdEnabled},
		{"modes","modes",0,MG_Nil | MG_GrpAttr,0},
		{"visible","viz",0,MG_Bool | MG_ModeAttr,MdHidden},
		{NULL,NULL,-1,0,0}};

// Check if a built-in mode is set, depending on its scope, and return Boolean result.  If bufp is NULL and needs to be
// accessed, check current buffer.
bool modeset(ushort id,Buffer *bufp) {
	ModeSpec *msp = mi.cache[id];
	if(msp->ms_flags & MdGlobal)
		return msp->ms_flags & MdEnabled;
	if(bufp == NULL)
		bufp = si.curbp;
	return bmsrch1(bufp,msp);
	}

// Set a built-in global mode and flag mode line update if needed.
void gmset(ModeSpec *msp) {

	if(!(msp->ms_flags & MdEnabled)) {
		msp->ms_flags |= MdEnabled;
		if((msp->ms_flags & (MdHidden | MdInLine)) != MdHidden)		// If mode line update needed...
			wnextis(NULL)->w_flags |= WFMode;			// update mode line of bottom window.
		}
	}

// Clear a built-in global mode and flag mode line update if needed.
void gmclear(ModeSpec *msp) {

	if(msp->ms_flags & MdEnabled) {
		msp->ms_flags &= ~MdEnabled;
		if((msp->ms_flags & (MdHidden | MdInLine)) != MdHidden)		// If mode line update needed...
			wnextis(NULL)->w_flags |= WFMode;			// update mode line of bottom window.
		}
	}

// Set new description in mode or group record.
static int setdesc(char *desc,char **descp) {

	if(desc != NULL) {
		if(*descp != NULL)
			free((void *) *descp);
		if((*descp = (char *) malloc(strlen(desc) + 1)) == NULL)
			return rcset(Panic,0,text94,"setdesc");
				// "%s(): Out of memory!"
		strcpy(*descp,desc);
		}
	return rc.status;
	}

// binsearch() helper function for returning a mode name, given table (array) and index.
static char *modename(void *table,ssize_t i) {

	return msptr(((Datum **) table)[i])->ms_name;
	}

// Search the mode table for given name and return pointer to ModeSpec object if found; otherwise, NULL.  In either case, set
// *indexp to target array index if indexp not NULL.
ModeSpec *mdsrch(char *name,ssize_t *indexp) {
	ssize_t i = 0;
	bool found = (mi.modetab.a_used == 0) ? false : binsearch(name,(void *) mi.modetab.a_elpp,mi.modetab.a_used,strcasecmp,
	 modename,&i);
	if(indexp != NULL)
		*indexp = i;
	return found ? msptr(mi.modetab.a_elpp[i]) : NULL;
	}

// Check a proposed mode or group name for length and valid (printable) characters and return Boolean result.  It is assumed
// that name is not null.
static bool validMG(char *name,ushort type) {

	if(strlen(name) > MaxMGName) {
		(void) rcset(Failure,0,text280,type == MG_GrpAttr ? text388 : text387,MaxMGName);
			// "%s name cannot be null or exceed %d characters","Group","Mode"
		return false;
		}
	char *str = name;
	do {
		if(*str <= ' ' || *str > '~') {
			(void) rcset(Failure,0,text397,type == MG_GrpAttr ? text390 : text389,Literal4,name);
					// "Invalid %s %s '%s'"		"group"		"mode","name"
			return false;
			}
		} while(*++str != '\0');
	return true;
	}

// Create a user mode and, if mspp not NULL, set *mspp to ModeSpec object.  Return status.
int mdcreate(char *name,ssize_t index,char *desc,ushort flags,ModeSpec **mspp) {
	ModeSpec *msp;
	Datum *datp;

	// Valid name?
	if(!validMG(name,MG_ModeAttr))
		return rc.status;

	// All is well... create a mode record.
	if((msp = (ModeSpec *) malloc(sizeof(ModeSpec) + strlen(name))) == NULL)
		return rcset(Panic,0,text94,"mdcreate");
			// "%s(): Out of memory!"

	// Set the attributes.
	msp->ms_group = NULL;
	msp->ms_flags = flags;
	strcpy(msp->ms_name,name);
	msp->ms_desc = NULL;
	if(setdesc(desc,&msp->ms_desc) != Success)
		return rc.status;

	// Set pointer to object if requested.
	if(mspp != NULL)
		*mspp = msp;

	// Insert record into mode table array and return.
	if(dnew(&datp) != 0)
		goto LibFail;
	dsetblobref((void *) msp,0,datp);
	if(ainsert(&mi.modetab,index,datp,false) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Delete a user mode and return status.
static int mddelete(ModeSpec *msp,ssize_t index) {
	Datum *datp;

	// Built-in mode?
	if(!(msp->ms_flags & MdUser))
		return rcset(Failure,0,text396,text389,msp->ms_name);
			// "Cannot delete built-in %s '%s'","mode"

	// No, user mode.  Check usage.
	if(msp->ms_flags & MdGlobal) {				// Global mode?
		if(msp->ms_flags & MdEnabled)			// Yes, enabled?
			wnextis(NULL)->w_flags |= WFMode;	// Yes, update mode line of bottom window.
		}
	else
		// Buffer mode.  Disable it in all buffers (if any).
		bmclearall(msp);
	if(msp->ms_group != NULL)				// Is mode a group member?
		--msp->ms_group->mg_usect;			// Yes, decrement use count.

	// Usage check completed... nuke the mode.
	if(msp->ms_desc != NULL)				// If mode description present...
		free((void *) msp->ms_desc);			// free it.
	datp = adelete(&mi.modetab,index);			// Free the array element...
	free((void *) msptr(datp));				// the ModeSpec object...
	ddelete(datp);						// and the Datum object.

	return rcset(Success,0,"%s %s",text387,text10);
				// "Mode","deleted"
	}

// Clear all global modes.  Return true if any mode was enabled; otherwise, false.
static bool gclear(void) {
	Array *aryp = &mi.modetab;
	Datum *datp;
	ModeSpec *msp;
	bool modeWasChanged = false;

	while((datp = aeach(&aryp)) != NULL) {
		msp = msptr(datp);
		if((msp->ms_flags & (MdGlobal | MdEnabled)) == (MdGlobal | MdEnabled)) {
			gmclear(msp);
			modeWasChanged = true;
			}
		}
	return modeWasChanged;
	}

// Find a mode group by name and return Boolean result.
// If group is found:
//	If slot not NULL, *slot is set to prior node.
//	If grp not NULL, *grp is set to ModeGrp object.
// If group is not found:
//	If slot not NULL, *slot is set to list-insertion slot.
bool mgsrch(char *name,ModeGrp **slot,ModeGrp **grp) {
	int result;
	bool rval;

	// Scan the mode group list.
	ModeGrp *mgp1 = NULL;
	ModeGrp *mgp2 = mi.gheadp;
	while(mgp2 != NULL) {
		if((result = strcasecmp(mgp2->mg_name,name)) == 0) {

			// Found it.
			if(grp != NULL)
				*grp = mgp2;
			rval = true;
			goto Retn;
			}
		if(result > 0)
			break;
		mgp1 = mgp2;
		mgp2 = mgp2->mg_nextp;
		}

	// Group not found.
	rval = false;
Retn:
	if(slot != NULL)
		*slot = mgp1;
	return rval;
	}

// Create a mode group and, if resultp not NULL, set *resultp to new object.  prior points to prior node in linked list.  Return
// status.
int mgcreate(char *name,ModeGrp *prior,char *desc,ModeGrp **resultp) {
	ModeGrp *mgp;

	// Valid name?
	if(!validMG(name,MG_GrpAttr))
		return rc.status;

	// All is well... create a mode group record.
	if((mgp = (ModeGrp *) malloc(sizeof(ModeGrp) + strlen(name))) == NULL)
		return rcset(Panic,0,text94,"mgcreate");
			// "%s(): Out of memory!"

	// Find the place in the list to insert this group.
	if(prior == NULL) {

		// Insert at the beginning.
		mgp->mg_nextp = mi.gheadp;
		mi.gheadp = mgp;
		}
	else {
		// Insert after prior.
		mgp->mg_nextp = prior->mg_nextp;
		prior->mg_nextp = mgp;
		}

	// Set the object attributes.
	mgp->mg_flags = MdUser;
	mgp->mg_usect = 0;
	strcpy(mgp->mg_name,name);
	mgp->mg_desc = NULL;
	(void) setdesc(desc,&mgp->mg_desc);

	// Set pointer to object if requested and return.
	if(resultp != NULL)
		*resultp = mgp;
	return rc.status;
	}

// Remove all members of a mode group, if any.
static void mgclear(ModeGrp *mgp) {

	if(mgp->mg_usect > 0) {
		ModeSpec *msp;
		Datum *datp;
		Array *aryp = &mi.modetab;
		while((datp = aeach(&aryp)) != NULL) {
			msp = msptr(datp);
			if(msp->ms_group == mgp) {
				msp->ms_group = NULL;
				if(--mgp->mg_usect == 0)
					return;
				}
			}
		}
	}

// Delete a mode group and return status.
static int mgdelete(ModeGrp *mgp1,ModeGrp *mgp2) {

	// Built-in group?
	if(!(mgp2->mg_flags & MdUser))
		return rcset(Failure,0,text396,text390,mgp2->mg_name);
			// "Cannot delete built-in %s '%s'","group"

	// It's a go.  Clear the group, delete it from the group list, and free the storage.
	mgclear(mgp2);
	if(mgp1 == NULL)
		mi.gheadp = mgp2->mg_nextp;
	else
		mgp1->mg_nextp = mgp2->mg_nextp;
	if(mgp2->mg_desc != NULL)
		free((void *) mgp2->mg_desc);
	free((void *) mgp2);

	return rcset(Success,0,"%s %s",text388,text10);
				// "Group","deleted"
	}

// Remove a mode's group membership, if any.
static void membclear(ModeSpec *msp) {

	if(msp->ms_group != NULL) {
		--msp->ms_group->mg_usect;
		msp->ms_group = NULL;
		}
	}

// Make a mode a member of a group.  Don't allow if group already has members and mode's scope doesn't match members'.  Also, if
// group already has an active member, disable mode being added if enabled (because modes are mutually exclusive).  Return
// status.
static int membset(ModeSpec *msp,ModeGrp *mgp) {

	// Check if scope mismatch or any mode in group is enabled.
	if(mgp->mg_usect > 0) {
		ushort gtype = USHRT_MAX;
		ModeSpec *msp2;
		Datum *datp;
		Array *aryp = &mi.modetab;
		bool activeFound = false;
		while((datp = aeach(&aryp)) != NULL) {
			msp2 = msptr(datp);
			if(msp2->ms_group == mgp) {				// Mode in group?
				if(msp2 == msp)					// Yes.  Is it the one to be added?
					return rc.status;			// Yes, nothing to do.
				if((gtype = msp2->ms_flags & MdGlobal)) {	// No.  Remember type and "active" status.
					if(msp2->ms_flags & MdEnabled)
						activeFound = true;
					}
				else if(bmsrch1(si.curbp,msp2))
					activeFound = true;
				}
			}
		if(gtype != USHRT_MAX && (msp->ms_flags & MdGlobal) != gtype)
			return rcset(Failure,0,text398,text389,msp->ms_name,text390,mgp->mg_name);
				// "Scope of %s '%s' must match %s '%s'","mode","group"

		// Disable mode if an enabled mode was found in group.
		if(activeFound)
			(msp->ms_flags & MdGlobal) ? gmclear(msp) : bmsrch(si.curbp,true,1,msp);
		}

	// Group is empty or scopes match... add mode.
	membclear(msp);
	msp->ms_group = mgp;
	++mgp->mg_usect;

	return rc.status;
	}

// Parse next token from string at *strp delimited by delim.  If token not found, return NULL; otherwise, (1), set *strp to next
// character past token, or NULL if terminating null was found; and (2), return pointer to trimmed result (which may be null).
char *strparse(char **strp,short delim) {
	short c;
	char *str1,*str2;
	char *str0 = *strp;
	if(str0 == NULL || *(str0 = nonwhite(str0,false)) == '\0')
		return NULL;
	str1 = str0 + 1;				// str0 points to beginning of token (non-whitespace character).
	while((c = *str1) != '\0' && c != delim)
		++str1;
	str2 = str1;					// str1 points to token delimiter.
	while(str2[-1] == ' ' || str2[-1] == '\t')
		--str2;
	*str2 = '\0';					// str2 points to end of trimmed token.
	*strp = (c == '\0') ? NULL : str1 + 1;
	return str0;
	}

// Ensure mutually-exclusive modes (those in the same group) are not set.
static void chkgrp(ModeSpec *msp,Buffer *bufp) {

	if(msp->ms_group != NULL && msp->ms_group->mg_usect > 1) {
		ModeSpec *msp2;
		Datum *datp;
		Array *aryp = &mi.modetab;
		while((datp = aeach(&aryp)) != NULL) {
			if((msp2 = msptr(datp)) != msp && msp2->ms_group == msp->ms_group) {
				if(msp2->ms_flags & MdGlobal)
					gmclear(msp2);
				else
					bmsrch(bufp,true,1,msp2);
				}
			}			
		}
	}

// binsearch() helper function for returning a mode or group attribute keyword, given table (array) and index.
static char *ainame(void *table,ssize_t i) {

	return ((struct attrinfo *) table)[i].name;
	}

// Check if mode scope change is valid.  Return true if so; otherwise, set an error and return false.
static bool scopeMatch(ModeSpec *msp) {

	// If mode not in a group or only member, scope-change is allowed.
	if(msp->ms_group == NULL || msp->ms_group->mg_usect == 1)
		return true;

	// No go... mode is in a group with other members.
	(void) rcset(Failure,0,text398,text389,msp->ms_name,text390,msp->ms_group->mg_name);
		// "Scope of %s '%s' must match %s '%s'","mode","group"
	return false;
	}

// Process a mode or group attribute.  Return status.
static int doattr(Datum *attrp,ushort type,void *mp) {
	ModeSpec *msp;
	ModeGrp *mgp;
	struct attrinfo *aip;
	ssize_t i;
	ushort *flagsp;
	char **descp,*str0,*str1;
	char wkbuf[strlen(attrp->d_str) + 1];

	// Make copy of attribute string so unmodified original can be used for error reporting.
	strcpy(str0 = wkbuf,attrp->d_str);

	if(type == MG_ModeAttr) {					// Set control variables.
		msp = (ModeSpec *) mp;
		flagsp = &msp->ms_flags;
		descp = &msp->ms_desc;
		}
	else {
		mgp = (ModeGrp *) mp;
		flagsp = &mgp->mg_flags;
		descp = &mgp->mg_desc;
		}

	str1 = strparse(&str0,':');					// Parse keyword from attribute string.
	if(str1 == NULL || *str1 == '\0')
		goto ErrRtn;
	if(!binsearch(str1,(void *) ai,sizeof(ai) /			// Keyword found.  Look it up and check if right type.
	 sizeof(struct attrinfo) - 1,strcasecmp,ainame,&i))
		goto ErrRtn;						// Not in attribute table.
	aip = ai + i;
	if(!(aip->cflags & type))
		goto ErrRtn;						// Wrong type.

	str1 = strparse(&str0,'\0');					// Valid keyword and type.  Get value.
	if(str1 == NULL || *str1 == '\0')
		goto ErrRtn;
	if(aip->cflags & (MG_Nil | MG_Bool)) {				// If parameter accepts a nil or Boolean value...
		bool haveTrue;

		if(strcasecmp(str1,viz_true) == 0) {			// Check if true, false, or nil.
			haveTrue = true;
			goto DoBool;
			}
		if(strcasecmp(str1,viz_false) == 0) {
			haveTrue = false;
DoBool:
			if(!(aip->cflags & MG_Bool))			// Have Boolean value... allowed?
				goto ErrRtn;				// No, error.
			if(aip->cflags & MG_ScopeAttr) {		// Yes, setting mode's scope?
				if(msp->ms_flags & MdLocked)		// Yes, error if mode is scope-locked.
					return rcset(Failure,0,text399,text387,msp->ms_name);
						// "%s '%s' has permanent scope","Mode"
				if(i == MG_Global) {			// Determine ramifications and make changes.
					if(haveTrue)			// "global: true"
						goto BufToGlobal;
					goto GlobalToBuf;		// "global: false"
					}
				else if(haveTrue) {			// "buffer: true"
GlobalToBuf:								// Do global -> buffer.
					if(!(*flagsp & MdGlobal) || !scopeMatch(msp))
						return rc.status;	// Not global mode or scope mismatch.
					haveTrue = (*flagsp & MdEnabled);
					*flagsp &= ~(MdEnabled | MdGlobal);
					if(haveTrue)			// If mode was enabled...
						(void) bmset(si.curbp,msp,false,NULL);	// set it in current buffer.
					}
				else {					// "buffer: false"
BufToGlobal:								// Do buffer -> global.
					if((*flagsp & MdGlobal) || !scopeMatch(msp))
						return rc.status;	// Not buffer mode or scope mismatch.
					haveTrue = bmsrch1(si.curbp,msp);	// If mode was enabled in current buffer...
					bmclearall(msp);
					*flagsp |= haveTrue ? (MdGlobal | MdEnabled) : MdGlobal;	// set it globally.
					}
				goto BFUpdate;
				}
			else {
				if(i == MG_Hidden) {
					if(haveTrue)			// "hidden: true"
						goto Hide;
					goto Expose;			// "hidden: false"
					}
				else if(haveTrue) {			// "visible: true"
Expose:
					if(*flagsp & MdHidden) {
						*flagsp &= ~MdHidden;
						goto FUpdate;
						}
					}
				else {					// "visible: false"
Hide:
					if(!(*flagsp & MdHidden)) {
						*flagsp |= MdHidden;
FUpdate:
						if(*flagsp & MdGlobal)
							wnextis(NULL)->w_flags |= WFMode;
						else
BFUpdate:
							supd_wflags(NULL,WFMode);
						}
					}
				}
			}
		else if(strcasecmp(str1,viz_nil) == 0) {
			if(!(aip->cflags & MG_Nil))
				goto ErrRtn;
			if(aip->cflags & MG_ModeAttr)			// "group: nil"
				membclear(msp);
			else						// "modes: nil"
				mgclear(mgp);
			}
		else {
			if(aip->cflags & MG_Nil)
				goto ProcString;
			goto ErrRtn;
			}
		}
	else {
ProcString:
		// str1 points to null-terminated value.  Process it.
		if((aip->cflags & (MG_ModeAttr | MG_GrpAttr)) == (MG_ModeAttr | MG_GrpAttr))	// "description: ..."
			(void) setdesc(str1,descp);
		else if(aip->cflags & MG_ModeAttr) {			// "group: ..."
			if(!mgsrch(str1,NULL,&mgp))
				return rcset(Failure,0,text395,text390,str1);
					// "No such %s '%s'","group"
			(void) membset(msp,mgp);
			}
		else {							// "modes: ..."
			str0 = str1;
			do {
				str1 = strparse(&str0,',');		// Get next mode name.
				if(str1 == NULL || *str1 == '\0')
					goto ErrRtn;
				if((msp = mdsrch(str1,NULL)) == NULL)
					return rcset(Failure,0,text395,text389,str1);
						// "No such %s '%s'","mode"
				if(membset(msp,mgp) != Success)
					break;
				} while(str0 != NULL);
			}
		}

	return rc.status;
ErrRtn:
	return rcset(Failure,0,text397,type == MG_GrpAttr ? text390 : text389,text391,attrp->d_str);
			// "Invalid %s %s '%s'"		"group"		"mode","setting"
	}

// Prompt for mode or group attribute and value, convert to string (script) form, and save in rp.  Return status.
static int getattr(Datum *rp,ushort type) {
	DStrFab prompt,result;
	Datum *datp;
	struct attrinfo *aip;
	char *attrname;
	char *lead = " (";

	// Build prompt for attribute to set or change.
	if(dnewtrk(&datp) != 0 || dopenwith(&result,rp,SFClear) != 0 || dopenwith(&prompt,datp,SFClear) != 0 ||
	 dputc(upcase[(int) text186[0]],&prompt) != 0 || dputs(text186 + 1,&prompt) != 0)
					// "attribute"
		goto LibFail;
	aip = ai;
	do {
		if(aip->cflags & type) {
			attrname = (term.t_ncol >= 80) ? aip->name : aip->abbr;
			if(dputs(lead,&prompt) != 0 || (aip->pindex == 1 && dputc(attrname[0],&prompt) != 0) ||
			 dputf(&prompt,"%c%c%c%c%c%c%c%s",AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrBoldOn,
			 (int) attrname[aip->pindex],AttrSpecBegin,AttrAllOff,attrname + aip->pindex + 1) != 0)
				goto LibFail;
			lead = ", ";
			}
		} while((++aip)->name != NULL);
	if(dputc(')',&prompt) != 0 || dclose(&prompt,sf_string) != 0)
		goto LibFail;

	// Get attribute from user and validate it.
	dclear(rp);
	if(terminp(datp,datp->d_str,ArgNotNull1 | ArgNil1,Term_LongPrmt | Term_Attr | Term_OneChar,NULL) != Success ||
	 datp->d_type == dat_nil)
		return rc.status;
	aip = ai;
	do {
		attrname = (term.t_ncol >= 80) ? aip->name : aip->abbr;
		if(attrname[aip->pindex] == datp->u.d_int)
			goto Found;
		} while((++aip)->name != NULL);
	return rcset(Failure,0,text397,type == MG_GrpAttr ? text390 : text389,text186,vizc(datp->u.d_int,VBaseDef));
			// "Invalid %s %s '%s'"			"group","mode","attribute"
Found:
	// Return result if Boolean attribute; otherwise, get string value.
	if(dputf(&result,"%s: ",aip->name) != 0)
		goto LibFail;
	if(aip->cflags & MG_Bool) {
		if(dputs(viz_true,&result) != 0)
			goto LibFail;
		}
	else {
		if(terminp(datp,text53,(aip->cflags & MG_Nil) ? ArgNotNull1 | ArgNil1 : ArgNotNull1,0,NULL) != Success
				// "Value"
		 || dtosf(&result,datp,NULL,CvtShowNil) != Success)
			return rc.status;
		}
	if(dclose(&result,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Create prompt for editMode or editModeGroup command.
static int emodeprmt(ushort type,int n,Datum *datp) {
	DStrFab prompt;

	return (dopenwith(&prompt,datp,SFClear) != 0 ||
	 dputf(&prompt,"%s %s",n == INT_MIN ? text402 : n <= 0 ? text26 : text403,text389) != 0 ||
					// "Edit"		"Delete","Create or edit","mode"
	 (type == MG_GrpAttr && dputf(&prompt," %s",text390) != 0) || dclose(&prompt,sf_string) != 0) ?
						// "group"
	 librcset(Failure) : rc.status;
	}

// Edit a user mode or group.  Return status.
static int editMG(Datum *rp,int n,Datum **argpp,ushort type) {
	void *mp;
	Datum *datp;
	char *name;
	bool created = false;

	if(dnewtrk(&datp) != 0)
		return librcset(Failure);

	// Get mode or group name and validate it.
	if(si.opflags & OpScript)
		name = argpp[0]->d_str;
	else {
		// Build prompt.
		if(emodeprmt(type,n,datp) != Success)
			return rc.status;

		// Get mode or group name from user.
		if(terminp(datp,datp->d_str,ArgNil1 | ArgNotNull1,type == MG_ModeAttr ? Term_C_GMode | Term_C_BMode : 0,NULL)
		 != Success || datp->d_type == dat_nil)
			return rc.status;
		name = datp->d_str;
		}
	if(type == MG_ModeAttr) {
		ssize_t index;
		ModeSpec *msp;

		if((msp = mdsrch(name,&index)) == NULL) {			// Mode not found.  Create it?
			if(n <= 0)						// No, error.
				return rcset(Failure,0,text395,text389,name);
						// "No such %s '%s'","mode"
			if(mdcreate(name,index,NULL,MdUser,&msp) != Success)	// Yes, create it.
				return rc.status;
			created = true;
			}
		else if(n <= 0 && n != INT_MIN)					// Mode found.  Delete it?
			return mddelete(msp,index);				// Yes.
		mp = (void *) msp;
		}
	else {
		ModeGrp *mgp1,*mgp2;

		if(!mgsrch(name,&mgp1,&mgp2)) {					// Group not found.  Create it?
			if(n <= 0)						// No, error.
				return rcset(Failure,0,text395,text390,name);
						// "No such %s '%s'","group"
			if(mgcreate(name,mgp1,NULL,&mgp2) != Success)		// Yes, create it.
				return rc.status;
			created = true;
			}
		else if(n <= 0 && n != INT_MIN)					// Group found.  Delete it?
			return mgdelete(mgp1,mgp2);				// Yes.
		mp = (void *) mgp2;
		}

	// If interactive mode, get an attribute; otherwise, loop through attribute arguments and process them.
	if(!(si.opflags & OpScript)) {
		if(getattr(datp,type) == Success && datp->d_type != dat_nil && doattr(datp,type,mp) == Success)
			rcset(Success,0,"%s %s",type == MG_ModeAttr ? text387 : text388,created ? text392 : text394);
								// "Mode","Group","created","attribute changed"
		}
	else {
		// Process any remaining arguments.
		while(havesym(s_comma,false)) {
			if(funcarg(datp,ArgNotNull1) != Success || doattr(datp,type,mp) != Success)
				break;
			}
		}

	return rc.status;
	}

// Edit a user mode.  Return status.
int editMode(Datum *rp,int n,Datum **argpp) {

	return editMG(rp,n,argpp,MG_ModeAttr);
	}

// Edit a user mode group.  Return status.
int editModeGroup(Datum *rp,int n,Datum **argpp) {

	return editMG(rp,n,argpp,MG_GrpAttr);
	}

// Retrieve enabled global (bufp is NULL) or buffer (bufp not NULL) modes and save as an array of keywords in *rp.  Return
// status.
int getmodes(Datum *rp,Buffer *bufp) {
	Array *aryp0;

	if(mkarray(rp,&aryp0) == Success) {
		Datum dat;
		dinit(&dat);
		if(bufp == NULL) {
			Datum *datp;
			ModeSpec *msp;
			Array *aryp = &mi.modetab;
			while((datp = aeach(&aryp)) != NULL) {
				msp = msptr(datp);
				if((msp->ms_flags & (MdGlobal | MdEnabled)) == (MdGlobal | MdEnabled))
					if(dsetstr(msp->ms_name,&dat) != 0 || apush(aryp0,&dat,true) != 0)
						goto LibFail;
				}
			}
		else {
			BufMode *bmp;

			for(bmp = bufp->b_modes; bmp != NULL; bmp = bmp->bm_nextp)
				if(dsetstr(bmp->bm_modep->ms_name,&dat) != 0 || apush(aryp0,&dat,true) != 0)
					goto LibFail;
			}
		dclear(&dat);
		}
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Check if a partial (or full) mode name is unique.  If "bmode" is true, consider buffer modes only; otherwise, global modes.
// Set *mspp to mode table entry if true; otherwise, NULL.  Return status.
static int modecmp(char *name,bool bmode,ModeSpec **mspp) {
	Datum *datp;
	Array *aryp = &mi.modetab;
	ModeSpec *msp0 = NULL,*msp;
	size_t len = strlen(name);
	while((datp = aeach(&aryp)) != NULL) {
		msp = msptr(datp);

		// Skip if wrong type of mode.
		if(bmode && (msp->ms_flags & MdGlobal))
			continue;

		if(strncasecmp(name,msp->ms_name,len) == 0) {

			// Exact match?
			if(msp->ms_name[len] == '\0') {
				msp0 = msp;
				goto Retn;
				}

			// Error if not first match.
			if(msp0 != NULL)
				goto ERetn;
			msp0 = msp;
			}
		}

	if(msp0 == NULL)
ERetn:
		return rcset(Failure,0,text66,name);
			// "Unknown or ambiguous mode '%s'"
Retn:
	*mspp = msp0;
	return rc.status;
	}

// Change a mode, given name, action (< 0: clear, 0: toggle, > 0: set), Buffer pointer if buffer mode (otherwise, NULL),
// optional "former state" pointer, and optional indirect ModeSpec pointer which is set to entry in mode table if mode was
// changed; otherwise, NULL.  If action < -1 or > 2, partial mode names are allowed.  Return status.
int chgmode(char *name,int action,Buffer *bufp,long *formerStatep,ModeSpec **resultp) {
	ModeSpec *msp;
	bool modeWasSet,modeWasChanged;

	// Check if name in mode table.
	if(action < -1 || action > 2) {
		if(modecmp(name,bufp != NULL,&msp) != Success)
			return rc.status;
		}
	else if((msp = mdsrch(name,NULL)) == NULL)
		return rcset(Failure,0,text395,text389,name);
				// "No such %s '%s'","mode"

	// Match found... validate mode and process it.  Error if (1), wrong type; or (2), ASave mode, $autoSave is zero, and
	// setting mode or toggling it and ASave not currently set.
	if(((msp->ms_flags & MdGlobal) != 0) != (bufp == NULL))
		return rcset(Failure,0,text66,name);
				// "Unknown or ambiguous mode '%s'"
	if(msp == mi.cache[MdIdxASave] && si.gasave == 0 &&
	 (action > 0 || (action == 0 && !(msp->ms_flags & MdEnabled))))
		return rcset(Failure,RCNoFormat,text35);
			// "$autoSave not set"

	if(bufp != NULL) {

		// Change buffer mode.
		if(action <= 0) {
			modeWasSet = bmsrch(bufp,true,1,msp);
			if(action == 0) {
				if(!modeWasSet && bmset(bufp,msp,false,NULL) != Success)
					return rc.status;
				modeWasChanged = true;
				}
			else
				modeWasChanged = modeWasSet;
			}
		else {
			if(bmset(bufp,msp,false,&modeWasSet) != Success)
				return rc.status;
			modeWasChanged = !modeWasSet;
			}
		}
	else {
		// Change global mode.
		modeWasSet = msp->ms_flags & MdEnabled;
		if(action < 0 || (action == 0 && modeWasSet)) {
			modeWasChanged = modeWasSet;
			gmclear(msp);
			}
		else {
			modeWasChanged = !modeWasSet;
			gmset(msp);
			}
		}
	if(formerStatep != NULL)
		*formerStatep = modeWasSet ? 1L : -1L;

	// Ensure mutually-exclusive modes (those in the same group) are not set.
	if(modeWasChanged && !modeWasSet)
		chkgrp(msp,bufp);

	if(resultp != NULL)
		*resultp = modeWasChanged ? msp : NULL;
	return rc.status;
	}

// Build portion of mode-change prompt and write to active string-fab object; e.g., "buffer mode".  Return -1 if error;
// otherwise, 0.
static int cmodeprmt(DStrFab *sfp,bool global) {

	return (dputc(' ',sfp) != 0 || dputs(global ? text146 : text83,sfp) != 0 || dputs(text63,sfp) != 0) ? -1 : 0;
						// "global",	"buffer",		" mode"
	}

// Change a mode, given result pointer, action (clear: n < 0, toggle: n == 0 or default, set: n == 1, or clear all and set: n >
// 1), optional ArgXXX control flags and/or MdGlobal flag, and optional buffer pointer.  Set rp to the former state (-1 or 1) of
// the last mode changed.  Return status.  Routine is called for chgBufMode and chgGlobalMode commands.
int changeMode(Datum *rp,int n,uint flags,Buffer *bufp) {
	Datum *datp,*argp,*oldmodes;
	long formerState = 0;			// Usually -1 or 1.
	int action = (n == INT_MIN) ? 0 : (n < 0) ? -1 : (n > 1) ? 2 : n;
	bool modeWasChanged = false;
	bool global = flags & MdGlobal;

	// Get ready.
	if(dnewtrk(&datp) != 0 || dnewtrk(&oldmodes) != 0)
		goto LibFail;

	// If interactive mode, build the proper prompt string; e.g. "Toggle global mode: ".
	if(!(si.opflags & OpScript)) {
		DStrFab prompt;

		// Build prompt.
		if(dopenwith(&prompt,datp,SFClear) != 0 ||
		 dputs(action < 0 ? text65 : action == 0 ? text231 : action == 1 ? text64 : text296,&prompt) != 0 ||
				// "Clear","Toggle","Set","Clear all and set"
		 cmodeprmt(&prompt,global) != 0 || dclose(&prompt,sf_string) != 0)
			goto LibFail;

		// Get mode name from user.
		if(terminp(datp,datp->d_str,ArgNil1,global ? Term_C_GMode : Term_C_BMode,NULL) != Success ||
		 (action <= 1 && datp->d_type == dat_nil))
			return rc.status;

		// Get buffer, if needed.
		if(!global) {
			if(n == INT_MIN)
				bufp = si.curbp;
			else {
				bufp = bdefault();
				if(bcomplete(rp,text229 + 2,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL)
				 != Success || bufp == NULL)
						// ", in"
					return rc.status;
				}
			}
		}

	// Have buffer (if applicable) and, if interactive, one mode name in datp (or nil).  Save current modes so they can be
	// passed to mode hook, if set.
	if(getmodes(oldmodes,bufp) != 0)
		return rc.status;

	// Clear all modes initially, if applicable.
	if(action > 1)
		modeWasChanged = global ? gclear() : bmclear(bufp);

	// Process mode argument(s).
	if(!(si.opflags & OpScript)) {
		if(datp->d_type == dat_nil)		// No mode was entered.
			return rc.status;
		argp = datp;
		goto DoMode;
		}
	else {
		Datum **elpp = NULL;
		ArraySize elct;
		int status;
		ModeSpec *msp;
		uint aflags = (flags & ~MdGlobal) | ArgNotNull1 | ArgArray1 | ArgMay;

		// Script mode: get zero or more arguments.
		do {
			if((status = nextarg(&argp,&aflags,datp,&elpp,&elct)) == NotFound)
				break;					// No arguments left.
			if(status != Success)
				return rc.status;
DoMode:
			if(chgmode(argp->d_str,action,bufp,&formerState,&msp) != Success)
				return rc.status;

			// Do special processing for specific global modes that changed.
			if(msp != NULL) {
				if(!modeWasChanged)
					modeWasChanged = true;
				if(msp == mi.cache[MdIdxHScrl]) {

					// If horizontal scrolling is now disabled, unshift any shifted windows on
					// current screen; otherwise, set shift of current window to line shift.
					if(!(msp->ms_flags & MdEnabled)) {
						EWindow *winp = si.wheadp;
						do {
							if(winp->w_face.wf_firstcol > 0) {
								winp->w_face.wf_firstcol = 0;
								winp->w_flags |= (WFHard | WFMode);
								}
							} while((winp = winp->w_nextp) != NULL);
						}
					else if(si.cursp->s_firstcol > 0) {
						si.curwp->w_face.wf_firstcol = si.cursp->s_firstcol;
						si.curwp->w_flags |= (WFHard | WFMode);
						}
					si.cursp->s_firstcol = 0;
					}
				}

			// Onward...
			} while(si.opflags & OpScript);
		}

	// Display new mode line.
	if(!(si.opflags & OpScript) && mlerase() != Success)
		return rc.status;

	// Return former state of last mode that was changed.
	dsetint(formerState,rp);

	// Run mode-change hook if any mode was changed and either: (1), a global mode; or (2), target buffer is not hidden and
	// not a macro.  Hook arguments are either: (1), nil,oldmodes (if global mode was changed); or (2), buffer name,oldmodes
	// (if buffer mode was changed).
	if(modeWasChanged && (bufp == NULL || !(bufp->b_flags & (BFHidden | BFMacro)))) {
		if(global)
			dclear(datp);
		else if(dsetstr(bufp->b_bname,datp) != 0)
			goto LibFail;
		(void) exechook(NULL,INT_MIN,hooktab + HkMode,0xA2,datp,oldmodes);
		}

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Build and pop up a buffer containing all the global and buffer modes.  Render buffer and return status.
int showModes(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	DStrFab rpt;
	ModeSpec *msp;
	ModeGrp *mgp;
	Array *aryp;
	Datum *datp;
	short c;
	int pagewidth;
	char *space = (term.t_ncol < 96) ? " " : "  ";
	int spacing = (term.t_ncol < 96) ? 1 : 2;
	static ColHdrWidth hcols[] = {
		{4,4},{11,11},{57,15},{0,0}};
	static ColHdrWidth hcolsMG[] = {
		{4,4},{11,11},{57,25},{0,0}};
	char wkbuf[strlen(text437) + strlen(text438) + 1];
			// "AUHL Name          Description"," / Members"
	struct mdinfo {
		char *hdr;
		uint mask;
		} *mtabp;
	static struct mdinfo mtab[] = {
		{text364,MdGlobal},
			// "GLOBAL MODES"
		{text365,0},
			// "BUFFER MODES"
		{NULL,0}};
	struct flaginfo {
		short c;
		ushort flag;
		char *desc;
		} *ftp;
	static struct flaginfo flagtab[] = {
		{SMActive,MdEnabled,text31},
				// "Active"
		{SMUser,MdUser,text62},
				// "User defined"
		{SMHidden,MdHidden,text400},
				// "Hidden"
		{SMLocked,MdLocked,text366},
				// "Scope-locked"
		{'\0',0,NULL}};

#if MMDebug & Debug_ModeDump
dumpmodes(NULL,true,"showModes() BEGIN - global modes\n");
dumpmodes(si.curbp,true,"showModes() BEGIN - buffer modes\n");
#endif
	// Get buffer for the mode list.
	if(sysbuf(text363,&bufp,BFTermAttr) != Success)
			// "Modes"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write global modes, then buffer modes.
	mtabp = mtab;
	do {
#if MMDebug & Debug_ModeDump
fprintf(logfile,"*** Writing %s modes...\n",mtabp->mask == 0 ? "BUFFER" : "GLOBAL");
#endif
		// Write headers.
		if(mtabp->mask == 0 && dputs("\n\n",&rpt) != 0)
			goto LibFail;
		if(rpthdr(&rpt,mtabp->hdr,false,text437,hcols,&pagewidth) != Success)
				// "AUHL Name          Description"
			return rc.status;

		// Loop through mode table.
		aryp = &mi.modetab;
		while((datp = aeach(&aryp)) != NULL) {
			msp = msptr(datp);

			// Skip if wrong type of mode.
			if((msp->ms_flags & MdGlobal) != mtabp->mask)
				continue;
			ftp = flagtab;
			if(dputc('\n',&rpt) != 0)
				goto LibFail;
			do {
				if(dputc((ftp->c != SMActive || mtabp->mask) ? (msp->ms_flags & ftp->flag ? ftp->c : ' ') :
				(bmsrch1(si.curbp,msp) ? ftp->c : ' '),&rpt) != 0)
					goto LibFail;
				} while((++ftp)->desc != NULL);
			if(dputf(&rpt,"%s%-*s",space,hcols[1].minwidth,msp->ms_name) != 0 ||
			 (msp->ms_desc != NULL && dputf(&rpt,"%s%s",space,msp->ms_desc) != 0))
				goto LibFail;
			}
		} while((++mtabp)->hdr != NULL);

	// Write mode groups.
#if MMDebug & Debug_ModeDump
fputs("*** Writing mode groups...\n",logfile);
#endif
	if(dputs("\n\n",&rpt) != 0)
		goto LibFail;
	sprintf(wkbuf,"%s%s",text437,text438);
	if(rpthdr(&rpt,text401,false,wkbuf,hcolsMG,NULL) != Success)
			// "MODE GROUPS"
		return rc.status;
	mgp = mi.gheadp;
	do {
		if(dputf(&rpt,"\n %c%*s%s%-*s",mgp->mg_flags & MdUser ? SMUser : ' ',hcols[0].minwidth - 2,"",space,
		 hcols[1].minwidth,mgp->mg_name) != 0 ||
		 (mgp->mg_desc != NULL && dputf(&rpt,"%s%s",space,mgp->mg_desc) != 0) ||
		 dputf(&rpt,"\n%*s",hcols[0].minwidth + hcols[1].minwidth + spacing + spacing + 4,"") != 0)
			goto LibFail;
		c = '[';
		aryp = &mi.modetab;
		while((datp = aeach(&aryp)) != NULL) {
			if((msp = msptr(datp))->ms_group == mgp) {
				if(dputc(c,&rpt) != 0 || dputs(msp->ms_name,&rpt) != 0)
					goto LibFail;
				c = ',';
				}
			}
		if((c == '[' && dputc(c,&rpt) != 0) || dputc(']',&rpt) != 0)
			goto LibFail;
		} while((mgp = mgp->mg_nextp) != NULL);

#if MMDebug & Debug_ModeDump
fputs("*** Writing footnote...\n",logfile);
#endif
	// Write footnote.
	if(sepline(pagewidth,&rpt) != 0)
		goto LibFail;
	ftp = flagtab;
	do {
		if(dputf(&rpt,"\n%c ",(int) ftp->c) != 0 || dputs(ftp->desc,&rpt) != 0)
			goto LibFail;
		} while((++ftp)->desc != NULL);

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		goto LibFail;
	if(bappend(bufp,rpt.sf_datp->d_str) != Success)
		return rc.status;

#if MMDebug & Debug_ModeDump
fputs("*** Done!\n",logfile);
#endif
	// Display results.
	return render(rp,n,bufp,RendNewBuf | RendReset);
LibFail:
	return librcset(Failure);
	}
