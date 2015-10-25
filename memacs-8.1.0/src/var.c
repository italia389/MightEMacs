// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// var.c	Routines dealing with variables for MightEMacs.

#include "os.h"
#include <ctype.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"

// Make selected global definitions local.
#define DATAvar

#include "evar.h"

// Return true if a variable is an integer type, given descriptor; otherwise, false.
bool intvar(VDesc *vdp) {
	Value *vp;

	switch(vdp->vd_type) {
		case VTYP_LVAR:
		case VTYP_GVAR:
			vp = vdp->u.vd_uvp->uv_vp;
			break;
		case VTYP_SVAR:
			return (vdp->u.vd_svp->sv_flags & V_INT) != 0;
		default:	// VTYP_NVAR.
			{MacArgList *malp = vdp->u.vd_malp;
			ushort argnum = vdp->vd_argnum;

			// Get argument value.  $0 resolves to the macro "n" argument.
			vp = (argnum == 0) ? scriptrun->nargp : marg(malp,argnum)->ma_valp;
			}
		}
	return vp->v_type == VALINT;
	}

// Return true if c is a valid first character of an identifier; otherwise, false.
bool isident1(int c) {

	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}

// Free user variable(s), given "stack" pointer.  All variables will be together at top of list (because they are created in
// stack fashion during macro execution and/or recursion).
void uvarclean(UVar *vstackp) {
	UVar *uvp;

	while(lvarsheadp != vstackp) {
		uvp = lvarsheadp->uv_nextp;

		// Free value ...
#if VDebug
		vdelete(lvarsheadp->uv_vp,"uvarclean");
#else
		vdelete(lvarsheadp->uv_vp);
#endif

		// free variable ...
		free((void *) lvarsheadp);

		// and advance head pointer.
		lvarsheadp = uvp;
		}
	}

// Search global or local variable list for given name (with prefix).  If found, return pointer to UVar record; otherwise,
// return NULL.  Local variable entries beyond scriptrun->uvp are not considered so that all local variables created or
// referenced in a particular macro invocation are visible and accessible only from that macro, which allows recursion to work
// properly.
UVar *uvarfind(char *varp) {
	UVar *uvp;
	UVar *vstackp;		// Search boundary in local variable table.
	char *namep;

	if(*varp == TKC_GVAR) {
		uvp = gvarsheadp;
		vstackp = NULL;
		namep = varp + 1;
		}
	else {
		uvp = lvarsheadp;
		vstackp = (scriptrun == NULL) ? NULL : scriptrun->uvp;
		namep = varp;
		}

	while(uvp != vstackp) {
		if(strcmp(namep,uvp->uv_name) == 0)
			return uvp;
		uvp = uvp->uv_nextp;
		}

	return NULL;
	}

// Return system variable name, given index.  Used in binary() calls.
static char *svarname(int i) {

	return sysvars[i].sv_name;
	}

// Perform binary search given key string, table-fetch function, and table size.  Return index (>= 0) if found; otherwise, -1.
int binary(char *key,char *(*tval)(),int tlength) {
	uint l,u;		// Lower and upper limits of binary search.
	uint i;			// Current search index.
	int cresult;		// Result of comparison.

	// Set current search limit to entire list.
	l = 0;
	u = tlength - 1;

	// Loop until a match or list shrinks to zero items.
	while(u >= l) {

		// Get the midpoint.
		i = (l + u) >> 1;

		// Do the comparison.
		cresult = strcmp(key,tval(i));
		if(cresult == 0)
			return i;
		if(cresult < 0) {
			if(i == 0)
				break;
			u = i - 1;
			}
		else
			l = i + 1;
		}

	return -1;
	}

// Get most recent kill (of unlimited size) and save in rp.  May be a null string.  Return status.
static int getkill(Value *rp) {
	char *strp;			// Pointer into KillBuf block data chunk.
	KillBuf *kptr;			// Pointer to the current KillBuf block.
	int counter;			// Index into data chunk.
	StrList kill;

	// If no kill buffer, nothing to do!
	if(kringp->kbufh == NULL) {
		vnull(rp);
		return rc.status;
		}

	// Set up the output object.
	if(vopen(&kill,rp,false) != 0)
		return vrcset();

	// Backed up characters?
	kptr = kringp->kbufh;
	if((counter = kringp->kskip) > 0) {
		strp = kptr->kl_chunk + counter;
		while(counter++ < KBLOCK)
			if(vputc(*strp++,&kill) != 0)
				return vrcset();
		kptr = kptr->kl_next;
		}

	if(kptr != NULL) {
		while(kptr != kringp->kbufp) {
			strp = kptr->kl_chunk;
			for(counter = 0; counter < KBLOCK; ++counter)
				if(vputc(*strp++,&kill) != 0)
					return vrcset();
			kptr = kptr->kl_next;
			}
		counter = kringp->kused;
		strp = kptr->kl_chunk;
		while(counter--)
			if(vputc(*strp++,&kill) != 0)
				return vrcset();
		}

	// and return the reconstructed value.
	return vclose(&kill) == 0 ? rc.status : vrcset();
	}

// Store the list of characters considered "in a word" into rp.  Return status.
static int getwlist(Value *rp) {

	// If we are defaulting to a standard word char list ...
	if((opflags & OPWORDLST) == 0)
		vnull(rp);
	else {
		StrList sl;
		char *strp,*strpz;

		if(vopen(&sl,rp,false) != 0)
			return vrcset();

		// Build the string of characters in the result buffer.
		strpz = (strp = wordlist) + 256;
		do {
			if(*strp && vputc(strp - wordlist,&sl) != 0)
				return vrcset();
			} while(++strp < strpz);

		if(vclose(&sl) != 0)
			return vrcset();
		}

	return rc.status;
	}

