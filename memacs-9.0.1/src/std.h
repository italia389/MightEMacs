// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// std.h	Standard definitions for MightEMacs included in all C source files.

// ProLib include files.
#include "pldatum.h"
#include "plstring.h"

// Other standard include files.
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Program-logic (source code) debugging flags.
#define Debug_Logfile	0x0001		// Open file "logfile" for debugging output.
#define Debug_ScrDump	0x0002		// Dump screens, windows, and buffers.
#define Debug_CFAB	0x0004		// Show CFAB pointer type in "showCFAM" display.
#define Debug_Narrow	0x0008		// Dump buffer info to log file in narrowBuf().
#define Debug_KillRing	0x0010		// Include dispkill() function.
#define Debug_BufWindCt	0x0020		// Display buffer's window count in "showBuffers" display.
#define Debug_ShowRE	0x0040		// Include showRegexp command.
#define Debug_Token	0x0080		// Dump token-parsing info to log file.
#define Debug_Datum	0x0100		// Dump Datum processing info to log file.
#define Debug_MacArg	0x0200		// Dump macro-argument info to log file.
#define Debug_Script	0x0400		// Write script lines to log file.
#define Debug_Expr	0x0800		// Write expression-parsing info to log file.
#define Debug_PPBuf	0x1000		// Dump script preprocessor blocks to log file and exit.
#define Debug_Array	0x2000		// Dump array heap-management info.
#define Debug_Bind	0x4000		// Dump binding table.
#define Debug_Temp	0x8000

#define MMDebug		0		// No debugging code.
//#define MMDebug		Debug_Logfile
//#define MMDebug		Debug_Array
//#define MMDebug		(Debug_Array | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Expr | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Token | Debug_Expr | Debug_Logfile)
//#define MMDebug		(Debug_Datum | Debug_Script | Debug_Token | Debug_Expr | Debug_Logfile)
//#define MMDebug		(Debug_Token | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_MacArg | Debug_Logfile)
//#define MMDebug		(Debug_PPBuf | Debug_Logfile)
//#define MMDebug		(Debug_Bind | Debug_Logfile)
//#define MMDebug		Debug_ShowRE
//#define MMDebug		(Debug_Logfile | Debug_Temp)

#if MMDebug & Debug_MacArg
#define execbuf		execbufDebug
#endif

// Program identification.
#define ProgName	"MightEMacs"
#define ProgVer		"9.0.1"

/***** BEGIN CUSTOMIZATIONS *****/

// Terminal output definitions -- [Set one of these].
#define TT_TermCap	1		// Use TERMCAP.
#define TT_Curses	0		// Use CURSES (not working).

// Terminal size definitions -- [Set any except TT_MinCols and TT_MinRows].
#define TT_MinCols	40		// Minimum number of columns.
#define TT_MaxCols	240		// Maximum number of columns.
#define TT_MinRows	3		// Minimum number of rows.
#define TT_MaxRows	80		// Maximum number of rows.

// Language text options -- [Set one of these].
#define English		1		// Default.
#define Spanish		0		// If any non-English macro is set, must translate english.h to the corresponding
					// header file (e.g., spanish.h) and edit lang.h to include it.
// Configuration options -- [Set any].
#define WordCount	0		// Include code for "countWords" command (deprecated).
#define VizBell		0		// Use a visible Bell (instead of an audible one) if it exists.
#define KMDelims	":;,\"'"	// Keyboard macro encoding delimiters ($keyMacro), in order of preference.
#define DefWordList	"A-Za-z0-9_"	// Default $wordChars value.
#define Backup_Ext	".bak"		// Backup file extension.
#define Script_Ext	".mm"		// Script file extension.
#define UserStartup	".memacs"	// User start-up file (in HOME directory).
#define SiteStartup	"memacs.mm"	// Site start-up file.
#define MMPath_Name	"MMPATH"	// Shell environmental variable name.
#define MMPath_Std	":/usr/local/lib/memacs"	// Standard search path.
#define MMPath_Alt	":/usr/lib/memacs"	// Alternate search path (Debian platform).
#define MMPath_AltDir	"/usr/lib/memacs"	// Alternate MightEMacs directory (Debian platform).
#define RestoreTerm	1		// Restore original terminal display on exit from editor.
#define Logfile		"memacs.log"	// Log file (for debugging).

// Limits -- [Set any].
#define MaxTab		240		// Maximum hard or soft tab size.
#define NBufName	24		// Number of bytes, buffer name.
#define NTermInp	(MaxPathname < 1024 ? 1024 : MaxPathname)
					// Number of bytes, terminal input (must be >= MaxPathname).
#define NBlock		32		// Number of bytes, line block chunks.
#define NKillRing	40		// Default number of buffers in kill ring.
#define NPatRing	20		// Default number of buffers in search and replacement rings.
#define NVarName	32		// Maximum number of characters in a user variable name (including prefix).
#define NASave		220		// Number of keystrokes before auto-save -- initial value.
#define MaxLoop		2500		// Default maximum number of script loop iterations allowed.
#define MaxMacroDepth	100		// Default maximum macro recursion depth allowed during script execution.
#define MaxArrayDepth	30		// Default maximum array recursion depth allowed when cloning, etc.
#define MaxPromptPct	80		// Default maximum percentage of terminal width for prompt string (in range 15-90).
#define FPause		26		// Default time in centiseconds to pause for fence matching.
#define TravJump	12		// Default line-traversal jump size.
#define TravJumpStr	"12"		// TravJump in string form.
#define JumpMax		49		// Maximum horizontal or vertical jump size (percentage).
#define JumpMaxStr	"49"		// JumpMax in string form.

/***** END CUSTOMIZATIONS *****/

// OS identification.
#if MACOS || LINUX
#	define OSName_CentOS	"CentOS Linux"
#	define OSName_Debian	"Debian Linux"
#	define OSName_MacOS	"macOS"
#	define OSName_RedHat	"Red Hat Linux"
#	define OSName_Ubuntu	"Ubuntu Linux"
#	define VersKey_Debian	"debian"
#	define VersKey_MacOS	"darwin"
#	define VersKey_Ubuntu	"ubuntu"
#	define CentOS_Release	"/etc/centos-release"
#	define RedHat_Release	"/etc/redhat-release"
#else
#	define OSName	"Unix"
#endif

// Miscellaneous.
#define MLExtend	0		// Include additional printf specifiers in mlprintf() function (deprecated).
#define Scratch		"scratch"	// Prefix for scratch buffers.
#define Buffer1		"untitled"	// First buffer created.
#define HiChar		256		// HiChar - 1 is the largest character we will deal with.

// Internal constants.
#define NKeyTab		5		// Number of key binding tables (no prefix, Meta, ^C, ^H, and ^X).
#define NKeyVect	(128 + 94 + 1 + 94)	// Number of slots in key binding vector (for one prefix key or no prefix).
#define NDelim		2		// Number of bytes, input and output record delimiters.
#define NPatMin		32		// Minimum array size for search or replacement pattern.
#define NPatMax		96		// Maximum array size to retain for search or replacement pattern.
#define NKbdChunk	48		// Extension size of keyboard macro buffer when full.
#define NWork		80		// Number of bytes, work buffer.
#define RMark		' '		// Mark which defines region endpoint.
#define RMarkLit	"SPC"		// Literal form of RMark (used in prompts).
#define WMark		'.'		// Work mark, used as default for certain commands.

// Operation flags used at runtime (in "opflags" global variable).
#define OpVTOpen	0x0001		// Virtual terminal open?
#define OpEval		0x0002		// Evaluate expressions?
#define OpHaveRev	0x0004		// Does reverse video exist?
#define OpHaveBold	0x0008		// Does bold exist?
#define OpHaveUL	0x0010		// Does underline exist?
#define OpStartup	0x0020		// In pre-edit-loop state?
#define OpScript	0x0040		// Script execution in progress?
#define OpParens	0x0080		// Command, alias, macro, or system function invoked in xxx() form.
#define OpNoLoad	0x0100		// Do not load function arguments (non-command-line hook is running).
#define OpScrRedraw	0x0200		// Clear and redraw screen if set.

// Terminal attribute flags.
#define AttrSeqBegin	'~'		// First character of a terminal attribute sequence.
#define AttrAlt		'#'		// Alternate attribute form.
#define AttrBoldOn	'b'		// Bold on.
#if TT_Curses
#define AttrBoldOff	'B'		// Bold off.
#endif
#define AttrRevOn	'r'		// Reverse on.
#if TT_Curses
#define AttrRevOff	'R'		// Reverse off.
#endif
#define AttrULOn	'u'		// Underline on.
#define AttrULOff	'U'		// Underline off.
#define AttrAllOff	'0'		// All attributes off.

// Flags used by bdefault().
#define BDefBack	0x0001		// Search backward through buffer list; otherwise, forward.
#define BDefTwo		0x0002		// Proceed only if two visible buffers exist; otherwise, return NULL.
#define BDefHidden	0x0004		// Include hidden, non-macro buffers as candidates.

