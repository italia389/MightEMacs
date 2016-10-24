// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// bind.c	Key binding routines for MightEMacs.

#include "os.h"
#include <stdarg.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"

// Make selected global definitions local.
#define DATAbind

#include "ebind.h"		// Default key bindings.

/*** Local declarations ***/

// Recognized keywords in string-encoded key bindings.
struct clit {
	char *kwp;		// Keyword.
	ushort ch;		// Character (extended key).
	} ltab[] = {
		{"SPC",' '},
		{"TAB",CTRL|'I'},
		{"ESC",CTRL|'['},
		{"RTN",CTRL|'M'},
		{"DEL",CTRL|'?'},
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
			if(++kvp == keytab + NPREFIX + 1)
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

// Get a key binding (using given prompt if interactive) and save in result.  If n <= 0, get one key only; otherwise, get a key
// sequence.  Return status.
static int getkb(char *prmtp,int n,ushort *result) {

	// Script mode?
	if(opflags & OPSCRIPT) {
		Value *vtokp;

		// Yes, get next argument.
		if(vnew(&vtokp,false) != 0)
			return vrcset();
		if(funcarg(vtokp,ARG_FIRST | ARG_NOTNULL | ARG_STR) == SUCCESS && (opflags & OPEVAL))
			(void) stoek(vtokp->v_strp,result);
		}
	else {
		// No, get key from the keyboard.
		if(mlputs(MLHOME | MLFORCE,prmtp) == SUCCESS)
			(void) ((n != INT_MIN && n <= 0) ? getkey(result) : getkseq(result,NULL));
		}

	return rc.status;
	}

// Describe the command for a certain key (interactive only).  Get single keystroke if n <= 0.
int showKey(Value *rp,int n) {
	ushort ek;		// Key to describe.
	char *strp,wkbuf[16];	// Work pointer and buffer.

	// Prompt the user for the key code.
	if(getkb(text13,n,&ek) != SUCCESS)
			// "Show key "
		return rc.status;

	// Find the command.
	if((strp = getkname(getbind(ek))) == NULL)
		strp = text48;
			// "[Not bound]"

	// Display result.
	return mlprintf(0,"'%s' -> %s",ektos(ek,wkbuf),strp);
	}

#if MMDEBUG & DEBUG_BIND
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
			fprintf(logfile,"    [%.8x] %10s -> ",(uint) kdp,ektos(kdp->k_code,keybuf));
			if(kdp->k_cfab.p_type & PTRCMDTYP)
				fprintf(logfile,"[%.8x] %s\n",(uint) kdp->k_cfab.u.p_cfp,kdp->k_cfab.u.p_cfp->cf_name);
			else
				fprintf(logfile,"[%.8x] %s\n",(uint) kdp->k_cfab.u.p_bufp,kdp->k_cfab.u.p_bufp->b_bname);
			} while(++kdp < (KeyDesc *) kvp + 128);
		} while(++kvp < keytab + NPREFIX + 1);

	fflush(logfile);
	}
#endif

// Clear extended key from key cache, if present.
static void clearcache(uint ek) {
	uint i = 0;

	do {
		if(corekeys[i].ek == ek) {
			corekeys[i].ek = 0;
			break;
			}
		} while(++i < NCOREKEYS);
	}

// Clear given key entry in the binding table.
void unbindent(KeyDesc *kdp) {

	// Remove key from cache.
	clearcache(kdp->k_code);

	// Clear the entry.
	kdp->k_code = 0;
	kdp->k_cfab.u.p_voidp = NULL;
	}

// Return binding slot for given extended key.
static KeyDesc *bindslot(uint ek) {
	uint i;

	// Set i to target key vector.
	switch(ek & (SHFT | FKEY | PREFIX)) {
		case 0:
			i = 0;
			break;
		case META:
			i = 1;
			break;
		case PREF1:
			i = 2;
			break;
		case PREF2:
			i = 3;
			break;
		case PREF3:
			i = 4;
			break;
		case FKEY:
			i = 5;
			break;
		default:
			i = 6;
		}

	return (KeyDesc *) (keytab + i) + ektoc(ek & ~(SHFT | FKEY | PREFIX));
	}

// Look up a key binding in the binding table, given extended key.
KeyDesc *getbind(uint ek) {
	KeyDesc *kdp = bindslot(ek);
	if(kdp->k_code != 0)
		return kdp;

	// No such binding.
	return NULL;
	}

// Add an extended key to the binding table.
static void newcbind(uint ek,CFABPtr *cfabp) {
	KeyDesc *kdp = bindslot(ek);

	// Set keycode and the command or buffer pointer.
	kdp->k_code = ek;
	kdp->k_cfab = *cfabp;
	}

