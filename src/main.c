// (c) Copyright 2022 Richard W. Marinelli
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
// INT_MIN is used as the default (and many commands will have a default behavior in that case).  The called routines typically
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

// Make selected global definitions local.
#define MainData
#define CmdData
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

enum h_type {h_copyright, h_version, h_usage, h_help};
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
#include "cmd.h"

// Return true if in interactive mode.
bool interactive(void) {

	return !(sess.opFlags & (OpStartup | OpNoLoad)) && (!(sess.opFlags & OpScript) || (sess.opFlags & OpUserCmd));
	}

// Copy a string to open fabrication object pFab, escaping AttrSpecBegin (~) characters.  Return -1 if error, otherwise 0.
int esctofab(const char *str, DFab *pFab) {

	while(*str != '\0')
		if((*str == AttrSpecBegin && dputc(AttrSpecBegin, pFab, 0) != 0) || dputc(*str++, pFab, 0) != 0)
			return -1;
	return 0;
	}

// Copy global return message to open fabrication object, escaping AttrSpecBegin (~) characters in terminal attribute
// specifications.  Return status.
int escAttr(DFab *pFab) {
	char *str = sess.rtn.msg.str;
	if(sess.rtn.flags & RSTermAttr) {
		if(dputs(str, pFab, 0) != 0 || dputc(AttrSpecBegin, pFab, 0) != 0 || dputc(AttrAllOff, pFab, 0) != 0)
			goto LibFail;
		}
	else if(esctofab(str, pFab) != 0)
LibFail:
		return librsset(Failure);

	return Success;
	}

