// (c) Copyright 2017 Richard W. Marinelli
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
#define Debug_CFAB	0x0004		// Show CFAB pointer type in "showBindings" display.
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
//#define MMDebug		(Debug_MacArg | Debug_Logfile)
//#define MMDebug		(Debug_PPBuf | Debug_Logfile)
//#define MMDebug		(Debug_Bind | Debug_Logfile)
//#define MMDebug		Debug_ShowRE
//#define MMDebug		(Debug_Logfile | Debug_Temp)

#if MMDebug & Debug_MacArg
#define dobuf		dobufDebug
#endif

// Program identification.  Scripts can query these via the "sysInfo" function.  REDHAT must be before CENTOS because both are
// defined for Red Hat Linux.
#define ProgName	"MightEMacs"
#define ProgVer		"8.5.1"
#if DEBIAN
#	define OSName	"Debian Linux"
#elif HPUX8 || HPUX
#	define OSName	"HP/UX"
#elif MACOS
#	define OSName	"macOS"
#elif REDHAT
#	define OSName	"Red Hat Linux"
#elif CENTOS
#	define OSName	"CentOS Linux"
#elif SOLARIS
#	define OSName	"Solaris"
#else
#	define OSName	"Unix"
#endif

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
#define TypeAhead	1		// Include $KeyPending and type-ahead checking (which skips unnecessary screen updates).
#define WordCount	0		// Include code for "countWords" command (deprecated).
#define MLScaled	0		// Include "%f" (scaled integer) formatting option in mlprintf() (deprecated).
#define VizMacro	0		// Update display during keyboard macro execution.
#define RevStatus	1		// Status line appears in reverse video.
#define Color		0		// Include code for color commands and windows (not working).
#define VizBell		0		// Use a visible Bell (instead of an audible one) if it exists.
#define KMDelims	":;,\"'"	// Keyboard macro encoding delimiters ($keyMacro), in order of preference.
#define DefWordLst	"A-Za-z0-9_"	// Default $wordChars value.
#define Backup_Ext	".bak"		// Backup file extension.
#define Script_Ext	".mm"		// Script file extension.
#define UserStartup	".memacs"	// User start-up file (in HOME directory).
#define SiteStartup	"memacs.mm"	// Site start-up file.
#define MMPath_Name	"MMPATH"	// Shell environmental variable name.
#if DEBIAN
#define MMPath_Default	":/usr/lib/memacs"	// Default search directories.
#else
#define MMPath_Default	":/usr/local/lib/memacs"	// Default search directories.
#endif
#define Logfile		"memacs.log"	// Log file (for debugging).

// Limits -- [Set any].
#define MaxTab		240		// Maximum hard or soft tab size.
#define NBufName	24		// Number of bytes, buffer name.
#define NTermInp	(MaxPathname < 1024 ? 1024 : MaxPathname)
					// Number of bytes, terminal input (must be >= MaxPathname).
#define NBlock		32		// Number of bytes, line block chunks.
#define KBlock		256		// Number of bytes, kill buffer chunks.
#define NRing		30		// Number of buffers in kill ring.
#define NVarName	32		// Maximum number of characters in a user variable name (including prefix).
#define NASave		220		// Number of keystrokes before auto-save -- initial value.
#define MaxLoop		2500		// Default maximum number of script loop iterations allowed.
#define MaxMacroDepth	100		// Default maximum macro recursion depth allowed during script execution.
#define MaxArrayDepth	30		// Default maximum array recursion depth allowed when cloning, etc.
#define FPause		26		// Default time in centiseconds to pause for fence matching.
#define VJumpMin	10		// Minimum vertical jump size (percentage).
#define JumpMax		49		// Maximum horizontal or vertical jump size (percentage).
#if Color
#define NColorS		16		// Number of supported colors.
#define NPalette	48		// Size of palette string (palstr).
#endif

/***** END CUSTOMIZATIONS *****/

