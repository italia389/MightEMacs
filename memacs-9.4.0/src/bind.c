// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// bind.c	Key binding routines for MightEMacs.

#include "os.h"
#include <stdarg.h>
#include "std.h"
#include "cmd.h"
#include "exec.h"
#include "search.h"

// Make selected global definitions local.
#define DATAbind
#include "bind.h"

/*** Local declarations ***/

// Recognized keywords in key binding literals.
static struct kwlit {
	char *keywd;		// Keyword.
	ushort ek;		// Character (extended key).
	} kwtab[] = {
		{"SPC",' '},
		{"TAB",Ctrl | 'I'},
		{"ESC",Ctrl | '['},
		{"RTN",Ctrl | 'M'},
		{"DEL",Ctrl | '?'},
		{NULL,'\0'}};

// Convert extended key to ordinal character value.  Collapse the Ctrl flag back into the ASCII code.  If "extend" is true,
// return function key values (94 possibilities) in range 128..(128+93), S-TAB as 128+94, and shifted function key values in
// range (128+94+1)..(128+94+1+93); otherwise, 0..127 like other characters.
short ektoc(ushort ek,bool extend) {

	// Do special cases first.
	if((ek & (Ctrl | 0xFF)) == (Ctrl | ' '))
		return 0;				// Null char.
	if((ek & (Shft | Ctrl | 0xFF)) == (Shft | Ctrl | 'I'))
		return 128 + 94;			// S-TAB.

	// Now do control keys and function keys.
	short c = ek & 0xFF;
	if(ek & Ctrl)
		return c ^ 0x40;			// Actual control char.
	if((ek & FKey) && extend)
		c += (ek & Shft) ? (128 + 94 + 1 - 33)	// FNx character in range ! .. ~.
		 : (128 - 33);
	return c;
	}

// Walk through all key binding lists and return next binding in sequence, or NULL if none left.  If kwp->hkp is NULL, reset
// to beginning and return first binding found.
KeyBind *nextbind(KeyWalk *kwp) {
	KeyVect *kvp = kwp->kvp;
	KeyBind *kbind = kwp->kbind;

	if(kvp == NULL)
		kbind = (KeyBind *) (kwp->kvp = kvp = keytab);

	for(;;) {
		if(kbind == (KeyBind *) kvp + NKeyVect) {
			if(++kvp == keytab + NKeyTab)
				return NULL;
			kbind = (KeyBind *) (kwp->kvp = kvp);
			}
		if(kbind->k_code != 0)
			break;
		++kbind;
		}
	kwp->kbind = kbind + 1;		
	return kbind;
	}

// Return the number of entries in the binding table that match the given universal pointer.
static int pentryct(UnivPtr *univp) {
	int count = 0;
	KeyWalk kw = {NULL,NULL};
	KeyBind *kbind = nextbind(&kw);

	// Search for existing bindings for the command or macro.
	do {
		if(kbind->k_targ.u.p_void == univp->u.p_void)
			++count;
		} while((kbind = nextbind(&kw)) != NULL);

	return count;
	}

// Scan the binding table for the first entry that matches the given universal pointer and return it, or NULL if none found.
KeyBind *getpentry(UnivPtr *univp) {
	KeyWalk kw = {NULL,NULL};
	KeyBind *kbind = nextbind(&kw);

	// Search for an existing binding for the command or macro.
	do {
		if(kbind->k_targ.u.p_void == univp->u.p_void)
			return kbind;
		} while((kbind = nextbind(&kw)) != NULL);

	return NULL;
	}

