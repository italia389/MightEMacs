// (c) Copyright 2018 Richard W. Marinelli
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

#if MMDebug & (Debug_Datum | Debug_Script | Debug_MacArg | Debug_PPBuf)
static char *showflags(Parse *newp) {
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
	if(newp != NULL)
		sprintf(str,", newlast.p_flags %.4hX",newp->p_flags);
	return wkbuf;
	}
#endif

// Search hook table for given name.  If found, set *hrpp to pointer; otherwise, return an error.
static int findhook(char *name,HookRec **hrpp) {
	HookRec *hrp = hooktab;

	do {
		if(strcasecmp(name,hrp->h_name) == 0) {
			*hrpp = hrp;
			return rc.status;
			}
		} while((++hrp)->h_name != NULL);

	return rcset(Failure,0,text317,name);
			// "No such hook '%s'"
	}

// Check if a macro is bound to a hook and return true if so (and set error if isError is true), otherwise, false.
bool ishook(Buffer *bufp,bool isError) {
	HookRec *hrp = hooktab;

	do {
		if(bufp == hrp->h_bufp) {
			if(isError)
				(void) rcset(Failure,0,text327,hrp->h_name);
						// "Macro bound to '%s' hook - cannot delete"
			return true;
			}
		} while((++hrp)->h_name != NULL);

	return false;
	}

// Set a hook.  Return status.
int setHook(Datum *rp,int n,Datum **argpp) {
	HookRec *hrp;
	UnivPtr univ;			// Pointer to the requested macro.
	Datum *datp;

	// First, get the hook (name) to set.
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript)
		datxfer(datp,argpp[0]);
	else if(terminp(datp,text318,ArgNotNull1 | ArgNil1,0,NULL) != Success || datp->d_type == dat_nil)
			// "Set hook "
		return rc.status;

	// Valid?
	if((si.opflags & (OpScript | OpEval)) != OpScript && findhook(datp->d_str,&hrp) != Success)
		return rc.status;

	// Get the macro name.  If interactive mode, build "progress" prompt first.
	if(si.opflags & OpScript) {
		if(!needsym(s_comma,true))
			return rc.status;
		if(havesym(kw_nil,false)) {
			univ.u.p_bufp = NULL;
			(void) getsym();
			}
		else
			(void) getcfm(NULL,&univ,PtrMacro);
		}
	else {
		char wkbuf[NWork + 1];

		sprintf(wkbuf,"%s%s %s %s",text318,datp->d_str,text339,text336);
					// "Set hook ","to","macro",
		(void) getcfm(wkbuf,&univ,PtrMacro_C);
		}

	if(rc.status == Success) {

		// Constrained macro?
		if(univ.u.p_bufp != NULL && !(univ.u.p_bufp->b_flags & BFConstrain))
			return rcset(Failure,RCTermAttr,"%s%s '~b%s~0'",text420,text414,univ.u.p_bufp->b_bname + 1);
					// "Hook may not be set to non-","constrained macro"

		// Unless script mode and not evaluating, set the hook.
		if((si.opflags & (OpScript | OpEval)) != OpScript)
			hrp->h_bufp = univ.u.p_bufp;
		}

	return rc.status;
	}

// Find carat or null byte in given string and return pointer to it.
static char *carat(char *str) {

	while(*str != '\0') {
		if(*str == '^')
			break;
		++str;
		}
	return str;
	}