// Internal constants.
#define NPrefix		6		// Number of prefix keys (Meta, ^C, ^H, ^X, FKey, and Shft-FKey).
#define NDelim		2		// Number of bytes, input and output record delimiters.
#define NPatMin		32		// Minimum array size for search or replacement pattern.
#define NPatMax		96		// Maximum array size to retain for search or replacement pattern.
#define NKbdChunk	48		// Extension size of keyboard macro buffer when full.
#define NWork		80		// Number of bytes, work buffer.
#define RMark		' '		// Mark which defines region endpoint.
#define WMark		'.'		// Work mark, used as default for certain commands.

// Operation flags used at runtime (in "opflags" global variable).
#define OpVTOpen	0x0001		// Virtual terminal open?
#define OpEval		0x0002		// Evaluate expressions?
#define OpHaveEOL	0x0004		// Does clear to EOL exist?
#define OpHaveRev	0x0008		// Does reverse video exist?
#define OpStartup	0x0010		// In pre-edit-loop state?
#define OpScript	0x0020		// Script execution flag.
#define OpParens	0x0040		// Command, alias, macro, or system function invoked in xxx() form.
#define OpNoLoad	0x0080		// Do not load function arguments (non-command-line hook is running).
#define OpScrRedraw	0x0100		// Clear and redraw screen if true.

// Buffer operation flags used by bufop().
#define BOpSetFlag	1		// Set buffer flag.
#define BOpClrFlag	2		// Clear buffer flag.
#define BOpBeginEnd	3		// Move to beginning or end of buffer.
#define BOpGotoLn	4		// Goto to a line in the buffer.
#define BOpReadBuf	5		// Read line(s) from a buffer.

// Flags used by catargs(), dtosf(), and atosf() for controlling conversions to string.
#define CvtExpr		0x0001		// Output as an expression; otherwise, as data.
#define CvtShowNil	0x0002		// Use "nil" for nil; otherwise, null.
#define CvtForceArray	0x0004		// If array contains itself, display with ellipsis; otherwise, return error.
#define CvtVizStr	0x0008		// Output strings in visible form; otherwise, unmodified.
#define CvtVizStrQ	0x0010		// Output strings in visible form enclosed in single (') quotes.

#define CvtKeepNil	0x0020		// Keep nil arguments.
#define CvtKeepNull	0x0040		// Keep null arguments.
#define CvtKeepAll	(CvtKeepNil | CvtKeepNull)

// Information display characters.
#define MacFormat	"@%.*s"		// sprintf() format string for prepending prefix to macro name.
#define AltBufCh	'*'		// Substitution character for non-macro buffer names that begin with SBMacro.
#define SBActive	':'		// BFActive flag indicator (activated buffer -- file read in).
#define SBChgd		'*'		// BFChgd flag indicator (changed buffer).
#define SBHidden	'?'		// BFHidden flag indicator (hidden buffer).
#define SBMacro		'@'		// BFMacro flag indicator (macro buffer).
#define SBPreproc	'+'		// BFPreproc flag indicator (preprocessed buffer).
#define SBNarrow	'<'		// BFNarrow flag indicator (narrowed buffer).

// Key prefixes.
#define Ctrl		0x0100		// Control flag.
#define Meta		0x0200		// Meta flag.
#define Pref1		0x0400		// Prefix1 (^X) flag.
#define Pref2		0x0800		// Prefix2 (^C) flag.
#define Pref3		0x1000		// Prefix3 (^H) flag.
#define Shft		0x2000		// Shift flag (for function keys).
#define FKey		0x4000		// Special key flag (function key).
#define Prefix		(Meta | Pref1 | Pref2 | Pref3)	// Prefix key mask.
#define KeySeq		(Meta | Pref1 | Pref2 | Pref3 | FKey)	// Key sequence mask.

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
#define RCNoFormat	0x0001		// Don't call vasprintf(), use format string verbatim.
#define RCNoWrap	0x0002		// Don't wrap Success message.
#define RCForce		0x0004		// Force save of new message of equal severity.
#define RCKeepMsg	0x0008		// Don't replace any existing message (just change severity).

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
	int km_state;			// Current state.
	int km_n;			// Number of repetitions (0 = infinite).
	ushort *km_buf;			// Macro buffer (allocated from heap).
	} KMacro;

// Keyboard macro states.
#define KMStop		0		// Not in use.
#define KMPlay		1		// Playing.
#define KMRecord	2		// Recording.

