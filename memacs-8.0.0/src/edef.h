// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// edef.h	Structure and preprocesser definitions for MightEMacs.

// GeekLib include file.
#include "geeklib.h"

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
#define DEBUG_CAM	0x0004		// Show CAM pointer type in "showBindings" display.
#define DEBUG_NARROW	0x0008		// Dump buffer info to log file in narrowBuf().
#define DEBUG_KILLRING	0x0010		// Include dispkill() function.
#define DEBUG_BWINDCT	0x0020		// Display buffer's window count in "showBuffers" display.
#define DEBUG_SHOWRE	0x0040		// Include showRegExp command.
#define DEBUG_TOKEN	0x0080		// Dump token-parsing info to log file.
#define DEBUG_VALUE	0x0100		// Dump Value processing info to log file.
#define DEBUG_MARG	0x0200		// Dump macro-argument info to log file.
#define DEBUG_SCRIPT	0x0400		// Write macro lines to log file.
#define DEBUG_EXPR	0x0800		// Write expression-parsing info to log file.
#define DEBUG_PPBUF	0x1000		// Dump script preprocessor blocks to log file and exit.

#define VDebug		0
#define MMDEBUG		0		// No debugging code.
//#define MMDEBUG		DEBUG_LOGFILE
//#define MMDEBUG		(DEBUG_SCRIPT | DEBUG_TOKEN | DEBUG_EXPR | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_SCRIPT | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_VALUE | DEBUG_SCRIPT | DEBUG_TOKEN | DEBUG_EXPR | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_TOKEN | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_MARG | DEBUG_LOGFILE)
//#define MMDEBUG		(DEBUG_PPBUF | DEBUG_LOGFILE)

#if MMDEBUG & DEBUG_MARG
#define dobuf		dobufDebug
#endif

// Program Identification ...
//
// Scripts can query these via the $EditorName, $Version, and $OS system variables.
#define PROGNAME	"MightEMacs"
#define VERSION		"8.0.0"
#if HPUX8 || HPUX9
#	define OSNAME	"HP/UX"
#elif REDHAT
#	define OSNAME	"Red Hat Linux"
#elif OSX
#	define OSNAME	"OS X"
#elif SOLARIS
#	define OSNAME	"Solaris"
#else
#	define OSNAME	"Unix"
#endif

/***** BEGIN CUSTOMIZATIONS *****/

// Terminal output definitions -- [Set one of these].
#define TT_TERMCAP	1		// Use TERMCAP.
#define TT_CURSES	0		// Use CURSES (not working).

// Terminal size definitions -- [Set any].
#define TT_MAXCOLS	240		// Maximum number of columns.
#define TT_MAXROWS	80		// Maximum number of rows.

// Language text options -- [Set one of these].
#define ENGLISH		1		// Default.
#define SPANISH		0		// If any non-English macro is set, must translate english.h to the corresponding
					// header file (e.g., spanish.h) and edit elang.h to include it.
// Configuration options -- [Set any].
#define TYPEAH		1		// Include $KeyPending and type-ahead checking (which skips unnecessary screen updates).
#define NULREGERR	0		// Consider a null region an error.
#define WORDCOUNT	0		// Include code for "countWords" command (deprecated).
#define MLSCALED	0		// Include "%f" (scaled integer) formatting option in mlprintf() (deprecated).
#define VISMAC		0		// Update display during keyboard macro execution.
#define REVSTA		1		// Status line appears in reverse video.
#define COLOR		0		// Include code for color commands and windows (not working).
#define VIZBELL		0		// Use a visible BELL (instead of an audible one) if it exists.
#define KMDELIMS	":;,\"'"	// Keyboard macro encoding delimiters ($keyMacro), in order of preference.
#define BACKUP_EXT	".bak"		// Backup file extension.
#define SCRIPT_EXT	".mm"		// Script file extension.
#define USER_STARTUP	".memacs"	// User start-up file (in HOME directory).
#define SITE_STARTUP	"memacs.mm"	// Site start-up file.
#define MMPATH_NAME	"MMPATH"	// Shell environmental variable name.
#define MMPATH_DEFAULT	":/usr/local/etc/memacs.d:/usr/local/etc"	// Default search directories.
#define LOGFILE		"memacs.log"	// Log file (for debugging).