// Buffer operation flags used by bufop().
#define BOpBeginEnd	1		// Move to beginning or end of buffer.
#define BOpGotoLn	2		// Go to to a line in the buffer.
#define BOpReadBuf	3		// Read line(s) from a buffer.

// Flags used by catargs(), dtosf(), and atosf() for controlling conversions to string.
#define CvtExpr		0x0001		// Output as an expression; otherwise, as data.
#define CvtShowNil	0x0002		// Use "nil" for nil; otherwise, null.
#define CvtForceArray	0x0004		// If array contains itself, display with ellipsis; otherwise, return error.
#define CvtTermAttr     0x0008          // Output strings with terminal attribute sequences escaped (~~).
#define CvtVizStr	0x0010		// Output strings in visible form; otherwise, unmodified.
#define CvtVizStrQ	0x0020		// Output strings in visible form enclosed in single (') quotes.

#define CvtKeepNil	0x0040		// Keep nil arguments.
#define CvtKeepNull	0x0080		// Keep null arguments.
#define CvtKeepAll	(CvtKeepNil | CvtKeepNull)

// Flags used by cvtcase() for controlling case conversions.
#define CaseWord	0x0001		// Convert word(s).
#define CaseLine	0x0002		// Convert line(s).
#define CaseRegion	0x0004		// Convert current region.
#define CaseLower	0x0008		// Convert to lower case.
#define CaseTitle	0x0010		// Convert to title case.
#define CaseUpper	0x0020		// Convert to upper case.

// Information display characters.
#define MacFormat	"@%.*s"		// sprintf() format string for prepending prefix to macro name.
#define AltBufCh	'*'		// Substitution character for non-macro buffer names that begin with SBMacro.
#define SBActive	':'		// BFActive flag indicator (activated buffer -- file read in).
#define SBChanged	'*'		// BFChanged flag indicator (changed buffer).
#define SBHidden	'?'		// BFHidden flag indicator (hidden buffer).
#define SBMacro		'@'		// BFMacro flag indicator (macro buffer).
#define SBPreproc	'+'		// BFPreproc flag indicator (preprocessed buffer).
#define SBNarrowed	'<'		// BFNarrowed flag indicator (narrowed buffer).
#define SBTermAttr	'~'		// BFTermAttr flag indicator (terminal attributes enabled).

// Key prefixes.
#define Ctrl		0x0100		// Control flag.
#define Meta		0x0200		// Meta flag.
#define Pref1		0x0400		// Prefix1 (^X) flag.
#define Pref2		0x0800		// Prefix2 (^C) flag.
#define Pref3		0x1000		// Prefix3 (^H) flag.
#define Shft		0x2000		// Shift flag (for function keys).
#define FKey		0x4000		// Special key flag (function key).
#define Prefix		(Meta | Pref1 | Pref2 | Pref3)	// Prefix key mask.

#define RtnKey		(Ctrl | 'M')	// "return" key as an extended key.
#define AltRtnKey	(Ctrl | 'J')	// newline as an extended key (alternate return key).

// Command return status codes.  Note that NotFound, IONSF, and IOEOF are never actually set via rcset() (so rc.status will
// never be one of those codes); whereas, all other status codes are always set, either explicitly or implicitly.
#define Panic		-11		// Panic return -- exit immediately (from rcset()).
#define OSError		-10		// Fatal OS error with errno lookup.
#define FatalError	-9		// Fatal system or library error.
#define ScriptExit	-8		// Script forced exit with dirty buffer(s).
#define UserExit	-7		// Clean buffer(s) or user forced exit with dirty ones.
#define HelpExit	-6		// -?, -h, or -V switch.
#define MinExit		HelpExit	// Minimum severity which causes program exit.
#define ScriptError	-5		// Last command failed during script execution.
#define Failure		-4		// Last command failed.
#define UserAbort	-3		// Last command aborted by user.
#define Cancelled	-2		// Last command cancelled by user.
#define NotFound	-1		// Last search or item retrieval was unsuccessful.
#define Success		0		// Last command succeeded.
#define IONSF		1		// "No such file" return on open.
#define IOEOF		2		// "End of file" return on read.

// Toggle-able values for routines that need directions.
#define Forward		0		// Do things in a forward direction.
#define Backward	1		// Do things in a backward direction.

#define Bell		0x07		// A bell character.
#define Tab		0x09		// A tab character.

#define LongWidth	sizeof(long) * 3

// Return code information from a command.
typedef struct {
	short status;			// Most severe status returned from any C function.
	ushort flags;			// Flags.
	char *helpText;			// Command-line help message (-?, -C, -h, or -V switch).
	Datum msg;			// Status message, if any.
	} RtnCode;

// Return code flags.
#define RCNoFormat	0x0001		// Don't call vasprintf() or parse terminal attributes; use format string verbatim.
#define RCNoWrap	0x0002		// Don't wrap Success message.
#define RCForce		0x0004		// Force-save new message of equal severity.
#define RCKeepMsg	0x0008		// Don't replace any existing message (just change severity).
#define RCUnFail	0x0010		// Reset severity to Success if currently Failure.
#define RCTermAttr	0x0020		// Enable terminal attributes in message.

// Sample string buffer used for error reporting -- allocated at program launch according to the terminal width.
typedef struct {
	char *buf;			// Buffer for sample string, often ending in "...".
	ushort buflen;			// Size of buffer (allocated from heap).
	ushort smallsize;		// Small sample size.
	} SampBuf;

// Keyboard macro information.
typedef struct {
	uint km_size;			// Current size of km_buf.
	ushort *km_slotp;		// Pointer to next slot in buffer.
	ushort *km_endp;		// Pointer to end of the last macro recorded.
	ushort km_state;		// Current state.
	int km_n;			// Requested number of repetitions (0 = infinite).
	ushort *km_buf;			// Macro buffer (allocated from heap).
	} KMacro;

// Keyboard macro states.
#define KMStop		0		// Not in use.
#define KMPlay		1		// Playing.
#define KMRecord	2		// Recording.

// Text insertion style.
#define Txt_Insert	0x0001
#define Txt_Overwrite	0x0002
#define Txt_Replace	0x0004
#define Txt_LiteralRtn	0x0008		// Literal "RTN" character.

// The editor communicates with the display using a high level interface.  A ETerm structure holds useful variables and
// indirect pointers to routines that do useful operations.  The low level get and put routines are here, too.  This lets a
// terminal, in addition to having non-standard commands, have funny get and put character code, too.  The calls might get
// changed to "termp->t_field" style in the future, to make it possible to run more than one terminal type.
typedef struct {
	int t_mcol;			// Maximum number of columns allowed (allocated).
	int t_ncol;			// Current number of columns.
	int t_mrow;			// Maximum number of rows allowed (allocated).
	int t_nrow;			// Current number of rows used.
	int (*t_open)(void);		// Open terminal.
	int (*t_close)(ushort flags);	// Close terminal.
	int (*t_kopen)(void);		// Open keyboard.
	int (*t_kclose)(void);		// Close keyboard.
	int (*t_getchar)(ushort *);	// Get character from keyboard.
	int (*t_putchar)(int);		// Put character to display.  Argument must be int -- function is passed to tputs().
	int (*t_flush)(void);		// Flush output buffers.
	int (*t_move)(int,int);		// Move the cursor, origin 0.
	int (*t_eeol)(void);		// Erase to end of line.
	int (*t_eeop)(void);		// Erase to end of page.
	int (*t_beep)(void);		// Beep.
#if TT_Curses
	int (*t_rev)(bool);		// Set reverse video state.
	int (*t_bold)(bool);		// Set bold state.
#else
	int (*t_rev)(void);		// Enter reverse video state.
	int (*t_bold)(void);		// Enter bold state.
#endif
	int (*t_ul)(bool);		// Set underline state.
	int (*t_attroff)(void);		// Turn all attributes off.
	} ETerm;

// Macros for terminal I/O indirect function calls.
#define TTopen		(*term.t_open)
#define TTclose		(*term.t_close)
#define TTkopen		(*term.t_kopen)
#define TTkclose	(*term.t_kclose)
#define TTgetc		(*term.t_getchar)
#define TTputc		(*term.t_putchar)
#define TTflush		(*term.t_flush)
#define TTmove		(*term.t_move)
#define TTeeol		(*term.t_eeol)
#define TTeeop		(*term.t_eeop)
#define TTbeep		(*term.t_beep)
#define TTrev		(*term.t_rev)
#define TTbold		(*term.t_bold)
#define TTul		(*term.t_ul)
#define TTattroff	(*term.t_attroff)

// Terminal open/close options.
#define TermForce	0x0001		// Force close (ignore any errors).
#if RestoreTerm
#define TermCup		0x0002		// Issue begin and end "cup" terminfo sequences.
#endif

