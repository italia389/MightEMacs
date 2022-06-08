// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// std.h	Standard definitions for MightEMacs included in all C source files.

#include "stdos.h"

// CXL include files.
#include "cxl/excep.h"
#include "cxl/datum.h"
#include "cxl/string.h"
#include "cxl/array.h"

// Other standard include files.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

// Program-logic (source code) debugging flags.
#define SanityCheck	1		// Include "sanity check" code (things that should never happen).

#define Debug_Logfile	0x00000001	// Open file SingleLogfile for debugging output.
#define Debug_Endless	0x00000002	// Append debugging output to file MultiLogfile.
#define Debug_ScrnDump	0x00000004	// Dump screens, windows, and buffers.
#define Debug_CFAB	0x00000008	// Show CFAB pointer type in "showCFAM" display.
#define Debug_Narrow	0x00000010	// Dump buffer info to log file in narrowBuf().
#define Debug_RingDump	0x00000020	// Include dumpRing() function.
#define Debug_WindCount	0x00000040	// Display buffer's window count in "showBuffers" display.
#define Debug_ShowRE	0x00000080	// Include showRegexp command.
#define Debug_Token	0x00000100	// Dump token-parsing info to log file.
#define Debug_Datum	0x00000200	// Dump Datum processing info to log file.
#define Debug_CallArg	0x00000400	// Dump user-routine-argument info to log file.
#define Debug_Script	0x00000800	// Write script lines to log file.
#define Debug_Expr	0x00001000	// Write expression-parsing info to log file.
#define Debug_Preproc	0x00002000	// Dump script preprocessor blocks to log file and exit.
#define Debug_ArrayLog	0x00004000	// Write array memory-management info to log file..
#define Debug_ArrayBuf	0x00008000	// Write array memory-management info to a buffer.
#define Debug_Bind	0x00010000	// Dump binding table.
#define Debug_Modes	0x00020000	// Write mode-processing info to log file.
#define Debug_ModeDump	0x00040000	// Dump buffer modes or mode table.
#define Debug_MsgLine	0x00080000	// Dump message-line input info.
#define Debug_Wrap	0x00100000	// Write line-wrapping info to log file.
#define Debug_PipeCmd	0x00200000	// Debug shell pipes and commands.
#define Debug_NCurses	0x00400000	// Debug ncurses routines.
#define Debug_SrchRepl	0x00800000	// Debug search and replace routines.
#define Debug_Regexp	0x01000000	// Debug regular expressions.
#define Debug_Temp	0x80000000	// For ad-hoc use.

#define MMDebug		0		// No debugging code.
//#define MMDebug		Debug_Logfile
//#define MMDebug		(Debug_SrchRepl | Debug_Logfile)
//#define MMDebug		(Debug_NCurses | Debug_Logfile)
//#define MMDebug		(Debug_PipeCmd | Debug_Logfile | Debug_Endless)
//#define MMDebug		(Debug_MsgLine | Debug_Logfile)
//#define MMDebug		(Debug_ModeDump | Debug_Logfile)
//#define MMDebug		(Debug_Modes | Debug_ModeDump | Debug_Logfile)
//#define MMDebug		(Debug_Narrow | Debug_Logfile)
//#define MMDebug		(Debug_ArrayLog | Debug_Datum | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Expr | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Token | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Token | Debug_Expr | Debug_Logfile)
//#define MMDebug		(Debug_Datum | Debug_Script | Debug_Token | Debug_Expr | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_CallArg | Debug_Logfile)
//#define MMDebug		(Debug_Preproc | Debug_Logfile)
//#define MMDebug		(Debug_Bind | Debug_Logfile)
//#define MMDebug		(Debug_ShowRE | Debug_Logfile)
//#define MMDebug		Debug_ShowRE
//#define MMDebug		(Debug_Logfile | Debug_ShowRE)
//#define MMDebug		(Debug_Logfile | Debug_Temp)

#if MMDebug & Debug_CallArg
#define execBuf		execBufDebug
#endif

// Program identification.
#define ProgName	"MightEMacs"
#define ProgVer		"9.6.0"

/***** BEGIN CUSTOMIZATIONS *****/

// Terminal size -- [Set any except TTY_MinCols and TTY_MinRows].
#define TTY_MinCols	40		// Minimum number of columns.
#define TTY_MaxCols	240		// Maximum number of columns.
#define TTY_MinRows	3		// Minimum number of rows.
#define TTY_MaxRows	80		// Maximum number of rows.

// Language text options -- [Set one of these].
#define English		1		// Default.
#define Spanish		0		// If any non-English macro is set, must translate english.h to the corresponding
					// header file (e.g., spanish.h) and edit lang.h to include it.
// Configuration options -- [Set any].
#define WordCount	0		// Include code for "countWords" command (deprecated).
#define MacroDelims	",|;"		// Macro encoding delimiters, in order of preference.
#define BackupExt	".bak"		// Backup file extension.
#define ScriptExt	".ms"		// Script file extension.
#define UserStartup	".memacs"	// User start-up file (in HOME directory).
#define SiteStartup	"memacs.ms"	// Site start-up file.
#define MMPath_Name	"MSPATH"	// Name of shell environmental variable containing custom search path.
#define MMPath ":/usr/local/share/memacs/scripts"
					// Standard search path for script files.

// Limits -- [Set any].
#define LineBlockSize	32		// Number of bytes, line block chunks.
#define MacroRingSize	 0		// Default number of entries in macro ring.
#define KillRingSize	50		// Default number of entries in kill ring.
#define DelRingSize	30		// Default number of entries in delete ring.
#define SearchRingSize	40		// Default number of entries in search ring.
#define ReplRingSize	20		// Default number of entries in replacement ring.
#define AutoSaveTrig	140		// Number of keystrokes before auto-save -- initial value.
#define MaxTabSize	240		// Maximum hard or soft tab size.
#define MaxTermInp	(MaxPathname < 1024 ? 1024 : MaxPathname)
					// Maximum length of terminal input in bytes (must be >= MaxPathname).
#define MaxBufname	24		// Maximum length of a buffer name in bytes.
#define MaxMacroName	32		// Maximum length of a macro name in bytes.
#define MaxModeGrpName	32		// Maximum length of a mode or group name in bytes.
#define MaxVarName	32		// Maximum length of a user variable name in bytes (including prefix).
#define MaxLoop		10000		// Default maximum number of script loop iterations allowed.
#define MaxCallDepth	100		// Default maximum depth of user command or function recursion allowed.
#define MaxArrayDepth	30		// Default maximum depth of array recursion allowed when cloning, etc.
#define MaxPromptPct	80		// Default maximum percentage of terminal width for prompt string (in range 15-90).
#define FencePause	26		// Default time in centiseconds to pause for fence matching.
#define PageOverlap	1		// Number of lines to overlap when paging on a screen.
#define HorzJump	15		// Default horizontal jump size (percentage).
#define HorzJumpStr	"15"		// HorzJump in string form.
#define VertJump	25		// Default vertical jump size (percentage).
#define VertJumpStr	"25"		// VertJump in string form.
#define TravJump	12		// Default line-traversal jump size (characters).
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
#	define VerKey_Debian	"debian"
#	define VerKey_MacOS	"darwin"
#	define VerKey_Ubuntu	"ubuntu"
#	define CentOS_Release	"/etc/centos-release"
#	define RedHat_Release	"/etc/redhat-release"
#else
#	define OSName	"Unix"
#endif

// Miscellaneous.
#define LocalLogfile	"memacs.log"	// Local log file for debugging (overwritten).
#define MultiLogfile	"/tmp/" LocalLogfile	// Multi-session log file for debugging (appended to).
#define Scratch		"scratch"	// Prefix for scratch buffers.
#define Buffer1		"unnamed"	// First buffer created.
#define WordChars	"A-Za-z0-9_"	// Characters in a word.

// Internal constants.
#define KeyTableCount	5		// Number of key binding tables (no prefix, Meta, ^C, ^H, and ^X).
#define KeyVectSlots	(128 + 94 + 1 + 94)	// Number of slots in key binding vector (for one prefix key or no prefix).
#define LineDelimLen	2		// Number of bytes, input and output line delimiters.
#define PatSizeMin	32		// Minimum array size for search or replacement pattern.
#define PatSizeMax	96		// Maximum array size to retain for search or replacement pattern.
#define MacroChunk	48		// Extension size of macro buffer when full.
#define WorkBufSize	80		// Number of bytes, work buffer.
#define RegionMark	'.'		// Region mark, which defines region end point.
#define RegionMarkStr	"?."
#define WorkMark	'`'		// Work mark, used to save point position by certain commands.
#define WorkMarkStr	"?`"

// Color overrides and defaults.  Basic colors provided by ncurses are mostly dim and unsuitable.
#define ColorML		88		// Best color number for mode lines when more than eight colors available (dark brown).
#define ColorInfo	28		// Best color number for informational display headers and lines (medium green).
#define DefColorInfo	COLOR_BLUE	// Default color number for informational display headers and lines.
#define ColorMRI	9		// Best color number for macro recording indicator (bright red).
#define DefColorMRI	COLOR_RED	// Default color number for macro recording indicator.
#define ColorText	15		// Best color number for text on colored background (bright white).
#define DefColorText	COLOR_WHITE	// Default color number for text on colored background.
#define ColorPairML	0		// Reserved color pair (at high end) for mode lines.
#define ColorPairMRI	1		// Reserved color pair (at high end) for macro recording indicator.
#define ReservedPairs	2		// Number of color pairs reserved for internal use (above).
#define ColorPairIH	0		// Unreserved color pair (at high work end) to use for informational header.
#define ColorPairISL	1		// Unreserved color pair (at high work end) to use for informational separator line.

// Operation flags used at runtime (in "opFlags" member of "sess" global variable).
#define OpVTermOpen	0x0001		// Virtual terminal open.
#define OpEval		0x0002		// Evaluating expressions (in "true" state).
#define OpHaveBold	0x0004		// Terminal supports bold attribute.
#define OpHaveRev	0x0008		// Terminal supports reverse video attribute.
#define OpHaveUL	0x0010		// Terminal supports underline attribute.
#define OpHaveColor	0x0020		// Terminal supports color.
#define OpStartup	0x0040		// In pre-edit-loop state.
#define OpScript	0x0080		// Script or command-line execution in progress.
#define OpParens	0x0100		// Command, function, or alias invoked in xxx() form.
#define OpNoLoad	0x0200		// Do not load function arguments (non-command-line hook is running).
#define OpScrnRedraw	0x0400		// Clear and redraw screen (in update() function).
#define OpUserCmd	0x0800		// User command being executed (interactive mode).

