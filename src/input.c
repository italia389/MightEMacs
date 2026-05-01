// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// input.c	Various user input routines for MightEMacs.
//
// MightEMacs's kernel processes two distinct forms of characters.  One of these is a standard unsigned character which is
// used in the edit buffer.  The other form, called an Extended Key is a two-byte value which contains both an ASCII value
// and flags for certain prefixes/events:
//
//	Bit	Usage
//	---	-----
//	0-7	Standard 8-bit ASCII character
//	8-15	Flags for control, meta, prefix 1-3, shift, and function keys.
//
// The machine-dependent driver is responsible for returning a byte stream from the keyboard via ncurses calls.  These values
// are then interpreted by getkey() to construct the proper extended key sequences to pass to the MightEMacs kernel.

#include "std.h"
#include <sys/types.h>
#include <pwd.h>
#include "cxl/lib.h"
#include "bind.h"
#include "exec.h"
#include "search.h"
#include "var.h"

// Local definitions.

// Line editing commands.
typedef enum {
	ed_ins, ed_bs, ed_del, ed_left, ed_right, ed_bol, ed_eol, ed_erase, ed_kill
	} LineEdit;

// One input character.
typedef struct {
	char c;				// Raw character.
	char len;			// Length of visible form of character.
	} InpChar;

// Terminal input state information, initialized by mli_open().
typedef struct {
	const char *prompt;		// Prompt string.
	InpChar *inpBuf;		// Pointer to caller's input buffer (of type 'InpChar [MaxTermInp]').
	InpChar *curInpPos;		// Pointer to current position in inpBuf.
	InpChar *endInpPos;		// Pointer to end-of-input position in inpBuf.
	InpChar *endBufPos;		// Pointer to end-of-buffer position in inpBuf.
	int leftShift;			// Current left-shift of inpBuf on message line.
	int partialLen;			// Length of partial character displayed at beginning of input area.
	uint maxLen;			// Maximum length of user input.
	int beginInpCol;		// Column in message line where user input begins (just past prompt).
	int inpSize;			// Size of input area (from beginInpCol to right edge of screen).
	int inpSize75;			// 75% of input area size.
	int jumpCols;			// Number of columns to shift input line when cursor moves out of input area.
	uint flags;			// Term_Attr and Term_NoKeyEcho flags.
	} InpCtrl;

// Completion state information, initialized by cMatch() and cList().
typedef struct {
	int (*nextItem)(bool reset, void *context,		// "Get next item" routine.  Returns Success, NotFound (if no
	 const char **pItem);					// items left), or error status.
	bool (*cmp)(InpCtrl *pInpCtrl, const char *item);	// String comparison routine.
	void *context;						// Caller's context.
	} Completion;

// Completion object for a buffer name.
typedef struct {
	int force;
	InpCtrl *pInpCtrl;
	HashNode **pAry;		// Buffer table array (created from hash).
	} CompBuf;

// Completion object for a command, function, or alias name.
typedef struct {
	uint selector;			// Selector flags.
	HashNode **ppHashNode;		// Sorted Hash records.
	HashNode **ppHashNodeEnd;
	HashNode **ppHashNode1;
	} CompCFA;

// Completion object for a filename.
typedef struct {
	InpCtrl *pInpCtrl;
	char *filename;
	} CompFile;

// Completion object for a macro name.
typedef struct {
	HashNode **ppHashNode;		// Sorted Hash records.
	HashNode **ppHashNodeEnd;
	HashNode **ppHashNode1;
	} CompMacro;

// Completion object for a ring name.
typedef struct {
	Ring *pRing;			// Current ring.
	Ring *pRingEnd;			// End of ring table.
	} CompRing;

// Completion object for a variable.
typedef struct {
	const char **cvList;		// Variable array.
	const char **pVar;		// Current variable name.
	const char **pVarEnd;		// End of variable array.
	} CompVar;

// "Unget" a key from getkey() for reprocessing in getkey() and editLoop().
void ungetkey(ExtKey extKey) {

	keyEntry.charPending = extKey;
	keyEntry.isPending = true;
	}

// Get one keystroke from the terminal driver, resolve any macro action, and return it in *pExtKey as an extended key.  The
// legal prefixes here are the FKey and Ctrl.  If saveKey is true, store extended key in keyEntry.lastKeySeq.  Return status.
int getkey(bool msgLine, ExtKey *pExtKey, bool saveKey) {
	ExtKey extKey;

	// If a character is pending...
	if(keyEntry.isPending) {

		// get it and return it.
		extKey = keyEntry.charPending;
		keyEntry.isPending = false;
		goto Retn;
		}

	// Otherwise, if we are playing a macro back...
	if(macro.state == MacPlaying) {
		Macro *pActive = macro.pActiveMacro;

		// End of macro?
		if(pActive->pMacSlot == pActive->pMacEnd) {

			// Yes, check if last iteration.
			if(--macro.n <= 0) {

				// Non-positive counter.  Error if infinite reps and hit loop maximum.
				if(macro.n < 0 && maxLoop > 0 && labs(macro.n) > maxLoop)
					return rsset(Failure, 0, text112, maxLoop);
						// "Maximum number of loop iterations (%'d) exceeded!"
				if(macro.n == 0) {

					// Finished last repetition.  Stop playing and update screen.
					macro.state = MacStopped;
					if(update(DefN) != Success)
						return sess.rtn.status;
					goto Fetch;
					}
				}

			// Not last rep.  Reset the macro to the begining for the next one.
			pActive->pMacSlot = pActive->pMacBuf;
			}

		// Return the next key.
		extKey = *pActive->pMacSlot++;
		goto Retn;
		}
Fetch:
	// Otherwise, fetch a character from the terminal driver.
	if(tgetc(msgLine, &extKey) != Success)
		return sess.rtn.status;

	// Save it if we need to...
	if(macro.state == MacRecording) {
		Macro *pStage = macro.pStageMacro;

		// Don't overrun the buffer.
		if(pStage->pMacSlot == pStage->pMacBuf + pStage->size)
			if(growMacro(pStage) != Success)
				return sess.rtn.status;
		*pStage->pMacSlot++ = extKey;
		}
Retn:
	// and finally, return the extended key.
	*pExtKey = extKey;
	if(saveKey)
		keyEntry.lastKeySeq = extKey;
	return sess.rtn.status;
	}

// Get a key sequence from the keyboard and return it in *pExtKey.  Process all applicable prefix keys.  If ppKeyBind is not
// NULL, set *ppKeyBind to the getBind() return pointer.  If saveKey is true, store extended key in keyEntry.lastKeySeq.  Return
// status.
int getKeySeq(bool msgLine, ExtKey *pExtKey, KeyBind **ppKeyBind, bool saveKey) {
	ExtKey extKey;		// Fetched keystroke.
	ExtKey prefix;		// Prefix mask.
	KeyBind *pKeyBind;	// Pointer to binding entry.

	// Get initial key.
	if(getkey(msgLine, &extKey, false) != Success)
		return sess.rtn.status;
	pKeyBind = getBind(extKey);

	// Get another key if first one was a prefix.
	if(pKeyBind != NULL && pKeyBind->cmd.type == PtrPseudo) {
		switch((CmdFuncId) (pKeyBind->cmd.u.pCmdFunc - cmdFuncTable)) {
			case cf_metaPrefix:
				prefix = Meta;
				goto Next;
			case cf_prefix1:
				prefix = Pref1;
				goto Next;
			case cf_prefix2:
				prefix = Pref2;
				goto Next;
			case cf_prefix3:
				prefix = Pref3;
Next:
				if(getkey(msgLine, &extKey, false) != Success)
					return sess.rtn.status;
				extKey |= prefix;			// Add prefix.
				if(ppKeyBind != NULL) {
					pKeyBind = getBind(extKey);	// Get new binding if requested.
					goto Retn;
					}
				break;
			default:		// Do nothing.
				break;
			}
		}

	if(ppKeyBind != NULL) {
Retn:
		*ppKeyBind = pKeyBind;
		}

	// Return key sequence.
	*pExtKey = extKey;
	if(saveKey)
		keyEntry.lastKeySeq = extKey;
	return sess.rtn.status;
	}

// Do "getKey" function.  Return status.
int fGetKey(Datum *pRtnVal, long n, Datum **args) {
	ExtKey extKey;
	char keyBuf[16];
	uint flags = cmdFuncTable[cf_getKey].pOptHdr->selFlags;

	// Get key or key sequence and return it.
	if((flags & GK_KeySeq ? getKeySeq(true, &extKey, NULL, true) : getkey(true, &extKey, true)) != Success)
		return sess.rtn.status;
	if(flags & GK_DoAbort && extKey == coreKeys[CK_Abort].extKey)
		return abortInp();
	if(!(flags & GK_KeyLit))
		dsetint(ektoc(extKey, true), pRtnVal);
	else if(dsetstr(ektos(keyBuf, extKey, false), pRtnVal) != 0)
		return libfail();

	return sess.rtn.status;
	}

// Shift rules:		$SC> a^T xyz ^R end<ES$
//			    ^		   ^
// Far right: term.cols - 2 (or before partial char); far left: beginInpCol + 1 (or past partial char).
//
// OLD CURSOR		EVENT				SHIFT	NEW CURSOR (smooth)	NEW CURSOR (jump)
// ---------------	--------------------------	-----	-------------------	-----------------
// Far right		Char entered or ^F		Left	Far right		Far right - jump
// Anywhere		^E, cursor past far right	Left	3/4 input width
// Far left		Backspace or ^B			Right	Far left		Far left + jump

