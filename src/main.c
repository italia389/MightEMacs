// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// main.c	Entry point for the MightEMacs text editor.
//
// This file contains the main (top level) routine and some keyboard processing code.
//
// Programming Notes:
// 1. After program initialization, the core operation of the editor is as follows: keys are read in editLoop(), system or user
// commands are called with a numeric argument, and a status code is returned.  If no numeric argument is specified by the user,
// DefN is used as the default (and many commands will have a default behavior in that case).  The called routines typically
// call (many) other routines, which may include hook routines.  Most of these functions return a status code as well.  The
// return code is usually Success or Failure; however, it can be more severe (see std.h for a list), such as UserAbort or
// FatalError.  Any code worse than UserAbort will cause program termination.  The most severe return code from the most distant
// function in the call chain will ultimately be returned to editLoop(), where it will be dealt with; that is, a message will be
// displayed if applicable before beginning the next loop.  This means that a more severe return code and message will always
// override a less severe one (which is enforced by the rsset() function).
//
// 2. There is a special return code, NotFound, which may be returned directly from a function, bypassing rsset().  This code
// typically indicates an item was not found or the end of a list was reached, and it requires special attention.  It is the
// responsibility of the calling function to decide what to do with it.  The functions that may return NotFound are:
//
// Function(s)							Returns NotFound, bypassing rsset() when...
// ---------------------------------------------------------	---------------------------------------------------------------
// backChar(), forwChar(), backLine(), forwLine(), edelc()	Move or delete is out of buffer.
// getSym(), parseTok()						Token not found and no error.
// getBufname()							Buffer not found, interactive mode, and operation is OpQuery.
// startup()							Script file not found.
// regScan(), scan()						Search string not found.
// ereaddir()							No entries left.

// Load global data in std.h.
#define MainData
#include "std.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#if LINUX
#include <sys/stat.h>
#endif
#include "cxl/getSwitch.h"
#include "cxl/lib.h"

// Local definitions.
#define ErrorRtnCode	1	// Error return code to OS (otherwise, zero).

// Flags used by startup routines.
#define ExecSite	0x0001		// Execute site startup file.
#define ExecUser	0x0002		// Execute user startup file.

static const char				// Switch names.
	*sw_buf_mode[] = {"buf-mode", "B", NULL},
	*sw_copyright[] = {"copyright", "C", NULL},
	*sw_D[] = {"D", NULL},
	*sw_dir[] = {"dir", "d", NULL},
	*sw_exec[] = {"exec", "e", NULL},
	*sw_global_mode[] = {"global-mode", "G", NULL},
	*sw_help[] = {"help", NULL},
	*sw_inp_delim[] = {"inp-delim", "i", NULL},
	*sw_no_read[] = {"no-read", "R", NULL},
	*sw_no_startup[] = {"no-startup", "n", NULL},
	*sw_no_user_startup[] = {"no-user-startup", "N", NULL},
	*sw_otp_delim[] = {"otp-delim", "o", NULL},
	*sw_path[] = {"path", NULL},
	*sw_r[] = {"r", NULL},
	*sw_rw[] = {"rw", NULL},
	*sw_search[] = {"search", "s", NULL},
	*sw_shell[] = {"shell", "S", NULL},
	*sw_usage[] = {"usage", "?", NULL},
	*sw_version[] = {"version", "V", NULL},
	*sw_xeq[] = {"xeq", "x", NULL};
static Switch cmdSwitchTable[] = {		// Command switch table.
/*1*/	{sw_copyright, SF_NoArg},
/*2*/	{sw_D, SF_NoArg},
/*3*/	{sw_dir, SF_RequiredArg},
/*4*/	{sw_exec, SF_RequiredArg | SF_AllowRepeat},
/*5*/	{sw_global_mode, SF_RequiredArg | SF_AllowRepeat},
/*6*/	{sw_help, SF_NoArg},
/*7*/	{sw_inp_delim, SF_RequiredArg},
/*8*/	{sw_no_read, SF_NoArg},
/*9*/	{sw_no_startup, SF_NoArg},
/*10*/	{sw_no_user_startup, SF_NoArg},
/*11*/	{sw_otp_delim, SF_RequiredArg},
/*12*/	{sw_path, SF_RequiredArg | SF_AllowNullArg},
/*13*/	{sw_r, SF_NoArg},
/*14*/	{sw_shell, SF_NoArg},
/*15*/	{sw_usage, SF_NoArg},
/*16*/	{sw_version, SF_NoArg},
/*17*/	{sw_xeq, SF_RequiredArg | SF_AllowRepeat}};

#define CSW_copyright		1
#define CSW_D			2
#define CSW_dir			3
#define CSW_exec		4
#define CSW_global_mode		5
#define CSW_help		6
#define CSW_inp_delim		7
#define CSW_no_read		8
#define CSW_no_startup		9
#define CSW_no_user_startup	10
#define CSW_otp_delim		11
#define CSW_path		12
#define CSW_r			13
#define CSW_shell		14
#define CSW_usage		15
#define CSW_version		16
#define CSW_xeq			17

static Switch argSwitchTable[] = {		// Argument switch table.
/*1*/	{NULL, SF_NumericSwitch},
/*2*/	{NULL, SF_NumericSwitch | SF_PlusType},
/*3*/	{sw_buf_mode, SF_RequiredArg | SF_AllowRepeat},
/*4*/	{sw_r, SF_NoArg},
/*5*/	{sw_rw, SF_NoArg},
/*6*/	{sw_search, SF_RequiredArg}};

#include "bind.h"
#include "exec.h"
#include "file.h"
#include "search.h"
#include "var.h"

// Load global data in cmd.h.
#define CmdData
#include "cmd.h"

// Return true if in interactive mode.
bool interactive(void) {

	return (sess.opFlags & (OpScript | OpUserCmd)) != OpScript;
	}

// Clear return message if RSKeepMsg flag not set, clear return flags, and set return status to Success.
void rsclear(ushort flags) {

#if MMDebug & Debug_MsgLnRtn
	fprintf(logfile, "rsclear(%.4hx)\n", flags);
#endif
	if(!(flags & RSKeepMsg))
		dsetnull(&sess.rtn.msg);
	sess.rtn.flags = 0;
	sess.rtn.status = Success;
	}

// Copy one return status object to another.  Return status.
static int rscpy(RtnStatus *pDest, RtnStatus *pSrc) {

	pDest->flags = pSrc->flags;
	if((pDest->status = pSrc->status) == HelpExit)
		dxfer(&pDest->msg, &pSrc->msg);
	else if(dcpy(&pDest->msg, &pSrc->msg) != 0)
		(void) libfail();
#if MMDebug & Debug_MsgLnRtn
	fprintf(logfile, "rscpy(): copied message \"%s\", status %hd, flags %.4x\n",
	 pDest->msg.str, pDest->status, pDest->flags);
#endif
	return sess.rtn.status;
	}

// Convert current "failure" return message to a "notice"; that is, a message with Success severity and RSNoWrap flag set.  All
// other message flags are preserved.
void rsunfail(void) {

	if(sess.rtn.status == Failure) {
		sess.rtn.status = Success;
		sess.rtn.flags |= RSNoWrap;
		}
	}

