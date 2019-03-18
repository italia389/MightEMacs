// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// vterm.c	Terminal display and window management routines for MightEMacs.
//
// Programming Notes:
// 1. This file contains functions that update the virtual screen via ncurses calls.  These functions use flags that are set in
//    the windows by the command routines.
// 2. Bold, reverse video, underline, and color are supported when the physical screen is written to the terminal, under certain
//    conditions:
//	  - the buffer containing line(s) with attributes must have its BFTermAttr flag set.
//	  - the window displaying the buffer cannot be horizontally shifted (including a pop-up window).
//    If these conditions are met, then any line in any window on the current screen (other than the current line in the current
//    window) that contains any of the attribute sequences described below will be rendered with the sequences converted to the
//    corresponding attributes (on or off).  Attribute sequences begin with the AttrSpecBegin (~) character and may be any of the
//    following:
//	~b	Begin bold.			~r	Begin reverse.			~u	Begin underline.
//	~B	End bold.			~R	End reverse.			~#u	Begin underline, skip spaces.
//	~<n>c	Begin color pair n.		~~	Literal ~.			~U	End underline.
//	~C	End color.			~Z	End all attributes.

#include "os.h"
#include <stdarg.h>
#include "std.h"
#include "bind.h"
#include "file.h"

/*** Local declarations ***/

#define VTDebug		0

// The display system uses an array of line flags allocated at startup which can accommodate the maximum possible screen size.
// The flags are used to managed extended lines and to keep track of which line in a window contains point.  There is also the
// concept of an "internal virtual screen", which is a virtual screen maintained by the editor that is different than the one
// maintained by the ncurses system which also holds the actual text of each line.  The internal screen is somewhat transitory
// and holds only the minimum information necessary to move window data lines and mode lines to ncurses.

// Virtual line flags.
#define VFExt		0x0001		// Extended virtual line (beyond terminal width).
#define VFDot		0x0002		// Dot line in associated window.

// Virtual screen control parameters.
typedef struct {
	int taboff;			// Offset for expanding tabs to virtual screen when line(s) are left-shifted.
	int vtcol;			// Column location of VTerm cursor.
	int vtrow;			// Row location of VTerm cursor.
	ushort *vlp;			// Pointer to current line flags.
	} VideoCtrl;
static VideoCtrl vt = {0,0,0,NULL};
static ushort *vlflags;			// Virtual screen line flags.

// Initialize the data structures used by the display system.  The virtual and physical screens are allocated and the operating
// system's terminal I/O channels are opened.  Return status.
int vtinit(void) {

	if(ttopen() != Success)		// Open the screen and keyboard.
		return rc.status;
	(void) attrset(0);		// Turn off all attributes.

	// Allocate the virtual screen line flags.  The vertical size is one less row than the maximum because the message
	// line is managed separately.
	if((vlflags = (ushort *) calloc(term.t_mrow - 1,sizeof(ushort))) == NULL)
		return rcset(Panic,0,text94,"vtinit");
			// "%s(): Out of memory!"

	return rc.status;
	}

// Set the virtual cursor to the specified row and column on the virtual screen.  If column is negative, move cursor to column
// zero so that vtputc() will write characters to the correct row when the column becomes non-negative.  Return status.
static int vtmove(int row,int col) {

	vt.vlp = &vlflags[row];
	vt.vtrow = row;
	vt.vtcol = col;
	return ttmove(row,col < 0 ? 0 : col);
	}

// Erase to end of line if possible, beginning at the current column in the virtual screen.  If the column is negative, it is
// assumed that the cursor is at column zero (so the ncurses call will succeed).  If the column is past the right edge, do
// nothing.
static void vteeol() {

	if(vt.vtcol < term.t_ncol)
		(void) clrtoeol();
	}

// Process a terminal attribute specification in a string.  If valid spec found, write attribute or AttrSpecBegin character to
// given ncurses window (unless TAScanOnly flag is set) and update *strp and TAAltUL flag in *flagsp if applicable; otherwise,
// update *strp so that the invalid spec is skipped over.  In either case, return number of invisible characters in the
// sequence; that is, number of characters which will not be displayed (usually its length).  *strp is assumed to point to the
// character immediately following the AttrSpecBegin (~) character.
int termattr(char **strp,char *strz,ushort *flagsp,WINDOW *wp) {
	short c,fg,bg;
	attr_t attr;
	int attrLen = 1;
	char *str = *strp;
	bool altForm = false;
	long i = 0;

	if(str == strz)					// End of string?
		return 1;				// Yes.
	if((c = *str++) == AttrSpecBegin) {		// ~~ sequence?
		if(!(*flagsp & TAScanOnly))		// Yes, process it.
			(void) waddch(wp,AttrSpecBegin);
		}
	else {
		if(c == AttrAlt) {				// Check for '#'.
			attrLen = 2;
			if(str == strz)				// ~# at EOL.
				goto Retn;
			altForm = true;
			c = *str++;
			}

		// Check for an integer prefix.
		if(c >= '0' && c <= '9') {
			do {
				++attrLen;
				if(str == strz)
					goto Retn;
				i = i * 10 + (c - '0');
				} while((c = *str++) >= '0' && c <= '9');
			}

		++attrLen;					// Count spec letter.
		switch(c) {
			case AttrAllOff:
				if(!(*flagsp & TAScanOnly)) {
					*flagsp &= ~TAAltUL;
					(void) wattrset(wp,0);
					}
				break;
			case AttrBoldOn:
				attr = A_BOLD;
				goto SetAttr;
			case AttrBoldOff:
				attr = A_BOLD;
				goto ClearAttr;
			case AttrColorOn:
				if(i > term.maxPair || pair_content(i,&fg,&bg) == ERR)
					goto Retn;
				if(*flagsp & TAScanOnly)
					break;
				attr = COLOR_PAIR(i);
				goto SetAttr;
			case AttrColorOff:
				if(*flagsp & TAScanOnly)
					break;
				(void) wattr_get(wp,&attr,&c,NULL);
				attr = COLOR_PAIR(c);
				goto ClearAttr;
			case AttrRevOn:
				attr = A_REVERSE;
				goto SetAttr;
			case AttrRevOff:
				attr = A_REVERSE;
				goto ClearAttr;
			case AttrULOn:
				if(*flagsp & TAScanOnly)
					break;
				attr = A_UNDERLINE;
				if(altForm)
					*flagsp |= TAAltUL;
SetAttr:
				if(!(*flagsp & TAScanOnly))
					(void) wattron(wp,attr);
				break;
			case AttrULOff:
				if(*flagsp & TAScanOnly)
					break;
				attr = A_UNDERLINE;
				*flagsp &= ~TAAltUL;
ClearAttr:
				if(!(*flagsp & TAScanOnly))
					(void) wattroff(wp,attr);
				break;
			default:
				// Invalid spec letter/character found: skip over it if a letter; otherwise, display literally.
				if(c < 'A' || (c > 'Z' && c < 'a') || c > 'z') {
					--str;
					--attrLen;
					}
			}
		}
Retn:
	*strp = str;
	return attrLen;
	}