// Text insertion style.
#define Txt_Insert	0x0001
#define Txt_OverWrt	0x0002
#define Txt_Replace	0x0003
#define Txt_LitRtn	0x0010

// The editor communicates with the display using a high level interface.  A ETerm structure holds useful variables and
// indirect pointers to routines that do useful operations.  The low level get and put routines are here, too.  This lets a
// terminal, in addition to having non-standard commands, have funny get and put character code, too.  The calls might get
// changed to "termp->t_field" style in the future, to make it possible to run more than one terminal type.
typedef struct {
	ushort t_mcol;			// Maximum number of columns allowed (allocated).
	ushort t_ncol;			// Current number of columns.
	ushort t_mrow;			// Maximum number of rows allowed (allocated).
	ushort t_nrow;			// Current number of rows used.
	ushort t_margin;		// Min margin for extended lines.
	ushort t_scrsiz;		// Size of scroll region.
	int (*t_open)(void);		// Open terminal at the start.
	int (*t_close)(void);		// Close terminal at end.
	int (*t_kopen)(void);		// Open keyboard.
	int (*t_kclose)(void);		// Close keyboard.
	int (*t_getchar)(ushort *);	// Get character from keyboard.
	int (*t_putchar)(int);		// Put character to display.
	int (*t_flush)(void);		// Flush output buffers.
	int (*t_move)(int,int);		// Move the cursor, origin 0.
	int (*t_eeol)(void);		// Erase to end of line.
	int (*t_eeop)(void);		// Erase to end of page.
	int (*t_clrdesk)(void);		// Clear the page totally.
	int (*t_beep)(void);		// Beep.
	int (*t_rev)(int);		// Set reverse video state.
#if Color
	int (*t_setfor)(int);		// Set foreground color.
	int (*t_setback)(int);		// Set background color.
#endif
	} ETerm;

// C macros for terminal I/O.
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
#define TTclrdesk	(*term.t_clrdesk)
#define TTbeep		(*term.t_beep)
#define TTrev		(*term.t_rev)
#if Color
#define TTforg		(*term.t_setfor)
#define TTbacg		(*term.t_setback)
#endif

// Operation types.
#define OpDelete	-1		// Delete an item.
#define OpQuery		0		// Look for an item.
#define OpCreate	1		// Create an item.

// The editor holds deleted and copied text chunks in an array of Kill buffers (the kill ring).  Each buffer is logically a
// stream of ASCII characters; however, due to its unpredicatable size, it is implemented as a linked list of KillBuf buffers.  
typedef struct KillBuf {
	struct KillBuf *kl_next;	// Pointer to next chunk; NULL if last.
	char kl_chunk[KBlock];		// Deleted or copied text.
	} KillBuf;

typedef struct {
	KillBuf *kbufh;			// Kill buffer header pointer.
	KillBuf *kbufp;			// Current kill buffer chunk pointer.
	int kskip;			// Number of bytes to skip in first kill chunk.
	int kused;			// Number of bytes used in last kill chunk.
	} Kill;

// Position of dot in a buffer.
typedef struct {
	struct Line *lnp;		// Pointer to Line structure.
	int off;			// Offset of dot in line.
	} Dot;

// Message line print flags.
#define MLHome		0x0001		// Move cursor to beginning of message line before display.
#define MLForce		0x0002		// Force output (ignore 'msg' global mode).
#define MLWrap		0x0004		// Wrap message [like this].
#define MLRaw		0x0008		// Output raw character.
#define MLTrack		0x0010		// Keep track of character length (for backspacing).

// Message line information.  span is set to the maximum terminal width + NTermInp and is allocated at program launch.
typedef struct {
	ushort ttcol;			// Current virtual cursor column (which may be greater than maximum terminal width).
	char *span;			// Buffer holding display size of each character posted (used for backspacing).
	char *spanw;			// Current postion of cursor in span buffer.
	} MsgLine;

// Settings that determine a window's "face"; that is, the location of dot in the buffer and in the window.
typedef struct {
	struct Line *wf_toplnp;		// Pointer to top line of window.
	Dot wf_dot;			// Dot position.
	int wf_fcol;			// First column displayed.
	} WindFace;

