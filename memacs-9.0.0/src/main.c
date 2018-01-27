// (c) Copyright 2018 Richard W. Marinelli
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
// return code is usually Success or Failure; however, it can be more severe (see std.h for a list), such as UserAbort or
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
#include "plgetswitch.h"

// Local definitions.
enum h_type {h_copyright,h_version,h_usage,h_help};
static char					// Command switch names.
	*sw_buf_mode[] = {"buf-mode","B",NULL},
	*sw_copyright[] = {"copyright","C",NULL},
	*sw_dir[] = {"dir","d",NULL},
	*sw_exec[] = {"exec","e",NULL},
	*sw_global_mode[] = {"global-mode","G",NULL},
	*sw_help[] = {"help",NULL},
	*sw_inp_delim[] = {"inp-delim","i",NULL},
	*sw_no_read[] = {"no-read","N",NULL},
	*sw_no_startup[] = {"no-startup","n",NULL},
	*sw_otp_delim[] = {"otp-delim","o",NULL},
	*sw_path[] = {"path",NULL},
	*sw_r[] = {"r",NULL},
	*sw_rw[] = {"rw",NULL},
	*sw_search[] = {"search","s",NULL},
	*sw_shell[] = {"shell","S",NULL},
	*sw_usage[] = {"usage","?",NULL},
	*sw_version[] = {"version","V",NULL};
static SwitchDescriptor cSwitchTable[] = {	// Command switch table.
/*1*/	{sw_copyright,SF_NoArg},
/*2*/	{sw_dir,SF_RequiredArg},
/*3*/	{sw_exec,SF_RequiredArg | SF_AllowRepeat},
/*4*/	{sw_global_mode,SF_RequiredArg | SF_AllowRepeat},
/*5*/	{sw_help,SF_NoArg},
/*6*/	{sw_inp_delim,SF_RequiredArg},
/*7*/	{sw_no_read,SF_NoArg},
/*8*/	{sw_no_startup,SF_NoArg},
/*9*/	{sw_otp_delim,SF_RequiredArg},
/*10*/	{sw_path,SF_RequiredArg},
/*11*/	{sw_r,SF_NoArg},
/*12*/	{sw_shell,SF_NoArg},
/*13*/	{sw_usage,SF_NoArg},
/*14*/	{sw_version,SF_NoArg},
	{NULL,0}};

#define CSW_copyright		1
#define CSW_help		5
#define CSW_no_startup		8
#define CSW_usage		13
#define CSW_version		14

static SwitchDescriptor aSwitchTable[] = {	// Argument switch table.
/*1*/	{NULL,SF_NumericSwitch},
/*2*/	{NULL,SF_NumericSwitch | SF_PlusType},
/*3*/	{sw_buf_mode,SF_RequiredArg | SF_AllowRepeat},
/*4*/	{sw_r,SF_NoArg},
/*5*/	{sw_rw,SF_NoArg},
/*6*/	{sw_search,SF_RequiredArg},
	{NULL,0}};

// Make selected global definitions local.
#define DATAmain
#define DATAcmd
#include "std.h"
#include "bind.h"
#include "exec.h"
#include "file.h"
#include "search.h"
#include "var.h"
#include "cmd.h"

// Copy a string to open string-fab object, escaping the leading ~ character (AttrSeqBegin) of a terminal attribute sequence if
// needed.  Return -1 if error; otherwise, 0.
int escattrtosf(DStrFab *sfp,char *str) {

	while(*str != '\0')
		if((*str == AttrSeqBegin && dputc(AttrSeqBegin,sfp) != 0) || dputc(*str++,sfp) != 0)
			return -1;
	return 0;
	}

// Copy global return message to open string-fab object, escaping the leading ~ character (AttrSeqBegin) of a terminal attribute
// sequence if needed.  Return status.
int escattr(DStrFab *sfp) {
	char *str = rc.msg.d_str;
	if(rc.flags & RCTermAttr) {
		if(dputs(str,sfp) != 0 || dputc(AttrSeqBegin,sfp) != 0 || dputc(AttrAllOff,sfp) != 0)
			goto Fail;
		}
	else if(escattrtosf(sfp,str) != 0)
Fail:
		return librcset(Failure);

	return Success;
	}

// Clear return message if RCKeepMsg flag not set, clear return flags, set return status to Success, and return it.
int rcclear(ushort flags) {

	if(!(flags & RCKeepMsg))
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
		(void) librcset(Failure);
	fprintf(logfile,"rccpy(): copied from (%hu) '%s' [%d] to (%hu) '%s' [%d]\n",
	 srcp->msg.d_type,srcp->msg.d_str,srcp->status,destp->msg.d_type,destp->msg.d_str,destp->status);
	return rc.status;
#else
	return datcpy(&destp->msg,&srcp->msg) != 0 ? librcset(Failure) : rc.status;
#endif
	}

