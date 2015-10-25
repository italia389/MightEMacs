// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// eval.c	Expresion evaluation library routines for MightEMacs.

#include "os.h"
#include <errno.h>
#include <ctype.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"
#include "evar.h"

// Definitions for strfmt().
#define FMTBUFSZ		32	// Size of temporary buffer.

// Flags.
#define FMTleft		0x0001		// Left-justify.
#define	FMTplus		0x0002		// Force plus (+) sign.
#define	FMTspc		0x0004		// Use ' ' for plus sign.
#define FMThash		0x0008		// Alternate conversion.
#define	FMTlong		0x0010		// 'l' flag.
#define FMT0pad		0x0020		// '0' flag.
#define FMTprec		0x0040		// Precision was specified.
#define FMTxuc		0x0080		// Use upper case hex letters.

// Return a value object as a logical (Boolean) value.
bool vistrue(Value *vp) {

	// Check for numeric truth (!= 0).
	if(vp->v_type == VALINT)
		return vp->u.v_int != 0;

	// Check for null string (which is false) and logical false values (false and nil).  All other strings are true.
	return !visnull(vp) && !vistfn(vp,VFALSE) && !vistfn(vp,VNIL);
	}

// Convert numeric logical to string logical.  Store result in dest and return status.
int ltos(Value *destp,int val) {

	return vsetstr(val ? val_true : val_false,destp) != 0 ? vrcset() : rc.status;
	}

// Return true if given string is nil; otherwise, false.
bool isnil(char *strp) {

	return strcmp(strp,val_nil) == 0;
	}

// Set destp to nil and return it.
char *nilcpy(char *destp) {

	return strcpy(destp,val_nil);
	}

// Return nil if given pointer is NULL; otherwise, the string.
char *defnil(char *strp) {

	return (strp == NULL) ? val_nil : strp;
	}

// Copy string from srcp to destp (an active string list), adding a double quote (") at beginning and end (if full is true) and
// escaping all control characters, backslashes, and characters that are escaped by parsesym().  Return status.
int quote(StrList *destp,char *srcp,bool full) {
	int c;
	bool ischar;
	char *strp;
	char wkbuf[8];

	if(full) {
		c = '"';
		goto litchar;
		}

	while((c = *srcp++) != '\0') {
		ischar = false;
		switch(c) {
			case '"':				// Double quote
				if(!full)
					goto litchar;
				strp = "\\\""; break;
			case '\\':				// Backslash
				strp = "\\\\"; break;
			case '\r':				// CR
				strp = "\\r"; break;
			case '\n':				// NL
				strp = "\\n"; break;
			case '\t':				// Tab
				strp = "\\t"; break;
			case '\b':				// Backspace
				strp = "\\b"; break;
			case '\f':				// Form feed
				strp = "\\f"; break;
			case 033:				// Escape
				strp = "\\e"; break;
			default:
				if(c < ' ' || c >= 0x7f)	// Non-printable character.
					sprintf(strp = wkbuf,"\\%.3o",c);
				else				// Literal character.
litchar:
					ischar = true;
			}

		if((ischar ? vputc(c,destp) : vputs(strp,destp)) != 0)
			return vrcset();
		}
	if(full && vputc('"',destp) != 0)
		(void) vrcset();

	return rc.status;
	}

// Force NULL pointer to null string.
char *fixnull(char *s) {

	return (s == NULL) ? "" : s;
	}

// Set hard or soft tab size and do range check.
int settab(int size,bool hard) {

	// Check if new tab size is valid.
	if((size != 0 || hard) && (size < 2 || size > MAXTAB))
		return rcset(FAILURE,0,text256,hard ? text49 : text50,size,MAXTAB);
			// "%s tab size %ld must be between 2 and %d","Hard","Soft"

	// Set new size.
	if(hard)
		htabsize = size;
	else {
		stabsize = size;
		(void) rcset(SUCCESS,0,text332,size);
				// "Soft tab size set to %d"
		}

	return rc.status;
	}

// Find pattern within source.  Find rightmost match if "rightmost" is true.  Set rp to 0-origin match position or nil if no
// match.  Return status.
static int sindex(Value *rp,Value *srcp,Value *patp,bool rightmost) {
	char *srcp1,*srcp2,*srcpz,*patp1;
	size_t len;
	int incr;

	// Scan through the source string.
	if(*(srcp1 = srcp->v_strp) != '\0') {
		len = strlen(srcp1);
		if(rightmost) {
			srcpz = srcp1 - 1;
			srcp1 += len + (incr = -1);
			}
		else {
			srcpz = srcp1 + len;
			incr = 1;
			}

		do {
			// Scan through the pattern.
			patp1 = patp->v_strp;
			srcp2 = srcp1;
			while(*patp1) {
				if(*patp1 != *srcp2)
					break;
				++patp1;
				++srcp2;
				}

			// Was it a match?
			if(*patp1 == '\0') {
				vsetint((long) (srcp1 - srcp->v_strp),rp);
				goto retn;
				}
			} while((srcp1 += incr) != srcpz);
		}

	// No match.
	(void) vnilmm(rp);
retn:
	return rc.status;
	}

// Strip whitespace off the beginning (op == -1), the end (op == 1), or both ends (op == 0) of a string.
#if 1
char *stripstr(char *srcp,int op) {
	char *srcpz;

	// Trim beginning, if applicable ...
	if(op <= 0)
		srcp = nonwhite(srcp);

	// trim end, if applicable ...
	if(op >= 0) {
		srcpz = strchr(srcp,'\0');
		while(srcpz-- > srcp)
			if(*srcpz != ' ' && *srcpz != '\t')
				break;
		srcpz[1] = '\0';
		}

	// and return result.
	return srcp;
	}
#else
static int stripstr(Value *rp,Value *vp,int op) {
	char *srcp,*srcpz;

	// Trim beginning, if applicable ...
	srcp = op > 0 ? vp->v_strp : nonwhite(vp->v_strp);

	// trim end, if applicable ...
	if(op >= 0) {
		srcpz = strchr(srcp,'\0');
		while(srcpz-- > srcp)
			if(*srcpz != ' ' && *srcpz != '\t')
				break;
		srcpz[1] = '\0';
		}

	// and return result.
	return vsetstr(srcp,rp) != 0 ? vrcset() : rc.status;
	}
#endif

// Substitute first occurrence (or all if n > 0) of s2 in s1 with s3 and store results in np->en_rp.  Return status.
static int strsub(Value *rp,int n,char *s1,char *s2,char *s3) {
	int rcode;
	StrList dest;

	// Return source string if s1 or s2 is empty.
	if(*s1 == '\0' || *s2 == '\0')
		rcode = vsetstr(s1,rp);
	else if((rcode = vopen(&dest,rp,false)) == 0) {
		int srclen,s2len,s3len;
		char *s;

		s2len = strlen(s2);
		s3len = strlen(s3);

		for(;;) {
			// Find next occurrence.
			if((s = strstr(s1,s2)) == NULL)
				break;

			// Compute offset and copy prefix.
			if((srclen = s - s1) > 0 && vputfs(s1,srclen,&dest) != 0)
				return vrcset();
			s1 = s + s2len;

			// Copy substitution string.
			if(vputfs(s3,s3len,&dest) != 0)
				return vrcset();

			// Bail out unless n > 0.
			if(n <= 0)
				break;
			}

		// Copy remainder, if any.
		if((srclen = strlen(s1)) > 0 && vputfs(s1,srclen,&dest) != 0)
			return vrcset();
		rcode = vclose(&dest);
		}

	return rcode == 0 ? rc.status : vrcset();
	}

// Pad a string, given result pointer, source pointer, and length.  Return status.
static int vpad(Value *rp,Value *srcp,long len) {
	long lval;

	lval = labs(len);
	if((lval -= strlen(srcp->v_strp)) <= 0)
		vxfer(rp,srcp);
	else {
		Value *vp;

		if(vnew(&vp,false) != 0 || vsalloc(vp,labs(len) + 1) != 0)
			(void) vrcset();
		else {
			char *strp = vp->v_strp;

			if(len < 0) {
				do {
					*strp++ = ' ';
					} while(--lval > 0);
				}
			strp = stpcpy(strp,srcp->v_strp);
			if(len > 0) {
				do {
					*strp++ = ' ';
					} while(--lval > 0);
				*strp = '\0';
				}
			vxfer(rp,vp);
			}
		}
	return rc.status;
	}

