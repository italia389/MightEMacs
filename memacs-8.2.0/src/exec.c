// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// exec.c	Routines dealing with execution of commands, command lines, buffers, and command files for MightEMacs.

#include "os.h"
#include <stdarg.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"
#include "evar.h"

/*** Local declarations ***/

// Control data for if/loop execution levels.
typedef struct {
	bool live;		// True if executing this level.
	bool loopspawn;		// True if level spawned by a loop directive.
	int loopcount;		// Number of times through the loop.
	bool ifwastrue;		// True if (possibly compound) 'if' statement was ever true.
	bool elseseen;		// True if 'else' directive has been processed at this level.
	} LevelInfo;

#define DIRNAMEMAX	8		// Length of longest directive name.

// Search hook table for given name.  If found, set *hrpp to pointer; otherwise, return an error.
static int findhook(char *namep,HookRec **hrpp) {
	HookRec *hrp = hooktab;

	do {
		if(strcmp(namep,hrp->h_name) == 0) {
			*hrpp = hrp;
			return rc.status;
			}
		} while((++hrp)->h_name != NULL);

	return rcset(FAILURE,0,text317,namep);
			// "No such hook '%s'"
	}

// Set a hook.  Return status.
int setHook(Value *rp,int n) {
	HookRec *hrp;
	CFABPtr cfab;			// Pointer to the requested command or macro.
	Value *vp;

	// First get the hook to set.
	if(vnew(&vp,false) != 0)
		return vrcset();
	if(getarg(vp,text318,NULL,CTRL | 'M',0,ARG_FIRST | ARG_NOTNULL | ARG_STR) != SUCCESS ||
	 (!(opflags & OPSCRIPT) && vistfn(vp,VNIL)))
			// "Set hook "
		return rc.status;

	// Valid?
	if((opflags & (OPSCRIPT | OPEVAL)) != OPSCRIPT && findhook(vp->v_strp,&hrp) != SUCCESS)
		return rc.status;

	// Get the command or macro name.  If interactive mode, build "progress" prompt first.
	if(opflags & OPSCRIPT) {
		if(!getcomma(true))
			return rc.status;
		if(havesym(kw_nil,false)) {
			cfab.p_type = PTRNUL;
			cfab.u.p_voidp = NULL;
			(void) getsym();
			}
		else
			(void) getcfm(NULL,&cfab,PTRCMD | PTRMACRO);
		}
	else {
		char wkbuf[NWORK + 1];

		if(mlputs(MLFORCE,vp->v_strp) != SUCCESS)
			return rc.status;
		sprintf(wkbuf,"%s%s %s %s",text318,vp->v_strp,text339,text267);
			// "Assign hook ","to","command or macro"
		(void) getcfm(wkbuf,&cfab,PTRCMD | PTRMACRO);
		}

	// Unless script mode and not evaluating, set the hook.
	if(rc.status == SUCCESS && (opflags & (OPSCRIPT | OPEVAL)) != OPSCRIPT)
		hrp->h_cfab = cfab;

	return rc.status;
	}

