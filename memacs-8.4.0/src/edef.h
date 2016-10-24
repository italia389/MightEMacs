// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// edef.h	Structure and preprocesser definitions for MightEMacs.

// GeekLib include files.
#include "gl_valobj.h"

// Other standard include files.
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Program-logic (source code) debugging flags.
#define DEBUG_LOGFILE	0x0001		// Open file "logfile" for debugging output.
#define DEBUG_SCRDUMP	0x0002		// Dump screens, windows, and buffers.
#define DEBUG_CFAB	0x0004		// Show CFAB pointer type in "showBindings" display.
#define DEBUG_NARROW	0x0008		// Dump buffer info to log file in narrowBuf().
#define DEBUG_KILLRING	0x0010		// Include dispkill() function.
#define DEBUG_BWINDCT	0x0020		// Display buffer's window count in "showBuffers" display.
#define DEBUG_SHOWRE	0x0040		// Include showRegexp command.
#define DEBUG_TOKEN	0x0080		// Dump token-parsing info to log file.
#define DEBUG_VALUE	0x0100		// Dump Value processing info to log file.
#define DEBUG_MARG	0x0200		// Dump macro-argument info to log file.
#define DEBUG_SCRIPT	0x0400		// Write macro lines to log file.
#define DEBUG_EXPR	0x0800		// Write expression-parsing info to log file.
#define DEBUG_PPBUF	0x1000		// Dump script preprocessor blocks to log file and exit.
#define DEBUG_BIND	0x2000		// Dump binding table.

#define VDebug		0
#define MMDEBUG		0		// No debugging code.
//#define MMDEBUG		DEBUG_LOGFILE
//#define MMDEBUG		(DEBUG_BIND | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_SCRIPT | DEBUG_EXPR | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_SCRIPT | DEBUG_TOKEN | DEBUG_EXPR | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_SCRIPT | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_VALUE | DEBUG_SCRIPT | DEBUG_TOKEN | DEBUG_EXPR | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_TOKEN | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_MARG | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_PPBUF | DEBUG_LOGFILE)
//#define MMDEBUG		DEBUG_SHOWRE

#if MMDEBUG & DEBUG_MARG
#define dobuf		dobufDebug
#endif

// Program Identification...
//
// Scripts can query these via the $EditorName, $Version, and $OS system variables.  CENTOS must be after REDHAT because
// both are defined for Red Hat.
#define PROGNAME	"MightEMacs"
#define VERSION		"8.4.0"
#if DEBIAN
#	define OSNAME	"Debian Linux"
#elif HPUX8 || HPUX
#	define OSNAME	"HP/UX"
#elif OSX
#	define OSNAME	"OS X"
#elif REDHAT
#	define OSNAME	"Red Hat Linux"
#elif CENTOS
#	define OSNAME	"CentOS Linux"
#elif SOLARIS
#	define OSNAME	"Solaris"
#else
#	define OSNAME	"Unix"
#endif

/***** BEGIN CUSTOMIZATIONS *****/

// Terminal output definitions -- [Set one of these].
#define TT_TERMCAP	1		// Use TERMCAP.
#define TT_CURSES	0		// Use CURSES (not working).

// Terminal size definitions -- [Set any except TT_MINCOLS and TT_MINROWS].
#define TT_MINCOLS	40		// Minimum number of columns.
#define TT_MAXCOLS	240		// Maximum number of columns.
#define TT_MINROWS	3		// Minimum number of rows.
#define TT_MAXROWS	80		// Maximum number of rows.

// Language text options -- [Set one of these].
#define ENGLISH		1		// Default.
#define SPANISH		0		// If any non-English macro is set, must translate english.h to the corresponding
					// header file (e.g., spanish.h) and edit elang.h to include it.
// Configuration options -- [Set any].
#define TYPEAH		1		// Include $KeyPending and type-ahead checking (which skips unnecessary screen updates).
#define WORDCOUNT	0		// Include code for "countWords" command (deprecated).
#define MLSCALED	0		// Include "%f" (scaled integer) formatting option in mlprintf() (deprecated).
#define VISMAC		0		// Update display during keyboard macro execution.
#define REVSTA		1		// Status line appears in reverse video.
#define COLOR		0		// Include code for color commands and windows (not working).
#define VIZBELL		0		// Use a visible BELL (instead of an audible one) if it exists.
#define KMDELIMS	":;,\"'"	// Keyboard macro encoding delimiters ($keyMacro), in order of preference.
#define DEFWORDLST	"A-Za-z0-9_"	// Default $wordChars value.
#define BACKUP_EXT	".bak"		// Backup file extension.
#define SCRIPT_EXT	".mm"		// Script file extension.
#define USER_STARTUP	".memacs"	// User start-up file (in HOME directory).
#define SITE_STARTUP	"memacs.mm"	// Site start-up file.
#define MMPATH_NAME	"MMPATH"	// Shell environmental variable name.
#if DEBIAN
#define MMPATH_DEFAULT	":/usr/lib/memacs"	// Default search directories.
#else
#define MMPATH_DEFAULT	":/usr/local/lib/memacs"	// Default search directories.
#endif
#define LOGFILE		"memacs.log"	// Log file (for debugging).

// Limits -- [Set any].
#define MAXTAB		240		// Maximum hard or soft tab size.
#define NBNAME		24		// Number of bytes, buffer name.
#define NTERMINP	(MaxPathname < 1024 ? 1024 : MaxPathname)
					// Number of bytes, terminal input (must be >= MaxPathname).
#define NBLOCK		32		// Number of bytes, line block chunks.
#define KBLOCK		256		// Number of bytes, kill buffer chunks.
#define NRING		30		// Number of buffers in kill ring.
#define NVNAME		32		// Maximum number of characters in a user variable name (including prefix).
#define NASAVE		220		// Number of keystrokes before auto-save -- initial value.
#define MAXLOOP		2500		// Default maximum number of script loop iterations allowed.
#define MAXRECURS	100		// Default maximum recursion depth allowed during script execution.
#define FPAUSE		26		// Default time in centiseconds to pause for fence matching.
#define VJUMPMIN	10		// Minimum vertical jump size (percentage).
#define JUMPMAX		49		// Maximum horizontal or vertical jump size (percentage).
#if COLOR
#define NCOLORS		16		// Number of supported colors.
#define NPALETTE	48		// Size of palette string (palstr).
#endif

/***** END CUSTOMIZATIONS *****/

// Internal constants.
#define NPREFIX		6		// Number of prefix keys (META, ^C, ^H, ^X, FKEY, and SHFT-FKEY).
#define NDELIM		2		// Number of bytes, input and output record delimiters.
#define NPATMIN		32		// Minimum array size for search or replacement pattern.
#define NPATMAX		96		// Maximum array size to retain for search or replacement pattern.
#define NKBDCHUNK	48		// Extension size of keyboard macro buffer when full.
#define NWORK		80		// Number of bytes, work buffer.
#define RMARK		' '		// Mark which defines region endpoint.
#define WMARK		'.'		// Work mark, used as default for certain commands.

// Codes for true, false, and nil pseudo types -- used by vistfn() to determine type of a Value object.
#define VNIL		0x0000
#define VFALSE		0x0001
#define VTRUE		0x0002
#define VANY		0x0004
#define VBOOL		(VFALSE | VTRUE)