// Operation types.  These are mutually exclusive, however they are coded as bit masks so they can be combined with other flags.
#define OpQuery		0x0001		// Find an item.
#define OpCreate	0x0002		// Create an item.
#define OpDelete	0x0004		// Delete an item.

// The editor holds text chunks in a double-linked list of buffers (the kill ring and search ring).  Chunks are stored in
// a Datum object so that text (of unpredictable size) can be saved iteratively and bidirectionally.  This can also be used in
// the future for other data types.
typedef struct RingEntry {
	struct RingEntry *re_prevp;	// Pointer to previous item in ring.
	struct RingEntry *re_nextp;	// Pointer to next item in ring.
	Datum re_data;			// Data item (text chunk).
	} RingEntry;

typedef struct {
	RingEntry *r_entryp;		// Current ring item.
	ushort r_size;			// Number of items in ring.
	ushort r_maxsize;		// Maximum size of ring.
	char *r_name;			// Name of ring (for exception messages).
	char *r_ename;			// Name of ring entry (for exception messages).
	} Ring;

// Position of dot in a buffer.
typedef struct {
	struct Line *lnp;		// Pointer to Line structure.
	int off;			// Offset of dot in line.
	} Dot;

// Message line print flags.
#define MLHome		0x0001		// Move cursor to beginning of message line before display.
#define MLForce		0x0002		// Force output (ignore 'msg' global mode).
#define MLTermAttr	0x0004		// Enable interpretation of terminal attribute sequences in message.
#define MLNoSpace	0x0008		// Skip spaces when underlining in mlputc().
#define MLWrap		0x0010		// Wrap message [like this].
#define MLRaw		0x0020		// Output raw character; otherwise, convert to visible form if needed.
#define MLNoEOL		0x0040		// Don't overwrite char at right edge of screen with LineExt ($) char.
#define MLFlush		0x0080		// Flush output in mlputs() and mlprintf().

// Settings that determine a window's "face"; that is, the location of dot in the buffer and in the window.
typedef struct {
	struct Line *wf_toplnp;		// Pointer to top line of window.
	Dot wf_dot;			// Dot position.
	int wf_firstcol;		// First column displayed when horizontal scrolling is enabled.
	} WindFace;

// There is a window structure allocated for every active display window.  The windows are kept in a list, in top to bottom
// screen order.  The list is associated with one screen and the list head is stored in "wheadp" whenever the screen is current.
// Each window contains its own values of dot and mark.  The flag field contains some bits that are set by commands to guide
// redisplay.  Although this is a bit of a compromise in terms of decoupling, the full blown redisplay is just too expensive
// to run for every input character.
typedef struct EWindow {
	struct EWindow *w_nextp;	// Next window.
	struct Buffer *w_bufp;		// Buffer displayed in window.
	WindFace w_face;		// Dot position, etc.
	ushort w_id;			// Unique window identifier (mark used to save face before it's buffer is narrowed).
	ushort w_toprow;		// Origin 0 top row of window.
	ushort w_nrows;			// Number of rows in window, excluding mode line.
	short w_rfrow;			// Target (reframe) row in window for line containing dot.
	ushort w_flags;			// Flags.
	} EWindow;

#define WFReframe	0x0001		// Window needs forced reframe.
#define WFMove		0x0002		// Movement from line to line.
#define WFEdit		0x0004		// Current line was edited.
#define WFHard		0x0008		// Full window update needed.
#define WFMode		0x0010		// Update mode line.

// This structure holds the information about each separate "screen" within the current editing session.  On a character based
// system (Text User Interface), which is used by MightEMacs, these screens overlay each other, and can individually be brought
// to the front.  On a windowing system like X-windows, OS X, MicroSoft Windows, etc., each screen is represented in an OS
// window.  The terminolgy is different in Emacs...
//
//	Emacs		The Outside World
//	------		-----------------
//	screen		window
//	window		pane
//
// Each EScreen structure contains its own private list of windows (EWindow structure).
typedef struct EScreen {
	struct EScreen *s_nextp;	// Pointer to next screen in list.
	EWindow *s_wheadp;		// Head of linked list of windows.
	EWindow *s_curwp;		// Current window in this screen.
	struct Buffer *s_lastbufp;	// Last buffer exited from.
	ushort s_num;			// Screen number (first is 1).
	ushort s_flags;			// Flags.
	ushort s_nrow;			// Height of screen.
	ushort s_ncol;			// Width of screen.
	char *s_wkdir;			// Working directory associated with screen.
	int s_cursrow;			// Current cursor row in buffer portion of window.
	int s_curscol;			// Current cursor column in buffer portion of window.
	int s_firstcol;			// First display column of current line when horizontal scrolling is disabled.
	} EScreen;

// Flags in s_flags.
#define EScrResize	0x0001		// Resize screen window(s) vertically when screen is frontmost.

// Flags for gotoScreen() and gotoWind() functions.
#define EScrWinRepeat	0x0001		// n argument is a repeat count.
#define EScrWinForw	0x0002		// Repeat forward; otherwise, backward.

// Dot mark in a buffer and flags.
typedef struct mark {
	struct mark *mk_nextp;		// Next mark.
	ushort mk_id;			// Mark identifier.
	short mk_rfrow;			// Target (reframe) row in window for dot.
	Dot mk_dot;			// Dot position.  Mark is invalid if mk_dot.lnp == NULL.
	} Mark;

#define MkOpt_AutoR	0x0001		// Use mark RMark if default n and mark WMark if n < 0; otherwise, prompt with no
					// default (setMark, swapMark).
#define MkOpt_AutoW	0x0002		// Use mark WMark if default n or n < 0; otherwise, prompt with no default (markBuf).
#define MkOpt_Hard	0x0004		// Always prompt, no default (deleteMark, gotoMark).
#define MkOpt_Viz	0x0008		// Mark must exist and be visible (gotoMark, swapMark).
#define MkOpt_Exist	0x0010		// Mark must exit (deleteMark).
#define MkOpt_Create	0x0020		// Mark does not have to exist (setMark, markBuf).
#define MkOpt_Query	0x0040		// Query mode.
#define MkOpt_Wind	0x0080		// Mark is a window id.

// Additional information for a (macro) buffer that contains a script.
typedef struct {
	struct LoopBlock *mi_execp;	// Pointer to compiled macro loop blocks.
	short mi_minArgs;		// Declared minimum argument count.
	short mi_maxArgs;		// Declared maximum argument count (-1 if variable).
	ushort mi_nexec;		// Count of active executions.
	Datum mi_usage;			// Macro usage text, if any.
	Datum mi_desc;			// Macro descriptive text, if any.
	} MacInfo;

// Text is kept in buffers.  A buffer header, described below, exists for every buffer in the system.  Buffers are kept in a
// linked list sorted by buffer name to speed up searching and eliminate the need to sort the "showBuffers" listing.  There is a
// safe store for a window's top line, dot, and first display column so that a window can be cleanly restored to the same point
// in a buffer (if possible) when switching among them.  b_nwind is the number of windows (on any screen) that a buffer is
// associated with.  Thus if the value is zero, the buffer is not being displayed anywhere.  The text for a buffer is kept in a
// circularly linked list of lines with a pointer to the header line in b_hdrlnp (which is sometimes referred to in the code as
// the "magic" line).  The header line is just a placeholder; it always exists but doesn't contain any text.  When a buffer is
// first created or cleared, the header line will point to itself, both forward and backward.  A buffer may be "inactive" which
// means the file associated with it has not been read in yet.  The file is read at "use buffer" time.
typedef struct Buffer {
	struct Buffer *b_prevp;		// Pointer to previous buffer.
	struct Buffer *b_nextp;		// Pointer to next buffer.
	WindFace b_face;		// Dot position, etc. from last detached window.
	struct Line *b_hdrlnp;		// Pointer to header (magic) line.
	struct Line *b_ntoplnp;		// Pointer to narrowed top text.
	struct Line *b_nbotlnp;		// Pointer to narrowed bottom text.
	Mark b_mroot;			// Dot mark RMark and list root.
	MacInfo *b_mip;			// Pointer to macro parameters, if applicable.
	EScreen *b_lastscrp;		// Last screen buffer was displayed on.
	ushort b_nwind;			// Count of windows displaying buffer.
	ushort b_nalias;		// Count of aliases pointing to this (macro) buffer.
	ushort b_flags;			// Flags.
	uint b_modes;			// Buffer modes.
	ushort b_inpdelimlen;		// Length of input delimiter string.
	char b_inpdelim[NDelim + 1];	// Record delimiters used to read buffer.
	char *b_fname;			// File name.
	char b_bname[NBufName + 1];	// Buffer name.
	} Buffer;

// Descriptor for buffer flags.
typedef struct {
	char *name;			// Name of flag.
	char *abbr;			// Abbreviation or NULL, if immutable.
	ushort mask;			// Bit mask.
	} BufFlagSpec;

