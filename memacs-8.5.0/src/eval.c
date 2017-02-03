// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// eval.c	Expresion evaluation library routines for MightEMacs.

#include "os.h"
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "std.h"
#include "lang.h"
#include "bind.h"
#include "cmd.h"
#include "exec.h"
#include "file.h"
#include "main.h"
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
	Array *aryp;
	ArraySize i;
	} ArrayState;

// Forwards.
#if !(MMDebug & Debug_Array)
static
#endif
int dtosf(DStrFab *destp,Datum *srcp,char *dlm,uint flags);

// Return a datum object as a logical (Boolean) value.
bool tobool(Datum *datp) {

	// Check for numeric truth (!= 0).
	if(datp->d_type == dat_int)
		return datp->u.d_int != 0;

	// Check for logical false values (false and nil).  All other strings (including null strings) are true.
	return datp->d_type != dat_false && datp->d_type != dat_nil;
	}

// Check if given datum object is nil or null and return Boolean result.
bool disnn(Datum *datp) {

	return datp->d_type == dat_nil || disnull(datp);
	}

// Write an array to destp (an active string-fab object) via calls to dtosf().  If CvtExpr flag is set, write in "[...]" form so
// that result can be subsequently evaluated as an expression; otherwise, write as data.  In the latter case, elements are
// separated by "dlm" delimiters (if not NULL), or a comma if CvtVizStr or CvtVizStrQ flag is set.  A nil argument is included
// in result if CvtExpr or CvtKeepNil flag is set, and a null argument is included if CvtExpr or CvtKeepNull flag is set;
// otherwise, they are skipped.  In all cases, if the array includes itself, stop recursion and write "[...]" for array if
// CvtForceArray flag is set; otherwise, set an error.   Return status.
static int atosf(DStrFab *destp,Datum *srcp,char *dlm,uint flags) {
	ArrayWrapper *awp = awptr(srcp);

	// Array includes self?
	if(awp->aw_mark) {

		// Yes.  Store an ellipsis or set an error.
		if(!(flags & CvtForceArray))
			(void) rcset(Failure,0,text374);
				// "Endless recursion detected (array contains itself)"
		else if(dputs("[...]",destp) != 0)
			(void) drcset();
		}
	else {
		Datum *datp;
		Array *aryp = awp->aw_aryp;
		Datum **elpp = aryp->a_elpp;
		uint n = aryp->a_used;
		bool first = true;
		char *realdlm = (flags & (CvtExpr | CvtVizStr | CvtVizStrQ)) ? "," : dlm;
		awp->aw_mark = true;
		if(flags & CvtExpr) {
			flags |= CvtKeepAll;
			if(dputc('[',destp) != 0)
				return drcset();
			}

		while(n-- > 0) {
			datp = *elpp++;

			// Skip nil or null string if appropriate.
			if(datp->d_type == dat_nil) {
				if(!(flags & CvtKeepNil))
					continue;
				}
			else if(disnull(datp) && !(flags & CvtKeepNull))
				continue;

			// Write optional delimiter and encoded value.
			if(!first && realdlm != NULL && dputs(realdlm,destp) != 0)
				return drcset();
			if(dtosf(destp,datp,dlm,flags) != Success)
				return rc.status;
			first = false;
			}
		if((flags & CvtExpr) && dputc(']',destp) != 0)
			(void) drcset();
		}

	return rc.status;
	}

// Add an array to wrapper list, clear all "marked" flags, and call atosf().
int atosfc(DStrFab *destp,Datum *srcp,char *dlm,uint flags) {

	agarbpush(srcp);
	aclrmark();
	return atosf(destp,srcp,dlm,flags);
	}

// Write a datum object to destp (an active string-fab object) in string form.  If CvtExpr flag is set, quote strings and write
// nil values as keywords.  (Output is intended to be subsequently evaluated as an expression.)  If CvtExpr flag is not set,
// write strings in encoded (visible) form if CvtVizStr flag is set, and also enclosed in single (') quotes if CvtVizStrQ flag
// is set; otherwise, unmodified, and write nil values as a keyword if CvtShowNil, CvtVizStr, or CvtVizStrQ flag is set;
// otherwise, a null string.  In all cases, write Boolean values as keywords, and call atosf() with dlm and flag arguments to
// write arrays.  Return status.
#if !(MMDebug & Debug_Array)
static
#endif
int dtosf(DStrFab *destp,Datum *srcp,char *dlm,uint flags) {
	char *str;

	// Determine type of datum object.
	if(srcp->d_type & DStrMask) {
		if(flags & CvtExpr)
			(void) quote(destp,srcp->d_str,true);
		else if(flags & (CvtVizStr | CvtVizStrQ)) {
			if(((flags & CvtVizStrQ) && dputc('\'',destp) != 0) || dvizs(srcp->d_str,0,VBaseDef,destp) != 0 ||
			 ((flags & CvtVizStrQ) && dputc('\'',destp) != 0))
				(void) drcset();
			}
		else if(dputs(srcp->d_str,destp) != 0)
			(void) drcset();
		}
	else
		switch(srcp->d_type) {
			case dat_int:
				if(dputf(destp,"%ld",srcp->u.d_int) != 0)
					(void) drcset();
				break;
			case dat_blobRef:	// Array
				(void) atosf(destp,srcp,dlm,flags);
				break;
			case dat_nil:
				if(flags & (CvtExpr | CvtShowNil | CvtVizStr | CvtVizStrQ)) {
					str = viz_nil;
					goto Viz;
					}
				break;
			default:		// Boolean
				str = (srcp->d_type == dat_false) ? viz_false : viz_true;
Viz:
				if(dputs(str,destp) != 0)
					(void) drcset();
			}

	return rc.status;
	}

// Call atosfc() if array so that "marked" flags in wrapper list are cleared first; otherwise, call dtosf().
int dtosfc(DStrFab *destp,Datum *srcp,char *dlm,uint flags) {
	int (*func)(DStrFab *destp,Datum *srcp,char *dlm,uint flags) = (srcp->d_type == dat_blobRef) ? atosfc : dtosf;
	return func(destp,srcp,dlm,flags);
	}

// Create an array in rp, given optional size and initializer.  Return status.
int array(Datum *rp,int n,Datum **argpp) {
	Array *aryp;
	ArraySize len = 0;
	Datum *initp = NULL;

	// Get array size and initializer, if present.
	if(*argpp != NULL) {
		len = (*argpp++)->u.d_int;
		if(*argpp != NULL)
			initp = *argpp;
		}
	if((aryp = anew(len,initp)) == NULL)
		return drcset();
	if(awrap(rp,aryp) != Success)
		return rc.status;

	// Create unique arrays if initializer is an array.
	if(len > 0 && initp != NULL && initp->d_type == dat_blobRef) {
		Datum **elpp = aryp->a_elpp;
		do {
			if(aryclone(*elpp++,initp,0) != Success)
				return rc.status;
			} while(--len > 0);
		}
	
	return rc.status;
	}

// Get a single-character delimiter from a Datum object.  Return pointer to it if found; otherwise, set an error and return
// NULL.
static char *gtdelim(Datum *delimp) {
	char *str = delimp->d_str;
	if(strlen(str) != 1) {
		(void) rcset(Failure,0,text291,str);
			// "Delimiter '%s' must be a single character"
		return NULL;
		}
	return str;
	}

// Split a string into an array and save in rp, given delimiter and optional limit value.  Return status.
int ssplit(Datum *rp,int n,Datum **argpp) {
	Array *aryp;
	char *dlm,*str;
	int limit = 0;

	// Get delimiter, string, and optional limit.
	if((dlm = gtdelim(*argpp++)) == NULL)
		return rc.status;
	str = (*argpp++)->d_str;
	if(*argpp != NULL)
		limit = (*argpp)->u.d_int;

	if((aryp = asplit(*dlm,str,limit)) == NULL)
		return drcset();
	return awrap(rp,aryp);
	}

// Copy string from src to destp (an active string-fab object), adding a double quote (") at beginning and end (if full is true)
// and escaping all control characters, backslashes, and characters that are escaped by parsesym().  Return status.
int quote(DStrFab *destp,char *src,bool full) {
	int c;
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
				if(c < ' ' || c >= 0x7f)	// Non-printable character.
					sprintf(str = wkbuf,"\\%.3o",c);
				else				// Literal character.
LitChar:
					ischar = true;
			}

		if((ischar ? dputc(c,destp) : dputs(str,destp)) != 0)
			return drcset();
		}
	if(full && dputc('"',destp) != 0)
		(void) drcset();

	return rc.status;
	}

// Force NULL pointer to null string.
char *fixnull(char *s) {

	return (s == NULL) ? "" : s;
	}

// Set hard or soft tab size and do range check.
int settab(int size,bool hard) {

	// Check if new tab size is valid.
	if((size != 0 || hard) && (size < 2 || size > MaxTab))
		return rcset(Failure,0,text256,hard ? text49 : text50,size,MaxTab);
			// "%s tab size %ld must be between 2 and %d","Hard","Soft"

	// Set new size.
	if(hard)
		htabsize = size;
	else {
		stabsize = size;
		(void) rcset(Success,0,text332,size);
				// "Soft tab size set to %d"
		}

	return rc.status;
	}

// Initialize match object.
static void minit(Match *mtp) {
	GrpInfo *gip,*gipz;

	mtp->flags = 0;
	mtp->ssize = mtp->rsize = 0;
	gipz = (gip = mtp->groups) + MaxGroups;
	do {
		gip->matchp = NULL;
		} while(++gip < gipz);
	mtp->matchp = NULL;
	}

