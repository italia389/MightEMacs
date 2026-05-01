// (c) Copyright 2026 Richard W. Marinelli
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

// Other standard include files.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

// Flags for new features and proposed changes.

// Program-logic (source code) debugging flags.
#define SanityCheck	0		// Include "sanity check" code (things that should never happen).

#define Debug_Logfile	0x00000001	// Open file Logfile for debugging output.
#define Debug_ArrayHash	0x00000002	// Debug array and hash usage.
#define Debug_AryHshBuf	0x00000004	// Write array and hash memory-management info to a buffer.
#define Debug_AryHshLog	0x00000008	// Write array and hash memory-management info to log file..
#define Debug_Bind	0x00000010	// Dump binding table.
#define Debug_CallArg	0x00000020	// Dump user-routine-argument info to log file.
#define Debug_Datum	0x00000040	// Dump Datum processing info to log file.
#define Debug_Expr	0x00000080	// Write (verbose) expression-parsing info to log file.
#define Debug_GarbInfo	0x00000100	// Write garbage collection info to log file.
#define Debug_KeyCode	0x00000200	// Check ncurses key codes and extended keys in binding table.
#define Debug_ModeDump	0x00000400	// Dump buffer modes or mode table.
#define Debug_Modes	0x00000800	// Write mode-processing info to log file.
#define Debug_MsgLnInp	0x00001000	// Write message-line input info to log file.
#define Debug_MsgLnRtn	0x00002000	// Write return message and message-line output info to log file.
#define Debug_NCurses	0x00004000	// Debug ncurses routines.
#define Debug_Narrow	0x00008000	// Dump buffer info to log file in narrowBuf().
#define Debug_PipeCmd	0x00010000	// Debug shell pipes and commands.
#define Debug_Preproc	0x00020000	// Dump script preprocessor blocks to log file and exit.
#define Debug_Regexp	0x00040000	// Debug regular expressions.
#define Debug_RingDump	0x00080000	// Include dumpRing() function.
#define Debug_SanityChk	0x00100000	// Do sanity checks at beginning of main() and exit.
#define Debug_Script	0x00200000	// Write script lines to log file.
#define Debug_ScrnDump	0x00400000	// Dump screens, windows, and buffers.
#define Debug_ShowRE	0x00800000	// Include showRegexp command.
#define Debug_SrchRepl	0x01000000	// Debug search and replace routines.
#define Debug_Token	0x02000000	// Dump token-parsing info to log file.
#define Debug_UserVars	0x04000000	// Write user variable creation and deletion info to log file.
#define Debug_VTerm	0x08000000	// Write screen-update details to message line.
#define Debug_WindCount	0x10000000	// Display buffer's window count in "showBuffers" display.
#define Debug_Wrap	0x20000000	// Write line-wrapping info to log file.
#define Debug_Temp	0x80000000	// For ad-hoc use.

// Debug settings.  If MMDebug not zero, debug log is written to file Logfile (defined below) in current directory.
#define MMDebug		0		// No debugging code.
//#define MMDebug		Debug_Logfile
//#define MMDebug		(Debug_Logfile | Debug_SanityChk)
//#define MMDebug		(Debug_SrchRepl | Debug_Logfile)
//#define MMDebug		(Debug_NCurses | Debug_Logfile)
//#define MMDebug		(Debug_KeyCode | Debug_Logfile)
//#define MMDebug		(Debug_PipeCmd | Debug_Logfile)
//#define MMDebug		(Debug_MsgLnInp | Debug_Logfile)
//#define MMDebug		(Debug_MsgLnRtn | Debug_Logfile)
//#define MMDebug		(Debug_ModeDump | Debug_Logfile)
//#define MMDebug		(Debug_Modes | Debug_ModeDump | Debug_Logfile)
//#define MMDebug		(Debug_Narrow | Debug_Logfile)
//#define MMDebug		(Debug_AryHshLog | Debug_Logfile)
//#define MMDebug		(Debug_AryHshLog | Debug_Datum | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_ArrayHash | Debug_UserVars | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Token | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Expr | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Token | Debug_Expr | Debug_Logfile)
//#define MMDebug		(Debug_Script | Debug_Datum | Debug_Token | Debug_Expr | Debug_Logfile)
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
#define ProgVer		"10.0.0"

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
#define BackupExt	".bak"		// Backup file extension.
#define ScriptExt	".ms"		// Script file extension.
#define UserStartup	".memacs"	// User start-up file (in HOME directory).
#define SiteStartup	"memacs.ms"	// Site start-up file.
#define MMPath_Name	"MSPATH"	// Name of shell environmental variable containing custom search path.
#define MMPath ":/usr/local/share/memacs/scripts"
					// Standard search path for script files.
#define FuzzySearch	0		// Include code for "fuzzy" searching via XRE library.

// Limits -- [Set any].
#define LineBlockSize	32		// Number of bytes, line block chunks.
#define KillRingSize	50		// Default number of entries in kill ring.
#define DelRingSize	50		// Default number of entries in delete ring.
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
#define HashGrowPct	35		// Amount of extra space to configure for "{...}" hash in a script (percentage).

/***** END CUSTOMIZATIONS *****/

// OS identification.
#if MACOS || LINUX
#	define OSName_CentOS	"CentOS Linux"
#	define OSName_MacOS	"macOS"
#	define OSName_RedHat	"Red Hat Linux"
#	define OSName_Ubuntu	"Ubuntu Linux"
#	define VerKey_MacOS	"darwin"
#	define VerKey_Ubuntu	"ubuntu"
#	define CentOS_Release	"/etc/centos-release"
#	define RedHat_Release	"/etc/redhat-release"
#else
#	define OSName	"Unix"
#endif

// Miscellaneous.
#define Logfile		"memacs.log"	// Log file for debugging (overwritten).
#define Scratch		"scratch"	// Prefix for scratch buffers.
#define Buffer1		"unnamed"	// First buffer created.
#define BufTableSize	67		// Initialize hash size of buffer table.
#define MacroDelim	' '		// Macro encoding delimiter.
#define ModeTableSize	53		// Initialize hash size of mode table.
#define Plural		0x80000000	// Use plural of word.  May be combined with other flags so hit bit is used.
#define Tag		0x00000001	// Used for garbage collection.
#define WordChars	"A-Za-z0-9_"	// Characters in a word.

// Internal constants.
#define DefN		LONG_MIN	// Default n argument.
#define MaxExecPause	600		// Maximum number of seconds to pause during script execution ("pause" function).
#define MaxFencePause	1000		// Maximum number of centiseconds to pause for fence matching.
#define MaxRingSize	USHRT_MAX	// Maximum ring size.
#define PatSizeMin	32		// Minimum array size for search or replacement pattern.
#define PatSizeMax	96		// Maximum array size to retain for search or replacement pattern.
#define MacroChunkSize	16		// Extension size of macro buffer when full.
#define WorkBufSize	80		// Number of bytes, work buffer.
#define RegionMark	'.'		// Region mark, which defines region end point.
#define RegionMarkStr	"~u.~U"
#define WorkMark	'`'		// Work mark, used to save point position by certain commands.
#define WorkMarkStr	"~u`~U"

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
#define OpScrnRedraw	0x0200		// Clear and redraw screen (in update() function).
#define OpUserCmd	0x0400		// User command being executed (interactive mode).

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