// Limits -- [Set any].
#define MAXTAB		240		// Maximum hard or soft tab size.
#define NBUFN		24		// Number of bytes, buffer name.
#define NTERMINP	256		// Number of bytes, terminal input.
#define NPAT		128		// Number of bytes, pattern.
#define NBLOCK		32		// Number of bytes, line block chunks.
#define KBLOCK		256		// Number of bytes, kill buffer chunks.
#define NRING		30		// Number of buffers in kill ring.
#define NVSIZE		32		// Maximum number of characters in a user variable name (including prefix).
#define NKBDM		256		// Maximum number of keystrokes, keyboard macro.
#define NMARKS		10		// Number of marks per window.
#define NASAVE		220		// Number of keystrokes before auto-save -- initial value.
#define IFNESTMAX	15		// Maximum if/loop nesting level in scripts.
#define LOOPMAX		2500		// Default maximum number of macro loop iterations allowed.
#define FPAUSE		26		// Default time in centiseconds to pause for fence matching.
#define VJUMPMIN	10		// Minimum vertical jump size (percentage).
#define JUMPMAX		49		// Maximum horizontal or vertical jump size (percentage).
#if COLOR
#define NCOLORS		16		// Number of supported colors.
#define NPALETTE	48		// Size of palette string (palstr).
#endif
#define NPATHINP	(NTERMINP < MaxPathname ? NTERMINP : MaxPathname)

/***** END CUSTOMIZATIONS *****/

// Internal constants.
#define NPREFIX		4		// Number of prefix keys (META, ^C, ^H, and ^X).
#define NDELIM		2		// Number of bytes, input and output record delimiters.
#define NWORK		80		// Number of bytes, work buffer.

// Codes for true, false, and nil pseudo types (used for Value object comparisons).
#define VNIL		-1
#define VFALSE		0
#define VTRUE		1

// Operation flags used at runtime (in "opflags" global variable).
#define OPVTOPEN	0x0001		// Virtual terminal open?
#define OPEVAL		0x0002		// Evaluate expressions?
#define OPHAVEEOL	0x0004		// Does clear to EOL exist?
#define OPHAVEREV	0x0008		// Does reverse video exist?
#define OPSTARTUP	0x0010		// In pre-edit-loop state?
#define OPSCRIPT	0x0020		// Script execution flag.
#define OPPARENS	0x0040		// Command, alias, macro, or system function invoked in xxx() form.
#define OPSCREDRAW	0x0080		// Clear and redraw screen if true.
#define OPWORDLST	0x0100		// Word list enabled flag.

// Buffer operation flags used by bufop().
#define BOPSETFLAG	1		// Set buffer flag.
#define BOPCLRFLAG	2		// Clear buffer flag.
#define BOPMOVEDOT	3		// Move dot in buffer.

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
#define CPREF		0x0400		// "C" prefix (^C) flag.
#define HPREF		0x0800		// "H" prefix (^H) flag.
#define XPREF		0x1000		// "X" prefix (^X) flag.
#define SHFT		0x2000		// Shift flag (for function keys).
#define FKEY		0x4000		// Special key flag (function key).
#define PREFIX		(META | CPREF | HPREF | XPREF)	// Prefix key mask.
#define KEYSEQ		(META | CPREF | HPREF | XPREF | FKEY)	// Key sequence mask.

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

// Message line character output options.
enum e_viz {
	vz_raw = -1,vz_show,vz_wrap
	};

// Message line print flags.
#define MLHOME		0x01		// Move cursor to beginning of message line before display.
#define MLFORCE		0x02		// Force output (ignore 'msg' global mode).
#define MLWRAP		0x04		// Wrap message [like this].

// Completion and prompt flags.
#define CMPL_CAM	0x0001		// Command, alias, or macro name.
#define CMPL_BUFFER	0x0002		// Buffer name.
#define CMPL_FILENAME	0x0004		// Filename (via directory search).
#define CMPL_MASK	0x0007		// Mask for types (above).

#define CMPL_NOAUTO	0x0008		// Don't auto-complete; wait for return key.

// User variable definition.
typedef struct uvar {
	struct uvar *uv_nextp;		// Pointer to next variable.
	char uv_name[NVSIZE + 1];	// Name of user variable.
	ushort uv_flags;		// Variable flags.
	Value *uv_vp;			// Value (integer or string).
	} UVar;

// Definition of system variables.
enum svarid {
	// Immutables.
	sv_ArgCount,sv_BufCount,sv_BufFlagActive,sv_BufFlagChanged,sv_BufFlagHidden,sv_BufFlagMacro,sv_BufFlagNarrowed,
	sv_BufFlagPreprocd,sv_BufFlagTruncated,sv_BufInpDelim,sv_BufList,sv_BufOtpDelim,sv_BufSize,sv_Date,sv_EditorName,
	sv_EditorVersion,
#if TYPEAHEAD
	sv_KeyPending,
#endif
	sv_KillText,sv_Language,sv_LineLen,sv_Match,sv_ModeAutoSave,sv_ModeBackup,sv_ModeC,sv_ModeClobber,sv_ModeColDisp,
	sv_ModeEsc8Bit,sv_ModeExact,sv_ModeExtraIndent,sv_ModeHorzScroll,sv_ModeKeyEcho,sv_ModeLineDisp,sv_ModeMEMacs,
	sv_ModeMsgDisp,sv_ModeNoUpdate,sv_ModeOver,sv_ModePerl,sv_ModeReadFirst,sv_ModeReadOnly,sv_ModeRegExp,sv_ModeReplace,
	sv_ModeRuby,sv_ModeSafeSave,sv_ModeShell,sv_ModeWorkDir,sv_ModeWrap,sv_OS,sv_RegionText,sv_ReturnMsg,sv_RunFile,
	sv_RunName,sv_TermCols,sv_TermRows,sv_WindCount,sv_WorkDir,

