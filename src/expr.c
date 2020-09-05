// (c) Copyright 2020 Richard W. Marinelli
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

// Array Management Notes:
//
// Arrays are managed differently than other data types in MightEMacs.  Non-array types, including strings, are always copied
// from place to place (by value) when an expression is evaluated.  For example, when a variable containing a string is
// dereferenced, a copy of the string is placed into the expression node and the variable retains its own copy.  This is not the
// case for arrays.  Except for a few special cases, arrays are always copied by reference instead of by value.  So for example,
// when a variable containing an array is dereferenced, a pointer to that array is placed into the expression node.  If the
// array in the node is subsequently modified, the variable becomes modified as well.  This technique is used is avoid making
// multiple copies of an array during expression evaluation, which would have a significant performance and memory-consumption
// impact.
//
// Passing pointers for arrays creates a problem, however.  It is extremely difficult to determine when an array (which is
// allocated from heap space, like all data types) is no longer being used during expression evaluation and can be freed.  To
// solve this problem, the following method is used:
//   1. Any time an array is created, it is encapsulated (wrapped) in an ArrayWrapper object and then saved in a Datum object.
//	This is accomplished via the awWrap() function.  The ArrayWrapper object is also pushed onto a "garbage collection" list
//	pointed to by "arrayGarbHead".
//   2. The ArrayWrapper object contains a Boolean member, marked, which is used to determine which arrays to keep and which
//	to free (later) and also to prevent endless recursion when an array contains itself.
//   3. Freeing of array space is not attempted at all during expression evaluation, simply because there is no "safe" time at
//	which to do so.  It is not done until just before control is returned to the user, in editLoop(), via a call to the
//	awGarbFree() function.
//   4. The job of awGarbFree() is to scan the arrayGarbHead list, freeing arrays which are not associated with a global
//	variable.  To accomplish this, it first checks the list.  If it is empty, it does nothing; otherwise, it (a), scans the
//	global variable list and adds any arrays found to the garbage list via the awGarbPush() function (which also adds nested
//	arrays) so that all existing arrays will be examined; (b), scans the garbage list and sets marked to false in each
//	array; (c), scans the global variable list again and sets marked to true in any arrays and nested arrays found via the
//	awKeep() function, which also uses marked during recursion to prevent an endless loop if an array contains itself; and
//	(d), scans the garbage list again and frees any arrays which have marked set to false.
//   5. When awGarbFree() is done, the only arrays left in the list will be those contained in global variables.  This will also
//	include nested arrays.  "arrayGarbHead" is set to NULL at this point so that the list will not be scanned every time the
//	user presses a key.
//   6. Lastly, to deal with the arrays contained in global variables, the awGarbPush() function is called whenever a new value
//	is assigned to one of those variables.  It puts the old array and any arrays it contains back onto the garbage list.
//
// Given the fact that arrays are passed around by reference, it is the responsibility of the user to clone arrays where needed
// (via the "clone" function).  However, there are a couple places where it is done automatically:
//	* If the initializer of the "array" function is itself an array, it is cloned for each element of the array created.
//	* If the first item in a concatenation expression is an array and is an lvalue (a variable or array element), it is
//	  cloned so that the value of the variable or array element is not changed.

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
#define FF_Concat	0x0010		// Concatenation.
#define FF_Rel		0x0020		// <, <=, > or >=.
#define FF_REQNE	0x0040		// =~ or !~.
#define FF_EQNE		0x0080		// == or !=.
#define FF_LogAndOr	0x0100		// && or ||.
#define FF_Cond		0x0200		// Conditional (hook).
#define FF_Assign	0x0400		// Straight assignment (=).

#define Str_Left	0x1000		// Convert left operand to string.
#define Str_Right	0x2000		// Convert right operand to string.

// forceFit() table for nil, Boolean (true or false), int, string, and array coersion combinations.
static struct ForceFit {
	ushort legal;			// Legal operations (FF_XXX flags)
	ushort strOp;			// Operations which cause call to toStr().  High bits determine left and/or right side.
	} forceFitTable[][5] = {
		{ /* nil */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* int */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0}},

		{ /* bool */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* int */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0}},

		{ /* int */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* int */	{~(FF_Concat | FF_Format | FF_REQNE), 0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond, 0}},

		{ /* string */
/* nil */	{FF_Assign | FF_Concat | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond, FF_Concat | Str_Right},
/* bool */	{FF_Assign | FF_Concat | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond, FF_Concat | Str_Right},
/* int */	{FF_Assign | FF_Concat | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond, FF_Concat | Str_Right},
/* string */	{FF_Assign | FF_Concat | FF_Format | FF_Rel | FF_REQNE | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* array */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond, FF_Concat | Str_Right}},

		{ /* array */
/* nil */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* bool */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* int */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* string */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond, 0},
/* array */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond, 0}}
		};

#if MMDebug & Debug_Expr
static int indent = -1;
#endif

#if MMDebug & Debug_Array
#if 0
// Write array-usage report to log file.
static int awUsage(bool first) {

	if(arrayGarbHead != NULL) {
		Array *pArray;
		Datum **ppArrayEl, **ppArrayElEnd;
		ArraySize i;
		ArrayWrapper *pWrap = arrayGarbHead;

		fprintf(logfile, "Array List: *%s*\n\n", first ? "BEFORE" : "AFTER");
		do {
			pArray = pWrap->pArray;
			fprintf(logfile, "\tFound wrapper %.8X -> array %.8X (keep %d):\n", (uint) pWrap, (uint) pArray,
			 pWrap->marked);
			ppArrayElEnd = (ppArrayEl = pArray->elements) + pArray->used;
			i = 0;
			while(ppArrayEl < ppArrayElEnd) {
				fprintf(logfile, "\t\t%2d: ", i++);
				if((*ppArrayEl)->type == dat_blobRef) {
					pArray = wrapPtr(*ppArrayEl)->pArray;
					fprintf(logfile, "ARRAY [%.8X], size %d\n", (uint) pArray, pArray->used);
					}
				else
					fprintf(logfile, ((*ppArrayEl)->type & DStrMask) ? "\"%s\"\n" : "%s\n",
					 dtos(*ppArrayEl, true));
				++ppArrayEl;
				}
			} while((pWrap = pWrap->next) != NULL);
		if(first)
			fputc('\n', logfile);
		}
	return sess.rtn.status;
	}
#else
// Write array contents to a fabrication object in abbreviated form.  Return status.
static int awUsage1(ArrayWrapper *pWrap, DFab *pFab) {
	Datum **ppArrayEl = pWrap->pArray->elements;
	Datum **ppArrayElEnd = ppArrayEl + pWrap->pArray->used;
	bool first = true;

	if(dputc('[', pFab) != 0)
		goto LibFail;
	while(ppArrayEl < ppArrayElEnd) {
		if(first)
			first = false;
		else if(dputc(', ', pFab) != 0)
			goto LibFail;
		if((*ppArrayEl)->type == dat_blobRef) {
			Array *pArray = wrapPtr(*ppArrayEl)->pArray;
			if(dputf(pFab, "[%.8X:%.8X:%d]", (uint) wrapPtr(*ppArrayEl), (uint) pArray, pArray->used) != 0)
				goto LibFail;
			}
		else if(dtosf(*ppArrayEl, NULL, CvtExpr, pFab) != Success)
			return sess.rtn.status;
		++ppArrayEl;
		}
	if(dputc(']', pFab) != 0)
LibFail:
		(void) librsset(Failure);

	return sess.rtn.status;
	}

// Write array-usage report to a buffer.
static int awUsage(DFab *pFab, int stage) {

	if(stage <= 0) {
		UserVar *pUserVar;
		ArrayWrapper *pWrap = arrayGarbHead;

		// Dump array info.
		if((stage < 0 && dopentrack(pFab) != 0) ||
		 dputf(pFab, "Array List: *%s*\n\n", stage < 0 ? "BEFORE" : "AFTER") != 0)
			goto LibFail;
		do {
			if(dputf(pFab, "%.8X   %.8X   %c", (uint) pWrap, (uint) pWrap->pArray,
			 pWrap->marked ? '*' : ' ') != 0)
				goto LibFail;
#if 0
			dsetblobref((void *) pWrap, sizeof(ArrayWrapper), &d);
			if(atosfclr(&d, NULL, CvtExpr | CvtForceArray, pFab) != Success)
				return sess.rtn.status;
#else
			if(awUsage1(pWrap, pFab) != Success)
				return sess.rtn.status;
#endif
			if(dputc('\n', pFab) != 0)
				goto LibFail;
			} while((pWrap = pWrap->next) != NULL);

		if(dputs("\nGLOBAL VARIABLES\n\n", pFab) != 0)
			goto LibFail;
		for(pUserVar = globalVarRoot; pUserVar != NULL; pUserVar = pUserVar->next) {
			if(pUserVar->pValue->type == dat_blobRef) {
				pWrap = wrapPtr(pUserVar->pValue);
				if(dputf(pFab, "%-18s %.8X   %.8X   %c", pUserVar->name, (uint) pWrap, (uint) pWrap->pArray,
				 pWrap->marked ? '*' : ' ') != 0)
					goto LibFail;
				if(awUsage1(wrapPtr(pUserVar->pValue), pFab) != Success)
					return sess.rtn.status;
				if(dputc('\n', pFab) != 0)
					goto LibFail;
				}
			}
		}
	else {
		Buffer *pBuf;
		bool created;

		if(dclose(pFab, Fab_String) != 0)
			goto LibFail;

		// Create debug buffer, clear it, and append results.
		if(bfind("Debug", BS_Create, 0, &pBuf, &created) != Success ||
		 (!created && bclear(pBuf, BC_IgnChgd) != Success))
			return sess.rtn.status;
		if(bappend(pBuf, pFab->pDatum->str) == Success) {
			Datum d;

			dinit(&d);
			(void) render(&d, 2, pBuf, RendReset);
			dclear(&d);
			}
		}
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}
#endif
#endif