// Operation flags used at runtime (in "opflags" global variable).
#define OPVTOPEN	0x0001		// Virtual terminal open?
#define OPEVAL		0x0002		// Evaluate expressions?
#define OPHAVEEOL	0x0004		// Does clear to EOL exist?
#define OPHAVEREV	0x0008		// Does reverse video exist?
#define OPSTARTUP	0x0010		// In pre-edit-loop state?
#define OPSCRIPT	0x0020		// Script execution flag.
#define OPPARENS	0x0040		// Command, alias, macro, or system function invoked in xxx() form.
#define OPSCREDRAW	0x0080		// Clear and redraw screen if true.

// Buffer operation flags used by bufop().
#define BOPSETFLAG	1		// Set buffer flag.
#define BOPCLRFLAG	2		// Clear buffer flag.
#define BOPBEGEND	3		// Move to beginning or end of buffer.
#define BOPGOTOLN	4		// Goto to a line in the buffer.
#define BOPREADBUF	5		// Read line(s) from a buffer.

// Flags used by join().
#define JNKEEPALL	0x0001		// Keep null and nil arguments.
#define JNSHOWNIL	0x0002		// Use "nil" for nil; otherwise, null.
#define JNSHOWBOOL	0x0004		// Use "true" and "false" for Boolean.

// Flags used by tostr().
#define TSNULL		0x0001		// Convert nil to a null string.
#define TSNOBOOL	0x0002		// Error if Boolean.
#define TSNOBOOLN	0x0004		// Error if Boolean or nil.

// Information display characters.
#define MACFORMAT	"@%.*s"		// sprintf() format string for prepending prefix to macro name.
#define ALTBUFCH	'*'		// Substitution character for non-macro buffer names that begin with SBMACRO.
#define SBACTIVE	':'		// BFACTIVE flag indicator (activated buffer -- file read in).
#define SBCHGD		'*'		// BFCHGD flag indicator (changed buffer).
#define SBHIDDEN	'?'		// BFHIDDEN flag indicator (hidden buffer).
#define SBMACRO		'@'		// BFMACRO flag indicator (macro buffer).
#define SBPREPROC	'+'		// BFPREPROC flag indicator (preprocessed buffer).
#define SBTRUNC		'#'		// BFTRUNC flag indicator (truncated buffer).
#define SBNARROW	'<'		// BFNARROW flag indicator (narrowed buffer).

// Key prefixes.
#define CTRL		0x0100		// Control flag.
#define META		0x0200		// Meta flag.
#define PREF1		0x0400		// Prefix1 (^X) flag.
#define PREF2		0x0800		// Prefix2 (^C) flag.
#define PREF3		0x1000		// Prefix3 (^H) flag.
#define SHFT		0x2000		// Shift flag (for function keys).
#define FKEY		0x4000		// Special key flag (function key).
#define PREFIX		(META | PREF1 | PREF2 | PREF3)	// Prefix key mask.
#define KEYSEQ		(META | PREF1 | PREF2 | PREF3 | FKEY)	// Key sequence mask.

#define RTNKEY		(CTRL | 'M')	// "return" key as an extended key.
#define ALTRTNKEY	(CTRL | 'J')	// newline as an extended key (alternate return key).

// Command return status codes.  Note that NOTFOUND, IONSF, and IOEOF are never actually set via rcset() (so rc.status will
// never be one of those codes); whereas, all other status codes are always set, either explicitly or implicitly.
#define PANIC		-10		// Panic return -- exit immediately (from rcset()).
#define OSERROR		-9		// Fatal OS error with errno lookup.
#define FATALERROR	-8		// Fatal system or library error.
#define SCRIPTEXIT	-7		// Script forced exit with dirty buffer(s).
#define USEREXIT	-6		// Clean buffer(s) or user forced exit with dirty ones.
#define HELPEXIT	-5		// -?, -h, or -V switch.
#define MINEXIT		HELPEXIT	// Minimum severity which causes program exit.
#define SCRIPTERROR	-4		// Last command failed during script execution.
#define FAILURE		-3		// Last command failed.
#define USERABORT	-2		// Last command aborted by user.
#define NOTFOUND	-1		// Last search or item retrieval failed.
#define SUCCESS		0		// Last command succeeded.
#define IONSF		1		// "No such file" return on open.
#define IOEOF		2		// "End of file" return on read.

// Message line print flags.
#define MLHOME		0x0001		// Move cursor to beginning of message line before display.
#define MLFORCE		0x0002		// Force output (ignore 'msg' global mode).
#define MLWRAP		0x0004		// Wrap message [like this].
#define MLRAW		0x0008		// Output raw character.
#define MLTRACK		0x0010		// Keep track of character length (for backspacing).

// User variable definition.
typedef struct uvar {
	struct uvar *uv_nextp;		// Pointer to next variable.
	char uv_name[NVNAME + 1];	// Name of user variable.
	ushort uv_flags;		// Variable flags.
	Value *uv_vp;			// Value (integer or string).
	} UVar;

// Definition of system variables.
enum svarid {
	// Immutables.
	sv_ArgCount,sv_BufCount,sv_BufFlagActive,sv_BufFlagChanged,sv_BufFlagHidden,sv_BufFlagMacro,sv_BufFlagNarrowed,
	sv_BufFlagPreprocd,sv_BufFlagTruncated,sv_BufInpDelim,sv_BufLen,sv_BufList,sv_BufOtpDelim,sv_BufSize,sv_Date,
	sv_EditorName,sv_EditorVersion,
#if TYPEAHEAD
	sv_KeyPending,
#endif
	sv_KillText,sv_Language,sv_LineLen,sv_Match,sv_ModeAutoSave,sv_ModeBackup,sv_ModeC,sv_ModeClobber,sv_ModeColDisp,
	sv_ModeEsc8Bit,sv_ModeExact,sv_ModeExtraIndent,sv_ModeHorzScroll,sv_ModeLineDisp,sv_ModeMEMacs,sv_ModeMsgDisp,
	sv_ModeNoUpdate,sv_ModeOver,sv_ModePerl,sv_ModeReadOnly,sv_ModeRegexp,sv_ModeReplace,sv_ModeRuby,sv_ModeSafeSave,
	sv_ModeShell,sv_ModeWorkDir,sv_ModeWrap,sv_OS,sv_RegionText,sv_ReturnMsg,sv_RunFile,sv_RunName,sv_TermCols,sv_TermRows,
	sv_WindCount,sv_WindList,

	// Mutables.
	sv_argIndex,sv_autoSave,sv_bufFile,sv_bufFlags,sv_bufLineNum,sv_bufModes,sv_bufName,sv_defModes,
#if COLOR
	sv_desktopColor,
#endif
	sv_execPath,sv_fencePause,sv_globalModes,sv_hardTabSize,sv_horzJump,sv_horzScrollCol,sv_inpDelim,sv_keyMacro,
	sv_lastKeySeq,sv_lineChar,sv_lineCol,sv_lineOffset,sv_lineText,sv_maxLoop,sv_maxRecursion,sv_otpDelim,sv_pageOverlap,
#if COLOR
	sv_palette,
#endif
	sv_randNumSeed,sv_replacePat,sv_screenNum,sv_searchDelim,sv_searchPat,sv_showModes,sv_softTabSize,sv_travJumpSize,
	sv_vertJump,sv_windLineNum,sv_windNum,sv_windSize,sv_wordChars,sv_workDir,sv_wrapCol
	};