// Terminal attribute characters.
#define AttrSpecBegin	'~'		// First character of a terminal attribute sequence.
#define AttrAlt		'#'		// Alternate attribute form.
#define AttrBoldOn	'b'		// Bold on.
#define AttrBoldOff	'B'		// Bold off.
#define AttrColorOn	'c'		// Color on.
#define AttrColorOff	'C'		// Color off.
#define AttrRevOn	'r'		// Reverse on.
#define AttrRevOff	'R'		// Reverse off.
#define AttrULOn	'u'		// Underline on.
#define AttrULOff	'U'		// Underline off.
#define AttrAllOff	'Z'		// All attributes off.

// Flags used when processing a terminal attribute specification in a string.  May be combined with ML* message line flags so
// higher bits are used.
#define TA_AltUL	0x1000		// Skip spaces when underlining.
#define TA_ScanOnly	0x2000		// Scan input string and return count only -- no output.

// Buffer operation flags used by bufOp() function.
#define BO_BeginEnd	1		// Move to beginning or end of buffer.
#define BO_GotoLine	2		// Go to to a line in the buffer.
#define BO_ReadBuf	3		// Read line(s) from a buffer.

// Flags used by convertCase() for controlling case conversions.
#define CaseWord	0x0001		// Convert word(s).
#define CaseLine	0x0002		// Convert line(s).
#define CaseRegion	0x0004		// Convert current region.
#define CaseLower	0x0008		// Convert to lower case.
#define CaseTitle	0x0010		// Convert to title case.
#define CaseUpper	0x0020		// Convert to upper case.

// Information display characters for showBuffers and showModes commands.
#define SB_Active	'+'		// BFActive flag indicator (activated buffer -- file read in).
#define SB_Background	'.'		// Buffer in background (not being displayed).
#define SB_Changed	'*'		// BFChanged flag indicator (changed buffer).
#define SB_Hidden	'?'		// BFHidden flag indicator (hidden buffer).
#define SB_Command	'c'		// BFCommand flag indicator (user command buffer).
#define SB_Func		'f'		// BFFunc flag indicator (user function buffer).
#define SB_Preproc	':'		// BFPreproc flag indicator (preprocessed buffer).
#define SB_Narrowed	'<'		// BFNarrowed flag indicator (narrowed buffer).
#define SB_ReadOnly	'#'		// BFReadOnly flag indicator (read-only buffer).
#define SB_TermAttr	'~'		// BFTermAttr flag indicator (terminal attributes enabled).

#define SM_Active	'+'		// MdEnabled flag indicator (active mode).
#define SM_User		':'		// MdUser flag indicator (user defined).
#define SM_Hidden	'?'		// MdHidden flag indicator (hidden mode).
#define SM_Locked	'#'		// MdLocked flag indicator (permanent scope).

// Key prefixes.
#define Ctrl		0x0100		// Control flag.  *** Also undef'd and redefined in unix.c ***
#define Meta		0x0200		// Meta flag.
#define Pref1		0x0400		// Prefix1 (^X) flag.
#define Pref2		0x0800		// Prefix2 (^C) flag.
#define Pref3		0x1000		// Prefix3 (^H) flag.
#define Shift		0x2000		// Shift flag (for function keys).
#define FKey		0x4000		// Special key flag (function key).
#define Prefix		(Meta | Pref1 | Pref2 | Pref3)	// Prefix key mask.

#define RtnKey		(Ctrl | 'M')	// "return" key as an extended key.
#define AltRtnKey	(Ctrl | 'J')	// newline as an extended key (alternate return key).
#define TermResizeKey	(FKey | 'R')	// Dummy key used for ncurses "terminal resize" event.

// Command return status codes.  Note that Cancelled, NotFound, IONSF, and IOEOF are never actually set via rsset() (so
// sess.rtn.status will never be one of those codes); whereas, all other status codes are always set, either explicitly or
// implicitly.
#define Panic		-11		// Panic return -- exit immediately (from rsset()).
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

// Return status information from a command or function.
typedef struct {
	short status;			// Most severe status returned from any C function.
	ushort flags;			// Flags.
	Datum msg;			// Status message, if any, or help message from -?, -C, -h, or -V switch.
	} RtnStatus;

// Return status flags.
#define RSNoFormat	0x0001		// Don't call vasprintf() or parse terminal attributes; use format string verbatim.
#define RSNoWrap	0x0002		// Don't wrap Success message.
#define RSOverride	0x0004		// Ignore 'RtnMsg' global mode (operate as if it is enabled).
#define RSForce		0x0008		// Force-save new message of equal severity -- ignore 'RtnMsg' global mode.
#define RSHigh		0x0010		// High priority Success message.  Overwrite any non-high one that wasn't forced.
#define RSKeepMsg	0x0020		// Don't replace any existing message (just change severity).
#define RSTermAttr	0x0040		// Enable terminal attributes in message.

// Sample string buffer used for error reporting -- allocated at program launch depending on the terminal width.
typedef struct {
	char *buf;			// Buffer for sample string, often ending in "...".
	ushort bufLen;			// Size of buffer (allocated from heap).
	ushort smallSize;		// Small sample size.
	} SampBuf;

// Macro parameters.
typedef struct {
	Datum name;			// Name of macro, or nil if none.
	uint size;			// Current size of pMacBuf.
	ushort *pMacBuf;		// Macro buffer (allocated from heap).
	ushort *pMacSlot;		// Pointer to next slot in buffer (during recording and playback).
	ushort *pMacEnd;		// Pointer to end of the last macro recorded.
	ushort state;			// Current state.
	int n;				// Requested number of repetitions (0 = infinite).
	} Macro;

// Macro states.
#define MacStop		0		// Stopped (idle).
#define MacPlay		1		// Playing.
#define MacRecord	2		// Recording.

// Macro flags for searching.
#define MF_Name		0x0001		// Search by name, otherwise entry number.
#define MF_Required	0x0002		// Error if entry not found.

// Text insertion style.
#define Txt_Insert	0x0001
#define Txt_Overwrite	0x0002
#define Txt_Replace	0x0004
#define Txt_LiteralNL	0x0008		// Literal newline character.

// xPathname flags.
#define XP_HomeOnly	0x0001		// Search HOME directory only.
#define XP_GlobPat	0x0002		// Do glob search.
#define XP_SkipNull	0x0004		// Skip null directories in $execPath.

// Flags used by runCmd().
#define RunQSimple	0x0001		// Simple quoting: wrap double quotes around argument.
#define RunQFull	0x0002		// Full quoting: call quote() to quote argument.

// Flags used by pipeCmd().
#define PipeWrite	0x0001		// Write target buffer to pipe1 (pipeBuf).
#define PipePopOnly	0x0002		// Pop command results only (shellCmd).
#define PipeInsert	0x0004		// Insert pipe output into target buffer; otherwise, replace its contents (readPipe).

// Descriptor for display item colors and array indices into "colors" ETerm member.
typedef struct {
	const char *name;		// Name of element.
	short colors[2];		// Foreground and background color numbers.
	} ItemColor;

#define ColorIdxInfo	0		// Colors for informational displays (show* commands).
#define ColorIdxModeLn	1		// Colors for mode lines.
#define ColorIdxMRI	2		// Colors for macro recording indicator.

// Universal keyword option object and control flags.
typedef struct {
	const char *keyword;		// Keyword.
	const char *abbr;		// Abbreviation or NULL, to use keyword.
	ushort ctrlFlags;		// Control flags.
	union {				// Option value.
		uint value;		// Unsigned integer value (flag).
		void *ptr;		// Boolean or caller-specific pointer.
		} u;
	} Option;

#define OptIgnore	0x0001		// Option ignored by parseOpts().
#define OptFalse	0x0002		// Set option's pointer target to false if selected.
#define OptSelected	0x0004		// Option was selected.

// Option header object.
typedef struct {
	uint argFlags;			// Argument flags for funcArg() call.
	const char *type;		// Brief description of option type, for error reporting.
	bool single;			// Single option only (items in list are mutually exclusive).
	Option *optTable;		// Option table.
	} OptHdr;

// The editor communicates with the terminal using an API (the ncurses library).  The ETerm structure holds parameters needed
// for terminal management.
typedef struct {
	int maxCols;			// Maximum number of columns allowed (allocated).
	int cols;			// Current number of columns.
	int maxRows;			// Maximum number of rows allowed (allocated).
	int rows;			// Current number of rows used.
	int maxPromptPct;		// Maximum percentage of terminal width for prompt string (in range 15-90).
	int msgLineCol;			// Current cursor column on message line + 1 (which may be equal to terminal width).
	WINDOW *pMsgLineWind;		// ncurses window for message line.
	short maxColor;			// Maximum color number available.
	short maxPair;			// Maximum color pair number available.
	short maxWorkPair;		// Maximum non-reserved color pair number available for general use (1..maxWorkPair).
	short nextPair;			// Next pair number returned by setColorPair function (1..maxWorkPair).
	short linesPerPage;		// Lines (colors) per page for showColors display.
	short colorText;		// Best text color number.
	short colorMRI;			// Best macro recording indicator color number.
	short colorInfo;		// Best informational display color number.
	ItemColor itemColors[3];	// Array of color pairs for display items.
	} ETerm;

// Operation types.  All but the last of these are mutually exclusive, however they are coded as bit masks so they can be
// combined with other flags.
#define OpQuery		0x0001		// Find an item.
#define OpCreate	0x0002		// Create an item.
#define OpUpdate	0x0004		// Update an item.
#define OpDelete	0x0008		// Delete an item.
#define OpConfirm	0x0010		// Get user confirmation for item creation.

// The editor holds text chunks in a double-linked list of buffers (e.g., the kill ring and search ring).  Chunks are stored in
// a Datum object so that text (of unpredictable size) can be saved iteratively and bidirectionally.  This can also be used in
// the future for other data types.
typedef struct RingEntry {
	struct RingEntry *prev;	// Pointer to previous item in ring.
	struct RingEntry *next;	// Pointer to next item in ring.
	Datum data;			// Data item (text chunk).
	} RingEntry;

typedef struct {
	RingEntry *pEntry;		// Current ring item.
	ushort size;			// Number of items in ring.
	ushort maxSize;			// Maximum size of ring.
	const char *ringName;		// Name of ring.
	const char *entryName;		// Name of ring entry/object (for notifications).
	} Ring;

// Ring indices.
#define RingIdxDel	0
#define RingIdxKill	1
#define RingIdxMacro	2
#define RingIdxRepl	3
#define RingIdxSearch	4

