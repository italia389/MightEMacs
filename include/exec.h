// (c) Copyright 2020 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// exec.h	Scripting definitions for MightEMacs.

// ProLib include files.
#include "cxl/array.h"

// Parsing definitions.
#define CmdFuncKeywd1	"arguments"	// Keyword for argument syntax string in first line of user routine definition.
#define CmdFuncKeywd2	"description"	// Keyword for description string in first line of user routine definition.

// All arrays are kept in a linked list of ArrayWrapper structures.  (See comments in expr.c for usage details.)
typedef struct ArrayWrapper {
	struct ArrayWrapper *next;	// Pointer to next item in list.
	Array *pArray;			// Pointer to array.
	bool marked;			// Used to prevent endless recursion in arrays that include self.
	} ArrayWrapper;

// Macro for fetching ArrayWrapper pointer from Datum object, given Datum *.
#define wrapPtr(pDatum)	((ArrayWrapper *) (pDatum)->u.blob.mem)

// Script invocation information.
typedef struct {
	char *path;			// Pathname of script loaded from a file.
	Buffer *pBuf;			// Buffer pointer to running script.
	Datum *pNArg;			// "n" argument.
	Datum *pArgs;			// User command/function (or buffer) arguments (array).
	ushort msgFlag;			// Prior state of 'RtnMsg' mode.
	struct UserVar *pVarStack;	// Local variables' "stack" pointer.
	} ScriptRun;

#define SRun_Parens	0x0001		// Invoked in xxx() form.

// Lexical symbols.
typedef enum {
	s_any = -1, s_nil, s_numLit, s_charLit, s_strLit, s_nArg, s_incr, s_decr, s_leftParen, s_rightParen, s_leftBracket,
	s_rightBracket, s_leftBrace, s_rightBrace, s_minus, s_plus, s_not, s_bitNot, s_mul, s_div, s_mod, s_leftShift,
	s_rightShift, s_bitAnd, s_bitOr, s_bitXOr, s_lt, s_le, s_gt, s_ge, s_eq, s_ne, s_regEQ, s_regNE, s_and, s_or, s_hook,
	s_colon, s_assign, s_assignAdd, s_assignSub, s_assignMul, s_assignDiv, s_assignMod, s_assignLeftShift,
	s_assignRightShift, s_assignBitAnd, s_assignBitXOr, s_assignBitOr, s_comma, s_globalVar, s_numVar, s_ident,
	s_identQuery, kw_and, kw_defn, kw_false, kw_in, kw_nil, kw_not, kw_or, kw_true,

	// Use bit masks for statement keywords so that they can be grouped by type.
	kw_break =	0x00000040,
	kw_command =	0x00000080,
	kw_else =	0x00000100,
	kw_elsif =	0x00000200,
	kw_endif =	0x00000400,
	kw_endloop =	0x00000800,
	kw_endroutine =	0x00001000,
	kw_for =	0x00002000,
	kw_force =	0x00004000,
	kw_function =	0x00008000,
	kw_if =		0x00010000,
	kw_loop =	0x00020000,
	kw_next =	0x00040000,
	kw_return =	0x00080000,
	kw_until =	0x00100000,
	kw_while =	0x00200000
	} Symbol;

// Statement types.
#define SLoopType	(kw_while | kw_until | kw_loop | kw_for)
#define SBreakType	(kw_break | kw_next)

// The while, until, for, and loop statements in the scripting language need to stack references to pending blocks.  These are
// stored in a linked list of the following structure, resolved at end-of-script, and saved in the buffer's CallInfo object.
typedef struct LoopBlock {
	struct LoopBlock *next;	// Next block in list.
	int type;			// Block type (statement Id).
	struct Line *pMarkLine;		// Pointer to while, until, for, loop, break, or next statement.
	struct Line *pJumpLine;		// Pointer to endloop statement.
	struct Line *pBreakLine;		// Pointer to parent's endloop statement, if any.
	} LoopBlock;

// Statement keyword informaton.
typedef struct {
	const char *name;		// Keyword.
	Symbol sym;			// Symbol id.
	} KeywordInfo;

