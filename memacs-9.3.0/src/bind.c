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

// Recognized keywords in string-encoded key bindings.
static struct clit {
	char *kw;		// Keyword.
	ushort ek;		// Character (extended key).
	} ltab[] = {
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
	KeyBind *kbp = kwp->kbp;

	if(kvp == NULL)
		kbp = (KeyBind *) (kwp->kvp = kvp = keytab);

	for(;;) {
		if(kbp == (KeyBind *) kvp + NKeyVect) {
			if(++kvp == keytab + NKeyTab)
				return NULL;
			kbp = (KeyBind *) (kwp->kvp = kvp);
			}
		if(kbp->k_code != 0)
			break;
		++kbp;
		}
	kwp->kbp = kbp + 1;		
	return kbp;
	}

// Return the number of entries in the binding table that match the given universal pointer.
static int pentryct(UnivPtr *univp) {
	int count = 0;
	KeyWalk kw = {NULL,NULL};
	KeyBind *kbp = nextbind(&kw);

	// Search for existing bindings for the command or macro.
	do {
		if(kbp->k_targ.u.p_voidp == univp->u.p_voidp)
			++count;
		} while((kbp = nextbind(&kw)) != NULL);

	return count;
	}

// Scan the binding table for the first entry that matches the given universal pointer and return it, or NULL if none found.
KeyBind *getpentry(UnivPtr *univp) {
	KeyWalk kw = {NULL,NULL};
	KeyBind *kbp = nextbind(&kw);

	// Search for an existing binding for the command or macro.
	do {
		if(kbp->k_targ.u.p_voidp == univp->u.p_voidp)
			return kbp;
		} while((kbp = nextbind(&kw)) != NULL);

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
	KeyBind *kbp = bindslot(ek);
	if(kbp->k_code != 0)
		return kbp;

	// No such binding.
	return NULL;
	}

// Add an extended key to the binding table.
static void newcbind(ushort ek,UnivPtr *univp) {
	KeyBind *kbp = bindslot(ek);

	// Set keycode and the command or buffer pointer.
	kbp->k_code = ek;
	kbp->k_targ = *univp;
	}

// Get binding of given extended key and return prefix flag if it's bound to a prefix command; otherwise, zero.
static ushort findPrefix(ushort ek) {
	KeyBind *kbp = getbind(ek);
	if(kbp != NULL && kbp->k_targ.p_type == PtrPseudo) {
		CmdFunc *cfp = kbp->k_targ.u.p_cfp;
		if(cfp->cf_aflags & CFPrefix) {
			enum cfid id = (enum cfid) (cfp - cftab);
			return id == cf_metaPrefix ? Meta : id == cf_prefix1 ? Pref1 : id == cf_prefix2 ? Pref2 : Pref3;
			}
		}
	return 0;
	}

// Get one value from the coded string in *keylitp.  Store result in *ekp, update *keylitp, and return true if successful;
// otherwise, false.
static bool stoek1(char **keylitp,ushort *ekp) {
	ushort c;
	ushort ek = 0;
	struct clit *ltabp;
	char *klit = *keylitp;

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
			ltabp = ltab;
			do {
				if(strncasecmp(klit,ltabp->kw,3) == 0) {
					klit += 2;
					c = ltabp->ek & 0xFF;
					ek |= ltabp->ek & ~0xFF;
					goto Vanilla;
					}
				} while((++ltabp)->kw != NULL);

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
			*keylitp = klit;
			*ekp = ek | c;
			break;
			}
		}
	return true;
	}

// Convert a coded string to an extended key code.  Set *resultp to zero if keylit is invalid.  Return status.
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
int stoek(char *keylit,ushort *resultp) {
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
				*resultp = 0;
				return rcset(Failure,0,text254,keylit);
						// "Invalid key literal '%s'"
				}
			if(kct == 1)
				ek = ek1;
			else
				ek |= ek1;
			break;
			}			
		}

	*resultp = ek;
	return rc.status;
	}

// Get a key binding (using given prompt if interactive) and save in *resultp.  If n <= 0, get one key only; otherwise, get a
// key sequence.  Return status.
static int getkb(char *prmt,int n,Datum **argpp,ushort *resultp) {

	// Script mode?
	if(si.opflags & OpScript) {

		// Yes, process argument.
		if(si.opflags & OpEval)
			(void) stoek(argpp[0]->d_str,resultp);
		}
	else {
		// No, get key from the keyboard.
		if(mlputs(MLHome | MLFlush,prmt) == Success)
			(void) ((n != INT_MIN && n <= 0) ? getkey(true,resultp,false) : getkseq(true,resultp,NULL,false));
		}

	return rc.status;
	}