// Descriptor for mode group.  Members of a group are mutually exclusive (only one of the modes can be enabled at any given
// time) and must all have the same scope (global or buffer).  Group may be empty or have just one member.
typedef struct ModeGrp {
	struct ModeGrp *next;		// Pointer to next record in linked list, or NULL if none.
	char *descrip;			// Description, or NULL if none.
	ushort flags;			// Attribute flags.
	ushort useCount;		// Use count.
	char name[1];			// Name of mode group (in camel case).
	} ModeGrp;

// Descriptor for global and buffer modes.
typedef struct {
	char *descrip;			// Description, or NULL if none.
	ModeGrp *pModeGrp;		// Pointer to group this mode is a member of, or NULL if none.
	ushort flags;			// Attribute and state flags.
	char name[1];			// Name of mode in camel case.
	} ModeSpec;

// Macro for fetching ModeSpec pointer from Datum object in an Array element, given Datum pointer.
#define modePtr(pDatum)	((ModeSpec *) (pDatum)->u.mem.ptr)

// Mode attribute and state flags.
#define MdUser		0x0001		// User defined.  Mode or group may be deleted if set.
#define MdGlobal	0x0002		// Global mode if set, otherwise buffer mode.  Cannot be changed if MdLocked set.
#define MdLocked	0x0004		// Scope cannot be changed if set (certain built-in modes).
#define MdHidden	0x0010		// Don't display on mode line if set.
#define MdInLine	0x0020		// Adds custom text to mode line (so update needed when changed).
#define MdEnabled	0x0040		// Global mode is enabled if set, otherwise N/A.

// Cache indices for built-in modes.
#define MdIdxASave	0
#define MdIdxATerm	1
#define MdIdxBak	2
#define MdIdxClob	3
#define MdIdxCol	4
#define MdIdxExact	5
#define MdIdxFence1	6
#define MdIdxFence2	7
#define MdIdxHScrl	8
#define MdIdxLine	9
#define MdIdxOver	10
#define MdIdxReadOnly	11
#define MdIdxRegexp	12
#define MdIdxRepl	13
#define MdIdxRtnMsg	14
#define MdIdxSafe	15
#define MdIdxWkDir	16
#define MdIdxWrap	17
#define NumModes	18		// Length of mode cache array.

// Mode information.
typedef struct {
	Array modeTable;		// Mode table: array of Datum objects containing pointers to ModeSpec records.
	ModeSpec *cache[NumModes];	// Cache for built-in modes.
	ModeGrp *grpHead;		// Mode group table: linked list of ModeGrp records.
	} ModeInfo;

// Position of point in a buffer.
typedef struct {
	struct Line *pLine;		// Pointer to Line structure.
	int offset;			// Offset of point in line.
	} Point;

// Message line print flags.
#define MLHome		0x0001		// Move cursor to beginning of message line before display.
#define MLTermAttr	0x0002		// Enable interpretation of terminal attribute sequences in message.
#define MLWrap		0x0004		// Wrap message [like this].
#define MLRaw		0x0008		// Output raw character; otherwise, convert to visible form if needed.
#define MLNoEOL		0x0010		// Don't overwrite char at right edge of screen with LineExt ($) char.
#define MLFlush		0x0020		// Flush output in mlputs() and mlprintf().
#define MLForce		0x0040		// Force output to message line (when script is running).

// Settings that determine a window's "face"; that is, the location of point in the buffer and in the window.
typedef struct {
	struct Line *pTopLine;		// Pointer to top line of window.
	Point point;			// Point position.
	int firstCol;			// First column displayed when horizontal scrolling is enabled.
	} Face;

// There is a window structure allocated for every active display window.  The windows are kept in a list, in top to bottom
// screen order.  The list is associated with one screen and the list head is stored in "windHead" whenever the screen is
// current.  Each window contains its own values of point and mark.  The flag field contains some bits that are set by commands
// to guide redisplay.  Although this is a bit of a compromise in terms of decoupling, the full blown redisplay is just too
// expensive to run for every input character.
typedef struct EWindow {
	struct EWindow *next;		// Next window.
	struct Buffer *pBuf;		// Buffer displayed in window.
	Face face;			// Point position, etc.
	ushort id;			// Unique window identifier (mark used to save face before it's buffer is narrowed).
	ushort topRow;			// Origin 0 top row of window.
	ushort rows;			// Number of rows in window, excluding mode line.
	short reframeRow;		// Target (reframe) row in window for line containing point.
	ushort flags;			// Update flags.
	} EWindow;

#define WFReframe	0x0001		// Window needs forced reframe.
#define WFMove		0x0002		// Point was moved to different line.
#define WFEdit		0x0004		// Current line was edited.
#define WFHard		0x0008		// Non-trivial changes... full window update needed.
#define WFMode		0x0010		// Recreate window's mode line.

// Structure for directory table.
typedef struct DirPath {
	struct DirPath *next;
	char *path;			// Absolute pathname of directory.
	} DirPath;

// This structure holds the information about each separate "screen" within the current editing session.  On a character based
// system (Text User Interface), which is used by MightEMacs, these screens overlay each other, and can individually be brought
// to the front.  On a windowing system like X-windows, macOS, MicroSoft Windows, etc., each screen is represented in an OS
// window.  The terminolgy is different in MightEMacs...
//
//	MightEMacs	The Outside World
//	----------	-----------------
//	screen		window
//	window		pane
//
// Each EScreen structure contains its own private list of windows (EWindow structure).
typedef struct EScreen {
	struct EScreen *next;		// Pointer to next screen in list.
	EWindow *windHead;		// Head of window list.
	EWindow *pCurWind;		// Current window in this screen.
	struct Buffer *pLastBuf;	// Last buffer on screen that was exited from.
	ushort num;			// Screen number (first is 1).
	ushort flags;			// Screen flags.
	ushort rows;			// Height of screen.
	ushort cols;			// Width of screen.
	const char *workDir;		// Working directory associated with screen (absolute pathname in directory table).
	int hardTabSize;		// Current hard tab size.
	int softTabSize;		// Current soft tab size (0 -> use hard tabs).
	int wrapCol;			// Current wrap column.
	int prevWrapCol;		// Previous wrap column.
	int cursorRow;			// Current cursor row on screen.
	int cursorCol;			// Current cursor column on screen.
	int firstCol;			// First display column of current line when horizontal scrolling is disabled.
	} EScreen;

// Flags in flags.
#define EScrnResize	0x0001		// Resize screen window(s) vertically when screen is frontmost.

// Flags for changing screens, windows, or buffers.
#define SWB_Repeat	0x0001		// n argument is a repeat count.
#define SWB_Forw	0x0002		// Repeat forward, otherwise backward.
#define SWB_ExitHook	0x0004		// Run "exitBuf" hook, otherwise "enterBuf".
#define SWB_NoBufHooks	0x0008		// Don't run "exitBuf" or "enterBuf" hook.
#define SWB_NoLastBuf	0x0010		// Don't update "last buffer" pointer in current screen.

// Line delimiter.
typedef struct {
	union {				// Line delimiter(s) as array or pointer.
		char delim[LineDelimLen + 1];
		char *pDelim;
		} u;
	ushort len;			// Length of line delimiter string.
	} LineDelim;

// Buffer mark and flags.
typedef struct Mark {
	struct Mark *next;		// Next mark.
	ushort id;			// Mark identifier.
	short reframeRow;		// Target (reframe) row in window for point.
	Point point;			// Point position.
	} Mark;

#define MKAutoR		0x0001		// Use mark RegionMark if default n; otherwise, prompt with no default (setMark).
#define MKAutoW		0x0002		// Use mark WorkMark if default n; otherwise, prompt with no default (markBuf).
#define MKHard		0x0004		// Always prompt, no default (delMark, gotoMark, swapMark).
#define MKViz		0x0008		// Mark must exist and be visible (gotoMark, swapMark).
#define MKExist		0x0010		// Mark must exit (delMark).
#define MKCreate	0x0020		// Mark does not have to exist (setMark, markBuf).
#define MKQuery		0x0040		// Query mode.
#define MKWind		0x0080		// Mark is a window id.

// Additional information for a buffer that contains a script.
typedef struct {
	struct LoopBlock *execBlocks;	// Pointer to compiled loop blocks.
	short minArgs;			// Declared minimum argument count.
	short maxArgs;			// Declared maximum argument count (-1 if variable).
	ushort execCount;		// Count of active executions.
	Datum argSyntax;		// User routine argument syntax, if any.
	Datum descrip;			// User routine description, if any.
	} CallInfo;

// Buffer mode record.
typedef struct BufMode {
	struct BufMode *next;		// Next mode in linked list (or NULL if none).
	ModeSpec *pModeSpec;		// Pointer to (enabled) mode in mode table.
	} BufMode;

// Text is kept in buffers.  A buffer header, described below, exists for every buffer in the system.  Buffers are kept in a
// array sorted by buffer name to provide quick access to the next or previous buffer, to allow for quick binary searching by
// name, and to eliminate the need to sort the list when doing buffer completions and creating the "showBuffers" listing.  There
// is a safe store for a window's top line, point, and first display column (in the Face object) so that windows can be cleanly
// restored to the same point in a buffer (if possible) when switching among them.  windCount is the number of windows (on any
// screen) that a buffer is associated with.  Thus if the value is zero, the buffer is not being displayed anywhere.  The text
// for a buffer is kept in a circularly linked list of lines with a pointer to the first line in pFirstLine.  When a buffer is
// first created or cleared, the first (and only) line will point to itself, both forward and backward.  The line will also be
// empty indicating that the buffer is empty.  A buffer will always have a minimum of one line associated with it at all times.
// All lines except the last have an implicit newline delimiter; however, the last line never does.  Hence, files which do not
// have a newline as their last character can be handled properly.  A buffer may be "inactive" which means the file associated
// with it has not been read in yet.  The file is read at "use buffer" time.
typedef struct Buffer {
	Face face;			// Point position, etc. from last detached window.
	struct Line *pFirstLine;	// Pointer to first line.
	struct Line *pNarTopLine;	// Pointer to narrowed top text.
	struct Line *pNarBotLine;	// Pointer to narrowed bottom text.
	Mark markHdr;			// Mark list header.
	CallInfo *pCallInfo;		// Pointer to user command/function parameters, if applicable.
	const char *saveDir;		// Buffer's "home" directory (absolute pathname in directory table).
	ushort windCount;		// Count of windows displaying buffer.
	ushort aliasCount;		// Count of aliases pointing to this user command/function buffer.
	ushort flags;			// Flags.
	BufMode *modes;			// Enabled buffer modes (linked list of BufMode records), initially NULL.
	LineDelim inpDelim;		// Record delimiter(s) used to read buffer.
	char *filename;			// Filename (on heap).
	char bufname[MaxBufname + 1];	// Buffer name.
	} Buffer;

