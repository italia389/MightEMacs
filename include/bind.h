// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// bind.h	Default key-to-command bindings for MightEMacs.

// Command binding table.
//
// This table is *roughly* in ASCII order, left to right across the characters of the command.
//
// Printable ASCII chars: SPC ! " # $ % & ' ( ) * + , - . / 0-9 : ; < = > ? @ A-Z [ \ ] ^ _ ` a-z { | } ~
//
//			Delete		Kill		Copy
//	Word		C-\		C-h C-\		C-c C-\
//	ToBreak		C-k		C-h C-k		C-c C-k
//	Line		C-l		C-h C-l		C-c C-l
//	Region		C-w		C-h C-w		C-c C-w
//	FencedRegion	C-x '		C-h '		C-c '

// Define ncurses constants.
#define SpecFKeySyms	"BCDEFHLNPRSVZ"	// Unique function key symbols in keyList table (declared in unix.c).
#define SpecFKeyCount	(sizeof(SpecFKeySyms) - 1)	// Number of special function keys (left-arrow, enter, etc.) supported.
#define NumFKeySyms	"0123456789abcdefghijklmnopqrstuvwxyz"	// Must be at least (KEY_DL - KEY_F0) / 2 in length (32).
#define NumFKeyCount	20		// Number of numbered function keys (F1, F2, etc.) supported.

#if NumFKeyCount < 10 || NumFKeyCount > (KEY_DL - KEY_F0) / 2
#error "NumFKeyCount macro value out of range"
#endif

#define KeyVectCount	5		// Number of key binding vectors (no prefix, Meta, ^X, ^C, and ^H).
// Define number of slots in a key binding vector (for one prefix key or no prefix key).  This is equal to all 7-bit characters
// (128) + S-TAB (1) + special function key characters - FNB, FNC, etc. (SpecFKeyCount) + numbered function key characters -
// FN1..FN9, FNa..FN? (NumFKeyCount) + shifted special function key characters (SpecFKeyCount) + shifted numbered function key
// characters (NumFKeyCount), in that order.
#define KeyVectSlots	(128 + 1 + SpecFKeyCount * 2 + NumFKeyCount * 2)

// Key prefixes.
#define Ctrl		0x0100		// Control flag.  *** Also undef'd and redefined in unix.c ***
#define Meta		0x0200		// Meta flag.
#define Pref1		0x0400		// Prefix1 (^X) flag.
#define CtrlX		Pref1
#define Pref2		0x0800		// Prefix2 (^C) flag.
#define CtrlC		Pref2
#define Pref3		0x1000		// Prefix3 (^H) flag.
#define CtrlH		Pref3
#define Shift		0x2000		// Shift flag (for tab and function keys).
#define FKey		0x4000		// Special key flag (function key).
#define Prefix		(Meta | Pref1 | Pref2 | Pref3)	// Prefix key mask.

#define RtnKey		(Ctrl | 'M')	// "return" key as an extended key.
#define AltRtnKey	(Ctrl | 'J')	// newline as an extended key (alternate return key).
#define TermResizeKey	(FKey | 'R')	// Dummy key used for ncurses "terminal resize" event.

// Descriptor for a key binding.
typedef ushort ExtKey;			// Type of extended key.
typedef struct {
	ExtKey extKey;			// Extended key.
	ExecPtr cmd;			// Command to execute.
	} KeyBind;

// Key binding array (vector) for one key prefix (or no prefix).
typedef KeyBind KeyVect[KeyVectSlots];

// Built-in/default key binding (which is copied to appropriate list at startup).
typedef struct {
	ExtKey extKey;			// Extended key.
	CmdFunc *pCmdFunc;		// Command-function table pointer.
	} BuiltinKey;

// Object for core keys bound to special commands (like "abort").  These are maintained in global variable "coreKeys" in
// addition to the binding table for fast access.  Note: indices must be in ascending, alphabetical order (by command name) so
// that coreKeys is initialized properly in editInit1().
typedef struct {
	ExtKey extKey;			// Extended key.
	CmdFuncId id;			// Command id (index into cmdFuncTable).
	} CoreKey;

#define CK_Abort	0		// Abort key.
#define CK_NegArg	1		// Negative argument (repeat) key.
#define CK_Quote	2		// Quote key.
#define CK_UnivArg	3		// Universal argument (repeat) key.
#define CoreKeyCount	4		// Number of cached keys.

