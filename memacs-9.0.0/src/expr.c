// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// expr.c	Core expresion evaluation routines for MightEMacs.

#include "os.h"
#include <errno.h>
#include "std.h"
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
//	This is accomplished via the awrap() function.  The ArrayWrapper object is also pushed onto a "garbage collection" list
//	pointed to by "aryGarbp".
//   2. The ArrayWrapper object contains a Boolean member, aw_mark, which is used to determine which arrays to keep and which
//	to free (later) and also to prevent endless recursion when an array contains itself.
//   3. Freeing of array space is not attempted at all during expression evaluation, simply because there is no "safe" time at
//	which to do so.  It is not done until just before control is returned to the user, in editloop(), via a call to the
//	agarbfree() function.
//   4. The job of agarbfree() is to scan the aryGarbp list, freeing arrays which are not associated with a global variable.
//	To accomplish this, it first checks the list.  If it is empty, it does nothing; otherwise, it (a), scans the global
//	variable list and adds any arrays found to the garbage list via the agarbpush() function (which also adds nested
//	arrays) so that all existing arrays will be examined; (b), scans the garbage list and sets aw_mark to false in each
//	array; (c), scans the global variable list again and sets aw_mark to true in any arrays and nested arrays found via the
//	akeep() function, which also uses aw_mark during recursion to prevent an endless loop if an array contains itself; and
//	(d), scans the garbage list again and frees any arrays which have aw_mark set to false.
//   5. When agarbfree() is done, the only arrays left in the list will be those contained in global variables.  This will also
//	include nested arrays.  "aryGarbp" is set to NULL at this point so that the list will not be scanned every time the
//	user presses a key.
//   6. Lastly, to deal with the arrays contained in global variables, the agarbpush() function is called whenever a new value
//	is assigned to one of those variables.  It puts the old array and any arrays it contains back onto the garbage list.
//
// Given the fact that arrays are passed around by reference, it is the responsibility of the user to clone arrays where needed
// (via the "clone" function).  However, there are a couple places where it is done automatically:
//	* If the initializer of the "array" function is itself an array, it is cloned for each element of the array created.
//	* If the first item in a concatenation expression is an array and is an lvalue (a variable or array element), it is
//	  cloned so that the value of the variable or array element is not changed.

// Binary operator info.
typedef struct {
	int (*xfunc)(ENode *np);	// Function at next higher level.
	enum e_sym *symp;		// Valid operator token(s).
	ushort flags;			// Kind of operation.
	} OpInfo;

// forcefit() types.
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

// forcefit() table for nil, Boolean (true or false), int, string, and array coersion combinations.
static struct ffinfo {
	ushort legal;			// Legal operations (FF_XXX flags)
	ushort str_op;			// Operations which cause call to tostr().  High bits determine left and/or right side.
	} fftab[][5] = {
		{ /* nil */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* int */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0}},

		{ /* bool */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* int */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0}},

		{ /* int */
/* nil */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* bool */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* int */	{~(FF_Concat | FF_Format | FF_REQNE),0},
/* string */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* array */	{FF_Assign | FF_EQNE | FF_LogAndOr | FF_Cond,0}},

		{ /* string */
/* nil */	{FF_Assign | FF_Concat | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond,FF_Concat | Str_Right},
/* bool */	{FF_Assign | FF_Concat | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond,FF_Concat | Str_Right},
/* int */	{FF_Assign | FF_Concat | FF_Format | FF_EQNE | FF_LogAndOr | FF_Cond,FF_Concat | Str_Right},
/* string */	{FF_Assign | FF_Concat | FF_Format | FF_Rel | FF_REQNE | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* array */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond,FF_Concat | Str_Right}},

		{ /* array */
/* nil */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* bool */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* int */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* string */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond,0},
/* array */	{FF_Assign | FF_Concat | FF_EQNE | FF_LogAndOr | FF_Cond,0}}
		};

#if MMDebug & Debug_Expr
static int indent = -1;
#endif

#if MMDebug & Debug_Array
#if 0
// Write array-usage report to log file.
static int ausage(bool first) {

	if(aryGarbp != NULL) {
		Array *aryp;
		Datum **elpp,**elppz;
		ArraySize i;
		ArrayWrapper *awp = aryGarbp;

		fprintf(logfile,"Array List: *%s*\n\n",first ? "BEFORE" : "AFTER");
		do {
			aryp = awp->aw_aryp;
			fprintf(logfile,"\tFound wrapper %.8X -> array %.8X (keep %d):\n",(uint) awp,(uint) aryp,awp->aw_mark);
			elppz = (elpp = aryp->a_elpp) + aryp->a_used;
			i = 0;
			while(elpp < elppz) {
				fprintf(logfile,"\t\t%2d: ",i++);
				if((*elpp)->d_type == dat_blobRef) {
					aryp = awptr(*elpp)->aw_aryp;
					fprintf(logfile,"ARRAY [%.8X], size %d\n",(uint) aryp,aryp->a_used);
					}
				else
					fprintf(logfile,((*elpp)->d_type & DStrMask) ? "\"%s\"\n" : "%s\n",dtos(*elpp,true));
				++elpp;
				}
			} while((awp = awp->aw_nextp) != NULL);
		if(first)
			fputc('\n',logfile);
		fflush(logfile);
		}
	return rc.status;
	}
#else
// Write array contents to a string-fab object in abbreviated form.  Return status.
static int ausage1(ArrayWrapper *awp,DStrFab *sfp) {
	Datum **elpp = awp->aw_aryp->a_elpp;
	Datum **elppz = elpp + awp->aw_aryp->a_used;
	bool first = true;

	if(dputc('[',sfp) != 0)
		return librcset(Failure);
	while(elpp < elppz) {
		if(first)
			first = false;
		else if(dputc(',',sfp) != 0)
			return librcset(Failure);
		if((*elpp)->d_type == dat_blobRef) {
			Array *aryp = awptr(*elpp)->aw_aryp;
			if(dputf(sfp,"[%.8X:%.8X:%d]",(uint) awptr(*elpp),(uint) aryp,aryp->a_used) != 0)
				return librcset(Failure);
			}
		else if(dtosf(sfp,*elpp,NULL,CvtExpr) != Success)
			return rc.status;
		++elpp;
		}
	if(dputc(']',sfp) != 0)
		(void) librcset(Failure);

	return rc.status;
	}

// Write array-usage report to a buffer.
static int ausage(DStrFab *sfp,int stage) {

	if(stage <= 0) {
		UVar *uvp;
		ArrayWrapper *awp = aryGarbp;

		// Dump array info.
		if((stage < 0 && dopentrk(sfp) != 0) || dputf(sfp,"Array List: *%s*\n\n",stage < 0 ? "BEFORE" : "AFTER") != 0)
			return librcset(Failure);
		do {
			if(dputf(sfp,"%.8X   %.8X   %c",(uint) awp,(uint) awp->aw_aryp,awp->aw_mark ? '*' : ' ') != 0)
				return librcset(Failure);
#if 0
			dsetblobref((void *) awp,sizeof(ArrayWrapper),&d);
			if(atosfclr(sfp,&d,NULL,CvtExpr | CvtForceArray) != Success)
				return rc.status;
#else
			if(ausage1(awp,sfp) != Success)
				return rc.status;
#endif
			if(dputc('\n',sfp) != 0)
				return librcset(Failure);
			} while((awp = awp->aw_nextp) != NULL);

		if(dputs("\nGLOBAL VARIABLES\n\n",sfp) != 0)
			return librcset(Failure);
		for(uvp = gvarsheadp; uvp != NULL; uvp = uvp->uv_nextp) {
			if(uvp->uv_datp->d_type == dat_blobRef) {
				awp = awptr(uvp->uv_datp);
				if(dputf(sfp,"%-18s %.8X   %.8X   %c",uvp->uv_name,(uint) awp,(uint) awp->aw_aryp,
				 awp->aw_mark ? '*' : ' ') != 0)
					return librcset(Failure);
				if(ausage1(awptr(uvp->uv_datp),sfp) != Success)
					return rc.status;
				if(dputc('\n',sfp) != 0)
					return librcset(Failure);
				}
			}
		}
	else {
		Buffer *bufp;
		bool created;

		if(dclose(sfp,sf_string) != 0)
			return librcset(Failure);

		// Create debug buffer, clear it, and append results.
		if(bfind("Debug",CRBCreate,0,&bufp,&created) != Success || (!created && bclear(bufp,CLBIgnChgd) != Success))
			return rc.status;
		if(bappend(bufp,sfp->sf_datp->d_str) == Success) {
			Datum d;

			dinit(&d);
			(void) render(&d,2,bufp,RendReset);
			dclear(&d);
			}
		}
	return rc.status;
	}
#endif
#endif

// Scan the wrapper list and clear all the "marked" flags.
void aclrmark(void) {
	ArrayWrapper *awp = aryGarbp;
	while(awp != NULL) {
		awp->aw_mark = false;
		awp = awp->aw_nextp;
		}
	}

