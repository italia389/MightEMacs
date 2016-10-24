// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// expr.c	Core expresion evaluation routines for MightEMacs.

#include "os.h"
#include <errno.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"
#include "evar.h"

// Ruby operator precedence (highest to lowest):
//	Precedence	Operator	Description					Associativity
//	==============	==============	==============================================	=============
//			!		Logical NOT					Right-to-left
//	1		~		Bitwise NOT (one's complement)
//			+		Unary plus
//	---------------------------------------------------------------------------------------------
//	2		**		Exponentiation					Left-to-right
//	---------------------------------------------------------------------------------------------
//	3		-		Unary minus					Right-to-left
//	---------------------------------------------------------------------------------------------
//			*		Multiplication					Left-to-right
//	4		/		Division
//			%		Modulo (remainder)
//	---------------------------------------------------------------------------------------------
//	5		+		Addition					Left-to-right
//			-		Subtraction
//	---------------------------------------------------------------------------------------------
//	6		<<		Bitwise left shift				Left-to-right
//			>>		Bitwise right shift
//	---------------------------------------------------------------------------------------------
//	7		&		Bitwise AND					Left-to-right
//	---------------------------------------------------------------------------------------------
//	8		|		Bitwise OR (inclusive or)			Left-to-right
//			^		Bitwise XOR (exclusive or)
//	---------------------------------------------------------------------------------------------
//			>		Greater than					Left-to-right
//	9		>=		Greater than or equal to
//			<		Less than
//			<=		Less than or equal to
//	---------------------------------------------------------------------------------------------
//			<=>		Comparison					Left-to-right
//			==		Equal to
//	10		!=		Not equal to
//			=~		RE equal to
//			!~		RE not equal to
//	---------------------------------------------------------------------------------------------
//	11		&&		Logical AND					Left-to-right
//	---------------------------------------------------------------------------------------------
//	12		||		Logical OR					Left-to-right
//	---------------------------------------------------------------------------------------------
//	13		..		Range, including last element			Left-to-right
//			...		Range, excluding last element
//	---------------------------------------------------------------------------------------------
//	14		?:		Ternary conditional				Right-to-left
//	---------------------------------------------------------------------------------------------
//			=		Direct assignment				Right-to-left
//			+=		Assignment by sum
//			-=		Assignment by difference
//			*=		Assignment by product
//			/=		Assignment by quotient
//	15		%=		Assignment by remainder
//			<<=		Assignment by bitwise left shift
//			>>=		Assignment by bitwise right shift
//			&=		Assignment by bitwise AND
//			^=		Assignment by bitwise XOR
//			|=		Assignment by bitwise OR
//	---------------------------------------------------------------------------------------------
//	16		defined?							Right-to-left
//	---------------------------------------------------------------------------------------------
//	17		not		Logical NOT (low precedence)			Right-to-left
//	---------------------------------------------------------------------------------------------
//	18		or		Logical OR (low precedence)			Left-to-right
//			and		Logical AND (low precedence)
//	---------------------------------------------------------------------------------------------

// C operator precedence (highest to lowest):
//	Precedence	Operator	Description					Associativity
//	==============	==============	==============================================	=============
//	1	X	::		Scope resolution (C++ only)			None
//	---------------------------------------------------------------------------------------------
//			++		Suffix increment				Left-to-right
//			--		Suffix decrement
//	2		()		Function call
//			[]		Array subscripting
//			.		Element selection by reference
//			->		Element selection through pointer
//	---------------------------------------------------------------------------------------------
//			++		Prefix increment				Right-to-left
//			--		Prefix decrement
//			+		Unary plus
//			-		Unary minus
//	3		!		Logical NOT
//			~		Bitwise NOT (one's complement)
//			(type)		Type cast
//			*		Indirection (dereference)
//			&		Address-of
//			sizeof		Size-of
//	---------------------------------------------------------------------------------------------
//	4	X	.*		Pointer to member (C++ only)			Left-to-right
//			->*		Pointer to member (C++ only)
//	---------------------------------------------------------------------------------------------
//			*		Multiplication					Left-to-right
//	5		/		Division
//			%		Modulo (remainder)
//	---------------------------------------------------------------------------------------------
//	6		+		Addition					Left-to-right
//			-		Subtraction
//	---------------------------------------------------------------------------------------------
//	7		<<		Bitwise left shift				Left-to-right
//			>>		Bitwise right shift
//	---------------------------------------------------------------------------------------------
//			<		Less than					Left-to-right
//	8		<=		Less than or equal to
//			>		Greater than
//			>=		Greater than or equal to
//	---------------------------------------------------------------------------------------------
//	9		==		Equal to					Left-to-right
//			!=		Not equal to
//	---------------------------------------------------------------------------------------------
//	10		&		Bitwise AND					Left-to-right
//	---------------------------------------------------------------------------------------------
//	11		^		Bitwise XOR (exclusive or)			Left-to-right
//	---------------------------------------------------------------------------------------------
//	12		|		Bitwise OR (inclusive or)			Left-to-right
//	---------------------------------------------------------------------------------------------
//	13		&&		Logical AND					Left-to-right
//	---------------------------------------------------------------------------------------------
//	14		||		Logical OR					Left-to-right
//	---------------------------------------------------------------------------------------------
//	15		?:		Ternary conditional				Right-to-left
//	---------------------------------------------------------------------------------------------
//			=		Direct assignment				Right-to-left
//			+=		Assignment by sum
//			-=		Assignment by difference
//			*=		Assignment by product
//			/=		Assignment by quotient
//	16		%=		Assignment by remainder
//			<<=		Assignment by bitwise left shift
//			>>=		Assignment by bitwise right shift
//			&=		Assignment by bitwise AND
//			^=		Assignment by bitwise XOR
//			|=		Assignment by bitwise OR
//	---------------------------------------------------------------------------------------------
//	17		throw		Throw operator (exceptions throwing, C++ only)	Right-to-left
//	---------------------------------------------------------------------------------------------
//	18		,		Comma						Left-to-right
//	---------------------------------------------------------------------------------------------
//
// Notes:
// The precedence table determines the order of binding in chained expressions, when it is not expressly specified by
// parentheses.
//	. For example, ++x*3 is ambiguous without some precedence rule(s). The precedence table tells us that: x is 'bound'
//	  more tightly to ++ than to *, so that whatever ++ does (now or later -- see below), it does it ONLY to x (and not to
//	  x*3); it is equivalent to (++x, x*3).
//	. Similarly, with 3*x++, where though the post-fix ++ is designed to act AFTER the entire expression is evaluated, the
//	  precedence table makes it clear that ONLY x gets incremented (and NOT 3*x). In fact, the expression (tmp=x++, 3*tmp)
//	  is evaluated with tmp being a temporary value. It is functionally equivalent to something like (tmp=3*x, ++x, tmp).

