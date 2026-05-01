// (c) Copyright 2026 Richard W. Marinelli
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
#include "exec.h"
#include "file.h"
#include "search.h"
#include "var.h"

// Initialize sConstVarTable.
static int cvinit1(SConstVar *pSConstVar) {
	HashNode *pNode;

	do {
		if((pNode = hnewnode(&sConstVarLookup, pSConstVar->name, NULL)) == NULL)
			return -1;
		dsetmemref((void *) pSConstVar, sizeof(*pSConstVar), pNode->pVal);
		} while((++pSConstVar)->name != NULL);
	return 0;
	}

// Initialize the global constant/variable hash and the system constant/variable lookup hash.  Return status.
int cvinit(void) {

	hinit(&gblConstVarTable, GlobalTableSize, 0);
	hinit(&sConstVarLookup, NumSysConsts + NumSysVars + (NumSysConsts + NumSysVars) / 7, 0);
	if(cvinit1(sysConstTable) != 0 || cvinit1(sysVarTable) != 0)
		return libfail();

	return sess.rtn.status;
	}

// Return true if a variable is an integer type, given descriptor, otherwise false.
bool isIntVar(VarDesc *pVarDesc) {
	Datum *pValue;

	switch(pVarDesc->type) {
		case VTyp_LocalVar:
			pValue = pVarDesc->p.pLocalVar->pVal;
			break;
		case VTyp_GConstVar:
			pValue = pVarDesc->p.pGConstVar->pVal;
			break;
		case VTyp_SConstVar:
			return (pVarDesc->p.pSConstVar->flags & (V_Int | V_Char)) != 0;
		case VTyp_NumVar:
			{ArraySize index = pVarDesc->i.index;

			// Get argument value.  $0 resolves to the user routine's n argument, which is always an integer.
			if(index == 0)
				return true;
			pValue = pVarDesc->p.pArray->elements[index - 1];
			}
			break;
		case VTyp_AryElRef:
			pValue = aget(pVarDesc->p.pArray, pVarDesc->i.index, 0);		// Should never return NULL.
			break;
		default:	// VTyp_HshNodRef
			{HashNode *pNode = hsearch(pVarDesc->p.pHash, pVarDesc->i.pKey->str);
			if(pNode == NULL)
				return false;		// Non-existent node is nil (not integer).
			pValue = pNode->pVal;
			}

		}
	return pValue->type == dat_int;
	}

// Return true if c is a valid first character of an identifier, otherwise false.
bool isIdent1(short c) {

	return is_letter(c) || c == '_';
	}

// Return number of global and/or system constants and variables currently in use per ctrlFlags (for building a name completion
// list or "showConstVars" list).
uint cvcount(uint ctrlFlags) {
	uint count = 0;

	// Add system variable and/or constant counts if requested.
	if(ctrlFlags & TermC_SysConst)
		count = NumSysConsts;
	if(ctrlFlags & TermC_SysVar)
		count += NumSysVars;

	// Add global variable and/or constant counts if requested.
	if((ctrlFlags & (TermC_GblConst | TermC_GblVar)) ==
	 (TermC_GblConst | TermC_GblVar))			// Include both global constants and variables.
		count += gblConstVarTable.nodeCount;
	else if(ctrlFlags & (TermC_GblConst | TermC_GblVar)) {	// Include either global constants or variables.
		HashNode *pNode;
		HashEach each;
		uint flag = ctrlFlags & TermC_GblConst ? V_Const : 0;

		heachinit(&each, &gblConstVarTable);
		while((pNode = heach(&each)) != NULL)
			if((((GConstVar *) dgetmemptr(pNode->pVal))->flags & V_Const) == flag)
				++count;
		}

	return count;
	}

// Compare two variable names (for qsort()).
static int varcmp(const void *pVar1, const void *pVar2) {

	return strcmp(*((const char **) pVar1), *((const char **) pVar2));
	}

// Copy system constant or variable name pointers.
static void cvcopy(const char ***pCVList, SConstVar *pSConstVar) {
	const char **cvList = *pCVList;
	do {
		*cvList++ = pSConstVar->name;
		} while((++pSConstVar)->name != NULL);
	*pCVList = cvList;
	}

// Created sorted list of all variables currently in use per ctrlFlags and store in cvList array.
void cvsort(const char *cvList[], uint count, uint ctrlFlags) {
	HashEach each;
	HashNode *pNode;
	GConstVar *pGConstVar;
	const char **cvList0 = cvList;

	// Store system constant and/or variable names if requested.
	if(ctrlFlags & (TermC_SysConst | TermC_SysVar)) {
		if(ctrlFlags & TermC_SysConst)
			cvcopy(&cvList, sysConstTable);
		if(ctrlFlags & TermC_SysVar)
			cvcopy(&cvList, sysVarTable);
		}

	// Store global constant and variable names if requested.
	if(ctrlFlags & (TermC_GblConst | TermC_GblVar)) {
		bool selectAll = (ctrlFlags & (TermC_GblConst | TermC_GblVar)) == (TermC_GblConst | TermC_GblVar);
		uint flag = ctrlFlags & TermC_GblConst ? V_Const : 0;
		heachinit(&each, &gblConstVarTable);
		while((pNode = heach(&each)) != NULL) {
			pGConstVar = (GConstVar *) dgetmemptr(pNode->pVal);
			if(selectAll || (pGConstVar->flags & V_Const) == flag)
				*cvList++ = pGConstVar->name;
			}
		}

	// Sort it.
	qsort((void *) cvList0, count, sizeof(char *), varcmp);
	}

// Free local variable(s), given stack pointer.  All variables will be together at top of list (because they are created in
// stack fashion during script execution and/or recursion).
void freeLocalVars(LocalVar *pVarStack) {
	LocalVar *pLocalVar;

	while(localVarRoot != pVarStack) {
		pLocalVar = localVarRoot->next;

#if MMDebug & Debug_UserVars
		fprintf(logfile, "freeLocalVars(): deleting '%s' user variable...\n", localVarRoot->name);
#endif
		// Free value...
		dfree(localVarRoot->pVal);

		// free variable...
		free((void *) localVarRoot);

		// and advance head pointer.
		localVarRoot = pLocalVar;
		}
	}

