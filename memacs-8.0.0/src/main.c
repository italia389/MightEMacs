// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// main.c	Entry point for the MightEMacs text editor.
//
// This file contains the main (top level) routine and some keyboard processing code.
//
// Program notes:
// 1. In general, keys are read in editloop(), command functions are called with a numeric argument, and a status code is
// returned.  If no numeric argument is specified by the user or command line, INT_MIN is used as the default (and many command
// functions will have a default behavior in that case).  The called functions typically call (many) other non-command functions
// which return a status code as well.  The return code is usually SUCCESS or FAILURE; however, it can be more severe (see
// edef.h for list), such as USERABORT or FATALERROR.  Any code worse than USERABORT will cause program termination.  The most
// severe return code from the most distant function in the call chain will be ultimately be returned to editloop(), where it
// will be dealt with; that is, a message will be displayed if applicable before beginning the next loop.  This means that a
// more severe return code and message will always override a less severe one (which is enforced by the rcset() function).
//
// 2. There is a special return code, NOTFOUND, which may be returned directly from a function, bypassing rcset().  This code
// typically indicates an item was not found or the end of a list was reached, and it requires special attention.  It is the
// responsibility of the calling function to decide what to do with it.  The functions that may return NOTFOUND are:
//
// Function(s)							Returns NOTFOUND, bypassing rcset() when ...
// ---------------------------------------------------------	---------------------------------------------------------------
// backChar(), forwChar(), backLine(), forwLine(), ldelete()	Move or delete is out of buffer.
// getsym(), parsetok()						Token not found and no error.
// getcbn()							Buffer not found, interactive mode, and operation is OPQUERY.
// startup()							Script file not found.
// mcscan(), scan()						Search string not found.
// ereaddir()							No entries left.

#include "os.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

// Make selected global definitions local.
#define DATAmain
#define DATAfunc

#include "edef.h"		// Global structures and defines.
#include "efunc.h"		// Variable prototype definitions.
#include "elang.h"		// Human language definitions.
#include "ecmd.h"		// Command name table.
#include "edata.h"		// Global definitions.

// Clear return code message, set return code status to SUCCESS, and return it.
int rcclear(void) {

	vnull(&rc.msg);
	rc.flags = 0;
	return rc.status = SUCCESS;
	}

// Copy one return code object to another.  Return status.
static int rccpy(RtnCode *destp,RtnCode *srcp) {

	destp->status = srcp->status;
	destp->flags = srcp->flags;
	destp->clhelptext = srcp->clhelptext;
#if MMDEBUG & DEBUG_VALUE
	if(vcpy(&destp->msg,&srcp->msg) != 0)
		(void) vrcset();
	fprintf(logfile,"rccpy(): copied from (%hu) '%s' [%d] to (%hu) '%s' [%d]\n",
	 srcp->msg.v_type,srcp->msg.v_strp,srcp->status,destp->msg.v_type,destp->msg.v_strp,destp->status);
	return rc.status;
#else
	return vcpy(&destp->msg,&srcp->msg) == 0 ? rc.status : vrcset();
#endif
	}

// Write text into the return code message (unless RCKEEPMSG flag) and return most severe status.  If status is SUCCESS, RCFORCE
// flag is not set, and global 'msg' mode is not set, do nothing; otherwise, if status is the same, keep existing message (do
// nothing) unless it's a force or existing message is null.
int rcset(int status,uint flags,char *fmtp,...) {
	va_list ap;

	// Check status.  If this status is not more severe, or SUCCESS, not force, and not displaying messages, return old one.
	if(status > rc.status || (!(flags & RCFORCE) && ((status == SUCCESS && !(modetab[MDR_GLOBAL].flags & MDMSG)) || 
	 (status == rc.status && !visnull(&rc.msg)))))
		return rc.status;

	// Save message (if RCKEEPMSG flag is not set) and new status.
	if(status == HELPEXIT)
		rc.clhelptext = fmtp;
	else if(fmtp != NULL) {
		if(!(flags & RCKEEPMSG) || visnull(&rc.msg)) {
			char *strp;

			if(status != PANIC) {
				va_start(ap,fmtp);
				(void) vasprintf(&strp,fmtp,ap);
				va_end(ap);
				}

			// If PANIC serverity or vasprintf() failed, panic!  That is, we're most likely out of memory, so bail
			// out with "great expediency".
			if(status == PANIC || strp == NULL) {
				(void) vttidy(true);
				fprintf(stderr,"%s: ",text189);
						// "Abort"
				if(status == PANIC) {
					va_start(ap,fmtp);
					vfprintf(stderr,fmtp,ap);
					va_end(ap);
					}
				else
					fprintf(stderr,text94,"rcset");
							// "%s(): Out of memory!"
				fputc('\n',stderr);
				exit(-1);
				}
			vsethstr(strp,&rc.msg);
			}
		}
	else if(flags & RCFORCE)
		vnull(&rc.msg);
	rc.flags = flags;
#if MMDEBUG & DEBUG_VALUE
	fprintf(logfile,"rcset(%d,%u,...): set msg '%s', status %d\n",status,flags,rc.msg.v_strp,status);
#endif
	return (rc.status = status);
	}

// Save current return code and message for $ReturnMsg.  Return status.
int rcsave(void) {
	StrList msg;

	// Copy to scriptrc via vstrlit().
	if(vopen(&msg,&scriptrc.msg,false) != 0)
		(void) vrcset();
	else {
		scriptrc.status = rc.status;
		if(vstrlit(&msg,rc.msg.v_strp,0) != 0 || vclose(&msg) != 0)
			(void) vrcset();
		}
	return rc.status;
	}

// Concatenate any function arguments and save prefix and result in *vpp.  Return status.
static int buildmsg(Value **vpp,char *prefix) {
	StrList msg;
	Value *vp;

	if(vnew(&vp,false) != 0 || vnew(vpp,false) != 0)
		return vrcset();
	if(join(vp,NULL,0,true) != SUCCESS || (prefix == NULL && (visnull(vp) || vistfn(vp,VNIL))))
		return rc.status;
	if(vopen(&msg,*vpp,false) != 0 || (prefix != NULL && vputs(prefix,&msg) != 0))
		return vrcset();
	if(!visnull(vp) && !vistfn(vp,VNIL))
		if((!vempty(&msg) && vputs(": ",&msg) != 0) || vputv(vp,&msg) != 0)
			return vrcset();
	return vclose(&msg) != 0 ? vrcset() : rc.status;
	}

// Clear keyboard macro and set to STOP state if "stop" is true.
void clearKeyMacro(bool stop) {

	kmacro.km_slotp = kmacro.km_buf;
	kmacro.km_endp = NULL;
	if(stop) {
		if(kmacro.km_state == KMRECORD)
			curwp->w_flags |= WFMODE;
		kmacro.km_state = KMSTOP;
		}
	}

// Begin recording a keyboard macro.  Error if not in the KMSTOPped state.  Set up variables and return status.
int beginKeyMacro(Value *rp,int n) {

	clearKeyMacro(false);
	if(kmacro.km_state != KMSTOP) {
		kmacro.km_state = KMSTOP;
		return rcset(FAILURE,0,text105);
			// "Macro already active, cancelled"
		}
	kmacro.km_state = KMRECORD;
	curwp->w_flags |= WFMODE;
	return rcset(SUCCESS,0,text106);
		// "Begin macro"
	}

// End keyboard macro recording.  Check for the same limit conditions as the above routine.  Set up the variables and return
// status.
int endKeyMacro(Value *rp,int n) {

	if(kmacro.km_state == KMSTOP)
		return rcset(FAILURE,0,text107);
			// "Keyboard macro not active"

	// else in KMRECORD state (KMPLAY not possible).
	kmacro.km_endp = kmacro.km_slotp;
	kmacro.km_state = KMSTOP;
	curwp->w_flags |= WFMODE;
	return rcset(SUCCESS,0,text108);
		// "End macro"
	}