// System variable record.
typedef struct {
	char *sv_name;			// Name of system variable.
	enum svarid sv_id;		// Unique identifier.
	ushort sv_flags;		// Variable flags.
	char *sv_desc;			// Short description.
	Value *sv_vp;			// Value pointer if a constant; otherwise, NULL.
	} SVar;

// System and user variable flags.
#define V_NULLTOK	0x0001		// Return null token on next &shift or &pop operation.
#define V_RDONLY	0x0002		// Read-only variable.
#define V_INT		0x0004		// Integer variable.
#define V_GLOBAL	0x0008		// Global variable (in user variable table).
#define V_MODE		0x0010		// Mode variable (get description from mode table).
#define V_NIL		0x0020		// Nil assignment allowed to system variable.
#define V_ESCDELIM	0x0040		// Use escape character as input delimiter when prompting for a value.

// Macro argument.
typedef struct marg {
	struct marg *ma_nextp;		// Pointer to next macro arg.
	ushort ma_num;			// Argument number.
	ushort ma_flags;		// Variable flags.
	Value *ma_valp;			// Argument value (integer or string).
	} MacArg;

// Macro argument list.
typedef struct {
	ushort mal_count;		// Number of arguments (from caller).
	MacArg *mal_headp;		// Pointer to first argument.
	MacArg *mal_argp;		// Pointer to next argument.
	} MacArgList;

// When the command interpreter needs to get a variable's name, rather than it's value, it is passed back as a VDesc variable
// description structure.  The union field is a pointer into the appropriate variable table.
typedef struct VDesc {
	ushort vd_type;			// Type of variable.
	ushort vd_argnum;		// Macro argument number.
	union {
		UVar *vd_uvp;
		SVar *vd_svp;
		MacArgList *vd_malp;
		} u;			// Pointer to variable in table.
	} VDesc;

// Variable types.
#define VTYP_UNK	0		// Unknown variable type.
#define VTYP_SVAR	1		// System variable.
#define VTYP_GVAR	2		// Global variable.
#define VTYP_LVAR	3		// Local (macro) variable.
#define VTYP_NVAR	4		// Numbered variable (macro argument).

// Script invocation information.
typedef struct {
	char *path;			// Pathname of macro loaded from a file.
	struct Buffer *bufp;		// Buffer pointer to running macro.
	Value *nargp;			// "n" argument.
	MacArgList *malp;		// Macro arguments.
	UVar *uvp;			// Local variables' "stack" pointer.
	} ScriptRun;

#define SRUN_STARTUP	0x0001		// Invoked "at startup" (used for proper exception message, if any).
#define SRUN_PARENS	0x0002		// Invoked in xxx() form.

// Toggle-able values for routines that need directions.
#define FORWARD		0		// Do things in a forward direction.
#define BACKWARD	1		// Do things in a backward direction.

#define BELL		0x07		// A bell character.
#define TAB		0x09		// A tab character.

#define LONGWIDTH	sizeof(long) * 3

// Lexical symbols.
enum e_sym {
	s_any = -1,s_nil,s_nlit,s_slit,s_narg,s_incr,s_decr,s_lparen,s_rparen,s_minus,s_plus,s_not,s_bnot,s_mul,s_div,s_mod,
	s_lsh,s_rsh,s_band,s_bor,s_bxor,s_lt,s_le,s_gt,s_ge,s_eq,s_ne,s_req,s_rne,s_and,s_or,s_hook,s_colon,
	s_assign,s_asadd,s_assub,s_asmul,s_asdiv,s_asmod,s_aslsh,s_asrsh,s_asband,s_asbxor,s_asbor,s_comma,
	s_gvar,s_nvar,s_ident,s_identq,
	kw_and,kw_defn,kw_false,kw_nil,kw_not,kw_or,kw_true,

	// Use bit masks for directive keywords so that they can be grouped by type.
	kw_break =	0x00000040,
	kw_else =	0x00000080,
	kw_elsif =	0x00000100,
	kw_endif =	0x00000200,
	kw_endloop =	0x00000400,
	kw_endmacro =	0x00000800,
	kw_force =	0x00001000,
	kw_if =		0x00002000,
	kw_loop =	0x00004000,
	kw_macro =	0x00008000,
	kw_next =	0x00010000,
	kw_return =	0x00020000,
	kw_until =	0x00040000,
	kw_while =	0x00080000
	};

// Directive types.
#define DLOOPTYPE	(kw_while | kw_until | kw_loop)
#define DBREAKTYPE	(kw_break | kw_next)

// The while, until, and loop directives in the scripting language need to stack references to pending blocks.  These are
// stored in a linked list of the following structure, resolved at end-of-script, and saved in the buffer's b_execp member.
typedef struct LoopBlock {
	struct Line *lb_mark;		// Pointer to while, until, loop, break, or next statement.
	struct Line *lb_jump;		// Pointer to endloop statement.
	struct Line *lb_break;		// Pointer to parent's endloop statement, if any.
	int lb_type;			// Block type (directive id).
	struct LoopBlock *lb_next;	// Next block in list.
	} LoopBlock;

typedef struct {
	char *name;			// Keyword.
	enum e_sym s;			// Id.
	} KeywordInfo;

// Expression statement parsing controls.
typedef struct {
	char *p_clp;			// Beginning of next symbol.
	int p_termch;			// Statement termination character (TKC_COMMENT or TKC_EXPREND).
	enum e_sym p_sym;		// Type of last parsed symbol.
	Value p_tok;			// Text of last parsed symbol.
	Value *p_vgarbp;		// Head of garbage collection list when parsing began.
	} Parse;

// Token characters.
#define TKC_COMMENT	'#'		// Comment.
#define TKC_GVAR	'$'		// Lead-in character for global variable or macro argument.
#define TKC_QUERY	'?'		// Trailing character for a "query" function or macro name.
#define TKC_EXPR	'#'		// Lead-in character for expression interpolation.
#define TKC_EXPRBEG	'{'		// Beginning of interpolated expression in a string.
#define TKC_EXPREND	'}'		// End of interpolated expression in a string.

// Expression evaluation controls and flags used by ge_xxx() functions.
typedef struct {
	Value *en_rp;			// Current expression value.
	uint en_flags;			// Node flags.
	long en_narg;			// "n" argument.
	} ENode;

#define EN_HAVENIL	0x0001		// Found nil (which needs special handling).
#define EN_HAVEBOOL	0x0002		// Found true or false (which need special handling).
#define EN_HAVEIDENT	0x0004		// Found (primary) identifier.
#define EN_HAVEGNVAR	0x0008		// Found '$' variable name.
#define EN_HAVEWHITE	0x0010		// Found white space following identifier.
#define EN_HAVENARG	0x0020		// Found n argument -- function call must follow.
#define EN_CONCAT	0x0040		// Concatenating (bypass bitwise &).

// Command argument and completion/prompt flags.  These are combined with PTRXXX flags for certain function calls so higher
// bits are used.
#define ARG_NOTNULL	0x00001000	// Argument may not be null.
#define ARG_FIRST	0x00002000	// First argument (so no preceding comma).
#define ARG_INT		0x00004000	// Integer argument required.
#define ARG_STR		0x00008000	// String argument required.
#define ARG_PRINT	0x00010000	// Must be one printable character.

