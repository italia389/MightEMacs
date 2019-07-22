// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// exec.c	Routines dealing with execution of commands, command lines, buffers, and command files for MightEMacs.

#include "os.h"
#include <stdarg.h>
#include "std.h"
#include "cmd.h"
#include "bind.h"

// Make selected global definitions local.
#define DATAexec
#include "exec.h"

#include "file.h"
#include "var.h"

/*** Local declarations ***/

// Control data for if/loop execution levels.
typedef struct {
	VDesc vd;		// Loop variable descriptor.
	Array *ary;		// Loop array.
	ArraySize index;	// Index of next element to process.
	} ForLoopInfo;
typedef struct LevelInfo {
	struct LevelInfo *prev,*next;
#if MMDebug & (Debug_Script | Debug_PPBuf)
	uint level;		// Execution level, beginning at zero.
#endif
	bool live;		// True if executing this level.
	bool loopspawn;		// True if level spawned by any loop statement.
	int loopcount;		// Number of times through the loop.
	ForLoopInfo *fli;	// Control information for "for" loop.
	bool ifwastrue;		// True if (possibly compound) "if" statement was ever true.
	bool elseseen;		// True if "else" clause has been processed at this level.
	} LevelInfo;

#define StmtKeywordMax	8		// Length of longest statement keyword name.

#if MMDebug & (Debug_Datum | Debug_Script | Debug_MacArg | Debug_PPBuf)
static char *showflags(Parse *new) {
	char *str;
	static char wkbuf[100];

	sprintf(wkbuf,"OpScript %s, last",si.opflags & OpScript ? "SET" : "CLEAR");
	str = strchr(wkbuf,'\0');
	if(last == NULL)
		str = stpcpy(str," NULL");
	else {
		sprintf(str,"->p_flags %.4hX",last->p_flags);
		str = strchr(str,'\0');
		}
	if(new != NULL)
		sprintf(str,", newlast.p_flags %.4hX",new->p_flags);
	return wkbuf;
	}
#endif

// Search hook table for given name.  If found, set *hrecp to pointer; otherwise, return an error.
static int findhook(char *name,HookRec **hrecp) {
	HookRec *hrec = hooktab;

	do {
		if(strcasecmp(name,hrec->h_name) == 0) {
			*hrecp = hrec;
			return rc.status;
			}
		} while((++hrec)->h_name != NULL);

	return rcset(Failure,0,text317,name);
			// "No such hook '%s'"
	}

// Check if a macro is bound to a hook and return true if so (and set error if isError is true), otherwise, false.
bool ishook(Buffer *buf,bool isError) {
	HookRec *hrec = hooktab;

	do {
		if(buf == hrec->h_buf) {
			if(isError)
				(void) rcset(Failure,0,text327,hrec->h_name);
						// "Macro bound to '%s' hook - cannot delete"
			return true;
			}
		} while((++hrec)->h_name != NULL);

	return false;
	}

// Set a hook.  Return status.
int setHook(Datum *rval,int n,Datum **argv) {
	HookRec *hrec;
	UnivPtr univ;			// Pointer to the requested macro.
	Datum *datum;

	// First, get the hook (name) to set.
	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript)
		datxfer(datum,argv[0]);
	else if(terminp(datum,text318,ArgNotNull1 | ArgNil1,0,NULL) != Success || datum->d_type == dat_nil)
			// "Set hook "
		return rc.status;

	// Valid?
	if((si.opflags & (OpScript | OpEval)) != OpScript && findhook(datum->d_str,&hrec) != Success)
		return rc.status;

	// Get the macro name.  If interactive mode, build "progress" prompt first.
	if(si.opflags & OpScript) {
		if(!needsym(s_comma,true))
			return rc.status;
		if(havesym(kw_nil,false)) {
			univ.u.p_buf = NULL;
			(void) getsym();
			}
		else
			(void) getcfm(NULL,&univ,PtrMacro);
		}
	else {
		char wkbuf[NWork + 1];

		sprintf(wkbuf,"%s%s %s %s",text318,datum->d_str,text339,text336);
					// "Set hook ","to","macro",
		(void) getcfm(wkbuf,&univ,PtrMacro_C);
		}

	if(rc.status == Success) {

		// Constrained macro?
		if(univ.u.p_buf != NULL && !(univ.u.p_buf->b_flags & BFConstrain))
			return rcset(Failure,RCTermAttr,"%s%s '%c%c%s%c%c'",text420,text414,AttrSpecBegin,AttrBoldOn,
							// "Hook may not be set to non-","constrained macro"
			 univ.u.p_buf->b_bname + 1,AttrSpecBegin,AttrBoldOff);

		// Unless script mode and not evaluating, set the hook.
		if((si.opflags & (OpScript | OpEval)) != OpScript)
			hrec->h_buf = univ.u.p_buf;
		}

	return rc.status;
	}