// Scan a string for terminal attribute sequences and return the total number of characters found that are not visible when the
// string is displayed (zero if none).  Invalid sequences or any that would begin at or past "maxcol" are ignored.  If len < 0,
// string is assumed to be null terminated.
short attrCount(char *str,int len,int maxcol) {
	char *strz;
	int curcol;			// Running count of visible characters.
	ushort flags = TAScanOnly;

	strz = (len < 0) ? strchr(str,'\0') : str + len;
	curcol = len = 0;
	while(str < strz && curcol < maxcol) {
		if(*str++ == AttrSpecBegin) {
			if(str == strz)
				break;
			if(*str == AttrSpecBegin) {
				++str;
				++len;
				++curcol;
				}
			else
				len += termattr(&str,strz,&flags,NULL);
			}
		else
			++curcol;
		}

	return len;
	}

// Write a character to the virtual screen.  The virtual row and column are updated.  If we are off the left or right edge of
// the screen, don't write the character.  If the line is too long put a "$" in the last column.  Non-printable characters are
// converted to visible form.  It is assumed that tab characters are converted to spaces by the caller.  Return number of
// characters written.
static int vtputc(short c) {
	int vtcol0 = vt.vtcol;
	if(vt.vtcol >= term.t_ncol) {

		// We are past the right edge -- add a '$' if just past.
		if(vt.vtcol++ == term.t_ncol)
			(void) mvaddch(vt.vtrow,term.t_ncol - 1,LineExt);
		}
	else {
		if(c < ' ' || c == 0x7F) {

			// Control character.
			vtputc('^');
			vtputc(c ^ 0x40);
			}
		else if(c > 0x7F) {
			char wkbuf[5],*str = wkbuf;

			// Display character with high bit set symbolically.
			sprintf(wkbuf,"<%.2X>",c);
			do {
				vtputc(*str++);
				} while(*str != '\0');
			}
		else {
			// Plain character; just put it in the screen map.
			if(vt.vtcol >= 0)
				(void) addch(c);
			++vt.vtcol;
			}
		}
	return vt.vtcol - vtcol0;
	}

// Check if the line containing point is in the given window and reframe it if needed or wanted.  Return status.
int wupd_reframe(EWindow *winp) {
	Line *lnp;
	int i;
	int nlines;
	Line *dotlnp = winp->w_face.wf_dot.lnp;

	// Get current window size.
	nlines = winp->w_nrows;

	// If not a forced reframe, check for a needed one.
	if(!(winp->w_flags & WFReframe)) {

		// If the top line of the window is at EOB...
		if((lnp = winp->w_face.wf_toplnp)->l_nextp == NULL && lnp->l_used == 0) {
			if(lnp == winp->w_bufp->b_lnp)
				return rc.status;	// Buffer is empty, no reframe needed.
			winp->w_face.wf_toplnp = winp->w_bufp->b_lnp;
			goto Reframe;			// Buffer is not empty, reframe.
			}

		// Check if point is in the window.
		if(inwind(winp,dotlnp))
			return rc.status;		// Found point... no reframe needed.
		}
Reframe:
	// Reaching here, we need a window refresh.  Either the top line of the window is at EOB and the buffer is not empty (a
	// macro buffer), the line containing dot is not in the window, or it's a force.  Set i to the new window line number
	// (0 .. nlines - 1) where the dot line should "land".  If it's a force, use w_rfrow for i and just range-check it;
	// otherwise, calcluate a reasonable value (either vjump/100 from top, center of window, or vjump/100 from bottom)
	// depending on direction dot has moved.
	i = winp->w_rfrow;

	// If not a force...
	if(!(winp->w_flags & WFReframe)) {
		Line *forwlnp,*backlnp,*lnp1;

		// Search through the buffer in both directions looking for point, counting lines from top of window.  If point
		// is found, set i to top or bottom line if smooth scrolling, or vjump/100 from top or bottom if jump scrolling.
		forwlnp = backlnp = winp->w_face.wf_toplnp;
		lnp1 = winp->w_bufp->b_lnp;
		i = 0;
		for(;;) {
			// Did point move downward?
			if(forwlnp == dotlnp) {

				// Yes.  Force i to center of window if point moved more than one line past the bottom.
				i = (i > nlines) ? nlines / 2 : (vtc.vjump == 0) ? nlines - 1 :
				 nlines * (100 - vtc.vjump) / 100;
				break;
				}

			// Did point move upward?
			if(backlnp == dotlnp) {

				// Yes.  Force i to center of window if point moved more than one line past the top or vjump is
				// zero and point is at end of buffer.
				i = (i > 1) ? nlines / 2 : (vtc.vjump > 0) ? nlines * vtc.vjump / 100 :
				 (dotlnp->l_nextp == NULL) ? nlines / 2 : 0;
				break;
				}

			// Advance forward and back.
			if(forwlnp->l_nextp != NULL)
				forwlnp = forwlnp->l_nextp;
			else {
				if(backlnp == lnp1)
					break;
				goto Back;
				}
			if(backlnp != lnp1)
Back:
				backlnp = backlnp->l_prevp;
			++i;
			}
		}

	// Forced reframe.  Make sure i is in range (within the window).
	else if(i > 0) {			// Position down from top?
		if(--i >= nlines)		// Too big...
			i = nlines - 1;		// use bottom line.
		}
	else if(i < 0) {			// Position up from bottom?
		i += nlines;
		if(i < 0)			// Too small...
			i = 0;			// use top line.
		}
	else
		i = nlines / 2;			// Center dot line in window.

	// Now set top line i lines above dot line.
	wnewtop(winp,dotlnp,-i);
	winp->w_flags &= ~WFReframe;

	return rc.status;
	}

