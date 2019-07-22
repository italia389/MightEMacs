// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// var.c	Routines dealing with variables for MightEMacs.

#include "os.h"
#include <ctype.h>
#include "pllib.h"

// Make selected global definitions local.
#define DATAvar
#include "std.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "file.h"
#include "search.h"
#include "var.h"

// Return true if a variable is an integer type, given descriptor; otherwise, false.
bool intvar(VDesc *vdp) {
	Datum *datum;

	switch(vdp->vd_type) {
		case VTyp_LVar:
		case VTyp_GVar:
			datum = vdp->p.vd_uvp->uv_datum;
			break;
		case VTyp_SVar:
			return (vdp->p.vd_svp->sv_flags & (V_Int | V_Char)) != 0;
		case VTyp_NVar:
			{ushort argnum = vdp->i.vd_argnum;

			// Get argument value.  $0 resolves to the macro "n" argument.
			datum = (argnum == 0) ? scriptrun->narg : awptr(vdp->p.vd_marg)->aw_ary->a_elp[argnum - 1];
			}
			break;
		default:	// VTyp_ARef
			datum = aget(vdp->p.vd_ary,vdp->i.vd_index,false);	// Should never return NULL.

		}
	return datum->d_type == dat_int;
	}

// Return true if c is a valid first character of an identifier; otherwise, false.
bool isident1(short c) {

	return isletter(c) || c == '_';
	}

// Return number of variables currently in use (for building a name completion list).
uint varct(uint cflags) {
	UVar *uvar;
	uint count;

	// Get system variable name count.
	if(cflags & Term_C_SVar) {		// Skip constants.
		SVar *svp = sysvars;
		count = 0;
		do {
			if(is_lower(svp->sv_name[1]))
				++count;
			} while((++svp)->sv_name != NULL);
		}
	else
		count = NSVars;

	// Add global variable counts.
	for(uvar = gvarshead; uvar != NULL; uvar = uvar->uv_next)
		if(!(cflags & Term_C_SVar) || is_lower(uvar->uv_name[1]))
			++count;

	return count;
	}

// Compare two variable names (for qsort()).
static int varcmp(const void *vp1,const void *vp2) {

	return strcmp(*((char **) vp1),*((char **) vp2));
	}

// Created sorted list of all variables currently in use and store in vlistv array.
void varlist(char *vlistv[],uint count,uint cflags) {
	SVar *svp;
	UVar *uvar;
	char **vlistv0 = vlistv;

	// Store system variable names.
	svp = sysvars;
	do {
		if(!(cflags & Term_C_SVar) || is_lower(svp->sv_name[1]))
			*vlistv++ = svp->sv_name;
		} while((++svp)->sv_name != NULL);

	// Store global variable names.
	for(uvar = gvarshead; uvar != NULL; uvar = uvar->uv_next)
		if(!(cflags & Term_C_SVar) || is_lower(uvar->uv_name[1]))
			*vlistv++ = uvar->uv_name;

	// Sort it.
	qsort((void *) vlistv0,count,sizeof(char *),varcmp);
	}

// Free user variable(s), given "stack" pointer.  All variables will be together at top of list (because they are created in
// stack fashion during macro execution and/or recursion).
int uvarclean(UVar *vstack) {
	UVar *uvar;

	while(lvarshead != vstack) {
		uvar = lvarshead->uv_next;

		// Free value...
#if DCaller
		ddelete(lvarshead->uv_datum,"uvarclean");
#else
		ddelete(lvarshead->uv_datum);
#endif

		// free variable...
		free((void *) lvarshead);

		// and advance head pointer.
		lvarshead = uvar;
		}
	return rc.status;
	}

// Search global or local variable list for given name (with prefix).  If found, return pointer to UVar record; otherwise,
// return NULL.  Local variable entries beyond scriptrun->uvar are not considered so that all local variables created or
// referenced in a particular macro invocation are visible and accessible only from that macro, which allows recursion to work
// properly.
UVar *uvarfind(char *var) {
	UVar *uvar;
	UVar *vstack;		// Search boundary in local variable table.

	if(*var == TokC_GVar) {
		uvar = gvarshead;
		vstack = NULL;
		}
	else {
		uvar = lvarshead;
		vstack = (scriptrun == NULL) ? NULL : scriptrun->uvar;
		}

	while(uvar != vstack) {
		if(strcmp(var,uvar->uv_name) == 0)
			return uvar;
		uvar = uvar->uv_next;
		}

	return NULL;
	}

// binsearch() helper function for returning system variable name, given table (array) and index.
static char *svarname(void *table,ssize_t i) {

	return ((SVar *) table)[i].sv_name + 1;
	}

// Place the list of characters considered "in a word" into rval.  Return status.
static int getwlist(Datum *rval) {
	DStrFab sfab;
	char *str,*strz;

	if(dopenwith(&sfab,rval,SFClear) != 0)
		goto LibFail;

	// Build the string of characters in the result buffer.
	strz = (str = wordlist) + 256;
	do {
		if(*str && dputc(str - wordlist,&sfab) != 0)
			goto LibFail;
		} while(++str < strz);

	if(dclose(&sfab,sf_string) != 0)
LibFail:
		(void) librcset(Failure);

	return rc.status;
	}

// Replace the current line with the given text.  Return status.  (Used only for setting the $lineText system variable.)
static int putctext(char *iline) {

	if(allowedit(true) != Success)			// Don't allow if read-only buffer.
		return rc.status;

	// Delete any text on the current line.
	if(si.curwin->w_face.wf_point.lnp->l_used > 0) {
		si.curwin->w_face.wf_point.off = 0;	// Start at the beginning of the line.
		if(kdctext(1,0,NULL) != Success)	// Put it in the undelete buffer.
			return rc.status;
		}

	// Insert the new text.
	return linstr(iline);
	}