// Macro for fetching Buffer pointer from Datum object in Array element, given Datum pointer.
#define bufPtr(pDatum)	((Buffer *) (pDatum)->u.mem.ptr)

// Buffer flags and masks.
#define BFActive	0x0001		// Active buffer (file was read).
#define BFChanged	0x0002		// Changed since last write.
#define BFCommand	0x0004		// Buffer is a user command.
#define BFFunc		0x0008		// Buffer is a user function.
#define BFHidden	0x0010		// Hidden buffer.
#define BFNarrowed	0x0020		// Buffer is narrowed.
#define BFPreproc	0x0040		// (Script) buffer has been preprocessed.
#define BFQSave		0x0080		// Buffer was saved via quickExit().
#define BFReadOnly	0x0100		// Buffer is read only.
#define BFTermAttr	0x0200		// Terminal attributes enabled.

#define BFCmdFunc	(BFCommand | BFFunc)
#define BCmdFuncLead	'@'		// Leading character of script buffer.
#define BAltBufLead	'*'		// Substitution character for non-script buffer names that begin with BCmdFuncLead.
#define BSysLead	'.'		// Leading character of system (internal) buffer.

// Buffer traversal flags.
#define BT_Backward	0x0001		// Traverse buffer list backward, otherwise forward.
#define BT_Hidden	0x0002		// Include hidden, non-user-routine buffers.
#define BT_HomeDir	0x0004		// Select buffers having same home directory as current buffer only.
#define BT_Delete	0x0008		// Delete current buffer after switch.

// Buffer searching flags.
#define BS_Query	0x0000		// Look-up only (do not create).
#define BS_Create	0x0001		// Create buffer if non-existent.
#define BS_Extend	0x0002		// Create buffer-extension record (CallInfo).
#define BS_Force	0x0004		// Force-create buffer with unique name.
#define BS_Derive	0x0008		// Derive buffer name from filename.
#define BS_CreateHook	0x0010		// Execute "createBuf" hook if a buffer is created.
#define BS_Confirm	0x0020		// Get user confirmation if creating buffer.

// Buffer clearing flags.
#define BC_IgnChgd	0x0001		// Ignore BFChanged (buffer changed) flag.
#define BC_Unnarrow	0x0002		// Force-clear narrowed buffer (unnarrow first).
#define BC_ClrFilename	0x0004		// Clear associated filename, if any.
#define BC_Confirm	0x0008		// Always ask for confirmation (when interactive).
#define BC_ShowName	0x0010		// Show buffer name in confirmation prompt.

// Buffer deleting flags.
#define BD_Visible	0x0001		// Select visible buffers.
#define BD_Unchanged	0x0002		// Select unchanged buffers.
#define BD_Homed	0x0004		// Select "homed" buffers.
#define BD_Inactive	0x0008		// Select inactive buffers.
#define BD_Displayed	0x0010		// Include displayed buffers.
#define BD_Hidden	0x0020		// Include hidden buffers.
#define BD_Confirm	0x0040		// Confirm deletion.
#define BD_Force	0x0080		// Force-delete all selected buffers (ignore changes).

// Buffer saving flags.
#define BS_All		0x0001		// Saving all buffers, otherwise just current one.
#define BS_MultiDir	0x0002		// Multiple working directories exist (in screens).
#define BS_QuickExit	0x0004		// Called from quickExit command.

// Buffer renaming flags.
#define BR_Auto		0x0001		// Auto-rename buffer (derive name from associated filename).
#define BR_Current	0x0002		// Rename current buffer (interactively).

// Buffer rendering flags.
#define RendRewind	0x0001		// Move point to beginning of buffer and unhide it if buffer is not deleted.
#define RendAltML	0x0002		// Use alternate (simple) mode line when popping buffer.
#define RendWait	0x0004		// Wait for user to press a key before returning from pop-up window.
#define	RendShift	0x0008		// Shift display left if needed to prevent line truncation in pop-up window.
#define RendNewBuf	0x0010		// Buffer was just created (delete after pop).
#define RendNotify	0x0020		// Display "created" message if n != -1.

// File reading and writing flags.
#define RWExist		0x0001		// File must exist.
#define RWKeep		0x0002		// Keep existing filename associated with buffer.
#define RWReadHook	0x0004		// Run "read" hook.
#define RWScratch	0x0008		// Create scratch buffer.
#define RWStats		0x0010		// Return message containing I/O statistics.

// Flags for strIndex() function.
#define Idx_Char	0x0001		// Find a character, otherwise a pattern.
#define Idx_Last	0x0002		// Find last occurrence, otherwise first.

// Region descriptor: starting location, size in characters, and number of lines.  If size is negative, region extends backward
// from point, otherwise forward.
typedef struct {
	Point point;			// Origin point location.
	long size;			// Length in characters.
	int lineCount;			// Line count.
	} Region;

// Operation flags for "get region" functions.
#define RForceBegin	0x0001		// Force point in Region object to beginning of region; otherwise, leave point at
					// original starting point and negate size if region extends backward.
#define RInclDelim	0x0002		// Include line delimiter (if any) on last line selected when n > 0.
#define REmptyOK	0x0004		// Allow empty region when getting line block.
#define RLineSelect	0x0008		// Include line at bottom of region in line count if possible.

// All text is kept in linked lists of Line objects.  The Buffer object points to the first line, the first line points backward
// to the last line (so that the last line can be located quickly), and the last line points to NULL.  When a buffer is created
// or cleared, one empty Line object is allocated which points to itself backward and NULL forward.  Each line contains the
// number of bytes in the line (the "used" size), the size of the text array (bytes allocated), and the text.  The line
// delimiter (newline) is not stored as a byte; it's implied.  However, the last line of a buffer never has a delimiter.
typedef struct Line {
	struct Line *next;		// Pointer to the next line.
	struct Line *prev;		// Pointer to the previous line.
	int size;			// Allocated size.
	int used;			// Used size.
	char text[1];			// A bunch of characters.
	} Line;

// Editing flags.
#define EditKill	0x0001		// Kill operation (save text in kill ring).
#define EditDel		0x0002		// Delete operation (save text in delete ring).
#define EditSpace	0x0004		// Insert space character, otherwise newline.
#define EditWrap	0x0008		// Do word wrap, if applicable.
#define EditHoldPoint	0x0010		// Hold (don't move) point.

// Structure and flags for the command-function table.  Flag rules are:
//  1. CFEdit and CFSpecArgs may not both be set.
//  2. ArgIntN and ArgArrayN may not both be set.
//  3. If CFAddlArg or CFNoArgs set, maxArgs must be greater than zero.
//  4. If no argument type flag (ArgNilN, ArgBoolN, ArgIntN, or ArgArrayN) is set, the corresponding Nth argument must be
//     string; otherwise, if ArgIntN or ArgArrayN is set, the Nth argument must be (or may be if ArgMay set) integer or array;
//     otherwise, the Nth argument must be string, nil (if ArgNilN set), or Boolean (if ArgBoolN set).
//  5. If ArgNISN is set, the Nth argument may be nil, integer, or string, and may also be array if ArgArrayN is set.
typedef struct {
	const char *name;		// Name of command or function.
	ushort attrFlags;		// Attribute flags.
	uint argFlags;			// Script-argument validation flags.
	short minArgs;			// Minimum number of required arguments with default n argument.
	short maxArgs;			// Maximum number of arguments allowed for any value of n.  No maximum if negative.
	int (*func)(Datum *pRtnVal, int n, Datum **args);// C function that command or function is bound to.
	const char *argSyntax;		// Argument syntax.
	const char *descrip;		// Command/function description.
	} CmdFunc;

// Attribute flags.
#define CFFunc		0x0001		// Is system function.
#define CFHook		0x0002		// System function may be bound to a hook.
#define CFHidden	0x0004		// Hidden command: for internal use only.
#define CFPrefix	0x0008		// Prefix command (meta, ^C, ^H, and ^X).
#define CFBind1		0x0010		// Is bound to a single key (use getkey() in bindcmd() and elsewhere).
#define CFUniq		0x0020		// Cannot have more than one binding.
#define CFEdit		0x0040		// Modifies current buffer.
#define CFPerm		0x0080		// Must have one or more bindings at all times.
#define CFTerm		0x0100		// Terminal (interactive) only -- not recognized in a script.
#define CFNCount	0x0200		// N argument is purely a repeat count.
#define CFSpecArgs	0x0400		// Needs special argument processing (bare word or symbol) -- never skipped.
#define CFAddlArg	0x0800		// Takes additional argument if n argument is not the default.
#define CFNoArgs	0x1000		// Takes no arguments if n argument is not the default.
#define CFMinLoad	0x2000		// Load minimum number of arguments possible in execCmdFunc().
#define CFShortLoad	0x4000		// Load one fewer argument than usual in execCmdFunc().
#define CFNoLoad	0x8000		// Load no arguments in execCmdFunc().

#define CFMaxArgs	4		// Maximum number of arguments that could be loaded by execCmdFunc() for any command or
					// function; that is, maximum value of maxArgs in cmdFuncTable where CFNoLoad is not set
					// and maxArgs > 0.

// Pointer structure for places where either a command, function, alias, or buffer is allowed, and for the "exec" table, which
// is a hash of all the current command, function, and alias names with this structure as the value.
typedef struct {
	ushort type;			// Pointer type.
	union {
		CmdFunc *pCmdFunc;	// Pointer into the command-function table.
		struct Alias *pAlias;	// Alias pointer.
		struct Buffer *pBuf;	// Buffer pointer.
		void *pVoid;		// For generic pointer comparisons.
		} u;
	} UnivPtr;

// Macro for fetching UnivPtr pointer from Datum object in hash record, given HashRec pointer.
#define univPtr(pHashRec)	((UnivPtr *) (pHashRec)->pValue->u.mem.ptr)

// Pointer types.  Set to different bits so that can be used as selector masks in function calls.
#define PtrNull		0x0000		// NULL pointer.
#define PtrSysCmd	0x0001		// Built-in command-function pointer -- command.
#define PtrPseudo	0x0002		// Built-in command-function pointer -- pseudo command (CFHidden flag set).
#define PtrSysFunc	0x0004		// Built-in command-function pointer -- function.
#define PtrUserCmd	0x0008		// User command (buffer) pointer.
#define PtrUserFunc	0x0010		// User function (buffer) pointer.
#define PtrBuf		0x0020		// Buffer pointer.
#define PtrAliasSysCmd	0x0040		// Alias pointer to a command.
#define PtrAliasSysFunc	0x0080		// Alias pointer to a function.
#define PtrAliasUserCmd	0x0100		// Alias pointer to a user command.
#define PtrAliasUserFunc 0x0200		// Alias pointer to a user function.

