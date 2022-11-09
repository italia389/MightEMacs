// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// buffer.c	Buffer management routines for MightEMacs.
//
// Some of the functions are internal, and some are attached to user keys.

#include "std.h"
#include <stdarg.h>
#include "cxl/lib.h"
#include "cmd.h"
#include "bind.h"
#include "exec.h"
#include "file.h"

#if MMDebug & Debug_ModeDump
// Dump mode table or buffer modes to log file.
static void dumpMode(ModeSpec *pModeSpec) {

	fprintf(logfile, "  %-16s%.4hX  %c%c%c%c  %-16s  %s\n", pModeSpec->name, pModeSpec->flags,
	 pModeSpec->flags & MdUser ? 'U' : '.',
	 pModeSpec->flags & MdGlobal ? 'G' : '.',
	 pModeSpec->flags & MdHidden ? 'H' : '.',
	 pModeSpec->flags & MdEnabled ? SM_Active : '.',
	 pModeSpec->pModeGrp == NULL ? "NULL" : pModeSpec->pModeGrp->name,
	 pModeSpec->descrip == NULL ? "NULL" : pModeSpec->descrip);
	}

void dumpModes(Buffer *pBuf, bool sepLine, const char *fmt, ...) {
	va_list varArgList;

	fputs(sepLine ? "----------\n" : "\n", logfile);
	va_start(varArgList, fmt);
	if(pBuf == NULL) {
		Datum *pArrayEl;
		Array *pArray = &modeInfo.modeTable;

		fputs("MODE TABLE - ", logfile);
		vfprintf(logfile, fmt, varArgList);
		while((pArrayEl = aeach(&pArray)) != NULL)
			dumpMode(modePtr(pArrayEl));
		}
	else {
		BufMode *pBufMode = pBuf->modes;
		fprintf(logfile, "BUFFER '%s': ", pBuf->bufname);
		vfprintf(logfile, fmt, varArgList);
		if(pBufMode == NULL)
			fputs("\t*None set*\n", logfile);
		else
			do
				dumpMode(pBufMode->pModeSpec);
				while((pBufMode = pBufMode->next) != NULL);
		}
	va_end(varArgList);
	}
#endif