// Get name of command or macro bound to a hook and return pointer to it.
static char *gethook(int id) {
	FABPtr *fabp = &hooktab[id].h_fab;

	return (fabp->p_type == PTRNUL) ? "" : (fabp->p_type == PTRMACRO) ? fabp->u.p_bufp->b_bname + 1 :
	 fabp->u.p_cfp->cf_name;
	}

// Replace the current line with the given text.  Return status.  (Used only for setting the $lineText system variable.)
static int putctext(char *iline) {

	if(allowedit(true) != SUCCESS)		// Don't allow if in read-only mode.
		return rc.status;

	// Delete the current line.
	curwp->w_face.wf_dot.off = 0;		// Start at the beginning of the line.
	if(kdctext(1,-1,NULL) != SUCCESS)	// Put it in the kill buffer.
		return rc.status;

	// Insert the new line.
	if(linstr(iline) != SUCCESS)
		return rc.status;
	if(lnewline() == SUCCESS)
		(void) backln(1);
	return rc.status;
	}

// Get current window number.  (For macro use.)
static int getcwnum(void) {
	EWindow *winp;
	int num;

	num = 1;
	winp = wheadp;
	while(winp != curwp) {
		winp = winp->w_nextp;
		++num;
		}
	return num;
	}

// Encode the current keyboard macro into dest in string form using ectos().  Return status.
static int kmtos(Value *destp) {

	// Recording a keyboard macro?
	if(kmacro.km_state == KMRECORD) {
		clearKeyMacro(true);
		return rcset(FAILURE,0,text338);
			// "Cannot access '$keyMacro' from a keyboard macro, cancelled"
		}

	// Null keyboard macro?
	if(kmacro.km_slotp == kmacro.km_buf)
		vnull(destp);
	else {
		ushort *kmp;
		char *strp;
		StrList dest;
		char wkbuf[16];

		// Find a delimter that can be used (a character that is not in the macro).  Default to tab.
		strp = KMDELIMS;
		wkbuf[0] = '\t';
		do {
			kmp = kmacro.km_buf;
			do {
				if(*kmp == *strp)
					goto next;
				} while(++kmp < kmacro.km_endp);

			// Found.
			wkbuf[0] = *strp;
			break;
next:;
			} while(*++strp != '\0');

		// Loop through keyboard macro keys and translate each into dest with delimiter found in previous step.
		if(vopen(&dest,destp,false) != 0)
			return vrcset();
		kmp = kmacro.km_buf;
		do {
			ectos(*kmp,wkbuf + 1,false);
			if(vputs(wkbuf,&dest) != 0)
				return vrcset();
			} while(++kmp < kmacro.km_endp);
		if(vclose(&dest) != 0)
			return vrcset();
		}

	return rc.status;
	}

