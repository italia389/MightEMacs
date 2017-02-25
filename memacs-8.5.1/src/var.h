// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// var.h	System and user variable definitions for MightEMacs.

// Definition of system variables.
enum svarid {
	// Immutables.
	sv_ArgVector,sv_BufFlagActive,sv_BufFlagChanged,sv_BufFlagHidden,sv_BufFlagMacro,sv_BufFlagNarrowed,sv_BufInpDelim,
	sv_Date,
#if TypeAhead
	sv_KeyPending,
#endif
	sv_LineLen,sv_Match,sv_ModeAutoSave,sv_ModeBackup,sv_ModeC,sv_ModeClobber,sv_ModeColDisp,sv_ModeEsc8Bit,sv_ModeExact,
	sv_ModeExtraIndent,sv_ModeHorzScroll,sv_ModeLineDisp,sv_ModeMEMacs,sv_ModeMsgDisp,sv_ModeNoUpdate,sv_ModeOver,
	sv_ModePerl,sv_ModeReadOnly,sv_ModeRegexp,sv_ModeReplace,sv_ModeRuby,sv_ModeSafeSave,sv_ModeShell,sv_ModeWorkDir,
	sv_ModeWrap,sv_RegionText,sv_ReturnMsg,sv_RunFile,sv_RunName,sv_TermCols,sv_TermRows,sv_WindCount,

	// Mutables.
	sv_autoSave,sv_bufFile,sv_bufFlags,sv_bufLineNum,sv_bufModes,sv_bufName,sv_defModes,
#if Color
	sv_desktopColor,
#endif
	sv_execPath,sv_fencePause,sv_globalModes,sv_hardTabSize,sv_horzJump,sv_horzScrollCol,sv_inpDelim,sv_keyMacro,
	sv_lastKeySeq,sv_lineChar,sv_lineCol,sv_lineOffset,sv_lineText,sv_maxArrayDepth,sv_maxLoop,sv_maxMacroDepth,sv_otpDelim,
	sv_pageOverlap,
#if Color
	sv_palette,
#endif
	sv_randNumSeed,sv_replacePat,sv_screenNum,sv_searchDelim,sv_searchPat,sv_showModes,sv_softTabSize,sv_travJump,
	sv_vertJump,sv_windLineNum,sv_windNum,sv_windSize,sv_wordChars,sv_workDir,sv_wrapCol
	};

// User variable record.
typedef struct UVar {
	struct UVar *uv_nextp;		// Pointer to next variable.
	char uv_name[NVarName + 1];	// Name of user variable.
	ushort uv_flags;		// Variable flags.
	Datum *uv_datp;			// Value.
	} UVar;

// System variable record.
typedef struct {
	char *sv_name;			// Name of system variable.
	enum svarid sv_id;		// Unique identifier.
	ushort sv_flags;		// Variable flags.
	char *sv_desc;			// Short description.
	union {				// Value if a constant; otherwise, NULL.
		char *sv_str;		// String value;
		long sv_int;		// Integer value;
		} u;
	} SVar;

// System and user variable flags.
#define V_RdOnly	0x0001		// Read-only system variable or constant.
#define V_Nil		0x0002		// Nil assignment allowed to system variable.
#define V_Int		0x0004		// Integer system variable.
#define V_Array		0x0008		// Array system variable.
#define V_Global	0x0010		// Global variable (in user variable table).
#define V_Mode		0x0020		// Mode variable (get description from mode table).
#define V_EscDelim	0x0040		// Use escape character as input delimiter when prompting for a value.

// Descriptor for variable or array reference (lvalue).
typedef struct {
	ushort vd_type;			// Type of variable.
	union {
		ushort vd_argnum;	// Macro argument number.
		ArraySize vd_index;	// Array index.
		} i;
	union {
		UVar *vd_uvp;
		SVar *vd_svp;
		Datum *vd_margp;	// Pointer to macro-argument array.
		Array *vd_aryp;
		} p;			// Pointer to array or variable in table.
	} VDesc;

