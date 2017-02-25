// (c) Copyright 2017 Richard W. Marinelli
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

// External function declarations.
extern int adjustmode(Datum *rp,int n,int type,Datum *datp);
extern int afind(char *aname,int op,CFABPtr *cfabp,Alias **app);
extern int aliasCFM(Datum *rp,int n,Datum **argpp);
extern int amfind(char *name,short op,uint type);
extern int backChar(Datum *rp,int n,Datum **argpp);
extern int backLine(Datum *rp,int n,Datum **argpp);
extern int backPage(Datum *rp,int n,Datum **argpp);
extern int backWord(Datum *rp,int n,Datum **argpp);
extern int beginKeyMacro(Datum *rp,int n,Datum **argpp);
extern int beginText(Datum *rp,int n,Datum **argpp);
extern int caseline(int n,char *trantab);
extern int cfabsearch(char *str,CFABPtr *cfabp,uint selector);
extern int deleteAlias(Datum *rp,int n,Datum **argpp);
extern int deleteAM(char *prmt,uint selector,char *emsg);
extern int deleteBlankLines(Datum *rp,int n,Datum **argpp);
extern int deleteMacro(Datum *rp,int n,Datum **argpp);
extern int deltab(int n);
extern int delwhite(int n);
extern int detabLine(Datum *rp,int n,Datum **argpp);
extern int dupLine(Datum *rp,int n,Datum **argpp);
extern int endKeyMacro(Datum *rp,int n,Datum **argpp);
extern int endWord(Datum *rp,int n,Datum **argpp);
extern int entabLine(Datum *rp,int n,Datum **argpp);
extern CmdFunc *ffind(char *cname);
extern int forwChar(Datum *rp,int n,Datum **argpp);
extern int forwLine(Datum *rp,int n,Datum **argpp);
extern int forwPage(Datum *rp,int n,Datum **argpp);
extern int forwWord(Datum *rp,int n,Datum **argpp);
extern int getcfam(char *prmt,uint selector,CFABPtr *cfabp,char *emsg0,char *emsg1);
extern int getcfm(char *prmt,CFABPtr *cfabp,uint selector);
extern int goline(Datum *datp,int n,int line);
extern int gotoLine(Datum *rp,int n,Datum **argpp);
extern int gotoMark(Datum *rp,int n,Datum **argpp);
extern int help(Datum *rp,int n,Datum **argpp);
extern int inserti(Datum *rp,int n,Datum **argpp);
extern int insertLineI(Datum *rp,int n,Datum **argpp);
extern int insnlspace(Datum *rp,int n,bool nl);
extern int inspre(int c);
extern int insrfence(int c);
extern int instab(int n);
extern int joinLines(Datum *rp,int n,Datum **argpp);
extern int kdcbword(int n,int kdc);
extern bool kdcfencedreg(int kdc);
extern int kdcfword(int n,int kdc);
extern int kdcline(int n,int kdc);
extern int lcWord(Datum *rp,int n,Datum **argpp);
extern int newlineI(Datum *rp,int n,Datum **argpp);
extern int notice(Datum *rp,int n,Datum **argpp);
extern int openLine(Datum *rp,int n,Datum **argpp);
extern int quit(Datum *rp,int n,Datum **argpp);
extern int quoteChar(Datum *rp,int n,Datum **argpp);
extern int shellCLI(Datum *rp,int n,Datum **argpp);
extern int shellCmd(Datum *rp,int n,Datum **argpp);
extern int showFunctions(Datum *rp,int n,Datum **argpp);
extern int showModes(Datum *rp,int n,Datum **argpp);
extern int suspendEMacs(Datum *rp,int n,Datum **argpp);
extern int tcWord(Datum *rp,int n,Datum **argpp);
extern int traverseLine(Datum *rp,int n,Datum **argpp);
extern int trimLine(Datum *rp,int n,Datum **argpp);
extern int ucWord(Datum *rp,int n,Datum **argpp);
extern int whence(Datum *rp,int n,Datum **argpp);
extern int wrapLine(Datum *rp,int n,Datum **argpp);
extern int wrapWord(Datum *rp,int n,Datum **argpp);
extern int writeBuf(Datum *rp,int n,Datum **argpp);
extern int xeqKeyMacro(Datum *rp,int n,Datum **argpp);

#ifdef DATAcmd

// **** For main.c ****