	// Mutables.
	sv_argIndex,sv_autoSave,sv_bufFile,sv_bufFlags,sv_bufLineNum,sv_bufModes,sv_bufName,sv_defModes,
#if COLOR
	sv_desktopColor,
#endif
	sv_enterBufHook,sv_execPath,sv_exitBufHook,sv_fencePause,sv_globalModes,sv_hardTabSize,sv_helpHook,sv_horzJump,
	sv_horzScrollCol,sv_inpDelim,sv_keyMacro,sv_lastKeySeq,sv_lineChar,sv_lineCol,sv_lineOffset,sv_lineText,sv_loopMax,
	sv_modeHook,sv_otpDelim,sv_pageOverlap,
#if COLOR
	sv_palette,
#endif
	sv_postKeyHook,sv_preKeyHook,sv_randNumSeed,sv_readHook,sv_replace,sv_screenNum,sv_search,sv_searchDelim,sv_showModes,
	sv_softTabSize,sv_travJumpSize,sv_vertJump,sv_windLineNum,sv_windNum,sv_windSize,sv_wordChars,sv_wrapCol,sv_wrapHook,
	sv_writeHook
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
#define V_ESCDELIM	0x0010		// Use escape character as input delimiter when prompting for a value.

// Macro argument.
typedef struct marg {
	struct marg *ma_nextp;		// Pointer to next macro arg.
	ushort ma_num;			// Argument number.
	ushort ma_flags;		// Variable flags.
	Value *ma_vp;			// Argument value (integer or string).
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
	Value *vp;			// "n" argument.
	MacArgList *malp;		// Macro arguments.
	UVar *uvp;			// Local variables' "stack" pointer.
	} ScriptRun;

#define SRUN_STARTUP	0x0001		// Invoked "at startup" (used for proper exception message, if any).
#define SRUN_PARENS	0x0002		// Invoked in xxx() form.

// Directive definitions.  Ids are set to bit masks so that multiple types (like loops) can be tested for easily in the code.
typedef struct {
	const char *name;
	ushort id;
	} DirName;

#define DIF		0x0001
#define DELSIF		0x0002
#define DELSE		0x0004
#define DENDIF		0x0008
#define DRETURN		0x0010
#define DMACRO		0x0020
#define DENDMACRO	0x0040
#define DWHILE		0x0080
#define DUNTIL		0x0100
#define DLOOP		0x0200
#define DENDLOOP	0x0400
#define DBREAK		0x0800
#define DNEXT		0x1000
#define DFORCE		0x2000

#define DLOOPTYPE	(DWHILE | DUNTIL | DLOOP)
#define DBREAKTYPE	(DBREAK | DNEXT)

// The !while, !until, and !loop directives in the scripting language need to stack references to pending blocks.  These are
// stored in a linked list of the following structure, resolved at end-of-script, and saved in the buffer's b_execp member.
typedef struct LoopBlock {
	struct Line *lb_mark;		// Pointer to !while, !until, !loop, !break, or !next statement.
	struct Line *lb_jump;		// Pointer to !endloop statement.
	struct Line *lb_break;		// Pointer to parent's !endloop statement, if any.
	int lb_type;			// Block type (directive id).
	struct LoopBlock *lb_next;	// Next block in list.
	} LoopBlock;

// Toggle-able values for routines that need directions.
#define PTBEGIN		0		// Leave the point at the beginning on search.
#define PTEND		1		// Leave the point at the end on search.

#define FORWARD		0		// Do things in a forward direction.
#define BACKWARD	1		// Do things in a backward direction.

#define BELL		0x07		// A bell character.
#define TAB		0x09		// A tab character.

#define LONGWIDTH	sizeof(long) * 3

// Lexical symbols.
enum e_sym {
	s_any = -1,s_nil,s_nlit,s_slit,s_narg,s_incr,s_decr,s_lparen,s_rparen,s_minus,s_plus,s_not,s_bnot,s_mul,s_div,s_mod,
	s_lsh,s_rsh,s_band,s_bor,s_bxor,s_lt,s_le,s_gt,s_ge,
#if 0
	s_cmp,
#endif
	s_eq,s_ne,s_req,s_rne,s_and,s_or,s_hook,s_colon,
	s_assign,s_asadd,s_assub,s_asmul,s_asdiv,s_asmod,s_aslsh,s_asrsh,s_asband,s_asbxor,s_asbor,s_comma,
	s_gvar,s_nvar,s_ident,s_identq,
	kw_and,kw_defn,kw_false,kw_nil,kw_not,kw_or,kw_true
#if 0
	kw_and,kw_cmp,kw_defn,kw_false,kw_nil,kw_not,kw_or,kw_true
	kw_break,kw_defn,kw_else,kw_elsif,kw_endif,kw_endloop,kw_endmacro,kw_false,kw_force,kw_if,kw_loop,kw_macro,kw_next,
	kw_nil,kw_not,kw_return,kw_true,kw_until,kw_while
#endif
	};

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

#define EN_TFN		0x0001		// Found true, false, or nil (which need special handling).
#define EN_IDENT	0x0002		// Found (primary) identifier.
#define EN_GNVAR	0x0004		// Found '$' variable name.
#define EN_WHITE	0x0008		// Found white space following identifier.
#define EN_NEEDFUNC	0x0010		// Found n argument -- function call must follow.
#define EN_CONCAT	0x0020		// Concatenating (bypass bitwise &).

// Command argument flags.
#define ARG_NOTNULL	0x0001		// Argument may not be null.
#define ARG_ONEKEY	0x0002		// Get one key only.
#define ARG_EVAL	0x0004		// Evaluate string read from terminal.
#define ARG_FIRST	0x0008		// First argument (so no preceding comma).
#define ARG_INT		0x0010		// Integer argument required.
#define ARG_STR		0x0020		// String argument required.

// Return code information from one command loop.
typedef struct {
	short status;			// Most severe status returned from any C function.
	ushort flags;			// Flags.
	char *clhelptext;		// Command-line help message (-?, -h, or -V switch).
	Value msg;			// Status message, if any.
	} RtnCode;

// Return code flags.
#define RCNOWRAP	0x0001		// Don't wrap SUCCESS message.
#define RCFORCE		0x0002		// Force save of new message of equal severity.
#define RCKEEPMSG	0x0004		// Don't replace any existing message (just change severity).

// Message line information.  span is based on the terminal width and allocated at program launch.
typedef struct {
	int ttcol;			// Current cursor column.
	ushort buflen;			// Length of span buffer.
	char *span;			// Buffer holding display size of each character posted (used for backspacing).
	char *spanptr;			// Pointer to span.
	} MsgLine;

// Sample string buffer used for error reporting -- allocated at program launch according to the terminal width.
typedef struct {
	char *buf;			// Buffer for sample string, often ending in "...".
	ushort buflen;			// Size of buffer (allocated from heap).
	ushort smallsize;		// Small sample size.
	} SampBuf;

// Keys bound to special commands used primarily during user input for various purposes.  These are maintained here in
// addition to the binding table for fast access.  Note: must be in descending, alphabetical order (by command name) so that
// they are initialized properly in edinit1().
typedef struct {
	ushort unarg;			// Universal argument (repeat) key.
	ushort quote;			// Quote key.
	ushort negarg;			// Negative argument (repeat) key.
	ushort abort;			// Abort key.
	} CoreKeys;

// Keyboard macro information.
typedef struct {
	ushort *km_slotp;		// Pointer to next slot in buffer.
	ushort *km_endp;		// Pointer to end of the last macro recorded.
	int km_state;			// Current state.
	int km_n;			// Number of repetitions (0 = infinite).
	ushort km_buf[NKBDM];		// Macro buffer.
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
enum e_textedit {
	txt_replace = -1,txt_overwrite,txt_insert
	};

// Position of dot in a buffer.
typedef struct {
	struct Line *lnp;		// Pointer to Line structure.
	int off;			// Offset of dot in line.
	} Dot;

// Dot mark.
typedef struct {
	short mk_force;			// Target line in window for line containing dot.
	Dot mk_dot;			// Dot position.
	} Mark;

// Settings that determine a window's "face"; that is, the location of dot in the buffer and in the window.
typedef struct {
	struct Line *wf_toplnp;		// Pointer to top line of window.
	Dot wf_dot;			// Dot position.
	Mark wf_mark[NMARKS];		// Dot marks.
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
// window.  The terminolgy is different in Emacs ...
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
// linked list sorted by buffer name to improve searching and for the buffers' listing.  There is a safe store for a window's
// top line, dot, marks, and first display column so that a window can be cleanly restored to the same point in a buffer (if
// possible) when switching among them.  "b_nwind" is the number of windows (on any screen) that a buffer is associated with. 
// Thus if the value is zero, the buffer is not being displayed.  The text for a buffer is kept in a circularly linked list of
// lines with a pointer to the header line in "b_hdrlnp" (which is sometimes referred to in the code as the "magic" line).  The
// header line is just a placeholder; it always exists but doesn't contain any text.  When a buffer is first created or cleared,
// the header line will point to itself, both forward and backward.  A buffer may be "inactive" which means the file associated
// with it has not been read in yet.  The file is read at "use buffer" time.
typedef struct Buffer {
	struct Buffer *b_prevp;		// Pointer to previous buffer.
	struct Buffer *b_nextp;		// Pointer to next buffer.
	WindFace b_face;		// Dot position, etc.
	struct Line *b_hdrlnp;		// Pointer to header (magic) line.
	struct Line *b_ntoplnp;		// Pointer to narrowed top text.
	struct Line *b_nbotlnp;		// Pointer to narrowed bottom text.
	LoopBlock *b_execp;		// Pointer to compiled macro loop blocks.
	ushort b_nwind;			// Count of windows on buffer.
	ushort b_nexec;			// Count of active executions.
	ushort b_nalias;		// Count of aliases pointing to this buffer (macro).
	short b_nargs;			// Maximum (macro) argument count (-1 if variable).
	ushort b_flags;			// Flags.
	uint b_modes;			// Buffer modes.
	ushort b_acount;		// Keystroke count until next auto-save.
	ushort b_inpdelimlen;		// Length of input delimiter string.
	char b_inpdelim[NDELIM + 1];	// Record delimiters used to read buffer.
	char b_otpdelim[NDELIM + 1];	// Record delimiters used to write buffer.
	char *b_fname;			// File name.
	char b_bname[NBUFN + 1];	// Buffer name.
	} Buffer;

// Buffer flags.
#define BFACTIVE	0x0001		// Active buffer (file was read).
#define BFCHGD		0x0002		// Changed since last write.
#define BFHIDDEN	0x0004		// Hidden buffer.
#define BFMACRO		0x0008		// Buffer is a macro.
#define BFPREPROC	0x0010		// (Script) buffer has been preprocessed.
#define BFTRUNC		0x0020		// Buffer was truncated when read.
#define BFNARROW	0x0040		// Buffer is narrowed.
#define BFQSAVE		0x0080		// Buffer was saved via quickExit().

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

// Buffer rendering flags.
#define RENDRESET	0x0001		// Move dot to beginning of buffer when displaying in a new window.
#define RENDALTML	0x0002		// Use alternate mode line when doing a real pop-up.
#define RENDBOOL	0x0004		// Return boolean argument in addition to buffer name.
#define RENDTRUE	0x0008		// Return true boolean argument; otherwise, false.

// Descriptor for global and buffer modes.
typedef struct {
	char *name;			// Name of mode in lower case.
	char *mlname;			// Name displayed on mode line.
	ushort code;			// (Letter) shortcut.
	uint mask;			// Bit mask.
	} ModeSpec;

// Global mode bit masks.
#define MDASAVE		0x0001		// Auto-save mode.
#define MDBAK		0x0002		// File backup mode.
#define MDCLOB		0x0004		// Macro-clobber mode.
#define MDESC8		0x0008		// Escape 8-bit mode.
#define MDEXACT		0x0010		// Exact matching for searches.
#define MDHSCRL		0x0020		// Horizontal-scroll-all mode.
#define MDKECHO		0x0040		// Key echo mode.
#define MDMSG		0x0080		// Message line display mode.
#define MDNOUPD		0x0100		// Suppress screen update mode.
#define MDRD1ST		0x0200		// Read first file at startup.
#define MDREGEXP	0x0400		// Regular expresions in search.
#define MDSAFE		0x0800		// Safe file save mode.
#define MDWKDIR		0x1000		// Working directory display mode.

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
#define MDGLOBAL	0x1fff		// All possible global modes.
#define MDBUFFER	0x0fff		// All possible buffer modes.
#define MDGRP_OVER	(MDOVER | MDREPL)
#define MDGRP_LANG	(MDC | MDMEMACS | MDPERL | MDRUBY | MDSHELL)

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
	int (*t_getchar)(int *);	// Get character from keyboard.
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
	int lastread;			// Last key read from tgetc().
	int lastkseq;			// Last key sequence (extended character) returned from getkseq().
	bool uselast;			// Use lastkseq for next key?
	int chpending;			// Character pushed back via tungetc().
	bool ispending;			// Character pending (chpending valid)?
	ushort lastflag;		// Flags, last command. 
	ushort thisflag;		// Flags, this command.
	} KeyEntry;

// Flags for thisflag and lastflag.
#define CFVMOV		0x0001		// Last command was a line up/down.
#define CFKILL		0x0002		// Last command was a kill.
#define CFNMOV		0x0004		// Last (yank) command did not move point.
#define CFTRAV		0x0008		// Last command was a traverse.
#define CFYANK		0x0010		// Last command was a yank.

// Structure and flags for the command/system-function table.  Flags CFEDIT and CFSPECARGS may not both be set.
typedef struct {
	char *cf_name;			// Name of command.
	ushort cf_flags;		// Flags.
	short cf_nargs;			// Number of required arguments.  If value is negative, count is variable with
					// a minimum of -cf_nargs - 1.
	int (*cf_func)(Value *rp,int n);// C function that command or system function is bound to.
	char *cf_usage;			// Usage text.
	char *cf_desc;			// Short description.
	} CmdFunc;

#define CFFUNC		0x0001		// Is system function.
#define CFHIDDEN	0x0002		// Hidden: for internal use only.
#define CFPREFIX	0x0004		// Prefix command (meta, ^C, ^H, and ^X).
#define CFBIND1		0x0008		// Is bound to a single key (use getkey() in bindcmd() and elsewhere).
#define CFUNIQ		0x0010		// Can't have more than one binding.
#define CFEDIT		0x0020		// Modifies current buffer.
#define CFPERM		0x0040		// Must have one or more bindings (at all times).
#define CFTERM		0x0080		// Terminal (interactive) only -- not recognized in a macro.
#define CFNCOUNT	0x0100		// N argument is purely a repeat count.
#define CFSPECARGS	0x0200		// Needs special argument processing (never skipped).
#define CFADDLARG	0x0400		// Takes additional argument if n arg is not the default.
#define CFNOARGS	0x0800		// Takes no arguments if n arg is not the default.
#define CFNUM1		0x1000		// First argument is numeric.
#define CFNUM2		0x2000		// Second argument is numeric.
#define CFNUM3		0x4000		// Third argument is numeric.
#define CFANY		0x8000		// Any argument can be numeric or string.

// Forward.
struct alias;

// Pointer structure for places where either a function, alias, buffer, or macro is allowed.
typedef struct {
	ushort p_type;			// Pointer type.
	union {
		CmdFunc *p_cfp;		// Pointer into the function table.
		struct alias *p_aliasp;	// Alias pointer.
		Buffer *p_bufp;		// Buffer pointer.
		void *p_voidp;		// For generic pointer comparisons.
		} u;
	} FABPtr;

// Pointer types.  Set to different bits so that can be used as selector masks in function calls.
#define PTRNUL		0x0000		// NULL pointer.
#define PTRCMD		0x0001		// Command-function pointer -- command
#define PTRFUNC		0x0002		// Command-function pointer -- non-command.
#define PTRALIAS	0x0004		// Alias pointer.
#define PTRBUF		0x0008		// Buffer pointer.
#define PTRMACRO	0x0010		// Macro (buffer) pointer.
#define PTRFAM		(PTRCMD | PTRFUNC | PTRALIAS | PTRMACRO)
#define PTRANY		(PTRCMD | PTRFUNC | PTRALIAS | PTRBUF | PTRMACRO)

// Structure for the alias list.
typedef struct alias {
	struct alias *a_nextp;		// Pointer to next alias.
	FABPtr a_fab;			// Command or macro pointer.
	char a_name[1];			// Name of alias.
	} Alias;

// Structure for the command/alias/macro list, which contains pointers to all the current command, alias, and macro names.
// Used for completion lookups.
typedef struct camrec {
	struct camrec *cr_nextp;	// Pointer to next CAM record.
	ushort cr_type;			// Pointer type (PTRXXX).
	char *cr_name;			// Name of command, alias, or macro.
	} CAMRec;

// Operation types.
#define OPDELETE	-1		// Delete an item.
#define OPQUERY		0		// Look for an item.
#define OPCREATE	1		// Create an item.

// Descriptor for a key binding.
typedef struct KeyDesc {
	struct KeyDesc *k_nextp;	// Pointer to next entry.
	ushort k_code;			// Key code.
	FABPtr k_fab;			// Command or macro to execute.
	} KeyDesc;

// Header record for the key binding lists.
typedef struct {
	KeyDesc *kh_headp;		// Head of linked list.
	KeyDesc *kh_tailp;		// Tail of linked list.
	} KeyHdr;

// The editor holds deleted and copied text chunks in a linked list of KillBuf buffers.  The kill buffer is logically a
// stream of ASCII characters; however, due to its unpredicatable size, it gets implemented as a linked list of chunks.
typedef struct KillBuf {
	struct KillBuf *kl_next;	// Pointer to next chunk; NULL if last.
	char kl_chunk[KBLOCK];		// Deleted text.
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
	char *h_name;			// Name of hook.
	FABPtr h_fab;			// Command or macro to execute.
	} HookRec;

#define HKENTRBUF	0		// Enter buffer hook.
#define HKEXITBUF	1		// Exit buffer hook.
#define HKHELP		2		// Help hook.
#define HKMODE		3		// Mode hook.
#define HKPOSTKEY	4		// Post-key hook.
#define HKPREKEY	5		// Pre-key hook.
#define HKREAD		6		// Read file or change buffer filename hook.
#define HKWRITE		7		// Write file hook.
#define HKWRAP		8		// Word wrap hook.

// Command-function ids.
enum cfid {
	cf_abort,cf_about,cf_abs,cf_alias,cf_alterBufMode,cf_alterDefMode,cf_alterGlobalMode,cf_alterShowMode,cf_appendFile,
	cf_backChar,cf_backLine,cf_backPage,cf_backPageNext,cf_backPagePrev,cf_backTab,cf_backWord,cf_basename,cf_beep,
	cf_beginBuf,cf_beginKeyMacro,cf_beginLine,cf_beginText,cf_beginWhite,cf_bindKey,cf_binding,cf_bufBoundQ,cf_bufWind,
	cf_cPrefix,cf_chDir,cf_chr,cf_clearBuf,cf_clearKillRing,cf_clearMark,cf_clearMsg,cf_copyFencedText,cf_copyLine,
	cf_copyRegion,cf_copyToBreak,cf_copyWord,
#if WORDCOUNT
	cf_countWords,
#endif
	cf_cycleKillRing,cf_definedQ,cf_deleteAlias,cf_deleteBackChar,cf_deleteBlankLines,cf_deleteBuf,cf_deleteFencedText,
	cf_deleteForwChar,cf_deleteLine,cf_deleteMacro,cf_deleteRegion,cf_deleteScreen,cf_deleteTab,cf_deleteToBreak,
	cf_deleteWhite,cf_deleteWind,cf_deleteWord,cf_detabLine,cf_dirname,cf_endBuf,cf_endKeyMacro,cf_endLine,cf_endWhite,
	cf_endWord,cf_entabLine,cf_env,cf_eval,cf_exit,cf_fileExistsQ,cf_findFile,cf_forwChar,cf_forwLine,cf_forwPage,
	cf_forwPageNext,cf_forwPagePrev,cf_forwTab,cf_forwWord,cf_getKey,cf_gotoFence,cf_gotoLine,cf_gotoMark,cf_growWind,
	cf_hPrefix,cf_help,cf_hideBuf,cf_huntBack,cf_huntForw,cf_includeQ,cf_indentRegion,cf_index,cf_insert,cf_insertBuf,
	cf_insertFile,cf_insertLineI,cf_insertPipe,cf_insertSpace,cf_inserti,cf_intQ,cf_join,cf_joinLines,cf_joinWind,
	cf_killFencedText,cf_killLine,cf_killRegion,cf_killToBreak,cf_killWord,cf_lcLine,cf_lcRegion,cf_lcString,cf_lcWord,
	cf_length,cf_let,cf_markBuf,cf_match,cf_metaPrefix,cf_moveWindDown,cf_moveWindUp,cf_narrowBuf,cf_negativeArg,
	cf_newScreen,cf_newline,cf_newlineI,cf_nextArg,cf_nextBuf,cf_nextScreen,cf_nextWind,cf_nilQ,cf_notice,cf_nullQ,
	cf_numericQ,cf_onlyWind,cf_openLine,cf_ord,cf_outdentRegion,cf_overwrite,cf_pad,cf_pathname,cf_pause,cf_pipeBuf,cf_pop,
	cf_prevBuf,cf_prevScreen,cf_prevWind,cf_print,cf_prompt,cf_push,cf_queryReplace,cf_quickExit,cf_quote,cf_quoteChar,
	cf_rand,cf_readBuf,cf_readFile,cf_readPipe,cf_redrawScreen,cf_replace,cf_replaceText,cf_resetTerm,cf_resizeWind,
	cf_restoreBuf,cf_restoreWind,cf_reverse,cf_run,cf_saveBuf,cf_saveFile,cf_saveWind,cf_scratchBuf,cf_searchBack,
	cf_searchForw,cf_selectBuf,cf_setBufFile,cf_setBufName,cf_setMark,cf_setWrapCol,cf_seti,cf_shQuote,cf_shell,cf_shellCmd,
	cf_shift,cf_showBindings,cf_showBuffers,cf_showFunctions,cf_showKey,cf_showKillRing,
#if MMDEBUG & DEBUG_SHOWRE
	cf_showRegExp,
#endif
	cf_showScreens,cf_showVariables,cf_shrinkWind,cf_space,cf_splitWind,cf_sprintf,cf_stringQ,cf_stringFit,cf_stringLit,
	cf_strip,cf_sub,cf_subLine,cf_subString,cf_suspend,cf_swapMark,cf_tab,cf_tcString,cf_tcWord,cf_toInt,cf_toString,cf_tr,
	cf_traverseLine,cf_trimLine,cf_truncBuf,cf_ucLine,cf_ucRegion,cf_ucString,cf_ucWord,cf_unbindKey,cf_unchangeBuf,
	cf_unhideBuf,cf_universalArg,cf_unshift,cf_updateScreen,cf_viewFile,cf_whence,cf_widenBuf,cf_wrapLine,cf_wrapWord,
	cf_writeBuf,cf_writeFile,cf_xPathname,cf_xPrefix,cf_xeqBuf,cf_xeqFile,cf_xeqKeyMacro,cf_yank,cf_yankPop
	};

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
#define MCE_GRPBEGIN	7		// Beginning of group.
#define MCE_GRPEND	8		// End of group.

// Metacharacter element types for replacements (in the mc_type member of ReplMetaChar below).
#define MCE_LITSTRING	9		// Literal string.
#define MCE_GROUP	10		// String of group match.
#define MCE_DITTO	11		// String of entire match.

// Closure masks.
#define MCE_CLOSURE0	0x0100		// Zero or more characters.
#define MCE_CLOSURE1	0x0200		// One or more characters.
#define MCE_CLOSURE01	0x0400		// Zero or one character.
#define MCE_MINCLOSURE	0x0800		// Match the minimum number possible; otherwise, maximum.

#define MCE_ALLCLOSURE	(MCE_CLOSURE0 | MCE_CLOSURE1 | MCE_CLOSURE01)
#define MCE_BASETYPE	~(MCE_ALLCLOSURE | MCE_MINCLOSURE)

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
#define MC_DITTO	'&'		// Matched string (in replacement pattern).
#define MC_GRPBEGIN	'('		// Start of group (no backslash prefix).
#define MC_GRPEND	')'		// End of group (no backslash prefix).
#define MC_ESC		'\\'		// Escape - suppress meta-meaning.
#define MC_OPT		':'		// Beginning of options (not part of pattern).

#define MCOPT_MULTI	'm'		// Multiline mode.

#define MAXGROUPS	10		// Maximum number of RE groups + 1 (#0 reserved for matched string).
#define BIT(n)		(1 << (n))	// An integer with one bit set.

// Bit map element type.
typedef char EBitMap[HICHAR >> 3];

// Structure for saving search results for group matches.
typedef struct {
	int elen;			// Length of text matched by whole RE pattern through end of group.
	Region region;			// Text matched by RE group.  region.r_size is initially the negated length of
					// text matched by whole RE pattern up to beginning of group.  The actual size
					// of the group is computed by adding "elen".
	} GrpInfo;

// Meta-character structure for a search pattern element.
typedef struct {
	ushort mc_type;
	union {
		int lchar;
		GrpInfo *ginfo;
		EBitMap *cclmap;
		} u;
	} MetaChar;

// Meta-character structure for a replacment pattern element.
typedef struct {
	ushort mc_type;
	union {
		int grpnum;
		char *rstr;
		} u;
	} ReplMetaChar;

// Search control variables -- kept in a structure so that they are all in one place.
typedef struct {
	ushort flags;			// RE flags (below).
	int sdelim;			// Search prompt terminator.
	Dot matchdot;			// Buffer line and offset of the beginning of the match.
	int matchlen;			// Match length.
	char *patmatch;			// Pointer to copy of matched string (from buffer) in grpmatch[0].
	int grpct;			// Number of groups in RE pattern.
	int patlen;			// Length of search pattern (RE and non-RE).
	char pat[NPAT + 1];		// Forward search pattern (RE and non-RE).
	char bpat[NPAT + 1];		// Backward (reversed) search pattern (RE and non-RE).
	char rpat[NPAT + 1];		// Replacement pattern (RE and non-RE).
	int fdelta1[HICHAR];		// Forward non-RE delta1 table.
	int bdelta1[HICHAR];		// Backward non-RE delta1 table.
	int fdelta2[NPAT];		// Forward non-RE delta2 table.
	int bdelta2[NPAT];		// Backward non-RE delta2 table.
	MetaChar mcpat[NPAT];		// Forward RE pattern.
	MetaChar bmcpat[NPAT];		// Backward (reversed) RE pattern.
	ReplMetaChar rmcpat[NPAT];	// Replacement RE pattern.
	char *grpmatch[MAXGROUPS];	// Holds match and "(...)" RE group strings found in search.
	} SearchInfo;

#define SREGICAL	0x0001		// Metacharacters in search string.
#define	RREGICAL	0x0002		// Metacharacters in replacement string.
#define SMULTILINE	0x0004		// RE search multiline mode ('.' matches '\r').