// Get value of a system variable, given result pointer and table pointer.
static int getsvar(Value *rp,SVar *svp) {
	enum svarid vid;
	char *strp;
	char wkbuf[16];

	// Fetch the corresponding value.
	if(svp->sv_vp != NULL) {
		if(vcpy(rp,svp->sv_vp) != 0)
			(void) vrcset();
		goto retn;
		}
	else {
		int i;
		switch(vid = svp->sv_id) {
			case sv_ArgCount:
				vsetint(scriptrun == NULL ? 0L : (long) scriptrun->malp->mal_count,rp);
				break;
			case sv_BufCount:
				vsetint((long) bufcount(),rp);
				break;
			case sv_BufInpDelim:
				strp = curbp->b_inpdelim;
				goto kopy;
			case sv_BufList:
				getbuflist(rp);
				break;
			case sv_BufOtpDelim:
				strp = curbp->b_otpdelim;
				goto kopy;
			case sv_BufSize:
				vsetint(bufsize(curbp),rp);
				break;
			case sv_Date:
				strp = timeset();
				goto kopy;
#if TYPEAHEAD
			case sv_KeyPending:
				{int count;

				if(typahead(&count) != SUCCESS)
					return rc.status;
				(void) ltos(rp,count > 0);
				}
				break;
#endif
			case sv_KillText:
				(void) getkill(rp);
				break;
			case sv_LineLen:
				vsetint((long) lused(curwp->w_face.wf_dot.lnp),rp);
				break;
			case sv_Match:
				strp = fixnull(srch.patmatch);
				goto kopy;
			case sv_RegionText:
				(void) getregtext(rp);
				break;
			case sv_ReturnMsg:
				strp = scriptrc.msg.v_strp;
				goto kopy;
			case sv_RunFile:
				strp = fixnull((scriptrun == NULL) ? NULL : scriptrun->path);
				goto kopy;
			case sv_RunName:
				{Buffer *bufp = (scriptrun == NULL) ? NULL : scriptrun->bufp;
				strp = fixnull(bufp == NULL ? NULL : *bufp->b_bname == SBMACRO ? bufp->b_bname + 1 :
				 bufp->b_bname);
				}
				goto kopy;
			case sv_TermCols:
				vsetint((long) term.t_ncol,rp);
				break;
			case sv_TermRows:
				vsetint((long) term.t_nrow,rp);
				break;
			case sv_WorkDir:
				(void) getwkdir(&strp,false);
				goto kopy;
			case sv_WindCount:
				vsetint((long) wincount(),rp);
				break;
			case sv_argIndex:
				if(scriptrun == NULL) {
					vsetint(1L,rp);
					}
				else {
					long lval = 1;
					MacArg *margp = scriptrun->malp->mal_headp;
					while(margp != scriptrun->malp->mal_argp) {
						++lval;
						margp = margp->ma_nextp;
						}
					vsetint(lval,rp);
					}
				break;
			case sv_autoSave:
				vsetint((long) gasave,rp);
				break;
			case sv_bufFile:
				strp = defnil(curbp->b_fname);
				goto kopy;
			case sv_bufFlags:
				vsetint((long) curbp->b_flags,rp);
				break;
			case sv_bufLineNum:
				vsetint(getlinenum(curbp,curwp->w_face.wf_dot.lnp),rp);
				break;
			case sv_bufName:
				strp = curbp->b_bname;
				goto kopy;
			case sv_bufModes:
				vsetint((long) curbp->b_modes,rp);
				break;
			case sv_defModes:
				vsetint((long) modetab[MDR_DEFAULT].flags,rp);
				break;
#if COLOR
			case sv_desktopColor:
				strp = (char *) cname[deskcolor];
				goto kopy;
#endif
			case sv_enterBufHook:
				i = HKENTRBUF;
				goto gethook;
			case sv_execPath:
				strp = execpath;
				goto kopy;
			case sv_exitBufHook:
				i = HKEXITBUF;
				goto gethook;
			case sv_fencePause:
				vsetint((long) fencepause,rp);
				break;
			case sv_globalModes:
				vsetint((long) modetab[MDR_GLOBAL].flags,rp);
				break;
			case sv_hardTabSize:
				vsetint((long) htabsize,rp);
				break;
			case sv_helpHook:
				i = HKHELP;
				goto gethook;
			case sv_horzJump:
				vsetint((long) hjump,rp);
				break;
			case sv_horzScrollCol:
				vsetint((long) curwp->w_face.wf_fcol,rp);
				break;
			case sv_inpDelim:
				strp = fi.inpdelim;
				goto kopy;
			case sv_keyMacro:
				(void) kmtos(rp);
				break;
			case sv_lastKeySeq:
				ectos(kentry.lastkseq,strp = wkbuf,false);
				goto kopy;
			case sv_lineChar:
				{Dot *dotp = &curwp->w_face.wf_dot;
				vsetint((long) (lused(dotp->lnp) == dotp->off ? '\r' :
				 lgetc(dotp->lnp,dotp->off)),rp);
				}
				break;
			case sv_lineCol:
				vsetint((long) getccol(),rp);
				break;
			case sv_lineOffset:
				vsetint((long) curwp->w_face.wf_dot.off,rp);
				break;
			case sv_lineText:
				{Line *lnp = curwp->w_face.wf_dot.lnp;
				if(vsetfstr(ltext(lnp),lused(lnp),rp) != 0)
					return vrcset();
				}
				break;
			case sv_maxLoop:
				vsetint((long) maxloop,rp);
				break;
			case sv_maxRecursion:
				vsetint((long) maxrecurs,rp);
				break;
			case sv_modeHook:
				i = HKMODE;
				goto gethook;
			case sv_otpDelim:
				strp = fi.otpdelim;
				goto kopy;
			case sv_pageOverlap:
				vsetint((long) overlap,rp);
				break;
#if COLOR
			case sv_palette:
				strp = palstr;
				goto kopy;
#endif
			case sv_postKeyHook:
				i = HKPOSTKEY;
				goto gethook;
			case sv_preKeyHook:
				i = HKPREKEY;
				goto gethook;
			case sv_randNumSeed:
				vsetint((long) randseed,rp);
				break;
			case sv_readHook:
				i = HKREAD;
				goto gethook;
			case sv_replace:
				strp = srch.rpat;
				goto kopy;
			case sv_screenNum:
				vsetint((long) cursp->s_num,rp);
				break;
			case sv_search:
				strp = srch.pat;
				goto kopy;
			case sv_searchDelim:
				ectos(srch.sdelim,strp = wkbuf,false);
				goto kopy;
			case sv_showModes:
				vsetint((long) modetab[MDR_SHOW].flags,rp);
				break;
			case sv_softTabSize:
				vsetint((long) stabsize,rp);
				break;
			case sv_travJumpSize:
				vsetint((long) tjump,rp);
				break;
			case sv_vertJump:
				vsetint((long) vjump,rp);
				break;
			case sv_windLineNum:
				vsetint((long) getwpos(),rp);
				break;
			case sv_windNum:
				vsetint((long) getcwnum(),rp);
				break;
			case sv_windSize:
				vsetint((long) curwp->w_nrows,rp);
				break;
			case sv_wordChars:
				(void) getwlist(rp);
				break;
			case sv_wrapCol:
				vsetint((long) wrapcol,rp);
				break;
			case sv_wrapHook:
				i = HKWRAP;
				goto gethook;
			case sv_writeHook:
				i = HKWRITE;
gethook:
				strp = gethook(i);
				goto kopy;
			default:
				// Never should get here.
				return rcset(FATALERROR,0,text3,"getsvar",vid,svp->sv_name);
						// "%s(): Unknown id %d for var '%s'!"
			}
		goto retn;
		}
kopy:
	if(vsetstr(strp,rp) != 0)
		return vrcset();
retn:
	return rc.status;
	}

// Set an alternative list of characters to be considered in a word.  Return status.
static int setwlist(Value *wclistp) {

	// If we are turning this facility off, just flag so.
	if(visnull(wclistp) || vistfn(wclistp,VNIL))
		opflags &= ~OPWORDLST;
	else {
		char *strp,*strpz;
		StrList sl;

		// First, clear the table ...
		strpz = (strp = wordlist) + 256;
		do {
			*strp++ = false;
			} while(strp < strpz);

		// expand the new value ...
		if(strexpand(&sl,wclistp) != SUCCESS)
			return rc.status;

		// and for each character in the new value, set that element in the table.
		strp = sl.sl_vp->v_strp;
		while(*strp != '\0')
			wordlist[(int) *strp++] = true;

		opflags |= OPWORDLST;
		}
	return rc.status;
	}

