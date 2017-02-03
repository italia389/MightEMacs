// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// main.c	Entry point for the MightEMacs text editor.
//
// This file contains the main (top level) routine and some keyboard processing code.
//
// Programming Notes:
// 1. After program initialization, the core operation of the editor is as follows: keys are read in editloop(), commands or
// macros are called with a numeric argument, and a status code is returned.  If no numeric argument is specified by the user,
// INT_MIN is used as the default (and many commands will have a default behavior in that case).  The called routines typically
// call (many) other routines, which may include hook routines.  Most of these functions return a status code as well.  The
// return code is usually Success or Failure; however, it can be more severe (see edef.h for a list), such as UserAbort or
// FatalError.  Any code worse than UserAbort will cause program termination.  The most severe return code from the most distant
// function in the call chain will ultimately be returned to editloop(), where it will be dealt with; that is, a message will be
// displayed if applicable before beginning the next loop.  This means that a more severe return code and message will always
// override a less severe one (which is enforced by the rcset() function).
//
// 2. There is a special return code, NotFound, which may be returned directly from a function, bypassing rcset().  This code
// typically indicates an item was not found or the end of a list was reached, and it requires special attention.  It is the
// responsibility of the calling function to decide what to do with it.  The functions that may return NotFound are:
//
// Function(s)							Returns NotFound, bypassing rcset() when...
// ---------------------------------------------------------	---------------------------------------------------------------
// backChar(), forwChar(), backLine(), forwLine(), ldelete()	Move or delete is out of buffer.
// getsym(), parsetok()						Token not found and no error.
// bcomplete()							Buffer not found, interactive mode, and operation is OpQuery.
// startup()							Script file not found.
// mcscan(), scan()						Search string not found.
// ereaddir()							No entries left.

#include "os.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

// Local definitions.
enum h_type {h_copyright,h_version,h_usage,h_help};

// Make selected global definitions local.
#define DATAmain
#define DATAcmd
#include "std.h"
#include "lang.h"
#include "bind.h"
#include "exec.h"
#include "file.h"
#include "search.h"
#include "var.h"
#include "main.h"
#include "cmd.h"

// Clear return code message, set return code status to Success, and return it.
int rcclear(void) {

	dsetnull(&rc.msg);
	rc.flags = 0;
	return rc.status = Success;
	}

// Copy one return code object to another.  Return status.
static int rccpy(RtnCode *destp,RtnCode *srcp) {

	destp->status = srcp->status;
	destp->flags = srcp->flags;
	destp->helpText = srcp->helpText;
#if MMDebug & Debug_Datum
	if(datcpy(&destp->msg,&srcp->msg) != 0)
		(void) drcset();
	fprintf(logfile,"rccpy(): copied from (%hu) '%s' [%d] to (%hu) '%s' [%d]\n",
	 srcp->msg.d_type,srcp->msg.d_str,srcp->status,destp->msg.d_type,destp->msg.d_str,destp->status);
	return rc.status;
#else
	return datcpy(&destp->msg,&srcp->msg) != 0 ? drcset() : rc.status;
#endif
	}

// Write text into the return code message (unless RCKeepMsg flag) and return most severe status.  If status is Success, RCForce
// flag is not set, and global 'msg' mode is not set, do nothing; otherwise, if status is the same, keep existing message (do
// nothing) unless it's a force or existing message is null.
int rcset(int status,ushort flags,char *fmt,...) {
	va_list ap;

	// Check status.  If this status is not more severe, or Success, not force, and not displaying messages, return old one.
	if(status > rc.status || (!(flags & RCForce) && ((status == Success && !(modetab[MdRec_Global].flags & MdMsg)) || 
	 (status == rc.status && !disnull(&rc.msg)))))
		return rc.status;

	// Save message (if RCKeepMsg flag is not set) and new status.
	if(status == HelpExit)
		rc.helpText = fmt;
	else if(fmt != NULL) {
		if(!(flags & RCKeepMsg) || disnull(&rc.msg)) {
			char *str;

			if(status != Panic) {
				va_start(ap,fmt);
				(void) vasprintf(&str,fmt,ap);
				va_end(ap);
				}

			// If Panic serverity or vasprintf() failed, panic!  That is, we're most likely out of memory, so bail
			// out with "great expediency".
			if(status == Panic || str == NULL) {
				(void) vttidy(true);
				fprintf(stderr,"%s: ",text189);
						// "Abort"
				if(status == Panic) {
					va_start(ap,fmt);
					vfprintf(stderr,fmt,ap);
					va_end(ap);
					}
				else
					fprintf(stderr,text94,"rcset");
							// "%s(): Out of memory!"
				fputc('\n',stderr);
				exit(-1);
				}
			dsetmemstr(str,&rc.msg);
			}
		}
	else if(flags & RCForce)
		dsetnull(&rc.msg);
	rc.flags = flags;
#if MMDebug & Debug_Datum
	fprintf(logfile,"rcset(%d,%u,...): set msg '%s', status %d\n",status,flags,rc.msg.d_str,status);
#endif
	return (rc.status = status);
	}

// Save current return code and message for $ReturnMsg.  Return status.
int rcsave(void) {

	// Copy to scriptrc via dviz().
	if(dviz(rc.msg.d_str,0,VBaseDef,&scriptrc.msg) == 0)
		scriptrc.status = rc.status;
	return rc.status;
	}

// Concatenate any function arguments and save prefix and result in *datpp.  Return status.
static int buildmsg(Datum **datpp,char *prefix) {
	DStrFab msg;
	Datum *datp;

	if(dnewtrk(&datp) != 0 || dnewtrk(datpp) != 0)
		return drcset();
	if(catargs(datp,0,NULL,0) != Success || (prefix == NULL && disnn(datp)))
		return rc.status;
	if(dopenwith(&msg,*datpp,false) != 0 || (prefix != NULL && dputs(prefix,&msg) != 0))
		return drcset();
	if(!disnn(datp))
		if((!disempty(&msg) && dputs(": ",&msg) != 0) || dputs(datp->d_str,&msg) != 0)
			return drcset();
	return dclose(&msg,sf_string) != 0 ? drcset() : rc.status;
	}

// Increase size of keyboard macro buffer.
int growKeyMacro(void) {
	uint n = kmacro.km_size;
	if((kmacro.km_buf = (ushort *) realloc((void *) kmacro.km_buf,(kmacro.km_size += NKbdChunk) * sizeof(ushort))) == NULL)
		return rcset(Panic,0,text94,"growKeyMacro");
			// "%s(): Out of memory!"
	kmacro.km_slotp = kmacro.km_buf + n;
	return rc.status;
	}

// Clear keyboard macro and set to STOP state if "stop" is true.
void clearKeyMacro(bool stop) {

	kmacro.km_slotp = kmacro.km_buf;
	kmacro.km_endp = NULL;
	if(stop) {
		if(kmacro.km_state == KMRecord)
			curwp->w_flags |= WFMode;
		kmacro.km_state = KMStop;
		}
	}

// Begin recording a keyboard macro.  Error if not in the KMStopped state.  Set up variables and return status.
int beginKeyMacro(Datum *rp,int n,Datum **argpp) {

	clearKeyMacro(false);
	if(kmacro.km_state != KMStop) {
		kmacro.km_state = KMStop;
		return rcset(Failure,0,text105);
			// "Keyboard macro already active, cancelled"
		}
	kmacro.km_state = KMRecord;
	curwp->w_flags |= WFMode;
	return rcset(Success,0,text106);
		// "Begin macro"
	}

