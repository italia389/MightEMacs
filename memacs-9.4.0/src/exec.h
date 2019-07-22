// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// exec.h	Scripting definitions for MightEMacs.

// ProLib include files.
#include "plarray.h"

// Parsing definitions.
#define MacKeyword1	"usage"		// Keyword for usage string in first line of macro definition.
#define MacKeyword2	"desc"		// Keyword for description string in first line of macro definition.

// All arrays are kept in a linked list of ArrayWrapper structures.  (See comments in expr.c for usage details.)
typedef struct ArrayWrapper {
	struct ArrayWrapper *aw_next;	// Pointer to next item in list.
	Array *aw_ary;			// Pointer to array.
	bool aw_mark;			// Used to prevent endless recursion in arrays that include self.
	} ArrayWrapper;

// Macro for fetching ArrayWrapper pointer from Datum object, given Datum *.
#define awptr(datum)	((ArrayWrapper *) (datum)->u.d_blob.b_mem)

// Script invocation information.
typedef struct {
	char *path;			// Pathname of macro loaded from a file.
	struct Buffer *buf;		// Buffer pointer to running macro.
	Datum *narg;			// "n" argument.
	Datum *marg;			// Macro arguments (array).
	struct UVar *uvar;		// Local variables' "stack" pointer.
	} ScriptRun;

#define SRun_Parens	0x0001		// Invoked in xxx() form.

// Lexical symbols.
enum e_sym {
	s_any = -1,s_nil,s_nlit,s_clit,s_slit,s_narg,s_incr,s_decr,s_lparen,s_rparen,s_lbrkt,s_rbrkt,s_lbrace,s_rbrace,s_minus,
	s_plus,s_not,s_bnot,s_mul,s_div,s_mod,s_lsh,s_rsh,s_band,s_bor,s_bxor,s_lt,s_le,s_gt,s_ge,s_eq,s_ne,s_req,s_rne,s_and,
	s_or,s_hook,s_colon,s_assign,s_asadd,s_assub,s_asmul,s_asdiv,s_asmod,s_aslsh,s_asrsh,s_asband,s_asbxor,s_asbor,s_comma,
	s_gvar,s_nvar,s_ident,s_identq,
	kw_and,kw_defn,kw_false,kw_in,kw_nil,kw_not,kw_or,kw_true,

	// Use bit masks for statement keywords so that they can be grouped by type.
	kw_break =	0x00000040,
	kw_constrain =	0x00000080,
	kw_else =	0x00000100,
	kw_elsif =	0x00000200,
	kw_endif =	0x00000400,
	kw_endloop =	0x00000800,
	kw_endmacro =	0x00001000,
	kw_for =	0x00002000,
	kw_force =	0x00004000,
	kw_if =		0x00008000,
	kw_loop =	0x00010000,
	kw_macro =	0x00020000,
	kw_next =	0x00040000,
	kw_return =	0x00080000,
	kw_until =	0x00100000,
	kw_while =	0x00200000
	};

// Statement types.
#define SLoopType	(kw_while | kw_until | kw_loop | kw_for)
#define SBreakType	(kw_break | kw_next)

// The while, until, for, and loop statements in the scripting language need to stack references to pending blocks.  These are
// stored in a linked list of the following structure, resolved at end-of-script, and saved in the buffer's MacInfo object.
typedef struct LoopBlock {
	struct Line *lb_mark;		// Pointer to while, until, for, loop, break, or next statement.
	struct Line *lb_jump;		// Pointer to endloop statement.
	struct Line *lb_break;		// Pointer to parent's endloop statement, if any.
	int lb_type;			// Block type (statement ID).
	struct LoopBlock *lb_next;	// Next block in list.
	} LoopBlock;

typedef struct {
	char *name;			// Keyword.
	enum e_sym s;			// Id.
	} KeywordInfo;

