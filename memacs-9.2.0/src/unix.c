// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// unix.c	Unix driver functions for MightEMacs.
//
//
// New features:
//
// 1. Timeouts waiting on a function key have been changed from 35000 to 200000 microseconds.
//
// 2. Additional keymapping entries can be made from the command language by issuing a 'set $palette xxx'.
//    The format of xxx is a string as follows:
//	"KEYMAP keybinding escape-sequence".
//    For example, to add "<ESC><[><A>" as a keybinding of FNN, issue:
//	"KEYMAP FNN ^[[A".
//    Note that the <ESC> is a real escape character and it's pretty difficult to enter.
//
// Porting notes:
//
// I have tried to create this file so that it should work as well as possible without changes on your part.
//
// However, if something does go wrong, read the following helpful hints:
//
// 1. On SUN-OS4, there is a problem trying to include both the termio.h and ioctl.h files.  I wish Sun would get their
//    act together.  Even though you get lots of redefined messages, it shouldn't cause any problems with the final object.
//
// 2. In the type-ahead detection code, the individual Unix system either has a FIONREAD or a FIORDCHK ioctl call.
//    Hopefully, your system uses one of them and this be detected correctly without any intervention.
//
// 3. Also lookout for directory handling.  The SCO Xenix system is the weirdest I've seen, requiring a special load file
//    (see below).  Some machine call the result of a readdir() call a "struct direct" or "struct dirent".  Includes files
//    are named differently depending of the O/S.  If your system doesn't have an opendir()/closedir()/readdir() library
//    call, then you should use the public domain utility "ndir".
//
// To compile:
//	Compile all files normally.
// To link:
//	Select one of the following operating systems:
//		SCO Xenix:
//			use "-ltermcap" and "-lx";
//		SUN 3 and 4:
//			use "-ltermcap";
//		IBM-RT, IBM-AIX, AT&T Unix, Altos Unix, Interactive:
//			use "-lcurses".

#include "os.h"
#include "std.h"

// Kill predefined macro(s).
#undef Ctrl				// Problems with Ctrl.

#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>			// System type definitions.
#include <errno.h>
#if MACOS
#include <sys/syslimits.h>
#endif
#include <sys/stat.h>			// File status definitions.
#include <sys/ioctl.h>			// I/O control definitions.
#include <fcntl.h>			// File descriptor definitions.
#include <string.h>
#include <glob.h>

// Additional include files.
#if LINUX
#include <wait.h>
#endif
#if LINUX || USG
#include <time.h>
#else
#include <sys/time.h>			// Timer definitions.
#endif
#include <sys/select.h>
#include <signal.h>			// Signal definitions.
#if USG
#include <termio.h>			// Terminal I/O definitions.
#else
#include <termios.h>			// Terminal I/O definitions.
#endif
#include <term.h>
#include <locale.h>
#if USG
#include <dirent.h>			// Directory entry definitions.
#define DIRENTRY	dirent
#else
#include <sys/dir.h>			// Directory entry definitions.
#define DIRENTRY	direct
#endif

// Restore predefined macro(s).
#undef Ctrl				// Restore Ctrl.
#define Ctrl		0x0100

#include "exec.h"
#include "file.h"
#if MMDebug & Debug_Bind
#include "bind.h"
#endif

// Minimum terminal capabilities.
#define CAP_EL		"el"		// Clear to end of line.
#define CAP_CLEAR	"clear"		// Clear to end of page.
#define CAP_CUP		"cup"		// Cursor motion.
#define CAP_BOLD	"bold"		// Bold starts.
#define CAP_REV		"rev"		// Reverse video starts.
#define CAP_SMUL	"smul"		// Underline starts.

/*** Local declarations ***/

static char *ttypath = "/dev/tty";

// Function key tables.
typedef struct {
	short kcode;
	ushort ek;
	} KeyTab;