// MightEMacs operator precedence (highest to lowest):
//	Precedence	Operator	Description					Associativity
//	==============	==============	==============================================	=============
//			++		Suffix increment				Left-to-right
//			--		Suffix decrement
//	1		()		Function call
//			<whitespace>	Function call
//			[]		Array subscripting
//	---------------------------------------------------------------------------------------------
//			++		Prefix increment				Right-to-left
//			--		Prefix decrement
//			+		Unary plus
//	2		-		Unary minus
//			!		Logical NOT
//			~		Bitwise NOT (one's complement)
//	---------------------------------------------------------------------------------------------
//			*		Multiplication					Left-to-right
//	3		/		Division
//			%		Modulo (if first operand is integer)
//	---------------------------------------------------------------------------------------------
//	4		+		Addition					Left-to-right
//			-		Subtraction
//	---------------------------------------------------------------------------------------------
//	5		=>		Numeric prefix (n)				Left-to-right
//	---------------------------------------------------------------------------------------------
//	6		<<		Bitwise left shift				Left-to-right
//			>>		Bitwise right shift
//	---------------------------------------------------------------------------------------------
//	7		&		Bitwise AND (if both operands are integers)	Left-to-right
//	---------------------------------------------------------------------------------------------
//	8		|		Bitwise OR (inclusive or)			Left-to-right
//			^		Bitwise XOR (exclusive or)
//	---------------------------------------------------------------------------------------------
//	9		%		String formatting (if first operand is string)	Left-to-right
//	---------------------------------------------------------------------------------------------
//	10		&		Concatenation (if first operand is string)	Left-to-right
//	---------------------------------------------------------------------------------------------
//			<		Less than					Left-to-right
//	11		<=		Less than or equal to
//			>		Greater than
//			>=		Greater than or equal to
//	---------------------------------------------------------------------------------------------
//			<=>		Comparison					Left-to-right
//			==		Equal to
//	12		!=		Not equal to
//			=~		RE equal to
//			!~		RE not equal to
//	---------------------------------------------------------------------------------------------
//	13		&&		Logical AND					Left-to-right
//	---------------------------------------------------------------------------------------------
//	14		||		Logical OR					Left-to-right
//	---------------------------------------------------------------------------------------------
//	15		?:		Ternary conditional				Right-to-left
//	---------------------------------------------------------------------------------------------
//			=		Direct assignment				Right-to-left
//			+=		Assignment by sum
//			-=		Assignment by difference
//			*=		Assignment by product
//			/=		Assignment by quotient
//	16		%=		Assignment by remainder
//			<<=		Assignment by bitwise left shift
//			>>=		Assignment by bitwise right shift
//			&=		Assignment by bitwise AND
//			|=		Assignment by bitwise OR
//			^=		Assignment by bitwise XOR
//	---------------------------------------------------------------------------------------------
//	17		not		Logical NOT (low precedence)			Right-to-left
//	---------------------------------------------------------------------------------------------
//	18		or		Logical OR (low precedence)			Left-to-right
//			and		Logical AND (low precedence)
//	---------------------------------------------------------------------------------------------
//	19		,		Comma						Left-to-right
//	---------------------------------------------------------------------------------------------

// Binary operator info.
typedef struct {
	int (*xfunc)(ENode *np);	// Function at next higher level.
	enum e_sym *symp;		// Valid operator token(s).
	ushort flags;			// Kind of operation.
	} OpInfo;

// forcefit() types.
#define FF_MATH		0x0001		// Add, sub, mul, div or mod.
#define FF_SHFT		0x0002		// Left or right bit shift.
#define FF_BITOP	0x0004		// &, | or ^.
#define FF_FORMAT	0x0008		// String format %.
#define FF_CONCAT	0x0010		// Concatenation.
#define FF_REL		0x0020		// <, <=, > or >=.
#define FF_REQNE	0x0040		// =~ or !~.
#define FF_EQNE		0x0080		// == or !=.
#define FF_LANDOR	0x0100		// && or ||.
#define FF_COND		0x0200		// Conditional (hook).
#define FF_ASSIGN	0x0400		// Straight assignment (=).

#define STR_LEFT	0x1000		// Convert left operand to string.
#define STR_RIGHT	0x2000		// Convert right operand to string.
#define FF_OPMASK	0x0fff

// forcefit() table for nil, bool (true or false), int, and string coersion combinations.
static struct ffinfo {
	ushort legal;			// Legal operations (FF_XXX flags)
	ushort str_op;			// Operations which cause call to tostr().  High bits determine left or right side.
	} fftb[][4] = {
		{ /* nil */
/* nil */	{FF_ASSIGN | FF_CONCAT | FF_EQNE | FF_LANDOR | FF_COND,0},
/* bool */	{FF_ASSIGN | FF_EQNE | FF_LANDOR | FF_COND,0},
/* int */	{FF_ASSIGN | FF_CONCAT | FF_EQNE | FF_LANDOR | FF_COND,FF_CONCAT | FF_EQNE | STR_RIGHT},
/* string */	{FF_ASSIGN | FF_CONCAT | FF_EQNE | FF_LANDOR | FF_COND,0}},

		{ /* bool */
/* nil */	{FF_ASSIGN | FF_EQNE | FF_LANDOR | FF_COND,0},
/* bool */	{FF_ASSIGN | FF_EQNE | FF_LANDOR | FF_COND,0},
/* int */	{FF_ASSIGN | FF_EQNE | FF_LANDOR | FF_COND,FF_EQNE | STR_RIGHT},
/* string */	{FF_ASSIGN | FF_EQNE | FF_LANDOR | FF_COND,0}},

		{ /* int */
/* nil */	{FF_ASSIGN | FF_EQNE | FF_LANDOR | FF_COND,FF_EQNE | STR_LEFT},
/* bool */	{FF_ASSIGN | FF_EQNE | FF_LANDOR | FF_COND,FF_EQNE | STR_LEFT},
/* int */	{~(FF_CONCAT | FF_FORMAT | FF_REQNE),0},
/* string */	{FF_ASSIGN | FF_LANDOR | FF_COND,0}},

		{ /* string */
/* nil*/	{FF_ASSIGN | FF_CONCAT | FF_FORMAT | FF_EQNE | FF_LANDOR | FF_COND,0},
/* bool*/	{FF_ASSIGN | FF_FORMAT | FF_EQNE | FF_LANDOR | FF_COND,0},
/* int */	{FF_ASSIGN | FF_CONCAT | FF_FORMAT | FF_LANDOR | FF_COND,FF_CONCAT | STR_RIGHT},
/* string */	{FF_ASSIGN | FF_CONCAT | FF_FORMAT | FF_REL | FF_REQNE | FF_EQNE | FF_LANDOR | FF_COND,0}}
		};

#if MMDEBUG & DEBUG_EXPR
static int indent = -1;
#endif

// Forwards.
static int ge_andor(ENode *np);

// Initialize an expression node with given value object.
void nodeinit(ENode *np,Value *rp) {

	vnull(rp);
	np->en_rp = rp;
	np->en_flags = 0;
	np->en_narg = INT_MIN;
	}

// Return true if node value is an lvalue; otherwise, set an error and return false.
static bool lvalue(ENode *np) {

	if((np->en_flags & EN_HAVEGNVAR) || ((np->en_flags & EN_HAVEIDENT) && uvarfind(np->en_rp->v_strp) != NULL))
		return true;
	if(np->en_flags & EN_HAVEIDENT)
		(void) rcset(FAILURE,0,text52,np->en_rp->v_strp);
				// "No such variable '%s'"
	else
		(void) rcset(FAILURE,0,text4,text82,last->p_tok.v_strp);
				// "%s expected (at token '%s')","Variable name"
	return false;
	}

#if MMDEBUG & DEBUG_EXPR
// Write integer or string value to log file, given value object.
void dumpval(Value *vp) {
	if(vp->v_type == VALINT)
		fprintf(logfile,"(%.8x %hu) %ld",(uint) vp,vp->v_type,vp->u.v_int);
	else
		fprintf(logfile,"(%.8x %hu) \"%s\"",(uint) vp,vp->v_type,vp->v_strp);
	}

// Write integer or string value to log file, given node.
static void ge_dumpval(ENode *np) {
	dumpval(np->en_rp);
	}

// Write expression parsing info to log file.
static void ge_dump(char *fname,ENode *np1,ENode *np2,bool begin) {
	int i = indent;
	while(i-- > 0)
		fputs("  ",logfile);
	fprintf(logfile,"%s %s(): %s '%s' (%d), cur val ",begin ? "BEGIN" : "END",fname,begin ? "parsing" : "returning at",
	 last->p_tok.v_strp,last->p_sym);
	ge_dumpval(np1);
	fprintf(logfile,", flags %.8x",np1->en_flags);
	if(np2 != NULL) {
		fputs(", val2 ",logfile);
		ge_dumpval(np2);
		fprintf(logfile,", flags %.8x",np2->en_flags);
		}
	fprintf(logfile,", EvalMode %x",opflags & OPEVAL);
	if(begin)
		fputs("...\n",logfile);
	else
		fprintf(logfile," [%d]\n",rc.status);
	fflush(logfile);
	}

// Intercept all calls to ge_xxx() functions and write status information to log file.
static int ge_debug1(char *fname,ENode *np1,ENode *np2,int (*xfunc)(ENode *np)) {
	int status;

	++indent;
	ge_dump(fname,np1,np2,true);
	status = xfunc(np2 != NULL ? np2 : np1);
	ge_dump(fname,np1,np2,false);
	--indent;
	return status;
	}
