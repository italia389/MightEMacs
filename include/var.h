// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// var.h	System and user variable definitions for MightEMacs.

// Definition of system variables.
typedef enum {
	// Immutables.
	sv_ArgList, sv_BufInpDelim, sv_BufModes, sv_GlobalModes, sv_HorzScrollCol, sv_LastKey, sv_LineLen, sv_Match,
	sv_RegionText, sv_ReturnMsg, sv_RunFile, sv_RunName, sv_ScreenCount, sv_TermSize, sv_WindCount,

	// Mutables.
	sv_autoSave, sv_bufFile, sv_bufLineNum, sv_bufname, sv_execPath, sv_fencePause, sv_hardTabSize, sv_horzJump,
	sv_inpDelim, sv_lastKeySeq, sv_lineChar, sv_lineCol, sv_lineOffset, sv_lineText, sv_maxCallDepth, sv_maxLoop,
	sv_maxPromptPct, sv_otpDelim, sv_pageOverlap, sv_replacePat, sv_screenNum, sv_searchDelim, sv_searchPat, sv_softTabSize,
	sv_travJump, sv_vertJump, sv_windLineNum, sv_windNum, sv_windSize, sv_workDir, sv_wrapCol
	} SConstVarId;

// Local variable object.
typedef struct LocalVar {
	struct LocalVar *next;		// Pointer to next variable.
	char name[MaxVarName + 1];	// Name of local variable.
	Datum *pVal;			// Value.
	} LocalVar;

// Global constant/variable object.
typedef struct {
	char *name;			// Name of global constant or variable.
	Datum *pVal;			// Value.
	ushort flags;			// Flag(s) (below).
	} GConstVar;

// System constant/variable object.
typedef struct {
	const char *name;		// Name of system constant or variable.
	SConstVarId id;			// Unique identifier.
	ushort flags;			// Flag(s) (below).
	const char *descrip;		// Short description.
	} SConstVar;

// System constant/variable flags.
#define V_Const		0x0001		// System or global constant, otherwise variable.
#define V_NewMint	0x0002		// Constant was "newly minted".
#define V_Nil		0x0004		// Nil assignment allowed to system variable.
#define V_Char		0x0008		// Character (int) system variable.
#define V_Int		0x0010		// Integer system variable.
#define V_Array		0x0020		// System variable is an array (constant).
#define V_EscDelim	0x0040		// Use escape character as input delimiter when prompting for a value.
#define V_GetKey	0x0080		// Call getkey() to get value interactively.
#define V_GetKeySeq	0x0100		// Call getKeySeq() to get value interactively.
#define V_ShowKey	0x0200		// Treat variable value as key or key sequence when displaying.

#define GlobalTableSize	17		// Initialize hash size of global variable table.

// Descriptor for variable, array element reference, or hash node reference (lvalue).  Note that i.index is valid only for
// VTyp_NumVar and VTyp_AryElRef types and i.pKey is valid only for VTyp_HshNodRef type; otherwise, they are undefined.
typedef struct {
	ushort type;			// Type of variable.
	union {
		ArraySize index;	// Array index.
		Datum *pKey;		// Hash key.
		} i;
	union {
		LocalVar *pLocalVar;
		GConstVar *pGConstVar;
		SConstVar *pSConstVar;
		Array *pArray;
		Hash *pHash;
		} p;			// Pointer to variable, array, or hash in table.
	} VarDesc;

// Variable types.
#define VTyp_Unk	0		// Unknown variable type.
#define VTyp_SConstVar	1		// System constant or variable.
#define VTyp_GConstVar	2		// Global constant or variable.
#define VTyp_LocalVar	3		// Local (script) variable.
#define VTyp_NumVar	4		// Numbered variable (user command/function argument).
#define VTyp_AryElRef	5		// Array element reference.
#define VTyp_HshNodRef	6		// Hash node reference.

// External function declarations.
extern int bumpVar(ExprNode *pNode, bool incr, bool pre);
extern int cvtAryElRef(VarDesc *pVarDesc, ExprNode *pNode, uint oflags);
extern int cvtHshNodRef(VarDesc *pVarDesc, ExprNode *pNode, uint oflags);
extern int delConstVar(Datum *pRtnVal, long n, Datum **args);
extern int dtofabattr(const Datum *pDatum, DFab *pFab, const char *delim, uint cflags);
#if MMDebug & Debug_Datum
extern void ddump(const Datum *pDatum, const char *tag);
extern void dumpVars(void);
#endif
extern LocalVar *findLocalVar(const char *name);
extern void freeLocalVars(LocalVar *pVarStack);
extern int getSConstVar(Datum *pRtnVal, SConstVar *pSConstVar);
extern bool immutable(VarDesc *pVarDesc);
extern bool isIntVar(VarDesc *pVarDesc);
extern bool isIdent1(short c);
extern int let(Datum *pRtnVal, long n, Datum **args);
extern int setVar(VarDesc *pVarDesc, Datum *pNewVal);
extern int showConstVar(Datum *pRtnVal, long n, Datum **args);
extern int showConstVars(Datum *pRtnVal, long n, Datum **args);
extern int scvtofab(SConstVar *pSConstVar, bool escTermAttr, DFab *pFab);
extern uint cvcount(uint ctrlFlags);
extern int cvinit(void);
extern void cvsort(const char *cvList[], uint count, uint ctrlFlags);
extern int vderefn(Datum *pDatum, const char *name);
extern int vderefv(Datum *pDatum, VarDesc *pVarDesc);
extern int vfind(const char *name, VarDesc *pVarDesc, uint op);

