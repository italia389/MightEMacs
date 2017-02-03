// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// bind.c	Key binding routines for MightEMacs.

#include "os.h"
#include <stdarg.h>
#include "std.h"
#include "lang.h"
#include "cmd.h"
#include "exec.h"
#include "main.h"

// Make selected global definitions local.
#define DATAbind
#include "bind.h"

/*** Local declarations ***/

// Recognized keywords in string-encoded key bindings.
struct clit {
	char *kw;		// Keyword.
	ushort ch;		// Character (extended key).
	} ltab[] = {
		{"SPC",' '},
		{"TAB",Ctrl|'I'},
		{"ESC",Ctrl|'['},
		{"RTN",Ctrl|'M'},
		{"DEL",Ctrl|'?'},
		{NULL,'\0'}};

// Walk through all key binding lists and return next binding in sequence, or NULL if none left.  If kwp->hkp is NULL, reset
// to beginning and return first binding found.
KeyDesc *nextbind(KeyWalk *kwp) {
	KeyVect *kvp = kwp->kvp;
	KeyDesc *kdp = kwp->kdp;

	if(kvp == NULL)
		kdp = (KeyDesc *) (kwp->kvp = kvp = keytab);

	for(;;) {
		if(kdp == (KeyDesc *) kvp + 128) {
			if(++kvp == keytab + NPrefix + 1)
				return NULL;
			kdp = (KeyDesc *) (kwp->kvp = kvp);
			}
		if(kdp->k_code != 0)
			break;
		++kdp;
		}
	kwp->kdp = kdp + 1;		
	return kdp;
	}

// Return the number of entries in the binding table that match the given FAB pointer.
static int pentryct(CFABPtr *cfabp) {
	int count = 0;
	KeyWalk kw = {NULL,NULL};
	KeyDesc *kdp = nextbind(&kw);

	// Search for existing bindings for the command or macro.
	do {
		if(kdp->k_cfab.u.p_voidp == cfabp->u.p_voidp)
			++count;
		} while((kdp = nextbind(&kw)) != NULL);

	return count;
	}

// Scan the binding table for the first entry that matches the given CFAB pointer and return it, or NULL if none found.
KeyDesc *getpentry(CFABPtr *cfabp) {
	KeyWalk kw = {NULL,NULL};
	KeyDesc *kdp = nextbind(&kw);

	// Search for an existing binding for the command or macro.
	do {
		if(kdp->k_cfab.u.p_voidp == cfabp->u.p_voidp)
			return kdp;
		} while((kdp = nextbind(&kw)) != NULL);

	return NULL;
	}

// Return binding slot for given extended key.
static KeyDesc *bindslot(ushort ek) {
	uint i;

	// Set i to target key vector.
	switch(ek & (Shft | FKey | Prefix)) {
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
		case Pref3:
			i = 4;
			break;
		case FKey:
			i = 5;
			break;
		default:
			i = 6;
		}

	return (KeyDesc *) (keytab + i) + ektoc(ek & ~(Shft | FKey | Prefix));
	}

// Look up a key binding in the binding table, given extended key.
KeyDesc *getbind(ushort ek) {
	KeyDesc *kdp = bindslot(ek);
	if(kdp->k_code != 0)
		return kdp;

	// No such binding.
	return NULL;
	}

// Add an extended key to the binding table.
static void newcbind(ushort ek,CFABPtr *cfabp) {
	KeyDesc *kdp = bindslot(ek);

	// Set keycode and the command or buffer pointer.
	kdp->k_code = ek;
	kdp->k_cfab = *cfabp;
	}

// Get binding of given extended key and return prefix flag if it's bound to a prefix command; otherwise, zero.
static ushort findPrefix(ushort ek) {
	KeyDesc *kdp = getbind(ek);
	if(kdp != NULL && kdp->k_cfab.p_type == PtrPseudo) {
		CmdFunc *cfp = kdp->k_cfab.u.p_cfp;
		if(cfp->cf_aflags & CFPrefix) {
			enum cfid id = (enum cfid) (cfp - cftab);
			return id == cf_metaPrefix ? Meta : id == cf_prefix1 ? Pref1 : id == cf_prefix2 ? Pref2 : Pref3;
			}
		}
	return 0;
	}