// Scan the wrapper list and clear all the "marked" flags.
void awClearMarks(void) {
	ArrayWrapper *pWrap = arrayGarbHead;
	while(pWrap != NULL) {
		pWrap->marked = false;
		pWrap = pWrap->next;
		}
	}

// Push an array and all its decendent arrays back on to the garbage list if they are not already there.
static void awGarbPushAll(Datum *pDatum) {
	ArrayWrapper *pGarb = arrayGarbHead;
	ArrayWrapper *pWrap = wrapPtr(pDatum);

	// Check if array is already on the list.
	while(pGarb != NULL) {
		if(pGarb == pWrap) {
			if(pWrap->marked)
				return;
			goto Mark;
			}
		pGarb = pGarb->next;
		}

	// Not found.  Mark it and add it.
	pWrap->next = arrayGarbHead;
	arrayGarbHead = pWrap;
Mark:
	pWrap->marked = true;

	// Scan element list and add those, too.
	Datum **ppArrayEl = pWrap->pArray->elements;
	Datum **ppArrayElEnd = ppArrayEl + pWrap->pArray->used;

	while(ppArrayEl < ppArrayElEnd) {
		if((*ppArrayEl)->type == dat_blobRef)
			awGarbPushAll(*ppArrayEl);
		++ppArrayEl;
		}
	}

// Clear marks in wrapper list and call awGarbPushAll().
void awGarbPush(Datum *pDatum) {

	awClearMarks();
	awGarbPushAll(pDatum);
	}

// Mark given array and all nested arrays (if any) as "keepers".
static void awKeep(Datum *pDatum) {
	ArrayWrapper *pWrap = wrapPtr(pDatum);
	if(!pWrap->marked) {
		Datum **ppArrayEl, **ppArrayElEnd;
		Array *pArray = pWrap->pArray;
		pWrap->marked = true;
		ppArrayElEnd = (ppArrayEl = pArray->elements) + pArray->used;
		while(ppArrayEl < ppArrayElEnd) {
			if((*ppArrayEl)->type == dat_blobRef)
				awKeep(*ppArrayEl);
			++ppArrayEl;
			}
		}
	}

// Free all heap space for unused arrays.
int awGarbFree(void) {

	if(arrayGarbHead != NULL) {
		ArrayWrapper *pWrap;
		UserVar *pUserVar;
#if MMDebug & Debug_Array
		DFab fab;
#endif
		// Scan global variable table and add any arrays found to wrapper list so that all known arrays are examined in
		// the following steps.  This is necessary because a global array currently not on the list may have had an
		// array added to it (for example) that would then not be marked as a "keeper" and would be freed in error.
		for(pUserVar = globalVarRoot; pUserVar != NULL; pUserVar = pUserVar->next)
			if(pUserVar->pValue->type == dat_blobRef)
				awGarbPush(pUserVar->pValue);

		// Clear all "marked" flags in wrapper list.
		awClearMarks();

#if MMDebug & Debug_Array
		if(awUsage(&fab, -1) != Success)
			return sess.rtn.status;
#endif
		// Scan global variable table again and mark any arrays found as "keepers".
		for(pUserVar = globalVarRoot; pUserVar != NULL; pUserVar = pUserVar->next)
			if(pUserVar->pValue->type == dat_blobRef)
				awKeep(pUserVar->pValue);
#if MMDebug & Debug_Array
		if(awUsage(&fab, 0) != Success)
			return sess.rtn.status;
#endif
		// Scan wrapper list, delete any non-keeper arrays, and free wrapper record.
		do {
			pWrap = arrayGarbHead;
			arrayGarbHead = pWrap->next;
			if(!pWrap->marked) {

				// Non-keeper array found.  Free it and its wrapper record.
#if MMDebug & Debug_Array
				if(dputf(&fab, "Freeing array %.8X...\n", (uint) pWrap->pArray) != 0)
					return librsset(Failure);
#endif
				free((void *) aclear(pWrap->pArray));
				free((void *) pWrap);
				}
			} while(arrayGarbHead != NULL);
#if MMDebug & Debug_Array
		(void) awUsage(&fab, 1);
#endif
		}

	return sess.rtn.status;
	}

// Create a wrapper record for an array, save pointer to it in given Datum object, and add it to the garbage list.  Return
// status.
int awWrap(Datum *pRtnVal, Array *pArray) {
	ArrayWrapper *pWrap;

	if((pWrap = (ArrayWrapper *) malloc(sizeof(ArrayWrapper))) == NULL)	// Create wrapper object...
		return rsset(Panic, 0, text94, "awWrap");
			// "%s(): Out of memory!"
	pWrap->pArray = pArray;							// initialize it...
	pWrap->next = arrayGarbHead;							// add it to garbage list...
	arrayGarbHead = pWrap;
	dsetblobref((void *) pWrap, sizeof(ArrayWrapper), pRtnVal);			// and set in Datum object.
	return sess.rtn.status;
	}

// Pop datGarbHead to given pointer, releasing heap space for Datum objects and arrays, if applicable.
void garbPop(Datum *pDatum) {
	Datum *pDatum1;

	while(datGarbHead != pDatum) {
		pDatum1 = datGarbHead;
		datGarbHead = datGarbHead->next;
		ddelete(pDatum1);
		}
	}

// Initialize an expression node with given Datum object.
void nodeInit(ExprNode *pNode, Datum *pRtnVal, bool topLevel) {

	dsetnil(pRtnVal);
	pNode->pValue = pRtnVal;
	pNode->flags = topLevel ? EN_TopLevel : 0;
	pNode->nArg = INT_MIN;
	}

// Return true if b is true; otherwise, set given error message and return false.
static bool isTrue(bool b, const char *msg) {

	if(b)
		return true;
	(void) rsset(Failure, RSNoFormat, msg);
	return false;
	}

// Return true if a pDatum object is a character (8-bit unsigned integer); otherwise, set an error and return false.
bool isCharVal(Datum *pDatum) {

	return isTrue(pDatum->type == dat_int && pDatum->u.intNum >= 0 && pDatum->u.intNum <= 0xFF, text288);
			// "Character expected"
	}

// Return true if a pDatum object is an integer; otherwise, set an error and return false.
bool isIntVal(Datum *pDatum) {

	return isTrue(pDatum->type == dat_int, text166);
			// "Integer expected"
	}

// Return true if a pDatum object is a string; otherwise, set an error and return false.
bool isStrVal(Datum *pDatum) {

	return isTrue(pDatum->type & DStrMask, text171);
			// "String expected"
	}

// Return true if a pDatum object is an array; otherwise, set an error and return false.
bool isArrayVal(Datum *pDatum) {

	return isTrue(pDatum->type == dat_blobRef, text371);
			// "Array expected"
	}

// Return true if node value is an lvalue; otherwise, set an error if "required" is true and return false.
static bool lvalue(ExprNode *pNode, bool required) {

	if((pNode->flags & EN_HaveGNVar) || ((pNode->flags & EN_HaveIdent) && findUserVar(pNode->pValue->str) != NULL) ||
	 (pNode->flags & EN_ArrayRef))
		return true;
	if(required) {
		if(pNode->flags & EN_HaveIdent)
			(void) rsset(Failure, 0, text52, pNode->pValue->str);
					// "No such variable '%s'"
		else
			(void) rsset(Failure, 0, text4, text82, pLastParse->tok.str);
					// "%s expected (at token '%s')", "Variable name"
		}
	return false;
	}

#if MMDebug & Debug_Expr
// Write integer or string value to log file, given pDatum object.
void dumpVal(Datum *pDatum) {
	char *str;

	fprintf(logfile, "(%.8X %hu) ", (uint) pDatum, pDatum->type);
	switch(pDatum->type) {
		case dat_nil:
			str = viz_nil;
			break;
		case dat_false:
			str = viz_false;
		case dat_true:
			str = viz_true;
			break;
		case dat_int:
			fprintf(logfile, "%ld", pDatum->u.intNum);
			return;
		case dat_miniStr:
		case dat_soloStr:
			fprintf(logfile, "\"%s\"", pDatum->str);
			return;
		default:
			str = "[array]";
		}

	fputs(str, logfile);
	}

// Write integer or string value to log file, given node.
static void ge_dumpVal(ExprNode *pNode) {
	dumpVal(pNode->pValue);
	}

