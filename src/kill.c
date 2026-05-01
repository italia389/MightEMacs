// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// kill.c	Data and kill ring functions for MightEMacs.
//
// These routines manage the kill ring and undelete buffer.

#include "std.h"
#include "bind.h"
#include "exec.h"
#include "search.h"

static long lastYankSize = -1;		// Last number of bytes yanked (used by yankCycle to delete prior yank).

// Convert string or byte string in *pDatum to visible form and return result, or NULL if error.
static char *cvtstr(Datum *pDatum) {
	DFab entry;
	const char *str;
	size_t len;

	if(pDatum->type == dat_byteStr) {
		str = dgetmemptr(pDatum);
		len = dgetmemsize(pDatum);
		}
	else
		len = strlen(str = pDatum->str);

	return dopentrack(&entry) != 0 || esctofab(str, len, &entry) != 0 || dclose(&entry, FabStr) != 0 ? NULL :
	 entry.pDatum->str;
	}

// Add top ring entry to open fabrication object.  Return status code.
static int toptofab(Ring *pRing, DFab *pFab) {
	char *str;
	size_t len, maxNewLen = term.cols / 3;
	char workBuf[maxNewLen + 1];

	if(dtypstr(&pRing->pEntry->data) && strchr(pRing->pEntry->data.str, AttrSpecBegin) == NULL)
		str = pRing->pEntry->data.str;
	else if((str = cvtstr(&pRing->pEntry->data)) == NULL)
		goto LibFail;
	if((len = strlen(str)) > maxNewLen) {
		strfit(workBuf, maxNewLen + 1, str, len);
		str = workBuf;
		}
	if((pRing->flags & RingIDMask) == RingIdxKill) {
		int c = lowCase[(int) *pRing->ringName];
		if(dputf(pFab, 0, text529, c, pRing->ringName + 1, str) != 0)
				// ": ~btop %c%s now~B '%s'"
			goto LibFail;
		}
	else if(dputf(pFab, 0, text528, (pRing->flags & RingIDMask) == RingIdxDel ? text143 : pRing->entryName, str) != 0)
			// ":~b%s now~B '%s'",					" top entry"
LibFail:
		return libfail();

	return 0;
	}

// Cycle a data ring forward or backward abs(n) times.  Return status.
int rcycle(Ring *pRing, long n, bool msg) {

	if(pRing->size == 0)
		(void) rsset(Failure, 0, text202, pRing->ringName);
				// "%s ring is empty"
	else if(n != 0 && pRing->size > 1) {
		if(n == DefN)
			n = 1;
		if(n < 0)
			do {
				pRing->pEntry = pRing->pEntry->next;
				} while(++n < 0);
		else
			do {
				pRing->pEntry = pRing->pEntry->prev;
				} while(--n > 0);
		if(msg) {
			DFab msg;

			if(dopentrack(&msg) != 0 || dputf(&msg, 0, text42, pRing->ringName) != 0 ||
							// "%s ring cycled"
			 toptofab(pRing, &msg) != 0 || dclose(&msg, FabStr) != 0)
				return libfail();
			(void) rsset(Success, RSHigh | RSNoFormat | RSTermAttr, msg.pDatum->str);
			}
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
		(void) rnew(ringTable + (flag == SF_Kill ?	// cycle kill or delete ring and start fresh (don't append this
		 RingIdxKill : RingIdxDel));			// kill or delete to the last one).

	keyEntry.curFlags |= flag;				// Mark this command as a "kill" or "delete".
	return sess.rtn.status;
	}

#if MMDebug & Debug_RingDump
// Dump a ring to the log file.
void dumpRing(Ring *pRing) {
	ushort size = 0;

	fprintf(logfile, "  RING DUMP -- %s (%hu entries)\n", pRing->ringName, pRing->size);
	if(pRing->size > 0) {
		int i = 0;
		RingEntry *pEntry = pRing->pEntry;
		do {
			if(pEntry->data.type == dat_byteStr)
				fprintf(logfile, "%6d  %.*s\n", i--, pEntry->data.u.mem.size > 80 ? 80 :
				 (int) pEntry->data.u.mem.size, pEntry->data.u.mem.ptr);
			else
				fprintf(logfile, "%6d  %.80s\n", i--, pEntry->data.str);
			++size;
			} while((pEntry = pEntry->prev) != pRing->pEntry);
		}
	fprintf(logfile, "  %hu entries found.\n", size);
	}
#endif

// Find an entry in a ring (that is assumed to not be "Kill" or "Delete", which allow byte string entries).  If found, return
// slot pointer, otherwise NULL.
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
static void rtop(Ring *pRing, RingEntry *pEntry) {
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

// Save an entry to a ring (that is assumed to not be "Kill" or "Delete", which allow byte string entries) if value not null.
// If force is false and entry matches an existing entry, move old entry to the top (without cycling); otherwise, create an
// entry.  Return status.
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
				(void) libfail();
			}
		}
	return sess.rtn.status;
	}