// Get one value from a coded string.  Return true if successful; otherwise, false.
static bool stoek1(char **keylitp,ushort *cp,bool *firstp) {
	ushort c;
	ushort ek = *cp;
	struct clit *ltabp;
	char *klit = *keylitp;

	// Loop until hit null or space.
	for(;;) {
		if(*klit == '\0')
			return false;

		// Prefix?
		if(klit[1] == '-') {
			switch(klit[0]) {
				case 'C':				// Ctrl prefix.
				case 'c':
					++klit;
					goto CtrlPref;
				case 'M':				// Meta prefix.
				case 'm':
					if(!*firstp || (ek & Meta))
						return false;
					ek |= Meta;
					break;
				case 'S':				// SHIFT prefix.
				case 's':
					if(!*firstp || (ek & Shft))
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
			if(klit[1] == '\0' || klit[1] == ' ')		// Bare '^'?
				goto Vanilla;				// Yes, take it literally
CtrlPref:
			if(ek & Ctrl)
				return false;
			ek |= Ctrl;
			++klit;
			}

		// Function key?
		else if(strncasecmp(klit,"fn",2) == 0) {
			if(!*firstp || (ek & FKey))
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
				if(strncmp(klit,ltabp->kw,3) == 0) {
					klit += 2;
					c = ltabp->ch & 0xff;
					ek |= ltabp->ch & ~0xff;
					goto Vanilla;
					}
				} while((++ltabp)->kw != NULL);

			// Not a keyword.  Literal control character? (boo, hiss)
			if(c < ' ' || c == 0x7f) {
				if(ek & Ctrl)				// Duplicate?
					return false;			// Yes, error.
				ek |= Ctrl;
				c ^= '@';				// Convert literal character to visible equivalent.
				++klit;
				}

			// Must be a vanilla character; that is, a printable (except space) or 8-bit character.  Move past it.
			else
Vanilla:
				++klit;

			// Character is in c and prefix flag(s) may have been set.  Do sanity checks.
			if((ek & 0xff) || (*klit != '\0' && (*klit != ' ' || !*firstp)))
									// Second char, not end of value, or more than two?
				return false;				// Yes, error.
			if(ek & Ctrl) {
				if(c == '@')				// C-@ or ^@ ?
					c = ' ';			// Yes, change back to a space.
				else if((c < '?' || c == '`' || c > 'z') && c != ' ')
					return false;			// Invalid character following C- or ^.
				}
			if((ek & (Ctrl | Meta)) && (ek & Shft))
				return false;

			// Make sure it's upper case if used with M-, C-, ^, follows a prefix, or solo S-.
			if((ek & (FKey | Shft)) == Shft) {

				// Have solo 'S-'.  Error if printable character follows and it's not a letter.
				if(isletter(c)) {
					ek &= ~Shft;
					goto UpCase;
					}
				if((c >= ' ' && c < 'A') || (c > 'Z' && c < 'a') || (c > 'z' && c <= '~'))
					return false;
				}
			else if(!(ek & FKey) && (ek & (Prefix | Ctrl)))
UpCase:
				c = upcase[c];

			// Success.  Check if first of two values, not meta, and a prefix key.	If so, set flag and clear char.
			if(*firstp && !(ek & Meta) && *klit == ' ') {
				ushort flag = findPrefix(ek | c);
				if(flag) {
					ek = flag;
					c = 0;
					}
				}

			// Return results.
			*keylitp = klit;
			*cp = ek | c;
			*firstp = false;
			break;
			}
		}
	return true;
	}

