// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// buffer.c	Buffer management routines for MightEMacs.
//
// Some of the functions are internal, and some are attached to user keys.

#include "os.h"
#include <stdarg.h>
#include "pllib.h"
#include "std.h"
#include "cmd.h"
#include "bind.h"
#include "exec.h"
#include "file.h"

#if MMDebug & Debug_ModeDump
// Dump mode table or buffer modes to log file.
static void dumpmode(ModeSpec *mspec) {

	fprintf(logfile,"  %-16s%.4hX  %c%c%c%c  %-16s  %s\n",mspec->ms_name,mspec->ms_flags,
	 mspec->ms_flags & MdUser ? 'U' : '.',
	 mspec->ms_flags & MdGlobal ? 'G' : '.',
	 mspec->ms_flags & MdHidden ? 'H' : '.',
	 mspec->ms_flags & MdEnabled ? SMActive : '.',
	 mspec->ms_group == NULL ? "NULL" : mspec->ms_group->mg_name,
	 mspec->ms_desc == NULL ? "NULL" : mspec->ms_desc);
	}

void dumpmodes(Buffer *buf,bool sepline,char *fmt,...) {
	va_list ap;

	fputs(sepline ? "----------\n" : "\n",logfile);
	va_start(ap,fmt);
	if(buf == NULL) {
		Datum *el;
		Array *ary = &mi.modetab;

		fputs("MODE TABLE - ",logfile);
		vfprintf(logfile,fmt,ap);
		while((el = aeach(&ary)) != NULL)
			dumpmode(msptr(el));
		}
	else {
		BufMode *bmp = buf->b_modes;
		fprintf(logfile,"BUFFER '%s': ",buf->b_bname);
		vfprintf(logfile,fmt,ap);
		if(bmp == NULL)
			fputs("\t*None set*\n",logfile);
		else
			do
				dumpmode(bmp->bm_mode);
				while((bmp = bmp->bm_next) != NULL);
		}
	va_end(ap);
	}
#endif