// Get ring entry specified by e and return pointer to it.  If not found, set an error and return NULL.
RingEntry *rget(Ring *pRing, long e) {
	RingEntry *pEntry;

	// e in range?
	if(pRing->size == 0) {
		(void) rsset(Failure, 0, text202, pRing->ringName);
				// "%s ring is empty"
		return NULL;
		}
	if(e > 0 || e <= -pRing->size) {
		(void) rsset(Failure, 0, text19, e, -((int) pRing->size - 1));
				// "No such ring entry %'ld (max %'d)"
		return NULL;
		}

	// e is valid.  Find entry in ring...
	for(pEntry = pRing->pEntry; e < 0; ++e)
		pEntry = pEntry->prev;

	// and return it.
	return pEntry;
	}

// Attempt to delete "count" entries from given ring, beginning at top.  It is assumed that ring->pEntry != NULL and count > 0.
// Return number of entries actually deleted.
//
// Note that most recent ring entry (top) is pointed to by pRing->pEntry, next most recent one is pointed to by
// pRing->pEntry->prev (not "next"), top entry's next pointer points to bottom entry, and bottom entry's prev pointer points to
// top entry.
static long rtrim(Ring *pRing, int count) {
	RingEntry *pEntry;
	RingEntry *pBotEntry = pRing->pEntry->next;
	ushort origSize = pRing->size;

	do {
		pEntry = pRing->pEntry;
		pRing->pEntry = pEntry->prev;
		dclear(&pEntry->data);
		free((void *) pEntry);
		} while(--pRing->size > 0 && --count > 0);
	if(pRing->size == 0)
		pRing->pEntry = NULL;
	else {
		pRing->pEntry->next = pBotEntry;
		pBotEntry->prev = pRing->pEntry;
		}

	return origSize - pRing->size;
	}

