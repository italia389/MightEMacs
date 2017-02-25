// (c) Copyright 2017 Richard W. Marinelli
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
//	Fenced region	^H { or }	^X { or }	^C { or }

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
extern char *ektos(ushort ek,char *dest);
extern KeyDesc *getbind(ushort ek);
extern int getkey(ushort *keyp);
extern char *getkname(KeyDesc *kdp);
extern int getkseq(ushort *keyp,KeyDesc **kdpp);
extern KeyDesc *getpentry(CFABPtr *cfabp);
extern int loadbind(void);
extern KeyDesc *nextbind(KeyWalk *kwp);
extern int showBindings(Datum *rp,int n,Datum **argpp);
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
	{Ctrl|'[',cf_metaPrefix},		// Put prefix bindings at top so they are found quickly.
	{Ctrl|'X',cf_prefix1},
	{Ctrl|'C',cf_prefix2},
	{Ctrl|'H',cf_prefix3},

	{Ctrl|' ',cf_setMark},
	{Ctrl|'A',cf_beginLine},
	{Ctrl|'B',cf_backChar},
	{Ctrl|'D',cf_deleteForwChar},
	{Ctrl|'E',cf_endLine},
	{Ctrl|'F',cf_forwChar},
	{Ctrl|'G',cf_abort},
	{Ctrl|'I',cf_tab},
	{Ctrl|'J',cf_newlineI},
	{Ctrl|'K',cf_deleteToBreak},
	{Ctrl|'L',cf_deleteLine},
	{Ctrl|'M',cf_newline},
	{Ctrl|'N',cf_forwLine},
	{Ctrl|'O',cf_openLine},
	{Ctrl|'P',cf_backLine},
	{Ctrl|'Q',cf_quoteChar},
	{Ctrl|'R',cf_searchBack},
	{Ctrl|'S',cf_searchForw},
	{Ctrl|'T',cf_traverseLine},
	{Ctrl|'U',cf_universalArg},
	{Ctrl|'V',cf_forwPage},
	{Ctrl|'W',cf_deleteRegion},
	{Ctrl|'Y',cf_yank},
	{Ctrl|'Z',cf_backPage},
	{Ctrl|'\\',cf_deleteWord},
	{Ctrl|']',cf_huntForw},
	{Ctrl|'^',cf_huntBack},
	{Ctrl|'_',cf_negativeArg},
	{Ctrl|'?',cf_deleteBackChar},
	{Shft|Ctrl|'I',cf_deleteBackTab},

	{' ',cf_space},

	{Pref1|Ctrl|' ',cf_deleteMark},
	{Pref1|Ctrl|'A',cf_appendFile},
	{Pref1|Ctrl|'B',cf_selectBuf},
	{Pref1|Ctrl|'C',cf_exit},
	{Pref1|Ctrl|'D',cf_detabLine},
	{Pref1|Ctrl|'E',cf_entabLine},
	{Pref1|Ctrl|'F',cf_findFile},
	{Pref1|Ctrl|'I',cf_insertFile},
	{Pref1|Ctrl|'K',cf_unbindKey},
	{Pref1|Ctrl|'L',cf_lcRegion},
	{Pref1|Ctrl|'M',cf_deleteMacro},
	{Pref1|Ctrl|'N',cf_moveWindDown},
	{Pref1|Ctrl|'O',cf_deleteBlankLines},
	{Pref1|Ctrl|'P',cf_moveWindUp},
	{Pref1|Ctrl|'R',cf_readFile},
	{Pref1|Ctrl|'S',cf_saveFile},
	{Pref1|Ctrl|'T',cf_trimLine},
	{Pref1|Ctrl|'U',cf_ucRegion},
	{Pref1|Ctrl|'V',cf_viewFile},
	{Pref1|Ctrl|'W',cf_writeFile},
	{Pref1|Ctrl|'X',cf_swapMark},
	{Pref1|Ctrl|'Z',cf_suspend},
#if 0
	{Pref1|Ctrl|'[',cf_???},
	{Pref1|Ctrl|'\\',cf_???},
	{Pref1|Ctrl|']',cf_???},
	{Pref1|Ctrl|'^',cf_???},
	{Pref1|Ctrl|'_',cf_???},
