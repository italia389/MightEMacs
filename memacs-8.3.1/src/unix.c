// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// unix.c	Unix driver functions for MightEMacs.
//
//
// New features:
//
// 1. Timeouts waiting on a function key have been changed from 35000 to 500000 microseconds.
//
// 2. Additional keymapping entries can be made from the command language by issuing a 'set $palette xxx'.
//    The format of xxx is a string as follows:
//	"KEYMAP keybinding escape-sequence".
//    For example, to add "<ESC><[><A>" as a keybinding of FNN, issue:
//	"KEYMAP FNN ^[[A".
//    Note that the <ESC> is a real escape character and it's pretty difficult to enter.
//
// 3. Colors are supported.  Under AIX the colors will be pulled in automatically.  For other environments, you can either
//    add the termcap entries C0 to D7, or the colors may be defined using the command language by issuing a
//    'set $palette xxx' command.  The format of xxx is a string as follows:
//	"CLRMAP	# escape-sequence".
//    The number is a number from 0 to 15, where 0 to 7 are the foreground colors, and 8 to 15 are background colors.
//    For example, to add foreground color 0 for ANSI terminals, issue:
//	"CLRMAP 0 ^[[30m".
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
#include "edef.h"			// Emacs definitions.

// Kill predefined macro(s).
#undef CTRL				// Problems with CTRL.

#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>			// System type definitions.
#include <errno.h>
#if OSX
#include <sys/syslimits.h>
#endif
#include <sys/stat.h>			// File status definitions.
#include <sys/ioctl.h>			// I/O control definitions.
#include <fcntl.h>			// File descriptor definitions.
#include <string.h>

// Additional include files.
#if CENTOS || DEBIAN
#include <time.h>
#elif BSD || OSX
#include <sys/time.h>			// Timer definitions.
#endif
#include <signal.h>			// Signal definitions.
#if HPUX8 || HPUX || USG
#include <termio.h>			// Terminal I/O definitions.
#elif CENTOS || DEBIAN || OSX
#include <termios.h>			// Terminal I/O definitions.
#endif
#if TT_CURSES || (TT_TERMCAP && (CENTOS || DEBIAN || OSX))
#include <curses.h>			// Curses screen output.
#endif
#if TT_TERMCAP && (CENTOS || DEBIAN || OSX)
#include <term.h>
#endif

// Completion include files.
// Directory accessing: try and figure this out ... if you can!
#if BSD || CENTOS || DEBIAN || OSX
#include <sys/dir.h>			// Directory entry definitions.
#define DIRENTRY	direct
#elif HPUX8 || HPUX || USG
#include <dirent.h>			// Directory entry definitions.
#define DIRENTRY	dirent
#endif

// Restore predefined macro(s).
#undef CTRL				// Restore CTRL.
#define CTRL	0x0100

#include "efunc.h"			// Function definitions.
#include "elang.h"			// Language definitions.
#include "edata.h"			// Global variable definitions.

// Parameters.
#define NKEYENT		300		// Number of keymap entries.
#define NINCHAR		64		// Input buffer size.
#define NOUTCHAR	256		// Output buffer size.
#if TT_TERMCAP
#define NCAPBUF		1024		// Termcap storage size.
#endif
#define RESTORE		1		// Restore original display on exit.
#define RESET		0		// Get and send "reset" command on exit.

// Constants.
#define TIMEOUT		255		// No character available.

// Type definitions.
struct keyent {				// Key mapping entry.
	struct keyent *samlvlp;		// Character on same level.
	struct keyent *nxtlvlp;		// Character on next level.
	char ch;			// Character.
	int ek;				// Resulting keycode (extended key).
	};
#if TT_TERMCAP
struct capbind {			// Capability binding entry.
	char *name;			// Termcap name.
	char *store;			// Storage variable.
	};
#endif
#if TT_TERMCAP || TT_CURSES
struct tkeybind {			// Keybinding entry.
	char *name;			// Termcap name.
	int ek;				// Keycode (extended key).
	};
#endif

/*** Local declarations ***/

static char *ttypath = "/dev/tty";
#if TT_TERMCAP && RESET
static char *reset = NULL;		// Reset string.
#endif
#if BSD
static struct sgttyb cursgtty;		// Current modes.
static struct sgttyb oldsgtty;		// Original modes.
static struct tchars oldtchars;		// Current tchars.
static struct ltchars oldlchars;	// Current ltchars.
static char blank[6] =			// Blank out character set.
	{-1,-1,-1,-1,-1,-1};
#endif
#if HPUX8 || HPUX || USG
static struct termio curterm;		// Current modes.
static struct termio oldterm;		// Original modes.
#elif CENTOS || DEBIAN || OSX
static struct termios curterm;		// Current modes.
static struct termios oldterm;		// Original modes.
#endif
#if TT_TERMCAP
static char tcapbuf[NCAPBUF];		// Termcap character storage.
#define CAP_CL		0		// Clear to end of page.
#define CAP_CM		1		// Cursor motion.
#define CAP_CE		2		// Clear to end of line.
#define CAP_SE		3		// Standout ends.
#define CAP_SO		4		// Standout (reverse video).
#define CAP_IS		5		// Initialize screen.
#define CAP_KS		6		// Keypad mode starts.
#define CAP_KE		7		// Keypad mode ends.
#define CAP_VB		8		// Visible bell.
#define CAP_TI		9		// Terminal initialize.
#define CAP_TE		10		// Terminal end.
#if COLOR
#define CAP_C0		11		// Foreground color #0.
#define CAP_C1		12		// Foreground color #1.
#define CAP_C2		13		// Foreground color #2.
#define CAP_C3		14		// Foreground color #3.
#define CAP_C4		15		// Foreground color #4.
#define CAP_C5		16		// Foreground color #5.
#define CAP_C6		17		// Foreground color #6.
#define CAP_C7		18		// Foreground color #7.
#define CAP_D0		19		// Background color #0.
#define CAP_D1		20		// Background color #1.
#define CAP_D2		21		// Background color #2.
#define CAP_D3		22		// Background color #3.
#define CAP_D4		23		// Background color #4.
#define CAP_D5		24		// Background color #5.
#define CAP_D6		25		// Background color #6.
#define CAP_D7		26		// Background color #7.
#if CENTOS || DEBIAN || OSX || USG
#define CAP_SF		27		// Set foreground color.
#define CAP_SB		28		// Set background color.
#endif
#endif // COLOR.
static struct capbind capbind[] = {	// Capability binding list.
	{ "cl" },			// Clear to end of page.
	{ "cm" },			// Cursor motion.
	{ "ce" },			// Clear to end of line.
	{ "se" },			// Standout ends.
	{ "so" },			// Standout (reverse video).
	{ "is" },			// Initialize screen.
	{ "ks" },			// Keypad mode starts.
	{ "ke" },			// Keypad mode ends.
	{ "vb" },			// Visible bell.
	{ "ti" },			// Terminal initialize.
	{ "te" },			// Terminal end.
#if COLOR
	{ "c0" },			// Foreground color #0.
	{ "c1" },			// Foreground color #1.
	{ "c2" },			// Foreground color #2.
	{ "c3" },			// Foreground color #3.
	{ "c4" },			// Foreground color #4.
	{ "c5" },			// Foreground color #5.
	{ "c6" },			// Foreground color #6.
	{ "c7" },			// Foreground color #7.
	{ "d0" },			// Background color #0.
	{ "d1" },			// Background color #1.
	{ "d2" },			// Background color #2.
	{ "d3" },			// Background color #3.
	{ "d4" },			// Background color #4.
	{ "d5" },			// Background color #5.
	{ "d6" },			// Background color #6.
	{ "d7" },			// Background color #7.
#if CENTOS || DEBIAN || OSX || USG
	{ "Sf" },			// Set foreground color.
	{ "Sb" },			// Set background color.
#endif
#endif // COLOR.
	};