// Find pattern within source.  Find rightmost match if "rightmost" is true.  Set rp to 0-origin match position or nil if no
// match.  Return status.
static int sindex(Datum *rp,Datum *srcp,Datum *patp,bool rightmost) {

	// No match if source or pattern is null.
	if(!disnull(srcp) && !disnull(patp)) {
		ushort flags = 0;

		// Examine pattern and save in global "rematch" record.
		(void) chkopts(patp->d_str,&flags);
		if(newspat(patp->d_str,&rematch,&flags) != Success)
			return rc.status;
		grpclear(&rematch);

		// Check pattern type.
		if(flags & SOpt_Regexp) {
			int offset;

			// Have regular expression.  Compile it...
			if(mccompile(&rematch) != Success)
				return rc.status;

			// perform operation...
			if(recmp(srcp,rightmost ? -1 : 0,&rematch,&offset) != Success)
				return rc.status;

			// and return index if a match was found.
			if(offset >= 0) {
				dsetint((long) offset,rp);
				return rc.status;
				}
			}
		else {
			char *src1,*srcz;
			size_t srclen;
			int incr;
			StrLoc *sfp = &rematch.groups->ml.str;
			int (*sncmp)(const char *s1,const char *s2,size_t n) = (flags & SOpt_Ignore) ? strncasecmp : strncmp;

			// Have plain text pattern.  Scan through the source string.
			rematch.grpct = 0;
			sfp->len = strlen(patp->d_str);
			srclen = strlen(src1 = srcp->d_str);
			if(rightmost) {
				srcz = src1 - 1;
				src1 += srclen + (incr = -1);
				}
			else {
				srcz = src1 + srclen;
				incr = 1;
				}

			do {
				// Scan through the string.  If match found, save results and return.
				if(sncmp(src1,patp->d_str,sfp->len) == 0) {
					sfp->sd.str = src1;
					dsetint((long) (src1 - srcp->d_str),rp);
					return savematch(&rematch);
					}
				} while((src1 += incr) != srcz);
			}
		}

	// No match.
	dsetnil(rp);
	return rc.status;
	}

// Strip whitespace off the beginning (op == -1), the end (op == 1), or both ends (op == 0) of a string.
char *stripstr(char *src,int op) {

	// Trim beginning, if applicable...
	if(op <= 0)
		src = nonwhite(src);

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

// Substitute first occurrence (or all if n > 1) of sstr in sp with rstr and store results in rp.  Ignore case in comparisons
// if flag set in "flags".  Return status.
int strsub(Datum *rp,int n,Datum *sp,char *sstr,char *rstr,ushort flags) {
	int rcode;
	DStrFab dest;
	char *(*strf)(const char *s1,const char *s2);
	char *str = sp->d_str;

	// Return source string if sp or sstr is empty.
	if(*str == '\0' || *sstr == '\0')
		rcode = dsetstr(str,rp);
	else if((rcode = dopenwith(&dest,rp,false)) == 0) {
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
				return drcset();
			str = s + sstrlen;

			// Copy substitution string.
			if(dputmem((void *) rstr,rstrlen,&dest) != 0)
				return drcset();

			// Bail out unless n > 1.
			if(n <= 1)
				break;
			}

		// Copy remainder, if any.
		if((srclen = strlen(str)) > 0 && dputmem((void *) str,srclen,&dest) != 0)
			return drcset();
		rcode = dclose(&dest,sf_string);
		}

	return rcode == 0 ? rc.status : drcset();
	}

#if MMDebug & Debug_Temp
static char *zstr(char *str) {
	static char wkbuf[256];
	char *wb = wkbuf;
	do {
		wb = stpcpy(wb,vizc(*str,VSpace));
		} while(*str++ != '\0');
	return wkbuf;
	}
#endif

// Perform RE substitution(s) in string sp using search pattern spat and replacement pattern rpat.  Save result in rp.  Do
// all occurrences of the search pattern if n > 1; otherwise, first only.  Return status.
static int resub(Datum *rp,int n,Datum *sp,char *spat,char *rpat,ushort flags) {
	DStrFab dest;
	Match match;
	int offset,scanoff;
	ReplMetaChar *rmcp;
	StrLoc *sfp;
	size_t len,lastscanlen;
	ulong loopcount;

	// Return null string if sp is empty.
	if(disnull(sp)) {
		dsetnull(rp);
		return rc.status;
		}

	// Error if search pattern is null.
	if(*spat == '\0')
		return rcset(Failure,0,text187,text266);
			// "%s cannot be null","Regular expression"

	// Save and compile patterns in local "match" variable.
	minit(&match);
	if(newspat(spat,&match,&flags) != Success || mccompile(&match) != Success)
		return rc.status;
	if(newrpat(rpat,&match) != Success || rmccompile(&match) != Success) {
		freespat(&match);
		return rc.status;
		}

	// Begin scan loop.  For each match found in sp, perform substitution and use string result in rp as source string (with
	// an offset) for next iteration.  This is necessary for RE matching to work correctly.
	sfp = &match.groups[0].ml.str;
	loopcount = lastscanlen = scanoff = 0;
	for(;;) {
		// Find next occurrence.
		if(recmp(sp,scanoff,&match,&offset) != Success)
			break;
		if(offset >= 0) {
			// Match found.  Error if we matched an empty string and scan position did not advance; otherwise, we'll
			// go into an infinite loop.
			if(++loopcount > 2 && sfp->len == 0 && strlen(sp->d_str + scanoff) == lastscanlen) {
				(void) rcset(Failure,0,text91);
					// "Repeating match at same position detected"
				break;
				}

			// Open string-fab object.
			if(dopenwith(&dest,rp,false) != 0) {
				(void) drcset();
				break;
				}

			// Copy any source text that is before the match location.
			if(offset > 0 && dputmem((void *) sp->d_str,offset,&dest) != 0) {
				(void) drcset();
				break;
				}

			// Copy replacement pattern to dest.
			if(match.flags & RRegical)
				for(rmcp = match.rmcpat; rmcp->mc_type != MCE_Nil; ++rmcp) {
					if(dputs(rmcp->mc_type == MCE_LitString ? rmcp->u.rstr :
					 rmcp->mc_type == MCE_Match ? match.matchp->d_str :
					 fixnull(match.groups[rmcp->u.grpnum].matchp->d_str),&dest) != 0) {
						(void) drcset();
						goto Retn;
						}
					}
			else if(dputs(match.rpat,&dest) != 0) {
				(void) drcset();
				break;
				}

			// Copy remaining source text to dest if any, and close it.
			if(((len = strlen(sp->d_str + offset + sfp->len)) > 0 &&
			 dputmem((void *) (sp->d_str + offset + sfp->len),len,&dest) != 0) || dclose(&dest,sf_string) != 0) {
				(void) drcset();
				break;
				}

			// If no text remains or repeat count reached, we're done.
			if(len == 0 || n <= 1)
				break;

			// In "find all" mode... keep going.
			lastscanlen = strlen(sp->d_str) - scanoff;
			scanoff = strlen(rp->d_str) - len;
			datxfer(sp,rp);
			}
		else {
			// No match found.  Transfer input to result and bail out.
			datxfer(rp,sp);
			break;
			}
		}
Retn:
	// Finis.  Free pattern space and return.
	freerpat(&match);
	freespat(&match);
	return rc.status;
	}

// Expand character ranges and escaped characters (if any) in a string.  Return status.
int strexpand(DStrFab *sfp,char *estr) {
	int c1,c2;
	char *str;

	if(dopentrk(sfp) != 0)
		return drcset();
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
					if(dputc(c1,sfp) != 0)
						return drcset();
				++str;
				break;
			case '\\':
				if(str[1] != '\0')
					c1 = *++str;
				// Fall through.
			default:
LitChar:
				if(dputc(c1,sfp) != 0)
					return drcset();
			}
		} while(*++str != '\0');

	return dclose(sfp,sf_string) != 0 ? drcset() : rc.status;
	}

// Prepare tr from and to strings.  Return status.
static int trprep(Datum *xfromp,Datum *xtop) {
	DStrFab sf;

	// Expand "from" string.
	if(strexpand(&sf,xfromp->d_str) != Success)
		return rc.status;
	datxfer(xfromp,sf.sf_datp);

	// Expand "to" string.
	if(xtop->d_type == dat_nil)
		dsetnull(xtop);
	else if(*xtop->d_str != '\0') {
		int lenfrom,lento;

		if(strexpand(&sf,xtop->d_str) != Success)
			return rc.status;
		datxfer(xtop,sf.sf_datp);

		if((lenfrom = strlen(xfromp->d_str)) > (lento = strlen(xtop->d_str))) {
			int c = xtop->d_str[lento - 1];
			int n = lenfrom - lento;

			if(dopenwith(&sf,xtop,true) != 0)
				return drcset();
			do {
				if(dputc(c,&sf) != 0)
					return drcset();
				} while(--n > 0);
			if(dclose(&sf,sf_string) != 0)
				return drcset();
			}
		}
	return rc.status;
	}

// Translate a string, given result pointer, source pointer, translate-from string, and translate-to string.  Return status.
// The translate-to string may be null, in which case all translate-from characters are deleted from the result.  It may also
// be shorter than the translate-from string, in which case it is padded to the same length with its last character.
static int tr(Datum *rp,Datum *srcp,Datum *xfromp,Datum *xtop) {
	int lento;
	char *xf;
	char *str;
	DStrFab result;

	// Validate arguments.
	if(strlen(xfromp->d_str) == 0)
		return rcset(Failure,0,text187,text328);
				// "%s cannot be null","tr \"from\" string"
	if(trprep(xfromp,xtop) != Success)
		return rc.status;

	// Scan source string.
	if(dopenwith(&result,rp,false) != 0)
		return drcset();
	str = srcp->d_str;
	lento = strlen(xtop->d_str);
	while(*str != '\0') {

		// Scan lookup table for a match.
		xf = xfromp->d_str;
		do {
			if(*str == *xf) {
				if(lento > 0 && dputc(xtop->d_str[xf - xfromp->d_str],&result) != 0)
					return drcset();
				goto Next;
				}
			} while(*++xf != '\0');

		// No match, copy the source char untranslated.
		if(dputc(*str,&result) != 0)
			return drcset();
Next:
		++str;
		}

	// Terminate and return the result.
	return dclose(&result,sf_string) != 0 ? drcset() : rc.status;
	}