// Write a string to the message line, given beginning cursor column, buffer offset, "write leading LineExt ($)" flag, string
// pointer, and ending cursor column.  If "str" is NULL, write from current input buffer position - offset to end of input via
// non-raw mlputc() calls.  Characters are written until the cursor moves past the right edge of the screen or end of string is
// reached.  Return status.
static int mli_write(InpCtrl *pInpCtrl, int beginCol, ushort bufOffset, bool extChar, const char *str, int endCol) {

#if MMDebug & Debug_MsgLnInp
	fprintf(logfile, "mli_write(..., beginCol: %d, bufOffset: %hu,%s,\"%s\", endCol: %d): curInpPos: '%s'...\n",
	 beginCol, bufOffset, extChar ? "true" : "false", str, endCol, pInpCtrl->curInpPos == pInpCtrl->endInpPos ? "EOL" :
	 vizc(pInpCtrl->curInpPos[-bufOffset].c, VizBaseDef));
#endif
	// Move cursor and write leading '$'.
	if(beginCol != term.msgLineCol && mlmove(beginCol) != Success)
		return sess.rtn.status;
	if(extChar)
		mlputc(LineExt, MLRaw);

	// Display string.
	if(str == NULL) {
		InpChar *pInpChar = pInpCtrl->curInpPos - bufOffset;
		while(pInpChar < pInpCtrl->endInpPos) {
			mlputc(pInpChar->c, 0);
			if(term.msgLineCol >= term.cols)
				goto Finish;
			++pInpChar;
			}
		}
	else if(mlputs(extChar ? str + 1 : str, MLRaw | MLFlush | MLForce) != Success)
		return sess.rtn.status;

	if(term.msgLineCol < term.cols)
		mleraseEOL();
Finish:
	// Reposition cursor if needed.
	if(term.msgLineCol != endCol)
		(void) mlmove(endCol);

	return sess.rtn.status;
	}

// Redisplay input buffer on message line and return status.  This routine is called when:
//	1. A navigation key is entered, like ^A or ^F.
//	2. A character is inserted when cursor is at right edge of screen.
//	3. Backspace is entered when line is shifted and cursor is at or near beginning of input area.
//	4. Prompt and input buffer need to be redisplayed during completion in termInp().
// Cursor position in message line is recalculated so that cursor stays in visible range.  If input line needs to be shifted
// left as a result, character at home position is displayed as LineExt ($); if input line extends beyond right edge of screen,
// last character is displayed as LineExt ($).
static int mli_update(InpCtrl *pInpCtrl, bool force) {
	int colPos1, colPos2;
	InpChar *pInpChar;

#if MMDebug & Debug_MsgLnInp
	fputs("mli_update()...\n", logfile);
#endif
	// Calculate new cursor position and determine if moving the cursor (only) is sufficient.  colPos1 is cursor position in
	// input area if line is not shifted; colPos2 is cursor position in input area keeping current shift value (which may be
	// zero).
	colPos1 = 0;
	for(pInpChar = pInpCtrl->inpBuf; pInpChar < pInpCtrl->curInpPos; ++pInpChar)
		colPos1 += pInpChar->len;
	colPos2 = colPos1 - pInpCtrl->leftShift;
	if(!force && (colPos2 > 0 || (colPos2 == 0 && pInpCtrl->leftShift == 0)) && colPos2 < pInpCtrl->inpSize - 1) {
#if MMDebug & Debug_MsgLnInp
		fprintf(logfile, "\tMoving cursor to column %d...\n", colPos2 + pInpCtrl->beginInpCol);
#endif
		// Cursor will still be in visible range... just move it.
		(void) mlmove(colPos2 + pInpCtrl->beginInpCol);
		}
	else {
		int n;
		char *str;
		char workBuf[(pInpCtrl->endInpPos - pInpCtrl->inpBuf) * 5 + 2];

		// workBuf (above) holds space for largest possible display of whole input line; that is, five bytes for each
		// character currently in input buffer (e.g., "<ESC>").  NOTE: Need two extra bytes instead of one for
		// terminating null for some reason -- program crashes otherwise!  Perhaps a bug in stpcpy() called below?

#if MMDebug & Debug_MsgLnInp
		fprintf(logfile, "\tRedisplaying line (inpSize %d, jumpCols %d, workBuf %lu)...\n",
		 pInpCtrl->inpSize, pInpCtrl->jumpCols, sizeof(workBuf));
#endif
		// New cursor position out of visible range or a force... recalculate left shift.
		if(colPos2 < 0 || (colPos2 == 0 && pInpCtrl->leftShift > 0)) {

			// Current position at or before home column (line shifted too far left)... decrease shift amount.
			pInpCtrl->leftShift = (colPos1 <= 1) ? 0 : (pInpCtrl->jumpCols == 1) ? colPos1 - 1 :
			 pInpCtrl->jumpCols >= colPos1 ? 0 : colPos1 - pInpCtrl->jumpCols;
			}
		else if(pInpCtrl->curInpPos == pInpCtrl->endInpPos) {

			// Current position at end of input... place cursor at 3/4 spot.  If shift result is negative (which
			// can happen on a force), set it to zero.
			if((pInpCtrl->leftShift = colPos1 - pInpCtrl->inpSize75) < 0)
				pInpCtrl->leftShift = 0;
			}
		else {
			// Current position at or past right edge of screen... shift line left.
			pInpCtrl->leftShift = colPos1 - (pInpCtrl->jumpCols == 1 ? pInpCtrl->inpSize :
			 pInpCtrl->inpSize - pInpCtrl->jumpCols) + 2;
			}

		// Redisplay visible portion of buffer on message line.
		pInpCtrl->partialLen = n = 0;
		pInpChar = pInpCtrl->inpBuf;
		for(;;) {
			if(n + pInpChar->len > pInpCtrl->leftShift)
				break;
			n += pInpChar->len;
			++pInpChar;
			}
		colPos2 = pInpCtrl->leftShift - n;	// Beginning portion of current char to skip over (if any).
		if(colPos2 > 0)
			pInpCtrl->partialLen = pInpChar->len - colPos2;
		str = workBuf;
		while(pInpChar < pInpCtrl->endInpPos) {
			str = stpcpy(str, vizc(pInpChar->c, VizBaseDef));
			++pInpChar;
			}
#if MMDebug & Debug_MsgLnInp
		fprintf(logfile, "\tleftShift: %d, partialLen: %d\n", pInpCtrl->leftShift, pInpCtrl->partialLen);
#endif
		if(str > workBuf)
			(void) mli_write(pInpCtrl, pInpCtrl->beginInpCol, 0, pInpCtrl->leftShift > 0, workBuf + colPos2,
			 pInpCtrl->beginInpCol + colPos1 - pInpCtrl->leftShift);
		}
	return sess.rtn.status;
	}

// Convert buffer in InpCtrl object to a string.  Return dest.
static char *inpToStr(char *dest, InpCtrl *pInpCtrl) {
	InpChar *pInpChar = pInpCtrl->inpBuf;
	char *dest0 = dest;

	while(pInpChar < pInpCtrl->endInpPos) {
		*dest++ = pInpChar->c;
		++pInpChar;
		}
	*dest = '\0';
	return dest0;
	}

// Delete a character from the message line input buffer.
static int mli_del(InpCtrl *pInpCtrl, LineEdit cmd) {
	InpChar *pInpChar1, *pInpChar2;
	int len;

#if MMDebug & Debug_MsgLnInp
	fprintf(logfile, "mli_del(..., cmd: %d)...\n", cmd);
#endif
	if(cmd == ed_bs) {				// Backspace key.
		if(pInpCtrl->curInpPos == pInpCtrl->inpBuf)
			goto BoundChk;
		--pInpCtrl->curInpPos;
		goto DelChar;
		}
	if(pInpCtrl->curInpPos == pInpCtrl->endInpPos) {			// ^D key.
BoundChk:
		if(pInpCtrl->flags & Term_NoKeyEcho)
			tbeep();
		return sess.rtn.status;
		}
DelChar:
	pInpChar1 = pInpCtrl->curInpPos;

	// Save display length of character being nuked, then remove it from buffer by left-shifting
	// characters that follow it (if any).
	len = pInpChar1->len;
	pInpChar2 = pInpChar1 + 1;
	while(pInpChar2 < pInpCtrl->endInpPos)
		*pInpChar1++ = *pInpChar2++;

	--pInpCtrl->endInpPos;

	// Adjust message line if echoing.
	if(!(pInpCtrl->flags & Term_NoKeyEcho)) {

		// At or near beginning or end of line?
		if(cmd == ed_bs) {
			if(pInpCtrl->leftShift > 0 && term.msgLineCol - len <= pInpCtrl->beginInpCol) {
				if(mli_update(pInpCtrl, false) != Success)
					return sess.rtn.status;
				goto Flush;
				}
			if(pInpCtrl->curInpPos == pInpCtrl->endInpPos && (pInpCtrl->leftShift == 0 ||
			 pInpCtrl->partialLen > 0 || term.msgLineCol - pInpCtrl->beginInpCol > 1)) {
#if MMDebug & Debug_MsgLnInp
				fprintf(logfile, "mli_del(): quick delete of %d char(s).\n", len);
#endif
				do {
					mlputc('\b', MLRaw);
					mlputc(' ', MLRaw);
					mlputc('\b', MLRaw);
					} while(--len > 0);
				goto Flush;
				}
			len = term.msgLineCol - len;
			}
		else {
			if(pInpCtrl->curInpPos == pInpCtrl->endInpPos && len == 1 && (pInpCtrl->leftShift == 0 ||
			 pInpCtrl->partialLen > 0 || term.msgLineCol - pInpCtrl->beginInpCol > 1)) {
				mlputc(' ', MLRaw);
				mlputc('\b', MLRaw);
				goto Flush;
				}
			len = term.msgLineCol;
			}

		// Not simple adjustment or special case... redo-from-cursor needed.
		if(mli_write(pInpCtrl, len, 0, len == pInpCtrl->beginInpCol && pInpCtrl->leftShift > 0, NULL, len) == Success)
Flush:
			(void) mlflush();
		}
	return sess.rtn.status;
	}