// Convert a coded string to an extended key code.  Set *resultp to zero if keylitp is invalid.  Return status.
//
// A coded key binding consists of one or two space-separated value(s).  Each value consists of zero or more prefixes
// followed by a character (other than space) or literal.  Recognized prefixes are:
//	M-	Meta prefix.
//	C-	Ctrl prefix.
//	^	Alternate Ctrl prefix.
//	S-	SHIFT prefix (for function key or character).
//	FN	Function prefix (which includes function keys and special keys like Delete and Up Arrow).
// All prefixes are case-insensitive.  Characters can be real control characters, printable characters, or any of the
// following literals:
//	DEL	Backspace key.
//	ESC	Escape key.
//	RTN	Return key.
//	SPC	Spacebar.
//	Tab	Tab key.
// The M-, S-, and FN prefixes are only valid on the first value, and all literals except ESC are only valid on the last value.
int stoek(char *keylit,ushort *resultp) {
	ushort ek = 0;		// Extended key to return.
	char *klit = keylit;	// Key literal.
	bool first = true;	// Decoding first value?

	// Parse it up.
	for(;;) {
		if(!stoek1(&klit,&ek,&first)) {
			*resultp = 0;
			return rcset(Failure,0,text254,keylit);
					// "Invalid key literal '%s'"
			}
		if(*klit == '\0')
			break;
		if(*klit == ' ')
			++klit;
		}

	*resultp = ek;
	return rc.status;
	}

// Get a key binding (using given prompt if interactive) and save in *resultp.  If n <= 0, get one key only; otherwise, get a
// key sequence.  Return status.
static int getkb(char *prmt,int n,Datum **argpp,ushort *resultp) {

	// Script mode?
	if(opflags & OpScript) {

		// Yes, process argument.
		if(opflags & OpEval)
			(void) stoek(argpp[0]->d_str,resultp);
		}
	else {
		// No, get key from the keyboard.
		if(mlputs(MLHome | MLForce,prmt) == Success)
			(void) ((n != INT_MIN && n <= 0) ? getkey(resultp) : getkseq(resultp,NULL));
		}

	return rc.status;
	}

// Describe the command or macro for a certain key.  Get single keystroke if n <= 0.  Return status.
int showKey(Datum *rp,int n,Datum **argpp) {
	ushort ek;
	KeyDesc *kdp;
	char *usage = NULL;
	char *desc = NULL;
	char *name,keybuf[16];

	// Prompt the user for the key code.
	if(getkb(text13,n,argpp,&ek) != Success)
			// "Show key "
		return rc.status;
	ektos(ek,keybuf);

	// Find the command or macro.
	if((kdp = getbind(ek)) == NULL)
		name = text48;
			// "[Not bound]"
	else if(kdp->k_cfab.p_type == PtrMacro) {
		Buffer *bufp = kdp->k_cfab.u.p_bufp;
		name = bufp->b_bname + 1;
		if(bufp->b_mip->mi_usage.d_type != dat_nil)
			usage = bufp->b_mip->mi_usage.d_str;
		if(bufp->b_mip->mi_desc.d_type != dat_nil)
			desc = bufp->b_mip->mi_desc.d_str;
		}
	else {
		CmdFunc *cfp = kdp->k_cfab.u.p_cfp;
		name = cfp->cf_name;
		usage = cfp->cf_usage;
		desc = cfp->cf_desc;
		}

	// Display result.
	if(mlprintf(MLHome,"'%s' -> %s",keybuf,name) == Success) {
		if(usage != NULL && (mlputc(MLRaw,' ') != Success || mlputs(MLRaw,usage) != Success))
			return rc.status;
		if(desc != NULL)
			(void) mlprintf(0," - %s",desc);
		}
	return rc.status;
	}