// FKey symbols used are "BCDEFINPVZ<>".  Keys that are unlikely to appear on any modern keyboard are set to '?'.
static KeyTab keylist[] = {
	{KEY_DOWN,FKey | 'N'},		// Down-arrow key.
	{KEY_UP,FKey | 'P'},		// Up-arrow key.
	{KEY_LEFT,FKey | 'B'},		// Left-arrow key.
	{KEY_RIGHT,FKey | 'F'},		// Right-arrow key.
	{KEY_HOME,FKey | '<'},		// Home key.
#if LINUX
	{KEY_BACKSPACE,Ctrl | '?'},	// Backspace key (delete key).
#else
	{KEY_BACKSPACE,Ctrl | 'H'},	// Backspace key (control + h).
#endif
#if 0
#define KEY_F0		0410		/* Function keys.  Space for 64 */
#define KEY_F(n)	(KEY_F0+(n))	/* Value of function key n */
#define KEY_DL		0510		/* delete-line key */
#endif
	{KEY_DL,FKey | '?'},		// Delete-line key.
	{KEY_IL,FKey | '?'},		// Insert-line key.
	{KEY_DC,FKey | 'D'},		// Delete-character key.
	{KEY_IC,FKey | 'I'},		// Insert-character key.
	{KEY_EIC,FKey | '?'},		// Sent by rmir or smir in insert mode.
	{KEY_CLEAR,FKey | 'C'},		// Clear-screen or erase key.
	{KEY_EOS,FKey | '?'},		// Clear-to-end-of-screen key.
	{KEY_EOL,FKey | '?'},		// Clear-to-end-of-line key.
	{KEY_SF,FKey | '?'},		// Scroll-forward key.
	{KEY_SR,FKey | '?'},		// Scroll-backward key.
	{KEY_NPAGE,FKey | 'V'},		// Next-page key.
	{KEY_PPAGE,FKey | 'Z'},		// Previous-page key.
	{KEY_STAB,FKey | '?'},		// Set-tab key.
	{KEY_CTAB,FKey | '?'},		// Clear-tab key.
	{KEY_CATAB,FKey | '?'},		// Clear-all-tabs key.
	{KEY_ENTER,FKey | 'E'},		// Enter/send key.
	{KEY_PRINT,FKey | '?'},		// Print key.
	{KEY_LL,FKey | '?'},		// Lower-left key (home down).
	{KEY_A1,FKey | '?'},		// Upper left of keypad.
	{KEY_A3,FKey | '?'},		// Upper right of keypad.
	{KEY_B2,FKey | '?'},		// Center of keypad.
	{KEY_C1,FKey | '?'},		// Lower left of keypad.
	{KEY_C3,FKey | '?'},		// Lower right of keypad.
	{KEY_BTAB,Shft | Ctrl | 'I'},	// Back-tab key.
	{KEY_BEG,FKey | '?'},		// Begin key.
	{KEY_CANCEL,FKey | '?'},	// Cancel key.
	{KEY_CLOSE,FKey | '?'},		// Close key.
	{KEY_COMMAND,FKey | '?'},	// Command key.
	{KEY_COPY,FKey | '?'},		// Copy key.
	{KEY_CREATE,FKey | '?'},	// Create key.
	{KEY_END,FKey | '>'},		// End key.
	{KEY_EXIT,FKey | '?'},		// Exit key.
	{KEY_FIND,FKey | '?'},		// Find key.
	{KEY_HELP,FKey | '?'},		// Help key.
	{KEY_MARK,FKey | '?'},		// Mark key.
	{KEY_MESSAGE,FKey | '?'},	// Message key.
	{KEY_MOVE,FKey | '?'},		// Move key.
	{KEY_NEXT,FKey | '?'},		// Next key.
	{KEY_OPEN,FKey | '?'},		// Open key.
	{KEY_OPTIONS,FKey | '?'},	// Options key.
	{KEY_PREVIOUS,FKey | '?'},	// Previous key.
	{KEY_REDO,FKey | '?'},		// Redo key.
	{KEY_REFERENCE,FKey | '?'},	// Reference key.
	{KEY_REFRESH,FKey | '?'},	// Refresh key.
	{KEY_REPLACE,FKey | '?'},	// Replace key.
	{KEY_RESTART,FKey | '?'},	// Restart key.
	{KEY_RESUME,FKey | '?'},	// Resume key.
	{KEY_SAVE,FKey | '?'},		// Save key.
	{KEY_SBEG,FKey | '?'},		// Shifted begin key.
	{KEY_SCANCEL,FKey | '?'},	// Shifted cancel key.
	{KEY_SCOMMAND,FKey | '?'},	// Shifted command key.
	{KEY_SCOPY,FKey | '?'},		// Shifted copy key.
	{KEY_SCREATE,FKey | '?'},	// Shifted create key.
	{KEY_SDC,FKey | '?'},		// Shifted delete-character key.
	{KEY_SDL,FKey | '?'},		// Shifted delete-line key.
	{KEY_SELECT,FKey | '?'},	// Select key.
	{KEY_SEND,FKey | '?'},		// Shifted end key.
	{KEY_SEOL,FKey | '?'},		// Shifted clear-to-end-of-line key.
	{KEY_SEXIT,FKey | '?'},		// Shifted exit key.
	{KEY_SFIND,FKey | '?'},		// Shifted find key.
	{KEY_SHELP,FKey | '?'},		// Shifted help key.
	{KEY_SHOME,FKey | '?'},		// Shifted home key.
	{KEY_SIC,FKey | '?'},		// Shifted insert-character key.
	{KEY_SLEFT,Shft| FKey | 'B'},	// Shifted left-arrow key.
	{KEY_SMESSAGE,FKey | '?'},	// Shifted message key.
	{KEY_SMOVE,FKey | '?'},		// Shifted move key.
	{KEY_SNEXT,FKey | '?'},		// Shifted next key.
	{KEY_SOPTIONS,FKey | '?'},	// Shifted options key.
	{KEY_SPREVIOUS,FKey | '?'},	// Shifted previous key.
	{KEY_SPRINT,FKey | '?'},	// Shifted print key.
	{KEY_SREDO,FKey | '?'},		// Shifted redo key.
	{KEY_SREPLACE,FKey | '?'},	// Shifted replace key.
	{KEY_SRIGHT,Shft | FKey | 'F'},	// Shifted right-arrow key.
	{KEY_SRSUME,FKey | '?'},	// Shifted resume key.
	{KEY_SSAVE,FKey | '?'},		// Shifted save key.
	{KEY_SSUSPEND,FKey | '?'},	// Shifted suspend key.
	{KEY_SUNDO,FKey | '?'},		// Shifted undo key.
	{KEY_SUSPEND,FKey | '?'},	// Suspend key.
	{KEY_UNDO,FKey | '?'},		// Undo key.
	{KEY_MOUSE,FKey | '?'},		// Mouse event has occurred.
	{KEY_RESIZE,FKey | 'R'},	// Terminal resize event.
	{KEY_EVENT,FKey | '?'},		// We were interrupted by an event.
	{-1,0},				// END OF LIST.
	};
//#define KEY_MAX		0777		/* Maximum key value is 0633 */

static ushort keytab[KEY_EVENT - KEY_MIN + 1];	// Table for fast function key lookups by index.
static DIR *dirp = NULL;		// Current directory stream.
static char *rdbuf = NULL;		// ereaddir() file buffer (allocated from heap).
static char *rdname = NULL;		// Pointer past end of path in rdbuf.
static char FNioctl[] = "ioctl";
static char FNfork[] = "fork";
static char FNpipefile[] = "<pipe>";

// Build OS error message, append TERM to it if known and addTERM is true, and return OSError status.
int scallerr(char *caller,char *call,bool addTERM) {

	(void) rcset(OSError,0,text44,call,caller);
		// "calling %s() from %s() function"
	if(addTERM && vtc.termnam != NULL) {
		DStrFab msg;

		if(dopenwith(&msg,&rc.msg,SFAppend) != 0 || dputf(&msg,", TERM '%s'",vtc.termnam) != 0 ||
		 dclose(&msg,sf_string) != 0)
			(void) librcset(Failure);
		}
	return rc.status;
	}

// Flush virtual screen to physical screen.  Return status.
int ttflush(void) {

	return refresh() == ERR ? scallerr("ttflush","refresh",false) : rc.status;
	}

// Get a character and return it in *cp if cp not NULL.  Return status.
int ttgetc(bool ml,ushort *cp) {
	short kcode;
	WINDOW *wp = ml ? term.mlwin : stdscr;

	// Loop until we get a character or an error occurs.
	for(;;) {
		if((kcode = wgetch(wp)) != ERR) {

			// Got a key.  Ignore it if unknown function key.
			if(!(kcode & ~0xFF) || keytab[kcode - KEY_MIN] != (FKey | '?'))
				break;
			}
		else if(errno != 0)
			return scallerr("ttgetc","wgetch",false);
		}

	if(cp != NULL) {
		ushort c;

		if(kcode & ~0xFF)
			c = keytab[kcode - KEY_MIN];
		else {
			// Convert to extended character if control key.
			if((c = kcode) == '\0')
				c = Ctrl | ' ';
			else if(c <= 0x1F || c == 0x7F)
				c = Ctrl | (c ^ 0x40);
			}
		*cp = c;
		}
	return rc.status;
	}

