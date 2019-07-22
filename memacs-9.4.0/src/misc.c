// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// misc.c	Miscellaneous functions for MightEMacs.
//
// This file contains command processing routines for some random commands.

#include "os.h"
#include <ctype.h>
#include "pllib.h"
#include "std.h"
#include "bind.h"
#include "exec.h"
#include "cmd.h"
#include "search.h"
#include "var.h"

// Is a character a letter?
bool isletter(short c) {

	return is_upper(c) || is_lower(c);
	}

// Is a character a lower case letter?
bool is_lower(short c) {

	return (c >= 'a' && c <= 'z');
	}

// Is a character an upper case letter?
bool is_upper(short c) {

	return (c >= 'A' && c <= 'Z');
	}

// Change the case of a letter.  First check lower and then upper.  If character is not a letter, it gets returned unchanged.
int chcase(short c) {

	// Translate lowercase.
	if(is_lower(c))
		return upcase[c];

	// Translate uppercase.
	if(is_upper(c))
		return lowcase[c];

	// Let the rest pass.
	return c;
	}

// Copy a string from src to dest, changing its case.  Return dest.
static char *trancase(char *dest,char *src,char *trantab) {
	char *str;

	str = dest;
	while(*src != '\0')
		*str++ = trantab[(int) *src++];
	*str = '\0';

	return dest;
	}

// Copy a string from src to dest, making it lower case.  Return dest.
char *mklower(char *dest,char *src) {

	return trancase(dest,src,lowcase);
	}

// Copy a string from src to dest, making it upper case.  Return dest.
char *mkupper(char *dest,char *src) {

	return trancase(dest,src,upcase);
	}

// Initialize the character upper/lower case tables.
void initchars(void) {
	int i;

	// Set all of both tables to their indices.
	for(i = 0; i < 256; ++i)
		lowcase[i] = upcase[i] = i;

	// Set letter translations.
	for(i = 'a'; i <= 'z'; ++i) {
		upcase[i] = i ^ 0x20;
		lowcase[i ^ 0x20] = i;
		}

	// And those international characters also.
	for(i = (uchar) '\340'; i <= (uchar) '\375'; ++i) {
		upcase[i] = i ^ 0x20;
		lowcase[i ^ 0x20] = i;
		}
	}

// Reverse string in place and return it.
char *strrev(char *s) {
	char *strz = strchr(s,'\0');
	if(strz - s > 1) {
		int c;
		char *str = s;
		do {
			c = *--strz;
			*strz = *str;
			*str++ = c;
			} while(strz > str);
		}
	return s;
	}

// Get line number, given buffer and line pointer.
long getlinenum(Buffer *buf,Line *targlnp) {
	Line *lnp;
	long n;

	// Start at the beginning of the buffer and count lines.
	lnp = buf->b_lnp;
	n = 0;
	do {
		// If we have reached the target line, stop.
		if(lnp == targlnp)
			break;
		++n;
		} while((lnp = lnp->l_next) != NULL);

	// Return result.
	return n + 1;
	}

// Return new column, given character in line and old column.
int newcol(short c,int col) {

	return col + (c == '\t' ? -(col % si.htabsize) + si.htabsize :
	 c < 0x20 || c == 0x7F ? 2 : c > 0x7F ? 4 : 1);
	}

// Return new column, given point and old column, adjusting for any terminal attribute specification if attribute enabled in
// current buffer.  *attrLen is set to length of specification, or zero if none.
int newcolTA(Point *point,int col,int *attrLen) {
	short c = point->lnp->l_text[point->off];
	if(c != AttrSpecBegin || !(si.curbuf->b_flags & BFTermAttr)) {
		*attrLen = 0;
		return newcol(c,col);
		}

	// Found terminal attribute specification... process it.
	int inviz;
	char *str0,*str;
	ushort flags = TAScanOnly;
	str = (str0 = point->lnp->l_text + point->off) + 1;
	inviz = termattr(&str,point->lnp->l_text + point->lnp->l_used,&flags,NULL);
	*attrLen = str - str0;
	return col + (*attrLen - inviz);
	}

// Return column of given point position.  If argument is NULL, use current point position.
int getcol(Point *point,bool termattr) {
	int i,col;

	if(point == NULL)
		point = &si.curwin->w_face.wf_point;

	col = 0;
	if(termattr) {
		Point tpoint = *point;
		int attrLen;
		for(tpoint.off = 0; tpoint.off < point->off; ++tpoint.off) {
			col = newcolTA(&tpoint,col,&attrLen);
			if(attrLen > 0)
				tpoint.off += attrLen - 1;
			}
		}
	else
		for(i = 0; i < point->off; ++i)
			col = newcol(point->lnp->l_text[i],col);

	return col;
	}

// Try to set current column to given position.  Return status.
int setccol(int pos) {
	int i;			// Index into current line.
	int col;		// Current point column.
	int llen;		// Length of line in bytes.
	Point *point = &si.curwin->w_face.wf_point;

	col = 0;
	llen = point->lnp->l_used;

	// Scan the line until we are at or past the target column.
	for(i = 0; i < llen; ++i) {

		// Upon reaching the target, drop out.
		if(col >= pos)
			break;

		// Advance one character.
		col = newcol(point->lnp->l_text[i],col);
		}

	// Set point to the new position and return.
	point->off = i;
	return rc.status;
	}

// Check if all white space from beginning of line, given length.  Return Boolean result, including true if length is zero.
bool is_white(Line *lnp,int length) {
	short c;
	int i;

	for(i = 0; i < length; ++i) {
		c = lnp->l_text[i];
		if(c != ' ' && c != '\t')
			return false;
		}
	return true;
	}

