// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// evar.h	System and user variable definitions for MightEMacs.

// Definition of system constants.  Used to initialize corresponding items in system variable table.
enum sconstid {
	sc_BufFlagActive,sc_BufFlagChanged,sc_BufFlagHidden,sc_BufFlagMacro,sc_BufFlagNarrowed,sc_BufFlagPreprocd,
	sc_BufFlagTruncated,sc_EditorName,sc_EditorVersion,sc_Language,sc_ModeAutoSave,sc_ModeBackup,sc_ModeC,sc_ModeClobber,
	sc_ModeColDisp,sc_ModeEsc8Bit,sc_ModeExact,sc_ModeExtraIndent,sc_ModeHorzScroll,sc_ModeLineDisp,sc_ModeMEMacs,
	sc_ModeMsgDisp,sc_ModeNoUpdate,sc_ModeOver,sc_ModePerl,sc_ModeReadOnly,sc_ModeRegexp,sc_ModeReplace,sc_ModeRuby,
	sc_ModeSafeSave,sc_ModeShell,sc_ModeWorkDir,sc_ModeWrap,sc_OS
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
	{NULL,VALSTR,Myself,(long) Myself},
	{NULL,VALSTR,Version,(long) Version},
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
	{NULL,VALINT,NULL,MDLINE},
	{NULL,VALINT,NULL,MDMEMACS},
	{NULL,VALINT,NULL,MDMSG},
	{NULL,VALINT,NULL,MDNOUPD},
	{NULL,VALINT,NULL,MDOVER},
	{NULL,VALINT,NULL,MDPERL},
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
	{"$ArgCount",		sv_ArgCount,		V_RDONLY | V_INT,VLIT_ArgCount,NULL},
	{"$BufCount",		sv_BufCount,		V_RDONLY | V_INT,VLIT_BufCount,NULL},
	{"$BufFlagActive",	sv_BufFlagActive,	V_RDONLY | V_INT,VLIT_BufFlagActive,sysconsts + sc_BufFlagActive},
	{"$BufFlagChanged",	sv_BufFlagChanged,	V_RDONLY | V_INT,VLIT_BufFlagChanged,sysconsts + sc_BufFlagChanged},
	{"$BufFlagHidden",	sv_BufFlagHidden,	V_RDONLY | V_INT,VLIT_BufFlagHidden,sysconsts + sc_BufFlagHidden},
	{"$BufFlagMacro",	sv_BufFlagMacro,	V_RDONLY | V_INT,VLIT_BufFlagMacro,sysconsts + sc_BufFlagMacro},
	{"$BufFlagNarrowed",	sv_BufFlagNarrowed,	V_RDONLY | V_INT,VLIT_BufFlagNarrowed,sysconsts + sc_BufFlagNarrowed},
	{"$BufFlagPreprocd",	sv_BufFlagPreprocd,	V_RDONLY | V_INT,VLIT_BufFlagPreprocd,sysconsts + sc_BufFlagPreprocd},
	{"$BufFlagTruncated",	sv_BufFlagTruncated,	V_RDONLY | V_INT,VLIT_BufFlagTruncated,sysconsts + sc_BufFlagTruncated},
	{"$BufInpDelim",	sv_BufInpDelim,		V_RDONLY,	VLIT_BufInpDelim,NULL},
	{"$BufLen",		sv_BufLen,		V_RDONLY | V_INT,VLIT_BufLen,NULL},
	{"$BufList",		sv_BufList,		V_RDONLY,	VLIT_BufList,NULL},
	{"$BufOtpDelim",	sv_BufOtpDelim,		V_RDONLY,	VLIT_BufOtpDelim,NULL},
	{"$BufSize",		sv_BufSize,		V_RDONLY | V_INT,VLIT_BufSize,NULL},
	{"$Date",		sv_Date,		V_RDONLY,	VLIT_Date,NULL},
	{"$EditorName",		sv_EditorName,		V_RDONLY,	VLIT_EditorName,sysconsts + sc_EditorName},
	{"$EditorVersion",	sv_EditorVersion,	V_RDONLY,	VLIT_EditorVersion,sysconsts + sc_EditorVersion},
#if TYPEAHEAD
	{"$KeyPending",		sv_KeyPending,		V_RDONLY,	VLIT_KeyPending,NULL},
#endif
	{"$KillText",		sv_KillText,		V_RDONLY,	VLIT_KillText,NULL},
	{"$Language",		sv_Language,		V_RDONLY,	VLIT_Language,sysconsts + sc_Language},
	{"$LineLen",		sv_LineLen,		V_RDONLY | V_INT,VLIT_LineLen,NULL},
	{"$Match",		sv_Match,		V_RDONLY,	VLIT_Match,NULL},
	{"$ModeAutoSave",	sv_ModeAutoSave,V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_ASAVE),sysconsts + sc_ModeAutoSave},
	{"$ModeBackup",		sv_ModeBackup,	V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_BAK),sysconsts + sc_ModeBackup},
	{"$ModeC",		sv_ModeC,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_C),sysconsts + sc_ModeC},
	{"$ModeClobber",	sv_ModeClobber,	V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_CLOB),sysconsts + sc_ModeClobber},
	{"$ModeColDisp",	sv_ModeColDisp,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_COL),sysconsts + sc_ModeColDisp},
	{"$ModeEsc8Bit",	sv_ModeEsc8Bit,	V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_ESC8),sysconsts + sc_ModeEsc8Bit},
	{"$ModeExact",		sv_ModeExact,	V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_EXACT),sysconsts + sc_ModeExact},
	{"$ModeExtraIndent",	sv_ModeExtraIndent,V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_XINDT),sysconsts + sc_ModeExtraIndent},
	{"$ModeHorzScroll",	sv_ModeHorzScroll,V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_HSCRL),sysconsts + sc_ModeHorzScroll},
	{"$ModeLineDisp",	sv_ModeLineDisp,V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_LINE),sysconsts + sc_ModeLineDisp},
	{"$ModeMEMacs",		sv_ModeMEMacs,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_MEMACS),sysconsts + sc_ModeMEMacs},
	{"$ModeMsgDisp",	sv_ModeMsgDisp,	V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_MSG),sysconsts + sc_ModeMsgDisp},
	{"$ModeNoUpdate",	sv_ModeNoUpdate,V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_NOUPD),sysconsts + sc_ModeNoUpdate},
	{"$ModeOver",		sv_ModeOver,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_OVER),sysconsts + sc_ModeOver},
	{"$ModePerl",		sv_ModePerl,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_PERL),sysconsts + sc_ModePerl},
	{"$ModeReadOnly",	sv_ModeReadOnly,V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_RDONLY),sysconsts + sc_ModeReadOnly},
	{"$ModeRegexp",		sv_ModeRegexp,	V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_REGEXP),sysconsts + sc_ModeRegexp},
	{"$ModeReplace",	sv_ModeReplace,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_REPL),sysconsts + sc_ModeReplace},
	{"$ModeRuby",		sv_ModeRuby,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_RUBY),sysconsts + sc_ModeRuby},
	{"$ModeSafeSave",	sv_ModeSafeSave,V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_SAFE),sysconsts + sc_ModeSafeSave},
	{"$ModeShell",		sv_ModeShell,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_SHELL),sysconsts + sc_ModeShell},
	{"$ModeWorkDir",	sv_ModeWorkDir,	V_RDONLY | V_INT | V_MODE,(char *) (gmodeinfo + MDO_WKDIR),sysconsts + sc_ModeWorkDir},
	{"$ModeWrap",		sv_ModeWrap,	V_RDONLY | V_INT | V_MODE,(char *) (bmodeinfo + MDO_WRAP),sysconsts + sc_ModeWrap},
	{"$OS",			sv_OS,			V_RDONLY,	VLIT_OS,sysconsts + sc_OS},
	{"$RegionText",		sv_RegionText,		V_RDONLY,	VLIT_RegionText,NULL},
	{"$ReturnMsg",		sv_ReturnMsg,		V_RDONLY,	VLIT_ReturnMsg,NULL},
	{"$RunFile",		sv_RunFile,		V_RDONLY,	VLIT_RunFile,NULL},
	{"$RunName",		sv_RunName,		V_RDONLY,	VLIT_RunName,NULL},
	{"$TermCols",		sv_TermCols,		V_RDONLY | V_INT,VLIT_TermCols,NULL},
	{"$TermRows",		sv_TermRows,		V_RDONLY | V_INT,VLIT_TermRows,NULL},
	{"$WindCount",		sv_WindCount,		V_RDONLY | V_INT,VLIT_WindCount,NULL},
	{"$WindList",		sv_WindList,		V_RDONLY,	VLIT_WindList,NULL},

	{"$argIndex",		sv_argIndex,		V_INT,		VLIT_argIndex,NULL},
	{"$autoSave",		sv_autoSave,		V_INT,		VLIT_autoSave,NULL},
	{"$bufFile",		sv_bufFile,		V_NIL,		VLIT_bufFile,NULL},
	{"$bufFlags",		sv_bufFlags,		V_INT,		VLIT_bufFlags,NULL},
	{"$bufLineNum",		sv_bufLineNum,		V_INT,		VLIT_bufLineNum,NULL},
	{"$bufModes",		sv_bufModes,		V_INT,		VLIT_bufModes,NULL},
	{"$bufName",		sv_bufName,		0,		VLIT_bufName,NULL},
	{"$defModes",		sv_defModes,		V_INT,		VLIT_defModes,NULL},
