// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// var.h	System and user variable definitions for MightEMacs.

// Definition of system variables.
enum svarid {
	// Immutables.
	sv_ARGV,sv_BufInpDelim,sv_BufModes,sv_Date,sv_GlobalModes,sv_HorzScrollCol,sv_LastKey,sv_LineLen,sv_Match,sv_RegionText,
	sv_ReturnMsg,sv_RunFile,sv_RunName,sv_ScreenCount,sv_TermSize,sv_WindCount,

	// Mutables.
	sv_autoSave,sv_bufFile,sv_bufLineNum,sv_bufname,sv_delRingSize,sv_execPath,sv_fencePause,sv_hardTabSize,sv_horzJump,
	sv_inpDelim,sv_keyMacro,sv_killRingSize,sv_lastKeySeq,sv_lineChar,sv_lineCol,sv_lineOffset,sv_lineText,sv_maxArrayDepth,
	sv_maxLoop,sv_maxMacroDepth,sv_maxPromptPct,sv_otpDelim,sv_pageOverlap,sv_randNumSeed,sv_replacePat,sv_replaceRingSize,
	sv_screenNum,sv_searchDelim,sv_searchPat,sv_searchRingSize,sv_softTabSize,sv_travJump,sv_vertJump,sv_windLineNum,
	sv_windNum,sv_windSize,sv_wordChars,sv_workDir,sv_wrapCol
	};

// User variable record.
typedef struct UVar {
	struct UVar *uv_next;		// Pointer to next variable.
	char uv_name[MaxVarName + 1];	// Name of user variable.
	ushort uv_flags;		// Variable flags.
	Datum *uv_datum;			// Value.
	} UVar;