// Write a buffer line to the virtual screen.  Expand hard tabs to spaces and convert attribute sequences to ncurses calls if
// doAttr is true.
static void vtputln(Line *lnp,bool doAttr) {
	char *str,*strz;
	short c;
	ushort flags = 0;			// Skip spaces when underlining?

	strz = (str = lnp->l_text) + lnp->l_used;
	while(str < strz) {
		if((c = *str++) == '\t' || c == ' ') {
			if(doAttr && (flags & TAAltUL))
				(void) attroff(A_UNDERLINE);

			// Output hardware tab as the right number of spaces.
			do {
				vtputc(' ');
				} while(c == '\t' && (vt.vtcol + vt.taboff) % si.htabsize != 0);
			if(doAttr && (flags & TAAltUL))
				(void) attron(A_UNDERLINE);
			continue;
			}
		else if(c == AttrSpecBegin && doAttr) {
			termattr(&str,strz,&flags,stdscr);
			continue;
			}
		vtputc(c);
		}
	if(doAttr)				// Always leave terminal attributes off.
		(void) attrset(0);
	}

// Transfer all lines in given window to virtual screen.
static int vupd_wind(EWindow *winp) {
	Line *lnp;		// Line to update.
	int row;		// Virtual screen line to update.
	int endrow;		// Last row in current window + 1.
	bool attrOn = (winp->w_bufp->b_flags & BFTermAttr) && (!modeset(MdIdxHScrl,winp->w_bufp) ||
	 winp->w_face.wf_firstcol == 0);
	Line *curlnp = (winp == si.curwp) ? winp->w_face.wf_dot.lnp : NULL;

	// Loop through the lines in the window, updating the corresponding rows in the virtual screen.
	lnp = winp->w_face.wf_toplnp;
	endrow = (row = winp->w_toprow) + winp->w_nrows;
	do {
		// Update the virtual line.
		vt.taboff = modeset(MdIdxHScrl,winp->w_bufp) ? winp->w_face.wf_firstcol :
		 lnp == curlnp ? si.cursp->s_firstcol : 0;
		if(vtmove(row,-vt.taboff) != Success)
			break;
		if(vt.taboff == 0)
			*vt.vlp &= ~VFExt;

		// If we are not at the end...
		if(lnp != NULL) {
#if VTDebug && 0
if(!all) { char wkbuf[32]; sprintf(wkbuf,"%.*s",lnp->l_used < 20 ? lnp->l_used : 20,lnp->l_text);
ushort u; mlprintf(MLHome | MLFlush,"Calling vtputln(%s...) from vupd_wind(): row %d, vt.taboff %d, v_flags %d... ",
wkbuf,row,vt.taboff,(int) *vt.vlp); getkey(true,&u,false);}
#endif
			vtputln(lnp,attrOn && lnp != curlnp);
			lnp = lnp->l_nextp;
			}

		// Truncate virtual line.
		vteeol();
		} while(++row < endrow);
	vt.taboff = 0;
	return rc.status;
	}

// De-extend and/or re-render any line in any window on the virtual screen that needs it; for example, a line in the current
// window that in a previous update() cycle was the current line, or the current line in another window that was previously the
// current window.  Such lines were marked with the VFExt and VFDot flags by the wupd_cursor() function.  The current line in
// the current window is skipped, however.
static int supd_dex(void) {
	EWindow *winp;
	Line *lnp;
	int row;
	int endrow;
	bool attrOn;

	winp = si.wheadp;
	do {
		attrOn = (winp->w_bufp->b_flags & BFTermAttr) && (!modeset(MdIdxHScrl,winp->w_bufp) ||
		 winp->w_face.wf_firstcol == 0);
		lnp = winp->w_face.wf_toplnp;
		endrow = (row = winp->w_toprow) + winp->w_nrows;
		do {
			if(vtmove(row,0) != Success)
				return rc.status;

			// Update virtual line if it's currently extended (shifted) or is a dot line that may need rendering.
			if((winp != si.curwp || lnp != winp->w_face.wf_dot.lnp) && ((*vt.vlp & VFExt) ||
			 ((*vt.vlp & VFDot) && attrOn))) {
				if(lnp != NULL) {
#if VTDebug && 0
ushort u; char wkbuf[32]; sprintf(wkbuf,"%.*s",lnp->l_used < 20 ? lnp->l_used : 20,lnp->l_text);
mlprintf(MLHome | MLFlush,"supd_dex(): De-extending line '%s'...",wkbuf); getkey(true,&u,false);
#endif
					vt.taboff = 0;
					vtputln(lnp,attrOn);
					}
				vteeol();

				// This line is no longer extended nor a dot line that needs to be re-rendered.
				*vt.vlp &= ~(VFExt | VFDot);
				}
			if(lnp != NULL)
				lnp = lnp->l_nextp;
			} while(++row < endrow);

		// Onward to the next window.
		} while((winp = winp->w_nextp) != NULL);
	return rc.status;
	}