// Buffer flags.
#define BFActive	0x0001		// Active buffer (file was read).
#define BFChanged	0x0002		// Changed since last write.
#define BFHidden	0x0004		// Hidden buffer.
#define BFMacro		0x0008		// Buffer is a macro.
#define BFNarrowed	0x0010		// Buffer is narrowed.
#define BFPreproc	0x0020		// (Script) buffer has been preprocessed.
#define BFQSave		0x0040		// Buffer was saved via quickExit().
#define BFTermAttr	0x0080		// Terminal attributes enabled.

#define BSysLead	'.'		// Leading character of system (internal) buffer.

// Buffer creation flags.
#define CRBQuery	0x0000		// Look-up only (do not create).
#define CRBCreate	0x0001		// Create buffer if non-existent.
#define CRBExtend	0x0002		// Create buffer-extension record (MacInfo).
#define CRBForce	0x0004		// Force-create buffer with unique name.
#define CRBFile		0x0008		// Derive buffer name from filename.
#define CRBHook		0x0010		// Execute "createBuf" hook if a buffer is created.

// Buffer clearing flags.
#define CLBIgnChgd	0x0001		// Ignore BFChanged (buffer changed) flag.
#define CLBUnnarrow	0x0002		// Force-clear narrowed buffer (unnarrow first).
#define CLBClrFilename	0x0004		// Clear associated filename, if any.
#define CLBMulti	0x0008		// Processing multiple buffers.

// Buffer rendering flags.
#define RendReset	0x0001		// Move point to beginning of buffer and unhide it if buffer is not deleted.
#define RendAltML	0x0002		// Use alternate (simple) mode line when popping buffer.
#define RendWait	0x0004		// Wait for user to press a key before returning from pop-up window.
#define	RendShift	0x0008		// Shift display left if needed to prevent line truncation in pop-up window.
#define RendNewBuf	0x0010		// Buffer was just created (delete after pop).

// Buffer saving flags.
#define SVBAll		0x0001		// Saving all buffers; otherwise, just current one.
#define SVBMultiDir	0x0002		// Multiple working directories exist (in screens).
#define SVBQExit	0x0004		// Called from quickExit command.

// File reading and writing flags.
#define RWExist		0x0001		// File must exist.
#define RWKeep		0x0002		// Keep existing filename associated with buffer.
#define RWScratch	0x0004		// Create scratch buffer.
#define RWStats		0x0008		// Return message containing I/O statistics.

// Descriptor for global and buffer modes.
typedef struct {
	char *name;			// Name of mode in lower case.
	char *mlname;			// Name displayed on mode line.
	uint mask;			// Bit mask identifier (read only).
	char *desc;			// Description.
	} ModeSpec;

// Global mode bit masks.
#define MdASave		0x00000001	// Auto-save mode.
#define MdBak		0x00000002	// File backup mode.
#define MdClob		0x00000004	// Macro-clobber mode.
#define MdExact		0x00000008	// Exact matching for searches.
#define MdHScrl		0x00000010	// Horizontal-scroll-all mode.
#define MdMsg		0x00000020	// Message line display mode.
#define MdRegexp	0x00000040	// Regular expresions in search.
#define MdSafe		0x00000080	// Safe file save mode.
#define MdWkDir		0x00000100	// Working directory display mode.

// Buffer mode bit masks -- language.
#define MdC		0x00010000	// C indentation and fence match.
#define MdMemacs	0x00020000	// MightEMacs indentation.
#define MdPerl		0x00040000	// Perl indentation and fence match.
#define MdRuby		0x00080000	// Ruby indentation and fence match.
#define MdShell		0x00100000	// Shell indentation and fence match.

// Buffer mode bit masks -- non-language.
#define MdCol		0x00200000	// Column position display mode.
#define MdLine		0x00400000	// Line number display mode.
#define MdOver		0x00800000	// Overwrite mode.
#define MdRdOnly	0x01000000	// Read-only buffer.
#define MdRepl		0x02000000	// Replace mode.
#define MdWrap		0x04000000	// Word wrap.
#define MdXIndt		0x08000000	// Extra fence indentation mode.

// Mode masks.
#define MdGlobal	0x80000000	// Global mode (otherwise, buffer).
#define MdAll_Global	0x000001FF	// All possible global modes.
#define MdAll_Buffer	0x0FFF0000	// All possible buffer modes.
#define MdGrp_Repl	(MdOver | MdRepl)
#define MdGrp_Lang	(MdC | MdMemacs | MdPerl | MdRuby | MdShell)

// Structure for non-buffer modes.
typedef struct {
	uint flags;			// Mode flags (bit masks).
	char *cmdlabel;			// Unique portion of command name in long form.
	} ModeRec;

#define MdIdxGlobal	0		// "Global" table index.
#define MdIdxShow	1		// "Show" (global) table index.
#define MdIdxBuffer	2		// "Buffer" PSEUDO table index.

// The starting position of a region, and the size of the region in characters, is kept in a region structure.
// Used by the region commands.
typedef struct {
	Dot r_dot;			// Origin Line address.
	long r_size;			// Length in characters.
	} Region;

// All text is kept in circularly linked lists of "Line" structures.  These begin at the header line (which is the blank line
// beyond the end of the buffer).  This line is pointed to by the "Buffer".  Each line contains a the number of bytes in the
// line (the "used" size), the size of the text array, and the text.  The end of line is not stored as a byte; it's implied.
typedef struct Line {
	struct Line *l_nextp;		// Pointer to the next line.
	struct Line *l_prevp;		// Pointer to the previous line.
	int l_size;			// Allocated size.
	int l_used;			// Used size.
	char l_text[1];			// A bunch of characters.
	} Line;

// Editing flags.
#define EditKill	0x0001		// Kill operation (save text in kill ring).
#define EditDel		0x0002		// Delete operation (save text in "undelete" buffer).
#define EditSpace	0x0004		// Insert space character; otherwise, newline.
#define EditWrap	0x0008		// Do word wrap, if applicable.
#define EditHoldPt	0x0010		// Hold (don't move) point.

// Structure and flags for the command-function table.  Flag rules are:
//  1. CFEdit and CFSpecArgs may not both be set.
//  2. CFIntN and CFArrayN may not both be set.
//  3. If CFAddlArg or CFNoArgs set, cf_maxArgs must be greater than zero.
//  4. If no argument type flag (CFNilN, CFBoolN, CFIntN, or CFArrayN) is set, the corresponding Nth argument must be string;
//     otherwise, if CFIntN or CFArrayN is set, the Nth argument must be (or may be if CFMay set) that type; otherwise, the Nth
//     argument must be string, nil (if CFNilN set), or Boolean (if CFBoolN set).
//  5. If CFNISN is set, the Nth argument may be any type except array unless CFArrayN is also set.
typedef struct {
	char *cf_name;			// Name of command or function.
	ushort cf_aflags;		// Attribute flags.
	uint cf_vflags;			// Script-argument validation flags.
	short cf_minArgs;		// Minimum number of required arguments with default n argument.
	short cf_maxArgs;		// Maximum number of arguments allowed for any value of n.  No maximum if negative.
	int (*cf_func)(Datum *rp,int n,Datum **argpp);// C function that command or system function is bound to.
	char *cf_usage;			// Usage text.
	char *cf_desc;			// Short description.
	} CmdFunc;

// Attribute flags.
#define CFFunc		0x0001		// Is system function.
#define CFHidden	0x0002		// Hidden: for internal use only.
#define CFPrefix	0x0004		// Prefix command (meta, ^C, ^H, and ^X).
#define CFBind1		0x0008		// Is bound to a single key (use getkey() in bindcmd() and elsewhere).
#define CFUniq		0x0010		// Cannot have more than one binding.
#define CFEdit		0x0020		// Modifies current buffer.
#define CFPerm		0x0040		// Must have one or more bindings at all times.
#define CFTerm		0x0080		// Terminal (interactive) only -- not recognized in a script.
#define CFNCount	0x0100		// N argument is purely a repeat count.
#define CFSpecArgs	0x0200		// Needs special argument processing (bare word or symbol) -- never skipped.
#define CFAddlArg	0x0400		// Takes additional argument if n argument is not the default.
#define CFNoArgs	0x0800		// Takes no arguments if n argument is not the default.
#define CFShrtLoad	0x1000		// Load one fewer argument than usual in execCF().
#define CFNoLoad	0x2000		// Load no arguments in execCF().