// Set a return code and message, with exceptions, and return most severe status.  If new status is less severe than existing
// status, do nothing; otherwise, if status is more severe or status is the same and RSForce flag is set, set new severity and
// message; otherwise (status is the same):
//	If status is Success:
//		- if global 'RtnMsg' mode is disabled and RSOverride flag not set, do nothing; otherwise,
//		- if existing message is null or RSHigh flag set (high priority) and existing RSForce and RSHigh flags are
//		  clear, set new message; otherwise, do nothing.
//	If other (non-Success) status:
//		- if existing message is null, set new message; otherwise, do nothing.
//	In all cases, if action is being taken:
//		- new message is set unless fmt is NULL or RSKeepMsg flag is set and message is not null.
//		- current message is cleared if fmt is NULL, RSForce is set, and RSKeepMsg is not set.
//		- new serverity is always set (which may be the same).
int rsset(int status, ushort flags, const char *fmt, ...) {
	bool newMsgSet = false;

#if MMDebug & Debug_MsgLnRtn
	fprintf(logfile, "rsset(%d, %.4hx, \"%s\", ...), current flags %.4hx, msg \"%s\"\n",
	 status, flags, fmt, sess.rtn.flags, sess.rtn.msg.str);
#endif
	// Check status.  If this status is not more severe, or not force and either (1), Success and not displaying messages;
	// or (2), same status but already have a message, return old one.
	if(status > sess.rtn.status)				// Return if new status is less severe.
		goto Retn;
	if(status < sess.rtn.status || flags & RSForce)		// Set new message if more severe status or a force.
		goto Set;

	// Same status.  Check special conditions.
	if(status == Success) {

		// If RSOverride flag set or 'RtnMsg' global mode enabled and either (1), existing message is null; or (2),
		// RSHigh flag set and existing RSForce and RSHigh flags are clear, set new message.
		if((flags & RSOverride || mode.cache[MdIdxRtnMsg]->flags & MdActive) &&
		 ((disnull(&sess.rtn.msg) || flags & RSHigh && !(sess.rtn.flags & (RSForce | RSHigh)))))
			goto Set;
		goto Retn;
		}
	else {
		// Same non-Success status... set only if existing message is null.
		if(disnull(&sess.rtn.msg))
			goto Set;
Retn:
		return sess.rtn.status;
		}
Set:
#if MMDebug & Debug_MsgLnRtn
	fprintf(logfile, "    -> attempting to set \"%s\" message...\n", fmt);
#endif
	// Save message (if RSKeepMsg flag not set) and new status.
	if(status == HelpExit) {
		dadoptstr((char *) fmt, &sess.rtn.msg);
		newMsgSet = true;
		}
	else if(fmt != NULL) {
		if(!(flags & RSKeepMsg) || disnull(&sess.rtn.msg)) {
			va_list varArgList;
			char *str;

			if(status != Panic) {
				if(flags & RSNoFormat)
					str = strdup(fmt);
				else {
					va_start(varArgList, fmt);
					(void) vasprintf(&str, fmt, varArgList);
					va_end(varArgList);
					}
				}

			// If Panic serverity or vasprintf() failed, panic!  That is, we're most likely out of memory, so bail
			// out with "great expediency".
			if(status == Panic || str == NULL) {
				(void) tclose(false);
				fprintf(stderr, "%s: ", text189);
						// "Abort"
				if(status == Panic) {
					va_start(varArgList, fmt);
					vfprintf(stderr, fmt, varArgList);
					va_end(varArgList);
					}
				else
					fprintf(stderr, text94, "rsset");
							// "%s(): Out of memory!"
				fputc('\n', stderr);
				exit(ErrorRtnCode);
				}
			dadoptstr(str, &sess.rtn.msg);
			newMsgSet = true;
			}
		}
	else if((flags & (RSForce | RSKeepMsg)) == RSForce)
		dsetnull(&sess.rtn.msg);

	// Preserve RSNoWrap and RSTermAttr flags if new message was not set.
	sess.rtn.flags = newMsgSet ? flags : (sess.rtn.flags & (RSNoWrap | RSTermAttr)) | flags;
#if MMDebug & Debug_MsgLnRtn
	fprintf(logfile, "rsset(%d, %.4x, ...): set msg '%s', status %d, new flags %.4x\n",
	 status, (uint) flags, sess.rtn.msg.str, status, (uint) sess.rtn.flags);
#endif
	return (sess.rtn.status = status);
	}

// Append text to return message and return status.
int rscat(const char *fmt, ...) {
	va_list varArgList;
	char *str;

	va_start(varArgList, fmt);
	(void) vasprintf(&str, fmt, varArgList);
	va_end(varArgList);
	if(str == NULL)
		return rsset(Panic, 0, text94, "rscat");
			// "%s(): Out of memory!"
	if(disnull(&sess.rtn.msg))
		dadoptstr(str, &sess.rtn.msg);
	else {
		DFab msg;

		if(dopenwith(&msg, &sess.rtn.msg, FabAppend) != 0 || dputs(str, &msg, 0) != 0 || dclose(&msg, FabStr) != 0)
			return libfail();
		free((void *) str);
		}
	return sess.rtn.status;
	}

// Save current return message for ReturnMsg system constant (code and flags not used).  Return status.
int rssave(void) {

	// Copy message to sess.scriptRtn.
	return dsetstr(sess.rtn.msg.str, &sess.scriptRtn.msg) != 0 ? libfail() : sess.rtn.status;
	}

// If current status not Success, save and clear it so it can be restored later.
void rspush(short *pStatus, ushort *pFlags) {

	if(sess.rtn.status == Success)
		*pStatus = SHRT_MAX;
	else {
		*pStatus = sess.rtn.status;
		*pFlags = sess.rtn.flags;
		rsclear(RSKeepMsg);
		}
	}

// Restore status.
void rspop(short oldStatus, ushort oldFlags) {

	if(oldStatus != SHRT_MAX) {
		sess.rtn.status = oldStatus;
		sess.rtn.flags = oldFlags;
		}
	}

// Set and return proper status from a failed CXL library call.
static int librsset(int status) {

	return rsset(cxlExcep.flags & ExcepMem ? Panic : status, RSNoFormat, cxlExcep.msg);
	}

// Set and return Failure status from a failed CXL library call.
int libfail(void) {

	return librsset(Failure);
	}

// Set and return FatalError status from a failed CXL library call.
static int libfatal(void) {

	return librsset(FatalError);
	}

// Copy a non-zero-length byte string to open fabrication object pFab with CvtVizChar flag, escaping AttrSpecBegin (~)
// characters.  Return -1 if error, otherwise zero.
int esctofab(const char *str, size_t len, DFab *pFab) {

	do {
		if((*str == AttrSpecBegin && dputc(AttrSpecBegin, pFab, 0) != 0) || dputc(*str++, pFab, CvtVizChar) != 0)
			return -1;
		} while(--len > 0);
	return 0;
	}

// Copy global return message to open fabrication object, escaping AttrSpecBegin (~) characters in terminal attribute
// specifications if needed.  Return status.
int escMsgAttr(DFab *pFab) {
	char *str = sess.rtn.msg.str;
	if(sess.rtn.flags & RSTermAttr) {
		if(dputs(str, pFab, 0) != 0 || dputc(AttrSpecBegin, pFab, 0) != 0 || dputc(AttrAllOff, pFab, 0) != 0)
			goto LibFail;
		}
	else {
		while(*str != '\0')
			if((*str == AttrSpecBegin && dputc(AttrSpecBegin, pFab, 0) != 0) || dputc(*str++, pFab, 0) != 0)
LibFail:
				return libfail();
		}

	return sess.rtn.status;
	}

// Concatenate any function arguments via catArgs() and save prefix and result in *ppDatum.  Return status.
static int buildMsg(Datum **ppDatum, const char *prefix, int minRequired) {
	DFab msg;
	Datum *pDatum;

	if((pDatum = dnewtrack()) == NULL || (*ppDatum = dnewtrack()) == NULL)
		goto LibFail;
	if(catArgs(pDatum, minRequired, NULL, 0) != Success || (prefix == NULL && isNN(pDatum)))
		return sess.rtn.status;
	if(dopenwith(&msg, *ppDatum, FabClear) != 0 || (prefix != NULL && dputs(prefix, &msg, 0) != 0))
		goto LibFail;
	if(!isNN(pDatum))
		if((!dfabempty(&msg) && dputs(": ", &msg, 0) != 0) || dputs(pDatum->str, &msg, 0) != 0)
			goto LibFail;
	if(dclose(&msg, FabStr) != 0)
LibFail:
		(void) libfail();
	return sess.rtn.status;
	}

// Abort.  Beep the beeper.  Kill off any macro that is in progress.  Called by "abort" command and any user-input
// function if user presses abort key (usually ^G).  Set and return UserAbort status.
int abortInp(void) {

	tbeep();
	if(macro.state == MacRecording)
		sess.pCurWind->face.wflags |= WFMode;
	macro.state = MacStopped;
	return rsset(UserAbort, RSNoFormat, text8);
			// "Aborted"
	}

// "abort" command: Call abortInp() to abort processing and build optional exception message.  Enable terminal attributes in
// message if n argument.
int abortOp(Datum *pRtnVal, long n, Datum **args) {

	(void) abortInp();

	// Short and sweet if called by user pressing a key.
	if(!(sess.opFlags & OpScript))
		return sess.rtn.status;

	// Called from elsewhere (a script)... build a message.
	rsclear(0);
	Datum *pDatum;
	if(buildMsg(&pDatum, text189, 0) == Success)
			// "Abort"
		(void) rsset(UserAbort, n == DefN ? RSNoFormat : RSNoFormat | RSTermAttr, pDatum->str);

	return sess.rtn.status;
	}

// Message options.
#define MsgForce	0x0001
#define MsgHigh		0x0002
#define MsgNoWrap	0x0004
#define MsgTermAttr	0x0008