#if COLOR
static int cfcolor = -1;		// Current forground color.
static int cbcolor = -1;		// Current background color.
#endif
static struct tkeybind tkeybind[] = {	// Keybinding list.
	{ "bt",SHFT|CTRL|'I' },		// Back-tab key.
	{ "k1",FKEY|'1'	},		// F1 key.
	{ "k2",FKEY|'2'	},		// F2 key.
	{ "k3",FKEY|'3'	},		// F3 key.
	{ "k4",FKEY|'4'	},		// F4 key.
	{ "k5",FKEY|'5'	},		// F5 key.
	{ "k6",FKEY|'6'	},		// F6 key.
	{ "k7",FKEY|'7'	},		// F7 key.
	{ "k8",FKEY|'8'	},		// F8 key.
	{ "k9",FKEY|'9'	},		// F9 key.
	{ "k;",FKEY|'0'	},		// F10 key.
	{ "F1",FKEY|'a'	},		// F11 key.
	{ "F2",FKEY|'b'	},		// F12 key.
	{ "F3",FKEY|'c'	},		// F13 key.
	{ "F4",FKEY|'d'	},		// F14 key.
	{ "F5",FKEY|'e'	},		// F15 key.
	{ "F6",FKEY|'f'	},		// F16 key.
	{ "F7",FKEY|'g'	},		// F17 key.
	{ "F8",FKEY|'h'	},		// F18 key.
	{ "F9",FKEY|'i'	},		// F19 key.
	{ "FA",FKEY|'j'	},		// F20 key.

	{ "FB",SHFT|FKEY|'1' },		// Shift-F1 key.
	{ "FC",SHFT|FKEY|'2' },		// Shift-F2 key.
	{ "FD",SHFT|FKEY|'3' },		// Shift-F3 key.
	{ "FE",SHFT|FKEY|'4' },		// Shift-F4 key.
	{ "FF",SHFT|FKEY|'5' },		// Shift-F5 key.
	{ "FG",SHFT|FKEY|'6' },		// Shift-F6 key.
	{ "FH",SHFT|FKEY|'7' },		// Shift-F7 key.
	{ "FI",SHFT|FKEY|'8' },		// Shift-F8 key.
	{ "FJ",SHFT|FKEY|'9' },		// Shift-F9 key.
	{ "FK",SHFT|FKEY|'0' },		// Shift-F10 key.
	{ "FL",SHFT|FKEY|'a' },		// Shift-F11 key.
	{ "FM",SHFT|FKEY|'b' },		// Shift-F12 key.
	{ "FN",SHFT|FKEY|'c' },		// Shift-F13 key.
	{ "FO",SHFT|FKEY|'d' },		// Shift-F14 key.
	{ "FP",SHFT|FKEY|'e' },		// Shift-F15 key.
	{ "FQ",SHFT|FKEY|'f' },		// Shift-F16 key.
	{ "FR",SHFT|FKEY|'g' },		// Shift-F17 key.
	{ "FS",SHFT|FKEY|'h' },		// Shift-F18 key.
	{ "FT",SHFT|FKEY|'i' },		// Shift-F19 key.
	{ "FU",SHFT|FKEY|'j' },		// Shift-F20 key.
	{ "%e",SHFT|FKEY|'P' },		// Shift-up arrow key.
	{ "%c",SHFT|FKEY|'N' },		// Shift-down arrow key.
	{ "%i",SHFT|FKEY|'F' },		// Shift-right arrow key.
	{ "#4",SHFT|FKEY|'B' },		// Shift-left arrow key.
	{ "FV",FKEY|'k'	},		// Other function key 1.
	{ "FW",FKEY|'l'	},		// Other function key 2.
	{ "FX",FKEY|'m'	},		// Other function key 3.
	{ "FY",FKEY|'n'	},		// Other function key 4.
	{ "FZ",FKEY|'o'	},		// Other function key 5.
	{ "Fa",FKEY|'p'	},		// Other function key 6.
	{ "Fb",FKEY|'q'	},		// Other function key 7.
	{ "Fc",FKEY|'r'	},		// Other function key 8.
	{ "Fd",FKEY|'s'	},		// Other function key 9.
	{ "Fe",FKEY|'t'	},		// Other function key 10.

	{ "Ff",SHFT|FKEY|'k' },		// Shift-other key 1.
	{ "Fg",SHFT|FKEY|'l' },		// Shift-other key 2.
	{ "Fh",SHFT|FKEY|'m' },		// Shift-other key 3.
	{ "Fi",SHFT|FKEY|'n' },		// Shift-other key 4.
	{ "Fj",SHFT|FKEY|'o' },		// Shift-other key 5.
	{ "Fk",SHFT|FKEY|'p' },		// Shift-other key 6.
	{ "Fl",SHFT|FKEY|'q' },		// Shift-other key 7.
	{ "Fm",SHFT|FKEY|'r' },		// Shift-other key 8.
	{ "Fn",SHFT|FKEY|'s' },		// Shift-other key 9.
	{ "Fo",SHFT|FKEY|'t' },		// Shift-other key 10.

	{ "kA",CTRL|'O'	},		// Insert line key.
	{ "kb",CTRL|'H'	},		// Backspace key.
	{ "kC",CTRL|'L'	},		// Clear screen key.
	{ "kD",FKEY|'D'	},		// Delete character key.
	{ "kd",FKEY|'N'	},		// Down arrow key.
	{ "kE",CTRL|'K'	},		// Clear to end of line key.
	{ "kF",CTRL|'V'	},		// Scroll forward key.
	{ "kH",FKEY|'>'	},		// End key.
	{ "@7",FKEY|'>'	},		// End key.
	{ "kh",FKEY|'<'	},		// Home key.
	{ "kI",FKEY|'C'	},		// Insert character key.
	{ "kL",CTRL|'K'	},		// Delete line key.
	{ "kl",FKEY|'B'	},		// Left arrow key.
	{ "kN",FKEY|'V'	},		// Next page key.
	{ "kP",FKEY|'Z'	},		// Previous page key.
	{ "kR",CTRL|'Z'	},		// Scroll backward key.
	{ "kr",FKEY|'F'	},		// Right arrow key.
	{ "ku",FKEY|'P'	}		// Up arrow key.
	};
#endif // TT_TERMCAP
#if TT_CURSES
static struct tkeybind tkeybind[] = {	// Keybinding list.
	{ "cbt",SHFT|CTRL|'I' },	// Back-tab key.
	{ "kf1",FKEY|'1' },		// F1 key.
	{ "kf2",FKEY|'2' },		// F2 key.
	{ "kf3",FKEY|'3' },		// F3 key.
	{ "kf4",FKEY|'4' },		// F4 key.
	{ "kf5",FKEY|'5' },		// F5 key.
	{ "kf6",FKEY|'6' },		// F6 key.
	{ "kf7",FKEY|'7' },		// F7 key.
	{ "kf8",FKEY|'8' },		// F8 key.
	{ "kf9",FKEY|'9' },		// F9 key.
	{ "kf0",FKEY|'0' },		// F0 or F10 key.
	{ "kf10",FKEY|'0' },		// F0 or F10 key.
	{ "kf11",SHFT|FKEY|'1' },	// Shift-F1 or F11 key.
	{ "kf12",SHFT|FKEY|'2' },	// Shift-F2 or F12 key.
	{ "kf13",SHFT|FKEY|'3' },	// Shift-F3 or F13 key.
	{ "kf14",SHFT|FKEY|'4' },	// Shift-F4 or F14 key.
	{ "kf15",SHFT|FKEY|'5' },	// Shift-F5 or F15 key.
	{ "kf16",SHFT|FKEY|'6' },	// Shift-F6 or F16 key.
	{ "kf17",SHFT|FKEY|'7' },	// Shift-F7 or F17 key.
	{ "kf18",SHFT|FKEY|'8' },	// Shift-F8 or F18 key.
	{ "kf19",SHFT|FKEY|'9' },	// Shift-F9 or F19 key.
	{ "kf20",SHFT|FKEY|'0' },	// Shift-F0 or F20 key.
	{ "kil1",CTRL|'O' },		// Insert line key.
	{ "kbs",CTRL|'H' },		// Backspace key.
	{ "kclr",CTRL|'L' },		// Clear screen key.
	{ "kdch1",FKEY|'D' },		// Delete character key.
	{ "kcud1",FKEY|'N' },		// Down arrow key.
	{ "kel",CTRL|'K' },		// Clear to end of line key.
	{ "kind",CTRL|'V' },		// Scroll forward key.
	{ "kll",FKEY|'>' },		// Home down key.
	{ "kend",FKEY|'>' },		// Home down key.
	{ "khome",FKEY|'<' },		// Home key.
	{ "kich1",FKEY|'C' },		// Insert character key.
	{ "kdl1",CTRL|'K' },		// Delete line key.
	{ "kcub1",FKEY|'B' },		// Left arrow key.
	{ "knp",FKEY|'V' },		// Next page key.
	{ "kpp",FKEY|'Z' },		// Previous page key.
	{ "kri",CTRL|'Z' },		// Scroll backward key.
	{ "kcuf1",FKEY|'F' },		// Right arrow key.
	{ "kcuu1",FKEY|'P' }		// Up arrow key.
	};