// There is a window structure allocated for every active display window.  The windows are kept in a list, in top to bottom
// screen order.  The list is associated with one screen and the list head is stored in "wheadp" whenever the screen is current.
// Each window contains its own values of dot and mark.  The flag field contains some bits that are set by commands to guide
// redisplay.  Although this is a bit of a compromise in terms of decoupling, the full blown redisplay is just too expensive
// to run for every input character.
typedef struct EWindow {
	struct EWindow *w_nextp;	// Next window.
	struct Buffer *w_bufp;		// struct Buffer displayed in window.
	WindFace w_face;		// Dot position, etc.
	ushort w_id;			// Unique window identifier (mark used to save face before it's buffer is narrowed).
	ushort w_toprow;		// Origin 0 top row of window.
	ushort w_nrows;			// Number of rows in window, excluding mode line.
	short w_force;			// Target line in window for line containing dot.
	ushort w_flags;			// Flags.
#if Color
	ushort w_fcolor;		// Current forground color.
	ushort w_bcolor;		// Current background color.
#endif
	} EWindow;

#define WFForce		0x0001		// Window needs forced reframe.
#define WFMove		0x0002		// Movement from line to line.
#define WFEdit		0x0004		// Editing within a line.
#define WFHard		0x0008		// Full screen update needed.
#define WFMode		0x0010		// Update mode line.
#if Color
#define WFColor		0x0020		// Needs a color change.
#endif

// This structure holds the information about each line appearing on the video display.  The redisplay module uses an array
// of virtual display lines.  On systems that do not have direct access to display memory, there is also an array of physical
// display lines used to minimize video updating.  In most cases, these two arrays are unique.
typedef struct Video {
	ushort v_flags;			// Flags.
#if Color
	int v_fcolor;			// Current forground color.
	int v_bcolor;			// Current background color.
	int v_rfcolor;			// Requested forground color.
	int v_rbcolor;			// Requested background color.
#endif
	short v_left;			// Left edge of reverse video.
	short v_right;			// Right edge of reverse video.
	char v_text[1];			// Screen data.
	} Video;

#define VFNew		0x0001		// Contents not meaningful yet.
#define VFChgd		0x0002		// Changed flag.
#define VFExt		0x0004		// Extended (beyond terminal width).
#if Color
#define VFColor		0x0008		// Color change requested.
#endif

// This structure holds the information about each separate "screen" within the current editing session.  On a character based
// system (Text User Interface), which is used by MightEMacs, these screens overlay each other, and can individually be brought
// to the front.  On a windowing system like X-windows, OS X, MicroSoft Windows, etc., each screen is represented in an OS
// window.  The terminolgy is different in Emacs...
//
//	EMACS		The Outside World
//	------		-----------------
//	screen		window
//	window		pane
//
// Each EScreen structure contains its own private list of windows (EWindow structure).
typedef struct EScreen {
	struct EScreen *s_nextp;	// Pointer to next screen in list.
	EWindow *s_wheadp;		// Head of linked list of windows.
	EWindow *s_curwp;		// Current window in this screen.
	ushort s_num;			// Screen number (first is 1).
	ushort s_flags;			// Flags.
	ushort s_nrow;			// Height of screen.
	ushort s_ncol;			// Width of screen.
	} EScreen;

#define EScrResize	0x01		// Resize screen window(s) vertically when screen is frontmost.

// Dot mark in a buffer and flags.
typedef struct mark {
	struct mark *mk_nextp;		// Next mark.
	ushort mk_id;			// Mark identifier.
	short mk_force;			// Target line in window for dot.
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
	short mi_nargs;			// Declared argument count (-1 if variable).
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
	ushort b_nwind;			// Count of windows displaying buffer.
	ushort b_nalias;		// Count of aliases pointing to this (macro) buffer.
	ushort b_flags;			// Flags.
	uint b_modes;			// Buffer modes.
	ushort b_inpdelimlen;		// Length of input delimiter string.
	char b_inpdelim[NDelim + 1];	// Record delimiters used to read buffer.
	char *b_fname;			// File name.
	char b_bname[NBufName + 1];	// Buffer name.
	} Buffer;