// Find pattern within source, using given Match record if match not NULL; otherwise, global "rematch" record.  Return status.
// If Idx_Char flag set, pattern is assumed to be a single character; otherwise, a plain text pattern or regular expression.  If
// Idx_Last flag set, last (rightmost) match is found; otherwise, first.  rval is set to 0-origin match position, or nil if no
// match.
int sindex(Datum *rval,ushort flags,Datum *src,Datum *pat,Match *match) {

	if(flags & Idx_Char) {

		// No match if source or character is null.
		if(charval(pat) && !disnull(src)) {
			int i;
			char *str;

			if((i = pat->u.d_int) != 0) {
				if(!(flags & Idx_Last)) {

					// Find first occurrence.
					if((str = strchr(src->d_str,i)) != NULL)
						goto Found;
					}
				else {
					// Find last (rightmost) occurrence.
					char *str0 = src->d_str;
					str = strchr(str0,'\0');
					do {
						if(*--str == i) {
Found:
							dsetint(str - src->d_str,rval);
							return rc.status;
							}
						} while(str != str0);
					}
				}
			}
		}

	// No match if source or pattern is null.
	else if(strval(pat) && !disnull(src) && !disnull(pat)) {

		// If match is NULL, compile pattern and save in global "rematch" record.
		if(match == NULL) {
			if(newspat(pat->d_str,match = &rematch,NULL) != Success ||
			 ((match->flags & SOpt_Regexp) && mccompile(match) != Success))
				return rc.status;
			grpclear(match);
			}

		// Check pattern type.
		if(match->flags & SOpt_Regexp) {
			int offset;

			// Have regular expression.  Perform operation...
			if(recmp(src,(flags & Idx_Last) ? -1 : 0,match,&offset) != Success)
				return rc.status;

			// and return index if a match was found.
			if(offset >= 0) {
				dsetint((long) offset,rval);
				return rc.status;
				}
			}
		else {
			char *src1,*srcz;
			size_t srclen;
			int incr;
			StrLoc *strloc = &match->groups->ml.str;
			int (*sncmp)(const char *s1,const char *s2,size_t n) = (match->flags & SOpt_Ignore) ?
			 strncasecmp : strncmp;

			// Have plain text pattern.  Scan through the source string.
			match->grpct = 0;
			strloc->len = strlen(pat->d_str);
			srclen = strlen(src1 = src->d_str);
			if(flags & Idx_Last) {
				srcz = src1 - 1;
				src1 += srclen + (incr = -1);
				}
			else {
				srcz = src1 + srclen;
				incr = 1;
				}

			do {
				// Scan through the string.  If match found, save results and return.
				if(sncmp(src1,pat->d_str,strloc->len) == 0) {
					strloc->strpoint.str = src1;
					dsetint((long) (src1 - src->d_str),rval);
					return savematch(match);
					}
				} while((src1 += incr) != srcz);
			}
		}

	// No match.
	dsetnil(rval);
	return rc.status;
	}

// Do "index" function.  Return status.
int indexF(Datum *rval,int n,Datum **argv) {
	ushort flags = 0;
	static Option options[] = {
		{"^Char",NULL,0,Idx_Char},
		{"^Last",NULL,0,Idx_Last},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		0,text451,false,options};
			// "function option"

	// Get options and set flags.
	if(n != INT_MIN) {
		if(parseopts(&ohdr,NULL,argv[2],NULL) != Success)
			return rc.status;
		flags = getFlagOpts(options);
		}

	return sindex(rval,flags,argv[0],argv[1],NULL);
	}

// Set or display i-variable parameters.  If interactive and n < 0, display parameters on message line; if n >= 0; set first
// parameter to n (only); otherwise, get arguments.  Return status.
int seti(Datum *rval,int n,Datum **argv) {
	int i = ivar.i;
	int inc = ivar.inc;
	bool newfmt = false;

	// Process n argument if interactive.
	if(!(si.opflags & OpScript) && n != INT_MIN) {
		if(n >= 0) {
			ivar.i = n;
			return rcset(Success,0,text287,ivar.i);
					// "i variable set to %d"
			}
		return rcset(Success,RCNoWrap | RCTermAttr,text384,AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,ivar.i,
				// "%c%ci:%c%c %d, %c%cformat:%c%c \"%s\", %c%cinc:%c%c %d"
		 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,ivar.format.d_str,
		 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,ivar.inc);
		}

	// Script mode or default n.  Get value(s).
	if(si.opflags & OpScript) {

		// Get "i" argument.
		i = (*argv++)->u.d_int;

		// Have "format" argument?
		if(*argv != NULL) {

			// Yes, get it.
			datxfer(rval,*argv++);
			newfmt = true;

			// Have "inc" argument?
			if(*argv != NULL) {

				// Yes, get it.
				inc = (*argv)->u.d_int;
				}
			}
		}
	else {
		Datum *datum;
		TermInp ti = {"1",RtnKey,0,NULL};
		char nbuf[LongWidth];

		if(dnewtrk(&datum) != 0)
			return librcset(Failure);

		// Prompt for "i" value.
		if(terminp(datum,text102,0,0,&ti) != Success || toint(datum) != Success)
				// "Beginning value"
			return rc.status;
		i = datum->u.d_int;

		// Prompt for "format" value.
		ti.defval = ivar.format.d_str;
		ti.delim = Ctrl | '[';
		if(terminp(rval,text235,ArgNotNull1,0,&ti) != Success)
				// "Format string"
			return rc.status;
		newfmt = true;

		// Prompt for "inc" value.
		sprintf(nbuf,"%d",inc);
		ti.defval = nbuf;
		ti.delim = RtnKey;
		if(terminp(datum,text234,0,0,&ti) != Success || toint(datum) != Success)
				// "Increment"
			return rc.status;
		inc = datum->u.d_int;
		}

	// Validate arguments.  rval contains format string.
	if(inc == 0)				// Zero increment.
		return rcset(Failure,RCNoFormat,text236);
			// "i increment cannot be zero!"

	// Validate format string if changed.
	if(newfmt) {
		if(strcmp(rval->d_str,ivar.format.d_str) == 0)
			newfmt = false;
		else {
			int c,icount,ocount;
			bool inspec = false;
			char *str = rval->d_str;

			icount = ocount = 0;
			do {
				c = *str++;
				if(inspec) {
					switch(c) {
						case '%':
							inspec = false;
							break;
						case 'd':
						case 'o':
						case 'u':
						case 'x':
						case 'X':
							++icount;
							inspec = false;
							break;
						default:
							if(strchr("0123456789+- .",c) == NULL) {
								++ocount;
								inspec = false;
								}
							}
					}
				else if(c == '%')
					inspec = true;
				} while(*str != '\0');

			if(icount != 1 || ocount > 0)		// Bad format string.
				return rcset(Failure,0,text237,rval->d_str);
					// "Invalid i format '%s' (must contain exactly one %%d, %%o, %%u, %%x, or %%X)"
			}
		}

	// Passed all edits... update ivar and return status.
	ivar.i = i;
	ivar.inc = inc;
	if(newfmt)
		datxfer(&ivar.format,rval);
	dsetnil(rval);
	return rc.status;
	}

// Return a pseudo-random integer in range 1..max.  If max <= 0, return zero.  This is a slight variation of the Xorshift
// pseudorandom number generator discovered by George Marsaglia.
long xorshift64star(long max) {
	if(max <= 0)
		return 0;
	else if(max == 1)
		return 1;
	else {
		si.randseed ^= si.randseed >> 12; // a
		si.randseed ^= si.randseed << 25; // b
		si.randseed ^= si.randseed >> 27; // c
		}
	return ((si.randseed * 0x2545F4914F6CDD1D) & LONG_MAX) % max + 1;
	}

