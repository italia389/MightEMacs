// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// eval.c	High-level expression evaluation routines for MightEMacs.

#include "os.h"
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "pllib.h"
#include "std.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "file.h"
#include "search.h"
#include "var.h"

// Definitions for fmtarg() and strfmt().
#define FMTBufSz	32		// Size of temporary buffer.

// Flags.
#define FMTleft		0x0001		// Left-justify.
#define	FMTplus		0x0002		// Force plus (+) sign.
#define	FMTspc		0x0004		// Use ' ' for plus sign.
#define FMThash		0x0008		// Alternate conversion.
#define	FMTlong		0x0010		// 'l' flag.
#define FMT0pad		0x0020		// '0' flag.
#define FMTprec		0x0040		// Precision was specified.
#define FMTxuc		0x0080		// Use upper case hex letters.

// Control record used to process array arguments.
typedef struct {
	Array *ary;
	ArraySize i;
	} ArrayState;

// Forwards.
int dtosf(DStrFab *dest,Datum *src,char *dlm,uint flags);

// Return a datum object as a logical (Boolean) value.
bool tobool(Datum *datum) {

	// Check for numeric truth (!= 0).
	if(datum->d_type == dat_int)
		return datum->u.d_int != 0;

	// Check for logical false values (false and nil).  All other strings (including null strings) are true.
	return datum->d_type != dat_false && datum->d_type != dat_nil;
	}

// Check if given datum object is nil or null and return Boolean result.
bool disnn(Datum *datum) {

	return datum->d_type == dat_nil || disnull(datum);
	}

