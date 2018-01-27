// (c) Copyright 2018 Richard W. Marinelli
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
	Array *aryp;
	ArraySize i;
	} ArrayState;

// System and session information table.
typedef struct {
	char *keyword;
	char *value;
	enum cfid id;
	} InfoTab;

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
			(void) rcset(Failure,RCNoFormat,text195);
				// "Endless recursion detected (array contains itself)"
		else if(dputs("[...]",destp) != 0)
			(void) librcset(Failure);
		}
	else {
		Datum *datp;
		Array *aryp = awp->aw_aryp;
		Datum **elpp = aryp->a_elpp;
		ArraySize n = aryp->a_used;
		bool first = true;
		char *realdlm = (flags & (CvtExpr | CvtVizStr | CvtVizStrQ)) ? "," : dlm;
		awp->aw_mark = true;
		if(flags & CvtExpr) {
			flags |= CvtKeepAll;
			if(dputc('[',destp) != 0)
				return librcset(Failure);
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
				return librcset(Failure);
			if(dtosf(destp,datp,dlm,flags) != Success)
				return rc.status;
			first = false;
			}
		if((flags & CvtExpr) && dputc(']',destp) != 0)
			(void) librcset(Failure);
		}

	return rc.status;
	}

// Add an array to wrapper list, clear all "marked" flags, and call atosf().
int atosfclr(DStrFab *destp,Datum *srcp,char *dlm,uint flags) {

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
		Datum *valp = srcp;
		if(flags & CvtTermAttr) {
			DStrFab val;

			if(dopentrk(&val) != 0 || escattrtosf(&val,srcp->d_str) != 0 || dclose(&val,sf_string) != 0)
				return librcset(Failure);
			valp = val.sf_datp;
			}
		if(flags & CvtExpr)
			(void) quote(destp,valp->d_str,true);
		else if(flags & (CvtVizStr | CvtVizStrQ)) {
			if(((flags & CvtVizStrQ) && dputc('\'',destp) != 0) || dvizs(valp->d_str,0,VBaseDef,destp) != 0 ||
			 ((flags & CvtVizStrQ) && dputc('\'',destp) != 0))
				(void) librcset(Failure);
			}
		else if(dputs(valp->d_str,destp) != 0)
			(void) librcset(Failure);
		}
	else
		switch(srcp->d_type) {
			case dat_int:
				if(dputf(destp,"%ld",srcp->u.d_int) != 0)
					(void) librcset(Failure);
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
					(void) librcset(Failure);
			}

	return rc.status;
	}

// Call atosfclr() if array so that "marked" flags in wrapper list are cleared first; otherwise, call dtosf().
int dtosfchk(DStrFab *destp,Datum *srcp,char *dlm,uint flags) {
	int (*func)(DStrFab *destp,Datum *srcp,char *dlm,uint flags) = (srcp->d_type == dat_blobRef) ? atosfclr : dtosf;
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
		return librcset(Failure);
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
		return librcset(Failure);
	return awrap(rp,aryp);
	}

// Copy string from src to destp (an active string-fab object), adding a double quote (") at beginning and end (if full is true)
// and escaping all control characters, backslashes, and characters that are escaped by parsesym().  Return status.
int quote(DStrFab *destp,char *src,bool full) {
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

		if((ischar ? dputc(c,destp) : dputs(str,destp)) != 0)
			return librcset(Failure);
		}
	if(full && dputc('"',destp) != 0)
		(void) librcset(Failure);

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
			// "%s tab size %d must be between 2 and %d","Hard","Soft"

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
void minit(Match *mtp) {
	GrpInfo *gip,*gipz;

	mtp->flags = 0;
	mtp->ssize = mtp->rsize = 0;
	gipz = (gip = mtp->groups) + MaxGroups;
	do {
		gip->matchp = NULL;
		} while(++gip < gipz);
	mtp->matchp = NULL;
	}

// Find pattern within source, using mtp Match record if mtp not NULL; otherwise, global "rematch" record.  Find rightmost match
// if "rightmost" is true.  Set rp to 0-origin match position or nil if no match.  Return status.
int sindex(Datum *rp,Datum *srcp,Datum *patp,Match *mtp,bool rightmost) {

	// No match if source or pattern is null.
	if(!disnull(srcp) && !disnull(patp)) {

		// If mtp is NULL, compile pattern and save in global "rematch" record.
		if(mtp == NULL) {
			if(newspat(patp->d_str,mtp = &rematch,NULL) != Success ||
			 ((mtp->flags & SOpt_Regexp) && mccompile(mtp) != Success))
				return rc.status;
			grpclear(mtp);
			}

		// Check pattern type.
		if(mtp->flags & SOpt_Regexp) {
			int offset;

			// Have regular expression.  Perform operation...
			if(recmp(srcp,rightmost ? -1 : 0,mtp,&offset) != Success)
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
			StrLoc *sfp = &mtp->groups->ml.str;
			int (*sncmp)(const char *s1,const char *s2,size_t n) = (mtp->flags & SOpt_Ignore) ?
			 strncasecmp : strncmp;

			// Have plain text pattern.  Scan through the source string.
			mtp->grpct = 0;
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
					return savematch(mtp);
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
	else if((rcode = dopenwith(&dest,rp,SFClear)) == 0) {
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
				return librcset(Failure);
			str = s + sstrlen;

			// Copy substitution string.
			if(dputmem((void *) rstr,rstrlen,&dest) != 0)
				return librcset(Failure);

			// Bail out unless n > 1.
			if(n <= 1)
				break;
			}

		// Copy remainder, if any.
		if((srclen = strlen(str)) > 0 && dputmem((void *) str,srclen,&dest) != 0)
			return librcset(Failure);
		rcode = dclose(&dest,sf_string);
		}

	return rcode == 0 ? rc.status : librcset(Failure);
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
	if(newspat(spat,&match,&flags) != Success)
		return rc.status;
	if(mccompile(&match) != Success || newrpat(rpat,&match) != Success)
		goto SFree;
	if(rmccompile(&match) != Success)
		goto RFree;

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
				(void) rcset(Failure,RCNoFormat,text91);
					// "Repeating match at same position detected"
				break;
				}

			// Open string-fab object.
			if(dopenwith(&dest,rp,SFClear) != 0) {
				(void) librcset(Failure);
				break;
				}

			// Copy any source text that is before the match location.
			if(offset > 0 && dputmem((void *) sp->d_str,offset,&dest) != 0) {
				(void) librcset(Failure);
				break;
				}

			// Copy replacement pattern to dest.
			if(match.flags & RRegical)
				for(rmcp = match.rmcpat; rmcp->mc_type != MCE_Nil; ++rmcp) {
					if(dputs(rmcp->mc_type == MCE_LitString ? rmcp->u.rstr :
					 rmcp->mc_type == MCE_Match ? match.matchp->d_str :
					 fixnull(match.groups[rmcp->u.grpnum].matchp->d_str),&dest) != 0) {
						(void) librcset(Failure);
						goto RFree;
						}
					}
			else if(dputs(match.rpat,&dest) != 0) {
				(void) librcset(Failure);
				break;
				}

			// Copy remaining source text to dest if any, and close it.
			if(((len = strlen(sp->d_str + offset + sfp->len)) > 0 &&
			 dputmem((void *) (sp->d_str + offset + sfp->len),len,&dest) != 0) || dclose(&dest,sf_string) != 0) {
				(void) librcset(Failure);
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
RFree:
	// Finis.  Free pattern space and return.
	freerpat(&match);
SFree:
	freespat(&match);
	return rc.status;
	}

// Expand character ranges and escaped characters (if any) in a string.  Return status.
int strexpand(DStrFab *sfp,char *estr) {
	short c1,c2;
	char *str;

	if(dopentrk(sfp) != 0)
		return librcset(Failure);
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
						return librcset(Failure);
				++str;
				break;
			case '\\':
				if(str[1] != '\0')
					c1 = *++str;
				// Fall through.
			default:
LitChar:
				if(dputc(c1,sfp) != 0)
					return librcset(Failure);
			}
		} while(*++str != '\0');

	return dclose(sfp,sf_string) != 0 ? librcset(Failure) : rc.status;
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
			short c = xtop->d_str[lento - 1];
			int n = lenfrom - lento;

			if(dopenwith(&sf,xtop,SFAppend) != 0)
				return librcset(Failure);
			do {
				if(dputc(c,&sf) != 0)
					return librcset(Failure);
				} while(--n > 0);
			if(dclose(&sf,sf_string) != 0)
				return librcset(Failure);
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
	if(dopenwith(&result,rp,SFClear) != 0)
		return librcset(Failure);
	str = srcp->d_str;
	lento = strlen(xtop->d_str);
	while(*str != '\0') {

		// Scan lookup table for a match.
		xf = xfromp->d_str;
		do {
			if(*str == *xf) {
				if(lento > 0 && dputc(xtop->d_str[xf - xfromp->d_str],&result) != 0)
					return librcset(Failure);
				goto Next;
				}
			} while(*++xf != '\0');

		// No match, copy the source char untranslated.
		if(dputc(*str,&result) != 0)
			return librcset(Failure);
Next:
		++str;
		}

	// Terminate and return the result.
	return dclose(&result,sf_string) != 0 ? librcset(Failure) : rc.status;
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

	if(dnewtrk(&datp) != 0 || ((opflags & OpEval) && dopenwith(&sf,rp,SFClear) != 0))
		return librcset(Failure);

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
				return librcset(Failure);
			if(dtosfchk(&sf,datp,dlm,flags) != Success)
				return rc.status;
			firstWrite = false;
			}