// Disable a hook that went awry.  Return status.
static int dishook(HookRec *hrec) {
	DStrFab msg;
	int rcode = 0;

	// Hook execution failed, let the user know what's up.
	if(rc.status > FatalError) {
		if((rcode = dopentrk(&msg)) == 0 && escattr(&msg) == Success) {
			if(disempty(&msg))
				rcode = dputf(&msg,text176,hrec->h_buf->b_bname + 1);
						 // "Macro '~b%s~B' failed"
#if MMDebug & Debug_Datum
			if(rcode == 0) {
				fprintf(logfile,"dishook(): calling dputf(...,%s,%s)...\n",text161,hrec->h_name);
				rcode = dputf(&msg,text161,hrec->h_name);
				ddump(msg.sf_datum,"dishook(): post dputf()...");
				}
			if(rcode == 0) {
				rcode = dclose(&msg,sf_string);
				fprintf(logfile,"dishook(): dclose() returned rc.msg '%s', status %d.\n",rc.msg.d_str,
				 rc.status);
#else
			if(rcode == 0 && (rcode = dputf(&msg,text161,hrec->h_name)) == 0 &&
							// " (disabled '%s' hook)"
			 (rcode = dclose(&msg,sf_string)) == 0) {
#endif
				(void) rcset(rc.status,RCForce | RCNoFormat | RCTermAttr,msg.sf_datum->d_str);
				}
			}
		}

	// disable the hook...
	hrec->h_buf = NULL;

	// and set library error if needed.
	return (rcode == 0) ? rc.status : librcset(Failure);
	}

// Execute a macro bound to a hook with rval (if not NULL) and n.  If arginfo == 0, invoke the macro directly; otherwise, build
// a command line with arguments.  In the latter case, arginfo contains the argument count and types of each argument (string,
// long, or Datum *).  The lowest four bits of arginfo comprise the argument count.  Each pair of higher bits is 0 for a string
// argument, 1 for a long (integer) argument, and 2 for a Datum pointer argument.
int exechook(Datum *rval,int n,HookRec *hrec,uint arginfo,...) {

	// Nothing to do if hook not set or macro already running.
	if(hrec->h_buf == NULL || hrec->h_buf->b_mip->mi_nexec > 0) {
		if(rval != NULL)
			dsetnil(rval);
		return rc.status;
		}
	if(rval == NULL && dnewtrk(&rval) != 0)		// For throw-away return value.
		goto LibFail;

	// Build command line if requested.
	if(arginfo != 0) {
		va_list ap;
		DStrFab cmd;
		char *str;
		short delim = ' ';
		uint argct = arginfo & 0xF;

		// Concatenate macro name with arguments...
		if(dopentrk(&cmd) != 0 || (n != INT_MIN && dputf(&cmd,"%d => ",n) != 0) ||
		 dputs(hrec->h_buf->b_bname + 1,&cmd) != 0)
			goto LibFail;
		va_start(ap,arginfo);
		arginfo >>= 4;
		do {
			if(dputc(delim,&cmd) != 0)
				goto LibFail;
			switch(arginfo & 3) {
				case 0:			// string argument.
					if((str = va_arg(ap,char *)) == NULL) {
						if(dputs(viz_nil,&cmd) != 0)
							goto LibFail;
						}
					else if(quote(&cmd,str,true) != Success)
						goto Fail;
					break;
				case 1:			// long argument.
					if(dputf(&cmd,"%ld",va_arg(ap,long)) != 0)
						goto LibFail;
					break;
				default:		// Datum *
					if(dtosfchk(&cmd,va_arg(ap,Datum *),NULL,CvtExpr) != Success)
						goto Fail;
				}
			arginfo >>= 2;
			delim = ',';
			} while(--argct > 0);
		va_end(ap);

		// and execute result.
		if(dclose(&cmd,sf_string) != 0)
			goto LibFail;
#if MMDebug & Debug_Datum
		fprintf(logfile,"exechook(): calling execestmt(%s)...\n",cmd.sf_datum->d_str);
		(void) execestmt(rval,cmd.sf_datum->d_str,TokC_ComLine,NULL);
		fprintf(logfile,"exechook(): execestmt() returned rc.msg '%s', status %d.\n",
		 rc.msg.d_str,rc.status);
		if(rc.status != Success)
#else
		if(execestmt(rval,cmd.sf_datum->d_str,TokC_ComLine,NULL) != Success)
#endif
			goto Fail;
		}
	else {
		uint oldnoload = si.opflags & OpNoLoad;
		si.opflags |= OpNoLoad;
		(void) execbuf(rval,n,hrec->h_buf,NULL,ArgFirst);
		si.opflags = (si.opflags & ~OpNoLoad) | oldnoload;
		if(rc.status != Success)
			goto Fail;
		}

	// Successful execution.  Check for false return.
	if(rval->d_type != dat_false)
		return rc.status;
Fail:
	// Execution failed or hook returned false.  Disable hook and let the user know what's up.
	return dishook(hrec);
LibFail:
	return librcset(Failure);
	}

// Check if given command, alias, or macro execution object can be run interactively.  Set an error and return false if not;
// otherwise, return true.
static bool canRun(UnivPtr *univp) {

	// Error if alias to a function, contrained macro, or alias to a constrained macro.
	if((univp->p_type & (PtrAlias_F | PtrMacro_C)) || (univp->p_type == PtrAlias_M &&
	 (univp->u.p_alias->a_targ.u.p_buf->b_flags & BFConstrain))) {
		(void) rcset(Failure,RCTermAttr,"%s%s '%c%c%s%c%c'%s",text314,univp->p_type == PtrMacro_C ? text414 :
								// "Cannot run "		"constrained macro"
		 text127,AttrSpecBegin,AttrBoldOn,univp->p_type == PtrMacro_C ? univp->u.p_buf->b_bname + 1 :
			// "alias"
		  univp->u.p_alias->a_name,AttrSpecBegin,AttrBoldOff,text415);
							// " interactively"
		return false;
	 	}
	return true;
	}

// Execute a named command, alias, or macro interactively even if it is not bound or is being invoked from a macro.
int run(Datum *rval,int n,Datum **argv) {
	UnivPtr univ;		// Pointer to command, alias, or macro to execute.
	ushort oldscript;
	ushort oldmsg = mi.cache[MdIdxRtnMsg]->ms_flags & MdEnabled;

	// If we are in script mode, force the command interactively.
	if((oldscript = si.opflags & OpScript)) {

		// Grab next symbol...
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;

		// look it up...
		if(si.opflags & OpEval) {
			if(!execfind(last->p_tok.d_str,OpQuery,PtrCmd | PtrAlias | PtrMacro,&univ))
				return rcset(Failure,0,text244,last->p_tok.d_str);
					// "No such command, alias, or macro '%s'"
			if(!canRun(&univ))
				return rc.status;
			}

		// and get next symbol.
		if(getsym() < NotFound)
			return rc.status;

		// If not evaluating, bail out here.
		if(!(si.opflags & OpEval))
			return rc.status;

		// Otherwise, prepare to execute the CAM -- interactively.
		si.opflags &= ~OpScript;
		gmset(mi.cache[MdIdxRtnMsg]);
		}

	// Prompt the user to type a named command, alias, or macro.
	else {
		// Interactive mode.
		if(getcfam(": ",PtrCmd | PtrAlias_C | PtrAlias_M | PtrMacro_O,&univ,text244) != Success ||
								// "No such command, alias, or macro '%s'"
		 univ.p_type == PtrNul)
			return rc.status;

		// Got a name.  Check if right type.
		if(!canRun(&univ))
			return rc.status;
		}

	// Execute it.
	bool fevalcall = false;
	if(univ.p_type & PtrAlias)
		univ = univ.u.p_alias->a_targ;
	if(univ.p_type == PtrMacro_O)
		(void) execbuf(rval,n,univ.u.p_buf,NULL,
		 si.opflags & OpParens ? ArgFirst | SRun_Parens : ArgFirst);	// Call the macro.
	else {									// Call the command.
		CmdFunc *cfp = univ.u.p_cfp;
		if(n != 0 || !(cfp->cf_aflags & CFNCount))
			if(allowedit(cfp->cf_aflags & CFEdit) == Success) {
				(void) execCF(rval,n,cfp,0,0);
				fevalcall = true;
				}
		}
	if(rc.status == Success && oldscript && !fevalcall)
		(void) rcsave();

	// Clean up and return result.
	if(oldscript && !oldmsg)
		gmclear(mi.cache[MdIdxRtnMsg]);
	si.opflags = (si.opflags & ~OpScript) | oldscript;
	return rc.status;
	}

// Concatenate all arguments and execute string result.  Return status.
int eval(Datum *rval,int n,Datum **argv) {
	Datum *datum;

	// Get the command line.
	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript) {

		// Concatenate all arguments into datum.
		if(catargs(datum,1,NULL,0) != Success)
			return rc.status;
		}
	else if(terminp(datum,": ",ArgNotNull1 | ArgNil1,0,NULL) != Success || datum->d_type == dat_nil)
		return rc.status;

	// Execute command line.
#if MMDebug & Debug_Script
	fprintf(logfile,"eval(): Calling execestmt(\"%s\")...\n",datum->d_str);
#endif
	return execestmt(rval,datum->d_str,TokC_ComLine,NULL);
	}

#if MMDebug & Debug_MacArg
// Dump macro argument list to log file.
static int margdump(int n,Buffer *buf,Datum *datum) {
	Datum arglist;

	fprintf(logfile,"-----\nmargdump(): n: %d, buf: '%s'\n",n,buf->b_bname);
	dinit(&arglist);
	(void) dquote(&arglist,datum,CvtExpr);
	fprintf(logfile,"Macro argument list: %s\n",arglist.d_str);
	dclear(&arglist);
	return rc.status;
	}
#endif

