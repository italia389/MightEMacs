// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// kill.c	Data and kill ring functions for MightEMacs.
//
// These routines manage the kill ring and undelete buffer.

#include "std.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "search.h"

static long lastYankSize = -1;	// Last number of bytes yanked (used by yankCycle to delete prior yank).

// Cycle a data ring forward or backward abs(n) times.  Return status.
int rcycle(Ring *pRing, int n, bool msg) {

	if(pRing->size == 0)
		(void) rsset(Failure, 0, text202, pRing->ringName);
				// "%s ring is empty"
	else if(n != 0 && pRing->size > 1) {
		if(n == INT_MIN)
			n = 1;
		if(n < 0)
			do {
				pRing->pEntry = pRing->pEntry->next;
				} while(++n < 0);
		else
			do {
				pRing->pEntry = pRing->pEntry->prev;
				} while(--n > 0);
		if(msg)
			(void) rsset(Success, 0, text42, pRing->ringName);
				// "%s ring cycled"
		}
	return sess.rtn.status;
	}

// Create a new RingEntry object and insert it into the given ring at the current position if room; otherwise, clear the oldest
// entry and cycle the ring so it's current.  Return status.
static int rnew(Ring *pRing) {

	if(pRing->maxSize == 0 || pRing->size < pRing->maxSize) {
		RingEntry *pEntry = (RingEntry *) malloc(sizeof(RingEntry));
		if(pEntry == NULL)
			return rsset(Panic, 0, text94, "rnew");
					// "%s(): Out of memory!"
		dinit(&pEntry->data);
		if(pRing->pEntry == NULL)
			pRing->pEntry = pEntry->next = pEntry->prev = pEntry;
		else {
			pEntry->next = pRing->pEntry->next;
			pEntry->prev = pRing->pEntry;
			pRing->pEntry = pRing->pEntry->next = pRing->pEntry->next->prev = pEntry;
			}
		++pRing->size;
		}
	else {
		(void) rcycle(pRing, -1, false);		// Can't fail.
		dclear(&pRing->pEntry->data);
		}

	return sess.rtn.status;
	}

// Prepare for new kill or delete.  Return status.
int killPrep(int kill) {
	ushort flag = kill ? SF_Kill : SF_Del;

	if(!(keyEntry.prevFlags & flag))			// If last command was not in "kill" or "delete" family...
		(void) rnew(flag == SF_Kill ?			// cycle kill or delete ring and start fresh (don't append this
		 ringTable + RingIdxKill : ringTable + RingIdxDel);	// kill or delete to the last one).

	keyEntry.curFlags |= flag;				// Mark this command as a "kill" or "delete".
	return sess.rtn.status;
	}

#if MMDebug & Debug_RingDump
// Dump a ring to the log file.
void dumpRing(Ring *pRing) {
	ushort size = 0;

	fprintf(logfile, "  RING DUMP -- %s (%hu entries)\n", pRing->ringName, pRing->size);
	if(pRing->size > 0) {
		int n = 0;
		RingEntry *pEntry = pRing->pEntry;
		do {
			fprintf(logfile, "%6d  %.80s\n", n--, pEntry->data.str);
			++size;
			} while((pEntry = pEntry->prev) != pRing->pEntry);
		}
	fprintf(logfile, "  %hu entries found.\n", size);
	}
#endif

// Find an entry in a ring.  If found, return slot pointer, otherwise NULL.
static RingEntry *rfind(Ring *pRing, const char *value) {

	if(pRing->size > 0) {
		RingEntry *pEntry = pRing->pEntry;

		do {
			if(strcmp(pEntry->data.str, value) == 0)
				return pEntry;
			} while((pEntry = pEntry->prev) != pRing->pEntry);
		}

	// Entry not found.
	return NULL;
	}