// Concatenate all function arguments into rp if runtime flag OpEval is set; otherwise, just consume them.  reqct is the number
// of required arguments.  Arg_First flag is set on first argument.  Null and nil arguments are included in result if CvtKeepNil
// and/or CvtKeepNull flags are set; otherwise, they are skipped.  A nil argument is output as a keyword if CvtShowNil,
// CvtVizStr, or CvtVizStrQ flag is set; otherwise, a null string.  Boolean arguments are always output as "false" and "true"
// and arrays are processed (recursively) as if each element was specified as an argument.  Return status.
int catargs(Datum *rp,int reqct,Datum *delimp,uint flags) {
	uint aflags = Arg_First | CFBool1 | CFArray1 | CFNIS1;
	DStrFab sf;
	Datum *datp;		// For arguments.
	bool firstWrite = true;
	char *dlm = (delimp != NULL && !disnn(delimp)) ? delimp->d_str : NULL;

	// Nothing to do if not evaluating and no arguments; for example, an "abort()" call.
	if((opflags & (OpScript | OpParens)) == (OpScript | OpParens) && havesym(s_rparen,false) &&
	 (!(opflags & OpEval) || reqct == 0))
		return rc.status;

	if(dnewtrk(&datp) != 0 || ((opflags & OpEval) && dopenwith(&sf,rp,false) != 0))
		return drcset();

	for(;;) {
		if(aflags & Arg_First) {
			if(!havesym(s_any,reqct > 0))
				break;				// Error or no arguments.
			}
		else if(!havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(datp,aflags) != Success)
			return rc.status;
		--reqct;
		if(opflags & OpEval) {

			// Skip nil or null string if appropriate.
			if(datp->d_type == dat_nil) {
				if(!(flags & CvtKeepNil))
					goto Onward;
				}
			else if(disnull(datp) && !(flags & CvtKeepNull))
				goto Onward;

			// Write optional delimiter and value.
			if(dlm != NULL && !firstWrite && dputs(dlm,&sf) != 0)
				return drcset();
			if(dtosfc(&sf,datp,dlm,flags) != Success)
				return rc.status;
			firstWrite = false;
			}
Onward:
		aflags = CFBool1 | CFArray1 | CFNIS1;
		}

	// Return result.
	if((opflags & OpEval) && dclose(&sf,sf_string) != 0)
		(void) drcset();

	return rc.status;
	}

// Process "prompt" user function, given n argument and prompt string in prmtp.  Save result in rp and return status.
static int uprompt(Datum *rp,int n,Datum *prmtp) {
	char *defval = NULL;				// Default value.
	Datum *defargp;
	ushort delim = RtnKey;				// Delimiter.
	int maxlen = 0;
	uint cflags = (n == 0) ? Term_NoKeyEcho : 0;	// Completion flags.

	if((opflags & OpEval) && disnn(prmtp))
		return rcset(Failure,0,"%s %s",text110,text214);
			// "Prompt string required for","'prompt' function"

	// Have "default" argument?
	if(havesym(s_comma,false)) {

		// Yes, get it and use it unless it's nil.
		if(dnewtrk(&defargp) != 0)
			return drcset();
		if(funcarg(defargp,CFNIS1) != Success)
			return rc.status;
		if((opflags & OpEval) && defargp->d_type != dat_nil) {
			if(tostr(defargp) != Success)
				return rc.status;
			defval = defargp->d_str;
			}

		// Have "type" argument?
		if(havesym(s_comma,false)) {
			bool getAnother = true;		// Get one more (optional) argument?

			// Yes, get it (into rp temporarily) and check it.
			if(funcarg(rp,CFNotNull1) != Success)
				return rc.status;
			if(opflags & OpEval) {
				char *str = rp->d_str;
				if(str[0] == '^') {
					cflags |= Term_C_NoAuto;
					++str;
					}
				if(str[0] == '\0' || str[1] != '\0')
					goto PrmtErr;
				switch(*str) {
					case 'c':
						cflags |= Term_OneKey;
						break;
					case 's':
						break;
					case 'b':
						cflags |= Term_C_Buffer;
						maxlen = NBufName;
						goto PrmtComp;
					case 'f':
						cflags |= Term_C_Filename;
						maxlen = MaxPathname;
						goto PrmtComp;
					case 'v':
						cflags |= Term_C_SVar;
						goto PrmtVar;
					case 'V':
						cflags |= Term_C_Var;
PrmtVar:
						maxlen = NVarName;
PrmtComp:
						// Using completion... no more arguments allowed.
						getAnother = false;
						break;
					default:
PrmtErr:
						return rcset(Failure,0,text295,rp->d_str);
							// "prompt type '%s' must be b, c, f, s, v, or V"
					}
				}

			// Have "delimiter" argument?
			if(getAnother && havesym(s_comma,false)) {

				// Yes, get it (into rp temporarily).
				if(funcarg(rp,CFNotNull1) != Success)
					return rc.status;
				if(opflags & OpEval) {
					if(stoek(rp->d_str,&delim) != Success)
						return rc.status;
					if(delim & KeySeq) {
						char wkbuf[16];

						return rcset(Failure,0,text341,ektos(delim,wkbuf),text342);
							// "Cannot use key sequence '%s' as %s delimiter","prompt"
						}
					}
				}
			}
		}

	// Prompt for input if evaluating arguments.
	if(opflags & OpEval)
		(void) terminp(rp,prmtp->d_str,defval,delim,maxlen,0,cflags);
	else
		dsetnull(rp);

	return rc.status;
	}

// Process a strPop, strPush, strShift, or strUnshift function and store result in rp if evaluating; otherwise, just "consume"
// arguments.  Set rp to nil if strShift or strPop and no items left.  Return status.
static int strfunc(Datum *rp,enum cfid fid,char *fname) {
	int status;
	VDesc vd;
	char *str1,*str2,*newvarval;
	bool spacedlm = false;
	Datum *delimp,*oldvarvalp,*argp,newvar,*newvarp;

	// Syntax of functions:
	//	strPush str,dlm,val    strPop str,dlm    strShift str,dlm    strUnshift str,dlm,val

	// Get variable name from current symbol, find it and its value, and validate it.
	if(!havesym(s_any,true))
		return rc.status;
	if(opflags & OpEval) {
		if(dnewtrk(&oldvarvalp) != 0)
			return drcset();
		if(findvar(str1 = last->p_tok.d_str,&vd,OpDelete) != Success)
			return rc.status;
		if((vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_RdOnly)) ||
		 (vd.vd_type == VTyp_NVar && vd.i.vd_argnum == 0))
			return rcset(Failure,0,text164,str1);
				// "Cannot modify read-only variable '%s'"
		if(vderefv(oldvarvalp,&vd) != Success)
			return rc.status;

		// Have var value in oldvarvalp.  Verify that it is nil or string.
		if(oldvarvalp->d_type == dat_nil)
			dsetnull(oldvarvalp);
		else if(!strval(oldvarvalp))
			return rc.status;
		}

	// Get delimiter into delimp.
	if(dnewtrk(&delimp) != 0)
		return drcset();
	if(getsym() < NotFound || funcarg(delimp,CFNil1) != Success)
		return rc.status;
	if(opflags & OpEval) {
		if(delimp->d_type == dat_nil)
			dsetnull(delimp);
		else if(*delimp->d_str == ' ')
			spacedlm = true;
		else if((fid == cf_strShift || fid == cf_strPop) && strlen(delimp->d_str) > 1)
			return rcset(Failure,0,text251,text288,delimp->d_str,1);
				// "%s delimiter '%s' cannot be more than %d character(s)","Function"
		}

	// Get value argument into argp for strPush and strUnshift functions.
	if(fid == cf_strPush || fid == cf_strUnshift) {
		if(dnewtrk(&argp) != 0)
			return drcset();
		if(funcarg(argp,CFNIS1) != Success)
			return rc.status;
		}

	// If not evaluating, we're done (all arguments consumed).
	if(!(opflags & OpEval))
		return rc.status;

	// Evaluating.  Convert value argument to string.
	if((fid == cf_strPush || fid == cf_strUnshift) && tostr(argp) != Success)
		return rc.status;

	// Function value is in argp (if strPush or strUnshift) and "old" var value is in oldvarvalp.  Do function-specific
	// operation.  Copy parsed token to rp if strPop or strShift.  Set newvarval to new value of var in all cases.
	switch(fid) {
		case cf_strPop:					// Get last token from old var value into rp.

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

			if(*(newvarval = oldvarvalp->d_str) == '\0')	// Null var value?
				status = NotFound;
			else {
				str1 = strchr(newvarval,'\0');
				status = rparsetok(rp,&str1,newvarval,spacedlm ? -1 : *delimp->d_str);
				}
			goto Check;
		case cf_strShift:
			newvarval = oldvarvalp->d_str;
			status = parsetok(rp,&newvarval,spacedlm ? -1 : *delimp->d_str);
Check:
			dinit(newvarp = &newvar);
			dsetstrref(newvarval,newvarp);

			if(status != Success) {			// Any tokens left?
				if(rc.status != Success)
					return rc.status;	// Fatal error.

				// No tokens left.  Signal end of token list.
				dsetnil(rp);
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
			str1 = oldvarvalp->d_str;		// First portion of new var value (old var value).
			str2 = argp->d_str;			// Second portion (value to append).
			goto Paste;
		default:	// cf_strUnshift
			str1 = argp->d_str;			// First portion of new var value (value to prepend).
			str2 = oldvarvalp->d_str;		// Second portion (old var value).
Paste:
			{DStrFab sf;

			if(dopenwith(&sf,rp,false) != 0 ||
			 dputs(str1,&sf) != 0)			// Copy initial portion of new var value to work buffer.
				return drcset();

			// Append a delimiter if oldvarvalp is not null, and value (if strPush) or var (if strUnshift).
			if((!disnull(oldvarvalp) && dputs(delimp->d_str,&sf) != 0) || dputs(str2,&sf) != 0 ||
			 dclose(&sf,sf_string) != 0)
				return drcset();
			newvarp = rp;				// New var value.
			}
			break;
			}

	// Update variable and return status.
	(void) putvar(newvarp,&vd);
Retn:
	return rc.status;
	}

