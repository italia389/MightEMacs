// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
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
// All commands that have the CFNCOUNT flag set and a non-NULL cf_func member are never executed when the n argument is zero.

#ifdef DATAfunc
CmdFunc cftab[] = {
	{"abort",		CFBIND1 | CFUNIQ,		-1,	abortOp,	LITERAL1,	FLIT_abort},
	{"about",		0,				0,	aboutMM,	"",		FLIT_about},
	{"abs",			CFFUNC | CFNUM1,		1,	NULL,		LITERAL21,	FLIT_abs},
	{"alias",		CFSPECARGS,			0,	aliasCM,	LITERAL2,	FLIT_alias},
	{"alterBufMode",	0,				-2,	NULL,		LITERAL3,	FLIT_alterBufMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterDefMode",	0,				-2,	NULL,		LITERAL3,	FLIT_alterDefMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterGlobalMode",	0,				-2,	NULL,		LITERAL3,	FLIT_alterGlobalMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterShowMode",	0,				-2,	NULL,		LITERAL3,	FLIT_alterShowMode},
		// Returns state (-1 or 1) or last mode altered.
	{"appendFile",		0,				1,	NULL,		LITERAL4,	FLIT_appendFile},
		// Returns filename.
	{"backChar",		CFNCOUNT,			0,	backChar,	"",		FLIT_backChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backLine",		CFNCOUNT,			0,	backLine,	"",		FLIT_backLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backPage",		CFNCOUNT,			0,	backPage,	"",		FLIT_backPage},
	{"backPageNext",	CFNCOUNT,			0,	NULL,		"",		FLIT_backPageNext},
	{"backPagePrev",	CFNCOUNT,			0,	NULL,		"",		FLIT_backPagePrev},
	{"backTab",		CFNCOUNT,			0,	NULL,		"",		FLIT_backTab},
	{"backWord",		CFNCOUNT,			0,	backWord,	"",		FLIT_backWord},
	{"basename",		CFFUNC,				1,	NULL,		LITERAL15,	FLIT_basename},
		// Returns filename component of pathname.
	{"beep",		0,				0,	beeper,		"",		FLIT_beep},
	{"beginBuf",		CFADDLARG,			0,	NULL,		LITERAL9,	FLIT_beginBuf},
	{"beginKeyMacro",	0,				0,	beginKeyMacro,	"",		FLIT_beginKeyMacro},
	{"beginLine",		0,				0,	NULL,		"",		FLIT_beginLine},
	{"beginText",		0,				0,	beginText,	"",		FLIT_beginText},
	{"beginWhite",		0,				0,	NULL,		"",		FLIT_beginWhite},
	{"bindKey",		CFSPECARGS,			0,	bindKeyCM,	LITERAL7,	FLIT_bindKey},
	{"binding",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_binding},
		// Returns name of command or macro key is bound to, or nil if none.
	{"bufBound?",		CFFUNC,				0,	NULL,		"",		FLIT_bufBoundQ},
		// Returns true if dot is at beginning, middle, or end of buffer per n argument.
	{"bufWind",		CFFUNC,				1,	NULL,		LITERAL4,	FLIT_bufWind},
		// Returns window number, or nil if not found.
	{"cPrefix",	CFHIDDEN | CFPREFIX | CFBIND1 | CFPERM,	0,	NULL,		NULL,		NULL},
	{"chDir",		0,				0,	changedir,	"",		FLIT_chDir},
		// Returns absolute pathname of new directory.
	{"chr",			CFFUNC | CFNUM1,		1,	NULL,		LITERAL21,	FLIT_chr},
		// Returns ordinal value of a character in string form.
	{"clearBuf",		CFADDLARG,			0,	clearBuf,	LITERAL9,	FLIT_clearBuf},
		// Returns false if buffer is not cleared; otherwise, true.
	{"clearKillRing",	0,				0,	NULL,		"",		FLIT_clearKillRing},
	{"clearMark",		0,				0,	clearMark,	"",		FLIT_clearMark},
	{"clearMsg",		CFFUNC,				0,	NULL,		"",		FLIT_clearMsg},
	{"copyFencedText",	0,				0,	NULL,		"",		FLIT_copyFencedText},
	{"copyLine",		0,				0,	NULL,		"",		FLIT_copyLine},
	{"copyRegion",		0,				0,	NULL,		"",		FLIT_copyRegion},
	{"copyToBreak",		0,				0,	NULL,		"",		FLIT_copyToBreak},
	{"copyWord",		0,				0,	NULL,		"",		FLIT_copyWord},
#if WORDCOUNT
	{"countWords",		CFTERM,				0,	countWords,	"",		FLIT_countWords},
#endif
	{"cycleKillRing",	0,				0,	NULL,		"",		FLIT_cycleKillRing},
	{"defined?",		CFFUNC | CFANY,			1,	NULL,		LITERAL4,	FLIT_definedQ},
		// Returns type of object, or nil if not found.
	{"deleteAlias",		CFSPECARGS,			0,	deleteAlias,	LITERAL8,	FLIT_deleteAlias},
	{"deleteBackChar",	CFEDIT | CFNCOUNT,		0,	NULL,		"",		FLIT_deleteBackChar},
	{"deleteBlankLines",	CFEDIT,				0,	deleteBlankLines,"",		FLIT_deleteBlankLines},
	{"deleteBuf",		0,				-2,	deleteBuf,	LITERAL8,	FLIT_deleteBuf},
		// Returns name of last buffer deleted.
	{"deleteFencedText",	CFEDIT,				0,	NULL,		"",		FLIT_deleteFencedText},
	{"deleteForwChar",	CFEDIT | CFNCOUNT,		0,	NULL,		"",		FLIT_deleteForwChar},
	{"deleteLine",		CFEDIT,				0,	NULL,		"",		FLIT_deleteLine},
	{"deleteMacro",		CFSPECARGS,			0,	deleteMacro,	LITERAL8,	FLIT_deleteMacro},
	{"deleteRegion",	CFEDIT,				0,	NULL,		"",		FLIT_deleteRegion},
	{"deleteScreen",	0,				0,	deleteScreen,	"",		FLIT_deleteScreen},
	{"deleteTab",		CFEDIT | CFNCOUNT,		0,	deleteTab,	"",		FLIT_deleteTab},
	{"deleteToBreak",	CFEDIT,				0,	NULL,		"",		FLIT_deleteToBreak},
	{"deleteWhite",		CFEDIT,				0,	NULL,		"",		FLIT_deleteWhite},
	{"deleteWind",		0,				0,	deleteWind,	"",		FLIT_deleteWind},
	{"deleteWord",		CFEDIT,				0,	NULL,		"",		FLIT_deleteWord},
	{"detabLine",		CFEDIT,				0,	detabLine,	"",		FLIT_detabLine},
	{"dirname",		CFFUNC,				1,	NULL,		LITERAL15,	FLIT_dirname},
		// Returns directory component of pathname.
	{"endBuf",		CFADDLARG,			0,	NULL,		LITERAL9,	FLIT_endBuf},
	{"endKeyMacro",		0,				0,	endKeyMacro,	"",		FLIT_endKeyMacro},
	{"endLine",		0,				0,	NULL,		"",		FLIT_endLine},
	{"endWhite",		0,				0,	NULL,		"",		FLIT_endWhite},
	{"endWord",		CFNCOUNT,			0,	endWord,	"",		FLIT_endWord},
	{"entabLine",		CFEDIT,				0,	entabLine,	"",		FLIT_entabLine},
	{"env",			CFFUNC,				1,	NULL,		LITERAL4,	FLIT_env},
		// Returns value of environmental variable, or nil if not found.
	{"eval",		0,				-2,	eval,		LITERAL10,	FLIT_eval},
		// Returns result of evaluation.
	{"exit",		0,				-1,	quit,		LITERAL1,	FLIT_exit},
	{"fileExists?",		CFFUNC,				1,	NULL,		LITERAL15,	FLIT_fileExistsQ},
		// Returns type of file, or nil if not found.
	{"findFile",		0,				1,	NULL,		LITERAL4,	FLIT_findFile},
		// Returns name of buffer, a tab, and "true" if buffer created.
	{"forwChar",		CFNCOUNT,			0,	forwChar,	"",		FLIT_forwChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwLine",		CFNCOUNT,			0,	forwLine,	"",		FLIT_forwLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwPage",		CFNCOUNT,			0,	forwPage,	"",		FLIT_forwPage},
	{"forwPageNext",	CFNCOUNT,			0,	NULL,		"",		FLIT_forwPageNext},
	{"forwPagePrev",	CFNCOUNT,			0,	NULL,		"",		FLIT_forwPagePrev},
	{"forwTab",		CFNCOUNT,			0,	NULL,		"",		FLIT_forwTab},
	{"forwWord",		CFNCOUNT,			0,	forwWord,	"",		FLIT_forwWord},
	{"getKey",		CFFUNC,				0,	NULL,		"",		FLIT_getKey},
		// Returns key in encoded form.
	{"gotoFence",		0,				0,	NULL,		"",		FLIT_gotoFence},
	{"gotoLine",		0,				0,	gotoLine,	"",		FLIT_gotoLine},
	{"gotoMark",		0,				0,	gotoMark,	"",		FLIT_gotoMark},
	{"growWind",		CFNCOUNT,			0,	NULL,		"",		FLIT_growWind},
	{"hPrefix",	CFHIDDEN | CFPREFIX | CFBIND1 | CFPERM,	0,	NULL,		NULL,		NULL},
	{"help",		CFTERM,				0,	help,		"",		FLIT_help},
	{"hideBuf",		CFADDLARG,			0,	NULL,		LITERAL9,	FLIT_hideBuf},
		// Returns name of buffer.
	{"huntBack",		CFNCOUNT,			0,	huntBack,	"",		FLIT_huntBack},
		// Returns string found, or false if not found.
	{"huntForw",		CFNCOUNT,			0,	huntForw,	"",		FLIT_huntForw},
		// Returns string found, or false if not found.
	{"include?",		CFFUNC,				3,	NULL,		LITERAL29,	FLIT_includeQ},
		// Returns true if value is in given list.
	{"indentRegion",	CFEDIT | CFNCOUNT,		0,	indentRegion,	"",		FLIT_indentRegion},
	{"index",		CFFUNC,				2,	NULL,		LITERAL19,	FLIT_index},
		// Returns string position, or nil if not found.
	{"insert",		CFFUNC | CFEDIT,		-2,	NULL,		LITERAL10,	FLIT_insert},
		// Returns text inserted.
	{"insertBuf",		CFEDIT,				1,	insertBuf,	LITERAL4,	FLIT_insertBuf},
		// Returns name of buffer.
	{"insertFile",		CFEDIT,				1,	insertFile,	LITERAL4,	FLIT_insertFile},
		// Returns filename.
	{"insertLineI",		CFEDIT | CFNCOUNT,		0,	insertLineI,	"",		FLIT_insertLineI},
	{"insertPipe",		CFEDIT,				-2,	insertPipe,	LITERAL10,	FLIT_insertPipe},
		// Returns false if failure.
	{"insertSpace",		CFEDIT | CFNCOUNT,		0,	NULL,		"",		FLIT_insertSpace},
	{"inserti",		CFEDIT | CFNCOUNT,		0,	inserti,	"",		FLIT_inserti},
	{"int?",		CFFUNC | CFANY,			1,	NULL,		LITERAL13,	FLIT_intQ},
		// Returns true if integer value.
	{"join",		CFFUNC | CFSPECARGS,		1,	NULL,		LITERAL20,	FLIT_join},
		// Returns string result.
	{"joinLines",		CFEDIT,				1,	joinLines,	LITERAL30,	FLIT_joinLines},
	{"joinWind",		0,				0,	joinWind,	"",		FLIT_joinWind},
	{"killFencedText",	CFEDIT,				0,	NULL,		"",		FLIT_killFencedText},
	{"killLine",		CFEDIT,				0,	NULL,		"",		FLIT_killLine},
	{"killRegion",		CFEDIT,				0,	NULL,		"",		FLIT_killRegion},
	{"killToBreak",		CFEDIT,				0,	NULL,		"",		FLIT_killToBreak},
	{"killWord",		CFEDIT,				0,	NULL,		"",		FLIT_killWord},
	{"lcLine",		CFEDIT,				0,	NULL,		"",		FLIT_lcLine},
	{"lcRegion",		CFEDIT,				0,	NULL,		"",		FLIT_lcRegion},
	{"lcString",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_lcString},
		// Returns string result.
	{"lcWord",		CFEDIT | CFNCOUNT,		0,	lcWord,		"",		FLIT_lcWord},
	{"length",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_length},
		// Returns string length.
	{"let",			CFTERM,				0,	setvar,		"",		FLIT_let},
	{"markBuf",		0,				0,	markBuf,	"",		FLIT_markBuf},
	{"match",		CFFUNC | CFNUM1,		1,	NULL,		LITERAL21,	FLIT_match},
		// Returns value of pattern match, or null if none.
	{"metaPrefix",	CFHIDDEN | CFPREFIX | CFBIND1 | CFPERM,	0,	NULL,		NULL,		NULL},
	{"moveWindDown",	CFNCOUNT,			0,	NULL,		"",		FLIT_moveWindDown},
	{"moveWindUp",		CFNCOUNT,			0,	moveWindUp,	"",		FLIT_moveWindUp},
	{"narrowBuf",		0,				0,	narrowBuf,	"",		FLIT_narrowBuf},
	{"negativeArg",		CFHIDDEN | CFBIND1 | CFUNIQ,	0,	NULL,		NULL,		NULL},
	{"newScreen",		0,				0,	newScreen,	"",		FLIT_newScreen},
		// Returns new screen number.
	{"newline",		CFEDIT | CFNCOUNT,		0,	NULL,		"",		FLIT_newline},
	{"newlineI",		CFEDIT | CFNCOUNT,		0,	newlineI,	"",		FLIT_newlineI},
	{"nextArg",		CFFUNC,				0,	NULL,		"",		FLIT_nextArg},
		// Returns value of next macro argument.
	{"nextBuf",		0,				0,	NULL,		"",		FLIT_nextBuf},
		// Returns name of buffer.
	{"nextScreen",		0,				0,	nextScreen,	"",		FLIT_nextScreen},
		// Returns new screen number.
	{"nextWind",		0,				0,	nextWind,	"",		FLIT_nextWind},
	{"nil?",		CFFUNC | CFANY,			1,	NULL,		LITERAL13,	FLIT_nilQ},
		// Returns true if nil value.
	{"notice",		CFFUNC,				-2,	notice,		LITERAL10,	FLIT_notice},
		// Returns true if default n or n >= 0; otherwise, false.
	{"null?",		CFFUNC | CFANY,			1,	NULL,		LITERAL13,	FLIT_nullQ},
		// Returns true if null string.
	{"numeric?",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_numericQ},
		// Returns true if numeric literal.
	{"onlyWind",		0,				0,	onlyWind,	"",		FLIT_onlyWind},
	{"openLine",		CFEDIT | CFNCOUNT,		0,	openLine,	"",		FLIT_openLine},
	{"ord",			CFFUNC,				1,	NULL,		LITERAL12,	FLIT_ord},
		// Returns ordinal value of first character of a string.
	{"outdentRegion",	CFEDIT | CFNCOUNT,		0,	outdentRegion,	"",		FLIT_outdentRegion},
	{"overwrite",		CFFUNC | CFEDIT,		-2,	NULL,		LITERAL10,	FLIT_overwrite},
		// Returns new text.
	{"pad",			CFFUNC | CFANY,			2,	NULL,		LITERAL22,	FLIT_pad},
		// Returns padded string.
	{"pathname",		CFFUNC,				1,	NULL,		LITERAL15,	FLIT_pathname},
		// Returns absolute pathname of a file or directory.
	{"pause",		CFFUNC,				0,	NULL,		"",		FLIT_pause},
	{"pipeBuf",		CFEDIT,				-2,	pipeBuf,	LITERAL10,	FLIT_pipeBuf},
		// Returns false if failure.
	{"pop",			CFFUNC | CFSPECARGS,		0,	NULL,		LITERAL23,	FLIT_pop},
		// Returns popped value, or nil if none left.
	{"prevBuf",		0,				0,	NULL,		"",		FLIT_prevBuf},
		// Returns name of buffer.
	{"prevScreen",		0,				0,	NULL,		"",		FLIT_prevScreen},
		// Returns new screen number.
	{"prevWind",		0,				0,	prevWind,	"",		FLIT_prevWind},
	{"print",		CFFUNC,				-2,	NULL,		LITERAL10,	FLIT_print},
	{"prompt",		CFFUNC | CFSPECARGS,		1,	NULL,		LITERAL24,	FLIT_prompt},
		// Returns response read from keyboard.
	{"push",		CFFUNC | CFSPECARGS,		0,	NULL,		LITERAL25,	FLIT_push},
		// Returns pushed value.
	{"queryReplace",	CFEDIT,				2,	NULL,		LITERAL11,	FLIT_queryReplace},
		// Returns false if search stopped prematurely; otherwise, true.
	{"quickExit",		0,				0,	NULL,		"",		FLIT_quickExit},
	{"quote",		CFFUNC | CFANY,			1,	NULL,		LITERAL13,	FLIT_quote},
		// Returns quoted string.
	{"quoteChar",	CFBIND1 | CFUNIQ | CFEDIT | CFNCOUNT,	0,	quoteChar,	"",		FLIT_quoteChar},
	{"rand",		CFFUNC,				0,	NULL,		"",		FLIT_rand},
		// Returns random integer.
	{"readBuf",		CFFUNC,				1,	readBuf,	LITERAL4,	FLIT_readBuf},
		// Returns next line from buffer.
	{"readFile",		0,				1,	NULL,		LITERAL4,	FLIT_readFile},
		// Returns name of buffer.
	{"readPipe",		0,				-2,	readPipe,	LITERAL10,	FLIT_readPipe},
		// Returns name of buffer, or false if failure.
	{"redrawScreen",	0,				0,	NULL,		"",		FLIT_redrawScreen},
	{"replace",		CFEDIT | CFNCOUNT,		2,	NULL,		LITERAL11,	FLIT_replace},
	{"replaceText",		CFFUNC | CFEDIT,		-2,	NULL,		LITERAL10,	FLIT_replaceText},
		// Returns new text.
	{"resetTerm",		0,				0,	resetTermc,	"",		FLIT_resetTerm},
	{"resizeWind",		0,				0,	resizeWind,	"",		FLIT_resizeWind},
	{"restoreBuf",		CFFUNC,				0,	NULL,		"",		FLIT_restoreBuf},
		// Returns name of buffer.
	{"restoreWind",		CFFUNC,				0,	NULL,		"",		FLIT_restoreWind},
	{"reverse",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_reverse},
		// Returns reversed string.
	{"run",			CFPERM | CFSPECARGS,		0,	run,		LITERAL4,	FLIT_run},
		// Returns execution result.
	{"saveBuf",		CFFUNC,				0,	NULL,		"",		FLIT_saveBuf},
		// Returns name of buffer.
	{"saveFile",		0,				0,	NULL,		"",		FLIT_saveFile},
	{"saveWind",		CFFUNC,				0,	NULL,		"",		FLIT_saveWind},
	{"scratchBuf",		0,				0,	scratchBuf,	"",		FLIT_scratchBuf},
		// Returns name of buffer.
	{"searchBack",		CFNCOUNT,			1,	searchBack,	LITERAL12,	FLIT_searchBack},
		// Returns string found, or false if not found.
	{"searchForw",		CFNCOUNT,			1,	searchForw,	LITERAL12,	FLIT_searchForw},
		// Returns string found, or false if not found.
	{"selectBuf",		0,				1,	selectBuf,	LITERAL4,	FLIT_selectBuf},
		// Returns name of buffer.
	{"setBufFile",		0,				1,	setBufFile,	LITERAL4,	FLIT_setBufFile},
		// Returns new filename.
	{"setBufName",		CFNOARGS,			1,	setBufName,	LITERAL9,	FLIT_setBufName},
		// Returns name of buffer.
	{"setMark",		0,				0,	setMark,	"",		FLIT_setMark},
	{"setWrapCol",		0,				0,	NULL,		"",		FLIT_setWrapCol},
	{"seti",		CFSPECARGS | CFNOARGS,		-2,	seti,		LITERAL14,	FLIT_seti},
	{"shQuote",		CFFUNC | CFANY,			1,	NULL,		LITERAL13,	FLIT_shQuote},
		// Returns quoted string.
	{"shell",		0,				0,	shellCLI,	"",		FLIT_shell},
		// Returns false if failure.
	{"shellCmd",		0,				-2,	shellCmd,	LITERAL10,	FLIT_shellCmd},
		// Returns false if failure.
	{"shift",		CFFUNC | CFSPECARGS,		0,	NULL,		LITERAL23,	FLIT_shift},
		// Returns shifted value, or nil if none left.
	{"showBindings",	CFADDLARG,			0,	showBindings,	LITERAL6,	FLIT_showBindings},
	{"showBuffers",		0,				0,	showBuffers,	"",		FLIT_showBuffers},
	{"showFunctions",	CFADDLARG,			0,	showFunctions,	LITERAL6,	FLIT_showFunctions},
	{"showKey",		CFTERM,				1,	showKey,	LITERAL16,	FLIT_showKey},
	{"showKillRing",	0,				0,	showKillRing,	"",		FLIT_showKillRing},
#if MMDEBUG & DEBUG_SHOWRE
	{"showRegExp",		0,				0,	showRegExp,		"",		FLIT_showRegExp},
#endif
	{"showScreens",		0,				0,	showScreens,	"",		FLIT_showScreens},
	{"showVariables",	CFADDLARG,			0,	showVariables,	LITERAL6,	FLIT_showVariables},
	{"shrinkWind",		CFNCOUNT,			0,	NULL,		"",		FLIT_shrinkWind},
	{"space",		CFEDIT | CFNCOUNT,		0,	NULL,		"",		FLIT_space},
	{"splitWind",		0,				0,	splitWind,	"",		FLIT_splitWind},
	{"sprintf",		CFFUNC | CFANY,			-2,	NULL,		LITERAL32,	FLIT_sprintf},
	{"string?",		CFFUNC | CFANY,			1,	NULL,		LITERAL13,	FLIT_stringQ},
		// Returns true if string value.
	{"stringFit",		CFFUNC | CFNUM2,		2,	NULL,		LITERAL28,	FLIT_stringFit},
		// Returns compressed string.
	{"stringLit",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_stringLit},
		// Returns exposed string.
	{"strip",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_strip},
		// Returns whitespace-stripped string.
	{"sub",			CFFUNC,				3,	NULL,		LITERAL18,	FLIT_sub},
		// Returns string result.
	{"subLine",		CFFUNC | CFNUM1 | CFNUM2,	2,	NULL,		LITERAL26,	FLIT_subLine},
		// Returns string result.
	{"subString",		CFFUNC | CFNUM2 | CFNUM3,	3,	NULL,		LITERAL27,	FLIT_subString},
		// Returns string result.
	{"suspend",		0,				0,	suspendEMacs,	"",		FLIT_suspend},
	{"swapMark",		0,				0,	swapMark,	"",		FLIT_swapMark},
	{"tab",			CFEDIT,				0,	NULL,		"",		FLIT_tab},
	{"tcString",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_tcString},
		// Returns string result.
	{"tcWord",		CFEDIT | CFNCOUNT,		0,	tcWord,		"",		FLIT_tcWord},
	{"toInt",		CFFUNC | CFANY,			1,	NULL,		LITERAL12,	FLIT_toInt},
		// Returns integer result.
	{"toString",		CFFUNC | CFANY,			1,	NULL,		LITERAL21,	FLIT_toString},
		// Returns string result.
	{"tr",			CFFUNC,				3,	NULL,		LITERAL18,	FLIT_tr},
		// Returns string result.
	{"traverseLine",	0,				0,	traverseLine,	"",		FLIT_traverseLine},
	{"trimLine",		CFEDIT,				0,	trimLine,	"",		FLIT_trimLine},
	{"truncBuf",		CFEDIT,				0,	NULL,		"",		FLIT_truncBuf},
		// Returns name of buffer.
	{"ucLine",		CFEDIT,				0,	NULL,		"",		FLIT_ucLine},
	{"ucRegion",		CFEDIT,				0,	NULL,		"",		FLIT_ucRegion},
	{"ucString",		CFFUNC,				1,	NULL,		LITERAL12,	FLIT_ucString},
		// Returns string result.
	{"ucWord",		CFEDIT | CFNCOUNT,		0,	ucWord,		"",		FLIT_ucWord},
	{"unbindKey",		0,				1,	unbindKey,	LITERAL16,	FLIT_unbindKey},
		// Returns Boolean result if n > 0.
	{"unchangeBuf",		CFADDLARG,			0,	NULL,		LITERAL9,	FLIT_unchangeBuf},
		// Returns name of buffer.
	{"undelete",		CFEDIT,				0,	NULL,		"",		FLIT_undelete},
	{"unhideBuf",		CFADDLARG,			0,	NULL,		LITERAL9,	FLIT_unhideBuf},
		// Returns name of buffer.
	{"universalArg",	CFHIDDEN | CFBIND1 | CFUNIQ,	0,	NULL,		NULL,		NULL},
	{"unshift",		CFFUNC | CFSPECARGS,		0,	NULL,		LITERAL25,	FLIT_unshift},
		// Returns unshifted value.
	{"updateScreen",	CFFUNC,				0,	NULL,		"",		FLIT_updateScreen},
	{"viewFile",		0,				1,	NULL,		LITERAL4,	FLIT_viewFile},
		// Returns name of buffer, a tab, and "true" if buffer created.
	{"whence",		CFTERM,				0,	whence,		"",		FLIT_whence},
	{"widenBuf",		0,				0,	widenBuf,	"",		FLIT_widenBuf},
	{"wrapLine",		CFEDIT,				2,	wrapLine,	LITERAL31,	FLIT_wrapLine},
	{"wrapWord",		CFEDIT,				0,	wrapWord,	"",		FLIT_wrapWord},
	{"writeBuf",		CFFUNC,				-3,	writeBuf,	LITERAL5,	FLIT_writeBuf},
		// Returns text written.
	{"writeFile",		0,				1,	NULL,		LITERAL4,	FLIT_writeFile},
		// Returns filename.
	{"xPathname",		CFFUNC, 			1,	NULL,		LITERAL15,	FLIT_xPathname},
		// Returns absolute pathname, or nil if not found.
	{"xPrefix",	CFHIDDEN | CFPREFIX | CFBIND1 | CFPERM, 0,	NULL,		NULL,		NULL},
	{"xeqBuf",		0,				-2,	xeqBuf,		LITERAL17,	FLIT_xeqBuf},
		// Returns execution result.
	{"xeqFile",		0,				-2,	xeqFile,	LITERAL17,	FLIT_xeqFile},
		// Returns execution result.
	{"xeqKeyMacro",		0,				0,	xeqKeyMacro,	"",		FLIT_xeqKeyMacro},
	{"yank",		CFEDIT,				0,	NULL,		"",		FLIT_yank},
	{"yankPop",		CFEDIT,				0,	yankPop,	"",		FLIT_yankPop},
	{NULL,			0,				0,	NULL,		NULL,		NULL}
	};

#define NFUNCS	(sizeof(cftab) / sizeof(CmdFunc)) - 1

#else
extern CmdFunc cftab[];
#endif