// Do include? system function.  Return status.
int doincl(Datum *rval,int n,Datum **argv) {
	bool result;
	Array *ary = awptr(argv[1])->aw_ary;
	ArraySize elct;
	Datum *datum,**elp;
	bool aryMatch,elMatch;
	bool firstArg = true;
	static bool any;
	static bool ignore;
	static Option options[] = {
		{"^All",NULL,OptFalse,.u.ptr = (void *) &any},
		{"^Ignore",NULL,0,.u.ptr = (void *) &ignore},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		ArgNil1,text451,false,options};
			// "function option"

	// Get options and set flags.
	initBoolOpts(options);
	if(parseopts(&ohdr,NULL,argv[0],NULL) != Success)
		return rc.status;
	setBoolOpts(options);

	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	result = !any;
	for(;;) {
		// Get next argument.
		if(!firstArg && !havesym(s_comma,false))
			break;					// At least one argument retrieved and none left.
		if(funcarg(datum,ArgNIS1 | ArgBool1 | ArgArray1) != Success)
			return rc.status;
		firstArg = false;

		// Loop through array elements and compare them to the argument if final result has not yet been
		// determined.
		if(result == !any) {
			elct = ary->a_used;
			elp = ary->a_elp;
			elMatch = false;
			while(elct-- > 0) {
				if((*elp)->d_type == dat_blobRef) {
					if(datum->d_type != dat_blobRef)
						goto NextEl;
					if(aryeq(datum,*elp,ignore,&aryMatch) != Success)
						return rc.status;
					if(aryMatch)
						goto Match;
					goto NextEl;
					}
				else if(dateq(datum,*elp,ignore)) {
Match:
					if(any)
						result = true;
					else
						elMatch = true;
					break;
					}
NextEl:
				++elp;
				}

			// Match found or all array elements checked.  Fail if "all" mode and no match found.
			if(!any && !elMatch)
				result = false;
			}
		}

	dsetbool(result,rval);
	return rc.status;
	}

// Create an empty array, stash into rval, and store pointer to it in *aryp.  Return status.
int mkarray(Datum *rval,Array **aryp) {

	return (*aryp = anew(0,NULL)) == NULL ? librcset(Failure) : awrap(rval,*aryp);
	}

// Mode and group attribute control flags.
#define MG_Nil		0x0001		// Nil value allowed.
#define MG_Bool		0x0002		// Boolean value allowed.
#define MG_ModeAttr	0x0004		// Valid mode attribute.
#define MG_GrpAttr	0x0008		// Valid group attribute.
#define MG_ScopeAttr	0x0010		// Scope attribute.

#define MG_Global	2		// Index of "global" table entry.
#define MG_Hidden	4		// Index of "hidden" table entry.

static struct attrinfo {
	char *name;		// Attribute name.
	char *abbr;		// Abbreviated name (for small screens).
	short pindex;		// Name index of interactive prompt letter.
	ushort cflags;		// Control flags.
	ushort mflags;		// Mode flags.
	} ai[] = {		// MdEnabled indicates that associated flag is to be set when value is "true".
		{"buffer","buf",0,MG_Bool | MG_ScopeAttr | MG_ModeAttr,MdGlobal},
		{"description","desc",0,MG_ModeAttr | MG_GrpAttr,0},
		{"global","glb",0,MG_Bool | MG_ScopeAttr | MG_ModeAttr,MdGlobal | MdEnabled},
		{"group","grp",1,MG_Nil | MG_ModeAttr,0},
		{"hidden","hid",0,MG_Bool | MG_ModeAttr,MdHidden | MdEnabled},
		{"modes","modes",0,MG_Nil | MG_GrpAttr,0},
		{"visible","viz",0,MG_Bool | MG_ModeAttr,MdHidden},
		{NULL,NULL,-1,0,0}};

// Check if a built-in mode is set, depending on its scope, and return Boolean result.  If buf is NULL and needs to be
// accessed, check current buffer.
bool modeset(ushort id,Buffer *buf) {
	ModeSpec *mspec = mi.cache[id];
	if(mspec->ms_flags & MdGlobal)
		return mspec->ms_flags & MdEnabled;
	if(buf == NULL)
		buf = si.curbuf;
	return bmsrch1(buf,mspec);
	}

// Set a built-in global mode and flag mode line update if needed.
void gmset(ModeSpec *mspec) {

	if(!(mspec->ms_flags & MdEnabled)) {
		mspec->ms_flags |= MdEnabled;
		if((mspec->ms_flags & (MdHidden | MdInLine)) != MdHidden)		// If mode line update needed...
			wnextis(NULL)->w_flags |= WFMode;			// update mode line of bottom window.
		}
	}

// Clear a built-in global mode and flag mode line update if needed.
void gmclear(ModeSpec *mspec) {

	if(mspec->ms_flags & MdEnabled) {
		mspec->ms_flags &= ~MdEnabled;
		if((mspec->ms_flags & (MdHidden | MdInLine)) != MdHidden)		// If mode line update needed...
			wnextis(NULL)->w_flags |= WFMode;			// update mode line of bottom window.
		}
	}

// Set new description in mode or group record.
static int setdesc(char *newdesc,char **desc) {

	if(newdesc != NULL) {
		if(*desc != NULL)
			free((void *) *desc);
		if((*desc = (char *) malloc(strlen(newdesc) + 1)) == NULL)
			return rcset(Panic,0,text94,"setdesc");
				// "%s(): Out of memory!"
		strcpy(*desc,newdesc);
		}
	return rc.status;
	}

// binsearch() helper function for returning a mode name, given table (array) and index.
static char *modename(void *table,ssize_t i) {

	return msptr(((Datum **) table)[i])->ms_name;
	}

// Search the mode table for given name and return pointer to ModeSpec object if found; otherwise, NULL.  In either case, set
// *index to target array index if index not NULL.
ModeSpec *mdsrch(char *name,ssize_t *index) {
	ssize_t i = 0;
	bool found = (mi.modetab.a_used == 0) ? false : binsearch(name,(void *) mi.modetab.a_elp,mi.modetab.a_used,strcasecmp,
	 modename,&i);
	if(index != NULL)
		*index = i;
	return found ? msptr(mi.modetab.a_elp[i]) : NULL;
	}