// Transfer the current line in given window to the virtual screen.
static int vupd_dotline(EWindow *winp) {
	Line *lnp;		// Line to update.
	int row;		// Physical screen line to update.

	// Search down to the line we want...
	lnp = winp->w_face.wf_toplnp;
	row = winp->w_toprow;
	while(lnp != winp->w_face.wf_dot.lnp) {
		++row;
		lnp = lnp->l_nextp;
		}

	// and update the virtual line.
	vt.taboff = modeset(MdIdxHScrl,winp->w_bufp) ? winp->w_face.wf_firstcol : (winp == si.curwp) ? si.cursp->s_firstcol : 0;
	if(vtmove(row,-vt.taboff) == Success) {
		vtputln(lnp,winp != si.curwp && (winp->w_bufp->b_flags & BFTermAttr) &&
		 (!modeset(MdIdxHScrl,winp->w_bufp) || winp->w_face.wf_firstcol == 0));
		vteeol();
		vt.taboff = 0;
		}
	return rc.status;
	}

// Write "== " to mode line on virtual screen and return count.
static int vupd_tab(int lchar) {

	vtputc(lchar);
	vtputc(lchar);
	vtputc(' ');
	return 3;
	}

// Write a null-terminated string to the virtual screen.  It is assumed the string does not contain attribute sequences.  Return
// number of characters written.
static int vtputs(char *str) {
	short c;
	int count = 0;

	while((c = *str++) != '\0')
		count += vtputc(c);
	return count;
	}

// Redisplay the mode line for the window pointed to by winp.  If popbuf is NULL, display a fully-formatted mode line containing
// the current buffer's name and filename (but in condensed form if the current terminal width is less than 96 columns);
// otherwise, display only the buffer name and filename of buffer "popbuf".  This is the only routine that has any idea of how
// the mode line is formatted.
static int vupd_modeline(EWindow *winp,Buffer *popbuf) {
	short c;
	int n;
	Buffer *bufp;
	Datum *datp;
	ModeSpec *msp;
	int lchar;
	Array *aryp = &mi.modetab;
	BufMode *bmp = winp->w_bufp->b_modes;
	short condensed = term.t_ncol < 80 ? -1 : term.t_ncol < 96 ? 1 : 0;
	char wkbuf[32];				// Work buffer.
	struct mlinfo {				// Mode display parameters.
		short leadch,trailch;
		} *mlip;
	static struct mlinfo mli[] = {
		{'(',')'},
		{'[',']'},
		{-1,-1}};
	static ushort progNameLen = sizeof(ProgName);
	static ushort progVerLen = sizeof(ProgVer);

	n = winp->w_toprow + winp->w_nrows;		// Row location.
	if(vtmove(n,0) != Success)			// Seek to right line on virtual screen.
		return rc.status;
	if(term.mdlColor[0] >= -1)			// Use color mode line if possible; otherwise, reverse video.
		(void) attron(COLOR_PAIR(term.maxPair - ColorPairML));
	else
		ttrev(false,true);
	if(winp == si.curwp)				// Make the current window's mode line stand out.
		lchar = '=';
	else if(si.opflags & OpHaveRV)
		lchar = ' ';
	else
		lchar = '-';

	// Full display?
	if(popbuf == NULL) {
		bufp = winp->w_bufp;
		vtputc((bufp->b_flags & BFNarrowed) ? SBNarrowed : lchar);	// "<" if narrowed.
		vtputc((bufp->b_flags & BFChanged) ? SBChanged : lchar);	// "*" if changed.
		vtputc(' ');
		n = 3;

		// Is window horizontally scrolled?
		if(winp->w_face.wf_firstcol > 0) {
			sprintf(wkbuf,"[<%d] ",winp->w_face.wf_firstcol);
			n += vtputs(wkbuf);
			}

		// Display the screen number if bottom window and there's more than one screen.
		if(winp->w_nextp == NULL && scrcount() > 1) {
			sprintf(wkbuf,"S%hu ",si.cursp->s_num);
			n += vtputs(wkbuf);
			}

		// If winp is current window...
		if(winp == si.curwp) {

			// Display keyboard macro recording state.
			if(kmacro.km_state == KMRecord) {
				if(si.opflags & OpHaveColor) {
					attr_t cattr = COLOR_PAIR(term.maxPair - ColorPairKMRI);
					char wkbuf[16];

					sprintf(wkbuf," %s ",text435);
							// "REC"

					// If color active, just change colors; otherwise, need to turn off reverse video first.
					if(term.mdlColor[0] >= -1) {
						(void) attron(cattr);
						n += vtputs(wkbuf);
						(void) attron(COLOR_PAIR(term.maxPair - ColorPairML));
						}
					else {
						ttrev(false,false);
						(void) attron(cattr);
						n += vtputs(wkbuf);
						(void) attroff(cattr);
						ttrev(false,true);
						}
					n += vtputc(' ');
					}
				else {
					n += vtputc('*');
					n += vtputs(text435);
						// "REC"
					n += vtputs("* ");
					}
				}

			// Display the line and/or column point position if enabled.
			if(modeset(MdIdxLine,winp->w_bufp)) {
				sprintf(wkbuf,"L:%ld ",getlinenum(bufp,winp->w_face.wf_dot.lnp));
				n += vtputs(wkbuf);
				}
			if(modeset(MdIdxCol,winp->w_bufp)) {
				sprintf(wkbuf,"C:%d ",getccol(NULL));
				n += vtputs(wkbuf);
				}
			}

		// Display the modes.  If condensed display, use short form.  If not bottom window, skip global modes.
		mlip = mli;
		do {
			// If bottom screen or not global modes...
			if(winp->w_nextp == NULL || mlip != mli) {
				c = mlip->leadch;
				for(;;) {
					if(mlip == mli) {
						// Get next global mode.
						if((datp = aeach(&aryp)) == NULL)
							break;
						msp = msptr(datp);

						// Skip if not a global mode or mode is disabled.
						if((msp->ms_flags & (MdGlobal | MdEnabled)) != (MdGlobal | MdEnabled))
							continue;
						}
					else if(bmp == NULL)
						break;
					else {
						// Get next buffer mode.
						msp = bmp->bm_modep;
						bmp = bmp->bm_nextp;
						}

					// Display mode if not hidden.
					if(!(msp->ms_flags & MdHidden)) {
						n += vtputc(c);
						c = ' ';
						if(condensed >= 0)
							n += vtputs(msp->ms_name);
						else {
							n += vtputc(msp->ms_name[0]);
							if(msp->ms_name[1] != '\0')
								n += vtputc(msp->ms_name[1]);
							}
						}
					}
				if(c != mlip->leadch) {
					n += vtputc(mlip->trailch);
					n += vtputc(' ');
					}
				}
			} while((++mlip)->leadch > 0);
#if 0
		// Display internal modes on modeline.
		vtputc(lchar);
		vtputc((winp->w_flags & WFMode) ? 'M' : lchar);
		vtputc((winp->w_flags & WFHard) ? 'H' : lchar);
		vtputc((winp->w_flags & WFEdit) ? 'E' : lchar);
		vtputc((winp->w_flags & WFMove) ? 'V' : lchar);
		vtputc((winp->w_flags & WFReframe) ? 'R' : lchar);
		vtputc(lchar);
		n += 8;
#endif
		if(n > 3)
			n += vupd_tab(lchar);
		}
	else {
		n = 0;
		bufp = popbuf;
		vtputc(lchar);
		n += vupd_tab(lchar) + 1;
		}

	// Display the buffer name.
	n += vtputs(bufp->b_bname) + 1;
	vtputc(' ');

	// Display the filename in the remaining space using strfit().
	if(bufp->b_fname != NULL) {
		char wkbuf[TT_MaxCols];

		n += vupd_tab(lchar);
		if(condensed < 0) {
			vtputc(*text34);
				// "File: "
			vtputc(':');
			vtputc(' ');
			n += 3;
			}
		else
			n += vtputs(text34);
				// "File: "
		n += vtputs(strfit(wkbuf,term.t_ncol - n - 1,bufp->b_fname,0)) + 1;
		vtputc(' ');
		}

	// If it's the current window, not a pop-up, "WkDir" mode, and room still available, display the working directory as
	// well.
	if(winp == si.curwp && popbuf == NULL && (mi.cache[MdIdxWkDir]->ms_flags & MdEnabled) && (term.t_ncol - n) > 12) {
		char wkbuf[TT_MaxCols];

		n += vupd_tab(lchar);
		n += vtputs(text274);
			// "WD: "
		n += vtputs(strfit(wkbuf,term.t_ncol - n - 1,si.cursp->s_wkdir,0)) + 1;
		vtputc(' ');
		}

	// If bottom window, not a pop, and root still available, display program name and version.
	if(!condensed && popbuf == NULL && winp->w_nextp == NULL) {
		int space = term.t_ncol - n;
		if(space >= progNameLen + progVerLen + 5) {
			while(n < term.t_ncol - (progNameLen + progVerLen + 1)) {
				vtputc(lchar);
				++n;
				}
			vtputc(' ');
			sprintf(wkbuf,"%s %s",Myself,Version);
			n += vtputs(wkbuf) + 2;
			vtputc(' ');
			}
		else if(space >= progNameLen + 4) {
			while(n < term.t_ncol - (progNameLen + 1)) {
				vtputc(lchar);
				++n;
				}
			vtputc(' ');
			n += vtputs(Myself) + 2;
			vtputc(' ');
			}
		}

	// Pad to full width and turn off all attributes.
	while(n < term.t_ncol) {
		vtputc(lchar);
		++n;
		}
	(void) attrset(0);
	return rc.status;
	}

