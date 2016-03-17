// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// ecmd.h	Command names and associated C function names.
//
// This file lists all the C functions used by MightEMacs and the command names that are used to bind keys to them.

// Name to function binding table
//
// This table contains all the commands (names, flags, and C function addresses).  These are used for binding to keys and
// macro execution.
//
// Non-bindable functions (CFFUNC) that use a numeric argument:
// n arg	Func		Type		Action
// --------	-----------	------		------------------------
// n <= 0	basename	monoplex	Exclude extension
// n > 0	getKey		nilplex		Get key sequence
// n <= 0	sub		triplex		First occurrence only
// n > 0	index		duplex		Rightmost
// < 0		strip		monoplex	Left end only
// defn or 0	strip				Both ends
// > 0		strip				Right end only
//
// Notes:
//  1. If the fifth value (cf_func) is not NULL, CFSHRTLOAD, CFNOLOAD, CFNUM1, CFNUM2, CFNUM3, and CFANY are ignored.
//  2. If CFSPECARGS is set and cf_func is not NULL, the third and fourth values (cf_minArgs and cf_maxArgs) are not used.
//     However if cf_func is NULL and CFFUNC is set, then cf_minArgs is used by feval() to get the initial arguments.
//  3. If CFNCOUNT is set and cf_func is not NULL, the specified C function is never executed when the n argument is zero.