#endif
	{Pref1|Ctrl|'?',cf_clearBuf},

	{Pref1|' ',cf_insertSpace},
	{Pref1|'!',cf_shellCmd},
	{Pref1|'$',cf_shell},
	{Pref1|'(',cf_beginKeyMacro},
	{Pref1|')',cf_endKeyMacro},
	{Pref1|'+',cf_growWind},
	{Pref1|'-',cf_shrinkWind},
	{Pref1|'.',cf_whence},
	{Pref1|'/',cf_xeqFile},
	{Pref1|'0',cf_deleteWind},
	{Pref1|'1',cf_onlyWind},
	{Pref1|'2',cf_splitWind},
	{Pref1|'<',cf_narrowBuf},
	{Pref1|'=',cf_let},
	{Pref1|'>',cf_widenBuf},
#if 0
	{Pref1|'?',cf_showKey},
#endif
	{Pref1|'@',cf_readPipe},

	{Pref1|'B',cf_setBufName},
	{Pref1|'E',cf_xeqKeyMacro},
	{Pref1|'F',cf_setBufFile},
	{Pref1|'H',cf_setHook},
	{Pref1|'I',cf_seti},
	{Pref1|'J',cf_joinWind},
	{Pref1|'K',cf_deleteBuf},
	{Pref1|'L',cf_lcLine},
	{Pref1|'M',cf_alterBufMode},
	{Pref1|'N',cf_nextWind},
	{Pref1|'P',cf_prevWind},
	{Pref1|'S',cf_scratchBuf},
	{Pref1|'U',cf_ucLine},
	{Pref1|'W',cf_resizeWind},
	{Pref1|'X',cf_xeqBuf},
	{Pref1|'Y',cf_cycleKillRing},
	{Pref1|'[',cf_prevBuf},
	{Pref1|'\\',cf_lastBuf},
	{Pref1|']',cf_nextBuf},
	{Pref1|'^',cf_insertBuf},
#if 0
	{Pref1|'_',cf_???},
#endif
	{Pref1|'`',cf_insertPipe},
	{Pref1|'{',cf_deleteFencedText},
	{Pref1|'|',cf_pipeBuf},
	{Pref1|'}',cf_deleteFencedText},
#if 0
	{Pref2|Ctrl|' ',cf_???},
#endif
	{Pref2|Ctrl|'A',cf_deleteAlias},
	{Pref2|Ctrl|'K',cf_copyToBreak},
	{Pref2|Ctrl|'L',cf_copyLine},
	{Pref2|Ctrl|'W',cf_copyRegion},
#if 0
	{Pref2|Ctrl|'[',cf_???},
#endif
	{Pref2|Ctrl|'\\',cf_copyWord},
#if 0
	{Pref2|Ctrl|']',cf_???},
	{Pref2|Ctrl|'^',cf_???},
	{Pref2|Ctrl|'_',cf_???},
	{Pref2|Ctrl|'?',cf_???},

	{Pref2|' ',cf_???},
#endif
	{Pref2|'.',cf_chDir},
	{Pref2|'A',cf_alias},
	{Pref2|'I',cf_inserti},
	{Pref2|'M',cf_alterDefMode},
	{Pref2|'{',cf_copyFencedText},
	{Pref2|'}',cf_copyFencedText},
#if 0
	{Pref3|Ctrl|' ',cf_???},
#endif
	{Pref3|Ctrl|'A',cf_beginWhite},
	{Pref3|Ctrl|'E',cf_endWhite},
	{Pref3|Ctrl|'K',cf_killToBreak},
	{Pref3|Ctrl|'L',cf_killLine},
	{Pref3|Ctrl|'V',cf_forwPagePrev},
	{Pref3|Ctrl|'W',cf_killRegion},
	{Pref3|Ctrl|'Z',cf_backPagePrev},
#if 0
	{Pref3|Ctrl|'[',cf_???},
#endif
	{Pref3|Ctrl|'\\',cf_killWord},
#if 0
	{Pref3|Ctrl|']',cf_???},
	{Pref3|Ctrl|'^',cf_???},
	{Pref3|Ctrl|'_',cf_???},
	{Pref3|Ctrl|'?',cf_???},
#endif
	{Pref3|' ',cf_showMarks},
	{Pref3|'?',cf_showKey},
	{Pref3|'A',cf_about},
	{Pref3|'B',cf_showBuffers},
	{Pref3|'F',cf_showFunctions},
	{Pref3|'H',cf_showHooks},
	{Pref3|'K',cf_showKillRing},
	{Pref3|'M',cf_alterShowMode},
	{Pref3|'N',cf_showBindings},
	{Pref3|'O',cf_showModes},
#if MMDebug & Debug_ShowRE
	{Pref3|'R',cf_showRegexp},
