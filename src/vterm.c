// (c) Copyright 2026 Richard W. Marinelli
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
//    corresponding attributes (on or off).  Attribute sequences begin with the AttrSpecBegin (~) character and may be any of
//    the following:
//	~b	Begin bold.			~r	Begin reverse.			~u	Begin underline.
//	~B	End bold.			~R	End reverse.			~#u	Begin underline, skip spaces.
//	~<n>c	Begin color pair n.		~~	Literal ~.			~U	End underline.
//	~C	End color.			~Z	End all attributes.

#include "std.h"
#include <stdarg.h>
#include "bind.h"
#include "file.h"

/*** Local declarations ***/

// The display system uses an array of line flags allocated at startup which can accommodate the maximum possible screen size.
// The flags are used to managed extended lines and to keep track of which line in a window contains point.  There is also the
// concept of an "internal virtual screen", which is a virtual screen maintained by the editor that is different than the one
// maintained by the ncurses system which also holds the actual text of each line.  The internal screen is somewhat transitory
// and holds only the minimum information necessary to move window data lines and mode lines to ncurses.

// Virtual line flags.
#define VFExt		0x0001		// Extended virtual line (beyond terminal width).
#define VFPoint		0x0002		// Point line in associated window.

// Virtual screen control parameters.
typedef struct {
	int tabOffset;			// Offset for expanding tabs to virtual screen when line(s) are left-shifted.
	int col;			// Column location of virtual cursor.
	int row;			// Row location of virtual cursor.
	ushort *pFlags;			// Pointer to current line flags.
	} VideoCtrl;
static VideoCtrl videoCtrl = {0, 0, 0, NULL};
static ushort *lineFlagTable;		// Virtual screen line flags.

// Initialize the data structures used by the display system.  The virtual and physical screens are allocated and the operating
// system's terminal I/O channels are opened.  Return status.
int vtinit(void) {

	if(topen() != Success)		// Open the screen and keyboard.
		return sess.rtn.status;
	(void) attrset(0);		// Turn off all attributes.

	// Allocate the virtual screen line flags.  The vertical size is one less row than the maximum because the message
	// line is managed separately.
	if((lineFlagTable = (ushort *) calloc(TTY_MaxRows - 1, sizeof(ushort))) == NULL)
		return rsset(Panic, 0, text94, "vtinit");
			// "%s(): Out of memory!"

	return sess.rtn.status;
	}

// Set the virtual cursor to the specified row and column on the virtual screen.  If column is negative, move cursor to column
// zero so that vtputc() will write characters to the correct column when it becomes non-negative.  Return status.
static int vtmove(int row, int col) {

	videoCtrl.pFlags = &lineFlagTable[row];
	videoCtrl.row = row;
	videoCtrl.col = col;
	return tmove(row, col < 0 ? 0 : col);
	}

// Erase to end of line if possible, beginning at the current column in the virtual screen.  If the column is negative, it is
// assumed that the cursor is at column zero (so the ncurses call will succeed).  If the column is past the right edge, do
// nothing.
static void vteeol() {

	if(videoCtrl.col < term.cols)
		(void) clrtoeol();
	}