// Delete the most recent e entries from given ring, entry number e if e < 0, or all entries if e == 0.  Return status.
int rdelete(Ring *pRing, long e) {
	Datum oldEntry;
	size_t maxOldLen;
	int ringSize;

	if(pRing->size == 0)
		return rsset(Failure, 0, text202, pRing->ringName);
				// "%s ring is empty"
	dinit(&oldEntry);
	maxOldLen = term.cols / 4;

	if(e == DefN)
		e = 1;
	else if(e == 0)
		e = pRing->size;
	else if(e < 0) {
		RingEntry *pEntry = rget(pRing, e);
		if(pEntry == NULL)
			return sess.rtn.status;

		// Delete "pEntry" item, which is not at top.  (Ring contains at least two entries.)
		pEntry->next->prev = pEntry->prev;
		pEntry->prev->next = pEntry->next;
		// Save pattern, kill, or delete for return message.
		dxfer(&oldEntry, &pEntry->data);
		free((void *) pEntry);
		--pRing->size;
		goto NukeMsg;
		}

	// Delete the most recent e entries (e > 0).
	ringSize = pRing->size;
	if(e == 1)
		dxfer(&oldEntry, &pRing->pEntry->data);
	if((e = rtrim(pRing, e)) == ringSize)
		(void) rsset(Success, 0, text228, pRing->ringName);
				// "%s ring cleared"
	else {
NukeMsg:;
		// Build and display post-operation status message (e < 0 or e > 0).
		DFab msg;
		char *str;
		size_t len;
		bool showOld = false;
		bool showNew = e > 0;

		// Show old entry if saved and length does not exceed maxOldLen.
		if(!disnil(&oldEntry)) {
			if(dtypstr(&oldEntry) && strchr(oldEntry.str, AttrSpecBegin) == NULL)
				str = oldEntry.str;
			else if((str = cvtstr(&oldEntry)) == NULL)
				goto LibFail;
			showOld = (len = strlen(str)) <= maxOldLen;
			}

		// Write "Search pattern" or "Kill".
		if(dopentrack(&msg) != 0 || (showOld && showNew && dputs("~b", &msg, 0) != 0) ||
		 dputs(pRing->ringName, &msg, 0) != 0 || dputs(pRing->entryName, &msg, 0) != 0 ||
		 (showOld && showNew && dputs("~B", &msg, 0) != 0))
			goto LibFail;

		// Add "s" or ring entry that was deleted, if available.
		if(disnil(&oldEntry)) {
			if(e > 1 && dputc('s', &msg, 0) != 0)
				goto LibFail;
			}
		else if(showOld && dputf(&msg, 0, " '%s'", str) != 0)
			goto LibFail;

		// Add " deleted" and new top pattern or kill if changed.
		if(dputs(text530, &msg, 0) != 0)
				// " deleted"
			goto LibFail;
		if(showNew && toptofab(pRing, &msg) != 0)
			goto LibFail;

		if(dclose(&msg, FabStr) != 0)
LibFail:
			return libfail();
		(void) rsset(Success, showNew ? RSHigh | RSNoFormat | RSTermAttr : RSHigh | RSNoFormat, msg.pDatum->str);
		}

	if(!disnil(&oldEntry))
		dclear(&oldEntry);

	return sess.rtn.status;
	}

// Find ring for given name and return pointer to it.  If not found, set an error and return NULL.
static Ring *findRing(Datum *pName) {
	Ring *pRing, *pRingEnd;

	// Search ring table.
	pRingEnd = (pRing = ringTable) + ringTableSize;
	do {
		if(strcasecmp(pName->str, pRing->ringName) == 0)
			return pRing;
		} while(++pRing < pRingEnd);

	// No such ring.
	(void) rsset(Failure, 0, text52, text157, pName->str);
			// "No such %s \"%s\"", "ring"
	return NULL;
	}