// Write expression parsing info to log file.
static void ge_dump(const char *filename, ExprNode *pNode1, ExprNode *pNode2, bool begin) {
	int i = indent;
	while(i-- > 0)
		fputs("  ", logfile);
	fprintf(logfile, "%s %s(): %s '%s' (%d), cur val ", begin ? "BEGIN" : "END", filename, begin ? "parsing" :
	 "returning at", pLastParse->tok.str, pLastParse->sym);
	ge_dumpVal(pNode1);
	fprintf(logfile, ", flags %.8X", pNode1->flags);
	if(pNode2 != NULL) {
		fputs(", val2 ", logfile);
		ge_dumpVal(pNode2);
		fprintf(logfile, ", flags %.8X", pNode2->flags);
		}
	fprintf(logfile, ", EvalMode %x", sess.opFlags & OpEval);
	if(begin)
		fputs("...\n", logfile);
	else
		fprintf(logfile, " [%d]\n", sess.rtn.status);
	}

// Intercept all calls to ge_xxx() functions and write status information to log file.
static int ge_debug1(const char *filename, ExprNode *pNode1, ExprNode *pNode2, int (*upFunc)(ExprNode *pNode)) {
	int status;

	++indent;
	ge_dump(filename, pNode1, pNode2, true);
	status = upFunc(pNode2 != NULL ? pNode2 : pNode1);
	ge_dump(filename, pNode1, pNode2, false);
	--indent;
	return status;
	}
#endif

// Dereference an lvalue (variable name or array element reference) in pNode if present and evaluating.  Return status.
static int ge_deref(ExprNode *pNode) {

	if(!(sess.opFlags & OpEval))

		// Not evaluating.  Just clear flags.
		pNode->flags &= ~(EN_TopLevel | EN_ArrayRef | EN_HaveIdent | EN_HaveGNVar | EN_HaveWhite);
	else {
		// Evaluating.  Dereference and clear flags.
		if(pNode->flags & (EN_HaveGNVar | EN_HaveIdent)) {
			(void) vderefn(pNode->pValue, pNode->pValue->str);
			pNode->flags &= ~(EN_TopLevel | EN_HaveIdent | EN_HaveGNVar | EN_HaveWhite);
			}
		else if(pNode->flags & EN_ArrayRef) {
			VarDesc varDesc;

			if(getArrayRef(pNode, &varDesc, false) == Success)
				(void) vderefv(pNode->pValue, &varDesc);
			pNode->flags &= ~(EN_TopLevel | EN_ArrayRef);
			}
		}

	return sess.rtn.status;
	}

// Coerce pDatum objects passed into compatable types for given operation flag(s) and return status.  If illegal fit, return
// error.  "kind" contains operation flag (FF_XXX) and "op" is operator token (for error reporting).
static int forceFit(ExprNode *pNode1, ExprNode *pNode2, ushort kind, const char *op) {

	// Get info from table.
	struct ForceFit *pInfo = forceFitTable[(pNode1->pValue->type == dat_nil) ? 0 : (pNode1->pValue->type & DBoolMask) ? 1 :
	 pNode1->pValue->type == dat_int ? 2 : pNode1->pValue->type == dat_blobRef ? 4 : 3] +
	 ((pNode2->pValue->type == dat_nil) ? 0 : (pNode2->pValue->type & DBoolMask) ? 1 : pNode2->pValue->type == dat_int ? 2 :
	 pNode2->pValue->type == dat_blobRef ? 4 : 3);

	// Valid operand types?
	if(!(pInfo->legal & kind))
#if MMDebug & Debug_Expr
		return rsset(Failure, 0, "Wrong operand type for '%s' (legal %.4hx, kind %.4hx)", op, pInfo->legal, kind);
#else
		return rsset(Failure, 0, text191, op);
				// "Wrong type of operand for '%s'"
#endif
	// Coerce one operand to string if needed.
	if(pInfo->strOp & kind) {
#if MMDebug & Debug_Expr
		ExprNode *pNode;
		char *str;
		if(pInfo->strOp & Str_Left) {
			pNode = pNode1;
			str = "left";
			}
		else {
			pNode = pNode2;
			str = "right";
			}
		fprintf(logfile, "### forceFit(): coercing %s operand...\n", str);
		if(toStr(pNode->pValue) == Success &&
		 (pInfo->strOp & (Str_Left | Str_Right)) == (Str_Left | Str_Right)) {
			fputs("### forceFit(): coercing right operand...\n", logfile);
			(void) toStr(pNode2->pValue);
			}
#else
		if(toStr((pInfo->strOp & Str_Left) ? pNode1->pValue : pNode2->pValue) == Success &&
		 (pInfo->strOp & (Str_Left | Str_Right)) == (Str_Left | Str_Right))
			(void) toStr(pNode2->pValue);
#endif
		}
	return sess.rtn.status;
	}

