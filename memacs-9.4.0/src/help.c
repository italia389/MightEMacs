// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// help.c	Help and info functions for MightEMacs.
//
// This file contains routines for all the informational commands and functions.

#include "os.h"
#include <sys/stat.h>
#include <sys/utsname.h>
#include "pllib.h"
#include "std.h"
#include "bind.h"
#include "exec.h"
#include "cmd.h"
#include "search.h"
#include "var.h"

// System and session information table.
typedef struct {
	char *keyword;
	char *value;
	enum cfid id;
	} InfoTab;

#ifndef OSName
// Get OS name for getInfo function and return it in rval.  Return status.
static int getOS(Datum *rval) {
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

	return dsetstr(name,rval) != 0 ? librcset(Failure) : rc.status;
	}
#endif

// Store buffer information into given, pre-allocated array.  Return status.
static int binfo(Array *ary,Buffer *buf,bool verbose) {
	long bytect,linect;
	Datum **elp = ary->a_elp;

	// Store buffer name and filename.
	if(dsetstr(buf->b_bname,*elp++) != 0)
		goto LibFail;
	if(buf->b_fname != NULL && dsetstr(buf->b_fname,*elp) != 0)
		goto LibFail;
	++elp;

	// Store buffer size in bytes and lines.
	bytect = buflength(buf,&linect);
	dsetint(bytect,*elp++);
	dsetint(linect,*elp++);

	// Store buffer attributes and modes if requested.
	if(verbose) {
		Array *ary1;
		Datum *el;
		Option *opt;

		if(mkarray(*elp++,&ary1) != Success)
			return rc.status;

		// Get buffer attributes.
		opt = battrinfo;
		do {
			if(buf->b_flags & opt->u.value)
				if((el = aget(ary1,ary1->a_used,true)) == NULL ||
				 dsetstr(*opt->keywd == '^' ? opt->keywd + 1 : opt->keywd,el) != 0)
					goto LibFail;
			} while((++opt)->keywd != NULL);

		// Get buffer modes.
		(void) getmodes(*elp,buf);
		}

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Build array of form [bufname], [bufname,filename,bytes,lines], or [bufname,filename,bytes,lines,[buf-attr,...],
// [buf-mode,...]] and return it in rval.  Return status.
int bufInfo(Datum *rval,int n,Datum **argv) {
	Array *ary0,*ary1,*ary;
	Datum *el;
	Datum *bname = argv[0];
	Buffer *buf;
	int count = 0;
	ushort bflags;
	ushort arySize = 4;
	ushort optflags = 0;
	static Option options[] = {
		{"^Hidden",NULL,0,BFHidden},
		{"^Macro",NULL,0,BFMacro},
		{"^Brief",NULL,0,0},
		{"^Verbose",NULL,0,0},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		0,text450,false,options};
			// "type keyword"

	// Get options and set buffer flags.
	if(mkarray(rval,&ary0) != Success)
		return rc.status;
	if(n != INT_MIN && parseopts(&ohdr,NULL,argv[1],&count) != Success)
		return rc.status;
	if(count > 0) {
		if((options[2].cflags & OptSelected) && (options[3].cflags & OptSelected))
			return rcset(Failure,0,text454,text451);
				// "Conflicting %ss","function option"
		optflags = getFlagOpts(options);
		if(options[2].cflags & OptSelected)
			arySize = 1;
		else if(options[3].cflags & OptSelected)
			arySize = 6;
		}

	// If single buffer requested, find it.
	if(bname->d_type != dat_nil) {
		if((buf = bsrch(bname->d_str,NULL)) == NULL)
			return rcset(Failure,0,text118,bname->d_str);
				// "No such buffer '%s'"
		count = 1;
		goto DoOneBuf;
		}

	// Cycle through buffers and make list in ary0.
	ary = &buftab;
	count = 0;
	while((el = aeach(&ary)) != NULL) {
		buf = bufptr(el);
DoOneBuf:
		// Select buffer if single buffer requested or both of the following are true:
		//  1. not hidden or "Hidden" option specified.
		//  2. not a macro or "Macro" option specified.
		bflags = (buf->b_flags & (BFHidden | BFMacro));
		if(count == 1 || !bflags ||
		 (bflags == (BFHidden | BFMacro) && (optflags & BFMacro)) ||
		 (bflags == BFHidden && (optflags & BFHidden))) {

			// Create array for this buffer if needed.
			if(count == 1) {
				// Single buffer.  Use ary0 and extend it to the correct size.
				if((el = aget(ary1 = ary0,arySize - 1,true)) == NULL)
					goto LibFail;
				}
			else {
				// Multiple buffers.  If not "Brief", create an array of the correct size.
				if((el = aget(ary0,ary0->a_used,true)) == NULL)
					goto LibFail;
				if(arySize > 1) {
					if((ary1 = anew(arySize,NULL)) == NULL)
						goto LibFail;
					if(awrap(el,ary1) != Success)
						return rc.status;
					}
				}

			// Store buffer information into array.
			if(arySize == 1) {
				if(dsetstr(buf->b_bname,el) != 0)
					goto LibFail;
				}
			else if(binfo(ary1,buf,arySize == 6) != Success)
				return rc.status;
			if(count == 1)
				break;
		 	}
		}
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Build array for getInfo or bufInfo function and return it in rval.  Return status.
static int getary(Datum *rval,int n,enum cfid id) {
	Array *ary0,*ary1,*ary;
	Datum *el,**elp;
	EScreen *scr;

	if(mkarray(rval,&ary0) != Success)
		goto Retn;
	switch(id) {
		case cf_showColors:;		// [[item-kw,[fg-color,bg-color]],...] or [colors,pairs]
			if(n == INT_MIN) {
				ItemColor *ctab,*ctabz;

				// Cycle through color items and make list.  For each item, create array in ary1 and push onto
				// ary0.  Also create color-pair array in ary and store in ary1 if applicable.
				ctabz = (ctab = term.itemColor) + sizeof(term.itemColor) / sizeof(ItemColor);
				do {
					if((ary1 = anew(2,NULL)) == NULL)
						goto LibFail;
					if(ctab->colors[0] < -1)
						dsetnil(ary1->a_elp[1]);
					else {
						if((ary = anew(2,NULL)) == NULL)
							goto LibFail;
						dsetint(ctab->colors[0],ary->a_elp[0]);
						dsetint(ctab->colors[1],ary->a_elp[1]);
						if(awrap(ary1->a_elp[1],ary) != Success)
							goto Retn;
						}
					if(dsetstr(ctab->name,ary1->a_elp[0]) != 0 ||
					 (el = aget(ary0,ary0->a_used,true)) == NULL)
						goto LibFail;
					if(awrap(el,ary1) != Success)
						goto Retn;
					} while(++ctab < ctabz);
				}
			else {
				// Create two-element array.
				if((el = aget(ary0,ary0->a_used,true)) == NULL)
					goto LibFail;
				dsetint(term.maxColor,el);
				if((el = aget(ary0,ary0->a_used,true)) == NULL)
					goto LibFail;
				dsetint(term.maxWorkPair,el);
				}
			break;
		case cf_showHooks:;		// [[hook-name,macro-name],...]
			HookRec *hrec = hooktab;

			// Cycle through hook table and make list.  For each hook, create array in ary1 and push onto ary0.
			do {
				if((ary1 = anew(2,NULL)) == NULL)
					goto LibFail;
				if(dsetstr(hrec->h_name,ary1->a_elp[0]) != 0 ||
				 (hrec->h_buf != NULL && dsetstr(hrec->h_buf->b_bname + 1,ary1->a_elp[1]) != 0) ||
				 (el = aget(ary0,ary0->a_used,true)) == NULL)
					goto LibFail;
				if(awrap(el,ary1) != Success)
					goto Retn;
				} while((++hrec)->h_name != NULL);
			break;
		case cf_showModes:;		// [[mode-name,group-name,user?,global?,hidden?,scope-lock?,active?],...]
			ModeSpec *mspec;
			static ushort modeflags[] = {MdUser,MdGlobal,MdHidden,MdLocked,0};
			ushort *mfp;

			// Cycle through mode table and make list.  For each mode, create array in ary1 and push onto ary0.
			ary = &mi.modetab;
			while((el = aeach(&ary)) != NULL) {
				mspec = msptr(el);
				if((ary1 = anew(7,NULL)) == NULL)
					goto LibFail;
				elp = ary1->a_elp;
				if(dsetstr(mspec->ms_name,*elp++) != 0 || (el = aget(ary0,ary0->a_used,true)) == NULL)
					goto LibFail;
				if(mspec->ms_group == NULL)
					++elp;
				else if(dsetstr(mspec->ms_group->mg_name,*elp++) != 0)
					goto LibFail;
				mfp = modeflags;
				do {
					dsetbool(mspec->ms_flags & *mfp++,*elp++);
					} while(*mfp != 0);
				dsetbool((mspec->ms_flags & MdGlobal) ? (mspec->ms_flags & MdEnabled) :
				 bmsrch1(si.curbuf,mspec),*elp++);
				if(awrap(el,ary1) != Success)
					goto Retn;
				}
			break;
		case cf_showScreens:;		// [[screen-num,wind-count,work-dir],...]

			// Cycle through screens and make list.  For each screen, create array in ary1 and push onto ary0.
			scr = si.shead;
			do {
				if((ary1 = anew(3,NULL)) == NULL)
					goto LibFail;
				elp = ary1->a_elp;
				dsetint((long) scr->s_num,*elp++);
				dsetint((long) wincount(scr,NULL),*elp++);
				if(dsetstr(scr->s_wkdir,*elp) != 0 || (el = aget(ary0,ary0->a_used,true)) == NULL)
					goto LibFail;
				if(awrap(el,ary1) != Success)
					goto Retn;
				} while((scr = scr->s_next) != NULL);
			break;
		default:;			// [[windNum,bufname],...] or [[screenNum,windNum,bufname],...]
			EWindow *win;
			long wnum;

			// Cycle through screens and make list.  For each window, create array in ary1 and push onto ary0.
			scr = si.shead;
			do {
				if(scr->s_num == si.curscr->s_num || n != INT_MIN) {

					// In all windows...
					wnum = 0;
					win = scr->s_whead;
					do {
						++wnum;
						if((ary1 = anew(n == INT_MIN ? 2 : 3,NULL)) == NULL)
							goto LibFail;
						elp = ary1->a_elp;
						if(n != INT_MIN)
							dsetint((long) scr->s_num,*elp++);
						dsetint(wnum,*elp++);
						if(dsetstr(win->w_buf->b_bname,*elp) != 0 ||
						 (el = aget(ary0,ary0->a_used,true)) == NULL)
							goto LibFail;
						if(awrap(el,ary1) != Success)
							goto Retn;
						} while((win = win->w_next) != NULL);
					}
				} while((scr = scr->s_next) != NULL);
		}
Retn:
	// Return results.
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// binsearch() helper function for returning a getInfo keyword, given table (array) and index.
static char *gikw(void *table,ssize_t i) {

	return ((InfoTab *) table)[i].keyword;
	}

// Get informational item per keyword argument.  Return status.
int getInfo(Datum *rval,int n,Datum **argv) {
	static InfoTab itab[] = {
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
		{"windows",NULL,-1}};			// [[windNum,bufname],...] or [[screenNum,windNum,bufname],...]
	ssize_t i;
	InfoTab *itp;
	char *keywd = argv[0]->d_str;

	// Search table for a match.
	if(binsearch(keywd,(void *) itab,sizeof(itab) / sizeof(InfoTab),strcasecmp,gikw,&i)) {

		// Match found.  Check keyword type.
		itp = itab + i;
		if(itp->value != NULL)
			return dsetstr(itp->value,rval) != 0 ? librcset(Failure) : rc.status;
#ifndef OSName
		if(strcmp(itp->keyword,"os") == 0)
			return getOS(rval);
#endif
		// Not a string constant value.  Build array and return it.
		return getary(rval,n,itp->id);
		}

	// Match not found.
	return rcset(Failure,0,text447,text450,keywd);
			// "Invalid %s '%s'","type keyword"
	}

// If default n, display the current line, column, and character position of the point in the current buffer, the fraction of
// the text that is before the point, and the character that is at point (in printable form and hex).  If n is not the
// default, display the point column and the character at point only.  The displayed column is not the current column,
// but the column on an infinite-width display.  *Interactive only*
int showPoint(Datum *rval,int n,Datum **argv) {
	Line *lnp;
	ulong dotline = 1;
	ulong numchars = 0;		// # of chars in file.
	ulong numlines = 0;		// # of lines in file.
	ulong prechars = 0;		// # chars preceding point.
	ulong prelines = 0;		// # lines preceding point.
	short curchar = '\0';		// Character at point.
	double ratio = 0.0;
	int col = 0;
	int ecol = 0;			// Column pos/end of current line.
	Point wkpoint;
	char *info;
	Point *point = &si.curwin->w_face.wf_point;
	char *str,wkbuf1[32],wkbuf2[32];

	str = strcpy(wkbuf2,"0.0");
	if(!bempty(NULL)) {
		if(n == INT_MIN) {

			// Start at the beginning of the buffer.
			lnp = si.curbuf->b_lnp;
			curchar = '\0';

			// Count characters and lines.
			numchars = numlines = 0;
			while(lnp->l_next != NULL || lnp->l_used > 0) {

				// If we are on the current line, save "pre-point" stats.
				if(lnp == point->lnp) {
					dotline = (prelines = numlines) + 1;
					prechars = numchars + point->off;
					if(lnp->l_next != NULL || point->off < lnp->l_used)
						curchar = (point->off < lnp->l_used) ? lnp->l_text[point->off] : '\n';
					}

				// On to the next line.
				++numlines;
				numchars += lnp->l_used + (lnp->l_next == NULL ? 0 : 1);
				if((lnp = lnp->l_next) == NULL)
					break;
				}

			// If point is at end of buffer, save "pre-point" stats.
			if(bufend(point)) {
				dotline = (prelines = numlines) + (point->lnp->l_used == 0 ? 1 : 0);
				prechars = numchars;
				}

			ratio = 0.0;				// Ratio before point.
			if(numchars > 0)
				ratio = (double) prechars / numchars * 100.0;
			sprintf(str = wkbuf2,"%.1f",ratio);
			if(numchars > 0) {

				// Fix rounding errors at buffer boundaries.
				if(prechars > 0 && strcmp(str,"0.0") == 0)
					str = "0.1";
				else if(prechars < numchars && strcmp(str,"100.0") == 0)
					str = "99.9";
				}
			}
		else if(!bempty(NULL) && (point->lnp->l_next != NULL || point->off < point->lnp->l_used))
			curchar = (point->off == point->lnp->l_used) ? '\n' : point->lnp->l_text[point->off];

		// Get real column and end-of-line column.
		col = getcol(NULL,false);
		wkpoint.lnp = point->lnp;
		wkpoint.off = point->lnp->l_used;
		ecol = getcol(&wkpoint,false);
		}

	// Summarize and report the info.
	if(curchar >= ' ' && curchar < 0x7F)
		sprintf(wkbuf1,"'%c' 0x%.2hX",(int) curchar,curchar);
	else
		sprintf(wkbuf1,"0x%.2hX",curchar);
	if((n == INT_MIN ? asprintf(&info,text60,AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,dotline,numlines,
		// "%c%cLine:%c%c %lu of %lu, %c%cCol:%c%c %d of %d, %c%cByte:%c%c %lu of %lu (%s%%); %c%cchar%c%c = %s"
	 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,col,ecol,
	 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,prechars,numchars,str,
	 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,wkbuf1) :
	 asprintf(&info,text340,
	 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,col,ecol,
	 AttrSpecBegin,AttrBoldOn,AttrSpecBegin,AttrBoldOff,wkbuf1)) < 0)
			// "%c%cCol:%c%c %d of %d; %c%cchar%c%c = %s"
		return rcset(Panic,0,text94,"showPoint");
			// "%s(): Out of memory!"
	(void) mlputs(MLHome | MLTermAttr | MLFlush,info);
	free((void *) info);
	return rc.status;
	}

// Determine if an object is defined, given op and name.  If op is "Name", set rval to result: "alias", "buffer", "command",
// "pseudo-command", "function", "macro", "variable", or nil; otherwise, set rval to true if mark is defined and active in
// current buffer (op "Mark"), mode exists (op "Mode"), or mode group exists (op "ModeGroup"); otherwise, false.
int isdef(Datum *rval,int n,Datum **argv) {
	char *op = argv[0]->d_str;
	Datum *name = argv[1];
	bool activeMark = false;
	char *result = NULL;
	UnivPtr univ;

	// Mark?
	if(strcasecmp(op,"mark") == 0 || (activeMark = strcasecmp(op,"activemark") == 0)) {
		if(intval(name) && markval(name)) {
			Mark *mark;

			dsetbool(mfind(name->u.d_int,&mark,activeMark ? MkOpt_Query | MkOpt_Viz : MkOpt_Query) == Success &&
			 mark != NULL,rval);
			}
		goto Retn;
		}

	if(!strval(name))
		goto Retn;

	// Mode?
	if(strcasecmp(op,"mode") == 0) {
		dsetbool(mdsrch(name->d_str,NULL) != NULL,rval);
		goto Retn;
		}

	// Mode group?
	if(strcasecmp(op,"modegroup") == 0) {
		dsetbool(mgsrch(name->d_str,NULL,NULL),rval);
		goto Retn;
		}

	// Unknown op?
	if(strcasecmp(op,"name") != 0) {
		(void) rcset(Failure,0,text447,text450,op);
				// "Invalid %s '%s'","type keyword"
		goto Retn;
		}

	// Variable?
	if(findvar(name->d_str,NULL,OpQuery))
		result = text292;
			// "variable"

	// Command, pseudo-command, function, alias, or macro?
	else if(execfind(name->d_str,OpQuery,PtrAny,&univ))
		switch(univ.p_type) {
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
			case PtrMacro_C:
			case PtrMacro_O:
				result = text336;
					// "macro"
				break;
			default:	// PtrAlias
				result = text127;
					// "alias"
			}
	else if(bsrch(name->d_str,NULL) != NULL)
		result = text83;
			// "buffer"

	// Return result.
	if(result == NULL)
		dsetnil(rval);
	else if(dsetstr(result,rval) != 0)
		(void) librcset(Failure);
Retn:
	return rc.status;
	}

#if WordCount
// Count the number of words in the marked region, along with average word sizes, number of chars, etc, and report on them.
// *Interactive only*
int countWords(Datum *rval,int n,Datum **argv) {
	Line *lnp;		// Current line to scan.
	int offset;		// Current char to scan.
	long size;		// Size of region left to count.
	short c;		// Current character to scan.
	bool wordflag;		// Is current character a word character?
	bool inword;		// Are we in a word now?
	long nwords;		// Total number of words.
	long nchars;		// Total number of word characters.
	int nlines;		// Total number of lines in region.
	Region region;		// Region to look at.
	char *info;

	// Make sure we have a region to count.
	if(getregion(&region,0) != Success)
		return rc.status;
	lnp = region.r_point.lnp;
	offset = region.r_point.off;
	size = region.r_size;

	// Count up things.
	inword = false;
	nchars = nwords = 0;
	nlines = 0;
	while(size--) {

		// Get the current character...
		if(offset == lnp->l_used) {	// End of line.
			c = '\n';
			lnp = lnp->l_next;
			offset = 0;

			++nlines;
			}
		else {
			c = lnp->l_text[offset];
			++offset;
			}

		// and tabulate it.
		if((wordflag = isletter(c) || isdigit(c)))
			++nchars;
		if(wordflag && !inword)
			++nwords;
		inword = wordflag;
		}

	// and report on the info.
	if(asprintf(&info,text100,nwords,nchars,region.r_size,nlines + 1,(nwords > 0) ? nchars * 1.0 / nwords : 0.0) < 0)
			// "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f"
		return rcset(Panic,0,text94,"countWords");
			// "%s(): Out of memory!"
	(void) mlputs(MLHome | MLFlush,info);
	free((void *) info);
	return rc.status;
	}
#endif

// Get an apropos match string with a null default.  Convert a nil argument to null as well.  Return status.
static int getamatch(ShowCtrl *scp,char *prmt,Datum **argv) {
	Datum *mstr = &scp->sc_mstr;

	if(!(si.opflags & OpScript)) {
		char wkbuf[NWork + 1];

		sprintf(wkbuf,"%s %s",text20,prmt);
				// "Apropos"
		if(terminp(mstr,wkbuf,ArgNil1,0,NULL) != Success)
			return rc.status;
		if(mstr->d_type == dat_nil)
			dsetnull(mstr);
		}
	else if(argv[0]->d_type == dat_nil)
		dsetnull(mstr);
	else
		datxfer(mstr,argv[0]);

	// Set up match record if pattern given.  Force "ignore" if non-RE and non-Exact.
	if(mstr->d_type != dat_nil && !disnull(mstr) && newspat(mstr->d_str,&scp->sc_match,NULL) == Success) {
		if(scp->sc_match.flags & SOpt_Regexp) {
			if(mccompile(&scp->sc_match) != Success)
				freespat(&scp->sc_match);
			}
		else if(!(scp->sc_match.flags & SOpt_Exact))
			scp->sc_match.flags |= SOpt_Ignore;
		}

	return rc.status;
	}

// Initialize color pairs for a "show" listing.
void initInfoColors(void) {

	if(si.opflags & OpHaveColor) {
		short *cp = term.itemColor[ColorIdxInfo].colors;
		if(cp[0] >= -1) {
			short fg,bg,line;

			fg = cp[0];
			bg = cp[1];
			line = bg >= 0 ? bg : fg;
			(void) init_pair(term.maxWorkPair - ColorPairIH,fg,bg);
			(void) init_pair(term.maxWorkPair - ColorPairISL,line,-1);
			}
		}
	}

// Initialize a ShowCtrl object for a "show" listing.  If argv not NULL, get a match string and save in control object for later
// use.  Return status.
int showopen(ShowCtrl *scp,int n,char *plabel,Datum **argv) {
	char *str,wkbuf[strlen(plabel) + 3];

	dinit(&scp->sc_name);
	dinit(&scp->sc_value);
	dinit(&scp->sc_mstr);
	minit(&scp->sc_match);
	scp->sc_n = n;

	// If argv not NULL, get match string.
	if(argv != NULL && getamatch(scp,plabel,argv) != Success)
		return rc.status;

	// Create a buffer name (plural of plabel) and get a buffer.
	str = stpcpy(wkbuf,plabel);
	wkbuf[0] = upcase[(int) wkbuf[0]];
	strcpy(str,str[-1] == 's' ? "es" : "s");
	if(sysbuf(wkbuf,&scp->sc_list,BFTermAttr) == Success)
		initInfoColors();

	return rc.status;
	}

// Copy src to dest in upper case, inserting a space between each two characters.  Return dest.
static char *inflate1(char *dest,char *src) {

	do {
		*dest++ = upcase[(int) *src++];
		*dest++ = ' ';
		} while(*src != '\0');
	return dest;
	}

// Copy src to dest in upper case, inserting a space between each two characters.  If plural is true, append "s" or "es".
// Return dest.
char *inflate(char *dest,char *src,bool plural) {

	dest = inflate1(dest,src);
	if(plural)
		dest = inflate1(dest,dest[-2] == 'S' ? "es" : "s");
	*--dest = '\0';
	return dest;
	}

// Write header lines to an open string-fab object, given object pointer, report title, plural flag, column headings line, and
// column widths.  Return status.  If wp is NULL, title (only) is written with bold and color attributes (if possible), centered
// in full terminal width and, if colhead not NULL, length of colhead string is used for colored length of title.  Otherwise,
// page width is calculated from wp array, title is written in bold, centered in calculated width, and headings are written in
// bold and color.  The minwidth member of the second-to-last element of the wp array may be -1, which indicates a width from
// the current column position to the right edge of the screen.  Spacing between columns is 1 if terminal width < 96; otherwise,
// 2.  *pgwidth is set to page width if pgwidth not NULL.
int rpthdr(DStrFab *rpt,char *title,bool plural,char *colhead,ColHdrWidth *wp,int *pgwidth) {
	ColHdrWidth *wp1;
	int leadin,whitespace,width;
	char *space = (term.t_ncol < 96) ? " " : "  ";
	int spacing = (term.t_ncol < 96) ? 1 : 2;
	short *cp = term.itemColor[ColorIdxInfo].colors;
	char wkbuf[term.t_ncol + 1];

	// Expand title, get page width, and begin color if applicable.
	inflate(wkbuf,title,plural);
	if(wp == NULL) {
		if(colhead == NULL)
			width = term.t_ncol;
		else {
			width = strlen(colhead);
			if((leadin = (term.t_ncol - width) >> 1) > 0 && dputf(rpt,"%*s",leadin,"") != 0)
				goto LibFail;
			}
		if((cp[0] >= -1) && dputf(rpt,"%c%hd%c",AttrSpecBegin,term.maxWorkPair - ColorPairIH,AttrColorOn) != 0)
			goto LibFail;
		}
	else {
		width = -spacing;
		wp1 = wp;
		do {
			if(wp1->minwidth == -1) {
				width = term.t_ncol;
				break;
				}
			width += wp1->minwidth + spacing;
			++wp1;
			} while(wp1->minwidth != 0);
		}
	if(pgwidth != NULL)
		*pgwidth = width;

	// Write centered title line in bold.
	whitespace = width - strlen(wkbuf);
	leadin = whitespace >> 1;
	if(dputf(rpt,"%*s%c%c%s%c%c",leadin,"",AttrSpecBegin,AttrBoldOn,wkbuf,AttrSpecBegin,AttrBoldOff) != 0)
		goto LibFail;

	// Finish title line and write column headings if requested.
	if(wp != NULL) {
		char *str = colhead;
		int curcol = 0;

		if(dputs("\n\n",rpt) != 0)
			goto LibFail;

		// Write column headings in bold, and in color if available.
		if(dputf(rpt,"%c%c",AttrSpecBegin,AttrBoldOn) != 0)
			goto LibFail;
		wp1 = wp;
		do {
			if(wp1 != wp) {
				if(dputs(space,rpt) != 0)
					goto LibFail;
				curcol += spacing;
				}
			width = (wp1->minwidth == -1) ? term.t_ncol - curcol : (int) wp1->minwidth;
			if(cp[0] >= -1) {
				if(dputf(rpt,"%c%hd%c%-*.*s%c%c",AttrSpecBegin,term.maxWorkPair - ColorPairIH,AttrColorOn,
				 width,(int) wp1->maxwidth,str,AttrSpecBegin,AttrColorOff) != 0)
					goto LibFail;
				}
			else if(dputf(rpt,"%-*.*s",width,(int) wp1->maxwidth,str) != 0)
				goto LibFail;
			curcol += width;
			str += wp1->maxwidth;
			++wp1;
			} while(wp1->minwidth != 0);
		if(dputf(rpt,"%c%c",AttrSpecBegin,AttrBoldOff) != 0)
			goto LibFail;

		// Underline each column header if no color.
		if(!(si.opflags & OpHaveColor)) {
			if(dputc('\n',rpt) != 0)
				goto LibFail;
			dupchr(str = wkbuf,'-',term.t_ncol);
			curcol = 0;
			wp1 = wp;
			do {
				if(wp1 != wp) {
					if(dputs(space,rpt) != 0)
						goto LibFail;
					curcol += spacing;
					}
				width = (wp1->minwidth == -1) ? term.t_ncol - curcol : (int) wp1->minwidth;
				if(dputf(rpt,"%.*s",width,str) != 0)
					goto LibFail;
				curcol += width;
				str += wp1->minwidth;
				++wp1;
				} while(wp1->minwidth != 0);
			}
		}
	else if((si.opflags & OpHaveColor) && dputf(rpt,"%*s%c%c",whitespace - leadin,"",AttrSpecBegin,AttrColorOff) != 0)
		goto LibFail;
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Write header lines to an open string-fab object, given report title.  Return status.
static int showhdr(ShowCtrl *scp,char *title) {

	// If not using color, write separator line.
	if(!(si.opflags & OpHaveColor)) {
		char wkbuf[term.t_ncol + 1];
		dupchr(wkbuf,'=',term.t_ncol);
		if(dputs(wkbuf,&scp->sc_rpt) != 0 || dputc('\n',&scp->sc_rpt) != 0)
			return librcset(Failure);
		}

	// Write title.
	return rpthdr(&scp->sc_rpt,title,true,NULL,NULL,NULL);
	}

// Build a "show" listing in a report buffer, given ShowCtrl object, flags, section title (which may be NULL), and pointer to
// routine which sets the name + usage, value (if applicable), and description for the next list item in the ShowCtrl object.
// Return status.
int showbuild(ShowCtrl *scp,ushort flags,char *title,int (*fp)(ShowCtrl *scp,ushort req,char **name)) {
	char *nametab[] = {NULL,NULL,NULL};
	char **name,*str0,*str,*strz;
	bool firstItem = true;
	bool doApropos = scp->sc_mstr.d_type != dat_nil;
	Datum *index,*src;
	short *cp = term.itemColor[ColorIdxInfo].colors;
	char sepline[term.t_ncol + 1];
	char wkbuf[NWork];

	// Initialize.
	scp->sc_item = NULL;
	if(flags & SHNoDesc)
		scp->sc_desc = NULL;
	if(doApropos && !(flags & SHExact)) {
		if(dnewtrk(&index) != 0 || dnewtrk(&src) != 0)
			goto LibFail;
		}

	// Open a string-fab object and write section header if applicable.
	if(dopentrk(&scp->sc_rpt) != 0)
		goto LibFail;
	if(title != NULL && showhdr(scp,title) != Success)
		return rc.status;

	// Loop through detail items.
	dupchr(sepline,'-',term.t_ncol);
	for(;;) {
		// Find next item and get its name.  Exit loop if no items left.
		if(fp(scp,SHReqNext,nametab) != Success)
			return rc.status;
		if(nametab[0] == NULL)
			break;

		// Skip if apropos in effect and item name doesn't match the search pattern.
		if(doApropos) {
			if(flags & SHExact) {
				if(strcmp(nametab[0],scp->sc_mstr.d_str) != 0)
					continue;
				}
			else if(!disnull(&scp->sc_mstr)) {
				name = nametab;
				do {
					if(dsetstr(*name++,src) != 0)
						goto LibFail;
					if(sindex(index,0,src,&scp->sc_mstr,&scp->sc_match) != Success)
						return rc.status;
					if(index->d_type != dat_nil)
						goto MatchFound;
					} while(*name != NULL);
				continue;
				}
			}
MatchFound:
		// Get item usage and description, and "has value" flag (in nametab[0] slot).
		if(fp(scp,SHReqUsage,nametab) != Success)
			return rc.status;

		// Begin next line.
		if((flags & SHSepLine) || firstItem) {
			if(dputc('\n',&scp->sc_rpt) != 0)
				goto LibFail;
			if((cp[0] >= -1) && dputf(&scp->sc_rpt,"%c%hd%c",AttrSpecBegin,
			 term.maxWorkPair - ColorPairISL,AttrColorOn) != 0)
				goto LibFail;
			if(dputs(sepline,&scp->sc_rpt) != 0)
				goto LibFail;
			if((cp[0] >= -1) && dputf(&scp->sc_rpt,"%c%c",AttrSpecBegin,AttrColorOff) != 0)
				goto LibFail;
			}
		firstItem = false;

		// Store item name and value, if any, in work buffer and add line to report.
		sprintf(wkbuf,"%c%c%s%c%c",AttrSpecBegin,AttrBoldOn,scp->sc_name.d_str,AttrSpecBegin,AttrBoldOff);
		if(nametab[0] != NULL) {
			str = pad(wkbuf,34);
			if(str[-2] != ' ')
				strcpy(str,str[-1] == ' ' ? " " : "  ");
			}
		if(dputc('\n',&scp->sc_rpt) != 0 || dputs(wkbuf,&scp->sc_rpt) != 0)
			goto LibFail;
		if(nametab[0] != NULL && fp(scp,SHReqValue,NULL) != Success)
			return rc.status;

		// Store indented description, if present and not blank.  Wrap into as many lines as needed.  May contain
		// terminal attribute sequences.
		if(scp->sc_desc != NULL) {
			int len;
			str0 = scp->sc_desc;
			for(;;) {						// Skip leading white space.
				if(*str0 != ' ')
					break;
				++str0;
				}
			strz = strchr(str0,'\0');
			for(;;) {						// Wrap loop.
				len = attrCount(str0,strz - str0,term.t_ncol - 4);
				if(strz - str0 - len <= term.t_ncol - 4)	// Remainder too long?
					str = strz;				// No.
				else {						// Yes, find space to break on.
					str = str0 + len + term.t_ncol - 4;
					for(;;) {
						if(*--str == ' ')
							break;
						if(str == str0) {
							str += len + term.t_ncol - 4;
							break;
							}
						}
					}
				if(dputc('\n',&scp->sc_rpt) != 0 || dputs("    ",&scp->sc_rpt) != 0 ||
				 dputmem((void *) str0,str - str0,&scp->sc_rpt) != 0)
					goto LibFail;
				if(str == strz)
					break;
				str0 = *str == ' ' ? str + 1 : str;
				}
			}
		}

	// Close string-fab object and append string to report buffer if any items were written.
	if(dclose(&scp->sc_rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(!firstItem) {

		// Write blank line if title not NULL and buffer not empty.
		if(title != NULL && !bempty(scp->sc_list))
			if(bappend(scp->sc_list,"") != Success)
				return rc.status;

		// Append section detail.
		if(bappend(scp->sc_list,scp->sc_rpt.sf_datum->d_str) != Success)
			return rc.status;
		}

	return rc.status;
	}

// Close a "show" listing.  Return status.
int showclose(Datum *rval,int n,ShowCtrl *scp) {

	dclear(&scp->sc_name);
	dclear(&scp->sc_value);
	dclear(&scp->sc_mstr);
	if(scp->sc_match.ssize > 0)
		freespat(&scp->sc_match);

	// Display the list.
	return render(rval,n,scp->sc_list,RendNewBuf | RendReset);
	}

// Get name, usage, and key bindings (if any) for current list item (command, function, or macro) and save in report-control
// object.  Return status.
int findkeys(ShowCtrl *scp,uint ktype,void *tp) {
	KeyWalk kw = {NULL,NULL};
	KeyBind *kbind;
	Buffer *buf;
	CmdFunc *cfp;
	MacInfo *mip = NULL;
	char *name,*usage;
	char keybuf[16];

	// Set pointers and item description.
	if(ktype & PtrMacro) {
		buf = (Buffer *) tp;
		name = buf->b_bname + 1;
		mip = buf->b_mip;
		if(mip != NULL) {
			usage = (mip->mi_usage.d_type != dat_nil) ? mip->mi_usage.d_str : NULL;
			scp->sc_desc = (mip->mi_desc.d_type != dat_nil) ? mip->mi_desc.d_str : NULL;
			}
		else
			scp->sc_desc = usage = NULL;
		}
	else {
		cfp = (CmdFunc *) tp;
		name = cfp->cf_name;
		usage = cfp->cf_usage;
		scp->sc_desc = cfp->cf_desc;
		}

	// Set item name and usage.
	if(usage == NULL) {
		if(dsetstr(name,&scp->sc_name) != 0)
			goto LibFail;
		}
	else {
		char wkbuf[strlen(name) + strlen(usage) + 1];
		sprintf(wkbuf,"%s %s",name,usage);
		if(dsetstr(wkbuf,&scp->sc_name) != 0)
			goto LibFail;
		}

	// Set key bindings, if any.
	if(ktype & PtrFunc)
		dclear(&scp->sc_value);
	else {
		DStrFab sfab;
		char *sep = NULL;

		// Search for any keys bound to command or macro (buffer) "tp".
		if(dopenwith(&sfab,&scp->sc_value,SFClear) != 0)
			goto LibFail;
		kbind = nextbind(&kw);
		do {
			if((kbind->k_targ.p_type & ktype) && kbind->k_targ.u.p_void == tp) {

				// Add the key sequence.
				ektos(kbind->k_code,keybuf,true);
				if((sep != NULL && dputs(sep,&sfab) != 0) ||
				 dputf(&sfab,"%c%c%c%s%c%c",AttrSpecBegin,AttrAlt,AttrULOn,keybuf,AttrSpecBegin,AttrULOff) != 0)
					goto LibFail;
				sep = ", ";
				}
			} while((kbind = nextbind(&kw)) != NULL);
		if(dclose(&sfab,sf_string) != 0)
			goto LibFail;
		}

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Get next command or function name or description and store in report-control object.  If req is SHReqNext, set *name to NULL
// if no items left; otherwise, pointer to its name.  Return status.
static int nextCmdFunc(ShowCtrl *scp,ushort req,char **name,ushort aflags) {
	CmdFunc *cfp;

	// First call?
	if(scp->sc_item == NULL)
		scp->sc_item = (void *) (cfp = cftab);
	else {
		cfp = (CmdFunc *) scp->sc_item;
		if(req == SHReqNext)
			++cfp;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; cfp->cf_name != NULL; ++cfp) {
	
				// Skip if wrong type.
				if((cfp->cf_aflags & CFFunc) != aflags)
					continue;

				// Found item... return its name.
				*name = cfp->cf_name;
				scp->sc_item = (void *) cfp;
				goto Retn;
				}

			// End of table.
			*name = NULL;
			break;
		case SHReqUsage:
			if(findkeys(scp,aflags ? PtrFunc : PtrCmdType,(void *) cfp) != Success)
				return rc.status;
			*name = (scp->sc_value.d_type == dat_nil) ? NULL : cfp->cf_name;
			break;
		default: // SHReqValue
			if(dputs(scp->sc_value.d_str,&scp->sc_rpt) != 0)
				(void) librcset(Failure);
		}
Retn:
	return rc.status;
	}

// Get next command name and description and store in report-control object via call to nextCmdFunc().
int nextCommand(ShowCtrl *scp,ushort req,char **name) {

	return nextCmdFunc(scp,req,name,0);
	}

// Create formatted list of commands via calls to "show" routines.  Return status.
int showCommands(Datum *rval,int n,Datum **argv) {
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,n,text158,argv) == Success && showbuild(&sc,SHSepLine,text158,nextCommand) == Success)
			// "command"
		(void) showclose(rval,n,&sc);
	return rc.status;
	}

// Get next function name and description and store in report-control object via call to nextCmdFunc().
static int nextFunction(ShowCtrl *scp,ushort req,char **name) {

	return nextCmdFunc(scp,req,name,CFFunc);
	}

// Create formatted list of system functions via calls to "show" routines.  Return status.
int showFunctions(Datum *rval,int n,Datum **argv) {
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,n,text247,argv) == Success && showbuild(&sc,SHSepLine,text247,nextFunction) == Success)
			// "function"
		(void) showclose(rval,n,&sc);
	return rc.status;
	}

// Get next macro name or description and store in report-control object.  If req is SHReqNext, set *name to NULL if no items
// left; otherwise, pointer to its name.  Return status.
int nextMacro(ShowCtrl *scp,ushort req,char **name) {
	Buffer *buf;
	Datum **blp;
	Datum **blpz = buftab.a_elp + buftab.a_used;

	// First call?
	if(scp->sc_item == NULL) {
		scp->sc_item = (void *) (blp = buftab.a_elp);
		buf = bufptr(buftab.a_elp[0]);
		}
	else {
		blp = (Datum **) scp->sc_item;
		buf = (req == SHReqNext && ++blp == blpz) ? NULL : bufptr(*blp);
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			while(buf != NULL) {

				// Select if macro buffer and (not constrained or bound to a hook or non-default n).
				if((buf->b_flags & BFMacro) && (!(buf->b_flags & BFConstrain) || ishook(buf,false) ||
				 scp->sc_n != INT_MIN)) {

					// Found macro... return its name.
					*name = buf->b_bname + 1;
					scp->sc_item = (void *) blp;
					goto Retn;
					}
				buf = (++blp == blpz) ? NULL : bufptr(*blp);
				}

			// End of list.
			*name = NULL;
			break;
		case SHReqUsage:
			if(findkeys(scp,PtrMacro,(void *) buf) != Success)
				return rc.status;
			*name = (scp->sc_value.d_type == dat_nil) ? NULL : buf->b_bname;
			break;
		default: // SHReqValue
			if(dputs(scp->sc_value.d_str,&scp->sc_rpt) != 0)
				(void) librcset(Failure);
		}
Retn:
	return rc.status;
	}

// Create formatted list of macros via calls to "show" routines.  Return status.
int showMacros(Datum *rval,int n,Datum **argv) {
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,n,text336,argv) == Success && showbuild(&sc,SHSepLine,text336,nextMacro) == Success)
			// "macro"
		(void) showclose(rval,n,&sc);
	return rc.status;
	}

// Get next alias name or description and store in report-control object.  If req is SHReqNext, set *name to NULL if no items
// left; otherwise, pointer to its name.  Return status.
static int nextAlias(ShowCtrl *scp,ushort req,char **name) {
	Alias *ap;

	// First call?
	if(scp->sc_item == NULL)
		scp->sc_item = (void *) (ap = ahead);
	else {
		ap = (Alias *) scp->sc_item;
		if(req == SHReqNext)
			ap = ap->a_nextp;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; ap != NULL; ap = ap->a_nextp) {

				// Found alias... return its name and name it points to.
				*name++ = ap->a_name;
				*name = (ap->a_type == PtrAlias_M ? ap->a_targ.u.p_buf->b_bname :
				 ap->a_targ.u.p_cfp->cf_name);
				scp->sc_item = (void *) ap;
				goto Retn;
				}

			// End of list.
			*name = NULL;
			break;
		case SHReqUsage:
			if(dsetstr(ap->a_name,&scp->sc_name) != 0)
				goto LibFail;
			*name = ap->a_name;
			break;
		default: // SHReqValue
			{char *name2 = (ap->a_targ.p_type & PtrMacro) ? ap->a_targ.u.p_buf->b_bname :
			 ap->a_targ.u.p_cfp->cf_name;
			if(dputs("-> ",&scp->sc_rpt) != 0 || dputs(name2,&scp->sc_rpt) != 0)
				goto LibFail;
			}
		}
Retn:
	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Create formatted list of aliases via calls to "show" routines.  Return status.
int showAliases(Datum *rval,int n,Datum **argv) {
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,n,text127,argv) == Success && showbuild(&sc,SHNoDesc,text127,nextAlias) == Success)
			// "alias"
		(void) showclose(rval,n,&sc);
	return rc.status;
	}

