// (c) Copyright 2020 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// unix.c	Unix driver functions for MightEMacs.

#include "std.h"

// Kill predefined macro(s) that are used in system header files.
#undef Ctrl

#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>				// System type definitions.
#include <errno.h>
#include <pwd.h>
#if MACOS
#include <sys/syslimits.h>
#endif
#include <sys/stat.h>				// File status definitions.
#include <sys/ioctl.h>				// I/O control definitions.
#include <fcntl.h>				// File descriptor definitions.
#include <string.h>
#include <glob.h>

// Additional include files.
#if LINUX
#include <wait.h>
#endif
#if LINUX || USG
#include <time.h>
#else
#include <sys/time.h>				// Timer definitions.
#endif
#include <sys/select.h>
#include <signal.h>				// Signal definitions.
#if USG
#include <termio.h>				// Terminal I/O definitions.
#else
#include <termios.h>				// Terminal I/O definitions.
#endif
#include <term.h>
#include <locale.h>
#if USG
#include <dirent.h>				// Directory entry definitions.
#define DIRENTRY	dirent
#else
#include <sys/dir.h>				// Directory entry definitions.
#define DIRENTRY	direct
#endif

// Restore predefined macro(s).
#undef Ctrl					// Restore Ctrl.
#define Ctrl		0x0100

#include "exec.h"
#include "file.h"
#include "bind.h"

// Minimum terminal capabilities.
#define CAP_EL		"pArrayEl"			// Clear to end of line.
#define CAP_CLEAR	"clear"			// Clear to end of page.
#define CAP_CUP		"cup"			// Cursor motion.
#define CAP_BOLD	"bold"			// Bold starts.
#define CAP_REV		"rev"			// Reverse video starts.
#define CAP_SMUL	"smul"			// Underline starts.

// Pipe I/O flags.
#define PipePause	4			// Number of centiseconds to pause between pipe-read attempts.

/*** Local declarations ***/

static char *ttyPath = "/dev/tty";

// Function key tables.
typedef struct {
	short keyCode;
	ushort extKey;
	} KeyTable;

// FKey symbols used are "BCDEFINPVZ<>".  Keys that are unlikely to appear on any modern keyboard are set to '?'.
//
// Key codes generated on CentOS Linux platforms depend on TERM setting, as follows:
//			-------------------- TERM --------------------
//	KEY		vt220		xterm-color	xterm-256color
//	delete		0x7F		0x7F		KEY_BACKSPACE
//	C-h		KEY_BACKSPACE	KEY_BACKSPACE	0x08
// tigetstr(dch1)	^[[P		^[[P		^[[P
// tigetstr(kbs)	^H		^H		^?
// tigetstr(kdch1)	NULL		^[[3~		^[[3~
//
// Key codes generated on Ubuntu Linux platforms depend on TERM setting, as follows:
//			-------------------- TERM --------------------
//	KEY		vt220		xterm-color	xterm-256color
//	delete		0x7F		KEY_BACKSPACE	KEY_BACKSPACE
//	C-h		KEY_BACKSPACE	0x08		0x08
// tigetstr(dch1)	^[[P		^[[P		^[[P
// tigetstr(kbs)	^H		^?		^?
// tigetstr(kdch1)	^[[3~		^[[3~		^[[3~
//
// Key codes generated on macOS platforms do NOT depend on TERM setting:
//			---------------------------- TERM ----------------------------
//	KEY		vt100		vt220		xterm-color	xterm-256color
//	delete		0x7F		0x7F		0x7F		0x7F
//	C-h		KEY_BACKSPACE	KEY_BACKSPACE	KEY_BACKSPACE	KEY_BACKSPACE
// tigetstr(dch1)	NULL		^[[P		^[[P		^[[P
// tigetstr(kbs)	^H		^H		^H		^H
// tigetstr(kdch1)	NULL		NULL		^[[3~		^[[3~
//
// Given the above, it is sufficient to check the value returned from tigetstr("kbs") at startup.  If "\0x08" is returned, we
// can assume that the delete key will be returned as 0x7F from the getch() call, and control+h will be returned as
// KEY_BACKSPACE.  Likewise, if tigetstr("kbs") returns "\0x7F", the delete key will be KEY_BACKSPACE and control+h will be
// 0x08.  Hence, we can set the keyTable table accordingly and get consistent extended key values for those two keys.
static KeyTable keyList[] = {
	{KEY_DOWN, FKey | 'N'},			// Down-arrow key.
	{KEY_UP, FKey | 'P'},			// Up-arrow key.
	{KEY_LEFT, FKey | 'B'},			// Left-arrow key.
	{KEY_RIGHT, FKey | 'F'},		// Right-arrow key.
	{KEY_HOME, FKey | '<'},			// Home key.
	{KEY_BACKSPACE, Ctrl | 'H'},		// Backspace key.
#if 0
#define KEY_F0		0410			/* Function keys.  Space for 64 */
#define KEY_F(n)	(KEY_F0+(n))		/* Value of function key n */
#define KEY_DL		0510			/* delete-line key */
#endif
	{KEY_DL, FKey | '?'},			// Delete-line key.
	{KEY_IL, FKey | '?'},			// Insert-line key.
	{KEY_DC, FKey | 'D'},			// Delete-character key.
	{KEY_IC, FKey | 'I'},			// Insert-character key.
	{KEY_EIC, FKey | '?'},			// Sent by rmir or smir in insert mode.
	{KEY_CLEAR, FKey | 'C'},		// Clear-screen or erase key.
	{KEY_EOS, FKey | '?'},			// Clear-to-end-of-screen key.
	{KEY_EOL, FKey | '?'},			// Clear-to-end-of-line key.
	{KEY_SF, FKey | '?'},			// Scroll-forward key.
	{KEY_SR, FKey | '?'},			// Scroll-backward key.
	{KEY_NPAGE, FKey | 'V'},		// Next-page key.
	{KEY_PPAGE, FKey | 'Z'},		// Previous-page key.
	{KEY_STAB, FKey | '?'},			// Set-tab key.
	{KEY_CTAB, FKey | '?'},			// Clear-tab key.
	{KEY_CATAB, FKey | '?'},		// Clear-all-tabs key.
	{KEY_ENTER, FKey | 'E'},		// Enter/send key.
	{KEY_PRINT, FKey | '?'},		// Print key.
	{KEY_LL, FKey | '?'},			// Lower-left key (home down).
	{KEY_A1, FKey | '?'},			// Upper left of keypad.
	{KEY_A3, FKey | '?'},			// Upper right of keypad.
	{KEY_B2, FKey | '?'},			// Center of keypad.
	{KEY_C1, FKey | '?'},			// Lower left of keypad.
	{KEY_C3, FKey | '?'},			// Lower right of keypad.
	{KEY_BTAB, Shift | Ctrl | 'I'},		// Back-tab key.
	{KEY_BEG, FKey | '?'},			// Begin key.
	{KEY_CANCEL, FKey | '?'},		// Cancel key.
	{KEY_CLOSE, FKey | '?'},		// Close key.
	{KEY_COMMAND, FKey | '?'},		// Command key.
	{KEY_COPY, FKey | '?'},			// Copy key.
	{KEY_CREATE, FKey | '?'},		// Create key.
	{KEY_END, FKey | '>'},			// End key.
	{KEY_EXIT, FKey | '?'},			// Exit key.
	{KEY_FIND, FKey | '?'},			// Find key.
	{KEY_HELP, FKey | '?'},			// Help key.
	{KEY_MARK, FKey | '?'},			// Mark key.
	{KEY_MESSAGE, FKey | '?'},		// Message key.
	{KEY_MOVE, FKey | '?'},			// Move key.
	{KEY_NEXT, FKey | '?'},			// Next key.
	{KEY_OPEN, FKey | '?'},			// Open key.
	{KEY_OPTIONS, FKey | '?'},		// Options key.
	{KEY_PREVIOUS, FKey | '?'},		// Previous key.
	{KEY_REDO, FKey | '?'},			// Redo key.
	{KEY_REFERENCE, FKey | '?'},		// Reference key.
	{KEY_REFRESH, FKey | '?'},		// Refresh key.
	{KEY_REPLACE, FKey | '?'},		// Replace key.
	{KEY_RESTART, FKey | '?'},		// Restart key.
	{KEY_RESUME, FKey | '?'},		// Resume key.
	{KEY_SAVE, FKey | '?'},			// Save key.
	{KEY_SBEG, FKey | '?'},			// Shifted begin key.
	{KEY_SCANCEL, FKey | '?'},		// Shifted cancel key.
	{KEY_SCOMMAND, FKey | '?'},		// Shifted command key.
	{KEY_SCOPY, FKey | '?'},		// Shifted copy key.
	{KEY_SCREATE, FKey | '?'},		// Shifted create key.
	{KEY_SDC, FKey | '?'},			// Shifted delete-character key.
	{KEY_SDL, FKey | '?'},			// Shifted delete-line key.
	{KEY_SELECT, FKey | '?'},		// Select key.
	{KEY_SEND, FKey | '?'},			// Shifted end key.
	{KEY_SEOL, FKey | '?'},			// Shifted clear-to-end-of-line key.
	{KEY_SEXIT, FKey | '?'},		// Shifted exit key.
	{KEY_SFIND, FKey | '?'},		// Shifted find key.
	{KEY_SHELP, FKey | '?'},		// Shifted help key.
	{KEY_SHOME, FKey | '?'},		// Shifted home key.
	{KEY_SIC, FKey | '?'},			// Shifted insert-character key.
	{KEY_SLEFT, Shift| FKey | 'B'},		// Shifted left-arrow key.
	{KEY_SMESSAGE, FKey | '?'},		// Shifted message key.
	{KEY_SMOVE, FKey | '?'},		// Shifted move key.
	{KEY_SNEXT, FKey | '?'},		// Shifted next key.
	{KEY_SOPTIONS, FKey | '?'},		// Shifted options key.
	{KEY_SPREVIOUS, FKey | '?'},		// Shifted previous key.
	{KEY_SPRINT, FKey | '?'},		// Shifted print key.
	{KEY_SREDO, FKey | '?'},		// Shifted redo key.
	{KEY_SREPLACE, FKey | '?'},		// Shifted replace key.
	{KEY_SRIGHT, Shift | FKey | 'F'},	// Shifted right-arrow key.
	{KEY_SRSUME, FKey | '?'},		// Shifted resume key.
	{KEY_SSAVE, FKey | '?'},		// Shifted save key.
	{KEY_SSUSPEND, FKey | '?'},		// Shifted suspend key.
	{KEY_SUNDO, FKey | '?'},		// Shifted undo key.
	{KEY_SUSPEND, FKey | '?'},		// Suspend key.
	{KEY_UNDO, FKey | '?'},			// Undo key.
	{KEY_MOUSE, FKey | '?'},		// Mouse event has occurred.
	{KEY_RESIZE, TermResizeKey},		// Terminal resize event.
	{KEY_EVENT, FKey | '?'},		// We were interrupted by an event.
	{-1, 0},				// END OF LIST.
	};
