// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// var.c	Routines dealing with variables for MightEMacs.

// Make selected global definitions local.
#define VarData
#include "std.h"

#include "cxl/lib.h"

#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "file.h"
#include "search.h"
#include "var.h"

// Return true if a variable is an integer type, given descriptor, otherwise false.
bool isIntVar(VarDesc *pVarDesc) {
	Datum *pDatum;

	switch(pVarDesc->type) {
		case VTyp_LocalVar:
		case VTyp_GlobalVar:
			pDatum = pVarDesc->p.pUserVar->pValue;
			break;
		case VTyp_SysVar:
			return (pVarDesc->p.pSysVar->flags & (V_Int | V_Char)) != 0;
		case VTyp_NumVar:
			{ushort argNum = pVarDesc->i.argNum;

			// Get argument value.  $0 resolves to the user command/function "n" argument.
			pDatum = (argNum == 0) ? pScriptRun->pNArg : pVarDesc->p.pArgs->u.pArray->elements[argNum - 1];
			}
			break;
		default:	// VTyp_ArrayElRef
			pDatum = aget(pVarDesc->p.pArray, pVarDesc->i.index, 0);	// Should never return NULL.

		}
	return pDatum->type == dat_int;
	}

// Return true if c is a valid first character of an identifier, otherwise false.
bool isIdent1(short c) {

	return is_letter(c) || c == '_';
	}

// Return number of variables currently in use (for building a name completion list).
uint varCount(uint ctrlFlags) {
	UserVar *pUserVar;
	uint count;

	// Get system variable name count.
	if(ctrlFlags & Term_C_MutVar) {		// Skip constants.
		SysVar *pSysVar = sysVars;
		count = 0;
		do {
			if(isLowerCase(pSysVar->name[1]))
				++count;
			} while((++pSysVar)->name != NULL);
		}
	else
		count = NumSysVars;

	// Add global variable counts.
	for(pUserVar = globalVarRoot; pUserVar != NULL; pUserVar = pUserVar->next)
		if(!(ctrlFlags & Term_C_MutVar) || isLowerCase(pUserVar->name[1]))
			++count;

	return count;
	}

// Compare two variable names (for qsort()).
static int varcmp(const void *pVar1, const void *pVar2) {

	return strcmp(*((const char **) pVar1), *((const char **) pVar2));
	}

// Created sorted list of all variables currently in use and store in varList array.
void varSort(const char *varList[], uint count, uint ctrlFlags) {
	SysVar *pSysVar;
	UserVar *pUserVar;
	const char **varList0 = varList;

	// Store system variable names.
	pSysVar = sysVars;
	do {
		if(!(ctrlFlags & Term_C_MutVar) || isLowerCase(pSysVar->name[1]))
			*varList++ = pSysVar->name;
		} while((++pSysVar)->name != NULL);

	// Store global variable names.
	for(pUserVar = globalVarRoot; pUserVar != NULL; pUserVar = pUserVar->next)
		if(!(ctrlFlags & Term_C_MutVar) || isLowerCase(pUserVar->name[1]))
			*varList++ = pUserVar->name;

	// Sort it.
	qsort((void *) varList0, count, sizeof(char *), varcmp);
	}

// Free user variable(s), given "stack" pointer.  All variables will be together at top of list (because they are created in
// stack fashion during script execution and/or recursion).
int freeUserVars(UserVar *pVarStack) {
	UserVar *pUserVar;

	while(localVarRoot != pVarStack) {
		pUserVar = localVarRoot->next;

		// Free value...
		dfree(localVarRoot->pValue);

		// free variable...
		free((void *) localVarRoot);

		// and advance head pointer.
		localVarRoot = pUserVar;
		}
	return sess.rtn.status;
	}

// Search global or local variable list for given name (with prefix).  If found, return pointer to UserVar record; otherwise,
// return NULL.  Local variable entries beyond pScriptRun->pUserVar are not considered so that all local variables created or
// referenced in a particular user command or function invocation are visible and accessible only from that command or function,
// which allows recursion to work properly.
UserVar *findUserVar(const char *var) {
	UserVar *pUserVar;
	UserVar *pVarStack;		// Search boundary in local variable table.

	if(*var == TokC_GlobalVar) {
		pUserVar = globalVarRoot;
		pVarStack = NULL;
		}
	else {
		pUserVar = localVarRoot;
		pVarStack = (pScriptRun == NULL) ? NULL : pScriptRun->pVarStack;
		}

	while(pUserVar != pVarStack) {
		if(strcmp(var, pUserVar->name) == 0)
			return pUserVar;
		pUserVar = pUserVar->next;
		}

	return NULL;
	}

// binSearch() helper function for returning system variable name, given table (array) and index.
static const char *sysVarName(const void *table, ssize_t i) {

	return ((SysVar *) table)[i].name + 1;
	}

// Replace the current line with the given text and return status.  (Used only for setting the $lineText system variable.)
static int putLineText(const char *text) {

	if(allowEdit(true) != Success)			// Don't allow if read-only buffer.
		return sess.rtn.status;

	// Delete any text on the current line.
	if(sess.cur.pFace->point.pLine->used > 0) {
		sess.cur.pFace->point.offset = 0;	// Start at the beginning of the line.
		if(kdcText(1, 0, NULL) != Success)	// Put it in the undelete buffer.
			return sess.rtn.status;
		}

	// Insert the new text.
	return einserts(text);
	}