// Describe the command or macro bound to a particular key.  Get single keystroke if n <= 0.  Display result on message line if
// n >= 0; otherwise, in a pop-up window (default).  Return status.
int showKey(Datum *rp,int n,Datum **argpp) {
	ushort ek;
	KeyBind *kbp;
	int (*nextf)(ShowCtrl *scp,ushort req,char **namep);
	char *usage = NULL;
	char *desc = NULL;
	char *name,keybuf[16];

	// Prompt the user for the key code.
	if(getkb(text13,n <= 0 && n != INT_MIN ? 0 : INT_MIN,argpp,&ek) != Success)
			// "Show key "
		return rc.status;

	// Find the command or macro.
	if((kbp = getbind(ek)) == NULL)
		name = text48;
			// "[Not bound]"
	else if(kbp->k_targ.p_type == PtrMacro_O) {
		Buffer *bufp = kbp->k_targ.u.p_bufp;
		name = bufp->b_bname + 1;
		if(n < 0) {
			nextf = nextMacro;
			usage = text336;
				// "macro"
			goto PopUp;
			}
		if(bufp->b_mip->mi_usage.d_type != dat_nil)
			usage = bufp->b_mip->mi_usage.d_str;
		if(bufp->b_mip->mi_desc.d_type != dat_nil)
			desc = bufp->b_mip->mi_desc.d_str;
		}
	else {
		CmdFunc *cfp = kbp->k_targ.u.p_cfp;
		name = cfp->cf_name;
		if(n < 0) {
			nextf = nextCommand;
			usage = text158;
				// "command"
			goto PopUp;
			}
		usage = cfp->cf_usage;
		desc = cfp->cf_desc;
		}

	// Display result on message line.
	ektos(ek,keybuf,false);
	if(mlprintf(MLHome | MLTermAttr,"%c%c%c%s%c%c -> %c%c%s%c%c",AttrSpecBegin,AttrAlt,AttrULOn,keybuf,AttrSpecBegin,
	 AttrULOff,AttrSpecBegin,AttrBoldOn,name,AttrSpecBegin,AttrBoldOff) == Success) {
		if(usage != NULL && mlprintf(MLTermAttr," %c%c%s%c%c",AttrSpecBegin,AttrBoldOn,usage,AttrSpecBegin,AttrBoldOff)
		 != Success)
			return rc.status;
		if(desc != NULL && (mlputs(MLTermAttr," - ") != Success || mlputs(MLTermAttr,desc) != Success))
			return rc.status;
		(void) mlflush();
		}
	return rc.status;
PopUp:;
	// Display result in pop-up window.
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,INT_MIN,usage,NULL) == Success) {
		if(dsetstr(name,&sc.sc_mstr) != 0)
			return librcset(Failure);
		if(showbuild(&sc,SHSepLine | SHExact,usage,nextf) == Success)
			(void) showclose(rp,-1,&sc);
		}
	return rc.status;
	}

#if MMDebug & Debug_Bind
// Dump binding table.
static void dumpbind(void) {
	KeyVect *kvp = keytab;
	KeyBind *kbp;
	int i = 0;
	char keybuf[16];

	do {
		fprintf(logfile,"\nBINDING LIST #%d\n",i++);
		kbp = (KeyBind *) kvp;
		do {
			if(kbp->k_code == 0)
				continue;
			fprintf(logfile,"    %8s   ->   %s\n",ektos(kbp->k_code,keybuf,false),
			(kbp->k_targ.p_type & PtrCmdType) ? kbp->k_targ.u.p_cfp->cf_name : kbp->k_targ.u.p_bufp->b_bname);
			} while(++kbp < (KeyBind *) kvp + NKeyVect);
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
void unbindent(KeyBind *kbp) {

	// Remove key from cache.
	clearcache(kbp->k_code);

	// Clear the entry.
	kbp->k_code = 0;
	kbp->k_targ.u.p_voidp = NULL;
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
	HashRec **hrpp0,**hrppz;
	HashRec **hrpp = NULL;
	(void) hsort(exectab,hcmp,&hrpp);
	hrppz = (hrpp0 = hrpp) + exectab->recCount;
	do {
		univp = univptr(*hrpp);
		fprintf(logfile,"%-20s%.4hX\n",(*hrpp)->key,univp->p_type);
		} while(++hrpp < hrppz);
	free((void *) hrpp0);
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
			univp->u.p_voidp = NULL;
			}
		else if(!execfind(last->p_tok.d_str,OpQuery,selector & PtrMacro ? selector | PtrMacro : selector,univp))
			return rcset(Failure,0,emsg,last->p_tok.d_str);
		(void) getsym();
		}
	else
		(void) getcfam(prmt,selector | Term_Attr,univp,emsg);
	return rc.status;
	}