//#define KEY_MAX		0777		/* Maximum key value is 0633 */

static ushort keyTable[KEY_EVENT - KEY_MIN + 1];// Table for fast function key lookups by index.
static DIR *dir = NULL;				// Current directory stream.
static char *filePath = NULL;			// ereaddir() file buffer (allocated from heap).
static char *filePathEnd = NULL;		// Pointer past end of path in filePath.
static const char FNioctl[] = "ioctl";
static const char FNfork[] = "fork";
static const char pipeFilename[] = "<pipe>";

// Build OS error message, append TERM to it if known and addTERM is true, and return OSError status.
int sysCallError(const char *caller, const char *call, bool addTERM) {

	(void) rsset(OSError, 0, text44, call, caller);
		// "calling %s() from %s() function"
	if(addTERM && vTerm.termName != NULL) {
		DFab msg;

		if(dopenwith(&msg, &sess.rtn.msg, FabAppend) != 0 || dputf(&msg, ", TERM '%s'", vTerm.termName) != 0 ||
		 dclose(&msg, Fab_String) != 0)
			(void) librsset(Failure);
		}
	return sess.rtn.status;
	}

// Flush virtual screen to physical screen.  Return status.
int tflush(void) {

	return refresh() == ERR ? sysCallError("tflush", "refresh", false) : sess.rtn.status;
	}

// Get a character and return it in *pChar if pChar not NULL.  Return status.
int tgetc(bool msgLine, ushort *pChar) {
	short keyCode;
	ushort c;
	WINDOW *pWindow = msgLine ? term.pMsgLineWind : stdscr;

	// Loop until we get a character or an error occurs.
	for(;;) {
		if((keyCode = wgetch(pWindow)) != ERR) {

			// Got a key.  Ignore it if unknown function key.
#if MMDebug & Debug_NCurses
			fprintf(logfile, "tgetc(): got keyCode %.4ho\n", keyCode);
			if(!((c = keyCode) & ~0xFF))
				break;
			c = keyTable[keyCode - KEY_MIN];
			fprintf(logfile, "  -> FKey x%.4hX\n", c);
			if(c != (FKey | '?'))
				break;
#else
			if(!((c = keyCode) & ~0xFF) || (c = keyTable[keyCode - KEY_MIN]) != (FKey | '?'))
				break;
#endif
			}
		else if(errno != 0)
			return sysCallError("tgetc", "wgetch", false);
		}

	if(pChar != NULL) {
		if(!(keyCode & ~0xFF)) {

			// Convert to extended character if control key.
			if(c == '\0')
				c = Ctrl | ' ';
			else if(c <= 0x1F || c == 0x7F)
				c = Ctrl | (c ^ 0x40);
			}
		*pChar = c;
		}
	return sess.rtn.status;
	}

// Get count of pending input characters.  Return status.
int typahead(int *pCount) {
	int count;

#ifdef FIONREAD

	// Get number of pending characters.
	if(ioctl(0, FIONREAD, &count) == -1)
		return sysCallError("typahead", FNioctl, false);
	*pCount = count;
#else
	// Ask hardware for count.
	count = ioctl(0, FIORDCHK, 0);
	if(count < 0)
		return sysCallError("typahead", FNioctl, false);
	*pCount = count;
#endif
	return sess.rtn.status;
	}

// Update terminal size parameters, given number of columns and rows.
void setTermSize(ushort cols, ushort rows) {

	sampBuf.smallSize = (term.cols = cols) / 4;
	term.rows = rows;
	calcHorzJumpCols();
	}

// Get current terminal window size and save (up to hard-coded maximum) to given pointers.  Return status.
// struct winsize {
//	unsigned short ws_row;		/* rows, in characters */
//	unsigned short ws_col;		/* columns, in characters */
//	unsigned short ws_xpixel;	/* horizontal size, pixels */
//	unsigned short ws_ypixel;	/* vertical size, pixels */
//	};
int getTermSize(ushort *col, ushort *row) {
	static const char myName[] = "getTermSize";
	struct winsize w;

	// Get terminal size.
	if(ioctl(0, TIOCGWINSZ, &w) == -1)
		return sysCallError(myName, FNioctl, true);

	// Do sanity check.
	if(w.ws_col < TTY_MinCols || w.ws_row < TTY_MinRows) {
		(void) endwin();
		return rsset(FatalError, 0, text190, w.ws_col, w.ws_row, Myself);
			// "Terminal size %hu x %hu is too small to run %s"
		}

	if((*col = w.ws_col) > term.maxCols)
		*col = term.maxCols;
	if((*row = w.ws_row) > term.maxRows)
		*row = term.maxRows;
	return sess.rtn.status;
	}