// Create formatted list of commands, macros, functions, aliases, and variables which match a pattern via calls to "show"
// routines.  Return status.
int apropos(Datum *rval,int n,Datum **argv) {
	ShowCtrl sc;

	// Open control object.
	if(showopen(&sc,n,Literal4,argv) == Success) {
			// "name"

		// Call the various show routines and build the list.
		if(showbuild(&sc,SHSepLine,text158,nextCommand) == Success &&
						// "command"
		 showbuild(&sc,SHSepLine,text336,nextMacro) == Success &&
						// "macro"
		 showbuild(&sc,SHSepLine,text247,nextFunction) == Success &&
						// "function"
		 showbuild(&sc,SHNoDesc,text127,nextAlias) == Success &&
						// "alias"
		 showbuild(&sc,SHSepLine,text21,nextSysVar) == Success &&
						// "system variable"
		 showbuild(&sc,SHNoDesc,text56,nextGlobalVar) == Success &&
		 showbuild(&sc,SHNoDesc,NULL,nextLocalVar) == Success)
						// "user variable"
			(void) showclose(rval,n,&sc);
		}
	return rc.status;
	}

// Build and pop up a buffer containing a list of all visible buffers.  List hidden buffers as well if n argument, but exclude
// macros if n is -1.  Render buffer and return status.
int showBuffers(Datum *rval,int n,Datum **argv) {
	Array *ary;
	Datum *el;
	Buffer *list,*buf;
	DStrFab rpt;
	short c;
	int pagewidth;
	char *space = (term.t_ncol < 96) ? " " : "  ";
	static ColHdrWidth hcols[] = {
		{10,10},{9,9},{MaxBufName - 4,MaxBufName - 4},{31,8},{0,0}};
	struct bfinfo {				// Array of buffer flags.
		ushort bflag,sbchar;
		} *bfrec;
	static struct bfinfo bftab[] = {
		{BFActive,SBActive},		// Activated buffer -- file read in.
		{BFHidden,SBHidden},		// Hidden buffer.
		{BFMacro,SBMacro},		// Macro buffer.
		{BFConstrain,SBConstrain},	// Constrained macro.
		{BFPreproc,SBPreproc},		// Preprocessed buffer.
		{BFNarrowed,SBNarrowed},	// Narrowed buffer.
		{BFTermAttr,SBTermAttr},	// Terminal-attributes buffer.
		{USHRT_MAX,SBBackground},	// Background buffer.
		{BFReadOnly,SBReadOnly},	// Read-only buffer.
		{BFChanged,SBChanged},		// Changed buffer.
		{0,0}};
	struct flaginfo {
		short c;
		char *desc;
		} *ftp;
	static struct flaginfo flagtab[] = {
		{SBActive,text31},
				// "Active"
		{SBHidden,text400},
				// "Hidden"
		{SBMacro,text412},
				// "Macro"
		{SBConstrain,text440},
				// "Constrained"
		{SBPreproc,text441},
				// "Preprocessed"
		{SBNarrowed,text308},
				// "Narrowed"
		{SBTermAttr,text442},
				// "Terminal attributes enabled"
		{SBBackground,text462},
				// "Background (not being displayed)"
		{SBReadOnly,text459},
				// "Read only"
		{SBChanged,text439},
				// "Changed"
		{'\0',NULL}};

	// Get new buffer and string-fab object.
	if(sysbuf(text159,&list,BFTermAttr) != Success)
			// "Buffers"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rpthdr(&rpt,text159,false,text30,hcols,&pagewidth) != Success)
			// "Buffers","AHMCPNTBRC   Size      Buffer              File"
		return rc.status;

	// Output the list of buffers.
	ary = &buftab;
	while((el = aeach(&ary)) != NULL) {
		buf = bufptr(el);

		// Skip hidden buffers if default n or just macros if n == -1.
		if(((buf->b_flags & BFHidden) && n == INT_MIN) || (n == -1 && (buf->b_flags & BFMacro)))
			continue;

		if(dputc('\n',&rpt) != 0)
			goto LibFail;

		// Output the buffer flag indicators.
		bfrec = bftab;
		do {
			if(bfrec->bflag == USHRT_MAX)
				c = (buf->b_nwind == 0) ? bfrec->sbchar : ' ';
			else if(!(buf->b_flags & bfrec->bflag))
				c = ' ';
			else if((c = bfrec->sbchar) == AttrSpecBegin && dputc(AttrSpecBegin,&rpt) != 0)
				goto LibFail;
			if(dputc(c,&rpt) != 0)
				goto LibFail;
			} while((++bfrec)->bflag != 0);

#if (MMDebug & Debug_BufWindCt) && 0
		dputf(&rpt,"%3hu",buf->b_nwind);
#endif
		// Output the buffer size, buffer name, and filename.
		if(dputf(&rpt,"%s%*lu%s%-*s",space,hcols[1].minwidth,buflength(buf,NULL),space,hcols[2].minwidth,
		 buf->b_bname) != 0 || (buf->b_fname != NULL && (dputs(space,&rpt) != 0 || dputs(buf->b_fname,&rpt) != 0)))
			goto LibFail;
		}

	// Write footnote.
	if(sepline(pagewidth,&rpt) != 0)
		goto LibFail;
	ftp = flagtab;
	do {
		if(dputf(&rpt,"\n%c",(int) ftp->c) != 0 || (ftp->c == AttrSpecBegin && dputc(AttrSpecBegin,&rpt) != 0) ||
		 dputc(' ',&rpt) != 0 || dputs(ftp->desc,&rpt) != 0)
			goto LibFail;
		} while((++ftp)->desc != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		goto LibFail;
	if(bappend(list,rpt.sf_datum->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rval,n,list,RendNewBuf | RendReset);
LibFail:
	return librcset(Failure);
	}

// Display color palette or users pairs in a pop-up window.
int showColors(Datum *rval,int n,Datum **argv) {

	if(!haveColor())
		return rc.status;

	Buffer *slist;
	DStrFab rpt;
	short pair;

	// Get a buffer and open a string-fab object.
	if(sysbuf(text428,&slist,BFTermAttr) != Success)
			// "Colors"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;

	// Display color pairs in use if n <= 0; otherwise, color palette (paged).
	if(n <= 0 && n != INT_MIN) {
		short fg,bg;

		if(dputs(text434,&rpt) != 0 || dputs("\n-----   -----------------",&rpt) != 0)
				// "COLOR PAIRS DEFINED\n\n~bPair#        Sample~B"
			goto LibFail;
		pair = 1;
		do {
			if(pair_content(pair,&fg,&bg) == OK && (fg != COLOR_BLACK || bg != COLOR_BLACK)) {
				if(dputf(&rpt,"\n%4hd    %c%hd%c %-16s%c%c",pair,AttrSpecBegin,pair,AttrColorOn,text431,
													// "\"A text sample\""
				 AttrSpecBegin,AttrColorOff) != 0)
					goto LibFail;
				}
			} while(++pair <= term.maxWorkPair);
		}
	else {
		short c;
		int pages = term.maxColor / term.lpp + 1;

		pair = term.maxWorkPair;
		if(n <= 1) {
			n = 1;
			c = 0;
			}
		else {
			if(n >= pages)
				n = pages;
			c = (n - 1) * term.lpp;
			}

		// Write first header line.
		if(dputf(&rpt,text429,n,pages) != 0 || dputs(text430,&rpt) != 0 ||
			// "COLOR PALETTE - Page %d of %d\n\n"
			//		"~bColor#         Foreground Samples                   Background Samples~B"
		 dputs("\n------ ----------------- -----------------   ----------------- -----------------",&rpt) != 0)
			goto LibFail;

		// For all colors...
		n = 0;
		do {
			// Write color samples.
			(void) init_pair(pair,c,-1);
			(void) init_pair(pair - 1,c,COLOR_BLACK);
			(void) init_pair(pair - 2,term.colorText,c);
			(void) init_pair(pair - 3,COLOR_BLACK,c);
			if((c > 0 && c % 8 == 0 && dputc('\n',&rpt) != 0) || dputf(&rpt,
			 "\n%4hd   %c%hd%c %-16s%c%c %c%hd%c %-16s%c%c   %c%hd%c %-16s%c%c %c%hd%c %-16s%c%c",
			 c,AttrSpecBegin,pair,AttrColorOn,text431,AttrSpecBegin,AttrColorOff,AttrSpecBegin,pair - 1,AttrColorOn,
			 text431,AttrSpecBegin,AttrColorOff,AttrSpecBegin,pair - 2,AttrColorOn,text432,AttrSpecBegin,
			 AttrColorOff,AttrSpecBegin,pair - 3,AttrColorOn,text433,AttrSpecBegin,AttrColorOff) != 0)
					// "\"A text sample\"","With white text","With black text"
				goto LibFail;
			pair -= 4;
			++c;
			++n;
			} while(c <= term.maxColor && n < term.lpp);
		}

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(slist,rpt.sf_datum->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rval,-1,slist,RendNewBuf);
	}

// Find carat or null byte in given string and return pointer to it.
static char *carat(char *str) {

	while(*str != '\0') {
		if(*str == '^')
			break;
		++str;
		}
	return str;
	}

// Build and pop up a buffer containing a list of all hooks, their associated macros, and descriptions of their arguments (if
// any).  Return status.
int showHooks(Datum *rval,int n,Datum **argv) {
	Buffer *slist;
	char *strn0,*strn,*strm0,*strm;
	HookRec *hrec = hooktab;
	DStrFab rpt;
	int indent,indent1,pagewidth;
	int spacing = (term.t_ncol < 96) ? 1 : 2;
	static ColHdrWidth hcols[] = {
		{9,9},{MaxBufName - 4,MaxBufName - 4},{21,21},{24,17},{0,0}};

	// Get a buffer and open a string-fab object.
	if(sysbuf(text316,&slist,BFTermAttr) != Success)
			// "Hooks"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rpthdr(&rpt,text316,false,text315,hcols,&pagewidth) != Success)
			// "Hooks"," Hook     Set to Macro         Numeric Argument     Macro Arguments"
		return rc.status;

	// For all hooks...
	do {
		if(hrec != hooktab && sepline(pagewidth,&rpt) != 0)
			goto LibFail;
		if(dputf(&rpt,"\n%-*s%*s%-*s",(int) hcols[0].minwidth,hrec->h_name,spacing,"",(int) hcols[1].minwidth,
		 hrec->h_buf == NULL ? "" : hrec->h_buf->b_bname + 1) != 0)
			goto LibFail;
		indent = spacing;
		strn = hrec->h_narg;
		strm = hrec->h_margs;
		indent1 = hcols[0].minwidth + hcols[1].minwidth + spacing + spacing;
		for(;;) {
			if(indent == indent1 && dputc('\n',&rpt) != 0)
				goto LibFail;
			strn = carat(strn0 = strn);
			strm = carat(strm0 = strm);
			if(dputf(&rpt,"%*s%-*.*s",indent,"",(int) hcols[2].minwidth,(int) (strn - strn0),strn0) != 0)
				goto LibFail;
			if(strm != strm0 && dputf(&rpt,"%*s%.*s",spacing,"",(int) (strm - strm0),strm0) != 0)
				goto LibFail;
			if(*strn == '\0' && *strm == '\0')
				break;
			indent = indent1;
			if(*strn == '^')
				++strn;
			if(*strm == '^')
				++strm;
			}

		// On to the next hook.
		} while((++hrec)->h_name != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(slist,rpt.sf_datum->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rval,n,slist,RendNewBuf | RendReset);
	}

// Describe the command or macro bound to a particular key.  Get single keystroke if n <= 0.  Display result on message line if
// n >= 0; otherwise, in a pop-up window (default).  Return status.
int showKey(Datum *rval,int n,Datum **argv) {
	ushort ek;
	KeyBind *kbind;
	int (*nextf)(ShowCtrl *scp,ushort req,char **name);
	char *usage = NULL;
	char *desc = NULL;
	char *name,keybuf[16];

	// Prompt the user for the key code.
	if(getkb(text13,n <= 0 && n != INT_MIN ? 0 : INT_MIN,argv,&ek) != Success)
			// "Show key "
		return rc.status;

	// Find the command or macro.
	if((kbind = getbind(ek)) == NULL)
		name = text48;
			// "[Not bound]"
	else if(kbind->k_targ.p_type == PtrMacro_O) {
		Buffer *buf = kbind->k_targ.u.p_buf;
		name = buf->b_bname + 1;
		if(n < 0) {
			nextf = nextMacro;
			usage = text336;
				// "macro"
			goto PopUp;
			}
		if(buf->b_mip->mi_usage.d_type != dat_nil)
			usage = buf->b_mip->mi_usage.d_str;
		if(buf->b_mip->mi_desc.d_type != dat_nil)
			desc = buf->b_mip->mi_desc.d_str;
		}
	else {
		CmdFunc *cfp = kbind->k_targ.u.p_cfp;
		name = cfp->cf_name;
		if(n < 0) {
			nextf = nextCommand;
			usage = text158;
				// "command"
			goto PopUp;
			}
		usage = cfp->cf_usage;
		desc = cfp->cf_desc;
		}

	// Display result on message line.
	ektos(ek,keybuf,false);
	if(mlprintf(MLHome | MLTermAttr,"%c%c%c%s%c%c -> %c%c%s%c%c",AttrSpecBegin,AttrAlt,AttrULOn,keybuf,AttrSpecBegin,
	 AttrULOff,AttrSpecBegin,AttrBoldOn,name,AttrSpecBegin,AttrBoldOff) == Success) {
		if(usage != NULL && mlprintf(MLTermAttr," %c%c%s%c%c",AttrSpecBegin,AttrBoldOn,usage,AttrSpecBegin,AttrBoldOff)
		 != Success)
			return rc.status;
		if(desc != NULL && (mlputs(MLTermAttr," - ") != Success || mlputs(MLTermAttr,desc) != Success))
			return rc.status;
		(void) mlflush();
		}
	return rc.status;
PopUp:;
	// Display result in pop-up window.
	ShowCtrl sc;

	// Open control object, build listing, and close it.
	if(showopen(&sc,INT_MIN,usage,NULL) == Success) {
		if(dsetstr(name,&sc.sc_mstr) != 0)
			return librcset(Failure);
		if(showbuild(&sc,SHSepLine | SHExact,usage,nextf) == Success)
			(void) showclose(rval,-1,&sc);
		}
	return rc.status;
	}

// Build and pop up a buffer containing all marks which exist in the current buffer.  Render buffer and return status.
int showMarks(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	Line *lnp;
	Mark *mark;
	DStrFab rpt;
	int max = term.t_ncol * 2;
	char *space = (term.t_ncol < 96) ? " " : "  ";
	int spacing = (term.t_ncol < 96) ? 1 : 2;
	static ColHdrWidth hcols[] = {
		{4,4},{6,6},{-1,11},{0,0}};

	// Get buffer for the mark list.
	if(sysbuf(text353,&buf,BFTermAttr) != Success)
			// "Marks"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rpthdr(&rpt,text353,false,text354,hcols,NULL) != Success)
			// "Marks","MarkOffset  Line Text"
		return rc.status;

	// Loop through lines in current buffer, searching for mark matches.
	lnp = si.curbuf->b_lnp;
	do {
		mark = &si.curbuf->b_mroot;
		do {
			if(mark->mk_id < '~' && mark->mk_point.lnp == lnp) {
				if(dputf(&rpt,"\n %c %*d",mark->mk_id,
				 (int)(hcols[0].minwidth - 3 + spacing + hcols[1].minwidth),mark->mk_point.off) != 0)
					goto LibFail;
				if(lnp->l_next == NULL && mark->mk_point.off == lnp->l_used) {
					if(dputf(&rpt,"%s  (EOB)",space) != 0)
						goto LibFail;
					}
				else if(lnp->l_used > 0 && (dputs(space,&rpt) != 0 ||
				 dvizs(lnp->l_text,lnp->l_used > max ? max : lnp->l_used,VBaseDef,&rpt) != 0))
					goto LibFail;
				}
			} while((mark = mark->mk_next) != NULL);
		} while((lnp = lnp->l_next) != NULL);

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(buf,rpt.sf_datum->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rval,n,buf,RendNewBuf | RendReset);
	}

// Build and pop up a buffer containing all the global and buffer modes.  Render buffer and return status.
int showModes(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	DStrFab rpt;
	ModeSpec *mspec;
	ModeGrp *mgrp;
	Array *ary;
	Datum *el;
	short c;
	int pagewidth;
	char *space = (term.t_ncol < 96) ? " " : "  ";
	int spacing = (term.t_ncol < 96) ? 1 : 2;
	static ColHdrWidth hcols[] = {
		{4,4},{11,11},{57,15},{0,0}};
	static ColHdrWidth hcolsMG[] = {
		{4,4},{11,11},{57,25},{0,0}};
	char wkbuf[strlen(text437) + strlen(text438) + 1];
			// "AUHL Name          Description"," / Members"
	struct mdinfo {
		char *hdr;
		uint mask;
		} *mdi;
	static struct mdinfo mtab[] = {
		{text364,MdGlobal},
			// "GLOBAL MODES"
		{text365,0},
			// "BUFFER MODES"
		{NULL,0}};
	struct flaginfo {
		short c;
		ushort flag;
		char *desc;
		} *ftp;
	static struct flaginfo flagtab[] = {
		{SMActive,MdEnabled,text31},
				// "Active"
		{SMUser,MdUser,text62},
				// "User defined"
		{SMHidden,MdHidden,text400},
				// "Hidden"
		{SMLocked,MdLocked,text366},
				// "Scope-locked"
		{'\0',0,NULL}};

#if MMDebug & Debug_ModeDump
dumpmodes(NULL,true,"showModes() BEGIN - global modes\n");
dumpmodes(si.curbuf,true,"showModes() BEGIN - buffer modes\n");
#endif
	// Get buffer for the mode list.
	if(sysbuf(text363,&buf,BFTermAttr) != Success)
			// "Modes"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write global modes, then buffer modes.
	mdi = mtab;
	do {
#if MMDebug & Debug_ModeDump
fprintf(logfile,"*** Writing %s modes...\n",mdi->mask == 0 ? "BUFFER" : "GLOBAL");
#endif
		// Write headers.
		if(mdi->mask == 0 && dputs("\n\n",&rpt) != 0)
			goto LibFail;
		if(rpthdr(&rpt,mdi->hdr,false,text437,hcols,&pagewidth) != Success)
				// "AUHL Name          Description"
			return rc.status;

		// Loop through mode table.
		ary = &mi.modetab;
		while((el = aeach(&ary)) != NULL) {
			mspec = msptr(el);

			// Skip if wrong type of mode.
			if((mspec->ms_flags & MdGlobal) != mdi->mask)
				continue;
			ftp = flagtab;
			if(dputc('\n',&rpt) != 0)
				goto LibFail;
			do {
				if(dputc((ftp->c != SMActive || mdi->mask) ? (mspec->ms_flags & ftp->flag ? ftp->c : ' ') :
				(bmsrch1(si.curbuf,mspec) ? ftp->c : ' '),&rpt) != 0)
					goto LibFail;
				} while((++ftp)->desc != NULL);
			if(dputf(&rpt,"%s%-*s",space,hcols[1].minwidth,mspec->ms_name) != 0 ||
			 (mspec->ms_desc != NULL && dputf(&rpt,"%s%s",space,mspec->ms_desc) != 0))
				goto LibFail;
			}
		} while((++mdi)->hdr != NULL);

	// Write mode groups.
#if MMDebug & Debug_ModeDump
fputs("*** Writing mode groups...\n",logfile);
#endif
	if(dputs("\n\n",&rpt) != 0)
		goto LibFail;
	sprintf(wkbuf,"%s%s",text437,text438);
	if(rpthdr(&rpt,text401,false,wkbuf,hcolsMG,NULL) != Success)
			// "MODE GROUPS"
		return rc.status;
	mgrp = mi.ghead;
	do {
		if(dputf(&rpt,"\n %c%*s%s%-*s",mgrp->mg_flags & MdUser ? SMUser : ' ',hcols[0].minwidth - 2,"",space,
		 hcols[1].minwidth,mgrp->mg_name) != 0 ||
		 (mgrp->mg_desc != NULL && dputf(&rpt,"%s%s",space,mgrp->mg_desc) != 0) ||
		 dputf(&rpt,"\n%*s",hcols[0].minwidth + hcols[1].minwidth + spacing + spacing + 4,"") != 0)
			goto LibFail;
		c = '[';
		ary = &mi.modetab;
		while((el = aeach(&ary)) != NULL) {
			if((mspec = msptr(el))->ms_group == mgrp) {
				if(dputc(c,&rpt) != 0 || dputs(mspec->ms_name,&rpt) != 0)
					goto LibFail;
				c = ',';
				}
			}
		if((c == '[' && dputc(c,&rpt) != 0) || dputc(']',&rpt) != 0)
			goto LibFail;
		} while((mgrp = mgrp->mg_next) != NULL);

#if MMDebug & Debug_ModeDump
fputs("*** Writing footnote...\n",logfile);
#endif
	// Write footnote.
	if(sepline(pagewidth,&rpt) != 0)
		goto LibFail;
	ftp = flagtab;
	do {
		if(dputf(&rpt,"\n%c ",(int) ftp->c) != 0 || dputs(ftp->desc,&rpt) != 0)
			goto LibFail;
		} while((++ftp)->desc != NULL);

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		goto LibFail;
	if(bappend(buf,rpt.sf_datum->d_str) != Success)
		return rc.status;

#if MMDebug & Debug_ModeDump
fputs("*** Done!\n",logfile);
#endif
	// Display results.
	return render(rval,n,buf,RendNewBuf | RendReset);
LibFail:
	return librcset(Failure);
	}

// Build and pop up a buffer containing all the strings in given ring.  Render buffer and return status.
int showRing(Datum *rval,int n,Ring *ring) {
	Buffer *buf;
	size_t n1;
	DStrFab rpt;
	char *space = (term.t_ncol < 96) ? " " : "  ";
	char wkbuf[16];
	static ColHdrWidth hcols[] = {
		{5,5},{-1,4},{0,0}};

	// Get buffer for the ring list.
	sprintf(wkbuf,text305,ring->r_rname);
			// "%sRing"
	if(sysbuf(wkbuf,&buf,BFTermAttr) != Success)
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	sprintf(wkbuf,"%s %s",ring->r_rname,text305 + 2);
					// "%sRing"
	if(rpthdr(&rpt,wkbuf,false,text330,hcols,NULL) != Success)
			// "EntryText"
		return rc.status;

	// Loop through ring, beginning at current slot and continuing until we arrive back where we began.
	if(ring->r_entry != NULL) {
		RingEntry *rep;
		size_t max = term.t_ncol * 2;
		int inum = 0;

		rep = ring->r_entry;
		do {
			if(dputf(&rpt,"\n%*d %s",(int) hcols[0].minwidth - 1,inum--,space) != 0)
				goto LibFail;
			if(rep->re_data.d_type != dat_nil && (n1 = strlen(rep->re_data.d_str)) > 0) {
				if(n1 > max)
					n1 = max;
				if(dvizs(rep->re_data.d_str,n1,VBaseDef,&rpt) != 0)
					goto LibFail;
				}

			// Back up to the next kill ring entry.
			} while((rep = rep->re_prev) != ring->r_entry);
		}

	// Add the report to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		goto LibFail;
	if(bappend(buf,rpt.sf_datum->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rval,n,buf,RendNewBuf | RendReset);
LibFail:
	return librcset(Failure);
	}

// Write a separator line to an open StrFab object (in color if possible), given length.  If length is -1, terminal width is
// used.  Return -1 if error.
int sepline(int len,DStrFab *strloc) {

	if(len < 0)
		len = term.t_ncol;
	char line[len + 1];
	dupchr(line,'-',len);
	if(dputc('\n',strloc) != 0)
		return -1;
	return (term.itemColor[ColorIdxInfo].colors[0] >= -1) ? dputf(strloc,"%c%hd%c%s%c%c",AttrSpecBegin,term.maxWorkPair -
	 ColorPairISL,AttrColorOn,line,AttrSpecBegin,AttrColorOff) : dputs(line,strloc);
	}

// Build and pop up a buffer containing a list of all screens and their associated buffers.  Render buffer and return status.
int showScreens(Datum *rval,int n,Datum **argv) {
	Buffer *slist;
	EScreen *scr;			// Pointer to current screen to list.
	EWindow *win;			// Pointer into current screens window list.
	uint wnum;
	int pagewidth;
	DStrFab rpt;
	bool chg = false;		// Screen has a changed buffer?
	char *space = (term.t_ncol < 96) ? " " : "  ";
	int spacing = (term.t_ncol < 96) ? 1 : 2;
	static ColHdrWidth hcols[] = {
		{6,6},{6,6},{MaxBufName - 4,MaxBufName - 4},{32,6},{0,0}};

	// Get a buffer and open a string-fab object.
	if(sysbuf(text160,&slist,BFTermAttr) != Success)
			// "Screens"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;
	initInfoColors();

	// Write headers.
	if(rpthdr(&rpt,text160,false,text89,hcols,&pagewidth) != Success)
			// "Screens","ScreenWindow  Buffer              File"
		return rc.status;

	// For all screens...
	scr = si.shead;
	do {
		// Write separator line.
		if(scr->s_num > 1 && sepline(pagewidth,&rpt) != 0)
			goto LibFail;

		// List screen's window numbers and buffer names.
		wnum = 0;
		win = scr->s_whead;
		do {
			Buffer *buf = win->w_buf;

			// Store screen number if first line.
			if(++wnum == 1) {
				if(dputf(&rpt,"\n%c%c%*hu%c%c  ",AttrSpecBegin,AttrBoldOn,hcols[0].minwidth - 2,scr->s_num,
				 AttrSpecBegin,AttrBoldOff) != 0)
					goto LibFail;
				}
			else if(dputf(&rpt,"\n%*s",hcols[0].minwidth,"") != 0)
				goto LibFail;

			// Store window number and "changed" marker.
			if(buf->b_flags & BFChanged)
				chg = true;
			if(dputf(&rpt,"%s%*u  %.*s%c",space,hcols[1].minwidth - 2,wnum,spacing - 1,space,
			 (buf->b_flags & BFChanged) ? '*' : ' ') != 0)
				goto LibFail;

			// Store buffer name and filename.
			if(buf->b_fname != NULL) {
				if(dputf(&rpt,"%-*s%s%s",hcols[2].minwidth,buf->b_bname,space,buf->b_fname) != 0)
					goto LibFail;
				}
			else if(dputs(buf->b_bname,&rpt) != 0)
				goto LibFail;

			// On to the next window.
			} while((win = win->w_next) != NULL);

		// Store the working directory.
		if(dputf(&rpt,"\n%*s%sCWD: %s",hcols[0].minwidth,"",space,scr->s_wkdir) != 0)
			goto LibFail;

		// On to the next screen.
		} while((scr = scr->s_next) != NULL);

	// Add footnote if applicable.
	if(chg) {
		if(sepline(pagewidth,&rpt) != 0 || dputs(text243,&rpt) != 0)
					// "\n* Changed buffer"
			goto LibFail;
		}

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(slist,rpt.sf_datum->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rval,n,slist,RendNewBuf | RendReset);
	}

// Get next system variable name or description and store in report-control object.  If req is SHReqNext, set *name to NULL if
// no items left; otherwise, pointer to its name.  Return status.
int nextSysVar(ShowCtrl *scp,ushort req,char **name) {
	SVar *svp;

	// First call?
	if(scp->sc_item == NULL)
		scp->sc_item = (void *) (svp = sysvars);
	else {
		svp = (SVar *) scp->sc_item;
		if(req == SHReqNext)
			++svp;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; svp->sv_name != NULL; ++svp) {

				// Found variable... return its name.
				*name = svp->sv_name;
				scp->sc_item = (void *) svp;
				goto Retn;
				}

			// End of list.
			*name = NULL;
			break;
		case SHReqUsage:
			// Set variable name and description.
			if(dsetstr(svp->sv_name,&scp->sc_name) != 0)
				goto LibFail;
			scp->sc_desc = svp->sv_desc;
			*name = svp->sv_name;
			break;
		default: // SHReqValue
			if(svp->sv_flags & (V_GetKey | V_GetKeySeq)) {
				Datum *datum;

				if(dnewtrk(&datum) != 0)
					goto LibFail;
				if(getsvar(datum,svp) == Success && dputf(&scp->sc_rpt,"%c%c%c%s%c%c",
				 AttrSpecBegin,AttrAlt,AttrULOn,datum->d_str,AttrSpecBegin,AttrULOff) != 0)
LibFail:
					(void) librcset(Failure);
				}
			else
				(void) svtosf(&scp->sc_rpt,svp,CvtTermAttr);
		}
Retn:
	return rc.status;
	}

// Get next user variable name or description and store in report-control object.  If req is SHReqNext, set *name to NULL if no
// items left; otherwise, pointer to its name.  Return status.
static int nextUserVar(ShowCtrl *scp,ushort req,char **name,UVar *vhead) {
	UVar *uvar;

	// First call?
	if(scp->sc_item == NULL)
		scp->sc_item = (void *) (uvar = vhead);
	else {
		uvar = (UVar *) scp->sc_item;
		if(req == SHReqNext)
			uvar = uvar->uv_next;
		}

	// Process request.
	switch(req) {
		case SHReqNext:
			for(; uvar != NULL; uvar = uvar->uv_next) {

				// Found variable... return its name.
				*name = uvar->uv_name;
				scp->sc_item = (void *) uvar;
				goto Retn;
				}

			// End of list.
			*name = NULL;
			break;
		case SHReqUsage:
			if(dsetstr(uvar->uv_name,&scp->sc_name) != 0)
				return librcset(Failure);
			*name = uvar->uv_name;
			break;
		default: // SHReqValue
			if(dtosfchk(&scp->sc_rpt,uvar->uv_datum,NULL,CvtTermAttr | CvtExpr | CvtForceArray) != Success)
				return rc.status;
		}
Retn:
	return rc.status;
	}

// Get next global variable name or description via call to nextUserVar().
int nextGlobalVar(ShowCtrl *scp,ushort req,char **name) {

	return nextUserVar(scp,req,name,gvarshead);
	}

// Get next local variable name or description via call to nextUserVar().
int nextLocalVar(ShowCtrl *scp,ushort req,char **name) {

	return nextUserVar(scp,req,name,lvarshead);
	}

// Create formatted list of system and user variables via calls to "show" routines.  Return status.
int showVariables(Datum *rval,int n,Datum **argv) {
	ShowCtrl sc;

	// Open control object.
	if(showopen(&sc,n,text292,argv) != Success)
			// "variable"
		return rc.status;

	// Do system variables.
	if(showbuild(&sc,SHSepLine,text21,nextSysVar) != Success)
				// "system variable"
		return rc.status;

	// Do user variables.
	if(showbuild(&sc,SHNoDesc,text56,nextGlobalVar) == Success && showbuild(&sc,0,NULL,nextLocalVar) == Success)
				// "user variable"
		(void) showclose(rval,n,&sc);
	return rc.status;
	}

#if MMDebug & Debug_ShowRE
// Build and pop up a buffer containing the search and replacement metacharacter arrays.  Render buffer and return status.
int showRegexp(Datum *rval,int n,Datum **argv) {
	uint m;
	Buffer *srlist;
	MetaChar *mcp;
	ReplMetaChar *rmc;
	DStrFab rpt;
	char *bm,*bmz,*str,*lit,wkbuf[NWork];
	char patbuf[srch.m.patlen + OptCh_N + 1];
	struct mcinfo {
		char *hdr;
		MetaChar *pat;
		} *mcobj;
	static struct mcinfo mctab[] = {
		{text997,NULL},
			// "Forward"
		{text998,NULL},
			// "Backward"
		{NULL,NULL}};

	// Get a buffer and open a string-fab object.
	if(sysbuf(text996,&srlist,0) != Success)
			// "RegexpInfo"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;

	if(dputf(&rpt,"Match flags: %.4x\n\n",srch.m.flags) != 0)
		goto LibFail;
	mkpat(patbuf,&srch.m);
	mctab[0].pat = srch.m.mcpat;
	mctab[1].pat = srch.m.bmcpat;
	mcobj = mctab;
	do {
		// Construct the search header lines.
		if(mcobj->pat == srch.m.bmcpat && dputs("\n\n",&rpt) != 0)
			goto LibFail;
		if(dputf(&rpt,"%s %s %s /",mcobj->hdr,text994,text999) != 0 || dvizs(patbuf,0,VBaseDef,&rpt) != 0 ||
		 dputs("/\n",&rpt) != 0)
						// "SEARCH","PATTERN"
			goto LibFail;

		// Loop through search pattern.
		mcp = mcobj->pat;
		for(;;) {
			str = stpcpy(wkbuf,"    ");

			// Any closure modifiers?
			if(mcp->mc_type & MCE_Closure) {
				str = stpcpy(wkbuf,"    ");
				sprintf(str,"%hd",mcp->cl.min);
				str = strchr(str,'\0');
				if(mcp->cl.max != mcp->cl.min) {
					if(mcp->cl.max < 0)
						str = stpcpy(str," or more");
					else {
						sprintf(str," to %hd",mcp->cl.max);
						str = strchr(str,'\0');
						}
					}
				*str++ = ' ';
				if(mcp->mc_type & MCE_MinClosure)
					str = stpcpy(str,"(minimum) ");
				strcpy(str,"of:\n");
				if(dputs(wkbuf,&rpt) != 0)
					goto LibFail;
				str = stpcpy(wkbuf,"        ");
				}

			// Describe metacharacter and any value.
			switch(mcp->mc_type & MCE_BaseType) {
				case MCE_Nil:
					lit = "NIL";
					goto Lit;
				case MCE_LitChar:
					sprintf(str,"%-14s'%c'","Char",mcp->u.lchar);
					break;
				case MCE_Any:
					lit = "Any";
					goto Lit;
				case MCE_CCL:
					lit = "ChClass      ";
					goto ChClass;
				case MCE_NCCL:
					lit = "NegChClass   ";
ChClass:
					if(dputs(wkbuf,&rpt) != 0 || dputs(lit,&rpt) != 0)
						goto LibFail;
					bmz = (bm = (char *) mcp->u.cclmap) + sizeof(EBitMap);
					m = 0;
					do {
						if(m ^= 1)
							if(dputc(' ',&rpt) != 0)
								goto LibFail;
						if(dputf(&rpt,"%.2x",*bm++) != 0)
							goto LibFail;
						} while(bm < bmz);
					if(dputc('\n',&rpt) != 0)
						goto LibFail;
					str = NULL;
					break;
				case MCE_WordBnd:
					lit = (mcp->mc_type & MCE_Not) ? "NotWordBoundary" : "WordBoundary";
					goto Lit;
				case MCE_BOL:
					lit = "BeginLine";
					goto Lit;
				case MCE_EOL:
					lit = "EndLine";
					goto Lit;
				case MCE_BOS:
					lit = "BeginString";
					goto Lit;
				case MCE_EOS:
					lit = "EndString";
					goto Lit;
				case MCE_EOSAlt:
					lit = "EndStringCR";
Lit:
					strcpy(str,lit);
					break;
				case MCE_GrpBegin:
					lit = "GroupBegin";
					goto Grp;
				case MCE_GrpEnd:
					lit = "GroupEnd";
Grp:
					sprintf(str,"%-14s%3u",lit,(uint) (mcp->u.ginfo - srch.m.groups));
				}

			// Add the line to the string-fab object.
			if(str != NULL && (dputs(wkbuf,&rpt) != 0 || dputc('\n',&rpt) != 0))
				goto LibFail;

			// Onward to the next element.
			if(mcp->mc_type == MCE_Nil)
				break;
			++mcp;
			}
		} while((++mcobj)->hdr != NULL);

	// Construct the replacement header lines.
	if(dputf(&rpt,"\n\n%s %s /",text995,text999) != 0 || dvizs(srch.m.rpat,0,VBaseDef,&rpt) != 0 ||
	 dputs("/\n",&rpt) != 0)
				// "REPLACE","PATTERN"
		goto LibFail;

	// Loop through replacement pattern.
	rmc = srch.m.rmcpat;
	for(;;) {
		str = stpcpy(wkbuf,"    ");

		// Describe metacharacter and any value.
		switch(rmc->mc_type) {
			case MCE_Nil:
				strcpy(str,"NIL");
				break;
			case MCE_LitString:
				if(dputs(wkbuf,&rpt) != 0 || dputf(&rpt,"%-14s'","String") != 0 ||
				 dvizs(rmc->u.rstr,0,VBaseDef,&rpt) != 0 || dputs("'\n",&rpt) != 0)
					goto LibFail;
				str = NULL;
				break;
			case MCE_Group:
				sprintf(str,"%-14s%3d","Group",rmc->u.grpnum);
				break;
			case MCE_Match:
				strcpy(str,"Matched string");
			}

		// Add the line to the string-fab object.
		if(str != NULL && (dputs(wkbuf,&rpt) != 0 || dputc('\n',&rpt) != 0))
			goto LibFail;

		// Onward to the next element.
		if(rmc->mc_type == MCE_Nil)
			break;
		++rmc;
		}

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(srlist,rpt.sf_datum->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rval,n,srlist,RendNewBuf | RendReset);
	}
#endif