// Line shift flags (for indenting and outdenting).
#define ShLineSelect	0x0001		// n argument is line block selector; otherwise, number of tab stops.
#define ShRegion	0x0002		// Operate on region lines, otherwise line block.
#define ShOutdent	0x0004		// Outdent line block, otherwise, indent.
#define ShHoldPoint	0x0008		// Do not move point.

// Buffer operation flags used by bufOp() function.
#define BM_BeginEnd	1		// Move to beginning or end of buffer.
#define BM_GotoLine	2		// Go to to a line in the buffer.
#define BM_ReadBuf	3		// Read line(s) from a buffer.

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

// Function return status codes in order of severity (highest to lowest).  Note that Cancelled, NotFound, IONSF, and IOEOF are
// never actually set via rsset() (so sess.rtn.status will never be one of those codes); whereas, all other status codes are
// always set, either explicitly or implicitly.
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

// Toggle-able values for routines that need a direction.
#define Forward		0		// Do things in a forward direction.
#define Backward	1		// Do things in a backward direction.

#define LongWidth	sizeof(long) * 3

// Return status information from a command or function.
typedef struct {
	short status;			// Most severe status returned from any C function.
	ushort flags;			// Status flags.
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

// Text insertion style.
#define Txt_Insert	0x0001
#define Txt_Overwrite	0x0002
#define Txt_Replace	0x0004
#define Txt_LiteralNL	0x0008		// Literal newline character.

// Flags used by runCmdline().
#define RunQWrap	0x0001		// Basic quoting: wrap double quotes around argument.
#define RunQFull	0x0002		// Full quoting: call dputs() with CvtLang flag to quote argument.

// Flags used by pipeCmd().
#define PipeWrite	0x0001		// Write target buffer to pipe1 (pipeBuf).
#define PipePopOnly	0x0002		// Pop command results only (shellCmd).
#define PipeInsert	0x0004		// Insert pipe output into target buffer; otherwise, replace its contents (readPipe).

// Help types.
enum h_type {h_copyright, h_version, h_usage, h_help};

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
	const char *abbr;		// Abbreviation or NULL (use keyword).
	uint ctrlFlags;			// Control flags.
	union {				// Option value.
		int svalue;		// Signed integer value.
		uint value;		// Unsigned integer value (flag).
		} u;
	} Option;

// Option control flags.
#define OptIgnore	0x0001		// Option ignored by parseOpts().
#define OptFalse	0x0002		// Treat option as negated if selected.
#define OptSelected	0x0004		// Option was selected.

// Option header object used by parseOpts() function to parse option keywords in script mode or build a custom prompt and get
// option letters interactively.  The function is also called by execCmdFunc() to perform "automatic" options processing if a
// OptHdr pointer is in the cmd-func table for a particular routine.  In that case, the following rules apply:
//   1. If argNum < 0, the options argument must be the "maxArgs" (last) one for the command or function; otherwise, if argNum
//	== 0, the options argument must be first argument and funcArg() is called to obtain it; otherwise (argNum > 0), the
//	options argument is the argNum argument; that is, args[argNum - 1].
//   2. Options are processed in script mode if argNum is zero, or if maxArgs or argNum > 0 arguments exist (as explained
//	above), or interactively if n is not the default.
//   3. If OptHdrShared flag is set, the OptIgnore flag is cleared in each option in optTable before processing the options
//	argument, except if an option's value (flag) is set in OptHdr.ctrlFlags, in which case the OptIgnore flag is set (and
//	that option is ignored).
//   4. If options are processed successfully, all the option's value flags that were selected are OR'd together and stored in
//	OptHdr.selFlags.
typedef struct {
	Option *optTable;		// Option table.
	const char *prompt;		// Interactive prompt.
	const char *type;		// Brief description of option type, for error reporting.
	ushort optCount;		// Number of options in table.
	short argNum;			// Options argument number (0..n), or -1 for default (last).
	uint selFlags;			// Flags selected at runtime.
	uint ctrlFlags;			// OptHdr flags plus option flags to ignore (if OptHdrShared flag set).
	} OptHdr;

// OptHdr control flags.  May be combined with CXL Op* flags and others so highest bits are used.
#define OHdrFunc	0x00400000	// Getting options for system function, otherwise command.
#define OHdrFirst	0x00800000	// Use ArgFirst flag when getting options argument in script mode.
#define OHdrNil		0x01000000	// Use ArgNil1 flag when getting options argument in script mode.
#define OHdrSingle	0x02000000	// Single selection only (options in list are mutually exclusive).
#define OHdrShared	0x04000000	// Options used by more than one routine.
#define OHdrPrep	0x08000000	// Call preprocessor to customize controls at runtime.
#define OHdrBuild	0x10000000	// Call routine to build prompt.
#define OHdrNoSel	0x20000000	// OptHdr.selFlags not used so skip getFlagOpts() call.
#define OHdrSpecial	0x40000000	// Special flag that may be checked, set, and/or cleared by custom code for command.
#define OHdrFound	0x80000000	// One or more options found by parseOpts() function.

// The editor communicates with the terminal using an API (the ncurses library).  The ETerm structure holds parameters needed
// for terminal management.
typedef struct {
	int cols;			// Current number of columns used.
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

// Operation types.  The first four of these are mutually exclusive, however they are coded as bit masks so they can be combined
// with other flags.
#define OpQuery		0x0001		// Find an item.
#define OpCreate	0x0002		// Create an item.
#define OpUpdate	0x0004		// Update an item.
#define OpDelete	0x0008		// Delete an item.
#define OpConfirm	0x0010		// Get user confirmation for item creation.
#define OpCurBName	0x0020		// Use current buffer name as default in getBufname() function.
#define OpDefBName	0x0040		// Generate default buffer name in getBufname() function.
#define OpHaveBName	0x0080		// Passing buffer name in first argument when calling getBufname() function.
#define OpAlt		0x0100		// Perform alternate operation.

// The editor holds text chunks in a double-linked list of buffers (e.g., the kill ring and search ring).  Chunks are stored in
// a Datum object so that text (of unpredictable size) can be saved iteratively and bidirectionally.  This can also be used in
// the future for other data types.
typedef struct RingEntry {
	struct RingEntry *prev;		// Pointer to previous item in ring.
	struct RingEntry *next;		// Pointer to next item in ring.
	Datum data;			// Data item (text chunk).
	} RingEntry;

typedef struct {
	RingEntry *pEntry;		// Current ring item.
	ushort size;			// Number of items in ring.
	ushort maxSize;			// Maximum size of ring.
	ushort flags;			// Ring ID and type.
	const char *ringName;		// Name of ring.
	const char *entryName;		// Name of ring entry/object (for notifications).
	} Ring;

// Ring indices and flags.
#define RingIdxDel	0
#define RingIdxKill	1
#define RingIdxRepl	2
#define RingIdxSearch	3

#define RingKD		0x0010		// Kill or delete ring.
#define RingIDMask	0x000f

// Ordered hash flags.
#define OH_Changed	0x0001		// Node was added or deleted from hash.

// Descriptor for an "ordered hash" object, a hybrid that contains a hash that uses names as keys (like a buffer name), and a
// sorted array of node pointers also so that the names can be accessed in ascending order when needed.  To reduce overhead, the
// array is recreated only when the "changed" flag is set and the array needs to be used.  Used for buffer and mode tables.
typedef struct {
	ushort flags;
	HashNode **pAry;
	Hash table;
	} OrdHash;

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
	char *name;			// Name of mode in camel case.
	} ModeSpec;