#endif // TT_CURSES
static int inbuf[NINCHAR];		// Input buffer.
static int *inbufh = inbuf;		// Head of input buffer.
static int *inbuft = inbuf;		// Tail of input buffer.
#if TT_TERMCAP
static char outbuf[NOUTCHAR];		// Output buffer.
static char *outbuft = outbuf;		// Output buffer tail.
#endif
static char keyseq[256];		// Prefix escape sequence table.
static struct keyent keymap[NKEYENT];	// Key map.
static struct keyent *nxtkeyp = keymap;	// Next free key entry.
static DIR *dirp = NULL;		// Current directory stream.
static char *rdbufp = NULL;		// ereaddir() file buffer (allocated from heap).
static char *rdnamep = NULL;		// Pointer past end of path in rdbuf.
#if TT_CURSES
static char name_addch[] = "addch";
#endif
static char name_ioctl[] = "ioctl";
static char name_tcsetattr[] = "tcsetattr";

// Terminal definition block.
static int ttgetc(int *cp),ttputc(),ttflush();
static int scopen(),scclose(),scmove(),scbeep(),sceeol(),sceeop(),screv(),sckopen(),sckclose();
#if COLOR
static int scfcol(),scbcol();
#endif // COLOR.

// Global variables.
ETerm term = {
	TT_MAXCOLS,			// Maximum number of columns.
	0,				// Current number of columns.
	TT_MAXROWS,			// Maximum number of rows.
	0,				// Current number of rows.
	0,				// Margin for extending lines.
	0,				// Scroll size for extending.
	scopen,				// Open terminal routine.
	scclose,			// Close terminal routine.
	sckopen,			// Open keyboard routine.
	sckclose,			// Close keyboard routine.
	ttgetc,				// Get character routine.
	ttputc,				// Put character routine.
	ttflush,			// Flush output routine.
	scmove,				// Move cursor routine.
	sceeol,				// Erase to end of line routine.
	sceeop,				// Erase to end of page routine.
	sceeop,				// Clear the desktop.
	scbeep,				// Beep! routine.
	screv,				// Set reverse video routine.
#if COLOR
	scfcol,				// Set forground color routine.
	scbcol,				// Set background color routine.
#endif
	};
#if HPUX8 || HPUX
int hpterm;				// Global flag braindead HP-terminal.
#endif