// End keyboard macro recording.  Check for the same limit conditions as the above routine.  Set up the variables and return
// status.
int endKeyMacro(Datum *rp,int n,Datum **argpp) {

	if(kmacro.km_state == KMStop)
		return rcset(Failure,0,text107);
			// "Keyboard macro not active"

	// else in KMRecord state (KMPlay not possible).
	kmacro.km_endp = kmacro.km_slotp;
	kmacro.km_state = KMStop;
	curwp->w_flags |= WFMode;
	return rcset(Success,0,text108);
		// "End macro"
	}

// Enable execution of a keyboard macro n times (which will be subsequently run by the getkey() function).  Error if not in
// KMStopped state.  Set up the variables and return status.
int xeqKeyMacro(Datum *rp,int n,Datum **argpp) {

	if(kmacro.km_state != KMStop) {
		clearKeyMacro(true);
		return rcset(Failure,0,text105);
			// "Keyboard macro already active, cancelled"
		}
	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"
	if(kmacro.km_endp == NULL)
		return rcset(Failure,0,text200);
				// "No keyboard macro defined"

	kmacro.km_n = n;			// Remember how many times to execute.
	kmacro.km_state = KMPlay;		// Switch to "play" mode...
	kmacro.km_slotp = kmacro.km_buf;	// at the beginning.

	return rc.status;
	}

// Abort.  Beep the beeper.  Kill off any keyboard macro that is in progress.  Called by "abort" command and any user-input
// function if user presses abort key (usually ^G).  Set and return UserAbort status.
int abortinp(void) {

	if(TTbeep() != Success)
		return rc.status;
	if(kmacro.km_state == KMRecord)
		curwp->w_flags |= WFMode;
	kmacro.km_state = KMStop;
	return rcset(UserAbort,0,text8);
			// "Aborted"
	}

// Call abortinp() to abort processing and build optional exception message.
int abortOp(Datum *rp,int n,Datum **argpp) {

	(void) abortinp();

	// Short and sweet if called by user pressing a key.
	if(!(opflags & OpScript))
		return rc.status;

	// Called from elsewhere (a script)... build a message.
	(void) rcclear();
	Datum *datp;
	if(buildmsg(&datp,text189) == Success)
			// "Abort"
		(void) rcset(UserAbort,0,datp->d_str);

	return rc.status;
	}

// Concatenate arguments and force-set result as an informational (Success) return message.  Return true if default n or n >= 0;
// otherwise, false.  Suppress brackets [] if n <= 0.
int notice(Datum *rp,int n,Datum **argpp) {

	if(havesym(s_any,true)) {
		Datum *datp;

		if(buildmsg(&datp,NULL) == Success) {
			dsetbool(n == INT_MIN || n >= 0,rp);
			(void) rcset(Success,n <= 0 && n != INT_MIN ? RCNoWrap | RCForce : RCForce,datp->d_str);
			}
		}

	return rc.status;
	}

// Get name from the function table, given index.  Used in binary() calls.
static char *cfname(int index) {

	return cftab[index].cf_name;
	}

// Search the function table for given name and return pointer to corresponding entry, or NULL if not found.
CmdFunc *ffind(char *cname) {
	int nval;

	return ((nval = binary(cname,cfname,NFuncs)) == -1) ? NULL : cftab + nval;
	}

// Check current buffer state.  Return error if "edit" is true and in an executing buffer or in read-only mode.
int allowedit(bool edit) {

	if(edit) {
		if(curbp->b_modes & MdRdOnly) {
			(void) TTbeep();
			return rcset(Failure,0,text109,text58);
				// "%s is in read-only mode","Buffer"
			}
		if(curbp->b_mip != NULL && curbp->b_mip->mi_nexec > 0) {
			(void) TTbeep();
			return rcset(Failure,0,text284,text276,text248);
				// "Cannot %s %s buffer","modify","an executing"
			}
		}

	return rc.status;
	}

#if Color
// Copy a string with buffer overflow checking.  Update *destp to point to trailing null and return status.
int chkcpy(char **destp,char *src,size_t destlen,char *emsg) {
	size_t srclen;

	if((srclen = strlen(src)) >= destlen)
		return rcset(Failure,0,text503,emsg,strsamp(src,srclen,0));
			// "%s string '%s' too long"
	*destp = stpcpy(*destp,src);
	return rc.status;
	}
#endif

// Pad a string to indicated length and return pointer to terminating null.
char *pad(char *s,uint len) {
	char *str1,*str2;
	uint curlen;

	str1 = strchr(s,'\0');
	if((curlen = str1 - s) < len) {
		str2 = str1 + (len - curlen);
		do {
			*str1++ = ' ';
			} while(str1 < str2);
		*str1 = '\0';
		}
	return str1;
	}

// Return a sample of a string for error reporting, given pointer, length, and maximum length (excluding the trailing null).
// If maximum length is zero, use a small size derived from the terminal width.
char *strsamp(char *src,size_t srclen,size_t maxlen) {
	size_t len;

	// Set maxlen to maximum destination size.
	if(maxlen == 0)
		maxlen = sampbuf.smallsize;
	else if(maxlen >= sampbuf.buflen)
		maxlen = sampbuf.buflen - 1;

	// Now compute sample size...
	if((len = srclen) > maxlen)
		len = maxlen;
 
	// and return string sample.
	return strfit(sampbuf.buf,len,src,srclen);
	}

// Execute a command or macro bound to given key (not a hook).  Return status.
static int execkey(KeyDesc *kdp,int n) {
	CFABPtr *cfabp = &kdp->k_cfab;
	Datum *dsinkp;				// For throw-away return value, if any.

	if(dnewtrk(&dsinkp) != 0)
		return drcset();
	dsetnil(dsinkp);

	// Bound to a macro?
	if(cfabp->p_type == PtrMacro)
		(void) dobuf(dsinkp,n,cfabp->u.p_bufp,NULL,0);

	// Bound to a command.  Skip execution if n is zero and a repeat count; otherwise, perform "edit" check before
	// invoking it.
	else {
		CmdFunc *cfp = cfabp->u.p_cfp;
		if(n == 0 && (cfp->cf_aflags & CFNCount))
			return rc.status;
		if(allowedit(cfp->cf_aflags & CFEdit) == Success)
			(void) execCF(dsinkp,n,cfp,0,0);
		}

	// False return?
	if(rc.status == Success && dsinkp->d_type == dat_false)
		(void) rcset(Failure,RCKeepMsg,text300);
				// "False return"
	return rc.status;
	}

// Initialize the return message structures.  Return status.
static int edinit0(void) {

	// Initialize i var, return code, and numeric literals.
#if DCaller
	dinit(&ivar.format,"edinit0");
#else
	dinit(&ivar.format);
#endif
	if(dsetsubstr("%d",2,&ivar.format) != 0)
		return drcset();
#if DCaller
	dinit(&rc.msg,"edinit1");
	dinit(&scriptrc.msg,"edinit0");
#else
	dinit(&rc.msg);
	dinit(&scriptrc.msg);
#endif
	dsetnull(&rc.msg);
	dsetnull(&scriptrc.msg);
	return rc.status;
	}

