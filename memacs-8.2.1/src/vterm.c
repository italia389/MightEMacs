// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// vterm.c	Terminal display and window management routines for MightEMacs.
//
// There are two halves: (1), functions that update the virtual display screen; and (2), functions that make the physical
// display screen the same as the virtual display screen.  These functions use hints that are left in the windows by the
// commands.

#include "os.h"
#include <stdarg.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"
#include "ebind.h"

/*** Local declarations ***/

#define FARRIGHT 9999			// Column beyond the right edge.

static int curcol;			// Cursor column.
static int currow;			// Cursor row.
static int oldcol;			// Old cursor column.
static int oldrow;			// Old cursor row.
static Video **pscreen;			// Physical screen.
static int taboff = 0;			// Tab offset for display.
static Video **vscreen;			// Virtual screen.
static int vtcol = 0;			// Column location of SW cursor.
static int vtrow = 0;			// Row location of SW cursor.

// Initialize the data structures used by the display code.  The edge vectors used to access the screens are set up.  The
// operating system's terminal I/O channel is set up.  All the other things get initialized at compile time.  The original
// window has "WFCHGD" set, so that it will get completely redrawn on the first call to "update".  Return status.
int vtinit(void) {
	int i;
	Video *vvp,*pvp;
	static char myname[] = "vtinit";

	if(TTopen() != SUCCESS ||	// Open the screen.
	 TTkopen() != SUCCESS ||	// Open the keyboard.
	 TTrev(false) != SUCCESS)
		return rc.status;

	// Allocate the virtual screen and physical shadow screen arrays (of pointers).
	if((vscreen = (Video **) malloc(term.t_mrow * sizeof(Video *))) == NULL ||
	 (pscreen = (Video **) malloc(term.t_mrow * sizeof(Video *))) == NULL)
		return rcset(PANIC,0,text94,myname);
			// "%s(): Out of memory!"

	// For every line in the display terminal.
	i = 0;
	do {
		// Allocate a virtual screen line and physical shadow screen line.
		if((vvp = (Video *) malloc(sizeof(Video) + term.t_mcol)) == NULL ||
		 (pvp = (Video *) malloc(sizeof(Video) + term.t_mcol)) == NULL)
			return rcset(PANIC,0,text94,myname);
				// "%s(): Out of memory!"

		// Initialize virtual line and save in line array.
		vvp->v_flags = 0;		// Init change flags.
		vvp->v_left = FARRIGHT;		// Init requested rev video boundaries.
		vvp->v_right = 0;
#if COLOR
		vvp->v_rfcolor = 7;		// Init fore/background colors.
		vvp->v_rbcolor = 0;
#endif
		vscreen[i] = vvp;

		// Initialize physical line and save in line array.
		pvp->v_flags = VFNEW;
		pvp->v_left = FARRIGHT;
		pvp->v_right = 0;
		pscreen[i] = pvp;
		} while(++i < term.t_mrow);

	return rc.status;
	}

// Clean up the virtual terminal system, in anticipation for a return to the operating system.  Move down to the last line and
// clear it out (the next system prompt will be written in the line).  Shut down the channel to the terminal.  Return status.
int vttidy(bool force) {

	// Don't close it if it ain't open.
	if(opflags & OPVTOPEN) {
		mlerase(MLFORCE);
		if((TTflush() == SUCCESS || force) && (TTclose() == SUCCESS || force))
			(void) TTkclose();
		opflags &= ~OPVTOPEN;
		}
	return rc.status;
	}

// Move up or down n lines (if possible) from given window line and set top line of window to result.  If n is negative, move
// up; if n is positive, move down.  Return true if top line was changed; otherwise, false.
bool wupd_newtop(EWindow *winp,Line *lnp,int n) {
	Line *lnp0 = lnp;

	if(n < 0) {
		while(lback(lnp) != winp->w_bufp->b_hdrlnp) {
			lnp = lback(lnp);
			if(++n == 0)
				break;
			}
		}
	else if(n > 0) {
		while(lforw(lnp) != winp->w_bufp->b_hdrlnp) {
			lnp = lforw(lnp);
			if(--n == 0)
				break;
			}
		}
	return (winp->w_face.wf_toplnp = lnp) != lnp0;
	}

// Set the virtual cursor to the specified row and column on the virtual screen.  There is no checking for nonsense values;
// this might be a good idea during the early stages.
static void vtmove(int row,int col) {

	vtrow = row;
	vtcol = col;
	}

// Write a character to the virtual screen.  The virtual row and column are updated.  Return number of characters written.  If
// we are not yet at the left edge, don't write it.  If the line is too long put a "$" in the last column.  This routine only
// (1), puts printing characters into the virtual terminal buffers; and (2), checks for column overflow.
static int vtputc(int c) {
	Video *vp;		// Pointer to line being updated.
	int vtcol0 = vtcol;

	// Defeat automatic sign extension.
	c &= 0xff;

	// Line receiving this character.
	vp = vscreen[vtrow];

	if(vtcol >= (int) term.t_ncol) {

		// We are at the right edge!
		++vtcol;
		vp->v_text[term.t_ncol - 1] = '$';
		}
	else if(c == '\t') {
		// Output a hardware tab as the right number of spaces.
		do {
			vtputc(' ');
			} while(((vtcol + taboff) % (htabsize)) != 0);
		}
	else if(c < 0x20 || c == 0x7F) {

		// Control character.
		vtputc('^');
		vtputc(c ^ 0x40);
		}
	else if(c > 0x7f && (modetab[MDR_GLOBAL].flags & MDESC8)) {
		char wkbuf[5],*strp = wkbuf;

		// Display character with high bit set symbolically.
		sprintf(wkbuf,"<%.2X>",c);
		do {
			vtputc(*strp++);
			} while(*strp != '\0');
		}
	else {
		// It's normal, just put it in the screen map.
		if(vtcol >= 0)
			vp->v_text[vtcol] = c;
		++vtcol;
		}
	return vtcol - vtcol0;
	}

