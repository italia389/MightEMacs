// (c) Copyright 2026 Richard W. Marinelli
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

#ifndef CmdData

// **** Definitions for all .c files, including main.c before declaring global command data ****

// Structure and flags for the command-function table.  Flag rules are:
//  1. CFEdit and CFSpecArgs may not both be set.
//  2. ArgIntN and (ArgArrayN or ArgHashN) may not both be set.
//  3. If no argument type flag (ArgNilN, ArgBoolN, ArgIntN, ArgArrayN, or ArgHashN) is set, the corresponding Nth argument must
//     be string; otherwise, if ArgIntN, ArgArrayN, or ArgHashN is set, the Nth argument must be (or may be if ArgMay set)
//     integer, array, or hash; otherwise, the Nth argument must be string, nil (if ArgNilN set), or Boolean (if ArgBoolN set).
//  4. If ArgNISN is set, the Nth argument may be nil, integer, or string, and may also be array or hash if ArgArrayN or
//     ArgHashN is set.
typedef struct CmdFunc {
	const char *name;		// Name of command or function.
	ushort attrFlags;		// Attribute flags.
	uint argFlags;			// Script-argument validation flags.
	short minArgs;			// Minimum number of required arguments with default n argument.
	short maxArgs;			// Maximum number of arguments allowed for any value of n.  No maximum if negative.
	OptHdr *pOptHdr;		// Options header, if any.
	int (*func)(Datum *pRtnVal,
	 long n, Datum **args);		// C function that command or function is "bound" to.
	const char *argSyntax;		// Argument syntax (help text).
	const char *descrip;		// Command or function description (help text).
	} CmdFunc;

// Attribute flags.
#define CFFunc		0x0001		// System function, otherwise command.
#define CFExecAH	0x0002		// Array or hash function handled by execAH().
#define CFHook		0x0004		// System function may be bound to a hook: for internal use only.
#define CFHidden	0x0008		// Hidden command: for internal use only.
#define CFPrefix	0x0010		// Prefix command (meta, ^C, ^H, and ^X).
#define CFBind1		0x0020		// Bound to a single key only.
#define CFUniq		0x0040		// Cannot have more than one binding.
#define CFEdit		0x0080		// Modifies current buffer.
#define CFPerm		0x0100		// Must have one or more bindings at all times.
#define CFTerm		0x0200		// Terminal (interactive) only -- not recognized in a script.
#define CFNCount	0x0400		// N argument is purely a repeat count.
#define CFSpecArgs	0x0800		// Needs special argument processing (bare word or symbol) -- never skipped.
#define CFMinLoad	0x1000		// Load minimum number of arguments possible in execCmdFunc().
#define CFShortLoad	0x2000		// Load one fewer argument than usual in execCmdFunc().
#define CFNoLoad	0x4000		// Load no arguments in execCmdFunc().

#define CFMaxArgs	4		// Maximum number of arguments that could be loaded by execCmdFunc() for any command or
					// function; that is, maximum value of maxArgs in cmdFuncTable where CFNoLoad is not set
					// and maxArgs > 0.

// Pointer structure for places where either a command, function, alias, or buffer is allowed, and for the "exec" table, which
// is a hash of all the current command, function, and alias names with this structure as the value.
typedef struct {
	ushort type;			// Pointer type.
	union {
		CmdFunc *pCmdFunc;	// Pointer into the command-function table.
		struct Alias *pAlias;	// Alias pointer.
		struct Buffer *pBuf;	// Buffer pointer.
		void *ptr;		// For generic pointer comparisons.
		} u;
	} ExecPtr;

// Pointer types.  Set to different bits (instead of values) so that can be used as selector masks in function calls.
#define PtrNull		0x0000		// NULL pointer.
#define PtrSysCmd	0x0001		// Built-in command-function pointer -- command.
#define PtrPseudo	0x0002		// Built-in command-function pointer -- pseudo command (CFHidden flag set).
#define PtrSysFunc	0x0004		// Built-in command-function pointer -- function.
#define PtrUserCmd	0x0008		// User command (buffer) pointer.
#define PtrUserFunc	0x0010		// User function (buffer) pointer.
#define PtrBuf		0x0020		// Buffer pointer.
#define PtrAliasSysCmd	0x0040		// Alias pointer to a system command.
#define PtrAliasSysFunc	0x0080		// Alias pointer to a system function.
#define PtrAliasUserCmd	0x0100		// Alias pointer to a user command.
#define PtrAliasUserFunc 0x0200		// Alias pointer to a user function.

#define PtrSysCmdPseudo	(PtrSysCmd | PtrPseudo)
#define PtrSysCmdFunc	(PtrSysCmd | PtrSysFunc)
#define PtrAlias	(PtrAliasSysCmd | PtrAliasSysFunc | PtrAliasUserCmd | PtrAliasUserFunc)
#define PtrUserCmdFunc	(PtrUserCmd | PtrUserFunc)
#define PtrAny		(PtrSysCmdFunc | PtrPseudo | PtrUserCmdFunc | PtrBuf | PtrAlias)

// Structure for the alias list.
typedef struct Alias {
	struct Alias *next;		// Pointer to next alias.
	ushort type;			// Alias type (PtrAlias*).
	ExecPtr targ;			// Command or function pointer.
	char name[1];			// Name of alias.
	} Alias;

// Command-function IDs.
typedef enum {
	cf_abort, cf_about, cf_abs, cf_aclear, cf_aclone, cf_acompact, cf_adelete, cf_adeleteif, cf_afill, cf_aincludeQ,
	cf_aindex, cf_ainsert, cf_alias, cf_apop, cf_appendFile, cf_apropos, cf_apush, cf_array, cf_ashift, cf_asort, cf_atoh,
	cf_aunshift, cf_backChar, cf_backLine, cf_backPage, cf_backPageNext, cf_backPagePrev, cf_backTab, cf_backWord,
	cf_backspace, cf_basename, cf_beep, cf_beginBuf, cf_beginLine, cf_beginMacro, cf_beginText, cf_beginWhite, cf_bemptyQ,
	cf_bgets, cf_bindKey, cf_binding, cf_bindingsof, cf_bprint, cf_bprintf, cf_btoa, cf_btos, cf_bufAttrQ, cf_bufBoundQ,
	cf_bufInfo, cf_bufMatchGrp, cf_bufWind, cf_chdir, cf_chgBufAttr, cf_chgBufHome, cf_chgMode, cf_chomp, cf_chr,
	cf_clearBuf, cf_clearHook, cf_clearMsgLine, cf_copyFencedRegion, cf_copyLine, cf_copyRegion, cf_copyToBreak,
	cf_copyWord, cf_cycleRing, cf_delAlias, cf_delBackChar, cf_delBackTab, cf_delBlankLines, cf_delBuf, cf_delConstVar,
	cf_delFencedRegion, cf_delFile, cf_delForwChar, cf_delForwTab, cf_delLine, cf_delMacro, cf_delMark, cf_delRegion,
	cf_delRingEntry, cf_delRoutine, cf_delScreen, cf_delShortcut, cf_delToBreak, cf_delWhite, cf_delWind, cf_delWord,
	cf_detabLine, cf_dirname, cf_dupLine, cf_editMacro, cf_editMode, cf_editModeGrp, cf_emptyQ, cf_endBuf, cf_endLine,
	cf_endMacro, cf_endWhite, cf_endWord, cf_entabLine, cf_env, cf_eval, cf_exit, cf_expandPath, cf_extension, cf_findFile,
	cf_forwChar, cf_forwLine, cf_forwPage, cf_forwPageNext, cf_forwPagePrev, cf_forwTab, cf_forwWord, cf_getInfo, cf_getKey,
	cf_getMacro, cf_getWord, cf_glob, cf_gotoFence, cf_gotoLine, cf_gotoMark, cf_growWind, cf_grpModeActiveQ, cf_hash,
	cf_hasKeyQ, cf_hclear, cf_hclone, cf_hcompact, cf_hdelete, cf_hdeleteif, cf_hfindnode, cf_hincludeQ, cf_hkeys, cf_hsort,
	cf_htoa, cf_huntBack, cf_huntForw, cf_hvalues, cf_indentLine, cf_indentRegion, cf_index, cf_insert, cf_insertBuf,
	cf_insertFile, cf_insertPipe, cf_insertSpace, cf_insertf, cf_inserti, cf_interactiveQ, cf_isBufQ, cf_isClassQ,
	cf_isConstVarQ, cf_isExecQ, cf_isMacroQ, cf_isMarkQ, cf_isModeQ, cf_isModeGrpQ, cf_isShortcutQ, cf_join, cf_joinLines,
	cf_joinWind, cf_keyPendingQ, cf_kill, cf_killFencedRegion, cf_killLine, cf_killRegion, cf_killToBreak, cf_killWord,
	cf_lastBuf, cf_lastScreen, cf_length, cf_let, cf_linkFile, cf_loadMacro, cf_lowerCaseLine, cf_lowerCaseRegion,
	cf_lowerCaseStr, cf_lowerCaseWord, cf_markBuf, cf_message, cf_metaPrefix, cf_mkdir, cf_modeActiveQ, cf_moveWindDown,
	cf_moveWindUp, cf_narrowBuf, cf_negativeArg, cf_newline, cf_newlineI, cf_nextBuf, cf_nextScreen, cf_nextWind, cf_nilQ,
	cf_nullQ, cf_numericQ, cf_onlyWind, cf_openLine, cf_openLineI, cf_ord, cf_outdentLine, cf_outdentRegion,
	cf_overwriteChar, cf_overwriteCol, cf_pathname, cf_pause, cf_pipeBuf, cf_popBuf, cf_popFile, cf_prefix1, cf_prefix2,
	cf_prefix3, cf_prevBuf, cf_prevScreen, cf_prevWind, cf_print, cf_printf, cf_prompt, cf_pwd, cf_queryReplace,
	cf_quickExit, cf_quote, cf_quoteChar, cf_rand, cf_readFile, cf_readPipe, cf_reframeWind, cf_renameBuf, cf_renameFile,
	cf_renameMacro, cf_replace, cf_resetTerm, cf_resizeWind, cf_restoreBuf, cf_restoreScreen, cf_restoreWind, cf_revertYank,
	cf_ringSize, cf_rmdir, cf_run, cf_saveBuf, cf_saveFile, cf_saveScreen, cf_saveWind, cf_scratchBuf, cf_searchBack,
	cf_searchForw, cf_selectBuf, cf_selectMacro, cf_selectScreen, cf_selectText, cf_selectWind, cf_setBufFile,
	cf_setColorPair, cf_setDefault, cf_setDispColor, cf_setHook, cf_setMark, cf_setShortcut, cf_setWrapCol, cf_seti,
	cf_shQuote, cf_shell, cf_shellCmd, cf_shiftLine, cf_showAliases, cf_showBindings, cf_showBuffers, cf_showColors,
	cf_showConstVar, cf_showConstVars, cf_showFence, cf_showHooks, cf_showKey, cf_showMacros, cf_showMarks, cf_showModes,
	cf_showPoint,
#if MMDebug & Debug_ShowRE
	cf_showRegexp,
#endif
	cf_showRing, cf_showRoutines, cf_showScreens, cf_shrinkWind, cf_sortRegion, cf_space, cf_split, cf_splitWind,
	cf_sprintf, cf_statQ, cf_strMatchGrp, cf_strPop, cf_strPush, cf_strShift, cf_strUnshift, cf_strfit, cf_strip, cf_sub,
	cf_subline, cf_substr, cf_suspend, cf_swapMark, cf_swapScreens, cf_tab, cf_titleCaseLine, cf_titleCaseRegion,
	cf_titleCaseStr, cf_titleCaseWord, cf_toint, cf_tostr, cf_tr, cf_traverseLine, cf_trimLine, cf_truncBuf, cf_typeof,
	cf_unbindKey, cf_undelete, cf_undeleteCycle, cf_universalArg, cf_updateScreen, cf_upperCaseLine, cf_upperCaseRegion,
	cf_upperCaseStr, cf_upperCaseWord, cf_viewFile, cf_widenBuf, cf_wrapLine, cf_wrapWord, cf_writeFile, cf_writeLine,
	cf_writeRegion, cf_xPathname, cf_xeqBuf, cf_xeqFile, cf_xeqMacro, cf_yank, cf_yankCycle, cf_yankDel
#if MMDebug & Debug_SanityChk
	, cf_END
#endif
	} CmdFuncId;