// Load all the built-in key bindings.  Return status.
int loadbind(void) {
	CFABPtr cfab;
	KeyItem *kip = keyitems;

	do {
		cfab.p_type = ((cfab.u.p_cfp = cftab + kip->ki_id)->cf_flags & CFHIDDEN) ? PTRPSEUDO : PTRCMD;
		newcbind(kip->ki_code,&cfab);
		} while((++kip)->ki_code != 0);

	return rc.status;
	}

// Get command, function, or macro name per selector flags.  Store pointer in *cfabp.  If interactive mode, pass prmtp to
// getcfam().  Return status.
int getcfm(char *prmtp,CFABPtr *cfabp,uint selector) {
	char *emsg = (selector & PTRFUNC) ? text312 : text130;
		// "No such command, function, or macro '%s'","No such command or macro '%s'"

	if(opflags & OPSCRIPT) {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(!(opflags & OPEVAL)) {
			cfabp->p_type = PTRNUL;
			cfabp->u.p_voidp = NULL;
			}
		else if(cfabsearch(last->p_tok.v_strp,cfabp,selector))
			return rcset(FAILURE,0,emsg,last->p_tok.v_strp);
		(void) getsym();
		}
	else
		(void) getcfam(prmtp,selector,cfabp,emsg,NULL);
	return rc.status;
	}

// Bind a key sequence to a command or macro.  Get single key if n <= 0.
int bindKeyCM(Value *rp,int n) {
	ushort ek;			// Key to bind.
	CFABPtr cfab;			// Pointer to the requested command or macro.
	KeyDesc *k_kdp,*c_kdp;
	char keybuf[16];
	char wkbuf[NWORK + 1];

	// Get the key or key sequence to bind.
	if(getkb(text15,n,&ek) != SUCCESS)
			// "Bind key "
		return rc.status;
	ektos(ek,keybuf);

	// If interactive mode, build "progress" prompt.
	if(!(opflags & OPSCRIPT)) {
		if(mlputc(MLFORCE,'\'') != SUCCESS || mlputs(MLFORCE,keybuf) != SUCCESS || mlputc(MLFORCE,'\'') != SUCCESS)
			return rc.status;
		sprintf(wkbuf,"%s'%s' %s %s",text15,keybuf,text339,text267);
			// "Bind key ","to","command or macro"
		}

	// Get the command or macro name.
	if(((opflags & OPSCRIPT) && !getcomma(true)) || getcfm(wkbuf,&cfab,PTRCMDTYP | PTRMACRO) != SUCCESS ||
	 cfab.p_type == PTRNUL)
		return rc.status;

	// Binding a key sequence to a single-key command?
	if((ek & KEYSEQ) && (cfab.p_type & PTRCMDTYP) && (cfab.u.p_cfp->cf_flags & CFBIND1))
		return rcset(FAILURE,0,text17,keybuf,cfab.u.p_cfp->cf_name);
			// "Cannot bind a key sequence '%s' to '%s' command"

	// If script mode and not evaluating, bail out here.
	if((opflags & (OPSCRIPT | OPEVAL)) == OPSCRIPT)
		return rc.status;

	// Interactive mode or evaluating.  Search the binding table to see if the key exists.
	if((k_kdp = getbind(ek)) != NULL) {

		// If the key is already bound to this command or macro, it's a no op.
		if(k_kdp->k_cfab.u.p_voidp == cfab.u.p_voidp)
			return rc.status;

		// If the key is bound to a permanent-bind command and the only such binding, it can't be reassigned.
		if((k_kdp->k_cfab.p_type & PTRCMDTYP) && (k_kdp->k_cfab.u.p_cfp->cf_flags & CFPERM) &&
		 pentryct(&k_kdp->k_cfab) < 2)
			return rcset(FAILURE,0,text210,keybuf,k_kdp->k_cfab.u.p_cfp->cf_name);
					// "'%s' is only binding to core command '%s' -- cannot delete or reassign"
		}

	// Remove key from cache.
	clearcache(ek);

	// If binding to a command and the command is maintained in a global variable (for internal use), it can only have one
	// binding at most.
	if((cfab.p_type & PTRCMDTYP) && (cfab.u.p_cfp->cf_flags & CFUNIQ)) {
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
			} while(++i < NCOREKEYS);
		}

	// Key already in binding table?
	if(k_kdp != NULL) {

		// Yes, change it.
		k_kdp->k_cfab = cfab;
#if MMDEBUG & DEBUG_BIND
		goto out;
#else
		return rc.status;
#endif
		}

	// Not in table.  Add it.
	newcbind(ek,&cfab);