// Macro for fetching ModeSpec pointer from a Datum object, given pointer.
#define modePtr(pDatum)	((ModeSpec *) dgetmemptr((pDatum)))

// Mode attribute and state flags.
#define MdUser		0x0001		// User defined.  Mode or group may be deleted if set.
#define MdGlobal	0x0002		// Global mode if set, otherwise buffer mode.  Cannot be changed if MdLocked set.
#define MdLocked	0x0004		// Scope cannot be changed if set (certain built-in modes).
#define MdHidden	0x0010		// Don't display on mode line if set.
#define MdInLine	0x0020		// Adds custom text to mode line (so update needed when changed).
#define MdActive	0x0040		// Global mode is active if set, otherwise N/A.

// Cache indices for built-in modes.
#define MdIdxASave	0
#define MdIdxATerm	1
#define MdIdxBak	2
#define MdIdxBufCount	3
#define MdIdxClob	4
#define MdIdxCol	5
#define MdIdxExact	6
#define MdIdxFence1	7
#define MdIdxFence2	8
#define MdIdxHScrl	9
#define MdIdxLine	10
#define MdIdxOver	11
#define MdIdxReadOnly	12
#define MdIdxRegexp	13
#define MdIdxRepl	14
#define MdIdxRtnMsg	15
#define MdIdxSafe	16
#define MdIdxWkDir	17
#define MdIdxWrap	18
#define NumModes	19		// Length of mode cache array.

// Mode information.
typedef struct {
	OrdHash idx;			// Mode table: contains hash of Datum objects holding pointers to ModeSpec records.
	ModeSpec *cache[NumModes];	// Cache for built-in modes.
	ModeGrp *grpHead;		// Mode group table: linked list of ModeGrp records.
	} ModeHdr;

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
	ushort wflags;			// Window update flags.
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
	ushort topRow;			// Origin 0 screen row number of top row of window.
	ushort rows;			// Number of rows in window, excluding mode line.
	short reframeRow;		// Target (reframe) row in window for line containing point.
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
#define SWB_RepeatCount	0x0001		// n argument is a repeat count.
#define SWB_Forw	0x0002		// Repeat forward, otherwise backward.
#define SWB_ExitHook	0x0004		// Run "exitBuf" hook, otherwise "enterBuf".
#define SWB_NoBufHooks	0x0008		// Don't run "exitBuf" or "enterBuf" hook.

// Buffer mark and flags.
typedef struct Mark {
	struct Mark *next;		// Next mark.
	ushort id;			// Mark identifier.
	short reframeRow;		// Target (reframe) row in window for point.
	Point point;			// Point position.
	} Mark;

// Mark operation flags.
#define MKAutoR		0x0001		// Use mark RegionMark if default n; otherwise, prompt with no default (setMark).
#define MKAutoW		0x0002		// Use mark WorkMark if default n; otherwise, prompt with no default (markBuf).
#define MKViz		0x0004		// Mark must exist and be visible (gotoMark, swapMark).
#define MKExist		0x0008		// Mark must exist (delMark).
#define MKCreate	0x0010		// Mark does not have to exist (setMark, markBuf).
#define MKQuery		0x0020		// Query mode.
#define MKWind		0x0040		// Mark is a window id.
#define MKStatic	0x0080		// Mark is a static object - don't allocate memory (queryReplace, replace).

// Additional information for a script buffer.
typedef struct {
	struct LoopBlock *execBlocks;	// Pointer to compiled loop blocks.
	short minArgs;			// Declared minimum argument count.
	short maxArgs;			// Declared maximum argument count (-1 if variable).
	ushort execCount;		// Count of active executions.
	Datum argSyntax;		// User routine argument syntax help, if any.
	Datum descrip;			// User routine description help, if any.
	} CallHdr;

// Buffer mode record.
typedef struct BufMode {
	struct BufMode *next;		// Next mode in linked list (or NULL if none).
	ModeSpec *pModeSpec;		// Pointer to (enabled) mode in mode table.
	} BufMode;

// Text is kept in buffers.  A buffer header, described below, exists for every buffer in the system.  Buffers are kept in a
// hash to provide quick lookups by name.  Additionally, an "on-demand" sorted list of name pointers is maintained for quick
// access to the next or previous buffer, buffer completions, and creating the "showBuffers" listing.  There is a safe store for
// a window's top line, point, and first display column (in the Face object) so that windows can be cleanly restored to the same
// point in a buffer (if possible) when switching among them.  windCount is the number of windows (on any screen) that a buffer
// is associated with.  Thus if the value is zero, the buffer is not being displayed anywhere.  The text for a buffer is kept in
// a circularly linked list of lines with a pointer to the first line in pFirstLine.  When a buffer is first created or cleared,
// the first (and only) line will point to itself, both forward and backward.  The line will also be empty indicating that the
// buffer is empty.  A buffer will always have a minimum of one line associated with it at all times.  All lines except the last
// have an implicit newline delimiter; however, the last line never does.  Hence, files which do not have a newline as their
// last character can be handled properly.  A buffer may be "inactive" which means the file associated with it has not been read
// in yet.  The file is read at "use buffer" time.
typedef struct Buffer {
	Face face;			// Point position, etc. from last detached window.
	struct Line *pFirstLine;	// Pointer to first line.
	struct Line *pNarTopLine;	// Pointer to narrowed top text.
	struct Line *pNarBotLine;	// Pointer to narrowed bottom text.
	Mark markHdr;			// Mark list header.
	CallHdr *pCallHdr;		// Pointer to user command/function parameters, if applicable.
	const char *homeDir;		// Buffer's "home" directory (absolute pathname in directory table).
	ushort windCount;		// Count of windows displaying buffer.
	ushort aliasCount;		// Count of aliases pointing to this user command/function buffer.
	ushort flags;			// Flags.
	BufMode *modes;			// Enabled buffer modes (linked list of BufMode records), initially NULL.
	ushort inpDelimSym;		// Symbolic name of line delimiter used to read buffer.
	char *filename;			// Filename (on heap).
	char *bufname;			// Buffer name.
	} Buffer;

// Macro for fetching Buffer pointer from Datum object in hash node, given Datum pointer.
#define bufPtr(pDatum)	((Buffer *) dgetmemptr((pDatum)))

