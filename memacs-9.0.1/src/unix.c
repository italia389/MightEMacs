// (c) Copyright 2018 Richard W. Marinelli
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

// Additional include files.
#if LINUX
#include <time.h>
#elif BSD || MACOS
#include <sys/time.h>			// Timer definitions.
#endif
#include <signal.h>			// Signal definitions.
#if USG
#include <termio.h>			// Terminal I/O definitions.
#elif LINUX || MACOS
#include <termios.h>			// Terminal I/O definitions.
#endif
#if TT_Curses || (TT_TermCap && (LINUX || MACOS))
#include <curses.h>			// Curses screen output.
#include <term.h>
#endif

// Completion include files.
// Directory accessing: try and figure this out... if you can!
#if BSD || LINUX || MACOS
#include <sys/dir.h>			// Directory entry definitions.
#define DIRENTRY	direct
#elif USG
#include <dirent.h>			// Directory entry definitions.
#define DIRENTRY	dirent
#endif

// Restore predefined macro(s).
#undef Ctrl				// Restore Ctrl.
#define Ctrl	0x0100

#include "exec.h"
#include "file.h"
#if MMDebug & Debug_Bind
#include "bind.h"
#endif

// Parameters.
#define NKEYENT		300		// Number of keymap entries.
#define NINCHAR		64		// Input buffer size.
#define NOUTCHAR	256		// Output buffer size.
#if TT_TermCap
#define NCAPBUF		1024		// Termcap storage size.
#endif
#define RESET		0		// Get and send "reset" command on exit.

// Constants.
#define TIMEOUT		255		// No character available.

// Type definitions.
struct keyent {				// Key mapping entry.
	struct keyent *samlvlp;		// Character on same level.
	struct keyent *nxtlvlp;		// Character on next level.
	short ch;			// Character.
	ushort ek;			// Resulting keycode (extended key).
	};
#if TT_TermCap
struct capbind {			// Capability binding entry.
	char *name;			// Termcap name.
	char *store;			// Storage variable.
	};
#endif
#if TT_TermCap || TT_Curses
struct tkeybind {			// Keybinding entry.
	char *name;			// Termcap name.
	ushort ek;			// Keycode (extended key).
	};
#endif

/*** Local declarations ***/

static char *ttypath = "/dev/tty";
#if TT_TermCap && RESET
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
#if USG
static struct termio curterm;		// Current modes.
static struct termio oldterm;		// Original modes.
#elif LINUX || MACOS
static struct termios curterm;		// Current modes.
static struct termios oldterm;		// Original modes.
#endif
#if TT_TermCap
static char tcapbuf[NCAPBUF];		// Termcap character storage.
#define CAP_CE		0		// Clear to end of line.
#define CAP_CL		1		// Clear to end of page.
#define CAP_CM		2		// Cursor motion.
#define CAP_IS		3		// Initialize screen.
#define CAP_KE		4		// Keypad mode ends.
#define CAP_KS		5		// Keypad mode starts.
#define CAP_MD		6		// Bold starts.
#define CAP_ME		7		// All attributes end.
#define CAP_MR		8		// Reverse video starts.
#define CAP_TI		9		// Terminal initialize.
#define CAP_TE		10		// Terminal end.
#define CAP_UE		11		// Underline ends.
#define CAP_US		12		// Underline starts.
#define CAP_VB		13		// Visible bell.
static struct capbind capbind[] = {	// Capability binding list.
	{"ce"},				// Clear to end of line.
	{"cl"},				// Clear to end of page.
	{"cm"},				// Cursor motion.
	{"is"},				// Initialize screen.
	{"ke"},				// Keypad mode ends.
	{"ks"},				// Keypad mode starts.
	{"md"},				// Bold starts.
	{"me"},				// All attributes end.
	{"mr"},				// Reverse video starts.
	{"ti"},				// Terminal initialize.
	{"te"},				// Terminal end.
	{"ue"},				// Underline ends.
	{"us"},				// Underline starts.
	{"vb"}				// Visible bell.
	};
static struct tkeybind tkeybind[] = {	// Keybinding list.
	{"bt",Shft|Ctrl | 'I'},		// Back-tab key.
	{"k1",FKey | '1'},		// F1 key.
	{"k2",FKey | '2'},		// F2 key.
	{"k3",FKey | '3'},		// F3 key.
	{"k4",FKey | '4'},		// F4 key.
	{"k5",FKey | '5'},		// F5 key.
	{"k6",FKey | '6'},		// F6 key.
	{"k7",FKey | '7'},		// F7 key.
	{"k8",FKey | '8'},		// F8 key.
	{"k9",FKey | '9'},		// F9 key.
	{"k;",FKey | '0'},		// F10 key.
	{"F1",FKey | 'a'},		// F11 key.
	{"F2",FKey | 'b'},		// F12 key.
	{"F3",FKey | 'c'},		// F13 key.
	{"F4",FKey | 'd'},		// F14 key.
	{"F5",FKey | 'e'},		// F15 key.
	{"F6",FKey | 'f'},		// F16 key.
	{"F7",FKey | 'g'},		// F17 key.
	{"F8",FKey | 'h'},		// F18 key.
	{"F9",FKey | 'i'},		// F19 key.
	{"FA",FKey | 'j'},		// F20 key.

