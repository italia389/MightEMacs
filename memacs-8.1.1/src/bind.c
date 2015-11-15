// (c) Copyright 2015 Richard W. Marinelli
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

// Walk through all key binding lists and return next binding in sequence, or NULL if none left.  If init is true, reset
// to beginning and return first binding found.
KeyDesc *nextbind(bool init) {
	static KeyHdr *khp = NULL;
	static KeyDesc *kdp = NULL;
	KeyDesc *kdp1;

	if(init) {
		khp = keytab;
		kdp = khp->kh_headp;
		}

	while(kdp == NULL) {
		if(++khp == keytab + NPREFIX + 1)
			return NULL;
		kdp = khp->kh_headp;
		}
	kdp1 = kdp;
	kdp = kdp->k_nextp;
	return kdp1;
	}

// Return the number of entries in the binding table that match the given FAB pointer.
static int pentryct(FABPtr *fabp) {
	int count = 0;
	KeyDesc *kdp = nextbind(true);

	// Search for existing bindings for the command or macro.
	do {
		if(kdp->k_fab.u.p_voidp == fabp->u.p_voidp)
			++count;
		} while((kdp = nextbind(false)) != NULL);

	return count;
	}

// Scan the binding table for the first entry that matches the given FAB pointer and return it, or NULL if none found.
KeyDesc *getpentry(FABPtr *fabp) {
	KeyDesc *kdp = nextbind(true);

	// Search for an existing binding for the command or macro.
	do {
		if(kdp->k_fab.u.p_voidp == fabp->u.p_voidp)
			return kdp;
		} while((kdp = nextbind(false)) != NULL);

	return NULL;
	}