// Clear return message if RSKeepMsg flag not set, clear return flags, and set return status to Success.
void rsclear(ushort flags) {

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
		(void) librsset(Failure);
#if MMDebug & Debug_Datum
	fprintf(logfile, "rscpy(): copied from (%hu) '%s' [%d] to (%hu) '%s' [%d]\n",
	 pSrc->msg.type, pSrc->msg.str, pSrc->status, pDest->msg.type, pDest->msg.str, pDest->status);
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

#if MMDebug & Debug_MsgLine
	fprintf(logfile, "rsset(%d, %.4hx, \"%s\", ...), current flags %.4hx, msg \"%s\"\n",
	 status, flags, fmt, sess.rtn.flags, sess.rtn.msg.str);
#endif
	// Check status.  If this status is not more severe, or not force and either (1), Success and not displaying messages;
	// or (2), same status but already have a message, return old one.
	if(status > sess.rtn.status)				// Return if new status is less severe.
		goto Retn;
	if(status < sess.rtn.status || (flags & RSForce))	// Set new message if more severe status or a force.
		goto Set;

	// Same status.  Check special conditions.
	if(status == Success) {

		// If RSOverride flag set or 'RtnMsg' global mode enabled and either (1), existing message is null; or (2),
		// RSHigh flag set and existing RSForce and RSHigh flags are clear, set new message.
		if(((flags & RSOverride) || (modeInfo.cache[MdIdxRtnMsg]->flags & MdEnabled)) &&
		 ((disnull(&sess.rtn.msg) || ((flags & RSHigh) && !(sess.rtn.flags & (RSForce | RSHigh))))))
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
#if MMDebug & Debug_MsgLine
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
				if(flags & RSNoFormat) {
					if((str = (char *) malloc(strlen(fmt) + 1)) != NULL)
						strcpy(str, fmt);
					}
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
#if MMDebug & Debug_MsgLine
	fprintf(logfile, "rsset(%d, %.4x, ...): set msg '%s', status %d, new flags %.4x\n",
	 status, (uint) flags, sess.rtn.msg.str, status, (uint) sess.rtn.flags);
#endif
	return (sess.rtn.status = status);
	}

// Save current return code and message for $ReturnMsg.  Return status.
int rssave(void) {

	// Copy to sess.scriptRtn.
	if(dcpy(&sess.scriptRtn.msg, &sess.rtn.msg) == 0)
		sess.scriptRtn.status = sess.rtn.status;
	return sess.rtn.status;
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

// Set and return proper status from a failed ProLib library call.
int librsset(int status) {

	return rsset((cxlExcep.flags & ExcepMem) ? Panic : status, 0, "%s", cxlExcep.msg);
	}

// Concatenate any function arguments via catArgs() and save prefix and result in *ppDatum.  Return status.
static int buildMsg(Datum **ppDatum, const char *prefix, int minRequired) {
	DFab msg;
	Datum *pDatum;

	if(dnewtrack(&pDatum) != 0 || dnewtrack(ppDatum) != 0)
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
		(void) librsset(Failure);
	return sess.rtn.status;
	}

// Abort.  Beep the beeper.  Kill off any macro that is in progress.  Called by "abort" command and any user-input
// function if user presses abort key (usually ^G).  Set and return UserAbort status.
int abortInp(void) {

	tbeep();
	if(curMacro.state == MacRecord)
		sess.cur.pWind->flags |= WFMode;
	curMacro.state = MacStop;
	return rsset(UserAbort, RSNoFormat, text8);
			// "Aborted"
	}

// "abort" command: Call abortInp() to abort processing and build optional exception message.  Enable terminal attributes in
// message if n argument.
int abortOp(Datum *pRtnVal, int n, Datum **args) {

	(void) abortInp();

	// Short and sweet if called by user pressing a key.
	if(!(sess.opFlags & OpScript))
		return sess.rtn.status;

	// Called from elsewhere (a script)... build a message.
	rsclear(0);
	Datum *pDatum;
	if(buildMsg(&pDatum, text189, 0) == Success)
			// "Abort"
		(void) rsset(UserAbort, n == INT_MIN ? RSNoFormat : RSNoFormat | RSTermAttr, pDatum->str);

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
int message(Datum *pRtnVal, int n, Datum **args) {
	Datum *pDatum;
	bool rtnVal = true;
	ushort rtnFlags = RSNoFormat | RSOverride;
	uint argFlags = ArgFirst;
	static Option options[] = {
		{"^Force", NULL, 0, RSForce},
		{"^High", NULL, 0, RSHigh},
		{"No^Wrap", NULL, 0, RSNoWrap},
		{"^TermAttr", NULL, 0, RSTermAttr},
		{"^Fail", NULL, 0, 0},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		ArgNotNull1, text451, false, options};
			// "function option"

	// Get options and set flags.
	if(n != INT_MIN) {
		if(funcArg(pRtnVal, ArgFirst) != Success || parseOpts(&optHdr, NULL, pRtnVal, NULL) != Success ||
		 !needSym(s_comma, true))
			return sess.rtn.status;
		rtnFlags |= getFlagOpts(options);
		if(options[4].ctrlFlags & OptSelected) {
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
			return rsset(Failure, 0, text109, text58, text460);
				// "%s is %s", "Buffer", "read-only"
			}
		if(sess.edit.pBuf->pCallInfo != NULL && sess.edit.pBuf->pCallInfo->execCount > 0) {
			tbeep();
			return rsset(Failure, 0, text284, text276, text248);
				// "Cannot %s %s buffer", "modify", "an executing"
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

// Return a sample of a string for error reporting, given pointer, length, and maximum length (excluding the trailing null).
// If maximum length is zero, use a small size derived from the terminal width.
char *strSamp(const char *src, size_t srcLen, size_t maxLen) {
	size_t len;

	// Set maxLen to maximum destination size.
	if(maxLen == 0)
		maxLen = sampBuf.smallSize;
	else if(maxLen >= sampBuf.bufLen)
		maxLen = sampBuf.bufLen - 1;

	// Now compute sample size...
	if((len = srcLen) > maxLen)
		len = maxLen;
 
	// and return string sample.
	return strfit(sampBuf.buf, len, src, srcLen);
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
static int execKey(KeyBind *pKeyBind, int n) {
	UnivPtr *pUniv = &pKeyBind->targ;
	Datum *pSink;				// For throw-away return value, if any.

	if(dnewtrack(&pSink) != 0)
		return librsset(Failure);
	dsetnil(pSink);

	// Bound to a user command?
	if(pUniv->type == PtrUserCmd) {
		sess.opFlags |= OpUserCmd;
		(void) execBuf(pSink, n, pUniv->u.pBuf, NULL, 0);
		sess.opFlags &= ~OpUserCmd;
		}
	else {
		// Bound to a system command.  Skip execution if n is zero and a repeat count; otherwise, perform "edit" check
		// before invoking it.
		CmdFunc *pCmdFunc = pUniv->u.pCmdFunc;
		if(n == 0 && (pCmdFunc->attrFlags & CFNCount))
			return sess.rtn.status;
		if(allowEdit(pCmdFunc->attrFlags & CFEdit) == Success)
			(void) execCmdFunc(pSink, n, pCmdFunc, 0, 0);
		}

	// False return?
	if(sess.rtn.status == Success && pSink->type == dat_false) {
		if(curMacro.state == MacPlay)
			(void) rsset(Failure, RSNoFormat | RSKeepMsg, text300);
							// "False return"
		else
			(void) rsset(Failure, 0, NULL);
		}

	return sess.rtn.status;
	}

// Initialize global Datum objects and return status.
static int editInit0(void) {

	// Initialize i var, return status, and numeric literals.
	dinit(&iVar.format);
	dinit(&sess.rtn.msg);
	dsetnull(&sess.rtn.msg);
	dinit(&sess.scriptRtn.msg);
	dsetnull(&sess.scriptRtn.msg);
	dinit(&curMacro.name);
	return dsetsubstr("%d", 2, &iVar.format) != 0 ? librsset(Failure) : sess.rtn.status;
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
	uint n;
	CoreKey *pCoreKey;
	UnivPtr univ;
	KeyBind *pKeyBind;
	CmdFunc *pCmdFunc;
	ModeSpec *pModeSpec, **ppModeSpec;
	ModeGrp *pModeGrp1, *pModeGrp2;
	ssize_t index;
	static const char myName[] = "editInit1";
	struct AliasInfo {
		const char *alias;
		UnivPtr targ;
		} *aliasInfo;
	static struct AliasInfo aliases[] = {
		{"cd",{PtrSysCmd, cmdFuncTable + cf_chgDir}},
		{"lcstr",{PtrSysFunc, cmdFuncTable + cf_lowerCaseStr}},
		{"pwd",{PtrSysCmd, cmdFuncTable + cf_showDir}},
		{"quit",{PtrSysCmd, cmdFuncTable + cf_exit}},
		{"require",{PtrSysCmd, cmdFuncTable + cf_xeqFile}},
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
			{"Clob", MLit_Clobber, MdGlobal | MdLocked | MdHidden, -1},
			{"Col", MLit_ColDisp, MdHidden | MdInLine, -1},
			{"Exact", MLit_Exact, MdGlobal | MdEnabled, -1},
			{"Fence1", MLit_Fence1, MdGlobal | MdHidden | MdEnabled, 1},
			{"Fence2", MLit_Fence2, MdGlobal | MdHidden, 1},
			{"HScrl", MLit_HorzScroll, MdGlobal | MdEnabled, -1},
			{"Line", MLit_LineDisp, MdHidden | MdInLine, -1},
			{"Over", MLit_Overwrite, MdLocked, 2},
			{"ReadOnly", MLit_ReadOnly, MdGlobal | MdLocked, -1},
			{"Regexp", MLit_Regexp, MdGlobal, -1},
			{"Repl", MLit_Replace, MdLocked, 2},
			{"RtnMsg", MLit_RtnMsg, MdGlobal | MdLocked | MdHidden | MdEnabled, -1},
			{"Safe", MLit_SafeSave, MdGlobal, -1},
			{"WkDir", MLit_WorkDir, MdGlobal | MdLocked | MdHidden | MdInLine, -1},
			{"Wrap", MLit_Wrap, 0, -1},
			{NULL, NULL, 0, -1}};

	// Initialize macro object.
	if(growMacro() != Success)
		return sess.rtn.status;

	// Add labels to the rings.
	ringTable[RingIdxDel].ringName = text26;
		// "Delete"
	ringTable[RingIdxDel].entryName = text444;
		// " ring item"
	ringTable[RingIdxKill].ringName = text436;
		// "Kill"
	ringTable[RingIdxMacro].ringName = text471;
		// "Macro"
	ringTable[RingIdxSearch].ringName = text78;
		// "Search"
	ringTable[RingIdxRepl].ringName = text84;
		// "Replace"
	ringTable[RingIdxSearch].entryName = ringTable[RingIdxRepl].entryName = text311;
		// " pattern"

	// Get space for SampBuf buffers.
	sampBuf.bufLen = n = term.maxCols + 1;
	sampBuf.smallSize = n / 4;
	if((sampBuf.buf = (char *) malloc(sampBuf.bufLen)) == NULL)
		return rsset(Panic, 0, text94, myName);
			// "%s(): Out of memory!"
	*sampBuf.buf = '\0';

	// Load all the key bindings.
	if(loadBindings() != Success)
		return sess.rtn.status;

	// Initialize the key cache and the "exec" hash table with all command and function names.
	if((execTable = hnew(CmdFuncCount, 0.7, 1.1)) == NULL)
		return librsset(Failure);
	pCoreKey = coreKeys;
	pCmdFunc = cmdFuncTable;
	do {
		univ.type = (pCmdFunc->attrFlags & CFFunc) ? PtrSysFunc :
		 (pCmdFunc->attrFlags & CFHidden) ? PtrPseudo : PtrSysCmd;
		univ.u.pCmdFunc = pCmdFunc;
		if(execNew(pCmdFunc->name, &univ) != Success)
			return sess.rtn.status;

		// Store frequently-used bindings in key cache.
		if(pCmdFunc->attrFlags & CFUniq) {
			pCoreKey->extKey = ((pKeyBind = getPtrEntry(&univ)) == NULL) ? 0 : pKeyBind->code;
			pCoreKey->id = pCmdFunc - cmdFuncTable;
			++pCoreKey;
			}
		} while((++pCmdFunc)->name != NULL);

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
	ainit(&modeInfo.modeTable);
	ppModeSpec = modeInfo.cache;
	modeInit = modeInitTable;
	index = 0;
	do {
		if(mcreate(modeInit->name, index++, modeInit->descrip, modeInit->flags, &pModeSpec) != Success)
			return sess.rtn.status;
		*ppModeSpec++ = pModeSpec;
		if(modeInit->group > 0)
			pModeSpec->pModeGrp = (modeInit->group == 1) ? modeInfo.grpHead : modeInfo.grpHead->next;
		} while((++modeInit)->name != NULL);

	// Clear the search tables and initialize the word list.
	if(newSearchPat("", &searchCtrl.match, NULL, false) == Success && newSearchPat("", &matchRE, NULL, false) == Success &&
	 newReplPat("", &searchCtrl.match, false) == Success && newReplPat("", &matchRE, false) == Success)
		(void) setWordList(WordChars);

	return sess.rtn.status;
	}

// Initialize all of the buffers, windows, and screens.  Return status.
static int editInit2(void) {

	// Allocate the initial buffer and screen.
	ainit(&bufTable);
	if(bfind(buffer1, BS_Create, 0, &sess.cur.pBuf, NULL) != Success || sfind(1, sess.cur.pBuf, &sess.scrnHead) != Success)
		return sess.rtn.status;

	// Set the global screen and window pointers.
	sess.edit.pBuf = sess.cur.pBuf;
	sess.cur.pWind = sess.windHead = (sess.edit.pScrn = sess.cur.pScrn = sess.scrnHead)->pCurWind = sess.scrnHead->windHead;
	sess.edit.pFace  = sess.cur.pFace = &(sess.edit.pWind = sess.cur.pWind)->face;

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
		return librsset(Failure);
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

	if(dnewtrack(&pSink) != 0 || dopentrack(&cmd) != 0 || dputs("xeqFile ", &cmd, 0) != 0)
		goto LibFail;
	if(argList == NULL) {
		char *str = filename;

		// Use existing command line.  Pick off initial filename, quote it, and add a comma.
		while(*str != '\0' && *str != ' ' && *str != '\t')
			++str;
		if(dputsubstr(filename, str - filename, &cmd, DCvtLang) != 0)
			goto LibFail;
		if(*str != '\0')
			if(dputc(',', &cmd, 0) != 0 || dputs(str, &cmd, 0) != 0)
				goto LibFail;
		}
	else {
		// Build command line.
		if(dputs(filename, &cmd, DCvtLang) != 0)
			goto LibFail;
		while(--argCount > 0)
			if(dputc(',', &cmd, 0) != 0 || dputs(*++argList, &cmd, DCvtLang) != 0)
				goto LibFail;
		}
	if(dclose(&cmd, FabStr) != 0)
LibFail:
		return librsset(Failure);

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
// set, quote argument; if RunQSimple flag is set, wrap argument in double quotes; otherwise, leave it as is (unquoted).
// If cmdSuffix not NULL, append it to "cmdPrefix + arg".  Return status.
int runCmd(Datum *pRtnVal, const char *cmdPrefix, const char *arg, const char *cmdSuffix, ushort flags) {
	DFab cmd;

	// Concatenate command name with arguments.
	if(dopentrack(&cmd) != 0 || dputs(cmdPrefix, &cmd, 0) != 0 || dputc(' ', &cmd, 0) != 0)
		goto LibFail;
	if(flags & RunQFull) {
		if(dputs(arg, &cmd, DCvtLang) != 0)
			goto LibFail;
		}
	else if(((flags & RunQSimple) && dputc('"', &cmd, 0) != 0) ||
	 dputs(arg, &cmd, 0) != 0 || ((flags & RunQSimple) && dputc('"', &cmd, 0) != 0))
		goto LibFail;
	if(cmdSuffix != NULL && dputs(cmdSuffix, &cmd, 0) != 0)
		goto LibFail;

	// Close fabrication object, disable "RtnMsg" mode, execute command line, and restore the mode.
	if(dclose(&cmd, FabStr) != 0)
LibFail:
		return librsset(Failure);
	ushort oldMsgFlag = modeInfo.cache[MdIdxRtnMsg]->flags & MdEnabled;
	if(oldMsgFlag)
		clearGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
	(void) execExprStmt(pRtnVal, cmd.pDatum->str, 0, NULL);
	if(oldMsgFlag)
		setGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
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
			nArg = -2;		// Enable partial-name matching in chgMode().
			offset = 1;
			}
		else {
			nArg = 3;		// Enable partial-name matching in chgMode().
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
		if(bswitch(pBuf, 0) != Success || bactivate(pBuf) != Success)
			return sess.rtn.status;

	// Delete first buffer if it is not reserved for standard input, it exists, is empty, and is not being displayed.
	if(stdInpFlag == 0 && (pBuf = bsrch(buffer1, NULL)) != NULL && bempty(pBuf) && pBuf->windCount == 0)
		(void) bdelete(pBuf, 0);
	return sess.rtn.status;
	}

// Display given type of help information and exit (return HelpExit status).
static int helpExit(enum h_type t, const char *binName) {
	char *info;

	if(t == h_version) {
		if(asprintf(&info, "%s %s (GPLv3) [linked with %s, %s]", Myself, Version, cxlvers(), xrevers()) < 0)
			goto Err;
		}
	else if(t == h_copyright) {
		if(asprintf(&info, "%s %s", Myself, Copyright) < 0)
			goto Err;
		}
	else {		// h_usage or h_help
		char *usage;

		if(asprintf(&usage, "%s%s%s%s%s", Usage[0], binName, Usage[1], binName, Usage[2]) < 0)
			goto Err;
		if(t == h_usage)
			info = usage;
		else {
			if(asprintf(&info, "%s%s%s%s", Myself, Help1, usage, Help2) < 0)
Err:
				return rsset(Panic, 0, text94, "helpExit");
					// "%s(): Out of memory!"
			free((void *) usage);
			}
		}

	return rsset(HelpExit, 0, info);
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
	binName = fbasename(*argList++, true);
	--argCount;
	while((rtnCode = getSwitch(&argCount, &argList, &pSwitch, elementsof(cmdSwitchTable), &result)) > 0) {
		pHelpSwitch = helpSwitchTable;
		do {
			if(rtnCode == *pHelpSwitch)
				return helpExit((enum h_type)(pHelpSwitch - helpSwitchTable), binName);
			} while(*++pHelpSwitch != 0);
		}

	return (rtnCode < 0) ? librsset(Failure) : sess.rtn.status;
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
	if(dnewtrack(&pDatum) != 0)
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
				if(runCmd(pDatum, "chgDir ", result.arg, NULL, RunQFull) != Success)
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
		uint dirLen;

		if(dsetstr(*argList, pDatum) != 0)
			goto LibFail;
		if(*(dirname = fdirname(pDatum->str, INT_MIN)) != '\0') {
			dirLen = strlen(dirname);
			if(runCmd(pDatum, "chgDir ", dirname, NULL, RunQFull) == Success)
				*argList += dirLen + 1;
			}
		}

	return sess.rtn.status;
LibFail:
	return librsset(Failure);
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
	ushort bufFlags;
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
	if(dnewtrack(&pSink) != 0)
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
				str = "$inpDelim = ";
				goto DoCmd;
			case CSW_no_read:
				readFirst = false;
				break;
			case CSW_otp_delim:
				str = "$otpDelim = ";
DoCmd:
				if(runCmd(pSink, str, result.arg, NULL, RunQSimple) != Success)
					return sess.rtn.status;
				break;
			case CSW_r:
				cRdOnly = true;
				modeInfo.cache[MdIdxReadOnly]->flags |=  MdEnabled;
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
		return librsset(FatalError);

	// Process remaining arguments, if any.
	if(!shellScript) {
		while(argCount-- > 0) {
			if(strcmp(*argList, "-") == 0) {	// Solo '-' for reading from standard input.
				if(stdInpFlag != 0)
					return rsset(FatalError, 0, text45);
						// "Multiple \"-\" arguments"
				stdInpFlag = 1;			// Process it.
				bufFlags = BS_Create | BS_CreateHook;
				str = buffer1;
				}
			else {
				str = *argList;
				bufFlags = BS_Create | BS_CreateHook | BS_Force | BS_Derive;
				}
			++argList;

			// Process a data file.  Set up a buffer for the file and set it to inactive.
			if(bfind(str, bufFlags, 0, &pBuf, NULL) != Success)
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
						(void) ascToLong(result.arg, &goLine, false);
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
							return rsset(FatalError, 0, text1);
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
						if(newSearchPat(result.arg, &searchCtrl.match, NULL, true) != Success)
							return sess.rtn.status;
						searchFlag = true;
						pBuf1 = pBuf;
						break;
					}
				}
			if(rtnCode < 0)
				return librsset(FatalError);
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

		sprintf(workBuf, goLine >= 0 ? "gotoLine %ld" : "endBuf() || %ld => forwLine", goLine);
		(void) execExprStmt(pSink, workBuf, 0, NULL);
		}
	else if(searchFlag)
		(void) huntForw(pSink, 1, NULL);

	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Prepare to insert one or more characters at point.  Delete characters first if in replace or overwrite mode and not at end
// of line.  Assume n > 0.  Return status.
int overPrep(int n) {
	bool mdRepl = isBufModeSet(sess.cur.pBuf, modeInfo.cache[MdIdxRepl]);

	if(mdRepl || isBufModeSet(sess.cur.pBuf, modeInfo.cache[MdIdxOver])) {
		int count = 0;
		Point *pPoint = &sess.cur.pFace->point;
		do {
			if(pPoint->offset == pPoint->pLine->used)
				break;

			// Delete character if we are in replace mode, character is not a tab, or we are at a tab stop.
			if(mdRepl || (pPoint->pLine->text[pPoint->offset] != '\t' ||
			 (getCol(NULL, false) + count) % sess.cur.pScrn->hardTabSize == (sess.cur.pScrn->hardTabSize - 1)))
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
static int execute(int n, ushort extKey, KeyBind *pKeyBind) {
	char keyBuf[16];

	// If key(s) bound...
	if(pKeyBind != NULL) {

		// Execute system or user command bound to key or key sequence.
		keyEntry.curFlags = 0;
		if(execKey(pKeyBind, n) == Success) {

			// If a macro recording was ended, finish the process.
			if(pKeyBind->targ.u.pCmdFunc == cmdFuncTable + cf_endMacro && finishMacro(pKeyBind) != Success) {
				if(sess.rtn.status > MinExit)
					(void) macReset();
				goto ErrRtn;
				}
			}

		goto ASaveChk;
		}

	// Key not bound... attempt self-insert abs(n) times.  Ignore "Over" and "Repl" buffer modes if n < 0.
	if(extKey >= 0x20 && extKey < 0xFF) {
		bool forceIns = false;
		if(n == INT_MIN)
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
				if(update(INT_MIN) == Success) {
					(void) moveChar(-1);
					(void) fenceMatch(extKey, false);
					(void) moveChar(1);
					}
				}
ASaveChk:
			// Save all changed buffers if auto-save mode and keystroke count reached.
			if((modeInfo.cache[MdIdxASave]->flags & MdEnabled) && sess.autoSaveTrig > 0 &&
			 --sess.autoSaveCount == 0) {
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
	return rsset(Failure, RSTermAttr, text14, ektos(extKey, keyBuf, true));	// and complain.
		// "~#u%s~U not bound"
ErrRtn:
	keyEntry.prevFlags = 0;						// Invalidate last flags.
	return sess.rtn.status;
	}

// Interactive command-processing routine.  Read key sequences and process them.  Return if program exit.
static int editLoop(void) {
	ushort extKey;			// Extended key read from user.
	int n;				// Numeric argument.
	ushort oldFlag;			// Old last flag value.
	KeyBind *pKeyBind;
	RtnStatus lastRtn;		// Return code from last key executed.
	Datum hookRtnVal;		// For calling pre/postKey hooks.

	dinit(&lastRtn.msg);
	dinit(&hookRtnVal);
	dsetnull(&lastRtn.msg);
	lastRtn.status = Success;
	sess.opFlags &= ~OpStartup;

	// If $lastKeySeq was set by a startup script, simulate it being entered as the first key.
	if(keyEntry.useLast)
		goto JumpStart;
	goto ChkMsg;

	// Do forever.
	for(;;) {
		rsclear(0);
		dgPop(NULL);				// Throw out all the datum garbage...
		if(agFree() != Success)			// and the array garbage.
			break;

		// Fix up the screen.
		if(update(INT_MIN) <= MinExit)
			break;

		// Any message from last loop to display?
		if(!disnull(&lastRtn.msg)) {
			ushort flags = MLHome | MLFlush;
			if(lastRtn.status == Success && !(lastRtn.flags & RSNoWrap))
				flags |= MLWrap;
			if(lastRtn.flags & RSTermAttr)
				flags |= MLTermAttr;
			if(mlputs(flags, lastRtn.msg.str) != Success ||
			 tmove(sess.cur.pScrn->cursorRow, sess.cur.pScrn->cursorCol) != Success || tflush() != Success)
				break;
			}

		// Get the next key from the keyboard or $lastKeySeq.
		setGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
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

		if(extKey == coreKeys[CK_UnivArg].extKey ||
		 extKey == coreKeys[CK_NegArg].extKey) {		// universalArg or negativeArg command.
			int neg;				// n negative?
			int state;				// Numeric argument processing state.
			bool digit;				// Digit entered?

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
							// "Arg: %d"
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
				if(getKeySeq(false, &extKey, &pKeyBind, true) != Success)
					goto Retn;
				break;
				}

			// Change sign of n if needed.
			if(neg)
				n = -n;

			// Clear the message line.
			if(mlerase(0) != Success)
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
			if(curMacro.state == MacPlay) {
				curMacro.n = 0;
				curMacro.state = MacStop;
				}

			// and force terminal input next time around.
			keyEntry.useLast = false;
			}
ChkMsg:
		// Any message returned?
		if(disnull(&sess.rtn.msg))
			dsetnull(&lastRtn.msg);				// No, clear last.
		else if(rscpy(&lastRtn, &sess.rtn) <= MinExit)		// Yes, save message and code.
			break;

		dsetnil(&hookRtnVal);
		}
Retn:
	return sess.rtn.status;
	}

// Convert return message to multiple lines (at ", in"), translate to visual form in case it contains non-printable
// character(s), and remove ~ (AttrSpecBegin) sequences if RSTermAttr flag set.  Return Success or Failure.
static int cleanMsg(char **pStr) {
	DFab msg;
	char *str = *pStr = sess.scriptRtn.msg.str;
	size_t len = strlen(text229);
			// ", in"

	if(dopentrack(&msg) == 0) {
		short c;

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
			if(dputc(c, &msg, c == '\n' || c == '\t' || (c >= ' ' && c <= '~') ? 0 : DCvtVizChar) != 0)
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

	// Do global initializations.
	sess.myPID = (unsigned) getpid();
	initCharTables();
#if MMDebug & Debug_Logfile
#if MMDebug & Debug_Endless
	logfile = fopen(MultiLogfile, "a");
#else
	logfile = fopen(LocalLogfile, "w");
#endif
	(void) setlinebuf(logfile);
	time_t t;
	(void) time(&t);
	fprintf(logfile, "\n----------\n%s started %s\n", Myself, ctime(&t));
#endif
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
		if(((flags & ExecSite) && startup(SiteStartup, 0, argCount, argList) != Success) ||
		 ((flags & ExecUser) && startup(UserStartup, XP_HomeOnly, argCount, argList) != Success))
			goto Retn;

		// Run create-buffer and change-directory hooks.
		if(execHook(NULL, INT_MIN, hookTable + HkCreateBuf, 1, sess.cur.pBuf->bufname) != Success ||
		 execHook(NULL, INT_MIN, hookTable + HkChDir, 0) != Success)
			goto Retn;

		// Process the command line.
		if(procCmdLine(argCount, argList) == Success) {

			// Enter user-command loop.
			sess.cur.pWind->flags |= WFMode;
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
#ifdef HashDebug
	hstats(execTable);
#endif
	(void) fclose(logfile);
#endif
	// Exit MightEMacs.
	if(sess.scriptRtn.status == UserExit || sess.scriptRtn.status == ScriptExit) {
		Datum *pArrayEl;
		Array *pArray = &bufTable;

		// Display names (in line mode) of any files saved via quickExit().
		while((pArrayEl = aeach(&pArray)) != NULL) {
			pBuf = bufPtr(pArrayEl);
			if(pBuf->flags & BFQSave)
				fprintf(stderr, text193, pBuf->filename);
						// "Saved file \"%s\"\n"
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
	Datum *pArrayEl;
	Buffer *pBuf;
	Array *pArray = &bufTable;

	while((pArrayEl = aeach(&pArray)) != NULL) {
		pBuf = bufPtr(pArrayEl);
		if((pBuf->flags & (BFCommand | BFFunc | BFChanged)) == BFChanged)
			return true;
		}
	return false;
	}

// Exit (quit) command.  If n != 0 and not default, always quit; otherwise confirm if a buffer has been changed and not written
// out.  If n < 0 and not default, force error return to OS.  Include message if script mode and one is available.
int quit(Datum *pRtnVal, int n, Datum **args) {
	char *msg = NULL;
	int status = UserExit;		// Default return status.
	bool forceClean = false;

	if((sess.exitNArg = n) == INT_MIN)
		n = 0;

	// Not argument-force and changed buffer?
	if(n == 0 && changedBuf()) {
		bool yep;

		// Have changed buffer(s)... user okay with that?
		if(terminpYN(text104, &yep) != Success)
				// "Modified buffers exist.  Exit anyway"
			return sess.rtn.status;
		if(yep) {
			// Force clean exit.
			forceClean = true;
			}
		else {
			// User changed his mind... don't exit (no error).
			status = Success;
			}

		// Clear prompt if script mode and user changed his mind.
		if((sess.opFlags & OpScript) && status == Success && mlerase(MLForce) != Success)
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
	if(n < 0)
		status = ScriptExit;

	return rsset(status, RSNoFormat, msg);
	}