// Check if any given mode (ModeSpec pointers in vararg list) is set in a buffer and return Boolean result.  If "clear" is true,
// also clear each mode that is found.
bool isAnyBufModeSet(Buffer *pBuf, bool clear, int modeCount, ...) {
	BufMode *pBufMode0, *pBufMode1, *pBufMode2;
	ModeSpec *pModeSpec;
	va_list varArgList;
	int count;
	bool modeFound = false;
	bool updateML = false;

#if MMDebug & Debug_Modes
	ModeSpec *pModeSpec2;
	if(clear)
		dumpModes(pBuf, true, "isAnyBufModeSet(..., CLEAR, %d, ...)\n", modeCount);
#endif
	// For each buffer mode...
	pBufMode0 = NULL;
	pBufMode1 = pBuf->modes;
	while(pBufMode1 != NULL) {
		pBufMode2 = pBufMode1->next;
		pModeSpec = pBufMode1->pModeSpec;
#if MMDebug & Debug_Modes
		if(clear)
			fprintf(logfile, "    Found enabled buffer mode '%s'...\n", pModeSpec->name);
#endif
		count = modeCount;
		va_start(varArgList, modeCount);

		// For each mode argument...
		do {
#if MMDebug & Debug_Modes
			pModeSpec2 = va_arg(varArgList, ModeSpec *);
			if(clear)
				fprintf(logfile, "\tgot mode argument '%s'...\n", pModeSpec2->name);
			if(pModeSpec2 == pModeSpec) {
#else
			if(va_arg(varArgList, ModeSpec *) == pModeSpec) {
#endif
				if(!clear)
					return true;
#if MMDebug & Debug_Modes
				fprintf(logfile, "    match found.  Clearing mode '%s'...\n", pModeSpec->name);
#endif
				// Remove mode record from linked list.
				if(pBufMode0 == NULL)
					pBuf->modes = pBufMode2;
				else
					pBufMode0->next = pBufMode2;
				free((void *) pBufMode1);

				modeFound = true;

				// Check if mode line needs an update.
				if((pModeSpec->flags & (MdHidden | MdInLine)) != MdHidden)
					updateML = true;
				goto Next;
				}
			} while(--count > 0);
		va_end(varArgList);

		// Advance pointers.
		pBufMode0 = pBufMode1;
Next:
		pBufMode1 = pBufMode2;
		}

	// Set window flags if a visible mode was cleared.
	if(updateML)
		supd_windFlags(pBuf, WFMode);

#if MMDebug & Debug_Modes
	if(clear)
		dumpModes(pBuf, false, "isAnyBufModeSet() END\n");
#endif
	return modeFound;
	}

// Check if given mode (ModeSpec pointer) is set in a buffer and return Boolean result.  This is a fast version for checking
// a single mode.
bool isBufModeSet(Buffer *pBuf, ModeSpec *pModeSpec) {
	BufMode *pBufMode;

	// For each buffer mode...
	for(pBufMode = pBuf->modes; pBufMode != NULL; pBufMode = pBufMode->next)
		if(pBufMode->pModeSpec == pModeSpec)
			return true;
	// Not found.
	return false;
	}

// Clear all modes in given buffer.  Return true if any mode was enabled, otherwise false.
bool bufClearModes(Buffer *pBuf) {
	BufMode *pBufMode1 = pBuf->modes, *pBufMode2;
	bool modeWasChanged = (pBufMode1 != NULL);
	bool updateML = false;

#if MMDebug & Debug_Modes
	dumpModes(pBuf, true, "bufClearModes() BEGIN\n");
#endif
	while(pBufMode1 != NULL) {
		pBufMode2 = pBufMode1->next;
		if((pBufMode1->pModeSpec->flags & (MdHidden | MdInLine)) != MdHidden)
			updateML = true;
		free((void *) pBufMode1);
		pBufMode1 = pBufMode2;
		}
	pBuf->modes = NULL;
	if(updateML)
		supd_windFlags(pBuf, WFMode);
#if MMDebug & Debug_Modes
	dumpModes(pBuf, false, "bufClearModes() END\n");
#endif
	return modeWasChanged;
	}

// Clear a mode in all buffers.
void clearBufMode(ModeSpec *pModeSpec) {
	Datum *pArrayEl;
	Array *pArray = &bufTable;
	while((pArrayEl = aeach(&pArray)) != NULL)
		isAnyBufModeSet(bufPtr(pArrayEl), true, 1, pModeSpec);
	}

// Set a mode in a buffer.  If clear is true, clear all existing modes first.  If clear is false and pWasSet not NULL, set
// *pWasSet to true if mode was already set, otherwise false.  It is assumed that mode points to a valid buffer mode in the
// mode table.  Return status.
int setBufMode(Buffer *pBuf, ModeSpec *pModeSpec, bool clear, bool *pWasSet) {
	BufMode *pBufMode0, *pBufMode1, *pBufMode2;
	bool wasSet = false;

	if(clear)						// "Clear modes" requested?
		bufClearModes(pBuf);				// Yes, nuke 'em.
	else if(isBufModeSet(pBuf, pModeSpec)) {		// No.  Is mode already set?
		wasSet = true;					// Yes.
		goto Retn;
		}

#if MMDebug & Debug_Modes
	dumpModes(pBuf, true, "setBufMode(..., %s, %s, ...)\n", pModeSpec->name, clear ? "CLEAR" : "false");
#endif
	// Insert mode in linked list alphabetically and return.
	if((pBufMode2 = (BufMode *) malloc(sizeof(BufMode))) == NULL)
		return rsset(Panic, 0, text94, "setBufMode");
			// "%s(): Out of memory!"
	pBufMode2->pModeSpec = pModeSpec;
	pBufMode0 = NULL;
	pBufMode1 = pBuf->modes;
	while(pBufMode1 != NULL) {
		if(strcasecmp(pBufMode1->pModeSpec->name, pModeSpec->name) > 0)
			break;
		pBufMode0 = pBufMode1;
		pBufMode1 = pBufMode1->next;
		}
	if(pBufMode0 == NULL)
		pBuf->modes = pBufMode2;
	else
		pBufMode0->next = pBufMode2;
	pBufMode2->next = pBufMode1;
	if((pModeSpec->flags & (MdHidden | MdInLine)) != MdHidden)
		supd_windFlags(pBuf, WFMode);
#if MMDebug & Debug_Modes
	dumpModes(pBuf, false, "setBufMode() END\n");
#endif
Retn:
	if(pWasSet != NULL)
		*pWasSet = wasSet;
	return sess.rtn.status;
	}

// Clear a buffer's filename, if any.
void clearBufFilename(Buffer *pBuf) {

	if(pBuf->filename != NULL) {
		free((void *) pBuf->filename);
		pBuf->filename = NULL;
		}
	}

// Get user confirmation for creating a buffer or changing a filename if appropriate flag set and not running a script.  Note
// that the interactive() call is not used here because buffer names and filenames are not entered interactively in a script and
// thus, cannot be mistyped.
bool opConfirm(ushort flags) {

	if((flags & (BF_CreateBuf | BF_WarnExists | BF_Overwrite)) && !(sess.opFlags & OpScript)) {
		bool yep;
		char *prompt, promptBuf[WorkBufSize];

		// Get user confirmation.
		if(flags & BF_CreateBuf)
			prompt = (char *) text140;
				// "Buffer does not exist.  Create it"
		else
			sprintf(prompt = promptBuf, text479, flags & BF_WarnExists ? text480 : text481);
					// "File already exists.  %s", "Allow overwrite", "Overwrite"
		if(terminpYN(prompt, &yep) != Success)
			return sess.rtn.status;
		if(!yep)
			return rsset(Cancelled, 0, NULL);
		}
	return sess.rtn.status;
	}

// Set buffer filename and execute "filename" hook if filename was changed.  If BF_UpdBufDir flag set and filename was changed,
// update buffer directory.  If BF_WarnExists or BF_Overwrite flag set, get user confirmation for filename change first if
// needed.
int setFilename(Buffer *pBuf, const char *filename, ushort flags) {
	bool fnChange = false;
	bool oldIsNull = (pBuf->filename == NULL);
	bool newIsNull = (filename == NULL || *filename == '\0');

	if(oldIsNull) {
		if(!newIsNull) {
			fnChange = true;
			goto Changed;
			}
		}
	else if(newIsNull) {
		clearBufFilename(pBuf);
		fnChange = true;
		}
	else {
		if(fnChange = (strcmp(pBuf->filename, filename) != 0)) {
Changed:
			// Attempting filename change.  Get user confirmation if needed.
			if(fileExists(filename) && opConfirm(flags) != Success)
				return sess.rtn.status;

			// Its a go... change the filename.
			clearBufFilename(pBuf);
			if((pBuf->filename = (char *) malloc(strlen(filename) + 1)) == NULL)
				return rsset(Panic, 0, text94, "setFilename");
					// "%s(): Out of memory!"
			strcpy(pBuf->filename, filename);
			}
		}

	if(fnChange) {
		if(flags & BF_UpdBufDir) {

			// Update buffer directory.
			pBuf->saveDir = (pBuf->filename == NULL || *pBuf->filename == '/') ? NULL : sess.cur.pScrn->workDir;
			}
		(void) execHook(NULL, INT_MIN, hookTable + HkFilename, 2, pBuf->bufname, pBuf->filename);
		}

	return sess.rtn.status;
	}

// Get the default buffer (a guess) for various buffer commands.  If only two visible buffers exist (active or inactive), return
// first one found that is not the current buffer, otherwise NULL.
Buffer *bdefault(void) {
	Datum *pArrayEl;
	Buffer *pBuf, *pBuf1 = NULL;
	int count = 0;
	Array *pArray = &bufTable;

	// Scan buffer list for visible buffers.
	while((pArrayEl = aeach(&pArray)) != NULL) {
		pBuf = bufPtr(pArrayEl);
		if(!(pBuf->flags & BFHidden)) {
			if(++count > 2)
				return NULL;
			if(pBuf != sess.cur.pBuf && pBuf1 == NULL)
				pBuf1 = pBuf;
			}
		}
	return pBuf1;
	}

#if MMDebug & Debug_Narrow
#if 0
static void lput(Line *pLine) {

	if(pLine == NULL)
		fputs("NULL'\n", logfile);
	else
		fprintf(logfile, "%.*s'\n", pLine->used, pLine->text);
	}

static void ldump(const char *name, Line *pLine) {

	fprintf(logfile, "*** %s (%.08x) = '", name, (uint) pLine);
	if(pLine == NULL)
		fputs("'\n", logfile);
	else {
		lput(pLine);
		fprintf(logfile, "\tl_next (%.08x) = '", (uint) pLine->next);
		if(pLine->next == NULL)
			fputs("'\n", logfile);
		else
			lput(pLine->next);
		fprintf(logfile, "\tl_prev (%.08x) = '", (uint) pLine->prev);
		if(pLine->prev == NULL)
			fputs("'\n", logfile);
		else
			lput(pLine->prev);
		}
	}
#endif
#endif

// Check if given buffer is empty and return Boolean result.  If pBuf is NULL, check current buffer.
bool bempty(Buffer *pBuf) {
	Line *pLine;

	if(pBuf == NULL)
		pBuf = sess.cur.pBuf;
	pLine = pBuf->pFirstLine;
	return (pLine->next == NULL && pLine->used == 0);
	}

// Return true if point is at beginning of current buffer, otherwise false.  If point is NULL, use point in current window.
bool bufBegin(Point *pPoint) {

	if(pPoint == NULL)
		pPoint = &sess.cur.pFace->point;
	return pPoint->pLine == sess.cur.pBuf->pFirstLine && pPoint->offset == 0;
	}

// Return true if point is at end of current buffer, otherwise false.  If point is NULL, use point in current window.
bool bufEnd(Point *pPoint) {

	if(pPoint == NULL)
		pPoint = &sess.cur.pFace->point;
	return pPoint->pLine->next == NULL && pPoint->offset == pPoint->pLine->used;
	}

// Inactivate all user marks that are outside the current narrowed buffer by negating their point offsets.
static void markOff(void) {
	Mark *pMark;
	Line *pLine;

	// First, inactivate all user marks in current buffer.
	pMark = &sess.cur.pBuf->markHdr;
	do {
		if(pMark->id <= '~')
			pMark->point.offset = -(pMark->point.offset + 1);
		} while((pMark = pMark->next) != NULL);

	// Now scan the buffer and reactivate the marks that are still in the narrowed region.
	pLine = sess.cur.pBuf->pFirstLine;
	do {
		// Any mark match this line?
		pMark = &sess.cur.pBuf->markHdr;
		do {
			if(pMark->point.pLine == pLine && pMark->point.offset < 0)
				pMark->point.offset = -pMark->point.offset - 1;
			} while((pMark = pMark->next) != NULL);
		} while((pLine = pLine->next) != NULL);
	}

// Narrow to lines or region.  Makes all but the specified line(s) in the current buffer hidden and inaccessible.  Set pRtnVal
// to buffer name and return status.  Note that the last line in the narrowed portion of the buffer inherits the "last line of
// buffer" property; that is, it is assumed to not end with a newline delimiter.
int narrowBuf(Datum *pRtnVal, int n, Datum **args) {
	EScreen *pScrn;
	EWindow *pWind;
	Mark *pMark;
	Line *pLine, *pLine1, *pLineEnd;
	const char *errorMsg;
	Point *pPoint = &sess.cur.pFace->point;

	// Make sure we aren't already narrowed or buffer is empty.
	if(sess.cur.pBuf->flags & BFNarrowed) {
		errorMsg = text71;
			// "%s '%s' is already narrowed"
		goto ErrRtn;
		}
	if(bempty(NULL)) {
		errorMsg = text377;
			// "%s '%s' is empty"
ErrRtn:
		return rsset(Failure, 0, errorMsg, text58, sess.cur.pBuf->bufname);
					// "Buffer"
		}

#if MMDebug & Debug_Narrow
	dumpBuffer("narrowBuf(): BEFORE", NULL, true);
#endif
	// Save faces of all windows displaying current buffer in a mark so they can be restored when buffer is widened.

	// In all screens...
	pScrn = sess.scrnHead;
	do {
		// In all windows...
		pWind = pScrn->windHead;
		do {
			if(pWind->pBuf == sess.cur.pBuf) {

				// Found window attached to current buffer.  Save its face using the window's mark id.
				if(findBufMark(pWind->id, &pMark, MKCreate) != Success)
					return sess.rtn.status;
				setWindMark(pMark, pWind);
				}
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	// Get the boundaries of the current region, if requested.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0) {
		int i = n;

		// Move back abs(n) lines max.
		n = 1;
		while(pPoint->pLine != sess.cur.pBuf->pFirstLine) {
			pPoint->pLine = pPoint->pLine->prev;
			++n;
			if(++i == 0)
				break;
			}
		}
	else if(n == 0 && regionLines(&n) != Success)
		return sess.rtn.status;

	// Current line is now at top of area to be narrowed and n is the number of lines (forward).
	pLine = pPoint->pLine;
	pLine1 = sess.cur.pBuf->pFirstLine;				// Save original first line of buffer...
	pLineEnd = pLine1->prev;					// and last line.

	// Archive the top fragment (hidden portion of buffer above narrowed region).
	if(pLine == pLine1)						// If current line is first line of buffer...
		sess.cur.pBuf->pNarTopLine = NULL;			// set old first line to NULL (no top fragment).
	else {
		sess.cur.pBuf->pNarTopLine = pLine1;			// Otherwise, save old first line of buffer...
		sess.cur.pBuf->pFirstLine = pLine;			// set new first line...
		pLine1->prev = pLine->prev;				// and save pointer to last line of fragment.
		}

	// Move point forward to line just past the end of the narrowed region.
	do {
		if((pPoint->pLine = pPoint->pLine->next) == NULL) {	// If narrowed region extends to bottom of buffer...
			sess.cur.pBuf->pNarBotLine = NULL;		// set old first line to NULL (no bottom fragment)...
			sess.cur.pBuf->pFirstLine->prev = pLineEnd;	// and point narrowed first line to original last line.
			goto FixMarks;
			}
		} while(--n > 0);

	// Narrowed region stops before EOB.  Archive the bottom fragment (hidden portion of buffer below narrowed region).
	(sess.cur.pBuf->pNarBotLine = pPoint->pLine)->prev->next = NULL;
								// Save first line of fragment, terminate line above it...
	sess.cur.pBuf->pFirstLine->prev = pPoint->pLine->prev;	// set new last line of buffer...
	pPoint->pLine->prev = pLineEnd;				// and save pointer to last line of fragment.
FixMarks:
	// Inactivate marks outside of narrowed region.
	markOff();

	// In all screens...
	pScrn = sess.scrnHead;
	do {
		// In all windows...
		pWind = pScrn->windHead;
		do {
			if(pWind->pBuf == sess.cur.pBuf) {

				// Found window attached to narrowed buffer.  Update its buffer settings.
				pWind->face.pTopLine = pWind->face.point.pLine = pLine;
				pWind->face.point.offset = pWind->face.firstCol = 0;
				pWind->flags |= (WFHard | WFMode);
				}
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

#if MMDebug & Debug_Narrow
#if 0
	ldump("sess.cur.pBuf->pNarTopLine", sess.cur.pBuf->pNarTopLine);
	ldump("sess.cur.pBuf->pNarBotLine", sess.cur.pBuf->pNarBotLine);
	ldump("sess.cur.pBuf->pFirstLine", sess.cur.pBuf->pFirstLine);
	ldump("pLine", pLine);
#else
	dumpBuffer("narrowBuf(): AFTER", NULL, true);
#endif
#endif
	// and now remember we are narrowed.
	sess.cur.pBuf->flags |= BFNarrowed;

	return dsetstr(sess.cur.pBuf->bufname, pRtnVal) != 0 ? libfail() : rsset(Success, RSHigh, text73, text58);
								// "%s narrowed", "Buffer"
	}

// Restore a buffer to its pre-narrowed state.
static void unnarrow(Buffer *pBuf) {
	EScreen *pScrn;
	EWindow *pWind;
	Line *pLine, *pLine1, *pLineEnd;
	Mark *pMark;

#if MMDebug & Debug_Narrow
	dumpBuffer("unnarrow(): BEFORE", NULL, true);
#endif
	// Get narrowed first and last lines.
	pLine1 = pBuf->pFirstLine;
	pLineEnd = pLine1->prev;

	// Recover the top fragment.
	if(pBuf->pNarTopLine != NULL) {
		pLine = (pBuf->pFirstLine = pBuf->pNarTopLine)->prev;
							// Point buffer to real first line, get last line of fragment...
		pLine->next = pLine1;			// point last line and narrowed first line to each other...
		pLine1->prev = pLine;
		pBuf->pNarTopLine = NULL;		// and deactivate top fragment.
		}

	// Recover the bottom fragment.
	if(pBuf->pNarBotLine == NULL)			// If none...
		pBuf->pFirstLine->prev = pLineEnd;	// point recovered first line going backward to real last line.
	else {						// otherwise, adjust point and marks.
		// Connect the bottom fragment.
		pLine = pBuf->pNarBotLine->prev;	// Get last line of fragment.
		pLineEnd->next = pBuf->pNarBotLine;	// Point narrowed last line and first line of fragment to each other...
		pBuf->pNarBotLine->prev = pLineEnd;
		pBuf->pFirstLine->prev = pLine;		// point first line going backward to last line...
		pBuf->pNarBotLine = NULL;		// and deactivate bottom fragment.
		}

	// Activate all marks in buffer.
	pMark = &pBuf->markHdr;
	do {
		if(pMark->point.offset < 0)
			pMark->point.offset = -pMark->point.offset - 1;
		} while((pMark = pMark->next) != NULL);

	// Restore faces of all windows displaying current buffer from the window's mark if it exists.

	// In all screens...
	pScrn = sess.scrnHead;
	do {
		// In all windows...
		pWind = pScrn->windHead;
		do {
			if(pWind != sess.cur.pWind && pWind->pBuf == sess.cur.pBuf) {

				// Found window attached to widened buffer.  Restore its face.
				(void) findBufMark(pWind->id, &pMark, MKWind);	// Can't return an error.
				if(pMark != NULL) {
					pWind->face.point = pMark->point;
					pWind->reframeRow = pMark->reframeRow;
					pWind->flags |= WFReframe;
					}
				}
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	// Set hard update in front screen only...
	supd_windFlags(pBuf, WFHard | WFMode);

#if MMDebug & Debug_Narrow
	dumpBuffer("unnarrow(): AFTER", NULL, true);
#endif
	// and now forget that we are narrowed.
	pBuf->flags &= ~BFNarrowed;
	}

// Widen (restore) a narrowed buffer.  Set pRtnVal to buffer name and return status.
int widenBuf(Datum *pRtnVal, int n, Datum **args) {

	// Make sure we are narrowed.
	if(!(sess.cur.pBuf->flags & BFNarrowed))
		return rsset(Failure, 0, text74, text58, sess.cur.pBuf->bufname);
			// "%s '%s' is not narrowed", "Buffer"

	// Restore current buffer to pre-narrowed state.
	unnarrow(sess.cur.pBuf);
	if(dsetstr(sess.cur.pBuf->bufname, pRtnVal) != 0)
		return libfail();
	(void) rsset(Success, RSHigh, text75, text58);
		// "%s widened", "Buffer"
	return execCmdFunc(pRtnVal, INT_MIN, cmdFuncTable + cf_reframeWind, 0, 0);
	}

// binSearch() helper function for returning a buffer name, given table (array) and index.
static const char *fetchBufname(const void *table, ssize_t i) {

	return bufPtr(((Datum **) table)[i])->bufname;
	}

// Search the buffer list for given name and return pointer to Buffer object if found, otherwise NULL.  In either case, set
// *pIndex to target array index if pIndex not NULL.
Buffer *bsrch(const char *bufname, ssize_t *pIndex) {

	// Check current buffer name first, being that it is often "searched" for (but only if index not requested).
	if(pIndex == NULL && strcmp(bufname, sess.cur.pBuf->bufname) == 0)
		return sess.cur.pBuf;

	// No go... search the buffer list.
	ssize_t i;
	bool found = binSearch(bufname, (const void *) bufTable.elements, bufTable.used, strcmp, fetchBufname, &i);
	if(pIndex != NULL)
		*pIndex = i;
	return found ? bufPtr(bufTable.elements[i]) : NULL;
	}

// Generate a valid buffer name from a pathname.  bufname is assumed to point to a buffer that is MaxBufname + 1 or greater
// in size.
static char *fileBufname(char *bufname, const char *filename) {
	char *str;

	// Get file basename with extension and validate it.
	stplcpy(bufname, fbasename(filename, true), MaxBufname + 1);
	if(*bufname == ' ' || *bufname == BCmdFuncLead)	// Convert any leading space or user command/function character...
		*bufname = BAltBufLead;
	stripStr(bufname, 1);				// remove any trailing white space...
	str = bufname;					// convert any non-printable characters...
	do {
		if(*str < ' ' || *str > '~')
			*str = BAltBufLead;
		} while(*++str != '\0');
	return bufname;					// and return result.
	}

// Generate a unique buffer name (from filename if not NULL) by appending digit(s) if needed and return it.  The string buffer
// that bufname points to is assumed to be of size MaxBufname + 1 or greater.
static char *bunique(char *bufname, const char *filename) {
	int n, i;
	char *str0, *str1, *strEnd = bufname + MaxBufname;
	char workBuf[sizeof(int) * 3];

	// Begin with file basename.
	if(filename != NULL)
		fileBufname(bufname, filename);

	// Check if name is already in use.
	while(bsrch(bufname, NULL) != NULL) {

		// Name already exists.  Strip off trailing digits, if any.
		str1 = strchr(bufname, '\0');
		while(str1 > bufname && str1[-1] >= '0' && str1[-1] <= '9')
			--str1;
		str0 = str1;
		n = i = 0;
		while(*str1 != '\0') {
			n = n * 10 + (*str1++ - '0');
			i = 1;
			}

		// Add 1 to numeric suffix (or begin at 0), and put it back.  Assume that MaxBufname is >= number of bytes
		// needed to encode an int in decimal form (10 for 32-bit int) and that we will never have more than INT_MAX
		// buffers (> 2 billion).
		longToAsc((long) (n + i), workBuf);
		n = strlen(workBuf);
		strcpy(str0 + n > strEnd ? strEnd - n : str0, workBuf);
		}

	return bufname;
	}

// Remove buffer from buffer list and return its Datum object.
static Datum *delistBuf(Buffer *pBuf) {
	Datum *pDatum;
	ssize_t index;

	(void) bsrch(pBuf->bufname, &index);		// Can't fail.
	pDatum = adelete(&bufTable, index);
	return pDatum;
	}

// Insert buffer into buffer list at given index.  Return status.
static int enlistBuf(Datum *pDatum, ssize_t index) {

	if(ainsert(&bufTable, pDatum, index, 0) != 0)
		(void) libfail();
	return sess.rtn.status;
	}

// Initialize point position, marks, first column position, and I/O delimiters of a buffer.
static void bufInit(Buffer *pBuf, Line *pLine) {

	faceInit(&pBuf->face, pLine, pBuf);
	*pBuf->inpDelim.u.delim = '\0';
	pBuf->inpDelim.len = 0;
	}

// Check if given buffer name is valid and return Boolean result.
static bool isBufname(const char *name) {
	const char *str;

	// Null?
	if(*name == '\0')
		return false;

	// All printable characters?
	str = name;
	do {
		if(*str < ' ' || *str > '~')
			return false;
		} while(*++str != '\0');

	// First or last character a space?
	if(*name == ' ' || str[-1] == ' ')
		return false;

	// All checks passed.
	return true;
	}

// Create buffer-extension record (CallInfo) for given buffer.  Return status.
int bextend(Buffer *pBuf) {
	CallInfo *pCallInfo;

	if((pBuf->pCallInfo = pCallInfo = (CallInfo *) malloc(sizeof(CallInfo))) == NULL)
		return rsset(Panic, 0, text94, "bextend");
			// "%s(): Out of memory!"
	pCallInfo->minArgs = 0;
	pCallInfo->maxArgs = -1;
	pCallInfo->execCount = 0;
	pCallInfo->execBlocks = NULL;
	dinit(&pCallInfo->argSyntax);
	dinit(&pCallInfo->descrip);
	return sess.rtn.status;
	}

// Find a buffer by name and return status or Boolean result.  Actions taken depend on ctrlFlags:
//	If BS_Derive is set:
//		Use the base filename of "name" as the default buffer name; otherwise, use "name" directly.
//	If BS_Force is set:
//		Create a unique buffer name derived from the default one.  (BS_Create is assumed to also be set.)
//	If BS_Create is set:
//		If the buffer is found and BS_Force is not set:
//			Set *ppBuf (if ppBuf not NULL) to the buffer pointer, set *created (if created not NULL) to false,
//			and return status.
//		If the buffer is not found or BS_Force is set:
//			Create a buffer (with confirmation if BS_Confirm is set), set it's flag word to bufFlags, create a
//			buffer-extension record if BS_Extend is set, run "createBuf" hook if BS_CreateHook is set, set *ppBuf
//			(if ppBuf not NULL) to buffer pointer, set *created (if created not NULL) to true, and return status.
//	If BS_Create is not set:
//		If the buffer is found:
//			Set *ppBuf (if ppBuf not NULL) to the buffer pointer and return true, ignoring created.
//		If the buffer is not found:
//			Return false, ignoring ppBuf and created.
// In all cases, if a buffer is created and BS_CreateHook is set, the "createBuf" hook is executed before returning.
int bfind(const char *name, ushort ctrlFlags, ushort bufFlags, Buffer **ppBuf, bool *created) {
	ssize_t index;
	Buffer *pBuf;
	char *bufname, workBuf[MaxBufname + 1];

	// Set default buffer name.
	if(ctrlFlags & BS_Force)
		(void) bsrch(bufname = (ctrlFlags & BS_Derive) ? bunique(workBuf, name) : bunique(strcpy(workBuf, name), NULL),
		 ctrlFlags & BS_Create ? &index : NULL);
	else {
		if(ctrlFlags & BS_Derive)
			fileBufname(bufname = workBuf, name);
		else
			bufname = (char *) name;

		// Search for the buffer.
		if((pBuf = bsrch(bufname, ctrlFlags & BS_Create ? &index : NULL)) != NULL) {

			// Found it.  Return results.
			if(ppBuf != NULL)
				*ppBuf = pBuf;
			if(ctrlFlags & BS_Create) {
				if(created != NULL)
					*created = false;
				return sess.rtn.status;
				}
			return true;
			}
		}

	// No such buffer exists, create it?
	if(ctrlFlags & BS_Create) {
		Line *pLine;
		Datum *pDatum;
		Buffer newBuf;

		// Valid buffer name?
		if(!isBufname(bufname))
			return rsset(Failure, 0, text447, text128, bufname);
					// "Invalid %s '%s'", "buffer name"

		// User command/function buffer name?
		if(*bufname == BCmdFuncLead && !(bufFlags & BFCmdFunc))
			return rsset(Failure, 0, text268, text180, bufname, BCmdFuncLead);
					// "Cannot %s buffer: name '%s' cannot begin with %c", "create"

		// Get user confirmation if requested.
		if((ctrlFlags & BS_Confirm) && opConfirm(BF_CreateBuf) != Success)
			return sess.rtn.status;

		// Allocate memory for the first line.
		if(lalloc(0, &pLine) != Success)
			return sess.rtn.status;		// Fatal error.
		newBuf.pFirstLine = pLine->prev = pLine;
		pLine->next = NULL;

		// Create buffer extension if requested...
		if(!(ctrlFlags & BS_Extend))
			newBuf.pCallInfo = NULL;
		else if(bextend(&newBuf) != Success)
			return sess.rtn.status;

		// and set up the other buffer fields.
		newBuf.markHdr.next = NULL;
		bufInit(&newBuf, pLine);
		newBuf.pNarTopLine = newBuf.pNarBotLine = NULL;
		newBuf.flags = bufFlags | BFActive;
		newBuf.modes = NULL;
		newBuf.windCount = newBuf.aliasCount = 0;
		newBuf.saveDir = NULL;
		newBuf.filename = NULL;
		strcpy(newBuf.bufname, bufname);

		// Insert a copy of the Buffer object into the list using slot index from bsrch() call and an
		// untracked Datum object.
		if(dnew(&pDatum) != 0 || dsetmem((void *) &newBuf, sizeof(Buffer), pDatum) != 0)
	 		return libfail();
	 	if(enlistBuf(pDatum, index) != Success)
	 		return sess.rtn.status;

		// Add user command or function name to execTable hash...
		if(bufFlags & BFCmdFunc) {
			UnivPtr univ;

			pBuf = bufPtr(pDatum);
			univ.type = (pBuf->flags & BFCommand) ? PtrUserCmd : PtrUserFunc;
			if(execFind((univ.u.pBuf = pBuf)->bufname + 1, OpCreate, univ.type, &univ) != Success)
				return sess.rtn.status;
			}

		// return results to caller...
		if(ppBuf != NULL)
			*ppBuf = bufPtr(pDatum);
		if(created != NULL)
			*created = true;

		// run createBuf hook if requested and return.
		return (ctrlFlags & BS_CreateHook) ? execHook(NULL, INT_MIN, hookTable + HkCreateBuf, 1, bufname) :
		 sess.rtn.status;
		}

	// Buffer not found and not creating.
	return false;
	}

// Free all line storage in given buffer and reset window pointers.  Return status.
static int bfree(Buffer *pBuf) {
	Line *pLine, *pLine1;
	EScreen *pScrn;
	EWindow *pWind;

	// Free all line objects except the first.
	for(pLine = pBuf->pFirstLine->prev; pLine != pBuf->pFirstLine; pLine = pLine1) {
		pLine1 = pLine->prev;
		free((void *) pLine);
		}

	// Free first line of buffer.  If allocated size is small, keep line and simply set "used" size to zero.  Otherwise,
	// allocate a new empty line for buffer so that memory is freed.
	if(pLine->size <= (LineBlockSize << 1)) {
		pLine->used = 0;
		pLine->prev = pLine;
		}
	else {
		if(lalloc(0, &pLine1) != Success)
			return sess.rtn.status;		// Fatal error.
		free((void *) pLine);
		pBuf->pFirstLine = pLine1->prev = pLine = pLine1;
		}
	pLine->next = NULL;

	// Reset window line links.

	// In all screens...
	pScrn = sess.scrnHead;
	do {
		// In all windows...
		pWind = pScrn->windHead;
		do {
			if(pWind->pBuf == pBuf)
				faceInit(&pWind->face, pLine, pBuf);
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	return sess.rtn.status;
	}

// Check if a buffer can be erased and get confirmation from user if needed.  If it's marked as changed, it will not be erased
// unless BC_IgnChgd is set in "flags" or the user gives the okay.  This to save the user the grief of losing text.  If the
// buffer is narrowed and BC_Unnarrow is set, the buffer is silently unnarrowed before being cleared; otherwise, the user is
// prompted to give the okay before proceeding.
//
// Return Success if it's a go, Failure if error, or Cancelled if user changed his or her mind.
int bconfirm(Buffer *pBuf, ushort flags) {
	bool yep;
	bool eraseML = false;
	bool eraseOK = false;

	// Executing buffer?
	if(pBuf->pCallInfo != NULL && pBuf->pCallInfo->execCount > 0)
		return rsset(Failure, 0, text226, text264, text238, pBuf->bufname);
			// "Cannot %s %s buffer '%s'", "clear", "executing"

	// Buffer already empty?  If so and no flags specified, it's a go.
	if(bempty(pBuf) && flags == 0)
		return sess.rtn.status;

	// "Confirm all" or changed buffer?  Skip if need "narrowed buffer" confirmation as well, which preempts this.
	if(((flags & BC_Confirm) || ((pBuf->flags & BFChanged) && !(flags & BC_IgnChgd))) &&
	 (!(pBuf->flags & BFNarrowed) || (flags & BC_Unnarrow))) {
		const char *prompt;
		char promptBuf[WorkBufSize];

		eraseML = true;

		// Get user confirmation.
		if(interactive() && !(flags & BC_ShowName))
			prompt = text32;
				// "Discard changes"
		else {
			if(pBuf->flags & BFChanged)
				sprintf(stpcpy(promptBuf, text32), text369, pBuf->bufname);
					// "Discard changes", " in buffer '%s'"
			else
				sprintf(promptBuf, "%s %s%s '%s'", text26, pBuf->flags & BFHidden ? text453 : "", text83,
							// "Delete",				"hidden ", "buffer"
				 pBuf->bufname);
			prompt = promptBuf;
			}
		if(terminpYN(prompt, &yep) != Success)
			return sess.rtn.status;
		if(!yep)
			goto ClearML;
		}

	// Narrowed buffer?
	if(pBuf->flags & BFNarrowed) {

		// Force?
		if(flags & BC_Unnarrow)

			// Yes, restore buffer to pre-narrowed state.
			unnarrow(pBuf);
		else if(!(flags & BC_IgnChgd)) {

			// Not a force.  Get user confirmation (and leave narrowed).
			eraseML = true;
			if(terminpYN(text95, &yep) != Success)
					// "Narrowed buffer... delete visible portion"
				return sess.rtn.status;
			if(!yep)
				goto ClearML;
			}
		}
	eraseOK = true;

	if(eraseML)
ClearML:
		(void) mlerase(MLForce);
	return eraseOK ? sess.rtn.status : rsset(Cancelled, 0, NULL);
	}

// This routine blows away all of the text in a buffer and returns status.  If BC_ClrFilename flag is set, the filename
// associated with the buffer is set to null.
int bclear(Buffer *pBuf, ushort flags) {

	if(bconfirm(pBuf, flags) != Success)
		return sess.rtn.status;

	// Buffer already empty?  If so and no flags specified, just reset buffer.
	if(bempty(pBuf) && flags == 0)
		goto Unchange;

	// It's a go... erase it.
	if(flags & BC_ClrFilename)
		clearBufFilename(pBuf);				// Zap any filename (buffer will never be narrowed).
	if(bfree(pBuf) != Success)				// Free all line storage and reset line pointers.
		return sess.rtn.status;
	bchange(pBuf, WFHard | WFMode);				// Update window flags.
	if(pBuf->flags & BFNarrowed) {				// If narrowed buffer...
		pBuf->flags |= BFChanged;			// mark as changed...
		faceInit(&pBuf->face, pBuf->pFirstLine, NULL);	// reset buffer line pointers and...
		Mark *pMark = &pBuf->markHdr;			// set all non-window visible marks to beginning of first line.
		do {
			if(pMark->id <= '~' && pMark->point.offset >= 0) {
				pMark->point.pLine = pBuf->pFirstLine;
				pMark->point.offset = 0;
				}
			} while((pMark = pMark->next) != NULL);
		}
	else {
Unchange:
		pBuf->flags &= ~BFChanged;			// Otherwise, mark as not changed...
		bufInit(pBuf, pBuf->pFirstLine);		// reset buffer line pointers, delete marks...
		supd_windFlags(pBuf, WFMode);			// and update mode line(s) if needed.
		}

	return sess.rtn.status;
	}

// Find a window displaying given buffer and return it, giving preference to current screen and current window.  If ppScrn not
// NULL, also set *ppScrn to screen that window is on.
EWindow *findWind(Buffer *pBuf, EScreen **ppScrn) {
	EScreen *pScrn = sess.cur.pScrn;
	EWindow *pWind;

	// If current window is displaying the buffer, use it.
	if(sess.cur.pWind->pBuf == pBuf)
		pWind = sess.cur.pWind;
	else {
		// In current screen...
		pWind = sess.windHead;
		do {
			if(pWind->pBuf == pBuf)
				goto Retn;
			} while((pWind = pWind->next) != NULL);

		// In all other screens...
		pScrn = sess.scrnHead;
		do {
			if(pScrn == sess.cur.pScrn)			// Skip current screen.
				continue;
			if(pScrn->pCurWind->pBuf == pBuf) {		// Use current window first.
				pWind = pScrn->pCurWind;
				break;
				}

			// In all windows...
			pWind = pScrn->windHead;
			do {
				if(pWind->pBuf == pBuf)
					goto Retn;
				} while((pWind = pWind->next) != NULL);
			} while((pScrn = pScrn->next) != NULL);
		}
Retn:
	if(ppScrn != NULL)
		*ppScrn = pScrn;
	return pWind;
	}

// Change zero or more buffer attributes (flags), given result pointer and action (n < 0: clear, n == 0 (default): toggle,
// n == 1: set, or n > 1: clear all and set).  Mutable attributes are: "Changed", "Hidden", and "TermAttr".
// If interactive:
//	. Prompt for one (if default n) or more (otherwise) single-letter attributes via parseOpts().
//	. Use current buffer if default n; otherwise, prompt for buffer name.
// If script mode:
//	. Get buffer name from args[0].
//	. Get zero or more keyword attributes via parseOpts().
// Perform operation, set pRtnVal to former state (-1 or 1) of last attribute altered (or zero if n > 1 and no attributes
// specified), and return status.
int chgBufAttr(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;
	int count;
	long formerState = 0;
	ushort oldBufFlags, newBufFlags, windFlags;
	ushort newFlags = 0;
	int action = (n == INT_MIN) ? 0 : n;
	static OptHdr optHdr = {
		ArgNil1, text374, false, bufAttrTable};
			// "buffer attribute"

	// Interactive?
	if(!(sess.opFlags & OpScript)) {
		DFab prompt;

		// Build prompt.
		if(dopentrack(&prompt) != 0 || dputf(&prompt, 0, "%s %s %s",
		 action < 0 ? text65 : action == 0 ? text231 : action == 1 ? text64 : text296, text83, text186) != 0 ||
				// "Clear", "Toggle", "Set", "Clear all and set"		"buffer", "attribute"
		 dclose(&prompt, FabStr) != 0)
			goto LibFail;

		// Get attribute(s) from user.
		optHdr.single = (n == INT_MIN);
		if(parseOpts(&optHdr, prompt.pDatum->str, NULL, &count) != Success || (count == 0 && action <= 1))
			return sess.rtn.status;

		// Get buffer.  If default n, use current buffer.
		if(n == INT_MIN)
			pBuf = sess.cur.pBuf;
		else {
			// n is not the default.  Get buffer name.  If nothing is entered, bag it.
			pBuf = bdefault();
			if(getBufname(pRtnVal, text229 + 2, pBuf != NULL ? pBuf->bufname : NULL, ArgFirst | OpDelete, &pBuf,
					// ", in"
			 NULL) != Success || disnil(pRtnVal))
				return sess.rtn.status;

			// If "Clear all and set" and no attribute was selected, skip attribute processing.
			if(action > 1 && count == 0)
				goto SetFlags;
			}
		}
	else {
		// Script mode.  Get buffer name argument.
		if((pBuf = bsrch(args[0]->str, NULL)) == NULL)
			return rsset(Failure, 0, text118, args[0]->str);
				// "No such buffer '%s'"

		// Get attribute(s).
		optHdr.single = false;
		if(parseOpts(&optHdr, NULL, args[1], &count) != Success)
			return sess.rtn.status;

		if(count == 0 && action <= 1)
			return rsset(Failure, 0, text455, optHdr.type);
				// "Missing required %s"
		}

	// Scan attribute table and set newFlags.
	if(count > 0) {
		Option *pOpt = bufAttrTable;
		do {
			if(pOpt->ctrlFlags & OptSelected) {
				formerState = (pBuf->flags & pOpt->u.value) ? 1L : -1L;
				newFlags |= pOpt->u.value;
				}
			} while((++pOpt)->keyword != NULL);
		}
SetFlags:
	// Have flag(s) and buffer.  Perform operation.
	oldBufFlags = newBufFlags = pBuf->flags;
	if(action > 1)
		newBufFlags &= ~(BFChanged | BFHidden | BFReadOnly | BFTermAttr);
	if(newFlags != 0) {
		if(action < 0)
			newBufFlags &= ~newFlags;			// Clear.
		else {
			if(action > 0)
				newBufFlags |= newFlags;		// Set.
			else
				newBufFlags ^= newFlags;		// Toggle.

			// Have new buffer flags in newBufFlags.  Check for conflicts.
			if((newBufFlags & (BFCommand | BFTermAttr)) == (BFCommand | BFTermAttr) ||
			 (newBufFlags & (BFFunc | BFTermAttr)) == (BFFunc | BFTermAttr))
				return rsset(Failure, 0, text344, text376);
					// "Operation not permitted on a %s buffer", "user command or function"
			if((newBufFlags & (BFChanged | BFReadOnly)) == (BFChanged | BFReadOnly)) {
				if(interactive())
					tbeep();
				if(pBuf->flags & BFChanged)
					return rsset(Failure, 0, text461, text260, text460);
							// "Cannot set %s buffer to %s", "changed", "read-only"
				return rsset(Failure, 0, text109, text58, text460);
						// "%s is %s", "Buffer", "read-only"
				}
			}
		}

	// All is well... set new buffer flags.
	pBuf->flags = newBufFlags;

	// Set window flags if needed.
	windFlags = (oldBufFlags & (BFChanged | BFReadOnly)) != (newBufFlags & (BFChanged | BFReadOnly)) ? WFMode : 0;
	if((oldBufFlags & BFTermAttr) != (newBufFlags & BFTermAttr))
		windFlags |= WFHard;
	if(windFlags)
		supd_windFlags(pBuf, windFlags);

	// Return former state of last attribute that was changed.
	dsetint(formerState, pRtnVal);

	// Wrap it up.
	return !(sess.opFlags & OpScript) && (newFlags & (BFChanged | BFReadOnly)) && n == INT_MIN ? mlerase(0) :
	 rsset(Success, RSHigh | RSNoFormat, text375);
		// "Attribute(s) changed"
LibFail:
	return libfail();
	}

// Get a buffer name (if n not default) and perform operation on buffer according to "op" argument.  If prompt == NULL (bgets
// function), set pRtnVal to function return value, otherwise buffer name.  Return status.
int bufOp(Datum *pRtnVal, int n, const char *prompt, uint op, int flag) {
	Point *pPoint;
	Buffer *pBuf = NULL;

	// Get the buffer name.  n will never be the default for a bgets() call.
	if(n == INT_MIN)
		pBuf = sess.cur.pBuf;
	else {
		if(prompt != NULL)
			pBuf = bdefault();
		if(getBufname(pRtnVal, prompt, pBuf != NULL ? pBuf->bufname : NULL, ArgFirst | OpDelete, &pBuf, NULL)
		 != Success || pBuf == NULL)
			return sess.rtn.status;
		}

	// Perform requested operation (BO_GotoLine, BO_BeginEnd, or BO_ReadBuf).
	if(op == BO_GotoLine && flag == 0) {
		op = BO_BeginEnd;
		flag = true;
		}

	// Move point in buffer... usually a massive adjustment.  Grab point.  If the buffer is being displayed, get from window
	// face instead of buffer face.
	if(pBuf->windCount > 0) {
		EWindow *pWind = findWind(pBuf, NULL);
		pWind->flags |= WFMove;			// Set "hard motion" flag.
		pPoint = &pWind->face.point;
		}
	else
		pPoint = &pBuf->face.point;

	// Perform operation.
	switch(op) {
		case BO_BeginEnd:
			// Go to beginning or end of buffer.
			if(flag) {		// EOB
				pPoint->pLine = pBuf->pFirstLine->prev;
				pPoint->offset = pPoint->pLine->used;
				}
			else {			// BOB
				pPoint->pLine = pBuf->pFirstLine;
				pPoint->offset = 0;
				}
			break;
		case BO_GotoLine:
			// Go to beginning of buffer and count lines.
			pPoint->pLine = pBuf->pFirstLine;
			pPoint->offset = 0;
			if(pBuf == sess.cur.pBuf)
				return moveLine(flag - 1);
			for(--flag; flag > 0; --flag) {
				if(pPoint->pLine->next == NULL)
					break;
				pPoint->pLine = pPoint->pLine->next;
				}
			break;
		default:	// BO_ReadBuf
			// Read next line from buffer n times.
			while(n-- > 0) {
				size_t len;

				// If we are at EOB, return nil.
				if(bufEnd(pPoint)) {
					dsetnil(pRtnVal);
					return sess.rtn.status;
					}

				// Get the length of the current line (from point to end)...
				len = pPoint->pLine->used - pPoint->offset;

				// return the spoils...
				if(dsetsubstr(pPoint->pLine->text + pPoint->offset, len, pRtnVal) != 0)
					return libfail();

				// and step the buffer's line pointer ahead one line (or to end of last line).
				if(pPoint->pLine->next == NULL)
					pPoint->offset = pPoint->pLine->used;
				else {
					pPoint->pLine = pPoint->pLine->next;
					pPoint->offset = 0;
					}
				}
		}

	return sess.rtn.status;
	}

// Set name of a system (internal) buffer and call bfind();
int sysBuf(const char *root, Buffer **ppBuf, ushort flags) {
	char bufname[strlen(root) + 2];

	bufname[0] = BSysLead;
	strcpy(bufname + 1, root);
	return bfind(bufname, BS_Create | BS_Force, BFHidden | flags, ppBuf, NULL);
	}

// Activate a buffer if needed.  Return status.
int bactivate(Buffer *pBuf) {

	// Check if buffer is active.
	if(!(pBuf->flags & BFActive)) {

		// Not active: check for directory mismatch.
		if(pBuf->saveDir != NULL && pBuf->saveDir != sess.cur.pScrn->workDir) {
			char workBuf[WorkBufSize];

			sprintf(workBuf, text415, text309, pBuf->bufname);
				// "%s buffer '%s'", "Directory unknown for"
			(void) rsset(Failure, RSHigh, text141, workBuf, pBuf->filename);
				// "I/O Error: %s, file \"%s\""
			}
		else
			// All is well.  Read attached file into buffer.
			(void) readIn(pBuf, NULL, RWReadHook | RWKeep | RWStats);
		}

	return sess.rtn.status;
	}

// Insert a buffer into the current buffer and set current region to inserted lines.  If n == 0, leave point before the inserted
// lines, otherwise after.  Return status.
int insertBuf(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;
	DataInsert dataInsert = {
		sess.cur.pBuf,	// Target buffer.
		NULL,		// Target line.
		text153		// "Inserting data..."
		};

	// Get the buffer name.  Reject if current buffer.
	pBuf = bdefault();
	if(getBufname(pRtnVal, text55, pBuf != NULL ? pBuf->bufname : NULL, ArgFirst | OpDelete, &pBuf, NULL) != Success)
			// "Insert"
		return sess.rtn.status;
	dclear(pRtnVal);
	if(pBuf == NULL)			// Interactive and nothing entered.
		return sess.rtn.status;
	if(pBuf == sess.cur.pBuf)
		return rsset(Failure, RSNoFormat, text124);
			// "Cannot insert current buffer into itself"

	// Let user know what's up.
	if(mlputs(MLHome | MLWrap | MLFlush, text153) != Success)
			// "Inserting data..."
		return sess.rtn.status;

	// Prepare buffer to be inserted.  Return after activation if buffer is empty.
	if(bactivate(pBuf) != Success)
		return sess.rtn.status;
	if(bempty(pBuf))
		return rsset(Success, RSHigh, "%s 0 %ss", text154, text205);
						// "Inserted", "line"

	// Buffer not empty.  Insert lines and report results.
	return insertData(n, pBuf, &dataInsert) != Success ? sess.rtn.status : rsset(Success, RSHigh, "%s %u %s%s%s",
	 text154, dataInsert.lineCt, text205, dataInsert.lineCt == 1 ? "" : "s", text355);
		// "Inserted", "line", " and marked as region"
	}

// Attach a buffer to the current window, creating it if necessary (default).  Render buffer and return status.
int selectBuf(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;
	bool created;
	const char *prompt;
	uint op;

	// Get the buffer name.
	pBuf = bdefault();
	if(n == -1) {
		prompt = text27;
			// "Pop"
		op = OpDelete;
		}
	else {
		prompt = (n == INT_MIN || n == 1) ? text113 : text24;
						// "Switch to", "Select"
		op = OpCreate | OpConfirm;
		}
	if(getBufname(pRtnVal, prompt, pBuf != NULL ? pBuf->bufname : NULL, ArgFirst | op, &pBuf, &created) != Success ||
	 pBuf == NULL)
		return sess.rtn.status;

	// Render the buffer.
	return render(pRtnVal, n == INT_MIN ? 1 : n, pBuf, created ? RendNewBuf | RendNotify : 0);
	}

// Display a file or buffer in a pop-up window with options.  Return status.
int doPop(Datum *pRtnVal, int n, bool popBuf) {
	Buffer *pBuf;
	int count = 0;
	ushort bufFlags;
	ushort rendFlags = 0;
	bool created = false;
	ushort oldMsgFlag = modeInfo.cache[MdIdxRtnMsg]->flags & MdEnabled;
	static Option options[] = {
		{"^Existing", "^Exist", 0, 0},
		{"^AltModeLine", "^AltML", 0, RendAltML},
		{"^Shift", "^Shft", 0, RendShift},
		{"^TermAttr", "^TAttr", 0, 0},
		{"^Delete", "^Del", 0, RendNewBuf},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text223, false, options};
			// "pop option"

	// Get buffer name or filename.
	if(popBuf) {
		// Get buffer name.
		pBuf = bdefault();
		if(getBufname(pRtnVal, text27, pBuf != NULL ? pBuf->bufname : NULL, ArgFirst | OpDelete, &pBuf, NULL)
				// "Pop"
		 != Success || pBuf == NULL)
			return sess.rtn.status;
		options[0].ctrlFlags |= OptIgnore;
		}
	else {
		// Get filename.
		if(getFilename(pRtnVal, text299, NULL, ArgFirst) != Success || disnil(pRtnVal))
				// "Pop"
			return sess.rtn.status;
		options[0].ctrlFlags &= ~OptIgnore;
		}

	// Get options if applicable.
	if(n != INT_MIN && parseOpts(&optHdr, text448, NULL, &count) != Success)
				// "Options"
		return sess.rtn.status;

	if(oldMsgFlag)
		clearGlobalMode(modeInfo.cache[MdIdxRtnMsg]);

	// Open and read file in pRtnVal, if applicable.
	if(!popBuf) {
		ushort ctrlFlags = (count > 0 && (options[0].ctrlFlags & OptSelected)) ? BS_Create | BS_Derive :
		 BS_Create | BS_Derive | BS_Force;
		if(bfind(pRtnVal->str, ctrlFlags, 0, &pBuf, &created) != Success ||
		 (created && readIn(pBuf, pRtnVal->str, RWKeep | RWExist) != Success))
			return sess.rtn.status;
		if(created)
			rendFlags |= RendNewBuf;
		}

	// Process options.
	if(count > 0) {
		rendFlags = getFlagOpts(options);
		if(options[3].ctrlFlags & OptSelected) {		// "TermAttr"
			if(pBuf->flags & BFCmdFunc)
				return rsset(Failure, 0, text344, text376);
					// "Operation not permitted on a %s buffer", "user command or function"
			pBuf->flags |= BFTermAttr;
			}
		}

	bufFlags = pBuf->flags;		// Save buffer flags for possible restore after pop.

	// Render the buffer and return notice if it is deleted and was not just created.
	if(render(pRtnVal, -1, pBuf, rendFlags) == Success) {
		if(oldMsgFlag)
			setGlobalMode(modeInfo.cache[MdIdxRtnMsg]);
		if(!(rendFlags & RendNewBuf))
			pBuf->flags = bufFlags;
		else if(!created)
			(void) rsset(Success, RSHigh, "%s %s", text58, text10);
						// "Buffer", "deleted"
		}

	return sess.rtn.status;
	}

// Create a scratch buffer; that is, one with a unique name and no filename associated with it.  Set *ppBuf to buffer pointer
// and return status.
int bscratch(Buffer **ppBuf) {
	Buffer *pBuf;
	char bufname[MaxBufname + 1];

	// Create a buffer with a unique name...
	sprintf(bufname, "%s%u", Scratch, scratchBufNum++);
	(void) bfind(bufname, BS_Create | BS_CreateHook | BS_Force, 0, &pBuf, NULL);

	// and return it.
	*ppBuf = pBuf;
	return sess.rtn.status;
	}

// Create a scratch buffer.  Render buffer and return status.
int scratchBuf(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;

	// Create buffer...
	if(bscratch(&pBuf) != Success)
		return sess.rtn.status;

	// and render it.
	return render(pRtnVal, n == INT_MIN ? 1 : n, pBuf, RendNewBuf);
	}

// Run exit-buffer hook (and return result) or enter-buffer hook.  Return status.
int runBufHook(Datum **ppRtnVal, ushort flags) {

	if(!(sess.cur.pBuf->flags & BFCmdFunc)) {
		if(*ppRtnVal == NULL && dnewtrack(ppRtnVal) != 0)
			return libfail();
		if(flags & SWB_ExitHook) {

			// Run exit-buffer user hook on current (old) buffer.
			dsetnil(*ppRtnVal);
			(void) execHook(*ppRtnVal, INT_MIN, hookTable + HkExitBuf, 0);
			}
		else
			// Run enter-buffer user hook on current (new) buffer.
			(void) execHook(*ppRtnVal, INT_MIN, hookTable + HkEnterBuf, 0x21, *ppRtnVal);
		}
	return sess.rtn.status;
	}

// Switch to given buffer in current window, update pLastBuf variable in current screen (per flags), and return status.  The top
// line, point, and display column offset values from the current window are saved in the old buffer's header and replacement
// ones are fetched from the new (given) buffer's header.
int bswitch(Buffer *pBuf, ushort flags) {
	Datum *pRtnVal = NULL;

	// Nothing to do if pBuf is current buffer.
	if(pBuf == sess.cur.pBuf)
		return sess.rtn.status;

	// Run exit-buffer user hook on current (old) buffer, if applicable.
	if(!(flags & SWB_NoBufHooks) && runBufHook(&pRtnVal, SWB_ExitHook) != Success)
		return sess.rtn.status;

	// Decrement window use count of current (old) buffer and save the current window settings.
	--sess.cur.pBuf->windCount;
	windFaceToBufFace(sess.cur.pWind, sess.cur.pBuf);
	if(!(flags & SWB_NoLastBuf))
		sess.cur.pScrn->pLastBuf = sess.cur.pBuf;

	// Switch to new buffer.
	sess.cur.pWind->pBuf = sess.edit.pBuf = sess.cur.pBuf = pBuf;
	++pBuf->windCount;

	// Activate buffer.
	if(bactivate(sess.cur.pBuf) <= MinExit)
		return sess.rtn.status;

	// Update window settings.
	bufFaceToWindFace(sess.cur.pBuf, sess.cur.pWind);

	// Run enter-buffer user hook on current (new) buffer, if applicable.
	if(sess.rtn.status == Success && !(flags & SWB_NoBufHooks))
		(void) runBufHook(&pRtnVal, 0);

	return sess.rtn.status;
	}

// Switch to the previous or next visible buffer in the buffer list per flags and return status.  Do nothing if not found.  Set
// pRtnVal (if not NULL) to name of last buffer switched to, or leave as is (nil) if no switch occurred.
//
// Buffers are selected per flags:
//	BT_Backward	Traverse buffer list backward, otherwise forward.
//	BT_Hidden	Include hidden, non-command/function buffers as candidates.
//	BT_HomeDir	Select buffers having same home directory as current screen only.
//	BT_Delete	Delete the current buffer after first switch.
int prevNextBuf(Datum *pRtnVal, ushort flags) {
	Buffer *pBuf0, *pBuf1;
	Datum **ppBufItem0, **ppBufItem, **ppBufItemEnd;
	ssize_t index;
	int incr = (flags & BT_Backward) ? -1 : 1;
	const char *homeDir = sess.cur.pScrn->workDir;

	// Get the current buffer list pointer.
	(void) bsrch(sess.cur.pBuf->bufname, &index);
	ppBufItem0 = bufTable.elements + index;

	// Find the first buffer before or after the current one that meets the criteria.
	ppBufItem = ppBufItem0 + incr;
	ppBufItemEnd = bufTable.elements + bufTable.used;
	for(;;) {
		// Wrap around buffer list if needed.
		if(ppBufItem < bufTable.elements || ppBufItem == ppBufItemEnd)
			ppBufItem = (flags & BT_Backward) ? ppBufItemEnd - 1 : bufTable.elements;

		if(ppBufItem == ppBufItem0) {

			// We're back where we started... buffer not found.  Bail out (do nothing).
			return sess.rtn.status;
			}
		pBuf1 = bufPtr(*ppBufItem);
		if(!(pBuf1->flags & BFCmdFunc) && (pBuf1->saveDir == homeDir || !(flags & BT_HomeDir)) &&
		 (!(pBuf1->flags & BFHidden) || (flags & BT_Hidden)))
			break;
		ppBufItem += incr;
		}

	// Buffer found... switch to it.
	pBuf0 = sess.cur.pBuf;
	if(bswitch(pBuf1, 0) != Success)
		return sess.rtn.status;
	if(flags & BT_Delete) {
		char bufname[MaxBufname + 1];

		strcpy(bufname, pBuf0->bufname);
		if(bdelete(pBuf0, 0) == Success)
			(void) rsset(Success, RSForce, text372, bufname);
					// "Buffer '%s' deleted"
		}

	return (pRtnVal != NULL && dsetstr(pBuf1->bufname, pRtnVal) != 0) ? libfail() : sess.rtn.status;
	}

// Clear current buffer, or named buffer if n argument.  Force it if n < 0.  Set pRtnVal to false if buffer is not cleared,
// otherwise true.  Return status.
int clearBuf(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;

	// Get the buffer name to clear.
	if(n == INT_MIN)
		pBuf = sess.cur.pBuf;
	else {
		pBuf = bdefault();
		if(getBufname(pRtnVal, text169, pBuf != NULL ? pBuf->bufname : NULL, ArgFirst | OpDelete, &pBuf, NULL)
				// "Clear"
		 != Success || pBuf == NULL)
			return sess.rtn.status;
		}

	// Blow text away unless user gets cold feet.
	if(bclear(pBuf, n < 0 && n != INT_MIN ? BC_IgnChgd : 0) >= Cancelled)
		dsetbool(sess.rtn.status == Success, pRtnVal);

	return sess.rtn.status;
	}

// Check if an attribute is set in a buffer and set pRtnVal to Boolean result.  Return status.
int bufAttrQ(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;
	char *attr = args[0]->str;
	bool result = false;
	Option *pOpt = bufAttrTable;

	// Get buffer.
	if(args[1] == NULL)
		pBuf = sess.cur.pBuf;
	else if((pBuf = bsrch(args[1]->str, NULL)) == NULL)
		return rsset(Failure, 0, text118, args[1]->str);
			// "No such buffer '%s'"

	// Scan buffer attribute table for a match.
	do {
		if(strcasecmp(attr, *pOpt->keyword == '^' ? pOpt->keyword + 1 : pOpt->keyword) == 0) {
			if(pBuf->flags & pOpt->u.value)
				result = true;
			goto Found;
			}
		} while((++pOpt)->keyword != NULL);

	// Match not found... return an error.
	return rsset(Failure, 0, text447, text374, attr);
			// "Invalid %s '%s'", "buffer attribute"
Found:
	dsetbool(result, pRtnVal);
	return sess.rtn.status;
	}

// Delete marks, given buffer pointer.  Free all visible user marks, plus invisible user marks if MKViz flag is not set,
// plus window marks if MKWind flag is set -- then set root mark to default.
void delBufMarks(Buffer *pBuf, ushort flags) {
	Mark *pMark0, *pMark1, *pMark2;
	Mark *pMark = &pBuf->markHdr;

	// Free marks in list.
	for(pMark1 = (pMark0 = pMark)->next; pMark1 != NULL; pMark1 = pMark2) {
		pMark2 = pMark1->next;
		if((pMark1->id <= '~' && (pMark1->point.offset >= 0 || !(flags & MKViz))) ||
		 (pMark1->id > '~' && (flags & MKWind))) {
			free((void *) pMark1);
			pMark0->next = pMark2;
			}
		else
			pMark0 = pMark1;
		}

	// Initialize root mark to end of buffer.
	pMark->id = RegionMark;
	pMark->point.offset = (pMark->point.pLine = pBuf->pFirstLine->prev)->used;
	pMark->reframeRow = 0;
	}

// Find all windows where given buffer is being displayed and switch to another buffer in each if possible.  The buffer to
// switch to is selected as follows:
//	1. Switch to last buffer displayed on associated screen if remembered, different than given buffer, and not a user
//	   command or function.
//	2. Switch to next visible buffer in buffer list with same home directory if it exists.
//	3. Switch to next visible buffer in buffer list if it exists.
//	4. Switch to next hidden, non-command/function buffer in buffer list if it exists.
// Return status or an error if a suitable buffer is not found.
static int bundisplay(Buffer *pBuf) {
	Buffer *pLastBuf;
	EWindow *pOldWind, *pWind;
	EScreen *pOldScrn, *pScrn;

	// Save current screen and window.
	pOldScrn = sess.cur.pScrn;
	pOldWind = sess.cur.pWind;

	// In all screens...
	pScrn = sess.scrnHead;
	do {
		// Disable remembered buffer for this screen if same as one to undisplay or is a user command or function.
		if((pLastBuf = pScrn->pLastBuf) != NULL && (pLastBuf == pBuf || (pLastBuf->flags & BFCmdFunc)))
			pLastBuf = NULL;

		// In all windows...
		pWind = pScrn->windHead;
		do {
			if(pWind->pBuf == pBuf) {

				// Found window displaying buffer... switch to it.
				(void) sswitch(pScrn, SWB_NoBufHooks);		// Can't fail.
				(void) wswitch(pWind, SWB_NoBufHooks);		// Can't fail.
				if(pLastBuf != NULL) {				// Try "last buffer" first.
					if(bswitch(pLastBuf, 0) != Success)
						goto Retn;
					}					// Not available.  Try next homed buffer.
				else if(prevNextBuf(NULL, BT_HomeDir) != Success)
					goto Retn;
				else if(sess.cur.pBuf == pBuf) {		// None found.  Try next visible buffer.
					if(prevNextBuf(NULL, 0) != Success)
						goto Retn;
					else if(sess.cur.pBuf == pBuf) {	// None found.  Try hidden.
						if(prevNextBuf(NULL, BT_Hidden) != Success)
							goto Retn;
						if(sess.cur.pBuf == pBuf) {

							// No buffer found to switch to... it's a bust!
							return rsset(Failure, RSNoFormat, text41);
								// "Only one buffer exists"
							}
						}
					}
				}
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	// Restore original screen and window.
	(void) sswitch(pOldScrn, SWB_NoBufHooks);
	(void) wswitch(pOldWind, SWB_NoBufHooks);
Retn:
	return sess.rtn.status;
	}

// Delete the buffer pointed to by pBuf.  Don't allow if buffer is being executed or aliased, or is a user function bound to a
// hook.  If buffer is being displayed, switch to a different buffer in all its windows first if possible; otherwise, return an
// error.  If buffer can be deleted, pass "flags" with BC_Unnarrow set to bclear() to clear the buffer, then free the header
// line and the buffer block.  Also delete any key binding and remove the execTable entry if it's a user command or function.
// Return status, including Cancelled if user changed his or her mind.
int bdelete(Buffer *pBuf, ushort flags) {
	UnivPtr univ;
	KeyBind *pKeyBind;
	EScreen *pScrn;

	// We cannot nuke an executing buffer.
	if(pBuf->pCallInfo != NULL && pBuf->pCallInfo->execCount > 0)
		return rsset(Failure, 0, text226, text263, text238, pBuf->bufname);
			// "Cannot %s %s buffer '%s'", "delete", "executing"

	// We cannot nuke a aliased buffer.
	if(pBuf->aliasCount > 0)
		return rsset(Failure, 0, text272, text412, text247, pBuf->aliasCount);
			// "%s or %s has %hu alias(es)", "User command", "function"

	// We cannot nuke a user function bound to a hook.
	if((pBuf->flags & BFFunc) && isHook(pBuf, true))
		goto Retn;

	// "Undisplay" buffer if needed.
	if(pBuf->windCount > 0 && bundisplay(pBuf) != Success)
		goto Retn;

	// It's a go.  Blow text away (unless user got cold feet).
	if(bclear(pBuf, flags | BC_ClrFilename | BC_Unnarrow) != Success)
		goto Retn;

	// Delete execTable entry.
	if((pBuf->flags & BFCmdFunc) && execFind(pBuf->bufname + 1, OpDelete, 0, NULL) != Success)
		goto Retn;

	delBufMarks(pBuf, MKWind);			// Delete all marks.
	univ.u.pBuf = pBuf;				// Get key binding, if any.
	pKeyBind = getPtrEntry(&univ);

	if(sess.pSavedBuf == pBuf)			// Unsave buffer if saved.
		sess.pSavedBuf = NULL;
	pScrn = sess.scrnHead;				// Clear from all screens' "last buffer".
	do {
		if(pScrn->pLastBuf == pBuf)
			pScrn->pLastBuf = NULL;
		} while((pScrn = pScrn->next) != NULL);
	preprocFree(pBuf);				// Release any script preprocessor storage.
	if(pBuf->pCallInfo != NULL)
		free((void *) pBuf->pCallInfo);		// Release buffer extension record.
	dfree(delistBuf(pBuf));				// Remove from buffer list and destroy Buffer and Datum objects.
	if(pKeyBind != NULL)
		unbind(pKeyBind);			// Delete buffer key binding.
Retn:
	return sess.rtn.status;
	}

// Delete buffers per "dflags" and return status.  If BD_Homed flag set, use "workDir" as home directory to match on.  Set
// *pCount to number of buffers deleted.
int bnuke(ushort dflags, const char *workDir, int *pCount) {
	Buffer *pBuf;
	Datum **ppBufItem = bufTable.elements;
	Datum **ppBufItemEnd = ppBufItem + bufTable.used;
	ushort cflags = (dflags & BD_Confirm) ? (BC_ShowName | BC_Confirm) :
	 (dflags & BD_Force) ? (BC_ShowName | BC_IgnChgd) : BC_ShowName;
	int count = 0;

	*pCount = 0;

	// Loop through buffer list.
	do {
		pBuf = bufPtr(*ppBufItem);

		// Skip buffer if:
		//  1. a user command or function;
		//  2. being displayed and BD_Displayed flag not specified;
		//  3. not homed to current screen and BD_Homed flag specified;
		//  4. hidden and BD_Hidden flag not specified;
		//  5. modified and BD_Unchanged flag specified;
		//  6. active and BD_Inactive flag specified.
		if((pBuf->flags & BFCmdFunc) ||
		 (pBuf->windCount > 0 && !(dflags & BD_Displayed)) ||
		 (pBuf->saveDir != workDir && (dflags & BD_Homed)) ||
		 ((pBuf->flags & BFHidden) && !(dflags & BD_Hidden)) ||
		 ((pBuf->flags & BFChanged) && (dflags & BD_Unchanged)) ||
		 ((pBuf->flags & BFActive) && (dflags & BD_Inactive)))
			++ppBufItem;
		else {
			// Nuke it (if confirmed), but don't increment ppBufItem if successful because array element it
			// points to will be removed.  Decrement ppBufItemEnd instead.
			if(!(cflags & BC_Confirm) && (!(pBuf->flags & BFChanged) || (cflags & BC_IgnChgd))) {
				if(mlprintf(MLHome | MLWrap | MLFlush, text367, pBuf->bufname) != Success)
						// "Deleting buffer %s..."
					return sess.rtn.status;
				if(interactive())
					centiPause(50);
				}
			if(bdelete(pBuf, cflags) < Cancelled)
				break;
			if(sess.rtn.status == Success) {
				++count;
				--ppBufItemEnd;
				}
			else {
				rsclear(0);
				++ppBufItem;
				}
			}
		} while(ppBufItem < ppBufItemEnd);

	// Done.
	*pCount = count;
	return sess.rtn.status;
	}

// Delete buffer(s) and return status.  Operation is controlled by n argument and options:
//
// If default n or n <= 0:
//	Delete named buffer(s) with confirmation if changed.  If n < 0, ignore changes (skip confirmation).
//
// If n > 0, first (and only) argument is a string containing comma-separated options.  One of the following must be specified:
//	Visible		Select all visible, non-displayed buffers.
//	Homed		Select visible, non-displayed buffers "homed" to current screen only.
//	Inactive	Select inactive, visible, non-displayed buffers only.
//	Unchanged	Select unchanged, visible, non-displayed buffers only.
// and zero or more of the following:
//	Displayed	Include buffers being displayed.
//	Hidden		Include hidden buffers.
// and zero or one of the following:
//	Confirm		Get confirmation for each buffer selected (changed or unchanged).
//	Force		Delete all buffers selected, with initial confirmation.
// Additionally, the following rules apply:
//  1. User command and function buffers are skipped unconditionally.
//  2. If neither "Confirm" nor "Force" is specified, confirmation is requested for changed buffers only.
//  3. If "Force" is specified, changed and/or narrowed buffers are deleted without confirmation.
int delBuf(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;
	uint count;
	bool delError = false;
	ushort dflags;
	char workBuf[64];
	static Option options[] = {
		{"^Visible", "^Viz", 0, .u.value = BD_Visible},
		{"^Unchanged", "^Unchg", 0, .u.value = BD_Unchanged},
		{"Ho^med", "Ho^m", 0, .u.value = BD_Homed},
		{"^Inactive", "^Inact", 0, .u.value = BD_Inactive},
		{"^Displayed", "^Disp", 0, .u.value = BD_Displayed},
		{"^Hidden", "^Hid", 0, .u.value = BD_Hidden},
		{"^Confirm", "^Cfrm", 0, .u.value = BD_Confirm},
		{"^Force", "^Frc", 0, .u.value = BD_Force},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		ArgFirst, text410, false, options};
			// "command option"

	dsetint(0, pRtnVal);

	// Check if processing multiple unnamed buffers.
	if(n > 0) {
		Option *pOpt, *pOptEnd;

		// Build prompt if interactive.
		if(sess.opFlags & OpScript)
			workBuf[0] = '\0';
		else
			sprintf(workBuf, "%s %ss", text26, text83);
					// "Delete", "buffer"

		// Get op codes, set flags, and validate them.
		if(parseOpts(&optHdr, workBuf, NULL, &count) != Success || count == 0)
			return sess.rtn.status;
		count = dflags = 0;
		pOptEnd = (pOpt = options) + 4;
		do {
			if(pOpt->ctrlFlags & OptSelected) {
				dflags |= pOpt->u.value;
				if(pOpt < pOptEnd)
					++count;
				}
			} while((++pOpt)->keyword != NULL);
		if(count == 0)
			return rsset(Failure, 0, text455, text410);
				// "Missing required %s", "command option"
		if(count > 1 || (dflags & (BD_Confirm | BD_Force)) == (BD_Confirm | BD_Force))
			return rsset(Failure, 0, text454, text410);
				// "Conflicting %ss", "command option"

		// Get confirmation if interactive and force-deleting all other buffers.
		if(interactive() && (dflags & (BD_Visible | BD_Force)) == (BD_Visible | BD_Force)) {
			bool yep;

			sprintf(workBuf, text168, dflags & BD_Hidden ? "" : text452);
				// "Delete all other %sbuffers", "visible "

			if(terminpYN(workBuf, &yep) != Success)
				return sess.rtn.status;
			if(!yep)
				return mlerase(0);
			}

		// It's a go.  Nuke the selected buffers.
		if(bnuke(dflags, sess.cur.pScrn->workDir, &count) == Success) {
			dsetint(count, pRtnVal);
			(void) rsset(Success, RSHigh, text368, count, count != 1 ? "s" : "");
						// "%d buffer%s deleted"
			}
		return sess.rtn.status;
		}

	// Process named buffer(s).
	count = 0;
	if(n == INT_MIN)
		n = 0;		// No force.

	// If interactive, get buffer name from user.
	if(!(sess.opFlags & OpScript)) {
		pBuf = bdefault();
		if(getBufname(pRtnVal, text26, pBuf != NULL ? pBuf->bufname : NULL, ArgFirst | OpDelete, &pBuf, NULL)
				// "Delete"
		 != Success || pBuf == NULL)
			return sess.rtn.status;
		goto NukeIt;
		}

	// Script mode: get buffer name(s) to delete.
	Datum *pBufname;
	uint argFlags = ArgFirst | ArgNotNull1;
	if(dnewtrack(&pBufname) != 0)
		return libfail();
	do {
		if(!(argFlags & ArgFirst) && !haveSym(s_comma, false))
			break;					// No arguments left.
		if(funcArg(pBufname, argFlags) != Success)
			return sess.rtn.status;
		argFlags = ArgNotNull1;
		if((pBuf = bsrch(pBufname->str, NULL)) == NULL)
			(void) rsset(Success, RSNoWrap, text118, pBufname->str);
				// "No such buffer '%s'"
		else {
NukeIt:
			// Nuke it.
			if(bdelete(pBuf, n < 0 ? BC_IgnChgd : 0) != Success) {

				// Don't cause a running script to fail on Failure status.
				if(sess.rtn.status < Failure)
					return sess.rtn.status;
				delError = true;
				(void) rsunfail();
				}
			else
				++count;
			}
		} while(sess.opFlags & OpScript);

	// Return count if no error.
	if(!delError) {
		dsetint((long) count, pRtnVal);
		if(interactive())
			(void) rsset(Success, RSHigh, "%s %s", text58, text10);
						// "Buffer", "deleted"
		}

	return sess.rtn.status;
	}

// Rename a buffer and set pRtnVal (if not NULL) to the new name.  If BR_Auto set in flags, derive the name from the filename
// attached to the buffer and use it, without prompting; otherwise, get new buffer name (and use BR_Current flag to determine
// correct prompt if interactive).  Return status.
int brename(Datum *pRtnVal, ushort flags, Buffer *pTargBuf) {
	Buffer *pBuf;
	Array *pArray;
	ssize_t index;
	const char *prompt;
	Datum *pDatum;
	Datum *pBufname;			// New buffer name.
	TermInpCtrl termInpCtrl = {NULL, RtnKey, MaxBufname, NULL};
	char askStr[WorkBufSize];

	// We cannot rename an executing buffer.
	if(pTargBuf->pCallInfo != NULL && pTargBuf->pCallInfo->execCount > 0)
		return rsset(Failure, 0, text284, text275, text248);
			// "Cannot %s %s buffer", "rename", "an executing"

	if(dnewtrack(&pBufname) != 0)
		goto LibFail;

	// Auto-rename if BR_Auto flag set.  Do nothing if buffer name is already the target name.
	if(flags & BR_Auto) {
		if(pTargBuf->filename == NULL)
			return rsset(Failure, 0, text145, pTargBuf->bufname);
					// "No filename associated with buffer '%s'"
		if(dsalloc(pBufname, MaxBufname + 1) != 0)
			goto LibFail;
		if(strcmp(fileBufname(pBufname->str, pTargBuf->filename), pTargBuf->bufname) == 0)
			return sess.rtn.status;
		bunique(pBufname->str, NULL);
		goto SetNew;
		}

	// Set initial prompt.
	prompt = (flags & BR_Current) ? text385 : text339;
			// "Change buffer name to", "to";

Ask:
	// Get the new buffer name.
	if(sess.opFlags & OpScript) {
		if(funcArg(pBufname, ArgNotNull1) != Success)
			return sess.rtn.status;
		}
	else if(termInp(pBufname, prompt, ArgNotNull1 | ArgNil1, 0, &termInpCtrl) != Success || disnil(pBufname))
		return sess.rtn.status;

	// Valid buffer name?
	if(!isBufname(pBufname->str)) {
		if(sess.opFlags & OpScript)
			return rsset(Failure, 0, text447, text128, pBufname->str);
				// "Invalid %s '%s'", "buffer name"
		sprintf(askStr, text447, text128, pBufname->str);
				// "Invalid %s '%s'", "buffer name"
		strcat(askStr, text324);
				// ".  New name"
		prompt = askStr;
		goto Ask;	// Try again.
		}

	// Check for duplicate name.
	pArray = &bufTable;
	while((pDatum = aeach(&pArray)) != NULL) {
		pBuf = bufPtr(pDatum);
		if(pBuf != pTargBuf) {

			// Other buffer with this name?
			if(strcmp(pBufname->str, pBuf->bufname) == 0) {
				if(sess.opFlags & OpScript)
					return rsset(Failure, 0, text181, text58, pBufname->str);
						// "%s name '%s' already in use", "Buffer"
				prompt = text25;
					// "That name is already in use.  New name"
				goto Ask;		// Try again.
				}

			// Command or function buffer name?
			if((*pTargBuf->bufname == BCmdFuncLead) != (*pBufname->str == BCmdFuncLead)) {
				if(sess.opFlags & OpScript)
					return rsset(Failure, 0, *pBufname->str == BCmdFuncLead ? text268 : text270, text275,
					 pBufname->str, BCmdFuncLead);
						// "Cannot %s buffer: name '%s' cannot begin with %c", "rename"
					// "Cannot %s command or function buffer: name '%s' must begin with %c", "rename"
				sprintf(askStr, "%s'%c'%s", text273, BCmdFuncLead, text324);
					// "Command and function buffer names (only) begin with ", ".  New name"
				prompt = askStr;
				goto Ask;		// Try again.
				}
			}
		}

	// Valid name?
	if(*pBufname->str == BCmdFuncLead) {
		char *str = pBufname->str + 1;
		Symbol sym = getIdent(&str, NULL);
		if((sym != s_ident && sym != s_identQuery) || *str != '\0') {
			if(sess.opFlags & OpScript)
				return rsset(Failure, 0, text447, text286, pBufname->str + 1);
					// "Invalid %s '%s'", "identifier"
			sprintf(askStr, text447, text286, pBufname->str + 1);
					// "Invalid %s '%s'", "identifier"
			strcat(askStr, text324);
					// ".  New name"
			prompt = askStr;
			goto Ask;	// Try again.
			}
		}
SetNew:
	// New name is unique and valid.  Rename the buffer.
	if((pTargBuf->flags & BFCmdFunc) &&
	 execFind(pTargBuf->bufname + 1, OpDelete, 0, NULL) != Success)	// Remove old command or function name from execTable...
		return sess.rtn.status;
	pDatum = delistBuf(pTargBuf);				// remove from buffer list...
	strcpy(pTargBuf->bufname, pBufname->str);		// copy new buffer name to structure...
	(void) bsrch(pTargBuf->bufname, &index);		// get index of new name...
	if(enlistBuf(pDatum, index) != Success)			// put it back in the right place...
		return sess.rtn.status;
	if(pTargBuf->flags & BFCmdFunc) {			// add new command or function name to execTable...
		UnivPtr univ = {
			.type = (pTargBuf->flags & BFCommand) ? PtrUserCmd : PtrUserFunc,
			.u.pBuf = pTargBuf};
		if(execFind(pTargBuf->bufname + 1, OpCreate, univ.type, &univ) != Success)
			return sess.rtn.status;
		}
	supd_windFlags(pTargBuf, WFMode);			// and update all affected mode lines.
	if(!(sess.opFlags & OpScript))
		(void) mlerase(0);
	if(pRtnVal != NULL)
		dxfer(pRtnVal, pBufname);

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Rename current buffer (if interactive and default n) or a named buffer.  Return status.
int renameBuf(Datum *pRtnVal, int n, Datum **args) {
	Buffer *pBuf;
	ushort flags = 0;

	// Get buffer.
	if(n == INT_MIN && !(sess.opFlags & OpScript)) {
		pBuf = sess.cur.pBuf;
		flags = BR_Current;
		}
	else if(getBufname(pRtnVal, text29, (pBuf = bdefault()) != NULL ? pBuf->bufname : NULL, ArgFirst | OpDelete, &pBuf,
				// "Rename"
	 NULL) != Success || pBuf == NULL)
		return sess.rtn.status;

	return brename(pRtnVal, flags, pBuf);
	}

// Get size of a buffer in lines and bytes.  Set *pLineCount to line count if not NULL and return byte count.
long bufLength(Buffer *pBuf, int *pLineCount) {
	long byteCt = 0;
	int lineCount = 0;
	Line *pLine = pBuf->pFirstLine;

	while(pLine->next != NULL) {			// Count all lines except the last...
		++lineCount;
		byteCt += pLine->used + 1;
		pLine = pLine->next;
		}
	byteCt += pLine->used;				// count bytes in last line...
	if(pLine->used > 0)				// and bump line count if last line not empty.
		++lineCount;
	if(pLineCount != NULL)
		*pLineCount = lineCount;
	return byteCt;
	}

// Add text (which may contain newlines) to the end of the given buffer and return status.  Note that (1), this works on
// foreground or background buffers; (2), the last line of the buffer is assumed to be empty (all lines are inserted before it);
// (3), the last null-terminated text segment has an implicit newline delimiter; and (4), the BFChanged buffer flag is not set.
// Function is used exclusively by "show" and "completion" routines where text is written to an empty buffer.
int bappend(Buffer *pBuf, const char *text) {
	Line *pLine;
	const char *str0, *str1;

	// Loop for each line segment found that ends with newline or null.
	str0 = str1 = text;
	for(;;) {
		if(*str1 == '\n' || *str1 == '\0') {

			// Allocate memory for the line and copy it over.
			if(lalloc(str1 - str0, &pLine) != Success)
				return sess.rtn.status;
			if(str1 > str0)
				memcpy(pLine->text, str0, str1 - str0);

			// Add the new line to the end of the buffer.
			llink(pLine, pBuf, pBuf->pFirstLine->prev);

			// Check and adjust source pointers.
			if(*str1 == '\0')
				break;
			str0 = str1 + 1;
			}
		++str1;
		}

	return sess.rtn.status;
	}

// Read the nth next line from a buffer and store in pRtnVal.  Return status.
int bgets(Datum *pRtnVal, int n, Datum **args) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rsset(Failure, 0, text39, text137, n, 0);
				// "%s (%d) must be %d or greater", "Repeat count"

	return bufOp(pRtnVal, n, NULL, BO_ReadBuf, 0);
	}