// Open terminal device.  Return status.
static int ttopen(void) {
	static char myname[] = "ttopen";

#if BSD
	// Get tty modes.
	if(ioctl(0,TIOCGETP,&oldsgtty) == -1 || ioctl(0,TIOCGETC,&oldtchars) == -1 ||
	 ioctl(0,TIOCGLTC,&oldlchars) == -1)
		return rcset(OSERROR,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"

	// Save to original mode variables.
	cursgtty = oldsgtty;

	// Set new modes.
	cursgtty.sg_flags |= CBREAK;
	cursgtty.sg_flags &= ~(ECHO | CRMOD);

	// Set tty modes.
	if(ioctl(0,TIOCSETP,&cursgtty) == -1 || ioctl(0,TIOCSETC,blank) == -1 || ioctl(0,TIOCSLTC,blank) == -1)
		return rcset(OSERROR,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"
#elif HPUX8 || HPUX || USG

	// Get tty modes.
	if(ioctl(0,TCGETA,&oldterm) == -1)
		return rcset(OSERROR,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"

	// Save to original mode variable.
	curterm = oldterm;

	// Set new modes.
	// I do not believe the flow control settings of the OS should be diddled by an application program.  But if you do,
	// change this 1 to a 0.  But be warned, all sorts of terminals will experience grief with this.
#if 0
	curterm.c_iflag &= ~(INLCR | ICRNL | IGNCR);
#else
	curterm.c_iflag &= ~(INLCR | ICRNL | IGNCR | IXON | IXANY | IXOFF);
#endif
	curterm.c_lflag &= ~(ICANON | ISIG | ECHO);
	curterm.c_cc[VMIN] = 1;
	curterm.c_cc[VTIME] = 0;

	// Set tty mode.
	if(ioctl(0,TCSETA,&curterm) == -1)
		return rcset(OSERROR,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"
#elif CENTOS || DEBIAN || OSX
	// Get tty modes.
	if(tcgetattr(0,&oldterm))
		return rcset(OSERROR,0,text44,"tcgetattr",myname);
			// "calling %s() from %s() function"

	// Save to original mode variable.
	curterm = oldterm;

	// Set new modes.
	curterm.c_iflag &= ~(INLCR | ICRNL | IGNCR | IXON | IXANY | IXOFF);
	curterm.c_lflag &= ~(ICANON | ISIG | ECHO | IEXTEN);
	curterm.c_cc[VMIN] = 1;		// Minimum number of characters for noncanonical read.
	curterm.c_cc[VTIME] = 0;	// Timeout in deciseconds for noncanonical read.

	// Set tty mode.
	if(tcsetattr(0,TCSANOW,&curterm) == -1)
		return rcset(OSERROR,0,text44,name_tcsetattr,myname);
			// "calling %s() from %s() function"
#endif
	// Success.
	return rc.status;
	}

// Close terminal device.  Return status.
static int ttclose(void) {
	static char myname[] = "ttclose";

#if TT_TERMCAP && RESET
	// Restore original terminal modes.
	if(reset != NULL)
		(void) write(1,reset,strlen(reset));
#endif
#if BSD
	if(ioctl(0,TIOCSETP,&oldsgtty) == -1 || ioctl(0,TIOCSETC,&oldtchars) == -1 ||
	 ioctl(0,TIOCSLTC,&oldlchars) == -1)
		return rcset(OSERROR,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"
#elif HPUX8 || HPUX || USG
	if(ioctl(0,TCSETA,&oldterm) == -1)
		return rcset(OSERROR,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"
#elif CENTOS || DEBIAN || OSX
	// Set tty mode.
	if(tcsetattr(0,TCSANOW,&oldterm) == -1)
		return rcset(OSERROR,0,text44,name_tcsetattr,myname);
			// "calling %s() from %s() function"
#endif
	// Success.
	return rc.status;
	}

// Flush output buffer to display.  Return status.
static int ttflush(void) {
#if TT_TERMCAP
	int len;

	// Compute length of write.
	if((len = outbuft - outbuf) == 0)
		return rc.status;

	// Reset buffer position.
	outbuft = outbuf;

	// Perform write to screen.
	if(write(1,outbuf,len) == -1)
		return rcset(OSERROR,0,text44,"write","ttflush");
			// "calling %s() from %s() function"
#elif TT_CURSES
	if(refresh() == ERR)
		return rcset(OSERROR,0,text44,"refresh","ttflush");
			// "calling %s() from %s() function"
#endif
	return rc.status;
	}

// Put character onto display.  Return status.
static int ttputc(int ch) {

#if TT_TERMCAP
	// Check for buffer full.
	if(outbuft == outbuf + sizeof(outbuf) && ttflush() != SUCCESS)
		return rc.status;

	// Add to buffer.
	*outbuft++ = ch;
#elif TT_CURSES
	// Put character on screen.
	if(addch(ch) == ERR)
		return rcset(OSERROR,0,text44,name_addch,"ttputc");
			// "calling %s() from %s() function"
#endif
	return rc.status;
	}

// Add character sequence to keymap table.  "ek" is key code.  Return status.
static int addkey(char *seq,int ek) {
	bool first;
	struct keyent *curp,*nxtcurp;

	// Skip null sequences.
	if(seq == NULL)
		return rc.status;

	// Skip single character sequences.
	if(strlen(seq) <= 1)
		return rc.status;

	// If no keys defined, go directly to insert mode.
	first = true;
	if(nxtkeyp != keymap) {

		// Start at top of key map.
		curp = keymap;

		// Loop until matches exhaust.
		while(*seq != '\0') {

			// Do we match current character?
			if(*seq == curp->ch) {

				// Advance to next level.
				++seq;
				curp = curp->nxtlvlp;
				first = false;
				}
			else {
				// Try next character on same level and stop if none left.
				if((nxtcurp = curp->samlvlp) == NULL)
					break;
				curp = nxtcurp;
				}
			}
		}

	// Check for room in keymap.
	if(strlen(seq) > (ulong) NKEYENT - (nxtkeyp - keymap))
		return rcset(FATALERROR,0,text157,"addkey",NKEYENT);
			// "%s(): key map space (%d entries) exhausted!"

	// If first character in sequence is inserted, add to prefix table.
	if(first)
		keyseq[(int) *seq] = 1;

	// If characters are left over, insert them into list.
	for(first = true; *seq; first = false) {

		// Make new entry.
		nxtkeyp->ch = *seq++;
		nxtkeyp->ek = ek;

		// If root, nothing to do.
		if(nxtkeyp != keymap) {

			// Set first to samlvlp, others to nxtlvlp.
			if(first)
				curp->samlvlp = nxtkeyp;
			else
				curp->nxtlvlp = nxtkeyp;
			}

		// Advance to next key.
		curp = nxtkeyp++;
		}

	return rc.status;
	}

// Grab input characters, with wait.  Return status.
static int grabwait(int *cp) {
	uchar ch;
	static char myname[] = "grabwait";

#if HPUX8 || HPUX || CENTOS || DEBIAN || OSX || USG
	// Change mode, if necessary.
	if(curterm.c_cc[VTIME]) {
		curterm.c_cc[VMIN] = 1;		// Minimum number of characters for noncanonical read.
		curterm.c_cc[VTIME] = 0;	// Timeout in deciseconds for noncanonical read.
#if HPUX8 || HPUX || USG
		if(ioctl(0,TCSETA,&curterm) == -1)
			return rcset(OSERROR,0,text44,name_ioctl,myname);
				// "calling %s() from %s() function"
#else
		if(tcsetattr(0,TCSANOW,&curterm) == -1)
			return rcset(OSERROR,0,text44,name_tcsetattr,myname);
				// "calling %s() from %s() function"
#endif
		}
#endif
	// Perform read.
	if(read(0,(void *) &ch,1) == -1)
		return rcset(OSERROR,0,text44,"read",myname);
			// "calling %s() from %s() function"

	// Return new character.
	*cp = ch;
	return rc.status;
	}

// Grab input characters, short wait.  Return status.
static int grabnowait(int *cp) {
	static char myname[] = "grabnowait";
#if BSD
	static struct timeval timout = {0,500000L};
	int count,r;

	// Select input.
	r = 1;
	count = select(1,&r,NULL,NULL,&timout);
	if(count == 0) {
		*cp = TIMEOUT;
		return rc.status;
		}
	if(count < 0)
		return rcset(OSERROR,0,text44,"select",myname);
			// "calling %s() from %s() function"

	// Perform read.
	return grabwait(cp);

#elif HPUX8 || HPUX || CENTOS || DEBIAN || OSX || USG
	int count;
	uchar ch;

	// Change mode, if necessary.
	if(curterm.c_cc[VTIME] == 0) {
		curterm.c_cc[VMIN] = 0;		// Minimum number of characters for noncanonical read.
		curterm.c_cc[VTIME] = 3;	// Timeout in deciseconds for noncanonical read.
#if HPUX8 || HPUX || USG
		if(ioctl(0,TCSETA,&curterm) == -1)
			return rcset(OSERROR,0,text44,name_ioctl,myname);
				// "calling %s() from %s() function"
#else
		if(tcsetattr(0,TCSANOW,&curterm) == -1)
			return rcset(OSERROR,0,text44,name_tcsetattr,myname);
				// "calling %s() from %s() function"
#endif
		}

	// Perform read.
	count = read(0,(void *) &ch,1);
	if(count == 0)
		ch = TIMEOUT;
	else if(count < 0)
		return rcset(OSERROR,0,text44,"read",myname);
			// "calling %s() from %s() function"

	// Return new character.
	*cp = ch;
	return rc.status;
#endif
	}

// Queue input character.  Return status.
static int qin(int ch) {

	// Check for overflow.
	if(inbuft == inbuf + sizeof(inbuf)) {

		// Annoy user.
		(void) scbeep();
		return rc.status;
		}

	// Add character.
	*inbuft++ = ch;
	return rc.status;
	}

// Cook input characters.  Return status.
static int cook(void) {
	int ch;
	struct keyent *cur;

	// Get first character untimed.
	if(grabwait(&ch) != SUCCESS || qin(ch) != SUCCESS)
		return rc.status;

	// Skip if the key isn't a special leading escape sequence.
	if(keyseq[ch] == 0) {

		// If it is a null, make it a (0/1/32).
		if(ch == '\0') {
			if(qin(CTRL >> 8) == SUCCESS)	// Control.
				(void) qin(32);		// Space.
			}
		return rc.status;
		}

	// Start at root of keymap.
	cur = keymap;

	// Loop until keymap exhausts.
	while(cur != NULL) {

		// Did we find a matching character.
		if(cur->ch == ch) {

			// Is this the end?
			if(cur->nxtlvlp == NULL) {

				// Replace all characters with new sequence.
				inbuft = inbuf;
				(void) qin(cur->ek);
				return rc.status;
				}
			else {
				// Advance to next level.
				cur = cur->nxtlvlp;

				// Get next character, timed, and queue it.
				if(grabnowait(&ch) != SUCCESS || ch == TIMEOUT || qin(ch) != SUCCESS)
					return rc.status;
				}
			}
		else
			// Try next character on same level.
			cur = cur->samlvlp;
		}

	return rc.status;
	}

// Get cooked character and return in *cp if cp is not NULL.  Return status.
static int ttgetc(int *cp) {
	int ch;

	// Loop until character is in input buffer.
	while(inbufh == inbuft)
		if(cook() != SUCCESS)
			return rc.status;

	// Get input from buffer, now that it is available.
	ch = *inbufh++;

	// Reset to the beginning of the buffer if no more pending characters.
	if(inbufh == inbuft)
		inbufh = inbuft = inbuf;

	// Return next character.
	if(cp)
		*cp = ch;
	return rc.status;
	}

#if TYPEAH
// Get count of pending input characters.  Return status.
int typahead(int *countp) {
	int count;

	// See if internal buffer is non-empty.
	if(inbufh != inbuft) {
		*countp = 1;
		return rc.status;
		}

	// Now check with system.
#ifdef FIONREAD

	// Get number of pending characters.
	if(ioctl(0,FIONREAD,&count) == -1)
		return rcset(OSERROR,0,text44,name_ioctl,"typahead");
			// "calling %s() from %s() function"
	*countp = count;
#else

	// Ask hardware for count.
	count = ioctl(0,FIORDCHK,0);
	if(count < 0)
		return rcset(OSERROR,0,text44,name_ioctl,"typahead");
			// "calling %s() from %s() function"
	*countp = count;
#endif
	return rc.status;
	}
#endif // TYPEAH.

#if TT_TERMCAP
// Put out sequence, with padding.  Return status.
static int putpad(char *seq) {

	// Call on termcap to send sequence.
	if(seq != NULL && tputs(seq,1,ttputc) == ERR)
		return rcset(OSERROR,0,text44,"tputs","putpad");
			// "calling %s() from %s() function"

	return rc.status;
	}
#endif

// Build OS error message if caller is not NULL, append TERM to it, and return OSERROR status.
static int termerr(char *caller,char *call) {
	StrList msg;

	if(caller != NULL)
		(void) rcset(OSERROR,0,text44,call,caller);
			// "calling %s() from %s() function"
	return (vopen(&msg,&rc.msg,true) != 0 || vputf(&msg,", TERM '%s'",termp) != 0 || vclose(&msg) != 0) ?
	 vrcset() : rc.status;
	}


// Update terminal size parameters, given number of columns and rows.
void settermsize(ushort ncol,ushort nrow) {

	sampbuf.smallsize = (term.t_ncol = ncol) / 4;
	term.t_nrow = nrow;
	term.t_margin = ncol / 10;			// 10% terminal width.
	term.t_scrsiz = ncol - (term.t_margin * 2);	// 80% terminal width.
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
		return termerr(myname,name_ioctl);

	// Do sanity check.
	if(w.ws_col < TT_MINCOLS || w.ws_row < TT_MINROWS) {
#if TT_CURSES
		(void) endwin();
#endif
		return rcset(FATALERROR,0,text190,w.ws_col,w.ws_row,Myself);
			// "Terminal size %hu x %hu is too small to run %s"
		}

	if((*colp = w.ws_col) > term.t_mcol)
		*colp = term.t_mcol;
	if((*rowp = w.ws_row) > term.t_mrow)
		*rowp = term.t_mrow;
	return rc.status;
	}

// Initialize screen package.  Return status.
static int scopen(void) {
#if TT_TERMCAP || TT_CURSES
	static char myname[] = "scopen";
	ushort ncol,nrow;
	struct tkeybind *kp;
#if TT_TERMCAP
	int status;
	char *strp;
	char tcbuf[1024];
	struct capbind *cb;
#if HPUX8 || HPUX || CENTOS || DEBIAN || OSX

	// For those OSes that don't have these in the termcap library...
	char *UP;
#if 0
	char PC;
	short ospeed;
#endif
#endif
#endif // TT_TERMCAP

	// Get terminal type.
	if((termp = getenv("TERM")) == NULL)
		return rcset(FATALERROR,0,text182);
		// "Environmental variable 'TERM' not defined!"

	// Fix up file descriptors if reading a file from standard input.
	if(!isatty(0)) {
		int fd;

		// File descriptor 0 is not a TTY so it must be a data file or pipe.  Get a new descriptor for it by calling
		// dup() (and saving the result in global stdinfd), then open /dev/tty, and dup2() that FD back to 0.  This is
		// done so that FD 0 is always the keyboard which should ensure that tgetent() will work properly on any brain-
		// dead Unix variants.
		if((stdinfd = dup(0)) == -1)
			return termerr(myname,"dup");
		if((fd = open(ttypath,O_RDONLY)) == -1)
			return termerr(myname,"open");
		if(dup2(fd,0) == -1)
			return termerr(myname,"dup2");
		}

#if TT_TERMCAP
	// Load termcap.
	if((status = tgetent(tcbuf,termp)) == -1)
		return termerr(myname,"tgetent");
	if(status == 0)
		return rcset(FATALERROR,0,text183,termp);
			// "Unknown terminal type '%s'!"
#elif TT_CURSES
	// Initialize screen.
	(void) initscr();
#endif
	// Get terminal size and save it.
	if(gettermsize(&ncol,&nrow) != SUCCESS)
		return rc.status;
	settermsize(ncol,nrow);
#if TT_TERMCAP
	// Start grabbing termcap commands.
	strp = tcapbuf;

#if RESET
	// Get the reset string.
	reset = tgetstr("is",&strp);
#endif
#if 0
	// Get the pad character.
	if(tgetstr("pc",&strp) != NULL)
		PC = tcapbuf[0];
#endif
	// Get up line capability.
	UP = tgetstr("up",&strp);

	// Get other capabilities.
	cb = capbind;
	while(cb < capbind + (sizeof(capbind) / sizeof(*capbind))) {
		cb->store = tgetstr(cb->name,&strp);
		++cb;
		}

	// Check for minimum.
	if(!capbind[CAP_CL].store && (!capbind[CAP_CM].store || !UP))
		return rcset(FATALERROR,0,text192,termp,Myself);
			// "This terminal (type '%s') does not have sufficient capabilities to run %s"

	// Set reverse video and erase to end of line.
	if(capbind[CAP_SO].store && capbind[CAP_SE].store)
		opflags |= OPHAVEREV;
	if(capbind[CAP_CE].store)
		opflags |= OPHAVEEOL;
#endif // TT_TERMCAP

	// Get keybindings.
#if CENTOS || DEBIAN || OSX
	// These keys don't make it into keymap for some reason (in loop below) so we add them here as a workaround.
	addkey("\033[Z",SHFT|CTRL|'I');		// Shift-Tab (bt)
	addkey("\033[1;2D",SHFT|FKEY|'B');	// Shift-left arrow (#4)
	addkey("\033[1;2C",SHFT|FKEY|'F');	// Shift-right arrow (%i)
	addkey("\033[3;2~",SHFT|FKEY|'D');	// Shift-forward delete
#endif
	kp = tkeybind;
	while(kp < tkeybind + (sizeof(tkeybind) / sizeof(*tkeybind))) {
#if TT_TERMCAP
		if(addkey(tgetstr(kp->name,&strp),kp->ek) != SUCCESS)
#else
		if(addkey(tgetstr(kp->name,NULL),kp->ek) != SUCCESS)
#endif
			return rc.status;
		++kp;
		}

#if TT_TERMCAP && (HPUX8 || HPUX)
	// Check for HP-Terminal (so we can label its function keys).
	hpterm = tgetflag("xs");
#endif

	// Open terminal device.
	if(ttopen() != SUCCESS)
		return termerr(NULL,NULL);

#if TT_TERMCAP
#if 0
	// Set speed for padding sequences.
#if BSD
	ospeed = cursgtty.sg_ospeed;
#elif HPUX8 || HPUX || USG
	ospeed = curterm.c_cflag & CBAUD;
#elif CENTOS || DEBIAN || OSX
	ospeed = cfgetospeed(&curterm);
#endif
#endif
	// Send out initialization sequences.
#if RESTORE
	if(putpad(capbind[CAP_TI].store) != SUCCESS ||
#else
	if(putpad(capbind[CAP_IS].store) != SUCCESS ||
#endif
	 putpad(capbind[CAP_KS].store) != SUCCESS || sckopen() != SUCCESS)
		return rc.status;
#endif // TT_TERMCAP
#endif // TT_TERMCAP || TT_CURSES

	// Success.
	opflags |= OPVTOPEN;
	return rc.status;
	}

// Close screen package.  Return status.
static int scclose(void) {

#if TT_TERMCAP
	// Turn off keypad mode.
	if(putpad(capbind[CAP_KE].store) != SUCCESS || sckclose() != SUCCESS)
		return rc.status;

	// Close terminal device.
	if(
#if RESTORE
	 putpad(capbind[CAP_TE].store) != SUCCESS ||
#endif
	 ttflush() != SUCCESS || ttclose() != SUCCESS)
#endif
		return rc.status;

#if TT_CURSES
	// Turn off curses.
	(void) endwin();

	// Close terminal device.
	if(ttflush() != SUCCESS || ttclose() != SUCCESS)
		return rc.status;
#endif
	// Success.
	return rc.status;
	}

// Open keyboard.  Return status.
static int sckopen(void) {

#if TT_TERMCAP
	if(putpad(capbind[CAP_KS].store) == SUCCESS)
		(void) ttflush();
	return rc.status;
#endif
	}

// Close keyboard.  Return status.
static int sckclose(void) {

#if TT_TERMCAP
	if(putpad(capbind[CAP_KE].store) == SUCCESS)
		(void) ttflush();
	return rc.status;
#endif
	}

// Move cursor.  Return status.
static int scmove(int row,int col) {
	char *tgoto();

#if TT_TERMCAP
	// Call on termcap to create move sequence.
	if(putpad(tgoto(capbind[CAP_CM].store,col,row)) != SUCCESS)
		return rc.status;
#elif TT_CURSES
	if(move(row,col) == ERR)
		return rcset(OSERROR,0,text44,"move","scmove");
			// "calling %s() from %s() function"
#endif
	// Success.
	return rc.status;
	}

// Erase to end of line.  Return status.
static int sceeol(void) {

#if TT_TERMCAP
	// Send erase sequence.
	if(putpad(capbind[CAP_CE].store) != SUCCESS)
		return rc.status;
#elif TT_CURSES
	if(clrtoeol() == ERR)
		return rcset(OSERROR,0,text44,"clrtoeol","sceeol");
			// "calling %s() from %s() function"
#endif
	// Success.
	return rc.status;
	}

// Clear screen.  Return status.
static int sceeop(void) {

#if TT_TERMCAP
#if COLOR
	if(scfcol(gfcolor) != SUCCESS || scbcol(gbcolor) != SUCCESS)
		return rc.status;
#endif
	// Send clear sequence.
	if(putpad(capbind[CAP_CL].store) != SUCCESS)
		return rc.status;
#elif TT_CURSES
	if(erase() == ERR)
		return rcset(OSERROR,0,text44,"erase","sceeop");
			// "calling %s() from %s() function"
#endif
	// Success.
	return rc.status;
	}

// Set reverse video state, given current state.  Return status.
static int screv(int state) {
#if TT_TERMCAP
#if COLOR
	int ftmp,btmp;		// Temporaries for colors.
#endif

	// Set reverse video state.
	if(putpad(state ? capbind[CAP_SO].store : capbind[CAP_SE].store) != SUCCESS)
		return rc.status;

#if COLOR
	if(state == false) {
		ftmp = cfcolor;
		btmp = cbcolor;
		cfcolor = -1;
		cbcolor = -1;
		if(scfcol(ftmp) != SUCCESS || scbcol(btmp) != SUCCESS)
			return rc.status;
		}
#endif
#endif

#if TT_CURSES
	if(state)
		standout();
	else
		standend();
#endif
	// Success.
	return rc.status;
	}

// Beep.  Return status.
static int scbeep(void) {

#if TT_TERMCAP
#if VIZBELL
	// Send out visible bell, if it exists.
	if(capbind[CAP_VB].store) {
		if(putpad(capbind[CAP_VB].store) != SUCCESS)
			return rc.status;
		}
	else
#endif
		// The old standby method.
		if(ttputc('\7') != SUCCESS)
			return rc.status;
#endif

#if TT_CURSES
	if(addch('\7') == ERR)
		return rcset(OSERROR,0,text44,name_addch,"scbeep");
			// "calling %s() from %s() function"
#endif
	// Success.
	return rc.status;
	}

#if COLOR
static char cmap[8] = {0,4,2,6,1,5,3,7};

// Set foreground color.  Return status.
static int scfcol(int color) {

#if TT_TERMCAP
	// Skip if already the correct color.
	if(color == cfcolor)
		return rc.status;

	// Send out color sequence.
	if(capbind[CAP_C0].store) {
		if(putpad(capbind[CAP_C0 + (color & 7)].store) != SUCCESS)
			return rc.status;
		cfcolor = color;
		}
#if CENTOS || DEBIAN || OSX || USG
	else if(capbind[CAP_SF].store) {
		if(putpad(tparm(capbind[CAP_SF].store,cmap[color & 7])) != SUCCESS)
			return rc.status;
		cfcolor = color;
		}
#endif
#endif

#if TT_CURSES
	// ?
#endif
	return rc.status;
	}

// Set background color.  Return status.
static int scbcol(int color) {

#if TT_TERMCAP
	// Skip if already the correct color.
	if(color == cbcolor)
		return rc.status;

	// Send out color sequence.
	if(capbind[CAP_C0].store) {
		if(putpad(capbind[CAP_D0 + (color & 7)].store) != SUCCESS)
			return rc.status;
		cbcolor = color;
		}
#if CENTOS || DEBIAN || OSX || USG
	else if(capbind[CAP_SB].store) {
		if(putpad(tparm(capbind[CAP_SB].store,cmap[color & 7])) != SUCCESS)
			return rc.status;
		cbcolor = color;
		}
#endif
#endif

#if TT_CURSES
	// ?
#endif
	return rc.status;
	}

// Set palette, given command.  Return status.
int spal(char *cmd) {
	int code;
	int dokeymap;
	char *strp;

	// Check for keymapping command.
	if(strncmp(cmd,"KEYMAP ",7) == 0)
		dokeymap = true;
	else
#if TT_TERMCAP
		if(strncmp(cmd,"CLRMAP ",7) == 0)
			dokeymap = false;
		else
#endif
			return rcset(FAILURE,0,text500,cmd);
				// "Invalid palette value '%s'"
	cmd += 7;

	// Look for a space.
	for(strp = cmd; *strp != '\0'; ++strp)
		if(*strp == ' ') {
			*strp++ = '\0';
			break;
			}
	if(*strp == '\0')
		return rc.status;

	// Perform operation.
	if(dokeymap)

		// Convert to keycode and add to tree.
		if(stoek(cmd,&code) != SUCCESS || addkey(strp,code) != SUCCESS)
			return rc.status;
#if TT_TERMCAP
	else {
		long lval;

		// Convert to color number.
		if(asc_long(cmd,&lval,false) != SUCCESS || (code = lval) < 0 || code > 15)
			return rcset(FAILURE,0,text500,cmd);
				// "Invalid palette value '%s'"

		// Move color code to capability structure.
		if((capbind[CAP_C0 + code].store = (char *) malloc(strlen(strp) + 1)) == NULL)
			return rcset(PANIC,0,text94,"spal");
				// "%s(): Out of memory!"
		strcpy(capbind[CAP_C0 + code].store,strp);
		}
#endif
	return rc.status;
	}
#endif // COLOR.

// Get working directory.  Force system call if force is true.  Return status.
int getwkdir(char **strpp,bool force) {
	static char *wkdir = NULL;

	// Return last one if not a force; otherwise, free it.
	if(wkdir != NULL) {
		if(!force)
			goto retn;
		(void) free((void *) wkdir);
		wkdir = NULL;
		}

	// Get path.
	if((wkdir = getcwd(NULL,0)) == NULL)
		return rcset(OSERROR,0,text44,"getcwd","getwkdir");
			// "calling %s() from %s() function"
retn:
	*strpp = wkdir;
	return rc.status;
	}

// Change working directory.
int changedir(Value *rp,int n) {
	char *pathp;

	// Get directory name.
	if(getarg(rp,text277,NULL,CTRL | 'M',MaxPathname,ARG_FIRST | ARG_STR | ARG_NOTNULL | TERM_C_FNAME) != SUCCESS ||
	 (!(opflags & OPSCRIPT) && vistfn(rp,VNIL)))
			// "Change directory"
		return rc.status;

	// Call system.
	if(chdir(rp->v_strp) != 0)
		return rcset(FAILURE,0,text265,rp->v_strp,strerror(errno));
			// "Cannot change to directory \"%s\": %s"

	// Remember new path and return it (absolute pathname).
	if(getwkdir(&pathp,true) == SUCCESS && vsetstr(pathp,rp) != 0)
		return vrcset();

	// Run change-directory user hook.
	if(exechook(NULL,INT_MIN,hooktab + HKCHDIR,0) != SUCCESS)
		return rc.status;

	// Display new directory if interactive.
	if(!(opflags & OPSCRIPT))
		mlputs(MLHOME,pathp);

	return rc.status;
	}

// Suspend emacs.
int suspendEMacs(Value *rp,int n) {

	// Reset the terminal and go to the last line.
	if(vttidy(false) != SUCCESS)
		return rc.status;

	// Send stop signal to self (suspend)...
	if(kill(getpid(),SIGTSTP) == -1)
		return rcset(OSERROR,0,text44,"kill","suspendEMacs");
			// "calling %s() from %s() function"

	// We should be back here after resuming.

	// Reopen the screen and redraw.
	if(scopen() != SUCCESS)
		return rc.status;
	curwp->w_flags = WFHARD;
	opflags |= OPSCREDRAW;

	// Success.
	return rc.status;
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

// Get time of day.
char *timeset(void) {
	time_t buf;
	char *strp1,*strp2;

#if !(BSD || CENTOS || DEBIAN || OSX)
	char *ctime();
#endif
	// Get system time.
	(void) time(&buf);

	// Pass system time to converter.
	strp1 = ctime(&buf);

	// Eat newline character.
	if((strp2 = strchr(strp1,'\n')) != NULL)
		*strp2 = '\0';

	return strp1;
	}

#if USG || HPUX8
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
#endif // USG || HPUX8.

// Create temporary filename, store in vp, and return status code.
static int tmpfname(Value *vp) {
	static unsigned n = 0U;

	if(vsalloc(vp,24) != 0)
		return excep.code;
	sprintf(vp->v_strp,"/tmp/_mm%u.%u",mypid,n++);
	return 0;
	}

// Call out to system to perform given command.  Return status, and false if error so that macros won't abort.
static int callout(Value *rp,Value *cfp,bool force) {
	int rcode;

	// Close down.
	if(scmove(term.t_nrow - 1,0) != SUCCESS || ttflush() != SUCCESS || sckclose() != SUCCESS || ttclose() != SUCCESS)
		return rc.status;

	// Do command.
	rcode = system(cfp->v_strp);

	// Restart system.
	opflags |= OPSCREDRAW;
	if(sckopen() != SUCCESS || ttopen() != SUCCESS)
		return rc.status;

	// If error occurred, set return value to false.
	if(rcode != 0 && vsetstr(val_false,rp) != 0)
		return vrcset();

	// If interactive, pause to display shell message if error or force.  If script mode, set alert message if error.
	if(!(opflags & OPSCRIPT) && (rcode != 0 || force)) {
		mlputs(MLHOME | MLFORCE | MLWRAP,text188);
			// "End"
		if(ttflush() == SUCCESS)
			(void) ttgetc(NULL);
		}
	else if((opflags & OPSCRIPT) && rcode != 0)
		(void) rcset(SUCCESS,0,text194,cfp->v_strp);
			// "Shell command '%s' failed"

	return rc.status;
	}

// Create subshell (user command).
int shellCLI(Value *rp,int n) {
	char *shpathp;
	Value *vp;

	// Get shell path.
	if((shpathp = getenv("SHELL")) == NULL)
#if BSD
		shpathp = "/bin/csh";
#else
		shpathp = "/bin/sh";
#endif

	// Do shell.
	return vnewstr(&vp,shpathp) != 0 ? vrcset() : callout(rp,vp,false);
	}

// Get Unix command line.  Return status.
static int getcmd(Value **cfpp,char *prmtp) {

	if(vnew(cfpp,false) != 0)
		return vrcset();
	if(opflags & OPSCRIPT) {

		// Concatenate all arguments into *cfpp.
		(void) join(*cfpp,NULL,1,JNKEEPALL);
		}
	else if(terminp(*cfpp,prmtp,NULL,CTRL | 'M',0,ARG_NOTNULL) == SUCCESS && !vistfn(*cfpp,VNIL))
		(void) strval(*cfpp);

	return rc.status;
	}

// Execute a Unix command and return status.  Return nil if error.
int shellCmd(Value *rp,int n) {
	Value *cfp;

	// Get the command line and execute it unless user hit RETURN at prompt.
	if(getcmd(&cfp,"> ") == SUCCESS && !vistfn(cfp,VNIL))
		(void) callout(rp,cfp,true);

	return rc.status;
	}

// Get a shell command, modify it to save results to a temporary file, and execute it (if tfilep2 is NULL).  Set *ucancelp to
// true if user cancels.  Return status.
static int prepcmd(Value *rp,StrList *slp,char *prmtp,bool *ucancelp,Value **tfilepp1,Value **tfilepp2) {
	Value *cfp;

	// Get shell command.
	if(getcmd(&cfp,prmtp) != SUCCESS)
		return rc.status;
	if(vistfn(cfp,VNIL)) {		// User hit RETURN at prompt.
		*ucancelp = true;
		return rc.status;
		}
	if(vnew(tfilepp1,false) != 0 || (tfilepp2 != NULL && vnew(tfilepp2,false) != 0) || vopen(slp,NULL,false) != 0 ||
	 vputs(cfp->v_strp,slp) != 0)
		return vrcset();

	// Modify command to send output to or read input from a temporary file.
	if(vputs(tfilepp2 == NULL ? " >" : " <",slp) != 0 || tmpfname(*tfilepp1) != 0 || vputs((*tfilepp1)->v_strp,slp) != 0)
		return vrcset();

	// Finish it.
	*ucancelp = false;
	if(tfilepp2 != NULL) {
		if(vputs(" >",slp) != 0 || tmpfname(*tfilepp2) != 0 || vputs((*tfilepp2)->v_strp,slp) != 0 || vclose(slp) != 0)
			return vrcset();
		}
	else {
		// Execute it.
		if(vclose(slp) != 0)
			return vrcset();
		if(callout(rp,slp->sl_vp,false) == SUCCESS && vistfn(rp,VFALSE))
			(void) unlink((*tfilepp1)->v_strp);
		}

	return rc.status;
	}

// Execute shell command and insert result into the current buffer.  If zero argument, leave point before the inserted text.
int insertPipe(Value *rp,int n) {
	StrList cmd;
	bool ucancel;
	Value *tfilep;

	// Get pipe-in command and execute it.
	if(prepcmd(rp,&cmd,text249,&ucancel,&tfilep,NULL) == SUCCESS && !ucancel && !vistfn(rp,VFALSE)) {
			// "Insert pipe"

		// Insert the temporary file (command output) and delete it.
		(void) ifile(tfilep->v_strp,n);
		(void) unlink(tfilep->v_strp);
		}

	return rc.status;
	}

// Pipe output of Unix command to a buffer (user command).
int readPipe(Value *rp,int n) {
	StrList cmd;
	bool ucancel;
	Value *tfilep;

	// Get pipe-in command and run it.
	if(prepcmd(rp,&cmd,text170,&ucancel,&tfilep,NULL) == SUCCESS && !ucancel && !vistfn(rp,VFALSE)) {
			// "Read pipe"

		// Return results.
		(void) rdfile(rp,n,tfilep->v_strp,true);
		(void) unlink(tfilep->v_strp);
		}

	return rc.status;
	}

// Pipe current buffer through a shell command (user command).  Set rp to nil if command failed.  Return status.
int pipeBuf(Value *rp,int n) {
	StrList cmd;
	char *fnamep0;
	bool ucancel;
	Value *tfilep1,*tfilep2;

	// Get the command.
	if(prepcmd(rp,&cmd,"| ",&ucancel,&tfilep1,&tfilep2) != SUCCESS || ucancel || vistfn(rp,VFALSE))
		return rc.status;

	// Save the current buffer's filename.
	fnamep0 = curbp->b_fname;		// Save the original (heap) name.
	curbp->b_fname = NULL;

	// Write out the buffer, checking for errors.
	if(writeout(curbp,tfilep1->v_strp,'w') != SUCCESS)
		return rc.status;

	// Execute the shell command.  If successful, read in the file that was created.
	if(callout(rp,cmd.sl_vp,false) == SUCCESS && !vistfn(rp,VFALSE))
		if(readin(curbp,tfilep2->v_strp,false) == SUCCESS) {

			// Mark buffer as changed.
			curbp->b_flags |= BFCHGD;
			}

	// Restore the original filename and get rid of the temporary files.
	curbp->b_fname = fnamep0;
	(void) unlink(tfilep1->v_strp);
	(void) unlink(tfilep2->v_strp);

	return rc.status;
	}

// Return pointer to base filename, given pathname or filename.  If withext if false, return base filename without extension.
// "name" is not modified in either case.
char *fbasename(char *name,bool withext) {
	char *strp1,*strp2;
	static char buf[MaxFilename + 1];

	if((strp1 = strp2 = strchr(name,'\0')) > name) {

		// Find rightmost slash, if any.
		while(strp1 > name) {
			if(strp1[-1] == '/')
				break;
			--strp1;
			}

		// Find and eliminate extension, if requested.
		if(!withext) {
			uint len;

			while(strp2 > strp1) {
				if(*--strp2 == '.') {
					if(strp2 == strp1)		// Bail out if '.' is first character.
						break;
					len = strp2 - strp1 + 1;
					stplcpy(buf,strp1,len > sizeof(buf) ? sizeof(buf) : len);
					return buf;
					}
				}
			}
		}

	return strp1;
	}

// Return pointer to directory name, given pathname or filename and n argument.  If non-default n, return "." if no directory
// portion found in name; otherwise, null.  Given name is modified (truncated).
char *fdirname(char *name,int n) {
	char *strp;

	strp = fbasename(name,true);
	if(*name == '/' && (*strp == '\0' || strp == name + 1))
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
		strp[-1] = '\0';
	return name;
	}

// Save pathname on heap (in pathp) and set *rpp to it.  Return status.
static int savepath(char **rpp,char *namep) {
	static char *pathp = NULL;

	if(pathp != NULL)
		(void) free((void *) pathp);
	if((pathp = (char *) malloc(strlen(namep) + 1)) == NULL)
		return rcset(PANIC,0,text94,"savepath");
			// "%s(): Out of memory!"
	strcpy(*rpp = pathp,namep);
	return rc.status;
	}

// Find a script file in the HOME directory or the $execPath directories.  Set *rpp to absolute pathname if found; otherwise,
// NULL.  Return status.  If the filename contains a '/', it is searched for verbatim; otherwise, if "hflag" is true, it is
// searched for in the HOME directory only; otherwise, it is searched for in every directory in $execPath.  "namep" is the
// (base) filename to search for.  All searches are for the original filename first, followed by <filename>SCRIPT_EXT unless the
// filename already has that extension.
int pathsearch(char **rpp,char *namep,bool hflag) {
	char *strp1,*strp2,**np;
	Value *enamep;				// Name with extension, if applicable.
	size_t maxlen = strlen(namep);
	static char *scriptExt = SCRIPT_EXT;
	size_t scriptExtLen = strlen(scriptExt);

	*rpp = NULL;

	// Null filename?
 	if(maxlen == 0)
 		return rc.status;

 	// Create filename-with-extension version.
 	if(vnew(&enamep,false) != 0)
 		return vrcset();
	if((strp1 = strchr(fbasename(namep,true),'.')) == NULL || strcmp(strp1,scriptExt) != 0) {
		maxlen += scriptExtLen;
		if(vsalloc(enamep,maxlen + 1) != 0)
			return vrcset();
		sprintf(enamep->v_strp,"%s%s",namep,scriptExt);
		}

	// If we have a '/' in the path, check only that.
	if(strchr(namep,'/') != NULL)
		return fexist(namep) == 0 ? savepath(rpp,namep) :
		 (!visnull(enamep) && fexist(enamep->v_strp) == 0) ? savepath(rpp,enamep->v_strp) : rc.status;

	// Create name list.
	char *namelist[] = {			// Filenames to check.
		namep,enamep->v_strp,NULL};

	// Check HOME directory (only), if requested.
	if(hflag) {
		if((strp1 = getenv("HOME")) != NULL) {
			char pathbuf[strlen(strp1) + maxlen + 2];
			np = namelist;
			do {
				if((*np)[0] != '\0') {

					// Build pathname and check it.
					sprintf(pathbuf,"%s/%s",strp1,*np);
					if(fexist(pathbuf) == 0)
						return savepath(rpp,pathbuf);
					}
				} while(*++np != NULL);
			}
		return rc.status;
		}

	// Now check the execpath directories.
	for(strp2 = execpath; *strp2 != '\0';) {
		char *dirsep;

		// Build next possible file spec.
		strp1 = strp2;
		while(*strp2 != '\0' && *strp2 != ':')
			++strp2;

		// Add a terminating dir separator if needed.
		dirsep = "";
		if(strp2 > strp1 && strp2[-1] != '/')
			dirsep = "/";

		// Loop through filename(s).
		char pathbuf[strp2 - strp1 + maxlen + 2];
		np = namelist;
		do {
			if((*np)[0] != '\0') {
				sprintf(pathbuf,"%.*s%s%s",(int) (strp2 - strp1),strp1,dirsep,*np);
				if(fexist(pathbuf) == 0)
					return savepath(rpp,pathbuf);
				}
			} while(*++np != NULL);

		// No go.  Onward...
		if(*strp2 == ':')
			++strp2;
		}

	// No such luck.
	return rc.status;
	}

// Get pathname of "fname" and return in "pathp".  Don't resolve it if it's a symbolic link and non-default n.  Return status.
int getpath(Value *pathp,int n,char *fname) {
	char buf[MaxPathname + 1];

	if(n <= 0 && n != INT_MIN) {
		struct stat s;

		if(lstat(fname,&s) != 0)
			goto ertn;
		if((s.st_mode & S_IFMT) != S_IFLNK)
			goto regfile;

		// File is a symbolic link.  Get pathname of parent directory and append filename.
		StrList sl;
		char *bnp = fbasename(fname,true);
		char bn[strlen(bnp) + 1];
		strcpy(bn,bnp);
		if(vopen(&sl,pathp,false) != 0)
			return vrcset();
		if(realpath(fname = fdirname(fname,1),buf) == NULL)
			goto ertn;
		if(vputs(buf,&sl) != 0 || (strcmp(buf,"/") != 0 && vputc('/',&sl) != 0) ||
		 vputs(bn,&sl) != 0 || vclose(&sl) != 0)
			return vrcset();
		}
	else {
regfile:
		if(realpath(fname,buf) == NULL)
ertn:
			return rcset(FAILURE,0,text33,text37,fname,strerror(errno));
				// "Cannot get %s of file \"%s\": %s","pathname"
		if(vsetstr(buf,pathp) != 0)
			(void) vrcset();
		}

	return rc.status;
	}

// Open directory for filename retrieval, given complete or partial pathname (which may end with a slash).  Initialize *fpp to
// static pointer to pathnames to be returned by ereaddir().  Return status.
int eopendir(char *fspec,char **fpp) {
	char *fnp,*termp;
	size_t preflen;

	// Find directory prefix.  Terminate after slash if Unix root directory; otherwise, at rightmost slash.
	termp = ((fnp = fbasename(fspec,true)) > fspec && fnp - fspec > 1) ? fnp - 1 : fnp;

	// Get space for directory name plus maximum filename.
	if(rdbufp != NULL)
		(void) free((void *) rdbufp);
	if((rdbufp = (char *) malloc((preflen = (termp - fspec)) + MaxFilename + 3)) == NULL)
		return rcset(PANIC,0,text94,"eopendir");
			// "%s(): Out of memory!"
	stplcpy(*fpp = rdbufp,fspec,preflen + 1);

	// Open the directory.
	if(dirp != NULL) {
		closedir(dirp);
		dirp = NULL;
		}
	if((dirp = opendir(*rdbufp == '\0' ? "." : rdbufp)) == NULL)
		return rcset(FAILURE,0,text88,rdbufp,strerror(errno));
			// "Cannot read directory \"%s\": %s"

	// Set rdnamep, restore trailing slash in pathname if applicable, and return.
	if(fnp > fspec)
		*((rdnamep = rdbufp + (fnp - fspec)) - 1) = '/';
	else
		rdnamep = rdbufp;

	return rc.status;
	}

// Get next filename from directory opened with eopendir().  Return NOTFOUND if none left.
int ereaddir(void) {
	struct DIRENTRY *dp;
	struct stat fstat;

	// Call for the next file.
	do {
		errno = 0;
		if((dp = readdir(dirp)) == NULL) {
			if(errno == 0) {
				closedir(dirp);
				(void) free((void *) rdbufp);
				dirp = NULL;
				rdnamep = rdbufp = NULL;
				return NOTFOUND;	// No entries left.
				}
			*rdnamep = '\0';
			return rcset(FAILURE,0,text88,rdbufp,strerror(errno));
				// "Cannot read directory \"%s\": %s"
			}
		strcpy(rdnamep,dp->d_name);
		if(stat(rdbufp,&fstat) != 0)
			return rcset(FAILURE,0,text33,text163,rdbufp,strerror(errno));
					// "Cannot get %s of file \"%s\": %s","status"

		// Skip all entries except regular files and directories.
		} while(((fstat.st_mode & S_IFMT) & (S_IFREG | S_IFDIR)) == 0);

	// If this entry is a directory name, say so.
	if((fstat.st_mode & S_IFMT) == S_IFDIR)
		strcat(rdnamep,"/");

	// Return result.
	return rc.status;
	}