// Expression statement parsing controls.
typedef struct {
	char *p_cl;			// Beginning of next symbol.
	ushort p_flags;			// Prior OpEval flag.
	short p_termch;			// Statement termination character (TokC_Comment or TokC_ExprEnd).
	enum e_sym p_sym;		// Type of last parsed symbol.
	Datum p_tok;			// Text of last parsed symbol.
	Datum *p_datGarb;		// Head of garbage collection list when parsing began.
	} Parse;

// Token characters.
#define TokC_ComLine	'#'		// Line comment.
#define TokC_ComInline0	'/'		// Beginning/end of in-line comment.
#define TokC_ComInline1	'#'		// Secondary character of in-line comment.
#define TokC_GVar	'$'		// Lead-in character for global variable or macro argument.
#define TokC_Query	'?'		// Trailing character for a "query" function or macro name.
#define TokC_Expr	'#'		// Lead-in character for expression interpolation.
#define TokC_ExprBegin	'{'		// Beginning of interpolated expression in a string.
#define TokC_ExprEnd	'}'		// End of interpolated expression in a string.

// Expression evaluation controls and flags used by ge_xxx() functions.
typedef struct {
	Datum *en_rp;			// Current expression value.
	ushort en_flags;		// Node flags.
	long en_narg;			// "n" argument.
	ArraySize en_index;		// Array index i for "[...][i]" expression.
	} ENode;

#define EN_TopLevel	0x0001		// Evaluating expression at top level (parallel assignment is allowed).
#define EN_ArrayRef	0x0002		// Node is an array element reference (lvalue).
#define EN_HaveIdent	0x0004		// Found (primary) identifier.
#define EN_HaveGNVar	0x0008		// Found '$' variable name.
#define EN_HaveWhite	0x0010		// Found white space following identifier.
#define EN_HaveNArg	0x0020		// Found n argument -- function call must follow.
#define EN_LValue	0x0040		// First primary or postfix expression was an lvalue.
#define EN_Concat	0x0080		// Concatenating (bypass bitwise &).
#define EN_PAssign	0x0100		// Processing a parallel assignment expression (x,y,z = [1,2,3]).