// Search local variable list for given name.  If found, return pointer to LocalVar object, otherwise NULL.  Entries beyond
// pScriptRun->pVarStack are not considered so that all local variables created or referenced in a particular user routine
// invocation are visible and accessible only from that routine, which allows recursion to work properly.
LocalVar *findLocalVar(const char *name) {
	LocalVar *pLocalVar;
	LocalVar *pVarStack;		// Search boundary in local variable table.

	pLocalVar = localVarRoot;
	pVarStack = (pScriptRun == NULL) ? NULL : pScriptRun->pVarStack;
	while(pLocalVar != pVarStack) {
		if(strcmp(name, pLocalVar->name) == 0)
			return pLocalVar;
		pLocalVar = pLocalVar->next;
		}

	return NULL;
	}

// Delete a constant or variable, given descriptor (which is assumed to contain a name, not an array or hash reference).
// Return status.
static int vdelete(VarDesc *pVarDesc) {

#if MMDebug & Debug_UserVars
	fputs("vdelete(): deleting ", logfile);
#endif
	switch(pVarDesc->type) {
		case VTyp_LocalVar:
			{LocalVar *pVarStack, *pLocalVar, *pPrevVar;
#if MMDebug & Debug_UserVars
			fprintf(logfile, "local var %s", pVarDesc->p.pLocalVar->name);
#endif
			pPrevVar = NULL;
			pLocalVar = localVarRoot;
			pVarStack = (pScriptRun == NULL) ? NULL : pScriptRun->pVarStack;
			while(pLocalVar != pVarStack) {
				if(pLocalVar == pVarDesc->p.pLocalVar) {

					// Variable found.  Remove it from linked list, free its value, and free object itself.
					if(pPrevVar == NULL)
						localVarRoot = pLocalVar->next;
					else
						pPrevVar->next = pLocalVar->next;
					dfree(pLocalVar->pVal);
					free((void *) pLocalVar);
					break;
					}
				pPrevVar = pLocalVar;
				pLocalVar = pLocalVar->next;
				}
			}
			break;
		case VTyp_GConstVar:
#if MMDebug & Debug_UserVars
			fprintf(logfile, "global CV %s", pVarDesc->p.pGConstVar->name);
#endif
			dfree(hdelete(&gblConstVarTable, pVarDesc->p.pGConstVar->name));
			break;
		default:
			{char *str, workBuf[16];

			if(pVarDesc->type == VTyp_NumVar)
				sprintf(str = workBuf, "%c%ld", TokC_GlobalVar, pVarDesc->i.index);
			else
				str = (char *) pVarDesc->p.pSConstVar->name;
			(void) rsset(Failure, 0, text535, text534, str);
					// "Cannot delete system %s '%s'", "constant or variable"
			}
		}

	return sess.rtn.status;
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

// Get value of a system constant or variable, given result pointer and table pointer.
int getSConstVar(Datum *pRtnVal, SConstVar *pSConstVar) {
	SConstVarId varId;
	const char *str;
	char workBuf[32];

	// Fetch the corresponding value.
	switch(varId = pSConstVar->id) {
		case sv_ArgList:
			if(pScriptRun == NULL)
				dsetnil(pRtnVal);
			else
				dsetarrayref(pScriptRun->pArgs, pRtnVal);
			break;
		case sv_BufInpDelim:
			str = delimSymToStr(sess.cur.pBuf->inpDelimSym);
			goto Kopy;
		case sv_BufModes:
			(void) getModes(pRtnVal, sess.cur.pBuf);
			break;
		case sv_GlobalModes:
			(void) getModes(pRtnVal, NULL);
			break;
		case sv_HorzScrollCol:
			dsetint(modeSet(MdIdxHScrl, NULL) ? sess.cur.pFace->firstCol : sess.pCurScrn->firstCol, pRtnVal);
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
				goto Retn;

			// Preallocate a string and copy.
			if(dsalloc(pRtnVal, region.size + 1) != 0)
				goto LibFail;
			regionToStr(pRtnVal->str, &region);
			}
			break;
		case sv_ReturnMsg:
			str = sess.scriptRtn.msg.str;
			goto Kopy;
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

			if(makeArray(pRtnVal, 2, &pArray) != Success)
				goto Retn;
			dsetint(term.cols, pArray->elements[0]);
			dsetint(term.rows, pArray->elements[1]);
			}
			break;
		case sv_WindCount:
			dsetint((long) getWindCount(sess.pCurScrn, NULL), pRtnVal);
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
			dsetint((long) sess.pCurScrn->hardTabSize, pRtnVal);
			break;
		case sv_horzJump:
			dsetint((long) vTerm.horzJumpPct, pRtnVal);
			break;
		case sv_inpDelim:
			str = delimSymToStr(fileInfo.userInpDelimSym);
			goto Kopy;
		case sv_lastKeySeq:
			ektos((char *)(str = workBuf), keyEntry.lastKeySeq, false);
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
			str = delimSymToStr(fileInfo.userOtpDelimSym);
			goto Kopy;
		case sv_pageOverlap:
			dsetint((long) sess.overlap, pRtnVal);
			break;
		case sv_replacePat:
			str = bufSearch.match.replPat;
			goto Kopy;
		case sv_screenNum:
			dsetint((long) sess.pCurScrn->num, pRtnVal);
			break;
		case sv_searchDelim:
			ektos((char *)(str = workBuf), bufSearch.inpDelim, false);
			goto Kopy;
		case sv_searchPat:
			{char patBuf[bufSearch.match.patLen + OptCh_N + 1];

			if(dsetstr(makePat(patBuf, &bufSearch.match), pRtnVal) != 0)
				goto LibFail;
			}
			break;
		case sv_softTabSize:
			dsetint((long) sess.pCurScrn->softTabSize, pRtnVal);
			break;
		case sv_travJump:
			dsetint((long) sess.travJumpCols, pRtnVal);
			break;
		case sv_vertJump:
			dsetint((long) vTerm.vertJumpPct, pRtnVal);
			break;
		case sv_windLineNum:
			dsetint((long) getWindPos(sess.pCurWind), pRtnVal);
			break;
		case sv_windNum:
			dsetint((long) getWindNum(sess.pCurWind), pRtnVal);
			break;
		case sv_windSize:
			dsetint((long) sess.pCurWind->rows, pRtnVal);
			break;
		case sv_workDir:
			str = (char *) sess.pCurScrn->workDir;
			goto Kopy;
		case sv_wrapCol:
			dsetint((long) sess.pCurScrn->wrapCol, pRtnVal);
			break;
#if SanityCheck
		default:
			// Never should get here.
			return rsset(FatalError, 0, text990, "getSConstVar", varId, pSConstVar->name);
					// "%s(): Unknown ID %d for variable '%s'!"
#endif
		}
	goto Retn;