// Build and pop up a buffer containing a list of all screens and their associated buffers.  Render buffer and return status.
int showHooks(Value *rp,int n) {
	Buffer *slistp;
	HookRec *hrp = hooktab;
	StrList rpt;

	// Get a buffer and open a string list.
	if(sysbuf(text316,&slistp) != SUCCESS)
			// "Hooks"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	// Construct the header lines.
	if(vputs(text315,&rpt) != 0 || vputc('\r',&rpt) != 0 ||
	    // "Hook Name    Set to"
	 vputs("----------   --------------------",&rpt) != 0)
		return vrcset();

	// For all hooks...
	do {
		if(vputf(&rpt,"\r%-13s%s",hrp->h_name,hrp->h_cfab.p_type == PTRNUL ? "" : hrp->h_cfab.p_type == PTRMACRO ?
		 hrp->h_cfab.u.p_bufp->b_bname + 1 : hrp->h_cfab.u.p_cfp->cf_name) != 0)
			return vrcset();

		// On to the next hook.
		} while((++hrp)->h_name != NULL);

	// Add the results to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(bappend(slistp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,slistp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}

// Disable a hook that went awry.  Return status.
static int dishook(HookRec *hrp) {
	StrList msg;
	int rcode = 0;
	CFABPtr *cfabp = &hrp->h_cfab;

	// Hook execution failed, let the user know what's up.
	if(rc.status > FATALERROR) {
		if((rcode = vopen(&msg,&rc.msg,visnull(&rc.msg) ? false : true)) == 0) {
			if(vempty(&msg)) {
				if(cfabp->p_type == PTRCMD)
					rcode = vputf(&msg,text175,cfabp->u.p_cfp->cf_name);
						 // "Command '%s' failed"
				else
					rcode = vputf(&msg,text176,cfabp->u.p_bufp->b_bname + 1);
						 // "Macro '%s' failed"
				}
#if MMDEBUG & DEBUG_VALUE
			if(rcode == 0) {
				fprintf(logfile,"dishook(): calling vputf(...,%s,%s)...\n",text161,hrp->h_name);
				rcode = vputf(&msg,text161,hrp->h_name);
				vdump(msg.sl_vp,"dishook(): post vputf()...");
				fflush(logfile);
				}
			if(rcode == 0) {
				rcode = vclose(&msg);
				fprintf(logfile,"dishook(): vclose() returned rc.msg '%s', status %d.\n",rc.msg.v_strp,rc.status);
				fflush(logfile);
				}
#else
			if(rcode == 0 && (rcode = vputf(&msg,text161,hrp->h_name)) == 0)
							// " (disabled '%s' hook)"
				rcode = vclose(&msg);
#endif
			}
		}

	// disable the hook...
	cfabp->p_type = PTRNUL;
	cfabp->u.p_voidp = NULL;

	// and set value object error if needed.
	return (rcode == 0) ? rc.status : vrcset();
	}

// Execute a command or buffer bound to a hook with rp (if not NULL) and n.  If arginfo == 0, invoke the command or macro
// directly; otherwise, build a command line with arguments.  In the latter case, arginfo contains the argument count and types
// of each argument (string or long).  The lowest four bits of arginfo is the argument count.  Each higher bit is 0 for a string
// argument and 1 for a long (integer) argument.
int exechook(Value *rp,int n,HookRec *hrp,uint arginfo,...) {
	bool fscall = false;		// feval() or doestmt() call?
	CFABPtr *cfabp = &hrp->h_cfab;

	// Unbound hook?
	if(cfabp->p_type == PTRNUL)
		return rc.status;

	// Don't allow edit command if in read-only mode.
	if(cfabp->p_type == PTRCMD && allowedit(cfabp->u.p_cfp->cf_flags & CFEDIT) != SUCCESS)
		goto fail;

	if(rp == NULL && vnew(&rp,false) != 0)		// For throw-away return value.
		goto vfail;

	// Build command line if requested.
	if(arginfo != 0) {
		va_list ap;
		StrList cmd;
		int delim = ' ';
		int argct = arginfo & 0xf;

		// Concatenate command or macro name with arguments...
		if(vopen(&cmd,NULL,false) != 0 || (n != INT_MIN && vputf(&cmd,"%d => ",n) != 0) ||
		 vputs(cfabp->p_type == PTRCMD ? cfabp->u.p_cfp->cf_name : cfabp->u.p_bufp->b_bname + 1,&cmd) != 0)
			goto vfail;
		va_start(ap,arginfo);
		arginfo >>= 4;
		do {
			if(vputc(delim,&cmd) != 0)
				goto vfail;
			if(arginfo & 1) {		// "long" argument?
				if(vputf(&cmd,"%ld",va_arg(ap,long)) != 0)
					goto vfail;
				}
			else if(quote(&cmd,va_arg(ap,char *),true) != SUCCESS)
				goto fail;
			arginfo >>= 1;
			delim = ',';
			} while(--argct > 0);
		va_end(ap);

		// and execute result.
		if(vclose(&cmd) != 0)
			goto vfail;
#if MMDEBUG & DEBUG_VALUE
		fprintf(logfile,"exechook(): calling doestmt(%s)...\n",cmd.sl_vp->v_strp);
		(void) doestmt(rp,cmd.sl_vp->v_strp,TKC_COMMENT,NULL);
		fprintf(logfile,"exechook(): doestmt() returned rc.msg '%s', status %d.\n",
		 rc.msg.v_strp,rc.status); fflush(logfile);
		if(rc.status != SUCCESS)
#else
		if(doestmt(rp,cmd.sl_vp->v_strp,TKC_COMMENT,NULL) != SUCCESS)
#endif
			goto fail;
		fscall = true;
		}
	else if(cfabp->p_type == PTRCMD) {
		CmdFunc *cfp = cfabp->u.p_cfp;
		if((rp != NULL && vnilmm(rp) != SUCCESS) ||
		 ((n != 0 || !(cfp->cf_flags & CFNCOUNT)) &&
		 (cfp->cf_func == NULL ? (feval(rp,n,cfp),fscall = true) : cfp->cf_func(rp,n)) != SUCCESS))
			goto fail;
		}
	else if(dobuf(rp,n,cfabp->u.p_bufp,NULL,0) != SUCCESS)
		goto fail;

	// Successful execution.  Save return message and check for false return.
	if((!fscall && (opflags & OPSCRIPT) && rcsave() != SUCCESS) || !vistfn(rp,VFALSE))
		return rc.status;

	// Hook returned false ... big trouble.
	(void) rcset(FAILURE,RCKEEPMSG,text300);
			// "False return"

	// Execution failed, let the user know what's up.
fail:
	return dishook(hrp);
vfail:
	return vrcset();
	}

// Execute a named command, alias, or macro interactively even if it is not bound or is being invoked from a macro.
int run(Value *rp,int n) {
	uint oldscript;
	CFABPtr cfab;		// Pointer to command, alias, or macro to execute.

	// If we are in script mode, force the command interactively.
	if((oldscript = opflags & OPSCRIPT)) {

		// Grab next symbol...
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;

		// look it up...
		if(opflags & OPEVAL) {
			if(cfabsearch(last->p_tok.v_strp,&cfab,PTRCMD | PTRALIAS | PTRMACRO))
				return rcset(FAILURE,0,text244,last->p_tok.v_strp);
					// "No such command, alias, or macro '%s'"
			if(cfab.p_type == PTRALIAS_F)
				return rcset(FAILURE,0,text314,last->p_tok.v_strp);
					// "Cannot execute alias '%s' interactively"
			}

		// and get next symbol.
		if(getsym() < NOTFOUND)
			return rc.status;

		// If not evaluating, bail out here.
		if(!(opflags & OPEVAL))
			return rc.status;

		// Otherwise, prepare to execute the CAM -- INTERACTIVELY.
		opflags &= ~OPSCRIPT;
		}

	// Prompt the user to type a named command, alias, or macro.
	else if(getcfam(": ",PTRCMD | PTRALIAS_C | PTRALIAS_M | PTRMACRO,&cfab,text244,text314) != SUCCESS ||
			// "No such command, alias, or macro '%s'","Cannot execute alias '%s' interactively"
	 cfab.p_type == PTRNUL)
		return rc.status;

	// Execute it.
	bool fevalcall = false;
	if(cfab.p_type & PTRALIAS)
		cfab = cfab.u.p_aliasp->a_cfab;
	if(cfab.p_type == PTRMACRO)
		(void) dobuf(rp,n,cfab.u.p_bufp,NULL,opflags & OPPARENS ? SRUN_PARENS : 0);	// Call the macro.
	else {											// Call the command function.
		CmdFunc *cfp = cfab.u.p_cfp;
		if(n != 0 || !(cfp->cf_flags & CFNCOUNT))
			if(allowedit(cfp->cf_flags & CFEDIT) == SUCCESS)
				(void) (cfp->cf_func == NULL ? (feval(rp,n,cfp),fevalcall = true) : cfp->cf_func(rp,n));
		}
	if(rc.status == SUCCESS && oldscript && !fevalcall)
		(void) rcsave();

	// Clean up and return result.
	opflags = (opflags & ~OPSCRIPT) | oldscript;
	return rc.status;
	}

// Concatenate all arguments and execute string result.  If interactive and non-default n, evaluate input string before
// executing it.  Return status.
int eval(Value *rp,int n) {
	Value *vp;

	// Get the command line.
	if(vnew(&vp,false) != 0)
		return vrcset();
	if(opflags & OPSCRIPT) {

		// Concatenate all arguments into vp.
		if(join(vp,NULL,1,JNKEEPALL) != SUCCESS)
			return rc.status;
		}
	else if(terminp(vp,": ",NULL,CTRL | 'M',0,n == INT_MIN ? ARG_NOTNULL : ARG_NOTNULL | TERM_EVAL) != SUCCESS ||
	 vistfn(vp,VNIL))
		return rc.status;

	// Execute command line.
	return doestmt(rp,vp->v_strp,TKC_COMMENT,NULL);
	}

// Free macro argument list.  If margheadpp is not NULL, release just the MacArg list it points to.
static void margfree(MacArgList *malp,MacArg **margheadpp) {
	MacArg *margp1,*margp2;

	// Free all MacArg heap space.
	for(margp1 = (margheadpp != NULL) ? *margheadpp : malp->mal_headp; margp1 != NULL; margp1 = margp2) {
		margp2 = margp1->ma_nextp;
#if VDebug
		vdelete(margp1->ma_valp,"margfree");
#else
		vdelete(margp1->ma_valp);
#endif
		free((void *) margp1);
		}

	// Free all MacArgList heap space if requested.
	if(margheadpp == NULL) {
#if MMDEBUG & DEBUG_MARG
		fprintf(logfile,"Freeing list %.8x (rc.status is %d",(uint) malp,rc.status);
		if(rc.status != SUCCESS)
			fprintf(logfile," [%s]",rc.msg.v_strp);
		fputs(")...\n",logfile);
		fflush(logfile);
#endif
		free((void *) malp);
		}
	}

#if MMDEBUG & DEBUG_MARG
// Verify that a valid macro argument list exists.
static void margcheck(int n,Buffer *bufp,MacArgList *malp) {
	int listnum = 1;
	MacArg *margp;

	fprintf(logfile,"-----\nmargcheck(): n %d, buf '%s'\n",n,bufp->b_bname);
	fprintf(logfile,
 "Macro argument list %.8x (%2d):\n\tmal_count: %hu\n\tmal_headp: %.8x\n\tmal_argp: %.8x\n",
	 (uint) malp,--listnum,malp->mal_count,(uint) malp->mal_headp,(uint) malp->mal_argp);
	for(margp = malp->mal_headp; margp; margp = margp->ma_nextp) {
		fprintf(logfile,"\t*Arg %2hu:\n\t\tma_nextp: %.8x\n\t\tma_valp (",margp->ma_num,(uint) margp->ma_nextp;
		if(margp->ma_valp->v_type == VALINT)
			fprintf(logfile,"int): %ld\n",margp->ma_valp->u.v_int);
		else
			fprintf(logfile,"string): '%s'\n",margp->ma_valp->v_strp);
		}
	fflush(logfile);
	}
#endif

// Create macro argument list on heap and save head pointer in *malpp.  If in script mode, evaluate some or all of remaining
// arguments (if any) on command line, dependent on number specified in Buffer record; otherwise, create empty list.  Return
// status.
#if MMDEBUG & DEBUG_MARG
static int margalloc(int n,Buffer *bufp,MacArgList **malpp,uint flags) {
#else
static int margalloc(Buffer *bufp,MacArgList **malpp,uint flags) {
#endif
	ushort count;
	MacArg *margp,*margheadp,*margtailp;
	static char myname[] = "margalloc";

#if MMDEBUG & DEBUG_MARG
	fprintf(logfile,"*margalloc(%d,%s,%.4x)...\n",n,bufp->b_bname,flags);
	fflush(logfile);
#endif
	margheadp = NULL;
	count = 0;
	if(opflags & OPSCRIPT) {
		Value *argp;
		int argct = bufp->b_nargs;
		int reqct = (argct >= 0) ? argct : 0;
		uint aflags = ARG_FIRST;

		// Get arguments until none left.  Error if b_nargs > 0 and count != b_nargs.
		if(vnew(&argp,false) != 0)
			return vrcset();

		// If xxx() call form, have ')' and (1), not evaluating; or (2), argct < 0; bail out (not an error).
		if(argct != 0 && (!(flags & SRUN_PARENS) || !havesym(s_rparen,false) || ((opflags & OPEVAL) && argct > 0))) {
			for(;;) {
				// Get next argument.
				if(aflags == ARG_FIRST) {
					if(!havesym(s_any,reqct > 0)) {
						if(rc.status != SUCCESS)
							goto eexit;		// Error.
						break;				// No arguments left.
						}
					}
				else if(!havesym(s_comma,false))
					break;					// No arguments left.

				// We have a symbol.  Get expression.
				if(funcarg(argp,aflags) != SUCCESS) {
eexit:
					margfree(NULL,&margheadp);
					return rc.status;
					}
				aflags = 0;
				--reqct;

				// Got an argument.  If not in "consume" mode, allocate a MacArg record and set its values.
				if(opflags & OPEVAL) {

					if((margp = (MacArg *) malloc(sizeof(MacArg))) == NULL)
						return rcset(PANIC,0,text94,myname);
								// "%s(): Out of memory!"
					margp->ma_nextp = NULL;
					margp->ma_num = ++count;
					margp->ma_flags = 0;
					if(vnew(&margp->ma_valp,true) != 0)
						return vrcset();
					vxfer(margp->ma_valp,argp);

					// Add argument to linked list.
					if(margheadp == NULL)
						margheadp = margtailp = margp;
					else
						margtailp = margtailp->ma_nextp = margp;
					}
				}
			if(reqct > 0 || (reqct < 0 && argct > 0))
				return rcset(FAILURE,0,text69,last->p_tok.v_strp);
					// "Wrong number of arguments (at token '%s')"
			}
		}

	// Create MacArgList record and return it.
	if((*malpp = (MacArgList *) malloc(sizeof(MacArgList))) == NULL)
		return rcset(PANIC,0,text94,myname);
				// "%s(): Out of memory!"
	(*malpp)->mal_headp = (*malpp)->mal_argp = margheadp;
	(*malpp)->mal_count = count;

#if MMDEBUG & DEBUG_MARG
	margcheck(n,bufp,malp);
#endif
	return rc.status;
	}

// Parse and execute a string as an expression statement, given return value object (which is assumed to be a null string),
// string pointer, terminator character, and optional pointer in which to return updated string pointer.  This function is
// called anytime a string needs to be executed as a statement (without a directive); for example, by dobuf(), xeqCmdLine(),
// "#{...}", or the -e switch at startup.  lastflag and thisflag are also updated.
int doestmt(Value *rp,char *clp,int termch,char **clpp) {
	uint oldscript;
	Parse *oldlast,newlast;
	ENode node;

#if MMDEBUG & DEBUG_VALUE
	fprintf(logfile,"doestmt(%.8x,%s,%.4x,%.8x)...\n",(uint) rp,clp,termch,(uint) clpp); fflush(logfile);
#endif
	// Begin new command line parsing "instance".
	oldlast = last;
	if(parsebegin(&newlast,clp,termch) < NOTFOUND)
		return rc.status;

	// Save old macro-mode flag and enable it.
	oldscript = opflags & OPSCRIPT;
	opflags = (opflags & ~OPPARENS) | OPSCRIPT;

	// Set up the default command values.
	kentry.lastflag = kentry.thisflag;
	kentry.thisflag = 0;

	// Evaluate the line (as an expression).
	nodeinit(&node,rp);
#if MMDEBUG & DEBUG_EXPR
	fprintf(logfile,"doestmt(): calling ge_comma() on token '%s', line '%s'...\n",last->p_tok.v_strp,last->p_clp);
#endif
	if(ge_comma(&node) == SUCCESS)
		if(!extrasym()) {			// Error if extra symbol(s).
			if(termch == TKC_EXPREND &&	// Check if parsing "#{}" and right brace missing.
			 *last->p_clp != TKC_EXPREND)
				(void) rcset(FAILURE,0,text173,TKC_EXPR,TKC_EXPRBEG,TKC_EXPREND);
						// "Unbalanced %c%c%c string parameter"
			}

#if MMDEBUG & DEBUG_EXPR
	fprintf(logfile,"doestmt(): Returning status %hd\n",rc.status);
	fflush(logfile);
#endif
	// Restore settings and return.
	if(clpp != NULL)
		*clpp = last->p_clp;
	parseend(oldlast);
	opflags = (opflags & ~OPSCRIPT) | oldscript;

	return rc.status;
	}

// Delete one or more macros.  Return status.
int deleteMacro(Value *rp,int n) {

	return deleteAM(text216,PTRMACRO,text116);
		// "Delete macro","No such macro '%s'"
	}

// Execute the contents of a buffer (of commands) and return result in rp.
int xeqBuf(Value *rp,int n) {
	Buffer *bufp;		// Pointer to buffer to execute.

	// Find out what buffer the user wants to execute.
	if(bcomplete(rp,text117,curbp->b_bname,OPDELETE,&bufp,NULL) != SUCCESS || bufp == NULL)
			// "Execute"
		return rc.status;
	if(!(opflags & OPSCRIPT))
		mlerase(0);

	// And now execute it with arg n.
#if (MMDEBUG & DEBUG_EXPR) && 0
	(void) dobuf(rp,n,bufp,bufp->b_fname,opflags & OPPARENS ? SRUN_PARENS : 0);
	if(rp->v_type == VALINT) {
		char wkbuf[LONGWIDTH];

		long_asc(rp->u.v_int,wkbuf);
		(void) mlputs(0,wkbuf);
		}
	else
		(void) mlputs(0,rp->v_strp);
	return rc.status;
#else
	return dobuf(rp,n,bufp,bufp->b_fname,opflags & OPPARENS ? SRUN_PARENS : 0);
#endif
	}

// Skip white space in a fixed-length string, given pointer to source pointer and length.  Update pointer to point to the first
// non-white character (or end of string if none) and return number of bytes remaining.
static int skipwhite(char **strpp,int len) {
	char *strp = *strpp;

	while(len > 0 && (*strp == ' ' || *strp == '\t')) {
		++strp;
		--len;
		}
	*strpp = strp;
	return len;
	}

// Look up a directive, given indirect pointer to beginning of command line and length.  Update *clpp and return symbol id if
// found; otherwise, s_nil.
static enum e_sym dfind(char **clpp,int len) {
	char *clp = *clpp;

	if(is_lower(*clp)) {
		enum e_sym s;
		ushort dlen;
		char wkbuf[DIRNAMEMAX + 2],*strp = wkbuf;

		// Copy enough of line into wkbuf (null terminated) to do keyword comparisons.
		stplcpy(wkbuf,clp,(uint) len < (sizeof(wkbuf) - 1) ? len + 1 : sizeof(wkbuf));

		// Check of line begins with a directive keyword.
		if((s = getident(&strp,&dlen)) >= kw_break) {
			*clpp = clp + dlen;
			return s;
			}
		}

	return s_nil;
	}

// Build a macro execution error message which includes the buffer name and line currently being executed, and set via rcset().
// If emsg0 is NULL, use rc.msg as the initial portion; if rc.msg is null, use a generic error message.  Return status.
static int macerror(char *emsg0,char *emsg1,Buffer *bufp,Line *lnp,uint flags) {
	int rcode;
	StrList msg;
	char *name,*label;

	if((flags & SRUN_STARTUP) && bufp->b_fname != NULL) {
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
	if((rcode = vopen(&msg,NULL,false)) == 0) {

		// Prepend emsg0.
		if(emsg0 == NULL)
			emsg0 = visnull(&rc.msg) ? text219 : rc.msg.v_strp;
						// "Script failed"
		if((rcode = vputf(&msg,emsg0,emsg1)) == 0 &&
		 (rcode = vputf(&msg,"%s %s '%s' %s %ld",text229,label,name,text230,getlinenum(bufp,lnp))) == 0 &&
						// ", in","at line"
		 (rcode = vclose(&msg)) == 0)
			(void) rcset(SCRIPTERROR,RCFORCE,"%s",msg.sl_vp->v_strp);
		}

	return (rcode == 0) ? rc.status : vrcset();
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

	if(bufp->b_execp != NULL) {
		lbfree(bufp->b_execp);
		bufp->b_execp = NULL;
		}
	bufp->b_flags &= ~BFPREPROC;		// Clear "preprocessed" flag.
	}

#if MMDEBUG & DEBUG_PPBUF
static void lbdump(Line *lnp,char *label) {

	fprintf(logfile,"%s: [%.8x] ",label,(uint) lnp);
	if(lnp == NULL)
		fputs("NULL\n",logfile);
	else {
		char *strp = ltext(lnp);
		int len = skipwhite(&strp,lused(lnp));
		fprintf(logfile,"%.*s\n",len,strp);
		}
	}
#endif

// Preprocess a buffer and return status.  If no errors found, set bufp->b_execp to result.
static int ppbuf(Buffer *bufp,uint flags) {
	Line *lnp;			// Pointer to line to execute.
	Line *hlp;			// Pointer to line header.
	enum e_sym dirnum;		// Directive index.
	int len;			// Line length.
	int saltlevel;			// 'macro' nesting depth.
	LoopBlock *lbexec;		// Pointer to execution loop-block list.
	LoopBlock *lbopen;		// Pointer during scan.
	LoopBlock *lbtemp;		// Temporary pointer.
	char *eline,*eline1;		// Text of line to scan.

	// Scan the buffer to execute, building loop blocks.
	lbexec = lbopen = NULL;
	saltlevel = 0;
	hlp = bufp->b_hdrlnp;
	for(lnp = lforw(hlp); lnp != hlp; lnp = lforw(lnp)) {

		// Scan the current line.
		eline = ltext(lnp);
		len = lused(lnp);
#if MMDEBUG & DEBUG_PPBUF
		fprintf(logfile,"%.8x >> %.*s\n",(uint) lnp,len,eline);
#endif
		// Find first non-whitespace character.  If line is blank, skip it.
		if((len = skipwhite(&eline,len)) == 0)
			continue;

		// Check for a directive.
		eline1 = eline;
		if((dirnum = dfind(&eline1,len)) != s_nil) {
			switch(dirnum) {
				case kw_macro:
					++saltlevel;
					break;
				case kw_endmacro:
					// endmacro directive: orphan?
					if(--saltlevel < 0) {
						eline = text121;
							// "Unmatched '%s' directive"
						eline1 = text197;
							// "endmacro"
						goto failexit;
						}
					break;
				case kw_return:
					break;
				case kw_loop:
				case kw_while:
				case kw_until:
					// while, until, or loop directive: make a block.
					goto newblock;
				case kw_break:
				case kw_next:
					// 'break' or 'next' directive: orphan?
					if(lbopen == NULL) {
						eline = text120;
							// "'break' or 'next' outside of any loop block"
						eline1 = NULL;
						goto failexit;
						}
newblock:
					// Not an orphan, make a block.
					if((lbtemp = (LoopBlock *) malloc(sizeof(LoopBlock))) == NULL)
						return rcset(PANIC,0,text94,"ppbuf");
							// "%s(): Out of memory!"

					// Record line and block type.
					lbtemp->lb_mark = lnp;
					lbtemp->lb_break = lbtemp->lb_jump = NULL;
					lbtemp->lb_type = dirnum;

					// Add the block to the open list.
					lbtemp->lb_next = lbopen;
					lbopen = lbtemp;
					break;
				case kw_endloop:
					// 'endloop' directive: orphan?
					if(lbopen == NULL) {
						eline = text121;
							// "Unmatched '%s' directive"
						eline1 = text196;
							// "endloop"
						goto failexit;
						}

					// Loop through records (including any BREAK or NEXT records) in the lbopen stack,
					// record this line in them, and move them to the top of the lbexec list until one loop
					// (DLOOPTYPE) record is moved.  This will complete the most recent DLOOPTYPE block.
					do {
						lbopen->lb_jump = lnp;
						if(lbopen->lb_type & DLOOPTYPE) {
							lbtemp = lbopen->lb_next;

							// For loop records, record the marker line (temporarily) of the
							// *parent* loop record in this record's lb_break member so that it can
							// be set later to the parent's ENDLOOP line (which is needed for
							// multi-level breaks).  If there is no parent loop block, the lb_break
							// member will remain NULL, which is okay.
							while(lbtemp != NULL) {
								if(lbtemp->lb_type & DLOOPTYPE) {

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
						} while(!(lbexec->lb_type & DLOOPTYPE));
					break;
				default:
					;	// Do nothing.
				}
			}

		// On to the next line.
		}

	// Buffer SCAN completed.  while/until/loop and 'endloop' match?
	if(lbopen != NULL) {
		eline = text121;
			// "Unmatched '%s' directive"
		eline1 = text122;
			// "loop"
		goto failexit;
		}

	// 'macro' and 'endmacro' match?
	if(saltlevel > 0) {
		eline = text121;
			// "Unmatched '%s' directive"
		eline1 = text336;
			// "macro"
failexit:
		(void) macerror(eline,eline1,bufp,lnp,flags);
		lbfree(lbopen);
		goto elexit;
		}

	// Everything looks good.  Last step is to fix up the loop records.
	for(lbtemp = lbexec; lbtemp != NULL; lbtemp = lbtemp->lb_next) {
		if((lbtemp->lb_type & DLOOPTYPE) && lbtemp->lb_break != NULL) {
			LoopBlock *lbtemp2 = lbexec;

			// Find loop block ahead of this block that contains this block's marker.
			do {
				if(lbtemp2->lb_mark == lbtemp->lb_break) {

					// Found parent block.  Set this loop's lb_break to parent's
					// ENDLOOP line (instead of the top marker line).
					lbtemp->lb_break = lbtemp2->lb_jump;
					goto nextfix;
					}
				} while((lbtemp2 = lbtemp2->lb_next) != NULL && lbtemp2->lb_mark != lbtemp->lb_mark);

			// Huh?  Matching loop block not found! (This is a bug.)
			(void) rcset(FAILURE,0,text220,getlinenum(bufp,lbtemp->lb_mark));
				// "Parent loop block of loop directive at line %ld not found during buffer scan"
			goto rcexit;
			}
nextfix:;
		}

#if MMDEBUG & DEBUG_PPBUF
	// Dump blocks to log file.
	fputs("\nLOOP BLOCKS\n",logfile);
	for(lbtemp = lbexec; lbtemp != NULL; lbtemp = lbtemp->lb_next) {
		char *strp;
		switch(lbtemp->lb_type) {
			case kw_while:
				strp = "while";
				break;
			case kw_until:
				strp = "until";
				break;
			case kw_loop:
				strp = "loop";
				break;
			case kw_break:
				strp = "break";
				break;
			case kw_next:
				strp = "next";
				break;
			default:
				strp = "UNKNOWN";
				break;
			}
		fprintf(logfile,"-----\n Type: %.4x %s\n",lbtemp->lb_type,strp);
		lbdump(lbtemp->lb_mark," Mark");
		lbdump(lbtemp->lb_jump," Jump");
		lbdump(lbtemp->lb_break,"Break");
		}
//	return rcset(FATALERROR,0,"ppbuf() exit");
#endif
	// Success!  Save block pointer in buffer record and set "preprocessed" flag.
	bufp->b_execp = lbexec;
	bufp->b_flags |= BFPREPROC;
	goto elexit;

	// Clean up and exit per rc.status.
rcexit:
	(void) macerror(NULL,NULL,bufp,lnp,flags);
elexit:
	if(rc.status == SUCCESS)
		return rc.status;
	lbfree(lbexec);
	return rc.status;
	}

// Return to the most recent loop level (bypassing 'if' levels).
static int prevlevel(LevelInfo *elevp0,LevelInfo **elevpp) {
	LevelInfo *elevp = *elevpp;

	while(elevp > elevp0 && !elevp->loopspawn)
		--elevp;
	if(elevp == elevp0) {

		// Huh?  Loop level not found! (This is a bug.)
		return rcset(FAILURE,0,text114);
			// "Prior loop execution level not found while rewinding stack" 
		}
	*elevpp = elevp;
	return rc.status;
	}

// Execute a compiled buffer, save result in rp, and return status.
static int execbuf(Value *rp,Buffer *bufp,uint flags) {
	Line *lnp;			// Pointer to line to execute.
	Line *hlp;			// Pointer to line header.
	int forcecmd;			// 'force' directive?
	enum e_sym dirnum;		// Directive index.
	int len;			// Line length.
	bool go;			// Used to evaluate 'while' and 'until' truth.
	int breaklevel;			// Number of levels to 'break'.
	int saltlevel;			// 'macro' nesting depth.
	LevelInfo execlevel[IFNESTMAX],	// Level table...
	 *elevp,*elevpz;		// and pointers to current and max.
	LoopBlock *lbp;
	Buffer *mbufp = NULL;		// 'macro' buffer pointer.
	Value *vp;			// Work value object.
	Value *vlinep;			// Null-terminated buffer line (value object).
	char *eline;			// Work pointer.
	char *eline1;			// Beginning of expression statement (past any directive).
	char *strp;
	Parse *oldlast,newlast;
	ENode node;

	// Prepare for execution.
	if(vnew(&vp,false) != 0 || vnew(&vlinep,false) != 0)
		return vrcset();
	saltlevel = 0;				// 'macro' not being "salted away".
	elevpz = (elevp = execlevel) + IFNESTMAX;
	elevp->live = true;			// Set initial (top) execution level to true.
	elevp->loopspawn = elevp->ifwastrue = elevp->elseseen = false;
	elevp->loopcount = breaklevel = 0;
	kentry.thisflag = kentry.lastflag;	// Let the first command inherit the flags from the last one.
	oldlast = last;
	newlast.p_clp = NULL;

	// Now EXECUTE the buffer.
#if MMDEBUG & (DEBUG_SCRIPT | DEBUG_PPBUF)
	static uint instance = 0;
	uint myinstance = ++instance;
	fprintf(logfile,"*** BEGIN execbuf(%u)...\n",myinstance);
#endif
	lnp = lforw(hlp = bufp->b_hdrlnp);
	while(lnp != hlp) {

		// Skip blank lines and comments (unless "salting").
		eline = ltext(lnp);
		len = lused(lnp);
#if MMDEBUG & (DEBUG_SCRIPT | DEBUG_PPBUF)
		fprintf(logfile,"%.8x [level %u.%s%u] >> %.*s\n",(uint) lnp,myinstance,elevp < execlevel ? "-" : "",(uint)
		 (elevp < execlevel ? execlevel - elevp : elevp - execlevel),len,eline); //fflush(logfile);
#endif
		if((len = skipwhite(&eline,len)) == 0 || *eline == TKC_COMMENT) {
			if(mbufp != NULL)
				goto salt0;
			goto onward;
			}

		// Allocate eline as a value object, null terminated, so that it can be parsed properly by getsym().
		if(vsetfstr(eline,len,vlinep) != 0) {
			(void) vrcset();
			goto rcexit;
			}
		eline = eline1 = vlinep->v_strp;

		// Parse 'macro' and 'endmacro' directives here...
		dirnum = s_nil;
		if((dirnum = dfind(&eline1,len)) != s_nil) {

			// Found directive ... process it.
			if(parsebegin(&newlast,eline1,TKC_COMMENT) < NOTFOUND)
				goto rcexit;
			switch(dirnum) {
				case kw_macro:

					// Skip if current level is false.
					if(!elevp->live)
						goto onward;

					// Create macro?
					if(++saltlevel != 1)

						// No, this is a nested macro.  Salt it away with the other lines.
						break;

					// Yes, get name and check if valid.
					if(!havesym(s_ident,false) && !havesym(s_identq,true))
						goto rcexit;
					if(strlen(last->p_tok.v_strp) > NBNAME - 1) {
						(void) rcset(FAILURE,0,text232,last->p_tok.v_strp,NBNAME - 1);
							// "Macro name '%s' cannot exceed %d characters"
						goto rcexit;
						}

					{CFABPtr cfab;
#if 0
					bool cleared;
#endif
					short argct = -1;

					// Construct the buffer header (and name) in rp temporarily.
					if(vsalloc(rp,strlen(last->p_tok.v_strp) + 3) != 0) {
						(void) vrcset();
						goto rcexit;
						}
					rp->v_strp[0] = TKC_COMMENT;
					rp->v_strp[1] = SBMACRO;
					strcpy(rp->v_strp + 2,last->p_tok.v_strp);

					// Get argument count.
					if(getsym() != NOTFOUND) {
						if(rc.status != SUCCESS)
							goto rcexit;
						if(funcarg(vp,ARG_INT) != SUCCESS)
							goto rcexit;
						argct = vp->u.v_int;

						// Extra arguments?
						if(extrasym())
							goto rcexit;
						}

					// Existing command, function, alias, macro, or user variable of same name?
					if((!cfabsearch(rp->v_strp + 2,&cfab,PTRCFAM) && (cfab.p_type != PTRMACRO ||
					 !(modetab[MDR_GLOBAL].flags & MDCLOB))) || uvarfind(rp->v_strp + 2) != NULL) {
						(void) rcset(FAILURE,0,text165,rp->v_strp + 2);
							// "Name '%s' already in use"
						goto rcexit;
						}

					// Create a hidden buffer and make sure it's empty.
					if(bfind(rp->v_strp + 1,CRBCREATE,BFHIDDEN | BFMACRO,&mbufp,NULL) != SUCCESS ||
#if 1
					 bclear(mbufp,CLBIGNCHGD | CLBUNNARROW | CLBCLFNAME,NULL) != SUCCESS)
						goto rcexit;
#else
					 bclear(mbufp,CLBIGNCHGD | CLBUNNARROW | CLBCLFNAME,&cleared) != SUCCESS)
						goto rcexit;

		// Not possible for bclear() to return SUCCESS and not clear the buffer ... me think.  So this is not needed.
					if(!cleared) {
						(void) rcset(FAILURE,0,text113,rp->v_strp + 2);
								// "Cannot create macro '%s'"
						goto rcexit;
						}
#endif
					// Set the argument count...
					mbufp->b_nargs = argct;
					}

					// and store it's name as a comment at the top of the new buffer.
					rp->v_strp[1] = ' ';
					len = strlen(eline = rp->v_strp);
					goto salt1;
				case kw_endmacro:

					// Extra arguments?
					if(extrasym())
						goto rcexit;

					// Check execution level.
					if(elevp->live) {

						// Turn off storing if prior salt level was zero.
						if(--saltlevel == 0) {
							mbufp = NULL;
							goto onward;
							}

						// 'endmacro' out of place?
						if(saltlevel < 0) {
							strp = "endmacro";
							eline = text198;
								// "Misplaced '%s' directive"
							goto misplaced;
							}
						}
					break;
				default:
					;	// Do nothing.
				}
			} // End directive parsing.

		// If macro store is on, just salt this (entire) line away.
		if(mbufp != NULL) {
			Line *mp;
salt0:
			// Save original line (as a debugging aid) but skip any leading tab.
			eline = ltext(lnp);
			if((len = lused(lnp)) > 0 && *eline == '\t') {
				++eline;
				--len;
				}
salt1:
			// Allocate the space for the line.
			if(lalloc(len,&mp) != SUCCESS)
				return rc.status;

			// Copy the text into the new line.
			memcpy(mp->l_text,eline,len);
			if(eline == rp->v_strp && vnilmm(rp) != SUCCESS)
				return rc.status;

			// Attach the line to the end of the buffer.
			mbufp->b_hdrlnp->l_prevp->l_nextp = mp;
			mp->l_prevp = mbufp->b_hdrlnp->l_prevp;
			mbufp->b_hdrlnp->l_prevp = mp;
			mp->l_nextp = mbufp->b_hdrlnp;
			goto onward;
			}

		// Not "salting" and not a 'macro' or 'endmacro' directive.  Check others.
		forcecmd = false;
		if(dirnum != s_nil) {
			switch(dirnum) {

				// Directives in the if/else family.
				case kw_if:	// 'if' directive.

					// Begin new level.
					if(++elevp == elevpz) {
toodeep:
						(void) rcset(FAILURE,0,text168,IFNESTMAX + 1);
							// "if/loop nesting level (%d) too deep"
						goto rcexit;
						}
					elevp->loopspawn = elevp->ifwastrue = elevp->elseseen = false;
					elevp->loopcount = 0;

					// Prior level true?
					if(elevp[-1].live) {
evalif:
						// Yes, grab the value of the logical expression.
						nodeinit(&node,vp);
						if(ge_comma(&node) != SUCCESS)
							goto rcexit;

						// Set this level to the result of the expression and update "ifwastrue" if the
						// result is true.
						if((elevp->live = vistrue(vp)))
							elevp->ifwastrue = true;
						}
					else
						// Prior level was false so this one is, too.
						elevp->live = false;
					goto onward;
				case kw_else:	// else directive.

					// At top level, initiated by a loop directive, or duplicate else?
					if(elevp == execlevel || elevp->loopspawn || elevp->elseseen) {

						// Yep, bail out.
						strp = "else";
						eline = text198;
							// "Misplaced '%s' directive"
						goto misplaced;
						}

					// Extra arguments?
					if(extrasym())
						goto rcexit;

					// Update current execution state.  Can be true only if (1), prior state is true; (2),
					// current state is false; and (3), this (possibly compound) 'if' statement was never
					// true.
					elevp->live = elevp[-1].live && !elevp->live && !elevp->ifwastrue;
					elevp->elseseen = true;
					goto onward;
				case kw_elsif:	// 'elsif' directive.

					// At top level, initiated by a loop directive, or 'elsif' follows 'else'?
					if(elevp == execlevel || elevp->loopspawn || elevp->elseseen) {

						// Yep, bail out.
						strp = "elsif";
						eline = text198;
							// "Misplaced '%s' directive"
						goto misplaced;
						}

					// Check expression if prior level is true, current level is false, and this compound if
					// statement has not yet been true.
					if(elevp[-1].live && !elevp[0].live && !elevp->ifwastrue)
						goto evalif;

					// Not evaluating so set current execution state to false.
					elevp->live = false;
					goto onward;
				case kw_endif:	// 'endif' directive.

					// At top level or was initiated by a loop directive?
					if(elevp == execlevel || elevp->loopspawn) {

						// Yep, bail out.
						strp = "endif";
						eline = text198;
							// "Misplaced '%s' directive"
						goto misplaced;
						}

					// Extra arguments?
					if(extrasym())
						goto rcexit;

					// Done with current level: return to previous one.
					--elevp;
					goto onward;

				// Directives that begin or end a loop.
				case kw_while:	// 'while' directive.
					go = true;
					goto doloop;
				case kw_until:	// 'until' directive.
					go = false;
					// Fall through.
				case kw_loop:	// 'loop' directive.
doloop:
					// Current level true?
					if(elevp->live) {

						// Yes, get the value of the 'while' or 'until' logical expression.
						if(dirnum != kw_loop) {
							nodeinit(&node,vp);
							if(ge_comma(&node) != SUCCESS)
								goto rcexit;
							}

						// Extra arguments?
						else if(extrasym())
							goto rcexit;

						// If (1), a 'loop' directive; (2), the 'while' condition is true; or (3), the
						// 'until' condition if false ... begin a new level, set it to true, and
						// continue.
						if(dirnum == kw_loop || vistrue(vp) == go) {
							if(++elevp == elevpz)
								goto toodeep;
							elevp->live = true;
							elevp->loopspawn = true;
							elevp->ifwastrue = elevp->elseseen = false;
							elevp->loopcount = 0;
							goto onward;
							}
						}

					// Current level or condition is false: skip this block.
					goto jumpdown;

				case kw_break:	// 'break' directive.
				case kw_next:	// 'next' directive.

					// Ignore if current level is false (nothing to jump out of).
					if(elevp->live == false)
						goto onward;

					// Check 'break' argument.
					if(dirnum == kw_break) {
						if(havesym(s_any,false)) {
							nodeinit(&node,vp);
							if(ge_comma(&node) != SUCCESS || !intval(vp))
								goto rcexit;
							if(vp->u.v_int <= 0) {
								(void) rcset(FAILURE,0,text217,vp->u.v_int);
									// "'break' level '%ld' must be 1 or greater"
								goto rcexit;
								}
							breaklevel = vp->u.v_int;
							}
						else
							breaklevel = 1;
						}

					// Extraneous 'next' argument(s)?
					else if(extrasym())
						goto rcexit;
jumpdown:
					// Continue or break out of loop: find the right block (kw_while, kw_until, kw_loop,
					// kw_break, or kw_next) and jump down to its 'endloop'.
					for(lbp = bufp->b_execp; lbp != NULL; lbp = lbp->lb_next)
						if(lbp->lb_mark == lnp) {

							// If this is a 'break' or 'next', set the line pointer to the line
							// before the 'endloop' line so that the 'endloop' is executed;
							// otherwise, set it to the to the 'endloop' line itself so that the
							// 'endloop' is bypassed.
							lnp = (dirnum & DBREAKTYPE) ? lback(lbp->lb_jump) : lbp->lb_jump;

							// Return to the most recent loop level (bypassing 'if' levels) if this
							// is a 'break' or 'next'; otherwise, just reset the loop counter.
							if(!(dirnum & DBREAKTYPE))
								elevp->loopcount = 0;
							else if(prevlevel(execlevel,&elevp) != SUCCESS)
								goto rcexit;

							goto onward;
							}
notfound:
					// Huh?  'endloop' line not found! (This is a bug.)
					(void) rcset(FAILURE,0,text126);
						// "Script loop boundary line not found"
					goto rcexit;

				case kw_endloop:	// 'endloop' directive.

					// Extra arguments?
					if(extrasym())
						goto rcexit;

					// This directive is executed only when its partner is a 'loop', a 'while' that's true,
					// or an 'until' that's false, or jumped to from a 'break' or 'next' (otherwise, it's
					// bypassed).
					if(breaklevel == 0) {

						// Is current level top or was initiated by an 'if' directive?
						if(elevp == execlevel || !elevp->loopspawn) {

							// Yep, bail out.
							strp = "endloop";
							eline = text198;
								// "Misplaced '%s' directive"
misplaced:
							(void) rcset(FAILURE,0,eline,strp);
							goto rcexit;
							}

						// Return to the previous level and check if maxloop exceeded.
						--elevp;
						if(maxloop > 0 && ++elevp->loopcount > maxloop) {
							(void) rcset(FAILURE,0,text112,maxloop);
								// "Maximum number of loop iterations (%d) exceeded!"
							goto rcexit;
							}
						}

					// We're good ... just find the 'loop', 'while' or 'until', and go back to it, or to a
					// prior level if we're processing a 'break'.
					for(lbp = bufp->b_execp; lbp != NULL; lbp = lbp->lb_next)
						if((lbp->lb_type & DLOOPTYPE) && lbp->lb_jump == lnp) {

							// while/until/loop block found: set the line pointer to its 'endloop'
							// line or the line before its parent's 'endloop' line if we're
							// processing a 'break' (and return to the previous execution level);
							// otherwise, back to the beginning.
							if(breaklevel > 0) {
								if(--breaklevel > 0) {
									if(lbp->lb_break == NULL) {
										(void) rcset(FAILURE,0,text225,breaklevel);
									// "Too many break levels (%d short) from inner 'break'"
										goto rcexit;
										}
									lnp = lback(lbp->lb_break);

									// Return to the most recent loop level before this one.
									--elevp;
									if(prevlevel(execlevel,&elevp) != SUCCESS)
										goto rcexit;
									}
								else
									--elevp;
								elevp->loopcount = 0;	// Reset the loop counter.
								}
							else
								lnp = lback(lbp->lb_mark);
							goto onward;
							}

					// Huh?  'while', 'until', or 'loop' line not found! (This is a bug.)
					goto notfound;

				case kw_return:	// 'return' directive.

					// Current level true?
					if(elevp->live == true) {

						// Yes, get return value, if any.
						if(!havesym(s_any,false))
							vnilmm(rp);
						else {
							nodeinit(&node,rp);
							if(ge_comma(&node) != SUCCESS)
								goto rcexit;
							}

						// Exit script.
						goto elexit;
						}

					// Nope, ignore 'return'.
					goto onward;

				case kw_force:	// 'force' directive.

					// Check if argument present.
					if(!havesym(s_any,true))
						goto rcexit;
					forcecmd = true;
					break;
				default:
					;	// Do nothing.
				}
			} // End directive execution.

		// A 'force' or not a directive.  Execute the statement.
		if(elevp->live) {
			(void) doestmt(rp,eline1,TKC_COMMENT,NULL);
			if(forcecmd)		// Force SUCCESS status.
				(void) rcclear();

			// Check for exit or a command error.
			if(rc.status <= MINEXIT)
				return rc.status;
			if(rc.status != SUCCESS) {
				EWindow *winp = wheadp;

				// Check if buffer is on-screen.
				do {
					if(winp->w_bufp == bufp) {

						// Found a window.  Set dot to error line.
						winp->w_face.wf_dot.lnp = lnp;
						winp->w_face.wf_dot.off = 0;
						winp->w_flags |= WFHARD;
						}
					} while((winp = winp->w_nextp) != NULL);

				// In any case, set the buffer dot.
				bufp->b_face.wf_dot.lnp = lnp;
				bufp->b_face.wf_dot.off = 0;

				// Build a more detailed message that includes the command error message, if any.
				(void) macerror(NULL,NULL,bufp,lnp,flags);
				goto elexit;
				}
			} // End statement execution.
onward:
		// On to the next line.
		if(newlast.p_clp != NULL) {
			parseend(oldlast);
			newlast.p_clp = NULL;
			}
		lnp = lforw(lnp);
		} // End buffer execution.
#if MMDEBUG & (DEBUG_SCRIPT | DEBUG_PPBUF)
	fprintf(logfile,"*** END execbuf(%u)...\n",myinstance);
#endif
	// 'if' and 'endif' match?
	if(elevp == execlevel)
		goto elexit;
	(void) rcset(FAILURE,0,text121,text199);
		// "Unmatched '%s' directive","if"

	// Clean up and exit per rc.status.
rcexit:
	(void) macerror(NULL,NULL,bufp,lnp,flags);
elexit:
	if(newlast.p_clp != NULL)
		parseend(oldlast);

	return (rc.status == SUCCESS) ? rc.status : rcset(SCRIPTERROR,0,NULL);
	}

// Execute the contents of a buffer, given destination buffer for return value, numeric argument, buffer pointer, pathname of
// file being executed (or NULL), and invocation flags.
//
// Directives are:
//
//	macro <name>[,argct]	Begin definition of named macro with optional maximum number of arguments.
//	endmacro		End a macro definition.
//	if <cond>		Execute following lines if condition is true.
//	elsif <cond>		Execute following lines if prior 'if' condition was false and this condition is true.
//	else			Execute following lines if prior 'elsif' or 'if' condition was false.
//	endif			Terminate if/elsif/else.
//	return [value]		Return from current macro unconditionally with optional value.
//	force <cmd-line>	Force macro to continue ... even if <cmd-line> fails.
//	while <cond>		Execute a loop while the condition is true.
//	until <cond>		Execute a loop while the condition is false.
//	loop			Execute a loop forever (must contain a 'break').
//	endloop			Terminate a 'while', 'until', or 'loop'.
//	break [n]		Break out of n enclosing loops (default 1).
//	next			Return to top of current loop (and reevaluate condition, if any).

// For version ??:
//	do			Execute the block of statements ending at 'while' (until its condition is false) or 'until'
//				(until its condition is true).
//
#if MMDEBUG & DEBUG_MARG
int _dobuf(Value *rp,int n,Buffer *bufp,char *runpath,uint flags) {
#else
int dobuf(Value *rp,int n,Buffer *bufp,char *runpath,uint flags) {
#endif
	// Check for narrowed buffer and runaway recursion.
	if(bufp->b_flags & BFNARROW)
		(void) rcset(FAILURE,0,text72);
			// "Cannot execute a narrowed buffer"
	else if(maxrecurs > 0 && (opflags & OPEVAL) && bufp->b_nexec >= maxrecurs)
		(void) rcset(FAILURE,0,text319,maxrecurs);
			// "Maximum recursion depth (%d) exceeded"
	else {
		MacArgList *malp;

		// Get macro arguments.
#if MMDEBUG & DEBUG_MARG
		if(margalloc(n,bufp,&malp,flags) == SUCCESS) {
#else
		if(margalloc(bufp,&malp,flags) == SUCCESS) {
#endif
			// If not in "consume" mode, preprocess buffer if needed.
			if((opflags & OPEVAL) && ((bufp->b_flags & BFPREPROC) || ppbuf(bufp,flags) == SUCCESS)) {
				ScriptRun *oldrun,newrun;
				uint oldscript;

				// Make new run instance and prepare for execution.
				oldrun = scriptrun;
				scriptrun = &newrun;
				oldscript = opflags & OPSCRIPT;
				scriptrun->malp = malp;
				scriptrun->path = fixnull(runpath);
				scriptrun->bufp = bufp;
				if(vnew(&scriptrun->nargp,true) != 0)
					return vrcset();			// Fatal error.
				vsetint(n,scriptrun->nargp);
				scriptrun->uvp = lvarsheadp;
				opflags = (opflags & ~OPPARENS) | OPSCRIPT;

				// Flag that we are executing the buffer and execute it.
				bufp->b_nexec += 1;
				(void) execbuf(rp,bufp,flags);			// Execute the buffer.
				uvarclean(scriptrun->uvp);			// Clear any local vars that were created.

				// Clean up.
				bufp->b_nexec -= 1;
				opflags = (opflags & ~OPSCRIPT) | oldscript;
#if VDebug
				vdelete(scriptrun->nargp,"dobuf");
#else
				vdelete(scriptrun->nargp);
#endif
				scriptrun = oldrun;
				}

			// Free up the macro arguments.
			margfree(malp,NULL);
			}
		}

	return rc.status;
	}

#if MMDEBUG & DEBUG_MARG
// Call _dobuf() with debugging.
int dobufDebug(Value *rp,int n,Buffer *bufp,char *runpath,uint flags) {

	fprintf(logfile,"*dobuf(...,%d,%s,%s,%.4x)...\n",n,bufp->b_bname,runpath,flags);
	fflush(logfile);
	(void) _dobuf(rp,n,bufp,runpath,flags);
	fprintf(logfile,"dobuf(%s) returned status %d [%s]\n",bufp->b_bname,rc.status,rc.msg.v_strp);
	fflush(logfile);
	return rc.status;
	}
#endif

// Yank a file into a buffer and execute it, given result pointer, filename, n arg, and macro invocation "at startup" flag (used
// for error reporting).  Suppress any extraneous messages during read.  If there are no errors, delete the buffer on exit.
int dofile(Value *rp,char *fname,int n,uint flags) {
	Buffer *bufp;

	if(bfind(fname,CRBCREATE | CRBUNIQ | CRBFILE,BFHIDDEN,&bufp,NULL) != SUCCESS)	// Create a buffer...
		return rc.status;
	bufp->b_modes = MDRDONLY;					// mark the buffer as read-only...
	if(setfname(bufp,fname) == SUCCESS) {				// set the filename...
		uint flags = modetab[MDR_GLOBAL].flags & MDMSG;		// save current message flag...
		modetab[MDR_GLOBAL].flags &= ~MDMSG;			// clear it...
		(void) readin(bufp,fname,true);				// read in the file...
		modetab[MDR_GLOBAL].flags |= flags;			// restore message flag...
		if(rc.status == SUCCESS)
			(void) dobuf(rp,n,bufp,bufp->b_fname,flags);	// and execute the buffer.
		}

	// If not displayed, remove the now unneeded buffer and exit.
	bufp->b_flags &= ~BFHIDDEN;
	return (rc.status == SUCCESS && bufp->b_nwind == 0) ? bdelete(bufp,CLBIGNCHGD) : rc.status;
	}

// Execute commands in a file and return result in rp.
int xeqFile(Value *rp,int n) {
	char *pathp;

	// Get the filename.
	if(getarg(rp,text129,NULL,CTRL | 'M',MaxPathname,ARG_FIRST | ARG_STR | ARG_NOTNULL | TERM_C_FNAME) != SUCCESS ||
	 (!(opflags & OPSCRIPT) && vistfn(rp,VNIL)))
			// "Execute script file"
		return rc.status;

	// Look up the path...
	if(pathsearch(&pathp,rp->v_strp,false) != SUCCESS)
		return rc.status;
	if(pathp == NULL)
		return rcset(FAILURE,0,text152,rp->v_strp);	// Not found.
			// "No such file \"%s\""

	// save it...
	if(vsetstr(pathp,rp) != 0)
		return vrcset();

	// skip any comma token...
	if((opflags & OPSCRIPT) && !getcomma(false) && rc.status != SUCCESS)
		return rc.status;

	// and execute it with arg n.
	return dofile(rp,rp->v_strp,n,0);
	}