#endif

// Update EN_HAVENIL and EN_HAVEBOOL flags in given node.
static void updtfn(ENode *np) {

	if(vistfn(np->en_rp,VNIL))
		np->en_flags |= EN_HAVENIL;
	else if(vistfn(np->en_rp,VBOOL))
		np->en_flags |= EN_HAVEBOOL;
	}

// Dereference an lvalue (variable name) in np if present and evaluating.  Return status.
static int ge_deref(ENode *np) {

	if(!(opflags & OPEVAL))

		// Not evaluating.  Just clear flags.
		np->en_flags &= ~(EN_HAVEIDENT | EN_HAVEGNVAR | EN_HAVEWHITE);

	// Evaluating.  Dereference and clear flags.
	else if((np->en_flags & EN_HAVEGNVAR) ||
	 ((np->en_flags & EN_HAVEIDENT) && cfabsearch(np->en_rp->v_strp,NULL,PTRCFAM))) {
		(void) derefn(np->en_rp,np->en_rp->v_strp);
		np->en_flags &= ~(EN_HAVEIDENT | EN_HAVEGNVAR | EN_HAVEWHITE);
		if(rc.status == SUCCESS)
			updtfn(np);
		}

	return rc.status;
	}

// Coerce value objects passed into compatable types for given operation flag(s) and return status.  If illegal fit, return
// error.  "kind" contains operation flag (FF_XXX) and "op" is operator token (for error reporting).
static int forcefit(ENode *np1,ENode *np2,ushort kind,char *op) {

	// Get info from table.
	struct ffinfo *info = fftb[(np1->en_flags & EN_HAVENIL) ? 0 : (np1->en_flags & EN_HAVEBOOL) ? 1 :
	 np1->en_rp->v_type == VALINT ? 2 : 3] +
	 ((np2->en_flags & EN_HAVENIL) ? 0 : (np2->en_flags & EN_HAVEBOOL) ? 1 : np2->en_rp->v_type == VALINT ? 2 : 3);

	// Valid operand types?
	if(!(info->legal & kind))
#if MMDEBUG & DEBUG_EXPR
		return rcset(FAILURE,0,"Wrong operand type for '%s' (legal %.4hx, kind %.4hx)",op,info->legal,kind);
#else
		return rcset(FAILURE,0,text191,op);
				// "Wrong type of operand for '%s'"
#endif
	// Coerce one operand to string if needed.
	if(info->str_op & kind) {
#if MMDEBUG & DEBUG_EXPR
		ENode *np;
		char *strp;
		if(info->str_op & STR_LEFT) {
			np = np1;
			strp = "left";
			}
		else {
			np = np2;
			strp = "right";
			}
		fprintf(logfile,"### forcefit(): coercing %s operand...\n",strp);
		tostr(np->en_rp,0);
#else
		tostr((info->str_op & STR_LEFT) ? np1->en_rp : np2->en_rp,0);
#endif
		}
	return rc.status;
	}

// Parse a primary expression and save the value in np.  If an identifier is found, save its name and set appropriate flags in
// np as well.  Return status.  Primary expressions are any of:
//	number
//	string
//	identifier
//	"true" | "false" | "nil" | "defn"
//	(and-or-expression)
static int ge_primary(ENode *np) {
	char *strp;

	switch(last->p_sym) {
		case s_nlit:
			if(opflags & OPEVAL) {
				long lval;

				if(asc_long(last->p_tok.v_strp,&lval,false) != SUCCESS)
					return rc.status;
				vsetint(lval,np->en_rp);
				}
			goto nextsym;
		case s_slit:
			// String literal.
			(void) evalslit(np->en_rp,last->p_tok.v_strp);
			break;
		case kw_true:					// Convert to true value.
			strp = val_true;
			goto isbool;
		case kw_false:					// Convert to false value.
			strp = val_false;
isbool:
			np->en_flags |= EN_HAVEBOOL;
			goto echeck;
		case kw_nil:					// Convert to nil value.
			strp = val_nil;
			np->en_flags |= EN_HAVENIL;
echeck:
			if((opflags & OPEVAL) && vsetstr(strp,np->en_rp) != 0)
				return vrcset();
			goto nextsym;
		case kw_defn:					// Convert to defn value.
			if(opflags & OPEVAL)
				vsetint(val_defn,np->en_rp);
			goto nextsym;
		case s_gvar:
		case s_nvar:
			np->en_flags |= EN_HAVEGNVAR;
			// Fall through.
		case s_ident:
		case s_identq:
			np->en_flags |= EN_HAVEIDENT;

			// Save identifier name in np.
			if(vsetstr(last->p_tok.v_strp,np->en_rp) != 0)
				return vrcset();

			// Set "white-space-after-identifier" flag for caller.
			if(havewhite())
				np->en_flags |= EN_HAVEWHITE;
			goto nextsym;
		case s_lparen:
			// Parenthesized expression.
#if MMDEBUG & DEBUG_EXPR
			if(getsym() < NOTFOUND || ge_debug1("ge_comma",np,NULL,ge_comma) != SUCCESS || !havesym(s_rparen,true))
#else
			if(getsym() < NOTFOUND || ge_comma(np) != SUCCESS || !havesym(s_rparen,true))
#endif
				return rc.status;
nextsym:
			(void) getsym();		// Past ')'.
			break;
		default:
			if(last->p_sym == s_nil)
				(void) rcset(FAILURE,0,text172);
						// "Token expected"
			else
				(void) rcset(FAILURE,0,text289,last->p_tok.v_strp);
						// "Unexpected token '%s'"
		}

	return rc.status;
	}