Kopy:
	if(dsetstr(str, pRtnVal) != 0)
LibFail:
		(void) libfail();
Retn:
	return sess.rtn.status;
	}

// Copy a new value to a variable, checking if old value is an array or hash in a global variable.
static int copyNewVal(Datum *pDest, Datum *pSrc, VarDesc *pVarDesc) {

	if(dtypah(pDest) && pVarDesc->type == VTyp_GConstVar)
		ahtrack(pDest, false);
#if MMDebug & Debug_UserVars
	dumpVal(pSrc, true);
#endif
	return dcpy(pDest, pSrc) != 0 ? libfail() : sess.rtn.status;
	}

// Calculate horizontal jump columns from percentage.
void calcHorzJumpCols(void) {

	if((vTerm.horzJumpCols = vTerm.horzJumpPct * term.cols / 100) == 0)
		vTerm.horzJumpCols = 1;
	}

// Check if system or global variable is an immutable constant and, if so, set error message and return true; otherwise, return
// false.
bool immutable(VarDesc *pVarDesc) {
	GConstVar *pGConstVar;
	SConstVar *pSConstVar;
	const char *name, *type;

	switch(pVarDesc->type) {
		case VTyp_GConstVar:
			pGConstVar = pVarDesc->p.pGConstVar;
			if((pGConstVar->flags & (V_Const | V_NewMint)) == V_Const) {
				name = pGConstVar->name;
				type = text146;
					// "global"
				goto ErrRtn;
				}
			break;
		case VTyp_SConstVar:
			pSConstVar = pVarDesc->p.pSConstVar;
			if(pSConstVar->flags & V_Const) {
				name = pSConstVar->name;
				type = text513;
					// "system"
ErrRtn:
				return rsset(Failure, RSTermAttr, text164, type, name);
						// "Cannot modify %s constant '~b%s~B'"
				}
		}
	return sess.rtn.status;
	}

// Validate and convert a line delimiter symbol in string form to a coded symbol and store result in *pSym.  Return status.
static int delimStrToSym(Datum *pDatum, const char *errPrefix, ushort *pSym) {
	char *str;
	ushort sym;
	static struct {
		const char *name;
		ushort sym;
		} delimStrs[] = {
			{"NL", FDelimNL},
			{"CRLF", FDelimCRLF},
			{"CR", FDelimCR},
			{"NUL", FDelimNUL}}, *pDelims, *pDelimsEnd;

	if(*(str = pDatum->str) == '\0')
		sym = FDelimUndef;
	else {
		pDelimsEnd = (pDelims = delimStrs) + elementsof(delimStrs);
		do {
			if(strcasecmp(str, pDelims->name) == 0) {
				sym = pDelims->sym;
				goto Retn;
				}
			} while(++pDelims < pDelimsEnd);

		// Unknown symbol string.
		return rsset(Failure, 0, text251, errPrefix, str);
				// "Unknown %s delimiter symbol \"%s\""
		}
Retn:
	// Found valid symbol.  Return it.
	*pSym = sym;
	return sess.rtn.status;
	}

