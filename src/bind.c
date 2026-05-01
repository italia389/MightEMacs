// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// bind.c	Key binding routines for MightEMacs.

#include "std.h"
#include <stdarg.h>
#include "exec.h"
#include "search.h"

// Make selected global definitions local.
#define BindData
#include "bind.h"

/*** Local declarations ***/

// Flags for macActive() function.
#define MA_Required	0x0001		// Error if macro not active.
#define MA_Active	0x0002		// Use "not active" error, otherwise "not defined".

// Recognized keywords in key binding literals.
typedef struct {
	char *keyword;		// Keyword.
	ExtKey extKey;		// Extended key.
	} KeyLit;
static KeyLit keyLiterals[] = {
	{"SPC", ' '},
	{"TAB", Ctrl | 'I'},
	{"ESC", Ctrl | '['},
	{"RTN", Ctrl | 'M'},
	{"DEL", Ctrl | '?'},
	{NULL, '\0'}};

// Convert raw 7-bit character to an extended key.
ExtKey ctoek(short c) {
	ExtKey extKey = c;

	if(extKey == '\0')
		extKey = Ctrl | ' ';
	else if(extKey <= 0x1f || extKey == 0x7f)
		extKey = Ctrl | (extKey ^ 0x40);
	return extKey;
	}

// Convert extended key to ordinal character value.  Collapse the Ctrl flag back into the ASCII code.  If "extend" is true,
// return S-TAB as 128, followed by:
//	SpecFKeyCount special function keys (FNB, FNC, etc.)
//	NumFKeyCount numbered function keys (FN1..FN9, FNa, etc.)
//	Shifted special function keys
//	Shifted numbered function keys.
// Otherwise 0..127 like other characters.
short ektoc(ExtKey extKey, bool extend) {

	// Do special cases first.
	if((extKey & ~0x7f) == 0x80)
		return extKey;					// Raw 8-bit character.
	if((extKey & (Ctrl | 0xff)) == (Ctrl | ' '))
		return 0;					// Null character.
	if((extKey & (Shift | Ctrl | 0xff)) == (Shift | Ctrl | 'I'))
		return 128;					// S-TAB.

	// Now do control keys and function keys.
	short c = extKey & 0x7f;
	if(extKey & Ctrl)
		return c ^ 0x40;				// Actual control character.
	if(extKey & FKey && extend) {				// FNx character offset in fKeySyms.
		char *str = strchr(fKeySyms, c);		// Can't fail.
		c = extKey & Shift ? 128 + 1 + SpecFKeyCount + NumFKeyCount + (str - fKeySyms) : 128 + 1 + (str - fKeySyms);
		}
	return c;
	}


// Return the number of entries in the binding table that match the given executable pointer.
static int ptrEntryCount(ExecPtr *pExec) {
	int count = 0;
	KeyBind *pKeyBind = (KeyBind *) keyBindTable;

	// Search for existing bindings for the command.
	do {
		if(pKeyBind->cmd.u.ptr == pExec->u.ptr)
			++count;
		} while(++pKeyBind < (KeyBind *)(keyBindTable + KeyVectCount));

	return count;
	}

// Scan the binding table for the first entry whose pointer member matches the given executable pointer and return it, or NULL
// if none found.
KeyBind *findBindPtr(ExecPtr *pExec) {
	KeyBind *pKeyBind = (KeyBind *) keyBindTable;

	// Search for an existing binding for the command.
	do {
		if(pKeyBind->cmd.u.ptr == pExec->u.ptr)
			return pKeyBind;
		} while(++pKeyBind < (KeyBind *)(keyBindTable + KeyVectCount));

	return NULL;
	}

// Return binding slot for given extended key.
KeyBind *bindSlot(ExtKey extKey) {
	int i;

	// Set i to target key vector.
	switch(extKey & Prefix) {
		case 0:
			i = 0;
			break;
		case Meta:
			i = 1;
			break;
		case Pref1:
			i = 2;
			break;
		case Pref2:
			i = 3;
			break;
		default:
			i = 4;
		}

	return (KeyBind *)(keyBindTable + i) + ektoc(extKey, true);
	}

// Look up a key binding in the binding table, given extended key, and return it, or NULL if not bound.
KeyBind *getBind(ExtKey extKey) {
	KeyBind *pKeyBind = bindSlot(extKey);
	return pKeyBind->cmd.u.ptr == NULL ? NULL : pKeyBind;
	}

// Add an extended key to the binding table.
static void newBind(ExtKey extKey, ExecPtr *pExec) {
	KeyBind *pKeyBind = bindSlot(extKey);

	// Set the command or buffer pointer.
#if MMDebug & Debug_KeyCode
	if(extKey != pKeyBind->extKey)
		fprintf(logfile, "newBind(): ERROR: Adding extKey %.4hx at slot with extKey %.4hx\n", extKey, pKeyBind->extKey);
#endif
	pKeyBind->cmd = *pExec;
	}

// Get binding of given extended key and return prefix flag if it's bound to a prefix command, otherwise zero.
static ExtKey findPrefix(ExtKey extKey) {
	KeyBind *pKeyBind = getBind(extKey);
	if(pKeyBind != NULL && pKeyBind->cmd.type == PtrPseudo) {
		CmdFunc *pCmdFunc = pKeyBind->cmd.u.pCmdFunc;
		if(pCmdFunc->attrFlags & CFPrefix) {
			CmdFuncId id = (CmdFuncId) (pCmdFunc - cmdFuncTable);
			return id == cf_metaPrefix ? Meta : id == cf_prefix1 ? Pref1 : id == cf_prefix2 ? Pref2 : Pref3;
			}
		}
	return 0;
	}

// Get name of command from given binding object and return it, or NULL if pKeyBind is NULL or key not bound.
const char *getBindCmd(KeyBind *pKeyBind) {

	return pKeyBind == NULL ? NULL : pKeyBind->cmd.u.ptr != NULL ?
	 (pKeyBind->cmd.type & PtrUserCmd ? pKeyBind->cmd.u.pBuf->bufname : pKeyBind->cmd.u.pCmdFunc->name) :
	 pKeyBind->extKey >= 0x20 && pKeyBind->extKey < 0xff ? text383 : NULL;
							// "(self insert)"
	}

// Get one value from the coded string in *pKeyCode.  Store result in *pExtKey, update *pKeyCode, and return true if successful,
// otherwise false.
static bool stoek1(ExtKey *pExtKey, const char **pKeyCode) {
	ExtKey c;
	ExtKey extKey = 0;
	KeyLit *pKeyLit;
	char *str;
	const char *keyCode = *pKeyCode;

	// Loop until hit null or space.
	for(;;) {
		if(*keyCode == '\0')
			return false;

		// Prefix?
		if(keyCode[1] == '-') {
			switch(keyCode[0]) {
				case 'C':			// Ctrl prefix.
				case 'c':
					++keyCode;
					goto CtrlPref;
				case 'M':			// Meta prefix.
				case 'm':
					if(extKey & Meta)
						return false;
					extKey |= Meta;
					break;
				case 'S':			// SHIFT prefix.
				case 's':
					if(extKey & Shift)
						return false;
					extKey |= Shift;
					break;
				default:
					return false;
				}
			keyCode += 2;
			}

		// Alternate control character form?
		else if((c = keyCode[0]) == '^') {
			if(keyCode[1] == '\0' || keyCode[1] == ' ')	// Bare '^'?
				goto Vanilla;			// Yes, take it literally
CtrlPref:
			if(extKey & Ctrl)
				return false;
			extKey |= Ctrl;
			++keyCode;
			}

		// Function key?
		else if(strncasecmp(keyCode, "fn", 2) == 0) {
			if(extKey & FKey)
				return false;
			extKey |= FKey;
			keyCode += 2;
			}

		// Space or keyword?
		else {
			if(c == ' ')
				return false;
			pKeyLit = keyLiterals;
			do {
				if(strncasecmp(keyCode, pKeyLit->keyword, 3) == 0) {
					keyCode += 2;
					c = pKeyLit->extKey & 0xff;
					extKey |= pKeyLit->extKey & ~0xff;
					goto Vanilla;
					}
				} while((++pKeyLit)->keyword != NULL);

			// Not a keyword.  Literal control character? (Boo, hiss.)
			if(c < ' ' || c == 0x7f) {
				if(extKey & Ctrl)		// Duplicate?
					return false;		// Yes, error.
				extKey |= Ctrl;
				c ^= '@';			// Convert literal character to visible equivalent.
				++keyCode;
				}

			// Must be a vanilla character; that is, a printable (except space) or 8-bit character.  Move past it
			// and finalize value.
			else
Vanilla:
				++keyCode;

			// Character is in c and prefix flag(s) are in extKey (if any).  Do sanity checks.
			if(*keyCode != '\0' && *keyCode != ' ')		// Not end of value?
				return false;				// Yes, error.
			if(extKey != (Ctrl | Shift) ||
			 (c != 'i' && c != 'I')) {			// Skip S-TAB special case.
				if(extKey & Ctrl) {
					if(extKey & Shift)		// Error if S-C-.
						return false;
					if(c == '@')			// C-@ or ^@ ?
						c = ' ';		// Yes, change back to a space.
					else if(c != ' ' && (c < '?' || c == '`' || c > 'z'))
						return false;		// Invalid character following C- or ^.
					}
				if(extKey & FKey && ((str = strchr(fKeySyms, c)) == NULL ||
				 (ulong)(str - fKeySyms) >= SpecFKeyCount + NumFKeyCount))
					return false;			// Error if unknown FKey symbol.
				if((extKey & (FKey | Shift)) == Shift) { // If SHIFT prefix without FNx...
					if(is_letter(c)) {		// error if not a letter.
						c = upCase[c];
						extKey &= ~Shift;
						}
					else
						return false;
					}
				}

			// Make sure it's upper case if used with C- or ^.
			if((extKey & (FKey | Ctrl)) == Ctrl)
				c = upCase[c];

			// Success.  Return results.
			*pKeyCode = keyCode;
			*pExtKey = extKey | c;
			break;
			}
		}
	return true;
	}

