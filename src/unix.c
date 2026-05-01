// (c) Copyright 2026 Richard W. Marinelli
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
#define CAP_EL		"el"			// Clear to end of line.
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
	ExtKey extKey;
	} ExtKeyEntry;

// Key codes generated on CentOS Linux platforms depend on TERM setting, as follows:
//			-------------------- TERM --------------------
//	KEY		vt220		xterm-color	xterm-256color
//	delete		0x7f		0x7f		KEY_BACKSPACE
//	C-h		KEY_BACKSPACE	KEY_BACKSPACE	0x08
// tigetstr(dch1)	^[[P		^[[P		^[[P
// tigetstr(kbs)	^H		^H		^?
// tigetstr(kdch1)	NULL		^[[3~		^[[3~
//
// Key codes generated on Ubuntu Linux platforms depend on TERM setting, as follows:
//			-------------------- TERM --------------------
//	KEY		vt220		xterm-color	xterm-256color
//	delete		0x7f		KEY_BACKSPACE	KEY_BACKSPACE
//	C-h		KEY_BACKSPACE	0x08		0x08
// tigetstr(dch1)	^[[P		^[[P		^[[P
// tigetstr(kbs)	^H		^?		^?
// tigetstr(kdch1)	^[[3~		^[[3~		^[[3~
//
// Key codes generated on macOS platforms do NOT depend on TERM setting:
//			---------------------------- TERM ----------------------------
//	KEY		vt100		vt220		xterm-color	xterm-256color
//	delete		0x7f		0x7f		0x7f		0x7f
//	C-h		KEY_BACKSPACE	KEY_BACKSPACE	KEY_BACKSPACE	KEY_BACKSPACE
// tigetstr(dch1)	NULL		^[[P		^[[P		^[[P
// tigetstr(kbs)	^H		^H		^H		^H
// tigetstr(kdch1)	NULL		NULL		^[[3~		^[[3~
//
// Given the above, it is sufficient to check the value returned from tigetstr("kbs") at startup.  If "\0x08" is returned, we
// can assume that the delete key will be returned as 0x7f from the getch() call, and control+h will be returned as
// KEY_BACKSPACE.  Likewise, if tigetstr("kbs") returns "\0x7f", the delete key will be KEY_BACKSPACE and control+h will be
// 0x08.  Hence, we can set the extKeyTable table accordingly and get consistent extended key values for those two keys.