#endif
	{Pref3|'S',cf_showScreens},
	{Pref3|'V',cf_showVariables},
	{Pref3|'{',cf_killFencedText},
	{Pref3|'}',cf_killFencedText},

	{Meta|Ctrl|' ',cf_markBuf},
	{Meta|Ctrl|'A',cf_beginText},
	{Meta|Ctrl|'B',cf_backTab},
	{Meta|Ctrl|'D',cf_deleteScreen},
	{Meta|Ctrl|'E',cf_endWord},
	{Meta|Ctrl|'F',cf_forwTab},
#if 0
	{Meta|Ctrl|'G',cf_gotoMark},
#endif
	{Meta|Ctrl|'H',cf_unhideBuf},
	{Meta|Ctrl|'I',cf_deleteForwTab},
	{Meta|Ctrl|'J',cf_joinLines},
	{Meta|Ctrl|'L',cf_lcWord},
	{Meta|Ctrl|'M',cf_wrapLine},
	{Meta|Ctrl|'T',cf_tcWord},
	{Meta|Ctrl|'U',cf_ucWord},
	{Meta|Ctrl|'V',cf_forwPageNext},
	{Meta|Ctrl|'X',cf_eval},
	{Meta|Ctrl|'Y',cf_clearKillRing},
	{Meta|Ctrl|'Z',cf_backPageNext},
	{Meta|Ctrl|'[',cf_resetTerm},
#if 0
	{Meta|Ctrl|'\\',cf_???},
	{Meta|Ctrl|']',cf_???},
	{Meta|Ctrl|'^',cf_???},
	{Meta|Ctrl|'_',cf_???},
	{Meta|Ctrl|'?',cf_???},
#endif
	{Meta|' ',cf_gotoMark},
	{Meta|'(',cf_outdentRegion},
	{Meta|')',cf_indentRegion},
#if 0
	{Meta|'.',cf_gotoMark},
	{Meta|'/',cf_xeqFile},
#endif
	{Meta|'<',cf_beginBuf},
#if WordCount
	{Meta|'=',cf_countWords},
#endif
	{Meta|'>',cf_endBuf},
	{Meta|'?',cf_help},
	{Meta|'B',cf_backWord},
	{Meta|'D',cf_dupLine},
	{Meta|'F',cf_forwWord},
	{Meta|'G',cf_gotoLine},
	{Meta|'H',cf_hideBuf},
	{Meta|'I',cf_insertLineI},
	{Meta|'K',cf_bindKey},
	{Meta|'L',cf_redrawScreen},
	{Meta|'M',cf_alterGlobalMode},
	{Meta|'Q',cf_queryReplace},
	{Meta|'R',cf_replace},
	{Meta|'S',cf_newScreen},
	{Meta|'T',cf_truncBuf},
	{Meta|'U',cf_undelete},
	{Meta|'W',cf_setWrapCol},
	{Meta|'X',cf_run},
	{Meta|'Y',cf_yankPop},
	{Meta|'Z',cf_quickExit},
	{Meta|'[',cf_prevScreen},
	{Meta|'\\',cf_deleteWhite},
	{Meta|']',cf_nextScreen},
	{Meta|'{',cf_gotoFence},
	{Meta|'}',cf_gotoFence},
	{Meta|'~',cf_unchangeBuf},

	{FKey|'<',cf_beginBuf},			// Home.
	{FKey|'>',cf_endBuf},			// End.
	{FKey|'B',cf_backChar},			// Left.
	{FKey|'D',cf_deleteForwChar},		// Delete.
	{FKey|'F',cf_forwChar},			// Right.
	{FKey|'N',cf_forwLine},			// Down.
	{FKey|'P',cf_backLine},			// Up.
	{FKey|'V',cf_forwPage},			// PgDn.
	{FKey|'Z',cf_backPage},			// PgUp.

#if CENTOS || DEBIAN || MACOS
	{Shft|FKey|'B',cf_backWord},		// Shift left.
	{Shft|FKey|'D',cf_deleteWhite},		// Shift forward delete.
	{Shft|FKey|'F',cf_forwWord},		// Shift right.
#endif
	{0,-1}
};

// Key binding table.  Contains an array of KeyVect objects for each prefix key plus one more for all unprefixed bindings (the
// first entry).  Each KeyVect array contains 128 KeyDesc objects -- a slot for every possible 7-bit character.  Unused slots
// have a k_code of zero, which is not a valid extended key value.
KeyVect keytab[NPrefix + 1];

#else

// **** For all the other .c files ****

// External variable declarations.
extern CoreKey corekeys[];
extern KeyEntry kentry;
extern KeyVect keytab[];
#endif