// Encode the current keyboard macro into dest in string form using ektos().  Return status.
static int kmtos(Datum *dest) {

	// Recording a keyboard macro?
	if(kmacro.km_state == KMRecord) {
		clearKeyMacro(true);
		return rcset(Failure,RCNoFormat,text338);
			// "Cannot access '$keyMacro' from a keyboard macro, cancelled"
		}

	// Null keyboard macro?
	if(kmacro.km_slot == kmacro.km_buf)
		dsetnull(dest);
	else {
		ushort *kmp;
		char *str;
		DStrFab sfab;
		char wkbuf[16];

		// Find a delimter that can be used (a character that is not in the macro).  Default to tab.
		str = KMDelims;
		wkbuf[0] = '\t';
		do {
			kmp = kmacro.km_buf;
			do {
				if(*kmp == *str)
					goto Next;
				} while(++kmp < kmacro.km_end);

			// Found.
			wkbuf[0] = *str;
			break;
Next:;
			} while(*++str != '\0');

		// Loop through keyboard macro keys and translate each into sfab with delimiter found in previous step.
		if(dopenwith(&sfab,dest,SFClear) != 0)
			goto LibFail;
		kmp = kmacro.km_buf;
		do {
			ektos(*kmp,wkbuf + 1,false);
			if(dputs(wkbuf,&sfab) != 0)
				goto LibFail;
			} while(++kmp < kmacro.km_end);
		if(dclose(&sfab,sf_string) != 0)
LibFail:
			return librcset(Failure);
		}

	return rc.status;
	}

// Get value of a system variable, given result pointer and table pointer.
int getsvar(Datum *rval,SVar *svp) {
	enum svarid vid;
	char *str;
	Ring *ring;
	char wkbuf[32];

	// Fetch the corresponding value.
	switch(vid = svp->sv_id) {
		case sv_ARGV:
			if(scriptrun == NULL)
				dsetnil(rval);
			else if(datcpy(rval,scriptrun->marg) != 0)
				goto LibFail;
			break;
		case sv_BufInpDelim:
			str = si.curbuf->b_inpdelim;
			goto Kopy;
		case sv_BufModes:
			(void) getmodes(rval,si.curbuf);
			break;
		case sv_Date:
			str = timeset();
			goto Kopy;
		case sv_GlobalModes:
			(void) getmodes(rval,NULL);
			break;
		case sv_HorzScrollCol:
			dsetint(modeset(MdIdxHScrl,NULL) ? si.curwin->w_face.wf_firstcol : si.curscr->s_firstcol,rval);
			break;
		case sv_LastKey:
			dsetint((long)(kentry.lastkseq & (Prefix | Shft | FKey | 0x80) ? -1 :
			 kentry.lastkseq & Ctrl ? ektoc(kentry.lastkseq,false) : kentry.lastkseq),rval);
			break;
		case sv_LineLen:
			dsetint((long) si.curwin->w_face.wf_point.lnp->l_used,rval);
			break;
		case sv_Match:
			str = fixnull(lastMatch->d_str);
			goto Kopy;
		case sv_RegionText:
			{Region region;

			// Get the region limits.
			if(getregion(&region,RegForceBegin) != Success)
				return rc.status;

			// Preallocate a string and copy.
			if(dsalloc(rval,region.r_size + 1) != 0)
				goto LibFail;
			regcpy(rval->d_str,&region);
			}
			break;
		case sv_ReturnMsg:
			str = scriptrc.msg.d_str;
			goto Kopy;
		case sv_RunFile:
			str = fixnull((scriptrun == NULL) ? NULL : scriptrun->path);
			goto Kopy;
		case sv_RunName:
			{Buffer *buf = (scriptrun == NULL) ? NULL : scriptrun->buf;
			str = fixnull(buf == NULL ? NULL : *buf->b_bname == SBMacro ? buf->b_bname + 1 :
			 buf->b_bname);
			}
			goto Kopy;
		case sv_ScreenCount:
			dsetint((long) scrcount(),rval);
			break;
		case sv_TermSize:
			{Array *ary;

			if((ary = anew(2,NULL)) == NULL)
				goto LibFail;
			if(awrap(rval,ary) == Success) {
				dsetint((long) term.t_ncol,ary->a_elp[0]);
				dsetint((long) term.t_nrow,ary->a_elp[1]);
				}
			}
			break;
		case sv_WindCount:
			dsetint((long) wincount(si.curscr,NULL),rval);
			break;
		case sv_autoSave:
			dsetint((long) si.gasave,rval);
			break;
		case sv_bufFile:
			if((str = si.curbuf->b_fname) != NULL)
				goto Kopy;
			dsetnil(rval);
			break;
		case sv_bufLineNum:
			dsetint(getlinenum(si.curbuf,si.curwin->w_face.wf_point.lnp),rval);
			break;
		case sv_bufname:
			str = si.curbuf->b_bname;
			goto Kopy;
		case sv_delRingSize:
			ring = &dring;
			goto GetRingSize;
		case sv_execPath:
			str = execpath;
			goto Kopy;
		case sv_fencePause:
			dsetint((long) si.fencepause,rval);
			break;
		case sv_hardTabSize:
			dsetint((long) si.htabsize,rval);
			break;
		case sv_horzJump:
			dsetint((long) vtc.hjump,rval);
			break;
		case sv_inpDelim:
			str = fi.inpdelim;
			goto Kopy;
		case sv_keyMacro:
			(void) kmtos(rval);
			break;
		case sv_killRingSize:
			ring = &kring;
			goto GetRingSize;
		case sv_lastKeySeq:
			ektos(kentry.lastkseq,str = wkbuf,false);
			goto Kopy;
		case sv_lineChar:
			{Point *point = &si.curwin->w_face.wf_point;
			dsetint(bufend(point) ? '\0' : point->off == point->lnp->l_used ? '\n' :
			 point->lnp->l_text[point->off],rval);
			}
			break;
		case sv_lineCol:
			dsetint((long) getcol(NULL,false),rval);
			break;
		case sv_lineOffset:
			dsetint((long) si.curwin->w_face.wf_point.off,rval);
			break;
		case sv_lineText:
			{Line *lnp = si.curwin->w_face.wf_point.lnp;
			if(dsetsubstr(lnp->l_text,lnp->l_used,rval) != 0)
				goto LibFail;
			}
			break;
		case sv_maxArrayDepth:
			dsetint((long) maxarydepth,rval);
			break;
		case sv_maxLoop:
			dsetint((long) maxloop,rval);
			break;
		case sv_maxMacroDepth:
			dsetint((long) maxmacdepth,rval);
			break;
		case sv_maxPromptPct:
			dsetint((long) term.maxprmt,rval);
			break;
		case sv_otpDelim:
			str = fi.otpdelim;
			goto Kopy;
		case sv_pageOverlap:
			dsetint((long) si.overlap,rval);
			break;
		case sv_randNumSeed:
			dsetint(si.randseed & LONG_MAX,rval);
			break;
		case sv_replacePat:
			str = srch.m.rpat;
			goto Kopy;
		case sv_replaceRingSize:
			ring = &rring;
			goto GetRingSize;
		case sv_screenNum:
			dsetint((long) si.curscr->s_num,rval);
			break;
		case sv_searchDelim:
			ektos(srch.sdelim,str = wkbuf,false);
			goto Kopy;
		case sv_searchPat:
			{char patbuf[srch.m.patlen + OptCh_N + 1];

			if(dsetstr(mkpat(patbuf,&srch.m),rval) != 0)
				goto LibFail;
			}
			break;
		case sv_searchRingSize:
			ring = &sring;
GetRingSize:
			dsetint((long) ring->r_maxsize,rval);
			break;
		case sv_softTabSize:
			dsetint((long) si.stabsize,rval);
			break;
		case sv_travJump:
			dsetint((long) si.tjump,rval);
			break;
		case sv_vertJump:
			dsetint((long) vtc.vjump,rval);
			break;
		case sv_windLineNum:
			dsetint((long) getwpos(si.curwin),rval);
			break;
		case sv_windNum:
			dsetint((long) getwnum(si.curwin),rval);
			break;
		case sv_windSize:
			dsetint((long) si.curwin->w_nrows,rval);
			break;
		case sv_wordChars:
			(void) getwlist(rval);
			break;
		case sv_workDir:
			str = si.curscr->s_wkdir;
			goto Kopy;
		case sv_wrapCol:
			dsetint((long) si.wrapcol,rval);
			break;
		default:
			// Never should get here.
			return rcset(FatalError,0,text3,"getsvar",vid,svp->sv_name);
					// "%s(): Unknown ID %d for variable '%s'!"
		}
	goto Retn;
Kopy:
	if(dsetstr(str,rval) != 0)
LibFail:
		(void) librcset(Failure);
Retn:
	return rc.status;
	}