#define PtrSysCmdType	(PtrSysCmd | PtrPseudo)
#define PtrSysCmdFunc	(PtrSysCmd | PtrSysFunc)
#define PtrAlias	(PtrAliasSysCmd | PtrAliasSysFunc | PtrAliasUserCmd | PtrAliasUserFunc)
#define PtrUserCmdFunc	(PtrUserCmd | PtrUserFunc)
#define PtrAny		(PtrSysCmdFunc | PtrPseudo | PtrUserCmdFunc | PtrBuf | PtrAlias)

// Structure for the alias list.
typedef struct Alias {
	struct Alias *next;		// Pointer to next alias.
	ushort type;			// Alias type (PtrAlias_X).
	UnivPtr targ;			// Command or function pointer.
	char name[1];			// Name of alias.
	} Alias;

// Structure for the hook table (hookTable).  Hooks are system or user functions that are invoked automatically when certain
// events occur during the editing session.
typedef struct {
	const char *name;		// Name of hook.
	const char *nArgDesc;		// Description of n argument passed to function.
	const char *macArgsDesc;	// Description of call arguments passed to function.
	short argCount;			// Required argument(s).
	bool running;			// True if hook is being executed.
	UnivPtr func;			// Function to execute.
	} HookRec;

#define HkChDir		0		// Change directory hook.
#define HkCreateBuf	1		// Create buffer hook.
#define HkEnterBuf	2		// Enter buffer hook.
#define HkExit		3		// Exit hook.
#define HkExitBuf	4		// Exit buffer hook.
#define HkFilename	5		// Change buffer filename hook.
#define HkMode		6		// Mode hook.
#define HkPostKey	7		// Post-key hook.
#define HkPreKey	8		// Pre-key hook.
#define HkRead		9		// Read file hook.
#define HkWrap		10		// Word wrap hook.
#define HkWrite		11		// Write file hook.

// Command-function IDs.
typedef enum {
	cf_abort, cf_about, cf_abs, cf_aclone, cf_acompact, cf_adelete, cf_adeleteif, cf_afill,	cf_aincludeQ, cf_aindex,
	cf_ainsert, cf_alias, cf_apop, cf_appendFile, cf_apropos, cf_apush, cf_array,
	cf_ashift, cf_aunshift, cf_backChar, cf_backLine, cf_backPage, cf_backPageNext, cf_backPagePrev, cf_backTab,
	cf_backWord, cf_backspace, cf_basename, cf_beep, cf_beginBuf, cf_beginLine, cf_beginMacro, cf_beginText, cf_beginWhite,
	cf_bemptyQ, cf_bgets, cf_bindKey, cf_binding, cf_bprint, cf_bprintf, cf_bufAttrQ, cf_bufBoundQ, cf_bufInfo, cf_bufWind,
	cf_chgBufAttr, cf_chgDir, cf_chgMode, cf_chr, cf_clearBuf, cf_clearHook, cf_clearMsgLine, cf_copyFencedRegion,
	cf_copyLine, cf_copyRegion, cf_copyToBreak, cf_copyWord,
#if WordCount
	cf_countWords,
#endif
	cf_cycleRing, cf_definedQ, cf_delAlias, cf_delBackChar, cf_delBackTab, cf_delBlankLines, cf_delBuf, cf_delFencedRegion,
	cf_delFile, cf_delForwChar, cf_delForwTab, cf_delLine, cf_delMark, cf_delRegion, cf_delRingEntry, cf_delRoutine,
	cf_delScreen, cf_delToBreak, cf_delWhite, cf_delWind, cf_delWord, cf_detabLine, cf_dirname, cf_dupLine, cf_editMode,
	cf_editModeGroup, cf_emptyQ, cf_endBuf, cf_endLine, cf_endMacro, cf_endWhite, cf_endWord, cf_entabLine, cf_env, cf_eval,
	cf_exit, cf_expandPath, cf_findFile, cf_forwChar, cf_forwLine, cf_forwPage, cf_forwPageNext, cf_forwPagePrev,
	cf_forwTab, cf_forwWord, cf_getInfo, cf_getKey, cf_getWord, cf_glob, cf_gotoFence, cf_gotoLine, cf_gotoMark,
	cf_groupModeQ, cf_growWind, cf_huntBack, cf_huntForw, cf_indentRegion, cf_index, cf_insert, cf_insertBuf, cf_insertFile,
	cf_insertPipe, cf_insertSpace, cf_insertf, cf_inserti, cf_interactiveQ, cf_isClassQ, cf_join, cf_joinLines, cf_joinWind,
	cf_keyPendingQ, cf_kill, cf_killFencedRegion, cf_killLine, cf_killRegion, cf_killToBreak, cf_killWord, cf_lastBuf,
	cf_length, cf_let, cf_linkFile, cf_lowerCaseLine, cf_lowerCaseRegion, cf_lowerCaseStr, cf_lowerCaseWord, cf_manageMacro,
	cf_markBuf, cf_match, cf_message, cf_metaPrefix, cf_modeQ, cf_moveWindDown, cf_moveWindUp, cf_narrowBuf, cf_negativeArg,
	cf_newline, cf_newlineI, cf_nextBuf, cf_nextScreen, cf_nextWind, cf_nilQ, cf_nullQ, cf_numericQ, cf_onlyWind,
	cf_openLine, cf_openLineI, cf_ord, cf_outdentRegion, cf_overwriteChar, cf_overwriteCol, cf_pathname, cf_pause,
	cf_pipeBuf, cf_popBuf, cf_popFile, cf_prefix1, cf_prefix2, cf_prefix3, cf_prevBuf, cf_prevScreen, cf_prevWind, cf_print,
	cf_printf, cf_prompt, cf_queryReplace, cf_quickExit, cf_quote, cf_quoteChar, cf_rand, cf_readFile, cf_readPipe,
	cf_reframeWind, cf_renameBuf, cf_renameFile, cf_renameMacro, cf_replace, cf_resetTerm, cf_resizeWind, cf_restoreBuf,
	cf_restoreScreen, cf_restoreWind, cf_revertYank, cf_ringSize, cf_run, cf_saveBuf, cf_saveFile, cf_saveScreen,
	cf_saveWind, cf_scratchBuf, cf_searchBack, cf_searchForw, cf_selectBuf, cf_selectLine, cf_selectScreen, cf_selectWind,
	cf_setBufFile, cf_setColorPair, cf_setDefault, cf_setDispColor, cf_setHook, cf_setMark, cf_setWrapCol, cf_seti,
	cf_shQuote, cf_shell, cf_shellCmd, cf_showAliases, cf_showBuffers, cf_showColors, cf_showCommands, cf_showDir,
	cf_showFence, cf_showFunctions, cf_showHooks, cf_showKey, cf_showMarks, cf_showModes, cf_showPoint,
#if MMDebug & Debug_ShowRE
	cf_showRegexp,
#endif
	cf_showRing, cf_showScreens, cf_showVariables, cf_shrinkWind, cf_sortRegion, cf_space, cf_split, cf_splitWind,
	cf_sprintf, cf_statQ, cf_strFit, cf_strPop, cf_strPush, cf_strShift, cf_strUnshift, cf_strip, cf_sub, cf_subline,
	cf_substr, cf_suspend, cf_swapMark, cf_tab, cf_titleCaseLine, cf_titleCaseRegion, cf_titleCaseStr, cf_titleCaseWord,
	cf_toInt, cf_toStr, cf_tr, cf_traverseLine, cf_trimLine, cf_truncBuf, cf_typeQ, cf_unbindKey, cf_undelete,
	cf_undeleteCycle, cf_universalArg, cf_updateScreen, cf_upperCaseLine, cf_upperCaseRegion, cf_upperCaseStr,
	cf_upperCaseWord, cf_viewFile, cf_widenBuf, cf_wrapLine, cf_wrapWord, cf_writeBuf, cf_writeFile, cf_xPathname,
	cf_xeqBuf, cf_xeqFile, cf_xeqMacro, cf_yank, cf_yankCycle
	} CmdFuncId;

// Object for core keys bound to special commands (like "abort").  These are maintained in global variable "coreKeys" in
// addition to the binding table for fast access.  Note: indices must be in descending, alphabetical order (by command name) so
// that coreKeys is initialized properly in editInit1().
typedef struct {
	ushort extKey;			// Extended key.
	CmdFuncId id;			// Command id (index into cmdFuncTable).
	} CoreKey;

#define CK_Abort	0		// Abort key.
#define CK_NegArg	1		// Negative argument (repeat) key.
#define CK_Quote	2		// Quote key.
#define CK_UnivArg	3		// Universal argument (repeat) key.
#define CoreKeyCount	4		// Number of cached keys.

// Structure for "i" variable.
typedef struct {
	int i;				// Current value.
	int incr;			// Increment to add to i.
	Datum format;			// sprintf() format string.
	} IVar;

// Miscellaneous definitions.
#define LineExt		'$'		// Character displayed which indicates a line is extended.

// Terminal input control parameters, which override the defaults.
typedef struct {
	const char *defVal;		// Default value.
	short delimKey;			// Input delimiter key.
	uint maxLen;			// Maximum input length (<= MaxTermInp).
	Ring *pRing;			// Data ring to use during terminal input (or NULL for none).
	} TermInpCtrl;

// Command argument validation flags.
#define ArgNotNull1	0x00000001	// First argument may not be null.
#define ArgNotNull2	0x00000002	// Second argument may not be null.
#define ArgNotNull3	0x00000004	// Third argument may not be null.
#define ArgNotNull4	0x00000008	// Fourth argument may not be null.
#define ArgNil1		0x00000010	// First argument may be nil.
#define ArgNil2		0x00000020	// Second argument may be nil.
#define ArgNil3		0x00000040	// Third argument may be nil.
#define ArgNil4		0x00000080	// Fourth argument may be nil.
#define ArgBool1	0x00000100	// First argument may be Boolean (true or false).
#define ArgBool2	0x00000200	// Second argument may be Boolean (true or false).
#define ArgBool3	0x00000400	// Third argument may be Boolean (true or false).
#define ArgBool4	0x00000800	// Fourth argument may be Boolean (true or false).
#define ArgInt1		0x00001000	// First argument must be integer.
#define ArgInt2		0x00002000	// Second argument must be integer.
#define ArgInt3		0x00004000	// Third argument must be integer.
#define ArgInt4		0x00008000	// Fourth argument must be integer.
#define ArgArray1	0x00010000	// First argument must be array (or "may" be if ArgNIS1 or ArgMay also set).
#define ArgArray2	0x00020000	// Second argument must be array (or "may" be if ArgNIS2 or ArgMay also set).
#define ArgArray3	0x00040000	// Third argument must be array (or "may" be if ArgNIS3 or ArgMay also set).
#define ArgArray4	0x00080000	// Fourth argument must be array (or "may" be if ArgNIS3 or ArgMay also set).
#define ArgNIS1		0x00100000	// First argument can be nil, integer, or string.
#define ArgNIS2		0x00200000	// Second argument can be nil, integer, or string.
#define ArgNIS3		0x00400000	// Third argument can be nil, integer, or string.
#define ArgNIS4		0x00800000	// Fourth argument can be nil, integer, or string.
#define ArgMay		0x01000000	// ArgIntN or ArgArrayN "may" be instead of "must" be.

