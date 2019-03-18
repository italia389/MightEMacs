// (c) Copyright 2019 Richard W. Marinelli
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
#if LINUX
#include <sys/stat.h>
#endif
#include "plgetswitch.h"
#include "pllib.h"

// Local definitions.
#define ErrRtn		1	// Error return code to OS (otherwise, zero).

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

// Copy a string to open string-fab object, escaping the leading ~ character (AttrSpecBegin) of a terminal attribute sequence if
// needed.  Return -1 if error; otherwise, 0.
int escattrtosf(DStrFab *sfp,char *str) {

	while(*str != '\0')
		if((*str == AttrSpecBegin && dputc(AttrSpecBegin,sfp) != 0) || dputc(*str++,sfp) != 0)
			return -1;
	return 0;
	}

// Copy global return message to open string-fab object, escaping the leading ~ character (AttrSpecBegin) of a terminal attribute
// specification if needed.  Return status.
int escattr(DStrFab *sfp) {
	char *str = rc.msg.d_str;
	if(rc.flags & RCTermAttr) {
		if(dputs(str,sfp) != 0 || dputc(AttrSpecBegin,sfp) != 0 || dputc(AttrAllOff,sfp) != 0)
			goto LibFail;
		}
	else if(escattrtosf(sfp,str) != 0)
LibFail:
		return librcset(Failure);

	return Success;
	}

// Clear return message if RCKeepMsg flag not set, clear return flags, and set return status to Success.
void rcclear(ushort flags) {

	if(!(flags & RCKeepMsg))
		dsetnull(&rc.msg);
	rc.flags = 0;
	rc.status = Success;
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

// Convert current "failure" return message to a "notice"; that is, a message with Success severity and RCNoWrap flag set.  All
// other message flags are preserved.
void rcunfail(void) {

	if(rc.status == Failure) {
		rc.status = Success;
		rc.flags |= RCNoWrap;
		}
	}

// Set a return code and message, with exceptions, and return most severe status.  If new status is less severe than existing
// status, do nothing; otherwise, if status is more severe or status is the same and RCForce flag is set, set new severity and
// message; otherwise (status is the same):
//	If status is Success:
//		- if global 'RtnMsg' mode is disabled, do nothing; otherwise,
//		- if existing message is null or RCHigh flag set (high priority) and existing RCForce and RCHigh flags are
//		  clear, set new message; otherwise, do nothing.
//	If other (non-Success) status:
//		- if existing message is null, set new message; otherwise, do nothing.
//	In all cases, if action is being taken:
//		- new message is set unless fmt is NULL or RCKeepMsg flag is set and message is not null.
//		- current message is cleared if fmt is NULL, RCForce is set, and RCKeepMsg is not set.
//		- new serverity is always set (which may be the same).
// Additionally, the RCMsgSet flag is cleared at the beginning of this routine, then set again if a message is actually set so
// that the calling routine can check the flag and append to the message if desired.
int rcset(int status,ushort flags,char *fmt,...) {

	// Clear RCMsgSet flag.
	rc.flags &= ~RCMsgSet;

	// Check status.  If this status is not more severe, or not force and either (1), Success and not displaying messages;
	// or (2), same status but already have a message, return old one.
	if(status > rc.status)				// Return if new status is less severe.
		goto Retn;
	if(status < rc.status || (flags & RCForce))	// Set new message if more severe status or a force.
		goto Set;

	// Same status.  Check special conditions.
	if(status == Success) {

		// If 'RtnMsg' global mode enabled and either (1), existing message is null; or (2), RCHigh flag set and
		// existing RCForce and RCHigh flags are clear, set new message.
		if((mi.cache[MdIdxRtnMsg]->ms_flags & MdEnabled) &&
		 ((disnull(&rc.msg) || ((flags & RCHigh) && !(rc.flags & (RCForce | RCHigh))))))
			goto Set;
		goto Retn;
		}
	else {
		// Same non-Success status... set only if existing message is null.
		if(disnull(&rc.msg))
			goto Set;
Retn:
		return rc.status;
		}
Set:
	// Save message (if RCKeepMsg flag is not set) and new status.
	if(status == HelpExit) {
		rc.helpText = fmt;
		rc.flags |= RCMsgSet;
		}
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
				(void) ttclose(false);
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
				exit(ErrRtn);
				}
			dsetmemstr(str,&rc.msg);
			rc.flags |= RCMsgSet;
			}
		}
	else if((flags & (RCForce | RCKeepMsg)) == RCForce)
		dsetnull(&rc.msg);

	// Preserve RCNoWrap and RCTermAttr flags if new message was not set.
	rc.flags = (rc.flags & RCMsgSet) ? flags | RCMsgSet :
	 (rc.flags & (RCNoWrap | RCTermAttr)) | flags;
