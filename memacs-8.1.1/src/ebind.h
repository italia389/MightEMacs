// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// ebind.h	Default key-to-command bindings for MightEMacs.

// Command binding table.
//
// This table is *roughly* in ASCII order, left to right across the characters of the command.
//
// Printable ASCII chars: <SPC> ! " # $ % & ' ( ) * + , - . / 0-9 : ; < = > ? @ A-Z [ \ ] ^ _ ` a-z { | } ~.
//
//			Kill		Delete		Copy
//	Line		H-^L		^L		C-^L
//	Region		H-^W		^W		C-^W
//	ToBreak		H-^K		^K		C-^K
//	Word		H-^\		^\		C-^\		.

// Built-in key binding (which is copied to appropriate list at startup).
typedef struct {
	ushort ki_code;			// Key code.
	enum cfid ki_id;		// Command-function id.
	} KeyItem;

#ifdef DATAbind
static KeyItem keyitems[] = {
	{CTRL|' ',cf_setMark},
	{CTRL|'A',cf_beginLine},
	{CTRL|'B',cf_backChar},
	{CTRL|'C',cf_cPrefix},
	{CTRL|'D',cf_deleteForwChar},
	{CTRL|'E',cf_endLine},
	{CTRL|'F',cf_forwChar},
	{CTRL|'G',cf_abort},
	{CTRL|'H',cf_hPrefix},
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
	{CTRL|'X',cf_xPrefix},
	{CTRL|'Y',cf_yank},
	{CTRL|'Z',cf_backPage},
	{CTRL|'[',cf_metaPrefix},
	{CTRL|'\\',cf_deleteWord},
	{CTRL|']',cf_huntForw},
	{CTRL|'^',cf_huntBack},
	{CTRL|'_',cf_negativeArg},
	{CTRL|'?',cf_deleteBackChar},
	{SHFT|CTRL|'I',cf_deleteTab},

	{' ',cf_space},
#if 0
	{CPREF|CTRL|' ',cf_???},
#endif
	{CPREF|CTRL|'A',cf_deleteAlias},
	{CPREF|CTRL|'K',cf_copyToBreak},
	{CPREF|CTRL|'L',cf_copyLine},
	{CPREF|CTRL|'W',cf_copyRegion},
#if 0
	{CPREF|CTRL|'[',cf_???},
#endif
	{CPREF|CTRL|'\\',cf_copyWord},
#if 0
	{CPREF|CTRL|']',cf_???},
	{CPREF|CTRL|'^',cf_???},
	{CPREF|CTRL|'_',cf_???},
	{CPREF|CTRL|'?',cf_???},

	{CPREF|' ',cf_???},
#endif
	{CPREF|'.',cf_chDir},
	{CPREF|'A',cf_alias},
	{CPREF|'M',cf_alterDefMode},
	{CPREF|'{',cf_copyFencedText},
	{CPREF|'}',cf_copyFencedText},
#if 0
	{HPREF|CTRL|' ',cf_???},
#endif
	{HPREF|CTRL|'A',cf_beginWhite},
	{HPREF|CTRL|'E',cf_endWhite},
	{HPREF|CTRL|'K',cf_killToBreak},
	{HPREF|CTRL|'L',cf_killLine},
	{HPREF|CTRL|'V',cf_forwPagePrev},
	{HPREF|CTRL|'W',cf_killRegion},
	{HPREF|CTRL|'Z',cf_backPagePrev},
#if 0
	{HPREF|CTRL|'[',cf_???},
#endif
	{HPREF|CTRL|'\\',cf_killWord},
#if 0
	{HPREF|CTRL|']',cf_???},
	{HPREF|CTRL|'^',cf_???},
	{HPREF|CTRL|'_',cf_???},
	{HPREF|CTRL|'?',cf_???},

	{HPREF|' ',cf_???},
#endif
	{HPREF|'?',cf_showKey},
	{HPREF|'A',cf_about},
	{HPREF|'B',cf_showBuffers},
	{HPREF|'F',cf_showFunctions},
	{HPREF|'K',cf_showKillRing},
	{HPREF|'M',cf_alterShowMode},
	{HPREF|'N',cf_showBindings},
#if MMDEBUG & DEBUG_SHOWRE
	{HPREF|'R',cf_showRegExp},
#endif
	{HPREF|'S',cf_showScreens},
	{HPREF|'V',cf_showVariables},
	{HPREF|'{',cf_killFencedText},
	{HPREF|'}',cf_killFencedText},

	{XPREF|CTRL|' ',cf_clearMark},
	{XPREF|CTRL|'A',cf_appendFile},
	{XPREF|CTRL|'B',cf_selectBuf},
	{XPREF|CTRL|'C',cf_exit},
	{XPREF|CTRL|'D',cf_detabLine},
	{XPREF|CTRL|'E',cf_entabLine},
	{XPREF|CTRL|'F',cf_findFile},
	{XPREF|CTRL|'I',cf_insertFile},
	{XPREF|CTRL|'K',cf_unbindKey},
	{XPREF|CTRL|'L',cf_lcRegion},
	{XPREF|CTRL|'M',cf_deleteMacro},
	{XPREF|CTRL|'N',cf_moveWindDown},
	{XPREF|CTRL|'O',cf_deleteBlankLines},
	{XPREF|CTRL|'P',cf_moveWindUp},
	{XPREF|CTRL|'R',cf_readFile},
	{XPREF|CTRL|'S',cf_saveFile},
	{XPREF|CTRL|'T',cf_trimLine},
	{XPREF|CTRL|'U',cf_ucRegion},
	{XPREF|CTRL|'V',cf_viewFile},
	{XPREF|CTRL|'W',cf_writeFile},
	{XPREF|CTRL|'X',cf_swapMark},
	{XPREF|CTRL|'Z',cf_suspend},
#if 0
	{XPREF|CTRL|'[',cf_???},
	{XPREF|CTRL|'\\',cf_???},
	{XPREF|CTRL|']',cf_???},
	{XPREF|CTRL|'^',cf_???},
	{XPREF|CTRL|'_',cf_???},
#endif
	{XPREF|CTRL|'?',cf_clearBuf},

	{XPREF|' ',cf_insertSpace},
	{XPREF|'!',cf_shellCmd},
	{XPREF|'$',cf_shell},
	{XPREF|'(',cf_beginKeyMacro},
	{XPREF|')',cf_endKeyMacro},
	{XPREF|'+',cf_growWind},
	{XPREF|'-',cf_shrinkWind},
	{XPREF|'.',cf_whence},
	{XPREF|'0',cf_deleteWind},
	{XPREF|'1',cf_onlyWind},
	{XPREF|'2',cf_splitWind},
	{XPREF|'<',cf_narrowBuf},
	{XPREF|'=',cf_let},
	{XPREF|'>',cf_widenBuf},
	{XPREF|'?',cf_showKey},
	{XPREF|'@',cf_readPipe},

	{XPREF|'B',cf_setBufName},
	{XPREF|'E',cf_xeqKeyMacro},
	{XPREF|'F',cf_setBufFile},
	{XPREF|'I',cf_seti},
	{XPREF|'J',cf_joinWind},
	{XPREF|'K',cf_deleteBuf},
	{XPREF|'L',cf_lcLine},
	{XPREF|'M',cf_alterBufMode},
	{XPREF|'N',cf_nextWind},
	{XPREF|'P',cf_prevWind},
	{XPREF|'R',cf_insertBuf},
	{XPREF|'S',cf_scratchBuf},
	{XPREF|'U',cf_ucLine},
	{XPREF|'W',cf_resizeWind},
	{XPREF|'X',cf_xeqBuf},
	{XPREF|'Y',cf_cycleKillRing},
	{XPREF|'[',cf_prevBuf},
#if 0
	{XPREF|'\\',cf_???},
#endif
	{XPREF|']',cf_nextBuf},
#if 0
	{XPREF|'_',cf_???},
#endif
	{XPREF|'`',cf_insertPipe},
	{XPREF|'{',cf_deleteFencedText},
	{XPREF|'|',cf_pipeBuf},
	{XPREF|'}',cf_deleteFencedText},

	{META|CTRL|' ',cf_markBuf},
	{META|CTRL|'A',cf_beginText},
	{META|CTRL|'B',cf_backTab},
	{META|CTRL|'D',cf_deleteScreen},
	{META|CTRL|'E',cf_endWord},
	{META|CTRL|'F',cf_forwTab},
	{META|CTRL|'G',cf_gotoMark},
	{META|CTRL|'H',cf_unhideBuf},
	{META|CTRL|'I',cf_inserti},
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
	{META|' ',cf_setMark},
	{META|'(',cf_outdentRegion},
	{META|')',cf_indentRegion},
	{META|'.',cf_gotoMark},
	{META|'/',cf_xeqFile},
	{META|'<',cf_beginBuf},
#if WORDCOUNT
	{META|'=',cf_countWords},
#endif
	{META|'>',cf_endBuf},
	{META|'?',cf_help},
	{META|'B',cf_backWord},
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
	{META|'^',cf_joinLines},
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

#if REDHAT || OSX
	{SHFT|FKEY|'B',cf_backWord},		// Shift left.
	{SHFT|FKEY|'D',cf_deleteWhite},		// Shift forward delete.
	{SHFT|FKEY|'F',cf_forwWord},		// Shift right.
#endif
	{0,-1}
};

// Key binding table.  Contains a KeyHdr for each prefix key plus one more for all unprefixed bindings (the first entry).
KeyHdr keytab[NPREFIX + 1];

#else

extern KeyHdr keytab[];
#endif

// External function declarations for files that include this one.
extern CmdFunc *cfind(char *cname);