#if (MMDebug & Debug_Token) && 0
static void showsym(char *name) {

	fprintf(logfile,"%s(): last is str \"%s\" (%d)\n",name,last->p_tok.d_str,last->p_sym);
	}
#endif

// Determine if given name is defined.  If default n, set rp to result: "alias", "buffer", "command", "pseudo-command",
// "function", "macro", "variable", or nil; otherwise, set rp to true if mark is defined in current buffer; otherwise, false.
static int checkdef(Datum *rp,int n,Datum *namep) {
	char *result = NULL;
	CFABPtr cfab;

	// Null or nil string?
	if(!disnn(namep)) {

		// Mark?
		if(n != INT_MIN) {
			Mark *mkp;

			dsetbool(namep->d_str[1] == '\0' &&
			 mfind(namep->d_str[0],&mkp,n > 0 ? (MkOpt_Query | MkOpt_Viz) : MkOpt_Query) == Success &&
			 mkp != NULL,rp);
			return rc.status;
			}

		// Variable?
		if(findvar(namep->d_str,NULL,OpQuery))
			result = text292;
				// "variable"

		// Command, function, alias, or macro?
		else if(cfabsearch(namep->d_str,&cfab,PtrAny) == 0) {
			switch(cfab.p_type) {
				case PtrCmd:
					result = text158;
						// "command"
					break;
				case PtrPseudo:
					result = text333;
						// "pseudo-command"
					break;
				case PtrFunc:
					result = text247;
						// "function"
					break;
				case PtrBuf:
					result = text83;
						// "buffer"
					break;
				case PtrMacro:
					result = text336;
						// "macro"
					break;
				default:	// PtrAlias
					result = text127;
						// "alias"
				}
			}
		}

	// Return result.
	if(result == NULL)
		dsetnil(rp);
	else if(dsetstr(result,rp) != 0)
		(void) drcset();
	return rc.status;
	}

// Concatenate command-line arguments into rp and insert, overwrite, replace, or write the resulting text to a buffer n times,
// given buffer pointer, text insertion style, and calling function pointer (for error reporting).  If bufp is NULL, use current
// buffer.  If n == 0, do one repetition and don't move point.  If n < 0, do one repetition and process all newline characters
// literally (don't create a new line).  Return status.
int chgtext(Datum *rp,int n,Buffer *bufp,uint t,CmdFunc *cfp) {
	Datum *dtextp;
	DStrFab text;
	EScreen *oscrp = NULL;
	EWindow *owinp = NULL;
	Buffer *obufp = NULL;
	uint aflags = Arg_First | CFBool1 | CFArray1 | CFNIS1;

	if(n == INT_MIN)
		n = 1;

	if(dnewtrk(&dtextp) != 0)
		return drcset();

	// Evaluate all the arguments and save in string-fab object (so that the text can be inserted more than once,
	// if requested).
	if(dopenwith(&text,rp,false) != 0)
		return drcset();

	for(;;) {
		if(aflags & Arg_First) {
			if(!havesym(s_any,true))
				return rc.status;		// Error.
			}
		else if(!havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(dtextp,aflags) != Success)
			return rc.status;
		aflags = CFBool1 | CFArray1 | CFNIS1;
		if(disnn(dtextp))
			continue;				// Ignore null and nil values.
		if(dtextp->d_type == dat_blobRef) {
			if(atosfc(&text,dtextp,NULL,0) != Success)
				return rc.status;
			}
		else if(dputd(dtextp,&text) != 0)		// Add text chunk to string-fab object.
			return drcset();
		}
	if(dclose(&text,sf_string) != 0)
		return drcset();

	// If the target buffer is being displayed in another window, remember current window and move to the other one;
	// otherwise, switch to the buffer in the current window.
	if(bufp != NULL && bufp != curbp) {
		if(bufp->b_nwind == 0) {

			// Target buffer is not being displayed in any window... switch to it in current window.
			obufp = curbp;
			if(bswitch(bufp) != Success)
				return rc.status;
			}
		else {
			// Target buffer is being displayed.  Get window and find screen.
			EWindow *winp = findwind(bufp);
			EScreen *scrp = sheadp;
			EWindow *winp2;
			owinp = curwp;
			do {
				winp2 = scrp->s_wheadp;
				do {
					if(winp2 == winp)
						goto Found;
					} while((winp2 = winp2->w_nextp) != NULL);
				} while((scrp = scrp->s_nextp) != NULL);
Found:
			// If screen is different, switch to it.
			if(scrp != cursp) {
				oscrp = cursp;
				if(sswitch(scrp) != Success)
					return rc.status;
				}

			// If window is different, switch to it.
			if(winp != curwp) {
				wswitch(winp);
				upmode(NULL);
				}
			}
		}

	// We have all the text (in rp).  Now insert, overwrite, or replace it n times.
	if(iortext(rp,n,t,false) == Success) {

		// Restore old screen, window, and/or buffer, if needed.
		if(obufp != NULL)
			(void) bswitch(obufp);
		else if(oscrp != NULL) {
			if(sswitch(oscrp) != Success)
				return rc.status;
			if(owinp != curwp)
				goto RestoreWind;
			}
		else if(owinp != NULL) {
RestoreWind:
			wswitch(owinp);
			upmode(NULL);
			}
		}

	return rc.status;
	}

// Process stat? function.  Return status.
static int ftest(Datum *rp,Datum *filep,Datum *tcodep) {

	if(disnull(tcodep))
		(void) rcset(Failure,0,text187,text335);
			// "%s cannot be null","File test code(s)"
	else {
		struct stat s;
		int result = false;
		char *str,tests[] = "defLlrswx";

		// Validate test code(s).
		for(str = tcodep->d_str; *str != '\0'; ++str) {
			if(strchr(tests,*str) == NULL)
				return rcset(Failure,0,text362,*str);
					// "Unknown file test code '%c'"
			}

		// Get file status.
		if(lstat(filep->d_str,&s) == 0) {

			// Loop through test codes.
			for(str = tcodep->d_str; *str != '\0'; ++str) {
				switch(*str) {
					case 'd':
						result = S_IFDIR;
						goto TypChk;
					case 'e':
						result = true;
						goto SetRetn;
					case 'f':
						result = S_IFREG;
						goto TypChk;
					case 'r':
					case 'w':
					case 'x':
						if((result = access(filep->d_str,*str == 'r' ? R_OK :
						 *str == 'w' ? W_OK : X_OK) == 0))
							goto SetRetn;
						break;
					case 's':
						if((result = s.st_size > 0))
							goto SetRetn;
						break;
					case 'L':
						result = S_IFLNK;
TypChk:
						if((result = (s.st_mode & S_IFMT) == result))
							goto SetRetn;
						break;
					default:	// 'l'
						if((result = (s.st_mode & S_IFMT) == S_IFREG && s.st_nlink > 1))
							goto SetRetn;
					}
				}
SetRetn:;
			}
		dsetbool(result,rp);
		}

	return rc.status;
	}

// Return next argument to strfmt(), "flattening" arrays in the process.  Return status.
static int fmtarg(Datum *rp,uint aflags,ArrayState *asp) {

	for(;;) {
		if(asp->aryp == NULL) {
			if(funcarg(rp,aflags | CFArray1 | CFMay) != Success)
				return rc.status;
			if(rp->d_type != dat_blobRef)
				break;
			asp->aryp = awptr(rp)->aw_aryp;
			asp->i = 0;
			}
		else {
			Array *aryp = asp->aryp;
			if(asp->i == aryp->a_used)
				asp->aryp = NULL;
			else {
				if(datcpy(rp,aryp->a_elpp[asp->i]) != 0)
					return drcset();
				++asp->i;
				break;
				}
			}
		}

	if(aflags == CFInt1)
		(void) intval(rp);
	else if(aflags == CFNil1 && rp->d_type != dat_nil)
		(void) strval(rp);
	return rc.status;
	}