// Concatenate arguments and set result as a return message.  Return status.  First argument is option string if n argument;
// otherwise, it is skipped.  Message is built from following argument(s).  Return value (pRtnVal) is set to false if "Fail"
// option specified, otherwise true.
int message(Datum *pRtnVal, long n, Datum **args) {
	Datum *pDatum;
	bool rtnVal = true;
	ushort rtnFlags = RSNoFormat | RSOverride;
	uint argFlags = ArgFirst;
	OptHdr *pOptHdr = cmdFuncTable[cf_message].pOptHdr;

	// Get options and set flags.
	if(pOptHdr->ctrlFlags & OHdrFound) {
		if(!needSym(s_comma, true))
			return sess.rtn.status;
		rtnFlags |= pOptHdr->selFlags;
		if(pOptHdr->optTable[4].ctrlFlags & OptSelected) {	// "NoWrap"
			rtnFlags |= RSNoWrap;
			rtnVal = false;
			}
		argFlags = 0;
		dclear(pRtnVal);
		}

	// Build return message from remaining arguments.
	if(buildMsg(&pDatum, NULL, 1) == Success) {
		dsetbool(rtnVal, pRtnVal);
		(void) rsset(Success, rtnFlags, pDatum->str);
		}

	return sess.rtn.status;
	}

// Check state of current edit buffer.  Return error if "edit" is true and buffer is read only or being executed.
int allowEdit(bool edit) {

	if(edit) {
		if(sess.edit.pBuf->flags & BFReadOnly) {
			tbeep();
			return rsset(Failure, 0, text284, text276, text460);
				// "Cannot %s %s buffer", "modify", "read-only"
			}
		if(sess.edit.pBuf->pCallHdr != NULL && sess.edit.pBuf->pCallHdr->execCount > 0) {
			tbeep();
			return rsset(Failure, 0, text284, text276, text238);
				// "Cannot %s %s buffer", "modify", "executing"
			}
		}

	return sess.rtn.status;
	}

// Pad a string to indicated length and return pointer to terminating null.
char *pad(char *s, uint len) {
	char *str1, *str2;
	uint curLen;

	str1 = strchr(s, '\0');
	if((curLen = str1 - s) < len) {
		str2 = str1 + (len - curLen);
		do {
			*str1++ = ' ';
			} while(str1 < str2);
		*str1 = '\0';
		}
	return str1;
	}

// Return true if have fence character and other conditions are met that warrant display of matching fence, otherwise false.
static bool hiliteFence(short fence, bool rightOnly) {

	// Valid fence?
	switch(fence) {
		case '<':
		case '>':
			if(!modeSet(MdIdxFence2, NULL) || (fence == '<' && rightOnly))
				return false;
			break;
		case '{':
		case '(':
		case '[':
			if(rightOnly)
				return false;
			// Fall through.
		case '}':
		case ')':
		case ']':
			if(!modeSet(MdIdxFence1, NULL) && !modeSet(MdIdxFence2, NULL))
				return false;
			break;
		default:
			return false;
		}

	// Success.
	return true;
	}

// Execute a system or user command bound to given key (not a hook).  Return status.
static int execKey(KeyBind *pKeyBind, long n) {
	ExecPtr *pExec = &pKeyBind->cmd;
	Datum *pSink;				// For throw-away return value, if any.

	if((pSink = dnewtrack()) == NULL)
		return libfail();

	// Bound to a user command?
	if(pExec->type == PtrUserCmd) {

		// Save and restore OpUserCmd flag.
		ushort oldUserCmd = sess.opFlags & OpUserCmd;
		sess.opFlags |= OpUserCmd;
		(void) execBuf(pSink, n, pExec->u.pBuf, NULL, 0);
		sess.opFlags = (sess.opFlags & ~OpUserCmd) | oldUserCmd;
		}
	else {
		// Bound to a system command.  Skip execution if n is zero and a repeat count; otherwise, perform "edit" check
		// before invoking it.
		CmdFunc *pCmdFunc = pExec->u.pCmdFunc;
		if(n == 0 && pCmdFunc->attrFlags & CFNCount)
			return sess.rtn.status;
		if(allowEdit(pCmdFunc->attrFlags & CFEdit) == Success)
			(void) execCmdFunc(pSink, n, pCmdFunc, 0, 0);
		}

	// False return?
	if(sess.rtn.status == Success && pSink->type == dat_false) {
		if(macro.state == MacPlaying)
			(void) rsset(Failure, RSNoFormat | RSKeepMsg, text300);
							// "False return"
		else
			(void) rsset(Failure, 0, NULL);
		}

	return sess.rtn.status;
	}

// Initialize global Datum objects and return status.
static int editInit0(void) {

	dinit(&iVar.format);
	dinit(&sess.rtn.msg);
	dsetnull(&sess.rtn.msg);
	dinit(&sess.scriptRtn.msg);
	dsetnull(&sess.scriptRtn.msg);
	dinit(&sess.qSearchPat);
	dsetnull(&sess.qSearchPat);
	dinit(&sess.qReplPat);
	dsetnull(&sess.qReplPat);
	return dsetstr("%d", &iVar.format) != 0 ? libfail() : sess.rtn.status;
	}

// Set a list of characters to be considered in a word.  The word table is assumed to be already cleared.  Return status.
static int setWordList(const char *wordList) {
	const char *str;
	DFab fab;

	// Expand the new value...
	if(strExpand(&fab, wordList) != Success)
		return sess.rtn.status;

	// and for each character in the list, set that element in the table.
	str = fab.pDatum->str;
	do {
		wordChar[(int) *str++] = true;
		} while(*str != '\0');

	return sess.rtn.status;
	}

