// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// exec.c	Routines dealing with execution of commands, command lines, buffers, and files for MightEMacs.

#include "std.h"
#include <ctype.h>
#include <stdarg.h>
#include "bind.h"

// Make selected global definitions local.
#define ExecData
#include "exec.h"

#include "file.h"
#include "var.h"

/*** Local declarations ***/

// Control data for if/loop execution levels.
typedef struct {
	VarDesc varDesc;		// Loop variable descriptor.
	ushort type;			// Type of iteration object (array, hash, or string).
	union {
		ArrayEach arrayEach;	// Array iteration state.
		HashEach hashEach;	// Hash iteration state.
		char *next;		// String iteration state.
		} e;
	char *str;			// String to iterate over.
	} ForLoopHdr;
typedef struct ExecLevel {
	struct ExecLevel *prev, *next;
#if MMDebug & (Debug_Script | Debug_Preproc)
	uint level;			// Execution level, beginning at zero.
#endif
	bool live;			// True if executing this level.
	bool loopSpawn;			// True if level spawned by any loop statement.
	int loopCount;			// Number of times through the loop.
	ForLoopHdr *pForHdr;		// Control information for "for" loop.
	bool ifWasTrue;			// True if (possibly compound) "if" statement was ever true.
	bool elseSeen;			// True if "else" clause has been processed at this level.
	} ExecLevel;

#define StmtKeywordMax	10		// Length of longest statement keyword name.

#if MMDebug & (Debug_Datum | Debug_Script | Debug_CallArg | Debug_Preproc)
static char *showFlags(Parse *pNewParse) {
	char *str;
	static char workBuf[100];

	sprintf(workBuf, "OpScript %s, lastParse", sess.opFlags & OpScript ? "SET" : "CLEAR");
	str = strchr(workBuf, '\0');
	if(pLastParse == NULL)
		str = stpcpy(str, " NULL");
	else {
		sprintf(str, "->flags %.4hx", pLastParse->flags);
		str = strchr(str, '\0');
		}
	if(pNewParse != NULL)
		sprintf(str, ", newParse.flags %.4hx", pNewParse->flags);
	return workBuf;
	}
#endif

// Search hook table for given name.  If found, set *ppHookRec to pointer; otherwise, return an error.
static int findHook(const char *name, HookRec **ppHookRec) {
	HookRec *pHookRec = hookTable;

	do {
		if(strcasecmp(name, pHookRec->name) == 0) {
			*ppHookRec = pHookRec;
			return sess.rtn.status;
			}
		} while((++pHookRec)->name != NULL);

	return rsset(Failure, 0, text52, text136, name);
			// "No such %s \"%s\"", "hook"
	}

// Check if a user function is bound to a hook and return true if so (and set error if isError is true), otherwise, false.
bool isHook(Buffer *pBuf, bool isError) {
	HookRec *pHookRec = hookTable;

	do {
		if(pBuf == pHookRec->func.u.pBuf) {
			if(isError)
				(void) rsset(Failure, 0, text327, pHookRec->name);
						// "User function bound to '%s' hook - cannot delete"
			return true;
			}
		} while((++pHookRec)->name != NULL);

	return false;
	}

// Erase (clear) a hook.
static void eraseHook(HookRec *pHookRec) {

	pHookRec->func.type = PtrNull;
	pHookRec->func.u.ptr = NULL;
	}

// Clear named hook(s), or all hooks if n <= 0.  Return number of hooks cleared.
int clearHook(Datum *pRtnVal, long n, Datum **args) {
	HookRec *pHookRec;
	int count = 0;

	if(n <= 0 && n != DefN) {

		// Clear all hooks.
		pHookRec = hookTable;
		do {
			eraseHook(pHookRec);
			++count;
			} while((++pHookRec)->name != NULL);
		}
	else {
		// Clear named hook(s).
		Datum *pHookName;
		uint argFlags = ArgFirst | ArgNotNull1;

		if((pHookName = dnewtrack()) == NULL)
			return libfail();
		for(;;) {
			if(!(argFlags & ArgFirst) && !haveSym(s_comma, false))
				break;					// No arguments left.
			if(funcArg(pHookName, argFlags) != Success)
				return sess.rtn.status;
			argFlags = ArgNotNull1;

			// Find hook and clear it.
			if(findHook(pHookName->str, &pHookRec) != Success)
				return sess.rtn.status;
			eraseHook(pHookRec);
			++count;
			}
		}

	// Return count.
	dsetint((long) count, pRtnVal);

	return sess.rtn.status;
	}

// Return name of function bound to a hook, given function pointer.
const char *hookFuncName(ExecPtr *pExec) {

	return (pExec->type == PtrSysFunc) ? pExec->u.pCmdFunc->name : pExec->u.pBuf->bufname + 1;
	}

// Set a hook.  Return status.
int setHook(Datum *pRtnVal, long n, Datum **args) {
	HookRec *pHookRec;
	ExecPtr exec;			// Pointer to the requested function.
	short minArgs, maxArgs;
	const char *name;
	Datum *pHookName = args[0];

	// Check if hook name is valid.
	if(sess.opFlags & OpEval && findHook(pHookName->str, &pHookRec) != Success)
		goto Retn;

	// Get the function name (bare word).
	if(!needSym(s_comma, true))
		goto Retn;
	if(getCF(NULL, &exec, PtrSysFunc | PtrUserFunc) != Success)
		goto Retn;

	// Non-hook system function?
	if(exec.type == PtrSysFunc && !(exec.u.pCmdFunc->attrFlags & CFHook))
		return rsset(Failure, RSTermAttr, text420, text336, hookFuncName(&exec));
					// "Hook may not be set to %s ~b%s~B", "system function"
	// Argument count mismatch?
	if(exec.type == PtrSysFunc) {
		minArgs = exec.u.pCmdFunc->minArgs;
		maxArgs = exec.u.pCmdFunc->maxArgs;
		if(minArgs != pHookRec->argCount || maxArgs != pHookRec->argCount) {
			name = exec.u.pCmdFunc->name;
			goto Mismatch;
		 	}
		}
	else {
		minArgs = exec.u.pBuf->pCallHdr->minArgs;
		maxArgs = exec.u.pBuf->pCallHdr->maxArgs;
		if(minArgs != pHookRec->argCount || maxArgs != pHookRec->argCount) {
			name = exec.u.pBuf->bufname + 1;
Mismatch:
			return rsset(Failure, RSTermAttr, text413, name, minArgs, maxArgs, pHookRec->name, pHookRec->argCount);
					// "Function \"~b%s~B\" argument count (%hd, %hd) does not match \"%s\" hook's (%hd)"
			}
	 	}

	// Set the hook if evaluating.
	if(sess.opFlags & OpEval)
		pHookRec->func = exec;
Retn:
	return sess.rtn.status;
	}

// Disable a hook that went awry.  Return status.
static int disableHook(HookRec *pHookRec) {

	// Hook execution failed -- let the user know what's up.
	pHookRec->running = false;
#if MMDebug & Debug_MsgLnRtn
	fprintf(logfile, "disableHook('%s') BEGIN: return status %hd, flags %.4hx, msg \"%s\"\n",
	 pHookRec->name, sess.rtn.status, sess.rtn.flags, sess.rtn.msg.str);
#endif
	if(sess.rtn.status > MinExit) {
		if(disnull(&sess.rtn.msg))
			(void) rsset(ScriptError, RSForce | RSTermAttr, text176, hookFuncName(&pHookRec->func));
								// "Function ~b%s~B failed"
		if(!(sess.opFlags & OpStartup))
			(void) rscat(text161, pHookRec->name);
				// " (disabled '%s' hook)"
		(void) rsset(ScriptError, 0, NULL);
#if MMDebug & Debug_MsgLnRtn
		fprintf(logfile, "disableHook(): set new return message \"%s\", flags now %.4hx\n",
		 sess.rtn.msg.str, sess.rtn.flags);
#endif
		}

	// Disable the hook and return (error) status.
	eraseHook(pHookRec);
	return sess.rtn.status;
	}

// Execute a function bound to a hook with pRtnVal (if not NULL) and n.  If argInfo == 0, invoke the function directly;
// otherwise, build a command line with arguments.  In the latter case, argInfo contains the argument count and types of each
// argument (string, long, or Datum *).  The lowest four bits of argInfo comprise the argument count.  Each pair of higher bits
// is 0 for a string argument, 1 for a long (integer) argument, and 2 for a Datum pointer argument.
int execHook(Datum *pRtnVal, long n, HookRec *pHookRec, uint argInfo, ...) {

	// Nothing to do if hook not set or function already running.
	if(pHookRec->func.type == PtrNull || pHookRec->running) {
		if(pRtnVal != NULL)
			dsetnil(pRtnVal);
		return sess.rtn.status;
		}
	if(pRtnVal == NULL && (pRtnVal = dnewtrack()) == NULL)		// For throw-away return value.
		goto LibFail;

	// Build command line if requested.
	if(argInfo != 0) {
		int rtnCode;
		va_list varArgList;
		DFab cmd;
		char *str;
		ushort oldUserCmd;
		short delimChar = ' ';
		uint argCount = argInfo & 0xf;

		// Concatenate function name with arguments...
		if(dopentrack(&cmd) != 0 || (n != DefN && dputf(&cmd, 0, "%ld => ", n) != 0) ||
		 dputs(hookFuncName(&pHookRec->func), &cmd, 0) != 0)
			goto LibFail;
		va_start(varArgList, argInfo);
		argInfo >>= 4;
		do {
			if(dputc(delimChar, &cmd, 0) != 0)
				goto LibFail;
			switch(argInfo & 3) {
				case 0:			// string argument.
					if((str = va_arg(varArgList, char *)) == NULL) {
						if(dputs(viz_nil, &cmd, 0) != 0)
							goto LibFail;
						}
					else if(dputs(str, &cmd, CvtLang) != 0)
						goto LibFail;
					break;
				case 1:			// long argument.
					if(dputf(&cmd, 0, "%ld", va_arg(varArgList, long)) != 0)
						goto LibFail;
					break;
				default:		// Datum *
					if((rtnCode = dputd(va_arg(varArgList, Datum *), &cmd, NULL, CvtLang)) < 0)
						goto LibFail;
					if(endless(rtnCode) != Success)
						goto Fail;
				}
			argInfo >>= 2;
			delimChar = ',';
			} while(--argCount > 0);
		va_end(varArgList);
		if(dclose(&cmd, FabStr) != 0)
			goto LibFail;

		// save and clear OpUserCmd flag...
		oldUserCmd = sess.opFlags & OpUserCmd;
		sess.opFlags &= ~OpUserCmd;

		// execute command...
		pHookRec->running = true;
#if MMDebug & Debug_Datum
		fprintf(logfile, "execHook(): calling execExprStmt(%s)...\n", cmd.pDatum->str);
		(void) execExprStmt(pRtnVal, cmd.pDatum->str, 0, NULL);
		fprintf(logfile, "execHook(): execExprStmt() returned sess.rtn.msg '%s', status %d.\n",
		 sess.rtn.msg.str, sess.rtn.status);
		if(sess.rtn.status != Success)
#else
		if(execExprStmt(pRtnVal, cmd.pDatum->str, 0, NULL) != Success)
#endif
			goto Fail;

		// and restore OpUserCmd flag.
		sess.opFlags |= oldUserCmd;
		}
	else {
		short oldStatus;
		ushort oldFlags;

		// Save and restore status if running "exit" hook so that execBuf() and execCmdFunc() will run.
		if(pHookRec == hookTable + HkExit)
			rspush(&oldStatus, &oldFlags);
		pHookRec->running = true;
		if(pHookRec->func.type == PtrUserFunc)
			(void) execBuf(pRtnVal, n, pHookRec->func.u.pBuf, NULL, SRun_NoArgs | ArgFirst);
		else
			(void) execCmdFunc(pRtnVal, n, pHookRec->func.u.pCmdFunc, 0, 0);
		if(pHookRec == hookTable + HkExit)
			rspop(oldStatus, oldFlags);
		else if(sess.rtn.status != Success)
			goto Fail;
		}

	// Successful execution.  Check for false return.
	pHookRec->running = false;
	if(pRtnVal->type != dat_false)
		return sess.rtn.status;
Fail:
	// Execution failed or hook returned false.  Disable hook and let the user know what's up.
	return disableHook(pHookRec);
LibFail:
	return libfail();
	}