// Convert a coded string to an extended key code.  Set *pExtKey to zero if keyLit is invalid.  Return status.
//
// A coded key binding consists of one or two space-separated value(s).  Each value consists of zero or more prefixes
// followed by a character (other than space) or literal.  Recognized prefixes are:
//	M-	Meta prefix.
//	C-	Ctrl prefix.
//	^	Alternate Ctrl prefix.
//	S-	SHIFT prefix (for function key or character).
//	FN	Function prefix (which includes function keys and special keys like Delete and Up Arrow).
// All prefixes are case-insensitive.  Characters can be real control characters, printable characters, or any of the
// following case-insensitive literals:
//	DEL	Backspace key.
//	ESC	Escape key.
//	RTN	Return key.
//	SPC	Spacebar.
//	TAB	Tab key.
// The M- prefix is valid only on the first value, and all literals except ESC are valid only on the last value.
int stoek(ExtKey *pExtKey, const char *keyCode) {
	ExtKey extKey1;				// One key.
	ExtKey extKey = 0;			// Final extended key.
	ushort keyCount = 0;
	const char *keyCode1 = keyCode;

	// Parse it up.
	for(;;) {
		// Get a value into extKey1.
		if(!stoek1(&extKey1, &keyCode1))
			goto BadCode;
		keyCount += extKey1 & Meta ? 2 : 1;

		// Success.  Done?
		if(*keyCode1 == ' ') {
			if(keyCount == 2)
				goto BadCode;

			// Have first of two values.  If not a prefix key, return an error; otherwise, set flag.
			ExtKey flag = findPrefix(extKey1);
			if(flag == 0)
				goto BadCode;
			extKey = flag;
			++keyCode1;
			}
		else {
			if(*keyCode1 != '\0') {
BadCode:
				*pExtKey = 0;
				return rsset(Failure, 0, text447, text254, keyCode);
						// "Invalid %s '%s'", "key literal"
				}
			if(keyCount == 1)
				extKey = extKey1;
			else
				extKey |= extKey1;
			break;
			}			
		}

	*pExtKey = extKey;
	return sess.rtn.status;
	}

// Search key literal table for extended key with matching prefix (Ctrl or none).  Store literal in *pLit and return true if
// found; otherwise, return false.
static bool ektolit(char **pLit, ExtKey extKey, ExtKey prefix) {
	KeyLit *pKeyLit = keyLiterals;

	extKey &= prefix | 0xff;
	do {
		if(pKeyLit->extKey == extKey) {
			*pLit = stpcpy(*pLit, pKeyLit->keyword);
			return true;
			}
		if(prefix == 0)
			break;
		} while((++pKeyLit)->keyword != NULL);
	return false;
	}

// Print character from an extended key to 'dest' and return pointer to trailing null.  Handle Ctrl and FKey flags.  Tilde (~)
// characters (AttrSpecBegin) of terminal attribute sequences are escaped if escTermAttr is true.
static char *ektos1(char *dest, ExtKey extKey, bool escTermAttr) {
	ExtKey c;

	// Function key?
	if(extKey & FKey) {
		dest = stpcpy(dest, "FN");
		*dest++ = extKey & 0xff;
		goto RetnChk;
		}

	// Print the character using the "control" literals in keyLiterals, if possible.
	if(ektolit(&dest, extKey, Ctrl))
		goto Retn;

	// No literal found.  Control key?
	if(extKey & Ctrl)
		dest = stpcpy(dest, "C-");

	// Print the character using the "non-control" literals in keyLiterals, if possible.
	if(ektolit(&dest, extKey, 0))
		goto Retn;

	// Print raw character, in encoded form if 8-bit.
	if((c = extKey & 0xff) & 0x80) {
		sprintf(dest, "<%.2hX>", c);
		dest = strchr(dest, '\0');
		}
	else {
		*dest++ = extKey & Ctrl ? lowCase[c] : c;
RetnChk:
		if(escTermAttr && dest[-1] == AttrSpecBegin)
			*dest++ = AttrSpecBegin;
Retn:
		*dest = '\0';
		}

	return dest;
	}

// Encode an extended key to a printable string, save result in 'dest', and return it.  Tilde (~) characters (AttrSpecBegin) of
// terminal attribute sequences are escaped if 'escTermAttr' is true.
char *ektos(char *dest, ExtKey extKey, bool escTermAttr) {
	char *str;
	static struct PrefKey {
		ExtKey code;
		CmdFuncId id;
		ExtKey flag;
		} prefKeyTable[] = {
			{Ctrl | '[', cf_metaPrefix, Meta},
			{Ctrl | 'X', cf_prefix1, Pref1},
			{Ctrl | 'C', cf_prefix2, Pref2},
			{Ctrl | 'H', cf_prefix3, Pref3}};

	// Do the prefix keys first, giving preference to the default values (^[, ^X, ^C, and ^H) in case multiple keys are
	// bound to the same prefix.
	str = dest;
	if(extKey & Prefix) {
		ExtKey extKey2;
		struct PrefKey *pPrefKey, *pPrefKeyEnd;
		pPrefKeyEnd = (pPrefKey = prefKeyTable) + elementsof(prefKeyTable);

		do {
			if(extKey & pPrefKey->flag) {
				if(findPrefix(pPrefKey->code) == pPrefKey->flag) {
					extKey2 = pPrefKey->code;
					goto Print1;
					}
				break;
				}
			} while(++pPrefKey < pPrefKeyEnd);

		// Default prefix key binding not found.  Find first binding in table instead.
		ExecPtr exec = {PtrNull};	// Pointer type not used.
		pPrefKey = prefKeyTable;
		for(;;) {
			if(extKey & pPrefKey->flag) {
				exec.u.pCmdFunc = cmdFuncTable + pPrefKey->id;
				extKey2 = findBindPtr(&exec)->extKey;
Print1:
				str = ektos1(str, extKey2, escTermAttr);
				*str++ = ' ';
				break;
				}
			++pPrefKey;
			}
		}

	// Print any shift prefix literal.
	if(extKey & Shift)
		str = stpcpy(str, "S-");

	// Print the base character and return result.
	ektos1(str, extKey, escTermAttr);
	return dest;
	}

#if MMDebug & Debug_Bind
// Dump binding table.
static void dumpBind(const char *label, bool all) {
	KeyVect *pKeyVect = keyBindTable;
	KeyBind *pKeyBind;
	uint i, slot;
	char keyBuf[16];

	fprintf(logfile, "----------\nBINDING TABLE DUMP - %s\nVector slot count: 128 + 1 + %lu * 2 + %d * 2 = %lu\n", label,
	 SpecFKeyCount, NumFKeyCount, KeyVectSlots);
	i = slot = 0;
	do {
		fprintf(logfile, "\nBINDING LIST #%u\n", i++);
		pKeyBind = (KeyBind *) pKeyVect;
		do {
			if(all || pKeyBind->cmd.u.ptr != NULL)
				fprintf(logfile, "%.4u  %12s   ->   %.4hX  %s\n", slot, ektos(keyBuf, pKeyBind->extKey, false),
				 pKeyBind->cmd.type, pKeyBind->cmd.u.ptr == NULL ? "  -" :
				 pKeyBind->cmd.type & PtrSysCmdPseudo ? pKeyBind->cmd.u.pCmdFunc->name :
				 pKeyBind->cmd.u.pBuf->bufname);
			++slot;
			} while(++pKeyBind < (KeyBind *) pKeyVect + KeyVectSlots);
		} while(++pKeyVect < keyBindTable + KeyVectCount);
	}
#endif

// Clear extended key from key cache, if present.
static void clearCache(ExtKey extKey) {
	uint i = 0;

	do {
		if(coreKeys[i].extKey == extKey) {
			coreKeys[i].extKey = 0;
			break;
			}
		} while(++i < CoreKeyCount);
	}

// Clear given key entry in the binding table.
void unbind(KeyBind *pKeyBind) {

	// Remove key from cache.
	clearCache(pKeyBind->extKey);

	// Clear the entry.
	pKeyBind->cmd.u.ptr = NULL;
	pKeyBind->cmd.type = PtrNull;
	}