// Reset terminal.  Get the current terminal dimensions, update the ETerm structure, flag all screens that have different
// dimensions for a "window resize", and flag current screen for a "redraw".  Return status.  Needs to be called when the size
// of the terminal window changes; for example, when switching from portrait to landscape viewing on a mobile device.
int resetTerm(Datum *pRtnVal, int n, Datum **args) {
	static const char myName[] = "resetTermc";
	EScreen *pScrn;		// Current screen being examined.
	ushort cols, rows;	// Current terminal dimensions.
	const char *name;

	// There is a quirk in the Linux ncurses library (observed on Ubuntu and Arch Linux) where the resizeterm() call in this
	// function causes a "terminal resize" event, which is bound to this routine.  Consequently, this routine is called
	// twice on those systems when the user resizes the terminal window -- once for the actual resizing with the mouse, and
	// again for the resizeterm() call.  It is as if the user resized the window, then executed the "resetTerm" command.
	// This would cause an endless loop if resizeterm() was called unconditionally and can cause the wrong informational
	// message to be returned to the user.
	//
	// To deal with this, if a "terminal resize" event occurred (entered key is TermResizeKey) but all screens match the
	// current terminal size (event is bogus), the correct return message is set and nothing else is done.

	// Get current terminal size.
	if(getTermSize(&cols, &rows) != Success)
		return sess.rtn.status;
#if MMDebug & Debug_NCurses
	fprintf(logfile, "resetTerm(): getTermSize() returned %hu x %hu\n", cols, rows);
#endif
	// Check if size of any screen is different.
	pScrn = sess.scrnHead;
	n = 0;
	do {
		// Flag screen if its not the current terminal size.
		if(pScrn->rows != rows || pScrn->cols != cols) {
			pScrn->flags |= EScrnResize;
			n = 1;
			}
		} while((pScrn = pScrn->next) != NULL);

	// Update ETerm settings and ncurses window if size of any screen has changed; otherwise, if a bogus "terminal resize"
	// event occurred, set return message only.
	if(n) {
		setTermSize(cols, rows);
		if(resizeterm(rows, cols) == ERR) {
			name = "resizeterm";
			goto Err;
			}
		if(mvwin(term.pMsgLineWind, rows - 1, 0) == ERR) {
			name = "mvwin";
Err:
			return sysCallError(myName, name, true);
			}

		// Force full screen update.
		sess.opFlags |= OpScrnRedraw;

		if(tflush() == Success)
			goto DimSet;
		}
	else if(keyEntry.lastKeySeq == TermResizeKey)
DimSet:
		(void) rsset(Success, 0, text227, cols, rows);
			// "Terminal dimensions set to %hu x %hu"
	else {
		// Force full screen update.
		sess.opFlags |= OpScrnRedraw;

		(void) rsset(Success, RSNoFormat, text211);
			// "Screen refreshed"
		}

	return sess.rtn.status;
	}

// Re-open screen package -- at program startup or after a "suspend" operation.  Return status.
static int topen2(bool resetTTY) {

	(void) tflush();

	// Initialize terminal attributes.
	if(resetTTY && reset_prog_mode() == ERR)
		return sysCallError("topen2", "reset_prog_mode", false);
	sess.opFlags |= (OpVTermOpen | OpScrnRedraw);

	return sess.rtn.status;
	}

// Initialize screen package -- done once at startup.  Return status.
int topen(void) {
	static const char myName[] = "topen";
	ushort cols, rows;

	// Get terminal type.
	if((vTerm.termName = getenv("TERM")) == NULL)
		return rsset(FatalError, RSNoFormat, text182);
		// "Environmental variable 'TERM' not defined!"

	// Fix up file descriptors if reading a file from standard input.
	if(!isatty(0)) {
		int fileHandle;

		// File descriptor 0 is not a TTY so it must be a data file or pipe.  Get a new descriptor for it by calling
		// dup() (and saving the result in fileInfo.stdInpFileHandle), then open /dev/tty, and dup2() that FD back to 0.
		// This is done so that FD 0 is always the keyboard which should ensure that certain functions, such as
		// tgetent(), will work properly on any brain-dead Unix variants.
		if((fileInfo.stdInpFileHandle = dup(0)) == -1)
			return sysCallError(myName, "dup", false);
		if((fileHandle = open(ttyPath, O_RDONLY)) == -1)
			return sysCallError(myName, "open", false);
		if(dup2(fileHandle, 0) == -1)
			return sysCallError(myName, "dup2", false);
		}

	// Get terminal size and save it.
	if(getTermSize(&cols, &rows) != Success)
		return sess.rtn.status;
	setTermSize(cols, rows);

	// Initialize locale and screen.  *NOTE: For future use if and when multibyte support is added.  For now the default
	// "C" locale gives the best performance.
#if 0
	(void) setlocale(LC_ALL, "");
#endif
	(void) initscr();
	if(has_colors()) {
		chtype wideChar, freePairs;

		// Terminal supports color.  Check if enough color pairs are available for showColors display at minimum.
		// If not, do not enable color (OpHaveColor flag).
		(void) start_color();
		(void) use_default_colors();
		wideChar = A_COLOR;
		if(wideChar > 0)
			while((wideChar & 1) == 0)
				wideChar >>= 1;
		term.maxColor = COLORS - 1;
		freePairs = COLOR_PAIRS - 1;
		term.maxPair = (freePairs < wideChar) ? freePairs : wideChar;

		// Require a minimum of 7 colors (not including color #0, which is black) and 63 pairs.  However, there must be
		// enough pairs to display a minimum of 8 lines in the showColors display (which requires 4 pairs per line) plus
		// ReservedPairs more, which are reserved for the mode line.  Note that even though the ncurses man pages seem
		// to indicate that color number COLORS (for example, #8 or #256) is a valid color, testing has shown that it is
		// not -- weird things happen when it is used.
		if(term.maxColor >= 7 && term.maxPair >= 63 && (freePairs = term.maxPair - ReservedPairs) >= (8 * 4)) {
			struct Colors {
				short defaultColor;
				short desiredColor;
				short *termColor;
				} *pColors;
			static struct Colors colors[] = {
				{DefColorText, ColorText, &term.colorText},
				{DefColorMRI, ColorMRI, &term.colorMRI},
				{DefColorInfo, ColorInfo, &term.colorInfo},
				{-2, -2, NULL}};

			// We're good to use color.  Set up other color parameters.
			term.linesPerPage = (freePairs >= 60 * 4) ? 60 : (freePairs >= 32 * 4) ? 32 : 8;
			term.maxWorkPair = term.maxPair - ReservedPairs;
			pColors = colors;
			do {
				*pColors->termColor = (pColors->desiredColor <= term.maxColor) ? pColors->desiredColor :
				 pColors->defaultColor;
				} while((++pColors)->termColor != NULL);

			// Set default colors for informational displays.
			term.itemColors[ColorIdxInfo].colors[0] = term.colorText;
			term.itemColors[ColorIdxInfo].colors[1] = term.colorInfo;

			// Set mode line to color and initialize reserved pair for it if possible.  If only eight colors are
			// available, just use reverse video because the color choices are poor and COLOR_WHITE is hard to see.
			if(term.maxColor > 7 && ColorML <= term.maxColor) {
				term.itemColors[ColorIdxModeLn].colors[0] = term.colorText;
				term.itemColors[ColorIdxModeLn].colors[1] = ColorML;
				(void) init_pair(term.maxPair - ColorPairML, term.colorText, ColorML);
				}

			// Initialize reserved pair for macro recording indicator and set "color enabled" flag.
			term.itemColors[ColorIdxMRI].colors[0] = term.colorText;
			term.itemColors[ColorIdxMRI].colors[1] = term.colorMRI;
			(void) init_pair(term.maxPair - ColorPairMRI, term.colorText, term.colorMRI);
			sess.opFlags |= OpHaveColor;
			}
		}
	if((term.pMsgLineWind = newwin(1, cols, rows - 1, 0)) == NULL ||
	 raw() == ERR ||
	 noecho() == ERR ||
	 nonl() == ERR ||
	 keypad(stdscr, TRUE) == ERR ||
	 keypad(term.pMsgLineWind, TRUE) == ERR ||
	 set_escdelay(200) == ERR)
		return rsset(OSError, RSNoFormat, text421);
			// "initializing ncurses"

	// Check for minimum capabilities.
	if(tigetstr(CAP_CLEAR) == NULL || tigetstr(CAP_CUP) == NULL || tigetstr(CAP_EL) == NULL)
		return rsset(FatalError, 0, text192, vTerm.termName, Myself);
			// "This terminal (%s) does not have sufficient capabilities to run %s"

	// Set attribute flags.
	if(tigetstr(CAP_REV) != NULL)
		sess.opFlags |= OpHaveRev;
	if(tigetstr(CAP_BOLD) != NULL)
		sess.opFlags |= OpHaveBold;
	if(tigetstr(CAP_SMUL) != NULL)
		sess.opFlags |= OpHaveUL;

	// Initialize escape-sequence table, including 20 function keys, both shifted and unshifted.  Note that 64 slots are
	// reserved for function keys in ncurses.h but shifted ones are not documented.  So we will assume that the shifted ones
	// begin at slot 32.
	KeyTable *pKeyTable = keyList;
	do {
		keyTable[pKeyTable->keyCode - KEY_MIN] = pKeyTable->extKey;
		} while((++pKeyTable)->keyCode != -1);
	char *fn;
	rows = KEY_F0 - KEY_MIN;
	cols = 0;
	do {
		fn = "1234567890abcdefghij";
		do {
			keyTable[rows++] = cols | FKey | *fn++;;
			} while(*fn != '\0');
		rows = KEY_F0 - KEY_MIN + (KEY_DL - KEY_F0) / 2;
		cols ^= Shift;
		} while(cols);

#if MMDebug & Debug_NCurses
// delete_character	dch1		dc	delete character
// key_backspace	kbs		kb	backspace key
// key_dc		kdch1		kD	delete-character key
	fprintf(logfile, "topen():\n  tigetstr(dch1) = \"%s\"\n  tigetstr(kbs) = \"%s\"\n  tigetstr(kdch1) = \"%s\"\n",
	 tigetstr("dch1"), tigetstr("kbs"), tigetstr("kdch1"));
#endif

#if LINUX
	// Fix KEY_BACKSPACE entry if needed.
	char *kbs = tigetstr("kbs");
	if(kbs != NULL && strcmp(kbs, "\x7F") == 0)
		keyTable[KEY_BACKSPACE - KEY_MIN] = Ctrl | '?';
#endif
	// Finish up.
	return topen2(false);
	}