// Decode and save a keyboard macro from a string containing encoded keys separated by semicolons.  The first character of the
// string is the delimiter.  Error if not in the KMSTOPped state.  Return status.
static int stokm(char *valp) {
	int delim;

	// Make sure a keyboard macro is not currently being recorded or played.
	clearKeyMacro(false);
	if(kmacro.km_state != KMSTOP) {
		if(kmacro.km_state == KMRECORD)
			curwp->w_flags |= WFMODE;
		kmacro.km_state = KMSTOP;
		return rcset(FAILURE,0,text338);
			// "Cannot access '$keyMacro' from a keyboard macro, cancelled"
		}

	// Get delimiter (first character) and parse string.
	if((delim = *valp++) != '\0' && *valp != '\0') {
		int c,ec;
		enum cfid cf;
		FABPtr fab;
		bool last;
		Value *vp;

		if(vnew(&vp,false) != 0)
			return vrcset();

		// Parse tokens and save in keyboard macro array.
		while(parsetok(vp,&valp,delim) != NOTFOUND) {

			// Convert token string to a key sequence.
			if(*vp->v_strp == '\0')
				return rcset(FAILURE,0,text254,"");
					// "Invalid key literal '%s'"
			if(stoec(vp->v_strp,&ec) != SUCCESS)
				break;

			// Loop once or twice, saving high and low values.
			last = false;
			do {
				// Have a prefix key?
				switch(ec & (META | CPREF | HPREF | XPREF)) {
					case META:
						cf = cf_metaPrefix;
						break;
					case XPREF:
						cf = cf_xPrefix;
						break;
					case HPREF:
						cf = cf_hPrefix;
						break;
					case CPREF:
						cf = cf_cPrefix;
						break;
					default:
						// No prefix.  Save extended character.
						c = ec;
						last = true;
						goto saveit;
					}

				// Get the key binding.
				fab.u.p_cfp = cftab + cf;
				c = getpentry(&fab)->k_code;
				ec &= ~(META | CPREF | HPREF | XPREF);
saveit:
				// Save key if room.
				if(kmacro.km_slotp == kmacro.km_buf + NKBDM) {
					clearKeyMacro(true);
					return rcset(FAILURE,0,text266,NKBDM);
						// "Maximum number of keyboard macro entries (%d) exceeded!"
					}
				*kmacro.km_slotp++ = c;
				} while(!last);
			}
		kmacro.km_endp = kmacro.km_slotp;
		}

	return rc.status;
	}

// Set a hook table entry to the given command or buffer name.
static int sethook(int id,char *namep) {
	FABPtr *fabp = &hooktab[id].h_fab;
	CmdFunc *cfp;

	// Are we unbinding it?
	if(*namep == '\0' || isnil(namep)) {
		fabp->p_type = PTRNUL;
		return rc.status;
		}

	// Binding to a built-in command?
	if((cfp = ffind(namep)) != NULL) {
		if(cfp->cf_flags & CFFUNC)
			goto bad;
		fabp->u.p_cfp = cfp;
		fabp->p_type = PTRCMD;
		return rc.status;
		}

	// Nope.  Is it a macro?
	else {
		Buffer *bufp;
		char bufn[NBUFN + 1];

		sprintf(bufn,MACFORMAT,NBUFN - 1,namep);
		if((bufp = bsrch(bufn,NULL)) != NULL) {
			fabp->u.p_bufp = bufp;
			fabp->p_type = PTRMACRO;
			return rc.status;
			}
		}
bad:
	// Not anything we can bind to.
	return rcset(FAILURE,0,text130,namep);
		// "No such command or macro '%s'"
	}