#ifdef VarData

// **** For var.c ****

// Global variables.
Hash gblConstVarTable;			// Global variable table.
LocalVar *localVarRoot = NULL;		// Head of local (user routine) variable list.

// Table of system variables.  Ones that begin with a capital letter are read-only.  Note that $lastKeySeq is mutable so that
// a script can simulate a user key entry.
SConstVar sysConstTable[] = {
	{"ArgList",		sv_ArgList,		V_Const | V_Array,	VLit_ArgList},
	{"BufInpDelim",		sv_BufInpDelim,		V_Const,		VLit_BufInpDelim},
	{"BufModes",		sv_BufModes,		V_Const | V_Array,	VLit_BufModes},
	{"GlobalModes",		sv_GlobalModes,		V_Const | V_Array,	VLit_GlobalModes},
	{"HorzScrollCol",	sv_HorzScrollCol,	V_Const | V_Int,	VLit_HorzScrollCol},
	{"LastKey",		sv_LastKey,		V_Const | V_Int,	VLit_LastKey},
	{"LineLen",		sv_LineLen,		V_Const | V_Int,	VLit_LineLen},
	{"Match",		sv_Match,		V_Const,		VLit_Match},
	{"RegionText",		sv_RegionText,		V_Const,		VLit_RegionText},
	{"ReturnMsg",		sv_ReturnMsg,		V_Const,		VLit_ReturnMsg},
	{"RunFile",		sv_RunFile,		V_Const,		VLit_RunFile},
	{"RunName",		sv_RunName,		V_Const,		VLit_RunName},
	{"ScreenCount",		sv_ScreenCount,		V_Const | V_Int,	VLit_ScreenCount},
	{"TermSize",		sv_TermSize,		V_Const | V_Array,	VLit_TermSize},
	{"WindCount",		sv_WindCount,		V_Const | V_Int,	VLit_WindCount},
	{NULL,			-1,			0,			NULL}};
#define NumSysConsts	(sizeof(sysConstTable) / sizeof(SConstVar) - 1)

SConstVar sysVarTable[] = {
	{"$autoSave",		sv_autoSave,		V_Int,			VLit_autoSave},
	{"$bufFile",		sv_bufFile,		V_Nil,			VLit_bufFile},
	{"$bufLineNum",		sv_bufLineNum,		V_Int,			VLit_bufLineNum},
	{"$bufname",		sv_bufname,		0,			VLit_bufname},
	{"$execPath",		sv_execPath,		V_Nil,			VLit_execPath},
	{"$fencePause",		sv_fencePause,		V_Int,			VLit_fencePause},
	{"$hardTabSize",	sv_hardTabSize,		V_Int,			VLit_hardTabSize},
	{"$horzJump",		sv_horzJump,		V_Int,			VLit_horzJump},
	{"$inpDelim",		sv_inpDelim,		V_Nil,			VLit_inpDelim},
	{"$lastKeySeq",		sv_lastKeySeq,		V_GetKeySeq,		VLit_lastKeySeq},
	{"$lineChar",		sv_lineChar,		V_Char,			VLit_lineChar},
	{"$lineCol",		sv_lineCol,		V_Int,			VLit_lineCol},
	{"$lineOffset",		sv_lineOffset,		V_Int,			VLit_lineOffset},
	{"$lineText",		sv_lineText,		V_Nil,			VLit_lineText},
	{"$maxCallDepth",	sv_maxCallDepth,	V_Int,			VLit_maxCallDepth},
	{"$maxLoop",		sv_maxLoop,		V_Int,			VLit_maxLoop},
	{"$maxPromptPct",	sv_maxPromptPct,	V_Int,			VLit_maxPromptPct},
	{"$otpDelim",		sv_otpDelim,		V_Nil,			VLit_otpDelim},
	{"$pageOverlap",	sv_pageOverlap,		V_Int,			VLit_pageOverlap},
	{"$replacePat",		sv_replacePat,		V_Nil | V_EscDelim,	VLit_replacePat},
	{"$screenNum",		sv_screenNum,		V_Int,			VLit_screenNum},
	{"$searchDelim",	sv_searchDelim,		V_GetKey | V_ShowKey,	VLit_searchDelim},
	{"$searchPat",		sv_searchPat,		V_Nil | V_EscDelim,	VLit_searchPat},
	{"$softTabSize",	sv_softTabSize,		V_Int,			VLit_softTabSize},
	{"$travJump",		sv_travJump,		V_Int,			VLit_travJump},
	{"$vertJump",		sv_vertJump,		V_Int,			VLit_vertJump},
	{"$windLineNum",	sv_windLineNum,		V_Int,			VLit_windLineNum},
	{"$windNum",		sv_windNum,		V_Int,			VLit_windNum},
	{"$windSize",		sv_windSize,		V_Int,			VLit_windSize},
	{"$workDir",		sv_workDir,		0,			VLit_workDir},
	{"$wrapCol",		sv_wrapCol,		V_Int,			VLit_wrapCol},
	{NULL,			-1,			0,			NULL}};
#define NumSysVars	(sizeof(sysVarTable) / sizeof(SConstVar) - 1)

Hash sConstVarLookup;			// For fast lookup of system constants and variables.
#else

// **** For all the other .c files ****

// External variable declarations.
extern Hash gblConstVarTable;
extern LocalVar *localVarRoot;
extern SConstVar sysConstTable[];
extern SConstVar sysVarTable[];
extern Hash sConstVarLookup;
#endif