// Flags for array, hash, and sort (function) operations.
#define AHS_Descending	OpCut		// Sort in descending order, otherwise ascending (via unused CXL operation flag).

// Flags for delFile, linkFile, and renameFile commands.
#define File_DiskOnly	0x00000001
#define File_ForceLink	0x00000002
#define File_FromBuf	0x00000004
#define File_IgnoreErr	0x00000008
#define File_Symbolic	0x00000010
#define File_WithBuf	0x00000020

// Flags for delWind and joinWind commands.
#define Wind_JoinDown	0x00000001
#define Wind_ForceJoin	0x00000002
#define Wind_DelBuf	0x00000004

// Flags for getKey function.
#define GK_KeySeq	0x00000001
#define GK_KeyLit	0x00000002
#define GK_DoAbort	0x00000004

// Flags for showBindings command.
#define Bnd_ExclFunc	0x00000001
#define Bnd_InclUnbound	0x00000002

// Flags for showBuffers command.
#define SBO_SelVisible	0x00000001
#define SBO_SelHomed	0x00000002
#define SBO_SelCommand	0x00000004
#define SBO_SelFunction	0x00000008
#define SBO_InclHidden	0x00000010
#define SBO_InclHomeDir	0x00000020

// Flags for apropos, showCommands, showConstVars, and showFunctions commands.
#define CFCV_SelCmd	0x00000001
#define CFCV_SelFunc	0x00000002
#define CFCV_SelSystem	0x00000004
#define CFCV_SelUser	0x00000008

// Flags for strIndex() function.
#define Idx_Char	0x00000001	// Find a character, otherwise a pattern.
#define Idx_Last	0x00000002	// Find last occurrence, otherwise first.

// Flags for xPathname function.
#define XP_HomeOnly	0x00000001	// Search HOME directory only.
#define XP_GlobPat	0x00000002	// Do glob search.
#define XP_SkipNull	0x00000004	// Skip null directories in $execPath.

// External variable declarations.
extern Alias *ahead;
extern CmdFunc cmdFuncTable[], *pCmdFuncEnd;
extern Hash execTable;

#else	// CmdData

// **** For main.c global data ****

// Global variables.
Alias *ahead = NULL;			// Head of alias list.

// OptHdr objects referenced in cmdFuncTable and processed automatically by execCmdFunc().
static Option ahsOpts[] = {					// Common array, hash, and sort options.
	{"^Descending", "^Desc", 0, .u.value = AHS_Descending},
	{"^IgnoreCase", "^Ign", 0, .u.value = OpIgnoreCase},
	{"In^Place", "I^P", 0, .u.value = OpInPlace},
	{"^Last", NULL, 0, .u.value = OpLast}};
static OptHdr asortHdr = {					// For asort function.
	ahsOpts, NULL, NULL, elementsof(ahsOpts), -1, 0, OHdrShared | OpLast};
static OptHdr ahIgnoreHdr = {					// For {a|h}deleteif, hfindnode, and {a|h}include? functions.
	ahsOpts, NULL, NULL, elementsof(ahsOpts), -1, 0, OHdrShared | AHS_Descending | OpInPlace | OpLast};
static OptHdr aindexHdr = {					// For aindex function.
	ahsOpts, NULL, NULL, elementsof(ahsOpts), -1, 0, OHdrShared | AHS_Descending | OpInPlace};
static OptHdr sortHdr = {					// For hsort and sortRegion routines.
	ahsOpts, NULL, NULL, elementsof(ahsOpts), -1, 0, OHdrShared | OpInPlace | OpLast};
static Option cfcvOptions[] = {
	{"^Command", "^Cmd", 0, .u.value = CFCV_SelCmd},
	{"^Function", "^Func", 0, .u.value = CFCV_SelFunc},
	{"^System", "^Sys", 0, .u.value = CFCV_SelSystem},
	{"^User", NULL, 0, .u.value = CFCV_SelUser}};
static OptHdr cfHdr = {						// For showRoutines command.
	cfcvOptions, NULL, NULL, elementsof(cfcvOptions), -1, 0, OHdrShared | OHdrBuild};
static OptHdr cfcvHdr = {					// For apropos and showConstVars commands.
	cfcvOptions, NULL, NULL, elementsof(cfcvOptions), -1, 0, OHdrShared | OHdrBuild | CFCV_SelCmd | CFCV_SelFunc};
static Option delBufOpts[] = {
	{"^Visible", "^Viz", 0, .u.value = BD_Visible},
	{"^Unchanged", "^Unchg", 0, .u.value = BD_Unchanged},
	{"Ho^med", "Ho^m", 0, .u.value = BD_Homed},
	{"^Inactive", "^Inact", 0, .u.value = BD_Inactive},
	{"^Displayed", "^Disp", 0, .u.value = BD_Displayed},
	{"^Hidden", "^Hid", 0, .u.value = BD_Hidden},
	{"^Confirm", "^Cfrm", 0, .u.value = BD_Confirm},
	{"^Force", "^Frc", 0, .u.value = BD_Force}};
static OptHdr delBufHdr = {					// For delBuf command.
	delBufOpts, NULL, NULL, elementsof(delBufOpts), 0, 0, OHdrFirst | OHdrPrep | OHdrBuild | OHdrNoSel};
static Option delFileOpts[] = {
	{"With^Buf", NULL, 0, .u.value = File_WithBuf},
	{"^IgnoreErr", "^Ign", 0, .u.value = File_IgnoreErr}};