Onward:
		aflags = CFBool1 | CFArray1 | CFNIS1;
		}

	// Return result.
	if((opflags & OpEval) && dclose(&sf,sf_string) != 0)
		(void) librcset(Failure);

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
			return librcset(Failure);
		if(findvar(str1 = last->p_tok.d_str,&vd,OpDelete) != Success)
			return rc.status;
		if((vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_RdOnly)) ||
		 (vd.vd_type == VTyp_NVar && vd.i.vd_argnum == 0))
			return rcset(Failure,RCTermAttr,text164,str1);
				// "Cannot modify read-only variable '~b%s~0'"
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
		return librcset(Failure);
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
			return librcset(Failure);
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

			if(dopenwith(&sf,rp,SFClear) != 0 ||
			 dputs(str1,&sf) != 0)			// Copy initial portion of new var value to work buffer.
				return librcset(Failure);

			// Append a delimiter if oldvarvalp is not null, and value (if strPush) or var (if strUnshift).
			if((!disnull(oldvarvalp) && dputs(delimp->d_str,&sf) != 0) || dputs(str2,&sf) != 0 ||
			 dclose(&sf,sf_string) != 0)
				return librcset(Failure);
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
		(void) librcset(Failure);
	return rc.status;
	}

// Insert, overwrite, replace, or write the text in srcp to a buffer n times.  This routine's job is to make the given buffer
// the current buffer so that iorstr() can insert the text, then restore the current screen to its original form.  srcp, n, and
// style are passed to iorstr().  If bufp is NULL, the current buffer is used.  Return status.
static int iortext(Datum *srcp,int n,ushort style,Buffer *bufp) {
	EScreen *oscrp = NULL;
	EWindow *owinp = NULL;
	Buffer *obufp = NULL;

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
				supd_wflags(NULL,WFMode);
				}
			}
		}

	// Target buffer is current.  Now insert, overwrite, or replace the text in srcp n times.
	if(iorstr(srcp,n,style,false) == Success) {

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
			supd_wflags(NULL,WFMode);
			}
		}

	return rc.status;
	}

// Concatenate command-line arguments into rp and insert, overwrite, replace, or write the resulting text to a buffer n times,
// given text insertion style and buffer pointer.  If bufp is NULL, use current buffer.  If n == 0, do one repetition and don't
// move point.  If n < 0, do one repetition and process all newline characters literally (don't create a new line).  Return
// status.
int chgtext(Datum *rp,int n,ushort style,Buffer *bufp) {
	Datum *dtextp;
	DStrFab text;
	uint aflags = Arg_First | CFBool1 | CFArray1 | CFNIS1;

	if(n == INT_MIN)
		n = 1;

	if(dnewtrk(&dtextp) != 0)
		return librcset(Failure);

	// Evaluate all the arguments and save in string-fab object (so that the text can be inserted more than once,
	// if requested).
	if(dopenwith(&text,rp,SFClear) != 0)
		return librcset(Failure);

	for(;;) {
		if(!(aflags & Arg_First) && !havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(dtextp,aflags) != Success)
			return rc.status;
		aflags = CFBool1 | CFArray1 | CFNIS1;
		if(disnn(dtextp))
			continue;				// Ignore null and nil values.
		if(dtextp->d_type == dat_blobRef || (dtextp->d_type & DBoolMask)) {
			if(dtosfchk(&text,dtextp,NULL,0) != Success)
				return rc.status;
			}
		else if(dputd(dtextp,&text) != 0)		// Add text chunk to string-fab object.
			return librcset(Failure);
		}
	if(dclose(&text,sf_string) != 0)
		return librcset(Failure);

	// We have all the text in rp.  Now insert, overwrite, or replace it n times.
	return iortext(rp,n,style,bufp);
	}