// Variable types.
#define VTyp_Unk	0		// Unknown variable type.
#define VTyp_SVar	1		// System variable.
#define VTyp_GVar	2		// Global variable.
#define VTyp_LVar	3		// Local (script) variable.
#define VTyp_NVar	4		// Numbered variable (macro argument).
#define VTyp_ARef	5		// Array element reference.

// External function declarations.
extern int aryget(ENode *np,VDesc *vdp,bool create);
extern int bumpvar(ENode *np,bool incr,bool pre);
#if MMDebug & Debug_Datum
extern void dumpvars(void);
#endif
extern int findvar(char *name,VDesc *vdp,int op);
extern bool intvar(VDesc *vdp);
extern bool isident1(int c);
extern int putvar(Datum *datp,VDesc *vdp);
extern int setvar(Datum *rp,int n,Datum **argpp);
extern int setwlist(char *wclist);
extern int showVariables(Datum *rp,int n,Datum **argpp);
extern int uvarclean(UVar *vstackp);
extern UVar *uvarfind(char *var);
extern uint varct(uint flags);
extern void varlist(char *vlistv[],uint count,uint flags);
extern int vderefn(Datum *datp,char *name);
extern int vderefv(Datum *datp,VDesc *vdp);

#ifdef DATAvar

// Table of system variables.  (Ones that are read-only begin with a capital letter.)
static SVar sysvars[] = {
	{"$ArgVector",		sv_ArgVector,		V_RdOnly | V_Array,VLit_ArgVector,NULL},
	{"$BufFlagActive",	sv_BufFlagActive,	V_RdOnly | V_Int,VLit_BufFlagActive,.u.sv_int = BFActive},
	{"$BufFlagChanged",	sv_BufFlagChanged,	V_RdOnly | V_Int,VLit_BufFlagChanged,.u.sv_int = BFChgd},
	{"$BufFlagHidden",	sv_BufFlagHidden,	V_RdOnly | V_Int,VLit_BufFlagHidden,.u.sv_int = BFHidden},
	{"$BufFlagMacro",	sv_BufFlagMacro,	V_RdOnly | V_Int,VLit_BufFlagMacro,.u.sv_int = BFMacro},
	{"$BufFlagNarrowed",	sv_BufFlagNarrowed,	V_RdOnly | V_Int,VLit_BufFlagNarrowed,.u.sv_int = BFNarrow},
	{"$BufInpDelim",	sv_BufInpDelim,		V_RdOnly,	VLit_BufInpDelim,NULL},
	{"$Date",		sv_Date,		V_RdOnly,	VLit_Date,NULL},
#if TypeAhead
	{"$KeyPending",		sv_KeyPending,		V_RdOnly,	VLit_KeyPending,NULL},
#endif
	{"$LineLen",		sv_LineLen,		V_RdOnly | V_Int,VLit_LineLen,NULL},
	{"$Match",		sv_Match,		V_RdOnly,	VLit_Match,NULL},
	{"$ModeAutoSave",sv_ModeAutoSave,V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_ASave),.u.sv_int = MdASave},
	{"$ModeBackup",		sv_ModeBackup,	V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_Bak),.u.sv_int = MdBak},
	{"$ModeC",		sv_ModeC,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_C),.u.sv_int = MdC},
	{"$ModeClobber",	sv_ModeClobber,	V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_Clob),.u.sv_int = MdClob},
	{"$ModeColDisp",	sv_ModeColDisp,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Col),.u.sv_int = MdCol},
	{"$ModeEsc8Bit",	sv_ModeEsc8Bit,	V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_Esc8),.u.sv_int = MdEsc8},
	{"$ModeExact",	sv_ModeExact,	V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_Exact),.u.sv_int = MdExact},
{"$ModeExtraIndent",sv_ModeExtraIndent,V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_XIndt),.u.sv_int = MdXIndt},
	{"$ModeHorzScroll",sv_ModeHorzScroll,V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_HScrl),.u.sv_int = MdHScrl},
	{"$ModeLineDisp",	sv_ModeLineDisp,V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Line),.u.sv_int = MdLine},
	{"$ModeMEMacs",	sv_ModeMEMacs,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Memacs),.u.sv_int = MdMemacs},
	{"$ModeMsgDisp",	sv_ModeMsgDisp,	V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_Msg),.u.sv_int = MdMsg},
	{"$ModeNoUpdate",sv_ModeNoUpdate,V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_NoUpd),.u.sv_int = MdNoUpd},
	{"$ModeOver",		sv_ModeOver,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Over),.u.sv_int = MdOver},
	{"$ModePerl",		sv_ModePerl,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Perl),.u.sv_int = MdPerl},
	{"$ModeReadOnly",sv_ModeReadOnly,V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_RdOnly),.u.sv_int = MdRdOnly},
	{"$ModeRegexp",	sv_ModeRegexp,	V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_Regexp),.u.sv_int = MdRegexp},
	{"$ModeReplace",	sv_ModeReplace,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Repl),.u.sv_int = MdRepl},
	{"$ModeRuby",		sv_ModeRuby,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Ruby),.u.sv_int = MdRuby},
	{"$ModeSafeSave",	sv_ModeSafeSave,V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_Safe),.u.sv_int = MdSafe},
	{"$ModeShell",	sv_ModeShell,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Shell),.u.sv_int = MdShell},
	{"$ModeWorkDir",sv_ModeWorkDir,	V_RdOnly | V_Int | V_Mode,(char *) (gmodeinfo + MdOff_WkDir),.u.sv_int = MdWkDir},
	{"$ModeWrap",		sv_ModeWrap,	V_RdOnly | V_Int | V_Mode,(char *) (bmodeinfo + MdOff_Wrap),.u.sv_int = MdWrap},
	{"$RegionText",		sv_RegionText,		V_RdOnly,	VLit_RegionText,NULL},
	{"$ReturnMsg",		sv_ReturnMsg,		V_RdOnly,	VLit_ReturnMsg,NULL},
	{"$RunFile",		sv_RunFile,		V_RdOnly,	VLit_RunFile,NULL},
	{"$RunName",		sv_RunName,		V_RdOnly,	VLit_RunName,NULL},
	{"$TermCols",		sv_TermCols,		V_RdOnly | V_Int,VLit_TermCols,NULL},
	{"$TermRows",		sv_TermRows,		V_RdOnly | V_Int,VLit_TermRows,NULL},
	{"$WindCount",		sv_WindCount,		V_RdOnly | V_Int,VLit_WindCount,NULL},

	{"$autoSave",		sv_autoSave,		V_Int,		VLit_autoSave,NULL},
	{"$bufFile",		sv_bufFile,		V_Nil,		VLit_bufFile,NULL},
	{"$bufFlags",		sv_bufFlags,		V_Int,		VLit_bufFlags,NULL},
	{"$bufLineNum",		sv_bufLineNum,		V_Int,		VLit_bufLineNum,NULL},
	{"$bufModes",		sv_bufModes,		V_Int,		VLit_bufModes,NULL},
	{"$bufName",		sv_bufName,		0,		VLit_bufName,NULL},
	{"$defModes",		sv_defModes,		V_Int,		VLit_defModes,NULL},