// Handle a function (command, alias, function, or macro) call.
static int fcall(ENode *np,bool needrparen,bool *foundp) {
	CFABPtr cfab;

	// Is identifier a command, alias, function, or macro?
#if MMDEBUG & (DEBUG_VALUE | DEBUG_EXPR)
	fprintf(logfile,"fcall(): identifying '%s' ... ",np->en_rp->v_strp);
#endif
	if(!cfabsearch(np->en_rp->v_strp,&cfab,PTRCFAM)) {
#if MMDEBUG & (DEBUG_VALUE | DEBUG_EXPR)
		fputs("found '",logfile);
		switch(cfab.p_type) {
			case PTRALIAS_C:
			case PTRALIAS_F:
			case PTRALIAS_M:
				fprintf(logfile,"%s' ALIAS",cfab.u.p_aliasp->a_name);
				break;
			case PTRMACRO:
				fprintf(logfile,"%s' MACRO",cfab.u.p_bufp->b_bname);
				break;
			case PTRCMD:
			case PTRFUNC:
				fprintf(logfile,"%s' %s",cfab.p_type == PTRCMD ? "COMMAND" : "FUNCTION",cfab.u.p_cfp->cf_name);
			}
		fprintf(logfile," (type %hu) for token '%s'\n",cfab.p_type,np->en_rp->v_strp);
#endif
		// Yes.  Resolve any alias.
		if(cfab.p_type & PTRALIAS)
			cfab = cfab.u.p_aliasp->a_cfab;

		// Check if interactive-only command.
		if(cfab.p_type == PTRCMD) {
			if(cfab.u.p_cfp->cf_flags & CFTERM)
				return rcset(FAILURE,0,text282,cfab.u.p_cfp->cf_name);
					// "'%s' command not allowed in a macro (use \"run\")"

			// If "alias" command (which uses "alias xxx = yyy" syntax), parentheses not allowed.
			if(needrparen && cfab.u.p_cfp->cf_func == aliasCFM)
				return rcset(FAILURE,0,text289,"(");
						// "Unexpected token '%s'"
			}
		if(foundp != NULL)
			*foundp = true;

		// Determine minimum number of required arguments, if possible.  Set to -1 if unknown.
		short minArgs = (cfab.p_type == PTRMACRO) ? (cfab.u.p_bufp->b_nargs < 0 ? 0 : cfab.u.p_bufp->b_nargs) :
		 !(cfab.u.p_cfp->cf_flags & (CFADDLARG | CFNOARGS)) ? cfab.u.p_cfp->cf_minArgs :
		 !(opflags & OPEVAL) ? -1 : np->en_narg == INT_MIN ? cfab.u.p_cfp->cf_minArgs :
		 (cfab.u.p_cfp->cf_flags & CFNOARGS) ? 0 : cfab.u.p_cfp->cf_minArgs + 1;

		// Check if "xxx()" call.
		if(needrparen && havesym(s_rparen,false)) {

			// Error if argument(s) required (whether or not evaluating).
			if(minArgs > 0 || ((cfab.u.p_cfp->cf_flags & CFNOARGS) && !(np->en_flags & EN_HAVENARG)))
				goto wrong;
			if(!(opflags & OPEVAL) && !(cfab.u.p_cfp->cf_flags & CFSPECARGS))
				goto out;
			}

		// Not "xxx()" call or argument requirement cannot be determined.  Proceed with execution or argument
		// consumption.  Note that unusual expressions such as "false && x => seti(5)" where the "seti(5)" is likely an
		// error are not checked here because the numeric prefix is not known when not evaluating.  The specific command
		// or function routine will do the validation.
		short maxArgs = (cfab.p_type == PTRMACRO) ? (cfab.u.p_bufp->b_nargs < 0 ? SHRT_MAX : cfab.u.p_bufp->b_nargs) :
		 ((opflags & OPEVAL) && np->en_narg != INT_MIN && (cfab.u.p_cfp->cf_flags & CFNOARGS)) ? 0 :
		 cfab.u.p_cfp->cf_maxArgs < 0 ? SHRT_MAX : cfab.u.p_cfp->cf_maxArgs;
		opflags = (opflags & ~OPPARENS) | (needrparen ? OPPARENS : 0);

		// Call the command, function, or macro (as a function) if it's a command or function and CFSPECARGS is set, or
		// evaluating and (1), it's a macro; or (2), it's a command or function and the n argument is not zero or not
		// just a repeat count.
		if(((cfab.p_type & (PTRCMD | PTRFUNC)) && (cfab.u.p_cfp->cf_flags & CFSPECARGS)) || ((opflags & OPEVAL) &&
		 (cfab.p_type == PTRMACRO || np->en_narg != 0 || !(cfab.u.p_cfp->cf_flags & CFNCOUNT)))) {

			// Clear node flags.
			np->en_flags &= EN_CONCAT;

			// Call macro or function.
			if(vnilmm(np->en_rp) == SUCCESS) {	// Set default return value.
				bool fevalcall = false;
				if(cfab.p_type == PTRMACRO)
					(void) dobuf(np->en_rp,(int) np->en_narg,cfab.u.p_bufp,NULL,
					 needrparen ? SRUN_PARENS : 0);
				else {
					CmdFunc *cfp = cfab.u.p_cfp;
#if MMDEBUG & (DEBUG_VALUE | DEBUG_EXPR)
					char fname[32],wk[80];
					strcpy(fname,np->en_rp->v_strp);
#endif
					if(!(opflags & OPEVAL) || allowedit(cfp->cf_flags & CFEDIT) == SUCCESS) {
						(void) (cfp->cf_func == NULL ? (feval(np->en_rp,(int) np->en_narg,cfp),
						 fevalcall = true) : cfp->cf_func(np->en_rp,(int) np->en_narg));
#if MMDEBUG & (DEBUG_VALUE | DEBUG_EXPR)
#if VDebug
						sprintf(wk,"fcall(): %s() returned [%d]...",fname,rc.status);
						vdump(np->en_rp,wk);
#else
						fprintf(logfile,"fcall(): %s() returned ... '%s' [%d]\n",fname,
						 np->en_rp->v_type == VALINT ? "(int)" : np->en_rp->v_strp,rc.status);
#endif
#endif
						}
					}
				if(rc.status == SUCCESS && (opflags & OPEVAL) && !fevalcall)
					(void) rcsave();
				}
			if(rc.status != SUCCESS)
				return rc.status;
			updtfn(np);
			}
		else {
			// Not evaluating or repeat count is zero ... consume arguments.
			np->en_flags &= EN_CONCAT;
			if(maxArgs > 0 && ((!havesym(s_rparen,false) && havesym(s_any,false)) ||
			 ((opflags & OPEVAL) && minArgs > 0))) {
				bool first = true;
				short argct = 0;
				for(;;) {
#if MMDEBUG & DEBUG_EXPR
fprintf(logfile,"### fcall(): consuming (minArgs %hd, maxArgs %hd): examing token '%s'...\n",minArgs,maxArgs,
 last->p_tok.v_strp);
#endif
					if(first)
						first = false;
					else if(!getcomma(false))
						break;		// Error or no arguments left.
#if MMDEBUG & DEBUG_EXPR
					if(ge_debug1("ge_andor",np,NULL,ge_andor) != SUCCESS)
#else
					if(ge_andor(np) != SUCCESS)	// Get next expression.
#endif
						break;
					++argct;
					}
				if(rc.status != SUCCESS)
					return rc.status;
				if((minArgs >= 0 && argct < minArgs) || argct > maxArgs)
					goto wrong;
				}
			}

		// Check for extra argument.
		if(maxArgs > 0 && havesym(s_comma,false))
wrong:
			return rcset(FAILURE,0,text69,last->p_tok.v_strp);
				// "Wrong number of arguments (at token '%s')"
		}
	else {
#if MMDEBUG & (DEBUG_VALUE | DEBUG_EXPR)
		fputs("UNKNOWN.\n",logfile);
#endif
		// Unknown CFAM.
		if(foundp == NULL)
			return rcset(FAILURE,0,text244,np->en_rp->v_strp);
					// "No such command, alias, or macro '%s'"
		*foundp = false;
#if MMDEBUG & (DEBUG_VALUE | DEBUG_EXPR)
		fputs("not a FAB.\n",logfile);
#endif
		}
out:
	// Get right paren, if applicable.
	if(needrparen && havesym(s_rparen,true))
		getsym();

	return rc.status;
	}
	
// Evaluate postfix expression and return status.  "narg" is numeric argument for function call, if any.  Postfix expressions
// are any of:
//	primary
//	postfix++
//	postfix--
//	postfix(comma-expression)
//	postfix comma-expression
//	postfix[expression]
static int ge_postfix(ENode *np) {
	bool needrparen;
	uint oldparens = opflags & OPPARENS;

#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1("ge_primary",np,NULL,ge_primary) != SUCCESS)
#else
	if(ge_primary(np) != SUCCESS)
#endif
		return rc.status;

	// Examples:
	//	getKey()
	//	myVar => insert myVar,' times'
	//	myVar++ => myMac => forwChar
	//	3 => myMac myVar => gotoMark		 Will be evaluated as 3 => myMac(myVar => gotoMark())
	//	index('ba','a') => setMark
	for(;;) {
		// Get postfix operator, if any.
		needrparen = false;
		switch(last->p_sym) {
			case s_incr:
			case s_decr:
				if(np->en_flags & EN_HAVENARG)
					goto nofunc;
				if(!lvalue(np))
					return rc.status;

				// Perform ++ or -- operation.
				if((opflags & OPEVAL) && bumpvar(np,last->p_sym == s_incr,false) != SUCCESS)
					return rc.status;
				if(getsym() < NOTFOUND)
					return rc.status;
				np->en_flags &= EN_CONCAT;
				break;
			case s_lparen:
				// A function call.  Error if primary was not an identifier or was an lvalue (variable).
				if(!(np->en_flags & EN_HAVEIDENT))
					return rcset(FAILURE,0,text4,text68,last->p_tok.v_strp);
							// "%s expected (at token '%s')","Identifier"
				if(np->en_flags & EN_HAVEGNVAR)
					return rcset(FAILURE,0,text244,np->en_rp->v_strp);
							// "No such command, alias, or macro '%s'"

				// Primary was an identifier and not a '$' variable.  Assume "function" type.  If white space
				// preceded the '(', the '(' is assumed to be the beginning of a primary expression and hence,
				// the first function argument "f (...),..."; otherwise, the "f(...,...)" form is assumed. 
				// Move past the '(' if no preceding white space and check tables.
				if(!(np->en_flags & EN_HAVEWHITE)) {
					if(getsym() < NOTFOUND)			// Past '('.
						return rc.status;
					needrparen = true;
					}

				// Call the function.
				if(fcall(np,needrparen,NULL) != SUCCESS)
					return rc.status;
				goto clear;
			default:
				// Was primary a non-variable identifier?
				if((np->en_flags & (EN_HAVEIDENT | EN_HAVEGNVAR)) == EN_HAVEIDENT) {
					bool found;
					if(fcall(np,false,&found) != SUCCESS)
						return rc.status;
					if(found) {
clear:
						// Clear flags obviated by a function call.
						np->en_flags &= (EN_HAVENIL | EN_HAVEBOOL | EN_CONCAT);
						break;
						}
					}

				// Not a function call.  Was last symbol a numeric prefix operator?
				if(np->en_flags & EN_HAVENARG)
nofunc:
					return rcset(FAILURE,0,text4,text67,np->en_rp->v_strp);
							// "%s expected (at token '%s')","Function call"

				// No postfix operators left.  Bail out.
				goto retn;
			}
		}
retn:
	opflags = (opflags & ~OPPARENS) | oldparens;
	return rc.status;
	}