	{"FB",Shft|FKey | '1'},		// Shift-F1 key.
	{"FC",Shft|FKey | '2'},		// Shift-F2 key.
	{"FD",Shft|FKey | '3'},		// Shift-F3 key.
	{"FE",Shft|FKey | '4'},		// Shift-F4 key.
	{"FF",Shft|FKey | '5'},		// Shift-F5 key.
	{"FG",Shft|FKey | '6'},		// Shift-F6 key.
	{"FH",Shft|FKey | '7'},		// Shift-F7 key.
	{"FI",Shft|FKey | '8'},		// Shift-F8 key.
	{"FJ",Shft|FKey | '9'},		// Shift-F9 key.
	{"FK",Shft|FKey | '0'},		// Shift-F10 key.
	{"FL",Shft|FKey | 'a'},		// Shift-F11 key.
	{"FM",Shft|FKey | 'b'},		// Shift-F12 key.
	{"FN",Shft|FKey | 'c'},		// Shift-F13 key.
	{"FO",Shft|FKey | 'd'},		// Shift-F14 key.
	{"FP",Shft|FKey | 'e'},		// Shift-F15 key.
	{"FQ",Shft|FKey | 'f'},		// Shift-F16 key.
	{"FR",Shft|FKey | 'g'},		// Shift-F17 key.
	{"FS",Shft|FKey | 'h'},		// Shift-F18 key.
	{"FT",Shft|FKey | 'i'},		// Shift-F19 key.
	{"FU",Shft|FKey | 'j'},		// Shift-F20 key.
	{"%e",Shft|FKey | 'P'},		// Shift-up arrow key.
	{"%c",Shft|FKey | 'N'},		// Shift-down arrow key.
	{"%i",Shft|FKey | 'F'},		// Shift-right arrow key.
	{"#4",Shft|FKey | 'B'},		// Shift-left arrow key.
	{"FV",FKey | 'k'},		// Other function key 1.
	{"FW",FKey | 'l'},		// Other function key 2.
	{"FX",FKey | 'm'},		// Other function key 3.
	{"FY",FKey | 'n'},		// Other function key 4.
	{"FZ",FKey | 'o'},		// Other function key 5.
	{"Fa",FKey | 'p'},		// Other function key 6.
	{"Fb",FKey | 'q'},		// Other function key 7.
	{"Fc",FKey | 'r'},		// Other function key 8.
	{"Fd",FKey | 's'},		// Other function key 9.
	{"Fe",FKey | 't'},		// Other function key 10.

	{"Ff",Shft|FKey | 'k'},		// Shift-other key 1.
	{"Fg",Shft|FKey | 'l'},		// Shift-other key 2.
	{"Fh",Shft|FKey | 'm'},		// Shift-other key 3.
	{"Fi",Shft|FKey | 'n'},		// Shift-other key 4.
	{"Fj",Shft|FKey | 'o'},		// Shift-other key 5.
	{"Fk",Shft|FKey | 'p'},		// Shift-other key 6.
	{"Fl",Shft|FKey | 'q'},		// Shift-other key 7.
	{"Fm",Shft|FKey | 'r'},		// Shift-other key 8.
	{"Fn",Shft|FKey | 's'},		// Shift-other key 9.
	{"Fo",Shft|FKey | 't'},		// Shift-other key 10.

	{"kA",Ctrl | 'O'},		// Insert line key.
	{"kb",Ctrl | 'H'},		// Backspace key.
	{"kC",Ctrl | 'L'},		// Clear screen key.
	{"kD",FKey | 'D'},		// Delete character key.
	{"kd",FKey | 'N'},		// Down arrow key.
	{"kE",Ctrl | 'K'},		// Clear to end of line key.
	{"kF",Ctrl | 'V'},		// Scroll forward key.
	{"kH",FKey | '>'},		// End key.
	{"@7",FKey | '>'},		// End key.
	{"kh",FKey | '<'},		// Home key.
	{"kI",FKey | 'C'},		// Insert character key.
	{"kL",Ctrl | 'K'},		// Delete line key.
	{"kl",FKey | 'B'},		// Left arrow key.
	{"kN",FKey | 'V'},		// Next page key.
	{"kP",FKey | 'Z'},		// Previous page key.
	{"kR",Ctrl | 'Z'},		// Scroll backward key.
	{"kr",FKey | 'F'},		// Right arrow key.
	{"ku",FKey | 'P'}		// Up arrow key.
	};
#endif // TT_TermCap
#if TT_Curses
static struct tkeybind tkeybind[] = {	// Keybinding list.
	{"cbt",Shft|Ctrl | 'I'},	// Back-tab key.
	{"kf1",FKey | '1'},		// F1 key.
	{"kf2",FKey | '2'},		// F2 key.
	{"kf3",FKey | '3'},		// F3 key.
	{"kf4",FKey | '4'},		// F4 key.
	{"kf5",FKey | '5'},		// F5 key.
	{"kf6",FKey | '6'},		// F6 key.
	{"kf7",FKey | '7'},		// F7 key.
	{"kf8",FKey | '8'},		// F8 key.
	{"kf9",FKey | '9'},		// F9 key.
	{"kf0",FKey | '0'},		// F0 or F10 key.
	{"kf10",FKey | '0'},		// F0 or F10 key.
	{"kf11",Shft|FKey | '1'},	// Shift-F1 or F11 key.
	{"kf12",Shft|FKey | '2'},	// Shift-F2 or F12 key.
	{"kf13",Shft|FKey | '3'},	// Shift-F3 or F13 key.
	{"kf14",Shft|FKey | '4'},	// Shift-F4 or F14 key.
	{"kf15",Shft|FKey | '5'},	// Shift-F5 or F15 key.
	{"kf16",Shft|FKey | '6'},	// Shift-F6 or F16 key.
	{"kf17",Shft|FKey | '7'},	// Shift-F7 or F17 key.
	{"kf18",Shft|FKey | '8'},	// Shift-F8 or F18 key.
	{"kf19",Shft|FKey | '9'},	// Shift-F9 or F19 key.
	{"kf20",Shft|FKey | '0'},	// Shift-F0 or F20 key.
	{"kil1",Ctrl | 'O'},		// Insert line key.
	{"kbs",Ctrl | 'H'},		// Backspace key.
	{"kclr",Ctrl | 'L'},		// Clear screen key.
	{"kdch1",FKey | 'D'},		// Delete character key.
	{"kcud1",FKey | 'N'},		// Down arrow key.
	{"kel",Ctrl | 'K'},		// Clear to end of line key.
	{"kind",Ctrl | 'V'},		// Scroll forward key.
	{"kll",FKey | '>'},		// Home down key.
	{"kend",FKey | '>'},		// Home down key.
	{"khome",FKey | '<'},		// Home key.
	{"kich1",FKey | 'C'},		// Insert character key.
	{"kdl1",Ctrl | 'K'},		// Delete line key.
	{"kcub1",FKey | 'B'},		// Left arrow key.
	{"knp",FKey | 'V'},		// Next page key.
	{"kpp",FKey | 'Z'},		// Previous page key.
	{"kri",Ctrl | 'Z'},		// Scroll backward key.
	{"kcuf1",FKey | 'F'},		// Right arrow key.
	{"kcuu1",FKey | 'P'}		// Up arrow key.
	};