// Write text into the return code message (unless RCKeepMsg flag) and return most severe status.  If status is Success, RCForce
// flag is not set, and global 'msg' mode is not set, do nothing; otherwise, if status is the same, keep existing message (do
// nothing) unless it's a force or existing message is null.
int rcset(int status,ushort flags,char *fmt,...) {

	if((flags & RCUnFail) && rc.status == Failure)
		(void) rcclear(flags);

	// Check status.  If this status is not more severe, or Success, not force, and not displaying messages, return old one.
	if(status > rc.status || (!(flags & RCForce) && ((status == Success && !(modetab[MdIdxGlobal].flags & MdMsg)) || 
	 (status == rc.status && !disnull(&rc.msg)))))
		return rc.status;

	// Save message (if RCKeepMsg flag is not set) and new status.
	if(status == HelpExit)
		rc.helpText = fmt;
	else if(fmt != NULL) {
		if(!(flags & RCKeepMsg) || disnull(&rc.msg)) {
			va_list ap;
			char *str;

			if(status != Panic) {
				if(flags & RCNoFormat) {
					if((str = (char *) malloc(strlen(fmt) + 1)) != NULL)
						strcpy(str,fmt);
					}
				else {
					va_start(ap,fmt);
					(void) vasprintf(&str,fmt,ap);
					va_end(ap);
					}
				}

			// If Panic serverity or vasprintf() failed, panic!  That is, we're most likely out of memory, so bail
			// out with "great expediency".
			if(status == Panic || str == NULL) {
				(void) TTclose(
#if RestoreTerm
				 TermCup |
#endif
				 TermForce);
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
				exit(255);
				}
			dsetmemstr(str,&rc.msg);
			}
		}
	else if((flags & (RCForce | RCKeepMsg)) == RCForce)
		dsetnull(&rc.msg);
	rc.flags = flags;
#if MMDebug & Debug_Datum
	fprintf(logfile,"rcset(%d,%.4X,...): set msg '%s', status %d\n",status,(uint) flags,rc.msg.d_str,status);
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
		return librcset(Failure);
	if(catargs(datp,0,NULL,0) != Success || (prefix == NULL && disnn(datp)))
		return rc.status;
	if(dopenwith(&msg,*datpp,SFClear) != 0 || (prefix != NULL && dputs(prefix,&msg) != 0))
		return librcset(Failure);
	if(!disnn(datp))
		if((!disempty(&msg) && dputs(": ",&msg) != 0) || dputs(datp->d_str,&msg) != 0)
			return librcset(Failure);
	return dclose(&msg,sf_string) != 0 ? librcset(Failure) : rc.status;
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
		return rcset(Failure,RCNoFormat,text105);
			// "Keyboard macro already active, cancelled"
		}
	kmacro.km_state = KMRecord;
	curwp->w_flags |= WFMode;
	return rcset(Success,RCNoFormat,text106);
		// "Begin macro"
	}

// End keyboard macro recording.  Check for the same limit conditions as the above routine.  Set up the variables and return
// status.
int endKeyMacro(Datum *rp,int n,Datum **argpp) {

	if(kmacro.km_state == KMStop)
		return rcset(Failure,RCNoFormat,text107);
			// "Keyboard macro not active"

	// else in KMRecord state (KMPlay not possible).
	kmacro.km_endp = kmacro.km_slotp;
	kmacro.km_state = KMStop;
	curwp->w_flags |= WFMode;
	return rcset(Success,RCNoFormat,text108);
		// "End macro"
	}

// Enable execution of a keyboard macro n times (which will be subsequently run by the getkey() function).  Error if not in
// KMStopped state.  Set up the variables and return status.
int xeqKeyMacro(Datum *rp,int n,Datum **argpp) {

	if(kmacro.km_state != KMStop) {
		clearKeyMacro(true);
		return rcset(Failure,RCNoFormat,text105);
			// "Keyboard macro already active, cancelled"
		}
	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"
	if(kmacro.km_endp == NULL)
		return rcset(Failure,RCNoFormat,text200);
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
	return rcset(UserAbort,RCNoFormat,text8);
			// "Aborted"
	}

// "abort" command: Call abortinp() to abort processing and build optional exception message.  Enable terminal attributes in
// message if n argument.
int abortOp(Datum *rp,int n,Datum **argpp) {

	(void) abortinp();

	// Short and sweet if called by user pressing a key.
	if(!(opflags & OpScript))
		return rc.status;

	// Called from elsewhere (a script)... build a message.
	(void) rcclear(0);
	Datum *datp;
	if(buildmsg(&datp,text189) == Success)
			// "Abort"
		(void) rcset(UserAbort,n == INT_MIN ? RCNoFormat : RCNoFormat | RCTermAttr,datp->d_str);

	return rc.status;
	}