// Parse a primary expression and save the value in pNode.  If an identifier is found, save its name and set appropriate flags
// in pNode as well.  Return status.  Primary expressions are any of:
//	number
//	"?" char-lit
//	string
//	identifier
//	"true" | "false" | "nil" | "defn"
//	(and-or-expression)
//	[and-or-expression, ...]
static int ge_primary(ExprNode *pNode) {
	bool b;

	switch(pLastParse->sym) {
		case s_numLit:
			// Numeric literal.
			if(sess.opFlags & OpEval) {
				long longVal;

				if(ascToLong(pLastParse->tok.str, &longVal, false) != Success)
					return sess.rtn.status;
				dsetint(longVal, pNode->pValue);
				}
			goto NextSym;
		case s_charLit:
			// Character literal.
			if(sess.opFlags & OpEval) {
				short c;
				char *str = pLastParse->tok.str + 1;	// Past '?'.
				if((c = *str) == '\\' && evalCharLit(&str, &c, true) != Success)
					return sess.rtn.status;
				dsetint(c, pNode->pValue);
				}		
			goto NextSym;
		case s_strLit:
			// String literal.
			if((sess.opFlags & OpEval) && evalStrLit(pNode->pValue, pLastParse->tok.str) != Success)
				return sess.rtn.status;
			goto NextSym;
		case kw_true:					// Convert to true value.
			b = true;
			goto IsBool;
		case kw_false:					// Convert to false value.
			b = false;
IsBool:
			if(sess.opFlags & OpEval)
				dsetbool(b, pNode->pValue);
			goto NextSym;
		case kw_nil:					// Convert to nil value.
			if(sess.opFlags & OpEval)
				dsetnil(pNode->pValue);
			goto NextSym;
		case kw_defn:					// Convert to defn value.
			if(sess.opFlags & OpEval)
				dsetint(defn, pNode->pValue);
			goto NextSym;
		case s_globalVar:
		case s_numVar:
			pNode->flags |= EN_HaveGNVar;
			// Fall through.
		case s_ident:
		case s_identQuery:
			pNode->flags |= EN_HaveIdent;

			// Save identifier name in pNode.
			if(dsetstr(pLastParse->tok.str, pNode->pValue) != 0)
				goto LibFail;

			// Set "white-space-after-identifier" flag for caller.
			if(haveWhite())
				pNode->flags |= EN_HaveWhite;
			goto NextSym;
		case s_leftParen:
			// Parenthesized expression.
			{ushort oldFlag = pNode->flags & EN_TopLevel;
			pNode->flags |= EN_TopLevel;
#if MMDebug & Debug_Expr
			if(getSym() < NotFound || ge_debug1("ge_andOr", pNode, NULL, ge_andOr) != Success ||
			 !haveSym(s_rightParen, true))
#else
			if(getSym() < NotFound || ge_andOr(pNode) != Success || !haveSym(s_rightParen, true))
#endif
				return sess.rtn.status;
			pNode->flags = (pNode->flags & ~EN_TopLevel) | oldFlag;
			}
			goto NextSym;
		case s_leftBracket:
			// Bracketed expression list.  Create array.
			{Array *pArray;
			Datum pWrap;
			bool first = true;

			if(sess.opFlags & OpEval) {
				dinit(&pWrap);
				if(makeArray(&pWrap, 0, &pArray) != Success)
					return sess.rtn.status;
				}
			if(getSym() < NotFound)
				return sess.rtn.status;

			// Get element list, if any.
			pNode->flags &= ~EN_TopLevel;
			for(;;) {
				if(haveSym(s_rightBracket, false))
					break;
				if(!first && !needSym(s_comma, true))
					return sess.rtn.status;

				// Get next expression.
				if(ge_andOr(pNode) != Success)
					return sess.rtn.status;
				if((sess.opFlags & OpEval) && apush(pArray, pNode->pValue, true) != 0)
					goto LibFail;

				// Reset node.
				nodeInit(pNode, pNode->pValue, false);
				first = false;
				}
			if(sess.opFlags & OpEval)
				datxfer(pNode->pValue, &pWrap);
			}
NextSym:
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

	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Handle a command, function, or alias call.
static int callCFA(ExprNode *pNode, bool needRightParen, bool *found) {
	UnivPtr univ;

	// Is identifier a command, function, or alias?
#if MMDebug & (Debug_Datum | Debug_Expr)
	fprintf(logfile, "callCFA(): identifying '%s'... ", pNode->pValue->str);
#endif
	if(execFind(pNode->pValue->str, OpQuery, PtrSysCmdFunc | PtrAlias | PtrUserCmdFunc, &univ)) {
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("found '", logfile);
		switch(univ.type) {
			case PtrAliasSysCmd:
			case PtrAliasSysFunc:
			case PtrAliasUserCmd:
			case PtrAliasUserFunc:
				fprintf(logfile, "%s' ALIAS", univ.u.pAlias->name);
				break;
			case PtrUserCmd:
			case PtrUserFunc:
				fprintf(logfile, "%s' USER %s", univ.u.pBuf->bufname,
				 univ.type == PtrUserCmd ? "COMMAND" : "FUNCTION");
				break;
			case PtrSysCmd:
			case PtrSysFunc:
				fprintf(logfile, "%s' %s", univ.u.pCmdFunc->name,
				 univ.type == PtrSysCmd ? "COMMAND" : "FUNCTION");
			}
		fprintf(logfile, " (type %hu) for token '%s'\n", univ.type, pNode->pValue->str);
#endif
		// Yes.  Resolve any alias.
		if(univ.type & PtrAlias)
			univ = univ.u.pAlias->targ;

		// Check if interactive-only command.
		if(univ.type == PtrSysCmd) {
			if(univ.u.pCmdFunc->attrFlags & CFTerm)
				return rsset(Failure, RSTermAttr, text282, univ.u.pCmdFunc->name);
					// "Cannot invoke '~b%s~B' command directly in a script (use '~brun~B')"

			// If "alias" command (which uses "alias xxx = yyy" syntax), parentheses not allowed.
			if(needRightParen && univ.u.pCmdFunc->func == alias)
				return rsset(Failure, 0, text289, "(");
						// "Unexpected token '%s'"
			}
		if(found != NULL)
			*found = true;

		// Have command, function, user command, or user function at this point.  Determine minimum number of required
		// arguments, if possible.  Set to -1 if unknown.
		short minArgs = (univ.type & PtrUserCmdFunc) ? univ.u.pBuf->pCallInfo->minArgs :
		 !(univ.u.pCmdFunc->attrFlags & (CFAddlArg | CFNoArgs)) ? univ.u.pCmdFunc->minArgs : !(sess.opFlags & OpEval) ?
		 -1 : pNode->nArg == INT_MIN ? univ.u.pCmdFunc->minArgs : (univ.u.pCmdFunc->attrFlags & CFNoArgs) ? 0 :
		 univ.u.pCmdFunc->minArgs + 1;

		// "xxx()" form?
		if(needRightParen && haveSym(s_rightParen, false)) {

			// Yes.  Error if argument(s) required (whether or not evaluating).
			if(minArgs > 0)
				goto Wrong;
			if(!(univ.type & PtrUserCmdFunc)) {
				if((univ.u.pCmdFunc->attrFlags & CFNoArgs) && !(pNode->flags & EN_HaveNArg))
					goto Wrong;
				if(!(sess.opFlags & OpEval) && !(univ.u.pCmdFunc->attrFlags & CFSpecArgs))
					goto Retn;
				}
			}

		// Not "xxx()" call, zero required arguments, or argument requirement cannot be determined.  Proceed with
		// execution or argument consumption.  Note that unusual expressions such as "false && x => seti(5)" where the
		// "seti(5)" is likely an error are not checked here because the numeric prefix is not known when not
		// evaluating.  The specific command or function routine will do the validation.
		short maxArgs;

		if(univ.type & PtrUserCmdFunc)
			maxArgs = (univ.u.pBuf->pCallInfo->maxArgs < 0) ? SHRT_MAX : univ.u.pBuf->pCallInfo->maxArgs;
		else if((maxArgs = univ.u.pCmdFunc->maxArgs) < 0)
			maxArgs = SHRT_MAX;
		else if((sess.opFlags & OpEval) && (univ.u.pCmdFunc->attrFlags & (CFAddlArg | CFNoArgs))) {
			if((univ.u.pCmdFunc->attrFlags & CFNoArgs) && pNode->nArg != INT_MIN)
				maxArgs = 0;
			else if((univ.u.pCmdFunc->attrFlags & CFAddlArg) && pNode->nArg == INT_MIN)
				--maxArgs;
			}
		sess.opFlags = (sess.opFlags & ~OpParens) | (needRightParen ? OpParens : 0);

		// Call the execution object if it's a command or function and CFSpecArgs is set, or evaluating and (1), it's a
		// user command or function; or (2), it's a command or function and the n argument is not zero or not just a
		// repeat count.
		if(((univ.type & (PtrSysCmdFunc)) && (univ.u.pCmdFunc->attrFlags & CFSpecArgs)) ||
		 ((sess.opFlags & OpEval) && ((univ.type & PtrUserCmdFunc) || pNode->nArg != 0 ||
		 !(univ.u.pCmdFunc->attrFlags & CFNCount)))) {
			bool cmdFuncCall = false;
#if MMDebug & (Debug_Datum | Debug_Expr)
			char filename[32], workBuf[80];
			strcpy(filename, pNode->pValue->str);
#endif
			// Clear node flags.
			pNode->flags &= EN_Concat;

			// Call command or function.
			dsetnil(pNode->pValue);			// Set default return value.
			if(univ.type & PtrUserCmdFunc) {
#if MMDebug & Debug_Expr
				fprintf(logfile, "callCFA(): calling user command or function %s(%d)...\n",
				 filename, (int) pNode->nArg);
#endif
#if MMDebug & Debug_Script
				(void) debug_execBuf(pNode->pValue, (int) pNode->nArg, univ.u.pBuf, NULL,
				 needRightParen ? ArgFirst | SRun_Parens : ArgFirst, "fcall");
#else
				(void) execBuf(pNode->pValue, (int) pNode->nArg, univ.u.pBuf, NULL,
				 needRightParen ? ArgFirst | SRun_Parens : ArgFirst);
#endif
				}
			else {
				CmdFunc *pCmdFunc = univ.u.pCmdFunc;
				if(!(sess.opFlags & OpEval) || allowEdit(pCmdFunc->attrFlags & CFEdit) == Success) {
#if MMDebug & Debug_Expr
					fprintf(logfile, "callCFA(): calling cmd/func %s(%d): minArgs %d, maxArgs %d...\n",
					 filename, (int) pNode->nArg, (int) minArgs, (int) maxArgs);
#endif
					execCmdFunc(pNode->pValue, (int) pNode->nArg, pCmdFunc, minArgs, maxArgs);
					cmdFuncCall = true;
#if MMDebug & (Debug_Datum | Debug_Expr)
#if DDebug
					sprintf(workBuf, "callCFA(): %s() returned [%d]...", filename, sess.rtn.status);
					ddump(pNode->pValue, workBuf);
#else
					fprintf(logfile, "callCFA(): %s() returned... '%s' [%d]\n", filename,
					 dtype(pNode->pValue, true), sess.rtn.status);
#endif
#endif
					}
				}
			if(sess.rtn.status != Success)
				return sess.rtn.status;
			if((sess.opFlags & OpEval) && !cmdFuncCall)
				(void) rssave();
			}
		else {
			// Not evaluating or repeat count is zero... consume arguments.
			pNode->flags &= EN_Concat;
			if(maxArgs > 0 && ((!haveSym(s_rightParen, false) && haveSym(s_any, false)) ||
			 ((sess.opFlags & OpEval) && minArgs > 0))) {
				bool first = true;
				short argCount = 0;
				for(;;) {
#if MMDebug & Debug_Expr
fprintf(logfile, "### callCFA(): consuming (minArgs %hd, maxArgs %hd): examing token '%s'...\n", minArgs, maxArgs,
 pLastParse->tok.str);
#endif
					if(first)
						first = false;
					else if(!needSym(s_comma, false))
						break;			// Error or no arguments left.
#if MMDebug & Debug_Expr
					if(ge_debug1("ge_andOr", pNode, NULL, ge_andOr) != Success)
#else
					if(ge_andOr(pNode) != Success)	// Get next expression.
#endif
						break;
					++argCount;
					}
				if(sess.rtn.status != Success)
					return sess.rtn.status;
				if((minArgs >= 0 && argCount < minArgs) || argCount > maxArgs)
					goto Wrong;
				}
			}

		// Check for extra command or function argument.
		if(maxArgs > 0 && haveSym(s_comma, false))
Wrong:
			return rsset(Failure, 0, text69, pLastParse->tok.str);
				// "Wrong number of arguments (at token '%s')"
		}
	else {
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("UNKNOWN.\n", logfile);
#endif
		// Unknown CFA.
		if(found == NULL)
			return rsset(Failure, 0, text267, pNode->pValue->str);
					// "No such command, function, or alias '%s'"
		*found = false;
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("not a FAB.\n", logfile);
#endif
		}
Retn:
	// Get right paren, if applicable.
	if(needRightParen && haveSym(s_rightParen, true))
		(void) getSym();

	return sess.rtn.status;
	}
	
// Evaluate postfix expression and return status.  Postfix expressions are any of:
//	primary
//	postfix++
//	postfix--
//	postfix(comma-expression)
//	postfix comma-expression
//	postfix[expression[,expression]]
static int ge_postfix(ExprNode *pNode) {
	bool needRightParen;
	uint oldParens = sess.opFlags & OpParens;

#if MMDebug & Debug_Expr
	if(ge_debug1("ge_primary", pNode, NULL, ge_primary) != Success)
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
		// Get postfix operator, if any.
		needRightParen = false;
		switch(pLastParse->sym) {
			case s_incr:
			case s_decr:
				if(pNode->flags & EN_HaveNArg)
					goto NoFunc;

				// Perform ++ or -- operation if evaluating.
				if((sess.opFlags & OpEval) &&
				 (!lvalue(pNode, true) || bumpVar(pNode, pLastParse->sym == s_incr, false) != Success))
					return sess.rtn.status;
				if(getSym() < NotFound)
					return sess.rtn.status;
				pNode->flags &= EN_Concat;
				break;
			case s_leftParen:
				// A function call.  Error if primary was not an identifier or was an lvalue (variable).
				if(!(pNode->flags & EN_HaveIdent))
					return rsset(Failure, 0, text4, text68, pLastParse->tok.str);
							// "%s expected (at token '%s')", "Identifier"
				if(pNode->flags & EN_HaveGNVar)
					return rsset(Failure, 0, text267, pNode->pValue->str);
							// "No such command, function, or alias '%s'"

				// Primary was an identifier and not a '$' variable.  Assume "function" type.  If white space
				// preceded the '(', the '(' is assumed to be the beginning of a primary expression and hence,
				// the first function argument "f (...), ..."; otherwise, the "f(..., ...)" form is assumed. 
				// Move past the '(' if no preceding white space and check tables.
				if(!(pNode->flags & EN_HaveWhite)) {
					if(getSym() < NotFound)			// Past '('.
						return sess.rtn.status;
					needRightParen = true;
					}

				// Call the function.
				if(callCFA(pNode, needRightParen, NULL) != Success)
					return sess.rtn.status;
				goto Clear;
			case s_leftBracket:
				// Possible array reference; e.g., "[9,[[[0, 1], 2], 3], 8][0, 2] [1][0][0][1] = 5".
				if(!(pNode->flags & EN_HaveWhite)) {
					long i1;
					Datum *pValue2;
					ExprNode node2;
					bool haveTwo = false;

					// If evaluating, check if current node is an array (otherwise, assume so).
					if(pNode->flags & EN_HaveIdent) {
						if(sess.opFlags & OpEval) {

							// Find and dereference variable.  Error if not an array.
							if(vderefn(pNode->pValue, pNode->pValue->str) != Success)
								return sess.rtn.status;
							if(!isArrayVal(pNode->pValue))
								goto Retn;
							}
						pNode->flags &= ~(EN_HaveIdent | EN_HaveGNVar);
						}
					else if((sess.opFlags & OpEval) && !isArrayVal(pNode->pValue))
						goto Retn;

					// Get first index.
					if(dnewtrack(&pValue2) != 0)
						goto LibFail;
					nodeInit(&node2, pValue2, false);
					if(getSym() < NotFound || ge_andOr(&node2) != Success)
						return sess.rtn.status;
					if(sess.opFlags & OpEval) {
						if(!isIntVal(node2.pValue))
							return sess.rtn.status;
						i1 = node2.pValue->u.intNum;
						}

					// Get second index, if present.
					if(needSym(s_comma, false)) {
						haveTwo = true;
						if(ge_andOr(&node2) != Success)
							return sess.rtn.status;
						if((sess.opFlags & OpEval) && !isIntVal(node2.pValue))
							return sess.rtn.status;
						}
					if(!needSym(s_rightBracket, true))
						return sess.rtn.status;

					// Evaluate if array slice; otherwise, save index in node for (possible use as an
					// lvalue) later.
					if(ge_deref(pNode) != Success)
						return sess.rtn.status;
					if(sess.opFlags & OpEval) {
						if(!isArrayVal(pNode->pValue))
							return sess.rtn.status;
						if(haveTwo) {
							Array *pArray;

							if((pArray = aslice(wrapPtr(pNode->pValue)->pArray, i1,
							 node2.pValue->u.intNum, true, false)) == NULL)
								goto LibFail;
							if(awWrap(pNode->pValue, pArray) != Success)
								return sess.rtn.status;
							}
						else {
							pNode->index = i1;
							pNode->flags |= EN_ArrayRef | EN_LValue;
							}
						}
					break;
					}
				// Fall through.
			default:
				// Was primary a non-variable identifier?
				if((pNode->flags & (EN_HaveIdent | EN_HaveGNVar)) == EN_HaveIdent) {
					bool found;
					if(callCFA(pNode, false, &found) != Success)
						return sess.rtn.status;
					if(found) {
Clear:
						// Clear flag(s) obviated by a function call.
						pNode->flags &= EN_Concat;
						break;
						}
					}

				// Not a function call.  Was last symbol a numeric prefix operator?
				if(pNode->flags & EN_HaveNArg)
NoFunc:
					return rsset(Failure, 0, text4, text67, pNode->pValue->str);
							// "%s expected (at token '%s')", "Function call"

				// No postfix operators left.  Bail out.
				goto Retn;
			}
		}
Retn:
	sess.opFlags = (sess.opFlags & ~OpParens) | oldParens;
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
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
		case s_decr:
		case s_incr:
		case s_minus:
		case s_plus:
		case s_not:
		case s_bitNot:
#if MMDebug & Debug_Expr
			if(getSym() < NotFound || ge_debug1("ge_unary", pNode, NULL, ge_unary) != Success)
#else
			if(getSym() < NotFound || ge_unary(pNode) != Success)
#endif
				return sess.rtn.status;
			if(sym == s_incr || sym == s_decr) {

				// Perform ++ or -- operation if evaluating.
				if((sess.opFlags & OpEval) && lvalue(pNode, true))
					(void) bumpVar(pNode, sym == s_incr, true);
				pNode->flags &= EN_Concat;
				}

			else {
				// Perform the operation.
#if MMDebug & Debug_Expr
				if(ge_debug1("ge_deref", pNode, NULL, ge_deref) != Success)
#else
				if(ge_deref(pNode) != Success)
#endif
					return sess.rtn.status;
				if(sess.opFlags & OpEval) {
					if(sym != s_not && !isIntVal(pNode->pValue))
						return sess.rtn.status;
					if(sym == s_not)
						dsetbool(toBool(pNode->pValue) == false, pNode->pValue);
					else if(sym != s_plus)
						dsetint(sym == s_minus ? -pNode->pValue->u.intNum :
						 ~pNode->pValue->u.intNum, pNode->pValue);
					}
				}
			break;
		default:
#if MMDebug & Debug_Expr
			(void) ge_debug1("ge_postfix", pNode, NULL, ge_postfix);
#else
			(void) ge_postfix(pNode);
#endif
		}
	return sess.rtn.status;
	}