// Update the position of the hardware cursor in the current window and handle extended lines.  This is the only update for
// simple moves.
static int vupd_cursor(void) {
	Line *lnp;
	int i,lastrow,*firstcolp;
	WindFace *wfp = &si.curwp->w_face;
	ushort modeflag = modeset(MdIdxHScrl,NULL) || modeset(MdIdxLine,NULL) || modeset(MdIdxCol,NULL) ? WFMode : 0;
	ushort *vlp;

	// Find the current row.
	lnp = wfp->wf_toplnp;
	lastrow = si.cursp->s_cursrow;
	si.cursp->s_cursrow = si.curwp->w_toprow;
	while(lnp != wfp->wf_dot.lnp) {
		++si.cursp->s_cursrow;
		lnp = lnp->l_nextp;
		}

	// Find the current column of point, ignoring terminal width.
	si.cursp->s_curscol = i = 0;
	while(i < wfp->wf_dot.off)
		si.cursp->s_curscol = newcol(lnp->l_text[i++],si.cursp->s_curscol);

	// Adjust cursor column by the current first column position of window (which is active when horizontal scrolling is
	// enabled) or line (if still on same terminal row as when this routine was last called).
	if(modeset(MdIdxHScrl,NULL))
		si.cursp->s_curscol -= *(firstcolp = &wfp->wf_firstcol);
	else {
		if(si.cursp->s_cursrow == lastrow)
			si.cursp->s_curscol -= si.cursp->s_firstcol;
		else
			si.cursp->s_firstcol = 0;
		firstcolp = &si.cursp->s_firstcol;
		}

	// Make sure it is not off the left edge of the screen or on a LineExt ($) character at the left edge.
	while(si.cursp->s_curscol < 0 || (si.cursp->s_curscol == 0 && si.cursp->s_firstcol > 0)) {
		if(*firstcolp >= vtc.hjumpcols) {
			si.cursp->s_curscol += vtc.hjumpcols;
			*firstcolp -= vtc.hjumpcols;
			}
		else {
			si.cursp->s_curscol += *firstcolp;
			*firstcolp = 0;
			}
		si.curwp->w_flags |= (WFHard | modeflag);
		}

	// Calculate window or line shift if needed.
	while(si.cursp->s_curscol >= term.t_ncol - 1) {
		si.cursp->s_curscol -= vtc.hjumpcols;
		*firstcolp += vtc.hjumpcols;
		si.curwp->w_flags |= (WFHard | modeflag);
		}

	// Mark line as a "dot" line and flag it if extended so that it can be re-rendered and/or de-extended by supd_dext() in
	// a future update() cycle.
	vlp = &vlflags[si.cursp->s_cursrow];
	if(*firstcolp > 0 && !modeset(MdIdxHScrl,NULL))
		*vlp |= (VFDot | VFExt);
	else
		*vlp = *vlp & ~VFExt | VFDot;

	// Update the virtual screen if current line or left shift was changed, or terminal attributes are "live".
	if(si.curwp->w_flags & WFHard) {
#if VTDebug && 0
ushort u; mlprintf(MLHome | MLFlush,"Calling vupd_wind() from vupd_cursor(): wf_firstcol %d, cursp->s_firstcol %d, cursp->s_curscol %d...",
wfp->wf_firstcol,si.cursp->s_firstcol,si.cursp->s_curscol); getkey(true,&u,false);
#endif
		if(vupd_wind(si.curwp) != Success)
			return rc.status;
		}
	else if((si.curwp->w_flags & WFEdit) && vupd_dotline(si.curwp) != Success)
		return rc.status;

	// Update mode line if (1), mode flag set; or (2), "Col" buffer mode set (point movement assumed); or (3), "Line"
	// buffer mode set and any window flag set.
	if((si.curwp->w_flags & WFMode) || modeset(MdIdxCol,NULL) || (si.curwp->w_flags && modeset(MdIdxLine,NULL)))
		vupd_modeline(si.curwp,NULL);
	si.curwp->w_flags = 0;

	// If horizontal scrolling mode not enabled and line is shifted, put a '$' in column 0.
	if(!modeset(MdIdxHScrl,NULL) && si.cursp->s_firstcol > 0)
		(void) mvaddch(si.cursp->s_cursrow,0,LineExt);
	return rc.status;
	}

