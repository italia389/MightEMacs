// (c) Copyright 2018 Richard W. Marinelli
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
typedef struct KeyDesc {
	ushort k_code;			// Key code.
	CFABPtr k_cfab;			// Command or macro to execute.
	} KeyDesc;

// Key binding array (vector) for one 7-bit key of a key sequence: plain characters (0..127) + printable character of a function
// key (! .. ~) + S-TAB + shifted function key.
typedef KeyDesc KeyVect[NKeyVect];

// Control object for walking through the key binding table.
typedef struct {
	KeyVect *kvp;			// Key vector pointer (initially NULL).
	KeyDesc *kdp;			// Pointer to next binding.
	} KeyWalk;

// Built-in key binding (which is copied to appropriate list at startup).
typedef struct {
	ushort ki_code;			// Key code.
	enum cfid ki_id;		// Command-function id.
	} KeyItem;

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
#define CFVMove		0x0001		// Last command was a line up/down.
#define CFKill		0x0002		// Last command was a kill.
#define CFDel		0x0004		// Last command was a delete.
#define CFNMove		0x0008		// Last (yank) command did not move point.
#define CFTrav		0x0010		// Last command was a traverse.
#define CFYank		0x0020		// Last command was a yank.

// External function declarations.
extern int bindKeyCM(Datum *rp,int n,Datum **argpp);
extern char *ektos(ushort ek,char *dest,bool escTermAttr);
extern KeyDesc *getbind(ushort ek);
extern int getkey(ushort *keyp);
extern char *getkname(KeyDesc *kdp);
extern int getkseq(ushort *keyp,KeyDesc **kdpp);
extern KeyDesc *getpentry(CFABPtr *cfabp);
extern int loadbind(void);
extern KeyDesc *nextbind(KeyWalk *kwp);
extern int showKey(Datum *rp,int n,Datum **argpp);
extern int stoek(char *keylit,ushort *resultp);
extern void unbindent(KeyDesc *kdp);
extern int unbindKey(Datum *rp,int n,Datum **argpp);

#ifdef DATAbind

// **** For bind.c ****

// Global variables.
CoreKey corekeys[NCoreKeys];		// Cache of frequently-used keys.
KeyEntry kentry = {0,false,0,false,0,0};
					// Terminal key entry variables.