// Expand character ranges and escaped characters (if any) in a string.  Return status.
int strexpand(StrList *slp,Value *vp) {
	int c1,c2;
	char *strp;

	if(vopen(slp,NULL,false) != 0)
		return vrcset();
	strp = vp->v_strp;
	do {
		switch(c1 = *strp) {
			case '-':
				if(strp == vp->v_strp || (c2 = strp[1]) == '\0')
					goto litchar;
				if(c2 < (c1 = strp[-1]))
					return rcset(FAILURE,0,text2,strp - 1);
						// "Invalid character range '%.3s' in tr string"
				while(++c1 <= c2)
					if(vputc(c1,slp) != 0)
						return vrcset();
				++strp;
				break;
			case '\\':
				if(strp[1] != '\0')
					c1 = *++strp;
				// Fall through.
			default:
litchar:
				if(vputc(c1,slp) != 0)
					return vrcset();
			}
		} while(*++strp != '\0');

	return vclose(slp) != 0 ? vrcset() : rc.status;
	}

// Prepare tr from and to strings.  Return status.
static int trprep(Value *xfromp,Value *xtop) {
	StrList sl;

	// Expand "from" string.
	if(strexpand(&sl,xfromp) != SUCCESS)
		return rc.status;
	vxfer(xfromp,sl.sl_vp);

	// Expand "to" string.
	if(vistfn(xtop,VNIL))
		vnull(xtop);
	else if(*xtop->v_strp != '\0') {
		int lenfrom,lento;

		if(strexpand(&sl,xtop) != SUCCESS)
			return rc.status;
		vxfer(xtop,sl.sl_vp);

		if((lenfrom = strlen(xfromp->v_strp)) > (lento = strlen(xtop->v_strp))) {
			int c = xtop->v_strp[lento - 1];
			int n = lenfrom - lento;

			if(vopen(&sl,xtop,true) != 0)
				return vrcset();
			do {
				if(vputc(c,&sl) != 0)
					return vrcset();
				} while(--n > 0);
			if(vclose(&sl) != 0)
				return vrcset();
			}
		}
	return rc.status;
	}

// Translate a string, given result pointer, source pointer, translate-from string, and translate-to string.  Return status.
// The translate-to string may be null, in which case all translate-from characters are deleted from the result.  It may also
// be shorter than the translate-from string, in which case it is padded to the same length with its last character.
static int tr(Value *rp,Value *srcp,Value *xfromp,Value *xtop) {
	int lento;
	char *xfp;
	char *strp;
	StrList result;

	// Validate arguments.
	if(strlen(xfromp->v_strp) == 0)
		return rcset(FAILURE,0,text187,text328);
				// "%s cannot be null","tr \"from\" string"
	if(trprep(xfromp,xtop) != SUCCESS)
		return rc.status;

	// Scan source string.
	if(vopen(&result,rp,false) != 0)
		return vrcset();
	strp = srcp->v_strp;
	lento = strlen(xtop->v_strp);
	while(*strp != '\0') {

		// Scan lookup table for a match.
		xfp = xfromp->v_strp;
		do {
			if(*strp == *xfp) {
				if(lento > 0 && vputc(xtop->v_strp[xfp - xfromp->v_strp],&result) != 0)
					return vrcset();
				goto xnext;
				}
			} while(*++xfp != '\0');

		// No match, copy the source char untranslated.
		if(vputc(*strp,&result) != 0)
			return vrcset();
xnext:
		++strp;
		}

	// Terminate and return the result.
	return vclose(&result) != 0 ? vrcset() : rc.status;
	}

// Join all remaining function arguments into rp using delimp (if not NULL) as "glue" if runtime flag OPEVAL is set; otherwise,
// just "consume" them.  reqct is the number of required arguments.  If delimp is NULL, set ARG_FIRST flag on first argument.
// If keepall is true, include null arguments in result; otherwise, skip them.  Return status.
int join(Value *rp,Value *delimp,int reqct,bool keepall) {
	uint aflags;
	StrList sl;
	Value *vp;		// For arguments.
	bool first = true;
	char *dlmp = (delimp != NULL && !vistfn(delimp,VNIL)) ? delimp->v_strp : NULL;

	// Nothing to do if not evaluating and no arguments.
	if((opflags & (OPSCRIPT | OPPARENS)) == (OPSCRIPT | OPPARENS) && havesym(s_rparen,false) &&
	 (!(opflags & OPEVAL) || reqct == 0))
		return rc.status;

	if(vnew(&vp,false) != 0 || ((opflags & OPEVAL) && vopen(&sl,rp,false) != 0))
		return vrcset();
	aflags = (delimp == NULL) ? ARG_FIRST : 0;

	for(;;) {
		if(aflags == ARG_FIRST) {
			if(!havesym(s_any,reqct > 0))
				break;				// Error or no arguments.
			}
		else if(!havesym(s_comma,false))
			break;					// No arguments left.
		if(macarg(vp,aflags) != SUCCESS)
			return rc.status;
		--reqct;
		if((opflags & OPEVAL) && (keepall || (!visnull(vp) && !vistfn(vp,VNIL)))) {
			if((dlmp != NULL && !first && vputs(dlmp,&sl) != 0) || vputv(vp,&sl) != 0)
				return vrcset();
			first = false;
			}
		aflags = 0;
		}

	// Return result.
	if((opflags & OPEVAL) && vclose(&sl) != 0)
		(void) vrcset();

	return rc.status;
	}

// Process "prompt" user function, given prompt string in prmtp, and save result in rp.  Return status.
static int uprompt(Value *rp,Value *prmtp) {
	uint oldscript;
	int delim;
	uint flags;
	char *defp;
	Value *defargp;

	if((opflags & OPEVAL) && (visnull(prmtp) || vistfn(prmtp,VNIL)))
		return rcset(FAILURE,0,"%s %s",text110,text214);
			// "Prompt string required for","'prompt' function"

	defp = NULL;				// Default value.
	delim = CTRL | 'M';			// Delimiter.
	flags = 0;				// Completion and argument flags.

	// Have "default" argument?
	if(havesym(s_comma,false)) {

		// Yes, get it and use it unless it's nil.
		if(vnew(&defargp,false) != 0)
			return vrcset();
		if(macarg(defargp,0) != SUCCESS)
			return rc.status;
		if((opflags & OPEVAL) && !vistfn(defargp,VNIL)) {
			if(tostr(defargp) != SUCCESS)
				return rc.status;
			defp = defargp->v_strp;
			}

		// Have "delimiter" argument?
		if(havesym(s_comma,false)) {

			// Yes, get it (into rp temporarily) and check it.
			if(macarg(rp,ARG_STR) != SUCCESS)
				return rc.status;
			if((opflags & OPEVAL) && !visnull(rp) && !vistfn(rp,VNIL) && stoec(rp->v_strp,&delim) != SUCCESS)
				return rc.status;

			// Have "type" argument?
			if(havesym(s_comma,false)) {

				// Yes, get it (into rp temporarily).
				if(macarg(rp,ARG_STR) != SUCCESS)
					return rc.status;

				// Bail out here if not evaluating arguments; otherwise, check type.
				if((opflags & OPEVAL) == 0) {
					vnull(rp);
					return rc.status;
					}
				if(!visnull(rp)) {
					int maxlen;

					if(*(rp->v_strp + 1) != '\0')
						goto prmt_err;
					switch(*rp->v_strp) {
						case 'c':
							flags |= ARG_ONEKEY;
							break;
						case 's':
							break;
						case 'b':
							flags = CMPL_BUFFER;
							maxlen = NBUFN;
							goto prmt_comp;
						case 'f':
							flags = CMPL_FILENAME;
							goto prmt_filename;
						case 'F':
							flags = CMPL_FILENAME | CMPL_NOAUTO;
prmt_filename:
							maxlen = NPATHINP;
prmt_comp:
							// Force interactive mode and do completion.
							oldscript = opflags & OPSCRIPT;
							opflags &= ~OPSCRIPT;
							if(complete(rp,prmtp->v_strp,defp,flags,maxlen,0) != SUCCESS)
								(void) vnilmm(rp);
							opflags = (opflags & ~OPSCRIPT) | oldscript;
							return rc.status;
						default:
prmt_err:
							return rcset(FAILURE,0,text295,rp->v_strp);
								// "prompt function type '%s' must be b, c, f, or s"
						}
					}
				}
			}
		}

	// Prompt for a string or single character.
	if(opflags & OPEVAL)
		(void) termarg(rp,prmtp->v_strp,defp,delim,flags);

	return rc.status;
	}