// Buffer flags.
#define BFActive	0x0001		// Active buffer (file was read).
#define BFChgd		0x0002		// Changed since last write.
#define BFHidden	0x0004		// Hidden buffer.
#define BFMacro		0x0008		// Buffer is a macro.
#define BFNarrow	0x0010		// Buffer is narrowed.
#define BFPreproc	0x0020		// (Script) buffer has been preprocessed.
#define BFQSave		0x0040		// Buffer was saved via quickExit().

#define BSysLead	'.'		// Leading character of system (internal) buffer.

// Buffer creation flags.
#define CRBQuery	0x0000		// Look-up only (do not create).
#define CRBCreate	0x0001		// Create buffer if non-existent.
#define CRBExtend	0x0002		// Create buffer-extension record (MacInfo).
#define CRBForce	0x0004		// Force-create buffer with unique name.
#define CRBFile		0x0008		// Derive buffer name from filename.

// Buffer clearing flags.
#define CLBIgnChgd	0x0001		// Ignore BFChgd (buffer changed) flag.
#define CLBUnnarrow	0x0002		// Force-clear narrowed buffer (unnarrow first).
#define CLBClrFilename	0x0004		// Clear associated filename, if any.
#define CLBMulti	0x0008		// Processing multiple buffers.

// Buffer rendering flags.
#define RendReset	0x0001		// Move dot to beginning of buffer when displaying in a new window.
#define RendAltML	0x0002		// Use alternate mode line when doing a real pop-up.
#define RendBool	0x0004		// Return boolean argument in addition to buffer name.
#define RendTrue	0x0008		// Return true boolean argument; otherwise, false.

// Descriptor for global and buffer modes.
typedef struct {
	char *name;			// Name of mode in lower case.
	char *mlname;			// Name displayed on mode line.
	uint mask;			// Bit mask.
	char *desc;			// Description.
	} ModeSpec;

// Global mode bit masks.
#define MdASave		0x0001		// Auto-save mode.
#define MdBak		0x0002		// File backup mode.
#define MdClob		0x0004		// Macro-clobber mode.
#define MdEsc8		0x0008		// Escape 8-bit mode.
#define MdExact		0x0010		// Exact matching for searches.
#define MdHScrl		0x0020		// Horizontal-scroll-all mode.
#define MdMsg		0x0040		// Message line display mode.
#define MdNoUpd		0x0080		// Suppress screen update mode.
#define MdRegexp	0x0100		// Regular expresions in search.
#define MdSafe		0x0200		// Safe file save mode.
#define MdWkDir		0x0400		// Working directory display mode.

// Buffer mode bit masks -- language.
#define MdC		0x0001		// C indentation and fence match.
#define MdMemacs	0x0002		// MightEMacs indentation.
#define MdPerl		0x0004		// Perl indentation and fence match.
#define MdRuby		0x0008		// Ruby indentation and fence match.
#define MdShell		0x0010		// Shell indentation and fence match.

// Buffer mode bit masks -- non-language.
#define MdCol		0x0020		// Column position display mode.
#define MdLine		0x0040		// Line number display mode.
#define MdOver		0x0080		// Overwrite mode.
#define MdRdOnly	0x0100		// Read-only buffer.
#define MdRepl		0x0200		// Replace mode.
#define MdWrap		0x0400		// Word wrap.
#define MdXIndt		0x0800		// Extra fence indentation mode.

// Mode masks.
#define MdGlobal	0x0fff		// All possible global modes.
#define MdBuffer	0x0fff		// All possible buffer modes.
#define MdGrp_Over	(MdOver | MdRepl)
#define MdGrp_Lang	(MdC | MdMemacs | MdPerl | MdRuby | MdShell)

// Global and buffer mode table offsets (for variable table).
#define MdOff_ASave	0
#define MdOff_Bak	1
#define MdOff_Clob	2
#define MdOff_Esc8	3
#define MdOff_Exact	4
#define MdOff_HScrl	5
#define MdOff_Msg	6
#define MdOff_NoUpd	7
#define MdOff_Regexp	8
#define MdOff_Safe	9
#define MdOff_WkDir	10

