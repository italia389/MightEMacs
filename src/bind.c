// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// bind.c	Key binding routines for MightEMacs.

#include "std.h"
#include <stdarg.h>
#include "cmd.h"
#include "exec.h"
#include "search.h"

// Make selected global definitions local.
#define BindData
#include "bind.h"

/*** Local declarations ***/

// Recognized keywords in key binding literals.
typedef struct {
	char *keyword;		// Keyword.
	ushort extKey;		// Character (extended key).
	} KeyLit;
static KeyLit keyLiterals[] = {
	{"SPC", ' '},
	{"TAB", Ctrl | 'I'},
	{"ESC", Ctrl | '['},
	{"RTN", Ctrl | 'M'},
	{"DEL", Ctrl | '?'},
	{NULL, '\0'}};

// Convert extended key to ordinal character value.  Collapse the Ctrl flag back into the ASCII code.  If "extend" is true,
// return function key values (94 possibilities) in range 128..(128+93), S-TAB as 128+94, and shifted function key values in
// range (128+94+1)..(128+94+1+93), otherwise 0..127 like other characters.
short ektoc(ushort extKey, bool extend) {

	// Do special cases first.
	if((extKey & (Ctrl | 0xFF)) == (Ctrl | ' '))
		return 0;					// Null char.
	if((extKey & (Shift | Ctrl | 0xFF)) == (Shift | Ctrl | 'I'))
		return 128 + 94;				// S-TAB.

	// Now do control keys and function keys.
	short c = extKey & 0xFF;
	if(extKey & Ctrl)
		return c ^ 0x40;				// Actual control char.
	if((extKey & FKey) && extend)
		c += (extKey & Shift) ? (128 + 94 + 1 - 33)	// FNx character in range ! .. ~.
		 : (128 - 33);
	return c;
	}

// Walk through all key binding lists and return next binding in sequence, or NULL if none left.  If pKeyWalk->hkp is NULL,
// reset to beginning and return first binding found.
KeyBind *nextBind(KeyWalk *pKeyWalk) {
	KeyVect *pKeyVect = pKeyWalk->pKeyVect;
	KeyBind *pKeyBind = pKeyWalk->pKeyBind;

	if(pKeyVect == NULL)
		pKeyBind = (KeyBind *) (pKeyWalk->pKeyVect = pKeyVect = keyBindTable);

	for(;;) {
		if(pKeyBind == (KeyBind *) pKeyVect + KeyVectSlots) {
			if(++pKeyVect == keyBindTable + KeyTableCount)
				return NULL;
			pKeyBind = (KeyBind *) (pKeyWalk->pKeyVect = pKeyVect);
			}
		if(pKeyBind->code != 0)
			break;
		++pKeyBind;
		}
	pKeyWalk->pKeyBind = pKeyBind + 1;		
	return pKeyBind;
	}

// Return the number of entries in the binding table that match the given universal pointer.
static int ptrEntryCount(UnivPtr *pUniv) {
	int count = 0;
	KeyWalk keyWalk = {NULL, NULL};
	KeyBind *pKeyBind = nextBind(&keyWalk);

	// Search for existing bindings for the command.
	do {
		if(pKeyBind->targ.u.pVoid == pUniv->u.pVoid)
			++count;
		} while((pKeyBind = nextBind(&keyWalk)) != NULL);

	return count;
	}

// Scan the binding table for the first entry that matches the given universal pointer and return it, or NULL if none found.
KeyBind *getPtrEntry(UnivPtr *pUniv) {
	KeyWalk keyWalk = {NULL, NULL};
	KeyBind *pKeyBind = nextBind(&keyWalk);

	// Search for an existing binding for the command.
	do {
		if(pKeyBind->targ.u.pVoid == pUniv->u.pVoid)
			return pKeyBind;
		} while((pKeyBind = nextBind(&keyWalk)) != NULL);

	return NULL;
	}

// Return binding slot for given extended key.
static KeyBind *bindSlot(ushort extKey) {
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

	return (KeyBind *) (keyBindTable + i) + ektoc(extKey, true);
	}

// Look up a key binding in the binding table, given extended key.
KeyBind *getBind(ushort extKey) {
	KeyBind *pKeyBind = bindSlot(extKey);
	if(pKeyBind->code != 0)
		return pKeyBind;

	// No such binding.
	return NULL;
	}

// Add an extended key to the binding table.
static void newBind(ushort extKey, UnivPtr *pUniv) {
	KeyBind *pKeyBind = bindSlot(extKey);

	// Set keycode and the command or buffer pointer.
	pKeyBind->code = extKey;
	pKeyBind->targ = *pUniv;
	}

// Get binding of given extended key and return prefix flag if it's bound to a prefix command, otherwise zero.
static ushort findPrefix(ushort extKey) {
	KeyBind *pKeyBind = getBind(extKey);
	if(pKeyBind != NULL && pKeyBind->targ.type == PtrPseudo) {
		CmdFunc *pCmdFunc = pKeyBind->targ.u.pCmdFunc;
		if(pCmdFunc->attrFlags & CFPrefix) {
			CmdFuncId id = (CmdFuncId) (pCmdFunc - cmdFuncTable);
			return id == cf_metaPrefix ? Meta : id == cf_prefix1 ? Pref1 : id == cf_prefix2 ? Pref2 : Pref3;
			}
		}
	return 0;
	}