// Build and pop up a buffer containing a list of all hooks, their associated macros, and descriptions of their arguments (if
// any).  Return status.
int showHooks(Datum *rp,int n,Datum **argpp) {
	Buffer *slistp;
	char *strn0,*strn,*strm0,*strm;
	HookRec *hrp = hooktab;
	DStrFab rpt;
	int indent;

	// Get a buffer and open a string-fab object.
	if(sysbuf(text316,&slistp,BFTermAttr) != Success)
			// "Hooks"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;

	// Construct the header lines.
	if(dputs(text315,&rpt) != 0 || dputc('\n',&rpt) != 0 ||
	    // "~bHook Name  Set to Macro          Numeric Argument       Macro Arguments~0"
	 dputs("---------  --------------------  ---------------------  ------------------------\n",&rpt) != 0)
		goto LibFail;

	// For all hooks...
	do {
		if(dputf(&rpt,"%-11s%-21s",hrp->h_name,hrp->h_bufp == NULL ? "" : hrp->h_bufp->b_bname + 1) != 0)
			goto LibFail;
		indent = 1;
		strn = hrp->h_narg;
		strm = hrp->h_margs;
		for(;;) {
			strn = carat(strn0 = strn);
			strm = carat(strm0 = strm);
			if(dputf(&rpt,"%*s%-23.*s",indent,"",(int) (strn - strn0),strn0) != 0)
				goto LibFail;
			if(strm != strm0 && dputf(&rpt,"%.*s",(int) (strm - strm0),strm0) != 0)
				goto LibFail;
			if(dputc('\n',&rpt) != 0)
				goto LibFail;
			if(*strn == '\0' && *strm == '\0')
				break;
			indent = 33;
			if(*strn == '^')
				++strn;
			if(*strm == '^')
				++strm;
			}

		// On to the next hook.
		} while((++hrp)->h_name != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(slistp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n,slistp,RendNewBuf | RendReset);
	}

// Disable a hook that went awry.  Return status.
static int dishook(HookRec *hrp) {
	DStrFab msg;
	int rcode = 0;

	// Hook execution failed, let the user know what's up.
	if(rc.status > FatalError) {
		if((rcode = dopentrk(&msg)) == 0 && escattr(&msg) == Success) {
			if(disempty(&msg))
				rcode = dputf(&msg,text176,hrp->h_bufp->b_bname + 1);
						 // "Macro '~b%s~0' failed"
#if MMDebug & Debug_Datum
			if(rcode == 0) {
				fprintf(logfile,"dishook(): calling dputf(...,%s,%s)...\n",text161,hrp->h_name);
				rcode = dputf(&msg,text161,hrp->h_name);
				ddump(msg.sf_datp,"dishook(): post dputf()...");
				}
			if(rcode == 0) {
				rcode = dclose(&msg,sf_string);
				fprintf(logfile,"dishook(): dclose() returned rc.msg '%s', status %d.\n",rc.msg.d_str,
				 rc.status);
#else
			if(rcode == 0 && (rcode = dputf(&msg,text161,hrp->h_name)) == 0 &&
							// " (disabled '%s' hook)"
			 (rcode = dclose(&msg,sf_string)) == 0) {
#endif
				(void) rcset(rc.status,RCForce | RCNoFormat | RCTermAttr,msg.sf_datp->d_str);
				}
			}
		}

	// disable the hook...
	hrp->h_bufp = NULL;

	// and set library error if needed.
	return (rcode == 0) ? rc.status : librcset(Failure);
	}

// Execute a macro bound to a hook with rp (if not NULL) and n.  If arginfo == 0, invoke the macro directly; otherwise, build a
// command line with arguments.  In the latter case, arginfo contains the argument count and types of each argument (string,
// long, or Datum *).  The lowest four bits of arginfo comprise the argument count.  Each pair of higher bits is 0 for a string
// argument, 1 for a long (integer) argument, and 2 for a Datum pointer argument.
int exechook(Datum *rp,int n,HookRec *hrp,uint arginfo,...) {
	bool fscall = false;		// execCF() or execestmt() call?

	// Nothing to do if hook not set or macro already running.
	if(hrp->h_bufp == NULL || hrp->h_bufp->b_mip->mi_nexec > 0) {
		if(rp != NULL)
			dsetnil(rp);
		return rc.status;
		}
	if(rp == NULL && dnewtrk(&rp) != 0)		// For throw-away return value.
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
		 dputs(hrp->h_bufp->b_bname + 1,&cmd) != 0)
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
		fprintf(logfile,"exechook(): calling execestmt(%s)...\n",cmd.sf_datp->d_str);
		(void) execestmt(rp,cmd.sf_datp->d_str,TokC_ComLine,NULL);
		fprintf(logfile,"exechook(): execestmt() returned rc.msg '%s', status %d.\n",
		 rc.msg.d_str,rc.status);
		if(rc.status != Success)
#else
		if(execestmt(rp,cmd.sf_datp->d_str,TokC_ComLine,NULL) != Success)
#endif
			goto Fail;
		fscall = true;
		}
	else {
		uint oldnoload = si.opflags & OpNoLoad;
		si.opflags |= OpNoLoad;
		(void) execbuf(rp,n,hrp->h_bufp,NULL,ArgFirst);
		si.opflags = (si.opflags & ~OpNoLoad) | oldnoload;
		if(rc.status != Success)
			goto Fail;
		}

	// Successful execution.  Save return message and check for false return.
	if((!fscall && (si.opflags & OpScript) && rcsave() != Success) || rp->d_type != dat_false)
		return rc.status;	// Fatal error or success.

	// Hook returned false... big trouble.
	(void) rcset(Failure,RCNoFormat | RCKeepMsg,text300);
			// "False return"
Fail:
	// Execution failed, let the user know what's up.
	return dishook(hrp);
LibFail:
	return librcset(Failure);
	}

// Check if given command, alias, or macro execution object can be run interactively.  Set an error and return false if not;
// otherwise, return true.
static bool canRun(UnivPtr *univp) {

	// Error if alias to a function, contrained macro, or alias to a constrained macro.
	if((univp->p_type & (PtrAlias_F | PtrMacro_C)) || (univp->p_type == PtrAlias_M &&
	 (univp->u.p_aliasp->a_targ.u.p_bufp->b_flags & BFConstrain))) {
		(void) rcset(Failure,RCTermAttr,"%s%s '~b%s~0'%s",text314,univp->p_type == PtrMacro_C ? text414 :
								// "Cannot run "		"constrained macro"
		 text127,univp->p_type == PtrMacro_C ? univp->u.p_bufp->b_bname + 1 : univp->u.p_aliasp->a_name,text415);
			// "alias"								" interactively"
		return false;
	 	}
	return true;
	}