// Buffer flags and masks.
#define BFActive	0x0001		// Active buffer (file was read).
#define BFChanged	0x0002		// Changed since last write.
#define BFCommand	0x0004		// Buffer is a user command.
#define BFFunc		0x0008		// Buffer is a user function.
#define BFHidden	0x0010		// Hidden buffer.
#define BFNarrowed	0x0020		// Buffer is narrowed.
#define BFPreproc	0x0040		// (Script) buffer has been preprocessed.
#define BFQSave		0x0080		// Buffer was saved via quickExit().
#define BFQBak		0x0100		// Buffer backup file was created via quickExit().
#define BFReadOnly	0x0200		// Buffer is read only.
#define BFTermAttr	0x0400		// Terminal attributes enabled.
#define BFCmdFunc	(BFCommand | BFFunc)
#define BFMutable	(BFChanged | BFHidden | BFReadOnly | BFTermAttr)

#define BCmdFuncLead	'@'		// Leading character of script buffer.
#define BAltBufLead	'*'		// Substitution character for non-script buffer names that begin with BCmdFuncLead.
#define BSysLead	'.'		// Leading character of system (internal) buffer.

// Buffer traversal flags.
#define BT_Backward	0x00000001	// Traverse buffer list backward, otherwise forward.
#define BT_Hidden	0x00000002	// Include hidden, non-user-routine buffers.
#define BT_Homed	0x00000004	// Select buffers homed to current screen only.
#define BT_Delete	0x00000008	// Delete current buffer after switch.

// Buffer searching flags (which may be combined with "creating", "clearing", or "deleting" flags below).
#define BS_Query	0x00000000	// Look-up only (do not create).

// Buffer creating (new) flags (which may be combined with "searching", "clearing", or "deleting" flags above and below).
#define BN_Create	0x00000001	// Create buffer if non-existent.
#define BN_Extend	0x00000002	// Create buffer-extension record (CallHdr).
#define BN_Force	0x00000004	// Force-create buffer with unique name.
#define BN_Derive	0x00000008	// Derive buffer name from filename.
#define BN_CreateHook	0x00000010	// Execute "createBuf" hook if a buffer is created.
#define BN_Confirm	0x00000020	// Get user confirmation if creating buffer.

// Buffer clearing flags (which may be combined with "searching", "creating", or "deleting" flags above and below).
#define BC_IgnChgd	0x00000040	// Ignore BFChanged (buffer changed) flag.
#define BC_Unnarrow	0x00000080	// Force-clear narrowed buffer (unnarrow first).
#define BC_ClrFilename	0x00000100	// Clear associated filename, if any.
#define BC_Confirm	0x00000200	// Always ask for confirmation (when interactive).
#define BC_ShowName	0x00000400	// Show buffer name in confirmation prompt.

// Buffer deleting flags (which may be combined with "searching", "creating", or "clearing" flags above).
#define BD_Visible	0x00000800	// Select visible buffers.
#define BD_Unchanged	0x00001000	// Select unchanged buffers.
#define BD_Homed	0x00002000	// Select "homed" buffers.
#define BD_Inactive	0x00004000	// Select inactive buffers.
#define BD_Displayed	0x00008000	// Include displayed buffers.
#define BD_Hidden	0x00010000	// Include hidden buffers.
#define BD_Confirm	0x00020000	// Confirm deletion.
#define BD_Force	0x00040000	// Force-delete all selected buffers (ignore changes).
#define BD_RtnMsg	0x00080000	// Return "buffer deleted" message.

// Buffer saving/writing flags.
#define BW_All		0x00000001	// Saving all buffers, otherwise just current one.
#define BW_MultiDir	0x00000002	// Multiple working directories exist (in screens).
#define BW_QuickExit	0x00000004	// Called from quickExit command.

// Buffer renaming flags.
#define BR_Auto		0x00000001	// Auto-rename buffer (derive name from associated filename).
#define BR_Current	0x00000002	// Rename current buffer (interactively).

// Buffer rendering flags.
#define RendRewind	0x00000001	// Move point to beginning of buffer and unhide it if buffer is not deleted.
#define RendAltML	0x00000002	// Use alternate (simple) mode line when popping buffer.
#define RendWait	0x00000004	// Wait for user to press a key before returning from pop-up window.
#define	RendShift	0x00000008	// Shift display left if needed to prevent line truncation in pop-up window.
#define RendNewBuf	0x00000010	// Buffer was just created (delete after pop).
#define RendNotify	0x00000020	// Display "created" message if n != -1.

// File reading and writing flags.
#define RWExist		0x00000001	// File must exist.
#define RWKeep		0x00000002	// Keep existing filename associated with buffer.
#define RWReadHook	0x00000004	// Run "read" hook.
#define RWScratch	0x00000008	// Create scratch buffer.
#define RWStats		0x00000010	// Return message containing I/O statistics.

// Region descriptor: starting location, size in characters, and number of lines.  If size is negative, region extends backward
// from point, otherwise forward.
typedef struct {
	Point point;			// Origin point location.
	long size;			// Length in characters.
	int lineCount;			// Line count.
	} Region;

// Operation flags for "get region" functions.
#define RForceBegin	0x00000001	// Force point in Region object to beginning of region; otherwise, leave point at
					// original starting point and negate size if region extends backward.
#define RInclDelim	0x00000002	// Include line delimiter (if any) on last line selected when n > 0.
#define REmptyOK	0x00000004	// Allow empty region when getting line block.
#define ROmitEndLine	0x00000008	// Exclude line at end of region from line count.

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
#define EditKill	0x00000001	// Kill operation (save text in kill ring).
#define EditDel		0x00000002	// Delete operation (save text in delete ring).
#define EditSpace	0x00000004	// Insert space character, otherwise newline.
#define EditWrap	0x00000008	// Do word wrap, if applicable.
#define EditHoldPoint	0x00000010	// Hold (don't move) point.

#include "cmd.h"

// Macro for fetching ExecPtr pointer from Datum object in hash record, given HashNode pointer.
#define execPtr(pHashNode)	((ExecPtr *) dgetmemptr((pHashNode)->pVal))

// Structure for the hook table (hookTable).  Hooks are system or user functions that are invoked automatically when certain
// events occur during the editing session.
typedef struct {
	const char *name;		// Name of hook.
	const char *nArgDesc;		// Description of n argument passed to function.
	const char *macArgsDesc;	// Description of call arguments passed to function.
	short argCount;			// Required argument(s).
	bool running;			// True if hook is being executed.
	ExecPtr func;			// Function to execute.
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
	Ring *pRing;			// Data ring to use during terminal input, or NULL for none.
	} TermInpCtrl;