// Push an array and all its decendent arrays back on to the garbage list if they are not already there.
static void agarbpushall(Datum *datp) {
	ArrayWrapper *agp = aryGarbp;
	ArrayWrapper *awp = awptr(datp);

	// Check if array is already on the list.
	while(agp != NULL) {
		if(agp == awp) {
			if(awp->aw_mark)
				return;
			goto Mark;
			}
		agp = agp->aw_nextp;
		}

	// Not found.  Mark it and add it.
	awp->aw_nextp = aryGarbp;
	aryGarbp = awp;
Mark:
	awp->aw_mark = true;

	// Scan element list and add those, too.
	Datum **elpp = awp->aw_aryp->a_elpp;
	Datum **elppz = elpp + awp->aw_aryp->a_used;

	while(elpp < elppz) {
		if((*elpp)->d_type == dat_blobRef)
			agarbpushall(*elpp);
		++elpp;
		}
	}

// Clear marks in wrapper list and call agarbpushall().
void agarbpush(Datum *datp) {

	aclrmark();
	agarbpushall(datp);
	}

// Mark given array and all nested arrays (if any) as "keepers".
static void akeep(Datum *datp) {
	ArrayWrapper *awp = awptr(datp);
	if(!awp->aw_mark) {
		Datum **elpp,**elppz;
		Array *aryp = awp->aw_aryp;
		awp->aw_mark = true;
		elppz = (elpp = aryp->a_elpp) + aryp->a_used;
		while(elpp < elppz) {
			if((*elpp)->d_type == dat_blobRef)
				akeep(*elpp);
			++elpp;
			}
		}
	}

// Free all heap space for unused arrays.
int agarbfree(void) {

	if(aryGarbp != NULL) {
		ArrayWrapper *awp;
		UVar *uvp;
#if MMDebug & Debug_Array
		DStrFab sf;
#endif
		// Scan global variable table and add any arrays found to wrapper list so that all known arrays are examined in
		// the following steps.  This is necessary because a global array currently not on the list may have had an
		// array added to it (for example) that would then not be marked as a "keeper" and would be freed in error.
		for(uvp = gvarsheadp; uvp != NULL; uvp = uvp->uv_nextp)
			if(uvp->uv_datp->d_type == dat_blobRef)
				agarbpush(uvp->uv_datp);

		// Clear all "marked" flags in wrapper list.
		aclrmark();

#if MMDebug & Debug_Array
		if(ausage(&sf,-1) != Success)
			return rc.status;
#endif
		// Scan global variable table again and mark any arrays found as "keepers".
		for(uvp = gvarsheadp; uvp != NULL; uvp = uvp->uv_nextp)
			if(uvp->uv_datp->d_type == dat_blobRef)
				akeep(uvp->uv_datp);
#if MMDebug & Debug_Array
		if(ausage(&sf,0) != Success)
			return rc.status;
#endif
		// Scan wrapper list, delete any non-keeper arrays, and free wrapper record.
		do {
			awp = aryGarbp;
			aryGarbp = awp->aw_nextp;
			if(!awp->aw_mark) {

				// Non-keeper array found.  Free it and its wrapper record.
#if MMDebug & Debug_Array
				if(dputf(&sf,"Freeing array %.8X...\n",(uint) awp->aw_aryp) != 0)
					return librcset(Failure);
#endif
				afree(awp->aw_aryp);
				free((void *) awp);
				}
			} while(aryGarbp != NULL);
#if MMDebug & Debug_Array
		(void) ausage(&sf,1);
#endif
		}

	return rc.status;
	}

// Create a wrapper record for an array, save pointer to it in given Datum object, and add it to the garbage list.  Return
// status.
int awrap(Datum *rp,Array *aryp) {
	ArrayWrapper *awp;

	if((awp = (ArrayWrapper *) malloc(sizeof(ArrayWrapper))) == NULL)	// Create wrapper object...
		return rcset(Panic,0,text94,"awrap");
			// "%s(): Out of memory!"
	awp->aw_aryp = aryp;							// initialize it...
	awp->aw_nextp = aryGarbp;						// add it to garbage list...
	aryGarbp = awp;
	dsetblobref((void *) awp,sizeof(ArrayWrapper),rp);			// and set in Datum object.
	return rc.status;
	}

// Pop datGarbp to given pointer, releasing heap space for Datum objects and arrays, if applicable.
void garbpop(Datum *datp) {
	Datum *datp1;

	while(datGarbp != datp) {
		datp1 = datGarbp;
		datGarbp = datGarbp->d_nextp;
		ddelete(datp1);
		}
	}

// Initialize an expression node with given Datum object.
void nodeinit(ENode *np,Datum *rp,bool toplev) {

	dsetnull(rp);
	np->en_rp = rp;
	np->en_flags = toplev ? EN_TopLevel : 0;
	np->en_narg = INT_MIN;
	}

// Return true if b is true; otherwise, set given error message and return false.
static bool isval(bool b,char *msg) {

	if(b)
		return true;
	(void) rcset(Failure,RCNoFormat,msg);
	return false;
	}

// Return true if a datum object is an integer; otherwise, set an error and return false.
bool intval(Datum *datp) {

	return isval(datp->d_type == dat_int,text166);
			// "Integer expected"
	}

// Return true if a datum object is a string; otherwise, set an error and return false.
bool strval(Datum *datp) {

	return isval(datp->d_type & DStrMask,text171);
			// "String expected"
	}

// Return true if a datum object is an array; otherwise, set an error and return false.
bool aryval(Datum *datp) {

	return isval(datp->d_type == dat_blobRef,text371);
			// "Array expected"
	}

// Return true if node value is an lvalue; otherwise, set an error if "required" is true and return false.
static bool lvalue(ENode *np,bool required) {

	if((np->en_flags & EN_HaveGNVar) || ((np->en_flags & EN_HaveIdent) && uvarfind(np->en_rp->d_str) != NULL) ||
	 (np->en_flags & EN_ArrayRef))
		return true;
	if(required) {
		if(np->en_flags & EN_HaveIdent)
			(void) rcset(Failure,0,text52,np->en_rp->d_str);
					// "No such variable '%s'"
		else
			(void) rcset(Failure,0,text4,text82,last->p_tok.d_str);
					// "%s expected (at token '%s')","Variable name"
		}
	return false;
	}

#if MMDebug & Debug_Expr
// Write integer or string value to log file, given datum object.
void dumpval(Datum *datp) {
	char *str;

	fprintf(logfile,"(%.8X %hu) ",(uint) datp,datp->d_type);
	switch(datp->d_type) {
		case dat_nil:
			str = viz_nil;
			break;
		case dat_false:
			str = viz_false;
		case dat_true:
			str = viz_true;
			break;
		case dat_int:
			fprintf(logfile,"%ld",datp->u.d_int);
			return;
		case dat_miniStr:
		case dat_soloStr:
			fprintf(logfile,"\"%s\"",datp->d_str);
			return;
		default:
			str = "[array]";
		}

	fputs(str,logfile);
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
	 last->p_tok.d_str,last->p_sym);
	ge_dumpval(np1);
	fprintf(logfile,", flags %.8X",np1->en_flags);
	if(np2 != NULL) {
		fputs(", val2 ",logfile);
		ge_dumpval(np2);
		fprintf(logfile,", flags %.8X",np2->en_flags);
		}
	fprintf(logfile,", EvalMode %x",opflags & OpEval);
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

// Check if given string is a command, pseudo-command, function, alias, buffer, or macro, according to selector masks.  If not
// found, return -1; otherwise, if wrong (alias) type, return 1; otherwise, set "cfabp" (if not NULL) to result and return 0.
int cfabsearch(char *str,CFABPtr *cfabp,uint selector) {
	CFABPtr cfab;

	// Figure out what the string is.
	if((selector & (PtrCmdType | PtrFunc)) &&
	 (cfab.u.p_cfp = ffind(str)) != NULL) {				// Is it a command or function?
		ushort foundtype = (cfab.u.p_cfp->cf_aflags & CFFunc) ? PtrFunc :
		 (cfab.u.p_cfp->cf_aflags & CFHidden) ? PtrPseudo : PtrCmd;
		if(!(selector & foundtype))				// Yep, correct type?
			return -1;					// No.
		cfab.p_type = foundtype;				// Yes, set it.
		}
	else if((selector & PtrAlias) &&
	 afind(str,OpQuery,NULL,&cfab.u.p_aliasp)) {			// Not a function... is it an alias?
		ushort foundtype = cfab.u.p_aliasp->a_type;
		if(!(selector & foundtype))				// Yep, correct type?
			return 1;					// No.
		cfab.p_type = cfab.u.p_aliasp->a_type;			// Yes, set it.
		}
	else if((selector & PtrBuf) &&					// No, is it a buffer?
	 (cfab.u.p_bufp = bsrch(str,NULL)) != NULL)			// Yep.
		cfab.p_type = PtrBuf;
	else if(selector & PtrMacro) {					// No, is it a macro?
		char mac[NBufName + 1];

		sprintf(mac,MacFormat,NBufName - 1,str);
		if((cfab.u.p_bufp = bsrch(mac,NULL)) != NULL)		// Yep.
			cfab.p_type = PtrMacro;
		else
			return -1;					// No, it's a bust.
		}
	else
		return -1;

	// Found it.
	if(cfabp != NULL)
		*cfabp = cfab;
	return 0;
	}

