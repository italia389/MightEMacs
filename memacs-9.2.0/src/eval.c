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
#include <sys/utsname.h>
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
			goto LibFail;
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
				goto LibFail;
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
				goto LibFail;
			if(dtosf(destp,datp,dlm,flags) != Success)
				return rc.status;
			first = false;
			}
		if((flags & CvtExpr) && dputc(']',destp) != 0)
			goto LibFail;
		}

	return rc.status;
LibFail:
	return librcset(Failure);
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
				goto LibFail;
			}
		else if(dputs(valp->d_str,destp) != 0)
			goto LibFail;
		}
	else
		switch(srcp->d_type) {
			case dat_int:
				if(dputf(destp,"%ld",srcp->u.d_int) != 0)
					goto LibFail;
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
					goto LibFail;
			}

	return rc.status;
LibFail:
	return librcset(Failure);
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

// Split a string into an array and save in rp, given delimiter and optional limit value.  Return status.
int ssplit(Datum *rp,int n,Datum **argpp) {
	Array *aryp;
	short delim;
	char *str;
	int limit = 0;

	// Get delimiter, string, and optional limit.
	if(!charval(*argpp))
		return rc.status;
	if((delim = (*argpp++)->u.d_int) == '\0')
		return rcset(Failure,0,text187,text409);
			// "%s cannot be null","Delimiter"
	str = (*argpp++)->d_str;
	if(*argpp != NULL)
		limit = (*argpp)->u.d_int;

	return (aryp = asplit(delim,str,limit)) == NULL ? librcset(Failure) : awrap(rp,aryp);
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

// Find pattern within source, using mtp Match record if mtp not NULL; otherwise, global "rematch" record.  If n <= 0, pattern
// is assumed to be a single character; otherwise, a string or regular expression.  If n >= 0, rightmost match is found.  Set rp
// to 0-origin match position, or nil if no match.  Return status.
int sindex(Datum *rp,int n,Datum *srcp,Datum *patp,Match *mtp) {
	bool rightmost = (n >= 0);

	if(n <= 0 && n != INT_MIN) {

		// No match if source or character is null.
		if(charval(patp) && !disnull(srcp)) {
			int i;
			char *str;

			if((i = patp->u.d_int) != 0) {
				if(n < 0) {
					if((str = strchr(srcp->d_str,i)) != NULL)
						goto Found;
					}
				else {
					char *str0 = srcp->d_str;
					str = strchr(str0,'\0');
					do {
						if(*--str == i) {
Found:
							dsetint(str - srcp->d_str,rp);
							return rc.status;
							}
						} while(str != str0);
					}
				}
			}
		}

	// No match if source or pattern is null.
	else if(strval(patp) && !disnull(srcp) && !disnull(patp)) {

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
			if(dopenwith(&dest,rp,SFClear) != 0)
				goto LibFail;

			// Copy any source text that is before the match location.
			if(offset > 0 && dputmem((void *) sp->d_str,offset,&dest) != 0)
				goto LibFail;

			// Copy replacement pattern to dest.
			if(match.flags & RRegical)
				for(rmcp = match.rmcpat; rmcp->mc_type != MCE_Nil; ++rmcp) {
					if(dputs(rmcp->mc_type == MCE_LitString ? rmcp->u.rstr :
					 rmcp->mc_type == MCE_Match ? match.matchp->d_str :
					 fixnull(match.groups[rmcp->u.grpnum].matchp->d_str),&dest) != 0)
						goto LibFail;
					}
			else if(dputs(match.rpat,&dest) != 0)
				goto LibFail;

			// Copy remaining source text to dest if any, and close it.
			if(((len = strlen(sp->d_str + offset + sfp->len)) > 0 &&
			 dputmem((void *) (sp->d_str + offset + sfp->len),len,&dest) != 0) || dclose(&dest,sf_string) != 0) {
LibFail:
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
					if(dputc(c1,sfp) != 0)
						goto LibFail;
				++str;
				break;
			case '\\':
				if(str[1] != '\0')
					c1 = *++str;
				// Fall through.
			default:
LitChar:
				if(dputc(c1,sfp) != 0)
					goto LibFail;
			}
		} while(*++str != '\0');

	if(dclose(sfp,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
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
				goto LibFail;
			do {
				if(dputc(c,&sf) != 0)
					goto LibFail;
				} while(--n > 0);
			if(dclose(&sf,sf_string) != 0)
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
		goto LibFail;
	str = srcp->d_str;
	lento = strlen(xtop->d_str);
	while(*str != '\0') {

		// Scan lookup table for a match.
		xf = xfromp->d_str;
		do {
			if(*str == *xf) {
				if(lento > 0 && dputc(xtop->d_str[xf - xfromp->d_str],&result) != 0)
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

// Concatenate all function arguments into rp if runtime flag OpEval is set; otherwise, just consume them.  reqct is the number
// of required arguments.  ArgFirst flag is set on first argument.  Null and nil arguments are included in result if CvtKeepNil
// and/or CvtKeepNull flags are set; otherwise, they are skipped.  A nil argument is output as a keyword if CvtShowNil,
// CvtVizStr, or CvtVizStrQ flag is set; otherwise, a null string.  Boolean arguments are always output as "false" and "true"
// and arrays are processed (recursively) as if each element was specified as an argument.  Return status.
int catargs(Datum *rp,int reqct,Datum *delimp,uint flags) {
	uint aflags = ArgFirst | ArgBool1 | ArgArray1 | ArgNIS1;
	DStrFab sf;
	Datum *datp;		// For arguments.
	bool firstWrite = true;
	char *dlm = (delimp != NULL && !disnn(delimp)) ? delimp->d_str : NULL;

	// Nothing to do if not evaluating and no arguments; for example, an "abort()" call.
	if((si.opflags & (OpScript | OpParens)) == (OpScript | OpParens) && havesym(s_rparen,false) &&
	 (!(si.opflags & OpEval) || reqct == 0))
		return rc.status;

	if(dnewtrk(&datp) != 0 || ((si.opflags & OpEval) && dopenwith(&sf,rp,SFClear) != 0))
		goto LibFail;

	for(;;) {
		if(aflags & ArgFirst) {
			if(!havesym(s_any,reqct > 0))
				break;				// Error or no arguments.
			}
		else if(!havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(datp,aflags) != Success)
			return rc.status;
		--reqct;
		if(si.opflags & OpEval) {

			// Skip nil or null string if appropriate.
			if(datp->d_type == dat_nil) {
				if(!(flags & CvtKeepNil))
					goto Onward;
				}
			else if(disnull(datp) && !(flags & CvtKeepNull))
				goto Onward;

			// Write optional delimiter and value.
			if(dlm != NULL && !firstWrite && dputs(dlm,&sf) != 0)
				goto LibFail;
			if(dtosfchk(&sf,datp,dlm,flags) != Success)
				return rc.status;
			firstWrite = false;
			}
Onward:
		aflags = ArgBool1 | ArgArray1 | ArgNIS1;
		}

	// Return result.
	if((si.opflags & OpEval) && dclose(&sf,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Process a strPop, strPush, strShift, or strUnshift function and store result in rp if evaluating; otherwise, just "consume"
// arguments.  Set rp to nil if strShift or strPop and no items left.  Return status.
static int strfunc(Datum *rp,enum cfid fid,char *fname) {
	int status;
	VDesc vd;
	char *str1,*str2,*newvarval;
	short delim = 0;
	bool spacedlm = false;
	Datum *delimp,*oldvarvalp,*argp,newvar,*newvarp;

	// Syntax of functions:
	//	strPush var,dlm,val    strPop var,dlm    strShift var,dlm    strUnshift var,dlm,val

	// Get variable name from current symbol, find it and its value, and validate it.
	if(!havesym(s_any,true))
		return rc.status;
	if(si.opflags & OpEval) {
		if(dnewtrk(&oldvarvalp) != 0)
			goto LibFail;
		if(findvar(str1 = last->p_tok.d_str,&vd,OpDelete) != Success)
			return rc.status;
		if((vd.vd_type == VTyp_SVar && (vd.p.vd_svp->sv_flags & V_RdOnly)) ||
		 (vd.vd_type == VTyp_NVar && vd.i.vd_argnum == 0))
			return rcset(Failure,RCTermAttr,text164,str1);
				// "Cannot modify read-only variable '~b%s~B'"
		if(vderefv(oldvarvalp,&vd) != Success)
			return rc.status;

		// Have var value in oldvarvalp.  Verify that it is nil or string.
		if(oldvarvalp->d_type == dat_nil)
			dsetnull(oldvarvalp);
		else if(!strval(oldvarvalp))
			return rc.status;
		}

	// Get delimiter into delimp and also delim if single byte (strShift and strPop).
	if(dnewtrk(&delimp) != 0)
		goto LibFail;
	if(getsym() < NotFound ||
	 funcarg(delimp,(fid == cf_strShift || fid == cf_strPop) ? (ArgInt1 | ArgNil1 | ArgMay) : ArgNil1) != Success)
		return rc.status;
	if(si.opflags & OpEval) {
		if(fid == cf_strShift || fid == cf_strPop) {
			if(delimp->d_type == dat_nil)
				delim = '\0';
			else if(!charval(delimp))
				return rc.status;
			else if((delim = delimp->u.d_int) == ' ')
				spacedlm = true;
			}
		else if(delimp->d_type == dat_nil)
			dsetnull(delimp);
		}

	// Get value argument into argp for strPush and strUnshift functions.
	if(fid == cf_strPush || fid == cf_strUnshift) {
		if(dnewtrk(&argp) != 0)
			goto LibFail;
		if(funcarg(argp,ArgNIS1) != Success)
			return rc.status;
		}

	// If not evaluating, we're done (all arguments consumed).
	if(!(si.opflags & OpEval))
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
				status = rparsetok(rp,&str1,newvarval,spacedlm ? -1 : delim);
				}
			goto Check;
		case cf_strShift:
			newvarval = oldvarvalp->d_str;
			status = parsetok(rp,&newvarval,spacedlm ? -1 : delim);
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
				goto LibFail;

			// Append a delimiter if oldvarvalp is not null, and value (if strPush) or var (if strUnshift).
			if((!disnull(oldvarvalp) && dputs(delimp->d_str,&sf) != 0) || dputs(str2,&sf) != 0 ||
			 dclose(&sf,sf_string) != 0)
				goto LibFail;
			newvarp = rp;				// New var value.
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

// Insert, overwrite, replace, or write the text in srcp to a buffer n times.  This routine's job is to make the given buffer
// the current buffer so that iorstr() can insert the text, then restore the current screen to its original form.  srcp, n, and
// style are passed to iorstr().  If bufp is NULL, the current buffer is used.  Return status.
int iortext(char *srcp,int n,ushort style,Buffer *bufp) {
	EScreen *oscrp = NULL;
	EWindow *owinp = NULL;
	Buffer *obufp = NULL;

	// If the target buffer is being displayed in another window, remember current window and move to the other one;
	// otherwise, switch to the buffer in the current window.
	if(bufp != NULL && bufp != si.curbp) {
		if(bufp->b_nwind == 0) {

			// Target buffer is not being displayed in any window... switch to it in current window.
			obufp = si.curbp;
			if(bswitch(bufp,SWB_NoHooks) != Success)
				return rc.status;
			}
		else {
			// Target buffer is being displayed.  Get window and find screen.
			EWindow *winp = findwind(bufp);
			EScreen *scrp = si.sheadp;
			EWindow *winp2;
			owinp = si.curwp;
			do {
				winp2 = scrp->s_wheadp;
				do {
					if(winp2 == winp)
						goto Found;
					} while((winp2 = winp2->w_nextp) != NULL);
				} while((scrp = scrp->s_nextp) != NULL);
Found:
			// If screen is different, switch to it.
			if(scrp != si.cursp) {
				oscrp = si.cursp;
				if(sswitch(scrp,SWB_NoHooks) != Success)
					return rc.status;
				}

			// If window is different, switch to it.
			if(winp != si.curwp) {
				(void) wswitch(winp,SWB_NoHooks);		// Can't fail.
				supd_wflags(NULL,WFMode);
				}
			}
		}

	// Target buffer is current.  Now insert, overwrite, or replace the text in srcp n times.
	if(iorstr(srcp,n,style,false) == Success) {

		// Restore old screen, window, and/or buffer, if needed.
		if(obufp != NULL)
			(void) bswitch(obufp,SWB_NoHooks);
		else if(oscrp != NULL) {
			if(sswitch(oscrp,SWB_NoHooks) != Success)
				return rc.status;
			if(owinp != si.curwp)
				goto RestoreWind;
			}
		else if(owinp != NULL) {
RestoreWind:
			(void) wswitch(owinp,SWB_NoHooks);		// Can't fail.
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
	uint aflags = ArgFirst | ArgBool1 | ArgArray1 | ArgNIS1;

	if(n == INT_MIN)
		n = 1;

	if(dnewtrk(&dtextp) != 0)
		goto LibFail;

	// Evaluate all the arguments and save in string-fab object (so that the text can be inserted more than once,
	// if requested).
	if(dopenwith(&text,rp,SFClear) != 0)
		goto LibFail;

	for(;;) {
		if(!(aflags & ArgFirst) && !havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(dtextp,aflags) != Success)
			return rc.status;
		aflags = ArgBool1 | ArgArray1 | ArgNIS1;
		if(disnn(dtextp))
			continue;				// Ignore null and nil values.
		if(dtextp->d_type == dat_blobRef || (dtextp->d_type & DBoolMask)) {
			if(dtosfchk(&text,dtextp,NULL,0) != Success)
				return rc.status;
			}
		else if(dputd(dtextp,&text) != 0)		// Add text chunk to string-fab object.
			goto LibFail;
		}
	if(dclose(&text,sf_string) != 0)
		goto LibFail;

	// We have all the text in rp.  Now insert, overwrite, or replace it n times.
	return iortext(rp->d_str,n,style,bufp);
LibFail:
	return librcset(Failure);
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

	// Write the message out: enable terminal attributes if n >= 0.
	if(n >= 0)
		flags |= MLTermAttr;
	(void) mlputs(flags,rp->d_str);
	dsetnil(rp);
	return rc.status;
	}

// Return next argument to strfmt(), "flattening" arrays in the process.  Return status.
static int fmtarg(Datum *rp,uint aflags,ArrayState *asp) {

	for(;;) {
		if(asp->aryp == NULL) {
			if(funcarg(rp,aflags | ArgArray1 | ArgMay) != Success)
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

	if(aflags == ArgInt1)
		(void) intval(rp);
	else if(aflags == ArgNil1 && rp->d_type != dat_nil)
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
	if(as.aryp != NULL && as.i < as.aryp->a_used)
		return rcset(Failure,RCNoFormat,text204);
			// "Too many arguments for 'printf' or 'sprintf' function"
	if(dclose(&result,sf_string) != 0)
LibFail:
		(void) librcset(Failure);

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

#ifndef OSName
// Get OS name for getInfo function and return it in rp.  Return status.
static int getOS(Datum *rp) {
	static char myname[] = "getOS";
	static char *osname = NULL;
	char *name;

	// Get uname() info if first call.
	if((name = osname) == NULL) {
		struct utsname utsname;
		struct osinfo {
			char *vkey;
			char *oname;
			} *osip;
		static struct osinfo osi[] = {
			{VersKey_MacOS,OSName_MacOS},
			{VersKey_Debian,OSName_Debian},
			{VersKey_Ubuntu,OSName_Ubuntu},
			{NULL,NULL}};

		if(uname(&utsname) != 0)
			return scallerr(myname,"uname",false);

		// OS name in "version" string?
		osip = osi;
		do {
			if(strcasestr(utsname.version,osip->vkey) != NULL) {
				name = osip->oname;
				goto Save;
				}
			} while((++osip)->vkey != NULL);

		// Nope.  CentOS or Red Hat release file exist?
		struct stat s;

		if(stat(CentOS_Release,&s) == 0) {
			name = OSName_CentOS;
			goto Save;
			}
		if(stat(RedHat_Release,&s) == 0) {
			name = OSName_RedHat;
			goto Save;
			}

		// Nope.  Use system name from uname() call.
		name = utsname.sysname;
Save:
		// Save OS name on heap for future calls, if any.
		if((osname = (char *) malloc(strlen(name) + 1)) == NULL)
			return rcset(Panic,0,text94,myname);
				// "%s(): Out of memory!"
		strcpy(osname,name);
		}

	return dsetstr(name,rp) != 0 ? librcset(Failure) : rc.status;
	}
#endif

// Build array for getInfo function and return it in rp.  Return status.
static int getary(Datum *rp,int n,InfoTab *itp) {
	Array *aryp0,*aryp1,*aryp;
	Datum *datp,**elpp;
	EScreen *scrp;

	if(mkarray(rp,&aryp0) != Success)
		goto Retn;
	switch(itp->id) {
		case cf_showBuffers:;		// [buf-name,...]
			Buffer *bufp;
			ushort exclude = (n == INT_MIN) ? BFHidden : n <= 0 ? BFMacro : 0;

			// Cycle through buffers and make list in aryp0.
			aryp = &buftab;
			while((datp = aeach(&aryp)) != NULL) {
				bufp = bufptr(datp);
				if(!(bufp->b_flags & exclude) &&
				 ((datp = aget(aryp0,aryp0->a_used,true)) == NULL || dsetstr(bufp->b_bname,datp) != 0))
					goto LibFail;
				}
			break;
		case cf_showColors:;		// [colors,pairs] or nil
			if(!(si.opflags & OpHaveColor))
				dsetnil(rp);
			else {
				if((datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					goto LibFail;
				dsetint(term.maxColor,datp);
				if((datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					goto LibFail;
				dsetint(term.maxWorkPair,datp);
				}
			break;
		case cf_showHooks:;		// [[hook-name,macro-name],...]
			HookRec *hrp = hooktab;

			// Cycle through hook table and make list.  For each hook, create array in aryp1 and push onto aryp0.
			do {
				if((aryp1 = anew(2,NULL)) == NULL)
					goto LibFail;
				if(dsetstr(hrp->h_name,aryp1->a_elpp[0]) != 0 ||
				 (hrp->h_bufp != NULL && dsetstr(hrp->h_bufp->b_bname + 1,aryp1->a_elpp[1]) != 0) ||
				 (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					goto LibFail;
				if(awrap(datp,aryp1) != Success)
					goto Retn;
				} while((++hrp)->h_name != NULL);
			break;
		case cf_showModes:;		// [[mode-name,group-name,user?,global?,hidden?,scope-lock?,active?],...]
			ModeSpec *msp;
			static ushort modeflags[] = {MdUser,MdGlobal,MdHidden,MdLocked,0};
			ushort *mfp;

			// Cycle through mode table and make list.  For each mode, create array in aryp1 and push onto aryp0.
			aryp = &mi.modetab;
			while((datp = aeach(&aryp)) != NULL) {
				msp = msptr(datp);
				if((aryp1 = anew(7,NULL)) == NULL)
					goto LibFail;
				elpp = aryp1->a_elpp;
				if(dsetstr(msp->ms_name,*elpp++) != 0 || (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					goto LibFail;
				if(msp->ms_group == NULL)
					++elpp;
				else if(dsetstr(msp->ms_group->mg_name,*elpp++) != 0)
					goto LibFail;
				mfp = modeflags;
				do {
					dsetbool(msp->ms_flags & *mfp++,*elpp++);
					} while(*mfp != 0);
				dsetbool((msp->ms_flags & MdGlobal) ? (msp->ms_flags & MdEnabled) :
				 bmsrch1(si.curbp,msp),*elpp++);
				if(awrap(datp,aryp1) != Success)
					goto Retn;
				}
			break;
		case cf_showScreens:;		// [[screen-num,wind-count,work-dir],...]

			// Cycle through screens and make list.  For each screen, create array in aryp1 and push onto aryp0.
			scrp = si.sheadp;
			do {
				if((aryp1 = anew(3,NULL)) == NULL)
					goto LibFail;
				elpp = aryp1->a_elpp;
				dsetint((long) scrp->s_num,*elpp++);
				dsetint((long) wincount(scrp,NULL),*elpp++);
				if(dsetstr(scrp->s_wkdir,*elpp) != 0 || (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
					goto LibFail;
				if(awrap(datp,aryp1) != Success)
					goto Retn;
				} while((scrp = scrp->s_nextp) != NULL);
			break;
		default:;			// [[windNum,bufName],...] or [[screenNum,windNum,bufName],...]
			EWindow *winp;
			long wnum;

			// Cycle through screens and make list.  For each window, create array in aryp1 and push onto aryp0.
			scrp = si.sheadp;
			do {
				if(scrp->s_num == si.cursp->s_num || n != INT_MIN) {

					// In all windows...
					wnum = 0;
					winp = scrp->s_wheadp;
					do {
						++wnum;
						if((aryp1 = anew(n == INT_MIN ? 2 : 3,NULL)) == NULL)
							goto LibFail;
						elpp = aryp1->a_elpp;
						if(n != INT_MIN)
							dsetint((long) scrp->s_num,*elpp++);
						dsetint(wnum,*elpp++);
						if(dsetstr(winp->w_bufp->b_bname,*elpp) != 0 ||
						 (datp = aget(aryp0,aryp0->a_used,true)) == NULL)
							goto LibFail;
						if(awrap(datp,aryp1) != Success)
							goto Retn;
						} while((winp = winp->w_nextp) != NULL);
					}
				} while((scrp = scrp->s_nextp) != NULL);
		}
Retn:
	// Return results.
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Resolve buffer name argument.
static Buffer *getbufarg(Datum *bname) {
	Buffer *bufp;

	if((bufp = bsrch(bname->d_str,NULL)) == NULL)
		(void) rcset(Failure,0,text118,bname->d_str);
			// "No such buffer '%s'"
	return bufp;
	}

// Get keyword argument(s) for function "cfp", compare them to values in the appropriate table, and process entries that match.
// Use modetab if bufMode? or globalMode? function and allow array arguments; otherwise, use bflaginfo table (bufAttr?
// function).  Return status.
//
// bufAttr?	name,flag,...		Keywords (bflaginfo)
// bufMode?	name,mode,...		Mode names (modetab)
// globalMode?	mode,...		Mode names (modetab)
static int tabcheck(Datum *rp,int n,Datum *bname,CmdFunc *cfp) {
	uint aflags = ArgFirst | ArgReq | ArgNotNull1;
	int status;
	ArraySize elct;
	ModeSpec *msp;
	char *keyword;
	bool matchFound;
	BufFlagSpec *bfsp;
	Datum *datp,*argp;
	Datum **elpp = NULL;
	Buffer *bufp = NULL;
	bool result = (n >= 0);
	int fnum = cfp - cftab;

	// Get ready.
	if(fnum != cf_globalModeQ && ((bufp = getbufarg(bname)) == NULL || !needsym(s_comma,true)))
		return rc.status;
	if(fnum != cf_bufAttrQ)
		aflags |= ArgArray1 | ArgMay;

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
		if(fnum == cf_bufAttrQ) {
			// bufAttr? function.
			bfsp = bflaginfo;
			do {
				if(strcasecmp(keyword,bfsp->name) == 0) {
					matchFound = true;
					if(bufp->b_flags & bfsp->mask) {
						if(n < 0)
							result = true;
						}
					else if(n >= 0)
						result = false;
					break;
					}
				} while((++bfsp)->name != NULL);
			}
		else {
			// bufMode? or globalMode? function.  Check mode table.
			if((msp = mdsrch(keyword,NULL)) != NULL) {

				// Match found.  Mode enabled?
				matchFound = true;
				if(bufp == NULL) {
					if(msp->ms_flags & MdEnabled)
						goto Good;
					goto Bad;
					}
				else if(bmsrch1(bufp,msp)) {
Good:
					// Yes, done (success) if "any" check.
					if(n < 0)
						result = true;
					}

				// Flag not set.  Failure if "all" check.
				else {
Bad:
					if(n >= 0)
						result = false;
					}
				}
			}

		// Error if keyword match not found and bufAttr? function or n > 0.
		if(!matchFound && (fnum == cf_bufAttrQ || n == INT_MIN || n > 0))
			return rcset(Failure,0,text344,cfp->cf_name,keyword);
					// "Unknown %s argument '%s'"
		}

	// All arguments processed.  Return result.
	dsetbool(result,rp);
	return rc.status;
	}

// Check if a mode in given group is set and return its name if so; otherwise, nil.  Return status.
int groupMode(Datum *rp,int n,Datum **argpp) {
	ModeSpec *msp;
	ModeGrp *mgp;
	Array *aryp;
	Datum *datp;
	ushort count;
	Buffer *bufp = NULL;
	ModeSpec *resultp = NULL;

	// Get mode group.
	if(!mgsrch(argpp[0]->d_str,NULL,&mgp)) {
		if(n == INT_MIN || n > 0)
			return rcset(Failure,0,text395,text390,argpp[0]->d_str);
					// "No such %s '%s'","group"
		else
			mgp = NULL;
		}

	// Get buffer.
	if(n < 0) {
		if(funcarg(rp,ArgNotNull1) != Success || (bufp = getbufarg(rp)) == NULL)
			return rc.status;
		if(mgp == NULL)
			goto SetNil;
		}

	// If group has members, continue...
	if(mgp->mg_usect > 0) {

		// Check for type mismatch; that is, a global mode specified for a buffer group, or vice versa.
		aryp = &mi.modetab;
		while((datp = aeach(&aryp)) != NULL) {
			msp = msptr(datp);
			if(msp->ms_group == mgp) {
				if(!(msp->ms_flags & MdGlobal) != (n < 0))
					return rcset(Failure,0,text404,mgp->mg_name,n < 0 ? text83 : text146);
						// "'%s' is not a %s group","buffer","global"
				break;
				}
			}

		// Have valid argument(s).  Check if any mode is enabled in group.
		aryp = &mi.modetab;
		count = 0;
		while((datp = aeach(&aryp)) != NULL) {
			msp = msptr(datp);
			if(msp->ms_group == mgp) {
				if(bufp == NULL) {
					if(msp->ms_flags & MdEnabled)
						goto Enabled;
					}
				else if(bmsrch1(bufp,msp)) {
Enabled:
					resultp = msp;
					break;
					}
				if(++count == mgp->mg_usect)
					break;
				}
			}
		}

	// Return result.
	if(resultp != NULL) {
		if(dsetstr(resultp->ms_name,rp) != 0)
			(void) librcset(Failure);
		}
	else
SetNil:
		dsetnil(rp);
	return rc.status;
	}

// binsearch() helper function for returning a getInfo keyword, given table (array) and index.
static char *gikw(void *table,ssize_t i) {

	return ((InfoTab *) table)[i].keyword;
	}

// Get informational item per keyword argument.  Return status.
static int getInfo(Datum *rp,int n,char *myname) {
	static InfoTab itab[] = {
		{"buffers",NULL,cf_showBuffers},	// [buf-name,...]
		{"colors",NULL,cf_showColors},		// [colors,pairs]
		{"editor",Myself,-1},
		{"hooks",NULL,cf_showHooks},		// [[hook-name,macro-name],...]
		{"language",Language,-1},
		{"modes",NULL,cf_showModes},	// [[mode-name,group-name,user?,global?,visible?,scope-lock?,active?],...]
#ifdef OSName
		{"os",OSName,-1},
#else
		{"os",NULL,-1},
#endif
		{"screens",NULL,cf_showScreens},	// [[screen-num,wind-count,work-dir],...]
		{"version",Version,-1},
		{"windows",NULL,-1}};			// [[windNum,bufName],...] or [[screenNum,windNum,bufName],...]
	ssize_t i;
	Datum *datp;
	InfoTab *itp;

	// Get keyword.
	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	if(funcarg(datp,ArgFirst | ArgNotNull1) != Success)
		return rc.status;

	// Search table for a match.
	if(binsearch(datp->d_str,(void *) itab,sizeof(itab) / sizeof(InfoTab),strcasecmp,gikw,&i)) {

		// Match found.  Check keyword type.
		itp = itab + i;
		if(itp->value != NULL)
			return dsetstr(itp->value,rp) != 0 ? librcset(Failure) : rc.status;
#ifndef OSName
		if(strcmp(itp->keyword,"os") == 0)
			return getOS(rp);
#endif
		// Not a string constant value.  Build array and return it.
		return getary(rp,n,itp);
		}

	// Match not found.
	return rcset(Failure,0,text344,myname,datp->d_str);
			// "Unknown %s argument '%s'"
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

// Convert any value into a string form which will resolve to the original value if subsequently evaluated as an expression
// (unless "force" is set (n > 0) and value is an array that includes itself).  Return status.
#if !(MMDebug & Debug_MacArg)
static
#endif
int dquote(Datum *rp,Datum *datp,uint flags) {
	DStrFab sf;

	if(dopenwith(&sf,rp,SFClear) != 0)
		goto LibFail;
	if(dtosfchk(&sf,datp,NULL,flags) == Success && dclose(&sf,sf_string) != 0)
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

// Set pattern at top of search or replace ring.  Make a copy so that a search pattern on the ring is not modified by
// chkopts() if newspat() is called.
static int setTopPat(Ring *ringp) {
	char *pat = (ringp->r_size == 0) ? "" : ringp->r_entryp->re_data.d_str;
	if(ringp == &sring) {
		char wkbuf[strlen(pat) + 1];

		return newspat(strcpy(wkbuf,pat),&srch.m,NULL);
		}
	return newrpat(pat,&srch.m);
	}

// Delete entri(es) from search or replace ring and update or delete current pattern in Match record, if applicable.
static int delpat(Ring *ringp,int n) {

	if(rdelete(ringp,n) == Success)
		(void) setTopPat(ringp);

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
	Datum *argp[CFMaxArgs + 1];	// Argument values plus NULL terminator.
	int argct = 0;			// Number of arguments loaded into argp.

	// If script mode and CFNoLoad is not set, retrieve the minimum number of arguments specified in cftab, up to the
	// maximum, and save in argp.  If CFSpecArgs flag is set or the maximum is negative, use the minimum for the maximum;
	// otherwise, if CFShrtLoad flag is set, decrease the maximum by one.
	argp[0] = NULL;
	if((si.opflags & (OpScript | OpNoLoad)) == OpScript && !(cfp->cf_aflags & CFNoLoad) &&
	 (!(si.opflags & OpParens) || !havesym(s_rparen,false))) {
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
					goto LibFail;
				if(funcarg(*argpp++,(argct == 0 ? ArgFirst : 0) | (((cfp->cf_vflags & (ArgNotNull1 << argct)) |
				 (cfp->cf_vflags & (ArgNil1 << argct)) | (cfp->cf_vflags & (ArgBool1 << argct)) |
				 (cfp->cf_vflags & (ArgInt1 << argct)) | (cfp->cf_vflags & (ArgArray1 << argct)) |
				 (cfp->cf_vflags & (ArgNIS1 << argct))) >> argct) | (cfp->cf_vflags & ArgMay)) != Success)
					return rc.status;
				} while(++argct < minArgs || (argct < maxArgs && havesym(s_comma,false)));
			*argpp = NULL;
			}
		}

	// Evaluate the command or function.
	if(cfp->cf_func != NULL)
		(void) cfp->cf_func(rp,n,argp);
	else {
		int i;
		char *str;
		long lval;
		Buffer *bufp;
		int fnum = cfp - cftab;

		switch(fnum) {
			case cf_abs:
				dsetint(labs(argp[0]->u.d_int),rp);
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
				Dot *dotp = &si.curwp->w_face.wf_dot;
				(void) (si.stabsize > 0 && dotp->off > 0 ? deltab(n == INT_MIN ? -1 : -n,true) :
				 ldelete(n == INT_MIN ? -1L : (long) -n,0));
				break;
			case cf_basename:
				if(dsetstr(fbasename(argp[0]->d_str,n == INT_MIN || n > 0),rp) != 0)
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
				(void) beline(rp,n,false);
				break;
			case cf_beginWhite:
				// Move the point to the beginning of white space surrounding dot.
				(void) spanwhite(false);
				break;
			case cf_bprintf:

				// Get buffer pointer.
				if((bufp = bsrch(argp[0]->d_str,NULL)) == NULL)
					return rcset(Failure,0,text118,argp[0]->d_str);
						// "No such buffer '%s'"

				// Create formatted string in rp and insert into buffer.
				if(strfmt(rp,argp[1],NULL) == Success)
					(void) iortext(rp->d_str,n == INT_MIN ? 1 : n,Txt_Insert,bufp);
				break;
			case cf_bufBoundQ:
				if(n != INT_MIN)
					n = n > 0 ? 1 : n < 0 ? -1 : 0;
				i = bufend(NULL) ? 1 : bufbegin(NULL) ? -1 : 0;
				dsetbool((n == INT_MIN && i != 0) || i == n,rp);
				break;
			case cf_bufAttrQ:
			case cf_bufModeQ:
			case cf_globalModeQ:
				(void) tabcheck(rp,n,argp[0],cfp);
				break;
			case cf_bufSize:;
				long bct,lct;

				bct = buflength(si.curbp,&lct);
				dsetint(n == INT_MIN ? lct : bct,rp);
				break;
			case cf_bufWind:
				{EWindow *winp;

				if((bufp = bsrch(argp[0]->d_str,NULL)) != NULL && (winp = whasbuf(bufp,n != INT_MIN)) != NULL)
					dsetint((long) getwnum(winp),rp);
				else
					dsetnil(rp);
				}
				break;
			case cf_chgBufMode:
				i = 0;
				if(!(si.opflags & OpScript))
					bufp = NULL;
				else {
					if((bufp = getbufarg(argp[0])) == NULL)
						break;
					if(n <= 1)
						i = ArgReq;
					}
				goto ChgMode;
			case cf_chgGlobalMode:
				i = (n > 1) ? (MdGlobal | ArgFirst) : (MdGlobal | ArgFirst | ArgReq);
				bufp = NULL;
ChgMode:
				(void) changeMode(rp,n,i,bufp);
				break;
			case cf_chr:
				if(charval(argp[0]))
					dsetchr(argp[0]->u.d_int,rp);
				break;
			case cf_clearMsgLine:
				(void) mlerase();
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
			case cf_cycleKillRing:
			case cf_cycleReplaceRing:
			case cf_cycleSearchRing:
				// Cycle a ring forward or backward.  If search or replace ring, also set pattern at top
				// when done.
				if(n != 0) {
					Ring *ringp = (fnum == cf_cycleKillRing) ? &kring : (fnum == cf_cycleSearchRing) ?
					 &sring : &rring;
					if(ringp->r_size > 1 && rcycle(ringp,n,true) == Success && fnum != cf_cycleKillRing)
						(void) setTopPat(ringp);
					}
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
					goto LibFail;
				break;
			case cf_emptyQ:
				if(argp[0]->d_type != dat_int || charval(argp[0]))
					dsetbool(argp[0]->d_type == dat_nil ? true :
					 argp[0]->d_type == dat_int ? argp[0]->u.d_int == 0 :
					 argp[0]->d_type & DStrMask ? *argp[0]->d_str == '\0' :
					 awptr(argp[0])->aw_aryp->a_used == 0,rp);
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
					goto LibFail;
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
			case cf_getInfo:
				(void) getInfo(rp,n,cfp->cf_name);
				break;
			case cf_getKey:
				{ushort ek;
				char keybuf[16];

				if(n == INT_MIN)
					n = 1;
				if((n <= 1 ? getkey(true,&ek,true) : getkseq(true,&ek,NULL,true)) != Success)
					break;
				if(ek == corekeys[CK_Abort].ek)
					return abortinp();
				if(n <= 0)
					dsetint(ektoc(ek,true),rp);
				else if(dsetstr(ektos(ek,keybuf,false),rp) != 0)
					goto LibFail;
				}
				break;
			case cf_growWind:
				// Grow (enlarge) the current window.  If n is negative, take lines from the upper window;
				// otherwise, lower.
				i = 1;
				goto GSWind;
			case cf_index:
				(void) sindex(rp,n,argp[0],argp[1],NULL);
				break;
			case cf_insert:
				(void) chgtext(rp,n,Txt_Insert,NULL);
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
				dsetbool(!(si.opflags & OpStartup) && !(last->p_flags & OpScript),rp);
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
				if(si.cursp->s_lastbufp != NULL) {
					Buffer *oldbufp = si.curbp;
					if(render(rp,1,si.cursp->s_lastbufp,0) == Success && n != INT_MIN && n < 0) {
						char bname[MaxBufName + 1];

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
			case cf_lowerCaseStr:
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
					goto LibFail;
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
				i = false;
				goto PNBuf;
			case cf_nextScreen:
				i = SWB_Repeat | SWB_Forw;
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
					Dot *dotp = &si.curwp->w_face.wf_dot;
					if(dotp->lnp->l_used > 0 && dotp->lnp->l_nextp->l_used == 0)
						(void) movech(1);
					}
				break;
			case cf_ord:
				dsetint((long) argp[0]->d_str[0],rp);
				break;
			case cf_overwrite:
				(void) chgtext(rp,n,Txt_Overwrite,NULL);
				break;
			case cf_pause:
				if((i = argp[0]->u.d_int) < 0)
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
				if(ainsert(aryp,fnum == cf_push ? aryp->a_used : 0,argp[1],false) != 0)
					goto LibFail;
				else {
					duntrk(argp[1]);
					datxfer(rp,argp[0]);
					}
				}
				break;
			case cf_prevBuf:
				// Switch to the previous buffer in the buffer list.
				i = true;
PNBuf:
				(void) pnbuffer(rp,n,i);
				break;
			case cf_prevScreen:
				i = SWB_Repeat;
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
				if(gtfilename(rp,n == -1 ? text299 : text131,si.curbp->b_fname,0) != Success ||
							// "Pop file","Read file"
				 rp->d_type == dat_nil)
					break;

				// and return results.
				(void) readFP(rp,n,rp->d_str,RWExist);
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
				si.curwp->w_rfrow = (n == INT_MIN) ? 0 : n;
				si.curwp->w_flags |= WFReframe;
				if(si.opflags & OpScript)
					(void) wupd_reframe(si.curwp);
				break;
			case cf_replace:
				// Search and replace.
				i = false;
Repl:
				(void) replstr(rp,n,i,argp);
				break;
			case cf_replaceText:
				(void) chgtext(rp,n,Txt_Replace,NULL);
				break;
			case cf_restoreBuf:
				// Restore the saved buffer.
				if(si.savbufp == NULL)
					return rcset(Failure,0,text208,text83);
							// "Saved %s not found","buffer"
				if(bswitch(si.savbufp,0) == Success && dsetstr(si.curbp->b_bname,rp) != 0)
					goto LibFail;
				break;
			case cf_restoreScreen:;
				// Restore the saved screen.
				EScreen *scrp;

				// Find the screen.
				scrp = si.sheadp;
				do {
					if(scrp == si.savscrp) {
						dsetint((long) scrp->s_num,rp);
						return sswitch(scrp,0);
						}
					} while((scrp = scrp->s_nextp) != NULL);
				(void) rcset(Failure,0,text208,text380);
					// "Saved %s not found","screen"
				si.savscrp = NULL;
				break;
			case cf_restoreWind:
				// Restore the saved window.
				{EWindow *winp;

				// Find the window.
				winp = si.wheadp;
				do {
					if(winp == si.savwinp) {
						si.curwp->w_flags |= WFMode;
						(void) wswitch(winp,0);
						si.curwp->w_flags |= WFMode;
						dsetint((long) getwnum(si.curwp),rp);
						return rc.status;
						}
					} while((winp = winp->w_nextp) != NULL);
				(void) rcset(Failure,0,text208,text331);
					// "Saved %s not found","window"
				}
				break;
			case cf_saveBuf:
				// Save pointer to current buffer.
				si.savbufp = si.curbp;
				if(dsetstr(si.curbp->b_bname,rp) != 0)
					goto LibFail;
				break;
			case cf_saveFile:
				// Save the contents of the current buffer (or all buffers if n argument) to their associated
				// files.
				(void) savebufs(n,0);
				break;
			case cf_saveScreen:
				// Save pointer to current screen.
				si.savscrp = si.cursp;
				break;
			case cf_saveWind:
				// Save pointer to current window.
				si.savwinp = si.curwp;
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
					if(getnarg(rp,text59) != Success || rp->d_type == dat_nil)
							// "Wrap column"
						return rc.status;
					n = rp->u.d_int;
					}
				(void) setwrap(n,true);
				break;
			case cf_shellCmd:
				str = "> ";
				i = PipePopOnly;
PipeCmd:
				(void) pipecmd(rp,n,str,i);
				break;
			case cf_showDir:
				if(dsetstr(si.cursp->s_wkdir,rp) != 0)
					goto LibFail;
				(void) rcset(Success,RCNoFormat | RCNoWrap,si.cursp->s_wkdir);
				break;
			case cf_shQuote:
				if(tostr(argp[0]) == Success && dshquote(argp[0]->d_str,rp) != 0)
					goto LibFail;
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
				else {
					if(dsalloc(rp,argp[1]->u.d_int + 1) != 0)
						goto LibFail;
					strfit(rp->d_str,argp[1]->u.d_int,argp[0]->d_str,0);
					}
				break;
			case cf_strPop:
			case cf_strPush:
			case cf_strShift:
			case cf_strUnshift:
				(void) strfunc(rp,fnum,cfp->cf_name);
				break;
			case cf_strip:
				if(dsetstr(stripstr(argp[0]->d_str,n == INT_MIN ? 0 : n),rp) != 0)
					goto LibFail;
				break;
			case cf_sub:
				{ushort flags = 0;
				int (*sub)(Datum *rp,int n,Datum *sp,char *spat,char *rpat,ushort flags);
				if(argp[1]->d_type == dat_nil)
					dsetnull(argp[1]);
				else
					(void) chkopts(argp[1]->d_str,&flags);
				sub = (flags & SOpt_Regexp) ? resub : strsub;
				(void) sub(rp,n <= 1 ? 1 : n,argp[0],argp[1]->d_str,argp[2]->d_type == dat_nil ? "" :
				 argp[2]->d_str,flags);
				}
				break;
			case cf_subline:
				{long lval2 = argct < 2 ? LONG_MAX : argp[1]->u.d_int;
				lval = argp[0]->u.d_int;
				if(lval2 != 0 && (i = si.curwp->w_face.wf_dot.lnp->l_used) > 0) {

					// Determine line offset and length and validate them.  Offset is position relative to
					// dot.
					lval += si.curwp->w_face.wf_dot.off;
					if(lval >= 0 && lval < i && (lval2 >= 0 || (lval2 = i - lval + lval2) > 0)) {
						if(lval2 > i - lval)
							lval2 = i - lval;
						if(dsetsubstr(si.curwp->w_face.wf_dot.lnp->l_text + (int) lval,(size_t) lval2,
						 rp) != 0)
							goto LibFail;
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
							goto LibFail;
						}
					else
						dsetnull(rp);
					}
				else
					dsetnull(rp);
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
				(void) tcstr(rp,argp[0]);
				break;
			case cf_titleCaseWord:
				i = CaseWord | CaseTitle;
				goto CvtCase;
			case cf_toInt:
				datxfer(rp,argp[0]);
				(void) toint(rp);
				break;
			case cf_toStr:
				if(n == INT_MIN && argp[0]->d_type != dat_blobRef) {
					datxfer(rp,argp[0]);
					(void) tostr(rp);
					}
				else {
					DStrFab sf;

					if(dopenwith(&sf,rp,SFClear) != 0)
						goto LibFail;
					if(dtosfchk(&sf,argp[0],NULL,n == INT_MIN ? 0 : n < 0 ? CvtKeepNil | CvtShowNil :
					 n == 0 ? CvtKeepAll | CvtForceArray | CvtVizStr :
					 CvtKeepAll | CvtForceArray | CvtVizStrQ) != Success)
						break;
					if(dclose(&sf,sf_string) != 0)
						goto LibFail;
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
					if(bufend(NULL))
						break;
					bytes = LONG_MAX;
					}
				else {
					if(bufbegin(NULL))
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
			case cf_upperCaseStr:
				i = 1;
CvtStr:;
				char *(*mk)(char *dest,char *src) = (i <= 0) ? mklower : mkupper;
				if(dsalloc(rp,strlen(argp[0]->d_str) + 1) != 0)
					goto LibFail;
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
				(void) fvfile(rp,n,i);
				break;
			case cf_wordCharQ:
				if(charval(argp[0]))
					dsetbool(wordlist[argp[0]->u.d_int],rp);
				break;
			case cf_writeFile:
				i = 'w';
AWFile:
				(void) awfile(rp,n,i == 'w' ? text144 : text218,i);
						// "Write file: ","Append file"
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
int evalslit(Datum *rp,char *src) {
	short termch,c;
	DStrFab result;
#if MMDebug & Debug_Token
	char *src0 = src;
#endif
	// Get ready.
	if(dopenwith(&result,rp,SFClear) != 0)
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
			Datum *datp;

			// "#{" found.  Execute what follows to "}" as a command line.
			if(dnewtrk(&datp) != 0)
				goto LibFail;
			if(execestmt(datp,src + 2,TokC_ExprEnd,&src) != Success)
				return rc.status;

			// Append the result to dest.
			if(datp->d_type != dat_nil) {
				if(tostr(datp) != Success)
					return rc.status;
				if(dputd(datp,&result) != 0)
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