// Set a variable to given value (the result of an expression which has already been evaluated).  Return status.
int putvar(Value *valp,VDesc *vdp) {
	char *strp1;
	static char myname[] = "putvar";

	// Set the appropriate value.
	switch(vdp->vd_type) {

		// Set a user variable.
		case VTYP_LVAR:
		case VTYP_GVAR:
			{UVar *uvp = vdp->u.vd_uvp;				// Grab pointer to old value.
			if(vcpy(uvp->uv_vp,valp) != 0)
				return vrcset();
			uvp->uv_flags &= ~V_NULLTOK;				// Clear "null token" flag.
			}
			break;

		// Set a system variable.
		case VTYP_SVAR:
			{int c;
			SVar *svp = vdp->u.vd_svp;
			Value *vsinkp;						// For throw-away return value, if any.

			svp->sv_flags &= ~V_NULLTOK;				// Clear "null token" flag.

			// Check if variable requires integer or string value.
			if(svp->sv_flags & V_INT) {
				if(!intval(valp))
					goto badtyp0;
				}
			else if(!strval(valp)) {
				StrList msg;
badtyp0:
				strp1 = svp->sv_name;
badtyp1:
				return (vopen(&msg,&rc.msg,true) != 0 ||
				 vputf(&msg,text334,TKC_GVAR,strp1) != 0 || vclose(&msg) != 0) ? vrcset() : rc.status;
						// ", setting variable '%c%s'"
				}

			// Do specific action for referenced variable.
			if(vnew(&vsinkp,false) != 0)
				return vrcset();
			switch(svp->sv_id) {
				case sv_ArgCount:
				case sv_BufCount:
				case sv_BufInpDelim:
				case sv_BufOtpDelim:
				case sv_BufSize:
				case sv_Date:
#if TYPEAHEAD
				case sv_KeyPending:
#endif
				case sv_KillText:
				case sv_LineLen:
				case sv_Match:
				case sv_RegionText:
				case sv_ReturnMsg:
				case sv_RunFile:
				case sv_RunName:
				case sv_TermCols:
				case sv_TermRows:
				case sv_WindCount:
					return rcset(FAILURE,0,text164,svp->sv_name);
							// "Cannot modify read-only variable '%s'"
				case sv_argIndex:
					if(valp->u.v_int <= 0) {
						c = 1;
						goto erange;
						}
					if(scriptrun != NULL) {
						MacArgList *malp = scriptrun->malp;
						malp->mal_argp = malp->mal_headp;
						while(malp->mal_argp != NULL && --valp->u.v_int > 0)
							malp->mal_argp = malp->mal_argp->ma_nextp;
						}
					break;
				case sv_autoSave:
					{Buffer *bufp = bheadp;
					gasave = valp->u.v_int;
					do {
						if(bufp->b_acount > gasave)
							bufp->b_acount = gasave;
						} while((bufp = bufp->b_nextp) != NULL);
					}
					break;
				case sv_bufFile:
					strp1 = "setBufFile ";
					goto setname;
				case sv_bufFlags:
					curbp->b_flags = (curbp->b_flags & ~(BFCHGD | BFHIDDEN)) |
					 (valp->u.v_int & (BFCHGD | BFHIDDEN));
					if(valp->u.v_int & BFCHGD)
						lchange(curbp,WFMODE);
					break;
				case sv_bufLineNum:
					(void) gotoLine(vsinkp,valp->u.v_int);
					break;
				case sv_bufModes:
					(void) adjustmode(NULL,1,3,valp);
					break;
				case sv_bufName:
					strp1 = "setBufName ";
setname:
					{StrList cmd;

					// Build command and quote string value so it can be re-evaluated.
					if(vopen(&cmd,NULL,false) != 0 || vputs(strp1,&cmd) != 0)
						return vrcset();
					if(quote(&cmd,valp->v_strp,true) == SUCCESS) {
						if(vclose(&cmd) != 0)
							(void) vrcset();
						else
							(void) doestmt(vsinkp,cmd.sl_vp->v_strp,TKC_COMMENT,NULL);
						}
					}
					break;
				case sv_defModes:
					(void) adjustmode(NULL,1,MDR_DEFAULT,valp);
					break;
#if COLOR
				case sv_desktopColor:
					if((c = lookup_color(mkupper(rp,valp->v_strp))) == -1)
						return rcset(FAILURE,0,text242,valp->v_strp);
								// "No such color '%s'"
					deskcolor = c;
					(void) refreshScreens();
					break;
#endif
				case sv_enterBufHook:
					c = HKENTRBUF;
					goto sethook;
				case sv_execPath:
					(void) setpath(valp->v_strp,false);
					break;
				case sv_exitBufHook:
					c = HKEXITBUF;
					goto sethook;
				case sv_fencePause:
					if(valp->u.v_int < 0)
						return rcset(FAILURE,0,text39,text119,(int) valp->u.v_int,0);
							// "%s (%d) must be %d or greater","Pause duration"
					fencepause = valp->u.v_int;
					break;
				case sv_globalModes:
					(void) adjustmode(NULL,1,MDR_GLOBAL,valp);
					break;
				case sv_hardTabSize:
					if(settab((int) valp->u.v_int,true) != SUCCESS)
						return rc.status;
					uphard();
					break;
				case sv_helpHook:
					c = HKHELP;
					goto sethook;
				case sv_horzJump:
					if((hjump = valp->u.v_int) < 0)
						hjump = 0;
					else if(hjump > JUMPMAX)
						hjump = JUMPMAX;
					if((hjumpcols = hjump * term.t_ncol / 100) == 0)
						hjumpcols = 1;
					break;
				case sv_horzScrollCol:
					curwp->w_face.wf_fcol = valp->u.v_int < 0 ? 0 : valp->u.v_int;
					curwp->w_flags |= WFHARD | WFMODE;
					break;
				case sv_inpDelim:
					if(strlen(valp->v_strp) > sizeof(fi.inpdelim) - 1)
						return rcset(FAILURE,0,text251,text46,valp->v_strp,sizeof(fi.inpdelim) - 1);
							// "%s delimiter '%s' cannot be more than %d character(s)","Input"
					strcpy(fi.inpdelim,valp->v_strp);
					break;
				case sv_keyMacro:
					(void) stokm(valp->v_strp);
					break;
				case sv_lastKeySeq:
					if(stoec(valp->v_strp,&c) == SUCCESS) {
						KeyDesc *kdp = getbind(c);
						if(kdp != NULL) {
							FABPtr *fabp = &kdp->k_fab;
							if(fabp->p_type == PTRCMD && (fabp->u.p_cfp->cf_flags & CFHIDDEN))
								return rcset(FAILURE,0,text333,svp->sv_name);
										// "Illegal value for '$%s' variable"
							}
						kentry.lastkseq = c;
						kentry.uselast = true;
						}
					break;
				case sv_lineChar:
					// Replace character under cursor.
					if(ldelete(1L,0) != SUCCESS)
						return rcset(FAILURE,0,text142,curbp->b_bname);
							// "Cannot change a character past end of buffer '%s'"
					if((valp->u.v_int == '\r' ? lnewline() : linsert(1,(int) valp->u.v_int)) != SUCCESS)
						return rc.status;
					(void) backch(1);
					break;
				case sv_lineCol:
					(void) setccol(valp->u.v_int);
					break;
				case sv_lineOffset:
					{int llen = lused(curwp->w_face.wf_dot.lnp);
					int loff = (valp->u.v_int < 0) ? llen + valp->u.v_int : valp->u.v_int;
					if(loff < 0 || loff > llen)
						return rcset(FAILURE,0,text224,valp->u.v_int);
							// "Line offset value %ld out of range"
					curwp->w_face.wf_dot.off = loff;
					curwp->w_flags |= WFMOVE;
					}
					break;
				case sv_lineText:
					(void) putctext(valp->v_strp);
					break;
				case sv_maxLoop:
					if(valp->u.v_int < 0) {
						c = 0;
						goto erange;
						}
					maxloop = valp->u.v_int;
					break;
				case sv_maxRecursion:
					if(valp->u.v_int < 0) {
						c = 0;
erange:
						return rcset(FAILURE,0,text111,svp->sv_name,c);
							// "'%s' value must be %d or greater"
						}
					maxrecurs = valp->u.v_int;
					break;
				case sv_modeHook:
					c = HKMODE;
					goto sethook;
				case sv_otpDelim:
					if((ulong) (c = strlen(valp->v_strp)) > sizeof(fi.otpdelim) - 1)
						return rcset(FAILURE,0,text251,text47,valp->v_strp,sizeof(fi.otpdelim) - 1);
							// "%s delimiter '%s' cannot be more than %d character(s)","Output"
					strcpy(fi.otpdelim,valp->v_strp);
					fi.otpdelimlen = c;
					break;
				case sv_pageOverlap:
					if(valp->u.v_int < 0 || valp->u.v_int > (int) (term.t_nrow - 1) / 2)
						return rcset(FAILURE,0,text184,valp->u.v_int,(int) (term.t_nrow - 1) / 2);
							// "Overlap %ld must be between 0 and %d"
					overlap = valp->u.v_int;
					break;
#if COLOR
				case sv_palette:
					strp1 = palstr;
					if(spal(valp->v_strp) == SUCCESS)
						(void) chkcpy(&strp1,valp->v_strp,NPALETTE + 1);
					break;
#endif
				case sv_postKeyHook:
					c = HKPOSTKEY;
					goto sethook;
				case sv_preKeyHook:
					c = HKPREKEY;
					goto sethook;
				case sv_randNumSeed:
					if((randseed = abs(valp->u.v_int)) == 0)  // Force it to be between 1 and 2**31 - 2.
						randseed = 1;
					else if(randseed > 0x7ffffffe)
						randseed = 0x7ffffffe;
					break;
				case sv_readHook:
					c = HKREAD;
					goto sethook;
				case sv_replace:
					strp1 = srch.rpat;
					if(chkcpy(&strp1,valp->v_strp,NPAT + 1) != SUCCESS)
						return rc.status;
					rmcclear();				// Clear RegExp replacement table.
					break;
				case sv_screenNum:
					(void) nextScreen(vsinkp,valp->u.v_int);
					break;
				case sv_search:
					strp1 = srch.pat;
					if(chkcpy(&strp1,valp->v_strp,NPAT + 1) != SUCCESS)
						return rc.status;
					newpat(-1);
					break;
				case sv_searchDelim:
					if(stoec(valp->v_strp,&c) != SUCCESS)
						return rc.status;
					srch.sdelim = c;
					break;
				case sv_showModes:
					(void) adjustmode(NULL,1,MDR_SHOW,valp);
					break;
				case sv_softTabSize:
					if(settab((int) valp->u.v_int,false) != SUCCESS)
						return rc.status;
					uphard();
					break;
				case sv_travJumpSize:
					if((tjump = valp->u.v_int) < 4)
						tjump = 4;
					else if(tjump > (int) term.t_ncol / 4 - 1)
						tjump = term.t_ncol / 4 - 1;
					break;
				case sv_vertJump:
					if((vjump = valp->u.v_int) < VJUMPMIN)
						vjump = 0;
					else if(vjump > JUMPMAX)
						vjump = JUMPMAX;
					break;
				case sv_windLineNum:
					(void) forwLine(vsinkp,valp->u.v_int - getwpos());
					break;
				case sv_windNum:
					(void) nextWind(vsinkp,valp->u.v_int);
					break;
				case sv_windSize:
					(void) resizeWind(vsinkp,valp->u.v_int);
					break;
				case sv_wordChars:
					(void) setwlist(valp);
					break;
				case sv_wrapCol:
					(void) feval(vsinkp,valp->u.v_int,cftab + cf_setWrapCol);
					break;
				case sv_wrapHook:
					c = HKWRAP;
					goto sethook;
				case sv_writeHook:
					c = HKWRITE;
sethook:
					(void) sethook(c,valp->v_strp);
					break;
				default:
					// Never should get here.
					return rcset(FATALERROR,0,text179,myname,svp->sv_id,svp->sv_name);
						// "%s(): Unknown id %d for variable '%s'!"
				}
			}
			break;

		// Set a macro argument.
		case VTYP_NVAR:
			if(vdp->vd_argnum == 0) {		// Allow numeric assignment (only) to $0.
				if(!intval(valp)) {
					strp1 = "0";
					goto badtyp1;
					}
				vsetint(valp->u.v_int,scriptrun->nargp);
				}
			else {
				MacArg *margp;

				// Macro argument assignment.  Find argument in list and set new value.
				margp = marg(vdp->u.vd_malp,vdp->vd_argnum);
				margp->ma_flags = 0;		// Clear "null token" flag.
				if(vcpy(margp->ma_valp,valp) != 0)
					return vrcset();
				}
			break;

		// Never should get here.
		default:
			return rcset(FATALERROR,0,text180,myname,(uint) vdp->vd_type);
				// "%s(): Unknown type %.8x for variable!"
		}
	return rc.status;
	}

