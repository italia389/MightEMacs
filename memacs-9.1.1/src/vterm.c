// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// vterm.c	Terminal display and window management routines for MightEMacs.
//
// Programming Notes:
// 1. There are two sections: (1), functions that update the virtual screen; and (2), functions that make the physical screen
//    the same as the virtual screen.  These functions use flags that are set in the windows by the command routines.
//
// 2. Bold, reverse video, and underline are supported when the physical screen is written to the terminal, under certain
//    conditions:
//	* The buffer containing line(s) with attributes must have its BFTermAttr flag set.
//	* The window displaying the buffer cannot be horizontally shifted (including a pop-up window).
// If these conditions are met, then any line in any window on the current screen (other than the current line in the current
// window) that contains any of the attribute sequences described below will be rendered with the sequences converted to the
// corresponding attributes (on or off).  Attribute sequences begin with the AttrSeqBegin (~) character and may be any of the
// following:
//	~b	Begin bold.			~r	Begin reverse.			~u	Begin underline.
//	~B	End bold (if TT_Curses).	~R	End reverse (if TT_Curses)	~#u	Begin underline, skip spaces.
//	~0	End all attributes.		~~	Literal ~.			~U	End underline.

#include "os.h"
#include <stdarg.h>
#include "std.h"
#include "bind.h"
#include "file.h"

/*** Local declarations ***/

#define VTDebug		0

// This structure holds the information about each line appearing on the video display.  The display system uses arrays of lines
// allocated at startup which can accommodate the maximum possible screen size.  There is one array for the virtual screen and
// one for the physical screen, both of which hold only the visible portions of buffers at any given time.  On any given line,
// characters past v_len are assumed to be blank.  The display system compares the virtual screen to the physical screen during
// an update cycle and writes changes (only) to the terminal, which minimizes video updating.
typedef struct {
	ushort v_flags;			// Flags.
	int v_len;			// Current length of visible portion of line (including terminal attribute sequences).
	short v_attrlen;		// Length of terminal attribute sequences.
	char v_text[1];			// Screen data.
	} VideoLine;

// Virtual line flags.
#define VFChgd		0x0001		// Virtual line was changed.
#define VFExt		0x0002		// Extended virtual line (beyond terminal width).
#define VFRev		0x0004		// Display line in reverse video (mode line).
#define VFDot		0x0008		// Dot line in associated window.
#define VFTermAttr	0x0010		// Process terminal attribute sequences in line.

static VideoLine **pscreen;		// Physical screen.
static VideoLine **vscreen;		// Virtual screen.
static int taboff = 0;			// Offset for expanding tabs to virtual screen when line(s) are left-shifted.
static int vtcol = 0;			// Column location of VTerm cursor.
static int vtrow = 0;			// Row location of VTerm cursor.