#define TERM_NOKECHO	0x00020000	// Don't echo keystrokes.
#define TERM_ONEKEY	0x00040000	// Get one key only.
#define TERM_EVAL	0x00080000	// Evaluate string read from terminal.
#define TERM_C_NOAUTO	0x00100000	// Don't auto-complete; wait for return key.
#define TERM_C_CFAM	0x00200000	// Command, function, alias, or macro name.
#define TERM_C_BUFFER	0x00400000	// Buffer name.
#define TERM_C_FNAME	0x00800000	// Filename (via directory search).
#define TERM_C_BMODE	0x01000000	// Buffer mode name.
#define TERM_C_GMODE	0x02000000	// Global mode name.
#define TERM_C_VAR	0x04000000	// Variable name -- all.
#define TERM_C_SVAR	0x08000000	// Variable name -- excluding constants.

#define TERM_C_MASK	(TERM_C_CFAM | TERM_C_BUFFER | TERM_C_FNAME | TERM_C_BMODE | TERM_C_GMODE | TERM_C_VAR | TERM_C_SVAR)
					// Completion-type mask.

// Return code information from one command loop.
typedef struct {
	short status;			// Most severe status returned from any C function.
	ushort flags;			// Flags.
	char *helpText;			// Command-line help message (-?, -C, -h, or -V switch).
	Value msg;			// Status message, if any.
	} RtnCode;

// Return code flags.
#define RCNOWRAP	0x0001		// Don't wrap SUCCESS message.
#define RCFORCE		0x0002		// Force save of new message of equal severity.
#define RCKEEPMSG	0x0004		// Don't replace any existing message (just change severity).

// Message line information.  span is set to the maximum terminal width + NTERMINP and is allocated at program launch.
typedef struct {
	ushort ttcol;			// Current virtual cursor column (which may be greater than maximum terminal width).
	char *span;			// Buffer holding display size of each character posted (used for backspacing).
	char *spanp;			// Current postion of cursor in span buffer.
	} MsgLine;

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
#define KMSTOP		0		// Not in use.
#define KMPLAY		1		// Playing.
#define KMRECORD	2		// Recording.

// File information.  Any given file is opened, processed, and closed before the next file is dealt with; therefore, the file
// handle (and control variables) can be shared among all files and I/O functions.  Note however, that inpdelim, otpdelim, and
// otpdelimlen are permanent and maintained by the user.
#define FILE_BUFSIZE	32768		// Size of file input buffer.
#define LINE_BUFSIZE	256		// Initial size of line input buffer.
typedef struct {
	char *fname;			// Filename passed to ffropen() or ffwopen().
	int fd;				// File descriptor.
	bool eof;			// End-of-file flag.
	char inpdelim[NDELIM + 1];	// User-assigned input line delimiter(s).
	int idelim1,idelim2;		// Actual input line delimiter(s) for file being read.
	char otpdelim[NDELIM + 1];	// User-assigned output line delimiter(s).
	ushort otpdelimlen;		// Length of user output delimiter string.
	char *odelim;			// Actual output line delimiter(s) for file being written.
	ushort odelimlen;		// Length of actual output delimiter string.
	char *lbuf;			// Pointer to input line buffer (on heap).
	char *lbufp,*lbufz;		// Line buffer pointers.
	char iobuf[FILE_BUFSIZE];	// I/O buffer.
	char *iobufp,*iobufz;		// I/O buffer pointers.
	} FInfo;

// Text insertion style.
#define TXT_INSERT	0x0001
#define TXT_OVERWRT	0x0002
#define TXT_REPLACE	0x0003
#define TXT_LITRTN	0x0010

// Position of dot in a buffer.
typedef struct {
	struct Line *lnp;		// Pointer to Line structure.
	int off;			// Offset of dot in line.
	} Dot;

// Dot mark in a buffer and flags.
typedef struct mark {
	struct mark *mk_nextp;		// Next mark.
	ushort mk_id;			// Mark identifier.
	short mk_force;			// Target line in window for dot.
	Dot mk_dot;			// Dot position.  Mark is invalid if mk_dot.lnp == NULL.
	} Mark;

#define MKOPT_AUTOR	0x0001		// Use mark RMARK if default n and mark WMARK if n < 0; otherwise, prompt with no
					// default (setMark, swapMark).
#define MKOPT_AUTOW	0x0002		// Use mark WMARK if default n or n < 0; otherwise, prompt with no default (markBuf).
#define MKOPT_HARD	0x0004		// Always prompt, no default (deleteMark, gotoMark).
#define MKOPT_VIZ	0x0008		// Mark must exist and be visible (gotoMark, swapMark).
#define MKOPT_EXIST	0x0010		// Mark must exit (deleteMark).
#define MKOPT_CREATE	0x0020		// Mark does not have to exist (setMark, markBuf).
#define MKOPT_QUERY	0x0040		// Query mode.
#define MKOPT_WIND	0x0080		// Mark is a window id.

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
	struct Buffer *w_bufp;		// Buffer displayed in window.
	WindFace w_face;		// Dot position, etc.
	ushort w_id;			// Unique window identifier (mark used to save face before it's buffer is narrowed).
	ushort w_toprow;		// Origin 0 top row of window.
	ushort w_nrows;			// Number of rows in window, excluding mode line.
	short w_force;			// Target line in window for line containing dot.
	ushort w_flags;			// Flags.
#if COLOR
	ushort w_fcolor;		// Current forground color.
	ushort w_bcolor;		// Current background color.
#endif
	} EWindow;

#define WFFORCE		0x0001		// Window needs forced reframe.
#define WFMOVE		0x0002		// Movement from line to line.
#define WFEDIT		0x0004		// Editing within a line.
#define WFHARD		0x0008		// Full screen update needed.
#define WFMODE		0x0010		// Update mode line.
#if COLOR
#define WFCOLOR		0x0020		// Needs a color change.
#endif

// This structure holds the information about each line appearing on the video display.  The redisplay module uses an array
// of virtual display lines.  On systems that do not have direct access to display memory, there is also an array of physical
// display lines used to minimize video updating.  In most cases, these two arrays are unique.
typedef struct Video {
	ushort v_flags;			// Flags.
#if COLOR
	int v_fcolor;			// Current forground color.
	int v_bcolor;			// Current background color.
	int v_rfcolor;			// Requested forground color.
	int v_rbcolor;			// Requested background color.
#endif
	short v_left;			// Left edge of reverse video.
	short v_right;			// Right edge of reverse video.
	char v_text[1];			// Screen data.
	} Video;

#define VFNEW		0x0001		// Contents not meaningful yet.
#define VFCHGD		0x0002		// Changed flag.
#define VFEXT		0x0004		// Extended (beyond terminal width).
#if COLOR
#define VFCOLOR		0x0008		// Color change requested.
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