// Initialize all of the core data structures and return status.
static int editInit1(void) {
	size_t k;
	CoreKey *pCoreKey;
	ExecPtr exec;
	KeyBind *pKeyBind;
	CmdFunc *pCmdFunc;
	ModeSpec *pModeSpec, **ppModeSpec;
	ModeGrp *pModeGrp1, *pModeGrp2;
	struct AliasInfo {
		const char *alias;
		ExecPtr targ;
		} *aliasInfo;
	static struct AliasInfo aliases[] = {
		{"cd",{PtrSysCmd, cmdFuncTable + cf_chdir}},
		{"lcstr",{PtrSysFunc, cmdFuncTable + cf_lowerCaseStr}},
		{"ln",{PtrSysCmd, cmdFuncTable + cf_linkFile}},
		{"quit",{PtrSysCmd, cmdFuncTable + cf_exit}},
		{"require",{PtrSysCmd, cmdFuncTable + cf_xeqFile}},
		{"rm",{PtrSysCmd, cmdFuncTable + cf_delFile}},
		{"size",{PtrSysFunc, cmdFuncTable + cf_length}},
		{"ucstr",{PtrSysFunc, cmdFuncTable + cf_upperCaseStr}},
		{NULL,{PtrNull, NULL}}};
	struct ModeGrpInfo {
		const char *name;
		const char *descrip;
		ushort useCount;
		} *pModeGrpInfo;
	static struct ModeGrpInfo modeGrpTable[] = {
		{"Fence", MGLit_Fence, 2},
		{"Typeover", MGLit_Typeover, 2},
		{NULL, NULL}};
	struct {
		const char *name;
		const char *descrip;
		ushort flags;
		short group;
		} *modeInit, modeInitTable[] = {
			{"ASave", MLit_AutoSave, MdGlobal | MdLocked, -1},
			{"ATerm", MLit_AutoTerm, 0, -1},
			{"Bak", MLit_Backup, 0, -1},
			{"BufCount", MLit_BufCount, MdGlobal | MdLocked | MdHidden, -1},
			{"Clob", MLit_Clobber, MdGlobal | MdLocked | MdHidden, -1},
			{"Col", MLit_ColDisp, MdHidden | MdInLine, -1},
			{"Exact", MLit_Exact, MdGlobal | MdActive, -1},
			{"Fence1", MLit_Fence1, MdGlobal | MdHidden | MdActive, 1},
			{"Fence2", MLit_Fence2, MdGlobal | MdHidden, 1},
			{"HScrl", MLit_HorzScroll, MdGlobal | MdActive, -1},
			{"Line", MLit_LineDisp, MdHidden | MdInLine, -1},
			{"Over", MLit_Overwrite, MdLocked, 2},
			{"ReadOnly", MLit_ReadOnly, MdGlobal | MdLocked, -1},
			{"Regexp", MLit_Regexp, MdGlobal, -1},
			{"Repl", MLit_Replace, MdLocked, 2},
			{"RtnMsg", MLit_RtnMsg, MdGlobal | MdLocked | MdHidden | MdActive, -1},
			{"Safe", MLit_SafeSave, MdGlobal, -1},
			{"WkDir", MLit_WorkDir, MdGlobal | MdLocked | MdHidden | MdInLine, -1},
			{"Wrap", MLit_Wrap, 0, -1},
			{NULL, NULL, 0, -1}};

	// Initialize macro parameters.
	dinit(&macro.macro1.name);
	dinit(&macro.macro2.name);
	hinit(&macro.index.table, 1, 0);	// Minimum size.
	hinit(&macro.shortcuts, 1, 0);		// Minimum size.
	if(growMacro(macro.pActiveMacro = &macro.macro1) != Success ||
	 growMacro(macro.pStageMacro = &macro.macro2) != Success)
		return sess.rtn.status;

	// Add labels to the rings.
	ringTable[RingIdxDel].ringName = text26;
		// "Delete"
	ringTable[RingIdxDel].entryName = text444;
		// " ring item"
	ringTable[RingIdxKill].ringName = text436;
		// "Kill"
	ringTable[RingIdxSearch].ringName = text78;
		// "Search"
	ringTable[RingIdxRepl].ringName = text84;
		// "Replace"
	ringTable[RingIdxSearch].entryName = ringTable[RingIdxRepl].entryName = text311;
		// " pattern"

	// Load all the key bindings.
	if(initBindings() != Success)
		return sess.rtn.status;

	// Initialize the key cache and the "exec" hash with all command and function names.
	k = elementsof(cmdFuncTable);
	hinit(&execTable, k + k / 3, 0);
	if(hconfig(&execTable, CfgBuild, 0.7, 1.1) != 0)
		return libfail();
	pCoreKey = coreKeys;
	pCmdFunc = cmdFuncTable;
	do {
		exec.type = pCmdFunc->attrFlags & CFFunc ? PtrSysFunc :
		 pCmdFunc->attrFlags & CFHidden ? PtrPseudo : PtrSysCmd;
		exec.u.pCmdFunc = pCmdFunc;
		if(execNew(pCmdFunc->name, &exec) != Success)
			return sess.rtn.status;

		// Store frequently-used bindings in key cache.
		if(pCmdFunc->attrFlags & CFUniq) {
			pCoreKey->extKey = ((pKeyBind = findBindPtr(&exec)) == NULL) ? 0 : pKeyBind->extKey;
			pCoreKey->id = pCmdFunc - cmdFuncTable;
			++pCoreKey;
			}
		} while(++pCmdFunc < pCmdFuncEnd);

	// Initialize the global constant and variable hashes.
	if(cvinit() != Success)
		return sess.rtn.status;
	hinit(&fileLinkKeepers, 1, 0);

	// Initialize the alias list.
	aliasInfo = aliases;
	do {
		if(editAlias(aliasInfo->alias, OpCreate, &aliasInfo->targ) != Success)
			return sess.rtn.status;
		} while((++aliasInfo)->alias != NULL);

	// Initialize the mode group table.
	pModeGrp1 = NULL;
	pModeGrpInfo = modeGrpTable;
	do {
		if(mgcreate(pModeGrpInfo->name, pModeGrp1, pModeGrpInfo->descrip, &pModeGrp2) != Success)
			return sess.rtn.status;
		pModeGrp2->useCount = pModeGrpInfo->useCount;
		pModeGrp2->flags = 0;
		pModeGrp1 = pModeGrp2;
		} while((++pModeGrpInfo)->name != NULL);

	// Initialize the mode table.
	hinit(&mode.idx.table, ModeTableSize, PropIgnoreCase);
	ppModeSpec = mode.cache;
	modeInit = modeInitTable;
	do {
		if(mcreate(modeInit->name, modeInit->descrip, modeInit->flags, &pModeSpec) != Success)
			return sess.rtn.status;
		*ppModeSpec++ = pModeSpec;
		if(modeInit->group > 0)
			pModeSpec->pModeGrp = (modeInit->group == 1) ? mode.grpHead : mode.grpHead->next;
		} while((++modeInit)->name != NULL);

	// Clear the search tables and initialize the word list.
	if(newSearchPat("", &bufSearch.match, NULL, false) == Success && newSearchPat("", &strMatch, NULL, false) == Success &&
	 newReplPat("", &bufSearch.match, false) == Success && newReplPat("", &strMatch, false) == Success)
		(void) setWordList(WordChars);

	return sess.rtn.status;
	}

// Initialize all of the buffers, windows, and screens.  Return status.
static int editInit2(void) {

	// Allocate the initial buffer and screen.
	hinit(&buf.table, BufTableSize, 0);
	if(bfind(buffer1, BN_Create, 0, &sess.cur.pBuf, NULL) != Success || sfind(1, sess.cur.pBuf, &sess.scrnHead) != Success)
		return sess.rtn.status;

	// Set the global screen and window pointers.
	sess.edit.pBuf = sess.cur.pBuf;
	sess.pCurWind = sess.windHead = (sess.pCurScrn = sess.scrnHead)->pCurWind = sess.scrnHead->windHead;
	sess.edit.pFace  = sess.cur.pFace = &sess.pCurWind->face;

	return sess.rtn.status;
	}

// Set execPath global variable using allocated memory.  Return status.
int setExecPath(const char *path) {
	char *str;
	Datum datum;
	DFab fab;
	char pathname[PATH_MAX + 1];

	// Save path to a fabrication object with each directory converted to an absolute pathname.
	dinit(&datum);
	if(dopen(&fab) != 0)
		goto LibFail;
	while(*path != '\0') {
		if(*path == ':') {
			if(dputc(*path++, &fab, 0) != 0)
				goto LibFail;
			}
		else {
			// Get next directory path.
			str = (char *) path;
			do {
				++str;
				} while(*str != '\0' && *str != ':');
			if(*path == '/') {
				if(dputmem(path, str - path, &fab, 0) != 0)
					goto LibFail;
				}
			else {
				if(dsetsubstr(path, str - path, &datum) != 0)
					goto LibFail;
				if(realpath(datum.str, pathname) == NULL) {
					(void) rsset(Failure, 0, text404, datum.str, strerror(errno));
						// "Cannot resolve path \"%s\": %s"
					goto ErrExit;
					}
				if(dputs(pathname, &fab, 0) != 0)
					goto LibFail;
				}
			path = str;
			}
		}

	// Finished expanding path.  Set it and clean up.
	if(dclose(&fab, FabStrRef) != 0)
LibFail:
		return libfail();
	if(execPath != NULL)
		free((void *) execPath);
	execPath = fab.pDatum->str;
	dfree(fab.pDatum);
ErrExit:
	dclear(&datum);

	return sess.rtn.status;
	}

// Execute a script file with arguments and return status.  Called for -shell (-S) and -xeq (-x) switches, and site and user
// startup files.  If argList is NULL (-xeq switch), "filename" contains a script filename plus optional arguments and is used
// directly as the argument(s) to the "xeqFile" command (with the script filename quoted).  If argList is not NULL, an "xeqFile"
// command line is built from argCount and argList (skipping first argument in argList) and executed.
static int runScript(char *filename, int argCount, char *argList[]) {
	DFab cmd;
	Datum *pSink;

	if((pSink = dnewtrack()) == NULL || dopentrack(&cmd) != 0 || dputs("xeqFile ", &cmd, 0) != 0)
		goto LibFail;
	if(argList == NULL) {
		char *str = filename;

		// Use existing command line.  Pick off initial filename, quote it, and add a comma.
		while(*str != '\0' && *str != ' ' && *str != '\t')
			++str;
		if(dputsubstr(filename, str - filename, &cmd, CvtLang) != 0)
			goto LibFail;
		if(*str != '\0')
			if(dputc(',', &cmd, 0) != 0 || dputs(str, &cmd, 0) != 0)
				goto LibFail;
		}
	else {
		// Build command line.
		if(dputs(filename, &cmd, CvtLang) != 0)
			goto LibFail;
		while(--argCount > 0)
			if(dputc(',', &cmd, 0) != 0 || dputs(*++argList, &cmd, CvtLang) != 0)
				goto LibFail;
		}
	if(dclose(&cmd, FabStr) != 0)
LibFail:
		return libfail();

	// Have command line in fabrication object.  Execute it.
	return execExprStmt(pSink, cmd.pDatum->str, 0, NULL);
	}

// Execute a startup file, given filename, optional XP_HomeOnly (search HOME only) flag, and command line arguments.  If file
// not found, ignore it.  Return status.
static int startup(const char *filename, ushort flag, int argCount, char *argList[]) {
	char *filename1;	// Filename to execute.

	// Look for script file on "execPath".  If found, execute the sucker; otherwise, return (no error).
	return pathSearch(filename, flag, (void *) &filename1, NULL) != Success || filename1 == NULL ? sess.rtn.status :
	 runScript(filename1, argCount, argList);
	}