#define MdOff_C		0
#define MdOff_Col	1
#define MdOff_Line	2
#define MdOff_Memacs	3
#define MdOff_Over	4
#define MdOff_Perl	5
#define MdOff_RdOnly	6
#define MdOff_Repl	7
#define MdOff_Ruby	8
#define MdOff_Shell	9
#define MdOff_Wrap	10
#define MdOff_XIndt	11

// Structure for non-buffer modes.
typedef struct {
	uint flags;			// Mode flags (bit masks).
	char *cmdlabel;			// Unique portion of command name and showBuffers label.
	} ModeRec;

#define MdRec_Global	0		// "Global" table index.
#define MdRec_Show	1		// "Show" (global) table index.
#define MdRec_Default	2		// "Default" table index.

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

// Flags for ldelete().
#define DFKill		0x0001		// Kill operation (save text in kill ring).
#define DFDel		0x0002		// Delete operation (save text in "undelete" buffer).

#define lforw(lnp)	((lnp)->l_nextp)
#define lback(lnp)	((lnp)->l_prevp)
#define lgetc(lnp,n)	((lnp)->l_text[(n)])
#define lputc(lnp,n,c)	((lnp)->l_text[(n)] = (c))
#define lused(lnp)	((lnp)->l_used)
#define lsize(lnp)	((lnp)->l_size)
#define ltext(lnp)	((lnp)->l_text)

// Structure and flags for the command-function table.  Flag rules are:
//  1. CFEdit and CFSpecArgs may not both be set.
//  2. CFIntN and CFArrayN may not both be set.
//  3. If no argument type flag (CFNilN, CFBoolN, CFIntN, or CFArrayN) is set, the corresponding Nth argument must be string;
//     otherwise, if CFIntN or CFArrayN is set, the Nth argument must be (or may be if CFMay set) that type; otherwise, the Nth
//     argument must be string, nil (if CFNilN set), or Boolean (if CFBoolN set).
//  4. If CFNISN is set, the Nth argument may be any type except array unless CFArrayN is also set.
typedef struct {
	char *cf_name;			// Name of command.
	ushort cf_aflags;		// Attribute flags.
	uint cf_vflags;			// Script-argument validation flags.
	short cf_minArgs;		// Minimum number of required arguments with default n arg.
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
#define CFSpecArgs	0x0200		// Needs special argument processing (never skipped).
#define CFAddlArg	0x0400		// Takes additional argument if n arg is not the default.
#define CFNoArgs	0x0800		// Takes no arguments if n arg is not the default.
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
	char *h_name;			// (Terse) name of hook.
	char *h_desc;			// Short description.
	struct Buffer *h_bufp;		// Macro to execute.
	} HookRec;

#define HkChDir		0		// Change directory hook.
#define HkEnterBuf	1		// Enter buffer hook.
#define HkExitBuf	2		// Exit buffer hook.
#define HkHelp		3		// Help hook.
#define HkMode		4		// Mode hook.
#define HkPostKey	5		// Post-key hook.
#define HkPreKey	6		// Pre-key hook.
#define HkRead		7		// Read file or change buffer filename hook.
#define HkWrap		8		// Word wrap hook.
#define HkWrite		9		// Write file hook.

