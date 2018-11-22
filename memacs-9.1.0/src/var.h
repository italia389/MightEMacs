// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// var.h	System and user variable definitions for MightEMacs.

// Definition of system variables.
enum svarid {
	// Immutables.
	sv_ARGV,sv_BufInpDelim,sv_BufModes,sv_Date,sv_GlobalModes,sv_HorzScrollCol,sv_LastKey,sv_LineLen,sv_Match,sv_RegionText,
	sv_ReturnMsg,sv_RunFile,sv_RunName,sv_ScreenCount,sv_TermCols,sv_TermRows,sv_WindCount,

	// Mutables.
	sv_autoSave,sv_bufFile,sv_bufLineNum,sv_bufName,sv_execPath,sv_fencePause,sv_hardTabSize,sv_horzJump,sv_inpDelim,
	sv_keyMacro,sv_killRingSize,sv_lastKeySeq,sv_lineChar,sv_lineCol,sv_lineOffset,sv_lineText,sv_maxArrayDepth,sv_maxLoop,
	sv_maxMacroDepth,sv_maxPromptPct,sv_otpDelim,sv_pageOverlap,sv_randNumSeed,sv_replacePat,sv_replaceRingSize,
	sv_screenNum,sv_searchDelim,sv_searchPat,sv_searchRingSize,sv_softTabSize,sv_travJump,sv_vertJump,sv_windLineNum,
	sv_windNum,sv_windSize,sv_wordChars,sv_workDir,sv_wrapCol
	};

