// (c) Copyright 2018 Richard W. Marinelli
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
	Datum *datp;

	switch(vdp->vd_type) {
		case VTyp_LVar:
		case VTyp_GVar:
			datp = vdp->p.vd_uvp->uv_datp;
			break;
		case VTyp_SVar:
			return (vdp->p.vd_svp->sv_flags & (V_Int | V_Char)) != 0;
		case VTyp_NVar:
			{ushort argnum = vdp->i.vd_argnum;

			// Get argument value.  $0 resolves to the macro "n" argument.
			datp = (argnum == 0) ? scriptrun->nargp : awptr(vdp->p.vd_margp)->aw_aryp->a_elpp[argnum - 1];
			}
			break;
		default:	// VTyp_ARef
			datp = aget(vdp->p.vd_aryp,vdp->i.vd_index,false);	// Should never return NULL.

		}
	return datp->d_type == dat_int;
	}

// Return true if c is a valid first character of an identifier; otherwise, false.
bool isident1(short c) {

	return isletter(c) || c == '_';
	}

// Return number of variables currently in use (for building a name completion list).
uint varct(uint cflags) {
	UVar *uvp;
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
	for(uvp = gvarsheadp; uvp != NULL; uvp = uvp->uv_nextp)
		if(!(cflags & Term_C_SVar) || is_lower(uvp->uv_name[1]))
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
	UVar *uvp;
	char **vlistv0 = vlistv;

	// Store system variable names.
	svp = sysvars;
	do {
		if(!(cflags & Term_C_SVar) || is_lower(svp->sv_name[1]))
			*vlistv++ = svp->sv_name;
		} while((++svp)->sv_name != NULL);

	// Store global variable names.
	for(uvp = gvarsheadp; uvp != NULL; uvp = uvp->uv_nextp)
		if(!(cflags & Term_C_SVar) || is_lower(uvp->uv_name[1]))
			*vlistv++ = uvp->uv_name;

	// Sort it.
	qsort((void *) vlistv0,count,sizeof(char *),varcmp);
	}

// Free user variable(s), given "stack" pointer.  All variables will be together at top of list (because they are created in
// stack fashion during macro execution and/or recursion).
int uvarclean(UVar *vstackp) {
	UVar *uvp;

	while(lvarsheadp != vstackp) {
		uvp = lvarsheadp->uv_nextp;

		// Free value...
#if DCaller
		ddelete(lvarsheadp->uv_datp,"uvarclean");
#else
		ddelete(lvarsheadp->uv_datp);
#endif

		// free variable...
		free((void *) lvarsheadp);

		// and advance head pointer.
		lvarsheadp = uvp;
		}
	return rc.status;
	}

// Search global or local variable list for given name (with prefix).  If found, return pointer to UVar record; otherwise,
// return NULL.  Local variable entries beyond scriptrun->uvp are not considered so that all local variables created or
// referenced in a particular macro invocation are visible and accessible only from that macro, which allows recursion to work
// properly.
UVar *uvarfind(char *var) {
	UVar *uvp;
	UVar *vstackp;		// Search boundary in local variable table.

	if(*var == TokC_GVar) {
		uvp = gvarsheadp;
		vstackp = NULL;
		}
	else {
		uvp = lvarsheadp;
		vstackp = (scriptrun == NULL) ? NULL : scriptrun->uvp;
		}

	while(uvp != vstackp) {
		if(strcmp(var,uvp->uv_name) == 0)
			return uvp;
		uvp = uvp->uv_nextp;
		}

	return NULL;
	}

// binsearch() helper function for returning system variable name, given table (array) and index.
static char *svarname(void *table,ssize_t i) {

	return ((SVar *) table)[i].sv_name + 1;
	}

// Place the list of characters considered "in a word" into rp.  Return status.
static int getwlist(Datum *rp) {
	DStrFab sf;
	char *str,*strz;

	if(dopenwith(&sf,rp,SFClear) != 0)
		goto LibFail;

	// Build the string of characters in the result buffer.
	strz = (str = wordlist) + 256;
	do {
		if(*str && dputc(str - wordlist,&sf) != 0)
			goto LibFail;
		} while(++str < strz);

	if(dclose(&sf,sf_string) != 0)
LibFail:
		(void) librcset(Failure);

	return rc.status;
	}

// Replace the current line with the given text.  Return status.  (Used only for setting the $lineText system variable.)
static int putctext(char *iline) {

	if(allowedit(true) != Success)		// Don't allow if in read-only mode.
		return rc.status;

	// Delete the text on the current line.
	si.curwp->w_face.wf_dot.off = 0;	// Start at the beginning of the line.
	if(kdctext(1,0,NULL) != Success)	// Put it in the undelete buffer.
		return rc.status;

	// Insert the new text.
	if(linstr(iline) == Success)
		si.curwp->w_face.wf_dot.off = 0;

	return rc.status;
	}