// Bind a key sequence to a command or macro.  Get single key if n <= 0.  Return status.
int bindKeyCM(Datum *rp,int n,Datum **argpp) {
	ushort ek;			// Key to bind.
	UnivPtr univ;			// Pointer to the requested command or macro.
	KeyBind *k_kdp,*c_kdp;
	char keybuf[16];
	char wkbuf[NWork + 1];

	// Get the key or key sequence to bind.
	if(getkb(text15,n,argpp,&ek) != Success)
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
		return rcset(Failure,RCTermAttr,"%s%s%s%s '%c%c%s%c%c'",text418,text416,text417,text414,AttrSpecBegin,AttrBoldOn,
		 univ.u.p_bufp->b_bname + 1,AttrSpecBegin,AttrBoldOff);
			// "Key binding"," not allowed"," on ","constrained macro"

	// If script mode and not evaluating, bail out here.
	if((si.opflags & (OpScript | OpEval)) == OpScript)
		return rc.status;

	// Interactive mode or evaluating.  Search the binding table to see if the key exists.
	if((k_kdp = getbind(ek)) != NULL) {

		// If the key is already bound to this command or macro, it's a no op.
		if(k_kdp->k_targ.u.p_voidp == univ.u.p_voidp)
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
	if(k_kdp != NULL) {

		// Yes, change it.
		k_kdp->k_targ = univ;
#if MMDebug & Debug_Bind
		goto Retn;
#else
		return rc.status;
#endif
		}

	// Not in table.  Add it.
	newcbind(ek,&univ);
#if MMDebug & Debug_Bind
Retn:
	fprintf(logfile,"bindKeyCM(%s) DONE.\n",ektos(ek,wkbuf,false));
	dumpbind();
#endif
	return rc.status;
	}

// Delete a key from the binding table.  Get single keystroke if n <= 0.  Ignore "key not bound" error if n > 0 and script mode.
int unbindKey(Datum *rp,int n,Datum **argpp) {
	ushort ek;			// Key to unbind.
	KeyBind *kbp;
	char keybuf[16];

	// Get the key or key sequence to unbind.
	if(getkb(text18,n,argpp,&ek) != Success)
			// "Unbind key "
		return rc.status;

	// Change key to something we can print.
	ektos(ek,keybuf,true);

	// Search the table to see if the key exists.
	if((kbp = getbind(ek)) != NULL) {

		// If the key is bound to a permanent-bind command and the only such binding, it can't be deleted.
		if((kbp->k_targ.p_type & PtrCmdType) && (kbp->k_targ.u.p_cfp->cf_aflags & CFPerm) && pentryct(&kbp->k_targ) < 2)
			return rcset(Failure,RCTermAttr,text210,keybuf,kbp->k_targ.u.p_cfp->cf_name);
					// "~#u%s~U is only binding to core command '~b%s~B' -- cannot delete or reassign"

		// It's a go... unbind it.
		unbindent(kbp);
		}
	else if(!(si.opflags & OpScript) || n <= 0)
		return rcset(Failure,RCTermAttr,text14,keybuf);
			// "~#u%s~U not bound"

	// Dump it out if interactive.
	if(!(si.opflags & OpScript))
		(void) mlprintf(MLTermAttr,"%c%c%c%s%c%c",AttrSpecBegin,AttrAlt,AttrULOn,keybuf,AttrSpecBegin,AttrULOff);
	else if(n > 0)
		dsetbool(kbp != NULL,rp);

	return rc.status;
	}

// Search ltab for extended key with matching flag.  Store literal in *strp and return true if found; otherwise, return false.
static bool ectol(ushort ek,ushort flag,char **strp) {
	ushort c;
	struct clit *clp = ltab;

	// Print the character using the Ctrl or non-Ctrl literal in ltab, if possible.
	c = ek & (flag | 0xFF);
	do {
		if((flag == 0 || (clp->ek & Ctrl)) && clp->ek == c) {
			*strp = stpcpy(*strp,clp->kw);
			return true;
			}
		} while((++clp)->kw != NULL);
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

	// Print the character using the "control" literals in ltab, if possible.
	if(ectol(ek,Ctrl,&str))
		goto Retn;

	// No literal found.  Control key?
	if(ek & Ctrl)
		str = stpcpy(str,"C-");

	// Print the character using the "non-control" literals in ltab, if possible.
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

// Get name associated with given KeyBind object.
char *getkname(KeyBind *kbp) {

	return (kbp == NULL) ? NULL : (kbp->k_targ.p_type & PtrCmdType) ? kbp->k_targ.u.p_cfp->cf_name :
	 (kbp->k_targ.p_type == PtrMacro_O) ? kbp->k_targ.u.p_bufp->b_bname : NULL;
	}

// Do binding? function.  If default n: return name of command or macro bound to given key binding, or nil if none; if n > 0:
// return array of key binding(s) bound to given command or macro; or n <= 0: return key-lit in standardized form, or nil if
// it is invalid.  Return status.
int binding(Datum *rp,int n,Datum **argpp) {
	ushort ek;

	if(n <= 0) {
		// Get key literal.
		if(funcarg(rp,ArgFirst | ArgNotNull1) != Success)
			goto Retn;
		if(!(si.opflags & OpEval))
			goto NilRtn;
		if(stoek(rp->d_str,&ek) != Success) {
			if(n == INT_MIN)
				goto Retn;
			rcclear(0);
			goto NilRtn;
			}
		if(n != INT_MIN) {
			char keybuf[16];

			if(dsetstr(ektos(ek,keybuf,false),rp) != 0)
				goto LibFail;
			}
		else {
			char *str = fixnull(getkname(getbind(ek)));
			if(*str == '\0') {
				if(ek >= 0x20 && ek < 0xFF)
					str = text383;
						// "(self insert)"
				else
					goto NilRtn;
				}
			if(dsetstr(*str == SBMacro ? str + 1 : str,rp) != 0)
				goto LibFail;
			}
		}
	else {
		Array *aryp;
		Datum dat;
		UnivPtr univ;
		KeyWalk kw = {NULL,NULL};
		KeyBind *kbp;
		char keybuf[16];

		// Get the command or macro name.
		if(getcfm(NULL,&univ,PtrCmdType | PtrMacro_O) != Success || univ.p_type == PtrNul)
			goto Retn;

		// Get key binding(s) for given command or macro.
		if(mkarray(rp,&aryp) != Success)
			goto Retn;
		dinit(&dat);
		kbp = nextbind(&kw);
		do {
			if(kbp->k_targ.u.p_voidp == univ.u.p_voidp) {

				// Add the key sequence.
				if(dsetstr(ektos(kbp->k_code,keybuf,false),&dat) != 0 || apush(aryp,&dat,true) != 0)
LibFail:
					return librcset(Failure);
				}
			} while((kbp = nextbind(&kw)) != NULL);
		dclear(&dat);
		}
	goto Retn;
NilRtn:
	dsetnil(rp);
Retn:
	return rc.status;
	}

// Create an entry in the execution table.
int execnew(char *name,UnivPtr *univp) {
	HashRec *hrp;

	return (hrp = hset(exectab,name,NULL,false)) == NULL || dsetblob((void *) univp,sizeof(UnivPtr),hrp->valuep) != 0 ?
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
	HashRec *hrp;
	char myname[] = "execfind";

	// Check the execution table.
	if((hrp = hsearch(exectab,name)) != NULL) {

		// Found it.  Check operation type.
		if(op == OpQuery) {
			if(!(univptr(hrp)->p_type & selector))
				return false;
			if(univp != NULL)
				*univp = *univptr(hrp);
			return true;
			}
		if(op == OpDelete)
			ddelete(hdelete(exectab,name));		// Delete entry and free the storage.
		else if(op == OpUpdate)
			*univptr(hrp) = *univp;
		return rc.status;
		}

	// No such entry exists.  Create one, return false, or fatal error (a bug).
	return (op == OpCreate) ? execnew(name,univp) : (op == OpQuery) ? false :
	 rcset(FatalError,0,text16,myname,name);
		// "%s(): No such entry '%s' to update or delete!"
	}