// Validation flags.
#define CFNotNull1	0x00000001	// First argument may not be null.
#define CFNotNull2	0x00000002	// Second argument may not be null.
#define CFNotNull3	0x00000004	// Third argument may not be null.
#define CFNil1		0x00000008	// First argument may be nil.
#define CFNil2		0x00000010	// Second argument may be nil.
#define CFNil3		0x00000020	// Third argument may be nil.
#define CFBool1		0x00000040	// First argument may be Boolean (true or false).
#define CFBool2		0x00000080	// Second argument may be Boolean (true or false).
#define CFBool3		0x00000100	// Third argument may be Boolean (true or false).
#define CFInt1		0x00000200	// First argument must be integer.
#define CFInt2		0x00000400	// Second argument must be integer.
#define CFInt3		0x00000800	// Third argument must be integer.
#define CFArray1	0x00001000	// First argument must be array (or "may" be if CFNIS1 or CFMay also set).
#define CFArray2	0x00002000	// Second argument must be array (or "may" be if CFNIS2 or CFMay also set).
#define CFArray3	0x00004000	// Third argument must be array (or "may" be if CFNIS3 or CFMay also set).
#define CFNIS1		0x00008000	// First argument can be nil, integer, or string.
#define CFNIS2		0x00010000	// Second argument can be nil, integer, or string.
#define CFNIS3		0x00020000	// Third argument can be nil, integer, or string.
#define CFMay		0x00040000	// CFIntN or CFArrayN "may" be instead of "must" be.

#define CFMaxArgs	3		// Maximum number of arguments that could be loaded by execCF() for any command or
					// function; that is, maximum value of cf_maxArgs in cftab where CFNoLoad is not set and
					// cf_maxArgs > 0.

// Pointer structure for places where either a command, function, alias, buffer, or macro is allowed.
typedef struct {
	ushort p_type;			// Pointer type.
	union {
		CmdFunc *p_cfp;		// Pointer into the function table.
		struct alias *p_aliasp;	// Alias pointer.
		struct Buffer *p_bufp;	// Buffer pointer.
		void *p_voidp;		// For generic pointer comparisons.
		} u;
	} CFABPtr;

// Pointer types.  Set to different bits so that can be used as selector masks in function calls.
#define PtrNul		0x0000		// NULL pointer.
#define PtrCmd		0x0001		// Command-function pointer -- command
#define PtrPseudo	0x0002		// Command-function pointer -- pseudo-command
#define PtrFunc		0x0004		// Command-function pointer -- function
#define PtrAlias_C	0x0008		// Alias pointer to a command.
#define PtrAlias_F	0x0010		// Alias pointer to a function.
#define PtrAlias_M	0x0020		// Alias pointer to a macro.
#define PtrBuf		0x0040		// Buffer pointer.
#define PtrMacro	0x0080		// Macro (buffer) pointer.

#define PtrCmdType	(PtrCmd | PtrPseudo)
#define PtrAlias	(PtrAlias_C | PtrAlias_F | PtrAlias_M)
#define PtrCFAM		(PtrCmd | PtrFunc | PtrAlias | PtrMacro)
#define PtrAny		(PtrCmd | PtrPseudo | PtrFunc | PtrAlias | PtrBuf | PtrMacro)

// Structure for the alias list.
typedef struct alias {
	struct alias *a_nextp;		// Pointer to next alias.
	ushort a_type;			// Alias type (PtrAlias_X).
	CFABPtr a_cfab;			// Command, function, or macro pointer.
	char a_name[1];			// Name of alias.
	} Alias;

// Structure for the command/function/alias/macro list, which contains pointers to all the current command, function, alias, and
// macro names.  Used for completion lookups.
typedef struct cfamrec {
	struct cfamrec *fr_nextp;	// Pointer to next CFAM record.
	ushort fr_type;			// Pointer type (PtrXXX).
	char *fr_name;			// Name of command, function, alias, or macro.
	} CFAMRec;

// Structure for the hook table and indices into hooktab array for each type.  Hooks are commands or macros that are invoked
// automatically when certain events occur during the editing session.
typedef struct {
	char *h_name;			// Name of hook.
	char *h_narg;			// Description of n argument passed to macro.
	char *h_margs;			// Description of arguments passed to macro.
	struct Buffer *h_bufp;		// Macro to execute.
	} HookRec;

#define HkChDir		0		// Change directory hook.
#define HkCreateBuf	1		// Create buffer hook.
#define HkEnterBuf	2		// Enter buffer hook.
#define HkExitBuf	3		// Exit buffer hook.
#define HkHelp		4		// Help hook.
#define HkMode		5		// Mode hook.
#define HkPostKey	6		// Post-key hook.
#define HkPreKey	7		// Pre-key hook.
#define HkRead		8		// Read file or change buffer filename hook.
#define HkWrap		9		// Word wrap hook.
#define HkWrite		10		// Write file hook.

// Command-function ids.
enum cfid {
	cf_abort,cf_about,cf_abs,cf_alias,cf_alterBufAttr,cf_alterBufMode,cf_alterGlobalMode,cf_alterShowMode,cf_appendFile,
	cf_apropos,cf_array,cf_backChar,cf_backLine,cf_backPage,cf_backPageNext,cf_backPagePrev,cf_backTab,cf_backWord,
	cf_backspace,cf_basename,cf_beep,cf_beginBuf,cf_beginKeyMacro,cf_beginLine,cf_beginText,cf_beginWhite,cf_bgets,
	cf_bindKey,cf_binding,cf_bprint,cf_bprintf,cf_bufAttrQ,cf_bufBoundQ,cf_bufModeQ,cf_bufSize,cf_bufWind,cf_chDir,cf_chr,
	cf_clearBuf,cf_clearMsg,cf_clone,cf_copyFencedRegion,cf_copyLine,cf_copyRegion,cf_copyToBreak,cf_copyWord,
#if WordCount
	cf_countWords,
#endif
	cf_cycleKillRing,cf_cycleReplaceRing,cf_cycleSearchRing,cf_definedQ,cf_deleteAlias,cf_deleteBackChar,cf_deleteBackTab,
	cf_deleteBlankLines,cf_deleteBuf,cf_deleteFencedRegion,cf_deleteForwChar,cf_deleteForwTab,cf_deleteKill,cf_deleteLine,
	cf_deleteMacro,cf_deleteMark,cf_deleteRegion,cf_deleteReplacePat,cf_deleteScreen,cf_deleteSearchPat,cf_deleteToBreak,
	cf_deleteWhite,cf_deleteWind,cf_deleteWord,cf_detabLine,cf_dirname,cf_dupLine,cf_emptyQ,cf_endBuf,cf_endKeyMacro,
	cf_endLine,cf_endWhite,cf_endWord,cf_entabLine,cf_env,cf_eval,cf_exit,cf_failure,cf_filterBuf,cf_findFile,cf_forwChar,
	cf_forwLine,cf_forwPage,cf_forwPageNext,cf_forwPagePrev,cf_forwTab,cf_forwWord,cf_getInfo,cf_getKey,cf_globalModeQ,
	cf_gotoFence,cf_gotoLine,cf_gotoMark,cf_growWind,cf_help,cf_huntBack,cf_huntForw,cf_includeQ,cf_indentRegion,cf_index,
	cf_insert,cf_insertBuf,cf_insertFile,cf_insertLineI,cf_insertPipe,cf_insertSpace,cf_inserti,cf_interactiveQ,cf_join,
	cf_joinLines,cf_joinWind,cf_keyPendingQ,cf_kill,cf_killFencedRegion,cf_killLine,cf_killRegion,cf_killToBreak,
	cf_killWord,cf_lastBuf,cf_length,cf_let,cf_lowerCaseLine,cf_lowerCaseRegion,cf_lowerCaseString,cf_lowerCaseWord,
	cf_markBuf,cf_match,cf_metaPrefix,cf_moveWindDown,cf_moveWindUp,cf_narrowBuf,cf_negativeArg,cf_newline,cf_newlineI,
	cf_nextBuf,cf_nextScreen,cf_nextWind,cf_nilQ,cf_nullQ,cf_numericQ,cf_onlyWind,cf_openLine,cf_ord,cf_outdentRegion,
	cf_overwrite,cf_pathname,cf_pause,cf_pop,cf_popBuf,cf_popFile,cf_prefix1,cf_prefix2,cf_prefix3,cf_prevBuf,cf_prevScreen,
	cf_prevWind,cf_print,cf_printf,cf_prompt,cf_push,cf_queryReplace,cf_quickExit,cf_quote,cf_quoteChar,cf_rand,cf_readFile,
	cf_readPipe,cf_reframeWind,cf_renameBuf,cf_replace,cf_replaceText,cf_resetTerm,cf_resizeWind,cf_restoreBuf,
	cf_restoreScreen,cf_restoreWind,cf_run,cf_saveBuf,cf_saveFile,cf_saveScreen,cf_saveWind,cf_scratchBuf,cf_searchBack,
	cf_searchForw,cf_selectBuf,cf_selectScreen,cf_selectWind,cf_setBufFile,cf_setHook,cf_setMark,cf_setWrapCol,cf_seti,
	cf_shQuote,cf_shell,cf_shellCmd,cf_shift,cf_showAliases,cf_showBuffers,cf_showCommands,cf_showFunctions,cf_showHooks,
	cf_showKey,cf_showKillRing,cf_showMacros,cf_showMarks,cf_showModeQ,cf_showModes,cf_showPoint,
#if MMDebug & Debug_ShowRE
	cf_showRegexp,
#endif
	cf_showReplaceRing,cf_showScreens,cf_showSearchRing,cf_showVariables,cf_shrinkWind,cf_space,cf_split,cf_splitWind,
	cf_sprintf,cf_statQ,cf_strFit,cf_strPop,cf_strPush,cf_strShift,cf_strUnshift,cf_strip,cf_sub,cf_subline,cf_substr,
	cf_success,cf_suspend,cf_swapMark,cf_tab,cf_titleCaseLine,cf_titleCaseRegion,cf_titleCaseString,cf_titleCaseWord,
	cf_toInt,cf_toString,cf_tr,cf_traverseLine,cf_trimLine,cf_truncBuf,cf_typeQ,cf_unbindKey,cf_undelete,cf_universalArg,
	cf_unshift,cf_updateScreen,cf_upperCaseLine,cf_upperCaseRegion,cf_upperCaseString,cf_upperCaseWord,cf_viewFile,
	cf_widenBuf,cf_wordCharQ,cf_wrapLine,cf_wrapWord,cf_writeFile,cf_xPathname,cf_xeqBuf,cf_xeqFile,cf_xeqKeyMacro,cf_yank,
	cf_yankCycle
	};