// Table of special ncurses key codes returned from getch() that we care about; that is, are useful and should be acted upon in
// some way; for example, the arrow keys and numbered function keys.  These are loaded into extKeyTable at startup, which is
// subsequently used to translate the key codes obtained from ncurses at runtime to extended keys used by the editor.  Keys that
// are unlikely to appear on any modern keyboard are set to '?' and ignored.  (It is unlikely they could ever be generated from
// a modern keyboard, but if so, they will be no-ops at runtime.)  All unique function key symbols (the 'x' in "FKey | 'x'")
// used in this table are also listed in the SpecFKeySyms macro string, which is defined in bind.h, so that adequate space can
// be reserved in the binding table for all function keys we are supporting.  If you change this table, be sure to update the
// macro also.
//
// All the lines in the table below were taken from the ncurses.h header for macOS (Free Software Foundation ver. 5.7) and
// modified to add the desired extended key for each key code (which was moved to the beginning of the comment at right).
// Comments were kept intact for the most part.
static ExtKeyEntry keyList[] = {
#if 0
// A few key codes do not depend upon the terminfo entry.
#define KEY_CODE_YES	0400			// A wchar_t contains a key code.
#define KEY_MIN		0401			// Minimum curses key.
#define KEY_BREAK	0401			// Break key (unreliable).
#define KEY_SRESET	0530			// Soft (partial) reset (unreliable).
#define KEY_RESET	0531			// Reset or hard reset (unreliable).
#endif
	{KEY_DOWN, FKey | 'N'},			// 0402 - Down-arrow key.
	{KEY_UP, FKey | 'P'},			// 0403 - Up-arrow key.
	{KEY_LEFT, FKey | 'B'},			// 0404 - Left-arrow key.
	{KEY_RIGHT, FKey | 'F'},		// 0405 - Right-arrow key.
	{KEY_HOME, FKey | 'H'},			// 0406 - Home key (fn-left-arrow).
	{KEY_BACKSPACE, Ctrl | 'H'},		// 0407 - Backspace key.
#if 0
#define KEY_F0		0410			// Function keys.  Space for 64.
#define KEY_F(n)	(KEY_F0+(n))		// Value of function key n.
#endif						// fn-delete -> S-FNe, option-right-arrow -> ESC f, option-left-arrow -> ESC b
//	{KEY_DL, FKey | '?'},			// 0510 - Delete-line key.
//	{KEY_IL, FKey | '?'},			// 0511 - Insert-line key.
	{KEY_DC, FKey | 'D'},			// 0512 - Delete-character key (fn-delete).
//	{KEY_IC, FKey | '?'},			// 0513 - Insert-character key.
//	{KEY_EIC, FKey | '?'},			// 0514 - Sent by rmir or smir in insert mode.
	{KEY_CLEAR, FKey | 'C'},		// 0515 - Clear-screen or erase key.
//	{KEY_EOS, FKey | '?'},			// 0516 - Clear-to-end-of-screen key.
//	{KEY_EOL, FKey | '?'},			// 0517 - Clear-to-end-of-line key.
//	{KEY_SF, FKey | '?'},			// 0520 - Scroll-forward key.
//	{KEY_SR, FKey | '?'},			// 0521 - Scroll-backward key.
	{KEY_NPAGE, FKey | 'V'},		// 0522 - Next-page key (fn-down-arrow).
	{KEY_PPAGE, FKey | 'Z'},		// 0523 - Previous-page key (fn-up-arrow).
//	{KEY_STAB, FKey | '?'},			// 0524 - Set-tab key.
//	{KEY_CTAB, FKey | '?'},			// 0525 - Clear-tab key.
//	{KEY_CATAB, FKey | '?'},		// 0526 - Clear-all-tabs key.
	{KEY_ENTER, FKey | 'S'},		// 0527 - Enter/send key (fn-return).
	/* Codes 0530 and 0531 are defined above (as unreliable). */
//	{KEY_PRINT, FKey | '?'},		// 0532 - Print key.
//	{KEY_LL, FKey | '?'},			// 0533 - Lower-left key (home down).
//	{KEY_A1, FKey | '?'},			// 0534 - Upper left of keypad.
//	{KEY_A3, FKey | '?'},			// 0535 - Upper right of keypad.
//	{KEY_B2, FKey | '?'},			// 0536 - Center of keypad.
//	{KEY_C1, FKey | '?'},			// 0537 - Lower left of keypad.
//	{KEY_C3, FKey | '?'},			// 0540 - Lower right of keypad.
	{KEY_BTAB, Shift | Ctrl | 'I'},		// 0541 - Back-tab key (shift-tab).
//	{KEY_BEG, FKey | '?'},			// 0542 - Begin key.
//	{KEY_CANCEL, FKey | '?'},		// 0543 - Cancel key.
//	{KEY_CLOSE, FKey | '?'},		// 0544 - Close key.
//	{KEY_COMMAND, FKey | '?'},		// 0545 - Command key.
//	{KEY_COPY, FKey | '?'},			// 0546 - Copy key.
//	{KEY_CREATE, FKey | '?'},		// 0547 - Create key.
	{KEY_END, FKey | 'E'},			// 0550 - End key (fn-right-arrow).
//	{KEY_EXIT, FKey | '?'},			// 0551 - Exit key.
//	{KEY_FIND, FKey | '?'},			// 0552 - Find key.
	{KEY_HELP, FKey | 'L'},			// 0553 - Help key.
//	{KEY_MARK, FKey | '?'},			// 0554 - Mark key.
//	{KEY_MESSAGE, FKey | '?'},		// 0555 - Message key.
//	{KEY_MOVE, FKey | '?'},			// 0556 - Move key.
//	{KEY_NEXT, FKey | '?'},			// 0557 - Next key.
//	{KEY_OPEN, FKey | '?'},			// 0560 - Open key.
//	{KEY_OPTIONS, FKey | '?'},		// 0561 - Options key.
//	{KEY_PREVIOUS, FKey | '?'},		// 0562 - Previous key.
//	{KEY_REDO, FKey | '?'},			// 0563 - Redo key.
//	{KEY_REFERENCE, FKey | '?'},		// 0564 - Reference key.
//	{KEY_REFRESH, FKey | '?'},		// 0565 - Refresh key.
//	{KEY_REPLACE, FKey | '?'},		// 0566 - Replace key.
//	{KEY_RESTART, FKey | '?'},		// 0567 - Restart key.
//	{KEY_RESUME, FKey | '?'},		// 0570 - Resume key.
//	{KEY_SAVE, FKey | '?'},			// 0571 - Save key.
//	{KEY_SBEG, FKey | '?'},			// 0572 - Shifted begin key.
//	{KEY_SCANCEL, FKey | '?'},		// 0573 - Shifted cancel key.
//	{KEY_SCOMMAND, FKey | '?'},		// 0574 - Shifted command key.
//	{KEY_SCOPY, FKey | '?'},		// 0575 - Shifted copy key.
//	{KEY_SCREATE, FKey | '?'},		// 0576 - Shifted create key.
	{KEY_SDC, Shift | FKey | 'D'},		// 0577 - Shifted delete-character key (shift-fn-delete).
//	{KEY_SDL, FKey | '?'},			// 0600 - Shifted delete-line key.
//	{KEY_SELECT, FKey | '?'},		// 0601 - Select key.
	{KEY_SEND, Shift | FKey | 'E'},		// 0602 - Shifted end key (shift-fn-right-arrow).
//	{KEY_SEOL, FKey | '?'},			// 0603 - Shifted clear-to-end-of-line key.
//	{KEY_SEXIT, FKey | '?'},		// 0604 - Shifted exit key.
//	{KEY_SFIND, FKey | '?'},		// 0605 - Shifted find key.
	{KEY_SHELP, Shift | FKey | 'L'},	// 0606 - Shifted help key.
	{KEY_SHOME, Shift | FKey | 'H'},	// 0607 - Shifted home key (shift-fn-left-arrow).
//	{KEY_SIC, FKey | '?'},			// 0610 - Shifted insert-character key.
	{KEY_SLEFT, Shift | FKey | 'B'},	// 0611 - Shifted left-arrow key (shift-left-arrow).
//	{KEY_SMESSAGE, FKey | '?'},		// 0612 - Shifted message key.
//	{KEY_SMOVE, FKey | '?'},		// 0613 - Shifted move key.
//	{KEY_SNEXT, FKey | '?'},		// 0614 - Shifted next key.
//	{KEY_SOPTIONS, FKey | '?'},		// 0615 - Shifted options key.
//	{KEY_SPREVIOUS, FKey | '?'},		// 0616 - Shifted previous key.
//	{KEY_SPRINT, FKey | '?'},		// 0617 - Shifted print key.
//	{KEY_SREDO, FKey | '?'},		// 0620 - Shifted redo key.
//	{KEY_SREPLACE, FKey | '?'},		// 0621 - Shifted replace key.
	{KEY_SRIGHT, Shift | FKey | 'F'},	// 0622 - Shifted right-arrow key (shift-right-arrow).
//	{KEY_SRSUME, FKey | '?'},		// 0623 - Shifted resume key.
//	{KEY_SSAVE, FKey | '?'},		// 0624 - Shifted save key.
//	{KEY_SSUSPEND, FKey | '?'},		// 0625 - Shifted suspend key.
//	{KEY_SUNDO, FKey | '?'},		// 0626 - Shifted undo key.
//	{KEY_SUSPEND, FKey | '?'},		// 0627 - Suspend key.
//	{KEY_UNDO, FKey | '?'},			// 0630 - Undo key.
//	{KEY_MOUSE, FKey | '?'},		// 0631 - Mouse event has occurred.
	{KEY_RESIZE, TermResizeKey},		// 0632 - Terminal resize event.
//	{KEY_EVENT, FKey | '?'},		// 0633 - We were interrupted by an event.
	{-1, 0},				// END OF LIST.
	};