// Concatenate two nodes.  First node may be string or array.  If the latter and "clone" is true, clone it first.
static int concat(ExprNode *pNode1, ExprNode *pNode2, bool clone) {

	if(pNode1->pValue->type == dat_blobRef) {
		Array *pArray1;

		if(clone && arrayClone(pNode1->pValue, pNode1->pValue, 0) != Success)
			return sess.rtn.status;
		pArray1 = wrapPtr(pNode1->pValue)->pArray;

		// If node2 is an array, expand it.
		if(pNode2->pValue->type == dat_blobRef) {
			if(agraft(pArray1, wrapPtr(pNode2->pValue)->pArray) == NULL)
				goto LibFail;
			}
		else {
			if(apush(pArray1, pNode2->pValue, false) != 0)
				goto LibFail;
			duntrack(pNode2->pValue);
			}
		}
	else if(pNode2->pValue->type != dat_nil) {
		DFab fab;

		if(dopenwith(&fab, pNode1->pValue, FabAppend) != 0 || dputd(pNode2->pValue, &fab) != 0 ||
		 dclose(&fab, Fab_String) != 0)
			goto LibFail;
		}
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Compare two arrays for equality and set *result to Boolean result.  Return status.
static int aequal(Datum *pDatum1, Datum *pDatum2, bool ignore, bool *result) {
	ArrayWrapper *pWrap1 = wrapPtr(pDatum1);
	ArrayWrapper *pWrap2 = wrapPtr(pDatum2);
	if(pWrap1->marked || pWrap2->marked)
		return rsset(Failure, RSNoFormat, text195);
			// "Endless recursion detected (array contains itself)"
	Array *pArray1 = pWrap1->pArray;
	Array *pArray2 = pWrap2->pArray;
	ArraySize size = pArray1->used;
	bool pRtnVal;
	if(size != pArray2->used)
		goto FalseRetn;
	if(size == 0)
		goto TrueRetn;
	pWrap1->marked = pWrap2->marked = true;
	Datum **ppArrayEl1 = pArray1->elements;
	Datum **ppArrayEl2 = pArray2->elements;
	Datum *pArrayEl1, *pArrayEl2;
	do {
		pArrayEl1 = *ppArrayEl1++;
		pArrayEl2 = *ppArrayEl2++;
		if(pArrayEl1->type == dat_blobRef) {
			if(pArrayEl2->type != dat_blobRef)
				goto FalseRetn;
			if(aequal(pArrayEl1, pArrayEl2, ignore, &pRtnVal) != Success)
				return sess.rtn.status;
			if(!pRtnVal)
				goto Retn;
			}
		else if(!dateq(pArrayEl1, pArrayEl2, ignore))
			goto FalseRetn;
		} while(--size > 0);
TrueRetn:
	pRtnVal = true;
	goto Retn;
FalseRetn:
	pRtnVal = false;
Retn:
	*result = pRtnVal;
	return sess.rtn.status;
	}

// Compare two arrays for equality and return Boolean result in *result.  Return status.
int arrayEQ(Datum *pDatum1, Datum *pDatum2, bool ignore, bool *result) {

	awGarbPush(pDatum1);
	awGarbPush(pDatum2);
	awClearMarks();
	return aequal(pDatum1, pDatum2, ignore, result);
	}

// Common routine to handle all of the legwork and error checking for all of the binary operators.
#if MMDebug & Debug_Expr
static int ge_binaryOp(ExprNode *pNode, const char *filename, OpInfo *pOpInfo) {
#else
static int ge_binaryOp(ExprNode *pNode, OpInfo *pOpInfo) {
#endif
	Symbol *pSym;
	Datum *pOp, *pValue2;
	ExprNode node2;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1(filename, pNode, NULL, pOpInfo->upFunc) != Success)
#else
	if(pOpInfo->upFunc(pNode) != Success)
#endif
		return sess.rtn.status;

	if(dnewtrack(&pOp) != 0 || dnewtrack(&pValue2) != 0)
		goto LibFail;

	// Loop until no operator(s) at this level remain.
	for(;;) {
		pSym = pOpInfo->pSym;
		for(;;) {
			if(*pSym == pLastParse->sym)
				break;
			if(*pSym == s_any) {

				// No operators left.  Clear EN_Concat flag if concatenate op.
				if((pOpInfo->flags & FF_Concat) && (pNode->flags & EN_Concat))
					pNode->flags &= ~EN_Concat;
				goto Retn;
				}
			++pSym;
			}

		// Found valid operator.  Dereference.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref", pNode, NULL, ge_deref) != Success)
#else
		if(ge_deref(pNode) != Success)
#endif
			goto Retn;

		// If evaluating, have '&' op, and ("concatenation" and first operand is not array or string) or ("bitwise and"
		// and (EN_Concat flag is set or first operand is not integer)), ignore it (at wrong level).  OR if evaluating,
		// have '%' op, and ("format" and first operand is not string) or ("modulus" and first operand is not integer),
		// ignore it as well.
		if(sess.opFlags & OpEval) {
			if(*pSym == s_bitAnd && (((pOpInfo->flags & FF_Concat) && pNode->pValue->type == dat_int) ||
			 ((pOpInfo->flags & FF_BitOp) && ((pNode->flags & EN_Concat) || pNode->pValue->type != dat_int))))
				goto Retn;
			if(*pSym == s_mod && (((pOpInfo->flags & FF_Format) && pNode->pValue->type == dat_int) ||
			 ((pOpInfo->flags & FF_Math) && (pNode->pValue->type != dat_int))))
				goto Retn;
			}

		// We're good.  Save operator for error reporting.
		datxfer(pOp, &pLastParse->tok);

		// Set "force concatenation" flag in second node if applicable, and call function at next higher level.
		nodeInit(&node2, pValue2, false);
		if(pOpInfo->flags & FF_Concat)
			node2.flags = EN_Concat;
#if MMDebug & Debug_Expr
		if(getSym() < NotFound || ge_debug1(filename, pNode, &node2, pOpInfo->upFunc) != Success)
#else
		if(getSym() < NotFound || pOpInfo->upFunc(&node2) != Success)
#endif
			goto Retn;

		// Dereference any lvalue.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref", pNode, &node2, ge_deref) != Success)
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
					// If FF_Concat flag is set, do concatenation; otherwise, bitwise and.
					if(!(pOpInfo->flags & FF_Concat))
						dsetint(pNode->pValue->u.intNum & node2.pValue->u.intNum, pNode->pValue);
					else if(concat(pNode, &node2, pNode->flags & EN_LValue) != Success)
						return sess.rtn.status;
					break;
				case s_bitOr:
					dsetint(pNode->pValue->u.intNum | node2.pValue->u.intNum, pNode->pValue);
					break;
				case s_bitXOr:
					dsetint(pNode->pValue->u.intNum ^ node2.pValue->u.intNum, pNode->pValue);
					break;
				case s_leftShift:
					dsetint((ulong) pNode->pValue->u.intNum << (ulong) node2.pValue->u.intNum,
					 pNode->pValue);
					break;
				case s_rightShift:
					dsetint((ulong) pNode->pValue->u.intNum >> (ulong) node2.pValue->u.intNum,
					 pNode->pValue);
					break;

				// Multiplicative and additive.
				case s_div:
					if(node2.pValue->u.intNum == 0)
						goto DivZero;
					dsetint(pNode->pValue->u.intNum / node2.pValue->u.intNum, pNode->pValue);
					break;
				case s_mod:
					// If FF_Format flag is set, do string formatting; otherwise, modulus.
					if(pOpInfo->flags & FF_Format) {
						Datum *pFormat;

						if(dnewtrack(&pFormat) != 0)
							goto LibFail;
						datxfer(pFormat, pNode->pValue);
						if(strFormat(pNode->pValue, pFormat, node2.pValue) != Success)
							return sess.rtn.status;
						}
					else {
						if(node2.pValue->u.intNum == 0)
DivZero:
						return rsset(Failure, 0, text245, pNode->pValue->u.intNum);
								// "Division by zero is undefined (%ld/0)"
						dsetint(pNode->pValue->u.intNum % node2.pValue->u.intNum, pNode->pValue);
						}
					break;
				case s_mul:
					dsetint(pNode->pValue->u.intNum * node2.pValue->u.intNum, pNode->pValue);
					break;
				case s_plus:
					dsetint(pNode->pValue->u.intNum + node2.pValue->u.intNum, pNode->pValue);
					break;
				case s_minus:
					dsetint(pNode->pValue->u.intNum - node2.pValue->u.intNum, pNode->pValue);
					break;
				case s_eq:
				case s_ne:
					{bool b = true;		// Default if two nil, false, or true operands.
					if(pNode->pValue->type != node2.pValue->type)
						b = false;
					else if(pNode->pValue->type != dat_nil && !(pNode->pValue->type & DBoolMask)) {
						if(pNode->pValue->type == dat_blobRef) {
							if(arrayEQ(pNode->pValue, node2.pValue, false, &b) != Success)
								return sess.rtn.status;
							}
						else
							b = pNode->pValue->type == dat_int ?
							 pNode->pValue->u.intNum == node2.pValue->u.intNum :
							 strcmp(pNode->pValue->str, node2.pValue->str) == 0;
						}
					dsetbool(b == (*pSym == s_eq), pNode->pValue);
					}
					break;
				case s_ge:
				case s_gt:
				case s_le:
				case s_lt:
					{long longVal1, longVal2;

					if(pNode->pValue->type == dat_int) {	// Both operands are integer.
						longVal1 = pNode->pValue->u.intNum;
						longVal2 = node2.pValue->u.intNum;
						}
					else {					// Both operands are string.
						longVal1 = strcmp(pNode->pValue->str, node2.pValue->str);
						longVal2 = 0L;
						}
					dsetbool(*pSym == s_lt ? longVal1 < longVal2 : *pSym == s_le ? longVal1 <= longVal2 :
					 *pSym == s_gt ? longVal1 > longVal2 : longVal1 >= longVal2, pNode->pValue);
					}
					break;

				// RE equality: s_regEQ, s_regNE
				default:
					if(disnull(node2.pValue))
						return rsset(Failure, 0, text187, text266);
							// "%s cannot be null", "Regular expression"

					// Compile the RE pattern.
					if(newSearchPat(node2.pValue->str, &matchRE, NULL, false) == Success) {
						if(matchRE.flags & SOpt_Plain)
							return rsset(Failure, 0, text36, OptCh_Plain, pOp->str);
								// "Invalid pattern option '%c' for '%s' operator"
						grpFree(&matchRE);
						if(compileRE(&matchRE, SCpl_ForwardRE) == Success) {
							regmatch_t group0;

							// Perform operation.
							if(regcmp(pNode->pValue, 0, &matchRE, &group0) == Success)
								dsetbool((group0.rm_so >= 0) == (*pSym == s_regEQ),
								 pNode->pValue);
							}
						}
				}
			pNode->flags &= ~EN_LValue;
			}
		}