// User variable record.
typedef struct UVar {
	struct UVar *uv_nextp;		// Pointer to next variable.
	char uv_name[MaxVarName + 1];	// Name of user variable.
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
#define V_Char		0x0004		// Character (int) system variable.
#define V_Int		0x0008		// Integer system variable.
#define V_Array		0x0010		// Array system variable.
#define V_Global	0x0020		// Global variable (in user variable table).
#define V_EscDelim	0x0040		// Use escape character as input delimiter when prompting for a value.
#define V_GetKey	0x0080		// Call getkey() to get value interactively.
#define V_GetKeySeq	0x0100		// Call getkseq() to get value interactively.

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
extern int findvar(char *name,VDesc *vdp,ushort op);
extern bool intvar(VDesc *vdp);
extern bool isident1(short c);
extern int putvar(Datum *datp,VDesc *vdp);
extern ulong seedinit(void);
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

// Table of system variables.  (Ones that begin with a capital letter are read-only.)
static SVar sysvars[] = {
	{"$ARGV",		sv_ARGV,		V_RdOnly | V_Array,VLit_ARGV,NULL},
	{"$BufInpDelim",	sv_BufInpDelim,		V_RdOnly,	VLit_BufInpDelim,NULL},
	{"$BufModes",		sv_BufModes,		V_RdOnly | V_Array,VLit_BufModes,NULL},
	{"$Date",		sv_Date,		V_RdOnly,	VLit_Date,NULL},
	{"$GlobalModes",	sv_GlobalModes,		V_RdOnly | V_Array,VLit_GlobalModes,NULL},
	{"$HorzScrollCol",	sv_HorzScrollCol,	V_RdOnly | V_Int,VLit_HorzScrollCol,.u.sv_int = 0},
	{"$LastKey",		sv_LastKey,		V_RdOnly | V_Int,VLit_LastKey,.u.sv_int = 0},
	{"$LineLen",		sv_LineLen,		V_RdOnly | V_Int,VLit_LineLen,.u.sv_int = 0},
	{"$Match",		sv_Match,		V_RdOnly,	VLit_Match,NULL},
	{"$RegionText",		sv_RegionText,		V_RdOnly,	VLit_RegionText,NULL},
	{"$ReturnMsg",		sv_ReturnMsg,		V_RdOnly,	VLit_ReturnMsg,NULL},
	{"$RunFile",		sv_RunFile,		V_RdOnly,	VLit_RunFile,NULL},
	{"$RunName",		sv_RunName,		V_RdOnly,	VLit_RunName,NULL},
	{"$ScreenCount",	sv_ScreenCount,		V_RdOnly | V_Int,VLit_ScreenCount,.u.sv_int = 0},
	{"$TermCols",		sv_TermCols,		V_RdOnly | V_Int,VLit_TermCols,.u.sv_int = 0},
	{"$TermRows",		sv_TermRows,		V_RdOnly | V_Int,VLit_TermRows,.u.sv_int = 0},
	{"$WindCount",		sv_WindCount,		V_RdOnly | V_Int,VLit_WindCount,.u.sv_int = 0},

	{"$autoSave",		sv_autoSave,		V_Int,		VLit_autoSave,NULL},
	{"$bufFile",		sv_bufFile,		V_Nil,		VLit_bufFile,NULL},
	{"$bufLineNum",		sv_bufLineNum,		V_Int,		VLit_bufLineNum,NULL},
	{"$bufName",		sv_bufName,		0,		VLit_bufName,NULL},
	{"$execPath",		sv_execPath,		V_Nil,		VLit_execPath,NULL},
	{"$fencePause",		sv_fencePause,		V_Int,		VLit_fencePause,NULL},
	{"$hardTabSize",	sv_hardTabSize,		V_Int,		VLit_hardTabSize,NULL},
	{"$horzJump",		sv_horzJump,		V_Int,		VLit_horzJump,NULL},
	{"$inpDelim",		sv_inpDelim,		V_Nil | V_EscDelim,VLit_inpDelim,NULL},
	{"$keyMacro",		sv_keyMacro,		V_Nil | V_EscDelim,VLit_keyMacro,NULL},
	{"$killRingSize",	sv_killRingSize,	V_Int,		VLit_killRingSize,NULL},
	{"$lastKeySeq",		sv_lastKeySeq,		V_GetKeySeq,	VLit_lastKeySeq,NULL},
	{"$lineChar",		sv_lineChar,		V_Char,		VLit_lineChar,NULL},
	{"$lineCol",		sv_lineCol,		V_Int,		VLit_lineCol,NULL},
	{"$lineOffset",		sv_lineOffset,		V_Int,		VLit_lineOffset,NULL},
	{"$lineText",		sv_lineText,		V_Nil,		VLit_lineText,NULL},
	{"$maxArrayDepth",	sv_maxArrayDepth,	V_Int,		VLit_maxArrayDepth,NULL},
	{"$maxLoop",		sv_maxLoop,		V_Int,		VLit_maxLoop,NULL},
	{"$maxMacroDepth",	sv_maxMacroDepth,	V_Int,		VLit_maxMacroDepth,NULL},
	{"$maxPromptPct",	sv_maxPromptPct,	V_Int,		VLit_maxPromptPct,NULL},
	{"$otpDelim",		sv_otpDelim,		V_Nil | V_EscDelim,VLit_otpDelim,NULL},
	{"$pageOverlap",	sv_pageOverlap,		V_Int,		VLit_pageOverlap,NULL},
	{"$randNumSeed",	sv_randNumSeed,		V_Int,		VLit_randNumSeed,NULL},
	{"$replacePat",		sv_replacePat,		V_Nil | V_EscDelim,VLit_replacePat,NULL},
	{"$replaceRingSize",	sv_replaceRingSize,	V_Int,		VLit_replaceRingSize,NULL},
	{"$screenNum",		sv_screenNum,		V_Int,		VLit_screenNum,NULL},
	{"$searchDelim",	sv_searchDelim,		V_GetKey,	VLit_searchDelim,NULL},
	{"$searchPat",		sv_searchPat,		V_Nil | V_EscDelim,VLit_searchPat,NULL},
	{"$searchRingSize",	sv_searchRingSize,	V_Int,		VLit_searchRingSize,NULL},
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