// Process a terminal attribute specification in a string.  If TA_ScanOnly flag is set, 'targ' is assumed to be of type 'int *',
// otherwise 'WINDOW *'.  If valid spec found, write attribute or AttrSpecBegin character to ncurses window in 'targ', update
// TA_AltUL flag in *flags if applicable, and return updated 'str' pointer; otherwise, return updated 'str' pointer so that the
// invalid spec is skipped over.  In either case, set 'targ' to the number of invisible characters in the sequence if
// TA_ScanOnly flag is set; that is, number of characters which will not be displayed (usually its length).  'str' is assumed to
// point to the character immediately following the AttrSpecBegin (~) character.
char *putAttrStr(const char *str, const char *strEnd, uint *flags, void *targ) {
	short c, foreground, background;
	attr_t attr;
	int attrLen = 1;
	bool altForm = false;
	long i = 0;
	WINDOW *pWindow = (WINDOW *) targ;
	int *pCount = (int *) targ;

	if(str == strEnd)					// End of string?
		goto Retn;					// Yes.
	if((c = *str++) == AttrSpecBegin) {			// ~~ sequence?
		if(!(*flags & TA_ScanOnly)) {			// Yes, process it.
			(void) waddch(pWindow, AttrSpecBegin);
			++videoCtrl.col;
			}
		}
	else {
		if(c == AttrAlt) {				// Check for '#'.
			attrLen = 2;
			if(str == strEnd)			// ~# at EOL.
				goto Retn;
			altForm = true;
			c = *str++;
			}

		// Check for an integer prefix.
		if(c >= '0' && c <= '9') {
			do {
				++attrLen;
				if(str == strEnd)
					goto Retn;
				i = i * 10 + (c - '0');
				} while((c = *str++) >= '0' && c <= '9');
			}

		++attrLen;					// Count spec letter.
		switch(c) {
			case AttrAllOff:
				if(!(*flags & TA_ScanOnly)) {
					*flags &= ~TA_AltUL;
					(void) wattrset(pWindow, 0);
					}
				break;
			case AttrBoldOn:
				attr = A_BOLD;
				goto SetAttr;
			case AttrBoldOff:
				attr = A_BOLD;
				goto ClearAttr;
			case AttrColorOn:
				if(i > term.maxPair || pair_content(i, &foreground, &background) == ERR)
					goto Retn;
				if(*flags & TA_ScanOnly)
					break;
				attr = COLOR_PAIR(i);
				goto SetAttr;
			case AttrColorOff:
				if(*flags & TA_ScanOnly)
					break;
				(void) wattr_get(pWindow, &attr, &c, NULL);
				attr = COLOR_PAIR(c);
				goto ClearAttr;
			case AttrRevOn:
				attr = A_REVERSE;
				goto SetAttr;
			case AttrRevOff:
				attr = A_REVERSE;
				goto ClearAttr;
			case AttrULOn:
				if(*flags & TA_ScanOnly)
					break;
				attr = A_UNDERLINE;
				if(altForm)
					*flags |= TA_AltUL;
SetAttr:
				if(!(*flags & TA_ScanOnly))
					(void) wattron(pWindow, attr);
				break;
			case AttrULOff:
				if(*flags & TA_ScanOnly)
					break;
				attr = A_UNDERLINE;
				*flags &= ~TA_AltUL;
ClearAttr:
				if(!(*flags & TA_ScanOnly))
					(void) wattroff(pWindow, attr);
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
	if(*flags & TA_ScanOnly)
		*pCount = attrLen;
	return (char *) str;
	}

// Scan a string for terminal attribute sequences and return the total number of characters found that are not visible when
// the string is displayed (zero if none).  Invalid sequences or any that would begin at or past "maxOffset" are ignored.  If
// len < 0, string is assumed to be null terminated.
int attrCount(const char *str, int len, int maxOffset) {
	const char *strEnd;
	int count;
	int curOffset;			// Running count of visible characters.
	uint flags = TA_ScanOnly;

	strEnd = (len < 0) ? strchr(str, '\0') : str + len;
	curOffset = len = 0;
	while(str < strEnd && curOffset < maxOffset) {
		if(*str++ == AttrSpecBegin) {
			if(str == strEnd)
				break;
			if(*str == AttrSpecBegin) {
				++str;
				++len;
				++curOffset;
				}
			else {
				str = (const char *) putAttrStr(str, strEnd, &flags, (void *) &count);
				len += count;
				}
			}
		else
			++curOffset;
		}

	return len;
	}

// Write a character to the virtual screen.  The virtual row and column are updated.  If we are off the left or right edge of
// the screen, don't write the character.  If the line is too long put a "$" in the last column.  Non-printable characters are
// converted to visible form.  It is assumed that tab characters are converted to spaces by the caller.  Return number of
// characters written.
static int vtputc(short c) {
	int col0 = videoCtrl.col;
	if(videoCtrl.col >= term.cols) {

		// We are past the right edge -- add a '$' if just past.
		if(videoCtrl.col++ == term.cols)
			(void) mvaddch(videoCtrl.row, term.cols - 1, LineExt);
		}
	else {
		if(c < ' ' || c == 0x7f) {

			// Control character.
			vtputc('^');
			vtputc(c ^ 0x40);
			}
		else if(c > 0x7f) {
			char workBuf[5], *str = workBuf;

			// Display character with high bit set symbolically.
			sprintf(workBuf, "<%.2X>", c);
			do {
				vtputc(*str++);
				} while(*str != '\0');
			}
		else {
			// Plain character; just put it in the screen map.
			if(videoCtrl.col >= 0)
				(void) addch(c);
			++videoCtrl.col;
			}
		}
	return videoCtrl.col - col0;
	}

// Check if the line containing point is in the given window and reframe it if needed or requested.  Return status.
int wupd_reframe(EWindow *pWind) {
	Line *pLine;
	int i;
	int lineCount;
	Line *pDotLine = pWind->face.point.pLine;

	// Get current window size.
	lineCount = pWind->rows;

	// If not a forced reframe, check for a needed one.
	if(!(pWind->face.wflags & WFReframe)) {

		// If the top line of the window is at EOB...
		pLine = pWind->face.pTopLine;
		if(pLine->next == NULL && pLine->used == 0) {
			if(pLine == pWind->pBuf->pFirstLine)
				return sess.rtn.status;		// Buffer is empty, no reframe needed.
			goto Reframe;				// Buffer is not empty, reframe.
			}

		// Check if point is in the window.
		if(lineInWind(pWind, pDotLine))
			return sess.rtn.status;			// Found point... no reframe needed.
		}
Reframe:
	// Reaching here, we need a window refresh.  Either the top line of the window is at EOB and the buffer is not empty (a
	// user command or function buffer), the line containing point is not in the window, or it's a force.  Set i to the new
	// window line number (0..lineCount - 1) where the point line should "land".  If it's a force, use reframeRow for i
	// and just range-check it; otherwise, calcluate a reasonable value (either vertJumpPct/100 from top, center of window,
	// or vertJumpPct/100 from bottom) depending on direction point has moved.
	i = pWind->reframeRow;

	// If not a force...
	if(!(pWind->face.wflags & WFReframe)) {
		Line *pForwLine, *pBackLine, *pLine1;

		// Search through the buffer in both directions looking for point, counting lines from top of window.  If point
		// is found, set i to top or bottom line if smooth scrolling, or vertJumpPct/100 from top or bottom if jump
		// scrolling.
		pForwLine = pBackLine = pWind->face.pTopLine;
		pLine1 = pWind->pBuf->pFirstLine;
		i = 0;
		for(;;) {
			// Did point move downward?
			if(pForwLine == pDotLine) {

				// Yes.  Force i to center of window if point moved more than one line past the bottom.
				i = (i > lineCount) ? lineCount / 2 : (vTerm.vertJumpPct == 0) ? lineCount - 1 :
				 lineCount * (100 - vTerm.vertJumpPct) / 100;
				break;
				}

			// Did point move upward?
			if(pBackLine == pDotLine) {

				// Yes.  Force i to center of window if point moved more than one line past the top or
				// vertJumpPct is zero and point is at end of buffer.
				i = (i > 1) ? lineCount / 2 : (vTerm.vertJumpPct > 0) ? lineCount * vTerm.vertJumpPct / 100 :
				 (pDotLine->next == NULL) ? lineCount / 2 : 0;
				break;
				}

			// Advance forward and back.
			if(pForwLine->next != NULL)
				pForwLine = pForwLine->next;
			else {
				if(pBackLine == pLine1)
					break;
				goto Back;
				}
			if(pBackLine != pLine1)
Back:
				pBackLine = pBackLine->prev;
			++i;
			}
		}

	// Forced reframe.  Make sure i is in range (within the window).
	else if(i > 0) {			// Position down from top?
		if(--i >= lineCount)		// Too big...
			i = lineCount - 1;	// use bottom line.
		}
	else if(i < 0) {			// Position up from bottom?
		i += lineCount;
		if(i < 0)			// Too small...
			i = 0;			// use top line.
		}
	else					// i is zero.
		i = lineCount / 2;		// Center point line in window.

	// Now set top line i lines above point line.
	windNewTop(pWind, pDotLine, -i);
	pWind->face.wflags &= ~WFReframe;

	return sess.rtn.status;
	}

// Write a buffer line to the virtual screen.  Expand hard tabs to spaces and convert attribute sequences to ncurses calls if
// doAttr is true.
static void vtputln(Line *pLine, bool doAttr) {
	char *str, *strEnd;
	short c;
	uint flags = 0;			// Skip spaces when underlining?

	strEnd = (str = pLine->text) + pLine->used;
	while(str < strEnd) {
		if((c = *str++) == '\t' || c == ' ') {
			if(doAttr && flags & TA_AltUL)
				(void) attroff(A_UNDERLINE);

			// Output hardware tab as the right number of spaces.
			do {
				vtputc(' ');
				} while(c == '\t' && (videoCtrl.col + videoCtrl.tabOffset) % sess.pCurScrn->hardTabSize != 0);
			if(doAttr && flags & TA_AltUL)
				(void) attron(A_UNDERLINE);
			continue;
			}
		else if(c == AttrSpecBegin && doAttr) {
			str = putAttrStr(str, strEnd, &flags, (void *) stdscr);
			continue;
			}
		vtputc(c);
		}
	if(doAttr)				// Always leave terminal attributes off.
		(void) attrset(0);
	}

// Transfer all lines in given window to virtual screen.
static int vupd_wind(EWindow *pWind) {
	Line *pLine;		// Line to update.
	int row;		// Virtual screen line to update.
	int endRow;		// Last row in current window + 1.
	bool attrOn = pWind->pBuf->flags & BFTermAttr && (!modeSet(MdIdxHScrl, pWind->pBuf) ||
	 pWind->face.firstCol == 0);
	Line *pCurLine = (pWind == sess.pCurWind) ? pWind->face.point.pLine : NULL;

	// Loop through the lines in the window, updating the corresponding rows in the virtual screen.
	pLine = pWind->face.pTopLine;
	endRow = (row = pWind->topRow) + pWind->rows;
	do {
		// Update the virtual line.
		videoCtrl.tabOffset = modeSet(MdIdxHScrl, pWind->pBuf) ? pWind->face.firstCol :
		 pLine == pCurLine ? sess.pCurScrn->firstCol : 0;
		if(vtmove(row, -videoCtrl.tabOffset) != Success)
			break;
		if(videoCtrl.tabOffset == 0)
			*videoCtrl.pFlags &= ~VFExt;

		// If we are not at the end...
		if(pLine != NULL) {
#if MMDebug & Debug_VTerm
			if(!all) {
				ExtKey extKey;
				char workBuf[32];
				sprintf(workBuf, "%.*s", pLine->used < 20 ? pLine->used : 20, pLine->text);
				mlprintf(MLHome | MLFlush,
				 "Calling vtputln(%s...) from vupd_wind(): row %d, videoCtrl.tabOffset %d, v_flags %d... ",
				 workBuf, row, videoCtrl.tabOffset, (int) *videoCtrl.pFlags);
				getkey(true, &extKey, false);
				}
#endif
			vtputln(pLine, attrOn && pLine != pCurLine);
			pLine = pLine->next;
			}

		// Truncate virtual line.
		vteeol();
		} while(++row < endRow);
	videoCtrl.tabOffset = 0;
	return sess.rtn.status;
	}

// Set given flags on all windows on current screen.  If pBuf is not NULL, mark only those windows.
void supd_windFlags(Buffer *pBuf, ushort flags) {

	if(pBuf == NULL || pBuf->windCount > 0) {
		EWindow *pWind;

		// Only need to update windows on current screen because windows on background screens are automatically
		// updated when the screen to brought to the foreground.
		pWind = sess.windHead;
		do {
			if(pBuf == NULL || pWind->pBuf == pBuf)
				pWind->face.wflags |= flags;
			} while((pWind = pWind->next) != NULL);
		}
	}

// De-extend and/or re-render any line in any window on the virtual screen that needs it; for example, a line in the current
// window that in a previous update() cycle was the current line, or the current line in another window that was previously the
// current window.  Such lines were marked with the VFExt and VFPoint flags by the wupd_cursor() function.  The current line in
// the current window is skipped, however.
static int supd_dextend(void) {
	EWindow *pWind;
	Line *pLine;
	int row;
	int endRow;
	bool attrOn;

	pWind = sess.windHead;
	do {
		attrOn = pWind->pBuf->flags & BFTermAttr && (!modeSet(MdIdxHScrl, pWind->pBuf) ||
		 pWind->face.firstCol == 0);
		pLine = pWind->face.pTopLine;
		endRow = (row = pWind->topRow) + pWind->rows;
		do {
			if(vtmove(row, 0) != Success)
				return sess.rtn.status;

			// Update virtual line if it's currently extended (shifted) or is a point line that may need rendering.
			if((pWind != sess.pCurWind || pLine != pWind->face.point.pLine) && (*videoCtrl.pFlags & VFExt ||
			 (*videoCtrl.pFlags & VFPoint && attrOn))) {
				if(pLine != NULL) {
#if MMDebug & Debug_VTerm
					ExtKey extKey;
					char workBuf[32];
					sprintf(workBuf, "%.*s", pLine->used < 20 ? pLine->used : 20, pLine->text);
					mlprintf(MLHome | MLFlush, "supd_dextend(): De-extending line '%s'...", workBuf);
					getkey(true, &extKey, false);
#endif
					videoCtrl.tabOffset = 0;
					vtputln(pLine, attrOn);
					}
				vteeol();

				// This line is no longer extended nor a point line that needs to be re-rendered.
				*videoCtrl.pFlags &= ~(VFExt | VFPoint);
				}
			if(pLine != NULL)
				pLine = pLine->next;
			} while(++row < endRow);

		// Onward to the next window.
		} while((pWind = pWind->next) != NULL);
	return sess.rtn.status;
	}

// Transfer the current line in given window to the virtual screen.
static int vupd_dotLine(EWindow *pWind) {
	Line *pLine;		// Line to update.
	int row;		// Physical screen line to update.

	// Search down to the line we want...
	pLine = pWind->face.pTopLine;
	row = pWind->topRow;
	while(pLine != pWind->face.point.pLine) {
		++row;
		pLine = pLine->next;
		}

	// and update the virtual line.
	videoCtrl.tabOffset = modeSet(MdIdxHScrl, pWind->pBuf) ? pWind->face.firstCol : (pWind == sess.pCurWind) ?
	 sess.pCurScrn->firstCol : 0;
	if(vtmove(row, -videoCtrl.tabOffset) == Success) {
		vtputln(pLine, pWind != sess.pCurWind && pWind->pBuf->flags & BFTermAttr &&
		 (!modeSet(MdIdxHScrl, pWind->pBuf) || pWind->face.firstCol == 0));
		vteeol();
		videoCtrl.tabOffset = 0;
		}
	return sess.rtn.status;
	}

// Write "== " to mode line on virtual screen and return count.
static int vttab(short lineChar) {

	vtputc(lineChar);
	vtputc(lineChar);
	vtputc(' ');
	return 3;
	}

// Write a null-terminated string to the virtual screen.  It is assumed the string does not contain attribute sequences.  Return
// number of characters written.
static int vtputs(const char *str) {
	short c;
	int count = 0;

	while((c = *str++) != '\0')
		count += vtputc(c);
	return count;
	}

// Redisplay the mode line in the given window and return status.  If popBuf is NULL, display a fully-formatted mode line
// containing the current buffer's name and filename (but in condensed form if the current terminal width is less than 96
// columns); otherwise, display only the buffer name and filename of buffer "popBuf".  This is the only routine that has any
// idea of how the mode line is formatted.
static int vupd_modeLine(EWindow *pWind, Buffer *popBuf) {
	short c;
	short lineChar;
	int i;
	Buffer *pBuf;
	ModeSpec *pModeSpec;
	HashNode *pNode, **pAry;
	BufMode *pBufMode = pWind->pBuf->modes;
	bool colorML = term.itemColors[ColorIdxModeLn].colors[0] >= -1;
	short condensed = term.cols < 80 ? -1 : term.cols < 96 ? 1 : 0;
	char workBuf[32];				// Work buffer.
	struct MsgLineChars {				// Mode display parameters.
		short leadChar, trailChar;
		} *pMsgLineChars;
	static struct MsgLineChars msgLineCharsTable[] = {
		{'(', ')'},
		{'[', ']'},
		{-1, -1}};
	static ushort progNameLen = sizeof(ProgName);
	static ushort progVerLen = sizeof(ProgVer);

	i = pWind->topRow + pWind->rows;		// Row location.
	if(vtmove(i, 0) != Success)			// Seek to right line on virtual screen.
		return sess.rtn.status;
	if(colorML)			// Use color mode line if possible, otherwise reverse video.
		(void) attron(COLOR_PAIR(term.maxPair - ColorPairML));
	else
		trev(false, true);
	if(pWind == sess.pCurWind)			// Make the current window's mode line stand out.
		lineChar = '=';
	else if(sess.opFlags & OpHaveRev)
		lineChar = ' ';
	else
		lineChar = '-';

	// Full display?
	if(popBuf == NULL) {
		pBuf = pWind->pBuf;
		vtputc(pBuf->flags & BFNarrowed ? SB_Narrowed : lineChar);	// < if narrowed.
		vtputc(pBuf->flags & BFChanged ? SB_Changed :			// * if changed.
		 pBuf->flags & BFReadOnly ? SB_ReadOnly : lineChar);		// # if read-only.
		vtputc(' ');
		i = 3;

		// Is window horizontally scrolled?
		if(pWind->face.firstCol > 0) {
			sprintf(workBuf, "[<%d] ", pWind->face.firstCol);
			i += vtputs(workBuf);
			}

		// If bottom window...
		if(pWind->next == NULL) {

			// Display macro recording state.
			if(macro.state == MacRecording) {
				if(term.itemColors[ColorIdxMRI].colors[0] >= -1) {
					attr_t colorAttr = COLOR_PAIR(term.maxPair - ColorPairMRI);
					char workBuf[16];

					sprintf(workBuf, " %s ", text435);
							// "REC"

					// If color active, just change colors; otherwise, need to turn off reverse video first.
					if(colorML) {
						(void) attron(colorAttr);
						tbold(false, true);
						i += vtputs(workBuf);
						tbold(false, false);
						(void) attron(COLOR_PAIR(term.maxPair - ColorPairML));
						}
					else {
						trev(false, false);
						(void) attron(colorAttr);
						tbold(false, true);
						i += vtputs(workBuf);
						tbold(false, false);
						(void) attroff(colorAttr);
						trev(false, true);
						}
					i += vtputc(' ');
					}
				else {
					tbold(false, true);
					i += vtputc('*');
					i += vtputs(text435);
						// "REC"
					i += vtputc('*');
					tbold(false, false);
					i += vtputc(' ');
					}
				}

			// Display the screen number if there's more than one screen.
			if((c = scrnCount()) > 1) {
				sprintf(workBuf, "S%hu/%hd ", sess.pCurScrn->num, c);
				i += vtputs(workBuf);
				}
			}

		// Display the line and/or column point position if enabled.
		if(modeSet(MdIdxLine, pBuf)) {
			sprintf(workBuf, "L:%'d ", getLineNum(pBuf, pWind->face.point.pLine));
			i += vtputs(workBuf);
			}
		if(modeSet(MdIdxCol, pBuf)) {
			sprintf(workBuf, "C:%'d ", getCol(&pWind->face.point, false));
			i += vtputs(workBuf);
			}

		// Display the modes.  If condensed display, use short form.  If not bottom window, skip global modes.
		if(ohAryPrep(&mode.idx, &pAry) != 0)
			return libfail();
		pMsgLineChars = msgLineCharsTable;
		do {
			// If bottom screen or not global modes...
			if(pWind->next == NULL || pMsgLineChars != msgLineCharsTable) {
				c = pMsgLineChars->leadChar;
				for(;;) {
					if(pMsgLineChars == msgLineCharsTable) {

						// Get next global mode.
						if((pNode = *pAry++) == NULL)
							break;
						pModeSpec = modePtr(pNode->pVal);

						// Skip if not a global mode or mode is disabled.
						if((pModeSpec->flags & (MdGlobal | MdActive)) != (MdGlobal | MdActive))
							continue;
						}
					else if(pBufMode == NULL)
						break;
					else {
						// Get next buffer mode.
						pModeSpec = pBufMode->pModeSpec;
						pBufMode = pBufMode->next;
						}

					// Display mode if not hidden.
					if(!(pModeSpec->flags & MdHidden)) {
						i += vtputc(c);
						c = ' ';
						if(condensed >= 0)
							i += vtputs(pModeSpec->name);
						else {
							i += vtputc(pModeSpec->name[0]);
							if(pModeSpec->name[1] != '\0')
								i += vtputc(pModeSpec->name[1]);
							}
						}
					}
				if(c != pMsgLineChars->leadChar) {
					i += vtputc(pMsgLineChars->trailChar);
					i += vtputc(' ');
					}
				}
			} while((++pMsgLineChars)->leadChar > 0);
#if 0
		// Display internal modes on modeline.
		vtputc(lineChar);
		vtputc(pWind->face.wflags & WFMode ? 'M' : lineChar);
		vtputc(pWind->face.wflags & WFHard ? 'H' : lineChar);
		vtputc(pWind->face.wflags & WFEdit ? 'E' : lineChar);
		vtputc(pWind->face.wflags & WFMove ? 'V' : lineChar);
		vtputc(pWind->face.wflags & WFReframe ? 'R' : lineChar);
		vtputc(lineChar);
		i += 8;
#endif
		if(i > 3)
			i += vttab(lineChar);
		}
	else {
		// Begin "pop" display.
		i = 0;
		pBuf = popBuf;
		vtputc(lineChar);
		i += vttab(lineChar) + 1;
		}

	// Display the buffer name.
	i += vtputs(pBuf->bufname) + 1;
	vtputc(' ');

	// Display buffer count if not a pop-up, bottom window, and mode enabled.
	if(popBuf == NULL && pWind->next == NULL && mode.cache[MdIdxBufCount]->flags & MdActive) {
		vtputc(msgLineCharsTable[1].leadChar);
		if(sess.chgBufCount > 0) {
			vtputc(SB_Changed);
			i += 2;
			}
		else
			++i;
		sprintf(workBuf, "%u%c ", sess.vizBufCount, msgLineCharsTable[1].trailChar);
		i += vtputs(workBuf);
		}

	// Display the filename in the remaining space using strfit().
	if(pBuf->filename != NULL) {
		char workBuf[TTY_MaxCols];

		i += vttab(lineChar);
		if(condensed < 0) {
			vtputc(*text34);
				// "File: "
			vtputc(':');
			vtputc(' ');
			i += 3;
			}
		else
			i += vtputs(text34);
				// "File: "
		i += vtputs(strfit(workBuf, term.cols - i, pBuf->filename, 0)) + 1;
		vtputc(' ');
		}

	// If not a pop-up...
	if(popBuf == NULL) {

		// If it's the current window, "WkDir" mode enabled, and room still available, display the working directory
		// as well.
		if(pWind == sess.pCurWind && mode.cache[MdIdxWkDir]->flags & MdActive && term.cols - i > 12) {
			char workBuf[TTY_MaxCols];

			i += vttab(lineChar);
			i += vtputs(text274);
				// "WD: "
			i += vtputs(strfit(workBuf, term.cols - i, sess.pCurScrn->workDir, 0)) + 1;
			vtputc(' ');
			}

		// If bottom window and room still available, display program name and version.
		if(!condensed && pWind->next == NULL) {
			int spacing = term.cols - i;
			if(spacing >= progNameLen + progVerLen + 5) {
				while(i < term.cols - (progNameLen + progVerLen + 1)) {
					vtputc(lineChar);
					++i;
					}
				vtputc(' ');
				sprintf(workBuf, "%s %s", Myself, Version);
				i += vtputs(workBuf) + 2;
				vtputc(' ');
				}
			else if(spacing >= progNameLen + 4) {
				while(i < term.cols - (progNameLen + 1)) {
					vtputc(lineChar);
					++i;
					}
				vtputc(' ');
				i += vtputs(Myself) + 2;
				vtputc(' ');
				}
			}
		}

	// Pad to full width and turn off all attributes.
	while(i < term.cols) {
		vtputc(lineChar);
		++i;
		}
	(void) attrset(0);
	return sess.rtn.status;
	}

// Update the position of the hardware cursor in the current window and handle extended lines.  This is the only update for
// simple moves.
static int vupd_cursor(void) {
	Line *pLine;
	int i, lastRow, *firstCol;
	Face *pFace = &sess.pCurWind->face;
	ushort windFlag = modeSet(MdIdxHScrl, NULL) || modeSet(MdIdxLine, NULL) || modeSet(MdIdxCol, NULL) ? WFMode : 0;
	ushort *pFlags;

	// Find the current row.
	pLine = pFace->pTopLine;
	lastRow = sess.pCurScrn->cursorRow;
	sess.pCurScrn->cursorRow = sess.pCurWind->topRow;
	while(pLine != pFace->point.pLine) {
		++sess.pCurScrn->cursorRow;
		pLine = pLine->next;
		}

	// Find the current column of point, ignoring terminal width.
	sess.pCurScrn->cursorCol = i = 0;
	while(i < pFace->point.offset)
		sess.pCurScrn->cursorCol = newCol(pLine->text[i++], sess.pCurScrn->cursorCol);

	// Adjust cursor column by the current first column position of window (which is active when horizontal scrolling is
	// enabled) or line (if still on same terminal row as when this routine was last called).
	if(modeSet(MdIdxHScrl, NULL))
		sess.pCurScrn->cursorCol -= *(firstCol = &pFace->firstCol);
	else {
		if(sess.pCurScrn->cursorRow == lastRow)
			sess.pCurScrn->cursorCol -= sess.pCurScrn->firstCol;
		else
			sess.pCurScrn->firstCol = 0;
		firstCol = &sess.pCurScrn->firstCol;
		}

	// Make sure it is not off the left edge of the screen or on a LineExt ($) character at the left edge.
	while(sess.pCurScrn->cursorCol < 0 || (sess.pCurScrn->cursorCol == 0 && sess.pCurScrn->firstCol > 0)) {
		if(*firstCol >= vTerm.horzJumpCols) {
			sess.pCurScrn->cursorCol += vTerm.horzJumpCols;
			*firstCol -= vTerm.horzJumpCols;
			}
		else {
			sess.pCurScrn->cursorCol += *firstCol;
			*firstCol = 0;
			}
		sess.pCurWind->face.wflags |= (WFHard | windFlag);
		}

	// Calculate window or line shift if needed.
	while(sess.pCurScrn->cursorCol >= term.cols - 1) {
		sess.pCurScrn->cursorCol -= vTerm.horzJumpCols;
		*firstCol += vTerm.horzJumpCols;
		sess.pCurWind->face.wflags |= (WFHard | windFlag);
		}

	// Mark line as a "point" line and flag it if extended so that it can be re-rendered and/or de-extended by
	// supd_dextendt() in a future update() cycle.
	pFlags = &lineFlagTable[sess.pCurScrn->cursorRow];
	if(*firstCol > 0 && !modeSet(MdIdxHScrl, NULL))
		*pFlags |= (VFPoint | VFExt);
	else
		*pFlags = *pFlags & ~VFExt | VFPoint;

	// Update the virtual screen if current line or left shift was changed, or terminal attributes are "live".
	if(sess.pCurWind->face.wflags & WFHard) {
#if MMDebug & Debug_VTerm
		ExtKey extKey;
		mlprintf(MLHome | MLFlush,
		 "Calling vupd_wind() from vupd_cursor(): firstCol %d, pCurScrn->firstCol %d, pCurScrn->cursorCol %d...",
		 pFace->firstCol, sess.pCurScrn->firstCol, sess.pCurScrn->cursorCol);
		getkey(true, &extKey, false);
#endif
		if(vupd_wind(sess.pCurWind) != Success)
			return sess.rtn.status;
		}
	else if(sess.pCurWind->face.wflags & WFEdit && vupd_dotLine(sess.pCurWind) != Success)
		return sess.rtn.status;

	// Update mode line if (1), mode flag set; or (2), "Col" buffer mode set (point movement assumed); or (3), "Line"
	// buffer mode set and any window flag set.
	if(sess.pCurWind->face.wflags & WFMode || modeSet(MdIdxCol, NULL) ||
	 (sess.pCurWind->face.wflags && modeSet(MdIdxLine, NULL)))
		vupd_modeLine(sess.pCurWind, NULL);
	sess.pCurWind->face.wflags = 0;

	// If horizontal scrolling mode not enabled and line is shifted, put a '$' in column 0.
	if(!modeSet(MdIdxHScrl, NULL) && sess.pCurScrn->firstCol > 0)
		(void) mvaddch(sess.pCurScrn->cursorRow, 0, LineExt);
	return sess.rtn.status;
	}

// Make sure that the display is right.  This is a four-part process.  First, resize the windows in the current screen to match
// the current terminal dimensions if needed.  Second, scan all of the windows on the current screen looking for dirty (flagged)
// ones.  Check the framing and refresh the lines.  Third, make sure that the cursor row and column are correct for the current
// window.  And fourth, make the virtual and physical screens the same.  Entire update is skipped if keystrokes are pending or a
// macro is executing unless n != 0.  Physical screen update is skipped if n <= 0.  Return status.
//
//  n value	Skip update if keystrokes	Redraw physical screen
//		 or macro running		 if needed
//  =======	=========================	======================
//  default		yes				yes
//  n < 0		no				no
//  n == 0		yes				no
//  n > 0		no				yes
int update(long n) {
	EWindow *pWind;
	int keyCount;

	// If we are not forcing the update...
	if(n == DefN || n == 0) {

		// If there are keystrokes waiting to be processed or a macro is being played back, skip this update.
		if(typahead(&keyCount) != Success || keyCount > 0 || macro.state == MacPlaying)
			return sess.rtn.status;
		}

	// Current screen dimensions wrong?
	if(sess.pCurScrn->flags) {			// EScrnResize set?
		EWindow *lastWind, *nextWind;
		int rows;

		// Loop until vertical size of all windows matches terminal rows.
		do {
			// Does current screen need to grow vertically?
			if(term.rows > sess.pCurScrn->rows) {

				// Yes, go to the last window...
				pWind = windNextIs(NULL);

				// and enlarge it as needed.
				pWind->rows = (sess.pCurScrn->rows = term.rows) - pWind->topRow - 2;
				pWind->face.wflags |= (WFHard | WFMode);
				}

			// Does current screen need to shrink vertically?
			else if(term.rows < sess.pCurScrn->rows) {

				// Rebuild the window structure.
				nextWind = sess.pCurScrn->windHead;
				lastWind = NULL;
				rows = 0;
				do {
					pWind = nextWind;
					nextWind = pWind->next;

					// Get rid of window if it is too low.
					if(pWind->topRow >= term.rows - 2) {

						// Save the "face" parameters.
						--pWind->pBuf->windCount;
						windFaceToBufFace(pWind, pWind->pBuf);

						// Update sess.pCurWind and lastWind if needed.
						if(pWind == sess.pCurWind) {

							// Defer any non-fatal error.
							if(wswitch(sess.windHead, 0) <= FatalError)
								return sess.rtn.status;
							}
						if(lastWind != NULL)
							lastWind->next = NULL;

						// Free the structure.
						free((void *) pWind);
						pWind = NULL;
						}
					else {
						// Need to change this window size?
						if(pWind->topRow + pWind->rows - 1 >= term.rows - 2) {
							pWind->rows = term.rows - pWind->topRow - 2;
							pWind->face.wflags |= (WFHard | WFMode);
							}
						rows += pWind->rows + 1;
						}

					lastWind = pWind;
					} while(nextWind != NULL);
				sess.pCurScrn->rows = rows;
				}
			} while(sess.pCurScrn->rows != term.rows);

		// Update screen controls and mark screen for a redraw.
		sess.pCurScrn->cols = term.cols;
		sess.pCurScrn->flags = 0;
		sess.opFlags |= OpScrnRedraw;
		}

	// If physical screen is garbage and redraw not being suppressed, force a redraw.
	if(sess.opFlags & OpScrnRedraw && (n == DefN || n > 0)) {
		if(mlerase(MLForce) != Success)				// Clear the message line.
			return sess.rtn.status;
		supd_windFlags(NULL, WFHard | WFMode);			// Force all windows to update.
		sess.opFlags &= ~OpScrnRedraw;				// Clear redraw flag.
		}

	// Check all windows and update virtual screen for any that need refreshing.
	pWind = sess.windHead;
	do {
		if(pWind->face.wflags) {					// WFMove is set, at minimum.

			// The window has changed in some way: service it.
			if(wupd_reframe(pWind) != Success)			// Check the framing.
				return sess.rtn.status;

			// Promote update if WFEdit and WFMove both set or WFMove set and terminal attributes enabled in buffer.
			if((pWind->face.wflags & (WFEdit | WFMove)) == (WFEdit | WFMove) ||
			 (pWind->face.wflags & WFMove && pWind->pBuf->flags & BFTermAttr))
				pWind->face.wflags |= WFHard;

			// Skip remaining tasks for current window.  These will be done by vupd_cursor().
			if(pWind != sess.pCurWind) {
				if((pWind->face.wflags & ~WFMode) == WFEdit) {
					if(vupd_dotLine(pWind) != Success)	// Update current (edited) line only.
						return sess.rtn.status;
					}
				else if(pWind->face.wflags & WFHard) {
					if(vupd_wind(pWind) != Success)		// Update all lines.
						return sess.rtn.status;
					}
				if(pWind->face.wflags & WFMode)
					vupd_modeLine(pWind, NULL);		// Update mode line.
				pWind->face.wflags = pWind->reframeRow = 0;
				}
			}

		// On to the next window.
		} while((pWind = pWind->next) != NULL);

	// Update lines in current window if needed and recalculate the hardware cursor location.
	if(vupd_cursor() != Success)
		return sess.rtn.status;

	// Check for lines to de-extend.
	if(!modeSet(MdIdxHScrl, NULL) && supd_dextend() != Success)
		return sess.rtn.status;

	// If updating physical screen...
	if(n == DefN || n > 0) {

		// update the cursor position and flush the buffers.
		if(tmove(sess.pCurScrn->cursorRow, sess.pCurScrn->cursorCol) == Success)
			(void) tflush();
		}

#if MMDebug & Debug_ScrnDump
	dumpScreens("Exiting update()");
#endif
	return sess.rtn.status;
	}

// Do "updateScreen" function.  Return status.
int updateScrn(Datum *pRtnVal, long n, Datum **args) {
	OptHdr *pOptHdr = cmdFuncTable[cf_updateScreen].pOptHdr;

	// Get options and validate them.
	if(pOptHdr->ctrlFlags & OHdrFound) {
		if(pOptHdr->optTable[0].ctrlFlags & OptSelected) {		// Force?
			if(pOptHdr->optTable[1].ctrlFlags & OptSelected)	// Yes, NoRedraw?
				n = -1;						// Yes, Force and NoRedraw.
			else
				n = 1;						// No, Force only.
			}
		else
			n = 0;							// NoRedraw only.
		}
	else
		n = DefN;							// Neither option.

	return update(n);
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
int bpop(Buffer *pBuf, uint flags) {
	Line *pLine1, *pLine, *pLineMax;
	int row;			// Current screen row number.
	int displayRows;		// Total number of display rows.
	int halfPage;			// Rows in a half page.
	int i;				// Rows to move.
	ExtKey extKey;			// Input extended key.
	int firstCol = 0;		// First column displayed.
	char *helpPrompt = NULL;	// Help prompt;
	bool firstPass = true;

	// Find last window on screen and rewrite its mode line.  Display special mode line if requested.
	vupd_modeLine(windNextIs(NULL), flags & RendAltML ? pBuf : NULL);

	// Check if buffer will fit on one page and if not, set pLineMax to first line of last page.
	displayRows = term.rows - 2;
	pLineMax = NULL;
	i = 0;
	pLine = pBuf->pFirstLine;
	do {
		if(++i > displayRows) {

			// Find beginning of last page.
			pLineMax = pBuf->pFirstLine;
			i = displayRows;
			do {
				pLineMax = pLineMax->prev;
				} while(--i > 0);
			break;
			}
		} while((pLine = pLine->next) != NULL);

	// Determine left shift, if any.
	if(flags & RendShift) {
		Point workPoint;
		int col, maxCol = 0;

		// Scan buffer, find column position of longest line...
		workPoint.pLine = pBuf->pFirstLine;
		do {
			workPoint.offset = workPoint.pLine->used;
			if((col = getCol(&workPoint, false)) > maxCol)
				maxCol = col;
			} while((workPoint.pLine = workPoint.pLine->next) != NULL);

		// and set shift amount so that rightmost portion of longest line will be visible.
		if(maxCol > term.cols)
			firstCol = maxCol - (term.cols - 1);
		}

	// Begin at the beginning.
	pLine1 = pBuf->pFirstLine;
	halfPage = displayRows / 2;
	i = 0;

	// Display a page (beginning at line pLine1 + i) and prompt for a naviagtion command.  Loop until exit key entered or
	// RendWait flag not set and buffer fits on one page (pLineMax is NULL).
	for(;;) {
		pLine = pLine1;

		// Moving backward?
		if(i < 0) {
			if(pLineMax != NULL)
				do {
					// At beginning of buffer?
					if(pLine1 == pBuf->pFirstLine)
						break;

					// No, back up one line.
					pLine1 = pLine1->prev;
					} while(++i < 0);
			}

		// Moving forward?
		else if(i > 0) {
			if(pLineMax != NULL)
				do {
					// At end of buffer or max line?
					if(pLine1->next == NULL || pLine1 == pLineMax)
						break;

					// No, move forward one line.
					pLine1 = pLine1->next;
					} while(--i > 0);
			}

		// Illegal command?
		if(i != 0 && pLine1 == pLine)

			// Yes, ignore it.
			i = 0;
		else {
			// Found first row... display page.
			pLine = pLine1;
			row = 0;
			do {
				// At end of buffer?
				if(pLine == NULL) {

					// Yes, erase remaining lines on virtual screen.
					while(row < displayRows) {
						if(vtmove(row, 0) != Success)
							return sess.rtn.status;
						(void) clrtoeol();
						*videoCtrl.pFlags = 0;
						++row;
						}
					break;
					}

				// Update the virtual screen image for this line.
				videoCtrl.tabOffset = firstCol;
				if(vtmove(row, -videoCtrl.tabOffset) != Success)
					return sess.rtn.status;
				vtputln(pLine, firstCol == 0 && pBuf->flags & BFTermAttr);
				vteeol();
				videoCtrl.tabOffset = 0;

				// If line is shifted, put a '$' in column 0.
				if(firstCol > 0)
					(void) mvaddch(row, 0, LineExt);

				// On to the next line.
				pLine = pLine->next;
				} while(++row < displayRows);

			// Screen is full.  Copy the virtual screen to the physical screen.
			if(tflush() != Success)
				return sess.rtn.status;

			// Bail out if called from termInp() and one-page buffer.
			if(firstPass && !(flags & RendWait) && pLineMax == NULL)
				goto Retn;
			firstPass = false;
			}

		// Display prompt.
		if(mlputs(helpPrompt != NULL ? helpPrompt : pLineMax == NULL || pLine1 == pLineMax ? text201 : ": ",
												// "End: "
		 MLHome | MLFlush | MLForce) != Success)
			return sess.rtn.status;

		// Get response.
		for(;;) {
			// Get a keystroke and decode it.
			if(getkey(true, &extKey, false) != Success)
				return sess.rtn.status;
			switch(extKey) {
				case Ctrl | '[':	// Exit.
				case 'q':
					goto Retn;
				case ' ':		// Forward whole page.
				case 'f':
					i = displayRows - sess.overlap;
					goto ProcCmd;
				case 'd':		// Forward half page.
					i = halfPage;
					goto ProcCmd;
				case 'b':		// Backward whole page.
					i = sess.overlap - displayRows;
					goto ProcCmd;
				case 'u':		// Backward half page.
					i = -halfPage;
					goto ProcCmd;
				case 'n':		// Forward one line.
					i = 1;
					goto ProcCmd;
				case 'p':		// Backward one line.
					i = -1;
					goto ProcCmd;
				case 'g':		// First page.
					if(pLineMax == NULL || pLine1 == pBuf->pFirstLine)
						i = -1;			// Force beep.
					else {
						pLine1 = pBuf->pFirstLine;
						i = 0;
						}
					goto ProcCmd;
				case 'G':		// Last page.
					if(pLineMax == NULL || pLine1 == pLineMax)
						i = 1;			// Force beep.
					else {
						pLine1 = pLineMax;
						i = 0;
						}
					goto ProcCmd;
				case '?':		// Display help prompt.

				// SPC|f +pg, b -pg, d +half, u -half, n +ln, p -ln, g top, G bot, ESC|q quit, ? help:
					if(mlputs(text483, MLHome | MLTermAttr | MLFlush | MLForce) != Success)
			// "~uSPC~U|~uf~U ~b+pg~B, ~ub~U ~b-pg~B, ~ud~U ~b+half~B, ~uu~U ~b-half~B, ~un~U ~b+ln~B,"
			// " ~up~U ~b-ln~B, ~ug~U ~btop~B, ~uG~U ~bbot~B, ~uESC~U|~uq~U ~bquit~B, ~u?~U ~bhelp~B: "
						return sess.rtn.status;
					break;
				default:
					// Other key.  "Unget" the key for reprocessing and return.
					ungetkey(extKey);
					goto Retn;
				}
			}
ProcCmd:;
		}
Retn:
	// Force virtual screen refresh.
	supd_windFlags(NULL, WFHard | WFMode);
	if(flags & RendWait)
		(void) mlerase(MLForce);

	return sess.rtn.status;
	}