// Dereference an lvalue (variable name or array element reference) in np if present and evaluating.  Return status.
static int ge_deref(ENode *np) {

	if(!(opflags & OpEval))

		// Not evaluating.  Just clear flags.
		np->en_flags &= ~(EN_TopLevel | EN_ArrayRef | EN_HaveIdent | EN_HaveGNVar | EN_HaveWhite);
	else {
		// Evaluating.  Dereference and clear flags.
		if(np->en_flags & (EN_HaveGNVar | EN_HaveIdent)) {
			(void) vderefn(np->en_rp,np->en_rp->d_str);
			np->en_flags &= ~(EN_TopLevel | EN_HaveIdent | EN_HaveGNVar | EN_HaveWhite);
			}
		else if(np->en_flags & EN_ArrayRef) {
			VDesc vd;

			if(aryget(np,&vd,false) == Success)
				(void) vderefv(np->en_rp,&vd);
			np->en_flags &= ~(EN_TopLevel | EN_ArrayRef);
			}
		}

	return rc.status;
	}

// Coerce datum objects passed into compatable types for given operation flag(s) and return status.  If illegal fit, return
// error.  "kind" contains operation flag (FF_XXX) and "op" is operator token (for error reporting).
static int forcefit(ENode *np1,ENode *np2,ushort kind,char *op) {

	// Get info from table.
	struct ffinfo *info = fftab[(np1->en_rp->d_type == dat_nil) ? 0 : (np1->en_rp->d_type & DBoolMask) ? 1 :
	 np1->en_rp->d_type == dat_int ? 2 : np1->en_rp->d_type == dat_blobRef ? 4 : 3] +
	 ((np2->en_rp->d_type == dat_nil) ? 0 : (np2->en_rp->d_type & DBoolMask) ? 1 : np2->en_rp->d_type == dat_int ? 2 :
	 np2->en_rp->d_type == dat_blobRef ? 4 : 3);

	// Valid operand types?
	if(!(info->legal & kind))
#if MMDebug & Debug_Expr
		return rcset(Failure,0,"Wrong operand type for '%s' (legal %.4hx, kind %.4hx)",op,info->legal,kind);
#else
		return rcset(Failure,0,text191,op);
				// "Wrong type of operand for '%s'"
#endif
	// Coerce one operand to string if needed.
	if(info->str_op & kind) {
#if MMDebug & Debug_Expr
		ENode *np;
		char *str;
		if(info->str_op & Str_Left) {
			np = np1;
			str = "left";
			}
		else {
			np = np2;
			str = "right";
			}
		fprintf(logfile,"### forcefit(): coercing %s operand...\n",str);
		if(tostr(np->en_rp) == Success &&
		 (info->str_op & (Str_Left | Str_Right)) == (Str_Left | Str_Right)) {
			fputs("### forcefit(): coercing right operand...\n",logfile);
			(void) tostr(np2->en_rp);
			}
#else
		if(tostr((info->str_op & Str_Left) ? np1->en_rp : np2->en_rp) == Success &&
		 (info->str_op & (Str_Left | Str_Right)) == (Str_Left | Str_Right))
			(void) tostr(np2->en_rp);
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
//	[and-or-expression,...]
static int ge_primary(ENode *np) {
	bool b;

	switch(last->p_sym) {
		case s_nlit:
			if(opflags & OpEval) {
				long lval;

				if(asc_long(last->p_tok.d_str,&lval,false) != Success)
					return rc.status;
				dsetint(lval,np->en_rp);
				}
			goto NextSym;
		case s_slit:
			// String literal.
			(void) evalslit(np->en_rp,last->p_tok.d_str);
			break;
		case kw_true:					// Convert to true value.
			b = true;
			goto IsBool;
		case kw_false:					// Convert to false value.
			b = false;
IsBool:
			if(opflags & OpEval)
				dsetbool(b,np->en_rp);
			goto NextSym;
		case kw_nil:					// Convert to nil value.
			if(opflags & OpEval)
				dsetnil(np->en_rp);
			goto NextSym;
		case kw_defn:					// Convert to defn value.
			if(opflags & OpEval)
				dsetint(val_defn,np->en_rp);
			goto NextSym;
		case s_gvar:
		case s_nvar:
			np->en_flags |= EN_HaveGNVar;
			// Fall through.
		case s_ident:
		case s_identq:
			np->en_flags |= EN_HaveIdent;

			// Save identifier name in np.
			if(dsetstr(last->p_tok.d_str,np->en_rp) != 0)
				return librcset(Failure);

			// Set "white-space-after-identifier" flag for caller.
			if(havewhite())
				np->en_flags |= EN_HaveWhite;
			goto NextSym;
		case s_lparen:
			// Parenthesized expression.
			{ushort oldflag = np->en_flags & EN_TopLevel;
			np->en_flags |= EN_TopLevel;
#if MMDebug & Debug_Expr
			if(getsym() < NotFound || ge_debug1("ge_andor",np,NULL,ge_andor) != Success || !havesym(s_rparen,true))
#else
			if(getsym() < NotFound || ge_andor(np) != Success || !havesym(s_rparen,true))
#endif
				return rc.status;
			np->en_flags = (np->en_flags & ~EN_TopLevel) | oldflag;
			}
			goto NextSym;
		case s_lbrkt:
			// Bracketed expression list.  Create array.
			{Array *aryp;
			Datum aw;
			bool first = true;

			if(opflags & OpEval) {
				dinit(&aw);
				if((aryp = anew(0,NULL)) == NULL)
					return librcset(Failure);
				if(awrap(&aw,aryp) != Success)
					return rc.status;
				}
			if(getsym() < NotFound)
				return rc.status;

			// Get element list, if any.
			np->en_flags &= ~EN_TopLevel;
			for(;;) {
				if(havesym(s_rbrkt,false))
					break;
				if(!first && !needsym(s_comma,true))
					return rc.status;

				// Get next expression.
				if(ge_andor(np) != Success)
					return rc.status;
				if((opflags & OpEval) && apush(aryp,np->en_rp) != 0)
					return librcset(Failure);

				// Reset node.
				nodeinit(np,np->en_rp,false);
				first = false;
				}
			if(opflags & OpEval)
				datxfer(np->en_rp,&aw);
			}
NextSym:
			(void) getsym();
			break;
		default:
			if(last->p_sym == s_nil)
				(void) rcset(Failure,RCNoFormat,text172);
						// "Token expected"
			else
				(void) rcset(Failure,0,text289,last->p_tok.d_str);
						// "Unexpected token '%s'"
		}

	return rc.status;
	}

// Handle a function (command, alias, function, or macro) call.
static int fcall(ENode *np,bool needrparen,bool *foundp) {
	CFABPtr cfab;

	// Is identifier a command, function, alias, or macro?
#if MMDebug & (Debug_Datum | Debug_Expr)
	fprintf(logfile,"fcall(): identifying '%s'... ",np->en_rp->d_str);
#endif
	if(cfabsearch(np->en_rp->d_str,&cfab,PtrCFAM) == 0) {
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("found '",logfile);
		switch(cfab.p_type) {
			case PtrAlias_C:
			case PtrAlias_F:
			case PtrAlias_M:
				fprintf(logfile,"%s' ALIAS",cfab.u.p_aliasp->a_name);
				break;
			case PtrMacro:
				fprintf(logfile,"%s' MACRO",cfab.u.p_bufp->b_bname);
				break;
			case PtrCmd:
			case PtrFunc:
				fprintf(logfile,"%s' %s",cfab.p_type == PtrCmd ? "COMMAND" : "FUNCTION",cfab.u.p_cfp->cf_name);
			}
		fprintf(logfile," (type %hu) for token '%s'\n",cfab.p_type,np->en_rp->d_str);
#endif
		// Yes.  Resolve any alias.
		if(cfab.p_type & PtrAlias)
			cfab = cfab.u.p_aliasp->a_cfab;

		// Check if interactive-only command.
		if(cfab.p_type == PtrCmd) {
			if(cfab.u.p_cfp->cf_aflags & CFTerm)
				return rcset(Failure,RCTermAttr,text282,cfab.u.p_cfp->cf_name);
					// "'~b%s~0' command not allowed in a script (use '~brun~0')"

			// If "alias" command (which uses "alias xxx = yyy" syntax), parentheses not allowed.
			if(needrparen && cfab.u.p_cfp->cf_func == aliasCFM)
				return rcset(Failure,0,text289,"(");
						// "Unexpected token '%s'"
			}
		if(foundp != NULL)
			*foundp = true;

		// Have command, function, or macro at this point.  Determine minimum number of required arguments, if possible.
		// Set to -1 if unknown.
		short minArgs = (cfab.p_type == PtrMacro) ? cfab.u.p_bufp->b_mip->mi_minArgs :
		 !(cfab.u.p_cfp->cf_aflags & (CFAddlArg | CFNoArgs)) ? cfab.u.p_cfp->cf_minArgs : !(opflags & OpEval) ? -1 :
		 np->en_narg == INT_MIN ? cfab.u.p_cfp->cf_minArgs : (cfab.u.p_cfp->cf_aflags & CFNoArgs) ? 0 :
		 cfab.u.p_cfp->cf_minArgs + 1;

		// "xxx()" form?
		if(needrparen && havesym(s_rparen,false)) {

			// Yes.  Error if argument(s) required (whether or not evaluating).
			if(minArgs > 0)
				goto Wrong;
			if(!(cfab.p_type == PtrMacro)) {
				if((cfab.u.p_cfp->cf_aflags & CFNoArgs) && !(np->en_flags & EN_HaveNArg))
					goto Wrong;
				if(!(opflags & OpEval) && !(cfab.u.p_cfp->cf_aflags & CFSpecArgs))
					goto Retn;
				}
			}

		// Not "xxx()" call, zero required arguments, or argument requirement cannot be determined.  Proceed with
		// execution or argument consumption.  Note that unusual expressions such as "false && x => seti(5)" where the
		// "seti(5)" is likely an error are not checked here because the numeric prefix is not known when not
		// evaluating.  The specific command or function routine will do the validation.
		short maxArgs;

		if(cfab.p_type == PtrMacro)
			maxArgs = (cfab.u.p_bufp->b_mip->mi_maxArgs < 0) ? SHRT_MAX : cfab.u.p_bufp->b_mip->mi_maxArgs;
		else if((maxArgs = cfab.u.p_cfp->cf_maxArgs) < 0)
			maxArgs = SHRT_MAX;
		else if((opflags & OpEval) && (cfab.u.p_cfp->cf_aflags & (CFAddlArg | CFNoArgs))) {
			if((cfab.u.p_cfp->cf_aflags & CFNoArgs) && np->en_narg != INT_MIN)
				maxArgs = 0;
			else if((cfab.u.p_cfp->cf_aflags & CFAddlArg) && np->en_narg == INT_MIN)
				--maxArgs;
			}
		opflags = (opflags & ~OpParens) | (needrparen ? OpParens : 0);

		// Call the command, function, or macro (as a function) if it's a command or function and CFSpecArgs is set, or
		// evaluating and (1), it's a macro; or (2), it's a command or function and the n argument is not zero or not
		// just a repeat count.
		if(((cfab.p_type & (PtrCmd | PtrFunc)) && (cfab.u.p_cfp->cf_aflags & CFSpecArgs)) || ((opflags & OpEval) &&
		 (cfab.p_type == PtrMacro || np->en_narg != 0 || !(cfab.u.p_cfp->cf_aflags & CFNCount)))) {
			bool fevalcall = false;
#if MMDebug & (Debug_Datum | Debug_Expr)
			char fname[32],wk[80];
			strcpy(fname,np->en_rp->d_str);
#endif
			// Clear node flags.
			np->en_flags &= EN_Concat;

			// Call macro or function.
			dsetnil(np->en_rp);			// Set default return value.
			if(cfab.p_type == PtrMacro) {
#if MMDebug & Debug_Expr
				fprintf(logfile,"fcall(): calling macro %s(%d)...\n",fname,(int) np->en_narg);
#endif
				(void) execbuf(np->en_rp,(int) np->en_narg,cfab.u.p_bufp,NULL,
				 needrparen ? Arg_First | SRun_Parens : Arg_First);
				}
			else {
				CmdFunc *cfp = cfab.u.p_cfp;
				if(!(opflags & OpEval) || allowedit(cfp->cf_aflags & CFEdit) == Success) {
#if MMDebug & Debug_Expr
					fprintf(logfile,"fcall(): calling cmd/func %s(%d): minArgs %d, maxArgs %d...\n",fname,
					 (int) np->en_narg,(int) minArgs,(int) maxArgs);
#endif
					execCF(np->en_rp,(int) np->en_narg,cfp,minArgs,maxArgs);
					fevalcall = true;
#if MMDebug & (Debug_Datum | Debug_Expr)
#if DDebug
					sprintf(wk,"fcall(): %s() returned [%d]...",fname,rc.status);
					ddump(np->en_rp,wk);
#else
					fprintf(logfile,"fcall(): %s() returned... '%s' [%d]\n",fname,dtype(np->en_rp,true),
					 rc.status);
#endif
#endif
					}
				}
			if(rc.status != Success)
				return rc.status;
			if((opflags & OpEval) && !fevalcall)
				(void) rcsave();
			}
		else {
			// Not evaluating or repeat count is zero... consume arguments.
			np->en_flags &= EN_Concat;
			if(maxArgs > 0 && ((!havesym(s_rparen,false) && havesym(s_any,false)) ||
			 ((opflags & OpEval) && minArgs > 0))) {
				bool first = true;
				short argct = 0;
				for(;;) {
#if MMDebug & Debug_Expr
fprintf(logfile,"### fcall(): consuming (minArgs %hd, maxArgs %hd): examing token '%s'...\n",minArgs,maxArgs,
 last->p_tok.d_str);
#endif
					if(first)
						first = false;
					else if(!needsym(s_comma,false))
						break;			// Error or no arguments left.
#if MMDebug & Debug_Expr
					if(ge_debug1("ge_andor",np,NULL,ge_andor) != Success)
#else
					if(ge_andor(np) != Success)	// Get next expression.
#endif
						break;
					++argct;
					}
				if(rc.status != Success)
					return rc.status;
				if((minArgs >= 0 && argct < minArgs) || argct > maxArgs)
					goto Wrong;
				}
			}

		// Check for extra command or function argument.
		if(maxArgs > 0 && havesym(s_comma,false))
Wrong:
			return rcset(Failure,0,text69,last->p_tok.d_str);
				// "Wrong number of arguments (at token '%s')"
		}
	else {
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("UNKNOWN.\n",logfile);
#endif
		// Unknown CFAM.
		if(foundp == NULL)
			return rcset(Failure,0,text244,np->en_rp->d_str);
					// "No such command, alias, or macro '~b%s~0'"
		*foundp = false;
#if MMDebug & (Debug_Datum | Debug_Expr)
		fputs("not a FAB.\n",logfile);
#endif
		}
Retn:
	// Get right paren, if applicable.
	if(needrparen && havesym(s_rparen,true))
		(void) getsym();

	return rc.status;
	}
	
// Evaluate postfix expression and return status.  "narg" is numeric argument for function call, if any.  Postfix expressions
// are any of:
//	primary
//	postfix++
//	postfix--
//	postfix(comma-expression)
//	postfix comma-expression
//	postfix[expression[,expression]]
static int ge_postfix(ENode *np) {
	bool needrparen;
	uint oldparens = opflags & OpParens;

#if MMDebug & Debug_Expr
	if(ge_debug1("ge_primary",np,NULL,ge_primary) != Success)
#else
	if(ge_primary(np) != Success)
#endif
		return rc.status;
	if(lvalue(np,false))
		np->en_flags |= EN_LValue;

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
				if(np->en_flags & EN_HaveNArg)
					goto NoFunc;

				// Perform ++ or -- operation if evaluating.
				if((opflags & OpEval) &&
				 (!lvalue(np,true) || bumpvar(np,last->p_sym == s_incr,false) != Success))
					return rc.status;
				if(getsym() < NotFound)
					return rc.status;
				np->en_flags &= EN_Concat;
				break;
			case s_lparen:
				// A function call.  Error if primary was not an identifier or was an lvalue (variable).
				if(!(np->en_flags & EN_HaveIdent))
					return rcset(Failure,0,text4,text68,last->p_tok.d_str);
							// "%s expected (at token '%s')","Identifier"
				if(np->en_flags & EN_HaveGNVar)
					return rcset(Failure,0,text244,np->en_rp->d_str);
							// "No such command, alias, or macro '~b%s~0'"

				// Primary was an identifier and not a '$' variable.  Assume "function" type.  If white space
				// preceded the '(', the '(' is assumed to be the beginning of a primary expression and hence,
				// the first function argument "f (...),..."; otherwise, the "f(...,...)" form is assumed. 
				// Move past the '(' if no preceding white space and check tables.
				if(!(np->en_flags & EN_HaveWhite)) {
					if(getsym() < NotFound)			// Past '('.
						return rc.status;
					needrparen = true;
					}

				// Call the function.
				if(fcall(np,needrparen,NULL) != Success)
					return rc.status;
				goto Clear;
			case s_lbrkt:
				// Possible array reference; e.g., "[9,[[[0,1],2],3],8][0,2] [1][0][0][1] = 5".
				if(!(np->en_flags & EN_HaveWhite)) {
					long i1;
					Datum *rp2;
					ENode node2;
					bool haveTwo = false;

					// If evaluating, check if current node is an array (otherwise, assume so).
					if(np->en_flags & EN_HaveIdent) {
						if(opflags & OpEval) {

							// Find and dereference variable.  Error if not an array.
							if(vderefn(np->en_rp,np->en_rp->d_str) != Success)
								return rc.status;
							if(!aryval(np->en_rp))
								goto Retn;
							}
						np->en_flags &= ~(EN_HaveIdent | EN_HaveGNVar);
						}
					else if((opflags & OpEval) && !aryval(np->en_rp))
						goto Retn;

					// Get first index.
					if(dnewtrk(&rp2) != 0)
						return librcset(Failure);
					nodeinit(&node2,rp2,false);
					if(getsym() < NotFound || ge_andor(&node2) != Success)
						return rc.status;
					if(opflags & OpEval) {
						if(!intval(node2.en_rp))
							return rc.status;
						i1 = node2.en_rp->u.d_int;
						}

					// Get second index, if present.
					if(needsym(s_comma,false)) {
						haveTwo = true;
						if(ge_andor(&node2) != Success)
							return rc.status;
						if((opflags & OpEval) && !intval(node2.en_rp))
							return rc.status;
						}
					if(!needsym(s_rbrkt,true))
						return rc.status;

					// Evaluate if array slice; otherwise, save index in node for (possible use as an
					// lvalue) later.
					if(ge_deref(np) != Success)
						return rc.status;
					if(opflags & OpEval) {
						if(!aryval(np->en_rp))
							return rc.status;
						if(haveTwo) {
							Array *aryp;

							if((aryp = aslice(awptr(np->en_rp)->aw_aryp,i1,node2.en_rp->u.d_int))
							 == NULL)
								return librcset(Failure);
							if(awrap(np->en_rp,aryp) != Success)
								return rc.status;
							}
						else {
							np->en_index = i1;
							np->en_flags |= EN_ArrayRef | EN_LValue;
							}
						}
					break;
					}
				// Fall through.
			default:
				// Was primary a non-variable identifier?
				if((np->en_flags & (EN_HaveIdent | EN_HaveGNVar)) == EN_HaveIdent) {
					bool found;
					if(fcall(np,false,&found) != Success)
						return rc.status;
					if(found) {
Clear:
						// Clear flag(s) obviated by a function call.
						np->en_flags &= EN_Concat;
						break;
						}
					}

				// Not a function call.  Was last symbol a numeric prefix operator?
				if(np->en_flags & EN_HaveNArg)
NoFunc:
					return rcset(Failure,0,text4,text67,np->en_rp->d_str);
							// "%s expected (at token '%s')","Function call"

				// No postfix operators left.  Bail out.
				goto Retn;
			}
		}
Retn:
	opflags = (opflags & ~OpParens) | oldparens;
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
#if MMDebug & Debug_Expr
			if(getsym() < NotFound || ge_debug1("ge_unary",np,NULL,ge_unary) != Success)
#else
			if(getsym() < NotFound || ge_unary(np) != Success)
#endif
				return rc.status;
			if(sym == s_incr || sym == s_decr) {

				// Perform ++ or -- operation if evaluating.
				if((opflags & OpEval) && lvalue(np,true))
					(void) bumpvar(np,sym == s_incr,true);
				np->en_flags &= EN_Concat;
				}

			else {
				// Perform the operation.
#if MMDebug & Debug_Expr
				if(ge_debug1("ge_deref",np,NULL,ge_deref) != Success)
#else
				if(ge_deref(np) != Success)
#endif
					return rc.status;
				if(opflags & OpEval) {
					if(sym != s_not && !intval(np->en_rp))
						return rc.status;
					if(sym == s_not)
						dsetbool(tobool(np->en_rp) == false,np->en_rp);
					else if(sym != s_plus)
						dsetint(sym == s_minus ? -np->en_rp->u.d_int : ~np->en_rp->u.d_int,np->en_rp);
					}
				}
			break;
		default:
#if MMDebug & Debug_Expr
			(void) ge_debug1("ge_postfix",np,NULL,ge_postfix);
#else
			(void) ge_postfix(np);
#endif
		}
	return rc.status;
	}