// Get count of pending input characters.  Return status.
int typahead(int *countp) {
	int count;

#ifdef FIONREAD

	// Get number of pending characters.
	if(ioctl(0,FIONREAD,&count) == -1)
		return scallerr("typahead",FNioctl,false);
	*countp = count;
#else
	// Ask hardware for count.
	count = ioctl(0,FIORDCHK,0);
	if(count < 0)
		return scallerr("typahead",FNioctl,false);
	*countp = count;
#endif
	return rc.status;
	}

// Update terminal size parameters, given number of columns and rows.
void settermsize(ushort ncol,ushort nrow) {

	sampbuf.smallsize = (term.t_ncol = ncol) / 4;
	term.t_nrow = nrow;
	calcHJC();
	}

// Get current terminal window size and save (up to hard-coded maximum) to given pointers.  Return status.
// struct winsize {
//	unsigned short ws_row;		/* rows, in characters */
//	unsigned short ws_col;		/* columns, in characters */
//	unsigned short ws_xpixel;	/* horizontal size, pixels */
//	unsigned short ws_ypixel;	/* vertical size, pixels */
//	};
int gettermsize(ushort *colp,ushort *rowp) {
	static char myname[] = "gettermsize";
	struct winsize w;

	// Get terminal size.
	if(ioctl(0,TIOCGWINSZ,&w) == -1)
		return scallerr(myname,FNioctl,true);

	// Do sanity check.
	if(w.ws_col < TT_MinCols || w.ws_row < TT_MinRows) {
		(void) endwin();
		return rcset(FatalError,0,text190,w.ws_col,w.ws_row,Myself);
			// "Terminal size %hu x %hu is too small to run %s"
		}

	if((*colp = w.ws_col) > term.t_mcol)
		*colp = term.t_mcol;
	if((*rowp = w.ws_row) > term.t_mrow)
		*rowp = term.t_mrow;
	return rc.status;
	}

// Reset terminal.  Get the current terminal dimensions, update the ETerm structure, flag all screens that have different
// dimensions for a "window resize", and flag current screen for a "redraw".  Force update if n > 0.  Return status.  Needs to
// be called when the size of the terminal window changes; for example, when switching from portrait to landscape viewing on a
// mobile device.
int resetTermc(Datum *rp,int n,Datum **argpp) {
	static char myname[] = "resetTermc";
	EScreen *scrp;		// Current screen being examined.
	ushort ncol,nrow;	// Current terminal dimensions.

	// Get current terminal size.
	if(gettermsize(&ncol,&nrow) != Success)
		return rc.status;

	// In all screens...
	scrp = si.sheadp;
	n = (n <= 0) ? 0 : 1;
	do {
		// Flag screen if its not the current terminal size.
		if(scrp->s_nrow != nrow || scrp->s_ncol != ncol) {
			scrp->s_flags |= EScrResize;
			n = 1;
			}
		} while((scrp = scrp->s_nextp) != NULL);

	// Perform update?
	if(n) {
		char *name;

		// Yes, update ETerm settings and ncurses window.
		settermsize(ncol,nrow);
		if(resizeterm(nrow,ncol) == ERR) {
			name = "resizeterm";
			goto Err;
			}
		if(mvwin(term.mlwin,nrow - 1,0) == ERR) {
			name = "mvwin";
Err:
			return scallerr(myname,name,true);
			}
		if(ttflush() == Success) {

			// Force full screen update.
			si.opflags |= OpScrRedraw;
			(void) rcset(Success,0,text227,ncol,nrow);
				// "Terminal dimensions set to %hu x %hu"
			}
		}

	return rc.status;
	}

// Re-open screen package -- at program startup or after a "suspend" operation.  Return status.
static int ttopen2(bool resetTTY) {

	(void) ttflush();

	// Initialize terminal attributes.
	if(resetTTY && reset_prog_mode() == ERR)
		return scallerr("ttopen2","reset_prog_mode",false);
	si.opflags |= (OpVTOpen | OpScrRedraw);

	return rc.status;
	}