// Get value of a system variable, given result pointer and table pointer.
int getSysVar(Datum *pRtnVal, SysVar *pSysVar) {
	SysVarId varId;
	char *str;
	char workBuf[32];

	// Fetch the corresponding value.
	switch(varId = pSysVar->id) {
		case sv_ArgList:
			if(pScriptRun == NULL)
				dsetnil(pRtnVal);
			else if(dcpy(pRtnVal, pScriptRun->pArgs) != 0)
				goto LibFail;
			break;
		case sv_BufInpDelim:
			str = sess.cur.pBuf->inpDelim.u.delim;
			goto Kopy;
		case sv_BufModes:
			(void) getModes(pRtnVal, sess.cur.pBuf);
			break;
		case sv_Date:
			str = strTime();
			goto Kopy;
		case sv_GlobalModes:
			(void) getModes(pRtnVal, NULL);
			break;
		case sv_HorzScrollCol:
			dsetint(modeSet(MdIdxHScrl, NULL) ? sess.cur.pFace->firstCol : sess.cur.pScrn->firstCol, pRtnVal);
			break;
		case sv_LastKey:
			dsetint((long)(keyEntry.lastKeySeq & (Prefix | Shift | FKey | 0x80) ? -1 :
			 keyEntry.lastKeySeq & Ctrl ? ektoc(keyEntry.lastKeySeq, false) : keyEntry.lastKeySeq), pRtnVal);
			break;
		case sv_LineLen:
			dsetint((long) sess.cur.pFace->point.pLine->used, pRtnVal);
			break;
		case sv_Match:
			str = (pLastMatch == NULL) ? "" : pLastMatch->str;
			goto Kopy;
		case sv_RegionText:
			{Region region;

			// Get the region limits.
			if(getRegion(&region, RForceBegin) != Success)
				return sess.rtn.status;

			// Preallocate a string and copy.
			if(dsalloc(pRtnVal, region.size + 1) != 0)
				goto LibFail;
			regionToStr(pRtnVal->str, &region);
			}
			break;
		case sv_ReturnMsg:
			str = sess.scriptRtn.msg.str;
			goto Kopy;
		case sv_RingNames:
			(void) ringNames(pRtnVal);
			break;
		case sv_RunFile:
			str = fixNull((pScriptRun == NULL) ? NULL : pScriptRun->path);
			goto Kopy;
		case sv_RunName:
			{Buffer *pBuf = (pScriptRun == NULL) ? NULL : pScriptRun->pBuf;
			str = fixNull(pBuf == NULL ? NULL : *pBuf->bufname == BCmdFuncLead ? pBuf->bufname + 1 :
			 pBuf->bufname);
			}
			goto Kopy;
		case sv_ScreenCount:
			dsetint((long) scrnCount(), pRtnVal);
			break;
		case sv_TermSize:
			{Array *pArray;

			if((pArray = anew(2, NULL)) == NULL)
				goto LibFail;
			agStash(pRtnVal, pArray);
			dsetint(term.cols, pArray->elements[0]);
			dsetint(term.rows, pArray->elements[1]);
			}
			break;
		case sv_WindCount:
			dsetint((long) getWindCount(sess.cur.pScrn, NULL), pRtnVal);
			break;
		case sv_autoSave:
			dsetint((long) sess.autoSaveTrig, pRtnVal);
			break;
		case sv_bufFile:
			if((str = sess.cur.pBuf->filename) != NULL)
				goto Kopy;
			dsetnil(pRtnVal);
			break;
		case sv_bufLineNum:
			dsetint(getLineNum(sess.cur.pBuf, sess.cur.pFace->point.pLine), pRtnVal);
			break;
		case sv_bufname:
			str = sess.cur.pBuf->bufname;
			goto Kopy;
		case sv_execPath:
			str = execPath;
			goto Kopy;
		case sv_fencePause:
			dsetint((long) sess.fencePause, pRtnVal);
			break;
		case sv_hardTabSize:
			dsetint((long) sess.cur.pScrn->hardTabSize, pRtnVal);
			break;
		case sv_horzJump:
			dsetint((long) vTerm.horzJumpPct, pRtnVal);
			break;
		case sv_inpDelim:
			str = fileInfo.userInpDelim.u.delim;
			goto Kopy;
		case sv_lastKeySeq:
			ektos(keyEntry.lastKeySeq, str = workBuf, false);
			goto Kopy;
		case sv_lineChar:
			{Point *pPoint = &sess.cur.pFace->point;
			dsetint(bufEnd(pPoint) ? '\0' : pPoint->offset == pPoint->pLine->used ? '\n' :
			 pPoint->pLine->text[pPoint->offset], pRtnVal);
			}
			break;
		case sv_lineCol:
			dsetint((long) getCol(NULL, false), pRtnVal);
			break;
		case sv_lineOffset:
			dsetint((long) sess.cur.pFace->point.offset, pRtnVal);
			break;
		case sv_lineText:
			{Line *pLine = sess.cur.pFace->point.pLine;
			if(dsetsubstr(pLine->text, pLine->used, pRtnVal) != 0)
				goto LibFail;
			}
			break;
		case sv_maxCallDepth:
			dsetint((long) maxCallDepth, pRtnVal);
			break;
		case sv_maxLoop:
			dsetint((long) maxLoop, pRtnVal);
			break;
		case sv_maxPromptPct:
			dsetint((long) term.maxPromptPct, pRtnVal);
			break;
		case sv_otpDelim:
			str = fileInfo.userOtpDelim.u.delim;
			goto Kopy;
		case sv_pageOverlap:
			dsetint((long) sess.overlap, pRtnVal);
			break;
		case sv_replacePat:
			str = bufSearch.match.replPat;
			goto Kopy;
		case sv_screenNum:
			dsetint((long) sess.cur.pScrn->num, pRtnVal);
			break;
		case sv_searchDelim:
			ektos(bufSearch.inpDelim, str = workBuf, false);
			goto Kopy;
		case sv_searchPat:
			{char patBuf[bufSearch.match.patLen + OptCh_N + 1];

			if(dsetstr(makePat(patBuf, &bufSearch.match), pRtnVal) != 0)
				goto LibFail;
			}
			break;
		case sv_softTabSize:
			dsetint((long) sess.cur.pScrn->softTabSize, pRtnVal);
			break;
		case sv_travJump:
			dsetint((long) sess.travJumpCols, pRtnVal);
			break;
		case sv_vertJump:
			dsetint((long) vTerm.vertJumpPct, pRtnVal);
			break;
		case sv_windLineNum:
			dsetint((long) getWindPos(sess.cur.pWind), pRtnVal);
			break;
		case sv_windNum:
			dsetint((long) getWindNum(sess.cur.pWind), pRtnVal);
			break;
		case sv_windSize:
			dsetint((long) sess.cur.pWind->rows, pRtnVal);
			break;
		case sv_workDir:
			str = (char *) sess.cur.pScrn->workDir;
			goto Kopy;
		case sv_wrapCol:
			dsetint((long) sess.cur.pScrn->wrapCol, pRtnVal);
			break;
		default:
			// Never should get here.
			return rsset(FatalError, 0, text3, "getSysVar", varId, pSysVar->name);
					// "%s(): Unknown ID %d for variable '%s'!"
		}
	goto Retn;
Kopy:
	if(dsetstr(str, pRtnVal) != 0)
LibFail:
		(void) libfail();
Retn:
	return sess.rtn.status;
	}

