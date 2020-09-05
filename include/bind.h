// (c) Copyright 2020 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// bind.h	Default key-to-command bindings for MightEMacs.

// Command binding table.
//
// This table is *roughly* in ASCII order, left to right across the characters of the command.
//
// Printable ASCII chars: SPC ! " # $ % & ' ( ) * + , - . / 0-9 : ; < = > ? @ A-Z [ \ ] ^ _ ` a-z { | } ~.
//
//			Kill		Delete		Copy
//	Word		^H ^\		^\		^C ^\		.
//	ToBreak		^H ^K		^K		^C ^K
//	Line		^H ^L		^L		^C ^L
//	Region		^H ^W		^W		^C ^W
//	Fenced region	^H  }		^X }		^C  }

// Descriptor for a key binding.
typedef struct {
	ushort code;			// Key code.
	UnivPtr targ;			// Command to execute.
	} KeyBind;

// Key binding array (vector) for one 7-bit key of a key sequence: plain characters (0..127) + printable character of a function
// key (! .. ~) + S-TAB + shifted function key.
typedef KeyBind KeyVect[KeyVectSlots];

// Control object for walking through the key binding table.
typedef struct {
	KeyVect *pKeyVect;		// Key vector pointer (initially NULL).
	KeyBind *pKeyBind;		// Pointer to next binding.
	} KeyWalk;

// Built-in key binding (which is copied to appropriate list at startup).
typedef struct {
	ushort code;			// Key code.
	CmdFuncId id;			// Command-function id.
	} KeyItem;

