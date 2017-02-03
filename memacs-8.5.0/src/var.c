// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// var.c	Routines dealing with variables for MightEMacs.

#include "os.h"
#include <ctype.h>
#include "std.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "file.h"
#include "main.h"
#include "search.h"

// Make selected global definitions local.
#define DATAvar
#include "lang.h"
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
			return (vdp->p.vd_svp->sv_flags & V_Int) != 0;
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
bool isident1(int c) {

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

// Return system variable name, given index.  Used in binary() calls.
static char *svarname(int i) {

	return sysvars[i].sv_name + 1;
	}

// Perform binary search given key string, table-fetch function, and table size.  Return index (>= 0) if found; otherwise, -1.
int binary(char *key,char *(*tval)(int i),int tsize) {
	uint l,u;		// Lower and upper limits of binary search.
	uint i;			// Current search index.
	int cresult;		// Result of comparison.

	// Set current search limit to entire list.
	l = 0;
	u = tsize - 1;

	// Loop until a match found or list shrinks to zero items.
	while(u >= l) {

		// Get the midpoint.
		i = (l + u) >> 1;

		// Do the comparison.
		if((cresult = strcmp(key,tval(i))) == 0)
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

// Place the list of characters considered "in a word" into rp.  Return status.
static int getwlist(Datum *rp) {
	DStrFab sf;
	char *str,*strz;

	if(dopenwith(&sf,rp,false) != 0)
		return drcset();

	// Build the string of characters in the result buffer.
	strz = (str = wordlist) + 256;
	do {
		if(*str && dputc(str - wordlist,&sf) != 0)
			return drcset();
		} while(++str < strz);

	if(dclose(&sf,sf_string) != 0)
		return drcset();

	return rc.status;
	}

// Replace the current line with the given text.  Return status.  (Used only for setting the $lineText system variable.)
static int putctext(char *iline) {

	if(allowedit(true) != Success)		// Don't allow if in read-only mode.
		return rc.status;

	// Delete the current line.
	curwp->w_face.wf_dot.off = 0;		// Start at the beginning of the line.
	if(kdctext(1,-1,NULL) != Success)	// Put it in the kill buffer.
		return rc.status;

	// Insert the new line.
	if(linstr(iline) != Success)
		return rc.status;
	if(lnewline() == Success)
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

// Encode the current keyboard macro into dest in string form using ektos().  Return status.
static int kmtos(Datum *destp) {

	// Recording a keyboard macro?
	if(kmacro.km_state == KMRecord) {
		clearKeyMacro(true);
		return rcset(Failure,0,text338);
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
		if(dopenwith(&dest,destp,false) != 0)
			return drcset();
		kmp = kmacro.km_buf;
		do {
			ektos(*kmp,wkbuf + 1);
			if(dputs(wkbuf,&dest) != 0)
				return drcset();
			} while(++kmp < kmacro.km_endp);
		if(dclose(&dest,sf_string) != 0)
			return drcset();
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
		if(!(svp->sv_flags & V_Int)) {
			str = svp->u.sv_str;
			goto Kopy;
			}
		dsetint(svp->u.sv_int,rp);
		goto Retn;
		}
	else {
		switch(vid = svp->sv_id) {
			case sv_ArgVector:
				if(scriptrun == NULL)
					dsetnil(rp);
				else if(datcpy(rp,scriptrun->margp) != 0)
					return drcset();
				break;
			case sv_BufInpDelim:
				str = curbp->b_inpdelim;
				goto Kopy;
			case sv_Date:
				str = timeset();
				goto Kopy;
#if TypeAhead
			case sv_KeyPending:
				{int count;

				if(typahead(&count) != Success)
					return rc.status;
				dsetbool(count > 0,rp);
				}
				break;
#endif
			case sv_LineLen:
				dsetint((long) lused(curwp->w_face.wf_dot.lnp),rp);
				break;
			case sv_Match:
				str = fixnull(lastMatch->d_str);
				goto Kopy;
			case sv_RegionText:
				{Region region;

				// Get the region limits.
#if 0
				if(getregion(&region,NULL) == Success)
					(void) regdatcpy(rp,&region);
#else
				if(getregion(&region,NULL) != Success)
					return rc.status;

				// Preallocate a string and copy.
				if(dsalloc(rp,region.r_size + 1) != 0)
					return drcset();
				regcpy(rp->d_str,&region);
#endif
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
			case sv_TermCols:
				dsetint((long) term.t_ncol,rp);
				break;
			case sv_TermRows:
				dsetint((long) term.t_nrow,rp);
				break;
			case sv_WindCount:
				dsetint((long) wincount(),rp);
				break;
			case sv_autoSave:
				dsetint((long) gasave,rp);
				break;
			case sv_bufFile:
				if((str = curbp->b_fname) != NULL)
					goto Kopy;
				dsetnil(rp);
				break;
			case sv_bufFlags:
				dsetint((long) curbp->b_flags,rp);
				break;
			case sv_bufLineNum:
				dsetint(getlinenum(curbp,curwp->w_face.wf_dot.lnp),rp);
				break;
			case sv_bufName:
				str = curbp->b_bname;
				goto Kopy;
			case sv_bufModes:
				dsetint((long) curbp->b_modes,rp);
				break;
			case sv_defModes:
				dsetint((long) modetab[MdRec_Default].flags,rp);
				break;
#if Color
			case sv_desktopColor:
				str = (char *) cname[deskcolor];
				goto Kopy;
#endif
			case sv_execPath:
				str = execpath;
				goto Kopy;
			case sv_fencePause:
				dsetint((long) fencepause,rp);
				break;
			case sv_globalModes:
				dsetint((long) modetab[MdRec_Global].flags,rp);
				break;
			case sv_hardTabSize:
				dsetint((long) htabsize,rp);
				break;
			case sv_horzJump:
				dsetint((long) hjump,rp);
				break;
			case sv_horzScrollCol:
				dsetint((long) curwp->w_face.wf_fcol,rp);
				break;
			case sv_inpDelim:
				str = fi.inpdelim;
				goto Kopy;
			case sv_keyMacro:
				(void) kmtos(rp);
				break;
			case sv_lastKeySeq:
				ektos(kentry.lastkseq,str = wkbuf);
				goto Kopy;
			case sv_lineChar:
				{Dot *dotp = &curwp->w_face.wf_dot;
				dsetchr(lused(dotp->lnp) == dotp->off ? '\n' : lgetc(dotp->lnp,dotp->off),rp);
				}
				break;
			case sv_lineCol:
				dsetint((long) getccol(),rp);
				break;
			case sv_lineOffset:
				dsetint((long) curwp->w_face.wf_dot.off,rp);
				break;
			case sv_lineText:
				{Line *lnp = curwp->w_face.wf_dot.lnp;
				if(dsetsubstr(ltext(lnp),lused(lnp),rp) != 0)
					(void) drcset();
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
			case sv_otpDelim:
				str = fi.otpdelim;
				goto Kopy;
			case sv_pageOverlap:
				dsetint((long) overlap,rp);
				break;
#if Color
			case sv_palette:
				str = palstr;
				goto Kopy;
#endif
			case sv_randNumSeed:
				dsetint(randseed & LONG_MAX,rp);
				break;
			case sv_replacePat:
				str = srch.m.rpat;
				goto Kopy;
			case sv_screenNum:
				dsetint((long) cursp->s_num,rp);
				break;
			case sv_searchPat:
				{char patbuf[srch.m.patlen + OptCh_N + 1];
				if(dsetstr(mkpat(patbuf,&srch.m),rp) != 0)
					(void) drcset();
				}
				break;
			case sv_searchDelim:
				ektos(srch.sdelim,str = wkbuf);
				goto Kopy;
			case sv_showModes:
				dsetint((long) modetab[MdRec_Show].flags,rp);
				break;
			case sv_softTabSize:
				dsetint((long) stabsize,rp);
				break;
			case sv_travJump:
				dsetint((long) tjump,rp);
				break;
			case sv_vertJump:
				dsetint((long) vjump,rp);
				break;
			case sv_windLineNum:
				dsetint((long) getwpos(curwp),rp);
				break;
			case sv_windNum:
				dsetint((long) getcwnum(),rp);
				break;
			case sv_windSize:
				dsetint((long) curwp->w_nrows,rp);
				break;
			case sv_wordChars:
				(void) getwlist(rp);
				break;
			case sv_workDir:
				(void) getwkdir(&str,false);
				goto Kopy;
			case sv_wrapCol:
				dsetint((long) wrapcol,rp);
				break;
			default:
				// Never should get here.
				return rcset(FatalError,0,text3,"getsvar",vid,svp->sv_name);
						// "%s(): Unknown id %d for var '%s'!"
			}
		goto Retn;
		}
Kopy:
	if(dsetstr(str,rp) != 0)
		(void) drcset();
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
	int delim;

	// Make sure a keyboard macro is not currently being recorded or played.
	clearKeyMacro(false);
	if(kmacro.km_state != KMStop) {
		if(kmacro.km_state == KMRecord)
			curwp->w_flags |= WFMode;
		kmacro.km_state = KMStop;
		return rcset(Failure,0,text338);
			// "Cannot access '$keyMacro' from a keyboard macro, cancelled"
		}

	// Get delimiter (first character) and parse string.
	if((delim = *estr++) != '\0' && *estr != '\0') {
		ushort c,ek;
		enum cfid id;
		CFABPtr cfab;
		bool last;
		Datum *datp;

		if(dnewtrk(&datp) != 0)
			return drcset();

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
				cfab.u.p_cfp = cftab + id;
				c = getpentry(&cfab)->k_code;
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
	return datcpy(destp,srcp) != 0 ? drcset() : rc.status;
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
			switch(svp->sv_id) {
				case sv_ArgVector:
				case sv_BufFlagActive:
				case sv_BufFlagChanged:
				case sv_BufFlagHidden:
				case sv_BufFlagMacro:
				case sv_BufFlagNarrowed:
				case sv_BufInpDelim:
				case sv_Date:
#if TypeAhead
				case sv_KeyPending:
#endif
				case sv_LineLen:
				case sv_Match:
				case sv_ModeAutoSave:
				case sv_ModeBackup:
				case sv_ModeC:
				case sv_ModeClobber:
				case sv_ModeColDisp:
				case sv_ModeEsc8Bit:
				case sv_ModeExact:
				case sv_ModeExtraIndent:
				case sv_ModeHorzScroll:
				case sv_ModeLineDisp:
				case sv_ModeMEMacs:
				case sv_ModeMsgDisp:
				case sv_ModeNoUpdate:
				case sv_ModeOver:
				case sv_ModePerl:
				case sv_ModeReadOnly:
				case sv_ModeRegexp:
				case sv_ModeReplace:
				case sv_ModeRuby:
				case sv_ModeSafeSave:
				case sv_ModeShell:
				case sv_ModeWorkDir:
				case sv_ModeWrap:
				case sv_RegionText:
				case sv_ReturnMsg:
				case sv_RunFile:
				case sv_RunName:
				case sv_TermCols:
				case sv_TermRows:
				case sv_WindCount:
					return rcset(Failure,0,text164,svp->sv_name);
							// "Cannot modify read-only variable '%s'"
				default:
					;	// Do nothing.
				}

			// Not read-only.  Check for legal value types.
			if(svp->sv_flags & V_Int) {
				if(!intval(datp))
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
				return (dopenwith(&msg,&rc.msg,true) != 0 ||
				 dputf(&msg,text334,str1) != 0 || dclose(&msg,sf_string) != 0) ? drcset() : rc.status;
						// ", setting variable '%s'"
				}

			// Do specific action for referenced (mutable) variable.
			if(dnewtrk(&dsinkp) != 0)
				return drcset();
			switch(svp->sv_id) {
				case sv_autoSave:
					{int n;

					if(datp->u.d_int < 0) {
						i = 0;
						goto ERange;
						}
					if((n = (datp->u.d_int > INT_MAX ? INT_MAX : datp->u.d_int)) == 0) {

						// ASave count set to zero... turn off global mode and clear counter.
						if(modetab[MdRec_Global].flags & MdASave) {
							modetab[MdRec_Global].flags &= ~MdASave;
							upmode(NULL);
							}
						gasave = gacount = 0;
						}
					else {
						int diff = n - gasave;
						if(diff != 0) {

							// New count > 0.  Adjust counter accordingly.
							gasave = n;
							if(diff > 0) {
								if((long) gacount + diff > INT_MAX)
									gacount = INT_MAX;
								else
									gacount += diff;
								}
							else if((gacount += diff) <= 0)
								gacount = 1;
							}
						}
					}
					break;
				case sv_bufFile:
					str1 = "0 => setBufFile ";
					goto XeqCmd;
				case sv_bufFlags:
					curbp->b_flags = (curbp->b_flags & ~(BFChgd | BFHidden)) |
					 (datp->u.d_int & (BFChgd | BFHidden));
					if(datp->u.d_int & BFChgd)
						lchange(curbp,WFMode);
					break;
				case sv_bufLineNum:
					(void) goline(dsinkp,INT_MIN,datp->u.d_int);
					break;
				case sv_bufModes:
					(void) adjustmode(NULL,1,3,datp);
					break;
				case sv_bufName:
					str1 = "setBufName ";
					goto XeqCmd;
				case sv_defModes:
					(void) adjustmode(NULL,1,MdRec_Default,datp);
					break;
#if Color
				case sv_desktopColor:
					if((i = lookup_color(mkupper(rp,datp->d_str))) == -1)
						return rcset(Failure,0,text501,datp->d_str);
								// "No such color '%s'"
					deskcolor = i;
					(void) refreshScreens();
					break;
#endif
				case sv_execPath:
					(void) setpath(datp->d_str,false);
					break;
				case sv_fencePause:
					if(datp->u.d_int < 0)
						return rcset(Failure,0,text39,text119,(int) datp->u.d_int,0);
							// "%s (%d) must be %d or greater","Pause duration"
					fencepause = datp->u.d_int;
					break;
				case sv_globalModes:
					(void) adjustmode(NULL,1,MdRec_Global,datp);
					break;
				case sv_hardTabSize:
					if(settab((int) datp->u.d_int,true) != Success)
						return rc.status;
					uphard();
					break;
				case sv_horzJump:
					if((hjump = datp->u.d_int) < 0)
						hjump = 0;
					else if(hjump > JumpMax)
						hjump = JumpMax;
					if((hjumpcols = hjump * term.t_ncol / 100) == 0)
						hjumpcols = 1;
					break;
				case sv_horzScrollCol:
					curwp->w_face.wf_fcol = datp->u.d_int < 0 ? 0 : datp->u.d_int;
					curwp->w_flags |= WFHard | WFMode;
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
						KeyDesc *kdp = getbind(ek);
						if(kdp != NULL) {
							CFABPtr *cfabp = &kdp->k_cfab;
							if(cfabp->p_type == PtrPseudo && (cfabp->u.p_cfp->cf_aflags & CFPrefix))
								return rcset(Failure,0,text373,svp->sv_name);
										// "Illegal value for '%s' variable"
							}
						kentry.lastkseq = ek;
						kentry.uselast = true;
						}
					break;
				case sv_lineChar:
					// Replace character at point with a string.
					if(ldelete(1L,0) != Success)
						return rcset(Failure,0,text142,curbp->b_bname);
							// "Cannot change a character past end of buffer '%s'"
					(void) linstr(datp->d_str);
					break;
				case sv_lineCol:
					(void) setccol(datp->u.d_int);
					break;
				case sv_lineOffset:
					{int llen = lused(curwp->w_face.wf_dot.lnp);
					int loff = (datp->u.d_int < 0) ? llen + datp->u.d_int : datp->u.d_int;
					if(loff < 0 || loff > llen)
						return rcset(Failure,0,text224,datp->u.d_int);
							// "Line offset value %ld out of range"
					curwp->w_face.wf_dot.off = loff;
					curwp->w_flags |= WFMove;
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
ERange:
						return rcset(Failure,0,text111,svp->sv_name,i);
							// "'%s' value must be %d or greater"
						}
					maxmacdepth = datp->u.d_int;
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
					overlap = datp->u.d_int;
					break;
#if Color
				case sv_palette:
					str1 = palstr;
					if(spal(datp->d_str) == Success)
						(void) chkcpy(&str1,datp->d_str,NPalette + 1,text502);
									// "Palette"
					break;
#endif
				case sv_randNumSeed:

					// Force seed to be between 1 and 2**31.
					if((randseed = (ulong) datp->u.d_int) == 0)
						randseed = seedinit();
					break;
				case sv_replacePat:
					(void) newrpat(datp->d_str,&srch.m);
					break;
				case sv_screenNum:
					(void) nextScreen(dsinkp,datp->u.d_int,NULL);
					break;
				case sv_searchDelim:
					if(stoek(datp->d_str,&ek) != Success)
						return rc.status;
					if(ek & KeySeq) {
						char keybuf[16];

						return rcset(Failure,0,text341,ektos(ek,keybuf),text343);
							// "Cannot use key sequence '%s' as %s delimiter","search"
						}
					srch.sdelim = ek;
					break;
				case sv_searchPat:
					(void) newspat(datp->d_str,&srch.m,NULL);
					break;
				case sv_showModes:
					(void) adjustmode(NULL,1,MdRec_Show,datp);
					break;
				case sv_softTabSize:
					if(settab((int) datp->u.d_int,false) != Success)
						return rc.status;
					uphard();
					break;
				case sv_travJump:
					if((tjump = datp->u.d_int) < 4)
						tjump = 4;
					else if(tjump > term.t_ncol / 4 - 1)
						tjump = term.t_ncol / 4 - 1;
					break;
				case sv_vertJump:
					if((vjump = datp->u.d_int) < VJumpMin)
						vjump = 0;
					else if(vjump > JumpMax)
						vjump = JumpMax;
					break;
				case sv_windLineNum:
					(void) forwLine(dsinkp,datp->u.d_int - getwpos(curwp),NULL);
					break;
				case sv_windNum:
					(void) nextWind(dsinkp,datp->u.d_int,NULL);
					break;
				case sv_windSize:
					(void) resizeWind(dsinkp,datp->u.d_int,NULL);
					break;
				case sv_wordChars:
					(void) setwlist(disnull(datp) ? wordlistd : datp->d_str);
					break;
				case sv_workDir:
					str1 = "chDir ";
XeqCmd:
					{DStrFab cmd;
					(void) runcmd(dsinkp,&cmd,str1,datp->d_str,true);
					}
					break;
				case sv_wrapCol:
					(void) execCF(dsinkp,datp->u.d_int,cftab + cf_setWrapCol,0,0);
					break;
				default:
					// Never should get here.
					return rcset(FatalError,0,text179,myname,svp->sv_id,svp->sv_name);
						// "%s(): Unknown id %d for variable '%s'!"
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
#if 0
		case VTyp_ARef:
#endif
		default:	// VTyp_ARef
			{Datum *elp = aget(vdp->p.vd_aryp,vdp->i.vd_index,false);
			if(elp == NULL)
				return drcset();
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
	}

// Create local or global user variable, given name and descriptor pointer.  Return status.
static int uvarnew(char *var,VDesc *vdp) {
	UVar *uvp;
	char *str;
	char *name = var + (*var == TokC_GVar ? 1 : 0);

	// Invalid length?
	if(*var == '\0' || *name == '\0' || strlen(var) > NVarName)
		return rcset(Failure,0,text280,text279,NVarName);
			// "%s name cannot be null or exceed %d characters","Variable"

	// Valid variable name?
	str = name;
	if(getident(&str,NULL) != s_ident || *str != '\0')
		(void) rcset(Failure,0,text286,name);
			// "Invalid identifier '%s'"

	// Same name as an existing command, function, alias, or macro?
	if(cfabsearch(var,NULL,PtrCFAM) == 0)
		return rcset(Failure,0,text165,var);
			// "Name '%s' already in use"

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

// Find a named variable's type and id.  If op is OpCreate: (1), create user variable if non-existent and either (i), variable
// is global; or (ii), variable is local and executing a buffer; and (2), return status.  If op is OpQuery: return true if
// variable is found; otherwise, false.  If op is OpDelete: return status if variable is found; otherwise, error.  In all cases,
// store results in *vdp (if vdp not NULL) and variable is found.
int findvar(char *name,VDesc *vdp,int op) {
	UVar *uvp;
	int i;
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
				if((i = binary(name + 1,svarname,NSVars)) >= 0) {
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
				return drcset();
		}

	// Copy value.
	return datcpy(datp,valp) != 0 ? drcset() : rc.status;
	}

// Derefence a variable, given name, and save variable's value in datp.  Return status.
int vderefn(Datum *datp,char *name) {
	VDesc vd;

	// Find and dereference variable.
	if(findvar(name,&vd,OpDelete) == Success)
		(void) vderefv(datp,&vd);

	return rc.status;
	}

// Set a variable -- "let" command (interactively only).  Evaluate value as an expression if n arg.  Return status.
int setvar(Datum *rp,int n,Datum **argpp) {
	VDesc vd;			// Variable descriptor.
	char *prmt;
	uint delim;
	uint aflags,cflags;
	long lval;
	Datum *datp;

	// First get the variable to set.
	if(dnewtrk(&datp) != 0)
		return drcset();
	if(terminp(datp,text51,NULL,RtnKey,0,0,Term_C_SVar) != Success || datp->d_type == dat_nil)
			// "Assign variable"
		return rc.status;

	// Find variable...
	if(findvar(datp->d_str,&vd,OpCreate) != Success)
		return rc.status;

	// get the value...
	if(n == INT_MIN) {
		delim = Ctrl | ((vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_EscDelim)) ? '[' : 'M');
		prmt = text53;
			// "Value"
		aflags = cflags = 0;
		}
	else {
		delim = RtnKey;
		prmt = text301;
			// "Expression"
		aflags = CFNotNull1;
		cflags = Term_Eval;
		}
	if(terminp(rp,prmt,NULL,delim,0,aflags,cflags) != Success)
		return rc.status;

	// and set it.
	if(n == INT_MIN && (rp->d_type & DStrMask) && (vd.vd_type == VTyp_GVar ||
	 (vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_Int))) && asc_long(rp->d_str,&lval,true))
		dsetint(lval,rp);
#if MMDebug & Debug_Datum
ddump(rp,"setvar(): Setting and returning value...");
	(void) putvar(rp,&vd);
	dumpvars();
	return rc.status;
#else
	return putvar(rp,&vd);
#endif
	}

// Convert an array reference node to a VDesc object and check if referenced element exists.  If not, create it if "create" is
// true; otherwise, set an error.  Return status.
int aryget(ENode *np,VDesc *vdp,bool create) {

	vdp->vd_type = VTyp_ARef;
	vdp->i.vd_index = np->en_index;
	vdp->p.vd_aryp = awptr(np->en_rp)->aw_aryp;
	if(aget(vdp->p.vd_aryp,vdp->i.vd_index,create) == NULL)
		(void) drcset();
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
			return rcset(Failure,0,text212,np->en_rp->d_str);
				// "Variable '%s' not an integer"
		}
	if(dnewtrk(&datp) != 0)
		return drcset();
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
	struct {
		char *label;
		UVar **uheadpp;
		} uvtab[] = {
			{"GLOBAL",&gvarsheadp},
			{"LOCAL",&lvarsheadp},
			{NULL,NULL}},
		*uvtp = uvtab;

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

// List all the system constants, system variables, and user variables and their values.  If default n, make
// full list; otherwise, get a match string and make partial list of variable names that contain it, ignoring case.  Render
// buffer and return status.
int showVariables(Datum *rp,int n,Datum **argpp) {
	Buffer *vlistp;
	SVar *svp;
	bool needBreak,skipLine;
	DStrFab rpt;
	Datum *mstrp = NULL;		// Match string.
	Datum *datp;
	char wkbuf[52];
	WindFace *wfp = &curwp->w_face;
	UVar *uvp,***uvppp,**uv[] = {&gvarsheadp,&lvarsheadp,NULL};

	// If not default n, get match string.
	if(n != INT_MIN) {
		if(dnewtrk(&mstrp) != 0)
			return drcset();
		if(apropos(mstrp,text292,argpp) != Success)
				// "variable"
			return rc.status;
		}

	// Get a buffer and open a string-fab object.
	if(sysbuf(text56,&vlistp) != Success)
			// "VariableList"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	needBreak = skipLine = false;

	// Build the system variable list.
	if(dnewtrk(&datp) != 0)
		return drcset();
	svp = sysvars;
	do {
		// Skip if an apropos and system variable name doesn't contain the search string.
		if(mstrp != NULL && strcasestr(svp->sv_name,mstrp->d_str) == NULL)
			continue;

		// Begin with the system variable name.
		strcpy(wkbuf,svp->sv_name);
		if(!skipLine && is_lower(svp->sv_name[1])) {
			if(needBreak && dputc('\n',&rpt) != 0)
				return drcset();
			skipLine = true;
			}
		if(needBreak && dputc('\n',&rpt) != 0)
			return drcset();
		pad(wkbuf,19);
		if(dputs(wkbuf,&rpt) != 0)
			return drcset();
		needBreak = true;

		// Add in the description.
		pad(strcpy(wkbuf,(svp->sv_flags & V_Mode) ? ((ModeSpec *) svp->sv_desc)->desc : svp->sv_desc),50);
		if(dputs(wkbuf,&rpt) != 0)
			return drcset();

		// Add in the value.  Skip $RegionText if no region defined or empty; use single-quote form for $replacePat and
		// $searchPat.
		if(svp->sv_id == sv_RegionText) {
			if(curbp->b_mroot.mk_dot.lnp != wfp->wf_dot.lnp || curbp->b_mroot.mk_dot.off != wfp->wf_dot.off) {
				Region region;
				bool truncated = false;

				// Cap size of region in case it's huge -- it will be truncated when displayed anyway.
				if(getregion(&region,NULL) != Success)
					return rc.status;
				if(region.r_size > term.t_ncol * 4) {
					region.r_size = term.t_ncol * 4;
					truncated = true;
					}
				if(dsalloc(datp,region.r_size + 4) != 0)
					return drcset();
				regcpy(datp->d_str,&region);
				if(truncated)
					strcpy(datp->d_str + term.t_ncol * 4,"...");
				}
			else
				dsetnull(datp);
			}
		else if(getsvar(datp,svp) != Success)
			return rc.status;
		if(dtosfc(&rpt,datp,NULL,(svp->sv_id == sv_replacePat || svp->sv_id == sv_searchPat) ? CvtVizStrQ :
		 CvtExpr) != Success)
			return rc.status;
		} while((++svp)->sv_name != NULL);

	// Build the user (global and local) variable list.
	uvppp = uv;
	do {
		if((uvp = **uvppp) != NULL) {
			if(needBreak && dputc('\n',&rpt) != 0)
				return drcset();
			do {
				// Add in the user variable name.
				strcpy(wkbuf,uvp->uv_name);

				// Skip if an apropos and user variable name doesn't contain the search string.
				if(mstrp != NULL && strcasestr(wkbuf,mstrp->d_str) == NULL)
					continue;
				if(needBreak && dputc('\n',&rpt) != 0)
					return drcset();
				pad(wkbuf,19);
				if(dputs(wkbuf,&rpt) != 0)
					return drcset();
				needBreak = true;

				// Add in the value.
				if(dtosfc(&rpt,uvp->uv_datp,NULL,CvtExpr | CvtForceArray) != Success)
					return rc.status;
				} while((uvp = uvp->uv_nextp) != NULL);
			}
		} while(*++uvppp != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(!disnull(rpt.sf_datp) && bappend(vlistp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display the list.
	return render(rp,n < 0 ? -2 : n,vlistp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}