// Command argument control flags.  May be combined with Arg* flags, Md* mode flags, or Term_* flags so higher bits are used.
#define ArgFirst	0x10000000	// First argument (so no preceding comma).
#define ArgPath		0x20000000	// Argument is a file pathname to be expanded if string type.  (Applies to first
					// argument only if set in cmdFuncTable, per code in execCmdFunc() function.)

// Terminal completion/prompt flags.  These are combined with Ptr* selector flags for certain function calls so higher bits
// are used.
#define Term_OneChar	0x00001000	// Get one key only with delimiter, C-SPC, and C-g checking.
#define Term_OneKey	0x00002000	// Get one key only, raw.
#define Term_OneKeySeq	0x00003000	// Get one key sequence only, raw.
#define Term_NoKeyEcho	0x00004000	// Don't echo keystrokes.
#define Term_LongPrmt	0x00008000	// Allow extra space (90% terminal width) for long prompt string.
#define Term_Attr	0x00010000	// Enable terminal attribute sequences in prompt string.
#define Term_NoDef	0x00020000	// Don't load default value into input buffer.
#define Term_C_NoAuto	0x00040000	// Don't auto-complete; wait for return key.
#define Term_C_CFA	0x00080000	// Command, function, or alias name completion.
#define Term_C_Buffer	0x00100000	// Buffer name completion.
#define Term_C_Filename	0x00200000	// Filename completion (via directory search).
#define Term_C_Macro	0x00400000	// Macro name completion.
#define Term_C_Mode	0x00800000	// Mode name completion (buffer or global).
#define Term_C_Ring	0x01000000	// Ring name completion.
#define Term_C_Var	0x02000000	// Variable name completion -- all.
#define Term_C_MutVar	0x04000000	// Variable name completion -- mutable (excluding constants).

#define Term_KeyMask	Term_OneKeySeq	// Mask for one-key type.
#define Term_C_Mask	(Term_C_CFA | Term_C_Buffer | Term_C_Filename | Term_C_Macro | Term_C_Mode | Term_C_Ring | Term_C_Var |\
 Term_C_MutVar)		// Completion mask.

// Control parameters for virtual terminal.
typedef struct {
	int horzJumpPct;		// Horizontal jump size - percentage.
	int horzJumpCols;		// Horizontal jump size - columns.
	int vertJumpPct;		// Vertical jump size (zero for smooth scrolling).
	const char *termName;		// Value of TERM environmental variable.
	} VTermCtrl;

// Buffer display and editing parameters.
typedef struct {
	Buffer *pBuf;			// Current buffer to display or edit.
	Face *pFace;			// Current window face to display or edit.
	EWindow *pWind;			// Current window to display or edit.
	EScreen *pScrn;			// Current screen to display or edit.
	} BufCtrl;

// Session control parameters.
typedef struct {
	BufCtrl cur;			// Current display pointers.
	BufCtrl edit;			// Current edit pointers.
	DirPath *dirHead;		// Head of directory list.
	int fencePause;			// Centiseconds to pause for fence matching.
	int autoSaveCount;		// Auto-save counter (for all buffers).
	int autoSaveTrig;		// Auto-save trigger count.
	uint myPID;			// Unix PID (for temporary filenames).
	ushort opFlags;			// Operation flags.
	int overlap;			// Number of lines to overlap when paging on a screen.
	Buffer *pSavedBuf;		// Saved buffer pointer ("saveBuf" function).
	EScreen *pSavedScrn;		// Saved screen pointer ("saveScreen" function).
	EWindow *pSavedWind;		// Saved window pointer ("saveWind" function).
	EScreen *scrnHead;		// Head of screen list.
	EWindow *windHead;		// Head of window list in current screen.
	int travJumpCols;		// Line-traversal jump size.
	int exitNArg;			// n argument given for "exit" or "quickExit" command.
	RtnStatus rtn;			// Current return parameters from a command or function.
	RtnStatus scriptRtn;		// Return parameters from a script (for $ReturnMsg).
	} SessionCtrl;

// Column-header widths, used by rptHdr().
typedef struct {
	short minWidth;			// Minimum column width, or -1 for current position to right edge of screen.
	short maxWidth;			// Maximum column width.
	} ColHdrWidth;

#include "lang.h"

// External functions not declared elsewhere.
extern int abortOp(Datum *pRtnVal, int n, Datum **args);
extern int abortInp(void);
extern int aboutMM(Datum *pRtnVal, int n, Datum **args);
extern int alias(Datum *pRtnVal, int n, Datum **args);
extern int allowEdit(bool edit);
extern int apropos(Datum *pRtnVal, int n, Datum **args);
extern int attrCount(const char *str, int len, int maxOffset);
extern int backChar(Datum *pRtnVal, int n, Datum **args);
extern int backLine(Datum *pRtnVal, int n, Datum **args);
extern int backPage(Datum *pRtnVal, int n, Datum **args);
extern int backWord(Datum *pRtnVal, int n, Datum **args);
extern int bactivate(Buffer *pBuf);
extern int bappend(Buffer *pBuf, const char *text);
extern void bchange(Buffer *pBuf, ushort flags);
extern int bclear(Buffer *pBuf, ushort flags);
extern int bconfirm(Buffer *pBuf, ushort flags);
extern Buffer *bdefault(void);
extern int bdelete(Buffer *pBuf, ushort flags);
extern int beginText(Datum *pRtnVal, int n, Datum **args);
extern int beginTxt(void);
extern int beginEndLine(Datum *pRtnVal, int n, bool end);
extern bool bempty(Buffer *pBuf);
extern int bextend(Buffer *pBuf);
extern int bfind(const char *name, ushort ctrlFlags, ushort bufFlags, Buffer **ppBuf, bool *created);
extern void bufFaceToWindFace(Buffer *pBuf, EWindow *pWind);
extern int bgets(Datum *pRtnVal, int n, Datum **args);
extern bool binSearch(const char *key, const void *table, ssize_t n, int (*cmp)(const char *str1, const char *str2),
 const char *(*fetch)(const void *table, ssize_t i), ssize_t *pIndex);