// Copy a new value to a variable, checking if old value is an array in a global variable.
static int copyNewVal(Datum *pDest, Datum *pSrc, VarDesc *pVarDesc) {

	if(dtyparray(pDest) && pVarDesc->type == VTyp_GlobalVar)
		agTrack(pDest);
	return dcpy(pDest, pSrc) != 0 ? libfail() : sess.rtn.status;
	}

// Calculate horizontal jump columns from percentage.
void calcHorzJumpCols(void) {

	if((vTerm.horzJumpCols = vTerm.horzJumpPct * term.cols / 100) == 0)
		vTerm.horzJumpCols = 1;
	}

// Set a variable to given value.  Return status.
int setVar(Datum *pDatum, VarDesc *pVarDesc) {
	const char *str;
	static const char myName[] = "putVar";

	// Set the appropriate value.
	switch(pVarDesc->type) {

		// Set a user variable.
		case VTyp_LocalVar:
		case VTyp_GlobalVar:
			{UserVar *pUserVar = pVarDesc->p.pUserVar;		// Grab pointer to old value.
			(void) copyNewVal(pUserVar->pValue, pDatum, pVarDesc);
			}
			break;

		// Set a system variable.
		case VTyp_SysVar:
			{int i;
			ushort extKey;
			SysVar *pSysVar = pVarDesc->p.pSysVar;
			Datum *pSink;						// For throw-away return value, if any.

			// Can't modify a read-only variable.
			if(pSysVar->flags & V_RdOnly)
				return rsset(Failure, RSTermAttr, text164, pSysVar->name);
						// "Cannot modify read-only variable '~b%s~B'"

			// Not read-only.  Check for legal value types.  Array and string variables may also be nil (if V_Nil
			// flag set), but all other variables are single-type only.
			if(pSysVar->flags & V_Int) {
				if(!isIntVal(pDatum))
					goto BadTyp1;
				}
			else if(pSysVar->flags & V_Char) {
				if(!isCharVal(pDatum))
					goto BadTyp1;
				}
			else if(dtypbool(pDatum)) {
				str = text360;
					// "Boolean"
				goto BadTyp0;
				}
			else if(disnil(pDatum)) {
				if(pSysVar->flags & V_Nil)
					dsetnull(pDatum);
				else {
					str = text359;
						// "nil"
BadTyp0:
					(void) rsset(Failure, 0, text358, str);
							// "Illegal use of %s value"
					goto BadTyp1;
					}
				}
			else if(pSysVar->flags & V_Array) {
				if(!isArrayVal(pDatum))
					goto BadTyp1;
				}
			else if(!isStrVal(pDatum)) {
				DFab msg;
BadTyp1:
				str = pSysVar->name;
BadTyp2:
				if(dopentrack(&msg) != 0)
					goto LibFail;
				else if(escAttr(&msg) == Success) {
					if(dputf(&msg, 0, text334, str) != 0 || dclose(&msg, FabStr) != 0)
							// ", setting variable '~b%s~B'"
						goto LibFail;
					(void) rsset(sess.rtn.status, RSForce | RSNoFormat | RSTermAttr, msg.pDatum->str);
					}
				return sess.rtn.status;
				}

			// Do specific action for referenced (mutable) variable.
			if(dnewtrack(&pSink) != 0)
				goto LibFail;
			switch(pSysVar->id) {
				case sv_autoSave:
					{int n;

					if(pDatum->u.intNum < 0) {
						i = 0;
						goto ERange;
						}
					if((n = (pDatum->u.intNum > INT_MAX ? INT_MAX : pDatum->u.intNum)) == 0) {

						// ASave count set to zero... turn off global mode and clear counter.
						clearGlobalMode(modeInfo.cache[MdIdxASave]);
						sess.autoSaveTrig = sess.autoSaveCount = 0;
						}
					else {
						int diff = n - sess.autoSaveTrig;
						if(diff != 0) {

							// New count > 0.  Adjust counter accordingly.
							sess.autoSaveTrig = n;
							if(diff > 0) {
								if((long) sess.autoSaveCount + diff > INT_MAX)
									sess.autoSaveCount = INT_MAX;
								else
									sess.autoSaveCount += diff;
								}
							else if((sess.autoSaveCount += diff) <= 0)
								sess.autoSaveCount = 1;
							}
						}
					}
					break;
				case sv_bufFile:
					str = "0 => setBufFile $bufname, ";
					goto XeqCmd;
				case sv_bufLineNum:
					(void) goLine(pSink, INT_MIN, pDatum->u.intNum);
					break;
				case sv_bufname:
					str = "renameBuf $bufname, ";
					goto XeqCmd;
				case sv_execPath:
					(void) setExecPath(pDatum->str);
					break;
				case sv_fencePause:
					if(pDatum->u.intNum < 0)
						return rsset(Failure, 0, text39, text119, (int) pDatum->u.intNum, 0);
							// "%s (%d) must be %d or greater", "Pause duration"
					sess.fencePause = pDatum->u.intNum;
					break;
				case sv_hardTabSize:
					if(setTabSize((int) pDatum->u.intNum, true) == Success)
						(void) supd_windFlags(NULL, WFHard | WFMode);
					break;
				case sv_horzJump:
					if((vTerm.horzJumpPct = pDatum->u.intNum) < 0)
						vTerm.horzJumpPct = 0;
					else if(vTerm.horzJumpPct > JumpMax)
						vTerm.horzJumpPct = JumpMax;
					calcHorzJumpCols();
					break;
				case sv_inpDelim:
					if(strlen(pDatum->str) > sizeof(fileInfo.userInpDelim.u.delim) - 1)
						return rsset(Failure, 0, text251, text46,
							// "%s delimiter '%s' cannot be more than %d character(s)", "Input"
						 pDatum->str, sizeof(fileInfo.userInpDelim.u.delim) - 1);
					strcpy(fileInfo.userInpDelim.u.delim, pDatum->str);
					break;
				case sv_lastKeySeq:
					// Don't allow variable to be set to a prefix key (pseudo command).
					if(stoek(pDatum->str, &extKey) == Success) {
						KeyBind *pKeyBind = getBind(extKey);
						if(pKeyBind != NULL && pKeyBind->targ.type == PtrPseudo)
							return rsset(Failure, RSTermAttr, text373, pSysVar->name);
									// "Illegal value for '~b%s~B' variable"
						keyEntry.lastKeySeq = extKey;
						keyEntry.useLast = true;
						}
					break;
				case sv_lineChar:
					// Replace character at point with an 8-bit integer value.
					if(isCharVal(pDatum)) {
						if(edelc(1, 0) != Success)
							return rsset(Failure, 0, text142, sess.cur.pBuf->bufname);
								// "Cannot change a character past end of buffer '%s'"
						(void) einsertc(1, pDatum->u.intNum);
						}
					break;
				case sv_lineCol:
					(void) setPointCol(pDatum->u.intNum);
					break;
				case sv_lineOffset:
					{int lineLen = sess.cur.pFace->point.pLine->used;
					int lineOffset = (pDatum->u.intNum < 0) ? lineLen + pDatum->u.intNum : pDatum->u.intNum;
					if(lineOffset < 0 || lineOffset > lineLen)
						return rsset(Failure, 0, text378, pDatum->u.intNum);
							// "Line offset value %ld out of range"
					sess.cur.pFace->point.offset = lineOffset;
					}
					break;
				case sv_lineText:
					(void) putLineText(pDatum->str);
					break;
				case sv_maxCallDepth:
					if(pDatum->u.intNum < 0) {
						i = 0;
						goto ERange;
						}
					maxCallDepth = pDatum->u.intNum;
					break;
				case sv_maxLoop:
					if(pDatum->u.intNum < 0) {
						i = 0;
ERange:
						return rsset(Failure, RSTermAttr, text111, pSysVar->name, i);
							// "'~b%s~B' value must be %d or greater"
						}
					maxLoop = pDatum->u.intNum;
					break;
				case sv_maxPromptPct:
					if(pDatum->u.intNum < 15 || pDatum->u.intNum > 90)
						return rsset(Failure, RSTermAttr, text379, pSysVar->name, 15, 90);
							// "'~b%s~B' value must be between %d and %d"
					term.maxPromptPct = pDatum->u.intNum;
					break;
				case sv_otpDelim:
					if((size_t) (i = strlen(pDatum->str)) > sizeof(fileInfo.userOtpDelim.u.delim) - 1)
						return rsset(Failure, 0, text251, text47, pDatum->str,
							// "%s delimiter '%s' cannot be more than %d character(s)", "Output"
						 (int) sizeof(fileInfo.userOtpDelim.u.delim) - 1);
					strcpy(fileInfo.userOtpDelim.u.delim, pDatum->str);
					fileInfo.userOtpDelim.len = i;
					break;
				case sv_pageOverlap:
					if(pDatum->u.intNum < 0 || pDatum->u.intNum > (int) (term.rows - 1) / 2)
						return rsset(Failure, 0, text184, pDatum->u.intNum, (int) (term.rows - 1) / 2);
							// "Overlap %ld must be between 0 and %d"
					sess.overlap = pDatum->u.intNum;
					break;
				case sv_replacePat:
					(void) newReplPat(pDatum->str, &bufSearch.match, true);
					break;
				case sv_screenNum:
					(void) gotoScreen(pDatum->u.intNum, 0);
					break;
				case sv_searchDelim:
					if(stoek(pDatum->str, &extKey) != Success)
						return sess.rtn.status;
					if(extKey & Prefix) {
						char keyBuf[16];

						return rsset(Failure, RSTermAttr, text341, ektos(extKey, keyBuf, true),
							// "Cannot use key sequence ~#u%s~U as %s delimiter"
						 text343);
							// "search"
						}
					bufSearch.inpDelim = extKey;
					break;
				case sv_searchPat:
					// Make copy so original is returned unmodified.
					{char workPat[strlen(pDatum->str) + 1];
					(void) newSearchPat(strcpy(workPat, pDatum->str), &bufSearch.match, NULL, true);
					}
					break;
				case sv_softTabSize:
					if(setTabSize((int) pDatum->u.intNum, false) == Success)
						(void) supd_windFlags(NULL, WFHard | WFMode);
					break;
				case sv_travJump:
					if((sess.travJumpCols = pDatum->u.intNum) < 4)
						sess.travJumpCols = 4;
					else if(sess.travJumpCols > term.cols / 4 - 1)
						sess.travJumpCols = term.cols / 4 - 1;
					break;
				case sv_vertJump:
					if((vTerm.vertJumpPct = pDatum->u.intNum) < 0)
						vTerm.vertJumpPct = 0;
					else if(vTerm.vertJumpPct > JumpMax)
						vTerm.vertJumpPct = JumpMax;
					break;
				case sv_windLineNum:
					(void) forwLine(pSink, pDatum->u.intNum - getWindPos(sess.cur.pWind), NULL);
					break;
				case sv_windNum:
					(void) gotoWind(pSink, pDatum->u.intNum, 0);
					break;
				case sv_windSize:
					(void) resizeWind(pSink, pDatum->u.intNum, NULL);
					break;
				case sv_workDir:
					str = "chgDir";
XeqCmd:
					(void) runCmd(pSink, str, pDatum->str, NULL, RunQFull);
					break;
				case sv_wrapCol:
					(void) setWrapCol(pDatum->u.intNum);
					break;
				default:
					// Never should get here.
					return rsset(FatalError, 0, text3, myName, pSysVar->id, pSysVar->name);
						// "%s(): Unknown ID %d for variable '%s'!"
				}
			}
			break;

		// Set a user command or function argument.
		case VTyp_NumVar:
			if(pVarDesc->i.argNum == 0) {		// Allow numeric assignment (only) to $0.
				if(!isIntVal(pDatum)) {
					str = "$0";
					goto BadTyp2;
					}
				dsetint(pDatum->u.intNum, pScriptRun->pNArg);
				}
			else
				// User command or function argument assignment.  Get array argument and set new value.
				(void) copyNewVal(pVarDesc->p.pArgs->u.pArray->elements[pVarDesc->i.argNum - 1], pDatum,
				 pVarDesc);
			break;
		default:	// VTyp_ArrayElRef
			{Datum *pArrayEl = aget(pVarDesc->p.pArray, pVarDesc->i.index, 0);
			if(pArrayEl == NULL)
				goto LibFail;
			if(dtyparray(pArrayEl))
				agTrack(pArrayEl);
			(void) dcpy(pArrayEl, pDatum);
			}
			break;
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Create local or global user variable, given name and descriptor pointer.  Return status.
static int newUserVar(const char *var, VarDesc *pVarDesc) {
	UserVar *pUserVar;
	char *str;
	const char *name = var + (*var == TokC_GlobalVar ? 1 : 0);

	// Invalid length?
	if(*var == '\0' || *name == '\0' || strlen(var) > MaxVarName)
		return rsset(Failure, 0, text280, text279, MaxVarName);
			// "%s name cannot be null or exceed %d characters", "Variable"

	// Valid variable name?
	str = (char *) name;
	if(getIdent(&str, NULL) != s_ident || *str != '\0')
		(void) rsset(Failure, 0, text447, text286, name);
			// "Invalid %s '%s'", "identifier"

	// Allocate new record, set its values, and add to beginning of list.
	if((pUserVar = (UserVar *) malloc(sizeof(UserVar))) == NULL)
		return rsset(Panic, 0, text94, "newUserVar");
				// "%s(): Out of memory!"
	strcpy((pVarDesc->p.pUserVar = pUserVar)->name, var);
	if(*var == TokC_GlobalVar) {
		pVarDesc->type = VTyp_GlobalVar;
		pUserVar->flags = V_Global;
		pUserVar->next = globalVarRoot;
		globalVarRoot = pUserVar;
		}
	else {
		pVarDesc->type = VTyp_LocalVar;
		pUserVar->flags = 0;
		pUserVar->next = localVarRoot;
		localVarRoot = pUserVar;
		}

	// Set value of new variable to a null string.
	return dnew(&pUserVar->pValue);
	}

// Find a named variable's type and id.  If op is OpCreate: (1), create user variable if non-existent and either (a), variable
// is global; or (b), variable is local and executing a buffer; and (2), return status.  If op is OpQuery: return true if
// variable is found, otherwise false.  If op is OpDelete: return status if variable is found, otherwise error.  In all cases,
// store results in *pVarDesc (if pVarDesc not NULL) and variable is found.
int findVar(const char *name, VarDesc *pVarDesc, ushort op) {
	UserVar *pUserVar;
	ssize_t i;
	VarDesc varDesc;

	// Get ready.
	varDesc.p.pUserVar = NULL;
	varDesc.type = VTyp_Unk;
	varDesc.i.argNum = 0;

	// Check lead-in character.
	if(*name == TokC_GlobalVar) {
		if(strlen(name) > 1) {

			// User command or function argument reference?
			if(is_digit(name[1])) {
				long longVal;

				// Yes, command or function running and number in range?
				if(pScriptRun != NULL && ascToLong(name + 1, &longVal, true) &&
				 longVal <= pScriptRun->pArgs->u.pArray->used) {

					// Valid reference.  Set type and save argument number.
					varDesc.type = VTyp_NumVar;
					varDesc.i.argNum = longVal;
					varDesc.p.pArgs = pScriptRun->pArgs;
					goto Found;
					}
				}
			else {
				// Check for existing global variable.
				if((pUserVar = findUserVar(name)) != NULL)
					goto UserVarFound;

				// Check for existing system variable.
				if(binSearch(name + 1, (const void *) sysVars, NumSysVars, strcmp, sysVarName, &i)) {
					varDesc.type = VTyp_SysVar;
					varDesc.p.pSysVar = sysVars + i;
					goto Found;
					}

				// Not found.  Create new one?
				if(op == OpCreate)
					goto Create;
				goto VarNotFound;
				}
			}
		}
	else if(*name != '\0') {

		// Check for existing local variable.
		if((pUserVar = findUserVar(name)) != NULL) {
UserVarFound:
			varDesc.type = (pUserVar->flags & V_Global) ? VTyp_GlobalVar : VTyp_LocalVar;
			varDesc.p.pUserVar = pUserVar;
			goto Found;
			}

		// Not found.  Create a new one (if executing a buffer)?
		if(op != OpCreate || pScriptRun == NULL)
			goto VarNotFound;

		// Create variable.  Local variable name same as an existing command, pseudo-command, function, or alias?
		if(execFind(name, OpQuery, PtrAny, NULL))
			return rsset(Failure, RSTermAttr, text165, name);
				// "Name '~b%s~B' already in use"
Create:
		if(newUserVar(name, &varDesc) != Success)
			return sess.rtn.status;
Found:
		if(pVarDesc != NULL)
			*pVarDesc = varDesc;
		return (op == OpQuery) ? true : sess.rtn.status;
		}
VarNotFound:
	// Variable not found.
	return (op == OpQuery) ? false : rsset(Failure, 0, text52, name);
					// "No such variable '%s'"
	}

// Derefence a variable, given descriptor, and save variable's value in *pDatum.  Return status.
int vderefv(Datum *pDatum, VarDesc *pVarDesc) {
	Datum *pValue;

	switch(pVarDesc->type) {
		case VTyp_LocalVar:
		case VTyp_GlobalVar:
			pValue = pVarDesc->p.pUserVar->pValue;
			break;
		case VTyp_SysVar:
			return getSysVar(pDatum, pVarDesc->p.pSysVar);
		case VTyp_NumVar:
			{ushort argNum = pVarDesc->i.argNum;

			// Get argument value.  $0 resolves to the user command or function "n" argument.
			pValue = (argNum == 0) ? pScriptRun->pNArg : pVarDesc->p.pArgs->u.pArray->elements[argNum - 1];
			}
			break;
		default:	// VTyp_ArrayElRef
			if((pValue = aget(pVarDesc->p.pArray, pVarDesc->i.index, 0)) == NULL)
				return libfail();
		}

	// Copy value.
	return dcpy(pDatum, pValue) != 0 ? libfail() : sess.rtn.status;
	}

// Derefence a variable, given name, and save variable's value in *pDatum.  Return status.
int vderefn(Datum *pDatum, const char *name) {
	VarDesc varDesc;

	// Find and dereference variable.
	if(findVar(name, &varDesc, OpDelete) == Success)
		(void) vderefv(pDatum, &varDesc);

	return sess.rtn.status;
	}

// Simulate dputd() CXL library function with terminal attribute escaping; that is, put a datum to a fabrication object with
// all AttrSpecBegin (~) characters doubled (~~).  Return same codes as dputd().
int dtofabattr(const Datum *pDatum, DFab *pFab, const char *delim, ushort cflags) {
	int rtnCode;
	DFab fab;
	char *str;

	// Convert datum to a string first, then write result to pFab with AttrSpecBegin characters doubled up.
	if(dopentrack(&fab) != 0 || (rtnCode = dputd(pDatum, &fab, delim, cflags)) < 0 || dclose(&fab, FabStr) != 0)
		return -1;
	str = fab.pDatum->str;
	while(*str != '\0') {
		if(dputc(*str, pFab, 0) != 0 || (*str == AttrSpecBegin && dputc(AttrSpecBegin, pFab, 0) != 0))
			return -1;
		++str;
		}
	return rtnCode;
	}

// Store the character value of a system variable in a fabrication object in "show" (?x) form.  Return status.
static int ctosf(Datum *pDatum, DFab *pFab) {
	short c = pDatum->u.intNum;

	if(dputc('?', pFab, 0) != 0)
		goto LibFail;
	if(c < '!' || c > '~') {
		if(dputc('\\', pFab, 0) != 0)
			goto LibFail;
		switch(c) {
			case '\t':	// Tab
				c = 't'; break;
			case '\r':	// CR
				c = 'r'; break;
			case '\n':	// NL
				c = 'n'; break;
			case '\e':	// Escape
				c = 'e'; break;
			case ' ':	// Space
				c = 's'; break;
			case '\f':	// Form feed
				c = 'f'; break;
			default:
				if(dputf(pFab, 0, "x%.2hX", c) != 0)
					goto LibFail;
				return sess.rtn.status;
			}
		}
	if(dputc(c, pFab, 0) != 0)
LibFail:
		(void) libfail();
	return sess.rtn.status;
	}

// Get the value of a system variable and store in a fabrication object in "show" form.  Return status.
int svtofab(SysVar *pSysVar, bool escTermAttr, DFab *pFab) {
	int rtnCode;
	Datum *pDatum;
	Point *pPoint = &sess.cur.pFace->point;

	// Get value.  Return null for $RegionText if no region defined or empty; use single-quote form for $replacePat
	// and $searchPat; use char-lit form for character variables.
	if(dnewtrack(&pDatum) != 0)
		goto LibFail;
	if(pSysVar->id == sv_RegionText || (pSysVar->id == sv_lineText && pPoint->pLine->used > term.cols * 2)) {

		// Cap size of region or current line in case it's huge -- it will be truncated when displayed anyway.
		if(pSysVar->id == sv_lineText) {
			if(dsalloc(pDatum, term.cols * 2 + 4) != 0)
				goto LibFail;
			strcpy((char *) memstpcpy((void *) pDatum->str, (void *) (pPoint->pLine->text), term.cols * 2), "...");
			}
		else if(sess.cur.pBuf->markHdr.point.pLine != pPoint->pLine ||
		 sess.cur.pBuf->markHdr.point.offset != pPoint->offset) {
			Region region;
			bool truncated = false;

			if(getRegion(&region, RForceBegin) != Success)
				return sess.rtn.status;
			if(region.size > term.cols * 2) {
				region.size = term.cols * 2;
				truncated = true;
				}
			if(dsalloc(pDatum, region.size + 4) != 0)
				goto LibFail;
			regionToStr(pDatum->str, &region);
			if(truncated)
				strcpy(pDatum->str + term.cols * 2, "...");
			}
		else
			// Have zero-length region.
			dsetnull(pDatum);
		}
	else if(getSysVar(pDatum, pSysVar) != Success)
		return sess.rtn.status;

	// Have system variable value in *pDatum.  Convert it to display form.
	if(pSysVar->flags & V_Char)
		return ctosf(pDatum, pFab);
	int (*func)(const Datum *, DFab *, const char *, ushort) = escTermAttr ? dtofabattr : dputd;
	if((rtnCode = func(pDatum, pFab, ", ", pSysVar->id == sv_replacePat || pSysVar->id == sv_searchPat ?
	 DCvtVizStr : DCvtLang)) >= 0)
		return endless(rtnCode);
LibFail:
	return libfail();
	}

// Set a variable interactively ("let" command).  Evaluate value as an expression if n argument.  *Interactive only*
int let(Datum *pRtnVal, int n, Datum **args) {
	VarDesc varDesc;			// Variable descriptor.
	short delimChar;
	uint argFlags, ctrlFlags;
	long longVal;
	Datum *pDatum;
	DFab fab;

	// First get the variable to set.
	if(dnewtrack(&pDatum) != 0)
		return libfail();
	if(termInp(pDatum, text51, ArgNil1, Term_C_MutVar, NULL) != Success || disnil(pDatum))
			// "Assign variable"
		goto Retn;

	// Find variable.
	if(findVar(pDatum->str, &varDesc, OpCreate) != Success)
		goto Retn;

	// Error if read-only.
	if(varDesc.type == VTyp_SysVar && (varDesc.p.pSysVar->flags & V_RdOnly))
		return rsset(Failure, RSTermAttr, text164, pDatum->str);
				// "Cannot modify read-only variable '~b%s~B'"

	// Build prompt with old value.
	if(n == INT_MIN) {
		delimChar = (varDesc.type == VTyp_SysVar && (varDesc.p.pSysVar->flags & V_EscDelim)) ? (Ctrl | '[') : RtnKey;
		argFlags = ArgNil1;
		ctrlFlags = (varDesc.type != VTyp_SysVar || !(varDesc.p.pSysVar->flags & (V_Char | V_GetKey | V_GetKeySeq))) ?
		 0 : (varDesc.p.pSysVar->flags & V_Char) ? Term_OneChar : (varDesc.p.pSysVar->flags & V_GetKeySeq) ?
		 Term_OneKeySeq : Term_OneKey;
		}
	else {
		delimChar = RtnKey;
		argFlags = ArgNotNull1;
		ctrlFlags = 0;
		}
	if(dopenwith(&fab, pRtnVal, FabClear) != 0 || dputs(text297, &fab, 0) != 0)
						// "Current value: "
		goto LibFail;
	if(varDesc.type == VTyp_SysVar) {
		if(varDesc.p.pSysVar->flags & (V_GetKey | V_GetKeySeq)) {
			if(getSysVar(pDatum, varDesc.p.pSysVar) != Success)
				goto Retn;
			if(dputf(&fab, 0, "~#u%s~U", pDatum->str) != 0)
				goto LibFail;
			ctrlFlags |= Term_Attr;
			}
		else if(svtofab(varDesc.p.pSysVar, false, &fab) != Success)
			goto Retn;
		}
	else if(dputd(varDesc.p.pUserVar->pValue, &fab, NULL, DCvtLang) < 0)
		goto LibFail;

	// Add "new value" type to prompt.
	if(dputs(text283, &fab, 0) != 0)
			// ", new value"
		goto LibFail;
	if(n != INT_MIN) {
		if(dputs(text301, &fab, 0) != 0)
				// " (expression)"
			goto LibFail;
		}
	else if(varDesc.type == VTyp_SysVar && (varDesc.p.pSysVar->flags & (V_Char | V_GetKey | V_GetKeySeq))) {
		if(dputs(varDesc.p.pSysVar->flags & V_Char ? text349 : text76, &fab, 0) != 0)
						// " (char)", " (key)"
			goto LibFail;
		}
	if(dclose(&fab, FabStr) != 0)
LibFail:
		return libfail();

	// Get new value.
	TermInpCtrl termInpCtrl = {NULL, delimChar, 0, NULL};
	if(termInp(pDatum, pRtnVal->str, argFlags, ctrlFlags, &termInpCtrl) != Success)
		goto Retn;

	// Evaluate result as an expression if requested.  Type checking is done in setVar() so no need to do it here.
	if(n != INT_MIN) {
		if(execExprStmt(pRtnVal, pDatum->str, 0, NULL) != Success)
			goto Retn;
		}
	else if(dtypstr(pDatum) && (varDesc.type == VTyp_GlobalVar ||
	 (varDesc.type == VTyp_SysVar && (varDesc.p.pSysVar->flags & V_Int))) && ascToLong(pDatum->str, &longVal, true))
		dsetint(longVal, pRtnVal);
	else
		dxfer(pRtnVal, pDatum);

	// Set variable to value in pRtnVal and return.
#if MMDebug & Debug_Datum
	ddump(pRtnVal, "let(): Setting and returning value...");
	(void) setVar(pRtnVal, &varDesc);
	dumpVars();
#else
	(void) setVar(pRtnVal, &varDesc);
#endif
Retn:
	return sess.rtn.status;
	}

// Convert an array reference node to a VarDesc object and check if referenced element exists.  If not, create it if "create" is
// true; otherwise, set an error.  Return status.
int getArrayRef(ExprNode *pNode, VarDesc *pVarDesc, bool create) {

	pVarDesc->type = VTyp_ArrayElRef;
	pVarDesc->i.index = pNode->index;
	pVarDesc->p.pArray = pNode->pValue->u.pArray;
	if(aget(pVarDesc->p.pArray, pVarDesc->i.index, create ? AOpGrow : 0) == NULL)
		(void) libfail();
	return sess.rtn.status;
	}

// Increment or decrement a variable or array reference, given node pointer, "incr" flag, and "pre" flag.  Set node to result
// and return status.
int bumpVar(ExprNode *pNode, bool incr, bool pre) {
	VarDesc varDesc;
	long longVal;
	Datum *pDatum;

	if(pNode->flags & EN_ArrayRef) {
		if(getArrayRef(pNode, &varDesc, false) != Success)	// Get array element...
			return sess.rtn.status;
		if(!isIntVar(&varDesc))					// and make sure it's an integer.
			return rsset(Failure, 0, text370, varDesc.i.index);
				// "Array element %d not an integer";
		}
	else {
		if(findVar(pNode->pValue->str, &varDesc, OpDelete) != Success)	// or find variable...
			return sess.rtn.status;
		if(!isIntVar(&varDesc))					// and make sure it's an integer.
			return rsset(Failure, RSTermAttr, text212, pNode->pValue->str);
				// "Variable '~b%s~B' not an integer"
		}
	if(dnewtrack(&pDatum) != 0)
		return libfail();
	if(vderefv(pDatum, &varDesc) != Success)			// Dereference variable...
		return sess.rtn.status;
	longVal = pDatum->u.intNum + (incr ? 1 : -1);			// compute new value of variable...
	dsetint(pre ? longVal : pDatum->u.intNum, pNode->pValue);	// set result to pre or post value...
	dsetint(longVal, pDatum);					// set new variable value in a Datum object...
	return setVar(pDatum, &varDesc);				// and update variable.
	}

#if MMDebug & Debug_Datum
// Dump all user variables to the log file.
void dumpVars(void) {
	UserVar *pUserVar;
	struct UserVarInfo {
		char *label;
		UserVar **pVarRoot;
		} *pUserVarInfo;
	static struct UserVarInfo userVarTable[] = {
		{"GLOBAL", &globalVarRoot},
		{"LOCAL", &localVarRoot},
		{NULL, NULL}};

	pUserVarInfo = userVarTable;
	do {
		fprintf(logfile, "%s VARS\n", pUserVarInfo->label);
		if((pUserVar = *pUserVarInfo->pVarRoot) != NULL) {
			do {
				ddump(pUserVar->pValue, pUserVar->name);
				} while((pUserVar = pUserVar->next) != NULL);
			}
		} while((++pUserVarInfo)->label != NULL);
	}
#endif