#ifdef DATAfunc
CmdFunc cftab[] = {
	{"abort",		CFBIND1 | CFUNIQ,	0,	-1,	abortOp,	LITERAL1,	FLIT_abort},
	{"about",		0,			0,	0,	aboutMM,	"",		FLIT_about},
	{"abs",			CFFUNC | CFNUM1,	1,	1,	NULL,		LITERAL21,	FLIT_abs},
	{"alias",		CFSPECARGS,		2,	2,	aliasCFM,	LITERAL2,	FLIT_alias},
	{"alterBufMode",	CFSHRTLOAD,		1,	-1,	NULL,		LITERAL3,	FLIT_alterBufMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterDefMode",	CFSHRTLOAD,		1,	-1,	NULL,		LITERAL3,	FLIT_alterDefMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterGlobalMode",	CFSHRTLOAD,		1,	-1,	NULL,		LITERAL3,	FLIT_alterGlobalMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterShowMode",	CFSHRTLOAD,		1,	-1,	NULL,		LITERAL3,	FLIT_alterShowMode},
		// Returns state (-1 or 1) or last mode altered.
	{"appendFile",		0,			1,	1,	NULL,		LITERAL4,	FLIT_appendFile},
		// Returns filename.
	{"backChar",		CFNCOUNT,		0,	0,	backChar,	"",		FLIT_backChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backLine",		CFNCOUNT,		0,	0,	backLine,	"",		FLIT_backLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backPage",		CFNCOUNT,		0,	0,	backPage,	"",		FLIT_backPage},
	{"backPageNext",	CFNCOUNT,		0,	0,	NULL,		"",		FLIT_backPageNext},
	{"backPagePrev",	CFNCOUNT,		0,	0,	NULL,		"",		FLIT_backPagePrev},
	{"backTab",		CFNCOUNT,		0,	0,	NULL,		"",		FLIT_backTab},
	{"backWord",		CFNCOUNT,		0,	0,	backWord,	"",		FLIT_backWord},
	{"basename",		CFFUNC,			1,	1,	NULL,		LITERAL15,	FLIT_basename},
		// Returns filename component of pathname.
	{"beep",		0,			0,	0,	beeper,		"",		FLIT_beep},
	{"beginBuf",		CFADDLARG,		0,	1,	NULL,		LITERAL9,	FLIT_beginBuf},
	{"beginKeyMacro",	0,			0,	0,	beginKeyMacro,	"",		FLIT_beginKeyMacro},
	{"beginLine",		0,			0,	0,	NULL,		"",		FLIT_beginLine},
	{"beginText",		0,			0,	0,	beginText,	"",		FLIT_beginText},
	{"beginWhite",		0,			0,	0,	NULL,		"",		FLIT_beginWhite},
	{"bindKey",		CFSPECARGS,		2,	2,	bindKeyCM,	LITERAL7,	FLIT_bindKey},
	{"binding",		CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_binding},
		// Returns name of command or macro key is bound to, or nil if none.
	{"bufBound?",		CFFUNC,			0,	0,	NULL,		"",		FLIT_bufBoundQ},
		// Returns true if dot is at beginning, middle, or end of buffer per n argument.
	{"chDir",		0,			1,	1,	changedir,	"",		FLIT_chDir},
		// Returns absolute pathname of new directory.
	{"chr",			CFFUNC | CFNUM1,	1,	1,	NULL,		LITERAL21,	FLIT_chr},
		// Returns ordinal value of a character in string form.
	{"clearBuf",		0,			0,	1,	clearBuf,	LITERAL9,	FLIT_clearBuf},
		// Returns false if buffer is not cleared; otherwise, true.
	{"clearKillRing",	0,			0,	0,	NULL,		"",		FLIT_clearKillRing},
	{"clearMsg",		CFFUNC,			0,	0,	NULL,		"",		FLIT_clearMsg},
	{"copyFencedText",	0,			0,	0,	NULL,		"",		FLIT_copyFencedText},
	{"copyLine",		0,			0,	0,	NULL,		"",		FLIT_copyLine},
	{"copyRegion",		0,			0,	0,	NULL,		"",		FLIT_copyRegion},
	{"copyToBreak",		0,			0,	0,	NULL,		"",		FLIT_copyToBreak},
	{"copyWord",		0,			0,	0,	NULL,		"",		FLIT_copyWord},
#if WORDCOUNT
	{"countWords",		CFTERM,			0,	0,	countWords,	"",		FLIT_countWords},
#endif
	{"cycleKillRing",	0,			0,	0,	NULL,		"",		FLIT_cycleKillRing},
	{"defined?",		CFFUNC,			1,	1,	NULL,		LITERAL4,	FLIT_definedQ},
		// Returns type of object, or nil if not found.
	{"deleteAlias",		CFSPECARGS,		1,	-1,	deleteAlias,	LITERAL8,	FLIT_deleteAlias},
	{"deleteBackChar",	CFEDIT | CFNCOUNT,	0,	0,	NULL,		"",		FLIT_deleteBackChar},
	{"deleteBlankLines",	CFEDIT,			0,	0,	deleteBlankLines,"",		FLIT_deleteBlankLines},
	{"deleteBuf",		0,			1,	-1,	deleteBuf,	LITERAL8,	FLIT_deleteBuf},
		// Returns name of last buffer deleted.
	{"deleteFencedText",	CFEDIT,			0,	0,	NULL,		"",		FLIT_deleteFencedText},
	{"deleteForwChar",	CFEDIT | CFNCOUNT,	0,	0,	NULL,		"",		FLIT_deleteForwChar},
	{"deleteLine",		CFEDIT,			0,	0,	NULL,		"",		FLIT_deleteLine},
	{"deleteMacro",		CFSPECARGS,		1,	-1,	deleteMacro,	LITERAL8,	FLIT_deleteMacro},
	{"deleteMark",		CFNOARGS,		1,	1,	deleteMark,	LITERAL22,	FLIT_deleteMark},
	{"deleteRegion",	CFEDIT,			0,	0,	NULL,		"",		FLIT_deleteRegion},
	{"deleteScreen",	0,			0,	0,	deleteScreen,	"",		FLIT_deleteScreen},
	{"deleteTab",		CFEDIT | CFNCOUNT,	0,	0,	deleteTab,	"",		FLIT_deleteTab},
	{"deleteToBreak",	CFEDIT,			0,	0,	NULL,		"",		FLIT_deleteToBreak},
	{"deleteWhite",		CFEDIT,			0,	0,	NULL,		"",		FLIT_deleteWhite},
	{"deleteWind",		0,			0,	0,	deleteWind,	"",		FLIT_deleteWind},
	{"deleteWord",		CFEDIT,			0,	0,	NULL,		"",		FLIT_deleteWord},
	{"detabLine",		CFEDIT,			0,	0,	detabLine,	"",		FLIT_detabLine},
	{"dirname",		CFFUNC,			1,	1,	NULL,		LITERAL15,	FLIT_dirname},
		// Returns directory component of pathname.
	{"dupLine",		CFEDIT,			0,	0,	dupLine,	"",		FLIT_dupLine},
	{"endBuf",		CFADDLARG,		0,	1,	NULL,		LITERAL9,	FLIT_endBuf},
	{"endKeyMacro",		0,			0,	0,	endKeyMacro,	"",		FLIT_endKeyMacro},
	{"endLine",		0,			0,	0,	NULL,		"",		FLIT_endLine},
	{"endWhite",		0,			0,	0,	NULL,		"",		FLIT_endWhite},
	{"endWord",		CFNCOUNT,		0,	0,	endWord,	"",		FLIT_endWord},
	{"entabLine",		CFEDIT,			0,	0,	entabLine,	"",		FLIT_entabLine},
	{"env",			CFFUNC,			1,	1,	NULL,		LITERAL4,	FLIT_env},
		// Returns value of environmental variable, or nil if not found.
	{"eval",		0,			1,	-1,	eval,		LITERAL10,	FLIT_eval},
		// Returns result of evaluation.
	{"exit",		0,			0,	-1,	quit,		LITERAL1,	FLIT_exit},
	{"fileExists?",		CFFUNC,			1,	1,	NULL,		LITERAL15,	FLIT_fileExistsQ},
		// Returns type of file, or nil if not found.
	{"findFile",		0,			1,	1,	NULL,		LITERAL4,	FLIT_findFile},
		// Returns name of buffer, a tab, and "true" if buffer created.
	{"forwChar",		CFNCOUNT,		0,	0,	forwChar,	"",		FLIT_forwChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwLine",		CFNCOUNT,		0,	0,	forwLine,	"",		FLIT_forwLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwPage",		CFNCOUNT,		0,	0,	forwPage,	"",		FLIT_forwPage},
	{"forwPageNext",	CFNCOUNT,		0,	0,	NULL,		"",		FLIT_forwPageNext},
	{"forwPagePrev",	CFNCOUNT,		0,	0,	NULL,		"",		FLIT_forwPagePrev},
	{"forwTab",		CFNCOUNT,		0,	0,	NULL,		"",		FLIT_forwTab},
	{"forwWord",		CFNCOUNT,		0,	0,	forwWord,	"",		FLIT_forwWord},
	{"getKey",		CFFUNC,			0,	0,	NULL,		"",		FLIT_getKey},
		// Returns key in encoded form.
	{"gotoFence",		0,			0,	0,	NULL,		"",		FLIT_gotoFence},
	{"gotoLine",		CFADDLARG,		1,	2,	gotoLine,	LITERAL34,	FLIT_gotoLine},
	{"gotoMark",		0,			1,	1,	gotoMark,	LITERAL35,	FLIT_gotoMark},
	{"growWind",		CFNCOUNT,		0,	0,	NULL,		"",		FLIT_growWind},
	{"help",		CFTERM,			0,	0,	help,		"",		FLIT_help},
	{"hideBuf",		CFADDLARG,		0,	1,	NULL,		LITERAL9,	FLIT_hideBuf},
		// Returns name of buffer.
	{"huntBack",		CFNCOUNT,		0,	0,	huntBack,	"",		FLIT_huntBack},
		// Returns string found, or false if not found.
	{"huntForw",		CFNCOUNT,		0,	0,	huntForw,	"",		FLIT_huntForw},
		// Returns string found, or false if not found.
	{"include?",	CFFUNC | CFNIL2 | CFNIL3 | CFBOOL3,3,	3,	NULL,		LITERAL29,	FLIT_includeQ},
		// Returns true if value is in given list.
	{"indentRegion",	CFEDIT | CFNCOUNT,	0,	0,	indentRegion,	"",		FLIT_indentRegion},
	{"index",		CFFUNC,			2,	2,	NULL,		LITERAL19,	FLIT_index},
		// Returns string position, or nil if not found.
	{"insert",	CFFUNC | CFEDIT | CFSHRTLOAD,	1,	-1,	NULL,		LITERAL10,	FLIT_insert},
		// Returns text inserted.
	{"insertBuf",		CFEDIT,			1,	1,	insertBuf,	LITERAL4,	FLIT_insertBuf},
		// Returns name of buffer.
	{"insertFile",		CFEDIT,			1,	1,	insertFile,	LITERAL4,	FLIT_insertFile},
		// Returns filename.
	{"insertLineI",		CFEDIT | CFNCOUNT,	0,	0,	insertLineI,	"",		FLIT_insertLineI},
	{"insertPipe",		CFEDIT,			1,	-1,	insertPipe,	LITERAL10,	FLIT_insertPipe},
		// Returns false if failure.
	{"insertSpace",		CFEDIT | CFNCOUNT,	0,	0,	NULL,		"",		FLIT_insertSpace},
	{"inserti",		CFEDIT | CFNCOUNT,	0,	0,	inserti,	"",		FLIT_inserti},
	{"int?",	CFFUNC | CFANY | CFNIL1 | CFBOOL1,1,	1,	NULL,		LITERAL13,	FLIT_intQ},
		// Returns true if integer value.
	{"join",	CFFUNC | CFSHRTLOAD | CFNIL1,	2,	-1,	NULL,		LITERAL20,	FLIT_join},
		// Returns string result.
	{"joinLines",		CFEDIT,			1,	1,	joinLines,	LITERAL30,	FLIT_joinLines},
	{"joinWind",		0,			0,	0,	joinWind,	"",		FLIT_joinWind},
	{"killFencedText",	CFEDIT,			0,	0,	NULL,		"",		FLIT_killFencedText},
	{"killLine",		CFEDIT,			0,	0,	NULL,		"",		FLIT_killLine},
	{"killRegion",		CFEDIT,			0,	0,	NULL,		"",		FLIT_killRegion},
	{"killToBreak",		CFEDIT,			0,	0,	NULL,		"",		FLIT_killToBreak},
	{"killWord",		CFEDIT,			0,	0,	NULL,		"",		FLIT_killWord},
	{"lcLine",		CFEDIT,			0,	0,	NULL,		"",		FLIT_lcLine},
	{"lcRegion",		CFEDIT,			0,	0,	NULL,		"",		FLIT_lcRegion},
	{"lcString",		CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_lcString},
		// Returns string result.
	{"lcWord",		CFEDIT | CFNCOUNT,	0,	0,	lcWord,		"",		FLIT_lcWord},
	{"length",		CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_length},
		// Returns string length.
	{"let",			CFTERM,			0,	0,	setvar,		"",		FLIT_let},
	{"markBuf",		0,			0,	1,	markBuf,	LITERAL22,	FLIT_markBuf},
	{"match",		CFFUNC | CFNUM1,	1,	1,	NULL,		LITERAL21,	FLIT_match},
		// Returns value of pattern match, or null if none.
	{"metaPrefix",	CFHIDDEN | CFPREFIX | CFBIND1 | CFPERM,0,0,	NULL,		"",		FLIT_metaPrefix},
	{"moveWindDown",	CFNCOUNT,		0,	0,	NULL,		"",		FLIT_moveWindDown},
	{"moveWindUp",		CFNCOUNT,		0,	0,	moveWindUp,	"",		FLIT_moveWindUp},
	{"narrowBuf",		0,			0,	0,	narrowBuf,	"",		FLIT_narrowBuf},
	{"negativeArg",		CFHIDDEN | CFBIND1 | CFUNIQ,0,	0,	NULL,		"",		FLIT_negativeArg},
	{"newScreen",		0,			0,	0,	newScreen,	"",		FLIT_newScreen},
		// Returns new screen number.
	{"newline",		CFEDIT | CFNCOUNT,	0,	0,	NULL,		"",		FLIT_newline},
	{"newlineI",		CFEDIT | CFNCOUNT,	0,	0,	newlineI,	"",		FLIT_newlineI},
	{"nextArg",		CFFUNC,			0,	0,	NULL,		"",		FLIT_nextArg},
		// Returns value of next macro argument.
	{"nextBuf",		0,			0,	0,	NULL,		"",		FLIT_nextBuf},
		// Returns name of buffer.
	{"nextScreen",		0,			0,	0,	nextScreen,	"",		FLIT_nextScreen},
		// Returns new screen number.
	{"nextWind",		0,			0,	0,	nextWind,	"",		FLIT_nextWind},
	{"nil?",	CFFUNC | CFANY | CFNIL1 | CFBOOL1,1,	1,	NULL,		LITERAL13,	FLIT_nilQ},
		// Returns true if nil value.
	{"notice",		CFFUNC | CFNIL1,	1,	-1,	notice,		LITERAL10,	FLIT_notice},
		// Returns true if default n or n >= 0; otherwise, false.
	{"null?",	CFFUNC | CFANY | CFNIL1 | CFBOOL1,1,	1,	NULL,		LITERAL13,	FLIT_nullQ},
		// Returns true if null string.
	{"numeric?",	CFFUNC | CFNIL1 | CFBOOL1,	1,	1,	NULL,		LITERAL12,	FLIT_numericQ},
		// Returns true if numeric literal.
	{"onlyWind",		0,			0,	0,	onlyWind,	"",		FLIT_onlyWind},
	{"openLine",		CFEDIT | CFNCOUNT,	0,	0,	openLine,	"",		FLIT_openLine},
	{"ord",			CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_ord},
		// Returns ordinal value of first character of a string.
	{"outdentRegion",	CFEDIT | CFNCOUNT,	0,	0,	outdentRegion,	"",		FLIT_outdentRegion},
	{"overwrite",	CFFUNC | CFEDIT | CFSHRTLOAD,	1,	-1,	NULL,		LITERAL10,	FLIT_overwrite},
		// Returns new text.
	{"pathname",		CFFUNC,			1,	1,	NULL,		LITERAL15,	FLIT_pathname},
		// Returns absolute pathname of a file or directory.
	{"pause",		CFFUNC,			0,	0,	NULL,		"",		FLIT_pause},
	{"pipeBuf",		CFEDIT,			1,	-1,	pipeBuf,	LITERAL10,	FLIT_pipeBuf},
		// Returns false if failure.
	{"pop",		CFFUNC | CFSPECARGS | CFNOLOAD,	2,	2,	NULL,		LITERAL23,	FLIT_pop},
		// Returns popped value, or nil if none left.
	{"prefix1",	CFHIDDEN | CFPREFIX | CFBIND1 | CFPERM,0,0,	NULL,		"",		FLIT_prefix1},
	{"prefix2",	CFHIDDEN | CFPREFIX | CFBIND1 | CFPERM,0,0,	NULL,		"",		FLIT_prefix2},
	{"prefix3",	CFHIDDEN | CFPREFIX | CFBIND1 | CFPERM,0,0,	NULL,		"",		FLIT_prefix3},
	{"prevBuf",		0,			0,	0,	NULL,		"",		FLIT_prevBuf},
		// Returns name of buffer.
	{"prevScreen",		0,			0,	0,	NULL,		"",		FLIT_prevScreen},
		// Returns new screen number.
	{"prevWind",		0,			0,	0,	prevWind,	"",		FLIT_prevWind},
	{"print",		CFFUNC | CFSHRTLOAD,	1,	-1,	NULL,		LITERAL10,	FLIT_print},
	{"prompt",		CFFUNC | CFSPECARGS,	1,	4,	NULL,		LITERAL24,	FLIT_prompt},
		// Returns response read from keyboard.
	{"push",	CFFUNC | CFSPECARGS | CFNOLOAD,	3,	3,	NULL,		LITERAL25,	FLIT_push},
		// Returns pushed value.
	{"queryReplace",	CFEDIT,			2,	2,	NULL,		LITERAL11,	FLIT_queryReplace},
		// Returns false if search stopped prematurely; otherwise, true.
	{"quickExit",		0,			0,	0,	NULL,		"",		FLIT_quickExit},
	{"quote",	CFFUNC | CFANY | CFNIL1 | CFBOOL1,1,	1,	NULL,		LITERAL13,	FLIT_quote},
		// Returns quoted string.
	{"quoteChar",	CFBIND1 | CFUNIQ | CFEDIT | CFNCOUNT,0,	0,	quoteChar,	"",		FLIT_quoteChar},
	{"rand",		CFFUNC,			0,	0,	NULL,		"",		FLIT_rand},
		// Returns random integer.
	{"readBuf",		CFFUNC | CFNCOUNT,	1,	1,	readBuf,	LITERAL4,	FLIT_readBuf},
		// Returns nth next line from buffer.
	{"readFile",		0,			1,	1,	NULL,		LITERAL4,	FLIT_readFile},
		// Returns name of buffer.
	{"readPipe",		0,			1,	-1,	readPipe,	LITERAL10,	FLIT_readPipe},
		// Returns name of buffer, or false if failure.
	{"redrawScreen",	0,			0,	0,	NULL,		"",		FLIT_redrawScreen},
	{"replace",		CFEDIT | CFNCOUNT,	2,	2,	NULL,		LITERAL11,	FLIT_replace},
	{"replaceText",	CFFUNC | CFEDIT | CFSHRTLOAD,	1,	-1,	NULL,		LITERAL10,	FLIT_replaceText},
		// Returns new text.
	{"resetTerm",		0,			0,	0,	resetTermc,	"",		FLIT_resetTerm},
	{"resizeWind",		0,			0,	0,	resizeWind,	"",		FLIT_resizeWind},
	{"restoreBuf",		CFFUNC,			0,	0,	NULL,		"",		FLIT_restoreBuf},
		// Returns name of buffer.
	{"restoreWind",		CFFUNC,			0,	0,	NULL,		"",		FLIT_restoreWind},
	{"reverse",		CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_reverse},
		// Returns reversed string.
	{"run",			CFPERM | CFSPECARGS,	1,	1,	run,		LITERAL4,	FLIT_run},
		// Returns execution result.
	{"saveBuf",		CFFUNC,			0,	0,	NULL,		"",		FLIT_saveBuf},
		// Returns name of buffer.
	{"saveFile",		0,			0,	0,	NULL,		"",		FLIT_saveFile},
	{"saveWind",		CFFUNC,			0,	0,	NULL,		"",		FLIT_saveWind},
	{"scratchBuf",		0,			0,	0,	scratchBuf,	"",		FLIT_scratchBuf},
		// Returns name of buffer.
	{"searchBack",		CFNCOUNT,		1,	1,	searchBack,	LITERAL12,	FLIT_searchBack},
		// Returns string found, or false if not found.
	{"searchForw",		CFNCOUNT,		1,	1,	searchForw,	LITERAL12,	FLIT_searchForw},
		// Returns string found, or false if not found.
	{"selectBuf",		0,			1,	1,	selectBuf,	LITERAL4,	FLIT_selectBuf},
		// Returns name of buffer.
	{"setBufFile",		0,			1,	1,	setBufFile,	LITERAL4,	FLIT_setBufFile},
		// Returns new filename.
	{"setBufName",		CFNOARGS,		1,	1,	setBufName,	LITERAL9,	FLIT_setBufName},
		// Returns name of buffer.
	{"setHook",		CFSPECARGS,		2,	2,	setHook,	LITERAL33,	FLIT_setHook},
	{"setMark",		0,			0,	1,	setMark,	LITERAL22,	FLIT_setMark},
	{"setWrapCol",		0,			0,	0,	NULL,		"",		FLIT_setWrapCol},
	{"seti",		CFNOARGS,		1,	3,	seti,		LITERAL14,	FLIT_seti},
	{"shQuote",		CFFUNC | CFANY,		1,	1,	NULL,		LITERAL13,	FLIT_shQuote},
		// Returns quoted string.
	{"shell",		0,			0,	0,	shellCLI,	"",		FLIT_shell},
		// Returns false if failure.
	{"shellCmd",		0,			1,	-1,	shellCmd,	LITERAL10,	FLIT_shellCmd},
		// Returns false if failure.
	{"shift",	CFFUNC | CFSPECARGS | CFNOLOAD,	2,	2,	NULL,		LITERAL23,	FLIT_shift},
		// Returns shifted value, or nil if none left.
	{"showBindings",	CFADDLARG,		0,	1,	showBindings,	LITERAL6,	FLIT_showBindings},
	{"showBuffers",		0,			0,	0,	showBuffers,	"",		FLIT_showBuffers},
	{"showFunctions",	CFADDLARG,		0,	1,	showFunctions,	LITERAL6,	FLIT_showFunctions},
	{"showHooks",		0,			0,	0,	showHooks,	"",		FLIT_showHooks},
	{"showKey",		0,			1,	0,	showKey,	LITERAL16,	FLIT_showKey},
	{"showKillRing",	0,			0,	0,	showKillRing,	"",		FLIT_showKillRing},
	{"showMarks",		0,			0,	0,	showMarks,	"",		FLIT_showMarks},
#if MMDEBUG & DEBUG_SHOWRE
	{"showRegexp",		0,			0,	0,	showRegexp,	"",		FLIT_showRegexp},
#endif
	{"showScreens",		0,			0,	0,	showScreens,	"",		FLIT_showScreens},
	{"showVariables",	CFADDLARG,		0,	1,	showVariables,	LITERAL6,	FLIT_showVariables},
	{"shrinkWind",		CFNCOUNT,		0,	0,	NULL,		"",		FLIT_shrinkWind},
	{"space",		CFEDIT | CFNCOUNT,	0,	0,	NULL,		"",		FLIT_space},
	{"splitWind",		0,			0,	0,	splitWind,	"",		FLIT_splitWind},
	{"sprintf",		CFFUNC | CFANY,		1,	-1,	NULL,		LITERAL32,	FLIT_sprintf},
	{"string?",	CFFUNC | CFANY | CFNIL1 | CFBOOL1,1,	1,	NULL,		LITERAL13,	FLIT_stringQ},
		// Returns true if string value.
	{"stringFit",		CFFUNC | CFNUM2,	2,	2,	NULL,		LITERAL28,	FLIT_stringFit},
		// Returns compressed string.
	{"strip",		CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_strip},
		// Returns whitespace-stripped string.
	{"sub",			CFFUNC | CFNIL3,	3,	3,	NULL,		LITERAL18,	FLIT_sub},
		// Returns string result.
	{"subLine",	CFFUNC | CFNUM1 | CFNUM2,	1,	2,	NULL,		LITERAL26,	FLIT_subLine},
		// Returns string result.
	{"subString",	CFFUNC | CFNUM2 | CFNUM3,	2,	3,	NULL,		LITERAL27,	FLIT_subString},
		// Returns string result.
	{"suspend",		0,			0,	0,	suspendEMacs,	"",		FLIT_suspend},
	{"swapMark",		0,			0,	1,	swapMark,	LITERAL22,	FLIT_swapMark},
	{"tab",			CFEDIT,			0,	0,	NULL,		"",		FLIT_tab},
	{"tcString",		CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_tcString},
		// Returns string result.
	{"tcWord",		CFEDIT | CFNCOUNT,	0,	0,	tcWord,		"",		FLIT_tcWord},
	{"toInt",		CFFUNC | CFANY,		1,	1,	NULL,		LITERAL12,	FLIT_toInt},
		// Returns integer result.
	{"toString",	CFFUNC | CFANY | CFNIL1 | CFBOOL1,1,	1,	NULL,		LITERAL13,	FLIT_toString},
		// Returns string result.
	{"tr",			CFFUNC | CFNIL3,	3,	3,	NULL,		LITERAL18,	FLIT_tr},
		// Returns string result.
	{"traverseLine",	0,			0,	0,	traverseLine,	"",		FLIT_traverseLine},
	{"trimLine",		CFEDIT,			0,	0,	trimLine,	"",		FLIT_trimLine},
	{"truncBuf",		CFEDIT,			0,	0,	NULL,		"",		FLIT_truncBuf},
		// Returns name of buffer.
	{"ucLine",		CFEDIT,			0,	0,	NULL,		"",		FLIT_ucLine},
	{"ucRegion",		CFEDIT,			0,	0,	NULL,		"",		FLIT_ucRegion},
	{"ucString",		CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_ucString},
		// Returns string result.
	{"ucWord",		CFEDIT | CFNCOUNT,	0,	0,	ucWord,		"",		FLIT_ucWord},
	{"unbindKey",		0,			1,	1,	unbindKey,	LITERAL16,	FLIT_unbindKey},
		// Returns Boolean result if n > 0.
	{"unchangeBuf",		CFADDLARG,		0,	1,	NULL,		LITERAL9,	FLIT_unchangeBuf},
		// Returns name of buffer.
	{"undelete",		CFEDIT,			0,	0,	NULL,		"",		FLIT_undelete},
	{"unhideBuf",		CFADDLARG,		0,	1,	NULL,		LITERAL9,	FLIT_unhideBuf},
		// Returns name of buffer.
	{"universalArg",	CFHIDDEN | CFBIND1 | CFUNIQ,0,	0,	NULL,		"",		FLIT_universalArg},
	{"unshift",	CFFUNC | CFSPECARGS | CFNOLOAD,	3,	3,	NULL,		LITERAL25,	FLIT_unshift},
		// Returns unshifted value.
	{"updateScreen",	CFFUNC,			0,	0,	NULL,		"",		FLIT_updateScreen},
	{"viewFile",		0,			1,	1,	NULL,		LITERAL4,	FLIT_viewFile},
		// Returns name of buffer, a tab, and "true" if buffer created.
	{"void?",	CFFUNC | CFANY | CFNIL1 | CFBOOL1,1,	1,	NULL,		LITERAL13,	FLIT_voidQ},
		// Returns true if nil value or null string.
	{"whence",		CFTERM,			0,	0,	whence,		"",		FLIT_whence},
	{"widenBuf",		0,			0,	0,	widenBuf,	"",		FLIT_widenBuf},
	{"wordChar?",		CFFUNC,			1,	1,	NULL,		LITERAL12,	FLIT_wordCharQ},
		// Returns true if first character of a string is in $wordChars.
	{"wrapLine",		CFEDIT,			2,	2,	wrapLine,	LITERAL31,	FLIT_wrapLine},
	{"wrapWord",		CFEDIT,			0,	0,	wrapWord,	"",		FLIT_wrapWord},
	{"writeBuf",		CFFUNC,			2,	-1,	writeBuf,	LITERAL5,	FLIT_writeBuf},
		// Returns text written.
	{"writeFile",		0,			1,	1,	NULL,		LITERAL4,	FLIT_writeFile},
		// Returns filename.
	{"xPathname",		CFFUNC, 		1,	1,	NULL,		LITERAL15,	FLIT_xPathname},
		// Returns absolute pathname, or nil if not found.
	{"xeqBuf",		0,			1,	-1,	xeqBuf,		LITERAL17,	FLIT_xeqBuf},
		// Returns execution result.
	{"xeqFile",		0,			1,	-1,	xeqFile,	LITERAL17,	FLIT_xeqFile},
		// Returns execution result.
	{"xeqKeyMacro",		0,			0,	0,	xeqKeyMacro,	"",		FLIT_xeqKeyMacro},
	{"yank",		CFEDIT,			0,	0,	NULL,		"",		FLIT_yank},
	{"yankPop",		CFEDIT,			0,	0,	yankPop,	"",		FLIT_yankPop},
	{NULL,			0,			0,	0,	NULL,		NULL,		NULL}
	};

#define NFUNCS	(sizeof(cftab) / sizeof(CmdFunc)) - 1

#else
extern CmdFunc cftab[];
#endif