// Object for core keys bound to special commands (like "abort").  These are maintained in global variable "corekeys" in
// addition to the binding table for fast access.  Note: indices must be in descending, alphabetical order (by command name) so
// that corekeys is initialized properly in edinit1().
typedef struct {
	ushort ek;			// Extended key.
	enum cfid id;			// Command id (index into cftab).
	} CoreKey;

#define CK_UnivArg	0		// Universal argument (repeat) key.
#define CK_Quote	1		// Quote key.
#define CK_NegArg	2		// Negative argument (repeat) key.
#define CK_Abort	3		// Abort key.
#define NCoreKeys	4		// Number of cache keys.

// Structure for "i" variable.
typedef struct {
	int i;				// Current value.
	int inc;			// Increment to add to i.
	Datum format;			// sprintf() format string.
	} IVar;

// Miscellaneous definitions.
#define LineExt		'$'		// Character displayed which indicates a line is extended.

// Terminal input control parameters which are used when not the defaults.
typedef struct {
	char *defval;			// Default value.
	ushort delim;			// Input delimiter key.
	uint maxlen;			// Maximum input length (<= NTermInp).
	Ring *ringp;			// Data ring to use during terminal input (or NULL for none).
	} TermInp;

// Command argument flag(s).  These are combined with CFXXX validation flags for certain function calls so higher bits are used.
#define Arg_First	0x00080000	// First argument (so no preceding comma).

// Terminal completion/prompt flags.  These are combined with PtrXXX selector flags for certain function calls so higher bits
// are used.
#define Term_OneChar	0x00000100	// Get one key only with delimiter, C-SPC, and C-g checking.
#define Term_OneKey	0x00000200	// Get one key only, raw.
#define Term_OneKeySeq	0x00000300	// Get one key sequence only, raw.
#define Term_NoKeyEcho	0x00000400	// Don't echo keystrokes.
#define Term_LongPrmt	0x00000800	// Allow extra space (90% terminal width) for long prompt string.
#define Term_Attr	0x00001000	// Enable terminal attribute sequences in prompt string.
#define Term_NoDef	0x00002000	// Don't load default value into input buffer.
#define Term_Eval	0x00004000	// Evaluate string read from terminal.
#define Term_C_NoAuto	0x00008000	// Don't auto-complete; wait for return key.
#define Term_C_CFAM	0x00010000	// Command, function, alias, or macro name completion.
#define Term_C_Buffer	0x00020000	// Buffer name completion.
#define Term_C_Filename	0x00040000	// Filename completion (via directory search).
#define Term_C_BMode	0x00080000	// Buffer mode name completion.
#define Term_C_GMode	0x00100000	// Global mode name completion.
#define Term_C_Var	0x00200000	// Variable name completion -- all.
#define Term_C_SVar	0x00400000	// Variable name completion -- excluding constants.

#define Term_KeyMask	0x00000300	// Mask for one-key type.
#define Term_C_Mask	(Term_C_CFAM | Term_C_Buffer | Term_C_Filename | Term_C_BMode | Term_C_GMode | Term_C_Var | Term_C_SVar)
					// Completion-type mask.

// Control parameters for virtual terminal.
typedef struct {
	int hjump;			// Horizontal jump size - percentage.
	int hjumpcols;			// Horizontal jump size - columns.
	int vjump;			// Vertical jump size (zero for smooth scrolling).
	char *termnam;			// Value of TERM environmental variable.
	} VTermCtrl;

#include "lang.h"