Retn:
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Process multiplication, division and modulus operators.
static int ge_mult(ExprNode *pNode) {
	static Symbol sym_mult[] = {s_mul, s_div, s_mod, s_any};
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

// Process numeric prefix (n) operator =>.
static int ge_numPrefix(ExprNode *pNode) {

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_add", pNode, NULL, ge_add) != Success)
#else
	if(ge_add(pNode) != Success)
#endif
		return sess.rtn.status;

	// Loop until no operator at this level remains.
	while(pLastParse->sym == s_nArg) {

		// Last expression was an n argument.  Verify that it was an integer and save it in the node so that the next
		// expression (a function call) can grab it.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref", pNode, NULL, ge_deref) != Success)
#else
		if(ge_deref(pNode) != Success)
#endif
			return sess.rtn.status;
		if(sess.opFlags & OpEval) {
			if(!isIntVal(pNode->pValue))
				return sess.rtn.status;
			pNode->nArg = pNode->pValue->u.intNum;
			}
		pNode->flags |= EN_HaveNArg;

		// The next expression must be a function call (which is verified by ge_postfix()).
#if MMDebug & Debug_Expr
		if(getSym() < NotFound || ge_debug1("ge_postfix", pNode, NULL, ge_postfix) != Success)
#else
		if(getSym() < NotFound || ge_postfix(pNode) != Success)
#endif
			return sess.rtn.status;
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

// Process concatenation operator &.
static int ge_concat(ExprNode *pNode) {
	static Symbol sym_concat[] = {s_bitAnd, s_any};
	static OpInfo opInfo_concat = {ge_format, sym_concat, FF_Concat};

#if MMDebug & Debug_Expr
	return ge_binaryOp(pNode, "ge_format", &opInfo_concat);
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
static int ge_logical(ExprNode *pNode, const char *filename, int (*func)(ExprNode *pNode)) {
#else
static int ge_logical(ExprNode *pNode, int (*func)(ExprNode *pNode)) {
#endif
	bool b;
	Datum *pValue2;
	ExprNode node2;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1(filename, pNode, NULL, func) != Success)
#else
	if(func(pNode) != Success)
#endif
		return sess.rtn.status;

	if(dnewtrack(&pValue2) != 0)
		return librsset(Failure);

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
				nodeInit(&node2, pValue2, false);
#if MMDebug & Debug_Expr
				if(ge_debug1("ge_deref", pNode, NULL, ge_deref) != Success)
#else
				if(ge_deref(pNode) != Success)			// Dereference var if needed.
#endif
					goto Retn;
				if(!(sess.opFlags & OpEval)) {			// Eating arguments?
#if MMDebug & Debug_Expr
					if(ge_debug1(filename, &node2, NULL, func) != Success)
#else
					if(func(&node2) != Success)		// Yes, bon appetit.
#endif
						goto Retn;
					}
				else if(toBool(pNode->pValue) == b) {		// No, does first argument determine outcome?
					dsetbool(b, pNode->pValue);			// Yes, convert to logical...
					sess.opFlags &= ~OpEval;			// and eat second argument.
#if MMDebug & Debug_Expr
					(void) ge_debug1(filename, pNode, &node2, func);
#else
					(void) func(&node2);
#endif
					sess.opFlags |= OpEval;
					if(sess.rtn.status != Success)
						goto Retn;
					}
				else {
#if MMDebug & Debug_Expr
					if(ge_debug1(filename, pNode, &node2, func) != Success ||
					 ge_debug1("ge_deref", &node2, NULL, ge_deref) != Success)
#else
					if(func(&node2) != Success ||		// No, evaluate second argument...
					 ge_deref(&node2) != Success)		// dereference var if needed...
#endif
						goto Retn;
					dsetbool(toBool(node2.pValue), pNode->pValue); // and convert to logical.
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
	if(ge_debug1("ge_or", pNode, NULL, ge_or) != Success)
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
		if(ge_debug1("ge_deref", pNode, NULL, ge_deref) != Success)
#else
		if(ge_deref(pNode) != Success)
#endif
			return sess.rtn.status;
		if(sess.opFlags & OpEval) {
			eat = !toBool(pNode->pValue);
			if(dnewtrack(&pValue2) != 0)
				return librsset(Failure);
			}

		// Loop twice.
		for(;;) {
			if(getSym() < NotFound)		// Past '?' or ':'.
				return sess.rtn.status;

			// Don't evaluate one of the arguments if "evaluate mode" was true when we started.
			if(sess.opFlags & OpEval) {
				if(eat) {
					nodeInit(&node2, pValue2, false);
					sess.opFlags &= ~OpEval;
#if MMDebug & Debug_Expr
					(void) ge_debug1("ge_cond", &node2, NULL, ge_cond);
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
			nodeInit(pNode, pNode->pValue, false);
#if MMDebug & Debug_Expr
			if(ge_debug1("ge_cond", pNode, NULL, ge_cond) != Success ||
			 ge_debug1("ge_deref", pNode, NULL, ge_deref) != Success)
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
	if(ge_debug1("ge_cond", pNode, NULL, ge_cond) != Success)
#else
	if(ge_cond(pNode) != Success)
#endif
		return sess.rtn.status;

	// Assignment?
	if((pLastParse->sym < s_assign || pLastParse->sym > s_assignBitOr) &&
	 (pLastParse->sym != s_comma || !(pNode->flags & (EN_TopLevel | EN_ParAssign)))) {

		// No, dereference any identifier or array reference and return.
#if MMDebug & Debug_Expr
		fprintf(logfile, "### ge_assign(): Dereferencing '%s'...\n", dtype(pNode->pValue, false));
#endif
		return ge_deref(pNode);
		}

	// Have assignment operator.  Valid?
	if((pNode->flags & EN_ParAssign) && (pLastParse->sym != s_assign && pLastParse->sym != s_comma))
		return rsset(Failure, 0, text4, "'='", pLastParse->tok.str);
			// "%s expected (at token '%s')"

	// Have valid operator.  Check if node is an lvalue and build a VarDesc object from it if evaluating.
	if(sess.opFlags & OpEval) {
		if(!(pNode->flags & (EN_HaveIdent | EN_ArrayRef)))
			goto BadLValue;
		if(pNode->flags & EN_HaveIdent) {

			// Error if name matches an existing command, pseudo-command, function, or alias.
			if(execFind(pNode->pValue->str, OpQuery, PtrAny, NULL))
BadLValue:
				return rsset(Failure, 0, text4, text82, pLastParse->tok.str);
					// "%s expected (at token '%s')", "Variable name"
			if(findVar(pNode->pValue->str, &varDesc, pLastParse->sym == s_assign ||
			 pLastParse->sym == s_comma ? OpCreate : OpDelete) != Success)
				return sess.rtn.status;
			}
		else if(getArrayRef(pNode, &varDesc, pLastParse->sym == s_assign || pLastParse->sym == s_comma) != Success)
			return sess.rtn.status;
		}
	pNode->flags &= ~(EN_HaveIdent | EN_HaveGNVar | EN_HaveWhite | EN_ArrayRef);

	// Set coercion flags.
	switch(sym = pLastParse->sym) {
		case s_comma:
		case s_assign:
			break;
		case s_assignAdd:
		case s_assignSub:
		case s_assignMul:
		case s_assignDiv:
		case s_assignMod:
			f = FF_Math;
			break;
		case s_assignLeftShift:
		case s_assignRightShift:
			f = FF_Shft;
			break;
		case s_assignBitAnd:
			if((sess.opFlags & OpEval) && !isIntVar(&varDesc)) {
				f = FF_Concat;
				break;
				}
			// else fall through.
		default:
			f = FF_BitOp;
		}

	// If evaluating, save assign op (for error reporting).
	if(sess.opFlags & OpEval) {
		if(dnewtrack(&pOp) != 0)
			goto LibFail;
		datxfer(pOp, &pLastParse->tok);
		}

	// Move past operator and prepare to get value expression.
	if(getSym() < NotFound)
		return sess.rtn.status;
	if(dnewtrack(&pValue2) != 0)
		goto LibFail;
	nodeInit(&node2, pValue2, sym == s_comma);

	// If doing parallel assignment, set array index in node2.nArg for next recursive call.
	if(sym == s_comma) {
		node2.flags = EN_ParAssign;
		if(pNode->flags & EN_ParAssign)
			node2.nArg = pNode->nArg + 1;	// Not first comma.  Bump index for next instance.
		else {						// First comma.  Initalize both nodes.
			pNode->nArg = 0;			// Current instance (lvalue) gets first array element...
			node2.nArg = 1;			// and next instance gets second.
			pNode->flags |= EN_ParAssign;
			}
		}

	// Get value.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_assign", pNode, &node2, ge_assign) != Success ||
	 ge_debug1("ge_deref", pNode, &node2, ge_deref) != Success)
#else
	if(ge_assign(&node2) != Success || ge_deref(&node2) != Success)
#endif
		return sess.rtn.status;

	// If evaluating...
	if(sess.opFlags & OpEval) {

		// Get current variable value or array element into pNode if not straight assignment, and coerce operands into a
		// compatible type.
		if(sym != s_assign && sym != s_comma)
			if(vderefv(pNode->pValue, &varDesc) != Success || forceFit(pNode, &node2, f, pOp->str) != Success)
				return sess.rtn.status;

		// Do operation and put result into pNode.  pNode contains left side and node2 contains right.
		switch(sym) {
			case s_assign:
				// Value must be an array if doing parallel assignment.
				if(pNode->flags & EN_ParAssign) {
					if(!isArrayVal(node2.pValue))
						return sess.rtn.status;
					pNode->flags &= ~EN_ParAssign;
					goto PAssign;
					}
#if DDebug
	ddump(node2.pValue, "ge_assign(): Transferring from src object...");
	ddump(pNode->pValue, "ge_assign(): to dest object...");
#endif
				datxfer(pNode->pValue, node2.pValue);
#if DDebug
	ddump(pNode->pValue, "ge_assign(): Result (dest)...");
#endif
				break;
			case s_comma:
PAssign:
				// Set lvalue to array element, or nil if element does not exist.  Return whole array in node.
				{Array *pArray = wrapPtr(node2.pValue)->pArray;
				if(pNode->nArg >= pArray->used) {
					Datum d;
					dinit(&d);
					(void) putVar(&d, &varDesc);
					}
				else
					(void) putVar(pArray->elements[pNode->nArg], &varDesc);
				}
				datxfer(pNode->pValue, node2.pValue);
				goto Retn;
			case s_assignAdd:
				pNode->pValue->u.intNum += node2.pValue->u.intNum;
				break;
			case s_assignSub:
				pNode->pValue->u.intNum -= node2.pValue->u.intNum;
				break;
			case s_assignMul:
				pNode->pValue->u.intNum *= node2.pValue->u.intNum;
				break;
			case s_assignDiv:
				if(node2.pValue->u.intNum == 0L)
					goto DivZero;
				pNode->pValue->u.intNum /= node2.pValue->u.intNum;
				break;
			case s_assignMod:
				if(node2.pValue->u.intNum == 0L)
DivZero:
					return rsset(Failure, 0, text245, pNode->pValue->u.intNum);
						// "Division by zero is undefined (%ld/0)"
				pNode->pValue->u.intNum %= node2.pValue->u.intNum;
				break;
			case s_assignLeftShift:
				pNode->pValue->u.intNum <<= node2.pValue->u.intNum;
				break;
			case s_assignRightShift:
				pNode->pValue->u.intNum >>= (ulong) node2.pValue->u.intNum;
				break;
			case s_assignBitAnd:
				if(f & FF_BitOp)
					pNode->pValue->u.intNum &= node2.pValue->u.intNum;
				else {
					if(concat(pNode, &node2, false) != Success)
						return sess.rtn.status;

					// If left node is an array, the lvalue was modified directly by concat(), so skip
					// (redundant) call to putVar().
					if(pNode->pValue->type == dat_blobRef)
						goto Retn;
					}
				break;
			case s_assignBitXOr:
				pNode->pValue->u.intNum ^= node2.pValue->u.intNum;
				break;
			default:	// s_assignBitOr
				pNode->pValue->u.intNum |= node2.pValue->u.intNum;
			}
		(void) putVar(pNode->pValue, &varDesc);
		}
Retn:
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Evaluate low precedence logical not expression "not".
static int ge_not(ExprNode *pNode) {

	if(pLastParse->sym == kw_not) {
#if MMDebug & Debug_Expr
		if(getSym() < NotFound || ge_debug1("ge_not", pNode, NULL, ge_not) != Success)
#else
		if(getSym() < NotFound || ge_not(pNode) != Success)
#endif
			return sess.rtn.status;

		// Perform operation.
		if(sess.opFlags & OpEval)
			dsetbool(!toBool(pNode->pValue), pNode->pValue);
		}
	else
#if MMDebug & Debug_Expr
		(void) ge_debug1("ge_assign", pNode, NULL, ge_assign);
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
	if(ge_debug1("ge_not", pNode, NULL, ge_not) != Success)
#else
	if(ge_not(pNode) != Success)
#endif
		return sess.rtn.status;

	if(dnewtrack(&pValue2) != 0)
		return librsset(Failure);
	eval = (sess.opFlags & OpEval) != 0;

	// Loop until no operator(s) at this level remain.  If we weren't evaluating initially (eval is false), then all ops
	// evaluate to false.
	for(;;) {
		priorTruth = toBool(pNode->pValue);
		switch(pLastParse->sym) {			// true or false or true
			case kw_and:			// false and true and false
				curTruth = false;	// true or false and EVAL
				goto AndOr;		// false and true or EVAL
			case kw_or:
				curTruth = true;
AndOr:
				if(getSym() < NotFound)				// Past 'and' or 'or'.
					goto Retn;
				nodeInit(&node2, pValue2, false);
				if(!(sess.opFlags & OpEval)) {			// Eating arguments?
					if(eval && curTruth != priorTruth) {	// Yes, stop the gluttony?
						sess.opFlags |= OpEval;		// Yes, enough already.
						goto Eval;
						}
#if MMDebug & Debug_Expr
					if(ge_debug1("ge_not", pNode, &node2, ge_not) != Success)
#else
					if(ge_not(&node2) != Success)		// No, bon appetit.
#endif
						goto Retn;
					}
				else if(priorTruth == curTruth) {		// No, does prior argument determine outcome?
					dsetbool(curTruth, pNode->pValue);		// Yes, convert to logical...
					sess.opFlags &= ~OpEval;			// and eat next argument.
#if MMDebug & Debug_Expr
					(void) ge_debug1("ge_not", pNode, &node2, ge_not);
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
					if(ge_debug1("ge_not", pNode, &node2, ge_not) != Success)
#else
					if(ge_not(&node2) != Success)
#endif
						goto Retn;
					dsetbool(toBool(node2.pValue), pNode->pValue);
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
		case dat_soloStr:
			str = "string";
			break;
		default:
			str = "array";
		}
	return str;
	}

// Validate a value per flags.  Return status.
int validateArg(Datum *pDatum, uint argFlags) {

	if((argFlags & (ArgInt1 | ArgMay)) == ArgInt1)					// 'int' flag set without "may be"?
		(void) isIntVal(pDatum);						// Yes, must be integer.
	else if((argFlags & (ArgArray1 | ArgNIS1 | ArgMay)) == ArgArray1)		// No, 'array' set without other types?
		(void) isArrayVal(pDatum);						// Yes, must be array.
	else if(!(argFlags & (ArgNil1 | ArgBool1 | ArgInt1 | ArgArray1 | ArgNIS1))) {	// No, non-string flags not set?
		if(isStrVal(pDatum) && (argFlags & ArgNotNull1) && disnull(pDatum))	// Yes, must be string.  Check if null
			goto NotNull;							// string is an error.
		}
	else if((!(argFlags & ArgArray1) && pDatum->type == dat_blobRef) ||	// No required types.  Check if found type
	 (!(argFlags & ArgBool1) && (pDatum->type & DBoolMask)) ||		// is allowed.
	 (!(argFlags & ArgNIS1) && (
	 (!(argFlags & ArgNil1) && pDatum->type == dat_nil) ||
	 (!(argFlags & ArgInt1) && pDatum->type == dat_int))))
		return rsset(Failure, 0, text329, dtype(pDatum, false));
				// "Unexpected %s argument"
	else if((pDatum->type & DStrMask) && (argFlags & ArgNotNull1) &&	// Null string error?
	 disnull(pDatum))
NotNull:
		return rsset(Failure, 0, text187, text285);
			// "%s cannot be null", "Call argument"
	return sess.rtn.status;							// No, all is well.
	}

// Get a script line argument, given pointer to result and argument flags.  Return an error if argument does not conform to
// validation flags.
int funcArg(Datum *pRtnVal, uint argFlags) {
	ExprNode node;

#if MMDebug & Debug_Expr
	fprintf(logfile, "funcArg(...,%.8X): BEGIN at token '%s', remainder '%s'...\n", argFlags, pLastParse->tok.str,
	 pLastParse->src);
#endif
	if(!(argFlags & ArgFirst) && !needSym(s_comma, true))
		return sess.rtn.status;
	nodeInit(&node, pRtnVal, false);
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_andOr", &node, NULL, ge_andOr) != Success)
#else
	if(ge_andOr(&node) != Success)
#endif
		return sess.rtn.status;

	// If evaluating, validate value and expand pathname, if applicable.
	if((sess.opFlags & OpEval) && validateArg(pRtnVal, argFlags) == Success && (argFlags & ArgPath) &&
	 (pRtnVal->type & DStrMask))
		expandPath(pRtnVal, NULL);
	return sess.rtn.status;
	}


// Simulate a series of calls to funcArg() routine by getting next "argument" from a comma-delimited string or an array, given
// indirect pointer to result, indirect pointer to input Datum object, work Datum object (used when input is a string), indirect
// string pointer (for managing a string input), indirect array pointer and indirect array length (for managing an array input).
// *ppInput is assumed to point to either a string or array Datum object, and is set to NULL after the first call to this
// routine.  Routine is called repeatedly to obtain next argument until NotFound is returned or a non-Success status.  *ppRtnVal
// is set to either "work" or the next array element for each extracted argument; however, if argument does not conform to
// validation flags, an error is returned.  All arguments are assumed to be optional, including the first.
int nextArg(Datum **ppRtnVal, Datum **ppInput, Datum *pWork, char **keywordList, Datum ***pppArrayEl, ArraySize *pElCount) {

	for(;;) {
		// First call?
		if(*ppInput != NULL) {
			if((*ppInput)->type == dat_blobRef) {

				// Set up for array processing.
				Array *pArray = wrapPtr(*ppInput)->pArray;
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
				return librsset(Failure);
			*ppRtnVal = pWork;
			break;
			}
		// Onward...
		}
	return validateArg(*ppRtnVal, ArgNotNull1);
	}