// Build a command line with one argument per given flags and execute it with return messages disabled.  If RunQFull flag is
// set, quote argument; if RunQWrap flag is set, wrap argument in double quotes; otherwise, leave it as is (unquoted).
// If cmdSuffix not NULL, append it directly to "cmdPrefix + ' ' + arg".  Return status.
int runCmdline(Datum *pRtnVal, const char *cmdPrefix, const char *arg, const char *cmdSuffix, uint flags) {
	DFab cmd;

	// Concatenate command name with arguments.
	if(dopentrack(&cmd) != 0 || dputs(cmdPrefix, &cmd, 0) != 0 || dputc(' ', &cmd, 0) != 0)
		goto LibFail;
	if(flags & RunQFull) {
		if(dputs(arg, &cmd, CvtLang) != 0)
			goto LibFail;
		}
	else if((flags & RunQWrap && dputc('"', &cmd, 0) != 0) ||
	 dputs(arg, &cmd, 0) != 0 || flags & RunQWrap && dputc('"', &cmd, 0 != 0))
		goto LibFail;
	if(cmdSuffix != NULL && dputs(cmdSuffix, &cmd, 0) != 0)
		goto LibFail;

	// Close fabrication object, disable "RtnMsg" mode, execute command line, and restore the mode.
	if(dclose(&cmd, FabStr) != 0)
LibFail:
		return libfail();
	ushort oldMsgFlag = mode.cache[MdIdxRtnMsg]->flags & MdActive;
	if(oldMsgFlag)
		clearGlobalMode(mode.cache[MdIdxRtnMsg]);
	(void) execExprStmt(pRtnVal, cmd.pDatum->str, 0, NULL);
	if(oldMsgFlag)
		setGlobalMode(mode.cache[MdIdxRtnMsg]);
	return sess.rtn.status;
	}

// Process -buf-mode (pBuf not NULL) or -global-mode (pBuf NULL) switch value.
static int modeSwitch(SwitchResult *pResult, Buffer *pBuf) {
	size_t len;
	ushort offset;
	int nArg;
	char *end, *value = pResult->arg;
	char workBuf[32];

	for(;;) {
		// Find beginning and end of next mode string.
		if((end = strchr(value, ',')) == NULL)
			end = strchr(value, '\0');

		// Set parameters, extract name, and call chgMode().
		if(*value == '^') {
			nArg = -2;		// Enable partial-name matching in chgMode1().
			offset = 1;
			}
		else {
			nArg = 3;		// Enable partial-name matching in chgMode1().
			offset = 0;
			}
		len = end - value - offset;
		sprintf(workBuf, "%.*s", (int)(len > sizeof(workBuf) - 1 ? sizeof(workBuf) - 1 : len), value + offset);
		if(chgMode1(workBuf, nArg, pBuf, NULL, NULL) != Success)
			return rsset(FatalError, 0, NULL);

		// Onward...
		if(*end == '\0')
			break;
		value = end + 1;
		}
	return sess.rtn.status;
	}

// Determine which buffer to "land on" prior to entering edit loop, and read its file if appropriate.  pBuf is associated with
// a file on command line (or NULL if none), depending on whether certain argument switches were specified.  Return status.
static int startBuf(Buffer *pBuf, short stdInpFlag, bool readFirst) {

	// Switch to first file (buffer) specified on command line if auto-load is enabled.
	if(pBuf != NULL && readFirst)
		if(bswitch(pBuf, 0) != Success || (!(pBuf->flags & BFActive) && bactivate(pBuf) != Success))
			return sess.rtn.status;

	// Delete first buffer if it is not reserved for standard input, it exists, is empty, and is not being displayed.
	if(stdInpFlag == 0 && (pBuf = bsrch(buffer1, NULL)) != NULL && bempty(pBuf) && pBuf->windCount == 0)
		(void) bdelete(pBuf, 0);
	return sess.rtn.status;
	}

// Build and return given type of help information in *pInfo.  Return status.
int helpInfo(char **pInfo, enum h_type t, const char *binName) {

	if(t == h_version) {
		if(asprintf(pInfo, "%s %s (GPLv3) [linked with %s, %s]", Myself, Version, cxlvers(), xrevers()) < 0)
			goto Err;
		}
	else if(t == h_copyright) {
		if(asprintf(pInfo, "%s %s", Myself, Copyright) < 0)
			goto Err;
		}
	else {		// h_usage or h_help
		char *usage;

		if(asprintf(&usage, "%s%s%s%s%s", Usage[0], binName, Usage[1], binName, Usage[2]) < 0)
			goto Err;
		if(t == h_usage)
			*pInfo = usage;
		else {
			if(asprintf(pInfo, "%s%s%s%s", Myself, Help1, usage, Help2) < 0)
Err:
				return rsset(Panic, 0, text94, "helpInfo");
					// "%s(): Out of memory!"
			free((void *) usage);
			}
		}
	return sess.rtn.status;
	}

// Display given type of help information and exit (return HelpExit status).
static int helpExit(enum h_type t, const char *binName) {
	char *info;

	return helpInfo(&info, t, binName) == Success ? rsset(HelpExit, 0, info) : sess.rtn.status;
	}

// Scan command line arguments, looking for any of the informational switches: -copyright, -help, -usage, or -version.  If one
// of the switches is found, return helpExit(); otherwise, return status.
static int procInfoSwitches(int argCount, char *argList[]) {
	char *binName;			// My binary name.
	short *pHelpSwitch;
	int rtnCode;
	SwitchResult result;
	Switch *pSwitch = cmdSwitchTable;
	static short helpSwitchTable[] = {
		CSW_copyright, CSW_version, CSW_usage, CSW_help, 0};

	// Get my name and scan command line arguments.
	binName = cname(*argList++, CNBasename | CNPtrOnly);
	--argCount;
	while((rtnCode = getSwitch(&argCount, &argList, &pSwitch, elementsof(cmdSwitchTable), &result)) > 0) {
		pHelpSwitch = helpSwitchTable;
		do {
			if(rtnCode == *pHelpSwitch)
				return helpExit((enum h_type)(pHelpSwitch - helpSwitchTable), binName);
			} while(*++pHelpSwitch != 0);
		}

	return (rtnCode < 0) ? libfail() : sess.rtn.status;
	}

