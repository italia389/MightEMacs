// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// cmd.h	Command-related declarations and data.
//
// This file contains the command-function table, which lists all the command and function names, flags, and the C function (if
// not NULL) that is invoked when the command or function is executed.  The built-in key binding table (in bind.h) contains
// offsets into this table.
//
// Notes:
//  1. If the fifth value (cf_func) is not NULL, CFShrtLoad, CFNoLoad, CFIntN, and CFNISN are ignored.
//  2. If CFSpecArgs is set and cf_func is not NULL, the third and fourth values (cf_minArgs and cf_maxArgs) are not used.
//     However if cf_func is NULL and CFFunc is set, then cf_minArgs is used by execCF() to get the initial arguments.
//  3. If CFNCount is set and cf_func is not NULL, the specified C function is never executed when the n argument is zero.

#ifdef DATAcmd

// **** For main.c ****

// Global variables.
Alias *aheadp = NULL;			// Head of alias list.
CmdFunc cftab[] = {
	{"abort",		CFBind1 | CFUniq,0,		0,-1,	abortOp,	Literal1,	CFLit_abort},
	{"about",		0,0,				0,0,	aboutMM,	NULL,		CFLit_about},
	{"abs",			CFFunc,CFInt1,			1,1,	NULL,		Literal21,	CFLit_abs},
	{"alias",		CFSpecArgs | CFNoLoad,0,	2,2,	aliasCFM,	Literal2,	CFLit_alias},
	{"alterBufAttr",	CFShrtLoad,CFNotNull1,		2,-1,	alterBufAttr,	Literal46,	CFLit_alterBufAttr},
		// Returns former state (-1 or 1) or last attribute altered.
	{"alterBufMode",	CFShrtLoad,CFNotNull1,		2,-1,	NULL,		Literal48,	CFLit_alterBufMode},
		// Returns former state (-1 or 1) or last mode altered.
	{"alterGlobalMode",	CFNoLoad,0,			1,-1,	NULL,		Literal49,	CFLit_alterGlobalMode},
		// Returns former state (-1 or 1) or last mode altered.
	{"alterShowMode",	CFNoLoad,0,			1,-1,	NULL,		Literal49,	CFLit_alterShowMode},
		// Returns former state (-1 or 1) or last mode altered.
	{"appendFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	CFLit_appendFile},
		// Returns filename.
	{"apropos",		0,CFNil1,			1,1,	apropos,	Literal42,	CFLit_apropos},
	{"array",CFFunc,CFInt1 | CFBool2 | CFArray2 | CFNIS2,	0,2,	array,		Literal29,	CFLit_array},
		// Returns new array.
	{"backChar",		CFNCount,0,			0,0,	backChar,	NULL,		CFLit_backChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backLine",		CFNCount,0,			0,0,	backLine,	NULL,		CFLit_backLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backPage",		0,0,				0,0,	backPage,	NULL,		CFLit_backPage},
	{"backPageNext",	0,0,				0,0,	NULL,		NULL,		CFLit_backPageNext},
	{"backPagePrev",	0,0,				0,0,	NULL,		NULL,		CFLit_backPagePrev},
	{"backTab",		CFNCount,0,			0,0,	NULL,		NULL,		CFLit_backTab},
	{"backWord",		CFNCount,0,			0,0,	backWord,	NULL,		CFLit_backWord},
	{"backspace",		CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		CFLit_backspace},
	{"basename",		CFFunc,0,			1,1,	NULL,		Literal15,	CFLit_basename},
		// Returns filename component of pathname.
	{"beep",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_beep},
	{"beginBuf",		CFAddlArg | CFNoLoad,0,		0,1,	NULL,		Literal9,	CFLit_beginBuf},
	{"beginKeyMacro",	0,0,				0,0,	beginKeyMacro,	NULL,		CFLit_beginKeyMacro},
	{"beginLine",		0,0,				0,0,	NULL,		NULL,		CFLit_beginLine},
	{"beginText",		0,0,				0,0,	beginText,	NULL,		CFLit_beginText},
	{"beginWhite",		0,0,				0,0,	NULL,		NULL,		CFLit_beginWhite},
	{"bgets",		CFFunc | CFNCount | CFNoLoad,0,	1,1,	bgets,		Literal4,	CFLit_bgets},
		// Returns nth next line from buffer.
	{"bindKey",	CFSpecArgs | CFShrtLoad,CFNotNull1,	2,2,	bindKeyCM,	Literal7,	CFLit_bindKey},
	{"binding",		CFFunc,0,			1,1,	NULL,		Literal16,	CFLit_binding},
		// Returns name of command or macro key is bound to, or nil if none.
	{"bprint",CFFunc | CFShrtLoad,CFNotNull1,		2,-1,	bprint,		Literal5,	CFLit_bprint},
		// Returns text written.
	{"bprintf",		CFFunc,CFNotNull1,		2,-1,	NULL,		Literal53,	CFLit_bprintf},
		// Returns text written.
	{"bufAttr?",CFFunc | CFShrtLoad,CFNotNull1,		2,-1,	NULL,		Literal46,	CFLit_bufAttrQ},
		// Returns true if any/all flag(s) set in buffer; otherwise, false.
	{"bufBound?",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_bufBoundQ},
		// Returns true if dot is at beginning, middle, or end of buffer per n argument.
	{"bufMode?",CFFunc | CFShrtLoad,CFNotNull1,		2,-1,	NULL,		Literal45,	CFLit_bufModeQ},
		// Returns true if any/all mode(s) set; otherwise, false.
	{"bufSize",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_bufSize},
		// Returns buffer size in lines or bytes.
	{"bufWind",		CFFunc,0,			1,1,	NULL,		Literal4,	CFLit_bufWind},
		// Returns ordinal number of first window on current screen displaying given buffer, or nil if none.
	{"chDir",		0,0,				1,1,	chwkdir,	Literal38,	CFLit_chDir},
		// Returns absolute pathname of new directory.
	{"chr",			CFFunc,CFInt1,			1,1,	NULL,		Literal21,	CFLit_chr},
		// Returns ordinal value of a character in string form.
	{"clearBuf",		CFNoLoad,0,			0,1,	clearBuf,	Literal9,	CFLit_clearBuf},
		// Returns false if buffer is not cleared; otherwise, true.
	{"clearMsg",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_clearMsg},
	{"clone",		CFFunc,CFArray1,		1,1,	NULL,		Literal23,	CFLit_clone},
		// Returns new array.
	{"copyFencedRegion",	0,0,				0,0,	NULL,		NULL,		CFLit_copyFencedRegion},
	{"copyLine",		0,0,				0,0,	NULL,		NULL,		CFLit_copyLine},
	{"copyRegion",		0,0,				0,0,	NULL,		NULL,		CFLit_copyRegion},
	{"copyToBreak",		0,0,				0,0,	NULL,		NULL,		CFLit_copyToBreak},
	{"copyWord",		0,0,				0,0,	NULL,		NULL,		CFLit_copyWord},
#if WordCount
	{"countWords",		CFTerm,0,			0,0,	countWords,	NULL,		CFLit_countWords},
#endif
	{"cycleKillRing",	0,0,				0,0,	NULL,		NULL,		CFLit_cycleKillRing},
	{"cycleReplaceRing",	0,0,				0,0,	NULL,		NULL,		CFLit_cycleReplaceRing},
	{"cycleSearchRing",	0,0,				0,0,	NULL,		NULL,		CFLit_cycleSearchRing},
	{"defined?",		CFFunc,0,			1,1,	NULL,		Literal4,	CFLit_definedQ},
		// Returns kind of object, or nil if not found.
	{"deleteAlias",		CFSpecArgs | CFNoLoad,0,	1,-1,	deleteAlias,	Literal8,	CFLit_deleteAlias},
		// Returns zero if failure; otherwise, number of aliases deleted.
	{"deleteBackChar",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		CFLit_deleteBackChar},
	{"deleteBackTab",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		CFLit_deleteBackTab},
	{"deleteBlankLines",	CFEdit,0,			0,0,	deleteBlankLines,NULL,		CFLit_deleteBlankLines},
	{"deleteBuf",		0,0,				0,-1,	deleteBuf,	Literal37,	CFLit_deleteBuf},
		// Returns zero if failure; otherwise, number of buffers deleted.
	{"deleteFencedRegion",	CFEdit,0,			0,0,	NULL,		NULL,	CFLit_deleteFencedRegion},
	{"deleteForwChar",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		CFLit_deleteForwChar},
	{"deleteForwTab",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		CFLit_deleteForwTab},
	{"deleteKill",		0,0,				0,0,	NULL,		NULL,		CFLit_deleteKill},
	{"deleteLine",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_deleteLine},
	{"deleteMacro",		CFSpecArgs | CFNoLoad,0,	1,-1,	deleteMacro,	Literal8,	CFLit_deleteMacro},
		// Returns zero if failure; otherwise, number of macros deleted.
	{"deleteMark",		CFNoLoad | CFNoArgs,0,		0,1,	deleteMark,	Literal52,	CFLit_deleteMark},
	{"deleteRegion",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_deleteRegion},
	{"deleteReplacePat",	0,0,				0,0,	NULL,		NULL,		CFLit_deleteReplacePat},
	{"deleteScreen",	CFNoLoad,0,			1,1,	deleteScreen,	Literal21,	CFLit_deleteScreen},
	{"deleteSearchPat",	0,0,				0,0,	NULL,		NULL,		CFLit_deleteSearchPat},
	{"deleteToBreak",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_deleteToBreak},
	{"deleteWhite",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_deleteWhite},
	{"deleteWind",		0,0,				0,0,	deleteWind,	NULL,		CFLit_deleteWind},
	{"deleteWord",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_deleteWord},
	{"detabLine",		CFEdit,0,			0,0,	detabLine,	NULL,		CFLit_detabLine},
	{"dirname",		CFFunc,0,			1,1,	NULL,		Literal15,	CFLit_dirname},
		// Returns directory component of pathname.
	{"dupLine",		CFEdit,0,			0,0,	dupLine,	NULL,		CFLit_dupLine},
	{"empty?",	CFFunc,CFNil1 | CFArray1 | CFMay,	1,1,	NULL,		Literal13,	CFLit_emptyQ},
		// Returns true if nil, null string, or empty array.
	{"endBuf",		CFAddlArg | CFNoLoad,0,		0,1,	NULL,		Literal9,	CFLit_endBuf},
	{"endKeyMacro",		0,0,				0,0,	endKeyMacro,	NULL,		CFLit_endKeyMacro},
	{"endLine",		0,0,				0,0,	NULL,		NULL,		CFLit_endLine},
	{"endWhite",		0,0,				0,0,	NULL,		NULL,		CFLit_endWhite},
	{"endWord",		CFNCount,0,			0,0,	endWord,	NULL,		CFLit_endWord},
		// Returns false if hit buffer boundary; otherwise, true.
	{"entabLine",		CFEdit,0,			0,0,	entabLine,	NULL,		CFLit_entabLine},
	{"env",			CFFunc,0,			1,1,	NULL,		Literal4,	CFLit_env},
		// Returns value of environmental variable, or nil if not found.
	{"eval",		CFNoLoad,0,			1,-1,	eval,		Literal10,	CFLit_eval},
		// Returns result of evaluation.
	{"exit",		0,0,				0,-1,	quit,		Literal1,	CFLit_exit},
	{"failure",		CFFunc | CFNoLoad,CFNil1,	1,-1,	NULL,		Literal10,	CFLit_failure},
		// Returns false.
	{"filterBuf",		CFEdit | CFNoLoad,0,		1,-1,	filterBuf,	Literal10,	CFLit_filterBuf},
		// Returns false if failure.
	{"findFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	CFLit_findFile},
		// Returns [name of buffer, "true" or "false" indicating whether the buffer was created].
	{"forwChar",		CFNCount,0,			0,0,	forwChar,	NULL,		CFLit_forwChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwLine",		CFNCount,0,			0,0,	forwLine,	NULL,		CFLit_forwLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwPage",		0,0,				0,0,	forwPage,	NULL,		CFLit_forwPage},
	{"forwPageNext",	0,0,				0,0,	NULL,		NULL,		CFLit_forwPageNext},
	{"forwPagePrev",	0,0,				0,0,	NULL,		NULL,		CFLit_forwPagePrev},
	{"forwTab",		CFNCount,0,			0,0,	NULL,		NULL,		CFLit_forwTab},
	{"forwWord",		CFNCount,0,			0,0,	forwWord,	NULL,		CFLit_forwWord},
	{"getInfo",		CFFunc | CFNoLoad,CFNotNull1,	1,1,	NULL,		Literal44,	CFLit_getInfo},
		// Returns informational item per keyword argument.
	{"getKey",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_getKey},
		// Returns key in encoded form.
	{"globalMode?",		CFFunc | CFNoLoad,0,		1,-1,	NULL,		Literal3,	CFLit_globalModeQ},
		// Returns true if any/all mode(s) set; otherwise, false.
	{"gotoFence",		0,0,				0,0,	NULL,		NULL,		CFLit_gotoFence},
	{"gotoLine",		CFAddlArg | CFNoLoad,0,		1,2,	gotoLine,	Literal34,	CFLit_gotoLine},
	{"gotoMark",		CFNoLoad,0,			1,1,	gotoMark,	Literal35,	CFLit_gotoMark},
	{"growWind",		CFNCount,0,			0,0,	NULL,		NULL,		CFLit_growWind},
	{"help",		CFTerm,0,			0,0,	help,		NULL,		CFLit_help},
	{"huntBack",		CFNCount,0,			0,0,	huntBack,	NULL,		CFLit_huntBack},
		// Returns string found, or false if not found.
	{"huntForw",		CFNCount,0,			0,0,	huntForw,	NULL,		CFLit_huntForw},
		// Returns string found, or false if not found.
	{"include?",		CFFunc | CFShrtLoad,CFArray1,	2,-1,	doincl,		Literal47,	CFLit_includeQ},
		// Returns true if any/all expression values are in given array.
	{"indentRegion",	CFEdit | CFNCount,0,		0,0,	indentRegion,	NULL,		CFLit_indentRegion},
	{"index",		CFFunc,0,			2,2,	NULL,		Literal19,	CFLit_index},
		// Returns string position, or nil if not found.
	{"insert",		CFFunc | CFEdit | CFShrtLoad,0,	1,-1,	NULL,		Literal10,	CFLit_insert},
		// Returns text inserted.
	{"insertBuf",		CFEdit | CFNoLoad,0,		1,1,	insertBuf,	Literal4,	CFLit_insertBuf},
		// Returns name of buffer.
	{"insertFile",		CFEdit | CFNoLoad,CFNotNull1,	1,1,	insertFile,	Literal4,	CFLit_insertFile},
		// Returns filename.
	{"insertLineI",		CFEdit | CFNCount,0,		0,0,	insertLineI,	NULL,		CFLit_insertLineI},
	{"insertPipe",		CFEdit | CFNoLoad,0,		1,-1,	insertPipe,	Literal10,	CFLit_insertPipe},
		// Returns false if failure.
	{"insertSpace",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_insertSpace},
	{"inserti",		CFEdit | CFNCount,0,		0,0,	inserti,	NULL,		CFLit_inserti},
	{"interactive?",	CFFunc,0,			0,0,	NULL,		NULL,		CFLit_interactiveQ},
		// Returns true if script is being executed interactively.
	{"join",		CFFunc | CFShrtLoad,CFNil1,	2,-1,	NULL,		Literal20,	CFLit_join},
		// Returns string result.
	{"joinLines",		CFEdit,CFNil1,			1,1,	joinLines,	Literal30,	CFLit_joinLines},
	{"joinWind",		0,0,				0,0,	joinWind,	NULL,		CFLit_joinWind},
	{"keyPending?",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_keyPendingQ},
		// Returns true if type-ahead key(s) pending.
	{"kill",		CFFunc,CFInt1,			1,1,	NULL,		Literal21,	CFLit_kill},
	{"killFencedRegion",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_killFencedRegion},
	{"killLine",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_killLine},
	{"killRegion",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_killRegion},
	{"killToBreak",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_killToBreak},
	{"killWord",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_killWord},
	{"lastBuf",		0,0,				0,0,	NULL,		NULL,		CFLit_lastBuf},
		// Returns name of buffer.
	{"length",		CFFunc,CFArray1 | CFMay,	1,1,	NULL,		Literal13,	CFLit_length},
		// Returns string or array length.
	{"let",			CFTerm,0,			0,0,	setvar,		NULL,		CFLit_let},
	{"lowerCaseLine",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_lowerCaseLine},
	{"lowerCaseRegion",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_lowerCaseRegion},
	{"lowerCaseString",	CFFunc,0,			1,1,	NULL,		Literal12,	CFLit_lowerCaseString},
		// Returns string result.
	{"lowerCaseWord",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		CFLit_lowerCaseWord},
	{"markBuf",		CFNoLoad,0,			0,1,	markBuf,	Literal52,	CFLit_markBuf},
	{"match",		CFFunc,CFInt1,			1,1,	NULL,		Literal21,	CFLit_match},
		// Returns value of pattern match, or null if none.
	{"metaPrefix",CFHidden | CFPrefix | CFBind1 | CFPerm,0,	0,0,	NULL,		NULL,		CFLit_metaPrefix},
	{"moveWindDown",	CFNCount,0,			0,0,	NULL,		NULL,		CFLit_moveWindDown},
	{"moveWindUp",		CFNCount,0,			0,0,	moveWindUp,	NULL,		CFLit_moveWindUp},
	{"narrowBuf",		0,0,				0,0,	narrowBuf,	NULL,		CFLit_narrowBuf},
	{"negativeArg",		CFHidden | CFBind1 | CFUniq,0,	0,0,	NULL,		NULL,		CFLit_negativeArg},
	{"newline",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_newline},
	{"newlineI",		CFEdit | CFNCount,0,		0,0,	newlineI,	NULL,		CFLit_newlineI},
	{"nextBuf",		0,0,				0,0,	NULL,		NULL,		CFLit_nextBuf},
		// Returns name of buffer.
	{"nextScreen",		CFNCount,0,			0,0,	NULL,		NULL,		CFLit_nextScreen},
	{"nextWind",		0,0,				0,0,	nextWind,	NULL,		CFLit_nextWind},
	{"nil?",	CFFunc,CFBool1 | CFArray1 | CFNIS1,	1,1,	NULL,		Literal13,	CFLit_nilQ},
		// Returns true if expression is nil.
	{"null?",		CFFunc,0,			1,1,	NULL,		Literal12,	CFLit_nullQ},
		// Returns true if null string.
	{"numeric?",		CFFunc,0,			1,1,	NULL,		Literal12,	CFLit_numericQ},
		// Returns true if numeric literal.
	{"onlyWind",		0,0,				0,0,	onlyWind,	NULL,		CFLit_onlyWind},
	{"openLine",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_openLine},
	{"ord",			CFFunc,0,			1,1,	NULL,		Literal12,	CFLit_ord},
		// Returns ordinal value of first character of a string.
	{"outdentRegion",	CFEdit | CFNCount,0,		0,0,	outdentRegion,	NULL,		CFLit_outdentRegion},
	{"overwrite",		CFFunc | CFEdit | CFShrtLoad,0,	1,-1,	NULL,		Literal10,	CFLit_overwrite},
		// Returns new text.
	{"pathname",		CFFunc,0,			1,1,	NULL,		Literal15,	CFLit_pathname},
		// Returns absolute pathname of a file or directory.
	{"pause",		CFFunc,CFInt1,			1,1,	NULL,		Literal21,	CFLit_pause},
	{"pop",			CFFunc,CFArray1,		1,1,	NULL,		Literal23,	CFLit_pop},
		// Returns popped value, or nil if none left.
	{"popBuf",		CFNoLoad,0,			1,2,	NULL,		Literal43,	CFLit_popBuf},
		// Returns name of buffer.
	{"popFile",		CFNoLoad,0,			1,2,	NULL,		Literal43,	CFLit_popFile},
		// Returns name of buffer.
	{"prefix1",CFHidden | CFPrefix | CFBind1 | CFPerm,0,	0,0,	NULL,		NULL,		CFLit_prefix1},
	{"prefix2",CFHidden | CFPrefix | CFBind1 | CFPerm,0,	0,0,	NULL,		NULL,		CFLit_prefix2},
	{"prefix3",CFHidden | CFPrefix | CFBind1 | CFPerm,0,	0,0,	NULL,		NULL,		CFLit_prefix3},
	{"prevBuf",		0,0,				0,0,	NULL,		NULL,		CFLit_prevBuf},
		// Returns name of buffer.
	{"prevScreen",		CFNCount,0,			0,0,	NULL,		NULL,		CFLit_prevScreen},
	{"prevWind",		0,0,				0,0,	prevWind,	NULL,		CFLit_prevWind},
	{"print",		CFFunc | CFShrtLoad,0,		1,-1,	NULL,		Literal10,	CFLit_print},
	{"printf",		CFFunc,0,			1,-1,	NULL,		Literal32,	CFLit_printf},
	{"prompt",		CFFunc | CFNoLoad,0,		1,4,	uprompt,	Literal24,	CFLit_prompt},
		// Returns response read from keyboard.
	{"push",CFFunc,CFArray1 | CFBool2 | CFArray2 | CFNIS2,	2,2,	NULL,		Literal25,	CFLit_push},
		// Returns new array value.
	{"queryReplace",	CFEdit,CFNotNull1 | CFNil2,	2,2,	NULL,		Literal11,	CFLit_queryReplace},
		// Returns false if search stopped prematurely; otherwise, true.
	{"quickExit",		0,0,				0,0,	NULL,		NULL,		CFLit_quickExit},
	{"quote",	CFFunc,CFBool1 | CFArray1 | CFNIS1,	1,1,	NULL,		Literal13,	CFLit_quote},
		// Returns quoted expression.
	{"quoteChar",	CFBind1 | CFUniq | CFEdit | CFNCount,0,	0,0,	quoteChar,	NULL,		CFLit_quoteChar},
	{"rand",		CFFunc,CFInt1,			1,1,	NULL,		Literal21,	CFLit_rand},
		// Returns pseudo-random integer.
	{"readFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	CFLit_readFile},
		// Returns name of buffer.
	{"readPipe",		CFNoLoad,0,			1,-1,	readPipe,	Literal10,	CFLit_readPipe},
		// Returns name of buffer, or false if failure.
	{"reframeWind",		0,0,				0,0,	NULL,		NULL,		CFLit_reframeWind},
	{"renameBuf",		CFNoLoad,0,			1,2,	renameBuf,	Literal50,	CFLit_renameBuf},
		// Returns name of buffer.
	{"replace",		CFEdit,CFNotNull1 | CFNil2,	2,2,	NULL,		Literal11,	CFLit_replace},
	{"replaceText",		CFFunc | CFEdit | CFShrtLoad,0,	1,-1,	NULL,		Literal10,	CFLit_replaceText},
		// Returns new text.
	{"resetTerm",		0,0,				0,0,	resetTermc,	NULL,		CFLit_resetTerm},
	{"resizeWind",		0,0,				0,0,	resizeWind,	NULL,		CFLit_resizeWind},
	{"restoreBuf",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_restoreBuf},
		// Returns name of buffer.
	{"restoreScreen",	CFFunc,0,			0,0,	NULL,		NULL,		CFLit_restoreScreen},
	{"restoreWind",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_restoreWind},
	{"run",		CFPerm | CFSpecArgs | CFNoLoad,0,	1,1,	run,		Literal4,	CFLit_run},
		// Returns execution result.
	{"saveBuf",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_saveBuf},
		// Returns name of buffer.
	{"saveFile",		0,0,				0,0,	NULL,		NULL,		CFLit_saveFile},
	{"saveScreen",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_saveScreen},
	{"saveWind",		CFFunc,0,			0,0,	NULL,		NULL,		CFLit_saveWind},
	{"scratchBuf",		0,0,				0,0,	scratchBuf,	NULL,		CFLit_scratchBuf},
		// Returns name of buffer.
	{"searchBack",		0,CFNotNull1,			1,1,	searchBack,	Literal42,	CFLit_searchBack},
		// Returns string found, or false if not found.
	{"searchForw",		0,CFNotNull1,			1,1,	searchForw,	Literal42,	CFLit_searchForw},
		// Returns string found, or false if not found.
	{"selectBuf",		CFNoLoad,0,			1,1,	selectBuf,	Literal4,	CFLit_selectBuf},
		// Returns name of buffer.
	{"selectScreen",	CFNoLoad,0,			0,1,	selectScreen,	Literal22,	CFLit_selectScreen},
	{"selectWind",		CFNoLoad,0,			1,1,	selectWind,	Literal21,	CFLit_selectWind},
	{"setBufFile",		CFAddlArg,CFNil1 | CFNil2,	1,2,	setBufFile,	Literal51,	CFLit_setBufFile},
		// Returns two-element array containing new buffer name and new filename.
	{"setHook",	CFSpecArgs | CFShrtLoad,CFNotNull1,	2,2,	setHook,	Literal33,	CFLit_setHook},
	{"setMark",		CFNoLoad,0,			0,1,	setMark,	Literal52,	CFLit_setMark},
	{"setWrapCol",		CFNoLoad,0,			0,1,	NULL,		Literal22,	CFLit_setWrapCol},
	{"seti",		CFNoArgs,CFInt1 | CFInt2,	0,3,	seti,		Literal14,	CFLit_seti},
	{"shQuote",		CFFunc,CFNIS1,			1,1,	NULL,		Literal13,	CFLit_shQuote},
		// Returns quoted string.
	{"shell",		0,0,				0,0,	shellCLI,	NULL,		CFLit_shell},
		// Returns false if failure.
	{"shellCmd",		CFNoLoad,0,			1,-1,	shellCmd,	Literal10,	CFLit_shellCmd},
		// Returns false if failure.
	{"shift",		CFFunc,CFArray1,		1,1,	NULL,		Literal23,	CFLit_shift},
		// Returns shifted value, or nil if none left.
	{"showAliases",		CFAddlArg,0,			0,1,	showAliases,	Literal6,	CFLit_showAliases},
	{"showBuffers",		0,0,				0,0,	showBuffers,	NULL,		CFLit_showBuffers},
	{"showCommands",	CFAddlArg,0,			0,1,	showCommands,	Literal6,	CFLit_showCommands},
	{"showFunctions",	CFAddlArg,0,			0,1,	showFunctions,	Literal6,	CFLit_showFunctions},
	{"showHooks",		0,0,				0,0,	showHooks,	NULL,		CFLit_showHooks},
	{"showKey",		0,CFNotNull1,			1,1,	showKey,	Literal16,	CFLit_showKey},
	{"showKillRing",	0,0,				0,0,	showKillRing,	NULL,		CFLit_showKillRing},
	{"showMacros",		CFAddlArg,0,			0,1,	showMacros,	Literal6,	CFLit_showMacros},
	{"showMarks",		0,0,				0,0,	showMarks,	NULL,		CFLit_showMarks},
	{"showMode?",		CFFunc | CFNoLoad,0,		1,-1,	NULL,		Literal3,	CFLit_showModeQ},
		// Returns true if any/all mode(s) set; otherwise, false.
	{"showModes",		0,0,				0,0,	showModes,	NULL,		CFLit_showModes},
	{"showPoint",		CFTerm,0,			0,0,	showPoint,	NULL,		CFLit_showPoint},
#if MMDebug & Debug_ShowRE
	{"showRegexp",		0,0,				0,0,	showRegexp,	NULL,		CFLit_showRegexp},
#endif
	{"showReplaceRing",	0,0,				0,0,	showReplaceRing,NULL,		CFLit_showReplaceRing},
	{"showScreens",		0,0,				0,0,	showScreens,	NULL,		CFLit_showScreens},
	{"showSearchRing",	0,0,				0,0,	showSearchRing,	NULL,		CFLit_showSearchRing},
	{"showVariables",	CFAddlArg,0,			0,1,	showVariables,	Literal6,	CFLit_showVariables},
	{"shrinkWind",		CFNCount,0,			0,0,	NULL,		NULL,		CFLit_shrinkWind},
	{"space",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_space},
	{"split",		CFFunc,CFNil1 | CFInt3,		2,3,	ssplit,		Literal39,	CFLit_split},
		// Returns array.
	{"splitWind",		0,0,				0,0,	NULL,		NULL,		CFLit_splitWind},
		// Returns ordinal number of new window not containing point.
	{"sprintf",		CFFunc,0,			1,-1,	NULL,		Literal32,	CFLit_sprintf},
	{"stat?",		CFFunc,0,			2,2,	NULL,		Literal36,	CFLit_statQ},
		// Returns Boolean result.
	{"strFit",		CFFunc,CFInt2,			2,2,	NULL,		Literal28,	CFLit_strFit},
		// Returns compressed string.
	{"strPop",	CFFunc | CFSpecArgs | CFNoLoad,0,	2,2,	NULL,		Literal40,	CFLit_strPop},
		// Returns popped value, or nil if none left.
	{"strPush",	CFFunc | CFSpecArgs | CFNoLoad,0,	3,3,	NULL,		Literal41,	CFLit_strPush},
		// Returns pushed value.
	{"strShift",	CFFunc | CFSpecArgs | CFNoLoad,0,	2,2,	NULL,		Literal40,	CFLit_strShift},
		// Returns shifted value, or nil if none left.
	{"strUnshift",	CFFunc | CFSpecArgs | CFNoLoad,0,	3,3,	NULL,		Literal41,	CFLit_strUnshift},
		// Returns unshifted value.
	{"strip",		CFFunc,0,			1,1,	NULL,		Literal12,	CFLit_strip},
		// Returns whitespace-stripped string.
	{"sub",			CFFunc,CFNil3,			3,3,	NULL,		Literal18,	CFLit_sub},
		// Returns string result.
	{"subline",		CFFunc,CFInt1 | CFInt2,		1,2,	NULL,		Literal26,	CFLit_subline},
		// Returns string result.
	{"substr",		CFFunc,CFInt2 | CFInt3,		2,3,	NULL,		Literal27,	CFLit_substr},
		// Returns string result.
	{"success",		CFFunc | CFNoLoad,CFNil1,	1,-1,	NULL,		Literal10,	CFLit_success},
		// Returns true.
	{"suspend",		0,0,				0,0,	suspendMM,	NULL,		CFLit_suspend},
	{"swapMark",		CFNoLoad,0,			0,1,	swapMark,	Literal52,	CFLit_swapMark},
	{"tab",			CFEdit,0,			0,0,	NULL,		NULL,		CFLit_tab},
	{"titleCaseLine",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_titleCaseLine},
	{"titleCaseRegion",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_titleCaseRegion},
	{"titleCaseString",	CFFunc,0,			1,1,	NULL,		Literal12,	CFLit_titleCaseString},
		// Returns string result.
	{"titleCaseWord",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		CFLit_titleCaseWord},
	{"toInt",		CFFunc,CFInt1 | CFMay,		1,1,	NULL,		Literal12,	CFLit_toInt},
		// Returns integer result.
	{"toString",	CFFunc,CFBool1 | CFArray1 | CFNIS1,	1,1,	NULL,		Literal13,	CFLit_toString},
		// Returns string result.
	{"tr",			CFFunc,CFNil3,			3,3,	NULL,		Literal18,	CFLit_tr},
		// Returns translated string.
	{"traverseLine",	0,0,				0,0,	traverseLine,	NULL,		CFLit_traverseLine},
	{"trimLine",		CFEdit,0,			0,0,	trimLine,	NULL,		CFLit_trimLine},
	{"truncBuf",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_truncBuf},
		// Returns name of buffer.
	{"type?",	CFFunc,CFBool1 | CFArray1 | CFNIS1,	1,1,	NULL,		Literal13,	CFLit_typeQ},
		// Returns type of value.
	{"unbindKey",		0,CFNotNull1,			1,1,	unbindKey,	Literal16,	CFLit_unbindKey},
		// Returns Boolean result if n > 0.
	{"undelete",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_undelete},
	{"universalArg",	CFHidden | CFBind1 | CFUniq,0,	0,0,	NULL,		NULL,		CFLit_universalArg},
	{"unshift",CFFunc,CFArray1 | CFBool2 | CFArray2 | CFNIS2,2,2,	NULL,		Literal25,	CFLit_unshift},
		// Returns new array value.
	{"updateScreen",	CFFunc,0,			0,0,	NULL,		NULL,		CFLit_updateScreen},
	{"upperCaseLine",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_upperCaseLine},
	{"upperCaseRegion",	CFEdit,0,			0,0,	NULL,		NULL,		CFLit_upperCaseRegion},
	{"upperCaseString",	CFFunc,0,			1,1,	NULL,		Literal12,	CFLit_upperCaseString},
		// Returns string result.
	{"upperCaseWord",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		CFLit_upperCaseWord},
	{"viewFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	CFLit_viewFile},
		// Returns [name of buffer, "true" or "false" indicating whether the buffer was created].
	{"widenBuf",		0,0,				0,0,	widenBuf,	NULL,		CFLit_widenBuf},
	{"wordChar?",		CFFunc,0,			1,1,	NULL,		Literal12,	CFLit_wordCharQ},
		// Returns true if first character of a string is in $wordChars.
	{"wrapLine",		CFEdit,CFNil1 | CFNil2,		2,2,	wrapLine,	Literal31,	CFLit_wrapLine},
	{"wrapWord",		CFFunc | CFEdit,0,		0,0,	wrapWord,	NULL,		CFLit_wrapWord},
	{"writeFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	CFLit_writeFile},
		// Returns filename.
	{"xPathname",		CFFunc,0, 			1,1,	NULL,		Literal15,	CFLit_xPathname},
		// Returns absolute pathname, or nil if not found.
	{"xeqBuf",		CFNoLoad,0,			1,-1,	xeqBuf,		Literal17,	CFLit_xeqBuf},
		// Returns execution result.
	{"xeqFile",		CFNoLoad,0,			1,-1,	xeqFile,	Literal17,	CFLit_xeqFile},
		// Returns execution result.
	{"xeqKeyMacro",		0,0,				0,0,	xeqKeyMacro,	NULL,		CFLit_xeqKeyMacro},
	{"yank",		CFEdit,0,			0,0,	NULL,		NULL,		CFLit_yank},
	{"yankCycle",		CFEdit,0,			0,0,	yankCycle,	NULL,		CFLit_yankCycle},
	{NULL,			0,0,				0,0,	NULL,		NULL,		NULL}
	};
CFAMRec *frheadp = NULL;		// Head of CFAM list.

#define NFuncs	(sizeof(cftab) / sizeof(CmdFunc)) - 1

#else

// **** For all the other .c files ****

// External variable declarations.
extern Alias *aheadp;
extern CmdFunc cftab[];
extern CFAMRec *frheadp;
#endif