#define ESRESIZE	0x01		// Resize screen window(s) vertically when screen is frontmost.

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
	Mark b_mroot;			// Dot mark RMARK and list root.
	LoopBlock *b_execp;		// Pointer to compiled macro loop blocks.
	ushort b_nwind;			// Count of windows on buffer.
	ushort b_nexec;			// Count of active executions.
	ushort b_nalias;		// Count of aliases pointing to this (macro) buffer.
	short b_nargs;			// (Macro) argument count (-1 if variable).
	ushort b_flags;			// Flags.
	uint b_modes;			// Buffer modes.
	ushort b_acount;		// Keystroke count until next auto-save.
	ushort b_inpdelimlen;		// Length of input delimiter string.
	char b_inpdelim[NDELIM + 1];	// Record delimiters used to read buffer.
	char b_otpdelim[NDELIM + 1];	// Record delimiters used to write buffer.
	char *b_fname;			// File name.
	char b_bname[NBNAME + 1];	// Buffer name.
	} Buffer;

// Buffer flags.
#define BFACTIVE	0x0001		// Active buffer (file was read).
#define BFCHGD		0x0002		// Changed since last write.
#define BFHIDDEN	0x0004		// Hidden buffer.
#define BFMACRO		0x0008		// Buffer is a macro.
#define BFPREPROC	0x0010		// (Script) buffer has been preprocessed.
#define BFTRUNC		0x0020		// Buffer was truncated when read.
#define BFNARROW	0x0040		// Buffer is narrowed.
#define BFUNKFACE	0x0080		// Buffer face is in an unknown state -- don't use (buffer was narrowed).
#define BFQSAVE		0x0100		// Buffer was saved via quickExit().

#define BSYSLEAD	'.'		// Leading character of system (internal) buffer.

// Buffer creation flags.
#define CRBQUERY	0x0000		// Look-up only (do not create).
#define CRBCREATE	0x0001		// Create buffer if non-existent.
#define CRBUNIQ		0x0002		// (Force) create buffer with unique name.
#define CRBFILE		0x0004		// Derive buffer name from filename.

// Buffer clearing flags.
#define CLBIGNCHGD	0x0001		// Ignore BFCHGD (buffer changed) flag.
#define CLBUNNARROW	0x0002		// Force-clear narrowed buffer (unnarrow first).
#define CLBCLFNAME	0x0004		// Clear associated filename, if any.
#define CLBMULTI	0x0008		// Processing multiple buffers.

// Buffer rendering flags.
#define RENDRESET	0x0001		// Move dot to beginning of buffer when displaying in a new window.
#define RENDALTML	0x0002		// Use alternate mode line when doing a real pop-up.
#define RENDBOOL	0x0004		// Return boolean argument in addition to buffer name.
#define RENDTRUE	0x0008		// Return true boolean argument; otherwise, false.

// Descriptor for global and buffer modes.
typedef struct {
	char *name;			// Name of mode in lower case.
	char *mlname;			// Name displayed on mode line.
	uint mask;			// Bit mask.
	char *desc;			// Description.
	} ModeSpec;

// Global mode bit masks.
#define MDASAVE		0x0001		// Auto-save mode.
#define MDBAK		0x0002		// File backup mode.
#define MDCLOB		0x0004		// Macro-clobber mode.
#define MDESC8		0x0008		// Escape 8-bit mode.
#define MDEXACT		0x0010		// Exact matching for searches.
#define MDHSCRL		0x0020		// Horizontal-scroll-all mode.
#define MDMSG		0x0040		// Message line display mode.
#define MDNOUPD		0x0080		// Suppress screen update mode.
#define MDREGEXP	0x0100		// Regular expresions in search.
#define MDSAFE		0x0200		// Safe file save mode.
#define MDWKDIR		0x0400		// Working directory display mode.

// Buffer mode bit masks -- language.
#define MDC		0x0001		// C indentation and fence match.
#define MDMEMACS	0x0002		// MightEMacs indentation.
#define MDPERL		0x0004		// Perl indentation and fence match.
#define MDRUBY		0x0008		// Ruby indentation and fence match.
#define MDSHELL		0x0010		// Shell indentation and fence match.

// Buffer mode bit masks -- non-language.
#define MDCOL		0x0020		// Column position display mode.
#define MDLINE		0x0040		// Line number display mode.
#define MDOVER		0x0080		// Overwrite mode.
#define MDRDONLY	0x0100		// Read-only buffer.
#define MDREPL		0x0200		// Replace mode.
#define MDWRAP		0x0400		// Word wrap.
#define MDXINDT		0x0800		// Extra fence indentation mode.

// Mode masks.
#define MDGLOBAL	0x0fff		// All possible global modes.
#define MDBUFFER	0x0fff		// All possible buffer modes.
#define MDGRP_OVER	(MDOVER | MDREPL)
#define MDGRP_LANG	(MDC | MDMEMACS | MDPERL | MDRUBY | MDSHELL)

// Global and buffer mode table offsets (for variable table).
#define MDO_ASAVE	0
#define MDO_BAK		1
#define MDO_CLOB	2
#define MDO_ESC8	3
#define MDO_EXACT	4
#define MDO_HSCRL	5
#define MDO_MSG		6
#define MDO_NOUPD	7
#define MDO_REGEXP	8
#define MDO_SAFE	9
#define MDO_WKDIR	10

#define MDO_C		0
#define MDO_COL		1
#define MDO_LINE	2
#define MDO_MEMACS	3
#define MDO_OVER	4
#define MDO_PERL	5
#define MDO_RDONLY	6
#define MDO_REPL	7
#define MDO_RUBY	8
#define MDO_SHELL	9
#define MDO_WRAP	10
#define MDO_XINDT	11

// Structure for non-buffer modes.
typedef struct {
	uint flags;			// Mode flags (bit masks).
	char *cmdlabel;			// Unique portion of command name and showBuffers label.
	} ModeRec;

#define MDR_GLOBAL	0		// "Global" table index.
#define MDR_SHOW	1		// "Show" (global) table index.
#define MDR_DEFAULT	2		// "Default" table index.

// Structure for "i" variable.
typedef struct {
	int i;				// Current value.
	int inc;			// Increment to add to i.
	Value format;			// sprintf() format string.
	} IVar;

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

#define lforw(lnp)	((lnp)->l_nextp)
#define lback(lnp)	((lnp)->l_prevp)
#define lgetc(lnp,n)	((lnp)->l_text[(n)])
#define lputc(lnp,n,c)	((lnp)->l_text[(n)] = (c))
#define lused(lnp)	((lnp)->l_used)
#define lsize(lnp)	((lnp)->l_size)
#define ltext(lnp)	((lnp)->l_text)

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
#if COLOR
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
#if COLOR
#define TTforg		(*term.t_setfor)
#define TTbacg		(*term.t_setback)
#endif

// Terminal key entry information.
typedef struct {
	ushort lastkseq;		// Last key sequence (extended key) returned from getkseq().
	bool uselast;			// Use lastkseq for next key?
	ushort chpending;		// Character pushed back via tungetc().
	bool ispending;			// Character pending (chpending valid)?
	ushort lastflag;		// Flags, last command. 
	ushort thisflag;		// Flags, this command.
	} KeyEntry;

// Flags for thisflag and lastflag.
#define CFVMOV		0x0001		// Last command was a line up/down.
#define CFKILL		0x0002		// Last command was a kill.
#define CFDEL		0x0004		// Last command was a delete.
#define CFNMOV		0x0008		// Last (yank) command did not move point.
#define CFTRAV		0x0010		// Last command was a traverse.
#define CFYANK		0x0020		// Last command was a yank.

// Flags for ldelete().
#define DFKILL		0x0001		// Kill operation (save text in kill ring).
#define DFDEL		0x0002		// Delete operation (save text in "undelete" buffer).