// Initialize the data structures used by the display system.  The virtual and physical screens are allocated and the operating
// system's terminal I/O channels are opened.  Return status.
int vtinit(void) {
	VideoLine *vvlp,*pvlp,**vscrp,**vscrpz,**pscrp;
	static char myname[] = "vtinit";

	if(TTopen() != Success ||	// Open the screen and keyboard.
	 TTattroff() != Success)	// Turn off all attributes.
		return rc.status;

	// Allocate the virtual screen and physical (shadow) screen (arrays of pointers).  The vertical size is one less row
	// than the maximum because the message line is managed separately from the display system.
	if((vscreen = (VideoLine **) malloc((term.t_mrow - 1) * sizeof(VideoLine *))) == NULL ||
	 (pscreen = (VideoLine **) malloc((term.t_mrow - 1) * sizeof(VideoLine *))) == NULL)
		return rcset(Panic,0,text94,myname);
			// "%s(): Out of memory!"

	// For every line in the display terminal...
	vscrpz = (vscrp = vscreen) + term.t_mrow - 1;
	pscrp = pscreen;
	do {
		// Allocate a virtual screen line and physical screen line.
		if((vvlp = (VideoLine *) malloc(sizeof(VideoLine) + term.t_mcol)) == NULL ||
		 (pvlp = (VideoLine *) malloc(sizeof(VideoLine) + term.t_mcol)) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"

		// Initialize lines and save in respective arrays.
		pvlp->v_len = vvlp->v_len = pvlp->v_flags = vvlp->v_flags = pvlp->v_attrlen = vvlp->v_attrlen = 0;
		*vscrp++ = vvlp;
		*pscrp++ = pvlp;
		} while(vscrp < vscrpz);

	return rc.status;
	}

// Set the virtual cursor to the specified row and column on the virtual screen.  There is no checking for nonsense values.
static void vtmove(int row,int col) {

	vscreen[row]->v_attrlen = 0;
	vtrow = row;
	vtcol = col;
	}

// Write a character to the virtual screen.  The virtual row and column are updated.  If we are off the left edge of the screen,
// don't write the character.  If the line is too long put a "$" in the last column.  Non-printable characters are converted to
// visible form.  Return number of characters written.
static int vtputc(short c) {
	int vtcol0 = vtcol;
	VideoLine *vlp = vscreen[vtrow];	// Line receiving this character.
	int rmargin = term.t_ncol + vlp->v_attrlen;
	if(vtcol >= rmargin) {

		// We are past the right edge!
		if(vtcol++ == rmargin)
			vlp->v_text[rmargin - 1] = LineExt;
		}
	else if(c == '\t') {

		// Output hardware tab as the right number of spaces.
		do {
			vtputc(' ');
			} while((vtcol + taboff) % si.htabsize != 0);
		}
	else if(c < ' ' || c == 0x7F) {

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
		if(vtcol >= 0)
			vlp->v_text[vtcol] = c;
		++vtcol;
		}

	return vtcol - vtcol0;
	}

// Write a null-terminated string to the virtual screen.  Return number of characters written.
static int vtputs(char *str) {
	short c;
	int count = 0;

	while((c = *str++) != '\0')
		count += vtputc(c);
	return count;
	}

// Write a buffer line to the virtual screen.
static void vtputln(Line *lnp) {
	char *str,*strz;

	strz = (str = lnp->l_text) + lnp->l_used;
	while(str < strz)
		vtputc(*str++);
	}

// Erase to end of line at current cursor position in virtual line.
static void vteeol(void) {
	int rmargin = term.t_ncol + vscreen[vtrow]->v_attrlen;
	vscreen[vtrow]->v_len = (vtcol < 0) ? 0 : (vtcol < rmargin) ? vtcol : rmargin;
	}

// Send a command to the terminal to move the hardware cursor to row "row" and column "col" (origin 0).  Update si.mlcol if
// applicable and return status.
int movecursor(int row,int col) {

	(void) TTmove(row,col);
	if(row == term.t_nrow - 1)
		si.mlcol = col;
	return rc.status;
	}

// Scan a string for terminal attribute sequences and return the total number of characters found that are not visible when the
// string is displayed (zero if none).  Invalid sequences or any that would begin at or past "maxcol" are ignored.  If len < 0,
// string is assumed to be null terminated.
short attrCount(char *str,int len,int maxcol) {
	bool altForm;
	char *strz;
	short c;
	int curcol;			// Running count of visible characters.

	if(len < 0)
		len = strlen(str);
	strz = str + len;
	curcol = len = 0;
	while(str < strz && curcol < maxcol) {
		if((c = *str++) == AttrSeqBegin) {
			if(str == strz)
				break;
			altForm = false;
			if((c = *str++) == AttrAlt) {
				if(str == strz)
					break;
				altForm = true;
				c = *str++;
				}

			switch(c) {
				case AttrSeqBegin:
					++len;
					++curcol;
					break;
				case AttrAllOff:
				case AttrBoldOn:
#if TT_Curses
				case AttrBoldOff:
#endif
				case AttrRevOn:
#if TT_Curses
				case AttrRevOff:
#endif
				case AttrULOn:
				case AttrULOff:
					len += altForm ? 3 : 2;
					break;
				default:
					// Invalid spec letter found; display raw.
					curcol += altForm ? 3 : 2;
				}
			}
		else
			++curcol;
		}

	return len;
	}

// Scan a buffer line for terminal attribute sequences and update the given row in the virtual screen with the results.  Set
// v_attrlen to the total number of characters found that are not visible when the line is displayed (zero if none), ignoring
// any sequences that would begin past the current terminal width when the line is displayed.  If any valid sequences are found,
// also set the VFTermAttr flag.
static void checkAttr(Line *lnp,int row) {
	VideoLine *vlp = vscreen[row];
	if((vlp->v_attrlen = attrCount(lnp->l_text,lnp->l_used,term.t_ncol)) > 0)
		vlp->v_flags |= VFTermAttr;
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

// Compare virtual and physical lines.  Return number of characters that match.
static int ldiff(char *virLeft,char *virRight,char *phyLeft,char *phyRight) {
	char *virLeft0 = virLeft;
	while(virLeft < virRight && phyLeft < phyRight && *virLeft == *phyLeft) {
		++virLeft;
		++phyLeft;
		}
	return virLeft - virLeft0;
	}

// Process a terminal attribute specification (except ~~).  Return status.
static int pupd_attr(char *virLeft,char *virLeftBlank,ushort *attrlenp,bool *skipSpacep) {
	short c;
	ushort attrlen = 0;
	bool attrOn;
	bool altForm = false;

	if(++virLeft == virLeftBlank)
		goto Retn;
	if((c = *virLeft++) == AttrAlt) {
		if(virLeft == virLeftBlank)
			goto Retn;
		altForm = true;
		attrlen = 1;
		c = *virLeft++;
		}

	switch(c) {
		case AttrAllOff:
			*skipSpacep = false;
			(void) TTattroff();
			goto Retn2;
		case AttrBoldOn:
#if !TT_Curses
			(void) TTbold();
#else
			attrOn = true;
			goto Bold;
		case AttrBoldOff:
			attrOn = false;
Bold:
			(void) TTbold(attrOn);
#endif
			goto Retn2;
		case AttrRevOn:
#if !TT_Curses
			(void) TTrev();
#else
			attrOn = true;
			goto Rev;
		case AttrRevOff:
			attrOn = false;
Rev:
			(void) TTrev(attrOn);
#endif
			goto Retn2;
		case AttrULOn:
			attrOn = true;
			if(altForm)
				*skipSpacep = true;
			goto UL;
		case AttrULOff:
			attrOn = false;
			*skipSpacep = false;
UL:
			(void) TTul(attrOn);
			goto Retn2;
		default:
			attrlen = 0;
			goto Retn;
		}
Retn2:
	attrlen += 2;
Retn:
	*attrlenp = attrlen;
	return rc.status;
	}

// Update a line on the physical screen from the virtual screen, given row, virtual screen image, and physical screen image.
// Update the physical row and column variables.  This routine does not know how to use insert or delete character sequences; we
// are using VT52 functionality.  It does try to exploit erase-to-end-of-line, however.  Return status.
static int pupd_line(int row,VideoLine *vvlp,VideoLine *pvlp) {
	char *virLeft;			// Left pointer to virtual line.
	char *phyLeft;			// Left pointer to physical line.
	char *virRight;			// Right pointer to virtual line.
	char *phyRight;			// Right pointer to physical line.
	char *virLeftBlank;		// Leftmost trailing blank in virtual line.
	int updateCol;			// First column of physical line needing an update.
	int len = -1;
	ushort attrlen;			// Length of terminal attribute sequence.
	bool revChange = false;		// Reverse video change?
	bool skipEEOL = false;		// Skip erase-to-end-of-line.
	bool skipSpace;			// Don't underline spaces?

	// Set up pointers to virtual and physical lines.
	virLeftBlank = virRight = (virLeft = vvlp->v_text) + vvlp->v_len;
	phyRight = (phyLeft = pvlp->v_text) + pvlp->v_len;
	updateCol = 0;

#if VTDebug
fprintf(logfile,"pupd_line(): virFlags: %.8X, virLeft: %.8X, virLen: %d '%.*s', phyFlags: %.8X, phyLeft: %.8X, phyLen: %d\n",
 vvlp->v_flags,(uint) virLeft,vvlp->v_len,vvlp->v_len,virLeft,pvlp->v_flags,(uint) phyLeft,pvlp->v_len);
#endif
	// Reverse video change?
	if((vvlp->v_flags & VFRev) || (pvlp->v_flags & VFRev)) {

		// Yes, update whole line.
		revChange = true;
		if(vvlp->v_flags & VFRev) {
			pvlp->v_flags |= VFRev;
#if TT_TermCap
			goto Update;
#endif
			}
		else
			pvlp->v_flags &= ~VFRev;
		if(vvlp->v_len > 0)
			goto BlankCheck;
		goto Update;
		}

	// Not a forced update.  Skip update if terminal attributes match and lines are identical.
	if((vvlp->v_flags & VFTermAttr) != (pvlp->v_flags & VFTermAttr) || vvlp->v_len != pvlp->v_len ||
	 (len = ldiff(virLeft,virRight,phyLeft,phyRight)) != vvlp->v_len) {

		// Lines differ.  Move past any common characters at left end.
		if((vvlp->v_flags & VFTermAttr) || (pvlp->v_flags & VFTermAttr)) {
			while(virLeft < virRight && phyLeft < phyRight && *virLeft == *phyLeft &&
			 *virLeft != AttrSeqBegin && *virRight != AttrSeqBegin) {
				++virLeft;
				++phyLeft;
				++updateCol;
				}
			goto LineCheck;
			}
		if(len < 0)
			len = ldiff(virLeft,virRight,phyLeft,phyRight);
		virLeft += len;
		phyLeft += len;
		updateCol += len;
LineCheck:
		// Any remaining characters in virtual line?
		if(virLeft < virRight) {

			// Yes.  Check for trailing spaces in virtual line if lines are different lengths.
			if(vvlp->v_len != pvlp->v_len) {
BlankCheck:
				while(virLeftBlank > virLeft && virLeftBlank[-1] == ' ')
					--virLeftBlank;
				if(virRight - virLeftBlank <= 3)	// Use only if run is > 3 characters.
					virLeftBlank = virRight;
				}
			else if(!revChange && !(vvlp->v_flags & VFTermAttr) && !(pvlp->v_flags & VFTermAttr)) {

				// Lines are same length, not reverse video change, and neither line contains terminal attribute
				// sequences.  Move past any common characters at right end.
				while(virRight > virLeft && virRight[-1] == phyRight[-1]) {
					--virRight;
					--phyRight;
					}
				virLeftBlank = virRight;
				skipEEOL = true;
				}
			}

		// If virtual and physical lines differ, reverse video change, or either line contains terminal attribute
		// sequences, proceed with update.
		if(virLeft < virRight || vvlp->v_len != pvlp->v_len || revChange ||
		 (vvlp->v_flags & VFTermAttr) || (pvlp->v_flags & VFTermAttr)) {
Update:
#if VTDebug && 0
if(row == 1) {
	ushort u;
	mlprintf(MLHome | MLFlush,
"pupd_line(): row %d, v_len %d, p_len %d, v_attrlen %d, v_flags %d, updateCol %d, ptrdiff %d, vch127 %c, pch127 %c... ",row,
	 (int) vscreen[row]->v_len,(int) pscreen[row]->v_len,(int) vscreen[row]->v_attrlen,(int) vscreen[row]->v_flags,
	 updateCol,(int)(virLeftBlank - virLeft),(int) vscreen[row]->v_text[127],(int) pscreen[row]->v_text[127]);
	getkey(&u,false);
	}
#endif
			// Move to the beginning of the text to update.
			if(movecursor(row,updateCol) != Success)
				return rc.status;

			// Transfer characters up to beginning of "blank" run at right (if any).
#if TT_Curses
			if(revChange && TTrev(vvlp->v_flags & VFRev) != Success)
#else
			if(revChange && ((vvlp->v_flags & VFRev) ? TTrev() : TTattroff()) != Success)
#endif
				return rc.status;
			skipSpace = false;
			while(virLeft < virLeftBlank) {
				if(*virLeft == AttrSeqBegin && (vvlp->v_flags & VFTermAttr)) {
					if((virLeftBlank - virLeft) > 1 && virLeft[1] == AttrSeqBegin) {
						if(TTputc(AttrSeqBegin) != Success)
							return rc.status;
#if TT_TermCap
						++updateCol;
#endif
						attrlen = 2;
						}
					else {
						if(pupd_attr(virLeft,virLeftBlank,&attrlen,&skipSpace) != Success)
							return rc.status;
						if(attrlen == 0)
							goto Plain;
						}
					do {
						*phyLeft++ = *virLeft++;
						} while(--attrlen > 0);
					}
				else {
Plain:
					if(*virLeft == ' ' && skipSpace && TTul(false) != Success)
						return rc.status;
					if(TTputc(*virLeft) != Success)
						return rc.status;
					if(*virLeft == ' ' && skipSpace && TTul(true) != Success)
						return rc.status;
#if TT_TermCap
					++updateCol;
#endif
					*phyLeft++ = *virLeft++;
					}
				}

			// Erase to end of line if not a skip and current virtual column less than physical line length or
			// reverse video change or either line contains terminal attribute sequences.
			if(!skipEEOL && ((virLeftBlank - vvlp->v_text - vvlp->v_attrlen) < pvlp->v_len - pvlp->v_attrlen ||
			 revChange)) {
#if TT_TermCap
				// TERMCAP does not tell us if the terminal propagates the current attributes to the end of the
				// line when an erase-to-end-of-line sequence is sent (which is needed here if reverse video is
				// on).  So we don't use EEOL in that case.
				if(vvlp->v_flags & VFRev)
					while(updateCol++ < term.t_ncol) {
						if(TTputc(' ') != Success)
							return rc.status;
						}
				else if(TTeeol() != Success)
					return rc.status;
#else
				if(TTeeol() != Success)
					return rc.status;
#endif
				}

			// Make physical line match virtual line.
			while(virLeftBlank < virRight)
				*phyLeft++ = *virLeftBlank++;

			// Lastly, adjust terminal attributes' flag.
			if(vvlp->v_flags & VFTermAttr)
				pvlp->v_flags |= VFTermAttr;
			else
				pvlp->v_flags &= ~VFTermAttr;
			}

		pvlp->v_len = vvlp->v_len;			// Update physical line length.
		pvlp->v_attrlen = vvlp->v_attrlen;
#if TT_Curses
		if(vvlp->v_flags & VFTermAttr)			// Always leave reverse video and terminal attributes off.
			(void) TTattroff();
		else if(revChange && (vvlp->v_flags & VFRev))
			(void) TTrev(false);
#else
		if((revChange && (vvlp->v_flags & VFRev)) ||
		 (vvlp->v_flags & VFTermAttr))			// Always leave reverse video and terminal attributes off.
			(void) TTattroff();
#endif
		}

	vvlp->v_flags &= ~VFChgd;				// Flag this line as updated.
	return rc.status;
	}

// Transfer the virtual screen to the physical screen.  Force it if "force" is true.  Return status.
static int pupd_all(bool force) {
	VideoLine **vscrp,**pscrp;
	int row;
	int keyct;

	row = 0;
	vscrp = vscreen;
	pscrp = pscreen;
	do {
		// For each line that needs to be updated...
		if((*vscrp)->v_flags & VFChgd) {
			if(!force) {
				if(typahead(&keyct) != Success || keyct > 0)
					return rc.status;
				}
			if(pupd_line(row,*vscrp,*pscrp) != Success)
				return rc.status;
			}
		++vscrp;
		++pscrp;
		} while(++row < term.t_nrow - 1);		// Exclude message line.

	return rc.status;
	}

// Transfer all lines in given window to virtual screen.
static void vupd_wind(EWindow *winp) {
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
		vscreen[row]->v_flags = vscreen[row]->v_flags & ~(VFRev | VFTermAttr) | VFChgd;
		vscreen[row]->v_attrlen = 0;
		taboff = modeset(MdIdxHScrl,winp->w_bufp) ? winp->w_face.wf_firstcol : lnp == curlnp ? si.cursp->s_firstcol : 0;
		vtmove(row,-taboff);
		if(taboff == 0)
			vscreen[row]->v_flags &= ~VFExt;

		// If we are not at the end...
		if(lnp != NULL) {
			if(attrOn && lnp != curlnp)
				checkAttr(lnp,row);
#if VTDebug && 0
if(!all) { char wkbuf[32]; sprintf(wkbuf,"%.*s",lnp->l_used < 20 ? lnp->l_used : 20,lnp->l_text);
ushort u; mlprintf(MLHome | MLFlush,"Calling vtputln(%s...) from vupd_wind(): row %d, taboff %d, v_flags %d... ",
wkbuf,row,taboff,(int) vscreen[row]->v_flags); getkey(&u,false);}
#endif
			vtputln(lnp);
			lnp = lnp->l_nextp;
			}

		// Truncate virtual line (set length).
		vteeol();
		} while(++row < endrow);
	taboff = 0;
	}

// De-extend and/or re-render any line in any window on the virtual screen that needs it; for example, a line in the current
// window that in a previous update() cycle was the current line, or the current line in another window that was previously the
// current window.  Such lines were marked with the VFExt and VFDot flags by the wupd_cursor() function.  The current line in
// the current window is skipped, however.
static void supd_dex(void) {
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
			// Update virtual line if it's currently extended (shifted) or is a dot line that may need rendering.
			if((winp != si.curwp || lnp != winp->w_face.wf_dot.lnp) && ((vscreen[row]->v_flags & VFExt) ||
			 ((vscreen[row]->v_flags & VFDot) && attrOn && vscreen[row]->v_attrlen == 0))) {
				if(lnp == NULL)
					vtmove(row,0);
				else {
#if VTDebug && 0
ushort u; char wkbuf[32]; sprintf(wkbuf,"%.*s",lnp->l_used < 20 ? lnp->l_used : 20,lnp->l_text);
mlprintf(MLHome | MLFlush,"supd_dex(): De-extending line '%s'...",wkbuf); getkey(&u,false);
#endif
					vtmove(row,taboff = 0);
					if(attrOn)
						checkAttr(lnp,row);
					vtputln(lnp);
					}
				vteeol();

				// This line is no longer extended nor a dot line that needs to be re-rendered.
				vscreen[row]->v_flags = vscreen[row]->v_flags & ~(VFExt | VFDot) | VFChgd;
				}
			if(lnp != NULL)
				lnp = lnp->l_nextp;
			} while(++row < endrow);

		// Onward to the next window.
		} while((winp = winp->w_nextp) != NULL);
	}