// Set variable pointed to by pVarDesc to given value (pNewVal).  Return status.
int setVar(VarDesc *pVarDesc, Datum *pNewVal) {
	const char *str;
	Datum *pOldVal;
#if MMDebug & Debug_UserVars
	Datum newVal;
#endif

	// Error if immutable constant.
	if(immutable(pVarDesc))
		return sess.rtn.status;

#if MMDebug & Debug_UserVars
	dinit(&newVal);
	fputs("setVar(): ", logfile);
#endif
	// Set the appropriate value.
	switch(pVarDesc->type) {

		// Set a local variable.
		case VTyp_LocalVar:
			pOldVal = pVarDesc->p.pLocalVar->pVal;		// Grab pointer to old value.
#if MMDebug & Debug_UserVars
			fprintf(logfile, "local var '%s'", pVarDesc->p.pLocalVar->name);
#endif
			goto Copy;

		// Set a global constant or variable.
		case VTyp_GConstVar:
			pVarDesc->p.pGConstVar->flags &= ~V_NewMint;
			pOldVal = pVarDesc->p.pGConstVar->pVal;		// Grab pointer to old value.
#if MMDebug & Debug_UserVars
			fprintf(logfile, "global CV '%s'", pVarDesc->p.pGConstVar->name);
#endif
Copy:
			(void) copyNewVal(pOldVal, pNewVal, pVarDesc);
			break;

		// Set a system variable.
		case VTyp_SConstVar:
			{const char *suffix;
			int i1, i2;
			ExtKey extKey;
			SConstVar *pSConstVar = pVarDesc->p.pSConstVar;
			Datum *pSink;						// For throw-away return value, if any.

#if MMDebug & Debug_UserVars
			fprintf(logfile, "system CV '%s' = (value)\n", pVarDesc->p.pSConstVar->name);
#endif
			// Check for legal value types.  Array and string variables may also be nil (if V_Nil flag set), but all
			// other variables are single-type only.
			if(pSConstVar->flags & V_Int) {
				if(!isIntVal(pNewVal))
					goto BadTyp1;
				}
			else if(pSConstVar->flags & V_Char) {
				if(!isCharVal(pNewVal))
					goto BadTyp1;
				}
			else if(dtypbool(pNewVal)) {
				str = text360;
					// "Boolean"
				goto BadTyp0;
				}
			else if(disnil(pNewVal)) {
				if(pSConstVar->flags & V_Nil)
					dsetnull(pNewVal);
				else {
					str = text359;
						// "nil"
BadTyp0:
					(void) rsset(Failure, 0, text358, str);
							// "Illegal use of %s value"
					goto BadTyp1;
					}
				}
#if 0			// No mutable system variables are currently an array type (so this will never be true).
			else if(pSConstVar->flags & V_Array) {
				if(!isArrayVal(pNewVal))
					goto BadTyp1;
				}
#endif
			else if(!isStrVal(pNewVal)) {
				DFab msg;
BadTyp1:
				str = pSConstVar->name;
BadTyp2:
				if(dopentrack(&msg) != 0)
					goto LibFail;
				else if(escMsgAttr(&msg) == Success) {
					if(dputf(&msg, 0, text334, str) != 0 || dclose(&msg, FabStr) != 0)
							// ", setting variable '~b%s~B'"
						goto LibFail;
					(void) rsset(sess.rtn.status, RSForce | RSNoFormat | RSTermAttr, msg.pDatum->str);
					}
				return sess.rtn.status;
				}

			// Do specific action for referenced (mutable) variable.
			if((pSink = dnewtrack()) == NULL)
				goto LibFail;
			suffix = NULL;
			switch(pSConstVar->id) {
				case sv_autoSave:
					{int keyCount;

					if(dgetint(pNewVal) < 0 || dgetint(pNewVal) > INT_MAX)
						goto ERange1;
					keyCount = dgetint(pNewVal) > INT_MAX ? INT_MAX : dgetint(pNewVal);
					if(keyCount == 0) {

						// ASave count set to zero... turn off global mode and clear counter.
						clearGlobalMode(mode.cache[MdIdxASave]);
						sess.autoSaveTrig = sess.autoSaveCount = 0;
						}
					else {
						int diff = keyCount - sess.autoSaveTrig;
						if(diff != 0) {

							// New count > 0.  Adjust counter accordingly.
							sess.autoSaveTrig = keyCount;
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
					str = "0 => setBufFile";
					suffix = ", $bufname";
					goto XeqCmd;
				case sv_bufLineNum:
					(void) bigBufMove(pSink, DefN, NULL, BM_GotoLine, dgetint(pNewVal));
					break;
				case sv_bufname:
					str = "renameBuf $bufname,";
					goto XeqCmd;
				case sv_execPath:
					(void) setExecPath(pNewVal->str);
					break;
				case sv_fencePause:
					if(dgetint(pNewVal) < 0 || dgetint(pNewVal) > MaxFencePause)
						return rsset(Failure, 0, text12, text119, dgetint(pNewVal), 0,
							// "%s (%'ld) must be in range %d - %'ld", "Pause duration"
						 (long) MaxFencePause);
					sess.fencePause = dgetint(pNewVal);
					break;
				case sv_hardTabSize:
					if(setTabSize(dgetint(pNewVal), true) == Success)
						(void) supd_windFlags(NULL, WFHard | WFMode);
					break;
				case sv_horzJump:
					if((vTerm.horzJumpPct = dgetint(pNewVal)) < 0)
						vTerm.horzJumpPct = 0;
					else if(vTerm.horzJumpPct > JumpMax)
						vTerm.horzJumpPct = JumpMax;
					calcHorzJumpCols();
					break;
				case sv_inpDelim:
					(void) delimStrToSym(pNewVal, text46, &fileInfo.userInpDelimSym);
								// "input"
					break;
				case sv_lastKeySeq:
					// Don't allow variable to be set to a prefix key (pseudo command).
					if(stoek(&extKey, pNewVal->str) == Success) {
						KeyBind *pKeyBind = getBind(extKey);
						if(pKeyBind != NULL && pKeyBind->cmd.type == PtrPseudo)
							return rsset(Failure, RSTermAttr, text373, pSConstVar->name);
									// "Illegal value for '~b%s~B' variable"
						keyEntry.lastKeySeq = extKey;
						keyEntry.useLast = true;
						}
					break;
				case sv_lineChar:
					// Replace character at point with an 8-bit integer value.
					if(isCharVal(pNewVal)) {
						if(edelc(1, 0) != Success)
							return rsset(Failure, 0, text142, sess.cur.pBuf->bufname);
								// "Cannot change a character past end of buffer '%s'"
						(void) einsertc(1, dgetint(pNewVal));
						}
					break;
				case sv_lineCol:
					(void) setPointCol(dgetint(pNewVal));
					break;
				case sv_lineOffset:
					{long lineLen = sess.cur.pFace->point.pLine->used;
					long lineOffset = dgetint(pNewVal) < 0 ? lineLen + dgetint(pNewVal) : dgetint(pNewVal);
					if(lineOffset < 0 || lineOffset > lineLen)
						return rsset(Failure, 0, text378, dgetint(pNewVal));
							// "Line offset value %'ld out of range"
					sess.cur.pFace->point.offset = lineOffset;
					}
					break;
				case sv_lineText:
					(void) putLineText(pNewVal->str);
					break;
				case sv_maxCallDepth:
					if(dgetint(pNewVal) < 0 || dgetint(pNewVal) > INT_MAX)
						goto ERange1;
					maxCallDepth = dgetint(pNewVal);
					break;
				case sv_maxLoop:
					if(dgetint(pNewVal) < 0 || dgetint(pNewVal) > INT_MAX) {
ERange1:
						i1 = 0;
						i2 = INT_MAX;
						goto ERange2;
						}
					maxLoop = dgetint(pNewVal);
					break;
				case sv_maxPromptPct:
					if(dgetint(pNewVal) < 15 || dgetint(pNewVal) > 90) {
						i1 = 15;
						i2 = 90;
ERange2:
						return rsset(Failure, RSTermAttr, text111, pSConstVar->name, dgetint(pNewVal),
								// "~b%s~B value (%'ld) must be in range %d - %'d"
						 i1, i2);
						}
					term.maxPromptPct = dgetint(pNewVal);
					break;
				case sv_otpDelim:
					(void) delimStrToSym(pNewVal, text47, &fileInfo.userOtpDelimSym);
								// "output"
					break;
				case sv_pageOverlap:
					if(dgetint(pNewVal) < 0 || dgetint(pNewVal) > (int) (term.rows - 1) / 2)
						return rsset(Failure, 0, text12, text184, dgetint(pNewVal), 0,
								// "%s (%'ld) must be in range %d - %'ld", "Overlap"
						 (long) (term.rows - 1) / 2);
					sess.overlap = dgetint(pNewVal);
					break;
				case sv_replacePat:
					(void) newReplPat(pNewVal->str, &bufSearch.match, true);
					break;
				case sv_screenNum:
					(void) gotoScreen(dgetint(pNewVal), 0);
					break;
				case sv_searchDelim:
					if(stoek(&extKey, pNewVal->str) != Success)
						return sess.rtn.status;
					if(extKey & Prefix) {
						char keyBuf[16];

						return rsset(Failure, RSTermAttr, text341, ektos(keyBuf, extKey, true),
							// "Cannot use key sequence ~#u%s~U as %s delimiter"
						 text343);
							// "search"
						}
					bufSearch.inpDelim = extKey;
					break;
				case sv_searchPat:
					// Make copy so original is returned unmodified.
					{char workPat[strlen(pNewVal->str) + 1];
					(void) newSearchPat(strcpy(workPat, pNewVal->str), &bufSearch.match, NULL, true);
					}
					break;
				case sv_softTabSize:
					if(setTabSize(dgetint(pNewVal), false) == Success)
						(void) supd_windFlags(NULL, WFHard | WFMode);
					break;
				case sv_travJump:
					if((sess.travJumpCols = dgetint(pNewVal)) < 4)
						sess.travJumpCols = 4;
					else if(sess.travJumpCols > term.cols / 4 - 1)
						sess.travJumpCols = term.cols / 4 - 1;
					break;
				case sv_vertJump:
					if((vTerm.vertJumpPct = dgetint(pNewVal)) < 0)
						vTerm.vertJumpPct = 0;
					else if(vTerm.vertJumpPct > JumpMax)
						vTerm.vertJumpPct = JumpMax;
					break;
				case sv_windLineNum:
					(void) forwLine(pSink, dgetint(pNewVal) - getWindPos(sess.pCurWind), NULL);
					break;
				case sv_windNum:
					(void) gotoWind(pSink, dgetint(pNewVal), 0);
					break;
				case sv_windSize:
					(void) resizeWind(pSink, dgetint(pNewVal), NULL);
					break;
				case sv_workDir:
					str = "chdir";
XeqCmd:
					(void) runCmdline(pSink, str, pNewVal->str, suffix, RunQFull);
					break;
				case sv_wrapCol:
					(void) setWrapCol(dgetint(pNewVal));
					break;
#if SanityCheck
				default:
					// Never should get here.
					return rsset(FatalError, 0, text990, "setVar", pSConstVar->id, pSConstVar->name);
							// "%s(): Unknown ID %d for variable '%s'!"
#endif
				}
			}
			break;

		// Set a numeric variable argument.
		case VTyp_NumVar:
#if MMDebug & Debug_UserVars
			fprintf(logfile, "$%ld", pVarDesc->i.index);
#endif
			if(pVarDesc->i.index == 0) {		// Allow numeric assignment (only) to $0.
				if(!isIntVal(pNewVal)) {
					str = "$0";
					goto BadTyp2;
					}
				pScriptRun->n = dgetint(pNewVal);
#if MMDebug & Debug_UserVars
				dumpVal(pNewVal, true);
#endif
				}
			else
				// Numeric variable argument assignment.  Get array argument and set new value.
				(void) copyNewVal(pVarDesc->p.pArray->elements[pVarDesc->i.index - 1], pNewVal, pVarDesc);
			break;

		// Set an array element.
		case VTyp_AryElRef:
			if((pOldVal = aget(pVarDesc->p.pArray, pVarDesc->i.index, 0)) == NULL)
				goto LibFail;
			if(dtypah(pOldVal))
				ahtrack(pOldVal, false);
			(void) dcpy(pOldVal, pNewVal);
#if MMDebug & Debug_UserVars
			fprintf(logfile, "array %.8x[%ld]", (uint) pVarDesc->p.pArray, pVarDesc->i.index);
			dumpVal(pNewVal, true);
#endif
			break;

		// Set a hash node.
		case VTyp_HshNodRef:
			{HashNode *pNode = hsearch(pVarDesc->p.pHash, pVarDesc->i.pKey->str);	// Can't fail.
			if(dtypah(pOldVal = pNode->pVal))
				ahtrack(pOldVal, false);
			(void) dcpy(pOldVal, pNewVal);
#if MMDebug & Debug_UserVars
			fprintf(logfile, "hash %.8x['%s']", (uint) pVarDesc->p.pHash, pVarDesc->i.pKey->str);
			dumpVal(pNewVal, true);
#endif
			}
			break;
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Create local or global user constant or variable, given name and descriptor pointer.  Return status.
static int newUserVar(const char *var, VarDesc *pVarDesc) {
	LocalVar *pLocalVar;
	GConstVar *pGConstVar;
	HashNode *pNode;
	void *pNewVar;
	char *str;
	Symbol sym;
	const char *name = var + (*var == TokC_GlobalVar ? 1 : 0);

#if MMDebug & Debug_UserVars
	fprintf(logfile, "newUserVar(): Creating user variable '%s'...", var);
	fflush(logfile);
#endif
	// Invalid length?
	if(*var == '\0' || *name == '\0' || strlen(var) > MaxVarName)
		return rsset(Failure, 0, text280, text279, MaxVarName);
			// "%s name cannot be null or exceed %d characters", "Variable"

	// Valid variable name?
	str = (char *) name;
	if(((sym = getIdent(&str, NULL)) != s_ident && sym != s_globalConst) || *str != '\0')
		(void) rsset(Failure, 0, text447, text286, name);
			// "Invalid %s '%s'", "identifier"
	if(*var == TokC_GlobalVar)
		sym = s_globalVar;

	// Allocate new object, set its values, and add to beginning of list (if local variable) or hash (if global constant or
	// variable).
	if((pNewVar = malloc(sym == s_ident ? sizeof(LocalVar) : sizeof(GConstVar))) == NULL)
		return rsset(Panic, 0, text94, "newUserVar");
				// "%s(): Out of memory!"
	if(sym == s_ident) {
		pLocalVar = (LocalVar *) pNewVar;
		strcpy((pVarDesc->p.pLocalVar = pLocalVar)->name, var);
		pVarDesc->type = VTyp_LocalVar;
		pLocalVar->next = localVarRoot;
		localVarRoot = pLocalVar;

		// Set value of new variable to nil.
		if((pLocalVar->pVal = dnew()) == NULL)
			goto LibFail;
		}
	else {
		pVarDesc->type = VTyp_GConstVar;
		pVarDesc->p.pGConstVar = pGConstVar = (GConstVar *) pNewVar;
		if((pNode = hnewnode(&gblConstVarTable, var, NULL)) == NULL)
			goto LibFail;
		pGConstVar->name = pNode->key;
		pGConstVar->flags = isUpperCase(*var) ? V_Const | V_NewMint : 0;
		dadoptmem(pNewVar, sizeof(*pGConstVar), pNode->pVal);

		// Set value of new variable to nil.
		if((pGConstVar->pVal = dnew()) == NULL)
			goto LibFail;
		}

#if MMDebug & Debug_UserVars
	fputs(" success.\n", logfile);
#endif
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Find a named variable's type and id.
// If op is OpCreate:
//	1. Create user variable if non-existent and either (a), variable is global; or (b), executing a buffer.
//	2. Return status.
// If op is OpQuery:
//	Return true if variable is found, otherwise false.
// If op is OpDelete:
//	Return status if variable is found, otherwise error.
// In all cases,
//	Save descriptor information in *pVarDesc if pVarDesc not NULL and variable is found or created.
int vfind(const char *name, VarDesc *pVarDesc, uint op) {
	HashNode *pNode;
	LocalVar *pLocalVar;
	VarDesc varDesc;

	// Get ready.
	if(pVarDesc == NULL)
		pVarDesc = &varDesc;		// Not returned to caller.
	else
		pVarDesc->i.pKey = NULL;

	// Check lead-in character.
	if(*name == TokC_GlobalVar || isUpperCase(*name)) {
		if(*name != TokC_GlobalVar || strlen(name) > 1) {

			// Numeric variable (user routine argument reference)?
			if(is_digit(name[1])) {
				long longVal;

				// Yes, command or function running and number in range?
				if(pScriptRun != NULL && ascToLong(name + 1, 10, &longVal, true) &&
				 longVal <= pScriptRun->pArgs->used) {

					// Valid reference.  Set type and save argument number.
					pVarDesc->type = VTyp_NumVar;
					pVarDesc->i.index = longVal;
					pVarDesc->p.pArray = pScriptRun->pArgs;
					goto Found;
					}
				}
			else {
				// Not numeric variable.  Check hashes for existing global or system constant or variable.
				if((pNode = hsearch(&gblConstVarTable, name)) != NULL) {
					pVarDesc->type = VTyp_GConstVar;
					pVarDesc->p.pGConstVar = (GConstVar *) dgetmemptr(pNode->pVal);
					goto Found;
					}
				if((pNode = hsearch(&sConstVarLookup, name)) != NULL) {
					pVarDesc->type = VTyp_SConstVar;
					pVarDesc->p.pSConstVar = (SConstVar *) dgetmemptr(pNode->pVal);
					goto Found;
					}

				// Not found.  Create new one?
				if(op == OpCreate) {
					if(*name == TokC_GlobalVar)
						goto Create;
					goto CheckName;
					}
				goto VarNotFound;
				}
			}
		}
	else if(*name != '\0') {

		// Check for existing local variable.
		if((pLocalVar = findLocalVar(name)) != NULL) {
			pVarDesc->type = VTyp_LocalVar;
			pVarDesc->p.pLocalVar = pLocalVar;
			goto Found;
			}

		// Local variable not found.  Create it if requested or executing a buffer; otherwise, return an error.
		if(op != OpCreate || pScriptRun == NULL)
			goto VarNotFound;
CheckName:
		// Create variable.  Don't allow if name same as an existing command, pseudo-command, function, or alias.
		if(execFind(name, OpQuery, PtrAny, NULL))
			return rsset(Failure, RSTermAttr, text165, name, text109);
						// "Name '~b%s~B'%s", " already in use"
Create:
#if MMDebug & Debug_UserVars
		fprintf(logfile, "vfind(): calling newUserVar(%s, ...)...\n", name);
#endif
		if(newUserVar(name, pVarDesc) != Success)
			return sess.rtn.status;
Found:
		return (op == OpQuery) ? true : sess.rtn.status;
		}
VarNotFound:
	// Variable not found.
	return (op == OpQuery) ? false : rsset(Failure, 0, text52, isUpperCase(*name) ? text130 : text239, name);
							// "No such %s \"%s\""		"constant", "variable"
	}

// Derefence a variable, given descriptor, and save variable's value in *pDatum.  Return status.
int vderefv(Datum *pDatum, VarDesc *pVarDesc) {
	Datum *pValue;

#if MMDebug & Debug_UserVars
	fputs("vderefv(): dereferencing ", logfile);
#endif
	switch(pVarDesc->type) {
		case VTyp_LocalVar:
			pValue = pVarDesc->p.pLocalVar->pVal;
#if MMDebug & Debug_UserVars
			fprintf(logfile, "local var %s", pVarDesc->p.pLocalVar->name);
#endif
			break;
		case VTyp_GConstVar:
			pValue = pVarDesc->p.pGConstVar->pVal;
#if MMDebug & Debug_UserVars
			fprintf(logfile, "global CV %s", pVarDesc->p.pGConstVar->name);
#endif
			break;
		case VTyp_SConstVar:
#if MMDebug & Debug_UserVars
			fprintf(logfile, "system CV %s\n", pVarDesc->p.pSConstVar->name);
#endif
			return getSConstVar(pDatum, pVarDesc->p.pSConstVar);
		case VTyp_NumVar:
			{ArraySize index = pVarDesc->i.index;
#if MMDebug & Debug_UserVars
			fprintf(logfile, "$%ld", index);
#endif

			// Get argument value.  $0 resolves to the user routine's n argument.
			if(index == 0) {
				dsetint(pScriptRun->n, pDatum);
				goto Retn;
				}
			pValue = pVarDesc->p.pArray->elements[index - 1];
			}
			break;
		case VTyp_AryElRef:
			if((pValue = aget(pVarDesc->p.pArray, pVarDesc->i.index, 0)) == NULL)
				goto LibFail;
#if MMDebug & Debug_UserVars
			fprintf(logfile, "%.8x[%ld]", (uint) pVarDesc->p.pArray, pVarDesc->i.index);
#endif
			break;
		default:	// VTyp_HshNodRef
			{HashNode *pNode = hsearch(pVarDesc->p.pHash, pVarDesc->i.pKey->str);
#if MMDebug & Debug_UserVars
			fprintf(logfile, "%.8x[%s]", (uint) pVarDesc->p.pHash, pVarDesc->i.pKey->str);
#endif
			if(pNode == NULL) {
				dsetnil(pDatum);	// Return nil if hash key does not exist.
				goto Retn;
				}
			pValue = pNode->pVal;
			}
		}

#if MMDebug & Debug_UserVars
	dumpVal(pValue, true);
#endif
	// Copy value.
	if(dcpy(pDatum, pValue) != 0)
LibFail:
		return libfail();
Retn:
	return sess.rtn.status;
	}

// Derefence a variable, given name, and save variable's value in *pDatum.  Return status.
int vderefn(Datum *pDatum, const char *name) {
	VarDesc varDesc;

	// Find and dereference variable.
	if(vfind(name, &varDesc, OpDelete) == Success)
		(void) vderefv(pDatum, &varDesc);

	return sess.rtn.status;
	}

// Delete one or more constants and/or variables.  Return status.
int delConstVar(Datum *pRtnVal, long n, Datum **args) {
	VarDesc varDesc;
	char *name;
	bool isConst = false;
	int count = 0;

	if(sess.opFlags & OpEval)
		dsetint(0L, pRtnVal);

	// If interactive, get one constant or variable name from user.
	if(!(sess.opFlags & OpScript)) {
		if(termInp(pRtnVal, text533, ArgNotNull1 | ArgNil1, TermC_AllConstVar, NULL) != Success || disnil(pRtnVal))
				// "Delete constant or variable"
			return sess.rtn.status;
		name = pRtnVal->str;
		goto Lookup;
		}

	// Script mode: get name(s) to delete.
	do {
		if(!haveSym(s_globalConst, false) && !haveSym(s_globalVar, false) && !haveSym(s_numVar, false) &&
		 !haveSym(s_ident, true))
			return sess.rtn.status;
		if(sess.opFlags & OpEval) {
			name = pLastParse->tok.str;
Lookup:
			if(!vfind(name, &varDesc, OpQuery))
				(void) rsset(Success, RSNoWrap,  text52, text534, name);
							// "No such %s \"%s\"", "constant or variable"
			else {
				// Nuke it.
				if(vdelete(&varDesc) != Success) {

					// Don't cause a running script to fail on Failure status.
					rsunfail();
					continue;
					}
				isConst = isUpperCase(*name);
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
		(void) rsset(Success, RSHigh, "%s %s", isConst ? text536 : text279, text10);
							// "Constant", "Variable", "deleted"
	return sess.rtn.status;
	}

// Simulate dputd() CXL library function with terminal attribute escaping; that is, put a datum to a fabrication object with
// all AttrSpecBegin (~) characters doubled (~~).  Return same codes as dputd().
int dtofabattr(const Datum *pDatum, DFab *pFab, const char *delim, uint cflags) {
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
static int ctofab(Datum *pDatum, DFab *pFab) {
	short c = dgetint(pDatum);

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

// Get the value of a system constant or variable and store in a fabrication object in "show" form.  Return status.
int scvtofab(SConstVar *pSConstVar, bool escTermAttr, DFab *pFab) {
	int rtnCode;
	Datum *pDatum;
	Point *pPoint = &sess.cur.pFace->point;

	// Get value.  Return null for RegionText if no region defined or empty; use single-quote form for $replacePat
	// and $searchPat; use char-lit form for character variables.
	if((pDatum = dnewtrack()) == NULL)
		goto LibFail;
	if(pSConstVar->id == sv_RegionText || (pSConstVar->id == sv_lineText && pPoint->pLine->used > term.cols * 2)) {

		// Cap size of region or current line in case it's huge -- it will be truncated when displayed anyway.
		if(pSConstVar->id == sv_lineText) {
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
	else if(getSConstVar(pDatum, pSConstVar) != Success)
		return sess.rtn.status;

	// Have system constant or variable value in *pDatum.  Convert it to display form.
	if(pSConstVar->flags & V_Char)
		return ctofab(pDatum, pFab);
	int (*func)(const Datum *, DFab *, const char *, uint) = escTermAttr ? dtofabattr : dputd;
	if((rtnCode = func(pDatum, pFab, ", ", pSConstVar->id == sv_replacePat || pSConstVar->id == sv_searchPat ?
	 CvtVizStr : CvtLang | CvtThouSep)) >= 0)
		return endless(rtnCode);
LibFail:
	return libfail();
	}

#if MMDebug & Debug_Datum
// Dump all user variables to the log file.
void dumpVars(void) {
	HashEach each;
	HashNode *pNode;
	LocalVar *pLocalVar;
	GConstVar *pGConstVar;
	int i = 0;

	do {
		fprintf(logfile, "%s VARIABLES\n", i == 0 ? "GLOBAL" : "LOCAL");
		if(i == 0) {
			heachinit(&each, &gblConstVarTable);
			while((pNode = heach(&each)) != NULL) {
				pGConstVar = (GConstVar *) dgetmemptr(pNode->pVal);
				ddump(pGConstVar->pVal, pGConstVar->name);
				}
			}
		else {
			pLocalVar = localVarRoot;
			do {
				ddump(pLocalVar->pVal, pLocalVar->name);
				} while((pLocalVar = pLocalVar->next) != NULL);
			}
		} while(++i < 2);
	}
#endif

// Set a variable interactively ("let" command).  Evaluate value as an expression if n argument.  *Interactive only*
int let(Datum *pRtnVal, long n, Datum **args) {
	VarDesc varDesc;			// Variable descriptor.
	short delimChar;
	uint argFlags, ctrlFlags;
	long longVal;
	Datum *pDatum;
	DFab fab;

	// First, get the variable to set.
	if((pDatum = dnewtrack()) == NULL)
		return libfail();
	if(termInp(pDatum, text51, ArgNil1, TermC_MutVar, NULL) != Success || disnil(pDatum))
			// "Assign variable"
		goto Retn;

	// Find variable.
	if(vfind(pDatum->str, &varDesc, OpCreate) != Success)
		goto Retn;

	// Error if a constant.
	if(immutable(&varDesc))
		return sess.rtn.status;

	// Build prompt with old value.
	if(n == DefN) {
		delimChar = (varDesc.type == VTyp_SConstVar && varDesc.p.pSConstVar->flags & V_EscDelim) ?
		 (Ctrl | '[') : RtnKey;
		argFlags = ArgNil1;
		ctrlFlags = (varDesc.type != VTyp_SConstVar || !(varDesc.p.pSConstVar->flags & (V_Char | V_GetKey |
		 V_GetKeySeq))) ? 0 : varDesc.p.pSConstVar->flags & V_Char ? Term_OneChar :
		 varDesc.p.pSConstVar->flags & V_GetKeySeq ? Term_OneKeySeq : Term_OneKey;
		}
	else {
		delimChar = RtnKey;
		argFlags = ArgNotNull1;
		ctrlFlags = 0;
		}
	if(dopenwith(&fab, pRtnVal, FabClear) != 0 || dputs(text297, &fab, 0) != 0)
						// "Current value: "
		goto LibFail;
	if(varDesc.type == VTyp_SConstVar) {
		if(varDesc.p.pSConstVar->flags & (V_GetKey | V_GetKeySeq)) {
			if(getSConstVar(pDatum, varDesc.p.pSConstVar) != Success)
				goto Retn;
			if(dputf(&fab, 0, "~#u%s~U", pDatum->str) != 0)
				goto LibFail;
			ctrlFlags |= Term_Attr;
			}
		else if(scvtofab(varDesc.p.pSConstVar, false, &fab) != Success)
			goto Retn;
		}
	else if(dputd(varDesc.p.pGConstVar->pVal, &fab, NULL, CvtLang | CvtThouSep) < 0)
		goto LibFail;

	// Add "new value" type to prompt.
	if(dputs(text283, &fab, 0) != 0)
			// "; new value"
		goto LibFail;
	if(n != DefN) {
		if(dputs(text301, &fab, 0) != 0)
				// " (expression)"
			goto LibFail;
		}
	else if(varDesc.type == VTyp_SConstVar && varDesc.p.pSConstVar->flags & (V_Char | V_GetKey | V_GetKeySeq)) {
		if(dputs(varDesc.p.pSConstVar->flags & V_Char ? text349 : text76, &fab, 0) != 0)
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

	// Evaluate result as an expression if requested and convert a numeric literal to an integer.  Type checking is done in
	// setVar() so no need to do it here.
	if(n != DefN) {
		if(execExprStmt(pRtnVal, pDatum->str, 0, NULL) != Success)
			goto Retn;
		}
	else if(dtypstr(pDatum) && (varDesc.type == VTyp_GConstVar || (varDesc.type == VTyp_SConstVar &&
	 (varDesc.p.pSConstVar->flags & V_Int))) && ascToLong(pDatum->str, 0, &longVal, true))
		dsetint(longVal, pRtnVal);
	else
		dxfer(pRtnVal, pDatum);

	// Set variable to value in pRtnVal and return.
#if MMDebug & Debug_Datum
	ddump(pRtnVal, "let(): Setting and returning value...");
	(void) setVar(&varDesc, pRtnVal);
	dumpVars();
#else
	(void) setVar(&varDesc, pRtnVal);
#endif
Retn:
	return sess.rtn.status;
	}

// Convert an array element reference node to a VarDesc object and check if referenced element exists.  If not, create it if
// OpCreate flag is set; otherwise, set an error.  Return status.
int cvtAryElRef(VarDesc *pVarDesc, ExprNode *pNode, uint oflags) {

	pVarDesc->type = VTyp_AryElRef;
	pVarDesc->i.index = pNode->i.index;
	pVarDesc->p.pArray = dgetarrayptr(pNode->pVal);
	if(aget(pVarDesc->p.pArray, pVarDesc->i.index, oflags & OpCreate ? OpGrow : 0) == NULL)
		(void) libfail();
	return sess.rtn.status;
	}

// Convert a hash reference node to a VarDesc object and optionally, check if referenced node exists per oflags:
//	OpQuery		Do nothing (non-existent key returns nil value).
//	OpCreate	Create node if key does not exist.
//	OpUpdate	Set an error if key does not exist.
int cvtHshNodRef(VarDesc *pVarDesc, ExprNode *pNode, uint oflags) {

	pVarDesc->type = VTyp_HshNodRef;
	pVarDesc->i.pKey = pNode->i.pKey;
	pVarDesc->p.pHash = dgethashptr(pNode->pVal);
	if(!(oflags & OpQuery))
		if(hsearch(pVarDesc->p.pHash, pVarDesc->i.pKey->str) == NULL) {
			if(oflags & OpUpdate)
				return rsset(Failure, 0, text52, text507, pVarDesc->i.pKey->str);
						// "No such %s \"%s\"", "hash key"

			// OpCreate: create node.
			if(hnewnode(pVarDesc->p.pHash, pVarDesc->i.pKey->str, NULL) == NULL)
				(void) libfail();
			}
	return sess.rtn.status;
	}

// Increment or decrement a variable or array/hash reference, given node pointer, "incr" flag, and "pre" flag.  Set node to
// result and return status.
int bumpVar(ExprNode *pNode, bool incr, bool pre) {
	VarDesc varDesc;
	long longVal;
	Datum *pDatum;

	if(pNode->flags & (EN_AryElRef | EN_HshNodRef)) {
		int (*cvtRef)(VarDesc *pVarDesc, ExprNode *pNode, uint oflags) = pNode->flags & EN_AryElRef ?
		 cvtAryElRef : cvtHshNodRef;
		if(cvtRef(&varDesc, pNode, OpUpdate) != Success)	// Get array element or hash node...
			return sess.rtn.status;
		if(!isIntVar(&varDesc))					// and make sure it's an integer.
			return pNode->flags & EN_AryElRef ? rsset(Failure, 0, text370, varDesc.i.index) :
								// "Array element %'ld not an integer";
			 rsset(Failure, 0, text508, varDesc.i.pKey->str);
				// "Hash value for key \"%s\" not an integer";
		}
	else {
		if(vfind(pNode->pVal->str, &varDesc, OpDelete) != Success)	// Or find variable...
			return sess.rtn.status;
		if(!isIntVar(&varDesc))					// and make sure it's an integer.
			return rsset(Failure, RSTermAttr, text212, pNode->pVal->str);
				// "Variable '~b%s~B' not an integer"
		}
	if((pDatum = dnewtrack()) == NULL)
		return libfail();
	if(vderefv(pDatum, &varDesc) != Success)			// Dereference variable...
		return sess.rtn.status;
	longVal = dgetint(pDatum) + (incr ? 1 : -1);			// compute new value of variable...
	dsetint(pre ? longVal : dgetint(pDatum), pNode->pVal);		// set result to pre or post value...
	dsetint(longVal, pDatum);					// set new variable value in a Datum object...
	return setVar(&varDesc, pDatum);				// and update variable.
	}