// Evaluate unary expression and return status.  Unary expressions are any of:
//	postfix
//	!unary
//	~unary
//	++unary
//	--unary
//	-unary
//	+unary
static int ge_unary(ENode *np) {
	enum e_sym sym;

	switch(sym = last->p_sym) {
		case s_decr:
		case s_incr:
		case s_minus:
		case s_plus:
		case s_not:
		case s_bnot:
#if MMDEBUG & DEBUG_EXPR
			if(getsym() < NOTFOUND || ge_debug1("ge_unary",np,NULL,ge_unary) != SUCCESS)
#else
			if(getsym() < NOTFOUND || ge_unary(np) != SUCCESS)
#endif
				return rc.status;
			if(sym == s_incr || sym == s_decr) {
				if(!lvalue(np))
					return rc.status;

				// Perform ++ or -- operation.
				if(opflags & OPEVAL)
					(void) bumpvar(np,sym == s_incr,true);
				np->en_flags &= EN_CONCAT;
				}

			else {
				// Perform the operation.
#if MMDEBUG & DEBUG_EXPR
				if(ge_debug1("ge_deref",np,NULL,ge_deref) != SUCCESS)
#else
				if(ge_deref(np) != SUCCESS)
#endif
					return rc.status;
				if(opflags & OPEVAL) {
					if(sym != s_not && !intval(np->en_rp))
						return rc.status;
					if(sym == s_not) {
						(void) ltos(np->en_rp,tobool(np->en_rp) == false);
						np->en_flags |= EN_HAVEBOOL;
						}
					else if(sym != s_plus)
						vsetint(sym == s_minus ? -np->en_rp->u.v_int : ~np->en_rp->u.v_int,np->en_rp);
					}
				}
			break;
		default:
#if MMDEBUG & DEBUG_EXPR
			(void) ge_debug1("ge_postfix",np,NULL,ge_postfix);
#else
			(void) ge_postfix(np);
#endif
		}
	return rc.status;
	}

// Concatenate two nodes.
static int concat(ENode *np1,ENode *np2) {

	if(vistfn(np1->en_rp,VNIL))
		vnull(np1->en_rp);
	if(!vistfn(np2->en_rp,VNIL)) {
		StrList sl;

		if(vopen(&sl,np1->en_rp,true) != 0 || vputv(np2->en_rp,&sl) != 0 || vclose(&sl) != 0)
			(void) vrcset();
		}
	return rc.status;
	}