// Concatenate two nodes.  First node may be string or array.  If the latter and "clone" is true, clone it first.
static int concat(ENode *np1,ENode *np2,bool clone) {

	if(np1->en_rp->d_type == dat_blobRef) {
		Array *ary1;

		if(clone && aryclone(np1->en_rp,np1->en_rp,0) != Success)
			return rc.status;
		ary1 = awptr(np1->en_rp)->aw_aryp;

		// If node2 is an array, expand it.
		if(np2->en_rp->d_type == dat_blobRef) {
			if(agraph(ary1,awptr(np2->en_rp)->aw_aryp) == NULL)
				return librcset(Failure);
			}
		else if(apush(ary1,np2->en_rp) != 0)
			return librcset(Failure);
		}
	else if(np2->en_rp->d_type != dat_nil) {
		DStrFab sf;

		if(dopenwith(&sf,np1->en_rp,SFAppend) != 0 || dputd(np2->en_rp,&sf) != 0 || dclose(&sf,sf_string) != 0)
			(void) librcset(Failure);
		}
	return rc.status;
	}

// Compare two arrays for equality and set *resultp to Boolean result.  Return status.
static int aequal(Datum *datp1,Datum *datp2,bool *resultp) {
	ArrayWrapper *awp1 = awptr(datp1);
	ArrayWrapper *awp2 = awptr(datp2);
	if(awp1->aw_mark || awp2->aw_mark)
		return rcset(Failure,RCNoFormat,text195);
			// "Endless recursion detected (array contains itself)"
	Array *ary1 = awp1->aw_aryp;
	Array *ary2 = awp2->aw_aryp;
	ArraySize size = ary1->a_used;
	bool result;
	if(size != ary2->a_used)
		goto FalseRetn;
	if(size == 0)
		goto TrueRetn;
	awp1->aw_mark = awp2->aw_mark = true;
	Datum **elpp1 = ary1->a_elpp;
	Datum **elpp2 = ary2->a_elpp;
	Datum *elp1,*elp2;
	do {
		elp1 = *elpp1++;
		elp2 = *elpp2++;
		if(elp1->d_type == dat_blobRef) {
			if(elp2->d_type != dat_blobRef)
				goto FalseRetn;
			if(aequal(elp1,elp2,&result) != Success)
				return rc.status;
			if(!result)
				goto Retn;
			}
		else if(!dateq(elp1,elp2,false))
			goto FalseRetn;
		} while(--size > 0);
TrueRetn:
	result = true;
	goto Retn;
FalseRetn:
	result = false;
Retn:
	*resultp = result;
	return rc.status;
	}