// Initialize screen package -- done once at startup.  Return status.
int ttopen(void) {
	static char myname[] = "ttopen";
	ushort ncol,nrow;

	// Get terminal type.
	if((vtc.termnam = getenv("TERM")) == NULL)
		return rcset(FatalError,RCNoFormat,text182);
		// "Environmental variable 'TERM' not defined!"

	// Fix up file descriptors if reading a file from standard input.
	if(!isatty(0)) {
		int fd;

		// File descriptor 0 is not a TTY so it must be a data file or pipe.  Get a new descriptor for it by calling
		// dup() (and saving the result in fi.stdinfd), then open /dev/tty, and dup2() that FD back to 0.  This is done
		// so that FD 0 is always the keyboard which should ensure that certain functions, such as tgetent(), will work
		// properly on any brain-dead Unix variants.
		if((fi.stdinfd = dup(0)) == -1)
			return scallerr(myname,"dup",false);
		if((fd = open(ttypath,O_RDONLY)) == -1)
			return scallerr(myname,"open",false);
		if(dup2(fd,0) == -1)
			return scallerr(myname,"dup2",false);
		}

	// Get terminal size and save it.
	if(gettermsize(&ncol,&nrow) != Success)
		return rc.status;
	settermsize(ncol,nrow);

	// Initialize locale and screen.
	(void) setlocale(LC_ALL,"");
	(void) initscr();
	if(has_colors()) {
		chtype wch,freePairs;

		// Terminal supports color.  Check if enough color pairs are available for showColors display at minimum.
		// If not, do not enable color (OpHaveColor flag).
		(void) start_color();
		(void) use_default_colors();
		wch = A_COLOR;
		if(wch > 0)
			while((wch & 1) == 0)
				wch >>= 1;
		term.maxColor = COLORS - 1;
		freePairs = COLOR_PAIRS - 1;
		term.maxPair = (freePairs < wch) ? freePairs : wch;

		// Require a minimum of 7 colors (not including color #0, which is black) and 63 pairs.  However, there must be
		// enough pairs to display a minimum of 8 lines in the showColors display (which requires 4 pairs per line) plus
		// three more (ReservedPairs) which are reserved for informational displays and the mode line.  Note that even
		// though the ncurses man pages seem to indicate that color number COLORS (for example, #8 or #256) is a valid
		// color, testing has shown that it is not -- weird things happen when it is used.
		if(term.maxColor >= 7 && term.maxPair >= 63 && (freePairs = term.maxPair - ReservedPairs) >= (8 * 4)) {

			// We're good.  Set up other color parameters.
			term.lpp = (freePairs >= 60 * 4) ? 60 : (freePairs >= 32 * 4) ? 32 : 8;
			term.maxWorkPair = term.maxPair - ReservedPairs;
			term.colorWhite = (ColorWhite <= term.maxColor) ? ColorWhite : COLOR_WHITE;
			term.colorRed = (ColorRed <= term.maxColor) ? ColorRed : COLOR_RED;
			term.colorGreen = (ColorGreen <= term.maxColor) ? ColorGreen : COLOR_GREEN;

			// Set mode line to color and initialize reserved pair for it if possible.
			if(ColorBrown <= term.maxColor) {
				term.mdlColor[0] = term.colorWhite;
				term.mdlColor[1] = ColorBrown;
				(void) init_pair(term.maxPair - ColorPairML,term.colorWhite,ColorBrown);
				}

			// Initialize reserved pair for keyboard macro recording indicator and set "color enabled" flag.
			(void) init_pair(term.maxPair - ColorPairKMRI,term.colorWhite,term.colorRed);
			si.opflags |= OpHaveColor;
			}
		}
	if((term.mlwin = newwin(1,ncol,nrow - 1,0)) == NULL ||
	 raw() == ERR ||
	 noecho() == ERR ||
	 nonl() == ERR ||
	 keypad(stdscr,TRUE) == ERR ||
	 keypad(term.mlwin,TRUE) == ERR ||
	 set_escdelay(200) == ERR)
		return rcset(OSError,RCNoFormat,text421);
			// "initializing ncurses"

	// Check for minimum capabilities.
	if(tigetstr(CAP_CLEAR) == NULL || tigetstr(CAP_CUP) == NULL || tigetstr(CAP_EL) == NULL)
		return rcset(FatalError,0,text192,vtc.termnam,Myself);
			// "This terminal (%s) does not have sufficient capabilities to run %s"

#if LINUX && 0
		// Linux: c_iflag: 00002800, c_oflag: 00000001, c_cflag: 000000BD, c_lflag: 00004A10 (045020)
		//	IMAXBEL | IXANY		OPOST		CREAD | CS8 | B9600	PENDIN | ECHOCTL | ECHOKE | ECHOE
		// macOS: c_iflag: 00002800, c_oflag: 00000001, c_cflag: 00004B00, c_lflag: 20000043
		//	IMAXBEL | IXANY		OPOST		HUPCL | CREAD | CS8	PENDIN | ECHOCTL | ECHOE | ECHOKE
	// Fix broken ncurses on Linux (delete key returns C-h instead of DEL).
	struct termios curterm;

	// Get tty modes.
	if(tcgetattr(0,&curterm) != 0)
		return scallerr(myname,"tcgetattr",true);

	char wkbuf[120];
	sprintf(wkbuf,"c_iflag: %.12lX, c_oflag: %.12lX, c_cflag: %.12lX, c_lflag: %.12lX",
	 curterm.c_iflag,curterm.c_oflag,curterm.c_cflag,curterm.c_lflag);
	return rcset(FatalError,0,wkbuf);
#elif 0
	// Set new modes.
	curterm.c_iflag &= ~(INLCR | ICRNL | IGNCR | IXON | IXANY | IXOFF);
	curterm.c_lflag &= ~(ICANON | ISIG | ECHO | IEXTEN);
	curterm.c_cc[VMIN] = 1;		// Minimum number of characters for noncanonical read.
	curterm.c_cc[VTIME] = 0;	// Timeout in deciseconds for noncanonical read.

	// Set tty mode.
	if(tcsetattr(0,TCSANOW,&curterm) != 0)
		return scallerr(myname,"tcsetattr",true);
#endif

	// Set attribute flags.
	if(tigetstr(CAP_REV) != NULL)
		si.opflags |= OpHaveRV;
	if(tigetstr(CAP_BOLD) != NULL)
		si.opflags |= OpHaveBold;
	if(tigetstr(CAP_SMUL) != NULL)
		si.opflags |= OpHaveUL;

	// Initialize escape-sequence table, including 20 function keys, both shifted and unshifted.  Note that 64 slots are
	// reserved for function keys in ncurses.h but shifted ones are not documented.  So we will assume that the shifted ones
	// begin at slot 32.
	KeyTab *ktp = keylist;
	do {
		keytab[ktp->kcode - KEY_MIN] = ktp->ek;
		} while((++ktp)->kcode != -1);
	char *fn;
	nrow = KEY_F0 - KEY_MIN;
	ncol = 0;
	do {
		fn = "1234567890abcdefghij";
		do {
			keytab[nrow++] = ncol | FKey | *fn++;;
			} while(*fn != '\0');
		nrow = KEY_F0 - KEY_MIN + (KEY_DL - KEY_F0) / 2;
		ncol ^= Shft;
		} while(ncol);

	// Finish up.
	return ttopen2(false);
	}

// Clean up the terminal in anticipation of a return to the operating system.  Close the keyboard, flush screen output, and
// restore the old terminal settings.  Return status.
int ttclose(bool saveTTY) {
	static char myname[] = "ttclose";

	// Skip this if terminal not open.
	if(si.opflags & OpVTOpen) {

		// Turn off curses.
		if(saveTTY && def_prog_mode() == ERR)
			return scallerr(myname,"def_prog_mode",false);
		if(endwin() == ERR)
			return scallerr(myname,"endwin",false);
		si.opflags &= ~OpVTOpen;
		}
	return rc.status;
	}

// Move cursor.  Return status.
int ttmove(int row,int col) {

	return move(row,col) == ERR ? scallerr("ttmove","move",false) : rc.status;
	}

// Set a video attribute, given operation flag, attribute flag, and Boolean on/off flag.
static void vattr(bool ml,ushort opflag,int aflag,bool attrOn) {

	// Set reverse video state.
	if(si.opflags & opflag) {
		WINDOW *winp = ml ? term.mlwin : stdscr;
		(void) (attrOn ? wattron(winp,aflag) : wattroff(winp,aflag));
		}
	}

// Set reverse video state, given Boolean on/off flag.
void ttrev(bool ml,bool attrOn) {

	vattr(ml,OpHaveRV,A_REVERSE,attrOn);
	}

// Set bold state, given Boolean on/off flag.
void ttbold(bool ml,bool attrOn) {

	vattr(ml,OpHaveBold,A_BOLD,attrOn);
	}

