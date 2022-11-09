// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// help.c	Help and info functions for MightEMacs.
//
// This file contains routines for all the informational commands and functions.

// Make selected global definitions local.
#define HelpData
#include "std.h"

#include <sys/stat.h>
#include <sys/utsname.h>
#include "cxl/lib.h"

#include "bind.h"
#include "exec.h"
#include "cmd.h"
#include "file.h"
#include "search.h"
#include "var.h"

// System and session information table.
typedef struct {
	char *keyword;
	char *value;
	CmdFuncId id;
	} InfoTable;

// Control parameters and flags for "show" routines.
typedef struct {
	int origNArg;			// Original n argument.
	Buffer *pRptBuf;		// Buffer to put list into.
	DFab rpt;			// Report object.
	void *pItem;			// Pointer to current item in list.
	Datum name;			// Name of CFA or variable plus arguments.
	Datum value;			// Item value (or nil).
	const char *descrip;		// Item description (or NULL);
	Datum pat;			// Match pattern for apropos.
	Match match;			// Match record for apropos.
	} ShowCtrl;

// Request types and processing flags used by showBuild().
#define SHReqNext	1		// Find next item and return its name.
#define SHReqSynop	2		// Save item's synopsis (arguments and description text) in control record.
#define SHReqValue	3		// Write item value to fabrication object.

#define SHSkipLine	0x0001		// Skip line before this section.
#define SHSepLine	0x0002		// Insert separator line between items.
#define SHExact		0x0004		// Apropos pattern must match exactly.
#define SHNoDesc	0x0008		// Item description is not used.

// Local data for showCommands() and showFunctions().
static bool selSystem, selUser, selAll;
static Option cmdFuncOptions[] = {
	{"^System", "^Sys", 0, .u.ptr = (void *) &selSystem},
	{"^User", NULL, 0, .u.ptr = (void *) &selUser},
	{"^All", NULL, 0, .u.ptr = (void *) &selAll},
	{NULL, NULL, 0, 0}};
static OptHdr cmdFuncOptHdr = {
	0, text410, false, cmdFuncOptions};
		// "command option"

#ifndef OSName
// Get OS name for getInfo function and return it in pRtnVal.  Return status.
static int getOS(Datum *pRtnVal) {
	static const char myName[] = "getOS";
	static char *osName = NULL;
	char *name;

	// Get uname() info if first call.
	if((name = osName) == NULL) {
		struct utsname utsname;
		struct OSInfo {
			char *key;
			char *name;
			} *pOSInfo, *pOSInfoEnd;
		static struct OSInfo osTable[] = {
			{VerKey_MacOS, OSName_MacOS},
			{VerKey_Debian, OSName_Debian},
			{VerKey_Ubuntu, OSName_Ubuntu}};

		if(uname(&utsname) != 0)
			return sysCallError(myName, "uname", false);

		// OS name in "version" string?
		pOSInfoEnd = (pOSInfo = osTable) + elementsof(osTable);
		do {
			if(strcasestr(utsname.version, pOSInfo->key) != NULL) {
				name = pOSInfo->name;
				goto Save;
				}
			} while(++pOSInfo < pOSInfoEnd);

		// Nope.  CentOS or Red Hat release file exist?
		struct stat s;

		if(stat(CentOS_Release, &s) == 0) {
			name = OSName_CentOS;
			goto Save;
			}
		if(stat(RedHat_Release, &s) == 0) {
			name = OSName_RedHat;
			goto Save;
			}

		// Nope.  Use system name from uname() call.
		name = utsname.sysname;
Save:
		// Save OS name on heap for future calls, if any.
		if((osName = (char *) malloc(strlen(name) + 1)) == NULL)
			return rsset(Panic, 0, text94, myName);
				// "%s(): Out of memory!"
		strcpy(osName, name);
		}

	return dsetstr(name, pRtnVal) != 0 ? libfail() : sess.rtn.status;
	}
#endif