// Compare two arrays for equality and return Boolean result in *resultp.  Return status.
int aryeq(Datum *datp1,Datum *datp2,bool *resultp) {

	agarbpush(datp1);
	agarbpush(datp2);
	aclrmark();
	return aequal(datp1,datp2,resultp);
	}

// Common routine to handle all of the legwork and error checking for all of the binary operators.
#if MMDebug & Debug_Expr
static int ge_binop(ENode *np,char *fname,OpInfo *oip) {
#else
static int ge_binop(ENode *np,OpInfo *oip) {
#endif
	enum e_sym *symp;
	Datum *op,*rp2;
	ENode node2;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1(fname,np,NULL,oip->xfunc) != Success)
#else
	if(oip->xfunc(np) != Success)
#endif
		return rc.status;

	if(dnewtrk(&op) != 0 || dnewtrk(&rp2) != 0)
		return librcset(Failure);

	// Loop until no operator(s) at this level remain.
	for(;;) {
		symp = oip->symp;
		for(;;) {
			if(*symp == last->p_sym)
				break;
			if(*symp == s_any) {

				// No operators left.  Clear EN_Concat flag if concatenate op.
				if((oip->flags & FF_Concat) && (np->en_flags & EN_Concat))
					np->en_flags &= ~EN_Concat;
				goto Retn;
				}
			++symp;
			}

		// Found valid operator.  Dereference.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref",np,NULL,ge_deref) != Success)
#else
		if(ge_deref(np) != Success)
#endif
			goto Retn;

		// If evaluating, have '&' op, and ("concatenation" and first operand is not array or string) or ("bitwise and"
		// and (EN_Concat flag is set or first operand is not integer)), ignore it (at wrong level).  OR if evaluating,
		// have '%' op, and ("format" and first operand is not string) or ("modulus" and first operand is not integer),
		// ignore it as well.
		if(opflags & OpEval) {
			if(*symp == s_band && (((oip->flags & FF_Concat) && np->en_rp->d_type == dat_int) ||
			 ((oip->flags & FF_BitOp) && ((np->en_flags & EN_Concat) || np->en_rp->d_type != dat_int))))
				goto Retn;
			if(*symp == s_mod && (((oip->flags & FF_Format) && np->en_rp->d_type == dat_int) ||
			 ((oip->flags & FF_Math) && (np->en_rp->d_type != dat_int))))
				goto Retn;
			}

		// We're good.  Save operator for error reporting.
		datxfer(op,&last->p_tok);

		// Set "force concatenation" flag in second node if applicable, and call function at next higher level.
		nodeinit(&node2,rp2,false);
		if(oip->flags & FF_Concat)
			node2.en_flags = EN_Concat;
#if MMDebug & Debug_Expr
		if(getsym() < NotFound || ge_debug1(fname,np,&node2,oip->xfunc) != Success)
#else
		if(getsym() < NotFound || oip->xfunc(&node2) != Success)
#endif
			goto Retn;

		// Dereference any lvalue.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref",np,&node2,ge_deref) != Success)
#else
		if(ge_deref(&node2) != Success)
#endif
			goto Retn;

		// If evaluating expressions, coerce binary operands and perform operation.
		if(opflags & OpEval) {
			if(forcefit(np,&node2,*symp == s_req || *symp == s_rne ? FF_REQNE : oip->flags,op->d_str) != Success)
				goto Retn;
			switch(*symp) {

				// Bitwise.
				case s_band:
					// If FF_Concat flag is set, do concatenation; otherwise, bitwise and.
					if(!(oip->flags & FF_Concat))
						dsetint(np->en_rp->u.d_int & node2.en_rp->u.d_int,np->en_rp);
					else if(concat(np,&node2,np->en_flags & EN_LValue) != Success)
						return rc.status;
					break;
				case s_bor:
					dsetint(np->en_rp->u.d_int | node2.en_rp->u.d_int,np->en_rp);
					break;
				case s_bxor:
					dsetint(np->en_rp->u.d_int ^ node2.en_rp->u.d_int,np->en_rp);
					break;
				case s_lsh:
					dsetint((ulong) np->en_rp->u.d_int << (ulong) node2.en_rp->u.d_int,np->en_rp);
					break;
				case s_rsh:
					dsetint((ulong) np->en_rp->u.d_int >> (ulong) node2.en_rp->u.d_int,np->en_rp);
					break;

				// Multiplicative and additive.
				case s_div:
					if(node2.en_rp->u.d_int == 0)
						goto DivZero;
					dsetint(np->en_rp->u.d_int / node2.en_rp->u.d_int,np->en_rp);
					break;
				case s_mod:
					// If FF_Format flag is set, do string formatting; otherwise, modulus.
					if(oip->flags & FF_Format) {
						Datum *tp;

						if(dnewtrk(&tp) != 0)
							return librcset(Failure);
						datxfer(tp,np->en_rp);
						if(strfmt(np->en_rp,tp,node2.en_rp) != Success)
							return rc.status;
						}
					else {
						if(node2.en_rp->u.d_int == 0)
DivZero:
						return rcset(Failure,0,text245,np->en_rp->u.d_int);
								// "Division by zero is undefined (%ld/0)"
						dsetint(np->en_rp->u.d_int % node2.en_rp->u.d_int,np->en_rp);
						}
					break;
				case s_mul:
					dsetint(np->en_rp->u.d_int * node2.en_rp->u.d_int,np->en_rp);
					break;
				case s_plus:
					dsetint(np->en_rp->u.d_int + node2.en_rp->u.d_int,np->en_rp);
					break;
				case s_minus:
					dsetint(np->en_rp->u.d_int - node2.en_rp->u.d_int,np->en_rp);
					break;
				case s_eq:
				case s_ne:
					{bool b = true;		// Default if two nil, false, or true operands.
					if(np->en_rp->d_type != node2.en_rp->d_type)
						b = false;
					else if(np->en_rp->d_type != dat_nil && !(np->en_rp->d_type & DBoolMask)) {
						if(np->en_rp->d_type == dat_blobRef) {
							if(aryeq(np->en_rp,node2.en_rp,&b) != Success)	// Compare arrays.
								return rc.status;
							}
						else
							b = np->en_rp->d_type == dat_int ?
							 np->en_rp->u.d_int == node2.en_rp->u.d_int :
							 strcmp(np->en_rp->d_str,node2.en_rp->d_str) == 0;
						}
					dsetbool(b == (*symp == s_eq),np->en_rp);
					}
					break;
				case s_ge:
				case s_gt:
				case s_le:
				case s_lt:
					{long lval1,lval2;

					if(np->en_rp->d_type == dat_int) {	// Both operands are integer.
						lval1 = np->en_rp->u.d_int;
						lval2 = node2.en_rp->u.d_int;
						}
					else {					// Both operands are string.
						lval1 = strcmp(np->en_rp->d_str,node2.en_rp->d_str);
						lval2 = 0L;
						}
					dsetbool(*symp == s_lt ? lval1 < lval2 : *symp == s_le ? lval1 <= lval2 :
					 *symp == s_gt ? lval1 > lval2 : lval1 >= lval2,np->en_rp);
					}
					break;

				// RE equality: s_req, s_rne
				default:
					if(disnull(node2.en_rp))
						return rcset(Failure,0,text187,text266);
							// "%s cannot be null","Regular expression"

					// Compile the RE pattern.
					if(newspat(node2.en_rp->d_str,&rematch,NULL) == Success) {
						if(rematch.flags & SOpt_Plain)
							return rcset(Failure,0,text36,OptCh_Plain,op->d_str);
								// "Invalid pattern option '%c' for '%s' operator"
						grpclear(&rematch);
						if(mccompile(&rematch) == Success) {
							int offset;

							// Perform operation.
							if(recmp(np->en_rp,0,&rematch,&offset) == Success)
								dsetbool((offset >= 0) == (*symp == s_req),np->en_rp);
							}
						}
				}
			np->en_flags &= ~EN_LValue;
			}
		}
Retn:
	return rc.status;
	}