// Check a proposed mode or group name for length and valid (printable) characters and return Boolean result.  It is assumed
// that name is not null.
static bool validMG(char *name,ushort type) {

	if(strlen(name) > MaxMGName) {
		(void) rcset(Failure,0,text280,type == MG_GrpAttr ? text388 : text387,MaxMGName);
			// "%s name cannot be null or exceed %d characters","Group","Mode"
		return false;
		}
	char *str = name;
	do {
		if(*str <= ' ' || *str > '~') {
			(void) rcset(Failure,0,text397,type == MG_GrpAttr ? text390 : text389,Literal4,name);
					// "Invalid %s %s '%s'"		"group"		"mode","name"
			return false;
			}
		} while(*++str != '\0');
	return true;
	}

// Create a user mode and, if mspecp not NULL, set *mspecp to ModeSpec object.  Return status.
int mdcreate(char *name,ssize_t index,char *desc,ushort flags,ModeSpec **mspecp) {
	ModeSpec *mspec;
	Datum *datum;

	// Valid name?
	if(!validMG(name,MG_ModeAttr))
		return rc.status;

	// All is well... create a mode record.
	if((mspec = (ModeSpec *) malloc(sizeof(ModeSpec) + strlen(name))) == NULL)
		return rcset(Panic,0,text94,"mdcreate");
			// "%s(): Out of memory!"

	// Set the attributes.
	mspec->ms_group = NULL;
	mspec->ms_flags = flags;
	strcpy(mspec->ms_name,name);
	mspec->ms_desc = NULL;
	if(setdesc(desc,&mspec->ms_desc) != Success)
		return rc.status;

	// Set pointer to object if requested.
	if(mspecp != NULL)
		*mspecp = mspec;

	// Insert record into mode table array and return.
	if(dnew(&datum) != 0)
		goto LibFail;
	dsetblobref((void *) mspec,0,datum);
	if(ainsert(&mi.modetab,index,datum,false) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Delete a user mode and return status.
static int mddelete(ModeSpec *mspec,ssize_t index) {
	Datum *datum;

	// Built-in mode?
	if(!(mspec->ms_flags & MdUser))
		return rcset(Failure,0,text396,text389,mspec->ms_name);
			// "Cannot delete built-in %s '%s'","mode"

	// No, user mode.  Check usage.
	if(mspec->ms_flags & MdGlobal) {				// Global mode?
		if(mspec->ms_flags & MdEnabled)			// Yes, enabled?
			wnextis(NULL)->w_flags |= WFMode;	// Yes, update mode line of bottom window.
		}
	else
		// Buffer mode.  Disable it in all buffers (if any).
		bmclearall(mspec);
	if(mspec->ms_group != NULL)				// Is mode a group member?
		--mspec->ms_group->mg_usect;			// Yes, decrement use count.

	// Usage check completed... nuke the mode.
	if(mspec->ms_desc != NULL)				// If mode description present...
		free((void *) mspec->ms_desc);			// free it.
	datum = adelete(&mi.modetab,index);			// Free the array element...
	free((void *) msptr(datum));				// the ModeSpec object...
	ddelete(datum);						// and the Datum object.

	return rcset(Success,0,"%s %s",text387,text10);
				// "Mode","deleted"
	}

// Clear all global modes.  Return true if any mode was enabled; otherwise, false.
static bool gclear(void) {
	Array *ary = &mi.modetab;
	Datum *el;
	ModeSpec *mspec;
	bool modeWasChanged = false;

	while((el = aeach(&ary)) != NULL) {
		mspec = msptr(el);
		if((mspec->ms_flags & (MdGlobal | MdEnabled)) == (MdGlobal | MdEnabled)) {
			gmclear(mspec);
			modeWasChanged = true;
			}
		}
	return modeWasChanged;
	}

// Find a mode group by name and return Boolean result.
// If group is found:
//	If slot not NULL, *slot is set to prior node.
//	If grp not NULL, *grp is set to ModeGrp object.
// If group is not found:
//	If slot not NULL, *slot is set to list-insertion slot.
bool mgsrch(char *name,ModeGrp **slot,ModeGrp **grp) {
	int result;
	bool rval;

	// Scan the mode group list.
	ModeGrp *mgp1 = NULL;
	ModeGrp *mgp2 = mi.ghead;
	while(mgp2 != NULL) {
		if((result = strcasecmp(mgp2->mg_name,name)) == 0) {

			// Found it.
			if(grp != NULL)
				*grp = mgp2;
			rval = true;
			goto Retn;
			}
		if(result > 0)
			break;
		mgp1 = mgp2;
		mgp2 = mgp2->mg_next;
		}

	// Group not found.
	rval = false;
Retn:
	if(slot != NULL)
		*slot = mgp1;
	return rval;
	}

// Create a mode group and, if result not NULL, set *result to new object.  prior points to prior node in linked list.  Return
// status.
int mgcreate(char *name,ModeGrp *prior,char *desc,ModeGrp **result) {
	ModeGrp *mgrp;

	// Valid name?
	if(!validMG(name,MG_GrpAttr))
		return rc.status;

	// All is well... create a mode group record.
	if((mgrp = (ModeGrp *) malloc(sizeof(ModeGrp) + strlen(name))) == NULL)
		return rcset(Panic,0,text94,"mgcreate");
			// "%s(): Out of memory!"

	// Find the place in the list to insert this group.
	if(prior == NULL) {

		// Insert at the beginning.
		mgrp->mg_next = mi.ghead;
		mi.ghead = mgrp;
		}
	else {
		// Insert after prior.
		mgrp->mg_next = prior->mg_next;
		prior->mg_next = mgrp;
		}

	// Set the object attributes.
	mgrp->mg_flags = MdUser;
	mgrp->mg_usect = 0;
	strcpy(mgrp->mg_name,name);
	mgrp->mg_desc = NULL;
	(void) setdesc(desc,&mgrp->mg_desc);

	// Set pointer to object if requested and return.
	if(result != NULL)
		*result = mgrp;
	return rc.status;
	}

// Remove all members of a mode group, if any.
static void mgclear(ModeGrp *mgrp) {

	if(mgrp->mg_usect > 0) {
		ModeSpec *mspec;
		Datum *el;
		Array *ary = &mi.modetab;
		while((el = aeach(&ary)) != NULL) {
			mspec = msptr(el);
			if(mspec->ms_group == mgrp) {
				mspec->ms_group = NULL;
				if(--mgrp->mg_usect == 0)
					return;
				}
			}
		}
	}

// Delete a mode group and return status.
static int mgdelete(ModeGrp *mgp1,ModeGrp *mgp2) {

	// Built-in group?
	if(!(mgp2->mg_flags & MdUser))
		return rcset(Failure,0,text396,text390,mgp2->mg_name);
			// "Cannot delete built-in %s '%s'","group"

	// It's a go.  Clear the group, delete it from the group list, and free the storage.
	mgclear(mgp2);
	if(mgp1 == NULL)
		mi.ghead = mgp2->mg_next;
	else
		mgp1->mg_next = mgp2->mg_next;
	if(mgp2->mg_desc != NULL)
		free((void *) mgp2->mg_desc);
	free((void *) mgp2);

	return rcset(Success,0,"%s %s",text388,text10);
				// "Group","deleted"
	}

// Remove a mode's group membership, if any.
static void membclear(ModeSpec *mspec) {

	if(mspec->ms_group != NULL) {
		--mspec->ms_group->mg_usect;
		mspec->ms_group = NULL;
		}
	}

// Make a mode a member of a group.  Don't allow if group already has members and mode's scope doesn't match members'.  Also, if
// group already has an active member, disable mode being added if enabled (because modes are mutually exclusive).  Return
// status.
static int membset(ModeSpec *mspec,ModeGrp *mgrp) {

	// Check if scope mismatch or any mode in group is enabled.
	if(mgrp->mg_usect > 0) {
		ushort gtype = USHRT_MAX;
		ModeSpec *msp2;
		Datum *el;
		Array *ary = &mi.modetab;
		bool activeFound = false;
		while((el = aeach(&ary)) != NULL) {
			msp2 = msptr(el);
			if(msp2->ms_group == mgrp) {				// Mode in group?
				if(msp2 == mspec)					// Yes.  Is it the one to be added?
					return rc.status;			// Yes, nothing to do.
				if((gtype = msp2->ms_flags & MdGlobal)) {	// No.  Remember type and "active" status.
					if(msp2->ms_flags & MdEnabled)
						activeFound = true;
					}
				else if(bmsrch1(si.curbuf,msp2))
					activeFound = true;
				}
			}
		if(gtype != USHRT_MAX && (mspec->ms_flags & MdGlobal) != gtype)
			return rcset(Failure,0,text398,text389,mspec->ms_name,text390,mgrp->mg_name);
				// "Scope of %s '%s' must match %s '%s'","mode","group"

		// Disable mode if an enabled mode was found in group.
		if(activeFound)
			(mspec->ms_flags & MdGlobal) ? gmclear(mspec) : bmsrch(si.curbuf,true,1,mspec);
		}

	// Group is empty or scopes match... add mode.
	membclear(mspec);
	mspec->ms_group = mgrp;
	++mgrp->mg_usect;

	return rc.status;
	}

// Ensure mutually-exclusive modes (those in the same group) are not set.
static void chkgrp(ModeSpec *mspec,Buffer *buf) {

	if(mspec->ms_group != NULL && mspec->ms_group->mg_usect > 1) {
		ModeSpec *msp2;
		Datum *el;
		Array *ary = &mi.modetab;
		while((el = aeach(&ary)) != NULL) {
			if((msp2 = msptr(el)) != mspec && msp2->ms_group == mspec->ms_group) {
				if(msp2->ms_flags & MdGlobal)
					gmclear(msp2);
				else
					bmsrch(buf,true,1,msp2);
				}
			}			
		}
	}

// binsearch() helper function for returning a mode or group attribute keyword, given table (array) and index.
static char *ainame(void *table,ssize_t i) {

	return ((struct attrinfo *) table)[i].name;
	}

// Check if mode scope change is valid.  Return true if so; otherwise, set an error and return false.
static bool scopeMatch(ModeSpec *mspec) {

	// If mode not in a group or only member, scope-change is allowed.
	if(mspec->ms_group == NULL || mspec->ms_group->mg_usect == 1)
		return true;

	// No go... mode is in a group with other members.
	(void) rcset(Failure,0,text398,text389,mspec->ms_name,text390,mspec->ms_group->mg_name);
		// "Scope of %s '%s' must match %s '%s'","mode","group"
	return false;
	}

// Process a mode or group attribute.  Return status.
static int doattr(Datum *attr,ushort type,void *mp) {
	ModeSpec *mspec;
	ModeGrp *mgrp;
	struct attrinfo *aip;
	ssize_t i;
	ushort *flags;
	char **desc,*str0,*str1;
	char wkbuf[strlen(attr->d_str) + 1];

	// Make copy of attribute string so unmodified original can be used for error reporting.
	strcpy(str0 = wkbuf,attr->d_str);

	if(type == MG_ModeAttr) {					// Set control variables.
		mspec = (ModeSpec *) mp;
		flags = &mspec->ms_flags;
		desc = &mspec->ms_desc;
		}
	else {
		mgrp = (ModeGrp *) mp;
		flags = &mgrp->mg_flags;
		desc = &mgrp->mg_desc;
		}

	str1 = strparse(&str0,':');					// Parse keyword from attribute string.
	if(str1 == NULL || *str1 == '\0')
		goto ErrRtn;
	if(!binsearch(str1,(void *) ai,sizeof(ai) /			// Keyword found.  Look it up and check if right type.
	 sizeof(struct attrinfo) - 1,strcasecmp,ainame,&i))
		goto ErrRtn;						// Not in attribute table.
	aip = ai + i;
	if(!(aip->cflags & type))
		goto ErrRtn;						// Wrong type.

	str1 = strparse(&str0,'\0');					// Valid keyword and type.  Get value.
	if(str1 == NULL || *str1 == '\0')
		goto ErrRtn;
	if(aip->cflags & (MG_Nil | MG_Bool)) {				// If parameter accepts a nil or Boolean value...
		bool haveTrue;

		if(strcasecmp(str1,viz_true) == 0) {			// Check if true, false, or nil.
			haveTrue = true;
			goto DoBool;
			}
		if(strcasecmp(str1,viz_false) == 0) {
			haveTrue = false;
DoBool:
			if(!(aip->cflags & MG_Bool))			// Have Boolean value... allowed?
				goto ErrRtn;				// No, error.
			if(aip->cflags & MG_ScopeAttr) {		// Yes, setting mode's scope?
				if(mspec->ms_flags & MdLocked)		// Yes, error if mode is scope-locked.
					return rcset(Failure,0,text399,text387,mspec->ms_name);
						// "%s '%s' has permanent scope","Mode"
				if(i == MG_Global) {			// Determine ramifications and make changes.
					if(haveTrue)			// "global: true"
						goto BufToGlobal;
					goto GlobalToBuf;		// "global: false"
					}
				else if(haveTrue) {			// "buffer: true"
GlobalToBuf:								// Do global -> buffer.
					if(!(*flags & MdGlobal) || !scopeMatch(mspec))
						return rc.status;	// Not global mode or scope mismatch.
					haveTrue = (*flags & MdEnabled);
					*flags &= ~(MdEnabled | MdGlobal);
					if(haveTrue)			// If mode was enabled...
						(void) bmset(si.curbuf,mspec,false,NULL);	// set it in current buffer.
					}
				else {					// "buffer: false"
BufToGlobal:								// Do buffer -> global.
					if((*flags & MdGlobal) || !scopeMatch(mspec))
						return rc.status;	// Not buffer mode or scope mismatch.
					haveTrue = bmsrch1(si.curbuf,mspec);	// If mode was enabled in current buffer...
					bmclearall(mspec);
					*flags |= haveTrue ? (MdGlobal | MdEnabled) : MdGlobal;	// set it globally.
					}
				goto BFUpdate;
				}
			else {
				if(i == MG_Hidden) {
					if(haveTrue)			// "hidden: true"
						goto Hide;
					goto Expose;			// "hidden: false"
					}
				else if(haveTrue) {			// "visible: true"
Expose:
					if(*flags & MdHidden) {
						*flags &= ~MdHidden;
						goto FUpdate;
						}
					}
				else {					// "visible: false"
Hide:
					if(!(*flags & MdHidden)) {
						*flags |= MdHidden;
FUpdate:
						if(*flags & MdGlobal)
							wnextis(NULL)->w_flags |= WFMode;
						else
BFUpdate:
							supd_wflags(NULL,WFMode);
						}
					}
				}
			}
		else if(strcasecmp(str1,viz_nil) == 0) {
			if(!(aip->cflags & MG_Nil))
				goto ErrRtn;
			if(aip->cflags & MG_ModeAttr)			// "group: nil"
				membclear(mspec);
			else						// "modes: nil"
				mgclear(mgrp);
			}
		else {
			if(aip->cflags & MG_Nil)
				goto ProcString;
			goto ErrRtn;
			}
		}
	else {
ProcString:
		// str1 points to null-terminated value.  Process it.
		if((aip->cflags & (MG_ModeAttr | MG_GrpAttr)) == (MG_ModeAttr | MG_GrpAttr))	// "description: ..."
			(void) setdesc(str1,desc);
		else if(aip->cflags & MG_ModeAttr) {			// "group: ..."
			if(!mgsrch(str1,NULL,&mgrp))
				return rcset(Failure,0,text395,text390,str1);
					// "No such %s '%s'","group"
			(void) membset(mspec,mgrp);
			}
		else {							// "modes: ..."
			str0 = str1;
			do {
				str1 = strparse(&str0,',');		// Get next mode name.
				if(str1 == NULL || *str1 == '\0')
					goto ErrRtn;
				if((mspec = mdsrch(str1,NULL)) == NULL)
					return rcset(Failure,0,text395,text389,str1);
						// "No such %s '%s'","mode"
				if(membset(mspec,mgrp) != Success)
					break;
				} while(str0 != NULL);
			}
		}

	return rc.status;
ErrRtn:
	return rcset(Failure,0,text397,type == MG_GrpAttr ? text390 : text389,text391,attr->d_str);
			// "Invalid %s %s '%s'"		"group"		"mode","setting"
	}

// Prompt for mode or group attribute and value, convert to string (script) form, and save in rval.  Return status.
static int getattr(Datum *rval,ushort type) {
	DStrFab prompt,result;
	Datum *datum;
	struct attrinfo *aip;
	char *attrname;
	char *lead = " (";

	// Build prompt for attribute to set or change.
	if(dnewtrk(&datum) != 0 || dopenwith(&result,rval,SFClear) != 0 || dopenwith(&prompt,datum,SFClear) != 0 ||
	 dputc(upcase[(int) text186[0]],&prompt) != 0 || dputs(text186 + 1,&prompt) != 0)
					// "attribute"
		goto LibFail;
	aip = ai;
	do {
		if(aip->cflags & type) {
			attrname = (term.t_ncol >= 80) ? aip->name : aip->abbr;
			if(dputs(lead,&prompt) != 0 || (aip->pindex == 1 && dputc(attrname[0],&prompt) != 0) ||
			 dputf(&prompt,"%c%c%c%c%c%c%c%s",AttrSpecBegin,AttrULOn,AttrSpecBegin,AttrBoldOn,
			 (int) attrname[aip->pindex],AttrSpecBegin,AttrAllOff,attrname + aip->pindex + 1) != 0)
				goto LibFail;
			lead = ", ";
			}
		} while((++aip)->name != NULL);
	if(dputc(')',&prompt) != 0 || dclose(&prompt,sf_string) != 0)
		goto LibFail;

	// Get attribute from user and validate it.
	dclear(rval);
	if(terminp(datum,datum->d_str,ArgNotNull1 | ArgNil1,Term_LongPrmt | Term_Attr | Term_OneChar,NULL) != Success ||
	 datum->d_type == dat_nil)
		return rc.status;
	aip = ai;
	do {
		attrname = (term.t_ncol >= 80) ? aip->name : aip->abbr;
		if(attrname[aip->pindex] == datum->u.d_int)
			goto Found;
		} while((++aip)->name != NULL);
	return rcset(Failure,0,text397,type == MG_GrpAttr ? text390 : text389,text186,vizc(datum->u.d_int,VBaseDef));
			// "Invalid %s %s '%s'"			"group","mode","attribute"
Found:
	// Return result if Boolean attribute; otherwise, get string value.
	if(dputf(&result,"%s: ",aip->name) != 0)
		goto LibFail;
	if(aip->cflags & MG_Bool) {
		if(dputs(viz_true,&result) != 0)
			goto LibFail;
		}
	else {
		if(terminp(datum,text53,(aip->cflags & MG_Nil) ? ArgNotNull1 | ArgNil1 : ArgNotNull1,0,NULL) != Success
				// "Value"
		 || dtosf(&result,datum,NULL,CvtShowNil) != Success)
			return rc.status;
		}
	if(dclose(&result,sf_string) != 0)
LibFail:
		(void) librcset(Failure);
	return rc.status;
	}

// Create prompt for editMode or editModeGroup command.
static int emodeprmt(ushort type,int n,Datum *datum) {
	DStrFab prompt;

	return (dopenwith(&prompt,datum,SFClear) != 0 ||
	 dputf(&prompt,"%s %s",n == INT_MIN ? text402 : n <= 0 ? text26 : text403,text389) != 0 ||
					// "Edit"		"Delete","Create or edit","mode"
	 (type == MG_GrpAttr && dputf(&prompt," %s",text390) != 0) || dclose(&prompt,sf_string) != 0) ?
						// "group"
	 librcset(Failure) : rc.status;
	}

// Edit a user mode or group.  Return status.
static int editMG(Datum *rval,int n,Datum **argv,ushort type) {
	void *mp;
	Datum *datum;
	char *name;
	bool created = false;

	if(dnewtrk(&datum) != 0)
		return librcset(Failure);

	// Get mode or group name and validate it.
	if(si.opflags & OpScript)
		name = argv[0]->d_str;
	else {
		// Build prompt.
		if(emodeprmt(type,n,datum) != Success)
			return rc.status;

		// Get mode or group name from user.
		if(terminp(datum,datum->d_str,ArgNil1 | ArgNotNull1,type == MG_ModeAttr ? Term_C_GMode | Term_C_BMode : 0,NULL)
		 != Success || datum->d_type == dat_nil)
			return rc.status;
		name = datum->d_str;
		}
	if(type == MG_ModeAttr) {
		ssize_t index;
		ModeSpec *mspec;

		if((mspec = mdsrch(name,&index)) == NULL) {			// Mode not found.  Create it?
			if(n <= 0)						// No, error.
				return rcset(Failure,0,text395,text389,name);
						// "No such %s '%s'","mode"
			if(mdcreate(name,index,NULL,MdUser,&mspec) != Success)	// Yes, create it.
				return rc.status;
			created = true;
			}
		else if(n <= 0 && n != INT_MIN)					// Mode found.  Delete it?
			return mddelete(mspec,index);				// Yes.
		mp = (void *) mspec;
		}
	else {
		ModeGrp *mgp1,*mgp2;

		if(!mgsrch(name,&mgp1,&mgp2)) {					// Group not found.  Create it?
			if(n <= 0)						// No, error.
				return rcset(Failure,0,text395,text390,name);
						// "No such %s '%s'","group"
			if(mgcreate(name,mgp1,NULL,&mgp2) != Success)		// Yes, create it.
				return rc.status;
			created = true;
			}
		else if(n <= 0 && n != INT_MIN)					// Group found.  Delete it?
			return mgdelete(mgp1,mgp2);				// Yes.
		mp = (void *) mgp2;
		}

	// If interactive mode, get an attribute; otherwise, loop through attribute arguments and process them.
	if(!(si.opflags & OpScript)) {
		if(getattr(datum,type) == Success && datum->d_type != dat_nil && doattr(datum,type,mp) == Success)
			rcset(Success,0,"%s %s",type == MG_ModeAttr ? text387 : text388,created ? text392 : text394);
								// "Mode","Group","created","attribute changed"
		}
	else {
		// Process any remaining arguments.
		while(havesym(s_comma,false)) {
			if(funcarg(datum,ArgNotNull1) != Success || doattr(datum,type,mp) != Success)
				break;
			}
		}

	return rc.status;
	}

// Edit a user mode.  Return status.
int editMode(Datum *rval,int n,Datum **argv) {

	return editMG(rval,n,argv,MG_ModeAttr);
	}

// Edit a user mode group.  Return status.
int editModeGroup(Datum *rval,int n,Datum **argv) {

	return editMG(rval,n,argv,MG_GrpAttr);
	}

// Retrieve enabled global (buf is NULL) or buffer (buf not NULL) modes and save as an array of keywords in *rval.  Return
// status.
int getmodes(Datum *rval,Buffer *buf) {
	Array *ary0;

	if(mkarray(rval,&ary0) == Success) {
		Datum dat;
		dinit(&dat);
		if(buf == NULL) {
			Datum *el;
			ModeSpec *mspec;
			Array *ary = &mi.modetab;
			while((el = aeach(&ary)) != NULL) {
				mspec = msptr(el);
				if((mspec->ms_flags & (MdGlobal | MdEnabled)) == (MdGlobal | MdEnabled))
					if(dsetstr(mspec->ms_name,&dat) != 0 || apush(ary0,&dat,true) != 0)
						goto LibFail;
				}
			}
		else {
			BufMode *bmp;

			for(bmp = buf->b_modes; bmp != NULL; bmp = bmp->bm_next)
				if(dsetstr(bmp->bm_mode->ms_name,&dat) != 0 || apush(ary0,&dat,true) != 0)
					goto LibFail;
			}
		dclear(&dat);
		}
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Check if a partial (or full) mode name is unique.  If "bmode" is true, consider buffer modes only; otherwise, global modes.
// Set *mspecp to mode table entry if true; otherwise, NULL.  Return status.
static int modecmp(char *name,bool bmode,ModeSpec **mspecp) {
	Datum *el;
	Array *ary = &mi.modetab;
	ModeSpec *msp0 = NULL,*mspec;
	size_t len = strlen(name);
	while((el = aeach(&ary)) != NULL) {
		mspec = msptr(el);

		// Skip if wrong type of mode.
		if(bmode && (mspec->ms_flags & MdGlobal))
			continue;

		if(strncasecmp(name,mspec->ms_name,len) == 0) {

			// Exact match?
			if(mspec->ms_name[len] == '\0') {
				msp0 = mspec;
				goto Retn;
				}

			// Error if not first match.
			if(msp0 != NULL)
				goto ERetn;
			msp0 = mspec;
			}
		}

	if(msp0 == NULL)
ERetn:
		return rcset(Failure,0,text66,name);
			// "Unknown or ambiguous mode '%s'"
Retn:
	*mspecp = msp0;
	return rc.status;
	}

// Change a mode, given name, action (< 0: clear, 0: toggle, > 0: set), Buffer pointer if buffer mode (otherwise, NULL),
// optional "former state" pointer, and optional indirect ModeSpec pointer which is set to entry in mode table if mode was
// changed; otherwise, NULL.  If action < -1 or > 2, partial mode names are allowed.  Return status.
int chgmode1(char *name,int action,Buffer *buf,long *formerStatep,ModeSpec **result) {
	ModeSpec *mspec;
	bool modeWasSet,modeWasChanged;

	// Check if name in mode table.
	if(action < -1 || action > 2) {
		if(modecmp(name,buf != NULL,&mspec) != Success)
			return rc.status;
		}
	else if((mspec = mdsrch(name,NULL)) == NULL)
		return rcset(Failure,0,text395,text389,name);
				// "No such %s '%s'","mode"

	// Match found... validate mode and process it.  Error if (1), wrong type; or (2), ASave mode, $autoSave is zero, and
	// setting mode or toggling it and ASave not currently set.
	if(((mspec->ms_flags & MdGlobal) != 0) != (buf == NULL))
		return rcset(Failure,0,text66,name);
				// "Unknown or ambiguous mode '%s'"
	if(mspec == mi.cache[MdIdxASave] && si.gasave == 0 &&
	 (action > 0 || (action == 0 && !(mspec->ms_flags & MdEnabled))))
		return rcset(Failure,RCNoFormat,text35);
			// "$autoSave not set"

	if(buf != NULL) {

		// Change buffer mode.
		if(action <= 0) {
			modeWasSet = bmsrch(buf,true,1,mspec);
			if(action == 0) {
				if(!modeWasSet && bmset(buf,mspec,false,NULL) != Success)
					return rc.status;
				modeWasChanged = true;
				}
			else
				modeWasChanged = modeWasSet;
			}
		else {
			if(bmset(buf,mspec,false,&modeWasSet) != Success)
				return rc.status;
			modeWasChanged = !modeWasSet;
			}
		}
	else {
		// Change global mode.
		modeWasSet = mspec->ms_flags & MdEnabled;
		if(action < 0 || (action == 0 && modeWasSet)) {
			modeWasChanged = modeWasSet;
			gmclear(mspec);
			}
		else {
			modeWasChanged = !modeWasSet;
			gmset(mspec);
			}
		}
	if(formerStatep != NULL)
		*formerStatep = modeWasSet ? 1L : -1L;

	// Ensure mutually-exclusive modes (those in the same group) are not set.
	if(modeWasChanged && !modeWasSet)
		chkgrp(mspec,buf);

	if(result != NULL)
		*result = modeWasChanged ? mspec : NULL;
	return rc.status;
	}

// Change a mode, given result pointer, action (clear: n < 0, toggle: n == 0 or default, set: n == 1, or clear all and set:
// n > 1), and argument vector.  Set rval to the former state (-1 or 1) of the last mode changed.  Return status.
int chgMode(Datum *rval,int n,Datum **argv) {
	Datum *datum,*arg,*oldmodes;
	ModeSpec *mspec;
	Buffer *buf = NULL;
	long formerState = 0;			// Usually -1 or 1.
	int action = (n == INT_MIN) ? 0 : (n < 0) ? -1 : (n > 1) ? 2 : n;
	bool modeWasChanged = false;
	bool global;

	// Get ready.
	if(dnewtrk(&datum) != 0 || dnewtrk(&oldmodes) != 0)
		goto LibFail;

	// If interactive mode, build proper prompt string (e.g., "Toggle mode: ") and get buffer, if needed.
	if(!(si.opflags & OpScript)) {
		DStrFab prompt;

		// Build prompt.
		if(dopenwith(&prompt,datum,SFClear) != 0 ||
		 dputs(action < 0 ? text65 : action == 0 ? text231 : action == 1 ? text64 : text296,&prompt) != 0 ||
				// "Clear","Toggle","Set","Clear all and set"
		 dputs(text63,&prompt) != 0 || dclose(&prompt,sf_string) != 0)
				// " mode"
			goto LibFail;

		// Get mode name from user.  If action > 1 (clear all and set) and nothing entered, just return because there
		// is no way to know whether to clear buffer modes or global modes.
		if(terminp(datum,datum->d_str,ArgNil1,Term_C_GMode | Term_C_BMode,NULL) != Success || datum->d_type == dat_nil)
			return rc.status;

		// Valid?
		if((mspec = mdsrch(datum->d_str,NULL)) == NULL)
			return rcset(Failure,0,text395,text389,datum->d_str);
					// "No such %s '%s'","mode"

		// Get buffer, if needed.
		if(mspec->ms_flags & MdGlobal)
			global = true;
		else {
			if(n == INT_MIN)
				buf = si.curbuf;
			else {
				buf = bdefault();
				if(bcomplete(rval,text229 + 2,buf != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL)
				 != Success || buf == NULL)
						// ", in"
					return rc.status;
				}
			global = false;
			}
		}
	else {
		// Script mode.  Check arguments.
		if((!(global = argv[0]->d_type == dat_nil) && (buf = getbufarg(argv[0])) == NULL) ||
		 (n <= 1 && valarg(argv[1],ArgNotNull1 | ArgArray1 | ArgMay) != Success))
			return rc.status;
		}

	// Have buffer (if applicable) and, if interactive, one mode name in datum.  Save current modes so they can be passed to
	// mode hook, if set.
	if(getmodes(oldmodes,buf) != 0)
		return rc.status;

	// Clear all modes initially, if applicable.
	if(action > 1)
		modeWasChanged = global ? gclear() : bmclear(buf);

	// Process mode argument(s).
	if(!(si.opflags & OpScript)) {
		arg = datum;
		goto DoMode;
		}
	else {
		char *kwlist;
		Datum **elp = NULL;
		ArraySize elct;
		int status;

		// Script mode: get mode keywords.
		while((status = nextarg(&arg,argv + 1,datum,&kwlist,&elp,&elct)) == Success) {
DoMode:
			if(chgmode1(arg->d_str,action,buf,&formerState,&mspec) != Success)
				return rc.status;

			// Do special processing for specific global modes that changed.
			if(mspec != NULL) {
				if(!modeWasChanged)
					modeWasChanged = true;
				if(mspec == mi.cache[MdIdxHScrl]) {

					// If horizontal scrolling is now disabled, unshift any shifted windows on
					// current screen; otherwise, set shift of current window to line shift.
					if(!(mspec->ms_flags & MdEnabled)) {
						EWindow *win = si.whead;
						do {
							if(win->w_face.wf_firstcol > 0) {
								win->w_face.wf_firstcol = 0;
								win->w_flags |= (WFHard | WFMode);
								}
							} while((win = win->w_next) != NULL);
						}
					else if(si.curscr->s_firstcol > 0) {
						si.curwin->w_face.wf_firstcol = si.curscr->s_firstcol;
						si.curwin->w_flags |= (WFHard | WFMode);
						}
					si.curscr->s_firstcol = 0;
					}
				}
			if(!(si.opflags & OpScript))
				break;
			}
		}

	// Display new mode line.
	if(!(si.opflags & OpScript) && mlerase() != Success)
		return rc.status;

	// Return former state of last mode that was changed.
	dsetint(formerState,rval);

	// Run mode-change hook if any mode was changed and either: (1), a global mode; or (2), target buffer is not hidden and
	// not a macro.  Hook arguments are either: (1), nil,oldmodes (if global mode was changed); or (2), buffer name,oldmodes
	// (if buffer mode was changed).
	if(modeWasChanged && (buf == NULL || !(buf->b_flags & (BFHidden | BFMacro)))) {
		if(global)
			dclear(datum);
		else if(dsetstr(buf->b_bname,datum) != 0)
			goto LibFail;
		(void) exechook(NULL,INT_MIN,hooktab + HkMode,0xA2,datum,oldmodes);
		}

	return rc.status;
LibFail:
	return librcset(Failure);
	}