// Create (permanent) Datum objects for group matches in given Match record.  Return status.
static int grpinit(Match *mtp) {
	GrpInfo *gip,*gipz;

	gipz = (gip = mtp->groups) + MaxGroups;
	do {
		if(dnew(&gip->matchp) != 0)
			return drcset();
		} while(++gip < gipz);
	return rc.status;
	}

// Initialize all of the core data structures.  Return status.
static int edinit1(void) {
	uint n;
	CoreKey *ckp;
	CFABPtr cfab;
	KeyDesc *kdp;
	CFAMRec *frp1,*frp2;
	CmdFunc *cfp;
	static char myname[] = "edinit1";
	struct {
		char *alias;
		CFABPtr cfab;
		} *ap,aliases[] = {
			{"cd",{PtrCmd,cftab + cf_chDir}},
			{"quit",{PtrCmd,cftab + cf_exit}},
			{"require",{PtrCmd,cftab + cf_xeqFile}},
			{NULL,{PtrNul,NULL}}};

	// Initialize keyboard macro.
	if(growKeyMacro() != Success)
		return rc.status;

	// Allocate permanent Datum objects for search tables.
	if(grpinit(&srch.m) != Success || grpinit(&rematch) != Success)
		return rc.status;
	lastMatch = srch.m.groups[0].matchp;		// Initially null.

	// Get space for SampBuf and MsgLine buffers.
	sampbuf.buflen = n = term.t_mcol + 1;
	sampbuf.smallsize = n / 4;

	if((ml.span = (char *) malloc(n + NTermInp)) == NULL || (sampbuf.buf = (char *) malloc(sampbuf.buflen)) == NULL)
		return rcset(Panic,0,text94,myname);
			// "%s(): Out of memory!"
	*sampbuf.buf = '\0';

	// Load all the key bindings.
	if(loadbind() != Success)
		return rc.status;

	// Initialize the kill ring.
	kringp = kring;
	do {
		kringp->kbufp = kringp->kbufh = NULL;
		kringp->kskip = 0;
		kringp->kused = KBlock;
		} while(++kringp < kringz);
	kringp = kring;

	// Initialize the key cache and the CFAM list with all command and function names.
	ckp = corekeys;
	cfab.p_type = PtrNul;		// Not used.
	cfp = cftab + NFuncs;
	frp2 = NULL;
	do {
		--cfp;

		// Allocate memory for a CFAM record.
		if((frp1 = (CFAMRec *) malloc(sizeof(CFAMRec))) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"
		frp1->fr_name = cfp->cf_name;
		frp1->fr_type = (cfp->cf_aflags & CFFunc) ? PtrFunc : (cfp->cf_aflags & CFHidden) ? PtrPseudo : PtrCmd;
		frp1->fr_nextp = frp2;
		frp2 = frp1;

		// Store frequently-used binding in key cache.
		if(cfp->cf_aflags & CFUniq) {
			cfab.u.p_cfp = cfp;
			ckp->ek = ((kdp = getpentry(&cfab)) == NULL) ? 0 : kdp->k_code;
			ckp->id = cfp - cftab;
			++ckp;
			}
		} while(cfp > cftab);
	frheadp = frp2;

	// Initialize the alias list.
	ap = aliases;
	do {
		if(afind(ap->alias,OpCreate,&ap->cfab,NULL) != Success)
			return rc.status;
		} while((++ap)->alias != NULL);

	// Clear the search tables.
	if(newspat("",&srch.m,NULL) == Success && newspat("",&rematch,NULL) == Success && newrpat("",&srch.m) == Success)
		(void) newrpat("",&rematch);

	// Initialize word list.
	return setwlist(wordlistd);
	}

// Initialize all of the buffers, windows, screens, and aliases.  Return status.
static int edinit2(void) {

	// Allocate the initial buffer and default screen.
	if(bfind(buffer1,CRBCreate,0,&curbp,NULL) != Success || sfind(1,curbp,&sheadp) != Success)
		return rc.status;

	// Set the current (default) screen and window.
	curwp = wheadp = (cursp = sheadp)->s_curwp = sheadp->s_wheadp;

	return rc.status;
	}

// Set execpath global variable using heap space.  Return status.
int setpath(char *path,bool prepend) {
	char *str1,*str2;
	size_t len;

	len = strlen(path) + 1;
	if(prepend)
		len += strlen(execpath) + 1;
	if((str1 = (char *) malloc(len)) == NULL)
		return rcset(Panic,0,text94,"setpath");
			// "%s(): Out of memory!"
	str2 = stpcpy(str1,path);
	if(prepend) {
		*str2++ = ':';
		strcpy(str2,execpath);
		}
	if(execpath != NULL)
		free((void *) execpath);
	execpath = str1;

	return rc.status;
	}

// Execute a startup file, given filename, "search HOME only" flag, and "ignore missing" flag.  If file not found and ignore is
// true, return (Success) status; otherwise, return an error.
static int startup(char *sfname,int hflag,bool ignore) {
	char *fname;			// Resulting filename to execute.
	Datum *dsinkp;			// For throw-away return value, if any.

	if(pathsearch(&fname,sfname,hflag) != Success)
		return rc.status;
	if(fname == NULL) {
		if(ignore)
			return rc.status;
		char wkbuf[strlen(text136) + strlen(execpath)];
				// " in path \"%s\"",
		if(strchr(sfname,'/') == NULL)
			sprintf(wkbuf,text136,execpath);
		else
			wkbuf[0] = '\0';
		return rcset(Failure,0,text40,sfname,wkbuf);
			// "Script file \"%s\" not found%s"
		}

	// Found: execute the sucker.
	if(dnewtrk(&dsinkp) != 0)
		return drcset();
	return dofile(dsinkp,fname,INT_MIN,SRun_Startup);
	}

// Build a command line with one argument in given string-fab object and execute it.  If fullQuote is true, quote() argument;
// otherwise, just wrap a "" pair around it.
int runcmd(Datum *dsinkp,DStrFab *cmdp,char *cname,char *arg,bool fullQuote) {

	if(dnewtrk(&dsinkp) != 0)
		return drcset();

	// Concatenate command or macro name with arguments.
	if(dopentrk(cmdp) != 0 || dputs(cname,cmdp) != 0 || dputc(' ',cmdp) != 0)
		return drcset();
	if(fullQuote) {
		if(quote(cmdp,arg,true) != Success)
			return rc.status;
		}
	else if(dputc('"',cmdp) != 0 || dputs(arg,cmdp) != 0 || dputc('"',cmdp) != 0)
		return drcset();

	// Close string-fab object and execute command line.
	return dclose(cmdp,sf_string) != 0 ? drcset() : doestmt(dsinkp,cmdp->sf_datp->d_str,TokC_Comment,NULL);
	}

// Process -D or -G switch value.
static int modeswitch(char *value,int index) {
	size_t offset;
	ModeRec *mrp = modetab + index;
	Datum *dsinkp;
	DStrFab cmd;
	char *narg,*end,cmdBuf[32],argBuf[32];

	if(dnewtrk(&dsinkp) != 0)
		return drcset();
	for(;;) {
		// Find beginning and end of next mode string.
		if((end = strchr(value,',')) == NULL)
			end = strchr(value,'\0');

		// Build command and argument in cmdBuf and argBuf.
		if(*value == '^') {
			narg = "-1";
			offset = 1;
			}
		else {
			narg = "1";
			offset = 0;
			}
		sprintf(cmdBuf,"%s => alter%.*sMode",narg,*mrp->cmdlabel == 'D' ? 3 : 99,mrp->cmdlabel);
		sprintf(argBuf,"%.*s",(int)(end - value - offset),value + offset);
		if(runcmd(dsinkp,&cmd,cmdBuf,argBuf,false) != Success) {
			DStrFab msg;

			// Append specifics to rc.msg.
			return (dopenwith(&msg,&rc.msg,true) != 0 || dputf(&msg,text43,*mrp->cmdlabel) != 0 ||
			 dclose(&msg,sf_string) != 0) ? drcset() : rcset(FatalError,0,NULL);
										// " specified with -%c switch"
			}

		// Onward...
		if(*end == '\0')
			break;
		value = end + 1;
		}
	return rc.status;
	}

// Check for a switch value.
static int swval(char *sw) {
	char wkbuf[6];

	if(sw[1] == '\0') {
		sprintf(wkbuf,*sw == '+' ? "'%c'" : "-%c",*sw);
		return rcset(FatalError,0,text45,wkbuf);
			// "%s switch requires a value"
		}
	return rc.status;
	}

// Process -g or + switch value.
static int gotoswitch(char *sw,long *ip1,long *ip2) {
	char *str;
	struct {
		long *var;
		char buf[NWork];
		} x[] = {{ip1,""},{ip2,""},{NULL,""}},*xp = x,*xpz;

	// Null value?
	if(swval(sw) != Success)
		return rc.status;

	// One or two values?
	if((str = strchr(sw + 1,':')) == NULL) {
		stplcpy(x[0].buf,sw + 1,NWork);
		xpz = xp + 1;
		}
	else {
		*str = '\0';
		stplcpy(x[0].buf,sw + 1,NWork);
		stplcpy(x[1].buf,str + 1,NWork);
		*str = ':';
		xpz = xp + 2;
		}

	// Loop through values found and validate them.
	do {
		if(asc_long(xp->buf,xp->var,false) != Success) {
			DStrFab msg;

			return (dopenwith(&msg,&rc.msg,true) != 0 ||
			 dputf(&msg,text61,*sw == '+' ? sw : sw - 1) != 0 || dclose(&msg,sf_string) != 0) ? drcset() :
				// ", switch '%s'"
			 rcset(FatalError,0,NULL);
			}
		} while(++xp < xpz);

	return rc.status;
	}

// Determine which buffer to "land on" prior to entering edit loop, and read its file if appropriate.  bufp is buffer associated
// with first file on command line (if any).  Return status.
static int startbuf(Buffer *bufp,int stdinflag,bool readFirst) {

	// Switch to first file (buffer) specified on command line if auto-load is enabled.
	if(bufp != NULL && readFirst) {
		if(bswitch(bufp) != Success || bactivate(bufp) != Success)
			return rc.status;
		curbp->b_modes |= modetab[MdRec_Default].flags;
		}

	// Delete first buffer if it is not reserved for standard input, it exists, is empty, and is not being displayed.
	if(stdinflag == 0 && (bufp = bsrch(buffer1,NULL)) != NULL && lforw(bufp->b_hdrlnp) == bufp->b_hdrlnp &&
	 bufp->b_nwind == 0) {

		// If first buffer is on another screen, seek and "destroy" it.
		if(bufp->b_nwind > 0) {
			EWindow *winp,*oldcurwp;
			EScreen *scrp = sheadp;
			EScreen *oldcursp = cursp;
			Buffer *oldcurbp = curbp;

			// In all screens...
			do {
				if(scrp != oldcursp) {
					(void) sswitch(scrp);
					oldcurwp = curwp;

					// In all windows...
					winp = wheadp;
					do {
						if(winp->w_bufp == bufp) {		// Found the sucker...
							wswitch(winp);			// switch to its window...
							if(bswitch(oldcurbp) != Success) // and change the buffer.
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
		if(bdelete(bufp,0) != Success)
			return rc.status;
		}

	return update(true);
	}

// Display given type of help information and exit.  Return HelpExit.
static int helpExit(enum h_type t,char *binname) {
	char *info;

	if(t == h_version) {
		if(asprintf(&info,"%s%s%s",Myself,Help0,Version) < 0)
			goto Err;
		}
	else if(t == h_copyright) {
		if(asprintf(&info,"%s %s",Myself,Copyright) < 0)
			goto Err;
		}
	else {		// h_usage or h_help
		char *usage;

		if(asprintf(&usage,"%s%s%s%s%s",Usage[0],binname,Usage[1],binname,Usage[2]) < 0)
			goto Err;
		if(t == h_usage)
			info = usage;
		else {
			if(asprintf(&info,"%s%s%s%s",Myself,Help1,usage,Help2) < 0)
Err:
				return rcset(Panic,0,text94,"helpExit");
					// "%s(): Out of memory!"
			}
		}

	return rcset(HelpExit,0,info);
	}

// Scan command line arguments, looking for a -?, -C, -h, -n, or -V switch.  If one other than -n is found, return helpExit();
// otherwise, set *dostartp to false if -n found, true otherwise, and return status.
static int scancmdline(int argc,char *argv[],bool *dostartp) {
	char *binname;			// My binary name.
	char **strp;
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
		strp = swlist;
		do {
			if(strcmp(*argv,*strp) == 0)
				return helpExit((enum h_type)(strp - swlist),binname);
			} while(*++strp != NULL);
		}

	*dostartp = true;
	return rc.status;
	}

// Process -S switch.  Return status.  For example, if file "/home/jack/bin/testit" contained:
//	#!/usr/local/bin/memacs -? -S
//	...
//
// and was invoked as:
//	$ testit abc xyz
//
// (assuming /home/jack/bin was in the PATH), the arguments would be:
//	0: '/usr/local/bin/memacs'
//	1: '-?'
//	2: '-S'
//	3: '/home/jack/bin/testit'
//	4: 'abc'
//	5: 'xyz'
static int doscript(int argc,char *argv[]) {

	// Any more arguments?
	if(argc == 1)
		return rcset(FatalError,0,text242,*argv);
			// "No arguments found following %s switch"

	// Build command line...
	DStrFab cmd;
	Datum *dsinkp;
	int delim = ' ';

	if(dnewtrk(&dsinkp) != 0 || dopentrk(&cmd) != 0 || dputs("xeqFile",&cmd) != 0)
		return drcset();
	do {
		if(dputc(delim,&cmd) != 0)
			return drcset();
		++argv;
		if(asc_long(*argv,NULL,true)) {
			if(dputs(*argv,&cmd) != 0)
				return drcset();
			}
		else if(quote(&cmd,*argv,true) != Success)
			return rc.status;
		delim = ',';
		} while(--argc > 1);
	if(dclose(&cmd,sf_string) != 0)
		return drcset();

	// and execute it.
	return doestmt(dsinkp,cmd.sf_datp->d_str,TokC_Comment,NULL);
	}

// Process command line.  Return status.
static int docmdline(int argc,char *argv[],char *helpmsg) {
	int n;
	Buffer *bufp;			// Temp buffer pointer.
	Buffer *firstbp = NULL;		// Pointer to first buffer in command line.
	bool rdonlyflag = false;	// Currently in read-only mode?
	bool gotoflag = false;		// Do we need to goto a line at start?
	long gline;			// If so, what line?
	long gchar;			// and what character?
	bool searchflag = false;	// Do we need to search at start?
	int stdinflag = 0;		// Do we need to read from stdin at start?
	char *str;
	Datum *dsinkp;
	DStrFab cmd;
	bool readFirst = true;		// Read first file?
	bool disphelp = true;		// Display initial help message?

	gline = gchar = 1;		// Line and character to go to.
	if(dnewtrk(&dsinkp) != 0)
		return drcset();

	// Parse command line.
	++argv;
	while(--argc > 0) {

		// Process switches.
		if((*argv)[0] == '-') {
			disphelp = false;
			switch((*argv)[1]) {
				case '\0':	// Solo '-' for reading from standard input.
					if(stdinflag)
						break;		// Ignore duplicate.
					stdinflag = 1;		// Process it.
					goto DoFile;
				case 'D':	// -D for add/delete default buffer mode(s).
					n = MdRec_Default;
					goto DoMode;
				case 'd':	// -d for change working directory.
					if(swval(*argv + 1) != Success)
						return rc.status;
					str = "chDir ";
					n = true;		// Escape string.
					goto DoCmd;
				case 'e':	// -e<cmd-line> to execute a command line.
					if(swval(*argv + 1) != Success || doestmt(dsinkp,*argv + 2,TokC_Comment,NULL) != Success)
						return rc.status;
					break;
				case 'G':	// -G for add/delete global mode(s).
					n = MdRec_Global;
DoMode:
					if(modeswitch(*argv + 2,n) != Success)
						return rc.status;
					break;
				case 'g':	// -g<line>[:<char>] for initial goto line and char.
					if(gotoswitch(*argv + 1,&gline,&gchar) != Success)
						return rc.status;
					gotoflag = true;
					break;
				case 'i':	// -i for setting $inpDelim system variable.
					str = "$inpDelim = ";
					n = false;		// Don't escape string.
DoCmd:
					if(runcmd(dsinkp,&cmd,str,*argv + 2,n) != Success)
						return rc.status;
					break;
				case 'N':	// -N for not reading first file at startup.
					readFirst = false;
				case 'n':	// -n for skipping startup files (already processed).
					break;
				case 'R':	// -R for non-read-only (edit) mode.
					rdonlyflag = false;
					break;
				case 'r':	// -r for read-only mode.
					rdonlyflag = true;
					break;
				case 'S':	// -S for execute file from shell.
					if(doscript(argc,argv) != Success)
						return rc.status;
					goto Done;
				case 's':	// -s for initial search string.
					if(swval(*argv + 1) != Success || newspat(*argv + 2,&srch.m,NULL) != Success)
						return rc.status;
					searchflag = true;
					break;
				case 'X':	// -X<path> for file-execute search path.
					if(swval(*argv + 1) != Success)
						return rc.status;
					if(setpath(*argv + 2,true) != Success)
						return rc.status;
					break;
				default:	// Unknown switch.
					return rcset(FatalError,0,text1,*argv);
							// "Unknown switch, %s"
				}
			}
		else if((*argv)[0] == '+') {	// +<line>[:<char>].
			if(gotoswitch(*argv,&gline,&gchar) != Success)
				return rc.status;
			disphelp = false;
			gotoflag = true;
			}
		else if((*argv)[0] == '@') {
			disphelp = false;

			// Process startup macro.
			if(startup(*argv + 1,false,false) != Success)
				return rc.status;
			}
		else {
DoFile:
			// Process an input file.
			disphelp = false;

			// Set up a buffer for this file and set it to inactive.
			if(stdinflag > 0) {
				if(bfind(buffer1,CRBCreate,0,&bufp,NULL) != Success)
					return rc.status;
				}
			else if(bfind(*argv,CRBCreate | CRBForce | CRBFile,0,&bufp,NULL) != Success)
				return rc.status;
			if(stdinflag > 0)		// If using standard input...
				stdinflag = -1;		// disable the flag so it can't be used again.
			else if(setfname(bufp,*argv) != Success)
				return rc.status;
			bufp->b_flags &= ~BFActive;
			if(firstbp == NULL)
				firstbp = bufp;

			// Set the modes appropriately.
			if(rdonlyflag)
				bufp->b_modes |= MdRdOnly;
			}
		++argv;
		}
Done:
	// Done processing arguments.

	// Select initial buffer.
	if(startbuf(firstbp,stdinflag,readFirst) != Success)
		return rc.status;
	if(strcmp(curbp->b_bname,buffer1) != 0)
		disphelp = false;

	// Process startup gotos and searches.
	if(gotoflag) {
		char wkbuf[NWork];

		if(searchflag)
			return rcset(FatalError,0,text101);
				// "Cannot search and goto at the same time!"
		sprintf(wkbuf,"gotoLine %ld",gline);
		if(doestmt(dsinkp,wkbuf,TokC_Comment,NULL) < Failure)
			return rc.status;
		if(rc.status == Success)
			(void) forwch(gchar - 1);
		}
	else if(searchflag) {
		if(huntForw(dsinkp,1,NULL) != Success) {
			(void) update(false);
			return rc.status;
			}
		}

	// Build help message if appropriate.
	*helpmsg = '\0';
	if(disphelp) {
		KeyDesc *kdp;
		struct {
			Datum *datp;
			CFABPtr cfab;
			} *cip,cmdinfo[] = {
				{NULL,{PtrCmd,cftab + cf_help}},
				{NULL,{PtrCmd,cftab + cf_exit}},
				{NULL,{PtrCmd,NULL}}};

		cip = cmdinfo;
		if(dnewtrk(&cip[0].datp) != 0 || dsalloc(cip[0].datp,16) != 0 ||
		 dnewtrk(&cip[1].datp) != 0 || dsalloc(cip[1].datp,16) != 0)
			return drcset();
		do {
			*cip->datp->d_str = '\0';
			if((kdp = getpentry(&cip->cfab)) != NULL) {
				ektos(kdp->k_code,cip->datp->d_str);

				// Change "M-" to "ESC " and "C-x" to "^X" (for the EMacs beginner's benefit).
				if(strsub(dsinkp,2,cip->datp,"M-","ESC ",0) != Success ||
				 strsub(cip->datp,2,dsinkp,"C-","^",0) != Success)
					return rc.status;
				str = cip->datp->d_str;
				do {
					if(str[0] == '^' && is_lower(str[1])) {
						str[1] = upcase[(int) str[1]];
						++str;
						}
					} while(*++str != '\0');
				}
			} while((++cip)->datp != NULL);

		if(*cmdinfo[0].datp->d_str != '\0' && *cmdinfo[1].datp->d_str != '\0')
			sprintf(helpmsg,text41,cmdinfo[0].datp->d_str,cmdinfo[1].datp->d_str);
				// "Enter \"%s\" for help, \"%s\" to quit"
		}

	return rc.status;
	}

// Prepare to insert one or more characters at point.  Delete characters first if in replace or overwrite mode and not at end
// of line.  Assume n > 0.  Return status.
int overprep(int n) {

	if(curbp->b_modes & MdGrp_Over) {
		int count = 0;
		Dot *dotp = &curwp->w_face.wf_dot;
		do {
			if(dotp->off == lused(dotp->lnp))
				break;

			// Delete char if we are in replace mode, or char is not a tab, or we are at a tab stop.
			if((curbp->b_modes & MdRepl) || (lgetc(dotp->lnp,dotp->off) != '\t' ||
			  (getccol() + count) % htabsize == (htabsize - 1)))
				if(ldelete(1L,0) != Success)
					break;
			++count;
			} while(--n > 0);
		}
	return rc.status;
	}

// This is the general interactive command execution routine.  It handles the fake binding of all the printable keys to
// "self-insert".  It also clears out the "thisflag" word and arranges to move it to "lastflag" so that the next command can
// look at it.  "ek" is the key to execute with argument "n".  Return status.
static int execute(ushort ek,KeyDesc *kdp,int n) {
	char keybuf[16];

	// If the keystroke is bound...
	if(kdp != NULL) {

		// Don't reset the command-execution flags on a prefix.
		if(kdp->k_cfab.p_type == PtrPseudo && (kdp->k_cfab.u.p_cfp->cf_aflags & CFPrefix))
			return execkey(kdp,n);

		kentry.thisflag = 0;
		(void) execkey(kdp,n);

		// Remove end-macro key sequence from keyboard macro, if applicable, and check for empty buffer.
		if(rc.status == Success && kdp->k_cfab.u.p_cfp == cftab + cf_endKeyMacro) {
			int len = (kdp->k_code & KeySeq) ? 2 : 1;
			kmacro.km_slotp -= len;
			kmacro.km_endp -= len;
			if(kmacro.km_endp == kmacro.km_buf) {
				kmacro.km_endp = NULL;
				(void) rcset(Success,RCForce,text200);
						// "No keyboard macro defined"
				}
			}

		goto AutoCheck;
		}

	// Keystroke not bound... attempt self-insert.
	if(ek >= 0x20 && ek <= 0xff) {
		int lmode;

		if(n == INT_MIN)
			n = 1;
		else if(n <= 0) {
			kentry.lastflag = 0;
			return rcset(Failure,0,NULL);
			}
		if(allowedit(true) != Success)		// Don't allow if in read-only mode.
			return rc.status;
		kentry.thisflag = 0;			// For the future.

		// In replace or overwrite mode?
		if(overprep(n) != Success)
			return rc.status;

		// Do the appropriate insertion:
		if(!(lmode = curbp->b_modes & MdGrp_Lang) || (curbp->b_modes & MdGrp_Over))
			goto Plain;

			// C, Perl, Ruby, Shell right brace.
		if((ek == '}' && !(lmode & MdMemacs)) ||
			// Ruby "end".
		 (ek == 'd' && (lmode & MdRuby)) ||
		 	// C, MightEMacs, Perl, Ruby, or Shell "else", Ruby "rescue", or Shell "done".
		 (ek == 'e') ||
		 	// MightEMacs, Perl, or Ruby "elsif", MightEMacs "endif", or Shell "elif".
		 (ek == 'f' && !(lmode & MdC)) ||
		 	// MightEMacs "endmacro" or Shell "do".
		 (ek == 'o' && (lmode & (MdMemacs | MdShell))) ||
		 	// MightEMacs "endloop".
		 (ek == 'p' && (lmode & MdMemacs)) ||
		 	// Shell "fi" or "esac".
		 ((ek == 'i' || ek == 'c') && (lmode & MdShell)) ||
		 	// Ruby or Shell "then".
		 (ek == 'n' && (lmode & (MdRuby | MdShell))))
		 	(void) insrfence((int) ek);
		else if((ek == '#' && (lmode & MdC)) || (ek == '=' && (lmode & MdRuby)))
			(void) inspre((int) ek);
		else
Plain:
			(void) linsert(n,(int) ek);

		if(rc.status == Success) {

			// Check for language-mode fence matching.
			if(lmode && (ek == '}' || ek == ')' || ek == ']'))
				fmatch(ek);
AutoCheck:
			// Check auto-save mode and save all changed buffers if needed.
			if((modetab[MdRec_Global].flags & MdASave) && gasave > 0 && --gacount == 0) {
				Datum vsink;		// For throw-away return value, if any.
#if DCaller
				dinit(&vsink,"execute");
#else
				dinit(&vsink);
#endif
				kentry.lastflag = kentry.thisflag;
				kentry.thisflag = 0;
				if(update(false) < Failure || execCF(&vsink,1,cftab + cf_saveFile,0,0) < Failure)
					return rc.status;
				dclear(&vsink);
				}
			}
		kentry.lastflag = kentry.thisflag;
		return rc.status;
		}

	// Unknown key.
	(void) TTbeep();
	kentry.lastflag = 0;					// Fake last flags.
	return rcset(Failure,0,text14,ektos(ek,keybuf));	// Complain.
		// "'%s' not bound"
	}

// Interactive command-processing routine.  Read key sequences and process them.  Return if program exit.
static int editloop(void) {
	int count;
	ushort ek;		// Extended key read from user.
	int n;			// Numeric argument.
	ushort oldflag;		// Old last flag value.
	KeyDesc *kdp;
	Datum rtn;		// Return value from pre-key hook.
	RtnCode lastrc;		// Return code from last key executed.
	char lastkstr[16];	// Last key in string form (supplied to command hooks).
#if DCaller
	static char myname[] = "editloop";

	dinit(&rtn,myname);
	dinit(&lastrc.msg,myname);
#else
	dinit(&rtn);
	dinit(&lastrc.msg);
#endif
	dsetnull(&lastrc.msg);
	opflags &= ~OpStartup;

	// If $lastKeySeq was set by a startup macro, simulate it being entered as the first key.
	if(kentry.uselast)
		goto JumpStart;
	goto ChkMsg;

	// Do forever.
	for(;;) {
		(void) rcclear();
		garbpop(NULL);				// Throw out all the garbage.
		if(agarbfree() != Success)
			break;

		// Update position on current modeline?
		if(modetab[MdRec_Global].flags & (MdLine | MdCol))
#if TypeAhead
			if(typahead(&count) != Success)
				break;
			if(count == 0)
#endif
				upmode(curbp);		// Flag all windows.

		// Fix up the screen.
		if(update(false) <= MinExit)
			break;

		// Any message from last loop to display?
		if(!disnull(&lastrc.msg)) {
			savecursor();
			ek = (lastrc.status == Success && !(lastrc.flags & RCNoWrap)) ? (MLHome | MLForce | MLWrap) :
			 (MLHome | MLForce);
			mlputs(ek,lastrc.msg.d_str);
			if(restorecursor() <= MinExit)
				break;
			}

		// Get the next key from the keyboard or $lastKeySeq.
		modetab[MdRec_Global].flags |= MdMsg;
		if(kentry.uselast) {
JumpStart:
			kdp = getbind(ek = kentry.lastkseq);
			kentry.uselast = false;
			}
		else {
			if(getkseq(&ek,&kdp) <= MinExit)
				break;
			if(rc.status != Success)
				goto Fail;
			}

		// If there is something on the message line, clear it.
		mlerase(MLForce);

		// Set default.
		n = INT_MIN;

		// Do universal/negative repeat argument processing.  ^U sequence is 2, 0, 3, 4,...  ^_ is -1, -2,...
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

		if(ek == corekeys[CK_UnivArg].ek ||
		 ek == corekeys[CK_NegArg].ek) {		// universalArg or negativeArg command.
			int neg;				// n negative?
			int state;				// Numeric argument processing state.
			bool digit;				// Digit entered?

			if(ek == corekeys[CK_NegArg].ek) {
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
				if(mlprintf(MLHome | MLForce,text209,neg ? -n : n) != Success)
							// "Arg: %d"
					break;

				// Get next key.
				if(getkey(&ek) != Success)
					goto Retn;
				digit = (ek >= '0' && ek <= '9');

				// Take action, depending on current state.
				switch(state) {
					case 1:
						if(ek == corekeys[CK_UnivArg].ek) {
							n = 0;
							state = 2;
							continue;
							}
						if(ek == corekeys[CK_NegArg].ek)
							goto Decr5;
						if(ek == '-')
							goto Neg3;
						if(digit)
							goto NBegin;
						break;
					case 2:
						if(ek == corekeys[CK_UnivArg].ek) {
							n = 3;
							state = 5;
							continue;
							}
						if(ek == corekeys[CK_NegArg].ek)
							goto Decr5;
						if(ek == '-') {
Neg3:
							neg = true;
							n = 1;
							state = 3;
							continue;
							}
						if(digit)
							goto NBegin;
						break;
					case 3:
					case 4:
						if(ek == corekeys[CK_UnivArg].ek)
							goto Incr5;
						if(ek == corekeys[CK_NegArg].ek)
							goto Decr5;
						if(digit) {
							if(state == 3) {
NBegin:
								n = ek - '0';
								state = 4;
								}
							else
								n = n * 10 + (ek - '0');
							continue;
							}
						break;
					case 5:
						if(ek == corekeys[CK_UnivArg].ek) {
Incr5:
							if(!neg)
								++n;
							else if(--n == 0)
								neg = false;
							state = 5;
							continue;
							}
						if(ek == corekeys[CK_NegArg].ek) {
Decr5:
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

				// "n" has been finalized.  Process ek as a command.
				tungetc(ek);
				if(getkseq(&ek,&kdp) != Success)
					goto Retn;
				break;
				}

			// Change sign of n if needed.
			if(neg)
				n = -n;

			// Clear the message line.
			mlerase(MLForce);
			}

		// Execute the user-assigned pre-key hook with n arg, preserving lastflag and capturing return value.
		oldflag = kentry.lastflag;
		if(exechook(&rtn,n,hooktab + HkPreKey,1,ektos(kentry.lastkseq,lastkstr)) <= MinExit)
			break;
		kentry.lastflag = oldflag;

	 	// If no pre-key hook error (or no hook)...
		if(rc.status == Success) {

			// Get updated key from pre-key hook, if any.
			if(kentry.uselast) {
				kdp = getbind(ek = kentry.lastkseq);
				kentry.uselast = false;
				}

			// Execute key if no pre-key hook or it returned false ("don't skip execution").
			if(hooktab[HkPreKey].h_bufp == NULL || !tobool(&rtn))
				if(execute(ek,kdp,n) <= MinExit)
					break;

		 	// If no key execution error...
			if(rc.status == Success) {

				// execute the user-assigned post-key hook with current n arg, preserving thisflag.
				oldflag = kentry.thisflag;
				if(exechook(NULL,n,hooktab + HkPostKey,1,lastkstr) <= MinExit)
					break;
				kentry.thisflag = oldflag;
				}
			}

		// Process return code and message from execution of key hook or entered key.
		if(rc.status != Success) {
Fail:
			// Key retrieval/execution failed or returned false.  Kill off any running keyboard macro...
			if(kmacro.km_state == KMPlay) {
				kmacro.km_n = 0;
				kmacro.km_state = KMStop;
				}

			// force terminal input next time around...
			kentry.uselast = false;
			}
ChkMsg:
		// Any message returned?
		if(disnull(&rc.msg))
			dsetnull(&lastrc.msg);			// No, clear last.
		else if(rccpy(&lastrc,&rc) <= MinExit)		// Yes, save message and code.
			break;
		}
Retn:
	return rc.status;
	}

// Return a random seed.
ulong seedinit(void) {

	return time(NULL) & ULONG_MAX;
	}

// Begin MightEMacs.  Basic tasks are:
//   1. Initialze terminal and data structures.
//   2. Process startup file(s) if found.
//   3. Do command-line processing.
//   4. If no errors, enter user-key-entry loop.
//   5. At loop exit, display any error message and return proper exit code to OS.
int main(int argc,char *argv[]) {
	bool dostart;
	char *path;
	Buffer *bufp;
	char helpmsg[50];

	// Do global initializations.
	randseed = seedinit();
	mypid = (unsigned) getpid();
#if MMDebug & Debug_Logfile
	logfile = fopen(Logfile,"w");
#endif
	// Begin using rc (RtnCode): do pre-edit-loop stuff, but use the same return-code mechanism as command calls.
	if(edinit0() == Success &&			// Initialize the return value structures.
	 scancmdline(argc,argv,&dostart) == Success &&	// Scan command line for informational and -n switches.
	 edinit1() == Success &&			// Initialize the core data structures.
	 vtinit() == Success &&				// Initialize the terminal.
	 edinit2() == Success) {			// Initialize buffers, windows, screens.
		initchars();				// Initialize character-set structures.

		// At this point, we are fully operational (ready to process commands).  Set execpath.
		if(setpath((path = getenv(MMPath_Name)) == NULL ? MMPath_Default : path,false) != Success)
			goto Retn;

		// Run site and user startup files and check for macro error.
		if(dostart && (startup(SiteStartup,false,true) != Success || startup(UserStartup,true,true) != Success))
			goto Retn;

		// Run change-directory user hook.
		if(exechook(NULL,INT_MIN,hooktab + HkChDir,0) != Success)
			goto Retn;

		// Process the command line.
		if(docmdline(argc,argv,helpmsg) == Success) {

			// Set help "return" message if appropriate and enter user-command loop.  The message will only be
			// displayed if a message was not previously set in a startup file.
			if(helpmsg[0] != '\0')
				(void) rcset(Success,0,"%s",helpmsg);
			curwp->w_flags |= WFMode;
			(void) editloop();
			}
		}
Retn:
	// Preserve the return code, close the terminal (ignoring any error), and return to line mode.
	if(rccpy(&scriptrc,&rc) == Panic)
		(void) rcset(Panic,0,NULL);	// No return.
	(void) rcclear();
	(void) vttidy(true);
#if MMDebug & Debug_Logfile
	(void) fclose(logfile);
#endif
	// Exit MightEMacs.
	if(scriptrc.status == UserExit || scriptrc.status == ScriptExit) {

		// Display names (in line mode) of any files saved via quickExit().
		bufp = bheadp;
		do {
			if(bufp->b_flags & BFQSave)
				fprintf(stderr,text193,bufp->b_fname);
						// "Saved file \"%s\"\n"
			} while((bufp = bufp->b_nextp) != NULL);

		// Display message, if any.
		if(!disnull(&scriptrc.msg)) {
			DStrFab msg;
			char *str = scriptrc.msg.d_str;

			if(dopentrk(&msg) == 0 && dvizs(str,0,VBaseDef,&msg) == 0 && dclose(&msg,sf_string) == 0)
				str = msg.sf_datp->d_str;
			else
				scriptrc.status = ScriptExit;
			fputs(str,stderr);
			fputc('\n',stderr);
			}

		return scriptrc.status == ScriptExit ? -1 : 0;
		}

	// Error or help exit.
	if(scriptrc.status == HelpExit) {
		fputs(scriptrc.helpText,stderr);
		fputc('\n',stderr);
		}
	else {
		char strbuf[NWork];

		sprintf(strbuf,"%s:",text189);
				// "Abort"
		if(strncmp(scriptrc.msg.d_str,strbuf,strlen(strbuf)) != 0)
			fprintf(stderr,"%s: ",text0);
					// "Error"
		if(scriptrc.status == OSError)
			fprintf(stderr,"%s, %s\n",strerror(errno),scriptrc.msg.d_str);
		else if(disnull(&scriptrc.msg))
			fprintf(stderr,text253,scriptrc.status);
				// "(return status %d)\n"
		else {
			DStrFab msg;
			char *str = scriptrc.msg.d_str;
			char msgbuf[512];

			if(dopentrk(&msg) == 0 && dvizs(str,0,VBaseDef,&msg) == 0 && dclose(&msg,sf_string) == 0)
				strfit(str = msgbuf,sizeof(msgbuf) - 1,msg.sf_datp->d_str,0);
			fputs(str,stderr);
			fputc('\n',stderr);
			}
		}
	return -1;
	}

// Give me some help!  Execute help hook.
int help(Datum *rp,int n,Datum **argpp) {
	HookRec *hrp = hooktab + HkHelp;

	return (hrp->h_bufp == NULL) ? rcset(Failure,0,text246) : exechook(rp,n,hrp,0);
						// "Help hook not set"
	}

// Loop through the list of buffers (ignoring hidden ones) and return true if any are changed; otherwise, false.
static int dirtybuf(void) {
	Buffer *bufp;

	bufp = bheadp;
	do {
		if(!(bufp->b_flags & BFHidden) && (bufp->b_flags & BFChgd) != 0)
			return true;
		} while((bufp = bufp->b_nextp) != NULL);
	return false;
	}

// Quit command.  If n != 0 and not default, always quit; otherwise confirm if a buffer has been changed and not written out.
// If n < 0 and not default, force error return to OS.  Include message if script mode and one is available.
int quit(Datum *rp,int n,Datum **argpp) {
	char *msg = NULL;
	int status = UserExit;		// Default return status.
	bool forceclean = false;

	if(n == INT_MIN)
		n = 0;

	// Not argument-force and dirty buffer?
	if(n == 0 && dirtybuf()) {
		bool yep;

		// Have changed buffer(s)... user okay with that?
		if(mlyesno(text104,&yep) != Success)
				// "Modified buffers exist.  Leave anyway"
			return rc.status;
		if(yep)
			// Force clean exit.
			forceclean = true;
		else
			// User changed his mind... don't exit, no error.
			status = Success;
		mlerase(0);
		}

	// Script mode?
	if(opflags & OpScript) {
		Datum *datp;

		// Get return message, if any, and save it if exiting.
		if(buildmsg(&datp,NULL) == Success && status != Success) {
			if(rcclear() == Success && !disnn(datp))
				msg = datp->d_str;
			if(!forceclean && dirtybuf())
				status = ScriptExit;	// Forced exit from a script with dirty buffer(s).
			}
		}

	// Force error exit if negative n.
	if(n < 0)
		status = ScriptExit;

	return rcset(status,0,msg);
	}

// Store character c in s len times and return pointer to terminating null.
static char *dupchr(char *s,ushort c,int len) {

	while(len-- > 0)
		*s++ = c;
	*s = '\0';
	return s;
	}

// Write a string to a buffer, centered in the current terminal width.  Return status.
static int center(Buffer *bufp,char *src) {
	int pad = (term.t_ncol - strlen(src) - 1) / 2 + 1;
	char *dest,wkbuf[term.t_ncol + 1];

	dest = (pad > 0) ? dupchr(wkbuf,' ',pad) : wkbuf;
	stplcpy(dest,src,wkbuf + sizeof(wkbuf) - dest);
	return bappend(bufp,wkbuf);
	}

// Build and pop up a buffer containing "about the editor" information.  Render buffer and return status.
int aboutMM(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	Datum *datp;
	DStrFab line;
	char *str;
	int i,maxlab;
	static struct liminfo {
		char *label;
		int value;
		} limits[] = {
			{ALit_maxCols,TT_MaxCols},
			{ALit_maxRows,TT_MaxRows},
			{ALit_maxTab,MaxTab},
			{ALit_maxPathname,MaxPathname},
			{ALit_maxBufName,NBufName},
			{ALit_maxUserVar,NVarName},
			{ALit_maxTermInp,NTermInp},
			{ALit_killRingSize,NRing},
			{ALit_typeAhead,-(TypeAhead + 1)},
			{NULL,0}
			};
	struct liminfo *limp;

	// Get a buffer.
	if(sysbuf(text6,&bufp) != Success)
			// "About"
		return rc.status;

	// Get a datum object.
	if(dnewtrk(&datp) != 0)
		return drcset();

	// Construct the "About MightEMacs" lines.
	if(bappend(bufp,"\n\n") != Success)				// Vertical space.
		return rc.status;
	if(dopenwith(&line,datp,false) != 0)					// Expand program name...
		return drcset();
	str = Myself;
	do {
		if(dputc(*str++,&line) != 0 || dputc(' ',&line) != 0)
			return drcset();
		} while(*str != '\0');
	if(dunputc(&line) != 0 || dclose(&line,sf_string) != 0)
		return drcset();
	if(center(bufp,datp->d_str) != Success)				// and center it.
		return rc.status;
	if(bappend(bufp,"") != Success)					// Blank line.
		return rc.status;
	if(dopenwith(&line,datp,false) != 0 ||				// Build "version" line...
	 dputf(&line,"%s %s",text185,Version) != 0 || dclose(&line,sf_string) != 0)
			// "Version"
		return drcset();
	if(center(bufp,datp->d_str) != Success)				// and center it.
		return rc.status;
	if(bappend(bufp,"") != Success)					// Blank line.
		return rc.status;
	if(center(bufp,ALit_author) != Success)				// Copyright.
			// "(c) Copyright 2017 Richard W. Marinelli <italian389@yahoo.com>"
		return rc.status;

	// Construct the program limits' lines.
	if(bappend(bufp,"\n") != Success)				// Blank lines.
		return rc.status;
	if(center(bufp,ALit_buildInfo) != Success)			// Limits' header.
		// "Build Information"
		return rc.status;
	if(bappend(bufp,"") != Success)					// Blank line.
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
		str = pad(wkbuf + i,maxlab + 1);
		if(limp->value < 0)
			sprintf(str,"%9s",limp->value == -1 ? "No" : "Yes");
		else
			sprintf(str,"%9d",limp->value);

		// Add line to buffer.
		if(bappend(bufp,wkbuf) != Success)
			return rc.status;
		} while((++limp)->label != NULL);
	}

	// Write platform, credit, and license blurb.
	if(bappend(bufp,"") != Success)					// Blank line.
		return rc.status;
	if(dopenwith(&line,datp,false) != 0 ||
	 dputf(&line,"%s%s",Myself,ALit_footer1) != 0 || dclose(&line,sf_string) != 0)
			// " runs on Unix and Linux platforms and is licensed under the"
		return drcset();
	if(center(bufp,datp->d_str) != Success || center(bufp,ALit_footer2) != Success || center(bufp,ALit_footer3) != Success)
			// "GNU General Public License (GPLv3), which can be viewed at"
			// "http://www.gnu.org/licenses/gpl-3.0.en.html"
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,bufp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}