// Clean up the terminal in anticipation of a return to the operating system.  Close the keyboard, flush screen output, and
// restore the old terminal settings.  Return status.
int tclose(bool saveTTY) {
	static const char myName[] = "tclose";

	// Skip this if terminal not open.
	if(sess.opFlags & OpVTermOpen) {

		// Turn off curses.
		if(saveTTY && def_prog_mode() == ERR)
			return sysCallError(myName, "def_prog_mode", false);
		if(endwin() == ERR)
			return sysCallError(myName, "endwin", false);
		sess.opFlags &= ~OpVTermOpen;
		}
	return sess.rtn.status;
	}

// Move cursor.  Return status.
int tmove(int row, int col) {

	return move(row, col) == ERR ? sysCallError("tmove", "move", false) : sess.rtn.status;
	}

// Set a video attribute, given operation flag, attribute flag, and Boolean on/off flag.
static void setvattr(bool msgLine, ushort opFlag, int attrFlag, bool attrOn) {

	// Set reverse video state.
	if(sess.opFlags & opFlag) {
		WINDOW *pWind = msgLine ? term.pMsgLineWind : stdscr;
		(void) (attrOn ? wattron(pWind, attrFlag) : wattroff(pWind, attrFlag));
		}
	}

// Set reverse video state, given Boolean on/off flag.
void trev(bool msgLine, bool attrOn) {

	setvattr(msgLine, OpHaveRev, A_REVERSE, attrOn);
	}

// Set bold state, given Boolean on/off flag.
void tbold(bool msgLine, bool attrOn) {

	setvattr(msgLine, OpHaveBold, A_BOLD, attrOn);
	}

// Set underline state, given Boolean on/off flag.
void tul(bool msgLine, bool attrOn) {

	setvattr(msgLine, OpHaveUL, A_UNDERLINE, attrOn);
	}

// Sound the beeper.
void tbeep(void) {

	(void) beep();
	}

// Get current working directory (absolute pathname) and save it in directory table and given screen.  Return status.
int setWorkDir(EScreen *pScrn) {
	char *wkDir;
	DirPath *pNode;
	bool newDir;
	static const char myName[] = "setWorkDir";

	if((wkDir = getcwd(NULL, 0)) == NULL)
		return rsset(OSError, 0, text44, "getcwd", myName);
			// "calling %s() from %s() function"

	// Check if directory is already in table.
	for(pNode = sess.dirHead; pNode != NULL; pNode = pNode->next) {
		if(strcmp(wkDir, pNode->path) == 0) {
			newDir = false;
			goto Found;
			}
		}

	// Path not found... make new node.
	if((pNode = (DirPath *) malloc(sizeof(DirPath))) == NULL)
		return rsset(Panic, 0, text94, myName);
			// "%s(): Out of memory!"
	newDir = true;
	pNode->next = sess.dirHead;
	sess.dirHead = pNode;
	pNode->path = wkDir;
Found:
	if(!newDir)
		free((void *) wkDir);
	pScrn->workDir = pNode->path;

	return sess.rtn.status;
	}

// Change working directory to "dir" via system call.  Return status.
int chgdir(const char *dir) {

	return (chdir(dir) != 0) ? rsset(Failure, 0, text265, dir, strerror(errno)) : sess.rtn.status;
			// "Cannot change directory to \"%s\": %s"
	}

// Do chgDir command (change working directory).  Return status.
int chgWorkDir(Datum *pRtnVal, int n, Datum **args) {
	HookRec *pHookRec = hookTable + HkChDir;
	TermInpCtrl termInpCtrl = {NULL, RtnKey, MaxPathname, NULL};

	// Get directory name.
	if(sess.opFlags & OpScript)
		expandPath(pRtnVal, args[0]);
	else if(termInp(pRtnVal, text277, ArgNil1, Term_C_Filename, &termInpCtrl) != Success || pRtnVal->type == dat_nil)
			// "Change directory"
		return sess.rtn.status;

	// Make the change.
	if(chgdir(pRtnVal->str) != Success)
		return sess.rtn.status;

	// Get new path (absolute pathname), save it in current screen, and return it to caller.
	if(setWorkDir(sess.pCurScrn) != Success)
		return sess.rtn.status;
	if(dsetstr(sess.pCurScrn->workDir, pRtnVal) != 0)
		return librsset(Failure);

	// Set mode line update flag if needed.
	if(modeInfo.cache[MdIdxWkDir]->flags & MdEnabled)
		sess.pCurWind->flags |= WFMode;

	// Run change-directory user hook unless it is not set or is currently running.
	if(pHookRec->func.type != PtrNull && !pHookRec->running && execHook(NULL, n, pHookRec, 0) != Success)
		return sess.rtn.status;

	// Display new directory if 'WkDir' global mode not enabled.
	return (modeInfo.cache[MdIdxWkDir]->flags & MdEnabled) ? sess.rtn.status :
	 rsset(Success, RSNoFormat | RSNoWrap, sess.pCurScrn->workDir);
	}

// Suspend MightEMacs.
int suspendMM(Datum *pRtnVal, int n, Datum **args) {

	// Reset the terminal and exit ncurses.
	if(tclose(true) != Success)
		return sess.rtn.status;

	// Send stop signal to self (suspend)...
	if(kill(getpid(), SIGTSTP) == -1)
		return sysCallError("suspendMM", "kill", false);

	// We should be back here after resuming.

	// Reopen the screen and redraw.
	return topen2(true);
	}