// Make sure that the display is right.  This is a four-part process.  First, resize the windows in the current screen to match
// the current terminal dimensions if needed.  Second, scan all of the windows on the current screen looking for dirty
// (flagged) ones.  Check the framing and refresh the lines.  Third, make sure that the cursor row and column are correct for
// the current window.  And fourth, make the virtual and physical screens the same.  Entire update is skipped if keystrokes are
// pending or a keyboard macro is executing unless n != 0.  Physical screen update (only) is skipped if n <= 0.  Return status.
int update(int n) {
	EWindow *winp;
	int keyct;
	bool force = (n != INT_MIN && n != 0);

	// If we are not forcing the update...
	if(!force) {
		// If there are keystrokes waiting to be processed or a keyboard macro is being played back, skip this update.
		if(typahead(&keyct) != Success || keyct > 0 || kmacro.km_state == KMPlay)
			return rc.status;
		}

	// Current screen dimensions wrong?
	if(si.cursp->s_flags) {			// EScrResize set?
		EWindow *lastwp,*nextwp;
		int nrow;

		// Loop until vertical size of all windows matches terminal rows.
		do {
			// Does current screen need to grow vertically?
			if(term.t_nrow > si.cursp->s_nrow) {

				// Yes, go to the last window...
				winp = wnextis(NULL);

				// and enlarge it as needed.
				winp->w_nrows = (si.cursp->s_nrow = term.t_nrow) - winp->w_toprow - 2;
				winp->w_flags |= (WFHard | WFMode);
				}

			// Does current screen need to shrink vertically?
			else if(term.t_nrow < si.cursp->s_nrow) {

				// Rebuild the window structure.
				nextwp = si.cursp->s_wheadp;
				lastwp = NULL;
				nrow = 0;
				do {
					winp = nextwp;
					nextwp = winp->w_nextp;

					// Get rid of window if it is too low.
					if(winp->w_toprow >= term.t_nrow - 2) {

						// Save the "face" parameters.
						if(--winp->w_bufp->b_nwind == 0)
							winp->w_bufp->b_lastscrp = si.cursp;
						wftobf(winp,winp->w_bufp);

						// Update si.curwp and lastwp if needed.
						if(winp == si.curwp)
							if(wswitch(si.wheadp,0) <= FatalError)	// Defer any non-fatal error.
								return rc.status;
						if(lastwp != NULL)
							lastwp->w_nextp = NULL;

						// Free the structure.
						free((void *) winp);
						winp = NULL;
						}
					else {
						// Need to change this window size?
						if(winp->w_toprow + winp->w_nrows - 1 >= term.t_nrow - 2) {
							winp->w_nrows = term.t_nrow - winp->w_toprow - 2;
							winp->w_flags |= (WFHard | WFMode);
							}
						nrow += winp->w_nrows + 1;
						}

					lastwp = winp;
					} while(nextwp != NULL);
				si.cursp->s_nrow = nrow;
				}
			} while(si.cursp->s_nrow != term.t_nrow);

		// Update screen controls and mark screen for a redraw.
		si.cursp->s_ncol = term.t_ncol;
		si.cursp->s_flags = 0;
		si.opflags |= OpScrRedraw;
		}

	// If physical screen is garbage, force a redraw.
	if((n == INT_MIN || n > 0) && (si.opflags & OpScrRedraw)) {
		if(mlerase() != Success)				// Clear the message line.
			return rc.status;
		supd_wflags(NULL,WFHard | WFMode);			// Force all windows to update.
		si.opflags &= ~OpScrRedraw;				// Clear redraw flag.
		}

	// Check all windows and update virtual screen for any that need refreshing.
	winp = si.wheadp;
	do {
		if(winp->w_flags) {					// WFMove is set, at minimum.

			// The window has changed in some way: service it.
			if(wupd_reframe(winp) != Success)		// Check the framing.
				return rc.status;
			if((winp->w_flags & (WFEdit | WFMove)) ==	// Promote update if WFEdit and WFMove both set or
			 (WFEdit | WFMove) || ((winp->w_flags & WFMove)	// WFMove set and terminal attributes enabled in buffer.
			 && (winp->w_bufp->b_flags & BFTermAttr)))
				winp->w_flags |= WFHard;

			// Skip remaining tasks for current window.  These will be done by vupd_cursor().
			if(winp != si.curwp) {
				if((winp->w_flags & ~WFMode) == WFEdit) {
					if(vupd_dotline(winp) != Success)	// Update current (edited) line only.
						return rc.status;
					}
				else if(winp->w_flags & WFHard) {
					if(vupd_wind(winp) != Success)	// Update all lines.
						return rc.status;
					}
				if(winp->w_flags & WFMode)
					vupd_modeline(winp,NULL);	// Update mode line.
				winp->w_flags = winp->w_rfrow = 0;
				}
			}

		// On to the next window.
		} while((winp = winp->w_nextp) != NULL);

	// Update lines in current window if needed and recalculate the hardware cursor location.
	if(vupd_cursor() != Success)
		return rc.status;

	// Check for lines to de-extend.
	if(!modeset(MdIdxHScrl,NULL) && supd_dex() != Success)
		return rc.status;

	// If updating physical screen...
	if(n == INT_MIN || n > 0) {

		// Update the cursor position and flush the buffers.
		if(ttmove(si.cursp->s_cursrow,si.cursp->s_curscol) == Success)
			(void) ttflush();
		}

#if MMDebug & Debug_ScrDump
	dumpscreens("Exiting update()");
#endif
	return rc.status;
	}