// Set underline state, given Boolean on/off flag.
void ttul(bool ml,bool attrOn) {

	vattr(ml,OpHaveUL,A_UNDERLINE,attrOn);
	}

// Sound the beeper.
void ttbeep(void) {

	(void) beep();
	}

// Get current working directory (absolute pathname) and save it in given screen.  Return // status.
int setwkdir(EScreen *scrp) {
	char *wkdir;

	if((wkdir = getcwd(NULL,0)) == NULL)
		return rcset(OSError,0,text44,"getcwd","setwkdir");
			// "calling %s() from %s() function"
	if(scrp->s_wkdir != NULL)
		(void) free((void *) scrp->s_wkdir);
	scrp->s_wkdir = wkdir;
	return rc.status;
	}

// Change working directory to "dir" via system call.  Return status.
int chgdir(char *dir) {

	return (chdir(dir) != 0) ? rcset(Failure,0,text265,dir,strerror(errno)) : rc.status;
			// "Cannot change directory to \"%s\": %s"
	}

// Do chdir command (change working directory).  Return status.
int chwkdir(Datum *rp,int n,Datum **argpp) {
	HookRec *hrp = hooktab + HkChDir;
	TermInp ti = {NULL,RtnKey,MaxPathname,NULL};

	// Get directory name.
	if(si.opflags & OpScript)
		datxfer(rp,argpp[0]);
	else if(terminp(rp,text277,ArgNil1,Term_C_Filename,&ti) != Success || rp->d_type == dat_nil)
			// "Change directory"
		return rc.status;

	// Make the change.
	if(chgdir(rp->d_str) != Success)
		return rc.status;

	// Get new path (absolute pathname), save it in current screen, and return it to caller.
	if(setwkdir(si.cursp) != Success)
		return rc.status;
	if(dsetstr(si.cursp->s_wkdir,rp) != 0)
		return librcset(Failure);

	// Invalidate "last screen" pointer in any buffer pointing to current screen.
	nukebufsp(si.cursp);

	// Set current window flag if needed.
	if(mi.cache[MdIdxWkDir]->ms_flags & MdEnabled)
		si.curwp->w_flags |= WFMode;

	// Run change-directory user hook unless it is not set or is currently running.
	if(hrp->h_bufp != NULL && hrp->h_bufp->b_mip->mi_nexec == 0 && exechook(NULL,n,hrp,0) != Success)
		return rc.status;

	// Display new directory if interactive and 'WkDir' global mode not enabled.
	return (!(si.opflags & OpScript) && !(mi.cache[MdIdxWkDir]->ms_flags & MdEnabled)) ?
	 rcset(Success,RCNoFormat | RCNoWrap,si.cursp->s_wkdir) : rc.status;
	}

// Suspend MightEMacs.
int suspendMM(Datum *rp,int n,Datum **argpp) {

	// Reset the terminal and exit ncurses.
	if(ttclose(true) != Success)
		return rc.status;

	// Send stop signal to self (suspend)...
	if(kill(getpid(),SIGTSTP) == -1)
		return scallerr("suspendMM","kill",false);

	// We should be back here after resuming.

	// Reopen the screen and redraw.
	return ttopen2(true);
	}

// Sleep for given number of centiseconds.  "n" is assumed to be non-negative.
//
// struct timeval {
//	__darwin_time_t 	tv_sec; 	# seconds
//	__darwin_suseconds_t	tv_usec;	# and microseconds
//	};.
void cpause(int n) {

	if(n > 0) {
		struct timeval t;

		// Set time record.
		t.tv_sec = n / 100;
		t.tv_usec = n % 100 * 10000;

		// Call select();
		(void) select(1,NULL,NULL,NULL,&t);
		}
	}

// Get time of day as a string.
char *timeset(void) {
	time_t buf;
	char *str1,*str2;

#if USG
	char *ctime();
#endif
	// Get system time.
	(void) time(&buf);

	// Pass system time to converter.
	str1 = ctime(&buf);

	// Eat newline character.
	if((str2 = strchr(str1,'\n')) != NULL)
		*str2 = '\0';

	return str1;
	}

#if USG
// Rename a file, given old and new.
int rename(const char *file1,const char *file2) {
	struct stat buf1,buf2;

	// No good if source file doesn't exist.
	if(stat(file1,&buf1) != 0)
		return -1;

	// Check for target.
	if(stat(file2,&buf2) == 0) {

		// See if file is the same.
		if(buf1.st_dev == buf2.st_dev && buf1.st_ino == buf2.st_ino)

			// Not necessary to rename file.
			return 0;
		}

	if(unlink(file2) != 0 ||	// Get rid of target.
	 link(file1,file2) != 0)	// Link two files together.
		return -1;

	// Unlink original file.
	return unlink(file1);
	}
#endif