// Encode the current keyboard macro into dest in string form using ektos().  Return status.
static int kmtos(Datum *destp) {

	// Recording a keyboard macro?
	if(kmacro.km_state == KMRecord) {
		clearKeyMacro(true);
		return rcset(Failure,RCNoFormat,text338);
			// "Cannot access '$keyMacro' from a keyboard macro, cancelled"
		}

	// Null keyboard macro?
	if(kmacro.km_slotp == kmacro.km_buf)
		dsetnull(destp);
	else {
		ushort *kmp;
		char *str;
		DStrFab dest;
		char wkbuf[16];

		// Find a delimter that can be used (a character that is not in the macro).  Default to tab.
		str = KMDelims;
		wkbuf[0] = '\t';
		do {
			kmp = kmacro.km_buf;
			do {
				if(*kmp == *str)
					goto Next;
				} while(++kmp < kmacro.km_endp);

			// Found.
			wkbuf[0] = *str;
			break;
Next:;
			} while(*++str != '\0');

		// Loop through keyboard macro keys and translate each into dest with delimiter found in previous step.
		if(dopenwith(&dest,destp,SFClear) != 0)
			goto LibFail;
		kmp = kmacro.km_buf;
		do {
			ektos(*kmp,wkbuf + 1,false);
			if(dputs(wkbuf,&dest) != 0)
				goto LibFail;
			} while(++kmp < kmacro.km_endp);
		if(dclose(&dest,sf_string) != 0)
LibFail:
			return librcset(Failure);
		}

	return rc.status;
	}

// Get value of a system variable, given result pointer and table pointer.
static int getsvar(Datum *rp,SVar *svp) {
	enum svarid vid;
	char *str;
	char wkbuf[16];

	// Fetch the corresponding value.
	if(svp->u.sv_str != NULL) {
		if(!(svp->sv_flags & (V_Int | V_Char))) {
			str = svp->u.sv_str;
			goto Kopy;
			}
		dsetint(svp->u.sv_int,rp);
		goto Retn;
		}
	else {
		switch(vid = svp->sv_id) {
			case sv_ARGV:
				if(scriptrun == NULL)
					dsetnil(rp);
				else if(datcpy(rp,scriptrun->margp) != 0)
					goto LibFail;
				break;
			case sv_BufInpDelim:
				str = si.curbp->b_inpdelim;
				goto Kopy;
			case sv_BufModes:
				(void) getmodes(rp,si.curbp);
				break;
			case sv_Date:
				str = timeset();
				goto Kopy;
			case sv_GlobalModes:
				(void) getmodes(rp,NULL);
				break;
			case sv_HorzScrollCol:
				dsetint(modeset(MdIdxHScrl,NULL) ? si.curwp->w_face.wf_firstcol : si.cursp->s_firstcol,rp);
				break;
			case sv_LastKey:
				dsetint((long)(kentry.lastkseq & (Prefix | Shft | FKey | 0x80) ? -1 :
				 kentry.lastkseq & Ctrl ? ektoc(kentry.lastkseq,false) : kentry.lastkseq),rp);
				break;
			case sv_LineLen:
				dsetint((long) si.curwp->w_face.wf_dot.lnp->l_used,rp);
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
				if(dsalloc(rp,region.r_size + 1) != 0)
					goto LibFail;
				regcpy(rp->d_str,&region);
				}
				break;
			case sv_ReturnMsg:
				str = scriptrc.msg.d_str;
				goto Kopy;
			case sv_RunFile:
				str = fixnull((scriptrun == NULL) ? NULL : scriptrun->path);
				goto Kopy;
			case sv_RunName:
				{Buffer *bufp = (scriptrun == NULL) ? NULL : scriptrun->bufp;
				str = fixnull(bufp == NULL ? NULL : *bufp->b_bname == SBMacro ? bufp->b_bname + 1 :
				 bufp->b_bname);
				}
				goto Kopy;
			case sv_ScreenCount:
				dsetint((long) scrcount(),rp);
				break;
			case sv_TermCols:
				dsetint((long) term.t_ncol,rp);
				break;
			case sv_TermRows:
				dsetint((long) term.t_nrow,rp);
				break;
			case sv_WindCount:
				dsetint((long) wincount(si.cursp,NULL),rp);
				break;
			case sv_autoSave:
				dsetint((long) si.gasave,rp);
				break;
			case sv_bufFile:
				if((str = si.curbp->b_fname) != NULL)
					goto Kopy;
				dsetnil(rp);
				break;
			case sv_bufLineNum:
				dsetint(getlinenum(si.curbp,si.curwp->w_face.wf_dot.lnp),rp);
				break;
			case sv_bufName:
				str = si.curbp->b_bname;
				goto Kopy;
			case sv_execPath:
				str = execpath;
				goto Kopy;
			case sv_fencePause:
				dsetint((long) si.fencepause,rp);
				break;
			case sv_hardTabSize:
				dsetint((long) si.htabsize,rp);
				break;
			case sv_horzJump:
				dsetint((long) vtc.hjump,rp);
				break;
			case sv_inpDelim:
				str = fi.inpdelim;
				goto Kopy;
			case sv_keyMacro:
				(void) kmtos(rp);
				break;
			case sv_killRingSize:
				dsetint((long) kring.r_maxsize,rp);
				break;
			case sv_lastKeySeq:
				ektos(kentry.lastkseq,str = wkbuf,false);
				goto Kopy;
			case sv_lineChar:
				{Dot *dotp = &si.curwp->w_face.wf_dot;
				dsetint(bufend(dotp) ? '\0' : dotp->off == dotp->lnp->l_used ? '\n' :
				 dotp->lnp->l_text[dotp->off],rp);
				}
				break;
			case sv_lineCol:
				dsetint((long) getccol(NULL),rp);
				break;
			case sv_lineOffset:
				dsetint((long) si.curwp->w_face.wf_dot.off,rp);
				break;
			case sv_lineText:
				{Line *lnp = si.curwp->w_face.wf_dot.lnp;
				if(dsetsubstr(lnp->l_text,lnp->l_used,rp) != 0)
					goto LibFail;
				}
				break;
			case sv_maxArrayDepth:
				dsetint((long) maxarydepth,rp);
				break;
			case sv_maxLoop:
				dsetint((long) maxloop,rp);
				break;
			case sv_maxMacroDepth:
				dsetint((long) maxmacdepth,rp);
				break;
			case sv_maxPromptPct:
				dsetint((long) si.maxprmt,rp);
				break;
			case sv_otpDelim:
				str = fi.otpdelim;
				goto Kopy;
			case sv_pageOverlap:
				dsetint((long) si.overlap,rp);
				break;
			case sv_randNumSeed:
				dsetint(si.randseed & LONG_MAX,rp);
				break;
			case sv_replacePat:
				str = srch.m.rpat;
				goto Kopy;
			case sv_replaceRingSize:
				dsetint((long) rring.r_maxsize,rp);
				break;
			case sv_screenNum:
				dsetint((long) si.cursp->s_num,rp);
				break;
			case sv_searchDelim:
				ektos(srch.sdelim,str = wkbuf,false);
				goto Kopy;
			case sv_searchPat:
				{char patbuf[srch.m.patlen + OptCh_N + 1];

				if(dsetstr(mkpat(patbuf,&srch.m),rp) != 0)
					goto LibFail;
				}
				break;
			case sv_searchRingSize:
				dsetint((long) sring.r_maxsize,rp);
				break;
			case sv_softTabSize:
				dsetint((long) si.stabsize,rp);
				break;
			case sv_travJump:
				dsetint((long) si.tjump,rp);
				break;
			case sv_vertJump:
				dsetint((long) vtc.vjump,rp);
				break;
			case sv_windLineNum:
				dsetint((long) getwpos(si.curwp),rp);
				break;
			case sv_windNum:
				dsetint((long) getwnum(si.curwp),rp);
				break;
			case sv_windSize:
				dsetint((long) si.curwp->w_nrows,rp);
				break;
			case sv_wordChars:
				(void) getwlist(rp);
				break;
			case sv_workDir:
				str = si.cursp->s_wkdir;
				goto Kopy;
			case sv_wrapCol:
				dsetint((long) si.wrapcol,rp);
				break;
			default:
				// Never should get here.
				return rcset(FatalError,0,text3,"getsvar",vid,svp->sv_name);
						// "%s(): Unknown ID %d for variable '%s'!"
			}
		goto Retn;
		}