// Move entry to the top of the ring if not already there.
void rtop(Ring *pRing, RingEntry *pEntry) {
	RingEntry *pEntry0 = pRing->pEntry;
	if(pEntry != pEntry0) {

		// Ring contains at least two entries.  If only two, just change the header pointer;
		// otherwise, remove matched entry from its current slot and insert it at the top.
		pRing->pEntry = pEntry;
		if(pRing->size > 2) {
			pEntry->next->prev = pEntry->prev;
			pEntry->prev->next = pEntry->next;

			pEntry->next = pEntry0->next;
			pEntry->prev = pEntry0;
			pEntry0->next = pEntry0->next->prev = pEntry;
			}
		}
	}

// Save an entry to a ring if entry not null.  If force is false and entry matches an existing entry, move old entry to the top
// (without cycling); otherwise, create an entry.  Return status.
int rsave(Ring *pRing, const char *value, bool force) {

	if(*value != '\0') {
		if(force)
			goto AddIt;

		RingEntry *pEntry = rfind(pRing, value);
		if(pEntry != NULL)
			rtop(pRing, pEntry);
		else {
AddIt:
			// Entry not found or a force... add it.
			if(rnew(pRing) == Success && dsetstr(value, &pRing->pEntry->data) != 0)
				(void) librsset(Failure);
			}
		}
	return sess.rtn.status;
	}

// Get ring entry specified by n and return pointer to it.  If not found, set an error and return NULL.
RingEntry *rget(Ring *pRing, int n) {
	RingEntry *pEntry;

	// n in range?
	if(pRing->size == 0) {
		(void) rsset(Failure, 0, text202, pRing->ringName);
				// "%s ring is empty"
		return NULL;
		}
	if(n > 0 || n <= -pRing->size) {
		(void) rsset(Failure, 0, text19, n, -((int) pRing->size - 1));
				// "No such ring entry %d (max %d)"
		return NULL;
		}

	// n is valid.  Find entry #n in ring...
	for(pEntry = pRing->pEntry; n < 0; ++n)
		pEntry = pEntry->prev;

	// and return it.
	return pEntry;
	}

// Attempt to delete n entries from given ring, beginning at top.  It is assumed that ring->pEntry != NULL and n > 0.  Return
// number of entries actually deleted.
static ushort rtrim(Ring *pRing, int n) {
	RingEntry *pEntry;
	RingEntry *pBotEntry = pRing->pEntry->next;
	ushort origSize = pRing->size;

	do {
		pEntry = pRing->pEntry;
		pRing->pEntry = pEntry->prev;
		dclear(&pEntry->data);
		free((void *) pEntry);
		} while(--pRing->size > 0 && --n > 0);
	if(pRing->size == 0)
		pRing->pEntry = NULL;
	else {
		pRing->pEntry->next = pBotEntry;
		pBotEntry->prev = pRing->pEntry;
		}

	return origSize - pRing->size;
	}

// Delete the most recent n entries (default 1) from given ring, kill number n if n < 0, or all entries if n == 0.
// Return status.
int rdelete(Ring *pRing, int n) {

	if(pRing->size == 0)
		return rsset(Failure, 0, text202, pRing->ringName);
				// "%s ring is empty"
	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		n = pRing->size;
	else if(n < 0) {
		RingEntry *pEntry = rget(pRing, n);
		if(pEntry == NULL)
			return sess.rtn.status;

		// Delete "entry" item, which is not at top.  (Ring contains at least two entries.)
		pEntry->next->prev = pEntry->prev;
		pEntry->prev->next = pEntry->next;
		dclear(&pEntry->data);
		free((void *) pEntry);
		--pRing->size;
		n = 1;
		goto NukeMsg;
		}

	// Delete the most recent n entries.
	int sz0 = pRing->size;
	if((n = rtrim(pRing, n)) == sz0)
		(void) rsset(Success, 0, text228, pRing->ringName);
				// "%s ring cleared"
	else
NukeMsg:
		(void) rsset(Success, RSHigh, text357, pRing->ringName, pRing->entryName, n > 1 ? "s" : "");
				// "%s%s%s deleted"

	return sess.rtn.status;
	}