// Sleep for given number of centiseconds.  "n" is assumed to be non-negative.
//
// struct timeval {
//	__darwin_time_t 	tv_sec; 	# seconds
//	__darwin_suseconds_t	tv_usec;	# and microseconds
//	};.
void centiPause(int n) {

	if(n > 0) {
		struct timeval t;

		// Set time record.
		t.tv_sec = n / 100;
		t.tv_usec = n % 100 * 10000;

		// Call select();
		(void) select(1, NULL, NULL, NULL, &t);
		}
	}

// Get time of day as a string.
char *strTime(void) {
	time_t pBuf;
	char *str1, *str2;

#if USG
	char *ctime();
#endif
	// Get system time.
	(void) time(&pBuf);

	// Pass system time to converter.
	str1 = ctime(&pBuf);

	// Eat newline character.
	if((str2 = strchr(str1, '\n')) != NULL)
		*str2 = '\0';

	return str1;
	}

#if USG
// Rename a file, given old and new.
int rename(const char *file1, const char *file2) {
	struct stat s1, s2;

	// No good if source file doesn't exist.
	if(stat(file1, &s1) != 0)
		return -1;

	// Check for target.
	if(stat(file2, &s2) == 0) {

		// See if file is the same.
		if(s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino)

			// Not necessary to rename file.
			return 0;
		}

	if(unlink(file2) != 0 ||	// Get rid of target.
	 link(file1, file2) != 0)	// Link two files together.
		return -1;

	// Unlink original file.
	return unlink(file1);
	}
#endif

// Get shell path.
static char *shellPath(void) {
	char *path;

	if((path = getenv("SHELL")) == NULL)
#if BSD
		path = "/bin/csh";
#else
		path = "/bin/sh";
#endif
	return path;
	}

// Create subshell.  Return status.
int shellCLI(Datum *pRtnVal, int n, Datum **args) {
	int rtnCode;
	char *sh = shellPath();

	if(tclose(true) != Success)			// Close down...
		return sess.rtn.status;
	rtnCode = system(sh);				// spawn a shell...
	dsetbool(rtnCode == 0, pRtnVal);		// set return value to false if error occurred; otherwise, true...
	if(rtnCode != 0)
		(void) rsset(Failure, 0, text194, sh);
			// "Shell command \"%s\" failed"
	return topen2(true);				// and restart system.
	}

// Get Unix command line.  Return status.
static int getCmd(Datum **ppCmdLine, const char *prompt) {

	if(dnewtrack(ppCmdLine) != 0)
		return librsset(Failure);
	if(sess.opFlags & OpScript) {

		// Concatenate all arguments into command line Datum object.
		if(catArgs(*ppCmdLine, 1, NULL, 0) == Success && disnull(*ppCmdLine))
			(void) rsset(Failure, 0, text187, text53);
				// "%s cannot be null", "Value"
		}
	else
		(void) termInp(*ppCmdLine, prompt, ArgNotNull1 | ArgNil1, 0, NULL);

	return sess.rtn.status;
	}

// Read data from open file descriptor, insert into given buffer, and close file.  Return status.
static int insertPipeData(int fileHandle, int n, Buffer *pBuf, DataInsert *pDataInsert, ushort flags) {

	if(inpInit(fileHandle) == Success) {
		pDataInsert->pTargBuf = pBuf;
		pDataInsert->pTargPoint = (pBuf == sess.pCurBuf) ? NULL : &pBuf->face.point;
		pDataInsert->msg = (flags & PipeInsert) ? text153 : text139;
					// "Inserting data...", "Reading data..."

		// Enable non-blocking reads and timeout retries.
		if(fcntl(fileHandle, F_SETFL, O_NONBLOCK) == -1)
			return sysCallError("insertPipeData", "fcntl", false);
		fileInfo.flags |= FIRetry;
		fileInfo.filename = pipeFilename;

		// Read the data.
		(void) insertData(n, NULL, pDataInsert);
		}
	return sess.rtn.status;
	}

// Save results of command execution in a temporary buffer.  If pCmdLine is NULL, skip header line.  Return status.
static int resultBuf(Buffer **ppBuf, Datum *pCmdLine, int *pipe3, int *pipe2) {
	ushort oldFlags;
	short oldStatus;
	DataInsert dataInsert;

	// Save message flag state and disable it.
	oldFlags = modeInfo.cache[MdIdxRtnMsg]->flags & MdEnabled;
	clearGlobalMode(modeInfo.cache[MdIdxRtnMsg]);

	// Get a temporary buffer.
	if(sysBuf(text424, ppBuf, 0) != Success)
			// "ShellCmd"
		return sess.rtn.status;

	// Write command.
	if(pCmdLine != NULL) {
		char workBuf[strlen(pCmdLine->str) + 4];

		sprintf(workBuf, "%c %s", getuid() == 0 ? '#' : '$', pCmdLine->str);
		if(insertLine(workBuf, strlen(workBuf), true, *ppBuf, &(*ppBuf)->face.point) != Success)
			goto ERtn;
		}

	// Read pipe3 (standard error) into buffer.
	if(pipe3 != NULL && insertPipeData(pipe3[0], 1, *ppBuf, &dataInsert, PipeInsert) != Success)
		goto ERtn;

	// Read pipe2 (standard output) into buffer.
	if(pipe2 != NULL && insertPipeData(pipe2[0], 1, *ppBuf, &dataInsert, PipeInsert) != Success)
		goto ERtn;

	// Unchange buffer and restore message flag.
	(*ppBuf)->flags &= ~BFChanged;
	if(oldFlags)
		setGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
	goto Retn;
ERtn:
	// Save and restore status (so that bdelete() will not fail).
	rspush(&oldStatus, &oldFlags);
	(void) bdelete(*ppBuf, BC_IgnChgd);
	rspop(oldStatus, oldFlags);
Retn:
	return sess.rtn.status;
	}

// Get count of pending input characters, given file descriptor.  Return status.
static int getPend(int fileHandle, int *pCount) {

	// Get number of pending characters using either a FIONREAD or FIORDCHK call, depending on underlying Unix system.
#ifdef FIONREAD
	if(ioctl(fileHandle, FIONREAD, pCount) == -1)
#else
	if((*pCount = ioctl(fileHandle, FIORDCHK, 0)) < 0)
#endif
		return sysCallError("getPend", FNioctl, false);
	return sess.rtn.status;
	}

