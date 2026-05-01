// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// expr.c	Core expresion evaluation routines for MightEMacs.

#include "std.h"
#include <errno.h>
#include "exec.h"
#include "search.h"
#include "var.h"

/* MightEMacs operator precedence (highest to lowest):
	Precedence	Operator	Description						Associativity
	==============	==============	======================================================	=============
			++		Suffix increment					Left-to-right
			--		Suffix decrement
	1		()		Function call
			<whitespace>	Function call
			[]		Array subscripting and hash lookup
	-----------------------------------------------------------------------------------------------------
			++		Prefix increment					Right-to-left
			--		Prefix decrement
			+		Unary plus
	2		-		Unary minus
			!		Logical NOT
			~		Bitwise NOT (one's complement)
	-----------------------------------------------------------------------------------------------------
			*		Multiplication (if first operand is integer)		Left-to-right
	3		/		Division
			%		Modulo (if first operand is integer)
	-----------------------------------------------------------------------------------------------------
	4		+		Addition						Left-to-right
			-		Subtraction (if first operand is integer)
	-----------------------------------------------------------------------------------------------------
	5		=>		Numeric prefix (n) or hash key/value delimiter		Left-to-right
	-----------------------------------------------------------------------------------------------------
	6		<<		Bitwise left shift					Left-to-right
			>>		Bitwise right shift
	-----------------------------------------------------------------------------------------------------
	7		&		Bitwise AND (if first operand is integer)		Left-to-right
	-----------------------------------------------------------------------------------------------------
	8		|		Bitwise OR (inclusive or; if first operand is integer)	Left-to-right
			^		Bitwise XOR (exclusive or)
	-----------------------------------------------------------------------------------------------------
	9		%		String formatting (if first operand is string)		Left-to-right
	-----------------------------------------------------------------------------------------------------
	10		*		Intersection (if first operand is array)		Left-to-right
			-		Exclusion (if first operand is array)
	-----------------------------------------------------------------------------------------------------
	11		|		Union (if first operand is array)			Left-to-right
	-----------------------------------------------------------------------------------------------------
	12		&		Concatenation (if first operand is string)		Left-to-right
	-----------------------------------------------------------------------------------------------------
			<		Less than						Left-to-right
	13		<=		Less than or equal to
			>		Greater than
			>=		Greater than or equal to
	-----------------------------------------------------------------------------------------------------
			<=>		Comparison						Left-to-right
			==		Equal to
	14		!=		Not equal to
			=~		RE equal to
			!~		RE not equal to
	-----------------------------------------------------------------------------------------------------
	15		&&		Logical AND						Left-to-right
	-----------------------------------------------------------------------------------------------------
	16		||		Logical OR						Left-to-right
	-----------------------------------------------------------------------------------------------------
	17		?:		Ternary conditional					Right-to-left
	-----------------------------------------------------------------------------------------------------
			=		Direct assignment					Right-to-left
			+=		Assignment by sum
			-=		Assignment by difference (or exclusion if first operand is array)
			*=		Assignment by product (or intersection if first operand is array)
			/=		Assignment by quotient
	18		%=		Assignment by remainder
			<<=		Assignment by bitwise left shift
			>>=		Assignment by bitwise right shift
			&=		Assignment by bitwise AND (or concatenation if first operand is array)
			|=		Assignment by bitwise OR (or union if first operand is array)
			^=		Assignment by bitwise XOR
	-----------------------------------------------------------------------------------------------------
	19		not		Logical NOT (low precedence)				Right-to-left
	-----------------------------------------------------------------------------------------------------
	20		or		Logical OR (low precedence)				Left-to-right
			and		Logical AND (low precedence)
	-----------------------------------------------------------------------------------------------------

Array and Hash Management Notes:

Arrays and hashes are managed differently than other data types in MightEMacs.  Non-array-hash types, including strings, are
always copied from place to place (by value) when an expression is evaluated.  For example, when a variable containing a string
is dereferenced, a copy of the string is placed into the expression node and the variable retains its own copy.  This is not the
case for arrays and hashess.  Except for a few special cases, arrays and hashess are always copied by reference instead of by
value.  So for example, when a variable containing an array or hash is dereferenced, a pointer to that array or hash is placed
into the expression node.  If the array or hash in the node is subsequently modified, the variable becomes modified as well.
This technique is used is avoid making multiple copies of an array or hash during expression evaluation, which would have a
significant performance and memory-consumption impact.

Passing pointers for arrays and hashes creates a problem, however.  It is extremely difficult to determine when an array or hash
(which is allocated from heap space, like all data types) is no longer being used during expression evaluation and can be freed.
To solve this problem, the following method is used:
 1. Any time an array or hash is created, it is saved in a Datum object and pushed onto a tracking list pointed to by
    "ahTrackHead".  This is done recursively so that any nested arrays and hashes are pushed onto the list also.
 2. Array and Hash objects contain a "user flags" member, "uflags", which is used to identify the object type, determine which
    arrays and hashes to keep and which to free (later), and also to prevent endless recursion when an array or hash contains
    itself, either directly or indirectly.
 3. Freeing of array and hash space is not attempted at all during expression evaluation, simply because there is no safe time
    at which to do so.  It is not done until just before control is returned to the user, in editLoop(), via a call to the
    ahfree() function.
 4. The job of ahfree() is to scan the ahTrackHead list, freeing arrays and hashes which are not associated with a global
    constant or variable.  To accomplish this, it first checks the list.  If it is empty, it does nothing; otherwise, it (a),
    scans the global constant/variable list and adds any arrays and hashes found to the tracking list via the ahtrack() function
    (which also adds nested arrays and hashes) so that all existing arrays and hashes will be examined; (b), scans the tracking
    list and sets the Tag bit of uflags to zero in each array and hash; (c), scans the global constant/variable list again and
    sets the Tag bit of uflags in any arrays and hashes and nested arrays and hashes found via the ahTag() function, which also
    uses uflags during recursion to prevent an endless loop if an array or hash contains itself; and (d), scans the tracking
    list again and frees any arrays or hashes which are not tagged.
 5. When ahfree() is done, the only arrays and hashes left in the list will be those contained in global constants and
    variables.  This will also include nested arrays and hashes.  "ahTrackHead" is set to NULL at this point so that the list
    will not be scanned every time the user presses a key.
 6. Lastly, to deal with the arrays and hashes contained in global constants and variables, the ahTag() function is called
    whenever a new value is assigned to one of those variables, or one of their elements or nodes that contains an array or
    hash.  It puts the old array or hash and any arrays or hashes it contains back onto the tracking list.

Given the fact that arrays and hashes are passed around by reference, it is the responsibility of the user to clone arrays and
hashes where needed (via the "aclone" and "hclone" functions).  However, there are a couple places where it is done
automatically:
	* If the initializer of the "array" function is itself an array, it is cloned for each element of the array created.
	* If the first item in a concatenation expression is an array or hash and is an lvalue (a variable, array element
	  reference, or hash node reference), it is cloned so that the value of the variable, array element reference, or hash
	  node reference is not changed.
*/

/*** Local declarations ***/

// Binary operator info.
typedef struct {
	int (*upFunc)(ExprNode *pNode);	// Function at next higher level.
	Symbol *pSym;			// Valid operator token(s).
	ushort flags;			// Kind of operation.
	} OpInfo;

// forceFit() types.
#define FF_Math		0x0001		// Add, sub, mul, div or mod.
#define FF_Shft		0x0002		// Left or right bit shift.
#define FF_BitOp	0x0004		// &, | or ^.
#define FF_Format	0x0008		// String format %.
#define FF_SetMatch	0x0010		// Array intersection or exclusion.
#define FF_Union	0x0020		// Array union.
#define FF_Concat	0x0040		// Concatenation.
#define FF_Rel		0x0080		// <, <=, > or >=.
#define FF_REQNE	0x0100		// =~ or !~.
#define FF_EQNE		0x0200		// == or !=.
#define FF_LogAndOr	0x0400		// && or ||.
#define FF_Cond		0x0800		// Conditional (hook).
#define FF_Assign	0x1000		// Straight assignment (=).

// forceFit() table for validating data types of a binary operation and optionally, coercing right operand to string.  Data
// types are: nil, Boolean (true or false), int, string, array, and hash.
static struct ForceFit {
	ushort legal;			// Legal operations (FF_XXX flags)
	ushort strOp;			// Operations which cause call to toStr() on right node.
	} forceFitTable[][6] = {
		{ /* nil */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* int */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* hash */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0}},

		{ /* bool */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* int */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* hash */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0}},

		{ /* int */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* int */	{~(FF_Concat | FF_Format | FF_REQNE),					0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* hash */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0}},

		{ /* string */
/* nil */	{FF_Assign | FF_Concat | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond,	FF_Concat},
/* bool */	{FF_Assign | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond,		0},
/* int */	{FF_Assign | FF_Concat | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond,	FF_Concat},
/* string */	{FF_Assign | FF_Concat | FF_Format | FF_Rel | FF_REQNE | FF_EQNE | FF_LogAndOr | FF_Cond,	0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* hash */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0}},

		{ /* array */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* int */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* array */	{FF_Assign | FF_SetMatch | FF_Union | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond,	0},
/* hash */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0}},

		{ /* hash */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* int */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,				0},
/* hash */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond,		0}}
		};

#if MMDebug & Debug_Expr
static int indent = -1;
#endif

// Pop datTrackHead to given pointer, releasing allocated memory for Datum objects, preserving any arrays and hashes.
void dtpop(Datum *pDatum) {
	Datum *pDatum1;

#if MMDebug & Debug_Datum
	fprintf(logfile, "dtpop(%.8x): datTrackHead %.8x...\n", (uint) pDatum, (uint) datTrackHead);
#endif
	while(datTrackHead != pDatum) {
		pDatum1 = datTrackHead;
		datTrackHead = datTrackHead->next;

#if MMDebug & Debug_Datum
		fprintf(logfile, "  freeing datum %.8x, type %.4hx...\n", (uint) pDatum1, pDatum1->type);
#endif
		// Don't free actual arrays and hashes (which are "ref" types in datum).
		dfree(pDatum1);
		}
#if MMDebug & Debug_Datum
	fputs("dtpop(): EXIT\n", logfile);
#endif
	}

// Initialize an expression node with given Datum object.
static void nodeClear(ExprNode *pNode) {

	dsetnil(pNode->pVal);
	pNode->flags = 0;
	pNode->nArg = DefN;
	pNode->i.pKey = NULL;
	}

// Initialize an expression node with given Datum object.
void nodeInit(ExprNode *pNode, Datum *pRtnVal, ushort flags) {

	pNode->pVal = pRtnVal;
	nodeClear(pNode);
	pNode->flags = flags;
	}