#if MMDebug & Debug_Bind
// Dump binding table.
static void dumpbind(void) {
	KeyVect *kvp = keytab;
	KeyDesc *kdp;
	int i = 0;
	char keybuf[16];

	do {
		fprintf(logfile,"BINDING LIST #%d\n",i++);
		kdp = (KeyDesc *) kvp;
		do {
			if(kdp->k_code == 0)
				continue;
			fprintf(logfile,"    [%.8X] %10s -> ",(uint) kdp,ektos(kdp->k_code,keybuf));
			if(kdp->k_cfab.p_type & PtrCmdType)
				fprintf(logfile,"[%.8X] %s\n",(uint) kdp->k_cfab.u.p_cfp,kdp->k_cfab.u.p_cfp->cf_name);
			else
				fprintf(logfile,"[%.8X] %s\n",(uint) kdp->k_cfab.u.p_bufp,kdp->k_cfab.u.p_bufp->b_bname);
			} while(++kdp < (KeyDesc *) kvp + 128);
		} while(++kvp < keytab + NPrefix + 1);

	fflush(logfile);
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
void unbindent(KeyDesc *kdp) {

	// Remove key from cache.
	clearcache(kdp->k_code);

	// Clear the entry.
	kdp->k_code = 0;
	kdp->k_cfab.u.p_voidp = NULL;
	}

// Load all the built-in key bindings.  Return status.
int loadbind(void) {
	CFABPtr cfab;
	KeyItem *kip = keyitems;

	do {
		cfab.p_type = ((cfab.u.p_cfp = cftab + kip->ki_id)->cf_aflags & CFHidden) ? PtrPseudo : PtrCmd;
		newcbind(kip->ki_code,&cfab);
		} while((++kip)->ki_code != 0);

	return rc.status;
	}

#if MMDebug & Debug_Temp
// Dump CFAM table to log file.
static void dumpcfam(void) {
	CFAMRec *frp = frheadp;
	do {
		fprintf(logfile,"%-20s%.4X\n",frp->fr_name,frp->fr_type);
		} while((frp = frp->fr_nextp) != NULL);
	}
#endif

// Get command, function, or macro name per selector flags.  Store pointer in *cfabp.  If interactive mode, pass prmt to
// getcfam().  Return status.
int getcfm(char *prmt,CFABPtr *cfabp,uint selector) {
	char *emsg = (selector & PtrFunc) ? text312 : (selector & PtrCmd) ? text130 : text116;
		// "No such command, function, or macro '%s'","No such command or macro '%s'","No such macro '%s'"

	if(opflags & OpScript) {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(!(opflags & OpEval)) {
			cfabp->p_type = PtrNul;
			cfabp->u.p_voidp = NULL;
			}
		else if(cfabsearch(last->p_tok.d_str,cfabp,selector) != 0)
			return rcset(Failure,0,emsg,last->p_tok.d_str);
		(void) getsym();
		}
	else
		(void) getcfam(prmt,selector,cfabp,emsg,NULL);
	return rc.status;
	}

// Bind a key sequence to a command or macro.  Get single key if n <= 0.  Return status.
int bindKeyCM(Datum *rp,int n,Datum **argpp) {
	ushort ek;			// Key to bind.
	CFABPtr cfab;			// Pointer to the requested command or macro.
	KeyDesc *k_kdp,*c_kdp;
	char keybuf[16];
	char wkbuf[NWork + 1];

	// Get the key or key sequence to bind.
	if(getkb(text15,n,argpp,&ek) != Success)
			// "Bind key "
		return rc.status;
	ektos(ek,keybuf);

	// If interactive mode, build "progress" prompt.
	if(!(opflags & OpScript)) {
		if(mlputc(MLForce,'\'') != Success || mlputs(MLForce,keybuf) != Success || mlputc(MLForce,'\'') != Success)
			return rc.status;
		sprintf(wkbuf,"%s'%s' %s %s",text15,keybuf,text339,text267);
			// "Bind key ","to","command or macro"
		}

	// Get the command or macro name.
	if(((opflags & OpScript) && !needsym(s_comma,true)) || getcfm(wkbuf,&cfab,PtrCmdType | PtrMacro) != Success ||
	 cfab.p_type == PtrNul)
		return rc.status;

	// Binding a key sequence to a single-key command?
	if((ek & KeySeq) && (cfab.p_type & PtrCmdType) && (cfab.u.p_cfp->cf_aflags & CFBind1))
		return rcset(Failure,0,text17,keybuf,cfab.u.p_cfp->cf_name);
			// "Cannot bind a key sequence '%s' to '%s' command"

	// If script mode and not evaluating, bail out here.
	if((opflags & (OpScript | OpEval)) == OpScript)
		return rc.status;

	// Interactive mode or evaluating.  Search the binding table to see if the key exists.
	if((k_kdp = getbind(ek)) != NULL) {

		// If the key is already bound to this command or macro, it's a no op.
		if(k_kdp->k_cfab.u.p_voidp == cfab.u.p_voidp)
			return rc.status;

		// If the key is bound to a permanent-bind command and the only such binding, it can't be reassigned.
		if((k_kdp->k_cfab.p_type & PtrCmdType) && (k_kdp->k_cfab.u.p_cfp->cf_aflags & CFPerm) &&
		 pentryct(&k_kdp->k_cfab) < 2)
			return rcset(Failure,0,text210,keybuf,k_kdp->k_cfab.u.p_cfp->cf_name);
					// "'%s' is only binding to core command '%s' -- cannot delete or reassign"
		}

	// Remove key from cache.
	clearcache(ek);

	// If binding to a command and the command is maintained in a global variable (for internal use), it can only have one
	// binding at most.
	if((cfab.p_type & PtrCmdType) && (cfab.u.p_cfp->cf_aflags & CFUniq)) {
		uint i = 0;

		// Search for an existing binding for the command and remove it.
		if((c_kdp = getpentry(&cfab)) != NULL)
			unbindent(c_kdp);

		// Update the key cache.
		do {
			if(corekeys[i].id == cfab.u.p_cfp - cftab) {
				corekeys[i].ek = ek;
				break;
				}
			} while(++i < NCoreKeys);
		}

	// Key already in binding table?
	if(k_kdp != NULL) {

		// Yes, change it.
		k_kdp->k_cfab = cfab;
#if MMDebug & Debug_Bind
		goto Retn;
#else
		return rc.status;
#endif
		}

	// Not in table.  Add it.
	newcbind(ek,&cfab);
#if MMDebug & Debug_Bind
Retn:
	fprintf(logfile,"bindKeyCM(%s) DONE.\n",cfab.u.p_cfp->cf_name);
	dumpbind();
#endif
	return rc.status;
	}

// Delete a key from the binding table.  Get single keystroke if n <= 0.  Ignore "key not bound" error if n > 0 and script mode.
int unbindKey(Datum *rp,int n,Datum **argpp) {
	ushort ek;			// Key to unbind.
	KeyDesc *kdp;
	char keybuf[16];

	// Get the key or key sequence to unbind.
	if(getkb(text18,n,argpp,&ek) != Success)
			// "Unbind key "
		return rc.status;

	// Change key to something we can print.
	ektos(ek,keybuf);

	// Search the table to see if the key exists.
	if((kdp = getbind(ek)) != NULL) {

		// If the key is bound to a permanent-bind command and the only such binding, it can't be deleted.
		if((kdp->k_cfab.p_type & PtrCmdType) && (kdp->k_cfab.u.p_cfp->cf_aflags & CFPerm) && pentryct(&kdp->k_cfab) < 2)
			return rcset(Failure,0,text210,keybuf,kdp->k_cfab.u.p_cfp->cf_name);
					// "'%s' is only binding to core command '%s' -- cannot delete or reassign"

		// It's a go... unbind it.
		unbindent(kdp);
		}
	else if(!(opflags & OpScript) || n <= 0)
		return rcset(Failure,0,text14,keybuf);
			// "'%s' not bound"

	// Dump it out if interactive.
	if(!(opflags & OpScript)) {
		if(mlputc(MLForce,'\'') == Success && mlputs(MLForce,keybuf) == Success)
			(void) mlputc(MLForce,'\'');
		}
	else if(n > 0)
		dsetbool(kdp == NULL ? false : true,rp);

	return rc.status;
	}

// Get a match (apropos) string with a null default.  Convert a nil argument to null as well.  Return status.
int apropos(Datum *mstrp,char *prmt,Datum **argpp) {

	if(!(opflags & OpScript)) {
		char wkbuf[NWork + 1];

		sprintf(wkbuf,"%s %s",text20,prmt);
				// "Apropos"
		(void) terminp(mstrp,wkbuf,"",RtnKey,0,0,0);
		}
	else if(argpp[0]->d_type == dat_nil)
		dsetnull(mstrp);
	else
		datxfer(mstrp,argpp[0]);

	return rc.status;
	}

// Write a list item to given string-fab object with padding.  Return status.
static int findkeys(DStrFab *rptp,uint ktype,void *tp) {
	KeyWalk kw = {NULL,NULL};
	KeyDesc *kdp;
	Buffer *bufp;
	CmdFunc *cfp;
	int len;
	bool first = true;
	MacInfo *mip = NULL;
	char *name,*usage;
	char keybuf[16];
	char blanks[NBufName + 2];

	blanks[0] = '\0';
	pad(blanks,NBufName + 1);

	// Set pointers and store the command name and argument syntax.
	if(ktype & PtrMacro) {
		bufp = (Buffer *) tp;
		name = bufp->b_bname + 1;
		mip = bufp->b_mip;
		usage = (mip != NULL && mip->mi_usage.d_type != dat_nil) ? mip->mi_usage.d_str : NULL;
		}
	else {
		cfp = (CmdFunc *) tp;
		name = cfp->cf_name;
		usage = cfp->cf_usage;
		}
	len = strlen(name);
	if(dputmem((void *) name,len,rptp) != 0)
		return drcset();
	if(usage != NULL) {
		len += strlen(usage) + 1;
		if(dputc(' ',rptp) != 0 || dputs(usage,rptp) != 0)
			return drcset();
		}
	if(len > NBufName) {
		if(dputc('\n',rptp) != 0)
			return drcset();
		len = 0;
		}
	if(dputmem((void *) blanks,(NBufName + 1) - len,rptp) != 0)
		return drcset();

	// Search for any keys bound to command or buffer (macro) "tp".
	kdp = nextbind(&kw);
	do {
		if((kdp->k_cfab.p_type & ktype) && kdp->k_cfab.u.p_voidp == tp) {
			if(!first && (dputc('\n',rptp) != 0 || dputmem((void *) blanks,NBufName + 1,rptp) != 0))
				return drcset();

			// Add the key sequence.
			ektos(kdp->k_code,keybuf);
			len = strlen(keybuf);
			if(dputmem((void *) keybuf,len,rptp) != 0)
				return drcset();
			if(first && ((ktype & PtrCmdType) || (mip != NULL && mip->mi_desc.d_type != dat_nil))) {

				// Add the command or macro description.
				if((len = 10 - len) > 0 && dputmem((void *) blanks,len,rptp) != 0)
					return drcset();
				if(dputs(ktype & PtrCmdType ? cfp->cf_desc : mip->mi_desc.d_str,rptp) != 0)
					return drcset();
				}
			first = false;
			}
		} while((kdp = nextbind(&kw)) != NULL);

	// If no key was bound, we need to dump it anyway.
	if(first) {
		if((ktype & PtrCmdType) || (mip != NULL && mip->mi_desc.d_type != dat_nil))
			if(dputmem((void *) blanks,10,rptp) != 0 ||
			 dputs(ktype & PtrCmdType ? cfp->cf_desc : mip->mi_desc.d_str,rptp) != 0)
				return drcset();
		}

	return rc.status;
	}

// List all commands and their bindings, if any.  If default n, make full list; otherwise, get a match string and make partial
// list of command names that contain it, ignoring case.  Render buffer and return status.
int showBindings(Datum *rp,int n,Datum **argpp) {
	CmdFunc *cfp;			// Pointer into the command binding table.
	Alias *ap;			// Pointer into the alias table.
	Buffer *listp;			// Buffer to put binding list into.
	Buffer *bufp;			// Buffer pointer for function scan.
	bool needBreak,skipLine;
	DStrFab rpt;
	Datum *mstrp = NULL;		// Match string.
#if MMDebug & Debug_CFAB
	char wkbuf[NBufName + 12 + 16];
#else
	char wkbuf[NBufName + 12];
#endif

	// If not default n, get match string.
	if(n != INT_MIN) {
		if(dnewtrk(&mstrp) != 0)
			return drcset();
		if(apropos(mstrp,Literal4,argpp) != Success)
				// "name"
			return rc.status;
		}

	// Get a new buffer for the binding list and open a string-fab object.
	if(sysbuf(text21,&listp) != Success)
			// "BindingList"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	// Scan the command-function table.
	cfp = cftab;
	skipLine = true;
	needBreak = false;
	do {
		// Skip if a function or an apropos and command name doesn't contain the search string.
		if((cfp->cf_aflags & CFFunc) || (mstrp != NULL && strcasestr(cfp->cf_name,mstrp->d_str) == NULL))
			continue;

 		if(skipLine) {
			if(dputs(Literal42,&rpt) != 0)
					// "COMMANDS"
				return drcset();
			needBreak = true;
			skipLine = false;
			}
		if(needBreak && dputc('\n',&rpt) != 0)
			return drcset();

		// Search for any keys bound to this command and add to the buffer.
		if(findkeys(&rpt,PtrCmdType,(void *) cfp) != Success)
			return rc.status;
		needBreak = true;

		// On to the next command.
		} while((++cfp)->cf_name != NULL);

	// Scan the buffers, looking for macros and their bindings.
	bufp = bheadp;
	skipLine = true;
	do {
		// Is this buffer a macro?
		if(!(bufp->b_flags & BFMacro))
			continue;

		// Skip if an apropos and buffer name doesn't contain the search string.
		if(mstrp != NULL && strcasestr(bufp->b_bname,mstrp->d_str) == NULL)
			continue;

		// Add a blank line between the command and macro list.
 		if(skipLine) {
			if(needBreak && dputc('\n',&rpt) != 0)
				return drcset();
			if(dputs(Literal43,&rpt) != 0)
					// "MACROS"
				return drcset();
			needBreak = true;
			skipLine = false;
			}
		if(needBreak && dputc('\n',&rpt) != 0)
			return drcset();

		// Search for any keys bound to this macro and add to the buffer.
		if(findkeys(&rpt,PtrMacro,(void *) bufp) != Success)
			return rc.status;
		needBreak = true;

		// On to the next buffer.
		} while((bufp = bufp->b_nextp) != NULL);

	// Scan the alias list, looking for alias names and names of commands or macros they point to.
	skipLine = true;
	for(ap = aheadp; ap != NULL; ap = ap->a_nextp) {

		// Skip if an apropos and alias name or name it points to doesn't contain the search string.
		if(mstrp != NULL && strcasestr(ap->a_name,mstrp->d_str) == NULL && strcasestr(ap->a_type == PtrAlias_M ?
		 ap->a_cfab.u.p_bufp->b_bname : ap->a_cfab.u.p_cfp->cf_name,mstrp->d_str) == NULL)
			continue;

		// Add a blank line between the macro and alias list.
 		if(skipLine) {
			if(needBreak && dputc('\n',&rpt) != 0)
				return drcset();
			if(dputs(Literal44,&rpt) != 0)
					// "ALIASES"
				return drcset();
			needBreak = true;
			skipLine = false;
			}
		if(needBreak && dputc('\n',&rpt) != 0)
			return drcset();

		// Add the alias to the string-fab object.
		strcpy(wkbuf,ap->a_name);
		strcpy(pad(wkbuf,NBufName + 1),"->");
		pad(wkbuf,NBufName + 11);
		if(dputs(wkbuf,&rpt) != 0)
			return drcset();
		strcpy(wkbuf,ap->a_cfab.p_type == PtrMacro ? ap->a_cfab.u.p_bufp->b_bname : ap->a_cfab.u.p_cfp->cf_name);
#if MMDebug & Debug_CFAB
		sprintf(strchr(wkbuf,'\0')," (type %hu)",ap->a_cfab.p_type);
#endif
		if(dputs(wkbuf,&rpt) != 0)
			return drcset();

		// On to the next alias.
		}

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(!disnull(rpt.sf_datp) && bappend(listp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display the list.
	return render(rp,n < 0 ? -2 : n,listp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}

// Search ltab for extended key with matching flag.  Store literal in *strp and return true if found; otherwise, return false.
static bool ectol(ushort ek,ushort flag,char **strp) {
	ushort c;
	struct clit *clp = ltab;

	// Print the character using the Ctrl or non-Ctrl literal in ltab, if possible.
	c = ek & (flag | 0xff);
	do {
		if((flag == 0 || (clp->ch & Ctrl)) && clp->ch == c) {
			*strp = stpcpy(*strp,clp->kw);
			return true;
			}
		} while((++clp)->kw != NULL);
	return false;
	}

// Print character from an extended key to str and return pointer to trailing null.  Handle Ctrl and FKey flags.
static char *ektos1(ushort ek,char *str) {
	ushort c;

	// Function key?
	if(ek & FKey) {
		str = stpcpy(str,"FN");
		*str++ = ek & 0xff;
		goto Retn;
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
	if((c = ek & 0xff) & 0x80) {
		sprintf(str,"<%.2X>",c);
		str = strchr(str,'\0');
		}
	else {
		*str++ = (ek & (Prefix | Ctrl)) ? lowcase[c] : c;
Retn:
		*str = '\0';
		}

	return str;
	}

// Encode an extended key to a printable string, save result in dest, and return it.
char *ektos(ushort ek,char *dest) {
	char *str;
	static struct pkey {
		ushort code;
		enum cfid id;
		ushort flag;
		} pkeys[] = {
			{Ctrl|'X',cf_prefix1,Pref1},
			{Ctrl|'C',cf_prefix2,Pref2},
			{Ctrl|'H',cf_prefix3,Pref3},
			{0,0,0}};

	// Do the non-Meta prefix keys first, giving preference to the default values (^X, ^C, and ^H) in case multiple
	// keys are bound to the same prefix.
	str = dest;
	if(ek & (Pref1 | Pref2 | Pref3)) {
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
		CFABPtr cfab = {PtrNul};	// Pointer type not used.
		pkp = pkeys;
		for(;;) {
			if(ek & pkp->flag) {
				cfab.u.p_cfp = cftab + pkp->id;
				ek2 = getpentry(&cfab)->k_code;
Print1:
				str = ektos1(ek2,str);
				*str++ = ' ';
				break;
				}
			++pkp;
			}
		}
	else {
		// Print any meta or shift prefix literals.
		if(ek & Meta)
			str = stpcpy(str,"M-");
		if(ek & Shft)
			str = stpcpy(str,"S-");
		}

	// Print the base character and return result.
	ektos1(ek,str);
	return dest;
	}

// Get name associated with given KeyDesc object.
char *getkname(KeyDesc *kdp) {

	return (kdp == NULL) ? NULL : (kdp->k_cfab.p_type & PtrCmdType) ? kdp->k_cfab.u.p_cfp->cf_name :
	 (kdp->k_cfab.p_type == PtrMacro) ? kdp->k_cfab.u.p_bufp->b_bname : NULL;
	}

// Find an alias or macro (by name) in the CFAM record list and return status or boolean result.  (1), if the CFAM record is
// found: if op is OpQuery, return true; if op is OpCreate, return rc.status; otherwise (op is OpDelete), delete the CFAM
// record.  (2), if the CFAM record is not found: if op is OpCreate, create a new entry with the given name and pointer type; if
// op is OpQuery, return false, ignoring crpp; otherwise, return FatalError (should not happen).
int amfind(char *name,short op,uint type) {
	CFAMRec *frp1,*frp2;
	int result;
	static char myname[] = "amfind";

	// Scan the CFAM record list.
	frp1 = NULL;
	frp2 = frheadp;
	do {
		if((result = strcmp(frp2->fr_name,name)) == 0) {

			// Found it.  Now what?
			if(op == OpDelete) {

				// Delete it from the list and free the storage.
				if(frp1 == NULL)
					frheadp = frp2->fr_nextp;
				else
					frp1->fr_nextp = frp2->fr_nextp;
				free((void *) frp2);

				return rc.status;
				}

			// Not a delete.
			return (op == OpQuery) ? true : rc.status;
			}
		if(result > 0)
			break;
		frp1 = frp2;
		} while((frp2 = frp2->fr_nextp) != NULL);

	// No such CFAM record exists, create it?
	if(op == OpCreate) {

		// Allocate the needed memory.
		if((frp2 = (CFAMRec *) malloc(sizeof(CFAMRec))) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"

		// Find the place in the list to insert this CFAM record.
		if(frp1 == NULL) {

			// Insert at the beginning.
			frp2->fr_nextp = frheadp;
			frheadp = frp2;
			}
		else {
			// Insert after frp1.
			frp2->fr_nextp = frp1->fr_nextp;
			frp1->fr_nextp = frp2;
			}

		// Set the other CFAMRec members.
		frp2->fr_name = name;
		frp2->fr_type = type;

		return rc.status;
		}

	// Entry not found and not a create.  Fatal error (a bug) if not OpQuery.
	return (op == OpQuery) ? false : rcset(FatalError,0,text16,myname,name);
						// "%s(): No such entry '%s' to delete!"
	}