// Structure and flags for the command-function table.  Flags CFEDIT and CFSPECARGS may not both be set, and the number of
// CFNUMn, CFNILn, and CFBOOLn flags must match CFMAXARGS.
typedef struct {
	char *cf_name;			// Name of command.
	uint cf_flags;			// Flags.
	short cf_minArgs;		// Minimum number of required arguments with default n arg.
	short cf_maxArgs;		// Maximum number of arguments allowed.  No maximum if negative.
	int (*cf_func)(Value *rp,int n);// C function that command or system function is bound to.
	char *cf_usage;			// Usage text.
	char *cf_desc;			// Short description.
	} CmdFunc;

#define CFFUNC		0x00000001	// Is system function.
#define CFHIDDEN	0x00000002	// Hidden: for internal use only.
#define CFPREFIX	0x00000004	// Prefix command (meta, ^C, ^H, and ^X).
#define CFBIND1		0x00000008	// Is bound to a single key (use getkey() in bindcmd() and elsewhere).
#define CFUNIQ		0x00000010	// Can't have more than one binding.
#define CFEDIT		0x00000020	// Modifies current buffer.
#define CFPERM		0x00000040	// Must have one or more bindings at all times.
#define CFTERM		0x00000080	// Terminal (interactive) only -- not recognized in a script.
#define CFNCOUNT	0x00000100	// N argument is purely a repeat count.
#define CFSPECARGS	0x00000200	// Needs special argument processing (never skipped).
#define CFADDLARG	0x00000400	// Takes additional argument if n arg is not the default.
#define CFNOARGS	0x00000800	// Takes no arguments if n arg is not the default.
#define CFSHRTLOAD	0x00001000	// Load one fewer argument than usual in feval().
#define CFNOLOAD	0x00002000	// Load no arguments in feval().
#define CFNUM1		0x00004000	// First argument is numeric.
#define CFNUM2		0x00008000	// Second argument is numeric.
#define CFNUM3		0x00010000	// Third argument is numeric.
#define CFNIL1		0x00020000	// First (string) argument may be nil.
#define CFNIL2		0x00040000	// Second (string) argument may be nil.
#define CFNIL3		0x00080000	// Third (string) argument may be nil.
#define CFBOOL1		0x00100000	// First (string) argument may be Boolean (true or false).
#define CFBOOL2		0x00200000	// Second (string) argument may be Boolean (true or false).
#define CFBOOL3		0x00400000	// Third (string) argument may be Boolean (true or false).
#define CFANY		0x00800000	// Any argument can be numeric or string.

#define CFMAXARGS	3		// Maximum number of arguments that could be loaded by feval() for any system function;
					// that is, maximum value of cf_maxArgs in cftab where CFFUNC is set, neither CFNOLOAD
					// or CFSPECARGS is set, and cf_maxArgs > 0.
// Forward.
struct alias;

// Pointer structure for places where either a command, function, alias, buffer, or macro is allowed.
typedef struct {
	ushort p_type;			// Pointer type.
	union {
		CmdFunc *p_cfp;		// Pointer into the function table.
		struct alias *p_aliasp;	// Alias pointer.
		Buffer *p_bufp;		// Buffer pointer.
		void *p_voidp;		// For generic pointer comparisons.
		} u;
	} CFABPtr;

// Pointer types.  Set to different bits so that can be used as selector masks in function calls.
#define PTRNUL		0x0000		// NULL pointer.
#define PTRCMD		0x0001		// Command-function pointer -- command
#define PTRPSEUDO	0x0002		// Command-function pointer -- pseudo-command
#define PTRFUNC		0x0004		// Command-function pointer -- non-command.
#define PTRALIAS_C	0x0008		// Alias pointer to a command.
#define PTRALIAS_F	0x0010		// Alias pointer to a function.
#define PTRALIAS_M	0x0020		// Alias pointer to a macro.
#define PTRBUF		0x0040		// Buffer pointer.
#define PTRMACRO	0x0080		// Macro (buffer) pointer.

#define PTRCMDTYP	(PTRCMD | PTRPSEUDO)
#define PTRALIAS	(PTRALIAS_C | PTRALIAS_F | PTRALIAS_M)
#define PTRCFAM		(PTRCMD | PTRFUNC | PTRALIAS | PTRMACRO)
#define PTRANY		(PTRCMD | PTRPSEUDO | PTRFUNC | PTRALIAS | PTRBUF | PTRMACRO)

// Structure for the alias list.
typedef struct alias {
	struct alias *a_nextp;		// Pointer to next alias.
	ushort a_type;			// Alias type (PTRALIAS_X).
	CFABPtr a_cfab;			// Command, function, or macro pointer.
	char a_name[1];			// Name of alias.
	} Alias;

// Structure for the command/function/alias/macro list, which contains pointers to all the current command, function, alias, and
// macro names.  Used for completion lookups.
typedef struct cfamrec {
	struct cfamrec *fr_nextp;	// Pointer to next CFAM record.
	ushort fr_type;			// Pointer type (PTRXXX).
	char *fr_name;			// Name of command, function, alias, or macro.
	} CFAMRec;

// Operation types.
#define OPDELETE	-1		// Delete an item.
#define OPQUERY		0		// Look for an item.
#define OPCREATE	1		// Create an item.

// Descriptor for a key binding.
typedef struct KeyDesc {
	ushort k_code;			// Key code.
	CFABPtr k_cfab;			// Command or macro to execute.
	} KeyDesc;

// Key binding array (vector) for one 7-bit key of a key sequence.
typedef KeyDesc KeyVect[128];

// Control object for walking through the key binding table.
typedef struct {
	KeyVect *kvp;			// Key vector pointer (initially NULL).
	KeyDesc *kdp;			// Pointer to next binding.
	} KeyWalk;

// The editor holds deleted and copied text chunks in an array of Kill buffers (the kill ring).  Each buffer is logically a
// stream of ASCII characters; however, due to its unpredicatable size, it is implemented as a linked list of KillBuf buffers.  
typedef struct KillBuf {
	struct KillBuf *kl_next;	// Pointer to next chunk; NULL if last.
	char kl_chunk[KBLOCK];		// Deleted or copied text.
	} KillBuf;

typedef struct {
	KillBuf *kbufh;			// Kill buffer header pointer.
	KillBuf *kbufp;			// Current kill buffer chunk pointer.
	int kskip;			// Number of bytes to skip in first kill chunk.
	int kused;			// Number of bytes used in last kill chunk.
	} Kill;

// Structure for the hook table and indices into hooktab array for each type.  Hooks are commands or macros that are invoked
// automatically when certain events occur during the editing session.
typedef struct {
	char *h_name;			// (Terse) name of hook.
	char *h_desc;			// Short description.
	CFABPtr h_cfab;			// Command or macro to execute.
	} HookRec;

#define HKCHDIR		0		// Change directory hook.
#define HKENTRBUF	1		// Enter buffer hook.
#define HKEXITBUF	2		// Exit buffer hook.
#define HKHELP		3		// Help hook.
#define HKMODE		4		// Mode hook.
#define HKPOSTKEY	5		// Post-key hook.
#define HKPREKEY	6		// Pre-key hook.
#define HKREAD		7		// Read file or change buffer filename hook.
#define HKWRAP		8		// Word wrap hook.
#define HKWRITE		9		// Write file hook.