// Return true if b is true; otherwise, set given error message and return false.
static bool isTrue(bool b, const char *msg) {

	if(b)
		return true;
	(void) rsset(Failure, RSNoFormat, msg);
	return false;
	}

// Return true if a Datum object is a character (8-bit unsigned integer); otherwise, set an error and return false.
bool isCharVal(Datum *pDatum) {

	return isTrue(pDatum->type == dat_int && dgetint(pDatum) >= 0 && dgetint(pDatum) <= 0xff, text288);
			// "Character expected"
	}

// Return true if Datum object is an integer; otherwise, set an error and return false.
bool isIntVal(Datum *pDatum) {

	return isTrue(pDatum->type == dat_int, text166);
			// "Integer expected"
	}

// Return true if Datum object is a string; otherwise, set an error and return false.
bool isStrVal(Datum *pDatum) {

	return isTrue(dtypstr(pDatum), text171);
			// "String expected"
	}

// Return true if Datum object is an array; otherwise, set an error and return false.
bool isArrayVal(Datum *pDatum) {

	return isTrue(dtyparray(pDatum), text371);
			// "Array expected"
	}

// Return true if Datum object is a hash; otherwise, set an error and return false.
static bool isHashVal(Datum *pDatum) {

	return isTrue(dtyphash(pDatum), text509);
			// "Hash expected"
	}

// Return true if Datum object is an array, hash, or string; otherwise, set an error and return false.
bool isAryHshStrVal(Datum *pDatum) {

	return isTrue(pDatum->type & (DArrayMask | DHashMask | DStrMask), text510);
							// "Array, hash, or string expected"
	}

// Return true if node value is an lvalue; otherwise, set an error if "required" is true and return false.
static bool lvalue(ExprNode *pNode, bool required) {

	if(pNode->flags & EN_HaveLValue || (pNode->flags & EN_HaveIdent && findLocalVar(pNode->pVal->str) != NULL))
		return true;
	if(required) {
		if(pNode->flags & EN_HaveIdent)
			(void) rsset(Failure, 0, text52, text239, pNode->pVal->str);
					// "No such %s \"%s\"", "variable"
		else
			(void) rsset(Failure, 0, text4, text82, pLastParse->tok.str);
					// "%s expected (at token '%s')", "Variable name"
		}
	return false;
	}

#if MMDebug & (Debug_ArrayHash | Debug_AryHshBuf | Debug_AryHshLog | Debug_Expr | Debug_UserVars)
// Write a value to log file, given Datum object and newline flag.
void dumpVal(Datum *pDatum, bool newln) {
	char *str;
	Datum datum;
	long size;
	uint bufLen = term.cols * 2 / 3 - 28 + 1;
	char workBuf[bufLen];

	fprintf(logfile, " (%.8x typ %.4hx) ", (uint) pDatum, pDatum->type);
	switch(pDatum->type) {
		case dat_nil:
			str = viz_nil;
			break;
		case dat_false:
			str = viz_false;
			break;
		case dat_true:
			str = viz_true;
			break;
		case dat_int:
			fprintf(logfile, "%ld", dgetint(pDatum));
			goto Newline;
		case dat_miniStr:
		case dat_longStr:
			fprintf(logfile, "\"%s\"", pDatum->str);
			goto Newline;
		case dat_array:
		case dat_arrayRef:
			str = "array";
			size = dgetarrayptr(pDatum)->used;
			goto PrintAH;
		default:
			str = "hash";
			size = dgethashptr(pDatum)->nodeCount;
PrintAH:
			dinit(&datum);
			(void) dtos(&datum, pDatum, NULL, CvtLang);
			strfit(workBuf, bufLen, datum.str, 0);
			fprintf(logfile, "[%s %.8x] size %ld %s", str, (uint) dgetptr(pDatum), size, workBuf);
			dclear(&datum);
			goto Newline;
		}
	fputs(str, logfile);
Newline:
	if(newln)
		fputc('\n', logfile);
	}
#endif
#if MMDebug & Debug_Expr

// Write expression parsing info to log file.
static void ge_dump(const char *funcName, short nodeNum, ExprNode *pNode1, ExprNode *pNode2, bool begin) {
	int i = indent;
	while(i-- > 0)
		fputs("  ", logfile);
	fprintf(logfile, "%s %s(%hd): %s '%s' (%d), cur val ", begin ? "BEGIN" : "END", funcName, nodeNum, begin ? "parsing" :
	 "returning at", pLastParse->tok.str, pLastParse->sym);
	dumpVal(pNode1->pVal, false);
	fprintf(logfile, ", nflags %.4hx", pNode1->flags);
	if(pNode2 != NULL) {
		fputs(", val2 ", logfile);
		dumpVal(pNode2->pVal, false);
		fprintf(logfile, ", nflags %.4hx", pNode2->flags);
		}
	fprintf(logfile, ", opFlags %.4hx", sess.opFlags);
	if(begin)
		fputs("...\n", logfile);
	else
		fprintf(logfile, " [%d]\n", sess.rtn.status);
	}

// Intercept all calls to ge_xxx() functions and write status information to log file.
static int ge_debug1(const char *funcName, short nodeNum, ExprNode *pNode1, ExprNode *pNode2, int (*upFunc)(ExprNode *pNode)) {
	int status;

	++indent;
	ge_dump(funcName, nodeNum, pNode1, pNode2, true);
	status = upFunc(pNode2 != NULL ? pNode2 : pNode1);
	ge_dump(funcName, nodeNum, pNode1, pNode2, false);
	--indent;
	return status;
	}
#endif

// Dereference an lvalue (variable name, array element reference, or hash node reference) in pNode if present and evaluating.
// Return status.
static int ge_deref(ExprNode *pNode) {

	if(!(sess.opFlags & OpEval))

		// Not evaluating.  Just clear flags.
		pNode->flags &= ~(EN_TopLevel | EN_HaveLValue | EN_HaveIdent | EN_HaveWhite);
	else {
		// Evaluating.  Dereference and clear flags.
		if(pNode->flags & EN_HaveIdent) {
			if(vderefn(pNode->pVal, pNode->pVal->str) == Success)
				pNode->flags &= ~(EN_TopLevel | EN_HaveIdent | EN_GNConstVar | EN_HaveWhite);
			}
		else if(pNode->flags & (EN_AryElRef | EN_HshNodRef)) {
			VarDesc varDesc;
			int (*cvtRef)(VarDesc *pVarDesc, ExprNode *pNode, uint oflags) = pNode->flags & EN_AryElRef ?
			 cvtAryElRef : cvtHshNodRef;

			if(cvtRef(&varDesc, pNode, OpQuery) == Success && vderefv(pNode->pVal, &varDesc) == Success)
				pNode->flags &= ~(EN_TopLevel | EN_AryElRef | EN_HshNodRef);
			}
		}

	return sess.rtn.status;
	}

// Coerce Datum objects passed into compatable types for given operation flag(s) and return status.  If illegal fit, return
// error.  "op" contains operation flag (FF_XXX) and "tok" is operator token (for error reporting).
static int forceFit(ExprNode *pNode1, ExprNode *pNode2, ushort op, const char *tok) {
	struct ForceFit *pFitInfo;

	// Get info from table.
	pFitInfo = forceFitTable[disnil(pNode1->pVal) ? 0 : dtypbool(pNode1->pVal) ? 1 :
	 pNode1->pVal->type == dat_int ? 2 : dtypstr(pNode1->pVal) ? 3 : dtyparray(pNode1->pVal) ? 4 : 5] +
	 (disnil(pNode2->pVal) ? 0 : dtypbool(pNode2->pVal) ? 1 : pNode2->pVal->type == dat_int ? 2 :
	 dtypstr(pNode2->pVal) ? 3 : dtyparray(pNode2->pVal) ? 4 : 5);
#if MMDebug & Debug_Expr
	fprintf(logfile, "### forceFit(): checking operands, types %.4hx:%.4hx, legal %.4hx, op %.4hx...\n",
	 pNode1->pVal->type, pNode2->pVal->type, pFitInfo->legal, op);
#endif
	// Valid operand types?
	if(!(pFitInfo->legal & op))
#if MMDebug & Debug_Expr
		return rsset(Failure, 0, "Wrong operand type for '%s' (legal %.4hx, op %.4hx, node types %.4hx:%.4hx)",
		 tok, pFitInfo->legal, op, pNode1->pVal->type, pNode2->pVal->type);
#else
		return rsset(Failure, 0, text191, tok);
				// "Wrong type of operand for '%s'"
#endif
	// Coerce right operand to string if needed.
	if(pFitInfo->strOp & op) {
#if MMDebug & Debug_Expr
		fputs("### forceFit(): coercing right operand to string...\n", logfile);
#endif
		(void) toStr(pNode2->pVal);
		}
#if MMDebug & Debug_Expr
	fputs("### forceFit(): EXIT\n", logfile);
#endif
	return sess.rtn.status;
	}