//#define KEY_MAX		0777		// Maximum key value is 0633.

static ExtKey extKeyTable[KEY_EVENT - KEY_MIN + 1];
						// Table for fast function key lookups by key code -> extended key.
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
	if(addTERM && vTerm.termName != NULL)
		(void) rscat(", TERM '%s'", vTerm.termName);
	return sess.rtn.status;
	}

// Flush virtual screen to physical screen.  Return status.
int tflush(void) {

	return refresh() == ERR ? sysCallError("tflush", "refresh", false) : sess.rtn.status;
	}

// Get a character and return it in *pExtKey if pExtKey not NULL.  Return status.
int tgetc(bool msgLine, ExtKey *pExtKey) {
	short keyCode;
	ExtKey extKey;
	WINDOW *pWindow = msgLine ? term.pMsgLineWind : stdscr;

	// Loop until we get a character or an error occurs.
	for(;;) {
		if((keyCode = wgetch(pWindow)) != ERR) {

			// Got a key.  Process it if no high bits set or is a known function key; otherwise, ignore it.
#if MMDebug & Debug_NCurses
			fprintf(logfile, "tgetc(): Got keyCode %.4ho\n", keyCode);
			if(!((extKey = keyCode) & ~0xff))
				break;
			extKey = extKeyTable[keyCode - KEY_MIN];
			fprintf(logfile, "  -> FKey x%.4hX\n", extKey);
			if(extKey != 0)
				break;
#else
			if(!((extKey = keyCode) & ~0xff) || (extKey = extKeyTable[keyCode - KEY_MIN]) != 0)
				break;
#endif
#if MMDebug & Debug_KeyCode
			fprintf(logfile, "tgetc(): Got unknown keyCode %.4ho\n", keyCode);
#endif
			}
		else if(errno != 0)
			return sysCallError("tgetc", "wgetch", false);
		}

	// Return key if requested.
	if(pExtKey != NULL) {
		if(!(keyCode & ~0x7f)) {	// If 7-bit key code...

			// Convert raw 7-bit character to extended key.
			extKey = ctoek(keyCode);
			}
		*pExtKey = extKey;
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
static void setTermSize(int cols, int rows) {

	term.cols = cols;
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
static int getTermSize(ushort *col, ushort *row) {
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

	if((*col = w.ws_col) > TTY_MaxCols)
		*col = TTY_MaxCols;
	if((*row = w.ws_row) > TTY_MaxRows)
		*row = TTY_MaxRows;

	return sess.rtn.status;
	}

// Reset terminal.  Get the current terminal dimensions, update the ETerm structure, flag all screens that have different
// dimensions for a "window resize", and flag current screen for a "redraw".  Return status.  Needs to be called when the size
// of the terminal window changes; for example, when switching from portrait to landscape viewing on a mobile device.
int resetTerm(Datum *pRtnVal, long n, Datum **args) {
	static const char myName[] = "resetTerm";
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
	ExtKeyEntry *pExtKeyEntry;
	const char *str, *strEnd;
	struct lconv *pLocale;
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

	// Initialize locale.  *NOTE: The LC_ALL option is for future use if and when multibyte support is added.  For now, the
	// default "C" locale gives the best performance and using the native locale for LC_NUMERIC enables thousands'
	// separators in numbers.
#if 0
	(void) setlocale(LC_ALL, "");
#else
	(void) setlocale(LC_NUMERIC, "");
#endif
	pLocale = localeconv();
	if((sess.thouSep = strdup(pLocale->thousands_sep)) == NULL)
		return rsset(Panic, 0, text94, myName);
			// "%s(): Out of memory!"
	sess.thouSepLen = strlen(sess.thouSep);

	// Initialize screen.
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

	// Initialize curses-key-code-to-extended-key extKeyTable, which is populated with all entries from keyList table (which
	// includes S-TAB) plus NumFKeyCount function keys, both shifted and unshifted.  Note that 64 slots (key codes) are
	// reserved for function keys in ncurses.h but shifted ones are not documented.  So we will assume that the shifted ones
	// begin at slot 32.

	// Add keys from keyList table...
	pExtKeyEntry = keyList;
	do {
#if SanityCheck
		// Validate extended key.
		cols = pExtKeyEntry->extKey;
		if((cols & (Ctrl | FKey)) == (Ctrl | FKey) || 
		 (cols & Ctrl && (((cols & 0xff) != ' ' && cols & 0xff < 'A') || cols & 0xff > '_')) ||
		 (cols & FKey && ((str = strchr(fKeySyms, cols & 0xff)) == NULL ||
		 (ulong)(str - fKeySyms) >= SpecFKeyCount + NumFKeyCount)))
			return rsset(FatalError, 0, text991, cols, pExtKeyEntry->keyCode);
				// "Invalid extended key %.4hx for key code %.4ho in keyList table"
#endif
		extKeyTable[pExtKeyEntry->keyCode - KEY_MIN] = pExtKeyEntry->extKey;
		} while((++pExtKeyEntry)->keyCode != -1);

	// and generate numbered function key entries, beginning with key code KEY_F0.
	rows = KEY_F0 - KEY_MIN;	// Table offset to first function key entry.
	cols = 0;			// Shift prefix.
	do {
		strEnd = (str = fKeySyms + SpecFKeyCount) + NumFKeyCount;
		do {
			extKeyTable[rows++] = cols | FKey | *str++;
			} while(str < strEnd);
		rows += (KEY_DL - KEY_F0) / 2;
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
	str = tigetstr("kbs");
	if(str != NULL && str[0] == 0x7f && str[1] == '\0')
		extKeyTable[KEY_BACKSPACE - KEY_MIN] = Ctrl | '?';
#endif
#if MMDebug & Debug_DumpExtKey
	// Dump extended key table to log file.
	ExtKey *pExtKey, *pExtKeyEnd;
	char keyBuf[16];

	rows = 0;
	pExtKeyEnd = (pExtKey = extKeyTable) + elementsof(extKeyTable);
	do {
		cols = *pExtKey++;
		fprintf(logfile, "%.4hx -> %s\n", rows++, cols == 0 ? "NUL" : cols == (FKey | '?') ? "FN ??" :
		 ektos(keyBuf, cols, false));
		} while(pExtKey < pExtKeyEnd);
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

// Do "chdir", "mkdir", or "rmdir" command.  Return status.
int dirFunc(Datum *pRtnVal, long n, Datum **args, CmdFuncId cmdFuncId) {
	TermInpCtrl termInpCtrl = {NULL, RtnKey, MaxPathname, NULL};
	const char *successMsg, *failureMsg;
	int rtnCode;
	mode_t pmode = 0777;
	ushort rflags = 0;

	// Get directory name.
	if(sess.opFlags & OpScript) {
		if(expandPath(pRtnVal, args[0]) != Success)
			goto Retn;
		}
	else if(termInp(pRtnVal, cmdFuncId == cf_chdir ? text277 : cmdFuncId == cf_mkdir ? text499 : text500, ArgNil1,
						// "Change directory",		"Create directory", "Remove directory"
	 TermC_Filename, &termInpCtrl) != Success || disnil(pRtnVal))
		goto Retn;

	// Process command.
	switch(cmdFuncId) {
		case cf_chdir:
			if(chgdir(pRtnVal->str) != Success)
				goto Retn;

			// Get new path (absolute pathname), save it in current screen, and return it to caller.
			if(setWorkDir(sess.pCurScrn) != Success)
				goto Retn;
			if(dsetstr(sess.pCurScrn->workDir, pRtnVal) != 0)
				return libfail();

			// Set mode line update flag if needed.
			if(mode.cache[MdIdxWkDir]->flags & MdActive)
				sess.pCurWind->face.wflags |= WFMode;

			// Run change-directory user hook unless it is not set or is currently running.
			HookRec *pHookRec = hookTable + HkChDir;
			if(pHookRec->func.type != PtrNull && !pHookRec->running && execHook(NULL, n, pHookRec, 0) != Success)
				goto Retn;

			// Return status if 'WkDir' global mode enabled; otherwise, display new directory.
			if(mode.cache[MdIdxWkDir]->flags & MdActive)
				goto Retn;
			successMsg = sess.pCurScrn->workDir;
			rflags = RSNoFormat | RSNoWrap;
			break;
		case cf_mkdir:
			// Get mode.
			if(sess.opFlags & OpScript) {
				if(args[1] != NULL)
					pmode = dgetint(args[1]);
				}
			else if(n != DefN) {
				Datum *pDatum;
				if((pDatum = dnewtrack()) == NULL)
					return libfail();
				if(termInp(pDatum, text387, ArgNil1 | ArgNotNull1, 0, NULL) != Success ||
						// "Mode"
				 disnil(pDatum) || toInt(pDatum, 0) != Success)
					goto Retn;
				pmode = dgetint(pDatum);
				}

			// Make system call.
			failureMsg = text501;
				// "Cannot create directory \"%s\": %s"
			successMsg = text503;
				// "Directory created"
			rtnCode = mkdir(pRtnVal->str, pmode);
			goto CheckStatus;
		case cf_rmdir:
			failureMsg = text502;
				// "Cannot remove directory \"%s\": %s"
			successMsg = text504;
				// "Directory removed"
			rtnCode = rmdir(pRtnVal->str);
CheckStatus:
			if(rtnCode != 0)
				return rsset(Failure, 0, failureMsg, pRtnVal->str, strerror(errno));
		}

	// Return success message.
	(void) rsset(Success, rflags, successMsg);
Retn:
	return sess.rtn.status;
	}

// Suspend MightEMacs.
int suspendMM(Datum *pRtnVal, long n, Datum **args) {

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
void centiPause(int cs) {

	if(cs > 0) {
		struct timeval t;

		// Set time record.
		t.tv_sec = cs / 100;
		t.tv_usec = cs % 100 * 10000;

		// Call select();
		(void) select(1, NULL, NULL, NULL, &t);
		}
	}

// Duplicate a string and return Panic if failure.
int strclone(char **pDest, const char *src, const char *funcName) {

	if((*pDest = strdup(src)) == NULL)
		return rsset(Panic, 0, text94, funcName);
			// "%s(): Out of memory!"
	return sess.rtn.status;
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
int shellCLI(Datum *pRtnVal, long n, Datum **args) {
	int rtnCode;
	char *sh = shellPath();

	if(tclose(true) != Success)			// Close down...
		return sess.rtn.status;
	rtnCode = system(sh);				// spawn a shell...
	dsetbool(rtnCode == 0, pRtnVal);		// set return value to false if error occurred, otherwise true...
	if(rtnCode != 0)
		(void) rsset(Failure, RSHigh, text194, sh);
			// "Shell command \"%s\" failed"
	return topen2(true);				// and restart system.
	}

// Get Unix command line.  Return status.
static int getCmd(Datum **ppCmdLine, const char *prompt) {

	if((*ppCmdLine = dnewtrack()) == NULL)
		return libfail();
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
static int insertPipeData(int fileHandle, long n, Buffer *pBuf, DataInsert *pDataInsert, ushort flags) {

#if FastFileIO
	inpInit(fileHandle);
	pDataInsert->pTargBuf = pBuf;
	pDataInsert->pTargPoint = (pBuf == sess.cur.pBuf) ? NULL : &pBuf->face.point;
	pDataInsert->msg = flags & PipeInsert ? text153 : text139;
				// "Inserting data...", "Reading data..."

	// Enable non-blocking reads and timeout retries.
	if(fcntl(fileHandle, F_SETFL, O_NONBLOCK) == -1)
		return sysCallError("insertPipeData", "fcntl", false);
	fileInfo.flags |= FIRetry;
	fileInfo.filename = pipeFilename;

	// Read the data.
	return insertData(n, NULL, pDataInsert);
#else
	if(inpInit(fileHandle) == Success) {
		pDataInsert->pTargBuf = pBuf;
		pDataInsert->pTargPoint = (pBuf == sess.cur.pBuf) ? NULL : &pBuf->face.point;
		pDataInsert->msg = flags & PipeInsert ? text153 : text139;
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
#endif
	}

// Save results of command execution in a temporary buffer.  If pCmdLine is NULL, skip header line.  Return status.
static int resultBuf(Buffer **ppBuf, Datum *pCmdLine, int *pipe3, int *pipe2) {
	ushort oldFlags;
	short oldStatus;
	DataInsert dataInsert;

	// Save message flag state and disable it.
	oldFlags = mode.cache[MdIdxRtnMsg]->flags & MdActive;
	clearGlobalMode(mode.cache[MdIdxRtnMsg]);

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
	bchgflag(*ppBuf, false);
	if(oldFlags)
		setGlobalMode(mode.cache[MdIdxRtnMsg]);
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
int pipeCmd(Datum *pRtnVal, long n, const char *prompt, uint flags) {
	const char *filename;
	Buffer *pResultBuf;
	Datum *pCmdLine;
	int count, pipe1[2], pipe2[2], pipe3[2];
	int status;
	pid_t pid;
	ushort action;
	ushort rendFlags = RendNewBuf | RendAltML | RendWait;
	OptHdr *pOptHdr;
	static const char myName[] = "pipeCmd";

	dsetbool(false, pRtnVal);

	// Get options if shellCmd.
	if(flags & PipePopOnly) {
#if MMDebug & Debug_PipeCmd
		fputs("\npipeCmd(): executing shellCmd...\n", logfile);
#endif
		pOptHdr = cmdFuncTable[cf_shellCmd].pOptHdr;
		if(n == DefN)
			deselectOpts(pOptHdr);
		else if(sess.opFlags & OpScript && !needSym(s_comma, true))
			return sess.rtn.status;
		}

	// Get shell command.  Cancel operation if nothing entered.
	if(getCmd(&pCmdLine, prompt) != Success || disnil(pCmdLine))
		return sess.rtn.status;

	// If target buffer is current buffer and executing a pipeBuf or readPipe command, verify it can be erased (getting user
	// okay if necessary) before proceeding.
	if(!(flags & (PipePopOnly | PipeInsert)) && (n == DefN || n == 1) && bconfirm(sess.cur.pBuf, 0) != Success)
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
				otpInit(sess.cur.pBuf, pipe1[1]);
				fileInfo.filename = pipeFilename;
				fileInfo.flags |= FIRetry;
				exit(writeDiskPipe(sess.cur.pBuf, NULL) == Success ? 0 : -1);
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
			if(getPend(action & 1 ? pipe3[0] : pipe2[0], &count) != Success) {
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
				if(pOptHdr->optTable[1].ctrlFlags & OptSelected) {
#if MMDebug & Debug_PipeCmd
					fputs("shellCmd: command succeeded, skipping pop-up per options.\n", logfile);
#endif
					n = true;
					goto SetResult;
					}
#if MMDebug & Debug_PipeCmd
				fputs("shellCmd: command succeeded, calling resultBuf()...\n", logfile);
#endif
				if(resultBuf(&pResultBuf, pOptHdr->optTable[2].ctrlFlags & OptSelected ? NULL : pCmdLine, pipe3,
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
				if(pOptHdr->optTable[0].ctrlFlags & OptSelected)
					rendFlags |= RendShift;
				n = true;
				goto PopBuf;
				}

			// pipeBuf, readPipe, or insertPipe: read or insert data from pipe2 into a buffer.
			Buffer *pBuf;
			DFab fab;
			DataInsert dataInsert;

			if(flags & PipeInsert || (!(flags & PipeWrite) && (n == DefN || n == 1))) {
				pBuf = sess.cur.pBuf;
				if(!(flags & PipeInsert) && readPrep(pBuf, BC_IgnChgd) != Success)
					goto CloseWait;
				}
			else if(bscratch(&pBuf) != Success)
				goto CloseWait;
			if(insertPipeData(pipe2[0], n, pBuf, &dataInsert, flags) == Success) {
				if(dopentrack(&fab) != 0)
					goto LibFail;
				if(ioStat(&fab, dataInsert.finalDelim ? 0 : IOS_NoDelim, NULL, Success, NULL,
				 flags & PipeInsert ? text154 : text131, dataInsert.lineCt) == Success) {
						// "Inserted", "Read"
					if(!(flags & PipeInsert)) {
						bchgflag(pBuf, false);

						// If pipeBuf command and results were destined for current buffer, swap
						// contents of scratch buffer and current buffer and delete the scratch.
						if(flags & PipeWrite && (n == DefN || n == 1)) {
							Line *pLine = pBuf->pFirstLine;
							pBuf->pFirstLine = sess.cur.pBuf->pFirstLine;
							sess.cur.pBuf->pFirstLine = pLine;
							(void) bdelete(pBuf, BC_IgnChgd);		// Can't fail.
							pBuf = sess.cur.pBuf;
							bchgflag(pBuf, false);
							supd_windFlags(pBuf, WFHard | WFMode);
							}
						faceInit(pBuf == sess.cur.pBuf ? &sess.pCurWind->face :
						 &pBuf->face, pBuf->pFirstLine, pBuf);
						(void) render(pRtnVal, n == DefN ? 1 : n, pBuf, n == DefN ? 0 :
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

				if(dopenwith(&alert, pRtnVal, FabClear) != 0 || dputf(&alert, 0, text194, pCmdLine->str) != 0 ||
										// "Shell command \"%s\" failed"
				 dclose(&alert, FabStr) != 0)
					goto LibFail;
				if(mlputs(pRtnVal->str, MLHome | MLFlush | MLForce) != Success)
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
	return libfail();
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
					if((name = getenv("HOME")) != NULL && dputs(name, &fab, 0) != 0)
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
				str = strcbrk(name = str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
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
					if(dputs(pPass->pw_dir, &fab, 0) != 0)
						goto LibFail;
					*str = c2;
					continue;
					}

				// Have "$var" or "${var}".
				if((name = getenv(name)) != NULL && dputs(name, &fab, 0) != 0)
					goto LibFail;
				*str = c2;
				if(braceForm)
					++str;
				continue;
			}

		// Not a special character... just copy it.
		if(dputc(c1, &fab, 0) != 0)
			goto LibFail;
		}

	// Expansion completed... return result.
	if(dclose(&fab, FabStr) != 0)
		goto LibFail;
	if(pSrc == NULL)
		dxfer(pDest, fab.pDatum);
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Save pathname on heap (in path) and set *pDest to it.  Return status.
static int savePath(char **pDest, const char *name) {
	static char *path = NULL;

	if(path != NULL)
		free((void *) path);
	(void) strclone(pDest, name, "savePath");
	path = *pDest;
	return sess.rtn.status;
	}

// Find script file(s) in the HOME directory or in the $execPath directories per flags, given name, and return status.  If name
// begins with "/", "./", or "../", it is either searched for verbatim (XP_GlobPat flag not set) or the search immediately fails
// (XP_GlobPat flag set); otherwise, if XP_HomeOnly flag is set, it is searched for in the HOME directory only; otherwise, it is
// searched for in every directory in $execPath (skipping null directories if XP_SkipNull flag set) either as a glob pattern
// (XP_GlobPat flag set) or a plain filename (XP_GlobPat flag not set).  Call details are as follows:
//
// If XP_GlobPat flag is set:
//	- result must be of type glob_t * and point to a glob_t structure provided by the caller.
//	- matched pathnames are returned in the gl_pathv member, as described in the glob() man page.  Flag GLOB_MARK is set.
//	- member gl_pathc will be zero if no matches were found.
//	- caller must call globfree() after processing pathnames if gl_pathc > 0.
// If XP_GlobPat flag is not set:
//	- result must be of type char **.
//	- *result is set to the absolute pathname if a match was found, otherwise NULL.
//	- searches are for the original filename first, followed by <filename>ScriptExt unless the filename already has that
//	  extension.
// In all cases, if XP_SkipNull flag is set, null directories in $execPath are skipped.
int pathSearch(const char *name, uint flags, void *result, const char *funcName) {
	char *str1, *str2;
	Datum *pExtName;			// Name with extension, if applicable.
	glob_t *pGlob;
	char **pPath;
	size_t maxLen = strlen(name);
	static char *scriptExt = ScriptExt;
	size_t scriptExtLen = strlen(scriptExt);

	// Null filename?
 	if(maxLen == 0)
 		return sess.rtn.status;

	// Get ready.
	if(flags & XP_GlobPat)
		pGlob = (glob_t *) result;
	else {
		pPath = (char **) result;
		*pPath = NULL;
		}

 	// Create filename-with-extension version.
 	if((pExtName = dnewtrack()) == NULL)
 		goto LibFail;

 	// Add extension to name if not already there.
	if(!(flags & XP_GlobPat) && strcmp(cname((char *) name, CNExtension | CNPtrOnly), scriptExt) != 0) {
		maxLen += scriptExtLen;
		if(dsalloc(pExtName, maxLen + 1) != 0)
			goto LibFail;
		sprintf(pExtName->str, "%s%s", name, scriptExt);
		}
	else
		dsetnull(pExtName);

	// Now find file.  If the path begins with '/', check only that: verbatim first, then with script extension.
	if(name[0] == '/' || (name[0] == '.' && (name[1] == '/' || (name[1] == '.' && name[2] == '/')))) {
		if(flags & XP_GlobPat)
			return rsset(Failure, 0, text407, funcName);
				// "Invalid argument for %s glob search"
		if(fileExists(name) & (FTypRegular | FTypSymLink))
			return savePath(pPath, name);
		if(!disnull(pExtName) && (fileExists(pExtName->str) & (FTypRegular | FTypSymLink)))
			return savePath(pPath, pExtName->str);
		return sess.rtn.status;
		}

	// Check HOME directory (only), if requested.
	if(flags & XP_HomeOnly) {
		if((str1 = getenv("HOME")) != NULL) {
			char pathBuf[strlen(str1) + maxLen + 2];

			// Build pathname and check it.
			sprintf(pathBuf, "%s/%s", str1, name);
			if(fileExists(pathBuf) & (FTypRegular | FTypSymLink))
				return savePath(pPath, pathBuf);
			}
		return sess.rtn.status;
		}

	// All special cases checked.  Create name list (filenames to check) and scan the execPath directories.
	char *dirSepTok;
	uint globFlags = GLOB_MARK | GLOB_NOSORT | GLOB_BRACE;
	const char *nameList[] = {
		name, pExtName->str, NULL};
	for(str2 = execPath; *str2 != '\0';) {

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
						if((r = glob(pathBuf, globFlags, NULL, pGlob)) != 0 && r != GLOB_NOMATCH)
							return rsset(errno == GLOB_NOSPACE ? FatalError : Failure, 0, text406,
											// "Glob of pattern \"%s\" failed: %s"
							 pathBuf, strerror(errno));
						if(pGlob->gl_pathc > 0)
							globFlags |= GLOB_APPEND;
						}
					else if(fileExists(pathBuf) & (FTypRegular | FTypSymLink))
						return savePath(pPath, pathBuf);
					}
				} while(*++ppName != NULL);
			}

		// Not found or glob search.  Onward...
		if(*str2 == ':')
			++str2;
		}

	// No such luck (or glob search).
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Get absolute pathname of "filename" (which may be modified) and return in "pPath".  Resolve it if it's a symbolic link and
// "resolve" is true.  Return status.
int absPath(char *filename, bool resolve, Datum *pPath) {
	char nameBuf[MaxPathname + 1];

	if(!resolve) {
		struct stat s;

		if(lstat(filename, &s) != 0)
			goto ErrRetn;
		if((s.st_mode & S_IFMT) != S_IFLNK)
			goto RegFile;

		// File is a symbolic link.  Get pathname of parent directory and append filename.
		DFab fab;
		char *basename = cname(filename, CNBasename);
		filename = cname(filename, CNDirname | CNDefaultDir);
		if(realpath(filename, nameBuf) == NULL)
			goto ErrRetn;
		if(dopenwith(&fab, pPath, FabClear) != 0 || dputs(nameBuf, &fab, 0) != 0 ||
		 (strcmp(nameBuf, "/") != 0 && dputc('/', &fab, 0) != 0) ||
		 dputs(basename, &fab, 0) != 0 || dclose(&fab, FabStr) != 0)
			goto LibFail;
		}
	else {
RegFile:
		if(realpath(filename, nameBuf) == NULL)
ErrRetn:
			return rsset(errno == ENOMEM ? FatalError : Failure, 0, text33, text37, filename, strerror(errno));
				// "Cannot get %s of file \"%s\": %s", "pathname"
		if(dsetstr(nameBuf, pPath) != 0)
LibFail:
			(void) libfail();
		}

	return sess.rtn.status;
	}

// Open directory for filename retrieval, given complete or partial pathname (which may end with a slash) in a mutable buffer.
// Initialize *pFilePath to static pointer to pathnames to be returned by ereaddir().  Return status.
int eopendir(const char *fileSpec, char **pFilePath) {
	char *basename, *term;
	size_t prefixLen;

	// Find directory prefix.  Terminate after slash if Unix root directory, otherwise at rightmost slash.
	basename = cname((char *) fileSpec, CNBasename | CNPtrOnly | CNKeepSlash);
	term = (basename > fileSpec && basename - fileSpec > 1) ? basename - 1 : basename;
	prefixLen = term - fileSpec;

	// Get space for directory name plus maximum filename.
	if(filePath != NULL)
		free((void *) filePath);
	if((filePath = (char *) malloc(prefixLen + MaxFilename + 3)) == NULL)
		return rsset(Panic, 0, text94, "eopendir");
			// "%s(): Out of memory!"
	stplcpy(*pFilePath = filePath, fileSpec, prefixLen + 1);

	// Open the directory.
	if(dir != NULL)
		closedir(dir);
	if((dir = opendir(*filePath == '\0' ? "." : filePath)) == NULL)
		return rsset(Failure, RSHigh, text88, filePath, strerror(errno));
			// "Cannot read directory \"%s\": %s"

	// Set filePathEnd, restore trailing slash in pathname if applicable, and return.
	if(basename > fileSpec)
		(filePathEnd = filePath + (basename - fileSpec))[-1] = '/';
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
			return rsset(Failure, RSHigh, text88, filePath, strerror(errno));
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