// Build string from "printf" format string (formatp) and following argument(s).  If arg1p is not NULL, process binary format
// (%) expression using arg1p as the argument; otherwise, process sprintf function.  Return status.
int strfmt(Datum *rp,int n,Datum *formatp,Datum *arg1p) {
	int c,specCount;
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

	fmt = formatp->d_str;

	// Create string-fab object for result and work Datum object for sprintf call.
	if(dopenwith(&result,rp,false) != 0 || (arg1p == NULL && dnewtrk(&tp) != 0))
		return drcset();

	// Loop through format string.
	specCount = 0;
	while((c = *fmt++) != '\0') {
		if(c != '%') {
			if(dputc(c,&result) != 0)
				return drcset();
			continue;
			}

		// Check for prefix(es).
		prefix = NULL;					// Assume no prefix.
		flags = 0;					// Reset.
		while((c = *fmt++) != '\0') {
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
				goto GetWidth;
			}
		}

		// Get width.
GetWidth:
		width = 0;
		if(c == '*') {
			if(arg1p != NULL)			// Error if format op (%).
				goto TooMany;
			if(fmtarg(tp,CFInt1,&as) != Success)	// Get next (int) argument for width.
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
				if(fmtarg(tp,CFInt1,&as) != Success)
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
			else if(fmtarg(tp,CFNil1,&as) != Success)
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
			else if(fmtarg(tp,CFInt1,&as) != Success)
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
			else if(fmtarg(tp,CFInt1,&as) != Success)
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
			else if(fmtarg(tp,CFInt1,&as) != Success)
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
					return rcset(Failure,0,text320);
						// "More than one spec in '%%' format string"
				tp = arg1p;
				}
			else if(fmtarg(tp,CFInt1,&as) != Success)
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
			return rcset(Failure,0,text321,c);
				// "Unknown format spec '%%%c'"
		}

		// Concatenate the pieces, which are padding, prefix, more padding, the string, and trailing padding.
		prefLen = (prefix == NULL) ? 0 : strlen(prefix); // Length of prefix string.
		padding = width - (prefLen + sLen);		// # of chars to pad.

		// If 0 padding, store prefix (if any).
		if((flags & FMT0pad) && prefix != NULL) {
			if(dputs(prefix,&result) != 0)
				return drcset();
			prefix = NULL;
			}

		// If right-justified and chars to pad, store prefix string.
		if(padding > 0) {
			if(!(flags & FMTleft)) {
				int c = (flags & FMT0pad) ? '0' : ' ';
				while(--padding >= 0)
					if(dputc(c,&result) != 0)
						return drcset();
				}
			}

		// Store prefix (if any).
		if(prefix != NULL && dputs(prefix,&result) != 0)
			return drcset();

		// Store (fixed-length) string.
		if(dputmem((void *) str,sLen,&result) != 0)
			return drcset();

 		// Store right padding.
		if(flags & FMTleft) {
			while(--padding >= 0)
				if(dputc(' ',&result) != 0)
					return drcset();
			}
		}

	// End of format string.  Check for errors and return.
	if(specCount == 0 && arg1p != NULL)
		(void) rcset(Failure,0,text281);
			// "Missing spec in '%%' format string"
	else if(as.aryp != NULL && as.i < as.aryp->a_used)
		(void) rcset(Failure,0,text377);
			// "Too many arguments for 'sprintf' function"
	else if(dclose(&result,sf_string) != 0)
		(void) drcset();
	return rc.status;
	}

// Get a kill n (of unlimited size) and save in rp.  May be a null string.  Return status.
static int getkill(Datum *rp,int n) {
	char *str;			// Pointer into KillBuf block data chunk.
	KillBuf *kbp;			// Pointer to the current KillBuf block.
	Kill *kp;
	int counter;			// Index into data chunk.
	DStrFab kill;

	// Which kill?
	if(n > 0 || n <= -NRing)
		return rcset(Failure,0,text19,n,-(NRing - 1));
				// "No such kill %d (max %d)"
	else if((kp = kringp + n) < kring)
		kp += NRing;

	// If no kill buffer, nothing to do!
	if((kbp = kp->kbufh) == NULL) {
		dsetnull(rp);
		return rc.status;
		}

	// Set up the output object.
	if(dopenwith(&kill,rp,false) != 0)
		return drcset();

	// Backed up characters?
	if((counter = kp->kskip) > 0) {
		str = kbp->kl_chunk + counter;
		while(counter++ < KBlock)
			if(dputc(*str++,&kill) != 0)
				return drcset();
		kbp = kbp->kl_next;
		}

	if(kbp != NULL) {
		while(kbp != kp->kbufp) {
			str = kbp->kl_chunk;
			for(counter = 0; counter < KBlock; ++counter)
				if(dputc(*str++,&kill) != 0)
					return drcset();
			kbp = kbp->kl_next;
			}
		counter = kp->kused;
		str = kbp->kl_chunk;
		while(counter--)
			if(dputc(*str++,&kill) != 0)
				return drcset();
		}

	// and return the reconstructed value.
	return dclose(&kill,sf_string) == 0 ? rc.status : drcset();
	}

// Clone an array.  Return status.
int aryclone(Datum *destp,Datum *srcp,int depth) {
	Array *aryp;

	if(maxarydepth > 0 && depth > maxarydepth)
		return rcset(Failure,0,text319,Literal23,maxarydepth);
			// "Maximum %s recursion depth (%d) exceeded","array"
	if((aryp = aclone(awptr(srcp)->aw_aryp)) == NULL)
		return drcset();
	if(awrap(destp,aryp) == Success) {
		ArraySize n = aryp->a_used;
		Datum **elpp = aryp->a_elpp;

		// Check for nested arrays.
		while(n-- > 0) {
			if((*elpp)->d_type == dat_blobRef && aryclone(*elpp,*elpp,depth + 1) != Success)
				return rc.status;
			++elpp;
			}
		}
	return rc.status;
	}