// Enable execution of a keyboard macro n times.  Error if not in KMSTOPped state.  The macro is actually run by the tgetc()
// function.  Set up the variables and return status.
int xeqKeyMacro(Value *rp,int n) {

	if(kmacro.km_state != KMSTOP) {
		clearKeyMacro(true);
		return rcset(FAILURE,0,text105);
			// "Macro already active, cancelled"
		}
	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Command repeat count"
	if(kmacro.km_endp == NULL)
		return rcset(FAILURE,0,text200);
				// "No keyboard macro defined"

	kmacro.km_n = n;			// Remember how many times to execute.
	kmacro.km_state = KMPLAY;		// Switch to "play" mode ...
	kmacro.km_slotp = kmacro.km_buf;	// at the beginning.

	return rc.status;
	}

// Beep the beeper n times.  (Mainly for macro use.)
int beeper(Value *rp,int n) {

	// Check if n is in range.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0 || n > 10)
		return rcset(FAILURE,0,text12,text137,n,0,10);
				// "%s (%d) must be between %d and %d"
					// "Command repeat count"

	// Beep n times.
	while(n-- > 0)
		if(TTbeep() != SUCCESS)
			break;

	return rc.status;
	}

// Abort.  Beep the beeper.  Kill off any keyboard macro that is in progress.  Called by "abort" command and any user-input
// function if user presses abort key (usually ^G).  Set and return USERABORT status.
int abortinp(void) {

	if(TTbeep() != SUCCESS)
		return rc.status;
	if(kmacro.km_state == KMRECORD)
		curwp->w_flags |= WFMODE;
	kmacro.km_state = KMSTOP;
	return rcset(USERABORT,0,text8);
			// "Aborted!"
	}

// Call abortinp() to abort processing and build optional exception message.
int abortOp(Value *rp,int n) {

	(void) abortinp();

	// Short and sweet if called by user pressing a key.
	if(kentry.lastread == ckeys.abort)
		return rc.status;

	// Called from elsewhere (a script) ... build a message.
	(void) rcclear();
	Value *vp;
	if(buildmsg(&vp,text189) == SUCCESS)
			// "Abort"
		(void) rcset(USERABORT,0,vp->v_strp);

	return rc.status;
	}

// Concatenate arguments and force-set result as an informational (SUCCESS) return message.  Return true if default n or n >= 0;
// otherwise, false.  Suppress brackets [] if n <= 0.
int notice(Value *rp,int n) {

	if(havesym(s_any,true)) {
		Value *vp;

		if(buildmsg(&vp,NULL) == SUCCESS) {
			if(vsetstr(n == INT_MIN || n >= 0 ? val_true : val_false,rp) != 0)
				(void) vrcset();
			else
				(void) rcset(SUCCESS,n <= 0 && n != INT_MIN ? RCNOWRAP | RCFORCE : RCFORCE,vp->v_strp);
			}
		}

	return rc.status;
	}

// Get name from the function table, given index.  Used in binary() calls.
static char *cfname(int index) {

	return cftab[index].cf_name;
	}

// Search the function table for given name and return pointer to corresponding entry, or NULL if not found.
CmdFunc *ffind(char *cnamep) {
	int nval;

	return ((nval = binary(cnamep,cfname,NFUNCS)) == -1) ? NULL : cftab + nval;
	}

// Check current buffer state.  Return error if "edit" is true and in an executing buffer or in read-only mode.
int allowedit(bool edit) {

	if(edit) {
		if(curbp->b_modes & MDRDONLY) {
			(void) TTbeep();
			return rcset(FAILURE,0,text109,text58);
				// "%s is in read-only mode","Buffer"
			}

		if(curbp->b_nexec > 0) {
			(void) TTbeep();
			return rcset(FAILURE,0,text284,text276,text248);
				// "Cannot %s %s buffer","modify","an executing"
			}
		}

	return rc.status;
	}

// Dummy function for binding to meta prefix (usually ESC).
int metaPrefix(Value *rp,int n) {

	return rc.status;
	}

// Dummy function for binding to "c" command prefix (usually ^C).
int cPrefix(Value *rp,int n) {

	return rc.status;
	}

// Dummy function for binding to "h" command prefix (usually ^H).
int hPrefix(Value *rp,int n) {

	return rc.status;
	}

// Dummy function for binding to "x" command prefix (usually ^X).
int xPrefix(Value *rp,int n) {

	return rc.status;
	}

// Dummy function for binding to universal argument (usually ^U).
int universalArg(Value *rp,int n) {

	return rc.status;
	}

// Dummy function for binding to negative argument (usually ^_).
int negativeArg(Value *rp,int n) {

	return rc.status;
	}

// Copy a string with buffer overflow checking.  Update *destp to point to trailing null and return status.
int chkcpy(char **destp,char *srcp,size_t destlen) {
	size_t srclen;

	if((srclen = strlen(srcp)) >= destlen)
		return rcset(FAILURE,0,text255,text58,strsamp(srcp,srclen,0));
			// "%s overflow copying string '%s'","Buffer"
	*destp = stpcpy(*destp,srcp);
	return rc.status;
	}

// Pad a string to indicated length and return pointer to terminating null.
char *pad(char *s,int len) {
	char *strp1,*strp2;
	int curlen;

	strp1 = strchr(s,'\0');
	if((curlen = strp1 - s) < len) {
		strp2 = strp1 + (len - curlen);
		do {
			*strp1++ = ' ';
			} while(strp1 < strp2);
		*strp1 = '\0';
		}
	return strp1;
	}

// Return a sample of a string for error reporting, given pointer, length, and maximum length (excluding the trailing null).
// If maximum length is zero, use a small size derived from the terminal width.
char *strsamp(char *srcp,size_t srclen,size_t maxlen) {
	size_t len;

	// Set maxlen to maximum destination size.
	if(maxlen == 0)
		maxlen = sampbuf.smallsize;
	else if(maxlen >= sampbuf.buflen)
		maxlen = sampbuf.buflen - 1;

	// Now compute sample size ...
	if((len = srclen) > maxlen)
		len = maxlen;
 
	// and return string sample.
	return strfit(sampbuf.buf,len,srcp,srclen);
	}