#if MMDEBUG & DEBUG_BIND
out:
	fprintf(logfile,"bindKeyCM(%s) DONE.\n",cfab.u.p_cfp->cf_name);
	dumpbind();
#endif
	return rc.status;
	}

// Delete a key from the binding table.  Get single keystroke if n <= 0.  Ignore "key not bound" error if n > 0 and script mode.
int unbindKey(Value *rp,int n) {
	ushort ek;			// Key to unbind.
	KeyDesc *kdp;
	char keybuf[16];

	// Get the key or key sequence to unbind.
	if(getkb(text18,n,&ek) != SUCCESS)
			// "Unbind key "
		return rc.status;

	// If script mode and not evaluating, bail out here.
	if((opflags & (OPSCRIPT | OPEVAL)) == OPSCRIPT)
		return rc.status;

	// Change key to something we can print.
	ektos(ek,keybuf);

	// Search the table to see if the key exists.
	if((kdp = getbind(ek)) != NULL) {

		// If the key is bound to a permanent-bind command and the only such binding, it can't be deleted.
		if((kdp->k_cfab.p_type & PTRCMDTYP) && (kdp->k_cfab.u.p_cfp->cf_flags & CFPERM) && pentryct(&kdp->k_cfab) < 2)
			return rcset(FAILURE,0,text210,keybuf,kdp->k_cfab.u.p_cfp->cf_name);
					// "'%s' is only binding to core command '%s' -- cannot delete or reassign"

		// It's a go ... unbind it.
		unbindent(kdp);
		}
	else if(!(opflags & OPSCRIPT) || n <= 0)
		return rcset(FAILURE,0,text14,keybuf);
			// "'%s' not bound"

	// Dump it out if interactive.
	if(!(opflags & OPSCRIPT)) {
		if(mlputc(MLFORCE,'\'') == SUCCESS && mlputs(MLFORCE,keybuf) == SUCCESS)
			(void) mlputc(MLFORCE,'\'');
		}
	else if(n > 0 && vsetstr(kdp == NULL ? val_false : val_true,rp) != 0)
		(void) vrcset();

	return rc.status;
	}

// Find an alias by name and return status or boolean result.  (1), if the alias is found: if op is OPQUERY or OPCREATE, set
// *app (if app not NULL) to the alias structure associated with it; otherwise (op is OPDELETE), delete the alias and the
// associated CFAM record.  If op is OPQUERY return true; othewise, return status.  (2), if the alias is not found: if op is
// OPCREATE, create a new entry, set its pointer record to *cfabp, and set *app (if app not NULL) to it; if op is OPQUERY,
// return false, ignoring app; otherwise, return an error.
int afind(char *anamep,int op,CFABPtr *cfabp,Alias **app) {
	Alias *ap1,*ap2;
	int result;

	// Scan the alias list.
	ap1 = NULL;
	ap2 = aheadp;
	while(ap2 != NULL) {
		if((result = strcmp(ap2->a_name,anamep)) == 0) {

			// Found it.  Check op.
			if(op == OPDELETE) {

				// Delete the CFAM record.
				if(amfind(anamep,OPDELETE,0) != SUCCESS)
					return rc.status;

				// Decrement alias use count on macro, if applicable.
				if(ap2->a_cfab.p_type == PTRMACRO)
					--ap2->a_cfab.u.p_bufp->b_nalias;

				// Delete the alias from the list and free the storage.
				if(ap1 == NULL)
					aheadp = ap2->a_nextp;
				else
					ap1->a_nextp = ap2->a_nextp;
				free((void *) ap2);

				return rc.status;
				}

			// Not a delete.  Return it.
			if(app != NULL)
				*app = ap2;
			return (op == OPQUERY) ? true : rc.status;
			}
		if(result > 0)
			break;
		ap1 = ap2;
		ap2 = ap2->a_nextp;
		}

	// No such alias exists, create it?
	if(op == OPCREATE) {
		enum e_sym sym;
		char *strp;

		// Valid identifier name?
		strp = anamep;
		if(((sym = getident(&strp,NULL)) != s_ident && sym != s_identq) || *strp != '\0')
			(void) rcset(FAILURE,0,text286,anamep);
				// "Invalid identifier '%s'"

		// Allocate the needed memory.
		if((ap2 = (Alias *) malloc(sizeof(Alias) + strlen(anamep))) == NULL)
			return rcset(PANIC,0,text94,"afind");
				// "%s(): Out of memory!"

		// Find the place in the list to insert this alias.
		if(ap1 == NULL) {

			// Insert at the beginning.
			ap2->a_nextp = aheadp;
			aheadp = ap2;
			}
		else {
			// Insert after ap1.
			ap2->a_nextp = ap1->a_nextp;
			ap1->a_nextp = ap2;
			}

		// Set the other record members.
		strcpy(ap2->a_name,anamep);
		ap2->a_cfab = *cfabp;
		ap2->a_type = cfabp->p_type == PTRCMD ? PTRALIAS_C : cfabp->p_type == PTRFUNC ? PTRALIAS_F : PTRALIAS_M;

		// Add its name to the CFAM list.
		if(amfind(ap2->a_name,OPCREATE,ap2->a_type) != SUCCESS)
			return rc.status;

		if(app != NULL)
			*app = ap2;
		return rc.status;
		}

	// Alias not found and not a create.
	return (op == OPQUERY) ? false : rcset(FAILURE,0,text271,anamep);
							// "No such alias '%s'"
	}