// Get a ring name and return ring pointer in *ppRing.  Return status.
int getRingName(Ring **ppRing) {
	Datum *pDatum;
	Ring *pRing;

	// Get ready.
	if((pDatum = dnewtrack()) == NULL)
		return libfail();

	if(!(sess.opFlags & OpScript)) {
		if(termInp(pDatum, text43, ArgFirst | ArgNotNull1, TermC_Ring, NULL) != Success || disnil(pDatum))
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

// Get or set ring size information and return status.  If interactive and n argument, or argument given in script mode, set
// maximum ring size to 'size' argument.  Return two-element array of form [current number of entries, maximum size].
int ringSize(Datum *pRtnVal, long n, Datum **args) {
	Array *pArray;
	Datum *pArrayEl;
	Ring *pRing;

	// Get ring pointer.
	if(getRingName(&pRing) != Success)
		goto Retn;

	// Set ring size, if requested.
	if(haveArg(n, NULL, true)) {
		long newSize;

		// Get size argument and check it.
		if(!(sess.opFlags & OpScript)) {
			char workBuf[WorkBufSize];

			sprintf(workBuf, text126, pRing->maxSize);
				// "Current size: %hu, new size"
			if(termInp(pRtnVal, workBuf, ArgInt1, 0, NULL) != Success || disnil(pRtnVal) ||
			 ascToLong(pRtnVal->str, 10, &newSize, false) != Success)
				goto Retn;
			}
		else {
			if(funcArg(pRtnVal, ArgInt1) != Success)
				goto Retn;
			newSize = dgetint(pRtnVal);
			}
		if(newSize < 0 || newSize > MaxRingSize)
			return rsset(Failure, 0, text12, text155, newSize, 0, (long) MaxRingSize);
				// "%s (%'ld) must be in range %d - %'ld", "Ring size"
		if(newSize < pRing->size)
			return rsset(Failure, 0, text241, newSize, (int) pRing->size);
				// "Maximum ring size (%'ld) less than current size (%'d)"
		pRing->maxSize = newSize;
		}

	// Success.  Set return value and message.
	if((pArray = anew(2, NULL)) == NULL)
		goto LibFail;
	astash(pRtnVal, pArray);
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
	return libfail();
	}

// Set pattern at top of "Search" or "Replace" ring.  If search pattern, make a copy so that a search pattern on the ring is not
// modified by checkOpts() if newSearchPat() is called.
static int setTopEntry(Ring *pRing) {
	char *entry = (pRing->size == 0) ? "" : pRing->pEntry->data.str;
	if((pRing->flags & RingIDMask) == RingIdxSearch) {
		char workBuf[strlen(entry) + 1];

		// Set search pattern.
		return newSearchPat(strcpy(workBuf, entry), &bufSearch.match, NULL, false);
		}

	// Set replacement pattern.
	return newReplPat(entry, &bufSearch.match, false);
	}

// Cycle a ring per n argument and return status.
int cycleRing(Datum *pRtnVal, long n, Datum **args) {
	Ring *pRing;

	if(getRingName(&pRing) == Success) {

		// Cycle ring forward or backward.  If search or replace ring, also set entry at top when done.
		if(n != 0 && pRing->size > 1 && rcycle(pRing, n, true) == Success)
			if(!(pRing->flags & RingKD))
				(void) setTopEntry(pRing);
		}
	return sess.rtn.status;
	}

// Delete ring entri(es) and, if script mode, return number deleted, or zero if error.  Return status.
int delRingEntry(Datum *pRtnVal, long n, Datum **args) {
	Ring *pRing;
	int count = n == DefN ? 1 : n;		// May be garbage due to integer overflow.

	if(getRingName(&pRing) == Success) {
		if(sess.opFlags & OpScript)
			count = count == 0 || count >= pRing->size ? pRing->size :
			 count <= -pRing->size ? 0 : count < 0 ? 1 : count;
		switch(pRing->flags & RingIDMask) {
			case RingIdxDel:
			case RingIdxKill:
				(void) rdelete(pRing, n);
				break;
			default:	// RingIdxRepl or RingIdxSearch
				{RingEntry *pTopEntry = pRing->pEntry;		// May be NULL.
				if(rdelete(pRing, n) == Success && pRing->pEntry != pTopEntry)
					(void) setTopEntry(pRing);
				}
			}

		// Ignore any non-fatal error in script mode.
		if(sess.opFlags & OpScript && sess.rtn.status >= Failure) {
			if(sess.rtn.status != Success) {
				rsunfail();
				count = 0;
				}
			dsetint(count, pRtnVal);
			}
		}

	return sess.rtn.status;
	}

// Insert, overwrite, or replace a string from a string buffer (src != NULL) or a "Kill" or "Delete" ring entry (src == NULL) at
// point n times (default 1) and return status.  In the latter case, use given ring and mark text as a region.  If n == 0, do
// one repetition and leave point before the new text, otherwise after.  If n < 0 and src != NULL, process newline characters
// literally.  If n < 0 and src == NULL, get the -nth ring entry instead of the most recent one without cycling the ring.
// Update kill state if src is NULL.
int iorStr(const char *src, long n, uint insertType, EScreen *pScrn, Ring *pRing) {
	long size0, size;		// Size of (byte) string to insert.
	int curOffset;
	const char *str0, *str;		// Pointer to string to insert.
	Line *pCurLine;
	RingEntry *pEntry;
	bool preText;			// Leave point before inserted text?
	int hardTabSize = pScrn != NULL ? pScrn->hardTabSize : 8;
	Point *pPoint = &sess.edit.pFace->point;
	short c;

	// Check if we have something to insert.  Not an error if no text.
	if(n == DefN)
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

		if(disnull(&pEntry->data) || (pEntry->data.type == dat_byteStr && pEntry->data.u.mem.size == 0)) {
			lastYankSize = 0;
			return sess.rtn.status;
			}

		// Set region mark.  Can assume we're editing current buffer (and current window) here because we got the
		// text from a ring, which implies a yank or undelete command.
		setWindMark(&sess.cur.pBuf->markHdr, sess.pCurWind);
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

	// For n times...
	if(n < 0) {
		n = 1;
		insertType |= Txt_LiteralNL;
		}
	if(src != NULL)
		size0 = strlen(str0 = src);
	else if(pEntry->data.type == dat_byteStr) {
		str0 = pEntry->data.u.mem.ptr;
		size0 = pEntry->data.u.mem.size;
		}
	else
		size0 = strlen(str0 = pEntry->data.str);
	while(n-- > 0) {
		str = str0;
		size = size0;
		do {
			// Have a newline?
			if((c = *str++) == '\n' && !(insertType & Txt_LiteralNL)) {
				if(einsertNL() != Success)
					return sess.rtn.status;
				}
			else {
				// Not a newline or processing literally.  Overwrite or replace and not at end of line?
				if(!(insertType & Txt_Insert) && pPoint->offset < pPoint->pLine->used) {

					// Yes.  Replace mode, not a tab, or at a tab stop?
					if(insertType & Txt_Replace || pPoint->pLine->text[pPoint->offset] != '\t' ||
					 getCol(NULL, false) % hardTabSize == hardTabSize - 1) {

						// Yes.  Delete before insert.
						if(edelc(1, 0) != Success)
							return sess.rtn.status;
						}
					}

				// Insert the character!  Amen.
				if(einsertc(1, c) != Success)
					return sess.rtn.status;
				}
			} while(--size > 0);
		}

	// If requested, set point back to the beginning of the new text and move region mark.
	if(preText) {
		if(src == NULL)
			setWindMark(&sess.cur.pBuf->markHdr, sess.pCurWind);
		pPoint->pLine = (pCurLine == NULL) ? sess.edit.pBuf->pFirstLine : pCurLine->next;
		pPoint->offset = curOffset;
		}

	// Update kill state.
	if(src == NULL) {
		curOffset = (pRing->flags & RingIDMask) == RingIdxKill ? SF_Yank : SF_Undel;
		keyEntry.curFlags |= (preText ? curOffset | SF_NoMove : curOffset);
		lastYankSize = size0;
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
int ycycle(Datum *pRtnVal, long n, Ring *pRing, ushort flag, uint cmd) {

	// If not the first consecutive time...
	if(keyEntry.prevFlags & flag && n != 0) {
		if(yrevert(flag) == Success &&			// delete the last yank or undelete...
		 rcycle(pRing, n, false) == Success)		// cycle the ring per n...
			(void) execCmdFunc(pRtnVal, 1, cmdFuncTable + cmd, 0, 0);	// and insert the current ring entry.
		}
	return sess.rtn.status;
	}