// Return true if given key is bound to given command.
static bool iscmd(uint ek,int (*cfunc)(Datum *rp,int n,Datum **argpp)) {
	KeyBind *kbp;

	return (kbp = getbind(ek)) != NULL && kbp->k_targ.p_type == PtrCmd && kbp->k_targ.u.p_cfp->cf_func == cfunc;
	}

// Find key(s) bound to given command, convert to string form (if not prefixed), and write to "prmtp" underscored.  Return -1
// if error.  Used by bpop() for help prompt.
static int hkey(int (*cfunc)(Datum *rp,int n,Datum **argpp),DStrFab *prmtp) {
	KeyWalk kw = {NULL,NULL};
	KeyBind *kbp;
	short c,cmd0;
	char *cmd;
	char wkbuf[16];
	bool found = false;

	// Initialize literals.
	if(cfunc == backPage || cfunc == forwPage) {
		c = '|';
		cmd = "pg";
		cmd0 = (cfunc == backPage) ? '-' : '+';
		}
	else {
		c = ' ';
		cmd = "ln";
		cmd0 = (cfunc == backLine) ? '-' : '+';
		}

	// Search binding table for any keys bound to command.
	kbp = nextbind(&kw);
	do {
		if(kbp->k_targ.p_type == PtrCmd && kbp->k_targ.u.p_cfp->cf_func == cfunc) {

			// Found one.  Skip if prefixed or function key.
			if(!(kbp->k_code & (Prefix | FKey))) {
				if(dputf(prmtp,"%c%c%c%s%c%c",c,AttrSpecBegin,AttrULOn,ektos(kbp->k_code,wkbuf,false),
				 AttrSpecBegin,AttrULOff) != 0)
					return -1;
				c = '|';
				found = true;
				}
			}
		} while((kbp = nextbind(&kw)) != NULL);

	// Return now if no keys found.
	if(!found)
		return 0;

	// One or more keys found.  Finish up command.
	return dputf(prmtp," %c%c%c%s%c%c,",AttrSpecBegin,AttrBoldOn,cmd0,cmd,AttrSpecBegin,AttrBoldOff);
	}