// Macro information for recording and executing a list of keystrokes.  Macros consist of two parts: a name and value (key
// sequence).  They are saved in a hash with name as key and value (in ExtKey [] array form) as node value.  Macros are recorded
// into this structure (staging area), then given a name and saved in the hash.  In order for a macro to be played or
// manipulated, it must exist here; that is, just been recorded or loaded here from the hash or by a command or function.
typedef struct {
	Datum name;			// Name of macro, or nil if none.
	ExtKey *pMacBuf;		// Macro buffer (allocated from heap).
	ExtKey *pMacSlot;		// Pointer to next slot in buffer (during recording and playback).
	ExtKey *pMacEnd;		// Pointer to end of last macro recorded, or NULL if none.
	uint size;			// Current size of pMacBuf in keys.
	} Macro;
typedef short Shortcut;			// Type of a macro shortcut character.
typedef struct {
	OrdHash index;			// Macro table.
	Hash shortcuts;			// Macro shortcut hash.
	Macro *pActiveMacro;		// Pointer to current macro, for execution (may be null).
	Macro *pStageMacro;		// Pointer to "staging" macro, for recording or loading.
	Macro macro1, macro2;		// Current and staging macros.
	long n;				// Requested number of playback repetitions (0 == infinite).
	ushort state;			// Current state.
	} MacroHdr;

// Macro states.
#define MacStopped	0		// Stopped (idle).
#define MacPlaying	1		// Playing.
#define MacRecording	2		// Recording.

// Macro searching and operation flags.
#define MF_Name		0x0001		// Include name when encoding, or search by name, otherwise value.
#define MF_Required	0x0002		// Error if macro not found.

// Terminal key entry information.
typedef struct {
	ExtKey lastKeySeq;		// Last key sequence (extended key) returned from getKeySeq().
	ExtKey charPending;		// Character pushed back via ungetkey().
	bool useLast;			// Use lastKeySeq for next key?
	bool isPending;			// Character pending (charPending valid)?
	ushort prevFlags;		// Flags, previous command. 
	ushort curFlags;		// Flags, current command.
	} KeyEntry;

// State flags for curFlags and prevFlags.
#define SF_VertMove	0x0001		// Last command was a vertical (line) move.
#define SF_Trav		0x0002		// Last command was a traverse.
#define SF_Kill		0x0004		// Last command was a kill.
#define SF_Del		0x0008		// Last command was a delete.
#define SF_Yank		0x0010		// Last command was a yank.
#define SF_Undel	0x0020		// Last command was an undelete.
#define SF_NoMove	0x0040		// Last yank or undelete command did not move point.

// External function declarations.
extern int beginMacro(Datum *pRtnVal, long n, Datum **args);
extern int binding(Datum *pRtnVal, Datum **args, bool haveLit);
extern int bindKey(Datum *pRtnVal, long n, Datum **args);
extern KeyBind *bindSlot(ExtKey extKey);
extern ExtKey ctoek(short c);
extern int delMacro(Datum *pRtnVal, long n, Datum **args);
extern int delShortcut(Datum *pRtnVal, long n, Datum **args);
extern int editMacro(Datum *pRtnVal, long n, Datum **args);
extern char *ektos(char *dest, ExtKey extKey, bool escTermAttr);
extern int endMacro(Datum *pRtnVal, long n, Datum **args);
extern short ektoc(ExtKey extKey, bool extend);
extern int encodeMacro(Datum *pDest, Macro *pMacro);
extern int fGetKey(Datum *pRtnVal, long n, Datum **args);
extern KeyBind *findBindPtr(ExecPtr *pExec);
extern int finishMacro(KeyBind *pKeyBind);
extern KeyBind *getBind(ExtKey extKey);
extern const char *getBindCmd(KeyBind *pKeyBind);
extern int getKeyBind(const char *prompt, long n, Datum **args, ExtKey *pExtKey);
extern int getkey(bool msgLine, ExtKey *pExtKey, bool saveKey);
extern int getKeySeq(bool msgLine, ExtKey *pExtKey, KeyBind **ppKeyBind, bool saveKey);
extern int getMacro(Datum *pRtnVal, long n, Datum **args);
extern int growMacro(Macro *pMacro);
extern int initBindings(void);
extern bool isActiveMacro(const char *name);
extern int loadMacro(Datum *pRtnVal, long n, Datum **args);
extern int loadStage(HashNode *pNode);
extern int renameMacro(Datum *pRtnVal, long n, Datum **args);
extern int selectMacro(Datum *pRtnVal, long n, Datum **args);
extern int setShortcut(Datum *pRtnVal, long n, Datum **args);
extern int showBindings(Datum *pRtnVal, long n, Datum **args);
extern int showKey(Datum *pRtnVal, long n, Datum **args);
extern int showMacros(Datum *pRtnVal, long n, Datum **args);
extern int stoek(ExtKey *pExtKey, const char *keyCode);
extern int tgetc(bool msgLine, ExtKey *pExtKey);
extern void unbind(KeyBind *pKeyBind);
extern int unbindKey(Datum *pRtnVal, long n, Datum **args);
extern void ungetkey(ExtKey extKey);
extern int xeqMacro(Datum *pRtnVal, long n, Datum **args);