// Common routine to handle all of the legwork and error checking for all of the binary operators.
#if MMDEBUG & DEBUG_EXPR
static int ge_binop(ENode *np,char *fname,OpInfo *oip) {
#else
static int ge_binop(ENode *np,OpInfo *oip) {
#endif
	enum e_sym *symp;
	Value *op,*rp2;
	ENode node2;

	// Call function at next higher level.
#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1(fname,np,NULL,oip->xfunc) != SUCCESS)
#else
	if(oip->xfunc(np) != SUCCESS)
#endif
		return rc.status;

	if(vnew(&op,false) != 0 || vnew(&rp2,false) != 0)
		return vrcset();

	// Loop until no operator(s) at this level remain.
	for(;;) {
		symp = oip->symp;
		for(;;) {
			if(*symp == last->p_sym)
				break;
			if(*symp == s_any) {

				// No operators left.  Clear EN_CONCAT flag if concatenate op.
				if((oip->flags & FF_CONCAT) && (np->en_flags & EN_CONCAT))
					np->en_flags &= ~EN_CONCAT;
				goto retn;
				}
			++symp;
			}

		// Found valid operator.  Dereference.
#if MMDEBUG & DEBUG_EXPR
		if(ge_debug1("ge_deref",np,NULL,ge_deref) != SUCCESS)
#else
		if(ge_deref(np) != SUCCESS)
#endif
			goto retn;

		// If evaluating, have '&' op, and ("concatenation" and first operand is not string) or ("bitwise and" and
		// (EN_CONCAT flag is set or first operand is not integer)), ignore it (at wrong level).  OR if evaluating, have
		// '%' op, and ("format" and first operand is not string) or ("modulus" and first operand is not integer),
		// ignore it as well.
		if(opflags & OPEVAL) {
			if(*symp == s_band && (((oip->flags & FF_CONCAT) && np->en_rp->v_type == VALINT) ||
			 ((oip->flags & FF_BITOP) && ((np->en_flags & EN_CONCAT) || np->en_rp->v_type != VALINT))))
				goto retn;
			if(*symp == s_mod && (((oip->flags & FF_FORMAT) && np->en_rp->v_type == VALINT) ||
			 ((oip->flags & FF_MATH) && (np->en_rp->v_type != VALINT))))
				goto retn;
			}

		// We're good.  Save operator for error reporting.
		vxfer(op,&last->p_tok);

		// Set "force concatenation" flag in second node if applicable, and call function at next higher level.
		nodeinit(&node2,rp2);
		if(oip->flags & FF_CONCAT)
			node2.en_flags = EN_CONCAT;
#if MMDEBUG & DEBUG_EXPR
		if(getsym() < NOTFOUND || ge_debug1(fname,np,&node2,oip->xfunc) != SUCCESS)
#else
		if(getsym() < NOTFOUND || oip->xfunc(&node2) != SUCCESS)
#endif
			goto retn;

		// Dereference any lvalue.
#if MMDEBUG & DEBUG_EXPR
		if(ge_debug1("ge_deref",np,&node2,ge_deref) != SUCCESS)
#else
		if(ge_deref(&node2) != SUCCESS)
#endif
			goto retn;

		// If evaluating expressions, coerce binary operands and perform operation.
		if(opflags & OPEVAL) {
			if(forcefit(np,&node2,*symp == s_req || *symp == s_rne ? FF_REQNE : oip->flags,op->v_strp) != SUCCESS)
				goto retn;
			np->en_flags &= ~(EN_HAVENIL | EN_HAVEBOOL);
			switch(*symp) {

				// Bitwise.
				case s_band:
					// If FF_CONCAT flag is set, do concatenation; otherwise, bitwise and.
					if(!(oip->flags & FF_CONCAT))
						vsetint(np->en_rp->u.v_int & node2.en_rp->u.v_int,np->en_rp);
					else if(concat(np,&node2) != SUCCESS)
						return rc.status;
					break;
				case s_bor:
					vsetint(np->en_rp->u.v_int | node2.en_rp->u.v_int,np->en_rp);
					break;
				case s_bxor:
					vsetint(np->en_rp->u.v_int ^ node2.en_rp->u.v_int,np->en_rp);
					break;
				case s_lsh:
					vsetint((ulong) np->en_rp->u.v_int << (ulong) node2.en_rp->u.v_int,np->en_rp);
					break;
				case s_rsh:
					vsetint((ulong) np->en_rp->u.v_int >> (ulong) node2.en_rp->u.v_int,np->en_rp);
					break;

				// Multiplicative and additive.
				case s_div:
					if(node2.en_rp->u.v_int == 0)
						goto divzero;
					vsetint(np->en_rp->u.v_int / node2.en_rp->u.v_int,np->en_rp);
					break;
				case s_mod:
					// If FF_FORMAT flag is set, do string formatting; otherwise, modulus.
					if(oip->flags & FF_FORMAT) {
						Value *tp;

						if(vnew(&tp,false) != 0)
							return vrcset();
						vxfer(tp,np->en_rp);
						if(strfmt(np->en_rp,INT_MIN,tp,node2.en_rp) != SUCCESS)
							return rc.status;
						}
					else {
						if(node2.en_rp->u.v_int == 0)
divzero:
						return rcset(FAILURE,0,text245,np->en_rp->u.v_int);
								// "Division by zero is undefined (%ld/0)"
						vsetint(np->en_rp->u.v_int % node2.en_rp->u.v_int,np->en_rp);
						}
					break;
				case s_mul:
					vsetint(np->en_rp->u.v_int * node2.en_rp->u.v_int,np->en_rp);
					break;
				case s_plus:
					vsetint(np->en_rp->u.v_int + node2.en_rp->u.v_int,np->en_rp);
					break;
				case s_minus:
					vsetint(np->en_rp->u.v_int - node2.en_rp->u.v_int,np->en_rp);
					break;
				case s_eq:
				case s_ne:
				case s_ge:
				case s_gt:
				case s_le:
				case s_lt:
					{long lval1,lval2;

					if(np->en_rp->v_type == VALINT) {	// Both operands are integer.
						lval1 = np->en_rp->u.v_int;
						lval2 = node2.en_rp->u.v_int;
						}
					else {					// Both operands are string.
						lval1 = strcmp(np->en_rp->v_strp,node2.en_rp->v_strp);
						lval2 = 0L;
						}
					(void) ltos(np->en_rp,*symp == s_lt ? lval1 < lval2 : *symp == s_le ? lval1 <= lval2 :
					 *symp == s_eq ? lval1 == lval2 : *symp == s_gt ? lval1 > lval2 : *symp == s_ge ?
					 lval1 >= lval2 : lval1 != lval2);
					np->en_flags |= EN_HAVEBOOL;
					}
					break;

				// RE equality: s_req, s_rne
				default:
					if(visnull(node2.en_rp))
						return rcset(FAILURE,0,text187,text266);
							// "%s cannot be null","Regular expression"

					// Compile the RE pattern.
					if(newspat(node2.en_rp->v_strp,&rematch,NULL) == SUCCESS) {
						if(rematch.flags & SOPT_PLAIN)
							return rcset(FAILURE,0,text36,OPTCH_PLAIN,op->v_strp);
								// "Invalid pattern option '%c' for %s operator"
						grpclear(&rematch);
						if(mccompile(&rematch) == SUCCESS) {
							int offset;

							// Perform operation.
							if(recmp(np->en_rp,0,&rematch,&offset) == SUCCESS &&
							 vsetstr((offset >= 0) == (*symp == s_req) ? val_true : val_false,
							 np->en_rp) != 0)
								return vrcset();
							}
						}
				}
			}
		}
retn:
	return rc.status;
	}

// Process multiplication, division and modulus operators.
static int ge_mult(ENode *np) {
	static enum e_sym sy_mult[] = {s_mul,s_div,s_mod,s_any};
	static OpInfo oi_mult = {ge_unary,sy_mult,FF_MATH};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_unary",&oi_mult);
#else
	return ge_binop(np,&oi_mult);
#endif
	}

// Process addition and subtraction operators.
static int ge_add(ENode *np) {
	static enum e_sym sy_add[] = {s_plus,s_minus,s_any};
	static OpInfo oi_add = {ge_mult,sy_add,FF_MATH};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_mult",&oi_add);
#else
	return ge_binop(np,&oi_add);
#endif
	}

// Process numeric prefix (n) operator =>.
static int ge_numpref(ENode *np) {

	// Call function at next higher level.
#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1("ge_add",np,NULL,ge_add) != SUCCESS)
#else
	if(ge_add(np) != SUCCESS)
#endif
		return rc.status;

	// Loop until no operator at this level remains.
	while(last->p_sym == s_narg) {

		// Last expression was an n argument.  Verify that it was an integer and save it in the node so that the next
		// expression (a function call) can grab it.
#if MMDEBUG & DEBUG_EXPR
		if(ge_debug1("ge_deref",np,NULL,ge_deref) != SUCCESS)
#else
		if(ge_deref(np) != SUCCESS)
#endif
			return rc.status;
		if(opflags & OPEVAL) {
			if(!intval(np->en_rp))
				return rc.status;
			np->en_narg = np->en_rp->u.v_int;
			}
		np->en_flags |= EN_HAVENARG;

		// The next expression must be a function call (which is verified by ge_postfix()).
#if MMDEBUG & DEBUG_EXPR
		if(getsym() < NOTFOUND || ge_debug1("ge_postfix",np,NULL,ge_postfix) != SUCCESS)
#else
		if(getsym() < NOTFOUND || ge_postfix(np) != SUCCESS)
#endif
			return rc.status;
		}

	return rc.status;
	}

// Process shift operators << and >>.
static int ge_shift(ENode *np) {
	static enum e_sym sy_shift[] = {s_lsh,s_rsh,s_any};
	static OpInfo oi_shift = {ge_numpref,sy_shift,FF_SHFT};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_numpref",&oi_shift);
#else
	return ge_binop(np,&oi_shift);
#endif
	}

// Process bitwise and operator &.
static int ge_bitand(ENode *np) {
	static enum e_sym sy_bitand[] = {s_band,s_any};
	static OpInfo oi_bitand = {ge_shift,sy_bitand,FF_BITOP};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_shift",&oi_bitand);
#else
	return ge_binop(np,&oi_bitand);
#endif
	}

// Process bitwise or and xor operators | and ^.
static int ge_bitor(ENode *np) {
	static enum e_sym sy_bitor[] = {s_bor,s_bxor,s_any};
	static OpInfo oi_bitor = {ge_bitand,sy_bitor,FF_BITOP};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_bitand",&oi_bitor);
#else
	return ge_binop(np,&oi_bitor);
#endif
	}

// Process string format operator %.
static int ge_format(ENode *np) {
	static enum e_sym sy_format[] = {s_mod,s_any};
	static OpInfo oi_format = {ge_bitor,sy_format,FF_FORMAT};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_bitor",&oi_format);
#else
	return ge_binop(np,&oi_format);
#endif
	}

// Process concatenation operator &.
static int ge_concat(ENode *np) {
	static enum e_sym sy_concat[] = {s_band,s_any};
	static OpInfo oi_concat = {ge_format,sy_concat,FF_CONCAT};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_format",&oi_concat);
#else
	return ge_binop(np,&oi_concat);
#endif
	}

// Process relational operators <, <=, > and >=.
static int ge_rel(ENode *np) {
	static enum e_sym sy_rel[] = {s_lt,s_gt,s_le,s_ge,s_any};
	static OpInfo oi_rel = {ge_concat,sy_rel,FF_REL};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_concat",&oi_rel);
#else
	return ge_binop(np,&oi_rel);
#endif
	}

// Process equality and inequality operators ==, !=, =~, and !~.
static int ge_eqne(ENode *np) {
	static enum e_sym sy_eqne[] = {s_eq,s_ne,s_req,s_rne,s_any};
	static OpInfo oi_eqne = {ge_rel,sy_eqne,FF_EQNE};

#if MMDEBUG & DEBUG_EXPR
	return ge_binop(np,"ge_rel",&oi_eqne);
#else
	return ge_binop(np,&oi_eqne);
#endif
	}

// Do logical and/or.
#if MMDEBUG & DEBUG_EXPR
static int ge_landor(ENode *np,char *fname,int (*fncp)(ENode *np)) {
#else
static int ge_landor(ENode *np,int (*fncp)(ENode *np)) {
#endif
	bool b;
	Value *rp2;
	ENode node2;

	// Call function at next higher level.
#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1(fname,np,NULL,fncp) != SUCCESS)
#else
	if(fncp(np) != SUCCESS)
#endif
		return rc.status;

	if(vnew(&rp2,false) != 0)
		return vrcset();

	// Loop until no operator(s) at this level remain.
	for(;;) {
		switch(last->p_sym) {
			case s_and:
				if(fncp != ge_eqne)
					goto retn;
				b = false;
				goto andor;
			case s_or:
				if(fncp == ge_eqne)
					goto retn;
				b = true;
andor:
				if(getsym() < NOTFOUND)				// Past '&&' or '||'.
					goto retn;
				nodeinit(&node2,rp2);
#if MMDEBUG & DEBUG_EXPR
				if(ge_debug1("ge_deref",np,NULL,ge_deref) != SUCCESS)
#else
				if(ge_deref(np) != SUCCESS)			// Dereference var if needed.
#endif
					goto retn;
				if(!(opflags & OPEVAL)) {			// Eating arguments?
#if MMDEBUG & DEBUG_EXPR
					if(ge_debug1(fname,&node2,NULL,fncp) != SUCCESS)
#else
					if(fncp(&node2) != SUCCESS)		// Yes, bon appetit.
#endif
						goto retn;
					}
				else if(tobool(np->en_rp) == b) {		// No, does first argument determine outcome?
					if(ltos(np->en_rp,b) != SUCCESS)	// Yes, convert to logical...
						goto retn;
					np->en_flags |= EN_HAVEBOOL;
					opflags &= ~OPEVAL;			// and eat second argument.
#if MMDEBUG & DEBUG_EXPR
					(void) ge_debug1(fname,np,&node2,fncp);
#else
					(void) fncp(&node2);
#endif
					opflags |= OPEVAL;
					if(rc.status != SUCCESS)
						goto retn;
					}
				else {
#if MMDEBUG & DEBUG_EXPR
					if(ge_debug1(fname,np,&node2,fncp) != SUCCESS ||
					 ge_debug1("ge_deref",&node2,NULL,ge_deref) != SUCCESS ||
#else
					if(fncp(&node2) != SUCCESS ||		// No, evaluate second argument...
					 ge_deref(&node2) != SUCCESS ||		// dereference var if needed...
#endif
					 ltos(np->en_rp,tobool(node2.en_rp)) != SUCCESS) // and convert to logical.
						goto retn;
					np->en_flags |= EN_HAVEBOOL;
					}
				break;
			default:
				goto retn;
			}
		}
retn:
	return rc.status;
	}

// Logical and operator &&.
static int ge_and(ENode *np) {

#if MMDEBUG & DEBUG_EXPR
	return ge_landor(np,"ge_eqne",ge_eqne);
#else
	return ge_landor(np,ge_eqne);
#endif
	}

// Logical or operator ||.
static int ge_or(ENode *np) {

#if MMDEBUG & DEBUG_EXPR
	return ge_landor(np,"ge_and",ge_and);
#else
	return ge_landor(np,ge_and);
#endif
	}

// Process conditional (hook) operator ? :.
static int ge_cond(ENode *np) {

	// Call function at next higher level.
#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1("ge_or",np,NULL,ge_or) != SUCCESS)
#else
	if(ge_or(np) != SUCCESS)
#endif
		return rc.status;

	if(last->p_sym == s_hook) {
		ENode node2;
		Value *rp2;
		bool loop2 = false;
		bool eat = true;

		// Dereference any lvalue.
#if MMDEBUG & DEBUG_EXPR
		if(ge_debug1("ge_deref",np,NULL,ge_deref) != SUCCESS)
#else
		if(ge_deref(np) != SUCCESS)
#endif
			return rc.status;
		if(opflags & OPEVAL) {
			eat = !tobool(np->en_rp);
			if(vnew(&rp2,false) != 0)
				return vrcset();
			}

		// Loop twice.
		for(;;) {
			if(getsym() < NOTFOUND)		// Past '?' or ':'.
				return rc.status;

			// Don't evaluate one of the arguments if "evaluate mode" was true when we started.
			if(opflags & OPEVAL) {
				if(eat) {
					nodeinit(&node2,rp2);
					opflags &= ~OPEVAL;
#if MMDEBUG & DEBUG_EXPR
					(void) ge_debug1("ge_cond",&node2,NULL,ge_cond);
#else
					(void) ge_cond(&node2);
#endif
					opflags |= OPEVAL;
					if(rc.status != SUCCESS)
						return rc.status;
					eat = false;
					goto nextsym;
					}
				else
					eat = true;
				}
			nodeinit(np,np->en_rp);
#if MMDEBUG & DEBUG_EXPR
			if(ge_debug1("ge_cond",np,NULL,ge_cond) != SUCCESS ||
			 ge_debug1("ge_deref",np,NULL,ge_deref) != SUCCESS)
#else
			if(ge_cond(np) != SUCCESS || ge_deref(np) != SUCCESS)
#endif
				return rc.status;
nextsym:
			if(loop2)
				break;
			if(!havesym(s_any,false) || last->p_sym != s_colon)
				return rcset(FAILURE,0,text291,":",last->p_tok.v_strp);
					// "'%s' expected (at token '%s')"
			loop2 = true;
			}
		}

	return rc.status;
	}

// Process assignment operators =, +=, -=, *=, /=, %=, <<=, >>=, &=, ^=, |=.
static int ge_assign(ENode *np) {
	VDesc vd;
	Value *rp2,*op;
	ENode node2;
	uint f;
	enum e_sym sym;

	// Call function at next higher level.
#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1("ge_cond",np,NULL,ge_cond) != SUCCESS)
#else
	if(ge_cond(np) != SUCCESS)
#endif
		return rc.status;

	// Assignment?
	if(last->p_sym < s_assign || last->p_sym > s_asbor) {

		// No, dereference any identifier (if evaluating) and return.
		if((opflags & OPEVAL) && (np->en_flags & EN_HAVEIDENT)) {
#if MMDEBUG & DEBUG_EXPR
			fprintf(logfile,"### ge_assign(): Dereferencing '%s'...\n",np->en_rp->v_strp);
#endif
			if(derefn(np->en_rp,np->en_rp->v_strp) == SUCCESS)
				updtfn(np);
			}
		np->en_flags &= ~(EN_HAVEIDENT | EN_HAVEGNVAR | EN_HAVEWHITE);
		return rc.status;
		}

	// Have assignment operator.  Check if former node was an lvalue.
	if(!(np->en_flags & EN_HAVEIDENT))
		goto notvar;

	// Look up variable if evaluating.
	if(opflags & OPEVAL) {
		if(!cfabsearch(np->en_rp->v_strp,NULL,PTRCFAM))
notvar:
			return rcset(FAILURE,0,text4,text82,last->p_tok.v_strp);
				// "%s expected (at token '%s')","Variable name"
		if(findvar(np->en_rp->v_strp,last->p_sym == s_assign ? OPCREATE : OPDELETE,&vd) != SUCCESS)
			return rc.status;
		}
	np->en_flags &= ~(EN_HAVEIDENT | EN_HAVEGNVAR | EN_HAVEWHITE);

	// Set coercion flags.
	switch(sym = last->p_sym) {
		case s_assign:
			f = FF_ASSIGN;
			break;
		case s_asadd:
		case s_assub:
		case s_asmul:
		case s_asdiv:
		case s_asmod:
			f = FF_MATH;
			break;
		case s_aslsh:
		case s_asrsh:
			f = FF_SHFT;
			break;
		case s_asband:
			if((opflags & OPEVAL) && !intvar(&vd)) {
				f = FF_CONCAT;
				break;
				}
			// else fall through.
		default:
			f = FF_BITOP;
		}

	// If evaluating, save assign op (for error reporting).
	if(opflags & OPEVAL) {
		if(vnew(&op,false) != 0)
			return vrcset();
		vxfer(op,&last->p_tok);
		}

	// Move past operator and get value expression.
	if(getsym() < NOTFOUND)
		return rc.status;
	if(vnew(&rp2,false) != 0)
		return vrcset();
	nodeinit(&node2,rp2);
#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1("ge_assign",np,&node2,ge_assign) != SUCCESS || ge_debug1("ge_deref",np,&node2,ge_deref) != SUCCESS)
#else
	if(ge_assign(&node2) != SUCCESS || ge_deref(&node2) != SUCCESS)
#endif
		return rc.status;

	// If evaluating, get current variable value into np.
	if(opflags & OPEVAL) {
		if(derefv(np->en_rp,&vd) != SUCCESS)
			return rc.status;

		// Coerce operands into compatible types.
		if(forcefit(np,&node2,f,op->v_strp) != SUCCESS)
			return rc.status;

		// Do operation and put result into np.  np contains left side and node2 contains right.
		switch(sym) {
			case s_assign:
#if VDebug
	vdump(node2.en_rp,"ge_assign(): Transferring from src object...");
	vdump(np->en_rp,"ge_assign(): to dest object...");
#endif
				vxfer(np->en_rp,node2.en_rp);
				np->en_flags |= node2.en_flags & (EN_HAVENIL | EN_HAVEBOOL);
#if VDebug
	vdump(np->en_rp,"ge_assign(): Result (dest)...");
#endif
				break;
			case s_asadd:
				np->en_rp->u.v_int += node2.en_rp->u.v_int;
				break;
			case s_assub:
				np->en_rp->u.v_int -= node2.en_rp->u.v_int;
				break;
			case s_asmul:
				np->en_rp->u.v_int *= node2.en_rp->u.v_int;
				break;
			case s_asdiv:
				if(node2.en_rp->u.v_int == 0L)
					goto divzero;
				np->en_rp->u.v_int /= node2.en_rp->u.v_int;
				break;
			case s_asmod:
				if(node2.en_rp->u.v_int == 0L)
divzero:
					return rcset(FAILURE,0,text245,np->en_rp->u.v_int);
						// "Division by zero is undefined (%ld/0)"
				np->en_rp->u.v_int %= node2.en_rp->u.v_int;
				break;
			case s_aslsh:
				np->en_rp->u.v_int <<= node2.en_rp->u.v_int;
				break;
			case s_asrsh:
				np->en_rp->u.v_int >>= (ulong) node2.en_rp->u.v_int;
				break;
			case s_asband:
				if(f & FF_BITOP)
					np->en_rp->u.v_int &= node2.en_rp->u.v_int;
				else if(concat(np,&node2) != SUCCESS)
					return rc.status;
				break;
			case s_asbxor:
				np->en_rp->u.v_int ^= node2.en_rp->u.v_int;
				break;
			default:	// s_asbor
				np->en_rp->u.v_int |= node2.en_rp->u.v_int;
			}
		(void) putvar(np->en_rp,&vd);
		}

	return rc.status;
	}

// Evaluate low precedence logical not expression "not".
static int ge_not(ENode *np) {

	if(last->p_sym == kw_not) {
#if MMDEBUG & DEBUG_EXPR
		if(getsym() < NOTFOUND || ge_debug1("ge_not",np,NULL,ge_not) != SUCCESS)
#else
		if(getsym() < NOTFOUND || ge_not(np) != SUCCESS)
#endif
			return rc.status;

		// Perform operation.
		if(opflags & OPEVAL) {
			(void) ltos(np->en_rp,!tobool(np->en_rp));
			np->en_flags |= EN_HAVEBOOL;
			}
		}
	else
#if MMDEBUG & DEBUG_EXPR
		(void) ge_debug1("ge_assign",np,NULL,ge_assign);
#else
		(void) ge_assign(np);
#endif

	return rc.status;
	}

// Evaluate low precedence logical and/or expressions "and", "or".
static int ge_andor(ENode *np) {
	bool eval,priorTruth,curTruth;
	Value *rp2;
	ENode node2;

	// Call function at next higher level.
#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1("ge_not",np,NULL,ge_not) != SUCCESS)
#else
	if(ge_not(np) != SUCCESS)
#endif
		return rc.status;

	if(vnew(&rp2,false) != 0)
		return vrcset();
	eval = (opflags & OPEVAL) != 0;

	// Loop until no operator(s) at this level remain.  If we weren't evaluating initially (eval is false), then all ops
	// evaluate to false.
	for(;;) {
		priorTruth = tobool(np->en_rp);
		switch(last->p_sym) {			// true or false or true
			case kw_and:			// false and true and false
				curTruth = false;	// true or false and EVAL
				goto andor;		// false and true or EVAL
			case kw_or:
				curTruth = true;
andor:
				if(getsym() < NOTFOUND)				// Past 'and' or 'or'.
					goto retn;
				nodeinit(&node2,rp2);
				if(!(opflags & OPEVAL)) {			// Eating arguments?
					if(eval && curTruth != priorTruth) {	// Yes, stop the gluttony?
						opflags |= OPEVAL;		// Yes, enough already.
						goto eval;
						}
#if MMDEBUG & DEBUG_EXPR
					if(ge_debug1("ge_not",np,&node2,ge_not) != SUCCESS)
#else
					if(ge_not(&node2) != SUCCESS)		// No, bon appetit.
#endif
						goto retn;
					}
				else if(priorTruth == curTruth) {		// No, does prior argument determine outcome?
					if(ltos(np->en_rp,curTruth) != SUCCESS)	// Yes, convert to logical...
						goto retn;
					np->en_flags |= EN_HAVEBOOL;
					opflags &= ~OPEVAL;			// and eat next argument.
#if MMDEBUG & DEBUG_EXPR
					(void) ge_debug1("ge_not",np,&node2,ge_not);
#else
					(void) ge_not(&node2);
#endif
					opflags |= OPEVAL;
					if(rc.status != SUCCESS)
						goto retn;
					}
				else {						// No, evaluate next argument.
eval:
#if MMDEBUG & DEBUG_EXPR
					if(ge_debug1("ge_not",np,&node2,ge_not) != SUCCESS ||
#else
					if(ge_not(&node2) != SUCCESS ||
#endif
					 ltos(np->en_rp,tobool(node2.en_rp)) != SUCCESS)
						goto retn;
					np->en_flags |= EN_HAVEBOOL;
					}
				break;
			default:
				goto retn;
			}
		}
retn:
	return rc.status;
	}

// Get a comma (,) expression.
int ge_comma(ENode *np) {

	// Loop until no commas at this level remain.
	for(;;) {
		// Call function at next higher level.
#if MMDEBUG & DEBUG_EXPR
		if(ge_debug1("ge_andor",np,NULL,ge_andor) != SUCCESS)
#else
		if(ge_andor(np) != SUCCESS)
#endif
			return rc.status;
		if(!getcomma(false))
			break;

		// Reset node.
		nodeinit(np,np->en_rp);
		}
	return rc.status;
	}

// Get a macro line argument, given pointer to result and argument flags.  Return an error if argument does not conform to
// ARG_NOTNULL, ARG_INT, or ARG_STR flags.
int funcarg(Value *rp,uint aflags) {
	ENode node;

#if MMDEBUG & DEBUG_EXPR
	fprintf(logfile,"funcarg(...,%.8x): BEGIN at token '%s', remainder '%s'...\n",aflags,last->p_tok.v_strp,last->p_clp);
	fflush(logfile);
#endif
	if(!(aflags & ARG_FIRST) && !getcomma(true))
		return rc.status;
	nodeinit(&node,rp);
#if MMDEBUG & DEBUG_EXPR
	if(ge_debug1("ge_andor",&node,NULL,ge_andor) != SUCCESS ||
#else
	if(ge_andor(&node) != SUCCESS ||
#endif
	 ((opflags & OPEVAL) && (((aflags & ARG_INT) && !intval(rp)) || ((aflags & ARG_STR) && !strval(rp)))))
		return rc.status;

	// If evaluating, a null string or non-printable character, and flag set, return an error.
	if(opflags & OPEVAL) {
		if((aflags & ARG_NOTNULL) && visnull(rp))
			return rcset(FAILURE,0,text187,text285);
				// "%s cannot be null","Call argument"
		if((aflags & ARG_PRINT) && (strlen(rp->v_strp) != 1 || *rp->v_strp < ' ' || *rp->v_strp > '~'))
			return rcset(FAILURE,0,"%s '%s'%s",text285,rp->v_strp,text345);
					//  "Call argument"," must be a printable character"
		}

	return rc.status;
	}