// Insert a character into the message line input buffer.  If echoing and "update" is true, update virtual message line also.
static int mli_putc(short c, InpCtrl *pInpCtrl, bool update) {

	if(pInpCtrl->endInpPos < pInpCtrl->endBufPos && c < 0x7f) {
#if MMDebug & Debug_MsgLnInp
		fprintf(logfile, "mli_putc('%s', ...)...\n", vizc(c, VizBaseDef));
#endif
		// Shift input buffer right one position if insertion point not at end.
		if(pInpCtrl->curInpPos < pInpCtrl->endInpPos) {
			InpChar *pInpChar1, *pInpChar2;

			pInpChar1 = (pInpChar2 = pInpCtrl->endInpPos) - 1;
			do {
				*pInpChar2-- = *pInpChar1--;
				} while(pInpChar2 > pInpCtrl->curInpPos);
			}
		pInpCtrl->curInpPos->c = c;
		++pInpCtrl->endInpPos;

		// Update message line.
		if((pInpCtrl->flags & Term_NoKeyEcho))
			++pInpCtrl->curInpPos;
		else {
			char *str = vizc(c, VizBaseDef);
			int len = strlen(str);
			pInpCtrl->curInpPos->len = len;
			++pInpCtrl->curInpPos;
			if(update) {
				if(term.msgLineCol + len < term.cols - 1)
					(void) mli_write(pInpCtrl, term.msgLineCol, 1, false, NULL, term.msgLineCol + len);
				else
					(void) mli_update(pInpCtrl, false);
				}
			}
		}
	else
		tbeep();

	return sess.rtn.status;
	}

// Insert a (possibly null) string into the message line input buffer.  Called only when "echo" is enabled.
static int mli_puts(const char *str, InpCtrl *pInpCtrl) {
	int max = pInpCtrl->endBufPos - pInpCtrl->curInpPos + 1;		// Space left in input buffer + 1.

	while(*str != '\0' && max-- > 0)
		if(mli_putc(*str++, pInpCtrl, false) != Success)
			return sess.rtn.status;
	return mli_update(pInpCtrl, true);
	}

// Move cursor in message line input buffer.
static int mli_nav(InpCtrl *pInpCtrl, LineEdit cmd) {

	switch(cmd) {
		case ed_left:
			if(pInpCtrl->curInpPos == pInpCtrl->inpBuf)
				goto BoundChk;
			--pInpCtrl->curInpPos;
			break;
		case ed_right:
			if(pInpCtrl->curInpPos == pInpCtrl->endInpPos) {
BoundChk:
				if(pInpCtrl->flags & Term_NoKeyEcho)
					tbeep();
				return sess.rtn.status;
				}
			++pInpCtrl->curInpPos;
			break;
		case ed_bol:
			if(pInpCtrl->curInpPos == pInpCtrl->inpBuf)
				return sess.rtn.status;
			pInpCtrl->curInpPos = pInpCtrl->inpBuf;
			break;
		default:	// ed_eol
			if(pInpCtrl->curInpPos == pInpCtrl->endInpPos)
				return sess.rtn.status;
			pInpCtrl->curInpPos = pInpCtrl->endInpPos;
		}

	if(!(pInpCtrl->flags & Term_NoKeyEcho) && mli_update(pInpCtrl, false) == Success)
		(void) mlflush();
	return sess.rtn.status;
	}

// Truncate line in the message line input buffer.
static int mli_trunc(InpCtrl *pInpCtrl, LineEdit cmd) {

	if(cmd == ed_kill) {
		if(pInpCtrl->endInpPos > pInpCtrl->curInpPos) {
			pInpCtrl->endInpPos = pInpCtrl->curInpPos;
			if(!(pInpCtrl->flags & Term_NoKeyEcho))
				goto Trunc;
			}
		}
	else if(pInpCtrl->endInpPos > pInpCtrl->inpBuf) {
		pInpCtrl->curInpPos = pInpCtrl->endInpPos = pInpCtrl->inpBuf;
		if(!(pInpCtrl->flags & Term_NoKeyEcho)) {
			pInpCtrl->leftShift = 0;
			if(mlmove(pInpCtrl->beginInpCol) == Success) {
Trunc:
				mleraseEOL();
				(void) mlflush();
				}
			}
		}
	return sess.rtn.status;
	}

// Erase message line, display prompt, and update "beginInpCol" in InpCtrl object.  Return status.
static int mli_prompt(InpCtrl *pInpCtrl) {

	// Clear message line, display prompt, and save "home" column.
	if(mlputs(pInpCtrl->prompt, pInpCtrl->flags & Term_Attr ? MLHome | MLTermAttr | MLFlush | MLForce :
	 MLHome | MLFlush | MLForce) == Success)
		pInpCtrl->beginInpCol = term.msgLineCol;
	return sess.rtn.status;
	}

// Redisplay prompt and current text in input buffer on message line.
static int mli_reset(InpCtrl *pInpCtrl) {

	if(mli_prompt(pInpCtrl) != Success)
		return sess.rtn.status;
	pInpCtrl->curInpPos = pInpCtrl->endInpPos;
	pInpCtrl->leftShift = 0;
	return pInpCtrl->flags & Term_NoKeyEcho ? sess.rtn.status : mli_update(pInpCtrl, true);
	}

// Initialize given InpCtrl object.  Return status.
static int mli_open(InpCtrl *pInpCtrl, const char *prompt, InpChar *pInpChar, const char *defVal, uint maxLen, uint flags) {

#if MMDebug & Debug_MsgLnInp
	fprintf(logfile, "mli_open(...,prompt: \"%s\", defVal: \"%s\"): horzJumpPct %d, horzJumpCols: %d\n",
	 prompt, defVal, vTerm.horzJumpPct, vTerm.horzJumpCols);
#endif
	pInpCtrl->prompt = prompt;
	pInpCtrl->maxLen = maxLen;
	pInpCtrl->endBufPos = (pInpCtrl->inpBuf = pInpCtrl->curInpPos = pInpCtrl->endInpPos = pInpChar) + maxLen;
	pInpCtrl->flags = flags;
	pInpCtrl->partialLen = pInpCtrl->leftShift = 0;

	if(mli_prompt(pInpCtrl) == Success) {
		pInpCtrl->inpSize75 = (pInpCtrl->inpSize = term.cols - pInpCtrl->beginInpCol) * 75 / 100;
		if((pInpCtrl->jumpCols = vTerm.horzJumpPct * pInpCtrl->inpSize / 100) == 0)
			pInpCtrl->jumpCols = 1;

		// Copy default value to input buffer and display it, if present.
		if(defVal != NULL && *defVal != '\0')
			(void) mli_puts(defVal, pInpCtrl);
		}

	return sess.rtn.status;
	}

// Convert input buffer to a string and save in *ppDatum.
static int mli_close(InpCtrl *pInpCtrl, Datum **ppDatum) {

#if MMDebug & Debug_MsgLnInp
	fprintf(logfile, "mli_close(): curInpPos: +%lu, endInpPos: +%lu\n",
	 pInpCtrl->curInpPos - pInpCtrl->inpBuf, pInpCtrl->endInpPos - pInpCtrl->inpBuf);
#endif
	if((*ppDatum = dnewtrack()) == NULL || dsalloc(*ppDatum, (pInpCtrl->endInpPos - pInpCtrl->inpBuf) + 1) != 0)
		return libfail();
	inpToStr((*ppDatum)->str, pInpCtrl);
	return sess.rtn.status;
	}