#if COLOR
	{"$desktopColor",	sv_desktopColor,	0,		VLIT_desktopColor,NULL},
#endif
	{"$execPath",		sv_execPath,		V_NIL,		VLIT_execPath,NULL},
	{"$fencePause",		sv_fencePause,		V_INT,		VLIT_fencePause,NULL},
	{"$globalModes",	sv_globalModes,		V_INT,		VLIT_globalModes,NULL},
	{"$hardTabSize",	sv_hardTabSize,		V_INT,		VLIT_hardTabSize,NULL},
	{"$horzJump",		sv_horzJump,		V_INT,		VLIT_horzJump,NULL},
	{"$horzScrollCol",	sv_horzScrollCol,	V_INT,		VLIT_horzScrollCol,NULL},
	{"$inpDelim",		sv_inpDelim,		V_NIL | V_ESCDELIM,VLIT_inpDelim,NULL},
	{"$keyMacro",		sv_keyMacro,		V_NIL | V_ESCDELIM,VLIT_keyMacro,NULL},
	{"$lastKeySeq",		sv_lastKeySeq,		V_ESCDELIM,	VLIT_lastKeySeq,NULL},
	{"$lineChar",		sv_lineChar,		V_NIL,		VLIT_lineChar,NULL},
	{"$lineCol",		sv_lineCol,		V_INT,		VLIT_lineCol,NULL},
	{"$lineOffset",		sv_lineOffset,		V_INT,		VLIT_lineOffset,NULL},
	{"$lineText",		sv_lineText,		V_NIL,		VLIT_lineText,NULL},
	{"$maxLoop",		sv_maxLoop,		V_INT,		VLIT_maxLoop,NULL},
	{"$maxRecursion",	sv_maxRecursion,	V_INT,		VLIT_maxRecursion,NULL},
	{"$otpDelim",		sv_otpDelim,		V_NIL | V_ESCDELIM,VLIT_otpDelim,NULL},
	{"$pageOverlap",	sv_pageOverlap,		V_INT,		VLIT_pageOverlap,NULL},