// Load macro arguments into an array and save in "margs".  If in script mode, evaluate some or all of remaining arguments (if
// any) on command line, dependent on number specified in Buffer record; otherwise, create empty list.  "flags" contains script
// flags and argument flags.  Return status.
#if MMDebug & Debug_MacArg
static int margload(int n,Buffer *buf,Datum *margs,uint flags) {
#else
static int margload(Buffer *buf,Datum *margs,uint flags) {
#endif
	Array *ary;

#if MMDebug & Debug_MacArg
	fprintf(logfile,"*margload(%d,%s,%.8X)...\n",n,buf->b_bname,flags);
#endif
	// Create array for argument list (which may remain empty).
	if(mkarray(margs,&ary) != Success)
		return rc.status;

	// Don't get arguments (that don't exist) if executing a hook; that is, OpNoLoad is set.
	if((si.opflags & (OpScript | OpNoLoad)) == OpScript) {
		Datum *arg;
		int argct,minArgs,maxArgs;
		uint aflags = (flags & ArgFirst) | ArgBool1 | ArgArray1 | ArgNIS1;

		if(buf->b_mip == NULL) {
			minArgs = 0;
			maxArgs = SHRT_MAX;
			}
		else {
			minArgs = buf->b_mip->mi_minArgs;
			maxArgs = (buf->b_mip->mi_maxArgs < 0) ? SHRT_MAX : buf->b_mip->mi_maxArgs;
			}

		// Get arguments until none left.
		if(dnewtrk(&arg) != 0)
			goto LibFail;
#if MMDebug & Debug_MacArg
		fprintf(logfile,"\tParsing arguments.  p_tok: \"%s\", p_cl: \"%s\"\n",last->p_tok.d_str,last->p_cl);
#endif
		// If xxx(...) call form, have ')', and minArgs == 0, bail out (not an error).
		if(minArgs > 0 || (maxArgs != 0 && (!(flags & SRun_Parens) || !havesym(s_rparen,false)))) {
			for(argct = 0;;) {

				// Get next argument.
				if(aflags & ArgFirst) {
					if(!havesym(s_any,minArgs > 0)) {
						if(rc.status != Success)
							return rc.status;	// Error.
						break;				// No arguments left.
						}
					if(havesym(s_rparen,false))
						break;				// No arguments left.
					}
				else if(!havesym(s_comma,false))
					break;					// No arguments left.

				// We have a symbol.  Get expression.
				if(funcarg(arg,aflags) != Success)
					return rc.status;
				aflags = ArgBool1 | ArgArray1 | ArgNIS1;

				// Got an argument... add it to array and count it.
				if(apush(ary,arg,true) != 0)
					goto LibFail;
				++argct;
				}

			// Too few or too many arguments found?
			if(argct < minArgs || argct > maxArgs)
				return rcset(Failure,0,text69,last->p_tok.d_str);
					// "Wrong number of arguments (at token '%s')"
			}
		}
	else if((si.opflags & OpNoLoad) && buf->b_mip != NULL && buf->b_mip->mi_minArgs > 0)
		return rcset(Failure,0,text69,"");
			// "Wrong number of arguments (at token '%s')"
	si.opflags &= ~OpNoLoad;
#if MMDebug & Debug_MacArg
	return margdump(n,buf,margs);
#else
	return rc.status;
#endif
LibFail:
	return librcset(Failure);
	}

#if MMDebug & (Debug_Expr | Debug_Token)
// Write parsing state to log file.
static void pinstance(bool begin) {

	if(last != NULL) {
		fputs(begin ? "parsebegin(): new instance" : "parseend(): old instance restored",logfile);
		fprintf(logfile,": p_cl \"%s\", p_tok \"%s\", p_sym %.4X.",last->p_cl,last->p_tok.d_str,last->p_sym);
		fputs(begin ? "..\n" : "\n",logfile);
		}
	}
#endif

// Initialize "last" global variable for new command line and get first symbol.  Return status.
static int parsebegin(Parse **oldp,Parse *new,char *cl,short termch) {

	// Save current parsing state and initialize new one.
	*oldp = last;
	last = new;
	last->p_cl = cl;
	last->p_termch = termch;
	last->p_sym = s_any;
#if DCaller
	dinit(&last->p_tok,"parsebegin");
#else
	dinit(&last->p_tok);
#endif
#if MMDebug & (Debug_Expr | Debug_Token)
	pinstance(true);
#endif
	last->p_datGarb = datGarb;
#if MMDebug & Debug_Datum
	fprintf(logfile,"parsebegin(): saved garbage pointer %.8X\n",(uint) datGarb);
#endif
	// Save old "script mode" flag and enable it.
	last->p_flags = si.opflags & OpScript;
	si.opflags = (si.opflags & ~OpParens) | OpScript;

	// Get first symbol and return.
	(void) getsym();		// Ignore any "NotFound" return.
	return rc.status;
	}

// End a parsing instance and restore previous one.
static void parseend(Parse *old) {

	garbpop(last->p_datGarb);
	dclear(&last->p_tok);
	si.opflags = (si.opflags & ~OpScript) | last->p_flags;
	last = old;
#if MMDebug & (Debug_Expr | Debug_Token)
	pinstance(false);
#endif
	}		

// Execute string in current parsing instance as an expression statement.  Low-level routine called by execestmt() and xbuf().
// Return status.
static int xestmt(Datum *rval) {
	ENode node;

	// Set up the default command values.
	kentry.lastflag = kentry.thisflag;
	kentry.thisflag = 0;

	// Evaluate the string (as an expression).
	nodeinit(&node,rval,true);
#if MMDebug & (Debug_Expr | Debug_Script)
	fprintf(logfile,"xestmt() BEGIN: calling ge_andor() on token '%s', line '%s'...\n",last->p_tok.d_str,last->p_cl);
#endif
	if(ge_andor(&node) == Success)
		if(!extrasym()) {				// Error if extra symbol(s).
			if(last->p_termch == TokC_ExprEnd &&	// Check if parsing "#{}" and right brace missing.
			 *last->p_cl != TokC_ExprEnd)
				(void) rcset(Failure,0,text173,TokC_Expr,TokC_ExprBegin,TokC_ExprEnd);
						// "Unbalanced %c%c%c string parameter"
			}
#if MMDebug & (Debug_Script | Debug_Expr)
	fprintf(logfile,"xestmt(): Returning status %hd \"%s\"\n",rc.status,rc.msg.d_str);
#endif
	return rc.status;
	}

// Parse and execute a string as an expression statement, given result pointer, string pointer, terminator character, and
// optional pointer in which to return updated string pointer.  This function is called anytime a string needs to be executed as
// a "statement" (without a leading statement keyword); for example, by xbuf(), xeqCmdLine(), "#{...}", or the -e switch at
// startup.
int execestmt(Datum *rval,char *cl,short termch,char **cmdln) {
	Parse *oldlast,newlast;

#if MMDebug & (Debug_Datum | Debug_Script)
	fprintf(logfile,"execestmt(%.8X,\"%s\",%.4hX,%.8X)...\n",(uint) rval,cl,termch,(uint) cmdln);
#endif
	// Begin new command line parsing instance.
	if(parsebegin(&oldlast,&newlast,cl,termch) == Success) {

		// Evaluate the line (as an expression).
		(void) xestmt(rval);

		// Restore settings and return.
		if(cmdln != NULL)
			*cmdln = last->p_cl;
		}
	parseend(oldlast);
#if MMDebug & Debug_Script
	fputs("execestmt(): EXIT\n",logfile);
#endif
	return rc.status;
	}

// Create or delete an alias in the linked list headed by ahead and in the execution table and return status.
// If the alias already exists:
//	If op is OpCreate:
//		Do nothing.
//	If op is OpDelete:
//		Delete the alias and the associated exectab entry.
// If the alias does not already exist:
//	If op is OpCreate:
//		Create a new object, set its contents to *univp, and add it to the linked list and execution table.
//	If op is OpDelete:
//		Return an error.
int aupdate(char *aname,ushort op,UnivPtr *univp) {
	Alias *ap1,*ap2;
	int result;

	// Scan the alias list.
	ap1 = NULL;
	ap2 = ahead;
	while(ap2 != NULL) {
		if((result = strcmp(ap2->a_name,aname)) == 0) {

			// Found it.  Check op.
			if(op == OpDelete) {

				// Delete the exectab entry.
				if(execfind(aname,OpDelete,0,NULL) != Success)
					return rc.status;

				// Decrement alias use count on macro, if applicable.
				if(ap2->a_targ.p_type & PtrMacro)
					--ap2->a_targ.u.p_buf->b_nalias;

				// Delete the alias from the list and free the storage.
				if(ap1 == NULL)
					ahead = ap2->a_nextp;
				else
					ap1->a_nextp = ap2->a_nextp;
				free((void *) ap2);

				return rc.status;
				}

			// Not a delete... nothing to do.
			return rc.status;
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
		UnivPtr univ;

		// Valid identifier name?
		str = aname;
		if(((sym = getident(&str,NULL)) != s_ident && sym != s_identq) || *str != '\0')
			(void) rcset(Failure,0,text447,text286,aname);
				// "Invalid %s '%s'","identifier"

		// Allocate the needed memory.
		if((ap2 = (Alias *) malloc(sizeof(Alias) + strlen(aname))) == NULL)
			return rcset(Panic,0,text94,"afind");
				// "%s(): Out of memory!"

		// Find the place in the list to insert this alias.
		if(ap1 == NULL) {

			// Insert at the beginning.
			ap2->a_nextp = ahead;
			ahead = ap2;
			}
		else {
			// Insert after ap1.
			ap2->a_nextp = ap1->a_nextp;
			ap1->a_nextp = ap2;
			}

		// Set the other record members.
		strcpy(ap2->a_name,aname);
		ap2->a_targ = *univp;
		ap2->a_type = univp->p_type == PtrCmd ? PtrAlias_C : univp->p_type == PtrFunc ? PtrAlias_F : PtrAlias_M;

		// Add its name to the exectab hash.
		return execfind((univ.u.p_alias = ap2)->a_name,OpCreate,univ.p_type = ap2->a_type,&univ);
		}

	// Delete request but alias not found.
	return rcset(Failure,0,text271,aname);
			// "No such alias '%s'"
	}

// Create an alias to a command, function, or macro.  Replace any existing alias if n > 0.
int aliasCFM(Datum *rval,int n,Datum **argv) {
	UnivPtr univ;
	Datum *dname;

	// Get the alias name.
	if(dnewtrk(&dname) != 0)
		goto LibFail;
	if(si.opflags & OpScript) {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(dsetstr(last->p_tok.d_str,dname) != 0)
			goto LibFail;
		}
	else if(terminp(dname,text215,ArgNotNull1 | ArgNil1,0,NULL) != Success || dname->d_type == dat_nil)
			// "Create alias "
		return rc.status;

	// Existing function, alias, macro, or user variable of same name?  Exclude aliases from search if n > 0 (a force).
	if((si.opflags & OpEval) && (execfind(dname->d_str,OpQuery,n > 0 ? PtrAny & ~PtrAlias : PtrAny,NULL) ||
	 uvarfind(dname->d_str) != NULL))
		return rcset(Failure,RCTermAttr,text165,dname->d_str);
			// "Name '~b%s~B' already in use"

	if(si.opflags & OpScript) {

		// Get equal sign.
		if(getsym() < NotFound || !havesym(s_any,true))
			return rc.status;
		if(last->p_sym != s_assign)
			return rcset(Failure,RCTermAttr,text23,(cftab + cf_alias)->cf_name,last->p_tok.d_str);
				// "Missing '=' in ~b%s~B command (at token '%s')"
		if(getsym() < NotFound)			// Past '='.
			return rc.status;

		// Get the command, function, or macro name.
		(void) getcfm(NULL,&univ,PtrCmd | PtrFunc | PtrMacro);
		}
	else {
		char wkbuf[strlen(text215) + strlen(dname->d_str) + strlen(text325) + strlen(text313) + 8];

		// Get the command, function, or macro name.
		sprintf(wkbuf,"%s'%c%c%s%c%c' %s %s",text215,AttrSpecBegin,AttrBoldOn,dname->d_str,AttrSpecBegin,AttrBoldOff,
						// "Create alias "
		 text325,text313);
			// "of","command, function, or macro"
		(void) getcfm(wkbuf,&univ,PtrCmd | PtrFunc | PtrMacro);
		}

	if(rc.status == Success && univ.p_type != PtrNul) {

		// Delete any existing alias.
		if(n > 0 && execfind(dname->d_str,OpQuery,PtrAlias,NULL))
			(void) aupdate(dname->d_str,OpDelete,NULL);

		// Create the alias.
		if(aupdate(dname->d_str,OpCreate,&univ) != Success)
			return rc.status;

		// Increment alias use count on macro.
		if(univ.p_type & PtrMacro)
			++univ.u.p_buf->b_nalias;
		}

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Delete one or more aliases or macros.  Set rval to count of items deleted or zero if error.  Return status.
static int delAM(Datum *rval,uint selector,char *prmt,char *name,char *emsg) {
	UnivPtr univ;
	uint count = 0;

	if(si.opflags & OpEval)
		dsetint(0L,rval);

	// If interactive, get alias or macro name from user.
	if(!(si.opflags & OpScript)) {
		if(getcfam(prmt,selector,&univ,emsg) != Success || univ.p_type == PtrNul)
			return rc.status;
		goto NukeIt;
		}

	// Script mode: get alias(es) or macro(s) to delete.
	do {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(si.opflags & OpEval) {
			if(!execfind(last->p_tok.d_str,OpQuery,selector,&univ))
				(void) rcset(Success,RCNoWrap,emsg,last->p_tok.d_str);
			else {
NukeIt:
				// Nuke it.
				if(selector & PtrMacro) {
					if(bdelete(univ.u.p_buf,BC_IgnChgd) != Success)
						goto Fail;
					}
				else if(aupdate(univ.u.p_alias->a_name,OpDelete,NULL) != Success) {
Fail:
					// Don't cause a running script to fail on Failure status.
					if(rc.status < Failure)
						return rc.status;
					(void) rcunfail();
					continue;
					}
				++count;
				}
			}
		} while((si.opflags & OpScript) && getsym() == Success && needsym(s_comma,false));

	// Return count if evaluating and no errors.
	if((si.opflags & OpEval) && disnull(&rc.msg))
		dsetint((long) count,rval);
	if(count == 1)
		(void) rcset(Success,0,"%s %s",name,text10);
						// "deleted"
	return rc.status;
	}

// Delete one or more alias(es).  Return status.
int delAlias(Datum *rval,int n,Datum **argv) {

	return delAM(rval,PtrAlias,text269,text411,text271);
			// "Delete alias","Alias","No such alias '%s'"
	}

// Delete one or more macros.  Return status.
int delMacro(Datum *rval,int n,Datum **argv) {

	return delAM(rval,PtrMacro,text216,text412,text116);
			// "Delete macro","Macro","No such macro '%s'"
	}

// Execute the contents of a buffer (of commands) and return result in rval.
int xeqBuf(Datum *rval,int n,Datum **argv) {
	Buffer *buf;		// Pointer to buffer to execute.

	// Find out what buffer the user wants to execute...
	if(bcomplete(rval,text117,si.curbuf->b_bname,OpDelete,&buf,NULL) != Success || buf == NULL)
			// "Execute"
		return rc.status;

	// and execute it with arg n.
#if MMDebug & Debug_Script
	return debug_execbuf(rval,n,buf,buf->b_fname,si.opflags & OpParens ? SRun_Parens : 0,"xeqBuf");
#else
	return execbuf(rval,n,buf,buf->b_fname,si.opflags & OpParens ? SRun_Parens : 0);
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

// Look up a statement keyword, given indirect pointer to beginning of command line and length.  Update *cmdln and return symbol
// id if found; otherwise, s_nil.
static enum e_sym kwfind(char **cmdln,int len) {
	char *cl = *cmdln;

	if(is_lower(*cl)) {
		enum e_sym s;
		ushort kwlen;
		char wkbuf[StmtKeywordMax + 2],*str = wkbuf;

		// Copy enough of line into wkbuf (null terminated) to do keyword comparisons.
		stplcpy(wkbuf,cl,(uint) len < (sizeof(wkbuf) - 1) ? (uint) len + 1 : sizeof(wkbuf));

		// Check if line begins with a statement keyword.
		if((s = getident(&str,&kwlen)) >= kw_break) {
			*cmdln = cl + kwlen;
			return s;
			}
		}

	return s_nil;
	}

// Build a macro execution error message which includes the buffer name and line currently being executed, and set via rcset().
// If emsg0 is NULL, use rc.msg as the initial portion; if rc.msg is null, use a generic error message.  Return status.
static int macerror(char *emsg0,char *emsg1,Buffer *buf,Line *lnp,uint flags) {
	int rcode;
	DStrFab msg;
	char *name,*label;
	short delim;

	if(buf->b_fname != NULL) {
		name = buf->b_fname;
		label = text99;
			// "file"
		delim = '"';
		}
	else {
		name = buf->b_bname;
		label = text83;
			// "buffer"
		delim = '\'';
		}

	// Build error message line.  emsg0 may be long and may be truncated when displayed on message line, but so be it.  The
	// first portion of the message is the most important to see.
	if((rcode = dopentrk(&msg)) == 0) {

		// Prepend emsg0 and emsg1.
		if(emsg0 == NULL && emsg1 == NULL && !disnull(&rc.msg)) {
			if(escattr(&msg) != Success)
				return rc.status;
			}
		else
			rcode = (emsg1 == NULL) ? dputs(emsg0 != NULL ? emsg0 : text219,&msg) : dputf(&msg,emsg0,emsg1);
									// "Script failed"
		if(rcode == 0 &&
		 (rcode = dputf(&msg,"%s %s %c%s%c %s %ld",text229,label,delim,name,delim,text230,getlinenum(buf,lnp))) == 0 &&
						// ", in","at line"
		 (rcode = dclose(&msg,sf_string)) == 0)
			(void) rcset(ScriptError,RCForce | RCNoFormat | RCTermAttr,msg.sf_datum->d_str);
		}

	return (rcode == 0) ? rc.status : librcset(Failure);
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
void ppfree(Buffer *buf) {
	MacInfo *mip = buf->b_mip;
	if(mip != NULL) {
		if(mip->mi_exec != NULL) {
			lbfree(mip->mi_exec);
			mip->mi_exec = NULL;
			}
		dclear(&mip->mi_usage);
		dclear(&mip->mi_desc);
		buf->b_flags &= ~BFPreproc;		// Clear "preprocessed" flag.
		}
	}

#if MMDebug & Debug_PPBuf
static void lbdump(Line *lnp,char *label) {

	fprintf(logfile,"%s: [%.8X] ",label,(uint) lnp);
	if(lnp == NULL)
		fputs("NULL\n",logfile);
	else {
		char *str = lnp->l_text;
		int len = skipwhite(&str,lnp->l_used);
		fprintf(logfile,"%.*s\n",len,str);
		}
	}
#endif

// Preprocess a buffer and return status.  If no errors found, set buf->b_mip->mi_exec to result.  buf->b_mip is assumed to
// not be NULL.
static int ppbuf(Buffer *buf,uint flags) {
	Line *lnp;			// Pointer to line to execute.
	enum e_sym kwid;		// Statement keyword ID
	int len;			// Line length.
	int saltlevel;			// "macro" nesting depth (for error checking).
	LoopBlock *lbexec;		// Pointer to execution loop-block list.
	LoopBlock *lbopen;		// Pointer during scan.
	LoopBlock *lbtemp;		// Temporary pointer.
	char *eline;			// Text of line to scan.
	char *err;
	bool skipLine;
	bool lastWasCL = false;		// Last line was a continuation line.

	// Scan the buffer to execute, building loop blocks for any loop statements found, regardless of "truth" state (which is
	// unknown at this point).  This allows for any loop block to be executed at execution time.
	lbexec = lbopen = NULL;
	saltlevel = 0;
	lnp = buf->b_lnp;
	do {
		// Scan the current line.
		eline = lnp->l_text;
		len = lnp->l_used;
#if MMDebug & Debug_PPBuf
		fprintf(logfile,"%.8X >> %.*s\n",(uint) lnp,len,eline);
#endif
		// Skip line if last line was a continuation line.
		skipLine = lastWasCL;
		lastWasCL = len > 0 && eline[len - 1] == '\\';
		if(skipLine)
			continue;

		// Find first non-whitespace character.  If line is blank or a comment, skip it.
		if((len = skipwhite(&eline,len)) == 0 || *eline == TokC_ComLine)
			continue;

		// Check for a statement keyword.
		if((kwid = kwfind(&eline,len)) != s_nil) {
			if(kwid == kw_constrain)
				if((len = skipwhite(&eline,len)) == 0 || *eline == TokC_ComLine ||
				 (kwid = kwfind(&eline,len)) != kw_macro) {
					err = text413;
						// "'macro' keyword expected"
					eline = NULL;
					goto FailExit;
					}
			switch(kwid) {
				case kw_macro:		// macro keyword: bump salting level.  Nested?
					if(++saltlevel > 1) {
						err = text419;
							// "Nested macro%s"
						eline = text416;
							// " not allowed"
						goto FailExit;
						}
					break;
				case kw_endmacro:	// endmacro keyword: orphan?
					if(--saltlevel < 0) {
						err = text121;
							// "Unmatched '~b%s~B' keyword"
						eline = text197;
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
						err = text120;
							// "'break' or 'next' outside of any loop block"
						eline = NULL;
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
						err = text121;
							// "Unmatched '~b%s~B' keyword"
						eline = text196;
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
		} while((lnp = lnp->l_next) != NULL);

	// Buffer SCAN completed.  while/until/for/loop and "endloop" match?
	if(lbopen != NULL) {
		err = text121;
			// "Unmatched '~b%s~B' keyword"
		eline = text122;
			// "loop"
		goto FailExit;
		}

	// "macro" and "endmacro" match?
	if(saltlevel > 0) {
		err = text121;
			// "Unmatched '~b%s~B' keyword"
		eline = text336;
			// "macro"
FailExit:
		(void) macerror(err,eline,buf,lnp,flags);
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
			(void) rcset(Failure,0,text220,getlinenum(buf,lbtemp->lb_mark));
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
		fprintf(logfile,"-----\n Type: %.4X %s\n",lbtemp->lb_type,str);
		lbdump(lbtemp->lb_mark," Mark");
		lbdump(lbtemp->lb_jump," Jump");
		lbdump(lbtemp->lb_break,"Break");
		}
//	return rcset(FatalError,RCNoFormat,"ppbuf() exit");
#endif
	// Success!  Save block pointer in buffer-extension record and set "preprocessed" flag.
	buf->b_mip->mi_exec = lbexec;
	buf->b_flags |= BFPreproc;
	goto Exit;

	// Clean up and exit per rc.status.
RCExit:
	lbfree(lbopen);
	(void) macerror(NULL,NULL,buf,lnp,flags);
Exit:
	if(rc.status == Success)
		return rc.status;
	lbfree(lbexec);
	return rc.status;
	}

// Free loop level storage.
static void llfree(LevelInfo *elevel) {
	LevelInfo *elevel1;

	do {
		elevel1 = elevel->next;
		if(elevel->fli != NULL)
			free((void *) elevel->fli);
		free((void *) elevel);
		} while((elevel = elevel1) != NULL);
	}

// Move forward to a new execution level, creating a level object if needed.
static int nextlevel(LevelInfo **elevelp) {
	LevelInfo *elevel = *elevelp;

	if(elevel == NULL || elevel->next == NULL) {
		LevelInfo *elevel1;

		if((elevel1 = (LevelInfo *) malloc(sizeof(LevelInfo))) == NULL)
			return rcset(Panic,0,text94,"nextlevel");
				// "%s(): Out of memory!"
		elevel1->fli = NULL;
		elevel1->next = NULL;
		if(elevel == NULL) {
			elevel1->prev = NULL;
#if MMDebug & (Debug_Script | Debug_PPBuf)
			elevel1->level = 0;
#endif
			}
		else {
			elevel->next = elevel1;
			elevel1->prev = elevel;
#if MMDebug & (Debug_Script | Debug_PPBuf)
			elevel1->level = elevel->level + 1;
#endif
			}
		elevel = elevel1;
		}
	else {
		elevel = elevel->next;
		if(elevel->fli != NULL) {
			free((void *) elevel->fli);
			elevel->fli = NULL;
			}
		}

	elevel->ifwastrue = elevel->elseseen = false;
	elevel->loopcount = 0;
	*elevelp = elevel;

	return rc.status;
	}

// Return to the most recent loop level (bypassing "if" levels).  Top level should never be reached.
static int prevlevel(LevelInfo **elevelp) {
	LevelInfo *elevel = *elevelp;

	for(;;) {
		if(elevel->prev == NULL)

			// Huh?  Loop level not found! (This is a bug.)
			return rcset(Failure,RCNoFormat,text114);
				// "Prior loop execution level not found while rewinding stack" 
		if(elevel->loopspawn)
			break;
		elevel = elevel->prev;
		}
	*elevelp = elevel;
	return rc.status;
	}

// Parse one macro usage string.
static int getmactext(Datum *datum,enum e_sym sym,ushort *np) {
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
		(void) funcarg(datum,ArgFirst | ArgNotNull1);
	return rc.status;
	}

// Parse first line of a macro definition and return status.  bflag is assumed to be either BFConstrain or zero.  Example
// definition:
//	macro grepFiles(2,)\
//	 {usage: 'pat,glob',desc: 'Find files which match pattern'}
//		...
//	endmacro
static int getmacro(Datum *rval,Buffer **mbufp,ushort bflag) {
	UnivPtr univ;
	MacInfo *mip;
	bool whitespace,exists;
	Datum *datum1,*datum2;
	ushort n1 = 0,n2 = 0;
	short minArgs = 0,maxArgs = -1;

	if(dnewtrk(&datum1) != 0 || dnewtrk(&datum2) != 0)
		goto LibFail;

	// Get macro name.
	if(!havesym(s_ident,false) && !havesym(s_identq,true))
		return rc.status;
	if(strlen(last->p_tok.d_str) > MaxBufName - 1)
		return rcset(Failure,RCTermAttr,text232,last->p_tok.d_str,MaxBufName - 1);
			// "Macro name '~b%s~B' cannot exceed %d characters"

	// Construct the buffer header (and name) in rval temporarily.
	if(dsalloc(rval,strlen(last->p_tok.d_str) + 3) != 0)
		goto LibFail;
	rval->d_str[0] = TokC_ComLine;
	rval->d_str[1] = SBMacro;
	strcpy(rval->d_str + 2,last->p_tok.d_str);

	// Get argument count.
	whitespace = havewhite();
	if(getsym() != NotFound) {
		if(rc.status != Success)
			return rc.status;
		if(havesym(s_lparen,false)) {
			if(whitespace)
				return rcset(Failure,0,text4,"'('"," ");
						// "%s expected (at token '%s')"
			if(getsym() < NotFound)
				return rc.status;
			if(!needsym(s_rparen,false)) {
				if(funcarg(datum1,ArgFirst | ArgInt1) != Success)
					return rc.status;
				if((minArgs = datum1->u.d_int) < 0)
					goto Err;
				if(!havesym(s_comma,false))
					maxArgs = minArgs;
				else {
					if(getsym() < NotFound || (!havesym(s_rparen,false) &&
					 funcarg(datum2,ArgFirst | ArgInt1) != Success))
						return rc.status;
					if(datum2->d_type != dat_nil)
						if((maxArgs = datum2->u.d_int) < 0) {
							minArgs = maxArgs;
Err:
							return rcset(Failure,0,text39,text206,(int) minArgs,0);
								// "%s (%d) must be %d or greater","Macro argument count"
							}
					}
				if(!needsym(s_rparen,true))
					return rc.status;
				if(maxArgs >= 0 && minArgs > maxArgs)
					return rcset(Failure,RCNoFormat,text386);
						// "Macro declaration contains invalid argument range"
				}
			}
		if(havesym(s_any,false)) {

			// Get usage string(s).
			if(getmactext(datum1,s_lbrace,&n1) != Success)
				return rc.status;
			if(havesym(s_comma,false)) {
				n2 = n1;
				if(getmactext(datum2,s_comma,&n2) != Success)
					return rc.status;
				}
			if(!needsym(s_rbrace,true) || extrasym())
				return rc.status;
			}
		}

	// Existing command, function, alias, macro, or user variable of same name as this macro?
	if(((exists = execfind(rval->d_str + 2,OpQuery,PtrAny,&univ)) && (!(univ.p_type & PtrMacro) ||
	 !(mi.cache[MdIdxClob]->ms_flags & MdEnabled))) || uvarfind(rval->d_str + 2) != NULL)
		return rcset(Failure,RCTermAttr,text165,rval->d_str + 2);
			// "Name '~b%s~B' already in use"

	// Create a hidden buffer, make sure it's empty, and has the correct pointer type in exectab.
	if(bfind(rval->d_str + 1,BS_Create | BS_Extend,bflag | BFHidden | BFMacro | BFReadOnly,mbufp,NULL) != Success)
		return rc.status;
	if(exists) {
		if(bclear(*mbufp,BC_IgnChgd | BC_Unnarrow | BC_ClrFilename) != Success)
			return rc.status;
		univ.p_type = bflag ? PtrMacro_C : PtrMacro_O;
		(void) execfind(rval->d_str + 2,OpUpdate,0,&univ);
		(*mbufp)->b_flags = ((*mbufp)->b_flags & ~BFConstrain) | bflag;
		}

	// Set the macro parameters.
	mip = (*mbufp)->b_mip;
	mip->mi_minArgs = minArgs;
	mip->mi_maxArgs = maxArgs;
	if(n1)
		datxfer(n1 == 1 ? &mip->mi_usage : &mip->mi_desc,datum1);
	if(n2)
		datxfer(n2 == 1 ? &mip->mi_usage : &mip->mi_desc,datum2);
	rval->d_str[1] = ' ';

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Parse a "for" line and initialize control variables.  Return status.
static int initfor(ForLoopInfo **flip,Datum *datum) {
	ENode node;
	ForLoopInfo *fli;

	if((fli = *flip) == NULL) {
		if((fli = (ForLoopInfo *) malloc(sizeof(ForLoopInfo))) == NULL)
			return rcset(Panic,0,text94,"initfor");
				// "%s(): Out of memory!"
		*flip = fli;
		}

	// Get the variable name and "in" keyword.
	if(!havesym(s_any,true) || findvar(last->p_tok.d_str,&fli->vd,OpCreate) != Success || getsym() < NotFound ||
	 !needsym(kw_in,true))
		return rc.status;

	// Get (array) expression.
	nodeinit(&node,datum,false);
	if(ge_assign(&node) != Success || extrasym() || !aryval(node.en_rp))
		return rc.status;

	// Finish up.
	fli->ary = awptr(datum)->aw_ary;
	fli->index = 0;
	return rc.status;
	}

// Get next array element and set "for" loop control variable to it if found; otherwise, return NotFound.  Return status.
static int nextfor(ForLoopInfo *fli) {
	Datum *datum;

	if(fli->index == fli->ary->a_used) {
		fli->index = -1;	// Invalidate control object.
		return NotFound;
		}
	if((datum = aget(fli->ary,fli->index++,false)) == NULL)
		return librcset(Failure);
	return putvar(datum,&fli->vd);
	}

// Execute a compiled buffer, save result in rval, and return status.
static int xbuf(Datum *rval,Buffer *buf,uint flags) {
	Line *blnp,*elnp;		// Pointers to beginning and ending line(s) to execute, allowing for continuation lines.
	int forcecmd;			// "force" statement?
	enum e_sym kwid;		// Keyword index.
	int len;			// Line length.
	bool isWhiteLine;		// True if current line is all white space or a comment.
	bool lastWasCL = false;		// Last line was a continuation line.
	bool thisIsCL = false;		// Current line is a continuation line.
	bool go;			// Used to evaluate "while" and "until" truth.
	int breaklevel;			// Number of levels to "break".
	ushort constrained;		// "constrain" modifier?
	LevelInfo *execlevel = NULL;	// Level table.
	LevelInfo *elevel;		// Current level.
	LoopBlock *lbp;
	Buffer *mbuf = NULL;		// "macro" buffer pointer.
	Datum *datum;			// Work datum object.
	Datum *dline;			// Null-terminated buffer line (Datum object).
	char *eline;			// Work pointer.
	char *eline1;			// Beginning of expression statement (past any statement keyword).
	char *str;
	Parse *oldlast,newlast;
	ENode node;

	// Prepare for execution.
	if(dnewtrk(&datum) != 0 || dnewtrk(&dline) != 0)
		return librcset(Failure);
	if(nextlevel(&execlevel) != Success)
		return rc.status;
	elevel = execlevel;
	elevel->live = true;			// Set initial (top) execution level to true.
	elevel->loopspawn = false;
	breaklevel = 0;
	kentry.thisflag = kentry.lastflag;	// Let the first command inherit the flags from the last one.
	newlast.p_cl = NULL;

	// Now EXECUTE the buffer.
#if MMDebug & (Debug_Script | Debug_PPBuf)
	static uint instance = 0;
	uint myinstance = ++instance;
	fprintf(logfile,"*** BEGIN xbuf(%u,%s): %s...\n",myinstance,buf->b_bname,showflags(&newlast));
#endif
	blnp = elnp = buf->b_lnp;
	do {
		// Skip blank lines and comments unless "salting" a macro.
		eline = elnp->l_text;
		len = elnp->l_used;
#if MMDebug & (Debug_Script | Debug_PPBuf)
		fprintf(logfile,"%.8X [level %u.%u] >> %.*s\n",(uint) elnp,myinstance,elevel->level,len,eline);
#endif
		isWhiteLine = false;
		if((len = skipwhite(&eline,len)) == 0 || *eline == TokC_ComLine) {
			isWhiteLine = true;
			thisIsCL = false;
			if(!lastWasCL) {
				if(mbuf != NULL)
					goto Salt0;
				goto NextLn;
				}
			}
		else {
			if(lastWasCL) {
				// Undo skipwhite().
				eline = elnp->l_text;
				len = elnp->l_used;
				}
			if((thisIsCL = (eline[len - 1] == '\\')))
				if(elnp->l_next == NULL) {
					(void) rcset(Failure,0,text405,len,eline);
						// "Incomplete line \"%.*s\""
					goto RCExit;
					}
			}

		// Allocate eline as a Datum object, null terminated, so that it can be parsed properly by getsym().
		if(!lastWasCL) {
			if(dsetsubstr(eline,thisIsCL ? len - 1 : len,dline) != 0)
				goto DErr;
			blnp = elnp;
			}
		else if(!isWhiteLine) {
			DStrFab sfab;

			// Append current line to dline.
			if(dopenwith(&sfab,dline,SFAppend) != 0 ||
			 dputmem((void *) eline,thisIsCL ? len - 1 : len,&sfab) != 0 || dclose(&sfab,sf_string) != 0) {
DErr:
				(void) librcset(Failure);
				goto RCExit;
				}
			}

		// Check for "endmacro" keyword now so that "salting" can be turned off if needed; otherwise, check for
		// statement keywords only if current line is not a continuation line.
		eline = eline1 = dline->d_str;
		if((kwid = kwfind(&eline1,strlen(eline1))) != s_nil) {
			if(thisIsCL) {
				if(kwid == kw_endmacro && elevel->live) {
					mbuf = NULL;
					goto NextLn;
					}
				}
			else {
				// Have complete line in dline.  Check for "constrain" modifier.  If present, assume "macro"
				// follows because it was already checked in ppbuf().
				constrained = 0;
				if(kwid == kw_constrain) {
					constrained = BFConstrain;
					while(*eline1 == ' ' || *eline1 == '\t')
						++eline1;
					kwid = kwfind(&eline1,strlen(eline1));
					}

				// Process any "macro" and "endmacro" statements.
				if(parsebegin(&oldlast,&newlast,eline1,TokC_ComLine) != Success)
					goto RCExit;
				switch(kwid) {
					case kw_macro:
						// Skip if current level is false.
						if(!elevel->live)
							goto NextLn;

						// Parse line...
						if(getmacro(rval,&mbuf,constrained) != Success)
							goto RCExit;

						// and store macro name as a comment at the top of the new buffer.
						len = strlen(eline = rval->d_str);
						goto Salt1;
					case kw_endmacro:

						// Extra arguments?
						if(extrasym())
							goto RCExit;

						// Check execution level.
						if(elevel->live) {

							// Turn off macro storing.
							mbuf = NULL;
							goto NextLn;
							}
						break;
					default:
						;	// Do nothing.
					}
				}
			} // End statement keyword parsing.

		// If macro store is on, just salt this (entire) line away.
		if(mbuf != NULL) {
			Line *mp;
Salt0:
			// Save line verbatim (as a debugging aid) but skip any leading tab, if possible.
			eline = elnp->l_text;
			if((len = elnp->l_used) > 0 && !lastWasCL && *eline == '\t') {
				++eline;
				--len;
				}
Salt1:
			// Allocate the space for the line.
			if(lalloc(len,&mp) != Success)
				return rc.status;

			// Copy the text into the new line.
			memcpy(mp->l_text,eline,len);
			if(eline == rval->d_str)
				dsetnil(rval);

			// Attach the line to the end of the buffer.
			llink(mp,mbuf,mbuf->b_lnp->l_prev);
			goto NextLn;
			}
		if(thisIsCL)
			goto NextLn;

		// Not "salting" and not a "macro" or "endmacro" keyword.  Check others.
		forcecmd = false;
		if(kwid != s_nil) {
			switch(kwid) {

				// Statements in the if/else family.
				case kw_if:	// "if" keyword.

					// Begin new level.
					if(nextlevel(&elevel) != Success)
						goto RCExit;
					elevel->loopspawn = false;

					// Prior level true?
					if(elevel->prev->live) {
EvalIf:
						// Yes, grab the value of the logical expression.
						nodeinit(&node,datum,false);
						if(ge_andor(&node) != Success || extrasym())
							goto RCExit;

						// Set this level to the result of the expression and update "ifwastrue" if the
						// result is true.
						if((elevel->live = tobool(datum)))
							elevel->ifwastrue = true;
						}
					else
						// Prior level was false so this one is, too.
						elevel->live = false;
					goto NextLn;
				case kw_else:	// else keyword.

					// At top level, initiated by a loop statement, or duplicate else?
					if(elevel == execlevel || elevel->loopspawn || elevel->elseseen) {

						// Yep, bail out.
						str = "else";
						eline = text198;
							// "Misplaced '~b%s~B' keyword"
						goto Misplaced;
						}

					// Extra arguments?
					if(extrasym())
						goto RCExit;

					// Update current execution state.  Can be true only if (1), prior state is true; (2),
					// current state is false; and (3), this (possibly compound) "if" statement was never
					// true.
					elevel->live = elevel->prev->live && !elevel->live && !elevel->ifwastrue;
					elevel->elseseen = true;
					goto NextLn;
				case kw_elsif:	// "elsif" keyword.

					// At top level, initiated by a loop statement, or "elsif" follows "else"?
					if(elevel == execlevel || elevel->loopspawn || elevel->elseseen) {

						// Yep, bail out.
						str = "elsif";
						eline = text198;
							// "Misplaced '~b%s~B' keyword"
						goto Misplaced;
						}

					// Check expression if prior level is true, current level is false, and this compound if
					// statement has not yet been true.
					if(elevel->prev->live && !elevel->live && !elevel->ifwastrue)
						goto EvalIf;

					// Not evaluating so set current execution state to false.
					elevel->live = false;
					goto NextLn;
				case kw_endif:	// "endif" keyword.

					// At top level or was initiated by a loop statement?
					if(elevel == execlevel || elevel->loopspawn) {

						// Yep, bail out.
						str = "endif";
						eline = text198;
							// "Misplaced '~b%s~B' keyword"
						goto Misplaced;
						}

					// Extra arguments?
					if(extrasym())
						goto RCExit;

					// Done with current level: return to previous one.
					elevel = elevel->prev;
					goto NextLn;

				// Statements that begin or end a loop.
				case kw_loop:	// "loop" keyword.

					// Skip unless current level is true.
					if(!elevel->live)
						goto JumpDown;

					// Error if extra arguments; otherwise, begin next loop.
					if(extrasym())
						goto RCExit;
					goto NextLoop;
				case kw_for:	// "for" keyword

					// Current level true?
					if(elevel->live) {
						int status;

						// Yes, parse line and initialize loop controls if first time.
						if((elevel->fli == NULL || elevel->fli->index < 0) &&
						 initfor(&elevel->fli,datum) != Success)
							goto RCExit;

						// Set control variable to next array element and begin next loop, or break if
						// none left.
						if((status = nextfor(elevel->fli)) == Success)
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
					if(elevel->live) {

						// Yes, get the "while" or "until" logical expression.
						nodeinit(&node,datum,false);
						if(ge_andor(&node) != Success || extrasym())
							goto RCExit;

						// If the "while" condition is true or the "until" condition if false, begin a
						// new level, set it to true, and continue.
						if(tobool(datum) == go) {
NextLoop:
							if(nextlevel(&elevel) != Success)
								goto RCExit;
							elevel->live = true;
							elevel->loopspawn = true;
							goto NextLn;
							}
						}

					// Current level or condition is false: skip this block.
					goto JumpDown;

				case kw_break:	// "break" keyword.
				case kw_next:	// "next" keyword.

					// Ignore if current level is false (nothing to jump out of).
					if(elevel->live == false)
						goto NextLn;

					// Check "break" argument.
					if(kwid == kw_break) {
						if(havesym(s_any,false)) {
							nodeinit(&node,datum,false);
							if(ge_assign(&node) != Success || extrasym() || !intval(datum))
								goto RCExit;
							if(datum->u.d_int <= 0) {
								(void) rcset(Failure,0,text217,datum->u.d_int);
									// "'break' level '%ld' must be 1 or greater"
								goto RCExit;
								}
							breaklevel = datum->u.d_int;
							}
						else
							breaklevel = 1;

						// Invalidate any "for" loop in progress.
						if(elevel->prev->fli != NULL)
							elevel->prev->fli->index = -1;
						}

					// Extraneous "next" argument(s)?
					else if(extrasym())
						goto RCExit;
JumpDown:
					// Continue or break out of loop: find the right block (kw_while, kw_until, kw_loop,
					// kw_for, kw_break, or kw_next) and jump down to its "endloop".
					for(lbp = buf->b_mip->mi_exec; lbp != NULL; lbp = lbp->lb_next)
						if(lbp->lb_mark == blnp) {

							// If this is a "break" or "next", set the line pointer to the "endloop"
							// line so that the "endloop" is executed; otherwise, set it to the line
							// after the "endloop" line so that the "endloop" is bypassed.
							elnp = (kwid & SBreakType) ? lbp->lb_jump : lbp->lb_jump->l_next;

							// Return to the most recent loop level (bypassing "if" levels) if this
							// is a "break" or "next"; otherwise, just reset the loop counter.
							if(!(kwid & SBreakType))
								elevel->loopcount = 0;
							else if(prevlevel(&elevel) != Success)
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
						if(elevel == execlevel || !elevel->loopspawn) {

							// Yep, bail out.
							str = "endloop";
							eline = text198;
								// "Misplaced '~b%s~B' keyword"
Misplaced:
							(void) rcset(Failure,RCTermAttr,eline,str);
							goto RCExit;
							}

						// Return to the previous level and check if maxloop exceeded.
						elevel = elevel->prev;
						if(maxloop > 0 && ++elevel->loopcount > maxloop) {
							(void) rcset(Failure,0,text112,maxloop);
								// "Maximum number of loop iterations (%d) exceeded!"
							goto RCExit;
							}
						}

					// We're good... just find the "loop", "for", "while" or "until", and go back to it, or
					// to a prior level if we're processing a "break".
					for(lbp = buf->b_mip->mi_exec; lbp != NULL; lbp = lbp->lb_next)
						if((lbp->lb_type & SLoopType) && lbp->lb_jump == blnp) {

							// while/until/loop/for block found: set the line pointer to the line
							// after its "endloop" line or its parent's "endloop" line if we're
							// processing a "break" (and return to the previous execution level);
							// otherwise, back to the beginning.
							if(breaklevel > 0) {
								if(--breaklevel > 0) {
									if(lbp->lb_break == NULL) {
										(void) rcset(Failure,0,text225,breaklevel);
									// "Too many break levels (%d short) from inner 'break'"
										goto RCExit;
										}
									elnp = lbp->lb_break;

									// Return to the most recent loop level before this one.
									elevel = elevel->prev;
									if(prevlevel(&elevel) != Success)
										goto RCExit;
									}
								else {
									elnp = elnp->l_next;
									elevel = elevel->prev;
									}
								elevel->loopcount = 0;	// Reset the loop counter.
								}
							else
								elnp = lbp->lb_mark;
							goto Onward;
							}

					// Huh?  "while", "until", "for", or "loop" line not found! (This is a bug.)
					goto BugExit;

				case kw_return:	// "return" keyword.

					// Current level true?
					if(elevel->live == true) {

						// Yes, get return value, if any.
						if(!havesym(s_any,false))
							dsetnil(rval);
						else {
							nodeinit(&node,rval,false);
							if(ge_andor(&node) != Success || extrasym())
								goto RCExit;
							}

						// Exit script.
						goto Exit;
						}

					// Nope, ignore "return".
					goto NextLn;

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

		// A "force" or not a statement keyword.  Execute the line as an expression statement if "true" state.
		if(elevel->live) {
			(void) (newlast.p_cl == NULL ? execestmt(rval,eline1,TokC_ComLine,NULL) : xestmt(rval));

			// Check for exit or a command error.
			if(rc.status <= MinExit)
				return rc.status;

			if(forcecmd)		// Force Success status.
				rcclear(0);
			if(rc.status != Success) {
				EWindow *win = si.whead;

				// Check if buffer is on-screen.
				do {
					if(win->w_buf == buf) {

						// Found a window.  Set point to error line.
						win->w_face.wf_point.lnp = blnp;
						win->w_face.wf_point.off = 0;
						win->w_flags |= WFMove;
						}
					} while((win = win->w_next) != NULL);

				// In any case, set the buffer point.
				buf->b_face.wf_point.lnp = blnp;
				buf->b_face.wf_point.off = 0;

				// Build a more detailed message that includes the command error message, if any.
				(void) macerror(NULL,NULL,buf,blnp,flags);
				goto Exit;
				}
			} // End statement execution.
NextLn:
		elnp = elnp->l_next;
Onward:
		// On to the next line.  eline and len point to the original line.
		if(!(lastWasCL = thisIsCL) && newlast.p_cl != NULL) {
			parseend(oldlast);
			newlast.p_cl = NULL;
			}
		} while(elnp != NULL);		// End buffer execution.

	// "if" and "endif" match?
	if(elevel == execlevel)
		goto Exit;
	(void) rcset(Failure,RCTermAttr,text121,text199);
		// "Unmatched '~b%s~B' keyword","if"

	// Clean up and exit per rc.status.
RCExit:
	(void) macerror(NULL,NULL,buf,blnp,flags);
Exit:
#if MMDebug & (Debug_Script | Debug_PPBuf)
	fprintf(logfile,"*** END xbuf(%u,%s), rc.msg \"%s\"...\n",myinstance,buf->b_bname,rc.msg.d_str);
#endif
	llfree(execlevel);
	if(newlast.p_cl != NULL)
		parseend(oldlast);

	return (rc.status == Success) ? rc.status : rcset(ScriptError,0,NULL);
	}

// Execute the contents of a buffer, given Datum object for return value, numeric argument, buffer pointer, pathname of file
// being executed (or NULL), and invocation flags.
int execbuf(Datum *rval,int n,Buffer *buf,char *runpath,uint flags) {

	// Check for narrowed buffer and runaway recursion.
	if(buf->b_flags & BFNarrowed)
		(void) rcset(Failure,RCNoFormat,text72);
			// "Cannot execute a narrowed buffer"
	else if(maxmacdepth > 0 && buf->b_mip != NULL && buf->b_mip->mi_nexec >= maxmacdepth)
		(void) rcset(Failure,0,text319,text336,maxmacdepth);
			// "Maximum %s recursion depth (%d) exceeded","macro"
	else {
		Datum margs;

		// Create buffer-extension record if needed.
		if(buf->b_mip == NULL && bextend(buf) != Success)
			return rc.status;

		// Get macro arguments.
		dinit(&margs);		// No need to dclear() this later because it will always contain a dat_blobRef.
#if MMDebug & Debug_MacArg
		if(margload(n,buf,&margs,flags) == Success) {
#else
		if(margload(buf,&margs,flags) == Success) {
#endif
			// If evaluating, preprocess buffer if needed.
			if((buf->b_flags & BFPreproc) || ppbuf(buf,flags) == Success) {
				ScriptRun *oldrun,newrun;

				// Make new run instance and prepare for execution.
				oldrun = scriptrun;
				scriptrun = &newrun;
				scriptrun->marg = &margs;
				scriptrun->path = fixnull(runpath);
				scriptrun->buf = buf;
				if(dnew(&scriptrun->narg) != 0)
					return librcset(Failure);		// Fatal error.
				dsetint(n,scriptrun->narg);
				scriptrun->uvar = lvarshead;

				// Flag that we are executing the buffer and execute it.
				++buf->b_mip->mi_nexec;
#if MMDebug & Debug_Script
				fprintf(logfile,"execbuf(): Calling xbuf(%s): %s...\n",buf->b_bname,showflags(NULL));
#endif
				(void) xbuf(rval,buf,flags);			// Execute the buffer.
#if MMDebug & Debug_Script
				fprintf(logfile,"execbuf(): xbuf() returned %s result and status %d \"%s\".\n",
				 dtype(rval,false),rc.status,rc.msg.d_str);
#endif
				(void) uvarclean(scriptrun->uvar);		// Clear any local vars that were created.

				// Clean up.
				--buf->b_mip->mi_nexec;
#if DCaller
				ddelete(scriptrun->narg,"execbuf");
#else
				ddelete(scriptrun->narg);
#endif
				scriptrun = oldrun;
				}
			}
		}

	return rc.status;
	}

#if MMDebug & (Debug_Datum | Debug_Script | Debug_MacArg | Debug_PPBuf)
// Call execbuf() with debugging.
int debug_execbuf(Datum *rval,int n,Buffer *buf,char *runpath,uint flags,char *caller) {

	fprintf(logfile,"*execbuf() called by %s() with args: n %d, b_bname %s, runpath '%s', flags %.8X, %s...\n",
	 caller,n,buf->b_bname,runpath,flags,showflags(NULL));
	(void) execbuf(rval,n,buf,runpath,flags);
	fprintf(logfile,"execbuf(%s) from %s() returned %s result and status %d \"%s\".\n",
	 buf->b_bname,caller,dtype(rval,false),rc.status,rc.msg.d_str);
	return rc.status;
	}
#endif

// Yank a file into a buffer and execute it, given result pointer, filename, n argument, and macro invocation "at startup" flag
// (used for error reporting).  Suppress any extraneous messages during read.  If there are no errors, delete the buffer on
// exit.
int execfile(Datum *rval,char *fname,int n,uint flags) {
	Buffer *buf;

	if(bfind(fname,BS_Create | BS_Force | BS_Derive,BFHidden,	// Create a hidden buffer...
	 &buf,NULL) != Success)
		return rc.status;
	ushort mflag = mi.cache[MdIdxRtnMsg]->ms_flags & MdEnabled;	// save current message flag...
	gmclear(mi.cache[MdIdxRtnMsg]);					// clear it...
	(void) readin(buf,fname,RWKeep | RWNoHooks | RWExist);		// read the file...
	if(mflag)
		gmset(mi.cache[MdIdxRtnMsg]);				// restore message flag...
	if(rc.status == Success) {
#if MMDebug & Debug_Script
		(void) debug_execbuf(rval,n,buf,buf->b_fname,flags,"execfile");
#else
		(void) execbuf(rval,n,buf,buf->b_fname,flags);		// and execute the buffer.
#endif
		}

	// If not displayed, remove the now unneeded buffer and return.
	buf->b_flags &= ~BFHidden;
	if(rc.status >= Cancelled && buf->b_nwind == 0)
		(void) bdelete(buf,BC_IgnChgd);
	return rc.status;
	}

// Execute commands in a file and return result in rval.
int xeqFile(Datum *rval,int n,Datum **argv) {
	char *path;

	// Get the filename.
	if(!(si.opflags & OpScript)) {
		TermInp ti = {NULL,RtnKey,MaxPathname,NULL};

		if(terminp(rval,text129,ArgNotNull1 | ArgNil1,Term_C_Filename,&ti) != Success || rval->d_type == dat_nil)
				// "Execute script file"
			return rc.status;
		}
	else if(funcarg(rval,ArgFirst | ArgNotNull1 | ArgPath) != Success)
		return rc.status;

	// Look up the path...
	if(pathsearch(rval->d_str,0,(void *) &path,NULL) != Success)
		return rc.status;
	if(path == NULL)
		return rcset(Failure,0,text152,rval->d_str);	// Not found.
			// "No such file \"%s\""

	// save it...
	if(dsetstr(path,rval) != 0)
		return librcset(Failure);

	// and execute it with arg n.
	return execfile(rval,rval->d_str,n,0);
	}