#if Color
	{"$desktopColor",	sv_desktopColor,	0,		VLit_desktopColor,NULL},
#endif
	{"$execPath",		sv_execPath,		V_Nil,		VLit_execPath,NULL},
	{"$fencePause",		sv_fencePause,		V_Int,		VLit_fencePause,NULL},
	{"$globalModes",	sv_globalModes,		V_Int,		VLit_globalModes,NULL},
	{"$hardTabSize",	sv_hardTabSize,		V_Int,		VLit_hardTabSize,NULL},
	{"$horzJump",		sv_horzJump,		V_Int,		VLit_horzJump,NULL},
	{"$horzScrollCol",	sv_horzScrollCol,	V_Int,		VLit_horzScrollCol,NULL},
	{"$inpDelim",		sv_inpDelim,		V_Nil | V_EscDelim,VLit_inpDelim,NULL},
	{"$keyMacro",		sv_keyMacro,		V_Nil | V_EscDelim,VLit_keyMacro,NULL},
	{"$lastKeySeq",		sv_lastKeySeq,		V_EscDelim,	VLit_lastKeySeq,NULL},
	{"$lineChar",		sv_lineChar,		V_Nil,		VLit_lineChar,NULL},
	{"$lineCol",		sv_lineCol,		V_Int,		VLit_lineCol,NULL},
	{"$lineOffset",		sv_lineOffset,		V_Int,		VLit_lineOffset,NULL},
	{"$lineText",		sv_lineText,		V_Nil,		VLit_lineText,NULL},
	{"$maxArrayDepth",	sv_maxArrayDepth,	V_Int,		VLit_maxArrayDepth,NULL},
	{"$maxLoop",		sv_maxLoop,		V_Int,		VLit_maxLoop,NULL},
	{"$maxMacroDepth",	sv_maxMacroDepth,	V_Int,		VLit_maxMacroDepth,NULL},
	{"$otpDelim",		sv_otpDelim,		V_Nil | V_EscDelim,VLit_otpDelim,NULL},
	{"$pageOverlap",	sv_pageOverlap,		V_Int,		VLit_pageOverlap,NULL},