// Execute a named command or alias to a command interactively even if it is not bound or is being invoked from a user command
// or function.
int run(Datum *pRtnVal, long n, Datum **args) {
	ExecPtr exec;		// Pointer to command or alias to execute.
	ushort oldScript;
	ushort oldMsgFlag = mode.cache[MdIdxRtnMsg]->flags & MdActive;

	// If we are in script mode, force the command interactively.
	if((oldScript = sess.opFlags & OpScript)) {

		// Grab next symbol...
		if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
			return sess.rtn.status;

		// look it up...
		if(sess.opFlags & OpEval) {
			if(!execFind(pLastParse->tok.str, OpQuery, PtrSysCmd | PtrAliasSysCmd | PtrUserCmd | PtrAliasUserCmd,
			 &exec))
				return rsset(Failure, 0, text52, text116, pLastParse->tok.str);
					// "No such %s \"%s\"", "command or alias"
			}

		// and get next symbol.
		if(getSym() < NotFound)
			return sess.rtn.status;

		// If not evaluating, bail out here.
		if(!(sess.opFlags & OpEval))
			return sess.rtn.status;

		// Otherwise, prepare to execute the command -- interactively.
		sess.opFlags &= ~OpScript;
		setGlobalMode(mode.cache[MdIdxRtnMsg]);
		}

	// Prompt the user to type a named command or alias.
	else {
		// Interactive mode.
		if(getCFA(": ", PtrSysCmd | PtrAliasSysCmd | PtrUserCmd | PtrAliasUserCmd, &exec, text116) != Success ||
											// "command or alias"
		 exec.type == PtrNull)
			return sess.rtn.status;
		}

	// Execute it.
	if(exec.type & PtrAlias)
		exec = exec.u.pAlias->targ;
	if(exec.type == PtrUserCmd) {

		// Save and restore OpUserCmd flag.
		ushort oldUserCmd = sess.opFlags & OpUserCmd;
		sess.opFlags |= OpUserCmd;
		(void) execBuf(pRtnVal, n, exec.u.pBuf, NULL,
		 sess.opFlags & OpParens ? ArgFirst | SRun_Parens : ArgFirst);		// Call the user command.
		sess.opFlags = (sess.opFlags & ~OpUserCmd) | oldUserCmd;
		}
	else {
		CmdFunc *pCmdFunc = exec.u.pCmdFunc;
		if(n != 0 || !(pCmdFunc->attrFlags & CFNCount))
			if(allowEdit(pCmdFunc->attrFlags & CFEdit) == Success)
				(void) execCmdFunc(pRtnVal, n, pCmdFunc, 0, 0);		// Call the system command.
		}
	if(sess.rtn.status == Success && oldScript)
		(void) rssave();

	// Clean up and return result.
	if(oldScript && !oldMsgFlag)
		clearGlobalMode(mode.cache[MdIdxRtnMsg]);
	sess.opFlags = (sess.opFlags & ~OpScript) | oldScript;
	return sess.rtn.status;
	}

// Concatenate all arguments and execute string result.  Return status.
int eval(Datum *pRtnVal, long n, Datum **args) {
	Datum *pDatum;

	// Get the command line.
	if((pDatum = dnewtrack()) == NULL)
		return libfail();
	if(sess.opFlags & OpScript) {

		// Concatenate all arguments into pDatum.
		if(catArgs(pDatum, 1, NULL, 0) != Success)
			return sess.rtn.status;
		}
	else if(termInp(pDatum, ": ", ArgNotNull1 | ArgNil1, 0, NULL) != Success || disnil(pDatum))
		return sess.rtn.status;

	// Execute command line.
#if MMDebug & Debug_Script
	fprintf(logfile, "eval(): Calling execExprStmt(\"%s\")...\n", pDatum->str);
#endif
	return execExprStmt(pRtnVal, pDatum->str, ParseStmtEnd, NULL);
	}

#if MMDebug & Debug_CallArg
// Dump user routine argument list to log file.
static int argDump(long n, Buffer *pBuf, Datum *pDatum) {
	Datum argList;

	fprintf(logfile, "-----\nargDump(): n: %ld, pBuf: '%s'\n", n, pBuf->bufname);
	dinit(&argList);
	if(dtos(&argList, pDatum, NULL, CvtLang) < 0)
		return libfail();
	fprintf(logfile, "User routine argument list: %s\n", argList.str);
	dclear(&argList);
	return sess.rtn.status;
	}
#endif