// Set a list of characters to be considered in a word.  Return status.
int setwlist(char *wclist) {
	char *str,*strz;
	DStrFab sfab;

	// First, expand the new value (and close the string-fab object)...
	if(strexpand(&sfab,wclist) != Success)
		return rc.status;

	// clear the word list table...
	strz = (str = wordlist) + 256;
	do {
		*str++ = false;
		} while(str < strz);

	// and for each character in the new value, set that element in the table.
	str = sfab.sf_datum->d_str;
	while(*str != '\0')
		wordlist[(int) *str++] = true;

	mcclear(&srch.m);		// Clear Regexp search arrays in case they contain \w or \W.
	return rc.status;
	}

// Decode and save a keyboard macro from a delimited string containing encoded keys.  The first character of the string is the
// delimiter.  Error if not in the KMStopped state.  Return status.
static int stokm(char *estr) {
	short delim;

	// Make sure a keyboard macro is not currently being recorded or played.
	clearKeyMacro(false);
	if(kmacro.km_state != KMStop) {
		if(kmacro.km_state == KMRecord)
			si.curwin->w_flags |= WFMode;
		kmacro.km_state = KMStop;
		return rcset(Failure,RCNoFormat,text338);
			// "Cannot access '$keyMacro' from a keyboard macro, cancelled"
		}

	// Get delimiter (first character) and parse string.
	if((delim = *estr++) != '\0' && *estr != '\0') {
		ushort c,ek;
		enum cfid id;
		UnivPtr univ;
		bool last;
		Datum *datum;

		if(dnewtrk(&datum) != 0)
			return librcset(Failure);

		// Parse tokens and save in keyboard macro array.
		while(parsetok(datum,&estr,delim) != NotFound) {

			// Convert token string to a key sequence.
			if(*datum->d_str == '\0')
				return rcset(Failure,0,text254,"");
					// "Invalid key literal '%s'"
			if(stoek(datum->d_str,&ek) != Success)
				break;

			// Loop once or twice, saving high and low values.
			last = false;
			do {
				// Have a prefix key?
				switch(ek & Prefix) {
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
						c = ek;
						last = true;
						goto SaveIt;
					}

				// Get the key binding.
				univ.u.p_cfp = cftab + id;
				c = getpentry(&univ)->k_code;
				ek &= ~Prefix;
SaveIt:
				// Save key if room.
				if(kmacro.km_slot == kmacro.km_buf + kmacro.km_size && growKeyMacro() != Success)
					return rc.status;
				*kmacro.km_slot++ = c;
				} while(!last);
			}
		kmacro.km_end = kmacro.km_slot;
		}

	return rc.status;
	}