#ifdef BindData

// **** For bind.c ****

// Global variables.
CoreKey coreKeys[CoreKeyCount];		// Cache of frequently-used keys.
const char fKeySyms[] = SpecFKeySyms NumFKeySyms;
					// Characters for special function keys (FNB, FNC, etc.) and numbered function keys
					// (FN1, FN2, etc.).  NumFKeyCount of the latter characters are used.
KeyEntry keyEntry = {0, 0, false, false, 0, 0};
					// Terminal key entry variables.
// Key binding table.  Contains an array of KeyVect objects for each prefix key plus one more for all unprefixed bindings (the
// first entry).  Each KeyVect array contains "KeyVectSlots" KeyBind objects -- a slot for every possible 7-bit character plus
// function keys and shifted versions.  Unused slots have a code of zero, which is not a valid extended key value.
KeyVect keyBindTable[KeyVectCount];

// Built-in/default key bindings loaded into keyBindTable.
static BuiltinKey builtinKeys[] = {
	{Ctrl | ' ', cmdFuncTable + cf_setMark},
	{Ctrl | 'A', cmdFuncTable + cf_beginLine},
	{Ctrl | 'B', cmdFuncTable + cf_backChar},
	{Ctrl | 'C', cmdFuncTable + cf_prefix2},
	{Ctrl | 'D', cmdFuncTable + cf_delForwChar},
	{Ctrl | 'E', cmdFuncTable + cf_endLine},
	{Ctrl | 'F', cmdFuncTable + cf_forwChar},
	{Ctrl | 'G', cmdFuncTable + cf_abort},
	{Ctrl | 'H', cmdFuncTable + cf_prefix3},
	{Ctrl | 'I', cmdFuncTable + cf_tab},
	{Ctrl | 'J', cmdFuncTable + cf_newlineI},
	{Ctrl | 'K', cmdFuncTable + cf_delToBreak},
	{Ctrl | 'L', cmdFuncTable + cf_delLine},
	{Ctrl | 'M', cmdFuncTable + cf_newline},
	{Ctrl | 'N', cmdFuncTable + cf_forwLine},
	{Ctrl | 'O', cmdFuncTable + cf_openLine},
	{Ctrl | 'P', cmdFuncTable + cf_backLine},
	{Ctrl | 'Q', cmdFuncTable + cf_quoteChar},
	{Ctrl | 'R', cmdFuncTable + cf_searchBack},
	{Ctrl | 'S', cmdFuncTable + cf_searchForw},
	{Ctrl | 'T', cmdFuncTable + cf_traverseLine},
	{Ctrl | 'U', cmdFuncTable + cf_universalArg},
	{Ctrl | 'V', cmdFuncTable + cf_forwPage},
	{Ctrl | 'W', cmdFuncTable + cf_delRegion},
	{Ctrl | 'X', cmdFuncTable + cf_prefix1},
	{Ctrl | 'Y', cmdFuncTable + cf_yank},
	{Ctrl | 'Z', cmdFuncTable + cf_backPage},
	{Ctrl | '[', cmdFuncTable + cf_metaPrefix},
	{Ctrl | '\\', cmdFuncTable + cf_delWord},
	{Ctrl | ']', cmdFuncTable + cf_huntForw},
	{Ctrl | '^', cmdFuncTable + cf_huntBack},
	{Ctrl | '_', cmdFuncTable + cf_negativeArg},
	{Ctrl | '?', cmdFuncTable + cf_backspace},

	{Shift | Ctrl | 'I', cmdFuncTable + cf_delBackTab},

	{' ', cmdFuncTable + cf_space},

// Function key bindings.
	{FKey | 'H', cmdFuncTable + cf_beginBuf},		// Home, fn-left-arrow.
	{FKey | 'E', cmdFuncTable + cf_endBuf},			// End, fn-right-arrow.
	{FKey | 'B', cmdFuncTable + cf_backChar},		// Left arrow.
	{FKey | 'C', cmdFuncTable + cf_clearBuf},		// Clear key.
	{FKey | 'D', cmdFuncTable + cf_delForwChar},		// Shift delete [x> key.
#if 0
	{FKey | 'S', cmdFuncTable + cf_???},			// Enter/send key, fn-return.
#endif
	{FKey | 'F', cmdFuncTable + cf_forwChar},		// Right arrow.
	{FKey | 'N', cmdFuncTable + cf_forwLine},		// Down arrow.
	{FKey | 'P', cmdFuncTable + cf_backLine},		// Up arrow.
	{FKey | 'R', cmdFuncTable + cf_resetTerm},		// Terminal-resize event.
	{FKey | 'V', cmdFuncTable + cf_forwPage},		// PgDn, fn-down-arrow.
	{FKey | 'Z', cmdFuncTable + cf_backPage},		// PgUp, fn-up-arrow.
#if LINUX || MACOS
#if 0
	{Shift | FKey | 'H', cmdFuncTable + cf_???},		// Shift home.
	{Shift | FKey | 'E', cmdFuncTable + cf_???},		// Shift end.
#endif
	{Shift | FKey | 'B', cmdFuncTable + cf_backWord},	// Shift left arrow.
	{Shift | FKey | 'D', cmdFuncTable + cf_delWhite},	// Shift delete [x> key.
	{Shift | FKey | 'F', cmdFuncTable + cf_forwWord},	// Shift right arrow.
#endif

// Meta (ESC) bindings.
	{Meta | Ctrl | ' ', cmdFuncTable + cf_gotoMark},
	{Meta | Ctrl | 'A', cmdFuncTable + cf_beginText},
	{Meta | Ctrl | 'B', cmdFuncTable + cf_backTab},
	{Meta | Ctrl | 'E', cmdFuncTable + cf_endWord},
	{Meta | Ctrl | 'F', cmdFuncTable + cf_forwTab},
#if 0
	{Meta | Ctrl | 'G', cmdFuncTable + cf_???},
#endif
	{Meta | Ctrl | 'I', cmdFuncTable + cf_delForwTab},
	{Meta | Ctrl | 'J', cmdFuncTable + cf_joinLines},
	{Meta | Ctrl | 'K', cmdFuncTable + cf_unbindKey},
	{Meta | Ctrl | 'L', cmdFuncTable + cf_lowerCaseWord},
	{Meta | Ctrl | 'M', cmdFuncTable + cf_wrapLine},
	{Meta | Ctrl | 'S', cmdFuncTable + cf_sortRegion},
	{Meta | Ctrl | 'T', cmdFuncTable + cf_titleCaseWord},
	{Meta | Ctrl | 'U', cmdFuncTable + cf_upperCaseWord},
	{Meta | Ctrl | 'V', cmdFuncTable + cf_forwPageNext},
	{Meta | Ctrl | 'X', cmdFuncTable + cf_eval},
	{Meta | Ctrl | 'Z', cmdFuncTable + cf_backPageNext},
	{Meta | Ctrl | '[', cmdFuncTable + cf_resetTerm},
#if 0
	{Meta | Ctrl | '\\', cmdFuncTable + cf_???},
	{Meta | Ctrl | ']', cmdFuncTable + cf_???},
	{Meta | Ctrl | '^', cmdFuncTable + cf_???},
	{Meta | Ctrl | '_', cmdFuncTable + cf_???},
#endif
	{Meta | Ctrl | '?', cmdFuncTable + cf_delBackChar},

	{Meta | ' ', cmdFuncTable + cf_insertSpace},
#if 0
	{Meta | '!', cmdFuncTable + cf_???},
	{Meta | '"', cmdFuncTable + cf_???},
	{Meta | '#', cmdFuncTable + cf_???},
	{Meta | '$', cmdFuncTable + cf_???},
	{Meta | '%', cmdFuncTable + cf_???},
	{Meta | '&', cmdFuncTable + cf_???},
#endif
	{Meta | '\'', cmdFuncTable + cf_gotoFence},
	{Meta | '(', cmdFuncTable + cf_outdentRegion},
	{Meta | ')', cmdFuncTable + cf_indentRegion},
	{Meta | '*', cmdFuncTable + cf_writeRegion},
#if 0
	{Meta | '+', cmdFuncTable + cf_???},
#endif
	{Meta | ',', cmdFuncTable + cf_setShortcut},
#if 0
	{Meta | '-', cmdFuncTable + cf_???},
	{Meta | '.', cmdFuncTable + cf_???},
	{Meta | '/', cmdFuncTable + cf_???},
	{Meta | '0', cmdFuncTable + cf_???},
	{Meta | ':', cmdFuncTable + cf_???},
	{Meta | ';', cmdFuncTable + cf_???},
#endif
	{Meta | '<', cmdFuncTable + cf_beginBuf},
	{Meta | '=', cmdFuncTable + cf_let},
	{Meta | '>', cmdFuncTable + cf_endBuf},
#if 0
	{Meta | '@', cmdFuncTable + cf_???},
#endif
	{Meta | '\\', cmdFuncTable + cf_lastScreen},

	{Meta | 'a', cmdFuncTable + cf_apropos},
	{Meta | 'b', cmdFuncTable + cf_backWord},
	{Meta | 'd', cmdFuncTable + cf_dupLine},
	{Meta | 'e', cmdFuncTable + cf_exit},			// Alternative binding.
	{Meta | 'f', cmdFuncTable + cf_forwWord},
	{Meta | 'g', cmdFuncTable + cf_gotoLine},
	{Meta | 'i', cmdFuncTable + cf_seti},
	{Meta | 'k', cmdFuncTable + cf_bindKey},
	{Meta | 'M', cmdFuncTable + cf_selectMacro},
	{Meta | 'm', cmdFuncTable + cf_chgMode},
	{Meta | 'q', cmdFuncTable + cf_queryReplace},
	{Meta | 'R', cmdFuncTable + cf_ringSize},
	{Meta | 'r', cmdFuncTable + cf_replace},
	{Meta | 'S', cmdFuncTable + cf_selectScreen},
	{Meta | 't', cmdFuncTable + cf_truncBuf},
	{Meta | 'u', cmdFuncTable + cf_undelete},
	{Meta | 'w', cmdFuncTable + cf_setWrapCol},
	{Meta | 'x', cmdFuncTable + cf_run},
	{Meta | 'z', cmdFuncTable + cf_quickExit},

	{Meta | '[', cmdFuncTable + cf_prevScreen},
#if 0
	{Meta | '\\', cmdFuncTable + cf_???},
#endif
	{Meta | ']', cmdFuncTable + cf_nextScreen},
#if 0
	{Meta | '^', cmdFuncTable + cf_???},
	{Meta | '_', cmdFuncTable + cf_???},
	{Meta | '`', cmdFuncTable + cf_???},
	{Meta | '{', cmdFuncTable + cf_???},
	{Meta | '|', cmdFuncTable + cf_???},
	{Meta | '}', cmdFuncTable + cf_??},
	{Meta | '~', cmdFuncTable + cf_???},

	{Meta | FKey | 'B', cmdFuncTable + cf_???},		// Left arrow.
	{Meta | FKey | 'F', cmdFuncTable + cf_???},		// Right arrow.
	{Meta | FKey | 'P', cmdFuncTable + cf_???},		// Up arrow.
	{Meta | FKey | 'N', cmdFuncTable + cf_???},		// Down arrow.
#if LINUX || MACOS
	{Meta | Shift | FKey | 'B', cmdFuncTable + cf_???},	// Shift left arrow.
	{Meta | Shift | FKey | 'F', cmdFuncTable + cf_???},	// Shift right arrow.
#endif
#endif

// Prefix1 (C-x) bindings:
	{CtrlX | Ctrl | ' ', cmdFuncTable + cf_delMark},
	{CtrlX | Ctrl | 'A', cmdFuncTable + cf_appendFile},
	{CtrlX | Ctrl | 'B', cmdFuncTable + cf_delBuf},
	{CtrlX | Ctrl | 'C', cmdFuncTable + cf_exit},
	{CtrlX | Ctrl | 'D', cmdFuncTable + cf_detabLine},
	{CtrlX | Ctrl | 'E', cmdFuncTable + cf_entabLine},
	{CtrlX | Ctrl | 'F', cmdFuncTable + cf_findFile},
	{CtrlX | Ctrl | 'I', cmdFuncTable + cf_insertFile},
	{CtrlX | Ctrl | 'L', cmdFuncTable + cf_lowerCaseLine},
	{CtrlX | Ctrl | 'N', cmdFuncTable + cf_moveWindDown},
	{CtrlX | Ctrl | 'O', cmdFuncTable + cf_delBlankLines},
	{CtrlX | Ctrl | 'P', cmdFuncTable + cf_moveWindUp},
	{CtrlX | Ctrl | 'R', cmdFuncTable + cf_readFile},
	{CtrlX | Ctrl | 'S', cmdFuncTable + cf_saveFile},
	{CtrlX | Ctrl | 'T', cmdFuncTable + cf_titleCaseLine},
	{CtrlX | Ctrl | 'U', cmdFuncTable + cf_upperCaseLine},
	{CtrlX | Ctrl | 'V', cmdFuncTable + cf_viewFile},
	{CtrlX | Ctrl | 'W', cmdFuncTable + cf_writeFile},
	{CtrlX | Ctrl | 'X', cmdFuncTable + cf_swapMark},
	{CtrlX | Ctrl | 'Y', cmdFuncTable + cf_revertYank},
	{CtrlX | Ctrl | 'Z', cmdFuncTable + cf_suspend},
#if 0
	{CtrlX | Ctrl | '[', cmdFuncTable + cf_???},
#endif
	{CtrlX | Ctrl | '\\', cmdFuncTable + cf_trimLine},
#if 0
	{CtrlX | Ctrl | ']', cmdFuncTable + cf_???},
	{CtrlX | Ctrl | '^', cmdFuncTable + cf_???},
	{CtrlX | Ctrl | '_', cmdFuncTable + cf_???},
#endif
	{CtrlX | Ctrl | '?', cmdFuncTable + cf_clearBuf},

	{CtrlX | ' ', cmdFuncTable + cf_delWhite},
	{CtrlX | '!', cmdFuncTable + cf_shellCmd},
#if 0
	{CtrlX | '"', cmdFuncTable + cf_???},
#endif
	{CtrlX | '#', cmdFuncTable + cf_scratchBuf},
	{CtrlX | '$', cmdFuncTable + cf_shell},
#if 0
	{CtrlX | '%', cmdFuncTable + cf_???},
#endif
	{CtrlX | '&', cmdFuncTable + cf_delRingEntry},
	{CtrlX | '\'', cmdFuncTable + cf_delFencedRegion},
	{CtrlX | '(', cmdFuncTable + cf_beginMacro},
	{CtrlX | ')', cmdFuncTable + cf_endMacro},
	{CtrlX | '*', cmdFuncTable + cf_writeLine},
	{CtrlX | '+', cmdFuncTable + cf_growWind},
	{CtrlX | ',', cmdFuncTable + cf_delShortcut},
	{CtrlX | '-', cmdFuncTable + cf_shrinkWind},
	{CtrlX | '.', cmdFuncTable + cf_reframeWind},
	{CtrlX | '/', cmdFuncTable + cf_xeqFile},
	{CtrlX | '0', cmdFuncTable + cf_delWind},
	{CtrlX | '1', cmdFuncTable + cf_onlyWind},
	{CtrlX | '2', cmdFuncTable + cf_splitWind},
	{CtrlX | ':', cmdFuncTable + cf_rmdir},
#if 0
	{CtrlX | ';', cmdFuncTable + cf_???},
#endif
	{CtrlX | '<', cmdFuncTable + cf_narrowBuf},
	{CtrlX | '=', cmdFuncTable + cf_delConstVar},
	{CtrlX | '>', cmdFuncTable + cf_widenBuf},
#if 0
	{CtrlX | '?', cmdFuncTable + cf_???},
#endif
	{CtrlX | '@', cmdFuncTable + cf_delRoutine},

	{CtrlX | 'a', cmdFuncTable + cf_chgBufAttr},
	{CtrlX | 'B', cmdFuncTable + cf_renameBuf},
	{CtrlX | 'b', cmdFuncTable + cf_selectBuf},
	{CtrlX | 'D', cmdFuncTable + cf_delFile},
	{CtrlX | 'e', cmdFuncTable + cf_xeqMacro},
	{CtrlX | 'F', cmdFuncTable + cf_setBufFile},
	{CtrlX | 'g', cmdFuncTable + cf_editModeGrp},
	{CtrlX | 'h', cmdFuncTable + cf_chgBufHome},
	{CtrlX | 'j', cmdFuncTable + cf_joinWind},
	{CtrlX | 'L', cmdFuncTable + cf_linkFile},
	{CtrlX | 'l', cmdFuncTable + cf_lowerCaseRegion},
	{CtrlX | 'M', cmdFuncTable + cf_delMacro},
	{CtrlX | 'm', cmdFuncTable + cf_editMode},
	{CtrlX | 'n', cmdFuncTable + cf_nextWind},
	{CtrlX | 'o', cmdFuncTable + cf_openLineI},
	{CtrlX | 'p', cmdFuncTable + cf_prevWind},
	{CtrlX | 'R', cmdFuncTable + cf_renameFile},
	{CtrlX | 'S', cmdFuncTable + cf_delScreen},
	{CtrlX | 't', cmdFuncTable + cf_titleCaseRegion},
	{CtrlX | 'u', cmdFuncTable + cf_upperCaseRegion},
	{CtrlX | 'w', cmdFuncTable + cf_selectWind},
	{CtrlX | 'x', cmdFuncTable + cf_xeqBuf},
	{CtrlX | 'z', cmdFuncTable + cf_resizeWind},

	{CtrlX | '[', cmdFuncTable + cf_prevBuf},
	{CtrlX | '\\', cmdFuncTable + cf_lastBuf},
	{CtrlX | ']', cmdFuncTable + cf_nextBuf},
	{CtrlX | '^', cmdFuncTable + cf_insertBuf},
#if 0
	{CtrlX | '_', cmdFuncTable + cf_???},
#endif
	{CtrlX | '`', cmdFuncTable + cf_insertPipe},
#if 0
	{CtrlX | '{', cmdFuncTable + cf_???},
#endif
	{CtrlX | '|', cmdFuncTable + cf_pipeBuf},
#if 0
	{CtrlX | '}', cmdFuncTable + cf_???},
#endif
	{CtrlX | '~', cmdFuncTable + cf_readPipe},

// Prefix2 (C-c) bindings:
	{CtrlC | Ctrl | ' ', cmdFuncTable + cf_markBuf},
	{CtrlC | Ctrl | 'A', cmdFuncTable + cf_delAlias},
	{CtrlC | Ctrl | 'I', cmdFuncTable + cf_shiftLine},
	{CtrlC | Ctrl | 'K', cmdFuncTable + cf_copyToBreak},
	{CtrlC | Ctrl | 'L', cmdFuncTable + cf_copyLine},
	{CtrlC | Ctrl | 'W', cmdFuncTable + cf_copyRegion},
	{CtrlC | Ctrl | 'Y', cmdFuncTable + cf_yankCycle},
#if 0
	{CtrlC | Ctrl | '[', cmdFuncTable + cf_???},
#endif
	{CtrlC | Ctrl | '\\', cmdFuncTable + cf_copyWord},
#if 0
	{CtrlC | Ctrl | ']', cmdFuncTable + cf_???},
	{CtrlC | Ctrl | '^', cmdFuncTable + cf_???},
	{CtrlC | Ctrl | '_', cmdFuncTable + cf_???},
	{CtrlC | Ctrl | '?', cmdFuncTable + cf_???},

	{CtrlC | ' ', cmdFuncTable + cf_???},
	{CtrlC | '!', cmdFuncTable + cf_???},
	{CtrlC | '"', cmdFuncTable + cf_???},
	{CtrlC | '#', cmdFuncTable + cf_???},
	{CtrlC | '$', cmdFuncTable + cf_???},
	{CtrlC | '%', cmdFuncTable + cf_???},
#endif
	{CtrlC | '&', cmdFuncTable + cf_cycleRing},
	{CtrlC | '\'', cmdFuncTable + cf_copyFencedRegion},
	{CtrlC | '(', cmdFuncTable + cf_outdentLine},
	{CtrlC | ')', cmdFuncTable + cf_indentLine},
#if 0
	{CtrlC | '*', cmdFuncTable + cf_???},
	{CtrlC | '+', cmdFuncTable + cf_???},
	{CtrlC | ',', cmdFuncTable + cf_???},
	{CtrlC | '-', cmdFuncTable + cf_???},
	{CtrlC | '.', cmdFuncTable + cf_???},
	{CtrlC | '/', cmdFuncTable + cf_???},
	{CtrlC | '0', cmdFuncTable + cf_???},
#endif
	{CtrlC | ':', cmdFuncTable + cf_mkdir},
#if 0
	{CtrlC | ';', cmdFuncTable + cf_???},
	{CtrlC | '<', cmdFuncTable + cf_???},
	{CtrlC | '=', cmdFuncTable + cf_???},
	{CtrlC | '>', cmdFuncTable + cf_???},
	{CtrlC | '?', cmdFuncTable + cf_???},
	{CtrlC | '@', cmdFuncTable + cf_???},
#endif
	{CtrlC | 'a', cmdFuncTable + cf_alias},
	{CtrlC | 'D', cmdFuncTable + cf_chdir},
	{CtrlC | 'i', cmdFuncTable + cf_inserti},
	{CtrlC | 'M', cmdFuncTable + cf_editMacro},
	{CtrlC | 'm', cmdFuncTable + cf_renameMacro},
	{CtrlC | 'S', cmdFuncTable + cf_swapScreens},
	{CtrlC | 'u', cmdFuncTable + cf_undeleteCycle},
#if 0
	{CtrlC | '[', cmdFuncTable + cf_???},
	{CtrlC | '\\', cmdFuncTable + cf_???},
	{CtrlC | ']', cmdFuncTable + cf_???},
	{CtrlC | '^', cmdFuncTable + cf_???},
	{CtrlC | '_', cmdFuncTable + cf_???},
	{CtrlC | '`', cmdFuncTable + cf_???},
	{CtrlC | '{', cmdFuncTable + cf_???},
	{CtrlC | '|', cmdFuncTable + cf_???},
	{CtrlC | '}', cmdFuncTable + cf_???},
	{CtrlC | '~', cmdFuncTable + cf_???},
#endif

// Prefix3 (C-h) bindings:
	{CtrlH | Ctrl | ' ', cmdFuncTable + cf_showMarks},
	{CtrlH | Ctrl | 'A', cmdFuncTable + cf_beginWhite},
	{CtrlH | Ctrl | 'E', cmdFuncTable + cf_endWhite},
	{CtrlH | Ctrl | 'H', cmdFuncTable + cf_showFence},
	{CtrlH | Ctrl | 'K', cmdFuncTable + cf_killToBreak},
	{CtrlH | Ctrl | 'L', cmdFuncTable + cf_killLine},
	{CtrlH | Ctrl | 'P', cmdFuncTable + cf_popFile},
	{CtrlH | Ctrl | 'V', cmdFuncTable + cf_forwPagePrev},
	{CtrlH | Ctrl | 'W', cmdFuncTable + cf_killRegion},
	{CtrlH | Ctrl | 'Y', cmdFuncTable + cf_yankDel},
	{CtrlH | Ctrl | 'Z', cmdFuncTable + cf_backPagePrev},
#if 0
	{CtrlH | Ctrl | '[', cmdFuncTable + cf_???},
#endif
	{CtrlH | Ctrl | '\\', cmdFuncTable + cf_killWord},
#if 0
	{CtrlH | Ctrl | ']', cmdFuncTable + cf_???},
	{CtrlH | Ctrl | '^', cmdFuncTable + cf_???},
	{CtrlH | Ctrl | '_', cmdFuncTable + cf_???},
	{CtrlH | Ctrl | '?', cmdFuncTable + cf_???},
#endif
#if 0
	{CtrlH | ' ', cmdFuncTable + cf_???},
	{CtrlH | '!', cmdFuncTable + cf_???},
	{CtrlH | '"', cmdFuncTable + cf_???},
	{CtrlH | '#', cmdFuncTable + cf_???},
	{CtrlH | '$', cmdFuncTable + cf_???},
	{CtrlH | '%', cmdFuncTable + cf_???},
#endif
	{CtrlH | '&', cmdFuncTable + cf_showRing},
	{CtrlH | '\'', cmdFuncTable + cf_killFencedRegion},
#if 0
	{CtrlH | '(', cmdFuncTable + cf_???},
	{CtrlH | ')', cmdFuncTable + cf_???},
	{CtrlH | '*', cmdFuncTable + cf_???},
	{CtrlH | '+', cmdFuncTable + cf_???},
	{CtrlH | ',', cmdFuncTable + cf_???},
	{CtrlH | '-', cmdFuncTable + cf_???},
#endif
	{CtrlH | '.', cmdFuncTable + cf_showPoint},
#if 0
	{CtrlH | '/', cmdFuncTable + cf_???},
	{CtrlH | '0', cmdFuncTable + cf_???},
	{CtrlH | ':', cmdFuncTable + cf_???},
	{CtrlH | ';', cmdFuncTable + cf_???},
	{CtrlH | '<', cmdFuncTable + cf_???},
#endif
	{CtrlH | '=', cmdFuncTable + cf_showConstVar},
#if 0
	{CtrlH | '>', cmdFuncTable + cf_???},
#endif
	{CtrlH | '?', cmdFuncTable + cf_showKey},

	{CtrlH | 'A', cmdFuncTable + cf_about},
	{CtrlH | 'a', cmdFuncTable + cf_showAliases},
	{CtrlH | 'b', cmdFuncTable + cf_showBuffers},
	{CtrlH | 'C', cmdFuncTable + cf_showColors},
	{CtrlH | 'D', cmdFuncTable + cf_pwd},
	{CtrlH | 'h', cmdFuncTable + cf_showHooks},
	{CtrlH | 'M', cmdFuncTable + cf_showMacros},
	{CtrlH | 'm', cmdFuncTable + cf_showModes},
	{CtrlH | 'n', cmdFuncTable + cf_showBindings},
	{CtrlH | 'p', cmdFuncTable + cf_popBuf},
	{CtrlH | 'r', cmdFuncTable + cf_showRoutines},
	{CtrlH | 'S', cmdFuncTable + cf_showScreens},
	{CtrlH | 'v', cmdFuncTable + cf_showConstVars},
#if 0
	{CtrlH | '[', cmdFuncTable + cf_???},
	{CtrlH | '\\', cmdFuncTable + cf_???},
	{CtrlH | ']', cmdFuncTable + cf_???},
	{CtrlH | '^', cmdFuncTable + cf_???},
	{CtrlH | '_', cmdFuncTable + cf_???},
	{CtrlH | '`', cmdFuncTable + cf_???},
	{CtrlH | '{', cmdFuncTable + cf_???},
	{CtrlH | '|', cmdFuncTable + cf_???},
	{CtrlH | '}', cmdFuncTable + cf_???},
#endif
#if MMDebug & Debug_ShowRE
	{CtrlH | '~', cmdFuncTable + cf_showRegexp},
#endif
	{0, NULL}};
MacroHdr macro = {
	.index = {OH_Changed, NULL}, .state = MacStopped};	// Macro control variables.

#else

// **** For all the other .c files ****

// External variable declarations.
extern CoreKey coreKeys[];
extern const char fKeySyms[];
extern KeyVect keyBindTable[];
extern KeyEntry keyEntry;
extern MacroHdr macro;
#endif