// Process a pop, push, shift, or unshift function and store result in rp if runtime flag OPEVAL is set; otherwise, just
// "consume" arguments.  Set rp to nil if shift or pop and no items left.  Return status.
static int varfunc(Value *rp,enum cfid fid) {
	int status;
	VDesc vd;
	char *strp1,*strp2,*newvarvalp;
	ushort *fp = NULL;		// Pointer to variable flags.
	bool nildlm = false;
	bool nulltok = false;
	Value *delimp,*oldvarvalp,newvar,*newvarp;

	// Syntax of functions:
	//	push var dlm s    pop var dlm    shift var dlm    unshift var dlm s

	// Get variable name from current symbol, find it and its value, and validate it.
	if(!havesym(s_any,true))
		return rc.status;
	if(opflags & OPEVAL) {
		strp1 = last->p_tok.v_strp;
		if(vnew(&oldvarvalp,false) != 0)
			return vrcset();
		if(findvar(strp1,OPDELETE,&vd) != SUCCESS)
			return rc.status;
		if((vd.vd_type == VTYP_SVAR && (vd.u.vd_svp->sv_flags & V_RDONLY)) ||
		 (vd.vd_type == VTYP_NVAR && vd.vd_argnum == 0))
			return rcset(FAILURE,0,text164,strp1);
				// "Cannot modify read-only variable '%s'"
		if(derefv(oldvarvalp,&vd) != SUCCESS || tostr(oldvarvalp) != SUCCESS)
			return rc.status;
		fp = (vd.vd_type == VTYP_LVAR || vd.vd_type == VTYP_GVAR) ? &vd.u.vd_uvp->uv_flags : (vd.vd_type == VTYP_SVAR) ?
		 &vd.u.vd_svp->sv_flags : &(marg(vd.u.vd_malp,vd.vd_argnum)->ma_flags);
		}

	// Get delimiter into delimp.
	if(vnew(&delimp,false) != 0)
		return vrcset();
	if(getsym() < NOTFOUND || macarg(delimp,ARG_STR) != SUCCESS)
		return rc.status;
	if(opflags & OPEVAL) {
		nildlm = vistfn(delimp,VNIL);
		if(!nildlm && (fid == cf_shift || fid == cf_pop) && strlen(delimp->v_strp) > 1)
			return rcset(FAILURE,0,text251,text288,delimp->v_strp,1);
				// "%s delimiter '%s' cannot be more than %d character(s)","Function"
		if(nildlm && (fid == cf_unshift || fid == cf_push))
			(void) vsetstr(" ",delimp);
		}

	// Get value argument into rp for push and unshift functions.
	if((fid == cf_push || fid == cf_unshift) && (macarg(rp,0) != SUCCESS || tostr(rp) != SUCCESS))
		return rc.status;

	// If not evaluating, we're done (all arguments consumed).
	if((opflags & OPEVAL) == 0)
		return rc.status;

	// Evaluating.  Function value is in rp (if push or unshift) and old value of variable is in oldvarvalp.  Do
	// function-specific operation.  Set newvarvalp to new value of variable and copy parsed token to rp if pop or shift.
	switch(fid) {
		case cf_pop:					// Get last token from old var value into rp.

			// Value	strp1		strp2		strp3		Correct result
			// :		:		null		null		"",""
			// ::		:#2		null		null		"","",""
			// x:		x		null		null		"","x"
			// x::		:#2		null		null		"","","x"

			// :x		x		null		null		"x",""
			// ::x		:#2		x		null		"x","",""

			// x		x		null		null		"x"
			// x:y:z	z		null		null		"z","y","x"
			// x::z		z		null		null		"z","","x"

			// "x  y  z"	" z"		null		null

			if(*(newvarvalp = oldvarvalp->v_strp) == '\0')	// Null variable value?
				status = NOTFOUND;
			else {
				strp1 = strchr(newvarvalp,'\0');
				status = rparsetok(rp,&strp1,newvarvalp,nildlm ? -1 : *delimp->v_strp);
				}
			goto check;
		case cf_shift:
			newvarvalp = oldvarvalp->v_strp;
			status = parsetok(rp,&newvarvalp,nildlm ? -1 : *delimp->v_strp);
check:
			newvarp = &newvar;
			newvarp->v_type = VALSTR;
			newvarp->v_strp = newvarp->u.v_solop = newvarvalp;

			if(status != SUCCESS) {			// Any tokens left?
				if(rc.status != SUCCESS)
					return rc.status;	// Fatal error.

				// No tokens left.  "Null token" flag set on variable?
				if(*fp & V_NULLTOK) {

					// Yes, clear flag and return a null token.
					*fp &= ~V_NULLTOK;
					vnull(rp);
					break;
					}

				// Flag not set.  Signal end of token list.
				(void) vnilmm(rp);
				goto rcexit;
				}

			// We have a token.  Set flag on variable if delimiter is not null or white space and last (if shift) or
			// first (if pop) character is a delimiter so that next call to shift or pop will return a null token
			// instead of nil.
			if(fid == cf_shift) {
				if(!nildlm && newvarvalp[0] == '\0' && newvarvalp[-1] == *delimp->v_strp)
					nulltok = true;
				}
			else if(strp1 <= newvarvalp) {		// Just popped last token?
				if(strp1 == newvarvalp && !nildlm)	// Yes, check for leading delimiter.
					nulltok = true;
				*newvarvalp = '\0';
				}
			else
				*strp1 = '\0';			// Not last.  Chop old at current spot (delimiter).
			break;
		case cf_push:
			strp1 = oldvarvalp->v_strp;		// First portion of new var value (old var value).
			strp2 = rp->v_strp;			// Second portion (value to append).
			goto paste;
		default:	// cf_unshift
			strp1 = rp->v_strp;			// First portion of new var value (value to prepend).
			strp2 = oldvarvalp->v_strp;		// Second portion (old var value).
paste:
			{StrList sl;

			if(vopen(&sl,NULL,false) != 0 ||
			 vputs(strp1,&sl) != 0)			// Copy initial portion of new variable value to work buffer.
				return vrcset();

			// Append a delimiter if oldvarvalp is not null, and value (if push) or variable (if unshift).
			if((!visnull(oldvarvalp) && vputs(delimp->v_strp,&sl) != 0) || vputs(strp2,&sl) != 0 || vclose(&sl) != 0)
				return vrcset();
			newvarp = sl.sl_vp;			// New variable value.
			}
			break;
			}

	// Update variable and return status.
	if(putvar(newvarp,&vd) != SUCCESS)
		return rc.status;
	if(nulltok)
		*fp |= V_NULLTOK;
rcexit:
	return rc.status;
	}

#if (MMDEBUG & DEBUG_TOKEN) && 0
static void showsym(char *name) {

	fprintf(logfile,"%s(): last is str \"%s\" (%d)\n",name,last->p_tok.v_strp,last->p_sym);
	}
#endif

// Determine if given item in itemp is defined.  If it's a name (string), set rp to result: "alias", "buffer", "command",
// "function", "macro", "variable", or nil.  If it's a number, set rp to true if mark is defined in current window; otherwise,
// false.  Return status.
static int checkdef(Value *rp,Value *itemp) {
	char *resultp;
	FABPtr fab;

	// Number?
	if(itemp->v_type == VALINT)
		resultp = (itemp->u.v_int >= 0 && itemp->u.v_int < NMARKS &&
		 curwp->w_face.wf_mark[itemp->u.v_int].mk_dot.lnp != NULL) ? val_true : val_false;

	// Null or nil string?
	else if(visnull(itemp) || vistfn(itemp,VNIL))
		resultp = val_nil;

	// Variable?
	else if(findvar(itemp->v_strp,OPQUERY,NULL))
		resultp = text292;
			// "variable"

	// Command, function, alias, or macro?
	else if(fabsearch(itemp->v_strp,&fab,PTRANY)) {
		switch(fab.p_type) {
			case PTRCMD:
				resultp = text158;
					// "command"
				break;
			case PTRFUNC:
				resultp = text247;
					// "function"
				break;
			case PTRBUF:
				resultp = text83;
					// "buffer"
				break;
			case PTRMACRO:
				resultp = text336;
					// "macro"
				break;
			default:	// PTRALIAS
				resultp = text127;
					// "alias"
			}
		}
	else
		// Unknown name.
		resultp = val_nil;

	return vsetstr(resultp,rp) != 0 ? vrcset() : rc.status;
	}