// Execute a system command or function, given result pointer, n argument, and pointer into the command-function table (cftab).
// Return status.  This is the execution routine for all commands and functions.  When script mode is active (runtime flag
// OpScript is set), arguments (if any) are preloaded and validated per descriptors in the table, then made available to the
// code for the specific command or function (which is here or in a separate routine).  If runtime flag OpEval is not set,
// arguments are "consumed" only and the execution code is bypassed if the maximum number of arguments for the command or
// function has been loaded.  Note that any command with CFNCount flag set must handle a zero n so that execCF() calls in
// routines other than fcall() will always work (for any value of n).
int execCF(Datum *rp,int n,CmdFunc *cfp,int minArgs,int maxArgs) {
	int i;
	char *str;
	long lval;
	Datum *argp[CFMaxArgs + 1];	// Argument values plus NULL terminator.
	int argct = 0;			// Number of arguments "loaded".
	int fnum = cfp - cftab;

	// If script mode and CFNoLoad is not set, retrieve the minimum number of arguments specified in cftab, up to the
	// maximum, and save in argp.  If CFSpecArgs or CFShrtLoad flag is set or the maximum is negative, use the minimum for
	// the maximum.
	if((opflags & (OpScript | OpNoLoad)) != OpScript || (cfp->cf_aflags & CFNoLoad) ||
	 ((opflags & OpParens) && havesym(s_rparen,false)))
		argp[0] = NULL;
	else {
		if(cfp->cf_aflags & CFShrtLoad)
			--minArgs;
		if((cfp->cf_aflags & CFSpecArgs) || cfp->cf_maxArgs < 0)
			maxArgs = minArgs;
		else if(cfp->cf_aflags & CFShrtLoad)
			--maxArgs;
		if(maxArgs > 0) {
			Datum **argpp = argp;
			do {
				if(dnewtrk(argpp) != 0)
					return drcset();
				if(funcarg(*argpp++,(argct == 0 ? Arg_First : 0) | (((cfp->cf_vflags & (CFNotNull1 << argct)) |
				 (cfp->cf_vflags & (CFNil1 << argct)) | (cfp->cf_vflags & (CFBool1 << argct)) |
				 (cfp->cf_vflags & (CFInt1 << argct)) | (cfp->cf_vflags & (CFArray1 << argct)) |
				 (cfp->cf_vflags & (CFNIS1 << argct))) >> argct) | (cfp->cf_vflags & CFMay)) != Success)
					return rc.status;
				} while(++argct < minArgs || (argct < maxArgs && havesym(s_comma,false)));
			*argpp = NULL;

			// If not evaluating, skip code execution if ordinary command or function and maximum number of
			// arguments was consumed.
			if(!(opflags & OpEval) && !(cfp->cf_aflags & CFSpecArgs) && argct == cfp->cf_maxArgs)
				return rc.status;
			}
		}

	// Evaluate the command or function.
	if(cfp->cf_func != NULL)
		(void) cfp->cf_func(rp,n,argp);
	else {
		switch(fnum) {
			case cf_abs:
				dsetint(labs(argp[0]->u.d_int),rp);
				break;
			case cf_alterBufMode:
				i = 3;
				goto AlterMode;
			case cf_alterDefMode:
				i = MdRec_Default;
				goto AlterMode;
			case cf_alterGlobalMode:
				i = MdRec_Global;
				goto AlterMode;
			case cf_alterShowMode:
				i = MdRec_Show;
AlterMode:
				(void) adjustmode(rp,n,i,NULL);
				break;
			case cf_appendFile:
				i = 'a';
				goto AWFile;
			case cf_backPageNext:
				// Scroll the next window up (backward) a page.
				(void) wscroll(rp,n,nextWind,backPage);
				break;
			case cf_backPagePrev:
				// Scroll the previous window up (backward) a page.
				(void) wscroll(rp,n,prevWind,backPage);
				break;
			case cf_backTab:
				// Move the point backward "n" tab stops.
				(void) bftab(n == INT_MIN ? -1 : -n);
				break;
			case cf_basename:
				if(dsetstr(fbasename(argp[0]->d_str,n == INT_MIN || n > 0),rp) != 0)
					(void) drcset();
				break;
			case cf_beep:
				// Beep the beeper n times.
				if(n == INT_MIN)
					n = 1;
				else if(n < 0 || n > 10)
					return rcset(Failure,0,text12,text137,n,0,10);
						// "%s (%d) must be between %d and %d","Repeat count"
				while(n-- > 0)
					if(TTbeep() != Success)
						break;
				break;
			case cf_beginBuf:
				// Goto the beginning of the buffer.
				str = text326;
					// "Begin"
				i = false;
				goto BEBuf;
			case cf_beginLine:
				// Move the point to the beginning of the [-]nth line.
				(void) beline(rp,n,false);
				break;
			case cf_beginWhite:
				// Move the point to the beginning of white space surrounding dot.
				(void) spanwhite(false);
				break;
			case cf_binding:
				{ushort ek;
				if(stoek(argp[0]->d_str,&ek) != Success)
					break;
				str = fixnull(getkname(getbind(ek)));
				}
				if(*str == '\0')
					dsetnil(rp);
				else if(dsetstr(*str == SBMacro ? str + 1 : str,rp) != 0)
					(void) drcset();
				break;
			case cf_bufBoundQ:
				if(n != INT_MIN)
					n = n > 0 ? 1 : n < 0 ? -1 : 0;
				i = (curwp->w_face.wf_dot.lnp == curbp->b_hdrlnp) ? 1 :
				 (curwp->w_face.wf_dot.lnp == lforw(curbp->b_hdrlnp) && curwp->w_face.wf_dot.off == 0) ? -1 : 0;
				dsetbool((n == INT_MIN && i != 0) || i == n,rp);
				break;
			case cf_bufList:
				getbuflist(rp,n);
				break;
			case cf_bufSize:
				{long bct,lct;
				bct = buflength(curbp,&lct);
				dsetint(n == INT_MIN ? lct : bct,rp);
				}
				break;
			case cf_chr:
				dsetchr(argp[0]->u.d_int,rp);
				break;
			case cf_clearKillRing:
				i = NRing;
				do {
					kcycle();
					} while(--i > 0);
				(void) rcset(Success,0,text228);
					// "Kill ring cleared"
				break;
			case cf_clearMsg:
				mlerase(n > 0 ? MLForce : 0);
				break;
			case cf_clone:
				(void) aryclone(rp,argp[0],0);
				break;
			case cf_copyFencedText:
				// Copy text to kill ring.
				i = 1;
				goto KDCFenced;
			case cf_copyLine:
				// Copy line(s) to kill ring.
				i = 1;
				goto KDCLine;
			case cf_copyRegion:
				// Copy all of the characters in the region to the kill ring.  Don't move dot at all.
				{Region region;

				if(getregion(&region,NULL) != Success || copyreg(&region) != Success)
					break;
				}
				(void) rcset(Success,0,text70);
					// "Region copied"
				break;
			case cf_copyToBreak:
				// Copy text to kill ring.
				i = 1;
				goto KDCText;
			case cf_copyWord:
				// Copy word(s) to kill buffer without moving point.
				(void) (n == INT_MIN ? kdcfword(1,1) : n < 0 ? kdcbword(-n,1) : kdcfword(n,1));
				break;
			case cf_metaPrefix:
			case cf_negativeArg:
			case cf_prefix1:
			case cf_prefix2:
			case cf_prefix3:
			case cf_universalArg:
				break;
			case cf_cycleKillRing:
				// Cycle the kill ring forward or backward.
				(void) cycle_ring(n,true);
				break;
			case cf_definedQ:
				(void) checkdef(rp,n,argp[0]);
				break;
			case cf_deleteBackChar:
				// Delete char backward.  Return status.
				(void) ldelete(n == INT_MIN ? -1L : (long) -n,0);
				break;
			case cf_deleteBackTab:
				// Delete tab backward.  Return status.
				(void) deltab(n == INT_MIN ? -1 : -n);
				break;
			case cf_deleteFencedText:
				// Delete text without saving in kill ring.
				i = 0;
				goto KDCFenced;
			case cf_deleteForwChar:
				// Delete char forward.  Return status.
				(void) ldelete(n == INT_MIN ? 1L : (long) n,0);
				break;
			case cf_deleteForwTab:
				// Delete tab forward.  Return status.
				(void) deltab(n);
				break;
			case cf_deleteLine:
				// Delete line(s) without saving text in kill ring.
				i = 0;
				goto KDCLine;
			case cf_deleteRegion:
				// Delete region without saving text in kill ring.
				i = false;
				goto DKReg;
			case cf_deleteToBreak:
				// Delete text without saving in kill ring.
				i = 0;
				goto KDCText;
			case cf_deleteWhite:
				// Delete white space surrounding point on current line.
				(void) delwhite(n);
				break;
			case cf_deleteWord:
				// Delete word(s) without saving text in kill buffer.
				(void) (n == INT_MIN ? kdcfword(1,0) : n < 0 ? kdcbword(-n,0) : kdcfword(n,0));
				break;
			case cf_dirname:
				if(dsetstr(fdirname(argp[0]->d_str,n),rp) != 0)
					(void) drcset();
				break;
			case cf_emptyQ:
				dsetbool(argp[0]->d_type == dat_nil ? true : argp[0]->d_type & DStrMask ?
				 *argp[0]->d_str == '\0' : awptr(argp[0])->aw_aryp->a_used == 0,rp);
				break;
			case cf_endBuf:
				// Move to the end of the buffer.
				str = text188;
					// "End"
				i = true;
BEBuf:
				(void) bufop(rp,n,str,BOpBeginEnd,i);
				break;
			case cf_endLine:
				// Move the point to the end of the [-]nth line.
				(void) beline(rp,n,true);
				break;
			case cf_endWhite:
				// Move the point to the end of white space surrounding dot.
				(void) spanwhite(true);
				break;
			case cf_env:
				if(dsetstr(fixnull(getenv(argp[0]->d_str)),rp) != 0)
					(void) drcset();
				break;
			case cf_findFile:
				i = false;
				goto FVFile;
			case cf_forwPageNext:
				// Scroll the next window down (forward) a page.
				(void) wscroll(rp,n,nextWind,forwPage);
				break;
			case cf_forwPagePrev:
				// Scroll the previous window down (forward) a page.
				(void) wscroll(rp,n,prevWind,forwPage);
				break;
			case cf_forwTab:
				// Move the point forward "n" tab stops.
				(void) bftab(n == INT_MIN ? 1 : n);
				break;
			case cf_getKey:
				{ushort ek;
				char keybuf[16];

				if(n == INT_MIN)
					n = 1;
				if((n <= 1 ? getkey(&ek) : getkseq(&ek,NULL)) != Success)
					break;
				if(ek == corekeys[CK_Abort].ek)
					return abortinp();
				if(n <= 0)
					dsetint(ektoc(ek),rp);
				else if(dsetstr(ektos(ek,keybuf),rp) != 0)
					(void) drcset();
				}
				break;
			case cf_gotoFence:
				// Move the point to a matching fence.
				{Region region;

				if(otherfence(&region) == 0)
					(void) rcset(Failure,0,NULL);
				}
				break;
			case cf_growWind:
				// Grow (enlarge) the current window.  If n is negative, take lines from the upper window;
				// otherwise, lower.
				i = 1;
				goto GSWind;
			case cf_hideBuf:
				i = BFHidden;
				str = text195;
					// "Hide"
				goto AltFlag;
			case cf_includeQ:
				{Array *aryp = awptr(argp[0])->aw_aryp;
				ArraySize len = aryp->a_used;
				Datum **elpp = aryp->a_elpp;
				bool match;
				i = false;
				while(len-- > 0) {
					if((*elpp)->d_type == dat_blobRef) {
						if(argp[1]->d_type == dat_blobRef) {
							if(aryeq(argp[1],*elpp,&match) != Success)
								return rc.status;
							if(match)
								goto Found;
							}
						}
					else if(dateq(argp[1],*elpp)) {
Found:
						i = true;
						break;
						}
					++elpp;
					}
				dsetbool(i,rp);
				}
				break;
			case cf_index:
				(void) sindex(rp,argp[0],argp[1],n > 0);
				break;
			case cf_insert:
				(void) chgtext(rp,n,NULL,Txt_Insert,cfp);
				break;
			case cf_insertSpace:
				// Insert space(s) forward into text without moving point.
				if(n != 0) {
					if(n == INT_MIN)
						n = 1;
					if(linsert(n,' ') == Success)
						(void) backch(n);
					}
				break;
			case cf_join:
				if(needsym(s_comma,true))
					(void) catargs(rp,1,argp[0],n == INT_MIN || n > 0 ? CvtKeepAll :
					 n == 0 ? CvtKeepNull : 0);
				break;
			case cf_kill:
				(void) getkill(rp,(int) argp[0]->u.d_int);
				break;
			case cf_killFencedText:
				// Delete text and save in kill ring.
				i = -1;
KDCFenced:
				(void) kdcfencedreg(i);
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
				// Delete word(s) and save text in kill buffer.
				(void) (n == INT_MIN ? kdcfword(1,-1) : n < 0 ? kdcbword(-n,-1) : kdcfword(n,-1));
				break;
			case cf_lastBuf:
				if(lastbufp != NULL) {
					Buffer *oldbufp = curbp;
					if(render(rp,1,lastbufp,0) == Success && n != INT_MIN && n < 0) {
						char bname[NBufName + 1];

						strcpy(bname,oldbufp->b_bname);
					 	if(bdelete(oldbufp,0) == Success)
							(void) rcset(Success,0,text372,bname);
									// "Deleted buffer '%s'"
						}
					}
				break;
			case cf_lcLine:
				// Lower case line.
				str = lowcase;
				goto CaseLine;
			case cf_lcRegion:
				// Lower case region.
				str = lowcase;
				goto CaseReg;
			case cf_lcString:
				i = -1;
				goto CaseStr;
			case cf_length:
				dsetint(argp[0]->d_type == dat_blobRef ? (long) awptr(argp[0])->aw_aryp->a_used :
				 (long) strlen(argp[0]->d_str),rp);
				break;
			case cf_match:
				if(argp[0]->u.d_int < 0 || argp[0]->u.d_int >= MaxGroups)
					return rcset(Failure,0,text5,argp[0]->u.d_int,MaxGroups - 1);
						// "Group number %ld must be between 0 and %d"
				if(dsetstr(fixnull((n == INT_MIN ? &rematch :
				 &srch.m)->groups[(int) argp[0]->u.d_int].matchp->d_str),rp) != 0)
					(void) drcset();
				break;
			case cf_moveWindDown:
				// Move the current window down by "n" lines and compute the new top line in the window.
				(void) moveWindUp(rp,n == INT_MIN ? -1 : -n,argp);
				break;
			case cf_newline:
				i = true;
				goto InsNLSPC;
			case cf_nextBuf:
				// Switch to the next buffer in the buffer list.
				i = false;
				goto PNBuf;
			case cf_nilQ:
				dsetbool(argp[0]->d_type == dat_nil,rp);
				break;
			case cf_nullQ:
				dsetbool(disnull(argp[0]),rp);
				break;
			case cf_numericQ:
				dsetbool(asc_long(argp[0]->d_str,&lval,true),rp);
				break;
			case cf_ord:
				dsetint((long) argp[0]->d_str[0],rp);
				break;
			case cf_overwrite:
				(void) chgtext(rp,n,NULL,Txt_OverWrt,cfp);
				break;
			case cf_pathname:
				(void) getpath(rp,n,argp[0]->d_str);
				break;
			case cf_pause:
				// Set default argument if none.
				if(n == INT_MIN)
					n = 100;		// Default is 1 second.
				else if(n < 0)
					return rcset(Failure,0,text39,text119,n,0);
						// "%s (%d) must be %d or greater","Pause duration"
				cpause(n);
				break;
			case cf_pop:
			case cf_shift:
				{Array *aryp = awptr(argp[0])->aw_aryp;
				if((argp[1] = (fnum == cf_pop) ? apop(aryp) : ashift(aryp)) == NULL)
					dsetnil(rp);
				else {
					datxfer(rp,argp[1]);
					if(rp->d_type == dat_blobRef)
						agarbpush(rp);
					}
				}
				break;
			case cf_push:
			case cf_unshift:
				{Array *aryp = awptr(argp[0])->aw_aryp;
				if(((fnum == cf_push) ? apush(aryp,argp[1]) : aunshift(aryp,argp[1])) != 0)
					(void) drcset();
				else
					datxfer(rp,argp[0]);
				}
				break;
			case cf_prevBuf:
				// Switch to the previous buffer in the buffer list.
				i = true;
PNBuf:
				(void) pnbuffer(rp,n,i);
				break;
			case cf_prevScreen:
				// Bring the previous screen number to the front.

				// Determine screen number if no argument.
				if(n == INT_MIN && (n = cursp->s_num - 1) == 0)
					n = scrcount();

				(void) nextScreen(rp,n,argp);
				break;
			case cf_print:
				// Concatenate all arguments into rp.
				if(catargs(rp,1,NULL,n != INT_MIN && n <= 0 ? CvtKeepNil | CvtShowNil : 0) != Success)
					return rc.status;

				// Write the message out.
				(void) mlputv(n == INT_MIN || n == 0 ? MLHome : MLHome | MLForce,rp);
				break;
			case cf_prompt:
				(void) uprompt(rp,n,argp[0]);
				break;
			case cf_queryReplace:
				// Search and replace with query.
				i = true;
				goto Repl;
			case cf_quickExit:
				// Quick exit from Emacs.  If any buffer has changed, do a save on that buffer first.
				if(savebufs(1,true) == Success)
					(void) rcset(UserExit,0,"");
				break;
			case cf_quote:
				// Convert any value into a string form which will resolve to the original value if subsequently
				// evaluated as an expression (unless "force" is set (n > 0) and value is an array that includes
				// itself).
				{DStrFab sf;

				if(dopenwith(&sf,rp,false) != 0)
					return drcset();
				if(dtosfc(&sf,argp[0],NULL,n > 0 ? CvtExpr | CvtForceArray : CvtExpr) != Success)
					return rc.status;
				if(dclose(&sf,sf_string) != 0)
					return drcset();
				}
				break;
			case cf_rand:
				dsetint(xorshift64star(argp[0]->u.d_int),rp);
				break;
			case cf_readFile:
				// Read a file into a buffer.

				// Get the filename...
				if(gtfilename(rp,n < 0 && n != INT_MIN ? text299 : text131,0) != Success ||
				 (!(opflags & OpScript) && rp->d_type == dat_nil))
						// "Pop file","Read file"
					break;

				// and return results.
				(void) rdfile(rp,n,rp->d_str,false);
				break;
			case cf_redrawScreen:
				// Redraw and possibly reposition dot in the current window.  If n is zero, redraw only;
				// otherwise, reposition dot per the standard redisplay code.
				if(n == 0)
					opflags |= OpScrRedraw;
				else {
					if(n == INT_MIN)
						n = 0;		// Default to 0 to center current line in window.
					curwp->w_force = n;
					curwp->w_flags |= WFForce;
					}
				break;
			case cf_replace:
				// Search and replace.
				i = false;
Repl:
				(void) replstr(i ? rp : NULL,n,argp);
				break;
			case cf_replaceText:
				(void) chgtext(rp,n,NULL,Txt_Replace,cfp);
				break;
			case cf_restoreBuf:
				// Restore the saved buffer.
				if(savbufp == NULL)
					return rcset(Failure,0,text208,text83);
							// "No saved %s to restore","buffer"
				if(bswitch(savbufp) == Success && dsetstr(curbp->b_bname,rp) != 0)
					(void) drcset();
				break;
			case cf_restoreWind:
				// Restore the saved window.
				{EWindow *winp;

				// Find the window.
				winp = wheadp;
				do {
					if(winp == savwinp) {
						curwp->w_flags |= WFMode;
						wswitch(winp);
						curwp->w_flags |= WFMode;
						return rc.status;
						}
					} while((winp = winp->w_nextp) != NULL);
				}
				(void) rcset(Failure,0,text208,text331);
					// "No saved %s to restore","window"
				break;
			case cf_saveBuf:
				// Save pointer to current buffer.
				savbufp = curbp;
				if(dsetstr(curbp->b_bname,rp) != 0)
					(void) drcset();
				break;
			case cf_saveFile:
				// Save the contents of the current buffer (or all buffers if n arg) to their associated files.
				(void) savebufs(n,false);
				break;
			case cf_saveWind:
				// Save pointer to current window.
				savwinp = curwp;
				break;
			case cf_setWrapCol:
				// Set wrap column to n.
				if(n == INT_MIN)
					n = 0;
				else if(n < 0)
					(void) rcset(Failure,0,text39,text59,n,0);
							// "%s (%d) must be %d or greater","Wrap column"
				else {
					wrapcol = n;
					(void) rcset(Success,0,"%s%s%d",text59,text278,n);
							// "Wrap column"," set to "
					}
				break;
			case cf_shQuote:
				if(tostr(argp[0]) == Success && dshquote(argp[0]->d_str,rp) != 0)
					(void) drcset();
				break;
			case cf_shrinkWind:
				// Shrink the current window.  If n is negative, give lines to the upper window; otherwise,
				// lower.
				i = -1;
GSWind:
				(void) gswind(rp,n,i);
				break;
			case cf_space:
				i = false;
InsNLSPC:
				(void) insnlspace(rp,n,i);
				break;
			case cf_sprintf:
				(void) strfmt(rp,n,argp[0],NULL);
				break;
			case cf_statQ:
				(void) ftest(rp,argp[0],argp[1]);
				break;
			case cf_strPop:
			case cf_strPush:
			case cf_strShift:
			case cf_strUnshift:
				(void) strfunc(rp,fnum,cfp->cf_name);
				break;
			case cf_stringFit:
				if(argp[1]->u.d_int < 0)
					return rcset(Failure,0,text39,text290,(int) argp[1]->u.d_int,0);
							// "%s (%d) must be %d or greater","Length argument"
				if(dsalloc(rp,argp[1]->u.d_int + 1) != 0)
					return drcset();
				strfit(rp->d_str,argp[1]->u.d_int,argp[0]->d_str,0);
				break;
			case cf_strip:
				if(dsetstr(stripstr(argp[0]->d_str,n == INT_MIN ? 0 : n),rp) != 0)
					(void) drcset();
				break;
			case cf_sub:
				if(n < 0 && n != INT_MIN)
					return rcset(Failure,0,text39,text137,n,0);
						// "%s (%d) must be %d or greater","Repeat count",
				if(n == 0)
					datxfer(rp,argp[0]);
				else {
					ushort flags = 0;
					int (*sub)(Datum *rp,int n,Datum *sp,char *spat,char *rpat,ushort flags);
					if(argp[1]->d_type == dat_nil)
						dsetnull(argp[1]);
					else
						(void) chkopts(argp[1]->d_str,&flags);
					sub = (flags & SOpt_Regexp) ? resub : strsub;
					(void) sub(rp,n,argp[0],argp[1]->d_str,argp[2]->d_type == dat_nil ? "" :
					 argp[2]->d_str,flags);
					}
				break;
			case cf_subLine:
				{long lval2 = argct < 2 ? LONG_MAX : argp[1]->u.d_int;
				lval = argp[0]->u.d_int;
				if(lval2 != 0 && (i = lused(curwp->w_face.wf_dot.lnp)) > 0) {

					// Determine line offset and length and validate them.  Offset is position relative to
					// dot.
					lval += curwp->w_face.wf_dot.off;
					if(lval >= 0 && lval < i && (lval2 >= 0 || (lval2 = i - lval + lval2) > 0)) {
						if(lval2 > i - lval)
							lval2 = i - lval;
						if(dsetsubstr(ltext(curwp->w_face.wf_dot.lnp) + (int) lval,(size_t) lval2,rp)
						 != 0)
							return drcset();
						}
					else
						dsetnull(rp);
					}
				else
					dsetnull(rp);
				}
				break;
			case cf_subString:
				{long lval2 = argp[1]->u.d_int;
				long lval3 = argct < 3 ? LONG_MAX : argp[2]->u.d_int;
				if(lval3 != 0 && (lval = strlen(argp[0]->d_str)) > 0 && (lval2 < 0 ? -lval2 - 1 :
				 lval2) < lval) {
					if(lval2 < 0)					// Negative offset.
						lval2 += lval;
					lval -= lval2;					// Maximum bytes can copy.
					if(lval3 > 0 || (lval3 += lval) > 0) {
						if(dsetsubstr(argp[0]->d_str + lval2,(size_t) (lval3 <= lval ? lval3 : lval),
						 rp) != 0)
							return drcset();
						}
					else
						dsetnull(rp);
					}
				else
					dsetnull(rp);
				}
				break;
			case cf_sysInfo:
				if(dsetstr(n == INT_MIN ? OSName : n == 0 ? Myself : n > 0 ? Version : Language,rp) != 0)
					(void) drcset();
				break;
			case cf_tab:
				// Process a tab.  Normally bound to ^I.
				(void) instab(n == INT_MIN ? 1 : n);
				break;
			case cf_tcString:
				i = 0;
				goto CaseStr;
			case cf_toInt:
				datxfer(rp,argp[0]);
				(void) toint(rp);
				break;
			case cf_toString:
				if(n == INT_MIN && argp[0]->d_type != dat_blobRef) {
					datxfer(rp,argp[0]);
					(void) tostr(rp);
					}
				else {
					DStrFab sf;

					if(dopenwith(&sf,rp,false) != 0)
						goto DFail;
					if(dtosfc(&sf,argp[0],NULL,n == INT_MIN ? 0 : n < 0 ? CvtKeepNil | CvtShowNil :
					 n == 0 ? CvtKeepAll | CvtForceArray | CvtVizStr :
					 CvtKeepAll | CvtForceArray | CvtVizStrQ) != Success)
						break;
					if(dclose(&sf,sf_string) != 0)
DFail:
						(void) drcset();
					}
				break;
			case cf_tr:
				(void) tr(rp,argp[0],argp[1],argp[2]);
				break;
			case cf_truncBuf:
				// Truncate buffer.  Delete all text from current buffer position to end and save in undelete
				// buffer.  Set rp to buffer name and return status.
				if(dsetstr(curbp->b_bname,rp) != 0)
					return drcset();
				if(curwp->w_face.wf_dot.lnp == curbp->b_hdrlnp)
					break;			// No op if currently at end of buffer.

				// Delete maximum possible, ignoring any end-of-buffer error.
				kdelete(&undelbuf);
				(void) ldelete(LONG_MAX,DFDel);
				break;
			case cf_typeQ:
				(void) dsetstr(dtype(argp[0],true),rp);
				break;
			case cf_ucLine:
				// Upper case line.
				str = upcase;
CaseLine:
				(void) caseline(n,str);
				break;
			case cf_ucRegion:
				// Upper case region.
				str = upcase;
CaseReg:
				(void) caseregion(n,str);
				break;
			case cf_ucString:
				i = 1;
CaseStr:
				{char *(*mk)(char *dest,char *src);
				mk = (i <= 0) ? mklower : mkupper;
				if(dsalloc(rp,strlen(argp[0]->d_str) + 1) != 0)
					return drcset();
				mk(rp->d_str,argp[0]->d_str);
				if(i == 0)
					*rp->d_str = upcase[(int) *argp[0]->d_str];
				}
				break;
			case cf_unchangeBuf:
				// Clear a buffer's "changed" flag.
				i = BFChgd;
				str = text250;
					// "Unchange"
				goto AltFlag;
			case cf_undelete:
				// Insert text from the undelete buffer.
				(void) iortext(NULL,n,Txt_Insert,false);
				break;
			case cf_unhideBuf:
				i = BFHidden;
				str = text186;
					// "Unhide"
AltFlag:
				(void) bufop(rp,n,str,str == text195 ? BOpSetFlag : BOpClrFlag,i);
				break;
			case cf_updateScreen:
				(void) update(n > 0);
				break;
			case cf_viewFile:
				i = true;
FVFile:
				(void) getfile(rp,n,i);
				break;
			case cf_windList:
				getwindlist(rp,n);
				break;
			case cf_wordCharQ:
				dsetbool(wordlist[(int) argp[0]->d_str[0]],rp);
				break;
			case cf_writeFile:
				i = 'w';
AWFile:
				(void) fileout(rp,n,i == 'w' ? text144 : text218,i);
						// "Write file: ","Append file"
				break;
			case cf_xPathname:
				if(pathsearch(&str,argp[0]->d_str,false) != Success)
					return rc.status;
				if(str == NULL)
					dsetnil(rp);
				else if(dsetstr(str,rp) != 0)
					return drcset();
				break;
			case cf_yank:
				// Yank text from the kill buffer.
				if(n == INT_MIN)
					n = 1;
				(void) iortext(NULL,n,Txt_Insert,true);
				break;
			}
		}

	// Command or function call completed.  Extra arguments in script mode are checked in fcall() so no need to do it here.
	return rc.status == Success ? rcsave() : rc.status;
	}