// Get shell path.
static char *shpath(void) {
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
int shellCLI(Datum *rp,int n,Datum **argpp) {
	int rcode;
	char *sh = shpath();

	if(ttclose(true) != Success)			// Close down...
		return rc.status;
	rcode = system(sh);				// spawn a shell...
	dsetbool(rcode == 0,rp);			// set return value to false if error occurred; otherwise, true...
	if(rcode != 0)
		(void) rcset(Failure,0,text194,sh);
			// "Shell command \"%s\" failed"
	return ttopen2(true);				// and restart system.
	}

// Get Unix command line.  Return status.
static int getcmd(Datum **clpp,char *prmt) {

	if(dnewtrk(clpp) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript) {

		// Concatenate all arguments into *clpp.
		if(catargs(*clpp,1,NULL,0) == Success && disnull(*clpp))
			(void) rcset(Failure,0,text187,text53);
				// "%s cannot be null","Value"
		}
	else
		(void) terminp(*clpp,prmt,ArgNotNull1 | ArgNil1,0,NULL);

	return rc.status;
	}

// Read data from open file descriptor, insert into given buffer, and close file.  Return status.
static int ipipe(int fd,int n,Buffer *bufp,DataInsert *dip,ushort flags) {

	if(rinit(fd) == Success) {
		dip->targbufp = bufp;
		dip->targdotp = (bufp == si.curbp) ? NULL : &bufp->b_face.wf_dot;
		dip->msg = (flags & PipeInsert) ? text153 : text139;
					// "Inserting data...","Reading data..."
		fi.fname = FNpipefile;
		(void) idata(n,NULL,dip);
		}
	return rc.status;
	}

// Save results of command execution in a temporary buffer.  Return status.
static int resultBuf(Buffer **bufpp,Datum *clp,int *pipe3,int *pipe2) {
	ushort mflag;
	DataInsert di;
	char wkbuf[strlen(clp->d_str) + 4];

	// Save message flag state and disable it.
	mflag = mi.cache[MdIdxRtnMsg]->ms_flags & MdEnabled;
	gmclear(mi.cache[MdIdxRtnMsg]);

	// Get a temporary buffer.
	if(sysbuf(text424,bufpp,0) != Success)
			// "ShellCmd"
		return rc.status;

	// Write command.
	sprintf(wkbuf,"%c %s",getuid() == 0 ? '#' : '$',clp->d_str);
	if(insline(wkbuf,strlen(wkbuf),true,*bufpp,&(*bufpp)->b_face.wf_dot) != Success)
		goto ERtn;

	// Read pipe3 (standard error) into buffer.
	if(pipe3 != NULL && ipipe(pipe3[0],1,*bufpp,&di,PipeInsert) != Success)
		goto ERtn;

	// Read pipe2 (standard output) into buffer.
	if(pipe2 != NULL && ipipe(pipe2[0],1,*bufpp,&di,PipeInsert) != Success)
		goto ERtn;

	// Unchange buffer and restore message flag.
	(*bufpp)->b_flags &= ~BFChanged;
	if(mflag)
		gmset(mi.cache[MdIdxRtnMsg]);
	return rc.status;
ERtn:
	return bdelete(*bufpp,BC_IgnChgd);
	}

// Get count of pending input characters, given file descriptor.  Return status.
static int getpend(int fd,int *countp) {

	// Get number of pending characters using either a FIONREAD or FIORDCHK call, depending on underlying Unix system.
#ifdef FIONREAD
	if(ioctl(fd,FIONREAD,countp) == -1)
#else
	if((*countp = ioctl(fd,FIORDCHK,0)) < 0)
#endif
		return scallerr("getpend",FNioctl,false);
	return rc.status;
	}

// Get a shell command, fork a child process to execute it, and capture results.  Return status.
int pipecmd(Datum *rp,int n,char *prmt,ushort flags) {
	char *fname;
	Buffer *rbufp;
	Datum *clp;
	int count,pipe1[2],pipe2[2],pipe3[2];
	int status;
	pid_t pid;
	ushort action;
	ushort rflags = RendNewBuf | RendAltML | RendWait;
	static char myname[] = "pipecmd";

	dsetbool(false,rp);

	// Get shell command.  Cancel operation if nothing entered.
	if(getcmd(&clp,prmt) != Success || clp->d_type == dat_nil)
		return rc.status;

	// If target buffer is current buffer and executing a pipeBuf or readPipe command, verify it can be erased (getting user
	// okay if necessary) before proceeding.
	if(!(flags & (PipePopOnly | PipeInsert)) && (n == INT_MIN || n == 1) && bconfirm(si.curbp,0) != Success)
		return rc.status;

	// Target buffer ready.  Create three pipes: one for writing target buffer into (pipeBuf command), one for reading
	// command output from, and one for reading standard error output from.  If PipeWrite flag not set, pipe1 is closed by
	// parent process so that child gets EOF immediately if shell command reads from standard input.
	if(pipe(pipe1) != 0 || pipe(pipe2) != 0 || pipe(pipe3) != 0) {
		fname = "pipe";
		goto OSErr;
		}

	// Fork a child process to run shell command.
	switch(fork()) {
		case 0:
			 // Child process.  Connect standard input to pipe1.
			if(dup2(pipe1[0],0) < 0)
				goto DupErr;
			(void) close(pipe1[1]);

			// Connect standard output to pipe2 and standard error to pipe3.
			if(dup2(pipe2[1],1) < 0 || dup2(pipe3[1],2) < 0) {
DupErr:
				fname = "dup2";
				goto OSErr;
				}
			(void) close(pipe2[0]);
			(void) close(pipe3[0]);

			// Pipes are ready... execute a shell with command as "-c" argument.
			char *cmd[4],**cmdp = cmd;

			*cmdp++ = shpath();
			*cmdp++ = "-c";
			*cmdp++ = clp->d_str;
			*cmdp = NULL;
			execvp(cmd[0],cmd);

			// Shouldn't get here... execvp() failed.
			fname = "execvp";
			goto OSErr;
		case -1:
			fname = FNfork;
			goto OSErr;
		default:
			// Parent process.
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
				winit(si.curbp,pipe1[1]);
				fi.fname = FNpipefile;
				exit(writefd(si.curbp,NULL) == Success ? 0 : -1);
			case -1:
				fname = FNfork;
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
	pid = action = 0;
	for(;;) {
		if(action < 2) {
			// Check if any bytes pending.
			if(getpend((action & 1) ? pipe3[0] : pipe2[0],&count) != Success) {
				fname = "getpend";
				goto OSErr;
				}

			// Have data?
			if(count > 0)		// Yes, done.
				break;
			}
		else {
			// Check if a child has terminated.
			if(pid > 0)
				break;
			if((pid = waitpid(-1,&status,WNOHANG)) < 0) {
				fname = "waitpid";
				goto OSErr;
				}
			}
		cpause(4);		// Pause and try again.
		action = (action + 1) % 3;
		}

	// Check results.  Treat child termination (with no data) as success.
	switch(action) {
		case 0:
		case 2:
			// A child terminated or have pipe2 data... command execution SUCCEEDED.
			if(flags & PipePopOnly) {

				// shellCmd: build result buffer if requested, pop it, and delete it.
				if(n == 0 || (n < 0 && n != INT_MIN)) {
					n = true;
					goto SetResult;
					}
				if(resultBuf(&rbufp,clp,pipe3,pipe2) != Success)
					goto CloseWait;
				if(n > 0)
					rflags |= RendShift;
				n = true;
				goto PopBuf;
				}

			// pipeBuf, readPipe, or insertPipe: read or insert data from pipe2 into a buffer.
			Buffer *bufp;
			DStrFab sf;
			DataInsert di;

			if((flags & PipeInsert) || (!(flags & PipeWrite) && (n == INT_MIN || n == 1))) {
				bufp = si.curbp;
				if(!(flags & PipeInsert) && readprep(bufp,BC_IgnChgd) != Success)
					goto CloseWait;
				}
			else if(bscratch(&bufp) != Success)
				goto CloseWait;
			if(ipipe(pipe2[0],n,bufp,&di,flags) == Success) {
				if(dopentrk(&sf) != 0)
					goto LibFail;
				if(iostat(&sf,di.finalDelim ? 0 : IOS_NoDelim,NULL,Success,NULL,flags & PipeInsert ?
				 text154 : text140,di.lineCt) == Success) {
						// "Inserted","Read"
					if(!(flags & PipeInsert)) {
						bufp->b_flags &= ~BFChanged;

						// If pipeBuf command and results were destined for current buffer, swap
						// contents of scratch buffer and current buffer and delete the scratch.
						if((flags & PipeWrite) && (n == INT_MIN || n == 1)) {
							Line *lnp = bufp->b_lnp;
							bufp->b_lnp = si.curbp->b_lnp;
							si.curbp->b_lnp = lnp;
							(void) bdelete(bufp,BC_IgnChgd);	// Can't fail.
							bufp = si.curbp;
							bufp->b_flags &= ~BFChanged;
							supd_wflags(bufp,WFHard | WFMode);
							}
						faceinit(bufp == si.curbp ? &si.curwp->w_face : &bufp->b_face,bufp->b_lnp,bufp);
						(void) render(rp,n == INT_MIN ? 1 : n,bufp,n == INT_MIN ? 0 :
						 RendNewBuf | RendNotify);
						}
				 	}
				}
			break;
		default:
			// Have pipe3 data -- command execution FAILED.  Create result buffer and pop it for user.
			if(resultBuf(&rbufp,clp,pipe3,pipe2) != Success)
				goto CloseWait;
			if(!(flags & PipePopOnly)) {
				DStrFab alert;

				if(dopenwith(&alert,rp,SFClear) != 0 || dputf(&alert,text194,clp->d_str) != 0 ||
										// "Shell command \"%s\" failed"
				 dclose(&alert,sf_string) != 0)
					goto LibFail;
				if(mlputs(MLHome | MLFlush,rp->d_str) != Success)
					goto CloseWait;
				cpause(160);
				}
			n = false;
PopBuf:
			if(render(rp,-1,rbufp,rflags) == Success)
SetResult:
				dsetbool(n,rp);
		}
CloseWait:
	// Close read ends of pipes 2 and 3 (in case an error occurred -- no harm otherwise) and wait for children to terminate.
	(void) close(pipe2[0]);
	(void) close(pipe3[0]);
	while(wait(&status) != -1)
		;

	return rc.status;
LibFail:
	return librcset(Failure);
OSErr:
	return rcset(OSError,0,text44,fname,myname);
		// "calling %s() from %s() function"
	}

// Return pointer to base filename, given pathname or filename.  If withext if false, return base filename without extension.
// "name" is not modified in either case.
char *fbasename(char *name,bool withext) {
	char *str1,*str2;
	static char buf[MaxFilename + 1];

	if((str1 = str2 = strchr(name,'\0')) > name) {

		// Find rightmost slash, if any.
		while(str1 > name) {
			if(str1[-1] == '/')
				break;
			--str1;
			}

		// Find and eliminate extension, if requested.
		if(!withext) {
			uint len;

			while(str2 > str1) {
				if(*--str2 == '.') {
					if(str2 == str1)		// Bail out if '.' is first character.
						break;
					len = str2 - str1 + 1;
					stplcpy(buf,str1,len > sizeof(buf) ? sizeof(buf) : len);
					return buf;
					}
				}
			}
		}

	return str1;
	}

// Return pointer to directory name, given pathname or filename and n argument.  If non-default n, return "." if no directory
// portion found in name; otherwise, null.  Given name is modified (truncated).
char *fdirname(char *name,int n) {
	char *str;

	str = fbasename(name,true);
	if(*name == '/' && (*str == '\0' || str == name + 1))
		name[1] = '\0';
	else if(strchr(name,'/') == NULL) {
		if(*name != '\0') {
			if(n == INT_MIN)
				name[0] = '\0';
			else
				strcpy(name,".");
			}
		}
	else
		str[-1] = '\0';
	return name;
	}

// Save pathname on heap (in path) and set *destp to it.  Return status.
static int savepath(char **destp,char *name) {
	static char *path = NULL;

	if(path != NULL)
		(void) free((void *) path);
	if((path = (char *) malloc(strlen(name) + 1)) == NULL)
		return rcset(Panic,0,text94,"savepath");
			// "%s(): Out of memory!"
	strcpy(*destp = path,name);
	return rc.status;
	}

// Find script file(s) in the HOME directory or in the $execPath directories per flags, and return status.  If name begins with
// a slash (/), it is either searched for verbatim (XPGlobPat flag not set) or the search immediately fails (XPGlobPat flag
// set); otherwise, if XPHomeOnly flag is set, it is searched for in the HOME directory only; otherwise, it is searched for in
// every directory in $execPath (skipping null directories if XPSkipNull flag set) either as a glob pattern (XPGlobPat flag set)
// or a plain filename (XPGlobPat flag not set).  Call details are as follows:
//
// If XPGlobPat flag is set:
//	- resultp must be of type glob_t * and point to a glob_t structure provided by the caller.
//	- matched pathnames are returned in the gl_pathv member, as described in the glob() man page.  Flag GLOB_MARK is set.
//	- member gl_pathc will be zero if no matches were found.
//	- caller must call globfree() after processing pathnames if gl_pathc > 0.
// If XPGlobPat flag is not set:
//	- resultp must be of type char **.
//	- *resultp is set to the absolute pathname if a match was found; otherwise, NULL.
//	- searches are for the original filename first, followed by <filename>Script_Ext unless the filename already has that
//	  extension.
// In all cases, if XPSkipNull flag is set, null directories in $execPath are skipped.
int pathsearch(char *name,ushort flags,void *resultp,char *funcname) {
	char *str1,*str2,**np;
	Datum *enamep;				// Name with extension, if applicable.
	glob_t *gp;
	char **pathp;
	size_t maxlen = strlen(name);
	static char *scriptExt = Script_Ext;
	size_t scriptExtLen = strlen(scriptExt);

	// Get ready.
	if(flags & XPGlobPat) {
		gp = (glob_t *) resultp;
		gp->gl_pathc = 0;
		}
	else {
		pathp = (char **) resultp;
		*pathp = NULL;
		}

	// Null filename?
 	if(maxlen == 0)
 		return rc.status;

 	// Create filename-with-extension version.
 	if(dnewtrk(&enamep) != 0)
 		goto LibFail;
	if(!(flags & XPGlobPat) && ((str1 = strchr(fbasename(name,true),'.')) == NULL || strcmp(str1,scriptExt) != 0)) {
		maxlen += scriptExtLen;
		if(dsalloc(enamep,maxlen + 1) != 0)
			goto LibFail;
		sprintf(enamep->d_str,"%s%s",name,scriptExt);
		}
	else
		dsetnull(enamep);

	// If the path begins with '/', check only that.
	if(*name == '/') {
		if(!(flags & XPGlobPat))
			return fexist(name) == 0 ? savepath(pathp,name) :
			 (!disnull(enamep) && fexist(enamep->d_str) == 0) ? savepath(pathp,enamep->d_str) : rc.status;
		return rcset(Failure,0,text407,funcname);
			// "Invalid argument for %s glob search"
		}

	// Check HOME directory (only), if requested.
	if(flags & XPHomeOnly) {
		if((str1 = getenv("HOME")) != NULL) {
			char pathbuf[strlen(str1) + maxlen + 2];

			// Build pathname and check it.
			sprintf(pathbuf,"%s/%s",str1,name);
			if(fexist(pathbuf) == 0)
				return savepath(pathp,pathbuf);
			}
		return rc.status;
		}

	// All special cases checked.  Create name list (filenames to check) and scan the execpath directories.
	char *namelist[] = {
		name,enamep->d_str,NULL};
	for(str2 = execpath; *str2 != '\0';) {
		char *dirsep;

		// Build next possible file spec or glob pattern.
		str1 = str2;
		while(*str2 != '\0' && *str2 != ':')
			++str2;

		// Skip null directory if requested.
		if(!(flags & XPSkipNull) || str2 > str1) {

			// Add a terminating dir separator if needed.
			dirsep = "";
			if(str2 > str1 && str2[-1] != '/')
				dirsep = "/";

			// Loop through filename(s).
			int r;
			char pathbuf[str2 - str1 + maxlen + 2];
			np = namelist;
			do {
				if((*np)[0] != '\0') {
					sprintf(pathbuf,"%.*s%s%s",(int) (str2 - str1),str1,dirsep,*np);
					if(flags & XPGlobPat) {
						if((r = glob(pathbuf,GLOB_MARK,NULL,gp)) != 0 && r != GLOB_NOMATCH) {
							return rcset(errno == GLOB_NOSPACE ? FatalError : Failure,0,text406,
											// "Glob of pattern \"%s\" failed: %s"
							 pathbuf,strerror(errno));
							}
						if(gp->gl_pathc > 0)
							return rc.status;
						}
					else if(fexist(pathbuf) == 0)
						return savepath(pathp,pathbuf);
					}
				} while(*++np != NULL);
			}

		// Not found.  Onward...
		if(*str2 == ':')
			++str2;
		}

	// No such luck.
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Get absolute pathname of "fname" and return in "pathp".  Resolve it if it's a symbolic link and "resolve" is true.  Return
// status.
int getpath(char *fname,bool resolve,Datum *pathp) {
	char buf[MaxPathname + 1];

	if(!resolve) {
		struct stat s;

		if(lstat(fname,&s) != 0)
			goto ErrRetn;
		if((s.st_mode & S_IFMT) != S_IFLNK)
			goto RegFile;

		// File is a symbolic link.  Get pathname of parent directory and append filename.
		DStrFab sf;
		char *str = fbasename(fname,true);
		char bn[strlen(str) + 1];
		strcpy(bn,str);
		if(dopenwith(&sf,pathp,SFClear) != 0)
			goto LibFail;
		if(realpath(fname = fdirname(fname,1),buf) == NULL)
			goto ErrRetn;
		if(dputs(buf,&sf) != 0 || (strcmp(buf,"/") != 0 && dputc('/',&sf) != 0) ||
		 dputs(bn,&sf) != 0 || dclose(&sf,sf_string) != 0)
			goto LibFail;
		}
	else {
RegFile:
		if(realpath(fname,buf) == NULL)
ErrRetn:
			return rcset(errno == ENOMEM ? FatalError : Failure,0,text33,text37,fname,strerror(errno));
				// "Cannot get %s of file \"%s\": %s","pathname"
		if(dsetstr(buf,pathp) != 0)
LibFail:
			(void) librcset(Failure);
		}

	return rc.status;
	}

// Open directory for filename retrieval, given complete or partial pathname (which may end with a slash).  Initialize *fp to
// static pointer to pathnames to be returned by ereaddir().  Return status.
int eopendir(char *fspec,char **fp) {
	char *fn,*term;
	size_t preflen;

	// Find directory prefix.  Terminate after slash if Unix root directory; otherwise, at rightmost slash.
	term = ((fn = fbasename(fspec,true)) > fspec && fn - fspec > 1) ? fn - 1 : fn;

	// Get space for directory name plus maximum filename.
	if(rdbuf != NULL)
		(void) free((void *) rdbuf);
	if((rdbuf = (char *) malloc((preflen = (term - fspec)) + MaxFilename + 3)) == NULL)
		return rcset(Panic,0,text94,"eopendir");
			// "%s(): Out of memory!"
	stplcpy(*fp = rdbuf,fspec,preflen + 1);

	// Open the directory.
	if(dirp != NULL) {
		closedir(dirp);
		dirp = NULL;
		}
	if((dirp = opendir(*rdbuf == '\0' ? "." : rdbuf)) == NULL)
		return rcset(Failure,0,text88,rdbuf,strerror(errno));
			// "Cannot read directory \"%s\": %s"

	// Set rdname, restore trailing slash in pathname if applicable, and return.
	if(fn > fspec)
		*((rdname = rdbuf + (fn - fspec)) - 1) = '/';
	else
		rdname = rdbuf;

	return rc.status;
	}

// Get next filename from directory opened with eopendir().  Return NotFound if none left.
int ereaddir(void) {
	struct DIRENTRY *dp;
	struct stat fstat;

	// Call for the next file.
	do {
		errno = 0;
		if((dp = readdir(dirp)) == NULL) {
			if(errno == 0) {
				closedir(dirp);
				(void) free((void *) rdbuf);
				dirp = NULL;
				rdname = rdbuf = NULL;
				return NotFound;	// No entries left.
				}
			*rdname = '\0';
			return rcset(Failure,0,text88,rdbuf,strerror(errno));
				// "Cannot read directory \"%s\": %s"
			}
		strcpy(rdname,dp->d_name);
		if(stat(rdbuf,&fstat) != 0)
			return rcset(Failure,0,text33,text163,rdbuf,strerror(errno));
					// "Cannot get %s of file \"%s\": %s","status"

		// Skip all entries except regular files and directories.
		} while(((fstat.st_mode & S_IFMT) & (S_IFREG | S_IFDIR)) == 0);

	// If this entry is a directory name, say so.
	if((fstat.st_mode & S_IFMT) == S_IFDIR)
		strcat(rdname,"/");

	// Return result.
	return rc.status;
	}