// Insert, overwrite, replace, or write text to a buffer n times, given buffer pointer and text insertion style.  If bufp is
// NULL, use current buffer.  If n == 0, do one repetition and don't move point.  Return text in rp and return status.
int chgtext(Value *rp,int n,Buffer *bufp,enum e_textedit t) {
	Buffer *obufp = NULL;
	Value *vtextp;
	StrList text;
	uint aflags = ARG_FIRST;

	if(n == INT_MIN)
		n = 1;

	if(vnew(&vtextp,false) != 0)
		return vrcset();

	// Evaluate all the arguments and save in string list (so that the text can be inserted more than once, if requested).
	if(vopen(&text,rp,false) != 0)
		return vrcset();

	for(;;) {
		if(aflags == ARG_FIRST) {
			if(!havesym(s_any,true))
				return rc.status;		// Error.
			}
		else if(!havesym(s_comma,false))
			break;					// No arguments left.
		if(macarg(vtextp,aflags) != SUCCESS)
			return rc.status;
		aflags = 0;
		if(visnull(vtextp) || vistfn(vtextp,VNIL))
			continue;				// Ignore null and nil values.

		// Add text chunk to string list.
		if(vputv(vtextp,&text) != 0)
			return vrcset();
		}
	if(vclose(&text) != 0)
		return vrcset();

	// Make the target buffer current and save its dot position.
	if(bufp != NULL && bufp != curbp) {
		obufp = curbp;
		if(bswitch(bufp) != SUCCESS)
			return rc.status;
		}

	// We have all the text (in rp).  Now insert, overwrite, or replace it n times.
	if(iortext(rp,n,t,false) == SUCCESS) {

		// Restore current buffer.
		if(obufp != NULL)
			(void) bswitch(obufp);
		}

	return rc.status;
	}

