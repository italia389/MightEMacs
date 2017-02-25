// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// exec.c	Routines dealing with execution of commands, command lines, buffers, and command files for MightEMacs.

#include "os.h"
#include <stdarg.h>
#include "std.h"
#include "lang.h"
#include "cmd.h"
#include "bind.h"
#include "main.h"

// Make selected global definitions local.
#define DATAexec
#include "exec.h"

#include "file.h"
#include "var.h"

/*** Local declarations ***/

// Control data for if/loop execution levels.
typedef struct {
	VDesc vd;		// Loop variable descriptor.
	Array *aryp;		// Loop array.
	ArraySize index;	// Index of next element to process.
	} ForLoopInfo;
typedef struct LevelInfo {
	struct LevelInfo *prevp,*nextp;
#if MMDebug & (Debug_Script | Debug_PPBuf)
	uint level;		// Execution level, beginning at zero.
#endif
	bool live;		// True if executing this level.
	bool loopspawn;		// True if level spawned by any loop statement.
	int loopcount;		// Number of times through the loop.
	ForLoopInfo *flip;	// Control information for "for" loop.
	bool ifwastrue;		// True if (possibly compound) "if" statement was ever true.
	bool elseseen;		// True if "else" clause has been processed at this level.
	} LevelInfo;

#define StmtKeywordMax	8		// Length of longest statement keyword name.

// Search hook table for given name.  If found, set *hrpp to pointer; otherwise, return an error.
static int findhook(char *name,HookRec **hrpp) {
	HookRec *hrp = hooktab;

	do {
		if(strcmp(name,hrp->h_name) == 0) {
			*hrpp = hrp;
			return rc.status;
			}
		} while((++hrp)->h_name != NULL);

	return rcset(Failure,0,text317,name);
			// "No such hook '%s'"
	}

// Set a hook.  Return status.
int setHook(Datum *rp,int n,Datum **argpp) {
	HookRec *hrp;
	CFABPtr cfab;			// Pointer to the requested macro.
	Datum *datp;

	// First get the hook to set.
	if(dnewtrk(&datp) != 0)
		return drcset();
	if(opflags & OpScript)
		datxfer(datp,argpp[0]);
	else if(terminp(datp,text318,NULL,RtnKey,0,CFNotNull1,0) != Success || datp->d_type == dat_nil)
			// "Set hook "
		return rc.status;

	// Valid?
	if((opflags & (OpScript | OpEval)) != OpScript && findhook(datp->d_str,&hrp) != Success)
		return rc.status;

	// Get the macro name.  If interactive mode, build "progress" prompt first.
	if(opflags & OpScript) {
		if(!needsym(s_comma,true))
			return rc.status;
		if(havesym(kw_nil,false)) {
			cfab.u.p_bufp = NULL;
			(void) getsym();
			}
		else
			(void) getcfm(NULL,&cfab,PtrMacro);
		}
	else {
		char wkbuf[NWork + 1];

		if(mlputs(MLForce,datp->d_str) != Success)
			return rc.status;
		sprintf(wkbuf,"%s%s %s %s",text318,datp->d_str,text339,text336);
			// "Assign hook ","to","macro",
		(void) getcfm(wkbuf,&cfab,PtrMacro);
		}

	// Unless script mode and not evaluating, set the hook.
	if(rc.status == Success && (opflags & (OpScript | OpEval)) != OpScript)
		hrp->h_bufp = cfab.u.p_bufp;

	return rc.status;
	}