// Find ring for given name and return pointer to it.  If not found, set an error and return NULL.
Ring *findRing(Datum *pName) {
	Ring *pRing, *pRingEnd;

	// Search ring table.
	pRingEnd = (pRing = ringTable) + ringTableSize;
	do {
		if(strcasecmp(pName->str, pRing->ringName) == 0)
			return pRing;
		} while(++pRing < pRingEnd);

	// No such ring.
	(void) rsset(Failure, 0, text395, text157, pName->str);
			// "No such %s '%s'", "ring"
	return NULL;
	}

// Get a ring name and return ring pointer in *ppRing.  Return status.
int getRingName(Ring **ppRing) {
	Datum *pDatum;
	Ring *pRing;

	// Get ready.
	if(dnewtrack(&pDatum) != 0)
		return librsset(Failure);

	if(!(sess.opFlags & OpScript)) {
		if(termInp(pDatum, text43, ArgFirst | ArgNotNull1, Term_C_Ring, NULL) != Success || disnil(pDatum))
				// "Ring name"
			return sess.rtn.status;
		}
	else if(funcArg(pDatum, ArgFirst | ArgNotNull1) != Success)
		return sess.rtn.status;

	// Find ring.
	if((pRing = findRing(pDatum)) != NULL)
		*ppRing = pRing;
	return sess.rtn.status;
	}