// Write a string to the virtual screen.  Return number of characters written.
static int vtputs(char *strp) {
	int c;
	int count = 0;

	while((c = *strp++) != '\0')
		count += vtputc(c);
	return count;
	}

// Erase from the end of the software cursor to the end of the line in which the software cursor is located.
static void vteeol(void) {
	Video *vp;

	vp = vscreen[vtrow];
	while(vtcol < (int) term.t_ncol) {
		if(vtcol >= 0)
			vp->v_text[vtcol] = ' ';
		++vtcol;
		}
	}

// Send a command to the terminal to move the hardware cursor to row "row" and column "col".  The row and column arguments are
// origin 0.  Return status.
int movecursor(int row,int col) {

	(void) TTmove(row,col);
	if(row == term.t_nrow - 1)
		ml.ttcol = col;
	return rc.status;
	}

// Check if the line containing dot is in the given window and re-frame it if needed or wanted.  Return status.
static int wupd_reframe(EWindow *winp) {
	Line *lnp;		// Search pointer.
	int i;			// General index/# lines to scroll.
	int nlines;		// Number of lines in current window.

	// Get current window size.
	nlines = winp->w_nrows;

	// If not a forced reframe, check for a needed one.
	if(!(winp->w_flags & WFFORCE)) {

		// If the top line of the window is at EOB...
		if((lnp = winp->w_face.wf_toplnp) == winp->w_bufp->b_hdrlnp) {
			if(lforw(winp->w_bufp->b_hdrlnp) == winp->w_bufp->b_hdrlnp)
				return rc.status;	// Buffer is empty, no reframe needed.
			else {
				winp->w_face.wf_toplnp = lforw(winp->w_bufp->b_hdrlnp);
				goto reframe;		// Buffer is not empty, reframe.
				}
			}

		// Check if dot is in the window.
		if(inwind(winp,winp->w_face.wf_dot.lnp))
			return rc.status;		// Found dot, no reframe needed.
		}
reframe:
	// Reaching here, we need a window refresh.  Either the top line of the window is at EOB and the buffer is not empty (a
	// macro buffer), the line containing dot is not in the window, or it's a force.  Set i to the new window line number
	// (0 .. nlines - 1) where the dot line should "land".  If it's a force, use w_force for i and just range-check it;
	// otherwise, calcluate a reasonable value (either vjump/100 from top, center of window, or vjump/100 from bottom)
	// depending on direction dot has moved.
	i = winp->w_force;

	// If not a force...
	if(!(winp->w_flags & WFFORCE)) {
		Line *forwp,*backp,*hdrp;
		Line *dotp = winp->w_face.wf_dot.lnp;

		// Search thru the buffer looking for the point.  If found, set i to top or bottom line if smooth scrolling,
		// or vjump/100 from top or bottom if jump scrolling.
		forwp = backp = winp->w_face.wf_toplnp;
		hdrp = winp->w_bufp->b_hdrlnp;
		i = 0;
		for(;;) {
			// Did dot move downward (below the window)?
			if(forwp == dotp) {

				// Yes.  Force i to center of window if dot moved more than one line past the bottom.
				i = (i > nlines) ? nlines / 2 : (vjump == 0) ? nlines - 1 : nlines * (100 - vjump) / 100;
				break;
				}

			// Did dot move upward (above the window)?
			if(backp == dotp) {

				// Yes.  Force i to center of window if dot moved more than one line past the top or vjump is
				// zero and dot is at end of buffer.
				i = (i > 1) ? nlines / 2 : (vjump > 0) ? nlines * vjump / 100 : (dotp == hdrp) ? nlines / 2 : 0;
				break;
				}

			// Advance forward and back.
			if(forwp != hdrp)
				forwp = lforw(forwp);
			else if(backp == hdrp)
				break;
			if(backp != hdrp)
				backp = lback(backp);
			else if(forwp == hdrp)
				break;
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

	// Now back up from dot line, set new top line, and set update flags.
	if(wupd_newtop(winp,winp->w_face.wf_dot.lnp,-i))
		winp->w_flags |= WFHARD;
	winp->w_flags &= ~WFFORCE;

	return rc.status;
	}

// Update a single line on the physical screen, given row, virtual screen image, and physical screen image.  This does not know
// how to use insert or delete character sequences; we are using VT52 functionality.  Update the physical row and column
// variables.  It does try to exploit erase to end of line.  Return status.
static int pupd_line(int row,Video *vp,Video *pp) {
	char *vir_left;			// Left pointer to virtual line.
	char *phy_left;			// Left pointer to physical line.
	char *vir_right;		// Right pointer to virtual line.
	char *phy_right;		// Right pointer to physical line.
	int rev_left;			// Leftmost reversed char index.
	int rev_right;			// Rightmost reversed char index.
	char *left_blank;		// Left-most trailing blank.
	int non_blanks;			// Non-blanks to the right flag.
	int update_column;		// Update column.
	int old_rev_state = false;	// Reverse video states.
	int new_rev_state;

	// Set up pointers to virtual and physical lines.
	vir_left = vp->v_text;
	vir_right = vp->v_text + term.t_ncol;
	phy_left = pp->v_text;
	phy_right = pp->v_text + term.t_ncol;
	update_column = 0;
	rev_left = FARRIGHT;
	rev_right = 0;
	non_blanks = true;

	// If this is a legitimate line to optimize.
	if(!(pp->v_flags & VFNEW)) {

		// Advance past any common chars at the left.
		while(vir_left != vp->v_text + term.t_ncol && vir_left[0] == phy_left[0]) {
			++vir_left;
			++update_column;
			++phy_left;
			}

		// Advance past any common chars at the right.
		non_blanks = false;
		while(vir_right[-1] == phy_right[-1] && vir_right >= vir_left) {
			--vir_right;
			--phy_right;

			// Note if any nonblank in right match.
			if(vir_right[0] != ' ')
				non_blanks = true;
			}
		}

#if COLOR
	// New line color?
	if(vp->v_rfcolor != vp->v_fcolor || vp->v_rbcolor != vp->v_bcolor) {
		vp->v_fcolor = vp->v_rfcolor;
		vp->v_bcolor = vp->v_rbcolor;
		vp->v_flags &= ~VFCOLOR;
		vir_left = vp->v_text;
		vir_right = vp->v_text + term.t_ncol;
		phy_left = pp->v_text;
		phy_right = pp->v_text + term.t_ncol;
		update_column = 0;
		}

	if(TTforg(vp->v_fcolor) != SUCCESS || TTbacg(vp->v_bcolor) != SUCCESS)
		return rc.status;
#endif

	// Reverse video changes?
	if(vp->v_left != pp->v_left || vp->v_right != pp->v_right) {

		// Adjust leftmost edge.
		if(vp->v_left < pp->v_left)
			rev_left = vp->v_left;
		else
			rev_left = pp->v_left;

		pp->v_left = vp->v_left;
		if(rev_left < update_column) {
			vir_left = vp->v_text + rev_left;
			phy_left = pp->v_text + rev_left;
			update_column = rev_left;
			}

		// Adjust rightmost edge.
		if(vp->v_right > pp->v_right)
			rev_right = vp->v_right;
		else
			rev_right = pp->v_right;

		pp->v_right = vp->v_right;
		if(vp->v_text + rev_right > vir_right) {
			vir_right = vp->v_text + rev_right;
			phy_right = pp->v_text + rev_right;
			}
		}
	else {
		rev_left = vp->v_left;
		rev_right = vp->v_right;
		}

	// If both lines are the same, no update needs to be done.
	if(!(pp->v_flags & VFNEW) && vir_left > vir_right) {
		vp->v_flags &= ~VFCHGD;			// Clear changed flag.
		return rc.status;
		}

	left_blank = vir_right;

	// Erase to EOL?
	if(non_blanks == false && (opflags & OPHAVEEOL)) {
		while(left_blank != vir_left && left_blank[-1] == ' ')
			--left_blank;
		if(vir_right - left_blank <= 3)		// Use only if erase is > 3 characters.
			left_blank = vir_right;
		}

	// Move to the beginning of the text to update.
	if(movecursor(row,update_column) != SUCCESS)
		return rc.status;

	while(vir_left != left_blank) {		// Ordinary.

		// Are we in a reverse video field?
		if(pp->v_left <= update_column && update_column < pp->v_right)
			new_rev_state = true;
		else
			new_rev_state = false;

		// If moving in or out of reverse video, change it.
		if(new_rev_state != old_rev_state) {
			if(TTrev(new_rev_state) != SUCCESS)
				return rc.status;
			old_rev_state = new_rev_state;
			}

		// Output the next character.
		if(TTputc(*vir_left) != SUCCESS)
			return rc.status;
		++update_column;
		*phy_left++ = *vir_left++;
		}

	if(left_blank != vir_right) {		// Erase.

		// Are we in a reverse video field?
		if(pp->v_left <= update_column && update_column < pp->v_right)
			new_rev_state = true;
		else
			new_rev_state = false;

		// If moving in or out of reverse video, change it.
		if(new_rev_state != old_rev_state) {
			if(TTrev(new_rev_state) != SUCCESS)
				return rc.status;
			old_rev_state = new_rev_state;
			}

#if TT_TERMCAP
		// TERMCAP does not tell us if the current terminal propagates the current attributes to the end of the line
		// when an erase to end of line sequence is sent.  Don't let TERMCAP use EEOL if in a reverse video line!
		if(new_rev_state == true)
			while(update_column++ < (int) term.t_ncol) {
				if(TTputc(' ') != SUCCESS)
					return rc.status;
				}
		else if(TTeeol() != SUCCESS)
			return rc.status;
#else
		if(TTeeol() != SUCCESS)
			return rc.status;
#endif
		while(vir_left != vir_right)
			*phy_left++ = *vir_left++;
		}

	vp->v_flags &= ~VFCHGD;			// Flag this line as updated.
#if COLOR
	vp->v_flags &= ~VFCOLOR;
#endif
	// Always leave in the default state.
	if(old_rev_state == true)
		(void) TTrev(false);

	return rc.status;
	}

// Transfer the virtual screen to the physical screen.  Force it if "force" is true.  Return status.
static int pupd_all(bool force) {
	Video *vp;
	int i,count;

	for(i = 0; i < (int) term.t_nrow - 1; ++i) {		// Exclude message line.
		vp = vscreen[i];

		// For each line that needs to be updated.
		if(vp->v_flags & VFCHGD) {
#if TYPEAH
			if(!force) {
				if(typahead(&count) != SUCCESS)
					return rc.status;
				if(count > 0)
					return rc.status;
				}
#endif
			if(pupd_line(i,vp,pscreen[i]) != SUCCESS)
				return rc.status;
			}
		}
	return rc.status;
	}

// Transfer all lines in given window to virtual screen.
static void vupd_all(EWindow *winp) {
	Line *lnp;		// Line to update.
	int sline;		// Physical screen line to update.
	int i;
	int nlines;		// Number of lines in the current window.

	// Search down the lines, updating them.
	lnp = winp->w_face.wf_toplnp;
	sline = winp->w_toprow;
	nlines = winp->w_nrows;
	taboff = winp->w_face.wf_fcol;
	while(sline < winp->w_toprow + nlines) {

		// Update the virtual line.
		vscreen[sline]->v_flags |= VFCHGD;
		vscreen[sline]->v_left = FARRIGHT;
		vscreen[sline]->v_right = 0;
		vtmove(sline,-taboff);

		// If we are not at the end...
		if(lnp != winp->w_bufp->b_hdrlnp) {
			for(i = 0; i < lused(lnp); ++i)
				vtputc(lgetc(lnp,i));
			lnp = lforw(lnp);
			}

		// Make sure we are on screen.
		if(vtcol < 0)
			vtcol = 0;

		// On to the next line.
#if COLOR
		vscreen[sline]->v_rfcolor = winp->w_face.wf_fcolor;
		vscreen[sline]->v_rbcolor = winp->w_bcolor;
#endif
		vteeol();
		++sline;
		}
	taboff = 0;
	}

// De-extend any line in any window that needs it.
static void supd_dex(void) {
	EWindow *winp;
	Line *lnp;
	int i,j;
	int nlines;		// Number of lines in the current window.

	winp = wheadp;
	do {
		lnp = winp->w_face.wf_toplnp;
		i = winp->w_toprow;
		nlines = winp->w_nrows;

		while(i < winp->w_toprow + nlines) {
			if(vscreen[i]->v_flags & VFEXT) {
				if(winp != curwp || lnp != winp->w_face.wf_dot.lnp || curcol < (int) term.t_ncol - 1) {
					if(lnp == winp->w_bufp->b_hdrlnp)
						vtmove(i,0);
					else {
						taboff = winp->w_face.wf_fcol;
						vtmove(i,-taboff);
						for(j = 0; j < lused(lnp); ++j)
							vtputc(lgetc(lnp,j));
						taboff = 0;
						}
					vteeol();

					// This line is no longer extended.
					vscreen[i]->v_flags &= ~VFEXT;
					vscreen[i]->v_flags |= VFCHGD;
					}
				}
			if(lnp != winp->w_bufp->b_hdrlnp)
				lnp = lforw(lnp);
			++i;
			}

		// Onward to the next window.
		} while((winp = winp->w_nextp) != NULL);
	}

// The screen is garbage and needs major repair.  Clear the physical and virtual screens and force a full update.
// Return status.
static int supd_redraw(void) {
	int i;
	char *strp,*strpz;

	for(i = 0; i < (int) term.t_nrow - 1; ++i) {		// Exclude message line.
		vscreen[i]->v_flags |= VFCHGD;
#if COLOR
		vscreen[i]->v_fcolor = gfcolor;
		vscreen[i]->v_bcolor = gbcolor;
#endif
		pscreen[i]->v_left = FARRIGHT;
		pscreen[i]->v_right = 0;
		strpz = (strp = pscreen[i]->v_text) + term.t_ncol;
		do {
			*strp++ = ' ';
			} while(strp < strpz);
		pscreen[i]->v_flags &= ~VFNEW;
		}

	 // Erase the screen.
	if(movecursor(0,0) != SUCCESS || TTeeop() != SUCCESS)
		return rc.status;
	opflags &= ~OPSCREDRAW;					 // Clear redraw flag.
#if COLOR
	mlerase(MLFORCE);
#endif
	return rc.status;
	}

// Update the current line to the virtual screen in given window.
static void vupd_dotline(EWindow *winp) {
	Line *lnp;		// Line to update.
	int sline;		// Physical screen line to update.
	int i;

	// Search down to the line we want...
	lnp = winp->w_face.wf_toplnp;
	sline = winp->w_toprow;
	while(lnp != winp->w_face.wf_dot.lnp) {
		++sline;
		lnp = lforw(lnp);
		}

	// and update the virtual line.
	vscreen[sline]->v_flags |= VFCHGD;
	taboff = winp->w_face.wf_fcol;
	vtmove(sline,-taboff);

	// Move each char of line to virtual screen until at end.
	for(i = 0; i < lused(lnp); ++i)
		vtputc(lgetc(lnp,i));
#if COLOR
	vscreen[sline]->v_rfcolor = winp->w_face.wf_fcolor;
	vscreen[sline]->v_rbcolor = winp->w_bcolor;
#endif
	vteeol();
	taboff = 0;
	}

// Update the extended line which the cursor is currently on at a column greater than the terminal width.  The line will be
// scrolled right or left to let the user see where the cursor is.
static void vupd_ext(void) {
	int rcursor;		// Real cursor location.
	Line *lnp;		// Pointer to current line.
	int j;			// Index into line.

	// Calculate what column the real cursor will end up in.
	rcursor = ((curcol - term.t_ncol) % term.t_scrsiz) + term.t_margin;	// 10% width <= rcursor < 90%
	lbound = curcol - rcursor + 1;
	taboff = lbound + curwp->w_face.wf_fcol;

	// Write the current line to the virtual screen.
	vtmove(currow,-taboff);			// Start scanning offscreen.
	lnp = curwp->w_face.wf_dot.lnp;		// Line to output.
	for(j = 0; j < lused(lnp); ++j)		// Until the end-of-line.
		vtputc(lgetc(lnp,j));

	// Truncate the virtual line, restore tab offset.
	vteeol();
	taboff = 0;

	// and put a '$' in column 1.
	vscreen[currow]->v_text[0] = '$';
	}

// Write "== " to mode line and return count.
static int wupd_tab(int lchar) {

	vtputc(lchar);
	vtputc(lchar);
	vtputc(' ');
	return 3;
	}

// Redisplay the mode line for the window pointed to by winp.  If popbuf is NULL, display a fully-formatted mode line containing
// the current buffer's name and filename (but in condensed form if the current terminal width is less than 96 columns);
// otherwise, display only the buffer name and filename of buffer "popbuf".  This is the only routine that has any idea of how
// the mode line is formatted.  You can change the mode line format by hacking at this routine.  Called by "update" any time
// there is a dirty window.
void wupd_modeline(EWindow *winp,Buffer *popbuf) {
	int c;
	int n;				// Cursor position.
	Buffer *bufp;
	ModeSpec *msp;
	int lchar;
	int condensed = term.t_ncol < 80 ? -1 : term.t_ncol < 96 ? 1 : 0;
	char wkbuf[32];			// Work buffer.
	static struct {			// Mode display parameters.
		int leadch,trailch;
		uint flags;
		} *mdp,md[] = {
			{'(',')',0},
			{'[',']',0},
			{-1,-1,0}};

	n = winp->w_toprow + winp->w_nrows;		// Row location.

	// Note that we assume that setting REVERSE will cause the terminal driver to draw with the inverted relationship of
	// fcolor and bcolor, so that when we say to set the foreground color to "white" and background color to "black", the
	// fact that "reverse" is enabled means that the terminal driver actually draws "black" on a background of "white".
	// Makes sense, no?  This way, devices for which the color controls are optional will still get the "reverse" signals.
	vscreen[n]->v_left = 0;
	vscreen[n]->v_right = term.t_ncol;
#if COLOR
	vscreen[n]->v_flags |= VFCHGD | VFCOLOR;	// Redraw next time.
	vscreen[n]->v_rfcolor = 7;			// Black on.
	vscreen[n]->v_rbcolor = 0;			// White...
#else
	vscreen[n]->v_flags |= VFCHGD;			// Redraw next time.
#endif
	vtmove(n,0);					// Seek to right line.
	if(winp == curwp)				// Make the current buffer stand out.
		lchar = '=';
#if REVSTA
	else if(opflags & OPHAVEREV)
		lchar = ' ';
#endif
	else
		lchar = '-';

	// Full display?
	if(popbuf == NULL) {
		bufp = winp->w_bufp;
		vtputc((bufp->b_flags & BFTRUNC) ? '#' : lchar);	// "#" if truncated.
		vtputc((bufp->b_flags & BFCHGD) ? '*' : lchar);		// "*" if changed.
		vtputc((bufp->b_flags & BFNARROW) ? '<' : lchar);	// "<" if narrowed.
		vtputc(' ');
		n = 4;

		// Display program name and version.
		if(!condensed) {
			sprintf(wkbuf,"%s %s ",myself,version);
			n += vtputs(wkbuf);
			}

		// Are we horizontally scrolled?
		if(winp->w_face.wf_fcol > 0) {
			sprintf(wkbuf,"[<%d] ",winp->w_face.wf_fcol);
			n += vtputs(wkbuf);
			}

		// Display the screen number if bottom window and there's more than one screen.
		if(winp->w_nextp == NULL && scrcount() > 1) {
			sprintf(wkbuf,"S%hu ",cursp->s_num);
			n += vtputs(wkbuf);
			}

		// Display keyboard macro recording state.
		if(kmacro.km_state == KMRECORD)
			n += vtputs("*R* ");

		// Display the line and/or column point position if enabled and winp is current window.
		if(winp == curwp) {
			if(curbp->b_modes & MDLINE) {
				sprintf(wkbuf,"L:%ld ",getlinenum(bufp,winp->w_face.wf_dot.lnp));
				n += vtputs(wkbuf);
				}
			if(curbp->b_modes & MDCOL) {
				sprintf(wkbuf,"C:%d ",getccol());
				n += vtputs(wkbuf);
				}
			}

		// Display the modes, in short form if condensed display.
		md[0].flags = modetab[MDR_GLOBAL].flags & modetab[MDR_SHOW].flags;
		md[1].flags = winp->w_bufp->b_modes;
		mdp = md;
		do {
			msp = ((c = mdp->leadch) == '[') ? bmodeinfo : gmodeinfo;
			do {
				if(mdp->flags & msp->mask) {
					n += vtputc(c);
					c = ' ';
					if(condensed < 0)
						n += vtputc(msp->code);
					else
						n += vtputs(msp->mlname);
					}
				} while((++msp)->name != NULL);
			if(c != mdp->leadch) {
				n += vtputc(mdp->trailch);
				n += vtputc(' ');
				}
			} while((++mdp)->leadch > 0);
#if 0
		// Display internal modes on modeline.
		vtputc(lchar);
		vtputc((winp->w_flags & WFCOLOR) != 0 ? 'C' : lchar);
		vtputc((winp->w_flags & WFMODE) != 0 ? 'M' : lchar);
		vtputc((winp->w_flags & WFHARD) != 0 ? 'H' : lchar);
		vtputc((winp->w_flags & WFEDIT) != 0 ? 'E' : lchar);
		vtputc((winp->w_flags & WFMOVE) != 0 ? 'V' : lchar);
		vtputc((winp->w_flags & WFFORCE) != 0 ? 'F' : lchar);
		vtputc(lchar);
		n += 8;
#endif
		n += wupd_tab(lchar);
		}
	else {
		n = 0;
		bufp = popbuf;
		vtputc(lchar);
		n += wupd_tab(lchar) + 1;
		}

	// Display the buffer name.
	n += vtputs(bufp->b_bname) + 1;
	vtputc(' ');

	// Display the filename in the remaining space using strfit().
	if(bufp->b_fname != NULL) {
		char wkbuf[TT_MAXCOLS];

		n += wupd_tab(lchar);
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

	// If it's the current window, not a pop-up, "pwd" mode, and room still available, display the working directory as
	// well.
	if(winp == curwp && popbuf == NULL && (modetab[MDR_GLOBAL].flags & MDWKDIR) && ((int) term.t_ncol - n) > 12) {
		char *wdp;
		char wkbuf[TT_MAXCOLS];

		n += wupd_tab(lchar);
		n += vtputs(text274);
			// "WD: "
		(void) getwkdir(&wdp,false);
		n += vtputs(strfit(wkbuf,term.t_ncol - n - 1,wdp,0)) + 1;
		vtputc(' ');
		}

	// Pad to full width.
	while(n < (int) term.t_ncol) {
		vtputc(lchar);
		++n;
		}
	}

// Update the position of the hardware cursor in the current window and handle extended lines.  This is the only update for
// simple moves.
static void wupd_cursor(void) {
	Line *lnp;
	int i;
	WindFace *wfp = &curwp->w_face;

	// Find the current row.
	lnp = wfp->wf_toplnp;
	currow = curwp->w_toprow;
	while(lnp != wfp->wf_dot.lnp) {
		++currow;
		lnp = lforw(lnp);
		}

	// Find the current column.
	curcol = i = 0;
	while(i < wfp->wf_dot.off)
		curcol = newcol(lgetc(lnp,i++),curcol);

	// Adjust by the current first column position.
	curcol -= wfp->wf_fcol;

	// Make sure it is not off the left side of the screen.
	while(curcol < 0) {
		if(wfp->wf_fcol >= hjumpcols) {
			curcol += hjumpcols;
			wfp->wf_fcol -= hjumpcols;
			}
		else {
			curcol += wfp->wf_fcol;
			wfp->wf_fcol = 0;
			}
		curwp->w_flags |= WFHARD | WFMODE;
		}

	// If horizontall scrolling is enabled, shift if needed.
	if(modetab[MDR_GLOBAL].flags & MDHSCRL) {
		while(curcol >= (int) term.t_ncol - 1) {
			curcol -= hjumpcols;
			wfp->wf_fcol += hjumpcols;
			curwp->w_flags |= WFHARD | WFMODE;
			}
		}
	else {
		// If extended, flag so and update the virtual line image.
		if(curcol >= (int) term.t_ncol - 1) {
			vscreen[currow]->v_flags |= (VFEXT | VFCHGD);
			vupd_ext();
			}
		else
			lbound = 0;
		}

	// Update the virtual screen if we had to move the window around.
	if(curwp->w_flags & WFHARD)
		vupd_all(curwp);
	if(curwp->w_flags & WFMODE)
		wupd_modeline(curwp,NULL);
	curwp->w_flags = 0;
	}

// Make sure that the display is right.  This is a four-part process.  First, resize the windows in the current screen to match
// the current terminal dimensions if needed.  Second, scan through all of the screen windows looking for dirty (flagged) ones.
// Check the framing and refresh the lines.  Third, make sure that "currow" and "curcol" are correct for the current window.
// And fourth, make the virtual and physical screens the same.  Return status.
int update(bool force) {
	EWindow *winp;
	int count;

#if TYPEAH || !VISMAC
	// If we are not forcing the update...
	if(!force) {
#if TYPEAH
		// If there are keystrokes waiting to be processed, skip the update.
		if(typahead(&count) != SUCCESS || count > 0)
			return rc.status;
#endif
#if !VISMAC
		// If we are replaying a keyboard macro, don't bother keeping updated.
		if(kmacro.km_state == KMPLAY)
			return rc.status;
#endif
		}
#endif
	// Current screen dimensions wrong?
	if(cursp->s_flags) {		// ESRESIZE set?
		EWindow *lastwp,*nextwp;
		int nrow;

		// Loop until vertical size of all windows matches terminal rows.
		do {
			// Does current screen need to grow vertically?
			if(term.t_nrow > cursp->s_nrow) {

				// Yes, go to the last window...
				winp = wnextis(NULL);

				// and enlarge it as needed.
				winp->w_nrows = (cursp->s_nrow = term.t_nrow) - winp->w_toprow - 2;
				winp->w_flags |= WFHARD | WFMODE;
				}

			// Does current screen need to shrink vertically?
			else if(term.t_nrow < cursp->s_nrow) {

				// Rebuild the window structure.
				nextwp = cursp->s_wheadp;
				lastwp = NULL;
				nrow = 0;
				do {
					winp = nextwp;
					nextwp = winp->w_nextp;

					// Get rid of window if it is too low.
					if(winp->w_toprow >= term.t_nrow - 2) {

						// Save the "face" parameters.
						--winp->w_bufp->b_nwind;
						wftobf(winp,winp->w_bufp);

						// Update curwp and lastwp if needed.
						if(winp == curwp)
							wswitch(wheadp);
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
							winp->w_flags |= WFHARD | WFMODE;
							}
						nrow += winp->w_nrows + 1;
						}

					lastwp = winp;
					} while(nextwp != NULL);
				cursp->s_nrow = nrow;
				}
			} while(cursp->s_nrow != term.t_nrow);

		// Update screen controls and mark screen for a redraw.
		cursp->s_ncol = term.t_ncol;
		cursp->s_flags = 0;
		opflags |= OPSCREDRAW;
		}

	// Check all windows and update any that need refreshing.
	winp = wheadp;
	do {
		if(winp->w_flags) {

			// The window has changed in some way: service it.
			if(wupd_reframe(winp) != SUCCESS)		// Check the framing.
				return rc.status;
			if((winp->w_flags & ~WFMODE) == WFEDIT)
				vupd_dotline(winp);			// Update EDITed line only.
			else if(winp->w_flags & ~WFMOVE)
				vupd_all(winp);				// Update all lines.
			if(winp->w_flags & WFMODE)
				wupd_modeline(winp,NULL);		// Update modeline.
			winp->w_flags = winp->w_force = 0;
			}

		// On to the next window.
		} while((winp = winp->w_nextp) != NULL);

	// Recalc the current hardware cursor location.
	wupd_cursor();

	// Check for lines to de-extend.
	supd_dex();

	// If screen is garbage, redraw it.
	if(opflags & OPSCREDRAW) {
		if(modetab[MDR_GLOBAL].flags & MDNOUPD)
			opflags &= ~OPSCREDRAW;
		else if(supd_redraw() != SUCCESS)
			return rc.status;
		}

	// Update the virtual screen to the physical screen.
	if(pupd_all(force) != SUCCESS)
		return rc.status;

	// Update the cursor position and flush the buffers.
	if(movecursor(currow,curcol - lbound) != SUCCESS || TTflush() != SUCCESS)
		return rc.status;

#if MMDEBUG & DEBUG_SCRDUMP
	dumpscreens("Exiting update()");
#endif
	return rc.status;
	}

// Save current cursor position.
void savecursor(void) {

	oldrow = currow;
	oldcol = curcol;
	}

// Restore cursor position.  Return status.
int restorecursor(void) {

	if(movecursor(oldrow,oldcol - lbound) == SUCCESS)
		(void) TTflush();
	return rc.status;
	}

// Return true if given key is bound to given command.
static bool iscmd(uint ek,int (*cfunc)(Value *rp,int n)) {
	KeyDesc *kdp;

	return (kdp = getbind(ek)) != NULL && kdp->k_cfab.p_type == PTRCMD && kdp->k_cfab.u.p_cfp->cf_func == cfunc;
	}

// Find key(s) bound to given command, convert to string form (if not prefixed), and store in dest.  Return status.  If c is
// non-zero, process "line" command.  Used by bpop() for help prompt.
static int hkey(StrList *destp,int (*cfunc)(Value *rp,int n),int c) {
	KeyWalk kw = {NULL,NULL};
	KeyDesc *kdp;
	char *strp,wkbuf[16];
	bool found = false;

	// Initialize leading literal.
	strp = c ? " (" : ",";

	// Search binding table for any keys bound to command.
	kdp = nextbind(&kw);
	do {
		if(kdp->k_cfab.p_type == PTRCMD && kdp->k_cfab.u.p_cfp->cf_func == cfunc) {

			// Found one.  Prefixed?
			if((kdp->k_code & KEYSEQ) == 0) {

				// No.  Save it.
				if(vputs(strp,destp) != 0)
					return vrcset();
				strp = ",";
				if(vputs(ektos(kdp->k_code,wkbuf),destp) != 0)
					return vrcset();
				found = true;
				}
			}
		} while((kdp = nextbind(&kw)) != NULL);

	// Return now if no keys found or not a "line" command.
	if(!found || !c)
		return rc.status;

	// One or more keys found.  Finish up "line" command.
	return vputf(destp,") %c%s",c,text205) == 0 ? rc.status : vrcset();
				// "line"
	}

// Display a pop-up window and page it for the user.  If altmodeline is true, display buffer name and filename (only) on bottom
// mode line.  If endprompt is true, wait for user to press a key before returning (regardless of page size).  Current bindings
// (if any) for backPage, forwPage, backLine, and forwLine commands are recognized as well as 'b' (backward page), 'f' or space
// (forward page), 'u' (backward half page), 'd' (forward half page), 'g' (goto first page), 'G' (goto last page), ESC or 'q'
// (exit), and '?' (help).  Any non-navigation key gets pushed back into the input stream to be interpeted later as a command. 
// Return status.
int bpop(Buffer *bufp,bool altmodeline,bool endprompt) {
	Line *lnp1,*lnp,*lpmax;
	int crow;		// Current screen row number.
	int disprows;		// Total number of display rows.
	int halfpage;		// Rows in a half page.
	int n;			// Rows to move.
	int ek;			// Input extended key.
	char *strp,*strpz;	// Line text pointers.
	char *hprompt = NULL;	// Help prompt;
	bool firstpass = true;

	// Display special mode line if requested.
	if(altmodeline) {

		// Find last window on screen and rewrite its mode line.
		wupd_modeline(wnextis(NULL),bufp);
		}

	// Set up and display a pop-up "window".
	disprows = term.t_nrow - 2;

	// Check if buffer will fit on one page and if not, set lpmax to first line of last page.
	lpmax = NULL;
	n = 0;
	for(lnp = lforw(bufp->b_hdrlnp); lnp != bufp->b_hdrlnp; lnp = lforw(lnp)) {
		if(++n > disprows) {

			// Find beginning of last page.
			lpmax = bufp->b_hdrlnp;
			n = disprows;
			do {
				lpmax = lback(lpmax);
				} while(--n > 0);
			break;
			}
		}

	// Begin at the beginning.
	lnp1 = lforw(bufp->b_hdrlnp);
	halfpage = disprows / 2;
	n = 0;

	// Display a page (beginning at line lnp1 + n) and prompt for a naviagtion command.  Loop until exit key entered or
	// endprompt is false and buffer fits on one page (lpmax is NULL).
	for(;;) {
		lnp = lnp1;

		// Moving backward?
		if(n < 0) {
			do {
				// At beginning of buffer?
				if(lpmax == NULL || lnp1 == lforw(bufp->b_hdrlnp))
					break;

				// No, back up one line.
				lnp1 = lback(lnp1);
				} while(++n < 0);
			}

		// Moving forward?
		else if(n > 0) {
			do {
				// At end of buffer or max line?
				if(lpmax == NULL || lnp1 == bufp->b_hdrlnp || lnp1 == lpmax)
					break;

				// No, move forward one line.
				lnp1 = lforw(lnp1);
				} while(--n > 0);
			}

		// Illegal command?
		if(n != 0 && lnp1 == lnp)

			// Yes, ignore it.
			n = 0;
		else {
			// Found first row ... display page.
			lnp = lnp1;
			crow = 0;
			do {
				// At end of buffer?
				if(lnp == bufp->b_hdrlnp) {

					// Yes, erase remaining lines on physical screen.
					while(crow < disprows) {
						vtmove(crow,0);
						vteeol();
#if COLOR
						vscreen[crow]->v_rfcolor = gfcolor;
						vscreen[crow]->v_rbcolor = gbcolor;
#endif
						vscreen[crow]->v_left = FARRIGHT;
						vscreen[crow]->v_right = 0;
#if COLOR
						vscreen[crow++]->v_flags |= VFCHGD | VFCOLOR;
#else
						vscreen[crow++]->v_flags |= VFCHGD;
#endif
						}
					break;
					}

				// Update the virtual screen image for this line.  Characters past right edge of screen won't be
				// displayed, so ignore those.
				vtmove(crow,0);
				strpz = (strp = ltext(lnp)) + (lused(lnp) <= (int) term.t_ncol ? lused(lnp) : term.t_ncol);
				while(strp < strpz)
					vtputc(*strp++);
				vteeol();
#if COLOR
				vscreen[crow]->v_rfcolor = gfcolor;
				vscreen[crow]->v_rbcolor = gbcolor;
#endif
				vscreen[crow]->v_left = FARRIGHT;
				vscreen[crow]->v_right = 0;
#if COLOR
				vscreen[crow++]->v_flags |= VFCHGD | VFCOLOR;
#else
				vscreen[crow++]->v_flags |= VFCHGD;
#endif
				// On to the next line.
				lnp = lforw(lnp);
				} while(crow < disprows);

			// Screen is full.  Copy the virtual screen to the physical screen.
			if(pupd_all(false) != SUCCESS)
				return rc.status;

			// Bail out if called from terminp() and one-page buffer.
			if(firstpass && !endprompt && lpmax == NULL)
				goto uexit;
			firstpass = false;
			}

		// Display prompt.
		mlputs(MLHOME | MLFORCE,hprompt != NULL ? hprompt : lpmax == NULL || lnp1 == lpmax ? text201 : ": ");
													// "End: "
		if(TTflush() != SUCCESS)
			return rc.status;

		// Get response.
		for(;;) {
			// Get a keystroke and decode it.
			if(getkey(&ek) != SUCCESS)
				return rc.status;

			// Exit?
			if(ek == (CTRL | '[') || ek == 'q')
				goto uexit;

			// Forward whole page?
			if(ek == ' ' || ek == 'f' || iscmd(ek,forwPage)) {
				n = disprows - overlap;
				break;
				}

			// Forward half page?
			if(ek == 'd') {
				n = halfpage;
				break;
				}

			// Backward whole page?
			if(ek == 'b' || iscmd(ek,backPage)) {
				n = overlap - disprows;
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
				if(lpmax == NULL || lnp1 == lforw(bufp->b_hdrlnp))
					n = -1;			// Force beep.
				else {
					lnp1 = lforw(bufp->b_hdrlnp);
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
				StrList msg;

				// Get string list...
				if(vopen(&msg,NULL,false) != 0)
					return vrcset();

				// build prompt...
				if(vputs(text202,&msg) != 0)
						// "(<SPC>,f"
					return vrcset();
				if(hkey(&msg,forwPage,0) != SUCCESS)
					return rc.status;
				if(vputs(text203,&msg) != 0)
						// ") +page (b"
					return vrcset();
				if(hkey(&msg,backPage,0) != SUCCESS)
					return rc.status;
				if(vputs(text204,&msg) != 0)
						// ") -page (d) +half (u) -half"
					return vrcset();
				if(hkey(&msg,forwLine,'+') != SUCCESS || hkey(&msg,backLine,'-') != SUCCESS)
					return rc.status;
				if(vputs(text206,&msg) != 0)
						// " (g) first (G) last (ESC,q) quit (?) help: "
					return vrcset();

				// and display it.
				if(vclose(&msg) != 0)
					return vrcset();
				mlputs(MLHOME | MLFORCE,msg.sl_vp->v_strp);
				}
			else {
				// Other key.  "Unget" the key for reprocessing and return.
				tungetc(ek);
				goto uexit;
				}
			}
		}
uexit:
	uphard();
	if(endprompt)
		mlerase(MLFORCE);

	return rc.status;
	}