// Load user command/function arguments into an array and save in "pArgV".  If in script mode, evaluate some or all of remaining
// arguments (if any) on command line, dependent on number specified in Buffer record; otherwise, create empty list.  "flags"
// contains script flags and argument flags.  Return status.
#if MMDebug & Debug_CallArg
static int argLoad(long n, Buffer *pBuf, Datum *pArgV, uint flags) {
#else
static int argLoad(Buffer *pBuf, Datum *pArgV, uint flags) {
#endif
	Array *pArray;

#if MMDebug & Debug_CallArg
	fprintf(logfile, "*argLoad(%ld, %s, %.8x)...\n", n, pBuf->bufname, flags);
#endif
	// Create array for argument list (which may remain empty).
	if(makeArray(pArgV, 0, &pArray) != Success)
		return sess.rtn.status;

	// Don't get arguments (that don't exist) if executing a hook.
	if(sess.opFlags & OpScript && !(flags & SRun_NoArgs)) {
		Datum *pArg;
		int argCount, minArgs, maxArgs;
		uint argFlags = (flags & ArgFirst) | ArgAny1;
		minArgs = pBuf->pCallHdr->minArgs;
		maxArgs = pBuf->pCallHdr->maxArgs < 0 ? SHRT_MAX : pBuf->pCallHdr->maxArgs;

		// Get arguments until none left.
		if((pArg = dnewtrack()) == NULL)
			goto LibFail;
#if MMDebug & Debug_CallArg
		fprintf(logfile, "\tParsing arguments.  tok: \"%s\", src: \"%s\"\n", pLastParse->tok.str, pLastParse->src);
#endif
		// If xxx(...) call form, have ')', and minArgs == 0, bail out (not an error).
		if(minArgs > 0 || (maxArgs != 0 && (!(flags & SRun_Parens) || !haveSym(s_rightParen, false)))) {
			for(argCount = 0;;) {

				// Get next argument.
				if(argFlags & ArgFirst) {
					if(!haveSym(s_any, minArgs > 0)) {
						if(sess.rtn.status != Success)
							return sess.rtn.status;	// Error.
						break;				// No arguments left.
						}
					if(haveSym(s_rightParen, false))
						break;				// No arguments left.
					}
				else if(!haveSym(s_comma, false))
					break;					// No arguments left.

				// We have a symbol.  Get expression.
				if(funcArg(pArg, argFlags) != Success)
					return sess.rtn.status;
				argFlags = ArgBool1 | ArgArray1 | ArgHash1 | ArgNIS1;

				// Got an argument... add it to array and count it.
				if(apush(pArray, pArg, OpXfer) != 0)
					goto LibFail;
				++argCount;
				}

			// Too few or too many arguments found?
			if(argCount < minArgs || argCount > maxArgs)
				return rsset(Failure, 0, text69, pLastParse->tok.str);
					// "Wrong number of arguments (at token '%s')"
			}
		}
	else if(flags & SRun_NoArgs && pBuf->pCallHdr->minArgs > 0)
		return rsset(Failure, 0, text69, "");
			// "Wrong number of arguments (at token '%s')"
#if MMDebug & Debug_CallArg
	return argDump(n, pBuf, pArgV);
#else
	return sess.rtn.status;
#endif
LibFail:
	return libfail();
	}

#if MMDebug & (Debug_Datum | Debug_Expr | Debug_Token)
// Write parsing state to log file.
static void printInstance(bool begin) {

	if(pLastParse != NULL) {
		fputs(begin ? "parseBegin(): new instance" : "parseEnd(): old instance restored", logfile);
		fprintf(logfile, " %.8x: src \"%s\", tok \"%s\", sym %.4x.",
		 (uint) pLastParse, pLastParse->src, pLastParse->tok.str, pLastParse->sym);
		fputs(begin ? "..\n" : "\n", logfile);
		}
	}
#endif

// Initialize "pLastParse" global variable for new command line and get first symbol.  Return status.
static int parseBegin(Parse **ppOldParse, Parse *pNewParse, char *cmdLine, ushort flags) {

	// Save current parsing state and initialize new one.
	*ppOldParse = pLastParse;
	pLastParse = pNewParse;
	pLastParse->src = cmdLine;
	pLastParse->sym = s_any;
	dinit(&pLastParse->tok);
#if MMDebug & (Debug_Datum | Debug_Expr | Debug_Token)
	printInstance(true);
#endif
	pLastParse->trackHead = datTrackHead;
#if MMDebug & Debug_Datum
	fprintf(logfile, "parseBegin(): saved datum tracking pointer %.8x\n", (uint) datTrackHead);
#endif
	// Save old "script mode" flag and enable it.
	pLastParse->flags = (sess.opFlags & OpScript) | flags;
	sess.opFlags = (sess.opFlags & ~OpParens) | OpScript;

	// Get first symbol and return.
	(void) getSym();			// Ignore any "NotFound" return.
	return sess.rtn.status;
	}

// End a parsing instance and restore previous one.
static void parseEnd(Parse *pOldParse) {

#if MMDebug & Debug_Datum
	fprintf(logfile, "parseEnd(): old instance %.8x: calling dtpop()...\n", (uint) pLastParse);
#endif
	dtpop(pLastParse->trackHead);
	dclear(&pLastParse->tok);
	sess.opFlags = (sess.opFlags & ~OpScript) | (pLastParse->flags & OpScript);
	pLastParse = pOldParse;
#if MMDebug & (Debug_Datum | Debug_Expr | Debug_Token)
	printInstance(false);
#endif
	}		

// Execute string in current parsing instance as an expression statement and return status.  Low-level routine called by
// execExprStmt() and xbuf().
static int xstmt(Datum *pRtnVal) {
	ExprNode node;

	// Loop through expressions separated by a semicolon (TokC_StmtEnd), if applicable.
	for(;;) {
		// Set up the default command values.
		keyEntry.prevFlags = keyEntry.curFlags;
		keyEntry.curFlags = 0;

		// Evaluate the string (as an expression).
		nodeInit(&node, pRtnVal, EN_TopLevel);
#if MMDebug & (Debug_Expr | Debug_Script)
		fprintf(logfile, "xstmt() BEGIN: calling ge_andOr() on token '%s', line '%s'...\n", pLastParse->tok.str,
		 pLastParse->src);
#endif
		if(ge_andOr(&node) != Success)
			break;

		// If parsing stopped at a semicolon, skip past it and evaluate next expression.
		if(*pLastParse->src == TokC_StmtEnd) {
			++pLastParse->src;
			(void) getSym();
			continue;
			}

		// Error if extra symbol(s) or if parsing "#{...}" and right brace missing.
		if(!extraSym()) {
			if(pLastParse->flags & ParseExprEnd && *pLastParse->src != TokC_ExprEnd)
				(void) rsset(Failure, 0, text173, TokC_Expr, TokC_ExprBegin, TokC_ExprEnd);
						// "Unbalanced %c%c%c string parameter"
			}
		break;
		}

#if MMDebug & (Debug_Script | Debug_Expr)
	fprintf(logfile, "xstmt(): Returning status %hd \"%s\"\n", sess.rtn.status, sess.rtn.msg.str);
#endif
	return sess.rtn.status;
	}

// Parse and execute a string as an expression statement, given result pointer, string pointer, parse-terminator flags, and
// optional pointer in which to return updated string pointer.  This function is called anytime a string needs to be executed as
// a "statement" (without a leading statement keyword); for example, by xbuf(), xeqCmdLine(), "#{...}", or the -e switch at
// startup.
int execExprStmt(Datum *pRtnVal, char *cmdLine, ushort flags, char **pCmdLine) {
	Parse *pOldLast, newLast;

#if MMDebug & (Debug_Datum | Debug_Script)
	fprintf(logfile, "execExprStmt(%.8x, \"%s\", %.4hx, %.8x)...\n", (uint) pRtnVal, cmdLine, flags, (uint) pCmdLine);
#endif
	// Begin new command line parsing instance.
	if(parseBegin(&pOldLast, &newLast, cmdLine, flags) == Success) {

		// Evaluate the line (as an expression).
		(void) xstmt(pRtnVal);

		// Restore settings and return.
		if(pCmdLine != NULL)
			*pCmdLine = pLastParse->src;
		}
	parseEnd(pOldLast);
#if MMDebug & Debug_Script
	fputs("execExprStmt(): EXIT\n", logfile);
#endif
	return sess.rtn.status;
	}

// Create or delete an alias in the linked list headed by ahead and in the execution table and return status.
// If the alias already exists:
//	If op is OpCreate:
//		Do nothing.
//	If op is OpDelete:
//		Delete the alias and the associated execTable entry.
// If the alias does not already exist:
//	If op is OpCreate:
//		Create a new object, set its contents to *pExec, and add it to the linked list and execution table.
//	If op is OpDelete:
//		Return an error.
int editAlias(const char *name, uint op, ExecPtr *pExec) {
	Alias *pAlias1, *pAlias2;
	int result;

	// Scan the alias list.
	pAlias1 = NULL;
	pAlias2 = ahead;
	while(pAlias2 != NULL) {
		if((result = strcmp(pAlias2->name, name)) == 0) {

			// Found it.  Check op.
			if(op == OpDelete) {

				// Delete the execTable entry.
				if(execFind(name, OpDelete, 0, NULL) != Success)
					return sess.rtn.status;

				// Decrement alias use count on user command or function, if applicable.
				if(pAlias2->targ.type & PtrUserCmdFunc)
					--pAlias2->targ.u.pBuf->aliasCount;

				// Delete the alias from the list and free the storage.
				if(pAlias1 == NULL)
					ahead = pAlias2->next;
				else
					pAlias1->next = pAlias2->next;
				free((void *) pAlias2);

				return sess.rtn.status;
				}

			// Not a delete... nothing to do.
			return sess.rtn.status;
			}
		if(result > 0)
			break;
		pAlias1 = pAlias2;
		pAlias2 = pAlias2->next;
		}

	// No such alias exists, create it?
	if(op == OpCreate) {
		Symbol sym;
		char *str;
		ExecPtr exec;

		// Valid identifier name?
		str = (char *) name;
		if(((sym = getIdent(&str, NULL)) != s_ident && sym != s_identQuery) || *str != '\0')
			(void) rsset(Failure, 0, text447, text286, name);
				// "Invalid %s '%s'", "identifier"

		// Allocate the needed memory.
		if((pAlias2 = (Alias *) malloc(sizeof(Alias) + strlen(name))) == NULL)
			return rsset(Panic, 0, text94, "editAlias");
				// "%s(): Out of memory!"

		// Find the place in the list to insert this alias.
		if(pAlias1 == NULL) {

			// Insert at the beginning.
			pAlias2->next = ahead;
			ahead = pAlias2;
			}
		else {
			// Insert after pAlias1.
			pAlias2->next = pAlias1->next;
			pAlias1->next = pAlias2;
			}

		// Set the other record members.
		strcpy(pAlias2->name, name);
		pAlias2->targ = *pExec;
		pAlias2->type = pExec->type == PtrSysCmd ? PtrAliasSysCmd : pExec->type == PtrSysFunc ? PtrAliasSysFunc :
		 pExec->type == PtrUserCmd ? PtrAliasUserCmd : PtrAliasUserFunc;

		// Add its name to the execTable hash.
		return execFind((exec.u.pAlias = pAlias2)->name, OpCreate, exec.type = pAlias2->type, &exec);
		}

	// Delete request but alias not found.
	return rsset(Failure, 0, text52, text127, name);
			// "No such %s \"%s\"", "alias"
	}

// Create an alias to a command or function.  Replace any existing alias if n > 0.
int alias(Datum *pRtnVal, long n, Datum **args) {
	ExecPtr exec;
	Datum *pName;

	// Get the alias name.
	if((pName = dnewtrack()) == NULL)
		goto LibFail;
	if(sess.opFlags & OpScript) {
		if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
			return sess.rtn.status;
		if(dsetstr(pLastParse->tok.str, pName) != 0)
			goto LibFail;
		}
	else if(termInp(pName, text215, ArgNotNull1 | ArgNil1, 0, NULL) != Success || disnil(pName))
			// "Create alias "
		return sess.rtn.status;

	// Existing command, function, alias, or local variable of same name?  Exclude aliases from search if n > 0 (a force).
	if(sess.opFlags & OpEval && (execFind(pName->str, OpQuery, n > 0 ? PtrAny & ~PtrAlias : PtrAny, NULL) ||
	 findLocalVar(pName->str) != NULL))
		return rsset(Failure, RSTermAttr, text165, pName->str, text109);
					// "Name '~b%s~B'%s", " already in use"

	if(sess.opFlags & OpScript) {

		// Get equal sign.
		if(getSym() < NotFound || !haveSym(s_any, true))
			return sess.rtn.status;
		if(pLastParse->sym != s_assign)
			return rsset(Failure, RSTermAttr, text23, (cmdFuncTable + cf_alias)->name, pLastParse->tok.str);
				// "Missing '=' in ~b%s~B command (at token '%s')"
		if(getSym() < NotFound)			// Past '='.
			return sess.rtn.status;

		// Get the command or function name.
		(void) getCF(NULL, &exec, PtrSysCmdFunc | PtrUserCmdFunc);
		}
	else {
		char workBuf[strlen(text215) + strlen(pName->str) + strlen(text325) + strlen(text313) + 8];
				// "Create alias "			"of"		"command or function"
		// Get the command or function name.
		sprintf(workBuf, "%s'~b%s~B' %s %s", text215, pName->str, text325, text313);
						// "Create alias "	"of", "command or function"
		(void) getCF(workBuf, &exec, PtrSysCmdFunc | PtrUserCmdFunc);
		}

	if(sess.rtn.status == Success && exec.type != PtrNull) {

		// Delete any existing alias.
		if(n > 0 && execFind(pName->str, OpQuery, PtrAlias, NULL))
			(void) editAlias(pName->str, OpDelete, NULL);

		// Create the alias.
		if(editAlias(pName->str, OpCreate, &exec) != Success)
			return sess.rtn.status;

		// Increment alias use count on user command or function.
		if(exec.type & PtrUserCmdFunc)
			++exec.u.pBuf->aliasCount;
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Delete one or more user commands, user functions, or aliases.  Set pRtnVal to count of items deleted or zero if error.
// Return status.
static int delCFA(Datum *pRtnVal, uint selector, const char *type) {
	ExecPtr exec;
	uint count = 0;

	if(sess.opFlags & OpEval)
		dsetint(0, pRtnVal);

	// If interactive, get one alias, command, or function name from user.
	if(!(sess.opFlags & OpScript)) {
		char prompt[strlen(text26) + strlen(type) + 2];
				// "Delete"
		sprintf(prompt, "%s %s", text26, type);
		if(getCFA(prompt, selector, &exec, type) != Success || exec.type == PtrNull)
			return sess.rtn.status;
		goto NukeIt;
		}

	// Script mode: get name(s) to delete.
	do {
		if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
			return sess.rtn.status;
		if(sess.opFlags & OpEval) {
			if(!execFind(pLastParse->tok.str, OpQuery, selector, &exec))
				(void) rsset(Success, RSNoWrap, text52, type, pLastParse->tok.str);
							// "No such %s \"%s\""
			else {
NukeIt:
				// Nuke it.
				if(selector & PtrUserCmdFunc) {
					if(bdelete(exec.u.pBuf, BC_IgnChgd) != Success)
						goto Fail;
					}
				else if(editAlias(exec.u.pAlias->name, OpDelete, NULL) != Success) {
Fail:
					// Don't cause a running script to fail on Failure status.
					if(sess.rtn.status < Failure)
						return sess.rtn.status;
					rsunfail();
					continue;
					}
				++count;
				}
			}
		} while(sess.opFlags & OpScript && getSym() == Success && needSym(s_comma, false));

	// If script mode...
	if(sess.opFlags & OpScript) {

		// Return count if evaluating.
		if(sess.opFlags & OpEval)
			dsetint(count, pRtnVal);
		}
	else if(count == 1)
		(void) rsset(Success, RSHigh, "%c%s %s", upCase[(int) *type], type + 1, text10);
										// "deleted"
	return sess.rtn.status;
	}

// Delete one or more alias(es).  Return status.
int delAlias(Datum *pRtnVal, long n, Datum **args) {

	return delCFA(pRtnVal, PtrAlias, text127);
					// "alias"
	}

// Delete one or more user routines (commands and/or functions).  Return status.
int delRoutine(Datum *pRtnVal, long n, Datum **args) {

	return delCFA(pRtnVal, PtrUserCmdFunc, text152);
					// "user routine"
	}

// Execute the contents of a buffer (of commands) and return result in pRtnVal.
int xeqBuf(Datum *pRtnVal, long n, Datum **args) {
	Buffer *pBuf;		// Pointer to buffer to execute.

	// Find out what buffer the user wants to execute, activate it if needed...
	if(getBufname(pRtnVal, text117, ArgFirst | OpDelete | OpCurBName, &pBuf, NULL) != Success || pBuf == NULL ||
			// "Execute"
	 (!(pBuf->flags & BFActive) && bactivate(pBuf) != Success))
		return sess.rtn.status;

	// and execute it with arg n.
#if MMDebug & Debug_Script
	return debug_execBuf(pRtnVal, n, pBuf, pBuf->filename, sess.opFlags & OpParens ? SRun_Parens : 0, "xeqBuf");
#else
	return execBuf(pRtnVal, n, pBuf, pBuf->filename, sess.opFlags & OpParens ? SRun_Parens : 0);
#endif
	}

// Look up a statement keyword, given indirect pointer to beginning of command line and length.  Update *pCmdLine and return
// symbol id if found, otherwise s_nil.
static Symbol findStmtKeyword(char **pCmdLine, int len) {
	char *cmdLine = *pCmdLine;

	if(isLowerCase(*cmdLine)) {
		Symbol s;
		ushort keywordLen;
		char workBuf[StmtKeywordMax + 2], *str = workBuf;

		// Copy enough of line into workBuf (null terminated) to do keyword comparisons.
		stplcpy(workBuf, cmdLine, (uint) len < (sizeof(workBuf) - 1) ? (uint) len + 1 : sizeof(workBuf));

		// Check if line begins with a statement keyword.
		if((s = getIdent(&str, &keywordLen)) >= kw_break) {
			*pCmdLine = cmdLine + keywordLen;
			return s;
			}
		}

	return s_nil;
	}

// Expand an error message from a user command or function execution which includes the file (preferred) or buffer name and line
// currently being executed, and set via rsset() with ScriptError severity.  If errorMsg0 is NULL, use sess.rtn.msg as the
// initial portion; if sess.rtn.msg is null, use a generic error message.  Return status.
static int userExecError(const char *errorMsg0, const char *errorMsg1, Buffer *pBuf, Line *pLine) {
	DFab msg;
	const char *name, *label;
	short delimChar;

	if(pBuf->filename != NULL) {
		name = pBuf->filename;
		label = text99;
			// "file"
		delimChar = '"';
		}
	else {
		name = pBuf->bufname;
		label = text83;
			// "buffer"
		delimChar = '\'';
		}

	// Build error message line.  errorMsg0 may be long and may be truncated when displayed on message line, but so be it.
	// The first portion of the message is the most important to see.
	if(dopentrack(&msg) != 0)
		goto LibFail;

	// Prepend errorMsg0 and errorMsg1.
	if(errorMsg0 == NULL && errorMsg1 == NULL && !disnull(&sess.rtn.msg)) {
		if(escMsgAttr(&msg) < Failure)
			return sess.rtn.status;
		}
	else if((errorMsg1 == NULL ? dputs(errorMsg0 != NULL ? errorMsg0 : text219, &msg, 0) :
								// "Script failed"
	 dputf(&msg, 0, errorMsg0, errorMsg1)) != 0)
		goto LibFail;
	if(dputf(&msg, 0, "%s %s %c%s%c %s %d", text229, label, delimChar, name, delimChar, text230,
					// ", in",				"at line"
	 getLineNum(pBuf, pLine)) != 0 || dclose(&msg, FabStr) != 0)
		goto LibFail;
	return rsset(ScriptError, RSForce | RSNoFormat | RSTermAttr, msg.pDatum->str);
LibFail:
	return libfail();
	}

// Free a list of loop block pointers, given head of list.
static void lbfree(LoopBlock *pBlock) {
	LoopBlock *pBlock0;

	while((pBlock0 = pBlock) != NULL) {
		pBlock = pBlock0->next;
		free((void *) pBlock0);
		}
	}

// Free any user command/function preprocessor storage in a buffer.
void preprocFree(Buffer *pBuf) {
	CallHdr *pCallHdr = pBuf->pCallHdr;
	if(pCallHdr != NULL) {
		if(pCallHdr->execBlocks != NULL) {
			lbfree(pCallHdr->execBlocks);
			pCallHdr->execBlocks = NULL;
			}
		dclear(&pCallHdr->argSyntax);
		dclear(&pCallHdr->descrip);
		pBuf->flags &= ~BFPreproc;		// Clear "preprocessed" flag.
		}
	}

#if MMDebug & Debug_Preproc
static void lbdump(Line *pLine, const char *label) {

	fprintf(logfile, "%s: [%.8x] ", label, (uint) pLine);
	if(pLine == NULL)
		fputs("NULL\n", logfile);
	else {
		char *str = pLine->text;
		int len = skipWhite(&str, pLine->used);
		fprintf(logfile, "%.*s\n", len, str);
		}
	}
#endif

// Preprocess a buffer and return status.  If no errors found, set pBuf->pCallHdr->execBlocks to result.  pBuf->pCallHdr is
// assumed to not be NULL.
static int preprocBuf(Buffer *pBuf) {
	Line *pLine;			// Pointer to line to execute.
	Symbol keywordId;		// Statement keyword ID
	int len;			// Line length.
	int saltLevel;			// "command" or "function" nesting depth (for error checking).
	LoopBlock *pExecBlock;		// Pointer to execution loop-block list.
	LoopBlock *pOpenBlock;		// Pointer during scan.
	LoopBlock *pTempBlock;		// Temporary pointer.
	char *lineText;			// Text of line to scan.
	const char *errorMsg1, *errorMsg2;
	bool skipLine;
	bool lastWasCL = false;		// Last line was a continuation line.

	// Scan the buffer to execute, building loop blocks for any loop statements found, regardless of "truth" state (which is
	// unknown at this point).  This allows for any loop block to be executed at execution time.
	pExecBlock = pOpenBlock = NULL;
	saltLevel = 0;
	pLine = pBuf->pFirstLine;
	do {
		// Scan the current line.
		lineText = pLine->text;
		len = pLine->used;
#if MMDebug & Debug_Preproc
		fprintf(logfile, "%.8x >> %.*s\n", (uint) pLine, len, lineText);
#endif
		// Skip line if last line was a continuation line.
		skipLine = lastWasCL;
		lastWasCL = len > 0 && lineText[len - 1] == '\\';
		if(skipLine)
			continue;

		// Find first non-whitespace character.  If line is blank or a comment, skip it.
		if((len = skipWhite(&lineText, len)) == 0 || *lineText == TokC_Comment)
			continue;

		// Check for a statement keyword.
		if((keywordId = findStmtKeyword(&lineText, len)) != s_nil) {
#if MMDebug & Debug_Preproc
		fprintf(logfile, "  found keywordId %.8x\n", keywordId);
#endif
			switch(keywordId) {
				case kw_command:
				case kw_function:	// command or function keyword: bump salting level.  Nested?
					if(++saltLevel > 1) {
						errorMsg1 = text419;
							// "Nested routine%s"
						errorMsg2 = text416;
							// " not allowed"
						goto FailExit;
						}
					break;
				case kw_endroutine:	// endroutine keyword: orphan?
					if(--saltLevel < 0) {
						errorMsg1 = text121;
							// "Unmatched '~b%s~B' keyword"
						errorMsg2 = text197;
							// "endroutine"
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
					if(pOpenBlock == NULL) {
						errorMsg1 = text120;
							// "'break' or 'next' outside of any loop block"
						errorMsg2 = NULL;
						goto FailExit;
						}
NewBlock:
					// Not an orphan, make a block.
					if((pTempBlock = (LoopBlock *) malloc(sizeof(LoopBlock))) == NULL)
						return rsset(Panic, 0, text94, "preprocBuf");
							// "%s(): Out of memory!"

					// Record line and block type.
					pTempBlock->pBlockBegin = pLine;
					pTempBlock->pEndLoopUp = pTempBlock->pEndLoop = NULL;
					pTempBlock->type = keywordId;

					// Add the block to the open list.
					pTempBlock->next = pOpenBlock;
					pOpenBlock = pTempBlock;
					break;
				case kw_endloop:	// "endloop" keyword: orphan?
					if(pOpenBlock == NULL) {
						errorMsg1 = text121;
							// "Unmatched '~b%s~B' keyword"
						errorMsg2 = text196;
							// "endloop"
						goto FailExit;
						}

					// Loop through records (including any BREAK or NEXT records) in the pOpenBlock stack,
					// record this line in them, and move them to the top of the pExecBlock list until one
					// loop (SLoopType) record is moved.  This will complete the most recent SLoopType
					// block.
					do {
						pOpenBlock->pEndLoop = pLine;
						if(pOpenBlock->type & SLoopType) {
							pTempBlock = pOpenBlock->next;

							// For loop records, record the block-begin line (temporarily) of the
							// *parent* loop record in this record's pEndLoopUp member so that it
							// can be set later to the parent's ENDLOOP line (which is needed for
							// multi-level breaks).  If there is no parent loop block, the
							// pEndLoopUp member will remain NULL, which is okay.
							while(pTempBlock != NULL) {
								if(pTempBlock->type & SLoopType) {

									// Found parent loop block.  Save block-begin pointer.
									pOpenBlock->pEndLoopUp = pTempBlock->pBlockBegin;
									break;
									}
								pTempBlock = pTempBlock->next;
								}
							}
						pTempBlock = pExecBlock;
						pExecBlock = pOpenBlock;
						pOpenBlock = pOpenBlock->next;
						pExecBlock->next = pTempBlock;
						} while(!(pExecBlock->type & SLoopType));
					break;
				default:
					;	// Do nothing.
				}
			}

		// On to the next line.
		} while((pLine = pLine->next) != NULL);

	// Buffer SCAN completed.  while/until/for/loop and "endloop" match?
	if(pOpenBlock != NULL) {
		errorMsg1 = text121;
			// "Unmatched '~b%s~B' keyword"
		errorMsg2 = text122;
			// "loop"
		goto FailExit;
		}

	// "command/function" and "endroutine" match?
	if(saltLevel > 0) {
		errorMsg1 = text314;
			// "Unmatched '~bcommand~B' or '~bfunction~B' keyword"
		errorMsg2 = NULL;
FailExit:
		(void) userExecError(errorMsg1, errorMsg2, pBuf, pLine);
		goto Exit;
		}

	// Everything looks good.  Last step is to fix up the loop records.
	for(pTempBlock = pExecBlock; pTempBlock != NULL; pTempBlock = pTempBlock->next) {
		if(pTempBlock->type & SLoopType && pTempBlock->pEndLoopUp != NULL) {
			LoopBlock *pTempBlock2 = pExecBlock;

			// Find loop block ahead of this block that contains this block's block-begin line.
			do {
				if(pTempBlock2->pBlockBegin == pTempBlock->pEndLoopUp) {

					// Found parent block.  Set this loop's pEndLoopUp to parent's
					// ENDLOOP line (instead of the block-begin line).
					pTempBlock->pEndLoopUp = pTempBlock2->pEndLoop;
					goto NextFix;
					}
				} while((pTempBlock2 = pTempBlock2->next) != NULL &&
				 pTempBlock2->pBlockBegin != pTempBlock->pBlockBegin);

			// Huh?  Matching loop block not found! (This is a bug.)
			(void) rsset(Failure, 0, text220, getLineNum(pBuf, pTempBlock->pBlockBegin));
				// "Parent block of loop block at line %'d not found during buffer scan"
			goto RCExit;
			}
NextFix:;
		}

#if MMDebug & Debug_Preproc
	// Dump blocks to log file.
	fputs("\nLOOP BLOCKS\n", logfile);
	for(pTempBlock = pExecBlock; pTempBlock != NULL; pTempBlock = pTempBlock->next) {
		char *str;
		switch(pTempBlock->type) {
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
		fprintf(logfile, "-----\n Type: %.4x %s\n", pTempBlock->type, str);
		lbdump(pTempBlock->pBlockBegin, " BlockBegin");
		lbdump(pTempBlock->pEndLoop, " EndLoop");
		lbdump(pTempBlock->pEndLoopUp, "EndLoopUp");
		}
//	return rsset(FatalError, RSNoFormat, "preprocBuf() exit");
#endif
	// Success!  Save block pointer in buffer-extension record and set "preprocessed" flag.
	pBuf->pCallHdr->execBlocks = pExecBlock;
	pBuf->flags |= BFPreproc;
	goto Exit;

	// Clean up and exit per sess.rtn.status.
RCExit:
	lbfree(pOpenBlock);
	(void) userExecError(NULL, NULL, pBuf, pLine);
Exit:
	if(sess.rtn.status == Success)
		return sess.rtn.status;
	lbfree(pExecBlock);
	return sess.rtn.status;
	}

// Clear and invalidate for loop header.
static void clearFor(ForLoopHdr *pForHdr) {

	if(pForHdr->str != NULL) {
		free((void *) pForHdr->str);
		pForHdr->str = NULL;
		}
	pForHdr->type = 0;
	}

// Free execution level storage.
static void lfree(ExecLevel *pLevel) {
	ExecLevel *pLevel1;

	do {
		pLevel1 = pLevel->next;
		if(pLevel->pForHdr != NULL) {
			clearFor(pLevel->pForHdr);
			free((void *) pLevel->pForHdr);
			}
		free((void *) pLevel);
		} while((pLevel = pLevel1) != NULL);
	}

// Move forward to a new execution level (for loop or if statement), creating a level object if needed.
static int nextLevel(ExecLevel **ppLevel) {
	ExecLevel *pLevel = *ppLevel;

	if(pLevel == NULL || pLevel->next == NULL) {
		ExecLevel *pLevel1;

		if((pLevel1 = (ExecLevel *) malloc(sizeof(ExecLevel))) == NULL)
			return rsset(Panic, 0, text94, "nextlevel");
				// "%s(): Out of memory!"
		pLevel1->pForHdr = NULL;
		pLevel1->next = NULL;
		if(pLevel == NULL) {
			pLevel1->prev = NULL;
#if MMDebug & (Debug_Script | Debug_Preproc)
			pLevel1->level = 0;
#endif
			}
		else {
			pLevel->next = pLevel1;
			pLevel1->prev = pLevel;
#if MMDebug & (Debug_Script | Debug_Preproc)
			pLevel1->level = pLevel->level + 1;
#endif
			}
		pLevel = pLevel1;
		}
	else {
		pLevel = pLevel->next;
		if(pLevel->pForHdr != NULL) {
			free((void *) pLevel->pForHdr);
			pLevel->pForHdr = NULL;
			}
		}

	pLevel->ifWasTrue = pLevel->elseSeen = false;
	pLevel->loopCount = 0;
	*ppLevel = pLevel;

	return sess.rtn.status;
	}

// Return to the most recent loop level (bypassing "if" levels).  Top level should never be reached.
static int prevLoopLevel(ExecLevel **ppLevel) {
	ExecLevel *pLevel = *ppLevel;

	for(;;) {
#if SanityCheck
		if(pLevel->prev == NULL)

			// Huh?  Loop level not found!  (This is a bug.)
			return rsset(Failure, RSNoFormat, text992);
				// "Prior loop execution level not found while rewinding stack"
#endif
		if(pLevel->loopSpawn)
			break;
		pLevel = pLevel->prev;
		}
	*ppLevel = pLevel;
	return sess.rtn.status;
	}

// Parse one user command/function help string.
static int getCmdFuncText(Datum *pDatum, Symbol sym, ushort *pHaveText) {
	static char keyword1[] = CmdFuncKeywd1;
	static char keyword2[] = CmdFuncKeywd2;
	char workBuf[32];

	if(!needSym(sym, true) || !haveSym(s_ident, true))
		return sess.rtn.status;
	if(strcmp(pLastParse->tok.str, keyword1) == 0) {
		if(*pHaveText == 1)
			goto DupErr;
		*pHaveText = 1;
		}
	else if(strcmp(pLastParse->tok.str, keyword2) == 0) {
		if(*pHaveText == 2) {
DupErr:
			sprintf(workBuf, "'%s'", *pHaveText == 2 ? keyword1 : keyword2);
			goto ErrRtn;
			}
		*pHaveText = 2;
		}
	else {
		sprintf(workBuf, "'%s' or '%s'", keyword1, keyword2);
ErrRtn:
		return rsset(Failure, 0, text4, workBuf, pLastParse->tok.str);
				// "%s expected (at token '%s')"
		}
	if(getSym() == Success && needSym(s_colon, true))
		(void) funcArg(pDatum, ArgFirst | ArgNotNull1);
	return sess.rtn.status;
	}

// Parse first line of a user command or function definition, beginning at name, and return status.  bufFlags is assumed to have
// either BFCommand or BFFunc flag set.  Example definition:
//	command grepFiles(2,)\
//	 {arguments: 'pat, glob', description: 'Find files which match pattern'}
//		...
//	endroutine
// Return value is set to command/function name as a comment.
static int getCmdFunc(Datum *pRtnVal, Buffer **ppCallBuf, ushort bufFlags) {
	ExecPtr exec;
	CallHdr *pCallHdr;
	bool whitespace, exists;
	Datum *pDatum1, *pDatum2;
	char *pCmdFuncName;
	int cmdFuncLen = strlen(text247);
		// "function"
	ushort haveText1 = 0, haveText2 = 0;
	short minArgs = 0, maxArgs = -1;

	if((pDatum1 = dnewtrack()) == NULL || (pDatum2 = dnewtrack()) == NULL)
		goto LibFail;

	// Get command or function name.
	if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
		return sess.rtn.status;
	if(strlen(pLastParse->tok.str) > MaxBufname - 1)
		return rsset(Failure, 0, text232, pLastParse->tok.str, MaxBufname - 1);
			// "Command or function name '%s' cannot exceed %d characters"

	// Construct the buffer header (and name) in pRtnVal.
	if(dsalloc(pRtnVal, strlen(pLastParse->tok.str) + cmdFuncLen + 4) != 0)
		goto LibFail;
	pRtnVal->str[0] = TokC_Comment;
	pRtnVal->str[1] = ' ';
	pCmdFuncName = stpcpy(pRtnVal->str + 2, bufFlags == BFFunc ? text247 : text158);
								// "function", "command"
	// Create buffer name (temporarily) for duplicate check.
	*pCmdFuncName++ = BCmdFuncLead;
	strcpy(pCmdFuncName, pLastParse->tok.str);

	// Get argument count.
	whitespace = haveWhite();
	if(getSym() != NotFound) {
		if(sess.rtn.status != Success)
			return sess.rtn.status;
		if(haveSym(s_leftParen, false)) {
			if(whitespace)
				return rsset(Failure, 0, text4, "'('", " ");
						// "%s expected (at token '%s')"
			if(getSym() < NotFound)
				return sess.rtn.status;
			if(!needSym(s_rightParen, false)) {
				if(funcArg(pDatum1, ArgFirst | ArgInt1) != Success)
					return sess.rtn.status;
				if((minArgs = dgetint(pDatum1)) < 0)
					goto Err;
				if(!haveSym(s_comma, false))
					maxArgs = minArgs;
				else {
					if(getSym() < NotFound || (!haveSym(s_rightParen, false) &&
					 funcArg(pDatum2, ArgFirst | ArgInt1) != Success))
						return sess.rtn.status;
					if(!disnil(pDatum2))
						if((maxArgs = dgetint(pDatum2)) < 0) {
							minArgs = maxArgs;
Err:
							return rsset(Failure, 0, text39, text206, (long) minArgs, 0);
								// "%s (%'ld) must be %d or greater"
								// "Command or function argument count"
							}
					}
				if(!needSym(s_rightParen, true))
					return sess.rtn.status;
				if(maxArgs >= 0 && minArgs > maxArgs)
					return rsset(Failure, RSNoFormat, text386);
						// "Command or function declaration contains invalid argument range"
				}
			}
		if(haveSym(s_any, false)) {

			// Get help string(s).
			if(getCmdFuncText(pDatum1, s_leftBrace, &haveText1) != Success)
				return sess.rtn.status;
			if(haveSym(s_comma, false)) {
				haveText2 = haveText1;
				if(getCmdFuncText(pDatum2, s_comma, &haveText2) != Success)
					return sess.rtn.status;
				}
			if(!needSym(s_rightBrace, true) || extraSym())
				return sess.rtn.status;
			}
		}

	// Existing command, function, alias, or local variable of same name as this user command or function?
	if(((exists = execFind(pCmdFuncName, OpQuery, PtrAny, &exec)) && (!(exec.type & PtrUserCmdFunc) ||
	 !(mode.cache[MdIdxClob]->flags & MdActive))) || findLocalVar(pCmdFuncName) != NULL)
		return rsset(Failure, RSTermAttr, text165, pCmdFuncName, text109);
					// "Name '~b%s~B'%s", " already in use"

	// Create a hidden buffer, make sure it's empty, and has the correct pointer type in execTable.
	if(bfind(pCmdFuncName - 1, BN_Create | BN_Extend, bufFlags | BFHidden | BFReadOnly, ppCallBuf, NULL) != Success)
		return sess.rtn.status;
	if(exists) {
		if(bclear(*ppCallBuf, BC_IgnChgd | BC_Unnarrow | BC_ClrFilename) != Success)
			return sess.rtn.status;
		exec.type = (bufFlags == BFCommand) ? PtrUserCmd : PtrUserFunc;
		(void) execFind(pCmdFuncName, OpUpdate, 0, &exec);
		(*ppCallBuf)->flags = ((*ppCallBuf)->flags & ~BFCmdFunc) | bufFlags;
		}

	// Set the buffer extension parameters.
	pCallHdr = (*ppCallBuf)->pCallHdr;
	pCallHdr->minArgs = minArgs;
	pCallHdr->maxArgs = maxArgs;
	if(haveText1)
		dxfer(haveText1 == 1 ? &pCallHdr->argSyntax : &pCallHdr->descrip, pDatum1);
	if(haveText2)
		dxfer(haveText2 == 1 ? &pCallHdr->argSyntax : &pCallHdr->descrip, pDatum2);
	pCmdFuncName[-1] = ' ';

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Parse a "for" line and initialize control variables.  Return status.
static int initFor(ForLoopHdr **ppForHdr, Datum *pDatum) {
	ExprNode node;
	ForLoopHdr *pForHdr;
	static const char myName[] = "initFor";

	if((pForHdr = *ppForHdr) == NULL) {
		if((pForHdr = (ForLoopHdr *) malloc(sizeof(ForLoopHdr))) == NULL)
			goto ErrRtn;
		pForHdr->str = NULL;
		*ppForHdr = pForHdr;
		}

	// Get the variable name and "in" keyword.
	if(!haveSym(s_any, true) || vfind(pLastParse->tok.str, &pForHdr->varDesc, OpCreate) != Success ||
	 getSym() < NotFound || !needSym(kw_in, true))
		return sess.rtn.status;

	// Get array, hash, or string expression.
	nodeInit(&node, pDatum, 0);
	if(ge_assign(&node) != Success || extraSym() || !isAryHshStrVal(node.pVal))
		return sess.rtn.status;

	// Finish up.
	pForHdr->type = pDatum->type;
	if(dtyphash(pDatum))
		heachinit(&pForHdr->e.hashEach, dgethashptr(pDatum));
	else if(dtyparray(pDatum))
		aeachinit(&pForHdr->e.arrayEach, dgetarrayptr(pDatum));
	else if((pForHdr->e.next = pForHdr->str = strdup(pDatum->str)) == NULL)
ErrRtn:
		return rsset(Panic, 0, text94, myName);
			// "%s(): Out of memory!"
	return sess.rtn.status;
	}

// Get next array element, hash node, or string character and set value in variable referenced in *pForHdr object.  If no items
// left, return NotFound, otherwise status.
static int nextForItem(ForLoopHdr *pForHdr) {
	Datum *pDatum;
	Datum datum;

	// If iterating through a hash, return next node as a [key, value] array.
	if(pForHdr->type & DHashMask) {
		Array *pArray;
		HashNode *pNode = heach(&pForHdr->e.hashEach);
		if(pNode == NULL)
			goto Done;
		if((pArray = hntoa(pNode)) == NULL)
			return libfail();
		dinit(&datum);
		astash(pDatum = &datum, pArray);
		}
	else if(pForHdr->type & DArrayMask) {

		// Return next array element.
		if((pDatum = aeach(&pForHdr->e.arrayEach)) == NULL)
			goto Done;
		}
	else {
		// Return next string character.
		if(*pForHdr->e.next == '\0') {
Done:
			clearFor(pForHdr);	// Invalidate control object.
			return NotFound;
			}
		dinit(&datum);
		dsetint(*pForHdr->e.next++, pDatum = &datum);
		}

	return setVar(&pForHdr->varDesc, pDatum);
	}

// Execute a compiled buffer, save result in pRtnVal, and return status.
static int xbuf(Datum *pRtnVal, Buffer *pBuf) {
	Line *pBeginLine, *pEndLine;	// Pointers to beginning and ending line(s) to execute, allowing for continuation lines.
	bool force;			// "force" statement?
	Symbol keywordId;		// Keyword index.
	int len;			// Line length.
	bool isWhiteLine;		// True if current line is all white space or a comment.
	bool lastWasCL = false;		// Last line was a continuation line.
	bool thisIsCL = false;		// Current line is a continuation line.
	bool go;			// Used to evaluate "while" and "until" truth.
	int breakLevel;			// Number of levels to "break".
	ushort bufFlags;
	ExecLevel *levelTable = NULL;	// Level table.
	ExecLevel *pLevel, *pLevel1;	// Current level.
	LoopBlock *pBlock;
	Buffer *pCallBuf = NULL;	// "cmd" or "func" buffer pointer.
	Datum *pDatum;			// Work Datum object.
	Datum *pFullLine;		// Null-terminated buffer line (Datum object).
	char *lineText;			// Work pointer.
	char *exprText;			// Beginning of expression statement (past any statement keyword).
	const char *errorMsg1, *errorMsg2;
	Parse *pOldLast, newLast;
	ExprNode node;
#if MMDebug & (Debug_Script | Debug_Preproc)
	static uint instance = 0;
	uint myInstance = ++instance;
#endif
	// Prepare for execution.
	pBeginLine = pEndLine = pBuf->pFirstLine;
	if((pDatum = dnewtrack()) == NULL || (pFullLine = dnewtrack()) == NULL)
		goto LibFail;
	if(nextLevel(&levelTable) != Success)
		return sess.rtn.status;
	pLevel = levelTable;
	pLevel->live = true;			// Set initial (top) execution level to true.
	pLevel->loopSpawn = false;
	breakLevel = 0;
	keyEntry.curFlags = keyEntry.prevFlags;	// Let the first command inherit the flags from the last one.
	newLast.src = NULL;

	// Now EXECUTE the buffer.
#if MMDebug & (Debug_Script | Debug_Preproc)
	fprintf(logfile, "*** BEGIN xbuf(%u, %s): %s...\n", myInstance, pBuf->bufname, showFlags(&newLast));
#endif
	do {
		// Skip blank lines and comments unless last line was a continuation line or "salting" a user routine.
		lineText = pEndLine->text;
		len = pEndLine->used;
#if MMDebug & (Debug_Script | Debug_Preproc)
		fprintf(logfile, "%.8x [level %u.%u] %c>> %.*s\n",
		 (uint) pEndLine, myInstance, pLevel->level, pLevel->live ? '!' : ' ', len, lineText);
#endif
		isWhiteLine = false;
		if((len = skipWhite(&lineText, len)) == 0 || *lineText == TokC_Comment) {
			isWhiteLine = true;
			thisIsCL = false;
			if(!lastWasCL) {
				if(pCallBuf != NULL)
					goto Salt0;
				goto NextLn;
				}
			}
		else {
			if(lastWasCL) {
				// Undo skipWhite() so that leading white space in a multi-line string literal is preserved.
				lineText = pEndLine->text;
				len = pEndLine->used;
				}
			if((thisIsCL = (lineText[len - 1] == '\\')))
				if(pEndLine->next == NULL) {
					(void) rsset(Failure, 0, text405, len, lineText);
						// "Incomplete line \"%.*s\""
					goto RCExit;
					}
			}

		// Allocate lineText as a Datum object, null terminated, so that it can be parsed properly by getSym().
		if(!lastWasCL) {
			if(dsetsubstr(lineText, thisIsCL ? len - 1 : len, pFullLine) != 0)
				goto LibFail;
			pBeginLine = pEndLine;		// Process as if all continued lines were prepended to this line.
			}
		else if(!isWhiteLine) {
			DFab fab;

			// Append current line to pFullLine.
			if(dopenwith(&fab, pFullLine, FabAppend) != 0 || dputmem((void *) lineText, thisIsCL ? len - 1 : len,
			 &fab, 0) != 0 || dclose(&fab, FabStr) != 0) {
LibFail:
				(void) libfail();
				goto RCExit;
				}
			}

		// Check for "endroutine" keyword now so that "salting" can be turned off if needed; otherwise, check for
		// statement keywords only if current line is not a continuation line.  Skip past any leading /#...#/ comment
		// first.
		if((lineText = nonWhite(pFullLine->str, true)) == NULL)
			goto RCExit;
		if(*lineText == '\0') {
			// Have /#...#/ comment on a "line" by itself (which includes any prior continuation lines).  Salt it if
			// needed; otherwise, skip it.
			if(!lastWasCL && !thisIsCL && pCallBuf != NULL)
				goto Salt0;
			goto NextLn;
			}
		exprText = lineText;
		if((keywordId = findStmtKeyword(&exprText, strlen(exprText))) != s_nil) {
			if(thisIsCL) {
				if(keywordId == kw_endroutine && pLevel->live) {
					pCallBuf = NULL;
					goto NextLn;
					}
				}
			else {
				// Have complete line in pFullLine.  Process any "command", "function", or "endroutine"
				// statements.
				bufFlags = 0;
				if(parseBegin(&pOldLast, &newLast, exprText, 0) != Success)
					goto RCExit;
				switch(keywordId) {
					case kw_command:
						bufFlags |= BFCommand;
						goto DoCmdFunc;
					case kw_function:
						bufFlags |= BFFunc;
DoCmdFunc:
						// Skip if current level is false.
						if(!pLevel->live)
							goto NextLn;

						// Parse line...
						if(getCmdFunc(pRtnVal, &pCallBuf, bufFlags) != Success)
							goto RCExit;

						// and store command/function name as a comment at the top of the new buffer.
						len = strlen(lineText = pRtnVal->str);
						goto Salt1;
					case kw_endroutine:

						// Extra arguments?
						if(extraSym())
							goto RCExit;

						// Turn off command/function storing if live.
						if(pLevel->live) {

							// Move point to beginning of command/function buffer...
							pCallBuf->face.point.pLine = pCallBuf->pFirstLine;
							pCallBuf->face.point.offset = 0;

							// and disable it.
							pCallBuf = NULL;
							}
						goto NextLn;
					default:
						;	// Do nothing.
					}
				}
			} // End statement keyword parsing.

		// If user command/function store is on, just salt this (entire) line away.
		if(pCallBuf != NULL) {
			Line *pLine;
Salt0:
			// Save line verbatim (as a debugging aid) but skip any leading tab, if possible.
			lineText = pEndLine->text;
			if((len = pEndLine->used) > 0 && !lastWasCL && *lineText == '\t') {
				++lineText;
				--len;
				}
Salt1:
			// Allocate the space for the line.
			if(lalloc(len, &pLine) != Success)
				return sess.rtn.status;

			// Copy the text into the new line.
			memcpy(pLine->text, lineText, len);
			if(lineText == pRtnVal->str)
				dsetnil(pRtnVal);

			// Attach the line to the end of the buffer.
			llink(pLine, pCallBuf, pCallBuf->pFirstLine->prev);
			goto NextLn;
			}
		if(thisIsCL)
			goto NextLn;

		// Not "salting" and not a "command", "function", or "endroutine" keyword.  Check others.
		force = false;
		if(keywordId != s_nil) {
			switch(keywordId) {

				// Statements in the if/else family.
				case kw_if:	// "if" keyword.

					// Begin new level.
					if(nextLevel(&pLevel) != Success)
						return sess.rtn.status;
					pLevel->loopSpawn = false;

					// Prior level true?
					if(pLevel->prev->live) {
EvalIf:
						// Yes, grab the value of the logical expression.
						nodeInit(&node, pDatum, 0);
						if(ge_andOr(&node) != Success || extraSym())
							goto RCExit;

						// Set this level to the result of the expression and update "ifWasTrue" if the
						// result is true.
						if((pLevel->live = toBool(pDatum)))
							pLevel->ifWasTrue = true;
						}
					else
						// Prior level was false so this one is, too.
						pLevel->live = false;
					goto NextLn;
				case kw_else:	// else keyword.

					// At top level, initiated by a loop statement, or duplicate else?
					if(pLevel == levelTable || pLevel->loopSpawn || pLevel->elseSeen) {

						// Yep, bail out.
						errorMsg1 = text198;
							// "Misplaced '~b%s~B' keyword"
						errorMsg2 = "else";
						goto Misplaced;
						}

					// Extra arguments?
					if(extraSym())
						goto RCExit;

					// Update current execution state.  Can be true only if (1), prior state is true; (2),
					// current state is false; and (3), this (possibly compound) "if" statement was never
					// true.
					pLevel->live = pLevel->prev->live && !pLevel->live && !pLevel->ifWasTrue;
					pLevel->elseSeen = true;
					goto NextLn;
				case kw_elsif:	// "elsif" keyword.

					// At top level, initiated by a loop statement, or "elsif" follows "else"?
					if(pLevel == levelTable || pLevel->loopSpawn || pLevel->elseSeen) {

						// Yep, bail out.
						errorMsg1 = text198;
							// "Misplaced '~b%s~B' keyword"
						errorMsg2 = "elsif";
						goto Misplaced;
						}

					// Check expression if prior level is true, current level is false, and this compound if
					// statement has not yet been true.
					if(pLevel->prev->live && !pLevel->live && !pLevel->ifWasTrue)
						goto EvalIf;

					// Not evaluating so set current execution state to false.
					pLevel->live = false;
					goto NextLn;
				case kw_endif:	// "endif" keyword.

					// At top level or was initiated by a loop statement?
					if(pLevel == levelTable || pLevel->loopSpawn) {

						// Yep, bail out.
						errorMsg1 = text198;
							// "Misplaced '~b%s~B' keyword"
						errorMsg2 = "endif";
						goto Misplaced;
						}

					// Extra arguments?
					if(extraSym())
						goto RCExit;

					// Done with current level: return to previous one.
					pLevel = pLevel->prev;
					goto NextLn;

				// Statements that begin or end a loop.
				case kw_loop:	// "loop" keyword.

					// Skip unless current level is true.
					if(!pLevel->live)
						goto JumpEnd;

					// Error if extra arguments; otherwise, begin next loop.
					if(extraSym())
						goto RCExit;
					goto NextLoop;
				case kw_for:	// "for" keyword

					// Current level true?
					if(pLevel->live) {
						int status;

						// Yes, parse line and initialize loop controls if first iteration.
						if((pLevel->pForHdr == NULL || pLevel->pForHdr->type == 0) &&
						 initFor(&pLevel->pForHdr, pDatum) != Success)
							goto RCExit;

						// Set loop variable to next item and begin next loop, or break if none left.
						if((status = nextForItem(pLevel->pForHdr)) == Success)
							goto NextLoop;
						if(status != NotFound)
							goto RCExit;

						// All iterations completed... jump to end of loop block.
						}
					goto JumpEnd;
				case kw_while:	// "while" keyword.
					go = true;
					goto DoLoop;
				case kw_until:	// "until" keyword.
					go = false;
					// Fall through.
DoLoop:
					// Current level true?
					if(pLevel->live) {

						// Yes, get the "while" or "until" logical expression.
						nodeInit(&node, pDatum, 0);
						if(ge_andOr(&node) != Success || extraSym())
							goto RCExit;

						// If the "while" condition is true or the "until" condition if false, begin a
						// new level, set it to true, and continue.
						if(toBool(pDatum) == go) {
NextLoop:
							if(nextLevel(&pLevel) != Success)
								return sess.rtn.status;
							pLevel->live = true;
							pLevel->loopSpawn = true;
							goto NextLn;
							}
						}

					// Current level or condition is false: skip this block.
					goto JumpEnd;

				case kw_break:	// "break" keyword.
				case kw_next:	// "next" keyword.

					// Ignore if current level is false (nothing to jump out of).
					if(pLevel->live == false)
						goto NextLn;

					// Check "break" argument.
					if(keywordId == kw_break) {
						if(haveSym(s_any, false)) {
							nodeInit(&node, pDatum, 0);
							if(ge_assign(&node) != Success || extraSym() || !isIntVal(pDatum))
								goto RCExit;
							if(dgetint(pDatum) <= 0) {
								(void) rsset(Failure, 0, text217, dgetint(pDatum));
									// "'break' level '%ld' must be 1 or greater"
								goto RCExit;
								}
							breakLevel = dgetint(pDatum);
							}
						else
							breakLevel = 1;

						// Invalidate any "for" loop in progress.
						pLevel1 = pLevel;
						if(prevLoopLevel(&pLevel1) != Success)
							goto RCExit;
						if(pLevel1->prev->pForHdr != NULL)
							clearFor(pLevel1->prev->pForHdr);
						}

					// Extraneous "next" argument(s)?
					else if(extraSym())
						goto RCExit;
JumpEnd:
					// Continue loop or break out of it.  If arrived here from top of loop block, then
					// either current level is not live or loop condition indicates "done"; otherwise,
					// processing a break or next statement within a loop block.
					//
					// Find the right block (kw_while, kw_until, kw_loop, kw_for, kw_break, or kw_next) and
					// jump down to its "endloop" or the line that follows it.
					for(pBlock = pBuf->pCallHdr->execBlocks; pBlock != NULL; pBlock = pBlock->next)
						if(pBlock->pBlockBegin == pBeginLine) {
							if(keywordId & SBreakType) {

								// Have "break" or "next".  Set the line pointer to the
								// "endloop" line so that the "endloop" is executed and return
								// to the most recent loop level (bypassing "if" levels).
								pEndLine = pBlock->pEndLoop;
								if(prevLoopLevel(&pLevel) != Success)
									goto RCExit;
								}
							else {
								// Jumped here from top of loop block -> done iterating and at
								// correct level.  Set the line pointer to the line after the
								// "endloop" line so that the "endloop" is bypassed, and reset
								// the loop counter.
								pEndLine = pBlock->pEndLoop->next;
								pLevel->loopCount = 0;
								}

							goto Onward;
							}

					// Huh?  "endloop" line not found! (This is a bug.)
					goto LoopBugExit;

				case kw_endloop:	// "endloop" keyword.

					// Continue loop or break out of it.  Arrived here normally from a "loop", "for" or
					// "while" that's true, or an "until" that's false, OR jumped here from a "break" or
					// "next".
					//
					// Extra arguments?
					if(extraSym())
						goto RCExit;

					// If not a "break"...
					if(breakLevel == 0) {

						// Is current level top or was initiated by an "if" statement?
						if(pLevel == levelTable || !pLevel->loopSpawn) {

							// Yep, bail out.
							errorMsg1 = text198;
								// "Misplaced '~b%s~B' keyword"
							errorMsg2 = "endloop";
Misplaced:
							(void) rsset(Failure, RSTermAttr, errorMsg1, errorMsg2);
							goto RCExit;
							}

						// Return to the previous level and check if maxLoop exceeded.
						pLevel = pLevel->prev;
						if(maxLoop > 0 && ++pLevel->loopCount > maxLoop) {
							(void) rsset(Failure, 0, text112, maxLoop);
								// "Maximum number of loop iterations (%'d) exceeded!"
							goto RCExit;
							}
						}

					// We're good... just find the "loop", "for", "while" or "until", and go back to it, or
					// to a prior level if we're processing a "break".
					for(pBlock = pBuf->pCallHdr->execBlocks; pBlock != NULL; pBlock = pBlock->next)
						if(pBlock->type & SLoopType && pBlock->pEndLoop == pBeginLine) {

							// while/until/loop/for block found: set the line pointer to the line
							// after its "endloop" line or its parent's "endloop" line if we're
							// processing a "break" (and return to the previous execution level);
							// otherwise, back to the beginning.
							if(breakLevel > 0) {
								if(--breakLevel > 0) {
									if(pBlock->pEndLoopUp == NULL) {
										(void) rsset(Failure, 0, text225, breakLevel);
									// "Too many break levels (%d short) from inner 'break'"
										goto RCExit;
										}
									pEndLine = pBlock->pEndLoopUp;

									// Return to the most recent loop level before this one.
									pLevel = pLevel->prev;
									if(prevLoopLevel(&pLevel) != Success)
										goto RCExit;
									}
								else {
									// Now at final "break"; that is, at correct parent loop
									// block and its level (inside block).  Move past
									// "endloop" and up one level.
									pEndLine = pEndLine->next;
									pLevel = pLevel->prev;
									}
								pLevel->loopCount = 0;	// Reset the loop counter.
								}
							else
								pEndLine = pBlock->pBlockBegin;
							goto Onward;
							}
LoopBugExit:
					// Huh?  "while", "until", "for", or "loop" line not found! (This is a bug.)
					(void) rsset(Failure, RSNoFormat, "Script loop boundary line not found");
					goto RCExit;

				case kw_return:	// "return" keyword.

					// Current level true?
					if(pLevel->live == true) {

						// Yes, get return value, if any.
						if(!haveSym(s_any, false))
							dsetnil(pRtnVal);
						else {
							nodeInit(&node, pRtnVal, 0);
							if(ge_andOr(&node) != Success || extraSym())
								goto RCExit;
							}

						// Exit script.
						goto Exit;
						}

					// Nope, ignore "return".
					goto NextLn;

				case kw_force:	// "force" keyword.

					// Check if argument present.
					if(!haveSym(s_any, true))
						goto RCExit;
					force = true;
					break;
#if SanityCheck
				default:
					// Huh?  Unknown keyword!  (This is a bug.)
					(void) rsset(Failure, 0, text993, (int)(exprText - lineText), lineText);
						// "xbuf(): Error: Unknown keyword '%.*s'",
					goto RCExit;
#endif

				}
			} // End statement keyword execution.

		// A "force" or not a statement keyword.  Execute the line as an expression statement if "true" state.
		if(pLevel->live) {
			if(newLast.src == NULL) {

				// No statement keyword -- need to call parseBegin().
				(void) execExprStmt(pRtnVal, exprText, ParseStmtEnd, NULL);
				}
			else {
				// Force -- parseBegin() already called.  Set ParseStmtEnd flag so that all expressions on
				// script line are forced; e.g., "force ...; ...".
				pLastParse->flags |= ParseStmtEnd;
				(void) xstmt(pRtnVal);
				}

			// Check for exit or a command error.
			if(sess.rtn.status <= MinExit)
				return sess.rtn.status;

			if(force)		// Force Success status.
				rsclear(0);
			else if(sess.rtn.status != Success) {
				EWindow *pWind = sess.windHead;

				// Check if buffer is on-screen.
				do {
					if(pWind->pBuf == pBuf) {

						// Found a window.  Set point to error line.
						pWind->face.point.pLine = pBeginLine;
						pWind->face.point.offset = 0;
						pWind->face.wflags |= WFMove;
						}
					} while((pWind = pWind->next) != NULL);

				// In any case, set the buffer point.
				pBuf->face.point.pLine = pBeginLine;
				pBuf->face.point.offset = 0;

				// Build a more detailed message that includes the command error message, if any.
				(void) userExecError(NULL, NULL, pBuf, pBeginLine);
				goto Exit;
				}
			} // End statement execution.
NextLn:
		pEndLine = pEndLine->next;
Onward:
		// On to the next line.  lineText and len point to the original line.
		if(!(lastWasCL = thisIsCL) && newLast.src != NULL) {
			parseEnd(pOldLast);
			newLast.src = NULL;
			}
		} while(pEndLine != NULL);		// End buffer execution.

	// "if" and "endif" match?
	if(pLevel == levelTable)
		goto Exit;
	(void) rsset(Failure, RSTermAttr, text121, text199);
		// "Unmatched '~b%s~B' keyword", "if"

	// Clean up and exit per sess.rtn.status.
RCExit:
	(void) userExecError(NULL, NULL, pBuf, pBeginLine);
Exit:
#if MMDebug & (Debug_Script | Debug_Preproc)
	fprintf(logfile, "*** END xbuf(%u, %s), sess.rtn.msg \"%s\"...\n", myInstance, pBuf->bufname, sess.rtn.msg.str);
	--myInstance;
#endif
	lfree(levelTable);
	if(newLast.src != NULL)
		parseEnd(pOldLast);

	return (sess.rtn.status == Success) ? sess.rtn.status : rsset(ScriptError, 0, NULL);
	}

// Execute the contents of a buffer, given Datum object for return value, numeric argument, buffer pointer, pathname of file
// being executed (or NULL), and invocation flags (which are passed to argLoad()).  Return status.
int execBuf(Datum *pRtnVal, long n, Buffer *pBuf, char *runPath, uint flags) {

	// Check for narrowed buffer and runaway recursion.
	if(pBuf->flags & BFNarrowed)
		(void) rsset(Failure, RSNoFormat, text72);
			// "Cannot execute a narrowed buffer"
	else if(maxCallDepth > 0 && pBuf->pCallHdr != NULL && pBuf->pCallHdr->execCount >= maxCallDepth)
		(void) rsset(Failure, 0, text319, text313, maxCallDepth);
			// "Maximum %s recursion depth (%'d) exceeded", "command or function"
	else {
		Datum args;

		// Create buffer-extension record if needed.
		if(pBuf->pCallHdr == NULL && bextend(pBuf) != Success)
			return sess.rtn.status;

		// Get call arguments.
		dinit(&args);		// No need to dclear() this later because it will always contain an array reference.
#if MMDebug & Debug_CallArg
		if(argLoad(n, pBuf, &args, flags) == Success) {
#else
		if(argLoad(pBuf, &args, flags) == Success) {
#endif
			// If evaluating, preprocess buffer if needed.
			if(pBuf->flags & BFPreproc || preprocBuf(pBuf) == Success) {
				ScriptRun *pOldRun, newRun;

				// Make new run instance and prepare for execution.
				pOldRun = pScriptRun;
				pScriptRun = &newRun;
				if((pScriptRun->msgFlag = mode.cache[MdIdxRtnMsg]->flags & MdActive))
					clearGlobalMode(mode.cache[MdIdxRtnMsg]);
				pScriptRun->pArgs = args.u.pArray;
				pScriptRun->path = fixNull(runPath);
				pScriptRun->pBuf = pBuf;
				pScriptRun->n = n;
				pScriptRun->pVarStack = localVarRoot;

				// Flag that we are executing the buffer and execute it.
				++pBuf->pCallHdr->execCount;
#if MMDebug & Debug_Script
				fprintf(logfile, "execBuf(): Calling xbuf(%s): %s...\n", pBuf->bufname, showFlags(NULL));
#endif
				(void) xbuf(pRtnVal, pBuf);			// Execute the buffer.
#if MMDebug & Debug_Script
				fprintf(logfile, "execBuf(): xbuf() returned %s result and status %d \"%s\".\n",
				 dtype(pRtnVal, false), sess.rtn.status, sess.rtn.msg.str);
#endif
				freeLocalVars(pScriptRun->pVarStack);		// Clear any local variables that were created.

				// Clean up.
				--pBuf->pCallHdr->execCount;
				if(pScriptRun->msgFlag)
					setGlobalMode(mode.cache[MdIdxRtnMsg]);
				pScriptRun = pOldRun;
				}
			}
		}

	return sess.rtn.status;
	}

#if MMDebug & (Debug_Datum | Debug_Script | Debug_CallArg | Debug_Preproc)
// Call execBuf() with debugging.
int debug_execBuf(Datum *pRtnVal, long n, Buffer *pBuf, char *runPath, uint flags, const char *caller) {

	fprintf(logfile, "*execBuf() called by %s() with args: n %ld, bufname %s, runPath '%s', flags %.8x, %s...\n",
	 caller, n, pBuf->bufname, runPath, flags, showFlags(NULL));
	(void) execBuf(pRtnVal, n, pBuf, runPath, flags);
	fprintf(logfile, "execBuf(%s) from %s() returned %s result and status %d \"%s\".\n",
	 pBuf->bufname, caller, dtype(pRtnVal, false), sess.rtn.status, sess.rtn.msg.str);
	return sess.rtn.status;
	}
#endif

// Yank a file into a buffer and execute it, given result pointer, filename, n argument, and "at startup" flag (used for error
// reporting).  Suppress any extraneous messages during read.  If there are no errors, delete the buffer on exit.
static int execFile(Datum *pRtnVal, const char *filename, long n, uint flags) {
	Buffer *pBuf;

	if(bfind(filename, BN_Create | BN_Force | BN_Derive, BFHidden,		// Create a hidden buffer...
	 &pBuf, NULL) != Success)
		return sess.rtn.status;
	ushort msgFlag = mode.cache[MdIdxRtnMsg]->flags & MdActive;		// save current message flag...
	if(msgFlag)
		clearGlobalMode(mode.cache[MdIdxRtnMsg]);			// clear it...
	(void) readIn(pBuf, filename, RWKeep | RWExist);			// read the file...
	if(msgFlag)
		setGlobalMode(mode.cache[MdIdxRtnMsg]);				// restore message flag...
	if(sess.rtn.status == Success) {
#if MMDebug & Debug_Script
		(void) debug_execBuf(pRtnVal, n, pBuf, pBuf->filename, flags, "execFile");
#else
		(void) execBuf(pRtnVal, n, pBuf, pBuf->filename, flags);	// and execute the buffer.
#endif
		}

	// If not displayed, remove the now unneeded buffer and return.
	if(sess.rtn.status >= Cancelled && pBuf->windCount == 0)
		(void) bdelete(pBuf, BC_IgnChgd);
	else {
		pBuf->flags &= ~BFHidden;
		bcount(pBuf, 0);
		}
	return sess.rtn.status;
	}

// Execute commands in a file and return result in pRtnVal.
int xeqFile(Datum *pRtnVal, long n, Datum **args) {
	char *path;

	// Get the filename.
	if(getFilename(pRtnVal, text129, NULL, ArgFirst) != Success || disnil(pRtnVal))
			// "Execute MScript"
		return sess.rtn.status;

	// Look up the path...
	if(pathSearch(pRtnVal->str, 0, (void *) &path, NULL) != Success)
		return sess.rtn.status;
	if(path == NULL)
		return rsset(Failure, 0, text52, text99, pRtnVal->str);	// Not found.
				// "No such %s \"%s\"", "file"

	// save it...
	if(dsetstr(path, pRtnVal) != 0)
		return libfail();

	// and execute it with arg n.
	return execFile(pRtnVal, pRtnVal->str, n, 0);
	}