// Expression statement parsing controls.
typedef struct {
	char *src;			// Beginning of next symbol.
	ushort flags;			// Prior OpEval flag.
	short termChar;			// Statement termination character (TokC_Comment or TokC_ExprEnd).
	Symbol sym;			// Type of last parsed symbol.
	Datum tok;			// Text of last parsed symbol.
	Datum *garbHead;		// Head of garbage collection list when parsing began.
	} Parse;

// Token characters.
#define TokC_Comment	'#'		// Line comment.
#define TokC_InlineComm0 '/'		// Beginning/end of in-line comment.
#define TokC_InlineComm1 '#'		// Secondary character of in-line comment.
#define TokC_GlobalVar	'$'		// Lead-in character for global variable or user routine argument.
#define TokC_Query	'?'		// Trailing character for a "query" function or user command/function name.
#define TokC_Expr	'#'		// Lead-in character for expression interpolation.
#define TokC_ExprBegin	'{'		// Beginning of interpolated expression in a string.
#define TokC_ExprEnd	'}'		// End of interpolated expression in a string.

// Expression evaluation controls and flags used by ge_xxx() functions.
typedef struct {
	Datum *pValue;			// Current expression value.
	ushort flags;			// Node flags.
	long nArg;			// "n" argument.
	ArraySize index;		// Array index i for "[...][i]" expression.
	} ExprNode;

#define EN_TopLevel	0x0001		// Evaluating expression at top level (parallel assignment is allowed).
#define EN_ArrayRef	0x0002		// Node is an array element reference (lvalue).
#define EN_HaveIdent	0x0004		// Found (primary) identifier.
#define EN_HaveGNVar	0x0008		// Found '$' variable name.
#define EN_HaveWhite	0x0010		// Found white space following identifier.
#define EN_HaveNArg	0x0020		// Found n argument -- function call must follow.
#define EN_LValue	0x0040		// First primary or postfix expression was an lvalue.
#define EN_Concat	0x0080		// Concatenating (bypass bitwise &).
#define EN_ParAssign	0x0100		// Processing a parallel assignment expression (x, y, z = [1, 2, 3]).

// External function declarations.
extern void awClearMarks(void);
extern int awGarbFree(void);
extern void awGarbPush(Datum *pDatum);
extern int array(Datum *pRtnVal, int n, Datum **args);
extern int arrayClone(Datum *pDest, Datum *pSrc, int depth);
extern int arrayEQ(Datum *pDatum1, Datum *pDatum2, bool ignore, bool *result);
extern bool isArrayVal(Datum *pDatum);
extern int ascToLong(const char *src, long *result, bool query);
extern int atosfclr(Datum *pDatum, const char *delim, uint flags, DFab *pFab);
extern int awWrap(Datum *pRtnVal, Array *pArray);
extern int catArgs(Datum *pRtnVal, int requiredCount, Datum *pDelim, uint flags);
extern bool isCharVal(Datum *pDatum);
extern int checkTabSize(int size, bool hard);
extern int clearHook(Datum *pRtnVal, int n, Datum **args);
#if MMDebug & (Debug_Datum | Debug_Script | Debug_MacArg | Debug_Preproc)
extern int debug_execBuf(Datum *pRtnVal, int n, Buffer *pBuf, char *runPath, uint flags, const char *caller);
#endif
extern bool isEmpty(Datum *pDatum);
#if MMDebug & Debug_MacArg
extern int quoteVal(Datum *pRtnVal, Datum *pDatum, uint flags);
#endif
extern int dtosf(Datum *pDatum, const char *delim, uint flags, DFab *pFab);
extern int dtosfchk(Datum *pDatum, const char *delim, uint flags, DFab *pFab);
extern char *dtype(Datum *pDatum, bool terse);
#if MMDebug & Debug_Expr
extern void dumpVal(Datum *pDatum);
#endif
extern int eval(Datum *pRtnVal, int n, Datum **args);
extern int evalCharLit(char **pSrc, short *pChar, bool allowNull);
extern int evalStrLit(Datum *pRtnVal, char *src);
extern int execBuf(Datum *pRtnVal, int n, Buffer *pBuf, char *runPath, uint flags);
extern int execCmdFunc(Datum *pRtnVal, int n, CmdFunc *pCmdFunc, int minArgs, int maxArgs);
extern int execExprStmt(Datum *pRtnVal, char *cmdLine, short termChar, char **pCmdLine);
extern int execFile(Datum *pRtnVal, const char *filename, int n, uint flags);
extern int execHook(Datum *pRtnVal, int n, HookRec *pHookRec, uint argInfo, ...);
extern bool extraSym(void);
extern char *fixNull(char *s);
extern int funcArg(Datum *pRtnVal, uint argFlags);
extern int ge_andOr(ExprNode *pNode);
extern int ge_assign(ExprNode *pNode);
extern Symbol getIdent(char **pSrc, ushort *pWordLen);
extern int getSym(void);
extern bool haveSym(Symbol sym, bool required);
extern bool haveWhite(void);
extern bool isIntVal(Datum *pDatum);
extern bool isHook(Buffer *pBuf, bool isError);
extern char *longToAsc(long n, char *dest);
extern bool needSym(Symbol sym, bool required);
extern int nextArg(Datum **ppRtnVal, Datum **ppInput, Datum *pWork, char **keywordList, Datum ***pppArrayEl,
 ArraySize *pElCount);