// Concatenate arguments and force-set result as an informational (Success) return message.  Suppress brackets [] if "success"
// is false or n <= 0.  Enable terminal attributes if n >= 0.  Return "success" value.
int notice(Datum *rp,int n,bool success) {

	if(havesym(s_any,true)) {
		Datum *datp;

		if(buildmsg(&datp,NULL) == Success) {
			ushort flags = RCNoFormat | RCForce;
			if(!success)
				goto NoWrap;
			if(n != INT_MIN) {
				if(n <= 0)
NoWrap:
					flags |= RCNoWrap;
				if(n >= 0)
					flags |= RCTermAttr;
				}
			dsetbool(success,rp);
			(void) rcset(Success,flags,datp->d_str);
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
		return librcset(Failure);
	dsetnil(dsinkp);

	// Bound to a macro?
	if(cfabp->p_type == PtrMacro)
		(void) execbuf(dsinkp,n,cfabp->u.p_bufp,NULL,0);

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
		(void) rcset(Failure,RCNoFormat | RCKeepMsg,text300);
				// "False return"
	return rc.status;
	}

// Initialize the return message structures.  Return status.
static int edinit0(void) {
#if DCaller
	static char myname[] = "edinit0";
#endif
	// Initialize i var, return code, and numeric literals.
#if DCaller
	dinit(&ivar.format,myname);
	dinit(&rc.msg,myname);
	dinit(&scriptrc.msg,myname);
#else
	dinit(&ivar.format);
	dinit(&rc.msg);
	dinit(&scriptrc.msg);
#endif
	if(dsetsubstr("%d",2,&ivar.format) != 0)
		return librcset(Failure);
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
			return librcset(Failure);
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

	// Initialize previous search patterns, "undelete" buffer, and keyboard macro.
#if DCaller
	dinit(&undelbuf.re_data,myname);
#else
	dinit(&undelbuf.re_data);
#endif
	if(growKeyMacro() != Success)
		return rc.status;

	// Allocate permanent Datum objects for search tables.
	if(grpinit(&srch.m) != Success || grpinit(&rematch) != Success)
		return rc.status;
	lastMatch = srch.m.groups[0].matchp;		// Initially null.

	// Get space for SampBuf buffers.
	sampbuf.buflen = n = term.t_mcol + 1;
	sampbuf.smallsize = n / 4;
	if((sampbuf.buf = (char *) malloc(sampbuf.buflen)) == NULL)
		return rcset(Panic,0,text94,myname);
			// "%s(): Out of memory!"
	*sampbuf.buf = '\0';

	// Load all the key bindings.
	if(loadbind() != Success)
		return rc.status;

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

	// Clear the search tables and initialize word list.
	if(newspat("",&srch.m,NULL) == Success && newspat("",&rematch,NULL) == Success && newrpat("",&srch.m) == Success &&
	 newrpat("",&rematch) == Success)
		(void) setwlist(wordlistd);

	return rc.status;
	}

// Initialize all of the buffers, windows, screens, and aliases.  Return status.
static int edinit2(void) {

	// Allocate the initial buffer and screen.
	if(bfind(buffer1,CRBCreate,0,&curbp,NULL) != Success || sfind(1,curbp,&sheadp) != Success)
		return rc.status;

	// Set the global screen and window pointers.
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
		return librcset(Failure);
	return execfile(dsinkp,fname,INT_MIN,Arg_First | SRun_Startup);
	}

// Build a command line with one argument in given string-fab object and execute it.  If fullQuote is true, quote() argument;
// otherwise, just wrap a "" pair around it.
int runcmd(Datum *dsinkp,DStrFab *cmdp,char *cname,char *arg,bool fullQuote) {

	if(dnewtrk(&dsinkp) != 0)
		return librcset(Failure);

	// Concatenate command or macro name with arguments.
	if(dopentrk(cmdp) != 0 || dputs(cname,cmdp) != 0 || dputc(' ',cmdp) != 0)
		return librcset(Failure);
	if(fullQuote) {
		if(quote(cmdp,arg,true) != Success)
			return rc.status;
		}
	else if(dputc('"',cmdp) != 0 || dputs(arg,cmdp) != 0 || dputc('"',cmdp) != 0)
		return librcset(Failure);

	// Close string-fab object and execute command line.
	return dclose(cmdp,sf_string) != 0 ? librcset(Failure) : execestmt(dsinkp,cmdp->sf_datp->d_str,TokC_Comment,NULL,NULL);
	}

// Process -buf-mode or -global-mode switch value.
static int modeswitch(SwitchResult *swrp,ModeRec *mrp,uint *flagp) {
	size_t len;
	ushort offset;
	Datum *dsinkp;
	DStrFab cmd;
	int narg;
	char *end,*value = swrp->value;
	char cmdBuf[32],argBuf[32];

	if(mrp != NULL && dnewtrk(&dsinkp) != 0)
		return librcset(Failure);
	for(;;) {
		// Find beginning and end of next mode string.
		if((end = strchr(value,',')) == NULL)
			end = strchr(value,'\0');

		// If global or show mode, build command and argument in cmdBuf and argBuf and call runcmd() to execute it;
		// otherwise (for a buffer mode), call domode().
		if(*value == '^') {
			narg = -1;
			offset = 1;
			}
		else {
			narg = 1;
			offset = 0;
			}
		len = end - value - offset;
		sprintf(argBuf,"%.*s",(int)(len > sizeof(argBuf) - 1 ? sizeof(argBuf) - 1 : len),value + offset);
		if(mrp != NULL) {
			sprintf(cmdBuf,"%d => alter%sMode",narg,mrp->cmdlabel);
			if(runcmd(dsinkp,&cmd,cmdBuf,argBuf,false) != Success) {
				DStrFab msg;

				// Append specifics to rc.msg.
				return (dopenwith(&msg,&rc.msg,SFPrepend) != 0 || dputf(&msg,text43,swrp->name) != 0 ||
											// "-%s switch: "
				 dclose(&msg,sf_string) != 0) ? librcset(Failure) : rcset(FatalError,0,NULL);
				}
			}
		else if(domode(argBuf,narg,MdIdxBuffer,flagp,NULL,NULL) != Success)
			return rc.status;

		// Onward...
		if(*end == '\0')
			break;
		value = end + 1;
		}
	return rc.status;
	}

// Determine which buffer to "land on" prior to entering edit loop, and read its file if appropriate.  bufp is associated with
// a file on command line (or NULL if none), depending on whether certain argument switches were specified.  Return status.
static int startbuf(Buffer *bufp,short stdinflag,bool readFirst) {

	// Switch to first file (buffer) specified on command line if auto-load is enabled.
	if(bufp != NULL && readFirst)
		if(bswitch(bufp) != Success || bactivate(bufp) != Success)
			return rc.status;

	// Delete first buffer if it is not reserved for standard input, it exists, is empty, and is not being displayed.
	if(stdinflag == 0 && (bufp = bsrch(buffer1,NULL)) != NULL && bufp->b_hdrlnp->l_nextp == bufp->b_hdrlnp &&
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
		(void) bdelete(bufp,0);
		}

	return rc.status;
	}

// Display given type of help information and exit (return HelpExit status).
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

// Scan command line arguments, looking for a -usage, -copyright, -help, -no-start, or -version switch.  If one other than
// -no-start is found, return helpExit(); otherwise, set *dostartp to false if -no-start found, true otherwise, and return
// status.
static int scancmdline(int argc,char *argv[],bool *dostartp) {
	char *binname;			// My binary name.
	short *rcp;
	int rcode;
	SwitchResult result;
	SwitchDescriptor *swdp = cSwitchTable;
	static short swlist[] = {
		CSW_copyright,CSW_version,CSW_usage,CSW_help,0};


	// Get my name and set default return value.
	binname = fbasename(*argv++,true);
	*dostartp = true;

	// Scan command line arguments.
	--argc;
	while((rcode = getswitch(&argc,&argv,&swdp,&result)) > 0) {
		if(!(cSwitchTable[rcode - 1].flags & SF_NumericSwitch)) {
			if(rcode == CSW_no_startup)
				*dostartp = false;
			else {
				rcp = swlist;
				do {
					if(rcode == *rcp)
						return helpExit((enum h_type)(rcp - swlist),binname);
					} while(*++rcp != 0);
				}
			}
		}

	return (rcode < 0) ? librcset(Failure) : rc.status;
	}

// Process -shell (-S) switch.  Return status.  For example, if file "/home/jack/bin/testit" contained:
//	#!/usr/local/bin/memacs -S -r
//	...
//
// and was invoked as:
//	$ testit abc xyz
//
// (assuming /home/jack/bin was in the PATH), the arguments would be:
//	0: '/usr/local/bin/memacs'
//	1: '-S'
//	2: '-r'
//	3: '/home/jack/bin/testit'
//	4: 'abc'
//	5: 'xyz'
//
// This function would be called with 'home/jack/bin/testit' as the first argument.
static int doscript(int argc,char *argv[]) {
	DStrFab cmd;
	Datum *dsinkp;
	short delim = ' ';

	// Build command line...
	if(dnewtrk(&dsinkp) != 0 || dopentrk(&cmd) != 0 || dputs("xeqFile",&cmd) != 0)
		return librcset(Failure);
	do {
		if(dputc(delim,&cmd) != 0)
			return librcset(Failure);
		if(asc_long(*argv,NULL,true)) {
			if(dputs(*argv,&cmd) != 0)
				return librcset(Failure);
			}
		else if(quote(&cmd,*argv,true) != Success)
			return rc.status;
		delim = ',';
		++argv;
		} while(--argc > 0);
	if(dclose(&cmd,sf_string) != 0)
		return librcset(Failure);

	// and execute it.
	return execestmt(dsinkp,cmd.sf_datp->d_str,TokC_Comment,NULL,NULL);
	}

// Process command line.  Return status.
static int docmdline(int argc,char *argv[],char *helpmsg) {
	int rcode,n;
	ushort fileActionCt = 0;	// Count of data files.
	Buffer *bufp;			// Temp buffer pointer.
	Buffer *bufp1 = NULL;		// Buffer pointer to data file to read first.
	bool cRdOnly = false;		// Command-level read-only mode?
	bool aRdOnly;			// Argument-level read-only mode?
	bool aRdWrite;			// Argument-level read/write mode?
	bool gotoflag = false;		// Go to a line at start?
	long gline;			// If so, what line?
	bool searchflag = false;	// Search for a pattern at start?
	short stdinflag = 0;		// Read from standard input at start?
	bool shellScript = false;	// Running as shell script?
	char *str;
	Datum *dsinkp;
	DStrFab cmd;
	bool readFirst = true;		// Read first file?
	bool displayHelp = true;	// Display initial help message?
	SwitchResult result;
	SwitchDescriptor *swdp = cSwitchTable;

	gline = 1;			// Line and character to go to.
	if(dnewtrk(&dsinkp) != 0)
		return librcset(Failure);

	// Get command switches.
	--argc;
	++argv;
	while((rcode = getswitch(&argc,&argv,&swdp,&result)) > 0) {
		displayHelp = false;
		switch(rcode) {
			case 2:		// -dir for change working directory.
				str = "chDir ";
				n = true;		// Escape string.
				goto DoCmd;
			case 3:		// -exec to execute a command line.
				if(execestmt(dsinkp,result.value,TokC_Comment,NULL,NULL) != Success)
					return rc.status;
				break;
			case 4:		// -global-mode for set/clear global mode(s).
				if(modeswitch(&result,modetab + MdIdxGlobal,NULL) != Success)
					return rc.status;
				break;
			case 6:		// -inp-delim for setting $inpDelim system variable.
				str = "$inpDelim = ";
				n = false;		// Don't escape string.
				goto DoCmd;
			case 7:		// -no-read for not reading first file at startup.
				readFirst = false;
				// Fall through.
			case 8:		// -no-startup for skipping startup files (already processed).
				break;
			case 9:		// -otp-delim for setting $otpDelim system variable.
				str = "$otpDelim = ";
				n = false;		// Don't escape string.
DoCmd:
				if(runcmd(dsinkp,&cmd,str,result.value,n) != Success)
					return rc.status;
				break;
			case 10:	// -path for setting file-execution search path.
				if(setpath(result.value,true) != Success)
					return rc.status;
				break;
			case 11:	// -r for read-only mode.
				cRdOnly = true;
				break;
			case 12:	// -shell for execute script file from shell.
				shellScript = true;
			}
		}

	// Switch error?
	if(rcode < 0)
		return librcset(FatalError);

	// Process remaining arguments, if any.
	if(!shellScript) {
		while(argc-- > 0) {
			if(strcmp(*argv,"-") == 0) {		// Solo '-' for reading from standard input.
				if(stdinflag != 0)
					return rcset(FatalError,0,text45);
						// "Multiple \"-\" arguments"
				stdinflag = 1;			// Process it.
				goto DoFile;
				}
			if((*argv)[0] == '@') {			// Process startup script.
				displayHelp = false;
				if(startup(*argv + 1,false,false) != Success)
					return rc.status;
				++argv;
				}
			else {					// Process a data file.
DoFile:
				// Set up a buffer for the file and set it to inactive.
				str = *argv++;
				if(stdinflag > 0) {
					if(bfind(buffer1,CRBCreate | CRBHook,0,&bufp,NULL) != Success)
						return rc.status;
					}
				else if(bfind(str,CRBCreate | CRBHook | CRBForce | CRBFile,0,&bufp,NULL) != Success)
					return rc.status;
				if(stdinflag > 0)		// If using standard input...
					stdinflag = -1;		// disable the flag so it can't be used again.
				else if(setfname(bufp,str) != Success)
					return rc.status;
				bufp->b_flags &= ~BFActive;
				if(cRdOnly)
					bufp->b_modes |= MdRdOnly;
				if(bufp1 == NULL)
					bufp1 = bufp;

				// Check for argument switches.
				displayHelp = aRdOnly = aRdWrite = false;
				swdp = aSwitchTable;
				while((rcode = getswitch(&argc,&argv,&swdp,&result)) > 0) {
					switch(rcode) {
						case 1:
						case 2:		//  {+|-}line for initial line to go to.
							if(++fileActionCt > 1 || gotoflag)
								goto Conflict2;
							if(!readFirst)
								goto Conflict3;
							(void) asc_long(result.value,&gline,false);
							gotoflag = true;
							bufp1 = bufp;
							break;
						case 3:		// -buf-mode for set/clear buffer mode(s).
							if(modeswitch(&result,NULL,&bufp->b_modes) != Success)
								return rc.status;
							break;
						case 4:		// -r for read-only mode.
							if(aRdWrite)
								goto Conflict1;
							aRdOnly = true;
							goto SetRW;
						case 5:		// -rw for read/write mode.
							if(aRdOnly)
Conflict1:
								return rcset(FatalError,0,text1);
									// "-r and -rw switch conflict"
							aRdWrite = true;
SetRW:
							// Set or clear read-only mode.
							if(aRdOnly)
								bufp->b_modes |= MdRdOnly;
							else
								bufp->b_modes &= ~MdRdOnly;
							break;
						default:	// -search for initial search string.
							if(++fileActionCt > 1 || gotoflag)
Conflict2:
								return rcset(FatalError,0,text101,text175);
										// "%sconflict", "-search, + or - switch "
							if(!readFirst)
Conflict3:
								return rcset(FatalError,0,text61,text175);
								// "%sconflicts with -no-read", "-search, + or - switch "
							if(newspat(result.value,&srch.m,NULL) != Success)
								return rc.status;
							searchflag = true;
							bufp1 = bufp;
							break;
						}
					}
				if(rcode < 0)
					return librcset(FatalError);
				}
			}
		}
	else if(doscript(argc,argv) != Success)
		return rc.status;

	// Done processing arguments.  Select initial buffer.
	if(startbuf(bufp1,stdinflag,readFirst) != Success)
		return rc.status;
	if(strcmp(curbp->b_bname,buffer1) != 0)
		displayHelp = false;

	// Process startup gotos and searches.
	if(gotoflag) {
		char wkbuf[NWork];

		sprintf(wkbuf,gline >= 0 ? "gotoLine %ld" : "endBuf() || %ld => forwLine",gline);
		if(execestmt(dsinkp,wkbuf,TokC_Comment,NULL,NULL) < Failure)
			return rc.status;
		}
	else if(searchflag && huntForw(dsinkp,1,NULL) != Success)
		return rc.status;

	// Build help message if appropriate.
	*helpmsg = '\0';
	if(displayHelp) {
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
			return librcset(Failure);
		do {
			*cip->datp->d_str = '\0';
			if((kdp = getpentry(&cip->cfab)) != NULL) {
				ektos(kdp->k_code,cip->datp->d_str,true);

				// Change "M-" to "ESC " and "C-x" to "^X" (for the MightEMacs beginner's benefit).
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
				// "Enter ~#u%s~U for help, ~#u%s~U to quit"
		}

	return rc.status;
	}

// Prepare to insert one or more characters at point.  Delete characters first if in replace or overwrite mode and not at end
// of line.  Assume n > 0.  Return status.
int overprep(int n) {

	if(curbp->b_modes & MdGrp_Repl) {
		int count = 0;
		Dot *dotp = &curwp->w_face.wf_dot;
		do {
			if(dotp->off == dotp->lnp->l_used)
				break;

			// Delete character if we are in replace mode, character is not a tab, or we are at a tab stop.
			if((curbp->b_modes & MdRepl) || (dotp->lnp->l_text[dotp->off] != '\t' ||
			  (getccol(NULL) + count) % htabsize == (htabsize - 1)))
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
			int len = (kdp->k_code & Prefix) ? 2 : 1;
			kmacro.km_slotp -= len;
			kmacro.km_endp -= len;
			if(kmacro.km_endp == kmacro.km_buf) {
				kmacro.km_endp = NULL;
				(void) rcset(Success,RCNoFormat | RCForce,text200);
						// "No keyboard macro defined"
				}
			}

		goto AutoCheck;
		}

	// Keystroke not bound... attempt self-insert.
	if(ek >= 0x20 && ek < 0xFF) {
		int lmode = 0;

		if(n == INT_MIN)
			n = 1;
		else if(n == 0 || allowedit(true) != Success)		// Don't allow if in read-only mode.
			return rc.status;
		kentry.thisflag = 0;

		// In replace or overwrite mode?
		if(n < 0) {
			n = -n;
			goto Plain;
			}
		if(overprep(n) != Success)
			return rc.status;

		// Do the appropriate insertion:
		if(!(lmode = curbp->b_modes & MdGrp_Lang) || (curbp->b_modes & MdGrp_Repl))
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
		 	(void) insrfence(ek);
		else if((ek == '#' && (lmode & MdC)) || (ek == '=' && (lmode & MdRuby)))
			(void) inspre(ek);
		else
Plain:
			(void) linsert(n,ek);

		if(rc.status == Success) {

			// Check for language-mode fence matching.
			if(lmode && (ek == '}' || ek == ')' || ek == ']'))
				fmatch(ek);
AutoCheck:
			// Save all changed buffers if auto-save mode and keystroke count reached.
			if((modetab[MdIdxGlobal].flags & MdASave) && gasave > 0 && --gacount == 0) {
				Datum vsink;		// For throw-away return value, if any.
#if DCaller
				dinit(&vsink,"execute");
#else
				dinit(&vsink);
#endif
				kentry.lastflag = kentry.thisflag;
				kentry.thisflag = 0;
				if(execCF(&vsink,1,cftab + cf_saveFile,0,0) < Failure)
					return rc.status;
				dclear(&vsink);
				}
			}
		kentry.lastflag = kentry.thisflag;
		return rc.status;
		}

	// Unknown key.
	(void) TTbeep();
	kentry.lastflag = 0;						// Fake last flags.
	return rcset(Failure,RCTermAttr,text14,ektos(ek,keybuf,true));	// Complain.
		// "~#u%s~U not bound"
	}

// Interactive command-processing routine.  Read key sequences and process them.  Return if program exit.
static int editloop(void) {
	ushort ek;		// Extended key read from user.
	int n;			// Numeric argument.
	ushort oldflag;		// Old last flag value.
	KeyDesc *kdp;
	RtnCode lastrc;		// Return code from last key executed.
	char lastkstr[16];	// Last key in string form (supplied to command hooks).
#if DCaller
	static char myname[] = "editloop";

	dinit(&lastrc.msg,myname);
#else
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
		(void) rcclear(0);
		garbpop(NULL);				// Throw out all the garbage.
		if(agarbfree() != Success)
			break;

		// Fix up the screen.
		if(update(INT_MIN) <= MinExit)
			break;

		// Any message from last loop to display?
		if(!disnull(&lastrc.msg)) {
			ushort flags = MLHome | MLForce | MLFlush;
			if(lastrc.status == Success && !(lastrc.flags & RCNoWrap))
				flags |= MLWrap;
			if(lastrc.flags & RCTermAttr)
				flags |= MLTermAttr;
			if(mlputs(flags,lastrc.msg.d_str) != Success ||
			 movecursor(cursp->s_cursrow,cursp->s_curscol) != Success || TTflush() != Success)
				break;
			}

		// Get the next key from the keyboard or $lastKeySeq.
		modetab[MdIdxGlobal].flags |= MdMsg;
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
		if(mlerase(MLForce) != Success)
			break;

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
				if(mlprintf(MLHome | MLForce | MLFlush,text209,neg ? -n : n) != Success)
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
			if(mlerase(MLForce) != Success)
				break;
			}

		oldflag = kentry.lastflag;

		// Execute the user-assigned pre-key hook with n argument, preserving lastflag.
		if(exechook(NULL,n,hooktab + HkPreKey,1,ektos(kentry.lastkseq,lastkstr,false)) <= MinExit)
			break;
		kentry.lastflag = oldflag;

	 	// If no pre-key hook error (or no hook)...
		if(rc.status == Success) {

			// Get updated key from pre-key hook, if any.
			if(kentry.uselast) {
				kdp = getbind(ek = kentry.lastkseq);
				kentry.uselast = false;
				}

			// Execute key.
			if(execute(ek,kdp,n) <= MinExit)
				break;

		 	// If no key execution error...
			if(rc.status == Success) {

				// execute the user-assigned post-key hook with current n argument, preserving thisflag.
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

// Remove ~ (AttrSeqBegin) sequences from return message if RCTermAttr flag set.  Return Success or Failure.
static int cleanmsg(char **strp,char *msgbuf,size_t bufsz) {
	DStrFab msg;
	char *str = *strp = scriptrc.msg.d_str;

	if(!(scriptrc.flags & RCTermAttr))
		return Success;
	if(dopentrk(&msg) == 0) {
		short c;

		// Copy string to string-fab object, replacing any ~~ in message with ~, ~u, ~#u, and ~U with ", and ~b, ~B, ~r,
		// ~R, and ~0 with nothing (remove).
		while((c = *str++) != '\0') {
			if(c == AttrSeqBegin) {
				if(str[0] == AttrAlt && str[1] == AttrULOn)
					++str;
				switch(*str) {
					case AttrBoldOn:
#if TT_Curses
					case AttrBoldOff:
#endif
					case AttrRevOn:
#if TT_Curses
					case AttrRevOff:
#endif
					case AttrAllOff:
						++str;
						continue;
					case AttrULOn:
					case AttrULOff:
						c = '"';
						// Fall through.
					case AttrSeqBegin:
						++str;
					}
				}
			if(dvizc(c,VBaseDef,&msg) != 0)
				break;
			}
		if(dclose(&msg,sf_string) == 0) {
			strfit(*strp = msgbuf,bufsz - 1,msg.sf_datp->d_str,0);
			return Success;
			}
		}
	return Failure;
	}

// Begin MightEMacs.  Basic tasks are:
//   1. Initialze terminal and data structures.
//   2. Process startup file(s) if found.
//   3. Do command-line processing.
//   4. If no errors, enter user-key-entry loop.
//   5. At loop exit, display any error message and return proper exit code to OS.
int main(int argc,char *argv[]) {
	bool dostart = true;
	char *path;
	Buffer *bufp;
	char helpmsg[50];

	// Do global initializations.
	randseed = seedinit();
	mypid = (unsigned) getpid();
	initchars();
#if MMDebug & Debug_Logfile
	logfile = fopen(Logfile,"w");
#endif
	// Begin using rc (RtnCode): do pre-edit-loop stuff, but use the same return-code mechanism as command calls.
	if(edinit0() == Success &&			// Initialize the return value structures.
	 scancmdline(argc,argv,&dostart) == Success &&	// Scan command line for informational and -n switches.
	 edinit1() == Success &&			// Initialize the core data structures.
	 vtinit() == Success &&				// Initialize the virtual terminal.
	 edinit2() == Success) {			// Initialize buffers, windows, and screens.

		// At this point, we are fully operational (ready to process commands).  Set execpath.
		if(setpath((path = getenv(MMPath_Name)) == NULL ? MMPath_Default : path,false) != Success)
			goto Retn;

		// Run site and user startup files and check for macro error.
		if(dostart && (startup(SiteStartup,false,true) != Success || startup(UserStartup,true,true) != Success))
			goto Retn;

		// Run create-buffer and change-directory hooks.
		if(exechook(NULL,INT_MIN,hooktab + HkCreateBuf,1,curbp->b_bname) != Success ||
		 exechook(NULL,INT_MIN,hooktab + HkChDir,0) != Success)
			goto Retn;

		// Process the command line.
		if(docmdline(argc,argv,helpmsg) == Success) {

			// Set help "return" message if appropriate and enter user-command loop.  The message will only be
			// displayed if a message was not previously set in a startup file.
			if(helpmsg[0] != '\0')
				(void) rcset(Success,RCTermAttr,"%s",helpmsg);
			curwp->w_flags |= WFMode;
			(void) editloop();
			}
		}
Retn:
	// Preserve the return code, close the terminal (ignoring any error), and return to line mode.
	if(rccpy(&scriptrc,&rc) == Panic)
		(void) rcset(Panic,0,NULL);	// No return.
	(void) rcclear(0);
	(void) TTclose(
#if RestoreTerm
	 TermCup |
#endif
	 TermForce);
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
			char *str,wkbuf[512];

			if(cleanmsg(&str,wkbuf,sizeof(wkbuf)) != Success)
				scriptrc.status = ScriptExit;
			fputs(str,stderr);
			fputc('\n',stderr);
			}

		return scriptrc.status == ScriptExit ? 255 : 0;
		}

	// Error or help exit.
	if(scriptrc.status == HelpExit) {
		fputs(scriptrc.helpText,stderr);
		fputc('\n',stderr);
		}
	else {
		char wkbuf[512];

		sprintf(wkbuf,"%s:",text189);
				// "Abort"
		if(strncmp(scriptrc.msg.d_str,wkbuf,strlen(wkbuf)) != 0)
			fprintf(stderr,"%s: ",text0);
					// "Error"
		if(scriptrc.status == OSError)
			fprintf(stderr,"%s, %s\n",strerror(errno),scriptrc.msg.d_str);
		else if(disnull(&scriptrc.msg))
			fprintf(stderr,text253,scriptrc.status);
				// "(return status %d)\n"
		else {
			char *str;

			(void) cleanmsg(&str,wkbuf,sizeof(wkbuf));
			fputs(str,stderr);
			fputc('\n',stderr);
			}
		}
	return 255;
	}

// Give me some help!  Execute help hook.
int help(Datum *rp,int n,Datum **argpp) {
	HookRec *hrp = hooktab + HkHelp;

	return (hrp->h_bufp == NULL) ? rcset(Failure,RCNoFormat,text246) : exechook(rp,n,hrp,0);
						// "Help hook not set"
	}

// Loop through the list of buffers (ignoring macros) and return true if any were changed; otherwise, false.
static bool dirtybuf(void) {
	Buffer *bufp;

	bufp = bheadp;
	do {
		if((bufp->b_flags & (BFMacro | BFChanged)) == BFChanged)
			return true;
		} while((bufp = bufp->b_nextp) != NULL);
	return false;
	}

// Exit (quit) command.  If n != 0 and not default, always quit; otherwise confirm if a buffer has been changed and not written
// out.  If n < 0 and not default, force error return to OS.  Include message if script mode and one is available.
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
		if(terminpYN(text104,&yep) != Success)
				// "Modified buffers exist.  Leave anyway"
			return rc.status;
		if(yep)
			// Force clean exit.
			forceclean = true;
		else
			// User changed his mind... don't exit, no error.
			status = Success;
		if(mlerase(0) != Success)
			return rc.status;
		}

	// Script mode?
	if(opflags & OpScript) {
		Datum *datp;

		// Get return message, if any, and save it if exiting.
		if(buildmsg(&datp,NULL) == Success && status != Success) {
			if(rcclear(0) == Success && !disnn(datp))
				msg = datp->d_str;
			if(!forceclean && dirtybuf())
				status = ScriptExit;	// Forced exit from a script with dirty buffer(s).
			}
		}

	// Force error exit if negative n.
	if(n < 0)
		status = ScriptExit;

	return rcset(status,RCNoFormat,msg);
	}

// Store character c in s len times and return pointer to terminating null.
char *dupchr(char *s,short c,int len) {

	while(len-- > 0)
		*s++ = c;
	*s = '\0';
	return s;
	}

// Write a string to a buffer, centered in the current terminal width.  Return status.
static int center(Buffer *bufp,char *src) {
	int pad = (term.t_ncol - (strlen(src) - attrCount(src,-1,INT_MAX)) - 1) / 2 + 1;
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
			{NULL,0}
			};
	struct liminfo *limp;

	// Get a buffer.
	if(sysbuf(text6,&bufp,BFTermAttr) != Success)
			// "About"
		return rc.status;

	// Get a datum object.
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);

	// Construct the "About MightEMacs" lines.
	if(bappend(bufp,"\n\n") != Success)				// Vertical space.
		return rc.status;
	if(dopenwith(&line,datp,SFClear) != 0 || dputs("~b",&line) != 0)// Turn bold on, expand program name...
		return librcset(Failure);
	str = Myself;
	do {
		if(dputc(*str++,&line) != 0 || dputc(' ',&line) != 0)
			return librcset(Failure);
		} while(*str != '\0');
	if(dunputc(&line) != 0 || dputs("~0",&line) != 0 || dclose(&line,sf_string) != 0)
		return librcset(Failure);
	if(center(bufp,datp->d_str) != Success)				// and center it.
		return rc.status;
	if(bappend(bufp,"") != Success)					// Blank line.
		return rc.status;
	if(dopenwith(&line,datp,SFClear) != 0 ||			// Build "version" line...
	 dputf(&line,"%s %s",text185,Version) != 0 || dclose(&line,sf_string) != 0)
			// "Version"
		return librcset(Failure);
	if(center(bufp,datp->d_str) != Success)				// and center it.
		return rc.status;
	if(bappend(bufp,"") != Success)					// Blank line.
		return rc.status;
	if(center(bufp,ALit_author) != Success)				// Copyright.
			// "(c) Copyright 2018 Richard W. Marinelli <italian389@yahoo.com>"
		return rc.status;

	// Construct the program limits' lines.
	if(bappend(bufp,"\n") != Success)				// Blank lines.
		return rc.status;
	if(center(bufp,ALit_buildInfo) != Success)			// Limits' header.
		// "~bBuild Information~0"
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
	if(dopenwith(&line,datp,SFClear) != 0 ||
	 dputf(&line,"%s%s",Myself,ALit_footer1) != 0 || dclose(&line,sf_string) != 0)
			// " runs on Unix and Linux platforms and is licensed under the"
		return librcset(Failure);
	if(center(bufp,datp->d_str) != Success || center(bufp,ALit_footer2) != Success || center(bufp,ALit_footer3) != Success)
			// "GNU General Public License (GPLv3), which can be viewed at"
			// "http://www.gnu.org/licenses/gpl-3.0.en.html"
		return rc.status;

	// Display results.
	return render(rp,n,bufp,RendNewBuf | RendReset);
	}