// Get a key binding (using given prompt if interactive) and save in result.  If n <= 0, get one key only; otherwise, get a key
// sequence.  Return status.
static int getkb(char *prmtp,int n,int *result) {

	// Script mode?
	if(opflags & OPSCRIPT) {
		Value *vtokp;

		// Yes, get next argument.
		if(vnew(&vtokp,false) != 0)
			return vrcset();
		if(macarg(vtokp,ARG_FIRST | ARG_NOTNULL | ARG_STR) == SUCCESS && (opflags & OPEVAL))
			(void) stoec(vtokp->v_strp,result);
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
	int c;			// Key to describe.
	char *strp,wkbuf[16];	// Work pointer and buffer.

	// Prompt the user for the key code.
	if(getkb(text13,n,&c) != SUCCESS)
			// "Show key "
		return rc.status;

	// Find the command.
	if((strp = getkname(getbind(c))) == NULL)
		strp = text48;
			// "[Not bound]"

	// Display result.
	return mlprintf(MLHOME,"%s %s",ectos(c,wkbuf,true),strp);
	}

// Remove given key entry from the binding table.
void unbindent(KeyDesc *kdp) {
	ushort c;
	KeyHdr *khp = keytab - 1;
	KeyDesc *kdp0,*kdp1;

	// Find the entry in the binding lists.
	kdp1 = NULL;
	for(;;) {
		while(kdp1 == NULL) {
			kdp0 = NULL;
			kdp1 = (++khp)->kh_headp;
			}
		if(kdp1 == kdp)
			break;
		kdp0 = kdp1;
		kdp1 = kdp1->k_nextp;
		}

	// Save the key code, relink, and free heap space.
	c = kdp->k_code;
	if(kdp0 == NULL) {
		khp->kh_headp = kdp->k_nextp;
		if(khp->kh_tailp == kdp)
			khp->kh_tailp = khp->kh_headp;
		}
	else {
		kdp0->k_nextp = kdp->k_nextp;
		if(khp->kh_tailp == kdp)
			khp->kh_tailp = kdp0;
		}
	(void) free((void *) kdp);

	// Update the global record (variable).
	if(ckeys.abort == c)
		ckeys.abort = 0;
	else if(ckeys.negarg == c)
		ckeys.negarg = 0;
	else if(ckeys.quote == c)
		ckeys.quote = 0;
	else if(ckeys.unarg == c)
		ckeys.unarg = 0;
	}

// Return proper binding list for given key.
static KeyHdr *bindlist(int c) {
	int i = 0;

	switch(c & (META | CPREF | HPREF | XPREF)) {
		case META:
			i = 1;
			break;
		case CPREF:
			i = 2;
			break;
		case HPREF:
			i = 3;
			break;
		case XPREF:
			i = 4;
		}
	return keytab + i;
	}

// Add a key to the binding table.
static int newcbind(int c,FABPtr *fabp) {
	KeyDesc *kdp;
	KeyHdr *khp = bindlist(c);

	// Get heap space for new entry ...
	if((kdp = (KeyDesc *) malloc(sizeof(KeyDesc))) == NULL)
		return rcset(PANIC,0,text94,"newcbind");
			// "%s(): Out of memory!"

	// Set keycode and the command or buffer pointer ...
	kdp->k_code = c;
	kdp->k_fab = *fabp;

	// and link it in.
	kdp->k_nextp = NULL;
	if(khp->kh_headp == NULL)
		khp->kh_headp = khp->kh_tailp = kdp;
	else
		khp->kh_tailp = khp->kh_tailp->k_nextp = kdp;

	return rc.status;
	}

// Load all the built-in key bindings.  Return status.
int loadbind(void) {
	FABPtr fab;
	KeyItem *kip = keyitems;

	fab.p_type = PTRCMD;
	do {
		fab.u.p_cfp = cftab + kip->ki_id;
		if(newcbind(kip->ki_code,&fab) != SUCCESS)
			break;
		} while((++kip)->ki_code != 0);

	return rc.status;
	}

// Get command or macro name.  Store command or buffer pointer in *fabp.  If interactive mode, pass prmtp to getcam().
// Return status.
static int getcm(char *prmtp,FABPtr *fabp) {

	if(opflags & OPSCRIPT) {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(!(opflags & OPEVAL)) {
			fabp->p_type = PTRNUL;
			fabp->u.p_voidp = NULL;
			}
		else if(!fabsearch(last->p_tok.v_strp,fabp,PTRCMD | PTRMACRO))
			return rcset(FAILURE,0,text130,last->p_tok.v_strp);
					// "No such command or macro '%s'"
		(void) getsym();
		}
	else
		(void) getcam(prmtp,PTRCMD | PTRMACRO,fabp,text130);
					// "No such command or macro '%s'"
	return rc.status;
	}

// Bind a key sequence to a command or macro.  Get single key if n <= 0.
int bindKeyCM(Value *rp,int n) {
	int c;				// Command key to bind.
	FABPtr fab;			// Pointer to the requested command or macro.
	KeyDesc *k_kdp,*c_kdp;
	char keybuf[16];
	char wkbuf[NWORK + 1];

	// Get the key or key sequence to bind.
	if(getkb(text15,n,&c) != SUCCESS)
			// "Bind key "
		return rc.status;

	// If interactive mode, build "progress" prompt.
	if(!(opflags & OPSCRIPT)) {
		if(mlputs(MLFORCE,ectos(c,keybuf,true)) != SUCCESS)
			return rc.status;
		sprintf(wkbuf,"%s%s %s %s",text15,keybuf,text339,text267);
			// "Bind key ","to","command or macro"
		}

	// Get the command or macro name.
	if(((opflags & OPSCRIPT) && !getcomma(true)) || getcm(wkbuf,&fab) != SUCCESS || fab.p_type == PTRNUL)
		return rc.status;

	// Binding a key sequence to a single-key command?
	if((c & KEYSEQ) && fab.p_type == PTRCMD && (fab.u.p_cfp->cf_flags & CFBIND1))
		return rcset(FAILURE,0,text17,keybuf,fab.u.p_cfp->cf_name);
			// "Cannot bind a key sequence '%s' to '%s' command"

	// If script mode and not evaluating, bail out here.
	if((opflags & (OPSCRIPT | OPEVAL)) == OPSCRIPT)
		return rc.status;

	// Interactive mode or evaluating.  Search the binding table to see if the key exists.
	if((k_kdp = getbind(c)) != NULL) {

		// If the key is already bound to this command or macro, it's a no op.
		if(k_kdp->k_fab.u.p_voidp == fab.u.p_voidp)
			return rc.status;

		// If the key is bound to a permanent-bind command and the only such binding, it can't be reassigned.
		if(k_kdp->k_fab.p_type == PTRCMD && (k_kdp->k_fab.u.p_cfp->cf_flags & CFPERM) && pentryct(&k_kdp->k_fab) < 2)
			return rcset(FAILURE,0,text210,wkbuf,k_kdp->k_fab.u.p_cfp->cf_name);
					// "'%s' is only binding to core command '%s' -- cannot delete or reassign"
		}

	// If binding to a command and the command is maintained in a global variable (for internal use), it can only have one
	// binding at most.
	if(fab.p_type == PTRCMD && (fab.u.p_cfp->cf_flags & CFUNIQ)) {

		// Search for an existing binding for the command and remove it.
		if((c_kdp = getpentry(&fab)) != NULL)
			unbindent(c_kdp);

		// Update the global record (variable).
		if(fab.u.p_cfp == cftab + cf_abort)
			ckeys.abort = c;
		else if(fab.u.p_cfp == cftab + cf_negativeArg)
			ckeys.negarg = c;
		else if(fab.u.p_cfp == cftab + cf_quoteChar)
			ckeys.quote = c;
		else
			ckeys.unarg = c;
		}

	// Key already in binding table?
	if(k_kdp != NULL) {

		// Yes, change it.
		k_kdp->k_fab = fab;
		return rc.status;
		}

	// Not in table.  Add it to the end.
	(void) newcbind(c,&fab);

	return rc.status;
	}

// Delete a key from the binding table.  Get single keystroke if n <= 0.  Ignore "key not bound" error if n > 0 and script mode.
int unbindKey(Value *rp,int n) {
	int c;			// Key to unbind.
	KeyDesc *kdp;
	char wkbuf[16];

	// Get the key or key sequence to unbind.
	if(getkb(text18,n,&c) != SUCCESS)
			// "Unbind key "
		return rc.status;

	// If script mode and not evaluating, bail out here.
	if((opflags & (OPSCRIPT | OPEVAL)) == OPSCRIPT)
		return rc.status;

	// Change key to something we can print.
	ectos(c,wkbuf,true);

	// Search the table to see if the key exists.
	if((kdp = getbind(c)) != NULL) {

		// If the key is bound to a permanent-bind command and the only such binding, it can't be deleted.
		if(kdp->k_fab.p_type == PTRCMD && (kdp->k_fab.u.p_cfp->cf_flags & CFPERM) && pentryct(&kdp->k_fab) < 2)
			return rcset(FAILURE,0,text210,wkbuf,kdp->k_fab.u.p_cfp->cf_name);
					// "'%s' is only binding to core command '%s' -- cannot delete or reassign"

		// It's a go ... unbind it.
		unbindent(kdp);
		}
	else if(!(opflags & OPSCRIPT) || n <= 0)
		return rcset(FAILURE,0,text14,wkbuf);
			// "%s not bound"

	// Dump it out if interactive.
	if(!(opflags & OPSCRIPT))
		(void) mlputs(MLFORCE,wkbuf);
	else if(n > 0 && vsetstr(kdp == NULL ? val_false : val_true,rp) != 0)
		(void) vrcset();

	return rc.status;
	}

// Find an alias by name and return status or boolean result.  (1), if the alias is found: if op is OPQUERY or OPCREATE, set
// *app (if app not NULL) to the alias structure associated with it; otherwise (op is OPDELETE), delete the alias and the
// associated CAM record.  If op is OPQUERY return true; othewise, return status.  (2), if the alias is not found: if op is
// OPCREATE, create a new entry, set its pointer record to *fabp, and set *app (if app not NULL) to it; if op is OPQUERY, return
// false, ignoring app; otherwise, return an error.
int afind(char *anamep,int op,FABPtr *fabp,Alias **app) {
	Alias *ap1,*ap2;
	int result;

	// Scan the alias list.
	ap1 = NULL;
	ap2 = aheadp;
	while(ap2 != NULL) {
		if((result = strcmp(ap2->a_name,anamep)) == 0) {

			// Found it.  Check op.
			if(op == OPDELETE) {

				// Delete the CAM record.
				if(camfind(anamep,OPDELETE,0) != SUCCESS)
					return rc.status;

				// Decrement alias use count on macro, if applicable.
				if(ap2->a_fab.p_type == PTRMACRO)
					--ap2->a_fab.u.p_bufp->b_nalias;

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
		if(((sym = getident(&strp)) != s_ident && sym != s_identq) || *strp != '\0')
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
		ap2->a_fab = *fabp;

		// Add its name to the CAM list.
		if(camfind(ap2->a_name,OPCREATE,PTRALIAS) != SUCCESS)
			return rc.status;

		if(app != NULL)
			*app = ap2;
		return rc.status;
		}

	// Alias not found and not a create.
	return (op == OPQUERY) ? false : rcset(FAILURE,0,text271,anamep);
							// "No such alias '%s'"
	}

// Create an alias to a command or macro.
int aliasCM(Value *rp,int n) {
	FABPtr fab;
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
	else if(termarg(vnamep,text215,NULL,CTRL | 'M',0) != SUCCESS || vistfn(vnamep,VNIL))
			// "Create alias "
		return rc.status;

	// Existing function, alias, macro, or user variable of same name?
	if((opflags & OPEVAL) && (fabsearch(vnamep->v_strp,NULL,PTRFAM) || uvarfind(vnamep->v_strp) != NULL))
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

		// Get the command or macro name.
		(void) getcm(NULL,&fab);
		}
	else {
		char wkbuf[strlen(text215) + strlen(vnamep->v_strp) + strlen(text325) + strlen(text267) + 8];

		// Get the command or macro name.
		sprintf(wkbuf,"%s%s %s %s",text215,vnamep->v_strp,text325,text267);
				// "Create alias ","for","command or macro"
		(void) getcm(wkbuf,&fab);
		}

	if(rc.status == SUCCESS && fab.p_type != PTRNUL) {

		// Create the alias.
		if(afind(vnamep->v_strp,OPCREATE,&fab,NULL) != SUCCESS)
			return rc.status;

		// Increment alias count on macro.
		if(fab.p_type == PTRMACRO)
			++fab.u.p_bufp->b_nalias;
		}

	return rc.status;
	}

// Delete one or more aliases or macros.  Return status.
int deleteAM(char *prmtp,uint selector,char *emsg) {
	FABPtr fab;

	// If interactive, get alias or macro name from user.
	if((opflags & OPSCRIPT) == 0) {
		if(getcam(prmtp,selector,&fab,emsg) != SUCCESS || fab.p_type == PTRNUL)
			return rc.status;
		goto nukeit;
		}

	// Script mode: get alias(es) or macro(s) to delete.
	do {
		if(!havesym(s_ident,false) && !havesym(s_identq,true))
			return rc.status;
		if(opflags & OPEVAL) {
			if(!fabsearch(last->p_tok.v_strp,&fab,selector))
				return rcset(FAILURE,0,emsg,last->p_tok.v_strp);
nukeit:
			// Nuke it.
			if(selector == PTRALIAS) {
				if(afind(fab.u.p_aliasp->a_name,OPDELETE,NULL,NULL) != SUCCESS)
					break;
				}
			else if(bdelete(fab.u.p_bufp,CLBIGNCHGD) != SUCCESS)
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
	if(getarg(mstrp,wkbuf,"",CTRL | 'M',ARG_FIRST | ARG_STR) == SUCCESS && vistfn(mstrp,VNIL))
		vnull(mstrp);

	return rc.status;
	}

// Write a list item to given buffer with padding.  Return status.
static int findkeys(StrList *rptp,int ktype,void *tp) {
	KeyDesc *kdp;
	Buffer *bufp;
	CmdFunc *cfp;
	bool first;
	char wkbuf[100];

	// Set pointers and store the command name and argument syntax.
	if(ktype == PTRMACRO) {
		bufp = (Buffer *) tp;
		strcpy(wkbuf,bufp->b_bname);
		}
	else {
		cfp = (CmdFunc *) tp;
		sprintf(wkbuf,"%s %s",cfp->cf_name,cfp->cf_usage);
		}

	// Search for any keys bound to command or buffer.
	kdp = nextbind(true);
	first = true;
	do {
		if(kdp->k_fab.p_type == ktype &&
		 ((ktype == PTRCMD ? (void *) kdp->k_fab.u.p_cfp : (void *) kdp->k_fab.u.p_bufp)) == tp) {
			ectos(kdp->k_code,pad(wkbuf,NBUFN + 3),true);	// Add the key sequence.
			if(!first) {
				if(vputc('\r',rptp) != 0)
					return vrcset();
				}
			else if(ktype == PTRCMD) {
				pad(wkbuf,NBUFN + 11);
				if(vputs(wkbuf,rptp) != 0)
					return vrcset();
				strcpy(wkbuf,cfp->cf_desc);		// Add the command description ...
				}
			if(vputs(wkbuf,rptp) != 0)
				return vrcset();
			first = false;
			*wkbuf = '\0';
			}
		} while((kdp = nextbind(false)) != NULL);

	// If no key was bound, we need to dump it anyway.
	if(wkbuf[0] != '\0') {
		if(ktype == PTRCMD) {
			pad(wkbuf,NBUFN + 11);
			if(vputs(wkbuf,rptp) != 0)
				return vrcset();
			strcpy(wkbuf,cfp->cf_desc);				// Add the command description ...
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
	bool needBreak,doapropos,skipLine;
	StrList rpt;
	Value *mstrp;			// Match string.
#if MMDEBUG & DEBUG_CAM
	char wkbuf[NBUFN + 12 + 16];
#else
	char wkbuf[NBUFN + 12];
#endif

	// If not default n, get match string.
	if((doapropos = n != INT_MIN)) {
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
		// Skip if a function, hidden command, or an apropos and command name doesn't contain the search string.
		if((cfp->cf_flags & (CFFUNC | CFHIDDEN)) || (doapropos && strcasestr(cfp->cf_name,mstrp->v_strp) == NULL))
			continue;

		if(needBreak && vputc('\r',&rpt) != 0)
			return vrcset();

		// Search for any keys bound to this command and add to the buffer.
		if(findkeys(&rpt,PTRCMD,(void *) cfp) != SUCCESS)
			return rc.status;
		needBreak = true;

		// On to the next command.
		} while((++cfp)->cf_name != NULL);

	// Scan the buffers, looking for macros and their bindings.
	bufp = bheadp;
	skipLine = true;
	do {
		// Is this buffer a macro?
		if((bufp->b_flags & BFMACRO) == 0)
			continue;

		// Skip if an apropos and buffer name doesn't contain the search string.
		if(doapropos && strcasestr(bufp->b_bname,mstrp->v_strp) == NULL)
			continue;

		// Add a blank line between the command and macro list.
 		if(skipLine) {
			if(needBreak && vputc('\r',&rpt) != 0)
				return vrcset();
			skipLine = false;
			}
		if(needBreak && vputc('\r',&rpt) != 0)
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
		if(doapropos && strcasestr(ap->a_name,mstrp->v_strp) == NULL && strcasestr(ap->a_fab.p_type == PTRMACRO ?
		 ap->a_fab.u.p_bufp->b_bname : ap->a_fab.u.p_cfp->cf_name,mstrp->v_strp) == NULL)
			continue;

		// Add a blank line between the macro and alias list.
 		if(skipLine) {
			if(needBreak && vputc('\r',&rpt) != 0)
				return vrcset();
			skipLine = false;
			}
		if(needBreak && vputc('\r',&rpt) != 0)
			return vrcset();

		// Add the alias to the string list.
		strcpy(wkbuf,ap->a_name);
		strcpy(pad(wkbuf,NBUFN + 3),"Alias");
		pad(wkbuf,NBUFN + 11);
		if(vputs(wkbuf,&rpt) != 0)
			return vrcset();
		strcpy(wkbuf,ap->a_fab.p_type == PTRMACRO ? ap->a_fab.u.p_bufp->b_bname : ap->a_fab.u.p_cfp->cf_name);
#if MMDEBUG & DEBUG_CAM
		sprintf(strchr(wkbuf,'\0')," (type %hu)",ap->a_fab.p_type);
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

// Change an extended key code to a string we can print out.  c is the key sequence to translate and destp is the destination
// string.  Use "SPC" for the space character if spc is true.  Return destp.
char *ectos(int c,char *destp,bool spc) {
	char *strp,*klp;
	uint *kvp;
	static uint keyval[] = {
		META,CPREF,HPREF,XPREF,SHFT,FKEY,0};
	static char *keylit = "M-C-H-X-S-FN";

	// Store corresponding two characters for each prefix.
	strp = destp;
	kvp = keyval;
	do {
		if(c & *kvp) {
			klp = keylit + (kvp - keyval) * 2;
			*strp++ = *klp++;
			*strp++ = *klp;
			}
		} while(*++kvp);

	// Apply control sequence if needed.
	if(c & CTRL) {

		// CTRL + <space> looks like ^@.
		if((c & 0xff) == ' ')
			c = '@';
		*strp++ = '^';
		}

	// Strip the prefixes and output the final sequence.
	if((*strp = c & 0xff) == ' ' && spc)
		strcpy(strp,"SPC");
	else
		strp[1] = '\0';

	return destp;
	}

// Look up a key binding in the binding table, given key.
KeyDesc *getbind(int c) {
	KeyDesc *kdp = bindlist(c)->kh_headp;

	// Scan through the binding table, looking for the key's entry.
	while(kdp != NULL) {
		if(kdp->k_code == c)
			return kdp;
		kdp = kdp->k_nextp;
		}

	// No such binding.
	return NULL;
	}

// Get name associated with given KeyDesc pointer entry.
char *getkname(KeyDesc *kdp) {
	CmdFunc *cfp,*kcfp;
	Buffer *bufp,*kbp;

	// If this isn't a valid key, it has no name.
	if(kdp == NULL)
		return NULL;

	// Loop through the command table, looking for a match.
	if(kdp->k_fab.p_type == PTRCMD) {
		kcfp = kdp->k_fab.u.p_cfp;
		cfp = cftab;
		do {
			if(cfp == kcfp)
				return cfp->cf_name;
			} while((++cfp)->cf_name != NULL);
		return NULL;
		}

	// Skim through the buffer list looking for a match.
	if(kdp->k_fab.p_type == PTRMACRO) {
		kbp = kdp->k_fab.u.p_bufp;
		bufp = bheadp;
		do {
			if(bufp == kbp)
				return bufp->b_bname;
			} while((bufp = bufp->b_nextp) != NULL);
		}

	return NULL;
	}

// Find a command, alias, or macro (by name) in the CAM record list and return status or boolean result.  (1), if the CAM record
// is found: if op is OPQUERY, return true; if op is OPCREATE, return rc.status; otherwise (op is OPDELETE), delete the CAM
// record.  (2), if the CAM record is not found: if op is OPCREATE, create a new entry with the given name and pointer type; if
// op is OPQUERY, return false, ignoring crpp; otherwise, return FATALERROR (should not happen).
int camfind(char *namep,int op,uint type) {
	CAMRec *crp1,*crp2;
	int result;
	static char myname[] = "camfind";

	// Scan the CAM record list.
	crp1 = NULL;
	crp2 = crheadp;
	do {
		if((result = strcmp(crp2->cr_name,namep)) == 0) {

			// Found it.  Now what?
			if(op == OPDELETE) {

				// Delete it from the list and free the storage.
				if(crp1 == NULL)
					crheadp = crp2->cr_nextp;
				else
					crp1->cr_nextp = crp2->cr_nextp;
				free((void *) crp2);

				return rc.status;
				}

			// Not a delete.
			return (op == OPQUERY) ? true : rc.status;
			}
		if(result > 0)
			break;
		crp1 = crp2;
		} while((crp2 = crp2->cr_nextp) != NULL);

	// No such CAM record exists, create it?
	if(op == OPCREATE) {

		// Allocate the needed memory.
		if((crp2 = (CAMRec *) malloc(sizeof(CAMRec))) == NULL)
			return rcset(PANIC,0,text94,myname);
				// "%s(): Out of memory!"

		// Find the place in the list to insert this CAM record.
		if(crp1 == NULL) {

			// Insert at the beginning.
			crp2->cr_nextp = crheadp;
			crheadp = crp2;
			}
		else {
			// Insert after crp1.
			crp2->cr_nextp = crp1->cr_nextp;
			crp1->cr_nextp = crp2;
			}

		// Set the other CAMRec members.
		crp2->cr_name = namep;
		crp2->cr_type = type;

		return rc.status;
		}

	// Entry not found and not a create.  Fatal error (a bug) if not OPQUERY.
	return (op == OPQUERY) ? false : rcset(FATALERROR,0,text16,myname,namep);
						// "%s(): No such entry '%s' to delete!"
	}

// Convert a coded string to an extended key code.  Set *resultp to zero if keylitp is invalid.  Return status.
//
//	A coded key binding consists of zero or more prefixes followed by a key.  Recognized prefixes must be in the
//	following order:
//
//	M-	Preceding META prefix
//	C-	Preceding C prefix
//	H-	Preceding H prefix
//	X-	Preceding X prefix
//	S-	Shifted function key or character
//	FN	Function key
//	^	Control key
//
//	All prefixes are case-insensitive.  Real control characters are allowed and "SPC" is allowed for a space character.
int stoec(char *keylitp,int *resultp) {
	int c,c2;		// Key sequence to return and work key.
	char *klp;

	if(*keylitp == '\0')
		goto bad;

	// Parse it up.
	klp = keylitp;
	c = 0;

	if(klp[1] == '-') {
		switch(klp[0]) {
			case 'M':			// META prefix.
			case 'm':
				c |= META;
				break;
			case 'X':			// X prefix.
			case 'x':
				c |= XPREF;
				break;
			case 'H':			// H prefix.
			case 'h':
				c |= HPREF;
				break;
			case 'C':			// C prefix.
			case 'c':
				c |= CPREF;
				break;
			case 'S':			// SHIFT prefix.
			case 's':
				c |= SHFT;
				klp += 2;
				goto fncheck;
			default:
				goto bad;
			}
		klp += 2;
		}

	// Check the function prefix.
	else {
fncheck:
//		if((klp[0] == 'F' || klp[0] == 'f') && (klp[1] == 'N' || klp[1] == 'n')) {
		if(strncasecmp(klp,"fn",2) == 0) {
			c |= FKEY;
			klp += 2;
			c2 = *klp;
			goto lencheck;			// Control key can't follow "FN".
			}
		}

	// An encoded control character?
	if((c2 = klp[0]) == '^') {
		if(klp[1] == '\0')			// Bare '^'?
			goto good;			// Yes, take it literally
		c |= CTRL;
		if((c2 = *++klp) == '@')		// ^@ ?
			c2 = ' ';			// Yes, change back to a space.
		else if(c2 < '?' || c2 == '`' || c2 > 'z')
			goto bad;			// Invalid character following '^'.
		}

	// SPC literal?
	else if(strncasecmp(klp,"spc",3) == 0) {
		klp += 2;
		c2 = ' ';
		goto lencheck;
		}

	// A literal control character? (boo, hiss)
	else if(c2 < ' ' || c2 == 0x7f) {
		if(c2 == '\0')				// Null byte?
			goto bad;			// Yes, error.
		c |= CTRL;
		c2 ^= '@';				// Convert literal character to visible equivalent.
		}

	// Too few or too many characters?
lencheck:
	if(c2 == '\0' || klp[1] != '\0')
		goto bad;

	// Make sure it's not lower case if used with isolated S- or M-, C-, H-, X-, or ^.
	if((c & (CTRL | KEYSEQ | SHFT)) == SHFT) {

		// Have solo 'S-'.  Error if printable character other than a letter follows.
		if(isletter(c2)) {
			c &= ~SHFT;
			goto upcase;
			}
		if((c2 >= ' ' && c2 < 'A') || (c2 > 'Z' && c2 < 'a') || (c2 > 'z' && c2 <= '~')) {
bad:
			*resultp = 0;
			return rcset(FAILURE,0,text254,keylitp);
					// "Invalid key literal '%s'"
			}
		}
	if(!(c & FKEY) && (c & (CTRL | PREFIX)))
upcase:
		c2 = upcase[c2];
good:
	// The final sequence ...
	c |= c2;
	*resultp = c;
	return rc.status;
	}