// Create an alias to a command, function, or macro.
int aliasCFM(Value *rp,int n) {
	CFABPtr cfab;
	Value *vnamep;

	// Get the alias name.
	if(vnew(&vnamep,false) != 0)
		return vrcset();
	if(opflags & OPSCRIPT) {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(vsetstr(last->p_tok.v_strp,vnamep) != 0)
			return vrcset();
		}
	else if(terminp(vnamep,text215,NULL,RTNKEY,0,0) != SUCCESS || vistfn(vnamep,VNIL))
			// "Create alias "
		return rc.status;

	// Existing function, alias, macro, or user variable of same name?
	if((opflags & OPEVAL) && (!cfabsearch(vnamep->v_strp,NULL,PTRCFAM) || uvarfind(vnamep->v_strp) != NULL))
		return rcset(FAILURE,0,text165,vnamep->v_strp);
			// "Name '%s' already in use"

	if(opflags & OPSCRIPT) {

		// Get equal sign.
		if(getsym() < NOTFOUND || !havesym(s_any,true))
			return rc.status;
		if(last->p_sym != s_assign)
			return rcset(FAILURE,0,text23,(cftab + cf_alias)->cf_name,last->p_tok.v_strp);
				// "Missing '=' in %s command (at token '%s')"
		if(getsym() < NOTFOUND)			// Past '='.
			return rc.status;

		// Get the command, function, or macro name.
		(void) getcfm(NULL,&cfab,PTRCMD | PTRFUNC | PTRMACRO);
		}
	else {
		char wkbuf[strlen(text215) + strlen(vnamep->v_strp) + strlen(text325) + strlen(text313) + 8];

		// Get the command, function, or macro name.
		sprintf(wkbuf,"%s%s %s %s",text215,vnamep->v_strp,text325,text313);
				// "Create alias ","for","command, function, or macro"
		(void) getcfm(wkbuf,&cfab,PTRCMD | PTRFUNC | PTRMACRO);
		}

	if(rc.status == SUCCESS && cfab.p_type != PTRNUL) {

		// Create the alias.
		if(afind(vnamep->v_strp,OPCREATE,&cfab,NULL) != SUCCESS)
			return rc.status;

		// Increment alias count on macro.
		if(cfab.p_type == PTRMACRO)
			++cfab.u.p_bufp->b_nalias;
		}

	return rc.status;
	}

// Delete one or more aliases or macros.  Return status.
int deleteAM(char *prmtp,uint selector,char *emsg) {
	CFABPtr cfab;

	// If interactive, get alias or macro name from user.
	if(!(opflags & OPSCRIPT)) {
		if(getcfam(prmtp,selector,&cfab,emsg,NULL) != SUCCESS || cfab.p_type == PTRNUL)
			return rc.status;
		goto nukeit;
		}

	// Script mode: get alias(es) or macro(s) to delete.
	do {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(opflags & OPEVAL) {
			if(cfabsearch(last->p_tok.v_strp,&cfab,selector))
				return rcset(FAILURE,0,emsg,last->p_tok.v_strp);
nukeit:
			// Nuke it.
			if(selector == PTRMACRO) {
				if(bdelete(cfab.u.p_bufp,CLBIGNCHGD,NULL) != SUCCESS)
					break;
				}
			else if(afind(cfab.u.p_aliasp->a_name,OPDELETE,NULL,NULL) != SUCCESS)
				break;
			}
		} while((opflags & OPSCRIPT) && getsym() == SUCCESS && getcomma(false));

	return rc.status;
	}

// Delete one or more alias(es).  Return status.
int deleteAlias(Value *rp,int n) {

	return deleteAM(text269,PTRALIAS,text271);
		// "Delete alias","No such alias '%s'"
	}