// Scan command line arguments, looking for -D, -dir, -no-start, -no-user-start, and -path switches and (1), change current
// directory if -D or -dir switch found (and also change filename pointer in argList if -D switch); (2), clear ExecSite and
// ExecUser flags in *pFlags if -no-start found; (3), clear ExecUser flag in *pFlags if -no-user-start found; and (4), set
// *pPath to switch value if -path found, otherwise NULL.
static int procPreSwitches(int argCount, char *argList[], ushort *pFlags, char **pPath) {
	Datum *pDatum;
	int rtnCode;
	bool foundD = false;
	SwitchResult result;
	Switch *pSwitch = cmdSwitchTable;

	// Set default return value.
	*pPath = NULL;
	if((pDatum = dnewtrack()) == NULL)
		goto LibFail;

	// Scan command line arguments.
	--argCount;
	++argList;
	while((rtnCode = getSwitch(&argCount, &argList, &pSwitch, elementsof(cmdSwitchTable), &result)) > 0) {
		switch(rtnCode) {
			case CSW_D:			// -D
				foundD = true;
				break;
			case CSW_dir:			// -d
				if(runCmdline(pDatum, "chdir", result.arg, NULL, RunQFull) != Success)
					return sess.rtn.status;
				break;
			case CSW_no_startup:		// -n
				*pFlags &= ~(ExecSite | ExecUser);
				break;
			case CSW_no_user_startup:	// -N
				*pFlags &= ~ExecUser;
				break;
			case CSW_path:			// -path
				*pPath = result.arg;
				break;
			}
		}
	if(rtnCode < 0)
		goto LibFail;

	// Process -D switch if found.
	if(foundD && argCount > 0) {
		char *dirname;

		// Get directory component of filename, if any.
		dirname = cname(*argList, CNDirname);
		if(*dirname != '\0') {
			if(runCmdline(pDatum, "chdir", dirname, NULL, RunQFull) == Success)
				*argList += strlen(dirname) + 1;
			}
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Process command line arguments, skipping switches processed by procInfoSwitches() and procPreSwitches(), and return status.
// Note that special processing is done for the -shell (-S) switch.  For example, if file "/home/jack/bin/testit" contained:
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
// The runScript() function would be called with 'home/jack/bin/testit' as the first argument in argList.
static int procCmdLine(int argCount, char *argList[]) {
	int rtnCode;
	uint oflags;
	ushort fileActionCt = 0;	// Count of data files.
	Buffer *pBuf;			// Temp buffer pointer.
	Buffer *pBuf1 = NULL;		// Buffer pointer to data file to read first.
	bool cRdOnly = false;		// Command-level read-only file?
	bool aRdOnly;			// Argument-level read-only file?
	bool aRdWrite;			// Argument-level read/write file?
	bool gotoFlag = false;		// Go to a line at start?
	long goLine;			// If so, what line?
	bool searchFlag = false;	// Search for a pattern at start?
	short stdInpFlag = 0;		// Read from standard input at start?
	bool shellScript = false;	// Running as shell script?
	char *str;
	Datum *pSink;
	bool readFirst = true;		// Read first file?
	SwitchResult result;
	Switch *pSwitch = cmdSwitchTable;

	goLine = 1;			// Line and character to go to.
	if((pSink = dnewtrack()) == NULL)
		goto LibFail;

	// Get command switches.
	--argCount;
	++argList;
	while((rtnCode = getSwitch(&argCount, &argList, &pSwitch, elementsof(cmdSwitchTable), &result)) > 0) {
		switch(rtnCode) {
			case CSW_exec:
				if(execExprStmt(pSink, result.arg, ParseStmtEnd, NULL) != Success)
					return sess.rtn.status;
				break;
			case CSW_global_mode:
				if(modeSwitch(&result, NULL) != Success)
					return sess.rtn.status;
				break;
			case CSW_inp_delim:
				str = "$inpDelim =";
				goto DoCmd;
			case CSW_no_read:
				readFirst = false;
				break;
			case CSW_otp_delim:
				str = "$otpDelim =";
DoCmd:
				if(runCmdline(pSink, str, result.arg, NULL, RunQWrap) != Success)
					return sess.rtn.status;
				break;
			case CSW_r:
				cRdOnly = true;
				mode.cache[MdIdxReadOnly]->flags |=  MdActive;
				break;
			case CSW_shell:
				shellScript = true;
				break;
			case CSW_xeq:
				// Process script file.
				if(runScript(result.arg, 0, NULL) != Success)
					return sess.rtn.status;
			}
		}

	// Switch error?
	if(rtnCode < 0)
		return libfatal();

	// Process remaining arguments, if any.
	if(!shellScript) {
		while(argCount-- > 0) {
			if(strcmp(*argList, "-") == 0) {	// Solo '-' for reading from standard input.
				if(stdInpFlag != 0)
					return rsset(FatalError, RSNoFormat, text45);
						// "Multiple \"-\" arguments"
				stdInpFlag = 1;			// Process it.
				oflags = BN_Create | BN_CreateHook;
				str = buffer1;
				}
			else {
				str = *argList;
				oflags = BN_Create | BN_CreateHook | BN_Force | BN_Derive;
				}
			++argList;

			// Process a data file.  Set up a buffer for the file and set it to inactive.
			if(bfind(str, oflags, 0, &pBuf, NULL) != Success)
				return sess.rtn.status;
			if(stdInpFlag > 0)		// If using standard input...
				stdInpFlag = -1;	// disable the flag so it can't be used again.
			else if(setFilename(pBuf, str, BF_UpdBufDir) != Success)
				return sess.rtn.status;
			pBuf->flags &= ~BFActive;
			if(cRdOnly)
				pBuf->flags |= BFReadOnly;

			if(pBuf1 == NULL)
				pBuf1 = pBuf;

			// Check for argument switches.
			aRdOnly = aRdWrite = false;
			pSwitch = argSwitchTable;
			while((rtnCode = getSwitch(&argCount, &argList, &pSwitch, elementsof(argSwitchTable),
			 &result)) > 0) {
				switch(rtnCode) {
					case 1:
					case 2:		//  {+|-}line for initial line to go to.
						if(++fileActionCt > 1 || gotoFlag)
							goto Conflict2;
						if(!readFirst)
							goto Conflict3;
						(void) ascToLong(result.arg, 10, &goLine, false);
						gotoFlag = true;
						pBuf1 = pBuf;
						break;
					case 3:		// -pBuf-mode for set/clear buffer mode(s).
						if(modeSwitch(&result, pBuf) != Success)
							return sess.rtn.status;
						break;
					case 4:		// -r for read-only file.
						if(aRdWrite)
							goto Conflict1;
						aRdOnly = true;
						goto SetRW;
					case 5:		// -rw for read/write file.
						if(aRdOnly)
Conflict1:
							return rsset(FatalError, RSNoFormat, text1);
								// "-r and -rw switch conflict"
						aRdWrite = true;
SetRW:
						// Set or clear read-only attribute.
						if(!aRdOnly)
							pBuf->flags &= ~BFReadOnly;
						else
							pBuf->flags |= BFReadOnly;
						break;
					default:	// -search for initial search string.
						if(++fileActionCt > 1 || gotoFlag)
Conflict2:
							return rsset(FatalError, 0, text101, text175);
									// "%sconflict", "-search, + or - switch "
						if(!readFirst)
Conflict3:
							return rsset(FatalError, 0, text61, text175);
							// "%sconflicts with -no-read", "-search, + or - switch "
						if(newSearchPat(result.arg, &bufSearch.match, NULL, true) != Success)
							return sess.rtn.status;
						searchFlag = true;
						pBuf1 = pBuf;
						break;
					}
				}
			if(rtnCode < 0)
				return libfatal();
			}
		}
	else if(runScript(*argList, argCount, argList) != Success)
		return sess.rtn.status;

	// Done processing arguments.  Select initial buffer.
	if(startBuf(pBuf1, stdInpFlag, readFirst) != Success)
		return sess.rtn.status;

	// Process startup gotos and searches.
	if(gotoFlag) {
		char workBuf[WorkBufSize];

		sprintf(workBuf, "gotoLine %ld", goLine);
		(void) execExprStmt(pSink, workBuf, 0, NULL);
		}
	else if(searchFlag)
		(void) huntForw(pSink, 1, NULL);

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Prepare to insert one or more characters at point.  Delete characters first if in replace or overwrite mode and not at end
// of line.  Assume n > 0.  Return status.
int overPrep(long n) {
	bool mdRepl = isBufModeSet(sess.cur.pBuf, mode.cache[MdIdxRepl]);

	if(mdRepl || isBufModeSet(sess.cur.pBuf, mode.cache[MdIdxOver])) {
		int count = 0;
		Point *pPoint = &sess.cur.pFace->point;
		do {
			if(pPoint->offset == pPoint->pLine->used)
				break;

			// Delete character if we are in replace mode, character is not a tab, or we are at a tab stop.
			if(mdRepl || (pPoint->pLine->text[pPoint->offset] != '\t' ||
			 (getCol(NULL, false) + count) % sess.pCurScrn->hardTabSize == (sess.pCurScrn->hardTabSize - 1)))
				if(edelc(1, 0) != Success)
					break;
			++count;
			} while(--n > 0);
		}
	return sess.rtn.status;
	}

// This is the general interactive command execution routine for a key or key sequence.  It handles the fake binding of all the
// printable keys to "self-insert".  It also clears out the "curFlags" word and arranges to move it to "prevFlags" so that the
// next command can look at it.  "extKey" is the key to execute with argument "n".  Return status.
static int execute(long n, ExtKey extKey, KeyBind *pKeyBind) {
	char keyBuf[16];

	// If key(s) bound...
	if(pKeyBind != NULL) {

		// Execute system or user command bound to key or key sequence.
		keyEntry.curFlags = 0;
		if(execKey(pKeyBind, n) == Success) {

			// If a macro recording was ended, finish the process.
			if(pKeyBind->cmd.u.pCmdFunc == cmdFuncTable + cf_endMacro && finishMacro(pKeyBind) != Success)
				goto ErrRtn;
			}

		goto ASaveChk;
		}

	// Key not bound... attempt self-insert abs(n) times.  Ignore "Over" and "Repl" buffer modes if n < 0.
	if(extKey >= 0x20 && extKey < 0xff) {
		bool forceIns = false;
		if(n == DefN)
			n = 1;
		else if(n < 0) {
			n = -n;
			forceIns = true;
			}
		else if(n == 0)
			return sess.rtn.status;

		if(allowEdit(true) != Success)		// Don't allow if read-only buffer.
			goto ErrRtn;
		keyEntry.curFlags = 0;

		// In replace or overwrite mode?
		if(!forceIns && overPrep(n) != Success)
			goto ErrRtn;

		// Insert the character.
		if(einsertc(n, extKey) == Success) {

			// Do fence matching, if possible.
			if(hiliteFence(extKey, true)) {

				// Show inserted character, then highlight other fence.
				if(update(DefN) == Success) {
					(void) moveChar(-1);
					(void) fenceMatch(extKey, false);
					(void) moveChar(1);
					}
				}
ASaveChk:
			// Save all changed buffers if auto-save mode and keystroke count reached.
			if(mode.cache[MdIdxASave]->flags & MdActive && sess.autoSaveTrig > 0 && --sess.autoSaveCount == 0) {
				Datum sink;		// For throw-away return value, if any.
				dinit(&sink);
				keyEntry.prevFlags = keyEntry.curFlags;
				keyEntry.curFlags = 0;
				if(execCmdFunc(&sink, 1, cmdFuncTable + cf_saveFile, 0, 0) < Failure)
					goto ErrRtn;
				dclear(&sink);
				}
			}
		keyEntry.prevFlags = keyEntry.curFlags;
		return sess.rtn.status;					// Success.
		}

	// Unknown key.
	tbeep();							// Beep the beeper...
	return rsset(Failure, RSTermAttr, text14, ektos(keyBuf, extKey, true));	// and complain.
		// "~#u%s~U not bound"
ErrRtn:
	keyEntry.prevFlags = 0;						// Invalidate last flags.
	return sess.rtn.status;
	}

// Do universal/negative numeric argument processing.  ^U sequence is 2, 0, 3, 4, ...  ^_ is -1, -2, ...
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
static int getn(long *pN, ExtKey *pExtKey, KeyBind **ppKeyBind) {
	long n;
	int neg;				// n negative?
	int state;				// Numeric argument processing state.
	bool digit;				// Digit entered?
	ExtKey extKey = *pExtKey;

	if(extKey == coreKeys[CK_NegArg].extKey) {
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
		if(mlprintf(MLHome | MLFlush, text209, neg ? -n : n) != Success)
					// "Arg: %ld"
			break;

		// Get next key.
		if(getkey(true, &extKey, false) != Success)
			goto Retn;
		digit = (extKey >= '0' && extKey <= '9');

		// Take action, depending on current state.
		switch(state) {
			case 1:
				if(extKey == coreKeys[CK_UnivArg].extKey) {
					n = 0;
					state = 2;
					continue;
					}
				if(extKey == coreKeys[CK_NegArg].extKey)
					goto Decr5;
				if(extKey == '-')
					goto Neg3;
				if(digit)
					goto NBegin;
				break;
			case 2:
				if(extKey == coreKeys[CK_UnivArg].extKey) {
					n = 3;
					state = 5;
					continue;
					}
				if(extKey == coreKeys[CK_NegArg].extKey)
					goto Decr5;
				if(extKey == '-') {
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
				if(extKey == coreKeys[CK_UnivArg].extKey)
					goto Incr5;
				if(extKey == coreKeys[CK_NegArg].extKey)
					goto Decr5;
				if(digit) {
					if(state == 3) {
NBegin:
						n = extKey - '0';
						state = 4;
						}
					else
						n = n * 10 + (extKey - '0');
					continue;
					}
				break;
			case 5:
				if(extKey == coreKeys[CK_UnivArg].extKey) {
Incr5:
					if(!neg)
						++n;
					else if(--n == 0)
						neg = false;
					state = 5;
					continue;
					}
				if(extKey == coreKeys[CK_NegArg].extKey) {
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

		// "n" has been finalized.  Process extKey as a command.
		ungetkey(extKey);
		if(getKeySeq(false, &extKey, ppKeyBind, true) != Success)
			goto Retn;
		break;
		}

	// Change sign of n if needed.
	if(neg)
		n = -n;

	// Return updated n and key.
	*pN = n;
	*pExtKey = extKey;

	// Clear the message line.
	(void) mlerase(0);
Retn:
	return sess.rtn.status;
	}

// Interactive command-processing routine.  Read key sequences and process them.  Return if program exit.
static int editLoop(void) {
	ExtKey extKey;			// Extended key read from user.
	long n;				// Numeric argument.
	ushort oldFlag;			// Old last flag value.
	KeyBind *pKeyBind;
	RtnStatus lastRtn;		// Return code from last key executed.
	Datum hookRtnVal;		// For calling pre/postKey hooks.
#if MMDebug & Debug_GarbInfo
	char keyBuf[16];
#endif

	dinit(&lastRtn.msg);
	dinit(&hookRtnVal);
	dsetnull(&lastRtn.msg);
	lastRtn.status = Success;

	// If $lastKeySeq was set by a startup script, simulate it being entered as the first key.
	if(keyEntry.useLast)
		goto JumpStart;
	goto ChkMsg;

	// Do forever.
	for(;;) {
#if MMDebug & Debug_MsgLnRtn
		fputs("editLoop(): calling rsclear(0)...\n", logfile);
#endif
		rsclear(0);

		// To reduce overhead, do garbage collection only if last key entered was not a printable character (normal text
		// entry).
#if MMDebug & Debug_GarbInfo
		ektos(keyBuf, keyEntry.lastKeySeq, false);
		fprintf(logfile, "lastKeySeq: %-12s ->  ", keyBuf);
#endif
		if(keyEntry.lastKeySeq & ~0x7f) {
#if MMDebug & Debug_GarbInfo
			fputs("GARBAGE COLLECTION\n", logfile);
#endif
			dtpop(NULL);				// Throw out all the datum garbage...
			if(ahfree() != Success)			// and the array and hash garbage.
				break;
			}
#if MMDebug & Debug_GarbInfo
		else
			fputs("skipped\n", logfile);
#endif
		// Fix up the screen.
		if(update(DefN) <= MinExit)
			break;

		// Any message from last loop to display?
		if(!disnull(&lastRtn.msg)) {
			ushort flags = MLHome | MLFlush;
			if(lastRtn.status == Success && !(lastRtn.flags & RSNoWrap))
				flags |= MLWrap;
			if(lastRtn.flags & RSTermAttr)
				flags |= MLTermAttr;
			if(mlputs(lastRtn.msg.str, flags) != Success ||
			 tmove(sess.pCurScrn->cursorRow, sess.pCurScrn->cursorCol) != Success || tflush() != Success)
				break;
			}

		// Get the next key from the keyboard or $lastKeySeq.
		setGlobalMode(mode.cache[MdIdxRtnMsg]);
		if(keyEntry.useLast) {
JumpStart:
			pKeyBind = getBind(extKey = keyEntry.lastKeySeq);
			keyEntry.useLast = false;
			}
		else {
			if(getKeySeq(false, &extKey, &pKeyBind, true) <= MinExit)
				break;
			if(sess.rtn.status != Success)
				goto Fail;
			}

		// If there is something on the message line, clear it.
		if(mlerase(0) != Success)
			break;

		// Set default n.
		n = DefN;

		// universalArg or negativeArg command entered?
		if(extKey == coreKeys[CK_UnivArg].extKey || extKey == coreKeys[CK_NegArg].extKey) {

			// Yes, get n argument and new key from user.
			if(getn(&n, &extKey, &pKeyBind) != Success)
				break;
		 	}

		// Execute the user-assigned pre-key hook with n argument, preserving prevFlags.
		oldFlag = keyEntry.prevFlags;
		if(execHook(&hookRtnVal, n, hookTable + HkPreKey, 0) <= MinExit)
			break;
		keyEntry.prevFlags = oldFlag;

	 	// If no pre-key hook error (or no hook)...
		if(sess.rtn.status == Success) {

			// Get updated key from pre-key hook, if any.
			if(keyEntry.useLast) {
				pKeyBind = getBind(extKey = keyEntry.lastKeySeq);
				keyEntry.useLast = false;
				}

			// Execute key.
			if(execute(n, extKey, pKeyBind) <= MinExit)
				break;

		 	// If no key execution error...
			if(sess.rtn.status == Success) {

				// execute the user-assigned post-key hook with n argument and pre-key return value (if any),
				// preserving curFlags.
				oldFlag = keyEntry.curFlags;
				if(execHook(&hookRtnVal, n, hookTable + HkPostKey, 0x21, &hookRtnVal) <= MinExit)
					break;
				keyEntry.curFlags = oldFlag;
				}
			}

		// Process return code and message from execution of key hook or entered key.
		if(sess.rtn.status != Success) {
Fail:
			// Key retrieval/execution failed or returned false.  Kill off any running macro...
			if(macro.state == MacPlaying) {
				macro.n = 0;
				macro.state = MacStopped;
				}

			// and force terminal input next time around.
			keyEntry.useLast = false;
			}
ChkMsg:
		// Any message returned?
#if MMDebug & Debug_MsgLnRtn
		fprintf(logfile, "editLoop(): saving returned message \"%s\" (flags %.4hx)...\n",
		 sess.rtn.msg.str, sess.rtn.flags);
#endif
		if(disnull(&sess.rtn.msg))
			dsetnull(&lastRtn.msg);				// No, clear last.
		else if(rscpy(&lastRtn, &sess.rtn) <= MinExit)		// Yes, save message and code.
			break;

		dsetnil(&hookRtnVal);
		}

	return sess.rtn.status;
	}

// Convert return message to multiple lines (at ", in"), translate to visual form in case it contains non-printable
// character(s), and remove ~ (AttrSpecBegin) sequences if RSTermAttr flag set.  Return Success or Failure.
static int cleanMsg(char **pStr) {
	DFab msg;
	char *str = *pStr = sess.scriptRtn.msg.str;
	size_t len = strlen(text229);
			// ", in"
	short c;

	if(dopentrack(&msg) == 0) {

		// Copy string to fabrication object, replacing any ~~ in message with ~, ~u, ~#u, and ~U with ", and ~b, ~B,
		// ~<n>c, ~C, ~r, ~R, and ~Z with nothing (remove) if RSTermAttr flag set.  Also insert newlines in text229
		// string (from userExecError() function), if any, so that error callback is easier to read.
		while((c = *str++) != '\0') {
			if(c == AttrSpecBegin) {
				if(sess.scriptRtn.flags & RSTermAttr) {
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
			else if(strncmp(str - 1, text229, len) == 0) {
				if(dputf(&msg, 0, ",\n %s", text229 + 1) != 0)
					break;
				str += len - 1;
				continue;
				}

			// Make any non-printable characters other than newline and tab visible.
			if(dputc(c, &msg, c == '\n' || c == '\t' || (c >= ' ' && c <= '~') ? 0 : CvtVizChar) != 0)
				break;
			}
		if(dclose(&msg, FabStr) == 0) {
			*pStr = msg.pDatum->str;
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
int main(int argCount, char *argList[]) {
	char *path;
	Buffer *pBuf;
	ushort flags = ExecSite | ExecUser;

#if MMDebug & Debug_Logfile
	logfile = fopen(Logfile, "w");
	(void) setlinebuf(logfile);
	time_t t;
	(void) time(&t);
	(void) helpInfo(&path, h_version, NULL);
	fprintf(logfile, "\n----------\n%s started %s\n", path, ctime(&t));
	free((void *) path);
#endif
#if MMDebug & Debug_SanityChk
	// Do sanity checks.
	fprintf(logfile, "SANITY CHECKS\n\ncf_END = %d\nelementsof(cmdFuncTable) = %u    (Size should be equal to cf_END)\n",
	 cf_END, (uint)(elementsof(cmdFuncTable)));
	return 0;
#endif
	// Do global initializations.
	sess.myPID = (unsigned) getpid();
	initCharTables();

	// Begin using sess.rtn (RtnStatus): do pre-edit-loop stuff, but use the same return-code mechanism as command calls.
	if(editInit0() == Success &&				// Initialize the return value structures.
	 procInfoSwitches(argCount, argList) == Success &&	// Scan command line for informational switches.
	 editInit1() == Success &&				// Initialize the core data structures.
	 vtinit() == Success &&					// Initialize the virtual terminal.
	 editInit2() == Success &&				// Initialize buffers, windows, and screens.
	 procPreSwitches(argCount, argList,
	  &flags, &path) == Success) {				// Scan command line for -D, -dir, -path, -n, and -N switches.

		// At this point, we are fully operational (ready to process commands).  Set $execPath (in -path, MSPATH,
		// built-in path order of priority).
		if(path == NULL && (path = getenv(MMPath_Name)) == NULL)
			path = MMPath;
		if(setExecPath(path) != Success)
			goto Retn;

		// Run site and user startup files and check for errors.
		if((flags & ExecSite && startup(SiteStartup, 0, argCount, argList) != Success) ||
		 (flags & ExecUser && startup(UserStartup, XP_HomeOnly, argCount, argList) != Success))
			goto Retn;

		// Run create-buffer and change-directory hooks.
		if(execHook(NULL, DefN, hookTable + HkCreateBuf, 1, sess.cur.pBuf->bufname) != Success ||
		 execHook(NULL, DefN, hookTable + HkChDir, 0) != Success)
			goto Retn;

		// Process the command line.
		if(procCmdLine(argCount, argList) == Success) {

			// Enter user-command loop.
			sess.pCurWind->face.wflags |= WFMode;
			sess.opFlags &= ~OpStartup;
			(void) editLoop();
			}
		}
Retn:
	// Processing completed.  Preserve the return code and message, and run "exit" hook if exiting via 'exit' or 'quickExit'
	// command.
	if(rscpy(&sess.scriptRtn, &sess.rtn) == Panic)
		(void) rsset(Panic, 0, NULL);	// No return.
	if(sess.rtn.status == UserExit || sess.rtn.status == ScriptExit)
		(void) execHook(NULL, sess.exitNArg, hookTable + HkExit, 0);
	rsclear(0);

	// Close the terminal (ignoring any error) and return to line mode.
	(void) tclose(false);
#if MMDebug & Debug_Logfile
	(void) fclose(logfile);
#endif
	// Exit MightEMacs.
	if(sess.scriptRtn.status == UserExit || sess.scriptRtn.status == ScriptExit) {
		HashNode *pNode;
		HashEach each;

		// Display names (in line mode) of any files saved via quickExit().
		heachinit(&each, &buf.table);
		while((pNode = heach(&each)) != NULL) {
			pBuf = bufPtr(pNode->pVal);
			if(pBuf->flags & BFQSave) {
				fprintf(stderr, text193, pBuf->filename);
						// "Saved file \"%s\""
				if(pBuf->flags & BFQBak)
					fprintf(stderr, "%s%s%s\"", text257, cname(pBuf->filename, CNBasename | CNPtrOnly),
							// ", original file renamed to \""
					 BackupExt);
				fputc('\n', stderr);
				}
			}

		// Display message, if any.
		if(!disnull(&sess.scriptRtn.msg)) {
			char *str;

			if(cleanMsg(&str) != Success)
				sess.scriptRtn.status = ScriptExit;
			fputs(str, stderr);
			fputc('\n', stderr);
			}

		return sess.scriptRtn.status == ScriptExit ? ErrorRtnCode : 0;
		}

	// Error or help exit.
	if(sess.scriptRtn.status == HelpExit) {
		fputs(sess.scriptRtn.msg.str, stderr);
		fputc('\n', stderr);
		}
	else {
		char workBuf[16];

		sprintf(workBuf, "%s:", text189);
				// "Abort"
		if(strncmp(sess.scriptRtn.msg.str, workBuf, strlen(workBuf)) != 0)
			fprintf(stderr, "%s: ", text0);
					// "Error"
		if(sess.scriptRtn.status == OSError)
			fprintf(stderr, "%s, %s\n", strerror(errno), sess.scriptRtn.msg.str);
		else if(disnull(&sess.scriptRtn.msg))
			fprintf(stderr, text253, sess.scriptRtn.status);
				// "(return status %d)\n"
		else {
			char *str;

			(void) cleanMsg(&str);
			fputs(str, stderr);
			fputc('\n', stderr);
			}
		}
	return ErrorRtnCode;
	}

// Loop through the list of buffers (ignoring user commands and functions) and return true if any were changed, otherwise
// false.
static bool changedBuf(void) {
	Buffer *pBuf;
	HashNode *pNode;
	HashEach each;

	heachinit(&each, &buf.table);
	while((pNode = heach(&each)) != NULL) {
		pBuf = bufPtr(pNode->pVal);
		if((pBuf->flags & (BFCommand | BFFunc | BFChanged)) == BFChanged)
			return true;
		}
	return false;
	}

// Exit (quit) command.  If non-default n, quit unconditionally; otherwise, get confirmation if a buffer has been changed and
// not saved.  If n < 0 and not default, force error return to OS.  Include message if script mode and one is available.
int quit(Datum *pRtnVal, long n, Datum **args) {
	char *msg = NULL;
	int status = UserExit;		// Default return status.
	bool forceClean = false;

	sess.exitNArg = n;

	// Not forced exit and changed buffer?
	if(n == DefN && changedBuf()) {
		bool yep;

		// Have changed buffer(s)... user okay with that?
		if(termInpYN(text104, &yep) != Success)
				// "Modified buffers exist.  Exit anyway"
			return sess.rtn.status;

		// If user changed his mind, don't exit (no error).
		if(yep) {
			// Force clean exit.
			forceClean = true;
			}

		// Clear prompt if script mode.
		if(sess.opFlags & OpScript)
			if(mlerase(MLForce) != Success)
				return sess.rtn.status;
		}

	// Script mode?
	if(sess.opFlags & OpScript) {
		Datum *pDatum;

		// Get return message, if any, and save it if exiting.
		if(buildMsg(&pDatum, NULL, 0) == Success && status != Success) {
			rsclear(0);
			if(!isNN(pDatum))
				msg = pDatum->str;
			if(!forceClean && changedBuf())
				status = ScriptExit;	// Forced exit from a script with changed buffer(s).
			}
		}

	// Force error exit if negative n.
	if(n < 0 && n != DefN)
		status = ScriptExit;

	return rsset(status, RSNoFormat, msg);
	}