Kopy:
	if(dsetstr(str,rp) != 0)
LibFail:
		(void) librcset(Failure);
Retn:
	return rc.status;
	}

// Set a list of characters to be considered in a word.  Return status.
int setwlist(char *wclist) {
	char *str,*strz;
	DStrFab sf;

	// First, expand the new value (and close the string-fab object)...
	if(strexpand(&sf,wclist) != Success)
		return rc.status;

	// clear the word list table...
	strz = (str = wordlist) + 256;
	do {
		*str++ = false;
		} while(str < strz);

	// and for each character in the new value, set that element in the table.
	str = sf.sf_datp->d_str;
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
			si.curwp->w_flags |= WFMode;
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
		Datum *datp;

		if(dnewtrk(&datp) != 0)
			return librcset(Failure);

		// Parse tokens and save in keyboard macro array.
		while(parsetok(datp,&estr,delim) != NotFound) {

			// Convert token string to a key sequence.
			if(*datp->d_str == '\0')
				return rcset(Failure,0,text254,"");
					// "Invalid key literal '%s'"
			if(stoek(datp->d_str,&ek) != Success)
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
				if(kmacro.km_slotp == kmacro.km_buf + kmacro.km_size && growKeyMacro() != Success)
					return rc.status;
				*kmacro.km_slotp++ = c;
				} while(!last);
			}
		kmacro.km_endp = kmacro.km_slotp;
		}

	return rc.status;
	}

// Copy a new value to a variable, checking if old value is an array in a global variable.
static int newval(Datum *destp,Datum *srcp,VDesc *vdp) {

	if(destp->d_type == dat_blobRef && vdp->vd_type == VTyp_GVar)
		agarbpush(destp);
	return datcpy(destp,srcp) != 0 ? librcset(Failure) : rc.status;
	}