// Process multiplication, division and modulus operators.
static int ge_mult(ENode *np) {
	static enum e_sym sy_mult[] = {s_mul,s_div,s_mod,s_any};
	static OpInfo oi_mult = {ge_unary,sy_mult,FF_Math};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_unary",&oi_mult);
#else
	return ge_binop(np,&oi_mult);
#endif
	}

// Process addition and subtraction operators.
static int ge_add(ENode *np) {
	static enum e_sym sy_add[] = {s_plus,s_minus,s_any};
	static OpInfo oi_add = {ge_mult,sy_add,FF_Math};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_mult",&oi_add);
#else
	return ge_binop(np,&oi_add);
#endif
	}

// Process numeric prefix (n) operator =>.
static int ge_numpref(ENode *np) {

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_add",np,NULL,ge_add) != Success)
#else
	if(ge_add(np) != Success)
#endif
		return rc.status;

	// Loop until no operator at this level remains.
	while(last->p_sym == s_narg) {

		// Last expression was an n argument.  Verify that it was an integer and save it in the node so that the next
		// expression (a function call) can grab it.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref",np,NULL,ge_deref) != Success)
#else
		if(ge_deref(np) != Success)
#endif
			return rc.status;
		if(opflags & OpEval) {
			if(!intval(np->en_rp))
				return rc.status;
			np->en_narg = np->en_rp->u.d_int;
			}
		np->en_flags |= EN_HaveNArg;

		// The next expression must be a function call (which is verified by ge_postfix()).
#if MMDebug & Debug_Expr
		if(getsym() < NotFound || ge_debug1("ge_postfix",np,NULL,ge_postfix) != Success)
#else
		if(getsym() < NotFound || ge_postfix(np) != Success)
#endif
			return rc.status;
		}

	return rc.status;
	}

// Process shift operators << and >>.
static int ge_shift(ENode *np) {
	static enum e_sym sy_shift[] = {s_lsh,s_rsh,s_any};
	static OpInfo oi_shift = {ge_numpref,sy_shift,FF_Shft};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_numpref",&oi_shift);
#else
	return ge_binop(np,&oi_shift);
#endif
	}

// Process bitwise and operator &.
static int ge_bitand(ENode *np) {
	static enum e_sym sy_bitand[] = {s_band,s_any};
	static OpInfo oi_bitand = {ge_shift,sy_bitand,FF_BitOp};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_shift",&oi_bitand);
#else
	return ge_binop(np,&oi_bitand);
#endif
	}

// Process bitwise or and xor operators | and ^.
static int ge_bitor(ENode *np) {
	static enum e_sym sy_bitor[] = {s_bor,s_bxor,s_any};
	static OpInfo oi_bitor = {ge_bitand,sy_bitor,FF_BitOp};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_bitand",&oi_bitor);
#else
	return ge_binop(np,&oi_bitor);
#endif
	}

// Process string format operator %.
static int ge_format(ENode *np) {
	static enum e_sym sy_format[] = {s_mod,s_any};
	static OpInfo oi_format = {ge_bitor,sy_format,FF_Format};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_bitor",&oi_format);
#else
	return ge_binop(np,&oi_format);
#endif
	}

// Process concatenation operator &.
static int ge_concat(ENode *np) {
	static enum e_sym sy_concat[] = {s_band,s_any};
	static OpInfo oi_concat = {ge_format,sy_concat,FF_Concat};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_format",&oi_concat);
#else
	return ge_binop(np,&oi_concat);
#endif
	}

// Process relational operators <, <=, > and >=.
static int ge_rel(ENode *np) {
	static enum e_sym sy_rel[] = {s_lt,s_gt,s_le,s_ge,s_any};
	static OpInfo oi_rel = {ge_concat,sy_rel,FF_Rel};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_concat",&oi_rel);
#else
	return ge_binop(np,&oi_rel);
#endif
	}

// Process equality and inequality operators ==, !=, =~, and !~.
static int ge_eqne(ENode *np) {
	static enum e_sym sy_eqne[] = {s_eq,s_ne,s_req,s_rne,s_any};
	static OpInfo oi_eqne = {ge_rel,sy_eqne,FF_EQNE};

#if MMDebug & Debug_Expr
	return ge_binop(np,"ge_rel",&oi_eqne);
#else
	return ge_binop(np,&oi_eqne);
#endif
	}