// Return binding slot for given extended key.
static KeyBind *bindslot(ushort ek) {
	uint i;

	// Set i to target key vector.
	switch(ek & Prefix) {
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

	return (KeyBind *) (keytab + i) + ektoc(ek,true);
	}

// Look up a key binding in the binding table, given extended key.
KeyBind *getbind(ushort ek) {
	KeyBind *kbind = bindslot(ek);
	if(kbind->k_code != 0)
		return kbind;

	// No such binding.
	return NULL;
	}

// Add an extended key to the binding table.
static void newcbind(ushort ek,UnivPtr *univp) {
	KeyBind *kbind = bindslot(ek);

	// Set keycode and the command or buffer pointer.
	kbind->k_code = ek;
	kbind->k_targ = *univp;
	}

// Get binding of given extended key and return prefix flag if it's bound to a prefix command; otherwise, zero.
static ushort findPrefix(ushort ek) {
	KeyBind *kbind = getbind(ek);
	if(kbind != NULL && kbind->k_targ.p_type == PtrPseudo) {
		CmdFunc *cfp = kbind->k_targ.u.p_cfp;
		if(cfp->cf_aflags & CFPrefix) {
			enum cfid id = (enum cfid) (cfp - cftab);
			return id == cf_metaPrefix ? Meta : id == cf_prefix1 ? Pref1 : id == cf_prefix2 ? Pref2 : Pref3;
			}
		}
	return 0;
	}

// Get one value from the coded string in *keylit.  Store result in *ekp, update *keylit, and return true if successful;
// otherwise, false.
static bool stoek1(char **keylit,ushort *ekp) {
	ushort c;
	ushort ek = 0;
	struct kwlit *kwl;
	char *klit = *keylit;

	// Loop until hit null or space.
	for(;;) {
		if(*klit == '\0')
			return false;

		// Prefix?
		if(klit[1] == '-') {
			switch(klit[0]) {
				case 'C':			// Ctrl prefix.
				case 'c':
					++klit;
					goto CtrlPref;
				case 'M':			// Meta prefix.
				case 'm':
					if(ek & Meta)
						return false;
					ek |= Meta;
					break;
				case 'S':			// SHIFT prefix.
				case 's':
					if(ek & Shft)
						return false;
					ek |= Shft;
					break;
				default:
					return false;
				}
			klit += 2;
			}

		// Alternate control character form?
		else if((c = klit[0]) == '^') {
			if(klit[1] == '\0' || klit[1] == ' ')	// Bare '^'?
				goto Vanilla;			// Yes, take it literally
CtrlPref:
			if(ek & Ctrl)
				return false;
			ek |= Ctrl;
			++klit;
			}

		// Function key?
		else if(strncasecmp(klit,"fn",2) == 0) {
			if(ek & FKey)
				return false;
			ek |= FKey;
			klit += 2;
			}

		// Space or keyword?
		else {
			if(c == ' ')
				return false;
			kwl = kwtab;
			do {
				if(strncasecmp(klit,kwl->keywd,3) == 0) {
					klit += 2;
					c = kwl->ek & 0xFF;
					ek |= kwl->ek & ~0xFF;
					goto Vanilla;
					}
				} while((++kwl)->keywd != NULL);

			// Not a keyword.  Literal control character? (Boo, hiss.)
			if(c < ' ' || c == 0x7F) {
				if(ek & Ctrl)			// Duplicate?
					return false;		// Yes, error.
				ek |= Ctrl;
				c ^= '@';			// Convert literal character to visible equivalent.
				++klit;
				}

			// Must be a vanilla character; that is, a printable (except space) or 8-bit character.  Move past it
			// and finalize value.
			else
Vanilla:
				++klit;

			// Character is in c and prefix flag(s) are in ek (if any).  Do sanity checks.
			if(*klit != '\0' && *klit != ' ')		// Not end of value?
				return false;				// Yes, error.
			if(ek != (Ctrl | Shft) ||
			 (c != 'i' && c != 'I')) {			// Skip S-TAB special case.
				if(ek & Ctrl) {
					if(ek & Shft)			// Error if S-C-.
						return false;
					if(c == '@')			// C-@ or ^@ ?
						c = ' ';		// Yes, change back to a space.
					else if(c != ' ' && (c < '?' || c == '`' || c > 'z'))
						return false;		// Invalid character following C- or ^.
					}
				if((ek & (FKey | Shft)) == Shft) {	// If SHIFT prefix without FNx...
					if(isletter(c)) {		// error if printable char. follows and not a letter.
						c = upcase[c];
						ek &= ~Shft;
						}
					else if((c >= ' ' && c < 'A') || (c > 'Z' && c < 'a') || (c > 'z' && c <= '~'))
						return false;
					}
				}

			// Make sure it's upper case if used with C- or ^.
			if((ek & (FKey | Ctrl)) == Ctrl)
				c = upcase[c];

			// Success.  Return results.
			*keylit = klit;
			*ekp = ek | c;
			break;
			}
		}
	return true;
	}

// Convert a coded string to an extended key code.  Set *result to zero if keylit is invalid.  Return status.
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
int stoek(char *keylit,ushort *result) {
	ushort ek1;			// One key.
	ushort ek = 0;			// Final extended key.
	ushort kct = 0;
	char *klit = keylit;

	// Parse it up.
	for(;;) {
		// Get a value into ek1.
		if(!stoek1(&klit,&ek1))
			goto BadLit;
		kct += (ek1 & Meta) ? 2 : 1;

		// Success.  Done?
		if(*klit == ' ') {
			if(kct == 2)
				goto BadLit;

			// Have first of two values.  If not a prefix key, return an error; otherwise, set flag.
			ushort flag = findPrefix(ek1);
			if(flag == 0)
				goto BadLit;
			ek = flag;
			++klit;
			}
		else {
			if(*klit != '\0') {
BadLit:
				*result = 0;
				return rcset(Failure,0,text447,text254,keylit);
						// "Invalid %s '%s'","key literal"
				}
			if(kct == 1)
				ek = ek1;
			else
				ek |= ek1;
			break;
			}			
		}

	*result = ek;
	return rc.status;
	}

#if MMDebug & Debug_Bind
// Dump binding table.
static void dumpbind(void) {
	KeyVect *kvp = keytab;
	KeyBind *kbind;
	int i = 0;
	char keybuf[16];

	do {
		fprintf(logfile,"\nBINDING LIST #%d\n",i++);
		kbind = (KeyBind *) kvp;
		do {
			if(kbind->k_code == 0)
				continue;
			fprintf(logfile,"    %8s   ->   %s\n",ektos(kbind->k_code,keybuf,false),
			(kbind->k_targ.p_type & PtrCmdType) ? kbind->k_targ.u.p_cfp->cf_name : kbind->k_targ.u.p_buf->b_bname);
			} while(++kbind < (KeyBind *) kvp + NKeyVect);
		} while(++kvp < keytab + NKeyTab);
	}
#endif

// Clear extended key from key cache, if present.
static void clearcache(ushort ek) {
	uint i = 0;

	do {
		if(corekeys[i].ek == ek) {
			corekeys[i].ek = 0;
			break;
			}
		} while(++i < NCoreKeys);
	}

// Clear given key entry in the binding table.
void unbindent(KeyBind *kbind) {

	// Remove key from cache.
	clearcache(kbind->k_code);

	// Clear the entry.
	kbind->k_code = 0;
	kbind->k_targ.u.p_void = NULL;
	}

// Load all the built-in command key bindings.  Return status.
int loadbind(void) {
	UnivPtr univ;
	KeyItem *kip = keyitems;

	do {
		univ.p_type = ((univ.u.p_cfp = cftab + kip->ki_id)->cf_aflags & CFHidden) ? PtrPseudo : PtrCmd;
		newcbind(kip->ki_code,&univ);
		} while((++kip)->ki_code != 0);

	return rc.status;
	}

#if 0
// Dump execution table to log file.
static void dumpexec(void) {
	UnivPtr *univp;
	HashRec **hrecp0,**hrecpz;
	HashRec **hrecp = NULL;
	(void) hsort(exectab,hcmp,&hrecp);
	hrecpz = (hrecp0 = hrecp) + exectab->recCount;
	do {
		univp = univptr(*hrecp);
		fprintf(logfile,"%-20s%.4hX\n",(*hrecp)->key,univp->p_type);
		} while(++hrecp < hrecpz);
	free((void *) hrecp0);
	}
#endif

// Get command, function, or macro name per selector flags.  Store pointer in *univp.  If interactive mode, pass prmt to
// getcfam().  Return status.
int getcfm(char *prmt,UnivPtr *univp,uint selector) {
	char *emsg = (selector & PtrFunc) ? text312 : (selector & PtrCmd) ? text130 : text116;
		// "No such command, function, or macro '%s'","No such command or macro '%s'","No such macro '%s'"

	if(si.opflags & OpScript) {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(!(si.opflags & OpEval)) {
			univp->p_type = PtrNul;
			univp->u.p_void = NULL;
			}
		else if(!execfind(last->p_tok.d_str,OpQuery,selector & PtrMacro ? selector | PtrMacro : selector,univp))
			return rcset(Failure,0,emsg,last->p_tok.d_str);
		(void) getsym();
		}
	else
		(void) getcfam(prmt,selector | Term_Attr,univp,emsg);
	return rc.status;
	}

// Get a key binding (using given prompt if interactive) and save in *result.  If n <= 0, get one key only; otherwise, get a
// key sequence.  Return status.
int getkb(char *prmt,int n,Datum **argv,ushort *result) {

	// Script mode?
	if(si.opflags & OpScript) {

		// Yes, process argument.
		if(si.opflags & OpEval)
			(void) stoek(argv[0]->d_str,result);
		}
	else {
		// No, get key from the keyboard.
		if(mlputs(MLHome | MLFlush,prmt) == Success)
			(void) ((n != INT_MIN && n <= 0) ? getkey(true,result,false) : getkseq(true,result,NULL,false));
		}

	return rc.status;
	}

// Bind a key sequence to a command or macro.  Get single interactively key if n <= 0.  Return status.
int bindKeyCM(Datum *rval,int n,Datum **argv) {
	ushort ek;			// Key to bind.
	UnivPtr univ;			// Pointer to the requested command or macro.
	KeyBind *k_kdp,*c_kdp;
	char keybuf[16];
	char wkbuf[NWork + 1];

	// Get the key or key sequence to bind.
	if(getkb(text15,n,argv,&ek) != Success)
			// "Bind key "
		return rc.status;
	ektos(ek,keybuf,true);

	// If interactive mode, build "progress" prompt.
	if(!(si.opflags & OpScript))
		sprintf(wkbuf,"%s%c%c%c%s%c%c %s %s",text15,AttrSpecBegin,AttrAlt,AttrULOn,keybuf,AttrSpecBegin,AttrULOff,
		 text339,text267);
			// "Bind key ","to","command or macro"

	// Get the command or macro name.
	if(((si.opflags & OpScript) && !needsym(s_comma,true)) ||
	 getcfm(wkbuf,&univ,PtrCmdType | PtrMacro_O) != Success || univ.p_type == PtrNul)
		return rc.status;

	// Binding a key sequence to a single-key command?
	if((ek & Prefix) && (univ.p_type & PtrCmdType) && (univ.u.p_cfp->cf_aflags & CFBind1))
		return rcset(Failure,RCTermAttr,text17,keybuf,univ.u.p_cfp->cf_name);
			// "Cannot bind key sequence ~#u%s~U to '~b%s~B' command"

	// Binding to a constrained macro?
	if(univ.p_type == PtrMacro_C)
		return rcset(Failure,RCTermAttr,"%s%s%s%s '%c%c%s%c%c'",text418,text416,text417,text414,
		 AttrSpecBegin,AttrBoldOn,univ.u.p_buf->b_bname + 1,AttrSpecBegin,AttrBoldOff);
							// "Key binding"," not allowed"," on ","constrained macro"

	// If script mode and not evaluating, bail out here.
	if((si.opflags & (OpScript | OpEval)) == OpScript)
		return rc.status;

	// Interactive mode or evaluating.  Search the binding table to see if the key exists.
	if((k_kdp = getbind(ek)) != NULL) {

		// If the key is already bound to this command or macro, it's a no op.
		if(k_kdp->k_targ.u.p_void == univ.u.p_void)
			return rc.status;

		// If the key is bound to a permanent-bind command and the only such binding, it can't be reassigned.
		if((k_kdp->k_targ.p_type & PtrCmdType) && (k_kdp->k_targ.u.p_cfp->cf_aflags & CFPerm) &&
		 pentryct(&k_kdp->k_targ) < 2)
			return rcset(Failure,RCTermAttr,text210,keybuf,k_kdp->k_targ.u.p_cfp->cf_name);
					// "~#u%s~U is only binding to core command '~b%s~B' -- cannot delete or reassign"
		}

	// Remove key from cache.
	clearcache(ek);

	// If binding to a command and the command is maintained in a global variable (for internal use), it can only have one
	// binding at most.
	if((univ.p_type & PtrCmdType) && (univ.u.p_cfp->cf_aflags & CFUniq)) {
		uint i = 0;

		// Search for an existing binding for the command and remove it.
		if((c_kdp = getpentry(&univ)) != NULL)
			unbindent(c_kdp);

		// Update the key cache.
		do {
			if(corekeys[i].id == univ.u.p_cfp - cftab) {
				corekeys[i].ek = ek;
				break;
				}
			} while(++i < NCoreKeys);
		}

	// Key already in binding table?
	if(k_kdp != NULL)

		// Yes, change it.
		k_kdp->k_targ = univ;
	else
		// Not in table.  Add it.
		newcbind(ek,&univ);

#if MMDebug & Debug_Bind
	fprintf(logfile,"bindKeyCM(%s) DONE.\n",ektos(ek,wkbuf,false));
	dumpbind();
#endif
	return (si.opflags & OpScript) ? rc.status : rcset(Success,RCNoFormat,text224);
									// "Binding set"
	}

// Delete a key from the binding table.  Get single keystroke if interactive and n <= 0.  Return Boolean result and, if script
// mode, ignore any "key not bound" error.
int unbindKey(Datum *rval,int n,Datum **argv) {
	ushort ek;
	KeyBind *kbind;
	char keybuf[16];

	// Get the key or key sequence to unbind.
	if(getkb(text18,n,argv,&ek) != Success)
			// "Unbind key "
		return rc.status;

	// Change key to printable form.
	ektos(ek,keybuf,true);

	// Search the binding table to see if the key exists.
	if((kbind = getbind(ek)) == NULL) {

		// Not bound.  Notify user if interactive.
		if(!(si.opflags & OpScript))
			(void) rcset(Success,RCNoWrap | RCTermAttr,text14,keybuf);
				// "~#u%s~U not bound"
		}
	else {
		// Found it.  If the key is bound to a permanent-bind command and the only such binding, it can't be deleted.
		if((kbind->k_targ.p_type & PtrCmdType) && (kbind->k_targ.u.p_cfp->cf_aflags & CFPerm) &&
		 pentryct(&kbind->k_targ) < 2)
			return rcset(Failure,RCTermAttr,text210,keybuf,kbind->k_targ.u.p_cfp->cf_name);
					// "~#u%s~U is only binding to core command '~b%s~B' -- cannot delete or reassign"

		// It's a go... unbind it.
		unbindent(kbind);

		// Print key literal if interactive (following prompt string).
		if(!(si.opflags & OpScript))
			(void) mlprintf(MLTermAttr | MLFlush,"%c%c%c%s%c%c",
			 AttrSpecBegin,AttrAlt,AttrULOn,keybuf,AttrSpecBegin,AttrULOff);
		}

	// Return Boolean result.
	dsetbool(kbind != NULL,rval);

	return rc.status;
	}

// Search kwtab for extended key with matching flag.  Store literal in *strp and return true if found; otherwise, return false.
static bool ectol(ushort ek,ushort flag,char **strp) {
	ushort c;
	struct kwlit *kwl = kwtab;

	// Print the character using the Ctrl or non-Ctrl literal in kwtab, if possible.
	c = ek & (flag | 0xFF);
	do {
		if((flag == 0 || (kwl->ek & Ctrl)) && kwl->ek == c) {
			*strp = stpcpy(*strp,kwl->keywd);
			return true;
			}
		} while((++kwl)->keywd != NULL);
	return false;
	}

// Print character from an extended key to str and return pointer to trailing null.  Handle Ctrl and FKey flags.  Tilde (~)
// characters (AttrSpecBegin) of terminal attribute sequences are escaped if escTermAttr is true.
static char *ektos1(ushort ek,char *str,bool escTermAttr) {
	ushort c;

	// Function key?
	if(ek & FKey) {
		str = stpcpy(str,"FN");
		*str++ = ek & 0xFF;
		goto RetnChk;
		}

	// Print the character using the "control" literals in kwtab, if possible.
	if(ectol(ek,Ctrl,&str))
		goto Retn;

	// No literal found.  Control key?
	if(ek & Ctrl)
		str = stpcpy(str,"C-");

	// Print the character using the "non-control" literals in kwtab, if possible.
	if(ectol(ek,0,&str))
		goto Retn;

	// Print raw character, in encoded form if 8-bit.
	if((c = ek & 0xFF) & 0x80) {
		sprintf(str,"<%.2X>",c);
		str = strchr(str,'\0');
		}
	else {
		*str++ = (ek & Ctrl) ? lowcase[c] : c;
RetnChk:
		if(escTermAttr && str[-1] == AttrSpecBegin)
			*str++ = AttrSpecBegin;
Retn:
		*str = '\0';
		}

	return str;
	}

// Encode an extended key to a printable string, save result in dest, and return it.  Tilde (~) characters (AttrSpecBegin) of
// terminal attribute sequences are escaped if escTermAttr is true.
char *ektos(ushort ek,char *dest,bool escTermAttr) {
	char *str;
	static struct pkey {
		ushort code;
		enum cfid id;
		ushort flag;
		} pkeys[] = {
			{Ctrl | '[',cf_metaPrefix,Meta},
			{Ctrl | 'X',cf_prefix1,Pref1},
			{Ctrl | 'C',cf_prefix2,Pref2},
			{Ctrl | 'H',cf_prefix3,Pref3},
			{0,0,0}};

	// Do the prefix keys first, giving preference to the default values (^[, ^X, ^C, and ^H) in case multiple keys are
	// bound to the same prefix.
	str = dest;
	if(ek & Prefix) {
		ushort ek2;
		struct pkey *pkp = pkeys;

		do {
			if(ek & pkp->flag) {
				if(findPrefix(pkp->code) == pkp->flag) {
					ek2 = pkp->code;
					goto Print1;
					}
				break;
				}
			} while((++pkp)->code != 0);

		// Default prefix key binding not found.  Find first binding in table instead.
		UnivPtr univ = {PtrNul};	// Pointer type not used.
		pkp = pkeys;
		for(;;) {
			if(ek & pkp->flag) {
				univ.u.p_cfp = cftab + pkp->id;
				ek2 = getpentry(&univ)->k_code;
Print1:
				str = ektos1(ek2,str,escTermAttr);
				*str++ = ' ';
				break;
				}
			++pkp;
			}
		}

	// Print any shift prefix literal.
	if(ek & Shft)
		str = stpcpy(str,"S-");

	// Print the base character and return result.
	ektos1(ek,str,escTermAttr);
	return dest;
	}

// Return name associated with given KeyBind object or NULL if none.
char *getkname(KeyBind *kbind) {

	return (kbind == NULL) ? NULL : (kbind->k_targ.p_type & PtrCmdType) ? kbind->k_targ.u.p_cfp->cf_name :
	 (kbind->k_targ.p_type == PtrMacro_O) ? kbind->k_targ.u.p_buf->b_bname : NULL;
	}

// Do binding? function.  If default n: return name of command or macro bound to given key binding, or nil if none; if n > 0:
// return array of key binding(s) bound to given command or macro; or n <= 0: return key-lit in standardized form, or nil if
// it is invalid.  Return status.
int binding(Datum *rval,int n,Datum **argv) {
	ushort ek;
	char *op = argv[0]->d_str;
	char *arg2 = argv[1]->d_str;

	// Check op keyword argument and set n accordingly.
	if(strcasecmp(op,"name") == 0)
		n = -1;
	else if(strcasecmp(op,"validate") == 0)
		n = 0;
	else {
		if(strcasecmp(op,"keylist") != 0)
			return rcset(Failure,0,text447,text449,op);
				// "Invalid %s '%s'","op keyword"
		n = 1;
		}

	// Have operation type in n.  Process it.
	if(n <= 0) {
		if(stoek(arg2,&ek) != Success) {
			if(n < 0)
				goto Retn;
			rcclear(0);
			goto NilRtn;
			}
		if(n == 0) {
			char keybuf[16];

			if(dsetstr(ektos(ek,keybuf,false),rval) != 0)
				goto LibFail;
			}
		else {
			char *str = getkname(getbind(ek));
			if(str == NULL) {
				if(ek >= 0x20 && ek < 0xFF)
					str = text383;
						// "(self insert)"
				else
					goto NilRtn;
				}
			if(dsetstr(*str == SBMacro ? str + 1 : str,rval) != 0)
				goto LibFail;
			}
		}
	else {
		Array *ary;
		Datum keylit;
		UnivPtr univ;
		KeyWalk kw = {NULL,NULL};
		KeyBind *kbind;
		char keybuf[16];

		// Get the command or macro.
		if(!execfind(arg2,OpQuery,PtrCmdType | PtrMacro,&univ))
			return rcset(Failure,0,text130,arg2);
				// "No such command or macro '%s'"

		// Get its key binding(s).
		if(mkarray(rval,&ary) != Success)
			goto Retn;
		dinit(&keylit);
		kbind = nextbind(&kw);
		do {
			if(kbind->k_targ.u.p_void == univ.u.p_void) {

				// Add the key sequence.
				if(dsetstr(ektos(kbind->k_code,keybuf,false),&keylit) != 0 || apush(ary,&keylit,true) != 0)
LibFail:
					return librcset(Failure);
				}
			} while((kbind = nextbind(&kw)) != NULL);
		dclear(&keylit);
		}
	goto Retn;
NilRtn:
	dsetnil(rval);
Retn:
	return rc.status;
	}

// Create an entry in the execution table.
int execnew(char *name,UnivPtr *univp) {
	HashRec *hrec;

	return (hrec = hset(exectab,name,NULL,false)) == NULL || dsetblob((void *) univp,sizeof(UnivPtr),hrec->value) != 0 ?
	 librcset(Failure) : rc.status;
	}

// Find an executable name (command, function, alias, or macro) in the execution table and return status or Boolean result.
// If the name is found:
//	If op is OpQuery:
//		Set univp (if not NULL) to the object and return true if entry type matches selector; otherwise, return false.
//	If op is OpCreate:
//		Return status.
//	If op is OpUpdate:
//		Update entry with *univp value and return status.
//	If op is OpDelete:
//		Delete the hash table entry and return status.
// If the name is not found:
//	If op is OpCreate:
//		Create an entry with the given name and *univp value, and return status.
//	If op is OpQuery:
//		Return false.
//	If op is OpUpdate or OpDelete:
//		Return FatalError (should not happen).
int execfind(char *name,ushort op,uint selector,UnivPtr *univp) {
	HashRec *hrec;
	char myname[] = "execfind";

	// Check the execution table.
	if((hrec = hsearch(exectab,name)) != NULL) {

		// Found it.  Check operation type.
		if(op == OpQuery) {
			if(!(univptr(hrec)->p_type & selector))
				return false;
			if(univp != NULL)
				*univp = *univptr(hrec);
			return true;
			}
		if(op == OpDelete)
			ddelete(hdelete(exectab,name));		// Delete entry and free the storage.
		else if(op == OpUpdate)
			*univptr(hrec) = *univp;
		return rc.status;
		}

	// No such entry exists.  Create one, return false, or fatal error (a bug).
	return (op == OpCreate) ? execnew(name,univp) : (op == OpQuery) ? false :
	 rcset(FatalError,0,text16,myname,name);
		// "%s(): No such entry '%s' to update or delete!"
	}