extern int bnuke(ushort dflags, const char *workDir, int *pCount);
extern int bpop(Buffer *pBuf, ushort flags);
extern int brename(Datum *pRtnVal, ushort flags, Buffer *pTargBuf);
extern int bscratch(Buffer **ppBuf);
extern int bsort(Buffer *pBuf, Point *pPoint, int n, ushort flags);
extern Buffer *bsrch(const char *bufname, ssize_t *index);
extern int bswitch(Buffer *pBuf, ushort flags);
extern int bufAttrQ(Datum *pRtnVal, int n, Datum **args);
extern bool bufBegin(Point *pPoint);
extern bool bufClearModes(Buffer *pBuf);
extern bool bufEnd(Point *pPoint);
extern int bufInfo(Datum *pRtnVal, int n, Datum **args);
extern long bufLength(Buffer *pBuf, int *pLineCount);
extern int bufOp(Datum *pRtnVal, int n, const char *prompt, uint op, int flag);
extern void calcHorzJumpCols(void);
extern void centiPause(int n);
extern int chgCase(short c);
extern int chgBufAttr(Datum *pRtnVal, int n, Datum **args);
extern int chgdir(const char *dir);
extern int chgMode(Datum *pRtnVal, int n, Datum **args);
extern int chgMode1(const char *name, int action, Buffer *pBuf, long *formerStatep, ModeSpec **result);
extern int chgWindSize(Datum *pRtnVal, int n, int how);
extern int chgWorkDir(Datum *pRtnVal, int n, Datum **args);
extern int clearBuf(Datum *pRtnVal, int n, Datum **args);
extern void clearBufFilename(Buffer *pBuf);
extern void clearBufMode(ModeSpec *pModeSpec);
extern void clearGlobalMode(ModeSpec *pModeSpec);
extern int convertCase(int n, ushort flags);
#if WordCount
extern int countWords(Datum *pRtnVal, int n, Datum **args);
#endif
extern int cycleRing(Datum *pRtnVal, int n, Datum **args);
extern int definedQ(Datum *pRtnVal, int n, Datum **args);
extern int delAlias(Datum *pRtnVal, int n, Datum **args);
extern int delBlankLines(Datum *pRtnVal, int n, Datum **args);
extern int delBuf(Datum *pRtnVal, int n, Datum **args);
extern void delBufMarks(Buffer *pBuf, ushort flags);
extern int delKillRegion(int n, bool kill);
extern int delMark(Datum *pRtnVal, int n, Datum **args);
extern int delRingEntry(Datum *pRtnVal, int n, Datum **args);
extern int delRoutine(Datum *pRtnVal, int n, Datum **args);
extern int delScreen(Datum *pRtnVal, int n, Datum **args);
extern int delWind(Datum *pRtnVal, int n, Datum **args);
extern int delTab(int n, bool force);
extern void deselectOpts(Option *pOpt);
extern int detabLine(Datum *pRtnVal, int n, Datum **args);
extern void dgPop(Datum *pDatum);
extern int doFill(Datum *pRtnVal, Datum **args);
extern int doPop(Datum *pRtnVal, int n, bool popBuf);
#if MMDebug & (Debug_ScrnDump | Debug_Narrow | Debug_Temp)
extern void dumpBuffer(const char *label, Buffer *pBuf, bool withData);
#endif
#if MMDebug & Debug_ModeDump
void dumpModes(Buffer *pBuf, bool sepLine, const char *fmt, ...);
#endif
#if MMDebug & Debug_ScrnDump
extern void dumpScreens(const char *msg);
#endif
extern int dupLine(Datum *pRtnVal, int n, Datum **args);
extern int edelc(long n, ushort flags);
extern int edInsertTab(int n);
extern int editAlias(const char *name, ushort op, UnivPtr *pUniv);
extern int editMode(Datum *pRtnVal, int n, Datum **args);
extern int editModeGroup(Datum *pRtnVal, int n, Datum **args);
extern int einsertc(int n, short c);
extern int einserts(const char *str);
extern int einsertNL(void);
extern short ektoc(ushort extKey, bool extend);
extern int endWord(Datum *pRtnVal, int n, Datum **args);
extern int entabLine(Datum *pRtnVal, int n, Datum **args);
extern int eopendir(const char *fileSpec, char **pFilePath);
extern int ereaddir(void);
extern int escAttr(DFab *pFab);
extern int esctofab(const char *str, DFab *pFab);
extern int execFind(const char *name, ushort op, uint selector, UnivPtr *pUniv);
extern int execNew(const char *name, UnivPtr *pUniv);
extern int expandPath(Datum *pDest, Datum *pSrc);
extern void faceInit(Face *pFace, Line *pLine, Buffer *pBuf);
extern int fenceMatch(short fence, bool doBeep);
extern Buffer *findBuf(Datum *pBufname);
extern int findBufMark(ushort id, Mark **ppMark, ushort flags);
extern int fIndex(Datum *pRtnVal, int n, Datum **args);
extern Ring *findRing(Datum *pName);
extern EWindow *findWind(Buffer *pBuf, EScreen **ppScrn);
extern void fixBufFace(Buffer *pBuf, int offset, int n, Line *pLine1, Line *pLine2);
extern void fixFace(int offset, int n, Line *pLine1, Line *pLine2);
extern int forwChar(Datum *pRtnVal, int n, Datum **args);
extern int forwLine(Datum *pRtnVal, int n, Datum **args);
extern int forwPage(Datum *pRtnVal, int n, Datum **args);
extern int forwWord(Datum *pRtnVal, int n, Datum **args);
extern int getBufname(Datum *pRtnVal, const char *prompt, const char *defName, uint flags, Buffer **ppBuf, bool *created);
extern int getCol(Point *pPoint, bool termAttr);
extern int getCFA(const char *prompt, uint selector, UnivPtr *pUniv, const char *errorMsg);
extern int getCF(const char *prompt, UnivPtr *pUniv, uint selector);
extern uint getFlagOpts(Option *pOpt);
extern int getInfo(Datum *pRtnVal, int n, Datum **args);
extern long getLineNum(Buffer *pBuf, Line *pTargLine);
extern int getLineRegion(int n, Region *pRegion, ushort flags);
extern int getModes(Datum *pRtnVal, Buffer *pBuf);
extern int getNArg(Datum *pRtnVal, const char *prompt);
extern int getRegion(Region *pRegion, ushort flags);
extern int getRingName(Ring **ppRing);
extern int getTermSize(ushort *col, ushort *row);
extern void getTextRegion(Point *pPoint, int n, Region *pRegion, ushort flags);
extern int getWindCount(EScreen *pScrn, int *pWindNum);
extern int getWindId(ushort *pWindId);
extern int getWindNum(EWindow *pWind);
extern int getWord(Datum *pRtnVal, int n, Datum **args);
extern int getWindPos(EWindow *pWind);
extern int goLine(Datum *pDatum, int n, int line);
extern int gotoFence(Datum *pRtnVal, int n, Datum **args);
extern int gotoLine(Datum *pRtnVal, int n, Datum **args);
extern int gotoMark(Datum *pRtnVal, int n, Datum **args);
extern int gotoScreen(int n, ushort flags);
extern int gotoWind(Datum *pRtnVal, int n, ushort flags);
extern int groupModeQ(Datum *pRtnVal, int n, Datum **args);
extern bool haveColor(void);
extern int help(Datum *pRtnVal, int n, Datum **args);
extern const char *hookFuncName(UnivPtr *pUniv);
extern int indentRegion(Datum *pRtnVal, int n, Datum **args);
extern char *inflate(char *dest, const char *src, bool plural);
extern void initBoolOpts(Option *pOpt);
extern void initCharTables(void);
extern void initInfoColors(void);
extern int insertBuf(Datum *pRtnVal, int n, Datum **args);
extern int inserti(Datum *pRtnVal, int n, Datum **args);
extern int insertNLSpace(int n, ushort flags);
extern bool interactive(void);
extern bool inWord(void);
extern int iorStr(const char *src, int n, ushort style, Ring *pRing);
extern int iorText(const char *src, int n, ushort style, Buffer *pBuf);
extern bool isAnyBufModeSet(Buffer *pBuf, bool clear, int modeCount, ...);
extern bool isBufModeSet(Buffer *pBuf, ModeSpec *pModeSpec);
extern bool is_digit(short c);
extern bool isLowerCase(short c);
extern bool isUpperCase(short c);
extern bool isWhite(Line *pLine, int length);
extern bool is_letter(short c);
extern int joinLines(Datum *pRtnVal, int n, Datum **args);
extern int joinWind(Datum *pRtnVal, int n, Datum **args);
extern int kdcBackWord(int n, int kdc);
extern bool kdcFencedRegion(int kdc);
extern int kdcForwWord(int n, int kdc);
extern int kdcLine(int n, int kdc);
extern int kdcText(int n, int kdc, Region *pRegion);
extern int killPrep(int kill);
extern int lalloc(int used, Line **ppLine);
#define libpanic()	(cxlExcep.flags & ExcepMem)
extern int librsset(int status);
extern void llink(Line *pLine1, Buffer *pBuf, Line *pLine2);
extern bool lineInWind(EWindow *pWind, Line *pLine);
extern void lreplace1(Line *pLine1, Buffer *pBuf, Line *pLine2);
extern void lunlink(Line *pLine, Buffer *pBuf);
extern int makeArray(Datum *pRtnVal, ArraySize len, Array **ppAry);
extern char *makeLower(char *dest, const char *src);
extern char *makeUpper(char *dest, const char *src);
extern int markBuf(Datum *pRtnVal, int n, Datum **args);
extern int mcreate(const char *name, ssize_t index, const char *desc, ushort flags, ModeSpec **ppModeSpec);
extern int message(Datum *pRtnVal, int n, Datum **args);
extern int mgcreate(const char *name, ModeGrp *prior, const char *desc, ModeGrp **result);
extern bool mgsrch(const char *name, ModeGrp **slot, ModeGrp **ppModeGrp);
extern void mleraseEOL(void);
extern int mlerase(ushort flags);
extern int mlflush(void);
extern int mlmove(int col);
extern int mlprintf(ushort flags, const char *fmt, ...);
extern void mlputc(ushort flags, short c);
extern int mlputs(ushort flags, const char *msg);
extern int mlrestore(void);
extern int modeQ(Datum *pRtnVal, int n, Datum **args);
extern bool modeSet(ushort id, Buffer *pBuf);
extern int moveChar(int n);
extern int moveLine(int n);
extern void movePoint(Point *pPoint);
extern int moveWord(int n);
extern int moveWindUp(Datum *pRtnVal, int n, Datum **args);
extern ModeSpec *msrch(const char *name, ssize_t *index);
extern int narrowBuf(Datum *pRtnVal, int n, Datum **args);
extern int newCol(short c, int col);
extern int newColTermAttr(Point *pPoint, int col, int *attrLen);
extern int newlineI(Datum *pRtnVal, int n, Datum **args);
extern int nextWind(Datum *pRtnVal, int n, Datum **args);
extern int nukeWhite(int n, bool prior);
extern int onlyWind(Datum *pRtnVal, int n, Datum **args);
extern int openLine(Datum *pRtnVal, int n, Datum **args);
extern int openLineI(Datum *pRtnVal, int n, Datum **args);
extern int otherFence(short fence, Region *pRegion);
extern int outdentRegion(Datum *pRtnVal, int n, Datum **args);
extern int overPrep(int n);
extern char *pad(char *s, uint len);
extern int parseOpts(OptHdr *pOptHdr, const char *basePrompt, Datum *pOptions, int *count);
extern int pathSearch(const char *name, ushort flags, void *result, const char *funcName);
extern int pipeCmd(Datum *pRtnVal, int n, const char *prompt, ushort flags);
extern int prevNextBuf(Datum *pRtnVal, ushort flags);
extern int prevWind(Datum *pRtnVal, int n, Datum **args);
extern bool pointInWind(void);
extern char *putAttrStr(const char *str, const char *strEnd, ushort *flags, void *targ);
extern int quit(Datum *pRtnVal, int n, Datum **args);
extern int quoteChar(Datum *pRtnVal, int n, Datum **args);
extern int rcycle(Ring *pRing, int n, bool msg);
extern int rdelete(Ring *pRing, int n);
extern int regionToKill(Region *pRegion);
extern char *regionToStr(char *buf, Region *pRegion);
extern int regionLines(int *pLineCount);
extern int renameBuf(Datum *pRtnVal, int n, Datum **args);
extern int render(Datum *pRtnVal, int n, Buffer *pBuf, ushort flags);
extern int resetTerm(Datum *pRtnVal, int n, Datum **args);
extern int resizeWind(Datum *pRtnVal, int n, Datum **args);
extern RingEntry *rget(Ring *pRing, int n);
extern int ringSize(Datum *pRtnVal, int n, Datum **args);
extern int rsave(Ring *pRing, const char *value, bool force);
extern void rsclear(ushort flags);
extern void rspop(short oldStatus, ushort oldFlags);
extern void rspush(short *pStatus, ushort *pFlags);
extern int rssave(void);
extern int rsset(int status, ushort flags, const char *fmt, ...);
extern void rsunfail(void);
extern void rtop(Ring *pRing, RingEntry *pEntry);
extern int runBufHook(Datum **ppRtnVal, ushort flags);
extern int runCmd(Datum *pRtnVal, const char *cmdPrefix, const char *arg, const char *cmdSuffix, ushort flags);
extern int scratchBuf(Datum *pRtnVal, int n, Datum **args);
extern int scrnCount(void);
extern int selectBuf(Datum *pRtnVal, int n, Datum **args);
extern int selectLine(Datum *pRtnVal, int n, Datum **args);
extern int selectScreen(Datum *pRtnVal, int n, Datum **args);
extern int selectWind(Datum *pRtnVal, int n, Datum **args);
extern int sepLine(int len, DFab *pFab);
extern void setBoolOpts(Option *pOpt);
extern int setBufMode(Buffer *pBuf, ModeSpec *pModeSpec, bool clear, bool *pWasSet);
extern int setColorPair(Datum *pRtnVal, int n, Datum **args);
extern int setDefault(Datum *pRtnVal, int n, Datum **args);
extern int setDispColor(Datum *pRtnVal, int n, Datum **args);
extern int setExecPath(const char *path);
extern int setFilename(Buffer *pBuf, const char *filename, ushort flags);
extern void setGlobalMode(ModeSpec *pModeSpec);
extern int seti(Datum *pRtnVal, int n, Datum **args);
extern int setMark(Datum *pRtnVal, int n, Datum **args);
extern int setPointCol(int pos);
extern void setTermSize(ushort cols, ushort rows);
extern void setWindMark(Mark *pMark, EWindow *pWind);
extern int setWorkDir(EScreen *pScrn);
extern int sfind(ushort scrNum, Buffer *pBuf, EScreen **ppScrn);
extern int shellCLI(Datum *pRtnVal, int n, Datum **args);
extern int showBuffers(Datum *pRtnVal, int n, Datum **args);
extern int showColors(Datum *pRtnVal, int n, Datum **args);
extern int showFence(Datum *pRtnVal, int n, Datum **args);
extern int showMarks(Datum *pRtnVal, int n, Datum **args);
extern int showModes(Datum *pRtnVal, int n, Datum **args);
extern int showPoint(Datum *pRtnVal, int n, Datum **args);
extern int showRing(Datum *pRtnVal, int n, Datum **args);
extern int showScreens(Datum *pRtnVal, int n, Datum **args);
extern int sortRegion(Datum *pRtnVal, int n, Datum **args);
extern int spanWhite(bool end);
extern int sswitch(EScreen *pScrn, ushort flags);
extern char *strparse(char **pStr, short delimChar);
extern char *strrev(char *s);
extern char *strSamp(const char *src, size_t srcLen, size_t maxLen);
extern void supd_windFlags(Buffer *pBuf, ushort flags);
extern int suspendMM(Datum *pRtnVal, int n, Datum **args);
extern int swapMark(Datum *pRtnVal, int n, Datum **args);
extern int swapMarkId(ushort id);
extern int sysBuf(const char *root, Buffer **ppBuf, ushort flags);
extern int sysCallError(const char *caller, const char *call, bool addTERM);
extern int tabStop(int n);
extern int termInp(Datum *pRtnVal, const char *basePrompt, uint argFlags, uint ctrlFlags, TermInpCtrl *pTermInp);
extern int terminpYN(const char *basePrompt, bool *result);
extern char *strTime(void);
extern int titleCaseStr(Datum *pRtnVal, int n, Datum **args);
extern int toString(Datum *pRtnVal, int n, Datum **args);
extern int traverseLine(Datum *pRtnVal, int n, Datum **args);
extern int trimLine(Datum *pRtnVal, int n, Datum **args);
extern void tbeep(void);
extern void tbold(bool msgLine, bool attrOn);
extern int tclose(bool saveTTY);
extern int tgetc(bool msgLine, ushort *pChar);
extern int tflush(void);
extern int tmove(int row, int col);
extern int topen(void);
extern void trev(bool msgLine, bool attrOn);
extern void tul(bool msgLine, bool attrOn);
extern int typahead(int *pCount);
extern void ungetkey(ushort extKey);
extern int update(int n);
extern int updateScrn(Datum *pRtnVal, int n, Datum **args);
extern long urand(long upperBound);
extern int userPrompt(Datum *pRtnVal, int n, Datum **args);
extern bool validMark(Datum *pDatum);
extern int vtinit(void);
extern void windFaceToBufFace(EWindow *pWind, Buffer *pBuf);
extern EWindow *windDispBuf(Buffer *pBuf, bool skipCur);
extern int widenBuf(Datum *pRtnVal, int n, Datum **args);
extern bool windNewTop(EWindow *pWind, Line *pLine, int n);
extern EWindow *windNextIs(EWindow *pWind);
extern int wrapLine(Datum *pRtnVal, int n, Datum **args);
extern int wrapWord(Datum *pRtnVal, int n, Datum **args);
extern int writeBuf(Datum *pRtnVal, int n, Datum **args);
extern int wscroll(Datum *pRtnVal, int n, int (*windFunc)(Datum *pRtnVal, int n, Datum **args),
 int (*pageFunc)(Datum *pRtnVal, int n, Datum **args));