// Do logical and/or.
#if MMDebug & Debug_Expr
static int ge_landor(ENode *np,char *fname,int (*fncp)(ENode *np)) {
#else
static int ge_landor(ENode *np,int (*fncp)(ENode *np)) {
#endif
	bool b;
	Datum *rp2;
	ENode node2;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1(fname,np,NULL,fncp) != Success)
#else
	if(fncp(np) != Success)
#endif
		return rc.status;

	if(dnewtrk(&rp2) != 0)
		return librcset(Failure);

	// Loop until no operator(s) at this level remain.
	for(;;) {
		switch(last->p_sym) {
			case s_and:
				if(fncp != ge_eqne)
					goto Retn;
				b = false;
				goto AndOr;
			case s_or:
				if(fncp == ge_eqne)
					goto Retn;
				b = true;
AndOr:
				if(getsym() < NotFound)				// Past '&&' or '||'.
					goto Retn;
				nodeinit(&node2,rp2,false);
#if MMDebug & Debug_Expr
				if(ge_debug1("ge_deref",np,NULL,ge_deref) != Success)
#else
				if(ge_deref(np) != Success)			// Dereference var if needed.
#endif
					goto Retn;
				if(!(opflags & OpEval)) {			// Eating arguments?
#if MMDebug & Debug_Expr
					if(ge_debug1(fname,&node2,NULL,fncp) != Success)
#else
					if(fncp(&node2) != Success)		// Yes, bon appetit.
#endif
						goto Retn;
					}
				else if(tobool(np->en_rp) == b) {		// No, does first argument determine outcome?
					dsetbool(b,np->en_rp);			// Yes, convert to logical...
					opflags &= ~OpEval;			// and eat second argument.
#if MMDebug & Debug_Expr
					(void) ge_debug1(fname,np,&node2,fncp);
#else
					(void) fncp(&node2);
#endif
					opflags |= OpEval;
					if(rc.status != Success)
						goto Retn;
					}
				else {
#if MMDebug & Debug_Expr
					if(ge_debug1(fname,np,&node2,fncp) != Success ||
					 ge_debug1("ge_deref",&node2,NULL,ge_deref) != Success)
#else
					if(fncp(&node2) != Success ||		// No, evaluate second argument...
					 ge_deref(&node2) != Success)		// dereference var if needed...
#endif
						goto Retn;
					dsetbool(tobool(node2.en_rp),np->en_rp); // and convert to logical.
					}
				break;
			default:
				goto Retn;
			}
		}
Retn:
	return rc.status;
	}

// Logical and operator &&.
static int ge_and(ENode *np) {

#if MMDebug & Debug_Expr
	return ge_landor(np,"ge_eqne",ge_eqne);
#else
	return ge_landor(np,ge_eqne);
#endif
	}

// Logical or operator ||.
static int ge_or(ENode *np) {

#if MMDebug & Debug_Expr
	return ge_landor(np,"ge_and",ge_and);
#else
	return ge_landor(np,ge_and);
#endif
	}

// Process conditional (hook) operator ? :.
static int ge_cond(ENode *np) {

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_or",np,NULL,ge_or) != Success)
#else
	if(ge_or(np) != Success)
#endif
		return rc.status;

	if(last->p_sym == s_hook) {
		ENode node2;
		Datum *rp2;
		bool loop2 = false;
		bool eat = true;

		// Dereference any lvalue.
#if MMDebug & Debug_Expr
		if(ge_debug1("ge_deref",np,NULL,ge_deref) != Success)
#else
		if(ge_deref(np) != Success)
#endif
			return rc.status;
		if(opflags & OpEval) {
			eat = !tobool(np->en_rp);
			if(dnewtrk(&rp2) != 0)
				return librcset(Failure);
			}

		// Loop twice.
		for(;;) {
			if(getsym() < NotFound)		// Past '?' or ':'.
				return rc.status;

			// Don't evaluate one of the arguments if "evaluate mode" was true when we started.
			if(opflags & OpEval) {
				if(eat) {
					nodeinit(&node2,rp2,false);
					opflags &= ~OpEval;
#if MMDebug & Debug_Expr
					(void) ge_debug1("ge_cond",&node2,NULL,ge_cond);
#else
					(void) ge_cond(&node2);
#endif
					opflags |= OpEval;
					if(rc.status != Success)
						return rc.status;
					eat = false;
					goto NextSym;
					}
				else
					eat = true;
				}
			nodeinit(np,np->en_rp,false);
#if MMDebug & Debug_Expr
			if(ge_debug1("ge_cond",np,NULL,ge_cond) != Success ||
			 ge_debug1("ge_deref",np,NULL,ge_deref) != Success)
#else
			if(ge_cond(np) != Success || ge_deref(np) != Success)
#endif
				return rc.status;
NextSym:
			if(loop2)
				break;
			if(!havesym(s_any,false) || last->p_sym != s_colon)
				return rcset(Failure,0,text4,"':'",last->p_tok.d_str);
					// "%s expected (at token '%s')"
			loop2 = true;
			}
		}

	return rc.status;
	}

// Process assignment operators =, +=, -=, *=, /=, %=, <<=, >>=, &=, ^=, |=, and parallel assignment x,y,z = [1,2,3].
int ge_assign(ENode *np) {
	VDesc vd;
	Datum *rp2,*op;
	ENode node2;
	uint f;
	enum e_sym sym;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_cond",np,NULL,ge_cond) != Success)
#else
	if(ge_cond(np) != Success)
#endif
		return rc.status;

	// Assignment?
	if((last->p_sym < s_assign || last->p_sym > s_asbor) &&
	 (last->p_sym != s_comma || !(np->en_flags & (EN_TopLevel | EN_PAssign)))) {

		// No, dereference any identifier or array reference and return.
#if MMDebug & Debug_Expr
		fprintf(logfile,"### ge_assign(): Dereferencing '%s'...\n",dtype(np->en_rp,false));
#endif
		return ge_deref(np);
		}

	// Have assignment operator.  Valid?
	if((np->en_flags & EN_PAssign) && (last->p_sym != s_assign && last->p_sym != s_comma))
		return rcset(Failure,0,text4,"'='",last->p_tok.d_str);
			// "%s expected (at token '%s')"

	// Have valid operator.  Check if node is an lvalue and build a VDesc object from it if evaluating.
	if(opflags & OpEval) {
		if(!(np->en_flags & (EN_HaveIdent | EN_ArrayRef)))
			goto BadLValue;
		if(np->en_flags & EN_HaveIdent) {

			// Error if name matches an existing command, function, alias, buffer, or macro.
			if(cfabsearch(np->en_rp->d_str,NULL,PtrCFAM) == 0)
BadLValue:
				return rcset(Failure,0,text4,text82,last->p_tok.d_str);
					// "%s expected (at token '%s')","Variable name"
			if(findvar(np->en_rp->d_str,&vd,last->p_sym == s_assign || last->p_sym == s_comma ? OpCreate : OpDelete)
			 != Success)
				return rc.status;
			}
		else if(aryget(np,&vd,last->p_sym == s_assign || last->p_sym == s_comma) != Success)
			return rc.status;
		}
	np->en_flags &= ~(EN_HaveIdent | EN_HaveGNVar | EN_HaveWhite | EN_ArrayRef);

	// Set coercion flags.
	switch(sym = last->p_sym) {
		case s_comma:
		case s_assign:
			break;
		case s_asadd:
		case s_assub:
		case s_asmul:
		case s_asdiv:
		case s_asmod:
			f = FF_Math;
			break;
		case s_aslsh:
		case s_asrsh:
			f = FF_Shft;
			break;
		case s_asband:
			if((opflags & OpEval) && !intvar(&vd)) {
				f = FF_Concat;
				break;
				}
			// else fall through.
		default:
			f = FF_BitOp;
		}

	// If evaluating, save assign op (for error reporting).
	if(opflags & OpEval) {
		if(dnewtrk(&op) != 0)
			return librcset(Failure);
		datxfer(op,&last->p_tok);
		}

	// Move past operator and prepare to get value expression.
	if(getsym() < NotFound)
		return rc.status;
	if(dnewtrk(&rp2) != 0)
		return librcset(Failure);
	nodeinit(&node2,rp2,sym == s_comma);

	// If doing parallel assignment, set array index in node2.en_narg for next recursive call.
	if(sym == s_comma) {
		node2.en_flags = EN_PAssign;
		if(np->en_flags & EN_PAssign)
			node2.en_narg = np->en_narg + 1;	// Not first comma.  Bump index for next instance.
		else {						// First comma.  Initalize both nodes.
			np->en_narg = 0;			// Current instance (lvalue) gets first array element...
			node2.en_narg = 1;			// and next instance gets second.
			np->en_flags |= EN_PAssign;
			}
		}

	// Get value.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_assign",np,&node2,ge_assign) != Success || ge_debug1("ge_deref",np,&node2,ge_deref) != Success)
#else
	if(ge_assign(&node2) != Success || ge_deref(&node2) != Success)
#endif
		return rc.status;

	// If evaluating...
	if(opflags & OpEval) {

		// Get current variable value or array element into np if not straight assignment, and coerce operands into a
		// compatible type.
		if(sym != s_assign && sym != s_comma)
			if(vderefv(np->en_rp,&vd) != Success || forcefit(np,&node2,f,op->d_str) != Success)
				return rc.status;

		// Do operation and put result into np.  np contains left side and node2 contains right.
		switch(sym) {
			case s_assign:
				// Value must be an array if doing parallel assignment.
				if(np->en_flags & EN_PAssign) {
					if(!aryval(node2.en_rp))
						return rc.status;
					np->en_flags &= ~EN_PAssign;
					goto PAssign;
					}
#if DDebug
	ddump(node2.en_rp,"ge_assign(): Transferring from src object...");
	ddump(np->en_rp,"ge_assign(): to dest object...");
#endif
				datxfer(np->en_rp,node2.en_rp);
#if DDebug
	ddump(np->en_rp,"ge_assign(): Result (dest)...");
#endif
				break;
			case s_comma:
PAssign:
				// Set lvalue to array element, or nil if element does not exist.  Return whole array in node.
				{Array *aryp = awptr(node2.en_rp)->aw_aryp;
				if(np->en_narg >= aryp->a_used) {
					Datum d;
					dinit(&d);
					(void) putvar(&d,&vd);
					}
				else
					(void) putvar(aryp->a_elpp[np->en_narg],&vd);
				}
				datxfer(np->en_rp,node2.en_rp);
				goto Retn;
			case s_asadd:
				np->en_rp->u.d_int += node2.en_rp->u.d_int;
				break;
			case s_assub:
				np->en_rp->u.d_int -= node2.en_rp->u.d_int;
				break;
			case s_asmul:
				np->en_rp->u.d_int *= node2.en_rp->u.d_int;
				break;
			case s_asdiv:
				if(node2.en_rp->u.d_int == 0L)
					goto DivZero;
				np->en_rp->u.d_int /= node2.en_rp->u.d_int;
				break;
			case s_asmod:
				if(node2.en_rp->u.d_int == 0L)
DivZero:
					return rcset(Failure,0,text245,np->en_rp->u.d_int);
						// "Division by zero is undefined (%ld/0)"
				np->en_rp->u.d_int %= node2.en_rp->u.d_int;
				break;
			case s_aslsh:
				np->en_rp->u.d_int <<= node2.en_rp->u.d_int;
				break;
			case s_asrsh:
				np->en_rp->u.d_int >>= (ulong) node2.en_rp->u.d_int;
				break;
			case s_asband:
				if(f & FF_BitOp)
					np->en_rp->u.d_int &= node2.en_rp->u.d_int;
				else {
					if(concat(np,&node2,false) != Success)
						return rc.status;

					// If left node is an array, the lvalue was modified directly by concat(), so skip
					// (redundant) call to putvar().
					if(np->en_rp->d_type == dat_blobRef)
						goto Retn;
					}
				break;
			case s_asbxor:
				np->en_rp->u.d_int ^= node2.en_rp->u.d_int;
				break;
			default:	// s_asbor
				np->en_rp->u.d_int |= node2.en_rp->u.d_int;
			}
		(void) putvar(np->en_rp,&vd);
		}
Retn:
	return rc.status;
	}