// Create local or global user variable, given name and descriptor pointer.  Return status.
static int uvarnew(char *varp,VDesc *vdp) {
	UVar *uvp;
	char *strp;
	char *namep = varp + (*varp == TKC_GVAR ? 1 : 0);

	// Invalid length?
	if(*varp == '\0' || *namep == '\0' || strlen(namep) > NVSIZE)
		return rcset(FAILURE,0,text280,text279,NVSIZE);
			// "%s name cannot be null or exceed %d characters","Variable"

	// Valid variable name?
	strp = namep;
	if(getident(&strp) != s_ident || *strp != '\0')
		(void) rcset(FAILURE,0,text286,namep);
			// "Invalid identifier '%s'"

	// Same name as an existing command, function, alias, or macro?
	if(fabsearch(varp,NULL,PTRFAM))
		return rcset(FAILURE,0,text165,varp);
			// "Name '%s' already in use"

	// Allocate new record, set its values, and add to beginning of list.
	if((uvp = (UVar *) malloc(sizeof(UVar))) == NULL)
		return rcset(PANIC,0,text94,"uvarnew");
				// "%s(): Out of memory!"
	strcpy((vdp->u.vd_uvp = uvp)->uv_name,namep);
	if(*varp == TKC_GVAR) {
		vdp->vd_type = VTYP_GVAR;
		uvp->uv_flags = V_GLOBAL;
		uvp->uv_nextp = gvarsheadp;
		gvarsheadp = uvp;
		}
	else {
		vdp->vd_type = VTYP_LVAR;
		uvp->uv_flags = 0;
		uvp->uv_nextp = lvarsheadp;
		lvarsheadp = uvp;
		}

	// Set value of new variable to a null string.
	return vnew(&uvp->uv_vp,true);
	}

