// (c) Copyright 2020 Richard W. Marinelli
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
//  1. If the fifth value (func) is not NULL, CFShortLoad, CFNoLoad, ArgIntN, and ArgNISN are ignored.
//  2. If CFSpecArgs is set and func is not NULL, the third and fourth values (minArgs and maxArgs) are not used.
//     However if func is NULL and CFFunc is set, then minArgs is used by execCmdFunc() to get the initial arguments.
//  3. If CFNCount is set and func is not NULL, the specified C function is never executed when the n argument is zero.

#include "cxl/hash.h"

#ifdef CmdData

// **** For main.c ****

// Global variables.
Alias *ahead = NULL;			// Head of alias list.
CmdFunc cmdFuncTable[] = {
	{"abort",		CFBind1 | CFUniq, 0,		0, -1,	abortOp,	text800,	CFLit_abort},
	{"about",		0, 0,				0, 0,	aboutMM,	NULL,		CFLit_about},
	{"abs",			CFFunc, ArgInt1,		1, 1,	NULL,		text820,	CFLit_abs},
	{"alias",		CFSpecArgs | CFNoLoad, 0,	2, 2,	alias,		text801,	CFLit_alias},
	{"appendFile",		CFNoLoad, 0,			1, 1,	NULL,		text868,	CFLit_appendFile},
		// Returns filename.
	{"apropos",		CFAddlArg, ArgNil1,		1, 2,	apropos,	text873,	CFLit_apropos},
	{"array", CFFunc, ArgInt1 | ArgBool2 | ArgArray2 | ArgNIS2, 0, 2, array,	text828,	CFLit_array},
		// Returns new array.
	{"backChar",		CFNCount, 0,			0, 0,	backChar,	NULL,		CFLit_backChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backLine",		CFNCount, 0,			0, 0,	backLine,	NULL,		CFLit_backLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backPage",		0, 0,				0, 0,	backPage,	NULL,		CFLit_backPage},
	{"backPageNext",	0, 0,				0, 0,	NULL,		NULL,		CFLit_backPageNext},
	{"backPagePrev",	0, 0,				0, 0,	NULL,		NULL,		CFLit_backPagePrev},
	{"backTab",		CFNCount, 0,			0, 0,	NULL,		NULL,		CFLit_backTab},
	{"backWord",		CFNCount, 0,			0, 0,	backWord,	NULL,		CFLit_backWord},
	{"backspace",		CFEdit | CFNCount, 0,		0, 0,	NULL,		NULL,		CFLit_backspace},
	{"basename",		CFFunc, 0,			1, 1,	NULL,		text814,	CFLit_basename},
		// Returns filename component of pathname.
	{"beep",		CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_beep},
	{"beginBuf",		CFNoLoad, 0,			0, 1,	NULL,		text808,	CFLit_beginBuf},
	{"beginLine",		0, 0,				0, 0,	NULL,		NULL,		CFLit_beginLine},
	{"beginMacro",		CFTerm, 0,			0, 0,	beginMacro,	NULL,		CFLit_beginMacro},
	{"beginText",		0, 0,				0, 0,	beginText,	NULL,		CFLit_beginText},
	{"beginWhite",		0, 0,				0, 0,	NULL,		NULL,		CFLit_beginWhite},
	{"bempty?",	CFFunc | CFAddlArg, ArgNotNull1,	0, 1,	NULL,		text808,	CFLit_bemptyQ},
		// Returns true if specified buffer is empty; otherwise, false.
	{"bgets",	CFFunc | CFNCount | CFNoLoad, 0,	1, 1,	bgets,		text867,	CFLit_bgets},
		// Returns nth next line from buffer.
	{"bindKey",	CFSpecArgs | CFShortLoad, ArgNotNull1,	2, 2,	bindKey,	text806,	CFLit_bindKey},
	{"binding",	CFFunc, ArgNotNull1 | ArgNotNull2,	2, 2,	binding,	text856,	CFLit_binding},
		// Returns name of command key is bound to, or nil if none; or array of key bindings for given command.
	{"bprint", CFFunc | CFShortLoad, ArgNotNull1,		2, -1,	bprint,		text804,	CFLit_bprint},
		// Returns text written.
	{"bprintf",		CFFunc, ArgNotNull1,		2, -1,	NULL,		text852,	CFLit_bprintf},
		// Returns text written.
	{"bufAttr?",	CFFunc, ArgNotNull1 | ArgNotNull2,	2, 2,	bufAttrQ,	text863,	CFLit_bufAttrQ},
		// Returns true if attribute flag set in buffer; otherwise, false.
	{"bufBound?",		CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_bufBoundQ},
		// Returns true if point is at beginning, middle, or end of buffer per n argument.
	{"bufInfo", CFFunc | CFAddlArg, ArgNil1 | ArgNotNull1 | ArgNotNull2,
								1, 2,	bufInfo,	text842,	CFLit_bufInfo},
		// Returns buffer information per keyword options.
	{"bufWind",		CFFunc, 0,			1, 1,	NULL,		text867,	CFLit_bufWind},
		// Returns ordinal number of first window on current screen displaying given buffer, or nil if none.
	{"chgBufAttr",		0, ArgNotNull1 | ArgNil2,	2, 2,	chgBufAttr,	text845,	CFLit_chgBufAttr},
		// Returns former state (-1 or 1) or last attribute altered.
	{"chgDir",		0, ArgNotNull1,			1, 1,	chgWorkDir,	text837,	CFLit_chgDir},
		// Returns absolute pathname of new directory.
	{"chgMode", 0, ArgNotNull1 | ArgNil1 | ArgNotNull2 | ArgNil2 | ArgArray2 | ArgMay,
								2, 2,	chgMode,	text847,	CFLit_chgMode},
		// Returns former state (-1 or 1) or last mode changed.
	{"chr",			CFFunc, ArgInt1,		1, 1,	NULL,		text820,	CFLit_chr},
		// Returns ordinal value of a character in string form.
	{"clearBuf",		CFNoLoad, 0,			0, 1,	clearBuf,	text808,	CFLit_clearBuf},
		// Returns false if buffer is not cleared; otherwise, true.
	{"clearHook",		CFFunc | CFNoLoad, 0,		0, -1,	clearHook,	text872,	CFLit_clearHook},
		// Returns zero if failure; otherwise, number of hooks cleared.
	{"clearMsgLine",	CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_clearMsgLine},
	{"clone",		CFFunc, ArgArray1,		1, 1,	NULL,		text822,	CFLit_clone},
		// Returns new array.
	{"copyFencedRegion",	0, 0,				0, 0,	NULL,		NULL,		CFLit_copyFencedRegion},
	{"copyLine",		0, 0,				0, 0,	NULL,		NULL,		CFLit_copyLine},
	{"copyRegion",		0, 0,				0, 0,	NULL,		NULL,		CFLit_copyRegion},
	{"copyToBreak",		0, 0,				0, 0,	NULL,		NULL,		CFLit_copyToBreak},
	{"copyWord",		0, 0,				0, 0,	NULL,		NULL,		CFLit_copyWord},
#if WordCount
	{"countWords",		CFTerm, 0,			0, 0,	countWords,	NULL,		CFLit_countWords},
#endif
	{"cycleRing",		CFNoLoad, 0,			1, 1,	cycleRing,	text874,	CFLit_cycleRing},
	{"defined?", CFFunc, ArgNotNull1 | ArgNotNull2 | ArgInt2 | ArgMay,
								2, 2,	definedQ,	text866,	CFLit_definedQ},
		// Returns kind of object, or nil if not found.
	{"delAlias",		CFSpecArgs | CFNoLoad, 0,	1, -1,	delAlias,	text807,	CFLit_delAlias},
		// Returns zero if failure; otherwise, number of aliases deleted.
	{"delBackChar",		CFEdit | CFNCount, 0,		0, 0,	NULL,		NULL,		CFLit_delBackChar},
	{"delBackTab",		CFEdit | CFNCount, 0,		0, 0,	NULL,		NULL,		CFLit_delBackTab},
	{"delBlankLines",	CFEdit, 0,			0, 0,	delBlankLines,	NULL,		CFLit_delBlankLines},
	{"delBuf",		CFNoLoad, 0,			1, -1,	delBuf,		text836,	CFLit_delBuf},
		// Returns zero if failure; otherwise, number of buffers deleted.
	{"delFencedRegion",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_delFencedRegion},
	{"delForwChar",		CFEdit | CFNCount, 0,		0, 0,	NULL,		NULL,		CFLit_delForwChar},
	{"delForwTab",		CFEdit | CFNCount, 0,		0, 0,	NULL,		NULL,		CFLit_delForwTab},
	{"delLine",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_delLine},
	{"delMark",		CFNoLoad | CFNoArgs, 0,		0, 1,	delMark,	text851,	CFLit_delMark},
	{"delRegion",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_delRegion},
	{"delRingEntry",	CFNoLoad, 0,			1, 2,	delRingEntry,	text876,	CFLit_delRingEntry},
	{"delRoutine",		CFSpecArgs | CFNoLoad, 0,	1, -1,	delRoutine,	text807,	CFLit_delRoutine},
		// Returns zero if failure; otherwise, number of user commands and/or functions deleted.
	{"delScreen",		CFNoLoad, 0,			1, 1,	delScreen,	text820,	CFLit_delScreen},
	{"delToBreak",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_delToBreak},
	{"delWhite",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_delWhite},
	{"delWind",		CFAddlArg, ArgNotNull1,		0, 1,	delWind,	text844,	CFLit_delWind},
	{"delWord",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_delWord},
	{"detabLine",	CFEdit, ArgInt1 | ArgNil1 | ArgMay,	1, 1,	detabLine,	text862,	CFLit_detabLine},
	{"dirname",		CFFunc, 0,			1, 1,	NULL,		text814,	CFLit_dirname},
		// Returns directory component of pathname.
	{"dupLine",		CFEdit, 0,			0, 0,	dupLine,	NULL,		CFLit_dupLine},
	{"editMode",		0, ArgNotNull1,			1, -1,	editMode,	text853,	CFLit_editMode},
	{"editModeGroup",	0, ArgNotNull1,			1, -1,	editModeGroup,	text854,	CFLit_editModeGroup},
	{"empty?",	CFFunc, ArgNIS1 | ArgArray1 | ArgMay,	1, 1,	NULL,		text812,	CFLit_emptyQ},
		// Returns true if nil, null string, or empty array.
	{"endBuf",		CFNoLoad, 0,			0, 1,	NULL,		text808,	CFLit_endBuf},
	{"endLine",		0, 0,				0, 0,	NULL,		NULL,		CFLit_endLine},
	{"endMacro",		CFTerm, 0,			0, 0,	endMacro,	NULL,		CFLit_endMacro},
	{"endWhite",		0, 0,				0, 0,	NULL,		NULL,		CFLit_endWhite},
	{"endWord",		CFNCount, 0,			0, 0,	endWord,	NULL,		CFLit_endWord},
		// Returns false if hit buffer boundary; otherwise, true.
	{"entabLine",	CFEdit, ArgInt1 | ArgNil1 | ArgMay,	1, 1,	entabLine,	text862,	CFLit_entabLine},
	{"env",			CFFunc, 0,			1, 1,	NULL,		text803,	CFLit_env},
		// Returns value of environmental variable, or nil if not found.
	{"eval",		CFNoLoad, 0,			1, -1,	eval,		text809,	CFLit_eval},
		// Returns result of evaluation.
	{"exit",		0, 0,				0, -1,	quit,		text800,	CFLit_exit},
	{"expandPath",		CFFunc, 0,			1, 1,	NULL,		text814,	CFLit_expandPath},
		// Returns pathname with "~/", "~user/", "$var", and "${var}" expanded.
	{"findFile",		CFNoLoad, 0,			1, 1,	NULL,		text868,	CFLit_findFile},
		// Returns [name of buffer, "true" or "false" indicating whether the buffer was created].
	{"forwChar",		CFNCount, 0,			0, 0,	forwChar,	NULL,		CFLit_forwChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwLine",		CFNCount, 0,			0, 0,	forwLine,	NULL,		CFLit_forwLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwPage",		0, 0,				0, 0,	forwPage,	NULL,		CFLit_forwPage},
	{"forwPageNext",	0, 0,				0, 0,	NULL,		NULL,		CFLit_forwPageNext},
	{"forwPagePrev",	0, 0,				0, 0,	NULL,		NULL,		CFLit_forwPagePrev},
	{"forwTab",		CFNCount, 0,			0, 0,	NULL,		NULL,		CFLit_forwTab},
	{"forwWord",		CFNCount, 0,			0, 0,	forwWord,	NULL,		CFLit_forwWord},
	{"getInfo",		CFFunc, ArgNotNull1,		1, 1,	getInfo,	text843,	CFLit_getInfo},
		// Returns informational item per keyword argument.
	{"getKey",	CFFunc | CFAddlArg, ArgNotNull1,	0, 1,	fGetKey,	text844,	CFLit_getKey},
		// Returns key in encoded form.
	{"getWord",		CFFunc, 0,			0, 0,	getWord,	NULL,		CFLit_getWord},
		// Returns word from current buffer.
	{"glob",		CFFunc, 0,			1, 1,	globPat,	text805,	CFLit_glob},
		// Returns array of pathnames.
	{"gotoFence",		CFAddlArg, ArgInt1,		0, 1,	gotoFence,	text857,	CFLit_gotoFence},
		// Returns true if matching fence found; otherwise, false.
	{"gotoLine",		CFAddlArg | CFNoLoad, 0,	1, 2,	gotoLine,	text833,	CFLit_gotoLine},
	{"gotoMark",		CFNoLoad, 0,			1, 1,	gotoMark,	text834,	CFLit_gotoMark},
	{"groupMode?",	CFFunc | CFAddlArg, ArgNotNull1 | ArgNil1 | ArgNotNull2 | ArgNotNull3,
								2, 3,	groupModeQ,	text855,	CFLit_groupModeQ},
		// Returns name of mode if a mode in a group is set; otherwise, nil.
	{"growWind",		CFNCount, 0,			0, 0,	NULL,		NULL,		CFLit_growWind},
	{"huntBack",		CFNCount, 0,			0, 0,	huntBack,	NULL,		CFLit_huntBack},
		// Returns string found, or false if not found.
	{"huntForw",		CFNCount, 0,			0, 0,	huntForw,	NULL,		CFLit_huntForw},
		// Returns string found, or false if not found.
	{"include?",		CFFunc | CFNoLoad, 0,		2, -1,	doIncl,		text846,	CFLit_includeQ},
		// Returns true if any/all expression values are in given array.
	{"indentRegion",	CFEdit | CFNCount, 0,		0, 0,	indentRegion,	NULL,		CFLit_indentRegion},
	{"index",	CFFunc | CFAddlArg, ArgInt2 | ArgMay | ArgNotNull3,
								2, 3,	fIndex,		text818,	CFLit_index},
		// Returns position of pattern in string, or nil if not found.
	{"insert",	CFFunc | CFEdit | CFShortLoad, 0,	1, -1,	NULL,		text809,	CFLit_insert},
		// Returns text inserted.
	{"insertBuf",		CFEdit | CFNoLoad, 0,		1, 1,	insertBuf,	text867,	CFLit_insertBuf},
		// Returns name of buffer.
	{"insertFile",		CFEdit | CFNoLoad, 0,		1, 1,	insertFile,	text868,	CFLit_insertFile},
		// Returns filename.
	{"insertPipe",		CFEdit | CFNoLoad, 0,		1, -1,	NULL,		text809,	CFLit_insertPipe},
		// Returns false if failure.
	{"insertSpace",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_insertSpace},
	{"inserti",		CFEdit | CFNCount, 0,		0, 0,	inserti,	NULL,		CFLit_inserti},
	{"interactive?",	CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_interactiveQ},
		// Returns true if script is being executed interactively.
	{"join",		CFFunc | CFShortLoad, ArgNil1,	2, -1,	NULL,		text819,	CFLit_join},
		// Returns string result.
	{"joinLines",		CFEdit, ArgNil1,		1, 1,	joinLines,	text829,	CFLit_joinLines},
	{"joinWind",		CFAddlArg, ArgNotNull1,		0, 1,	joinWind,	text844,	CFLit_joinWind},
	{"keyPending?",		CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_keyPendingQ},
		// Returns true if type-ahead key(s) pending.
	{"kill",		CFFunc, ArgInt1,		1, 1,	NULL,		text820,	CFLit_kill},
	{"killFencedRegion",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_killFencedRegion},
	{"killLine",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_killLine},
	{"killRegion",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_killRegion},
	{"killToBreak",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_killToBreak},
	{"killWord",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_killWord},
	{"lastBuf",		0, 0,				0, 0,	NULL,		NULL,		CFLit_lastBuf},
		// Returns name of buffer.
	{"length",		CFFunc, ArgArray1 | ArgMay,	1, 1,	NULL,		text812,	CFLit_length},
		// Returns string or array length.
	{"let",			CFTerm, 0,			0, 0,	setVar,		NULL,		CFLit_let},
	{"lowerCaseLine",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_lowerCaseLine},
	{"lowerCaseRegion",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_lowerCaseRegion},
	{"lowerCaseStr",	CFFunc, 0,			1, 1,	NULL,		text811,	CFLit_lowerCaseStr},
		// Returns string result.
	{"lowerCaseWord",	CFEdit | CFNCount, 0,		0, 0,	NULL,		NULL,		CFLit_lowerCaseWord},
	{"manageMacro",		CFFunc | CFNoLoad, 0,		1, 2,	manageMacro,	text848,	CFLit_manageMacro},
		// Returns various values, depending on operation and options.
	{"markBuf",		CFNoLoad, 0,			0, 1,	markBuf,	text851,	CFLit_markBuf},
	{"match",		CFFunc, ArgInt1,		1, 1,	NULL,		text820,	CFLit_match},
		// Returns value of pattern match, or null if none.
	{"message",		CFFunc | CFNoLoad, 0,		1, -1,	message,	text861,	CFLit_message},
		// Returns Boolean value.
	{"metaPrefix", CFHidden | CFPrefix | CFBind1 | CFPerm, 0, 0, 0,	NULL,		NULL,		CFLit_metaPrefix},
	{"mode?", CFFunc | CFAddlArg, ArgNotNull1 | ArgNil1 | ArgNotNull2 | ArgArray2 | ArgMay | ArgNotNull3,
								2, 3,	modeQ,		text802,	CFLit_modeQ},
		// Returns true if any/all mode(s) set; otherwise, false.
	{"moveWindDown",	CFNCount, 0,			0, 0,	NULL,		NULL,		CFLit_moveWindDown},
	{"moveWindUp",		CFNCount, 0,			0, 0,	moveWindUp,	NULL,		CFLit_moveWindUp},
	{"narrowBuf",		0, 0,				0, 0,	narrowBuf,	NULL,		CFLit_narrowBuf},
	{"negativeArg",		CFHidden | CFBind1 | CFUniq, 0,	0, 0,	NULL,		NULL,		CFLit_negativeArg},
	{"newline",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_newline},
	{"newlineI",		CFEdit | CFNCount, 0,		0, 0,	newlineI,	NULL,		CFLit_newlineI},
	{"nextBuf",		0, 0,				0, 0,	NULL,		NULL,		CFLit_nextBuf},
		// Returns name of buffer.
	{"nextScreen",		CFNCount, 0,			0, 0,	NULL,		NULL,		CFLit_nextScreen},
	{"nextWind",		0, 0,				0, 0,	nextWind,	NULL,		CFLit_nextWind},
	{"nil?",	CFFunc, ArgBool1 | ArgArray1 | ArgNIS1,	1, 1,	NULL,		text812,	CFLit_nilQ},
		// Returns true if expression is nil.
	{"null?",		CFFunc, 0,			1, 1,	NULL,		text811,	CFLit_nullQ},
		// Returns true if null string.
	{"numeric?",		CFFunc, 0,			1, 1,	NULL,		text811,	CFLit_numericQ},
		// Returns true if numeric literal.
	{"onlyWind",		0, 0,				0, 0,	onlyWind,	NULL,		CFLit_onlyWind},
	{"openLine",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_openLine},
	{"openLineI",		CFEdit | CFNCount, 0,		0, 0,	openLineI,	NULL,		CFLit_openLineI},
	{"ord",			CFFunc, 0,			1, 1,	NULL,		text811,	CFLit_ord},
		// Returns ordinal value of first character of a string.
	{"outdentRegion",	CFEdit | CFNCount, 0,		0, 0,	outdentRegion,	NULL,		CFLit_outdentRegion},
	{"overwrite",	CFFunc | CFEdit | CFShortLoad, 0,	1, -1,	NULL,		text809,	CFLit_overwrite},
		// Returns new text.
	{"pathname",	CFFunc, ArgArray1 | ArgMay | ArgPath,	1, 1,	absPathname,	text858,	CFLit_pathname},
		// Returns absolute pathname, or array of absolute pathnames.
	{"pause",		CFFunc, ArgInt1,		1, 1,	NULL,		text820,	CFLit_pause},
	{"pipeBuf",		CFEdit | CFNoLoad, 0,		1, -1,	NULL,		text809,	CFLit_pipeBuf},
		// Returns false if failure.
	{"pop",			CFFunc, ArgArray1,		1, 1,	NULL,		text822,	CFLit_pop},
		// Returns popped value, or nil if none left.
	{"popBuf",		CFNoLoad, 0,			1, 2,	NULL,		text842,	CFLit_popBuf},
		// Returns name of buffer.
	{"popFile",		CFNoLoad, 0,			1, 2,	NULL,		text870,	CFLit_popFile},
		// Returns name of buffer.
	{"prefix1", CFHidden | CFPrefix | CFBind1 | CFPerm, 0,	0, 0,	NULL,		NULL,		CFLit_prefix1},
	{"prefix2", CFHidden | CFPrefix | CFBind1 | CFPerm, 0,	0, 0,	NULL,		NULL,		CFLit_prefix2},
	{"prefix3", CFHidden | CFPrefix | CFBind1 | CFPerm, 0,	0, 0,	NULL,		NULL,		CFLit_prefix3},
	{"prevBuf",		0, 0,				0, 0,	NULL,		NULL,		CFLit_prevBuf},
		// Returns name of buffer.
	{"prevScreen",		CFNCount, 0,			0, 0,	NULL,		NULL,		CFLit_prevScreen},
	{"prevWind",		0, 0,				0, 0,	prevWind,	NULL,		CFLit_prevWind},
	{"print",		CFFunc | CFShortLoad, 0,	1, -1,	NULL,		text809,	CFLit_print},
	{"printf",		CFFunc, 0,			1, -1,	NULL,		text831,	CFLit_printf},
	{"prompt",		CFFunc, ArgNil1,		1, -1,	userPrompt,	text823,	CFLit_prompt},
		// Returns response read from keyboard.
	{"push", CFFunc, ArgArray1 | ArgBool2 | ArgArray2 | ArgNIS2, 2, 2, NULL,	text824,	CFLit_push},
		// Returns new array value.
	{"queryReplace",	CFEdit, ArgNotNull1 | ArgNil2,	2, 2,	NULL,		text810,	CFLit_queryReplace},
		// Returns false if search stopped prematurely by user; otherwise, true.
	{"quickExit",		0, 0,				0, 0,	NULL,		NULL,		CFLit_quickExit},
	{"quote",	CFFunc, ArgBool1 | ArgArray1 | ArgNIS1,	1, 1,	NULL,		text812,	CFLit_quote},
		// Returns quoted expression.
	{"quoteChar",		CFBind1 | CFUniq | CFEdit, 0,	0, 0,	quoteChar,	NULL,		CFLit_quoteChar},
	{"rand",		CFFunc, ArgInt1,		1, 1,	NULL,		text820,	CFLit_rand},
		// Returns pseudo-random integer.
	{"readFile",		CFNoLoad, 0,			1, 1,	readFile,	text868,	CFLit_readFile},
		// Returns name of buffer.
	{"readPipe",		CFNoLoad, 0,			1, -1,	NULL,		text809,	CFLit_readPipe},
		// Returns name of buffer, or false if failure.
	{"reframeWind",		0, 0,				0, 0,	NULL,		NULL,		CFLit_reframeWind},
	{"renameBuf",		CFNoLoad, 0,			2, 2,	renameBuf,	text849,	CFLit_renameBuf},
		// Returns name of new buffer.
	{"renameMacro",		CFNoLoad, 0,			2, 2,	renameMacro,	text849,	CFLit_renameMacro},
		// Returns name of new macro.
	{"replace",		CFEdit, ArgNotNull1 | ArgNil2,	2, 2,	NULL,		text810,	CFLit_replace},
		// Returns false if fewer than n replacements were made; otherwise, true.
	{"replaceText",	CFFunc | CFEdit | CFShortLoad, 0,	1, -1,	NULL,		text809,	CFLit_replaceText},
		// Returns new text.
	{"resetTerm",		0, 0,				0, 0,	resetTerm,	NULL,		CFLit_resetTerm},
	{"resizeWind",		0, 0,				0, 0,	resizeWind,	NULL,		CFLit_resizeWind},
	{"restoreBuf",		CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_restoreBuf},
		// Returns name of buffer.
	{"restoreScreen",	CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_restoreScreen},
	{"restoreWind",		CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_restoreWind},
	{"revertYank",		0, 0,				0, 0,	NULL,		NULL,		CFLit_revertYank},
	{"ringSize",		CFNoLoad, 0,			1, 2,	ringSize,	text875,	CFLit_ringSize},
		// Returns array of form [count, size].
	{"run",		CFPerm | CFSpecArgs | CFNoLoad, 0,	1, 1,	run,		text803,	CFLit_run},
		// Returns execution result.
	{"saveBuf",		CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_saveBuf},
		// Returns name of buffer.
	{"saveFile",		0, 0,				0, 0,	NULL,		NULL,		CFLit_saveFile},
	{"saveScreen",		CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_saveScreen},
	{"saveWind",		CFFunc, 0,			0, 0,	NULL,		NULL,		CFLit_saveWind},
	{"scratchBuf",		0, 0,				0, 0,	scratchBuf,	NULL,		CFLit_scratchBuf},
		// Returns name of buffer.
	{"searchBack",		0, ArgNotNull1,			1, 1,	searchBack,	text805,	CFLit_searchBack},
		// Returns string found, or false if not found.
	{"searchForw",		0, ArgNotNull1,			1, 1,	searchForw,	text805,	CFLit_searchForw},
		// Returns string found, or false if not found.
	{"selectBuf",		CFNoLoad, 0,			1, 1,	selectBuf,	text867,	CFLit_selectBuf},
		// Returns name of buffer.
	{"selectLine",		CFFunc, ArgInt1,		1, 1,	selectLine,	text820,	CFLit_selectLine},
		// Returns number of lines selected.
	{"selectScreen",	CFNoLoad, 0,			1, 1,	selectScreen,	text820,	CFLit_selectScreen},
	{"selectWind",		CFNoLoad, 0,			1, 1,	selectWind,	text820,	CFLit_selectWind},
	{"setBufFile",		CFNoLoad, 0,			2, 2,	setBufFile,	text850,	CFLit_setBufFile},
		// Returns two-element array containing new buffer name and new filename.
	{"setColorPair", CFFunc, ArgInt1 | ArgInt2 | ArgInt3,	3, 3,	setColorPair,	text864,	CFLit_setColorPair},
		// Returns color pair number.
	{"setDispColor", CFFunc, ArgNotNull1 | ArgArray2 | ArgNil2 | ArgMay,
								2, 2,	setDispColor,	text865,	CFLit_setDispColor},
	{"setHook", CFFunc | CFSpecArgs | CFShortLoad, ArgNotNull1,
								2, 2,	setHook,	text832,	CFLit_setHook},
	{"setMark",		CFNoLoad, 0,			0, 1,	setMark,	text851,	CFLit_setMark},
	{"setWrapCol",		CFNoLoad, 0,			0, 1,	NULL,		text821,	CFLit_setWrapCol},
	{"seti",		0, ArgInt1 | ArgInt3,		1, 3,	seti,		text813,	CFLit_seti},
	{"shQuote",		CFFunc, ArgNIS1,		1, 1,	NULL,		text812,	CFLit_shQuote},
		// Returns quoted string.
	{"shell",		0, 0,				0, 0,	shellCLI,	NULL,		CFLit_shell},
		// Returns false if failure.
	{"shellCmd",		CFNoLoad, 0,			1, -1,	NULL,		text861,	CFLit_shellCmd},
		// Returns false if failure.
	{"shift",		CFFunc, ArgArray1,		1, 1,	NULL,		text822,	CFLit_shift},
		// Returns shifted value, or nil if none left.
	{"showAliases",		0, ArgNil1,			1, 1,	showAliases,	text805,	CFLit_showAliases},
	{"showBuffers",		CFAddlArg, ArgNotNull1,		0, 1,	showBuffers,	text844,	CFLit_showBuffers},
	{"showColors",		0, 0,				0, 0,	showColors,	NULL,		CFLit_showColors},
	{"showCommands",	CFAddlArg, ArgNil1,		1, 2,	showCommands,	text873,	CFLit_showCommands},
	{"showDir",		0, 0,				0, 0,	NULL,		NULL,		CFLit_showDir},
		// Returns absolute pathname of current directory.
	{"showFence",		CFAddlArg, ArgInt1,		0, 1,	showFence,	text857,	CFLit_showFence},
	{"showFunctions",	CFAddlArg, ArgNil1,		1, 2,	showFunctions,	text873,	CFLit_showFunctions},
	{"showHooks",		0, 0,				0, 0,	showHooks,	NULL,		CFLit_showHooks},
	{"showKey",		0, ArgNotNull1,			1, 1,	showKey,	text815,	CFLit_showKey},
	{"showMarks",		0, 0,				0, 0,	showMarks,	NULL,		CFLit_showMarks},
	{"showModes",		0, 0,				0, 0,	showModes,	NULL,		CFLit_showModes},
	{"showPoint",		CFTerm, 0,			0, 0,	showPoint,	NULL,		CFLit_showPoint},
#if MMDebug & Debug_ShowRE
	{"showRegexp",		0, 0,				0, 0,	showRegexp,	NULL,		CFLit_showRegexp},
#endif
	{"showRing",		CFNoLoad, 0,			1, 1,	showRing,	text874,	CFLit_showRing},
	{"showScreens",		0, 0,				0, 0,	showScreens,	NULL,		CFLit_showScreens},
	{"showVariables",	0, ArgNil1,			1, 1,	showVariables,	text805,	CFLit_showVariables},
	{"shrinkWind",		CFNCount, 0,			0, 0,	NULL,		NULL,		CFLit_shrinkWind},
	{"sortRegion",	CFEdit | CFAddlArg, ArgNotNull1,	0, 1,	sortRegion,	text844,	CFLit_sortRegion},
	{"space",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_space},
	{"split",		CFFunc, ArgInt1 | ArgNil1 | ArgInt3,
								2, 3,	strSplit,	text838,	CFLit_split},
		// Returns array.
	{"splitWind",		0, 0,				0, 0,	NULL,		NULL,		CFLit_splitWind},
		// Returns ordinal number of new window not containing point.
	{"sprintf",		CFFunc, 0,			1, -1,	NULL,		text831,	CFLit_sprintf},
	{"stat?",		CFFunc, ArgPath,		2, 2,	NULL,		text835,	CFLit_statQ},
		// Returns Boolean result.
	{"strFit",		CFFunc, ArgInt2,		2, 2,	NULL,		text827,	CFLit_strFit},
		// Returns compressed string.
	{"strPop",	CFFunc | CFSpecArgs | CFNoLoad, 0,	2, 2,	NULL,		text839,	CFLit_strPop},
		// Returns popped value, or nil if none left.
	{"strPush",	CFFunc | CFSpecArgs | CFNoLoad, 0,	3, 3,	NULL,		text840,	CFLit_strPush},
		// Returns pushed value.
	{"strShift",	CFFunc | CFSpecArgs | CFNoLoad, 0,	2, 2,	NULL,		text839,	CFLit_strShift},
		// Returns shifted value, or nil if none left.
	{"strUnshift",	CFFunc | CFSpecArgs | CFNoLoad, 0,	3, 3,	NULL,		text840,	CFLit_strUnshift},
		// Returns unshifted value.
	{"strip",		CFFunc, 0,			1, 1,	NULL,		text811,	CFLit_strip},
		// Returns whitespace-stripped string.
	{"sub",			CFFunc, ArgNil3,		3, 3,	substitute,	text817,	CFLit_sub},
		// Returns string result.
	{"subline",		CFFunc, ArgInt1 | ArgInt2,	1, 2,	NULL,		text825,	CFLit_subline},
		// Returns string result.
	{"substr",		CFFunc, ArgInt2 | ArgInt3,	2, 3,	NULL,		text826,	CFLit_substr},
		// Returns string result.
	{"suspend",		0, 0,				0, 0,	suspendMM,	NULL,		CFLit_suspend},
	{"swapMark",		CFNoLoad, 0,			0, 1,	swapMark,	text834,	CFLit_swapMark},
	{"tab",			CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_tab},
	{"titleCaseLine",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_titleCaseLine},
	{"titleCaseRegion",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_titleCaseRegion},
	{"titleCaseStr",	CFFunc, 0,			1, 1,	titleCaseStr,	text811,	CFLit_titleCaseStr},
		// Returns string result.
	{"titleCaseWord",	CFEdit | CFNCount, 0,		0, 0,	NULL,		NULL,		CFLit_titleCaseWord},
	{"toInt",		CFFunc, ArgInt1 | ArgMay,	1, 1,	NULL,		text811,	CFLit_toInt},
		// Returns integer result.
	{"toStr", CFFunc | CFAddlArg, ArgBool1 | ArgArray1 | ArgNIS1 | ArgNotNull2,
								1, 2,	toString,	text841,	CFLit_toStr},
		// Returns string result.
	{"tr",			CFFunc, ArgNil3,		3, 3,	NULL,		text817,	CFLit_tr},
		// Returns translated string.
	{"traverseLine",	0, 0,				0, 0,	traverseLine,	NULL,		CFLit_traverseLine},
	{"trimLine",		CFEdit, 0,			0, 0,	trimLine,	NULL,		CFLit_trimLine},
	{"truncBuf",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_truncBuf},
		// Returns name of buffer.
	{"type?",	CFFunc, ArgBool1 | ArgArray1 | ArgNIS1,	1, 1,	NULL,		text812,	CFLit_typeQ},
		// Returns type of value.
	{"unbindKey",		0, ArgNotNull1,			1, 1,	unbindKey,	text815,	CFLit_unbindKey},
		// Returns Boolean result if script mode; otherwise, nil.
	{"undelete",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_undelete},
	{"undeleteCycle",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_undeleteCycle},
	{"universalArg", CFHidden | CFBind1 | CFUniq, 0,	0, 0,	NULL,		NULL,		CFLit_universalArg},
	{"unshift", CFFunc, ArgArray1 | ArgBool2 | ArgArray2 | ArgNIS2,
								2, 2,	NULL,		text824,	CFLit_unshift},
		// Returns new array value.
	{"updateScreen", CFFunc | CFAddlArg, ArgNotNull1,	0, 1,	updateScrn,	text844,	CFLit_updateScreen},
	{"upperCaseLine",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_upperCaseLine},
	{"upperCaseRegion",	CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_upperCaseRegion},
	{"upperCaseStr",	CFFunc, 0,			1, 1,	NULL,		text811,	CFLit_upperCaseStr},
		// Returns string result.
	{"upperCaseWord",	CFEdit | CFNCount, 0,		0, 0,	NULL,		NULL,		CFLit_upperCaseWord},
	{"viewFile",		CFNoLoad, 0,			1, 1,	NULL,		text868,	CFLit_viewFile},
		// Returns [name of buffer, "true" or "false" indicating whether the buffer was created].
	{"widenBuf",		0, 0,				0, 0,	widenBuf,	NULL,		CFLit_widenBuf},
	{"wordChar?",		CFFunc, ArgInt1,		1, 1,	NULL,		text860,	CFLit_wordCharQ},
		// Returns true if a character is a word character.
	{"wrapLine",		CFEdit, ArgNil1 | ArgNil2,	2, 2,	wrapLine,	text830,	CFLit_wrapLine},
	{"wrapWord",		CFFunc | CFHook | CFEdit, 0,	0, 0,	wrapWord,	NULL,		CFLit_wrapWord},
	{"writeBuf",		CFNoLoad, 0,			1, 1,	writeBuf,	text867,	CFLit_writeBuf},
		// Returns text copied.
	{"writeFile",		CFNoLoad, 0,			1, 1,	NULL,		text868,	CFLit_writeFile},
		// Returns filename.
	{"xPathname", CFFunc | CFAddlArg, ArgPath | ArgNotNull2, 1, 2,	xPathname,	text859,	CFLit_xPathname},
		// Returns pathname, or array of pathnames.
	{"xeqBuf",		CFNoLoad, 0,			1, -1,	xeqBuf,		text816,	CFLit_xeqBuf},
		// Returns execution result.
	{"xeqFile",		CFNoLoad, 0,			1, -1,	xeqFile,	text869,	CFLit_xeqFile},
		// Returns execution result.
	{"xeqMacro",		CFNoLoad, 0,			0, 1,	xeqMacro,	text871,	CFLit_xeqMacro},
	{"yank",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_yank},
	{"yankCycle",		CFEdit, 0,			0, 0,	NULL,		NULL,		CFLit_yankCycle},
	{NULL,			0, 0,				0, 0,	NULL,		NULL,		NULL}
	};
Hash *execTable;			// Table of executable names (commands, functions, and aliases).

#define CmdFuncCount	(sizeof(cmdFuncTable) / sizeof(CmdFunc)) - 1

#else

// **** For all the other .c files ****

// External variable declarations.
extern Alias *ahead;
extern CmdFunc cmdFuncTable[];
extern Hash *execTable;
#endif