// Evaluate a string literal and return result.  src is assumed to begin and end with ' or ".  In single-quoted strings,
// escaped backslashes '\\' and apostrophes '\'' are recognized (only); in double-quoted strings, escaped backslashes "\\",
// double quotes "\"", special letters (like "\n" and "\t"), \nnn octal and hexadecimal sequences, and Ruby-style interpolated
// #{} expressions are recognized (and executed); e.g., "Values are #{sub "#{join ',',values,final}",',',delim} [#{ct}]".
int evalslit(Datum *rp,char *src) {
	int termch,c;
	char *src0;
	DStrFab result;		// Put object.

	// Get ready.
	if((opflags & OpEval) && dopenwith(&result,rp,false) != 0)
		return drcset();
	termch = *src;
	src0 = src++;

	// Loop until null or string terminator hit.
	while((c = *src) != termch) {
#if MMDebug & Debug_Token
		if(c == '\0')
			return rcset(Failure,0,"String terminator %c not found in '%s'",termch,src0);
#endif
		// Process escaped characters.
		if(c == '\\') {
			if(*++src == '\0')
				break;

			// Do single-quote processing.
			if(termch == '\'') {

				// Check for escaped backslash or apostrophe only.
				if(*src == '\\' || *src == '\'')
					c = *src++;
				}

			// Do double-quote processing.
			else {
				// Initialize variables for \nn parsing, if any.  \0x... and \x... are hex; otherwise, octal.
				int c2;
				int base = 8;
				int maxlen = 3;
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
						while((c2 = *src) != '\0' && maxlen > 0) {
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
								goto BadSeq;
							++src;
							--maxlen;
							}

						// Any digits decoded?
						if(src == src1)
							goto LitChar;

						// Null?
						if(c == 0)
BadSeq:
							return rcset(Failure,0,text337,strsamp(src0,strlen(src0),
							 (size_t) term.t_ncol * 3 / 10));
								// "Invalid \\nn sequence in string %s"
						// Valid sequence.
						break;
					default:	// Literal character.
LitChar:
						c = src[-1];
					}
				}
			}

		// Not a backslash.  Check for beginning of interpolation.
		else if(termch == '"' && c == TokC_Expr && src[1] == TokC_ExprBegin) {
			Datum *datp;

			// "#{" found.  Execute what follows to "}" as a command line.
			if(dnewtrk(&datp) != 0)
				return drcset();
			if(doestmt(datp,src + 2,TokC_ExprEnd,&src) != Success)
				return rc.status;

			// Append the result to dest.
			if((opflags & OpEval) && datp->d_type != dat_nil) {
				if(tostr(datp) != Success)
					return rc.status;
				if(dputd(datp,&result) != 0)
					return drcset();
				}

			// Success.
			++src;
			continue;
			}
		else
			// Not an interpolated expression.  Vanilla character.
			++src;

		// Save the character.
		if((opflags & OpEval) && dputc(c,&result) != 0)
			return drcset();
		}

	// Terminate the result and return.
	if((opflags & OpEval) && dclose(&result,sf_string) != 0)
		return drcset();
	return getsym();
	}