// Display a buffer in a pop-up window and page it for the user.  If RendAltML flag is set, display buffer name and filename
// (only) on bottom mode line.  If RendWait flag is set, wait for user to press a key before returning (regardless of page
// size).  If RendShift flag is set, shift display left if needed so that no lines are truncated.  Current bindings, if any, for
// backPage, forwPage, backLine, and forwLine commands are recognized as well as 'b' (backward page), 'f' or space (forward
// page), 'u' (backward half page), 'd' (forward half page), 'g' (goto first page), 'G' (goto last page), ESC or 'q' (exit), and
// '?' (help).  Any non-navigation key gets pushed back into the input stream to be interpeted later as a command.  Return
// status.
//
// Note that this routine writes to the virtual screen directly and bypasses the usual update mechanism (the vupd_wind()
// function).  The mode line of the bottommost window is commandeered and rewritten.  Window flags are set before returning
// however, so that the original virtual screen is restored.
int bpop(Buffer *bufp,ushort flags) {
	Line *lnp1,*lnp,*lpmax;
	int row;			// Current screen row number.
	int disprows;			// Total number of display rows.
	int halfpage;			// Rows in a half page.
	int n;				// Rows to move.
	ushort ek;			// Input extended key.
	int firstcol = 0;		// First column displayed.
	char *hprompt = NULL;		// Help prompt;
	bool firstpass = true;

	// Find last window on screen and rewrite its mode line.  Display special mode line if requested.
	vupd_modeline(wnextis(NULL),flags & RendAltML ? bufp : NULL);

	// Check if buffer will fit on one page and if not, set lpmax to first line of last page.
	disprows = term.t_nrow - 2;
	lpmax = NULL;
	n = 0;
	lnp = bufp->b_lnp;
	do {
		if(++n > disprows) {

			// Find beginning of last page.
			lpmax = bufp->b_lnp;
			n = disprows;
			do {
				lpmax = lpmax->l_prevp;
				} while(--n > 0);
			break;
			}
		} while((lnp = lnp->l_nextp) != NULL);

	// Determine left shift, if any.
	if(flags & RendShift) {
		Dot dot;
		int col,maxcol = 0;

		// Scan buffer, find column position of longest line...
		dot.lnp = bufp->b_lnp;
		do {
			dot.off = dot.lnp->l_used;
			if((col = getccol(&dot)) > maxcol)
				maxcol = col;
			} while((dot.lnp = dot.lnp->l_nextp) != NULL);

		// and set shift amount so that rightmost portion of longest line will be visible.
		if(maxcol > term.t_ncol)
			firstcol = maxcol - (term.t_ncol - 1);
		}

	// Begin at the beginning.
	lnp1 = bufp->b_lnp;
	halfpage = disprows / 2;
	n = 0;

	// Display a page (beginning at line lnp1 + n) and prompt for a naviagtion command.  Loop until exit key entered or
	// RendWait flag not set and buffer fits on one page (lpmax is NULL).
	for(;;) {
		lnp = lnp1;

		// Moving backward?
		if(n < 0) {
			if(lpmax != NULL)
				do {
					// At beginning of buffer?
					if(lnp1 == bufp->b_lnp)
						break;

					// No, back up one line.
					lnp1 = lnp1->l_prevp;
					} while(++n < 0);
			}

		// Moving forward?
		else if(n > 0) {
			if(lpmax != NULL)
				do {
					// At end of buffer or max line?
					if(lnp1->l_nextp == NULL || lnp1 == lpmax)
						break;

					// No, move forward one line.
					lnp1 = lnp1->l_nextp;
					} while(--n > 0);
			}

		// Illegal command?
		if(n != 0 && lnp1 == lnp)

			// Yes, ignore it.
			n = 0;
		else {
			// Found first row... display page.
			lnp = lnp1;
			row = 0;
			do {
				// At end of buffer?
				if(lnp == NULL) {

					// Yes, erase remaining lines on virtual screen.
					while(row < disprows) {
						if(vtmove(row,0) != Success)
							return rc.status;
						(void) clrtoeol();
						*vt.vlp = 0;
						++row;
						}
					break;
					}

				// Update the virtual screen image for this line.
				vt.taboff = firstcol;
				if(vtmove(row,-vt.taboff) != Success)
					return rc.status;
				vtputln(lnp,firstcol == 0 && (bufp->b_flags & BFTermAttr));
				vteeol();
				vt.taboff = 0;

				// If line is shifted, put a '$' in column 0.
				if(firstcol > 0)
					(void) mvaddch(row,0,LineExt);

				// On to the next line.
				lnp = lnp->l_nextp;
				} while(++row < disprows);

			// Screen is full.  Copy the virtual screen to the physical screen.
			if(ttflush() != Success)
				return rc.status;

			// Bail out if called from terminp() and one-page buffer.
			if(firstpass && !(flags & RendWait) && lpmax == NULL)
				goto Retn;
			firstpass = false;
			}

		// Display prompt.
		if(mlputs(MLHome | MLFlush,hprompt != NULL ? hprompt : lpmax == NULL || lnp1 == lpmax ? text201 :
		 ": ") != Success || mlflush() != Success)
													// "End: "
			return rc.status;

		// Get response.
		for(;;) {
			// Get a keystroke and decode it.
			if(getkey(true,&ek,false) != Success)
				return rc.status;

			// Exit?
			if(ek == (Ctrl | '[') || ek == 'q')
				goto Retn;

			// Forward whole page?
			if(ek == ' ' || ek == 'f' || iscmd(ek,forwPage)) {
				n = disprows - si.overlap;
				break;
				}

			// Forward half page?
			if(ek == 'd') {
				n = halfpage;
				break;
				}

			// Backward whole page?
			if(ek == 'b' || iscmd(ek,backPage)) {
				n = si.overlap - disprows;
				break;
				}

			// Backward half page?
			if(ek == 'u') {
				n = -halfpage;
				break;
				}

			// Forward a line?
			if(iscmd(ek,forwLine)) {
				n = 1;
				break;
				}

			// Backward a line?
			if(iscmd(ek,backLine)) {
				n = -1;
				break;
				}

			// First page?
			if(ek == 'g') {
				if(lpmax == NULL || lnp1 == bufp->b_lnp)
					n = -1;			// Force beep.
				else {
					lnp1 = bufp->b_lnp;
					n = 0;
					}
				break;
				}

			// Last page?
			if(ek == 'G') {
				if(lpmax == NULL || lnp1 == lpmax)
					n = 1;			// Force beep.
				else {
					lnp1 = lpmax;
					n = 0;
					}
				break;
				}

			// Help?
			if(ek == '?') {
				DStrFab prompt;

				// Build prompt and display it:
			// SPC f C-v +pg, b C-z -pg, d +half, u -half, C-n +ln, C-p -ln, g top, G bot, ESC q quit, ? help:
				if(dopentrk(&prompt) != 0 ||
				 dputf(&prompt,"%c%cSPC%c%c|%c%cf%c%c",AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrULOff,
				 AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrULOff) != 0 ||
				 hkey(forwPage,&prompt) != 0 ||
				dputf(&prompt," %c%cb%c%c",AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrULOff) != 0 ||
				hkey(backPage,&prompt) != 0 ||
				dputf(&prompt," %c%cd%c%c %c%c+half%c%c, %c%cu%c%c %c%c-half%c%c,",AttrSpecBegin,AttrULOn,
				 AttrSpecBegin,AttrULOff,AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,AttrSpecBegin,AttrULOn,
				 AttrSpecBegin,AttrULOff,AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff) != 0 ||
				hkey(forwLine,&prompt) != 0 || hkey(backLine,&prompt) != 0 ||
				dputf(&prompt,
		 " %c%cg%c%c %c%ctop%c%c, %c%cG%c%c %c%cbot%c%c, %c%cESC%c%c|%c%cq%c%c %c%cquit%c%c, %c%c?%c%c %c%chelp%c%c: ",
		 		 AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrULOff,AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,
		 		 AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrULOff,AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,
		 		 AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrULOff,AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrULOff,
		 		 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrULOff,
		 		 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff) != 0 || dclose(&prompt,sf_string) != 0)
		 			return librcset(Failure);
				if(mlputs(MLHome | MLTermAttr | MLFlush,prompt.sf_datp->d_str) != Success)
					return rc.status;
				}
			else {
				// Other key.  "Unget" the key for reprocessing and return.
				tungetc(ek);
				goto Retn;
				}
			}
		}
Retn:
	// Force virtual screen refresh.
	supd_wflags(NULL,WFHard | WFMode);
	if(flags & RendWait)
		(void) mlerase();

	return rc.status;
	}