// Process stat? function.  Return status.
static int ftest(Datum *rp,int n,Datum *filep,Datum *tcodep) {

	if(disnull(tcodep))
		(void) rcset(Failure,0,text187,text335);
			// "%s cannot be null","File test code(s)"
	else {
		struct stat s;
		bool result;
		char *str,tests[] = "defLlrswx";

		// Validate test code(s).
		for(str = tcodep->d_str; *str != '\0'; ++str) {
			if(strchr(tests,*str) == NULL)
				return rcset(Failure,0,text362,*str);
					// "Unknown file test code '%c'"
			}

		// Get file status.
		if(lstat(filep->d_str,&s) != 0)
			result = false;
		else {
			int flag;
			bool match;
			result = (n != INT_MIN);

			// Loop through test codes.
			for(str = tcodep->d_str; *str != '\0'; ++str) {
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
						match = access(filep->d_str,*str == 'r' ? R_OK :
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
		dsetbool(result,rp);
		}

	return rc.status;
	}

// Write "print" or "printf" text in rp to message line with flags set per n value.  Set rp to nil and return status.
static int printmsg(Datum *rp,int n) {
	ushort flags = MLHome | MLFlush;

	// Write the message out: force if n != 0, enable terminal attributes if n >= 0.
	if(n != INT_MIN) {
		if(n != 0)
			flags |= MLForce;
		if(n >= 0)
			flags |= MLTermAttr;
		}
	(void) mlputs(flags,rp->d_str);
	dsetnil(rp);
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
					return librcset(Failure);
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

// Build string from "printf" format string (formatp) and argument(s).  If arg1p is not NULL, process binary format (%)
// expression using arg1p as the argument; otherwise, process printf, sprintf, or bprintf function.
// Return status.
int strfmt(Datum *rp,Datum *formatp,Datum *arg1p) {
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

	fmt = formatp->d_str;

	// Create string-fab object for result and work Datum object for fmtarg() calls.
	if(dopenwith(&result,rp,SFClear) != 0 || (arg1p == NULL && dnewtrk(&tp) != 0))
		return librcset(Failure);

	// Loop through format string.
	specCount = 0;
	while((c = *fmt++) != '\0') {
		if(c != '%') {
			if(dputc(c,&result) != 0)
				return librcset(Failure);
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
					return rcset(Failure,RCNoFormat,text320);
						// "More than one spec in '%' format string"
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
			return rcset(Failure,0,text321,(int) '%',c == '\0' ? "" : vizc(c,VBaseDef));
				// "Unknown format spec '%c%s'"
		}

		// Concatenate the pieces, which are padding, prefix, more padding, the string, and trailing padding.
		prefLen = (prefix == NULL) ? 0 : strlen(prefix); // Length of prefix string.
		padding = width - (prefLen + sLen);		// # of chars to pad.

		// If 0 padding, store prefix (if any).
		if((flags & FMT0pad) && prefix != NULL) {
			if(dputs(prefix,&result) != 0)
				return librcset(Failure);
			prefix = NULL;
			}

		// If right-justified and chars to pad, store prefix string.
		if(padding > 0) {
			if(!(flags & FMTleft)) {
				short c1 = (flags & FMT0pad) ? '0' : ' ';
				while(--padding >= 0)
					if(dputc(c1,&result) != 0)
						return librcset(Failure);
				}
			}

		// Store prefix (if any).
		if(prefix != NULL && dputs(prefix,&result) != 0)
			return librcset(Failure);

		// Store (fixed-length) string.
		if(dputmem((void *) str,sLen,&result) != 0)
			return librcset(Failure);

 		// Store right padding.
		if(flags & FMTleft) {
			while(--padding >= 0)
				if(dputc(' ',&result) != 0)
					return librcset(Failure);
			}
		}

	// End of format string.  Check for errors.
	if(specCount == 0 && arg1p != NULL)
		return rcset(Failure,RCNoFormat,text281);
			// "Missing spec in '%' format string"
	if(as.aryp != NULL && as.i < as.aryp->a_used)
		return rcset(Failure,RCNoFormat,text204);
			// "Too many arguments for 'printf' or 'sprintf' function"
	if(dclose(&result,sf_string) != 0)
		return librcset(Failure);

	return rc.status;
	}

// Get a kill specified by n (of unlimited size) and save in rp.  May be a null string.  Return status.
static int getkill(Datum *rp,int n) {
	RingEntry *kp;

	// Which kill?
	if((kp = rget(&kring,n)) != NULL && datcpy(rp,&kp->re_data) != 0)
		(void) librcset(Failure);
	return rc.status;
	}

// Build array for getInfo function and return it in rp.  Return status.
static int getary(Datum *rp,int n,InfoTab *itp) {
	Array *aryp0,*aryp1;
	Datum *datp,**elpp;
	EScreen *scrp;

	if((aryp0 = anew(0,NULL)) == NULL)
		return librcset(Failure);
	switch(itp->id) {
		case cf_showBuffers:;		// [buf-name,...]
			Buffer *bufp;
			ushort exclude = (n == INT_MIN) ? BFHidden : n <= 0 ? BFMacro : 0;

			// Cycle through buffers and make list in aryp0.
			bufp = bheadp;
			do {
				if(!(bufp->b_flags & exclude) &&
				 ((datp = aget(aryp0,aryp0->a_used,true)) == NULL || dsetstr(bufp->b_bname,datp) != 0))
					goto Fail;
				} while((bufp = bufp->b_nextp) != NULL);
			break;
		case cf_showHooks:;		// [[hook-name,macro-name],...]
			HookRec *hrp = hooktab;

			// Cycle through hook table and make list.  For each hook, create array in aryp1 and push onto aryp0.
			do {
				if((aryp1 = anew(2,NULL)) == NULL)
					goto Fail;
				if(dsetstr(hrp->h_name,aryp1->a_elpp[0]) != 0 ||
				 (hrp->h_bufp != NULL && dsetstr(hrp->h_bufp->b_bname + 1,aryp1->a_elpp[1]) != 0) ||
				 (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					goto Fail;
				if(awrap(datp,aryp1) != Success)
					return rc.status;
				} while((++hrp)->h_name != NULL);
			break;
		case cf_showModes:;		// [[mode-name,global?,active?,shown?],...]
			ModeSpec *msp = modeinfo;

			// Cycle through mode table and make list.  For each mode, create array in aryp1 and push onto aryp0.
			do {
				if((aryp1 = anew(4,NULL)) == NULL)
					goto Fail;
				elpp = aryp1->a_elpp;
				if(dsetstr(msp->mlname,*elpp++) != 0 ||
				 (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					goto Fail;
				dsetbool(msp->mask & MdGlobal,*elpp++);
				dsetbool((msp->mask & MdGlobal) ? (modetab[MdIdxGlobal].flags & msp->mask) :
				 (curbp->b_modes & msp->mask),*elpp++);
				dsetbool(modetab[MdIdxShow].flags & msp->mask,*elpp);
				if(awrap(datp,aryp1) != Success)
					return rc.status;
				} while((++msp)->mlname != NULL);
			break;
		case cf_showScreens:;		// [[screen-num,wind-count,work-dir],...]

			// Cycle through screens and make list.  For each screen, create array in aryp1 and push onto aryp0.
			scrp = sheadp;
			do {
				if((aryp1 = anew(3,NULL)) == NULL)
					goto Fail;
				elpp = aryp1->a_elpp;
				dsetint((long) scrp->s_num,*elpp++);
				dsetint((long) wincount(scrp,NULL),*elpp++);
				if(dsetstr(scrp->s_wkdir,*elpp) != 0 || (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					goto Fail;
				if(awrap(datp,aryp1) != Success)
					return rc.status;
				} while((scrp = scrp->s_nextp) != NULL);
			break;
		default:;			// [[windNum,bufName],...] or [[screenNum,windNum,bufName],...]
			EWindow *winp;
			long wnum;

			// Cycle through screens and make list.  For each window, create array in aryp1 and push onto aryp0.
			scrp = sheadp;
			do {
				if(scrp->s_num == cursp->s_num || n != INT_MIN) {

					// In all windows...
					wnum = 0;
					winp = scrp->s_wheadp;
					do {
						++wnum;
						if((aryp1 = anew(n == INT_MIN ? 2 : 3,NULL)) == NULL)
							goto Fail;
						elpp = aryp1->a_elpp;
						if(n != INT_MIN)
							dsetint((long) scrp->s_num,*elpp++);
						dsetint(wnum,*elpp++);
						if(dsetstr(winp->w_bufp->b_bname,*elpp) != 0 ||
						 (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
							goto Fail;
						if(awrap(datp,aryp1) != Success)
							return rc.status;
						} while((winp = winp->w_nextp) != NULL);
					}
				} while((scrp = scrp->s_nextp) != NULL);
		}

	// Return results.
	return awrap(rp,aryp0);
Fail:
	return librcset(Failure);
	}

// Get keyword argument(s) for various functions, compare them to values in a table, and process entries that match.  If itp is
// not NULL, get single argument (for getInfo function); otherwise, multiple.  If itp is NULL, use modeinfo table if type >= 0
// (xxxMode? functions) and allow array arguments; otherwise, use bflaginfo table (bufAttr? function).  If type < 0 or
// type == 2 (MdIdxBuffer), use bufp argument.  Return status.
//
// bufAttr?	name,flag,...		Keywords (bflaginfo)
// xxxMode?	mode,...		Keywords (modeinfo)
// getInfo	kw			Keyword
static int tabcheck(Datum *rp,int n,InfoTab *itp,int type,Buffer *bufp,char *myname) {
	uint aflags = Arg_First | CFNotNull1;
	uint flags;
	int status;
	ArraySize elct;
	ModeSpec *msp;
	char *keyword;
	BufFlagSpec *bfsp;
	Datum *datp,*argp;
	Datum **elpp = NULL;
	bool matchFound;
	bool result = !(n == INT_MIN);

	// Get ready.
	if(itp == NULL) {
		flags = (bufp == NULL) ? modetab[type].flags : type < 0 ? bufp->b_flags : bufp->b_modes;
		if(type >= 0)
			aflags |= CFArray1 | CFMay;
		}

	// Get keyword(s).
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	for(;;) {
		if((status = nextarg(&argp,&aflags,datp,&elpp,&elct)) == NotFound)
			break;					// No arguments left.
		if(status != Success)
			return rc.status;
		keyword = argp->d_str;

		// Search table for a match.
		matchFound = false;
		if(itp == NULL) {
			if(type >= 0) {
				// xxxMode? function.  Scan mode table.
				if(modecmp(keyword,type,&msp,false) != Success)
					return rc.status;

				// Match found?
				if(msp != NULL) {
					matchFound = true;

					// Yes, mode flag set?
					if(flags & (msp->mask & ~MdGlobal)) {

						// Yes, done (success) if "any" check.
						if(n == INT_MIN)
							result = true;
						}

					// Flag not set.  Failure if "all" check.
					else if(n != INT_MIN)
						result = false;
					}
				}
			else {
				// bufAttr? function.
				bfsp = bflaginfo;
				do {
					if(strcasecmp(keyword,bfsp->name) == 0) {
						matchFound = true;
						if(flags & bfsp->mask) {
							if(n == INT_MIN)
								result = true;
							}
						else if(n != INT_MIN)
							result = false;
						break;
						}
					} while((++bfsp)->name != NULL);
				}
			}
		else
			// getInfo function.
			do {
				if(strcasecmp(keyword,itp->keyword) == 0) {
					if(itp->value != NULL)
						return dsetstr(itp->value,rp) != 0 ? librcset(Failure) : rc.status;

					// Not a string return value.  Build array and return it.
					return getary(rp,n,itp);
					}
				} while((++itp)->keyword != NULL);

		// Error if keyword match not found.
		if(!matchFound)
			return rcset(Failure,0,text344,myname,keyword);
					// "Unknown %s argument '%s'"
		}

	// All arguments processed.  Return result.
	dsetbool(result,rp);
	return rc.status;
	}

// Get informational item per keyword argument.  Return status.
static int getInfo(Datum *rp,int n,Datum **argpp,char *myname) {
	static InfoTab itab[] = {
		{"buffers",NULL,cf_showBuffers},	// [buf-name,...]
		{"editor",Myself,-1},
		{"hooks",NULL,cf_showHooks},		// [[hook-name,macro-name],...]
		{"language",Language,-1},
		{"modes",NULL,cf_showModes},		// [[mode-name,global?,active?,shown?],...]
		{"os",OSName,-1},
		{"screens",NULL,cf_showScreens},	// [[screen-num,wind-count,work-dir],...]
		{"version",Version,-1},
		{"windows",NULL,-1},			// [[windNum,bufName],...] or [[screenNum,windNum,bufName],...]
		{NULL,NULL,-1}};

	return tabcheck(rp,n,itab,-1,NULL,myname);
	}

// Clone an array.  Return status.
int aryclone(Datum *destp,Datum *srcp,int depth) {
	Array *aryp;

	if(maxarydepth > 0 && depth > maxarydepth)
		return rcset(Failure,0,text319,Literal23,maxarydepth);
			// "Maximum %s recursion depth (%d) exceeded","array"
	if((aryp = aclone(awptr(srcp)->aw_aryp)) == NULL)
		return librcset(Failure);
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

// Resolve buffer name argument.
static Buffer *getbufarg(Datum **argpp) {
	Buffer *bufp;

	if((bufp = bsrch(argpp[0]->d_str,NULL)) == NULL)
		(void) rcset(Failure,0,text118,argpp[0]->d_str);
			// "No such buffer '%s'"
	return bufp;
	}

// Convert any value into a string form which will resolve to the original value if subsequently evaluated as an expression
// (unless "force" is set (n > 0) and value is an array that includes itself).  Return status.
#if !(MMDebug & Debug_MacArg)
static
#endif
int dquote(Datum *rp,Datum *datp,uint flags) {
	DStrFab sf;

	if(dopenwith(&sf,rp,SFClear) != 0)
		goto Fail;
	if(dtosfchk(&sf,datp,NULL,flags) == Success && dclose(&sf,sf_string) != 0)
Fail:
		(void) librcset(Failure);

	return rc.status;
	}

// Set wrap column to n.  Return status.
int setwrap(int n,bool msg) {

	if(n < 0)
		(void) rcset(Failure,0,text39,text59,n,0);
			// "%s (%d) must be %d or greater","Wrap column"
	else {
		wrapcol0 = wrapcol;
		wrapcol = n;
		if(msg)
			(void) rcset(Success,0,"%s%s%d",text59,text278,n);
					// "Wrap column"," set to "
		}
	return rc.status;
	}

// Convert a string to title case.  Return status.
static int tcstr(Datum *destp,Datum *srcp) {
	char *src,*dest;
	short c;
	bool inword = false;

	if(dsalloc(destp,strlen(src = srcp->d_str) + 1) != 0)
		return librcset(Failure);
	dest = destp->d_str;
	while((c = *src++) != '\0') {
		if(wordlist[c]) {
			*dest++ = inword ? lowcase[c] : upcase[c];
			inword = true;
			}
		else {
			*dest++ = c;
			inword = false;
			}
		}
	*dest = '\0';
	return rc.status;
	}

// Delete entri(es) from search or replace ring and update or delete current pattern in Match record, if applicable.
static int delpat(Ring *ringp,int n) {

	if(rdelete(ringp,n) == Success) {
		Match *mtp = &srch.m;
		if(ringp == &sring)
			(void) newspat(ringp->r_size == 0 ? "" : ringp->r_entryp->re_data.d_str,mtp,NULL);
		else
			(void) newrpat(ringp->r_size == 0 ? "" : ringp->r_entryp->re_data.d_str,mtp);
		}

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
int execCF(Datum *rp,int n,CmdFunc *cfp,int minArgs,int maxArgs) {
	int i;
	char *str;
	long lval;
	Datum *argp[CFMaxArgs + 1];	// Argument values plus NULL terminator.
	int argct = 0;			// Number of arguments loaded into argp.
	int fnum = cfp - cftab;

	// If script mode and CFNoLoad is not set, retrieve the minimum number of arguments specified in cftab, up to the
	// maximum, and save in argp.  If CFSpecArgs flag is set or the maximum is negative, use the minimum for the maximum;
	// otherwise, if CFShrtLoad flag is set, decrease the maximum by one.
	argp[0] = NULL;
	if((opflags & (OpScript | OpNoLoad)) == OpScript && !(cfp->cf_aflags & CFNoLoad) &&
	 (!(opflags & OpParens) || !havesym(s_rparen,false))) {
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
					return librcset(Failure);
				if(funcarg(*argpp++,(argct == 0 ? Arg_First : 0) | (((cfp->cf_vflags & (CFNotNull1 << argct)) |
				 (cfp->cf_vflags & (CFNil1 << argct)) | (cfp->cf_vflags & (CFBool1 << argct)) |
				 (cfp->cf_vflags & (CFInt1 << argct)) | (cfp->cf_vflags & (CFArray1 << argct)) |
				 (cfp->cf_vflags & (CFNIS1 << argct))) >> argct) | (cfp->cf_vflags & CFMay)) != Success)
					return rc.status;
				} while(++argct < minArgs || (argct < maxArgs && havesym(s_comma,false)));
			*argpp = NULL;
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
				{Buffer *bufp;

				if(!(opflags & OpScript))
					bufp = NULL;
				else if((bufp = getbufarg(argp)) == NULL || !needsym(s_comma,true))
					return rc.status;
				(void) alterMode(rp,n,MdIdxBuffer,bufp);
				}
				break;
			case cf_alterGlobalMode:
				i = MdIdxGlobal;
				goto AlterMode;
			case cf_alterShowMode:
				i = MdIdxShow;
AlterMode:
				(void) alterMode(rp,n,i,NULL);
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
			case cf_backspace:;
				// Delete char or soft tab backward.  Return status.
				Dot *dotp = &curwp->w_face.wf_dot;
				(void) (stabsize > 0 && dotp->off > 0 ? deltab(n == INT_MIN ? -1 : -n,true) :
				 ldelete(n == INT_MIN ? -1L : (long) -n,0));
				break;
			case cf_basename:
				if(dsetstr(fbasename(argp[0]->d_str,n == INT_MIN || n > 0),rp) != 0)
					(void) librcset(Failure);
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
				// Go to the beginning of the buffer.
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
				if(stoek(argp[0]->d_str,&ek) != Success) {
					if(n == INT_MIN)
						break;
					rcclear(0);
					goto NilRtn;
					}
				if(n != INT_MIN) {
					char keybuf[16];

					if(dsetstr(ektos(ek,keybuf,false),rp) != 0)
						(void) librcset(Failure);
					}
				else {
					str = fixnull(getkname(getbind(ek)));
					if(*str == '\0') {
						if(ek >= 0x20 && ek < 0xFF)
							str = text383;
								// "(self insert)"
						else {
NilRtn:
							dsetnil(rp);
							break;
							}
						}
					if(dsetstr(*str == SBMacro ? str + 1 : str,rp) != 0)
						(void) librcset(Failure);
					}
				}
				break;
			case cf_bprintf:
				{Buffer *bufp;

				// Get buffer pointer.
				if((bufp = bsrch(argp[0]->d_str,NULL)) == NULL)
					return rcset(Failure,0,text118,argp[0]->d_str);
						// "No such buffer '%s'"

				// Create formatted string in rp and insert into buffer.
				if(strfmt(rp,argp[1],NULL) == Success)
					(void) iortext(rp,n == INT_MIN ? 1 : n,Txt_Insert,bufp);
				}
				break;
			case cf_bufBoundQ:
				if(n != INT_MIN)
					n = n > 0 ? 1 : n < 0 ? -1 : 0;
				i = (curwp->w_face.wf_dot.lnp == curbp->b_hdrlnp) ? 1 :
				 (curwp->w_face.wf_dot.lnp == curbp->b_hdrlnp->l_nextp && curwp->w_face.wf_dot.off == 0) ?
				 -1 : 0;
				dsetbool((n == INT_MIN && i != 0) || i == n,rp);
				break;
			case cf_bufAttrQ:
				i = -1;
				goto BufAttr;
			case cf_bufModeQ:
				i = 3;
BufAttr:
				{Buffer *bufp = getbufarg(argp);

				if(bufp != NULL && needsym(s_comma,true))
					(void) tabcheck(rp,n,NULL,i,bufp,cfp->cf_name);
				}
				break;
			case cf_bufSize:;
				long bct,lct;
				bct = buflength(curbp,&lct);
				dsetint(n == INT_MIN ? lct : bct,rp);
				break;
			case cf_bufWind:
				{Buffer *bufp;
				EWindow *winp;

				if((bufp = bsrch(argp[0]->d_str,NULL)) != NULL && (winp = whasbuf(bufp,n != INT_MIN)) != NULL)
					dsetint((long) getwnum(winp),rp);
				else
					dsetnil(rp);
				}
				break;
			case cf_chr:
				dsetchr(argp[0]->u.d_int,rp);
				break;
			case cf_clearMsg:
				(void) mlerase(n > 0 ? MLForce : 0);
				break;
			case cf_clone:
				(void) aryclone(rp,argp[0],0);
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

				if(getregion(&region,false,NULL) != Success || copyreg(&region) != Success)
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
			case cf_cycleKillRing:
			case cf_cycleReplaceRing:
			case cf_cycleSearchRing:
				// Cycle the kill ring forward or backward.
				(void) rcycle(fnum == cf_cycleKillRing ? &kring : fnum == cf_cycleSearchRing ? &sring :
				 &rring,n,true);
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
				(void) deltab(n == INT_MIN ? -1 : -n,false);
				break;
			case cf_deleteFencedRegion:
				// Delete text without saving in kill ring.
				i = 0;
				goto KDCFenced;
			case cf_deleteForwChar:
				// Delete char forward.  Return status.
				(void) ldelete(n == INT_MIN ? 1L : (long) n,0);
				break;
			case cf_deleteForwTab:
				// Delete tab forward.  Return status.
				(void) deltab(n,false);
				break;
			case cf_deleteKill:
				(void) rdelete(&kring,n);
				break;
			case cf_deleteLine:
				// Delete line(s) without saving text in kill ring.
				i = 0;
				goto KDCLine;
			case cf_deleteRegion:
				// Delete region without saving text in kill ring.
				i = false;
				goto DKReg;
			case cf_deleteReplacePat:
				(void) delpat(&rring,n);
				break;
			case cf_deleteSearchPat:
				(void) delpat(&sring,n);
				break;
			case cf_deleteToBreak:
				// Delete text without saving in kill ring.
				i = 0;
				goto KDCText;
			case cf_deleteWhite:
				// Delete white space surrounding or immediately before point on current line.
				(void) delwhite(n,true);
				break;
			case cf_deleteWord:
				// Delete word(s) without saving text in kill ring.
				(void) (n == INT_MIN ? kdcfword(1,0) : n < 0 ? kdcbword(-n,0) : kdcfword(n,0));
				break;
			case cf_dirname:
				if(dsetstr(fdirname(argp[0]->d_str,n),rp) != 0)
					(void) librcset(Failure);
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
					(void) librcset(Failure);
				break;
			case cf_failure:
				// Return false: with terminal attributes enabled if n argument.
				i = false;
				goto Notice;
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
			case cf_getInfo:
				(void) getInfo(rp,n,argp,cfp->cf_name);
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
					dsetint(ektoc(ek,true),rp);
				else if(dsetstr(ektos(ek,keybuf,false),rp) != 0)
					(void) librcset(Failure);
				}
				break;
			case cf_globalModeQ:
				i = MdIdxGlobal;
				goto CheckMode;
			case cf_gotoFence:
				// Move the point to a matching fence.
				(void) otherfence(NULL);
				break;
			case cf_growWind:
				// Grow (enlarge) the current window.  If n is negative, take lines from the upper window;
				// otherwise, lower.
				i = 1;
				goto GSWind;
			case cf_index:
				(void) sindex(rp,argp[0],argp[1],NULL,n > 0);
				break;
			case cf_insert:
				(void) chgtext(rp,n,Txt_Insert,NULL);
				break;
			case cf_insertSpace:
				// Insert space(s) forward into text without moving point.
				(void) insnlspace(n,EditSpace | EditHoldPt);
				break;
			case cf_interactiveQ:
				dsetbool(!(opflags & OpStartup) && !(last->p_flags & OpScript),rp);
				break;
			case cf_join:
				if(needsym(s_comma,true))
					(void) catargs(rp,1,argp[0],n == INT_MIN || n > 0 ? CvtKeepAll :
					 n == 0 ? CvtKeepNull : 0);
				break;
			case cf_keyPendingQ:
				if(typahead(&i) == Success)
					dsetbool(i > 0,rp);
				break;
			case cf_kill:
				(void) getkill(rp,(int) argp[0]->u.d_int);
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
				if(cursp->s_lastbufp != NULL) {
					Buffer *oldbufp = curbp;
					if(render(rp,1,cursp->s_lastbufp,0) == Success && n != INT_MIN && n < 0) {
						char bname[NBufName + 1];

						strcpy(bname,oldbufp->b_bname);
					 	if(bdelete(oldbufp,0) == Success)
							(void) rcset(Success,0,text372,bname);
									// "Buffer '%s' deleted"
						}
					}
				break;
			case cf_length:
				dsetint(argp[0]->d_type == dat_blobRef ? (long) awptr(argp[0])->aw_aryp->a_used :
				 (long) strlen(argp[0]->d_str),rp);
				break;
			case cf_lowerCaseLine:
				i = CaseLine | CaseLower;
				goto CvtCase;
			case cf_lowerCaseRegion:
				i = CaseRegion | CaseLower;
				goto CvtCase;
			case cf_lowerCaseString:
				i = -1;
				goto CvtStr;
			case cf_lowerCaseWord:
				i = CaseWord | CaseLower;
				goto CvtCase;
			case cf_match:
				if(argp[0]->u.d_int < 0 || argp[0]->u.d_int >= MaxGroups)
					return rcset(Failure,0,text5,argp[0]->u.d_int,MaxGroups - 1);
						// "Group number %ld must be between 0 and %d"
				if(dsetstr(fixnull((n == INT_MIN ? &rematch :
				 &srch.m)->groups[(int) argp[0]->u.d_int].matchp->d_str),rp) != 0)
					(void) librcset(Failure);
				break;
			case cf_moveWindDown:
				// Move the current window down by "n" lines and compute the new top line in the window.
				(void) moveWindUp(rp,n == INT_MIN ? -1 : -n,argp);
				break;
			case cf_newline:
				i = EditWrap;
				goto InsNLSPC;
			case cf_nextBuf:
				// Switch to the next buffer in the buffer list.
				i = 0;
				goto PNBuf;
			case cf_nextScreen:
				i = EScrWinRepeat | EScrWinForw;
				goto PNScreen;
			case cf_nilQ:
				dsetbool(argp[0]->d_type == dat_nil,rp);
				break;
			case cf_nullQ:
				dsetbool(disnull(argp[0]),rp);
				break;
			case cf_numericQ:
				dsetbool(asc_long(argp[0]->d_str,NULL,true),rp);
				break;
			case cf_openLine:
				if(insnlspace(n,EditHoldPt) == Success && n < 0 && n != INT_MIN) {

					// Move point to first empty line if possible.
					Dot *dotp = &curwp->w_face.wf_dot;
					if(dotp->lnp->l_used > 0 && dotp->lnp->l_nextp->l_used == 0)
						(void) forwch(1);
					}
				break;
			case cf_ord:
				dsetint((long) argp[0]->d_str[0],rp);
				break;
			case cf_overwrite:
				(void) chgtext(rp,n,Txt_Overwrite,NULL);
				break;
			case cf_pathname:
				(void) getpath(rp,n,argp[0]->d_str);
				break;
			case cf_pause:
				if((i = argp[0]->u.d_int) < 0)
					return rcset(Failure,0,text39,text119,(int) i,0);
						// "%s (%d) must be %d or greater","Pause duration"
				cpause(n == INT_MIN ? i * 100 : i);
				break;
			case cf_popBuf:
				i = true;
				goto PopIt;
			case cf_popFile:
				i = false;
PopIt:
				(void) dopop(rp,n,i);
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
					(void) librcset(Failure);
				else
					datxfer(rp,argp[0]);
				}
				break;
			case cf_prevBuf:
				// Switch to the previous buffer in the buffer list.
				i = BDefBack;
PNBuf:
				(void) pnbuffer(rp,n,i);
				break;
			case cf_prevScreen:
				i = EScrWinRepeat;
PNScreen:
				(void) gotoScreen(n,i);
				break;
			case cf_print:
				// Concatenate all arguments into rp and write result to message line.
				if(catargs(rp,1,NULL,0) == Success)
					(void) printmsg(rp,n);
				break;
			case cf_printf:
				if(strfmt(rp,argp[0],NULL) == Success)
					(void) printmsg(rp,n);
				break;
			case cf_queryReplace:
				// Search and replace with query.
				i = true;
				goto Repl;
			case cf_quickExit:
				// Quick exit from MightEMacs.  If any buffer has changed, do a save on that buffer first.
				if(savebufs(1,SVBQExit) == Success)
					(void) rcset(UserExit,RCForce,NULL);
				break;
			case cf_quote:
				// Convert any value into a string form which will resolve to the original value.
				(void) dquote(rp,argp[0],n > 0 ? CvtExpr | CvtForceArray : CvtExpr);
				break;
			case cf_rand:
				dsetint(xorshift64star(argp[0]->u.d_int),rp);
				break;
			case cf_readFile:
				// Read a file into a buffer.

				// Get the filename...
				if(gtfilename(rp,n == -1 ? text299 : text131,curbp->b_fname,0) != Success ||
							// "Pop file","Read file"
				 rp->d_type == dat_nil)
					break;

				// and return results.
				(void) rdfile(rp,n,rp->d_str,RWExist);
				break;
			case cf_reframeWind:
				// Reposition dot in current window per the standard redisplay subsystem.  Row defaults to zero
				// to center current line in window.
				curwp->w_rfrow = (n == INT_MIN) ? 0 : n;
				curwp->w_flags |= WFReframe;
				break;
			case cf_replace:
				// Search and replace.
				i = false;
Repl:
				(void) replstr(i ? rp : NULL,n,argp);
				break;
			case cf_replaceText:
				(void) chgtext(rp,n,Txt_Replace,NULL);
				break;
			case cf_restoreBuf:
				// Restore the saved buffer.
				if(savbufp == NULL)
					return rcset(Failure,0,text208,text83);
							// "Saved %s not found","buffer"
				if(bswitch(savbufp) == Success && dsetstr(curbp->b_bname,rp) != 0)
					(void) librcset(Failure);
				break;
			case cf_restoreScreen:;
				// Restore the saved screen.
				EScreen *scrp;

				// Find the screen.
				scrp = sheadp;
				do {
					if(scrp == savscrp) {
						dsetint((long) scrp->s_num,rp);
						return sswitch(scrp);
						}
					} while((scrp = scrp->s_nextp) != NULL);
				(void) rcset(Failure,0,text208,text380);
					// "Saved %s not found","screen"
				savscrp = NULL;
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
						dsetint((long) getwnum(curwp),rp);
						return rc.status;
						}
					} while((winp = winp->w_nextp) != NULL);
				(void) rcset(Failure,0,text208,text331);
					// "Saved %s not found","window"
				}
				break;
			case cf_saveBuf:
				// Save pointer to current buffer.
				savbufp = curbp;
				if(dsetstr(curbp->b_bname,rp) != 0)
					(void) librcset(Failure);
				break;
			case cf_saveFile:
				// Save the contents of the current buffer (or all buffers if n argument) to their associated
				// files.
				(void) savebufs(n,0);
				break;
			case cf_saveScreen:
				// Save pointer to current screen.
				savscrp = cursp;
				break;
			case cf_saveWind:
				// Save pointer to current window.
				savwinp = curwp;
				break;
			case cf_setWrapCol:
				// Set wrap column to N, or previous value if n argument.
				if(n != INT_MIN) {
					if(wrapcol0 < 0)
						return rcset(Failure,RCNoFormat,text298);
							// "No previous wrap column set"
					n = wrapcol0;
					}
				else {
					if(getnarg(rp,text59) != Success || rp->d_type == dat_nil)
							// "Wrap column"
						return rc.status;
					n = rp->u.d_int;
					}
				(void) setwrap(n,true);
				break;
			case cf_shQuote:
				if(tostr(argp[0]) == Success && dshquote(argp[0]->d_str,rp) != 0)
					(void) librcset(Failure);
				break;
			case cf_showModeQ:
				i = MdIdxShow;
CheckMode:
				(void) tabcheck(rp,n,NULL,i,NULL,cfp->cf_name);
				break;
			case cf_shrinkWind:
				// Shrink the current window.  If n is negative, give lines to the upper window; otherwise,
				// lower.
				i = -1;
GSWind:
				(void) gswind(rp,n,i);
				break;
			case cf_space:
				i = EditSpace | EditWrap;
InsNLSPC:
				(void) insnlspace(n,i);
				break;
			case cf_splitWind:
				{EWindow *winp;
				(void) wsplit(n,&winp);
				dsetint((long) getwnum(winp),rp);
				}
				break;
			case cf_sprintf:
				(void) strfmt(rp,argp[0],NULL);
				break;
			case cf_statQ:
				(void) ftest(rp,n,argp[0],argp[1]);
				break;
			case cf_strFit:
				if(argp[1]->u.d_int < 0)
					(void) rcset(Failure,0,text39,text290,(int) argp[1]->u.d_int,0);
							// "%s (%d) must be %d or greater","Length argument"
				else if(dsalloc(rp,argp[1]->u.d_int + 1) != 0)
					(void) librcset(Failure);
				else
					strfit(rp->d_str,argp[1]->u.d_int,argp[0]->d_str,0);
				break;
			case cf_strPop:
			case cf_strPush:
			case cf_strShift:
			case cf_strUnshift:
				(void) strfunc(rp,fnum,cfp->cf_name);
				break;
			case cf_strip:
				if(dsetstr(stripstr(argp[0]->d_str,n == INT_MIN ? 0 : n),rp) != 0)
					(void) librcset(Failure);
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
			case cf_subline:
				{long lval2 = argct < 2 ? LONG_MAX : argp[1]->u.d_int;
				lval = argp[0]->u.d_int;
				if(lval2 != 0 && (i = curwp->w_face.wf_dot.lnp->l_used) > 0) {

					// Determine line offset and length and validate them.  Offset is position relative to
					// dot.
					lval += curwp->w_face.wf_dot.off;
					if(lval >= 0 && lval < i && (lval2 >= 0 || (lval2 = i - lval + lval2) > 0)) {
						if(lval2 > i - lval)
							lval2 = i - lval;
						if(dsetsubstr(curwp->w_face.wf_dot.lnp->l_text + (int) lval,(size_t) lval2,rp)
						 != 0)
							return librcset(Failure);
						}
					else
						dsetnull(rp);
					}
				else
					dsetnull(rp);
				}
				break;
			case cf_substr:
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
							return librcset(Failure);
						}
					else
						dsetnull(rp);
					}
				else
					dsetnull(rp);
				}
				break;
			case cf_success:
				// Return true: not in brackets if n <= 0, with terminal attributes enabled if n >= 0.
				i = true;
Notice:
				(void) notice(rp,n,i);
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
			case cf_titleCaseString:
				(void) tcstr(rp,argp[0]);
				break;
			case cf_titleCaseWord:
				i = CaseWord | CaseTitle;
				goto CvtCase;
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

					if(dopenwith(&sf,rp,SFClear) != 0)
						goto DFail;
					if(dtosfchk(&sf,argp[0],NULL,n == INT_MIN ? 0 : n < 0 ? CvtKeepNil | CvtShowNil :
					 n == 0 ? CvtKeepAll | CvtForceArray | CvtVizStr :
					 CvtKeepAll | CvtForceArray | CvtVizStrQ) != Success)
						break;
					if(dclose(&sf,sf_string) != 0)
DFail:
						(void) librcset(Failure);
					}
				break;
			case cf_tr:
				(void) tr(rp,argp[0],argp[1],argp[2]);
				break;
			case cf_truncBuf:;
				// Truncate buffer.  Delete all text from current buffer position to end (default or n > 0) or
				// beginning (n <= 0) and save in undelete buffer.

				// No-op if currently at buffer boundary.
				long bytes;
				if(n == INT_MIN || n > 0) {
					if(curwp->w_face.wf_dot.lnp == curbp->b_hdrlnp)
						break;
					bytes = LONG_MAX;
					}
				else {
					if(curwp->w_face.wf_dot.lnp == curbp->b_hdrlnp->l_nextp &&
					 curwp->w_face.wf_dot.off == 0)
						break;
					bytes = LONG_MIN + 1;
					}

				// Delete maximum possible, ignoring any end-of-buffer error.
				dclear(&undelbuf.re_data);
				(void) ldelete(bytes,EditDel);
				break;
			case cf_typeQ:
				(void) dsetstr(dtype(argp[0],true),rp);
				break;
			case cf_undelete:
				// Insert text from the undelete buffer.
				(void) iorstr(NULL,n,Txt_Insert,false);
				break;
			case cf_updateScreen:
				(void) update(n);
				break;
			case cf_upperCaseLine:
				i = CaseLine | CaseUpper;
				goto CvtCase;
			case cf_upperCaseRegion:
				i = CaseRegion | CaseUpper;
				goto CvtCase;
			case cf_upperCaseString:
				i = 1;
CvtStr:;
				char *(*mk)(char *dest,char *src) = (i <= 0) ? mklower : mkupper;
				if(dsalloc(rp,strlen(argp[0]->d_str) + 1) != 0)
					return librcset(Failure);
				mk(rp->d_str,argp[0]->d_str);
				break;
			case cf_upperCaseWord:
				i = CaseWord | CaseUpper;
CvtCase:
				(void) cvtcase(n,i);
				break;
			case cf_viewFile:
				i = true;
FVFile:
				(void) getfile(rp,n,i);
				break;
			case cf_wordCharQ:
				dsetbool(wordlist[(int) argp[0]->d_str[0]],rp);
				break;
			case cf_writeFile:
				i = 'w';
AWFile:
				(void) awfile(rp,n,i == 'w' ? text144 : text218,i);
						// "Write file: ","Append file"
				break;
			case cf_xPathname:
				if(pathsearch(&str,argp[0]->d_str,false) != Success)
					break;
				if(str == NULL)
					dsetnil(rp);
				else if(dsetstr(str,rp) != 0)
					(void) librcset(Failure);
				break;
			case cf_yank:
				// Yank text from the kill ring.
				if(n == INT_MIN)
					n = 1;
				(void) iorstr(NULL,n,Txt_Insert,true);
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
	short termch,c;
	char *src0;
	DStrFab result;		// Put object.

	// Get ready.
	if((opflags & OpEval) && dopenwith(&result,rp,SFClear) != 0)
		return librcset(Failure);
	termch = *src;
	src0 = src++;

	// Loop until null or string terminator hit.
	while((c = *src) != termch) {
#if MMDebug & Debug_Token
		if(c == '\0')
			return rcset(Failure,0,"String terminator %c not found in '%s'",(int) termch,src0);
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
				short c2;
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
							if(c > 0xFF)
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
				return librcset(Failure);
			if(execestmt(datp,src + 2,TokC_ExprEnd,NULL,&src) != Success)
				return rc.status;

			// Append the result to dest.
			if((opflags & OpEval) && datp->d_type != dat_nil) {
				if(tostr(datp) != Success)
					return rc.status;
				if(dputd(datp,&result) != 0)
					return librcset(Failure);
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
			return librcset(Failure);
		}

	// Terminate the result and return.
	if((opflags & OpEval) && dclose(&result,sf_string) != 0)
		return librcset(Failure);
	return getsym();
	}