// Store buffer information into given, pre-allocated array.  Return status.
static int binfo(Array *pArray, Buffer *pBuf, bool extended) {
	long byteCount;
	int lineCount;
	Datum **ppArrayEl = pArray->elements;

	// Store buffer name, filename, and home directory.
	if(dsetstr(pBuf->bufname, *ppArrayEl++) != 0)
		goto LibFail;
	if(pBuf->filename != NULL && dsetstr(pBuf->filename, *ppArrayEl) != 0)
		goto LibFail;
	++ppArrayEl;
	if(pBuf->saveDir != NULL && dsetstr(pBuf->saveDir, *ppArrayEl) != 0)
		goto LibFail;
	++ppArrayEl;

	// Store buffer size in bytes and lines.
	byteCount = bufLength(pBuf, &lineCount);
	dsetint(byteCount, *ppArrayEl++);
	dsetint(lineCount, *ppArrayEl++);

	// Store buffer attributes and modes if requested.
	if(extended) {
		Array *pArray1;
		Datum *pArrayEl;
		Option *pOpt;

		if(makeArray(*ppArrayEl++, 0, &pArray1) != Success)
			return sess.rtn.status;

		// Get buffer attributes.
		pOpt = bufAttrTable;
		do {
			if(pBuf->flags & pOpt->u.value)
				if((pArrayEl = aget(pArray1, pArray1->used, AOpGrow)) == NULL ||
				 dsetstr(*pOpt->keyword == '^' ? pOpt->keyword + 1 : pOpt->keyword, pArrayEl) != 0)
					goto LibFail;
			} while((++pOpt)->keyword != NULL);

		// Get buffer modes.
		(void) getModes(*ppArrayEl, pBuf);
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Build array of form [bufname], [bufname, filename, homeDir, bytes, lines], or [bufname, filename, homeDir, bytes, lines,
// [buf-attr, ...], [buf-mode, ...]] and return it in pRtnVal.  Return status.
int bufInfo(Datum *pRtnVal, int n, Datum **args) {
	Array *pArray0, *pArray1, *pArray;
	Datum *pArrayEl;
	Datum *pBufname = args[0];
	Buffer *pBuf;
	int count;
	ushort bufFlags;
	ushort arySize = 5;
	ushort optFlags = 0;
	bool selVisible = false;
	static Option options[] = {
		{"^Visible", NULL, 0, 0},
		{"^Hidden", NULL, 0, BFHidden},
		{"^Command", NULL, 0, BFCommand},
		{"^Function", NULL, 0, BFFunc},
		{"^Brief", NULL, 0, 0},
		{"^Extended", NULL, 0, 0},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text318, false, options};
			// "attribute keyword"

	// Get options and set buffer flags.
	if(makeArray(pRtnVal, 0, &pArray0) != Success)
		return sess.rtn.status;
	if(args[1] != NULL) {
		if(parseOpts(&optHdr, NULL, args[1], NULL) != Success)
			return sess.rtn.status;
		selVisible = options[0].ctrlFlags & OptSelected;
		if((optFlags = getFlagOpts(options)) == 0 && !selVisible && disnil(pBufname))
			return rsset(Failure, 0, text455, optHdr.type);
				// "Missing required %s"
		if((options[4].ctrlFlags & OptSelected) && (options[5].ctrlFlags & OptSelected))
			return rsset(Failure, 0, text454, text451);
				// "Conflicting %ss", "function option"
		if(options[4].ctrlFlags & OptSelected)
			arySize = 1;
		else if(options[5].ctrlFlags & OptSelected)
			arySize = 7;
		}

	// If single buffer requested, find it.
	if(!disnil(pBufname)) {
		if((pBuf = bsrch(pBufname->str, NULL)) == NULL)
			return rsset(Failure, 0, text118, pBufname->str);
				// "No such buffer '%s'"
		count = 1;
		goto DoOneBuf;
		}

	// Cycle through buffers and make list in pArray0.
	pArray = &bufTable;
	count = 0;
	while((pArrayEl = aeach(&pArray)) != NULL) {
		pBuf = bufPtr(pArrayEl);
DoOneBuf:
		// Select buffer if single buffer requested or any of the following are true:
		//  1. Not hidden or user command/function and "Visible" option specified.
		//  2. User command and "Command" option specified.
		//  3. User function and "Function" option specified.
		//  4. Hidden and "Hidden" option specified.
		bufFlags = (pBuf->flags & (BFHidden | BFCommand | BFFunc));
		if(count == 1 || (!bufFlags && selVisible) ||
		 ((bufFlags & BFCommand) && (optFlags & BFCommand)) ||
		 ((bufFlags & BFFunc) && (optFlags & BFFunc)) ||
		 (bufFlags == BFHidden && (optFlags & BFHidden))) {

			// Create array for this buffer if needed.
			if(count == 1) {
				// Single buffer.  Use pArray0 and extend it to the correct size.
				if((pArrayEl = aget(pArray1 = pArray0, arySize - 1, AOpGrow)) == NULL)
					goto LibFail;
				}
			else {
				// Multiple buffers.  If not "Brief", create an array of the correct size.
				if((pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
					goto LibFail;
				if(arySize > 1) {
					if((pArray1 = anew(arySize, NULL)) == NULL)
						goto LibFail;
					agStash(pArrayEl, pArray1);
					}
				}

			// Store buffer information into array.
			if(arySize == 1) {
				if(dsetstr(pBuf->bufname, pArrayEl) != 0)
					goto LibFail;
				}
			else if(binfo(pArray1, pBuf, arySize == 7) != Success)
				return sess.rtn.status;
			if(count == 1)
				break;
		 	}
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Build array for getInfo or bufInfo function and return it in pRtnVal.  Return status.
static int buildArray(Datum *pRtnVal, int n, CmdFuncId id) {
	Array *pArray0, *pArray1, *pArray;
	Datum *pArrayEl, **ppArrayEl;
	EScreen *pScrn;

	if(makeArray(pRtnVal, 0, &pArray0) == Success)
		switch(id) {
			case cf_showColors:;		// [[item-kw, [foreground-color, background-color]], ...] or
							// [colors, pairs]
				if(n == INT_MIN) {
					ItemColor *pItemColor, *pItemColorEnd;

					// Cycle through color items and make list.  For each item, create array in pArray1 and
					// push onto pArray0.  Also create color-pair array in pArray and store in pArray1 if
					// applicable.
					pItemColorEnd = (pItemColor = term.itemColors) +
					 sizeof(term.itemColors) / sizeof(ItemColor);
					do {
						if((pArray1 = anew(2, NULL)) == NULL)
							goto LibFail;
						if(pItemColor->colors[0] < -1)
							dsetnil(pArray1->elements[1]);
						else {
							if((pArray = anew(2, NULL)) == NULL)
								goto LibFail;
							dsetint(pItemColor->colors[0], pArray->elements[0]);
							dsetint(pItemColor->colors[1], pArray->elements[1]);
							agStash(pArray1->elements[1], pArray);
							}
						if(dsetstr(pItemColor->name, pArray1->elements[0]) != 0 ||
						 (pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
							goto LibFail;
						agStash(pArrayEl, pArray1);
						} while(++pItemColor < pItemColorEnd);
					}
				else {
					// Create two-element array.
					if((pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
						goto LibFail;
					dsetint(term.maxColor, pArrayEl);
					if((pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
						goto LibFail;
					dsetint(term.maxWorkPair, pArrayEl);
					}
				break;
			case cf_setDefault:;		// [[param-name, value], ...]
				int *pDef = defParams;
				Option *pOpt = defOptions;

				// Cycle through defaults table and make list.  For each parameter, create array in pArray1 and
				// push onto pArray0.
				do {
					if((pArray1 = anew(2, NULL)) == NULL)
						goto LibFail;
					if(dsetstr(pOpt->keyword, pArray1->elements[0]) != 0 ||
					 (pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
						goto LibFail;
					dsetint(*pDef++, pArray1->elements[1]);
					agStash(pArrayEl, pArray1);
					} while((++pOpt)->keyword != NULL);
				break;
			case cf_showHooks:;		// [[hook-name, function-name], ...]
				HookRec *pHookRec = hookTable;

				// Cycle through hook table and make list.  For each hook, create array in pArray1 and push onto
				// pArray0.
				do {
					if((pArray1 = anew(2, NULL)) == NULL)
						goto LibFail;
					if(dsetstr(pHookRec->name, pArray1->elements[0]) != 0 ||
					 (pHookRec->func.type != PtrNull && dsetstr(hookFuncName(&pHookRec->func),
					 pArray1->elements[1]) != 0) ||
					 (pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
						goto LibFail;
					agStash(pArrayEl, pArray1);
					} while((++pHookRec)->name != NULL);
				break;
			case cf_showModes:;		// [[mode-name, group-name, user?, global?, hidden?, scope-lock?,
							// active?], ...]
				ModeSpec *pModeSpec;
				static ushort modeFlags[] = {MdUser, MdGlobal, MdHidden, MdLocked, 0};
				ushort *pFlag;

				// Cycle through mode table and make list.  For each mode, create array in pArray1 and push onto
				// pArray0.
				pArray = &modeInfo.modeTable;
				while((pArrayEl = aeach(&pArray)) != NULL) {
					pModeSpec = modePtr(pArrayEl);
					if((pArray1 = anew(7, NULL)) == NULL)
						goto LibFail;
					ppArrayEl = pArray1->elements;
					if(dsetstr(pModeSpec->name, *ppArrayEl++) != 0 ||
					 (pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
						goto LibFail;
					if(pModeSpec->pModeGrp == NULL)
						++ppArrayEl;
					else if(dsetstr(pModeSpec->pModeGrp->name, *ppArrayEl++) != 0)
						goto LibFail;
					pFlag = modeFlags;
					do {
						dsetbool(pModeSpec->flags & *pFlag++, *ppArrayEl++);
						} while(*pFlag != 0);
					dsetbool((pModeSpec->flags & MdGlobal) ? (pModeSpec->flags & MdEnabled) :
					 isBufModeSet(sess.cur.pBuf, pModeSpec), *ppArrayEl++);
					agStash(pArrayEl, pArray1);
					}
				break;
			case cf_showScreens:;		// [[screen-num, wind-count, work-dir], ...]

				// Cycle through screens and make list.  For each screen, create array in pArray1 and push onto
				// pArray0.
				pScrn = sess.scrnHead;
				do {
					if((pArray1 = anew(3, NULL)) == NULL)
						goto LibFail;
					ppArrayEl = pArray1->elements;
					dsetint((long) pScrn->num, *ppArrayEl++);
					dsetint((long) getWindCount(pScrn, NULL), *ppArrayEl++);
					if(dsetstr(pScrn->workDir, *ppArrayEl) != 0 ||
					 (pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
						goto LibFail;
					agStash(pArrayEl, pArray1);
					} while((pScrn = pScrn->next) != NULL);
				break;
			default:;			// [[windNum, bufname], ...] or [[screenNum, [windNum, bufname], ...]]
				EWindow *pWind;
				long windNum;

				// Cycle through screens and make list.  For each window, create array in pArray1 and push onto
				// pArray0.
				pScrn = sess.scrnHead;
				do {
					if(pScrn->num == sess.cur.pScrn->num || n != INT_MIN) {

						// In all windows...
						windNum = 0;
						pWind = pScrn->windHead;
						do {
							++windNum;
							if((pArray1 = anew(n == INT_MIN ? 2 : 3, NULL)) == NULL)
								goto LibFail;
							ppArrayEl = pArray1->elements;
							if(n != INT_MIN)
								dsetint((long) pScrn->num, *ppArrayEl++);
							dsetint(windNum, *ppArrayEl++);
							if(dsetstr(pWind->pBuf->bufname, *ppArrayEl) != 0 ||
							 (pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
								goto LibFail;
							agStash(pArrayEl, pArray1);
							} while((pWind = pWind->next) != NULL);
						}
					} while((pScrn = pScrn->next) != NULL);
			}

	// Return results.
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// binSearch() helper function for returning a getInfo keyword, given table (array) and index.
static const char *getInfoKeyword(const void *table, ssize_t i) {

	return ((InfoTable *) table)[i].keyword;
	}

// Get informational item per keyword argument.  Return status.
int getInfo(Datum *pRtnVal, int n, Datum **args) {
	static InfoTable infoTable[] = {
		{"colors", NULL, cf_showColors},	// [colors, pairs]
		{"defaults", NULL, cf_setDefault},	// [[parm-name, value], ...]
		{"editor", Myself, -1},
		{"hooks", NULL, cf_showHooks},		// [[hook-name, function-name], ...]
		{"language", Language, -1},
		{"modes", NULL, cf_showModes}, // [[mode-name, group-name, user?, global?, visible?, scope-lock?, active?], ...]
#ifdef OSName
		{"os", OSName, -1},
#else
		{"os", NULL, -1},
#endif
		{"screens", NULL, cf_showScreens},	// [[screen-num, wind-count, work-dir], ...]
		{"version", Version, -1},
		{"windows", NULL, -1}};			// [[windNum, bufname], ...] or [[screenNum, windNum, bufname], ...]
	ssize_t i;
	InfoTable *pInfoTable;
	char *keyword = args[0]->str;

	// Search table for a match.
	if(binSearch(keyword, (const void *) infoTable, elementsof(infoTable), strcasecmp, getInfoKeyword, &i)) {

		// Match found.  Check keyword type.
		pInfoTable = infoTable + i;
		if(pInfoTable->value != NULL)
			return dsetstr(pInfoTable->value, pRtnVal) != 0 ? libfail() : sess.rtn.status;
#ifndef OSName
		if(strcmp(pInfoTable->keyword, "os") == 0)
			return getOS(pRtnVal);
#endif
		// Not a string constant value.  Build array and return it.
		return buildArray(pRtnVal, n, pInfoTable->id);
		}

	// Match not found.
	return rsset(Failure, 0, text447, text450, keyword);
			// "Invalid %s '%s'", "type keyword"
	}

// If default n, display the current line, column, and character position of the point in the current buffer, the fraction of
// the text that is before the point, and the character that is at point (in printable form and hex).  If n is not the
// default, display the point column and the character at point only.  The displayed column is not the current column,
// but the column on an infinite-width display.  *Interactive only*
int showPoint(Datum *pRtnVal, int n, Datum **args) {
	Line *pLine;
	ulong curLine = 1;
	ulong numChars = 0;		// # of chars in file.
	ulong numLines = 0;		// # of lines in file.
	ulong preChars = 0;		// # chars preceding point.
	ulong preLineCount = 0;		// # lines preceding point.
	short curChar = '\0';		// Character at point.
	double ratio = 0.0;
	int col = 0;
	int endCol = 0;			// Column pos/end of current line.
	Point workPoint;
	char *info;
	Point *pPoint = &sess.cur.pFace->point;
	char *str, workBuf1[32], workBuf2[32];

	str = strcpy(workBuf2, "0.0");
	if(!bempty(NULL)) {
		if(n == INT_MIN) {

			// Start at the beginning of the buffer.
			pLine = sess.cur.pBuf->pFirstLine;
			curChar = '\0';

			// Count characters and lines.
			numChars = numLines = 0;
			while(pLine->next != NULL || pLine->used > 0) {

				// If we are on the current line, save "pre-point" stats.
				if(pLine == pPoint->pLine) {
					curLine = (preLineCount = numLines) + 1;
					preChars = numChars + pPoint->offset;
					if(pLine->next != NULL || pPoint->offset < pLine->used)
						curChar = (pPoint->offset < pLine->used) ? pLine->text[pPoint->offset] : '\n';
					}

				// On to the next line.
				++numLines;
				numChars += pLine->used + (pLine->next == NULL ? 0 : 1);
				if((pLine = pLine->next) == NULL)
					break;
				}

			// If point is at end of buffer, save "pre-point" stats.
			if(bufEnd(pPoint)) {
				curLine = (preLineCount = numLines) + (pPoint->pLine->used == 0 ? 1 : 0);
				preChars = numChars;
				}

			ratio = 0.0;				// Ratio before point.
			if(numChars > 0)
				ratio = (double) preChars / numChars * 100.0;
			sprintf(str = workBuf2, "%.1f", ratio);
			if(numChars > 0) {

				// Fix rounding errors at buffer boundaries.
				if(preChars > 0 && strcmp(str, "0.0") == 0)
					str = "0.1";
				else if(preChars < numChars && strcmp(str, "100.0") == 0)
					str = "99.9";
				}
			}
		else if(!bempty(NULL) && (pPoint->pLine->next != NULL || pPoint->offset < pPoint->pLine->used))
			curChar = (pPoint->offset == pPoint->pLine->used) ? '\n' : pPoint->pLine->text[pPoint->offset];

		// Get real column and end-of-line column.
		col = getCol(NULL, false);
		workPoint.pLine = pPoint->pLine;
		workPoint.offset = pPoint->pLine->used;
		endCol = getCol(&workPoint, false);
		}

	// Summarize and report the info.
	if(curChar >= ' ' && curChar < 0x7F)
		sprintf(workBuf1, "'%c' 0x%.2hX", (int) curChar, curChar);
	else
		sprintf(workBuf1, "0x%.2hX", curChar);
	if((n == INT_MIN ? asprintf(&info, text60, curLine, numLines, col, endCol, preChars, numChars, str, workBuf1) :
			// "~bLine:~B %lu of %lu, ~bCol:~B %d of %d, ~bByte:~B %lu of %lu (%s%%); ~bchar~B = %s"
	 asprintf(&info, text340, col, endCol, workBuf1)) < 0)
			// "~bCol:~B %d of %d; ~bchar~B = %s"
		return rsset(Panic, 0, text94, "showPoint");
			// "%s(): Out of memory!"
	(void) mlputs(MLHome | MLTermAttr | MLFlush, info);
	free((void *) info);
	return sess.rtn.status;
	}

// Determine if an object is defined, given op and object.  If op is "Name", set *pRtnVal to result: "alias", "buffer", "system
// command", "pseudo command", "system function", "user command", "user function", "variable", or nil; if op is "Mode", set
// *pRtnVal to result: "buffer" or "global"; otherwise, set *pRtnVal to true if mark is defined and active in current buffer (op
// "Mark") or mode group exists (op "ModeGroup"), otherwise false.
int definedQ(Datum *pRtnVal, int n, Datum **args) {
	char *op = args[0]->str;
	Datum *pObj = args[1];
	bool activeMark = false;
	const char *result = NULL;
	UnivPtr univ;

	// Mark?
	if(strcasecmp(op, "mark") == 0 || (activeMark = strcasecmp(op, "activemark") == 0)) {
		if(isIntVal(pObj) && validMark(pObj)) {
			Mark *pMark;

			dsetbool(findBufMark(pObj->u.intNum, &pMark, activeMark ? MKQuery | MKViz : MKQuery) == Success &&
			 pMark != NULL, pRtnVal);
			}
		goto Retn;
		}

	if(!isStrVal(pObj))
		goto Retn;

	// Mode?
	if(strcasecmp(op, "mode") == 0) {
		ModeSpec *pModeSpec = msrch(pObj->str, NULL);
		if(pModeSpec != NULL)
			result = (pModeSpec->flags & MdGlobal) ? text146 : text83;
							// "global", "buffer"
		goto Set;
		}

	// Mode group?
	if(strcasecmp(op, "modegroup") == 0) {
		dsetbool(mgsrch(pObj->str, NULL, NULL), pRtnVal);
		goto Retn;
		}

	// Unknown op?
	if(strcasecmp(op, "name") != 0) {
		(void) rsset(Failure, 0, text447, text450, op);
				// "Invalid %s '%s'", "type keyword"
		goto Retn;
		}

	// Variable?
	if(findVar(pObj->str, NULL, OpQuery))
		result = text292;
			// "variable"

	// Command, pseudo-command, function, or alias?
	else if(execFind(pObj->str, OpQuery, PtrAny, &univ))
		switch(univ.type) {
			case PtrSysCmd:
				result = text414;
					// "system command"
				break;
			case PtrPseudo:
				result = text333;
					// "pseudo command"
				break;
			case PtrUserCmd:
				result = text417;
					// "user command"
				break;
			case PtrSysFunc:
				result = text336;
					// "system function"
				break;
			case PtrUserFunc:
				result = text418;
					// "user function"
				break;
			default:	// PtrAlias
				result = text127;
					// "alias"
			}
	else if(bsrch(pObj->str, NULL) != NULL)
		result = text83;
			// "buffer"
Set:
	// Return result.
	if(result == NULL)
		dsetnil(pRtnVal);
	else if(dsetstr(result, pRtnVal) != 0)
		(void) libfail();
Retn:
	return sess.rtn.status;
	}

#if WordCount
// Count the number of words in the marked region, along with average word sizes, number of chars, etc, and display results on
// message line.  *Interactive only*
int countWords(Datum *pRtnVal, int n, Datum **args) {
	Line *pLine;		// Current line to scan.
	int offset;		// Current char to scan.
	long size;		// Size of region left to count.
	short c;		// Current character to scan.
	bool isWordChar;		// Is current character a word character?
	bool inWord;		// Are we in a word now?
	long wordCount;		// Total number of words.
	long charCount;		// Total number of word characters.
	int lineCount;		// Total number of lines in region.
	Region region;		// Region to look at.

	// Make sure we have a region to count.
	if(getRegion(&region, 0) != Success)
		return sess.rtn.status;
	pLine = region.point.pLine;
	offset = region.point.offset;
	size = region.size;

	// Count up things.
	inWord = false;
	charCount = wordCount = 0;
	lineCount = 0;
	while(size--) {

		// Get the current character...
		if(offset == pLine->used) {	// End of line.
			c = '\n';
			pLine = pLine->next;
			offset = 0;

			++lineCount;
			}
		else {
			c = pLine->text[offset];
			++offset;
			}

		// and tabulate it.
		if((isWordChar = c == '_' || is_letter(c) || is_digit(c)))
			++charCount;
		if(isWordChar && !inWord)
			++wordCount;
		inWord = isWordChar;
		}

	// Report results.
	return mlprintf(MLHome | MLFlush, text100,
			// "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f"
	 wordCount, charCount, region.size, lineCount + 1, (wordCount > 0) ? charCount * 1.0 / wordCount : 0.0);
	}
#endif

// Get an apropos match string with a null default.  Convert a nil argument to null as well.  Return status.
static int getApropos(ShowCtrl *pShowCtrl, const char *prompt, Datum **args) {
	Datum *pPat = &pShowCtrl->pat;

	if(!(sess.opFlags & OpScript)) {
		char workBuf[WorkBufSize + 1];

		sprintf(workBuf, "%s %s", text20, prompt);
				// "Apropos"
		if(termInp(pPat, workBuf, ArgNil1, 0, NULL) != Success)
			return sess.rtn.status;
		if(disnil(pPat))
			dsetnull(pPat);
		}
	else if(disnil(args[0]))
		dsetnull(pPat);
	else
		dxfer(pPat, args[0]);

	// Set up match record if pattern given.  Force "ignore" if non-RE and non-Exact.
	if(!disnil(pPat) && !disnull(pPat) && newSearchPat(pPat->str, &pShowCtrl->match, NULL, false) == Success) {
		if(pShowCtrl->match.flags & SOpt_Regexp) {
			if(compileRE(&pShowCtrl->match, SCpl_ForwardRE) != Success)
				freeSearchPat(&pShowCtrl->match);
			}
		else if(!(pShowCtrl->match.flags & SOpt_Exact))
			pShowCtrl->match.flags |= SOpt_Ignore;
		}

	return sess.rtn.status;
	}

// Initialize color pairs for a "show" listing.
void initInfoColors(void) {

	if(sess.opFlags & OpHaveColor) {
		short *colorPair = term.itemColors[ColorIdxInfo].colors;
		if(colorPair[0] >= -1) {
			short foreground, background, line;

			foreground = colorPair[0];
			background = colorPair[1];
			line = background >= 0 ? background : foreground;
			(void) init_pair(term.maxWorkPair - ColorPairIH, foreground, background);
			(void) init_pair(term.maxWorkPair - ColorPairISL, line, -1);
			}
		}
	}

// Initialize a ShowCtrl object for a "show" listing.  If args not NULL, get a match string and save in control object for later
// use.  Return status.
static int showOpen(ShowCtrl *pShowCtrl, int n, const char *promptLabel, Datum **args) {
	char *str, workBuf[strlen(promptLabel) + 3];

	dinit(&pShowCtrl->name);
	dinit(&pShowCtrl->value);
	dinit(&pShowCtrl->pat);
	minit(&pShowCtrl->match);
	pShowCtrl->origNArg = n;

	// If args not NULL, get match string.
	if(args != NULL && getApropos(pShowCtrl, promptLabel, args) != Success)
		return sess.rtn.status;

	// Create a buffer name (plural of promptLabel) and get a buffer.
	str = stpcpy(workBuf, promptLabel);
	workBuf[0] = upCase[(int) workBuf[0]];
	strcpy(str, str[-1] == 's' ? "es" : "s");
	if(sysBuf(workBuf, &pShowCtrl->pRptBuf, BFTermAttr) == Success)
		initInfoColors();

	return sess.rtn.status;
	}

// Store character c in s len times and return pointer to terminating null.
static char *dupChar(char *s, short c, int len) {

	while(len-- > 0)
		*s++ = c;
	*s = '\0';
	return s;
	}

// Copy src to dest in upper case, inserting a space between each two characters.  Return dest.
static char *inflate1(char *dest, const char *src) {

	do {
		*dest++ = upCase[(int) *src++];
		*dest++ = ' ';
		} while(*src != '\0');
	return dest;
	}

// Copy src to dest in upper case, inserting a space between each two characters.  If plural is true, append "s" or "es".
// Return dest.
char *inflate(char *dest, const char *src, bool plural) {

	dest = inflate1(dest, src);
	if(plural)
		dest = inflate1(dest, dest[-2] == 'S' ? "es" : "s");
	*--dest = '\0';
	return dest;
	}

// Write header lines to an open fabrication object, given object pointer, report title, plural flag, column headings line, and
// column widths.  Return status.  If pWidth is NULL, title (only) is written with bold and color attributes (if possible),
// centered in full terminal width and, if colHead not NULL, length of colHead string is used for colored length of title.
// Otherwise, page width is calculated from pWidth array, title is written in bold, centered in calculated width, and headings
// are written in bold and color.  The minWidth member of the second-to-last element of the pWidth array may be -1, which
// indicates a width from the current column position to the right edge of the screen.  Spacing between columns is 1 if terminal
// width < 96, otherwise 2.  *pPageWidth is set to page width if pPageWidth not NULL.
static int rptHdr(DFab *rpt, const char *title, bool plural, const char *colHead, ColHdrWidth *pWidth, int *pPageWidth) {
	ColHdrWidth *pWidth1;
	int leadIn, whitespace, width;
	const char *space = (term.cols < 96) ? " " : "  ";
	int spacing = (term.cols < 96) ? 1 : 2;
	short *colorPair = term.itemColors[ColorIdxInfo].colors;
	char workBuf[term.cols + 1];

	// Expand title, get page width, and begin color if applicable.
	inflate(workBuf, title, plural);
	if(pWidth == NULL) {
		if(colHead == NULL)
			width = term.cols;
		else {
			width = strlen(colHead);
			if((leadIn = (term.cols - width) >> 1) > 0 && dputf(rpt, 0, "%*s", leadIn, "") != 0)
				goto LibFail;
			}
		if((colorPair[0] >= -1) &&
		 dputf(rpt, 0, "~%dc", term.maxWorkPair - ColorPairIH) != 0)
			goto LibFail;
		}
	else {
		width = -spacing;
		pWidth1 = pWidth;
		do {
			if(pWidth1->minWidth == -1) {
				width = term.cols;
				break;
				}
			width += pWidth1->minWidth + spacing;
			++pWidth1;
			} while(pWidth1->minWidth != 0);
		}
	if(pPageWidth != NULL)
		*pPageWidth = width;

	// Write centered title line in bold.
	whitespace = width - strlen(workBuf);
	leadIn = whitespace >> 1;
	if(dputf(rpt, 0, "%*s~b%s~B", leadIn, "", workBuf) != 0)
		goto LibFail;

	// Finish title line and write column headings if requested.
	if(pWidth != NULL) {
		const char *str = colHead;
		int curCol = 0;

		if(dputs("\n\n", rpt, 0) != 0)
			goto LibFail;

		// Write column headings in bold, and in color if available.
		if(dputs("~b", rpt, 0) != 0)
			goto LibFail;
		pWidth1 = pWidth;
		do {
			if(pWidth1 != pWidth) {
				if(dputs(space, rpt, 0) != 0)
					goto LibFail;
				curCol += spacing;
				}
			width = (pWidth1->minWidth == -1) ? term.cols - curCol : (int) pWidth1->minWidth;
			if(colorPair[0] >= -1) {
				if(dputf(rpt, 0, "~%dc%-*.*s~C", term.maxWorkPair - ColorPairIH, width, (int) pWidth1->maxWidth,
				 str) != 0)
					goto LibFail;
				}
			else if(dputf(rpt, 0, "%-*.*s", width, (int) pWidth1->maxWidth, str) != 0)
				goto LibFail;
			curCol += width;
			str += pWidth1->maxWidth;
			++pWidth1;
			} while(pWidth1->minWidth != 0);
		if(dputs("~B", rpt, 0) != 0)
			goto LibFail;

		// Underline each column header if no color.
		if(!(sess.opFlags & OpHaveColor)) {
			if(dputc('\n', rpt, 0) != 0)
				goto LibFail;
			dupChar(workBuf, '-', term.cols);
			str = workBuf;
			curCol = 0;
			pWidth1 = pWidth;
			do {
				if(pWidth1 != pWidth) {
					if(dputs(space, rpt, 0) != 0)
						goto LibFail;
					curCol += spacing;
					}
				width = (pWidth1->minWidth == -1) ? term.cols - curCol : (int) pWidth1->minWidth;
				if(dputf(rpt, 0, "%.*s", width, str) != 0)
					goto LibFail;
				curCol += width;
				str += pWidth1->minWidth;
				++pWidth1;
				} while(pWidth1->minWidth != 0);
			}
		}
	else if((sess.opFlags & OpHaveColor) && dputf(rpt, 0, "%*s~C", whitespace - leadIn, "") != 0)
		goto LibFail;
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Write header lines to an open fabrication object, given report title.  Return status.
static int showHdr(ShowCtrl *pShowCtrl, const char *title) {

	// If not using color, write separator line.
	if(!(sess.opFlags & OpHaveColor)) {
		char workBuf[term.cols + 1];
		dupChar(workBuf, '=', term.cols);
		if(dputs(workBuf, &pShowCtrl->rpt, 0) != 0 || dputc('\n', &pShowCtrl->rpt, 0) != 0)
			return libfail();
		}

	// Write title.
	return rptHdr(&pShowCtrl->rpt, title, true, NULL, NULL, NULL);
	}

// Store indented description in report buffer if present and not blank, and return status.  Wrap into as many lines as needed.
// Text may contain terminal attribute sequences, newlines, and/or tabs.
static int storeDesc(ShowCtrl *pShowCtrl) {
	int attrLen, col, tabCol, spaces;
	ushort flags = TA_ScanOnly;
	char *str0, *str1, *str, *strEnd, *line, *lineEnd;
	char lineBuf[term.cols * 2];

	str0 = (char *) pShowCtrl->descrip;

	// Skip leading white space and trailing newlines in text.
	for(;;) {
		if(*str0 != ' ')
			break;
		++str0;
		}
	for(strEnd = strchr(str0, '\0'); strEnd > str0 && strEnd[-1] == '\n'; --strEnd);
	if(str0 == strEnd)
		return sess.rtn.status;

	tabCol = 4;
	lineEnd = lineBuf + sizeof(lineBuf);
	str = str0;
	goto Init;

	// Wrap loop.
	for(;;) {
		switch(*str) {
			case '\n':
				tabCol = 4;
				goto NewLine;
			case '\t':
				col += (spaces = 4 - col % 4);
				if(col >= term.cols - 16)
					goto WrapErr;			// Left indent too far.
				tabCol = col;
				do {
					if(line == lineEnd)
						goto WrapErr;
					*line++ = ' ';
					} while(--spaces > 0);
				break;
			case AttrSpecBegin:
				if(str + 1 == strEnd)
					goto NewLine;			// Ignore '~' at end of text.
				if(str[1] == AttrSpecBegin) {
					str1 = str + 2;
					++col;
					}
				else {
					str1 = putAttrStr(str + 1, strEnd, &flags, (void *) &attrLen);
					col += str1 - str - attrLen;
					}

				if(col >= term.cols)
					goto NewLine;
				while(str < str1) {			// Copy attribute sequence to line buffer.
					if(line == lineEnd)
						goto WrapErr;
					*line++ = *str++;
					}
				continue;
			default:
				// Store plain character.  Check if any room left.
				if(col < term.cols) {
					if(line == lineEnd) {
WrapErr:
						// Line overflow.  Find end of help item name and return error.
						for(str = pShowCtrl->name.str; *str != '\0' && *str != ' '; ++str);
						return rsset(Failure, RSTermAttr, text40, (int) (str - pShowCtrl->name.str),
							// "Line overflow, wrapping descriptive text for name '~b%.*s~B'"
						 pShowCtrl->name.str);
						}
					*line++ = *str;
					++col;
					break;
					}

				// Current column at right edge of screen.  Break line at last space character.
				str1 = str;
				for(;;) {
					if(*str == ' ') {
						line -= (str1 - str);
						break;
						}
					if(--str == str0) {
						// Space not found.
						str = str1;
						break;
						}
					}
				goto NewLine;
			}

		if(++str == strEnd) {
NewLine:
			// Write line buffer and check if done.
			if(dputc('\n', &pShowCtrl->rpt, 0) != 0 ||
			 dputmem((void *) lineBuf, line - lineBuf, &pShowCtrl->rpt, 0) != 0)
				goto LibFail;
			if(str == strEnd)
				break;
			if(*str == ' ' || *str == '\n')
				++str;
			if(str == strEnd)
				break;
Init:
			// Have more text to store.  Indent next line.
			line = lineBuf;
			col = 0;
			do {
				line = stpcpy(line, "    ");
				} while((col += 4) < tabCol);

			str0 = str;
			}
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Build a "show" listing in a report buffer, given ShowCtrl object, flags, section title (which may be NULL), and pointer to
// routine which sets the name + args, value (if applicable), and description for the next list item in the ShowCtrl object.
// Return status.
static int showBuild(ShowCtrl *pShowCtrl, ushort flags, const char *title,
 int (*func)(ShowCtrl *pShowCtrl, ushort request, const char **pName)) {
	const char *nameTable[] = {NULL, NULL, NULL};
	const char **pName;
	char *str;
	bool firstItem = true;
	bool doApropos = !disnil(&pShowCtrl->pat);
	Datum *pIndex, *pSrc;
	short *colorPair = term.itemColors[ColorIdxInfo].colors;
	char sepLine[term.cols + 1];
	char workBuf[WorkBufSize];

	// Initialize.
	pShowCtrl->pItem = NULL;
	if(flags & SHNoDesc)
		pShowCtrl->descrip = NULL;
	if(doApropos && !(flags & SHExact)) {
		if(dnewtrack(&pIndex) != 0 || dnewtrack(&pSrc) != 0)
			goto LibFail;
		}

	// Open a fabrication object and write section header if applicable.
	if(dopentrack(&pShowCtrl->rpt) != 0)
		goto LibFail;
	if(title != NULL && showHdr(pShowCtrl, title) != Success)
		return sess.rtn.status;

	// Loop through detail items.
	dupChar(sepLine, '-', term.cols);
	for(;;) {
		// Find next item and get its name.  Exit loop if no items left.
		if(func(pShowCtrl, SHReqNext, nameTable) != Success)
			return sess.rtn.status;
		if(nameTable[0] == NULL)
			break;

		// Skip if apropos in effect and item name doesn't match the search pattern.
		if(doApropos) {
			if(flags & SHExact) {
				if(strcmp(nameTable[0], pShowCtrl->pat.str) != 0)
					continue;
				}
			else if(!disnull(&pShowCtrl->pat)) {
				pName = nameTable;
				do {
					if(dsetstr(*pName++, pSrc) != 0)
						goto LibFail;
					if(strIndex(pIndex, 0, pSrc, &pShowCtrl->pat, &pShowCtrl->match) != Success)
						return sess.rtn.status;
					if(!disnil(pIndex))
						goto MatchFound;
					} while(*pName != NULL);
				continue;
				}
			}
MatchFound:
		// Get item arguments and description, and "has value" flag (in nameTable[0] slot).
		if(func(pShowCtrl, SHReqSynop, nameTable) != Success)
			return sess.rtn.status;

		// Begin next line.
		if((flags & SHSepLine) || firstItem) {
			if(dputc('\n', &pShowCtrl->rpt, 0) != 0)
				goto LibFail;
			if(colorPair[0] >= -1 && dputf(&pShowCtrl->rpt, 0, "~%dc", term.maxWorkPair - ColorPairISL) != 0)
				goto LibFail;
			if(dputs(sepLine, &pShowCtrl->rpt, 0) != 0)
				goto LibFail;
			if(colorPair[0] >= -1 && dputs("~C", &pShowCtrl->rpt, 0) != 0)
				goto LibFail;
			}
		firstItem = false;

		// Store item name and value, if any, in work buffer and add line to report.
		sprintf(workBuf, "~b%s~B", pShowCtrl->name.str);
		if(nameTable[0] != NULL) {
			str = pad(workBuf, 34);
			if(str[-2] != ' ')
				strcpy(str, str[-1] == ' ' ? " " : "  ");
			}
		if(dputc('\n', &pShowCtrl->rpt, 0) != 0 || dputs(workBuf, &pShowCtrl->rpt, 0) != 0)
			goto LibFail;
		if(nameTable[0] != NULL && func(pShowCtrl, SHReqValue, NULL) != Success)
			return sess.rtn.status;

		// Store indented description, if present and not blank.
		if(pShowCtrl->descrip != NULL && storeDesc(pShowCtrl) != Success)
			return sess.rtn.status;
		}

	// Close fabrication object and append report (string) to report buffer if any items were written.
	if(dclose(&pShowCtrl->rpt, FabStr) != 0)
LibFail:
		return libfail();
	if(!firstItem) {

		// Write blank line if title not NULL and buffer not empty.
		if(title != NULL && !bempty(pShowCtrl->pRptBuf))
			if(bappend(pShowCtrl->pRptBuf, "") != Success)
				return sess.rtn.status;

		// Append section detail.
		(void) bappend(pShowCtrl->pRptBuf, pShowCtrl->rpt.pDatum->str);
		}

	return sess.rtn.status;
	}

// Close a "show" listing.  Return status.
int showClose(Datum *pRtnVal, int n, ShowCtrl *pShowCtrl) {

	dclear(&pShowCtrl->name);
	dclear(&pShowCtrl->value);
	dclear(&pShowCtrl->pat);
	freeSearchPat(&pShowCtrl->match);

	// Add "no entries found" message to empty report buffer.
	if(bempty(pShowCtrl->pRptBuf) && bappend(pShowCtrl->pRptBuf, text456) != Success)
						// "\n\n\t(No entries found)"
		return sess.rtn.status;

	// Display the list.
	return render(pRtnVal, n, pShowCtrl->pRptBuf, RendNewBuf | RendRewind);
	}

// Get name, arguments, and key bindings (if any) for current list item (system/user command or function) and save in
// report-control object.  Return status.
static int findKeys(ShowCtrl *pShowCtrl, uint keyType, void *pItem) {
	KeyWalk keyWalk = {NULL, NULL};
	KeyBind *pKeyBind;
	Buffer *pBuf;
	CmdFunc *pCmdFunc;
	CallInfo *pCallInfo = NULL;
	char *name, *argSyntax;
	char keyBuf[16];

	// Set pointers and item description.
	if(keyType & PtrUserCmdFunc) {
		pBuf = (Buffer *) pItem;
		name = pBuf->bufname + 1;
		pCallInfo = pBuf->pCallInfo;
		if(pCallInfo != NULL) {
			argSyntax = !disnil(&pCallInfo->argSyntax) ? pCallInfo->argSyntax.str : NULL;
			pShowCtrl->descrip = !disnil(&pCallInfo->descrip) ? pCallInfo->descrip.str : NULL;
			}
		else
			pShowCtrl->descrip = argSyntax = NULL;
		}
	else {
		pCmdFunc = (CmdFunc *) pItem;
		name = (char *) pCmdFunc->name;
		argSyntax = (char *) pCmdFunc->argSyntax;
		pShowCtrl->descrip = pCmdFunc->descrip;
		}

	// Set item name and arguments.
	if(argSyntax == NULL) {
		if(dsetstr(name, &pShowCtrl->name) != 0)
			goto LibFail;
		}
	else {
		char workBuf[strlen(name) + strlen(argSyntax) + 1];
		sprintf(workBuf, "%s %s", name, argSyntax);
		if(dsetstr(workBuf, &pShowCtrl->name) != 0)
			goto LibFail;
		}

	// Set key bindings, if any.
	if(keyType & (PtrSysFunc | PtrUserFunc))
		dclear(&pShowCtrl->value);
	else {
		DFab fab;
		char *sep = NULL;

		// Search for any keys bound to system or user command "pItem".
		if(dopenwith(&fab, &pShowCtrl->value, FabClear) != 0)
			goto LibFail;
		pKeyBind = nextBind(&keyWalk);
		do {
			if((pKeyBind->targ.type & keyType) && pKeyBind->targ.u.pVoid == pItem) {

				// Add the key sequence.
				ektos(pKeyBind->code, keyBuf, true);
				if((sep != NULL && dputs(sep, &fab, 0) != 0) || dputf(&fab, 0, "~#u%s~U", keyBuf) != 0)
					goto LibFail;
				sep = ", ";
				}
			} while((pKeyBind = nextBind(&keyWalk)) != NULL);
		if(dclose(&fab, FabStr) != 0)
			goto LibFail;
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Get options and set flags for showCommands() and showFunctions() functions.  Return status, including Cancelled if no options
// are entered interactively.
static int getCFOpts(ShowCtrl *pShowCtrl, int n, Datum **args, const char *type) {

	initBoolOpts(cmdFuncOptions);
	if(n == INT_MIN) {
		selSystem = selUser = true;
		selAll = false;
		}
	else {
		int count;
		char workBuf[64];

		// Ignore "All" option if showCommands.
		cmdFuncOptions[2].ctrlFlags = (type == text158)	? OptIgnore : 0;
						// "command"
		// Build prompt if interactive
		if(sess.opFlags & OpScript)
			workBuf[0] = '\0';
		else if(type == text803)
			sprintf(workBuf, "%s %ss and %ss", text24, text158, text247);
						// "Select", "command", "function"
		else
			sprintf(workBuf, "%s %ss", text24, type);
					// "Select"

		if(parseOpts(&cmdFuncOptHdr, workBuf, args[1], &count) != Success)
			return sess.rtn.status;
		if(count == 0)
			return Cancelled;
		setBoolOpts(cmdFuncOptions);
		if(!selSystem && !selUser)
			selSystem = selUser = true;	// "All" option implies "System" and "User".
		}
	return sess.rtn.status;
	}

// Flags for nextCmdFunc().
#define CFUser		0x0001		// Process user command or function, otherwise system.
#define CFCommand	0x0002		// Process a command, otherwise a function.
#define CFSelAll	0x0004		// Select all items that match pattern, ignoring any other requirements.

// Get next system/user command/function name or description and store in report-control object.  If request is SHReqNext,
// set *pName to item name, or NULL if no items left.  Return status.
static int nextCmdFunc(ShowCtrl *pShowCtrl, ushort request, const char **pName, ushort flags) {
	CmdFunc *pCmdFunc;
	Buffer *pBuf;
	Datum **ppBufItem;
	Datum **ppBufItemEnd = bufTable.elements + bufTable.used;

	// First call?
	if(pShowCtrl->pItem == NULL) {
		if(flags & CFUser) {
			pShowCtrl->pItem = (void *) (ppBufItem = bufTable.elements);
			pBuf = bufPtr(bufTable.elements[0]);
			}
		else
			pShowCtrl->pItem = (void *) (pCmdFunc = cmdFuncTable);
		}
	else if(flags & CFUser) {
		ppBufItem = (Datum **) pShowCtrl->pItem;
		pBuf = (request == SHReqNext && ++ppBufItem == ppBufItemEnd) ? NULL : bufPtr(*ppBufItem);
		}
	else {
		pCmdFunc = (CmdFunc *) pShowCtrl->pItem;
		if(request == SHReqNext)
			++pCmdFunc;
		}

	// Process request.
	switch(request) {
		case SHReqNext:
			if(flags & CFUser) {
				ushort attrFlag = (flags & CFCommand) ? BFCommand : BFFunc;
				while(pBuf != NULL) {

					// Select if command or function buffer is right type and (is a command or CFSelAll flag
					// set or has help text or is bound to a hook).
					if((pBuf->flags & BFCmdFunc) == attrFlag && (attrFlag == BFCommand ||
					 (flags & CFSelAll) || !disnil(&pBuf->pCallInfo->argSyntax) ||
					 !disnil(&pBuf->pCallInfo->descrip) || isHook(pBuf, false))) {

						// Found user command or function... return its name.
						*pName = pBuf->bufname + 1;
						pShowCtrl->pItem = (void *) ppBufItem;
						goto Retn;
						}
					pBuf = (++ppBufItem == ppBufItemEnd) ? NULL : bufPtr(*ppBufItem);
					}
				}
			else {
				ushort attrFlag = (flags & CFCommand) ? 0 : CFFunc;
				for(; pCmdFunc->name != NULL; ++pCmdFunc) {
		
					// Skip if wrong type.
					if((pCmdFunc->attrFlags & CFFunc) != attrFlag)
						continue;

					// Found item... return its name.
					*pName = pCmdFunc->name;
					pShowCtrl->pItem = (void *) pCmdFunc;
					goto Retn;
					}
				}

			// End of table.
			*pName = NULL;
			break;
		case SHReqSynop:
			if(flags & CFUser) {
				if(findKeys(pShowCtrl, (flags & CFCommand) ? PtrUserCmd : PtrUserFunc,
				 (void *) pBuf) != Success)
					return sess.rtn.status;
				*pName = disnil(&pShowCtrl->value) ? NULL : pBuf->bufname;
				}
			else {
				if(findKeys(pShowCtrl, (flags & CFCommand) ? PtrSysCmdType : PtrSysFunc,
				 (void *) pCmdFunc) != Success)
					return sess.rtn.status;
				*pName = disnil(&pShowCtrl->value) ? NULL : pCmdFunc->name;
				}
			break;
		default: // SHReqValue
			if(dputs(pShowCtrl->value.str, &pShowCtrl->rpt, 0) != 0)
				(void) libfail();
		}
Retn:
	return sess.rtn.status;
	}

// Get next system command name and description and store in report-control object via call to nextCmdFunc().
static int nextSysCommand(ShowCtrl *pShowCtrl, ushort request, const char **pName) {

	return nextCmdFunc(pShowCtrl, request, pName, CFCommand);
	}

// Get next user command name and description and store in report-control object via call to nextCmdFunc().
static int nextUserCommand(ShowCtrl *pShowCtrl, ushort request, const char **pName) {

	return nextCmdFunc(pShowCtrl, request, pName, CFUser | CFCommand);
	}

// Create formatted list of commands via calls to "show" routines.  Return status.
int showCommands(Datum *pRtnVal, int n, Datum **args) {
	ShowCtrl showCtrl;

	// Get options and set flags.
	if(getCFOpts(&showCtrl, n, args, text158) == Success) {
				// "command"

		// Open control object, build listing, and close it.
		if(showOpen(&showCtrl, n, text158, args) == Success &&
				// "command"
		 (!selSystem || showBuild(&showCtrl, SHSepLine, text414, nextSysCommand) == Success) &&
							// "system command"
		 (!selUser || showBuild(&showCtrl, SHSepLine, text417, nextUserCommand) == Success))
							// "user command"
			(void) showClose(pRtnVal, n, &showCtrl);
		}
	return sess.rtn.status;
	}

// Get next system function name and description and store in report-control object via call to nextCmdFunc().
static int nextSysFunction(ShowCtrl *pShowCtrl, ushort request, const char **pName) {

	return nextCmdFunc(pShowCtrl, request, pName, 0);
	}

// Get next user function name and description and store in report-control object via call to nextCmdFunc().
static int nextUserFunction(ShowCtrl *pShowCtrl, ushort request, const char **pName) {

	return nextCmdFunc(pShowCtrl, request, pName, selAll ? CFUser | CFSelAll : CFUser);
	}

// Create formatted list of system functions via calls to "show" routines.  Return status.
int showFunctions(Datum *pRtnVal, int n, Datum **args) {
	ShowCtrl showCtrl;

	// Get options and set flags.
	if(getCFOpts(&showCtrl, n, args, text247) == Success) {
				// "function"

		// Open control object, build listing, and close it.
		if(showOpen(&showCtrl, n, text247, args) == Success &&
				// "function"
		 (!selSystem || showBuild(&showCtrl, SHSepLine, text336, nextSysFunction) == Success) &&
							// "system function"
		 (!selUser || showBuild(&showCtrl, SHSepLine, text418, nextUserFunction) == Success))
							// "user function"
			(void) showClose(pRtnVal, n, &showCtrl);
		}
	return sess.rtn.status;
	}

// Get next alias name or description and store in report-control object.  If request is SHReqNext, set *pName to item name, or
// NULL if no items left.  Return status.
static int nextAlias(ShowCtrl *pShowCtrl, ushort request, const char **pName) {
	Alias *pAlias;

	// First call?
	if(pShowCtrl->pItem == NULL)
		pShowCtrl->pItem = (void *) (pAlias = ahead);
	else {
		pAlias = (Alias *) pShowCtrl->pItem;
		if(request == SHReqNext)
			pAlias = pAlias->next;
		}

	// Process request.
	switch(request) {
		case SHReqNext:
			for(; pAlias != NULL; pAlias = pAlias->next) {

				// Found alias... return its name and name it points to.
				*pName++ = pAlias->name;
				*pName = (pAlias->type & (PtrAliasUserCmd | PtrAliasUserFunc)) ? pAlias->targ.u.pBuf->bufname :
				 pAlias->targ.u.pCmdFunc->name;
				pShowCtrl->pItem = (void *) pAlias;
				goto Retn;
				}

			// End of list.
			*pName = NULL;
			break;
		case SHReqSynop:
			if(dsetstr(pAlias->name, &pShowCtrl->name) != 0)
				goto LibFail;
			*pName = pAlias->name;
			break;
		default: // SHReqValue
			{const char *name2 = (pAlias->targ.type & PtrUserCmdFunc) ? pAlias->targ.u.pBuf->bufname :
			 pAlias->targ.u.pCmdFunc->name;
			if(dputs("-> ", &pShowCtrl->rpt, 0) != 0 || dputs(name2, &pShowCtrl->rpt, 0) != 0)
				goto LibFail;
			}
		}
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Create formatted list of aliases via calls to "show" routines.  Return status.
int showAliases(Datum *pRtnVal, int n, Datum **args) {
	ShowCtrl showCtrl;

	// Open control object, build listing, and close it.
	if(showOpen(&showCtrl, n, text127, args) == Success &&
			// "alias"
	 showBuild(&showCtrl, SHNoDesc, text127, nextAlias) == Success)
		(void) showClose(pRtnVal, n, &showCtrl);
	return sess.rtn.status;
	}

// Write a separator line to an open Fab object (in color if possible), given length.  If length is -1, terminal width is
// used.  Return -1 if error.
int sepLine(int len, DFab *pFab) {

	if(len < 0)
		len = term.cols;
	char line[len + 1];
	dupChar(line, '-', len);
	if(dputc('\n', pFab, 0) != 0)
		return -1;
	return (term.itemColors[ColorIdxInfo].colors[0] >= -1) ?
	 dputf(pFab, 0, "~%dc%s~C", term.maxWorkPair - ColorPairISL, line) : dputs(line, pFab, 0);
	}

// Build and pop up a buffer containing a list of all visible buffers (default) or those selected per options if n argument.
// Render buffer and return status.
int showBuffers(Datum *pRtnVal, int n, Datum **args) {
	Array *pArray;
	Datum *pArrayEl;
	Buffer *pRptBuf, *pBuf;
	DFab rpt;
	short c;
	int pageWidth;
	bool firstBuf = true;
	char *space = (term.cols < 96) ? " " : "  ";
	static ColHdrWidth colWidths[] = {
		{9, 9}, {9, 9}, {MaxBufname - 4, MaxBufname - 4}, {31, 8}, {0, 0}};
	struct BufFlag {				// Buffer flag item.
		ushort bufFlag, dispChar;
		} *pBufFlag;
	static struct BufFlag bufFlagTable[] = {	// Array of buffer flags.
		{BFActive, SB_Active},			// Activated buffer -- file read in.
		{BFHidden, SB_Hidden},			// Hidden buffer.
		{BFCommand, 0},				// User command buffer.
		{BFFunc, 0},				// User function buffer.
		{BFPreproc, SB_Preproc},		// Preprocessed buffer.
		{BFNarrowed, SB_Narrowed},		// Narrowed buffer.
		{BFTermAttr, SB_TermAttr},		// Terminal-attributes buffer.
		{USHRT_MAX, SB_Background},		// Background buffer.
		{BFReadOnly, SB_ReadOnly},		// Read-only buffer.
		{BFChanged, SB_Changed},		// Changed buffer.
		{0, 0}};
	struct FlagInfo {
		short c;
		const char *descrip;
		} *pFlagInfo;
	static struct FlagInfo flagTable[] = {
		{SB_Active, text31},			// "Active"
		{SB_Hidden, text400},			// "Hidden"
		{SB_Command, text412},			// "User command"
		{SB_Func, text183},			// "User function"
		{SB_Preproc, text441},			// "Preprocessed"
		{SB_Narrowed, text308},			// "Narrowed"
		{SB_TermAttr, text442},			// "Terminal attributes enabled"
		{SB_Background, text462},		// "Background (not being displayed)"
		{SB_ReadOnly, text459},			// "Read only"
		{SB_Changed, text439},			// "Changed"
		{'\0', NULL}};
	static bool selCommand, selFunction, selHomed, selVisible, inclHidden, inclHomeDir;
	static Option options[] = {
		{"^Visible", "^Viz", 0, .u.ptr = (void *) &selVisible},
		{"Ho^med", "H^md", 0, .u.ptr = (void *) &selHomed},
		{"^Command", "^Cmd", 0, .u.ptr = (void *) &selCommand},
		{"^Function", "^Func", 0, .u.ptr = (void *) &selFunction},
		{"^Hidden", "^Hid", 0, .u.ptr = (void *) &inclHidden},
		{"Home^Dir", "Hm^Dir", 0, .u.ptr = (void *) &inclHomeDir},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text410, false, options};
			// "command option"

	// Get options and set flags.
	initBoolOpts(options);
	if(n == INT_MIN)
		selVisible = true;
	else {
		int count;
		char workBuf[64];

		// Build prompt if interactive
		if(sess.opFlags & OpScript)
			workBuf[0] = '\0';
		else
			sprintf(workBuf, "%s %ss", text250, text83);
					// "Show", "buffer"

		if(parseOpts(&optHdr, workBuf, args[0], &count) != Success || count == 0)
			return sess.rtn.status;
		setBoolOpts(options);
		if(selVisible)
			selHomed = false;
		else if(!selHomed && !selCommand && !selFunction)
			return rsset(Failure, 0, text455, optHdr.type);
				// "Missing required %s",
		}

	// Get new buffer and fabrication object.
	if(sysBuf(text159, &pRptBuf, BFTermAttr) != Success)
			// "Buffers"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rptHdr(&rpt, text159, false, text30, colWidths, &pageWidth) != Success)
			// "Buffers", "AHUPNTBRC   Size      Buffer              File"
		return sess.rtn.status;

	// Output the list of buffers.
	pArray = &bufTable;
	while((pArrayEl = aeach(&pArray)) != NULL) {
		pBuf = bufPtr(pArrayEl);

		// Skip if buffer not requested.
		if(pBuf->flags & BFCommand) {
			if(!selCommand)
				continue;
			}
		else if(pBuf->flags & BFFunc) {
			if(!selFunction)
				continue;
			}
		else {
			if((pBuf->flags & BFHidden) && !inclHidden)
				continue;
			if(!selVisible && (!selHomed || pBuf->saveDir == NULL || pBuf->saveDir != sess.cur.pScrn->workDir))
				continue;
			}

		// Write separator line if displaying home directory and not first buffer.
		if(firstBuf)
			firstBuf = false;
		else if(inclHomeDir && sepLine(pageWidth, &rpt) != 0)
			goto LibFail;

		if(dputc('\n', &rpt, 0) != 0)
			goto LibFail;

		// Output the buffer flag indicators.
		pBufFlag = bufFlagTable;
		do {
			if(pBufFlag->bufFlag == USHRT_MAX)
				c = (pBuf->windCount == 0) ? pBufFlag->dispChar : ' ';
			else if(pBufFlag->bufFlag == BFCommand) {
				c = (pBuf->flags & BFCommand) ? SB_Command : (pBuf->flags & BFFunc) ? SB_Func : ' ';
				++pBufFlag;
				}
			else if(!(pBuf->flags & pBufFlag->bufFlag))
				c = ' ';
			else if((c = pBufFlag->dispChar) == AttrSpecBegin && dputc(AttrSpecBegin, &rpt, 0) != 0)
				goto LibFail;
			if(dputc(c, &rpt, 0) != 0)
				goto LibFail;
			} while((++pBufFlag)->bufFlag != 0);

#if (MMDebug & Debug_WindCount) && 0
		dputf(&rpt, 0, "%3hu", pBuf->windCount);
#endif
		// Output the buffer size, buffer name, and filename.
		if(dputf(&rpt, 0, "%s%*lu%s%-*s", space, colWidths[1].minWidth, bufLength(pBuf, NULL), space,
		 colWidths[2].minWidth, pBuf->bufname) != 0 || (pBuf->filename != NULL && (dputs(space, &rpt, 0) != 0 ||
		 dputs(pBuf->filename, &rpt, 0) != 0)))
			goto LibFail;

		// Output the home directory if requested and not NULL.
		if(inclHomeDir && pBuf->saveDir != NULL &&
		 dputf(&rpt, 0, "\n%*s%sCWD: %s", colWidths[0].minWidth, "", space, pBuf->saveDir) != 0)
			goto LibFail;
		}

	// Write footnote.
	if(sepLine(pageWidth, &rpt) != 0)
		goto LibFail;
	pFlagInfo = flagTable;
	do {
		if(dputf(&rpt, 0, "\n%c", (int) pFlagInfo->c) != 0 || (pFlagInfo->c == AttrSpecBegin && dputc(AttrSpecBegin,
		 &rpt, 0) != 0) || dputc(' ', &rpt, 0) != 0 || dputs(pFlagInfo->descrip, &rpt, 0) != 0)
			goto LibFail;
		} while((++pFlagInfo)->descrip != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt, FabStr) != 0)
		goto LibFail;
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, n, pRptBuf, RendNewBuf | RendRewind);
LibFail:
	return libfail();
	}

// Display color palette or users pairs in a pop-up window.
int showColors(Datum *pRtnVal, int n, Datum **args) {

	if(!haveColor())
		return sess.rtn.status;

	Buffer *pRptBuf;
	DFab rpt;
	short pair;

	// Get a buffer and open a fabrication object.
	if(sysBuf(text428, &pRptBuf, BFTermAttr) != Success)
			// "Colors"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;

	// Display color pairs in use if n <= 0, otherwise color palette (paged).
	if(n <= 0 && n != INT_MIN) {
		short foreground, background;

		if(dputs(text434, &rpt, 0) != 0 || dputs("\n-----   -----------------", &rpt, 0) != 0)
				// "COLOR PAIRS DEFINED\n\n~bPair#        Sample~B"
			goto LibFail;
		pair = 1;
		do {
			if(pair_content(pair, &foreground, &background) == OK &&
			 (foreground != COLOR_BLACK || background != COLOR_BLACK)) {
				if(dputf(&rpt, 0, "\n%4hd    ~%hdc %-16s~C", pair, pair, text431) != 0)
										// "\"A text sample\""
					goto LibFail;
				}
			} while(++pair <= term.maxWorkPair);
		}
	else {
		short c;
		int pages = term.maxColor / term.linesPerPage + 1;

		pair = term.maxWorkPair;
		if(n <= 1) {
			n = 1;
			c = 0;
			}
		else {
			if(n >= pages)
				n = pages;
			c = (n - 1) * term.linesPerPage;
			}

		// Write first header line.
		if(dputf(&rpt, 0, text429, n, pages) != 0 || dputs(text430, &rpt, 0) != 0 ||
			// "COLOR PALETTE - Page %d of %d\n\n"
			//		"~bColor#         Foreground Samples                   Background Samples~B"
		 dputs("\n------ ----------------- -----------------   ----------------- -----------------", &rpt, 0) != 0)
			goto LibFail;

		// For all colors...
		n = 0;
		do {
			// Write color samples.
			(void) init_pair(pair, c, -1);
			(void) init_pair(pair - 1, c, COLOR_BLACK);
			(void) init_pair(pair - 2, term.colorText, c);
			(void) init_pair(pair - 3, COLOR_BLACK, c);
			if((c > 0 && c % 8 == 0 && dputc('\n', &rpt, 0) != 0) || dputf(&rpt, 0,
			 "\n%4hd   ~%dc %-16s~C ~%dc %-16s~C   ~%dc %-16s~C ~%dc %-16s~C",
			 c, (int) pair, text431, pair - 1, text431, pair - 2, text432, pair - 3, text433) != 0)
					// "\"A text sample\"", "With white text", "With black text"
				goto LibFail;
			pair -= 4;
			++c;
			++n;
			} while(c <= term.maxColor && n < term.linesPerPage);
		}

	// Add the results to the buffer.
	if(dclose(&rpt, FabStr) != 0)
LibFail:
		return libfail();
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, -1, pRptBuf, RendNewBuf);
	}

// Find circumflex or null byte in given string and return pointer to it.
static const char *circumflex(const char *str) {

	while(*str != '\0') {
		if(*str == '^')
			break;
		++str;
		}
	return str;
	}

// Build and pop up a buffer containing a list of all hooks, their associated functions, and descriptions of their arguments (if
// any).  Return status.
int showHooks(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pRptBuf;
	const char *nArgDesc0, *nArgDesc, *macArgsDesc0, *macArgsDesc;
	HookRec *pHookRec = hookTable;
	DFab rpt;
	int indent, indent1, pageWidth;
	int spacing = (term.cols < 96) ? 1 : 2;
	static ColHdrWidth colWidths[] = {
		{9, 9}, {MaxBufname - 4, MaxBufname - 4}, {21, 21}, {24, 20}, {0, 0}};

	// Get a buffer and open a fabrication object.
	if(sysBuf(text316, &pRptBuf, BFTermAttr) != Success)
			// "Hooks"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rptHdr(&rpt, text316, false, text315, colWidths, &pageWidth) != Success)
			// "Hooks", " Hook     Set to Function      Numeric Argument     Function Arguments"
		return sess.rtn.status;

	// For all hooks...
	do {
		if(pHookRec != hookTable && sepLine(pageWidth, &rpt) != 0)
			goto LibFail;
		if(dputf(&rpt, 0, "\n%-*s%*s%-*s", (int) colWidths[0].minWidth, pHookRec->name, spacing, "",
		 (int) colWidths[1].minWidth, pHookRec->func.type == PtrNull ? "" : hookFuncName(&pHookRec->func)) != 0)
			goto LibFail;
		indent = spacing;
		nArgDesc = pHookRec->nArgDesc;
		macArgsDesc = pHookRec->macArgsDesc;
		indent1 = colWidths[0].minWidth + colWidths[1].minWidth + spacing + spacing;
		for(;;) {
			if(indent == indent1 && dputc('\n', &rpt, 0) != 0)
				goto LibFail;
			nArgDesc = circumflex(nArgDesc0 = nArgDesc);
			macArgsDesc = circumflex(macArgsDesc0 = macArgsDesc);
			if(dputf(&rpt, 0, "%*s%-*.*s", indent, "", (int) colWidths[2].minWidth + attrCount(nArgDesc0,
			 nArgDesc - nArgDesc0, INT_MAX), (int) (nArgDesc - nArgDesc0), nArgDesc0) != 0)
				goto LibFail;
			if(macArgsDesc != macArgsDesc0 &&
			 dputf(&rpt, 0, "%*s%.*s", spacing, "", (int) (macArgsDesc - macArgsDesc0), macArgsDesc0) != 0)
				goto LibFail;
			if(*nArgDesc == '\0' && *macArgsDesc == '\0')
				break;
			indent = indent1;
			if(*nArgDesc == '^')
				++nArgDesc;
			if(*macArgsDesc == '^')
				++macArgsDesc;
			}

		// On to the next hook.
		} while((++pHookRec)->name != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt, FabStr) != 0)
LibFail:
		return libfail();
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, n, pRptBuf, RendNewBuf | RendRewind);
	}

// Describe the system or user command bound to a particular key.  Get single keystroke if n <= 0.  Display result on message
// line if n >= 0, otherwise in a pop-up window (default).  Return status.  *Interactive only*
int showKey(Datum *pRtnVal, int n, Datum **args) {
	ushort extKey;
	KeyBind *pKeyBind;
	const char *argSyntax = NULL;
	const char *descrip = NULL;
	const char *name;
	int (*nextCommand)(ShowCtrl *pShowCtrl, ushort request, const char **pName);
	char keyBuf[16];

	// Prompt the user for the key code.
	if(getKeyBind(text13, n <= 0 && n != INT_MIN ? 0 : INT_MIN, args, &extKey) != Success)
			// "Show key "
		return sess.rtn.status;

	// Find the command.
	if((pKeyBind = getBind(extKey)) == NULL)
		name = text48;
			// "[Not bound]"
	else if(pKeyBind->targ.type == PtrUserCmd) {
		Buffer *pBuf = pKeyBind->targ.u.pBuf;
		name = pBuf->bufname + 1;
		if(n < 0) {
			nextCommand = nextUserCommand;
			argSyntax = text417;
				// "user command"
			goto CmdPop;
			}
		if(!disnil(&pBuf->pCallInfo->argSyntax))
			argSyntax = pBuf->pCallInfo->argSyntax.str;
		if(!disnil(&pBuf->pCallInfo->descrip))
			descrip = pBuf->pCallInfo->descrip.str;
		}
	else {
		CmdFunc *pCmdFunc = pKeyBind->targ.u.pCmdFunc;
		name = pCmdFunc->name;
		nextCommand = nextSysCommand;
		if(n < 0) {
			argSyntax = text414;
				// "system command"
CmdPop:
			goto PopUp;
			}
		argSyntax = pCmdFunc->argSyntax;
		descrip = pCmdFunc->descrip;
		}

	// Display result on message line.
	ektos(extKey, keyBuf, false);
	if(mlprintf(MLHome | MLTermAttr, "~#u%s~U -> ~b%s~B", keyBuf, name) == Success) {
		if(argSyntax != NULL && mlprintf(MLTermAttr, " ~b%s~B", argSyntax) != Success)
			return sess.rtn.status;
		if(descrip != NULL && (mlputs(MLTermAttr, " - ") != Success || mlputs(MLTermAttr, descrip) != Success))
			return sess.rtn.status;
		(void) mlflush();
		}
	return sess.rtn.status;
PopUp:;
	// Display result in pop-up window.
	ShowCtrl showCtrl;

	// Open control object, build listing, and close it.
	if(showOpen(&showCtrl, INT_MIN, text158, NULL) == Success) {
				// "command"
		if(dsetstr(name, &showCtrl.pat) != 0)
			return libfail();
		if(showBuild(&showCtrl, SHSepLine | SHExact, argSyntax, nextCommand) == Success)
			(void) showClose(pRtnVal, -1, &showCtrl);
		}
	return sess.rtn.status;
	}

// Build and pop up a buffer containing all marks which exist in the current buffer.  Render buffer and return status.
int showMarks(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pRptBuf;
	Line *pLine;
	Mark *pMark;
	DFab rpt;
	int max = term.cols * 2;
	char *space = (term.cols < 96) ? " " : "  ";
	int spacing = (term.cols < 96) ? 1 : 2;
	static ColHdrWidth colWidths[] = {
		{4, 4}, {6, 6}, {-1, 11}, {0, 0}};

	// Get buffer for the mark list.
	if(sysBuf(text353, &pRptBuf, BFTermAttr) != Success)
			// "Marks"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rptHdr(&rpt, text353, false, text354, colWidths, NULL) != Success)
			// "Marks", "MarkOffset  Line Text"
		return sess.rtn.status;

	// Loop through lines in current buffer, searching for mark matches.
	pLine = sess.cur.pBuf->pFirstLine;
	do {
		pMark = &sess.cur.pBuf->markHdr;
		do {
			if(pMark->id < '~' && pMark->point.pLine == pLine) {
				if(dputf(&rpt, 0, "\n %c %*d", pMark->id,
				 (int)(colWidths[0].minWidth - 3 + spacing + colWidths[1].minWidth), pMark->point.offset) != 0)
					goto LibFail;
				if(pLine->next == NULL && pMark->point.offset == pLine->used) {
					if(dputf(&rpt, 0, "%s  (EOB)", space) != 0)
						goto LibFail;
					}
				else if(pLine->used > 0 && (dputs(space, &rpt, 0) != 0 ||
				 dputsubstr(pLine->text, pLine->used > max ? max : pLine->used, &rpt, DCvtVizChar) != 0))
					goto LibFail;
				}
			} while((pMark = pMark->next) != NULL);
		} while((pLine = pLine->next) != NULL);

	// Add the report to the buffer.
	if(dclose(&rpt, FabStr) != 0)
LibFail:
		return libfail();
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, n, pRptBuf, RendNewBuf | RendRewind);
	}

// Build and pop up a buffer containing all the global and buffer modes.  Render buffer and return status.
int showModes(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pRptBuf;
	DFab rpt;
	ModeSpec *pModeSpec;
	ModeGrp *pModeGrp;
	Array *pArray;
	Datum *pArrayEl;
	char *str;
	int pageWidth;
	char *space = (term.cols < 96) ? " " : "  ";
	int spacing = (term.cols < 96) ? 1 : 2;
	static ColHdrWidth colWidths[] = {
		{4, 4}, {11, 11}, {57, 15}, {0, 0}};
	static ColHdrWidth colWidthsMG[] = {
		{4, 4}, {11, 11}, {57, 25}, {0, 0}};
	char workBuf[strlen(text437) + strlen(text438) + 1];
			// "AUHL Name          Description", " / Members"
	struct ModeHdr {
		const char *hdr;
		uint mask;
		} *pModeHdr;
	static struct ModeHdr modeHdrTable[] = {
		{text364, MdGlobal},
			// "GLOBAL MODES"
		{text365, 0},
			// "BUFFER MODES"
		{NULL, 0}};
	struct FlagInfo {
		short c;
		ushort flag;
		const char *descrip;
		} *pFlagInfo;
	static struct FlagInfo flagTable[] = {
		{SM_Active, MdEnabled, text31},
				// "Active"
		{SM_User, MdUser, text62},
				// "User-defined"
		{SM_Hidden, MdHidden, text400},
				// "Hidden"
		{SM_Locked, MdLocked, text366},
				// "Scope-locked"
		{'\0', 0, NULL}};

#if MMDebug & Debug_ModeDump
	dumpModes(NULL, true, "showModes() BEGIN - global modes\n");
	dumpModes(sess.cur.pBuf, true, "showModes() BEGIN - buffer modes\n");
#endif
	// Get buffer for the mode list.
	if(sysBuf(text363, &pRptBuf, BFTermAttr) != Success)
			// "Modes"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write global modes, then buffer modes.
	pModeHdr = modeHdrTable;
	do {
#if MMDebug & Debug_ModeDump
		fprintf(logfile, "*** Writing %s modes...\n", pModeHdr->mask == 0 ? "BUFFER" : "GLOBAL");
#endif
		// Write headers.
		if(pModeHdr->mask == 0 && dputs("\n\n", &rpt, 0) != 0)
			goto LibFail;
		if(rptHdr(&rpt, pModeHdr->hdr, false, text437, colWidths, &pageWidth) != Success)
				// "AUHL Name          Description"
			return sess.rtn.status;

		// Loop through mode table.
		pArray = &modeInfo.modeTable;
		while((pArrayEl = aeach(&pArray)) != NULL) {
			pModeSpec = modePtr(pArrayEl);

			// Skip if wrong type of mode.
			if((pModeSpec->flags & MdGlobal) != pModeHdr->mask)
				continue;
			pFlagInfo = flagTable;
			if(dputc('\n', &rpt, 0) != 0)
				goto LibFail;
			do {
				if(dputc((pFlagInfo->c != SM_Active || pModeHdr->mask) ? (pModeSpec->flags & pFlagInfo->flag ?
				 pFlagInfo->c : ' ') : (isBufModeSet(sess.cur.pBuf, pModeSpec) ?
				 pFlagInfo->c : ' '), &rpt, 0) != 0)
					goto LibFail;
				} while((++pFlagInfo)->descrip != NULL);
			if(dputf(&rpt, 0, "%s%-*s", space, colWidths[1].minWidth, pModeSpec->name) != 0 ||
			 (pModeSpec->descrip != NULL && dputf(&rpt, 0, "%s%s", space, pModeSpec->descrip) != 0))
				goto LibFail;
			}
		} while((++pModeHdr)->hdr != NULL);

	// Write mode groups.
#if MMDebug & Debug_ModeDump
	fputs("*** Writing mode groups...\n", logfile);
#endif
	if(dputs("\n\n", &rpt, 0) != 0)
		goto LibFail;
	sprintf(workBuf, "%s%s", text437, text438);
	if(rptHdr(&rpt, text401, false, workBuf, colWidthsMG, NULL) != Success)
			// "MODE GROUPS"
		return sess.rtn.status;
	pModeGrp = modeInfo.grpHead;
	do {
		if(dputf(&rpt, 0, "\n %c%*s%s%-*s", pModeGrp->flags & MdUser ? SM_User : ' ', colWidths[0].minWidth - 2, "",
		 space, colWidths[1].minWidth, pModeGrp->name) != 0 ||
		 (pModeGrp->descrip != NULL && dputf(&rpt, 0, "%s%s", space, pModeGrp->descrip) != 0) ||
		 dputf(&rpt, 0, "\n%*s", colWidths[0].minWidth + colWidths[1].minWidth + spacing + spacing + 4, "") != 0)
			goto LibFail;
		str = "[";
		pArray = &modeInfo.modeTable;
		while((pArrayEl = aeach(&pArray)) != NULL) {
			if((pModeSpec = modePtr(pArrayEl))->pModeGrp == pModeGrp) {
				if(dputs(str, &rpt, 0) != 0 || dputs(pModeSpec->name, &rpt, 0) != 0)
					goto LibFail;
				str = ", ";
				}
			}
		if((*str == '[' && dputc('[', &rpt, 0) != 0) || dputc(']', &rpt, 0) != 0)
			goto LibFail;
		} while((pModeGrp = pModeGrp->next) != NULL);

#if MMDebug & Debug_ModeDump
	fputs("*** Writing footnote...\n", logfile);
#endif
	// Write footnote.
	if(sepLine(pageWidth, &rpt) != 0)
		goto LibFail;
	pFlagInfo = flagTable;
	do {
		if(dputf(&rpt, 0, "\n%c ", (int) pFlagInfo->c) != 0 || dputs(pFlagInfo->descrip, &rpt, 0) != 0)
			goto LibFail;
		} while((++pFlagInfo)->descrip != NULL);

	// Add the report to the buffer.
	if(dclose(&rpt, FabStr) != 0)
		goto LibFail;
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

#if MMDebug & Debug_ModeDump
	fputs("*** Done!\n", logfile);
#endif
	// Display results.
	return render(pRtnVal, n, pRptBuf, RendNewBuf | RendRewind);
LibFail:
	return libfail();
	}

// Build and pop up a buffer containing all the strings in a ring.  Render buffer and return status.
int showRing(Datum *pRtnVal, int n, Datum **args) {
	Ring *pRing;

	if(getRingName(&pRing) != Success)
		return sess.rtn.status;

	Buffer *pRptBuf;
	DFab rpt;
	char *space = (term.cols < 96) ? " " : "  ";
	char workBuf[20];
	static ColHdrWidth colWidths[] = {
		{5, 5}, {-1, 4}, {0, 0}};

	// Get buffer for the ring list.
	{const char *src = pRing->ringName;
	char *dest = workBuf;
	do {
		*dest++ = (*src == ' ') ? upCase[(int) *++src] : *src;
		} while(*++src != '\0');
	strcpy(dest, text305);
		// "Ring"
	}
	if(sysBuf(workBuf, &pRptBuf, BFTermAttr) != Success)
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	sprintf(workBuf, "%s %s", pRing->ringName, text305);
					// "Ring"
	if(rptHdr(&rpt, workBuf, false, text330, colWidths, NULL) != Success)
			// "EntryText"
		return sess.rtn.status;

	// Loop through ring, beginning at current slot and continuing until we arrive back to where we began.  If the key
	// macro ring is being processed, display the names in bold.
	if(pRing->pEntry != NULL) {
		DFab item;
		Datum *pDatum;
		char *delim;
		RingEntry *pEntry;
		int nameLen;
		size_t dispLen, max = term.cols * 2;
		int entryNum = 0;

		pEntry = pRing->pEntry;
		do {
			if(dputf(&rpt, 0, "\n%*d %s", (int) colWidths[0].minWidth - 1, entryNum--, space) != 0)
				goto LibFail;
			if(!disnil(&pEntry->data) && strlen(pEntry->data.str) > 0) {
				if(dopentrack(&item) != 0 || esctofab(pEntry->data.str, &item) != 0 ||
				 dclose(&item, FabStr) != 0)
					goto LibFail;
				pDatum = item.pDatum;
				if((dispLen = strlen(pDatum->str)) > max)
					dispLen = max;
				if(pRing == ringTable + RingIdxMacro) {
					delim = strchr(pDatum->str + 1, *pDatum->str);
					nameLen = delim - (pDatum->str + 1);
					if(dputf(&rpt, 0, "%c~b%.*s~B%.*s", *pDatum->str, nameLen, pDatum->str + 1,
					 dispLen - nameLen, delim) != 0)
						goto LibFail;
					}
				else if(dputsubstr(pDatum->str, dispLen, &rpt, DCvtVizChar) != 0)
					goto LibFail;
				}

			// Back up to the next kill ring entry.
			} while((pEntry = pEntry->prev) != pRing->pEntry);
		}

	// Add the report to the buffer.
	if(dclose(&rpt, FabStr) != 0)
		goto LibFail;
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, n, pRptBuf, RendNewBuf | RendRewind);
LibFail:
	return libfail();
	}

// Build and pop up a buffer containing a list of all screens and their associated buffers.  Render buffer and return status.
int showScreens(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pRptBuf;
	EScreen *pScrn;			// Pointer to current screen to list.
	EWindow *pWind;			// Pointer into current screens window list.
	uint windNum;
	int pageWidth;
	DFab rpt;
	bool chgBuf = false;		// Screen has a changed buffer?
	char *space = (term.cols < 96) ? " " : "  ";
	int spacing = (term.cols < 96) ? 1 : 2;
	static ColHdrWidth colWidths[] = {
		{6, 6}, {6, 6}, {MaxBufname - 4, MaxBufname - 4}, {32, 6}, {0, 0}};

	// Get a buffer and open a fabrication object.
	if(sysBuf(text160, &pRptBuf, BFTermAttr) != Success)
			// "Screens"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rptHdr(&rpt, text160, false, text89, colWidths, &pageWidth) != Success)
			// "Screens", "ScreenWindow  Buffer              File"
		return sess.rtn.status;

	// For all screens...
	pScrn = sess.scrnHead;
	do {
		// Write separator line.
		if(pScrn->num > 1 && sepLine(pageWidth, &rpt) != 0)
			goto LibFail;

		// Store screen number and working directory.
		if(dputf(&rpt, 0, "\n~b%*hu~B  %sCWD: %s", colWidths[0].minWidth - 2, pScrn->num, space, pScrn->workDir) != 0)
			goto LibFail;

		// List screen's window numbers and buffer names.
		windNum = 0;
		pWind = pScrn->windHead;
		do {
			Buffer *pBuf = pWind->pBuf;
			++windNum;
			if(dputf(&rpt, 0, "\n%*s", colWidths[0].minWidth, "") != 0)
				goto LibFail;

			// Store window number and "changed" marker.
			if(pBuf->flags & BFChanged)
				chgBuf = true;
			if(dputf(&rpt, 0, "%s%*u  %.*s%c", space, colWidths[1].minWidth - 2, windNum, spacing - 1, space,
			 (pBuf->flags & BFChanged) ? '*' : ' ') != 0)
				goto LibFail;

			// Store buffer name and filename.
			if(pBuf->filename != NULL) {
				if(dputf(&rpt, 0, "%-*s%s%s", colWidths[2].minWidth, pBuf->bufname, space, pBuf->filename) != 0)
					goto LibFail;
				}
			else if(dputs(pBuf->bufname, &rpt, 0) != 0)
				goto LibFail;

			// On to the next window.
			} while((pWind = pWind->next) != NULL);

		// On to the next screen.
		} while((pScrn = pScrn->next) != NULL);

	// Add footnote if applicable.
	if(chgBuf) {
		if(sepLine(pageWidth, &rpt) != 0 || dputs(text243, &rpt, 0) != 0)
					// "\n* Changed buffer"
			goto LibFail;
		}

	// Add the results to the buffer.
	if(dclose(&rpt, FabStr) != 0)
LibFail:
		return libfail();
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, n, pRptBuf, RendNewBuf | RendRewind);
	}

// Get next system variable name or description and store in report-control object.  If request is SHReqNext, set *pName to item
// name, or NULL if no items left.  Return status.
int nextSysVar(ShowCtrl *pShowCtrl, ushort request, const char **pName) {
	SysVar *pSysVar;

	// First call?
	if(pShowCtrl->pItem == NULL)
		pShowCtrl->pItem = (void *) (pSysVar = sysVars);
	else {
		pSysVar = (SysVar *) pShowCtrl->pItem;
		if(request == SHReqNext)
			++pSysVar;
		}

	// Process request.
	switch(request) {
		case SHReqNext:
			for(; pSysVar->name != NULL; ++pSysVar) {

				// Found variable... return its name.
				*pName = pSysVar->name;
				pShowCtrl->pItem = (void *) pSysVar;
				goto Retn;
				}

			// End of list.
			*pName = NULL;
			break;
		case SHReqSynop:
			// Set variable name and description.
			if(dsetstr(pSysVar->name, &pShowCtrl->name) != 0)
				goto LibFail;
			pShowCtrl->descrip = (char *) pSysVar->descrip;
			*pName = pSysVar->name;
			break;
		default: // SHReqValue
			if(pSysVar->flags & (V_GetKey | V_GetKeySeq)) {
				Datum *pDatum;

				if(dnewtrack(&pDatum) != 0)
					goto LibFail;
				if(getSysVar(pDatum, pSysVar) == Success &&
				 dputf(&pShowCtrl->rpt, 0, "~#u%s~U", pDatum->str) != 0)
LibFail:
					(void) libfail();
				}
			else
				(void) svtofab(pSysVar, true, &pShowCtrl->rpt);
		}
Retn:
	return sess.rtn.status;
	}

// Get next user variable name or description and store in report-control object.  If request is SHReqNext, set *pName to item
// name, or NULL if no items left.  Return status.
static int nextUserVar(ShowCtrl *pShowCtrl, ushort request, const char **pName, UserVar *pUserVar) {
	UserVar *pUserVar1;

	// First call?
	if(pShowCtrl->pItem == NULL)
		pShowCtrl->pItem = (void *) (pUserVar1 = pUserVar);
	else {
		pUserVar1 = (UserVar *) pShowCtrl->pItem;
		if(request == SHReqNext)
			pUserVar1 = pUserVar1->next;
		}

	// Process request.
	switch(request) {
		case SHReqNext:
			for(; pUserVar1 != NULL; pUserVar1 = pUserVar1->next) {

				// Found variable... return its name.
				*pName = pUserVar1->name;
				pShowCtrl->pItem = (void *) pUserVar1;
				goto Retn;
				}

			// End of list.
			*pName = NULL;
			break;
		case SHReqSynop:
			if(dsetstr(pUserVar1->name, &pShowCtrl->name) != 0)
				return libfail();
			*pName = pUserVar1->name;
			break;
		default: // SHReqValue
			if(dtofabattr(pUserVar1->pValue, &pShowCtrl->rpt, NULL, DCvtLang) < 0)
				return libfail();
		}
Retn:
	return sess.rtn.status;
	}

// Get next global variable name or description via call to nextUserVar().
int nextGlobalVar(ShowCtrl *pShowCtrl, ushort request, const char **pName) {

	return nextUserVar(pShowCtrl, request, pName, globalVarRoot);
	}

// Get next local variable name or description via call to nextUserVar().
int nextLocalVar(ShowCtrl *pShowCtrl, ushort request, const char **pName) {

	return nextUserVar(pShowCtrl, request, pName, localVarRoot);
	}

// Create formatted list of system and user variables via calls to "show" routines.  Return status.
int showVariables(Datum *pRtnVal, int n, Datum **args) {
	ShowCtrl showCtrl;

	// Open control object.
	if(showOpen(&showCtrl, n, text292, args) != Success)
			// "variable"
		return sess.rtn.status;

	// Do system variables.
	if(showBuild(&showCtrl, SHSepLine, text21, nextSysVar) != Success)
				// "system variable"
		return sess.rtn.status;

	// Do user variables.
	if(showBuild(&showCtrl, SHNoDesc, text56, nextGlobalVar) == Success &&
				// "user variable"
	 showBuild(&showCtrl, 0, NULL, nextLocalVar) == Success)
		(void) showClose(pRtnVal, n, &showCtrl);
	return sess.rtn.status;
	}

// Create formatted list of commands, functions, aliases, and variables which match a pattern via calls to "show" routines.
// Return status.
int apropos(Datum *pRtnVal, int n, Datum **args) {
	ShowCtrl showCtrl;

	// Get options and set flags.
	if(getCFOpts(&showCtrl, n, args, text803) == Success) {
				// "name"

		// Open control object.
		if(showOpen(&showCtrl, n, text803, args) == Success) {
				// "name"

			// Call the various show routines and build the list.
			if((!selSystem || showBuild(&showCtrl, SHSepLine, text414, nextSysCommand) == Success) &&
							// "system command"
			 (!selUser || showBuild(&showCtrl, SHSepLine, text417, nextUserCommand) == Success) &&
							// "user command"
			 (!selSystem || showBuild(&showCtrl, SHSepLine, text336, nextSysFunction) == Success) &&
							// "system function"
			 (!selUser || showBuild(&showCtrl, SHSepLine, text418, nextUserFunction) == Success) &&
							// "user function"
			 showBuild(&showCtrl, SHNoDesc, text127, nextAlias) == Success &&
							// "alias"
			 showBuild(&showCtrl, SHSepLine, text21, nextSysVar) == Success &&
							// "system variable"
			 showBuild(&showCtrl, SHNoDesc, text56, nextGlobalVar) == Success &&
							// "user variable"
			 showBuild(&showCtrl, SHNoDesc, NULL, nextLocalVar) == Success)
				(void) showClose(pRtnVal, n, &showCtrl);
			}
		}
	return sess.rtn.status;
	}

#if MMDebug & Debug_ShowRE
/*** NOTE: The following function is old code and needs to be updated to compile and run properly. ***/

// Build and pop up a buffer containing the compiled search and replacement patterns.  Render buffer and return status.
int showRegexp(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pRegList;
	DFab rpt;
	char patBuf[bufSearch.match.patLen + OptCh_N + 1];
	struct RegInfo {
		char *hdr;
		char *pat;
		regex_t *compPat;
		} *pRegInfo;
	static struct RegInfo regInfoTable[] = {
		{text997, NULL, NULL},
			// "Forward"
		{text998, NULL, NULL},
			// "Backward"
		{NULL, NULL, NULL}};

	// Get a buffer and open a fabrication object.
	if(sysBuf(text996, &pRegList, 0) != Success)
			// "RegexpInfo"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;

	makePat(patBuf, &bufSearch.match);
	if(dputf(&rpt, 0, "Match flags: %.4x\n", bufSearch.match.flags) != 0 ||
	 dputf(&rpt, 0, "%s %s /", text994, text999) != 0 || dputs(patBuf, &rpt, DCvtVizChar) != 0 ||
	 dputs("/\n", &rpt, 0) != 0)
				// "SEARCH", "PATTERN"
		goto LibFail;
	if(bufSearch.match.flags & SCpl_ForwardRE) {
		regInfoTable[0].pat = bufSearch.match.pat;
		regInfoTable[0].compPat = &bufSearch.match.regPat.compPat;
		regInfoTable[1].pat = bufSearch.match.regPat.backPat;
		regInfoTable[1].compPat = &bufSearch.match.regPat.compBackPat;
		pRegInfo = regInfoTable;

		do {
			if(dputf(&rpt, 0, "\n%s%s /", pRegInfo->hdr, text311) != 0 || dputs(pRegInfo->pat, &rpt,
						// " pattern"
			 DCvtVizChar) != 0 || dputs("/\n", &rpt, 0) != 0)
				goto LibFail;
			if((pRegInfo->pat == bufSearch.match.pat || (bufSearch.match.flags & SCpl_BackwardRE)) &&
			 dputf(&rpt, 0, "Group count: %lu\n", pRegInfo->compPat->re_nsub) != 0)
				goto LibFail;
			} while((++pRegInfo)->hdr != NULL);
		}
#if 0
	// Construct the replacement header lines.
	if(dputf(&rpt, 0, "\n\n%s %s /", text995, text999) != 0 ||
	 dputs(bufSearch.match.replPat, &rpt, DCvtVizChar) != 0 || dputs("/\n", &rpt, 0) != 0)
				// "REPLACE", "PATTERN"
		goto LibFail;

	// Loop through replacement pattern.
	if((pReplPat = bufSearch.match.compReplPat) != NULL) {
		char *str, workBuf[WorkBufSize];
		do {
			str = stpcpy(workBuf, "    ");

			// Describe metacharacter and any value.
			switch(pReplPat->type) {
				case RPE_Nil:
					strcpy(str, "NIL");
					break;
				case RPE_LitString:
					if(dputs(workBuf, &rpt, 0) != 0 || dputf(&rpt, 0, "%-14s'", "String") != 0 ||
					 dputs(pReplPat->u.replStr, &rpt, DCvtVizChar) != 0 || dputs("'\n", &rpt, 0) != 0)
						goto LibFail;
					str = NULL;
					break;
				case RPE_GrpMatch:
					sprintf(str, "%-14s%3d", "Group", pReplPat->u.grpNum);
				}

			// Add the line to the fabrication object.
			if(str != NULL && (dputs(workBuf, &rpt, 0) != 0 || dputc('\n', &rpt, 0) != 0))
				goto LibFail;

			// Onward to the next element.
			} while((pReplPat = pReplPat->next) != NULL);
		}
#endif
	// Add the results to the buffer.
	if(dclose(&rpt, FabStr) != 0)
LibFail:
		return libfail();
	if(bappend(pRegList, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, n, pRegList, RendNewBuf | RendRewind);
	}
#endif

// Write a string to an open fabrication object, centered in the current terminal width.  Return status.
static int center(DFab *rpt, const char *str) {
	int indent = (term.cols - (strlen(str) - attrCount(str, -1, INT_MAX))) / 2;
	if(dputc('\n', rpt, 0) != 0 || (indent > 0 && dputf(rpt, 0, "%*s", indent, "") != 0) || dputs(str, rpt, 0) != 0)
		(void) libfail();
	return sess.rtn.status;
	}

// Build and pop up a buffer containing "about the editor" information.  Render buffer and return status.
int aboutMM(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pRptBuf;
	DFab rpt;
	char *str;
	int len1, len2, maxLabelLen;
	char footer1[sizeof(ProgName) + sizeof(ALit_Footer1) + 1];	// Longest line displayed.
	char workBuf1[term.maxCols + 1];
	char workBuf2[32];
	struct LimitInfo {
		char *label;
		int value;
		} *pLimitInfo;
	static struct LimitInfo limits[] = {
		{ALit_MaxCols, TTY_MaxCols},
		{ALit_MaxRows, TTY_MaxRows},
		{ALit_MaxTabSize, MaxTabSize},
		{ALit_MaxPathname, MaxPathname},
		{ALit_MaxBufname, MaxBufname},
		{ALit_MaxModeGrpName, MaxModeGrpName},
		{ALit_MaxUserVar, MaxVarName},
		{ALit_MaxTermInp, MaxTermInp},
		{ALit_LibInternals, -1},
		{ALit_LibRegexp, -2},
		{NULL, 0}};

	// Get new buffer and fabrication object.
	if(sysBuf(text6, &pRptBuf, BFTermAttr) != Success)
			// "About"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Construct the "About MightEMacs" lines.
	sprintf(footer1, "%s%s", Myself, ALit_Footer1);
			// " runs on Unix and Linux platforms and is licensed under the"

	if(dputs("\n\n", &rpt, 0) != 0)					// Vertical space.
		goto LibFail;
	if(rptHdr(&rpt, Myself, false, footer1, NULL, NULL) != Success)	// Editor name in color.
		return sess.rtn.status;
	if(dputc('\n', &rpt, 0) != 0)					// Blank line.
		goto LibFail;
	sprintf(workBuf1, "%s %s", text185, Version);			// Build "version" line...
			// "Version"
	if(center(&rpt, workBuf1) != Success)				// and center it.
		return sess.rtn.status;
	if(dputc('\n', &rpt, 0) != 0)					// Blank line.
		goto LibFail;
	if(center(&rpt, ALit_Author) != Success)			// Copyright.
			// "(c) Copyright 2022 Richard W. Marinelli <italian389@yahoo.com>"
		return sess.rtn.status;

	// Construct the program limits' lines.
	if(dputs("\n\n", &rpt, 0) != 0)					// Blank lines.
		goto LibFail;
	sprintf(workBuf1, "~b%s~B", ALit_BuildInfo);			// Limits' header.
		// "Build Information"
	if(center(&rpt, workBuf1) != Success)
		return sess.rtn.status;
	if(dputc('\n', &rpt, 0) != 0)					// Blank line.
		goto LibFail;

	// Find longest limits' label and set len1 to indentation required to center it and its value.
	pLimitInfo = limits;
	maxLabelLen = 0;
	do {
		if((len1 = strlen(pLimitInfo->label)) > maxLabelLen)
			maxLabelLen = len1;
		} while((++pLimitInfo)->label != NULL);
	if((len1 = (term.cols - (maxLabelLen + 10)) / 2) < 0)
		len1 = 0;
	++maxLabelLen;

	// Loop through limit values and add to report.
	str = dupChar(workBuf1, ' ', len1);
	pLimitInfo = limits;
	do {
		len1 = strlen(pLimitInfo->label);
		if(pLimitInfo->value >= 0)
			sprintf(str, "%-*s%9d", maxLabelLen, pLimitInfo->label, pLimitInfo->value);
		else {
			strcpy(workBuf2, pLimitInfo->value == -1 ? cxlvers() : xrevers());
			strchr(workBuf2, '(')[-1] = '\0';
			len2 = strlen(workBuf2);
			sprintf(str, "%-*s%s", maxLabelLen + 9 - len2, pLimitInfo->label, workBuf2);
			}
		str[len1] = ':';
		if((pLimitInfo->value == -1 && dputc('\n', &rpt, 0) != 0) || dputc('\n', &rpt, 0) != 0 ||
		 dputs(workBuf1, &rpt, 0) != 0)
			goto LibFail;
		} while((++pLimitInfo)->label != NULL);

	// Write platform, credit, and license blurb.
	if(dputc('\n', &rpt, 0) != 0)					// Blank line.
		goto LibFail;
	if(center(&rpt, footer1) != Success || center(&rpt, ALit_Footer2) != Success || center(&rpt, ALit_Footer3) != Success)
			// "GNU General Public License (GPLv3), which can be viewed at"
			// "http://www.gnu.org/licenses/gpl-3.0.en.html"
		return sess.rtn.status;

	// Add the results to the buffer.
	if(dclose(&rpt, FabStr) != 0)
		goto LibFail;
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, n, pRptBuf, RendNewBuf | RendRewind);
LibFail:
	return libfail();
	}