// Initialize the binding table and load all the built-in command bindings.  Return status.
int initBindings(void) {
	ExecPtr exec;
	KeyBind *pKeyBind;
	KeyVect *pKeyVect;
	BuiltinKey *pBuiltinKey;
	const char *fnSym;
	ushort i;
	ExtKey prefix, extKey;

	// Set extended key value in all table entries.
	prefix = 0;
	pKeyVect = keyBindTable;
	do {
		pKeyBind = (KeyBind *) pKeyVect;
		i = 0;
		do {
			if(i < 128)
				extKey = prefix | ctoek(i);
			else if(i == 128)
				extKey = prefix | Shift | Ctrl | 'I';
			else {
				// Reset FN char pointer if at first FNx slot or S-FNx slot.
				if(i == 128 + 1 || i == 128 + 1 + SpecFKeyCount + NumFKeyCount)
					fnSym = fKeySyms;
				extKey = prefix | FKey | *fnSym++;
				if(i >= 128 + 1 + SpecFKeyCount + NumFKeyCount)
					extKey |= Shift;
				}
			pKeyBind->extKey = extKey;
			++i;
			} while(++pKeyBind < (KeyBind *) pKeyVect + KeyVectSlots);
		if(prefix == 0)
			prefix = Meta;
		else
			prefix <<= 1;
		} while(++pKeyVect < keyBindTable + KeyVectCount);

	// Load all built-in command bindings.
	pBuiltinKey = builtinKeys;
	do {
		exec.u.pCmdFunc = pBuiltinKey->pCmdFunc;
		exec.type = pBuiltinKey->pCmdFunc->attrFlags & CFHidden ? PtrPseudo : PtrSysCmd;
		newBind(pBuiltinKey->extKey, &exec);
		} while((++pBuiltinKey)->extKey != 0);

	return sess.rtn.status;
	}

#if 0
// Dump execution table to log file.
static void dumpExec(void) {
	ExecPtr *pExec;
	HashNode **ppHashNode0, **ppHashNodeEnd;
	HashNode **ppHashNode = NULL;
	fputs("\nEXEC TABLE\n", logfile);
	(void) hsort(&execTable, hcmp, &ppHashNode);
	ppHashNodeEnd = (ppHashNode0 = ppHashNode) + execTable.nodeCount;
	do {
		pExec = execPtr(*ppHashNode);
		fprintf(logfile, "%-20s%.4hx\n", (*ppHashNode)->key, pExec->type);
		} while(++ppHashNode < ppHashNodeEnd);
	free((void *) ppHashNode0);
	}
#endif

// Get command or function name per selector flags and return status.  Store pointer in *pExec (which may be NULL if interactive
// and user cancelled).  If interactive mode, pass prompt to getCFA().  Called by:
//	bindKey(..., PtrSysCmdPseudo | PtrUserCmd)
//	setHook(..., PtrSysFunc | PtrUserFunc)
//	alias(..., PtrSysCmdFunc | PtrUserCmdFunc)
int getCF(const char *prompt, ExecPtr *pExec, uint selector) {
	const char *errItem = selector & PtrPseudo ? text158 : selector & PtrSysCmd ? text313 : text247;
						// "command"			"command or function", "function"
	if(sess.opFlags & OpScript) {
		if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
			return sess.rtn.status;
		if(!(sess.opFlags & OpEval)) {
			pExec->type = PtrNull;
			pExec->u.ptr = NULL;
			}
		else if(!execFind(pLastParse->tok.str, OpQuery, selector, pExec))
			return rsset(Failure, 0, text52, errItem, pLastParse->tok.str);
					// "No such %s \"%s\""
		(void) getSym();
		}
	else
		(void) getCFA(prompt, selector | Term_Attr, pExec, errItem);
	return sess.rtn.status;
	}

// Get a key binding (using given prompt if interactive) and save in *pExtKey.  If n <= 0, get one key only; otherwise, get a
// key sequence.  Return status.
int getKeyBind(const char *prompt, long n, Datum **args, ExtKey *pExtKey) {

	// Script mode?
	if(sess.opFlags & OpScript) {

		// Yes, process argument.
		if(sess.opFlags & OpEval)
			(void) stoek(pExtKey, args[0]->str);
		}
	else {
		// No, get key from the keyboard.
		if(mlputs(prompt, MLHome | MLFlush) == Success)
			(void) ((n != DefN && n <= 0) ? getkey(true, pExtKey, false) : getKeySeq(true, pExtKey, NULL, false));
		}

	return sess.rtn.status;
	}

// Bind a key sequence to a command.  Get single key interactively if n <= 0.  Return status.
int bindKey(Datum *pRtnVal, long n, Datum **args) {
	ExtKey extKey;			// Key to bind.
	ExecPtr exec;			// Pointer to the requested command.
	KeyBind *pKey, *pCmd;
	char keyBuf[16];
	char workBuf[WorkBufSize];

	// Get the key or key sequence to bind.
	if(getKeyBind(text15, n, args, &extKey) != Success)
			// "Bind key "
		return sess.rtn.status;
	if(!(sess.opFlags & OpScript) || sess.opFlags & OpEval)
		ektos(keyBuf, extKey, true);

	// If interactive mode, build "progress" prompt.
	if(!(sess.opFlags & OpScript))
		sprintf(workBuf, "%s~#u%s~U %s %s", text15, keyBuf, text339, text158);
						// "Bind key "		"to", "command"
	else if(!needSym(s_comma, true))
		goto Retn;

	// Get the command name.  If script mode and not evaluating (null exec type), bail out here.
	if(getCF(workBuf, &exec, PtrSysCmdPseudo | PtrUserCmd) != Success || exec.type == PtrNull)
		goto Retn;

	// Binding a key sequence to a single-key command?
	if(extKey & Prefix && exec.type & PtrSysCmdPseudo && exec.u.pCmdFunc->attrFlags & CFBind1)
		return rsset(Failure, RSTermAttr, text17, keyBuf, exec.u.pCmdFunc->name);
			// "Cannot bind ~#u%s~U to ~b%s~B command -- must be bound to a single key"

	// Search the binding table to see if the key exists.
	if((pKey = getBind(extKey)) != NULL) {

		// If the key is already bound to this command or function, it's a no op.
		if(pKey->cmd.u.ptr == exec.u.ptr)
			goto Retn;

		// If the key is bound to a permanent-bind command and the only such binding, it can't be reassigned.
		if(pKey->cmd.type & PtrSysCmdPseudo && pKey->cmd.u.pCmdFunc->attrFlags & CFPerm &&
		 ptrEntryCount(&pKey->cmd) < 2)
			return rsset(Failure, RSTermAttr, text210, keyBuf, pKey->cmd.u.pCmdFunc->name);
					// "~#u%s~U is only binding to core command ~b%s~B -- cannot delete or reassign"
		}

	// Remove key from cache.
	clearCache(extKey);

	// If binding to a command and the command is maintained in a global variable (for internal use), it can only have one
	// binding at most.
	if(exec.type & PtrSysCmdPseudo && exec.u.pCmdFunc->attrFlags & CFUniq) {
		uint i = 0;

		// Search for an existing binding for the command and remove it.
		if((pCmd = findBindPtr(&exec)) != NULL)
			unbind(pCmd);

		// Update the key cache.
		do {
			if(coreKeys[i].id == exec.u.pCmdFunc - cmdFuncTable) {
				coreKeys[i].extKey = extKey;
				break;
				}
			} while(++i < CoreKeyCount);
		}

	// Key already in binding table?
	if(pKey != NULL)

		// Yes, change it.
		pKey->cmd = exec;
	else
		// Not in table.  Add it.
		newBind(extKey, &exec);

#if MMDebug & Debug_Bind && 0
	fprintf(logfile, "bindKey(%s) DONE.\n", ektos(workBuf, extKey, false));
	dumpBind("After bindKey() Call", true);
#endif
	return rsset(Success, RSNoFormat, text224);
				// "Binding set"
Retn:
	return sess.rtn.status;
	}

// Delete a key from the binding table.  Get single keystroke if interactive and n <= 0.  Return Boolean result and, if script
// mode, ignore any "key not bound" error.
int unbindKey(Datum *pRtnVal, long n, Datum **args) {
	ExtKey extKey;
	KeyBind *pKeyBind;
	char keyBuf[16];

	// Get the key or key sequence to unbind.
	if(getKeyBind(text18, n, args, &extKey) != Success)
			// "Unbind key "
		return sess.rtn.status;

	// Change key to printable form.
	ektos(keyBuf, extKey, true);

	// Search the binding table to see if the key exists.
	if((pKeyBind = getBind(extKey)) == NULL) {

		// Not bound.  Notify user.
		(void) rsset(Success, RSNoWrap | RSTermAttr, text14, keyBuf);
			// "~#u%s~U not bound"
		}
	else {
		// Found it.  If the key is bound to a permanent-bind command and the only such binding, it can't be deleted.
		if(pKeyBind->cmd.type & PtrSysCmdPseudo && pKeyBind->cmd.u.pCmdFunc->attrFlags & CFPerm &&
		 ptrEntryCount(&pKeyBind->cmd) < 2)
			return rsset(Failure, RSTermAttr, text210, keyBuf, pKeyBind->cmd.u.pCmdFunc->name);
					// "~#u%s~U is only binding to core command ~b%s~B -- cannot delete or reassign"

		// It's a go... unbind it.
		unbind(pKeyBind);

		// Print key literal if interactive (following prompt string).
		if(!(sess.opFlags & OpScript))
			(void) mlprintf(MLTermAttr | MLFlush, "~#u%s~U", keyBuf);
		}

	// Return Boolean result.
	dsetbool(pKeyBind != NULL, pRtnVal);

	return sess.rtn.status;
	}