// Write an array to dest (an active string-fab object) via calls to dtosf() and return status.  If CvtExpr flag is set, array
// is written in "[...]" form so that result can be subsequently evaluated as an expression; otherwise, it is written as data.
// In the latter case, elements are separated by "dlm" delimiters if not NULL.  A nil argument is included in result if CvtExpr
// or CvtKeepNil flag is set, and a null argument is included if CvtExpr or CvtKeepNull flag is set; otherwise, they are
// skipped.  In all cases, if the array includes itself, recursion stops and "[...]" is written for array if CvtForceArray flag
// is set; otherwise, an error is set.
static int atosf(DStrFab *dest,Datum *src,char *dlm,uint flags) {
	ArrayWrapper *aw = awptr(src);

	// Array includes self?
	if(aw->aw_mark) {

		// Yes.  Store an ellipsis or set an error.
		if(!(flags & CvtForceArray))
			(void) rcset(Failure,RCNoFormat,text195);
				// "Endless recursion detected (array contains itself)"
		else if(dputs("[...]",dest) != 0)
			goto LibFail;
		}
	else {
		Datum *datum;
		Array *ary = aw->aw_ary;
		Datum **elp = ary->a_elp;
		ArraySize n = ary->a_used;
		bool first = true;
		char *realdlm = (flags & CvtExpr) ? "," : dlm;
		aw->aw_mark = true;
		if(flags & CvtExpr) {
			flags |= CvtKeepAll;
			if(dputc('[',dest) != 0)
				goto LibFail;
			}

		while(n-- > 0) {
			datum = *elp++;

			// Skip nil or null string if appropriate.
			if(datum->d_type == dat_nil) {
				if(!(flags & CvtKeepNil))
					continue;
				}
			else if(disnull(datum) && !(flags & CvtKeepNull))
				continue;

			// Write optional delimiter and encoded value.
			if(!first && realdlm != NULL && dputs(realdlm,dest) != 0)
				goto LibFail;
			if(dtosf(dest,datum,dlm,flags) != Success)
				return rc.status;
			first = false;
			}
		if((flags & CvtExpr) && dputc(']',dest) != 0)
			goto LibFail;
		}

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Add an array to wrapper list, clear all "marked" flags, and call atosf().
int atosfclr(DStrFab *dest,Datum *src,char *dlm,uint flags) {

	agarbpush(src);
	aclrmark();
	return atosf(dest,src,dlm,flags);
	}

// Write a datum object to dest (an active string-fab object) in string form and return status.  If CvtExpr flag is set, quote
// strings and write nil values as keywords.  (Output is intended to be subsequently evaluated as an expression.)  If CvtExpr
// flag is not set, write strings in encoded (visible) form if CvtVizStr flag is set, and enclose in single (') or double (")
// quotes if CvtQuote1 or CvtQuote2 flag is set; otherwise, unmodified, and write nil values as a keyword if CvtShowNil flag is
// set; otherwise, a null string.  In all cases, write Boolean values as keywords, and call atosf() with dlm and flag arguments
// to write arrays.
int dtosf(DStrFab *dest,Datum *src,char *dlm,uint flags) {
	char *str;

	// Determine type of datum object.
	if(src->d_type & DStrMask) {
		Datum *value = src;
		if(flags & CvtTermAttr) {
			DStrFab val;

			if(dopentrk(&val) != 0 || escattrtosf(&val,src->d_str) != 0 || dclose(&val,sf_string) != 0)
				return librcset(Failure);
			value = val.sf_datum;
			}
		if(flags & CvtExpr)
			(void) quote(dest,value->d_str,true);
		else {
			short qc = flags & CvtQuote1 ? '\'' : flags & CvtQuote2 ? '"' : -1;
			if((flags & (CvtQuote1 | CvtQuote2)) && dputc(qc,dest) != 0)
				goto LibFail;
			if(flags & CvtVizStr) {
				if(dvizs(value->d_str,0,VBaseDef,dest) != 0)
					goto LibFail;
				}
			else if(dputs(value->d_str,dest) != 0)
				goto LibFail;
			if((flags & (CvtQuote1 | CvtQuote2)) && dputc(qc,dest) != 0)
				goto LibFail;
			}
		}
	else
		switch(src->d_type) {
			case dat_int:
				if(dputf(dest,"%ld",src->u.d_int) != 0)
					goto LibFail;
				break;
			case dat_blobRef:	// Array
				(void) atosf(dest,src,dlm,flags);
				break;
			case dat_nil:
				if(flags & (CvtExpr | CvtShowNil)) {
					str = viz_nil;
					goto Viz;
					}
				break;
			default:		// Boolean
				str = (src->d_type == dat_false) ? viz_false : viz_true;
Viz:
				if(dputs(str,dest) != 0)
					goto LibFail;
			}

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Call atosfclr() if array so that "marked" flags in wrapper list are cleared first; otherwise, call dtosf().
int dtosfchk(DStrFab *dest,Datum *src,char *dlm,uint flags) {
	int (*func)(DStrFab *dest,Datum *src,char *dlm,uint flags) = (src->d_type == dat_blobRef) ? atosfclr : dtosf;
	return func(dest,src,dlm,flags);
	}

// Create an array in rval, given optional size and initializer.  Return status.
int array(Datum *rval,int n,Datum **argv) {
	Array *ary;
	ArraySize len = 0;
	Datum *ival = NULL;

	// Get array size and initializer, if present.
	if(*argv != NULL) {
		len = (*argv++)->u.d_int;
		if(*argv != NULL)
			ival = *argv;
		}
	if((ary = anew(len,ival)) == NULL)
		return librcset(Failure);
	if(awrap(rval,ary) != Success)
		return rc.status;

	// Create unique arrays if initializer is an array.
	if(len > 0 && ival != NULL && ival->d_type == dat_blobRef) {
		Datum **elp = ary->a_elp;
		do {
			if(aryclone(*elp++,ival,0) != Success)
				return rc.status;
			} while(--len > 0);
		}

	return rc.status;
	}

// Split a string into an array and save in rval, given delimiter and optional limit value.  Return status.
int ssplit(Datum *rval,int n,Datum **argv) {
	Array *ary;
	short delim;
	char *str;
	int limit = 0;

	// Get delimiter, string, and optional limit.
	if(!charval(*argv))
		return rc.status;
	if((delim = (*argv++)->u.d_int) == '\0')
		return rcset(Failure,0,text187,text409);
			// "%s cannot be null","Delimiter"
	str = (*argv++)->d_str;
	if(*argv != NULL)
		limit = (*argv)->u.d_int;

	return (ary = asplit(delim,str,limit)) == NULL ? librcset(Failure) : awrap(rval,ary);
	}

// Copy string from src to dest (an active string-fab object), adding a double quote (") at beginning and end (if full is true)
// and escaping all control characters, backslashes, and characters that are escaped by parsesym().  Return status.
int quote(DStrFab *dest,char *src,bool full) {
        short c;
	bool ischar;
	char *str;
	char wkbuf[8];

	if(full) {
		c = '"';
		goto LitChar;
		}

	while((c = *src++) != '\0') {
		ischar = false;
		switch(c) {
			case '"':				// Double quote
				if(!full)
					goto LitChar;
				str = "\\\""; break;
			case '\\':				// Backslash
				str = "\\\\"; break;
			case '\r':				// CR
				str = "\\r"; break;
			case '\n':				// NL
				str = "\\n"; break;
			case '\t':				// Tab
				str = "\\t"; break;
			case '\b':				// Backspace
				str = "\\b"; break;
			case '\f':				// Form feed
				str = "\\f"; break;
			case 033:				// Escape
				str = "\\e"; break;
			default:
				if(c < ' ' || c >= 0x7F)	// Non-printable character.
					sprintf(str = wkbuf,"\\%.3o",c);
				else				// Literal character.
LitChar:
					ischar = true;
			}

		if((ischar ? dputc(c,dest) : dputs(str,dest)) != 0)
			return librcset(Failure);
		}
	if(full && dputc('"',dest) != 0)
		(void) librcset(Failure);

	return rc.status;
	}

// Force NULL pointer to null string.
char *fixnull(char *s) {

	return (s == NULL) ? "" : s;
	}

// Do range check for hard or soft tab size.
int chktab(int size,bool hard) {

	return ((size != 0 || hard) && (size < 2 || size > MaxTab)) ?
	 rcset(Failure,0,text256,hard ? text49 : text50,size,MaxTab) :
		// "%s tab size %d must be between 2 and %d","Hard","Soft"
	 rc.status;
	}

// Set hard or soft tab size and do range check.
int settab(int size,bool hard) {

	// Check if new tab size is valid.
	if(chktab(size,hard) == Success) {

		// Set new size.
		if(hard)
			si.htabsize = size;
		else
			si.stabsize = size;
		(void) rcset(Success,0,text332,hard ? text49 : text50,size);
				// "%s tab size set to %d","Hard","Soft"
		}

	return rc.status;
	}

// Initialize match object.
void minit(Match *match) {
	GrpInfo *gip,*gipz;

	match->flags = 0;
	match->ssize = match->rsize = 0;
	gipz = (gip = match->groups) + MaxGroups;
	do {
		gip->match = NULL;
		} while(++gip < gipz);
	match->match = NULL;
	}

// Strip whitespace off the beginning (op == -1), the end (op == 1), or both ends (op == 0) of a string.
char *stripstr(char *src,int op) {

	// Trim beginning, if applicable...
	if(op <= 0)
		src = nonwhite(src,false);

	// trim end, if applicable...
	if(op >= 0) {
		char *srcz = strchr(src,'\0');
		while(srcz-- > src)
			if(*srcz != ' ' && *srcz != '\t')
				break;
		srcz[1] = '\0';
		}

	// and return result.
	return src;
	}

// Substitute first occurrence (or all if n > 1) of sstr in sp with rstr and store results in rval.  Ignore case in comparisons
// if flag set in "flags".  Return status.
int strsub(Datum *rval,int n,Datum *sp,char *sstr,char *rstr,ushort flags) {
	int rcode;
	DStrFab dest;
	char *(*strf)(const char *s1,const char *s2);
	char *str = sp->d_str;

	// Return source string if sp or sstr is empty.
	if(*str == '\0' || *sstr == '\0')
		rcode = dsetstr(str,rval);
	else if((rcode = dopenwith(&dest,rval,SFClear)) == 0) {
		int srclen,sstrlen,rstrlen;
		char *s;

		strf = flags & SOpt_Ignore ? strcasestr : strstr;
		sstrlen = strlen(sstr);
		rstrlen = strlen(rstr);

		for(;;) {
			// Find next occurrence.
			if((s = strf(str,sstr)) == NULL)
				break;

			// Compute offset and copy prefix.
			if((srclen = s - str) > 0 && dputmem((void *) str,srclen,&dest) != 0)
				goto LibFail;
			str = s + sstrlen;

			// Copy substitution string.
			if(dputmem((void *) rstr,rstrlen,&dest) != 0)
				goto LibFail;

			// Bail out unless n > 1.
			if(n <= 1)
				break;
			}

		// Copy remainder, if any.
		if((srclen = strlen(str)) > 0 && dputmem((void *) str,srclen,&dest) != 0)
			goto LibFail;
		rcode = dclose(&dest,sf_string);
		}

	if(rcode == 0)
		return rc.status;
LibFail:
	return librcset(Failure);
	}

#if 0
// Return string in visible form and with space characters exposed.
static char *zstr(char *str) {
	static char wkbuf[256];
	char *wb = wkbuf;
	do {
		wb = stpcpy(wb,vizc(*str,VSpace));
		} while(*str++ != '\0');
	return wkbuf;
	}
#endif

// Perform RE substitution(s) in string sp using search pattern spat and replacement pattern rpat.  Save result in rval.  Do
// all occurrences of the search pattern if n > 1; otherwise, first only.  Return status.
static int resub(Datum *rval,int n,Datum *sp,char *spat,char *rpat,ushort flags) {
	DStrFab dest;
	Match match;
	int offset,scanoff;
	ReplMetaChar *rmc;
	StrLoc *strloc;
	size_t len,lastscanlen;
	ulong loopcount;

	// Return null string if sp is empty.
	if(disnull(sp)) {
		dsetnull(rval);
		return rc.status;
		}

	// Error if search pattern is null.
	if(*spat == '\0')
		return rcset(Failure,0,text187,text266);
			// "%s cannot be null","Regular expression"

	// Save and compile patterns in local "match" variable.
	minit(&match);
	if(newspat(spat,&match,&flags) != Success)
		return rc.status;
	if(mccompile(&match) != Success || newrpat(rpat,&match) != Success)
		goto SFree;
	if(rmccompile(&match) != Success)
		goto RFree;

	// Begin scan loop.  For each match found in sp, perform substitution and use string result in rval as source string
	// (with an offset) for next iteration.  This is necessary for RE matching to work correctly.
	strloc = &match.groups[0].ml.str;
	loopcount = lastscanlen = scanoff = 0;
	for(;;) {
		// Find next occurrence.
		if(recmp(sp,scanoff,&match,&offset) != Success)
			break;
		if(offset >= 0) {
			// Match found.  Error if we matched an empty string and scan position did not advance; otherwise, we'll
			// go into an infinite loop.
			if(++loopcount > 2 && strloc->len == 0 && strlen(sp->d_str + scanoff) == lastscanlen) {
				(void) rcset(Failure,RCNoFormat,text91);
					// "Repeating match at same position detected"
				break;
				}

			// Open string-fab object.
			if(dopenwith(&dest,rval,SFClear) != 0)
				goto LibFail;

			// Copy any source text that is before the match location.
			if(offset > 0 && dputmem((void *) sp->d_str,offset,&dest) != 0)
				goto LibFail;

			// Copy replacement pattern to dest.
			if(match.flags & RRegical)
				for(rmc = match.rmcpat; rmc->mc_type != MCE_Nil; ++rmc) {
					if(dputs(rmc->mc_type == MCE_LitString ? rmc->u.rstr :
					 rmc->mc_type == MCE_Match ? match.match->d_str :
					 fixnull(match.groups[rmc->u.grpnum].match->d_str),&dest) != 0)
						goto LibFail;
					}
			else if(dputs(match.rpat,&dest) != 0)
				goto LibFail;

			// Copy remaining source text to dest if any, and close it.
			if(((len = strlen(sp->d_str + offset + strloc->len)) > 0 &&
			 dputmem((void *) (sp->d_str + offset + strloc->len),len,&dest) != 0) || dclose(&dest,sf_string) != 0) {
LibFail:
				(void) librcset(Failure);
				break;
				}

			// If no text remains or repeat count reached, we're done.
			if(len == 0 || n <= 1)
				break;

			// In "find all" mode... keep going.
			lastscanlen = strlen(sp->d_str) - scanoff;
			scanoff = strlen(rval->d_str) - len;
			datxfer(sp,rval);
			}
		else {
			// No match found.  Transfer input to result and bail out.
			datxfer(rval,sp);
			break;
			}
		}
RFree:
	// Finis.  Free pattern space and return.
	freerpat(&match);
SFree:
	freespat(&match);
	return rc.status;
	}

// Expand character ranges and escaped characters (if any) in a string.  Return status.
int strexpand(DStrFab *strloc,char *estr) {
	short c1,c2;
	char *str;

	if(dopentrk(strloc) != 0)
		goto LibFail;
	str = estr;
	do {
		switch(c1 = *str) {
			case '-':
				if(str == estr || (c2 = str[1]) == '\0')
					goto LitChar;
				if(c2 < (c1 = str[-1]))
					return rcset(Failure,0,text2,str - 1,estr);
						// "Invalid character range '%.3s' in string '%s'"
				while(++c1 <= c2)
					if(dputc(c1,strloc) != 0)
						goto LibFail;
				++str;
				break;
			case '\\':
				if(str[1] != '\0')
					c1 = *++str;
				// Fall through.
			default:
LitChar:
				if(dputc(c1,strloc) != 0)
					goto LibFail;
			}
		} while(*++str != '\0');

	if(dclose(strloc,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Prepare tr from and to strings.  Return status.
static int trprep(Datum *xfrom,Datum *xto) {
	DStrFab sfab;

	// Expand "from" string.
	if(strexpand(&sfab,xfrom->d_str) != Success)
		return rc.status;
	datxfer(xfrom,sfab.sf_datum);

	// Expand "to" string.
	if(xto->d_type == dat_nil)
		dsetnull(xto);
	else if(*xto->d_str != '\0') {
		int lenfrom,lento;

		if(strexpand(&sfab,xto->d_str) != Success)
			return rc.status;
		datxfer(xto,sfab.sf_datum);

		if((lenfrom = strlen(xfrom->d_str)) > (lento = strlen(xto->d_str))) {
			short c = xto->d_str[lento - 1];
			int n = lenfrom - lento;

			if(dopenwith(&sfab,xto,SFAppend) != 0)
				goto LibFail;
			do {
				if(dputc(c,&sfab) != 0)
					goto LibFail;
				} while(--n > 0);
			if(dclose(&sfab,sf_string) != 0)
				goto LibFail;
			}
		}
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Translate a string, given result pointer, source pointer, translate-from string, and translate-to string.  Return status.
// The translate-to string may be null, in which case all translate-from characters are deleted from the result.  It may also
// be shorter than the translate-from string, in which case it is padded to the same length with its last character.
static int tr(Datum *rval,Datum *src,Datum *xfrom,Datum *xto) {
	int lento;
	char *xf;
	char *str;
	DStrFab result;

	// Validate arguments.
	if(strlen(xfrom->d_str) == 0)
		return rcset(Failure,0,text187,text328);
				// "%s cannot be null","tr \"from\" string"
	if(trprep(xfrom,xto) != Success)
		return rc.status;

	// Scan source string.
	if(dopenwith(&result,rval,SFClear) != 0)
		goto LibFail;
	str = src->d_str;
	lento = strlen(xto->d_str);
	while(*str != '\0') {

		// Scan lookup table for a match.
		xf = xfrom->d_str;
		do {
			if(*str == *xf) {
				if(lento > 0 && dputc(xto->d_str[xf - xfrom->d_str],&result) != 0)
					goto LibFail;
				goto Next;
				}
			} while(*++xf != '\0');

		// No match, copy the source char untranslated.
		if(dputc(*str,&result) != 0)
			goto LibFail;
Next:
		++str;
		}

	// Terminate and return the result.
	if(dclose(&result,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Concatenate all function arguments into rval if runtime flag OpEval is set; otherwise, just consume them.  reqct is the
// number of required arguments.  ArgFirst flag is set on first argument.  Null and nil arguments are included in result if
// CvtKeepNil and/or CvtKeepNull flags are set; otherwise, they are skipped.  A nil argument is output as a keyword if
// CvtShowNil flag is set; otherwise, a null string.  Boolean arguments are always output as "false" and "true" and arrays are
// processed (recursively) as if each element was specified as an argument.  Return status.
int catargs(Datum *rval,int reqct,Datum *delim,uint flags) {
	uint aflags = ArgFirst | ArgBool1 | ArgArray1 | ArgNIS1;
	DStrFab sfab;
	Datum *datum;		// For arguments.
	bool firstWrite = true;
	char *dlm = (delim != NULL && !disnn(delim)) ? delim->d_str : NULL;

	// Nothing to do if not evaluating and no arguments; for example, an "abort()" call.
	if((si.opflags & (OpScript | OpParens)) == (OpScript | OpParens) && havesym(s_rparen,false) &&
	 (!(si.opflags & OpEval) || reqct == 0))
		return rc.status;

	if(dnewtrk(&datum) != 0 || ((si.opflags & OpEval) && dopenwith(&sfab,rval,SFClear) != 0))
		goto LibFail;

	for(;;) {
		if(aflags & ArgFirst) {
			if(!havesym(s_any,reqct > 0))
				break;				// Error or no arguments.
			}
		else if(!havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(datum,aflags) != Success)
			return rc.status;
		--reqct;
		if(si.opflags & OpEval) {

			// Skip nil or null string if appropriate.
			if(datum->d_type == dat_nil) {
				if(!(flags & CvtKeepNil))
					goto Onward;
				}
			else if(disnull(datum) && !(flags & CvtKeepNull))
				goto Onward;

			// Write optional delimiter and value.
			if(dlm != NULL && !firstWrite && dputs(dlm,&sfab) != 0)
				goto LibFail;
			if(dtosfchk(&sfab,datum,dlm,flags) != Success)
				return rc.status;
			firstWrite = false;
			}
Onward:
		aflags = ArgBool1 | ArgArray1 | ArgNIS1;
		}

	// Return result.
	if((si.opflags & OpEval) && dclose(&sfab,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Process a strPop, strPush, strShift, or strUnshift function and store result in rval if evaluating; otherwise, just "consume"
// arguments.  Set rval to nil if strShift or strPop and no items left.  Return status.
static int strfunc(Datum *rval,enum cfid fid,char *fname) {
	int status;
	VDesc vd;
	char *str1,*str2,*newvarval;
	short dlm = 0;
	bool spacedlm = false;
	Datum *delim,*oldvarval,*arg,newvar,*newvarp;

	// Syntax of functions:
	//	strPush var,dlm,val    strPop var,dlm    strShift var,dlm    strUnshift var,dlm,val

	// Get variable name from current symbol, find it and its value, and validate it.
	if(!havesym(s_any,true))
		return rc.status;
	if(si.opflags & OpEval) {
		if(dnewtrk(&oldvarval) != 0)
			goto LibFail;
		if(findvar(str1 = last->p_tok.d_str,&vd,OpDelete) != Success)
			return rc.status;
		if((vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_RdOnly)) ||
		 (vd.vd_type == VTyp_NVar && vd.i.vd_argnum == 0))
			return rcset(Failure,RCTermAttr,text164,str1);
				// "Cannot modify read-only variable '~b%s~B'"
		if(vderefv(oldvarval,&vd) != Success)
			return rc.status;

		// Have var value in oldvarval.  Verify that it is nil or string.
		if(oldvarval->d_type == dat_nil)
			dsetnull(oldvarval);
		else if(!strval(oldvarval))
			return rc.status;
		}

	// Get delimiter into delim and also dlm if single byte (strShift and strPop).
	if(dnewtrk(&delim) != 0)
		goto LibFail;
	if(getsym() < NotFound ||
	 funcarg(delim,(fid == cf_strShift || fid == cf_strPop) ? (ArgInt1 | ArgNil1 | ArgMay) : ArgNil1) != Success)
		return rc.status;
	if(si.opflags & OpEval) {
		if(fid == cf_strShift || fid == cf_strPop) {
			if(delim->d_type == dat_nil)
				dlm = '\0';
			else if(!charval(delim))
				return rc.status;
			else if((dlm = delim->u.d_int) == ' ')
				spacedlm = true;
			}
		else if(delim->d_type == dat_nil)
			dsetnull(delim);
		}

	// Get value argument into arg for strPush and strUnshift functions.
	if(fid == cf_strPush || fid == cf_strUnshift) {
		if(dnewtrk(&arg) != 0)
			goto LibFail;
		if(funcarg(arg,ArgNIS1) != Success)
			return rc.status;
		}

	// If not evaluating, we're done (all arguments consumed).
	if(!(si.opflags & OpEval))
		return rc.status;

	// Evaluating.  Convert value argument to string.
	if((fid == cf_strPush || fid == cf_strUnshift) && tostr(arg) != Success)
		return rc.status;

	// Function value is in arg (if strPush or strUnshift) and "old" var value is in oldvarval.  Do function-specific
	// operation.  Copy parsed token to rval if strPop or strShift.  Set newvarval to new value of var in all cases.
	switch(fid) {
		case cf_strPop:					// Get last token from old var value into rval.

			// Value	str1		str2		str3		Correct result
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

			if(*(newvarval = oldvarval->d_str) == '\0')	// Null var value?
				status = NotFound;
			else {
				str1 = strchr(newvarval,'\0');
				status = rparsetok(rval,&str1,newvarval,spacedlm ? -1 : dlm);
				}
			goto Check;
		case cf_strShift:
			newvarval = oldvarval->d_str;
			status = parsetok(rval,&newvarval,spacedlm ? -1 : dlm);
Check:
			dinit(newvarp = &newvar);
			dsetstrref(newvarval,newvarp);

			if(status != Success) {			// Any tokens left?
				if(rc.status != Success)
					return rc.status;	// Fatal error.

				// No tokens left.  Signal end of token list.
				dsetnil(rval);
				goto Retn;
				}

			// We have a token.
			if(fid == cf_strPop) {
				if(str1 <= newvarval)		// Just popped last token?
					*newvarval = '\0';	// Yes, clear variable.
				else
					*str1 = '\0';		// Not last.  Chop old at current spot (delimiter).
				}
			break;
		case cf_strPush:
			str1 = oldvarval->d_str;		// First portion of new var value (old var value).
			str2 = arg->d_str;			// Second portion (value to append).
			goto Paste;
		default:	// cf_strUnshift
			str1 = arg->d_str;			// First portion of new var value (value to prepend).
			str2 = oldvarval->d_str;		// Second portion (old var value).
Paste:
			{DStrFab sfab;

			if(dopenwith(&sfab,rval,SFClear) != 0 ||
			 dputs(str1,&sfab) != 0)			// Copy initial portion of new var value to work buffer.
				goto LibFail;

			// Append a delimiter if oldvarval is not null, and value (if strPush) or var (if strUnshift).
			if((!disnull(oldvarval) && dputs(delim->d_str,&sfab) != 0) || dputs(str2,&sfab) != 0 ||
			 dclose(&sfab,sf_string) != 0)
				goto LibFail;
			newvarp = rval;				// New var value.
			}
			break;
			}

	// Update variable and return status.
	(void) putvar(newvarp,&vd);
Retn:
	return rc.status;
LibFail:
	return librcset(Failure);
	}

#if (MMDebug & Debug_Token) && 0
static void showsym(char *name) {

	fprintf(logfile,"%s(): last is str \"%s\" (%d)\n",name,last->p_tok.d_str,last->p_sym);
	}
#endif

// Insert, overwrite, replace, or write the text in src to a buffer n times.  This routine's job is to make the given buffer
// the current buffer so that iorstr() can insert the text, then restore the current screen to its original form.  src, n, and
// style are passed to iorstr().  If buf is NULL, the current buffer is used.  Return status.
int iortext(char *src,int n,ushort style,Buffer *buf) {
	EScreen *oldscr = NULL;
	EWindow *oldwin = NULL;
	Buffer *oldbuf = NULL;

	// If the target buffer is being displayed in another window, remember current window and move to the other one;
	// otherwise, switch to the buffer in the current window.
	if(buf != NULL && buf != si.curbuf) {
		if(buf->b_nwind == 0) {

			// Target buffer is not being displayed in any window... switch to it in current window.
			oldbuf = si.curbuf;
			if(bswitch(buf,SWB_NoHooks) != Success)
				return rc.status;
			}
		else {
			// Target buffer is being displayed.  Get window and find screen.
			EWindow *win = findwind(buf);
			EScreen *scr = si.shead;
			EWindow *win2;
			oldwin = si.curwin;
			do {
				win2 = scr->s_whead;
				do {
					if(win2 == win)
						goto Found;
					} while((win2 = win2->w_next) != NULL);
				} while((scr = scr->s_next) != NULL);
Found:
			// If screen is different, switch to it.
			if(scr != si.curscr) {
				oldscr = si.curscr;
				if(sswitch(scr,SWB_NoHooks) != Success)
					return rc.status;
				}

			// If window is different, switch to it.
			if(win != si.curwin) {
				(void) wswitch(win,SWB_NoHooks);		// Can't fail.
				supd_wflags(NULL,WFMode);
				}
			}
		}

	// Target buffer is current.  Now insert, overwrite, or replace the text in src n times.
	if(iorstr(src,n,style,NULL) == Success) {

		// Restore old screen, window, and/or buffer, if needed.
		if(oldbuf != NULL)
			(void) bswitch(oldbuf,SWB_NoHooks);
		else if(oldscr != NULL) {
			if(sswitch(oldscr,SWB_NoHooks) != Success)
				return rc.status;
			if(oldwin != si.curwin)
				goto RestoreWind;
			}
		else if(oldwin != NULL) {
RestoreWind:
			(void) wswitch(oldwin,SWB_NoHooks);		// Can't fail.
			supd_wflags(NULL,WFMode);
			}
		}

	return rc.status;
	}

// Concatenate command-line arguments into rval and insert, overwrite, replace, or write the resulting text to a buffer n times,
// given text insertion style and buffer pointer.  If buf is NULL, use current buffer.  If n == 0, do one repetition and don't
// move point.  If n < 0, do one repetition and process all newline characters literally (don't create a new line).  Return
// status.
int chgtext(Datum *rval,int n,ushort style,Buffer *buf) {
	Datum *dtext;
	DStrFab text;
	uint aflags = ArgFirst | ArgBool1 | ArgArray1 | ArgNIS1;

	if(n == INT_MIN)
		n = 1;

	if(dnewtrk(&dtext) != 0)
		goto LibFail;

	// Evaluate all the arguments and save in string-fab object (so that the text can be inserted more than once,
	// if requested).
	if(dopenwith(&text,rval,SFClear) != 0)
		goto LibFail;

	for(;;) {
		if(!(aflags & ArgFirst) && !havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(dtext,aflags) != Success)
			return rc.status;
		aflags = ArgBool1 | ArgArray1 | ArgNIS1;
		if(disnn(dtext))
			continue;				// Ignore null and nil values.
		if(dtext->d_type == dat_blobRef || (dtext->d_type & DBoolMask)) {
			if(dtosfchk(&text,dtext,NULL,0) != Success)
				return rc.status;
			}
		else if(dputd(dtext,&text) != 0)		// Add text chunk to string-fab object.
			goto LibFail;
		}
	if(dclose(&text,sf_string) != 0)
		goto LibFail;

	// We have all the text in rval.  Now insert, overwrite, or replace it n times.
	return iortext(rval->d_str,n,style,buf);
LibFail:
	return librcset(Failure);
	}

// Process stat? function.  Return status.
static int ftest(Datum *rval,int n,Datum *file,Datum *tcode) {

	if(disnull(tcode))
		(void) rcset(Failure,0,text187,text335);
			// "%s cannot be null","File test code(s)"
	else {
		struct stat s;
		bool result;
		char *str,tests[] = "defLlrswx";

		// Validate test code(s).
		for(str = tcode->d_str; *str != '\0'; ++str) {
			if(strchr(tests,*str) == NULL)
				return rcset(Failure,0,text362,*str);
					// "Unknown file test code '%c'"
			}

		// Get file status.
		if(lstat(file->d_str,&s) != 0)
			result = false;
		else {
			int flag;
			bool match;
			result = (n != INT_MIN);

			// Loop through test codes.
			for(str = tcode->d_str; *str != '\0'; ++str) {
				switch(*str) {
					case 'd':
						flag = S_IFDIR;
						goto TypChk;
					case 'e':
						match = true;
						break;
					case 'f':
						flag = S_IFREG;
						goto TypChk;
					case 'r':
					case 'w':
					case 'x':
						match = access(file->d_str,*str == 'r' ? R_OK :
						 *str == 'w' ? W_OK : X_OK) == 0;
						break;
					case 's':
						match = s.st_size > 0;
						break;
					case 'L':
						flag = S_IFLNK;
TypChk:
						match = (s.st_mode & S_IFMT) == flag;
						break;
					default:	// 'l'
						match = (s.st_mode & S_IFMT) == S_IFREG && s.st_nlink > 1;
					}
				if(match) {
					if(n == INT_MIN) {
						result = true;
						break;
						}
					}
				else if(n != INT_MIN) {
					result = false;
					break;
					}
				}
			}
		dsetbool(result,rval);
		}

	return rc.status;
	}

// Return next argument to strfmt(), "flattening" arrays in the process.  Return status.
static int fmtarg(Datum *rval,uint aflags,ArrayState *asp) {

	for(;;) {
		if(asp->ary == NULL) {
			if(funcarg(rval,aflags | ArgArray1 | ArgMay) != Success)
				return rc.status;
			if(rval->d_type != dat_blobRef)
				break;
			asp->ary = awptr(rval)->aw_ary;
			asp->i = 0;
			}
		else {
			Array *ary = asp->ary;
			if(asp->i == ary->a_used)
				asp->ary = NULL;
			else {
				if(datcpy(rval,ary->a_elp[asp->i]) != 0)
					return librcset(Failure);
				++asp->i;
				break;
				}
			}
		}

	if(aflags == ArgInt1)
		(void) intval(rval);
	else if(aflags == ArgNil1 && rval->d_type != dat_nil)
		(void) strval(rval);
	return rc.status;
	}

// Build string from "printf" format string (format) and argument(s).  If arg1p is not NULL, process binary format (%)
// expression using arg1p as the argument; otherwise, process printf, sprintf, or bprintf function.
// Return status.
int strfmt(Datum *rval,Datum *format,Datum *arg1p) {
	short c;
	int specCount;
	ulong ul;
	char *prefix,*fmt;
	int prefLen;		// Length of prefix string.
	int width;		// Field width.
	int padding;		// # of chars to pad (on left or right).
	int precision;
	char *str;
	int sLen;		// Length of formatted string s.
	int flags;		// FMTxxx.
	int base;		// Number base (decimal, octal, etc.).
	Datum *tp;
	DStrFab result;
	ArrayState as = {NULL,0};
	char wkbuf[FMTBufSz];

	fmt = format->d_str;

	// Create string-fab object for result and work Datum object for fmtarg() calls.
	if(dopenwith(&result,rval,SFClear) != 0 || (arg1p == NULL && dnewtrk(&tp) != 0))
		goto LibFail;

	// Loop through format string.
	specCount = 0;
	while((c = *fmt++) != '\0') {
		if(c != '%') {
			if(dputc(c,&result) != 0)
				goto LibFail;
			continue;
			}

		// Check for prefix(es).
		prefix = NULL;					// Assume no prefix.
		flags = 0;					// Reset.
		while((c = *fmt++) != '\0') {
			switch(c) {
				case '0':
					flags |= FMT0pad; 	// Pad with 0's.
					break;
				case '-':
					flags |= FMTleft; 	// Left-justify.
					break;
				case '+':
					flags |= FMTplus; 	// Do + or - sign.
					break;
				case ' ':
					flags |= FMTspc;	// Space flag.
					break;
				case '#':
					flags |= FMThash; 	// Alternate form.
					break;
				default:
					goto GetWidth;
				}
			}

		// Get width.
GetWidth:
		width = 0;
		if(c == '*') {
			if(arg1p != NULL)			// Error if format op (%).
				goto TooMany;
			if(fmtarg(tp,ArgInt1,&as) != Success)	// Get next (int) argument for width.
				return rc.status;
			if((width = tp->u.d_int) < 0) {		// Negative field width?
				flags |= FMTleft;		// Yes, left justify field.
				width = -width;
				}
			c = *fmt++;
			}
		else {
			while(isdigit(c)) {
				width = width * 10 + c - '0';
				c = *fmt++;
				}
			}

		// Get precision.
		precision = 0;
		if(c == '.') {
			c = *fmt++;
			if(c == '*') {
				if(arg1p != NULL)		// Error if format op (%).
					goto TooMany;
				if(fmtarg(tp,ArgInt1,&as) != Success)
					return rc.status;
				if((precision = tp->u.d_int) < 0)
					precision = 0;
				else
					flags |= FMTprec;
				c = *fmt++;
				}
			else if(isdigit(c)) {
				flags |= FMTprec;
				do {
					precision = precision * 10 + c - '0';
					} while(isdigit(c = *fmt++));
				}
			}

		// Get el flag.
		if(c == 'l') {
			flags |= FMTlong;
			c = *fmt++;
			}

		// Get spec.
		switch(c) {
		case 's':
			if(arg1p != NULL) {
				if(arg1p->d_type != dat_nil) {
					if(!strval(arg1p))	// Check arg type.
						return rc.status;
					if(++specCount > 1)	// Check spec count.
						goto TooMany;
					}
				tp = arg1p;
				}
			else if(fmtarg(tp,ArgNil1,&as) != Success)
				return rc.status;
			if(tp->d_type == dat_nil)
				dsetnull(tp);
			sLen = strlen(str = tp->d_str);		// Length of string.
			if(flags & FMTprec) {			// If there is a precision...
				if(precision < sLen)
					if((sLen = precision) < 0)
						sLen = 0;
				}
			break;
		case '%':
			wkbuf[0] = '%';
			goto SaveCh;
		case 'c':
			if(arg1p != NULL) {
				if(!intval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
					goto TooMany;
				tp = arg1p;
				}
			else if(fmtarg(tp,ArgInt1,&as) != Success)
				return rc.status;
			wkbuf[0] = tp->u.d_int;
SaveCh:
			str = wkbuf;
			sLen = 1;
			break;
		case 'd':
		case 'i':
			if(arg1p != NULL) {
				if(!intval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
					goto TooMany;
				tp = arg1p;
				}
			else if(fmtarg(tp,ArgInt1,&as) != Success)
				return rc.status;
			base = 10;
			ul = labs(tp->u.d_int);
			prefix = tp->u.d_int < 0 ? "-" : (flags & FMTplus) ? "+" : (flags & FMTspc) ? " " : "";
			goto ULFmt;
		case 'b':
			base = 2;
			goto GetUns;
		case 'o':
			base = 8;
			goto GetUns;
		case 'u':
			base = 10;
GetUns:
			if(arg1p != NULL) {
				if(!intval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
					goto TooMany;
				tp = arg1p;
				}
			else if(fmtarg(tp,ArgInt1,&as) != Success)
				return rc.status;
			ul = (ulong) tp->u.d_int;
			goto ULFmt;
		case 'X':
			flags |= FMTxuc;
		case 'x':
			if(arg1p != NULL) {
				if(!intval(arg1p))		// Check arg type.
					return rc.status;
				if(++specCount > 1)		// Check spec count.
TooMany:
					return rcset(Failure,RCNoFormat,text320);
						// "More than one spec in '%' format string"
				tp = arg1p;
				}
			else if(fmtarg(tp,ArgInt1,&as) != Success)
				return rc.status;
			base = 16;
			ul = (ulong) tp->u.d_int;
			if((flags & FMThash) && ul)
				prefix = (c == 'X') ? "0X" : "0x";

			// Fall through.
ULFmt:
			// Ignore '0' flag if precision specified.
			if((flags & (FMT0pad | FMTprec)) == (FMT0pad | FMTprec))
				flags &= ~FMT0pad;

			str = wkbuf + (FMTBufSz - 1);
			if(ul) {
				for(;;) {
					*str = (ul % base) + '0';
					if(*str > '9')
						*str += (flags & FMTxuc) ? 'A' - '0' - 10 : 'a' - '0' - 10;
					if((ul /= base) == 0)
						break;
					--str;
					}
				sLen = wkbuf + FMTBufSz - str;
				}
			else if((flags & FMTprec) && precision == 0)
				sLen = 0;
			else {
				*str = '0';
				sLen = 1;
				}

			if(sLen < precision) {
				if(precision > FMTBufSz)
					precision = FMTBufSz;
				do {
					*--str = '0';
					} while(++sLen < precision);
				}
			else if(sLen > 0 && c == 'o' && (flags & FMThash) && *str != '0') {
				*--str = '0';
				++sLen;
				}
			break;
		default:
			return rcset(Failure,0,text321,c == '\0' ? "" : vizc(c,VBaseDef));
				// "Unknown format spec '%%%s'"
		}

		// Concatenate the pieces, which are padding, prefix, more padding, the string, and trailing padding.
		prefLen = (prefix == NULL) ? 0 : strlen(prefix); // Length of prefix string.
		padding = width - (prefLen + sLen);		// # of chars to pad.

		// If 0 padding, store prefix (if any).
		if((flags & FMT0pad) && prefix != NULL) {
			if(dputs(prefix,&result) != 0)
				goto LibFail;
			prefix = NULL;
			}

		// If right-justified and chars to pad, store prefix string.
		if(padding > 0) {
			if(!(flags & FMTleft)) {
				short c1 = (flags & FMT0pad) ? '0' : ' ';
				while(--padding >= 0)
					if(dputc(c1,&result) != 0)
						goto LibFail;
				}
			}

		// Store prefix (if any).
		if(prefix != NULL && dputs(prefix,&result) != 0)
			goto LibFail;

		// Store (fixed-length) string.
		if(dputmem((void *) str,sLen,&result) != 0)
			goto LibFail;

 		// Store right padding.
		if(flags & FMTleft) {
			while(--padding >= 0)
				if(dputc(' ',&result) != 0)
					goto LibFail;
			}
		}

	// End of format string.  Check for errors.
	if(specCount == 0 && arg1p != NULL)
		return rcset(Failure,RCNoFormat,text281);
			// "Missing spec in '%' format string"
	if(as.ary != NULL && as.i < as.ary->a_used)
		return rcset(Failure,RCNoFormat,text204);
			// "Too many arguments for 'printf' or 'sprintf' function"
	if(dclose(&result,sf_string) != 0)
LibFail:
		(void) librcset(Failure);

	return rc.status;
	}

// Get a kill specified by n (of unlimited size) and save in rval.  May be a null string.  Return status.
static int getkill(Datum *rval,int n) {
	RingEntry *kp;

	// Which kill?
	if((kp = rget(&kring,n)) != NULL && datcpy(rval,&kp->re_data) != 0)
		(void) librcset(Failure);
	return rc.status;
	}

// Resolve a buffer name argument.  Return status.
Buffer *getbufarg(Datum *bname) {
	Buffer *buf;

	if((buf = bsrch(bname->d_str,NULL)) == NULL)
		(void) rcset(Failure,0,text118,bname->d_str);
			// "No such buffer '%s'"
	return buf;
	}

// Do mode? function.  Return status.
int modeQ(Datum *rval,int n,Datum **argv) {
	int status;
	ModeSpec *mspec;
	bool matchFound;
	Datum *datum,*arg;
	char *kwlist,*keyword;
	ArraySize elct;
	Datum **elp = NULL;
	Buffer *buf = NULL;
	bool result = false;
	static bool checkAll;
	static bool ignore;
	static Option options[] = {
		{"^All",NULL,0,.u.ptr = (void *) &checkAll},
		{"^Ignore",NULL,0,.u.ptr = (void *) &ignore},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		0,text451,false,options};
			// "function option"

	// Get buffer if specified.
	if(argv[0]->d_type != dat_nil && (buf = getbufarg(argv[0])) == NULL)
		return rc.status;

	// Get options and set flags.
	initBoolOpts(options);
	if(n != INT_MIN) {
		if(parseopts(&ohdr,NULL,argv[2],NULL) != Success)
			return rc.status;
		setBoolOpts(options);
		if(checkAll)
			result = true;
		}

	// Get keyword(s).
	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	while((status = nextarg(&arg,argv + 1,datum,&kwlist,&elp,&elct)) == Success) {
		keyword = arg->d_str;

		// Search mode table for a match.
		matchFound = false;
		if((mspec = mdsrch(keyword,NULL)) != NULL) {

			// Match found.  Mode enabled?
			matchFound = true;
			if(buf == NULL) {

				// Checking global modes.  Right type?
				if(!(mspec->ms_flags & MdGlobal))
					goto Wrong;
				if(mspec->ms_flags & MdEnabled)
					goto Good;
				goto Bad;
				}
			else {
				// Checking buffer modes.  Right type?
				if(mspec->ms_flags & MdGlobal)
Wrong:
					return rcset(Failure,0,text456,keyword);
						// "Wrong type of mode '%s'"
				if(bmsrch1(buf,mspec)) {
Good:
					// Mode is enabled.  Done (success) if "any" check.
					if(!checkAll)
						result = true;
					}

				// Buffer mode not enabled.  Failure if "all" check.
				else {
Bad:
					if(checkAll)
						result = false;
					}
				}
			}

		// Unknown keyword.  Error if "Ignore" not specified.
		else if(!ignore)
			return rcset(Failure,0,text66,keyword);
				// "Unknown or ambiguous mode '%s'"
		}

	// All arguments processed.  Return result.
	if(status == NotFound) {
		if(!matchFound)
			return rcset(Failure,0,text446,text389);
				// "Valid %s not specified","mode"
		dsetbool(result,rval);
		}
	return rc.status;
	}

// Check if a mode in given group is set and return its name if so; otherwise, nil.  Return status.
int groupMode(Datum *rval,int n,Datum **argv) {
	ModeSpec *mspec;
	ModeGrp *mgrp;
	Array *ary;
	Datum *el;
	ushort count;
	Buffer *buf = NULL;
	ModeSpec *result = NULL;
	bool ignore = false;
	bool bufMode = false;
	static Option options[] = {
		{"^Ignore",NULL,0,0},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		0,text451,false,options};
			// "function option"

	// Get buffer if specified.
	if(argv[0]->d_type != dat_nil) {
		if((buf = getbufarg(argv[0])) == NULL)
			return rc.status;
		bufMode = true;
		}

	// Get options and set flags.
	if(n != INT_MIN) {
		if(parseopts(&ohdr,NULL,argv[2],NULL) != Success)
			return rc.status;
		if(options[0].cflags & OptSelected)
			ignore = true;
		}

	// Get mode group.
	if(!mgsrch(argv[1]->d_str,NULL,&mgrp)) {
		if(ignore)
			goto SetNil;
		return rcset(Failure,0,text395,text390,argv[1]->d_str);
				// "No such %s '%s'","group"
		}

	// If group has members, continue...
	if(mgrp->mg_usect > 0) {

		// Check for type mismatch; that is, a global mode specified for a buffer group, or vice versa.
		ary = &mi.modetab;
		while((el = aeach(&ary)) != NULL) {
			mspec = msptr(el);
			if(mspec->ms_group == mgrp) {
				if(!(mspec->ms_flags & MdGlobal) != bufMode)
					return rcset(Failure,0,text404,mgrp->mg_name,bufMode ? text83 : text146);
						// "'%s' is not a %s group","buffer","global"
				break;
				}
			}

		// Have valid argument(s).  Check if any mode is enabled in group.
		ary = &mi.modetab;
		count = 0;
		while((el = aeach(&ary)) != NULL) {
			mspec = msptr(el);
			if(mspec->ms_group == mgrp) {
				if(buf == NULL) {
					if(mspec->ms_flags & MdEnabled)
						goto Enabled;
					}
				else if(bmsrch1(buf,mspec)) {
Enabled:
					result = mspec;
					break;
					}
				if(++count == mgrp->mg_usect)
					break;
				}
			}
		}

	// Return result.
	if(result != NULL) {
		if(dsetstr(result->ms_name,rval) != 0)
			(void) librcset(Failure);
		}
	else
SetNil:
		dsetnil(rval);
	return rc.status;
	}

// Clone an array.  Return status.
int aryclone(Datum *dest,Datum *src,int depth) {
	Array *ary;

	if(maxarydepth > 0 && depth > maxarydepth)
		return rcset(Failure,0,text319,Literal23,maxarydepth);
			// "Maximum %s recursion depth (%d) exceeded","array"
	if((ary = aclone(awptr(src)->aw_ary)) == NULL)
		return librcset(Failure);
	if(awrap(dest,ary) == Success) {
		ArraySize n = ary->a_used;
		Datum **elp = ary->a_elp;

		// Check for nested arrays.
		while(n-- > 0) {
			if((*elp)->d_type == dat_blobRef && aryclone(*elp,*elp,depth + 1) != Success)
				return rc.status;
			++elp;
			}
		}
	return rc.status;
	}

// Convert any value into a string form which will resolve to the original value if subsequently evaluated as an expression
// (unless "force" is set (n > 0) and value is an array that includes itself).  Return status.
#if !(MMDebug & Debug_MacArg)
static
#endif
int dquote(Datum *rval,Datum *datum,uint flags) {
	DStrFab sfab;

	if(dopenwith(&sfab,rval,SFClear) != 0)
		goto LibFail;
	if(dtosfchk(&sfab,datum,NULL,flags) == Success && dclose(&sfab,sf_string) != 0)
LibFail:
		(void) librcset(Failure);

	return rc.status;
	}

// Set wrap column to n.  Return status.
int setwrap(int n,bool msg) {

	if(n < 0)
		(void) rcset(Failure,0,text39,text59,n,0);
			// "%s (%d) must be %d or greater","Wrap column"
	else {
		si.pwrapcol = si.wrapcol;
		si.wrapcol = n;
		if(msg)
			(void) rcset(Success,0,"%s%s%d",text59,text278,n);
					// "Wrap column"," set to "
		}
	return rc.status;
	}

// Convert a string to title case.  Return status.
static int tcstr(Datum *dest,Datum *src) {
	char *src1,*dest1;
	short c;
	bool inword = false;

	if(dsalloc(dest,strlen(src1 = src->d_str) + 1) != 0)
		return librcset(Failure);
	dest1 = dest->d_str;
	while((c = *src1++) != '\0') {
		if(wordlist[c]) {
			*dest1++ = inword ? lowcase[c] : upcase[c];
			inword = true;
			}
		else {
			*dest1++ = c;
			inword = false;
			}
		}
	*dest1 = '\0';
	return rc.status;
	}

// Set pattern at top of search or replace ring.  Make a copy so that a search pattern on the ring is not modified by
// chkopts() if newspat() is called.
static int setTopPat(Ring *ring) {
	char *pat = (ring->r_size == 0) ? "" : ring->r_entry->re_data.d_str;
	if(ring == &sring) {
		char wkbuf[strlen(pat) + 1];

		return newspat(strcpy(wkbuf,pat),&srch.m,NULL);
		}
	return newrpat(pat,&srch.m);
	}

// Delete entri(es) from search or replace ring and update or delete current pattern in Match record, if applicable.
static int delpat(Ring *ring,int n) {

	if(rdelete(ring,n) == Success)
		(void) setTopPat(ring);

	return rc.status;
	}

// Execute a system command or function, given result pointer, n argument, pointer into the command-function table (cftab), and
// minimum and maximum number of arguments.  Return status.  This is the execution routine for all commands and functions.  When
// script mode is active, arguments (if any) are preloaded and validated per descriptors in the cftab table, then made available
// to the code for the specific command or function (which is here or in a separate routine).  If not evaluating (OpEval not
// set), arguments are "consumed" only and the execution code is bypassed if the maximum number of arguments for the command or
// function has been loaded.  Note that (1), if not evaluating, this routine will only be called if the CFSpecArgs flag is set;
// and (2), any command or function with CFNCount flag set must handle a zero n so that execCF() calls from routines other than
// fcall() will always work (for any value of n).
int execCF(Datum *rval,int n,CmdFunc *cfp,int minArgs,int maxArgs) {
	Datum *args[CFMaxArgs + 1];	// Argument values plus NULL terminator.
	int argct = 0;			// Number of arguments loaded into args array.

	// If script mode and CFNoLoad is not set, retrieve the minimum number of arguments specified in cftab, up to the
	// maximum, and save in args.  If CFSpecArgs flag is set or the maximum is negative, use the minimum for the maximum;
	// otherwise, if CFShrtLoad flag is set, decrease the maximum by one.
	args[0] = NULL;
	if((si.opflags & (OpScript | OpNoLoad)) == OpScript && !(cfp->cf_aflags & CFNoLoad) &&
	 (!(si.opflags & OpParens) || !havesym(s_rparen,false))) {
		if(cfp->cf_aflags & CFShrtLoad)
			--minArgs;
		if((cfp->cf_aflags & CFSpecArgs) || cfp->cf_maxArgs < 0)
			maxArgs = minArgs;
		else if(cfp->cf_aflags & CFShrtLoad)
			--maxArgs;
		if(maxArgs > 0) {
			Datum **argv = args;
			uint aflags = ArgFirst | (cfp->cf_vflags & ArgPath);
			do {
				if(dnewtrk(argv) != 0)
					goto LibFail;
				if(funcarg(*argv++,aflags | (((cfp->cf_vflags & (ArgNotNull1 << argct)) |
				 (cfp->cf_vflags & (ArgNil1 << argct)) | (cfp->cf_vflags & (ArgBool1 << argct)) |
				 (cfp->cf_vflags & (ArgInt1 << argct)) | (cfp->cf_vflags & (ArgArray1 << argct)) |
				 (cfp->cf_vflags & (ArgNIS1 << argct))) >> argct) | (cfp->cf_vflags & ArgMay)) != Success)
					return rc.status;
				aflags = 0;
				} while(++argct < minArgs || (argct < maxArgs && havesym(s_comma,false)));
			*argv = NULL;
			}
		}

	// Evaluate the command or function.
	if(cfp->cf_func != NULL)
		(void) cfp->cf_func(rval,n,args);
	else {
		int i;
		char *str;
		long lval;
		Buffer *buf;
		Ring *ring;
		int fnum = cfp - cftab;

		switch(fnum) {
			case cf_abs:
				dsetint(labs(args[0]->u.d_int),rval);
				break;
			case cf_appendFile:
				i = 'a';
				goto AWFile;
			case cf_backPageNext:
				// Scroll the next window up (backward) a page.
				(void) wscroll(rval,n,nextWind,backPage);
				break;
			case cf_backPagePrev:
				// Scroll the previous window up (backward) a page.
				(void) wscroll(rval,n,prevWind,backPage);
				break;
			case cf_backTab:
				// Move the point backward "n" tab stops.
				(void) bftab(n == INT_MIN ? -1 : -n);
				break;
			case cf_backspace:;
				// Delete char or soft tab backward.  Return status.
				Point *point = &si.curwin->w_face.wf_point;
				(void) (si.stabsize > 0 && point->off > 0 ? deltab(n == INT_MIN ? -1 : -n,true) :
				 ldelete(n == INT_MIN ? -1L : (long) -n,0));
				break;
			case cf_basename:
				if(dsetstr(fbasename(args[0]->d_str,n == INT_MIN || n > 0),rval) != 0)
					goto LibFail;
				break;
			case cf_beep:
				// Beep the beeper n times.
				if(n == INT_MIN)
					n = 1;
				else if(n < 0 || n > 10)
					return rcset(Failure,0,text12,text137,n,0,10);
						// "%s (%d) must be between %d and %d","Repeat count"
				while(n-- > 0)
					ttbeep();
				break;
			case cf_beginBuf:
				// Go to the beginning of the buffer.
				str = text326;
					// "Begin"
				i = false;
				goto BEBuf;
			case cf_beginLine:
				// Move the point to the beginning of the [-]nth line.
				(void) beline(rval,n,false);
				break;
			case cf_beginWhite:
				// Move the point to the beginning of white space surrounding point.
				(void) spanwhite(false);
				break;
			case cf_bemptyQ:

				// Get buffer pointer.
				if(n == INT_MIN)
					buf = si.curbuf;
				else if((buf = bsrch(args[0]->d_str,NULL)) == NULL)
					return rcset(Failure,0,text118,args[0]->d_str);
						// "No such buffer '%s'"

				// Return Boolean result.
				dsetbool(bempty(buf),rval);
				break;
			case cf_bprintf:

				// Get buffer pointer.
				if((buf = bsrch(args[0]->d_str,NULL)) == NULL)
					return rcset(Failure,0,text118,args[0]->d_str);
						// "No such buffer '%s'"

				// Create formatted string in rval and insert into buffer.
				if(strfmt(rval,args[1],NULL) == Success)
					(void) iortext(rval->d_str,n == INT_MIN ? 1 : n,Txt_Insert,buf);
				break;
			case cf_bufBoundQ:
				if(n != INT_MIN)
					n = n > 0 ? 1 : n < 0 ? -1 : 0;
				i = bufend(NULL) ? 1 : bufbegin(NULL) ? -1 : 0;
				dsetbool((n == INT_MIN && i != 0) || i == n,rval);
				break;
			case cf_bufWind:
				{EWindow *win;

				if((buf = bsrch(args[0]->d_str,NULL)) != NULL && (win = whasbuf(buf,n != INT_MIN)) != NULL)
					dsetint((long) getwnum(win),rval);
				else
					dsetnil(rval);
				}
				break;
			case cf_chr:
				if(charval(args[0]))
					dsetchr(args[0]->u.d_int,rval);
				break;
			case cf_clearMsgLine:
				(void) mlerase();
				break;
			case cf_clone:
				(void) aryclone(rval,args[0],0);
				break;
			case cf_copyFencedRegion:
				// Copy text to kill ring.
				i = 1;
				goto KDCFenced;
			case cf_copyLine:
				// Copy line(s) to kill ring.
				i = 1;
				goto KDCLine;
			case cf_copyRegion:;
				// Copy all of the characters in the region to the kill ring without moving point.
				Region region;

				if(getregion(&region,0) != Success || copyreg(&region) != Success)
					break;
				(void) rcset(Success,RCNoFormat,text70);
					// "Region copied"
				break;
			case cf_copyToBreak:
				// Copy text to kill ring.
				i = 1;
				goto KDCText;
			case cf_copyWord:
				// Copy word(s) to kill ring without moving point.
				(void) (n == INT_MIN ? kdcfword(1,1) : n < 0 ? kdcbword(-n,1) : kdcfword(n,1));
				break;
			case cf_metaPrefix:
			case cf_negativeArg:
			case cf_prefix1:
			case cf_prefix2:
			case cf_prefix3:
			case cf_universalArg:
				break;
			case cf_cycleDelRing:
				ring = &dring;
				goto CycleRing;
			case cf_cycleKillRing:
				ring = &kring;
				goto CycleRing;
			case cf_cycleReplaceRing:
				ring = &rring;
				goto CycleRing;
			case cf_cycleSearchRing:
				ring = &sring;
CycleRing:
				// Cycle a ring forward or backward.  If search or replace ring, also set pattern at top
				// when done.
				if(n != 0 && ring->r_size > 1 && rcycle(ring,n,true) == Success)
					if(fnum != cf_cycleDelRing && fnum != cf_cycleKillRing)
						(void) setTopPat(ring);
				break;
			case cf_delBackChar:
				// Delete char backward.  Return status.
				(void) ldelete(n == INT_MIN ? -1L : (long) -n,0);
				break;
			case cf_delBackTab:
				// Delete tab backward.  Return status.
				(void) deltab(n == INT_MIN ? -1 : -n,false);
				break;
			case cf_delDelEntry:
				ring = &dring;
				goto DelRingEntry;
			case cf_delFencedRegion:
				// Delete text without saving in kill ring.
				i = 0;
				goto KDCFenced;
			case cf_delForwChar:
				// Delete char forward.  Return status.
				(void) ldelete(n == INT_MIN ? 1L : (long) n,0);
				break;
			case cf_delForwTab:
				// Delete tab forward.  Return status.
				(void) deltab(n,false);
				break;
			case cf_delKillEntry:
				ring = &kring;
DelRingEntry:
				(void) rdelete(ring,n);
				break;
			case cf_delLine:
				// Delete line(s) without saving text in kill ring.
				i = 0;
				goto KDCLine;
			case cf_delRegion:
				// Delete region without saving text in kill ring.
				i = false;
				goto DKReg;
			case cf_delReplacePat:
				ring = &rring;
				goto DelPat;
			case cf_delSearchPat:
				ring = &sring;
DelPat:
				(void) delpat(ring,n);
				break;
			case cf_delToBreak:
				// Delete text without saving in kill ring.
				i = 0;
				goto KDCText;
			case cf_delWhite:
				// Delete white space surrounding or immediately before point on current line.
				(void) delwhite(n,true);
				break;
			case cf_delWord:
				// Delete word(s) without saving text in kill ring.
				(void) (n == INT_MIN ? kdcfword(1,0) : n < 0 ? kdcbword(-n,0) : kdcfword(n,0));
				break;
			case cf_dirname:
				if(dsetstr(fdirname(args[0]->d_str,n),rval) != 0)
					goto LibFail;
				break;
			case cf_emptyQ:
				if(args[0]->d_type != dat_int || charval(args[0]))
					dsetbool(args[0]->d_type == dat_nil ? true :
					 args[0]->d_type == dat_int ? args[0]->u.d_int == 0 :
					 args[0]->d_type & DStrMask ? *args[0]->d_str == '\0' :
					 awptr(args[0])->aw_ary->a_used == 0,rval);
				break;
			case cf_endBuf:
				// Move to the end of the buffer.
				str = text188;
					// "End"
				i = true;
BEBuf:
				(void) bufop(rval,n,str,BOpBeginEnd,i);
				break;
			case cf_endLine:
				// Move the point to the end of the [-]nth line.
				(void) beline(rval,n,true);
				break;
			case cf_endWhite:
				// Move the point to the end of white space surrounding point.
				(void) spanwhite(true);
				break;
			case cf_env:
				if(dsetstr(fixnull(getenv(args[0]->d_str)),rval) != 0)
					goto LibFail;
				break;
			case cf_expandPath:
				(void) expath(rval,args[0]);
				break;
			case cf_findFile:
				i = false;
				goto FVFile;
			case cf_forwPageNext:
				// Scroll the next window down (forward) a page.
				(void) wscroll(rval,n,nextWind,forwPage);
				break;
			case cf_forwPagePrev:
				// Scroll the previous window down (forward) a page.
				(void) wscroll(rval,n,prevWind,forwPage);
				break;
			case cf_forwTab:
				// Move the point forward "n" tab stops.
				(void) bftab(n == INT_MIN ? 1 : n);
				break;
			case cf_growWind:
				// Grow (enlarge) the current window.  If n is negative, take lines from the upper window;
				// otherwise, lower.
				i = 1;
				goto GSWind;
			case cf_insert:
				(void) chgtext(rval,n,Txt_Insert,NULL);
				break;
			case cf_insertPipe:
				str = text249;
					// "Insert pipe"
				i = PipeInsert;
				goto PipeCmd;
			case cf_insertSpace:
				// Insert space(s) forward into text without moving point.
				(void) insnlspace(n,EditSpace | EditHoldPt);
				break;
			case cf_interactiveQ:
				dsetbool(!(si.opflags & OpStartup) && !(last->p_flags & OpScript),rval);
				break;
			case cf_join:
				if(needsym(s_comma,true))
					(void) catargs(rval,1,args[0],n == INT_MIN || n > 0 ? CvtKeepAll :
					 n == 0 ? CvtKeepNull : 0);
				break;
			case cf_keyPendingQ:
				if(typahead(&i) == Success)
					dsetbool(i > 0,rval);
				break;
			case cf_kill:
				(void) getkill(rval,(int) args[0]->u.d_int);
				break;
			case cf_killFencedRegion:
				// Delete text and save in kill ring.
				i = -1;
KDCFenced:
				(void) kdcFencedRegion(i);
				break;
			case cf_killLine:
				// Delete line(s) and save text in kill ring.
				i = -1;
KDCLine:
				(void) kdcline(n,i);
				break;
			case cf_killRegion:
				// Delete region and save text in kill ring.
				i = true;
DKReg:
				(void) dkregion(n,i);
				break;
			case cf_killToBreak:
				// Delete text and save in kill ring.
				i = -1;
KDCText:
				(void) kdctext(n,i,NULL);
				break;
			case cf_killWord:
				// Delete word(s) and save text in kill ring.
				(void) (n == INT_MIN ? kdcfword(1,-1) : n < 0 ? kdcbword(-n,-1) : kdcfword(n,-1));
				break;
			case cf_lastBuf:
				if(si.curscr->s_lastbuf != NULL) {
					Buffer *oldbuf = si.curbuf;
					if(render(rval,1,si.curscr->s_lastbuf,0) == Success && n != INT_MIN && n < 0) {
						char bname[MaxBufName + 1];

						strcpy(bname,oldbuf->b_bname);
					 	if(bdelete(oldbuf,0) == Success)
							(void) rcset(Success,0,text372,bname);
									// "Buffer '%s' deleted"
						}
					}
				break;
			case cf_length:
				dsetint(args[0]->d_type == dat_blobRef ? (long) awptr(args[0])->aw_ary->a_used :
				 (long) strlen(args[0]->d_str),rval);
				break;
			case cf_lowerCaseLine:
				i = CaseLine | CaseLower;
				goto CvtCase;
			case cf_lowerCaseRegion:
				i = CaseRegion | CaseLower;
				goto CvtCase;
			case cf_lowerCaseStr:
				i = -1;
				goto CvtStr;
			case cf_lowerCaseWord:
				i = CaseWord | CaseLower;
				goto CvtCase;
			case cf_match:
				if(args[0]->u.d_int < 0 || args[0]->u.d_int >= MaxGroups)
					return rcset(Failure,0,text5,args[0]->u.d_int,MaxGroups - 1);
						// "Group number %ld must be between 0 and %d"
				if(dsetstr(fixnull((n == INT_MIN ? &rematch :
				 &srch.m)->groups[(int) args[0]->u.d_int].match->d_str),rval) != 0)
					goto LibFail;
				break;
			case cf_moveWindDown:
				// Move the current window down by "n" lines and compute the new top line in the window.
				(void) moveWindUp(rval,n == INT_MIN ? -1 : -n,args);
				break;
			case cf_newline:
				i = EditWrap;
				goto InsNLSPC;
			case cf_nextBuf:
				// Switch to the next buffer in the buffer list.
				i = false;
				goto PNBuf;
			case cf_nextScreen:
				i = SWB_Repeat | SWB_Forw;
				goto PNScreen;
			case cf_nilQ:
				dsetbool(args[0]->d_type == dat_nil,rval);
				break;
			case cf_nullQ:
				dsetbool(disnull(args[0]),rval);
				break;
			case cf_numericQ:
				dsetbool(asc_long(args[0]->d_str,NULL,true),rval);
				break;
			case cf_openLine:
				if(insnlspace(n,EditHoldPt) == Success && n < 0 && n != INT_MIN) {

					// Move point to first empty line if possible.
					Point *point = &si.curwin->w_face.wf_point;
					if(point->lnp->l_used > 0 && point->lnp->l_next->l_used == 0)
						(void) movech(1);
					}
				break;
			case cf_ord:
				dsetint((long) args[0]->d_str[0],rval);
				break;
			case cf_overwrite:
				(void) chgtext(rval,n,Txt_Overwrite,NULL);
				break;
			case cf_pause:
				if((i = args[0]->u.d_int) < 0)
					return rcset(Failure,0,text39,text119,(int) i,0);
						// "%s (%d) must be %d or greater","Pause duration"
				cpause(n == INT_MIN ? i * 100 : i);
				break;
			case cf_pipeBuf:
				str = text306;
					// "Pipe command"
				i = PipeWrite;
				goto PipeCmd;
			case cf_popBuf:
				i = true;
				goto PopIt;
			case cf_popFile:
				i = false;
PopIt:
				(void) dopop(rval,n,i);
				break;
			case cf_pop:
			case cf_shift:
				{Array *ary = awptr(args[0])->aw_ary;
				if((args[1] = (fnum == cf_pop) ? apop(ary) : ashift(ary)) == NULL)
					dsetnil(rval);
				else {
					datxfer(rval,args[1]);
					if(rval->d_type == dat_blobRef)
						agarbpush(rval);
					}
				}
				break;
			case cf_push:
			case cf_unshift:
				{Array *ary = awptr(args[0])->aw_ary;
				if(ainsert(ary,fnum == cf_push ? ary->a_used : 0,args[1],false) != 0)
					goto LibFail;
				else {
					duntrk(args[1]);
					datxfer(rval,args[0]);
					}
				}
				break;
			case cf_prevBuf:
				// Switch to the previous buffer in the buffer list.
				i = true;
PNBuf:
				(void) pnbuffer(rval,n,i);
				break;
			case cf_prevScreen:
				i = SWB_Repeat;
PNScreen:
				(void) gotoScreen(n,i);
				break;
			case cf_print:
				// Concatenate all arguments into rval and write result to message line.
				if(catargs(rval,1,NULL,0) == Success)
					goto PrintMsg;
				break;
			case cf_printf:
				// Build message in rval and write result to message line.
				if(strfmt(rval,args[0],NULL) == Success) {
PrintMsg:
					(void) mlputs(n == INT_MIN ? MLHome | MLFlush : MLHome | MLFlush | MLTermAttr,
					 rval->d_str);
					dsetnil(rval);
					}
				break;
			case cf_queryReplace:
				// Search and replace with query.
				i = true;
				goto Repl;
			case cf_quickExit:
				// Quick exit from MightEMacs.  If any buffer has changed, do a save on that buffer first.
				if(savebufs(1,BS_QExit) == Success)
					(void) rcset(UserExit,RCForce,NULL);
				break;
			case cf_quote:
				// Convert any value into a string form which will resolve to the original value.
				(void) dquote(rval,args[0],n > 0 ? CvtExpr | CvtForceArray : CvtExpr);
				break;
			case cf_rand:
				dsetint(xorshift64star(args[0]->u.d_int),rval);
				break;
			case cf_readFile:
				// Read a file into a buffer.

				// Get the filename...
				if(gtfilename(rval,n == -1 ? text299 : text131,si.curbuf->b_fname,0) != Success ||
							// "Pop file","Read file"
				 rval->d_type == dat_nil)
					break;

				// and return results.
				(void) readFP(rval,n,rval->d_str,RWExist);
				break;
			case cf_readPipe:
				str = text170;
					// "Read pipe"
				i = 0;
				goto PipeCmd;
			case cf_reframeWind:
				// Reposition point in current window per the standard redisplay subsystem.  Row defaults to
				// zero to center current line in window.  If in script mode, also do partial screen update so
				// that the window is moved to new point location.
				si.curwin->w_rfrow = (n == INT_MIN) ? 0 : n;
				si.curwin->w_flags |= WFReframe;
				if(si.opflags & OpScript)
					(void) wupd_reframe(si.curwin);
				break;
			case cf_replace:
				// Search and replace.
				i = false;
Repl:
				(void) replstr(rval,n,i,args);
				break;
			case cf_replaceText:
				(void) chgtext(rval,n,Txt_Replace,NULL);
				break;
			case cf_restoreBuf:
				// Restore the saved buffer.
				if(si.savbuf == NULL)
					return rcset(Failure,0,text208,text83);
							// "Saved %s not found","buffer"
				if(bswitch(si.savbuf,0) == Success && dsetstr(si.curbuf->b_bname,rval) != 0)
					goto LibFail;
				break;
			case cf_restoreScreen:;
				// Restore the saved screen.
				EScreen *scr;

				// Find the screen.
				scr = si.shead;
				do {
					if(scr == si.savscr) {
						dsetint((long) scr->s_num,rval);
						return sswitch(scr,0);
						}
					} while((scr = scr->s_next) != NULL);
				(void) rcset(Failure,0,text208,text380);
					// "Saved %s not found","screen"
				si.savscr = NULL;
				break;
			case cf_restoreWind:
				// Restore the saved window.
				{EWindow *win;

				// Find the window.
				win = si.whead;
				do {
					if(win == si.savwin) {
						si.curwin->w_flags |= WFMode;
						(void) wswitch(win,0);
						si.curwin->w_flags |= WFMode;
						dsetint((long) getwnum(si.curwin),rval);
						return rc.status;
						}
					} while((win = win->w_next) != NULL);
				(void) rcset(Failure,0,text208,text331);
					// "Saved %s not found","window"
				}
				break;
			case cf_revertYank:
				(void) yrevert(CFYank | CFUndel);
				break;
			case cf_saveBuf:
				// Save pointer to current buffer.
				si.savbuf = si.curbuf;
				if(dsetstr(si.curbuf->b_bname,rval) != 0)
					goto LibFail;
				break;
			case cf_saveFile:
				// Save the contents of the current buffer (or all buffers if n argument) to their associated
				// files.
				(void) savebufs(n,0);
				break;
			case cf_saveScreen:
				// Save pointer to current screen.
				si.savscr = si.curscr;
				break;
			case cf_saveWind:
				// Save pointer to current window.
				si.savwin = si.curwin;
				break;
			case cf_setWrapCol:
				// Set wrap column to N, or previous value if n argument.
				if(n != INT_MIN) {
					if(si.pwrapcol < 0)
						return rcset(Failure,RCNoFormat,text298);
							// "No previous wrap column set"
					n = si.pwrapcol;
					}
				else {
					if(getnarg(rval,text59) != Success || rval->d_type == dat_nil)
							// "Wrap column"
						return rc.status;
					n = rval->u.d_int;
					}
				(void) setwrap(n,true);
				break;
			case cf_shellCmd:
				str = "> ";
				i = PipePopOnly;
PipeCmd:
				(void) pipecmd(rval,n,str,i);
				break;
			case cf_showDelRing:
				ring = &dring;
				goto ShowRing;
			case cf_showDir:
				if(dsetstr(si.curscr->s_wkdir,rval) != 0)
					goto LibFail;
				(void) rcset(Success,RCNoFormat | RCNoWrap,si.curscr->s_wkdir);
				break;
			case cf_showKillRing:
				ring = &kring;
				goto ShowRing;
			case cf_showSearchRing:
				ring = &sring;
				goto ShowRing;
			case cf_showReplaceRing:
				ring = &rring;
ShowRing:
				(void) showRing(rval,n,ring);
				break;
			case cf_shQuote:
				if(tostr(args[0]) == Success && dshquote(args[0]->d_str,rval) != 0)
					goto LibFail;
				break;
			case cf_shrinkWind:
				// Shrink the current window.  If n is negative, give lines to the upper window; otherwise,
				// lower.
				i = -1;
GSWind:
				(void) gswind(rval,n,i);
				break;
			case cf_space:
				i = EditSpace | EditWrap;
InsNLSPC:
				(void) insnlspace(n,i);
				break;
			case cf_splitWind:
				{EWindow *win;
				(void) wsplit(n,&win);
				dsetint((long) getwnum(win),rval);
				}
				break;
			case cf_sprintf:
				(void) strfmt(rval,args[0],NULL);
				break;
			case cf_statQ:
				(void) ftest(rval,n,args[0],args[1]);
				break;
			case cf_strFit:
				if(args[1]->u.d_int < 0)
					(void) rcset(Failure,0,text39,text290,(int) args[1]->u.d_int,0);
							// "%s (%d) must be %d or greater","Length argument"
				else {
					if(dsalloc(rval,args[1]->u.d_int + 1) != 0)
						goto LibFail;
					strfit(rval->d_str,args[1]->u.d_int,args[0]->d_str,0);
					}
				break;
			case cf_strPop:
			case cf_strPush:
			case cf_strShift:
			case cf_strUnshift:
				(void) strfunc(rval,fnum,cfp->cf_name);
				break;
			case cf_strip:
				if(dsetstr(stripstr(args[0]->d_str,n == INT_MIN ? 0 : n),rval) != 0)
					goto LibFail;
				break;
			case cf_sub:
				{ushort flags = 0;
				int (*sub)(Datum *rval,int n,Datum *sp,char *spat,char *rpat,ushort flags);
				if(args[1]->d_type == dat_nil)
					dsetnull(args[1]);
				else
					(void) chkopts(args[1]->d_str,&flags);
				sub = (flags & SOpt_Regexp) ? resub : strsub;
				(void) sub(rval,n <= 1 ? 1 : n,args[0],args[1]->d_str,args[2]->d_type == dat_nil ? "" :
				 args[2]->d_str,flags);
				}
				break;
			case cf_subline:
				{long lval2 = argct < 2 ? LONG_MAX : args[1]->u.d_int;
				lval = args[0]->u.d_int;
				if(lval2 != 0 && (i = si.curwin->w_face.wf_point.lnp->l_used) > 0) {

					// Determine line offset and length and validate them.  Offset is position relative to
					// point.
					lval += si.curwin->w_face.wf_point.off;
					if(lval >= 0 && lval < i && (lval2 >= 0 || (lval2 = i - lval + lval2) > 0)) {
						if(lval2 > i - lval)
							lval2 = i - lval;
						if(dsetsubstr(si.curwin->w_face.wf_point.lnp->l_text + (int) lval,
						 (size_t) lval2,rval) != 0)
							goto LibFail;
						}
					else
						dsetnull(rval);
					}
				else
					dsetnull(rval);
				}
				break;
			case cf_substr:
				{long lval2 = args[1]->u.d_int;
				long lval3 = argct < 3 ? LONG_MAX : args[2]->u.d_int;
				if(lval3 != 0 && (lval = strlen(args[0]->d_str)) > 0 && (lval2 < 0 ? -lval2 - 1 :
				 lval2) < lval) {
					if(lval2 < 0)					// Negative offset.
						lval2 += lval;
					lval -= lval2;					// Maximum bytes can copy.
					if(lval3 > 0 || (lval3 += lval) > 0) {
						if(dsetsubstr(args[0]->d_str + lval2,(size_t) (lval3 <= lval ? lval3 : lval),
						 rval) != 0)
							goto LibFail;
						}
					else
						dsetnull(rval);
					}
				else
					dsetnull(rval);
				}
				break;
			case cf_tab:
				// Process a tab.  Normally bound to ^I.
				(void) instab(n == INT_MIN ? 1 : n);
				break;
			case cf_titleCaseLine:
				i = CaseLine | CaseTitle;
				goto CvtCase;
			case cf_titleCaseRegion:
				i = CaseRegion | CaseTitle;
				goto CvtCase;
			case cf_titleCaseStr:
				(void) tcstr(rval,args[0]);
				break;
			case cf_titleCaseWord:
				i = CaseWord | CaseTitle;
				goto CvtCase;
			case cf_toInt:
				datxfer(rval,args[0]);
				(void) toint(rval);
				break;
			case cf_tr:
				(void) tr(rval,args[0],args[1],args[2]);
				break;
			case cf_truncBuf:;
				// Truncate buffer.  Delete all text from current buffer position to end (default or n > 0) or
				// beginning (n <= 0) and save in undelete buffer.

				// No-op if currently at buffer boundary.
				long bytes;
				bool yep;
				if(n == INT_MIN || n > 0) {
					if(bufend(NULL))
						break;
					bytes = LONG_MAX;
					yep = true;
					}
				else {
					if(bufbegin(NULL))
						break;
					bytes = LONG_MIN + 1;
					yep = false;
					}

				// Get confirmation if interactive and delete maximum possible, ignoring any end-of-buffer
				// error.
				if(!(si.opflags & OpScript)) {
					char wkbuf[NWork];

					sprintf(wkbuf,text463,yep ? text465 : text464);
						// "Truncate to %s of buffer","end","beginning"
					if(terminpYN(wkbuf,&yep) != Success || !yep)
						break;
					}
				if(kprep(false) == Success)
					(void) ldelete(bytes,EditDel);
				break;
			case cf_typeQ:
				(void) dsetstr(dtype(args[0],true),rval);
				break;
			case cf_undelete:
				// Yank text from the delete ring.
				if(n == INT_MIN)
					n = 1;
				(void) iorstr(NULL,n,Txt_Insert,&dring);
				break;
			case cf_undeleteCycle:
				(void) ycycle(rval,n,&dring,CFUndel,cf_undelete);
				break;
			case cf_upperCaseLine:
				i = CaseLine | CaseUpper;
				goto CvtCase;
			case cf_upperCaseRegion:
				i = CaseRegion | CaseUpper;
				goto CvtCase;
			case cf_upperCaseStr:
				i = 1;
CvtStr:;
				char *(*mk)(char *dest,char *src) = (i <= 0) ? mklower : mkupper;
				if(dsalloc(rval,strlen(args[0]->d_str) + 1) != 0)
					goto LibFail;
				mk(rval->d_str,args[0]->d_str);
				break;
			case cf_upperCaseWord:
				i = CaseWord | CaseUpper;
CvtCase:
				(void) cvtcase(n,i);
				break;
			case cf_viewFile:
				i = true;
FVFile:
				(void) fvfile(rval,n,i);
				break;
			case cf_wordCharQ:
				if(charval(args[0]))
					dsetbool(wordlist[args[0]->u.d_int],rval);
				break;
			case cf_writeFile:
				i = 'w';
AWFile:
				(void) awfile(rval,n,i == 'w' ? text144 : text218,i);
						// "Write file: ","Append file"
				break;
			case cf_yank:
				// Yank text from the kill ring.
				if(n == INT_MIN)
					n = 1;
				(void) iorstr(NULL,n,Txt_Insert,&kring);
				break;
			case cf_yankCycle:
				(void) ycycle(rval,n,&kring,CFYank,cf_yank);
			}
		}

	// Command or function call completed.  Extra arguments in script mode are checked in fcall() so no need to do it here.
	return rc.status == Success ? rcsave() : rc.status;
LibFail:
	return librcset(Failure);
	}

// Parse an escaped character sequence, given pointer to leading backslash (\) character.  A sequence includes numeric form \nn,
// where \0x... and \x... are hexadecimal; otherwise, octal.  Source pointer is updated after parsing is completed.  If
// allowNull is true, a null (zero) value is allowed; otherwise, an error is returned.  If cp is not NULL, result is returned in
// *cp.  Return status.
int evalclit(char **srcp,short *cp,bool allowNull) {
	short c,c1;
	int base = 8;
	int maxlen = 3;
	char *src0 = *srcp;
	char *src = src0 + 1;		// Past backslash.
	char *src1;

	// Parse \x and \0n... sequences.
	switch(*src++) {
		case 't':	// Tab
			c = 011; break;
		case 'r':	// CR
			c = 015; break;
		case 'n':	// NL
			c = 012; break;
		case 'e':	// Escape
			c = 033; break;
		case 's':	// Space
			c = 040; break;
		case 'f':	// Form feed
			c = 014; break;
		case 'x':
			goto IsNum;
		case '0':
			if(*src == 'x') {
				++src;
IsNum:
				base = 16;
				maxlen = 2;
				goto GetNum;
				}
			// Fall through.
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			--src;
GetNum:
			// \nn found.  src is at first digit (if any).  Decode it.
			c = 0;
			src1 = src;
			while((c1 = *src) != '\0' && maxlen > 0) {
				if(c1 >= '0' && (c1 <= '7' || (c1 <= '9' && base != 8)))
					c = c * base + (c1 - '0');
				else {
					c1 = lowcase[c1];
					if(base == 16 && (c1 >= 'a' && c1 <= 'f'))
						c = c * 16 + (c1 - ('a' - 10));
					else
						break;
					}
				++src;
				--maxlen;
				}

			// Any digits decoded?
			if(src == src1)
				goto LitChar;

			// Null character... return error if not allowed.
			if(c == 0 && !allowNull)
				return rcset(Failure,0,text337,(int)(src - src0),src0);
					// "Invalid escape sequence \"%.*s\""
			// Valid sequence.
			break;
		default:	// Literal character.
LitChar:
			c = src[-1];
		}

	// Return results.
	if(cp != NULL)
		*cp = c;
	*srcp = src;
	return rc.status;
	}

// Evaluate a string literal and return result.  src is assumed to begin and end with ' or ".  In single-quoted strings, escaped
// backslashes '\\' and apostrophes '\'' are recognized (only); in double-quoted strings, escaped backslashes "\\", double
// quotes "\"", special letters (like "\n" and "\t"), \nnn octal and hexadecimal sequences, and Ruby-style interpolated #{}
// expressions are recognized (and executed); e.g., "Values are #{sub "#{join ',',values,final}",',',delim} [#{ct}]".  Return
// status.
int evalslit(Datum *rval,char *src) {
	short termch,c;
	DStrFab result;
#if MMDebug & Debug_Token
	char *src0 = src;
#endif
	// Get ready.
	if(dopenwith(&result,rval,SFClear) != 0)
		goto LibFail;
	termch = *src++;

	// Loop until null or string terminator hit.
	while((c = *src) != termch) {
#if MMDebug & Debug_Token
		if(c == '\0')
			return rcset(Failure,0,"String terminator %c not found in %s",(int) termch,src0);
#endif
		// Process escaped characters.  Backslash cannot be followed by '\0'.
		if(c == '\\') {

			// Do single-quote processing.
			if(termch == '\'') {
				++src;

				// Check for escaped backslash or apostrophe only.
				if(*src == '\\' || *src == '\'')
					c = *src++;
				}

			// Do double-quote processing.
			else if(evalclit(&src,&c,false) != Success)
				return rc.status;
			}

		// Not a backslash.  Check for beginning of interpolation.
		else if(termch == '"' && c == TokC_Expr && src[1] == TokC_ExprBegin) {
			Datum *datum;

			// "#{" found.  Execute what follows to "}" as a command line.
			if(dnewtrk(&datum) != 0)
				goto LibFail;
			if(execestmt(datum,src + 2,TokC_ExprEnd,&src) != Success)
				return rc.status;

			// Append the result to dest.
			if(datum->d_type != dat_nil) {
				if(tostr(datum) != Success)
					return rc.status;
				if(dputd(datum,&result) != 0)
					goto LibFail;
				}

			// Success.
			++src;
			continue;
			}
		else
			// Not an interpolated expression.  Vanilla character.
			++src;

		// Save the character.
		if(dputc(c,&result) != 0)
			goto LibFail;
		}

	// Terminate the result and return.
	if(dclose(&result,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}
