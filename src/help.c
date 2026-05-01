// (c) Copyright 2026 Richard W. Marinelli
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
#include "file.h"
#include "search.h"
#include "var.h"

// getInfo() item.
typedef struct {
	const char *keyword;
	char *value;
	ushort flags;
	ushort ahSize;
	} InfoItem;

// Control parameters and flags for "show" routines.
#define ValueCol	34		// Report column for value parameter.

typedef struct {
	Buffer *pRptBuf;		// Buffer to put list into.
	DFab rpt;			// Report object.
	const char **pNameList;		// Optional pointer to sorted name list.
	const char *separator;		// Separator line in report.
	Datum name;			// Name of CFA or variable plus arguments, if applicable.
	Datum value;			// Item value (or nil), placed on same line as name at tab stop.
	const char *descrip;		// Item description (or NULL), placed on next line and indented.
	Datum pat;			// Match pattern for apropos.
	Match match;			// Match record for apropos.
	Datum src;			// Source string for strIndex() apropos matching.
	Datum index;			// Result from strIndex() call.
	} ShowCtrl;

// Request types and operation flags used by showbuild().
#define ShReqBegin	1		// Initialize build items in control object and write title to report.
#define ShReqWrName	2		// Write item's name + arguments to report.
#define ShReqEnd	3		// Write completed report to buffer.
#define ShReqMask	0x00ff

#define ShOp_SepLine	0x00000100	// Insert separator line between items.
#define ShOp_NoTitle	0x00000200	// Report section has no title (local variables).
#define ShOp_Force	0x00000400	// Force save of section text to report buffer (global constants and variables).
#define ShOp_Exact	0x00000800	// Apropos pattern must match exactly.
#define ShOp_FirstItem	0x00001000	// Processing first list item.
#define ShOp_HasValue	0x00002000	// Current item has a value.
#define ShOp_Command	0x00004000	// Process command(s), otherwise function(s).

// getInfo() parameters.
#define GIArray		0x1000		// Build array at top level.
#define GIHash		0x2000		// Build hash at top level.

#define GI_Bindings	0 | GIHash
#define GI_Buffers	1 | GIArray
#define GI_Colors	2 | GIHash
#define GI_Date		3
#define GI_Defaults	4 | GIHash
#define GI_Hooks	5 | GIHash
#define GI_MacroCount	6
#define GI_Modes	7 | GIHash
#define GI_RingNames	8 | GIArray
#define GI_Screens	9 | GIArray
#define GI_Version	10
#define GI_Windows	11 | GIArray
#define GI_ItemMask	0x00ff