// Check if any given mode (ModeSpec pointers in vararg list) is set in a buffer and return Boolean result.  If "clear" is true,
// also clear each mode that is found.
bool bmsrch(Buffer *buf,bool clear,int modect,...) {
	BufMode *bmp0,*bmp1,*bmp2;
	ModeSpec *mspec;
	va_list ap;
	int ct;
	bool modeFound = false;
	bool updateML = false;

#if MMDebug & Debug_Modes
	ModeSpec *msp2;
	if(clear)
		dumpmodes(buf,true,"bmsrch(...,CLEAR,%d,...)\n",modect);
#endif
	// For each buffer mode...
	bmp0 = NULL;
	bmp1 = buf->b_modes;
	while(bmp1 != NULL) {
		bmp2 = bmp1->bm_next;
		mspec = bmp1->bm_mode;
#if MMDebug & Debug_Modes
		if(clear)
			fprintf(logfile,"    Found enabled buffer mode '%s'...\n",mspec->ms_name);
#endif
		ct = modect;
		va_start(ap,modect);

		// For each mode argument...
		do {
#if MMDebug & Debug_Modes
			msp2 = va_arg(ap,ModeSpec *);
			if(clear)
				fprintf(logfile,"\tgot mode argument '%s'...\n",msp2->ms_name);
			if(msp2 == mspec) {
#else
			if(va_arg(ap,ModeSpec *) == mspec) {
#endif
				if(!clear)
					return true;
#if MMDebug & Debug_Modes
				fprintf(logfile,"    match found.  Clearing mode '%s'...\n",mspec->ms_name);
#endif
				// Remove mode record from linked list.
				if(bmp0 == NULL)
					buf->b_modes = bmp2;
				else
					bmp0->bm_next = bmp2;
				free((void *) bmp1);

				modeFound = true;

				// Check if mode line needs an update.
				if((mspec->ms_flags & (MdHidden | MdInLine)) != MdHidden)
					updateML = true;
				goto Next;
				}
			} while(--ct > 0);
		va_end(ap);

		// Advance pointers.
		bmp0 = bmp1;
Next:
		bmp1 = bmp2;
		}

	// Set window flags if a visible mode was cleared.
	if(updateML)
		supd_wflags(buf,WFMode);

#if MMDebug & Debug_Modes
	if(clear)
		dumpmodes(buf,false,"bmsrch() END\n");
#endif
	return modeFound;
	}

// Check if given mode (ModeSpec pointer) is set in a buffer and return Boolean result.  This is a fast version for checking
// a single mode.
bool bmsrch1(Buffer *buf,ModeSpec *mspec) {
	BufMode *bmp;

	// For each buffer mode...
	for(bmp = buf->b_modes; bmp != NULL; bmp = bmp->bm_next)
		if(bmp->bm_mode == mspec)
			return true;
	// Not found.
	return false;
	}

// Clear all modes in given buffer.  Return true if any mode was enabled; otherwise, false.
bool bmclear(Buffer *buf) {
	BufMode *bmp1 = buf->b_modes,*bmp2;
	bool modeWasChanged = (bmp1 != NULL);
	bool updateML = false;

#if MMDebug & Debug_Modes
	dumpmodes(buf,true,"bmclear() BEGIN\n");
#endif
	while(bmp1 != NULL) {
		bmp2 = bmp1->bm_next;
		if((bmp1->bm_mode->ms_flags & (MdHidden | MdInLine)) != MdHidden)
			updateML = true;
		free((void *) bmp1);
		bmp1 = bmp2;
		}
	buf->b_modes = NULL;
	if(updateML)
		supd_wflags(buf,WFMode);
#if MMDebug & Debug_Modes
	dumpmodes(buf,false,"bmclear() END\n");
#endif
	return modeWasChanged;
	}

// Clear a mode in all buffers.
void bmclearall(ModeSpec *mspec) {
	Datum *el;
	Array *ary = &buftab;
	while((el = aeach(&ary)) != NULL)
		bmsrch(bufptr(el),true,1,mspec);
	}

// Set a mode in a buffer.  If clear is true, clear all existing modes first.  If clear is false and wasSetp not NULL, set
// *wasSetp to true if mode was already set; otherwise, false.  It is assumed that mode points to a valid buffer mode in the
// mode table.  Return status.
int bmset(Buffer *buf,ModeSpec *mspec,bool clear,bool *wasSetp) {
	BufMode *bmp0,*bmp1,*bmp2;
	bool wasSet = false;

	if(clear)						// "Clear modes" requested?
		bmclear(buf);					// Yes, nuke 'em.
	else if(bmsrch1(buf,mspec)) {				// No.  Is mode already set?
		wasSet = true;					// Yes.
		goto Retn;
		}

#if MMDebug & Debug_Modes
	dumpmodes(buf,true,"bmset(...,%s,%s,...)\n",mspec->ms_name,clear ? "CLEAR" : "false");
#endif
	// Insert mode in linked list alphabetically and return.
	if((bmp2 = (BufMode *) malloc(sizeof(BufMode))) == NULL)
		return rcset(Panic,0,text94,"bmset");
			// "%s(): Out of memory!"
	bmp2->bm_mode = mspec;
	bmp0 = NULL;
	bmp1 = buf->b_modes;
	while(bmp1 != NULL) {
		if(strcasecmp(bmp1->bm_mode->ms_name,mspec->ms_name) > 0)
			break;
		bmp0 = bmp1;
		bmp1 = bmp1->bm_next;
		}
	if(bmp0 == NULL)
		buf->b_modes = bmp2;
	else
		bmp0->bm_next = bmp2;
	bmp2->bm_next = bmp1;
	if((mspec->ms_flags & (MdHidden | MdInLine)) != MdHidden)
		supd_wflags(buf,WFMode);
#if MMDebug & Debug_Modes
	dumpmodes(buf,false,"bmset() END\n");
#endif
Retn:
	if(wasSetp != NULL)
		*wasSetp = wasSet;
	return rc.status;
	}

// Clear a buffer's filename, if any.
void clfname(Buffer *buf) {

	if(buf->b_fname != NULL) {
		(void) free((void *) buf->b_fname);
		buf->b_fname = NULL;
		}
	}

// Set buffer filename if possible and execute "filename" hook if filename was changed and not a macro.  Return status.
int setfname(Buffer *buf,char *fname) {
	bool fnChange = false;
	bool oldIsNull = (buf->b_fname == NULL);
	bool newIsNull = (fname == NULL || *fname == '\0');

	if(oldIsNull) {
		if(!newIsNull) {
			fnChange = true;
			goto ClearSet;
			}
		}
	else if(newIsNull) {
		clfname(buf);
		fnChange = true;
		}
	else {
		if(fnChange = (strcmp(buf->b_fname,fname) != 0)) {
ClearSet:
			clfname(buf);
			if((buf->b_fname = (char *) malloc(strlen(fname) + 1)) == NULL)
				return rcset(Panic,0,text94,"setfname");
					// "%s(): Out of memory!"
			strcpy(buf->b_fname,fname);
			}
		}

	if(!(buf->b_flags & BFMacro) && fnChange)
		(void) exechook(NULL,INT_MIN,hooktab + HkFilename,2,buf->b_bname,buf->b_fname);

	return rc.status;
	}

// Invalidate "last screen" pointer in any buffer pointing to given screen.
void nukebufsp(EScreen *scr) {
	Datum *el;
	Buffer *buf;
	Array *ary = &buftab;
	while((el = aeach(&ary)) != NULL) {
		buf = bufptr(el);
		if(buf->b_lastscr == scr)
			buf->b_lastscr = NULL;
		}
	}

// Get the default buffer (a guess) for various buffer commands.  If only two visible buffers exist (active or inactive), return
// first one found that is not the current buffer; otherwise, NULL.
Buffer *bdefault(void) {
	Datum *el;
	Buffer *buf,*bufp1 = NULL;
	ushort count = 0;
	Array *ary = &buftab;

	// Scan buffer list for visible buffers.
	while((el = aeach(&ary)) != NULL) {
		buf = bufptr(el);
		if(!(buf->b_flags & BFHidden)) {
			++count;
			if(buf != si.curbuf && bufp1 == NULL)
				bufp1 = buf;
			}
		}
	return (count == 2) ? bufp1 : NULL;
	}

#if 0
static void lput(Line *lnp) {

	if(lnp == NULL)
		fputs("NULL'\n",logfile);
	else
		fprintf(logfile,"%.*s'\n",lnp->l_used,lnp->l_text);
	}

static void ldump(char *name,Line *lnp) {

	fprintf(logfile,"*** %s (%.08x) = '",name,(uint) lnp);
	if(lnp == NULL)
		fputs("'\n",logfile);
	else {
		lput(lnp);
		fprintf(logfile,"\tl_next (%.08x) = '",(uint) lnp->l_next);
		if(lnp->l_next == NULL)
			fputs("'\n",logfile);
		else
			lput(lnp->l_next);
		fprintf(logfile,"\tl_prev (%.08x) = '",(uint) lnp->l_prev);
		if(lnp->l_prev == NULL)
			fputs("'\n",logfile);
		else
			lput(lnp->l_prev);
		}
	}
#endif

// Check if given buffer is empty and return Boolean result.  If buf is NULL, check current buffer.
bool bempty(Buffer *buf) {
	Line *lnp;

	if(buf == NULL)
		buf = si.curbuf;
	lnp = buf->b_lnp;
	return (lnp->l_next == NULL && lnp->l_used == 0);
	}

// Return true if point is at beginning of current buffer; otherwise, false.  If point is NULL, use point in current window.
bool bufbegin(Point *point) {

	if(point == NULL)
		point = &si.curwin->w_face.wf_point;
	return point->lnp == si.curbuf->b_lnp && point->off == 0;
	}

// Return true if point is at end of current buffer; otherwise, false.  If point is NULL, use point in current window.
bool bufend(Point *point) {

	if(point == NULL)
		point = &si.curwin->w_face.wf_point;
	return point->lnp->l_next == NULL && point->off == point->lnp->l_used;
	}

// Inactivate all user marks that are outside the current narrowed buffer by negating their point offsets.
static void mrkoff(void) {
	Mark *mark;
	Line *lnp;

	// First, inactivate all user marks in current buffer.
	mark = &si.curbuf->b_mroot;
	do {
		if(mark->mk_id <= '~')
			mark->mk_point.off = -(mark->mk_point.off + 1);
		} while((mark = mark->mk_next) != NULL);

	// Now scan the buffer and reactivate the marks that are still in the narrowed region.
	lnp = si.curbuf->b_lnp;
	do {
		// Any mark match this line?
		mark = &si.curbuf->b_mroot;
		do {
			if(mark->mk_point.lnp == lnp && mark->mk_point.off < 0)
				mark->mk_point.off = -mark->mk_point.off - 1;
			} while((mark = mark->mk_next) != NULL);
		} while((lnp = lnp->l_next) != NULL);
	}

// Narrow to lines or region.  Makes all but the specified line(s) in the current buffer hidden and inaccessible.  Set rval to
// buffer name and return status.  Note that the last line in the narrowed portion of the buffer inherits the "last line of
// buffer" property; that is, it is assumed to not end with a newline delimiter.
int narrowBuf(Datum *rval,int n,Datum **argv) {
	EScreen *scr;
	EWindow *win;
	Mark *mark;
	Line *lnp,*lnp1,*lnpz;
	char *emsg;
	Point *point = &si.curwin->w_face.wf_point;

	// Make sure we aren't already narrowed or buffer is empty.
	if(si.curbuf->b_flags & BFNarrowed) {
		emsg = text71;
			// "%s '%s' is already narrowed"
		goto ErrRtn;
		}
	if(bempty(NULL)) {
		emsg = text377;
			// "%s '%s' is empty"
ErrRtn:
		return rcset(Failure,0,emsg,text58,si.curbuf->b_bname);
					// "Buffer"
		}

#if MMDebug & Debug_Narrow
	dumpbuffer("narrowBuf(): BEFORE",NULL,true);
#endif
	// Save faces of all windows displaying current buffer in a mark so they can be restored when buffer is widened.

	// In all screens...
	scr = si.shead;
	do {
		// In all windows...
		win = scr->s_whead;
		do {
			if(win->w_buf == si.curbuf) {

				// Found window attached to current buffer.  Save its face using the window's mark id.
				if(mfind(win->w_id,&mark,MkOpt_Create) != Success)
					return rc.status;
				mset(mark,win);
				}
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);

	// Get the boundaries of the current region, if requested.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0) {
		int i = n;

		// Move back abs(n) lines max.
		n = 1;
		while(point->lnp != si.curbuf->b_lnp) {
			point->lnp = point->lnp->l_prev;
			++n;
			if(++i == 0)
				break;
			}
		}
	else if(n == 0 && reglines(&n) != Success)
		return rc.status;

	// Current line is now at top of area to be narrowed and n is the number of lines (forward).
	lnp = point->lnp;
	lnp1 = si.curbuf->b_lnp;						// Save original first line of buffer...
	lnpz = lnp1->l_prev;						// and last line.

	// Archive the top fragment (hidden portion of buffer above narrowed region).
	if(lnp == lnp1)							// If current line is first line of buffer...
		si.curbuf->b_ntoplnp = NULL;				// set old first line to NULL (no top fragment).
	else {
		si.curbuf->b_ntoplnp = lnp1;				// Otherwise, save old first line of buffer...
		si.curbuf->b_lnp = lnp;					// set new first line...
		lnp1->l_prev = lnp->l_prev;				// and save pointer to last line of fragment.
		}

	// Move point forward to line just past the end of the narrowed region.
	do {
		if((point->lnp = point->lnp->l_next) == NULL) {	// If narrowed region extends to bottom of buffer...
			si.curbuf->b_nbotlnp = NULL;		// set old first line to NULL (no bottom fragment)...
			si.curbuf->b_lnp->l_prev = lnpz;	// and point narrowed first line to original last line.
			goto FixMarks;
			}
		} while(--n > 0);

	// Narrowed region stops before EOB.  Archive the bottom fragment (hidden portion of buffer below narrowed region).
	(si.curbuf->b_nbotlnp = point->lnp)->l_prev->l_next = NULL;
								// Save first line of fragment, terminate line above it...
	si.curbuf->b_lnp->l_prev = point->lnp->l_prev;		// set new last line of buffer...
	point->lnp->l_prev = lnpz;				// and save pointer to last line of fragment.
FixMarks:
	// Inactivate marks outside of narrowed region.
	mrkoff();

	// In all screens...
	scr = si.shead;
	do {
		// In all windows...
		win = scr->s_whead;
		do {
			if(win->w_buf == si.curbuf) {

				// Found window attached to narrowed buffer.  Update its buffer settings.
				win->w_face.wf_toplnp = win->w_face.wf_point.lnp = lnp;
				win->w_face.wf_point.off = win->w_face.wf_firstcol = 0;
				win->w_flags |= (WFHard | WFMode);
				}
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);

#if MMDebug & Debug_Narrow
#if 0
	ldump("si.curbuf->b_ntoplnp",si.curbuf->b_ntoplnp);
	ldump("si.curbuf->b_nbotlnp",si.curbuf->b_nbotlnp);
	ldump("si.curbuf->b_lnp",si.curbuf->b_lnp);
	ldump("lnp",lnp);
#else
	dumpbuffer("narrowBuf(): AFTER",NULL,true);
#endif
#endif
	// and now remember we are narrowed.
	si.curbuf->b_flags |= BFNarrowed;

	return dsetstr(si.curbuf->b_bname,rval) != 0 ? librcset(Failure) : rcset(Success,0,text73,text58);
								// "%s narrowed","Buffer"
	}

// Restore a buffer to its pre-narrowed state.
static void unnarrow(Buffer *buf) {
	EScreen *scr;
	EWindow *win;
	Line *lnp,*lnp1,*lnpz;
	Mark *mark;

#if MMDebug & Debug_Narrow
	dumpbuffer("unnarrow(): BEFORE",NULL,true);
#endif
	// Get narrowed first and last lines.
	lnp1 = buf->b_lnp;
	lnpz = lnp1->l_prev;

	// Recover the top fragment.
	if(buf->b_ntoplnp != NULL) {
		lnp = (buf->b_lnp = buf->b_ntoplnp)->l_prev;
							// Point buffer to real first line, get last line of fragment...
		lnp->l_next = lnp1;			// point last line and narrowed first line to each other...
		lnp1->l_prev = lnp;
		buf->b_ntoplnp = NULL;			// and deactivate top fragment.
		}

	// Recover the bottom fragment.
	if(buf->b_nbotlnp == NULL)			// If none...
		buf->b_lnp->l_prev = lnpz;		// point recovered first line going backward to real last line.
	else {						// otherwise, adjust point and marks.
		// Connect the bottom fragment.
		lnp = buf->b_nbotlnp->l_prev;		// Get last line of fragment.
		lnpz->l_next = buf->b_nbotlnp;	// Point narrowed last line and first line of fragment to each other...
		buf->b_nbotlnp->l_prev = lnpz;
		buf->b_lnp->l_prev = lnp;		// point first line going backward to last line...
		buf->b_nbotlnp = NULL;			// and deactivate bottom fragment.
		}

	// Activate all marks in buffer.
	mark = &buf->b_mroot;
	do {
		if(mark->mk_point.off < 0)
			mark->mk_point.off = -mark->mk_point.off - 1;
		} while((mark = mark->mk_next) != NULL);

	// Restore faces of all windows displaying current buffer from the window's mark if it exists.

	// In all screens...
	scr = si.shead;
	do {
		// In all windows...
		win = scr->s_whead;
		do {
			if(win != si.curwin && win->w_buf == si.curbuf) {

				// Found window attached to widened buffer.  Restore its face.
				(void) mfind(win->w_id,&mark,MkOpt_Wind);	// Can't return an error.
				if(mark != NULL) {
					win->w_face.wf_point = mark->mk_point;
					win->w_rfrow = mark->mk_rfrow;
					win->w_flags |= WFReframe;
					}
				}
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);

	// Set hard update in front screen only...
	supd_wflags(buf,WFHard | WFMode);

#if MMDebug & Debug_Narrow
	dumpbuffer("unnarrow(): AFTER",NULL,true);
#endif
	// and now forget that we are narrowed.
	buf->b_flags &= ~BFNarrowed;
	}

// Widen (restore) a narrowed buffer.  Set rval to buffer name and return status.
int widenBuf(Datum *rval,int n,Datum **argv) {

	// Make sure we are narrowed.
	if(!(si.curbuf->b_flags & BFNarrowed))
		return rcset(Failure,0,text74,text58,si.curbuf->b_bname);
			// "%s '%s' is not narrowed","Buffer"

	// Restore current buffer to pre-narrowed state.
	unnarrow(si.curbuf);
	if(dsetstr(si.curbuf->b_bname,rval) != 0)
		return librcset(Failure);
	(void) rcset(Success,0,text75,text58);
		// "%s widened","Buffer"
	return execCF(rval,INT_MIN,cftab + cf_reframeWind,0,0);
	}

// binsearch() helper function for returning a buffer name, given table (array) and index.
static char *bufname(void *table,ssize_t i) {

	return bufptr(((Datum **) table)[i])->b_bname;
	}

// Search the buffer list for given name and return pointer to Buffer object if found; otherwise, NULL.  In either case, set
// *index to target array index if index not NULL.
Buffer *bsrch(char *bname,ssize_t *index) {

	// Check current buffer name first, being that it is often "searched" for (but only if index not requested).
	if(index == NULL && strcmp(bname,si.curbuf->b_bname) == 0)
		return si.curbuf;

	// No go... search the buffer list.
	ssize_t i;
	bool found = binsearch(bname,(void *) buftab.a_elp,buftab.a_used,strcmp,bufname,&i);
	if(index != NULL)
		*index = i;
	return found ? bufptr(buftab.a_elp[i]) : NULL;
	}

// Generate a valid buffer name from a pathname.  bname is assumed to point to a buffer that is MaxBufName + 1 or greater in
// size.
static char *fbname(char *bname,char *fname) {
	char *str;

	// Get file basename and validate it.  Keep filename extension if it's numeric.
	stplcpy(bname,fbasename(fname,(str = strrchr(fname,'.')) != NULL && asc_long(str + 1,NULL,true) ? true : false),
	 MaxBufName + 1);
	if(*bname == ' ' || *bname == SBMacro)		// Convert any leading space or macro character...
		*bname = AltBufCh;
	stripstr(bname,1);				// remove any trailing white space...
	str = bname;					// convert any non-printable characters...
	do {
		if(*str < ' ' || *str > '~')
			*str = AltBufCh;
		} while(*++str != '\0');
	return bname;					// and return result.
	}

// Generate a unique buffer name (from fname if not NULL) by appending digit(s) if needed and return it.  The string buffer
// that bname points to is assumed to be of size MaxBufName + 1 or greater.
static char *bunique(char *bname,char *fname) {
	int n,i;
	char *str0,*str1,*strz = bname + MaxBufName;
	char wkbuf[sizeof(int) * 3];

	// Begin with file basename.
	if(fname != NULL)
		fbname(bname,fname);

	// Check if name is already in use.
	while(bsrch(bname,NULL) != NULL) {

		// Name already exists.  Strip off trailing digits, if any.
		str1 = strchr(bname,'\0');
		while(str1 > bname && str1[-1] >= '0' && str1[-1] <= '9')
			--str1;
		str0 = str1;
		n = i = 0;
		while(*str1 != '\0') {
			n = n * 10 + (*str1++ - '0');
			i = 1;
			}

		// Add 1 to numeric suffix (or begin at 0), and put it back.  Assume that MaxBufName is >= number of bytes
		// needed to encode an int in decimal form (10 for 32-bit int) and that we will never have more than INT_MAX
		// buffers (> 2 billion).
		long_asc((long) (n + i),wkbuf);
		n = strlen(wkbuf);
		strcpy(str0 + n > strz ? strz - n : str0,wkbuf);
		}

	return bname;
	}

// Remove buffer from buffer list and return its Datum object.
static Datum *delistbuf(Buffer *buf) {
	Datum *datum;
	ssize_t index;

	(void) bsrch(buf->b_bname,&index);		// Can't fail.
	datum = adelete(&buftab,index);
	return datum;
	}

// Insert buffer into buffer list at given index.  Return status.
static int enlistbuf(Datum *datum,ssize_t index) {

	if(ainsert(&buftab,index,datum,false) != 0)
		(void) librcset(Failure);
	return rc.status;
	}

// Initialize point position, marks, first column position, and I/O delimiters of a buffer.
static void bufinit(Buffer *buf,Line *lnp) {

	faceinit(&buf->b_face,lnp,buf);
	*buf->b_inpdelim = '\0';
	buf->b_inpdelimlen = 0;
	}

// Check if given buffer name is valid and return Boolean result.
static bool isbname(char *name) {
	char *str;

	// Null?
	if(*name == '\0')
		return false;

	// All printable characters?
	str = name;
	do {
		if(*str < ' ' || *str > '~')
			return false;
		} while(*++str != '\0');

	// First or last character a space?
	if(*name == ' ' || str[-1] == ' ')
		return false;

	// All checks passed.
	return true;
	}

// Create buffer-extension record (MacInfo) for given buffer.  Return status.
int bextend(Buffer *buf) {
	MacInfo *mip;

	if((buf->b_mip = mip = (MacInfo *) malloc(sizeof(MacInfo))) == NULL)
		return rcset(Panic,0,text94,"bextend");
			// "%s(): Out of memory!"
	mip->mi_minArgs = 0;
	mip->mi_maxArgs = -1;
	mip->mi_nexec = 0;
	mip->mi_exec = NULL;
	dinit(&mip->mi_usage);
	dinit(&mip->mi_desc);
	return rc.status;
	}

// Find a buffer by name and return status or Boolean result.  Actions taken depend on cflags:
//	If BS_Derive is set:
//		Use the base filename of "name" as the default buffer name; otherwise, use "name" directly.
//	If BS_Force is set:
//		Create a unique buffer name derived from the default one.  (BS_Create is assumed to also be set.)
//	If BS_Create is set:
//		If the buffer is found and BS_Force is not set:
//			Set *bufp (if bufp not NULL) to the buffer pointer, set *created (if created not NULL) to false,
//			and return status.
//		If the buffer is not found or BS_Force is set:
//			Create a buffer, set it's flag word to bflags, create a buffer-extension record if BS_Extend is set,
//			run "createBuf" hook if BS_Hook is set, set *bufp (if bufp not NULL) to the buffer pointer, set
//			*created (if created not NULL) to true, and return status.
//	If BS_Create is not set:
//		If the buffer is found:
//			Set *bufp (if bufp not NULL) to the buffer pointer and return true, ignoring created.
//		If the buffer is not found:
//			Return false, ignoring bufp and created.
// In all cases, if a buffer is created and BS_Hook is set, the "createBuf" hook is executed before returning.
int bfind(char *name,ushort cflags,ushort bflags,Buffer **bufp,bool *created) {
	ssize_t index;
	Buffer *buf;
	char *bname,wkbuf[MaxBufName + 1];

	// Set default buffer name.
	if(cflags & BS_Force)
		(void) bsrch(bname = (cflags & BS_Derive) ? bunique(wkbuf,name) : bunique(strcpy(wkbuf,name),NULL),
		cflags & BS_Create ? &index : NULL);
	else {
		if(cflags & BS_Derive)
			fbname(bname = wkbuf,name);
		else
			bname = name;

		// Search for the buffer.
		if((buf = bsrch(bname,cflags & BS_Create ? &index : NULL)) != NULL) {

			// Found it.  Return results.
			if(bufp != NULL)
				*bufp = buf;
			if(cflags & BS_Create) {
				if(created != NULL)
					*created = false;
				return rc.status;
				}
			return true;
			}
		}

	// No such buffer exists, create it?
	if(cflags & BS_Create) {
		Line *lnp;
		Datum *datum;
		Buffer buf;

		// Valid buffer name?
		if(!isbname(bname))
			return rcset(Failure,0,text447,text128,bname);
					// "Invalid %s '%s'","buffer name"
		// Macro name?
		if(*bname == SBMacro && !(bflags & BFMacro))
			return rcset(Failure,0,text268,text180,bname,SBMacro);
					// "Cannot %s buffer: name '%s' cannot begin with %c","create"

		// Allocate memory for the first line.
		if(lalloc(0,&lnp) != Success)
			return rc.status;	// Fatal error.
		buf.b_lnp = lnp->l_prev = lnp;
		lnp->l_next = NULL;

		// Create buffer extension if requested...
		if(!(cflags & BS_Extend))
			buf.b_mip = NULL;
		else if(bextend(&buf) != Success)
			return rc.status;

		// and set up the other buffer fields.
		buf.b_mroot.mk_next = NULL;
		bufinit(&buf,lnp);
		buf.b_ntoplnp = buf.b_nbotlnp = NULL;
		buf.b_flags = bflags | BFActive;
		buf.b_modes = NULL;
		buf.b_nwind = buf.b_nalias = 0;
		buf.b_lastscr = NULL;
		buf.b_fname = NULL;
		strcpy(buf.b_bname,bname);

		// Insert a copy of the Buffer object into the list using slot index from bsrch() call and an
		// untracked Datum object.
		if(dnew(&datum) != 0 || dsetblob((void *) &buf,sizeof(Buffer),datum) != 0)
	 		return librcset(Failure);
	 	if(enlistbuf(datum,index) != Success)
	 		return rc.status;

		// Add macro name to exectab hash...
		if(bflags & BFMacro) {
			UnivPtr univ;
			Buffer *tbuf = bufptr(datum);

			univ.p_type = (tbuf->b_flags & BFConstrain) ? PtrMacro_C : PtrMacro_O;
			if(execfind((univ.u.p_buf = tbuf)->b_bname + 1,OpCreate,univ.p_type,&univ) != Success)
				return rc.status;
			}

		// return results to caller...
		if(bufp != NULL)
			*bufp = bufptr(datum);
		if(created != NULL)
			*created = true;

		// run createBuf hook if requested and return.
		return (cflags & BS_Hook) ? exechook(NULL,INT_MIN,hooktab + HkCreateBuf,1,bname) : rc.status;
		}

	// Buffer not found and not creating.
	return false;
	}

// Free all line storage in given buffer and reset window pointers.  Return status.
static int bfree(Buffer *buf) {
	Line *lnp,*lnp1;
	EScreen *scr;
	EWindow *win;

	// Free all line objects except the first.
	for(lnp = buf->b_lnp->l_prev; lnp != buf->b_lnp; lnp = lnp1) {
		lnp1 = lnp->l_prev;
		free((void *) lnp);
		}

	// Free first line of buffer.  If allocated size is small, keep line and simply set "used" size to zero.  Otherwise,
	// allocate a new empty line for buffer so that memory is freed.
	if(lnp->l_size <= (NBlock << 1)) {
		lnp->l_used = 0;
		lnp->l_prev = lnp;
		}
	else {
		if(lalloc(0,&lnp1) != Success)
			return rc.status;		// Fatal error.
		free((void *) lnp);
		buf->b_lnp = lnp1->l_prev = lnp = lnp1;
		}
	lnp->l_next = NULL;

	// Reset window line links.

	// In all screens...
	scr = si.shead;
	do {
		// In all windows...
		win = scr->s_whead;
		do {
			if(win->w_buf == buf)
				faceinit(&win->w_face,lnp,buf);
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);

	return rc.status;
	}

// Check if a buffer can be erased and get confirmation from user if needed.  If it's marked as changed, it will not be erased
// unless BC_IgnChgd is set in "flags" or the user gives the okay.  This to save the user the grief of losing text.  If the
// buffer is narrowed and BC_Unnarrow is set, the buffer is silently unnarrowed before being cleared; otherwise, the user is
// prompted to give the okay before proceeding.
//
// Return Success if it's a go, Failure if error, or Cancelled if user changed his or her mind.
int bconfirm(Buffer *buf,ushort flags) {
	bool yep;
	bool eraseML = false;
	bool eraseOK = false;

	// Executing buffer?
	if(buf->b_mip != NULL && buf->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text226,text264,text238,buf->b_bname);
			// "Cannot %s %s buffer '%s'","clear","executing"

	// Buffer already empty?  If so and no flags specified, it's a go.
	if(bempty(buf) && flags == 0)
		return rc.status;

	// "Confirm all" or changed buffer?  Skip if need "narrowed buffer" confirmation as well, which preempts this.
	if(((flags & BC_Confirm) || ((buf->b_flags & BFChanged) && !(flags & BC_IgnChgd))) &&
	 (!(buf->b_flags & BFNarrowed) || (flags & BC_Unnarrow))) {
		char *str,prompt[NWork];
		eraseML = true;

		// Get user confirmation.
		if(!(si.opflags & OpScript) && !(flags & BC_ShowName))
			str = text32;
				// "Discard changes"
		else {
			if(buf->b_flags & BFChanged)
				sprintf(stpcpy(prompt,text32),text369,buf->b_bname);
					// "Discard changes"," in buffer '%s'"
			else
				sprintf(prompt,"%s %s%s '%s'",text26,buf->b_flags & BFHidden ? text453 : "",text83,
							// "Delete",				"hidden ","buffer"
				 buf->b_bname);
			str = prompt;
			}
		if(terminpYN(str,&yep) != Success)
			return rc.status;
		if(!yep)
			goto ClearML;
		}

	// Narrowed buffer?
	if(buf->b_flags & BFNarrowed) {

		// Force?
		if(flags & BC_Unnarrow)

			// Yes, restore buffer to pre-narrowed state.
			unnarrow(buf);
		else if(!(flags & BC_IgnChgd)) {

			// Not a force.  Get user confirmation (and leave narrowed).
			eraseML = true;
			if(terminpYN(text95,&yep) != Success)
					// "Narrowed buffer... delete visible portion"
				return rc.status;
			if(!yep)
				goto ClearML;
			}
		}
	eraseOK = true;

	if(eraseML)
ClearML:
		(void) mlerase();
	return eraseOK ? rc.status : rcset(Cancelled,0,NULL);
	}

// This routine blows away all of the text in a buffer and returns status.  If BC_ClrFilename flag is set, the filename
// associated with the buffer is set to null.
int bclear(Buffer *buf,ushort flags) {

	if(bconfirm(buf,flags) != Success)
		return rc.status;

	// Buffer already empty?  If so and no flags specified, just reset buffer.
	if(bempty(buf) && flags == 0)
		goto Unchange;

	// It's a go... erase it.
	if(flags & BC_ClrFilename)
		clfname(buf);					// Zap any filename (buffer will never be narrowed).
	if(bfree(buf) != Success)				// Free all line storage and reset line pointers.
		return rc.status;
	bchange(buf,WFHard | WFMode);				// Update window flags.
	if(buf->b_flags & BFNarrowed) {				// If narrowed buffer...
		buf->b_flags |= BFChanged;			// mark as changed...
		faceinit(&buf->b_face,buf->b_lnp,NULL);		// reset buffer line pointers and...
		Mark *mark = &buf->b_mroot;			// set all non-window visible marks to beginning of first line.
		do {
			if(mark->mk_id <= '~' && mark->mk_point.off >= 0) {
				mark->mk_point.lnp = buf->b_lnp;
				mark->mk_point.off = 0;
				}
			} while((mark = mark->mk_next) != NULL);
		}
	else {
Unchange:
		buf->b_flags &= ~BFChanged;			// Otherwise, mark as not changed...
		bufinit(buf,buf->b_lnp);			// reset buffer line pointers, delete marks...
		supd_wflags(buf,WFMode);			// and update mode line(s) if needed.
		}

	return rc.status;
	}

// Find a window displaying given buffer and return it, giving preference to current screen and current window.
EWindow *findwind(Buffer *buf) {
	EScreen *scr;
	EWindow *win;

	// If current window is displaying the buffer, use it.
	if(si.curwin->w_buf == buf)
		return si.curwin;

	// In current screen...
	win = si.whead;
	do {
		if(win->w_buf == buf)
			goto Retn;
		} while((win = win->w_next) != NULL);

	// In all other screens...
	scr = si.shead;
	do {
		if(scr == si.curscr)			// Skip current screen.
			continue;
		if(scr->s_curwin->w_buf == buf)	// Use current window first.
			return scr->s_curwin;

		// In all windows...
		win = scr->s_whead;
		do {
			if(win->w_buf == buf)
				goto Retn;
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);
Retn:
	return win;
	}

// Change zero or more buffer attributes (flags), given result pointer and action (n < 0: clear, n == 0 (default): toggle,
// n == 1: set, or n > 1: clear all and set).  Mutable attributes are: "Changed", "Hidden", and "TermAttr".
// If interactive:
//	. Prompt for one (if default n) or more (otherwise) single-letter attributes via parseopts().
//	. Use current buffer if default n; otherwise, prompt for buffer name.
// If script mode:
//	. Get buffer name from argv[0].
//	. Get zero or more keyword attributes via parseopts().
// Perform operation, set rval to former state (-1 or 1) of last attribute altered (or zero if n > 1 and no attributes
// specified), and return status.
int chgBufAttr(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	int count;
	long formerState = 0;
	ushort oldflags,newbflags,wflags;
	ushort newflags = 0;
	int action = (n == INT_MIN) ? 0 : n;
	static OptHdr ohdr = {
		ArgNil1,text374,false,battrinfo};
			// "buffer attribute"

	// Interactive?
	if(!(si.opflags & OpScript)) {
		DStrFab prompt;

		// Build prompt.
		if(dopentrk(&prompt) != 0 || dputf(&prompt,"%s %s %s",
		 action < 0 ? text65 : action == 0 ? text231 : action == 1 ? text64 : text296,text83,text186) != 0 ||
				// "Clear","Toggle","Set","Clear all and set"		"buffer","attribute"
		 dclose(&prompt,sf_string) != 0)
			goto LibFail;

		// Get attribute(s) from user.
		ohdr.single = (n == INT_MIN);
		if(parseopts(&ohdr,prompt.sf_datum->d_str,NULL,&count) != Success || (count == 0 && action <= 1))
			return rc.status;

		// Get buffer.  If default n, use current buffer.
		if(n == INT_MIN)
			buf = si.curbuf;
		else {
			// n is not the default.  Get buffer name.  If nothing is entered, bag it.
			buf = bdefault();
			if(bcomplete(rval,text229 + 2,buf != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL) != Success ||
					// ", in"
			 rval->d_type == dat_nil)
				return rc.status;

			// If "Clear all and set" and no attribute was selected, skip attribute processing.
			if(action > 1 && count == 0)
				goto SetFlags;
			}
		}
	else {
		// Script mode.  Get buffer name argument.
		if((buf = bsrch(argv[0]->d_str,NULL)) == NULL)
			return rcset(Failure,0,text118,argv[0]->d_str);
				// "No such buffer '%s'"

		// Get attribute(s).
		ohdr.single = false;
		if(parseopts(&ohdr,NULL,argv[1],&count) != Success)
			return rc.status;

		if(count == 0 && action <= 1)
			return rcset(Failure,0,text455,ohdr.otyp);
				// "Missing required %s"
		}

	// Scan attribute table and set newflags.
	if(count > 0) {
		Option *opt = battrinfo;
		do {
			if(opt->cflags & OptSelected) {
				formerState = (buf->b_flags & opt->u.value) ? 1L : -1L;
				newflags |= opt->u.value;
				}
			} while((++opt)->keywd != NULL);
		}
SetFlags:
	// Have flag(s) and buffer.  Perform operation.
	oldflags = newbflags = buf->b_flags;
	if(action > 1)
		newbflags &= ~(BFChanged | BFHidden | BFReadOnly | BFTermAttr);
	if(newflags != 0) {
		if(action < 0)
			newbflags &= ~newflags;			// Clear.
		else {
			if(action > 0)
				newbflags |= newflags;		// Set.
			else
				newbflags ^= newflags;		// Toggle.

			// Have new buffer flags in newbflags.  Check for conflicts.
			if((newflags & (BFMacro | BFTermAttr)) == (BFMacro | BFTermAttr))
				return rcset(Failure,RCNoFormat,text376);
					// "Cannot enable terminal attributes in a macro buffer"
			if((newbflags & (BFChanged | BFReadOnly)) == (BFChanged | BFReadOnly)) {
				if(!(si.opflags & OpScript))
					ttbeep();
				if(buf->b_flags & BFChanged)
					return rcset(Failure,0,text461,text260,text460);
							// "Cannot set %s buffer to %s","changed","read-only"
				return rcset(Failure,0,text109,text58,text460);
						// "%s is %s","Buffer","read-only"
				}
			}
		}

	// All is well... set new buffer flags.
	buf->b_flags = newbflags;

	// Set window flags if needed.
	wflags = (oldflags & (BFChanged | BFReadOnly)) != (newbflags & (BFChanged | BFReadOnly)) ? WFMode : 0;
	if((oldflags & BFTermAttr) != (newbflags & BFTermAttr))
		wflags |= WFHard;
	if(wflags)
		supd_wflags(buf,wflags);

	// Return former state of last attribute that was changed.
	dsetint(formerState,rval);

	// Wrap it up.
	return (si.opflags & OpScript) ? rc.status : (newflags & (BFChanged | BFReadOnly)) && n == INT_MIN ? mlerase() :
	 rcset(Success,RCNoFormat,text375);
		// "Attribute(s) changed"
LibFail:
	return librcset(Failure);
	}

// Get a buffer name (if n not default) and perform operation on buffer according to "op" argument.  If prmt == NULL (bgets
// function), set rval to function return value; otherwise, buffer name.  Return status.
int bufop(Datum *rval,int n,char *prmt,uint op,int flag) {
	Point *point;
	Buffer *buf = NULL;

	// Get the buffer name.  n will never be the default for a bgets() call.
	if(n == INT_MIN)
		buf = si.curbuf;
	else {
		if(prmt != NULL)
			buf = bdefault();
		if(bcomplete(rval,prmt,buf != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL) != Success || buf == NULL)
			return rc.status;
		}

	// Perform requested operation (BOpGotoLn, BOpBeginEnd, or BOpReadBuf).
	if(op == BOpGotoLn && flag == 0) {
		op = BOpBeginEnd;
		flag = true;
		}

	// Move point in buffer... usually a massive adjustment.  Grab point.  If the buffer is being displayed, get from window
	// face instead of buffer face.
	if(buf->b_nwind > 0) {
		EWindow *win = findwind(buf);
		win->w_flags |= WFMove;		// Set "hard motion" flag.
		point = &win->w_face.wf_point;
		}
	else
		point = &buf->b_face.wf_point;

	// Perform operation.
	switch(op) {
		case BOpBeginEnd:
			// Go to beginning or end of buffer.
			if(flag) {		// EOB
				point->lnp = buf->b_lnp->l_prev;
				point->off = point->lnp->l_used;
				}
			else {			// BOB
				point->lnp = buf->b_lnp;
				point->off = 0;
				}
			break;
		case BOpGotoLn:
			// Go to beginning of buffer and count lines.
			point->lnp = buf->b_lnp;
			point->off = 0;
			if(buf == si.curbuf)
				return moveln(flag - 1);
			for(--flag; flag > 0; --flag) {
				if(point->lnp->l_next == NULL)
					break;
				point->lnp = point->lnp->l_next;
				}
			break;
		default:	// BOpReadBuf
			// Read next line from buffer n times.
			while(n-- > 0) {
				size_t len;

				// If we are at EOB, return nil.
				if(bufend(point)) {
					dsetnil(rval);
					return rc.status;
					}

				// Get the length of the current line (from point to end)...
				len = point->lnp->l_used - point->off;

				// return the spoils...
				if(dsetsubstr(point->lnp->l_text + point->off,len,rval) != 0)
					return librcset(Failure);

				// and step the buffer's line pointer ahead one line (or to end of last line).
				if(point->lnp->l_next == NULL)
					point->off = point->lnp->l_used;
				else {
					point->lnp = point->lnp->l_next;
					point->off = 0;
					}
				}
		}

	return rc.status;
	}

// Set name of a system (internal) buffer and call bfind();
int sysbuf(char *root,Buffer **bufp,ushort flags) {
	char bname[strlen(root) + 2];

	bname[0] = BSysLead;
	strcpy(bname + 1,root);
	return bfind(bname,BS_Create | BS_Force,BFHidden | flags,bufp,NULL);
	}

// Activate a buffer if needed.  Return status.
int bactivate(Buffer *buf) {

	// Check if buffer is active.
	if(!(buf->b_flags & BFActive)) {

		// Not active: read attached file.
		(void) readin(buf,NULL,RWKeep | RWStats);
		}

	return rc.status;
	}

// Insert a buffer into the current buffer and set current region to inserted lines.  If n == 0, leave point before the inserted
// lines; otherwise, after.  Return status.
int insertBuf(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	DataInsert di = {
		si.curbuf,	// Target buffer.
		NULL,		// Target line.
		text153		// "Inserting data..."
		};

	// Get the buffer name.  Reject if current buffer.
	buf = bdefault();
	if(bcomplete(rval,text55,buf != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL) != Success)
			// "Insert"
		return rc.status;
	dclear(rval);
	if(buf == NULL) {
		if(!(si.opflags & OpScript))
			return rc.status;
		buf = si.curbuf;
		}
	if(buf == si.curbuf)
		return rcset(Failure,RCNoFormat,text124);
			// "Cannot insert current buffer into itself"

	// Let user know what's up.
	if(mlputs(MLHome | MLWrap | MLFlush,text153) != Success)
			// "Inserting data..."
		return rc.status;

	// Prepare buffer to be inserted.  Return after activation if buffer is empty.
	if(bactivate(buf) != Success)
		return rc.status;
	if(bempty(buf))
		return rcset(Success,RCHigh,"%s 0 %ss",text154,text205);
						// "Inserted","line"

	// Buffer not empty.  Insert lines and report results.
	return idata(n,buf,&di) != Success ? rc.status :
	 rcset(Success,RCHigh,"%s %u %s%s%s",text154,di.lineCt,text205,di.lineCt == 1 ? "" : "s",text355);
					// "Inserted","line"," and marked as region"
	}

// Attach a buffer to the current window, creating it if necessary (default).  Render buffer and return status.
int selectBuf(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	bool created;
	char *prmt;
	uint op;

	// Get the buffer name.
	buf = bdefault();
	if(n == -1) {
		prmt = text27;
			// "Pop"
		op = OpDelete;
		}
	else {
		prmt = (n == INT_MIN || n == 1) ? text113 : text24;
						// "Switch to","Select"
		op = OpCreate;
		}
	if(bcomplete(rval,prmt,buf != NULL ? buf->b_bname : NULL,op,&buf,&created) != Success || buf == NULL)
		return rc.status;

	// Render the buffer.
	return render(rval,n == INT_MIN ? 1 : n,buf,created ? RendNewBuf | RendNotify : 0);
	}

// Display a file or buffer in a pop-up window with options.  Return status.
int dopop(Datum *rval,int n,bool popbuf) {
	Buffer *buf;
	int count = 0;
	ushort bflags;
	ushort rflags = 0;
	bool created = false;
	ushort oldmsg = mi.cache[MdIdxRtnMsg]->ms_flags & MdEnabled;
	static Option options[] = {
		{"^Existing","^Exist",0,0},
		{"^AltModeLine","^AltML",0,RendAltML},
		{"^Shift","^Shft",0,RendShift},
		{"^TermAttr","^TAttr",0,0},
		{"^Delete","^Del",0,RendNewBuf},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		0,text223,false,options};
			// "pop option"

	// Get buffer name or filename.
	if(popbuf) {
		// Get buffer name.
		buf = bdefault();
		if(bcomplete(rval,text27,buf != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL) != Success || buf == NULL)
				// "Pop"
			return rc.status;
		options[0].cflags |= OptIgnore;
		}
	else {
		// Get filename.
		if(gtfilename(rval,text299,NULL,0) != Success || rval->d_type == dat_nil)
				// "Pop file"
			return rc.status;
		options[0].cflags &= ~OptIgnore;
		}

	// Get options if applicable.
	if(n != INT_MIN && parseopts(&ohdr,text448,NULL,&count) != Success)
				// "Options"
		return rc.status;

	if(oldmsg)
		gmclear(mi.cache[MdIdxRtnMsg]);

	// Open and read file in rval, if applicable.
	if(!popbuf) {
		ushort cflags = (count > 0 && (options[0].cflags & OptSelected)) ? BS_Create | BS_Derive :
		 BS_Create | BS_Derive | BS_Force;
		if(bfind(rval->d_str,cflags,0,&buf,&created) != Success ||
		 (created && readin(buf,rval->d_str,RWKeep | RWNoHooks | RWExist) != Success))
			return rc.status;
		if(created)
			rflags |= RendNewBuf;
		}

	// Process options.
	if(count > 0) {
		rflags = getFlagOpts(options);
		if(options[3].cflags & OptSelected) {		// "TermAttr"
			if(buf->b_flags & BFMacro)
				return rcset(Failure,RCNoFormat,text376);
					// "Cannot enable terminal attributes in a macro buffer"
			buf->b_flags |= BFTermAttr;
			}
		}

	bflags = buf->b_flags;		// Save buffer flags for possible restore after pop.

	// Render the buffer and return notice if it is deleted and was not just created.
	if(render(rval,-1,buf,rflags) == Success) {
		if(oldmsg)
			gmset(mi.cache[MdIdxRtnMsg]);
		if(!(rflags & RendNewBuf))
			buf->b_flags = bflags;
		else if(!created)
			(void) rcset(Success,0,"%s %s",text58,text10);
						// "Buffer","deleted"
		}

	return rc.status;
	}

// Create a scratch buffer; that is, one with a unique name and no filename associated with it.  Set *bufp to buffer pointer,
// and return status.
int bscratch(Buffer **bufp) {
	uint maxtries = 100;
	Buffer *buf;
	bool created;
	char bname[MaxBufName + 1];

	// Create a buffer with a unique name.
	do {
		sprintf(bname,"%s%ld",Scratch,xorshift64star(1000) - 1);
		if(bfind(bname,BS_Create | BS_Hook,0,&buf,&created) != Success)
			return rc.status;

		// Was a new buffer created?
		if(created)
			goto Retn;	// Yes, use it.

		// Nope, a buffer with that name already exists.  Try again unless limit reached.
		} while(--maxtries > 0);

	// Random-number approach failed... let bfind() "uniquify" it.
	(void) bfind(bname,BS_Create | BS_Hook | BS_Force,0,&buf,NULL);
Retn:
	*bufp = buf;
	return rc.status;
	}

// Create a scratch buffer.  Render buffer and return status.
int scratchBuf(Datum *rval,int n,Datum **argv) {
	Buffer *buf;

	// Create buffer...
	if(bscratch(&buf) != Success)
		return rc.status;

	// and render it.
	return render(rval,n == INT_MIN ? 1 : n,buf,RendNewBuf);
	}

// Run exit-buffer hook (and return result) or enter-buffer hook.  Return status.
int bhook(Datum **rvalp,ushort flags) {

	if(!(si.curbuf->b_flags & BFMacro)) {
		if(*rvalp == NULL && dnewtrk(rvalp) != 0)
			return librcset(Failure);
		if(flags & SWB_ExitHook) {

			// Run exit-buffer user hook on current (old) buffer.
			dsetnil(*rvalp);
			(void) exechook(*rvalp,INT_MIN,hooktab + HkExitBuf,0);
			}
		else
			// Run enter-buffer user hook on current (new) buffer.
			(void) exechook(*rvalp,INT_MIN,hooktab + HkEnterBuf,0x21,*rvalp);
		}
	return rc.status;
	}

// Make given buffer current, update s_lastbuf variable in current screen, and return status.  The top line, point, and column
// offset values from the current window are saved in the old buffer's header and replacement ones are fetched from the new
// (given) buffer's header.
int bswitch(Buffer *buf,ushort flags) {
	Datum *rval = NULL;

	// Nothing to do if buf is current buffer.
	if(buf == si.curbuf)
		return rc.status;
	// Run exit-buffer user hook on current (old) buffer, if applicable.
	if(!(flags & SWB_NoHooks) && bhook(&rval,SWB_ExitHook) != Success)
		return rc.status;

	// Decrement window use count of current (old) buffer and save the current window settings.
	if(--si.curbuf->b_nwind == 0)
		si.curbuf->b_lastscr = si.curscr;
	wftobf(si.curwin,si.curbuf);
	si.curscr->s_lastbuf = si.curbuf;

	// Switch to new buffer.
	si.curwin->w_buf = si.curbuf = buf;
	++si.curbuf->b_nwind;

	// Activate buffer.
	if(bactivate(si.curbuf) <= MinExit)
		return rc.status;

	// Update window settings.
	bftowf(si.curbuf,si.curwin);

	// Run enter-buffer user hook on current (new) buffer, if applicable.
	if(rc.status == Success && !(flags & SWB_NoHooks))
		(void) bhook(&rval,0);

	return rc.status;
	}

// Switch to the previous or next visible buffer in the buffer list n times.  Set rval to name of final buffer if successful.
// If n == 0, switch once and include hidden, non-macro buffers as candidates.  If n < 0, switch once and delete the current
// buffer.  Return status.
int pnbuffer(Datum *rval,int n,bool backward) {
	Buffer *bufp0,*bufp1;
	ushort bflags;
	Datum **blp0,**blp,**blpz;
	ssize_t index;
	int incr = (backward ? -1 : 1);

	if(n == INT_MIN)
		n = 1;

	// Cycle backward or forward through buffer list n times, or just once if n <= 0.
	do {
		// Get the current buffer list pointer.
		(void) bsrch(si.curbuf->b_bname,&index);
		blp0 = buftab.a_elp + index;

		// Find the first buffer before or after the current one that meets the criteria.
		blp = blp0 + incr;
		blpz = buftab.a_elp + buftab.a_used;
		for(;;) {
			// Loop around buffer list if needed.
			if(blp < buftab.a_elp || blp == blpz)
				blp = backward ? blpz - 1 : buftab.a_elp;

			if(blp == blp0)

				// We're back where we started... buffer not found.  Bail out (do nothing).
				return rc.status;
			else {
				bufp1 = bufptr(*blp);
				if(!(bufp1->b_flags & BFHidden) || (!(bufp1->b_flags & BFMacro) && n == 0))
					break;
				blp += incr;
				}
			}

		// Buffer found... switch to it.
		bufp0 = si.curbuf;
		bflags = bufp1->b_flags;
		if(bswitch(bufp1,0) != Success)
			return rc.status;
		if(n < 0) {
			char bname[MaxBufName + 1];
			strcpy(bname,bufp0->b_bname);
			if(bdelete(bufp0,0) != Success || mlprintf(MLHome | MLWrap | MLFlush,text372,bname) != Success)
									// "Buffer '%s' deleted"
				return rc.status;
			if(!(bflags & BFActive))
				cpause(50);		// Pause a moment for "deleted" message to be seen before read.
			}
		} while(--n > 0);

	return dsetstr(bufp1->b_bname,rval) != 0 ? librcset(Failure) : rc.status;
	}

// Clear current buffer, or named buffer if n >= 0.  Force it if n != 0.  Set rval to false if buffer is not cleared; otherwise,
// true.  Return status.
int clearBuf(Datum *rval,int n,Datum **argv) {
	Buffer *buf;

	// Get the buffer name to clear.
	if(n == INT_MIN)
		buf = si.curbuf;
	else {
		buf = bdefault();
		if(bcomplete(rval,text169,buf != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL) != Success || buf == NULL)
				// "Clear"
			return rc.status;
		}

	// Blow text away unless user got cold feet.
	if(bclear(buf,n < 0 && n != INT_MIN ? BC_IgnChgd : 0) >= Cancelled) {
		dsetbool(rc.status == Success,rval);
		if(n >= 0)
			(void) mlerase();
		}

	return rc.status;
	}

// Check if an attribute is set in a buffer and set rval to Boolean result.  Return status.
int bufAttrQ(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	char *keyword = argv[1]->d_str;
	bool result = false;
	Option *opt = battrinfo;

	// Get buffer.
	if((buf = bsrch(argv[0]->d_str,NULL)) == NULL)
		return rcset(Failure,0,text118,argv[0]->d_str);
			// "No such buffer '%s'"

	// Scan buffer attribute table for a match.
	do {
		if(strcasecmp(keyword,*opt->keywd == '^' ? opt->keywd + 1 : opt->keywd) == 0) {
			if(buf->b_flags & opt->u.value)
				result = true;
			goto Found;
			}
		} while((++opt)->keywd != NULL);

	// Match not found... return an error.
	return rcset(Failure,0,text447,text374,keyword);
			// "Invalid %s '%s'","buffer attribute"
Found:
	dsetbool(result,rval);
	return rc.status;
	}

// Delete marks, given buffer pointer.  Free all visible user marks, plus invisible user marks if MkOpt_Viz flag is not set,
// plus window marks if MkOpt_Wind flag is set -- then set root mark to default.
void mdelete(Buffer *buf,ushort flags) {
	Mark *mkp0,*mkp1,*mkp2;
	Mark *mark = &buf->b_mroot;

	// Free marks in list.
	for(mkp1 = (mkp0 = mark)->mk_next; mkp1 != NULL; mkp1 = mkp2) {
		mkp2 = mkp1->mk_next;
		if((mkp1->mk_id <= '~' && (mkp1->mk_point.off >= 0 || !(flags & MkOpt_Viz))) ||
		 (mkp1->mk_id > '~' && (flags & MkOpt_Wind))) {
			(void) free((void *) mkp1);
			mkp0->mk_next = mkp2;
			}
		else
			mkp0 = mkp1;
		}

	// Initialize root mark to end of buffer.
	mark->mk_id = RegMark;
	mark->mk_point.off = (mark->mk_point.lnp = buf->b_lnp->l_prev)->l_used;
	mark->mk_rfrow = 0;
	}

// Delete the buffer pointed to by buf.  Don't allow if buffer is being displayed, executed, or aliased.  Pass "flags" with
// BC_Unnarrow set to bclear() to clear the buffer, then free the header line and the buffer block.  Also delete any key binding
// and remove the exectab entry if it's a macro.  Return status, including Cancelled if user changed his or her mind.
int bdelete(Buffer *buf,ushort flags) {
	UnivPtr univ;
	KeyBind *kbind;

	// We cannot nuke a displayed buffer.
	if(buf->b_nwind > 0)
		return rcset(Failure,0,text28,text58);
			// "%s is being displayed","Buffer"

	// We cannot nuke an executing buffer.
	if(buf->b_mip != NULL && buf->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text226,text263,text238,buf->b_bname);
			// "Cannot %s %s buffer '%s'","delete","executing"

	// We cannot nuke a aliased buffer.
	if(buf->b_nalias > 0)
		return rcset(Failure,0,text272,buf->b_nalias);
			// "Macro has %hu alias(es)"

	// We cannot nuke a macro bound to a hook.
	if((buf->b_flags & BFMacro) && ishook(buf,true))
		return rc.status;

	// It's a go.  Blow text away (unless user got cold feet).
	if(bclear(buf,flags | BC_ClrFilename | BC_Unnarrow) != Success)
		return rc.status;

	// Delete exectab entry.
	if((buf->b_flags & BFMacro) && execfind(buf->b_bname + 1,OpDelete,0,NULL) != Success)
		return rc.status;

	mdelete(buf,MkOpt_Wind);			// Delete all marks.
	univ.u.p_buf = buf;				// Get key binding, if any.
	kbind = getpentry(&univ);

	if(si.savbuf == buf)				// Unsave buffer if saved.
		si.savbuf = NULL;
	if(si.curscr->s_lastbuf == buf)			// Clear "last buffer".
		si.curscr->s_lastbuf = NULL;
	ppfree(buf);					// Release any macro preprocessor storage.
	if(buf->b_mip != NULL)
		free((void *) buf->b_mip);		// Release buffer extension record.
	ddelete(delistbuf(buf));			// Remove from buffer list and destroy Buffer and Datum objects.
	if(kbind != NULL)
		unbindent(kbind);				// Delete buffer key binding.

	return rc.status;
	}

// Delete buffer(s) and return status.  Operation is controlled by n argument and operation codes:
//
// If default n or n <= 0:
//	Delete named buffer(s) with confirmation if changed.  If n <= 0, ignore changes (skip confirmation).
//
// If n > 0, first (and only) argument is a string containing comma-separated op codes (keywords).  One of the following must
// be specified:
//	Unchanged	Select unchanged, visible buffers.
//	All		Select all visible buffers.
// and zero or one of the following:
//	Hidden		Include hidden buffers.
// and zero or one of the following:
//	Confirm		Get confirmation for each buffer selected (changed or unchanged).
//	Force		Delete all buffers selected, with initial confirmation.
// Additionally, the following rules apply:
//  1. Macro buffers and any buffer being displayed are skipped unconditionally.
//  2. If neither "Confirm" nor "Force" is specified, confirmation is requested for changed buffers only.
//  3. If "Force" is specified, changed and/or narrowed buffers are deleted without confirmation.
int delBuf(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	int count;
	char wkbuf[64];
	static bool allViz;
	static bool inactive;
	static bool unchanged;
	static bool hidden;
	static bool confirm;
	static bool force;
	static Option options[] = {
		{"^AllVisible","^AllViz",0,.u.ptr = (void *) &allViz},
		{"^Inactive","^Inact",0,.u.ptr = (void *) &inactive},
		{"^Unchanged","^Unchg",0,.u.ptr = (void *) &unchanged},
		{"^Hidden","^Hid",0,.u.ptr = (void *) &hidden},
		{"^Confirm","^Cfrm",0,.u.ptr = (void *) &confirm},
		{"^Force","^Frc",0,.u.ptr = (void *) &force},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		ArgFirst,text410,false,options};
			// "command option"

	dsetint(0L,rval);
	initBoolOpts(options);

	// Check if processing multiple unnamed buffers.
	if(n > 0) {
		Option *opt,*optz;

		// Build prompt if interactive
		if(si.opflags & OpScript)
			wkbuf[0] = '\0';
		else
			sprintf(wkbuf,"%s %ss",text26,text83);
					// "Delete","buffer"

		// Get op codes, set flags, and validate them.
		if(parseopts(&ohdr,wkbuf,NULL,&count) != Success || count == 0)
			return rc.status;
		optz = (opt = options) + 3;
		count = 0;
		do {
			if(opt->cflags & OptSelected) {
				*((bool *) opt->u.ptr) = true;
				if(opt < optz)
					++count;
				}
			} while((++opt)->keywd != NULL);
		if(count == 0)
			return rcset(Failure,0,text455,text410);
				// "Missing required %s","command option"
		if(count > 1 || (confirm && force))
			return rcset(Failure,0,text454,text410);
				// "Conflicting %ss","command option"

		// Get confirmation if interactive and force-deleting all other buffers.
		if(!(si.opflags & OpScript) && allViz && force) {
			bool yep;

			sprintf(wkbuf,text168,hidden ? "" : text452);
				// "Delete all other %sbuffers","visible "

			if(terminpYN(wkbuf,&yep) != Success)
				return rc.status;
			if(!yep)
				return mlerase();
			}

		// It's a go.  Loop through buffer list.
		Datum **blp = buftab.a_elp;
		Datum **blpz = blp + buftab.a_used;
		ushort flags = confirm ? (BC_ShowName | BC_Confirm) : force ? (BC_ShowName | BC_IgnChgd) : BC_ShowName;
		count = 0;
		do {
			buf = bufptr(*blp);

			// Skip if:
			//  1. being displayed or a macro;
			//  2. hidden and "Hidden" op code not specified;
			//  3. modified and "Unchanged" op code specified.
			//  4. active and "Inactive" op code specified.
			if(buf->b_nwind > 0 || (buf->b_flags & BFMacro) ||
			 ((buf->b_flags & BFHidden) && !hidden) ||
			 ((buf->b_flags & BFChanged) && unchanged) ||
			 ((buf->b_flags & BFActive) && inactive))
				++blp;
			else {
				// Nuke it (if confirmed), but don't increment blp if successful because array element it points
				// to will be removed.  Decrement blpz instead.
				if(!(flags & BC_Confirm) && (!(buf->b_flags & BFChanged) || (flags & BC_IgnChgd))) {
					if(mlprintf(MLHome | MLWrap | MLFlush,text367,buf->b_bname) != Success)
							// "Deleting buffer %s..."
						return rc.status;
					cpause(50);
					}
				if(bdelete(buf,flags) < Cancelled)
					break;
				if(rc.status == Success) {
					++count;
					--blpz;
					}
				else {
					rcclear(0);
					++blp;
					}
				}
			} while(blp < blpz);

		if(rc.status == Success) {
			dsetint((long) count,rval);
			(void) rcset(Success,RCHigh,text368,count,count != 1 ? "s" : "");
						// "%u buffer%s deleted"
			}
		return rc.status;
		}

	// Process named buffer(s).
	count = 0;
	if(n == INT_MIN)
		n = 0;		// No force.

	// If interactive, get buffer name from user.
	if(!(si.opflags & OpScript)) {
		buf = bdefault();
		if(bcomplete(rval,text26,buf != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL) != Success || buf == NULL)
				// "Delete"
			return rc.status;
		goto NukeIt;
		}

	// Script mode: get buffer name(s) to delete.
	Datum *bname;
	uint aflags = ArgFirst | ArgNotNull1;
	if(dnewtrk(&bname) != 0)
		return librcset(Failure);
	do {
		if(!(aflags & ArgFirst) && !havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(bname,aflags) != Success)
			return rc.status;
		aflags = ArgNotNull1;
		if((buf = bsrch(bname->d_str,NULL)) == NULL)
			(void) rcset(Success,RCNoWrap,text118,bname->d_str);
				// "No such buffer '%s'"
		else {
NukeIt:
			// Nuke it.
			if(bdelete(buf,n < 0 ? BC_IgnChgd : 0) != Success) {

				// Don't cause a running script to fail on Failure status.
				if(rc.status < Failure)
					return rc.status;
				(void) rcunfail();
				}
			else
				++count;
			}
		} while(si.opflags & OpScript);

	// Return count if no errors.
	if(disnull(&rc.msg)) {
		dsetint((long) count,rval);
		if(!(si.opflags & OpScript))
			(void) rcset(Success,0,"%s %s",text58,text10);
						// "Buffer","deleted"
		}

	return rc.status;
	}

// Rename a buffer and set rval (if not NULL) to the new name.  If BR_Auto set in flags, derive the name from the filename
// attached to the buffer and use it, without prompting; otherwise, get new buffer name (and use BR_Current flag to determine
// correct prompt if interactive).  Return status.
int brename(Datum *rval,ushort flags,Buffer *targbuf) {
	Buffer *buf;
	Array *ary;
	ssize_t index;
	char *prmt;
	Datum *datum;
	Datum *bname;			// New buffer name.
	UnivPtr univ;
	TermInp ti = {NULL,RtnKey,MaxBufName,NULL};
	char askstr[NWork];

	// We cannot rename an executing buffer.
	if(targbuf->b_mip != NULL && targbuf->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text284,text275,text248);
			// "Cannot %s %s buffer","rename","an executing"

	if(dnewtrk(&bname) != 0)
		goto LibFail;

	// Auto-rename if BR_Auto flag set.  Do nothing if buffer name is already the target name.
	if(flags & BR_Auto) {
		if(targbuf->b_fname == NULL)
			return rcset(Failure,0,text145,targbuf->b_bname);
					// "No filename associated with buffer '%s'"
		if(dsalloc(bname,MaxBufName + 1) != 0)
			goto LibFail;
		if(strcmp(fbname(bname->d_str,targbuf->b_fname),targbuf->b_bname) == 0)
			return rc.status;
		bunique(bname->d_str,NULL);
		goto SetNew;
		}

	// Set initial prompt.
	prmt = (flags & BR_Current) ? text385 : text339;
			// "Change buffer name to","to";

Ask:
	// Get the new buffer name.
	if(si.opflags & OpScript) {
		if(funcarg(bname,ArgNotNull1) != Success)
			return rc.status;
		}
	else if(terminp(bname,prmt,ArgNotNull1 | ArgNil1,0,&ti) != Success || bname->d_type == dat_nil)
		return rc.status;

	// Valid buffer name?
	if(!isbname(bname->d_str)) {
		if(si.opflags & OpScript)
			return rcset(Failure,0,text447,text128,bname->d_str);
				// "Invalid %s '%s'","buffer name"
		sprintf(prmt = askstr,text447,text128,bname->d_str);
				// "Invalid %s '%s'","buffer name"
		strcat(prmt,text324);
				// ".  New name"
		goto Ask;	// Try again.
		}

	// Check for duplicate name.
	ary = &buftab;
	while((datum = aeach(&ary)) != NULL) {
		buf = bufptr(datum);
		if(buf != targbuf) {

			// Other buffer with this name?
			if(strcmp(bname->d_str,buf->b_bname) == 0) {
				if(si.opflags & OpScript)
					return rcset(Failure,0,text181,text58,bname->d_str);
						// "%s name '%s' already in use","Buffer"
				prmt = text25;
					// "That name is already in use.  New name"
				goto Ask;		// Try again.
				}

			// Macro buffer name?
			if((*targbuf->b_bname == SBMacro) != (*bname->d_str == SBMacro)) {
				if(si.opflags & OpScript)
					return rcset(Failure,0,*bname->d_str == SBMacro ? text268 : text270,text275,
					 bname->d_str,SBMacro);
						// "Cannot %s buffer: name '%s' cannot begin with %c","rename"
						// "Cannot %s macro buffer: name '%s' must begin with %c","rename"
				sprintf(prmt = askstr,"%s'%c'%s",text273,SBMacro,text324);
					// "Macro buffer names (only) begin with ",".  New name"
				goto Ask;		// Try again.
				}
			}
		}

	// Valid macro name?
	if(*bname->d_str == SBMacro) {
		char *str = bname->d_str + 1;
		enum e_sym sym = getident(&str,NULL);
		if((sym != s_ident && sym != s_identq) || *str != '\0') {
			if(si.opflags & OpScript)
				return rcset(Failure,0,text447,text286,bname->d_str + 1);
					// "Invalid %s '%s'","identifier"
			sprintf(prmt = askstr,text447,text286,bname->d_str + 1);
					// "Invalid %s '%s'","identifier"
			strcat(prmt,text324);
					// ".  New name"
			goto Ask;	// Try again.
			}
		}
SetNew:
	// New name is unique and valid.  Rename the buffer.
	if((targbuf->b_flags & BFMacro) &&
	 execfind(targbuf->b_bname + 1,OpDelete,0,NULL) != Success)	// Remove old macro name from exectab hash...
		return rc.status;
	datum = delistbuf(targbuf);				// remove from buffer list...
	strcpy(targbuf->b_bname,bname->d_str);			// copy new buffer name to structure...
	(void) bsrch(targbuf->b_bname,&index);			// get index of new name...
	if(enlistbuf(datum,index) != Success)			// put it back in the right place...
		return rc.status;
	if((targbuf->b_flags & BFMacro) && execfind((univ.u.p_buf = targbuf)->b_bname + 1,
	 OpCreate,univ.p_type = (targbuf->b_flags & BFConstrain) ? PtrMacro_C : PtrMacro_O,
	 &univ) != Success)					// add new macro name to exectab hash...
		return rc.status;
	supd_wflags(targbuf,WFMode);				// and make all affected mode lines replot.
	if(!(si.opflags & OpScript))
		(void) mlerase();
	if(rval != NULL)
		datxfer(rval,bname);

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Rename current buffer (if interactive and default n) or a named buffer.  Return status.
int renameBuf(Datum *rval,int n,Datum **argv) {
	Buffer *buf;
	ushort flags = 0;

	// Get buffer.
	if(n == INT_MIN && !(si.opflags & OpScript)) {
		buf = si.curbuf;
		flags = BR_Current;
		}
	else if(bcomplete(rval,text29,(buf = bdefault()) != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL) != Success ||
			// "Rename"
	 buf == NULL)
		return rc.status;

	return brename(rval,flags,buf);
	}

// Get size of a buffer in lines and bytes.  Set *lp to line count if not NULL and return byte count.
long buflength(Buffer *buf,long *lp) {
	long nlines,byteCt;
	Line *lnp = buf->b_lnp;

	nlines = byteCt = 0;
	while(lnp->l_next != NULL) {			// Count all lines except the last...
		++nlines;
		byteCt += lnp->l_used + 1;
		lnp = lnp->l_next;
		}
	byteCt += lnp->l_used;				// count bytes in last line...
	if(lnp->l_used > 0)				// and bump line count if last line not empty.
		++nlines;
	if(lp != NULL)
		*lp = nlines;
	return byteCt;
	}

// Add text (which may contain newlines) to the end of the given buffer.  Return status.  Note that (1), this works on non-
// displayed buffers as well; (2), the last line of the buffer is assumed to be empty (all lines are inserted before it); (3),
// the last null-terminated text segment has an implicit newline delimiter; and (4), the BFChanged buffer flag is not set.
// Function is used exclusively by "show" and "completion" routines where text is written to an empty buffer.
int bappend(Buffer *buf,char *text) {
	Line *lnp;
	char *str0,*str1;

	// Loop for each line segment found that ends with newline or null.
	str0 = str1 = text;
	for(;;) {
		if(*str1 == '\n' || *str1 == '\0') {

			// Allocate memory for the line and copy it over.
			if(lalloc(str1 - str0,&lnp) != Success)
				return rc.status;
			if(str1 > str0)
				memcpy(lnp->l_text,str0,str1 - str0);

			// Add the new line to the end of the buffer.
			llink(lnp,buf,buf->b_lnp->l_prev);

			// Check and adjust source pointers.
			if(*str1 == '\0')
				break;
			str0 = str1 + 1;
			}
		++str1;
		}

	return rc.status;
	}

// Read the nth next line from a buffer and store in rval.  Return status.
int bgets(Datum *rval,int n,Datum **argv) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"

	return bufop(rval,n,NULL,BOpReadBuf,0);
	}