#if COLOR
	{"$palette",		sv_palette,		0,		VLIT_palette,NULL},
#endif
	{"$randNumSeed",	sv_randNumSeed,		V_INT,		VLIT_randNumSeed,NULL},
	{"$replacePat",		sv_replacePat,		V_NIL | V_ESCDELIM,VLIT_replacePat,NULL},
	{"$screenNum",		sv_screenNum,		V_INT,		VLIT_screenNum,NULL},
	{"$searchDelim",	sv_searchDelim,		0,		VLIT_searchDelim,NULL},
	{"$searchPat",		sv_searchPat,		V_NIL | V_ESCDELIM,VLIT_searchPat,NULL},
	{"$showModes",		sv_showModes,		V_INT,		VLIT_showModes,NULL},
	{"$softTabSize",	sv_softTabSize,		V_INT,		VLIT_softTabSize,NULL},
	{"$travJumpSize",	sv_travJumpSize,	V_INT,		VLIT_travJumpSize,NULL},
	{"$vertJump",		sv_vertJump,		V_INT,		VLIT_vertJump,NULL},
	{"$windLineNum",	sv_windLineNum,		V_INT,		VLIT_windLineNum,NULL},
	{"$windNum",		sv_windNum,		V_INT,		VLIT_windNum,NULL},
	{"$windSize",		sv_windSize,		V_INT,		VLIT_windSize,NULL},
	{"$wordChars",		sv_wordChars,		V_NIL,		VLIT_wordChars,NULL},
	{"$workDir",		sv_workDir,		0,		VLIT_workDir,NULL},
	{"$wrapCol",		sv_wrapCol,		V_INT,		VLIT_wrapCol,NULL},
	{NULL,			-1,			0,		NULL,NULL}
	};

#define NSVARS	(sizeof(sysvars) / sizeof(SVar)) - 1

static UVar *gvarsheadp = NULL;		// Head of global variable list.
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