// Find a named variable's type and id.  If op is OPCREATE: (1), create user variable if non-existent and either (i), variable
// is global; or (ii), variable is local and executing a buffer; and (2), return status.  If op is OPQUERY: return true if
// variable is found; otherwise, false.  If op is OPDELETE: return status if variable is found; otherwise, error.  In all cases,
// store results in vdp if not NULL and variable is found.
int findvar(char *namep,int op,VDesc *vdp) {
	UVar *uvp;
	int i;
	VDesc vd;

	// Get ready.
	vd.u.vd_uvp = NULL;
	vd.vd_type = VTYP_UNK;
	vd.vd_argnum = 0;

	// Check lead-in character.
	if(*namep == TKC_GVAR) {
		if(strlen(namep) > 1) {

			// Macro argument reference?
			if(isdigit(namep[1])) {
				long lval;

				// Yes, macro running and number in range?
				if(scriptrun != NULL && asc_long(namep + 1,&lval,true) && lval <= scriptrun->malp->mal_count) {

					// Valid reference.  Set type and save argument number.
					vd.vd_type = VTYP_NVAR;
					vd.vd_argnum = lval;
					vd.u.vd_malp = scriptrun->malp;
					goto found;
					}
				}
			else {
				// Check for existing global variable.
				if((uvp = uvarfind(namep)) != NULL)
					goto uvfound;

				// Check for existing system variable.
				if((i = binary(namep + 1,svarname,NSVARS)) >= 0) {
					vd.vd_type = VTYP_SVAR;
					vd.u.vd_svp = sysvars + i;
					goto found;
					}

				// Not found.  Create new one?
				if(op == OPCREATE)
					goto create;
				goto notfound;
				}
			}
		}
	else if(*namep != '\0') {

		// Check for existing local variable.
		if((uvp = uvarfind(namep)) != NULL) {
uvfound:
			vd.vd_type = (uvp->uv_flags & V_GLOBAL) ? VTYP_GVAR : VTYP_LVAR;
			vd.u.vd_uvp = uvp;
			goto found;
			}

		// Not found.  Create a new one (if executing a buffer)?
		if(op != OPCREATE || scriptrun == NULL)
			goto notfound;
create:
		if(uvarnew(namep,&vd) != SUCCESS)
			return rc.status;
found:
		if(vdp != NULL)
			*vdp = vd;
		return (op == OPQUERY) ? true : rc.status;
		}
notfound:
	// Variable not found.
	return (op == OPQUERY) ? false : rcset(FAILURE,0,text52,namep);
					// "No such variable '%s'"
	}

// Find macro argument record and return it, given list pointer and argument number.
MacArg *marg(MacArgList *malp,ushort argnum) {
	MacArg *margp = malp->mal_headp;

	while(argnum-- > 1)
		margp = margp->ma_nextp;
	return margp;
	}

// Derefence a variable, given descriptor, and save variable's value in valp.  Return status.
int derefv(Value *valp,VDesc *vdp) {
	Value *vp;

	switch(vdp->vd_type) {
		case VTYP_LVAR:
		case VTYP_GVAR:
			vp = vdp->u.vd_uvp->uv_vp;
			break;
		case VTYP_SVAR:
			return getsvar(valp,vdp->u.vd_svp);
		default:	// VTYP_NVAR.
			{MacArgList *malp = vdp->u.vd_malp;
			ushort argnum = vdp->vd_argnum;

			// Get argument value.  $0 resolves to the macro "n" argument.
			vp = (argnum == 0) ? scriptrun->nargp : marg(malp,argnum)->ma_valp;
			}
		}

	// Copy value.
	if(vcpy(valp,vp) != 0)
		(void) vrcset();

	return rc.status;
	}

// Derefence a variable, given name, and save variable's value in valp.  Return status.
int derefn(Value *valp,char *namep) {
	VDesc vd;

	// Find and dereference variable.
	if(findvar(namep,OPDELETE,&vd) == SUCCESS)
		(void) derefv(valp,&vd);

	return rc.status;
	}

// Set a variable -- "let" command (interactively only).  Evaluate value as an expression if n arg.  Return status.
int setvar(Value *rp,int n) {
	VDesc vd;			// Variable descriptor.
	char *promptp;
	int delim;
	uint aflags;
	long lval;
	Value *vp;

	// First get the variable to set.
	if(vnew(&vp,false) != 0)
		return vrcset();
	if(termarg(vp,text51,NULL,CTRL | 'M',0) != SUCCESS || vistfn(vp,VNIL))
			// "Assign variable"
		return rc.status;

	// Find variable ...
	if(findvar(vp->v_strp,OPCREATE,&vd) != SUCCESS)
		return rc.status;

	// get the value ...
	if(n == INT_MIN) {
		delim = CTRL | ((vd.vd_type == VTYP_SVAR && (vd.u.vd_svp->sv_flags & V_ESCDELIM)) ? '[' : 'M');
		promptp = text53;
			// "Value"
		aflags = 0;
		}
	else {
		delim = CTRL | 'M';
		promptp = text301;
			// "Expression"
		aflags = ARG_EVAL;
		}
	if(termarg(rp,promptp,NULL,delim,aflags) != SUCCESS)
		return rc.status;

	// and set it.
	if(n == INT_MIN && (vd.vd_type == VTYP_GVAR || (vd.vd_type == VTYP_SVAR && (vd.u.vd_svp->sv_flags & V_INT))) &&
	 asc_long(rp->v_strp,&lval,true))
		vsetint(lval,rp);
#if MMDEBUG & DEBUG_VALUE
vdump(rp,"setvar(): Setting and returning value ...");
	(void) putvar(rp,&vd);
	dumpvars();
	return rc.status;
#else
	return putvar(rp,&vd);
#endif
	}