// Get a shell command, fork a child process to execute it, and capture results.  Return status.  Routine is called for
// insertPipe, pipeBuf, readPipe, and shellCmd commands.
int pipeCmd(Datum *pRtnVal, int n, const char *prompt, ushort flags) {
	const char *filename;
	Buffer *pResultBuf;
	Datum *pCmdLine;
	int count, pipe1[2], pipe2[2], pipe3[2];
	int status;
	pid_t pid;
	ushort action;
	ushort rendFlags = RendNewBuf | RendAltML | RendWait;
	static Option optTable[] = {
		{"^Shift", "^Shift", 0, 0},
		{"No^Pop", NULL, 0, 0},
		{"No^Hdr", NULL, 0, 0},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		ArgFirst, text410, false, optTable};
				// "command option"
	static const char myName[] = "pipeCmd";

	dsetbool(false, pRtnVal);

	// Get options if shellCmd.
	if(flags & PipePopOnly) {
#if MMDebug & Debug_PipeCmd
		fputs("\npipeCmd(): executing shellCmd...\n", logfile);
#endif
		if(n == INT_MIN)
			deselectOpts(optTable);
		else if(parseOpts(&optHdr, text448, NULL, NULL) != Success ||
					// "Options"
		 ((sess.opFlags & OpScript) && !needSym(s_comma, true)))
			return sess.rtn.status;
		}

	// Get shell command.  Cancel operation if nothing entered.
	if(getCmd(&pCmdLine, prompt) != Success || pCmdLine->type == dat_nil)
		return sess.rtn.status;

	// If target buffer is current buffer and executing a pipeBuf or readPipe command, verify it can be erased (getting user
	// okay if necessary) before proceeding.
	if(!(flags & (PipePopOnly | PipeInsert)) && (n == INT_MIN || n == 1) && bconfirm(sess.pCurBuf, 0) != Success)
		return sess.rtn.status;

	// Target buffer ready.  Create three pipes: one for writing target buffer into (pipeBuf command), one for reading
	// command output from, and one for reading standard error output from.  If PipeWrite flag not set, pipe1 is closed by
	// parent process so that child gets EOF immediately if shell command reads from standard input.
	if(pipe(pipe1) != 0 || pipe(pipe2) != 0 || pipe(pipe3) != 0) {
		filename = "pipe";
		goto OSErr;
		}

	// Fork a child process to run shell command.
#if MMDebug & Debug_PipeCmd
	fputs("forking child process...\n", logfile);
#endif
	switch(fork()) {
		case 0:
			 // Child process.  Connect standard input to pipe1.
#if MMDebug & Debug_PipeCmd
			fprintf(logfile, "Child process is PID %d\nChild: connecting standard input to pipe1...\n", getpid());
#endif
			if(dup2(pipe1[0], 0) < 0)
				goto DupErr;
			(void) close(pipe1[1]);

			// Connect standard output to pipe2 and standard error to pipe3.
#if MMDebug & Debug_PipeCmd
			fputs("Child: connecting standard output to pipe2 and standard error to pipe3...\n", logfile);
#endif
			if(dup2(pipe2[1], 1) < 0 || dup2(pipe3[1], 2) < 0) {
DupErr:
				filename = "dup2";
				goto OSErr;
				}
			(void) close(pipe2[0]);
			(void) close(pipe3[0]);

			// Pipes are ready... execute a shell with command as "-c" argument.
			char *cmd[4], **pCmd = cmd;

			*pCmd++ = shellPath();
			*pCmd++ = "-c";
			*pCmd++ = pCmdLine->str;
			*pCmd = NULL;
#if MMDebug & Debug_PipeCmd
			fprintf(logfile, "Child: calling execvp() with command '%s'...\n", pCmdLine->str);
#endif
			execvp(cmd[0], cmd);

			// Shouldn't get here... execvp() failed.
			filename = "execvp";
			goto OSErr;
		case -1:
			filename = FNfork;
			goto OSErr;
		default:
			// Parent process.
#if MMDebug & Debug_PipeCmd
			fprintf(logfile, "Parent process is PID %d\n", getpid());
#endif
			break;
		}

	// Close pipe ends not needed for parent or second child (below).
	(void) close(pipe1[0]);
	(void) close(pipe2[1]);
	(void) close(pipe3[1]);

	// If pipeBuf command, fork another process to write current buffer to pipe 1 and close it.
	if(flags & PipeWrite) {
		switch(fork()) {
			case 0:
				// Child process.  Write current buffer to pipe 1 and return success or failure (to parent).
				(void) close(pipe2[0]);
				(void) close(pipe3[0]);
				otpInit(sess.pCurBuf, pipe1[1]);
				fileInfo.filename = pipeFilename;
				fileInfo.flags |= FIRetry;
				exit(writeDiskPipe(sess.pCurBuf, NULL) == Success ? 0 : -1);
			case -1:
				filename = FNfork;
				goto OSErr;
			default:
				// Parent process.
				(void) close(pipe1[1]);
				break;
			}
		}

	// Check status of child/children.  To accomplish this, we could wait for say, the first child to terminate and then
	// check its return code; however, that won't work for two reasons: (1), the child may block on output because the
	// amount of data to write is large (and we're not reading from the other end of the pipe, which would create a deadlock
	// between parent and child); and (2), certain commands will return non-zero for a "successful" execution (for example,
	// the "grep" command when nothing matches) so the return code is unreliable.  Another option is to assume that the
	// first child executing the command will write something to either its standard output or standard error (possibly
	// both) eventually, depending on whether it encountered an error or not.  However, that's not true in every case -- a
	// "mv" command or "grep" command where nothing matches will generate no output at all.  So to deal with this dilemma,
	// we perform three actions in a loop until one of them succeeds: (a), check if data is pending on pipe2; (b), check if
	// data is pending on pipe3; (c), check if any child terminates.  If a child terminates, we will check both pipes once
	// more to make sure there is no data, because we want to read it if so.  We will also insert a short pause between
	// actions to smooth things out a bit.
#if MMDebug & Debug_PipeCmd
	fputs("Entering child-status-check loop...\n", logfile);
#endif
	pid = action = 0;
	for(;;) {
		if(action < 2) {
			// Check if any bytes pending.
			if(getPend((action & 1) ? pipe3[0] : pipe2[0], &count) != Success) {
				filename = "getPend";
				goto OSErr;
				}

			// Have data?
			if(count > 0) {		// Yes, done.
#if MMDebug & Debug_PipeCmd
				fprintf(logfile, "found data (%d bytes) for action %hu, exiting loop.\n", count, action);
#endif
				break;
				}
			}
		else {
			// Check if a child has terminated.
			if(pid > 0) {
#if MMDebug & Debug_PipeCmd
				fprintf(logfile, "PID %d terminated for action %hu, exiting loop.\n", pid, action);
#endif
				break;
				}
			if((pid = waitpid(-1, &status, WNOHANG)) < 0) {
				filename = "waitpid";
				goto OSErr;
				}
			}
		centiPause(PipePause);		// Pause and try again.
		action = (action + 1) % 3;
		}

	// Check results.  Treat child termination (with no data) as success.
	switch(action) {
		case 0:
		case 2:
			// A child terminated or have pipe2 data... command execution SUCCEEDED.
			if(flags & PipePopOnly) {

				// shellCmd: build result buffer if requested, pop it, and delete it.
				if(optTable[1].ctrlFlags & OptSelected) {
#if MMDebug & Debug_PipeCmd
					fputs("shellCmd: command succeeded, skipping pop-up per options.\n", logfile);
#endif
					n = true;
					goto SetResult;
					}
#if MMDebug & Debug_PipeCmd
				fputs("shellCmd: command succeeded, calling resultBuf()...\n", logfile);
#endif
				if(resultBuf(&pResultBuf, optTable[2].ctrlFlags & OptSelected ? NULL : pCmdLine, pipe3,
				 pipe2) != Success) {
#if MMDebug & Debug_PipeCmd
					fprintf(logfile, "  resultBuf() FAILED: returned %d '%s'\n",
					 sess.rtn.status, sess.rtn.msg.str);
#endif
					goto CloseWait;
				 	}
#if MMDebug & Debug_PipeCmd
				fputs("  resultBuf() succeeded, popping buffer.\n", logfile);
#endif
				if(optTable[0].ctrlFlags & OptSelected)
					rendFlags |= RendShift;
				n = true;
				goto PopBuf;
				}

			// pipeBuf, readPipe, or insertPipe: read or insert data from pipe2 into a buffer.
			Buffer *pBuf;
			DFab fab;
			DataInsert dataInsert;

			if((flags & PipeInsert) || (!(flags & PipeWrite) && (n == INT_MIN || n == 1))) {
				pBuf = sess.pCurBuf;
				if(!(flags & PipeInsert) && readPrep(pBuf, BC_IgnChgd) != Success)
					goto CloseWait;
				}
			else if(bscratch(&pBuf) != Success)
				goto CloseWait;
			if(insertPipeData(pipe2[0], n, pBuf, &dataInsert, flags) == Success) {
				if(dopentrack(&fab) != 0)
					goto LibFail;
				if(ioStat(&fab, dataInsert.finalDelim ? 0 : IOS_NoDelim, NULL, Success, NULL,
				 flags & PipeInsert ? text154 : text140, dataInsert.lineCt) == Success) {
						// "Inserted", "Read"
					if(!(flags & PipeInsert)) {
						pBuf->flags &= ~BFChanged;

						// If pipeBuf command and results were destined for current buffer, swap
						// contents of scratch buffer and current buffer and delete the scratch.
						if((flags & PipeWrite) && (n == INT_MIN || n == 1)) {
							Line *pLine = pBuf->pFirstLine;
							pBuf->pFirstLine = sess.pCurBuf->pFirstLine;
							sess.pCurBuf->pFirstLine = pLine;
							(void) bdelete(pBuf, BC_IgnChgd);		// Can't fail.
							pBuf = sess.pCurBuf;
							pBuf->flags &= ~BFChanged;
							supd_windFlags(pBuf, WFHard | WFMode);
							}
						faceInit(pBuf == sess.pCurBuf ? &sess.pCurWind->face :
						 &pBuf->face, pBuf->pFirstLine, pBuf);
						(void) render(pRtnVal, n == INT_MIN ? 1 : n, pBuf, n == INT_MIN ? 0 :
						 RendNewBuf | RendNotify);
						}
				 	}
				}
			break;
		default:
			// Have pipe3 data -- command execution FAILED.  Create result buffer and pop it for user.
			if(resultBuf(&pResultBuf, pCmdLine, pipe3, pipe2) != Success)
				goto CloseWait;
			if(!(flags & PipePopOnly)) {
				DFab alert;

				if(dopenwith(&alert, pRtnVal, FabClear) != 0 || dputf(&alert, text194, pCmdLine->str) != 0 ||
										// "Shell command \"%s\" failed"
				 dclose(&alert, Fab_String) != 0)
					goto LibFail;
				if(mlputs(MLHome | MLFlush, pRtnVal->str) != Success)
					goto CloseWait;
				centiPause(160);
				}
			n = false;
PopBuf:
			if(render(pRtnVal, -1, pResultBuf, rendFlags) == Success)
SetResult:
				dsetbool(n, pRtnVal);
		}
CloseWait:
	// Close read ends of pipes 2 and 3 (in case an error occurred -- no harm otherwise) and wait for children to terminate.
	(void) close(pipe2[0]);
	(void) close(pipe3[0]);
	while(wait(&status) != -1)
		;

	return sess.rtn.status;
LibFail:
	return librsset(Failure);
OSErr:
	return rsset(OSError, 0, text44, filename, myName);
		// "calling %s() from %s() function"
	}