extern int wsplit(int n, EWindow **ppWind);
extern int wswitch(EWindow *pWind, ushort flags);
extern int wupd_reframe(EWindow *pWind);
extern int ycycle(Datum *pRtnVal, int n, Ring *pRing, ushort flag, ushort cmd);
extern int yrevert(ushort flags);

#ifdef MainData

// **** For main.c ****

// Forward.
CmdFunc cmdFuncTable[];

// Global variables.
Option bufAttrTable[] = {		// Buffer attribute table.
	{"Active", NULL, OptIgnore, BFActive},
	{"^Changed", "^Chg", 0, BFChanged},
	{"Command", NULL, OptIgnore, BFCommand},
	{"Function", NULL, OptIgnore, BFFunc},
	{"^Hidden", "^Hid", 0, BFHidden},
	{"Narrowed", NULL, OptIgnore, BFNarrowed},
	{"^ReadOnly", "^RdO", 0, BFReadOnly},
	{"^TermAttr", "^TAttr", 0, BFTermAttr},
	{NULL, NULL, 0, 0}};
char buffer1[] = Buffer1;		// Name of first buffer ("unnamed").
Array bufTable;				// Buffer table (array).
char *Copyright = "(c) Copyright 2022 Richard W. Marinelli";
Macro curMacro = {
	.size = 0, NULL, NULL, NULL, MacStop, 0};	// Macro control variables.
Option defOptions[] = {
	{"HardTabSize", NULL, 0, 0},
	{"SoftTabSize", NULL, 0, 0},
	{"WrapCol", NULL, 0, 0},
	{NULL, NULL, 0, 0}};
int defParams[] = {-1, -1, -1};		// Default screen parameters.
HookRec hookTable[] = {			// Hook table.
	{"chgDir", HLitN_chgDir, HLitArg_none, 0, false, {PtrNull, NULL}},
	{"createBuf", HLitN_defn, HLitArg_createBuf, 1, false, {PtrNull, NULL}},
	{"enterBuf", HLitN_defn, HLitArg_enterBuf, 1, false, {PtrNull, NULL}},
	{"exit", HLitN_exit, HLitArg_none, 0, false, {PtrNull, NULL}},
	{"exitBuf", HLitN_defn, HLitArg_none, 0, false, {PtrNull, NULL}},
	{"filename", HLitN_defn, HLitArg_filename, 2, false, {PtrNull, NULL}},
	{"mode", HLitN_defn, HLitArg_mode, 2, false, {PtrNull, NULL}},
	{"postKey", HLitN_postKey, HLitArg_postKey, 1, false, {PtrNull, NULL}},
	{"preKey", HLitN_preKey, HLitArg_none, 0, false, {PtrNull, NULL}},
	{"read", HLitN_defn, HLitArg_read, 2, false, {PtrNull, NULL}},
	{"wrap", HLitN_defn, HLitArg_none, 0, false, {PtrSysFunc, cmdFuncTable + cf_wrapWord}},
	{"write", HLitN_defn, HLitArg_write, 2, false, {PtrNull, NULL}},
	{NULL, NULL, NULL, 0, false, {PtrNull, NULL}}
	};
IVar iVar = {1, 1};			// "i" variable.
#if MMDebug
FILE *logfile;				// Log file for debugging.
#endif
char lowCase[256];			// Upper-to-lower translation table.
char macroDelims[] = MacroDelims;	// Possible macro delimiters (not allowed in the name).
ModeInfo modeInfo;			// Mode information record.
char Myself[] = ProgName;		// Common name of program.
Ring ringTable[] = {
	{NULL, 0, DelRingSize, NULL, NULL},
	{NULL, 0, KillRingSize, NULL, ""},
	{NULL, 0, MacroRingSize, NULL, ""},
	{NULL, 0, ReplRingSize, NULL, NULL},
	{NULL, 0, SearchRingSize, NULL, NULL}};
ushort ringTableSize = elementsof(ringTable);
SampBuf sampBuf;			// "Sample" string buffer.
uint scratchBufNum = 0;			// Unique suffix number for scratch buffers.
SessionCtrl sess = {			// Session parameters.
	{NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL}, NULL, FencePause, AutoSaveTrig, AutoSaveTrig, 0,
	OpEval | OpStartup | OpScrnRedraw, PageOverlap, NULL, NULL, NULL, NULL, NULL, TravJump, 0, {Success, 0}, {Success, 0}
	};
ETerm term = {
	TTY_MaxCols,			// Maximum number of columns.
	0,				// Current number of columns.
	TTY_MaxRows,			// Maximum number of rows.
	0,				// Current number of rows.
	MaxPromptPct,			// Maximum percentage of terminal width for prompt string (in range 15-90).
	INT_MAX,			// Current cursor column on message line + 1 (which may be equal to terminal width).
	NULL,				// ncurses window for message line.
	0,				// Maximum color number available.
	0,				// Maximum color pair number available.
	0,				// Maximum non-reserved color pair number available for general use (1..maxWorkPair).
	1,				// Next pair number returned by setColorPair function (1..maxWorkPair).
	0,				// Lines (colors) per page for showColors display.
	-2,				// Best white color number.
	-2,				// Best red color number.
	-2,				// Best green color number.
	{{"Info", -2, -2},		// Color pair for informational displays.
	{"ModeLine", -2, -2},		// Color pair for mode lines.
	{"Record", -2, -2}}		// Color pair for macro recording indicator.
	};
char upCase[256];			// Lower-to-upper translation table.
char Version[] = ProgVer;		// MightEMacs version.
char viz_false[] = "false";		// Visual form of false.
char viz_nil[] = "nil";			// Visual form of nil.
char viz_true[] = "true";		// Visual form of true.
VTermCtrl vTerm = {
	HorzJump,			// Horizontal jump size - percentage.
	1,				// Horizontal jump size - columns.
	VertJump,			// Vertical jump size - percentage.
	NULL				// Value of TERM environmental variable.
	};

#else

// **** For all the other .c files ****

// External declarations for global variables defined above, with a few exceptions noted.
extern Option bufAttrTable[];
extern char buffer1[];
extern Array bufTable;
extern Macro curMacro;
extern Option defOptions[];
extern int defParams[];
extern HookRec hookTable[];
extern IVar iVar;
#if MMDebug
extern FILE *logfile;
#endif
extern char lowCase[];
extern char macroDelims[];
extern ModeInfo modeInfo;
extern char Myself[];
extern Ring ringTable[];
extern ushort ringTableSize;
extern SampBuf sampBuf;
extern uint scratchBufNum;
extern SessionCtrl sess;
extern ETerm term;
extern char upCase[];
extern char Version[];
extern char viz_false[];
extern char viz_nil[];
extern char viz_true[];
extern VTermCtrl vTerm;
#endif