// Execute a named command, alias, or macro interactively even if it is not bound or is being invoked from a macro.
int run(Datum *rp,int n,Datum **argpp) {
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
		univ = univ.u.p_aliasp->a_targ;
	if(univ.p_type == PtrMacro_O)
		(void) execbuf(rp,n,univ.u.p_bufp,NULL,
		 si.opflags & OpParens ? ArgFirst | SRun_Parens : ArgFirst);	// Call the macro.
	else {									// Call the command.
		CmdFunc *cfp = univ.u.p_cfp;
		if(n != 0 || !(cfp->cf_aflags & CFNCount))
			if(allowedit(cfp->cf_aflags & CFEdit) == Success) {
				(void) execCF(rp,n,cfp,0,0);
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
int eval(Datum *rp,int n,Datum **argpp) {
	Datum *datp;

	// Get the command line.
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript) {

		// Concatenate all arguments into datp.
		if(catargs(datp,1,NULL,0) != Success)
			return rc.status;
		}
	else if(terminp(datp,": ",ArgNotNull1 | ArgNil1,0,NULL) != Success || datp->d_type == dat_nil)
		return rc.status;

	// Execute command line.
#if MMDebug & Debug_Script
	fprintf(logfile,"eval(): Calling execestmt(\"%s\")...\n",datp->d_str);
#endif
	return execestmt(rp,datp->d_str,TokC_ComLine,NULL);
	}

#if MMDebug & Debug_MacArg
// Dump macro argument list to log file.
static int margdump(int n,Buffer *bufp,Datum *datp) {
	Datum arglist;

	fprintf(logfile,"-----\nmargdump(): n: %d, buf: '%s'\n",n,bufp->b_bname);
	dinit(&arglist);
	(void) dquote(&arglist,datp,CvtExpr);
	fprintf(logfile,"Macro argument list: %s\n",arglist.d_str);
	dclear(&arglist);
	return rc.status;
	}
#endif

// Load macro arguments into an array and save in "margs".  If in script mode, evaluate some or all of remaining arguments (if
// any) on command line, dependent on number specified in Buffer record; otherwise, create empty list.  "flags" contains script
// flags and argument flags.  Return status.
#if MMDebug & Debug_MacArg
static int margload(int n,Buffer *bufp,Datum *margs,uint flags) {
#else
static int margload(Buffer *bufp,Datum *margs,uint flags) {
#endif
	Array *aryp;

#if MMDebug & Debug_MacArg
	fprintf(logfile,"*margload(%d,%s,%.8X)...\n",n,bufp->b_bname,flags);
#endif
	// Create array for argument list (which may remain empty).
	if(mkarray(margs,&aryp) != Success)
		return rc.status;

	// Don't get arguments (that don't exist) if executing a hook; that is, OpNoLoad is set.
	if((si.opflags & (OpScript | OpNoLoad)) == OpScript) {
		Datum *argp;
		int argct,minArgs,maxArgs;
		uint aflags = (flags & ArgFirst) | ArgBool1 | ArgArray1 | ArgNIS1;

		if(bufp->b_mip == NULL) {
			minArgs = 0;
			maxArgs = SHRT_MAX;
			}
		else {
			minArgs = bufp->b_mip->mi_minArgs;
			maxArgs = (bufp->b_mip->mi_maxArgs < 0) ? SHRT_MAX : bufp->b_mip->mi_maxArgs;
			}

		// Get arguments until none left.
		if(dnewtrk(&argp) != 0)
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
				if(funcarg(argp,aflags) != Success)
					return rc.status;
				aflags = ArgBool1 | ArgArray1 | ArgNIS1;

				// Got an argument... add it to array and count it.
				if(apush(aryp,argp,true) != 0)
					goto LibFail;
				++argct;
				}

			// Too few or too many arguments found?
			if(argct < minArgs || argct > maxArgs)
				return rcset(Failure,0,text69,last->p_tok.d_str);
					// "Wrong number of arguments (at token '%s')"
			}
		}
	else if((si.opflags & OpNoLoad) && bufp->b_mip != NULL && bufp->b_mip->mi_minArgs > 0)
		return rcset(Failure,0,text69,"");
			// "Wrong number of arguments (at token '%s')"
	si.opflags &= ~OpNoLoad;
#if MMDebug & Debug_MacArg
	return margdump(n,bufp,margs);
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
static int parsebegin(Parse **oldpp,Parse *newp,char *cl,short termch) {

	// Save current parsing state and initialize new one.
	*oldpp = last;
	last = newp;
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
	last->p_datGarbp = datGarbp;
#if MMDebug & Debug_Datum
	fprintf(logfile,"parsebegin(): saved garbage pointer %.8X\n",(uint) datGarbp);
#endif
	// Save old "script mode" flag and enable it.
	last->p_flags = si.opflags & OpScript;
	si.opflags = (si.opflags & ~OpParens) | OpScript;

	// Get first symbol and return.
	(void) getsym();		// Ignore any "NotFound" return.
	return rc.status;
	}

// End a parsing instance and restore previous one.
static void parseend(Parse *oldp) {

	garbpop(last->p_datGarbp);
	dclear(&last->p_tok);
	si.opflags = (si.opflags & ~OpScript) | last->p_flags;
	last = oldp;
#if MMDebug & (Debug_Expr | Debug_Token)
	pinstance(false);
#endif
	}		

// Execute string in current parsing instance as an expression statement.  Low-level routine called by execestmt() and xbuf().
// Return status.
static int xestmt(Datum *rp) {
	ENode node;

	// Set up the default command values.
	kentry.lastflag = kentry.thisflag;
	kentry.thisflag = 0;

	// Evaluate the string (as an expression).
	nodeinit(&node,rp,true);
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
int execestmt(Datum *rp,char *cl,short termch,char **clp) {
	Parse *oldlast,newlast;

#if MMDebug & (Debug_Datum | Debug_Script)
	fprintf(logfile,"execestmt(%.8X,\"%s\",%.4hX,%.8X)...\n",(uint) rp,cl,termch,(uint) clp);
#endif
	// Begin new command line parsing instance.
	if(parsebegin(&oldlast,&newlast,cl,termch) == Success) {

		// Evaluate the line (as an expression).
		(void) xestmt(rp);

		// Restore settings and return.
		if(clp != NULL)
			*clp = last->p_cl;
		}
	parseend(oldlast);
#if MMDebug & Debug_Script
	fputs("execestmt(): EXIT\n",logfile);
#endif
	return rc.status;
	}

// Create or delete an alias in the linked list headed by aheadp and in the execution table and return status.
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
	ap2 = aheadp;
	while(ap2 != NULL) {
		if((result = strcmp(ap2->a_name,aname)) == 0) {

			// Found it.  Check op.
			if(op == OpDelete) {

				// Delete the exectab entry.
				if(execfind(aname,OpDelete,0,NULL) != Success)
					return rc.status;

				// Decrement alias use count on macro, if applicable.
				if(ap2->a_targ.p_type & PtrMacro)
					--ap2->a_targ.u.p_bufp->b_nalias;

				// Delete the alias from the list and free the storage.
				if(ap1 == NULL)
					aheadp = ap2->a_nextp;
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
		ap2->a_targ = *univp;
		ap2->a_type = univp->p_type == PtrCmd ? PtrAlias_C : univp->p_type == PtrFunc ? PtrAlias_F : PtrAlias_M;

		// Add its name to the exectab hash.
		return execfind((univ.u.p_aliasp = ap2)->a_name,OpCreate,univ.p_type = ap2->a_type,&univ);
		}

	// Delete request but alias not found.
	return rcset(Failure,0,text271,aname);
			// "No such alias '%s'"
	}

// Create an alias to a command, function, or macro.  Replace any existing alias if n > 0.
int aliasCFM(Datum *rp,int n,Datum **argpp) {
	UnivPtr univ;
	Datum *dnamep;

	// Get the alias name.
	if(dnewtrk(&dnamep) != 0)
		goto LibFail;
	if(si.opflags & OpScript) {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(dsetstr(last->p_tok.d_str,dnamep) != 0)
			goto LibFail;
		}
	else if(terminp(dnamep,text215,ArgNotNull1 | ArgNil1,0,NULL) != Success || dnamep->d_type == dat_nil)
			// "Create alias "
		return rc.status;

	// Existing function, alias, macro, or user variable of same name?  Exclude aliases from search if n > 0 (a force).
	if((si.opflags & OpEval) && (execfind(dnamep->d_str,OpQuery,n > 0 ? PtrAny & ~PtrAlias : PtrAny,NULL) ||
	 uvarfind(dnamep->d_str) != NULL))
		return rcset(Failure,RCTermAttr,text165,dnamep->d_str);
			// "Name '~b%s~0' already in use"

	if(si.opflags & OpScript) {

		// Get equal sign.
		if(getsym() < NotFound || !havesym(s_any,true))
			return rc.status;
		if(last->p_sym != s_assign)
			return rcset(Failure,RCTermAttr,text23,(cftab + cf_alias)->cf_name,last->p_tok.d_str);
				// "Missing '=' in ~b%s~0 command (at token '%s')"
		if(getsym() < NotFound)			// Past '='.
			return rc.status;

		// Get the command, function, or macro name.
		(void) getcfm(NULL,&univ,PtrCmd | PtrFunc | PtrMacro);
		}
	else {
		char wkbuf[strlen(text215) + strlen(dnamep->d_str) + strlen(text325) + strlen(text313) + 8];

		// Get the command, function, or macro name.
		sprintf(wkbuf,"%s'~b%s~0' %s %s",text215,dnamep->d_str,text325,text313);
					// "Create alias ","of","command, function, or macro"
		(void) getcfm(wkbuf,&univ,PtrCmd | PtrFunc | PtrMacro);
		}

	if(rc.status == Success && univ.p_type != PtrNul) {

		// Delete any existing alias.
		if(n > 0 && execfind(dnamep->d_str,OpQuery,PtrAlias,NULL))
			(void) aupdate(dnamep->d_str,OpDelete,NULL);

		// Create the alias.
		if(aupdate(dnamep->d_str,OpCreate,&univ) != Success)
			return rc.status;

		// Increment alias use count on macro.
		if(univ.p_type & PtrMacro)
			++univ.u.p_bufp->b_nalias;
		}

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Delete one or more aliases or macros.  Set rp to count of items deleted or zero if error.  Return status.
static int deleteAM(Datum *rp,uint selector,char *prmt,char *name,char *emsg) {
	UnivPtr univ;
	uint count = 0;

	if(si.opflags & OpEval)
		dsetint(0L,rp);

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
					if(bdelete(univ.u.p_bufp,BC_IgnChgd) != Success)
						goto Fail;
					}
				else if(aupdate(univ.u.p_aliasp->a_name,OpDelete,NULL) != Success) {
Fail:
					// Don't cause a running script to fail.
					if(rc.status < Failure)
						return rc.status;
					(void) rcset(Success,RCUnFail | RCKeepMsg | RCForce | RCNoWrap,NULL);
					continue;
					}
				++count;
				}
			}
		} while((si.opflags & OpScript) && getsym() == Success && needsym(s_comma,false));

	// Return count if evaluating and no errors.
	if((si.opflags & OpEval) && disnull(&rc.msg))
		dsetint((long) count,rp);
	if(count == 1)
		(void) rcset(Success,0,"%s %s",name,text10);
						// "deleted"
	return rc.status;
	}

// Delete one or more alias(es).  Return status.
int deleteAlias(Datum *rp,int n,Datum **argpp) {

	return deleteAM(rp,PtrAlias,text269,text411,text271);
			// "Delete alias","Alias","No such alias '%s'"
	}

// Delete one or more macros.  Return status.
int deleteMacro(Datum *rp,int n,Datum **argpp) {

	return deleteAM(rp,PtrMacro,text216,text412,text116);
			// "Delete macro","Macro","No such macro '%s'"
	}

// Execute the contents of a buffer (of commands) and return result in rp.
int xeqBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;		// Pointer to buffer to execute.

	// Find out what buffer the user wants to execute...
	if(bcomplete(rp,text117,si.curbp->b_bname,OpDelete,&bufp,NULL) != Success || bufp == NULL)
			// "Execute"
		return rc.status;

	// and execute it with arg n.
#if MMDebug & Debug_Script
	return debug_execbuf(rp,n,bufp,bufp->b_fname,si.opflags & OpParens ? SRun_Parens : 0,"xeqBuf");
#else
	return execbuf(rp,n,bufp,bufp->b_fname,si.opflags & OpParens ? SRun_Parens : 0);
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
	short delim;

	if(bufp->b_fname != NULL) {
		name = bufp->b_fname;
		label = text99;
			// "file"
		delim = '"';
		}
	else {
		name = bufp->b_bname;
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
		 (rcode = dputf(&msg,"%s %s %c%s%c %s %ld",text229,label,delim,name,delim,text230,getlinenum(bufp,lnp))) == 0 &&
						// ", in","at line"
		 (rcode = dclose(&msg,sf_string)) == 0)
			(void) rcset(ScriptError,RCForce | RCNoFormat | RCTermAttr,msg.sf_datp->d_str);
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
		char *str = lnp->l_text;
		int len = skipwhite(&str,lnp->l_used);
		fprintf(logfile,"%.*s\n",len,str);
		}
	}
