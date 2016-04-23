// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// ebind.h	Default key-to-command bindings for MightEMacs.

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

// Built-in key binding (which is copied to appropriate list at startup).
typedef struct {
	ushort ki_code;			// Key code.
	enum cfid ki_id;		// Command-function id.
	} KeyItem;

#ifdef DATAbind
static KeyItem keyitems[] = {
	{CTRL|'[',cf_metaPrefix},		// Put prefix bindings at top so they are found quickly.
	{CTRL|'X',cf_prefix1},
	{CTRL|'C',cf_prefix2},
	{CTRL|'H',cf_prefix3},

	{CTRL|' ',cf_setMark},
	{CTRL|'A',cf_beginLine},
	{CTRL|'B',cf_backChar},
	{CTRL|'D',cf_deleteForwChar},
	{CTRL|'E',cf_endLine},
	{CTRL|'F',cf_forwChar},
	{CTRL|'G',cf_abort},
	{CTRL|'I',cf_tab},
	{CTRL|'J',cf_newlineI},
	{CTRL|'K',cf_deleteToBreak},
	{CTRL|'L',cf_deleteLine},
	{CTRL|'M',cf_newline},
	{CTRL|'N',cf_forwLine},
	{CTRL|'O',cf_openLine},
	{CTRL|'P',cf_backLine},
	{CTRL|'Q',cf_quoteChar},
	{CTRL|'R',cf_searchBack},
	{CTRL|'S',cf_searchForw},
	{CTRL|'T',cf_traverseLine},
	{CTRL|'U',cf_universalArg},
	{CTRL|'V',cf_forwPage},
	{CTRL|'W',cf_deleteRegion},
	{CTRL|'Y',cf_yank},
	{CTRL|'Z',cf_backPage},
	{CTRL|'\\',cf_deleteWord},
	{CTRL|']',cf_huntForw},
	{CTRL|'^',cf_huntBack},
	{CTRL|'_',cf_negativeArg},
	{CTRL|'?',cf_deleteBackChar},
	{SHFT|CTRL|'I',cf_deleteTab},

	{' ',cf_space},

	{PREF1|CTRL|' ',cf_deleteMark},
	{PREF1|CTRL|'A',cf_appendFile},
	{PREF1|CTRL|'B',cf_selectBuf},
	{PREF1|CTRL|'C',cf_exit},
	{PREF1|CTRL|'D',cf_detabLine},
	{PREF1|CTRL|'E',cf_entabLine},
	{PREF1|CTRL|'F',cf_findFile},
	{PREF1|CTRL|'I',cf_insertFile},
	{PREF1|CTRL|'K',cf_unbindKey},
	{PREF1|CTRL|'L',cf_lcRegion},
	{PREF1|CTRL|'M',cf_deleteMacro},
	{PREF1|CTRL|'N',cf_moveWindDown},
	{PREF1|CTRL|'O',cf_deleteBlankLines},
	{PREF1|CTRL|'P',cf_moveWindUp},
	{PREF1|CTRL|'R',cf_readFile},
	{PREF1|CTRL|'S',cf_saveFile},
	{PREF1|CTRL|'T',cf_trimLine},
	{PREF1|CTRL|'U',cf_ucRegion},
	{PREF1|CTRL|'V',cf_viewFile},
	{PREF1|CTRL|'W',cf_writeFile},
	{PREF1|CTRL|'X',cf_swapMark},
	{PREF1|CTRL|'Z',cf_suspend},
#if 0
	{PREF1|CTRL|'[',cf_???},
	{PREF1|CTRL|'\\',cf_???},
	{PREF1|CTRL|']',cf_???},
	{PREF1|CTRL|'^',cf_???},
	{PREF1|CTRL|'_',cf_???},
#endif
	{PREF1|CTRL|'?',cf_clearBuf},

	{PREF1|' ',cf_insertSpace},
	{PREF1|'!',cf_shellCmd},
	{PREF1|'$',cf_shell},
	{PREF1|'(',cf_beginKeyMacro},
	{PREF1|')',cf_endKeyMacro},
	{PREF1|'+',cf_growWind},
	{PREF1|'-',cf_shrinkWind},
	{PREF1|'.',cf_whence},
	{PREF1|'0',cf_deleteWind},
	{PREF1|'1',cf_onlyWind},
	{PREF1|'2',cf_splitWind},
	{PREF1|'<',cf_narrowBuf},
	{PREF1|'=',cf_let},
	{PREF1|'>',cf_widenBuf},
#if 0
	{PREF1|'?',cf_showKey},
#endif
	{PREF1|'@',cf_readPipe},

	{PREF1|'B',cf_setBufName},
	{PREF1|'E',cf_xeqKeyMacro},
	{PREF1|'F',cf_setBufFile},
	{PREF1|'H',cf_setHook},
	{PREF1|'I',cf_seti},
	{PREF1|'J',cf_joinWind},
	{PREF1|'K',cf_deleteBuf},
	{PREF1|'L',cf_lcLine},
	{PREF1|'M',cf_alterBufMode},
	{PREF1|'N',cf_nextWind},
	{PREF1|'P',cf_prevWind},
	{PREF1|'S',cf_scratchBuf},
	{PREF1|'U',cf_ucLine},
	{PREF1|'W',cf_resizeWind},
	{PREF1|'X',cf_xeqBuf},
	{PREF1|'Y',cf_cycleKillRing},
	{PREF1|'[',cf_prevBuf},
#if 0
	{PREF1|'\\',cf_???},
#endif
	{PREF1|']',cf_nextBuf},
	{PREF1|'^',cf_insertBuf},
#if 0
	{PREF1|'_',cf_???},
#endif
	{PREF1|'`',cf_insertPipe},
	{PREF1|'{',cf_deleteFencedText},
	{PREF1|'|',cf_pipeBuf},
	{PREF1|'}',cf_deleteFencedText},
#if 0
	{PREF2|CTRL|' ',cf_???},
#endif
	{PREF2|CTRL|'A',cf_deleteAlias},
	{PREF2|CTRL|'K',cf_copyToBreak},
	{PREF2|CTRL|'L',cf_copyLine},
	{PREF2|CTRL|'W',cf_copyRegion},
#if 0
	{PREF2|CTRL|'[',cf_???},
#endif
	{PREF2|CTRL|'\\',cf_copyWord},
#if 0
	{PREF2|CTRL|']',cf_???},
	{PREF2|CTRL|'^',cf_???},
	{PREF2|CTRL|'_',cf_???},
	{PREF2|CTRL|'?',cf_???},

	{PREF2|' ',cf_???},
#endif
	{PREF2|'.',cf_chDir},
	{PREF2|'A',cf_alias},
	{PREF2|'M',cf_alterDefMode},
	{PREF2|'{',cf_copyFencedText},
	{PREF2|'}',cf_copyFencedText},
#if 0
	{PREF3|CTRL|' ',cf_???},
#endif
	{PREF3|CTRL|'A',cf_beginWhite},
	{PREF3|CTRL|'E',cf_endWhite},
	{PREF3|CTRL|'K',cf_killToBreak},
	{PREF3|CTRL|'L',cf_killLine},
	{PREF3|CTRL|'V',cf_forwPagePrev},
	{PREF3|CTRL|'W',cf_killRegion},
	{PREF3|CTRL|'Z',cf_backPagePrev},
#if 0
	{PREF3|CTRL|'[',cf_???},
#endif
	{PREF3|CTRL|'\\',cf_killWord},
#if 0
	{PREF3|CTRL|']',cf_???},
	{PREF3|CTRL|'^',cf_???},
	{PREF3|CTRL|'_',cf_???},
	{PREF3|CTRL|'?',cf_???},
#endif
	{PREF3|' ',cf_showMarks},
	{PREF3|'?',cf_showKey},
	{PREF3|'A',cf_about},
	{PREF3|'B',cf_showBuffers},
	{PREF3|'F',cf_showFunctions},
	{PREF3|'H',cf_showHooks},
	{PREF3|'K',cf_showKillRing},
	{PREF3|'M',cf_alterShowMode},
	{PREF3|'N',cf_showBindings},
#if MMDEBUG & DEBUG_SHOWRE
	{PREF3|'R',cf_showRegexp},
#endif
	{PREF3|'S',cf_showScreens},
	{PREF3|'V',cf_showVariables},
	{PREF3|'{',cf_killFencedText},
	{PREF3|'}',cf_killFencedText},

	{META|CTRL|' ',cf_markBuf},
	{META|CTRL|'A',cf_beginText},
	{META|CTRL|'B',cf_backTab},
	{META|CTRL|'D',cf_deleteScreen},
	{META|CTRL|'E',cf_endWord},
	{META|CTRL|'F',cf_forwTab},
#if 0
	{META|CTRL|'G',cf_gotoMark},
#endif
	{META|CTRL|'H',cf_unhideBuf},
	{META|CTRL|'I',cf_inserti},
	{META|CTRL|'J',cf_joinLines},
	{META|CTRL|'L',cf_lcWord},
	{META|CTRL|'M',cf_wrapLine},
	{META|CTRL|'S',cf_newScreen},
	{META|CTRL|'T',cf_tcWord},
	{META|CTRL|'U',cf_ucWord},
	{META|CTRL|'V',cf_forwPageNext},
	{META|CTRL|'X',cf_eval},
	{META|CTRL|'Y',cf_clearKillRing},
	{META|CTRL|'Z',cf_backPageNext},
	{META|CTRL|'[',cf_resetTerm},
#if 0
	{META|CTRL|'\\',cf_???},
	{META|CTRL|']',cf_???},
	{META|CTRL|'^',cf_???},
	{META|CTRL|'_',cf_???},
	{META|CTRL|'?',cf_???},
#endif
	{META|' ',cf_gotoMark},
	{META|'(',cf_outdentRegion},
	{META|')',cf_indentRegion},
#if 0
	{META|'.',cf_gotoMark},
#endif
	{META|'/',cf_xeqFile},
	{META|'<',cf_beginBuf},
#if WORDCOUNT
	{META|'=',cf_countWords},
#endif
	{META|'>',cf_endBuf},
	{META|'?',cf_help},
	{META|'B',cf_backWord},
	{META|'D',cf_dupLine},
	{META|'F',cf_forwWord},
	{META|'G',cf_gotoLine},
	{META|'H',cf_hideBuf},
	{META|'I',cf_insertLineI},
	{META|'K',cf_bindKey},
	{META|'L',cf_redrawScreen},
	{META|'M',cf_alterGlobalMode},
	{META|'Q',cf_queryReplace},
	{META|'R',cf_replace},
	{META|'T',cf_truncBuf},
	{META|'U',cf_undelete},
	{META|'W',cf_setWrapCol},
	{META|'X',cf_run},
	{META|'Y',cf_yankPop},
	{META|'Z',cf_quickExit},
	{META|'[',cf_prevScreen},
	{META|'\\',cf_deleteWhite},
	{META|']',cf_nextScreen},
	{META|'{',cf_gotoFence},
	{META|'}',cf_gotoFence},
	{META|'~',cf_unchangeBuf},

	{FKEY|'<',cf_beginBuf},			// Home.
	{FKEY|'>',cf_endBuf},			// End.
	{FKEY|'B',cf_backChar},			// Left.
	{FKEY|'D',cf_deleteForwChar},		// Delete.
	{FKEY|'F',cf_forwChar},			// Right.
	{FKEY|'N',cf_forwLine},			// Down.
	{FKEY|'P',cf_backLine},			// Up.
	{FKEY|'V',cf_forwPage},			// PgDn.
	{FKEY|'Z',cf_backPage},			// PgUp.

#if CENTOS || DEBIAN || OSX
	{SHFT|FKEY|'B',cf_backWord},		// Shift left.
	{SHFT|FKEY|'D',cf_deleteWhite},		// Shift forward delete.
	{SHFT|FKEY|'F',cf_forwWord},		// Shift right.
#endif
	{0,-1}
};

// Key binding table.  Contains an array of KeyVect objects for each prefix key plus one more for all unprefixed bindings (the
// first entry).  Each KeyVect array contains 128 KeyDesc objects -- a slot for every possible 7-bit character.  Unused slots
// have a k_code of zero, which is not a valid extended key value.
KeyVect keytab[NPREFIX + 1];

#else

extern KeyVect keytab[];
#endif

// External function declarations for files that include this one.
extern CmdFunc *cfind(char *cname);