// Terminal completion/prompt flags.  These are combined with Ptr* selector flags for certain function calls so higher bits
// are used.
#define Term_OneChar	0x00001000	// Get one key only with delimiter, C-SPC, and C-g checking.
#define Term_OneKey	0x00002000	// Get one key only, raw.
#define Term_OneKeySeq	0x00003000	// Get one key sequence only, raw.
#define Term_NoKeyEcho	0x00004000	// Don't echo keystrokes.
#define Term_LongPrmt	0x00008000	// Allow extra space (90% terminal width) for long prompt string.
#define Term_Attr	0x00010000	// Enable terminal attribute sequences in prompt string.
#define Term_NoDef	0x00020000	// Don't load default value into input buffer.
#define TermC_NoAuto	0x00040000	// Don't auto-complete; wait for return key.
#define TermC_CFA	0x00080000	// Command, function, or alias name completion.
#define TermC_Buffer	0x00100000	// Buffer name completion.
#define TermC_Filename	0x00200000	// Filename completion (via directory search).
#define TermC_Macro	0x00400000	// Macro name completion.
#define TermC_Mode	0x00800000	// Mode name completion (buffer or global).
#define TermC_Ring	0x01000000	// Ring name completion.
#define TermC_SysConst	0x02000000	// System constant name completion.
#define TermC_SysVar	0x04000000	// System variable name completion.
#define TermC_GblConst	0x08000000	// Global constant name completion.
#define TermC_GblVar	0x10000000	// Global variable name completion.

#define Term_KeyMask	Term_OneKeySeq	// Mask for one-key type.
#define TermC_AllConstVar (TermC_SysConst | TermC_SysVar | TermC_GblConst | TermC_GblVar)
#define TermC_MutVar	(TermC_SysVar | TermC_GblVar)
#define TermC_Mask	(TermC_CFA | TermC_Buffer | TermC_Filename | TermC_Macro | TermC_Mode | TermC_Ring | TermC_AllConstVar)
					// Completion mask.

// Control parameters for virtual terminal.
typedef struct {
	int horzJumpPct;		// Horizontal jump size - percentage.
	int horzJumpCols;		// Horizontal jump size - columns.
	int vertJumpPct;		// Vertical jump size (zero for smooth scrolling).
	const char *termName;		// Value of TERM environmental variable.
	} VTermCtrl;

// Display pointers.
typedef struct {
	Buffer *pBuf;			// Current buffer to display or edit.
	Face *pFace;			// Current window face to display or edit.
	} DispCtrl;