#ifndef OSName
// Get OS name for getInfo function and return it in pRtnVal.  Return status.
static int getOS(Datum *pRtnVal) {
	static const char myName[] = "getOS";
	static char *osName = NULL;
	const char *name;

	// Get uname() info if first call.
	if((name = osName) == NULL) {
		struct stat s;
		struct utsname utsname;
		struct OSInfo {
			const char *key;
			const char *name;
			} *pOSInfo, *pOSInfoEnd;
		static struct OSInfo osTable[] = {
			{VerKey_MacOS, OSName_MacOS},
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
		if(stat(CentOS_Release, &s) == 0)
			name = OSName_CentOS;
		else if(stat(RedHat_Release, &s) == 0)
			name = OSName_RedHat;
		else
			// Nope.  Use system name from uname() call.
			name = utsname.sysname;
Save:
		// Save OS name on heap for future calls, if any.
		if(strclone(&osName, name, myName) != Success)
			return sess.rtn.status;
		}

	return dsetstr(name, pRtnVal) != 0 ? libfail() : sess.rtn.status;
	}
#endif

// Build hash of form [bufname], [bufname, filename, homeDir, bytes, lines], or [bufname, filename, homeDir, bytes, lines,
// [buf-attr, ...], [buf-mode, ...]] and return it in pRtnVal.  Return status.

// Build hash of form {"Filename" => filename, "HomeDir" => dir, "Size" => n} for given buffer and return it.  If n argument,
// add {"Lines" => n, "Attributes" => [...], "Modes" => [...]} also.  Return status.
int bufInfo(Datum *pRtnVal, long n, Datum **args) {
	Hash *pHash;
	HashNode *pNode;
	Buffer *pBuf;
	int lineCount;
	struct BufInfoItem {
		const char *key;
		union {
			const char *str;
			long i;
			} u;
		} *pItem;
	static struct BufInfoItem nodes[] = {
		{text520, NULL},	// "Filename"
		{text521, NULL},	// "HomeDir"
		{text522, NULL},	// "Size"
		{text519, NULL},	// "Lines"
		{NULL, NULL}};

	// Get options and set buffer flags.
	if(makeHash(pRtnVal, n == DefN ? 5 : 7, &pHash) != Success)
		return sess.rtn.status;

	// Get buffer and activate it if needed.
	if((pBuf = findBuf(args[0])) == NULL || (!(pBuf->flags & BFActive) && bactivate(pBuf) != Success))
		return sess.rtn.status;

	// Set up table.
	nodes[0].u.str = pBuf->filename;
	nodes[1].u.str = pBuf->homeDir;
	nodes[2].u.i = bufLength(pBuf, &lineCount);
	nodes[3].u.i = lineCount;

	// Create hash nodes.
	pItem = nodes;
	do {
		if((pNode = hnewnode(pHash, pItem->key, NULL)) == NULL)
			goto LibFail;
		if(pItem - nodes > 1)
			dsetint(pItem->u.i, pNode->pVal);
		else if(pItem->u.str != NULL && dsetstr(pItem->u.str, pNode->pVal) != 0)
			goto LibFail;
		} while((++pItem)->key != NULL);

	// Extend hash if requested.
	if(n != DefN) {
		Array *pArray;
		Datum *pArrayEl;
		Option *pOpt, *pOptEnd;

		// Store buffer attributes and modes.
		if((pNode = hnewnode(pHash, text523, NULL)) == NULL)
					// "Attributes"
			goto LibFail;
		if(makeArray(pNode->pVal, 0, &pArray) != Success)
			return sess.rtn.status;

		// Get buffer attributes.
		pOptEnd = (pOpt = bufAttrTable) + bufAttrHdr.optCount;
		do {
			if(pBuf->flags & pOpt->u.value)
				if((pArrayEl = aget(pArray, pArray->used, OpGrow)) == NULL ||
				 dsetstr(*pOpt->keyword == '^' ? pOpt->keyword + 1 : pOpt->keyword, pArrayEl) != 0)
					goto LibFail;
			} while(++pOpt < pOptEnd);

		// Get buffer modes.
		if((pNode = hnewnode(pHash, text363, NULL)) == NULL)
					// "Modes"
			goto LibFail;
		(void) getModes(pNode->pVal, pBuf);
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Make hash node for a key binding.  Return status code.
static int makeBindNode(Hash *pHash, KeyBind *pKeyBind, bool force) {
	HashNode *pNode;
	const char *name = getBindCmd(pKeyBind);
	char keyBuf[16];

	if(force || name != NULL) {
		ektos(keyBuf, pKeyBind->extKey, false);
		if((pNode = hnewnode(pHash, keyBuf, NULL)) == NULL)
			return -1;
		if(name != NULL && dsetstr(name, pNode->pVal) != 0)
			return -1;
		}

	return 0;
	}

// binSearch() helper function for returning a getInfo keyword, given table (array) and index.
static const char *getInfoKeyword(const void *table, ssize_t i) {

	return ((InfoItem *) table)[i].keyword;
	}

// Store screen pointers in given table in screen number order.
static void sortScreens(EScreen **ppScrn, int count) {
	EScreen *pScrn = sess.scrnHead;
	do {
		ppScrn[pScrn->num - 1] = pScrn;
		} while((pScrn = pScrn->next) != NULL);
	ppScrn[count] = NULL;
	}

// Get informational item per keyword argument.  Return status.
int getInfo(Datum *pRtnVal, long n, Datum **args) {
	static InfoItem infoTable[] = {
		{"Bindings", NULL, GI_Bindings, 37},	// {key-lit => cmd-name, ...}
		{"Buffers", NULL, GI_Buffers, 0},	// [bufname, ...]
		{"Colors", NULL, GI_Colors, 7},		// {"ColorRange" => [0, 255], ...}
		{"Date", NULL, GI_Date, 0},
		{"Defaults", NULL, GI_Defaults, 5},	// {"HardTabSize" => -1, "SoftTabSize" => -1, "WrapCol" => 128}
/*Const*/	{"Editor", Myself, 0, 0},
		{"Hooks", NULL, GI_Hooks, 13},		// {hook-name => function-name, ...}
/*Const*/	{"Language", Language, 0},
		{"MacroCount", NULL, GI_MacroCount, 0},
		{"Modes", NULL, GI_Modes, 0},		// {mode-name => {"GroupName" => name, "User?" => Boolean, ...}, ...}
#ifdef OSName
/*Const*/	{"OS", OSName, 0, 0},
#else
		{"OS", NULL, 0, 0},
#endif
		{"RingNames", NULL, GI_RingNames, 0},	// [name, ...]
		{"Screens", NULL, GI_Screens, 0},	// [[screen-num, wind-count, work-dir], ...]
		{"Version", NULL, GI_Version, 0},
		{"Windows", NULL, GI_Windows, 0}};	// [[scrnNum, windNum, bufname], ...]
	ssize_t i;
	InfoItem *pItem;
	Hash *pHash;
	HashNode *pNode;
	Array *pArray, *pArray1;
	Datum *pArrayEl, **ppArrayEl;
	EScreen *pScrn;
	char *keyword = args[0]->str;

	// Search table for a match.
	if(!binSearch(keyword, (const void *) infoTable, elementsof(infoTable), strcasecmp, getInfoKeyword, &i)) {

		// Match not found.
		return rsset(Failure, 0, text447, text450, keyword);
				// "Invalid %s '%s'", "type keyword"
		}

	// Match found.  Check keyword type.
	pItem = infoTable + i;
	if(pItem->value != NULL)
		return dsetstr(pItem->value, pRtnVal) != 0 ? libfail() : sess.rtn.status;
#ifndef OSName
	if(strcasecmp(pItem->keyword, "OS") == 0)
		return getOS(pRtnVal);
#endif
	// Not a string constant value.  Create top level array or hash, if applicable.
	if(pItem->flags & GIArray) {
		if(makeArray(pRtnVal, pItem->ahSize, &pArray) != Success)
			goto Retn;
		}
	else if(pItem->flags & GIHash && makeHash(pRtnVal, pItem->ahSize, &pHash) != Success)
		goto Retn;

	// Get requested data.
	switch(pItem->flags & GI_ItemMask) {
		case GI_Bindings & GI_ItemMask:;	// {key-lit => cmd-name, ...}
			KeyBind *pKeyBind = (KeyBind *) keyBindTable;
			do {
				if(makeBindNode(pHash, pKeyBind, n != DefN) != 0)
					goto LibFail;
				} while(++pKeyBind < (KeyBind *)(keyBindTable + KeyVectCount));
			break;
		case GI_Buffers & GI_ItemMask:;		// [bufname, ...]
			HashEach each;
			Buffer *pBuf;

			heachinit(&each, &buf.table);
			while((pNode = heach(&each)) != NULL) {
				pBuf = bufPtr(pNode->pVal);
				if(!(pBuf->flags & (BFCommand | BFFunc)) && (n != DefN || !(pBuf->flags & BFHidden)))
					if((pArrayEl = aget(pArray, pArray->used, OpGrow)) == NULL ||
					 dsetstr(pBuf->bufname, pArrayEl) != 0)
						goto LibFail;
				}
			break;
		case GI_Colors & GI_ItemMask:;		// {"ColorRange" => [0, 255], "PairRange" => [1, 255],
							//  "Help" => [15, 28], "ModeLine" => [15, 88],
							// "Record" => [15, 9]}
			ItemColor *pItemColor, *pItemColorEnd;

			// Loop through color items and make list.  For each item, create array in pArray1 and store in hash.
			// Also create "ColorRange" and "PairRange" two-elements arrays.
			pItemColorEnd = (pItemColor = term.itemColors) + sizeof(term.itemColors) / sizeof(ItemColor);
			do {
				if((pNode = hnewnode(pHash, pItemColor->name, NULL)) == NULL)
					goto LibFail;
				if(pItemColor->colors[0] >= -1) {
					if((pArray1 = anew(2, NULL)) == NULL)
						goto LibFail;
					dsetint(pItemColor->colors[0], pArray1->elements[0]);
					dsetint(pItemColor->colors[1], pArray1->elements[1]);
					astash(pNode->pVal, pArray1);
					}
				} while(++pItemColor < pItemColorEnd);

			// Create color range array.
			if((pArray1 = anew(2, NULL)) == NULL || (pNode = hnewnode(pHash, "ColorRange", NULL)) == NULL)
				goto LibFail;
			dsetint(0, pArray1->elements[0]);
			dsetint(term.maxColor, pArray1->elements[1]);
			astash(pNode->pVal, pArray1);

			// Create pair range array.
			if((pArray1 = anew(2, NULL)) == NULL || (pNode = hnewnode(pHash, "PairRange", NULL)) == NULL)
				goto LibFail;
			dsetint(1, pArray1->elements[0]);
			dsetint(term.maxWorkPair, pArray1->elements[1]);
			astash(pNode->pVal, pArray1);
			break;
		case GI_Date & GI_ItemMask:
			if(dsetstr(strTime(), pRtnVal) != 0)
				goto LibFail;
			break;
		case GI_Defaults & GI_ItemMask:;	// {param-name => value, ...}
			Option *pOpt, *pOptEnd;

			// Loop through defaults (options) table and store each parameter in hash.
			pOptEnd = (pOpt = defOptions) + defOptHdr.optCount;
			do {
				if((pNode = hnewnode(pHash, pOpt->keyword, NULL)) == NULL)
					goto LibFail;
				dsetint(pOpt->u.svalue, pNode->pVal);
				} while(++pOpt < pOptEnd);
			break;
		case GI_Hooks & GI_ItemMask:;		// {hook-name => function-name, ...}
			HookRec *pHookRec = hookTable;

			// Loop through hook table.  For each hook, make node in hash with name as key and function name as
			// value.
			do {
				if((pNode = hnewnode(pHash, pHookRec->name, NULL)) == NULL || (pHookRec->func.type != PtrNull &&
				 dsetstr(hookFuncName(&pHookRec->func), pNode->pVal) != 0))
					goto LibFail;
				} while((++pHookRec)->name != NULL);
			break;
		case GI_MacroCount & GI_ItemMask:
			dsetint(macro.index.table.nodeCount, pRtnVal);
			break;
		case GI_Modes & GI_ItemMask:;		// {mode-name => {"GroupName" => name or nil,
							// "User?" => Boolean, "Global?" => Boolean, ...}}
			Hash *pHash1;
			ModeSpec *pModeSpec;
			HashNode *pAryNode, **pAry;
			struct ModeFlagInfo {
				const char *name;
				ushort mflag;
				} *pInfo;
			static struct ModeFlagInfo modeFlagInfo[] = {
				{"User?", MdUser},
				{"Global?", MdGlobal},
				{"Hidden?", MdHidden},
				{"ScopeLocked?", MdLocked},
				{"Active?", 0}};

			// Loop through mode table.  For each mode, create hash of parameters and save in main hash with mode
			// name as key and hash as value.
			if(ohAryPrep(&mode.idx, &pAry) != 0)
				goto LibFail;
			while((pAryNode = *pAry++) != NULL) {
				pModeSpec = modePtr(pAryNode->pVal);
				if((pHash1 = hnew(7, 0)) == NULL || (pNode = hnewnode(pHash, pModeSpec->name, NULL)) == NULL)
					goto LibFail;
				hstash(pNode->pVal, pHash1);
				if((pNode = hnewnode(pHash1, "GroupName", NULL)) == NULL)
					goto LibFail;
				if(pModeSpec->pModeGrp != NULL)
					if(dsetstr(pModeSpec->pModeGrp->name, pNode->pVal) != 0)
						goto LibFail;
				pInfo = modeFlagInfo;
				for(;;) {
					if((pNode = hnewnode(pHash1, pInfo->name, NULL)) == NULL)
						goto LibFail;
					if(pInfo->mflag == 0)
						break;
					dsetbool(pModeSpec->flags & pInfo->mflag, pNode->pVal);
					++pInfo;
					}
				dsetbool(pModeSpec->flags & MdGlobal ? pModeSpec->flags & MdActive :
				 isBufModeSet(sess.cur.pBuf, pModeSpec), pNode->pVal);
				}
			break;
		case GI_RingNames & GI_ItemMask:;	// RingNames: [name, ...]
			Ring *pRing, *pRingEnd;

			pRingEnd = (pRing = ringTable) + ringTableSize;
			do {
				if((pArrayEl = aget(pArray, pArray->used, OpGrow)) == NULL ||
				 dsetstr(pRing->ringName, pArrayEl) != 0)
					goto LibFail;
				} while(++pRing < pRingEnd);
			break;
		case GI_Screens & GI_ItemMask:		// [[screen-num, wind-count, work-dir], ...]

			// Loop through screens and make list.  For each screen, create array and push onto pArray.
			{int scrnTotal = scrnCount();
			EScreen *scrnTable[scrnTotal + 1], **ppScrn = scrnTable;
			sortScreens(scrnTable, scrnTotal);
			do {
				pScrn = *ppScrn++;
				if((pArray1 = anew(3, NULL)) == NULL)
					goto LibFail;
				ppArrayEl = pArray1->elements;
				dsetint((long) pScrn->num, *ppArrayEl++);
				dsetint((long) getWindCount(pScrn, NULL), *ppArrayEl++);
				if(dsetstr(pScrn->workDir, *ppArrayEl) != 0 ||
				 (pArrayEl = aget(pArray, pArray->used, OpGrow)) == NULL)
					goto LibFail;
				astash(pArrayEl, pArray1);
				} while(*ppScrn != NULL);
			}
			break;
		case GI_Version & GI_ItemMask:
			if(n == DefN) {
				if(dsetstr(Version, pRtnVal) != 0)
					goto LibFail;
				}
			else {
				char *info;

				if(helpInfo(&info, h_version, NULL) == Success)
					dadoptstr(info, pRtnVal);
				}
			break;
		case GI_Windows & GI_ItemMask:;		// [[scrnNum, windNum, bufname], ...]]
			{EWindow *pWind;
			long windNum;
			int scrnTotal = scrnCount();
			EScreen *scrnTable[scrnTotal + 1], **ppScrn = scrnTable;

			// Loop through screens and make list.  For each window, create array and push onto pArray.
			sortScreens(scrnTable, scrnTotal);
			do {
				pScrn = *ppScrn++;
				// In all windows...
				windNum = 0;
				pWind = pScrn->windHead;
				do {
					++windNum;
					if((pArray1 = anew(3, NULL)) == NULL)
						goto LibFail;
					ppArrayEl = pArray1->elements;
					dsetint((long) pScrn->num, *ppArrayEl++);
					dsetint(windNum, *ppArrayEl++);
					if(dsetstr(pWind->pBuf->bufname, *ppArrayEl) != 0 ||
					 (pArrayEl = aget(pArray, pArray->used, OpGrow)) == NULL)
						goto LibFail;
					astash(pArrayEl, pArray1);
					} while((pWind = pWind->next) != NULL);
				} while(*ppScrn != NULL);
			}
			break;
		}
Retn:
	// Return results.
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// If default n, display the current line, column, and character position of the point in the current buffer, the fraction of
// the text that is before the point, and the character that is at point (in printable form and hex).  If n is not the
// default, display the point column and the character at point only.  The displayed column is not the current column,
// but the column on an infinite-width display.  *Interactive only*
int showPoint(Datum *pRtnVal, long n, Datum **args) {
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
	char *pct, *strChar, workBuf1[32], workBuf2[32];

	pct = strcpy(workBuf2, "0.0");
	if(!bempty(NULL)) {
		if(n == DefN) {

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
			sprintf(pct = workBuf2, "%.1f", ratio);
			if(numChars > 0) {

				// Fix rounding errors at buffer boundaries.
				if(preChars > 0 && strcmp(pct, "0.0") == 0)
					pct = "0.1";
				else if(preChars < numChars && strcmp(pct, "100.0") == 0)
					pct = "99.9";
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
	strChar = workBuf1;
	if(curChar >= ' ' && curChar < 0x7f) {
		sprintf(workBuf1, curChar == AttrSpecBegin ? "'~%c' " : "'%c' ", (int) curChar);
		strChar = strchr(workBuf1, '\0');
		}
	sprintf(strChar, "0x%.2hX", curChar);
	if((n == DefN ? asprintf(&info, text60, curLine, numLines, col, endCol, preChars, numChars, pct, workBuf1) :
			// "~bLine:~B %'lu of %'lu; ~bCol:~B %'d of %'d; ~bByte:~B %'lu of %'lu (%s%%); ~bchar~B = %s"
	 asprintf(&info, text340, col, endCol, workBuf1)) < 0)
			// "~bCol:~B %'d of %'d; ~bchar~B = %s"
		return rsset(Panic, 0, text94, "showPoint");
			// "%s(): Out of memory!"
	(void) mlputs(info, MLHome | MLTermAttr | MLFlush);
	free((void *) info);
	return sess.rtn.status;
	}

// Set and return result from isXXX? function.
static int isResult(Datum *pRtnVal, const char *result) {

	if(result == NULL)
		dsetnil(pRtnVal);
	else if(dsetstr(result, pRtnVal) != 0)
		(void) libfail();
	return sess.rtn.status;
	}

// Check if given string argument is a buffer name and return Boolean result.
int isBufQ(Datum *pRtnVal, long n, Datum **args) {

	dsetbool(bsrch(args[0]->str, NULL) != NULL, pRtnVal);
	return sess.rtn.status;
	}

// Check if given string argument is name of a command, function, or alias (executable).  If found, return keyword result,
// otherwise nil.
int isExecQ(Datum *pRtnVal, long n, Datum **args) {
	ExecPtr exec;
	const char *result = NULL;

	// Command, pseudo-command, function, or alias?
	if(execFind(args[0]->str, OpQuery, PtrAny, &exec))
		switch(exec.type) {
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

	// Return result.
	return isResult(pRtnVal, result);
	}

// Check if given mark is defined and active in current buffer and return Boolean result.
int isMarkQ(Datum *pRtnVal, long n, Datum **args) {
	Mark *pMark;

	dsetbool(findBufMark(dgetint(args[0]), &pMark, MKQuery | MKViz) == Success && pMark != NULL, pRtnVal);
	return sess.rtn.status;
	}

// Check if given string argument is name of a mode.  If found, return "global" (global mode) or "buffer" (buffer mode),
// otherwise nil.
int isModeQ(Datum *pRtnVal, long n, Datum **args) {
	const char *result = NULL;
	ModeSpec *pModeSpec = msrch(args[0]->str);

	if(pModeSpec != NULL)
		result = pModeSpec->flags & MdGlobal ? text146 : text83;
						// "global", "buffer"
	// Return result.
	return isResult(pRtnVal, result);
	}

// Check if given string argument is name of a mode group and return Boolean result.
int isModeGrpQ(Datum *pRtnVal, long n, Datum **args) {

	dsetbool(mgsrch(args[0]->str, NULL, NULL), pRtnVal);
	return sess.rtn.status;
	}

// Check if given string argument is name of a constant or variable and return Boolean result.
int isConstVarQ(Datum *pRtnVal, long n, Datum **args) {

	dsetbool(vfind(args[0]->str, NULL, OpQuery), pRtnVal);
	return sess.rtn.status;
	}

// Display value of a constant or variable on message line.
int showConstVar(Datum *pRtnVal, long n, Datum **args) {
	Datum *pValue;
	DFab msg;
	VarDesc varDesc;

	// Get the constant or variable name.
	if(sess.opFlags & OpScript)
		dxfer(pRtnVal, args[0]);
	else if(termInp(pRtnVal, text527, ArgNotNull1 | ArgNil1, TermC_AllConstVar, NULL) != Success || disnil(pRtnVal))
			// "Show constant or variable"
		return sess.rtn.status;

	// Look it up.
	if(vfind(pRtnVal->str, &varDesc, OpDelete) == Success) {
		if((pValue = dnewtrack()) == NULL)
			goto LibFail;
		if(vderefv(pValue, &varDesc) == Success) {
			if(dopentrack(&msg) != 0 || dputf(&msg, 0, "%s = ", pRtnVal->str) != 0 ||
			 dputd(pValue, &msg, NULL, CvtLang | CvtThouSep) < 0 || dclose(&msg, FabStr) != 0)
LibFail:
				return libfail();
			(void) mlputs(msg.pDatum->str, MLHome | MLForce | MLFlush);
			}
		}
	return sess.rtn.status;
	}

// Get an apropos match string with a null default and return status.
static int getApropos(ShowCtrl *pShowCtrl, const char *itemName, Datum **args) {
	Datum *pPat = &pShowCtrl->pat;

	if(!(sess.opFlags & OpScript)) {
		char *str, workBuf[WorkBufSize];

		str = stpcpy(workBuf, text20);
				// "Apropos"
		*str++ = ' ';
		str = stpcpy(str, itemName);
		strcpy(str, str[-1] == 's' ? "es" : "s");
		if(termInp(pPat, workBuf, ArgNil1, 0, NULL) != Success)
			return sess.rtn.status;
		if(disnil(pPat))
			dsetnull(pPat);
		}
	else if(args[0] != NULL)
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

// Store character c in s len times and return pointer to terminating null.
static char *dupChar(char *s, short c, int len) {

	while(len-- > 0)
		*s++ = c;
	*s = '\0';
	return s;
	}

// Write a separator line to an open Fab object (in color if possible), given length.  If length is -1, terminal width is
// used.  Return -1 if error.
static int sepLine(int len, DFab *pFab) {

	if(len < 0)
		len = term.cols;
	char line[len + 1];
	dupChar(line, '-', len);
	if(dputc('\n', pFab, 0) != 0)
		return -1;
	return (term.itemColors[ColorIdxInfo].colors[0] >= -1) ?
	 dputf(pFab, 0, "~%dc%s~C", term.maxWorkPair - ColorPairISL, line) : dputs(line, pFab, 0);
	}

// Initialize color pairs for a "show" listing.
static void initInfoColors(void) {

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
static int showopen(ShowCtrl *pShowCtrl, char *separator, const char *promptLabel, const char *bufname, Datum **args) {

	dinit(&pShowCtrl->name);
	dinit(&pShowCtrl->value);
	dinit(&pShowCtrl->pat);
	minit(&pShowCtrl->match);
	dinit(&pShowCtrl->src);
	dinit(&pShowCtrl->index);
	dupChar(separator, '-', term.cols);
	pShowCtrl->separator = separator;

	// If args not NULL, get match string.
	if(args != NULL && getApropos(pShowCtrl, promptLabel, args) != Success)
		return sess.rtn.status;

	// Create report buffer.  If "bufname" is NULL, construct a name (plural of promptLabel in title case).
	if(sysBuf(bufname == NULL ? promptLabel : bufname, &pShowCtrl->pRptBuf, BFTermAttr | Plural) == Success)
		initInfoColors();

	return sess.rtn.status;
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
static char *inflate(char *dest, const char *src, bool plural) {

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
		if((colorPair[0] >= -1) && dputf(rpt, 0, "~%dc", term.maxWorkPair - ColorPairIH) != 0)
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
	else if(sess.opFlags & OpHaveColor && dputf(rpt, 0, "%*s~C", whitespace - leadIn, "") != 0)
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
static int storeDesc(ShowCtrl *pShowCtrl, const char *descrip) {
	int attrLen, col, tabCol, spaces;
	uint flags = TA_ScanOnly;
	char *str0, *str1, *str, *strEnd, *line, *lineEnd;
	char lineBuf[term.cols * 2];

	str0 = descrip == NULL ? (char *) pShowCtrl->descrip : (char *) descrip;

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
			if(*str == '\n')
				++str;
			else
				while(*str == ' ')
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

// Do apropos matching.  Set *pResult to true if apropos not in effect or name matches the search pattern, otherwise false.
// Return status.
static int aproposMatch(ShowCtrl *pShowCtrl, const char *name, uint flags, bool *pResult) {
	bool match;

	if(disempty(&pShowCtrl->pat))
		match = true;
	else if(flags & ShOp_Exact)
		match = strcmp(name, pShowCtrl->pat.str) == 0;
	else {
		if(dsetstr(name, &pShowCtrl->src) != 0)
			return libfail();
		if(strIndex(&pShowCtrl->index, 0, &pShowCtrl->src, &pShowCtrl->pat, &pShowCtrl->match) != Success)
			return sess.rtn.status;
		match = !disnil(&pShowCtrl->index);
		}
	*pResult = match;
	return sess.rtn.status;
	}

// Build a "show" listing in a report buffer, given ShowCtrl object, flags, section title (which may be NULL), and pointer to
// routine which sets various components for the next list item in the ShowCtrl object.  Requests to this routine are:
static int showbuild(ShowCtrl *pShowCtrl, uint flags, const char *text) {
	char *str;
	short *colorPair = term.itemColors[ColorIdxInfo].colors;
	char workBuf[WorkBufSize];

	// Process request.
	switch(flags & ShReqMask) {
		case ShReqBegin:	// Do report section initialization.
			pShowCtrl->descrip = NULL;

			// Open a fabrication object and write section header, if applicable.
			if(dopentrack(&pShowCtrl->rpt) != 0)
				goto LibFail;
			if(!(flags & ShOp_NoTitle) && showHdr(pShowCtrl, text) != Success)
				return sess.rtn.status;
			break;
		case ShReqWrName:	// Write item's name to report and optionally, tab out to value column.

			// Begin next line.
			if(flags & (ShOp_SepLine | ShOp_FirstItem)) {
				if(!(flags & ShOp_NoTitle) && dputc('\n', &pShowCtrl->rpt, 0) != 0)
					goto LibFail;
				if(colorPair[0] >= -1 && dputf(&pShowCtrl->rpt, 0, "~%dc",
				 term.maxWorkPair - ColorPairISL) != 0)
					goto LibFail;
				if(dputs(pShowCtrl->separator, &pShowCtrl->rpt, 0) != 0)
					goto LibFail;
				if(colorPair[0] >= -1 && dputs("~C", &pShowCtrl->rpt, 0) != 0)
					goto LibFail;
				}

			// Store item name and trailing space, if needed, in work buffer and add line to report.
			sprintf(workBuf, "~b%s~B", pShowCtrl->name.str);
			if(flags & ShOp_HasValue) {
				str = pad(workBuf, ValueCol);
				if(str[-2] != ' ')
					strcpy(str, str[-1] == ' ' ? " " : "  ");
				}
			if(dputc('\n', &pShowCtrl->rpt, 0) != 0 || dputs(workBuf, &pShowCtrl->rpt, 0) != 0)
				goto LibFail;
			break;
		default:	// ShReqEnd

			// Close fabrication object and append report (string) to report buffer if any items were written or
			// ShOp_Force flag set.
			if(dclose(&pShowCtrl->rpt, FabStr) != 0)
LibFail:
				return libfail();
			if(!(flags & ShOp_FirstItem) || flags & ShOp_Force) {

				// Write blank line if has title and buffer not empty.
				if(!(flags & ShOp_NoTitle) && !bempty(pShowCtrl->pRptBuf))
					if(bappend(pShowCtrl->pRptBuf, "") != Success)
						return sess.rtn.status;

				// Append section detail.
				(void) bappend(pShowCtrl->pRptBuf, pShowCtrl->rpt.pDatum->str);
				}
		}

	return sess.rtn.status;
	}

// Close a "show" listing.  Return status.
static int showclose(Datum *pRtnVal, long n, ShowCtrl *pShowCtrl) {

	dclear(&pShowCtrl->name);
	dclear(&pShowCtrl->value);
	dclear(&pShowCtrl->pat);
	dclear(&pShowCtrl->src);
	dclear(&pShowCtrl->index);
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
static int itemInfo(ShowCtrl *pShowCtrl, uint keyType, void *pItem) {
	KeyBind *pKeyBind;
	Buffer *pBuf;
	CmdFunc *pCmdFunc;
	CallHdr *pCallHdr = NULL;
	char *name, *argSyntax;
	char keyBuf[16];

	// Set pointers and item description.
	if(keyType & PtrUserCmdFunc) {
		pBuf = (Buffer *) pItem;
		name = pBuf->bufname + 1;
		pCallHdr = pBuf->pCallHdr;
		if(pCallHdr != NULL) {
			argSyntax = !disnil(&pCallHdr->argSyntax) ? pCallHdr->argSyntax.str : NULL;
			pShowCtrl->descrip = !disnil(&pCallHdr->descrip) ? pCallHdr->descrip.str : NULL;
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
		pKeyBind = (KeyBind *) keyBindTable;
		do {
			if(pKeyBind->cmd.type & keyType && pKeyBind->cmd.u.ptr == pItem) {

				// Add the key sequence.
				ektos(keyBuf, pKeyBind->extKey, true);
				if((sep != NULL && dputs(sep, &fab, 0) != 0) || dputf(&fab, 0, "~#u%s~U", keyBuf) != 0)
					goto LibFail;
				sep = ", ";
				}
			} while(++pKeyBind < (KeyBind *)(keyBindTable + KeyVectCount));
		if(dclose(&fab, FabStr) != 0)
			goto LibFail;
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Get and check option flags for "showConstVars", "showRoutines", and "apropos" commands.  Set both CFCV_SelCmd and
// CFCV_SelFunc flags if neither option entered interactively or options argument not found in script mode, and do the same with
// CFCV_SelSystem and CFCV_SelUser flags.  Return flags.
static uint getCFCVOpts(CmdFuncId id) {
	uint flags = cmdFuncTable[id].pOptHdr->selFlags;

	if(flags == 0)
		return CFCV_SelCmd | CFCV_SelFunc | CFCV_SelSystem | CFCV_SelUser;
	if((flags & (CFCV_SelCmd | CFCV_SelFunc)) == 0)
		flags |= CFCV_SelCmd | CFCV_SelFunc;
	if((flags & (CFCV_SelSystem | CFCV_SelUser)) == 0)
		flags |= CFCV_SelSystem | CFCV_SelUser;
	return flags;
	}

// List system commands or functions per flags via calls to showbuild() function.  Return status.
static int listSysCmdFuncs(ShowCtrl *pShowCtrl, uint flags) {
	CmdFunc *pCmdFunc;
	bool match;
	uint firstFlag = ShOp_FirstItem;
	ushort attrFlag = flags & ShOp_Command ? 0 : CFFunc;

	if(showbuild(pShowCtrl, ShReqBegin, flags & ShOp_Command ? text414 : text336) != Success)
							// "system command", "system function"
		return sess.rtn.status;

	// Loop through system command-function table.
	pCmdFunc = cmdFuncTable;
	do {
		// Skip if wrong type or not apropos match.
		if((pCmdFunc->attrFlags & CFFunc) != attrFlag)
			continue;
		if(aproposMatch(pShowCtrl, pCmdFunc->name, flags & ShOp_Exact, &match) != Success)
			return sess.rtn.status;
		if(!match)
			continue;

		// Match found.  Set item values in ShowCtrl object.
		if(itemInfo(pShowCtrl, flags & ShOp_Command ? PtrSysCmdPseudo : PtrSysFunc, (void *) pCmdFunc) != Success)
			return sess.rtn.status;

		// Write item values to report.
		if(showbuild(pShowCtrl, firstFlag | ShOp_SepLine | ShReqWrName |
		 (disnil(&pShowCtrl->value) ? 0 : ShOp_HasValue), NULL) != Success)
			return sess.rtn.status;
		if(!disnil(&pShowCtrl->value) && dputs(pShowCtrl->value.str, &pShowCtrl->rpt, 0) != 0)
			return libfail();
		if(pShowCtrl->descrip != NULL && storeDesc(pShowCtrl, NULL) != Success)
			return sess.rtn.status;

		firstFlag = 0;
		} while(++pCmdFunc < pCmdFuncEnd);

	return showbuild(pShowCtrl, ShReqEnd | firstFlag, NULL);
	}

// List user commands or functions per flags via calls to showbuild() function.  Return status.
static int listUserCmdFuncs(ShowCtrl *pShowCtrl, uint flags) {
	Buffer *pBuf;
	HashNode **pAry;
	bool match;
	uint firstFlag = ShOp_FirstItem;
	ushort attrFlag = flags & ShOp_Command ? BFCommand : BFFunc;

	if(showbuild(pShowCtrl, ShReqBegin, flags & ShOp_Command ? text417 : text418) != Success)
							// "user command", "user function"
		return sess.rtn.status;

	// Sort names and loop through array.
	if(ohAryPrep(&buf, &pAry) != 0)
		goto LibFail;
	for(; *pAry != NULL; ++pAry) {
		pBuf = bufPtr((*pAry)->pVal);

		// Skip buffer if wrong type or (is a function and no pat and has no help text and is not bound to a hook).
		if((pBuf->flags & BFCmdFunc) != attrFlag || (attrFlag == BFFunc && disempty(&pShowCtrl->pat) &&
		 disnil(&pBuf->pCallHdr->argSyntax) && disnil(&pBuf->pCallHdr->descrip) && !isHook(pBuf, false)))
			continue;

		// Skip if not apropos match.
		if(aproposMatch(pShowCtrl, pBuf->bufname + 1, flags & ShOp_Exact, &match) != Success)
			return sess.rtn.status;
		if(!match)
			continue;

		// Match found.  Set item values in ShowCtrl object.
		if(itemInfo(pShowCtrl, flags & ShOp_Command ? PtrUserCmd : PtrUserFunc, (void *) pBuf) != Success)
			return sess.rtn.status;

		// Write item values to report.
		if(showbuild(pShowCtrl, firstFlag | ShOp_SepLine | ShReqWrName |
		 (disnil(&pShowCtrl->value) ? 0 : ShOp_HasValue), NULL) != Success)
			return sess.rtn.status;
		if(!disnil(&pShowCtrl->value) && dputs(pShowCtrl->value.str, &pShowCtrl->rpt, 0) != 0)
LibFail:
			return libfail();
		if(pShowCtrl->descrip != NULL && storeDesc(pShowCtrl, NULL) != Success)
			return sess.rtn.status;

		firstFlag = 0;
		}

	return showbuild(pShowCtrl, ShReqEnd | firstFlag, NULL);
	}

// Create formatted list of commands and functions which match a pattern via calls to "show" and "list" routines.  Return
// status.
int showRoutines(Datum *pRtnVal, long n, Datum **args) {
	ShowCtrl showCtrl;
	uint flags = getCFCVOpts(cf_showRoutines);
	char separator[term.cols + 1];

	// Open control object.
	if(showopen(&showCtrl, separator, text539, NULL, args) == Success) {
				// "routine"

		// Call the various show routines and build the list.
		if((flags & (CFCV_SelCmd | CFCV_SelSystem)) == (CFCV_SelCmd | CFCV_SelSystem) &&
		 listSysCmdFuncs(&showCtrl, ShOp_Command) != Success)
			goto Retn;
		if((flags & (CFCV_SelCmd | CFCV_SelUser)) == (CFCV_SelCmd | CFCV_SelUser) &&
		 listUserCmdFuncs(&showCtrl, ShOp_Command) != Success)
			goto Retn;
		if((flags & (CFCV_SelFunc | CFCV_SelSystem)) == (CFCV_SelFunc | CFCV_SelSystem) &&
		 listSysCmdFuncs(&showCtrl, 0) != Success)
			goto Retn;
		if((flags & (CFCV_SelFunc | CFCV_SelUser)) == (CFCV_SelFunc | CFCV_SelUser) &&
		 listUserCmdFuncs(&showCtrl, 0) != Success)
			goto Retn;
		(void) showclose(pRtnVal, n, &showCtrl);
		}
Retn:
	return sess.rtn.status;
	}

// List aliases via calls to showbuild() function.  Return status.
static int listAliases(ShowCtrl *pShowCtrl) {
	Alias *pAlias;
	const char *name;
	bool match;
	uint firstFlag = ShOp_FirstItem;
	char workBuf[WorkBufSize];

	if(showbuild(pShowCtrl, ShReqBegin, text127) != Success)
					// "alias"
		return sess.rtn.status;
	strcpy(workBuf, "-> ");

	// Loop through alias list.
	for(pAlias = ahead; pAlias != NULL; pAlias = pAlias->next) {

		// Do apropos matching.
		if(aproposMatch(pShowCtrl, pAlias->name, 0, &match) != Success)
			return sess.rtn.status;
		name = (pAlias->type & (PtrAliasUserCmd | PtrAliasUserFunc)) ?
		 pAlias->targ.u.pBuf->bufname + 1 : pAlias->targ.u.pCmdFunc->name;
		if(!match) {
			if(aproposMatch(pShowCtrl, name, 0, &match) != Success)
				return sess.rtn.status;
			if(!match)
				continue;
			}

		// Found matching alias.  Add it to report.
		if(dsetstr(pAlias->name, &pShowCtrl->name) != 0)
			goto LibFail;
		if(showbuild(pShowCtrl, firstFlag | ShReqWrName | ShOp_HasValue, NULL) != Success)
			return sess.rtn.status;
		strcpy(workBuf + 3, name);
		if(dputs(workBuf, &pShowCtrl->rpt, 0) != 0)
LibFail:
			return libfail();
		firstFlag = 0;
		}

	return showbuild(pShowCtrl, ShReqEnd | firstFlag, NULL);
	}

// Create formatted list of aliases via calls to "show" and "list" routines.  Return status.
int showAliases(Datum *pRtnVal, long n, Datum **args) {
	ShowCtrl showCtrl;
	char separator[term.cols + 1];

	// Open control object, build listing, and close it.
	if(showopen(&showCtrl, separator, text127, NULL, args) == Success && listAliases(&showCtrl) == Success)
					// "alias"
		(void) showclose(pRtnVal, n, &showCtrl);

	return sess.rtn.status;
	}

// List system constant or variable names per flag via calls to showbuild() function.  Return status.
static int listSConstVars(ShowCtrl *pShowCtrl, bool sysConst) {
	SConstVar *pSConstVar;
	bool match;
	uint firstFlag = ShOp_FirstItem;

	if(showbuild(pShowCtrl, ShReqBegin, sysConst ? text515 : text21) != Success)
						// "system constant", "system variable"
		return sess.rtn.status;

	// Loop through system constant or variable table.
	for(pSConstVar = sysConst ? sysConstTable : sysVarTable; pSConstVar->name != NULL; ++pSConstVar) {

		// Do apropos matching.
		if(aproposMatch(pShowCtrl, pSConstVar->name, 0, &match) != Success)
			return sess.rtn.status;
		if(!match)
			continue;

		// Match found.  Set variable name and add to report.
		if(dsetstr(pSConstVar->name, &pShowCtrl->name) != 0)
			goto LibFail;
		if(showbuild(pShowCtrl, firstFlag | ShOp_SepLine | ShReqWrName | ShOp_HasValue, NULL) != Success)
			return sess.rtn.status;

		// Write value and description.
		if(pSConstVar->flags & V_ShowKey) {
			Datum *pDatum;

			if((pDatum = dnewtrack()) == NULL)
				goto LibFail;
			if(getSConstVar(pDatum, pSConstVar) == Success &&
			 dputf(&pShowCtrl->rpt, 0, "~#u%s~U", pDatum->str) != 0)
LibFail:
				return libfail();
			}
		else if(scvtofab(pSConstVar, true, &pShowCtrl->rpt) != Success)
			return sess.rtn.status;
		if(storeDesc(pShowCtrl, pSConstVar->descrip) != Success)
			return sess.rtn.status;

		firstFlag = 0;
		}

	return showbuild(pShowCtrl, ShReqEnd | firstFlag, NULL);
	}

// List global constant or variable names in pre-sorted array via calls to showbuild() function.  Return status.
static int listGConstVars(ShowCtrl *pShowCtrl, const char *typeLabel) {
	const char **pNameList, *name;
	GConstVar *pGConstVar;
	bool match;
	uint firstFlag = ShOp_FirstItem;

	if(showbuild(pShowCtrl, ShReqBegin, typeLabel) != Success)
		return sess.rtn.status;

	// Loop through sorted global constant or variable array.
	for(pNameList = pShowCtrl->pNameList; *pNameList != NULL; ++pNameList) {

		// Do apropos matching.
		if(aproposMatch(pShowCtrl, name = *pNameList, 0, &match) != Success)
			return sess.rtn.status;
		if(!match)
			continue;

		// Match found.  Set variable name and add to report.
		if(dsetstr(name, &pShowCtrl->name) != 0)
			goto LibFail;
		if(showbuild(pShowCtrl, firstFlag | ShReqWrName | ShOp_HasValue, NULL) != Success)
			return sess.rtn.status;

		// Write value.
		pGConstVar = dgetmemptr(hsearch(&gblConstVarTable, name)->pVal);
		if(dtofabattr(pGConstVar->pVal, &pShowCtrl->rpt, NULL, CvtLang | CvtThouSep) < 0)
LibFail:
			return libfail();

		firstFlag = 0;
		}

	return showbuild(pShowCtrl, typeLabel == text56 && localVarRoot != NULL ? firstFlag | ShReqEnd | ShOp_Force :
					// "user variable"
	 firstFlag | ShReqEnd, NULL);
	}

// List local variable names via calls to showbuild() function.  Return status.
static int listLocalVars(ShowCtrl *pShowCtrl) {
	LocalVar *pLocalVar;
	bool match;
	uint firstFlag = ShOp_FirstItem;

	if(showbuild(pShowCtrl, ShReqBegin | ShOp_NoTitle, NULL) != Success)
		return sess.rtn.status;

	// Loop through local variable linked list.
	for(pLocalVar = localVarRoot; pLocalVar != NULL; pLocalVar = pLocalVar->next) {

		// Do apropos matching.
		if(aproposMatch(pShowCtrl, pLocalVar->name, 0, &match) != Success)
			return sess.rtn.status;
		if(!match)
			continue;

		// Match found.  Set variable name and add to report.
		if(dsetstr(pLocalVar->name, &pShowCtrl->name) != 0)
			goto LibFail;
		if(showbuild(pShowCtrl, firstFlag | ShReqWrName | ShOp_NoTitle | ShOp_HasValue, NULL) != Success)
			return sess.rtn.status;

		// Write value.
		if(dtofabattr(pLocalVar->pVal, &pShowCtrl->rpt, NULL, CvtLang | CvtThouSep) < 0)
LibFail:
			return libfail();

		firstFlag = 0;
		}

	return showbuild(pShowCtrl, ShReqEnd | firstFlag, NULL);
	}

// Get global constant or variable names into cvList and sort it.
static void prepCVList(ShowCtrl *pShowCtrl, const char **pCVList, uint cvListSize, uint flags) {

	if(cvListSize > 0)
		cvsort(pCVList, cvListSize, flags);
	pCVList[cvListSize] = NULL;
	pShowCtrl->pNameList = pCVList;
	}

// Create formatted lists of constants and variables via calls to "list" and "show" routines.  Return status.
static int showCV(Datum *pRtnVal, long n, ShowCtrl *pShowCtrl, uint flags) {
	uint gcListSize, gvListSize;
	const char *gcList[(gcListSize = cvcount(TermC_GblConst)) + 1];
	const char *gvList[(gvListSize = cvcount(TermC_GblVar)) + 1];

	// Show system constants and variables.
	if(flags & CFCV_SelSystem &&
	 (listSConstVars(pShowCtrl, true) != Success || listSConstVars(pShowCtrl, false) != Success))
		goto Retn;

	if(flags & CFCV_SelUser) {
		// Do global constants, if any.
		if(gcListSize > 0) {
			prepCVList(pShowCtrl, gcList, gcListSize, TermC_GblConst);
			if(listGConstVars(pShowCtrl, text514) != Success)
						// "user constant"
				goto Retn;
			}

		// Do global variables, if any.
		if(gvListSize > 0 || localVarRoot != NULL) {
			prepCVList(pShowCtrl, gvList, gvListSize, TermC_GblVar);
			if(listGConstVars(pShowCtrl, text56) != Success)
						// "user variable"
				goto Retn;
			}

		// Do local variables, if any.
		if(localVarRoot != NULL)
			if(listLocalVars(pShowCtrl) != Success)
				goto Retn;
		}

	// Close control object and return.
	(void) showclose(pRtnVal, n, pShowCtrl);
Retn:
	return sess.rtn.status;
	}

// Create formatted list of system and user constants and variables via calls to "show" routines.  Return status.
int showConstVars(Datum *pRtnVal, long n, Datum **args) {
	ShowCtrl showCtrl;
	uint flags = getCFCVOpts(cf_showConstVars);
	char separator[term.cols + 1];

	// Open control object.
	if(showopen(&showCtrl, separator, text292, text318, args) == Success)
			// "constants and variable", "ConstVars"

		// Show constants and variables.
		(void) showCV(pRtnVal, n, &showCtrl, flags);

	return sess.rtn.status;
	}

// Create formatted list of commands, functions, aliases, constants, and variables which match a pattern via calls to "show" and
// "list" routines.  Return status.
int apropos(Datum *pRtnVal, long n, Datum **args) {
	ShowCtrl showCtrl;
	uint flags = getCFCVOpts(cf_apropos);
	char separator[term.cols + 1];

	// Open control object.
	if(showopen(&showCtrl, separator, text97, NULL, args) == Success) {
					// "name"

		// Call the various show routines and build the list.
		if(flags & CFCV_SelSystem && listSysCmdFuncs(&showCtrl, ShOp_Command) != Success)
			goto Retn;
		if(flags & CFCV_SelUser && listUserCmdFuncs(&showCtrl, ShOp_Command) != Success)
			goto Retn;
		if(flags & CFCV_SelSystem && listSysCmdFuncs(&showCtrl, 0) != Success)
			goto Retn;
		if(flags & CFCV_SelUser && listUserCmdFuncs(&showCtrl, 0) != Success)
			goto Retn;
		if(listAliases(&showCtrl) == Success)
			(void) showCV(pRtnVal, n, &showCtrl, flags);
		}
Retn:
	return sess.rtn.status;
	}

// Describe the system or user command bound to a particular key.  Get single keystroke if n <= 0.  Display result on message
// line if n >= 0, otherwise in a pop-up window (default).  Return status.  *Interactive only*
int showKey(Datum *pRtnVal, long n, Datum **args) {
	ExtKey extKey;
	KeyBind *pKeyBind;
	const char *argSyntax = NULL;
	const char *descrip = NULL;
	const char *name;
	int (*listCommand)(ShowCtrl *pShowCtrl, uint flags);
	char keyBuf[16];

	// Prompt the user for the key code.
	if(getKeyBind(text13, n <= 0 && n != DefN ? 0 : DefN, args, &extKey) != Success)
			// "Show key "
		return sess.rtn.status;

	// Find the command.
	if((pKeyBind = getBind(extKey)) == NULL)
		name = text48;
			// "[Not bound]"
	else if(pKeyBind->cmd.type == PtrUserCmd) {
		Buffer *pBuf = pKeyBind->cmd.u.pBuf;
		name = pBuf->bufname + 1;
		if(n < 0) {
			listCommand = listUserCmdFuncs;
			argSyntax = text417;
				// "user command"
			goto PopUp;
			}
		if(!disnil(&pBuf->pCallHdr->argSyntax))
			argSyntax = pBuf->pCallHdr->argSyntax.str;
		if(!disnil(&pBuf->pCallHdr->descrip))
			descrip = pBuf->pCallHdr->descrip.str;
		}
	else {
		CmdFunc *pCmdFunc = pKeyBind->cmd.u.pCmdFunc;
		name = pCmdFunc->name;
		listCommand = listSysCmdFuncs;
		if(n < 0) {
			argSyntax = text414;
				// "system command"
			goto PopUp;
			}
		argSyntax = pCmdFunc->argSyntax;
		descrip = pCmdFunc->descrip;
		}

	// Display result on message line.
	ektos(keyBuf, extKey, true);
	if(mlprintf(MLHome | MLTermAttr, "~#u%s~U -> ~b%s~B", keyBuf, name) == Success) {
		if(argSyntax != NULL && mlprintf(MLTermAttr, " ~b%s~B", argSyntax) != Success)
			return sess.rtn.status;
		if(descrip != NULL && (mlputs(" - ", MLTermAttr) != Success || mlputs(descrip, MLTermAttr) != Success))
			return sess.rtn.status;
		(void) mlflush();
		}
	return sess.rtn.status;
PopUp:;
	// Display result in pop-up window.
	ShowCtrl showCtrl;
	char separator[term.cols + 1];

	// Open control object, build listing, and close it.
	if(showopen(&showCtrl, separator, text158, NULL, NULL) == Success) {
				// "command"
		if(dsetstr(name, &showCtrl.pat) != 0)
			return libfail();
		if(listCommand(&showCtrl, ShOp_Command | ShOp_Exact) == Success)
			(void) showclose(pRtnVal, -1, &showCtrl);
		}
	return sess.rtn.status;
	}

// Create formatted list of key bindings via calls to "show" routines.  Return status.
int showBindings(Datum *pRtnVal, long n, Datum **args) {
	static ColHdrWidth colWidths[] = {
		{7, 7}, {MaxBufname - 6, MaxBufname - 6}, {MaxBufname - 6, MaxBufname - 6}, {MaxBufname - 6, MaxBufname - 6},
		{MaxBufname - 6, MaxBufname - 6}, {MaxBufname - 6, MaxBufname - 12}, {0, 0}};
	uint flags = cmdFuncTable[cf_showBindings].pOptHdr->selFlags;
	ShowCtrl showCtrl;
	KeyBind *pKeyBind, *pKeyBind1;
	const char *name;
	char *str;
	bool match;
	int pageWidth, spacing = (term.cols < 96) ? 1 : 2;
	char separator[term.cols + 1];
	char workBuf[32];
	ushort prefixMask, lastPrefix = 0;

	// If no options were selected interactively, bail out (user cancelled).
	if(n != DefN && !(sess.opFlags & OpScript) && flags == 0)
		return sess.rtn.status;

	// Get a buffer and open fabrication object.
	if(showopen(&showCtrl, separator, text474, text474 + 4, args) != Success)
				// "key binding", "binding"
		return sess.rtn.status;
	if(dopentrack(&showCtrl.rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rptHdr(&showCtrl.rpt, text474, true, text357, colWidths, &pageWidth) != Success)
		// "key binding", " Key     No Prefix         ESC Prefix        C-x Prefix        C-c Prefix        C-h Prefix"
		return sess.rtn.status;

	// Scan binding table.
	for(pKeyBind = (KeyBind *) keyBindTable; pKeyBind < (KeyBind *)(keyBindTable + 1); ++pKeyBind) {

		// Select key if Bnd_ExclFunc flag not set or not a function key and Bnd_InclUnbound flag set or key is bound to
		// a command (including self insert).
		if((!(flags & Bnd_ExclFunc) || !(pKeyBind->extKey & FKey)) && (flags & Bnd_InclUnbound ||
		 pKeyBind->cmd.u.ptr != NULL || (pKeyBind->extKey >= 0x20 && pKeyBind->extKey < 0x7f))) {

			// Found key.  Skip if not apropos match.
			if(aproposMatch(&showCtrl, ektos(workBuf, pKeyBind->extKey, false), 0, &match) != Success)
				return sess.rtn.status;
			if(!match)
				continue;

			// Found key.  Write separator line if key prefix has changed.
			if((pKeyBind->extKey & Prefix) != lastPrefix) {
				if(sepLine(pageWidth, &showCtrl.rpt) != 0)
					goto LibFail;
				lastPrefix = pKeyBind->extKey & Prefix;
				}

			// Write key literal.
			if(dputf(&showCtrl.rpt, 0, "\n ~b%-*s~B", (int)(colWidths[0].minWidth - ((pKeyBind->extKey & 0xff) ==
			 AttrSpecBegin ? 0 : 1) + spacing), ektos(workBuf, pKeyBind->extKey, true)) != 0)
				goto LibFail;

			// Loop through prefixes and write command bound to each (or spaces).
			prefixMask = 0;
			do {
				if(prefixMask == 0) {
					prefixMask = Ctrl;
					pKeyBind1 = pKeyBind;
					}
				else {
					prefixMask <<= 1;
					pKeyBind1 = bindSlot(pKeyBind->extKey | prefixMask);
					}
				if((name = getBindCmd(pKeyBind1)) != NULL) {
					str = workBuf;
					if(pKeyBind1->cmd.type & PtrSysCmdPseudo)
						*str++ = ' ';
					strcpy(str, name);
					}
				else
					workBuf[0] = '\0';
				name = workBuf;

				if(prefixMask == Pref3) {
					if(dputs(name, &showCtrl.rpt, 0) != 0)
						goto LibFail;
					}
				else if(dputf(&showCtrl.rpt, 0, "%-*s", (int)(colWidths[1].minWidth + spacing), name) != 0)
					goto LibFail;
				} while(prefixMask < Pref3);
			}
		}

	// Add the results to the buffer.
	if(dclose(&showCtrl.rpt, FabStr) != 0)
		goto LibFail;
	if(bappend(showCtrl.pRptBuf, showCtrl.rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return showclose(pRtnVal, n, &showCtrl);
LibFail:
	return libfail();
	}

// Build and pop up a buffer containing a list of all visible buffers (default) or those selected per options if interactive and
// n argument, or argument given in script mode.  Render buffer and return status.
int showBuffers(Datum *pRtnVal, long n, Datum **args) {
	static ColHdrWidth colWidths[] = {
		{9, 9}, {13, 13}, {MaxBufname - 4, MaxBufname - 4}, {31, 8}, {0, 0}};
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
	HashNode *pNode, **pAry;
	Buffer *pRptBuf, *pBuf;
	DFab rpt;
	short c;
	int pageWidth;
	bool firstBuf = true;
	char *space = (term.cols < 96) ? " " : "  ";
	OptHdr *pOptHdr = cmdFuncTable[cf_showBuffers].pOptHdr;
	uint flags = pOptHdr->selFlags;

	// If no options were selected interactively, bail out (user cancelled).
	if(flags == 0) {
		if(n != DefN && !(sess.opFlags & OpScript))
			return sess.rtn.status;
		flags = SBO_SelVisible;
		}
	else {
		if(flags & SBO_SelVisible)
			flags &= ~SBO_SelHomed;
		else if(!(flags & (SBO_SelHomed | SBO_SelCommand | SBO_SelFunction)))
			return rsset(Failure, 0, text455, pOptHdr->type);
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
			// "Buffers", "AHUPNTBRC     Size        Buffer              File"
		return sess.rtn.status;

	// Build the list of buffers.
	if(ohAryPrep(&buf, &pAry) != 0)
		goto LibFail;
	while((pNode = *pAry++) != NULL) {
		pBuf = bufPtr(pNode->pVal);

		// Skip if buffer not requested.
		if(pBuf->flags & BFCommand) {
			if(!(flags & SBO_SelCommand))
				continue;
			}
		else if(pBuf->flags & BFFunc) {
			if(!(flags & SBO_SelFunction))
				continue;
			}
		else {
			if(pBuf->flags & BFHidden && !(flags & SBO_InclHidden))
				continue;
			if(!(flags & SBO_SelVisible) &&
			 (!(flags & SBO_SelHomed) || pBuf->homeDir == NULL || pBuf->homeDir != sess.pCurScrn->workDir))
				continue;
			}

		// Write separator line if displaying home directory and not first buffer.
		if(firstBuf)
			firstBuf = false;
		else if(flags & SBO_InclHomeDir && sepLine(pageWidth, &rpt) != 0)
			goto LibFail;

		if(dputc('\n', &rpt, 0) != 0)
			goto LibFail;

		// Output the buffer flag indicators.
		pBufFlag = bufFlagTable;
		do {
			if(pBufFlag->bufFlag == USHRT_MAX)
				c = (pBuf->windCount == 0) ? pBufFlag->dispChar : ' ';
			else if(pBufFlag->bufFlag == BFCommand) {
				c = pBuf->flags & BFCommand ? SB_Command : pBuf->flags & BFFunc ? SB_Func : ' ';
				++pBufFlag;
				}
			else if(!(pBuf->flags & pBufFlag->bufFlag))
				c = ' ';
			else if((c = pBufFlag->dispChar) == AttrSpecBegin && dputc(AttrSpecBegin, &rpt, 0) != 0)
				goto LibFail;
			if(dputc(c, &rpt, 0) != 0)
				goto LibFail;
			} while((++pBufFlag)->bufFlag != 0);

#if MMDebug & Debug_WindCount
		dputf(&rpt, 0, "%3hu", pBuf->windCount);
#endif
		// Output the buffer size, buffer name, and filename.
		if(dputf(&rpt, 0, "%s%'*lu%s%-*s", space, colWidths[1].minWidth, bufLength(pBuf, NULL), space,
		 colWidths[2].minWidth, pBuf->bufname) != 0 || (pBuf->filename != NULL && (dputs(space, &rpt, 0) != 0 ||
		 dputs(pBuf->filename, &rpt, 0) != 0)))
			goto LibFail;

		// Output the home directory if requested and not NULL.
		if(flags & SBO_InclHomeDir && pBuf->homeDir != NULL &&
		 dputf(&rpt, 0, "\n%*s%sCWD: %s", colWidths[0].minWidth, "", space, pBuf->homeDir) != 0)
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
int showColors(Datum *pRtnVal, long n, Datum **args) {

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
	if(n <= 0 && n != DefN) {
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
int showHooks(Datum *pRtnVal, long n, Datum **args) {
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

// Build and pop up a buffer containing all macros and shortcuts in macro tables.  Render buffer and return status.
int showMacros(Datum *pRtnVal, long n, Datum **args) {
	Buffer *pRptBuf;
	HashNode *pNode, **pAry;
	Macro *pStage;
	Datum *pDatum;
	DFab rpt;
	Shortcut s;
	int pageWidth, spacing = (term.cols < 96) ? 1 : 2;
	static ColHdrWidth colWidths[] = {
		{4, 4}, {MaxMacroName - 4, MaxMacroName - 4}, {-1, 12}, {0, 0}};

	// Get buffer for the key list.
	if(sysBuf(text466, &pRptBuf, Plural | BFTermAttr) != Success)
			// "macro"
		return sess.rtn.status;
	if((pDatum = dnewtrack()) == NULL || dopentrack(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rptHdr(&rpt, text471, true, text271, colWidths, &pageWidth) != Success)
			// "Macro", "Shrt    Name                        Key List"
		return sess.rtn.status;

	if(macro.index.table.nodeCount > 0) {

		// Build the list of macros and shortcuts.
		if(ohAryPrep(&macro.index, &pAry) != 0)
			goto LibFail;
		pStage = macro.pStageMacro;
		while((pNode = *pAry++) != NULL) {
			if(loadStage(pNode) != Success || encodeMacro(pDatum, pStage) != Success)
				return sess.rtn.status;
			pNode = hfindnode(&macro.shortcuts, &pStage->name, 0);
			s = pNode == NULL ? ' ' : *pNode->key;
			if(dputf(&rpt, 0, "\n %-*c%c%-*s", (int)(colWidths[0].minWidth - 2 + spacing), s,
			 isActiveMacro(pStage->name.str) ? '*' : ' ', (int)(colWidths[1].minWidth + spacing),
			 pStage->name.str) != 0 || esctofab(pDatum->str, strlen(pDatum->str), &rpt) != 0)
				goto LibFail;
			}

		// Write footnote if applicable.
		if(!disnil(&macro.pActiveMacro->name)) {
			if(sepLine(pageWidth, &rpt) != 0)
				goto LibFail;
			if(dputf(&rpt, 0, "\n* %s", text312) != 0)
					// "Active macro"
				goto LibFail;
			}
		}

	// Add the report to the buffer.
	if(dclose(&rpt, FabStr) != 0)
LibFail:
		return libfail();
	if(bappend(pRptBuf, rpt.pDatum->str) != Success)
		return sess.rtn.status;

	// Display results.
	return render(pRtnVal, n, pRptBuf, RendNewBuf | RendRewind);
	}

// Build and pop up a buffer containing all marks which exist in the current buffer.  Render buffer and return status.
int showMarks(Datum *pRtnVal, long n, Datum **args) {
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
				 dputsubstr(pLine->text, pLine->used > max ? max : pLine->used, &rpt, CvtVizChar) != 0))
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
int showModes(Datum *pRtnVal, long n, Datum **args) {
	Buffer *pRptBuf;
	DFab rpt;
	ModeSpec *pModeSpec;
	ModeGrp *pModeGrp;
	HashNode *pNode, **pAry;
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
		{SM_Active, MdActive, text31},
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
		if(ohAryPrep(&mode.idx, &pAry) != 0)
			goto LibFail;
		while((pNode = *pAry++) != NULL) {
			pModeSpec = modePtr(pNode->pVal);

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
	pModeGrp = mode.grpHead;
	do {
		if(dputf(&rpt, 0, "\n %c%*s%s%-*s", pModeGrp->flags & MdUser ? SM_User : ' ', colWidths[0].minWidth - 2, "",
		 space, colWidths[1].minWidth, pModeGrp->name) != 0 ||
		 (pModeGrp->descrip != NULL && dputf(&rpt, 0, "%s%s", space, pModeGrp->descrip) != 0) ||
		 dputf(&rpt, 0, "\n%*s", colWidths[0].minWidth + colWidths[1].minWidth + spacing + spacing + 4, "") != 0)
			goto LibFail;
		str = "[";
		if(ohAryPrep(&mode.idx, &pAry) != 0)
			goto LibFail;
		while((pNode = *pAry++) != NULL) {
			if((pModeSpec = modePtr(pNode->pVal))->pModeGrp == pModeGrp) {
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
int showRing(Datum *pRtnVal, long n, Datum **args) {
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

	// Loop through ring, beginning at current slot and continuing until we arrive back to where we began.
	if(pRing->pEntry != NULL) {
		DFab item;
		Datum *pDatum;
		char *str;
		RingEntry *pEntry;
		size_t len, max = term.cols * 2;
		int entryNum = 0;

		pEntry = pRing->pEntry;
		do {
			if(dputf(&rpt, 0, "\n%*d %s", (int) colWidths[0].minWidth - 1, entryNum--, space) != 0)
				goto LibFail;
			if(pEntry->data.type == dat_byteStr) {
				if((len = pEntry->data.u.mem.size) == 0)
					continue;
				str = pEntry->data.u.mem.ptr;
				}
			else if((len = strlen(str = pEntry->data.str)) == 0)
				continue;

			// Convert text to visible string, doubling up any AttrSpecBegin (~) characters.
			if(dopentrack(&item) != 0)
				goto LibFail;
			if(len > max)
				len = max;
			if(esctofab(str, len, &item) != 0 || dclose(&item, FabStr) != 0)
				goto LibFail;

			// Write converted substring to report.
			pDatum = item.pDatum;
			if((len = strlen(pDatum->str)) > max)
				len = max;
			if(dputsubstr(pDatum->str, len, &rpt, 0) != 0)
				goto LibFail;

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

// Store buffer name and filename in screen listing.
static int storeBufFile(DFab *pRpt, Buffer *pBuf, ColHdrWidth *colWidths, char *space) {

	if(pBuf->filename != NULL) {
		if(dputf(pRpt, 0, "%-*s%s%s", colWidths[2].minWidth, pBuf->bufname, space, pBuf->filename) != 0)
			goto LibFail;
		}
	else if(dputs(pBuf->bufname, pRpt, 0) != 0)
LibFail:
		return libfail();

	return 0;
	}

// Build and pop up a buffer containing a list of all screens and their associated buffers.  Render buffer and return status.
int showScreens(Datum *pRtnVal, long n, Datum **args) {
	Buffer *pRptBuf, *pBuf;
	HashNode *pNode, **pAry;
	uint windNum;
	int pageWidth;
	DFab rpt;
	bool chgBuf = false;		// Screen has a changed buffer?
	char *space = (term.cols < 96) ? " " : "  ";
	int spacing = (term.cols < 96) ? 1 : 2;
	int scrnTotal = scrnCount();
	EScreen *pScrn, *scrnTable[scrnTotal + 1], **ppScrn = scrnTable;
	EWindow *pWind;
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
	sortScreens(scrnTable, scrnTotal);
	do {
		pScrn = *ppScrn++;
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
			 pBuf->flags & BFChanged ? '*' : ' ') != 0)
				goto LibFail;

			// Store buffer name and filename.
			if(storeBufFile(&rpt, pBuf, colWidths, space) != 0)
				goto LibFail;

			// On to the next window.
			} while((pWind = pWind->next) != NULL);

		// Search buffer list and add any remaining, non-displayed buffers homed to this screen.
		if(ohAryPrep(&buf, &pAry) != 0)
			goto LibFail;
		while((pNode = *pAry++) != NULL) {
			pBuf = bufPtr(pNode->pVal);

			// Skip if buffer hidden, being displayed, or not homed to current screen.
			if(pBuf->flags & BFHidden || pBuf->windCount > 0 || pBuf->homeDir == NULL ||
			 pBuf->homeDir != pScrn->workDir)
				continue;

			// Store indentation and "changed" marker.
			if(pBuf->flags & BFChanged)
				chgBuf = true;
			if(dputf(&rpt, 0, "\n%*s%.*s%c", colWidths[0].minWidth + spacing + colWidths[1].minWidth, "",
			 spacing - 1, space, pBuf->flags & BFChanged ? '*' : ' ') != 0)
				goto LibFail;

			// Store buffer name and filename.
			if(storeBufFile(&rpt, pBuf, colWidths, space) != 0)
				goto LibFail;
			}

		// On to the next screen.
		} while(*ppScrn != NULL);

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

#if MMDebug & Debug_ShowRE
/*** NOTE: The following function is old code and needs to be updated to compile and run properly. ***/

// Build and pop up a buffer containing the compiled search and replacement patterns.  Render buffer and return status.
int showRegexp(Datum *pRtnVal, long n, Datum **args) {
	Buffer *pRegList;
	DFab rpt;
	char patBuf[bufSearch.match.patLen + OptCh_N + 1];
	struct RegInfo {
		char *hdr;
		char *pat;
		regex_t *compPat;
		} *pRegInfo;
	static struct RegInfo regInfoTable[] = {
		{text998, NULL, NULL},
			// "Forward"
		{text999, NULL, NULL},
			// "Backward"
		{NULL, NULL, NULL}};

	// Get a buffer and open a fabrication object.
	if(sysBuf(text997, &pRegList, 0) != Success)
			// "RegexpInfo"
		return sess.rtn.status;
	if(dopentrack(&rpt) != 0)
		goto LibFail;

	makePat(patBuf, &bufSearch.match);
	if(dputf(&rpt, 0, "Match flags: %.4x\n", bufSearch.match.flags) != 0 ||
	 dputf(&rpt, 0, "%s %s /", text995, text996) != 0 || dputs(patBuf, &rpt, CvtVizChar) != 0 ||
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
			 CvtVizChar) != 0 || dputs("/\n", &rpt, 0) != 0)
				goto LibFail;
			if((pRegInfo->pat == bufSearch.match.pat || bufSearch.match.flags & SCpl_BackwardRE) &&
			 dputf(&rpt, 0, "Group count: %lu\n", pRegInfo->compPat->re_nsub) != 0)
				goto LibFail;
			} while((++pRegInfo)->hdr != NULL);
		}
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
int aboutMM(Datum *pRtnVal, long n, Datum **args) {
	Buffer *pRptBuf;
	DFab rpt;
	char *str;
	int len1, len2, maxLabelLen;
	char footer1[sizeof(ProgName) + sizeof(ALit_Footer1) + 1];	// Longest line displayed.
	char workBuf1[TTY_MaxCols + 1];
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
			// "(c) Copyright YYYY Richard W. Marinelli <italian389@yahoo.com>"
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