// The screen is garbage and needs major repair.  Clear the physical screen and force a full update.  Return status.
static int pupd_redraw(void) {
	VideoLine **vscrp,**vscrpz,**pscrp;

	vscrpz = (vscrp = vscreen) + (term.t_nrow - 1);		// Exclude message line.
	pscrp = pscreen;
	do {							// Erase the physical screen...
		(*vscrp)->v_flags |= VFChgd;			// and mark all lines in virtual screen as changed.
		(*pscrp)->v_len = (*pscrp)->v_flags = 0;
		++pscrp;
		} while(++vscrp < vscrpz);

	if(movecursor(0,0) != Success || TTeeop() != Success)	// Erase the terminal window.
		return rc.status;
	si.opflags &= ~OpScrRedraw;				// Clear redraw flag.

	return rc.status;
	}

// Transfer the current line in given window to the virtual screen.
static void vupd_dotline(EWindow *winp) {
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
	vscreen[row]->v_flags = vscreen[row]->v_flags & ~(VFRev | VFTermAttr) | VFChgd;
	vscreen[row]->v_attrlen = 0;
	taboff = modeset(MdIdxHScrl,winp->w_bufp) ? winp->w_face.wf_firstcol : (winp == si.curwp) ? si.cursp->s_firstcol : 0;
	vtmove(row,-taboff);
	if(winp != si.curwp && (winp->w_bufp->b_flags & BFTermAttr) &&
	 (!modeset(MdIdxHScrl,winp->w_bufp) || winp->w_face.wf_firstcol == 0))
		checkAttr(lnp,row);
	vtputln(lnp);
	vteeol();
	taboff = 0;
	}