// System variable record.
typedef struct {
	char *sv_name;			// Name of system variable.
	enum svarid sv_id;		// Unique identifier.
	ushort sv_flags;		// Variable flags.
	char *sv_desc;			// Short description.
#if 0
	union {				// Value if a constant; otherwise, NULL.
		char *sv_str;		// String value;
		long sv_int;		// Integer value;
		} u;
#endif
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
		Datum *vd_marg;	// Pointer to macro-argument array.
		Array *vd_ary;
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
extern int getsvar(Datum *rval,SVar *svp);
extern bool intvar(VDesc *vdp);
extern bool isident1(short c);
extern int putvar(Datum *datum,VDesc *vdp);
extern ulong seedinit(void);
extern int setvar(Datum *rval,int n,Datum **argv);
extern int setwlist(char *wclist);
extern int showVariables(Datum *rval,int n,Datum **argv);
extern int svtosf(DStrFab *dest,SVar *svp,ushort flags);
extern int uvarclean(UVar *vstack);
extern UVar *uvarfind(char *var);
extern uint varct(uint flags);
extern void varlist(char *vlistv[],uint count,uint flags);
extern int vderefn(Datum *datum,char *name);
extern int vderefv(Datum *datum,VDesc *vdp);

#ifdef DATAvar

// Table of system variables.  (Ones that begin with a capital letter are read-only.)
SVar sysvars[] = {
	{"$ARGV",		sv_ARGV,		V_RdOnly | V_Array,	VLit_ARGV},
	{"$BufInpDelim",	sv_BufInpDelim,		V_RdOnly,		VLit_BufInpDelim},
	{"$BufModes",		sv_BufModes,		V_RdOnly | V_Array,	VLit_BufModes},
	{"$Date",		sv_Date,		V_RdOnly,		VLit_Date},
	{"$GlobalModes",	sv_GlobalModes,		V_RdOnly | V_Array,	VLit_GlobalModes},
	{"$HorzScrollCol",	sv_HorzScrollCol,	V_RdOnly | V_Int,	VLit_HorzScrollCol},
	{"$LastKey",		sv_LastKey,		V_RdOnly | V_Int,	VLit_LastKey},
	{"$LineLen",		sv_LineLen,		V_RdOnly | V_Int,	VLit_LineLen},
	{"$Match",		sv_Match,		V_RdOnly,		VLit_Match},
	{"$RegionText",		sv_RegionText,		V_RdOnly,		VLit_RegionText},
	{"$ReturnMsg",		sv_ReturnMsg,		V_RdOnly,		VLit_ReturnMsg},
	{"$RunFile",		sv_RunFile,		V_RdOnly,		VLit_RunFile},
	{"$RunName",		sv_RunName,		V_RdOnly,		VLit_RunName},
	{"$ScreenCount",	sv_ScreenCount,		V_RdOnly | V_Int,	VLit_ScreenCount},
	{"$TermSize",		sv_TermSize,		V_RdOnly | V_Array,	VLit_TermSize},
	{"$WindCount",		sv_WindCount,		V_RdOnly | V_Int,	VLit_WindCount},

	{"$autoSave",		sv_autoSave,		V_Int,			VLit_autoSave},
	{"$bufFile",		sv_bufFile,		V_Nil,			VLit_bufFile},
	{"$bufLineNum",		sv_bufLineNum,		V_Int,			VLit_bufLineNum},
	{"$bufname",		sv_bufname,		0,			VLit_bufname},
	{"$delRingSize",	sv_delRingSize,		V_Int,			VLit_delRingSize},
	{"$execPath",		sv_execPath,		V_Nil,			VLit_execPath},
	{"$fencePause",		sv_fencePause,		V_Int,			VLit_fencePause},
	{"$hardTabSize",	sv_hardTabSize,		V_Int,			VLit_hardTabSize},
	{"$horzJump",		sv_horzJump,		V_Int,			VLit_horzJump},
	{"$inpDelim",		sv_inpDelim,		V_Nil | V_EscDelim,	VLit_inpDelim},
	{"$keyMacro",		sv_keyMacro,		V_Nil | V_EscDelim,	VLit_keyMacro},
	{"$killRingSize",	sv_killRingSize,	V_Int,			VLit_killRingSize},
	{"$lastKeySeq",		sv_lastKeySeq,		V_GetKeySeq,		VLit_lastKeySeq},
	{"$lineChar",		sv_lineChar,		V_Char,			VLit_lineChar},
	{"$lineCol",		sv_lineCol,		V_Int,			VLit_lineCol},
	{"$lineOffset",		sv_lineOffset,		V_Int,			VLit_lineOffset},
	{"$lineText",		sv_lineText,		V_Nil,			VLit_lineText},
	{"$maxArrayDepth",	sv_maxArrayDepth,	V_Int,			VLit_maxArrayDepth},
	{"$maxLoop",		sv_maxLoop,		V_Int,			VLit_maxLoop},
	{"$maxMacroDepth",	sv_maxMacroDepth,	V_Int,			VLit_maxMacroDepth},
	{"$maxPromptPct",	sv_maxPromptPct,	V_Int,			VLit_maxPromptPct},
	{"$otpDelim",		sv_otpDelim,		V_Nil | V_EscDelim,	VLit_otpDelim},
	{"$pageOverlap",	sv_pageOverlap,		V_Int,			VLit_pageOverlap},
	{"$randNumSeed",	sv_randNumSeed,		V_Int,			VLit_randNumSeed},
	{"$replacePat",		sv_replacePat,		V_Nil | V_EscDelim,	VLit_replacePat},
	{"$replaceRingSize",	sv_replaceRingSize,	V_Int,			VLit_replaceRingSize},
	{"$screenNum",		sv_screenNum,		V_Int,			VLit_screenNum},
	{"$searchDelim",	sv_searchDelim,		V_GetKey,		VLit_searchDelim},
	{"$searchPat",		sv_searchPat,		V_Nil | V_EscDelim,	VLit_searchPat},
	{"$searchRingSize",	sv_searchRingSize,	V_Int,			VLit_searchRingSize},
	{"$softTabSize",	sv_softTabSize,		V_Int,			VLit_softTabSize},
	{"$travJump",		sv_travJump,		V_Int,			VLit_travJump},
	{"$vertJump",		sv_vertJump,		V_Int,			VLit_vertJump},
	{"$windLineNum",	sv_windLineNum,		V_Int,			VLit_windLineNum},
	{"$windNum",		sv_windNum,		V_Int,			VLit_windNum},
	{"$windSize",		sv_windSize,		V_Int,			VLit_windSize},
	{"$wordChars",		sv_wordChars,		V_Nil,			VLit_wordChars},
	{"$workDir",		sv_workDir,		0,			VLit_workDir},
	{"$wrapCol",		sv_wrapCol,		V_Int,			VLit_wrapCol},
	{NULL,			-1,			0,		NULL}
	};

#define NSVars	(sizeof(sysvars) / sizeof(SVar)) - 1

// **** For var.c ****

// Global variables.
UVar *gvarshead = NULL;		// Head of global variable list.
Datum *lastMatch;			// Last pattern match.
UVar *lvarshead = NULL;		// Head of local (macro) variable list.

#else

// **** For all the other .c files ****

// External variable declarations.
extern UVar *gvarshead;
extern Datum *lastMatch;
extern UVar *lvarshead;
extern SVar sysvars[];
#endif
