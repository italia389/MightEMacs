// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// exec.c	Routines dealing with execution of commands, command lines, buffers, and files for MightEMacs.

#include "std.h"
#include <stdarg.h>
#include "cmd.h"
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
	Array *pArray;			// Loop array.
	ArraySize index;		// Index of next element to process.
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

	return rsset(Failure, 0, text317, name);
			// "No such hook '%s'"
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
	pHookRec->func.u.pVoid = NULL;
	}

// Clear named hook(s), or all hooks if n <= 0.  Return status.
int clearHook(Datum *pRtnVal, int n, Datum **args) {
	HookRec *pHookRec;
	int count = 0;

	if(n <= 0 && n != INT_MIN) {

		// Clear all hooks.
		pHookRec = hookTable;
		do {
			eraseHook(pHookRec);
			++count;
			} while((++pHookRec)->name != NULL);
		}

	// Clear named hook(s).
	else {
		Datum *pHookName;
		uint argFlags = ArgFirst | ArgNotNull1;

		if(dnewtrack(&pHookName) != 0)
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
const char *hookFuncName(UnivPtr *pUniv) {

	return (pUniv->type == PtrSysFunc) ? pUniv->u.pCmdFunc->name : pUniv->u.pBuf->bufname + 1;
	}

// Set a hook.  Return status.
int setHook(Datum *pRtnVal, int n, Datum **args) {
	HookRec *pHookRec;
	UnivPtr univ;			// Pointer to the requested function.
	short minArgs, maxArgs;
	const char *name;
	Datum *pHookName = args[0];

	// Check if hook name is valid.
	if((sess.opFlags & OpEval) && findHook(pHookName->str, &pHookRec) != Success)
		goto Retn;

	// Get the function name (bare word).
	if(!needSym(s_comma, true))
		goto Retn;
	if(getCF(NULL, &univ, PtrSysFunc | PtrUserFunc) != Success)
		goto Retn;

	// Non-hook system function?
	if(univ.type == PtrSysFunc && !(univ.u.pCmdFunc->attrFlags & CFHook))
		return rsset(Failure, RSTermAttr, text420, text336, hookFuncName(&univ));
					// "Hook may not be set to %s '~b%s~B'", "system function"
	// Argument count mismatch?
	if(univ.type == PtrSysFunc) {
		minArgs = univ.u.pCmdFunc->minArgs;
		maxArgs = univ.u.pCmdFunc->maxArgs;
		if(minArgs != pHookRec->argCount || maxArgs != pHookRec->argCount) {
			name = univ.u.pCmdFunc->name;
			goto Mismatch;
		 	}
		}
	else {
		minArgs = univ.u.pBuf->pCallInfo->minArgs;
		maxArgs = univ.u.pBuf->pCallInfo->maxArgs;
		if(minArgs != pHookRec->argCount || maxArgs != pHookRec->argCount) {
			name = univ.u.pBuf->bufname + 1;
Mismatch:
			return rsset(Failure, RSTermAttr, text413, name, minArgs, maxArgs, pHookRec->argCount);
					// "Function '~b%s~B' argument count (%hd, %hd) does not match hook's (%hd)"
			}
	 	}

	// Set the hook if evaluating.
	if(sess.opFlags & OpEval)
		pHookRec->func = univ;
Retn:
	return sess.rtn.status;
	}

// Disable a hook that went awry.  Return status.
static int disableHook(HookRec *pHookRec) {

	// Hook execution failed -- let the user know what's up.
	pHookRec->running = false;
	if(sess.rtn.status > FatalError) {
		DFab msg;

		if(dopenwith(&msg, &sess.rtn.msg, FabAppend) != 0)
			goto LibFail;
		if(dfabempty(&msg) && dputf(&msg, 0, text176, hookFuncName(&pHookRec->func)) != 0)
					 // "Function '~b%s~B' failed"
			goto LibFail;
		if(dputf(&msg, 0, text161, pHookRec->name) != 0 || dclose(&msg, FabStr) != 0)
				// " (disabled '%s' hook)"
			goto LibFail;
#if MMDebug & Debug_MsgLine
		fprintf(logfile, "disableHook(): set new return message '%s'\n", sess.rtn.msg.str);
#endif
		}

	// Disable the hook and return (error) status.
	eraseHook(pHookRec);
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Execute a function bound to a hook with pRtnVal (if not NULL) and n.  If argInfo == 0, invoke the function directly;
// otherwise, build a command line with arguments.  In the latter case, argInfo contains the argument count and types of each
// argument (string, long, or Datum *).  The lowest four bits of argInfo comprise the argument count.  Each pair of higher bits
// is 0 for a string argument, 1 for a long (integer) argument, and 2 for a Datum pointer argument.
int execHook(Datum *pRtnVal, int n, HookRec *pHookRec, uint argInfo, ...) {

	// Nothing to do if hook not set or function already running.
	if(pHookRec->func.type == PtrNull || pHookRec->running) {
		if(pRtnVal != NULL)
			dsetnil(pRtnVal);
		return sess.rtn.status;
		}
	if(pRtnVal == NULL && dnewtrack(&pRtnVal) != 0)		// For throw-away return value.
		goto LibFail;

	// Build command line if requested.
	if(argInfo != 0) {
		int rtnCode;
		va_list varArgList;
		DFab cmd;
		char *str;
		ushort oldOpFlag;
		short delimChar = ' ';
		uint argCount = argInfo & 0xF;

		// Concatenate function name with arguments...
		if(dopentrack(&cmd) != 0 || (n != INT_MIN && dputf(&cmd, 0, "%d => ", n) != 0) ||
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
					else if(dputs(str, &cmd, DCvtLang) != 0)
						goto LibFail;
					break;
				case 1:			// long argument.
					if(dputf(&cmd, 0, "%ld", va_arg(varArgList, long)) != 0)
						goto LibFail;
					break;
				default:		// Datum *
					if((rtnCode = dputd(va_arg(varArgList, Datum *), &cmd, NULL, DCvtLang)) < 0)
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
		oldOpFlag = sess.opFlags & OpUserCmd;
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
		sess.opFlags |= oldOpFlag;
		}
	else {
		short oldStatus;
		ushort oldFlags;
		ushort oldNoLoad = sess.opFlags & OpNoLoad;

		// Save and restore status if running "exit" hook so that execBuf() or execCmdFunc() will run.
		if(pHookRec == hookTable + HkExit)
			rspush(&oldStatus, &oldFlags);
		sess.opFlags |= OpNoLoad;
		pHookRec->running = true;
		if(pHookRec->func.type == PtrUserFunc)
			(void) execBuf(pRtnVal, n, pHookRec->func.u.pBuf, NULL, ArgFirst);
		else
			(void) execCmdFunc(pRtnVal, n, pHookRec->func.u.pCmdFunc, 0, 0);
		sess.opFlags = (sess.opFlags & ~OpNoLoad) | oldNoLoad;
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
int run(Datum *pRtnVal, int n, Datum **args) {
	UnivPtr univ;		// Pointer to command or alias to execute.
	ushort oldScript;
	ushort oldMsgFlag = modeInfo.cache[MdIdxRtnMsg]->flags & MdEnabled;

	// If we are in script mode, force the command interactively.
	if((oldScript = sess.opFlags & OpScript)) {

		// Grab next symbol...
		if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
			return sess.rtn.status;

		// look it up...
		if(sess.opFlags & OpEval) {
			if(!execFind(pLastParse->tok.str, OpQuery, PtrSysCmd | PtrAliasSysCmd | PtrUserCmd | PtrAliasUserCmd,
			 &univ))
				return rsset(Failure, 0, text244, pLastParse->tok.str);
					// "No such command or alias '%s'"
			}

		// and get next symbol.
		if(getSym() < NotFound)
			return sess.rtn.status;

		// If not evaluating, bail out here.
		if(!(sess.opFlags & OpEval))
			return sess.rtn.status;

		// Otherwise, prepare to execute the command -- interactively.
		sess.opFlags &= ~OpScript;
		setGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
		}

	// Prompt the user to type a named command or alias.
	else {
		// Interactive mode.
		if(getCFA(": ", PtrSysCmd | PtrAliasSysCmd | PtrUserCmd | PtrAliasUserCmd, &univ, text244) != Success ||
								// "No such command or alias '%s'"
		 univ.type == PtrNull)
			return sess.rtn.status;
		}

	// Execute it.
	if(univ.type & PtrAlias)
		univ = univ.u.pAlias->targ;
	if(univ.type == PtrUserCmd) {
		sess.opFlags |= OpUserCmd;
		(void) execBuf(pRtnVal, n, univ.u.pBuf, NULL,
		 sess.opFlags & OpParens ? ArgFirst | SRun_Parens : ArgFirst);		// Call the user command.
		sess.opFlags &= ~OpUserCmd;
		}
	else {
		CmdFunc *pCmdFunc = univ.u.pCmdFunc;
		if(n != 0 || !(pCmdFunc->attrFlags & CFNCount))
			if(allowEdit(pCmdFunc->attrFlags & CFEdit) == Success)
				(void) execCmdFunc(pRtnVal, n, pCmdFunc, 0, 0);		// Call the system command.
		}
	if(sess.rtn.status == Success && oldScript)
		(void) rssave();

	// Clean up and return result.
	if(oldScript && !oldMsgFlag)
		clearGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
	sess.opFlags = (sess.opFlags & ~OpScript) | oldScript;
	return sess.rtn.status;
	}

// Concatenate all arguments and execute string result.  Return status.
int eval(Datum *pRtnVal, int n, Datum **args) {
	Datum *pDatum;

	// Get the command line.
	if(dnewtrack(&pDatum) != 0)
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
// Dump user command/function argument list to log file.
static int argDump(int n, Buffer *pBuf, Datum *pDatum) {
	Datum argList;

	fprintf(logfile, "-----\nargDump(): n: %d, pBuf: '%s'\n", n, pBuf->bufname);
	dinit(&argList);
	if(dtos(&argList, pDatum, NULL, DCvtLang) < 0)
		return libfail();
	fprintf(logfile, "User command/function argument list: %s\n", argList.str);
	dclear(&argList);
	return sess.rtn.status;
	}
#endif

// Load user command/function arguments into an array and save in "pArgV".  If in script mode, evaluate some or all of remaining
// arguments (if any) on command line, dependent on number specified in Buffer record; otherwise, create empty list.  "flags"
// contains script flags and argument flags.  Return status.
#if MMDebug & Debug_CallArg
static int argLoad(int n, Buffer *pBuf, Datum *pArgV, uint flags) {
#else
static int argLoad(Buffer *pBuf, Datum *pArgV, uint flags) {
#endif
	Array *pArray;

#if MMDebug & Debug_CallArg
	fprintf(logfile, "*argLoad(%d,%s,%.8x)...\n", n, pBuf->bufname, flags);
#endif
	// Create array for argument list (which may remain empty).
	if(makeArray(pArgV, 0, &pArray) != Success)
		return sess.rtn.status;

	// Don't get arguments (that don't exist) if executing a hook; that is, OpNoLoad is set.
	if((sess.opFlags & (OpScript | OpNoLoad)) == OpScript) {
		Datum *pArg;
		int argCount, minArgs, maxArgs;
		uint argFlags = (flags & ArgFirst) | ArgBool1 | ArgArray1 | ArgNIS1;

		if(pBuf->pCallInfo == NULL) {
			minArgs = 0;
			maxArgs = SHRT_MAX;
			}
		else {
			minArgs = pBuf->pCallInfo->minArgs;
			maxArgs = (pBuf->pCallInfo->maxArgs < 0) ? SHRT_MAX : pBuf->pCallInfo->maxArgs;
			}

		// Get arguments until none left.
		if(dnewtrack(&pArg) != 0)
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
				argFlags = ArgBool1 | ArgArray1 | ArgNIS1;

				// Got an argument... add it to array and count it.
				if(apush(pArray, pArg, AOpCopy) != 0)
					goto LibFail;
				++argCount;
				}

			// Too few or too many arguments found?
			if(argCount < minArgs || argCount > maxArgs)
				return rsset(Failure, 0, text69, pLastParse->tok.str);
					// "Wrong number of arguments (at token '%s')"
			}
		}
	else if((sess.opFlags & OpNoLoad) && pBuf->pCallInfo != NULL && pBuf->pCallInfo->minArgs > 0)
		return rsset(Failure, 0, text69, "");
			// "Wrong number of arguments (at token '%s')"
	sess.opFlags &= ~OpNoLoad;
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
	pLastParse->garbHead = datGarbHead;
#if MMDebug & Debug_Datum
	fprintf(logfile, "parseBegin(): saved datum garbage pointer %.8x\n", (uint) datGarbHead);
#endif
	// Save old "script mode" flag and enable it.
	pLastParse->flags = (sess.opFlags & OpScript) | flags;
	sess.opFlags = (sess.opFlags & ~OpParens) | OpScript;

	// Get first symbol and return.
	(void) getSym();		// Ignore any "NotFound" return.
	return sess.rtn.status;
	}

// End a parsing instance and restore previous one.
static void parseEnd(Parse *pOldParse) {

#if MMDebug & Debug_Datum
	fprintf(logfile, "parseEnd(): old instance %.8x: calling dgPop()...\n", (uint) pLastParse);
#endif
	dgPop(pLastParse->garbHead);
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
		nodeInit(&node, pRtnVal, true);
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
			if((pLastParse->flags & ParseExprEnd) && *pLastParse->src != TokC_ExprEnd)
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

// Parse and execute a string as an expression statement, given result pointer, string pointer, terminator character, and
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
//		Create a new object, set its contents to *pUniv, and add it to the linked list and execution table.
//	If op is OpDelete:
//		Return an error.
int editAlias(const char *name, ushort op, UnivPtr *pUniv) {
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
		UnivPtr univ;

		// Valid identifier name?
		str = (char *) name;
		if(((sym = getIdent(&str, NULL)) != s_ident && sym != s_identQuery) || *str != '\0')
			(void) rsset(Failure, 0, text447, text286, name);
				// "Invalid %s '%s'", "identifier"

		// Allocate the needed memory.
		if((pAlias2 = (Alias *) malloc(sizeof(Alias) + strlen(name))) == NULL)
			return rsset(Panic, 0, text94, "afind");
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
		pAlias2->targ = *pUniv;
		pAlias2->type = pUniv->type == PtrSysCmd ? PtrAliasSysCmd : pUniv->type == PtrSysFunc ? PtrAliasSysFunc :
		 pUniv->type == PtrUserCmd ? PtrAliasUserCmd : PtrAliasUserFunc;

		// Add its name to the execTable hash.
		return execFind((univ.u.pAlias = pAlias2)->name, OpCreate, univ.type = pAlias2->type, &univ);
		}

	// Delete request but alias not found.
	return rsset(Failure, 0, text271, name);
			// "No such alias '%s'"
	}

// Create an alias to a command or function.  Replace any existing alias if n > 0.
int alias(Datum *pRtnVal, int n, Datum **args) {
	UnivPtr univ;
	Datum *pName;

	// Get the alias name.
	if(dnewtrack(&pName) != 0)
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

	// Existing command, function, alias, or user variable of same name?  Exclude aliases from search if n > 0 (a force).
	if((sess.opFlags & OpEval) && (execFind(pName->str, OpQuery, n > 0 ? PtrAny & ~PtrAlias : PtrAny, NULL) ||
	 findUserVar(pName->str) != NULL))
		return rsset(Failure, RSTermAttr, text165, pName->str);
			// "Name '~b%s~B' already in use"

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
		(void) getCF(NULL, &univ, PtrSysCmdFunc | PtrUserCmdFunc);
		}
	else {
		char workBuf[strlen(text215) + strlen(pName->str) + strlen(text325) + strlen(text313) + 8];

		// Get the command or function name.
		sprintf(workBuf, "%s'~b%s~B' %s %s", text215, pName->str, text325, text313);
						// "Create alias "	"of", "command or function"
		(void) getCF(workBuf, &univ, PtrSysCmdFunc | PtrUserCmdFunc);
		}

	if(sess.rtn.status == Success && univ.type != PtrNull) {

		// Delete any existing alias.
		if(n > 0 && execFind(pName->str, OpQuery, PtrAlias, NULL))
			(void) editAlias(pName->str, OpDelete, NULL);

		// Create the alias.
		if(editAlias(pName->str, OpCreate, &univ) != Success)
			return sess.rtn.status;

		// Increment alias use count on user command or function.
		if(univ.type & PtrUserCmdFunc)
			++univ.u.pBuf->aliasCount;
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Delete one or more user commands, user functions, or aliases.  Set pRtnVal to count of items deleted or zero if error.
// Return status.
static int delCFA(Datum *pRtnVal, uint selector, const char *prompt, const char *type, const char *errorMsg) {
	UnivPtr univ;
	uint count = 0;
	bool delError = false;

	if(sess.opFlags & OpEval)
		dsetint(0L, pRtnVal);

	// If interactive, get alias, command, or function name from user.
	if(!(sess.opFlags & OpScript)) {
		if(getCFA(prompt, selector, &univ, errorMsg) != Success || univ.type == PtrNull)
			return sess.rtn.status;
		goto NukeIt;
		}

	// Script mode: get name(s) to delete.
	do {
		if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
			return sess.rtn.status;
		if(sess.opFlags & OpEval) {
			if(!execFind(pLastParse->tok.str, OpQuery, selector, &univ))
				(void) rsset(Success, RSNoWrap, errorMsg, pLastParse->tok.str);
			else {
NukeIt:
				// Nuke it.
				if(selector & PtrUserCmdFunc) {
					if(bdelete(univ.u.pBuf, BC_IgnChgd) != Success)
						goto Fail;
					}
				else if(editAlias(univ.u.pAlias->name, OpDelete, NULL) != Success) {
Fail:
					// Don't cause a running script to fail on Failure status.
					if(sess.rtn.status < Failure)
						return sess.rtn.status;
					delError = true;
					(void) rsunfail();
					continue;
					}
				++count;
				}
			}
		} while((sess.opFlags & OpScript) && getSym() == Success && needSym(s_comma, false));

	// Return count if evaluating and no error.
	if((sess.opFlags & OpEval) && !delError)
		dsetint((long) count, pRtnVal);
	if(count == 1)
		(void) rsset(Success, RSHigh, "%s %s", type, text10);
						// "deleted"
	return sess.rtn.status;
	}

// Delete one or more alias(es).  Return status.
int delAlias(Datum *pRtnVal, int n, Datum **args) {

	return delCFA(pRtnVal, PtrAlias, text269, text411, text271);
			// "Delete alias", "Alias", "No such alias '%s'"
	}

// Delete one or more user routines (commands and/or functions).  Return status.
int delRoutine(Datum *pRtnVal, int n, Datum **args) {

	return delCFA(pRtnVal, PtrUserCmdFunc, text216, text449, text440);
			// "Delete user routine", "User routine", "No such user routine '%s'"
	}

// Execute the contents of a buffer (of commands) and return result in pRtnVal.
int xeqBuf(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;		// Pointer to buffer to execute.

	// Find out what buffer the user wants to execute...
	if(getBufname(pRtnVal, text117, sess.cur.pBuf->bufname, ArgFirst | OpDelete, &pBuf, NULL) != Success || pBuf == NULL)
			// "Execute"
		return sess.rtn.status;

	// and execute it with arg n.
#if MMDebug & Debug_Script
	return debug_execBuf(pRtnVal, n, pBuf, pBuf->filename, sess.opFlags & OpParens ? SRun_Parens : 0, "xeqBuf");
#else
	return execBuf(pRtnVal, n, pBuf, pBuf->filename, sess.opFlags & OpParens ? SRun_Parens : 0);
#endif
	}

// Skip white space in a fixed-length string, given indirect source pointer and length.  Update pointer to point to the first
// non-white character (or end of string if none) and return number of bytes remaining.
static int skipWhite(char **pSrc, int len) {
	char *src = *pSrc;

	while(len > 0 && (*src == ' ' || *src == '\t')) {
		++src;
		--len;
		}
	*pSrc = src;
	return len;
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

// Build an error message for a user command or function execution which includes the buffer name and line currently being
// executed, and set via rsset().  If errorMsg0 is NULL, use sess.rtn.msg as the initial portion; if sess.rtn.msg is null, use a
// generic error message.  Return status.
static int userExecError(const char *errorMsg0, const char *errorMsg1, Buffer *pBuf, Line *pLine, uint flags) {
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
		if(escAttr(&msg) != Success)
			return sess.rtn.status;
		}
	else if((errorMsg1 == NULL ? dputs(errorMsg0 != NULL ? errorMsg0 : text219, &msg, 0) :
								// "Script failed"
	 dputf(&msg, 0, errorMsg0, errorMsg1)) != 0)
		goto LibFail;
	if(dputf(&msg, 0, "%s %s %c%s%c %s %ld", text229, label, delimChar, name, delimChar, text230,
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
	CallInfo *pCallInfo = pBuf->pCallInfo;
	if(pCallInfo != NULL) {
		if(pCallInfo->execBlocks != NULL) {
			lbfree(pCallInfo->execBlocks);
			pCallInfo->execBlocks = NULL;
			}
		dclear(&pCallInfo->argSyntax);
		dclear(&pCallInfo->descrip);
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

// Preprocess a buffer and return status.  If no errors found, set pBuf->pCallInfo->execBlocks to result.  pBuf->pCallInfo is
// assumed to not be NULL.
static int preprocBuf(Buffer *pBuf, uint flags) {
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
						return rsset(Panic, 0, text94, "ppbuf");
							// "%s(): Out of memory!"

					// Record line and block type.
					pTempBlock->pMarkLine = pLine;
					pTempBlock->pBreakLine = pTempBlock->pJumpLine = NULL;
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
						pOpenBlock->pJumpLine = pLine;
						if(pOpenBlock->type & SLoopType) {
							pTempBlock = pOpenBlock->next;

							// For loop records, record the marker line (temporarily) of the
							// *parent* loop record in this record's pBreakLine member so that it
							// can be set later to the parent's ENDLOOP line (which is needed for
							// multi-level breaks).  If there is no parent loop block, the
							// pBreakLine member will remain NULL, which is okay.
							while(pTempBlock != NULL) {
								if(pTempBlock->type & SLoopType) {

									// Found parent loop block.  Save marker line pointer.
									pOpenBlock->pBreakLine = pTempBlock->pMarkLine;
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
		(void) userExecError(errorMsg1, errorMsg2, pBuf, pLine, flags);
		goto Exit;
		}

	// Everything looks good.  Last step is to fix up the loop records.
	for(pTempBlock = pExecBlock; pTempBlock != NULL; pTempBlock = pTempBlock->next) {
		if((pTempBlock->type & SLoopType) && pTempBlock->pBreakLine != NULL) {
			LoopBlock *pTempBlock2 = pExecBlock;

			// Find loop block ahead of this block that contains this block's marker.
			do {
				if(pTempBlock2->pMarkLine == pTempBlock->pBreakLine) {

					// Found parent block.  Set this loop's pBreakLine to parent's
					// ENDLOOP line (instead of the top marker line).
					pTempBlock->pBreakLine = pTempBlock2->pJumpLine;
					goto NextFix;
					}
				} while((pTempBlock2 = pTempBlock2->next) != NULL &&
				 pTempBlock2->pMarkLine != pTempBlock->pMarkLine);

			// Huh?  Matching loop block not found! (This is a bug.)
			(void) rsset(Failure, 0, text220, getLineNum(pBuf, pTempBlock->pMarkLine));
				// "Parent block of loop block at line %ld not found during buffer scan"
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
		lbdump(pTempBlock->pMarkLine, " Mark");
		lbdump(pTempBlock->pJumpLine, " Jump");
		lbdump(pTempBlock->pBreakLine, "Break");
		}
//	return rsset(FatalError, RSNoFormat, "preprocBuf() exit");
#endif
	// Success!  Save block pointer in buffer-extension record and set "preprocessed" flag.
	pBuf->pCallInfo->execBlocks = pExecBlock;
	pBuf->flags |= BFPreproc;
	goto Exit;

	// Clean up and exit per sess.rtn.status.
RCExit:
	lbfree(pOpenBlock);
	(void) userExecError(NULL, NULL, pBuf, pLine, flags);
Exit:
	if(sess.rtn.status == Success)
		return sess.rtn.status;
	lbfree(pExecBlock);
	return sess.rtn.status;
	}

// Free execution level storage.
static void lfree(ExecLevel *pLevel) {
	ExecLevel *pLevel1;

	do {
		pLevel1 = pLevel->next;
		if(pLevel->pForHdr != NULL)
			free((void *) pLevel->pForHdr);
		free((void *) pLevel);
		} while((pLevel = pLevel1) != NULL);
	}

// Move forward to a new execution level, creating a level object if needed.
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
static int prevLevel(ExecLevel **ppLevel) {
	ExecLevel *pLevel = *ppLevel;

	for(;;) {
#if SanityCheck
		if(pLevel->prev == NULL)

			// Huh?  Loop level not found! (This is a bug.)
			return rsset(Failure, RSNoFormat, "Prior loop execution level not found while rewinding stack");
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
	UnivPtr univ;
	CallInfo *pCallInfo;
	bool whitespace, exists;
	Datum *pDatum1, *pDatum2;
	char *pCmdFuncName;
	int cmdFuncLen = strlen(text247);
		// "function"
	ushort haveText1 = 0, haveText2 = 0;
	short minArgs = 0, maxArgs = -1;

	if(dnewtrack(&pDatum1) != 0 || dnewtrack(&pDatum2) != 0)
		goto LibFail;

	// Get command or function name.
	if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
		return sess.rtn.status;
	if(strlen(pLastParse->tok.str) > MaxBufname - 1)
		return rsset(Failure, 0, text232, pLastParse->tok.str, MaxBufname - 1);
			// "Command or function name '%s' cannot exceed %d characters"

	// Construct the buffer header (and name) in pRtnVal temporarily.
	if(dsalloc(pRtnVal, strlen(pLastParse->tok.str) + cmdFuncLen + 4) != 0)
		goto LibFail;
	pRtnVal->str[0] = TokC_Comment;
	pRtnVal->str[1] = ' ';
	pCmdFuncName = stpcpy(pRtnVal->str + 2, bufFlags == BFFunc ? text247 : text158);
								// "function", "command"
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
				if((minArgs = pDatum1->u.intNum) < 0)
					goto Err;
				if(!haveSym(s_comma, false))
					maxArgs = minArgs;
				else {
					if(getSym() < NotFound || (!haveSym(s_rightParen, false) &&
					 funcArg(pDatum2, ArgFirst | ArgInt1) != Success))
						return sess.rtn.status;
					if(!disnil(pDatum2))
						if((maxArgs = pDatum2->u.intNum) < 0) {
							minArgs = maxArgs;
Err:
							return rsset(Failure, 0, text39, text206, (int) minArgs, 0);
								// "%s (%d) must be %d or greater"
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

	// Existing command, function, alias, or user variable of same name as this user command or function?
	if(((exists = execFind(pCmdFuncName, OpQuery, PtrAny, &univ)) && (!(univ.type & PtrUserCmdFunc) ||
	 !(modeInfo.cache[MdIdxClob]->flags & MdEnabled))) || findUserVar(pCmdFuncName) != NULL)
		return rsset(Failure, RSTermAttr, text165, pCmdFuncName);
			// "Name '~b%s~B' already in use"

	// Create a hidden buffer, make sure it's empty, and has the correct pointer type in execTable.
	if(bfind(pCmdFuncName - 1, BS_Create | BS_Extend, bufFlags | BFHidden | BFReadOnly, ppCallBuf, NULL) != Success)
		return sess.rtn.status;
	if(exists) {
		if(bclear(*ppCallBuf, BC_IgnChgd | BC_Unnarrow | BC_ClrFilename) != Success)
			return sess.rtn.status;
		univ.type = (bufFlags == BFCommand) ? PtrUserCmd : PtrUserFunc;
		(void) execFind(pCmdFuncName, OpUpdate, 0, &univ);
		(*ppCallBuf)->flags = ((*ppCallBuf)->flags & ~BFCmdFunc) | bufFlags;
		}

	// Set the buffer extension parameters.
	pCallInfo = (*ppCallBuf)->pCallInfo;
	pCallInfo->minArgs = minArgs;
	pCallInfo->maxArgs = maxArgs;
	if(haveText1)
		dxfer(haveText1 == 1 ? &pCallInfo->argSyntax : &pCallInfo->descrip, pDatum1);
	if(haveText2)
		dxfer(haveText2 == 1 ? &pCallInfo->argSyntax : &pCallInfo->descrip, pDatum2);
	pCmdFuncName[-1] = ' ';

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Parse a "for" line and initialize control variables.  Return status.
static int initFor(ForLoopHdr **ppForHdr, Datum *pDatum) {
	ExprNode node;
	ForLoopHdr *pForHdr;

	if((pForHdr = *ppForHdr) == NULL) {
		if((pForHdr = (ForLoopHdr *) malloc(sizeof(ForLoopHdr))) == NULL)
			return rsset(Panic, 0, text94, "initFor");
				// "%s(): Out of memory!"
		*ppForHdr = pForHdr;
		}

	// Get the variable name and "in" keyword.
	if(!haveSym(s_any, true) || findVar(pLastParse->tok.str, &pForHdr->varDesc, OpCreate) != Success ||
	 getSym() < NotFound || !needSym(kw_in, true))
		return sess.rtn.status;

	// Get (array) expression.
	nodeInit(&node, pDatum, false);
	if(ge_assign(&node) != Success || extraSym() || !isArrayVal(node.pValue))
		return sess.rtn.status;

	// Finish up.
	pForHdr->pArray = pDatum->u.pArray;
	pForHdr->index = 0;
	return sess.rtn.status;
	}

// Get next array element and set "for" loop control variable to it if found; otherwise, return NotFound.  Return status.
static int nextForEl(ForLoopHdr *pForHdr) {
	Datum *pDatum;

	if(pForHdr->index == pForHdr->pArray->used) {
		pForHdr->index = -1;	// Invalidate control object.
		return NotFound;
		}
	if((pDatum = aget(pForHdr->pArray, pForHdr->index++, 0)) == NULL)
		return libfail();
	return setVar(pDatum, &pForHdr->varDesc);
	}

// Execute a compiled buffer, save result in pRtnVal, and return status.
static int xbuf(Datum *pRtnVal, Buffer *pBuf, uint flags) {
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
	ExecLevel *pLevel;		// Current level.
	LoopBlock *pBlock;
	Buffer *pCallBuf = NULL;	// "cmd" or "func" buffer pointer.
	Datum *pDatum;			// Work Datum object.
	Datum *pFullLine;		// Null-terminated buffer line (Datum object).
	char *lineText;			// Work pointer.
	char *exprText;			// Beginning of expression statement (past any statement keyword).
	const char *errorMsg1, *errorMsg2;
	Parse *pOldLast, newLast;
	ExprNode node;

	// Prepare for execution.
	if(dnewtrack(&pDatum) != 0 || dnewtrack(&pFullLine) != 0)
		return libfail();
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
	static uint instance = 0;
	uint myInstance = ++instance;
	fprintf(logfile, "*** BEGIN xbuf(%u,%s): %s...\n", myInstance, pBuf->bufname, showFlags(&newLast));
#endif
	pBeginLine = pEndLine = pBuf->pFirstLine;
	do {
		// Skip blank lines and comments unless last line was a continuation line or "salting" a user routine.
		lineText = pEndLine->text;
		len = pEndLine->used;
#if MMDebug & (Debug_Script | Debug_Preproc)
		fprintf(logfile, "%.8x [level %u.%u] >> %.*s\n", (uint) pEndLine, myInstance, pLevel->level, len, lineText);
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
						if(pLevel->live)
							pCallBuf = NULL;
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
						goto RCExit;
					pLevel->loopSpawn = false;

					// Prior level true?
					if(pLevel->prev->live) {
EvalIf:
						// Yes, grab the value of the logical expression.
						nodeInit(&node, pDatum, false);
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
						goto JumpDown;

					// Error if extra arguments; otherwise, begin next loop.
					if(extraSym())
						goto RCExit;
					goto NextLoop;
				case kw_for:	// "for" keyword

					// Current level true?
					if(pLevel->live) {
						int status;

						// Yes, parse line and initialize loop controls if first time.
						if((pLevel->pForHdr == NULL || pLevel->pForHdr->index < 0) &&
						 initFor(&pLevel->pForHdr, pDatum) != Success)
							goto RCExit;

						// Set control variable to next array element and begin next loop, or break if
						// none left.
						if((status = nextForEl(pLevel->pForHdr)) == Success)
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
					if(pLevel->live) {

						// Yes, get the "while" or "until" logical expression.
						nodeInit(&node, pDatum, false);
						if(ge_andOr(&node) != Success || extraSym())
							goto RCExit;

						// If the "while" condition is true or the "until" condition if false, begin a
						// new level, set it to true, and continue.
						if(toBool(pDatum) == go) {
NextLoop:
							if(nextLevel(&pLevel) != Success)
								goto RCExit;
							pLevel->live = true;
							pLevel->loopSpawn = true;
							goto NextLn;
							}
						}

					// Current level or condition is false: skip this block.
					goto JumpDown;

				case kw_break:	// "break" keyword.
				case kw_next:	// "next" keyword.

					// Ignore if current level is false (nothing to jump out of).
					if(pLevel->live == false)
						goto NextLn;

					// Check "break" argument.
					if(keywordId == kw_break) {
						if(haveSym(s_any, false)) {
							nodeInit(&node, pDatum, false);
							if(ge_assign(&node) != Success || extraSym() || !isIntVal(pDatum))
								goto RCExit;
							if(pDatum->u.intNum <= 0) {
								(void) rsset(Failure, 0, text217, pDatum->u.intNum);
									// "'break' level '%ld' must be 1 or greater"
								goto RCExit;
								}
							breakLevel = pDatum->u.intNum;
							}
						else
							breakLevel = 1;

						// Invalidate any "for" loop in progress.
						if(pLevel->prev->pForHdr != NULL)
							pLevel->prev->pForHdr->index = -1;
						}

					// Extraneous "next" argument(s)?
					else if(extraSym())
						goto RCExit;
JumpDown:
					// Continue or break out of loop: find the right block (kw_while, kw_until, kw_loop,
					// kw_for, kw_break, or kw_next) and jump down to its "endloop".
					for(pBlock = pBuf->pCallInfo->execBlocks; pBlock != NULL; pBlock = pBlock->next)
						if(pBlock->pMarkLine == pBeginLine) {

							// If this is a "break" or "next", set the line pointer to the "endloop"
							// line so that the "endloop" is executed; otherwise, set it to the line
							// after the "endloop" line so that the "endloop" is bypassed.
							pEndLine = (keywordId & SBreakType) ? pBlock->pJumpLine :
							 pBlock->pJumpLine->next;

							// Return to the most recent loop level (bypassing "if" levels) if this
							// is a "break" or "next"; otherwise, just reset the loop counter.
							if(!(keywordId & SBreakType))
								pLevel->loopCount = 0;
							else if(prevLevel(&pLevel) != Success)
								goto RCExit;

							goto Onward;
							}

					// Huh?  "endloop" line not found! (This is a bug.)
					goto LoopBugExit;

				case kw_endloop:	// "endloop" keyword.

					// Extra arguments?
					if(extraSym())
						goto RCExit;

					// This line is executed only when its partner is a "loop", "for", a "while" that's
					// true, or an "until" that's false, or jumped to from a "break" or "next" (otherwise,
					// it's bypassed).
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
								// "Maximum number of loop iterations (%d) exceeded!"
							goto RCExit;
							}
						}

					// We're good... just find the "loop", "for", "while" or "until", and go back to it, or
					// to a prior level if we're processing a "break".
					for(pBlock = pBuf->pCallInfo->execBlocks; pBlock != NULL; pBlock = pBlock->next)
						if((pBlock->type & SLoopType) && pBlock->pJumpLine == pBeginLine) {

							// while/until/loop/for block found: set the line pointer to the line
							// after its "endloop" line or its parent's "endloop" line if we're
							// processing a "break" (and return to the previous execution level);
							// otherwise, back to the beginning.
							if(breakLevel > 0) {
								if(--breakLevel > 0) {
									if(pBlock->pBreakLine == NULL) {
										(void) rsset(Failure, 0, text225, breakLevel);
									// "Too many break levels (%d short) from inner 'break'"
										goto RCExit;
										}
									pEndLine = pBlock->pBreakLine;

									// Return to the most recent loop level before this one.
									pLevel = pLevel->prev;
									if(prevLevel(&pLevel) != Success)
										goto RCExit;
									}
								else {
									pEndLine = pEndLine->next;
									pLevel = pLevel->prev;
									}
								pLevel->loopCount = 0;	// Reset the loop counter.
								}
							else
								pEndLine = pBlock->pMarkLine;
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
							nodeInit(&node, pRtnVal, false);
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
				default:
#if SanityCheck
					// Huh?  Unknown keyword! (This is a bug.)
					(void) rsset(Failure, 0, "xbuf(): Error: Unknown keyword '%.*s'",
					 (int) (exprText - lineText), lineText);
					goto RCExit;
#else
					;
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
			if(sess.rtn.status != Success) {
				EWindow *pWind = sess.windHead;

				// Check if buffer is on-screen.
				do {
					if(pWind->pBuf == pBuf) {

						// Found a window.  Set point to error line.
						pWind->face.point.pLine = pBeginLine;
						pWind->face.point.offset = 0;
						pWind->flags |= WFMove;
						}
					} while((pWind = pWind->next) != NULL);

				// In any case, set the buffer point.
				pBuf->face.point.pLine = pBeginLine;
				pBuf->face.point.offset = 0;

				// Build a more detailed message that includes the command error message, if any.
				(void) userExecError(NULL, NULL, pBuf, pBeginLine, flags);
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
	(void) userExecError(NULL, NULL, pBuf, pBeginLine, flags);
Exit:
#if MMDebug & (Debug_Script | Debug_Preproc)
	fprintf(logfile, "*** END xbuf(%u,%s), sess.rtn.msg \"%s\"...\n", myInstance, pBuf->bufname, sess.rtn.msg.str);
	--myInstance;
#endif
	lfree(levelTable);
	if(newLast.src != NULL)
		parseEnd(pOldLast);

	return (sess.rtn.status == Success) ? sess.rtn.status : rsset(ScriptError, 0, NULL);
	}

// Execute the contents of a buffer, given Datum object for return value, numeric argument, buffer pointer, pathname of file
// being executed (or NULL), and invocation flags.
int execBuf(Datum *pRtnVal, int n, Buffer *pBuf, char *runPath, uint flags) {

	// Check for narrowed buffer and runaway recursion.
	if(pBuf->flags & BFNarrowed)
		(void) rsset(Failure, RSNoFormat, text72);
			// "Cannot execute a narrowed buffer"
	else if(maxCallDepth > 0 && pBuf->pCallInfo != NULL && pBuf->pCallInfo->execCount >= maxCallDepth)
		(void) rsset(Failure, 0, text319, text313, maxCallDepth);
			// "Maximum %s recursion depth (%d) exceeded", "command or function"
	else {
		Datum args;

		// Create buffer-extension record if needed.
		if(pBuf->pCallInfo == NULL && bextend(pBuf) != Success)
			return sess.rtn.status;

		// Get call arguments.
		dinit(&args);		// No need to dclear() this later because it will always contain an array.
#if MMDebug & Debug_CallArg
		if(argLoad(n, pBuf, &args, flags) == Success) {
#else
		if(argLoad(pBuf, &args, flags) == Success) {
#endif
			// If evaluating, preprocess buffer if needed.
			if((pBuf->flags & BFPreproc) || preprocBuf(pBuf, flags) == Success) {
				ScriptRun *pOldRun, newRun;

				// Make new run instance and prepare for execution.
				pOldRun = pScriptRun;
				pScriptRun = &newRun;
				if((pScriptRun->msgFlag = modeInfo.cache[MdIdxRtnMsg]->flags & MdEnabled))
					clearGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
				pScriptRun->pArgs = &args;
				pScriptRun->path = fixNull(runPath);
				pScriptRun->pBuf = pBuf;
				if(dnew(&pScriptRun->pNArg) != 0)
					return libfail();		// Fatal error.
				dsetint(n, pScriptRun->pNArg);
				pScriptRun->pVarStack = localVarRoot;

				// Flag that we are executing the buffer and execute it.
				++pBuf->pCallInfo->execCount;
#if MMDebug & Debug_Script
				fprintf(logfile, "execBuf(): Calling xbuf(%s): %s...\n", pBuf->bufname, showFlags(NULL));
#endif
				(void) xbuf(pRtnVal, pBuf, flags);		// Execute the buffer.
#if MMDebug & Debug_Script
				fprintf(logfile, "execBuf(): xbuf() returned %s result and status %d \"%s\".\n",
				 dtype(pRtnVal, false), sess.rtn.status, sess.rtn.msg.str);
#endif
				(void) freeUserVars(pScriptRun->pVarStack);	// Clear any local vars that were created.

				// Clean up.
				--pBuf->pCallInfo->execCount;
				dfree(pScriptRun->pNArg);
				if(pScriptRun->msgFlag)
					setGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
				pScriptRun = pOldRun;
				}
			}
		}

	return sess.rtn.status;
	}

#if MMDebug & (Debug_Datum | Debug_Script | Debug_CallArg | Debug_Preproc)
// Call execBuf() with debugging.
int debug_execBuf(Datum *pRtnVal, int n, Buffer *pBuf, char *runPath, uint flags, const char *caller) {

	fprintf(logfile, "*execBuf() called by %s() with args: n %d, bufname %s, runPath '%s', flags %.8x, %s...\n",
	 caller, n, pBuf->bufname, runPath, flags, showFlags(NULL));
	(void) execBuf(pRtnVal, n, pBuf, runPath, flags);
	fprintf(logfile, "execBuf(%s) from %s() returned %s result and status %d \"%s\".\n",
	 pBuf->bufname, caller, dtype(pRtnVal, false), sess.rtn.status, sess.rtn.msg.str);
	return sess.rtn.status;
	}
#endif

// Yank a file into a buffer and execute it, given result pointer, filename, n argument, and "at startup" flag (used for error
// reporting).  Suppress any extraneous messages during read.  If there are no errors, delete the buffer on exit.
int execFile(Datum *pRtnVal, const char *filename, int n, uint flags) {
	Buffer *pBuf;

	if(bfind(filename, BS_Create | BS_Force | BS_Derive, BFHidden,		// Create a hidden buffer...
	 &pBuf, NULL) != Success)
		return sess.rtn.status;
	ushort msgFlag = modeInfo.cache[MdIdxRtnMsg]->flags & MdEnabled;	// save current message flag...
	if(msgFlag)
		clearGlobalMode(modeInfo.cache[MdIdxRtnMsg]);			// clear it...
	(void) readIn(pBuf, filename, RWKeep | RWExist);			// read the file...
	if(msgFlag)
		setGlobalMode(modeInfo.cache[MdIdxRtnMsg]);			// restore message flag...
	if(sess.rtn.status == Success) {
#if MMDebug & Debug_Script
		(void) debug_execBuf(pRtnVal, n, pBuf, pBuf->filename, flags, "execFile");
#else
		(void) execBuf(pRtnVal, n, pBuf, pBuf->filename, flags);	// and execute the buffer.
#endif
		}

	// If not displayed, remove the now unneeded buffer and return.
	pBuf->flags &= ~BFHidden;
	if(sess.rtn.status >= Cancelled && pBuf->windCount == 0)
		(void) bdelete(pBuf, BC_IgnChgd);
	return sess.rtn.status;
	}

// Execute commands in a file and return result in pRtnVal.
int xeqFile(Datum *pRtnVal, int n, Datum **args) {
	char *path;

	// Get the filename.
	if(!(sess.opFlags & OpScript)) {
		TermInpCtrl termInpCtrl = {NULL, RtnKey, MaxPathname, NULL};

		if(termInp(pRtnVal, text129, ArgNotNull1 | ArgNil1, Term_C_Filename, &termInpCtrl) != Success ||
				// "Execute script file"
		 disnil(pRtnVal))
			return sess.rtn.status;
		}
	else if(funcArg(pRtnVal, ArgFirst | ArgNotNull1 | ArgPath) != Success)
		return sess.rtn.status;

	// Look up the path...
	if(pathSearch(pRtnVal->str, 0, (void *) &path, NULL) != Success)
		return sess.rtn.status;
	if(path == NULL)
		return rsset(Failure, 0, text152, pRtnVal->str);	// Not found.
			// "No such file \"%s\""

	// save it...
	if(dsetstr(path, pRtnVal) != 0)
		return libfail();

	// and execute it with arg n.
	return execFile(pRtnVal, pRtnVal->str, n, 0);
	}
