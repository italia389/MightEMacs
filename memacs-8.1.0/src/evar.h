// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// evar.h	System and user variable definitions for MightEMacs.

// Definition of system constants.  Used to initialize corresponding items in system variable table.
enum sconstid {
	sc_BufFlagActive,sc_BufFlagChanged,sc_BufFlagHidden,sc_BufFlagMacro,sc_BufFlagNarrowed,sc_BufFlagPreprocd,
	sc_BufFlagTruncated,sc_EditorName,sc_EditorVersion,sc_Language,sc_ModeAutoSave,sc_ModeBackup,sc_ModeC,sc_ModeClobber,
	sc_ModeColDisp,sc_ModeEsc8Bit,sc_ModeExact,sc_ModeExtraIndent,sc_ModeHorzScroll,sc_ModeKeyEcho,sc_ModeLineDisp,
	sc_ModeMEMacs,sc_ModeMsgDisp,sc_ModeNoUpdate,sc_ModeOver,sc_ModePerl,sc_ModeReadFirst,sc_ModeReadOnly,sc_ModeRegExp,
	sc_ModeReplace,sc_ModeRuby,sc_ModeSafeSave,sc_ModeShell,sc_ModeWorkDir,sc_ModeWrap,sc_OS
	};

#ifdef DATAvar
// Table of system constants.
static Value sysconsts[] = {
	{NULL,VALINT,NULL,BFACTIVE},
	{NULL,VALINT,NULL,BFCHGD},
	{NULL,VALINT,NULL,BFHIDDEN},
	{NULL,VALINT,NULL,BFMACRO},
	{NULL,VALINT,NULL,BFNARROW},
	{NULL,VALINT,NULL,BFPREPROC},
	{NULL,VALINT,NULL,BFTRUNC},
	{NULL,VALSTR,myself,(long) myself},
	{NULL,VALSTR,version,(long) version},
	{NULL,VALSTR,language,(long) language},
	{NULL,VALINT,NULL,MDASAVE},
	{NULL,VALINT,NULL,MDBAK},
	{NULL,VALINT,NULL,MDC},
	{NULL,VALINT,NULL,MDCLOB},
	{NULL,VALINT,NULL,MDCOL},
	{NULL,VALINT,NULL,MDESC8},
	{NULL,VALINT,NULL,MDEXACT},
	{NULL,VALINT,NULL,MDXINDT},
	{NULL,VALINT,NULL,MDHSCRL},
	{NULL,VALINT,NULL,MDKECHO},
	{NULL,VALINT,NULL,MDLINE},
	{NULL,VALINT,NULL,MDMEMACS},
	{NULL,VALINT,NULL,MDMSG},
	{NULL,VALINT,NULL,MDNOUPD},
	{NULL,VALINT,NULL,MDOVER},
	{NULL,VALINT,NULL,MDPERL},
	{NULL,VALINT,NULL,MDRD1ST},
	{NULL,VALINT,NULL,MDRDONLY},
	{NULL,VALINT,NULL,MDREGEXP},
	{NULL,VALINT,NULL,MDREPL},
	{NULL,VALINT,NULL,MDRUBY},
	{NULL,VALINT,NULL,MDSAFE},
	{NULL,VALINT,NULL,MDSHELL},
	{NULL,VALINT,NULL,MDWKDIR},
	{NULL,VALINT,NULL,MDWRAP},
	{NULL,VALSTR,osname,(long) osname}
	};