// Do "binding" or "bindingsof" function per haveLit argument and return status.  If haveLit is true (binding function), return
// name of command bound to given key binding, or nil if none.  If haveLit is false (bindingsof function), return array of key
// binding(s) bound to given command.
int binding(Datum *pRtnVal, Datum **args, bool haveLit) {

	// Check haveLit argument.
	if(haveLit) {
		const char *str;
		ExtKey extKey;

		// Process key literal ("binding" function).
		if(stoek(&extKey, args[0]->str) != Success)
			goto Retn;
		if((str = getBindCmd(getBind(extKey))) == NULL) {
			dsetnil(pRtnVal);
			goto Retn;
			}
		if(dsetstr(*str == BCmdFuncLead ? str + 1 : str, pRtnVal) != 0)
			goto LibFail;
		}

	// Process bare word command name ("bindingsof" function).
	else if(haveSym(s_ident, true)) {
		if(sess.opFlags & OpEval) {
			Array *pArray;
			Datum keyLit;
			ExecPtr exec;
			KeyBind *pKeyBind;
			char keyBuf[16];

			// Get the command.
			if(!execFind(pLastParse->tok.str, OpQuery, PtrSysCmdPseudo | PtrUserCmd, &exec))
				return rsset(Failure, 0, text52, text158, pLastParse->tok.str);
						// "No such %s \"%s\"", "command"

			// Get its key binding(s).
			if(makeArray(pRtnVal, 0, &pArray) != Success)
				goto Retn;
			dinit(&keyLit);
			pKeyBind = (KeyBind *) keyBindTable;
			do {
				if(pKeyBind->cmd.u.ptr == exec.u.ptr) {

					// Add the key sequence.
					if(dsetstr(ektos(keyBuf, pKeyBind->extKey, false), &keyLit) != 0 ||
					 apush(pArray, &keyLit, OpXfer) != 0)
LibFail:
						return libfail();
					}
				} while(++pKeyBind < (KeyBind *)(keyBindTable + KeyVectCount));
			}
		(void) getSym();
		}
Retn:
	return sess.rtn.status;
	}

// Create an entry in the execution table hash.  It is assumed that the key (name) does not already exist.
int execNew(const char *name, ExecPtr *pExec) {
	HashNode *pHashNode;

	return (pHashNode = hnewnode(&execTable, name, NULL)) == NULL ||
	 dsetmem((void *) pExec, sizeof(ExecPtr), pHashNode->pVal) != 0 ? libfail() : sess.rtn.status;
	}

// Find an executable name (command, function, or alias) in the execution table and return status or Boolean result.
// If the name is found:
//	If op is OpQuery:
//		Set pExec (if not NULL) to the object and return true if entry type matches selector; otherwise, return false.
//	If op is OpCreate:
//		Return status.
//	If op is OpUpdate:
//		Update entry with *pExec value and return status.
//	If op is OpDelete:
//		Delete the hash table entry and return status.
// If the name is not found:
//	If op is OpCreate:
//		Create an entry with the given name and *pExec value, and return status.
//	If op is OpQuery:
//		Return false.
//	If op is OpUpdate or OpDelete:
//		Return FatalError (should not happen).
int execFind(const char *name, uint op, uint selector, ExecPtr *pExec) {
	HashNode *pHashNode;
	char myName[] = "execFind";

	// Check the execution table.
	if((pHashNode = hsearch(&execTable, name)) != NULL) {

		// Found it.  Check operation type.
		if(op == OpQuery) {
			if(!(execPtr(pHashNode)->type & selector))
				return false;
			if(pExec != NULL)
				*pExec = *execPtr(pHashNode);
			return true;
			}
		if(op == OpDelete)
			dfree(hdelete(&execTable, name));		// Delete entry and free the storage.
		else if(op == OpUpdate)
			*execPtr(pHashNode) = *pExec;
		return sess.rtn.status;
		}

	// No such entry exists.  Create one, return false, or fatal error (a bug).
	return (op == OpCreate) ? execNew(name, pExec) : (op == OpQuery) ? false :
	 rsset(FatalError, 0, text16, myName, name);
		// "%s(): No such entry '%s' to update or delete!"
	}