// Session control parameters.
typedef struct {
	DispCtrl cur;			// Current display pointers.
	DispCtrl edit;			// Current edit pointers (for editing a buffer other than the one in current window).
	EWindow *pCurWind;		// Current display window.
	EScreen *pCurScrn;		// Current display screen.
	DirPath *dirHead;		// Head of directory list.
	int fencePause;			// Centiseconds to pause for fence matching.
	int autoSaveCount;		// Auto-save counter (for all buffers).
	int autoSaveTrig;		// Auto-save trigger count.
	uint myPID;			// Unix PID (for temporary filenames).
	ushort opFlags;			// Operation flags.
	ushort maxWindID;		// Maximum window ID.
	int overlap;			// Number of lines to overlap when paging on a screen.
	int travJumpCols;		// Line-traversal jump size.
	long exitNArg;			// n argument given for "exit" or "quickExit" command.
	uint vizBufCount;		// Number of visible buffers.
	uint chgBufCount;		// Number of changed, non-CmdFunc buffers.
	uint thouSepLen;		// Length of thousands' separator string.
	char *thouSep;			// Thousands' separator string (char) from native locale.
	Buffer *pSavedBuf;		// Saved buffer pointer ("saveBuf" function).
	EScreen *pSavedScrn;		// Saved screen pointer ("saveScreen" function).
	EWindow *pSavedWind;		// Saved window pointer ("saveWind" function).
	EScreen *pLastScrn;		// Last screen that was exited from.
	EScreen *scrnHead;		// Head of screen list.
	EWindow *windHead;		// Head of window list in current screen.
	Datum qSearchPat;		// Most recent query-replace search pattern.
	Datum qReplPat;			// Most recent query-replace replace pattern.
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
extern int abortOp(Datum *pRtnVal, long n, Datum **args);
extern int abortInp(void);
extern int aboutMM(Datum *pRtnVal, long n, Datum **args);
extern int alias(Datum *pRtnVal, long n, Datum **args);
extern int allowEdit(bool edit);
extern int apropos(Datum *pRtnVal, long n, Datum **args);
extern int array(Datum *pRtnVal, long n, Datum **args);
extern int arrayClone(Datum *pDest, const Datum *pSrc);
extern int attrCount(const char *str, int len, int maxOffset);
extern int backChar(Datum *pRtnVal, long n, Datum **args);
extern int backLine(Datum *pRtnVal, long n, Datum **args);
extern int backPage(Datum *pRtnVal, long n, Datum **args);
extern int backWord(Datum *pRtnVal, long n, Datum **args);
extern int bactivate(Buffer *pBuf);
extern int bappend(Buffer *pBuf, const char *text);
extern void bchange(Buffer *pBuf, ushort flags);
extern void bchgflag(Buffer *pBuf, bool setFlag);
extern int bclear(Buffer *pBuf, uint flags);
extern int bconfirm(Buffer *pBuf, uint flags);
extern void bcount(Buffer *pBuf, ushort flags);
extern Buffer *bdefault(void);
extern int bdelete(Buffer *pBuf, uint flags);
extern int beginText(Datum *pRtnVal, long n, Datum **args);
extern bool beginTxt(void);
extern int beginEndLine(Datum *pRtnVal, long n, bool end);
extern bool bempty(Buffer *pBuf);
extern int bextend(Buffer *pBuf);
extern int bfind(const char *name, uint opFlags, ushort bufFlags, Buffer **ppBuf, bool *created);
extern int bigBufMove(Datum *pRtnVal, long n, const char *prompt, uint op, long arg);
extern void bufFaceToWindFace(Buffer *pBuf, EWindow *pWind);
extern bool binSearch(const char *key, const void *table, ssize_t n, int (*cmp)(const char *str1, const char *str2),
 const char *(*fetch)(const void *table, ssize_t i), ssize_t *pIndex);
extern int bnuke(uint oflags, const char *workDir, uint *pCount);
extern int bpop(Buffer *pBuf, uint flags);
extern int brename(Datum *pRtnVal, uint flags, Buffer *pTargBuf);
extern int bscratch(Buffer **ppBuf);
extern int bsort(Buffer *pBuf, Point *pPoint, long n, uint flags);
extern Buffer *bsrch(const char *bufname, ssize_t *pIndex);
extern int bswitch(Buffer *pBuf, uint flags);
extern int bufAttrQ(Datum *pRtnVal, long n, Datum **args);
extern bool bufBegin(Point *pPoint);
extern bool bufClearModes(Buffer *pBuf);
extern bool bufEnd(Point *pPoint);
extern int bufInfo(Datum *pRtnVal, long n, Datum **args);
extern long bufLength(Buffer *pBuf, int *pLineCount);
extern void calcHorzJumpCols(void);
extern void centiPause(int cs);
extern int chgBufAttr(Datum *pRtnVal, long n, Datum **args);
extern int chgBufHome(Datum *pRtnVal, long n, Datum **args);
extern int chgCase(short c);
extern int chgdir(const char *dir);
extern int chgMode(Datum *pRtnVal, long n, Datum **args);
extern int chgMode1(const char *name, int action, Buffer *pBuf, long *formerStatep, ModeSpec **result);
extern int chgWindSize(Datum *pRtnVal, long n, int how);
extern int clearBuf(Datum *pRtnVal, long n, Datum **args);
extern void clearBufFilename(Buffer *pBuf);
extern void clearBufMode(ModeSpec *pModeSpec);
extern void clearGlobalMode(ModeSpec *pModeSpec);
extern int convertCase(long n, uint flags);
extern int cvtBuf(Datum *pRtnVal, Datum **args, bool toArray);
extern int cycleRing(Datum *pRtnVal, long n, Datum **args);
extern int delAlias(Datum *pRtnVal, long n, Datum **args);
extern int delBlankLines(Datum *pRtnVal, long n, Datum **args);
extern int delBuf(Datum *pRtnVal, long n, Datum **args);
extern int delBufMarks(Buffer *pBuf, uint flags);
extern int delJoinWind(long n, CmdFunc *pCmdFunc);
extern int delKillRegion(bool kill);
extern int delMark(Datum *pRtnVal, long n, Datum **args);
extern int delMark1(Mark *pMark, uint flags);
extern int delRingEntry(Datum *pRtnVal, long n, Datum **args);
extern int delRoutine(Datum *pRtnVal, long n, Datum **args);
extern int delScreen(Datum *pRtnVal, long n, Datum **args);
extern int delTab(long n, bool force);
extern void deselectOpts(OptHdr *pOptHdr);
extern int detabLine(Datum *pRtnVal, long n, Datum **args);
extern int dirFunc(Datum *pRtnVal, long n, Datum **args, CmdFuncId cmdFuncId);
extern int doPop(Datum *pRtnVal, long n, CmdFunc *pCmdFunc);
#if MMDebug & (Debug_ScrnDump | Debug_Narrow)
extern void dumpBuffer(const char *label, Buffer *pBuf, bool withData);
#endif
#if MMDebug & Debug_ModeDump
void dumpModes(Buffer *pBuf, bool sepLine, const char *fmt, ...);
#endif
#if MMDebug & Debug_RingDump
extern void dumpRing(Ring *pRing);
#endif
#if MMDebug & Debug_ScrnDump
extern void dumpScreens(const char *msg);
#endif
#if MMDebug & (Debug_ArrayHash | Debug_AryHshBuf | Debug_AryHshLog | Debug_Expr | Debug_UserVars)
extern void dumpVal(Datum *pDatum, bool newln);
#endif
extern int dupLine(Datum *pRtnVal, long n, Datum **args);
extern int edelc(long n, uint flags);
extern int edInsertTab(long n);
extern int editAlias(const char *name, uint op, ExecPtr *pExec);
extern int editMode(Datum *pRtnVal, long n, Datum **args);
extern int editModeGrp(Datum *pRtnVal, long n, Datum **args);
extern int einsertc(long n, short c);
extern int einserts(const char *str);
extern int einsertNL(void);
extern int endless(int rtnCode);
extern int endWord(Datum *pRtnVal, long n, Datum **args);
extern int entabLine(Datum *pRtnVal, long n, Datum **args);
extern int eopendir(const char *fileSpec, char **pFilePath);
extern int ereaddir(void);
extern int escMsgAttr(DFab *pFab);
extern int esctofab(const char *str, size_t len, DFab *pFab);
extern int execAH(Datum *pRtnVal, long n, Datum **args, CmdFunc *pCmdFunc);
extern int execFind(const char *name, uint op, uint selector, ExecPtr *pExec);
extern int execNew(const char *name, ExecPtr *pExec);
extern int expandPath(Datum *pDest, Datum *pSrc);
extern void faceInit(Face *pFace, Line *pLine, Buffer *pBuf);
extern int fenceMatch(short fence, bool doBeep);
extern Buffer *findBuf(Datum *pBufname);
extern int findBufMark(ushort id, Mark **ppMark, uint flags);
extern EWindow *findWind(Buffer *pBuf, EScreen **ppScrn);
extern void fixBufFace(Buffer *pBuf, int offset, long n, Line *pLine1, Line *pLine2);
extern void fixFace(int offset, long n, Line *pLine1, Line *pLine2);
extern int forwChar(Datum *pRtnVal, long n, Datum **args);
extern int forwLine(Datum *pRtnVal, long n, Datum **args);
extern int forwPage(Datum *pRtnVal, long n, Datum **args);
extern int forwWord(Datum *pRtnVal, long n, Datum **args);
extern int getBufname(Datum *pRtnVal, const char *prompt, uint flags, Buffer **ppBuf, bool *created);
extern int getCol(Point *pPoint, bool termAttr);
extern int getCF(const char *prompt, ExecPtr *pExec, uint selector);
extern int getCFA(const char *prompt, uint selector, ExecPtr *pExec, const char *errItem);
extern uint getFlagOpts(OptHdr *pOptHdr);
extern int getInfo(Datum *pRtnVal, long n, Datum **args);
extern int getLineNum(Buffer *pBuf, Line *pTargLine);
extern int getLineRegion(long n, Region *pRegion, uint flags);
extern int getModes(Datum *pRtnVal, Buffer *pBuf);
extern int getNArg(Datum *pRtnVal, const char *prompt);
extern int getNum(const char *prompt, int screen, int *pNum);
extern int getRegion(Region *pRegion, uint flags);
extern int getRingName(Ring **ppRing);
extern void getTextRegion(Point *pPoint, long n, Region *pRegion, uint flags);
extern int getWindCount(EScreen *pScrn, int *pWindNum);
extern int getWindNum(EWindow *pWind);
extern int getWord(Datum *pRtnVal, long n, Datum **args);
extern int getWindPos(EWindow *pWind);
extern int gotoFence(Datum *pRtnVal, long n, Datum **args);
extern int gotoLine(Datum *pRtnVal, long n, Datum **args);
extern int gotoMark(Datum *pRtnVal, long n, Datum **args);
extern int gotoScreen(long n, uint flags);
extern int gotoWind(Datum *pRtnVal, long n, uint flags);
extern int grpModeActiveQ(Datum *pRtnVal, long n, Datum **args);
extern int hashClone(Datum *pDest, const Datum *pSrc);
extern bool haveColor(void);
extern int helpInfo(char **pInfo, enum h_type t, const char *binName);
extern const char *hookFuncName(ExecPtr *pExec);
extern void initCharTables(void);
extern int insertBuf(Datum *pRtnVal, long n, Datum **args);
extern int inserti(Datum *pRtnVal, long n, Datum **args);
extern int insertNLSpace(long n, uint flags);
extern bool interactive(void);
extern bool inWord(Point *pPoint);
extern int iorStr(const char *src, long n, uint insertType, EScreen *pScrn, Ring *pRing);
extern int iorText(const char *src, long n, uint insertType, Buffer *pBuf);
extern bool isBufModeSet(Buffer *pBuf, ModeSpec *pModeSpec);
extern int isBufQ(Datum *pRtnVal, long n, Datum **args);
extern int isConstVarQ(Datum *pRtnVal, long n, Datum **args);
extern int isExecQ(Datum *pRtnVal, long n, Datum **args);
extern bool isLowerCase(short c);
extern int isMarkQ(Datum *pRtnVal, long n, Datum **args);
extern int isModeQ(Datum *pRtnVal, long n, Datum **args);
extern int isModeGrpQ(Datum *pRtnVal, long n, Datum **args);
extern bool isUpperCase(short c);
extern bool is_digit(short c);
extern bool is_letter(short c);
extern bool iswhite(short c);
extern int joinLines(Datum *pRtnVal, long n, Datum **args);
extern int kdcBackWord(long n, int kdc);
extern bool kdcFencedRegion(int kdc);
extern int kdcForwWord(long n, int kdc);
extern int kdcLine(long n, int kdc);
extern int kdcText(long n, int kdc, Region *pRegion);
extern int killPrep(int kill);
extern int lalloc(int used, Line **ppLine);
extern int libfail(void);
extern void llink(Line *pLine1, Buffer *pBuf, Line *pLine2);
extern bool lineInWind(EWindow *pWind, Line *pLine);
extern void lreplace1(Line *pLine1, Buffer *pBuf, Line *pLine2);
extern int makeArray(Datum *pRtnVal, ArraySize len, Array **ppArray);
extern int makeHash(Datum *pRtnVal, HashSize size, Hash **ppHash);
extern char *makeLower(char *dest, const char *src);
extern char *makeUpper(char *dest, const char *src);
extern int markBuf(Datum *pRtnVal, long n, Datum **args);
extern int mcreate(const char *name, const char *desc, ushort flags, ModeSpec **ppModeSpec);
extern int message(Datum *pRtnVal, long n, Datum **args);
extern int mgcreate(const char *name, ModeGrp *prior, const char *desc, ModeGrp **result);
extern bool mgsrch(const char *name, ModeGrp **slot, ModeGrp **ppModeGrp);
extern int mlerase(uint flags);
extern void mleraseEOL(void);
extern int mlflush(void);
extern int mlmove(int col);
extern int mlprintf(uint flags, const char *fmt, ...);
extern void mlputc(short c, uint flags);
extern int mlputs(const char *msg, uint flags);
extern int mlrestore(void);
extern int modeActiveQ(Datum *pRtnVal, long n, Datum **args);
extern bool modeSet(uint id, Buffer *pBuf);
extern int moveChar(long n);
extern int moveLine(long n);
extern void movePoint(Point *pPoint);
extern int moveWord(long n);
extern int moveWindUp(Datum *pRtnVal, long n, Datum **args);
extern bool mscan(Buffer *pBuf, bool clear, int modeCount, ...);
extern ModeSpec *msrch(const char *name);
extern int narrowBuf(Datum *pRtnVal, long n, Datum **args);
extern int newCol(short c, int col);
extern int newColTermAttr(Point *pPoint, int col, int *attrLen);
extern int newlineI(Datum *pRtnVal, long n, Datum **args);
extern void newWorkDir(EScreen *pOldScrn);
extern int nextWind(Datum *pRtnVal, long n, Datum **args);
extern int nukeWhite(long n, bool prior);
extern int ohAryPrep(OrdHash *pOrdHash, HashNode ***ppAry);
extern int onlyWind(Datum *pRtnVal, long n, Datum **args);
extern int openLineI(Datum *pRtnVal, long n, Datum **args);
extern int otherFence(short fence, Region *pRegion);
extern int overPrep(long n);
extern char *pad(char *s, uint len);
extern int parseOpts(OptHdr *pOptHdr, Datum *pOptions);
extern int pathSearch(const char *name, uint flags, void *result, const char *funcName);
extern int pipeCmd(Datum *pRtnVal, long n, const char *prompt, uint flags);
extern int prevNextBuf(Datum *pRtnVal, uint flags);
extern int prevWind(Datum *pRtnVal, long n, Datum **args);
extern bool pointInWind(void);
extern char *putAttrStr(const char *str, const char *strEnd, uint *flags, void *targ);
extern int quit(Datum *pRtnVal, long n, Datum **args);
extern int quoteChar(Datum *pRtnVal, long n, Datum **args);
extern int rcycle(Ring *pRing, long n, bool msg);
extern int rdelete(Ring *pRing, long e);
extern int regionToKill(Region *pRegion);
extern char *regionToStr(char *regBuf, Region *pRegion);
extern int regionLines(long *pLineCount);
extern int renameBuf(Datum *pRtnVal, long n, Datum **args);
extern int render(Datum *pRtnVal, long n, Buffer *pBuf, uint flags);
extern int resetTerm(Datum *pRtnVal, long n, Datum **args);
extern int resizeWind(Datum *pRtnVal, long n, Datum **args);
extern RingEntry *rget(Ring *pRing, long e);
extern int ringSize(Datum *pRtnVal, long n, Datum **args);
extern int rsave(Ring *pRing, const char *value, bool force);
extern void rsclear(ushort flags);
extern void rspop(short oldStatus, ushort oldFlags);
extern void rspush(short *pStatus, ushort *pFlags);
extern int rscat(const char *fmt, ...);
extern int rssave(void);
extern int rsset(int status, ushort flags, const char *fmt, ...);
extern void rsunfail(void);
extern int runBufHook(Datum **ppRtnVal, uint flags);
extern int runCmdline(Datum *pRtnVal, const char *cmdPrefix, const char *arg, const char *cmdSuffix, uint flags);
extern int scratchBuf(Datum *pRtnVal, long n, Datum **args);
extern int scrnCount(void);
extern int sdelete(EScreen *pScrn, bool delBufs);
extern int selectBuf(Datum *pRtnVal, long n, Datum **args);
extern int selectScreen(Datum *pRtnVal, long n, Datum **args);
extern int selectText(Datum *pRtnVal, long n, Datum **args);
extern int selectWind(Datum *pRtnVal, long n, Datum **args);
extern int setBufMode(Buffer *pBuf, ModeSpec *pModeSpec, bool clear, bool *pWasSet);
extern int setColorPair(Datum *pRtnVal, long n, Datum **args);
extern int setDefault(Datum *pRtnVal, long n, Datum **args);
extern int setDispColor(Datum *pRtnVal, long n, Datum **args);
extern int setExecPath(const char *path);
extern int setFilename(Buffer *pBuf, const char *filename, uint flags);
extern void setGlobalMode(ModeSpec *pModeSpec);
extern int seti(Datum *pRtnVal, long n, Datum **args);
extern int setMark(Datum *pRtnVal, long n, Datum **args);
extern int setPointCol(int pos);
extern void setWindMark(Mark *pMark, EWindow *pWind);
extern int setWorkDir(EScreen *pScrn);
extern int sfind(ushort scrnNum, Buffer *pBuf, EScreen **ppScrn);
extern int shellCLI(Datum *pRtnVal, long n, Datum **args);
extern int shiftLineBlock(Datum *pRtnVal, long n, Datum **args, uint flags);
extern int showBuffers(Datum *pRtnVal, long n, Datum **args);
extern int showColors(Datum *pRtnVal, long n, Datum **args);
extern int showFence(Datum *pRtnVal, long n, Datum **args);
extern int showMarks(Datum *pRtnVal, long n, Datum **args);
extern int showModes(Datum *pRtnVal, long n, Datum **args);
extern int showPoint(Datum *pRtnVal, long n, Datum **args);
extern int showRing(Datum *pRtnVal, long n, Datum **args);
extern int showScreens(Datum *pRtnVal, long n, Datum **args);
extern int skipWhite(char **pSrc, int len);
extern int sortRegion(Datum *pRtnVal, long n, Datum **args);
extern int spanWhite(Datum *pRtnVal, bool end);
extern int sswitch(EScreen *pScrn, uint flags);
extern int strclone(char **pDest, const char *src, const char *funcName);
extern char *strparse(char **pStr, short delimChar);
extern char *strTime(void);
extern int substr(Datum *pRtnVal, Datum *pStr, long i1, long i2);
extern void supd_windFlags(Buffer *pBuf, ushort flags);
extern int suspendMM(Datum *pRtnVal, long n, Datum **args);
extern int swapMark(Datum *pRtnVal, long n, Datum **args);
extern int swapMarkId(ushort id);
extern int swapScreens(Datum *pRtnVal, long n, Datum **args);
extern int sysBuf(const char *root, Buffer **ppBuf, uint flags);
extern int sysCallError(const char *caller, const char *call, bool addTERM);
extern int tabStop(int count);
extern int termInp(Datum *pRtnVal, const char *basePrompt, uint argFlags, uint ctrlFlags, TermInpCtrl *pTermInp);
extern int termInpAttrYN(const char *basePrompt, bool *result);
extern int termInpYN(const char *basePrompt, bool *result);
extern int titleCaseStr(Datum *pRtnVal, long n, Datum **args);
extern int toString(Datum *pRtnVal, long n, Datum **args);
extern int traverseLine(Datum *pRtnVal, long n, Datum **args);
extern int trimLine(Datum *pRtnVal, long n, Datum **args);
extern void tbeep(void);
extern void tbold(bool msgLine, bool attrOn);
extern int tclose(bool saveTTY);
extern int tflush(void);
extern int tmove(int row, int col);
extern int topen(void);
extern void trev(bool msgLine, bool attrOn);
extern void tul(bool msgLine, bool attrOn);
extern int typahead(int *pCount);
extern int update(long n);
extern int updateScrn(Datum *pRtnVal, long n, Datum **args);
extern long urand(long upperBound);
extern int userPrompt(Datum *pRtnVal, long n, Datum **args);
extern int vtinit(void);
extern bool whiteLine(Line *pLine);
extern void windFaceToBufFace(EWindow *pWind, Buffer *pBuf);
extern EWindow *windDispBuf(Buffer *pBuf, bool skipCur);
extern int widenBuf(Datum *pRtnVal, long n, Datum **args);
extern bool windNewTop(EWindow *pWind, Line *pLine, long n);
extern EWindow *windNextIs(EWindow *pWind);
extern int wrapLine(Datum *pRtnVal, long n, Datum **args);
extern int wrapWord(Datum *pRtnVal, long n, Datum **args);
extern int writeBuf(Datum *pRtnVal, long n, uint flags);
extern int wscroll(Datum *pRtnVal, long n, int (*windFunc)(Datum *pRtnVal, long n, Datum **args),
 int (*pageFunc)(Datum *pRtnVal, long n, Datum **args));
extern int wsplit(long n, EWindow **ppWind);
extern int wswitch(EWindow *pWind, uint flags);
extern int wupd_reframe(EWindow *pWind);
extern int ycycle(Datum *pRtnVal, long n, Ring *pRing, ushort flag, uint cmd);
extern int yrevert(ushort flags);

#ifdef MainData

// **** For main.c global data ****

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
	{"^TermAttr", "^TAttr", 0, BFTermAttr}};