// Build a prompt and save the result in "dest" (assumed to be >= current terminal width in size).  Any non-printable characters
// in the prompt string are ignored.  The result is shrunk if necessary to fit in size "prmtPct * current terminal width".  If
// the prompt string begins with a letter, ": " is appended; otherwise, the prompt string is used as is, however any leading
// single (') or double (") quote character is stripped off first and any trailing white space is moved to the very end.  Return
// status.
static int buildPrompt(char *dest, const char *prompt1, int maxPct, short defVal, short delimChar, uint ctrlFlags) {
	bool addSpace;
	bool literal = false;
	bool addColon = false;
	char *str, *promptEnd;
	const char *src;
	int promptLen, maxLen;
	DFab prompt;

	if(dopentrack(&prompt) != 0)
		goto LibFail;
	src = strchr(prompt1, '\0');

	if(is_letter(*prompt1))
		addColon = true;
	else if(*prompt1 == '\'' || *prompt1 == '"') {
		++prompt1;
		literal = true;
		while(src[-1] == ' ')
			--src;
		}

	// Save adjusted prompt, minus any invisible characters.
	for(str = (char *) prompt1; str < src; ++str)
		if(*str >= ' ' && *str <= '~' && dputc(*str, &prompt, 0) != 0)
			goto LibFail;
	addSpace = *prompt1 != '\0' && src[-1] != ' ';
	if(defVal >= 0) {
		if(addSpace) {
			if(dputc(' ', &prompt, 0) != 0)
				goto LibFail;
			addSpace = false;
			}
		if(dputc('[', &prompt, 0) != 0 || dputs(vizc(defVal, VizBaseDef), &prompt, 0) != 0 ||
		 dputc(']', &prompt, 0) != 0)
			goto LibFail;
		addColon = true;
		}
	if(delimChar != RtnKey && delimChar != AltRtnKey) {
		if(addSpace && dputc(' ', &prompt, 0) != 0)
			goto LibFail;
		if(dputc(ektoc(delimChar, false), &prompt, VizSpace) != 0)
			goto LibFail;
		addColon = true;
		}
	if(*src == ' ' && dputs(src, &prompt, 0) != 0)
		goto LibFail;

	// Add two characters to make room for ": " (if needed) and close the fabrication object.
	if(dputs("xx", &prompt, 0) != 0 || dclose(&prompt, FabStr) != 0)
		goto LibFail;
	promptEnd = strchr(str = prompt.pDatum->str, '\0');
	promptEnd[-2] = '\0';

	// Add ": " if needed.
	if(*src != ' ' && addColon && !literal && promptEnd[-3] != ' ')
		strcpy(promptEnd - 2, ": ");

	// Make sure prompt fits on the screen.
	maxLen = maxPct * term.cols / 100;
	promptLen = strlen(str);
	strfit(dest, maxLen + (ctrlFlags & Term_Attr ? attrCount(str, promptLen, maxLen) + 1 : 1), str, promptLen);
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Compare a string to the message line input buffer and return Boolean result.
static bool inpcmp(InpCtrl *pInpCtrl, const char *str) {
	InpChar *pInpChar = pInpCtrl->inpBuf;

	// If str is shorter than the input buffer, the trailing null in str won't match, so no special check needed.
	while(pInpChar < pInpCtrl->endInpPos) {
		if(pInpChar->c != *str++)
			return false;
		++pInpChar;
		}
	return true;
	}

// Compare a string to the message line input buffer, ignoring case, and return Boolean result.
static bool inpcasecmp(InpCtrl *pInpCtrl, const char *str) {
	InpChar *pInpChar = pInpCtrl->inpBuf;

	// If str is shorter than the input buffer, the trailing null in str won't match, so need need to check for it.
	while(pInpChar < pInpCtrl->endInpPos) {
		if(lowCase[(int) pInpChar->c] != lowCase[(int) *str++])
			return false;
		++pInpChar;
		}
	return true;
	}

// Attempt a completion on a name, given pointer to InpCtrl object (containing partial name to complete) and pointer to
// Completion object, which contains a pointer to a callback routine that supplies the list items.  Add and display completed
// characters to terminal input buffer and return status (Success) if unique name found; otherwise, return NotFound, bypassing
// rsset().  Any item that is longer than MaxTermInp bytes is truncated to that length.
static int cMatch(InpCtrl *pInpCtrl, Completion *pComp) {
	short c;
	const char *item;
	uint i;
	uint matches = 0;			// Match count.
	int status;
	uint n = pInpCtrl->endInpPos - pInpCtrl->inpBuf;
	uint longestLen;			// Length of longest match.
	char longestMatch[MaxTermInp + 1];	// Longest match found so far.

	// Set up buffer for fast completions -- so that the item list can be scanned just once instead of for each character
	// completed.
	inpToStr(longestMatch, pInpCtrl);
	if(pComp->nextItem(true, pComp->context, NULL) != Success)
		return sess.rtn.status;

	// Start attempting completions.  Begin at the first item and scan the list.
	while((status = pComp->nextItem(false, pComp->context, &item)) == Success) {

		// Does the initial portion of this item match the characters in the input buffer?
		if(n > 0 && !pComp->cmp(pInpCtrl, item))
			continue;

		// A match.  If this is the first, simply record it and save the next character.
		if(++matches == 1) {
			stplcpy(longestMatch, item, sizeof(longestMatch));
			longestLen = strlen(longestMatch);
			c = item[n];
			}
		else {
			if(item[n] != c && (pComp->cmp == inpcmp || lowCase[(int) item[n]] != lowCase[(int) c]))

				// A difference... stop here.  Can't auto-extend input buffer any further.
				goto Fail;

			// Item matches.  Update longestMatch; that is, find longest common prefix.
			for(i = n + 1; i < longestLen; ++i)
				if(longestMatch[i] != item[i]) {
					longestLen = i;
					longestMatch[i] = '\0';
					break;
					}
			}
		}
	if(status != NotFound)
		return sess.rtn.status;

	// If no match was found, it's a bust.  Beep the beeper.
	if(matches == 0)
		tbeep();
	else {
		// Found one or more matches.  The longestMatch buffer contains the longest match, so display it.
		pInpCtrl->curInpPos = pInpCtrl->endInpPos;
		if(mli_puts(longestMatch + n, pInpCtrl) != Success)
			return sess.rtn.status;

		// If only one item matched, we are done -- we have completed all the way.
		if(matches == 1)
			return sess.rtn.status;
		}
Fail:
	// Match not found or unfinished completion.
	return NotFound;
	}

// Make a completion list based on a partial name, given pointer to InpCtrl object (containing partial name), result buffer
// pointer, and pointer to Completion object, which contains a pointer to a callback routine that supplies the list items.
// Return status.
static int cList(InpCtrl *pInpCtrl, Buffer *pBuf, Completion *pComp) {
	const char *item;

	// Start at the first item and scan the list.
	if(pComp->nextItem(true, pComp->context, NULL) == Success)
		while(pComp->nextItem(false, pComp->context, &item) == Success) {

			// Does the initial portion of this item match the characters in the input buffer?
			if(pComp->cmp(pInpCtrl, item))
				if(bappend(pBuf, item) != Success)
					break;
			}

	return sess.rtn.status;
	}

// Helper routine for cMatch() and cList().  Store next buffer name in *pItem and return status, or NotFound if none left.
static int nextBufname(bool reset, void *context, const char **pItem) {
	CompBuf *pCompBuf = context;
	if(reset) {
		if(ohAryPrep(&buf, &pCompBuf->pAry) != 0)
			return libfail();
		}
	else {
		HashNode *pNode;
		Buffer *pBuf;

		while((pNode = *pCompBuf->pAry++) != NULL) {
			pBuf = bufPtr(pNode->pVal);

			// Is this buffer a candidate?  Skip hidden buffers if no partial name.
			if(pCompBuf->force || pCompBuf->pInpCtrl->endInpPos > pCompBuf->pInpCtrl->inpBuf ||
			 !(pBuf->flags & BFHidden)) {
				*pItem = pBuf->bufname;
				goto Retn;
			 	}
			}
		return NotFound;
		}
Retn:
	return sess.rtn.status;
	}

// Attempt a completion on a buffer name via cMatch(), given pointer to InpCtrl object.  Return status.
static int cMatchBuf(InpCtrl *pInpCtrl) {
	CompBuf compBuf = {-1, pInpCtrl};
	Completion comp = {nextBufname, inpcmp, &compBuf};
	return cMatch(pInpCtrl, &comp);
	}

// Make a completion list based on a partial buffer name via cList(), given pointer to InpCtrl object, indirect result buffer
// pointer, and "force all" flag.  Return status.
static int cListBuf(InpCtrl *pInpCtrl, Buffer *pBuf, bool forceAll) {
	CompBuf compBuf = {forceAll, pInpCtrl};
	Completion comp = {nextBufname, inpcmp, &compBuf};
	return cList(pInpCtrl, pBuf, &comp);
	}

// Helper routine for cMatch() and cList().  Return next command, function, or alias name, or NotFound status if none left.
static int nextCFA(bool reset, void *context, const char **pItem) {
	CompCFA *pCompCFA = context;
	if(reset)
		pCompCFA->ppHashNode1 = pCompCFA->ppHashNode;
	else {
		HashNode **ppHashNode1;
		ExecPtr *pExec;
		while(pCompCFA->ppHashNode1 < pCompCFA->ppHashNodeEnd) {
			ppHashNode1 = pCompCFA->ppHashNode1++;
			pExec = execPtr(*ppHashNode1);
			if(pExec->type & pCompCFA->selector) {
				*pItem = (*ppHashNode1)->key;
				goto Retn;
				}
			}
		return NotFound;
		}
Retn:
	return sess.rtn.status;
	}

// Attempt a completion on a command, function, or alias name via cMatch(), given pointer to InpCtrl object, selector mask, and
// sorted hash table.  Return status.
static int cMatchCFA(InpCtrl *pInpCtrl, uint selector, HashNode **ppHashNode) {
	CompCFA compCFA = {selector, ppHashNode, ppHashNode + execTable.nodeCount};
	Completion comp = {nextCFA, inpcmp, &compCFA};
	return cMatch(pInpCtrl, &comp);
	}

// Make a completion list based on a partial name via cList(), given pointer to InpCtrl object, indirect result buffer pointer,
// selector mask, and sorted hash table.  Return status.
static int cListCFA(InpCtrl *pInpCtrl, Buffer *pBuf, uint selector, HashNode **ppHashNode) {
	CompCFA compCFA = {selector, ppHashNode, ppHashNode + execTable.nodeCount};
	Completion comp = {nextCFA, inpcmp, &compCFA};
	return cList(pInpCtrl, pBuf, &comp);
	}

// Helper routine for cMatch() and cList().  Return next filename, or NotFound status if none left.
static int nextFile(bool reset, void *context, const char **pItem) {
	CompFile *pCompFile = context;
	int status;

	if(reset) {
		char pathname[MaxPathname + 1];

		// Open the directory entered (in the input buffer).
		inpToStr(pathname, pCompFile->pInpCtrl);
		status = eopendir(pathname, &pCompFile->filename);
		}
	else if((status = ereaddir()) == Success)
		*pItem = pCompFile->filename;

	return status;
	}

// Attempt a completion on a filename via cMatch(), given pointer to InpCtrl object.  Return status.
static int cMatchFile(InpCtrl *pInpCtrl) {
	CompFile compFile = {pInpCtrl, NULL};
	Completion comp = {nextFile, inpcmp, &compFile};

	// If only one file matches, it is completed unless the last character is a '/', indicating a
	// directory.  In that case we do NOT want to signal a completion.
	int status = cMatch(pInpCtrl, &comp);
	return (status == Success && pInpCtrl->endInpPos[-1].c == '/') ? NotFound : status;
	}

// Make a completion list based on a partial pathname via cList(), given pointer to InpCtrl object and indirect result buffer
// pointer.  Return status.
static int cListFile(InpCtrl *pInpCtrl, Buffer *pBuf) {
	CompFile compFile = {pInpCtrl, NULL};
	Completion comp = {nextFile, inpcmp, &compFile};
	if(cList(pInpCtrl, pBuf, &comp) == Success && pBuf->pFirstLine->next != NULL) {

		// At least two lines written to completion list buffer... sort them.
		pBuf->face.point.pLine = pBuf->pFirstLine;
		pBuf->face.point.offset = 0;
		(void) bsort(pBuf, &pBuf->face.point, 0, 0);
		}
	return sess.rtn.status;
	}

// Helper routine for cMatch() and cList().  Return next macro name, or NotFound status if none left.
static int nextMacro(bool reset, void *context, const char **pItem) {
	CompMacro *pCompMacro = context;
	if(reset)
		pCompMacro->ppHashNode1 = pCompMacro->ppHashNode;
	else {
		HashNode **ppHashNode1;
		if(pCompMacro->ppHashNode1 < pCompMacro->ppHashNodeEnd) {
			ppHashNode1 = pCompMacro->ppHashNode1++;
			*pItem = (*ppHashNode1)->key;
			}
		else
			return NotFound;
		}

	return sess.rtn.status;
	}

// Attempt a completion on a macro name via cMatch(), given pointer to InpCtrl object.  Return status.
static int cMatchMacro(InpCtrl *pInpCtrl, HashNode **ppHashNode) {
	CompMacro compMacro = {ppHashNode, ppHashNode + macro.index.table.nodeCount};
	Completion comp = {nextMacro, inpcmp, &compMacro};
	return cMatch(pInpCtrl, &comp);
	}

// Make a completion list based on a partial name via cList(), given pointer to InpCtrl object and indirect result buffer
// pointer.  Return status.
static int cListMacro(InpCtrl *pInpCtrl, Buffer *pBuf, HashNode **ppHashNode) {
	CompMacro compMacro = {ppHashNode, ppHashNode + macro.index.table.nodeCount};
	Completion comp = {nextMacro, inpcmp, &compMacro};
	return cList(pInpCtrl, pBuf, &comp);
	}

// Helper routine for cMatch() and cList().  Return next ring name, or NotFound status if none left.
static int nextRing(bool reset, void *context, const char **pItem) {
	CompRing *pCompRing = context;
	if(reset)
		pCompRing->pRing = ringTable;
	else {
		if(pCompRing->pRing == pCompRing->pRingEnd)
			return NotFound;
		*pItem = pCompRing->pRing->ringName;
		++pCompRing->pRing;
		}

	return sess.rtn.status;
	}

// Attempt a completion on a macro name via cMatch(), given pointer to InpCtrl object.  Return status.
static int cMatchRing(InpCtrl *pInpCtrl) {
	CompRing compRing = {NULL, ringTable + ringTableSize};
	Completion comp = {nextRing, inpcasecmp, &compRing};
	return cMatch(pInpCtrl, &comp);
	}

// Make a completion list based on a partial name via cList(), given pointer to InpCtrl object and indirect result buffer
// pointer.  Return status.
static int cListRing(InpCtrl *pInpCtrl, Buffer *pBuf) {
	CompRing compRing = {NULL, ringTable + ringTableSize};
	Completion comp = {nextRing, inpcasecmp, &compRing};
	return cList(pInpCtrl, pBuf, &comp);
	}

// Helper routine for cMatch() and cList().  Return next buffer or global mode name, or NotFound status if none left.
static int nextMode(bool reset, void *context, const char **pItem) {

	if(reset) {
		if(ohAryPrep(&mode.idx, (HashNode ***) context) != 0)
			return libfail();
		}
	else {
		HashNode *pNode = *(*((HashNode ***) context))++;
		if(pNode == NULL)
			return NotFound;
		*pItem = modePtr(pNode->pVal)->name;
		}

	return sess.rtn.status;
	}

// Attempt a completion on a mode name via cMatch(), given pointer to InpCtrl object.  Return status.
static int cMatchMode(InpCtrl *pInpCtrl) {
	HashNode **pAry = NULL;
	Completion comp = {nextMode, inpcasecmp, &pAry};
	return cMatch(pInpCtrl, &comp);
	}

// Make a completion list based on a partial mode name via cList(), given pointer to InpCtrl object and indirect result buffer
// pointer.  Return status.
static int cListMode(InpCtrl *pInpCtrl, Buffer *pBuf) {
	HashNode **pAry = NULL;
	Completion comp = {nextMode, inpcasecmp, &pAry};
	return cList(pInpCtrl, pBuf, &comp);
	}

// Helper routine for cMatch() and cList().  Return next variable name, or NotFound status if none left.
static int nextVar(bool reset, void *context, const char **pItem) {
	CompVar *pCompVar = context;

	if(reset)
		pCompVar->pVar = pCompVar->cvList;
	else {
		if(pCompVar->pVar == pCompVar->pVarEnd)
			return NotFound;
		*pItem = *pCompVar->pVar++;
		}

	return sess.rtn.status;
	}

// Attempt a completion on a variable name via cMatch(), given pointer to InpCtrl object, variable list vector, and size.
// Return status.
static int cMatchVar(InpCtrl *pInpCtrl, const char *cvList[], uint cvListSize) {
	CompVar compVar = {cvList, NULL, cvList + cvListSize};
	Completion comp = {nextVar, inpcmp, &compVar};
	return cMatch(pInpCtrl, &comp);
	}

// Make a completion list based on a partial variable name via cList(), given pointer to InpCtrl object, indirect result buffer
// pointer, variable list vector, and size.  Return status.
static int cListVar(InpCtrl *pInpCtrl, Buffer *pBuf, const char *cvList[], uint cvListSize) {
	CompVar compVar = {cvList, NULL, cvList + cvListSize};
	Completion comp = {nextVar, inpcmp, &compVar};
	return cList(pInpCtrl, pBuf, &comp);
	}

// Get one key from terminal and save in pRtnVal as a key literal or character (int).  Return status.
static int termInp1(Datum *pRtnVal, const char *defVal, short delimChar, uint delim2, uint argFlags, uint ctrlFlags) {
	ExtKey extKey;
	bool quote = false;

	if(((ctrlFlags & Term_KeyMask) == Term_OneKeySeq ?
	 getKeySeq(true, &extKey, NULL, true) : getkey(true, &extKey, true)) != Success) // Get a key.
		return sess.rtn.status;
	if(((ctrlFlags & Term_KeyMask) == Term_OneChar) &&		// If C-q...
	 extKey == (Ctrl | 'Q')) {
		quote = true;						// get another key.
		if(getkey(true, &extKey, true) != Success)
			return sess.rtn.status;
	 	}
	if(mlerase(MLForce) != Success)					// Clear the message line.
		return sess.rtn.status;
	if((ctrlFlags & Term_KeyMask) == Term_OneChar) {
		if(quote)						// Quoted key?
			goto SetKey;					// Yes, set it.
		if(extKey == delimChar || extKey == delim2) {		// Terminator?
			if(defVal == NULL)				// Yes, return default value or nil.
				dsetnil(pRtnVal);
			else
				dsetint((long) *defVal, pRtnVal);
			}
		else if(extKey == (Ctrl | ' ')) {			// No, C-SPC?
			if(argFlags & ArgNotNull1)			// Yes, return error or null character.
				(void) rsset(Failure, 0, text187, text53);
					// "%s cannot be null", "Value"
			else
				dsetint(0L, pRtnVal);
			}
		else if(extKey == coreKeys[CK_Abort].extKey)		// No, user abort?
			(void) abortInp();				// Yes, return UserAbort status.
		else
SetKey:
			dsetint(ektoc(extKey, false), pRtnVal);		// No, save (raw) key and return.
		}
	else {
		char keyBuf[16];

		ektos(keyBuf, extKey, false);
		if(dsetstr(keyBuf, pRtnVal) != 0)
			return libfail();
		}

	return sess.rtn.status;
	}

// Expand ~, ~username, and $var in input buffer if possible.  Return status.
static int expandVar(InpCtrl *pInpCtrl) {
	char *str;

	if(pInpCtrl->inpBuf->c == '~') {
		if(pInpCtrl->endInpPos == pInpCtrl->inpBuf + 1) {

			// Solo '~'.
			str = "HOME";
			goto ExpVar;
			}
		else {
			// Have ~username... lookup the home directory.
			char dir[MaxPathname + 1];
			inpToStr(dir, pInpCtrl);
			struct passwd *pPass = getpwnam(dir + 1);
			if(pPass == NULL)
				goto Beep;
			str = pPass->pw_dir;
			}
		}
	else {
		// Expand environmental variable.
		char workBuf[MaxTermInp + 1];
		inpToStr(workBuf, pInpCtrl);
		str = workBuf + 1;
ExpVar:
		if((str = getenv(str)) == NULL || strlen(str) > pInpCtrl->maxLen) {
Beep:
			tbeep();
			return sess.rtn.status;
			}
		}

	// Expansion successful.  Replace input buffer with new value and add a trailing slash.
	if(mli_trunc(pInpCtrl, ed_erase) == Success && mli_puts(str, pInpCtrl) == Success &&
	 mli_putc('/', pInpCtrl, true) == Success)
		(void) mlflush();

	return sess.rtn.status;
	}

// Get input from the terminal, possibly with completion, and return in "pRtnVal" as a string (or nil) given result pointer,
// prompt, argument flags, control flags, and parameters pointer.  If last argument is NULL, default parameter (TermInpCtrl)
// values are used.  Recognized argument flags are ArgNil1, ArgNIS1, and ArgNotNull1.  Control flags are prompt options + type
// of completion + selector mask for TermC_CFA calls.  If completion is requested, the delimiter is forced to be "return";
// otherwise, if delimiter is "return" or "newline", it's partner is also accepted as a delimiter; otherwise, "return" is
// converted to "newline".  If maxLen is zero, it is set to the maximum.  Return status and value per the following table:
//
//	Input Result		Return Status		Return Value
//	===============		==============		============
//	Terminator only		Success/Failure		Error if ArgNil1 and ArgNIS1 not set, otherwise nil.
//	C-SPC			Success/Failure		Error if ArgNotNull1, otherwise null string or character.
//	Char(s) entered		Success			Input string.
int termInp(Datum *pRtnVal, const char *basePrompt, uint argFlags, uint ctrlFlags, TermInpCtrl *pTermInp) {
	ushort delim2;
	TermInpCtrl termInpCtrl = {NULL, RtnKey, 0, NULL};
	char prompt[term.cols];

	if(pTermInp != NULL)
		termInpCtrl = *pTermInp;

	// Error if doing a completion but echo is off.
	if(ctrlFlags & TermC_Mask) {
		if(ctrlFlags & Term_NoKeyEcho)
			return rsset(Failure, RSNoFormat, text54);
				// "Key echo required for completion"
		termInpCtrl.delimKey = RtnKey;
		}

	// Error if length of default value exceeds maximum input length.
	if(termInpCtrl.maxLen == 0)
		termInpCtrl.maxLen = MaxTermInp;
	if(termInpCtrl.defVal != NULL && !(ctrlFlags & Term_NoDef)) {
		size_t defLen = strlen(termInpCtrl.defVal);
		uint max = ctrlFlags & Term_KeyMask ? 1 : termInpCtrl.maxLen;
		if(defLen > max)
			return rsset(Failure, 0, text110, defLen, max);
				// "Default value length (%lu) exceeds maximum input length (%u)"
		}

	// If delimiter is CR or NL, accept it's partner as a delimiter also.
	delim2 = (termInpCtrl.delimKey == RtnKey) ? AltRtnKey : (termInpCtrl.delimKey == AltRtnKey) ? RtnKey : 0xffff;

	// Build prompt.
	if(buildPrompt(prompt, basePrompt, ctrlFlags & Term_LongPrmt ? 90 : term.maxPromptPct,
	 (ctrlFlags & Term_KeyMask) == Term_OneChar && termInpCtrl.defVal != NULL ? *termInpCtrl.defVal : -1,
	 termInpCtrl.delimKey, ctrlFlags) != Success)
		return sess.rtn.status;

	// One key?
	if(ctrlFlags & Term_KeyMask) {

		// Display prompt and get a key.
		if(mlputs(prompt, ctrlFlags & Term_Attr ? MLHome | MLTermAttr | MLFlush | MLForce :
		 MLHome | MLFlush | MLForce) != Success)
			return sess.rtn.status;
		return termInp1(pRtnVal, termInpCtrl.defVal, termInpCtrl.delimKey, delim2, argFlags, ctrlFlags);
		}

	// Not a single-key request.  Get multiple characters, with completion if applicable.
	ExtKey extKey;			// Current extended input character.
	int r;				// Return code.
	long e;				// Ring entry.
	bool forceAll;			// Show hidden buffers?
	bool quoteNext = false;		// Quoting next character?
	bool popup = false;		// Completion pop-up window being displayed?
	bool scrnRefresh = false;	// Update screen before getting next key.
	bool cycleRing = false;		// Don't cycle search or replacement ring for first C-p key.
	LineEdit cmd;
	uint cvListSize;		// Variable list size.
	Datum *pDatum;
	HashNode **ppHashNode;		// For macro and command completion.
	int (*func)(InpCtrl *pInpCtrl, LineEdit cmd);
	InpCtrl inpCtrl;
	InpChar inpBuf[MaxTermInp];	// Input buffer.
	const char *cvList[cvListSize = ctrlFlags & TermC_AllConstVar ? cvcount(ctrlFlags) : 1];	// Variable name list.
	HashNode *macCFANameList[ctrlFlags & TermC_Macro ? macro.index.table.nodeCount + 1 : ctrlFlags & TermC_CFA ?
	 execTable.nodeCount : 1];	// Macro or command name list.

	// Get ready for terminal input.
	if(mli_open(&inpCtrl, prompt, inpBuf, ctrlFlags & Term_NoDef ? NULL : termInpCtrl.defVal, termInpCtrl.maxLen,
	 ctrlFlags & (Term_Attr | Term_NoKeyEcho)) != Success)
		return sess.rtn.status;
	if(ctrlFlags & TermC_AllConstVar)
		cvsort(cvList, cvListSize, ctrlFlags);
	else if(ctrlFlags & (TermC_Macro | TermC_CFA)) {
		ppHashNode = macCFANameList;
		if(hsort(ctrlFlags & TermC_Macro ? &macro.index.table : &execTable, hcmp, &ppHashNode) != 0)
			return libfail();
		}

	// Get a string from the user with line-editing capabilities (via control characters and arrow keys).  Process each key
	// via mli_xxx() routines, which manage the cursor on the message line and edit characters in the inpBuf array.  Loop
	// until a line delimiter or abort key is entered.
	goto JumpStart;
	for(;;) {
		// Update screen if pop-up window is being displayed (from a completion) and refresh requested.
		if(popup && scrnRefresh) {
			if(update(1) != Success || mlrestore() != Success)
				return sess.rtn.status;
			popup = false;
			}
		scrnRefresh = false;
JumpStart:
		// Get a keystroke and decode it.
		if(getkey(true, &extKey, false) != Success)
			return sess.rtn.status;

		// If the line delimiter or C-SPC was entered, wrap it up.
		if((extKey == termInpCtrl.delimKey || extKey == delim2 || extKey == (Ctrl | ' ')) && !quoteNext) {

			// Clear the message line.
			if(mlerase(MLForce) != Success)
				return sess.rtn.status;

			// If C-SPC was entered, return null; otherwise, if nothing was entered, return nil; otherwise, break
			// out of key-entry loop (and return characters in input buffer).
			if(extKey == (Ctrl | ' ')) {
				if(argFlags & ArgNotNull1)
					return rsset(Failure, 0, text187, text53);
						// "%s cannot be null", "Value"
				dsetnull(pRtnVal);
				return sess.rtn.status;
				}
			if(inpCtrl.endInpPos == inpCtrl.inpBuf) {
				if(!(argFlags & (ArgNil1 | ArgNIS1)))
					return rsset(Failure, 0, text214, text53);
						// "%s cannot be nil", "Value"
				dsetnil(pRtnVal);
				return sess.rtn.status;
				}
			break;
			}

#if MMDebug & Debug_MsgLnInp
		fprintf(logfile, "termInp(): Got extended key 0x%.4X '%s'\n", (uint) extKey, vizc(extKey & 0x7f, VizBaseDef));
#endif
		// If not in quote mode, check key that was entered.
		if(!quoteNext) {
			if(extKey == coreKeys[CK_Abort].extKey)		// Abort... bail out.
				return abortInp();
			switch(extKey) {
				case Ctrl | '?':			// Rubout/erase... back up.
					cmd = ed_bs;
					goto DelChar;
				case Ctrl | 'D':			// Delete... erase current char.
				case FKey | 'D':
					cmd = ed_del;
DelChar:
					func = mli_del;
					goto Call;
				case Ctrl | 'B':			// ^B or left arrow... move cursor left.
				case FKey | 'B':
					cmd = ed_left;
					goto Nav;
				case Ctrl | 'F':			// ^F or right arrow... move cursor right.
				case FKey | 'F':
					cmd = ed_right;
					goto Nav;
				case Ctrl | 'P':			// ^P, or up arrow... get previous ring entry or
				case FKey | 'P':			// move cursor to BOL.
					if(termInpCtrl.pRing != NULL) {
						e = 1;
						goto GetRing;
						}
					// Fall through.
				case Ctrl | 'A':			// ^A... move cursor to BOL.
					cmd = ed_bol;
					goto Nav;
				case Ctrl | 'N':			// ^N, or down arrow... get next ring entry or
			 	case FKey | 'N':			// move cursor to EOL.
					if(termInpCtrl.pRing != NULL) {
						e = -1;
GetRing:
						if(termInpCtrl.pRing->size == 0)
							goto BadKey;
						if(cycleRing || e != 1)
							(void) rcycle(termInpCtrl.pRing, e, false);	// Can't fail.
						cycleRing = true;

						// If ring entry is a byte string (which indicates it contains at least one
						// embedded null byte), treat as a null-terminated string and ignore whatever
						// bytes are beyond the null because embedded nulls are not supported in string
						// data types.
						if(mli_trunc(&inpCtrl, ed_erase) != Success ||
						 mli_puts(termInpCtrl.pRing->pEntry->data.type == dat_byteStr ?
						 termInpCtrl.pRing->pEntry->data.u.mem.ptr :
						 termInpCtrl.pRing->pEntry->data.str, &inpCtrl) != Success)
							return sess.rtn.status;
						scrnRefresh = true;
						continue;
						}
					// Fall through.
				case Ctrl | 'E':			// ^E... move cursor to EOL.
					cmd = ed_eol;
Nav:
					func = mli_nav;
					goto Call;
				case Ctrl | 'K':			// ^K... truncate line and continue.
					cmd = ed_kill;
					goto Trunc;
				case Ctrl | 'U':			// ^U... erase buffer and continue.
					cmd = ed_erase;
Trunc:
					scrnRefresh = true;
					func = mli_trunc;
Call:
					if(func(&inpCtrl, cmd) != Success)
						return sess.rtn.status;
					continue;
				}
			if(extKey & FKey)				// Non-arrow function key... ignore it.
				continue;
			if(extKey == coreKeys[CK_Quote].extKey) {	// ^Q... quote next character.
				quoteNext = true;
				continue;
				}
			if(extKey == RtnKey)				// CR... convert to newline.
				extKey = AltRtnKey;

			// Check completion special keys.
			else if(ctrlFlags & TermC_Mask) {
				if(extKey == (Ctrl | 'I')) {		// Tab... attempt a completion.
					switch(ctrlFlags & TermC_Mask) {
						case TermC_Buffer:
							r = cMatchBuf(&inpCtrl);
							break;
						case TermC_CFA:
							r = cMatchCFA(&inpCtrl, ctrlFlags, ppHashNode);
							break;
						case TermC_Filename:
							r = cMatchFile(&inpCtrl);
							break;
						case TermC_Macro:
							r = cMatchMacro(&inpCtrl, ppHashNode);
							break;
						case TermC_Ring:
							r = cMatchRing(&inpCtrl);
							break;
						case TermC_Mode:
							r = cMatchMode(&inpCtrl);
							break;
						default:	// TermC_*Var*
							r = cMatchVar(&inpCtrl, cvList, cvListSize);
						}
					if(r == Success && !(ctrlFlags & TermC_NoAuto)) {	// Success!
						centiPause(sess.fencePause);
						break;
						}
					forceAll = false;
					goto CompList;
					}
				if(extKey == '/' && ctrlFlags & TermC_Filename && inpCtrl.curInpPos == inpCtrl.endInpPos) {
					size_t size = inpCtrl.curInpPos - inpCtrl.inpBuf;
					if((size > 0 && inpCtrl.inpBuf->c == '~') || (size > 1 && inpCtrl.inpBuf->c == '$')) {
						if(expandVar(&inpCtrl) != Success)
							return sess.rtn.status;
						continue;
						}
					}
				if(extKey == (Shift | Ctrl | 'I')) {	// S-TAB... pop list of all items unconditionally.
					Buffer *pBuf;
					forceAll = true;
CompList:
					// Make a completion list and pop it (without key input).
					if(sysBuf(text125, &pBuf, 0) != Success)
							// "Completions"
						return sess.rtn.status;
					switch(ctrlFlags & TermC_Mask) {
						case TermC_Buffer:
							r = cListBuf(&inpCtrl, pBuf, forceAll);
							break;
						case TermC_CFA:
							r = cListCFA(&inpCtrl, pBuf, ctrlFlags, ppHashNode);
							break;
						case TermC_Filename:
							r = cListFile(&inpCtrl, pBuf);
							break;
						case TermC_Macro:
							r = cListMacro(&inpCtrl, pBuf, ppHashNode);
							break;
						case TermC_Ring:
							r = cListRing(&inpCtrl, pBuf);
							break;
						case TermC_Mode:
							r = cListMode(&inpCtrl, pBuf);
							break;
						default:	// TermC_*Var
							r = cListVar(&inpCtrl, pBuf, cvList, cvListSize);
						}
					popup = true;

					// Check status, delete the pop-up buffer, and reprompt the user.
					if(r != Success || bpop(pBuf, RendShift) != Success || bdelete(pBuf, 0) != Success ||
					 mli_reset(&inpCtrl) != Success)
						return sess.rtn.status;
					continue;
					}
				}
			}

		// Non-special or quoted character.
		quoteNext = false;

		// If it is a (quoted) function key, ignore it and beep the beeper.
		if(extKey & (FKey | Shift)) {
BadKey:
			tbeep();
			continue;
			}

		// Plain character... insert it into the buffer.
		else {
			if(mli_putc(ektoc(extKey, false), &inpCtrl, true) != Success || mlflush() != Success)
				return sess.rtn.status;
			continue;
			}

		// Finished processing one key... flush tty output if echoing.
		if(!(inpCtrl.flags & Term_NoKeyEcho))
			if(mlflush() != Success)
				return sess.rtn.status;
		}

	// Terminal input completed.  Convert input buffer to a string and return.
	if(mli_close(&inpCtrl, &pDatum) == Success)
		dxfer(pRtnVal, pDatum);
	return sess.rtn.status;
	}

// Ask a yes or no question on the message line and set *result to the answer (true or false).  Used any time a confirmation is
// required.  Return status, including UserAbort, if user pressed the abort key.
static int termInpQ(const char *basePrompt, uint flags, bool *result) {
	Datum *pDatum;
	TermInpCtrl termInpCtrl = {"n", RtnKey, 0, NULL};
	char prompt[strlen(basePrompt) + strlen(text162) + 1];
					// " (y/n)?"
	// Build prompt.
	sprintf(prompt, "%s%s", basePrompt, text162);

	// Prompt user, get the response, and check it.
	if((pDatum = dnewtrack()) == NULL)
		return libfail();
	for(;;) {
		if(termInp(pDatum, prompt, 0, flags, &termInpCtrl) != Success)
			return sess.rtn.status;
		if(dgetint(pDatum) == 'y') {
			*result = true;
			break;
			}
		if(dgetint(pDatum) == 'n') {
			*result = false;
			break;
			}
		tbeep();
		}

	return sess.rtn.status;
	}

// Ask a yes or no question on message line.
int termInpYN(const char *basePrompt, bool *result) {

	return termInpQ(basePrompt, Term_OneChar, result);
	}

// Ask a yes or no question on message line with terminal attributes enabled in prompt.
int termInpAttrYN(const char *basePrompt, bool *result) {

	return termInpQ(basePrompt, Term_OneChar | Term_Attr, result);
	}

// Get a required numeric argument for a command (via prompt if interactive) and save in pRtnVal as an integer.  Return status.
// If script mode, prompt may be NULL.  If interactive and nothing was entered, pRtnVal will be nil.
int getNArg(Datum *pRtnVal, const char *prompt) {

	if(sess.opFlags & OpScript) {
		if(funcArg(pRtnVal, ArgFirst | ArgInt1) != Success)
			return sess.rtn.status;
		}
	else if(termInp(pRtnVal, prompt, ArgNil1, 0, NULL) != Success || disnil(pRtnVal))
		return sess.rtn.status;

	// Convert to integer.
	return toInt(pRtnVal, 10);
	}

// Get a completion from the user for a command, function, or alias name, given prompt, selector mask, result pointer, and "not
// found" error item.
int getCFA(const char *prompt, uint selector, ExecPtr *pExec, const char *errItem) {
	Datum *pDatum;

	if((pDatum = dnewtrack()) == NULL)
		return libfail();
	if(sess.opFlags & OpScript) {
		if(funcArg(pDatum, ArgFirst | ArgNotNull1) != Success)
			return sess.rtn.status;
		}
	else if(termInp(pDatum, prompt, ArgNotNull1 | ArgNil1, TermC_CFA | selector, NULL) != Success || mlerase(0) != Success)
		return sess.rtn.status;

	if(!disnil(pDatum)) {

		// Get pointer object for name and return status.
		return execFind(pDatum->str, OpQuery, selector, pExec) ? sess.rtn.status :
		 rsset(Failure, 0, text52, errItem, pDatum->str);
				// "No such %s \"%s\""
		}

	// User cancelled (no error).
	pExec->type = PtrNull;
	pExec->u.ptr = NULL;

	return sess.rtn.status;
	}

// Get a buffer name, given result pointer, prompt prefix, operation flags (which may contain ArgFirst or TermC_NoAuto),
// buffer-return pointer, and "buffer was created" pointer (which may be NULL).  "created" is passed to bfind().  If OpCurBName
// flag set, current buffer name is used as a default; if OpDefBName flag set, bdefault() is called instead.  If interactive,
// TermC_NoAuto flag is passed to termInp() and, if nothing is entered, *ppBuf is set to NULL.  If script mode and OpHaveBName
// flag set, funcArg() call is skipped.  If OpCreate flag is set, home new buffer to current screen.  It is assumed that the
// OpQuery flag will never be set (always OpDelete or OpCreate).  Return status.
int getBufname(Datum *pRtnVal, const char *prompt, uint flags, Buffer **ppBuf, bool *created) {

	// Get buffer name.
	if(!(sess.opFlags & OpScript)) {
		Buffer *pBuf;
		const char *defName = NULL;

		if(flags & OpCurBName)
			defName = sess.cur.pBuf->bufname;
		else if(flags & OpDefBName) {
			if((pBuf = bdefault()) != NULL)
				defName = pBuf->bufname;
			}
		TermInpCtrl termInpCtrl = {defName, RtnKey, MaxBufname, NULL};
		char promptBuf[WorkBufSize];
		sprintf(promptBuf, "%s %s", prompt, text83);
						// "buffer"
		if(termInp(pRtnVal, promptBuf, ArgNotNull1 | ArgNil1, TermC_Buffer | (flags & TermC_NoAuto),
		 &termInpCtrl) != Success)
			return sess.rtn.status;
		if(disnil(pRtnVal)) {
			*ppBuf = NULL;
			goto Retn;
			}
		}
	else if(!(flags & OpHaveBName))
		if(funcArg(pRtnVal, (flags & ArgFirst) | ArgNotNull1) != Success)
			return sess.rtn.status;

	// Have buffer name in pRtnVal.  Find it or create it.  Note that if OpCreate flag not set, returned status is Boolean.
	int status = bfind(pRtnVal->str, flags & OpCreate ? (flags & OpConfirm ? BN_Confirm | BN_Create | BN_CreateHook :
	 BN_Create | BN_CreateHook) : BS_Query, 0, ppBuf, created);
	if(!(flags & OpCreate) && !status) {

		// Buffer not found.  Return error.
		return rsset(Failure, 0, text52, text83, pRtnVal->str);
				// "No such %s \"%s\"", "buffer"
		}
	if(flags & OpCreate && status == Success)
		(*ppBuf)->homeDir = sess.pCurScrn->workDir;
Retn:
	if(!(sess.opFlags & OpScript))
		(void) mlerase(0);
	return sess.rtn.status;
	}

// Prompt options.
#define Prm_Type		0
#define Prm_Delim		1
#define Prm_TermAttr		2
#define Prm_NoAuto		3
#define Prm_NoEcho		4

// Prompt option types.
#define PTypBuffer		0
#define PTypChar		1
#define PTypConstVar		2
#define PTypDelRing		3
#define PTypFile		4
#define PTypKey			5
#define PTypKeySeq		6
#define PTypKillRing		7
#define PTypMacro		8
#define PTypMode		9
#define PTypMutableVar		10
#define PTypReplaceRing		11
#define PTypRingName		12
#define PTypSearchRing		13
#define PTypString		14

// binSearch() helper function for returning an option keyword, given table (array) and index.
static const char *optKeyword(const void *table, ssize_t i) {

	return ((const char **) table)[i];
	}

// Process "prompt" function.  Save result in pRtnVal and return status.
int userPrompt(Datum *pRtnVal, long n, Datum **args) {
	Datum *pOptStr;				// Unmodified option string (for error reporting).
	Datum *pPrompt, *defVal;
	char *str0, *str1, *str2, *option;
	const char **pKeyword;
	ssize_t i;
	uint id;
	uint typeFlags = 0;			// Prompt type flags.
	uint ctrlFlags = 0;			// Other control flags.
	bool delimAllowed = true;		// "Delim" option allowed (not a completion prompt type)?
	TermInpCtrl termInpCtrl = {NULL, RtnKey, 0, NULL};	// Default value, delimiter, maximum input length, Ring pointer.
	static const char *keywordList[] = {"Type", "Delim", "TermAttr", "NoAuto", "NoEcho", NULL};
	static const char *types[] = {"Buffer", "Char", "ConstVar", "DelRing", "File", "Key", "KeySeq", "KillRing", "Macro",
	 "Mode", "MutableVar", "ReplaceRing", "RingName", "SearchRing", "String"};

	if((pOptStr = dnewtrack()) == NULL)
		goto LibFail;

	// Set pPrompt to prompt string (which may be NULL or nil).
	if((pPrompt = *args) != NULL)
		++args;

	// Get options if given, which are strings of form "keyword[: value]".
	if(*args != NULL && !disnil(defVal = *args++)) {
		str0 = defVal->str;
		do {
			if(*(str0 = strip(str0, -1)) == '\0') {			// Blank?
				dsetnull(pOptStr);				// Yes, error.
				goto ErrRtn;
				}
			if(strncasecmp(str0, "Delim:", 6) == 0) {		// Check for "Delim" special case.
				option = str0;
				str0 = NULL;
				pKeyword = keywordList + 1;
				}
			else {
				option = strparse(&str0, ',');			// Can't fail, but option may be null.
				pKeyword = NULL;
				}
			if(dsetstr((str1 = option), pOptStr) != 0)		// Save option string for error reporting.
				goto LibFail;
			if((str2 = strparse(&str1, ':')) == NULL)		// Get option name.
				goto ErrRtn;
			if(pKeyword == NULL) {
				pKeyword = keywordList;				// Valid?
				do {
					if(strcasecmp(str2, *pKeyword) == 0)
						goto FoundKW;
					} while(*++pKeyword != NULL);
				goto ErrRtn;					// Nope... unknown.
				}
FoundKW:
			if((id = pKeyword - keywordList) <= 1)			// Yes, get option value, if applicable.
				if(str1 == NULL || *(str2 = strip(str1, 0)) == '\0')
					goto ErrRtn;
			switch(id) {						// Process option.
				case Prm_Type:
					if(!binSearch(str2, (const void *) types, elementsof(types), strcasecmp,
					 optKeyword, &i))
						return rsset(Failure, 0, text295, str2);
							// "Unknown prompt type '%s'"

					// Have valid type... process it.
					switch(i) {
						case PTypChar:
							typeFlags = Term_OneChar;
							break;
						case PTypString:
							break;
						case PTypBuffer:
							typeFlags = TermC_Buffer;
							termInpCtrl.maxLen = MaxBufname;
							goto PromptComp;
						case PTypFile:
							typeFlags = TermC_Filename;
							termInpCtrl.maxLen = MaxPathname;
							goto PromptComp;
						case PTypKey:
							typeFlags = Term_OneKey;
							break;
						case PTypKeySeq:
							typeFlags = Term_OneKeySeq;
							break;
						case PTypKillRing:
							termInpCtrl.pRing = ringTable + RingIdxKill;
							break;
						case PTypMacro:
							typeFlags = TermC_Macro;
							termInpCtrl.maxLen = MaxMacroName;
							goto PromptComp;;
						case PTypMode:
							typeFlags = TermC_Mode;
							goto PromptComp;
						case PTypReplaceRing:
							termInpCtrl.pRing = ringTable + RingIdxRepl;
							break;
						case PTypRingName:
							typeFlags = TermC_Ring;
							goto PromptComp;
						case PTypSearchRing:
							termInpCtrl.pRing = ringTable + RingIdxSearch;
							break;
						case PTypDelRing:
							termInpCtrl.pRing = ringTable + RingIdxDel;
							break;
						case PTypMutableVar:
							typeFlags = TermC_MutVar;
							goto PromptVar;
						default:	// PTypConstVar
							typeFlags = TermC_AllConstVar;
PromptVar:
							termInpCtrl.maxLen = MaxVarName;
PromptComp:
							// Using completion... note it.
							delimAllowed = false;
						}
					break;
				case Prm_Delim:
					if(stoek(&termInpCtrl.delimKey, str2) != Success)
						return sess.rtn.status;
					if(termInpCtrl.delimKey & Prefix) {
						char workBuf[16];

						return rsset(Failure, RSTermAttr, text341,
							// "Cannot use key sequence ~#u%s~U as %s delimiter"
						 ektos(workBuf, termInpCtrl.delimKey, true), text342);
											// "prompt"
						}
					break;
				case Prm_TermAttr:
					ctrlFlags |= Term_Attr;
					break;
				case Prm_NoAuto:
					ctrlFlags |= TermC_NoAuto;
					break;
				default:	// Prm_NoEcho
					ctrlFlags |= Term_NoKeyEcho;
				}
			} while(str0 != NULL);
		}

	// Validate and prepare default value, if any.
	if((defVal = *args) != NULL && !disnil(defVal)) {
		if(typeFlags == Term_OneChar) {
			if(!isCharVal(defVal))
				return sess.rtn.status;
			dconvchr(dgetint(defVal), defVal);
			}
		else if(toStr(defVal) != Success)
			return sess.rtn.status;
		termInpCtrl.defVal = defVal->str;
		}

	// Do final validation checks.
	if((termInpCtrl.delimKey != RtnKey && !delimAllowed) ||
	 ((typeFlags & Term_KeyMask) >= Term_OneKey && (termInpCtrl.defVal != NULL || termInpCtrl.delimKey != RtnKey)))
		return rsset(Failure, RSNoFormat, text242);
			// "Option(s) conflict with prompt type"

	// All is well... prompt for input.
	return termInp(pRtnVal, pPrompt == NULL || disnil(pPrompt) ? "" : pPrompt->str, ArgNil1, typeFlags | ctrlFlags,
	 &termInpCtrl);
ErrRtn:
	return rsset(Failure, 0, text447, text457, pOptStr->str);
		// "Invalid %s '%s'", "prompt option"
LibFail:
	return libfail();
	}