static OptHdr delFileHdr = {					// For delFile command.
	delFileOpts, NULL, NULL, elementsof(delFileOpts), -1, 0, 0};
static Option djWindOpts[] = {
	{"^JoinDown", "^JDwn", 0, .u.value = Wind_JoinDown},
	{"^ForceJoin", "^Force", 0, .u.value = Wind_ForceJoin},
	{"^DelBuf", "^Del", 0, .u.value = Wind_DelBuf}};
static OptHdr djWindHdr = {					// For delWind and joinWind commands.
	djWindOpts, NULL, NULL, elementsof(djWindOpts), -1, 0, 0};
static Option indexOpts[] = {
	{"^Char", NULL, 0, .u.value = Idx_Char},
	{"^Last", NULL, 0, .u.value = Idx_Last}};
static OptHdr indexHdr = {					// For index function.
	indexOpts, NULL, NULL, elementsof(indexOpts), -1, 0, OHdrFunc};
static Option gkOpts[] = {
	{"^KeySeq", NULL, 0, .u.value = GK_KeySeq},
	{"^Char", NULL, OptFalse, .u.value = GK_KeyLit},
	{"^LitAbort", NULL, OptFalse, .u.value = GK_DoAbort}};
static OptHdr getKeyHdr = {					// For getKey function.
	gkOpts, NULL, NULL, elementsof(gkOpts), -1, 0, OHdrFunc | OHdrPrep};
static Option linkFileOpts[] = {
	{"^SymLink", "^SymLk", 0, .u.value = File_Symbolic},
	{"^Force", NULL, 0, .u.value = File_ForceLink}};
static OptHdr linkFileHdr = {					// For linkFile command.
	linkFileOpts, NULL, NULL, elementsof(linkFileOpts), -1, 0, 0};
static Option loadMacroOpts[] = {
	{"^Activate", NULL, 0, 0},
	{"^Force", NULL, 0, 0}};
static OptHdr loadMacroHdr = {					// For loadMacro function.
	loadMacroOpts, NULL, text451, elementsof(loadMacroOpts), -1, 0, OHdrFunc | OHdrNoSel};
		// "function option"
static Option msgOpts[] = {
	{"^Force", NULL, 0, .u.value = RSForce},
	{"^High", NULL, 0, .u.value = RSHigh},
	{"No^Wrap", NULL, 0, .u.value = RSNoWrap},
	{"^TermAttr", NULL, 0, .u.value = RSTermAttr},
	{"^Fail", NULL, 0, 0}};
static OptHdr msgHdr = {					// For message function.
	msgOpts, NULL, NULL, elementsof(msgOpts), 0, 0, OHdrFunc | OHdrFirst};
static Option popOptions[] = {
	{"^Existing", "^Exist", 0, 0},
	{"^AltModeLine", "^AltML", 0, RendAltML},
	{"^Shift", "^Shft", 0, RendShift},
	{"^TermAttr", "^TAttr", 0, 0},
	{"^Delete", "^Del", 0, RendNewBuf}};
static OptHdr popHdr = {					// For popBuf and popFile commands.
	popOptions, NULL, text223, elementsof(popOptions), -1, 0, OHdrPrep};
		// "pop option"
static Option renameFileOpts[] = {
	{"From^Buf", NULL, 0, .u.value = File_FromBuf},
	{"^DiskOnly", "^DskOnly", 0, .u.value = File_DiskOnly}};
static OptHdr renameFileHdr = {					// For renameFile command.
	renameFileOpts, NULL, NULL, elementsof(renameFileOpts), -1, 0, 0};
static Option shellCmdOpts[] = {
	{"^Shift", "^Shft", 0, 0},
	{"No^Pop", NULL, 0, 0},
	{"No^Hdr", NULL, 0, 0}};
static OptHdr shellCmdHdr = {					// For shellCmd command.
	shellCmdOpts, NULL, NULL, elementsof(shellCmdOpts), 0, 0, OHdrFirst};
static Option showBindOpts[] = {
	{"^NoFuncKey", "^NoFnK", 0, .u.value = Bnd_ExclFunc},
	{"^Unbound", "^Unbnd", 0, .u.value = Bnd_InclUnbound}};
static OptHdr showBindHdr = {					// For showBindings command.
	showBindOpts, NULL, NULL, elementsof(showBindOpts), -1, 0, 0};
static Option showBufOpts[] = {
	{"^Visible", "^Viz", 0, .u.value = SBO_SelVisible},
	{"Ho^med", "H^md", 0, .u.value = SBO_SelHomed},
	{"^Command", "^Cmd", 0, .u.value = SBO_SelCommand},
	{"^Function", "^Func", 0, .u.value = SBO_SelFunction},
	{"^Hidden", "^Hid", 0, .u.value = SBO_InclHidden},
	{"Home^Dir", "Hm^Dir", 0, .u.value = SBO_InclHomeDir}};
static OptHdr showBufHdr = {					// For showBuffers command.
	showBufOpts, NULL, NULL, elementsof(showBufOpts), -1, 0, OHdrBuild};
static Option tostrOpts[] = {
	{"Show^Nil", NULL, 0, .u.value = CvtShowNil},
	{"S^kipNil", NULL, 0, .u.value = CvtSkipNil},
	{"Quote^1", NULL, 0, .u.value = CvtQuote1},
	{"Quote^2", NULL, 0, .u.value = CvtQuote2},
	{"^Quote", NULL, 0, .u.value = CvtQuote},
	{"^VizChar", NULL, 0, .u.value = CvtVizChar},
	{"^EscChar", NULL, 0, .u.value = CvtEscChar},
	{"^ThouSep", NULL, 0, .u.value = CvtThouSep},
	{"^Delim", NULL, 0, .u.value = CvtDelim2},
	{"^Fences", NULL, 0, .u.value = CvtFences},
	{"^Lang", NULL, 0, .u.value = CvtLang},
	{"Viz^Str", NULL, 0, .u.value = CvtVizStr}};
static OptHdr tostrHdr = {					// For tostr function.
	tostrOpts, NULL, NULL, elementsof(tostrOpts), 2, 0, OHdrFunc};
static Option updScrnOpts[] = {
	{"^Force", NULL, 0, 0},
	{"No^Redraw", NULL, 0, 0}};
static OptHdr updScrnHdr = {					// For updateScreen function.
	updScrnOpts, NULL, NULL, elementsof(updScrnOpts), -1, 0, OHdrFunc | OHdrNoSel};
static Option xPathOpts[] = {
	{"^Glob", NULL, 0, .u.value = XP_GlobPat},
	{"^SkipNull", NULL, 0, .u.value = XP_SkipNull}};
static OptHdr xPathHdr = {					// For xPathname function.
	xPathOpts, NULL, NULL, elementsof(xPathOpts), -1, 0, OHdrFunc};