// Write "== " to mode line on virtual screen and return count.
static int vupd_tab(int lchar) {

	vtputc(lchar);
	vtputc(lchar);
	vtputc(' ');
	return 3;
	}

// Redisplay the mode line for the window pointed to by winp.  If popbuf is NULL, display a fully-formatted mode line containing
// the current buffer's name and filename (but in condensed form if the current terminal width is less than 96 columns);
// otherwise, display only the buffer name and filename of buffer "popbuf".  This is the only routine that has any idea of how
// the mode line is formatted.
void vupd_modeline(EWindow *winp,Buffer *popbuf) {
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

	// Note that we assume that setting REVERSE will cause the terminal driver to draw with the inverted relationship of
	// fcolor and bcolor, so that when we say to set the foreground color to "white" and background color to "black", the
	// fact that "reverse" is enabled means that the terminal driver actually draws "black" on a background of "white".
	// Makes sense, no?  This way, devices for which the color controls are optional will still get the "reverse" signals.
	vscreen[n]->v_flags |= (VFChgd | VFRev);	// Redraw mode line in reverse video.
	vtmove(n,0);					// Seek to right line on virtual screen.
	if(winp == si.curwp)				// Make the current window's mode line stand out.
		lchar = '=';
	else if(si.opflags & OpHaveRev)
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
			if(kmacro.km_state == KMRecord)
				n += vtputs("*R* ");

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

	// Pad to full width.
	while(n < term.t_ncol) {
		vtputc(lchar);
		++n;
		}
	vteeol();
	}