// Copy a new value to a variable, checking if old value is an array in a global variable.
static int newval(Datum *dest,Datum *src,VDesc *vdp) {

	if(dest->d_type == dat_blobRef && vdp->vd_type == VTyp_GVar)
		agarbpush(dest);
	return datcpy(dest,src) != 0 ? librcset(Failure) : rc.status;
	}

// Calculate horizontal jump columns from percentage.
void calcHJC(void) {

	if((vtc.hjumpcols = vtc.hjump * term.t_ncol / 100) == 0)
		vtc.hjumpcols = 1;
	}

// Set a variable to given value.  Return status.
int putvar(Datum *datum,VDesc *vdp) {
	char *str1;
	static char myname[] = "putvar";

	// Set the appropriate value.
	switch(vdp->vd_type) {

		// Set a user variable.
		case VTyp_LVar:
		case VTyp_GVar:
			{UVar *uvar = vdp->p.vd_uvp;				// Grab pointer to old value.
			(void) newval(uvar->uv_datum,datum,vdp);
			}
			break;

		// Set a system variable.
		case VTyp_SVar:
			{int i;
			ushort ek;
			Ring *ring;
			SVar *svp = vdp->p.vd_svp;
			Datum *dsink;						// For throw-away return value, if any.

			// Can't modify a read-only variable.
			if(svp->sv_flags & V_RdOnly)
				return rcset(Failure,RCTermAttr,text164,svp->sv_name);
						// "Cannot modify read-only variable '~b%s~B'"

			// Not read-only.  Check for legal value types.  Array and string variables may also be nil (if V_Nil
			// flag set), but all other variables are single-type only.
			if(svp->sv_flags & V_Int) {
				if(!intval(datum))
					goto BadTyp1;
				}
			else if(svp->sv_flags & V_Char) {
				if(!charval(datum))
					goto BadTyp1;
				}
			else if(datum->d_type & DBoolMask) {
				str1 = text360;
					// "Boolean"
				goto BadTyp0;
				}
			else if(datum->d_type == dat_nil) {
				if(svp->sv_flags & V_Nil)
					dsetnull(datum);
				else {
					str1 = text359;
						// "nil"
BadTyp0:
					(void) rcset(Failure,0,text358,str1);
							// "Illegal use of %s value"
					goto BadTyp1;
					}
				}
			else if(svp->sv_flags & V_Array) {
				if(!aryval(datum))
					goto BadTyp1;
				}
			else if(!strval(datum)) {
				DStrFab msg;
BadTyp1:
				str1 = svp->sv_name;
BadTyp2:
				if(dopentrk(&msg) != 0)
					goto LibFail;
				else if(escattr(&msg) == Success) {
					if(dputf(&msg,text334,str1) != 0 || dclose(&msg,sf_string) != 0)
							// ", setting variable '~b%s~B'"
						goto LibFail;
					(void) rcset(rc.status,RCForce | RCNoFormat | RCTermAttr,msg.sf_datum->d_str);
					}
				return rc.status;
				}

			// Do specific action for referenced (mutable) variable.
			if(dnewtrk(&dsink) != 0)
				goto LibFail;
			switch(svp->sv_id) {
				case sv_autoSave:
					{int n;

					if(datum->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					if((n = (datum->u.d_int > INT_MAX ? INT_MAX : datum->u.d_int)) == 0) {

						// ASave count set to zero... turn off global mode and clear counter.
						gmclear(mi.cache[MdIdxASave]);
						si.gasave = si.gacount = 0;
						}
					else {
						int diff = n - si.gasave;
						if(diff != 0) {

							// New count > 0.  Adjust counter accordingly.
							si.gasave = n;
							if(diff > 0) {
								if((long) si.gacount + diff > INT_MAX)
									si.gacount = INT_MAX;
								else
									si.gacount += diff;
								}
							else if((si.gacount += diff) <= 0)
								si.gacount = 1;
							}
						}
					}
					break;
				case sv_bufFile:
					str1 = "0 => setBufFile $bufname,";
					goto XeqCmd;
				case sv_bufLineNum:
					(void) goline(dsink,INT_MIN,datum->u.d_int);
					break;
				case sv_bufname:
					str1 = "renameBuf $bufname,";
					goto XeqCmd;
				case sv_delRingSize:
					ring = &dring;
					goto SetRingSize;
				case sv_execPath:
					(void) setpath(datum->d_str,false);
					break;
				case sv_fencePause:
					if(datum->u.d_int < 0)
						return rcset(Failure,0,text39,text119,(int) datum->u.d_int,0);
							// "%s (%d) must be %d or greater","Pause duration"
					si.fencepause = datum->u.d_int;
					break;
				case sv_hardTabSize:
					if(settab((int) datum->u.d_int,true) == Success)
						(void) supd_wflags(NULL,WFHard | WFMode);
					break;
				case sv_horzJump:
					if((vtc.hjump = datum->u.d_int) < 0)
						vtc.hjump = 0;
					else if(vtc.hjump > JumpMax)
						vtc.hjump = JumpMax;
					calcHJC();
					break;
				case sv_inpDelim:
					if(strlen(datum->d_str) > sizeof(fi.inpdelim) - 1)
						return rcset(Failure,0,text251,text46,datum->d_str,sizeof(fi.inpdelim) - 1);
							// "%s delimiter '%s' cannot be more than %d character(s)","Input"
					strcpy(fi.inpdelim,datum->d_str);
					break;
				case sv_keyMacro:
					(void) stokm(datum->d_str);
					break;
				case sv_killRingSize:
					ring = &kring;
					goto SetRingSize;
				case sv_lastKeySeq:
					// Don't allow variable to be set to a prefix key or numeric prefix (pseudo command).
					if(stoek(datum->d_str,&ek) == Success) {
						KeyBind *kbind = getbind(ek);
						if(kbind != NULL && kbind->k_targ.p_type == PtrPseudo)
							return rcset(Failure,RCTermAttr,text373,svp->sv_name);
									// "Illegal value for '~b%s~B' variable"
						kentry.lastkseq = ek;
						kentry.uselast = true;
						}
					break;
				case sv_lineChar:
					// Replace character at point with an integer ASCII value.
					if(charval(datum)) {
						if(ldelete(1L,0) != Success)
							return rcset(Failure,0,text142,si.curbuf->b_bname);
								// "Cannot change a character past end of buffer '%s'"
						(void) (datum->u.d_int == 012 ? lnewline() : linsert(1,datum->u.d_int));
						}
					break;
				case sv_lineCol:
					(void) setccol(datum->u.d_int);
					break;
				case sv_lineOffset:
					{int llen = si.curwin->w_face.wf_point.lnp->l_used;
					int loff = (datum->u.d_int < 0) ? llen + datum->u.d_int : datum->u.d_int;
					if(loff < 0 || loff > llen)
						return rcset(Failure,0,text378,datum->u.d_int);
							// "Line offset value %ld out of range"
					si.curwin->w_face.wf_point.off = loff;
					}
					break;
				case sv_lineText:
					(void) putctext(datum->d_str);
					break;
				case sv_maxArrayDepth:
					if(datum->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					maxarydepth = datum->u.d_int;
					break;
				case sv_maxLoop:
					if(datum->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					maxloop = datum->u.d_int;
					break;
				case sv_maxMacroDepth:
					if(datum->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					maxmacdepth = datum->u.d_int;
					break;
				case sv_maxPromptPct:
					if(datum->u.d_int < 15 || datum->u.d_int > 90)
						return rcset(Failure,RCTermAttr,text379,svp->sv_name,15,90);
							// "'~b%s~B' value must be between %d and %d"
					term.maxprmt = datum->u.d_int;
					break;
				case sv_otpDelim:
					if((size_t) (i = strlen(datum->d_str)) > sizeof(fi.otpdelim) - 1)
						return rcset(Failure,0,text251,text47,datum->d_str,
							// "%s delimiter '%s' cannot be more than %d character(s)","Output"
						 (int) sizeof(fi.otpdelim) - 1);
					strcpy(fi.otpdelim,datum->d_str);
					fi.otpdelimlen = i;
					break;
				case sv_pageOverlap:
					if(datum->u.d_int < 0 || datum->u.d_int > (int) (term.t_nrow - 1) / 2)
						return rcset(Failure,0,text184,datum->u.d_int,(int) (term.t_nrow - 1) / 2);
							// "Overlap %ld must be between 0 and %d"
					si.overlap = datum->u.d_int;
					break;
				case sv_randNumSeed:

					// Generate new seed if zero.
					if((si.randseed = (ulong) datum->u.d_int) == 0)
						si.randseed = seedinit();
					break;
				case sv_replacePat:
					(void) newrpat(datum->d_str,&srch.m);
					break;
				case sv_replaceRingSize:
					ring = &rring;
					goto SetRingSize;
				case sv_screenNum:
					(void) gotoScreen(datum->u.d_int,0);
					break;
				case sv_searchDelim:
					if(stoek(datum->d_str,&ek) != Success)
						return rc.status;
					if(ek & Prefix) {
						char keybuf[16];

						return rcset(Failure,RCTermAttr,text341,ektos(ek,keybuf,true),text343);
							// "Cannot use key sequence ~#u%s~U as %s delimiter","search"
						}
					srch.sdelim = ek;
					break;
				case sv_searchPat:
					// Make copy so original is returned unmodified.
					{char wkpat[strlen(datum->d_str) + 1];
					(void) newspat(strcpy(wkpat,datum->d_str),&srch.m,NULL);
					}
					break;
				case sv_searchRingSize:
					ring = &sring;
SetRingSize:
					if(datum->u.d_int < 0) {
						i = 0;
ERange:
						return rcset(Failure,RCTermAttr,text111,svp->sv_name,i);
							// "'~b%s~B' value must be %d or greater"
						}
					if(datum->u.d_int < ring->r_size)
						return rcset(Failure,0,text241,datum->u.d_int,ring->r_size);
							// "Maximum ring size (%ld) less than current size (%d)"
					ring->r_maxsize = datum->u.d_int;
					break;
				case sv_softTabSize:
					if(settab((int) datum->u.d_int,false) == Success)
						(void) supd_wflags(NULL,WFHard | WFMode);
					break;
				case sv_travJump:
					if((si.tjump = datum->u.d_int) < 4)
						si.tjump = 4;
					else if(si.tjump > term.t_ncol / 4 - 1)
						si.tjump = term.t_ncol / 4 - 1;
					break;
				case sv_vertJump:
					if((vtc.vjump = datum->u.d_int) < 0)
						vtc.vjump = 0;
					else if(vtc.vjump > JumpMax)
						vtc.vjump = JumpMax;
					break;
				case sv_windLineNum:
					(void) forwLine(dsink,datum->u.d_int - getwpos(si.curwin),NULL);
					break;
				case sv_windNum:
					(void) gotoWind(dsink,datum->u.d_int,0);
					break;
				case sv_windSize:
					(void) resizeWind(dsink,datum->u.d_int,NULL);
					break;
				case sv_wordChars:
					(void) setwlist(disnil(datum) || disnull(datum) ? wordlistd : datum->d_str);
					break;
				case sv_workDir:
					str1 = "chgDir";
XeqCmd:
					{DStrFab cmd;
					(void) runcmd(dsink,&cmd,str1,datum->d_str,true);
					}
					break;
				case sv_wrapCol:
					(void) setwrap(datum->u.d_int,false);
					break;
				default:
					// Never should get here.
					return rcset(FatalError,0,text3,myname,svp->sv_id,svp->sv_name);
						// "%s(): Unknown ID %d for variable '%s'!"
				}
			}
			break;

		// Set a macro argument.
		case VTyp_NVar:
			if(vdp->i.vd_argnum == 0) {		// Allow numeric assignment (only) to $0.
				if(!intval(datum)) {
					str1 = "$0";
					goto BadTyp2;
					}
				dsetint(datum->u.d_int,scriptrun->narg);
				}
			else
				// Macro argument assignment.  Get array argument and set new value.
				(void) newval(awptr(vdp->p.vd_marg)->aw_ary->a_elp[vdp->i.vd_argnum - 1],datum,vdp);
			break;
		default:	// VTyp_ARef
			{Datum *el = aget(vdp->p.vd_ary,vdp->i.vd_index,false);
			if(el == NULL)
				goto LibFail;
			(void) datcpy(el,datum);
			}
			break;
#if 0
		// Never should get here.
		default:
			return rcset(FatalError,0,text180,myname,(uint) vdp->vd_type);
				// "%s(): Unknown type %.8X for variable!"
#endif
		}
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Create local or global user variable, given name and descriptor pointer.  Return status.
static int uvarnew(char *var,VDesc *vdp) {
	UVar *uvar;
	char *str;
	char *name = var + (*var == TokC_GVar ? 1 : 0);

	// Invalid length?
	if(*var == '\0' || *name == '\0' || strlen(var) > MaxVarName)
		return rcset(Failure,0,text280,text279,MaxVarName);
			// "%s name cannot be null or exceed %d characters","Variable"

	// Valid variable name?
	str = name;
	if(getident(&str,NULL) != s_ident || *str != '\0')
		(void) rcset(Failure,0,text447,text286,name);
			// "Invalid %s '%s'","identifier"

	// Allocate new record, set its values, and add to beginning of list.
	if((uvar = (UVar *) malloc(sizeof(UVar))) == NULL)
		return rcset(Panic,0,text94,"uvarnew");
				// "%s(): Out of memory!"
	strcpy((vdp->p.vd_uvp = uvar)->uv_name,var);
	if(*var == TokC_GVar) {
		vdp->vd_type = VTyp_GVar;
		uvar->uv_flags = V_Global;
		uvar->uv_next = gvarshead;
		gvarshead = uvar;
		}
	else {
		vdp->vd_type = VTyp_LVar;
		uvar->uv_flags = 0;
		uvar->uv_next = lvarshead;
		lvarshead = uvar;
		}

	// Set value of new variable to a null string.
	return dnew(&uvar->uv_datum);
	}

// Find a named variable's type and id.  If op is OpCreate: (1), create user variable if non-existent and either (a), variable
// is global; or (b), variable is local and executing a buffer; and (2), return status.  If op is OpQuery: return true if
// variable is found; otherwise, false.  If op is OpDelete: return status if variable is found; otherwise, error.  In all cases,
// store results in *vdp (if vdp not NULL) and variable is found.
int findvar(char *name,VDesc *vdp,ushort op) {
	UVar *uvar;
	ssize_t i;
	VDesc vd;

	// Get ready.
	vd.p.vd_uvp = NULL;
	vd.vd_type = VTyp_Unk;
	vd.i.vd_argnum = 0;

	// Check lead-in character.
	if(*name == TokC_GVar) {
		if(strlen(name) > 1) {

			// Macro argument reference?
			if(isdigit(name[1])) {
				long lval;

				// Yes, macro running and number in range?
				if(scriptrun != NULL && asc_long(name + 1,&lval,true) &&
				 lval <= awptr(scriptrun->marg)->aw_ary->a_used) {

					// Valid reference.  Set type and save argument number.
					vd.vd_type = VTyp_NVar;
					vd.i.vd_argnum = lval;
					vd.p.vd_marg = scriptrun->marg;
					goto Found;
					}
				}
			else {
				// Check for existing global variable.
				if((uvar = uvarfind(name)) != NULL)
					goto UVarFound;

				// Check for existing system variable.
				if(binsearch(name + 1,(void *) sysvars,NSVars,strcmp,svarname,&i)) {
					vd.vd_type = VTyp_SVar;
					vd.p.vd_svp = sysvars + i;
					goto Found;
					}

				// Not found.  Create new one?
				if(op == OpCreate)
					goto Create;
				goto VarNotFound;
				}
			}
		}
	else if(*name != '\0') {

		// Check for existing local variable.
		if((uvar = uvarfind(name)) != NULL) {
UVarFound:
			vd.vd_type = (uvar->uv_flags & V_Global) ? VTyp_GVar : VTyp_LVar;
			vd.p.vd_uvp = uvar;
			goto Found;
			}

		// Not found.  Create a new one (if executing a buffer)?
		if(op != OpCreate || scriptrun == NULL)
			goto VarNotFound;

		// Create variable.  Local variable name same as an existing command, pseudo-command, function, alias, or macro?
		if(execfind(name,OpQuery,PtrAny,NULL))
			return rcset(Failure,RCTermAttr,text165,name);
				// "Name '~b%s~B' already in use"
Create:
		if(uvarnew(name,&vd) != Success)
			return rc.status;
Found:
		if(vdp != NULL)
			*vdp = vd;
		return (op == OpQuery) ? true : rc.status;
		}
VarNotFound:
	// Variable not found.
	return (op == OpQuery) ? false : rcset(Failure,0,text52,name);
					// "No such variable '%s'"
	}

// Derefence a variable, given descriptor, and save variable's value in datum.  Return status.
int vderefv(Datum *datum,VDesc *vdp) {
	Datum *value;

	switch(vdp->vd_type) {
		case VTyp_LVar:
		case VTyp_GVar:
			value = vdp->p.vd_uvp->uv_datum;
			break;
		case VTyp_SVar:
			return getsvar(datum,vdp->p.vd_svp);
		case VTyp_NVar:
			{ushort argnum = vdp->i.vd_argnum;

			// Get argument value.  $0 resolves to the macro "n" argument.
			value = (argnum == 0) ? scriptrun->narg : awptr(vdp->p.vd_marg)->aw_ary->a_elp[argnum - 1];
			}
			break;
		default:	// VTyp_ARef
			if((value = aget(vdp->p.vd_ary,vdp->i.vd_index,false)) == NULL)
				return librcset(Failure);
		}

	// Copy value.
	return datcpy(datum,value) != 0 ? librcset(Failure) : rc.status;
	}

// Derefence a variable, given name, and save variable's value in datum.  Return status.
int vderefn(Datum *datum,char *name) {
	VDesc vd;

	// Find and dereference variable.
	if(findvar(name,&vd,OpDelete) == Success)
		(void) vderefv(datum,&vd);

	return rc.status;
	}

// Store the character value of a system variable in a string-fab object in "show" (?x) form.  Return status.
static int ctosf(DStrFab *dest,Datum *datum) {
	short c = datum->u.d_int;

	if(dputc('?',dest) != 0)
		goto LibFail;
	if(c < '!' || c > '~') {
		if(dputc('\\',dest) != 0)
			goto LibFail;
		switch(c) {
			case '\t':	// Tab
				c = 't'; break;
			case '\r':	// CR
				c = 'r'; break;
			case '\n':	// NL
				c = 'n'; break;
			case '\e':	// Escape
				c = 'e'; break;
			case ' ':	// Space
				c = 's'; break;
			case '\f':	// Form feed
				c = 'f'; break;
			default:
				if(dputf(dest,"x%.2hX",c) != 0)
					goto LibFail;
				return rc.status;
			}
		}
	if(dputc(c,dest) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Get the value of a system variable and store in a string-fab object in "show" form.  Pass flags to dtosfchk().  Return
// status.
int svtosf(DStrFab *dest,SVar *svp,ushort flags) {
	Datum *datum;
	Point *point = &si.curwin->w_face.wf_point;

	// Get value.  Return null for $RegionText if no region defined or empty; use single-quote form for $replacePat
	// and $searchPat; use char-lit form for character variables.
	if(dnewtrk(&datum) != 0)
		goto LibFail;
	if(svp->sv_id == sv_RegionText || (svp->sv_id == sv_lineText && point->lnp->l_used > term.t_ncol * 2)) {

		// Cap size of region or current line in case it's huge -- it will be truncated when displayed anyway.
		if(svp->sv_id == sv_lineText) {
			if(dsalloc(datum,term.t_ncol * 2 + 4) != 0)
				goto LibFail;
			strcpy((char *) memzcpy((void *) datum->d_str,(void *) (point->lnp->l_text),term.t_ncol * 2),"...");
			}
		else if(si.curbuf->b_mroot.mk_point.lnp != point->lnp || si.curbuf->b_mroot.mk_point.off != point->off) {
			Region region;
			bool truncated = false;

			if(getregion(&region,RegForceBegin) != Success)
				return rc.status;
			if(region.r_size > term.t_ncol * 2) {
				region.r_size = term.t_ncol * 2;
				truncated = true;
				}
			if(dsalloc(datum,region.r_size + 4) != 0)
				goto LibFail;
			regcpy(datum->d_str,&region);
			if(truncated)
				strcpy(datum->d_str + term.t_ncol * 2,"...");
			}
		else
			// Have zero-length region.
			dsetnull(datum);
		}
	else if(getsvar(datum,svp) != Success)
		return rc.status;

	// Have system variable value in *datum.  Convert it to display form.
	return (svp->sv_flags & V_Char) ? ctosf(dest,datum) : dtosfchk(dest,datum,",",flags | (svp->sv_id == sv_replacePat
	 || svp->sv_id == sv_searchPat ? CvtShowNil | CvtVizStr | CvtQuote1 : CvtExpr));
LibFail:
	return librcset(Failure);
	}

// Set a variable -- "let" command (interactively only).  Evaluate value as an expression if n argument.  Return status.
int setvar(Datum *rval,int n,Datum **argv) {
	VDesc vd;			// Variable descriptor.
	ushort delim;
	uint aflags,cflags;
	long lval;
	Datum *datum;
	DStrFab sfab;

	// First get the variable to set.
	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	if(terminp(datum,text51,ArgNil1,Term_C_SVar,NULL) != Success || datum->d_type == dat_nil)
			// "Assign variable"
		goto Retn;

	// Find variable.
	if(findvar(datum->d_str,&vd,OpCreate) != Success)
		goto Retn;

	// Error if read-only.
	if(vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_RdOnly))
		return rcset(Failure,RCTermAttr,text164,datum->d_str);
				// "Cannot modify read-only variable '~b%s~B'"

	// Build prompt with old value.
	if(n == INT_MIN) {
		delim = (vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_EscDelim)) ? (Ctrl | '[') : RtnKey;
		aflags = ArgNil1;
		cflags = (vd.vd_type != VTyp_SVar || !(vd.p.vd_svp->sv_flags & (V_Char | V_GetKey | V_GetKeySeq))) ? 0 :
		 (vd.p.vd_svp->sv_flags & V_Char) ? Term_OneChar : (vd.p.vd_svp->sv_flags & V_GetKeySeq) ? Term_OneKeySeq :
		 Term_OneKey;
		}
	else {
		delim = RtnKey;
		aflags = ArgNotNull1;
		cflags = 0;
		}
	if(dopenwith(&sfab,rval,SFClear) != 0 || dputs(text297,&sfab) != 0)
						// "Current value: "
		goto LibFail;
	if(vd.vd_type == VTyp_SVar) {
		if(vd.p.vd_svp->sv_flags & (V_GetKey | V_GetKeySeq)) {
			if(getsvar(datum,vd.p.vd_svp) != Success)
				goto Retn;
			if(dputf(&sfab,"%c%c%c%s%c%c",AttrSpecBegin,AttrAlt,AttrULOn,datum->d_str,AttrSpecBegin,AttrULOff) != 0)
				goto LibFail;
			cflags |= Term_Attr;
			}
		else if(svtosf(&sfab,vd.p.vd_svp,0) != Success)
			goto Retn;
		}
	else if(dtosfchk(&sfab,vd.p.vd_uvp->uv_datum,NULL,CvtExpr | CvtForceArray) != Success)
		goto Retn;

	// Add "new value" type to prompt.
	if(dputs(text283,&sfab) != 0)
			// ", new value"
		goto LibFail;
	if(n != INT_MIN) {
		if(dputs(text301,&sfab) != 0)
				// " (expression)"
			goto LibFail;
		}
	else if(vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & (V_Char | V_GetKey | V_GetKeySeq))) {
		if(dputs(vd.p.vd_svp->sv_flags & V_Char ? text349 : text76,&sfab) != 0)
						// " (char)"," (key)"
			goto LibFail;
		}
	if(dclose(&sfab,sf_string) != 0)
LibFail:
		return librcset(Failure);

	// Get new value.
	TermInp ti = {NULL,delim,0,NULL};
	if(terminp(datum,rval->d_str,aflags,cflags,&ti) != Success)
		goto Retn;

	// Evaluate result as an expression if requested.  Type checking is done in putvar() so no need to do it here.
	if(n != INT_MIN) {
		if(execestmt(rval,datum->d_str,TokC_ComLine,NULL) != Success)
			goto Retn;
		}
	else if((datum->d_type & DStrMask) && (vd.vd_type == VTyp_GVar ||
	 (vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_Int))) && asc_long(datum->d_str,&lval,true))
		dsetint(lval,rval);
	else
		datxfer(rval,datum);

	// Set variable to value in rval and return.
#if MMDebug & Debug_Datum
	ddump(rval,"setvar(): Setting and returning value...");
	(void) putvar(rval,&vd);
	dumpvars();
#else
	(void) putvar(rval,&vd);
#endif
Retn:
	return rc.status;
	}

// Convert an array reference node to a VDesc object and check if referenced element exists.  If not, create it if "create" is
// true; otherwise, set an error.  Return status.
int aryget(ENode *np,VDesc *vdp,bool create) {

	vdp->vd_type = VTyp_ARef;
	vdp->i.vd_index = np->en_index;
	vdp->p.vd_ary = awptr(np->en_rp)->aw_ary;
	if(aget(vdp->p.vd_ary,vdp->i.vd_index,create) == NULL)
		(void) librcset(Failure);
	return rc.status;
	}

// Increment or decrement a variable or array reference, given node pointer, "incr" flag, and "pre" flag.  Set node to result
// and return status.
int bumpvar(ENode *np,bool incr,bool pre) {
	VDesc vd;
	long lval;
	Datum *datum;

	if(np->en_flags & EN_ArrayRef) {
		if(aryget(np,&vd,false) != Success)		// Get array element...
			return rc.status;
		if(!intvar(&vd))				// and make sure it's an integer.
			return rcset(Failure,0,text370,vd.i.vd_index);
				// "Array element %d not an integer";
		}
	else {
		if(findvar(np->en_rp->d_str,&vd,OpDelete) != Success)	// or find variable...
			return rc.status;
		if(!intvar(&vd))					// and make sure it's an integer.
			return rcset(Failure,RCTermAttr,text212,np->en_rp->d_str);
				// "Variable '~b%s~B' not an integer"
		}
	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	if(vderefv(datum,&vd) != Success)			// Dereference variable...
		return rc.status;
	lval = datum->u.d_int + (incr ? 1 : -1);			// compute new value of variable...
	dsetint(pre ? lval : datum->u.d_int,np->en_rp);		// set result to pre or post value...
	dsetint(lval,datum);					// set new variable value in a datum object...
	return putvar(datum,&vd);				// and update variable.
	}

#if MMDebug & Debug_Datum
// Dump all user variables to the log file.
void dumpvars(void) {
	UVar *uvar;
	struct uvinfo {
		char *label;
		UVar **uheadp;
		} *uvi;
	static struct uvinfo uvtab[] = {
		{"GLOBAL",&gvarshead},
		{"LOCAL",&lvarshead},
		{NULL,NULL}};

	do {
		fprintf(logfile,"%s VARS\n",uvi->label);
		if((uvar = *uvi->uheadp) != NULL) {
			do {
				ddump(uvar->uv_datum,uvar->uv_name);
				} while((uvar = uvar->uv_next) != NULL);
			}
		} while((++uvi)->label != NULL);
	}
#endif