// Evaluate low precedence logical not expression "not".
static int ge_not(ENode *np) {

	if(last->p_sym == kw_not) {
#if MMDebug & Debug_Expr
		if(getsym() < NotFound || ge_debug1("ge_not",np,NULL,ge_not) != Success)
#else
		if(getsym() < NotFound || ge_not(np) != Success)
#endif
			return rc.status;

		// Perform operation.
		if(opflags & OpEval)
			dsetbool(!tobool(np->en_rp),np->en_rp);
		}
	else
#if MMDebug & Debug_Expr
		(void) ge_debug1("ge_assign",np,NULL,ge_assign);
#else
		(void) ge_assign(np);
#endif

	return rc.status;
	}

// Evaluate low precedence logical and/or expressions "and", "or".
int ge_andor(ENode *np) {
	bool eval,priorTruth,curTruth;
	Datum *rp2;
	ENode node2;

	// Call function at next higher level.
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_not",np,NULL,ge_not) != Success)
#else
	if(ge_not(np) != Success)
#endif
		return rc.status;

	if(dnewtrk(&rp2) != 0)
		return librcset(Failure);
	eval = (opflags & OpEval) != 0;

	// Loop until no operator(s) at this level remain.  If we weren't evaluating initially (eval is false), then all ops
	// evaluate to false.
	for(;;) {
		priorTruth = tobool(np->en_rp);
		switch(last->p_sym) {			// true or false or true
			case kw_and:			// false and true and false
				curTruth = false;	// true or false and EVAL
				goto AndOr;		// false and true or EVAL
			case kw_or:
				curTruth = true;
AndOr:
				if(getsym() < NotFound)				// Past 'and' or 'or'.
					goto Retn;
				nodeinit(&node2,rp2,false);
				if(!(opflags & OpEval)) {			// Eating arguments?
					if(eval && curTruth != priorTruth) {	// Yes, stop the gluttony?
						opflags |= OpEval;		// Yes, enough already.
						goto Eval;
						}
#if MMDebug & Debug_Expr
					if(ge_debug1("ge_not",np,&node2,ge_not) != Success)
#else
					if(ge_not(&node2) != Success)		// No, bon appetit.
#endif
						goto Retn;
					}
				else if(priorTruth == curTruth) {		// No, does prior argument determine outcome?
					dsetbool(curTruth,np->en_rp);		// Yes, convert to logical...
					opflags &= ~OpEval;			// and eat next argument.
#if MMDebug & Debug_Expr
					(void) ge_debug1("ge_not",np,&node2,ge_not);
#else
					(void) ge_not(&node2);
#endif
					opflags |= OpEval;
					if(rc.status != Success)
						goto Retn;
					}
				else {						// No, evaluate next argument.
Eval:
#if MMDebug & Debug_Expr
					if(ge_debug1("ge_not",np,&node2,ge_not) != Success)
#else
					if(ge_not(&node2) != Success)
#endif
						goto Retn;
					dsetbool(tobool(node2.en_rp),np->en_rp);
					}
				break;
			default:
				goto Retn;
			}
		}
Retn:
	return rc.status;
	}

// Return type of Datum object as a string.
char *dtype(Datum *datp,bool terse) {
	char *str;

	switch(datp->d_type) {
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
static int valarg(Datum *datp,uint aflags) {

	if((aflags & (CFInt1 | CFMay)) == CFInt1)				// 'int' flag set without "may be"?
		(void) intval(datp);						// Yes, must be integer.
	else if((aflags & (CFArray1 | CFNIS1 | CFMay)) == CFArray1)		// No, 'array' set without other types?
		(void) aryval(datp);						// Yes, must be array.
	else if(!(aflags & (CFNil1 | CFBool1 | CFInt1 | CFArray1 | CFNIS1))) {	// No, non-string flags not set?
		if(strval(datp) && (aflags & CFNotNull1) && disnull(datp))	// Yes, must be string.  Check if null string
			goto NotNull;						// is an error.
		}
	else if((!(aflags & CFArray1) && datp->d_type == dat_blobRef) ||	// No required types.  Check if found type
	 (!(aflags & CFBool1) && (datp->d_type & DBoolMask)) ||			// is allowed.
	 (!(aflags & CFNIS1) && (
	 (!(aflags & CFNil1) && datp->d_type == dat_nil) ||
	 (!(aflags & CFInt1) && datp->d_type == dat_int))))
		return rcset(Failure,0,text329,dtype(datp,false));
				// "Unexpected %s argument"
	else if((datp->d_type & DStrMask) && (aflags & CFNotNull1) &&		// Null string error?
	 disnull(datp))
NotNull:
		return rcset(Failure,0,text187,text285);
			// "%s cannot be null","Call argument"
	return rc.status;							// No, all is well.
	}

// Get a script line argument, given pointer to result and argument flags.  Return an error if argument does not conform to
// validation flags.
int funcarg(Datum *rp,uint aflags) {
	ENode node;

#if MMDebug & Debug_Expr
	fprintf(logfile,"funcarg(...,%.8X): BEGIN at token '%s', remainder '%s'...\n",aflags,last->p_tok.d_str,last->p_cl);
	fflush(logfile);
#endif
	if(!(aflags & Arg_First) && !needsym(s_comma,true))
		return rc.status;
	nodeinit(&node,rp,false);
#if MMDebug & Debug_Expr
	if(ge_debug1("ge_andor",&node,NULL,ge_andor) != Success)
#else
	if(ge_andor(&node) != Success)
#endif
		return rc.status;

	// If evaluating, validate value.
	return (opflags & OpEval) ? valarg(rp,aflags) : rc.status;
	}

// Get next argument from script line or an array, given indirect pointer to result, pointer to argument flags, work Datum
// object, indirect array pointer, and indirect array length.  Return an error if argument does not conform to validation flags
// or NotFound if no arguments left.
int nextarg(Datum **rpp,uint *aflagsp,Datum *datp,Datum ***elppp,ArraySize *elctp) {

	for(;;) {
		// Processing an array?
		if(*elppp != NULL) {
			if((*elctp)-- > 0)//{*rpp = *(*elppp)++; break;}
				return valarg(*rpp = *(*elppp)++,*aflagsp);
			*elppp = NULL;
			}
		else {
			if(!(*aflagsp & Arg_First) && !havesym(s_comma,false))
				return NotFound;			// At least one argument retrieved and none left.
			if(funcarg(datp,*aflagsp) != Success)
				return rc.status;
			*aflagsp &= ~Arg_First;
			if(datp->d_type == dat_blobRef) {
				Array *aryp = awptr(datp)->aw_aryp;
				*elppp = aryp->a_elpp;
				*elctp = aryp->a_used;
				}
			else {
				*rpp = datp;
				break;
				}
			}
		// Onward...
		}
	return rc.status;
	}