// List the names of all the functions.  If default n, make full list; otherwise, get a match string and make
// partial list of function names that contain it, ignoring case.  Render buffer and return status.
int showFunctions(Datum *rp,int n,Datum **argpp) {
	Buffer *flistp;			// Buffer to put function list into.
	CmdFunc *cfp;
	bool first;
	DStrFab rpt;
	Datum *mstrp = NULL;		// Match string.
	char *str,wkbuf[NWork];

	// If not default n, get match string.
	if(n != INT_MIN) {
		if(dnewtrk(&mstrp) != 0)
			return drcset();
		if(apropos(mstrp,text247,argpp) != Success)
				// "function"
			return rc.status;
		}

	// Get a buffer and open a string-fab object.
	if(sysbuf(text211,&flistp) != Success)
			// "FunctionList"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	// Build the function list.
	first = true;
	cfp = cftab;
	do {
		// Skip if a command.
		if(!(cfp->cf_aflags & CFFunc))
			continue;

		// Skip if an apropos and function name doesn't contain the search string.
		str = stpcpy(wkbuf,cfp->cf_name);
		if(mstrp != NULL && strcasestr(wkbuf,mstrp->d_str) == NULL)
			continue;

		// Begin next line.
		if(!first && dputc('\n',&rpt) != 0)
			return drcset();

		// Store function name, args, and description.
		if(cfp->cf_usage != NULL) {
			*str++ = ' ';
			strcpy(str,cfp->cf_usage);
			}
		if(strlen(wkbuf) > 22) {
			if(dputs(wkbuf,&rpt) != 0 || dputc('\n',&rpt) != 0)
				return drcset();
			wkbuf[0] = '\0';
			}
		pad(wkbuf,26);
		if(dputs(wkbuf,&rpt) != 0 || dputs(cfp->cf_desc,&rpt) != 0)
			return drcset();
		first = false;
		} while((++cfp)->cf_name != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(!disnull(rpt.sf_datp) && bappend(flistp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display the list.
	return render(rp,n < 0 ? -2 : n,flistp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}