// External functions not declared elsewhere.
extern int abortOp(Datum *rp,int n,Datum **argpp);
extern int abortinp(void);
extern int aboutMM(Datum *rp,int n,Datum **argpp);
extern int afind(char *aname,ushort op,CFABPtr *cfabp,Alias **app);
extern int aliasCFM(Datum *rp,int n,Datum **argpp);
extern int allowedit(bool edit);
extern int alterBufAttr(Datum *rp,int n,Datum **argpp);
extern int alterMode(Datum *rp,int n,int type,Buffer *bufp);
extern int amfind(char *name,ushort op,uint type);
extern int apropos(Datum *rp,int n,Datum **argpp);
extern short attrCount(char *str,int len,int maxcol);
extern int backChar(Datum *rp,int n,Datum **argpp);
extern int backLine(Datum *rp,int n,Datum **argpp);
extern int backPage(Datum *rp,int n,Datum **argpp);
extern int backWord(Datum *rp,int n,Datum **argpp);
extern int backch(int n);
extern int backln(int n);
extern int backwd(int n);
extern int bactivate(Buffer *bufp);
extern int bappend(Buffer *bufp,char *text);
extern void bchange(Buffer *bufp,ushort flags);
extern int bclear(Buffer *bufp,ushort flags);
extern int bcomplete(Datum *rp,char *prmt,char *defname,uint flags,Buffer **bufpp,bool *createdp);
extern Buffer *bdefault(ushort flags);
extern int bdelete(Buffer *bufp,ushort flags);
extern int beginKeyMacro(Datum *rp,int n,Datum **argpp);
extern int beginText(Datum *rp,int n,Datum **argpp);
extern int begintxt(void);
extern int beline(Datum *rp,int n,bool end);
extern int bextend(Buffer *bufp);
extern int bfind(char *name,ushort cflags,ushort bflags,Buffer **bufpp,bool *createdp);
extern int bftab(int n);
extern void bftowf(Buffer *bufp,EWindow *winp);
extern int bgets(Datum *rp,int n,Datum **argpp);
extern int bhook(Datum *rp,bool exitHook);
extern int binary(char *key,char *(*tval)(int i),int tsize);
extern int bpop(Buffer *bufp,ushort flags);
extern int bprint(Datum *rp,int n,Datum **argpp);
extern int brename(Datum *rp,int n,Buffer *targbufp);
extern int bscratch(char *dest,Buffer **bufpp);
extern Buffer *bsrch(char *bname,Buffer **bufpp);
extern int bswitch(Buffer *bufp);
extern long buflength(Buffer *bufp,long *lp);
extern int bufop(Datum *rp,int n,char *prmt,uint op,int flag);
extern int cfabsearch(char *str,CFABPtr *cfabp,uint selector);
extern int changedir(char *dir);
extern int chcase(short ch);
extern int chgtext(Datum *rp,int n,ushort style,Buffer *bufp);
extern int chwkdir(Datum *rp,int n,Datum **argpp);
extern int clearBuf(Datum *rp,int n,Datum **argpp);
extern void clearKeyMacro(bool stop);
extern void clfname(Buffer *bufp);
extern int copyreg(Region *regp);
#if WordCount
extern int countWords(Datum *rp,int n,Datum **argpp);
#endif
extern void cpause(int n);
extern int cvtcase(int n,ushort flags);
extern int deleteAlias(Datum *rp,int n,Datum **argpp);
extern int deleteBlankLines(Datum *rp,int n,Datum **argpp);
extern int deleteBuf(Datum *rp,int n,Datum **argpp);
extern int deleteMacro(Datum *rp,int n,Datum **argpp);
extern int deleteMark(Datum *rp,int n,Datum **argpp);
extern int deleteScreen(Datum *rp,int n,Datum **argpp);
extern int deleteWind(Datum *rp,int n,Datum **argpp);
extern int deltab(int n,bool force);
extern int delwhite(int n,bool prior);
extern int detabLine(Datum *rp,int n,Datum **argpp);
extern int dkregion(int n,bool kill);
extern int doincl(Datum *rp,int n,Datum **argpp);
extern int domode(char *kw,int action,int type,uint *flagp,long *formerStatep,uint *resultp);
extern int dopop(Datum *rp,int n,bool popBuf);
extern bool dotinwind(void);
#if MMDebug & Debug_ScrDump
extern void dumpscreens(char *msg);
#endif
extern char *dupchr(char *s,short c,int len);
extern int dupLine(Datum *rp,int n,Datum **argpp);
extern short ektoc(ushort ek,bool extend);
extern int endKeyMacro(Datum *rp,int n,Datum **argpp);
extern int endWord(Datum *rp,int n,Datum **argpp);
extern int entabLine(Datum *rp,int n,Datum **argpp);
extern int eopendir(char *fspec,char **fp);
extern int ereaddir(void);
extern int escattr(DStrFab *sfp);
extern int escattrtosf(DStrFab *sfp,char *str);
extern void faceinit(WindFace *wfp,struct Line *lnp,Buffer *bufp);
extern CmdFunc *ffind(char *cname);
extern int filterBuf(Datum *rp,int n,Datum **argpp);
extern EWindow *findwind(Buffer *bufp);
extern int fmatch(short ch);
extern int forwChar(Datum *rp,int n,Datum **argpp);
extern int forwLine(Datum *rp,int n,Datum **argpp);
extern int forwPage(Datum *rp,int n,Datum **argpp);
extern int forwWord(Datum *rp,int n,Datum **argpp);
extern int forwch(int n);
extern int forwln(int n);
extern void garbpop(Datum *datp);
extern int getccol(Dot *dotp);
extern int getcfam(char *prmt,uint selector,CFABPtr *cfabp,char *emsg0,char *emsg1);
extern int getcfm(char *prmt,CFABPtr *cfabp,uint selector);
extern long getlinenum(Buffer *bufp,Line *targlnp);
extern int getmodes(Datum *rp,uint flags);
extern int getnarg(Datum *rp,char *prmt);
extern int getregion(Region *regp,bool forceBegin,bool *wholebufp);
extern int gettermsize(ushort *colp,ushort *rowp);
extern int getwid(ushort *widp);
extern int getwnum(EWindow *winp);
extern int getwpos(EWindow *winp);
extern int goline(Datum *datp,int n,int line);
extern int gotoLine(Datum *rp,int n,Datum **argpp);
extern int gotoMark(Datum *rp,int n,Datum **argpp);
extern int gotoScreen(int n,ushort flags);
extern int gotoWind(Datum *rp,int n,ushort flags);
extern int growKeyMacro(void);
extern int gswind(Datum *rp,int n,int how);
extern int help(Datum *rp,int n,Datum **argpp);
extern int indentRegion(Datum *rp,int n,Datum **argpp);
extern void initchars(void);
extern int insertBuf(Datum *rp,int n,Datum **argpp);
extern int insertLineI(Datum *rp,int n,Datum **argpp);
extern int inserti(Datum *rp,int n,Datum **argpp);
extern int insnlspace(int n,ushort flags);
extern int inspre(short c);
extern int insrfence(short c);
extern int instab(int n);
extern bool inwind(EWindow *winp,Line *lnp);
extern bool inword(void);
extern int iorstr(Datum *srcp,int n,ushort style,bool kill);
extern bool is_lower(short ch);
extern bool is_upper(short ch);
extern bool is_white(Line *lnp,int length);
extern bool isletter(short ch);
extern int joinLines(Datum *rp,int n,Datum **argpp);
extern int joinWind(Datum *rp,int n,Datum **argpp);
extern int kdcbword(int n,int kdc);
extern bool kdcFencedRegion(int kdc);
extern int kdcfword(int n,int kdc);
extern int kdcline(int n,int kdc);
extern int kdctext(int n,int kdc,Region *regp);
extern int kprep(int kill);
extern int lalloc(int used,Line **lnpp);
extern int ldelete(long n,ushort flags);
extern void lfree(Line *lnp);
extern int librcset(int status);
extern int linsert(int n,short c);
extern int linstr(char *s);
extern int lnewline(void);
extern int markBuf(Datum *rp,int n,Datum **argpp);
extern void mdelete(Buffer *bufp,ushort flags);
extern int mfind(ushort id,Mark **mkpp,ushort flags);
extern char *mklower(char *dest,char *src);
extern char *mkupper(char *dest,char *src);
extern int mlerase(ushort flags);
extern int mlmove(int col);
extern int mlprintf(ushort flags,char *fmt,...);
extern int mlputc(ushort flags,short c);
extern int mlputs(ushort flags,char *str);
extern int mlrestore(void);
extern int modecmp(char *kw,int type,ModeSpec **mspp,bool errorIfNot);
extern int moveWindUp(Datum *rp,int n,Datum **argpp);
extern int movecursor(int row,int col);
extern void mset(Mark *mkp,EWindow *winp);
extern int narrowBuf(Datum *rp,int n,Datum **argpp);
extern int newcol(short c,int col);
extern int newlineI(Datum *rp,int n,Datum **argpp);
extern int nextWind(Datum *rp,int n,Datum **argpp);
extern int notice(Datum *rp,int n,bool success);
extern void nukebufsp(EScreen *scrp);
extern int onlyWind(Datum *rp,int n,Datum **argpp);
extern int openLine(Datum *rp,int n,Datum **argpp);
extern int otherfence(Region *regp);
extern int outdentRegion(Datum *rp,int n,Datum **argpp);
extern int overprep(int n);
extern char *pad(char *s,uint len);
extern int pathsearch(char **destp,char *name,bool homeOnly);
extern int pnbuffer(Datum *rp,int n,ushort flags);
extern int prevWind(Datum *rp,int n,Datum **argpp);
extern int quit(Datum *rp,int n,Datum **argpp);
extern int quoteChar(Datum *rp,int n,Datum **argpp);
extern int rcclear(ushort flags);
extern int rcsave(void);
extern int rcset(int status,ushort flags,char *fmt,...);
extern int rcycle(Ring *ringp,int n,bool msg);
extern int rdelete(Ring *ringp,int n);
extern char *regcpy(char *buf,Region *regp);
extern int reglines(int *np,bool *allp);
extern int renameBuf(Datum *rp,int n,Datum **argpp);
extern int render(Datum *rp,int n,Buffer *bufp,ushort flags);
extern int resetTermc(Datum *rp,int n,Datum **argpp);
extern int resizeWind(Datum *rp,int n,Datum **argpp);
extern RingEntry *rget(Ring *ringp,int n);
extern int rsetpat(char *str,Ring *ringp);
extern int runcmd(Datum *dsinkp,DStrFab *cmdp,char *cname,char *arg,bool fullQuote);
extern int scratchBuf(Datum *rp,int n,Datum **argpp);
extern int scrcount(void);
extern int selectBuf(Datum *rp,int n,Datum **argpp);
extern int selectScreen(Datum *rp,int n,Datum **argpp);
extern int selectWind(Datum *rp,int n,Datum **argpp);
extern int setMark(Datum *rp,int n,Datum **argpp);
extern int setccol(int pos);
extern int setfname(Buffer *bufp,char *fname);
extern int seti(Datum *rp,int n,Datum **argpp);
extern int setpath(char *path,bool prepend);
extern void settermsize(ushort ncol,ushort nrow);
extern int setwkdir(EScreen *scrp);
extern int sfind(ushort scr_num,Buffer *bufp,EScreen **spp);
extern int shellCLI(Datum *rp,int n,Datum **argpp);
extern int shellCmd(Datum *rp,int n,Datum **argpp);
extern int showBuffers(Datum *rp,int n,Datum **argpp);
extern int showKillRing(Datum *rp,int n,Datum **argpp);
extern int showMarks(Datum *rp,int n,Datum **argpp);
extern int showModes(Datum *rp,int n,Datum **argpp);
extern int showPoint(Datum *rp,int n,Datum **argpp);
extern int showRing(Datum *rp,int n,Ring *ringp);
extern int showScreens(Datum *rp,int n,Datum **argpp);
extern int spanwhite(bool end);
extern int sswitch(EScreen *scrp);
extern char *strrev(char *s);
extern char *strsamp(char *src,size_t srclen,size_t maxlen);
extern void supd_wflags(Buffer *bufp,ushort flags);
extern int suspendMM(Datum *rp,int n,Datum **argpp);
extern int swapMark(Datum *rp,int n,Datum **argpp);
extern int swapmid(ushort id);
extern int sysbuf(char *root,Buffer **bufpp,ushort flags);
extern int tabstop(int n);
extern int terminp(Datum *rp,char *prmt,uint aflags,uint cflags,TermInp *tip);
extern int terminpYN(char *prmt,bool *resultp);
extern char *timeset(void);
extern int traverseLine(Datum *rp,int n,Datum **argpp);
extern int trimLine(Datum *rp,int n,Datum **argpp);
extern void tungetc(ushort ek);
extern int typahead(int *countp);
extern int update(int n);
extern int uprompt(Datum *rp,int n,Datum **argpp);
extern int vtinit(void);
extern void vupd_modeline(EWindow *winp,Buffer *popbuf);
extern void wftobf(EWindow *winp,Buffer *bufp);
extern EWindow *whasbuf(Buffer *bufp,bool skipCur);
extern int widenBuf(Datum *rp,int n,Datum **argpp);
extern int wincount(EScreen *scrp,int *wnump);
extern bool wnewtop(EWindow *winp,Line *lnp,int n);
extern EWindow *wnextis(EWindow *winp);
extern int wrapLine(Datum *rp,int n,Datum **argpp);
extern int wrapWord(Datum *rp,int n,Datum **argpp);
extern int wscroll(Datum *rp,int n,int (*winfunc)(Datum *rp,int n,Datum **argpp),int (*pagefunc)(Datum *rp,int n,Datum **argpp));
extern int wsplit(int n,EWindow **winpp);
extern void wswitch(EWindow *winp);
extern int xeqKeyMacro(Datum *rp,int n,Datum **argpp);
extern long xorshift64star(long max);
extern int yankCycle(Datum *rp,int n,Datum **argpp);