// Get a match (apropos) string with a null default.  Convert a nil argument to null as well.  Return status.
int apropos(Value *mstrp,char *prmtp) {
	char wkbuf[NWORK + 1];

	sprintf(wkbuf,"%s %s",text20,prmtp);
			// "Apropos"
	if(getarg(mstrp,wkbuf,"",RTNKEY,0,ARG_FIRST | ARG_STR) == SUCCESS && vistfn(mstrp,VNIL))
		vnull(mstrp);

	return rc.status;
	}

// Write a list item to given buffer with padding.  Return status.
static int findkeys(StrList *rptp,uint ktype,void *tp) {
	KeyWalk kw = {NULL,NULL};
	KeyDesc *kdp;
	Buffer *bufp;
	CmdFunc *cfp;
	bool first;
	char wkbuf[100];

	// Set pointers and store the command name and argument syntax.
	if(ktype & PTRMACRO) {
		bufp = (Buffer *) tp;
		strcpy(wkbuf,bufp->b_bname);
		}
	else {
		cfp = (CmdFunc *) tp;
		sprintf(wkbuf,"%s %s",cfp->cf_name,cfp->cf_usage);
		}

	// Search for any keys bound to command or buffer (macro) "tp".
	kdp = nextbind(&kw);
	first = true;
	do {
		if((kdp->k_cfab.p_type & ktype) &&
		 ((ktype & PTRCMDTYP) ? (void *) kdp->k_cfab.u.p_cfp : (void *) kdp->k_cfab.u.p_bufp) == tp) {
			ektos(kdp->k_code,pad(wkbuf,NBNAME + 1));	// Add the key sequence.
			if(!first) {
				if(vputc('\n',rptp) != 0)
					return vrcset();
				}
			else if(ktype & PTRCMDTYP) {
				pad(wkbuf,NBNAME + 11);
				if(vputs(wkbuf,rptp) != 0)
					return vrcset();
				strcpy(wkbuf,cfp->cf_desc);		// Add the command description...
				}
			if(vputs(wkbuf,rptp) != 0)
				return vrcset();
			first = false;
			*wkbuf = '\0';
			}
		} while((kdp = nextbind(&kw)) != NULL);

	// If no key was bound, we need to dump it anyway.
	if(wkbuf[0] != '\0') {
		if(ktype & PTRCMDTYP) {
			pad(wkbuf,NBNAME + 11);
			if(vputs(wkbuf,rptp) != 0)
				return vrcset();
			strcpy(wkbuf,cfp->cf_desc);				// Add the command description...
			}
		if(vputs(wkbuf,rptp) != 0)
			return vrcset();
		}

	return rc.status;
	}