// Update the position of the hardware cursor in the current window and handle extended lines.  This is the only update for
// simple moves.
static void vupd_cursor(void) {
	Line *lnp;
	int i,lastrow,*firstcolp;
	WindFace *wfp = &si.curwp->w_face;
	ushort modeflag = modeset(MdIdxHScrl,NULL) || modeset(MdIdxLine,NULL) || modeset(MdIdxCol,NULL) ? WFMode : 0;

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
	if(*firstcolp > 0 && !modeset(MdIdxHScrl,NULL))
		vscreen[si.cursp->s_cursrow]->v_flags |= (VFDot | VFExt);
	else
		vscreen[si.cursp->s_cursrow]->v_flags = vscreen[si.cursp->s_cursrow]->v_flags & ~VFExt | VFDot;

	// Update the virtual screen if current line or left shift was changed, or terminal attributes are "live".
	if(si.curwp->w_flags & WFHard) {
#if VTDebug && 0
ushort u; mlprintf(MLHome | MLFlush,"Calling vupd_wind() from vupd_cursor(): wf_firstcol %d, cursp->s_firstcol %d, cursp->s_curscol %d... ",
wfp->wf_firstcol,si.cursp->s_firstcol,si.cursp->s_curscol); getkey(&u,false);
#endif
		vupd_wind(si.curwp);
		}
	else if((si.curwp->w_flags & WFEdit) || vscreen[si.cursp->s_cursrow]->v_attrlen > 0)
		vupd_dotline(si.curwp);

	// Update mode line if (1), mode flag set; or (2), "Col" buffer mode set (point movement assumed); or (3), "Line"
	// buffer mode set and any window flag set.
	if((si.curwp->w_flags & WFMode) || modeset(MdIdxCol,NULL) || (si.curwp->w_flags && modeset(MdIdxLine,NULL)))
		vupd_modeline(si.curwp,NULL);
	si.curwp->w_flags = 0;

	// If horizontal scrolling mode not enabled and line is shifted, put a '$' in column 0.
	if(!modeset(MdIdxHScrl,NULL) && si.cursp->s_firstcol > 0)
		vscreen[si.cursp->s_cursrow]->v_text[0] = LineExt;
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
	if(si.cursp->s_flags) {		// EScrResize set?
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
							wswitch(si.wheadp);
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
				if((winp->w_flags & ~WFMode) == WFEdit)
					vupd_dotline(winp);		// Update current (edited) line only.
				else if(winp->w_flags & WFHard)
					vupd_wind(winp);		// Update all lines.
				if(winp->w_flags & WFMode)
					vupd_modeline(winp,NULL);	// Update mode line.
				winp->w_flags = winp->w_rfrow = 0;
				}
			}

		// On to the next window.
		} while((winp = winp->w_nextp) != NULL);

	// Update lines in current window if needed and recalculate the hardware cursor location.
	vupd_cursor();

	// Check for lines to de-extend.
	if(!modeset(MdIdxHScrl,NULL))
		supd_dex();

	// If updating physical screen...
	if(n == INT_MIN || n > 0) {

		// If physical screen is garbage, clear it and force redraw.
		if(!(si.opflags & OpScrRedraw) || pupd_redraw() == Success) {

			// Update the physical screen from the virtual screen.
			if(pupd_all(force) == Success) {

				// Update the cursor position and flush the buffers.
				if(movecursor(si.cursp->s_cursrow,si.cursp->s_curscol) == Success)
					(void) TTflush();
				}
			}
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

// Find key(s) bound to given command, convert to string form (if not prefixed), and append to message line underscored.  Return
// status.  If c is non-zero, process "line" command.  Used by bpop() for help prompt.
static int hkey(int (*cfunc)(Datum *rp,int n,Datum **argpp)) {
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
				if(mlprintf(MLTermAttr,"%c~u%s~U",c,ektos(kbp->k_code,wkbuf,false)) != Success)
					return rc.status;
				c = '|';
				found = true;
				}
			}
		} while((kbp = nextbind(&kw)) != NULL);

	// Return now if no keys found.
	if(!found)
		return rc.status;

	// One or more keys found.  Finish up command.
	return mlprintf(MLTermAttr," ~b%c%s~0,",cmd0,cmd);
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
	VideoLine *vlp;
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
						vtmove(row,0);
						vteeol();
						vscreen[row++]->v_flags = VFChgd;
						}
					break;
					}

				// Update the virtual screen image for this line.
				vlp = vscreen[row];
				vlp->v_flags = VFChgd;
				taboff = firstcol;
				vtmove(row,-taboff);
				if(firstcol == 0 && (bufp->b_flags & BFTermAttr))
					checkAttr(lnp,row);
				vtputln(lnp);
				vteeol();
				taboff = 0;

				// If line is shifted, put a '$' in column 0.
				if(firstcol > 0)
					vlp->v_text[0] = LineExt;

				// On to the next line.
				lnp = lnp->l_nextp;
				} while(++row < disprows);

			// Screen is full.  Copy the virtual screen to the physical screen.
			if(pupd_all(false) != Success)
				return rc.status;

			// Bail out if called from terminp() and one-page buffer.
			if(firstpass && !(flags & RendWait) && lpmax == NULL)
				goto Retn;
			firstpass = false;
			}

		// Display prompt.
		if(mlputs(MLHome | MLFlush,hprompt != NULL ? hprompt : lpmax == NULL || lnp1 == lpmax ? text201 :
		 ": ") != Success || TTflush() != Success)
													// "End: "
			return rc.status;

		// Get response.
		for(;;) {
			// Get a keystroke and decode it.
			if(getkey(&ek,false) != Success)
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
				// Build prompt and display it:
			// SPC f C-v +pg, b C-z -pg, d +half, u -half, C-n +ln, C-p -ln, g top, G bot, ESC q quit, ? help:
				if(mlprintf(MLHome | MLTermAttr,"~uSPC~U|~uf~U") != Success ||
				 hkey(forwPage) != Success || mlprintf(MLTermAttr," ~ub~U") != Success ||
				 hkey(backPage) != Success || mlprintf(MLTermAttr,
				 " ~ud~U ~b+half~0, ~uu~U ~b-half~0,") != Success || hkey(forwLine) != Success ||
				 hkey(backLine) != Success || mlprintf(MLFlush | MLTermAttr,
				 " ~ug~U ~btop~0, ~uG~U ~bbot~0, ~uESC~U|~uq~U ~bquit~0, ~u?~U ~bhelp~0: ") != Success)
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
