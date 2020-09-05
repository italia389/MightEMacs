// (c) Copyright 2020 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// var.h	System and user variable definitions for MightEMacs.

// Definition of system variables.
typedef enum {
	// Immutables.
	sv_ARGV, sv_BufInpDelim, sv_BufModes, sv_Date, sv_GlobalModes, sv_HorzScrollCol, sv_LastKey, sv_LineLen, sv_Match,
	sv_RegionText, sv_ReturnMsg, sv_RingNames, sv_RunFile, sv_RunName, sv_ScreenCount, sv_TermSize, sv_WindCount,

	// Mutables.
	sv_autoSave, sv_bufFile, sv_bufLineNum, sv_bufname, sv_execPath, sv_fencePause, sv_hardTabSize, sv_horzJump,
	sv_inpDelim, sv_lastKeySeq, sv_lineChar, sv_lineCol, sv_lineOffset, sv_lineText, sv_maxArrayDepth, sv_maxCallDepth,
	sv_maxLoop, sv_maxPromptPct, sv_otpDelim, sv_pageOverlap, sv_randNumSeed, sv_replacePat, sv_screenNum, sv_searchDelim,
	sv_searchPat, sv_softTabSize, sv_travJump, sv_vertJump, sv_windLineNum, sv_windNum, sv_windSize, sv_workDir,
	sv_wrapCol
	} SysVarId;

// User variable record.
typedef struct UserVar {
	struct UserVar *next;		// Pointer to next variable.
	char name[MaxVarName + 1];	// Name of user variable.
	ushort flags;			// Variable flags.
	Datum *pValue;			// Value.
	} UserVar;

// System variable record.
typedef struct {
	const char *name;		// Name of system variable.
	SysVarId id;			// Unique identifier.
	ushort flags;			// Variable flags.
	const char *descrip;		// Short description.
#if 0
	union {				// Value if a constant; otherwise, NULL.
		const char *strVal;	// String value;
		long intVal;		// Integer value;
		} u;
#endif
	} SysVar;

// System and user variable flags.
#define V_RdOnly	0x0001		// Read-only system variable or constant.
#define V_Nil		0x0002		// Nil assignment allowed to system variable.
#define V_Char		0x0004		// Character (int) system variable.
#define V_Int		0x0008		// Integer system variable.
#define V_Array		0x0010		// Array system variable.
#define V_Global	0x0020		// Global variable (in user variable table).
#define V_EscDelim	0x0040		// Use escape character as input delimiter when prompting for a value.
#define V_GetKey	0x0080		// Call getkey() to get value interactively.
#define V_GetKeySeq	0x0100		// Call getKeySeq() to get value interactively.

// Descriptor for variable or array reference (lvalue).
typedef struct {
	ushort type;			// Type of variable.
	union {
		ushort argNum;		// User command/function argument number.
		ArraySize index;	// Array index.
		} i;
	union {
		UserVar *pUserVar;
		SysVar *pSysVar;
		Datum *pArgs;		// Pointer to user-routine-argument array.
		Array *pArray;
		} p;			// Pointer to array or variable in table.
	} VarDesc;

// Variable types.
#define VTyp_Unk	0		// Unknown variable type.
#define VTyp_SysVar	1		// System variable.
#define VTyp_GlobalVar	2		// Global variable.
#define VTyp_LocalVar	3		// Local (script) variable.
#define VTyp_NumVar	4		// Numbered variable (user command/function argument).
#define VTyp_ArrayElRef	5		// Array element reference.

// External function declarations.
extern int bumpVar(ExprNode *pNode, bool incr, bool pre);
#if MMDebug & Debug_Datum
extern void dumpVars(void);
#endif
extern UserVar *findUserVar(const char *var);
extern int findVar(const char *name, VarDesc *pVarDesc, ushort op);
extern int freeUserVars(UserVar *pVarStack);
extern int getArrayRef(ExprNode *pNode, VarDesc *pVarDesc, bool create);
extern int getSysVar(Datum *pRtnVal, SysVar *pSysVar);
extern bool isIntVar(VarDesc *pVarDesc);
extern bool isIdent1(short c);
extern int putVar(Datum *pDatum, VarDesc *pVarDesc);
extern int ringNames(Datum *pRtnVal);
extern ulong seedInit(void);
extern int setVar(Datum *pRtnVal, int n, Datum **args);
extern int showVariables(Datum *pRtnVal, int n, Datum **args);
extern int svtosf(SysVar *pSysVar, ushort flags, DFab *pFab);
extern uint varCount(uint ctrlFlags);
extern void varSort(const char *varList[], uint count, uint ctrlFlags);
extern int vderefn(Datum *pDatum, const char *name);
extern int vderefv(Datum *pDatum, VarDesc *pVarDesc);