// Table of system variables.  (Ones that are read-only begin with a capital letter.)
static SVar sysvars[] = {
	{"ArgCount",		sv_ArgCount,		V_RDONLY | V_INT,VLIT_ArgCount,NULL},
	{"BufCount",		sv_BufCount,		V_RDONLY | V_INT,VLIT_BufCount,NULL},
	{"BufFlagActive",	sv_BufFlagActive,	V_RDONLY | V_INT,VLIT_BufFlagActive,sysconsts + sc_BufFlagActive},
	{"BufFlagChanged",	sv_BufFlagChanged,	V_RDONLY | V_INT,VLIT_BufFlagChanged,sysconsts + sc_BufFlagChanged},
	{"BufFlagHidden",	sv_BufFlagHidden,	V_RDONLY | V_INT,VLIT_BufFlagHidden,sysconsts + sc_BufFlagHidden},
	{"BufFlagMacro",	sv_BufFlagMacro,	V_RDONLY | V_INT,VLIT_BufFlagMacro,sysconsts + sc_BufFlagMacro},
	{"BufFlagNarrowed",	sv_BufFlagNarrowed,	V_RDONLY | V_INT,VLIT_BufFlagNarrowed,sysconsts + sc_BufFlagNarrowed},
	{"BufFlagPreprocd",	sv_BufFlagPreprocd,	V_RDONLY | V_INT,VLIT_BufFlagPreprocd,sysconsts + sc_BufFlagPreprocd},
	{"BufFlagTruncated",	sv_BufFlagTruncated,	V_RDONLY | V_INT,VLIT_BufFlagTruncated,sysconsts + sc_BufFlagTruncated},
	{"BufInpDelim",		sv_BufInpDelim,		V_RDONLY,	VLIT_BufInpDelim,NULL},
	{"BufList",		sv_BufList,		V_RDONLY,	VLIT_BufList,NULL},
	{"BufOtpDelim",		sv_BufOtpDelim,		V_RDONLY,	VLIT_BufOtpDelim,NULL},
	{"BufSize",		sv_BufSize,		V_RDONLY | V_INT,VLIT_BufSize,NULL},
	{"Date",		sv_Date,		V_RDONLY,	VLIT_Date,NULL},
	{"EditorName",		sv_EditorName,		V_RDONLY,	VLIT_EditorName,sysconsts + sc_EditorName},
	{"EditorVersion",	sv_EditorVersion,	V_RDONLY,	VLIT_EditorVersion,sysconsts + sc_EditorVersion},
#if TYPEAHEAD
	{"KeyPending",		sv_KeyPending,		V_RDONLY,	VLIT_KeyPending,NULL},
#endif
	{"KillText",		sv_KillText,		V_RDONLY,	VLIT_KillText,NULL},
	{"Language",		sv_Language,		V_RDONLY,	VLIT_Language,sysconsts + sc_Language},
	{"LineLen",		sv_LineLen,		V_RDONLY | V_INT,VLIT_LineLen,NULL},
	{"Match",		sv_Match,		V_RDONLY,	VLIT_Match,NULL},
	{"ModeAutoSave",	sv_ModeAutoSave,	V_RDONLY | V_INT,VLIT_ModeAutoSave,sysconsts + sc_ModeAutoSave},
	{"ModeBackup",		sv_ModeBackup,		V_RDONLY | V_INT,VLIT_ModeBackup,sysconsts + sc_ModeBackup},
	{"ModeC",		sv_ModeC,		V_RDONLY | V_INT,VLIT_ModeC,sysconsts + sc_ModeC},
	{"ModeClobber",		sv_ModeClobber,		V_RDONLY | V_INT,VLIT_ModeClobber,sysconsts + sc_ModeClobber},
	{"ModeColDisp",		sv_ModeColDisp,		V_RDONLY | V_INT,VLIT_ModeColDisp,sysconsts + sc_ModeColDisp},
	{"ModeEsc8Bit",		sv_ModeEsc8Bit,		V_RDONLY | V_INT,VLIT_ModeEsc8Bit,sysconsts + sc_ModeEsc8Bit},
	{"ModeExact",		sv_ModeExact,		V_RDONLY | V_INT,VLIT_ModeExact,sysconsts + sc_ModeExact},
	{"ModeExtraIndent",	sv_ModeExtraIndent,	V_RDONLY | V_INT,VLIT_ModeExtraIndent,sysconsts + sc_ModeExtraIndent},
	{"ModeHorzScroll",	sv_ModeHorzScroll,	V_RDONLY | V_INT,VLIT_ModeHorzScroll,sysconsts + sc_ModeHorzScroll},
	{"ModeKeyEcho",		sv_ModeKeyEcho,		V_RDONLY | V_INT,VLIT_ModeKeyEcho,sysconsts + sc_ModeKeyEcho},
	{"ModeLineDisp",	sv_ModeLineDisp,	V_RDONLY | V_INT,VLIT_ModeLineDisp,sysconsts + sc_ModeLineDisp},
	{"ModeMEMacs",		sv_ModeMEMacs,		V_RDONLY | V_INT,VLIT_ModeMEMacs,sysconsts + sc_ModeMEMacs},
	{"ModeMsgDisp",		sv_ModeMsgDisp,		V_RDONLY | V_INT,VLIT_ModeMsgDisp,sysconsts + sc_ModeMsgDisp},
	{"ModeNoUpdate",	sv_ModeNoUpdate,	V_RDONLY | V_INT,VLIT_ModeNoUpdate,sysconsts + sc_ModeNoUpdate},
	{"ModeOver",		sv_ModeOver,		V_RDONLY | V_INT,VLIT_ModeOver,sysconsts + sc_ModeOver},
	{"ModePerl",		sv_ModePerl,		V_RDONLY | V_INT,VLIT_ModePerl,sysconsts + sc_ModePerl},
	{"ModeReadFirst",	sv_ModeReadFirst,	V_RDONLY | V_INT,VLIT_ModeReadFirst,sysconsts + sc_ModeReadFirst},
	{"ModeReadOnly",	sv_ModeReadOnly,	V_RDONLY | V_INT,VLIT_ModeReadOnly,sysconsts + sc_ModeReadOnly},
	{"ModeRegExp",		sv_ModeRegExp,		V_RDONLY | V_INT,VLIT_ModeRegExp,sysconsts + sc_ModeRegExp},
	{"ModeReplace",		sv_ModeReplace,		V_RDONLY | V_INT,VLIT_ModeReplace,sysconsts + sc_ModeReplace},
	{"ModeRuby",		sv_ModeRuby,		V_RDONLY | V_INT,VLIT_ModeRuby,sysconsts + sc_ModeRuby},
	{"ModeSafeSave",	sv_ModeSafeSave,	V_RDONLY | V_INT,VLIT_ModeSafeSave,sysconsts + sc_ModeSafeSave},
	{"ModeShell",		sv_ModeShell,		V_RDONLY | V_INT,VLIT_ModeShell,sysconsts + sc_ModeShell},
	{"ModeWorkDir",		sv_ModeWorkDir,		V_RDONLY | V_INT,VLIT_ModeWorkDir,sysconsts + sc_ModeWorkDir},
	{"ModeWrap",		sv_ModeWrap,		V_RDONLY | V_INT,VLIT_ModeWrap,sysconsts + sc_ModeWrap},
	{"OS",			sv_OS,			V_RDONLY,	VLIT_OS,sysconsts + sc_OS},
	{"RegionText",		sv_RegionText,		V_RDONLY,	VLIT_RegionText,NULL},
	{"ReturnMsg",		sv_ReturnMsg,		V_RDONLY,	VLIT_ReturnMsg,NULL},
	{"RunFile",		sv_RunFile,		V_RDONLY,	VLIT_RunFile,NULL},
	{"RunName",		sv_RunName,		V_RDONLY,	VLIT_RunName,NULL},
	{"TermCols",		sv_TermCols,		V_RDONLY | V_INT,VLIT_TermCols,NULL},
	{"TermRows",		sv_TermRows,		V_RDONLY | V_INT,VLIT_TermRows,NULL},
	{"WindCount",		sv_WindCount,		V_RDONLY | V_INT,VLIT_WindCount,NULL},
	{"WorkDir",		sv_WorkDir,		V_RDONLY,	VLIT_WorkDir,NULL},

	{"argIndex",		sv_argIndex,		V_INT,		VLIT_argIndex,NULL},
	{"autoSave",		sv_autoSave,		V_INT,		VLIT_autoSave,NULL},
	{"bufFile",		sv_bufFile,		0,		VLIT_bufFile,NULL},
	{"bufFlags",		sv_bufFlags,		V_INT,		VLIT_bufFlags,NULL},
	{"bufLineNum",		sv_bufLineNum,		V_INT,		VLIT_bufLineNum,NULL},
	{"bufModes",		sv_bufModes,		V_INT,		VLIT_bufModes,NULL},
	{"bufName",		sv_bufName,		0,		VLIT_bufName,NULL},
	{"defModes",		sv_defModes,		V_INT,		VLIT_defModes,NULL},
#if COLOR
	{"desktopColor",	sv_desktopColor,	0,		VLIT_desktopColor,NULL},
#endif
	{"enterBufHook",	sv_enterBufHook,	0,		VLIT_enterBufHook,NULL},
	{"execPath",		sv_execPath,		0,		VLIT_execPath,NULL},
	{"exitBufHook",		sv_exitBufHook,		0,		VLIT_exitBufHook,NULL},
	{"fencePause",		sv_fencePause,		V_INT,		VLIT_fencePause,NULL},
	{"globalModes",		sv_globalModes,		V_INT,		VLIT_globalModes,NULL},
	{"hardTabSize",		sv_hardTabSize,		V_INT,		VLIT_hardTabSize,NULL},
	{"helpHook",		sv_helpHook,		0,		VLIT_helpHook,NULL},
	{"horzJump",		sv_horzJump,		V_INT,		VLIT_horzJump,NULL},
	{"horzScrollCol",	sv_horzScrollCol,	V_INT,		VLIT_horzScrollCol,NULL},
	{"inpDelim",		sv_inpDelim,		V_ESCDELIM,	VLIT_inpDelim,NULL},
	{"keyMacro",		sv_keyMacro,		V_ESCDELIM,	VLIT_keyMacro,NULL},
	{"lastKeySeq",		sv_lastKeySeq,		V_ESCDELIM,	VLIT_lastKeySeq,NULL},
	{"lineChar",		sv_lineChar,		V_INT,		VLIT_lineChar,NULL},
	{"lineCol",		sv_lineCol,		V_INT,		VLIT_lineCol,NULL},
	{"lineOffset",		sv_lineOffset,		V_INT,		VLIT_lineOffset,NULL},
	{"lineText",		sv_lineText,		0,		VLIT_lineText,NULL},
	{"maxLoop",		sv_maxLoop,		V_INT,		VLIT_maxLoop,NULL},
	{"maxRecursion",	sv_maxRecursion,	V_INT,		VLIT_maxRecursion,NULL},
	{"modeHook",		sv_modeHook,		0,		VLIT_modeHook,NULL},
	{"otpDelim",		sv_otpDelim,		V_ESCDELIM,	VLIT_otpDelim,NULL},
	{"pageOverlap",		sv_pageOverlap,		V_INT,		VLIT_pageOverlap,NULL},
#if COLOR
	{"palette",		sv_palette,		0,		VLIT_palette,NULL},
#endif
	{"postKeyHook",		sv_postKeyHook,		0,		VLIT_postKeyHook,NULL},
	{"preKeyHook",		sv_preKeyHook,		0,		VLIT_preKeyHook,NULL},
	{"randNumSeed",		sv_randNumSeed,		V_INT,		VLIT_randNumSeed,NULL},
	{"readHook",		sv_readHook,		0,		VLIT_readHook,NULL},
	{"replace",		sv_replace,		V_ESCDELIM,	VLIT_replace,NULL},
	{"screenNum",		sv_screenNum,		V_INT,		VLIT_screenNum,NULL},
	{"search",		sv_search,		V_ESCDELIM,	VLIT_search,NULL},
	{"searchDelim",		sv_searchDelim,		0,		VLIT_searchDelim,NULL},
	{"showModes",		sv_showModes,		V_INT,		VLIT_showModes,NULL},
	{"softTabSize",		sv_softTabSize,		V_INT,		VLIT_softTabSize,NULL},
	{"travJumpSize",	sv_travJumpSize,	V_INT,		VLIT_travJumpSize,NULL},
	{"vertJump",		sv_vertJump,		V_INT,		VLIT_vertJump,NULL},
	{"windLineNum",		sv_windLineNum,		V_INT,		VLIT_windLineNum,NULL},
	{"windNum",		sv_windNum,		V_INT,		VLIT_windNum,NULL},
	{"windSize",		sv_windSize,		V_INT,		VLIT_windSize,NULL},
	{"wordChars",		sv_wordChars,		0,		VLIT_wordChars,NULL},
	{"wrapCol",		sv_wrapCol,		V_INT,		VLIT_wrapCol,NULL},
	{"wrapHook",		sv_wrapHook,		0,		VLIT_wrapHook,NULL},
	{"writeHook",		sv_writeHook,		0,		VLIT_writeHook,NULL},
	{NULL,			-1,			0,		NULL,NULL}
	};

#define NSVARS	(sizeof(sysvars) / sizeof(SVar)) - 1

UVar *gvarsheadp = NULL;		// Head of global variable list.
UVar *lvarsheadp = NULL;		// Head of local (macro) variable list.

#else
extern UVar *lvarsheadp;
#endif

// External function declarations for files that include this one.
extern int derefn(Value *valp,char *namep);
extern int derefv(Value *valp,VDesc *vdp);
extern int findvar(char *namep,int op,VDesc *vdp);
extern MacArg *marg(MacArgList *malp,ushort argnum);
extern int putvar(Value *valp,VDesc *vdp);
extern void uvarclean(UVar *vstackp);