#endif // TT_Curses
static int inbuf[NINCHAR];		// Input buffer.
static int *inbufh = inbuf;		// Head of input buffer.
static int *inbuft = inbuf;		// Tail of input buffer.
#if TT_TermCap
static char outbuf[NOUTCHAR];		// Output buffer.
static char *outbuft = outbuf;		// Output buffer tail.
#endif
static char keyseq[256];		// Prefix escape sequence table.
static struct keyent keymap[NKEYENT];	// Key map.
static struct keyent *nxtkeyp = keymap;	// Next free key entry.
static DIR *dirp = NULL;		// Current directory stream.
static char *rdbuf = NULL;		// ereaddir() file buffer (allocated from heap).
static char *rdname = NULL;		// Pointer past end of path in rdbuf.
#if TT_Curses
static char name_addch[] = "addch";
#endif
static char name_ioctl[] = "ioctl";
static char name_tcsetattr[] = "tcsetattr";

// Terminal definition block.
static int ttgetc(ushort *),ttputc(int),ttflush(void);
static int scopen(void),scclose(ushort flags),scmove(int,int),scbeep(void),sceeol(void),sceeop(void),scul(bool),sckopen(void),
 sckclose(void),scattroff(void),
#if TT_Curses
 screv(bool),scbold(bool);
#else
 screv(void),scbold(void);
#endif

// Global variables.
ETerm term = {
	TT_MaxCols,			// Maximum number of columns.
	0,				// Current number of columns.
	TT_MaxRows,			// Maximum number of rows.
	0,				// Current number of rows.
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
	scbeep,				// Beep! routine.
	screv,				// Set reverse video routine.
	scbold,				// Set bold routine.
	scul,				// Set underline routine.
	scattroff			// All attributes off routine.
	};