// Get a macro shortcut (key) and set *pShortcut to result.  It is assumed that shortcut is first argument in script mode.  Set
// *ppNode to node pointer if shortcut exists and either (1), OpDelete flag set; or (2), OpCreate flag set and either (a),
// script mode; or (b), interactive mode and user confirms overwrite, otherwise NULL.  If interactive and nothing is entered
// (user-cancel), set *pShortcut to zero and *ppNode to NULL.  "pTermInp" is passed to termInp() function.  Return status.
static int getShortcut(const char *basePrompt, TermInpCtrl *pTermInp, uint opFlags, Shortcut *pShortcut, HashNode **ppNode) {
	Datum *pDatum;
	HashNode *pNode;
	Shortcut s;
	char strShortcut[2], promptBuf[term.cols];

	// Initialize.
	*pShortcut = 0;
	*ppNode = NULL;
	if((pDatum = dnewtrack()) == NULL)
		goto LibFail;

	// Build prompt if interactive.
	if(!(sess.opFlags & OpScript)) {
		if(opFlags & OpDelete) {
			if(macro.shortcuts.nodeCount == 0)
				return rsset(Failure, 0, text200, text543);
						// "No %s defined", "shortcut"
			DFab prompt;
			HashEach each;
			const char *delim;

			// Build prompt with existing shortcuts in parentheses.
			if(dopenwith(&prompt, pDatum, FabClear) != 0 || dputf(&prompt, 0, "%s %s ", basePrompt, text543) != 0)
													// "shortcut"
				goto LibFail;
			delim = "(~#u";

			// Loop through all nodes in shortcut hash and add them to prompt.
			heachinit(&each, &macro.shortcuts);
			while((pNode = heach(&each)) != NULL) {
				if(dputs(delim, &prompt, 0) != 0 || dputs(pNode->key, &prompt, 0) != 0)
					goto LibFail;
				delim = " ";
				}
			if(dputs("~U)", &prompt, 0) != 0 || dclose(&prompt, FabStr) != 0)
				goto LibFail;

			// Fit the prompt in roughly 90% of the terminal width.
			strfit(promptBuf, (size_t) term.cols * 90 / 100 + 1, pDatum->str, 0);
			}
		else {
			// Build basic prompt.
			sprintf(promptBuf, "%s %s", basePrompt, text543);
							// "shortcut"
			}
		}

	// If interactive, prompt until get shortcut or user cancels.
	strShortcut[1] = '\0';
	for(;;) {
		if(sess.opFlags & OpScript) {

			// Script mode.  Get shortcut argument.
			if(funcArg(pDatum, ArgFirst | ArgInt1) != Success || !isCharVal(pDatum))
				break;
			}
		else if(termInp(pDatum, promptBuf, 0, Term_OneChar | Term_Attr, pTermInp) != Success || disnil(pDatum))
			break;

		// Got a key.  Check it.
		s = dgetint(pDatum);
		if(opFlags & OpCreate && (s < '!' || s >= '~'))
			return rsset(Failure, RSTermAttr, "%s ~u%s~U%s", text544, vizc(s, VizSpace), text345);
							//  "Shortcut", " invalid"
		strShortcut[0] = s;
		if((pNode = hsearch(&macro.shortcuts, strShortcut)) == NULL) {
			if(opFlags & OpDelete)
				return rsset(Failure, RSTermAttr, text482, text543, vizc(s, VizSpace));
						// "No such %s ~u%s~U", "shortcut"
			}
		else {
			// Found shortcut in hash.  Get overwrite confirmation if interactive and creating.
			if(!(sess.opFlags & OpScript) && opFlags & OpCreate) {
				bool yep;
				char prompt[WorkBufSize];

				sprintf(prompt, "%s ~u%c~U%s.  %s", text544, s, text468, text84);
					// "Shortcut", " already exists", "Replace"
				if(termInpAttrYN(prompt, &yep) != Success)
					break;
				if(!yep)
					continue;
				}

			// Return node pointer.
			*ppNode = pNode;
			}

		// Return shortcut.
		*pShortcut = s;
		break;
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Find a macro in the macro hash, given name (MF_Name flag set) or value and length (MF_Name flag not set) to match on.  Return
// pointer to node if found; otherwise, set an error if MF_Required flag set and return NULL.
static HashNode *macFind(const void *macroStr, size_t len, uint flags) {

	if(macro.index.table.nodeCount > 0) {
		HashNode *pNode;

		if(!(flags & MF_Name)) {
			Datum *pDatum;

			// Find node that matches macro key list.
			if((pDatum = dnewtrack()) == NULL) {
				(void) libfail();
				return NULL;
				}
			dsetmemref(macroStr, len, pDatum);
			if((pNode = hfindnode(&macro.index.table, pDatum, 0)) != NULL)
				return pNode;
			}
		else if((pNode = hsearch(&macro.index.table, macroStr)) != NULL)
			return pNode;
		}

	// Macro not found.  Set an error if applicable.
	if((flags & (MF_Name | MF_Required)) == (MF_Name | MF_Required))
		(void) rsset(Failure, 0, text52, text466, (const char *) macroStr);
			// "No such %s \"%s\"", "macro"
	return NULL;
	}

// Validate a macro name and return status.  Set "pResult" to nil if name is valid, otherwise error message if script mode, or
// new prompt if interactive.
static int checkName(Datum *pName, Datum *pResult) {
	size_t len;
	DFab fab;
	enum {WrongLen, BadName} result;
	const char *name1, *name = pName->str;

	if(*name == '\0' || (len = strlen(name)) > MaxMacroName)
		result = WrongLen;
	else {
		name1 = name;
		do {
			if(*name1 <= ' ' || *name1 >= '~') {
				result = BadName;
				goto NotValid;
				}
			} while(*++name1 != '\0');

		// Name is valid.
		dsetnil(pResult);
		return sess.rtn.status;
		}
NotValid:
	// Name not valid.  Build exception message.
	if(dopenwith(&fab, pResult, FabClear) != 0)
		goto LibFail;
	if(result == WrongLen) {
		if(dputf(&fab, 0, text280, text471, MaxMacroName) != 0)
				// "%s name cannot be null or exceed %d characters"
			goto LibFail;
		}
	else if(dputf(&fab, 0, "%s%s%s", text471, text477, text541) != 0)
				// "Macro", " name cannot contain ", "space, ~, or non-printable characters"
		goto LibFail;
	if((!(sess.opFlags & OpScript) && dputf(&fab, 0, ".  %s", text478) != 0) || dclose(&fab, FabStr) != 0)
							// "New name"
		goto LibFail;

	// Error message built in "pResult".  Return it.
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Get a macro name, given result pointer, prompt prefix, default value (which may be NULL), script argument flags, and
// operation flags.  If interactive and user cancels, *pRtnVal is set to nil; otherwise, it is set to either a pointer to the
// hash node (if OpAlt specified in opFlags, OpCreate is not, and macro found), or the macro name (otherwise).  Return status.
static int getMacroName(Datum *pRtnVal, const char *prefix, const char *defName, uint argFlags, uint opFlags) {
	const char *prompt;
	Datum *pDatum;
	HashNode *pNode = NULL;
	TermInpCtrl termInpCtrl = {defName, RtnKey, MaxMacroName, NULL};
	enum {Start, NameInUse, BadName} promptType = Start;
	char promptBuf[WorkBufSize];

	if((pDatum = dnewtrack()) == NULL)
		return libfail();
	if(sess.opFlags & OpScript) {

		// Get macro name and check if valid.
		if(funcArg(pRtnVal, argFlags) != Success || checkName(pRtnVal, pDatum) != Success)
			return sess.rtn.status;
		if(!disnil(pDatum))
			return rsset(Failure, RSNoFormat, pDatum->str);
		goto CheckDup;
		}

	// Loop until get valid name interactively or user cancels.
	for(;;) {
		// Determine prompt...
		if(promptType == Start) {
			sprintf(promptBuf, "%s %s", prefix, text466);
					// "macro"
			prompt = promptBuf;
			}
		else if(promptType == NameInUse) {
			sprintf(promptBuf, text25, text109, text324);
					// "That name is%s%s", " already in use", ".  New name"
			prompt = promptBuf;
			}
		else // BadName
			prompt = pDatum->str;

		// and get macro name.
		if(termInp(pRtnVal, prompt, ArgNil1, opFlags & OpCreate ? Term_Attr | TermC_Macro | TermC_NoAuto :
		 Term_Attr | TermC_Macro, &termInpCtrl) != Success)
			return sess.rtn.status;

		// Bail out if nothing was entered; otherwise, validate name.
		if(disnil(pRtnVal))
			goto Retn;
		if(checkName(pRtnVal, pDatum) != Success)
			return sess.rtn.status;

		// If invalid name, reprompt; otherwise, find name in hash.
		if(!disnil(pDatum))
			promptType = BadName;
		else {
CheckDup:
			// Got macro name.  Check if it already exists.  Note that OpUpdate is set only when creating a macro
			// interactively (endMacro command, which is interactive only), so overwrite confirmation dialogue below
			// will never happen in script mode.
			if((pNode = macFind(pRtnVal->str, 0, opFlags & OpCreate ? MF_Name : MF_Name | MF_Required)) != NULL) {

				// Entry exists.  Confirm overwrite, if applicable.
				if(opFlags & OpUpdate) {		// finishMacro() call.
					bool yep;

					sprintf(promptBuf, "%s '%s'%s.  %s", text471, pRtnVal->str, text468, text84);
									// "Macro", " already exists", "Replace"
					if(termInpYN(promptBuf, &yep) != Success)
						return sess.rtn.status;
					if(yep)
						break;
					promptType = Start;
					}
				else if(opFlags & OpCreate) {		// renameMacro() call.
					if(sess.opFlags & OpScript)
						return rsset(Failure, 0, text181, text471, pRtnVal->str, text109);
								// "%s name '%s'%s", "Macro", " already in use"
					promptType = NameInUse;
					}
				else				// editMacro(), renameMacro(), xeqMacro(), delMacro() calls.
					break;
				}
			else {
				if(opFlags & OpCreate)
					break;

				// Macro not found.  Return error (from macFind() call).
				return sess.rtn.status;
				}
			}
		}
Retn:
	if(!(sess.opFlags & OpScript))
		(void) mlerase(0);
	if((opFlags & (OpCreate | OpAlt)) == OpAlt)
		dsetmemref(pNode, 0, pRtnVal);
	return sess.rtn.status;
	}

// Encode the given macro into pDest in string form using ektos(), or set to null if macro not defined.  Return status.
int encodeMacro(Datum *pDest, Macro *pMacro) {

	// Undefined macro?
	if(pMacro->pMacEnd == NULL)
		dsetnull(pDest);
	else {
		DFab fab;
		ExtKey *pExtKey;
		char *key;
		char workBuf[16];

		// Loop through macro keys and translate each into fab, preceded by MacroDelim (except first key).
		if(dopenwith(&fab, pDest, FabClear) != 0)
			goto LibFail;
		workBuf[0] = MacroDelim;
		key = workBuf + 1;
		pExtKey = pMacro->pMacBuf;
		do {
			ektos(workBuf + 1, *pExtKey, false);
			if(dputs(key, &fab, 0) != 0)
				goto LibFail;
			key = workBuf;
			} while(++pExtKey < pMacro->pMacEnd);
		if(dclose(&fab, FabStr) != 0)
LibFail:
			return libfail();
		}

	return sess.rtn.status;
	}

// Increase size of a macro buffer.  Double chunk size on first allocation.
int growMacro(Macro *pMacro) {
	uint i = pMacro->size;
	if((pMacro->pMacBuf = (ExtKey *) realloc((void *) pMacro->pMacBuf,
	 (pMacro->size += (i == 0 ? MacroChunkSize << 1 : MacroChunkSize)) * sizeof(ExtKey))) == NULL)
		return rsset(Panic, 0, text94, "growMacro");
			// "%s(): Out of memory!"
	pMacro->pMacSlot = pMacro->pMacBuf + i;
	return sess.rtn.status;
	}

// Clear staging macro.
static Macro *macClear(Macro *pMacro, bool all) {

	if(all)
		dsetnil(&pMacro->name);
	pMacro->pMacSlot = pMacro->pMacBuf;
	pMacro->pMacEnd = NULL;
	return pMacro;
	}

// Return true if a macro is already active; otherwise, set an error ("not defined", or "not active" if MA_Active flag set) if
// MA_Required flag set and return false.
static bool macActive(Macro *pMacro, uint flags) {
	if(pMacro->pMacEnd != NULL && pMacro->pMacEnd > pMacro->pMacBuf)
		return true;
	if(flags & MA_Required)
		(void) rsset(Failure, 0, flags & MA_Active ? text248 : text200, text466);
				// "No active %s", "No %s defined", "macro"
	return false;
	}

// Check if macro in MacStopped state.  If so, return false.  If not, reset the state to MacStopped, set given error message,
// and return true.
static bool macBusy(const char *fmt, ...) {

	switch(macro.state) {
		case MacStopped:
			return false;

		// Invalid macro state...
		case MacRecording:
			sess.pCurWind->face.wflags |= WFMode;
			break;
		default: // MacPlaying
			macClear(macro.pActiveMacro, true);
		}

	// reset it.
	macro.state = MacStopped;

	// Set error message.
	int r;
	va_list varArgList;
	char *str;

	va_start(varArgList, fmt);
	r = vasprintf(&str, fmt, varArgList);
	va_end(varArgList);
	if(r < 0)
		(void) rsset(Panic, 0, text94, "macBusy");
			// "%s(): Out of memory!"
	else {
		(void) rsset(Failure, RSNoFormat, str);
		free((void *) str);
		}

	return true;
	}

// Decode and store a macro (key list) into staging area from a delimited string containing encoded keys.  Macro name in staging
// area is not modified.  Return status.
static int decodeMacro(char *macroStr) {
	ExtKey c, extKey;
	CmdFuncId id;
	ExecPtr exec;
	bool last;
	Datum *pTok, *pErr;
	Macro *pStage = macro.pStageMacro;
	char *macroStr1 = macroStr;

	// Parse string.
	if(macroStr[0] == '\0')
		(void) rsset(Failure, 0, text447, text466, macroStr);
			// "Invalid %s '%s'", "macro"
	else {
		if((pTok = dnewtrack()) == NULL || (pErr = dnewtrack()) == NULL)
			return libfail();
		macClear(pStage, false);

		// Parse tokens and save in macro staging array.
		while(parseTok(pTok, &macroStr1, MacroDelim) != NotFound) {

			// Convert token string to a key sequence.
			if(*pTok->str == '\0')
				return rsset(Failure, 0, text447, text254, "");
					// "Invalid %s '%s'", "key literal"

			// Process key literal.
			if(stoek(&extKey, pTok->str) != Success)
				goto Retn;

			// Loop once or twice, saving high and low values.
			last = false;
			do {
				// Have a prefix key?
				switch(extKey & Prefix) {
					case Meta:
						id = cf_metaPrefix;
						break;
					case Pref1:
						id = cf_prefix1;
						break;
					case Pref2:
						id = cf_prefix2;
						break;
					case Pref3:
						id = cf_prefix3;
						break;
					default:
						// No prefix.  Save extended key.
						c = extKey;
						last = true;
						goto SaveIt;
					}

				// Get the key binding.
				exec.u.pCmdFunc = cmdFuncTable + id;
				c = findBindPtr(&exec)->extKey;
				extKey &= ~Prefix;
SaveIt:
				// Save key if room.
				if(pStage->pMacSlot == pStage->pMacBuf + pStage->size && growMacro(pStage) != Success)
					goto Retn;
				*pStage->pMacSlot++ = c;
				} while(!last);
			}

		// Empty macro?
		pStage->pMacEnd = pStage->pMacSlot;
		(void) macActive(pStage, MA_Required);
		}
Retn:
	return sess.rtn.status;
	}

// Begin recording a macro.  Error if not in the MacStopped state.  Set up control variables and return status.
// *Interactive only*
int beginMacro(Datum *pRtnVal, long n, Datum **args) {

	if(macBusy(text338, text267, text466, text466))
			// "Cannot %s a %s from a %s", "record", "macro", "macro"
		return sess.rtn.status;

	// Initialize controls and start recording.
	macClear(macro.pStageMacro, true);
	macro.state = MacRecording;
	windNextIs(NULL)->face.wflags |= WFMode;			// Update mode line of bottom window.
	return rsset(Success, 0, text106, text466);
		// "Begin %s recording", "macro"
	}

// End macro recording.  Error if not in the MacRecording state.  Finalize control variables and return status.  The macro
// name, other error checking, and ring update is done by the caller (execute() function) which has access to the command's key
// binding -- needed to adjust the macro length.  *Interactive only*
int endMacro(Datum *pRtnVal, long n, Datum **args) {

	switch(macro.state) {
		case MacStopped:
			(void) rsset(Failure, 0, text107, text471);
				// "%s not active", "Macro"
			break;
		case MacPlaying:
			(void) rsset(Failure, 0, text338, text465, text466, text466);
				// "Cannot %s a %s from a %s", "end", "macro", "macro"
			break;
		default: // MacRecording
			macro.pStageMacro->pMacEnd = macro.pStageMacro->pMacSlot;
			macro.state = MacStopped;
			windNextIs(NULL)->face.wflags |= WFMode;	// Update mode line of bottom window.
		}
	return sess.rtn.status;
	}

// Save macro in staging area to hash and set as active macro if successful and "activate" is true.  Return status.
static int macSave(bool activate) {
	HashNode *pNode;
	Macro *pStage = macro.pStageMacro;

	if((pNode = hnewnode(&macro.index.table, pStage->name.str, NULL)) == NULL ||
	 dsetmem(pStage->pMacBuf, (pStage->pMacEnd - pStage->pMacBuf) * sizeof(ExtKey), pNode->pVal) != 0)
		return libfail();
	macro.index.flags |= OH_Changed;
	if(activate) {
		macro.pStageMacro = macro.pActiveMacro;
		macro.pActiveMacro = pStage;
		}

	return sess.rtn.status;
	}

// Finish macro recording, which includes removing end-macro key sequence, naming the macro, and adding it to the macro hash.
// Called immediately after "endMacro" command.  Return status.  *Interactive only*
int finishMacro(KeyBind *pKeyBind) {
	HashNode *pNode;
	int slotNum = pKeyBind->extKey & Prefix ? 2 : 1;
	Macro *pStage = macro.pStageMacro;
	pStage->pMacSlot -= slotNum;
	pStage->pMacEnd -= slotNum;
	if(!macActive(pStage, MA_Required))
		return sess.rtn.status;

	// Valid macro was recorded.  Check if duplicate.
	if((pNode = macFind(pStage->pMacBuf, (pStage->pMacEnd - pStage->pMacBuf) * sizeof(ExtKey), 0)) != NULL) {
		bool yep;
		char promptBuf[128];

		// New macro is a duplicate (by value)... user okay with that?
		sprintf(promptBuf, text470, text471, pNode->key);
			// "%s is identical to '%s'", "Macro"
		strcat(promptBuf, text476);
			// ".  Discard"
		if(termInpYN(promptBuf, &yep) != Success || yep)
			return sess.rtn.status;
		}

	// Get new macro name.
	if(getMacroName(&pStage->name, text469, NULL, 0, OpCreate | OpUpdate) != Success)
				// "Save to"
		return sess.rtn.status;
	if(disnil(&pStage->name))
		return rsset(Success, RSHigh, "%s %s", text471, text472);
						// "Macro", "discarded"
	// It's a keeper... add macro to hash.
	return macSave(true) == Success ? rsset(Success, RSForce, "%s %s", text471, text108) : sess.rtn.status;
								// "Macro", "saved"
	}

// Load macro in given hash node into staging area.  Return status.
int loadStage(HashNode *pNode) {
	Macro *pStage = macro.pStageMacro;
	size_t byteLen = dgetmemsize(pNode->pVal);
	uint keyCount = byteLen / sizeof(ExtKey);

	// Set macro name...
	if(dsetstr(pNode->key, &pStage->name) != 0)
		return libfail();

	// make sure there's room in staging buffer for key list...
	while(keyCount > pStage->size)
		if(growMacro(pStage) != Success)
			return sess.rtn.status;

	// and copy macro from hash node to staging area.
	memcpy(pStage->pMacSlot = pStage->pMacBuf, dgetmemptr(pNode->pVal), byteLen);
	pStage->pMacEnd = pStage->pMacBuf + keyCount;

	return sess.rtn.status;
	}

// Check if given name matches active macro and return Boolean result.
bool isActiveMacro(const char *name) {

	return !disnil(&macro.pActiveMacro->name) && strcmp(name, macro.pActiveMacro->name.str) == 0;
	}

// Load macro in given hash node (if new) and set as active macro.  Return status.
static int macLoad(HashNode *pNode) {

	// Skip this if exiting program or macro to load is already active.
	if(sess.rtn.status > MinExit && !isActiveMacro(pNode->key)) {
		if(loadStage(pNode) == Success) {

			// Macro loaded into staging area successfully.  Now set it as active macro and return.
			Macro *pStage = macro.pStageMacro;
			macro.pStageMacro = macro.pActiveMacro;
			macro.pActiveMacro = pStage;
			}
		}

	return sess.rtn.status;
	}

// Store macro into new two-element array and return it in *ppArray.
static int macStore(Array **ppArray, Datum *pDatum, Macro *pMacro) {
	Array *pArray;

	// Convert key list to string.
	if(encodeMacro(pDatum, pMacro) != Success)
		return sess.rtn.status;

	// Create array for macro and store as [name, key-list].
	if((pArray = anew(2, NULL)) == NULL || dsetstr(pMacro->name.str, pArray->elements[0]) != 0 ||
	 dsetstr(pDatum->str, pArray->elements[1]) != 0)
		return libfail();

	*ppArray = pArray;
	return sess.rtn.status;
	}

// [Re]create a macro shortcut and return status.  If interactive and n argument, create shortcut for named macro; otherwise,
// create shortcut for active macro.
int setShortcut(Datum *pRtnVal, long n, Datum **args) {
	Datum *pName;
	HashNode *pNode1, *pNode2;
	Macro *pActive = macro.pActiveMacro;
	bool useActive;
	Shortcut s;
	char strShortcut[2], workBuf[WorkBufSize];

	// Do sanity checks.
	if(macBusy(text338, text180, text543, text466))
			// "Cannot %s a %s from a %s", "create", "shortcut", "macro"
		goto Retn;
	if(!(sess.opFlags & OpScript) && (useActive = n == DefN) && !macActive(pActive, MA_Required | MA_Active))
		goto Retn;

	// Get shortcut.
	strShortcut[1] = '\0';
	for(;;) {
		if(getShortcut(text542, NULL, OpCreate, &s, &pNode1) != Success || s == 0)
				// "Create"
			break;

		// If script mode, check if using active macro.
		if(sess.opFlags & OpScript && (useActive = !haveSym(s_comma, false)) &&
		 !macActive(pActive, MA_Required | MA_Active))
			break;

		// Get macro name.
		if(useActive) {
			// Active macro.
			pName = &pActive->name;
			}
		else {
			// Named macro.  Get name.
			if(getMacroName(pRtnVal, text545, NULL, ArgNotNull1 | ArgNil1, OpDelete) != Success || disnil(pRtnVal))
						// "for"
				break;
			pName = pRtnVal;
			}

		// Check if macro already has a shortcut.
		if((pNode2 = hfindnode(&macro.shortcuts, pName, 0)) != NULL) {
			if(!(sess.opFlags & OpScript))
				sprintf(workBuf, "%s ~u%s~U%s %s %s '%s'",
				 text544, pNode2->key, text468, text545, text466, pName->str);
					// "Shortcut", " already exists", "for", "macro"
			if(pNode2 == pNode1) {

				// Identical shortcut.  Nothing to do.
				if(!(sess.opFlags & OpScript))
					(void) rsset(Success, RSTermAttr | RSNoWrap, "%s - %c%s", workBuf,
					 lowCase[(int) *text395], text395 + 1);
							// "No changes made"
				break;
				}

			// Get overwrite confirmation if interactive.
			if(!(sess.opFlags & OpScript)) {
				bool yep;

				sprintf(strchr(workBuf, '\0'), ".  %s", text84);
								// "Replace"
				if(termInpAttrYN(workBuf, &yep) != Success || !yep)
					break;
				}

			// Got user okay or script mode.  Delete old shortcut.
			dfree(hdelete(&macro.shortcuts, pNode2->key));
			}

		// Create or replace shortcut.
		strShortcut[0] = s;
		if((pNode1 == NULL && (pNode1 = hnewnode(&macro.shortcuts, strShortcut, NULL)) == NULL) ||
		 dsetstr(pName->str, pNode1->pVal) != 0)
			return libfail();
		if(!(sess.opFlags & OpScript))
			(void) rsset(Success, RSTermAttr, "%s ~u%c~U %s %s %s \"%s\"", text544, s, text392, text545, text466,
										// "Shortcut", "created", "for", "macro"
			 pName->str);
		break;
		}
	dsetnil(pRtnVal);
Retn:
	return sess.rtn.status;
	}

// Delete macro shortcut[s], or all shortcuts if n <= 0.  Delete one shortcut interactively if default n or n > 0.  No error if
// script mode and a shortcut does not exist.
int delShortcut(Datum *pRtnVal, long n, Datum **args) {
	int count = 0;
	HashNode *pNode;
	Shortcut s;

	if(macBusy(text338, text263, text543, text466))
			// "Cannot %s a %s from a %s", "delete", "shortcut", "macro"
		goto Retn;

	if(n <= 0 && n != DefN) {

		// Get confirmation if interactive.
		if(interactive()) {
			bool yep;
			char workBuf[WorkBufSize];

			sprintf(workBuf, text538, text543);
				// "Delete all %ss", "shortcut"
			if(termInpYN(workBuf, &yep) != Success || !yep)
				goto Retn;
			}

		// Delete all shortcuts.
		if((count = macro.shortcuts.nodeCount) > 0)
			hclear(&macro.shortcuts, 1);
		(void) rsset(Success, RSHigh, text299, text543);
					// "All %ss deleted", "shortcut"
		}
	else {
		// Delete given shortcut(s), or one if interactive.
		if(!(sess.opFlags & OpScript)) {
			if(getShortcut(text26, NULL, OpDelete, &s, &pNode) == Success && pNode != NULL) {
					// "Delete"
				dfree(hdelete(&macro.shortcuts, pNode->key));
				(void) rsset(Success, RSTermAttr, "%s ~u%c~U %s", text544, s, text10);
									// "Shortcut", "deleted"
				}
			goto Retn;
			}
		else {
			uint argFlags = ArgFirst | ArgInt1;
			char strShortcut[2];
			strShortcut[1] = '\0';
			for(;;) {
				if(funcArg(pRtnVal, argFlags) != Success || !isCharVal(pRtnVal))
					goto Retn;

				// Delete shortcut.  No error if it does not exist.
				if(macro.shortcuts.nodeCount > 0) {
					strShortcut[0] = dgetint(pRtnVal);
					if((pNode = hsearch(&macro.shortcuts, strShortcut)) != NULL) {
						dfree(hdelete(&macro.shortcuts, pNode->key));
						++count;
						}
					}
				if(!haveSym(s_comma, false))
					break;
				argFlags = ArgInt1;
				}
			}
		}

	// Return count if script mode.
	if(sess.opFlags & OpScript)
		dsetint((long) count, pRtnVal);
Retn:
	return sess.rtn.status;
	}

// Delete a macro and its shortcut if they exist in hash, and clear active macro if name matches.  Return 1 if successful,
// otherwise 0.
static int macDel(const char *name) {
	Datum *pDatum;
	HashNode *pNode;
	int i = 0;

	// Delete macro if it exists.
	if((pDatum = hdelete(&macro.index.table, name)) != NULL) {
		Macro *pActive = macro.pActiveMacro;
		dfree(pDatum);
		macro.index.flags |= OH_Changed;
		i = 1;
		if(isActiveMacro(name))
			macClear(pActive, true);

		// Delete shortcut if it exists.
		if(macro.shortcuts.nodeCount > 0) {
			Datum datum;

			dinit(&datum);
			dsetstrref(name, &datum);
			if((pNode = hfindnode(&macro.shortcuts, &datum, 0)) != NULL)
				dfree(hdelete(&macro.shortcuts, pNode->key));		// Can't fail.
			}
		}
	return i;
	}

// Get a named macro, active macro (if first argument is nil), or all macros (if no argument) and return status.  Error if not
// in the MacStopped state.  Return macro(s) in array [name, value] form, or nil if none.
int getMacro(Datum *pRtnVal, long n, Datum **args) {
	Macro *pMacro;
	HashNode *pNode, **pAry;
	Array *pArray0, *pArray1;
	Datum *pArrayEl, *pDatum;

	if(macBusy(text338, text473, text466, text466))
			// "Cannot %s a %s from a %s", "operate on", "macro", "macro"
		goto Retn;

	if((pDatum = dnewtrack()) == NULL)
		goto LibFail;
	if(*args == NULL) {

		// Return all macros.  Create array with nested two-element arrays.
		if((pArray0 = anew(0, NULL)) == NULL)
			goto LibFail;
		astash(pRtnVal, pArray0);
		if(macro.index.table.nodeCount > 0) {
			pMacro = macro.pStageMacro;
			if(ohAryPrep(&macro.index, &pAry) != 0)
				goto LibFail;

			// Loop through all nodes in macro hash.
			while((pNode = *pAry++) != NULL) {

				// Load macro into staging area and convert to string form.
				if(loadStage(pNode) != Success)
					goto Retn;
				if(macStore(&pArray1, pDatum, pMacro) != Success)
					goto Retn;

				// Append converted macro to main array.
				if((pArrayEl = aget(pArray0, pArray0->used, OpGrow)) == NULL)
					goto LibFail;
				astash(pArrayEl, pArray1);
				}
			}
		}
	else {
		// Return either active macro or named macro.  Check if requested macro exists.
		dsetnil(pRtnVal);
		if(disnil(*args)) {
			pMacro = macro.pActiveMacro;
			if(pMacro->pMacEnd == NULL)
				goto Retn;
			}
		else if((pNode = macFind((*args)->str, 0, MF_Name)) != NULL) {
			if(loadStage(pNode) != Success)
				goto Retn;
			pMacro = macro.pStageMacro;
			}
		else
			goto Retn;
		if(macStore(&pArray1, pDatum, pMacro) == Success)
			astash(pRtnVal, pArray1);
		}
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Load a macro into staging area and return status.  Error if not in the MacStopped state or macro is a duplicate by name or
// value.  Options are:
//	Activate	Make macro active after it is loaded.
//	Force		Force overwrite if duplicate by name or value.
int loadMacro(Datum *pRtnVal, long n, Datum **args) {
	Datum *pErr;
	OptHdr *pOptHdr;
	HashNode *pNode;
	bool activate, force;
	Macro *pStage = macro.pStageMacro;
	const char *dupName1, *dupName2;

	if(macBusy(text338, text473, text466, text466))
			// "Cannot %s a %s from a %s", "operate on", "macro", "macro"
		goto Retn;

	// Set options.
	pOptHdr = cmdFuncTable[cf_loadMacro].pOptHdr;
	activate = pOptHdr->optTable[0].ctrlFlags & OptSelected;
	force = pOptHdr->optTable[1].ctrlFlags & OptSelected;

	// Validate and save macro name.
	if((pErr = dnewtrack()) == NULL)
		return libfail();
	if(checkName(args[0], pErr) != Success)
		return sess.rtn.status;
	if(!disnil(pErr))
		return rsset(Failure, RSNoFormat, pErr->str);
	dxfer(&pStage->name, args[0]);

	// Load key list into staging area.
	if(decodeMacro(args[1]->str) != Success)
 		goto Retn;

	// Does another macro with same key list exist?
	dupName1 = dupName2 = NULL;
	if((pNode = macFind(pStage->pMacBuf, (pStage->pMacEnd - pStage->pMacBuf) * sizeof(ExtKey), 0)) != NULL) {
		if(force)
			dupName1 = pNode->key;
		else
			return rsset(Failure, 0, text470, text471, pNode->key);
				// "%s is identical to '%s'", "Macro"
		}

	// Does another macro with same name exist?
	if(macFind(pStage->name.str, 0, MF_Name) != NULL) {
		if(force)
			dupName2 = pStage->name.str;
		else
			return rsset(Failure, 0, "%s '%s'%s", text471, pStage->name.str, text468);
							// "Macro", " already exists"
		}

	// Remove any duplicate macro(s) from hash.
	if(dupName1 != NULL)
		(void) macDel(dupName1);
	if(dupName2 != NULL)
		(void) macDel(dupName2);

	// Save macro in staging area and return.
	(void) macSave(activate);
Retn:
	return sess.rtn.status;
	}

// Perform preliminary operations for some macro routines.  If OpAlt flag specified, macro node pointer is returned in pRtnVal;
// otherwise, macro name is returned in *ppDatum (and ppDatum is assumed to not be NULL).  Additionally, "prefix" is assumed to
// be a title-case operation word; for example, "Select" or "Edit".
static int macGet(Datum *pRtnVal, const char *prefix, const char *defName, uint opFlags, Datum **ppDatum) {
	char lcPrefix[16];

	sprintf(lcPrefix, "%c%s", lowCase[(int) *prefix], prefix + 1);
	if(macBusy(text338, lcPrefix, text466, text466))
			// "Cannot %s a %s from a %s", "macro", "macro"
		return sess.rtn.status;

	// Get new datum and return it.
	if(ppDatum != NULL && (*ppDatum = dnew()) == NULL)
		return libfail();

	// Get macro to operate on and return.
	return getMacroName(opFlags & OpAlt ? pRtnVal : *ppDatum, prefix, defName, ArgFirst | ArgNotNull1, opFlags);
	}

// Select and activate a macro.
int selectMacro(Datum *pRtnVal, long n, Datum **args) {
	HashNode *pNode;

	if(macGet(pRtnVal, text24, NULL, OpDelete | OpAlt, NULL) != Success || (pNode = dgetmemptr(pRtnVal)) == NULL ||
			// "Select"
	 macLoad(pNode) != Success)
		goto Retn;
	if(!(sess.opFlags & OpScript))
		(void) rsset(Success, 0, "%s '%s' %s", text471, pNode->key, text540);
						// "Macro", "activated"
Retn:
	return sess.rtn.status;
	}

// Edit a macro on message line and return status.  If n argument or macro is active, load it after edit.  *Interactive only*
int editMacro(Datum *pRtnVal, long n, Datum **args) {
	HashNode *pNode;
	Datum *pDatum;
	DFab fab;
	Macro *pStage = macro.pStageMacro;
	TermInpCtrl termInpCtrl = {NULL, RtnKey, 0, NULL};

	if(macGet(pRtnVal, text402, !disnil(&macro.pActiveMacro->name) ? macro.pActiveMacro->name.str : NULL, OpDelete | OpAlt,
			// "Edit"
	 &pDatum) != Success || (pNode = dgetmemptr(pRtnVal)) == NULL)
		goto Retn;

	// Load macro from hash into staging area and encode value into pDatum.
	if(loadStage(pNode) != Success || encodeMacro(pDatum, pStage) != Success)
		goto Retn;

	// Get new value from user into pRtnVal.
	termInpCtrl.defVal = pDatum->str;
	if(termInp(pRtnVal, text317, ArgNil1, 0, &termInpCtrl) != Success || disnil(pRtnVal))
			// "New value"
		goto Retn;

	// Make sure new value is valid if changed.
	if(deq(pDatum, pRtnVal, 0))
		(void) rsset(Success, RSNoFormat, text395);
				// "No changes made"
	else {
		if(decodeMacro(pRtnVal->str) != Success)
			goto Retn;

		// We're good.  Have updated macro in staging area... save it.  Also, make it active if non-default n or old
		// version is active.
		if(dsetmem(pStage->pMacBuf, (pStage->pMacEnd - pStage->pMacBuf) * sizeof(ExtKey), pNode->pVal) != 0)
			goto LibFail;
		if(dopenwith(&fab, pDatum, FabClear) != 0 || dputf(&fab, 0, "%s '%s' %s", text471, pNode->key, text260) != 0)
											// "Macro", "changed"
			goto LibFail;
		if(n != DefN || isActiveMacro(pNode->key)) {
			macro.pStageMacro = macro.pActiveMacro;
			macro.pActiveMacro = pStage;
			if(dputs(text440, &fab, 0) != 0)
					// " and activated"
				goto LibFail;
			}
		if(dclose(&fab, FabStr) != 0)
			goto LibFail;
		(void) rsset(Success, RSNoFormat, pDatum->str);
		}
Retn:
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Enable execution of a macro abs(n) times (which will be subsequently run by the getkey() function).  Error if not in
// MacStopped state.  If interactive, run a macro as follows:
//   If default n:	Run active macro once, or return error if not defined.
//   If n < 0:		Prompt for macro name, load it, and run it abs(n) times.
//   If n >= 0:
//		If no shortcut(s):	Execute active macro n times if it exists; otherwise, return error.
//		If shortcut(s) exist:	Create prompt with shortcuts and take action as indicated in following table:
//   Active
//   Macro?	Has Shortcut?	Prompt Default			Action
//	Y		Y	Active macro shortcut		Run entered or default shortcut n times.
//	Y		N	None, show RTN default in text	Run shortcut entered (or active macro if RTN) n times.
//	N		-	None				Run shortcut entered n times.  Cancel operation if RTN.
// If script mode and n < 0, return an error.  Set up the control variables and return status.
int xeqMacro(Datum *pRtnVal, long n, Datum **args) {
	Macro *pActive = macro.pActiveMacro;
	long n1 = n == DefN ? 1 : n;

	if(macBusy(text338, text244, text466, text466))
			// "Cannot %s a %s from a %s", "execute", "macro", "macro"
		goto Retn;
	if(sess.opFlags & OpScript) {
		if(n1 < 0)
			return rsset(Failure, 0, text39, text137, n, 0);
					// "%s (%'ld) must be %d or greater", "Repeat count"
		if(haveSym(s_any, false) && (!(sess.opFlags & OpParens) || !haveSym(s_rightParen, false)))
			goto GetName;
		if(!macActive(pActive, MA_Required | MA_Active))
			goto Retn;
		}
	else if(n1 < 0) {
GetName:;
		HashNode *pNode;

		if(getMacroName(pRtnVal, text117, n == 0 && !disnil(&pActive->name) ? pActive->name.str : NULL,
				// "Execute"
		 ArgFirst | ArgNotNull1, OpDelete | OpAlt) != Success || (pNode = dgetmemptr(pRtnVal)) == NULL ||
		 macLoad(pNode) != Success)
			goto Retn;
		dsetnil(pRtnVal);
		n1 = labs(n1);
		}
	else {
		// Error if default n or no shortcuts exist and no active macro.
		if(n == DefN || macro.shortcuts.nodeCount == 0) {
			if(!macActive(pActive, MA_Required | MA_Active))
				goto Retn;
			}
		else {
			HashNode *pNode;
			Shortcut s;
			bool activeMacro = macActive(pActive, 0);
			TermInpCtrl termInpCtrl = {NULL, RtnKey, 0, NULL};

			// Prompt for macro shortcut and load it..
			if(activeMacro && (pNode = hfindnode(&macro.shortcuts, &pActive->name, 0)) != NULL)
				termInpCtrl.defVal = pNode->key;
			if(getShortcut(text117, &termInpCtrl, OpDelete, &s, &pNode) != Success || pNode == NULL ||
					// "Execute"
			 macLoad(hsearch(&macro.index.table, pNode->pVal->str)) != Success)
				goto Retn;
			}
		}

	// Have active macro at this point... enable playback.
	pActive = macro.pActiveMacro;
	macro.n = n1;					// Remember how many times to execute...
	macro.state = MacPlaying;			// switch to "play" mode...
	pActive->pMacSlot = pActive->pMacBuf;		// at the beginning.
Retn:
	return sess.rtn.status;
	}

// Rename a macro and return status.  Error if not in MacStopped state.
int renameMacro(Datum *pRtnVal, long n, Datum **args) {
	Datum *pDatum;

	// Get name of macro to rename.
	if(macGet(NULL, text29, NULL, OpDelete, &pDatum) != Success || disnil(pDatum))
			// "Rename"
		goto Retn;

	// Get new name.
	if(getMacroName(pRtnVal, text339, NULL, ArgNotNull1, OpCreate) != Success || disnil(pRtnVal))
			// "to"
		goto Retn;

	// Have old and new names... rename macro.
	if(hrename(&macro.index.table, pDatum->str, pRtnVal->str, NULL) != 0 ||
	 (isActiveMacro(pDatum->str) && dsetstr(pRtnVal->str, &macro.pActiveMacro->name) != 0))
		return libfail();
	macro.index.flags |= OH_Changed;

	if(!(sess.opFlags & OpScript))
		(void) rsset(Success, 0, "%s %s", text471, text467);
						// "Macro", "renamed"
Retn:
	return sess.rtn.status;
	}

// Delete macro(s) by name, or all macros if n <= 0.  Error if not in MacStopped state.  No error if script mode and macro not
// found.  Return number of macros deleted.
int delMacro(Datum *pRtnVal, long n, Datum **args) {

	if(macBusy(text338, text263, text466, text466))
			// "Cannot %s a %s from a %s", "delete", "macro", "macro"
		goto Retn;
	dsetint(0, pRtnVal);
	if(macro.index.table.nodeCount == 0)
		return sess.opFlags & OpScript ? sess.rtn.status : rsset(Failure, 0, text200, text466);
										// "No %s defined", "macro"
	if(n <= 0 && n != DefN) {

		// Get confirmation if interactive.
		if(interactive()) {
			bool yep;
			char workBuf[WorkBufSize];

			sprintf(workBuf, text538, text466);
				// "Delete all %ss", "macro"
			if(termInpYN(workBuf, &yep) != Success || !yep)
				goto Retn;
			}

		// Delete all macros and shortcuts.
		n = macro.index.table.nodeCount;
		hclear(&macro.shortcuts, 1);
		hclear(&macro.index.table, 1);
		macro.index.flags |= OH_Changed;
		macClear(macro.pActiveMacro, true);
		(void) rsset(Success, RSHigh, text299, text466);
					// "All %ss deleted", "macro"
		}
	else {
		// Delete given macro(s), or one if interactive.
		if(!(sess.opFlags & OpScript)) {
			if(getMacroName(pRtnVal, text26, NULL, ArgNotNull1, OpDelete) != Success || disnil(pRtnVal))
					// "Delete"
				goto Retn;
			(void) macDel(pRtnVal->str);
			(void) rsset(Success, 0, "%s '%s' %s", text471, pRtnVal->str, text10);
							// "Macro", "deleted"
			goto Retn;
			}
		else {
			Datum *pName;
			uint argFlags = ArgFirst | ArgNotNull1;
			if((pName = dnewtrack()) == NULL)
				return libfail();
			n = 0;
			for(;;) {
				if(funcArg(pName, argFlags) != Success)
					goto Retn;

				// Delete macro.  No error if it does not exist.
				n += macDel(pName->str);
				if(!haveSym(s_comma, false))
					break;
				argFlags = ArgNotNull1;
				}
			}
		}

	// Return count if script mode.
	if(sess.opFlags & OpScript)
		dsetint(n, pRtnVal);
Retn:
	return sess.rtn.status;
	}