#ifdef VarData

// **** For var.c ****

// Global variables.
UserVar *globalVarRoot = NULL;		// Head of global variable list.
UserVar *localVarRoot = NULL;		// Head of local (user command/function) variable list.
Datum *pLastMatch = NULL;		// Last search pattern match.

// Table of system variables.  Ones that begin with a capital letter are read-only.
SysVar sysVars[] = {
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
	{"$RingNames",		sv_RingNames,		V_RdOnly | V_Array,	VLit_RingNames},
	{"$RunFile",		sv_RunFile,		V_RdOnly,		VLit_RunFile},
	{"$RunName",		sv_RunName,		V_RdOnly,		VLit_RunName},
	{"$ScreenCount",	sv_ScreenCount,		V_RdOnly | V_Int,	VLit_ScreenCount},
	{"$TermSize",		sv_TermSize,		V_RdOnly | V_Array,	VLit_TermSize},
	{"$WindCount",		sv_WindCount,		V_RdOnly | V_Int,	VLit_WindCount},

	{"$autoSave",		sv_autoSave,		V_Int,			VLit_autoSave},
	{"$bufFile",		sv_bufFile,		V_Nil,			VLit_bufFile},
	{"$bufLineNum",		sv_bufLineNum,		V_Int,			VLit_bufLineNum},
	{"$bufname",		sv_bufname,		0,			VLit_bufname},
	{"$execPath",		sv_execPath,		V_Nil,			VLit_execPath},
	{"$fencePause",		sv_fencePause,		V_Int,			VLit_fencePause},
	{"$hardTabSize",	sv_hardTabSize,		V_Int,			VLit_hardTabSize},
	{"$horzJump",		sv_horzJump,		V_Int,			VLit_horzJump},
	{"$inpDelim",		sv_inpDelim,		V_Nil | V_EscDelim,	VLit_inpDelim},
	{"$lastKeySeq",		sv_lastKeySeq,		V_GetKeySeq,		VLit_lastKeySeq},
	{"$lineChar",		sv_lineChar,		V_Char,			VLit_lineChar},
	{"$lineCol",		sv_lineCol,		V_Int,			VLit_lineCol},
	{"$lineOffset",		sv_lineOffset,		V_Int,			VLit_lineOffset},
	{"$lineText",		sv_lineText,		V_Nil,			VLit_lineText},
	{"$maxArrayDepth",	sv_maxArrayDepth,	V_Int,			VLit_maxArrayDepth},
	{"$maxCallDepth",	sv_maxCallDepth,	V_Int,			VLit_maxCallDepth},
	{"$maxLoop",		sv_maxLoop,		V_Int,			VLit_maxLoop},
	{"$maxPromptPct",	sv_maxPromptPct,	V_Int,			VLit_maxPromptPct},
	{"$otpDelim",		sv_otpDelim,		V_Nil | V_EscDelim,	VLit_otpDelim},
	{"$pageOverlap",	sv_pageOverlap,		V_Int,			VLit_pageOverlap},
	{"$randNumSeed",	sv_randNumSeed,		V_Int,			VLit_randNumSeed},
	{"$replacePat",		sv_replacePat,		V_Nil | V_EscDelim,	VLit_replacePat},
	{"$screenNum",		sv_screenNum,		V_Int,			VLit_screenNum},
	{"$searchDelim",	sv_searchDelim,		V_GetKey,		VLit_searchDelim},
	{"$searchPat",		sv_searchPat,		V_Nil | V_EscDelim,	VLit_searchPat},
	{"$softTabSize",	sv_softTabSize,		V_Int,			VLit_softTabSize},
	{"$travJump",		sv_travJump,		V_Int,			VLit_travJump},
	{"$vertJump",		sv_vertJump,		V_Int,			VLit_vertJump},
	{"$windLineNum",	sv_windLineNum,		V_Int,			VLit_windLineNum},
	{"$windNum",		sv_windNum,		V_Int,			VLit_windNum},
	{"$windSize",		sv_windSize,		V_Int,			VLit_windSize},
	{"$workDir",		sv_workDir,		0,			VLit_workDir},
	{"$wrapCol",		sv_wrapCol,		V_Int,			VLit_wrapCol},
	{NULL,			-1,			0,		NULL}
	};

#define NumSysVars	(sizeof(sysVars) / sizeof(SysVar)) - 1

#else

// **** For all the other .c files ****

// External variable declarations.
extern UserVar *globalVarRoot;
extern UserVar *localVarRoot;
extern Datum *pLastMatch;
extern SysVar sysVars[];
#endif
