// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// exec.h	Scripting definitions for MightEMacs.

// Parsing definitions.
#define CmdFuncKeywd1	"arguments"	// Keyword for argument syntax string in first line of user routine definition.
#define CmdFuncKeywd2	"description"	// Keyword for description string in first line of user routine definition.

// Script invocation information.
typedef struct {
	char *path;			// Pathname of script loaded from a file.
	Buffer *pBuf;			// Buffer pointer to running script.
	long n;				// "n" argument.
	Array *pArgs;			// User routine (or buffer) arguments (array).
	ushort msgFlag;			// Prior state of 'RtnMsg' mode.
	struct LocalVar *pVarStack;	// Local variables' "stack" pointer.
	} ScriptRun;

#define SRun_Parens	0x0001		// User routine invoked in xxx() form.
#define SRun_NoArgs	0x0002		// No-argument hook is running (so do not load arguments for user function).

// Lexical symbols.
typedef enum {
	s_any = -1, s_nil, s_numLit, s_charLit, s_strLit, s_bind, s_incr, s_decr, s_leftParen, s_rightParen, s_leftBracket,
	s_rightBracket, s_leftBrace, s_rightBrace, s_minus, s_plus, s_not, s_bitCompl, s_mult, s_div, s_mod, s_leftShift,
	s_rightShift, s_bitAnd, s_bitOr, s_bitXOr, s_lt, s_le, s_gt, s_ge, s_eq, s_ne, s_regEQ, s_regNE, s_and, s_or, s_hook,
	s_colon, s_assign, s_assignAdd, s_assignSub, s_assignMult, s_assignDiv, s_assignMod, s_assignLeftShift,
	s_assignRightShift, s_assignBitAnd, s_assignBitXOr, s_assignBitOr, s_comma,
	s_globalConst, s_globalVar, s_numVar, s_ident, s_identQuery,
	kw_and, kw_defn, kw_false, kw_in, kw_nil, kw_not, kw_or, kw_true,

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
// stored in a linked list of the following structure, resolved at end-of-script, and saved in the buffer's CallHdr object.
typedef struct LoopBlock {
	struct LoopBlock *next;		// Next block in list.
	int type;			// Block type (statement Id).
	struct Line *pBlockBegin;	// Pointer to while, until, for, loop, break, or next statement.
	struct Line *pEndLoop;		// Pointer to endloop statement.
	struct Line *pEndLoopUp;	// Pointer to parent's endloop statement, if any.
	} LoopBlock;

// Statement keyword informaton.
typedef struct {
	const char *name;		// Keyword.
	Symbol sym;			// Symbol id.
	} KeywordInfo;

// Expression statement parsing controls.
typedef struct {
	char *src;			// Beginning of next symbol.
	ushort flags;			// Prior OpScript flag.
	Symbol sym;			// Type of last parsed symbol.
	Datum tok;			// Text of last parsed symbol.
	Datum *trackHead;		// Head of Datum tracking list when parsing began.
	} Parse;

// Token characters.
#define TokC_Comment	'#'		// Line comment.
#define TokC_InlineComm0 '/'		// Beginning/end of in-line comment.
#define TokC_InlineComm1 '#'		// Secondary character of in-line comment.
#define TokC_GlobalVar	'$'		// Lead-in character for global or numeric variable.
#define TokC_Query	'?'		// Trailing character for a "query" function or user command/function name.
#define TokC_Expr	'#'		// Lead-in character for expression interpolation.
#define TokC_ExprBegin	'{'		// Beginning of interpolated expression in a string.
#define TokC_ExprEnd	'}'		// End of interpolated expression in a string.
#define TokC_StmtEnd	';'		// End of expression statement.

// Parsing flags or'd in to "flags" member of Parse record (in addition to session operation flags defined in std.h).
#define ParseExprEnd	0x8000		// Stop at TokC_ExprEnd character when parsing (otherwise, TokC_Comment).
#define ParseStmtEnd	0x4000		// Allow TokC_StmtEnd character to end parsing, in addition to usual ones.

// Expression evaluation controls and flags used by ge_xxx() functions.
typedef struct {
	Datum *pVal;			// Current expression value.
	ushort flags;			// Node flags.
	long nArg;			// "n" argument or array index for parallel assignment.
	union {
		ArraySize index;	// Array index i for "[...][i]" expression.
		Datum *pKey;		// Hash key for "{...}[key]" expression.
		} i;
	} ExprNode;

#define EN_TopLevel	0x0001		// Evaluating expression at top level (parallel assignment is allowed).
#define EN_HaveIdent	0x0002		// Found (primary) identifier (including '$name', '$n', and 'name').
#define EN_GNConstVar	0x0004		// Found '$name' or '$n' variable or 'Name' global constant.
#define EN_HaveWhite	0x0008		// Found white space following identifier.
#define EN_HaveLParen	0x0010		// Found left parenthesis immediately following identifier.
#define EN_HaveNArg	0x0020		// Found n argument -- function call must follow.
#define EN_LValue	0x0040		// First primary or postfix expression was an lvalue.
#define EN_AryElRef	0x0080		// Node is an array element reference (lvalue).
#define EN_HshNodRef	0x0100		// Node is a hash node reference (lvalue).
#define EN_ParAssign	0x0200		// Processing a parallel assignment expression (x, y, z = [1, 2, 3]).
#define EN_Format	0x0400		// Doing string format operation (bypass modulus %).
#define EN_Concat	0x0800		// Doing concatenation (bypass bitwise &).
#define EN_HashExpr	0x1000		// Processing "{key => value}" (bypass prefix op => if left operand is string).
#define EN_HaveNegSign	0x2000		// Found unary '-' preceding numeric literal.
#define EN_HaveLValue	(EN_GNConstVar | EN_AryElRef | EN_HshNodRef)

// Command argument validation flags.
#define ArgNotNull1	0x00000001	// First argument may not be null.
#define ArgNotNull2	0x00000002	// Second argument may not be null.
#define ArgNotNull3	0x00000004	// Third argument may not be null.
#define ArgNotNull4	0x00000008	// Fourth argument may not be null.
#define ArgNil1		0x00000010	// First argument may be nil.
#define ArgNil2		0x00000020	// Second argument may be nil.
#define ArgNil3		0x00000040	// Third argument may be nil.
#define ArgNil4		0x00000080	// Fourth argument may be nil.
#define ArgBool1	0x00000100	// First argument may be Boolean (true or false).
#define ArgBool2	0x00000200	// Second argument may be Boolean (true or false).
#define ArgBool3	0x00000400	// Third argument may be Boolean (true or false).
#define ArgBool4	0x00000800	// Fourth argument may be Boolean (true or false).
#define ArgInt1		0x00001000	// First argument must be integer.
#define ArgInt2		0x00002000	// Second argument must be integer.
#define ArgInt3		0x00004000	// Third argument must be integer.
#define ArgInt4		0x00008000	// Fourth argument must be integer.
#define ArgArray1	0x00010000	// First argument must be array (or "may" be if ArgNIS1 or ArgMay also set).
#define ArgArray2	0x00020000	// Second argument must be array (or "may" be if ArgNIS2 or ArgMay also set).
#define ArgArray3	0x00040000	// Third argument must be array (or "may" be if ArgNIS3 or ArgMay also set).
#define ArgArray4	0x00080000	// Fourth argument must be array (or "may" be if ArgNIS3 or ArgMay also set).
#define ArgHash1	0x00100000	// First argument must be hash (or "may" be if ArgNIS1 or ArgMay also set).
#define ArgHash2	0x00200000	// Second argument must be hash (or "may" be if ArgNIS2 or ArgMay also set).
#define ArgHash3	0x00400000	// Third argument must be hash (or "may" be if ArgNIS3 or ArgMay also set).
#define ArgHash4	0x00800000	// Fourth argument must be hash (or "may" be if ArgNIS3 or ArgMay also set).
#define ArgNIS1		0x01000000	// First argument can be nil, integer, or string.
#define ArgNIS2		0x02000000	// Second argument can be nil, integer, or string.
#define ArgNIS3		0x04000000	// Third argument can be nil, integer, or string.
#define ArgNIS4		0x08000000	// Fourth argument can be nil, integer, or string.
#define ArgMay		0x10000000	// ArgIntN, ArgArrayN, or ArgHashN "may" be instead of "must" be.
#define ArgAny1		(ArgBool1 | ArgNIS1 | ArgArray1 | ArgHash1)
#define ArgAny2		(ArgBool2 | ArgNIS2 | ArgArray2 | ArgHash2)
#define ArgAny3		(ArgBool3 | ArgNIS3 | ArgArray3 | ArgHash3)
#define ArgAny4		(ArgBool4 | ArgNIS4 | ArgArray4 | ArgHash4)

// Command argument control flags.  May be combined with Arg*, Md*, Op*, or Term_* flags so higher bits are used.
#define ArgFirst	0x40000000	// First argument (so no preceding comma).
#define ArgPath		0x80000000	// Argument is a file pathname to be expanded if string type.  (Applies to first
					// argument only if set in cmdFuncTable.)

#define dtypah(pDatum)	((pDatum)->type & (dat_array | dat_arrayRef | dat_hash | dat_hashRef))

// External function declarations.
extern int ahfree(void);
extern void astash(Datum *pDatum, Array *pArray);
extern void ahtrack(Datum *pDatum, bool scan);
extern int ascToLong(const char *src, int base, long *result, bool query);
extern int catArgs(Datum *pRtnVal, int minRequired, Datum *pDelim, uint cflags);
extern int checkTabSize(long size, bool hard);
extern int checkWrapCol(long col);
extern int clearHook(Datum *pRtnVal, long n, Datum **args);
#if MMDebug & (Debug_Datum | Debug_Script | Debug_MacArg | Debug_Preproc)
extern int debug_execBuf(Datum *pRtnVal, long n, Buffer *pBuf, char *runPath, uint flags, const char *caller);
#endif
extern void dtpop(Datum *pDatum);
extern char *dtype(Datum *pDatum, bool terse);
extern int eval(Datum *pRtnVal, long n, Datum **args);
extern int evalCharLit(char **pSrc, short *pChar, bool allowNull);
extern int evalStrLit(Datum *pRtnVal, char *src);
extern int execBuf(Datum *pRtnVal, long n, Buffer *pBuf, char *runPath, uint flags);
extern int execCmdFunc(Datum *pRtnVal, long n, CmdFunc *pCmdFunc, int minArgs, int maxArgs);
extern int execExprStmt(Datum *pRtnVal, char *cmdLine, ushort flags, char **pCmdLine);
extern int execHook(Datum *pRtnVal, long n, HookRec *pHookRec, uint argInfo, ...);
extern bool extraSym(void);
extern char *fixNull(char *s);
extern int funcArg(Datum *pRtnVal, uint argFlags);
extern int ge_andOr(ExprNode *pNode);
extern int ge_assign(ExprNode *pNode);
extern Symbol getIdent(char **pSrc, ushort *pWordLen);
extern int getSym(void);
extern bool haveArg(long n, Datum **pArg, bool exists);
extern bool haveSym(Symbol sym, bool required);
extern bool haveWhite(void);
extern void hstash(Datum *pDatum, Hash *pHash);
extern bool isArrayVal(Datum *pDatum);
extern bool isAryHshStrVal(Datum *pDatum);
extern bool isCharVal(Datum *pDatum);
extern bool isIntVal(Datum *pDatum);
extern bool isHook(Buffer *pBuf, bool isError);
extern bool isNN(Datum *pDatum);
extern bool isStrVal(Datum *pDatum);
extern char *longToAsc(long n, char *dest);
extern bool needSym(Symbol sym, bool required);
extern int nextArg(Datum **ppRtnVal, Datum **ppInput, Datum *pWork, char **keywordList, Datum ***pppArrayEl,
 ArraySize *pElCount);
extern bool nextChar(short c);
extern void nodeInit(ExprNode *pNode, Datum *pRtnVal, ushort flags);
extern char *nonWhite(const char *s, bool skipInLine);
extern int parseTok(Datum *pDest, char **pSrc, short delimChar);
extern void preprocFree(Buffer *pBuf);
extern int revParseTok(Datum *pDest, char **pSrc, char *base, short delimChar);
extern int run(Datum *pRtnVal, long n, Datum **args);
extern int setHook(Datum *pRtnVal, long n, Datum **args);
extern int setTabSize(long size, bool hard);
extern int setWrapCol(long col);
extern int showAliases(Datum *pRtnVal, long n, Datum **args);
extern int showHooks(Datum *pRtnVal, long n, Datum **args);
extern int showRoutines(Datum *pRtnVal, long n, Datum **args);
extern int strSplit(Datum *pRtnVal, long n, Datum **args);
extern int strExpand(DFab *pFab, const char *src);
extern int strFormat(Datum *pRtnVal, Datum *pFormat, Datum *pArg);
extern char *stripStr(char *src, int op);
extern int substitute(Datum *pRtnVal, long n, Datum **args);
extern bool toBool(Datum *pDatum);
extern int toInt(Datum *pDatum, int base);
extern int toStr(Datum *pDatum);
extern int xeqBuf(Datum *pRtnVal, long n, Datum **args);
extern int xeqFile(Datum *pRtnVal, long n, Datum **args);

#ifdef ExecData

// **** For exec.c ****

// Global variables.
char *execPath = NULL;			// Search path for command files.
Parse *pLastParse = NULL;		// Last symbol parsed from a command line.
int maxLoop = MaxLoop;			// Maximum number of iterations allowed in a loop block.
int maxCallDepth = MaxCallDepth;	// Maximum depth of user command or function recursion allowed.
ScriptRun *pScriptRun = NULL;		// Running buffer (script) information.
long defn = DefN;			// Value of defn.
char wordChar[256];			// Characters considered "in a word".

#else

// **** For all the other .c files ****

// External variable declarations.
extern char *execPath;
extern Parse *pLastParse;
extern int maxLoop;
extern int maxCallDepth;
extern ScriptRun *pScriptRun;
extern long defn;
extern char wordChar[];
#endif