// Get or set ring size information and return status.  If n argument, set maximum ring size to 'size' argument.  Return
// two-element array of form [current number of entries, maximum size].
int ringSize(Datum *pRtnVal, int n, Datum **args) {
	Array *pArray;
	Datum *pArrayEl;
	Ring *pRing;

	// Get ring pointer.
	if(getRingName(&pRing) != Success)
		goto Retn;

	// Set ring size, if requested.
	if(n != INT_MIN) {
		long longVal;

		// Get size argument and check it.
		if(!(sess.opFlags & OpScript)) {
			char workBuf[WorkBufSize];

			sprintf(workBuf, text126, pRing->maxSize);
				// "Current size: %hu, new size"
			if(termInp(pRtnVal, workBuf, ArgInt1, 0, NULL) != Success || disnil(pRtnVal) ||
			 ascToLong(pRtnVal->str, &longVal, false) != Success)
				goto Retn;
			}
		else {
			if(funcArg(pRtnVal, ArgInt1) != Success)
				goto Retn;
			longVal = pRtnVal->u.intNum;
			}
		if(longVal < 0 || longVal > USHRT_MAX)
			return rsset(Failure, 0, text12, text155, (int) longVal, 0, USHRT_MAX);
				// "%s (%d) must be between %d and %d", "Ring size"
		if(longVal < pRing->size)
			return rsset(Failure, 0, text241, longVal, (int) pRing->size);
				// "Maximum ring size (%ld) less than current size (%d)"
		pRing->maxSize = longVal;
		}

	// Success.  Set return value and message.
	if((pArray = anew(2, NULL)) == NULL)
		goto LibFail;
	agStash(pRtnVal, pArray);
	if((pArrayEl = aget(pArray, 0, 0)) == NULL)
		goto LibFail;
	dsetint((long) pRing->size, pArrayEl);
	if((pArrayEl = aget(pArray, 1, 0)) == NULL)
		goto LibFail;
	dsetint((long) pRing->maxSize, pArrayEl);
	(void) rsset(Success, RSNoWrap | RSTermAttr, text114, pRing->ringName, pRing->size, pRing->maxSize);
			// "~bRing~B: %s, ~bentries~B: %hu, ~bsize~B: %hu"
Retn:
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Set pattern or macro at top of search, replace, or macro ring.  If search pattern, make a copy so that a search
// pattern on the ring is not modified by checkOpts() if newSearchPat() is called.
static int setTopEntry(Ring *pRing) {
	char *entry = (pRing->size == 0) ? "" : pRing->pEntry->data.str;
	if(pRing == ringTable + RingIdxSearch) {
		char workBuf[strlen(entry) + 1];

		// Set search pattern.
		return newSearchPat(strcpy(workBuf, entry), &searchCtrl.match, NULL, false);
		}
	if(pRing == ringTable + RingIdxRepl)

		// Set replacement pattern.
		return newReplPat(entry, &searchCtrl.match, false);

	// Set macro.
	return decodeMacro(entry);
	}

// Delete entri(es) from search or replace ring and update or delete current pattern.
static int delEntry(Ring *pRing, int n) {

	if(rdelete(pRing, n) == Success)
		(void) setTopEntry(pRing);

	return sess.rtn.status;
	}

// Cycle a ring per n argument and return status.
int cycleRing(Datum *pRtnVal, int n, Datum **args) {
	Ring *pRing;

	if(getRingName(&pRing) == Success) {
		ptrdiff_t ringId = pRing - ringTable;
		if(ringId == RingIdxMacro) {
			if(macBusy(true, text338, text473, text466, text466))
					// "Cannot %s a %s from a %s, cancelled", "operate on", "macro", "macro"
				goto Retn;
			}

		// Cycle ring forward or backward.  If search, replace, or macro ring, also set entry at top when done.
		if(n != 0 && pRing->size > 1 && rcycle(pRing, n, true) == Success)
			if(ringId != RingIdxDel && ringId != RingIdxKill)
				(void) setTopEntry(pRing);
		}
Retn:
	return sess.rtn.status;
	}

// Delete ring entri(es) and return status.
int delRingEntry(Datum *pRtnVal, int n, Datum **args) {
	Ring *pRing;

	if(getRingName(&pRing) == Success) {
		switch(pRing - ringTable) {
			case RingIdxDel:
			case RingIdxKill:
				(void) rdelete(pRing, n);
				break;
			case RingIdxMacro:
				(void) delMacro(pRtnVal, pRing, n);
				break;
			default:	// RingIdxRepl or RingIdxSearch
				(void) delEntry(pRing, n);
			}
		}
	return sess.rtn.status;
	}

// Create array of ring names and save in *pRtnVal.  Return status.
int ringNames(Datum *pRtnVal) {
	Array *pArray;

	if(makeArray(pRtnVal, ringTableSize, &pArray) == Success) {
		Ring *pRing, *pRingEnd;
		Datum **ppArrayEl = pArray->elements;
		pRingEnd = (pRing = ringTable) + ringTableSize;
		do {
			if(dsetstr(pRing->ringName, *ppArrayEl++) != 0)
				return librsset(Failure);
			} while(++pRing < pRingEnd);
		}
	return sess.rtn.status;
	}

// Insert, overwrite, or replace a character at point.  Return status.
static int iorChar(short c, ushort t) {

	// Newline?
	if(c == '\n' && !(t & Txt_LiteralNL))
		return einsertNL();

	// Not a newline or processing literally.  Overwrite or replace and not at end of line?
	Point *pPoint = &sess.edit.pFace->point;
	if(!(t & Txt_Insert) && pPoint->offset < pPoint->pLine->used) {
		int hardTabSize = sess.edit.pScrn != NULL ? sess.edit.pScrn->hardTabSize : 8;

		// Yes.  Replace mode, not a tab, or at a tab stop?
		if((t & Txt_Replace) || pPoint->pLine->text[pPoint->offset] != '\t' ||
		 getCol(NULL, false) % hardTabSize == hardTabSize - 1) {

			// Yes.  Delete before insert.
			if(edelc(1, 0) != Success)
				return sess.rtn.status;
			}
		}

	// Insert the character! (amen).
	return einsertc(1, c);
	}

// Insert, overwrite, or replace a string from a string buffer (src != NULL) or a kill or delete ring entry (src == NULL) at
// point n times (default 1) and return status.  In the latter case, use given ring and mark text as a region.  If n == 0, do
// one repetition and leave point before the new text, otherwise after.  If n < 0 and src != NULL, process newline characters
// literally.  If n < 0 and src == NULL, get the -nth ring entry instead of the most recent one without cycling the ring.
// Update kill state if src is NULL.
int iorStr(const char *src, int n, ushort style, Ring *pRing) {
	long size;
	int reps;
	const char *str;	// Pointer into string to insert.
	Line *pCurLine;
	int curOffset;
	RingEntry *pEntry;
	bool preText;		// Leave point before inserted text?
	Point *pPoint = &sess.edit.pFace->point;

	// Check if we have something to insert.  Not an error if no text.
	if(n == INT_MIN)
		n = 1;
	if(src == NULL) {

		// Which kill?
		if(pRing->size == 0)
			return rsset(Failure, 0, text202, pRing->ringName);
					// "%s ring is empty"
		else if(n >= 0)
			pEntry = pRing->pEntry;
		else {
			if((pEntry = rget(pRing, n)) == NULL)
				return sess.rtn.status;
			n = 1;
			}

		if(disnull(&pEntry->data)) {
			lastYankSize = 0;
			return sess.rtn.status;
			}

		// Set region mark.  Can assume we're editing current buffer (and current window) here because we got the
		// text from a ring, which implies a yank or undelete command.
		setWindMark(&sess.cur.pBuf->markHdr, sess.cur.pWind);
		}
	else if(*src == '\0')
		return sess.rtn.status;

	// If n == 0, remember point.  However, must save the *previous* line (if possible), since the line we are on may
	// disappear due to re-allocation.
	if((preText = n == 0)) {
		pCurLine = (pPoint->pLine == sess.edit.pBuf->pFirstLine) ? NULL : pPoint->pLine->prev;
		curOffset = pPoint->offset;
		n = 1;
		}

	// For "reps" times...
	if(n < 0) {
		reps = 1;
		style |= Txt_LiteralNL;
		}
	else
		reps = n;
	while(reps-- > 0) {
		size = 0;
		str = (src == NULL) ? pEntry->data.str : src;
		do {
			if(iorChar(*str++, style) != Success)
				return sess.rtn.status;
			++size;
			} while(*str != '\0');
		}

	// If requested, set point back to the beginning of the new text and move region mark.
	if(preText) {
		if(src == NULL)
			setWindMark(&sess.cur.pBuf->markHdr, sess.cur.pWind);
		pPoint->pLine = (pCurLine == NULL) ? sess.edit.pBuf->pFirstLine : pCurLine->next;
		pPoint->offset = curOffset;
		}

	// Update kill state.
	if(src == NULL) {
		reps = (pRing == ringTable + RingIdxKill) ? SF_Yank : SF_Undel;
		keyEntry.curFlags |= (preText ? (reps | SF_NoMove) : reps);
		lastYankSize = size;
		}

	return sess.rtn.status;
	}

// Revert yanked or undeleted text; that is, undo the insertion.  Nothing is done unless prior command was a yank or undelete
// per given flags.  Return status.
int yrevert(ushort flags) {

	if(keyEntry.prevFlags & flags) {
		(void) edelc(keyEntry.prevFlags & SF_NoMove ? lastYankSize : -lastYankSize, 0);
		lastYankSize = 0;
		}
	return sess.rtn.status;
	}

// Yank or undelete text, replacing the text from the last invocation if this not the first.
int ycycle(Datum *pRtnVal, int n, Ring *pRing, ushort flag, ushort cmd) {

	// If not the first consecutive time...
	if((keyEntry.prevFlags & flag) && n != 0) {
		if(yrevert(flag) == Success &&			// delete the last yank or undelete...
		 rcycle(pRing, n, false) == Success)		// cycle the ring per n...
			(void) execCmdFunc(pRtnVal, 1, cmdFuncTable + cmd, 0, 0);	// and insert the current ring entry.
		}
	return sess.rtn.status;
	}