#endif

// Preprocess a buffer and return status.  If no errors found, set bufp->b_mip->mi_execp to result.  bufp->b_mip is assumed to
// not be NULL.
static int ppbuf(Buffer *bufp,uint flags) {
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
	lnp = bufp->b_lnp;
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
							// "Unmatched '~b%s~0' keyword"
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
							// "Unmatched '~b%s~0' keyword"
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
		} while((lnp = lnp->l_nextp) != NULL);

	// Buffer SCAN completed.  while/until/for/loop and "endloop" match?
	if(lbopen != NULL) {
		err = text121;
			// "Unmatched '~b%s~0' keyword"
		eline = text122;
			// "loop"
		goto FailExit;
		}

	// "macro" and "endmacro" match?
	if(saltlevel > 0) {
		err = text121;
			// "Unmatched '~b%s~0' keyword"
		eline = text336;
			// "macro"
FailExit:
		(void) macerror(err,eline,bufp,lnp,flags);
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
		fprintf(logfile,"-----\n Type: %.4X %s\n",lbtemp->lb_type,str);
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
		(void) funcarg(datp,ArgFirst | ArgNotNull1);
	return rc.status;
	}

// Parse first line of a macro definition and return status.  bflag is assumed to be either BFConstrain or zero.  Example
// definition:
//	macro grepFiles(2,)\
//	 {usage: 'pat,glob',desc: 'Find files which match pattern'}
//		...
//	endmacro
static int getmacro(Datum *rp,Buffer **mbufpp,ushort bflag) {
	UnivPtr univ;
	MacInfo *mip;
	bool whitespace,exists;
	Datum *datp1,*datp2;
	ushort n1 = 0,n2 = 0;
	short minArgs = 0,maxArgs = -1;

	if(dnewtrk(&datp1) != 0 || dnewtrk(&datp2) != 0)
		goto LibFail;

	// Get macro name.
	if(!havesym(s_ident,false) && !havesym(s_identq,true))
		return rc.status;
	if(strlen(last->p_tok.d_str) > MaxBufName - 1)
		return rcset(Failure,RCTermAttr,text232,last->p_tok.d_str,MaxBufName - 1);
			// "Macro name '~b%s~0' cannot exceed %d characters"

	// Construct the buffer header (and name) in rp temporarily.
	if(dsalloc(rp,strlen(last->p_tok.d_str) + 3) != 0)
		goto LibFail;
	rp->d_str[0] = TokC_ComLine;
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
			if(getsym() < NotFound)
				return rc.status;
			if(!needsym(s_rparen,false)) {
				if(funcarg(datp1,ArgFirst | ArgInt1) != Success)
					return rc.status;
				if((minArgs = datp1->u.d_int) < 0)
					goto Err;
				if(!havesym(s_comma,false))
					maxArgs = minArgs;
				else {
					if(getsym() < NotFound || (!havesym(s_rparen,false) &&
					 funcarg(datp2,ArgFirst | ArgInt1) != Success))
						return rc.status;
					if(datp2->d_type != dat_nil)
						if((maxArgs = datp2->u.d_int) < 0) {
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
	if(((exists = execfind(rp->d_str + 2,OpQuery,PtrAny,&univ)) && (!(univ.p_type & PtrMacro) ||
	 !(mi.cache[MdIdxClob]->ms_flags & MdEnabled))) || uvarfind(rp->d_str + 2) != NULL)
		return rcset(Failure,RCTermAttr,text165,rp->d_str + 2);
			// "Name '~b%s~0' already in use"

	// Create a hidden buffer, make sure it's empty, and has the correct pointer type in exectab.
	if(bfind(rp->d_str + 1,BS_Create | BS_Extend,bflag | BFHidden | BFMacro,mbufpp,NULL) != Success)
		return rc.status;
	if(exists) {
		if(bclear(*mbufpp,BC_IgnChgd | BC_Unnarrow | BC_ClrFilename) != Success)
			return rc.status;
		univ.p_type = bflag ? PtrMacro_C : PtrMacro_O;
		(void) execfind(rp->d_str + 2,OpUpdate,0,&univ);
		(*mbufpp)->b_flags = ((*mbufpp)->b_flags & ~BFConstrain) | bflag;
		}

	// Set the macro parameters.
	mip = (*mbufpp)->b_mip;
	mip->mi_minArgs = minArgs;
	mip->mi_maxArgs = maxArgs;
	if(n1)
		datxfer(n1 == 1 ? &mip->mi_usage : &mip->mi_desc,datp1);
	if(n2)
		datxfer(n2 == 1 ? &mip->mi_usage : &mip->mi_desc,datp2);
	rp->d_str[1] = ' ';

	return rc.status;
LibFail:
	return librcset(Failure);
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
		return librcset(Failure);
	return putvar(datp,&flip->vd);
	}

// Execute a compiled buffer, save result in rp, and return status.
static int xbuf(Datum *rp,Buffer *bufp,uint flags) {
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
	LevelInfo *elevp;		// Current level.
	LoopBlock *lbp;
	Buffer *mbufp = NULL;		// "macro" buffer pointer.
	Datum *datp;			// Work datum object.
	Datum *dlinep;			// Null-terminated buffer line (Datum object).
	char *eline;			// Work pointer.
	char *eline1;			// Beginning of expression statement (past any statement keyword).
	char *str;
	Parse *oldlast,newlast;
	ENode node;

	// Prepare for execution.
	if(dnewtrk(&datp) != 0 || dnewtrk(&dlinep) != 0)
		return librcset(Failure);
	if(nextlevel(&execlevel) != Success)
		return rc.status;
	elevp = execlevel;
	elevp->live = true;			// Set initial (top) execution level to true.
	elevp->loopspawn = false;
	breaklevel = 0;
	kentry.thisflag = kentry.lastflag;	// Let the first command inherit the flags from the last one.
	newlast.p_cl = NULL;

	// Now EXECUTE the buffer.
#if MMDebug & (Debug_Script | Debug_PPBuf)
	static uint instance = 0;
	uint myinstance = ++instance;
	fprintf(logfile,"*** BEGIN xbuf(%u,%s): %s...\n",myinstance,bufp->b_bname,showflags(&newlast));
#endif
	blnp = elnp = bufp->b_lnp;
	do {
		// Skip blank lines and comments unless "salting" a macro.
		eline = elnp->l_text;
		len = elnp->l_used;
#if MMDebug & (Debug_Script | Debug_PPBuf)
		fprintf(logfile,"%.8X [level %u.%u] >> %.*s\n",(uint) elnp,myinstance,elevp->level,len,eline);
#endif
		isWhiteLine = false;
		if((len = skipwhite(&eline,len)) == 0 || *eline == TokC_ComLine) {
			isWhiteLine = true;
			thisIsCL = false;
			if(!lastWasCL) {
				if(mbufp != NULL)
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
				if(elnp->l_nextp == NULL) {
					(void) rcset(Failure,0,text405,len,eline);
						// "Incomplete line \"%.*s\""
					goto RCExit;
					}
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
			if(dopenwith(&sf,dlinep,SFAppend) != 0 || dputmem((void *) eline,thisIsCL ? len - 1 : len,&sf) != 0 ||
			 dclose(&sf,sf_string) != 0) {
DErr:
				(void) librcset(Failure);
				goto RCExit;
				}
			}

		// Check for "endmacro" keyword now so that "salting" can be turned off if needed; otherwise, check for
		// statement keywords only if current line is not a continuation line.
		eline = eline1 = dlinep->d_str;
		if((kwid = kwfind(&eline1,strlen(eline1))) != s_nil) {
			if(thisIsCL) {
				if(kwid == kw_endmacro && elevp->live) {
					mbufp = NULL;
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
						if(!elevp->live)
							goto NextLn;

						// Parse line...
						if(getmacro(rp,&mbufp,constrained) != Success)
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

							// Turn off macro storing.
							mbufp = NULL;
							goto NextLn;
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
			if(eline == rp->d_str)
				dsetnil(rp);

			// Attach the line to the end of the buffer.
			llink(mp,mbufp,mbufp->b_lnp->l_prevp);
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
					goto NextLn;
				case kw_else:	// else keyword.

					// At top level, initiated by a loop statement, or duplicate else?
					if(elevp == execlevel || elevp->loopspawn || elevp->elseseen) {

						// Yep, bail out.
						str = "else";
						eline = text198;
							// "Misplaced '~b%s~0' keyword"
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
					goto NextLn;
				case kw_elsif:	// "elsif" keyword.

					// At top level, initiated by a loop statement, or "elsif" follows "else"?
					if(elevp == execlevel || elevp->loopspawn || elevp->elseseen) {

						// Yep, bail out.
						str = "elsif";
						eline = text198;
							// "Misplaced '~b%s~0' keyword"
						goto Misplaced;
						}

					// Check expression if prior level is true, current level is false, and this compound if
					// statement has not yet been true.
					if(elevp->prevp->live && !elevp->live && !elevp->ifwastrue)
						goto EvalIf;

					// Not evaluating so set current execution state to false.
					elevp->live = false;
					goto NextLn;
				case kw_endif:	// "endif" keyword.

					// At top level or was initiated by a loop statement?
					if(elevp == execlevel || elevp->loopspawn) {

						// Yep, bail out.
						str = "endif";
						eline = text198;
							// "Misplaced '~b%s~0' keyword"
						goto Misplaced;
						}

					// Extra arguments?
					if(extrasym())
						goto RCExit;

					// Done with current level: return to previous one.
					elevp = elevp->prevp;
					goto NextLn;

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
							goto NextLn;
							}
						}

					// Current level or condition is false: skip this block.
					goto JumpDown;

				case kw_break:	// "break" keyword.
				case kw_next:	// "next" keyword.

					// Ignore if current level is false (nothing to jump out of).
					if(elevp->live == false)
						goto NextLn;

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

							// If this is a "break" or "next", set the line pointer to the "endloop"
							// line so that the "endloop" is executed; otherwise, set it to the line
							// after the "endloop" line so that the "endloop" is bypassed.
							elnp = (kwid & SBreakType) ? lbp->lb_jump : lbp->lb_jump->l_nextp;

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
								// "Misplaced '~b%s~0' keyword"
Misplaced:
							(void) rcset(Failure,RCTermAttr,eline,str);
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
									elevp = elevp->prevp;
									if(prevlevel(&elevp) != Success)
										goto RCExit;
									}
								else {
									elnp = elnp->l_nextp;
									elevp = elevp->prevp;
									}
								elevp->loopcount = 0;	// Reset the loop counter.
								}
							else
								elnp = lbp->lb_mark;
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
		if(elevp->live) {
			(void) (newlast.p_cl == NULL ? execestmt(rp,eline1,TokC_ComLine,NULL) : xestmt(rp));

			// Check for exit or a command error.
			if(rc.status <= MinExit)
				return rc.status;

			if(forcecmd)		// Force Success status.
				rcclear(0);
			if(rc.status != Success) {
				EWindow *winp = si.wheadp;

				// Check if buffer is on-screen.
				do {
					if(winp->w_bufp == bufp) {

						// Found a window.  Set dot to error line.
						winp->w_face.wf_dot.lnp = blnp;
						winp->w_face.wf_dot.off = 0;
						winp->w_flags |= WFMove;
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
NextLn:
		elnp = elnp->l_nextp;
Onward:
		// On to the next line.  eline and len point to the original line.
		if(!(lastWasCL = thisIsCL) && newlast.p_cl != NULL) {
			parseend(oldlast);
			newlast.p_cl = NULL;
			}
		} while(elnp != NULL);		// End buffer execution.

	// "if" and "endif" match?
	if(elevp == execlevel)
		goto Exit;
	(void) rcset(Failure,RCTermAttr,text121,text199);
		// "Unmatched '~b%s~0' keyword","if"

	// Clean up and exit per rc.status.
RCExit:
	(void) macerror(NULL,NULL,bufp,blnp,flags);
Exit:
#if MMDebug & (Debug_Script | Debug_PPBuf)
	fprintf(logfile,"*** END xbuf(%u,%s), rc.msg \"%s\"...\n",myinstance,bufp->b_bname,rc.msg.d_str);
#endif
	llfree(execlevel);
	if(newlast.p_cl != NULL)
		parseend(oldlast);

	return (rc.status == Success) ? rc.status : rcset(ScriptError,0,NULL);
	}

// Execute the contents of a buffer, given Datum object for return value, numeric argument, buffer pointer, pathname of file
// being executed (or NULL), and invocation flags.
int execbuf(Datum *rp,int n,Buffer *bufp,char *runpath,uint flags) {

	// Check for narrowed buffer and runaway recursion.
	if(bufp->b_flags & BFNarrowed)
		(void) rcset(Failure,RCNoFormat,text72);
			// "Cannot execute a narrowed buffer"
	else if(maxmacdepth > 0 && bufp->b_mip != NULL && bufp->b_mip->mi_nexec >= maxmacdepth)
		(void) rcset(Failure,0,text319,text336,maxmacdepth);
			// "Maximum %s recursion depth (%d) exceeded","macro"
	else {
		Datum margs;

		// Create buffer-extension record if needed.
		if(bufp->b_mip == NULL && bextend(bufp) != Success)
			return rc.status;

		// Get macro arguments.
		dinit(&margs);		// No need to dclear() this later because it will always contain a dat_blobRef.
#if MMDebug & Debug_MacArg
		if(margload(n,bufp,&margs,flags) == Success) {
#else
		if(margload(bufp,&margs,flags) == Success) {
#endif
			// If evaluating, preprocess buffer if needed.
			if((bufp->b_flags & BFPreproc) || ppbuf(bufp,flags) == Success) {
				ScriptRun *oldrun,newrun;

				// Make new run instance and prepare for execution.
				oldrun = scriptrun;
				scriptrun = &newrun;
				scriptrun->margp = &margs;
				scriptrun->path = fixnull(runpath);
				scriptrun->bufp = bufp;
				if(dnew(&scriptrun->nargp) != 0)
					return librcset(Failure);		// Fatal error.
				dsetint(n,scriptrun->nargp);
				scriptrun->uvp = lvarsheadp;

				// Flag that we are executing the buffer and execute it.
				++bufp->b_mip->mi_nexec;
#if MMDebug & Debug_Script
				fprintf(logfile,"execbuf(): Calling xbuf(%s): %s...\n",bufp->b_bname,showflags(NULL));
#endif
				(void) xbuf(rp,bufp,flags);			// Execute the buffer.
#if MMDebug & Debug_Script
				fprintf(logfile,"execbuf(): xbuf() returned %s result and status %d \"%s\".\n",
				 dtype(rp,false),rc.status,rc.msg.d_str);
#endif
				(void) uvarclean(scriptrun->uvp);		// Clear any local vars that were created.

				// Clean up.
				--bufp->b_mip->mi_nexec;
#if DCaller
				ddelete(scriptrun->nargp,"execbuf");
#else
				ddelete(scriptrun->nargp);
#endif
				scriptrun = oldrun;
				}
			}
		}

	return rc.status;
	}

#if MMDebug & (Debug_Datum | Debug_Script | Debug_MacArg | Debug_PPBuf)
// Call execbuf() with debugging.
int debug_execbuf(Datum *rp,int n,Buffer *bufp,char *runpath,uint flags,char *caller) {

	fprintf(logfile,"*execbuf() called by %s() with args: n %d, b_bname %s, runpath '%s', flags %.8X, %s...\n",
	 caller,n,bufp->b_bname,runpath,flags,showflags(NULL));
	(void) execbuf(rp,n,bufp,runpath,flags);
	fprintf(logfile,"execbuf(%s) from %s() returned %s result and status %d \"%s\".\n",
	 bufp->b_bname,caller,dtype(rp,false),rc.status,rc.msg.d_str);
	return rc.status;
	}
#endif

// Yank a file into a buffer and execute it, given result pointer, filename, n argument, and macro invocation "at startup" flag
// (used for error reporting).  Suppress any extraneous messages during read.  If there are no errors, delete the buffer on
// exit.
int execfile(Datum *rp,char *fname,int n,uint flags) {
	Buffer *bufp;

	if(bfind(fname,BS_Create | BS_Force | BS_File,BFHidden,		// Create a hidden buffer...
	 &bufp,NULL) != Success ||
	 bmset(bufp,mi.cache[MdIdxRdOnly],true,NULL) != Success)	// mark the buffer as read-only...
		return rc.status;
	ushort mflag = mi.cache[MdIdxRtnMsg]->ms_flags & MdEnabled;	// save current message flag...
	gmclear(mi.cache[MdIdxRtnMsg]);					// clear it...
	(void) readin(bufp,fname,RWKeep | RWNoHooks | RWExist);		// read the file...
	if(mflag)
		gmset(mi.cache[MdIdxRtnMsg]);				// restore message flag...
	if(rc.status == Success) {
#if MMDebug & Debug_Script
		(void) debug_execbuf(rp,n,bufp,bufp->b_fname,flags,"execfile");
#else
		(void) execbuf(rp,n,bufp,bufp->b_fname,flags);		// and execute the buffer.
#endif
		}

	// If not displayed, remove the now unneeded buffer and return.
	bufp->b_flags &= ~BFHidden;
	if(rc.status >= Cancelled && bufp->b_nwind == 0)
		(void) bdelete(bufp,BC_IgnChgd);
	return rc.status;
	}

// Execute commands in a file and return result in rp.
int xeqFile(Datum *rp,int n,Datum **argpp) {
	char *path;

	// Get the filename.
	if(!(si.opflags & OpScript)) {
		TermInp ti = {NULL,RtnKey,MaxPathname,NULL};

		if(terminp(rp,text129,ArgNotNull1 | ArgNil1,Term_C_Filename,&ti) != Success || rp->d_type == dat_nil)
				// "Execute script file"
			return rc.status;
		}
	else if(funcarg(rp,ArgFirst | ArgNotNull1) != Success)
		return rc.status;

	// Look up the path...
	if(pathsearch(rp->d_str,0,(void *) &path,NULL) != Success)
		return rc.status;
	if(path == NULL)
		return rcset(Failure,0,text152,rp->d_str);	// Not found.
			// "No such file \"%s\""

	// save it...
	if(dsetstr(path,rp) != 0)
		return librcset(Failure);

	// and execute it with arg n.
	return execfile(rp,rp->d_str,n,0);
	}