extern void nodeInit(ExprNode *pNode, Datum *pRtnVal, bool topLevel);
extern char *nonWhite(const char *s, bool skipInLine);
extern int parseTok(Datum *pDest, char **pSrc, short delimChar);
extern void preprocFree(Buffer *pBuf);
extern int quote(DFab *pFab, const char *src, bool full);
extern int revParseTok(Datum *pDest, char **pSrc, char *base, short delimChar);
extern int run(Datum *pRtnVal, int n, Datum **args);
extern int setHook(Datum *pRtnVal, int n, Datum **args);
extern int setTabSize(int size, bool hard);
extern int setWrapCol(int n, bool msg);
extern int showAliases(Datum *pRtnVal, int n, Datum **args);
extern int showCommands(Datum *pRtnVal, int n, Datum **args);
extern int showFunctions(Datum *pRtnVal, int n, Datum **args);
extern int showHooks(Datum *pRtnVal, int n, Datum **args);
extern int strSplit(Datum *pRtnVal, int n, Datum **args);
extern int strExpand(DFab *pFab, const char *src);
extern int strFormat(Datum *pRtnVal, Datum *pFormat, Datum *pArg);
extern char *stripStr(char *src, int op);
extern bool isStrVal(Datum *pDatum);
extern int substitute(Datum *pRtnVal, int n, Datum **args);
extern bool toBool(Datum *pDatum);
extern int toInt(Datum *pDatum);
extern int toStr(Datum *pDatum);
extern int validateArg(Datum *pDatum, uint argFlags);
#if 0
extern char *white(char *s);
#endif
extern int xeqBuf(Datum *pRtnVal, int n, Datum **args);
extern int xeqFile(Datum *pRtnVal, int n, Datum **args);

#ifdef ExecData

// **** For exec.c ****

// Global variables.
ArrayWrapper *arrayGarbHead = NULL;	// Head of array garbage collection list.
char *execPath = NULL;			// Search path for command files.
Parse *pLastParse = NULL;		// Last symbol parsed from a command line.
int maxLoop = MaxLoop;			// Maximum number of iterations allowed in a loop block.
int maxArrayDepth = MaxArrayDepth;	// Maximum depth of array recursion allowed when cloning, etc.
int maxCallDepth = MaxCallDepth;	// Maximum depth of user command or function recursion allowed.
ScriptRun *pScriptRun = NULL;		// Running buffer (script) information.
long defn = INT_MIN;			// Value of defn.
char wordChar[256];			// Characters considered "in a word".

#else

// **** For all the other .c files ****

// External variable declarations.
extern ArrayWrapper *arrayGarbHead;
extern char *execPath;
extern Parse *pLastParse;
extern int maxArrayDepth;
extern int maxLoop;
extern int maxCallDepth;
extern ScriptRun *pScriptRun;
extern long defn;
extern char wordChar[];
#endif