// List all commands and their bindings, if any.  If default n, make full list; otherwise, get a match string and make partial
// list of command names that contain it, ignoring case.  Render buffer and return status.
int showBindings(Value *rp,int n) {
	CmdFunc *cfp;			// Pointer into the command binding table.
	Alias *ap;			// Pointer into the alias table.
	Buffer *listp;			// Buffer to put binding list into.
	Buffer *bufp;			// Buffer pointer for function scan.
	bool needBreak,skipLine;
	StrList rpt;
	Value *mstrp = NULL;		// Match string.
#if MMDEBUG & DEBUG_CFAB
	char wkbuf[NBNAME + 12 + 16];
#else
	char wkbuf[NBNAME + 12];
#endif

	// If not default n, get match string.
	if(n != INT_MIN) {
		if(vnew(&mstrp,false) != 0)
			return vrcset();
		if(apropos(mstrp,LITERAL4) != SUCCESS)
				// "name"
			return rc.status;
		}

	// Get a new buffer for the binding list and open a string list.
	if(sysbuf(text21,&listp) != SUCCESS)
			// "BindingList"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	// Scan the command-function table.
	cfp = cftab;
	needBreak = false;
	do {
		// Skip if a function or an apropos and command name doesn't contain the search string.
		if((cfp->cf_flags & CFFUNC) || (mstrp != NULL && strcasestr(cfp->cf_name,mstrp->v_strp) == NULL))
			continue;

		if(needBreak && vputc('\n',&rpt) != 0)
			return vrcset();

		// Search for any keys bound to this command and add to the buffer.
		if(findkeys(&rpt,PTRCMDTYP,(void *) cfp) != SUCCESS)
			return rc.status;
		needBreak = true;

		// On to the next command.
		} while((++cfp)->cf_name != NULL);

	// Scan the buffers, looking for macros and their bindings.
	bufp = bheadp;
	skipLine = true;
	do {
		// Is this buffer a macro?
		if(!(bufp->b_flags & BFMACRO))
			continue;

		// Skip if an apropos and buffer name doesn't contain the search string.
		if(mstrp != NULL && strcasestr(bufp->b_bname,mstrp->v_strp) == NULL)
			continue;

		// Add a blank line between the command and macro list.
 		if(skipLine) {
			if(needBreak && vputc('\n',&rpt) != 0)
				return vrcset();
			skipLine = false;
			}
		if(needBreak && vputc('\n',&rpt) != 0)
			return vrcset();

		// Search for any keys bound to this macro and add to the buffer.
		if(findkeys(&rpt,PTRMACRO,(void *) bufp) != SUCCESS)
			return rc.status;
		needBreak = true;

		// On to the next buffer.
		} while((bufp = bufp->b_nextp) != NULL);

	// Scan the alias list, looking for alias names and names of commands or macros they point to.
	skipLine = true;
	for(ap = aheadp; ap != NULL; ap = ap->a_nextp) {

		// Skip if an apropos and alias name or name it points to doesn't contain the search string.
		if(mstrp != NULL && strcasestr(ap->a_name,mstrp->v_strp) == NULL && strcasestr(ap->a_type == PTRALIAS_M ?
		 ap->a_cfab.u.p_bufp->b_bname : ap->a_cfab.u.p_cfp->cf_name,mstrp->v_strp) == NULL)
			continue;

		// Add a blank line between the macro and alias list.
 		if(skipLine) {
			if(needBreak && vputc('\n',&rpt) != 0)
				return vrcset();
			skipLine = false;
			}
		if(needBreak && vputc('\n',&rpt) != 0)
			return vrcset();

		// Add the alias to the string list.
		strcpy(wkbuf,ap->a_name);
		strcpy(pad(wkbuf,NBNAME + 1),"Alias");
		pad(wkbuf,NBNAME + 11);
		if(vputs(wkbuf,&rpt) != 0)
			return vrcset();
		strcpy(wkbuf,ap->a_cfab.p_type == PTRMACRO ? ap->a_cfab.u.p_bufp->b_bname : ap->a_cfab.u.p_cfp->cf_name);
#if MMDEBUG & DEBUG_CFAB
		sprintf(strchr(wkbuf,'\0')," (type %hu)",ap->a_cfab.p_type);
#endif
		if(vputs(wkbuf,&rpt) != 0)
			return vrcset();

		// On to the next alias.
		}

	// Add the results to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(!visnull(rpt.sl_vp) && bappend(listp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display the list.
	return render(rp,n < 0 ? -2 : n,listp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}

// Get binding of given extended key and return prefix flag if it's bound to a prefix command; otherwise, zero.
static uint findPrefix(uint ek) {
	KeyDesc *kdp = getbind(ek);
	if(kdp != NULL && kdp->k_cfab.p_type == PTRPSEUDO) {
		CmdFunc *cfp = kdp->k_cfab.u.p_cfp;
		if(cfp->cf_flags & CFPREFIX) {
			enum cfid id = (enum cfid) (cfp - cftab);
			return id == cf_metaPrefix ? META : id == cf_prefix1 ? PREF1 : id == cf_prefix2 ? PREF2 : PREF3;
			}
		}
	return 0;
	}

// Search ltab for extended key with matching flag.  Store literal in *strpp and return true if found; otherwise, return false.
static bool ectol(uint ek,uint flag,char **strpp) {
	uint c;
	struct clit *clp = ltab;

	// Print the character using the CTRL or non-CTRL literal in ltab, if possible.
	c = ek & (flag | 0xff);
	do {
		if((flag == 0 || (clp->ch & CTRL)) && clp->ch == c) {
			*strpp = stpcpy(*strpp,clp->kwp);
			return true;
			}
		} while((++clp)->kwp != NULL);
	return false;
	}

// Print character from an extended key to strp and return pointer to trailing null.  Handle CTRL and FKEY flags.
static char *ektos1(uint ek,char *strp) {
	uint c;

	// Function key?
	if(ek & FKEY) {
		strp = stpcpy(strp,"FN");
		*strp++ = ek & 0xff;
		goto retn;
		}

	// Print the character using the "control" literals in ltab, if possible.
	if(ectol(ek,CTRL,&strp))
		goto retn;

	// No literal found.  Control key?
	if(ek & CTRL)
		strp = stpcpy(strp,"C-");

	// Print the character using the "non-control" literals in ltab, if possible.
	if(ectol(ek,0,&strp))
		goto retn;

	// Print raw character, in encoded form if 8-bit.
	if((c = ek & 0xff) & 0x80) {
		sprintf(strp,"<%.2X>",c);
		strp = strchr(strp,'\0');
		}
	else {
		*strp++ = (ek & (PREFIX | CTRL)) ? lowcase[c] : c;
retn:
		*strp = '\0';
		}

	return strp;
	}

// Encode an extended key to a printable string, save result in destp, and return it.
char *ektos(uint ek,char *destp) {
	char *strp;
	static struct pkey {
		ushort code;
		enum cfid id;
		ushort flag;
		} pkeys[] = {
			{CTRL|'X',cf_prefix1,PREF1},
			{CTRL|'C',cf_prefix2,PREF2},
			{CTRL|'H',cf_prefix3,PREF3},
			{0,0,0}};

	// Do the non-Meta prefix keys first, giving preference to the default values (^X, ^C, and ^H) in case multiple
	// keys are bound to the same prefix.
	strp = destp;
	if(ek & (PREF1 | PREF2 | PREF3)) {
		ushort ek2;
		struct pkey *pkp = pkeys;

		do {
			if(ek & pkp->flag) {
				if(findPrefix(pkp->code) == pkp->flag) {
					ek2 = pkp->code;
					goto print1;
					}
				break;
				}
			} while((++pkp)->code != 0);

		// Default prefix key binding not found.  Find first binding in table instead.
		CFABPtr cfab = {PTRNUL};	// Pointer type not used.
		pkp = pkeys;
		for(;;) {
			if(ek & pkp->flag) {
				cfab.u.p_cfp = cftab + pkp->id;
				ek2 = getpentry(&cfab)->k_code;
print1:
				strp = ektos1(ek2,strp);
				*strp++ = ' ';
				break;
				}
			++pkp;
			}
		}
	else {
		// Print any meta or shift prefix literals.
		if(ek & META)
			strp = stpcpy(strp,"M-");
		if(ek & SHFT)
			strp = stpcpy(strp,"S-");
		}

	// Print the base character and return result.
	ektos1(ek,strp);
	return destp;
	}

// Get name associated with given KeyDesc object.
char *getkname(KeyDesc *kdp) {

	return (kdp == NULL) ? NULL : (kdp->k_cfab.p_type & PTRCMDTYP) ? kdp->k_cfab.u.p_cfp->cf_name :
	 (kdp->k_cfab.p_type == PTRMACRO) ? kdp->k_cfab.u.p_bufp->b_bname : NULL;
	}

// Find an alias or macro (by name) in the CFAM record list and return status or boolean result.  (1), if the CFAM record is
// found: if op is OPQUERY, return true; if op is OPCREATE, return rc.status; otherwise (op is OPDELETE), delete the CFAM
// record.  (2), if the CFAM record is not found: if op is OPCREATE, create a new entry with the given name and pointer type; if
// op is OPQUERY, return false, ignoring crpp; otherwise, return FATALERROR (should not happen).
int amfind(char *namep,int op,uint type) {
	CFAMRec *frp1,*frp2;
	int result;
	static char myname[] = "amfind";

	// Scan the CFAM record list.
	frp1 = NULL;
	frp2 = frheadp;
	do {
		if((result = strcmp(frp2->fr_name,namep)) == 0) {

			// Found it.  Now what?
			if(op == OPDELETE) {

				// Delete it from the list and free the storage.
				if(frp1 == NULL)
					frheadp = frp2->fr_nextp;
				else
					frp1->fr_nextp = frp2->fr_nextp;
				free((void *) frp2);

				return rc.status;
				}

			// Not a delete.
			return (op == OPQUERY) ? true : rc.status;
			}
		if(result > 0)
			break;
		frp1 = frp2;
		} while((frp2 = frp2->fr_nextp) != NULL);

	// No such CFAM record exists, create it?
	if(op == OPCREATE) {

		// Allocate the needed memory.
		if((frp2 = (CFAMRec *) malloc(sizeof(CFAMRec))) == NULL)
			return rcset(PANIC,0,text94,myname);
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
		frp2->fr_name = namep;
		frp2->fr_type = type;

		return rc.status;
		}

	// Entry not found and not a create.  Fatal error (a bug) if not OPQUERY.
	return (op == OPQUERY) ? false : rcset(FATALERROR,0,text16,myname,namep);
						// "%s(): No such entry '%s' to delete!"
	}

// Get one value from a coded string.  Return true if successful; otherwise, false.
static bool stoek1(char **klpp,ushort *cp,bool *firstp) {
	ushort c;
	ushort ek = *cp;
	struct clit *ltabp;
	char *klp = *klpp;

	// Loop until hit null or space.
	for(;;) {
		if(*klp == '\0')
			return false;

		// Prefix?
		if(klp[1] == '-') {
			switch(klp[0]) {
				case 'C':				// CTRL prefix.
				case 'c':
					++klp;
					goto ctrlpref;
				case 'M':				// META prefix.
				case 'm':
					if(!*firstp || (ek & META))
						return false;
					ek |= META;
					break;
				case 'S':				// SHIFT prefix.
				case 's':
					if(!*firstp || (ek & SHFT))
						return false;
					ek |= SHFT;
					break;
				default:
					return false;
				}
			klp += 2;
			}

		// Alternate control character form?
		else if((c = klp[0]) == '^') {
			if(klp[1] == '\0' || klp[1] == ' ')		// Bare '^'?
				goto vanilla;				// Yes, take it literally
ctrlpref:
			if(ek & CTRL)
				return false;
			ek |= CTRL;
			++klp;
			}

		// Function key?
		else if(strncasecmp(klp,"fn",2) == 0) {
			if(!*firstp || (ek & FKEY))
				return false;
			ek |= FKEY;
			klp += 2;
			}

		// Space or keyword?
		else {
			if(c == ' ')
				return false;
			ltabp = ltab;
			do {
				if(strncmp(klp,ltabp->kwp,3) == 0) {
					klp += 2;
					c = ltabp->ch & 0xff;
					ek |= ltabp->ch & ~0xff;
					goto vanilla;
					}
				} while((++ltabp)->kwp != NULL);

			// Not a keyword.  Literal control character? (boo, hiss)
			if(c < ' ' || c == 0x7f) {
				if(ek & CTRL)				// Duplicate?
					return false;			// Yes, error.
				ek |= CTRL;
				c ^= '@';				// Convert literal character to visible equivalent.
				++klp;
				}

			// Must be a vanilla character; that is, a printable (except space) or 8-bit character.  Move past it.
			else
vanilla:
				++klp;

			// Character is in c and prefix flag(s) may have been set.  Do sanity checks.
			if((ek & 0xff) || (*klp != '\0' && (*klp != ' ' || !*firstp)))
									// Second char, not end of value, or more than two?
				return false;				// Yes, error.
			if(ek & CTRL) {
				if(c == '@')				// C-@ or ^@ ?
					c = ' ';			// Yes, change back to a space.
				else if((c < '?' || c == '`' || c > 'z') && c != ' ')
					return false;			// Invalid character following C- or ^.
				}
			if((ek & (CTRL | META)) && (ek & SHFT))
				return false;

			// Make sure it's upper case if used with M-, C-, ^, follows a prefix, or solo S-.
			if((ek & (FKEY | SHFT)) == SHFT) {

				// Have solo 'S-'.  Error if printable character follows and it's not a letter.
				if(isletter(c)) {
					ek &= ~SHFT;
					goto upcase;
					}
				if((c >= ' ' && c < 'A') || (c > 'Z' && c < 'a') || (c > 'z' && c <= '~'))
					return false;
				}
			else if(!(ek & FKEY) && (ek & (PREFIX | CTRL)))
upcase:
				c = upcase[c];

			// Success.  Check if first of two values, not meta, and a prefix key.	If so, set flag and clear char.
			if(*firstp && !(ek & META) && *klp == ' ') {
				uint flag = findPrefix(ek | c);
				if(flag) {
					ek = flag;
					c = 0;
					}
				}

			// Return results.
			*klpp = klp;
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
//	M-	META prefix.
//	C-	CTRL prefix.
//	^	Alternate CTRL prefix.
//	S-	SHIFT prefix (for function key or character).
//	FN	Function prefix (which includes function keys and special keys like Delete and Up Arrow).
// All prefixes are case-insensitive.  Characters can be real control characters, printable characters, or any of the
// following literals:
//	DEL	Backspace key.
//	ESC	Escape key.
//	RTN	Return key.
//	SPC	Spacebar.
//	TAB	Tab key.
// The M-, S-, and FN prefixes are only valid on the first value, and all literals except ESC are only valid on the last value.
int stoek(char *keylitp,ushort *resultp) {
	ushort ek = 0;		// Extended key to return.
	char *klp = keylitp;	// Key literal.
	bool first = true;	// Decoding first value?

	// Parse it up.
	for(;;) {
		if(!stoek1(&klp,&ek,&first)) {
			*resultp = 0;
			return rcset(FAILURE,0,text254,keylitp);
					// "Invalid key literal '%s'"
			}
		if(*klp == '\0')
			break;
		if(*klp == ' ')
			++klp;
		}

	*resultp = ek;
	return rc.status;
	}