static KeyItem keyitems[] = {
	{Ctrl | ' ',cf_setMark},
	{Ctrl | 'A',cf_beginLine},
	{Ctrl | 'B',cf_backChar},
	{Ctrl | 'C',cf_prefix2},
	{Ctrl | 'D',cf_deleteForwChar},
	{Ctrl | 'E',cf_endLine},
	{Ctrl | 'F',cf_forwChar},
	{Ctrl | 'G',cf_abort},
	{Ctrl | 'H',cf_prefix3},
	{Ctrl | 'I',cf_tab},
	{Ctrl | 'J',cf_newlineI},
	{Ctrl | 'K',cf_deleteToBreak},
	{Ctrl | 'L',cf_deleteLine},
	{Ctrl | 'M',cf_newline},
	{Ctrl | 'N',cf_forwLine},
	{Ctrl | 'O',cf_openLine},
	{Ctrl | 'P',cf_backLine},
	{Ctrl | 'Q',cf_quoteChar},
	{Ctrl | 'R',cf_searchBack},
	{Ctrl | 'S',cf_searchForw},
	{Ctrl | 'T',cf_traverseLine},
	{Ctrl | 'U',cf_universalArg},
	{Ctrl | 'V',cf_forwPage},
	{Ctrl | 'W',cf_deleteRegion},
	{Ctrl | 'X',cf_prefix1},
	{Ctrl | 'Y',cf_yank},
	{Ctrl | 'Z',cf_backPage},
	{Ctrl | '[',cf_metaPrefix},
	{Ctrl | '\\',cf_deleteWord},
	{Ctrl | ']',cf_huntForw},
	{Ctrl | '^',cf_huntBack},
	{Ctrl | '_',cf_negativeArg},
	{Ctrl | '?',cf_backspace},

	{Shft | Ctrl | 'I',cf_deleteBackTab},

	{' ',cf_space},

// Function key bindings.
	{FKey | '<',cf_beginBuf},		// Home.
	{FKey | '>',cf_endBuf},			// End.
	{FKey | 'B',cf_backChar},		// Left arrow.
	{FKey | 'D',cf_deleteForwChar},		// Delete [x> key.
	{FKey | 'F',cf_forwChar},		// Right arrow.
	{FKey | 'N',cf_forwLine},		// Down arrow.
	{FKey | 'P',cf_backLine},		// Up arrow.
	{FKey | 'V',cf_forwPage},		// PgDn.
	{FKey | 'Z',cf_backPage},		// PgUp.
#if CENTOS || DEBIAN || MACOS
	{Shft | FKey | 'B',cf_backWord},	// Shift left arrow.
	{Shft | FKey | 'D',cf_deleteWhite},	// Shift delete [x> key.
	{Shft | FKey | 'F',cf_forwWord},	// Shift right arrow.
#endif

// Prefix1 (C-x) bindings:
	{Pref1 | Ctrl | ' ',cf_deleteMark},
	{Pref1 | Ctrl | 'A',cf_appendFile},
	{Pref1 | Ctrl | 'B',cf_deleteBuf},
	{Pref1 | Ctrl | 'C',cf_exit},
	{Pref1 | Ctrl | 'D',cf_detabLine},
	{Pref1 | Ctrl | 'E',cf_entabLine},
	{Pref1 | Ctrl | 'F',cf_findFile},
	{Pref1 | Ctrl | 'I',cf_insertFile},
	{Pref1 | Ctrl | 'L',cf_lowerCaseRegion},
	{Pref1 | Ctrl | 'N',cf_moveWindDown},
	{Pref1 | Ctrl | 'O',cf_deleteBlankLines},
	{Pref1 | Ctrl | 'P',cf_moveWindUp},
	{Pref1 | Ctrl | 'R',cf_readFile},
	{Pref1 | Ctrl | 'S',cf_saveFile},
	{Pref1 | Ctrl | 'T',cf_titleCaseRegion},
	{Pref1 | Ctrl | 'U',cf_upperCaseRegion},
	{Pref1 | Ctrl | 'V',cf_viewFile},
	{Pref1 | Ctrl | 'W',cf_writeFile},
	{Pref1 | Ctrl | 'X',cf_swapMark},
	{Pref1 | Ctrl | 'Z',cf_suspend},
#if 0
	{Pref1 | Ctrl | '[',cf_???},
#endif
	{Pref1 | Ctrl | '\\',cf_trimLine},
#if 0
	{Pref1 | Ctrl | ']',cf_???},
	{Pref1 | Ctrl | '^',cf_???},
	{Pref1 | Ctrl | '_',cf_???},
#endif
	{Pref1 | Ctrl | '?',cf_clearBuf},

	{Pref1 | ' ',cf_insertSpace},
	{Pref1 | '!',cf_shellCmd},
#if 0
	{Pref1 | '"',cf_???},
#endif
	{Pref1 | '#',cf_scratchBuf},
	{Pref1 | '$',cf_shell},
#if 0
	{Pref1 | '%',cf_???},
	{Pref1 | '&',cf_???},
	{Pref1 | '\'',cf_???},
#endif
	{Pref1 | '(',cf_beginKeyMacro},
	{Pref1 | ')',cf_endKeyMacro},
#if 0
	{Pref1 | '*',cf_???},
#endif
	{Pref1 | '+',cf_growWind},
#if 0
	{Pref1 | ',',cf_???},
#endif
	{Pref1 | '-',cf_shrinkWind},
#if 0
	{Pref1 | '.',cf_???},
#endif
	{Pref1 | '/',cf_xeqFile},
	{Pref1 | '0',cf_deleteWind},
	{Pref1 | '1',cf_onlyWind},
	{Pref1 | '2',cf_splitWind},
#if 0
	{Pref1 | ':',cf_???},
	{Pref1 | ';',cf_???},
#endif
	{Pref1 | '<',cf_narrowBuf},
#if 0
	{Pref1 | '=',cf_???},
#endif
	{Pref1 | '>',cf_widenBuf},
#if 0
	{Pref1 | '?',cf_???},
#endif
	{Pref1 | '@',cf_deleteMacro},

	{Pref1 | 'a',cf_alterBufAttr},
	{Pref1 | 'B',cf_renameBuf},
	{Pref1 | 'b',cf_selectBuf},
	{Pref1 | 'e',cf_xeqKeyMacro},
	{Pref1 | 'F',cf_setBufFile},
	{Pref1 | 'h',cf_setHook},
	{Pref1 | 'i',cf_insertLineI},
	{Pref1 | 'j',cf_joinWind},
	{Pref1 | 'k',cf_deleteKill},
	{Pref1 | 'l',cf_lowerCaseLine},
	{Pref1 | 'm',cf_alterBufMode},
	{Pref1 | 'n',cf_nextWind},
	{Pref1 | 'p',cf_prevWind},
	{Pref1 | 'r',cf_deleteReplacePat},
	{Pref1 | 's',cf_deleteSearchPat},
	{Pref1 | 't',cf_titleCaseLine},
	{Pref1 | 'u',cf_upperCaseLine},
	{Pref1 | 'v',cf_filterBuf},
	{Pref1 | 'w',cf_selectWind},
	{Pref1 | 'x',cf_xeqBuf},
	{Pref1 | 'z',cf_resizeWind},

	{Pref1 | '[',cf_prevBuf},
	{Pref1 | '\\',cf_lastBuf},
	{Pref1 | ']',cf_nextBuf},
	{Pref1 | '^',cf_insertBuf},
#if 0
	{Pref1 | '_',cf_???},
#endif
	{Pref1 | '`',cf_insertPipe},
#if 0
	{Pref1 | '{',cf_???},
	{Pref1 | '|',cf_???},
#endif
	{Pref1 | '}',cf_deleteFencedRegion},
	{Pref1 | '~',cf_readPipe},
#if 0
	{Pref1 | FKey | 'B',cf_???},		// Left arrow.
	{Pref1 | FKey | 'F',cf_???},		// Right arrow.
	{Pref1 | FKey | 'P',cf_???},		// Up arrow.
	{Pref1 | FKey | 'N',cf_???},		// Down arrow.
#if CENTOS || DEBIAN || MACOS
	{Pref1 | Shft | FKey | 'B',cf_???},	// Shift left arrow.
	{Pref1 | Shft | FKey | 'F',cf_???},	// Shift right arrow.
#endif
#endif

// Prefix2 (C-c) bindings:
#if 0
	{Pref2 | Ctrl | ' ',cf_???},
#endif
	{Pref2 | Ctrl | 'A',cf_deleteAlias},
	{Pref2 | Ctrl | 'K',cf_copyToBreak},
	{Pref2 | Ctrl | 'L',cf_copyLine},
	{Pref2 | Ctrl | 'W',cf_copyRegion},
#if 0
	{Pref2 | Ctrl | '[',cf_???},
#endif
	{Pref2 | Ctrl | '\\',cf_copyWord},
#if 0
	{Pref2 | Ctrl | ']',cf_???},
	{Pref2 | Ctrl | '^',cf_???},
	{Pref2 | Ctrl | '_',cf_???},
	{Pref2 | Ctrl | '?',cf_???},

	{Pref2 | ' ',cf_???},
	{Pref2 | '!',cf_???},
	{Pref2 | '"',cf_???},
	{Pref2 | '#',cf_???},
	{Pref2 | '$',cf_???},
	{Pref2 | '%',cf_???},
	{Pref2 | '&',cf_???},
	{Pref2 | '\'',cf_???},
	{Pref2 | '(',cf_???},
	{Pref2 | ')',cf_???},
	{Pref2 | '*',cf_???},
	{Pref2 | '+',cf_???},
	{Pref2 | ',',cf_???},
	{Pref2 | '-',cf_???},
	{Pref2 | '.',cf_???},
	{Pref2 | '/',cf_???},
	{Pref2 | '0',cf_???},
	{Pref2 | ':',cf_???},
	{Pref2 | ';',cf_???},
	{Pref2 | '<',cf_???},
	{Pref2 | '=',cf_???},
	{Pref2 | '>',cf_???},
	{Pref2 | '@',cf_???},
#endif
	{Pref2 | 'a',cf_alias},
	{Pref2 | 'd',cf_chDir},
	{Pref2 | 'i',cf_inserti},
	{Pref2 | 'k',cf_cycleKillRing},
	{Pref2 | 'r',cf_cycleReplaceRing},
	{Pref2 | 's',cf_cycleSearchRing},

#if 0
	{Pref2 | '[',cf_???},
	{Pref2 | '\\',cf_???},
	{Pref2 | ']',cf_???},
	{Pref2 | '^',cf_???},
	{Pref2 | '_',cf_???},
	{Pref2 | '`',cf_???},
	{Pref2 | '{',cf_???},
	{Pref2 | '|',cf_???},
#endif
	{Pref2 | '}',cf_copyFencedRegion},
#if 0
	{Pref2 | '~',cf_???},
	{Pref2 | '?',cf_???},

	{Pref2 | FKey | 'B',cf_???},		// Left arrow.
	{Pref2 | FKey | 'F',cf_???},		// Right arrow.
	{Pref2 | FKey | 'P',cf_???},		// Up arrow.
	{Pref2 | FKey | 'N',cf_???},		// Down arrow.
#if CENTOS || DEBIAN || MACOS
	{Pref2 | Shft | FKey | 'B',cf_???},	// Shift left arrow.
	{Pref2 | Shft | FKey | 'F',cf_???},	// Shift right arrow.
#endif
#endif

// Prefix3 (C-h) bindings:
#if 0
	{Pref3 | Ctrl | ' ',cf_???},
#endif
	{Pref3 | Ctrl | 'A',cf_beginWhite},
	{Pref3 | Ctrl | 'E',cf_endWhite},
	{Pref3 | Ctrl | 'K',cf_killToBreak},
	{Pref3 | Ctrl | 'L',cf_killLine},
	{Pref3 | Ctrl | 'P',cf_popFile},
	{Pref3 | Ctrl | 'V',cf_forwPagePrev},
	{Pref3 | Ctrl | 'W',cf_killRegion},
	{Pref3 | Ctrl | 'Z',cf_backPagePrev},
#if 0
	{Pref3 | Ctrl | '[',cf_???},
#endif
	{Pref3 | Ctrl | '\\',cf_killWord},
#if 0
	{Pref3 | Ctrl | ']',cf_???},
	{Pref3 | Ctrl | '^',cf_???},
	{Pref3 | Ctrl | '_',cf_???},
	{Pref3 | Ctrl | '?',cf_???},
#endif
	{Pref3 | ' ',cf_showMarks},
#if 0
	{Pref3 | '!',cf_???},
	{Pref3 | '"',cf_???},
	{Pref3 | '#',cf_???},
	{Pref3 | '$',cf_???},
	{Pref3 | '%',cf_???},
	{Pref3 | '&',cf_???},
	{Pref3 | '\'',cf_???},
	{Pref3 | '(',cf_???},
	{Pref3 | ')',cf_???},
	{Pref3 | '*',cf_???},
	{Pref3 | '+',cf_???},
	{Pref3 | ',',cf_???},
	{Pref3 | '-',cf_???},
#endif
	{Pref3 | '.',cf_showPoint},
#if 0
	{Pref3 | '/',cf_???},
	{Pref3 | '0',cf_???},
	{Pref3 | ':',cf_???},
	{Pref3 | ';',cf_???},
	{Pref3 | '<',cf_???},
	{Pref3 | '=',cf_???},
	{Pref3 | '>',cf_???},
#endif
	{Pref3 | '?',cf_showKey},
	{Pref3 | '@',cf_showMacros},

	{Pref3 | 'A',cf_about},
	{Pref3 | 'a',cf_showAliases},
	{Pref3 | 'b',cf_showBuffers},
	{Pref3 | 'c',cf_showCommands},
	{Pref3 | 'f',cf_showFunctions},
	{Pref3 | 'h',cf_showHooks},
	{Pref3 | 'k',cf_showKillRing},
	{Pref3 | 'M',cf_showModes},
	{Pref3 | 'm',cf_alterShowMode},
	{Pref3 | 'p',cf_popBuf},
	{Pref3 | 'S',cf_showScreens},
	{Pref3 | 's',cf_showSearchRing},
	{Pref3 | 'r',cf_showReplaceRing},
	{Pref3 | 'v',cf_showVariables},
#if 0
	{Pref3 | '[',cf_???},
	{Pref3 | '\\',cf_???},
	{Pref3 | ']',cf_???},
	{Pref3 | '^',cf_???},
	{Pref3 | '_',cf_???},
	{Pref3 | '`',cf_???},
	{Pref3 | '{',cf_???},
	{Pref3 | '|',cf_???},
#endif
	{Pref3 | '}',cf_killFencedRegion},
#if MMDebug & Debug_ShowRE
	{Pref3 | '~',cf_showRegexp},
#endif
#if 0
	{Pref3 | FKey | 'B',cf_???},		// Left arrow.
	{Pref3 | FKey | 'F',cf_???},		// Right arrow.
	{Pref3 | FKey | 'P',cf_???},		// Up arrow.
	{Pref3 | FKey | 'N',cf_???},		// Down arrow.
#if CENTOS || DEBIAN || MACOS
	{Pref3 | Shft | FKey | 'B',cf_???},	// Shift left arrow.
	{Pref3 | Shft | FKey | 'F',cf_???},	// Shift right arrow.
#endif
#endif

// Meta (ESC) bindings.
	{Meta | Ctrl | ' ',cf_markBuf},
	{Meta | Ctrl | 'A',cf_beginText},
	{Meta | Ctrl | 'B',cf_backTab},
	{Meta | Ctrl | 'D',cf_deleteScreen},
	{Meta | Ctrl | 'E',cf_endWord},
	{Meta | Ctrl | 'F',cf_forwTab},
#if 0
	{Meta | Ctrl | 'G',cf_???},
#endif
	{Meta | Ctrl | 'I',cf_deleteForwTab},
	{Meta | Ctrl | 'J',cf_joinLines},
	{Meta | Ctrl | 'K',cf_unbindKey},
	{Meta | Ctrl | 'L',cf_lowerCaseWord},
	{Meta | Ctrl | 'M',cf_wrapLine},
	{Meta | Ctrl | 'T',cf_titleCaseWord},
	{Meta | Ctrl | 'U',cf_upperCaseWord},
	{Meta | Ctrl | 'V',cf_forwPageNext},
	{Meta | Ctrl | 'X',cf_eval},
	{Meta | Ctrl | 'Y',cf_yankCycle},
	{Meta | Ctrl | 'Z',cf_backPageNext},
	{Meta | Ctrl | '[',cf_resetTerm},
#if 0
	{Meta | Ctrl | '\\',cf_???},
	{Meta | Ctrl | ']',cf_???},
	{Meta | Ctrl | '^',cf_???},
	{Meta | Ctrl | '_',cf_???},
#endif
	{Meta | Ctrl | '?',cf_deleteBackChar},

	{Meta | ' ',cf_gotoMark},
#if 0
	{Meta | '!',cf_???},
	{Meta | '"',cf_???},
#endif
#if WordCount
	{Meta | '#',cf_countWords},
#endif
#if 0
	{Meta | '$',cf_???},
	{Meta | '%',cf_???},
	{Meta | '&',cf_???},
	{Meta | '\'',cf_???},
#endif
	{Meta | '(',cf_outdentRegion},
	{Meta | ')',cf_indentRegion},
#if 0
	{Meta | '*',cf_???},
	{Meta | '+',cf_???},
	{Meta | ',',cf_???},
	{Meta | '-',cf_???},
	{Meta | '.',cf_???},
	{Meta | '/',cf_???},
	{Meta | '0',cf_???},
	{Meta | ':',cf_???},
	{Meta | ';',cf_???},
#endif
	{Meta | '<',cf_beginBuf},
	{Meta | '=',cf_let},
	{Meta | '>',cf_endBuf},
	{Meta | '?',cf_help},
#if 0
	{Meta | '@',cf_???},
#endif
	{Meta | 'a',cf_apropos},
	{Meta | 'b',cf_backWord},
	{Meta | 'd',cf_dupLine},
	{Meta | 'f',cf_forwWord},
	{Meta | 'g',cf_gotoLine},
	{Meta | 'i',cf_seti},
	{Meta | 'k',cf_bindKey},
	{Meta | 'l',cf_reframeWind},
	{Meta | 'm',cf_alterGlobalMode},
	{Meta | 'q',cf_queryReplace},
	{Meta | 'r',cf_replace},
	{Meta | 's',cf_selectScreen},
	{Meta | 't',cf_truncBuf},
	{Meta | 'u',cf_undelete},
	{Meta | 'w',cf_setWrapCol},
	{Meta | 'x',cf_run},
	{Meta | 'z',cf_quickExit},

	{Meta | '[',cf_prevScreen},
	{Meta | '\\',cf_deleteWhite},
	{Meta | ']',cf_nextScreen},
#if 0
	{Meta | '^',cf_???},
	{Meta | '_',cf_???},
	{Meta | '`',cf_???},
	{Meta | '{',cf_???},
	{Meta | '|',cf_???},
#endif
	{Meta | '}',cf_gotoFence},
#if 0
	{Meta | '~',cf_???},

	{Meta | FKey | 'B',cf_???},		// Left arrow.
	{Meta | FKey | 'F',cf_???},		// Right arrow.
	{Meta | FKey | 'P',cf_???},		// Up arrow.
	{Meta | FKey | 'N',cf_???},		// Down arrow.
#if CENTOS || DEBIAN || MACOS
	{Meta | Shft | FKey | 'B',cf_???},	// Shift left arrow.
	{Meta | Shft | FKey | 'F',cf_???},	// Shift right arrow.
#endif
#endif
	{0,-1}
};

// Key binding table.  Contains an array of KeyVect objects for each prefix key plus one more for all unprefixed bindings (the
// first entry).  Each KeyVect array contains 128 KeyDesc objects -- a slot for every possible 7-bit character.  Unused slots
// have a k_code of zero, which is not a valid extended key value.
KeyVect keytab[NKeyTab];

#else

// **** For all the other .c files ****

// External variable declarations.
extern CoreKey corekeys[];
extern KeyEntry kentry;
extern KeyVect keytab[];
#endif