// Command-function ids.
enum cfid {
	cf_abort,cf_about,cf_abs,cf_alias,cf_alterBufMode,cf_alterDefMode,cf_alterGlobalMode,cf_alterShowMode,cf_appendFile,
	cf_backChar,cf_backLine,cf_backPage,cf_backPageNext,cf_backPagePrev,cf_backTab,cf_backWord,cf_basename,cf_beep,
	cf_beginBuf,cf_beginKeyMacro,cf_beginLine,cf_beginText,cf_beginWhite,cf_bindKey,cf_binding,cf_bufBoundQ,cf_chDir,
	cf_chr,cf_clearBuf,cf_clearKillRing,cf_clearMsg,cf_copyFencedText,cf_copyLine,cf_copyRegion,cf_copyToBreak,cf_copyWord,
#if WORDCOUNT
	cf_countWords,
#endif
	cf_cycleKillRing,cf_definedQ,cf_deleteAlias,cf_deleteBackChar,cf_deleteBlankLines,cf_deleteBuf,cf_deleteFencedText,
	cf_deleteForwChar,cf_deleteLine,cf_deleteMacro,cf_deleteMark,cf_deleteRegion,cf_deleteScreen,cf_deleteTab,
	cf_deleteToBreak,cf_deleteWhite,cf_deleteWind,cf_deleteWord,cf_detabLine,cf_dirname,cf_dupLine,cf_endBuf,cf_endKeyMacro,
	cf_endLine,cf_endWhite,cf_endWord,cf_entabLine,cf_env,cf_eval,cf_exit,cf_findFile,cf_forwChar,cf_forwLine,cf_forwPage,
	cf_forwPageNext,cf_forwPagePrev,cf_forwTab,cf_forwWord,cf_getKey,cf_gotoFence,cf_gotoLine,cf_gotoMark,cf_growWind,
	cf_help,cf_hideBuf,cf_huntBack,cf_huntForw,cf_includeQ,cf_indentRegion,cf_index,cf_insert,cf_insertBuf,cf_insertFile,
	cf_insertLineI,cf_insertPipe,cf_insertSpace,cf_inserti,cf_intQ,cf_join,cf_joinLines,cf_joinWind,cf_killFencedText,
	cf_killLine,cf_killRegion,cf_killToBreak,cf_killWord,cf_lcLine,cf_lcRegion,cf_lcString,cf_lcWord,cf_length,cf_let,
	cf_markBuf,cf_match,cf_metaPrefix,cf_moveWindDown,cf_moveWindUp,cf_narrowBuf,cf_negativeArg,cf_newScreen,cf_newline,
	cf_newlineI,cf_nextArg,cf_nextBuf,cf_nextScreen,cf_nextWind,cf_nilQ,cf_notice,cf_nullQ,cf_numericQ,cf_onlyWind,
	cf_openLine,cf_ord,cf_outdentRegion,cf_overwrite,cf_pathname,cf_pause,cf_pipeBuf,cf_pop,cf_prefix1,cf_prefix2,
	cf_prefix3,cf_prevBuf,cf_prevScreen,cf_prevWind,cf_print,cf_prompt,cf_push,cf_queryReplace,cf_quickExit,cf_quote,
	cf_quoteChar,cf_rand,cf_readBuf,cf_readFile,cf_readPipe,cf_redrawScreen,cf_replace,cf_replaceText,cf_resetTerm,
	cf_resizeWind,cf_restoreBuf,cf_restoreWind,cf_run,cf_saveBuf,cf_saveFile,cf_saveWind,cf_scratchBuf,cf_searchBack,
	cf_searchForw,cf_selectBuf,cf_setBufFile,cf_setBufName,cf_setHook,cf_setMark,cf_setWrapCol,cf_seti,cf_shQuote,cf_shell,
	cf_shellCmd,cf_shift,cf_showBindings,cf_showBuffers,cf_showFunctions,cf_showHooks,cf_showKey,cf_showKillRing,
	cf_showMarks,cf_showModes,
#if MMDEBUG & DEBUG_SHOWRE
	cf_showRegexp,
#endif
	cf_showScreens,cf_showVariables,cf_shrinkWind,cf_space,cf_splitWind,cf_sprintf,cf_statQ,cf_stringQ,cf_stringFit,
	cf_strip,cf_sub,cf_subLine,cf_subString,cf_suspend,cf_swapMark,cf_tab,cf_tcString,cf_tcWord,cf_toInt,cf_toString,cf_tr,
	cf_traverseLine,cf_trimLine,cf_truncBuf,cf_ucLine,cf_ucRegion,cf_ucString,cf_ucWord,cf_unbindKey,cf_unchangeBuf,
	cf_undelete,cf_unhideBuf,cf_universalArg,cf_unshift,cf_updateScreen,cf_viewFile,cf_voidQ,cf_whence,cf_widenBuf,
	cf_wordCharQ,cf_wrapLine,cf_wrapWord,cf_writeBuf,cf_writeFile,cf_xPathname,cf_xeqBuf,cf_xeqFile,cf_xeqKeyMacro,cf_yank,
	cf_yankPop
	};

// Object for core keys bound to special commands (like "abort").  These are maintained in global variable "corekeys" in
// addition to the binding table for fast access.  Note: indices must be in descending, alphabetical order (by command name) so
// that corekeys is initialized properly in edinit1().
typedef struct {
	ushort ek;			// Extended key.
	enum cfid id;			// Command id (index into cftab).
	} CoreKey;

#define CK_UNARG	0		// Universal argument (repeat) key.
#define CK_QUOTE	1		// Quote key.
#define CK_NEGARG	2		// Negative argument (repeat) key.
#define CK_ABORT	3		// Abort key.
#define NCOREKEYS	4		// Number of cache keys.

// Regular expression macro definitions.
//
// HICHAR - 1 is the largest character we will deal with.
#define HICHAR		256

// Metacharacter element types for searches (in the mc_type member of MetaChar below).  MCE_NIL is used for both search and
// replacement.
#define MCE_NIL		0		// Like the '\0' for strings.
#define MCE_LITCHAR	1		// Literal character.
#define MCE_ANY		2		// Any character but newline.
#define MCE_CCL		3		// Character class.
#define MCE_NCCL	4		// Negated character class.
#define MCE_BOL		5		// Beginning of line.
#define MCE_EOL		6		// End of line.
#define MCE_BOS		7		// Beginning of string.
#define MCE_EOSALT	8		// End of string or just before trailing newline.
#define MCE_EOS		9		// End of string.
#define MCE_WORDBND	10		// Word boundary.
#define MCE_GRPBEGIN	11		// Beginning of group.
#define MCE_GRPEND	12		// End of group.

// Metacharacter element types for replacements (in the mc_type member of ReplMetaChar below).
#define MCE_LITSTRING	13		// Literal string.
#define MCE_GROUP	14		// String of group match.
#define MCE_MATCH	15		// String of entire match.

// Element type masks.
#define MCE_CLOSURE	0x0100		// Element has closure.
#define MCE_MINCLOSURE	0x0200		// Match the minimum number possible; otherwise, maximum.
#define MCE_NOT		0x0400		// Negate meaning of element type (e.g., not a word boundary).

#define MCE_BASETYPE	0x00ff