#if Color
	{"$palette",		sv_palette,		0,		VLit_palette,NULL},
#endif
	{"$randNumSeed",	sv_randNumSeed,		V_Int,		VLit_randNumSeed,NULL},
	{"$replacePat",		sv_replacePat,		V_Nil | V_EscDelim,VLit_replacePat,NULL},
	{"$screenNum",		sv_screenNum,		V_Int,		VLit_screenNum,NULL},
	{"$searchDelim",	sv_searchDelim,		0,		VLit_searchDelim,NULL},
	{"$searchPat",		sv_searchPat,		V_Nil | V_EscDelim,VLit_searchPat,NULL},
	{"$showModes",		sv_showModes,		V_Int,		VLit_showModes,NULL},
	{"$softTabSize",	sv_softTabSize,		V_Int,		VLit_softTabSize,NULL},
	{"$travJump",		sv_travJump,		V_Int,		VLit_travJump,NULL},
	{"$vertJump",		sv_vertJump,		V_Int,		VLit_vertJump,NULL},
	{"$windLineNum",	sv_windLineNum,		V_Int,		VLit_windLineNum,NULL},
	{"$windNum",		sv_windNum,		V_Int,		VLit_windNum,NULL},
	{"$windSize",		sv_windSize,		V_Int,		VLit_windSize,NULL},
	{"$wordChars",		sv_wordChars,		V_Nil,		VLit_wordChars,NULL},
	{"$workDir",		sv_workDir,		0,		VLit_workDir,NULL},
	{"$wrapCol",		sv_wrapCol,		V_Int,		VLit_wrapCol,NULL},
	{NULL,			-1,			0,		NULL,NULL}
	};

#define NSVars	(sizeof(sysvars) / sizeof(SVar)) - 1

// **** For var.c ****

// Global variables.
UVar *gvarsheadp = NULL;		// Head of global variable list.
Datum *lastMatch;			// Last pattern match.
UVar *lvarsheadp = NULL;		// Head of local (macro) variable list.

#else

// **** For all the other .c files ****

// External variable declarations.
extern UVar *gvarsheadp;
extern Datum *lastMatch;
extern UVar *lvarsheadp;
#endif