// Initialize attributes of terminal device.  Return status.
static int ttinit(void) {
	static char myname[] = "ttinit";
#if BSD
	// Get tty modes.
	if(ioctl(0,TIOCGETP,&oldsgtty) == -1 || ioctl(0,TIOCGETC,&oldtchars) == -1 ||
	 ioctl(0,TIOCGLTC,&oldlchars) == -1)
		return rcset(OSError,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"

	// Save to original mode variables.
	cursgtty = oldsgtty;

	// Set new modes.
	cursgtty.sg_flags |= CBREAK;
	cursgtty.sg_flags &= ~(ECHO | CRMOD);

	// Set tty modes.
	if(ioctl(0,TIOCSETP,&cursgtty) == -1 || ioctl(0,TIOCSETC,blank) == -1 || ioctl(0,TIOCSLTC,blank) == -1)
		return rcset(OSError,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"
#elif USG

	// Get tty modes.
	if(ioctl(0,TCGETA,&oldterm) == -1)
		return rcset(OSError,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"

	// Save to original mode variable.
	curterm = oldterm;

	// Set new modes, including disabling flow control.  Note that this could be a problem if a real terminal (directly
	// connected to a serial port) is used.
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
		return rcset(OSError,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"
#elif LINUX || MACOS
	// Get tty modes.
	if(tcgetattr(0,&oldterm))
		return rcset(OSError,0,text44,"tcgetattr",myname);
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
		return rcset(OSError,0,text44,name_tcsetattr,myname);
			// "calling %s() from %s() function"
#endif
	// Success.
	return rc.status;
	}

// Restore old attributes of terminal device.  Return status.
static int ttrestore(void) {
	static char myname[] = "ttrestore";

#if TT_TermCap && RESET
	// Restore original terminal modes.
	if(reset != NULL)
		(void) write(1,reset,strlen(reset));
#endif
#if BSD
	if(ioctl(0,TIOCSETP,&oldsgtty) == -1 || ioctl(0,TIOCSETC,&oldtchars) == -1 ||
	 ioctl(0,TIOCSLTC,&oldlchars) == -1)
		return rcset(OSError,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"
#elif USG
	if(ioctl(0,TCSETA,&oldterm) == -1)
		return rcset(OSError,0,text44,name_ioctl,myname);
			// "calling %s() from %s() function"
#elif LINUX || MACOS
	// Set tty mode.
	if(tcsetattr(0,TCSANOW,&oldterm) == -1)
		return rcset(OSError,0,text44,name_tcsetattr,myname);
			// "calling %s() from %s() function"
#endif
	// Success.
	return rc.status;
	}

// Flush output buffer to display.  Return status.
static int ttflush(void) {
#if TT_TermCap
	int len;

	// Compute length of write.
	if((len = outbuft - outbuf) == 0)
		return rc.status;

	// Reset buffer position.
	outbuft = outbuf;

	// Perform write to screen.
	if(write(1,outbuf,len) == -1)
		return rcset(OSError,0,text44,"write","ttflush");
			// "calling %s() from %s() function"
#elif TT_Curses
	if(refresh() == ERR)
		return rcset(OSError,0,text44,"refresh","ttflush");
			// "calling %s() from %s() function"
#endif
	return rc.status;
	}

// Put character onto display.  Return status.
static int ttputc(int ch) {

#if TT_TermCap
	// Check for buffer full.
	if(outbuft == outbuf + sizeof(outbuf) && ttflush() != Success)
		return rc.status;

	// Add to buffer.
	*outbuft++ = ch;
#elif TT_Curses
	// Put character on screen.
	if(addch(ch) == ERR)
		return rcset(OSError,0,text44,name_addch,"ttputc");
			// "calling %s() from %s() function"
#endif
	return rc.status;
	}

#if MMDebug & Debug_Bind
// Write arguments to log file in visible form.
static void dumpkey(char *seq,ushort ek) {
	short c;
	char *kb,keybuf1[16],keybuf2[16];

	kb = keybuf1;
	while((c = *seq++) != '\0')
		if(c >= ' ' && c <= '~')
			*kb++ = c;
		else {
			sprintf(kb,"<%.2hX>",c);
			kb += 4;
			}
	*kb = '\0';

	fprintf(logfile,"addkey(...):    %-12s ->    %-10s%.4hX\n",keybuf1,ektos(ek,keybuf2,false),ek);
	}
#endif

// Add character sequence to keymap table.  "ek" is key code.  Return status.
static int addkey(char *seq,ushort ek) {
	bool first;
	struct keyent *curp,*nxtcurp;

	// Skip null and single-character sequences.
	if(seq == NULL || strlen(seq) <= 1)
		return rc.status;

#if MMDebug & Debug_Bind
	dumpkey(seq,ek);
#endif

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
		return rcset(FatalError,0,text157,"addkey",NKEYENT);
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
static int grabwait(short *cp) {
	uchar ch;
	static char myname[] = "grabwait";

#if LINUX || MACOS || USG
	// Change mode, if necessary.
	if(curterm.c_cc[VTIME]) {
		curterm.c_cc[VMIN] = 1;		// Minimum number of characters for noncanonical read.
		curterm.c_cc[VTIME] = 0;	// Timeout in deciseconds for noncanonical read.
#if USG
		if(ioctl(0,TCSETA,&curterm) == -1)
			return rcset(OSError,0,text44,name_ioctl,myname);
				// "calling %s() from %s() function"
#else
		if(tcsetattr(0,TCSANOW,&curterm) == -1)
			return rcset(OSError,0,text44,name_tcsetattr,myname);
				// "calling %s() from %s() function"
#endif
		}
#endif
	// Perform read.
	if(read(0,(void *) &ch,1) == -1)
		return rcset(OSError,0,text44,"read",myname);
			// "calling %s() from %s() function"

	// Return new character.
	*cp = ch;
	return rc.status;
	}

// Grab input characters, short wait.  Return status.
static int grabnowait(short *cp) {
	static char myname[] = "grabnowait";
#if BSD
	static struct timeval timout = {0,200000L};
	int count,r;

	// Select input.
	r = 1;
	count = select(1,&r,NULL,NULL,&timout);
	if(count == 0) {
		*cp = TIMEOUT;
		return rc.status;
		}
	if(count < 0)
		return rcset(OSError,0,text44,"select",myname);
			// "calling %s() from %s() function"

	// Perform read.
	return grabwait(cp);

#elif LINUX || MACOS || USG
	int count;
	uchar ch;

	// Change mode, if necessary.
	if(curterm.c_cc[VTIME] == 0) {
		curterm.c_cc[VMIN] = 0;		// Minimum number of characters for noncanonical read.
		curterm.c_cc[VTIME] = 2;	// Timeout in deciseconds for noncanonical read.
#if USG
		if(ioctl(0,TCSETA,&curterm) == -1)
			return rcset(OSError,0,text44,name_ioctl,myname);
				// "calling %s() from %s() function"
#else
		if(tcsetattr(0,TCSANOW,&curterm) == -1)
			return rcset(OSError,0,text44,name_tcsetattr,myname);
				// "calling %s() from %s() function"
#endif
		}

	// Perform read.
	count = read(0,(void *) &ch,1);
	if(count == 0)
		ch = TIMEOUT;
	else if(count < 0)
		return rcset(OSError,0,text44,"read",myname);
			// "calling %s() from %s() function"

	// Return new character.
	*cp = ch;
	return rc.status;
#endif
	}

// Queue input character.  Return status.
static int qin(ushort ch) {

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
	short ch;
	struct keyent *cur;

	// Get first character untimed.
	if(grabwait(&ch) != Success || qin(ch) != Success)
		return rc.status;

	// Skip if the key isn't a special leading escape sequence.
	if(keyseq[ch] == 0) {

		// If it is a null, make it a (0/1/32).
		if(ch == '\0') {
			if(qin(Ctrl >> 8) == Success)	// Control.
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
				return qin(cur->ek);
				}
			else {
				// Advance to next level.
				cur = cur->nxtlvlp;

				// Get next character, timed, and queue it.
				if(grabnowait(&ch) != Success || ch == TIMEOUT || qin(ch) != Success)
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
static int ttgetc(ushort *cp) {
	short ch;

	// Loop until character is in input buffer.
	while(inbufh == inbuft)
		if(cook() != Success)
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
		return rcset(OSError,0,text44,name_ioctl,"typahead");
			// "calling %s() from %s() function"
	*countp = count;
#else

	// Ask hardware for count.
	count = ioctl(0,FIORDCHK,0);
	if(count < 0)
		return rcset(OSError,0,text44,name_ioctl,"typahead");
			// "calling %s() from %s() function"
	*countp = count;
#endif
	return rc.status;
	}

#if TT_TermCap
// Put out sequence, with padding.  Return status.
static int putpad(char *seq) {

	// Call on termcap to send sequence.
	if(seq == NULL || tputs(seq,1,ttputc) == ERR)
		return rcset(OSError,0,text44,"tputs","putpad");
			// "calling %s() from %s() function"

	return rc.status;
	}
#endif

// Build OS error message if caller is not NULL, append TERM to it, and return OSError status.
static int termerr(char *caller,char *call) {
	DStrFab msg;

	if(caller != NULL)
		(void) rcset(OSError,0,text44,call,caller);
			// "calling %s() from %s() function"
	return (dopenwith(&msg,&rc.msg,SFAppend) != 0 || dputf(&msg,", TERM '%s'",vtc.termnam) != 0 ||
	 dclose(&msg,sf_string) != 0) ? librcset(Failure) : rc.status;
	}


// Update terminal size parameters, given number of columns and rows.
void settermsize(ushort ncol,ushort nrow) {

	sampbuf.smallsize = (term.t_ncol = ncol) / 4;
	term.t_nrow = nrow;
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
	if(w.ws_col < TT_MinCols || w.ws_row < TT_MinRows) {
#if TT_Curses
		(void) endwin();
#endif
		return rcset(FatalError,0,text190,w.ws_col,w.ws_row,Myself);
			// "Terminal size %hu x %hu is too small to run %s"
		}

	if((*colp = w.ws_col) > term.t_mcol)
		*colp = term.t_mcol;
	if((*rowp = w.ws_row) > term.t_mrow)
		*rowp = term.t_mrow;
	return rc.status;
	}

// Re-open screen package -- at program startup or after a "suspend" operation.  Return status.
#if RestoreTerm
static int scopen2(ushort flags) {
#else
static int scopen2(void) {
#endif

	// Initialize terminal attributes.
	if(ttinit() != Success)
		return termerr(NULL,NULL);
#if TT_TermCap
#if 0
	// Set speed for padding sequences.
#if BSD
	ospeed = cursgtty.sg_ospeed;
#elif USG
	ospeed = curterm.c_cflag & CBAUD;
#elif LINUX || MACOS
	ospeed = cfgetospeed(&curterm);
#endif
#endif
	// Send out initialization sequences.
#if RestoreTerm
	if(((flags & TermCup) && putpad(capbind[CAP_TI].store) != Success) ||
#else
	if(putpad(capbind[CAP_IS].store) != Success ||
#endif
	 putpad(capbind[CAP_KS].store) != Success || sckopen() != Success)
		return rc.status;
#endif // TT_TermCap

	// Success.
	opflags |= (OpVTOpen | OpScrRedraw);
	return rc.status;
	}

// Initialize screen package.  Return status.
static int scopen(void) {
#if TT_TermCap || TT_Curses
	static char myname[] = "scopen";
	ushort ncol,nrow;
	struct tkeybind *kp;
#if TT_TermCap
	int status;
	char *str;
	char tcbuf[1024];
	struct capbind *cb;
#endif // TT_TermCap

	// Get terminal type.
	if((vtc.termnam = getenv("TERM")) == NULL)
		return rcset(FatalError,RCNoFormat,text182);
		// "Environmental variable 'TERM' not defined!"

	// Fix up file descriptors if reading a file from standard input.
	if(!isatty(0)) {
		int fd;

		// File descriptor 0 is not a TTY so it must be a data file or pipe.  Get a new descriptor for it by calling
		// dup() (and saving the result in fi.stdinfd), then open /dev/tty, and dup2() that FD back to 0.  This
		// is done so that FD 0 is always the keyboard which should ensure that tgetent() will work properly on any
		// brain- dead Unix variants.
		if((fi.stdinfd = dup(0)) == -1)
			return termerr(myname,"dup");
		if((fd = open(ttypath,O_RDONLY)) == -1)
			return termerr(myname,"open");
		if(dup2(fd,0) == -1)
			return termerr(myname,"dup2");
		}

#if TT_TermCap
	// Load termcap.
	if((status = tgetent(tcbuf,vtc.termnam)) == -1)
		return termerr(myname,"tgetent");
	if(status == 0)
		return rcset(FatalError,0,text183,vtc.termnam);
			// "Unknown terminal type '%s'!"
#elif TT_Curses
	// Initialize screen.
	(void) initscr();
#endif
	// Get terminal size and save it.
	if(gettermsize(&ncol,&nrow) != Success)
		return rc.status;
	settermsize(ncol,nrow);
#if TT_TermCap
	// Start grabbing termcap commands.
	str = tcapbuf;

#if RESET
	// Get the reset string.
	reset = tgetstr("is",&str);
#endif
	// Get capabilities and save in table.
	cb = capbind;
	while(cb < capbind + (sizeof(capbind) / sizeof(*capbind))) {
		cb->store = tgetstr(cb->name,&str);
		++cb;
		}

	// Check for minimum.
	if(capbind[CAP_CL].store == NULL || capbind[CAP_CM].store == NULL || capbind[CAP_CE].store == NULL)
		return rcset(FatalError,0,text192,vtc.termnam,Myself);
			// "This terminal (%s) does not have sufficient capabilities to run %s"

	// Set attribute flags.
	if(capbind[CAP_ME].store) {
		if(capbind[CAP_MR].store)
			opflags |= OpHaveRev;
		if(capbind[CAP_MD].store)
			opflags |= OpHaveBold;
		}
	if(capbind[CAP_US].store && capbind[CAP_UE].store)
		opflags |= OpHaveUL;
#endif // TT_TermCap

	// Get key bindings.
#if LINUX || MACOS
	// These keys don't make it into keymap for some reason (in loop below) so we add them here as a workaround.
	addkey("\033[Z",Shft|Ctrl | 'I');	// Shift-Tab (bt)
	addkey("\033[1;2D",Shft|FKey | 'B');	// Shift-left arrow (#4)
	addkey("\033[1;2C",Shft|FKey | 'F');	// Shift-right arrow (%i)
	addkey("\033[3;2~",Shft|FKey | 'D');	// Shift-forward delete
#endif
	kp = tkeybind;
	while(kp < tkeybind + (sizeof(tkeybind) / sizeof(*tkeybind))) {
#if TT_TermCap
		if(addkey(tgetstr(kp->name,&str),kp->ek) != Success)
#else
		if(addkey(tgetstr(kp->name,NULL),kp->ek) != Success)
#endif
			return rc.status;
		++kp;
		}

#endif // TT_TermCap || TT_Curses

	// Finish up.
#if RestoreTerm
	return scopen2(TermCup);
#else
	return scopen2();
#endif
	}

// Clean up the terminal in anticipation of a return to the operating system.  Close the keyboard, flush screen output, and
// restore the old terminal settings.  Return status.
static int scclose(ushort flags) {

	// Skip this if terminal not open.
	if(opflags & OpVTOpen) {
#if TT_TermCap
		// Turn off keypad mode.
		if(putpad(capbind[CAP_KE].store) == Success || (flags & TermForce))
			if(sckclose() == Success || (flags & TermForce))

				// Close terminal device.
				if(
#if RestoreTerm
				 (!(flags & TermCup) || putpad(capbind[CAP_TE].store) == Success) &&
#endif
				 ttflush() == Success)
					(void) ttrestore();
#elif TT_Curses
				// Turn off curses.
				(void) endwin();

				// Close terminal device.
				if(ttflush() == Success)
					(void) ttrestore();
#endif
		opflags &= ~OpVTOpen;
		}
	return rc.status;
	}

// Open keyboard.  Return status.
static int sckopen(void) {

#if TT_TermCap
	if(putpad(capbind[CAP_KS].store) == Success)
		(void) ttflush();
#endif
	return rc.status;
	}

// Close keyboard.  Return status.
static int sckclose(void) {

#if TT_TermCap
	if(putpad(capbind[CAP_KE].store) == Success)
		(void) ttflush();
#endif
	return rc.status;
	}

// Move cursor.  Return status.
static int scmove(int row,int col) {
	char *tgoto();

#if TT_TermCap
	// Call on termcap to create move sequence.
	(void) putpad(tgoto(capbind[CAP_CM].store,col,row));
#elif TT_Curses
	if(move(row,col) == ERR)
		(void) rcset(OSError,0,text44,"move","scmove");
			// "calling %s() from %s() function"
#endif
	return rc.status;
	}

// Erase to end of line.  Return status.
static int sceeol(void) {

#if TT_TermCap
	// Send erase sequence.
	(void) putpad(capbind[CAP_CE].store);
#elif TT_Curses
	if(clrtoeol() == ERR)
		(void) rcset(OSError,0,text44,"clrtoeol","sceeol");
			// "calling %s() from %s() function"
#endif
	return rc.status;
	}

// Clear screen.  Return status.
static int sceeop(void) {
#if TT_TermCap
	// Send clear sequence.
	(void) putpad(capbind[CAP_CL].store);
#elif TT_Curses
	if(erase() == ERR)
		(void) rcset(OSError,0,text44,"erase","sceeop");
			// "calling %s() from %s() function"
#endif
	return rc.status;
	}

#if TT_Curses
// Set reverse video state, given Boolean on/off flag.  Return status.
static int screv(bool attrOn) {
#else
// Enter reverse video state.  Return status.
static int screv(void) {
#endif
	// Set reverse video state.
	if(opflags & OpHaveRev)
#if TT_TermCap
		(void) putpad(capbind[CAP_MR].store);
#elif TT_Curses
		(void) (attrOn ? attron(A_REVERSE) : attroff(A_REVERSE));
#endif
	return rc.status;
	}

#if TT_Curses
// Set bold state, given Boolean on/off flag.  Return status.
static int scbold(bool attrOn) {
#else
// Enter bold state.  Return status.
static int scbold(void) {
#endif
	// Set bold state.
	if(opflags & OpHaveBold)
#if TT_TermCap
		(void) putpad(capbind[CAP_MD].store);
#elif TT_Curses
		(void) (attrOn ? attron(A_BOLD) : attroff(A_BOLD));
#endif
	return rc.status;
	}

// Set underline state, given Boolean on/off flag.  Return status.
static int scul(bool attrOn) {

	// Set underline state.
	if(opflags & OpHaveUL)
#if TT_TermCap
		(void) putpad(attrOn ? capbind[CAP_US].store : capbind[CAP_UE].store);
#elif TT_Curses
		(void) (attrOn ? attron(A_UNDERLINE) : attroff(A_UNDERLINE));
#endif
	return rc.status;
	}

// Turn off all (bold, reverse video, underline) attributes.  Return status.
static int scattroff(void) {

#if TT_TermCap
	(void) putpad(capbind[CAP_ME].store);
#elif TT_Curses
	(void) attrset(0);
#endif
	return rc.status;
	}

// Beep.  Return status.
static int scbeep(void) {
#if TT_TermCap
#if VizBell
	// Send out visible bell, if it exists.
	if(capbind[CAP_VB].store)
		(void) putpad(capbind[CAP_VB].store);
	else
#endif
		// The old standby method.
		(void) ttputc('\7');
#elif TT_Curses
	if(addch('\7') == ERR)
		(void) rcset(OSError,0,text44,name_addch,"scbeep");
			// "calling %s() from %s() function"
#endif
	return rc.status;
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
int changedir(char *dir) {

	return (chdir(dir) != 0) ? rcset(Failure,0,text265,dir,strerror(errno)) : rc.status;
			// "Cannot change to directory \"%s\": %s"
	}

// Do chdir command (change working directory).  Return status.
int chwkdir(Datum *rp,int n,Datum **argpp) {
	HookRec *hrp = hooktab + HkChDir;
	TermInp ti = {NULL,RtnKey,MaxPathname,NULL};

	// Get directory name.
	if(opflags & OpScript)
		datxfer(rp,argpp[0]);
	else if(terminp(rp,text277,CFNil1,Term_C_Filename,&ti) != Success || rp->d_type == dat_nil)
			// "Change directory"
		return rc.status;

	// Make the change.
	if(changedir(rp->d_str) != Success)
		return rc.status;

	// Get new path (absolute pathname), save it in current screen, and return it to caller.
	if(setwkdir(cursp) != Success)
		return rc.status;
	if(dsetstr(cursp->s_wkdir,rp) != 0)
		return librcset(Failure);

	// Invalidate "last screen" pointer in any buffer pointing to current screen.
	nukebufsp(cursp);

	// Set current window flag if needed.
	if(modetab[MdIdxGlobal].flags & MdWkDir)
		curwp->w_flags |= WFMode;

	// Run change-directory user hook unless it is not set or is currently running.
	if(hrp->h_bufp != NULL && hrp->h_bufp->b_mip->mi_nexec == 0 && exechook(NULL,n,hrp,0) != Success)
		return rc.status;

	// Display new directory if interactive and 'WkDir' global mode not enabled.
	return (!(opflags & OpScript) && !(modetab[MdIdxGlobal].flags & MdWkDir)) ?
	 rcset(Success,RCNoFormat | RCNoWrap,cursp->s_wkdir) : rc.status;
	}

// Suspend MightEMacs.
int suspendMM(Datum *rp,int n,Datum **argpp) {

	// Reset the terminal and go to the last line.
	if(scclose(
#if RestoreTerm
	 TermCup
#else
	 0
#endif
	 ) != Success)
		return rc.status;

	// Send stop signal to self (suspend)...
	if(kill(getpid(),SIGTSTP) == -1)
		return rcset(OSError,0,text44,"kill","suspendMM");
			// "calling %s() from %s() function"

	// We should be back here after resuming.

	// Reopen the screen and redraw.
#if RestoreTerm
	return scopen2(TermCup);
#else
	return scopen2();
#endif
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

#if !(BSD || LINUX || MACOS)
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

// Create temporary filename, store in datp, and return status code.
static int tmpfname(Datum *datp) {
	static unsigned n = 0U;

	if(dsalloc(datp,24) != 0)
		return -1;
	sprintf(datp->d_str,"/tmp/_mm%u.%u",mypid,n++);
	return 0;
	}

// Call out to system to perform given command.  Return status, and false if error so that macros won't abort.
static int callout(Datum *rp,Datum *cfp,bool force) {
	int rcode;

	// Close down.
	if(scclose(0) != Success)
		return rc.status;

	// Do command.
	rcode = system(cfp->d_str);

	// Restart system.
#if RestoreTerm
	if(scopen2(0) != Success)
#else
	if(scopen2() != Success)
#endif
		return rc.status;

	// Set return value to false if error occurred; otherwise, true.
	dsetbool(rcode == 0,rp);

	// If script mode, set alert message if error.  If interactive, pause to display shell message if error or force.
	if(opflags & OpScript) {
		if(rcode != 0)
			(void) rcset(Success,RCNoWrap,text194,cfp->d_str);
				// "Shell command \"%s\" failed"
		}
	else if(rcode != 0 || force) {
		if(mlputs(MLHome | MLForce | MLWrap | MLFlush,text188) == Success)
							// "End"
			(void) ttgetc(NULL);
		}

	return rc.status;
	}

// Create subshell.  Return status..
int shellCLI(Datum *rp,int n,Datum **argpp) {
	char *shpath;
	Datum *datp;

	// Get shell path.
	if((shpath = getenv("SHELL")) == NULL)
#if BSD
		shpath = "/bin/csh";
#else
		shpath = "/bin/sh";
#endif

	// Do shell.
	return dnewtrk(&datp) != 0 || dsetstr(shpath,datp) != 0 ? librcset(Failure) : callout(rp,datp,false);
	}

// Get Unix command line.  Return status.
static int getcmd(Datum **cfpp,char *prmt) {

	if(dnewtrk(cfpp) != 0)
		return librcset(Failure);
	if(opflags & OpScript) {

		// Concatenate all arguments into *cfpp.
		(void) catargs(*cfpp,1,NULL,0);
		}
	else
		(void) terminp(*cfpp,prmt,CFNotNull1 | CFNil1,0,NULL);

	return rc.status;
	}

// Execute a Unix command and return status.  Return nil if error.
int shellCmd(Datum *rp,int n,Datum **argpp) {
	Datum *cfp;

	// Get the command line and execute it unless user hit RETURN at prompt.
	if(getcmd(&cfp,"> ") == Success && cfp->d_type != dat_nil)
		(void) callout(rp,cfp,true);

	return rc.status;
	}

// Get a shell command, modify it to save results to a temporary file, and execute it (if tfilep2 is NULL).  Set *ucancelp to
// true if user cancels.  Return status.
static int prepcmd(Datum *rp,DStrFab *sfp,char *prmt,bool *ucancelp,Datum **tfilepp1,Datum **tfilepp2) {
	Datum *cfp;

	// Get shell command.
	if(getcmd(&cfp,prmt) != Success)
		return rc.status;
	if(cfp->d_type == dat_nil) {		// User hit RETURN at prompt.
		*ucancelp = true;
		return rc.status;
		}
	if(dnewtrk(tfilepp1) != 0 || (tfilepp2 != NULL && dnewtrk(tfilepp2) != 0) || dopentrk(sfp) != 0 ||
	 dputs(cfp->d_str,sfp) != 0)
		return librcset(Failure);

	// Modify command to send output to or read input from a temporary file.
	if(dputs(tfilepp2 == NULL ? " >" : " <",sfp) != 0 || tmpfname(*tfilepp1) != 0 || dputs((*tfilepp1)->d_str,sfp) != 0)
		return librcset(Failure);

	// Finish it.
	*ucancelp = false;
	if(tfilepp2 != NULL) {
		if(dputs(" >",sfp) != 0 || tmpfname(*tfilepp2) != 0 || dputs((*tfilepp2)->d_str,sfp) != 0 ||
		 dclose(sfp,sf_string) != 0)
			return librcset(Failure);
		}
	else {
		// Execute it.
		if(dclose(sfp,sf_string) != 0)
			return librcset(Failure);
		if(callout(rp,sfp->sf_datp,false) == Success && rp->d_type == dat_false)
			(void) unlink((*tfilepp1)->d_str);
		}

	return rc.status;
	}

// Execute shell pipeline and insert result into the current buffer.  If n == 0, leave point before the inserted text.
int insertPipe(Datum *rp,int n,Datum **argpp) {
	DStrFab cmd;
	bool ucancel;
	Datum *tfilep;

	// Get pipe-in command and execute it.
	if(prepcmd(rp,&cmd,text249,&ucancel,&tfilep,NULL) == Success && !ucancel && rp->d_type != dat_false) {
			// "Insert pipe"

		// Insert the temporary file (command output) and delete it.
		(void) ifile(tfilep->d_str,n);
		(void) unlink(tfilep->d_str);
		}

	return rc.status;
	}

// Pipe output of Unix pipeline into current buffer.  Return status..
int readPipe(Datum *rp,int n,Datum **argpp) {
	DStrFab cmd;
	bool ucancel;
	Datum *tfilep;

	// Get pipe-in command and run it.
	if(prepcmd(rp,&cmd,text170,&ucancel,&tfilep,NULL) == Success && !ucancel && rp->d_type != dat_false) {
			// "Read pipe"

		// Return results.
		(void) rdfile(rp,n,tfilep->d_str,RWScratch);
		(void) unlink(tfilep->d_str);
		}

	return rc.status;
	}

// Filter current buffer through a shell command.  Set rp to nil if command failed.  Return status.
int filterBuf(Datum *rp,int n,Datum **argpp) {
	DStrFab cmd;
	char *fname0;
	bool ucancel;
	Datum *tfilep1,*tfilep2;

	// Get the command.
	if(prepcmd(rp,&cmd,text306,&ucancel,&tfilep1,&tfilep2) != Success || ucancel || rp->d_type == dat_false)
			// "Filter command"
		return rc.status;

	// Save the current buffer's filename.
	fname0 = curbp->b_fname;		// Save the original (heap) name.
	curbp->b_fname = NULL;

	// Write out the buffer, checking for errors.
	if(writeout(curbp,tfilep1->d_str,'w',0) != Success)
		return rc.status;

	// Execute the shell command.  If successful, read in the file that was created.
	if(callout(rp,cmd.sf_datp,false) == Success && rp->d_type != dat_false)
		if(readin(curbp,tfilep2->d_str,RWExist) == Success) {

			// Mark buffer as changed.
			curbp->b_flags |= BFChanged;
			}

	// Restore the original filename and get rid of the temporary files.
	curbp->b_fname = fname0;
	(void) unlink(tfilep1->d_str);
	(void) unlink(tfilep2->d_str);

	return rc.status;
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

// Find a script file in the HOME directory or the $execPath directories.  Set *destp to absolute pathname if found; otherwise,
// NULL.  Return status.  If the filename contains a '/', it is searched for verbatim; otherwise, if "homeOnly" is true, it is
// searched for in the HOME directory only; otherwise, it is searched for in every directory in $execPath.  "name" is the
// (base) filename to search for.  All searches are for the original filename first, followed by <filename>Script_Ext unless the
// filename already has that extension.
int pathsearch(char **destp,char *name,bool homeOnly) {
	char *str1,*str2,**np;
	Datum *enamep;				// Name with extension, if applicable.
	size_t maxlen = strlen(name);
	static char *scriptExt = Script_Ext;
	size_t scriptExtLen = strlen(scriptExt);

	*destp = NULL;

	// Null filename?
 	if(maxlen == 0)
 		return rc.status;

 	// Create filename-with-extension version.
 	if(dnewtrk(&enamep) != 0)
 		return librcset(Failure);
	if((str1 = strchr(fbasename(name,true),'.')) == NULL || strcmp(str1,scriptExt) != 0) {
		maxlen += scriptExtLen;
		if(dsalloc(enamep,maxlen + 1) != 0)
			return librcset(Failure);
		sprintf(enamep->d_str,"%s%s",name,scriptExt);
		}
	else
		dsetnull(enamep);

	// If we have a '/' in the path, check only that.
	if(strchr(name,'/') != NULL)
		return fexist(name) == 0 ? savepath(destp,name) :
		 (!disnull(enamep) && fexist(enamep->d_str) == 0) ? savepath(destp,enamep->d_str) : rc.status;

	// Create name list.
	char *namelist[] = {			// Filenames to check.
		name,enamep->d_str,NULL};

	// Check HOME directory (only), if requested.
	if(homeOnly) {
		if((str1 = getenv("HOME")) != NULL) {
			char pathbuf[strlen(str1) + maxlen + 2];

			// Build pathname and check it.
			sprintf(pathbuf,"%s/%s",str1,name);
			if(fexist(pathbuf) == 0)
				return savepath(destp,pathbuf);
			}
		return rc.status;
		}

	// Now check the execpath directories.
	for(str2 = execpath; *str2 != '\0';) {
		char *dirsep;

		// Build next possible file spec.
		str1 = str2;
		while(*str2 != '\0' && *str2 != ':')
			++str2;

		// Add a terminating dir separator if needed.
		dirsep = "";
		if(str2 > str1 && str2[-1] != '/')
			dirsep = "/";

		// Loop through filename(s).
		char pathbuf[str2 - str1 + maxlen + 2];
		np = namelist;
		do {
			if((*np)[0] != '\0') {
				sprintf(pathbuf,"%.*s%s%s",(int) (str2 - str1),str1,dirsep,*np);
				if(fexist(pathbuf) == 0)
					return savepath(destp,pathbuf);
				}
			} while(*++np != NULL);

		// No go.  Onward...
		if(*str2 == ':')
			++str2;
		}

	// No such luck.
	return rc.status;
	}

// Get pathname of "fname" and return in "pathp".  Don't resolve it if it's a symbolic link and non-default n.  Return status.
int getpath(Datum *pathp,int n,char *fname) {
	char buf[MaxPathname + 1];

	if(n <= 0 && n != INT_MIN) {
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
			return librcset(Failure);
		if(realpath(fname = fdirname(fname,1),buf) == NULL)
			goto ErrRetn;
		if(dputs(buf,&sf) != 0 || (strcmp(buf,"/") != 0 && dputc('/',&sf) != 0) ||
		 dputs(bn,&sf) != 0 || dclose(&sf,sf_string) != 0)
			return librcset(Failure);
		}
	else {
RegFile:
		if(realpath(fname,buf) == NULL)
ErrRetn:
			return rcset(Failure,0,text33,text37,fname,strerror(errno));
				// "Cannot get %s of file \"%s\": %s","pathname"
		if(dsetstr(buf,pathp) != 0)
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