// Command-function ids.
enum cfid {
	cf_abort,cf_about,cf_abs,cf_alias,cf_alterBufMode,cf_alterDefMode,cf_alterGlobalMode,cf_alterShowMode,cf_appendFile,
	cf_array,cf_backChar,cf_backLine,cf_backPage,cf_backPageNext,cf_backPagePrev,cf_backTab,cf_backWord,cf_basename,cf_beep,
	cf_beginBuf,cf_beginKeyMacro,cf_beginLine,cf_beginText,cf_beginWhite,cf_bindKey,cf_binding,cf_bufBoundQ,cf_bufList,
	cf_bufSize,cf_chDir,cf_chr,cf_clearBuf,cf_clearKillRing,cf_clearMsg,cf_clone,cf_copyFencedText,cf_copyLine,
	cf_copyRegion,cf_copyToBreak,cf_copyWord,
#if WordCount
	cf_countWords,
#endif
	cf_cycleKillRing,cf_definedQ,cf_deleteAlias,cf_deleteBackChar,cf_deleteBackTab,cf_deleteBlankLines,cf_deleteBuf,
	cf_deleteFencedText,cf_deleteForwChar,cf_deleteForwTab,cf_deleteLine,cf_deleteMacro,cf_deleteMark,cf_deleteRegion,
	cf_deleteScreen,cf_deleteToBreak,cf_deleteWhite,cf_deleteWind,cf_deleteWord,cf_detabLine,cf_dirname,cf_dupLine,
	cf_emptyQ,cf_endBuf,cf_endKeyMacro,cf_endLine,cf_endWhite,cf_endWord,cf_entabLine,cf_env,cf_eval,cf_exit,cf_findFile,
	cf_forwChar,cf_forwLine,cf_forwPage,cf_forwPageNext,cf_forwPagePrev,cf_forwTab,cf_forwWord,cf_getKey,cf_gotoFence,
	cf_gotoLine,cf_gotoMark,cf_growWind,cf_help,cf_hideBuf,cf_huntBack,cf_huntForw,cf_includeQ,cf_indentRegion,cf_index,
	cf_insert,cf_insertBuf,cf_insertFile,cf_insertLineI,cf_insertPipe,cf_insertSpace,cf_inserti,cf_join,cf_joinLines,
	cf_joinWind,cf_kill,cf_killFencedText,cf_killLine,cf_killRegion,cf_killToBreak,cf_killWord,cf_lastBuf,cf_lcLine,
	cf_lcRegion,cf_lcString,cf_lcWord,cf_length,cf_let,cf_markBuf,cf_match,cf_metaPrefix,cf_moveWindDown,cf_moveWindUp,
	cf_narrowBuf,cf_negativeArg,cf_newScreen,cf_newline,cf_newlineI,cf_nextBuf,cf_nextScreen,cf_nextWind,cf_nilQ,cf_notice,
	cf_nullQ,cf_numericQ,cf_onlyWind,cf_openLine,cf_ord,cf_outdentRegion,cf_overwrite,cf_pathname,cf_pause,cf_pipeBuf,
	cf_pop,cf_prefix1,cf_prefix2,cf_prefix3,cf_prevBuf,cf_prevScreen,cf_prevWind,cf_print,cf_prompt,cf_push,cf_queryReplace,
	cf_quickExit,cf_quote,cf_quoteChar,cf_rand,cf_readBuf,cf_readFile,cf_readPipe,cf_redrawScreen,cf_replace,cf_replaceText,
	cf_resetTerm,cf_resizeWind,cf_restoreBuf,cf_restoreWind,cf_run,cf_saveBuf,cf_saveFile,cf_saveWind,cf_scratchBuf,
	cf_searchBack,cf_searchForw,cf_selectBuf,cf_setBufFile,cf_setBufName,cf_setHook,cf_setMark,cf_setWrapCol,cf_seti,
	cf_shQuote,cf_shell,cf_shellCmd,cf_shift,cf_showBindings,cf_showBuffers,cf_showFunctions,cf_showHooks,cf_showKey,
	cf_showKillRing,cf_showMarks,cf_showModes,
#if MMDebug & Debug_ShowRE
	cf_showRegexp,
#endif
	cf_showScreens,cf_showVariables,cf_shrinkWind,cf_space,cf_split,cf_splitWind,cf_sprintf,cf_statQ,cf_strPop,cf_strPush,
	cf_strShift,cf_strUnshift,cf_stringFit,cf_strip,cf_sub,cf_subLine,cf_subString,cf_suspend,cf_swapMark,cf_sysInfo,cf_tab,
	cf_tcString,cf_tcWord,cf_toInt,cf_toString,cf_tr,cf_traverseLine,cf_trimLine,cf_truncBuf,cf_typeQ,cf_ucLine,cf_ucRegion,
	cf_ucString,cf_ucWord,cf_unbindKey,cf_unchangeBuf,cf_undelete,cf_unhideBuf,cf_universalArg,cf_unshift,cf_updateScreen,
	cf_viewFile,cf_whence,cf_widenBuf,cf_windList,cf_wordCharQ,cf_wrapLine,cf_wrapWord,cf_writeBuf,cf_writeFile,
	cf_xPathname,cf_xeqBuf,cf_xeqFile,cf_xeqKeyMacro,cf_yank,cf_yankPop
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