// External function declarations.
extern void aclrmark(void);
extern int agarbfree(void);
extern void agarbpush(Datum *datum);
extern int array(Datum *rval,int n,Datum **argv);
extern int aryclone(Datum *dest,Datum *src,int depth);
extern int aryeq(Datum *datum1,Datum *datum2,bool ignore,bool *result);
extern bool aryval(Datum *datum);
extern int asc_long(char *src,long *result,bool query);
extern int atosfclr(DStrFab *dest,Datum *src,char *dlm,uint flags);
extern int awrap(Datum *rval,Array *ary);
extern int catargs(Datum *rval,int reqct,Datum *delim,uint flags);
extern bool charval(Datum *datum);
extern int chktab(int size,bool hard);
#if MMDebug & (Debug_Datum | Debug_Script | Debug_MacArg | Debug_PPBuf)
extern int debug_execbuf(Datum *rval,int n,struct Buffer *buf,char *runpath,uint flags,char *caller);
#endif
extern bool disnn(Datum *datum);
#if MMDebug & Debug_MacArg
extern int dquote(Datum *rval,Datum *datum,uint flags);
#endif
extern int dtosf(DStrFab *dest,Datum *src,char *dlm,uint flags);
extern int dtosfchk(DStrFab *dest,Datum *src,char *dlm,uint flags);
extern char *dtype(Datum *datum,bool terse);
#if MMDebug & Debug_Expr
extern void dumpval(Datum *datum);
#endif
extern int eval(Datum *rval,int n,Datum **argv);
extern int evalclit(char **srcp,short *cp,bool allowNull);
extern int evalslit(Datum *rval,char *src);
extern int execbuf(Datum *rval,int n,struct Buffer *buf,char *runpath,uint flags);
extern int execCF(Datum *rval,int n,CmdFunc *cfp,int minArgs,int maxArgs);
extern int execestmt(Datum *rval,char *cl,short termch,char **cmdln);
extern int execfile(Datum *rval,char *fname,int n,uint flags);
extern int exechook(Datum *rval,int n,HookRec *hrec,uint arginfo,...);
extern bool extrasym(void);
extern char *fixnull(char *s);
extern int funcarg(Datum *rval,uint aflags);
extern int ge_andor(ENode *np);
extern int ge_assign(ENode *np);
extern enum e_sym getident(char **srcp,ushort *len);
extern int getsym(void);
extern bool havesym(enum e_sym sym,bool required);
extern bool havewhite(void);
extern bool intval(Datum *datum);
extern bool ishook(Buffer *buf,bool isError);
extern char *long_asc(long n,char *dest);
extern bool needsym(enum e_sym sym,bool required);
extern int nextarg(Datum **rvalp,Datum **input,Datum *work,char **kwlist,Datum ***elpp,ArraySize *elct);
extern void nodeinit(ENode *np,Datum *rval,bool toplev);
extern char *nonwhite(char *s,bool skipInLine);
extern int parsetok(Datum *dest,char **srcp,short delim);
extern void ppfree(struct Buffer *buf);
extern int quote(DStrFab *dest,char *src,bool full);
extern int rparsetok(Datum *dest,char **srcp,char *base,short delim);
extern int run(Datum *rval,int n,Datum **argv);
extern int setHook(Datum *rval,int n,Datum **argv);
extern int settab(int size,bool hard);
extern int setwrap(int n,bool msg);
extern int showAliases(Datum *rval,int n,Datum **argv);
extern int showCommands(Datum *rval,int n,Datum **argv);
extern int showFunctions(Datum *rval,int n,Datum **argv);
extern int showHooks(Datum *rval,int n,Datum **argv);
extern int showMacros(Datum *rval,int n,Datum **argv);
extern int ssplit(Datum *rval,int n,Datum **argv);
extern int strexpand(DStrFab *strloc,char *estr);
extern int strfmt(Datum *rval,Datum *format,Datum *arg1p);
extern char *stripstr(char *src,int op);
extern int strsub(Datum *rval,int n,Datum *sp,char *sstr,char *rstr,ushort flags);
extern bool strval(Datum *datum);
extern bool tobool(Datum *datum);
extern int toint(Datum *datum);
extern int tostr(Datum *datum);
extern int valarg(Datum *datum,uint aflags);
#if 0
extern char *white(char *s);
#endif
extern int xeqBuf(Datum *rval,int n,Datum **argv);
extern int xeqFile(Datum *rval,int n,Datum **argv);

#ifdef DATAexec

// **** For exec.c ****

// Global variables.
ArrayWrapper *aryGarb = NULL;		// Head of array garbage collection list.
char *execpath = NULL;			// Search path for command files.
Parse *last = NULL;			// Last symbol parsed from a command line.
int maxloop = MaxLoop;			// Maximum number of iterations allowed in a loop block.
int maxarydepth = MaxArrayDepth;	// Maximum depth of array recursion allowed when cloning, etc.
int maxmacdepth = MaxMacroDepth;	// Maximum depth of macro recursion allowed during script execution.
RtnCode scriptrc = {Success,0,""};	// Return code record for macro command.
ScriptRun *scriptrun = NULL;		// Running buffer (script) information.
long val_defn = INT_MIN;		// Value of defn.
char wordlist[256];			// Characters considered "in a word".
char wordlistd[] = DefWordList;		// Default word list.

#else

// **** For all the other .c files ****

// External variable declarations.
extern ArrayWrapper *aryGarb;
extern char *execpath;
extern Parse *last;
extern int maxarydepth;
extern int maxloop;
extern int maxmacdepth;
extern RtnCode scriptrc;
extern ScriptRun *scriptrun;
extern long val_defn;
extern char wordlist[];
extern char wordlistd[];
#endif