// Global variables.
Alias *aheadp = NULL;			// Head of alias list.
CmdFunc cftab[] = {
	{"abort",		CFBind1 | CFUniq,0,		0,-1,	abortOp,	Literal1,	FLit_abort},
	{"about",		0,0,				0,0,	aboutMM,	NULL,		FLit_about},
	{"abs",			CFFunc,CFInt1,			1,1,	NULL,		Literal21,	FLit_abs},
	{"alias",		CFSpecArgs | CFNoLoad,0,	2,2,	aliasCFM,	Literal2,	FLit_alias},
	{"alterBufMode",	CFNoLoad,0,			1,-1,	NULL,		Literal3,	FLit_alterBufMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterDefMode",	CFNoLoad,0,			1,-1,	NULL,		Literal3,	FLit_alterDefMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterGlobalMode",	CFNoLoad,0,			1,-1,	NULL,		Literal3,	FLit_alterGlobalMode},
		// Returns state (-1 or 1) or last mode altered.
	{"alterShowMode",	CFNoLoad,0,			1,-1,	NULL,		Literal3,	FLit_alterShowMode},
		// Returns state (-1 or 1) or last mode altered.
	{"appendFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	FLit_appendFile},
		// Returns filename.
	{"array",CFFunc,CFInt1 | CFBool2 | CFArray2 | CFNIS2,	0,2,	array,		Literal29,	FLit_array},
		// Returns new array.
	{"backChar",		CFNCount,0,			0,0,	backChar,	NULL,		FLit_backChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backLine",		CFNCount,0,			0,0,	backLine,	NULL,		FLit_backLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"backPage",		CFNCount,0,			0,0,	backPage,	NULL,		FLit_backPage},
	{"backPageNext",	CFNCount,0,			0,0,	NULL,		NULL,		FLit_backPageNext},
	{"backPagePrev",	CFNCount,0,			0,0,	NULL,		NULL,		FLit_backPagePrev},
	{"backTab",		CFNCount,0,			0,0,	NULL,		NULL,		FLit_backTab},
	{"backWord",		CFNCount,0,			0,0,	backWord,	NULL,		FLit_backWord},
	{"basename",		CFFunc,0,			1,1,	NULL,		Literal15,	FLit_basename},
		// Returns filename component of pathname.
	{"beep",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_beep},
	{"beginBuf",		CFAddlArg | CFNoLoad,0,		0,1,	NULL,		Literal9,	FLit_beginBuf},
	{"beginKeyMacro",	0,0,				0,0,	beginKeyMacro,	NULL,		FLit_beginKeyMacro},
	{"beginLine",		0,0,				0,0,	NULL,		NULL,		FLit_beginLine},
	{"beginText",		0,0,				0,0,	beginText,	NULL,		FLit_beginText},
	{"beginWhite",		0,0,				0,0,	NULL,		NULL,		FLit_beginWhite},
	{"bindKey",	CFSpecArgs | CFShrtLoad,CFNotNull1,	2,2,	bindKeyCM,	Literal7,	FLit_bindKey},
	{"binding",		CFFunc,0,			1,1,	NULL,		Literal12,	FLit_binding},
		// Returns name of command or macro key is bound to, or nil if none.
	{"bufBound?",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_bufBoundQ},
		// Returns true if dot is at beginning, middle, or end of buffer per n argument.
	{"bufList",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_bufList},
		// Returns buffer list (array).
	{"bufSize",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_bufSize},
		// Returns buffer size in lines or bytes.
	{"chDir",		0,0,				1,1,	changedir,	Literal38,	FLit_chDir},
		// Returns absolute pathname of new directory.
	{"chr",			CFFunc,CFInt1,			1,1,	NULL,		Literal21,	FLit_chr},
		// Returns ordinal value of a character in string form.
	{"clearBuf",		CFNoLoad,0,			0,1,	clearBuf,	Literal9,	FLit_clearBuf},
		// Returns false if buffer is not cleared; otherwise, true.
	{"clearKillRing",	0,0,				0,0,	NULL,		NULL,		FLit_clearKillRing},
	{"clearMsg",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_clearMsg},
	{"clone",		CFFunc,CFArray1,		1,1,	NULL,		Literal23,	FLit_clone},
		// Returns new array.
	{"copyFencedText",	0,0,				0,0,	NULL,		NULL,		FLit_copyFencedText},
	{"copyLine",		0,0,				0,0,	NULL,		NULL,		FLit_copyLine},
	{"copyRegion",		0,0,				0,0,	NULL,		NULL,		FLit_copyRegion},
	{"copyToBreak",		0,0,				0,0,	NULL,		NULL,		FLit_copyToBreak},
	{"copyWord",		0,0,				0,0,	NULL,		NULL,		FLit_copyWord},
#if WordCount
	{"countWords",		CFTerm,0,			0,0,	countWords,	NULL,		FLit_countWords},
#endif
	{"cycleKillRing",	0,0,				0,0,	NULL,		NULL,		FLit_cycleKillRing},
	{"defined?",		CFFunc,0,			1,1,	NULL,		Literal4,	FLit_definedQ},
		// Returns kind of object, or nil if not found.
	{"deleteAlias",		CFSpecArgs | CFNoLoad,0,	1,-1,	deleteAlias,	Literal8,	FLit_deleteAlias},
	{"deleteBackChar",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		FLit_deleteBackChar},
	{"deleteBackTab",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		FLit_deleteBackTab},
	{"deleteBlankLines",	CFEdit,0,			0,0,	deleteBlankLines,NULL,		FLit_deleteBlankLines},
	{"deleteBuf",		0,0,				0,-1,	deleteBuf,	Literal37,	FLit_deleteBuf},
		// Returns name of last buffer deleted.
	{"deleteFencedText",	CFEdit,0,			0,0,	NULL,		NULL,		FLit_deleteFencedText},
	{"deleteForwChar",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		FLit_deleteForwChar},
	{"deleteForwTab",	CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		FLit_deleteForwTab},
	{"deleteLine",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_deleteLine},
	{"deleteMacro",		CFSpecArgs | CFNoLoad,0,	1,-1,	deleteMacro,	Literal8,	FLit_deleteMacro},
	{"deleteMark",		CFNoLoad | CFNoArgs,0,		1,1,	deleteMark,	Literal22,	FLit_deleteMark},
	{"deleteRegion",	CFEdit,0,			0,0,	NULL,		NULL,		FLit_deleteRegion},
	{"deleteScreen",	0,0,				0,0,	deleteScreen,	NULL,		FLit_deleteScreen},
	{"deleteToBreak",	CFEdit,0,			0,0,	NULL,		NULL,		FLit_deleteToBreak},
	{"deleteWhite",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_deleteWhite},
	{"deleteWind",		0,0,				0,0,	deleteWind,	NULL,		FLit_deleteWind},
	{"deleteWord",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_deleteWord},
	{"detabLine",		CFEdit,0,			0,0,	detabLine,	NULL,		FLit_detabLine},
	{"dirname",		CFFunc,0,			1,1,	NULL,		Literal15,	FLit_dirname},
		// Returns directory component of pathname.
	{"dupLine",		CFEdit,0,			0,0,	dupLine,	NULL,		FLit_dupLine},
	{"empty?",	CFFunc,CFNil1 | CFArray1 | CFMay,	1,1,	NULL,		Literal13,	FLit_emptyQ},
		// Returns true if nil value, null string, or empty array.
	{"endBuf",		CFAddlArg | CFNoLoad,0,		0,1,	NULL,		Literal9,	FLit_endBuf},
	{"endKeyMacro",		0,0,				0,0,	endKeyMacro,	NULL,		FLit_endKeyMacro},
	{"endLine",		0,0,				0,0,	NULL,		NULL,		FLit_endLine},
	{"endWhite",		0,0,				0,0,	NULL,		NULL,		FLit_endWhite},
	{"endWord",		CFNCount,0,			0,0,	endWord,	NULL,		FLit_endWord},
		// Returns false if hit buffer boundary; otherwise, true.
	{"entabLine",		CFEdit,0,			0,0,	entabLine,	NULL,		FLit_entabLine},
	{"env",			CFFunc,0,			1,1,	NULL,		Literal4,	FLit_env},
		// Returns value of environmental variable, or nil if not found.
	{"eval",		CFNoLoad,0,			1,-1,	eval,		Literal10,	FLit_eval},
		// Returns result of evaluation.
	{"exit",		0,0,				0,-1,	quit,		Literal1,	FLit_exit},
	{"findFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	FLit_findFile},
		// Returns [name of buffer, "true" or "false" indicating whether the buffer was created].
	{"forwChar",		CFNCount,0,			0,0,	forwChar,	NULL,		FLit_forwChar},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwLine",		CFNCount,0,			0,0,	forwLine,	NULL,		FLit_forwLine},
		// Returns false if hit buffer boundary; otherwise, true.
	{"forwPage",		CFNCount,0,			0,0,	forwPage,	NULL,		FLit_forwPage},
	{"forwPageNext",	CFNCount,0,			0,0,	NULL,		NULL,		FLit_forwPageNext},
	{"forwPagePrev",	CFNCount,0,			0,0,	NULL,		NULL,		FLit_forwPagePrev},
	{"forwTab",		CFNCount,0,			0,0,	NULL,		NULL,		FLit_forwTab},
	{"forwWord",		CFNCount,0,			0,0,	forwWord,	NULL,		FLit_forwWord},
	{"getKey",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_getKey},
		// Returns key in encoded form.
	{"gotoFence",		0,0,				0,0,	NULL,		NULL,		FLit_gotoFence},
	{"gotoLine",		CFAddlArg | CFNoLoad,0,		1,2,	gotoLine,	Literal34,	FLit_gotoLine},
	{"gotoMark",		CFNoLoad,0,			1,1,	gotoMark,	Literal35,	FLit_gotoMark},
	{"growWind",		CFNCount,0,			0,0,	NULL,		NULL,		FLit_growWind},
	{"help",		CFTerm,0,			0,0,	help,		NULL,		FLit_help},
	{"hideBuf",		CFAddlArg | CFNoLoad,0,		0,1,	NULL,		Literal9,	FLit_hideBuf},
		// Returns name of buffer.
	{"huntBack",		CFNCount,0,			0,0,	huntBack,	NULL,		FLit_huntBack},
		// Returns string found, or false if not found.
	{"huntForw",		CFNCount,0,			0,0,	huntForw,	NULL,		FLit_huntForw},
		// Returns string found, or false if not found.
	{"include?",CFFunc,CFArray1 | CFArray2 | CFBool2 | CFNIS2,2,2,	NULL,		Literal25,	FLit_includeQ},
		// Returns true if value is in given array.
	{"indentRegion",	CFEdit | CFNCount,0,		0,0,	indentRegion,	NULL,		FLit_indentRegion},
	{"index",		CFFunc,0,			2,2,	NULL,		Literal19,	FLit_index},
		// Returns string position, or nil if not found.
	{"insert",		CFFunc | CFEdit | CFShrtLoad,0,	1,-1,	NULL,		Literal10,	FLit_insert},
		// Returns text inserted.
	{"insertBuf",		CFEdit | CFNoLoad,0,		1,1,	insertBuf,	Literal4,	FLit_insertBuf},
		// Returns name of buffer.
	{"insertFile",		CFEdit | CFNoLoad,CFNotNull1,	1,1,	insertFile,	Literal4,	FLit_insertFile},
		// Returns filename.
	{"insertLineI",		CFEdit | CFNCount,0,		0,0,	insertLineI,	NULL,		FLit_insertLineI},
	{"insertPipe",		CFEdit | CFNoLoad,0,		1,-1,	insertPipe,	Literal10,	FLit_insertPipe},
		// Returns false if failure.
	{"insertSpace",		CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		FLit_insertSpace},
	{"inserti",		CFEdit | CFNCount,0,		0,0,	inserti,	NULL,		FLit_inserti},
	{"join",		CFFunc | CFShrtLoad,CFNil1,	2,-1,	NULL,		Literal20,	FLit_join},
		// Returns string result.
	{"joinLines",		CFEdit,CFNil1,			1,1,	joinLines,	Literal30,	FLit_joinLines},
	{"joinWind",		0,0,				0,0,	joinWind,	NULL,		FLit_joinWind},
	{"kill",		CFFunc,CFInt1,			1,1,	NULL,		Literal21,	FLit_kill},
	{"killFencedText",	CFEdit,0,			0,0,	NULL,		NULL,		FLit_killFencedText},
	{"killLine",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_killLine},
	{"killRegion",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_killRegion},
	{"killToBreak",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_killToBreak},
	{"killWord",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_killWord},
	{"lastBuf",		0,0,				0,0,	NULL,		NULL,		FLit_lastBuf},
		// Returns name of buffer.
	{"lcLine",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_lcLine},
	{"lcRegion",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_lcRegion},
	{"lcString",		CFFunc,0,			1,1,	NULL,		Literal12,	FLit_lcString},
		// Returns string result.
	{"lcWord",		CFEdit | CFNCount,0,		0,0,	lcWord,		NULL,		FLit_lcWord},
	{"length",		CFFunc,CFArray1 | CFMay,	1,1,	NULL,		Literal13,	FLit_length},
		// Returns string or array length.
	{"let",			CFTerm,0,			0,0,	setvar,		NULL,		FLit_let},
	{"markBuf",		CFNoLoad,0,			0,1,	markBuf,	Literal22,	FLit_markBuf},
	{"match",		CFFunc,CFInt1,			1,1,	NULL,		Literal21,	FLit_match},
		// Returns value of pattern match, or null if none.
	{"metaPrefix",CFHidden | CFPrefix | CFBind1 | CFPerm,0,	0,0,	NULL,		NULL,		FLit_metaPrefix},
	{"moveWindDown",	CFNCount,0,			0,0,	NULL,		NULL,		FLit_moveWindDown},
	{"moveWindUp",		CFNCount,0,			0,0,	moveWindUp,	NULL,		FLit_moveWindUp},
	{"narrowBuf",		0,0,				0,0,	narrowBuf,	NULL,		FLit_narrowBuf},
	{"negativeArg",		CFHidden | CFBind1 | CFUniq,0,	0,0,	NULL,		NULL,		FLit_negativeArg},
	{"newScreen",		0,0,				0,0,	newScreen,	NULL,		FLit_newScreen},
		// Returns new screen number.
	{"newline",		CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		FLit_newline},
	{"newlineI",		CFEdit | CFNCount,0,		0,0,	newlineI,	NULL,		FLit_newlineI},
	{"nextBuf",		0,0,				0,0,	NULL,		NULL,		FLit_nextBuf},
		// Returns name of buffer.
	{"nextScreen",		0,0,				0,0,	nextScreen,	NULL,		FLit_nextScreen},
		// Returns new screen number.
	{"nextWind",		0,0,				0,0,	nextWind,	NULL,		FLit_nextWind},
	{"nil?",	CFFunc,CFBool1 | CFArray1 | CFNIS1,	1,1,	NULL,		Literal13,	FLit_nilQ},
		// Returns true if nil value.
	{"notice",		CFFunc | CFNoLoad,CFNil1,	1,-1,	notice,		Literal10,	FLit_notice},
		// Returns true if default n or n >= 0; otherwise, false.
	{"null?",		CFFunc,0,			1,1,	NULL,		Literal12,	FLit_nullQ},
		// Returns true if null string.
	{"numeric?",		CFFunc,0,			1,1,	NULL,		Literal12,	FLit_numericQ},
		// Returns true if numeric literal.
	{"onlyWind",		0,0,				0,0,	onlyWind,	NULL,		FLit_onlyWind},
	{"openLine",		CFEdit | CFNCount,0,		0,0,	openLine,	NULL,		FLit_openLine},
	{"ord",			CFFunc,0,			1,1,	NULL,		Literal12,	FLit_ord},
		// Returns ordinal value of first character of a string.
	{"outdentRegion",	CFEdit | CFNCount,0,		0,0,	outdentRegion,	NULL,		FLit_outdentRegion},
	{"overwrite",		CFFunc | CFEdit | CFShrtLoad,0,	1,-1,	NULL,		Literal10,	FLit_overwrite},
		// Returns new text.
	{"pathname",		CFFunc,0,			1,1,	NULL,		Literal15,	FLit_pathname},
		// Returns absolute pathname of a file or directory.
	{"pause",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_pause},
	{"pipeBuf",		CFEdit | CFNoLoad,0,		1,-1,	pipeBuf,	Literal10,	FLit_pipeBuf},
		// Returns false if failure.
	{"pop",			CFFunc,CFArray1,		1,1,	NULL,		Literal23,	FLit_pop},
		// Returns popped value, or nil if none left.
	{"prefix1",CFHidden | CFPrefix | CFBind1 | CFPerm,0,	0,0,	NULL,		NULL,		FLit_prefix1},
	{"prefix2",CFHidden | CFPrefix | CFBind1 | CFPerm,0,	0,0,	NULL,		NULL,		FLit_prefix2},
	{"prefix3",CFHidden | CFPrefix | CFBind1 | CFPerm,0,	0,0,	NULL,		NULL,		FLit_prefix3},
	{"prevBuf",		0,0,				0,0,	NULL,		NULL,		FLit_prevBuf},
		// Returns name of buffer.
	{"prevScreen",		0,0,				0,0,	NULL,		NULL,		FLit_prevScreen},
		// Returns new screen number.
	{"prevWind",		0,0,				0,0,	prevWind,	NULL,		FLit_prevWind},
	{"print",		CFFunc | CFShrtLoad,0,		1,-1,	NULL,		Literal10,	FLit_print},
	{"prompt",		CFFunc | CFSpecArgs,0,		1,4,	NULL,		Literal24,	FLit_prompt},
		// Returns response read from keyboard.
	{"push",CFFunc,CFArray1 | CFBool2 | CFArray2 | CFNIS2,	2,2,	NULL,		Literal25,	FLit_push},
		// Returns new array value.
	{"queryReplace",CFEdit | CFNCount,CFNotNull1 | CFNil2,	2,2,	NULL,		Literal11,	FLit_queryReplace},
		// Returns false if search stopped prematurely; otherwise, true.
	{"quickExit",		0,0,				0,0,	NULL,		NULL,		FLit_quickExit},
	{"quote",	CFFunc,CFBool1 | CFArray1 | CFNIS1,	1,1,	NULL,		Literal13,	FLit_quote},
		// Returns quoted value.
	{"quoteChar",	CFBind1 | CFUniq | CFEdit | CFNCount,0,	0,0,	quoteChar,	NULL,		FLit_quoteChar},
	{"rand",		CFFunc,CFInt1,			1,1,	NULL,		Literal21,	FLit_rand},
		// Returns pseudo-random integer.
	{"readBuf",		CFFunc | CFNCount | CFNoLoad,0,	1,1,	readBuf,	Literal4,	FLit_readBuf},
		// Returns nth next line from buffer.
	{"readFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	FLit_readFile},
		// Returns name of buffer.
	{"readPipe",		CFNoLoad,0,			1,-1,	readPipe,	Literal10,	FLit_readPipe},
		// Returns name of buffer, or false if failure.
	{"redrawScreen",	0,0,				0,0,	NULL,		NULL,		FLit_redrawScreen},
	{"replace",	CFEdit | CFNCount,CFNotNull1 | CFNil2,	2,2,	NULL,		Literal11,	FLit_replace},
	{"replaceText",		CFFunc | CFEdit | CFShrtLoad,0,	1,-1,	NULL,		Literal10,	FLit_replaceText},
		// Returns new text.
	{"resetTerm",		0,0,				0,0,	resetTermc,	NULL,		FLit_resetTerm},
	{"resizeWind",		0,0,				0,0,	resizeWind,	NULL,		FLit_resizeWind},
	{"restoreBuf",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_restoreBuf},
		// Returns name of buffer.
	{"restoreWind",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_restoreWind},
	{"run",		CFPerm | CFSpecArgs | CFNoLoad,0,	1,1,	run,		Literal4,	FLit_run},
		// Returns execution result.
	{"saveBuf",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_saveBuf},
		// Returns name of buffer.
	{"saveFile",		0,0,				0,0,	NULL,		NULL,		FLit_saveFile},
	{"saveWind",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_saveWind},
	{"scratchBuf",		0,0,				0,0,	scratchBuf,	NULL,		FLit_scratchBuf},
		// Returns name of buffer.
	{"searchBack",		CFNCount,CFNotNull1,		1,1,	searchBack,	Literal12,	FLit_searchBack},
		// Returns string found, or false if not found.
	{"searchForw",		CFNCount,CFNotNull1,		1,1,	searchForw,	Literal12,	FLit_searchForw},
		// Returns string found, or false if not found.
	{"selectBuf",		CFNoLoad,0,			1,1,	selectBuf,	Literal4,	FLit_selectBuf},
		// Returns name of buffer.
	{"setBufFile",		0,CFNil1,			1,1,	setBufFile,	Literal4,	FLit_setBufFile},
		// Returns new filename.
	{"setBufName",		CFNoLoad | CFNoArgs,0,		1,1,	setBufName,	Literal9,	FLit_setBufName},
		// Returns name of buffer.
	{"setHook",	CFSpecArgs | CFShrtLoad,CFNotNull1,	2,2,	setHook,	Literal33,	FLit_setHook},
	{"setMark",		CFNoLoad,0,			0,1,	setMark,	Literal22,	FLit_setMark},
	{"setWrapCol",		0,0,				0,0,	NULL,		NULL,		FLit_setWrapCol},
	{"seti",		CFNoArgs,CFInt1 | CFInt2,	1,3,	seti,		Literal14,	FLit_seti},
	{"shQuote",		CFFunc,CFNIS1,			1,1,	NULL,		Literal13,	FLit_shQuote},
		// Returns quoted string.
	{"shell",		0,0,				0,0,	shellCLI,	NULL,		FLit_shell},
		// Returns false if failure.
	{"shellCmd",		CFNoLoad,0,			1,-1,	shellCmd,	Literal10,	FLit_shellCmd},
		// Returns false if failure.
	{"shift",		CFFunc,CFArray1,		1,1,	NULL,		Literal23,	FLit_shift},
		// Returns shifted value, or nil if none left.
	{"showBindings",	CFAddlArg,0,			0,1,	showBindings,	Literal6,	FLit_showBindings},
	{"showBuffers",		0,0,				0,0,	showBuffers,	NULL,		FLit_showBuffers},
	{"showFunctions",	CFAddlArg,0,			0,1,	showFunctions,	Literal6,	FLit_showFunctions},
	{"showHooks",		0,0,				0,0,	showHooks,	NULL,		FLit_showHooks},
	{"showKey",		0,CFNotNull1,			1,1,	showKey,	Literal16,	FLit_showKey},
	{"showKillRing",	0,0,				0,0,	showKillRing,	NULL,		FLit_showKillRing},
	{"showMarks",		0,0,				0,0,	showMarks,	NULL,		FLit_showMarks},
	{"showModes",		0,0,				0,0,	showModes,	NULL,		FLit_showModes},
#if MMDebug & Debug_ShowRE
	{"showRegexp",		0,0,				0,0,	showRegexp,	NULL,		FLit_showRegexp},
#endif
	{"showScreens",		0,0,				0,0,	showScreens,	NULL,		FLit_showScreens},
	{"showVariables",	CFAddlArg,0,			0,1,	showVariables,	Literal6,	FLit_showVariables},
	{"shrinkWind",		CFNCount,0,			0,0,	NULL,		NULL,		FLit_shrinkWind},
	{"space",		CFEdit | CFNCount,0,		0,0,	NULL,		NULL,		FLit_space},
	{"split",		CFFunc,CFNil1 | CFInt3,		2,3,	ssplit,		Literal39,	FLit_split},
		// Returns array.
	{"splitWind",		0,0,				0,0,	splitWind,	NULL,		FLit_splitWind},
	{"sprintf",		CFFunc,0,			1,-1,	NULL,		Literal32,	FLit_sprintf},
	{"stat?",		CFFunc,0,			2,2,	NULL,		Literal36,	FLit_statQ},
		// Returns Boolean result.
	{"strPop",	CFFunc | CFSpecArgs | CFNoLoad,0,	2,2,	NULL,		Literal40,	FLit_strPop},
		// Returns popped value, or nil if none left.
	{"strPush",	CFFunc | CFSpecArgs | CFNoLoad,0,	3,3,	NULL,		Literal41,	FLit_strPush},
		// Returns pushed value.
	{"strShift",	CFFunc | CFSpecArgs | CFNoLoad,0,	2,2,	NULL,		Literal40,	FLit_strShift},
		// Returns shifted value, or nil if none left.
	{"strUnshift",	CFFunc | CFSpecArgs | CFNoLoad,0,	3,3,	NULL,		Literal41,	FLit_strUnshift},
		// Returns unshifted value.
	{"stringFit",		CFFunc,CFInt2,			2,2,	NULL,		Literal28,	FLit_stringFit},
		// Returns compressed string.
	{"strip",		CFFunc,0,			1,1,	NULL,		Literal12,	FLit_strip},
		// Returns whitespace-stripped string.
	{"sub",			CFFunc,CFNil3,			3,3,	NULL,		Literal18,	FLit_sub},
		// Returns string result.
	{"subLine",		CFFunc,CFInt1 | CFInt2,		1,2,	NULL,		Literal26,	FLit_subLine},
		// Returns string result.
	{"subString",		CFFunc,CFInt2 | CFInt3,		2,3,	NULL,		Literal27,	FLit_subString},
		// Returns string result.
	{"suspend",		0,0,				0,0,	suspendEMacs,	NULL,		FLit_suspend},
	{"swapMark",		CFNoLoad,0,			0,1,	swapMark,	Literal22,	FLit_swapMark},
	{"sysInfo",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_sysInfo},
		// Returns various information, depending on n arg.
	{"tab",			CFEdit,0,			0,0,	NULL,		NULL,		FLit_tab},
	{"tcString",		CFFunc,0,			1,1,	NULL,		Literal12,	FLit_tcString},
		// Returns string result.
	{"tcWord",		CFEdit | CFNCount,0,		0,0,	tcWord,		NULL,		FLit_tcWord},
	{"toInt",		CFFunc,CFInt1 | CFMay,		1,1,	NULL,		Literal12,	FLit_toInt},
		// Returns integer result.
	{"toString",	CFFunc,CFBool1 | CFArray1 | CFNIS1,	1,1,	NULL,		Literal13,	FLit_toString},
		// Returns string result.
	{"tr",			CFFunc,CFNil3,			3,3,	NULL,		Literal18,	FLit_tr},
		// Returns string result.
	{"traverseLine",	0,0,				0,0,	traverseLine,	NULL,		FLit_traverseLine},
	{"trimLine",		CFEdit,0,			0,0,	trimLine,	NULL,		FLit_trimLine},
	{"truncBuf",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_truncBuf},
		// Returns name of buffer.
	{"type?",	CFFunc,CFBool1 | CFArray1 | CFNIS1,	1,1,	NULL,		Literal13,	FLit_typeQ},
		// Returns type of value.
	{"ucLine",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_ucLine},
	{"ucRegion",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_ucRegion},
	{"ucString",		CFFunc,0,			1,1,	NULL,		Literal12,	FLit_ucString},
		// Returns string result.
	{"ucWord",		CFEdit | CFNCount,0,		0,0,	ucWord,		NULL,		FLit_ucWord},
	{"unbindKey",		0,CFNotNull1,			1,1,	unbindKey,	Literal16,	FLit_unbindKey},
		// Returns Boolean result if n > 0.
	{"unchangeBuf",		CFAddlArg | CFNoLoad,0,		0,1,	NULL,		Literal9,	FLit_unchangeBuf},
		// Returns name of buffer.
	{"undelete",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_undelete},
	{"unhideBuf",		CFAddlArg | CFNoLoad,0,		0,1,	NULL,		Literal9,	FLit_unhideBuf},
		// Returns name of buffer.
	{"universalArg",	CFHidden | CFBind1 | CFUniq,0,	0,0,	NULL,		NULL,		FLit_universalArg},
	{"unshift",CFFunc,CFArray1 | CFBool2 | CFArray2 | CFNIS2,2,2,	NULL,		Literal25,	FLit_unshift},
		// Returns new array value.
	{"updateScreen",	CFFunc,0,			0,0,	NULL,		NULL,		FLit_updateScreen},
	{"viewFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	FLit_viewFile},
		// Returns [name of buffer, "true" or "false" indicating whether the buffer was created].
	{"whence",		CFTerm,0,			0,0,	whence,		NULL,		FLit_whence},
	{"widenBuf",		0,0,				0,0,	widenBuf,	NULL,		FLit_widenBuf},
	{"windList",		CFFunc,0,			0,0,	NULL,		NULL,		FLit_windList},
		// Returns window list (array).
	{"wordChar?",		CFFunc,0,			1,1,	NULL,		Literal12,	FLit_wordCharQ},
		// Returns true if first character of a string is in $wordChars.
	{"wrapLine",		CFEdit,CFNil1 | CFNil2,		2,2,	wrapLine,	Literal31,	FLit_wrapLine},
	{"wrapWord",		CFFunc | CFEdit,0,		0,0,	wrapWord,	NULL,		FLit_wrapWord},
	{"writeBuf",		CFFunc | CFShrtLoad,CFNotNull1,	2,-1,	writeBuf,	Literal5,	FLit_writeBuf},
		// Returns text written.
	{"writeFile",		CFNoLoad,0,			1,1,	NULL,		Literal4,	FLit_writeFile},
		// Returns filename.
	{"xPathname",		CFFunc,0, 			1,1,	NULL,		Literal15,	FLit_xPathname},
		// Returns absolute pathname, or nil if not found.
	{"xeqBuf",		CFNoLoad,0,			1,-1,	xeqBuf,		Literal17,	FLit_xeqBuf},
		// Returns execution result.
	{"xeqFile",		CFNoLoad,0,			1,-1,	xeqFile,	Literal17,	FLit_xeqFile},
		// Returns execution result.
	{"xeqKeyMacro",		0,0,				0,0,	xeqKeyMacro,	NULL,		FLit_xeqKeyMacro},
	{"yank",		CFEdit,0,			0,0,	NULL,		NULL,		FLit_yank},
	{"yankPop",		CFEdit,0,			0,0,	yankPop,	NULL,		FLit_yankPop},
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