#if MMDebug & Debug_Datum
	fprintf(logfile,"rcset(%d,%.4X,...): set msg '%s', status %d, new flags %.4X\n",
	 status,(uint) flags,rc.msg.d_str,status,(uint) rc.flags);
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
		goto LibFail;
	if(catargs(datp,0,NULL,0) != Success || (prefix == NULL && disnn(datp)))
		return rc.status;
	if(dopenwith(&msg,*datpp,SFClear) != 0 || (prefix != NULL && dputs(prefix,&msg) != 0))
		goto LibFail;
	if(!disnn(datp))
		if((!disempty(&msg) && dputs(": ",&msg) != 0) || dputs(datp->d_str,&msg) != 0)
			goto LibFail;
	if(dclose(&msg,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
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
			si.curwp->w_flags |= WFMode;
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
	si.curwp->w_flags |= WFMode;
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
	si.curwp->w_flags |= WFMode;
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

	ttbeep();
	if(kmacro.km_state == KMRecord)
		si.curwp->w_flags |= WFMode;
	kmacro.km_state = KMStop;
	return rcset(UserAbort,RCNoFormat,text8);
			// "Aborted"
	}

// "abort" command: Call abortinp() to abort processing and build optional exception message.  Enable terminal attributes in
// message if n argument.
int abortOp(Datum *rp,int n,Datum **argpp) {

	(void) abortinp();

	// Short and sweet if called by user pressing a key.
	if(!(si.opflags & OpScript))
		return rc.status;

	// Called from elsewhere (a script)... build a message.
	rcclear(0);
	Datum *datp;
	if(buildmsg(&datp,text189) == Success)
			// "Abort"
		(void) rcset(UserAbort,n == INT_MIN ? RCNoFormat : RCNoFormat | RCTermAttr,datp->d_str);

	return rc.status;
	}

// Message options.
#define MsgForce	0x0001
#define MsgNoWrap	0x0002
#define MsgTermAttr	0x0003

// Concatenate arguments and set result as a return message.  Set as high priority if abs(n) == 1; force if abs(n) > 1.  Parse
// message options from first argument and build message from following argument(s).  Set return value to false if n <= 0;
// otherwise, true.
int message(Datum *rp,int n,Datum **argpp) {
	bool result = (n == INT_MIN || n > 0);
	ushort flags;
	struct MsgOpt {
		char *kw;
		ushort flag;
		} *mop;
	static struct MsgOpt msgtab[] = {
		{"NoWrap",RCNoWrap},
		{"TermAttr",RCTermAttr},
		{NULL,0}};

	// Initialize message flags.
	flags = RCNoFormat;
	if(!result)
		flags |= RCNoWrap;
	if(n != 0 && n != INT_MIN)
		flags |= (abs(n) == 1 ? RCHigh : RCForce);

	// Get option string.
	if(funcarg(rp,ArgFirst | ArgNil1) != Success)
		return rc.status;
	if(!disnn(rp)) {
		char *str0,*str1;

		// Parse options.
		str0 = rp->d_str;
		while((str1 = strparse(&str0,',')) != NULL) {
			if(*str1 == '\0')
				continue;
			mop = msgtab;
			do {
				if(strcasecmp(str1,mop->kw) == 0)
					goto Found;
				} while((++mop)->kw != NULL);
			return rcset(Failure,0,text410,str1);
				// "Invalid message option '%s'"
Found:
			flags |= mop->flag;
			}
		}

	// Build return message from remaining arguments.
	if(needsym(s_comma,true)) {
		Datum *datp;

		if(buildmsg(&datp,NULL) == Success) {
			dsetbool(result,rp);
			(void) rcset(Success,flags,datp->d_str);
			}
		}

	return rc.status;
	}

// Check current buffer state.  Return error if "edit" is true and in an executing buffer or in read-only mode.
int allowedit(bool edit) {

	if(edit) {
		if(bmsrch1(si.curbp,mi.cache[MdIdxRdOnly])) {
			ttbeep();
			return rcset(Failure,0,text109,text58);
				// "%s is in read-only mode","Buffer"
			}
		if(si.curbp->b_mip != NULL && si.curbp->b_mip->mi_nexec > 0) {
			ttbeep();
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
static int execkey(KeyBind *kbp,int n) {
	UnivPtr *univp = &kbp->k_targ;
	Datum *dsinkp;				// For throw-away return value, if any.

	if(dnewtrk(&dsinkp) != 0)
		return librcset(Failure);
	dsetnil(dsinkp);

	// Bound to a macro?
	if(univp->p_type == PtrMacro_O)
		(void) execbuf(dsinkp,n,univp->u.p_bufp,NULL,0);

	// Bound to a command.  Skip execution if n is zero and a repeat count; otherwise, perform "edit" check before
	// invoking it.
	else {
		CmdFunc *cfp = univp->u.p_cfp;
		if(n == 0 && (cfp->cf_aflags & CFNCount))
			return rc.status;
		if(allowedit(cfp->cf_aflags & CFEdit) == Success)
			(void) execCF(dsinkp,n,cfp,0,0);
		}

	// False return?
	if(rc.status == Success && dsinkp->d_type == dat_false) {
		if(kmacro.km_state == KMPlay)
			(void) rcset(Failure,RCNoFormat | RCKeepMsg,text300);
							// "False return"
		else
			(void) rcset(Failure,0,NULL);
		}

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
	UnivPtr univ;
	KeyBind *kbp;
	CmdFunc *cfp;
	ModeSpec *msp,**mspp;
	ModeGrp *mgp1,*mgp2;
	ssize_t index;
	static char myname[] = "edinit1";
	struct ainfo {
		char *alias;
		UnivPtr targ;
		} *ap;
	static struct ainfo aliases[] = {
		{"cd",{PtrCmd,cftab + cf_chgDir}},
		{"pwd",{PtrCmd,cftab + cf_showDir}},
		{"quit",{PtrCmd,cftab + cf_exit}},
		{"require",{PtrCmd,cftab + cf_xeqFile}},
		{NULL,{PtrNul,NULL}}};
	struct mginfo {
		char *name;
		char *desc;
		ushort usect;
		} *mgip;
	static struct mginfo mginit[] = {
		{"Typeover",MGLit_Typeover,2},
		{NULL,NULL}};
	struct {
		char *name;
		char *desc;
		ushort flags;
		short grp;
		} *mip,minit[] = {
			{mi.MdLitASave,MLit_AutoSave,MdGlobal | MdLocked,-1},
			{mi.MdLitATerm,MLit_AutoTerm,0,-1},
			{mi.MdLitBak,MLit_Backup,0,-1},
			{mi.MdLitClob,MLit_Clobber,MdGlobal | MdLocked | MdHidden,-1},
			{mi.MdLitCol,MLit_ColDisp,MdHidden | MdInLine,-1},
			{mi.MdLitExact,MLit_Exact,MdGlobal | MdEnabled,-1},
			{mi.MdLitFence,MLit_Fence,MdGlobal | MdHidden | MdEnabled,-1},
			{mi.MdLitHScrl,MLit_HorzScroll,MdGlobal | MdEnabled,-1},
			{mi.MdLitLine,MLit_LineDisp,MdHidden | MdInLine,-1},
			{mi.MdLitOver,MLit_Overwrite,MdLocked,1},
			{mi.MdLitRdOnly,MLit_ReadOnly,MdLocked,-1},
			{mi.MdLitRegexp,MLit_Regexp,MdGlobal,-1},
			{mi.MdLitRepl,MLit_Replace,MdLocked,1},
			{mi.MdLitRtnMsg,MLit_RtnMsg,MdGlobal | MdLocked | MdHidden | MdEnabled,-1},
			{mi.MdLitSafe,MLit_SafeSave,MdGlobal,-1},
			{mi.MdLitWkDir,MLit_WorkDir,MdGlobal | MdLocked | MdHidden | MdInLine,-1},
			{mi.MdLitWrap,MLit_Wrap,0,-1},
			{NULL,NULL,0,-1}};

	// Initialize "undelete" buffer and keyboard macro.
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

	// Add labels to the rings.
	kring.r_rname = text436;
		// "Kill"
	sring.r_rname = text78;
		// "Search"
	rring.r_rname = text84;
		// "Replace"
	sring.r_ename = rring.r_ename = text311;
		// " pattern"

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

	// Initialize the key cache and the "exec" hash table with all command and function names.
	if((exectab = hnew(NFuncs,0.7,1.1)) == NULL)
		return librcset(Failure);
	ckp = corekeys;
	cfp = cftab;
	do {
		univ.p_type = (cfp->cf_aflags & CFFunc) ? PtrFunc : (cfp->cf_aflags & CFHidden) ? PtrPseudo : PtrCmd;
		univ.u.p_cfp = cfp;
		if(execnew(cfp->cf_name,&univ) != Success)
			return rc.status;

		// Store frequently-used bindings in key cache.
		if(cfp->cf_aflags & CFUniq) {
			ckp->ek = ((kbp = getpentry(&univ)) == NULL) ? 0 : kbp->k_code;
			ckp->id = cfp - cftab;
			++ckp;
			}
		} while((++cfp)->cf_name != NULL);

	// Initialize the alias list.
	ap = aliases;
	do {
		if(aupdate(ap->alias,OpCreate,&ap->targ) != Success)
			return rc.status;
		} while((++ap)->alias != NULL);

	// Initialize the mode group table.
	mgp1 = NULL;
	mgip = mginit;
	do {
		if(mgcreate(mgip->name,mgp1,mgip->desc,&mgp2) != Success)
			return rc.status;
		mgp2->mg_usect = mgip->usect;
		mgp2->mg_flags = 0;
		mgp1 = mgp2;
		} while((++mgip)->name != NULL);

	// Initialize the mode table.
	ainit(&mi.modetab);
	mspp = mi.cache;
	mip = minit;
	index = 0;
	do {
		if(mdcreate(mip->name,index++,mip->desc,mip->flags,&msp) != Success)
			return rc.status;
		*mspp++ = msp;
		if(mip->grp > 0)
			msp->ms_group = mi.gheadp;
		} while((++mip)->name != NULL);

	// Clear the search tables and initialize the word list.
	if(newspat("",&srch.m,NULL) == Success && newspat("",&rematch,NULL) == Success && newrpat("",&srch.m) == Success &&
	 newrpat("",&rematch) == Success)
		(void) setwlist(wordlistd);

	return rc.status;
	}

// Initialize all of the buffers, windows, and screens.  Return status.
static int edinit2(void) {

	// Allocate the initial buffer and screen.
	ainit(&buftab);
	if(bfind(buffer1,BS_Create,0,&si.curbp,NULL) != Success || sfind(1,si.curbp,&si.sheadp) != Success)
		return rc.status;

	// Set the global screen and window pointers.
	si.curwp = si.wheadp = (si.cursp = si.sheadp)->s_curwp = si.sheadp->s_wheadp;

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
static int startup(char *sfname,ushort hflag,bool ignore) {
	char *fname;			// Resulting filename to execute.
	Datum *dsinkp;			// For throw-away return value, if any.

	if(pathsearch(sfname,hflag,(void *) &fname,NULL) != Success)
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
	return execfile(dsinkp,fname,INT_MIN,ArgFirst);
	}

// Build a command line with one argument in given string-fab object and execute it.  If fullQuote is true, quote() argument;
// otherwise, just wrap a "" pair around it.
int runcmd(Datum *dsinkp,DStrFab *cmdp,char *cname,char *arg,bool fullQuote) {

	if(dnewtrk(&dsinkp) != 0)
		goto LibFail;

	// Concatenate command or macro name with arguments.
	if(dopentrk(cmdp) != 0 || dputs(cname,cmdp) != 0 || dputc(' ',cmdp) != 0)
		goto LibFail;
	if(fullQuote) {
		if(quote(cmdp,arg,true) != Success)
			return rc.status;
		}
	else if(dputc('"',cmdp) != 0 || dputs(arg,cmdp) != 0 || dputc('"',cmdp) != 0)
		goto LibFail;

	// Close string-fab object and execute command line.
	if(dclose(cmdp,sf_string) != 0)
LibFail:
		return librcset(Failure);
	return execestmt(dsinkp,cmdp->sf_datp->d_str,TokC_ComLine,NULL);
	}

// Process -buf-mode (bufp not NULL) or -global-mode (bufp NULL) switch value.
static int modeswitch(SwitchResult *swrp,Buffer *bufp) {
	size_t len;
	ushort offset;
	int narg;
	char *end,*value = swrp->value;
	char wkbuf[32];

	for(;;) {
		// Find beginning and end of next mode string.
		if((end = strchr(value,',')) == NULL)
			end = strchr(value,'\0');

		// Set parameters, extract name, and call chgmode().
		if(*value == '^') {
			narg = -2;		// Enable partial-name matching in chgmode().
			offset = 1;
			}
		else {
			narg = 3;		// Enable partial-name matching in chgmode().
			offset = 0;
			}
		len = end - value - offset;
		sprintf(wkbuf,"%.*s",(int)(len > sizeof(wkbuf) - 1 ? sizeof(wkbuf) - 1 : len),value + offset);
		if(chgmode(wkbuf,narg,bufp,NULL,NULL) != Success)
			return rcset(FatalError,0,NULL);

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
		if(bswitch(bufp,0) != Success || bactivate(bufp) != Success)
			return rc.status;

	// Delete first buffer if it is not reserved for standard input, it exists, is empty, and is not being displayed.
	if(stdinflag == 0 && (bufp = bsrch(buffer1,NULL)) != NULL && bempty(bufp) && bufp->b_nwind == 0)
		(void) bdelete(bufp,0);
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
		goto LibFail;
	do {
		if(dputc(delim,&cmd) != 0)
			goto LibFail;
		if(asc_long(*argv,NULL,true)) {
			if(dputs(*argv,&cmd) != 0)
				goto LibFail;
			}
		else if(quote(&cmd,*argv,true) != Success)
			return rc.status;
		delim = ',';
		++argv;
		} while(--argc > 0);
	if(dclose(&cmd,sf_string) != 0)
LibFail:
		return librcset(Failure);

	// and execute it.
	return execestmt(dsinkp,cmd.sf_datp->d_str,TokC_ComLine,NULL);
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
		goto LibFail;

	// Get command switches.
	--argc;
	++argv;
	while((rcode = getswitch(&argc,&argv,&swdp,&result)) > 0) {
		displayHelp = false;
		switch(rcode) {
			case 2:		// -dir for change working directory.
				str = "chgDir ";
				n = true;		// Escape string.
				goto DoCmd;
			case 3:		// -exec to execute a command line.
				if(execestmt(dsinkp,result.value,TokC_ComLine,NULL) != Success)
					return rc.status;
				break;
			case 4:		// -global-mode for set/clear global mode(s).
				if(modeswitch(&result,NULL) != Success)
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
				if(startup(*argv + 1,0,false) != Success)
					return rc.status;
				++argv;
				}
			else {					// Process a data file.
DoFile:
				// Set up a buffer for the file and set it to inactive.
				str = *argv++;
				if(stdinflag > 0) {
					if(bfind(buffer1,BS_Create | BS_Hook,0,&bufp,NULL) != Success)
						return rc.status;
					}
				else if(bfind(str,BS_Create | BS_Hook | BS_Force | BS_Derive,0,&bufp,NULL) != Success)
					return rc.status;
				if(stdinflag > 0)		// If using standard input...
					stdinflag = -1;		// disable the flag so it can't be used again.
				else if(setfname(bufp,str) != Success)
					return rc.status;
				bufp->b_flags &= ~BFActive;
				if(cRdOnly && bmset(bufp,mi.cache[MdIdxRdOnly],false,NULL) != Success)
					return rc.status;

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
							if(modeswitch(&result,bufp) != Success)
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
							if(!aRdOnly)
								bmsrch(bufp,true,1,mi.cache[MdIdxRdOnly]);
							else if(bmset(bufp,mi.cache[MdIdxRdOnly],false,NULL) != Success)
								return rc.status;
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
	if(strcmp(si.curbp->b_bname,buffer1) != 0)
		displayHelp = false;

	// Process startup gotos and searches.
	if(gotoflag) {
		char wkbuf[NWork];

		sprintf(wkbuf,gline >= 0 ? "gotoLine %ld" : "endBuf() || %ld => forwLine",gline);
		if(execestmt(dsinkp,wkbuf,TokC_ComLine,NULL) < Failure)
			return rc.status;
		}
	else if(searchflag && huntForw(dsinkp,1,NULL) != Success)
		return rc.status;

	// Build help message if appropriate.
	*helpmsg = '\0';
	if(displayHelp) {
		KeyBind *kbp;
		struct cmdinfo {
			Datum *datp;
			UnivPtr cmd;
			} *cip;
		static struct cmdinfo ci[] = {
			{NULL,{PtrCmd,cftab + cf_help}},
			{NULL,{PtrCmd,cftab + cf_exit}},
			{NULL,{PtrCmd,NULL}}};

		cip = ci;
		if(dnewtrk(&cip[0].datp) != 0 || dsalloc(cip[0].datp,16) != 0 ||
		 dnewtrk(&cip[1].datp) != 0 || dsalloc(cip[1].datp,16) != 0)
			goto LibFail;
		do {
			*cip->datp->d_str = '\0';
			if((kbp = getpentry(&cip->cmd)) != NULL) {
				ektos(kbp->k_code,cip->datp->d_str,true);

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

		if(*ci[0].datp->d_str != '\0' && *ci[1].datp->d_str != '\0')
			sprintf(helpmsg,text41,ci[0].datp->d_str,ci[1].datp->d_str);
				// "Enter ~#u%s~U for help, ~#u%s~U to quit"
		}

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Prepare to insert one or more characters at point.  Delete characters first if in replace or overwrite mode and not at end
// of line.  Assume n > 0.  Return status.
int overprep(int n) {
	bool mdRepl = bmsrch1(si.curbp,mi.cache[MdIdxRepl]);

	if(mdRepl || bmsrch1(si.curbp,mi.cache[MdIdxOver])) {
		int count = 0;
		Dot *dotp = &si.curwp->w_face.wf_dot;
		do {
			if(dotp->off == dotp->lnp->l_used)
				break;

			// Delete character if we are in replace mode, character is not a tab, or we are at a tab stop.
			if(mdRepl || (dotp->lnp->l_text[dotp->off] != '\t' ||
			 (getccol(NULL) + count) % si.htabsize == (si.htabsize - 1)))
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
static int execute(ushort ek,KeyBind *kbp,int n) {
	char keybuf[16];

	// If the keystroke is bound...
	if(kbp != NULL) {

		// Don't reset the command-execution flags on a prefix.
		if(kbp->k_targ.p_type == PtrPseudo && (kbp->k_targ.u.p_cfp->cf_aflags & CFPrefix))
			return execkey(kbp,n);

		kentry.thisflag = 0;
		(void) execkey(kbp,n);

		// Remove end-macro key sequence from keyboard macro, if applicable, and check for empty buffer.
		if(rc.status == Success && kbp->k_targ.u.p_cfp == cftab + cf_endKeyMacro) {
			int len = (kbp->k_code & Prefix) ? 2 : 1;
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

	// Keystroke not bound... attempt self-insert abs(n) times.  Ignore "Over" and "Repl" buffer modes if n < 0.
	if(ek >= 0x20 && ek < 0xFF) {
		bool forceIns = false;
		if(n == INT_MIN)
			n = 1;
		else if(n < 0) {
			n = -n;
			forceIns = true;
			}
		else if(n == 0)
			return rc.status;

		if(allowedit(true) != Success)		// Don't allow if in read-only mode.
			return rc.status;
		kentry.thisflag = 0;

		// In replace or overwrite mode?
		if(!forceIns && overprep(n) != Success)
			return rc.status;

		// Insert the character.
		if(linsert(n,ek) == Success) {

			// Do fence matching.
			if((ek == '}' || ek == ')' || ek == ']') && modeset(MdIdxFence,NULL))
				fmatch(ek);
AutoCheck:
			// Save all changed buffers if auto-save mode and keystroke count reached.
			if((mi.cache[MdIdxASave]->ms_flags & MdEnabled) && si.gasave > 0 && --si.gacount == 0) {
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
	ttbeep();
	kentry.lastflag = 0;						// Fake last flags.
	return rcset(Failure,RCTermAttr,text14,ektos(ek,keybuf,true));	// Complain.
		// "~#u%s~U not bound"
	}

// Interactive command-processing routine.  Read key sequences and process them.  Return if program exit.
static int editloop(void) {
	ushort ek;		// Extended key read from user.
	int n;			// Numeric argument.
	ushort oldflag;		// Old last flag value.
	KeyBind *kbp;
	RtnCode lastrc;		// Return code from last key executed.
	Datum hkrtn;		// For calling pre/postKey hooks.
#if DCaller
	static char myname[] = "editloop";

	dinit(&lastrc.msg,myname);
	dinit(&hkrtn,myname);
#else
	dinit(&lastrc.msg);
	dinit(&hkrtn);
#endif
	dsetnull(&lastrc.msg);
	si.opflags &= ~OpStartup;

	// If $lastKeySeq was set by a startup macro, simulate it being entered as the first key.
	if(kentry.uselast)
		goto JumpStart;
	goto ChkMsg;

	// Do forever.
	for(;;) {
		rcclear(0);
		garbpop(NULL);				// Throw out all the garbage.
		if(agarbfree() != Success)
			break;

		// Fix up the screen.
		if(update(INT_MIN) <= MinExit)
			break;

		// Any message from last loop to display?
		if(!disnull(&lastrc.msg)) {
			ushort flags = MLHome | MLFlush;
			if(lastrc.status == Success && !(lastrc.flags & RCNoWrap))
				flags |= MLWrap;
			if(lastrc.flags & RCTermAttr)
				flags |= MLTermAttr;
			if(mlputs(flags,lastrc.msg.d_str) != Success ||
			 ttmove(si.cursp->s_cursrow,si.cursp->s_curscol) != Success || ttflush() != Success)
				break;
			}

		// Get the next key from the keyboard or $lastKeySeq.
		gmset(mi.cache[MdIdxRtnMsg]);
		if(kentry.uselast) {
JumpStart:
			kbp = getbind(ek = kentry.lastkseq);
			kentry.uselast = false;
			}
		else {
			if(getkseq(false,&ek,&kbp,true) <= MinExit)
				break;
			if(rc.status != Success)
				goto Fail;
			}

		// If there is something on the message line, clear it.
		if(mlerase() != Success)
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
				if(mlprintf(MLHome | MLFlush,text209,neg ? -n : n) != Success)
							// "Arg: %d"
					break;

				// Get next key.
				if(getkey(true,&ek,false) != Success)
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
				if(getkseq(false,&ek,&kbp,true) != Success)
					goto Retn;
				break;
				}

			// Change sign of n if needed.
			if(neg)
				n = -n;

			// Clear the message line.
			if(mlerase() != Success)
				break;
			}

		// Execute the user-assigned pre-key hook with n argument, preserving lastflag.
		oldflag = kentry.lastflag;
		if(exechook(&hkrtn,n,hooktab + HkPreKey,0) <= MinExit)
			break;
		kentry.lastflag = oldflag;

	 	// If no pre-key hook error (or no hook)...
		if(rc.status == Success) {

			// Get updated key from pre-key hook, if any.
			if(kentry.uselast) {
				kbp = getbind(ek = kentry.lastkseq);
				kentry.uselast = false;
				}

			// Execute key.
			if(execute(ek,kbp,n) <= MinExit)
				break;

		 	// If no key execution error...
			if(rc.status == Success) {

				// execute the user-assigned post-key hook with n argument and pre-key return value (if any),
				// preserving thisflag.
				oldflag = kentry.thisflag;
				if(exechook(&hkrtn,n,hooktab + HkPostKey,0x21,&hkrtn) <= MinExit)
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

		dsetnil(&hkrtn);
		}
Retn:
	return rc.status;
	}

// Return a random seed.
ulong seedinit(void) {

	return time(NULL) & ULONG_MAX;
	}

// Convert return message to multiple lines (at ", in") and remove ~ (AttrSpecBegin) sequences if RCTermAttr flag set.  Return
// Success or Failure.
static int cleanmsg(char **strp) {
	DStrFab msg;
	char *str = *strp = scriptrc.msg.d_str;
	size_t len = strlen(text229);
			// ", in"

	if(dopentrk(&msg) == 0) {
		short c;

		// Copy string to string-fab object, replacing any ~~ in message with ~, ~u, ~#u, and ~U with ", and ~b, ~B,
		// ~<n>c, ~C, ~r, ~R, and ~Z with nothing (remove) if RCTermAttr flag set.  Also insert newlines in text229
		// string (from macerror() function), if any, so that error callback is easier to read.
		while((c = *str++) != '\0') {
			if(c == AttrSpecBegin) {
				if(scriptrc.flags & RCTermAttr) {
					if(str[0] == AttrAlt && str[1] == AttrULOn)
						++str;
					else
						while(*str >= '0' && *str <= '9')
							++str;
					switch(*str) {
						case AttrBoldOn:
						case AttrBoldOff:
						case AttrColorOn:
						case AttrColorOff:
						case AttrRevOn:
						case AttrRevOff:
						case AttrAllOff:
							++str;
							continue;
						case AttrULOn:
						case AttrULOff:
							c = '"';
							// Fall through.
						case AttrSpecBegin:
							++str;
						}
					}
				}
			else if(strncmp(str - 1,text229,len) == 0) {
				if(dputf(&msg,",\n  %s",text229 + 1) != 0)
					break;
				str += len - 1;
				continue;
				}
			if(dvizc(c,VBaseDef,&msg) != 0)
				break;
			}
		if(dclose(&msg,sf_string) == 0) {
			*strp = msg.sf_datp->d_str;
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
	si.randseed = seedinit();
	si.mypid = (unsigned) getpid();
	initchars();
#if MMDebug & Debug_Logfile
	logfile = fopen(Logfile,"w");
	(void) setlinebuf(logfile);
#endif
	// Begin using rc (RtnCode): do pre-edit-loop stuff, but use the same return-code mechanism as command calls.
	if(edinit0() == Success &&			// Initialize the return value structures.
	 scancmdline(argc,argv,&dostart) == Success &&	// Scan command line for informational and -n switches.
	 edinit1() == Success &&			// Initialize the core data structures.
	 vtinit() == Success &&				// Initialize the virtual terminal.
	 edinit2() == Success) {			// Initialize buffers, windows, and screens.

		// At this point, we are fully operational (ready to process commands).  Set $execPath.
		if((path = getenv(MMPath_Name)) == NULL)
			path = MMPath;
		if(setpath(path,false) != Success)
			goto Retn;

		// Run site and user startup files and check for macro error.
		if(dostart && (startup(SiteStartup,0,true) != Success || startup(UserStartup,XPHomeOnly,true) != Success))
			goto Retn;

		// Run create-buffer and change-directory hooks.
		if(exechook(NULL,INT_MIN,hooktab + HkCreateBuf,1,si.curbp->b_bname) != Success ||
		 exechook(NULL,INT_MIN,hooktab + HkChDir,0) != Success)
			goto Retn;

		// Process the command line.
		if(docmdline(argc,argv,helpmsg) == Success) {

			// Set help "return" message if appropriate and enter user-command loop.  The message will only be
			// displayed if a message was not previously set in a startup file.
			if(helpmsg[0] != '\0')
				(void) rcset(Success,RCHigh | RCTermAttr,"%s",helpmsg);
			si.curwp->w_flags |= WFMode;
			(void) editloop();
			}
		}
Retn:
	// Preserve the return code, close the terminal (ignoring any error), and return to line mode.
	if(rccpy(&scriptrc,&rc) == Panic)
		(void) rcset(Panic,0,NULL);	// No return.
	rcclear(0);
	(void) ttclose(false);
#if MMDebug & Debug_Logfile
#ifdef HDebug
	hstats(exectab);
#endif
	(void) fclose(logfile);
#endif
	// Exit MightEMacs.
	if(scriptrc.status == UserExit || scriptrc.status == ScriptExit) {
		Datum *datp;
		Array *aryp = &buftab;

		// Display names (in line mode) of any files saved via quickExit().
		while((datp = aeach(&aryp)) != NULL) {
			bufp = bufptr(datp);
			if(bufp->b_flags & BFQSave)
				fprintf(stderr,text193,bufp->b_fname);
						// "Saved file \"%s\"\n"
			}

		// Display message, if any.
		if(!disnull(&scriptrc.msg)) {
			char *str;

			if(cleanmsg(&str) != Success)
				scriptrc.status = ScriptExit;
			fputs(str,stderr);
			fputc('\n',stderr);
			}

		return scriptrc.status == ScriptExit ? ErrRtn : 0;
		}

	// Error or help exit.
	if(scriptrc.status == HelpExit) {
		fputs(scriptrc.helpText,stderr);
		fputc('\n',stderr);
		}
	else {
		char wkbuf[16];

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

			(void) cleanmsg(&str);
			fputs(str,stderr);
			fputc('\n',stderr);
			}
		}
	return ErrRtn;
	}

// Give me some help!  Execute help hook.
int help(Datum *rp,int n,Datum **argpp) {
	HookRec *hrp = hooktab + HkHelp;

	return (hrp->h_bufp == NULL) ? rcset(Failure,RCNoFormat,text246) : exechook(rp,n,hrp,0);
						// "Help hook not set"
	}

// Loop through the list of buffers (ignoring macros) and return true if any were changed; otherwise, false.
static bool dirtybuf(void) {
	Datum *datp;
	Buffer *bufp;
	Array *aryp = &buftab;

	while((datp = aeach(&aryp)) != NULL) {
		bufp = bufptr(datp);
		if((bufp->b_flags & (BFMacro | BFChanged)) == BFChanged)
			return true;
		}
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
		if(mlerase() != Success)
			return rc.status;
		}

	// Script mode?
	if(si.opflags & OpScript) {
		Datum *datp;

		// Get return message, if any, and save it if exiting.
		if(buildmsg(&datp,NULL) == Success && status != Success) {
			rcclear(0);
			if(!disnn(datp))
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

// Write a string to an open string-fab object, centered in the current terminal width.  Return status.
static int center(DStrFab *rptp,char *src) {
	int indent = (term.t_ncol - (strlen(src) - attrCount(src,-1,INT_MAX))) / 2;
	if(dputc('\n',rptp) != 0 || (indent > 0 && dputf(rptp,"%*s",indent,"") != 0) || dputs(src,rptp) != 0)
		(void) librcset(Failure);
	return rc.status;
	}

// Build and pop up a buffer containing "about the editor" information.  Render buffer and return status.
int aboutMM(Datum *rp,int n,Datum **argpp) {
	Buffer *listp;
	DStrFab rpt;
	char *str;
	int i,maxlab;
	char footer1[sizeof(Myself) + sizeof(ALit_footer1) + 1];	// Longest line displayed.
	char wkbuf[term.t_mcol + 1];
	struct liminfo {
		char *label;
		int value;
		} *limp;
	static struct liminfo limits[] = {
		{ALit_maxCols,TT_MaxCols},
		{ALit_maxRows,TT_MaxRows},
		{ALit_maxTab,MaxTab},
		{ALit_maxPathname,MaxPathname},
		{ALit_maxBufName,MaxBufName},
		{ALit_maxMGName,MaxMGName},
		{ALit_maxUserVar,MaxVarName},
		{ALit_maxTermInp,NTermInp},
		{NULL,0}};

	// Get new buffer and string-fab object.
	if(sysbuf(text6,&listp,BFTermAttr) != Success)
			// "About"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Construct the "About MightEMacs" lines.
	sprintf(footer1,"%s%s",Myself,ALit_footer1);
			// " runs on Unix and Linux platforms and is licensed under the"

	if(dputs("\n\n",&rpt) != 0)					// Vertical space.
		goto LibFail;
	if(rpthdr(&rpt,Myself,false,footer1,NULL,NULL) != Success)	// Editor name in color.
		return rc.status;
	if(dputc('\n',&rpt) != 0)					// Blank line.
		goto LibFail;
	sprintf(wkbuf,"%s %s",text185,Version);				// Build "version" line...
			// "Version"
	if(center(&rpt,wkbuf) != Success)				// and center it.
		return rc.status;
	if(dputc('\n',&rpt) != 0)					// Blank line.
		goto LibFail;
	if(center(&rpt,ALit_author) != Success)				// Copyright.
			// "(c) Copyright 2019 Richard W. Marinelli <italian389@yahoo.com>"
		return rc.status;

	// Construct the program limits' lines.
	if(dputs("\n\n",&rpt) != 0)					// Blank lines.
		goto LibFail;
	sprintf(wkbuf,"%c%c%s%c%c",AttrSpecBegin,AttrBoldOn,
	 ALit_buildInfo,AttrSpecBegin,AttrBoldOff);			// Limits' header.
		// "Build Information"
	if(center(&rpt,wkbuf) != Success)
		return rc.status;
	if(dputc('\n',&rpt) != 0)					// Blank line.
		goto LibFail;

	// Find longest limits' label and set i to indentation required to center it and its value.
	limp = limits;
	maxlab = 0;
	do {
		if((i = strlen(limp->label)) > maxlab)
			maxlab = i;
		} while((++limp)->label != NULL);
	if((i = (term.t_ncol - (maxlab + 10)) / 2) < 0)
		i = 0;
	++maxlab;

	// Loop through limit values and add to report.
	str = dupchr(wkbuf,' ',i);
	limp = limits;
	do {
		i = strlen(limp->label);
		sprintf(str,"%-*s%9d",maxlab,limp->label,limp->value);
		str[i] = ':';
		if(dputc('\n',&rpt) != 0 || dputs(wkbuf,&rpt) != 0)
			goto LibFail;
		} while((++limp)->label != NULL);

	// Write platform, credit, and license blurb.
	if(dputc('\n',&rpt) != 0)					// Blank line.
		goto LibFail;
	if(center(&rpt,footer1) != Success || center(&rpt,ALit_footer2) != Success || center(&rpt,ALit_footer3) != Success)
			// "GNU General Public License (GPLv3), which can be viewed at"
			// "http://www.gnu.org/licenses/gpl-3.0.en.html"
		return rc.status;

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		goto LibFail;
	if(bappend(listp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n,listp,RendNewBuf | RendReset);
LibFail:
	return librcset(Failure);
	}