// Parse a primary expression and save the value in pNode.  If an identifier is found, save its name and set appropriate flags
// in pNode as well.  Return status.  Primary expressions are any of:
//	[-]number
//	"?" char-lit
//	string
//	identifier
//	"$"[identifier | number]
//	"true" | "false" | "nil" | "defn"
//	[and-or-expression, ...]
//	{string-expression => and-or-expression, ...}
//	(and-or-expression)
static int ge_primary(ExprNode *pNode) {
	bool b;

	switch(pLastParse->sym) {
		case s_numLit:
			// Numeric literal.
			if(sess.opFlags & OpEval) {
				long i;
				char *str, workBuf[strlen(pLastParse->tok.str) + 2];

				if(pNode->flags & EN_HaveNegSign) {
					workBuf[0] = '-';
					strcpy(workBuf + 1, pLastParse->tok.str);
					str = workBuf;
					}
				else
					str = pLastParse->tok.str;
				if(ascToLong(str, 0, &i, false) != Success)
					break;
				dsetint(i, pNode->pVal);
				}
			goto NextSym;
		case s_charLit:
			// Character literal.
			if(sess.opFlags & OpEval) {
				short c;
				char *str = pLastParse->tok.str + 1;	// Past '?'.
				if((c = *str) == '\\' && evalCharLit(&str, &c, true) != Success)
					break;
				dsetint(c, pNode->pVal);
				}		
			goto NextSym;
		case s_strLit:
			// String literal.
			if(sess.opFlags & OpEval && evalStrLit(pNode->pVal, pLastParse->tok.str) != Success)
				break;
			goto NextSym;
		case kw_true:					// Convert to true value.
			b = true;
			goto IsBool;
		case kw_false:					// Convert to false value.
			b = false;
IsBool:
			if(sess.opFlags & OpEval)
				dsetbool(b, pNode->pVal);
			goto NextSym;
		case kw_nil:					// Convert to nil value.
			if(sess.opFlags & OpEval)
				dsetnil(pNode->pVal);
			goto NextSym;
		case kw_defn:					// Convert to defn value.
			if(sess.opFlags & OpEval)
				dsetint(defn, pNode->pVal);
			goto NextSym;
		case s_globalConst:
		case s_globalVar:
		case s_numVar:
			pNode->flags |= EN_GNConstVar;
			// Fall through.
		case s_ident:
		case s_identQuery:
			pNode->flags |= EN_HaveIdent;

			// Save identifier name in pNode.
			if(dsetstr(pLastParse->tok.str, pNode->pVal) != 0)
				goto LibFail;
			goto NextSym;
		case s_leftParen:
			// Parenthesized expression.
			{ushort oldFlags = pNode->flags;
			nodeClear(pNode);
			pNode->flags = EN_TopLevel;
#if MMDebug & Debug_Expr
			if(getSym() < NotFound || ge_debug1("ge_andOr", 0, pNode, NULL, ge_andOr) != Success ||
			 !haveSym(s_rightParen, true))
#else
			if(getSym() < NotFound || ge_andOr(pNode) != Success || !haveSym(s_rightParen, true))
#endif
				break;
			pNode->flags = oldFlags;
			}
			goto NextSym;
		case s_leftBracket:
			// Bracketed expression list.  Create array.
			{Array *pArray;
			Datum result;
			ushort oldFlags = pNode->flags;
			bool first = true;

			if(sess.opFlags & OpEval) {
				dinit(&result);
				if(makeArray(&result, 0, &pArray) != Success)
					break;
				}
			if(getSym() < NotFound)
				break;

			// Get element list, if any.
			while(!haveSym(s_rightBracket, false)) {
				if(!first && !needSym(s_comma, true))
					goto Retn;

				// Get next expression.
				nodeClear(pNode);
				if(ge_andOr(pNode) != Success)
					goto Retn;
				if(sess.opFlags & OpEval && apush(pArray, pNode->pVal, OpXfer) != 0)
					goto LibFail;
				first = false;
				}
			if(sess.opFlags & OpEval)
				dxfer(pNode->pVal, &result);
			pNode->flags = oldFlags;
			}
			goto NextSym;
		case s_leftBrace:
			// Braced expression list.  Create hash.
			{Hash *pHash;
			HashNode *pHashNode;
			Datum result;
			ushort oldFlags = pNode->flags;
			bool first = true;

			if(sess.opFlags & OpEval) {
				dinit(&result);
				if(makeHash(&result, 0, &pHash) != Success)
					goto Retn;
				}
			if(getSym() < NotFound)
				break;

			// Get key-value list, if any.
			while(!haveSym(s_rightBrace, false)) {
				if(!first && !needSym(s_comma, true))
					goto Retn;

				// Get hash key.
				nodeClear(pNode);
				pNode->flags = EN_HashExpr;
				if(ge_assign(pNode) != Success)
					goto Retn;
				if(sess.opFlags & OpEval) {
					if(!isStrVal(pNode->pVal))
						goto Retn;
					if((pHashNode = hnewnode(pHash, pNode->pVal->str, NULL)) == NULL)
						goto LibFail;
					}

				// Get '=>' and node value.
				nodeClear(pNode);
				if(!needSym(s_bind, true) || ge_andOr(pNode) != Success)
					goto Retn;
				if(sess.opFlags & OpEval)
					dxfer(pHashNode->pVal, pNode->pVal);

				first = false;
				}

			// Finished parsing hash initializer.  Adjust hash size if evaluating.
			if(sess.opFlags & OpEval) {
				dxfer(pNode->pVal, &result);
				if(pHash->nodeCount > 0)
					if(hconfig(pHash, CfgSize,
					 (HashSize)(pHash->nodeCount * (1 + HashGrowPct / 100.0))) != 0)
						goto LibFail;
				}
			pNode->flags = oldFlags;

			// Check for trailing "ignore case" flag: {...}:'i'
			if(nextChar(':')) {
				(void) getSym();		// Colon.
				(void) getSym();		// Past colon.
				if(haveSym(s_any, true)) {
					ExprNode node2;
					Datum *pValue2;

					if((pValue2 = dnewtrack()) == NULL)
						goto LibFail;
					nodeInit(&node2, pValue2, 0);
					if(ge_primary(&node2) == Success && sess.opFlags & OpEval && isStrVal(pValue2)) {
						if(strcmp(pValue2->str, "i") != 0)
							return rsset(Failure, 0, text511, pValue2->str);
									// "Unknown hash property flag(s) \"%s\""
						if(hconfig(pHash, CfgProp, PropIgnoreCase) != 0)
							goto LibFail;
						}
					}
				goto Retn;
				}
			}
NextSym:
			// Set "white-space-after-identifier" flag for caller.
			if(haveWhite())
				pNode->flags |= EN_HaveWhite;
			(void) getSym();
			break;
		default:
			if(pLastParse->sym == s_nil)
				(void) rsset(Failure, RSNoFormat, text172);
						// "Token expected"
			else
				(void) rsset(Failure, 0, text289, pLastParse->tok.str);
						// "Unexpected token '%s'"
		}
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Handle a command, function, or alias call.
static int callCFA(ExprNode *pNode, bool *found) {
	ExecPtr exec;
	int minArgs, maxArgs;

	// Is identifier a command, function, or alias?
#if MMDebug & (Debug_Datum | Debug_Expr)
	fprintf(logfile, "callCFA(): identifying '%s'... ", pNode->pVal->str);
#endif
	if(!execFind(pNode->pVal->str, OpQuery, PtrSysCmdFunc | PtrAlias | PtrUserCmdFunc, &exec)) {

		// Unknown CFA.
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("UNKNOWN.\n", logfile);
#endif
		if(found == NULL)
			return rsset(Failure, 0, text52, text118, pNode->pVal->str);
				// "No such %s \"%s\"", "command, function, or alias"
		*found = false;
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("not a FAB.\n", logfile);
#endif
		}
	else {
		// Yes.  Resolve any alias.
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("found '", logfile);
		switch(exec.type) {
			case PtrAliasSysCmd:
			case PtrAliasSysFunc:
			case PtrAliasUserCmd:
			case PtrAliasUserFunc:
				fprintf(logfile, "%s' ALIAS", exec.u.pAlias->name);
				break;
			case PtrUserCmd:
			case PtrUserFunc:
				fprintf(logfile, "%s' USER %s", exec.u.pBuf->bufname,
				 exec.type == PtrUserCmd ? "COMMAND" : "FUNCTION");
				break;
			case PtrSysCmd:
			case PtrSysFunc:
				fprintf(logfile, "%s' %s", exec.u.pCmdFunc->name,
				 exec.type == PtrSysCmd ? "COMMAND" : "FUNCTION");
			}
		fprintf(logfile, " (type %hu) for token '%s'\n", exec.type, pNode->pVal->str);
#endif
		if(exec.type & PtrAlias)
			exec = exec.u.pAlias->targ;

		// Check if interactive-only command.
		if(exec.type == PtrSysCmd) {
			if(exec.u.pCmdFunc->attrFlags & CFTerm)
				return rsset(Failure, RSTermAttr, text282, exec.u.pCmdFunc->name);
					// "Cannot invoke ~b%s~B command directly in a script (use ~brun~B)"

			// If "alias" command (which uses "alias xxx = yyy" syntax), parentheses not allowed.
			if(pNode->flags & EN_HaveLParen && exec.u.pCmdFunc->func == alias)
				return rsset(Failure, 0, text289, "(");
						// "Unexpected token '%s'"
			}
		if(found != NULL)
			*found = true;

		// Have command, function, user command, or user function at this point.  Determine minimum number of required
		// arguments.
		minArgs = exec.type & PtrUserCmdFunc ? exec.u.pBuf->pCallHdr->minArgs : exec.u.pCmdFunc->minArgs;

		// "xxx()" form?
		if(pNode->flags & EN_HaveLParen && haveSym(s_rightParen, false)) {

			// Yes.  Error if argument(s) required (whether or not evaluating).
			if(minArgs > 0)
				goto Wrong;

			// No arguments required.  If not user routine, not evaluating, and not special invocation, we're done.
			if(!(exec.type & PtrUserCmdFunc) && !(sess.opFlags & OpEval) &&
			 !(exec.u.pCmdFunc->attrFlags & CFSpecArgs))
				goto Retn;
			}

		// Not "xxx()" call, zero required arguments, or special-invocation routine.  Proceed with execution or argument
		// consumption.  Note that unusual expressions such as "false && x => seti(5)" where the "seti(5)" is likely an
		// error are not checked here because the numeric prefix is not known when not evaluating.  The specific command
		// or function routine will do the validation.
		if(exec.type & PtrUserCmdFunc)
			maxArgs = exec.u.pBuf->pCallHdr->maxArgs < 0 ? INT_MAX : exec.u.pBuf->pCallHdr->maxArgs;
		else
			maxArgs = exec.u.pCmdFunc->maxArgs < 0 ? INT_MAX : exec.u.pCmdFunc->maxArgs;
		// Save calling form in operation flags.
		sess.opFlags = (sess.opFlags & ~OpParens) | (pNode->flags & EN_HaveLParen ? OpParens : 0);

		// Call the execution object if it's a system command or function and CFSpecArgs is set, or evaluating and (1),
		// it's a user command or function; or (2), it's a system command or function and the n argument is not zero or
		// not a repeat count.
		if((exec.type & PtrSysCmdFunc && exec.u.pCmdFunc->attrFlags & CFSpecArgs) ||
		 (sess.opFlags & OpEval && (exec.type & PtrUserCmdFunc || pNode->nArg != 0 ||
		 !(exec.u.pCmdFunc->attrFlags & CFNCount)))) {
			bool sysCall = false;
#if MMDebug & (Debug_Datum | Debug_Expr)
			char funcName[32];
			strcpy(funcName, pNode->pVal->str);
#endif
			// Call command or function.
			dsetnil(pNode->pVal);			// Set default return value.
			if(exec.type & PtrUserCmdFunc) {
#if MMDebug & Debug_Expr
				fprintf(logfile, "callCFA(): calling user command or function %s(%ld)...\n",
				 funcName, pNode->nArg);
#endif
#if MMDebug & Debug_Script
				(void) debug_execBuf(pNode->pVal, pNode->nArg, exec.u.pBuf, NULL,
				 pNode->flags & EN_HaveLParen ? ArgFirst | SRun_Parens : ArgFirst, "fcall");
#else
				(void) execBuf(pNode->pVal, pNode->nArg, exec.u.pBuf, NULL,
				 pNode->flags & EN_HaveLParen ? ArgFirst | SRun_Parens : ArgFirst);
#endif
				}
			else {
				CmdFunc *pCmdFunc = exec.u.pCmdFunc;
				if(!(sess.opFlags & OpEval) || allowEdit(pCmdFunc->attrFlags & CFEdit) == Success) {
#if MMDebug & Debug_Expr
					fprintf(logfile, "callCFA(): calling cmd/func %s(%ld): minArgs %d, maxArgs %d...\n",
					 funcName, pNode->nArg, minArgs, maxArgs);
#endif
					execCmdFunc(pNode->pVal, pNode->nArg, pCmdFunc, minArgs, maxArgs);
					sysCall = true;
#if MMDebug & (Debug_Datum | Debug_Expr)
					fprintf(logfile, "callCFA(): %s() returned... '%s' [%d]\n", funcName,
					 dtype(pNode->pVal, true), sess.rtn.status);
#endif
					}
				}
			if(sess.rtn.status != Success)
				return sess.rtn.status;
			if(sess.opFlags & OpEval && !sysCall)
				(void) rssave();
			}
		else {
			// Not evaluating or repeat count is zero.  Consume arguments if maxArgs > 0 and (1), next
			// symbol exists and is not a right paren; or (2), evaluating and minArgs > 0.
			if(maxArgs > 0 && ((haveSym(s_any, false) && !haveSym(s_rightParen, false)) ||
			 (sess.opFlags & OpEval && minArgs > 0))) {
				ExprNode node2;
				Datum *pValue2;
				bool first = true;
				int argCount = 0;
				if((pValue2 = dnewtrack()) == NULL)
					return libfail();
				nodeInit(&node2, pValue2, 0);

				// Get initial argument and following arguments until no comma delimiter found; that is, get
				// "actual" arguments, ignoring minArgs and maxArgs (which are checked afterward).
				for(;;) {
#if MMDebug & Debug_Expr
					fprintf(logfile,
					 "### callCFA(): consuming (minArgs %d, maxArgs %d): examing token '%s'...\n",
					 minArgs, maxArgs, pLastParse->tok.str);
#endif
					if(first)
						first = false;
					else if(!needSym(s_comma, false))
						break;					// Error or no arguments left.
#if MMDebug & Debug_Expr
					if(ge_debug1("ge_andOr", 1, &node2, NULL, ge_andOr) != Success)
#else
					nodeClear(&node2);
					if(ge_andOr(&node2) != Success)			// Get next expression.
#endif
						break;
					++argCount;
					}
				if(sess.rtn.status != Success)
					goto Retn;
				if(argCount < minArgs || argCount > maxArgs)
					goto Wrong;
				}
			}

		// Check for extra argument(s).
		if(maxArgs > 0 && haveSym(s_comma, false))
Wrong:
			return rsset(Failure, 0, text69, pLastParse->tok.str);
				// "Wrong number of arguments (at token '%s')"
		}
Retn:
	// Get right paren, if applicable.
	if(pNode->flags & EN_HaveLParen) {
		if(haveWhite())
			pNode->flags |= EN_HaveWhite;
		if(needSym(s_rightParen, true))
			pNode->flags &= ~EN_HaveLParen;
		}

	return sess.rtn.status;
	}
	
// Evaluate postfix expression and return status.  Postfix expressions are any of:
//	primary
//	postfix++
//	postfix--
//	postfix(...)
//	postfix[expression[,expression]]
static int ge_postfix(ExprNode *pNode) {
	uint oldParens = sess.opFlags & OpParens;

#if MMDebug & Debug_Expr
	if(ge_debug1("ge_primary", 1, pNode, NULL, ge_primary) != Success)
#else
	if(ge_primary(pNode) != Success)
#endif
		return sess.rtn.status;
	if(lvalue(pNode, false))
		pNode->flags |= EN_LValue;

	// Examples:
	//	getKey()
	//	myVar => insert myVar, ' times'
	//	myVar++ => myMac => forwChar
	//	3 => myMac myVar => gotoMark		 Will be evaluated as 3 => myMac(myVar => gotoMark())
	//	index('ba', 'a') => setMark
	for(;;) {
		// Process postfix operator, if any.
		switch(pLastParse->sym) {
			case s_incr:
			case s_decr:
				if(pNode->flags & EN_HaveNArg)
					goto NoFunc;
				if(pNode->flags & EN_HaveWhite)
					return rsset(Failure, 0, text512, pLastParse->tok.str);
							// "Unexpected white space (at token '%s')"

				// Perform ++ or -- operation if evaluating.
				if(sess.opFlags & OpEval &&
				 (!lvalue(pNode, true) || bumpVar(pNode, pLastParse->sym == s_incr, false) != Success))
					goto Retn;
				if(getSym() < NotFound)
					goto Retn;
				pNode->flags &= (EN_Format | EN_Concat | EN_HashExpr);
				break;
			case s_leftParen:
				// A function call.  Error if primary was not an identifier or was an lvalue (variable).
				if(!(pNode->flags & EN_HaveIdent))
					return rsset(Failure, 0, text4, text68, pLastParse->tok.str);
							// "%s expected (at token '%s')", "Identifier"
				if(pNode->flags & EN_GNConstVar)
					return rsset(Failure, 0, text52, text118, pNode->pVal->str);
						// "No such %s \"%s\"", "command, function, or alias"

				// Primary was an identifier and not a '$' constant or variable.  Assume "function" type.  If
				// white space preceded the '(', the '(' is assumed to be the beginning of a primary expression
				// and hence, the first function argument "f (...), ..."; otherwise, the "f(..., ...)" form is
				// assumed.  Move past the '(' if no preceding white space and call the routine.
				if(pNode->flags & EN_HaveWhite)
					pNode->flags &= ~EN_HaveWhite;
				else {
					if(getSym() < NotFound)			// Past '('.
						goto Retn;
					pNode->flags |= EN_HaveLParen;
					}

				// Call the function.
				if(callCFA(pNode, NULL) != Success)
					goto Retn;
				goto Clear;
			case s_leftBracket:
				// Possible string, hash, or array reference; e.g.,
				// "[9, [[[0, 1], 2], 3], 8][0, 2] [1][0][0][1] = 5"

				// Skip if white space precedes '['.
				if(pNode->flags & EN_HaveWhite)
					pNode->flags &= ~EN_HaveWhite;
				else {
					long i1;
					Datum *pValue2;
					ExprNode node2;
					bool haveTwo = false;

					// If evaluating, check if current node is an array, hash, or string (otherwise,
					// assume so).
					if(pNode->flags & EN_HaveIdent) {
						if(sess.opFlags & OpEval) {

							// Find and dereference variable.  Error if not array, hash, or string.
							if(vderefn(pNode->pVal, pNode->pVal->str) != Success)
								goto Retn;
							if(!isAryHshStrVal(pNode->pVal))
								goto Retn;
							}
						pNode->flags &= ~(EN_HaveIdent | EN_GNConstVar);
						}
					else if(ge_deref(pNode) != Success || (sess.opFlags & OpEval &&
					 !isAryHshStrVal(pNode->pVal)))
						goto Retn;

					// Get first index or hash key.
					if((pValue2 = dnewtrack()) == NULL)
						goto LibFail;
					nodeInit(&node2, pValue2, 0);
					if(getSym() < NotFound || ge_assign(&node2) != Success)
						goto Retn;
					if(sess.opFlags & OpEval) {
						if(!dtyphash(pNode->pVal)) {
							if(!isIntVal(node2.pVal))
								goto Retn;
							i1 = dgetint(node2.pVal);
							}
						else if(!isStrVal(node2.pVal))
							goto Retn;
						}

					// Get second index, if present.
					if((!(sess.opFlags & OpEval) || !dtypstr(node2.pVal)) && needSym(s_comma, false)) {
						haveTwo = true;
						nodeClear(&node2);

						// If [i,] form, primary node must be a string.
						if(haveSym(s_rightBracket, false)) {
							if(sess.opFlags & OpEval && !dtypstr(pNode->pVal)) {

								// Force "Integer expected" error.
								(void) isIntVal(pNode->pVal);
								goto Retn;
								}

							// Leave node2.pVal as nil to indicate unspecified index2.
							}
						else if(ge_assign(&node2) != Success ||
						 (sess.opFlags & OpEval && !isIntVal(node2.pVal)))
							goto Retn;
						}

					// Move past right bracket.
					if(haveWhite())
						pNode->flags |= EN_HaveWhite;
					if(!needSym(s_rightBracket, true))
						goto Retn;

					// Evaluate if string element, substring, or array slice; otherwise, save index or key
					// in node for possible use as an lvalue later.
					if(sess.opFlags & OpEval) {
						if(haveTwo) {
							if(dtypstr(pNode->pVal)) {
								if(disnil(node2.pVal))
									dsetint(LONG_MAX, node2.pVal);
								if(substr(pNode->pVal, pNode->pVal, i1,
								 dgetint(node2.pVal)) != Success)
									goto Retn;
								}
							else {
								Array *pArray;

								if((pArray = aslice(dgetarrayptr(pNode->pVal), i1,
								 dgetint(node2.pVal), 0)) == NULL)
									goto LibFail;
								astash(pNode->pVal, pArray);
								}
							}
						else if(dtypstr(pNode->pVal)) {
							long len = strlen(pNode->pVal->str);
							if(len == 0 || i1 >= len || labs(i1) > len)
								return rsset(Failure, 0, text524, i1, len);
									// "No such string element %'ld (string length %'ld)"
							dsetint(pNode->pVal->str[i1 < 0 ? len + i1 : i1], pNode->pVal);
							}
						else if(dtyphash(pNode->pVal)) {
							pNode->i.pKey = node2.pVal;
							pNode->flags |= EN_HshNodRef | EN_LValue;
							}
						else {
							pNode->i.index = i1;
							pNode->flags |= EN_AryElRef | EN_LValue;
							}
						}
					break;
					}
				// Fall through.
			default:
				// Was primary a non-variable identifier?
				if((pNode->flags & (EN_HaveIdent | EN_GNConstVar)) == EN_HaveIdent) {
					bool found;

					if(callCFA(pNode, &found) != Success)
						goto Retn;
					if(found) {
Clear:
						// Clear all flags, except ones not obviated by a function call.
						pNode->flags &= (EN_HaveWhite | EN_Format | EN_Concat | EN_HashExpr);
						break;
						}
					}

				// Not a function call.  Was last symbol a numeric prefix operator?
				if(pNode->flags & EN_HaveNArg)
NoFunc:
					return rsset(Failure, 0, text4, text67, pLastParse->tok.str);
							// "%s expected (at token '%s')", "Function call"

				// No postfix operators left.  Bail out.
				pNode->flags &= ~EN_HaveWhite;
				goto LoopExit;
			}
		}
LoopExit:
	sess.opFlags = (sess.opFlags & ~OpParens) | oldParens;
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Evaluate unary expression and return status.  Unary expressions are any of:
//	postfix
//	!unary
//	~unary
//	++unary
//	--unary
//	-unary
//	+unary
static int ge_unary(ExprNode *pNode) {
	Symbol sym;

	switch(sym = pLastParse->sym) {
		case s_minus:
			// If next character is a digit, set node flag so that LONG_MIN can be converted to an integer correctly
			// by ge_primary().
			if(is_digit(*pLastParse->src))
				pNode->flags |= EN_HaveNegSign;
			// Fall through.
		case s_plus:
		case s_decr:
		case s_incr:
		case s_not:
		case s_bitCompl:
#if MMDebug & Debug_Expr
			if(getSym() < NotFound || ge_debug1("ge_unary", 1, pNode, NULL, ge_unary) != Success)
#else
			if(getSym() < NotFound || ge_unary(pNode) != Success)
#endif
				return sess.rtn.status;
			if(sym == s_incr || sym == s_decr) {

				// Perform ++ or -- operation if evaluating.
				if(sess.opFlags & OpEval && lvalue(pNode, true))
					(void) bumpVar(pNode, sym == s_incr, true);
				pNode->flags &= (EN_Format | EN_Concat | EN_HashExpr);
				}

			else {
				// Perform the operation.
#if MMDebug & Debug_Expr
				if(ge_debug1("ge_deref", 0, pNode, NULL, ge_deref) != Success)
#else
				if(ge_deref(pNode) != Success)
#endif
					return sess.rtn.status;
				if(sess.opFlags & OpEval) {
					if(sym != s_not && !isIntVal(pNode->pVal))
						return sess.rtn.status;
					if(sym == s_not)
						dsetbool(toBool(pNode->pVal) == false, pNode->pVal);
					else if(sym != s_plus && (sym != s_minus || !(pNode->flags & EN_HaveNegSign)))
						dsetint(sym == s_minus ? -dgetint(pNode->pVal) : ~dgetint(pNode->pVal),
						 pNode->pVal);
					}
				pNode->flags &= ~EN_HaveNegSign;
				}
			break;
		default:
#if MMDebug & Debug_Expr
			(void) ge_debug1("ge_postfix", 1, pNode, NULL, ge_postfix);
#else
			(void) ge_postfix(pNode);
#endif
		}
	return sess.rtn.status;
	}

// Perform operation per "sym" on two array nodes.  If "clone" is true, clone array in *pNode1 first.  Also, if union or
// concatenation operation (which copy elements), force object copy if *pNode2 is an lvalue.
static int arrayOp(ExprNode *pNode1, ExprNode *pNode2, Symbol sym, bool clone) {
	Array *pArray1;

	if(clone && arrayClone(pNode1->pVal, pNode1->pVal) != Success)
		return sess.rtn.status;
	pArray1 = dgetarrayptr(pNode1->pVal);

	// Perform operation per sym.
	switch(sym) {
		case s_mult:		// Intersection.
		case s_assignMult:
			if(amatch(pArray1, dgetarrayptr(pNode2->pVal), OpInPlace) == NULL)
				goto LibFail;
			break;
		case s_minus:		// Exclusion.
		case s_assignSub:
			if(amatch(pArray1, dgetarrayptr(pNode2->pVal), OpInPlace | OpNonMatching) == NULL)
				goto LibFail;
			break;
		case s_bitOr:		// Union.
		case s_assignBitOr:
			if(auniq(pArray1, dgetarrayptr(pNode2->pVal),
			 pNode2->flags & EN_LValue ? OpInPlace | OpObjCopy : OpInPlace) == NULL)
				goto LibFail;
			goto AHTrack;
			break;
		default:		// s_bitAnd, s_assignBitAnd - Concatenation.
			if(acat(pArray1, dgetarrayptr(pNode2->pVal),
			 pNode2->flags & EN_LValue ? OpInPlace | OpObjCopy : OpInPlace) == NULL)
				goto LibFail;
AHTrack:;
			// Add any new arrays or hashes (after object copy) to tracking list.
			if(pNode2->flags & EN_LValue) {
#if MMDebug & (Debug_ArrayHash | Debug_AryHshBuf | Debug_AryHshLog | Debug_Datum)
				fputs("arrayOp(): tracking object-copy arrays and hashes...\n", logfile);
#endif
				ahtrack(pNode1->pVal, true);
				}
			break;
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Concatenate two string or hash nodes and return status.
static int concat(ExprNode *pNode1, ExprNode *pNode2, bool clone) {

	if(dtyphash(pNode1->pVal)) {
		if(!clone || hashClone(pNode1->pVal, pNode1->pVal) == Success) {
			if(hcat(dgethashptr(pNode1->pVal), dgethashptr(pNode2->pVal),
			 pNode2->flags & EN_LValue ? OpInPlace | OpObjCopy : OpInPlace) == NULL)
				goto LibFail;
			if(pNode2->flags & EN_LValue)
				ahtrack(pNode1->pVal, true);
			}
		}
	else if(!disnil(pNode2->pVal)) {
		DFab fab;

		if(dopenwith(&fab, pNode1->pVal, FabAppend) != 0 || dputs(pNode2->pVal->str, &fab, 0) != 0 ||
		 dclose(&fab, FabStr) != 0)
			goto LibFail;
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Common routine to handle all of the legwork and error checking for all of the binary operators.
#if MMDebug & Debug_Expr
static int ge_binaryOp(ExprNode *pNode, const char *funcName, OpInfo *pOpInfo) {
#else
static int ge_binaryOp(ExprNode *pNode, OpInfo *pOpInfo) {
#endif
	Symbol *pSym;
	Datum *pOp, *pValue2;
	ExprNode node2;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1(funcName, 1, pNode, NULL, pOpInfo->upFunc) != Success)
#else
	if(pOpInfo->upFunc(pNode) != Success)
#endif
		return sess.rtn.status;

	if((pOp = dnewtrack()) == NULL || (pValue2 = dnewtrack()) == NULL)
		goto LibFail;

	// Loop until no operator(s) at this level remain.
	for(;;) {
		pSym = pOpInfo->pSym;
		for(;;) {
			if(*pSym == pLastParse->sym)
				break;
			if(*pSym == s_any) {

				// No operators left.  Clear EN_Format or EN_Concat flag if format or concatenate op.
				if(pOpInfo->flags & FF_Format && pNode->flags & EN_Format)
					pNode->flags &= ~EN_Format;
				else if(pOpInfo->flags & FF_Concat && pNode->flags & EN_Concat)
					pNode->flags &= ~EN_Concat;
				goto Retn;
				}
			++pSym;
			}

		// Found valid operator.  Dereference.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref", 0, pNode, NULL, ge_deref) != Success)
#else
		if(ge_deref(pNode) != Success)
#endif
			goto Retn;

		// If evaluating, ignore "operator overload" operations if EN_XXX flag set (like using '*' for multiplication
		// and set intersection) where left operand is incorrect type for operator, which means we are at the wrong
		// recursion level.
		if(sess.opFlags & OpEval) {
			if(*pSym == s_mod) {		// Have '%' op:

				// If "modulus" and either EN_Format flag is set (processing second node of format op) or
				// first operand is string (allow format processing on return), bail out.
				if(pOpInfo->flags & FF_Math && (pNode->flags & EN_Format || dtypstr(pNode->pVal)))
					goto Retn;
				}
			else if(*pSym == s_mult ||
			 *pSym == s_minus) {		// Have '*' or '-' op.

				// If "multiplication" or "subtraction" and first operand is array (allow set operation on
				// return), bail out.
				if(pOpInfo->flags & FF_Math && dtyparray(pNode->pVal))
					goto Retn;
				}
			else if(*pSym == s_bitOr) {	// Have '|' op.

				// If "bitwise or" and first operand is array (allow set operation on return), bail out.
				if(pOpInfo->flags & FF_BitOp && dtyparray(pNode->pVal))
					goto Retn;
				}
			if(*pSym == s_bitAnd) {		// Have '&' op.

				// If "bitwise and" and either EN_Concat flag is set (processing second node of concat op) or
				// first operand is string, array, or hash (allow concat processing on return), bail out.
				if(pOpInfo->flags & FF_BitOp && (pNode->flags & EN_Concat ||
				 (pNode->pVal->type & (DStrMask | DArrayMask | DHashMask))))
					goto Retn;
				}
			}

		// We're good.  Save operator for error reporting.
		dxfer(pOp, &pLastParse->tok);

		// Set operator overload flag in second node if applicable, and call function at next higher level.
		nodeInit(&node2, pValue2, 0);
		if(pOpInfo->flags & (FF_Format | FF_Concat))
			node2.flags = pOpInfo->flags & FF_Format ? EN_Format : EN_Concat;
#if MMDebug & Debug_Expr
		if(getSym() < NotFound || ge_debug1(funcName, 2, pNode, &node2, pOpInfo->upFunc) != Success)
#else
		if(getSym() < NotFound || pOpInfo->upFunc(&node2) != Success)
#endif
			goto Retn;

		// Dereference any lvalue.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref", 0, pNode, &node2, ge_deref) != Success)
#else
		if(ge_deref(&node2) != Success)
#endif
			goto Retn;

		// If evaluating expressions, coerce binary operands and perform operation.
		if(sess.opFlags & OpEval) {
			if(forceFit(pNode, &node2, *pSym == s_regEQ || *pSym == s_regNE ? FF_REQNE :
			 pOpInfo->flags, pOp->str) != Success)
				goto Retn;
			switch(*pSym) {

				// Bitwise.
				case s_bitAnd:
					// If FF_Concat flag is set, do concatenation, otherwise bitwise and.
					if(!(pOpInfo->flags & FF_Concat))
						dsetint(dgetint(pNode->pVal) & dgetint(node2.pVal), pNode->pVal);
					else if(dtyparray(pNode->pVal))
						goto ArrayOp;
					else if(concat(pNode, &node2, pNode->flags & EN_LValue) != Success)
						return sess.rtn.status;
					break;
				case s_bitOr:
					// If FF_Union flag is set, do set union, otherwise bitwise or.
					if(pOpInfo->flags & FF_Union)
						goto ArrayOp;
					dsetint(dgetint(pNode->pVal) | dgetint(node2.pVal), pNode->pVal);
					break;
				case s_bitXOr:
					dsetint(dgetint(pNode->pVal) ^ dgetint(node2.pVal), pNode->pVal);
					break;
				case s_leftShift:
					dsetint((ulong) dgetint(pNode->pVal) << (ulong) dgetint(node2.pVal),
					 pNode->pVal);
					break;
				case s_rightShift:
					dsetint((ulong) dgetint(pNode->pVal) >> (ulong) dgetint(node2.pVal),
					 pNode->pVal);
					break;

				// Multiplicative and additive.
				case s_div:
					if(dgetint(node2.pVal) == 0)
						goto DivZero;
					dsetint(dgetint(pNode->pVal) / dgetint(node2.pVal), pNode->pVal);
					break;
				case s_mod:
					// If FF_Format flag is set, do string formatting, otherwise modulus.
					if(pOpInfo->flags & FF_Format) {
						Datum *pFormat;

						if((pFormat = dnewtrack()) == NULL)
							goto LibFail;
						dxfer(pFormat, pNode->pVal);
						if(strFormat(pNode->pVal, pFormat, node2.pVal) != Success)
							return sess.rtn.status;
						}
					else {
						if(dgetint(node2.pVal) == 0)
DivZero:
						return rsset(Failure, 0, text245, dgetint(pNode->pVal));
								// "Division by zero is undefined (%ld/0)"
						dsetint(dgetint(pNode->pVal) % dgetint(node2.pVal), pNode->pVal);
						}
					break;
				case s_mult:
					// If FF_SetMatch flag is set, do set intersection, otherwise multiplication.
					if(pOpInfo->flags & FF_SetMatch)
						goto ArrayOp;
					dsetint(dgetint(pNode->pVal) * dgetint(node2.pVal), pNode->pVal);
					break;
				case s_plus:
					dsetint(dgetint(pNode->pVal) + dgetint(node2.pVal), pNode->pVal);
					break;
				case s_minus:
					// If FF_SetMatch flag is set, do set union, otherwise subtraction.
					if(!(pOpInfo->flags & FF_SetMatch))
						dsetint(dgetint(pNode->pVal) - dgetint(node2.pVal), pNode->pVal);
					else
ArrayOp:
						if(arrayOp(pNode, &node2, *pSym, pNode->flags & EN_LValue) != Success)
							return sess.rtn.status;
					break;
				case s_eq:
				case s_ne:
					{bool b = true;		// Default if two nil, false, or true operands.
					if(pNode->pVal->type != node2.pVal->type)
						b = false;
					else if(!disnil(pNode->pVal) && !dtypbool(pNode->pVal)) {
						b = dtyphash(pNode->pVal) ?
						 heq(dgethashptr(pNode->pVal), dgethashptr(node2.pVal), 0) :
						 dtyparray(pNode->pVal) ?
						 aeq(dgetarrayptr(pNode->pVal), dgetarrayptr(node2.pVal), 0) :
						 pNode->pVal->type == dat_int ?
						 dgetint(pNode->pVal) == dgetint(node2.pVal) :
						 strcmp(pNode->pVal->str, node2.pVal->str) == 0;
						}
					dsetbool(b == (*pSym == s_eq), pNode->pVal);
					}
					break;
				case s_ge:
				case s_gt:
				case s_le:
				case s_lt:
					{long longVal1, longVal2;

					if(pNode->pVal->type == dat_int) {	// Both operands are integer.
						longVal1 = dgetint(pNode->pVal);
						longVal2 = dgetint(node2.pVal);
						}
					else {					// Both operands are string.
						longVal1 = strcmp(pNode->pVal->str, node2.pVal->str);
						longVal2 = 0L;
						}
					dsetbool(*pSym == s_lt ? longVal1 < longVal2 : *pSym == s_le ? longVal1 <= longVal2 :
					 *pSym == s_gt ? longVal1 > longVal2 : longVal1 >= longVal2, pNode->pVal);
					}
					break;

				// RE equality: s_regEQ, s_regNE
				default:
					if(disnull(node2.pVal))
						return rsset(Failure, 0, text187, text266);
							// "%s cannot be null", "Regular expression"

					// Compile the RE pattern.
					if(newSearchPat(node2.pVal->str, &strMatch, NULL, false) == Success) {
						if(strMatch.flags & SOpt_Plain)
							return rsset(Failure, 0, text36, OptCh_Plain, pOp->str);
								// "Invalid pattern option '%c' for '%s' operator"
						grpFree(&strMatch);
						if(compileRE(&strMatch, SCpl_ForwardRE) == Success) {
							regmatch_t group0;

							// Perform operation.
							if(regcmp(pNode->pVal, 0, &strMatch, &group0) == Success)
								dsetbool((group0.rm_so >= 0) == (*pSym == s_regEQ),
								 pNode->pVal);
							}
						}
				}
			pNode->flags &= ~EN_LValue;
			}
		}
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Process multiplication, division and modulus operators.
static int ge_mult(ExprNode *pNode) {
	static Symbol sym_mult[] = {s_mult, s_div, s_mod, s_any};
	static OpInfo opInfo_mult = {ge_unary, sym_mult, FF_Math};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_unary", &opInfo_mult);
#else
	return ge_binaryOp(pNode, &opInfo_mult);
#endif
	}

// Process addition and subtraction operators.
static int ge_add(ExprNode *pNode) {
	static Symbol sym_add[] = {s_plus, s_minus, s_any};
	static OpInfo opInfo_add = {ge_mult, sym_add, FF_Math};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_mult", &opInfo_add);
#else
	return ge_binaryOp(pNode, &opInfo_add);
#endif
	}

// Process numeric prefix (n) operator:
//	add-expression => postfix
static int ge_numPrefix(ExprNode *pNode) {

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_add", 1, pNode, NULL, ge_add) != Success)
#else
	if(ge_add(pNode) != Success)
#endif
		return sess.rtn.status;

	// Loop until no operator at this level remains.
	while(pLastParse->sym == s_bind) {

		// Last expression was an n argument or hash key.  Do nothing if the latter and not evaluating or node type
		// is string.
		if(pNode->flags & EN_HashExpr && !(sess.opFlags & OpEval))
			break;
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref", 0, pNode, NULL, ge_deref) != Success)
#else
		if(ge_deref(pNode) != Success)
#endif
			break;

		// Stop here if evaluating hash key and node type is string; otherwise (have n argument), verify that value is
		// an integer and save it in the node so that the next expression (a function call) can grab it.
		if(sess.opFlags & OpEval) {
			if(pNode->flags & EN_HashExpr && dtypstr(pNode->pVal))
				break;
			if(!isIntVal(pNode->pVal))
				break;
			pNode->nArg = dgetint(pNode->pVal);
			}
		pNode->flags |= EN_HaveNArg;

		// The next expression must be a function call (which is verified by ge_postfix()).
#if MMDebug & Debug_Expr
		if(getSym() < NotFound || ge_debug1("ge_postfix", 2, pNode, NULL, ge_postfix) != Success)
#else
		if(getSym() < NotFound || ge_postfix(pNode) != Success)
#endif
			break;
		}

	return sess.rtn.status;
	}

// Process shift operators << and >>.
static int ge_shift(ExprNode *pNode) {
	static Symbol sym_shift[] = {s_leftShift, s_rightShift, s_any};
	static OpInfo opInfo_shift = {ge_numPrefix, sym_shift, FF_Shft};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_numPrefix", &opInfo_shift);
#else
	return ge_binaryOp(pNode, &opInfo_shift);
#endif
	}

// Process bitwise and operator &.
static int ge_bitAnd(ExprNode *pNode) {
	static Symbol sym_bitAnd[] = {s_bitAnd, s_any};
	static OpInfo opInfo_bitAnd = {ge_shift, sym_bitAnd, FF_BitOp};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_shift", &opInfo_bitAnd);
#else
	return ge_binaryOp(pNode, &opInfo_bitAnd);
#endif
	}

// Process bitwise or and xor operators | and ^.
static int ge_bitOr(ExprNode *pNode) {
	static Symbol sym_bitOr[] = {s_bitOr, s_bitXOr, s_any};
	static OpInfo opInfo_bitOr = {ge_bitAnd, sym_bitOr, FF_BitOp};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_bitAnd", &opInfo_bitOr);
#else
	return ge_binaryOp(pNode, &opInfo_bitOr);
#endif
	}

// Process string format operator %.
static int ge_format(ExprNode *pNode) {
	static Symbol sym_format[] = {s_mod, s_any};
	static OpInfo opInfo_format = {ge_bitOr, sym_format, FF_Format};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_bitOr", &opInfo_format);
#else
	return ge_binaryOp(pNode, &opInfo_format);
#endif
	}

// Process set matching operators * and -.
static int ge_setMatch(ExprNode *pNode) {
	static Symbol sym_setMatch[] = {s_mult, s_minus, s_any};
	static OpInfo opInfo_setMatch = {ge_format, sym_setMatch, FF_SetMatch};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_format", &opInfo_setMatch);
#else
	return ge_binaryOp(pNode, &opInfo_setMatch);
#endif
	}

// Process set union operator |.
static int ge_union(ExprNode *pNode) {
	static Symbol sym_union[] = {s_bitOr, s_any};
	static OpInfo opInfo_union = {ge_setMatch, sym_union, FF_Union};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_setMatch", &opInfo_union);
#else
	return ge_binaryOp(pNode, &opInfo_union);
#endif
	}

// Process concatenation operator &.
static int ge_concat(ExprNode *pNode) {
	static Symbol sym_concat[] = {s_bitAnd, s_any};
	static OpInfo opInfo_concat = {ge_union, sym_concat, FF_Concat};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_union", &opInfo_concat);
#else
	return ge_binaryOp(pNode, &opInfo_concat);
#endif
	}

// Process relational operators <, <=, > and >=.
static int ge_relational(ExprNode *pNode) {
	static Symbol sym_relational[] = {s_lt, s_gt, s_le, s_ge, s_any};
	static OpInfo opInfo_relational = {ge_concat, sym_relational, FF_Rel};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_concat", &opInfo_relational);
#else
	return ge_binaryOp(pNode, &opInfo_relational);
#endif
	}

// Process equality and inequality operators ==, !=, =~, and !~.
static int ge_equality(ExprNode *pNode) {
	static Symbol sym_equality[] = {s_eq, s_ne, s_regEQ, s_regNE, s_any};
	static OpInfo opInfo_equality = {ge_relational, sym_equality, FF_EQNE};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_relational", &opInfo_equality);
#else
	return ge_binaryOp(pNode, &opInfo_equality);
#endif
	}

// Do logical and/or.
#if MMDebug & Debug_Expr
static int ge_logical(ExprNode *pNode, const char *funcName, int (*func)(ExprNode *pNode)) {
#else
static int ge_logical(ExprNode *pNode, int (*func)(ExprNode *pNode)) {
#endif
	bool b;
	Datum *pValue2;
	ExprNode node2;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1(funcName, 1, pNode, NULL, func) != Success)
#else
	if(func(pNode) != Success)
#endif
		return sess.rtn.status;

	if((pValue2 = dnewtrack()) == NULL)
		return libfail();

	// Loop until no operator(s) at this level remain.
	for(;;) {
		switch(pLastParse->sym) {
			case s_and:
				if(func != ge_equality)
					goto Retn;
				b = false;
				goto AndOr;
			case s_or:
				if(func == ge_equality)
					goto Retn;
				b = true;
AndOr:
				if(getSym() < NotFound)				// Past '&&' or '||'.
					goto Retn;
				nodeInit(&node2, pValue2, 0);
#if MMDebug & Debug_Expr
				if(ge_debug1("ge_deref", 0, pNode, NULL, ge_deref) != Success)
#else
				if(ge_deref(pNode) != Success)			// Dereference var if needed.
#endif
					goto Retn;
				if(!(sess.opFlags & OpEval)) {			// Eating arguments?
#if MMDebug & Debug_Expr
					if(ge_debug1(funcName, 2, &node2, NULL, func) != Success)
#else
					if(func(&node2) != Success)		// Yes, bon appetit.
#endif
						goto Retn;
					}
				else if(toBool(pNode->pVal) == b) {		// No, does first argument determine outcome?
					dsetbool(b, pNode->pVal);		// Yes, convert to logical...
					sess.opFlags &= ~OpEval;		// and eat second argument.
#if MMDebug & Debug_Expr
					(void) ge_debug1(funcName, 2, pNode, &node2, func);
#else
					(void) func(&node2);
#endif
					sess.opFlags |= OpEval;
					if(sess.rtn.status != Success)
						goto Retn;
					}
				else {
#if MMDebug & Debug_Expr
					if(ge_debug1(funcName, 2, pNode, &node2, func) != Success ||
					 ge_debug1("ge_deref", 0, &node2, NULL, ge_deref) != Success)
#else
					if(func(&node2) != Success ||		// No, evaluate second argument...
					 ge_deref(&node2) != Success)		// dereference var if needed...
#endif
						goto Retn;
					dsetbool(toBool(node2.pVal), pNode->pVal); // and convert to logical.
					}
				break;
			default:
				goto Retn;
			}
		}
Retn:
	return sess.rtn.status;
	}

// Logical and operator &&.
static int ge_and(ExprNode *pNode) {

#if MMDebug & Debug_Expr
	return ge_logical(pNode, "ge_equality", ge_equality);
#else
	return ge_logical(pNode, ge_equality);
#endif
	}

// Logical or operator ||.
static int ge_or(ExprNode *pNode) {

#if MMDebug & Debug_Expr
	return ge_logical(pNode, "ge_and", ge_and);
#else
	return ge_logical(pNode, ge_and);
#endif
	}

// Process conditional (hook) operator ? :.
static int ge_cond(ExprNode *pNode) {

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_or", 1, pNode, NULL, ge_or) != Success)
#else
	if(ge_or(pNode) != Success)
#endif
		return sess.rtn.status;

	if(pLastParse->sym == s_hook) {
		ExprNode node2;
		Datum *pValue2;
		bool loop2 = false;
		bool eat = true;

		// Dereference any lvalue.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref", 0, pNode, NULL, ge_deref) != Success)
#else
		if(ge_deref(pNode) != Success)
#endif
			return sess.rtn.status;
		if(sess.opFlags & OpEval) {
			eat = !toBool(pNode->pVal);
			if((pValue2 = dnewtrack()) == NULL)
				return libfail();
			}

		// Loop twice.
		for(;;) {
			if(getSym() < NotFound)		// Past '?' or ':'.
				return sess.rtn.status;

			// Don't evaluate one of the arguments if "evaluate mode" was true when we started.
			if(sess.opFlags & OpEval) {
				if(eat) {
					nodeInit(&node2, pValue2, 0);
					sess.opFlags &= ~OpEval;
#if MMDebug & Debug_Expr
					(void) ge_debug1("ge_cond", 2, &node2, NULL, ge_cond);
#else
					(void) ge_cond(&node2);
#endif
					sess.opFlags |= OpEval;
					if(sess.rtn.status != Success)
						return sess.rtn.status;
					eat = false;
					goto NextSym;
					}
				else
					eat = true;
				}
			nodeInit(pNode, pNode->pVal, 0);
#if MMDebug & Debug_Expr
			if(ge_debug1("ge_cond", 1, pNode, NULL, ge_cond) != Success ||
			 ge_debug1("ge_deref", 0, pNode, NULL, ge_deref) != Success)
#else
			if(ge_cond(pNode) != Success || ge_deref(pNode) != Success)
#endif
				return sess.rtn.status;
NextSym:
			if(loop2)
				break;
			if(!haveSym(s_any, false) || pLastParse->sym != s_colon)
				return rsset(Failure, 0, text4, "':'", pLastParse->tok.str);
					// "%s expected (at token '%s')"
			loop2 = true;
			}
		}

	return sess.rtn.status;
	}

// Process assignment operators =, +=, -=, *=, /=, %=, <<=, >>=, &=, ^=, |=, and parallel assignment x, y, z = [1, 2, 3].
int ge_assign(ExprNode *pNode) {
	VarDesc varDesc;
	Datum *pValue2, *pOp;
	ExprNode node2;
	uint f;
	Symbol sym;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_cond", 1, pNode, NULL, ge_cond) != Success)
#else
	if(ge_cond(pNode) != Success)
#endif
		return sess.rtn.status;

	// Error if doing parallel assignment and symbol not '=' or ','.
	if(pNode->flags & EN_ParAssign && (pLastParse->sym != s_assign && pLastParse->sym != s_comma))
		return rsset(Failure, 0, text4, "'='", pLastParse->tok.str);
			// "%s expected (at token '%s')"

	// Have assignment operator?
	if((pLastParse->sym < s_assign || pLastParse->sym > s_assignBitOr) &&
	 (pLastParse->sym != s_comma || !(pNode->flags & (EN_TopLevel | EN_ParAssign)))) {

		// No, dereference any identifier or array or hash reference and return.
#if MMDebug & Debug_Expr
		fprintf(logfile, "### ge_assign(): Dereferencing '%s'...\n", dtype(pNode->pVal, false));
#endif
		return ge_deref(pNode);
		}

	// Have valid operator.  Check if node is an lvalue and build a VarDesc object from it if evaluating.
	if(sess.opFlags & OpEval) {
		if(!(pNode->flags & (EN_HaveIdent | EN_AryElRef | EN_HshNodRef)))
			goto BadLValue;
		if(pNode->flags & EN_HaveIdent) {

			// Error if name matches an existing command, pseudo-command, function, or alias.
			if(execFind(pNode->pVal->str, OpQuery, PtrAny, NULL))
BadLValue:
				return rsset(Failure, 0, text4, text82, pLastParse->tok.str);
					// "%s expected (at token '%s')", "Variable name"
			if(vfind(pNode->pVal->str, &varDesc, pLastParse->sym == s_assign ||
			 pLastParse->sym == s_comma ? OpCreate : OpDelete) != Success)
				return sess.rtn.status;
			}
		else {
			int (*cvtRef)(VarDesc *pVarDesc, ExprNode *pNode, uint oflags) = pNode->flags & EN_AryElRef ?
			 cvtAryElRef : cvtHshNodRef;
			if(cvtRef(&varDesc, pNode, pLastParse->sym == s_assign || pLastParse->sym == s_comma ? OpCreate :
			 OpUpdate) != Success)
				return sess.rtn.status;
			}
		}
	pNode->flags &= ~(EN_HaveIdent | EN_HaveLValue);

	// Set coercion flags.
	switch(sym = pLastParse->sym) {
		case s_comma:
		case s_assign:			// =
			break;
		case s_assignSub:		// -=
		case s_assignMult:		// *=
			if(sess.opFlags & OpEval && !isIntVar(&varDesc)) {

				// Assume lvalue is an array, hence doing a set operation.
				f = FF_SetMatch;
				break;
				}
			// else fall through.
		case s_assignAdd:		// +=
		case s_assignDiv:		// /=
		case s_assignMod:		// %=
			f = FF_Math;
			break;
		case s_assignLeftShift:		// <<=
		case s_assignRightShift:	// >>=
			f = FF_Shft;
			break;
		case s_assignBitAnd:		// &=
		case s_assignBitOr:		// |=
			if(sess.opFlags & OpEval && !isIntVar(&varDesc)) {
				f = sym == s_assignBitAnd ? FF_Concat : FF_Union;
				break;
				}
			// else fall through.
		default:			// ^=
			f = FF_BitOp;
		}

	// If evaluating, save assign op (for error reporting).
	if(sess.opFlags & OpEval) {
		if((pOp = dnewtrack()) == NULL)
			goto LibFail;
		dxfer(pOp, &pLastParse->tok);
		}

	// Move past operator and prepare to get value expression.
	if(getSym() < NotFound)
		return sess.rtn.status;
	if((pValue2 = dnewtrack()) == NULL)
		goto LibFail;
	nodeInit(&node2, pValue2, 0);

	// If doing parallel assignment, use nArg in node for array index, and set it for next recursive call.
	if(sym == s_comma) {
		node2.flags = EN_ParAssign;
		if(pNode->flags & EN_ParAssign)
			node2.nArg = pNode->nArg + 1;		// Not first comma.  Bump index for next instance.
		else {						// First comma.  Initalize both nodes.
			pNode->nArg = 0;			// Current instance (lvalue) gets first array element...
			node2.nArg = 1;				// and next instance gets second.
			pNode->flags |= EN_ParAssign;
			}
		}

	// Get value.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_assign", 2, pNode, &node2, ge_assign) != Success)
#else
	if(ge_assign(&node2) != Success)
#endif
		return sess.rtn.status;

	// If evaluating...
	if(sess.opFlags & OpEval) {

		// Get current variable value, array element, or hash node into *pNode if not straight assignment, and coerce
		// operands into a compatible type.
		if(sym != s_assign && sym != s_comma)
			if(vderefv(pNode->pVal, &varDesc) != Success || forceFit(pNode, &node2, f, pOp->str) != Success)
				return sess.rtn.status;

		// Do operation and put result into *pNode.  *pNode contains left side and node2 contains right.
		switch(sym) {
			case s_assign:
				// Value must be an array if doing parallel assignment.
				if(pNode->flags & EN_ParAssign) {
					if(!isArrayVal(node2.pVal))
						return sess.rtn.status;
					pNode->flags &= ~EN_ParAssign;
					goto PAssign;
					}
				if(dtypah(pNode->pVal))
					ahtrack(pNode->pVal, false);
				dxfer(pNode->pVal, node2.pVal);
				break;
			case s_comma:
PAssign:
				// Set lvalue to array element, or nil if element does not exist.  Return whole array in node.
				{Array *pArray = dgetarrayptr(node2.pVal);
				if(pNode->nArg >= pArray->used) {
					Datum nilVal;
					dinit(&nilVal);
					(void) setVar(&varDesc, &nilVal);
					}
				else
					(void) setVar(&varDesc, pArray->elements[pNode->nArg]);
				}
				dxfer(pNode->pVal, node2.pVal);
				goto Retn;
			case s_assignAdd:
				dgetint(pNode->pVal) += dgetint(node2.pVal);
				break;
			case s_assignSub:
				if(f & FF_SetMatch)
					goto ArrayOp;
				dgetint(pNode->pVal) -= dgetint(node2.pVal);
				break;
			case s_assignMult:
				if(f & FF_SetMatch)
					goto ArrayOp;
				dgetint(pNode->pVal) *= dgetint(node2.pVal);
				break;
			case s_assignDiv:
				if(dgetint(node2.pVal) == 0L)
					goto DivZero;
				dgetint(pNode->pVal) /= dgetint(node2.pVal);
				break;
			case s_assignMod:
				if(dgetint(node2.pVal) == 0L)
DivZero:
					return rsset(Failure, 0, text245, dgetint(pNode->pVal));
						// "Division by zero is undefined (%ld/0)"
				dgetint(pNode->pVal) %= dgetint(node2.pVal);
				break;
			case s_assignLeftShift:
				dgetint(pNode->pVal) <<= dgetint(node2.pVal);
				break;
			case s_assignRightShift:
				dgetint(pNode->pVal) >>= (ulong) dgetint(node2.pVal);
				break;
			case s_assignBitAnd:
				if(!(f & FF_Concat))
					dgetint(pNode->pVal) &= dgetint(node2.pVal);
				else if(dtyparray(pNode->pVal))
					goto ArrayOp;
				else {
					if(concat(pNode, &node2, false) != Success)
						return sess.rtn.status;

					// If left node is a hash, the lvalue was modified directly by concat(), so skip
					// (redundant) call to setVar().
					if(dtyphash(pNode->pVal))
						goto Retn;
					}
				break;
			case s_assignBitXOr:
				dgetint(pNode->pVal) ^= dgetint(node2.pVal);
				break;
			default:	// s_assignBitOr
				if(f & FF_BitOp)
					dgetint(pNode->pVal) |= dgetint(node2.pVal);
				else {
ArrayOp:
					if(arrayOp(pNode, &node2, sym, false) != Success)
						return sess.rtn.status;

					// Lvalue was modified directly by arrayOp(), so skip (redundant) call to setVar().
					goto Retn;
					}
			}
		(void) setVar(&varDesc, pNode->pVal);
		}
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Evaluate low precedence logical not expression "not".
static int ge_not(ExprNode *pNode) {

	if(pLastParse->sym == kw_not) {
#if MMDebug & Debug_Expr
		if(getSym() < NotFound || ge_debug1("ge_not", 1, pNode, NULL, ge_not) != Success)
#else
		if(getSym() < NotFound || ge_not(pNode) != Success)
#endif
			return sess.rtn.status;

		// Perform operation.
		if(sess.opFlags & OpEval)
			dsetbool(!toBool(pNode->pVal), pNode->pVal);
		}
	else
#if MMDebug & Debug_Expr
		(void) ge_debug1("ge_assign", 1, pNode, NULL, ge_assign);
#else
		(void) ge_assign(pNode);
#endif

	return sess.rtn.status;
	}

// Evaluate low precedence logical and/or expressions "and", "or".
int ge_andOr(ExprNode *pNode) {
	bool eval, priorTruth, curTruth;
	Datum *pValue2;
	ExprNode node2;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_not", 1, pNode, NULL, ge_not) != Success)
#else
	if(ge_not(pNode) != Success)
#endif
		return sess.rtn.status;

	if((pValue2 = dnewtrack()) == NULL)
		return libfail();
	eval = (sess.opFlags & OpEval) != 0;

	// Loop until no operator(s) at this level remain.  If we weren't evaluating initially (eval is false), then all ops
	// evaluate to false.
	for(;;) {
		priorTruth = toBool(pNode->pVal);
		switch(pLastParse->sym) {		// true or false or true
			case kw_and:			// false and true and false
				curTruth = false;	// true or false and EVAL
				goto AndOr;		// false and true or EVAL
			case kw_or:
				curTruth = true;
AndOr:
				if(getSym() < NotFound)				// Past 'and' or 'or'.
					goto Retn;
				nodeInit(&node2, pValue2, 0);
				if(!(sess.opFlags & OpEval)) {			// Eating arguments?
					if(eval && curTruth != priorTruth) {	// Yes, stop the gluttony?
						sess.opFlags |= OpEval;		// Yes, enough already.
						goto Eval;
						}
#if MMDebug & Debug_Expr
					if(ge_debug1("ge_not", 2, pNode, &node2, ge_not) != Success)
#else
					if(ge_not(&node2) != Success)		// No, bon appetit.
#endif
						goto Retn;
					}
				else if(priorTruth == curTruth) {		// No, does prior argument determine outcome?
					dsetbool(curTruth, pNode->pVal);	// Yes, convert to logical...
					sess.opFlags &= ~OpEval;		// and eat next argument.
#if MMDebug & Debug_Expr
					(void) ge_debug1("ge_not", 2, pNode, &node2, ge_not);
#else
					(void) ge_not(&node2);
#endif
					sess.opFlags |= OpEval;
					if(sess.rtn.status != Success)
						goto Retn;
					}
				else {						// No, evaluate next argument.
Eval:
#if MMDebug & Debug_Expr
					if(ge_debug1("ge_not", 2, pNode, &node2, ge_not) != Success)
#else
					if(ge_not(&node2) != Success)
#endif
						goto Retn;
					dsetbool(toBool(node2.pVal), pNode->pVal);
					}
				break;
			default:
				goto Retn;
			}
		}
Retn:
	return sess.rtn.status;
	}

// Return type of Datum object as a string.
char *dtype(Datum *pDatum, bool terse) {
	char *str;

	switch(pDatum->type) {
		case dat_nil:
			str = viz_nil;
			break;
		case dat_false:
		case dat_true:
			str = terse ? "bool" : "Boolean";
			break;
		case dat_int:
			str = terse ? "int" : "integer";
			break;
		case dat_miniStr:
		case dat_longStr:
			str = "string";
			break;
		case dat_array:
		case dat_arrayRef:
			str = "array";
			break;
		default:
			str = "hash";
		}
	return str;
	}

// Validate a value per flags.  Return status.
static int validateArg(Datum *pDatum, uint argFlags) {

	if((argFlags & (ArgInt1 | ArgMay)) == ArgInt1)					// 'int' flag set without "may be"?
		(void) isIntVal(pDatum);						// Yes, must be integer.
	else if((argFlags & (ArgHash1 | ArgNIS1 | ArgMay)) == ArgHash1)			// No, 'hash' set without other types?
		(void) isHashVal(pDatum);						// Yes, must be hash.
	else if((argFlags & (ArgArray1 | ArgNIS1 | ArgMay)) == ArgArray1)		// No, 'array' set without other types?
		(void) isArrayVal(pDatum);						// Yes, must be array.
	else if(!(argFlags & (ArgNil1 | ArgBool1 | ArgInt1 | ArgArray1 | ArgHash1 | ArgNIS1))) {
											// No, non-string flags not set?
		if(isStrVal(pDatum) && argFlags & ArgNotNull1 && disnull(pDatum))	// Yes, must be string.  Also check if
			goto NotNull;							// null string is an error.
		}
	else if((!(argFlags & ArgHash1) && dtyphash(pDatum)) ||			// No required types.  Check if found type
	 (!(argFlags & ArgArray1) && dtyparray(pDatum)) ||			// is allowed.
	 (!(argFlags & ArgBool1) && dtypbool(pDatum)) ||
	 (!(argFlags & ArgNIS1) && (
	 (!(argFlags & ArgNil1) && disnil(pDatum)) ||
	 (!(argFlags & ArgInt1) && pDatum->type == dat_int))))
		return rsset(Failure, 0, text329, dtype(pDatum, false));
				// "Unexpected %s argument"
	else if(dtypstr(pDatum) && argFlags & ArgNotNull1 &&	// Null string error?
	 disnull(pDatum))
NotNull:
		return rsset(Failure, 0, text187, text285);
			// "%s cannot be null", "Argument"
	return sess.rtn.status;							// No, all is well.
	}

// Get a script line argument, given pointer to result and argument flags.  Return an error if argument does not conform to
// validation flags.
int funcArg(Datum *pRtnVal, uint argFlags) {
	ExprNode node;

#if MMDebug & Debug_Expr
	fprintf(logfile, "funcArg(..., %.8x): BEGIN at token '%s', remainder '%s'...\n", argFlags, pLastParse->tok.str,
	 pLastParse->src);
#endif
	if(!(argFlags & ArgFirst) && !needSym(s_comma, true))
		return sess.rtn.status;
	nodeInit(&node, pRtnVal, 0);
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_andOr", -1, &node, NULL, ge_andOr) != Success)
#else
	if(ge_andOr(&node) != Success)
#endif
		return sess.rtn.status;

	// If evaluating, validate value and expand pathname, if applicable.
	if(sess.opFlags & OpEval && validateArg(pRtnVal, argFlags) == Success && argFlags & ArgPath && dtypstr(pRtnVal))
		(void) expandPath(pRtnVal, NULL);
	return sess.rtn.status;
	}


// Simulate a series of calls to funcArg() routine by getting next "argument" from a comma-delimited string or an array, given
// indirect pointer to result, indirect pointer to input Datum object, work Datum object (used when input is a string), indirect
// string pointer (for managing a string input), indirect array pointer and indirect array length (for managing an array input).
// *ppInput is assumed to point to either a string or array, and is set to NULL after the first call to this routine.  Routine
// is called repeatedly to obtain next argument until NotFound is returned or a non-Success status.  *ppRtnVal is set to either
// "work" or the next array element for each extracted argument; however, if argument does not conform to validation flags, an
// error is returned.  All arguments are assumed to be optional, including the first.
int nextArg(Datum **ppRtnVal, Datum **ppInput, Datum *pWork, char **keywordList, Datum ***pppArrayEl, ArraySize *pElCount) {

	for(;;) {
		// First call?
		if(*ppInput != NULL) {
			if(dtyparray(*ppInput)) {

				// Set up for array processing.
				Array *pArray = dgetarrayptr((*ppInput));
				*pppArrayEl = pArray->elements;
				*pElCount = pArray->used;
				*keywordList = NULL;
				}
			else {
				// Set up for string processing.
				*keywordList = (*ppInput)->str;
				*pppArrayEl = NULL;
				}
			*ppInput = NULL;
			}

		// Processing an array?
		if(*pppArrayEl != NULL) {
			if((*pElCount)-- == 0)
				return NotFound;
			*ppRtnVal = *(*pppArrayEl)++;
			break;
			}
		else {
			// Get next keyword from string.
			char *str = strparse(keywordList, ',');
			if(str == NULL)
				return NotFound;
			if(*str == '\0')
				continue;
			if(dsetstr(str, pWork) != 0)
				return libfail();
			*ppRtnVal = pWork;
			break;
			}
		// Onward...
		}
	return validateArg(*ppRtnVal, ArgNotNull1);
	}