// Metacharacters.
#define MC_ANY		'.'		// 'Any' character (except newline).
#define MC_CCLBEGIN	'['		// Beginning of character class.
#define MC_NCCL		'^'		// Negated character class.
#define MC_CCLRANGE	'-'		// Range indicator in character class.
#define MC_CCLEND	']'		// End of character class.
#define MC_BOL		'^'		// Beginning of line.
#define MC_EOL		'$'		// End of line.
#define MC_CLOSURE0	'*'		// Closure - zero or more characters.
#define MC_CLOSURE1	'+'		// Closure - one or more characters.
#define MC_CLOSURE01	'?'		// Closure - zero or one characters, and modifier to match minimum number possible.
#define MC_CLBEGIN	'{'		// Closure - beginning of count.
#define MC_CLEND	'}'		// Closure - end of count.
#define MC_DITTO	'&'		// Matched string (in replacement pattern).
#define MC_GRPBEGIN	'('		// Beginning of group (no backslash prefix).
#define MC_GRPEND	')'		// End of group (no backslash prefix).
#define MC_ESC		'\\'		// Escape - suppress meta-meaning.

#define MC_BOS		'A'		// Beginning of string (\A).
#define MC_EOSALT	'Z'		// End of string or just before trailing newline (\Z).
#define MC_EOS		'z'		// End of string (\z).
#define MC_WORDBND	'b'		// Word boundary (\b).
#define MC_NWORDBND	'B'		// Not a word boundary (\B).
#define MC_TAB		't'		// Tab character (\t).
#define MC_CR		'r'		// Carriage return (\r).
#define MC_NL		'n'		// Newline (\n).
#define MC_FF		'f'		// Form feed (\f).
#define MC_DIGIT	'd'		// Digit [0-9] (\d).
#define MC_NDIGIT	'D'		// Non-digit [^0-9] (\D).
#define MC_LETTER	'l'		// Letter [a-zA-Z] (\l).
#define MC_NLETTER	'L'		// Non-letter [^a-zA-Z] (\L).
#define MC_SPACE	's'		// Space character [ \t\r\n\f] (\s).
#define MC_NSPACE	'S'		// Non-space character [^ \t\r\n\f] (\S).
#define MC_WORD		'w'		// Word character [a-zA-Z0-9_] (\w).
#define MC_NWORD	'W'		// Non-word character [^a-zA-Z0-9_] (\W).

#define OPTCH_BEGIN	':'		// Beginning of options (not part of pattern).
#define OPTCH_EXACT	'e'		// Case-sensitive matches.
#define OPTCH_IGNORE	'i'		// Ignore case.
#define OPTCH_MULTI	'm'		// Multiline mode.
#define OPTCH_PLAIN	'p'		// Plain-text mode.
#define OPTCH_REGEXP	'r'		// Regexp mode.
#define OPTCH_N		6		// Number of option characters.

#define MAXGROUPS	10		// Maximum number of RE groups + 1 (#0 reserved for entire match).
#define BIT(n)		(0x80 >> (n))	// An 8-bit integer with one bit set.

// Bit map element type.
typedef char EBitMap[HICHAR >> 3];

// String "dot" for RE scanning.
typedef struct {
	char *strp0;			// Initial string pointer (needed for '^' matching).
	char *strp;			// Current string pointer during scan.
	} StrDot;

// Scan "dot" and type definitions.
typedef struct {
	ushort type;
	union {
		Dot bd;			// Buffer dot.
		StrDot sd;		// String dot.
		} u;
	} ScanDot;

#define BUFDOT		0
#define STRDOT		1

// String match-location object.
typedef struct {
	StrDot sd;
	long len;
	} StrLoc;

// Match text.
typedef union {
	Region reg;			// Buffer location of the beginning of the match and length.
	StrLoc str;			// String pointer to the beginning of the match and length.
	} MatchLoc;

// Structure for saving search results for group matches.  "elen" and "ml" are used only during scan; "matchp" is the scan
// result (or NULL if N/A) and remains until the next scan.
typedef struct {
	int elen;			// Length of text matched by whole RE pattern through end of group.
	MatchLoc ml;			// Text in scan object matched by RE group.  ml.reg.r_size and ml.str.len are initially
					// the negated length of text matched by whole RE pattern up to beginning of group.
					// The actual size of the group is computed by adding "elen".
	Value *matchp;			// Pointer to heap copy of matched string.
	} GrpInfo;

// Meta-character structure for a search pattern element.
typedef struct {
	ushort mc_type;
	union {
		int lchar;
		GrpInfo *ginfo;
		EBitMap *cclmap;
		} u;
	struct {
		short min,max;		// Closure minimum and maximum.
		} cl;
	} MetaChar;

// Meta-character structure for a replacment pattern element.
typedef struct {
	ushort mc_type;
	union {
		int grpnum;
		char *rstr;
		} u;
	} ReplMetaChar;

// Pattern-matching control variables.  Note that grouping items in "mcpat" point to elements in "groups".
typedef struct {
	ushort flags;			// RE flags (below).
	uint ssize;			// Size of search arrays.
	uint rsize;			// Size of replacement arrays.
	Value *matchp;			// Pointer to heap copy of matched string (taken from groups[0].matchp).
	int grpct;			// Number of groups in RE pattern.
	int patlen;			// Length of search pattern (RE and non-RE) without trailing option characters.
	char *pat;			// Forward search pattern (RE and non-RE) without trailing option characters.
	char *rpat;			// Replacement pattern (RE and non-RE).
	MetaChar *mcpat;		// Forward RE pattern.
	MetaChar *bmcpat;		// Backward (reversed) RE pattern.
	ReplMetaChar *rmcpat;		// Replacement RE pattern.
	GrpInfo groups[MAXGROUPS];	// Descriptors for each RE group string found in search.  Group 0 contains the location
					// of entire matched text in source object (buffer or string).
	} Match;

// Search control variables -- for search and replace commands.
typedef struct {
	ushort sdelim;			// Search prompt terminator.
	char *bpat;			// Backward (reversed) search pattern (RE and non-RE).
	int fdelta1[HICHAR];		// Forward non-RE delta1 table.
	int bdelta1[HICHAR];		// Backward non-RE delta1 table.
	int *fdelta2;			// Forward non-RE delta2 table.
	int *bdelta2;			// Backward non-RE delta2 table.
	Match m;			// Match information.
	} SearchInfo;

#define SREGICAL	0x0001		// Metacharacters found in search string.
#define	RREGICAL	0x0002		// Metacharacters found in replacement string.

// The following pattern options must be different than FORWARD and BACKWARD bit(s).
#define SOPT_EXACT	0x0004		// Case-sensitive comparisons.
#define SOPT_IGNORE	0x0008		// Ignore case in comparisons.
#define SOPT_MULTI	0x0010		// Multiline mode ('.' and [^...] match '\n').
#define SOPT_PLAIN	0x0020		// Plain-text mode.
#define SOPT_REGEXP	0x0040		// Regexp mode.
#define SOPT_ALL	(SOPT_EXACT | SOPT_IGNORE | SOPT_MULTI | SOPT_PLAIN | SOPT_REGEXP)

#define SCPL_EXACT	0x0100		// Plain text compile done in Exact mode?

#define SXEQ_EXACT	0x0200		// Exact mode in effect?
#define SXEQ_ALL	SXEQ_EXACT