CmdFunc cmdFuncTable[] = {
  {"abort",		CFBind1 | CFUniq, 0,			0, -1,	NULL, abortOp,		text800, CFLit_abort},
  {"about",		0, 0,					0, 0,	NULL, aboutMM,		NULL, CFLit_about},
  {"abs",		CFFunc, ArgInt1,			1, 1,	NULL, NULL,		text820, CFLit_abs},
  {"aclear",		CFFunc | CFExecAH, ArgArray1,		1, 1,	NULL, NULL,		text822, CFLit_aclear},
		// Returns array.
  {"aclone",		CFFunc | CFExecAH, ArgArray1,		1, 1,	NULL, NULL,		text822, CFLit_aclone},
		// Returns new array.
  {"acompact",		CFFunc | CFExecAH, ArgArray1,		1, 1,	NULL, NULL,		text822, CFLit_acompact},
		// Returns deletion count.
  {"adelete",		CFFunc | CFExecAH, ArgArray1 | ArgInt2 | ArgInt3,
								2, 3,	NULL, NULL,		text846, CFLit_adelete},
		// Returns array element or slice.
  {"adeleteif",		CFFunc | CFExecAH, ArgArray1 | ArgAny2,	2, 3,	&ahIgnoreHdr, NULL,	text894, CFLit_adeleteif},
		// Returns deletion count.
  {"afill",		CFFunc | CFExecAH, ArgArray1 | ArgInt2 | ArgInt3 | ArgAny4,
								4, 4,	NULL, NULL,		text877, CFLit_afill},
		// Returns existing array.
  {"ainclude?",		CFFunc | CFExecAH, ArgArray1 | ArgAny2,	2, 3,	&ahIgnoreHdr, NULL,	text894, CFLit_aincludeQ},
		// Returns true if a value occurs in given array.
  {"aindex",		CFFunc | CFExecAH, ArgArray1 | ArgAny2,	2, 3,	&aindexHdr, NULL,	text894, CFLit_aindex},
		// Returns index if a value occurs in given array.
  {"ainsert",		CFFunc | CFExecAH, ArgArray1 | ArgInt2 | ArgAny3,
								3, 3,	NULL, NULL,		text878, CFLit_ainsert},
		// Returns existing array.
  {"alias",		CFSpecArgs | CFNoLoad, 0,		2, 2,	NULL, alias,		text801, CFLit_alias},
  {"apop",		CFFunc | CFExecAH, ArgArray1,		1, 1,	NULL, NULL,		text822, CFLit_apop},
		// Returns popped value, or nil if none left.
  {"appendFile",	CFNoLoad, 0,				1, 1,	NULL, NULL,		text99, CFLit_appendFile},
		// Returns filename.
  {"apropos",		0, ArgNil1,				0, 2,	&cfcvHdr, apropos,	text873, CFLit_apropos},
  {"apush",	CFFunc | CFExecAH, ArgArray1 | ArgAny2,		2, 2,	NULL, NULL,		text824, CFLit_apush},
		// Returns new array value.
  {"array",		CFFunc, ArgInt1 | ArgAny2,	 	0, 2,	NULL, array,		text828, CFLit_array},
		// Returns new array.
  {"ashift",		CFFunc | CFExecAH, ArgArray1,		1, 1,	NULL, NULL,		text822, CFLit_ashift},
		// Returns shifted value, or nil if none left.
  {"asort",	 CFFunc | CFExecAH, ArgArray1 | ArgNotNull2,	1, 2,	&asortHdr, NULL,	text889, CFLit_asort},
		// Returns sorted array.
  {"atoh",		CFFunc | CFExecAH, ArgArray1,		1, 2,	&ahIgnoreHdr, NULL,	text889, CFLit_atoh},
		// Returns hash.
  {"aunshift",	CFFunc | CFExecAH, ArgArray1 | ArgAny2,		2, 2,	NULL, NULL,		text824, CFLit_aunshift},
		// Returns new array value.
  {"backChar",		CFNCount, 0,				0, 0,	NULL, backChar,		NULL, CFLit_backChar},
		// Returns false if hit buffer boundary, otherwise true.
  {"backLine",		CFNCount, 0,				0, 0,	NULL, backLine,		NULL, CFLit_backLine},
		// Returns false if hit buffer boundary, otherwise true.
  {"backPage",		0, 0,					0, 0,	NULL, backPage,		NULL, CFLit_backPage},
  {"backPageNext",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_backPageNext},
  {"backPagePrev",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_backPagePrev},
  {"backTab",		CFNCount, 0,				0, 0,	NULL, NULL,		NULL, CFLit_backTab},
  {"backWord",		CFNCount, 0,				0, 0,	NULL, backWord,		NULL, CFLit_backWord},
  {"backspace",		CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_backspace},
  {"basename",		CFFunc, 0,				1, 1,	NULL, NULL,		text814, CFLit_basename},
		// Returns filename component of pathname.
  {"beep",		CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_beep},
  {"beginBuf",		CFNoLoad, 0,				0, 1,	NULL, NULL,		text808, CFLit_beginBuf},
  {"beginLine",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_beginLine},
  {"beginMacro",	CFTerm, 0,				0, 0,	NULL, beginMacro,	NULL, CFLit_beginMacro},
  {"beginText",		0, 0,					0, 0,	NULL, beginText,	NULL, CFLit_beginText},
  {"beginWhite",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_beginWhite},
  {"bempty?",		CFFunc, ArgNotNull1,			0, 1,	NULL, NULL,		text808, CFLit_bemptyQ},
		// Returns true if specified buffer is empty, otherwise false.
  {"bgets",		CFFunc | CFNCount | CFNoLoad, 0,	1, 1,	NULL, NULL,		text83, CFLit_bgets},
		// Returns nth next line from buffer.
  {"bindKey",		CFSpecArgs | CFShortLoad, ArgNotNull1,	2, 2,	NULL, bindKey,		text806, CFLit_bindKey},
  {"binding",		CFFunc, ArgNotNull1,			1, 1,	NULL, NULL,		text815, CFLit_binding},
		// Returns name of command key is bound to, or nil if none.
  {"bindingsof",	CFSpecArgs | CFFunc | CFNoLoad, 0,	1, 1,	NULL, NULL,		text856, CFLit_bindingsof},
		// Returns array of keys bound to command.
  {"bprint",		CFFunc | CFShortLoad, ArgNotNull1,	2, -1,	NULL, NULL,		text804, CFLit_bprint},
		// Returns text written.
  {"bprintf",		CFFunc, ArgNotNull1,			2, -1,	NULL, NULL,		text852, CFLit_bprintf},
		// Returns text written.
  {"btoa",		CFFunc, ArgNotNull1,			1, 1,	NULL, NULL,		text83, CFLit_btoa},
		// Returns buffer converted to an array.
  {"btos",		CFFunc, ArgNotNull1,			1, 1,	NULL, NULL,		text83, CFLit_btos},
		// Returns buffer converted to a string.
  {"bufAttr?",		CFFunc, ArgNotNull1 | ArgNotNull2,	1, 2,	NULL, bufAttrQ,		text863, CFLit_bufAttrQ},
		// Returns true if attribute set in buffer, otherwise false.
  {"bufBound?",		CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_bufBoundQ},
		// Returns true if point is at beginning, middle, or end of buffer per n argument, otherwise false.
  {"bufInfo",		CFFunc, ArgNotNull1,			1, 1,	NULL, bufInfo,		text83, CFLit_bufInfo},
		// Returns hash of information for given buffer.
  {"bufMatchGrp",	CFFunc, ArgInt1,			0, 1,	NULL, NULL,		text821, CFLit_bufMatchGrp},
		// Returns group match string for buffer search, or null if none.
  {"bufWind",		CFFunc, 0,				1, 1,	NULL, NULL,		text83, CFLit_bufWind},
		// Returns ordinal number of first window on current screen displaying given buffer, or nil if none.
  {"chdir",		0, ArgNotNull1,				1, 1,	NULL, NULL,		text837, CFLit_chdir},
		// Returns absolute pathname of new directory.
  {"chgBufAttr",	0, ArgNotNull2,				2, 2,	&bufAttrHdr, chgBufAttr, text845, CFLit_chgBufAttr},
		// Returns former state (-1 or 1) or last attribute changed.
  {"chgBufHome",	CFNoLoad, ArgInt1,			1, 2,	NULL, chgBufHome,	text833, CFLit_chgBufHome},
		// Returns two-element array containing new buffer name and new filename.
  {"chgMode",		0, ArgNotNull1 | ArgArray1 | ArgNotNull2 | ArgMay,
								1, 2,	NULL, chgMode,		text847, CFLit_chgMode},
		// Returns former state (-1 or 1) or last mode changed.
  {"chomp",		CFFunc, ArgInt2,			2, 2,	NULL, NULL,		text887, CFLit_chomp},
		// Returns string with last character removed.
  {"chr",		CFFunc, ArgInt1,			1, 1,	NULL, NULL,		text820, CFLit_chr},
		// Returns ordinal value of a character in string form.
  {"clearBuf",		CFNoLoad, 0,				0, 1,	NULL, clearBuf,		text808, CFLit_clearBuf},
		// Returns false if buffer is not cleared, otherwise true.
  {"clearHook",		CFFunc | CFNoLoad, 0,			0, -1,	NULL, clearHook,	text872, CFLit_clearHook},
		// Returns zero if failure, otherwise number of hooks cleared.
  {"clearMsgLine",	CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_clearMsgLine},
  {"copyFencedRegion",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_copyFencedRegion},
  {"copyLine",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_copyLine},
  {"copyRegion",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_copyRegion},
  {"copyToBreak",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_copyToBreak},
  {"copyWord",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_copyWord},
  {"cycleRing",		CFNoLoad, 0,				1, 1,	NULL, cycleRing,	text874, CFLit_cycleRing},
  {"delAlias",		CFSpecArgs | CFNoLoad, 0,		1, -1,	NULL, delAlias,		text807, CFLit_delAlias},
		// Returns zero if failure, otherwise number of aliases deleted.
  {"delBackChar",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_delBackChar},
  {"delBackTab",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_delBackTab},
  {"delBlankLines",	CFEdit, 0,				0, 0,	NULL, delBlankLines,	NULL, CFLit_delBlankLines},
  {"delBuf",		CFNoLoad, 0,				1, -1,	&delBufHdr, delBuf,	text836, CFLit_delBuf},
		// Returns zero if failure, otherwise number of buffers deleted.
  {"delConstVar",	CFSpecArgs | CFNoLoad, 0,		1, -1,	NULL, delConstVar,	text807, CFLit_delConstVar},
		// Returns zero if failure, otherwise number of constants and/or variables deleted.
  {"delFencedRegion",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_delFencedRegion},
  {"delFile",		0, ArgNotNull1,				1, 2,	&delFileHdr, delFile,	text879, CFLit_delFile},
		// Returns filename if successful, otherwise nil.
  {"delForwChar",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_delForwChar},
  {"delForwTab",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_delForwTab},
  {"delLine",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_delLine},
  {"delMacro",		CFNoLoad, 0,				0, -1,	NULL, delMacro,		text872, CFLit_delMacro},
  {"delMark",		CFNoLoad, 0,				0, -1,	NULL, delMark,		text891, CFLit_delMark},
  {"delRegion",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_delRegion},
  {"delRingEntry",	CFNoLoad, 0,				1, 2,	NULL, delRingEntry,	text874, CFLit_delRingEntry},
  {"delRoutine",	CFSpecArgs | CFNoLoad, 0,		1, -1,	NULL, delRoutine,	text807, CFLit_delRoutine},
		// Returns zero if failure, otherwise number of user commands and/or functions deleted.
  {"delScreen",		CFNoLoad, 0,				1, 1,	NULL, delScreen,	text820, CFLit_delScreen},
  {"delShortcut",	CFNoLoad, 0,				0, -1,	NULL, delShortcut,	text868, CFLit_delShortcut},
  {"delToBreak",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_delToBreak},
  {"delWhite",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_delWhite},
  {"delWind",		0, 0,					0, 1,	&djWindHdr, NULL,	text844, CFLit_delWind},
  {"delWord",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_delWord},
  {"detabLine",		CFEdit, ArgInt1,			1, 1,	NULL, detabLine,	text862, CFLit_detabLine},
  {"dirname",		CFFunc, 0,				1, 1,	NULL, NULL,		text814, CFLit_dirname},
		// Returns directory component of pathname.
  {"dupLine",		CFEdit, 0,				0, 0,	NULL, dupLine,		NULL, CFLit_dupLine},
  {"editMacro",		CFTerm, 0,				0, 0,	NULL, editMacro,	NULL, CFLit_editMacro},
  {"editMode",		0, ArgNotNull1,				1, -1,	NULL, editMode,		text853, CFLit_editMode},
  {"editModeGrp",	0, ArgNotNull1,				1, -1,	NULL, editModeGrp,	text854, CFLit_editModeGrp},
  {"empty?",		CFFunc, ArgNIS1 | ArgArray1 | ArgHash1 | ArgMay,
								1, 1,	NULL, NULL,		text812, CFLit_emptyQ},
		// Returns true if nil, ?\0, null string, empty array, or empty hash.
  {"endBuf",		CFNoLoad, 0,				0, 1,	NULL, NULL,		text808, CFLit_endBuf},
  {"endLine",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_endLine},
  {"endMacro",		CFTerm, 0,				0, 0,	NULL, endMacro,		NULL, CFLit_endMacro},
  {"endWhite",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_endWhite},
  {"endWord",		CFNCount, 0,				0, 0,	NULL, endWord,		NULL, CFLit_endWord},
		// Returns false if hit buffer boundary, otherwise true.
  {"entabLine",		CFEdit, ArgInt1,			1, 1,	NULL, entabLine,	text862, CFLit_entabLine},
  {"env",		CFFunc, 0,				1, 1,	NULL, NULL,		text97, CFLit_env},
		// Returns value of environmental variable, or nil if not found.
  {"eval",		CFNoLoad, 0,				1, -1,	NULL, eval,		text809, CFLit_eval},
		// Returns result of evaluation.
  {"exit",		0, 0,					0, -1,	NULL, quit,		text800, CFLit_exit},
  {"expandPath",	CFFunc, 0,				1, 1,	NULL, NULL,		text814, CFLit_expandPath},
		// Returns pathname with "~/", "~user/", "$var", and "${var}" expanded.
  {"extension",		CFFunc, 0,				1, 1,	NULL, NULL,		text814, CFLit_extension},
		// Returns extension component of pathname.
  {"findFile",		CFNoLoad, 0,				1, 1,	NULL, NULL,		text99, CFLit_findFile},
		// Returns [name of buffer, "true" or "false" indicating whether the buffer was created].
  {"forwChar",		CFNCount, 0,				0, 0,	NULL, forwChar,		NULL, CFLit_forwChar},
		// Returns false if hit buffer boundary, otherwise true.
  {"forwLine",		CFNCount, 0,				0, 0,	NULL, forwLine,		NULL, CFLit_forwLine},
		// Returns false if hit buffer boundary, otherwise true.
  {"forwPage",		0, 0,					0, 0,	NULL, forwPage,		NULL, CFLit_forwPage},
  {"forwPageNext",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_forwPageNext},
  {"forwPagePrev",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_forwPagePrev},
  {"forwTab",		CFNCount, 0,				0, 0,	NULL, NULL,		NULL, CFLit_forwTab},
  {"forwWord",		CFNCount, 0,				0, 0,	NULL, forwWord,		NULL, CFLit_forwWord},
  {"getInfo",		CFFunc, ArgNotNull1,			1, 1,	NULL, getInfo,		text546, CFLit_getInfo},
		// Returns informational item per keyword argument.
  {"getKey",		CFFunc, 0,				0, 1,	&getKeyHdr, fGetKey,	text844, CFLit_getKey},
		// Returns key in encoded form.
  {"getMacro",		CFFunc, ArgNotNull1 | ArgNil1,		0, 1,	NULL, getMacro,		text871, CFLit_getMacro},
  {"getWord",		CFFunc, 0,				0, 0,	NULL, getWord,		NULL, CFLit_getWord},
		// Returns word from current buffer.
  {"glob",		CFFunc | CFNoLoad, 0,			1, -1,	NULL, globPat,		text895, CFLit_glob},
		// Returns array of pathnames.
  {"gotoFence",		0, ArgInt1,				0, 1,	NULL, gotoFence,	text857, CFLit_gotoFence},
		// Returns true if matching fence found, otherwise false.
  {"gotoLine",		CFMinLoad, ArgInt1,			1, 2,	NULL, gotoLine,		text833, CFLit_gotoLine},
  {"gotoMark",		CFNoLoad, 0,				1, 1,	NULL, gotoMark,		text834, CFLit_gotoMark},
  {"growWind",		CFNCount, 0,				0, 0,	NULL, NULL,		NULL, CFLit_growWind},
  {"grpModeActive?",	CFFunc, ArgNotNull1 | ArgNotNull2,	1, 2,	NULL, grpModeActiveQ,	text855, CFLit_grpModeActiveQ},
		// Returns name of mode if a mode in group is active, otherwise nil.
  {"hash",		CFFunc | CFExecAH, ArgInt1,		0, 2,	&ahIgnoreHdr, NULL,	text884, CFLit_hash},
		// Returns new hash.
  {"hasKey?",		CFFunc | CFExecAH, ArgHash1,		2, 2,	NULL, NULL,		text886, CFLit_hasKeyQ},
		// Returns true if hash key exists.
  {"hclear",		CFFunc | CFExecAH, ArgHash1 | ArgInt2,	1, 2,	NULL, NULL,		text876, CFLit_hclear},
		// Returns hash.
  {"hclone",		CFFunc | CFExecAH, ArgHash1,		1, 1,	NULL, NULL,		text885, CFLit_hclone},
		// Returns new hash.
  {"hcompact",		CFFunc | CFExecAH, ArgHash1,		1, 1,	NULL, NULL,		text885, CFLit_hcompact},
		// Returns deletion count.
  {"hdelete",		CFFunc | CFExecAH, ArgHash1,		2, 2,	NULL, NULL,		text886, CFLit_hdelete},
		// Returns node value.
  {"hdeleteif",		CFFunc | CFExecAH, ArgHash1 | ArgAny2,	2, 3,	&ahIgnoreHdr, NULL,	text888, CFLit_hdeleteif},
		// Returns deletion count.
  {"hfindnode",		CFFunc | CFExecAH, ArgHash1 | ArgAny2,	2, 3,	&ahIgnoreHdr, NULL,	text888, CFLit_hfindnode},
		// Returns index of value if it occurs in hash.
  {"hinclude?",		CFFunc | CFExecAH, ArgHash1 | ArgAny2,	2, 3,	&ahIgnoreHdr, NULL,	text888, CFLit_hincludeQ},
		// Returns true if value occurs in hash.
  {"hkeys",		CFFunc | CFExecAH, ArgHash1,		1, 1,	NULL, NULL,		text885, CFLit_hkeys},
		// Returns array.
  {"hsort",	CFFunc | CFExecAH, ArgHash1 | ArgNotNull2,	1, 2,	&sortHdr, NULL,		text890, CFLit_hsort},
		// Returns sorted array.
  {"htoa",		CFFunc | CFExecAH, ArgHash1,		1, 1,	NULL, NULL,		text885, CFLit_htoa},
		// Returns array.
  {"huntBack",		0, 0,					0, 0,	NULL, huntBack,		NULL, CFLit_huntBack},
		// Returns string found, or false if not found.
  {"huntForw",		CFNCount, 0,				0, 0,	NULL, huntForw,		NULL, CFLit_huntForw},
		// Returns string found, or false if not found.
  {"hvalues",		CFFunc | CFExecAH, ArgHash1,		1, 1,	NULL, NULL,		text885, CFLit_hvalues},
		// Returns array.
  {"indentLine",	CFEdit, ArgInt1,			1, 1,	NULL, NULL,		text892, CFLit_indentLine},
  {"indentRegion",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_indentRegion},
  {"index",		CFFunc, ArgInt2 | ArgMay | ArgNotNull3,	2, 3,	&indexHdr, NULL,	text818, CFLit_index},
		// Returns position of pattern in string, or nil if not found.
  {"insert",		CFFunc | CFEdit | CFNoLoad, 0,		1, -1,	NULL, NULL,		text809, CFLit_insert},
		// Returns text inserted.
  {"insertBuf",		CFEdit | CFNoLoad, 0,			1, 1,	NULL, insertBuf,	text83, CFLit_insertBuf},
		// Returns name of buffer.
  {"insertFile",	CFEdit | CFNoLoad, 0,			1, 1,	NULL, insertFile,	text99, CFLit_insertFile},
		// Returns filename.
  {"insertPipe",	CFEdit | CFNoLoad, 0,			1, -1,	NULL, NULL,		text809, CFLit_insertPipe},
		// Returns false if failure.
  {"insertSpace",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_insertSpace},
  {"insertf",		CFFunc | CFEdit, 0,			1, -1,	NULL, NULL,		text831, CFLit_insertf},
		// Returns text inserted.
  {"inserti",		CFEdit | CFNCount, 0,			0, 0,	NULL, inserti,		NULL, CFLit_inserti},
  {"interactive?",	CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_interactiveQ},
		// Returns true if script is being executed interactively.
  {"isBuf?",		CFFunc, ArgNotNull1,			1, 1,	NULL, isBufQ,		text97, CFLit_isBufQ},
		// Returns Boolean result.
  {"isClass?",		CFFunc, ArgNotNull1 | ArgInt2 | ArgMay | ArgNotNull2,
								2, 2,	NULL, NULL,		text860, CFLit_isClassQ},
		// Returns true if a character is in a given class.
  {"isConstVar?",	CFFunc, ArgNotNull1,			1, 1,	NULL, isConstVarQ,	text97, CFLit_isConstVarQ},
		// Returns Boolean result.
  {"isExec?",		CFFunc, ArgNotNull1,			1, 1,	NULL, isExecQ,		text97, CFLit_isExecQ},
		// Returns keyword result if found, otherwise nil.
  {"isMacro?",		CFFunc, 0,				1, 1,	NULL, NULL,		text97, CFLit_isMacroQ},
		// Returns Boolean result.
  {"isMark?",		CFFunc, ArgInt1,			1, 1,	NULL, isMarkQ,		text834, CFLit_isMarkQ},
		// Returns Boolean result.
  {"isMode?",		CFFunc, ArgNotNull1,			1, 1,	NULL, isModeQ,		text97, CFLit_isModeQ},
		// Returns keyword result if found, otherwise nil.
  {"isModeGrp?",	CFFunc, ArgNotNull1,			1, 1,	NULL, isModeGrpQ,	text97, CFLit_isModeGrpQ},
		// Returns Boolean result.
  {"isShortcut?",	CFFunc, ArgInt1,			1, 1,	NULL, NULL,		text843, CFLit_isShortcutQ},
		// Returns macro name or nil.
  {"join",		CFFunc | CFShortLoad, ArgNil1,		2, -1,	NULL, NULL,		text819, CFLit_join},
		// Returns string result.
  {"joinLines",		CFEdit, ArgNil1,			1, 1,	NULL, joinLines,	text829, CFLit_joinLines},
  {"joinWind",		0, 0,					0, 1,	&djWindHdr, NULL,	text844, CFLit_joinWind},
  {"keyPending?",	CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_keyPendingQ},
		// Returns true if type-ahead key(s) pending.
  {"kill",		CFFunc, ArgInt1,			1, 1,	NULL, NULL,		text820, CFLit_kill},
  {"killFencedRegion",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_killFencedRegion},
  {"killLine",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_killLine},
  {"killRegion",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_killRegion},
  {"killToBreak",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_killToBreak},
  {"killWord",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_killWord},
  {"lastBuf",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_lastBuf},
		// Returns name of buffer.
  {"lastScreen",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_lastScreen},
		// Returns screen number.
  {"length",		CFFunc, ArgArray1 | ArgHash1 | ArgMay,	1, 1,	NULL, NULL,		text812, CFLit_length},
		// Returns string or array length.
  {"let",		CFTerm, 0,				0, 0,	NULL, let,		NULL, CFLit_let},
  {"linkFile",		0, ArgNotNull1 | ArgNotNull2,		2, 3,	&linkFileHdr, linkFile,	text880, CFLit_linkFile},
		// Returns new filename if successful.
  {"loadMacro",		CFFunc, ArgNotNull1 | ArgNotNull2 | ArgNotNull3,
								2, 3,	&loadMacroHdr, loadMacro, text848, CFLit_loadMacro},
  {"lowerCaseLine",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_lowerCaseLine},
  {"lowerCaseRegion",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_lowerCaseRegion},
  {"lowerCaseStr",	CFFunc, 0,				1, 1,	NULL, NULL,		text811, CFLit_lowerCaseStr},
		// Returns string result.
  {"lowerCaseWord",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_lowerCaseWord},
  {"markBuf",		CFNoLoad, 0,				0, 1,	NULL, markBuf,		text851, CFLit_markBuf},
  {"message",		CFFunc | CFNoLoad, 0,			1, -1,	&msgHdr, message,	text861, CFLit_message},
		// Returns Boolean value.
  {"metaPrefix", CFHidden | CFPrefix | CFBind1 | CFPerm, 0,	0, 0,	NULL, NULL,		NULL, CFLit_metaPrefix},
  {"mkdir", 		0, ArgNotNull1 | ArgInt2,		1, 2,	NULL, NULL,		text866, CFLit_mkdir},
		// Returns absolute pathname of directory created.
  {"modeActive?",	CFFunc, ArgNotNull1 | ArgNotNull2,	1, 2,	NULL, modeActiveQ,	text802, CFLit_modeActiveQ},
		// Returns true if mode active, otherwise false.
  {"moveWindDown",	CFNCount, 0,				0, 0,	NULL, NULL,		NULL, CFLit_moveWindDown},
  {"moveWindUp",	CFNCount, 0,				0, 0,	NULL, moveWindUp,	NULL, CFLit_moveWindUp},
  {"narrowBuf",		0, 0,					0, 0,	NULL, narrowBuf,	NULL, CFLit_narrowBuf},
  {"negativeArg",	CFHidden | CFBind1 | CFUniq, 0,		0, 0,	NULL, NULL,		NULL, CFLit_negativeArg},
  {"newline",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_newline},
  {"newlineI",		CFEdit | CFNCount, 0,			0, 0,	NULL, newlineI,		NULL, CFLit_newlineI},
  {"nextBuf",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_nextBuf},
		// Returns name of buffer.
  {"nextScreen",	CFNCount, 0,				0, 0,	NULL, NULL,		NULL, CFLit_nextScreen},
  {"nextWind",		0, 0,					0, 0,	NULL, nextWind,		NULL, CFLit_nextWind},
  {"nil?",		CFFunc, ArgAny1,	 		1, 1,	NULL, NULL,		text812, CFLit_nilQ},
		// Returns true if expression is nil.
  {"null?",		CFFunc, 0,				1, 1,	NULL, NULL,		text811, CFLit_nullQ},
		// Returns true if null string.
  {"numeric?",		CFFunc, ArgInt2,			1, 2,	NULL, NULL,		text883, CFLit_numericQ},
		// Returns true if numeric literal.
  {"onlyWind",		0, 0,					0, 0,	NULL, onlyWind,		NULL, CFLit_onlyWind},
  {"openLine",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_openLine},
  {"openLineI",		CFEdit | CFNCount, 0,			0, 0,	NULL, openLineI,	NULL, CFLit_openLineI},
  {"ord",		CFFunc, 0,				1, 1,	NULL, NULL,		text811, CFLit_ord},
		// Returns ordinal value of first character of a string.
  {"outdentLine",	CFEdit, ArgInt1,			1, 1,	NULL, NULL,		text892, CFLit_outdentLine},
  {"outdentRegion",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_outdentRegion},
  {"overwriteChar",	CFFunc | CFEdit | CFNoLoad, 0,		1, -1,	NULL, NULL,		text809, CFLit_overwriteChar},
		// Returns new text.
  {"overwriteCol",	CFFunc | CFEdit | CFNoLoad, 0,		1, -1,	NULL, NULL,		text809, CFLit_overwriteCol},
		// Returns new text.
  {"pathname",		CFFunc, ArgArray1 | ArgMay | ArgPath,	1, 1,	NULL, absPathname,	text858, CFLit_pathname},
		// Returns absolute pathname, or array of absolute pathnames.
  {"pause",		CFFunc, ArgInt1,			1, 1,	NULL, NULL,		text820, CFLit_pause},
  {"pipeBuf",		CFEdit | CFNoLoad, 0,			1, -1,	NULL, NULL,		text809, CFLit_pipeBuf},
		// Returns false if failure.
  {"popBuf",		0, ArgNotNull1,				1, 2,	&popHdr, NULL,		text842, CFLit_popBuf},
		// Returns name of buffer.
  {"popFile",		0, ArgNotNull1 | ArgPath,		1, 2,	&popHdr, NULL,		text870, CFLit_popFile},
		// Returns name of buffer.
  {"prefix1",	CFHidden | CFPrefix | CFBind1 | CFPerm, 0,	0, 0,	NULL, NULL,		NULL, CFLit_prefix1},
  {"prefix2",	CFHidden | CFPrefix | CFBind1 | CFPerm, 0,	0, 0,	NULL, NULL,		NULL, CFLit_prefix2},
  {"prefix3",	CFHidden | CFPrefix | CFBind1 | CFPerm, 0,	0, 0,	NULL, NULL,		NULL, CFLit_prefix3},
  {"prevBuf",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_prevBuf},
		// Returns name of buffer.
  {"prevScreen",	CFNCount, 0,				0, 0,	NULL, NULL,		NULL, CFLit_prevScreen},
  {"prevWind",		0, 0,					0, 0,	NULL, prevWind,		NULL, CFLit_prevWind},
  {"print",		CFFunc | CFNoLoad, 0,			1, -1,	NULL, NULL,		text809, CFLit_print},
  {"printf",		CFFunc, 0,				1, -1,	NULL, NULL,		text831, CFLit_printf},
  {"prompt",		CFFunc, ArgNil1 | ArgNotNull1 | ArgNil2 | ArgNotNull2 | ArgNIS3,
								0, 3,	NULL, userPrompt,	text823, CFLit_prompt},
		// Returns response read from keyboard.
  {"pwd",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_pwd},
		// Returns absolute pathname of current directory.
  {"queryReplace",	CFEdit, ArgNotNull1 | ArgNil2,		2, 2,	NULL, NULL,		text810, CFLit_queryReplace},
		// Returns false if search stopped prematurely by user, otherwise true.
  {"quickExit",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_quickExit},
  {"quote",		CFFunc, ArgAny1,	 		1, 1,	NULL, NULL,		text812, CFLit_quote},
		// Returns quoted expression.
  {"quoteChar",		CFBind1 | CFUniq | CFEdit, 0,		0, 0,	NULL, quoteChar,	NULL, CFLit_quoteChar},
  {"rand",		CFFunc, ArgInt1,			1, 1,	NULL, NULL,		text820, CFLit_rand},
		// Returns pseudo-random integer.
  {"readFile",		CFNoLoad, 0,				1, 1,	NULL, readFile,		text99, CFLit_readFile},
		// Returns name of buffer.
  {"readPipe",		CFNoLoad, 0,				1, -1,	NULL, NULL,		text809, CFLit_readPipe},
		// Returns name of buffer, or false if failure.
  {"reframeWind",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_reframeWind},
  {"renameBuf",		CFNoLoad, 0,				2, 2,	NULL, renameBuf,	text849, CFLit_renameBuf},
		// Returns name of new buffer.
  {"renameFile",	0, ArgNotNull1 | ArgNotNull2,		2, 3,	&renameFileHdr, renameFile, text881, CFLit_renameFile},
		// Returns new filename if successful, otherwise nil.
  {"renameMacro",	CFNoLoad, 0,				2, 2,	NULL, renameMacro,	text849, CFLit_renameMacro},
		// Returns name of new macro.
  {"replace",		CFEdit, ArgNotNull1 | ArgNil2,		2, 2,	NULL, NULL,		text810, CFLit_replace},
		// Returns false if fewer than n replacements were made, otherwise true.
  {"resetTerm",		0, 0,					0, 0,	NULL, resetTerm,	NULL, CFLit_resetTerm},
  {"resizeWind",	0, 0,					0, 0,	NULL, resizeWind,	NULL, CFLit_resizeWind},
  {"restoreBuf",	CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_restoreBuf},
		// Returns name of buffer.
  {"restoreScreen",	CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_restoreScreen},
  {"restoreWind",	CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_restoreWind},
  {"revertYank",	0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_revertYank},
  {"ringSize",		CFNoLoad, 0,				1, 2,	NULL, ringSize,		text875, CFLit_ringSize},
		// Returns array of form [count, size].
  {"rmdir", 		0, ArgNotNull1,				1, 1,	NULL, NULL,		text837, CFLit_rmdir},
		// Returns absolute pathname of directory removed.
  {"run",		CFPerm | CFSpecArgs | CFNoLoad, 0,	1, 1,	NULL, run,		text856, CFLit_run},
		// Returns execution result.  Note: CFPerm used here as last resort method to exit program interactively if all
		// possible keys were unbound.
  {"saveBuf",		CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_saveBuf},
		// Returns name of buffer.
  {"saveFile",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_saveFile},
  {"saveScreen",	CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_saveScreen},
  {"saveWind",		CFFunc, 0,				0, 0,	NULL, NULL,		NULL, CFLit_saveWind},
  {"scratchBuf",	0, 0,					0, 0,	NULL, scratchBuf,	NULL, CFLit_scratchBuf},
		// Returns name of buffer.
  {"searchBack",	0, ArgNotNull1,				1, 1,	NULL, searchBack,	text805, CFLit_searchBack},
		// Returns string found, or false if not found.
  {"searchForw",	0, ArgNotNull1,				1, 1,	NULL, searchForw,	text805, CFLit_searchForw},
		// Returns string found, or false if not found.
  {"selectBuf",		CFNoLoad, 0,				1, 1,	NULL, selectBuf,	text83, CFLit_selectBuf},
		// Returns name of buffer.
  {"selectMacro",	CFNoLoad, 0,				1, 1,	NULL, selectMacro,	text97, CFLit_selectMacro},
  {"selectScreen",	CFNoLoad, 0,				1, 1,	NULL, selectScreen,	text820, CFLit_selectScreen},
  {"selectText",	CFFunc, ArgInt1,			1, 1,	NULL, selectText,	text820, CFLit_selectText},
		// Returns number of lines selected.
  {"selectWind",	CFNoLoad, 0,				1, 1,	NULL, selectWind,	text820, CFLit_selectWind},
  {"setBufFile",	CFShortLoad, ArgNil1 | ArgPath,		2, 2,	NULL, setBufFile,	text850, CFLit_setBufFile},
		// Returns two-element array containing new buffer name and new filename.
  {"setColorPair",	CFFunc, ArgInt1 | ArgInt2 | ArgInt3,	3, 3,	NULL, setColorPair,	text864, CFLit_setColorPair},
		// Returns color pair number.
  {"setDefault",	CFFunc, ArgInt2,			2, 2,	NULL, setDefault,	text882, CFLit_setDefault},
  {"setDispColor",	CFFunc, ArgNotNull1 | ArgArray2 | ArgNil2 | ArgMay,
								2, 2,	NULL, setDispColor,	text865, CFLit_setDispColor},
  {"setHook",	CFFunc | CFSpecArgs | CFShortLoad, ArgNotNull1,	2, 2,	NULL, setHook,		text832, CFLit_setHook},
  {"setMark",		CFNoLoad, 0,				0, 1,	NULL, setMark,		text851, CFLit_setMark},
  {"setShortcut",	CFNoLoad, 0,				1, 2,	NULL, setShortcut,	text803, CFLit_setShortcut},
  {"setWrapCol",	CFNoLoad, 0,				0, 1,	NULL, NULL,		text821, CFLit_setWrapCol},
  {"seti",		0, ArgInt1 | ArgInt3,			1, 3,	NULL, seti,		text813, CFLit_seti},
  {"shQuote",		CFFunc, ArgNIS1,			1, 1,	NULL, NULL,		text812, CFLit_shQuote},
		// Returns quoted string.
  {"shell",		0, 0,					0, 0,	NULL, shellCLI,		NULL, CFLit_shell},
		// Returns false if failure.
  {"shellCmd",		CFNoLoad, 0,				1, -1,	&shellCmdHdr, NULL,	text861, CFLit_shellCmd},
		// Returns false if failure.
  {"shiftLine",		CFEdit, ArgInt1,			0, 0,	NULL, NULL,		NULL, CFLit_shiftLine},
  {"showAliases",	0, ArgNil1,				0, 1,	NULL, showAliases,	text893, CFLit_showAliases},
  {"showBindings",	0, ArgNil1,				0, 2,	&showBindHdr, showBindings,text873, CFLit_showBindings},
  {"showBuffers",	0, 0,					0, 1,	&showBufHdr, showBuffers,text844, CFLit_showBuffers},
  {"showColors",	0, 0,					0, 0,	NULL, showColors,	NULL, CFLit_showColors},
  {"showConstVar",	0, ArgNotNull1,				1, 1,	NULL, showConstVar,	text97, CFLit_showConstVar},
  {"showConstVars",	0, ArgNil1,				0, 2,	&cfcvHdr, showConstVars,text873, CFLit_showConstVars},
  {"showFence",		0, ArgInt1,				0, 1,	NULL, showFence,	text857, CFLit_showFence},
  {"showHooks",		0, 0,					0, 0,	NULL, showHooks,	NULL, CFLit_showHooks},
  {"showKey",		CFTerm, 0,				0, 0,	NULL, showKey,		NULL, CFLit_showKey},
  {"showMacros",	0, 0,					0, 0,	NULL, showMacros,	NULL, CFLit_showMacros},
  {"showMarks",		0, 0,					0, 0,	NULL, showMarks,	NULL, CFLit_showMarks},
  {"showModes",		0, 0,					0, 0,	NULL, showModes,	NULL, CFLit_showModes},
  {"showPoint",		CFTerm, 0,				0, 0,	NULL, showPoint,	NULL, CFLit_showPoint},
#if MMDebug & Debug_ShowRE
  {"showRegexp",	0, 0,					0, 0,	NULL, showRegexp,	NULL, CFLit_showRegexp},
#endif
  {"showRing",		CFNoLoad, 0,				1, 1,	NULL, showRing,		text874, CFLit_showRing},
  {"showRoutines",	0, ArgNil1,				0, 2,	&cfHdr, showRoutines,	text873, CFLit_showRoutines},
  {"showScreens",	0, 0,					0, 0,	NULL, showScreens,	NULL, CFLit_showScreens},
  {"shrinkWind",	CFNCount, 0,				0, 0,	NULL, NULL,		NULL, CFLit_shrinkWind},
  {"sortRegion",	CFEdit, ArgNotNull1,			0, 1,	&sortHdr, sortRegion,	text844, CFLit_sortRegion},
  {"space",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_space},
  {"split",		CFFunc, ArgNIS2 | ArgInt3,		2, 3,	NULL, strSplit,		text838, CFLit_split},
		// Returns array.
  {"splitWind",		0, 0,					0, 0,	NULL, NULL,		NULL, CFLit_splitWind},
		// Returns ordinal number of new window not containing point.
  {"sprintf",		CFFunc, 0,				1, -1,	NULL, NULL,		text831, CFLit_sprintf},
  {"stat?",		CFFunc, ArgPath,			2, 2,	NULL, NULL,		text835, CFLit_statQ},
		// Returns Boolean result.
  {"strMatchGrp",	CFFunc, ArgInt1,			0, 1,	NULL, NULL,		text821, CFLit_strMatchGrp},
		// Returns group match string for =~ op and index, or null if none.
  {"strPop",		CFFunc | CFSpecArgs | CFNoLoad, 0,	2, 2,	NULL, NULL,		text839, CFLit_strPop},
		// Returns popped value, or nil if none left.
  {"strPush",		CFFunc | CFSpecArgs | CFNoLoad, 0,	3, 3,	NULL, NULL,		text840, CFLit_strPush},
		// Returns pushed value.
  {"strShift",		CFFunc | CFSpecArgs | CFNoLoad, 0,	2, 2,	NULL, NULL,		text839, CFLit_strShift},
		// Returns shifted value, or nil if none left.
  {"strUnshift",	CFFunc | CFSpecArgs | CFNoLoad, 0,	3, 3,	NULL, NULL,		text840, CFLit_strUnshift},
		// Returns unshifted value.
  {"strfit",		CFFunc, ArgInt2,			2, 2,	NULL, NULL,		text827, CFLit_strfit},
		// Returns compressed string.
  {"strip",		CFFunc, 0,				1, 1,	NULL, NULL,		text811, CFLit_strip},
		// Returns whitespace-stripped string.
  {"sub",		CFFunc, ArgNil3,			3, 3,	NULL, substitute,	text817, CFLit_sub},
		// Returns string result.
  {"subline",		CFFunc, ArgInt1 | ArgInt2,		1, 2,	NULL, NULL,		text825, CFLit_subline},
		// Returns string result.
  {"substr",		CFFunc, ArgInt2 | ArgInt3,		2, 3,	NULL, NULL,		text826, CFLit_substr},
		// Returns string result.
  {"suspend",		0, 0,					0, 0,	NULL, suspendMM,	NULL, CFLit_suspend},
  {"swapMark",		CFNoLoad, 0,				0, 1,	NULL, swapMark,		text834, CFLit_swapMark},
  {"swapScreens",	0, ArgInt1 | ArgInt2,			2, 2,	NULL, swapScreens,	text867, CFLit_swapScreens},
  {"tab",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_tab},
  {"titleCaseLine",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_titleCaseLine},
  {"titleCaseRegion",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_titleCaseRegion},
  {"titleCaseStr",	CFFunc, 0,				1, 1,	NULL, titleCaseStr,	text811, CFLit_titleCaseStr},
		// Returns string result.
  {"titleCaseWord",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_titleCaseWord},
  {"toint",		CFFunc, ArgInt1 | ArgInt2 | ArgMay,	1, 2,	NULL, NULL,		text883, CFLit_toint},
		// Returns integer result.
  {"tostr",		CFFunc, ArgAny1 | ArgNil2,		1, 3,	&tostrHdr, toString,	text841, CFLit_tostr},
		// Returns string result.
  {"tr",		CFFunc, ArgNil3,			3, 3,	NULL, NULL,		text817, CFLit_tr},
		// Returns translated string.
  {"traverseLine",	0, 0,					0, 0,	NULL, traverseLine,	NULL, CFLit_traverseLine},
  {"trimLine",		CFEdit, 0,				0, 0,	NULL, trimLine,		NULL, CFLit_trimLine},
  {"truncBuf",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_truncBuf},
  {"typeof",		CFFunc, ArgAny1,	 		1, 1,	NULL, NULL,		text812, CFLit_typeof},
		// Returns type of expression as string keyword.
  {"unbindKey",		0, ArgNotNull1,				1, 1,	NULL, unbindKey,	text815, CFLit_unbindKey},
		// Returns Boolean result if script mode, otherwise nil.
  {"undelete",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_undelete},
  {"undeleteCycle",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_undeleteCycle},
  {"universalArg",	CFHidden | CFBind1 | CFUniq, 0,		0, 0,	NULL, NULL,		NULL, CFLit_universalArg},
  {"updateScreen",	CFFunc, 0,				0, 1,	&updScrnHdr, updateScrn, text844, CFLit_updateScreen},
  {"upperCaseLine",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_upperCaseLine},
  {"upperCaseRegion",	CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_upperCaseRegion},
  {"upperCaseStr",	CFFunc, 0,				1, 1,	NULL, NULL,		text811, CFLit_upperCaseStr},
		// Returns string result.
  {"upperCaseWord",	CFEdit | CFNCount, 0,			0, 0,	NULL, NULL,		NULL, CFLit_upperCaseWord},
  {"viewFile",		CFNoLoad, 0,				1, 1,	NULL, NULL,		text99, CFLit_viewFile},
		// Returns [name of buffer, "true" or "false" indicating whether the buffer was created].
  {"widenBuf",		0, 0,					0, 0,	NULL, widenBuf,		NULL, CFLit_widenBuf},
  {"wrapLine",		CFEdit, ArgNil1 | ArgNil2,		2, 2,	NULL, wrapLine,		text830, CFLit_wrapLine},
  {"wrapWord",		CFFunc | CFHook | CFEdit, 0,		0, 0,	NULL, wrapWord,		NULL, CFLit_wrapWord},
  {"writeFile",		CFNoLoad, 0,				1, 1,	NULL, NULL,		text99, CFLit_writeFile},
		// Returns filename.
  {"writeLine",		CFNoLoad, 0,				1, 1,	NULL, NULL,		text83, CFLit_writeLine},
		// Returns text written.
  {"writeRegion",	CFNoLoad, 0,				1, 1,	NULL, NULL,		text83, CFLit_writeRegion},
		// Returns text written.
  {"xPathname", 	CFFunc, ArgNotNull1 | ArgPath,		1, 2,	&xPathHdr, xPathname,	text859, CFLit_xPathname},
		// Returns pathname, or array of pathnames.
  {"xeqBuf",		CFNoLoad, 0,				1, -1,	NULL, xeqBuf,		text816, CFLit_xeqBuf},
		// Returns execution result.
  {"xeqFile",		CFNoLoad, 0,				1, -1,	NULL, xeqFile,		text869, CFLit_xeqFile},
		// Returns execution result.
  {"xeqMacro",		CFNoLoad, 0,				0, 1,	NULL, xeqMacro,		text871, CFLit_xeqMacro},
  {"yank",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_yank},
  {"yankCycle",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_yankCycle},
  {"yankDel",		CFEdit, 0,				0, 0,	NULL, NULL,		NULL, CFLit_yankDel}};
CmdFunc *pCmdFuncEnd = cmdFuncTable + elementsof(cmdFuncTable);
Hash execTable;				// Table of executable names (commands, functions, and aliases).

#endif