#ifdef DATAmain

// **** For main.c ****

// Forward.
CmdFunc cftab[];

// Global variables.
BufFlagSpec bflaginfo[] = {		// Buffer flag table.
	{"active",NULL,BFActive},
	{"changed","chg",BFChanged},
	{"hidden","hid",BFHidden},
	{"macro",NULL,BFMacro},
	{"narrowed",NULL,BFNarrowed},
	{"termattr","tattr",BFTermAttr},
	{NULL,NULL,0}};
Buffer *bheadp = NULL;			// Head of buffer list.
Buffer *btailp = NULL;			// Tail of buffer list.
char buffer1[] = Buffer1;		// Name of first buffer ("untitled").
char *Copyright = "(c) Copyright 2018 Richard W. Marinelli";
Buffer *curbp;				// Current buffer.
EScreen *cursp;				// Current screen.
EWindow *curwp;				// Current window.
int fencepause = FPause;		// Centiseconds to pause for fence matching.
int gacount = NASave;			// Global ASave counter (for all buffers).
int gasave = NASave;			// Global ASave size.
HookRec hooktab[] = {			// Hook table.
	{"chDir",HLitN_chDir,HLitArg_none,NULL},
	{"createBuf",HLitN_defn,HLitArg_createBuf,NULL},
	{"enterBuf",HLitN_defn,HLitArg_enterBuf,NULL},
	{"exitBuf",HLitN_defn,HLitArg_none,NULL},
	{"help",HLitN_help,HLitArg_none,NULL},
	{"mode",HLitN_defn,HLitArg_mode,NULL},
	{"postKey",HLitN_postKey,HLitArg_postKey,NULL},
	{"preKey",HLitN_preKey,HLitArg_preKey,NULL},
	{"read",HLitN_defn,HLitArg_read,NULL},
	{"wrap",HLitN_defn,HLitArg_none,NULL},
	{"write",HLitN_defn,HLitArg_write,NULL},
	{NULL,NULL,NULL,NULL}
	};
int htabsize = 8;			// Current hard tab size.
IVar ivar = {1,1};			// "i" variable.
KMacro kmacro = {
	0,NULL,NULL,KMStop,0,NULL};	// Keyboard macro control variables.
Ring kring = {				// Kill ring (initially empty).
	NULL,0,NKillRing,"Kill",""};
#if MMDebug
FILE *logfile;				// Log file for debugging.
#endif
char lowcase[HiChar + 1];		// Upper-to-lower translation table.
int maxprmt = MaxPromptPct;		// Maximum percentage of terminal width for prompt string (in range 15-90).
int mlcol = INT_MAX;			// Last cursor column on message line (which may be equal to terminal width).
ModeSpec modeinfo[] = {			// Mode table.
	{"asave","ASave",MdGlobal | MdASave,MLit_ModeAutoSave},
	{"bak","Bak",MdGlobal | MdBak,MLit_ModeBackup},
	{"c","C",MdC,MLit_ModeC},
	{"clob","Clob",MdGlobal | MdClob,MLit_ModeClobber},
	{"col","Col",MdCol,MLit_ModeColDisp},
	{"exact","Exact",MdGlobal | MdExact,MLit_ModeExact},
	{"hscrl","HScrl",MdGlobal | MdHScrl,MLit_ModeHorzScroll},
	{"line","Line",MdLine,MLit_ModeLineDisp},
	{"memacs","Memacs",MdMemacs,MLit_ModeMEMacs},
	{"msg","Msg",MdGlobal | MdMsg,MLit_ModeMsgDisp},
	{"over","Over",MdOver,MLit_ModeOver},
	{"perl","Perl",MdPerl,MLit_ModePerl},
	{"rdonly","RdOnly",MdRdOnly,MLit_ModeReadOnly},
	{"regexp","Regexp",MdGlobal | MdRegexp,MLit_ModeRegexp},
	{"repl","Repl",MdRepl,MLit_ModeReplace},
	{"ruby","Ruby",MdRuby,MLit_ModeRuby},
	{"safe","Safe",MdGlobal | MdSafe,MLit_ModeSafeSave},
	{"shell","Shell",MdShell,MLit_ModeShell},
	{"wkdir","WkDir",MdGlobal | MdWkDir,MLit_ModeWorkDir},
	{"wrap","Wrap",MdWrap,MLit_ModeWrap},
	{"xindt","XIndt",MdXIndt,MLit_ModeExtraIndent},
	{NULL,NULL,0,NULL}};
ModeRec modetab[] = {			// Global and show mode settings.
	{MdExact | MdHScrl | MdMsg,"Global"},
	{MdASave | MdBak | MdC | MdExact | MdHScrl | MdMemacs | MdOver | MdPerl | MdRdOnly | MdRegexp | MdRepl | MdRuby |
	 MdSafe | MdShell | MdWrap | MdXIndt,"Show"},
	{0,NULL}};
uint mypid = 0U;			// Unix PID (for temporary filenames).
char Myself[] = ProgName;		// Common name of program.
ushort opflags = OpEval | OpStartup | OpScrRedraw;	// Operation flags (bit masks).
int overlap = 2;			// Overlap when paging on a screen.
ulong randseed = 1;			// Random number seed.
RtnCode rc = {Success,0,""};		// Return code record.
SampBuf sampbuf;			// "Sample" string buffer.
Buffer *savbufp = NULL;			// Saved buffer pointer.
EScreen *savscrp = NULL;		// Saved screen pointer.
EWindow *savwinp = NULL;		// Saved window pointer.
EScreen *sheadp = NULL;			// Head of screen list.
int stabsize = 0;			// Current soft tab size (0: use hard tabs).
int tjump = TravJump;			// Line-traversal jump size.
char upcase[HiChar + 1];		// Lower-to-upper translation table.
RingEntry undelbuf = {NULL,NULL};	// Undelete buffer.
char Version[] = ProgVer;		// MightEMacs version.
char viz_false[] = "false";		// Visual form of false.
char viz_nil[] = "nil";			// Visual form of nil.
char viz_true[] = "true";		// Visual form of true.
VTermCtrl vtc = {0,1,0,NULL};		// Control parameters for virtual terminal.
EWindow *wheadp = NULL;			// Head of window list.
int wrapcol = 74;			// Current wrap column.
int wrapcol0 = -1;			// Previous wrap column.

#else

// **** For all the other .c files ****

// External declarations for global variables defined above, with a few exceptions noted.
extern BufFlagSpec bflaginfo[];
extern Buffer *bheadp;
extern Buffer *btailp;
extern char buffer1[];
extern Buffer *curbp;
extern EScreen *cursp;
extern EWindow *curwp;
extern int fencepause;
extern int gacount;
extern int gasave;
extern HookRec hooktab[];
extern int htabsize;
extern IVar ivar;
extern KMacro kmacro;
extern Ring kring;
#if MMDebug
extern FILE *logfile;
#endif
extern char lowcase[];
extern int maxprmt;
extern int mlcol;
extern ModeSpec modeinfo[];
extern ModeRec modetab[];
extern uint mypid;
extern char Myself[];
extern ushort opflags;
extern int overlap;
extern ulong randseed;
extern RtnCode rc;
extern SampBuf sampbuf;
extern Buffer *savbufp;
extern EScreen *savscrp;
extern EWindow *savwinp;
extern ulong seedinit(void);
extern EScreen *sheadp;
extern int stabsize;
extern int tjump;
extern RingEntry undelbuf;
extern char upcase[];
extern char Version[];
extern char viz_false[];
extern char viz_nil[];
extern char viz_true[];
extern VTermCtrl vtc;
extern EWindow *wheadp;
extern int wrapcol;
extern int wrapcol0;
#endif

// External declarations for global variables defined in C source files other than main.c.
extern ETerm term;		// Terminal table, defined in unix.c.