// Build string from "printf" format string (formatp) and following argument(s).  If arg1p is not NULL, process binary format
// (%) expression using arg1p as the argument; otherwise, process sprintf function.  Return status.
int strfmt(Value *rp,int n,Value *formatp,Value *arg1p) {
	int c,specCount;
	ulong ul;
	char *prefixp,*fmtp;
	int prefLen;		// Length of prefix string.
	int width;		// Field width.
	int padding;		// # of chars to pad (on left or right).
	int precision;
	char *strp;
	int sLen;		// Length of formatted string s.
	int flags;		// FMTxxx.
	int base;		// Number base (decimal, octal, etc.).
	Value *tp;
	StrList result;
	char wkbuf[FMTBUFSZ];

	fmtp = formatp->v_strp;

	// Create string list for result and work Value object for sprintf call.
	if(vopen(&result,rp,false) != 0 || (arg1p == NULL && vnew(&tp,false) != 0))
		return vrcset();

	// Loop through format string.
	specCount = 0;
	while((c = *fmtp++) != '\0') {
		if(c != '%') {
			if(vputc(c,&result) != 0)
				return vrcset();
			continue;
			}

		// Check for prefix(es).
		prefixp = NULL;					// Assume no prefix.
		flags = 0;					// Reset.
		while((c = *fmtp++) != '\0') {
			switch(c) {
			case '0':
				flags |= FMT0pad; 		// Pad with 0's.
				break;
			case '-':
				flags |= FMTleft; 		// Left-justify.
				break;
			case '+':
				flags |= FMTplus; 		// Do + or - sign.
				break;
			case ' ':
				flags |= FMTspc;		// Space flag.
				break;
			case '#':
				flags |= FMThash; 		// Alternate form.
				break;
			default:
				goto getwidth;
			}
		}

		// Get width.
getwidth:
		width = 0;
		if(c == '*') {
			if(arg1p != NULL)			// Error if format op (%).
				goto toomany;
			if(macarg(tp,ARG_INT) != SUCCESS)	// Get next (int) argument for width.
				return rc.status;
			if((width = tp->u.v_int) < 0) {		// Negative field width?
				flags |= FMTleft;		// Yes, left justify field.
				width = -width;
				}
			c = *fmtp++;
			}
		else {
			while(isdigit(c)) {
				width = width * 10 + c - '0';
				c = *fmtp++;
				}
			}

		// Get precision.
		precision = 0;
		if(c == '.') {
			c = *fmtp++;
			if(c == '*') {
				if(arg1p != NULL)		// Error if format op (%).
					goto toomany;
				if(macarg(tp,ARG_INT) != SUCCESS)
					return rc.status;
				if((precision = tp->u.v_int) < 0)
					precision = 0;
				else
					flags |= FMTprec;
				c = *fmtp++;
				}
			else if(isdigit(c)) {
				flags |= FMTprec;
				do {
					precision = precision * 10 + c - '0';
					} while(isdigit(c = *fmtp++));
				}
			}

		// Get el flag.
		if(c == 'l') {
			flags |= FMTlong;
			c = *fmtp++;
			}

		// Get spec.
		switch(c) {
		case 's':
			if(arg1p != NULL) {
				if(!strval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
					goto toomany;
				tp = arg1p;
				}
			else if(macarg(tp,ARG_STR) != SUCCESS)
				return rc.status;
			sLen = strlen(strp = tp->v_strp);	// Length of string.
			if(flags & FMTprec) {			// If there is a precision ...
				if(precision < sLen)
					if((sLen = precision) < 0)
						sLen = 0;
				}
			break;
		case '%':
			wkbuf[0] = '%';
			goto savech;
		case 'c':
			if(arg1p != NULL) {
				if(!intval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
					goto toomany;
				tp = arg1p;
				}
			else if(macarg(tp,ARG_INT) != SUCCESS)
				return rc.status;
			wkbuf[0] = tp->u.v_int;
savech:
			strp = wkbuf;
			sLen = 1;
			break;
		case 'd':
		case 'i':
			if(arg1p != NULL) {
				if(!intval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
					goto toomany;
				tp = arg1p;
				}
			else if(macarg(tp,ARG_INT) != SUCCESS)
				return rc.status;
			base = 10;
			ul = labs(tp->u.v_int);
			prefixp = tp->u.v_int < 0 ? "-" : (flags & FMTplus) ? "+" : (flags & FMTspc) ? " " : "";
			goto ulfmt;
		case 'b':
			base = 2;
			goto getuns;
		case 'o':
			base = 8;
			goto getuns;
		case 'u':
			base = 10;
getuns:
			if(arg1p != NULL) {
				if(!intval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
					goto toomany;
				tp = arg1p;
				}
			else if(macarg(tp,ARG_INT) != SUCCESS)
				return rc.status;
			ul = (ulong) tp->u.v_int;
			goto ulfmt;
		case 'X':
			flags |= FMTxuc;
		case 'x':
			if(arg1p != NULL) {
				if(!intval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
toomany:
					return rcset(FAILURE,0,text320);
						// "More than one argument specified for '%' expression"
				tp = arg1p;
				}
			else if(macarg(tp,ARG_INT) != SUCCESS)
				return rc.status;
			base = 16;
			ul = (ulong) tp->u.v_int;
			if((flags & FMThash) && ul)
				prefixp = (c == 'X') ? "0X" : "0x";

			// Fall through.
ulfmt:
			// Ignore '0' flag if precision specified.
			if((flags & (FMT0pad | FMTprec)) == (FMT0pad | FMTprec))
				flags &= ~FMT0pad;

			strp = wkbuf + (FMTBUFSZ - 1);
			if(ul) {
				for(;;) {
					*strp = (ul % base) + '0';
					if(*strp > '9')
						*strp += (flags & FMTxuc) ? 'A' - '0' - 10 : 'a' - '0' - 10;
					if((ul /= base) == 0)
						break;
					--strp;
					}
				sLen = wkbuf + FMTBUFSZ - strp;
				}
			else if((flags & FMTprec) && precision == 0)
				sLen = 0;
			else {
				*strp = '0';
				sLen = 1;
				}

			if(sLen < precision) {
				if(precision > FMTBUFSZ)
					precision = FMTBUFSZ;
				do {
					*--strp = '0';
					} while(++sLen < precision);
				}
			else if(sLen > 0 && c == 'o' && (flags & FMThash) && *strp != '0') {
				*--strp = '0';
				++sLen;
				}
			break;
		default:
			return rcset(FAILURE,0,text321,c);
				// "Unknown format spec '%%%c'"
		}

		// Concatenate the pieces, which are padding, prefix, more padding, the string, and trailing padding.
		prefLen = (prefixp == NULL) ? 0 : strlen(prefixp); // Length of prefix string.
		padding = width - (prefLen + sLen);		// # of chars to pad.

		// If 0 padding, store prefix (if any).
		if((flags & FMT0pad) && prefixp != NULL) {
			if(vputs(prefixp,&result) != 0)
				return vrcset();
			prefixp = NULL;
			}

		// If right-justified and chars to pad, store prefix string.
		if(padding > 0) {
			if(!(flags & FMTleft)) {
				int c = (flags & FMT0pad) ? '0' : ' ';
				while(--padding >= 0)
					if(vputc(c,&result) != 0)
						return vrcset();
				}
			}

		// Store prefix (if any).
		if(prefixp != NULL && vputs(prefixp,&result) != 0)
			return vrcset();

		// Store (fixed-length) string.
		if(vputfs(strp,sLen,&result) != 0)
			return vrcset();

 		// Store right padding.
		if(flags & FMTleft) {
			while(--padding >= 0)
				if(vputc(' ',&result) != 0)
					return vrcset();
			}
		}

	return vclose(&result) != 0 ? vrcset() : rc.status;
	}

// Evaluate a system function, given result pointer, n argument, and command-function pointer.  Return status.  If runtime flag
// OPEVAL is not set, routine is called only for functions which need special processing (CFSPECARGS command flag is set).
// Also, any command with CFNCOUNT flag set that is processed here must handle a zero n so that feval() calls in other routines
// will always work (for any value of n).
int feval(Value *rp,int n,CmdFunc *cfp) {
	int i;
	char *strp;
	long lval;
	Value *rp1,*rp2,*rp3;		// Values of first three arguments.
	int fnum = cfp - cftab;

	// If not a command and needed, retrieve the first argument ...
	if(cfp->cf_flags & CFFUNC) {
		if(cfp->cf_nargs >= 1) {
			if(vnew(&rp1,false) != 0)
				return vrcset();
			if(macarg(rp1,ARG_FIRST) != SUCCESS)
				return rc.status;

			// and the second ...
			if(cfp->cf_nargs >= 2) {
				if(vnew(&rp2,false) != 0)
					return vrcset();
				if(macarg(rp2,0) != SUCCESS)
					return rc.status;

				// and the third.
				if(cfp->cf_nargs >= 3) {
					if(vnew(&rp3,false) != 0)
						return vrcset();
					if(macarg(rp3,0) != SUCCESS)
						return rc.status;
					}
				}
			}

		// Check types.
		if(((cfp->cf_flags & CFNUM1) && !intval(rp1)) ||
		 ((cfp->cf_flags & CFNUM2) && !intval(rp2)) ||
		 ((cfp->cf_flags & CFNUM3) && !intval(rp3)) ||
		 (cfp->cf_nargs >= 1 && (cfp->cf_flags & (CFNUM1 | CFANY)) == 0 && !strval(rp1)) ||
		 (cfp->cf_nargs >= 2 && (cfp->cf_flags & (CFNUM2 | CFANY)) == 0 && !strval(rp2)) ||
		 (cfp->cf_nargs >= 3 && (cfp->cf_flags & (CFNUM3 | CFANY)) == 0 && !strval(rp3)))
			return rc.status;
		}

	// Evaluate the function.
	switch(fnum) {
		case cf_abs:
			vsetint(labs(rp1->u.v_int),rp);
			break;
		case cf_alterBufMode:
			i = 3;
			goto altermode;
		case cf_alterDefMode:
			i = MDR_DEFAULT;
			goto altermode;
		case cf_alterGlobalMode:
			i = MDR_GLOBAL;
			goto altermode;
		case cf_alterShowMode:
			i = MDR_SHOW;
altermode:
			(void) adjustmode(rp,n,i,NULL);
			break;
		case cf_appendFile:
			i = 'a';
			goto awfile;
		case cf_backPageNext:
			// Scroll the next window up (backward) a page.
			(void) wscroll(rp,n,nextWind,backPage);
			break;
		case cf_backPagePrev:
			// Scroll the previous window up (backward) a page.
			(void) wscroll(rp,n,prevWind,backPage);
			break;
		case cf_backTab:
			// Move the cursor backward "n" tab stops.
			(void) bftab(n == INT_MIN ? -1 : -n);
			break;
		case cf_basename:
			if(vsetstr(fbasename(rp1->v_strp,n == INT_MIN || n > 0),rp) != 0)
				(void) vrcset();
			break;
		case cf_beginBuf:
			// Goto the beginning of the buffer.
			strp = text326;
				// "Begin"
			i = false;
			goto bebuf;
		case cf_beginLine:
			// Move the cursor to the beginning of the [-]nth line.
			(void) beline(rp,n,0);
			break;
		case cf_beginWhite:
			// Move the cursor to the beginning of white space surrounding dot.
			(void) spanwhite(false);
			break;
		case cf_binding:
			if(stoec(rp1->v_strp,&i) != SUCCESS)
				break;
			strp = fixnull(getkname(getbind(i)));

			if(vsetstr(*strp == '\0' ? val_nil : *strp == SBMACRO ? strp + 1 : strp,rp) != 0)
				(void) vrcset();
			break;
		case cf_bufBoundQ:
			if(n != INT_MIN)
				n = n > 0 ? 1 : n < 0 ? -1 : 0;
			i = (curwp->w_face.wf_dot.lnp == curbp->b_hdrlnp) ? 1 :
			 (curwp->w_face.wf_dot.lnp == lforw(curbp->b_hdrlnp) && curwp->w_face.wf_dot.off == 0) ? -1 : 0;
			(void) ltos(rp,(n == INT_MIN && i != 0) || i == n);
			break;
		case cf_bufWind:
			if((i = bufWind(NULL,rp1->v_strp,NULL)) > 0)
				vsetint((long) i,rp);
			else
				(void) vnilmm(rp);
			break;
		case cf_chr:
			vsetchr(rp1->u.v_int,rp);
			break;
		case cf_clearKillRing:
			i = NRING;
			do {
				kcycle();
				} while(--i > 0);
			(void) rcset(SUCCESS,0,text228);
				// "Kill ring cleared"
			break;
		case cf_clearMsg:
			mlerase(n > 0 ? MLFORCE : 0);
			break;
		case cf_copyFencedText:
			// Copy text to kill ring.
			i = 1;
			goto kdcfenced;
		case cf_copyLine:
			// Copy line(s) to kill ring.
			i = 1;
			goto kdcln;
		case cf_copyRegion:
			// Copy all of the characters in the region to the kill ring.  Don't move dot at all.
			{Region region;

			if(getregion(&region,NULL) != SUCCESS || copyreg(&region) != SUCCESS)
				break;
			}
			(void) rcset(SUCCESS,0,text70);
				// "Region copied"
			break;
		case cf_copyToBreak:
			// Copy text to kill ring.
			i = 1;
			goto kdctxt;
		case cf_copyWord:
			// Copy word(s) to kill buffer without moving point.
			(void) (n == INT_MIN ? kdcfword(1,1) : n < 0 ? kdcbword(-n,1) : kdcfword(n,1));
			break;
		case cf_cPrefix:
		case cf_hPrefix:
		case cf_metaPrefix:
		case cf_negativeArg:
		case cf_universalArg:
		case cf_xPrefix:
			break;
		case cf_cycleKillRing:
			// Cycle the kill ring forward or backward.
			(void) cycle_ring(n,true);
			break;
		case cf_definedQ:
			(void) checkdef(rp,rp1);
			break;
		case cf_deleteBackChar:
			// Delete backward.  Return status.
			(void) ldelete(n == INT_MIN ? -1L : (long) -n,0);
			break;
		case cf_deleteFencedText:
			// Delete text without saving in kill ring.
			i = 0;
			goto kdcfenced;
		case cf_deleteForwChar:
			// Delete forward.  Return status.
			(void) ldelete(n == INT_MIN ? 1L : (long) n,0);
			break;
		case cf_deleteLine:
			// Delete line(s) without saving text in kill ring.
			i = 0;
			goto kdcln;
		case cf_deleteRegion:
			// Delete region without saving text in kill ring.
			i = false;
			goto dkreg;
		case cf_deleteToBreak:
			// Delete text without saving in kill ring.
			i = 0;
			goto kdctxt;
		case cf_deleteWhite:
			// Delete white space surrounding point on current line.
			(void) delwhite();
			break;
		case cf_deleteWord:
			// Delete word(s) without saving text in kill buffer.
			(void) (n == INT_MIN ? kdcfword(1,0) : n < 0 ? kdcbword(-n,0) : kdcfword(n,0));
			break;
		case cf_dirname:
			if(vsetstr(fdirname(rp1->v_strp),rp) != 0)
				(void) vrcset();
			break;
		case cf_endBuf:
			// Move to the end of the buffer.
			strp = text188;
				// "End"
			i = true;
bebuf:
			(void) bufop(rp,n,strp,BOPMOVEDOT,i);
			break;
		case cf_endLine:
			// Move the cursor to the end of the [-]nth line.
			(void) beline(rp,n,lused(curwp->w_face.wf_dot.lnp));
			break;
		case cf_endWhite:
			// Move the cursor to the end of white space surrounding dot.
			(void) spanwhite(true);
			break;
		case cf_env:
			if(vsetstr(fixnull(getenv(rp1->v_strp)),rp) != 0)
				(void) vrcset();
			break;
		case cf_fileExistsQ:
			if(vsetstr((i = fexist(rp1->v_strp)) < 0 ? val_nil : i > 0 ? text335 : text99,rp) != 0)
										// "directory","file"
				(void) vrcset();
			break;
		case cf_findFile:
			i = false;
			goto fvfile;
		case cf_forwPageNext:
			// Scroll the next window down (forward) a page.
			(void) wscroll(rp,n,nextWind,forwPage);
			break;
		case cf_forwPagePrev:
			// Scroll the previous window down (forward) a page.
			(void) wscroll(rp,n,prevWind,forwPage);
			break;
		case cf_forwTab:
			// Move the cursor forward "n" tab stops.
			(void) bftab(n == INT_MIN ? 1 : n);
			break;
		case cf_getKey:
			if((n <= 0 ? getkey(&i) : getkseq(&i,NULL)) != SUCCESS)
				break;
			if(i == ckeys.abort)
				return abortinp();
			{char wkbuf[16];
			if(vsetstr(ectos(i,wkbuf,false),rp) != 0)
				(void) vrcset();
			}
			break;
		case cf_gotoFence:
			// Move the cursor to a matching fence.
			{Region region;

			if(otherfence(&region) == 0)
				(void) rcset(FAILURE,0,NULL);
			}
			break;
		case cf_growWind:
			// Enlarge the current window.  If n is negative, take lines from the upper window; otherwise, lower.
			i = true;
			goto gswnd;
		case cf_hideBuf:
			i = BFHIDDEN;
			strp = text195;
				// "Hide"
			goto altflag;
		case cf_includeQ:
			{bool nildlm = vistfn(rp2,VNIL);
			if(!nildlm) {
				if(*rp2->v_strp == '\0')
					return rcset(FAILURE,0,text187,text329);
						// "%s cannot be null","in? delimiter"
				if(strlen(rp2->v_strp) > 1)
					return rcset(FAILURE,0,text251,text288,rp2->v_strp,1);
						// "%s delimiter '%s' cannot be more than %d character(s)","Function"
				}
			if(visnull(rp1) || vistfn(rp1,VNIL) || visnull(rp2) || visnull(rp3) || vistfn(rp3,VNIL))
				i = false;
			else if((strp = strstr(rp1->v_strp,rp3->v_strp)) == NULL)
				i = false;
			else {
				// Match found.  Check boundaries.
				int dlm = nildlm ? -1 : *rp2->v_strp;
				i = (strp == rp1->v_strp || strp[-1] == dlm ||
				 (dlm < 0 && (strp[-1] == ' ' || strp[-1] == '\t'))) &&
				 (*(strp += strlen(rp3->v_strp)) == '\0' || *strp == dlm ||
				 (dlm < 0 && (*strp == ' ' || *strp == '\t'))) ? true : false;
				}
			}
			(void) ltos(rp,i);
			break;
		case cf_index:
			(void) sindex(rp,rp1,rp2,n > 0);
			break;
		case cf_insert:
			(void) chgtext(rp,n,NULL,txt_insert);
			break;
		case cf_insertSpace:
			// Insert space(s) forward into text without moving point.
			if(n != 0) {
				if(n == INT_MIN)
					n = 1;
				if(linsert(n,' ') == SUCCESS)
					(void) backch(n);
				}
			break;
		case cf_intQ:
			(void) ltos(rp,rp1->v_type == VALINT);
			break;
		case cf_join:
			(void) join(rp,rp1,1,n == INT_MIN || n > 0);
			break;
		case cf_killFencedText:
			// Delete text and save in kill ring.
			i = -1;
kdcfenced:
			(void) kdcfencedreg(i);
			break;
		case cf_killLine:
			// Delete line(s) and save text in kill ring.
			i = -1;
kdcln:
			(void) kdcline(n,i);
			break;
		case cf_killRegion:
			// Delete region and save text in kill ring.
			i = true;
dkreg:
			(void) dkregion(n,i);
			break;
		case cf_killToBreak:
			// Delete text and save in kill ring.
			i = -1;
kdctxt:
			(void) kdctext(n,i,NULL);
			break;
		case cf_killWord:
			// Delete word(s) and save text in kill buffer.
			(void) (n == INT_MIN ? kdcfword(1,-1) : n < 0 ? kdcbword(-n,-1) : kdcfword(n,-1));
			break;
		case cf_lcLine:
			// Lower case line.
			strp = lowcase;
			goto caseln;
		case cf_lcRegion:
			// Lower case region.
			strp = lowcase;
			goto casereg;
		case cf_lcString:
			i = -1;
			goto casestr;
		case cf_length:
			vsetint((long) strlen(rp1->v_strp),rp);
			break;
		case cf_match:
			if(rp1->u.v_int < 0 || rp1->u.v_int >= MAXGROUPS)
				return rcset(FAILURE,0,text5,rp1->u.v_int,MAXGROUPS - 1);
					// "Group number %ld must be between 0 and %d"
			if(vsetstr(fixnull(srch.grpmatch[(int) rp1->u.v_int]),rp) != 0)
				(void) vrcset();
			break;
		case cf_moveWindDown:
			// Move the current window down by "n" lines and compute the new top line in the window.
			(void) moveWindUp(rp,n == INT_MIN ? -1 : -n);
			break;
		case cf_nextArg:
			if(scriptrun == NULL || scriptrun->malp->mal_argp == NULL)
				(void) vnilmm(rp);
			else if(vcpy(rp,scriptrun->malp->mal_argp->ma_valp) != 0)
				(void) vrcset();
			else
				scriptrun->malp->mal_argp = scriptrun->malp->mal_argp->ma_nextp;
			break;
		case cf_newline:
			i = true;
			goto inlsp;
		case cf_nextBuf:
			// Switch to the next buffer in the buffer list.
			i = false;
			goto pnbuf;
		case cf_nilQ:
			(void) ltos(rp,vistfn(rp1,VNIL));
			break;
		case cf_nullQ:
			(void) ltos(rp,visnull(rp1));
			break;
		case cf_numericQ:
			(void) ltos(rp,asc_long(rp1->v_strp,&lval,true));
			break;
		case cf_ord:
			vsetint((long) rp1->v_strp[0],rp);
			break;
		case cf_overwrite:
			(void) chgtext(rp,n,NULL,txt_overwrite);
			break;
		case cf_pad:
			if(!intval(rp2) || tostr(rp1) != SUCCESS)
				return rc.status;
			(void) vpad(rp,rp1,rp2->u.v_int);
			break;
		case cf_pathname:
			(void) getpath(rp,rp1->v_strp);
			break;
		case cf_pause:
			// Set default argument if none.
			if(n == INT_MIN)
				n = 100;		// Default is 1 second.
			else if(n < 0)
				return rcset(FAILURE,0,text39,text119,n,0);
					// "%s (%d) must be %d or greater","Pause duration"
			(void) cpause(n);
			break;
		case cf_pop:
		case cf_push:
		case cf_shift:
		case cf_unshift:
			(void) varfunc(rp,fnum);
			break;
		case cf_prevBuf:
			// Switch to the previous buffer in the buffer list.
			i = true;
pnbuf:
			(void) pnbuffer(rp,n,i);
			break;
		case cf_prevScreen:
			// Bring the previous screen number to the front.

			// Determine screen number if no argument.
			if(n == INT_MIN && (n = cursp->s_num - 1) == 0)
				n = scrcount();

			(void) nextScreen(rp,n);
			break;
		case cf_print:
			// Concatenate all arguments into rp.
			if(join(rp,NULL,1,true) != SUCCESS)
				return rc.status;

			// Write the message out.
			(void) mlputv(n <= 0 ? MLHOME : MLHOME | MLFORCE,rp,vz_show);
			break;
		case cf_prompt:
			(void) uprompt(rp,rp1);
			break;
		case cf_queryReplace:
			// Search and replace with query.
			rp1 = rp;
			goto repl;
		case cf_quickExit:
			// Quick exit from Emacs.  If any buffer has changed, do a save on that buffer first.
			if(savebufs(1,true) == SUCCESS)
				(void) rcset(USEREXIT,0,"");
			break;
		case cf_quote:
			{StrList sl;

			if(vopen(&sl,rp,false) != 0)
				(void) vrcset();
			else if(tostr(rp1) == SUCCESS && quote(&sl,rp1->v_strp,true) == SUCCESS && vclose(&sl) != 0)
				(void) vrcset();
			}
			break;
		case cf_rand:
			vsetint((long) ernd(),rp);
			break;
		case cf_readFile:
			// Read a file into a buffer.

			// Get the filename ...
			if(gtfilename(rp,n < 0 && n != INT_MIN ? text299 : text131,0) != SUCCESS ||
			 ((opflags & OPSCRIPT) == 0 && vistfn(rp,VNIL)))
					// "Pop file","Read file"
				break;

			// and return results.
			(void) rdfile(rp,n,rp->v_strp,false);
			break;
		case cf_redrawScreen:
			// Redraw and possibly reposition dot in the current window.  If n is zero, redraw only; otherwise,
			// reposition dot per the standard redisplay code.
			if(n == 0)
				opflags |= OPSCREDRAW;
			else {
				if(n == INT_MIN)
					n = 0;		// Default to 0 to center current line in window.
				curwp->w_force = n;
				curwp->w_flags |= WFFORCE;
				}
			break;
		case cf_replace:
			// Search and replace.
			rp1 = NULL;
repl:
			(void) replstr(rp1,n);
			break;
		case cf_replaceText:
			(void) chgtext(rp,n,NULL,txt_replace);
			break;
		case cf_restoreBuf:
			// Restore the saved buffer.
			if(sbuffer == NULL)
				return rcset(FAILURE,0,text208,text83);
						// "No saved %s to restore","buffer"
			if(bswitch(sbuffer) == SUCCESS && vsetstr(curbp->b_bname,rp) != 0)
				(void) vrcset();
			break;
		case cf_restoreWind:
			// Restore the saved window.
			{EWindow *winp;

			// Find the window.
			winp = wheadp;
			do {
				if(winp == swindow) {
					curwp->w_flags |= WFMODE;
					wswitch(winp);
					curwp->w_flags |= WFMODE;
					return rc.status;
					}
				} while((winp = winp->w_nextp) != NULL);
			}
			(void) rcset(FAILURE,0,text208,text331);
				// "No saved %s to restore","window"
			break;
		case cf_reverse:
			strrev(vxfer(rp,rp1)->v_strp);
			break;
		case cf_saveBuf:
			// Save pointer to current buffer.
			sbuffer = curbp;
			if(vsetstr(curbp->b_bname,rp) != 0)
				(void) vrcset();
			break;
		case cf_saveFile:
			// Save the contents of the current buffer (or all buffers if n arg) to their associated files.
			(void) savebufs(n,false);
			break;
		case cf_saveWind:
			// Save pointer to current window.
			swindow = curwp;
			break;
		case cf_setWrapCol:
			// Set wrap column to n.
			if(n == INT_MIN)
				n = 0;
			else if(n < 0)
				(void) rcset(FAILURE,0,text39,text59,n,0);
						// "%s (%d) must be %d or greater","Wrap column"
			else {
				wrapcol = n;
				(void) rcset(SUCCESS,0,"%s%s%d",text59,text278,n);
						// "Wrap column"," set to "
				}
			break;
		case cf_shQuote:
			if(tostr(rp1) == SUCCESS && vshquote(rp,rp1->v_strp) != 0)
				(void) vrcset();
			break;
		case cf_shrinkWind:
			// Shrink the current window.  If n is negative, give lines to the upper window; otherwise, lower.
			i = false;
gswnd:
			(void) gswind(rp,n,i);
			break;
		case cf_space:
			i = false;
inlsp:
			(void) insnlspace(rp,n,i);
			break;
		case cf_sprintf:
			// Get format string and call strfmt().
			{Value *formatp;

			if(vnew(&formatp,false) != 0)
				return vrcset();
			if(macarg(formatp,ARG_FIRST | ARG_NOTNULL | ARG_STR) != SUCCESS)
				return rc.status;
			(void) strfmt(rp,n,formatp,NULL);
			}
			break;
		case cf_stringQ:
			(void) ltos(rp,rp1->v_type != VALINT);
			break;
		case cf_stringFit:
			if(rp2->u.v_int < 0)
				return rcset(FAILURE,0,text39,text290,(int) rp2->u.v_int,0);
						// "%s (%d) must be %d or greater","Length argument"
			if(vsalloc(rp,rp2->u.v_int + 1) != 0)
				return vrcset();
			strfit(rp->v_strp,rp2->u.v_int,rp1->v_strp,0);
			break;
		case cf_stringLit:
			{StrList sl;

			if(tostr(rp1) != SUCCESS)
				return rc.status;
			if(vopen(&sl,rp,false) != 0 || vstrlit(&sl,rp1->v_strp,0) != 0 || vclose(&sl) != 0)
				return vrcset();
			}
			break;
		case cf_strip:
#if 0
			(void) stripstr(rp,rp1,n == INT_MIN ? 0 : n);
#else
			if(vsetstr(stripstr(rp1->v_strp,n == INT_MIN ? 0 : n),rp) != 0)
				(void) vrcset();
#endif
			break;
		case cf_sub:
			(void) strsub(rp,n,rp1->v_strp,rp2->v_strp,rp3->v_strp);
			break;
		case cf_subLine:
			{long lval2 = rp2->u.v_int;
			lval = rp1->u.v_int;
			if(lval2 != 0 && (i = lused(curwp->w_face.wf_dot.lnp)) > 0) {

				// Determine line offset and length and validate them.  Offset is position relative to dot.
				lval += curwp->w_face.wf_dot.off;
				if(lval >= 0 && lval < i && (lval2 >= 0 || (lval2 = i - lval + lval2) > 0)) {
					if(lval2 > i - lval)
						lval2 = i - lval;
					if(vsetfstr(ltext(curwp->w_face.wf_dot.lnp) + (int) lval,(size_t) lval2,rp) != 0)
						return vrcset();
					}
				else
					vnull(rp);
				}
			else
				vnull(rp);
			}
			break;
		case cf_subString:
			if(rp3->u.v_int != 0 && (lval = strlen(rp1->v_strp)) > 0 && labs(rp2->u.v_int) < lval) {
				if(rp2->u.v_int < 0)					// Negative offset.
					rp2->u.v_int += lval;
				lval -= rp2->u.v_int;					// Maximum bytes can copy.
				if(rp3->u.v_int > 0 || (rp3->u.v_int += lval) > 0) {
					if(vsetfstr(rp1->v_strp + rp2->u.v_int,(size_t) (rp3->u.v_int <= lval ?
					 rp3->u.v_int : lval),rp) != 0)
						return vrcset();
					}
				else
					vnull(rp);
				}
			else
				vnull(rp);
			break;
		case cf_tab:
			// Process a tab.  Normally bound to ^I.
			(void) instab(n == INT_MIN ? 1 : n);
			break;
		case cf_tcString:
			i = 0;
			goto casestr;
		case cf_toInt:
			vxfer(rp,rp1);
			(void) toint(rp);
			break;
		case cf_toString:
			vxfer(rp,rp1);
			(void) tostr(rp);
			break;
		case cf_tr:
			(void) tr(rp,rp1,rp2,rp3);
			break;
		case cf_truncBuf:
			// Truncate buffer.  Delete all text from current buffer position to end and save in undelete buffer.
			// Set rp to buffer name and return status.
			if(vsetstr(curbp->b_bname,rp) != 0)
				return vrcset();
			if(curwp->w_face.wf_dot.lnp == curbp->b_hdrlnp)
				break;			// No op if currently at end of buffer.

			// Delete maximum possible, ignoring any end-of-buffer error.
			kdelete(&undelbuf);
			(void) ldelete(LONG_MAX,DFDEL);
			break;
		case cf_ucLine:
			// Upper case line.
			strp = upcase;
caseln:
			(void) caseline(n,strp);
			break;
		case cf_ucRegion:
			// Upper case region.
			strp = upcase;
casereg:
			(void) caseregion(n,strp);
			break;
		case cf_ucString:
			i = 1;
casestr:
			{char *(*mk)(char *destp,char *srcp);
			mk = (i <= 0) ? mklower : mkupper;
			if(vsalloc(rp,strlen(rp1->v_strp) + 1) != 0)
				return vrcset();
			mk(rp->v_strp,rp1->v_strp);
			if(i == 0)
				*rp->v_strp = upcase[(int) *rp1->v_strp];
			}
			break;
		case cf_unchangeBuf:
			// Clear a buffer's "changed" flag.
			i = BFCHGD;
			strp = text250;
				// "Unchange"
			goto altflag;
		case cf_undelete:
			// Insert text from the undelete buffer.
			(void) iortext(NULL,n,txt_insert,false);
			break;
		case cf_unhideBuf:
			i = BFHIDDEN;
			strp = text186;
				// "Unhide"
altflag:
			(void) bufop(rp,n,strp,strp == text195 ? BOPSETFLAG : BOPCLRFLAG,i);
			break;
		case cf_updateScreen:
			(void) update(n > 0);
			break;
		case cf_viewFile:
			i = true;
fvfile:
			(void) getfile(rp,n,i);
			break;
		case cf_writeBuf:
			(void) writeBuf(rp,n);
			break;
		case cf_writeFile:
			i = 'w';
awfile:
			(void) fileout(rp,i == 'w' ? text144 : text218,i);
					// "Write file: ","Append file"
			break;
		case cf_xPathname:
			if(pathsearch(&strp,rp1->v_strp,false) != SUCCESS)
				return rc.status;
			if(vsetstr(strp == NULL ? val_nil : strp,rp) != 0)
				return vrcset();
			break;
		case cf_yank:
			// Yank text from the kill buffer.
			if(n == INT_MIN)
				n = 1;
			(void) iortext(NULL,n,txt_insert,true);
			break;
		}

	// Function call completed.  Extra arguments in script mode are checked in fcall() so no need to do it here.
	return rc.status == SUCCESS ? rcsave() : rc.status;
	}

// Evaluate a string literal and return result.  srcp is assumed to begin and end with ' or ".  In single-quoted strings,
// escaped backslashes '\\' and apostrophes '\'' are recognized (only); in double-quoted strings, escaped backslashes "\\",
// double quotes "\"", special letters (like "\r" and "\t"), \nnn octal and hexadecimal sequences, and Ruby-style interpolated
// #{} expressions are recognized (and executed).  For example, "Values are #{sub #{join ',',values,final},',',delim} [#{ct}]".
int evalslit(Value *rp,char *srcp) {
	int termch,c;
	char *srcp0;
	StrList result;		// Put object.

	// Get ready.
	if((opflags & OPEVAL) && vopen(&result,rp,false) != 0)
		return vrcset();
	termch = *srcp;
	srcp0 = srcp++;

	// Loop until null or string terminator hit.
	while((c = *srcp) != termch) {
#if MMDEBUG & DEBUG_TOKEN
		if(c == '\0')
			return rcset(FAILURE,0,"String terminator %c not found in '%s'",termch,srcp0);
#endif
		// Process escaped characters.
		if(c == '\\') {
			if(*++srcp == '\0')
				break;

			// Do single-quote processing.
			if(termch == '\'') {

				// Check for escaped backslash or apostrophe only.
				if(*srcp == '\\' || *srcp == '\'')
					c = *srcp++;
				}

			// Do double-quote processing.
			else {
				// Initialize variables for \nn parsing, if any.  \0x... and \x... are hex; otherwise, octal.
				int c2;
				int base = 8;
				int maxlen = 3;
				char *srcp1;

				// Parse \x and \0n... sequences.
				switch(*srcp++) {
					case 'r':	// CR
						c = 015; break;
					case 'n':	// NL
						c = 012; break;
					case 't':	// Tab
						c = 011; break;
					case 'b':	// Backspace
						c = 010; break;
					case 'f':	// Form feed
						c = 014; break;
					case 'e':	// Escape
						c = 033; break;
					case 'x':
						goto isnum;
					case '0':
						if(*srcp == 'x') {
							++srcp;
isnum:
							base = 16;
							maxlen = 2;
							goto getnum;
							}
						// Fall through.
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						--srcp;
getnum:
						// \nn found.  srcp is at first digit (if any).  Decode it.
						c = 0;
						srcp1 = srcp;
						while((c2 = *srcp) != '\0' && maxlen > 0) {
							if(c2 >= '0' && (c2 <= '7' || (c2 <= '9' && base != 8)))
								c = c * base + (c2 - '0');
							else {
								c2 = lowcase[c2];
								if(base == 16 && (c2 >= 'a' && c2 <= 'f'))
									c = c * 16 + (c2 - ('a' - 10));
								else
									break;
								}

							// Char overflow?
							if(c > 0xff)
								return rcset(FAILURE,0,text337,strsamp(srcp0,strlen(srcp0),
								 (size_t) term.t_ncol * 3 / 10));
									// "Invalid \\nn sequence in string %s"
							++srcp;
							--maxlen;
							}

						// Any digits decoded?
						if(srcp == srcp1)
							goto litchar;

						// Valid sequence.
						break;
					default:	// Literal character.
litchar:
						c = srcp[-1];
						goto saveit;
					}
				}
			}

		// Not a backslash.  Check for beginning of interpolation.
		else if(termch == '"' && c == TKC_EXPR && srcp[1] == TKC_EXPRBEG) {
			Value *vp;

			// "#{" found.  Execute what follows to "}" as a command line.
			if(vnew(&vp,false) != 0)
				return vrcset();
			if(doestmt(vp,srcp + 2,TKC_EXPREND,&srcp) != SUCCESS)
				return rc.status;

			// Append the result to dest.
			if((opflags & OPEVAL) && vputv(vp,&result) != 0)
				return vrcset();

			// Success.
			++srcp;
			continue;
			}
		else
			// Not an interpolated expression.  Vanilla character.
			++srcp;
saveit:
		// Save the character.
		if((opflags & OPEVAL) && vputc(c,&result) != 0)
			return vrcset();
		}

	// Terminate the result and return.
	if((opflags & OPEVAL) && vclose(&result) != 0)
		return vrcset();
	return getsym();
	}

// List the names of all the functions (interactive only).  If default n, make full list; otherwise, get a match string and make
// partial list of function names that contain it, ignoring case.  Render buffer and return status.
int showFunctions(Value *rp,int n) {
	Buffer *flistp;			// Buffer to put function list into.
	CmdFunc *cfp;
	bool first,doapropos;
	StrList rpt;
	Value *mstrp;			// Match string.
	char *strp,wkbuf[NWORK];

	// If not default n, get match string.
	if((doapropos = n != INT_MIN)) {
		if(vnew(&mstrp,false) != 0)
			return vrcset();
		if(apropos(mstrp,text247) != SUCCESS)
				// "function"
			return rc.status;
		}

	// Get a buffer for the function list and open a string list.
	if(sysbuf(text211,&flistp) != SUCCESS)
			// "FunctionList"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	// Build the function list.
	first = true;
	cfp = cftab;
	do {
		// Skip if a command.
		if(!(cfp->cf_flags & CFFUNC))
			continue;

		// Skip if an apropos and function name doesn't contain the search string.
		strp = stpcpy(wkbuf,cfp->cf_name);
		if(doapropos && strcasestr(wkbuf,mstrp->v_strp) == NULL)
			continue;

		// Store function name, args, and description.
		*strp++ = ' ';
		strcpy(strp,cfp->cf_usage);
		pad(wkbuf,29);
		if(!first && vputc('\r',&rpt) != 0)
			return vrcset();
		if(vputs(wkbuf,&rpt) != 0 || vputs(cfp->cf_desc,&rpt) != 0)
			return vrcset();
		first = false;
		} while((++cfp)->cf_name != NULL);

	// Add the results to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(!visnull(rpt.sl_vp) && bappend(flistp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display the list.
	return render(rp,n < 0 ? -2 : n,flistp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}