// Terminal key entry information.
typedef struct {
	ushort lastKeySeq;		// Last key sequence (extended key) returned from getKeySeq().
	bool useLast;			// Use lastKeySeq for next key?
	ushort charPending;		// Character pushed back via ungetkey().
	bool isPending;			// Character pending (chpending valid)?
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
extern int beginMacro(Datum *pRtnVal, int n, Datum **args);
extern int binding(Datum *pRtnVal, int n, Datum **args);
extern int bindKey(Datum *pRtnVal, int n, Datum **args);
extern int delMacro(Datum *pRtnVal, Ring *pRing, int n);
extern char *ektos(ushort extKey, char *dest, bool escTermAttr);
extern int endMacro(Datum *pRtnVal, int n, Datum **args);
extern int fGetKey(Datum *pRtnVal, int n, Datum **args);
extern int finishMacro(KeyBind *pKeyBind);
extern KeyBind *getBind(ushort extKey);
extern int getKeyBind(const char *prompt, int n, Datum **args, ushort *result);
extern int getkey(bool msgLine, ushort *pExtKey, bool saveKey);
extern int getKeySeq(bool msgLine, ushort *pExtKey, KeyBind **ppKeyBind, bool saveKey);
extern KeyBind *getPtrEntry(UnivPtr *pUniv);
extern int growMacro(void);
extern int loadBindings(void);
extern bool macBusy(bool strict, const char *fmt, ...);
extern int macReset(void);
extern char *macSplit(char *name, const char *macro);
extern int manageMacro(Datum *pRtnVal, int n, Datum **args);
extern KeyBind *nextBind(KeyWalk *pKeyWalk);
extern int renameMacro(Datum *pRtnVal, int n, Datum **args);
extern int showKey(Datum *pRtnVal, int n, Datum **args);
extern int stoek(const char *keyCode, ushort *result);
extern int decodeMacro(char *macro);
extern void unbind(KeyBind *pKeyBind);
extern int unbindKey(Datum *pRtnVal, int n, Datum **args);
extern int xeqMacro(Datum *pRtnVal, int n, Datum **args);

#ifdef BindData

// **** For bind.c ****

// Global variables.
CoreKey coreKeys[CoreKeyCount];		// Cache of frequently-used keys.
KeyEntry keyEntry = {0, false, 0, false, 0, 0};
					// Terminal key entry variables.
static KeyItem keyItems[] = {
	{Ctrl | ' ', cf_setMark},
	{Ctrl | 'A', cf_beginLine},
	{Ctrl | 'B', cf_backChar},
	{Ctrl | 'C', cf_prefix2},
	{Ctrl | 'D', cf_delForwChar},
	{Ctrl | 'E', cf_endLine},
	{Ctrl | 'F', cf_forwChar},
	{Ctrl | 'G', cf_abort},
	{Ctrl | 'H', cf_prefix3},
	{Ctrl | 'I', cf_tab},
	{Ctrl | 'J', cf_newlineI},
	{Ctrl | 'K', cf_delToBreak},
	{Ctrl | 'L', cf_delLine},
	{Ctrl | 'M', cf_newline},
	{Ctrl | 'N', cf_forwLine},
	{Ctrl | 'O', cf_openLine},
	{Ctrl | 'P', cf_backLine},
	{Ctrl | 'Q', cf_quoteChar},
	{Ctrl | 'R', cf_searchBack},
	{Ctrl | 'S', cf_searchForw},
	{Ctrl | 'T', cf_traverseLine},
	{Ctrl | 'U', cf_universalArg},
	{Ctrl | 'V', cf_forwPage},
	{Ctrl | 'W', cf_delRegion},
	{Ctrl | 'X', cf_prefix1},
	{Ctrl | 'Y', cf_yank},
	{Ctrl | 'Z', cf_backPage},
	{Ctrl | '[', cf_metaPrefix},
	{Ctrl | '\\', cf_delWord},
	{Ctrl | ']', cf_huntForw},
	{Ctrl | '^', cf_huntBack},
	{Ctrl | '_', cf_negativeArg},
	{Ctrl | '?', cf_backspace},

	{Shift | Ctrl | 'I', cf_delBackTab},

	{' ', cf_space},

// Function key bindings.
	{FKey | '<', cf_beginBuf},		// Home.
	{FKey | '>', cf_endBuf},		// End.
	{FKey | 'B', cf_backChar},		// Left arrow.
	{FKey | 'D', cf_delForwChar},		// Delete [x> key.
	{FKey | 'F', cf_forwChar},		// Right arrow.
	{FKey | 'N', cf_forwLine},		// Down arrow.
	{FKey | 'P', cf_backLine},		// Up arrow.
	{FKey | 'R', cf_resetTerm},		// Terminal-resize event.
	{FKey | 'V', cf_forwPage},		// PgDn.
	{FKey | 'Z', cf_backPage},		// PgUp.
#if LINUX || MACOS
	{Shift | FKey | 'B', cf_backWord},	// Shift left arrow.
	{Shift | FKey | 'D', cf_delWhite},	// Shift delete [x> key.
	{Shift | FKey | 'F', cf_forwWord},	// Shift right arrow.
#endif

// Prefix1 (C-x) bindings:
	{Pref1 | Ctrl | ' ', cf_delMark},
	{Pref1 | Ctrl | 'A', cf_appendFile},
	{Pref1 | Ctrl | 'B', cf_delBuf},
	{Pref1 | Ctrl | 'C', cf_exit},
	{Pref1 | Ctrl | 'D', cf_detabLine},
	{Pref1 | Ctrl | 'E', cf_entabLine},
	{Pref1 | Ctrl | 'F', cf_findFile},
	{Pref1 | Ctrl | 'I', cf_insertFile},
	{Pref1 | Ctrl | 'L', cf_lowerCaseLine},
	{Pref1 | Ctrl | 'N', cf_moveWindDown},
	{Pref1 | Ctrl | 'O', cf_delBlankLines},
	{Pref1 | Ctrl | 'P', cf_moveWindUp},
	{Pref1 | Ctrl | 'R', cf_readFile},
	{Pref1 | Ctrl | 'S', cf_saveFile},
	{Pref1 | Ctrl | 'T', cf_titleCaseLine},
	{Pref1 | Ctrl | 'U', cf_upperCaseLine},
	{Pref1 | Ctrl | 'V', cf_viewFile},
	{Pref1 | Ctrl | 'W', cf_writeFile},
	{Pref1 | Ctrl | 'X', cf_swapMark},
	{Pref1 | Ctrl | 'Y', cf_revertYank},
	{Pref1 | Ctrl | 'Z', cf_suspend},
#if 0
	{Pref1 | Ctrl | '[', cf_???},
#endif
	{Pref1 | Ctrl | '\\', cf_trimLine},
#if 0
	{Pref1 | Ctrl | ']', cf_???},
	{Pref1 | Ctrl | '^', cf_???},
	{Pref1 | Ctrl | '_', cf_???},
#endif
	{Pref1 | Ctrl | '?', cf_clearBuf},

	{Pref1 | ' ', cf_insertSpace},
	{Pref1 | '!', cf_shellCmd},
#if 0
	{Pref1 | '"', cf_???},
#endif
	{Pref1 | '#', cf_scratchBuf},
	{Pref1 | '$', cf_shell},
#if 0
	{Pref1 | '%', cf_???},
#endif
	{Pref1 | '&', cf_delRingEntry},
	{Pref1 | '\'', cf_delFencedRegion},
	{Pref1 | '(', cf_beginMacro},
	{Pref1 | ')', cf_endMacro},
	{Pref1 | '*', cf_writeBuf},
	{Pref1 | '+', cf_growWind},
#if 0
	{Pref1 | ',', cf_???},
#endif
	{Pref1 | '-', cf_shrinkWind},
	{Pref1 | '.', cf_reframeWind},
	{Pref1 | '/', cf_xeqFile},
	{Pref1 | '0', cf_delWind},
	{Pref1 | '1', cf_onlyWind},
	{Pref1 | '2', cf_splitWind},
#if 0
	{Pref1 | ':', cf_???},
	{Pref1 | ';', cf_???},
#endif
	{Pref1 | '<', cf_narrowBuf},
#if 0
	{Pref1 | '=', cf_???},
#endif
	{Pref1 | '>', cf_widenBuf},
#if 0
	{Pref1 | '?', cf_???},
#endif
	{Pref1 | 'a', cf_chgBufAttr},
	{Pref1 | 'B', cf_renameBuf},
	{Pref1 | 'b', cf_selectBuf},
	{Pref1 | 'e', cf_xeqMacro},
	{Pref1 | 'F', cf_setBufFile},
	{Pref1 | 'g', cf_editModeGroup},
	{Pref1 | 'j', cf_joinWind},
	{Pref1 | 'l', cf_lowerCaseRegion},
	{Pref1 | 'm', cf_editMode},
	{Pref1 | 'N', cf_renameMacro},
	{Pref1 | 'n', cf_nextWind},
	{Pref1 | 'o', cf_openLineI},
	{Pref1 | 'p', cf_prevWind},
	{Pref1 | 'S', cf_sortRegion},
	{Pref1 | 't', cf_titleCaseRegion},
	{Pref1 | 'U', cf_delRoutine},
	{Pref1 | 'u', cf_upperCaseRegion},
	{Pref1 | 'w', cf_selectWind},
	{Pref1 | 'x', cf_xeqBuf},
	{Pref1 | 'z', cf_resizeWind},

	{Pref1 | '[', cf_prevBuf},
	{Pref1 | '\\', cf_lastBuf},
	{Pref1 | ']', cf_nextBuf},
	{Pref1 | '^', cf_insertBuf},
#if 0
	{Pref1 | '_', cf_???},
#endif
	{Pref1 | '`', cf_insertPipe},
#if 0
	{Pref1 | '{', cf_???},
#endif
	{Pref1 | '|', cf_pipeBuf},
#if 0
	{Pref1 | '}', cf_???},
#endif
	{Pref1 | '~', cf_readPipe},
#if 0
	{Pref1 | FKey | 'B', cf_???},		// Left arrow.
	{Pref1 | FKey | 'F', cf_???},		// Right arrow.
	{Pref1 | FKey | 'P', cf_???},		// Up arrow.
	{Pref1 | FKey | 'N', cf_???},		// Down arrow.
#if LINUX || MACOS
	{Pref1 | Shift | FKey | 'B', cf_???},	// Shift left arrow.
	{Pref1 | Shift | FKey | 'F', cf_???},	// Shift right arrow.
#endif
#endif

// Prefix2 (C-c) bindings:
#if 0
	{Pref2 | Ctrl | ' ', cf_???},
#endif
	{Pref2 | Ctrl | 'A', cf_delAlias},
	{Pref2 | Ctrl | 'K', cf_copyToBreak},
	{Pref2 | Ctrl | 'L', cf_copyLine},
	{Pref2 | Ctrl | 'W', cf_copyRegion},
#if 0
	{Pref2 | Ctrl | '[', cf_???},
#endif
	{Pref2 | Ctrl | '\\', cf_copyWord},
#if 0
	{Pref2 | Ctrl | ']', cf_???},
	{Pref2 | Ctrl | '^', cf_???},
	{Pref2 | Ctrl | '_', cf_???},
	{Pref2 | Ctrl | '?', cf_???},

	{Pref2 | ' ', cf_???},
	{Pref2 | '!', cf_???},
	{Pref2 | '"', cf_???},
	{Pref2 | '#', cf_???},
	{Pref2 | '$', cf_???},
	{Pref2 | '%', cf_???},
#endif
	{Pref2 | '&', cf_cycleRing},
	{Pref2 | '\'', cf_copyFencedRegion},
#if 0
	{Pref2 | '(', cf_???},
	{Pref2 | ')', cf_???},
	{Pref2 | '*', cf_???},
	{Pref2 | '+', cf_???},
	{Pref2 | ',', cf_???},
	{Pref2 | '-', cf_???},
	{Pref2 | '.', cf_???},
	{Pref2 | '/', cf_???},
	{Pref2 | '0', cf_???},
	{Pref2 | ':', cf_???},
	{Pref2 | ';', cf_???},
	{Pref2 | '<', cf_???},
	{Pref2 | '=', cf_???},
	{Pref2 | '>', cf_???},
	{Pref2 | '@', cf_???},
#endif
	{Pref2 | 'a', cf_alias},
	{Pref2 | 'D', cf_chgDir},
	{Pref2 | 'i', cf_inserti},
#if 0
	{Pref2 | '[', cf_???},
	{Pref2 | '\\', cf_???},
	{Pref2 | ']', cf_???},
	{Pref2 | '^', cf_???},
	{Pref2 | '_', cf_???},
	{Pref2 | '`', cf_???},
	{Pref2 | '{', cf_???},
	{Pref2 | '|', cf_???},
	{Pref2 | '}', cf_???},
	{Pref2 | '~', cf_???},
	{Pref2 | '?', cf_???},

	{Pref2 | FKey | 'B', cf_???},		// Left arrow.
	{Pref2 | FKey | 'F', cf_???},		// Right arrow.
	{Pref2 | FKey | 'P', cf_???},		// Up arrow.
	{Pref2 | FKey | 'N', cf_???},		// Down arrow.
#if LINUX || MACOS
	{Pref2 | Shift | FKey | 'B', cf_???},	// Shift left arrow.
	{Pref2 | Shift | FKey | 'F', cf_???},	// Shift right arrow.
#endif
#endif

// Prefix3 (C-h) bindings:
#if 0
	{Pref3 | Ctrl | ' ', cf_???},
#endif
	{Pref3 | Ctrl | 'A', cf_beginWhite},
	{Pref3 | Ctrl | 'E', cf_endWhite},
	{Pref3 | Ctrl | 'H', cf_showFence},
	{Pref3 | Ctrl | 'K', cf_killToBreak},
	{Pref3 | Ctrl | 'L', cf_killLine},
	{Pref3 | Ctrl | 'P', cf_popFile},
	{Pref3 | Ctrl | 'V', cf_forwPagePrev},
	{Pref3 | Ctrl | 'W', cf_killRegion},
	{Pref3 | Ctrl | 'Z', cf_backPagePrev},
#if 0
	{Pref3 | Ctrl | '[', cf_???},
#endif
	{Pref3 | Ctrl | '\\', cf_killWord},
#if 0
	{Pref3 | Ctrl | ']', cf_???},
	{Pref3 | Ctrl | '^', cf_???},
	{Pref3 | Ctrl | '_', cf_???},
	{Pref3 | Ctrl | '?', cf_???},
#endif
	{Pref3 | ' ', cf_showMarks},
#if 0
	{Pref3 | '!', cf_???},
	{Pref3 | '"', cf_???},
	{Pref3 | '#', cf_???},
	{Pref3 | '$', cf_???},
	{Pref3 | '%', cf_???},
#endif
	{Pref3 | '&', cf_showRing},
	{Pref3 | '\'', cf_killFencedRegion},
#if 0
	{Pref3 | '(', cf_???},
	{Pref3 | ')', cf_???},
	{Pref3 | '*', cf_???},
	{Pref3 | '+', cf_???},
	{Pref3 | ',', cf_???},
	{Pref3 | '-', cf_???},
#endif
	{Pref3 | '.', cf_showPoint},
#if 0
	{Pref3 | '/', cf_???},
	{Pref3 | '0', cf_???},
	{Pref3 | ':', cf_???},
	{Pref3 | ';', cf_???},
	{Pref3 | '<', cf_???},
	{Pref3 | '=', cf_???},
	{Pref3 | '>', cf_???},
#endif
	{Pref3 | '?', cf_showKey},

	{Pref3 | 'A', cf_about},
	{Pref3 | 'a', cf_showAliases},
	{Pref3 | 'b', cf_showBuffers},
	{Pref3 | 'C', cf_showColors},
	{Pref3 | 'c', cf_showCommands},
	{Pref3 | 'D', cf_showDir},
	{Pref3 | 'f', cf_showFunctions},
	{Pref3 | 'h', cf_showHooks},
	{Pref3 | 'm', cf_showModes},
	{Pref3 | 'p', cf_popBuf},
	{Pref3 | 'S', cf_showScreens},
	{Pref3 | 'v', cf_showVariables},
#if 0
	{Pref3 | '[', cf_???},
	{Pref3 | '\\', cf_???},
	{Pref3 | ']', cf_???},
	{Pref3 | '^', cf_???},
	{Pref3 | '_', cf_???},
	{Pref3 | '`', cf_???},
	{Pref3 | '{', cf_???},
	{Pref3 | '|', cf_???},
	{Pref3 | '}', cf_???},
#endif
#if MMDebug & Debug_ShowRE
	{Pref3 | '~', cf_showRegexp},
#endif
#if 0
	{Pref3 | FKey | 'B', cf_???},		// Left arrow.
	{Pref3 | FKey | 'F', cf_???},		// Right arrow.
	{Pref3 | FKey | 'P', cf_???},		// Up arrow.
	{Pref3 | FKey | 'N', cf_???},		// Down arrow.
#if LINUX || MACOS
	{Pref3 | Shift | FKey | 'B', cf_???},	// Shift left arrow.
	{Pref3 | Shift | FKey | 'F', cf_???},	// Shift right arrow.
#endif
#endif

// Meta (ESC) bindings.
	{Meta | Ctrl | ' ', cf_markBuf},
	{Meta | Ctrl | 'A', cf_beginText},
	{Meta | Ctrl | 'B', cf_backTab},
	{Meta | Ctrl | 'D', cf_delScreen},
	{Meta | Ctrl | 'E', cf_endWord},
	{Meta | Ctrl | 'F', cf_forwTab},
#if 0
	{Meta | Ctrl | 'G', cf_???},
#endif
	{Meta | Ctrl | 'I', cf_delForwTab},
	{Meta | Ctrl | 'J', cf_joinLines},
	{Meta | Ctrl | 'K', cf_unbindKey},
	{Meta | Ctrl | 'L', cf_lowerCaseWord},
	{Meta | Ctrl | 'M', cf_wrapLine},
	{Meta | Ctrl | 'T', cf_titleCaseWord},
	{Meta | Ctrl | 'U', cf_upperCaseWord},
	{Meta | Ctrl | 'V', cf_forwPageNext},
	{Meta | Ctrl | 'X', cf_eval},
	{Meta | Ctrl | 'Y', cf_yankCycle},
	{Meta | Ctrl | 'Z', cf_backPageNext},
	{Meta | Ctrl | '[', cf_resetTerm},
#if 0
	{Meta | Ctrl | '\\', cf_???},
	{Meta | Ctrl | ']', cf_???},
	{Meta | Ctrl | '^', cf_???},
	{Meta | Ctrl | '_', cf_???},
#endif
	{Meta | Ctrl | '?', cf_delBackChar},

	{Meta | ' ', cf_gotoMark},
#if 0
	{Meta | '!', cf_???},
	{Meta | '"', cf_???},
#endif
#if WordCount
	{Meta | '#', cf_countWords},
#endif
#if 0
	{Meta | '$', cf_???},
	{Meta | '%', cf_???},
	{Meta | '&', cf_???},
#endif
	{Meta | '\'', cf_gotoFence},
	{Meta | '(', cf_outdentRegion},
	{Meta | ')', cf_indentRegion},
#if 0
	{Meta | '*', cf_???},
	{Meta | '+', cf_???},
	{Meta | ',', cf_???},
	{Meta | '-', cf_???},
	{Meta | '.', cf_???},
	{Meta | '/', cf_???},
	{Meta | '0', cf_???},
	{Meta | ':', cf_???},
	{Meta | ';', cf_???},
#endif
	{Meta | '<', cf_beginBuf},
	{Meta | '=', cf_let},
	{Meta | '>', cf_endBuf},
#if 0
	{Meta | '@', cf_???},
#endif
	{Meta | 'a', cf_apropos},
	{Meta | 'b', cf_backWord},
	{Meta | 'd', cf_dupLine},
	{Meta | 'e', cf_exit},			// Alternative binding.
	{Meta | 'f', cf_forwWord},
	{Meta | 'g', cf_gotoLine},
	{Meta | 'i', cf_seti},
	{Meta | 'k', cf_bindKey},
	{Meta | 'm', cf_chgMode},
	{Meta | 'q', cf_queryReplace},
	{Meta | 'R', cf_ringSize},
	{Meta | 'r', cf_replace},
	{Meta | 's', cf_selectScreen},
	{Meta | 't', cf_truncBuf},
	{Meta | 'U', cf_undeleteCycle},
	{Meta | 'u', cf_undelete},
	{Meta | 'w', cf_setWrapCol},
	{Meta | 'x', cf_run},
	{Meta | 'z', cf_quickExit},

	{Meta | '[', cf_prevScreen},
	{Meta | '\\', cf_delWhite},
	{Meta | ']', cf_nextScreen},
#if 0
	{Meta | '^', cf_???},
	{Meta | '_', cf_???},
	{Meta | '`', cf_???},
	{Meta | '{', cf_???},
	{Meta | '|', cf_???},
	{Meta | '}', cf_??},
	{Meta | '~', cf_???},

	{Meta | FKey | 'B', cf_???},		// Left arrow.
	{Meta | FKey | 'F', cf_???},		// Right arrow.
	{Meta | FKey | 'P', cf_???},		// Up arrow.
	{Meta | FKey | 'N', cf_???},		// Down arrow.
#if LINUX || MACOS
	{Meta | Shift | FKey | 'B', cf_???},	// Shift left arrow.
	{Meta | Shift | FKey | 'F', cf_???},	// Shift right arrow.
#endif
#endif
	{0, -1}
};

// Key binding table.  Contains an array of KeyVect objects for each prefix key plus one more for all unprefixed bindings (the
// first entry).  Each KeyVect array contains 128 KeyBind objects -- a slot for every possible 7-bit character.  Unused slots
// have a code of zero, which is not a valid extended key value.
KeyVect keyBindTable[KeyTableCount];

#else

// **** For all the other .c files ****

// External variable declarations.
extern CoreKey coreKeys[];
extern KeyEntry keyEntry;
extern KeyVect keyBindTable[];
#endif