// Copy pathname in pSrc to pDest, expanding any "~/" or "~user/" at beginning to user's HOME directory and any embedded
// environmental variables in form "$var" or "${var}".  If pSrc is NULL, string in pDest is expanded in place.  Return status.
int expandPath(Datum *pDest, Datum *pSrc) {
	char *str1, *str, *name;
	DFab fab;
	bool braceForm;
	short c1, c2;

	// Get ready.
	if(pSrc == NULL) {
		str = pDest->str;
		if(dopentrack(&fab) != 0)
			goto LibFail;
		}
	else {
		str = pSrc->str;
		if(dopenwith(&fab, pDest, FabClear) != 0)
			goto LibFail;
		}

	// Loop through string and do expansions.
	str1 = str + 1;
	while((c1 = *str++) != '\0') {
		switch(c1) {
			case '~':
				// Expand "~/" or "~user/" at beginning of string (only) to home directory.
				if(str != str1)
					break;
				if(*str == '/') {
					if((name = getenv("HOME")) != NULL && dputs(name, &fab) != 0)
						goto LibFail;
					continue;
					}
				// Fall through.
			case '$':
				// Check for "~user/", "$var" or "${var}".
				if(c1 == '$' && *str == '{') {
					braceForm = true;
					++str;
					}
				else
					braceForm = false;

				// Verify that first character of variable name or username is a letter or underscore.  If not,
				// pass string literally.
				if(!is_letter(c2 = *str) && c2 != '_') {
					if(braceForm)
						--str;
					break;
					}

				// So far so good.  Scan to end of name.
				str = strpspn(name = str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
				if(braceForm && *str != '}') {
					str = name - 1;
					break;
					}
				if(c1 == '~' && (*str != '/' && *str != '\0')) {
					str = name;
					break;
					}

				// Valid name... add null byte.
				c2 = *str;
				*str = '\0';
				if(c1 == '~') {
					// Have "~user".  Get the home directory.
					struct passwd *pPass = getpwnam(name);
					if(pPass == NULL) {
						// No go.  Reset controls and pass characters literally.
						*str = c2;
						str = name;
						break;
						}

					// Found it.
					if(dputs(pPass->pw_dir, &fab) != 0)
						goto LibFail;
					*str = c2;
					continue;
					}

				// Have "$var" or "${var}".
				if((name = getenv(name)) != NULL && dputs(name, &fab) != 0)
					goto LibFail;
				*str = c2;
				if(braceForm)
					++str;
				continue;
			}

		// Not a special character... just copy it.
		if(dputc(c1, &fab) != 0)
			goto LibFail;
		}

	// Expansion completed... return result.
	if(dclose(&fab, Fab_String) != 0)
		goto LibFail;
	if(pSrc == NULL)
		datxfer(pDest, fab.pDatum);
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Return pointer to base filename, given pathname or filename.  If withExt if false, return base filename without extension.
// "name" is not modified in either case.
char *fbasename(const char *name, bool withExt) {
	char *str1, *str2;
	static char pBuf[MaxFilename + 1];

	if((str1 = str2 = strchr(name, '\0')) > name) {

		// Find rightmost slash, if any.
		while(str1 > name) {
			if(str1[-1] == '/')
				break;
			--str1;
			}

		// Find and eliminate extension, if requested.
		if(!withExt) {
			uint len;

			while(str2 > str1) {
				if(*--str2 == '.') {
					if(str2 == str1)		// Bail out if '.' is first character.
						break;
					len = str2 - str1 + 1;
					stplcpy(pBuf, str1, len > sizeof(pBuf) ? sizeof(pBuf) : len);
					return pBuf;
					}
				}
			}
		}

	return str1;
	}

// Return pointer to directory name, given pathname or filename and n argument.  If non-default n, return "." if no directory
// portion found in name; otherwise, null.  Given name is modified (truncated).
char *fdirname(char *name, int n) {
	char *str;

	str = fbasename(name, true);
	if(*name == '/' && (*str == '\0' || str == name + 1))
		name[1] = '\0';
	else if(strchr(name, '/') == NULL) {
		if(*name != '\0') {
			if(n == INT_MIN)
				name[0] = '\0';
			else
				strcpy(name, ".");
			}
		}
	else
		str[-1] = '\0';
	return name;
	}

// Save pathname on heap (in path) and set *pDest to it.  Return status.
static int savePath(char **pDest, const char *name) {
	static char *path = NULL;

	if(path != NULL)
		free((void *) path);
	if((path = (char *) malloc(strlen(name) + 1)) == NULL)
		return rsset(Panic, 0, text94, "savePath");
			// "%s(): Out of memory!"
	strcpy(*pDest = path, name);
	return sess.rtn.status;
	}

// Find script file(s) in the HOME directory or in the $execPath directories per flags, and return status.  If name begins with
// a slash (/), it is either searched for verbatim (XP_GlobPat flag not set) or the search immediately fails (XP_GlobPat flag
// set); otherwise, if XP_HomeOnly flag is set, it is searched for in the HOME directory only; otherwise, it is searched for in
// every directory in $execPath (skipping null directories if XP_SkipNull flag set) either as a glob pattern (XP_GlobPat flag
// set) or a plain filename (XP_GlobPat flag not set).  Call details are as follows:
//
// If XP_GlobPat flag is set:
//	- result must be of type glob_t * and point to a glob_t structure provided by the caller.
//	- matched pathnames are returned in the gl_pathv member, as described in the glob() man page.  Flag GLOB_MARK is set.
//	- member gl_pathc will be zero if no matches were found.
//	- caller must call globfree() after processing pathnames if gl_pathc > 0.
// If XP_GlobPat flag is not set:
//	- result must be of type char **.
//	- *result is set to the absolute pathname if a match was found; otherwise, NULL.
//	- searches are for the original filename first, followed by <filename>ScriptExt unless the filename already has that
//	  extension.
// In all cases, if XP_SkipNull flag is set, null directories in $execPath are skipped.
int pathSearch(const char *name, ushort flags, void *result, const char *funcName) {
	char *str1, *str2;
	Datum *pExtName;			// Name with extension, if applicable.
	glob_t *pGlob;
	char **pPath;
	size_t maxLen = strlen(name);
	static char *scriptExt = ScriptExt;
	size_t scriptExtLen = strlen(scriptExt);

	// Get ready.
	if(flags & XP_GlobPat) {
		pGlob = (glob_t *) result;
		pGlob->gl_pathc = 0;
		}
	else {
		pPath = (char **) result;
		*pPath = NULL;
		}

	// Null filename?
 	if(maxLen == 0)
 		return sess.rtn.status;

 	// Create filename-with-extension version.
 	if(dnewtrack(&pExtName) != 0)
 		goto LibFail;
	if(!(flags & XP_GlobPat) && ((str1 = strchr(fbasename(name, true), '.')) == NULL || strcmp(str1, scriptExt) != 0)) {
		maxLen += scriptExtLen;
		if(dsalloc(pExtName, maxLen + 1) != 0)
			goto LibFail;
		sprintf(pExtName->str, "%s%s", name, scriptExt);
		}
	else
		dsetnull(pExtName);

	// If the path begins with '/', check only that.
	if(*name == '/') {
		if(!(flags & XP_GlobPat))
			return fileExist(name) == 0 ? savePath(pPath, name) : (!disnull(pExtName) &&
			 fileExist(pExtName->str) == 0) ? savePath(pPath, pExtName->str) : sess.rtn.status;
		return rsset(Failure, 0, text407, funcName);
			// "Invalid argument for %s glob search"
		}

	// Check HOME directory (only), if requested.
	if(flags & XP_HomeOnly) {
		if((str1 = getenv("HOME")) != NULL) {
			char pathBuf[strlen(str1) + maxLen + 2];

			// Build pathname and check it.
			sprintf(pathBuf, "%s/%s", str1, name);
			if(fileExist(pathBuf) == 0)
				return savePath(pPath, pathBuf);
			}
		return sess.rtn.status;
		}

	// All special cases checked.  Create name list (filenames to check) and scan the execPath directories.
	const char *nameList[] = {
		name, pExtName->str, NULL};
	for(str2 = execPath; *str2 != '\0';) {
		char *dirSepTok;

		// Build next possible file spec or glob pattern.
		str1 = str2;
		while(*str2 != '\0' && *str2 != ':')
			++str2;

		// Skip null directory if requested.
		if(!(flags & XP_SkipNull) || str2 > str1) {

			// Add a terminating dir separator if needed.
			dirSepTok = "";
			if(str2 > str1 && str2[-1] != '/')
				dirSepTok = "/";

			// Loop through filename(s).
			int r;
			const char **ppName = nameList;
			char pathBuf[str2 - str1 + maxLen + 2];
			do {
				if((*ppName)[0] != '\0') {
					sprintf(pathBuf, "%.*s%s%s", (int) (str2 - str1), str1, dirSepTok, *ppName);
					if(flags & XP_GlobPat) {
						if((r = glob(pathBuf, GLOB_MARK, NULL, pGlob)) != 0 && r != GLOB_NOMATCH) {
							return rsset(errno == GLOB_NOSPACE ? FatalError : Failure, 0, text406,
											// "Glob of pattern \"%s\" failed: %s"
							 pathBuf, strerror(errno));
							}
						if(pGlob->gl_pathc > 0)
							return sess.rtn.status;
						}
					else if(fileExist(pathBuf) == 0)
						return savePath(pPath, pathBuf);
					}
				} while(*++ppName != NULL);
			}

		// Not found.  Onward...
		if(*str2 == ':')
			++str2;
		}

	// No such luck.
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Get absolute pathname of "filename" (which may be modified) and return in "pPath".  Resolve it if it's a symbolic link and
// "resolve" is true.  Return status.
int getPath(char *filename, bool resolve, Datum *pPath) {
	char pBuf[MaxPathname + 1];

	if(!resolve) {
		struct stat s;

		if(lstat(filename, &s) != 0)
			goto ErrRetn;
		if((s.st_mode & S_IFMT) != S_IFLNK)
			goto RegFile;

		// File is a symbolic link.  Get pathname of parent directory and append filename.
		DFab fab;
		char *str = fbasename(filename, true);
		char base[strlen(str) + 1];
		strcpy(base, str);
		if(dopenwith(&fab, pPath, FabClear) != 0)
			goto LibFail;
		if(realpath(filename = fdirname(filename, 1), pBuf) == NULL)
			goto ErrRetn;
		if(dputs(pBuf, &fab) != 0 || (strcmp(pBuf, "/") != 0 && dputc('/', &fab) != 0) ||
		 dputs(base, &fab) != 0 || dclose(&fab, Fab_String) != 0)
			goto LibFail;
		}
	else {
RegFile:
		if(realpath(filename, pBuf) == NULL)
ErrRetn:
			return rsset(errno == ENOMEM ? FatalError : Failure, 0, text33, text37, filename, strerror(errno));
				// "Cannot get %s of file \"%s\": %s", "pathname"
		if(dsetstr(pBuf, pPath) != 0)
LibFail:
			(void) librsset(Failure);
		}

	return sess.rtn.status;
	}

// Open directory for filename retrieval, given complete or partial pathname (which may end with a slash).  Initialize
// *pFilePath to static pointer to pathnames to be returned by ereaddir().  Return status.
int eopendir(const char *fileSpec, char **pFilePath) {
	char *fn, *term;
	size_t prefLen;

	// Find directory prefix.  Terminate after slash if Unix root directory; otherwise, at rightmost slash.
	term = ((fn = fbasename(fileSpec, true)) > fileSpec && fn - fileSpec > 1) ? fn - 1 : fn;

	// Get space for directory name plus maximum filename.
	if(filePath != NULL)
		free((void *) filePath);
	if((filePath = (char *) malloc((prefLen = (term - fileSpec)) + MaxFilename + 3)) == NULL)
		return rsset(Panic, 0, text94, "eopendir");
			// "%s(): Out of memory!"
	stplcpy(*pFilePath = filePath, fileSpec, prefLen + 1);

	// Open the directory.
	if(dir != NULL) {
		closedir(dir);
		dir = NULL;
		}
	if((dir = opendir(*filePath == '\0' ? "." : filePath)) == NULL)
		return rsset(Failure, 0, text88, filePath, strerror(errno));
			// "Cannot read directory \"%s\": %s"

	// Set filePathEnd, restore trailing slash in pathname if applicable, and return.
	if(fn > fileSpec)
		*((filePathEnd = filePath + (fn - fileSpec)) - 1) = '/';
	else
		filePathEnd = filePath;

	return sess.rtn.status;
	}

// Get next filename from directory opened with eopendir().  Return NotFound if none left.
//
// The errors below are ignored in the stat() call (assumed to be from invalid symbolic links):
//	ENOENT		The named file does not exist.
//	ENOTDIR		A component of the path prefix is not a directory.
int ereaddir(void) {
	struct DIRENTRY *pDirEntry;
	struct stat s;

	// Call for the next file.
	do {
		errno = 0;
		if((pDirEntry = readdir(dir)) == NULL) {
			if(errno == 0) {
				closedir(dir);
				free((void *) filePath);
				dir = NULL;
				filePathEnd = filePath = NULL;
				return NotFound;	// No entries left.
				}
			*filePathEnd = '\0';
			return rsset(Failure, 0, text88, filePath, strerror(errno));
				// "Cannot read directory \"%s\": %s"
			}
		strcpy(filePathEnd, pDirEntry->d_name);
		if(stat(filePath, &s) != 0) {
			if(errno == ENOENT || errno == ENOTDIR)		// Ignore these errors.
				s.st_mode = 0;
			else
				return rsset(Failure, 0, text33, text163, filePath, strerror(errno));
						// "Cannot get %s of file \"%s\": %s", "status"
			}

		// Skip all entries except regular files and directories.
		} while(((s.st_mode & S_IFMT) & (S_IFREG | S_IFDIR)) == 0);

	// If this entry is a directory name, say so.
	if((s.st_mode & S_IFMT) == S_IFDIR)
		strcat(filePathEnd, "/");

	// Return result.
	return sess.rtn.status;
	}