// Set a variable to given value.  Return status.
int putvar(Datum *datp,VDesc *vdp) {
	char *str1;
	static char myname[] = "putvar";

	// Set the appropriate value.
	switch(vdp->vd_type) {

		// Set a user variable.
		case VTyp_LVar:
		case VTyp_GVar:
			{UVar *uvp = vdp->p.vd_uvp;				// Grab pointer to old value.
			(void) newval(uvp->uv_datp,datp,vdp);
			}
			break;

		// Set a system variable.
		case VTyp_SVar:
			{int i;
			ushort ek;
			SVar *svp = vdp->p.vd_svp;
			Datum *dsinkp;						// For throw-away return value, if any.

			// Can't modify a read-only variable.
			if(svp->sv_flags & V_RdOnly)
				return rcset(Failure,RCTermAttr,text164,svp->sv_name);
						// "Cannot modify read-only variable '~b%s~0'"

			// Not read-only.  Check for legal value types.
			if(svp->sv_flags & V_Int) {
				if(!intval(datp))
					goto BadTyp1;
				}
			else if(svp->sv_flags & V_Char) {
				if(!charval(datp))
					goto BadTyp1;
				}
#if 0
			else if(svp->sv_flags & V_Array) {
				if(!aryval(datp))
					goto BadTyp1;
				}
#endif
			else if(datp->d_type & DBoolMask) {
				str1 = text360;
					// "Boolean"
				goto BadTyp0;
				}
			else if(datp->d_type == dat_nil) {
				if(svp->sv_flags & V_Nil)
					dsetnull(datp);
				else {
					str1 = text359;
						// "nil"
BadTyp0:
					(void) rcset(Failure,0,text358,str1);
							// "Illegal use of %s value"
					goto BadTyp1;
					}
				}
			else if(!strval(datp)) {
				DStrFab msg;
BadTyp1:
				str1 = svp->sv_name;
BadTyp2:
				if(dopentrk(&msg) != 0)
					goto LibFail;
				else if(escattr(&msg) == Success) {
					if(dputf(&msg,text334,str1) != 0 || dclose(&msg,sf_string) != 0)
							// ", setting variable '~b%s~0'"
						goto LibFail;
					(void) rcset(rc.status,RCForce | RCNoFormat | RCTermAttr,msg.sf_datp->d_str);
					}
				return rc.status;
				}

			// Do specific action for referenced (mutable) variable.
			if(dnewtrk(&dsinkp) != 0)
				goto LibFail;
			switch(svp->sv_id) {
				case sv_autoSave:
					{int n;

					if(datp->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					if((n = (datp->u.d_int > INT_MAX ? INT_MAX : datp->u.d_int)) == 0) {

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
					str1 = "0 => setBufFile $bufName,";
					goto XeqCmd;
				case sv_bufLineNum:
					(void) goline(dsinkp,INT_MIN,datp->u.d_int);
					break;
				case sv_bufName:
					str1 = "renameBuf";
					goto XeqCmd;
				case sv_execPath:
					(void) setpath(datp->d_str,false);
					break;
				case sv_fencePause:
					if(datp->u.d_int < 0)
						return rcset(Failure,0,text39,text119,(int) datp->u.d_int,0);
							// "%s (%d) must be %d or greater","Pause duration"
					si.fencepause = datp->u.d_int;
					break;
				case sv_hardTabSize:
					if(settab((int) datp->u.d_int,true) == Success)
						(void) supd_wflags(NULL,WFHard | WFMode);
					break;
				case sv_horzJump:
					if((vtc.hjump = datp->u.d_int) < 0)
						vtc.hjump = 0;
					else if(vtc.hjump > JumpMax)
						vtc.hjump = JumpMax;
					if((vtc.hjumpcols = vtc.hjump * term.t_ncol / 100) == 0)
						vtc.hjumpcols = 1;
					break;
				case sv_inpDelim:
					if(strlen(datp->d_str) > sizeof(fi.inpdelim) - 1)
						return rcset(Failure,0,text251,text46,datp->d_str,sizeof(fi.inpdelim) - 1);
							// "%s delimiter '%s' cannot be more than %d character(s)","Input"
					strcpy(fi.inpdelim,datp->d_str);
					break;
				case sv_keyMacro:
					(void) stokm(datp->d_str);
					break;
				case sv_lastKeySeq:
					if(stoek(datp->d_str,&ek) == Success) {
						KeyBind *kbp = getbind(ek);
						if(kbp != NULL) {
							UnivPtr *univp = &kbp->k_targ;
							if(univp->p_type == PtrPseudo && (univp->u.p_cfp->cf_aflags & CFPrefix))
								return rcset(Failure,RCTermAttr,text373,svp->sv_name);
										// "Illegal value for '~b%s~0' variable"
							}
						kentry.lastkseq = ek;
						kentry.uselast = true;
						}
					break;
				case sv_lineChar:
					// Replace character at point with an integer ASCII value.
					if(charval(datp)) {
						if(ldelete(1L,0) != Success)
							return rcset(Failure,0,text142,si.curbp->b_bname);
								// "Cannot change a character past end of buffer '%s'"
						(void) (datp->u.d_int == 012 ? lnewline() : linsert(1,datp->u.d_int));
						}
					break;
				case sv_lineCol:
					(void) setccol(datp->u.d_int);
					break;
				case sv_lineOffset:
					{int llen = si.curwp->w_face.wf_dot.lnp->l_used;
					int loff = (datp->u.d_int < 0) ? llen + datp->u.d_int : datp->u.d_int;
					if(loff < 0 || loff > llen)
						return rcset(Failure,0,text378,datp->u.d_int);
							// "Line offset value %ld out of range"
					si.curwp->w_face.wf_dot.off = loff;
					}
					break;
				case sv_lineText:
					(void) putctext(datp->d_str);
					break;
				case sv_maxArrayDepth:
					if(datp->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					maxarydepth = datp->u.d_int;
					break;
				case sv_maxLoop:
					if(datp->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					maxloop = datp->u.d_int;
					break;
				case sv_maxMacroDepth:
					if(datp->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					maxmacdepth = datp->u.d_int;
					break;
				case sv_maxPromptPct:
					if(datp->u.d_int < 15 || datp->u.d_int > 90)
						return rcset(Failure,RCTermAttr,text379,svp->sv_name,15,90);
							// "'~b%s~0' value must be between %d and %d"
					si.maxprmt = datp->u.d_int;
					break;
				case sv_otpDelim:
					if((size_t) (i = strlen(datp->d_str)) > sizeof(fi.otpdelim) - 1)
						return rcset(Failure,0,text251,text47,datp->d_str,
							// "%s delimiter '%s' cannot be more than %d character(s)","Output"
						 (int) sizeof(fi.otpdelim) - 1);
					strcpy(fi.otpdelim,datp->d_str);
					fi.otpdelimlen = i;
					break;
				case sv_pageOverlap:
					if(datp->u.d_int < 0 || datp->u.d_int > (int) (term.t_nrow - 1) / 2)
						return rcset(Failure,0,text184,datp->u.d_int,(int) (term.t_nrow - 1) / 2);
							// "Overlap %ld must be between 0 and %d"
					si.overlap = datp->u.d_int;
					break;
				case sv_randNumSeed:

					// Generate new seed if zero.
					if((si.randseed = (ulong) datp->u.d_int) == 0)
						si.randseed = seedinit();
					break;
				case sv_replacePat:
					(void) newrpat(datp->d_str,&srch.m);
					break;
				case sv_screenNum:
					(void) gotoScreen(datp->u.d_int,0);
					break;
				case sv_searchDelim:
					if(stoek(datp->d_str,&ek) != Success)
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
					{char wkpat[strlen(datp->d_str) + 1];
					(void) newspat(strcpy(wkpat,datp->d_str),&srch.m,NULL);
					}
					break;
				case sv_killRingSize:
				case sv_replaceRingSize:
				case sv_searchRingSize:
					{Ring *ringp = (svp->sv_id == sv_killRingSize) ? &kring : (svp->sv_id ==
					 sv_replaceRingSize) ? &rring : &sring;
					if(datp->u.d_int < 0) {
						i = 0;
ERange:
						return rcset(Failure,RCTermAttr,text111,svp->sv_name,i);
							// "'~b%s~0' value must be %d or greater"
						}
					if(datp->u.d_int < ringp->r_size)
						return rcset(Failure,0,text241,datp->u.d_int,ringp->r_size);
							// "Maximum ring size (%ld) less than current size (%d)"
					ringp->r_maxsize = datp->u.d_int;
					}
					break;
				case sv_softTabSize:
					if(settab((int) datp->u.d_int,false) == Success)
						(void) supd_wflags(NULL,WFHard | WFMode);
					break;
				case sv_travJump:
					if((si.tjump = datp->u.d_int) < 4)
						si.tjump = 4;
					else if(si.tjump > term.t_ncol / 4 - 1)
						si.tjump = term.t_ncol / 4 - 1;
					break;
				case sv_vertJump:
					if((vtc.vjump = datp->u.d_int) < 0)
						vtc.vjump = 0;
					else if(vtc.vjump > JumpMax)
						vtc.vjump = JumpMax;
					break;
				case sv_windLineNum:
					(void) forwLine(dsinkp,datp->u.d_int - getwpos(si.curwp),NULL);
					break;
				case sv_windNum:
					(void) gotoWind(dsinkp,datp->u.d_int,0);
					break;
				case sv_windSize:
					(void) resizeWind(dsinkp,datp->u.d_int,NULL);
					break;
				case sv_wordChars:
					(void) setwlist(disnil(datp) || disnull(datp) ? wordlistd : datp->d_str);
					break;
				case sv_workDir:
					str1 = "chgDir";
XeqCmd:
					{DStrFab cmd;
					(void) runcmd(dsinkp,&cmd,str1,datp->d_str,true);
					}
					break;
				case sv_wrapCol:
					(void) setwrap(datp->u.d_int,false);
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
				if(!intval(datp)) {
					str1 = "$0";
					goto BadTyp2;
					}
				dsetint(datp->u.d_int,scriptrun->nargp);
				}
			else
				// Macro argument assignment.  Get array argument and set new value.
				(void) newval(awptr(vdp->p.vd_margp)->aw_aryp->a_elpp[vdp->i.vd_argnum - 1],datp,vdp);
			break;
		default:	// VTyp_ARef
			{Datum *elp = aget(vdp->p.vd_aryp,vdp->i.vd_index,false);
			if(elp == NULL)
				goto LibFail;
			(void) datcpy(elp,datp);
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
	UVar *uvp;
	char *str;
	char *name = var + (*var == TokC_GVar ? 1 : 0);

	// Invalid length?
	if(*var == '\0' || *name == '\0' || strlen(var) > MaxVarName)
		return rcset(Failure,0,text280,text279,MaxVarName);
			// "%s name cannot be null or exceed %d characters","Variable"

	// Valid variable name?
	str = name;
	if(getident(&str,NULL) != s_ident || *str != '\0')
		(void) rcset(Failure,0,text286,name);
			// "Invalid identifier '%s'"

	// Allocate new record, set its values, and add to beginning of list.
	if((uvp = (UVar *) malloc(sizeof(UVar))) == NULL)
		return rcset(Panic,0,text94,"uvarnew");
				// "%s(): Out of memory!"
	strcpy((vdp->p.vd_uvp = uvp)->uv_name,var);
	if(*var == TokC_GVar) {
		vdp->vd_type = VTyp_GVar;
		uvp->uv_flags = V_Global;
		uvp->uv_nextp = gvarsheadp;
		gvarsheadp = uvp;
		}
	else {
		vdp->vd_type = VTyp_LVar;
		uvp->uv_flags = 0;
		uvp->uv_nextp = lvarsheadp;
		lvarsheadp = uvp;
		}

	// Set value of new variable to a null string.
	return dnew(&uvp->uv_datp);
	}

// Find a named variable's type and id.  If op is OpCreate: (1), create user variable if non-existent and either (a), variable
// is global; or (b), variable is local and executing a buffer; and (2), return status.  If op is OpQuery: return true if
// variable is found; otherwise, false.  If op is OpDelete: return status if variable is found; otherwise, error.  In all cases,
// store results in *vdp (if vdp not NULL) and variable is found.
int findvar(char *name,VDesc *vdp,ushort op) {
	UVar *uvp;
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
				 lval <= awptr(scriptrun->margp)->aw_aryp->a_used) {

					// Valid reference.  Set type and save argument number.
					vd.vd_type = VTyp_NVar;
					vd.i.vd_argnum = lval;
					vd.p.vd_margp = scriptrun->margp;
					goto Found;
					}
				}
			else {
				// Check for existing global variable.
				if((uvp = uvarfind(name)) != NULL)
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
		if((uvp = uvarfind(name)) != NULL) {
UVarFound:
			vd.vd_type = (uvp->uv_flags & V_Global) ? VTyp_GVar : VTyp_LVar;
			vd.p.vd_uvp = uvp;
			goto Found;
			}

		// Not found.  Create a new one (if executing a buffer)?
		if(op != OpCreate || scriptrun == NULL)
			goto VarNotFound;

		// Create variable.  Local variable name same as an existing command, pseudo-command, function, alias, or macro?
		if(execfind(name,OpQuery,PtrAny,NULL))
			return rcset(Failure,RCTermAttr,text165,name);
				// "Name '~b%s~0' already in use"
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

// Derefence a variable, given descriptor, and save variable's value in datp.  Return status.
int vderefv(Datum *datp,VDesc *vdp) {
	Datum *valp;

	switch(vdp->vd_type) {
		case VTyp_LVar:
		case VTyp_GVar:
			valp = vdp->p.vd_uvp->uv_datp;
			break;
		case VTyp_SVar:
			return getsvar(datp,vdp->p.vd_svp);
		case VTyp_NVar:
			{ushort argnum = vdp->i.vd_argnum;

			// Get argument value.  $0 resolves to the macro "n" argument.
			valp = (argnum == 0) ? scriptrun->nargp : awptr(vdp->p.vd_margp)->aw_aryp->a_elpp[argnum - 1];
			}
			break;
		default:	// VTyp_ARef
			if((valp = aget(vdp->p.vd_aryp,vdp->i.vd_index,false)) == NULL)
				return librcset(Failure);
		}

	// Copy value.
	return datcpy(datp,valp) != 0 ? librcset(Failure) : rc.status;
	}

// Derefence a variable, given name, and save variable's value in datp.  Return status.
int vderefn(Datum *datp,char *name) {
	VDesc vd;

	// Find and dereference variable.
	if(findvar(name,&vd,OpDelete) == Success)
		(void) vderefv(datp,&vd);

	return rc.status;
	}

// Store the character value of a system variable in a string-fab object in "show" (?x) form.  Return status.
static int ctosf(DStrFab *destp,Datum *datp) {
	short c = datp->u.d_int;

	if(dputc('?',destp) != 0)
		goto LibFail;
	if(c < '!' || c > '~') {
		if(dputc('\\',destp) != 0)
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
				if(dputf(destp,"x%.2hX",c) != 0)
					goto LibFail;
				return rc.status;
			}
		}
	if(dputc(c,destp) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Get the value of a system variable and store in a string-fab object in "show" form.  Pass flags to dtosfchk().  Return
// status.
static int svtosf(DStrFab *destp,SVar *svp,ushort flags) {
	Datum *datp;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Get value.  Return null for $RegionText if no region defined or empty; use single-quote form for $replacePat
	// and $searchPat; use char-lit form for character variables.
	if(dnewtrk(&datp) != 0)
		goto LibFail;
	if(svp->sv_id == sv_RegionText || (svp->sv_id == sv_lineText && dotp->lnp->l_used > term.t_ncol * 2)) {

		// Cap size of region or current line in case it's huge -- it will be truncated when displayed anyway.
		if(svp->sv_id == sv_lineText) {
			if(dsalloc(datp,term.t_ncol * 2 + 4) != 0)
				goto LibFail;
			strcpy((char *) memzcpy((void *) datp->d_str,(void *) (dotp->lnp->l_text),term.t_ncol * 2),"...");
			}
		else if(si.curbp->b_mroot.mk_dot.lnp != dotp->lnp || si.curbp->b_mroot.mk_dot.off != dotp->off) {
			Region region;
			bool truncated = false;

			if(getregion(&region,RegForceBegin) != Success)
				return rc.status;
			if(region.r_size > term.t_ncol * 2) {
				region.r_size = term.t_ncol * 2;
				truncated = true;
				}
			if(dsalloc(datp,region.r_size + 4) != 0)
				goto LibFail;
			regcpy(datp->d_str,&region);
			if(truncated)
				strcpy(datp->d_str + term.t_ncol * 2,"...");
			}
		else
			// Have zero-length region.
			dsetnull(datp);
		}
	else if(getsvar(datp,svp) != Success)
		return rc.status;

	// Have system variable value in *datp.  Convert it to display form.
	return (svp->sv_flags & V_Char) ? ctosf(destp,datp) : dtosfchk(destp,datp,NULL,flags | (svp->sv_id == sv_replacePat
	 || svp->sv_id == sv_searchPat ? CvtVizStrQ : CvtExpr));
LibFail:
	return librcset(Failure);
	}

// Set a variable -- "let" command (interactively only).  Evaluate value as an expression if n argument.  Return status.
int setvar(Datum *rp,int n,Datum **argpp) {
	VDesc vd;			// Variable descriptor.
	ushort delim;
	uint aflags,cflags;
	long lval;
	Datum *datp;
	DStrFab sf;

	// First get the variable to set.
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	if(terminp(datp,text51,ArgNil1,Term_C_SVar,NULL) != Success || datp->d_type == dat_nil)
			// "Assign variable"
		goto Retn;

	// Find variable.
	if(findvar(datp->d_str,&vd,OpCreate) != Success)
		goto Retn;

	// Error if read-only.
	if(vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_RdOnly))
		return rcset(Failure,RCTermAttr,text164,datp->d_str);
				// "Cannot modify read-only variable '~b%s~0'"

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
	if(dopenwith(&sf,rp,SFClear) != 0 || dputs(text297,&sf) != 0)
						// "Current value: "
		goto LibFail;
	if(vd.vd_type == VTyp_SVar) {
		if(vd.p.vd_svp->sv_flags & (V_GetKey | V_GetKeySeq)) {
			if(getsvar(datp,vd.p.vd_svp) != Success)
				goto Retn;
			if(dputf(&sf,"~#u%s~U",datp->d_str) != 0)
				goto LibFail;
			cflags |= Term_Attr;
			}
		else if(svtosf(&sf,vd.p.vd_svp,0) != Success)
			goto Retn;
		}
	else if(dtosfchk(&sf,vd.p.vd_uvp->uv_datp,NULL,CvtExpr | CvtForceArray) != Success)
		goto Retn;

	// Add "new value" type to prompt.
	if(dputs(text283,&sf) != 0)
			// ", new value"
		goto LibFail;
	if(n != INT_MIN) {
		if(dputs(text301,&sf) != 0)
				// " (expression)"
			goto LibFail;
		}
	else if(vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & (V_Char | V_GetKey | V_GetKeySeq))) {
		if(dputs(vd.p.vd_svp->sv_flags & V_Char ? text349 : text76,&sf) != 0)
						// " (char)"," (key)"
			goto LibFail;
		}
	if(dclose(&sf,sf_string) != 0)
LibFail:
		return librcset(Failure);

	// Get new value.
	TermInp ti = {NULL,delim,0,NULL};
	if(terminp(rp,rp->d_str,aflags,cflags,&ti) != Success)
		goto Retn;

	// Evaluate result as an expression if requested.  Type checking is done in putvar() so no need to do it here.
	if(n != INT_MIN) {
		if(execestmt(rp,rp->d_str,TokC_ComLine,NULL) != Success)
			goto Retn;
		}
	else if((rp->d_type & DStrMask) && (vd.vd_type == VTyp_GVar ||
	 (vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_Int))) && asc_long(rp->d_str,&lval,true))
		dsetint(lval,rp);

	// Set variable to value in rp and return.
#if MMDebug & Debug_Datum
	ddump(rp,"setvar(): Setting and returning value...");
	(void) putvar(rp,&vd);
	dumpvars();
#else
	(void) putvar(rp,&vd);
#endif
Retn:
	return rc.status;
	}

// Convert an array reference node to a VDesc object and check if referenced element exists.  If not, create it if "create" is
// true; otherwise, set an error.  Return status.
int aryget(ENode *np,VDesc *vdp,bool create) {

	vdp->vd_type = VTyp_ARef;
	vdp->i.vd_index = np->en_index;
	vdp->p.vd_aryp = awptr(np->en_rp)->aw_aryp;
	if(aget(vdp->p.vd_aryp,vdp->i.vd_index,create) == NULL)
		(void) librcset(Failure);
	return rc.status;
	}

// Increment or decrement a variable or array reference, given node pointer, "incr" flag, and "pre" flag.  Set node to result
// and return status.
int bumpvar(ENode *np,bool incr,bool pre) {
	VDesc vd;
	long lval;
	Datum *datp;

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
				// "Variable '~b%s~0' not an integer"
		}
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	if(vderefv(datp,&vd) != Success)			// Dereference variable...
		return rc.status;
	lval = datp->u.d_int + (incr ? 1 : -1);			// compute new value of variable...
	dsetint(pre ? lval : datp->u.d_int,np->en_rp);		// set result to pre or post value...
	dsetint(lval,datp);					// set new variable value in a datum object...
	return putvar(datp,&vd);				// and update variable.
	}

#if MMDebug & Debug_Datum
// Dump all user variables to the log file.
void dumpvars(void) {
	UVar *uvp;
	struct uvinfo {
		char *label;
		UVar **uheadpp;
		} *uvtp;
	static struct uvinfo uvtab[] = {
		{"GLOBAL",&gvarsheadp},
		{"LOCAL",&lvarsheadp},
		{NULL,NULL}};

	do {
		fprintf(logfile,"%s VARS\n",uvtp->label);
		if((uvp = *uvtp->uheadpp) != NULL) {
			do {
				ddump(uvp->uv_datp,uvp->uv_name);
				} while((uvp = uvp->uv_nextp) != NULL);
			}
		} while((++uvtp)->label != NULL);
	}
#endif

// Get next system variable name or description and store in report-control object.  If req is SHReqNext, set *namep to NULL if
// no items left; otherwise, pointer to its name.  Return status.
int nextSysVar(ShowCtrl *scp,ushort req,char **namep) {
	SVar *svp;

	// First call?
	if(scp->sc_itemp == NULL)
		scp->sc_itemp = (void *) (svp = sysvars);
	else {
		svp = (SVar *) scp->sc_itemp;
		if(req == SHReqNext)
			++svp;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; svp->sv_name != NULL; ++svp) {

				// Found variable... return its name.
				*namep = svp->sv_name;
				scp->sc_itemp = (void *) svp;
				goto Retn;
				}

			// End of list.
			*namep = NULL;
			break;
		case SHReqUsage:
			// Set variable name and description.
			if(dsetstr(svp->sv_name,&scp->sc_name) != 0)
				goto LibFail;
			scp->sc_desc = svp->sv_desc;
			*namep = svp->sv_name;
			break;
		default: // SHReqValue
			if(svp->sv_flags & (V_GetKey | V_GetKeySeq)) {
				Datum *datp;

				if(dnewtrk(&datp) != 0)
					goto LibFail;
				if(getsvar(datp,svp) == Success && dputf(&scp->sc_rpt,"~#u%s~U",datp->d_str) != 0)
LibFail:
					(void) librcset(Failure);
				}
			else
				(void) svtosf(&scp->sc_rpt,svp,CvtTermAttr);
		}
Retn:
	return rc.status;
	}

// Get next user variable name or description and store in report-control object.  If req is SHReqNext, set *namep to NULL if no
// items left; otherwise, pointer to its name.  Return status.
static int nextUserVar(ShowCtrl *scp,ushort req,char **namep,UVar *vheadp) {
	UVar *uvp;

	// First call?
	if(scp->sc_itemp == NULL)
		scp->sc_itemp = (void *) (uvp = vheadp);
	else {
		uvp = (UVar *) scp->sc_itemp;
		if(req == SHReqNext)
			uvp = uvp->uv_nextp;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; uvp != NULL; uvp = uvp->uv_nextp) {

				// Found variable... return its name.
				*namep = uvp->uv_name;
				scp->sc_itemp = (void *) uvp;
				goto Retn;
				}

			// End of list.
			*namep = NULL;
			break;
		case SHReqUsage:
			if(dsetstr(uvp->uv_name,&scp->sc_name) != 0)
				return librcset(Failure);
			*namep = uvp->uv_name;
			break;
		default: // SHReqValue
			if(dtosfchk(&scp->sc_rpt,uvp->uv_datp,NULL,CvtTermAttr | CvtExpr | CvtForceArray) != Success)
				return rc.status;
		}
Retn:
	return rc.status;
	}

// Get next global variable name or description via call to nextUserVar().
int nextGlobalVar(ShowCtrl *scp,ushort req,char **namep) {

	return nextUserVar(scp,req,namep,gvarsheadp);
	}

// Get next local variable name or description via call to nextUserVar().
int nextLocalVar(ShowCtrl *scp,ushort req,char **namep) {

	return nextUserVar(scp,req,namep,lvarsheadp);
	}

// Create formatted list of system and user variables via calls to "show" routines.  Return status.
int showVariables(Datum *rp,int n,Datum **argpp) {
	ShowCtrl sc;

	// Open control object.
	if(showopen(&sc,n,text292,argpp) != Success)
			// "variable"
		return rc.status;

	// Do system variables.
	if(showbuild(&sc,SHSepLine,text21,nextSysVar) != Success)
				// "system variable"
		return rc.status;

	// Do user variables.
	if(showbuild(&sc,SHNoDesc,text56,nextGlobalVar) == Success && showbuild(&sc,0,NULL,nextLocalVar) == Success)
				// "user variable"
		(void) showclose(rp,n,&sc);
	return rc.status;
	}