// Increment or decrement a variable, given name in np, "incr" flag, and "pre" flag.  Set np to result and return status.
int bumpvar(ENode *np,bool incr,bool pre) {
	VDesc vd;
	long lval;
	Value *vp;

	if(findvar(np->en_rp->v_strp,OPDELETE,&vd) != SUCCESS)	// Find variable ...
		return rc.status;
	if(!intvar(&vd))					// and make sure it's an integer.
		return rcset(FAILURE,0,text212,np->en_rp->v_strp);
				// "Variable '%s' not an integer"
	if(vnew(&vp,false) != 0)
		return vrcset();
	if(derefv(vp,&vd) != SUCCESS)				// Dereference variable ...
		return rc.status;
	lval = vp->u.v_int + (incr ? 1L : -1L);			// compute new value of variable ...
	vsetint(pre ? lval : vp->u.v_int,np->en_rp);		// set result to pre or post value ...
	vsetint(lval,vp);					// set new variable value in a value object ...
	return putvar(vp,&vd);					// and update variable.
	}

#if MMDEBUG & DEBUG_VALUE
// Dump all user variables to the log file.
void dumpvars(void) {
	UVar *uvp;
	struct {
		char *labelp;
		UVar **uheadpp;
		} uvtab[] = {
			{"GLOBAL",&gvarsheadp},
			{"LOCAL",&lvarsheadp},
			{NULL,NULL}},
		*uvtp = uvtab;

	do {
		fprintf(logfile,"%s VARS\n",uvtp->labelp);
		if((uvp = *uvtp->uheadpp) != NULL) {
			do {
				if(uvp->uv_flags & V_GLOBAL)
					fputc(TKC_GVAR,logfile);
				vdump(uvp->uv_vp,uvp->uv_name);
				} while((uvp = uvp->uv_nextp) != NULL);
			}
		} while((++uvtp)->labelp != NULL);
	}
#endif

// List all the system constants, system variables, and user variables and their values (interactive only).  If default n, make
// full list; otherwise, get a match string and make partial list of variable names that contain it, ignoring case.  Render
// buffer and return status.
int showVariables(Value *rp,int n) {
	Buffer *vlistp;
	SVar *svp;
	bool needBreak,doapropos,skipLine;
	char *strp;
	StrList rpt;
	Value *mstrp;			// Match string.
	Value *valp;
	char wkbuf[52];
	WindFace *wfp = &curwp->w_face;
	UVar *uvp,***uvppp,**uv[] = {&gvarsheadp,&lvarsheadp,NULL};

	// If not default n, get match string.
	if((doapropos = n != INT_MIN)) {
		if(vnew(&mstrp,false) != 0)
			return vrcset();
		if(apropos(mstrp,text292) != SUCCESS)
				// "variable"
			return rc.status;
		}

	// Get a buffer for the variable list and open a string list.
	if(sysbuf(text56,&vlistp) != SUCCESS)
			// "VariableList"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	needBreak = skipLine = false;

	// Build the system variable list.
	if(vnew(&valp,false) != 0)
		return vrcset();
	svp = sysvars;
	do {
		// Begin with the system variable name.
		wkbuf[0] = '$';
		strcpy(wkbuf + 1,svp->sv_name);

		// Skip if an apropos and system variable name doesn't contain the search string.
		if(doapropos && strcasestr(wkbuf,mstrp->v_strp) == NULL)
			continue;
		if(!skipLine && is_lower(*svp->sv_name)) {
			if(needBreak && vputc('\r',&rpt) != 0)
				return vrcset();
			skipLine = true;
			}
		if(needBreak && vputc('\r',&rpt) != 0)
			return vrcset();
		pad(wkbuf,19);
		if(vputs(wkbuf,&rpt) != 0)
			return vrcset();
		needBreak = true;

		// Add in the description.
		pad(strcpy(wkbuf,svp->sv_desc),50);
		if(vputs(wkbuf,&rpt) != 0)
			return vrcset();

		// Add in the value.  Skip $RegionText if no region defined.
		if(svp->sv_id != sv_RegionText || (wfp->wf_mark[0].mk_dot.lnp != NULL &&
		 (wfp->wf_mark[0].mk_dot.lnp != wfp->wf_dot.lnp || wfp->wf_mark[0].mk_dot.off != wfp->wf_dot.off))) {
			if(getsvar(valp,svp) != SUCCESS || tostr(valp) != SUCCESS ||
			 vstrlit(&rpt,valp->v_strp,0) != SUCCESS)
				return rc.status;
			}
		} while((++svp)->sv_name != NULL);

	// Build the user (global and local) variable list.
	uvppp = uv;
	do {
		if((uvp = **uvppp) != NULL) {
			if(needBreak && vputc('\r',&rpt) != 0)
				return vrcset();
			do {
				// Add in the user variable name.
				if(uvp->uv_flags & V_GLOBAL) {
					wkbuf[0] = TKC_GVAR;
					strp = wkbuf + 1;
					}
				else
					strp = wkbuf;
				strcpy(strp,uvp->uv_name);

				// Skip if an apropos and user variable name doesn't contain the search string.
				if(doapropos && strcasestr(wkbuf,mstrp->v_strp) == NULL)
					continue;
				if(needBreak && vputc('\r',&rpt) != 0)
					return vrcset();
				pad(wkbuf,19);
				if(vputs(wkbuf,&rpt) != 0)
					return vrcset();
				needBreak = true;

				// Add in the value.
				if(uvp->uv_vp->v_type == VALINT) {
					if(vputf(&rpt,"%ld",uvp->uv_vp->u.v_int) != 0)
						return vrcset();
					}
				else if(vstrlit(&rpt,uvp->uv_vp->v_strp,0) != 0)
					return vrcset();
				} while((uvp = uvp->uv_nextp) != NULL);
			}
		} while(*++uvppp != NULL);

	// Add the results to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(!visnull(rpt.sl_vp) && bappend(vlistp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display the list.
	return render(rp,n < 0 ? -2 : n,vlistp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}