// Disable a hook that went awry.  Return status.
static int dishook(HookRec *hrp) {
	StrList msg;
	int rcode = 0;
	FABPtr *fabp = &hrp->h_fab;

	// Hook execution failed, let the user know what's up.
	if(rc.status > FATALERROR) {
		if((rcode = vopen(&msg,&rc.msg,visnull(&rc.msg) ? false : true)) == 0) {
			if(vempty(&msg)) {
				if(fabp->p_type == PTRCMD)
					rcode = vputf(&msg,text175,fabp->u.p_cfp->cf_name);
						 // "Command '%s' failed"
				else
					rcode = vputf(&msg,text176,fabp->u.p_bufp->b_bname + 1);
						 // "Macro '%s' failed"
				}
#if MMDEBUG & DEBUG_VALUE
			if(rcode == 0) {
				fprintf(logfile,"dishook(): calling vputf(...,%s,%s) ...\n",text161,hrp->h_name);
				rcode = vputf(&msg,text161,hrp->h_name);
				vdump(msg.sl_vp,"dishook(): post vputf() ...");
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

	// disable the hook ...
	fabp->p_type = PTRNUL;

	// and set value object error if needed.
	return (rcode == 0) ? rc.status : vrcset();
	}

// Execute a command or buffer bound to a hook with rp (if not NULL) and n.  If arginfo == 0, invoke the command or macro
// directly; otherwise, build a command line with arguments.  In the latter case, arginfo contains the argument count and types
// of each argument (string or long).  The lowest four bits of arginfo is the argument count.  Each higher bit is 0 for a string
// argument and 1 for a long (integer) argument.
int exechook(Value *rp,int n,HookRec *hrp,uint arginfo,...) {
	bool fscall = false;		// feval() or doestmt() call?
	FABPtr *fabp = &hrp->h_fab;

	// Unbound hook?
	if(fabp->p_type == PTRNUL)
		return rc.status;

	// Don't allow edit command if in read-only mode.
	if(fabp->p_type == PTRCMD && allowedit(fabp->u.p_cfp->cf_flags & CFEDIT) != SUCCESS)
		goto fail;

	if(rp == NULL && vnew(&rp,false) != 0)		// For throw-away return value.
		goto vfail;

	// Build command line if requested.
	if(arginfo != 0) {
		va_list ap;
		StrList cmd;
		int delim = ' ';
		int argct = arginfo & 0xf;

		// Concatenate command or macro name with arguments ...
		if(vopen(&cmd,NULL,false) != 0 || (n != INT_MIN && vputf(&cmd,"%d => ",n) != 0) ||
		 vputs(fabp->p_type == PTRCMD ? fabp->u.p_cfp->cf_name : fabp->u.p_bufp->b_bname + 1,&cmd) != 0)
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
		fprintf(logfile,"exechook(): calling doestmt(%s) ...\n",cmd.sl_vp->v_strp);
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
	else if(fabp->p_type == PTRCMD) {
		CmdFunc *cfp = fabp->u.p_cfp;
		if((rp != NULL && vnilmm(rp) != SUCCESS) ||
		 (cfp->cf_func == NULL ? (feval(rp,n,cfp),fscall = true) : cfp->cf_func(rp,n)) != SUCCESS)
			goto fail;
		}
	else if(dobuf(rp,n,fabp->u.p_bufp,NULL,0) != SUCCESS)
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

// Execute a command or macro bound to given key (not a hook).
static int execkey(KeyDesc *kdp,int n) {
	FABPtr *fabp = &kdp->k_fab;
	Value *vsinkp;				// For throw-away return value, if any.

	if(vnew(&vsinkp,false) != 0 || vsetstr(val_nil,vsinkp) != 0)
		return vrcset();

	// Bound to a macro?
	if(fabp->p_type == PTRMACRO)
		(void) dobuf(vsinkp,n,fabp->u.p_bufp,NULL,0);

	// Bound to a command.  Perform "edit" check before invoking it.
	else {
		CmdFunc *cfp = fabp->u.p_cfp;
		if(allowedit(cfp->cf_flags & CFEDIT) == SUCCESS)
			(void) (cfp->cf_func == NULL ? feval(vsinkp,n,cfp) : cfp->cf_func(vsinkp,n));
		}

	// False return?
	if(rc.status == SUCCESS && vistfn(vsinkp,VFALSE))
		(void) rcset(FAILURE,RCKEEPMSG,text300);
				// "False return"
	return rc.status;
	}

// Initialize the return message structures.  Return status.
static int edinit0(void) {

	// Initialize i var, return code, and numeric literals.
#if VDebug
	vinit(&ivar.format,"edinit1");
#else
	vinit(&ivar.format);
#endif
	if(vsetfstr("%d",2,&ivar.format) != 0)
		return vrcset();
#if VDebug
	vinit(&rc.msg,"edinit1");
	vinit(&scriptrc.msg,"edinit1");
#else
	vinit(&rc.msg);
	vinit(&scriptrc.msg);
#endif
	return rc.status;
	}

// Initialize all of the core data structures.  Return status.
static int edinit1(void) {
	uint n;
	ushort *usp;
	FABPtr fab;
	KeyDesc *kdp;
	CAMRec *crp1,*crp2;
	CmdFunc *cfp;
	static char myname[] = "edinit1";
	struct {
		char *alias;
		FABPtr fab;
		} *ap,aliases[] = {
			{"cd",{PTRCMD,cftab + cf_chDir}},
			{"quit",{PTRCMD,cftab + cf_exit}},
			{"require",{PTRCMD,cftab + cf_xeqFile}},
			{NULL,{PTRNUL,NULL}}};

	// Initialize keyboard macro.
	clearKeyMacro(false);

	// Get space for SampBuf and MsgLine buffers.
	sampbuf.buflen = ml.buflen = n = term.t_mcol + 1;
	sampbuf.smallsize = n / 4;

	if((ml.span = (char *) malloc(n)) == NULL || (sampbuf.buf = (char *) malloc(sampbuf.buflen)) == NULL)
		return rcset(PANIC,0,text94,myname);
			// "%s(): Out of memory!"
	*sampbuf.buf = '\0';

	// Load all the key bindings.
	if(loadbind() != SUCCESS)
		return rc.status;

	// Initialize the kill ring.
	kringp = kring;
	do {
		kringp->kbufp = kringp->kbufh = NULL;
		kringp->kskip = 0;
		kringp->kused = KBLOCK;
		} while(++kringp < kringz);
	kringp = kring;

	// Initialize the CAM list with all command names and the core-key record.
	usp = (ushort *) &ckeys;
	fab.p_type = PTRCMD;
	cfp = cftab + NFUNCS;
	crp2 = NULL;
	do {
		// Skip non-bindable functions.
		if((--cfp)->cf_flags & CFFUNC)
			continue;

		// Allocate memory for a CAM record.
		if((crp1 = (CAMRec *) malloc(sizeof(CAMRec))) == NULL)
			return rcset(PANIC,0,text94,myname);
				// "%s(): Out of memory!"
		crp1->cr_name = cfp->cf_name;
		crp1->cr_type = PTRCMD;
		crp1->cr_nextp = crp2;
		crp2 = crp1;

		// Store core key binding in "ckeys".
		if(cfp->cf_flags & CFUNIQ) {
			fab.u.p_cfp = cfp;
			*usp++ = ((kdp = getpentry(&fab)) == NULL) ? 0U : kdp->k_code;
			}
		} while(cfp > cftab);
	crheadp = crp2;

	// Initialize the alias list.
	ap = aliases;
	do {
		if(afind(ap->alias,OPCREATE,&ap->fab,NULL) != SUCCESS)
			return rc.status;
		} while((++ap)->alias != NULL);

	// Clear the search tables.
	srch.fdelta1[0] = -1;
	srch.rmcpat[0].mc_type = MCE_NIL;
	mcclear();

	return rc.status;
	}

// Initialize all of the buffers, windows, screens, and aliases.  Return status.
static int edinit2(void) {

	// Allocate the initial buffer and default screen.
	if(bfind(buffer1,CRBCREATE,0,&curbp,NULL) != SUCCESS || sfind(1,curbp,&sheadp) != SUCCESS)
		return rc.status;

	// Set the current (default) screen/buffer/window.
	curwp = wheadp = (cursp = sheadp)->s_curwp = sheadp->s_wheadp;

	return rc.status;
	}

// Set execpath global variable using heap space.  Return status.
int setpath(char *path,bool prepend) {
	char *strp1,*strp2;
	size_t len;

	len = strlen(path) + 1;
	if(prepend)
		len += strlen(execpath) + 1;
	if((strp1 = (char *) malloc(len)) == NULL)
		return rcset(PANIC,0,text94,"setpath");
			// "%s(): Out of memory!"
	strp2 = stpcpy(strp1,path);
	if(prepend) {
		*strp2++ = ':';
		strcpy(strp2,execpath);
		}
	if(execpath != NULL)
		free((void *) execpath);
	execpath = strp1;

	return rc.status;
	}

// Execute a startup file, given filename, "search HOME only" flag, and "ignore missing" flag.  If file not found and ignore is
// true, return (SUCCESS) status; otherwise, return an error.
static int startup(char *sfname,int hflag,bool ignore) {
	char *fname;			// Resulting filename to execute.
	Value *vsinkp;			// For throw-away return value, if any.

	if(pathsearch(&fname,sfname,hflag) != SUCCESS)
		return rc.status;
	if(fname == NULL) {
		if(ignore)
			return rc.status;
		char wkbuf[strlen(text136) + strlen(execpath)];
				// " in path '%s'",
		if(strchr(sfname,'/') == NULL)
			sprintf(wkbuf,text136,execpath);
		else
			wkbuf[0] = '\0';
		return rcset(FAILURE,0,text40,sfname,wkbuf);
			// "Script file '%s' not found%s"
		}

	// Found: execute the sucker.
	if(vnew(&vsinkp,false) != 0)
		return vrcset();
	return dofile(vsinkp,fname,INT_MIN,SRUN_STARTUP);
	}

// Process -D or -G switch value.
static int modeswitch(char *value,int index) {
	size_t offset;
	char *nargp,*endp;
	ModeRec *mrp = modetab + index;
	Value *vsinkp;
	StrList cmd;

	if(vnew(&vsinkp,false) != 0)
		return vrcset();
	for(;;) {
		// Find beginning and end of next mode string.
		if((endp = strchr(value,',')) == NULL)
			endp = strchr(value,'\0');

		// Build command in cmd and execute it.
		if(*value == '!') {
			nargp = "-1";
			offset = 1;
			}
		else {
			nargp = "1";
			offset = 0;
			}
		if(vopen(&cmd,NULL,false) != 0 || vputf(&cmd,"%s => alter%.*sMode '%.*s'",nargp,*mrp->cmdlabel == 'D' ? 3 : 99,
		 mrp->cmdlabel,(int)(endp - value - offset),value + offset) != 0 || vclose(&cmd) != 0)
			return vrcset();
		if(doestmt(vsinkp,cmd.sl_vp->v_strp,TKC_COMMENT,NULL) != SUCCESS) {
			StrList msg;

			// Append specifics to rc.msg.
			return (vopen(&msg,&rc.msg,true) != 0 || vputf(&msg,text43,*mrp->cmdlabel) != 0 ||
			 vclose(&msg) != 0) ? vrcset() : rcset(FATALERROR,0,NULL);
											// " specified with -%c switch"
			}

		// Onward ...
		if(*endp == '\0')
			break;
		value = endp + 1;
		}
	return rc.status;
	}

// Check for a switch value.
static int swval(char *sw) {
	char wkbuf[6];

	if(sw[1] == '\0') {
		sprintf(wkbuf,*sw == '+' ? "'%c'" : "-%c",*sw);
		return rcset(FATALERROR,0,text45,wkbuf);
			// "%s switch requires a value"
		}
	return rc.status;
	}

// Process -g or + switch value.
static int gotoswitch(char *sw,long *ip1,long *ip2) {
	char *strp;
	struct {
		long *var;
		char buf[NWORK];
		} x[] = {{ip1,""},{ip2,""},{NULL,""}},*xp = x,*xpz;

	// Null value?
	if(swval(sw) != SUCCESS)
		return rc.status;

	// One or two values?
	if((strp = strchr(sw + 1,':')) == NULL) {
		stplcpy(x[0].buf,sw + 1,NWORK);
		xpz = xp + 1;
		}
	else {
		*strp = '\0';
		stplcpy(x[0].buf,sw + 1,NWORK);
		stplcpy(x[1].buf,strp + 1,NWORK);
		*strp = ':';
		xpz = xp + 2;
		}

	// Loop through values found and validate them.
	do {
		if(asc_long(xp->buf,xp->var,false) != SUCCESS) {
			StrList msg;

			return (vopen(&msg,&rc.msg,true) != 0 ||
			 vputf(&msg,text61,*sw == '+' ? sw : sw - 1) != 0 || vclose(&msg) != 0) ? vrcset() :
				// ", switch '%s'"
			 rcset(FATALERROR,0,NULL);
			}
		} while(++xp < xpz);

	return rc.status;
	}

// Determine which buffer to "land on" prior to entering edit loop, and read its file if appropriate.  bufp is buffer associated
// with first file on command line (if any).  Return status.
static int startbuf(Buffer *bufp,int stdinflag) {

	// Switch to first file (buffer) specified on command line if auto-load is enabled.
	if(bufp != NULL && (modetab[MDR_GLOBAL].flags & MDRD1ST)) {
		if(bswitch(bufp) != SUCCESS)
			return rc.status;
		curbp->b_modes |= modetab[MDR_DEFAULT].flags;
		}

	// Delete first buffer if it exists, is empty, is not reserved for standard input, and is not being displayed on current
	// screen.
	if(stdinflag == 0 && (bufp = bsrch(buffer1,NULL)) != NULL && lforw(bufp->b_hdrlnp) == bufp->b_hdrlnp &&
	 !bufWind(bufp,NULL,NULL)) {

		// If first buffer is on another screen, seek and "destroy" it.
		if(bufp->b_nwind > 0) {
			EWindow *winp,*oldcurwp;
			EScreen *scrp = sheadp;
			EScreen *oldcursp = cursp;
			Buffer *oldcurbp = curbp;

			// In all screens ...
			do {
				if(scrp != oldcursp) {
					(void) sswitch(scrp);
					oldcurwp = curwp;

					// In all windows ...
					winp = wheadp;
					do {
						if(winp->w_bufp == bufp) {		// Found the sucker ...
							wswitch(winp);			// switch to its window ...
							if(bswitch(oldcurbp) != SUCCESS) // and change the buffer.
								return rc.status;
							}
						} while((winp = winp->w_nextp) != NULL);
					if(curwp != oldcurwp)
						wswitch(oldcurwp);
					}
				} while((scrp = scrp->s_nextp) != NULL);
			if(cursp != oldcursp)
				(void) sswitch(oldcursp);
			}

		// All clear.  Delete first buffer.
		if(bdelete(bufp,0) != SUCCESS)
			return rc.status;
		}

	return update(true);
	}

// Display program copyright, version, brief command line usage, or detailed command line usage and exit, given level of
// -2, -1, 0, or 1.  Return HELPEXIT.
static int clhelp(int level,char *binname) {
	uint strsize;
	char *hp,*strp,**strpp;

	if(level < 0) {		// Copyright or version.
		strsize = strlen(myself);
		strsize += (level == -1) ? strlen(help0) + strlen(version) : strlen(copyright) + 1;
		}
	else {
		// Tally usage line.
		strsize = strlen(binname);
		strpp = usage;
		do {
			strsize += strlen(*strpp++);
			} while(*strpp != NULL);

		if(level > 0) {

			// Tally detailed help.
			strsize += strlen(myself);
			strsize += strlen(help1);
			strpp = help2;
			do {
				strsize += strlen(*strpp++);
				} while(*strpp != NULL);
			}
		}
	++strsize;

	// Get space for help string ...
	if((hp = (char *) malloc(strsize)) == NULL)
		return rcset(PANIC,0,text94,"clhelp");
			// "%s(): Out of memory!"

	// build it ...
	if(level == -2)
		sprintf(hp,"%s %s",myself,copyright);
	else if(level == -1)
		sprintf(hp,"%s%s%s",myself,help0,version);
	else {
		if(level > 0) {
			strp = stpcpy(hp,myself);
			strp = stpcpy(strp,help1);
			}
		else
			strp = hp;

		strp = stpcpy(strp,usage[0]);
		strp = stpcpy(strp,binname);
		for(strpp = usage + 1; *strpp != NULL; ++strpp)
			strp = stpcpy(strp,*strpp);

		if(level > 0) {
			strpp = help2;
			do {
				strp = stpcpy(strp,*strpp++);
				} while(*strpp != NULL);
			}
		*strp = '\0';
		}

	// and return it.
	return rcset(HELPEXIT,0,hp);
	}

// Scan command line arguments, looking for a -?, -C, -h, -n, or -V switch.  If one other than -n is found, return clhelp();
// otherwise, set *dostartp to false if -n found, true otherwise, and return status.
static int scancmdline(int argc,char *argv[],bool *dostartp) {
	char *binname;			// My binary name.
	char **strpp;
	static char *swlist[] = {
		"-C","-V","-?","-h",NULL};

	// Get my name.
	binname = fbasename(*argv,true);

	// Scan command line arguments.
	while(--argc > 0) {
		++argv;
		if(strcmp(*argv,"-n") == 0) {
			*dostartp = false;
			return rc.status;
			}
		strpp = swlist;
		do {
			if(strcmp(*argv,*strpp) == 0)
				return clhelp((int)(strpp - swlist) - 2,binname);
			} while(*++strpp != NULL);
		}

	*dostartp = true;
	return rc.status;
	}

// Process command line.  Return status.
static int docmdline(int argc,char *argv[],char *helpmsg) {
	int n;
	Buffer *bufp;			// Temp buffer pointer.
	Buffer *firstbp = NULL;		// Pointer to first buffer in cmd line.
	bool rdonlyflag = false;	// Are we starting in read-only mode?
	bool gotoflag = false;		// Do we need to goto a line at start?
	long gline;			// If so, what line?
	long gchar;			// and what character?
	bool searchflag = false;	// Do we need to search at start?
	int stdinflag = 0;		// Do we need to read from stdin at start?
	char *strp;
	Value *vsinkp;
	StrList cmd;
	int helpflag = true;		// Display initial help message?

	gline = gchar = 1;		// Line and character to go to.
	if(vnew(&vsinkp,false) != 0)
		return vrcset();

	// Parse command line.
	++argv;
	while(--argc > 0) {

		// Process switches.
		if((*argv)[0] == '-') {
			helpflag = false;
			switch((*argv)[1]) {
				case '\0':	// Solo '-' for reading from standard input.
					if(stdinflag)
						break;		// Ignore duplicate.
					stdinflag = 1;		// Process it.
					goto dofile;
				case 'D':	// -D for add/delete default buffer mode(s).
					n = MDR_DEFAULT;
					goto domode;
				case 'd':	// -d for change working directory.
					if(swval(*argv + 1) != SUCCESS)
						return rc.status;
					strp = "chDir ";
					n = true;		// Escape string.
					goto docmd;
				case 'e':	// -e<cmd-line> to execute a command line.
					if(swval(*argv + 1) != SUCCESS || doestmt(vsinkp,*argv + 2,TKC_COMMENT,NULL) != SUCCESS)
						return rc.status;
					break;
				case 'G':	// -G for add/delete global mode(s).
					n = MDR_GLOBAL;
domode:
					if(modeswitch(*argv + 2,n) != SUCCESS)
						return rc.status;
					break;
				case 'g':	// -g<line>[:<char>] for initial goto line and char.
					if(gotoswitch(*argv + 1,&gline,&gchar) != SUCCESS)
						return rc.status;
					gotoflag = true;
					break;
				case 'i':	// -i for setting $inpDelim system variable.
					strp = "$inpDelim = ";
					n = false;		// Don't escape string.
docmd:
					if(vopen(&cmd,NULL,false) != 0 || vputs(strp,&cmd) != 0)
						return vrcset();
					if(n) {
						if(quote(&cmd,*argv + 2,true) != SUCCESS)
							return rc.status;
						if(vclose(&cmd) != 0)
							return vrcset();
						}
					else if(vputc('"',&cmd) != 0 || vputs(*argv + 2,&cmd) != 0 || vputc('"',&cmd) != 0 ||
					 vclose(&cmd) != 0)
						return vrcset();
					if(doestmt(vsinkp,cmd.sl_vp->v_strp,TKC_COMMENT,NULL) != SUCCESS)
						return rc.status;
					break;
				case 'n':	// -n for skipping startup files (already processed).
					break;
				case 'R':	// -R for non-read-only (edit) mode.
					rdonlyflag = false;
					break;
				case 'r':	// -r for read-only mode.
					rdonlyflag = true;
					break;
				case 's':	// -s for initial search string.
					if(swval(*argv + 1) != SUCCESS)
						return rc.status;
					stplcpy(srch.pat,*argv + 2,NPAT + 1);
					newpat(-1);
					searchflag = true;
					break;
				case 'X':	// -X<path> for file-execute search path.
					if(swval(*argv + 1) != SUCCESS)
						return rc.status;
					if(setpath(*argv + 2,true) != SUCCESS)
						return rc.status;
					break;
				default:	// Unknown switch.
					return rcset(FATALERROR,0,text1,*argv);
							// "Unknown switch, %s"
				}
			}
		else if((*argv)[0] == '+') {	// +<line>[:<char>].
			if(gotoswitch(*argv,&gline,&gchar) != SUCCESS)
				return rc.status;
			helpflag = false;
			gotoflag = true;
			}
		else if((*argv)[0] == '@') {
			helpflag = false;

			// Process startup macro.
			if(startup(*argv + 1,false,false) != SUCCESS)
				return rc.status;
			}
		else {
dofile:
			// Process an input file.
			helpflag = false;

			// Set up a buffer for this file and set it to inactive.
			if(stdinflag > 0) {
				if(bfind(buffer1,CRBCREATE,0,&bufp,NULL) != SUCCESS)
					return rc.status;
				}
			else if(bfind(*argv,CRBCREATE | CRBUNIQ | CRBFILE,0,&bufp,NULL) != SUCCESS)
				return rc.status;
			if(stdinflag > 0)		// If using standard input ...
				stdinflag = -1;		// disable the flag so it can't be used again.
			else if(setfname(bufp,*argv) != SUCCESS)
				return rc.status;
			bufp->b_flags &= ~BFACTIVE;
			if(firstbp == NULL)
				firstbp = bufp;

			// Set the modes appropriately.
			if(rdonlyflag)
				bufp->b_modes |= MDRDONLY;
			}
		++argv;
		}

	// Done processing arguments.

	// Select initial buffer.
	if(startbuf(firstbp,stdinflag) != SUCCESS)
		return rc.status;
	if(strcmp(curbp->b_bname,buffer1) != 0)
		helpflag = false;

	// Process startup gotos and searches.
	if(gotoflag) {
		if(searchflag)
			return rcset(FATALERROR,0,text101);
				// "Cannot search and goto at the same time!"
		if(gotoLine(vsinkp,gline) == SUCCESS)
			(void) forwch(gchar - 1);
		}
	else if(searchflag) {
		if(huntForw(vsinkp,1) != SUCCESS) {
			(void) update(false);
			return rc.status;
			}
		}

	// Build help message if appropriate.
	*helpmsg = '\0';
	if(helpflag) {
		KeyDesc *kdp;
		char hcmd[16],qcmd[16],work[16];
		struct {
			char *ck;
			FABPtr fab;
			} *cip,cmdinfo[] = {
				{hcmd,{PTRCMD,cftab + cf_help}},
				{qcmd,{PTRCMD,cftab + cf_exit}},
				{NULL,{PTRCMD,NULL}}};

		cip = cmdinfo;
		do {
			*cip->ck = '\0';
			if((kdp = getpentry(&cip->fab)) != NULL) {
				ectos(kdp->k_code,cip->ck,true);

				// Change "M-" to "ESC-" and "[CHX]-" to "^[CHX]" (for the EMacs beginner's benefit).
				if(cip->ck[1] == '-') {
					cip->ck[1] = ' ';
					strcpy(work,cip->ck + 1);
					if(cip->ck[0] == 'M') {
						strcpy(cip->ck,"ESC");
						strcpy(cip->ck + 3,work);
						}
					else {
						cip->ck[1] = cip->ck[0];
						cip->ck[0] = '^';
						strcpy(cip->ck + 2,work);
						}
					}
				}
			} while((++cip)->ck != NULL);

		if(hcmd[0] != '\0' && qcmd[0] != '\0')
			sprintf(helpmsg,text41,hcmd,qcmd);
				// "Enter \"%s\" for help, \"%s\" to quit"
		}

	return rc.status;
	}

// Prepare to insert one or more characters at point.  Delete characters first if in replace or overwrite mode and not at end
// of line.  Assume n > 0.  Return status.
int overprep(int n) {

	if(curbp->b_modes & MDGRP_OVER) {
		int count = 0;
		Dot *dotp = &curwp->w_face.wf_dot;
		do {
			if(dotp->off == lused(dotp->lnp))
				break;

			// Delete char if we are in replace mode, or char is not a tab, or we are at a tab stop.
			if((curbp->b_modes & MDREPL) || (lgetc(dotp->lnp,dotp->off) != '\t' ||
			  (getccol() + count) % htabsize == (htabsize - 1)))
				if(ldelete(1L,false) != SUCCESS)
					break;
			++count;
			} while(--n > 0);
		}
	return rc.status;
	}

// This is the general interactive command execution routine.  It handles the fake binding of all the printable keys to
// "self-insert".  It also clears out the "thisflag" word and arranges to move it to "lastflag" so that the next command can
// look at it.  "c" is the key to execute with argument "n".  Return status.
static int execute(int c,KeyDesc *kdp,int n) {
	char strbuf[16];

	// If the keystroke is bound ...
	if(kdp != NULL) {

		// Don't reset the command-execution flags on a prefix.
		if(kdp->k_fab.p_type == PTRCMD && (kdp->k_fab.u.p_cfp->cf_flags & CFPREFIX)) {
			(void) execkey(kdp,n);
			return rc.status;
			}

		kentry.thisflag = 0;
		(void) execkey(kdp,n);

		// Remove end-macro key sequence from keyboard macro, if applicable, and check for empty buffer.
		if(rc.status == SUCCESS && kdp->k_fab.u.p_cfp == cftab + cf_endKeyMacro) {
			int len = (kdp->k_code & KEYSEQ) ? 2 : 1;
			kmacro.km_slotp -= len;
			kmacro.km_endp -= len;
			if(kmacro.km_endp == kmacro.km_buf) {
				kmacro.km_endp = NULL;
				(void) rcset(SUCCESS,RCFORCE,text200);
						// "No keyboard macro defined"
				}
			}

		goto acheck;
		}

	// Keystroke not bound ... attempt self-insert.
	if(c >= 0x20 && c <= 0xff) {
		int lmode;

		if(n == INT_MIN)
			n = 1;
		else if(n <= 0) {
			kentry.lastflag = 0;
			return rcset(FAILURE,0,NULL);
			}
		if(allowedit(true) != SUCCESS)		// Don't allow if in read-only mode.
			return rc.status;
		kentry.thisflag = 0;			// For the future.

		// In replace or overwrite mode?
		if(overprep(n) != SUCCESS)
			return rc.status;

		// Do the appropriate insertion:
		if(!(lmode = curbp->b_modes & MDGRP_LANG) || (curbp->b_modes & MDGRP_OVER))
			goto plain;

			// C, Perl, Ruby, Shell right brace.
		if((c == '}' && (lmode & MDMEMACS) == 0) ||
			// Ruby "end".
		 (c == 'd' && (lmode & MDRUBY) != 0) ||
		 	// C, Perl, Ruby, or Shell "else", MightEMacs !else, Ruby "rescue", or Shell "done".
		 (c == 'e') ||
		 	// Perl or Ruby "elsif", MightEMacs !elsif or !endif, or Shell "elif".
		 (c == 'f' && (lmode & MDC) == 0) ||
		 	// MightEMacs !endmacro or Shell "do".
		 (c == 'o' && (lmode & (MDMEMACS | MDSHELL)) != 0) ||
		 	// MightEMacs !endloop.
		 (c == 'p' && (lmode & MDMEMACS) != 0) ||
		 	// Shell "fi" or "esac".
		 ((c == 'i' || c == 'c') && (lmode & MDSHELL) != 0) ||
		 	// Ruby or Shell "then".
		 (c == 'n' && (lmode & (MDRUBY | MDSHELL)) != 0))
		 	(void) insrfence(c);
		else if((c == '#' && (lmode & MDC) != 0) || (c == '=' && (lmode & MDRUBY) != 0))
			(void) inspre(c);
		else
plain:
			(void) linsert(n,c);

		if(rc.status == SUCCESS) {

			// Check for language-mode fence matching.
			if(lmode != 0 && (c == '}' || c == ')' || c == ']'))
				fmatch(c);
acheck:
			// Check auto-save mode and save the file if needed.
			if((modetab[MDR_GLOBAL].flags & MDASAVE) && --curbp->b_acount == 0) {

				// If current buffer has no associated filename, just reset the counter.
				if(curbp->b_fname == NULL)
					curbp->b_acount = gasave;
				else {
					Value vsink;		// For throw-away return value, if any.
#if VDebug
					vinit(&vsink,"execute");
#else
					vinit(&vsink);
#endif
					kentry.lastflag = kentry.thisflag;
					kentry.thisflag = 0;
					if(update(false) < FAILURE || feval(&vsink,INT_MIN,cftab + cf_saveFile) < FAILURE)
						return rc.status;
					vnull(&vsink);
					}
				}
			}
		kentry.lastflag = kentry.thisflag;
		return rc.status;
		}

	// Unknown key.
	(void) TTbeep();
	kentry.lastflag = 0;					// Fake last flags.
	return rcset(FAILURE,0,text14,ectos(c,strbuf,true));	// Complain.
		// "%s not bound"
	}

#if 0
// Get one argument from pre-key hook return value.  Return status.
static int getprearg(Value *vp,char **strpp,int argnum) {
	int status = parsetok(vp,strpp,'\t');
	if(status == NOTFOUND) {
		if(argnum != 3)
			(void) rcset(FAILURE,0,text327);
					// "Invalid hook return value"
		}
	else if(status == SUCCESS && argnum == 1)
		(void) intval(vp);
	return rc.status;
	}

// Check pre-key hook return value -- two arguments separated by a tab: (1), (updated) n arg; and (2), boolean "execute key".
// Return status.
static int chkprekey(Value *rp,int *nptr,bool *xkeyp) {
	Value *vp;

	if(strval(rp)) {
		if(vnew(&vp,false) != 0)
			return vrcset();

		// Parse first argument.
		char *strp = rp->v_strp;
		if(getprearg(vp,&strp,1) == SUCCESS) {
			*nptr = vp->u.v_int;

			// Parse second argument.
			if(getprearg(vp,&strp,2) == SUCCESS) {
				*xkeyp = vistrue(vp);

				// Extra arguments?
				if(getprearg(vp,&strp,3) == SUCCESS)
					return rc.status;
				}
			}
		}

	// No go ... disable hook and return error.
	return dishook(hooktab + HKPREKEY);
	}
#endif

// Interactive command-processing routine.  Read key sequences and process them.  Return if program exit.
static int editloop(void) {
	int count;
	int c;			// Command character.
	int n;			// Numeric argument.
	ushort oldflag;		// Old last flag value.
	KeyDesc *kdp;
	Value rtn;		// Return value from pre-key hook.
	RtnCode lastrc;		// Return code from last key executed.
	char lastkstr[16];	// Last key in string form (supplied to command hooks).
#if VDebug
	static char myname[] = "editloop";

	vinit(&rtn,myname);
	vinit(&lastrc.msg,myname);
#else
	vinit(&rtn);
	vinit(&lastrc.msg);
#endif
	opflags &= ~OPSTARTUP;

	// If $lastKeySeq was set by a startup macro, simulate it being entered as the first key.
	if(kentry.uselast)
		goto jumpstart;
	goto chkmsg;

	// Do forever.
	for(;;) {
		(void) rcclear();
		vgarbpop(NULL);				// Throw out all the garbage.

		// Update position on current modeline?
		if(modetab[MDR_GLOBAL].flags & (MDLINE | MDCOL))
#if TYPEAH
			if(typahead(&count) != SUCCESS)
				break;
			if(count == 0)
#endif
				upmode(curbp);		// Flag all windows.

		// Fix up the screen.
		if(update(false) <= MINEXIT)
			break;

		// Any message from last loop to display?
		if(!visnull(&lastrc.msg)) {
			savecursor();
			c = (lastrc.status == SUCCESS && !(lastrc.flags & RCNOWRAP)) ? (MLHOME | MLFORCE | MLWRAP) :
			 (MLHOME | MLFORCE);
			mlputs(c,lastrc.msg.v_strp,vz_show);
			if(restorecursor() <= MINEXIT)
				break;
			}

		// Get the next key from the keyboard or $lastKeySeq.
		modetab[MDR_GLOBAL].flags |= (MDKECHO | MDMSG);
		if(kentry.uselast) {
jumpstart:
			kdp = getbind(c = kentry.lastkseq);
			kentry.uselast = false;
			}
		else {
			if(getkseq(&c,&kdp) <= MINEXIT)
				break;
			if(rc.status != SUCCESS)
				goto fail;
			}

		// If there is something on the message line, clear it.
		mlerase(MLFORCE);

		// Set default.
		n = INT_MIN;

		// Do universal/negative repeat argument processing.  ^U sequence is 2, 0, 3, 4, ...  ^_ is -1, -2, ...
		// Use the following decision table to determine what action to take as each key (^U, ^_, minus sign, digit,
		// or other) is entered in a given state.  Actions are:
		//	n = <value>	Set n to <value>
		//	-> state n	Change to state n
		//	begin n		Begin building number n
		//	continue n	Continue building number n
		//	insert it	Break out of processing loop and execute '-' key (self insert)
		//	execute it	Break out of processing loop and execute other key
		// State 0 is the point prior to entering ^U or ^_ for the first time:
		// =====	============	============	============	============	============
		// State	^U		^_		-		digit		other key
		// =====	============	============	============	============	============
		// 0		n = 2		n = -1
		// 		-> state 1	-> state 3
		// -----	------------	------------	------------	------------	------------
		// 1		n = 0		--n		n = -1		begin n		execute it
		// 		-> state 2	-> state 5	-> state 3	-> state 4
		// -----	------------	------------	------------	------------	------------
		// 2		n = 3		--n		n = -1		begin n		execute it
		// 		-> state 5	-> state 5	-> state 3	-> state 4
		// -----	------------	------------	------------	------------	------------
		// 3		++n		--n		insert it	begin n		execute it
		// 		-> state 5	-> state 5			-> state 4
		// -----	------------	------------	------------	------------	------------
		// 4		++n		--n		insert it	continue n	execute it
		// 		-> state 5	-> state 5
		// -----	------------	------------	------------	------------	------------
		// 5		++n		--n		insert it	insert it	execute it
		// -----	------------	------------	------------	------------	------------

		if(c == ckeys.unarg || c == ckeys.negarg) {	// universalArg or negativeArg command.
			int neg;				// n negative?
			int state;				// Numeric argument processing state.
			bool digit;				// Digit entered?

			if(c == ckeys.negarg) {
				neg = true;
				n = 1;				// Begin at -1.
				state = 3;			// No digit entered yet.
				}
			else {
				neg = false;
				n = 2;				// Begin at +2.
				state = 1;			// No sign or digit entered yet.
				}

			// Loop until numeric argument completed.
			for(;;) {
				// Display current value.
				if(mlprintf(MLHOME | MLFORCE,text209,neg ? -n : n) != SUCCESS)
							// "Arg: %d"
					break;

				// Get next key.
				if(getkey(&c) != SUCCESS)
					goto retn;
				digit = (c >= '0' && c <= '9');

				// Take action, depending on current state.
				switch(state) {
					case 1:
						if(c == ckeys.unarg) {
							n = 0;
							state = 2;
							continue;
							}
						if(c == ckeys.negarg)
							goto decr5;
						if(c == '-')
							goto neg3;
						if(digit)
							goto nbegin;
						break;
					case 2:
						if(c == ckeys.unarg) {
							n = 3;
							state = 5;
							continue;
							}
						if(c == ckeys.negarg)
							goto decr5;
						if(c == '-') {
neg3:
							neg = true;
							n = 1;
							state = 3;
							continue;
							}
						if(digit)
							goto nbegin;
						break;
					case 3:
					case 4:
						if(c == ckeys.unarg)
							goto incr5;
						if(c == ckeys.negarg)
							goto decr5;
						if(digit) {
							if(state == 3) {
nbegin:
								n = c - '0';
								state = 4;
								}
							else
								n = n * 10 + (c - '0');
							continue;
							}
						break;
					case 5:
						if(c == ckeys.unarg) {
incr5:
							if(!neg)
								++n;
							else if(--n == 0)
								neg = false;
							state = 5;
							continue;
							}
						if(c == ckeys.negarg) {
decr5:
							if(neg)
								++n;
							else if(--n < 0) {
								n = 1;
								neg = true;
								}
							state = 5;
							continue;
							}
						}

				// "n" has been finalized.  Process c as a command.
				tungetc(c);
				if(getkseq(&c,&kdp) != SUCCESS)
					goto retn;
				break;
				}

			// Change sign of n if needed.
			if(neg)
				n = -n;

			// Clear the message line.
			mlerase(MLFORCE);
			}

		// Execute the user-assigned pre-key hook with n arg, preserving lastflag and capturing return value.
		oldflag = kentry.lastflag;
		if(exechook(&rtn,n,hooktab + HKPREKEY,1,ectos(kentry.lastkseq,lastkstr,false)) <= MINEXIT)
			break;
		kentry.lastflag = oldflag;

	 	// If no pre-key hook error (or no hook) ...
		if(rc.status == SUCCESS) {

			// Get updated key from pre-key hook, if any.
			if(kentry.uselast) {
				kdp = getbind(c = kentry.lastkseq);
				kentry.uselast = false;
				}

			// Execute key if no pre-key hook or it returned false ("don't skip execution").
			if(hooktab[HKPREKEY].h_fab.p_type == PTRNUL || !vistrue(&rtn))
				if(execute(c,kdp,n) <= MINEXIT)
					break;

		 	// If no key execution error ...
			if(rc.status == SUCCESS) {

				// execute the user-assigned post-key hook with current n arg, preserving thisflag.
				oldflag = kentry.thisflag;
				if(exechook(NULL,n,hooktab + HKPOSTKEY,1,lastkstr) <= MINEXIT)
					break;
				kentry.thisflag = oldflag;
				}
			}

		// Process return code and message from execution of key hook or entered key.
		if(rc.status != SUCCESS) {
fail:
			// Key retrieval/execution failed or returned false.  Kill off any running keyboard macro ...
			if(kmacro.km_state == KMPLAY) {
				kmacro.km_n = 0;
				kmacro.km_state = KMSTOP;
				}

			// force terminal input next time around ...
			kentry.uselast = false;
			}
chkmsg:
		// Any message returned?
		if(visnull(&rc.msg))
			vnull(&lastrc.msg);			// No, clear last.
		else if(rccpy(&lastrc,&rc) <= MINEXIT)		// Yes, save message and code.
			break;
		}
retn:
	return rc.status;
	}

// Begin MightEMacs.  Basic tasks are:
// 1. Initialze terminal and data structures.
// 2. Process startup file(s) if found.
// 3. Do command-line processing.
// 4. If no errors, enter user-key-entry loop.
// 5. At loop exit, display any error message and return proper exit code to OS.
int main(int argc,char *argv[]) {
	bool dostart;
	char *path;
	Buffer *bufp;
	char helpmsg[50];

	// Do global initializations.
	randseed = time(NULL);
	mypid = (unsigned) getpid();
#if MMDEBUG & DEBUG_LOGFILE
	logfile = fopen(LOGFILE,"w");
#endif
	// Begin using rc (RtnCode): do pre-edit-loop stuff, but use the same return-code mechanism as command calls.
	if(edinit0() == SUCCESS &&		// Initialize the return value structures.
	 scancmdline(argc,argv,&dostart) == SUCCESS &&	// Check for informational and -n switches.
	 edinit1() == SUCCESS &&		// Initialize the core data structures.
	 vtinit() == SUCCESS &&			// Initialize the terminal.
	 edinit2() == SUCCESS) {		// Initialize buffers, windows, screens.
		initchars();			// Initialize character-set structures.

		// At this point, we are fully operational (ready to process commands).  Set execpath.
		if(setpath((path = getenv(MMPATH_NAME)) == NULL ? MMPATH_DEFAULT : path,false) != SUCCESS)
			goto out;

		// Run site and user startup files and check for macro error.
		if(dostart && (startup(SITE_STARTUP,false,true) != SUCCESS || startup(USER_STARTUP,true,true) != SUCCESS))
			goto out;

		// Process the command line.
		if(docmdline(argc,argv,helpmsg) == SUCCESS) {

			// Display help message if appropriate.
			if(*helpmsg != '\0')
				mlputs(MLHOME | MLFORCE | MLWRAP,helpmsg,vz_show);

			// Enter user-command loop.
			curwp->w_flags |= WFMODE;
			(void) editloop();
			}
		}
out:
	// Preserve the return code, close the terminal (ignoring any error), and return to line mode.
	if(rccpy(&scriptrc,&rc) == PANIC)
		(void) rcset(PANIC,0,NULL);	// No return.
	(void) rcclear();
	(void) vttidy(true);
#if MMDEBUG & DEBUG_LOGFILE
	(void) fclose(logfile);
#endif
	// Exit MightEMacs.
	if(scriptrc.status == USEREXIT || scriptrc.status == SCRIPTEXIT) {

		// Display names (in line mode) of any files saved via quickExit().
		bufp = bheadp;
		do {
			if(bufp->b_flags & BFQSAVE)
				fprintf(stderr,text193,bufp->b_fname);
						// "Saved file \"%s\"\n"
			} while((bufp = bufp->b_nextp) != NULL);

		// Display message, if any.
		if(!visnull(&scriptrc.msg)) {
			StrList msg;
			char *msgp = scriptrc.msg.v_strp;

			if(vopen(&msg,NULL,false) == 0 && vstrlit(&msg,msgp,0) == 0 && vclose(&msg) == 0)
				msgp = msg.sl_vp->v_strp;
			else
				scriptrc.status = SCRIPTEXIT;
			fprintf(stderr,"%s\n",msgp);
			}

		return scriptrc.status == SCRIPTEXIT ? -1 : 0;
		}

	// Error or help exit.
	if(scriptrc.status == HELPEXIT)
		fprintf(stderr,"%s\n",scriptrc.clhelptext);
	else {
		char strbuf[NWORK];

		sprintf(strbuf,"%s:",text189);
				// "Abort"
		if(strncmp(scriptrc.msg.v_strp,strbuf,strlen(strbuf)) != 0)
			fprintf(stderr,"%s: ",text0);
					// "Error"
		if(scriptrc.status == OSERROR)
			fprintf(stderr,"%s, %s\n",strerror(errno),scriptrc.msg.v_strp);
		else if(visnull(&scriptrc.msg))
			fprintf(stderr,text253,scriptrc.status);
				// "(return status %d)\n"
		else {
			StrList msg;
			char *msgp = scriptrc.msg.v_strp;

			if(vopen(&msg,NULL,false) == 0 && vstrlit(&msg,msgp,0) == 0 && vclose(&msg) == 0)
				msgp = msg.sl_vp->v_strp;
			fprintf(stderr,"%s\n",msgp);
			}
		}
	return -1;
	}

// Give me some help!  Execute help hook.
int help(Value *rp,int n) {
	HookRec *hrp = hooktab + HKHELP;

	return (hrp->h_fab.p_type == PTRNUL) ? rcset(FAILURE,0,text246,hrp->h_name) : exechook(rp,n,hrp,0);
							// "Help hook not set"
	}

// Loop through the list of buffers (ignoring hidden ones) and return true if any are changed; otherwise, false.
static int dirtybuf(void) {
	Buffer *bufp;

	bufp = bheadp;
	do {
		if((bufp->b_flags & BFHIDDEN) == 0 && (bufp->b_flags & BFCHGD) != 0)
			return true;
		} while((bufp = bufp->b_nextp) != NULL);
	return false;
	}

// Quit command.  If n != 0 and not default, always quit; otherwise confirm if a buffer has been changed and not written out.
// If n < 0 and not default, force error return to OS.  Include message if script mode and one is available.
int quit(Value *rp,int n) {
	char *msgp = NULL;
	int status = USEREXIT;		// Default return status.
	bool forceclean = false;

	if(n == INT_MIN)
		n = 0;

	// Not argument-force and dirty buffer?
	if(n == 0 && dirtybuf()) {
		bool yep;

		// Have changed buffer(s) ... user okay with that?
		if(mlyesno(text104,&yep) != SUCCESS)
				// "Modified buffers exist.  Leave anyway"
			return rc.status;
		if(yep)
			// Force clean exit.
			forceclean = true;
		else
			// User changed his mind ... don't exit, no error.
			status = SUCCESS;
		mlerase(0);
		}

	// Script mode?
	if(opflags & OPSCRIPT) {
		Value *vp;

		// Get return message, if any, and save it if exiting.
		if(buildmsg(&vp,NULL) == SUCCESS && status != SUCCESS) {
			if(rcclear() == SUCCESS && !visnull(vp) && !vistfn(vp,VNIL))
				msgp = vp->v_strp;
			if(!forceclean && dirtybuf())
				status = SCRIPTEXIT;	// Forced exit from a script with dirty buffer(s).
			}
		}

	// Force error exit if negative n.
	if(n < 0)
		status = SCRIPTEXIT;

	return rcset(status,0,msgp);
	}

// Write a string to a buffer, centered in the current terminal width.  Return status.
static int center(Buffer *bufp,char *srcp) {
	int pad = (term.t_ncol - strlen(srcp)) / 2;
	char *dstp,wkbuf[term.t_ncol + 1];

	dstp = (pad > 0) ? dupchr(wkbuf,' ',pad) : wkbuf;
	stplcpy(dstp,srcp,wkbuf + sizeof(wkbuf) - dstp);
	(void) bappend(bufp,wkbuf);

	return rc.status;
	}

// Build and pop up a buffer containing "about the editor" information.  Render buffer and return status.
int aboutMM(Value *rp,int n) {
	Buffer *bufp;
	Value *vp;
	StrList line;
	char *strp;
	int i,maxlab;
	static struct liminfo {
		char *label;
		int value;
		} limits[] = {
			{ALIT_maxCols,TT_MAXCOLS},
			{ALIT_maxRows,TT_MAXROWS},
			{ALIT_maxIfNest,IFNESTMAX},
			{ALIT_maxTab,MAXTAB},
			{ALIT_maxPathname,MaxPathname},
			{ALIT_maxBufName,NBUFN},
			{ALIT_maxUserVar,NVSIZE},
			{ALIT_maxTermInp,NTERMINP},
			{ALIT_maxPat,NPAT},
			{ALIT_maxKMacro,NKBDM},
			{ALIT_maxREGroups,MAXGROUPS},
			{ALIT_killRingSize,NRING},
			{ALIT_maxMarks,NMARKS},
			{ALIT_typeAhead,-(TYPEAH + 1)},
			{NULL,0}
			};
	struct liminfo *limp;

	// Get a buffer.
	if(sysbuf(text6,&bufp) != SUCCESS)
			// "About"
		return rc.status;

	// Get a value object.
	if(vnew(&vp,false) != 0)
		return vrcset();

	// Construct the "About MightEMacs" lines.
	if(bappend(bufp,"\r") != SUCCESS)				// Vertical space.
		return rc.status;
	if(vopen(&line,vp,false) != 0)					// Expand program name ...
		return vrcset();
	strp = myself;
	do {
		if(vputc(*strp++,&line) != 0 || vputc(' ',&line) != 0)
			return vrcset();
		} while(*strp != '\0');
	if(vunputc(&line) != 0 || vclose(&line) != 0)
		return vrcset();
	if(center(bufp,vp->v_strp) != SUCCESS)				// and center it.
		return rc.status;
	if(bappend(bufp,"") != SUCCESS)					// Blank line.
		return rc.status;
	if(vopen(&line,vp,false) != 0 ||				// Build "version" line ...
	 vputf(&line,"%s %s",text185,version) != 0 || vclose(&line) != 0)
			// "Version"
		return vrcset();
	if(center(bufp,vp->v_strp) != SUCCESS)				// and center it.
		return rc.status;
	if(bappend(bufp,"") != SUCCESS)					// Blank line.
		return rc.status;
	if(center(bufp,ALIT_author) != SUCCESS)				// Copyright.
			// "(c) Copyright 2015 Richard W. Marinelli <italian389@yahoo.com>"
		return rc.status;

	// Construct the program limits' lines.
	if(bappend(bufp,"\r") != SUCCESS)				// Blank lines.
		return rc.status;
	if(center(bufp,ALIT_buildInfo) != SUCCESS)			// Limits' header.
		// "Build Information"
		return rc.status;
	if(bappend(bufp,"") != SUCCESS)					// Blank line.
		return rc.status;

	// Find longest limits' label and set i to indentation required to center it and its value.
	limp = limits;
	maxlab = 0;
	do {
		if((i = strlen(limp->label)) > maxlab)
			maxlab = i;
		} while((++limp)->label != NULL);
	if((i = (term.t_ncol - (maxlab + 10)) / 2) < 0)
		i = 0;

	// Loop through limit values.
	{char wkbuf[i + maxlab + 12];

	dupchr(wkbuf,' ',i);
	limp = limits;
	do {
		// Store label, padding, and value.
		strcpy(stpcpy(wkbuf + i,limp->label),":");
		strp = pad(wkbuf + i,maxlab + 1);
		if(limp->value < 0)
			sprintf(strp,"%9s",limp->value == -1 ? "No" : "Yes");
		else
			sprintf(strp,"%9d",limp->value);

		// Add line to buffer.
		if(bappend(bufp,wkbuf) != SUCCESS)
			return rc.status;
		} while((++limp)->label != NULL);
	}

	// Write platform, credit, and license blurb.
	if(bappend(bufp,"") != SUCCESS)					// Blank line.
		return rc.status;
	if(vopen(&line,vp,false) != 0 ||
	 vputf(&line,"[%s%s",myself,ALIT_footer1) != 0 || vclose(&line) != 0)
			// " runs on Unix and Linux platforms and is licensed under the"
		return vrcset();
	if(center(bufp,vp->v_strp) != SUCCESS || center(bufp,ALIT_footer2) != SUCCESS || center(bufp,ALIT_footer3) != SUCCESS)
			// "Creative Commons Attribution-NonCommercial 4.0 International License which"
			// "can be viewed at http://creativecommons.org/licenses/by-nc/4.0/legalcode]"
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,bufp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}