// Get one value from the coded string in *pKeyCode.  Store result in *pExtKey, update *pKeyCode, and return true if successful,
// otherwise false.
static bool stoek1(const char **pKeyCode, ushort *pExtKey) {
	ushort c;
	ushort extKey = 0;
	KeyLit *pKeyLit;
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
					c = pKeyLit->extKey & 0xFF;
					extKey |= pKeyLit->extKey & ~0xFF;
					goto Vanilla;
					}
				} while((++pKeyLit)->keyword != NULL);

			// Not a keyword.  Literal control character? (Boo, hiss.)
			if(c < ' ' || c == 0x7F) {
				if(extKey & Ctrl)			// Duplicate?
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
					if(extKey & Shift)			// Error if S-C-.
						return false;
					if(c == '@')			// C-@ or ^@ ?
						c = ' ';		// Yes, change back to a space.
					else if(c != ' ' && (c < '?' || c == '`' || c > 'z'))
						return false;		// Invalid character following C- or ^.
					}
				if((extKey & (FKey | Shift)) == Shift) {	// If SHIFT prefix without FNx...
					if(is_letter(c)) {		// error if printable char. follows and not a letter.
						c = upCase[c];
						extKey &= ~Shift;
						}
					else if((c >= ' ' && c < 'A') || (c > 'Z' && c < 'a') || (c > 'z' && c <= '~'))
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

// Convert a coded string to an extended key code.  Set *result to zero if keyLit is invalid.  Return status.
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
int stoek(const char *keyCode, ushort *result) {
	ushort extKey1;				// One key.
	ushort extKey = 0;			// Final extended key.
	ushort keyCount = 0;
	const char *keyCode1 = keyCode;

	// Parse it up.
	for(;;) {
		// Get a value into extKey1.
		if(!stoek1(&keyCode1, &extKey1))
			goto BadCode;
		keyCount += (extKey1 & Meta) ? 2 : 1;

		// Success.  Done?
		if(*keyCode1 == ' ') {
			if(keyCount == 2)
				goto BadCode;

			// Have first of two values.  If not a prefix key, return an error; otherwise, set flag.
			ushort flag = findPrefix(extKey1);
			if(flag == 0)
				goto BadCode;
			extKey = flag;
			++keyCode1;
			}
		else {
			if(*keyCode1 != '\0') {
BadCode:
				*result = 0;
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

	*result = extKey;
	return sess.rtn.status;
	}

#if MMDebug & Debug_Bind
// Dump binding table.
static void dumpBind(void) {
	KeyVect *pKeyVect = keyBindTable;
	KeyBind *pKeyBind;
	int i = 0;
	char keyBuf[16];

	do {
		fprintf(logfile, "\nBINDING LIST #%d\n", i++);
		pKeyBind = (KeyBind *) pKeyVect;
		do {
			if(pKeyBind->code == 0)
				continue;
			fprintf(logfile, "    %8s   ->   %s\n", ektos(pKeyBind->code, keyBuf, false),
			 (pKeyBind->targ.type & PtrSysCmdType) ? pKeyBind->targ.u.pCmdFunc->name :
			 pKeyBind->targ.u.pBuf->bufname);
			} while(++pKeyBind < (KeyBind *) pKeyVect + KeyVectSlots);
		} while(++pKeyVect < keyBindTable + KeyTableCount);
	}
#endif

// Clear extended key from key cache, if present.
static void clearCache(ushort extKey) {
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
	clearCache(pKeyBind->code);

	// Clear the entry.
	pKeyBind->code = 0;
	pKeyBind->targ.u.pVoid = NULL;
	}

// Load all the built-in command key bindings.  Return status.
int loadBindings(void) {
	UnivPtr univ;
	KeyItem *pKeyItem = keyItems;

	do {
		univ.type = ((univ.u.pCmdFunc = cmdFuncTable + pKeyItem->id)->attrFlags & CFHidden) ? PtrPseudo : PtrSysCmd;
		newBind(pKeyItem->code, &univ);
		} while((++pKeyItem)->code != 0);

	return sess.rtn.status;
	}

#if 0
// Dump execution table to log file.
static void dumpExec(void) {
	UnivPtr *pUniv;
	HashRec **ppHashRec0, **ppHashRecEnd;
	HashRec **ppHashRec = NULL;
	fputs("\nEXEC TABLE\n", logfile);
	(void) hsort(execTable, hcmp, &ppHashRec);
	ppHashRecEnd = (ppHashRec0 = ppHashRec) + execTable->recCount;
	do {
		pUniv = univPtr(*ppHashRec);
		fprintf(logfile, "%-20s%.4hx\n", (*ppHashRec)->key, pUniv->type);
		} while(++ppHashRec < ppHashRecEnd);
	free((void *) ppHashRec0);
	}
#endif

// Get command or function name per selector flags and return status.  Store pointer in *pUniv (which may be NULL if interactive
// and user cancelled).  If interactive mode, pass prompt to getCFA().  Called by:
//	bindKey(..., PtrSysCmdType | PtrUserCmd)
//	setHook(..., PtrSysFunc | PtrUserFunc)
//	alias(..., PtrSysCmdFunc | PtrUserCmdFunc)
int getCF(const char *prompt, UnivPtr *pUniv, uint selector) {
	const char *errorMsg = (selector & PtrPseudo) ? text130 : (selector & PtrSysCmd) ? text312 : text116;
			// "No such command '%s'", "No such command or function '%s'", "No such function '%s'"
	if(sess.opFlags & OpScript) {
		if(!haveSym(s_ident, false) && !haveSym(s_identQuery, true))
			return sess.rtn.status;
		if(!(sess.opFlags & OpEval)) {
			pUniv->type = PtrNull;
			pUniv->u.pVoid = NULL;
			}
		else if(!execFind(pLastParse->tok.str, OpQuery, selector, pUniv))
			return rsset(Failure, 0, errorMsg, pLastParse->tok.str);
		(void) getSym();
		}
	else
		(void) getCFA(prompt, selector | Term_Attr, pUniv, errorMsg);
	return sess.rtn.status;
	}

// Get a key binding (using given prompt if interactive) and save in *result.  If n <= 0, get one key only; otherwise, get a
// key sequence.  Return status.
int getKeyBind(const char *prompt, int n, Datum **args, ushort *result) {

	// Script mode?
	if(sess.opFlags & OpScript) {

		// Yes, process argument.
		if(sess.opFlags & OpEval)
			(void) stoek(args[0]->str, result);
		}
	else {
		// No, get key from the keyboard.
		if(mlputs(MLHome | MLFlush, prompt) == Success)
			(void) ((n != INT_MIN && n <= 0) ? getkey(true, result, false) : getKeySeq(true, result, NULL, false));
		}

	return sess.rtn.status;
	}

// Bind a key sequence to a command.  Get single interactively key if n <= 0.  Return status.
int bindKey(Datum *pRtnVal, int n, Datum **args) {
	ushort extKey;			// Key to bind.
	UnivPtr univ;			// Pointer to the requested command.
	KeyBind *pKey, *pCmd;
	char keyBuf[16];
	char workBuf[WorkBufSize];

	// Get the key or key sequence to bind.
	if(getKeyBind(text15, n, args, &extKey) != Success)
			// "Bind key "
		return sess.rtn.status;
	ektos(extKey, keyBuf, true);

	// If interactive mode, build "progress" prompt.
	if(!(sess.opFlags & OpScript))
		sprintf(workBuf, "%s~#u%s~U %s %s", text15, keyBuf, text339, text158);
						// "Bind key "		"to", "command"
	// Get the command name.
	if(((sess.opFlags & OpScript) && !needSym(s_comma, true)) ||
	 getCF(workBuf, &univ, PtrSysCmdType | PtrUserCmd) != Success || univ.type == PtrNull)
		return sess.rtn.status;

	// Binding a key sequence to a single-key command?
	if((extKey & Prefix) && (univ.type & PtrSysCmdType) && (univ.u.pCmdFunc->attrFlags & CFBind1))
		return rsset(Failure, RSTermAttr, text17, keyBuf, univ.u.pCmdFunc->name);
			// "Cannot bind key sequence ~#u%s~U to '~b%s~B' command"

	// If script mode and not evaluating, bail out here.
	if((sess.opFlags & (OpScript | OpEval)) == OpScript)
		return sess.rtn.status;

	// Interactive mode or evaluating.  Search the binding table to see if the key exists.
	if((pKey = getBind(extKey)) != NULL) {

		// If the key is already bound to this command or function, it's a no op.
		if(pKey->targ.u.pVoid == univ.u.pVoid)
			return sess.rtn.status;

		// If the key is bound to a permanent-bind command and the only such binding, it can't be reassigned.
		if((pKey->targ.type & PtrSysCmdType) && (pKey->targ.u.pCmdFunc->attrFlags & CFPerm) &&
		 ptrEntryCount(&pKey->targ) < 2)
			return rsset(Failure, RSTermAttr, text210, keyBuf, pKey->targ.u.pCmdFunc->name);
					// "~#u%s~U is only binding to core command '~b%s~B' -- cannot delete or reassign"
		}

	// Remove key from cache.
	clearCache(extKey);

	// If binding to a command and the command is maintained in a global variable (for internal use), it can only have one
	// binding at most.
	if((univ.type & PtrSysCmdType) && (univ.u.pCmdFunc->attrFlags & CFUniq)) {
		uint i = 0;

		// Search for an existing binding for the command and remove it.
		if((pCmd = getPtrEntry(&univ)) != NULL)
			unbind(pCmd);

		// Update the key cache.
		do {
			if(coreKeys[i].id == univ.u.pCmdFunc - cmdFuncTable) {
				coreKeys[i].extKey = extKey;
				break;
				}
			} while(++i < CoreKeyCount);
		}

	// Key already in binding table?
	if(pKey != NULL)

		// Yes, change it.
		pKey->targ = univ;
	else
		// Not in table.  Add it.
		newBind(extKey, &univ);

#if MMDebug & Debug_Bind
	fprintf(logfile, "bindKey(%s) DONE.\n", ektos(extKey, workBuf, false));
	dumpBind();
#endif
	return rsset(Success, RSNoFormat, text224);
				// "Binding set"
	}

// Delete a key from the binding table.  Get single keystroke if interactive and n <= 0.  Return Boolean result and, if script
// mode, ignore any "key not bound" error.
int unbindKey(Datum *pRtnVal, int n, Datum **args) {
	ushort extKey;
	KeyBind *pKeyBind;
	char keyBuf[16];

	// Get the key or key sequence to unbind.
	if(getKeyBind(text18, n, args, &extKey) != Success)
			// "Unbind key "
		return sess.rtn.status;

	// Change key to printable form.
	ektos(extKey, keyBuf, true);

	// Search the binding table to see if the key exists.
	if((pKeyBind = getBind(extKey)) == NULL) {

		// Not bound.  Notify user.
		(void) rsset(Success, RSNoWrap | RSTermAttr, text14, keyBuf);
			// "~#u%s~U not bound"
		}
	else {
		// Found it.  If the key is bound to a permanent-bind command and the only such binding, it can't be deleted.
		if((pKeyBind->targ.type & PtrSysCmdType) && (pKeyBind->targ.u.pCmdFunc->attrFlags & CFPerm) &&
		 ptrEntryCount(&pKeyBind->targ) < 2)
			return rsset(Failure, RSTermAttr, text210, keyBuf, pKeyBind->targ.u.pCmdFunc->name);
					// "~#u%s~U is only binding to core command '~b%s~B' -- cannot delete or reassign"

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

// Search key literal table for extended key with matching prefix (Ctrl or none).  Store literal in *pLit and return true if
// found; otherwise, return false.
static bool ektolit(ushort extKey, ushort prefix, char **pLit) {
	KeyLit *pKeyLit = keyLiterals;

	extKey &= prefix | 0xFF;
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
static char *ektos1(ushort extKey, char *dest, bool escTermAttr) {
	ushort c;

	// Function key?
	if(extKey & FKey) {
		dest = stpcpy(dest, "FN");
		*dest++ = extKey & 0xFF;
		goto RetnChk;
		}

	// Print the character using the "control" literals in keyLiterals, if possible.
	if(ektolit(extKey, Ctrl, &dest))
		goto Retn;

	// No literal found.  Control key?
	if(extKey & Ctrl)
		dest = stpcpy(dest, "C-");

	// Print the character using the "non-control" literals in keyLiterals, if possible.
	if(ektolit(extKey, 0, &dest))
		goto Retn;

	// Print raw character, in encoded form if 8-bit.
	if((c = extKey & 0xFF) & 0x80) {
		sprintf(dest, "<%.2hX>", c);
		dest = strchr(dest, '\0');
		}
	else {
		*dest++ = (extKey & Ctrl) ? lowCase[c] : c;
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
char *ektos(ushort extKey, char *dest, bool escTermAttr) {
	char *str;
	static struct PrefKey {
		ushort code;
		CmdFuncId id;
		ushort flag;
		} prefKeyTable[] = {
			{Ctrl | '[', cf_metaPrefix, Meta},
			{Ctrl | 'X', cf_prefix1, Pref1},
			{Ctrl | 'C', cf_prefix2, Pref2},
			{Ctrl | 'H', cf_prefix3, Pref3}};

	// Do the prefix keys first, giving preference to the default values (^[, ^X, ^C, and ^H) in case multiple keys are
	// bound to the same prefix.
	str = dest;
	if(extKey & Prefix) {
		ushort extKey2;
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
		UnivPtr univ = {PtrNull};	// Pointer type not used.
		pPrefKey = prefKeyTable;
		for(;;) {
			if(extKey & pPrefKey->flag) {
				univ.u.pCmdFunc = cmdFuncTable + pPrefKey->id;
				extKey2 = getPtrEntry(&univ)->code;
Print1:
				str = ektos1(extKey2, str, escTermAttr);
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
	ektos1(extKey, str, escTermAttr);
	return dest;
	}

// Return name associated with given KeyBind object or NULL if none.
static const char *getKeyName(KeyBind *pKeyBind) {

	return (pKeyBind == NULL) ? NULL : (pKeyBind->targ.type & PtrSysCmdType) ? pKeyBind->targ.u.pCmdFunc->name :
	 pKeyBind->targ.u.pBuf->bufname;
	}

// Do binding? function per op keyword and return status.  If "Name" (n < 0): return name of command bound to given key binding,
// or nil if none; if "KeyList" (n > 0): return array of key binding(s) bound to given command; or if "Validate" (n < 0): return
// key-lit in standardized form, or nil if it is invalid.
int binding(Datum *pRtnVal, int n, Datum **args) {
	ushort extKey;
	char *op = args[0]->str;
	char *arg2 = args[1]->str;

	// Check op keyword argument and set n accordingly.
	if(strcasecmp(op, "name") == 0)
		n = -1;
	else if(strcasecmp(op, "validate") == 0)
		n = 0;
	else {
		if(strcasecmp(op, "keylist") != 0)
			return rsset(Failure, 0, text447, text451, op);
				// "Invalid %s '%s'", "function option"
		n = 1;
		}

	// Have operation type in n.  Process it.
	if(n <= 0) {
		if(stoek(arg2, &extKey) != Success) {
			if(n < 0)
				goto Retn;
			rsclear(0);
			goto NilRtn;
			}
		if(n == 0) {
			char keyBuf[16];

			if(dsetstr(ektos(extKey, keyBuf, false), pRtnVal) != 0)
				goto LibFail;
			}
		else {
			const char *str = getKeyName(getBind(extKey));
			if(str == NULL) {
				if(extKey >= 0x20 && extKey < 0xFF)
					str = text383;
						// "(self insert)"
				else
					goto NilRtn;
				}
			if(dsetstr(*str == BCmdFuncLead ? str + 1 : str, pRtnVal) != 0)
				goto LibFail;
			}
		}
	else {
		Array *pArray;
		Datum keyLit;
		UnivPtr univ;
		KeyWalk keyWalk = {NULL, NULL};
		KeyBind *pKeyBind;
		char keyBuf[16];

		// Get the command.
		if(!execFind(arg2, OpQuery, PtrSysCmdType | PtrUserCmd, &univ))
			return rsset(Failure, 0, text130, arg2);
				// "No such command '%s'"

		// Get its key binding(s).
		if(makeArray(pRtnVal, 0, &pArray) != Success)
			goto Retn;
		dinit(&keyLit);
		pKeyBind = nextBind(&keyWalk);
		do {
			if(pKeyBind->targ.u.pVoid == univ.u.pVoid) {

				// Add the key sequence.
				if(dsetstr(ektos(pKeyBind->code, keyBuf, false), &keyLit) != 0 ||
				 apush(pArray, &keyLit, AOpCopy) != 0)
LibFail:
					return librsset(Failure);
				}
			} while((pKeyBind = nextBind(&keyWalk)) != NULL);
		dclear(&keyLit);
		}
	goto Retn;
NilRtn:
	dsetnil(pRtnVal);
Retn:
	return sess.rtn.status;
	}

// Create an entry in the execution table.
int execNew(const char *name, UnivPtr *pUniv) {
	HashRec *pHashRec;

	return (pHashRec = hset(execTable, name, NULL, false)) == NULL ||
	 dsetmem((void *) pUniv, sizeof(UnivPtr), pHashRec->pValue) != 0 ? librsset(Failure) : sess.rtn.status;
	}

// Find an executable name (command, function, or alias) in the execution table and return status or Boolean result.
// If the name is found:
//	If op is OpQuery:
//		Set pUniv (if not NULL) to the object and return true if entry type matches selector; otherwise, return false.
//	If op is OpCreate:
//		Return status.
//	If op is OpUpdate:
//		Update entry with *pUniv value and return status.
//	If op is OpDelete:
//		Delete the hash table entry and return status.
// If the name is not found:
//	If op is OpCreate:
//		Create an entry with the given name and *pUniv value, and return status.
//	If op is OpQuery:
//		Return false.
//	If op is OpUpdate or OpDelete:
//		Return FatalError (should not happen).
int execFind(const char *name, ushort op, uint selector, UnivPtr *pUniv) {
	HashRec *pHashRec;
	char myName[] = "execFind";

	// Check the execution table.
	if((pHashRec = hsearch(execTable, name)) != NULL) {

		// Found it.  Check operation type.
		if(op == OpQuery) {
			if(!(univPtr(pHashRec)->type & selector))
				return false;
			if(pUniv != NULL)
				*pUniv = *univPtr(pHashRec);
			return true;
			}
		if(op == OpDelete)
			dfree(hdelete(execTable, name));		// Delete entry and free the storage.
		else if(op == OpUpdate)
			*univPtr(pHashRec) = *pUniv;
		return sess.rtn.status;
		}

	// No such entry exists.  Create one, return false, or fatal error (a bug).
	return (op == OpCreate) ? execNew(name, pUniv) : (op == OpQuery) ? false :
	 rsset(FatalError, 0, text16, myName, name);
		// "%s(): No such entry '%s' to update or delete!"
	}

// Split a macro string into a name and value.  Store name in "name" and return value.
char *macSplit(char *name, const char *macro) {
	char *value = strchr(macro + 1, *macro);
	stplcpy(name, macro + 1, value - macro);
	return value;
	}

// Find a macro on the macro ring, given name (MF_Name flag set) or value (MF_Name flag not set) to match on.  Return
// pointer to slot if found (and offset, if slot not NULL); otherwise, set an error if MF_Required flag set and return NULL.
static RingEntry *macFind(const char *macro, ushort flags, int *slot) {

	if(ringTable[RingIdxMacro].size > 0) {
		char *value;
		int n = 1;
		RingEntry *pEntry = ringTable[RingIdxMacro].pEntry;
		char name[MaxMacroName + 1];
		do {
			--n;
			value = macSplit(name, pEntry->data.str);
			if(flags & MF_Name) {
				if(strcmp(name, macro) == 0)
					goto Found;
				}
			else if(strcmp(value, macro) == 0) {
Found:
				if(slot != NULL)
					*slot = n;
				return pEntry;
				}
			} while((pEntry = pEntry->prev) != ringTable[RingIdxMacro].pEntry);
		}

	// Macro not found.  Set an error if applicable.
	if((flags & (MF_Name | MF_Required)) == (MF_Name | MF_Required))
		(void) rsset(Failure, 0, text395, text466, macro);
			// "No such %s '%s'", "macro"
	return NULL;
	}

// Get a macro name, given result pointer, prompt prefix, default value (which may be NULL), script argument flags,
// operation flags, indirect ring slot pointer (which may be NULL), and slot offset pointer (which also may be NULL.  If
// interactive and user cancels, *pRtnVal is set to nil; otherwise, it is set to the name and *ppEntry and *slot are set to the
// slot pointer and offset if the macro already exists.  Return status.
static int getMacroName(Datum *pRtnVal, const char *prefix, const char *defName, uint argFlags, ushort opFlags,
 RingEntry **ppEntry, int *slot) {
	int n = 1;
	enum {Plain, NotNull, BadName, NameInUse} promptType = Plain;
	RingEntry *pEntry = NULL;
	TermInpCtrl termInpCtrl = {defName, RtnKey, MaxMacroName, NULL};
	const char *prompt;
	char promptBuf[WorkBufSize];

	if(sess.opFlags & OpScript) {

		// Get macro name, check if valid, and truncate it if too long.
		if(funcArg(pRtnVal, argFlags) != Success)
			return sess.rtn.status;
		if(strpbrk(pRtnVal->str, macroDelims) != NULL)
			return rsset(Failure, 0, text477, text471, macroDelims);
					// "%s name cannot contain \"%s\"", "Macro"
		if(strlen(pRtnVal->str) > MaxMacroName)
			pRtnVal->str[MaxMacroName] = '\0';
		goto CheckName;
		}

	for(;;) {
		prompt = promptBuf;
		if(promptType == Plain)
			sprintf(promptBuf, "%s %s", prefix, text466);
					// "macro"
		else if(promptType == NameInUse)
			prompt = text25;
				// "That name is already in use.  New name"
		else {
			if(promptType == BadName)
				sprintf(promptBuf, text477, text471, macroDelims);
					// "%s name cannot contain \"%s\"", "Macro"
			else	// NotNull
				sprintf(promptBuf, text187, text53);
					// "%s cannot be null", "Value"
			sprintf(strchr(promptBuf, '\0'), ".  %s", text478);
							// "New name"
			}
		if(termInp(pRtnVal, prompt, ArgNil1, opFlags & OpCreate ? Term_Attr | Term_C_Macro | Term_C_NoAuto :
		 Term_Attr | Term_C_Macro, &termInpCtrl) != Success)
			return sess.rtn.status;

		if(disnull(pRtnVal))
			promptType = NotNull;
		else if(disnil(pRtnVal)) {
			dsetnil(pRtnVal);
			goto Retn;
			}
		else if(strpbrk(pRtnVal->str, macroDelims) != NULL)
			promptType = BadName;
		else {
CheckName:
			// Got macro name (which is <= MaxMacroName in length).  Check if it already exists.  Note that
			// OpUpdate is set only when creating a macro interactively (endMacro command, which is interactive
			// only), so overwrite confirmation dialogue below will never happen in script mode.
			if((pEntry = macFind(pRtnVal->str, (opFlags & OpCreate) ? MF_Name : MF_Name | MF_Required,
			 &n)) != NULL) {

				// Entry exists.  Confirm overwrite, if applicable.
				if(opFlags & OpUpdate) {		// finishMacro() call.
					bool yep;

					sprintf(promptBuf, text468, text471, pRtnVal->str);
						// "%s '%s' already exists", "Macro"
					sprintf(strchr(promptBuf, '\0'), ".  %s", text84);
									// "Replace"
					if(terminpYN(promptBuf, &yep) != Success)
						return sess.rtn.status;
					if(yep)
						break;
					promptType = Plain;
					}
				else if(opFlags & OpCreate) {		// renameMacro() call.
					if(sess.opFlags & OpScript)
						return rsset(Failure, 0, text181, text471, pRtnVal->str);
								// "%s name '%s' already in use", "Macro"
					promptType = NameInUse;
					}
				else
					break;
				}
			else {
				if(opFlags & OpCreate)
					break;

				// Macro not found.  Return error.
				return sess.rtn.status;
				}
			}
		}
Retn:
	if(!(sess.opFlags & OpScript))
		(void) mlerase(0);
	if(ppEntry != NULL)
		*ppEntry = pEntry;
	if(slot != NULL)
		*slot = n;
	return sess.rtn.status;
	}

// Encode the current macro into pDest in string form using ektos().  Include it's name at beginning if name is defined.
// Return status.
static int encodeMacro(Datum *pDest) {

	// Undefined macro?
	if(curMacro.pMacEnd == NULL)
		dsetnull(pDest);
	else {
		ushort *pKey;
		char *str;
		DFab fab;
		char workBuf[16];

		// Find a delimter that can be used -- a character that is not in the macro name or key bindings.
		// Default to tab.
		str = macroDelims;
		workBuf[0] = '\t';
		do {
			if(!disnil(&curMacro.name) && strchr(curMacro.name.str, *str) != NULL)
				continue;
			pKey = curMacro.pMacBuf;
			do {
				if(*pKey == *str)
					goto Next;
				} while(++pKey < curMacro.pMacEnd);

			// Found.
			workBuf[0] = *str;
			break;
Next:;
			} while(*++str != '\0');

		// Loop through macro keys and translate each into fab with delimiter found in previous step.
		if(dopenwith(&fab, pDest, FabClear) != 0 || (!disnil(&curMacro.name) &&
		 (dputc(workBuf[0], &fab, 0) != 0 || dputs(curMacro.name.str, &fab, 0) != 0)))
			goto LibFail;
		pKey = curMacro.pMacBuf;
		do {
			ektos(*pKey, workBuf + 1, false);
			if(dputs(workBuf, &fab, 0) != 0)
				goto LibFail;
			} while(++pKey < curMacro.pMacEnd);
		if(dclose(&fab, FabStr) != 0)
LibFail:
			return librsset(Failure);
		}

	return sess.rtn.status;
	}

// Increase size of macro buffer.
int growMacro(void) {
	uint n = curMacro.size;
	if((curMacro.pMacBuf = (ushort *) realloc((void *) curMacro.pMacBuf,
	 (curMacro.size += MacroChunk) * sizeof(ushort))) == NULL)
		return rsset(Panic, 0, text94, "growMacro");
			// "%s(): Out of memory!"
	curMacro.pMacSlot = curMacro.pMacBuf + n;
	return sess.rtn.status;
	}

// Clear macro.
static void macClear(void) {

	dsetnil(&curMacro.name);
	curMacro.pMacSlot = curMacro.pMacBuf;
	curMacro.pMacEnd = NULL;
	}

// Return true if a macro is already defined; otherwise, set an error and return false.
static bool macDefined(void) {

	if(curMacro.pMacEnd != NULL && curMacro.pMacEnd > curMacro.pMacBuf)
		return true;
	(void) rsset(Failure, 0, text200, text466);
			// "No %s defined", "macro"
	return false;
	}

// Decode and store a macro into 'macro' global variable (Macro object) from a delimited string containing a name and encoded
// keys.  The first character of the string is the delimiter.  Current macro is assumed to be in the MacStopped state and macro
// to be added is assumed to not be a duplicate.  Return status.
int decodeMacro(char *macro) {
	short delimChar;

	macClear();

	// Get delimiter (first character) and parse string.
	if((delimChar = macro[0]) == '\0' || macro[1] == '\0')
		(void) rsset(Failure, 0, text447, text466, macro);
			// "Invalid %s '%s'", "macro"
	else {
		ushort c, extKey;
		CmdFuncId id;
		UnivPtr univ;
		bool last, first = true;
		Datum *pDatum;
		char *macro1 = macro + 1;

		if(dnewtrack(&pDatum) != 0)
			goto LibFail;

		// Parse tokens and save in macro array.
		while(parseTok(pDatum, &macro1, delimChar) != NotFound) {

			// Convert token string to a key sequence.
			if(*pDatum->str == '\0')
				return rsset(Failure, 0, text447, text254, "");
					// "Invalid %s '%s'", "key literal"
			if(first) {
				// Save macro name.  Truncate if too long.
				if(strlen(pDatum->str) > MaxMacroName)
					pDatum->str[MaxMacroName] = '\0';
				if(dsetstr(pDatum->str, &curMacro.name) != 0)
					goto LibFail;
				first = false;
				continue;
				}

			// Process key literal.
			if(stoek(pDatum->str, &extKey) != Success)
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
				univ.u.pCmdFunc = cmdFuncTable + id;
				c = getPtrEntry(&univ)->code;
				extKey &= ~Prefix;
SaveIt:
				// Save key if room.
				if(curMacro.pMacSlot == curMacro.pMacBuf + curMacro.size && growMacro() != Success)
					goto Retn;
				*curMacro.pMacSlot++ = c;
				} while(!last);
			}

		// Empty macro?
		curMacro.pMacEnd = curMacro.pMacSlot;
		(void) macDefined();
		}
Retn:
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Reset macro (reload from ring) and return current status.
int macReset(void) {

	// Skip this if exiting program.
	if(sess.rtn.status > MinExit) {
		if(ringTable[RingIdxMacro].size > 0) {
			ushort oldFlags;
			short oldStatus;

			// Save and restore status if needed (so that decodeMacro() will not fail).
			rspush(&oldStatus, &oldFlags);
			(void) decodeMacro(ringTable[RingIdxMacro].pEntry->data.str);		// Can't fail.
			rspop(oldStatus, oldFlags);
			}
		else
			macClear();
		}
	return sess.rtn.status;
	}

// Check if macro in MacStopped state (if strict is true), or not in MacRecord state (otherwise).  If so, return false.  If
// not, reset the state to MacStop, call macReset(), set error message, and return true.
bool macBusy(bool strict, const char *fmt, ...) {

	switch(curMacro.state) {
		case MacStop:
			return false;
		case MacPlay:
			if(!strict)
				return false;
		}

	// Invalid macro state... reset it.
	if(curMacro.state == MacRecord)
		sess.cur.pWind->flags |= WFMode;
	curMacro.state = MacStop;
	(void) macReset();

	// Set error message.
	int r;
	va_list varArgList;
	char *str;

	va_start(varArgList, fmt);
	r = vasprintf(&str, fmt, varArgList);
	va_end(varArgList);
	if(r < 0)
		(void) rsset(Panic, 0, text94, "kmBusy");
			// "%s(): Out of memory!"
	else {
		(void) rsset(Failure, RSNoFormat, str);
		free((void *) str);
		}

	return true;
	}

// Begin recording a macro.  Error if not in the MacStopped state.  Set up control variables and return status.
// *Interactive only*
int beginMacro(Datum *pRtnVal, int n, Datum **args) {

	if(macBusy(true, text105, text471))
			// "%s already active, cancelled", "Macro"
		return sess.rtn.status;

	// Initialize controls and start recording.
	macClear();
	curMacro.state = MacRecord;
	windNextIs(NULL)->flags |= WFMode;			// Update mode line of bottom window.
	return rsset(Success, 0, text106, text466);
		// "Begin %s recording", "macro"
	}

// End macro recording.  Error if not in the MacRecord state.  Finalize control variables and return status.  The macro
// name, other error checking, and ring update is done by the caller (execute() function) which has access to the command's key
// binding -- needed to adjust the macro length.  *Interactive only*
int endMacro(Datum *pRtnVal, int n, Datum **args) {

	if(curMacro.state == MacStop)
		(void) rsset(Failure, 0, text107, text471);
			// "%s not active", "Macro"
	else {
		// In MacRecord state (MacPlay not possible).
		curMacro.pMacEnd = curMacro.pMacSlot;
		curMacro.state = MacStop;
		windNextIs(NULL)->flags |= WFMode;		// Update mode line of bottom window.
		}
	return sess.rtn.status;
	}

// Finish macro recording, which includes removing end-macro key sequence, naming the macro, and adding it to the macro
// ring.  Return status.
int finishMacro(KeyBind *pKeyBind) {
	RingEntry *pEntry;
	Datum *pDatum;
	int n = (pKeyBind->code & Prefix) ? 2 : 1;
	curMacro.pMacSlot -= n;
	curMacro.pMacEnd -= n;
	if(!macDefined())
		return sess.rtn.status;

	// Valid macro was recorded.  Check if duplicate.
	if(dnewtrack(&pDatum) != 0)
		return librsset(Failure);
	if(encodeMacro(pDatum) != Success)
		return sess.rtn.status;
	if((pEntry = macFind(pDatum->str, 0, NULL)) != NULL) {
		bool yep;
		char *dupMacro = pEntry->data.str;
		char name[MaxMacroName + 1];
		char promptBuf[128];

		// New macro is a duplicate (by value)... user okay with that?
		macSplit(name, dupMacro);
		sprintf(promptBuf, text470, text471, name);
			// "%s is identical to '%s'", "Macro"
		strcat(promptBuf, text476);
			// ".  Discard"
		if(terminpYN(promptBuf, &yep) != Success || yep)
			return sess.rtn.status;
		}

	// Get macro name.
	if(getMacroName(&curMacro.name, text469, NULL, ArgFirst | ArgNotNull1, OpCreate | OpUpdate, &pEntry, &n) != Success)
				// "Save to"
		return sess.rtn.status;
	if(disnil(&curMacro.name)) {
		(void) macReset();
		return rsset(Success, RSHigh, "%s %s", text471, text472);
					// "Macro", "discarded"
		}

	// It's a keeper... prepend name to macro and add it to macro ring.
	char workBuf[strlen(curMacro.name.str) + strlen(pDatum->str) + 2];
	sprintf(workBuf, "%c%s%s", *pDatum->str, curMacro.name.str, pDatum->str);
	if((pEntry == NULL || rdelete(ringTable + RingIdxMacro, n == 0 ? INT_MIN : n) == Success) &&
	 rsave(ringTable + RingIdxMacro, workBuf, true) == Success)
		(void) rsset(Success, RSForce, "%s %s", text471, text108);
					// "Macro", "saved"
	return sess.rtn.status;
	}

// Load given macro ring entry.
static void macLoad(RingEntry *pEntry) {

	rtop(ringTable + RingIdxMacro, pEntry);
	(void) macReset();
	}

// Perform macro operation per op keyword and return status.  Error if not in the MacStopped state.  Synopsis is:
//   manageMacro 'get'[, opts]		  Get current macro.  Return nil if none, or macro in string form by default.  If n
//					  argument, opts are: 'Split' -> [name, value], 'All' -> [[name, value], ...].
//   manageMacro 'select', name		  Find named macro and load it.  Return true if found, otherwise false.
//   manageMacro 'set', {array | string}  Set a macro.  Return nil on success, otherwise error message (either "duplicate
//					  name..." or "duplicate value...").  Force overwrite or insert if n > 0.
int manageMacro(Datum *pRtnVal, int n, Datum **args) {
	static Option options[] = {
		{"^All", NULL, 0, 0},
		{"^Split", NULL, 0, 0},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text451, false, options};
			// "function option"

	if(macBusy(true, text338, text473, text466, text466))
			// "Cannot %s a %s from a %s, cancelled", "operate on", "macro", "macro"
		goto Retn;

	// Get op argument and check it.
	if(funcArg(pRtnVal, ArgFirst | ArgNotNull1) != Success)
		goto Retn;
	if(strcasecmp(pRtnVal->str, "get") == 0) {
		if(curMacro.pMacEnd == NULL)
			dsetnil(pRtnVal);
		else if(n == INT_MIN)
			(void) encodeMacro(pRtnVal);
		else {
			Array *pArray0, *pArray1;
			Datum *pArrayEl;
			bool all;
			RingEntry *pEntry;
			char *value, name[MaxMacroName + 1];

			// Get options.
			if(parseOpts(&optHdr, NULL, NULL, NULL) != Success)
				goto Retn;
			all = options[0].ctrlFlags & OptSelected;
			if((pArray0 = anew(all ? 0 : 2, NULL)) == NULL)
				goto LibFail;
			agStash(pRtnVal, pArray0);

			// Loop through all items in macro ring if "All" option; otherwise, do just one iteration.
			pEntry = ringTable[RingIdxMacro].pEntry;
			do {
				value = macSplit(name, pEntry->data.str);
				if(all) {
					if((pArray1 = anew(2, NULL)) == NULL ||
					 (pArrayEl = aget(pArray0, pArray0->used, AOpGrow)) == NULL)
						goto LibFail;
					agStash(pArrayEl, pArray1);
					}
				else
					pArray1 = pArray0;
				if(dsetstr(name, pArray1->elements[0]) != 0 || dsetstr(value, pArray1->elements[1]) != 0)
					goto LibFail;
				} while(all && (pEntry = pEntry->prev) != ringTable[RingIdxMacro].pEntry);
			}
		}
	else if(strcasecmp(pRtnVal->str, "select") == 0) {
		RingEntry *pEntry;
		bool result = false;
		if(funcArg(pRtnVal, ArgNotNull1) != Success)
			goto Retn;
		if((pEntry = macFind(pRtnVal->str, MF_Name, NULL)) != NULL) {
			result = true;
			macLoad(pEntry);
			}
		dsetbool(result, pRtnVal);
		}
	else if(strcasecmp(pRtnVal->str, "set") == 0) {
		Datum *pDatum;

		if(dnew(&pDatum) != 0)
			goto LibFail;
		if(funcArg(pDatum, ArgNotNull1 | ArgArray1 | ArgMay) != Success)
			goto Retn;
		if(dtyparray(pDatum)) {		// Array
			Array *pArray = pDatum->u.pArray;
			Datum **ppArrayEl = pArray->elements;
			DFab fab;
			if(pArray->used != 2)
				return rsset(Failure, 0, text97, text247);
						// "Invalid %s argument", "function";
			if(!isStrVal(ppArrayEl[0]) || !isStrVal(ppArrayEl[1]))
				goto Retn;
			if(dopenwith(&fab, pDatum, FabClear) != 0 || dputf(&fab, 0, "%c%s%s", *ppArrayEl[1]->str,
			 ppArrayEl[0]->str, ppArrayEl[1]->str) != 0 || dclose(&fab, FabStr) != 0)
				goto LibFail;
			}

		// Have macro in "pDatum" (in string form).  Save it.
		dsetnil(pRtnVal);
		if(decodeMacro(pDatum->str) != Success)
			goto MacReset;

		RingEntry *pEntry;
		int i;
		char errorBuf[WorkBufSize];

		if((pEntry = macFind(strchr(pDatum->str + 1, *pDatum->str), 0, &i)) != NULL) {
			if(n <= 0) {
				char name[MaxMacroName + 1];

				macSplit(name, pEntry->data.str);
				sprintf(errorBuf, text470, text471, name);
					// "%s is identical to '%s'", "Macro"
				goto Dup;
				}
			goto DelEntry;
			}
		else if(macFind(curMacro.name.str, MF_Name, &i) != NULL) {
			if(n <= 0) {
				sprintf(errorBuf, text468, text471, curMacro.name.str);
					// "%s '%s' already exists", "Macro"
Dup:
				if(dsetstr(errorBuf, pRtnVal) != 0)
					goto LibFail;
MacReset:
				(void) macReset();
				goto Retn;
				}
DelEntry:
			(void) rdelete(ringTable + RingIdxMacro, i == 0 ? INT_MIN : i);		// Can't fail.
			rsclear(0);
			}
		if(encodeMacro(pDatum) == Success)
			(void) rsave(ringTable + RingIdxMacro, pDatum->str, true);
		}
	else
		return rsset(Failure, 0, text447, text451, pRtnVal->str);
			// "Invalid %s '%s'", "function option"
Retn:
	return sess.rtn.status;
LibFail:
	return librsset(Failure);
	}

// Enable execution of a macro abs(n) times (which will be subsequently run by the getkey() function).  Error if not in
// MacStopped state.  If n <= 0, prompt for macro name and run it abs(n) times; othewise, run current macro n times.  Set up the
// control variables and return status.
int xeqMacro(Datum *pRtnVal, int n, Datum **args) {

	if(macBusy(true, text105, text471))
			// "%s already active, cancelled", "Macro"
		return sess.rtn.status;
	if(!macDefined())
		return sess.rtn.status;
	if(n == INT_MIN)
		n = 1;
	else if(n <= 0) {
		RingEntry *pEntry;

		if(getMacroName(pRtnVal, text117, n == 0 ? curMacro.name.str : NULL, ArgFirst | ArgNotNull1, OpDelete, &pEntry,
				// "Execute"
		 NULL) != Success || disnil(pRtnVal))
			return sess.rtn.status;
		dsetnil(pRtnVal);
		macLoad(pEntry);
		n = -n;
		}

	// It's a go... enable playback.
	curMacro.n = n;				// Remember how many times to execute...
	curMacro.state = MacPlay;		// switch to "play" mode...
	curMacro.pMacSlot = curMacro.pMacBuf;	// at the beginning.

	return sess.rtn.status;
	}

// Rename a macro and return status.  Error if not in MacStopped state.
int renameMacro(Datum *pRtnVal, int n, Datum **args) {
	RingEntry *pEntry;
	Datum *pDatum;

	if(macBusy(true, text338, text275, text466, text466) || !macDefined())
			// "Cannot %s a %s from a %s, cancelled", "rename", "macro", "macro"
		return sess.rtn.status;

	// Get name of macro to rename.
	if(dnew(&pDatum) != 0)
		return librsset(Failure);
	if(getMacroName(pDatum, text29, NULL, ArgFirst | ArgNotNull1, OpDelete, &pEntry, NULL) != Success || disnil(pDatum))
			// "Rename"
		return sess.rtn.status;

	// Get new name.
	if(getMacroName(pRtnVal, text339, NULL, ArgNotNull1, OpCreate, NULL, NULL) != Success || disnil(pRtnVal))
			// "to"
		return sess.rtn.status;

	// Got old and new names... rename macro.
	char *value = strchr(pEntry->data.str + 1, *pEntry->data.str);
	char macro[strlen(value) + MaxMacroName + 2];
	sprintf(macro, "%c%s%s", *value, pRtnVal->str, value);
	if(dsetstr(macro, &pEntry->data) != 0)
		return librsset(Failure);
	if(n != INT_MIN)
		macLoad(pEntry);
	else if(pEntry == ringTable[RingIdxMacro].pEntry)
		(void) macReset();		// Can't fail.

	return rsset(Success, 0, "%s %s", text471, text467);
					// "Macro", "renamed"
	}

// Delete a macro by name or n value and return status.  Error if not in MacStopped state.
int delMacro(Datum *pRtnVal, Ring *pRing, int n) {

	if(macBusy(true, text338, text263, text466, text466))
			// "Cannot %s a %s from a %s, cancelled", "delete", "macro", "macro"
		return sess.rtn.status;
	if(pRing->size == 0)
		return rsset(Failure, 0, text202, pRing->ringName);
				// "%s ring is empty"
	if(n == INT_MIN) {
		if(getMacroName(pRtnVal, text26, curMacro.name.str, ArgNotNull1, OpDelete, NULL, &n) != Success ||
				// "Delete"
		 disnil(pRtnVal))
			return sess.rtn.status;
		dsetnil(pRtnVal);
		if(n == 0)
			n = 1;
		}
	if(rdelete(pRing, n) == Success)
		if(pRing->size == 0 || n == 1)
			(void) macReset();

	return sess.rtn.status;
	}