OptHdr bufAttrHdr = {
	bufAttrTable, NULL, text374, elementsof(bufAttrTable), 1, 0, OHdrPrep | OHdrBuild | OHdrNoSel};
		// "buffer attribute"
char buffer1[] = Buffer1;		// Name of first buffer ("unnamed").
OrdHash buf = {OH_Changed, NULL};	// Buffer table.
char *Copyright = "(c) Copyright 2026 Richard W. Marinelli";
Option defOptions[] = {
	{"HardTabSize", NULL, 0, -1},
	{"SoftTabSize", NULL, 0, -1},
	{"WrapCol", NULL, 0, -1}};
OptHdr defOptHdr = {
	defOptions, NULL, text497, elementsof(defOptions), -1, 0, OHdrSingle};
		// "parameter name"
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
	{"write", HLitN_defn, HLitArg_write, 3, false, {PtrNull, NULL}},
	{NULL, NULL, NULL, 0, false, {PtrNull, NULL}}
	};
IVar iVar = {1, 1};			// "i" variable.
#if MMDebug
FILE *logfile;				// Log file for debugging.
#endif
char lowCase[256];			// Upper-to-lower translation table.
ModeHdr mode = {{OH_Changed, NULL}};	// Mode header record.
char Myself[] = ProgName;		// Common name of program.
Ring ringTable[] = {
	{NULL, 0, DelRingSize, RingIdxDel | RingKD, NULL, NULL},
	{NULL, 0, KillRingSize, RingIdxKill | RingKD, NULL, ""},
	{NULL, 0, ReplRingSize, RingIdxRepl, NULL, NULL},
	{NULL, 0, SearchRingSize, RingIdxSearch, NULL, NULL}};
ushort ringTableSize = elementsof(ringTable);
uint scratchBufNum = 0;			// Unique suffix number for scratch buffers.
SessionCtrl sess = {			// Session parameters.
	.fencePause = FencePause, .autoSaveCount = AutoSaveTrig, .autoSaveTrig = AutoSaveTrig,
	.opFlags = OpEval | OpStartup | OpScrnRedraw, .overlap = PageOverlap, .travJumpCols = TravJump,
	.maxWindID = USHRT_MAX, .rtn = {Success, 0}, .scriptRtn = {Success, 0}
	};
ETerm term = {
	0,				// Current number of columns.
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
	{{"Show", -2, -2},		// Color pair for "show" displays.
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
extern OptHdr bufAttrHdr;
extern Option bufAttrTable[];
extern char buffer1[];
extern OrdHash buf;
extern OptHdr defOptHdr;
extern Option defOptions[];
extern HookRec hookTable[];
extern IVar iVar;
#if MMDebug
extern FILE *logfile;
#endif
extern char lowCase[];
extern ModeHdr mode;
extern char Myself[];
extern Ring ringTable[];
extern ushort ringTableSize;
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