// Build and pop up a buffer containing a list of all screens and their associated buffers.  Render buffer and return status.
int showHooks(Datum *rp,int n,Datum **argpp) {
	Buffer *slistp;
	HookRec *hrp = hooktab;
	DStrFab rpt;

	// Get a buffer and open a string-fab object.
	if(sysbuf(text316,&slistp) != Success)
			// "Hooks"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	// Construct the header lines.
	if(dputs(text315,&rpt) != 0 || dputc('\n',&rpt) != 0 ||
	    // "Hook Name    Set to"
	 dputs("----------   --------------------",&rpt) != 0)
		return drcset();

	// For all hooks...
	do {
		if(dputf(&rpt,"\n%-13s%s",hrp->h_name,hrp->h_bufp == NULL ? "" : hrp->h_bufp->b_bname + 1) != 0)
			return drcset();

		// On to the next hook.
		} while((++hrp)->h_name != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(bappend(slistp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,slistp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}

// Disable a hook that went awry.  Return status.
static int dishook(HookRec *hrp) {
	DStrFab msg;
	int rcode = 0;

	// Hook execution failed, let the user know what's up.
	if(rc.status > FatalError) {
		if((rcode = dopenwith(&msg,&rc.msg,disnull(&rc.msg) ? false : true)) == 0) {
			if(disempty(&msg))
				rcode = dputf(&msg,text176,hrp->h_bufp->b_bname + 1);
						 // "Macro '%s' failed"
#if MMDebug & Debug_Datum
			if(rcode == 0) {
				fprintf(logfile,"dishook(): calling dputf(...,%s,%s)...\n",text161,hrp->h_name);
				rcode = dputf(&msg,text161,hrp->h_name);
				ddump(msg.sf_datp,"dishook(): post dputf()...");
				fflush(logfile);
				}
			if(rcode == 0) {
				rcode = dclose(&msg,sf_string);
				fprintf(logfile,"dishook(): dclose() returned rc.msg '%s', status %d.\n",rc.msg.d_str,
				 rc.status);
				fflush(logfile);
				}
#else
			if(rcode == 0 && (rcode = dputf(&msg,text161,hrp->h_name)) == 0)
							// " (disabled '%s' hook)"
				rcode = dclose(&msg,sf_string);
#endif
			}
		}

	// disable the hook...
	hrp->h_bufp = NULL;

	// and set Datum object error if needed.
	return (rcode == 0) ? rc.status : drcset();
	}

// Execute a macro bound to a hook with rp (if not NULL) and n.  If arginfo == 0, invoke the macro directly; otherwise, build a
// command line with arguments.  In the latter case, arginfo contains the argument count and types of each argument (string,
// long, or Datum *).  The lowest four bits of arginfo is the argument count.  Each pair of higher bits is 0 for a string
// argument, 1 for a long (integer) argument, and 2 for a Datum pointer argument.
int exechook(Datum *rp,int n,HookRec *hrp,uint arginfo,...) {
	bool fscall = false;		// execCF() or doestmt() call?

	// Unbound hook?
	if(hrp->h_bufp == NULL)
		return rc.status;
	if(rp == NULL && dnewtrk(&rp) != 0)		// For throw-away return value.
		goto DFail;

	// Build command line if requested.
	if(arginfo != 0) {
		va_list ap;
		DStrFab cmd;
		char *str;
		int delim = ' ';
		uint argct = arginfo & 0xf;

		// Concatenate macro name with arguments...
		if(dopentrk(&cmd) != 0 || (n != INT_MIN && dputf(&cmd,"%d => ",n) != 0) ||
		 dputs(hrp->h_bufp->b_bname + 1,&cmd) != 0)
			goto DFail;
		va_start(ap,arginfo);
		arginfo >>= 4;
		do {
			if(dputc(delim,&cmd) != 0)
				goto DFail;
			switch(arginfo & 3) {
				case 0:			// string argument.
					if((str = va_arg(ap,char *)) == NULL) {
						if(dputs(viz_nil,&cmd) != 0)
							goto DFail;
						}
					else if(quote(&cmd,str,true) != Success)
						goto Fail;
					break;
				case 1:			// long argument.
					if(dputf(&cmd,"%ld",va_arg(ap,long)) != 0)
						goto DFail;
					break;
				default:		// Datum *
					if(dtosfc(&cmd,va_arg(ap,Datum *),NULL,CvtExpr) != Success)
						goto Fail;
				}
			arginfo >>= 2;
			delim = ',';
			} while(--argct > 0);
		va_end(ap);

		// and execute result.
		if(dclose(&cmd,sf_string) != 0)
			goto DFail;
#if MMDebug & Debug_Datum
		fprintf(logfile,"exechook(): calling doestmt(%s)...\n",cmd.sf_datp->d_str);
		(void) doestmt(rp,cmd.sf_datp->d_str,TokC_Comment,NULL);
		fprintf(logfile,"exechook(): doestmt() returned rc.msg '%s', status %d.\n",
		 rc.msg.d_str,rc.status); fflush(logfile);
		if(rc.status != Success)
#else
		if(doestmt(rp,cmd.sf_datp->d_str,TokC_Comment,NULL) != Success)
#endif
			goto Fail;
		fscall = true;
		}
	else {
		uint oldnoload = opflags & OpNoLoad;
		opflags |= OpNoLoad;
		(void) dobuf(rp,n,hrp->h_bufp,NULL,0);
		opflags = (opflags & ~OpNoLoad) | oldnoload;
		if(rc.status != Success)
			goto Fail;
		}

	// Successful execution.  Save return message and check for false return.
	if((!fscall && (opflags & OpScript) && rcsave() != Success) || rp->d_type != dat_false)
		return rc.status;	// Fatal error or success.

	// Hook returned false... big trouble.
	(void) rcset(Failure,RCNoFormat | RCKeepMsg,text300);
			// "False return"
Fail:
	// Execution failed, let the user know what's up.
	return dishook(hrp);
DFail:
	return drcset();
	}

// Execute a named command, alias, or macro interactively even if it is not bound or is being invoked from a macro.
int run(Datum *rp,int n,Datum **argpp) {
	uint oldscript;
	CFABPtr cfab;		// Pointer to command, alias, or macro to execute.

	// If we are in script mode, force the command interactively.
	if((oldscript = opflags & OpScript)) {

		// Grab next symbol...
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;

		// look it up...
		if(opflags & OpEval) {
			if(cfabsearch(last->p_tok.d_str,&cfab,PtrCmd | PtrAlias | PtrMacro) != 0)
				return rcset(Failure,0,text244,last->p_tok.d_str);
					// "No such command, alias, or macro '%s'"
			if(cfab.p_type == PtrAlias_F)
				return rcset(Failure,0,text314,last->p_tok.d_str);
					// "Cannot execute alias '%s' interactively"
			}

		// and get next symbol.
		if(getsym() < NotFound)
			return rc.status;

		// If not evaluating, bail out here.
		if(!(opflags & OpEval))
			return rc.status;

		// Otherwise, prepare to execute the CAM -- INTERACTIVELY.
		opflags &= ~OpScript;
		}

	// Prompt the user to type a named command, alias, or macro.
	else if(getcfam(": ",PtrCmd | PtrAlias_C | PtrAlias_M | PtrMacro,&cfab,text244,text314) != Success ||
			// "No such command, alias, or macro '%s'","Cannot execute alias '%s' interactively"
	 cfab.p_type == PtrNul)
		return rc.status;

	// Execute it.
	bool fevalcall = false;
	if(cfab.p_type & PtrAlias)
		cfab = cfab.u.p_aliasp->a_cfab;
	if(cfab.p_type == PtrMacro)
		(void) dobuf(rp,n,cfab.u.p_bufp,NULL,opflags & OpParens ? SRun_Parens : 0);	// Call the macro.
	else {											// Call the command function.
		CmdFunc *cfp = cfab.u.p_cfp;
		if(n != 0 || !(cfp->cf_aflags & CFNCount))
			if(allowedit(cfp->cf_aflags & CFEdit) == Success) {
				(void) execCF(rp,n,cfp,0,0);
				fevalcall = true;
				}
		}
	if(rc.status == Success && oldscript && !fevalcall)
		(void) rcsave();

	// Clean up and return result.
	opflags = (opflags & ~OpScript) | oldscript;
	return rc.status;
	}

// Concatenate all arguments and execute string result.  If interactive and non-default n, evaluate input string before
// executing it.  Return status.
int eval(Datum *rp,int n,Datum **argpp) {
	Datum *datp;

	// Get the command line.
	if(dnewtrk(&datp) != 0)
		return drcset();
	if(opflags & OpScript) {

		// Concatenate all arguments into datp.
		if(catargs(datp,1,NULL,0) != Success)
			return rc.status;
		}
	else if(terminp(datp,": ",NULL,RtnKey,0,CFNotNull1,n == INT_MIN ? 0 : Term_Eval) != Success ||
	 datp->d_type == dat_nil)
		return rc.status;

	// Execute command line.
#if MMDebug & Debug_Script
	fprintf(logfile,"eval(): Calling doestmt(\"%s\")...\n",datp->d_str);
#endif
	return doestmt(rp,datp->d_str,TokC_Comment,NULL);
	}

#if MMDebug & Debug_MacArg
// Verify that a valid macro argument list exists.
static void margcheck(int n,Buffer *bufp,MacArgList *malp) {
	int listnum = 1;
	MacArg *margp;

	fprintf(logfile,"-----\nmargcheck(): n %d, buf '%s'\n",n,bufp->b_bname);
	fprintf(logfile,
	 "Macro argument list %.8X (%2d):\n\tmal_count: %hu\n\tmal_headp: %.8X\n\tmal_argp: %.8X\n",
	 (uint) malp,--listnum,malp->mal_count,(uint) malp->mal_headp,(uint) malp->mal_argp);
	for(margp = malp->mal_headp; margp; margp = margp->ma_nextp) {
		fprintf(logfile,"\t*Arg %2hu:\n\t\tma_nextp: %.8X\n\t\tma_datp (",margp->ma_num,(uint) margp->ma_nextp;
		if(margp->ma_datp->d_type == dat_int)
			fprintf(logfile,"int): %ld\n",margp->ma_datp->u.d_int);
		else
			fprintf(logfile,"string): '%s'\n",margp->ma_datp->d_str);
		}
	fflush(logfile);
	}
#endif

// Load macro arguments into an array and save in "margs".  If in script mode, evaluate some or all of remaining arguments (if
// any) on command line, dependent on number specified in Buffer record; otherwise, create empty list.  Return status.
#if MMDebug & Debug_MacArg
static int margload(int n,Buffer *bufp,Datum *margs,uint flags) {
#else
static int margload(Buffer *bufp,Datum *margs,uint flags) {
#endif
	Array *aryp;

#if MMDebug & Debug_MacArg
	fprintf(logfile,"*margload(%d,%s,%.4x)...\n",n,bufp->b_bname,flags);
	fflush(logfile);
#endif
	// Create array for argument list (which may remain empty).
	if((aryp = anew(0,NULL)) == NULL)
		return drcset();
	if(awrap(margs,aryp) != Success)
		return rc.status;

	// Don't get arguments (that don't exist) if executing a hook; that is, OpNoLoad is set.
	if((opflags & (OpScript | OpNoLoad)) == OpScript) {
		Datum *argp;
		int argct = bufp->b_mip == NULL ? -1 : bufp->b_mip->mi_nargs;
		int reqct = (argct >= 0) ? argct : 0;
		uint aflags = Arg_First | CFBool1 | CFArray1 | CFNIS1;

		// Get arguments until none left.  Error if mi_nargs > 0 and count != mi_nargs.
		if(dnewtrk(&argp) != 0)
			return drcset();

		// If xxx() call form, have ')' and (1), not evaluating; or (2), argct < 0; bail out (not an error).
		if(argct != 0 && (!(flags & SRun_Parens) || !havesym(s_rparen,false) || ((opflags & OpEval) && argct > 0))) {
			for(;;) {
				// Get next argument.
				if(aflags & Arg_First) {
					if(!havesym(s_any,reqct > 0)) {
						if(rc.status != Success)
							return rc.status;	// Error.
						break;				// No arguments left.
						}
					}
				else if(!havesym(s_comma,false))
					break;					// No arguments left.

				// We have a symbol.  Get expression.
				if(funcarg(argp,aflags) != Success)
					return rc.status;
				aflags = CFBool1 | CFArray1 | CFNIS1;
				--reqct;

				// Got an argument.  If not in "consume" mode, add it to array.
				if((opflags & OpEval) && apush(aryp,argp) != 0)
					return drcset();
				}
			if(reqct > 0 || (reqct < 0 && argct > 0))
				return rcset(Failure,0,text69,last->p_tok.d_str);
					// "Wrong number of arguments (at token '%s')"
			}
		}
	opflags &= ~OpNoLoad;
#if MMDebug & Debug_MacArg
	margcheck(n,bufp,malp);
#endif
	return rc.status;
	}

// Parse and execute a string as an expression statement, given return datum object (which is assumed to be a null string),
// string pointer, terminator character, and optional pointer in which to return updated string pointer.  This function is
// called anytime a string needs to be executed as a "statement" (without a leading statement keyword); for example, by dobuf(),
// xeqCmdLine(), "#{...}", or the -e switch at startup.  lastflag and thisflag are also updated.
int doestmt(Datum *rp,char *cl,int termch,char **clp) {
	uint oldscript;
	Parse *oldlast,newlast;
	ENode node;

#if MMDebug & (Debug_Datum | Debug_Script)
	fprintf(logfile,"doestmt(%.8X,\"%s\",%.4x,%.8X)...\n",(uint) rp,cl,termch,(uint) clp); fflush(logfile);
#endif
	// Begin new command line parsing "instance".
	oldlast = last;
	if(parsebegin(&newlast,cl,termch) != Success)
		return rc.status;

	// Save old "script mode" flag and enable it.
	oldscript = opflags & OpScript;
	opflags = (opflags & ~OpParens) | OpScript;

	// Set up the default command values.
	kentry.lastflag = kentry.thisflag;
	kentry.thisflag = 0;

	// Evaluate the line (as an expression).
	nodeinit(&node,rp,true);
#if MMDebug & Debug_Expr
	fprintf(logfile,"doestmt(): calling ge_andor() on token '%s', line '%s'...\n",last->p_tok.d_str,last->p_cl);
#endif
	if(ge_andor(&node) == Success)
		if(!extrasym()) {			// Error if extra symbol(s).
			if(termch == TokC_ExprEnd &&	// Check if parsing "#{}" and right brace missing.
			 *last->p_cl != TokC_ExprEnd)
				(void) rcset(Failure,0,text173,TokC_Expr,TokC_ExprBegin,TokC_ExprEnd);
						// "Unbalanced %c%c%c string parameter"
			}

#if MMDebug & Debug_Expr
	fprintf(logfile,"doestmt(): Returning status %hd\n",rc.status);
	fflush(logfile);
#endif
	// Restore settings and return.
	if(clp != NULL)
		*clp = last->p_cl;
	parseend(oldlast);
	opflags = (opflags & ~OpScript) | oldscript;

	return rc.status;
	}

// Find an alias by name and return status or boolean result.  (1), if the alias is found: if op is OpQuery or OpCreate, set
// *app (if app not NULL) to the alias structure associated with it; otherwise (op is OpDelete), delete the alias and the
// associated CFAM record.  If op is OpQuery return true; othewise, return status.  (2), if the alias is not found: if op is
// OpCreate, create a new entry, set its pointer record to *cfabp, and set *app (if app not NULL) to it; if op is OpQuery,
// return false, ignoring app; otherwise, return an error.
int afind(char *aname,int op,CFABPtr *cfabp,Alias **app) {
	Alias *ap1,*ap2;
	int result;

	// Scan the alias list.
	ap1 = NULL;
	ap2 = aheadp;
	while(ap2 != NULL) {
		if((result = strcmp(ap2->a_name,aname)) == 0) {

			// Found it.  Check op.
			if(op == OpDelete) {

				// Delete the CFAM record.
				if(amfind(aname,OpDelete,0) != Success)
					return rc.status;

				// Decrement alias use count on macro, if applicable.
				if(ap2->a_cfab.p_type == PtrMacro)
					--ap2->a_cfab.u.p_bufp->b_nalias;

				// Delete the alias from the list and free the storage.
				if(ap1 == NULL)
					aheadp = ap2->a_nextp;
				else
					ap1->a_nextp = ap2->a_nextp;
				free((void *) ap2);

				return rc.status;
				}

			// Not a delete.  Return it.
			if(app != NULL)
				*app = ap2;
			return (op == OpQuery) ? true : rc.status;
			}
		if(result > 0)
			break;
		ap1 = ap2;
		ap2 = ap2->a_nextp;
		}

	// No such alias exists, create it?
	if(op == OpCreate) {
		enum e_sym sym;
		char *str;

		// Valid identifier name?
		str = aname;
		if(((sym = getident(&str,NULL)) != s_ident && sym != s_identq) || *str != '\0')
			(void) rcset(Failure,0,text286,aname);
				// "Invalid identifier '%s'"

		// Allocate the needed memory.
		if((ap2 = (Alias *) malloc(sizeof(Alias) + strlen(aname))) == NULL)
			return rcset(Panic,0,text94,"afind");
				// "%s(): Out of memory!"

		// Find the place in the list to insert this alias.
		if(ap1 == NULL) {

			// Insert at the beginning.
			ap2->a_nextp = aheadp;
			aheadp = ap2;
			}
		else {
			// Insert after ap1.
			ap2->a_nextp = ap1->a_nextp;
			ap1->a_nextp = ap2;
			}

		// Set the other record members.
		strcpy(ap2->a_name,aname);
		ap2->a_cfab = *cfabp;
		ap2->a_type = cfabp->p_type == PtrCmd ? PtrAlias_C : cfabp->p_type == PtrFunc ? PtrAlias_F : PtrAlias_M;

		// Add its name to the CFAM list.
		if(amfind(ap2->a_name,OpCreate,ap2->a_type) != Success)
			return rc.status;

		if(app != NULL)
			*app = ap2;
		return rc.status;
		}

	// Alias not found and not a create.
	return (op == OpQuery) ? false : rcset(Failure,0,text271,aname);
							// "No such alias '%s'"
	}

// Create an alias to a command, function, or macro.
int aliasCFM(Datum *rp,int n,Datum **argpp) {
	CFABPtr cfab;
	Datum *dnamep;

	// Get the alias name.
	if(dnewtrk(&dnamep) != 0)
		return drcset();
	if(opflags & OpScript) {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(dsetstr(last->p_tok.d_str,dnamep) != 0)
			return drcset();
		}
	else if(terminp(dnamep,text215,NULL,RtnKey,0,CFNotNull1,0) != Success || dnamep->d_type == dat_nil)
			// "Create alias "
		return rc.status;

	// Existing function, alias, macro, or user variable of same name?
	if((opflags & OpEval) && (cfabsearch(dnamep->d_str,NULL,PtrCFAM) == 0 || uvarfind(dnamep->d_str) != NULL))
		return rcset(Failure,0,text165,dnamep->d_str);
			// "Name '%s' already in use"

	if(opflags & OpScript) {

		// Get equal sign.
		if(getsym() < NotFound || !havesym(s_any,true))
			return rc.status;
		if(last->p_sym != s_assign)
			return rcset(Failure,0,text23,(cftab + cf_alias)->cf_name,last->p_tok.d_str);
				// "Missing '=' in %s command (at token '%s')"
		if(getsym() < NotFound)			// Past '='.
			return rc.status;

		// Get the command, function, or macro name.
		(void) getcfm(NULL,&cfab,PtrCmd | PtrFunc | PtrMacro);
		}
	else {
		char wkbuf[strlen(text215) + strlen(dnamep->d_str) + strlen(text325) + strlen(text313) + 8];

		// Get the command, function, or macro name.
		sprintf(wkbuf,"%s%s %s %s",text215,dnamep->d_str,text325,text313);
				// "Create alias ","for","command, function, or macro"
		(void) getcfm(wkbuf,&cfab,PtrCmd | PtrFunc | PtrMacro);
		}

	if(rc.status == Success && cfab.p_type != PtrNul) {

		// Create the alias.
		if(afind(dnamep->d_str,OpCreate,&cfab,NULL) != Success)
			return rc.status;

		// Increment alias count on macro.
		if(cfab.p_type == PtrMacro)
			++cfab.u.p_bufp->b_nalias;
		}

	return rc.status;
	}

// Delete one or more aliases or macros.  Return status.
int deleteAM(char *prmt,uint selector,char *emsg) {
	CFABPtr cfab;

	// If interactive, get alias or macro name from user.
	if(!(opflags & OpScript)) {
		if(getcfam(prmt,selector,&cfab,emsg,NULL) != Success || cfab.p_type == PtrNul)
			return rc.status;
		goto NukeIt;
		}

	// Script mode: get alias(es) or macro(s) to delete.
	do {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(opflags & OpEval) {
			if(cfabsearch(last->p_tok.d_str,&cfab,selector) != 0)
				return rcset(Failure,0,emsg,last->p_tok.d_str);
NukeIt:
			// Nuke it.
			if(selector == PtrMacro) {
				if(bdelete(cfab.u.p_bufp,CLBIgnChgd) != Success)
					break;
				}
			else if(afind(cfab.u.p_aliasp->a_name,OpDelete,NULL,NULL) != Success)
				break;
			}
		} while((opflags & OpScript) && getsym() == Success && needsym(s_comma,false));

	return rc.status;
	}

// Delete one or more alias(es).  Return status.
int deleteAlias(Datum *rp,int n,Datum **argpp) {

	return deleteAM(text269,PtrAlias,text271);
		// "Delete alias","No such alias '%s'"
	}

// Delete one or more macros.  Return status.
int deleteMacro(Datum *rp,int n,Datum **argpp) {

	return deleteAM(text216,PtrMacro,text116);
		// "Delete macro","No such macro '%s'"
	}

// Execute the contents of a buffer (of commands) and return result in rp.
int xeqBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;		// Pointer to buffer to execute.

	// Find out what buffer the user wants to execute.
	if(bcomplete(rp,text117,curbp->b_bname,OpDelete,&bufp,NULL) != Success || bufp == NULL)
			// "Execute"
		return rc.status;
	if(!(opflags & OpScript))
		mlerase(0);

	// And now execute it with arg n.
#if MMDebug & Debug_Script
	(void) dobuf(rp,n,bufp,bufp->b_fname,opflags & OpParens ? SRun_Parens : 0);
	fprintf(logfile,"*** xeqBuf(): dobuf() returned '%s' result [%d]",dtype(rp,false),rc.status);
	if(rc.status != Success)
		fprintf(logfile," \"%s\"",rc.msg.d_str);
	fputc('\n',logfile);
	return rc.status;
#else
	return dobuf(rp,n,bufp,bufp->b_fname,opflags & OpParens ? SRun_Parens : 0);
#endif
	}

// Skip white space in a fixed-length string, given pointer to source pointer and length.  Update pointer to point to the first
// non-white character (or end of string if none) and return number of bytes remaining.
static int skipwhite(char **strp,int len) {
	char *str = *strp;

	while(len > 0 && (*str == ' ' || *str == '\t')) {
		++str;
		--len;
		}
	*strp = str;
	return len;
	}

// Look up a statement keyword, given indirect pointer to beginning of command line and length.  Update *clp and return symbol
// id if found; otherwise, s_nil.
static enum e_sym kwfind(char **clp,int len) {
	char *cl = *clp;

	if(is_lower(*cl)) {
		enum e_sym s;
		ushort kwlen;
		char wkbuf[StmtKeywordMax + 2],*str = wkbuf;

		// Copy enough of line into wkbuf (null terminated) to do keyword comparisons.
		stplcpy(wkbuf,cl,(uint) len < (sizeof(wkbuf) - 1) ? (uint) len + 1 : sizeof(wkbuf));

		// Check if line begins with a statement keyword.
		if((s = getident(&str,&kwlen)) >= kw_break) {
			*clp = cl + kwlen;
			return s;
			}
		}

	return s_nil;
	}

// Build a macro execution error message which includes the buffer name and line currently being executed, and set via rcset().
// If emsg0 is NULL, use rc.msg as the initial portion; if rc.msg is null, use a generic error message.  Return status.
static int macerror(char *emsg0,char *emsg1,Buffer *bufp,Line *lnp,uint flags) {
	int rcode;
	DStrFab msg;
	char *name,*label;

	if((flags & SRun_Startup) && bufp->b_fname != NULL) {
		name = bufp->b_fname;
		label = text99;
			// "file"
		}
	else {
		name = bufp->b_bname;
		label = text83;
			// "buffer"
		}

	// Build error message line.  emsg0 may be long and may be truncated when displayed on message line, but so be it.  The
	// first portion of the message is the most important to see.
	if((rcode = dopentrk(&msg)) == 0) {

		// Prepend emsg0 and emsg1.
		rcode = (emsg1 == NULL) ? dputs(emsg0 != NULL ? emsg0 : disnull(&rc.msg) ? text219 : rc.msg.d_str,&msg) :
										// "Script failed"
		 dputf(&msg,emsg0,emsg1);
		if(rcode == 0 &&
		 (rcode = dputf(&msg,"%s %s '%s' %s %ld",text229,label,name,text230,getlinenum(bufp,lnp))) == 0 &&
						// ", in","at line"
		 (rcode = dclose(&msg,sf_string)) == 0)
			(void) rcset(ScriptError,RCForce,"%s",msg.sf_datp->d_str);
		}

	return (rcode == 0) ? rc.status : drcset();
	}

// Free a list of loop block pointers, given head of list.
static void lbfree(LoopBlock *lbp) {
	LoopBlock *lbp0;

	while((lbp0 = lbp) != NULL) {
		lbp = lbp0->lb_next;
		free((void *) lbp0);
		}
	}

// Free any macro preprocessor storage in a buffer.
void ppfree(Buffer *bufp) {
	MacInfo *mip = bufp->b_mip;
	if(mip != NULL) {
		if(mip->mi_execp != NULL) {
			lbfree(mip->mi_execp);
			mip->mi_execp = NULL;
			}
		dclear(&mip->mi_usage);
		dclear(&mip->mi_desc);
		bufp->b_flags &= ~BFPreproc;		// Clear "preprocessed" flag.
		}
	}

#if MMDebug & Debug_PPBuf
static void lbdump(Line *lnp,char *label) {

	fprintf(logfile,"%s: [%.8X] ",label,(uint) lnp);
	if(lnp == NULL)
		fputs("NULL\n",logfile);
	else {
		char *str = ltext(lnp);
		int len = skipwhite(&str,lused(lnp));
		fprintf(logfile,"%.*s\n",len,str);
		}
	}
#endif

// Preprocess a buffer and return status.  If no errors found, set bufp->b_mip->mi_execp to result.  bufp->b_mip is assumed to
// not be NULL.
static int ppbuf(Buffer *bufp,uint flags) {
	Line *lnp;			// Pointer to line to execute.
	Line *hlp;			// Pointer to line header.
	enum e_sym kwid;		// Statement keyword ID
	int len;			// Line length.
	int saltlevel;			// "macro" nesting depth (for error checking).
	LoopBlock *lbexec;		// Pointer to execution loop-block list.
	LoopBlock *lbopen;		// Pointer during scan.
	LoopBlock *lbtemp;		// Temporary pointer.
	char *eline,*eline1;		// Text of line to scan.
	bool skipLine;
	bool lastWasCL = false;		// Last line was a continuation line.

	// Scan the buffer to execute, building loop blocks for any loop statements found, regardless of "truth" state (which is
	// unknown at this point).  This allows for any loop block to be executed at execution time.
	lbexec = lbopen = NULL;
	saltlevel = 0;
	hlp = bufp->b_hdrlnp;
	for(lnp = lforw(hlp); lnp != hlp; lnp = lforw(lnp)) {

		// Scan the current line.
		eline = ltext(lnp);
		len = lused(lnp);
#if MMDebug & Debug_PPBuf
		fprintf(logfile,"%.8X >> %.*s\n",(uint) lnp,len,eline);
#endif
		// Skip line if last line was a continuation line.
		skipLine = lastWasCL;
		lastWasCL = len > 0 && eline[len - 1] == '\\';
		if(skipLine)
			continue;

		// Find first non-whitespace character.  If line is blank or a comment, skip it.
		if((len = skipwhite(&eline,len)) == 0 || *eline == TokC_Comment)
			continue;

		// Check for a statement keyword.
		eline1 = eline;
		if((kwid = kwfind(&eline1,len)) != s_nil) {
			switch(kwid) {
				case kw_macro:		// macro keyword: bump salting level.
					++saltlevel;
					break;
				case kw_endmacro:	// endmacro keyword: orphan?
					if(--saltlevel < 0) {
						eline = text121;
							// "Unmatched '%s' keyword"
						eline1 = text197;
							// "endmacro"
						goto FailExit;
						}
					break;
				case kw_for:
				case kw_loop:
				case kw_while:
				case kw_until:		// while, until, for, or loop keyword: make a block.
					goto NewBlock;
				case kw_break:
				case kw_next:		// "break" or "next" keyword: orphan?
					if(lbopen == NULL) {
						eline = text120;
							// "'break' or 'next' outside of any loop block"
						eline1 = NULL;
						goto FailExit;
						}
NewBlock:
					// Not an orphan, make a block.
					if((lbtemp = (LoopBlock *) malloc(sizeof(LoopBlock))) == NULL)
						return rcset(Panic,0,text94,"ppbuf");
							// "%s(): Out of memory!"

					// Record line and block type.
					lbtemp->lb_mark = lnp;
					lbtemp->lb_break = lbtemp->lb_jump = NULL;
					lbtemp->lb_type = kwid;

					// Add the block to the open list.
					lbtemp->lb_next = lbopen;
					lbopen = lbtemp;
					break;
				case kw_endloop:	// "endloop" keyword: orphan?
					if(lbopen == NULL) {
						eline = text121;
							// "Unmatched '%s' keyword"
						eline1 = text196;
							// "endloop"
						goto FailExit;
						}

					// Loop through records (including any BREAK or NEXT records) in the lbopen stack,
					// record this line in them, and move them to the top of the lbexec list until one loop
					// (SLoopType) record is moved.  This will complete the most recent SLoopType block.
					do {
						lbopen->lb_jump = lnp;
						if(lbopen->lb_type & SLoopType) {
							lbtemp = lbopen->lb_next;

							// For loop records, record the marker line (temporarily) of the
							// *parent* loop record in this record's lb_break member so that it can
							// be set later to the parent's ENDLOOP line (which is needed for
							// multi-level breaks).  If there is no parent loop block, the lb_break
							// member will remain NULL, which is okay.
							while(lbtemp != NULL) {
								if(lbtemp->lb_type & SLoopType) {

									// Found parent loop block.  Save marker line pointer.
									lbopen->lb_break = lbtemp->lb_mark;
									break;
									}
								lbtemp = lbtemp->lb_next;
								}
							}
						lbtemp = lbexec;
						lbexec = lbopen;
						lbopen = lbopen->lb_next;
						lbexec->lb_next = lbtemp;
						} while(!(lbexec->lb_type & SLoopType));
					break;
				default:
					;	// Do nothing.
				}
			}

		// On to the next line.
		}

	// Buffer SCAN completed.  while/until/for/loop and "endloop" match?
	if(lbopen != NULL) {
		eline = text121;
			// "Unmatched '%s' keyword"
		eline1 = text122;
			// "loop"
		goto FailExit;
		}

	// "macro" and "endmacro" match?
	if(saltlevel > 0) {
		eline = text121;
			// "Unmatched '%s' keyword"
		eline1 = text336;
			// "macro"
FailExit:
		(void) macerror(eline,eline1,bufp,lnp,flags);
		goto Exit;
		}

	// Everything looks good.  Last step is to fix up the loop records.
	for(lbtemp = lbexec; lbtemp != NULL; lbtemp = lbtemp->lb_next) {
		if((lbtemp->lb_type & SLoopType) && lbtemp->lb_break != NULL) {
			LoopBlock *lbtemp2 = lbexec;

			// Find loop block ahead of this block that contains this block's marker.
			do {
				if(lbtemp2->lb_mark == lbtemp->lb_break) {

					// Found parent block.  Set this loop's lb_break to parent's
					// ENDLOOP line (instead of the top marker line).
					lbtemp->lb_break = lbtemp2->lb_jump;
					goto NextFix;
					}
				} while((lbtemp2 = lbtemp2->lb_next) != NULL && lbtemp2->lb_mark != lbtemp->lb_mark);

			// Huh?  Matching loop block not found! (This is a bug.)
			(void) rcset(Failure,0,text220,getlinenum(bufp,lbtemp->lb_mark));
				// "Parent block of loop block at line %ld not found during buffer scan"
			goto RCExit;
			}
NextFix:;
		}

#if MMDebug & Debug_PPBuf
	// Dump blocks to log file.
	fputs("\nLOOP BLOCKS\n",logfile);
	for(lbtemp = lbexec; lbtemp != NULL; lbtemp = lbtemp->lb_next) {
		char *str;
		switch(lbtemp->lb_type) {
			case kw_while:
				str = "while";
				break;
			case kw_until:
				str = "until";
				break;
			case kw_for:
				str = "for";
				break;
			case kw_loop:
				str = "loop";
				break;
			case kw_break:
				str = "break";
				break;
			case kw_next:
				str = "next";
				break;
			default:
				str = "UNKNOWN";
				break;
			}
		fprintf(logfile,"-----\n Type: %.4x %s\n",lbtemp->lb_type,str);
		lbdump(lbtemp->lb_mark," Mark");
		lbdump(lbtemp->lb_jump," Jump");
		lbdump(lbtemp->lb_break,"Break");
		}
//	return rcset(FatalError,RCNoFormat,"ppbuf() exit");
#endif
	// Success!  Save block pointer in buffer-extension record and set "preprocessed" flag.
	bufp->b_mip->mi_execp = lbexec;
	bufp->b_flags |= BFPreproc;
	goto Exit;

	// Clean up and exit per rc.status.
RCExit:
	lbfree(lbopen);
	(void) macerror(NULL,NULL,bufp,lnp,flags);
Exit:
	if(rc.status == Success)
		return rc.status;
	lbfree(lbexec);
	return rc.status;
	}

// Free loop level storage.
static void llfree(LevelInfo *elevp) {
	LevelInfo *elevp1;

	do {
		elevp1 = elevp->nextp;
		if(elevp->flip != NULL)
			free((void *) elevp->flip);
		free((void *) elevp);
		} while((elevp = elevp1) != NULL);
	}

// Move forward to a new execution level, creating a level object if needed.
static int nextlevel(LevelInfo **elevpp) {
	LevelInfo *elevp = *elevpp;

	if(elevp == NULL || elevp->nextp == NULL) {
		LevelInfo *elevp1;

		if((elevp1 = (LevelInfo *) malloc(sizeof(LevelInfo))) == NULL)
			return rcset(Panic,0,text94,"nextlevel");
				// "%s(): Out of memory!"
		elevp1->flip = NULL;
		elevp1->nextp = NULL;
		if(elevp == NULL) {
			elevp1->prevp = NULL;
#if MMDebug & (Debug_Script | Debug_PPBuf)
			elevp1->level = 0;
#endif
			}
		else {
			elevp->nextp = elevp1;
			elevp1->prevp = elevp;
#if MMDebug & (Debug_Script | Debug_PPBuf)
			elevp1->level = elevp->level + 1;
#endif
			}
		elevp = elevp1;
		}
	else {
		elevp = elevp->nextp;
		if(elevp->flip != NULL) {
			free((void *) elevp->flip);
			elevp->flip = NULL;
			}
		}

	elevp->ifwastrue = elevp->elseseen = false;
	elevp->loopcount = 0;
	*elevpp = elevp;

	return rc.status;
	}

// Return to the most recent loop level (bypassing "if" levels).  Top level should never be reached.
static int prevlevel(LevelInfo **elevpp) {
	LevelInfo *elevp = *elevpp;

	for(;;) {
		if(elevp->prevp == NULL)

			// Huh?  Loop level not found! (This is a bug.)
			return rcset(Failure,RCNoFormat,text114);
				// "Prior loop execution level not found while rewinding stack" 
		if(elevp->loopspawn)
			break;
		elevp = elevp->prevp;
		}
	*elevpp = elevp;
	return rc.status;
	}

// Parse one macro usage string.
static int getmactext(Datum *datp,enum e_sym sym,ushort *np) {
	static char kw1[] = MacKeyword1;
	static char kw2[] = MacKeyword2;
	char wkbuf[32];

	if(!needsym(sym,true) || !havesym(s_ident,true))
		return rc.status;
	if(strcmp(last->p_tok.d_str,kw1) == 0) {
		if(*np == 1)
			goto DupErr;
		*np = 1;
		}
	else if(strcmp(last->p_tok.d_str,kw2) == 0) {
		if(*np == 2) {
DupErr:
			sprintf(wkbuf,"'%s'",*np == 2 ? kw1 : kw2);
			goto ErrRtn;
			}
		*np = 2;
		}
	else {
		sprintf(wkbuf,"'%s' or '%s'",kw1,kw2);
ErrRtn:
		return rcset(Failure,0,text4,wkbuf,last->p_tok.d_str);
				// "%s expected (at token '%s')"
		}
	if(getsym() == Success && needsym(s_colon,true))
		(void) funcarg(datp,Arg_First | CFNotNull1);
	return rc.status;
	}

// Parse first line of a macro definition and return status.  Example definition:
//	macro grepFiles(2)\
//	 {usage: 'pat,glob',desc: 'Find files which match pattern'}
//		...
//	endmacro
static int getmacro(Datum *rp,Buffer **mbufpp) {
	CFABPtr cfab;
	MacInfo *mip;
	bool whitespace;
	Datum *datp1,*datp2;
	ushort n1 = 0,n2 = 0;
	short argct = -1;

	if(dnewtrk(&datp1) != 0 || dnewtrk(&datp2) != 0)
		return drcset();

	// Get macro name.
	if(!havesym(s_ident,false) && !havesym(s_identq,true))
		return rc.status;
	if(strlen(last->p_tok.d_str) > NBufName - 1)
		return rcset(Failure,0,text232,last->p_tok.d_str,NBufName - 1);
			// "Macro name '%s' cannot exceed %d characters"

	// Construct the buffer header (and name) in rp temporarily.
	if(dsalloc(rp,strlen(last->p_tok.d_str) + 3) != 0)
		return drcset();
	rp->d_str[0] = TokC_Comment;
	rp->d_str[1] = SBMacro;
	strcpy(rp->d_str + 2,last->p_tok.d_str);

	// Get argument count.
	whitespace = havewhite();
	if(getsym() != NotFound) {
		if(rc.status != Success)
			return rc.status;
		if(havesym(s_lparen,false)) {
			if(whitespace)
				return rcset(Failure,0,text4,"'('"," ");
						// "%s expected (at token '%s')"
			if(getsym() < NotFound || funcarg(datp1,Arg_First | CFInt1) != Success || !needsym(s_rparen,true))
				return rc.status;
			if(datp1->u.d_int < 0)
				return rcset(Failure,0,text39,text378,(int) datp1->u.d_int,0);
					// "%s (%d) must be %d or greater","Macro argument count"
			argct = datp1->u.d_int;
			}
		if(havesym(s_any,false)) {

			// Get usage string(s).
			if(getmactext(datp1,s_lbrace,&n1) != Success)
				return rc.status;
			if(havesym(s_comma,false)) {
				n2 = n1;
				if(getmactext(datp2,s_comma,&n2) != Success)
					return rc.status;
				}
			if(!needsym(s_rbrace,true) || extrasym())
				return rc.status;
			}
		}

	// Existing command, function, alias, macro, or user variable of same name as this macro?
	if((cfabsearch(rp->d_str + 2,&cfab,PtrCFAM) == 0 && (cfab.p_type != PtrMacro ||
	 !(modetab[MdRec_Global].flags & MdClob))) || uvarfind(rp->d_str + 2) != NULL)
		return rcset(Failure,0,text165,rp->d_str + 2);
			// "Name '%s' already in use"

	// Create a hidden buffer and make sure it's empty.
	if(bfind(rp->d_str + 1,CRBCreate | CRBExtend,BFHidden | BFMacro,mbufpp,NULL) != Success ||
	 bclear(*mbufpp,CLBIgnChgd | CLBUnnarrow | CLBClrFilename) != Success)
		return rc.status;

	// Set the macro parameters.
	mip = (*mbufpp)->b_mip;
	mip->mi_nargs = argct;
	if(n1)
		datxfer(n1 == 1 ? &mip->mi_usage : &mip->mi_desc,datp1);
	if(n2)
		datxfer(n2 == 1 ? &mip->mi_usage : &mip->mi_desc,datp2);
	rp->d_str[1] = ' ';

	return rc.status;
	}

// Parse a "for" line and initialize control variables.  Return status.
static int initfor(ForLoopInfo **flipp,Datum *datp) {
	ENode node;
	ForLoopInfo *flip;

	if((flip = *flipp) == NULL) {
		if((flip = (ForLoopInfo *) malloc(sizeof(ForLoopInfo))) == NULL)
			return rcset(Panic,0,text94,"initfor");
				// "%s(): Out of memory!"
		*flipp = flip;
		}

	// Get the variable name and "in" keyword.
	if(!havesym(s_any,true) || findvar(last->p_tok.d_str,&flip->vd,OpCreate) != Success || getsym() < NotFound ||
	 !needsym(kw_in,true))
		return rc.status;

	// Get (array) expression.
	nodeinit(&node,datp,false);
	if(ge_assign(&node) != Success || extrasym() || !aryval(node.en_rp))
		return rc.status;

	// Finish up.
	flip->aryp = awptr(datp)->aw_aryp;
	flip->index = 0;
	return rc.status;
	}

// Get next array element and set "for" loop control variable to it if found; otherwise, return NotFound.  Return status.
static int nextfor(ForLoopInfo *flip) {
	Datum *datp;

	if(flip->index == flip->aryp->a_used) {
		flip->index = -1;	// Invalidate control object.
		return NotFound;
		}
	if((datp = aget(flip->aryp,flip->index++,false)) == NULL)
		return drcset();
	return putvar(datp,&flip->vd);
	}

// Execute a compiled buffer, save result in rp, and return status.
static int execbuf(Datum *rp,Buffer *bufp,uint flags) {
	Line *blnp,*elnp;		// Pointers to beginning and ending line(s) to execute, allowing for continuation lines.
	Line *hlp;			// Pointer to line header.
	int forcecmd;			// "force" statement?
	enum e_sym kwid;		// Keyword index.
	int len;			// Line length.
	bool isWhiteLine;		// True if current line is all white space or a comment.
	bool lastWasCL = false;		// Last line was a continuation line.
	bool thisIsCL = false;		// Current line is a continuation line.
	bool go;			// Used to evaluate "while" and "until" truth.
	int breaklevel;			// Number of levels to "break".
	int saltlevel;			// "macro" nesting depth.
	LevelInfo *execlevel = NULL;	// Level table.
	LevelInfo *elevp;		// Current level.
	LoopBlock *lbp;
	Buffer *mbufp = NULL;		// "macro" buffer pointer.
	Datum *datp;			// Work datum object.
	Datum *dlinep;			// Null-terminated buffer line (datum object).
	char *eline;			// Work pointer.
	char *eline1;			// Beginning of expression statement (past any statement keyword).
	char *str;
	Parse *oldlast,newlast;
	ENode node;

	// Prepare for execution.
	if(dnewtrk(&datp) != 0 || dnewtrk(&dlinep) != 0)
		return drcset();
	saltlevel = 0;				// "macro" not being "salted away".
	if(nextlevel(&execlevel) != Success)
		return rc.status;
	elevp = execlevel;
	elevp->live = true;			// Set initial (top) execution level to true.
	elevp->loopspawn = false;
	breaklevel = 0;
	kentry.thisflag = kentry.lastflag;	// Let the first command inherit the flags from the last one.
	oldlast = last;
	newlast.p_cl = NULL;

	// Now EXECUTE the buffer.
#if MMDebug & (Debug_Script | Debug_PPBuf)
	static uint instance = 0;
	uint myinstance = ++instance;
	fprintf(logfile,"*** BEGIN execbuf(%u)...\n",myinstance);
#endif
	blnp = elnp = lforw(hlp = bufp->b_hdrlnp);
	while(elnp != hlp) {

		// Skip blank lines and comments unless "salting".
		eline = ltext(elnp);
		len = lused(elnp);
#if MMDebug & (Debug_Script | Debug_PPBuf)
		fprintf(logfile,"%.8X [level %u.%u] >> %.*s\n",(uint) elnp,myinstance,elevp->level,len,eline);
#endif
		isWhiteLine = false;
		if((len = skipwhite(&eline,len)) == 0 || *eline == TokC_Comment) {
			isWhiteLine = true;
			thisIsCL = false;
			if(!lastWasCL) {
				if(mbufp != NULL)
					goto Salt0;
				goto Onward;
				}
			}
		else {
			if(lastWasCL) {
				eline = ltext(elnp);
				len = lused(elnp);
				}
			thisIsCL = (eline[len - 1] == '\\');
			}

		// Allocate eline as a Datum object, null terminated, so that it can be parsed properly by getsym().
		if(!lastWasCL) {
			if(dsetsubstr(eline,thisIsCL ? len - 1 : len,dlinep) != 0)
				goto DErr;
			blnp = elnp;
			}
		else if(!isWhiteLine) {
			DStrFab sf;

			// Append current line to dline.
			if(dopenwith(&sf,dlinep,true) != 0 || dputmem((void *) eline,thisIsCL ? len - 1 : len,&sf) != 0 ||
			 dclose(&sf,sf_string) != 0) {
DErr:
				(void) drcset();
				goto RCExit;
				}
			}

		// Check for "endmacro" keyword now so that "salting" can be turned off if needed; otherwise, check for
		// statement keywords only if current line is not a continuation line.
		eline = eline1 = dlinep->d_str;
		if((kwid = kwfind(&eline1,strlen(eline1))) != s_nil) {
			if(thisIsCL) {
				if(kwid == kw_endmacro && elevp->live && saltlevel == 1) {
					mbufp = NULL;
					goto Onward;
					}
				}
			else {
				// Have complete line in dline.  Process any "macro" and "endmacro" statements.
				if(parsebegin(&newlast,eline1,TokC_Comment) != Success)
					goto RCExit;
				switch(kwid) {
					case kw_macro:
						// Skip if current level is false.
						if(!elevp->live)
							goto Onward;

						// Create macro?
						if(++saltlevel != 1)

							// No, this is a nested macro.  Salt it away with the other lines.
							break;

						// Yes, parse line...
						if(getmacro(rp,&mbufp) != Success)
							goto RCExit;

						// and store macro name as a comment at the top of the new buffer.
						len = strlen(eline = rp->d_str);
						goto Salt1;
					case kw_endmacro:

						// Extra arguments?
						if(extrasym())
							goto RCExit;

						// Check execution level.
						if(elevp->live) {

							// Turn off storing if prior salt level was zero.
							if(--saltlevel == 0) {
								mbufp = NULL;
								goto Onward;
								}

							// "endmacro" out of place?
							if(saltlevel < 0) {
								str = "endmacro";
								eline = text198;
									// "Misplaced '%s' keyword"
								goto Misplaced;
								}
							}
						break;
					default:
						;	// Do nothing.
					}
				}
			} // End statement keyword parsing.

		// If macro store is on, just salt this (entire) line away.
		if(mbufp != NULL) {
			Line *mp;
Salt0:
			// Save line verbatim (as a debugging aid) but skip any leading tab, if possible.
			eline = ltext(elnp);
			if((len = lused(elnp)) > 0 && !lastWasCL && *eline == '\t') {
				++eline;
				--len;
				}
Salt1:
			// Allocate the space for the line.
			if(lalloc(len,&mp) != Success)
				return rc.status;

			// Copy the text into the new line.
			memcpy(mp->l_text,eline,len);
			if(eline == rp->d_str)
				dsetnil(rp);

			// Attach the line to the end of the buffer.
			mbufp->b_hdrlnp->l_prevp->l_nextp = mp;
			mp->l_prevp = mbufp->b_hdrlnp->l_prevp;
			mbufp->b_hdrlnp->l_prevp = mp;
			mp->l_nextp = mbufp->b_hdrlnp;

			goto Onward;
			}
		if(thisIsCL)
			goto Onward;

		// Not "salting" and not a "macro" or "endmacro" keyword.  Check others.
		forcecmd = false;
		if(kwid != s_nil) {
			switch(kwid) {

				// Statements in the if/else family.
				case kw_if:	// "if" keyword.

					// Begin new level.
					if(nextlevel(&elevp) != Success)
						goto RCExit;
					elevp->loopspawn = false;

					// Prior level true?
					if(elevp->prevp->live) {
EvalIf:
						// Yes, grab the value of the logical expression.
						nodeinit(&node,datp,false);
						if(ge_andor(&node) != Success || extrasym())
							goto RCExit;

						// Set this level to the result of the expression and update "ifwastrue" if the
						// result is true.
						if((elevp->live = tobool(datp)))
							elevp->ifwastrue = true;
						}
					else
						// Prior level was false so this one is, too.
						elevp->live = false;
					goto Onward;
				case kw_else:	// else keyword.

					// At top level, initiated by a loop statement, or duplicate else?
					if(elevp == execlevel || elevp->loopspawn || elevp->elseseen) {

						// Yep, bail out.
						str = "else";
						eline = text198;
							// "Misplaced '%s' keyword"
						goto Misplaced;
						}

					// Extra arguments?
					if(extrasym())
						goto RCExit;

					// Update current execution state.  Can be true only if (1), prior state is true; (2),
					// current state is false; and (3), this (possibly compound) "if" statement was never
					// true.
					elevp->live = elevp->prevp->live && !elevp->live && !elevp->ifwastrue;
					elevp->elseseen = true;
					goto Onward;
				case kw_elsif:	// "elsif" keyword.

					// At top level, initiated by a loop statement, or "elsif" follows "else"?
					if(elevp == execlevel || elevp->loopspawn || elevp->elseseen) {

						// Yep, bail out.
						str = "elsif";
						eline = text198;
							// "Misplaced '%s' keyword"
						goto Misplaced;
						}

					// Check expression if prior level is true, current level is false, and this compound if
					// statement has not yet been true.
					if(elevp->prevp->live && !elevp->live && !elevp->ifwastrue)
						goto EvalIf;

					// Not evaluating so set current execution state to false.
					elevp->live = false;
					goto Onward;
				case kw_endif:	// "endif" keyword.

					// At top level or was initiated by a loop statement?
					if(elevp == execlevel || elevp->loopspawn) {

						// Yep, bail out.
						str = "endif";
						eline = text198;
							// "Misplaced '%s' keyword"
						goto Misplaced;
						}

					// Extra arguments?
					if(extrasym())
						goto RCExit;

					// Done with current level: return to previous one.
					elevp = elevp->prevp;
					goto Onward;

				// Statements that begin or end a loop.
				case kw_loop:	// "loop" keyword.

					// Skip unless current level is true.
					if(!elevp->live)
						goto JumpDown;

					// Error if extra arguments; otherwise, begin next loop.
					if(extrasym())
						goto RCExit;
					goto NextLoop;
				case kw_for:	// "for" keyword

					// Current level true?
					if(elevp->live) {
						int status;

						// Yes, parse line and initialize loop controls if first time.
						if((elevp->flip == NULL || elevp->flip->index < 0) &&
						 initfor(&elevp->flip,datp) != Success)
							goto RCExit;

						// Set control variable to next array element and begin next loop, or break if
						// none left.
						if((status = nextfor(elevp->flip)) == Success)
							goto NextLoop;
						if(status != NotFound)
							goto RCExit;
						}
					goto JumpDown;
				case kw_while:	// "while" keyword.
					go = true;
					goto DoLoop;
				case kw_until:	// "until" keyword.
					go = false;
					// Fall through.
DoLoop:
					// Current level true?
					if(elevp->live) {

						// Yes, get the "while" or "until" logical expression.
						nodeinit(&node,datp,false);
						if(ge_andor(&node) != Success || extrasym())
							goto RCExit;

						// If the "while" condition is true or the "until" condition if false, begin a
						// new level, set it to true, and continue.
						if(tobool(datp) == go) {
NextLoop:
							if(nextlevel(&elevp) != Success)
								goto RCExit;
							elevp->live = true;
							elevp->loopspawn = true;
							goto Onward;
							}
						}

					// Current level or condition is false: skip this block.
					goto JumpDown;

				case kw_break:	// "break" keyword.
				case kw_next:	// "next" keyword.

					// Ignore if current level is false (nothing to jump out of).
					if(elevp->live == false)
						goto Onward;

					// Check "break" argument.
					if(kwid == kw_break) {
						if(havesym(s_any,false)) {
							nodeinit(&node,datp,false);
							if(ge_assign(&node) != Success || extrasym() || !intval(datp))
								goto RCExit;
							if(datp->u.d_int <= 0) {
								(void) rcset(Failure,0,text217,datp->u.d_int);
									// "'break' level '%ld' must be 1 or greater"
								goto RCExit;
								}
							breaklevel = datp->u.d_int;
							}
						else
							breaklevel = 1;

						// Invalidate any "for" loop in progress.
						if(elevp->prevp->flip != NULL)
							elevp->prevp->flip->index = -1;
						}

					// Extraneous "next" argument(s)?
					else if(extrasym())
						goto RCExit;
JumpDown:
					// Continue or break out of loop: find the right block (kw_while, kw_until, kw_loop,
					// kw_for, kw_break, or kw_next) and jump down to its "endloop".
					for(lbp = bufp->b_mip->mi_execp; lbp != NULL; lbp = lbp->lb_next)
						if(lbp->lb_mark == blnp) {

							// If this is a "break" or "next", set the line pointer to the line
							// before the "endloop" line so that the "endloop" is executed;
							// otherwise, set it to the to the "endloop" line itself so that the
							// "endloop" is bypassed.
							elnp = (kwid & SBreakType) ? lback(lbp->lb_jump) : lbp->lb_jump;

							// Return to the most recent loop level (bypassing "if" levels) if this
							// is a "break" or "next"; otherwise, just reset the loop counter.
							if(!(kwid & SBreakType))
								elevp->loopcount = 0;
							else if(prevlevel(&elevp) != Success)
								goto RCExit;

							goto Onward;
							}
BugExit:
					// Huh?  "endloop" line not found! (This is a bug.)
					(void) rcset(Failure,RCNoFormat,text126);
						// "Script loop boundary line not found"
					goto RCExit;

				case kw_endloop:	// "endloop" keyword.

					// Extra arguments?
					if(extrasym())
						goto RCExit;

					// This line is executed only when its partner is a "loop", "for", a "while" that's
					// true, or an "until" that's false, or jumped to from a "break" or "next" (otherwise,
					// it's bypassed).
					if(breaklevel == 0) {

						// Is current level top or was initiated by an "if" statement?
						if(elevp == execlevel || !elevp->loopspawn) {

							// Yep, bail out.
							str = "endloop";
							eline = text198;
								// "Misplaced '%s' keyword"
Misplaced:
							(void) rcset(Failure,0,eline,str);
							goto RCExit;
							}

						// Return to the previous level and check if maxloop exceeded.
						elevp = elevp->prevp;
						if(maxloop > 0 && ++elevp->loopcount > maxloop) {
							(void) rcset(Failure,0,text112,maxloop);
								// "Maximum number of loop iterations (%d) exceeded!"
							goto RCExit;
							}
						}

					// We're good... just find the "loop", "for", "while" or "until", and go back to it, or
					// to a prior level if we're processing a "break".
					for(lbp = bufp->b_mip->mi_execp; lbp != NULL; lbp = lbp->lb_next)
						if((lbp->lb_type & SLoopType) && lbp->lb_jump == blnp) {

							// while/until/loop/for block found: set the line pointer to its
							// "endloop" line or the line before its parent's "endloop" line if
							// we're processing a "break" (and return to the previous execution
							// level); otherwise, back to the beginning.
							if(breaklevel > 0) {
								if(--breaklevel > 0) {
									if(lbp->lb_break == NULL) {
										(void) rcset(Failure,0,text225,breaklevel);
									// "Too many break levels (%d short) from inner 'break'"
										goto RCExit;
										}
									elnp = lback(lbp->lb_break);

									// Return to the most recent loop level before this one.
									elevp = elevp->prevp;
									if(prevlevel(&elevp) != Success)
										goto RCExit;
									}
								else
									elevp = elevp->prevp;
								elevp->loopcount = 0;	// Reset the loop counter.
								}
							else
								elnp = lback(lbp->lb_mark);
							goto Onward;
							}

					// Huh?  "while", "until", "for", or "loop" line not found! (This is a bug.)
					goto BugExit;

				case kw_return:	// "return" keyword.

					// Current level true?
					if(elevp->live == true) {

						// Yes, get return value, if any.
						if(!havesym(s_any,false))
							dsetnil(rp);
						else {
							nodeinit(&node,rp,false);
							if(ge_andor(&node) != Success || extrasym())
								goto RCExit;
							}

						// Exit script.
						goto Exit;
						}

					// Nope, ignore "return".
					goto Onward;

				case kw_force:	// "force" keyword.

					// Check if argument present.
					if(!havesym(s_any,true))
						goto RCExit;
					forcecmd = true;
					break;
				default:	// "force" keyword.
					;	// Fall through.
				}
			} // End statement keyword execution.

		// A "force" or not a statement keyword.  Execute the line as an expression statement.
		if(elevp->live) {
			(void) doestmt(rp,eline1,TokC_Comment,NULL);
			if(forcecmd)		// Force Success status.
				(void) rcclear();

			// Check for exit or a command error.
			if(rc.status <= MinExit)
				return rc.status;
			if(rc.status != Success) {
				EWindow *winp = wheadp;

				// Check if buffer is on-screen.
				do {
					if(winp->w_bufp == bufp) {

						// Found a window.  Set dot to error line.
						winp->w_face.wf_dot.lnp = blnp;
						winp->w_face.wf_dot.off = 0;
						winp->w_flags |= WFHard;
						}
					} while((winp = winp->w_nextp) != NULL);

				// In any case, set the buffer dot.
				bufp->b_face.wf_dot.lnp = blnp;
				bufp->b_face.wf_dot.off = 0;

				// Build a more detailed message that includes the command error message, if any.
				(void) macerror(NULL,NULL,bufp,blnp,flags);
				goto Exit;
				}
			} // End statement execution.
Onward:
		// On to the next line.  eline and len point to the original line.
		if(!(lastWasCL = thisIsCL) && newlast.p_cl != NULL) {
			parseend(oldlast);
			newlast.p_cl = NULL;
			}
		elnp = lforw(elnp);
		} // End buffer execution.
#if MMDebug & (Debug_Script | Debug_PPBuf)
	fprintf(logfile,"*** END execbuf(%u)...\n",myinstance);
#endif
	// "if" and "endif" match?
	if(elevp == execlevel)
		goto Exit;
	(void) rcset(Failure,0,text121,text199);
		// "Unmatched '%s' keyword","if"

	// Clean up and exit per rc.status.
RCExit:
	(void) macerror(NULL,NULL,bufp,blnp,flags);
Exit:
	if(newlast.p_cl != NULL)
		parseend(oldlast);
	llfree(execlevel);

	return (rc.status == Success) ? rc.status : rcset(ScriptError,0,NULL);
	}

// Execute the contents of a buffer, given destination buffer for return value, numeric argument, buffer pointer, pathname of
// file being executed (or NULL), and invocation flags.
#if MMDebug & Debug_MacArg
int _dobuf(Datum *rp,int n,Buffer *bufp,char *runpath,ushort flags) {
#else
int dobuf(Datum *rp,int n,Buffer *bufp,char *runpath,ushort flags) {
#endif
#if MMDebug & (Debug_Datum | Debug_Script)
	fprintf(logfile,"dobuf(...,%s,...,%.4hX)...\n",bufp->b_bname,flags);
#endif
	// Check for narrowed buffer and runaway recursion.
	if(bufp->b_flags & BFNarrow)
		(void) rcset(Failure,RCNoFormat,text72);
			// "Cannot execute a narrowed buffer"
	else if((opflags & OpEval) && maxmacdepth > 0 && bufp->b_mip != NULL && bufp->b_mip->mi_nexec >= maxmacdepth)
		(void) rcset(Failure,0,text319,text336,maxmacdepth);
			// "Maximum %s recursion depth (%d) exceeded","macro"
	else {
		Datum margs;

		// Create buffer-extension record if needed.
		if((opflags & OpEval) && bufp->b_mip == NULL && bextend(bufp) != Success)
			return rc.status;

		// Get macro arguments.
		dinit(&margs);		// No need to dclear() this later because it will always contain a dat_blobRef.
#if MMDebug & Debug_MacArg
		if(margload(n,bufp,&margs,flags) == Success) {
#else
		if(margload(bufp,&margs,flags) == Success) {
#endif
			// If evaluating, preprocess buffer if needed.
			if((opflags & OpEval) && ((bufp->b_flags & BFPreproc) || ppbuf(bufp,flags) == Success)) {
				ScriptRun *oldrun,newrun;
				uint oldscript;

				// Make new run instance and prepare for execution.
				oldrun = scriptrun;
				scriptrun = &newrun;
				oldscript = opflags & OpScript;
				scriptrun->margp = &margs;
				scriptrun->path = fixnull(runpath);
				scriptrun->bufp = bufp;
				if(dnew(&scriptrun->nargp) != 0)
					return drcset();			// Fatal error.
				dsetint(n,scriptrun->nargp);
				scriptrun->uvp = lvarsheadp;
				opflags = (opflags & ~OpParens) | OpScript;

				// Flag that we are executing the buffer and execute it.
				++bufp->b_mip->mi_nexec;
				(void) execbuf(rp,bufp,flags);			// Execute the buffer.
				(void) uvarclean(scriptrun->uvp);		// Clear any local vars that were created.

				// Clean up.
				--bufp->b_mip->mi_nexec;
				opflags = (opflags & ~OpScript) | oldscript;
#if DCaller
				ddelete(scriptrun->nargp,"dobuf");
#else
				ddelete(scriptrun->nargp);
#endif
				scriptrun = oldrun;
				}
			}
		}

	return rc.status;
	}

#if MMDebug & Debug_MacArg
// Call _dobuf() with debugging.
static int dobufDebug(Datum *rp,int n,Buffer *bufp,char *runpath,ushort flags) {

	fprintf(logfile,"*dobuf(...,%d,%s,%s,%.4x)...\n",n,bufp->b_bname,runpath,flags);
	fflush(logfile);
	(void) _dobuf(rp,n,bufp,runpath,flags);
	fprintf(logfile,"dobuf(%s) returned status %d [%s]\n",bufp->b_bname,rc.status,rc.msg.d_str);
	fflush(logfile);
	return rc.status;
	}
#endif

// Yank a file into a buffer and execute it, given result pointer, filename, n arg, and macro invocation "at startup" flag (used
// for error reporting).  Suppress any extraneous messages during read.  If there are no errors, delete the buffer on exit.
int dofile(Datum *rp,char *fname,int n,ushort flags) {
	Buffer *bufp;

	if(bfind(fname,CRBCreate | CRBForce | CRBFile,BFHidden,&bufp,NULL) != Success)	// Create a buffer...
		return rc.status;
	bufp->b_modes = MdRdOnly;					// mark the buffer as read-only...
	uint mflag = modetab[MdRec_Global].flags & MdMsg;		// save current message flag...
	modetab[MdRec_Global].flags &= ~MdMsg;				// clear it...
	(void) readin(bufp,fname,true);					// read in the file...
	modetab[MdRec_Global].flags |= mflag;				// restore message flag...
	if(rc.status == Success)
		(void) dobuf(rp,n,bufp,bufp->b_fname,flags);		// and execute the buffer.

	// If not displayed, remove the now unneeded buffer and return.
	bufp->b_flags &= ~BFHidden;
	if(rc.status >= Cancelled && bufp->b_nwind == 0)
		(void) bdelete(bufp,CLBIgnChgd);
	return rc.status;
	}

// Execute commands in a file and return result in rp.
int xeqFile(Datum *rp,int n,Datum **argpp) {
	char *path;

	// Get the filename.
	if(getarg(rp,text129,NULL,RtnKey,MaxPathname,Arg_First | CFNotNull1,Term_C_Filename) != Success ||
	 (!(opflags & OpScript) && rp->d_type == dat_nil))
			// "Execute script file"
		return rc.status;

	// Look up the path...
	if(pathsearch(&path,rp->d_str,false) != Success)
		return rc.status;
	if(path == NULL)
		return rcset(Failure,0,text152,rp->d_str);	// Not found.
			// "No such file \"%s\""

	// save it...
	if(dsetstr(path,rp) != 0)
		return drcset();

	// skip any comma token...
	if((opflags & OpScript) && !needsym(s_comma,false) && rc.status != Success)
		return rc.status;

	// and execute it with arg n.
	return dofile(rp,rp->d_str,n,0);
	}
