// (c) Copyright 2018 Richard W. Marinelli
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
static void dumpmode(ModeSpec *msp) {

	fprintf(logfile,"  %-16s%.4hX  %c%c%c%c  %-16s  %s\n",msp->ms_name,msp->ms_flags,
	 msp->ms_flags & MdUser ? 'U' : '.',
	 msp->ms_flags & MdGlobal ? 'G' : '.',
	 msp->ms_flags & MdHidden ? 'H' : '.',
	 msp->ms_flags & MdEnabled ? SMActive : '.',
	 msp->ms_group == NULL ? "NULL" : msp->ms_group->mg_name,
	 msp->ms_desc == NULL ? "NULL" : msp->ms_desc);
	}

void dumpmodes(Buffer *bufp,bool sepline,char *fmt,...) {
	va_list ap;

	fputs(sepline ? "----------\n" : "\n",logfile);
	va_start(ap,fmt);
	if(bufp == NULL) {
		Datum *datp;
		Array *aryp = &mi.modetab;

		fputs("MODE TABLE - ",logfile);
		vfprintf(logfile,fmt,ap);
		while((datp = aeach(&aryp)) != NULL)
			dumpmode(msptr(datp));
		}
	else {
		BufMode *bmp = bufp->b_modes;
		fprintf(logfile,"BUFFER '%s': ",bufp->b_bname);
		vfprintf(logfile,fmt,ap);
		if(bmp == NULL)
			fputs("\t*None set*\n",logfile);
		else
			do
				dumpmode(bmp->bm_modep);
				while((bmp = bmp->bm_nextp) != NULL);
		}
	va_end(ap);
	}
#endif

// Check if any given mode (ModeSpec pointers in vararg list) is set in a buffer and return Boolean result.  If "clear" is true,
// also clear each mode that is found.
bool bmsrch(Buffer *bufp,bool clear,int modect,...) {
	BufMode *bmp0,*bmp1,*bmp2;
	ModeSpec *msp;
	va_list ap;
	int ct;
	bool modeFound = false;
	bool updateML = false;

#if MMDebug & Debug_Modes
	ModeSpec *msp2;
	if(clear)
		dumpmodes(bufp,true,"bmsrch(...,CLEAR,%d,...)\n",modect);
#endif
	// For each buffer mode...
	bmp0 = NULL;
	bmp1 = bufp->b_modes;
	while(bmp1 != NULL) {
		bmp2 = bmp1->bm_nextp;
		msp = bmp1->bm_modep;
#if MMDebug & Debug_Modes
		if(clear)
			fprintf(logfile,"    Found enabled buffer mode '%s'...\n",msp->ms_name);
#endif
		ct = modect;
		va_start(ap,modect);

		// For each mode argument...
		do {
#if MMDebug & Debug_Modes
			msp2 = va_arg(ap,ModeSpec *);
			if(clear)
				fprintf(logfile,"\tgot mode argument '%s'...\n",msp2->ms_name);
			if(msp2 == msp) {
#else
			if(va_arg(ap,ModeSpec *) == msp) {
#endif
				if(!clear)
					return true;
#if MMDebug & Debug_Modes
				fprintf(logfile,"    match found.  Clearing mode '%s'...\n",msp->ms_name);
#endif
				// Remove mode record from linked list.
				if(bmp0 == NULL)
					bufp->b_modes = bmp2;
				else
					bmp0->bm_nextp = bmp2;
				free((void *) bmp1);

				modeFound = true;

				// Check if mode line needs an update.
				if((msp->ms_flags & (MdHidden | MdInLine)) != MdHidden)
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
		supd_wflags(bufp,WFMode);

#if MMDebug & Debug_Modes
	if(clear)
		dumpmodes(bufp,false,"bmsrch() END\n");
#endif
	return modeFound;
	}

// Check if given mode (ModeSpec pointer) is set in a buffer and return Boolean result.  This is a fast version for checking
// a single mode.
bool bmsrch1(Buffer *bufp,ModeSpec *msp) {
	BufMode *bmp;

	// For each buffer mode...
	for(bmp = bufp->b_modes; bmp != NULL; bmp = bmp->bm_nextp)
		if(bmp->bm_modep == msp)
			return true;
	// Not found.
	return false;
	}

// Clear all modes in given buffer.  Return true if any mode was enabled; otherwise, false.
bool bmclear(Buffer *bufp) {
	BufMode *bmp1 = bufp->b_modes,*bmp2;
	bool modeWasChanged = (bmp1 != NULL);
	bool updateML = false;

#if MMDebug & Debug_Modes
	dumpmodes(bufp,true,"bmclear() BEGIN\n");
#endif
	while(bmp1 != NULL) {
		bmp2 = bmp1->bm_nextp;
		if((bmp1->bm_modep->ms_flags & (MdHidden | MdInLine)) != MdHidden)
			updateML = true;
		free((void *) bmp1);
		bmp1 = bmp2;
		}
	bufp->b_modes = NULL;
	if(updateML)
		supd_wflags(bufp,WFMode);
#if MMDebug & Debug_Modes
	dumpmodes(bufp,false,"bmclear() END\n");
#endif
	return modeWasChanged;
	}

// Clear a mode in all buffers.
void bmclearall(ModeSpec *msp) {
	Datum *datp;
	Array *aryp = &buftab;
	while((datp = aeach(&aryp)) != NULL)
		bmsrch(bufptr(datp),true,1,msp);
	}

// Set a mode in a buffer.  If clear is true, clear all existing modes first.  If clear is false and wasSetp not NULL, set
// *wasSetp to true if mode was already set; otherwise, false.  It is assumed that modep points to a valid buffer mode in the
// mode table.  Return status.
int bmset(Buffer *bufp,ModeSpec *msp,bool clear,bool *wasSetp) {
	BufMode *bmp0,*bmp1,*bmp2;
	bool wasSet = false;

	if(clear)						// "Clear modes" requested?
		bmclear(bufp);					// Yes, nuke 'em.
	else if(bmsrch1(bufp,msp)) {				// No.  Is mode already set?
		wasSet = true;					// Yes.
		goto Retn;
		}

#if MMDebug & Debug_Modes
	dumpmodes(bufp,true,"bmset(...,%s,%s,...)\n",msp->ms_name,clear ? "CLEAR" : "false");
#endif
	// Insert mode in linked list alphabetically and return.
	if((bmp2 = (BufMode *) malloc(sizeof(BufMode))) == NULL)
		return rcset(Panic,0,text94,"bmset");
			// "%s(): Out of memory!"
	bmp2->bm_modep = msp;
	bmp0 = NULL;
	bmp1 = bufp->b_modes;
	while(bmp1 != NULL) {
		if(strcasecmp(bmp1->bm_modep->ms_name,msp->ms_name) > 0)
			break;
		bmp0 = bmp1;
		bmp1 = bmp1->bm_nextp;
		}
	if(bmp0 == NULL)
		bufp->b_modes = bmp2;
	else
		bmp0->bm_nextp = bmp2;
	bmp2->bm_nextp = bmp1;
	if((msp->ms_flags & (MdHidden | MdInLine)) != MdHidden)
		supd_wflags(bufp,WFMode);
#if MMDebug & Debug_Modes
	dumpmodes(bufp,false,"bmset() END\n");
#endif
Retn:
	if(wasSetp != NULL)
		*wasSetp = wasSet;
	return rc.status;
	}

// Clear a buffer's filename, if any.
void clfname(Buffer *bufp) {

	if(bufp->b_fname != NULL) {
		(void) free((void *) bufp->b_fname);
		bufp->b_fname = NULL;
		}
	}

// Run "filename" hook on given buffer and return status.
int fnhook(Buffer *bufp) {

	return exechook(NULL,INT_MIN,hooktab + HkFilename,2,bufp->b_bname,bufp->b_fname);
	}

// Set buffer filename if possible and set *changedp (if not NULL) to true if filename was changed; otherwise, false.  Return
// status.
int setfname(Buffer *bufp,char *fname,bool *changedp) {
	bool fnChange = false;
	bool oldIsNull = (bufp->b_fname == NULL);
	bool newIsNull = (fname == NULL || *fname == '\0');

	if(oldIsNull) {
		if(!newIsNull) {
			fnChange = true;
			goto ClearSet;
			}
		}
	else if(newIsNull) {
		clfname(bufp);
		fnChange = true;
		}
	else {
		if(fnChange = (strcmp(bufp->b_fname,fname) != 0)) {
ClearSet:
			clfname(bufp);
			if((bufp->b_fname = (char *) malloc(strlen(fname) + 1)) == NULL)
				return rcset(Panic,0,text94,"setfname");
					// "%s(): Out of memory!"
			strcpy(bufp->b_fname,fname);
			}
		}

	if(changedp != NULL)
		*changedp = fnChange;
	if(!(si.opflags & OpScript))
		(void) mlerase();

	return rc.status;
	}

// Invalidate "last screen" pointer in any buffer pointing to given screen.
void nukebufsp(EScreen *scrp) {
	Datum *datp;
	Buffer *bufp;
	Array *aryp = &buftab;
	while((datp = aeach(&aryp)) != NULL) {
		bufp = bufptr(datp);
		if(bufp->b_lastscrp == scrp)
			bufp->b_lastscrp = NULL;
		}
	}

// Get the default buffer (a guess) for various buffer commands.  Search backward if BDefBack flag is set; otherwise, forward.
// If BDefTwo flag is set, proceed only if two visible buffers exist (active or inactive); otherwise, consider all visible
// buffers.  If BDefHidden flag is set, include hidden, non-macro buffers as well.  Return a pointer to the first buffer found,
// or NULL if none exist or search conditions not met.
Buffer *bdefault(ushort flags) {
	Buffer *bufp;

	// Two visible buffers only?
	if(flags & BDefTwo) {
		Datum *datp;
		ushort count = 0;
		Array *aryp = &buftab;
		while((datp = aeach(&aryp)) != NULL) {
			bufp = bufptr(datp);
			if(!(bufp->b_flags & BFHidden))
				++count;
			}
		if(count != 2)
			return NULL;
		}

	// Find the first buffer before or after the current one that meets the criteria.
	if(si.curblp == NULL) {
		ssize_t index;

		(void) bsrch(si.curbp->b_bname,&index);
		si.curblp = buftab.a_elpp + index;
		}
	Datum **blp = si.curblp + (flags & BDefBack ? -1 : 1);
	Datum **blpz = buftab.a_elpp + buftab.a_used;
	for(;;) {
		// Loop around buffer list if needed.
		if(blp < buftab.a_elpp || blp == blpz)
			blp = (flags & BDefBack) ? blpz - 1 : buftab.a_elpp;
		else if(blp == si.curblp)
			return NULL;
		else {
			bufp = bufptr(*blp);
			if(!(bufp->b_flags & BFHidden) ||
			 (!(bufp->b_flags & BFMacro) && (flags & (BDefHidden | BDefTwo)) == BDefHidden))
				break;
			blp += (flags & BDefBack ? -1 : 1);
			}
		}

	return bufp;
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
		fprintf(logfile,"\tl_nextp (%.08x) = '",(uint) lnp->l_nextp);
		if(lnp->l_nextp == NULL)
			fputs("'\n",logfile);
		else
			lput(lnp->l_nextp);
		fprintf(logfile,"\tl_prevp (%.08x) = '",(uint) lnp->l_prevp);
		if(lnp->l_prevp == NULL)
			fputs("'\n",logfile);
		else
			lput(lnp->l_prevp);
		}
	}
#endif

// Check if given buffer is empty and return Boolean result.  If bufp is NULL, check current buffer.
bool bempty(Buffer *bufp) {
	Line *lnp;

	if(bufp == NULL)
		bufp = si.curbp;
	lnp = bufp->b_lnp;
	return (lnp->l_nextp == NULL && lnp->l_used == 0);
	}

// Return true if dot is at beginning of current buffer; otherwise, false.  If dotp is NULL, use dot in current window.
bool bufbegin(Dot *dotp) {

	if(dotp == NULL)
		dotp = &si.curwp->w_face.wf_dot;
	return dotp->lnp == si.curbp->b_lnp && dotp->off == 0;
	}

// Return true if dot is at end of current buffer; otherwise, false.  If dotp is NULL, use dot in current window.
bool bufend(Dot *dotp) {

	if(dotp == NULL)
		dotp = &si.curwp->w_face.wf_dot;
	return dotp->lnp->l_nextp == NULL && dotp->off == dotp->lnp->l_used;
	}

// Inactivate all user marks that are outside the current narrowed buffer by negating their dot offsets.
static void mrkoff(void) {
	Mark *mkp;
	Line *lnp;

	// First, inactivate all user marks in current buffer.
	mkp = &si.curbp->b_mroot;
	do {
		if(mkp->mk_id <= '~')
			mkp->mk_dot.off = -(mkp->mk_dot.off + 1);
		} while((mkp = mkp->mk_nextp) != NULL);

	// Now scan the buffer and reactivate the marks that are still in the narrowed region.
	lnp = si.curbp->b_lnp;
	do {
		// Any mark match this line?
		mkp = &si.curbp->b_mroot;
		do {
			if(mkp->mk_dot.lnp == lnp && mkp->mk_dot.off < 0)
				mkp->mk_dot.off = -mkp->mk_dot.off - 1;
			} while((mkp = mkp->mk_nextp) != NULL);
		} while((lnp = lnp->l_nextp) != NULL);
	}

// Narrow to lines or region.  Makes all but the specified line(s) in the current buffer hidden and inaccessible.  Set rp to
// buffer name and return status.  Note that the last line in the narrowed portion of the buffer inherits the "last line of
// buffer" property; that is, it is assumed to not end with a newline delimiter.
int narrowBuf(Datum *rp,int n,Datum **argpp) {
	EScreen *scrp;
	EWindow *winp;
	Mark *mkp;
	Line *lnp,*lnp1,*lnpz;
	char *emsg;
	Dot *dotp = &si.curwp->w_face.wf_dot;

	// Make sure we aren't already narrowed or buffer is empty.
	if(si.curbp->b_flags & BFNarrowed) {
		emsg = text71;
			// "%s '%s' is already narrowed"
		goto ErrRtn;
		}
	if(bempty(NULL)) {
		emsg = text377;
			// "%s '%s' is empty"
ErrRtn:
		return rcset(Failure,0,emsg,text58,si.curbp->b_bname);
					// "Buffer"
		}

#if MMDebug & Debug_Narrow
	dumpbuffer("narrowBuf(): BEFORE",NULL,true);
#endif
	// Save faces of all windows displaying current buffer in a mark so they can be restored when buffer is widened.

	// In all screens...
	scrp = si.sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp->w_bufp == si.curbp) {

				// Found window attached to current buffer.  Save its face using the window's mark id.
				if(mfind(winp->w_id,&mkp,MkOpt_Create) != Success)
					return rc.status;
				mset(mkp,winp);
				}
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// Get the boundaries of the current region, if requested.
	if(n == INT_MIN)
		n = 1;
	else if(n < 0) {
		int i = n;

		// Move back abs(n) lines max.
		n = 1;
		while(dotp->lnp != si.curbp->b_lnp) {
			dotp->lnp = dotp->lnp->l_prevp;
			++n;
			if(++i == 0)
				break;
			}
		}
	else if(n == 0 && reglines(&n) != Success)
		return rc.status;

	// Current line is now at top of area to be narrowed and n is the number of lines (forward).
	lnp = dotp->lnp;
	lnp1 = si.curbp->b_lnp;						// Save original first line of buffer...
	lnpz = lnp1->l_prevp;						// and last line.

	// Archive the top fragment (hidden portion of buffer above narrowed region).
	if(lnp == lnp1)							// If current line is first line of buffer...
		si.curbp->b_ntoplnp = NULL;				// set old first line to NULL (no top fragment).
	else {
		si.curbp->b_ntoplnp = lnp1;				// Otherwise, save old first line of buffer...
		si.curbp->b_lnp = lnp;					// set new first line...
		lnp1->l_prevp = lnp->l_prevp;				// and save pointer to last line of fragment.
		}

	// Move dot forward to line just past the end of the narrowed region.
	do {
		if((dotp->lnp = dotp->lnp->l_nextp) == NULL) {	// If narrowed region extends to bottom of buffer...
			si.curbp->b_nbotlnp = NULL;		// set old first line to NULL (no bottom fragment)...
			si.curbp->b_lnp->l_prevp = lnpz;	// and point narrowed first line to original last line.
			goto FixMarks;
			}
		} while(--n > 0);

	// Narrowed region stops before EOB.  Archive the bottom fragment (hidden portion of buffer below narrowed region).
	(si.curbp->b_nbotlnp = dotp->lnp)->l_prevp->l_nextp = NULL;
								// Save first line of fragment, terminate line above it...
	si.curbp->b_lnp->l_prevp = dotp->lnp->l_prevp;		// set new last line of buffer...
	dotp->lnp->l_prevp = lnpz;				// and save pointer to last line of fragment.
FixMarks:
	// Inactivate marks outside of narrowed region.
	mrkoff();

	// In all screens...
	scrp = si.sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp->w_bufp == si.curbp) {

				// Found window attached to narrowed buffer.  Update its buffer settings.
				winp->w_face.wf_toplnp = winp->w_face.wf_dot.lnp = lnp;
				winp->w_face.wf_dot.off = winp->w_face.wf_firstcol = 0;
				winp->w_flags |= (WFHard | WFMode);
				}
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

#if MMDebug & Debug_Narrow
#if 0
	ldump("si.curbp->b_ntoplnp",si.curbp->b_ntoplnp);
	ldump("si.curbp->b_nbotlnp",si.curbp->b_nbotlnp);
	ldump("si.curbp->b_lnp",si.curbp->b_lnp);
	ldump("lnp",lnp);
#else
	dumpbuffer("narrowBuf(): AFTER",NULL,true);
#endif
#endif
	// and now remember we are narrowed.
	si.curbp->b_flags |= BFNarrowed;

	return dsetstr(si.curbp->b_bname,rp) != 0 ? librcset(Failure) : rcset(Success,0,text73,text58);
								// "%s narrowed","Buffer"
	}

// Restore a buffer to its pre-narrowed state.
static void unnarrow(Buffer *bufp) {
	EScreen *scrp;
	EWindow *winp;
	Line *lnp,*lnp1,*lnpz;
	Mark *mkp;

#if MMDebug & Debug_Narrow
	dumpbuffer("unnarrow(): BEFORE",NULL,true);
#endif
	// Get narrowed first and last lines.
	lnp1 = bufp->b_lnp;
	lnpz = lnp1->l_prevp;

	// Recover the top fragment.
	if(bufp->b_ntoplnp != NULL) {
		lnp = (bufp->b_lnp = bufp->b_ntoplnp)->l_prevp;
							// Point buffer to real first line, get last line of fragment...
		lnp->l_nextp = lnp1;			// point last line and narrowed first line to each other...
		lnp1->l_prevp = lnp;
		bufp->b_ntoplnp = NULL;			// and deactivate top fragment.
		}

	// Recover the bottom fragment.
	if(bufp->b_nbotlnp == NULL)			// If none...
		bufp->b_lnp->l_prevp = lnpz;		// point recovered first line going backward to real last line.
	else {						// otherwise, adjust point and marks.
		// Connect the bottom fragment.
		lnp = bufp->b_nbotlnp->l_prevp;		// Get last line of fragment.
		lnpz->l_nextp = bufp->b_nbotlnp;	// Point narrowed last line and first line of fragment to each other...
		bufp->b_nbotlnp->l_prevp = lnpz;
		bufp->b_lnp->l_prevp = lnp;		// point first line going backward to last line...
		bufp->b_nbotlnp = NULL;			// and deactivate bottom fragment.
		}

	// Activate all marks in buffer.
	mkp = &bufp->b_mroot;
	do {
		if(mkp->mk_dot.off < 0)
			mkp->mk_dot.off = -mkp->mk_dot.off - 1;
		} while((mkp = mkp->mk_nextp) != NULL);

	// Restore faces of all windows displaying current buffer from the window's mark if it exists.

	// In all screens...
	scrp = si.sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp != si.curwp && winp->w_bufp == si.curbp) {

				// Found window attached to widened buffer.  Restore its face.
				(void) mfind(winp->w_id,&mkp,MkOpt_Wind);	// Can't return an error.
				if(mkp != NULL) {
					winp->w_face.wf_dot = mkp->mk_dot;
					winp->w_rfrow = mkp->mk_rfrow;
					winp->w_flags |= WFReframe;
					}
				}
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// Set hard update in front screen only...
	supd_wflags(bufp,WFHard | WFMode);

#if MMDebug & Debug_Narrow
	dumpbuffer("unnarrow(): AFTER",NULL,true);
#endif
	// and now forget that we are narrowed.
	bufp->b_flags &= ~BFNarrowed;
	}

// Widen (restore) a narrowed buffer.  Set rp to buffer name and return status.
int widenBuf(Datum *rp,int n,Datum **argpp) {

	// Make sure we are narrowed.
	if(!(si.curbp->b_flags & BFNarrowed))
		return rcset(Failure,0,text74,text58,si.curbp->b_bname);
			// "%s '%s' is not narrowed","Buffer"

	// Restore current buffer to pre-narrowed state.
	unnarrow(si.curbp);
	if(dsetstr(si.curbp->b_bname,rp) != 0)
		return librcset(Failure);
	(void) rcset(Success,0,text75,text58);
		// "%s widened","Buffer"
	return execCF(rp,INT_MIN,cftab + cf_reframeWind,0,0);
	}

// binsearch() helper function for returning a buffer name, given table (array) and index.
static char *bufname(void *table,ssize_t i) {

	return bufptr(((Datum **) table)[i])->b_bname;
	}

// Search the buffer list for given name and return pointer to Buffer object if found; otherwise, NULL.  In either case, set
// *indexp to target array index if indexp not NULL.
Buffer *bsrch(char *bname,ssize_t *indexp) {
	ssize_t i;
	bool found = binsearch(bname,(void *) buftab.a_elpp,buftab.a_used,strcmp,bufname,&i);
	if(indexp != NULL)
		*indexp = i;
	return found ? bufptr(buftab.a_elpp[i]) : NULL;
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
static Datum *delistbuf(Buffer *bufp) {
	Datum *datp;
	ssize_t index;

	(void) bsrch(bufp->b_bname,&index);		// Can't fail.
	datp = adelete(&buftab,index);
	si.curblp = NULL;
	return datp;
	}

// Insert buffer into buffer list at given index and update si.curblp if needed.  Return status.
static int enlistbuf(Datum *datp,ssize_t index) {

	if(ainsert(&buftab,index,datp,false) != 0)
		return librcset(Failure);
	si.curblp = NULL;
	return rc.status;
	}

// Initialize dot position, marks, first column position, and I/O delimiters of a buffer.
static void bufinit(Buffer *bufp,Line *lnp) {

	faceinit(&bufp->b_face,lnp,bufp);
	*bufp->b_inpdelim = '\0';
	bufp->b_inpdelimlen = 0;
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
int bextend(Buffer *bufp) {
	MacInfo *mip;

	if((bufp->b_mip = mip = (MacInfo *) malloc(sizeof(MacInfo))) == NULL)
		return rcset(Panic,0,text94,"bextend");
			// "%s(): Out of memory!"
	mip->mi_minArgs = 0;
	mip->mi_maxArgs = -1;
	mip->mi_nexec = 0;
	mip->mi_execp = NULL;
	dinit(&mip->mi_usage);
	dinit(&mip->mi_desc);
	return rc.status;
	}

// Find a buffer by name and return status or Boolean result.  Actions taken depend on cflags:
//	If BS_File is set:
//		Use the base filename of "name" as the default buffer name; otherwise, use "name" directly.
//	If BS_Force is set:
//		Create a unique buffer name derived from the default one.  (BS_Create is assumed to also be set.)
//	If BS_Create is set:
//		If the buffer is found and BS_Force is not set:
//			Set *bufpp (if bufpp not NULL) to the buffer pointer, set *createdp (if createdp not NULL) to false,
//			and return status.
//		If the buffer is not found or BS_Force is set:
//			Create a buffer, set it's flag word to bflags, create a buffer-extension record if BS_Extend is set,
//			run "createBuf" hook if BS_Hook is set, set *bufpp (if bufpp not NULL) to the buffer pointer, set
//			*createdp (if createdp not NULL) to true, and return status.
//	If BS_Create is not set:
//		If the buffer is found:
//			Set *bufpp (if bufpp not NULL) to the buffer pointer and return true, ignoring createdp.
//		If the buffer is not found:
//			Return false, ignoring bufpp and createdp.
// In all cases, if a buffer is created and BS_Hook is set, the "createBuf" hook is executed before returning.
int bfind(char *name,ushort cflags,ushort bflags,Buffer **bufpp,bool *createdp) {
	ssize_t index;
	Buffer *bufp;
	char *bname,wkbuf[MaxBufName + 1];

	// Set default buffer name.
	if(cflags & BS_Force)
		(void) bsrch(bname = (cflags & BS_File) ? bunique(wkbuf,name) : bunique(strcpy(wkbuf,name),NULL),
		cflags & BS_Create ? &index : NULL);
	else {
		if(cflags & BS_File)
			fbname(bname = wkbuf,name);
		else
			bname = name;

		// Search for the buffer.
		if((bufp = bsrch(bname,cflags & BS_Create ? &index : NULL)) != NULL) {

			// Found it.  Return results.
			if(bufpp != NULL)
				*bufpp = bufp;
			if(cflags & BS_Create) {
				if(createdp != NULL)
					*createdp = false;
				return rc.status;
				}
			return true;
			}
		}

	// No such buffer exists, create it?
	if(cflags & BS_Create) {
		Line *lnp;
		Datum *datp;
		Buffer buf;

		// Valid buffer name?
		if(!isbname(bname))
			return rcset(Failure,0,text128,bname);
					// "Invalid buffer name '%s'"
		// Macro name?
		if(*bname == SBMacro && !(bflags & BFMacro))
			return rcset(Failure,0,text268,text180,bname,SBMacro);
					// "Cannot %s buffer: name '%s' cannot begin with %c","create"

		// Allocate memory for the first line.
		if(lalloc(0,&lnp) != Success)
			return rc.status;	// Fatal error.
		buf.b_lnp = lnp->l_prevp = lnp;
		lnp->l_nextp = NULL;

		// Create buffer extension if requested...
		if(!(cflags & BS_Extend))
			buf.b_mip = NULL;
		else if(bextend(&buf) != Success)
			return rc.status;

		// and set up the other buffer fields.
		buf.b_mroot.mk_nextp = NULL;
		bufinit(&buf,lnp);
		buf.b_ntoplnp = buf.b_nbotlnp = NULL;
		buf.b_flags = bflags | BFActive;
		buf.b_modes = NULL;
		buf.b_nwind = buf.b_nalias = 0;
		buf.b_lastscrp = NULL;
		buf.b_fname = NULL;
		strcpy(buf.b_bname,bname);

		// Insert a copy of the Buffer object into the list using slot index from bsrch() call and an
		// untracked Datum object.
		if(dnew(&datp) != 0 || dsetblob((void *) &buf,sizeof(Buffer),datp) != 0)
	 		return librcset(Failure);
	 	if(enlistbuf(datp,index) != Success)
	 		return rc.status;

		// Add macro name to exectab hash...
		if(bflags & BFMacro) {
			UnivPtr univ;
			Buffer *tbufp = bufptr(datp);

			univ.p_type = (tbufp->b_flags & BFConstrain) ? PtrMacro_C : PtrMacro_O;
			if(execfind((univ.u.p_bufp = tbufp)->b_bname + 1,OpCreate,univ.p_type,&univ) != Success)
				return rc.status;
			}

		// return results to caller...
		if(bufpp != NULL)
			*bufpp = bufptr(datp);
		if(createdp != NULL)
			*createdp = true;

		// run createBuf hook if requested and return.
		return (cflags & BS_Hook) ? exechook(NULL,INT_MIN,hooktab + HkCreateBuf,1,bname) : rc.status;
		}

	// Buffer not found and not creating.
	return false;
	}

// Free all line storage in given buffer and reset window pointers.  Return status.
static int bfree(Buffer *bufp) {
	Line *lnp,*lnp1;
	EScreen *scrp;
	EWindow *winp;

	// Free all line objects except the first.
	for(lnp = bufp->b_lnp->l_prevp; lnp != bufp->b_lnp; lnp = lnp1) {
		lnp1 = lnp->l_prevp;
		free((void *) lnp);
		}

	// Free first line of buffer.  If allocated size is small, keep line and simply set "used" size to zero.  Otherwise,
	// allocate a new empty line for buffer so that memory is freed.
	if(lnp->l_size <= (NBlock << 1)) {
		lnp->l_used = 0;
		lnp->l_prevp = lnp;
		}
	else {
		if(lalloc(0,&lnp1) != Success)
			return rc.status;		// Fatal error.
		free((void *) lnp);
		bufp->b_lnp = lnp1->l_prevp = lnp = lnp1;
		}
	lnp->l_nextp = NULL;

	// Reset window line links.

	// In all screens...
	scrp = si.sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp->w_bufp == bufp)
				faceinit(&winp->w_face,lnp,bufp);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	return rc.status;
	}

// This routine blows away all of the text in a buffer.  If it's marked as changed, it will not be cleared unless BC_IgnChgd is
// set in "flags" or the user gives the okay.  This to save the user the grief of losing text.  If BC_ClrFilename is set, the
// filename associated with the buffer is set to null.  If the buffer is narrowed and BC_Unnarrow is set, the buffer is silently
// unnarrowed before being cleared; otherwise, the user is prompted to give the okay before proceeding.  The window chain is
// nearly always wrong if this routine is called; the caller must arrange for the updates that are required.
//
// Return success if buffer is erased; otherwise, "cancelled", if user changed his or her mind.
int bclear(Buffer *bufp,ushort flags) {
	bool yep;
	bool bufErased = false;
	bool eraseML = false;

	// Executing buffer?
	if(bufp->b_mip != NULL && bufp->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text226,text264,text238,bufp->b_bname);
			// "Cannot %s %s buffer '%s'","clear","executing"

	// Changed buffer?  Skip if need "narrowed buffer" confirmation as well (which preempts this).
	if((bufp->b_flags & BFChanged) && !(flags & BC_IgnChgd) && (!(bufp->b_flags & BFNarrowed) || (flags & BC_Unnarrow))) {
		char *str,prmt[NWork];
		eraseML = true;

		// Get user confirmation.
		if((si.opflags & OpScript) || (flags & BC_Multi)) {
			sprintf(stpcpy(prmt,text32),text369,bufp->b_bname);
				// "Discard changes"," in buffer '%s'"
			str = prmt;
			}
		else
			str = text32;
		if(terminpYN(str,&yep) != Success)
			return rc.status;
		if(!yep)
			goto ClearML;
		}

	// Narrowed buffer?
	if(bufp->b_flags & BFNarrowed) {

		// Force?
		if(flags & BC_Unnarrow)

			// Yes, restore buffer to pre-narrowed state.
			unnarrow(bufp);
		else if(!(flags & BC_IgnChgd)) {

			// Not a force.  Get user confirmation (and leave narrowed).
			eraseML = true;
			if(terminpYN(text95,&yep) != Success)
					// "Narrowed buffer... delete current portion"
				return rc.status;
			if(!yep)
				goto ClearML;
			}
		}

	// It's a go... erase it.
	if(flags & BC_ClrFilename)
		clfname(bufp);					// Zap any filename (buffer will never be narrowed).
	if(bfree(bufp) != Success)				// Free all line storage and reset line pointers.
		return rc.status;
	bchange(bufp,WFHard | WFMode);				// Update window flags.
	if(bufp->b_flags & BFNarrowed) {			// If narrowed buffer...
		bufp->b_flags |= BFChanged;			// mark as changed...
		faceinit(&bufp->b_face,bufp->b_lnp,NULL);	// reset buffer line pointers and...
		Mark *mkp = &bufp->b_mroot;			// set all non-window visible marks to beginning of first line.
		do {
			if(mkp->mk_id <= '~' && mkp->mk_dot.off >= 0) {
				mkp->mk_dot.lnp = bufp->b_lnp;
				mkp->mk_dot.off = 0;
				}
			} while((mkp = mkp->mk_nextp) != NULL);
		}
	else {
		bufp->b_flags &= ~BFChanged;			// Otherwise, mark as not changed...
		bufinit(bufp,bufp->b_lnp);			// reset buffer line pointers, and delete marks.
		}
	bufErased = true;

	if(eraseML)
ClearML:
		(void) mlerase();
	return bufErased ? rc.status : rcset(Cancelled,0,NULL);
	}

// Find a window displaying given buffer and return it, giving preference to current screen and current window.
EWindow *findwind(Buffer *bufp) {
	EScreen *scrp;
	EWindow *winp;

	// If current window is displaying the buffer, use it.
	if(si.curwp->w_bufp == bufp)
		return si.curwp;

	// In current screen...
	winp = si.wheadp;
	do {
		if(winp->w_bufp == bufp)
			goto Retn;
		} while((winp = winp->w_nextp) != NULL);

	// In all other screens...
	scrp = si.sheadp;
	do {
		if(scrp == si.cursp)			// Skip current screen.
			continue;
		if(scrp->s_curwp->w_bufp == bufp)	// Use current window first.
			return scrp->s_curwp;

		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp->w_bufp == bufp)
				goto Retn;
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);
Retn:
	return winp;
	}

// Change zero or more buffer attributes (flags), given result pointer and action (n < 0: clear, n == 0 (default): toggle,
// n == 1: set, or n > 1: clear all and set).  Mutable attributes are: "Changed", "Hidden", and "TermAttr".
// If interactive:
//	. Read one single-letter attribute.
//	. Use current buffer if default n; otherwise, prompt for buffer name.
// If script mode:
//	. Get buffer name from argpp[0].
//	. Use current buffer if buffer argument is nil.
//	. Get zero or more attribute (keyword) arguments.
// Perform operation, set rp to former state (-1 or 1) of last attribute altered (or zero if n > 1 and no arguments given), and
// return status.
int alterBufAttr(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	BufFlagSpec *bfsp;
	Datum *datp;
	char *keyword;
	long formerState = 0;
	ushort oldflags,newflags0,wflags;
	ushort newflags = 0;
	int action = (n == INT_MIN) ? 0 : n;

	if(dnewtrk(&datp) != 0)
		goto LibFail;

	// Get attribute if interactive.
	if(!(si.opflags & OpScript)) {
		DStrFab prompt;
		char *attrname;
		char *lead = " (";

		// Build prompt.
		if(dopentrk(&prompt) != 0 || dputf(&prompt,"%s %s %s",
		 action < 0 ? text65 : action == 0 ? text231 : action == 1 ? text64 : text296,text83,text186) != 0)
				// "Clear","Toggle","Set","Clear all and set"		"buffer","attribute"
			goto LibFail;
		bfsp = bflaginfo;
		do {
			if(bfsp->abbr != NULL) {
				attrname = (term.t_ncol >= 80) ? bfsp->name : bfsp->abbr;
				if(dputf(&prompt,"%s~u~b%c~0%s",lead,(int) attrname[0],attrname + 1) != 0)
					goto LibFail;
				lead = ", ";
				}
			} while((++bfsp)->name != NULL);
		if(dputc(')',&prompt) != 0 || dclose(&prompt,sf_string) != 0)
			goto LibFail;

		// Get attribute from user.
		if(terminp(datp,prompt.sf_datp->d_str,ArgNotNull1 | ArgNil1,Term_LongPrmt | Term_Attr | Term_OneChar,
		 NULL) != Success || (action <= 1 && datp->d_type == dat_nil))
			return rc.status;
		dsetchr(datp->u.d_int,datp);

		// If default n, use current buffer.
		if(n == INT_MIN)
			bufp = si.curbp;
		else {
			// n is not the default.  Get buffer name.  If nothing is entered, bag it.
			bufp = bdefault(BDefTwo);
			if(bcomplete(rp,text229 + 2,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success ||
					// ", in"
			 rp->d_type == dat_nil)
				return rc.status;

			// If "Clear all and set" and no attribute was selected, skip attribute processing.
			if(action > 1 && datp->d_type == dat_nil)
				goto Done;
			}
		goto DoFlag;
		}
	else {
		// Script mode.  Get buffer name argument.
		datxfer(rp,argpp[0]);
		if((bufp = bsrch(rp->d_str,NULL)) == NULL)
			return rcset(Failure,0,text118,rp->d_str);
				// "No such buffer '%s'"

		// Get attribute arguments.
		bool req = (action <= 1);
		do {
			if(!req && !havesym(s_comma,false))
				break;					// No arguments left.
			if(funcarg(datp,ArgNotNull1) != Success)
				return rc.status;
			req = false;

			// Scan buffer flag table for a match.
DoFlag:
			keyword = datp->d_str;
			bfsp = bflaginfo;
			do {
				if(bfsp->abbr == NULL)
					continue;
				if(!(si.opflags & OpScript)) {
					if(*keyword == bfsp->name[0]) {
						formerState = (bufp->b_flags & (newflags = bfsp->mask)) ? 1L : -1L;
						goto Done;
						}
					}
				else if(strcasecmp(keyword,bfsp->name) == 0) {
					formerState = (bufp->b_flags & bfsp->mask) ? 1L : -1L;
					newflags |= bfsp->mask;
					goto Onward;
					}
				} while((++bfsp)->name != NULL);
			return rcset(Failure,0,text374,si.opflags & OpScript ? keyword : vizc(*keyword,VBaseDef));
					// "Invalid buffer attribute '%s'"
Onward:;
			} while(si.opflags & OpScript);
		}
Done:
	// Have flag(s) and buffer.  Perform operation.
	oldflags = newflags0 = bufp->b_flags;
	if(action > 1)
		newflags0 &= ~(BFChanged | BFHidden | BFTermAttr);
	if(newflags != 0) {
		if(action < 0)
			newflags0 &= ~newflags;			// Clear.
		else {
			if((newflags & BFTermAttr) && (bufp->b_flags & BFMacro))
				return rcset(Failure,RCNoFormat,text376);
					// "Cannot enable terminal attributes in a macro buffer"
			if(action > 0)
				newflags0 |= newflags;		// Set.
			else
				newflags0 ^= newflags;		// Toggle.
			}
		}
	bufp->b_flags = newflags0;

	// Set window flags if needed.
	wflags = (oldflags & BFChanged) != (newflags0 & BFChanged) ? WFMode : 0;
	if((oldflags & BFTermAttr) != (bufp->b_flags & BFTermAttr))
		wflags |= WFHard;
	if(wflags)
		supd_wflags(bufp,wflags);

	// Return former state of last attribute that was changed.
	dsetint(formerState,rp);

	// Wrap it up.
	return (si.opflags & OpScript) ? rc.status : (newflags == BFChanged) ? mlerase() : rcset(Success,RCNoFormat,text375);
												// "Attribute(s) changed"
LibFail:
	return librcset(Failure);
	}

// Get a buffer name (if n not default) and perform operation on buffer according to "op" argument.  If prmt == NULL (bgets
// function), set rp to function return value; otherwise, buffer name.  Return status.
int bufop(Datum *rp,int n,char *prmt,uint op,int flag) {
	Dot *dotp;
	Buffer *bufp = NULL;

	// Get the buffer name.  n will never be the default for a bgets() call.
	if(n == INT_MIN)
		bufp = si.curbp;
	else {
		if(prmt != NULL)
			bufp = bdefault(BDefTwo);
		if(bcomplete(rp,prmt,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success || bufp == NULL)
			return rc.status;
		}

	// Perform requested operation (BOpGotoLn, BOpBeginEnd, or BOpReadBuf).
	if(op == BOpGotoLn && flag == 0) {
		op = BOpBeginEnd;
		flag = true;
		}

	// Move dot in buffer... usually a massive adjustment.  Grab dot.  If the buffer is being displayed, get from window
	// face instead of buffer face.
	if(bufp->b_nwind > 0) {
		EWindow *winp = findwind(bufp);
		winp->w_flags |= WFMove;		// Set "hard motion" flag.
		dotp = &winp->w_face.wf_dot;
		}
	else
		dotp = &bufp->b_face.wf_dot;

	// Perform operation.
	switch(op) {
		case BOpBeginEnd:
			// Go to beginning or end of buffer.
			if(flag) {		// EOB
				dotp->lnp = bufp->b_lnp->l_prevp;
				dotp->off = dotp->lnp->l_used;
				}
			else {			// BOB
				dotp->lnp = bufp->b_lnp;
				dotp->off = 0;
				}
			break;
		case BOpGotoLn:
			// Go to beginning of buffer and count lines.
			dotp->lnp = bufp->b_lnp;
			dotp->off = 0;
			if(bufp == si.curbp)
				return moveln(flag - 1);
			for(--flag; flag > 0; --flag) {
				if(dotp->lnp->l_nextp == NULL)
					break;
				dotp->lnp = dotp->lnp->l_nextp;
				}
			break;
		default:	// BOpReadBuf
			// Read next line from buffer n times.
			while(n-- > 0) {
				size_t len;

				// If we are at EOB, return nil.
				if(bufend(dotp)) {
					dsetnil(rp);
					return rc.status;
					}

				// Get the length of the current line (from dot to end)...
				len = dotp->lnp->l_used - dotp->off;

				// return the spoils...
				if(dsetsubstr(dotp->lnp->l_text + dotp->off,len,rp) != 0)
					return librcset(Failure);

				// and step the buffer's line pointer ahead one line (or to end of last line).
				if(dotp->lnp->l_nextp == NULL)
					dotp->off = dotp->lnp->l_used;
				else {
					dotp->lnp = dotp->lnp->l_nextp;
					dotp->off = 0;
					}
				}
		}

	return rc.status;
	}

// Set name of a system (internal) buffer and call bfind();
int sysbuf(char *root,Buffer **bufpp,ushort flags) {
	char bname[strlen(root) + 2];

	bname[0] = BSysLead;
	strcpy(bname + 1,root);
	return bfind(bname,BS_Create | BS_Force,BFHidden | flags,bufpp,NULL);
	}

// Activate a buffer if needed.  Return status.
int bactivate(Buffer *bufp) {

	// Check if buffer is active.
	if(!(bufp->b_flags & BFActive)) {

		// Not active: read attached file.
		(void) readin(bufp,NULL,RWKeep | RWFNHook | RWStats);
		}

	return rc.status;
	}

// Insert a buffer into the current buffer and set current region to inserted lines.  If n == 0, leave point before the inserted
// lines; otherwise, after.  Return status.
int insertBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	uint lineCt = n;

	// Get the buffer name.  Reject if current buffer.
	bufp = bdefault(BDefTwo);
	if(bcomplete(rp,text55,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success)
			// "Insert"
		return rc.status;
	dclear(rp);
	if(bufp == NULL) {
		if(!(si.opflags & OpScript))
			return rc.status;
		bufp = si.curbp;
		}
	if(bufp == si.curbp)
		return rcset(Failure,RCNoFormat,text124);
			// "Cannot insert current buffer into itself"

	// Let user know what's up.
	if(mlputs(MLHome | MLWrap | MLFlush,text153) != Success)
			// "Inserting data..."
		return rc.status;

	// Prepare buffer to be inserted.  Return after activation if buffer is empty.
	if(bactivate(bufp) != Success)
		return rc.status;
	if(bempty(bufp))
		return rcset(Success,RCHigh,"%s 0 %ss",text154,text205);
						// "Inserted","line"

	// Buffer not empty.  Insert lines and report results.
	return inslines(&lineCt,bufp,si.curbp,NULL,NULL,NULL) != Success ? rc.status :
	 rcset(Success,RCHigh,"%s %u %s%s%s",text154,lineCt,text205,lineCt == 1 ? "" : "s",text355);
					// "Inserted","line"," and marked as region"
	}

// Attach a buffer to the current window, creating it if necessary (default).  Render buffer and return status.
int selectBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	bool created;
	char *prmt;
	uint op;

	// Get the buffer name.
	bufp = bdefault(BDefTwo);
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
	if(bcomplete(rp,prmt,bufp != NULL ? bufp->b_bname : NULL,op,&bufp,&created) != Success || bufp == NULL)
		return rc.status;

	// Render the buffer.
	return render(rp,n == INT_MIN ? 1 : n,bufp,created ? RendNewBuf | RendNotify : 0);
	}

// Display a file or buffer in a pop-up window with options.  Return status.
int dopop(Datum *rp,int n,bool popbuf) {
	Buffer *bufp;
	ushort bflags,rflags;

	// Get buffer name or filename.
	if(popbuf) {
		// Get buffer name.
		bufp = bdefault(BDefTwo);
		if(bcomplete(rp,text27,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success || bufp == NULL)
				// "Pop"
			return rc.status;
		rflags = 0;
		}
	else {
		bool created;
		ushort cflags = n <= 0 && n != INT_MIN ? BS_Create | BS_File : BS_Create | BS_File | BS_Force;

		// Get filename and optionally, read file into a new buffer.
		if(gtfilename(rp,text299,NULL,0) != Success || rp->d_type == dat_nil ||
				// "Pop file"
		 bfind(rp->d_str,cflags,0,&bufp,&created) != Success || (created &&
		 readin(bufp,rp->d_str,RWKeep | RWNoHooks | RWExist) != Success))
			return rc.status;
		rflags = created ? RendNewBuf : 0;
		}
	bflags = bufp->b_flags;		// Save buffer flags for possible restore after pop.

	// Get option letters and validate them, if applicable.
	if(n != INT_MIN) {
		char *str;

		if(si.opflags & OpScript) {
			if(funcarg(rp,ArgNotNull1) != Success)
				return rc.status;
			}
		else if(terminp(rp,term.t_ncol >= 80 ? text223 : text224,ArgNotNull1,Term_LongPrmt | Term_Attr,NULL) != Success)
				// "Options (~u~ba~0lt mode line, ~u~bd~0elete buffer, ~u~bs~0hift long lines, ~u~bt~0ermattr)"
				// "Opts (~u~ba~0ltml, ~u~bd~0el, ~u~bs~0hift, ~u~bt~0attr)"
			return rc.status;
		str = rp->d_str;
		do {
			switch(*str) {
				case 'a':
					rflags |= RendAltML;
					break;
				case 'd':
					rflags |= RendNewBuf;
					break;
				case 's':
					rflags |= RendShift;
					break;
				case 't':
					if(bufp->b_flags & BFMacro)
						return rcset(Failure,RCNoFormat,text376);
							// "Cannot enable terminal attributes in a macro buffer"
					bufp->b_flags |= BFTermAttr;
					break;
				default:
					return rcset(Failure,0,text250,vizc(*str,VBaseDef));
						// "Unknown pop option '%s'"
				}
			} while(*++str != '\0');
		}

	// Render the buffer.
	if(render(rp,-1,bufp,rflags) == Success && !(rflags & RendNewBuf))
		bufp->b_flags = bflags;
	return rc.status;
	}

// Create a scratch buffer; that is, one with a unique name and no filename associated with it.  Set dest to name, *bufpp to
// buffer pointer, and return status.
int bscratch(char *dest,Buffer **bufpp) {
	uint maxtries = 100;
	Buffer *bufp;
	bool created;

	// Create a buffer with a unique name.
	do {
		sprintf(dest,"%s%ld",Scratch,xorshift64star(1000) - 1);
		if(bfind(dest,BS_Create | BS_Hook,0,&bufp,&created) != Success)
			return rc.status;

		// Was a new buffer created?
		if(created)
			goto Retn;	// Yes, use it.

		// Nope, a buffer with that name already exists.  Try again unless limit reached.
		} while(--maxtries > 0);

	// Random-number approach failed... let bfind() "uniquify" it.
	(void) bfind(dest,BS_Create | BS_Hook | BS_Force,0,&bufp,NULL);
Retn:
	*bufpp = bufp;
	return rc.status;
	}

// Create a scratch buffer.  Render buffer and return status.
int scratchBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	char bname[MaxBufName + 1];

	// Create buffer...
	if(bscratch(bname,&bufp) != Success)
		return rc.status;

	// and render it.
	return render(rp,n == INT_MIN ? 1 : n,bufp,RendNewBuf);
	}

// Run exit-buffer hook (and return result) or enter-buffer hook.  Return status.
int bhook(Datum *rp,bool exitHook) {

	if(!(si.curbp->b_flags & BFMacro)) {
		if(exitHook) {
			// Run exit-buffer user hook on current (old) buffer.
			dsetnil(rp);
			(void) exechook(rp,INT_MIN,hooktab + HkExitBuf,0);
			}
		else
			// Run enter-buffer user hook on current (new) buffer.
			(void) exechook(rp,INT_MIN,hooktab + HkEnterBuf,0x21,rp);
		}
	return rc.status;
	}

// Make given buffer current, update s_lastbufp variable in current screen, and return status.  The top line, dot, and column
// offset values from the current window are saved in the old buffer's header and replacement ones are fetched from the new
// (given) buffer's header.
int bswitch(Buffer *bufp) {
	Datum *rp;

	// Nothing to do if bufp is current buffer.
	if(bufp == si.curbp)
		return rc.status;

	// Run exit-buffer user hook on current (old) buffer.
	if(dnewtrk(&rp) != 0)
		return librcset(Failure);
	if(bhook(rp,true) != Success)
		return rc.status;

	// Decrement window use count of current (old) buffer and save the current window settings.
	if(--si.curbp->b_nwind == 0)
		si.curbp->b_lastscrp = si.cursp;
	wftobf(si.curwp,si.curbp);
	si.cursp->s_lastbufp = si.curbp;

	// Switch to new buffer.
	si.curwp->w_bufp = si.curbp = bufp;
	si.curblp = NULL;
	++si.curbp->b_nwind;

	// Activate buffer.
	if(bactivate(si.curbp) <= MinExit)
		return rc.status;

	// Update window settings.
	bftowf(si.curbp,si.curwp);

	// Run enter-buffer user hook on current (new) buffer.
	if(rc.status == Success)
		(void) bhook(rp,false);

	return rc.status;
	}

// Switch to the previous or next visible buffer in the buffer list n times.  Set rp to name of final buffer if successful.  If
// n == 0, switch once and include hidden, non-macro buffers as candidates.  If n < 0, switch once and delete the current
// buffer.  Return status.
int pnbuffer(Datum *rp,int n,ushort flags) {
	Buffer *bufp0,*bufp1;
	ushort bflags;

	if(n == INT_MIN)
		n = 1;
	else if(n == 0)
		flags |= BDefHidden;

	// Cycle backward or forward through buffer list n times, or just once if n <= 0.
	do {
		if((bufp1 = bdefault(flags)) == NULL)
			return rc.status;		// Only one visible buffer.
		bufp0 = si.curbp;
		bflags = bufp1->b_flags;
		if(bswitch(bufp1) != Success)
			return rc.status;
		if(n < 0) {
			char bname[MaxBufName + 1];
			strcpy(bname,bufp0->b_bname);
			if(bdelete(bufp0,0) != Success || mlprintf(MLHome | MLWrap | MLFlush,text372,bname) != Success)
									// "Buffer '%s' deleted"
				return rc.status;
			if(!(bflags & BFActive))
				cpause(50);
			}
		} while(--n > 0);

	return dsetstr(bufp1->b_bname,rp) != 0 ? librcset(Failure) : rc.status;
	}

// Clear current buffer, or named buffer if n >= 0.  Force it if n != 0.  Set rp to false if buffer is not cleared; otherwise,
// true.  Return status.
int clearBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;

	// Get the buffer name to clear.
	if(n < 0)
		bufp = si.curbp;
	else {
		bufp = (n >= 0) ? bdefault(BDefTwo) : NULL;
		if(bcomplete(rp,text169,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success || bufp == NULL)
				// "Clear"
			return rc.status;
		}

	// Blow text away unless user got cold feet.
	if(bclear(bufp,n != 0 ? BC_IgnChgd : 0) >= Cancelled) {
		dsetbool(rc.status == Success,rp);
		if(n >= 0)
			(void) mlerase();
		}

	return rc.status;
	}

// Delete marks, given buffer pointer.  Free all visible user marks, plus invisible user marks if MkOpt_Viz flag is not set,
// plus window marks if MkOpt_Wind flag is set -- then set root mark to default.
void mdelete(Buffer *bufp,ushort flags) {
	Mark *mkp0,*mkp1,*mkp2;
	Mark *mkp = &bufp->b_mroot;

	// Free marks in list.
	for(mkp1 = (mkp0 = mkp)->mk_nextp; mkp1 != NULL; mkp1 = mkp2) {
		mkp2 = mkp1->mk_nextp;
		if((mkp1->mk_id <= '~' && (mkp1->mk_dot.off >= 0 || !(flags & MkOpt_Viz))) ||
		 (mkp1->mk_id > '~' && (flags & MkOpt_Wind))) {
			(void) free((void *) mkp1);
			mkp0->mk_nextp = mkp2;
			}
		else
			mkp0 = mkp1;
		}

	// Initialize root mark to end of buffer.
	mkp->mk_id = RegMark;
	mkp->mk_dot.off = (mkp->mk_dot.lnp = bufp->b_lnp->l_prevp)->l_used;
	mkp->mk_rfrow = 0;
	}

// Delete the buffer pointed to by bufp.  Don't allow if buffer is being displayed, executed, or aliased.  Pass "flags" with
// BC_Unnarrow set to bclear() to clear the buffer, then free the header line and the buffer block.  Also delete any key binding
// and remove the exectab entry if it's a macro.  Return status, including Cancelled if user changed his or her mind.
int bdelete(Buffer *bufp,ushort flags) {
	UnivPtr univ;
	KeyBind *kbp;

	// We cannot nuke a displayed buffer.
	if(bufp->b_nwind > 0)
		return rcset(Failure,0,text28,text58);
			// "%s is being displayed","Buffer"

	// We cannot nuke an executing buffer.
	if(bufp->b_mip != NULL && bufp->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text226,text263,text238,bufp->b_bname);
			// "Cannot %s %s buffer '%s'","delete","executing"

	// We cannot nuke a aliased buffer.
	if(bufp->b_nalias > 0)
		return rcset(Failure,0,text272,bufp->b_nalias);
			// "Macro has %hu alias(es)"

	// We cannot nuke a macro bound to a hook.
	if((bufp->b_flags & BFMacro) && ishook(bufp,true))
		return rc.status;

	// It's a go.  Blow text away (unless user got cold feet).
	if(bclear(bufp,flags | BC_ClrFilename | BC_Unnarrow) != Success)
		return rc.status;

	// Delete exectab entry.
	if((bufp->b_flags & BFMacro) && execfind(bufp->b_bname + 1,OpDelete,0,NULL) != Success)
		return rc.status;

	mdelete(bufp,MkOpt_Wind);			// Delete all marks.
	univ.u.p_bufp = bufp;				// Get key binding, if any.
	kbp = getpentry(&univ);

	if(si.savbufp == bufp)				// Unsave buffer if saved.
		si.savbufp = NULL;
	if(si.cursp->s_lastbufp == bufp)		// Clear "last buffer".
		si.cursp->s_lastbufp = NULL;
	ppfree(bufp);					// Release any macro preprocessor storage.
	if(bufp->b_mip != NULL)
		free((void *) bufp->b_mip);		// Release buffer extension record.
	ddelete(delistbuf(bufp));			// Remove from buffer list and destroy Buffer and Datum objects.
	if(kbp != NULL)
		unbindent(kbp);				// Delete buffer key binding.

	return rc.status;
	}

// Delete buffer(s) and return status.  Operation is controlled by numeric argument:
//	default		Delete named buffer(s), no force.
//	n < 0		Force-delete named buffer(s).
//	n == 0		Delete all unchanged, visible buffers, skipping ones being displayed.
//	n == 1		Delete all visible buffers, no force, skipping ones being displayed.
//	n > 1		Force-delete all visible buffers, with initial confirmation, skipping ones being displayed.
// If a force, changed and/or narrowed buffers are deleted without user confirmation.
int deleteBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	uint count = 0;

	dsetint(0L,rp);

	// Check if processing all buffers.
	if(n >= 0) {
		// Get confirmation if interactive and force-deleting all other buffers.
		if(!(si.opflags & OpScript) && n > 1) {
			bool yep;

			if(terminpYN(text168,&yep) != Success)
					// "Delete all other visible buffers"
				return rc.status;
			if(!yep)
				return mlerase();
			}

		// Loop through buffer list.
		Datum **blp = buftab.a_elpp;
		Datum **blpz = blp + buftab.a_used;
		do {
			bufp = bufptr(*blp);

			// Skip if (1), being displayed; (2), hidden; or (3), modified and n == 0.
			if(bufp->b_nwind > 0 || (bufp->b_flags & BFHidden) || ((bufp->b_flags & BFChanged) && n == 0))
				++blp;
			else {
				// Nuke it (but don't increment blp because array element it points to will be removed).
				if(mlprintf(MLHome | MLWrap | MLFlush,text367,bufp->b_bname) != Success)
						// "Deleting buffer %s..."
					return rc.status;
				cpause(50);
				if(bdelete(bufp,n > 1 ? BC_IgnChgd : n == 1 ? BC_Multi : 0) < Cancelled)
					break;
				if(rc.status == Success)
					++count;
				else
					rcclear(0);
				--blpz;
				}
			} while(blp < blpz);

		if(rc.status == Success) {
			dsetint((long) count,rp);
			(void) rcset(Success,RCHigh,text368,count,count != 1 ? "s" : "");
						// "%u buffer%s deleted"
			}
		return rc.status;
		}

	// Not all... process named buffer(s).
	if(n == INT_MIN)
		n = 0;		// No force.

	// If interactive, get buffer name from user.
	if(!(si.opflags & OpScript)) {
		bufp = bdefault(BDefTwo);
		if(bcomplete(rp,text26,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success || bufp == NULL)
				// "Delete"
			return rc.status;
		goto NukeIt;
		}

	// Script mode: get buffer name(s) to delete.
	Datum *bnamep;
	uint aflags = ArgFirst | ArgNotNull1;
	if(dnewtrk(&bnamep) != 0)
		return librcset(Failure);
	do {
		if(!(aflags & ArgFirst) && !havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(bnamep,aflags) != Success)
			return rc.status;
		aflags = ArgNotNull1;
		if((bufp = bsrch(bnamep->d_str,NULL)) == NULL)
			(void) rcset(Success,RCNoWrap,text118,bnamep->d_str);
				// "No such buffer '%s'"
		else {
NukeIt:
			// Nuke it.
			if(bdelete(bufp,n < 0 ? BC_IgnChgd : 0) != Success) {

				// Don't cause a running script to fail.
				if(rc.status < Failure)
					return rc.status;
				(void) rcset(Success,RCUnFail | RCKeepMsg | RCForce | RCNoWrap,NULL);
				}
			else
				++count;
			}
		} while(si.opflags & OpScript);

	// Return count if no errors.
	if(disnull(&rc.msg)) {
		dsetint((long) count,rp);
		if(!(si.opflags & OpScript))
			(void) rcset(Success,0,"%s %s",text58,text10);
						// "Buffer","deleted"
		}

	return rc.status;
	}

// Rename a buffer and set rp (if not NULL) to the new name.  If n > 0, derive the name from the filename attached to the buffer
// and use it, without prompting; otherwise, get new buffer name, using ArgFirst flag in script mode if n is not the default.
// Return status.
int brename(Datum *rp,int n,Buffer *targbufp) {
	Buffer *bufp;
	Array *aryp;
	ssize_t index;
	char *prmt;
	Datum *datp;
	Datum *bnamep;			// New buffer name.
	UnivPtr univ;
	TermInp ti = {NULL,RtnKey,MaxBufName,NULL};
	char askstr[NWork];

	// We cannot rename an executing buffer.
	if(targbufp->b_mip != NULL && targbufp->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text284,text275,text248);
			// "Cannot %s %s buffer","rename","an executing"

	if(dnewtrk(&bnamep) != 0)
		goto LibFail;

	// Auto-rename if n > 0 (which indicates evaluation mode as well).  Do nothing if buffer name is
	// already the target name.
	if(n > 0) {
		if(targbufp->b_fname == NULL)
			return rcset(Failure,0,text145,targbufp->b_bname);
					// "No filename associated with buffer '%s'"
		if(dsalloc(bnamep,MaxBufName + 1) != 0)
			goto LibFail;
		if(strcmp(fbname(bnamep->d_str,targbufp->b_fname),targbufp->b_bname) == 0)
			return rc.status;
		bunique(bnamep->d_str,NULL);
		goto SetNew;
		}

	// Set initial prompt.
	prmt = (n == INT_MIN) ? text385 : text339;
			// "Change buffer name to","to";

Ask:
	// Get the new buffer name.
	if(si.opflags & OpScript) {
		if(funcarg(bnamep,n == INT_MIN ? ArgFirst | ArgNotNull1 : ArgNotNull1) != Success)
			return rc.status;
		}
	else if(terminp(bnamep,prmt,ArgNotNull1 | ArgNil1,0,&ti) != Success || bnamep->d_type == dat_nil)
		return rc.status;

	// Valid buffer name?
	if(!isbname(bnamep->d_str)) {
		if(si.opflags & OpScript)
			return rcset(Failure,0,text128,bnamep->d_str);
				// "Invalid buffer name '%s'"
		sprintf(prmt = askstr,text128,bnamep->d_str);
				// "Invalid buffer name '%s'"
		strcat(prmt,text324);
				// ".  New name"
		goto Ask;	// Try again.
		}

	// Check for duplicate name.
	aryp = &buftab;
	while((datp = aeach(&aryp)) != NULL) {
		bufp = bufptr(datp);
		if(bufp != targbufp) {

			// Other buffer with this name?
			if(strcmp(bnamep->d_str,bufp->b_bname) == 0) {
				if(si.opflags & OpScript)
					return rcset(Failure,0,text181,text58,bnamep->d_str);
						// "%s name '%s' already in use","Buffer"
				prmt = text25;
					// "That name is already in use.  New name"
				goto Ask;		// Try again.
				}

			// Macro buffer name?
			if((*targbufp->b_bname == SBMacro) != (*bnamep->d_str == SBMacro)) {
				if(si.opflags & OpScript)
					return rcset(Failure,0,*bnamep->d_str == SBMacro ? text268 : text270,text275,
					 bnamep->d_str,SBMacro);
						// "Cannot %s buffer: name '%s' cannot begin with %c","rename"
						// "Cannot %s macro buffer: name '%s' must begin with %c","rename"
				sprintf(prmt = askstr,"%s'%c'%s",text273,SBMacro,text324);
					// "Macro buffer names (only) begin with ",".  New name"
				goto Ask;		// Try again.
				}
			}
		}

	// Valid macro name?
	if(*bnamep->d_str == SBMacro) {
		char *str = bnamep->d_str + 1;
		enum e_sym sym = getident(&str,NULL);
		if((sym != s_ident && sym != s_identq) || *str != '\0') {
			if(si.opflags & OpScript)
				return rcset(Failure,0,text286,bnamep->d_str + 1);
					// "Invalid identifier '%s'"
			sprintf(prmt = askstr,text286,bnamep->d_str + 1);
					// "Invalid identifier '%s'"
			strcat(prmt,text324);
					// ".  New name"
			goto Ask;	// Try again.
			}
		}
SetNew:
	// New name is unique and valid.  Rename the buffer.
	if((targbufp->b_flags & BFMacro) &&
	 execfind(targbufp->b_bname + 1,OpDelete,0,NULL) != Success)	// Remove old macro name from exectab hash...
		return rc.status;
	datp = delistbuf(targbufp);				// remove from buffer list...
	strcpy(targbufp->b_bname,bnamep->d_str);		// copy new buffer name to structure...
	(void) bsrch(targbufp->b_bname,&index);			// get index of new name...
	if(enlistbuf(datp,index) != Success)			// put it back in the right place...
		return rc.status;
	if((targbufp->b_flags & BFMacro) && execfind((univ.u.p_bufp = targbufp)->b_bname + 1,
	 OpCreate,univ.p_type = (targbufp->b_flags & BFConstrain) ? PtrMacro_C : PtrMacro_O,
	 &univ) != Success)					// add new macro name to exectab hash...
		return rc.status;
	supd_wflags(targbufp,WFMode);				// and make all affected mode lines replot.
	if(!(si.opflags & OpScript))
		(void) mlerase();
	if(rp != NULL)
		datxfer(rp,bnamep);

	return rc.status;
LibFail:
	return librcset(Failure);
	}

// Rename current buffer or a named buffer.  If n > 0, derive its name from the attached filename and use it (without
// prompting).  Return status.
int renameBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;

	// Get buffer.
	if(n == INT_MIN)
		bufp = si.curbp;
	else if(bcomplete(rp,text29,n > 0 ? si.curbp->b_bname : (bufp = bdefault(BDefTwo)) != NULL ? bufp->b_bname : NULL,
	 OpDelete,&bufp,NULL) != Success || bufp == NULL)
			// "Rename"
		return rc.status;

	return brename(rp,n,bufp);
	}

// Get size of a buffer in lines and bytes.  Set *lp to line count if not NULL and return byte count.
long buflength(Buffer *bufp,long *lp) {
	long nlines,byteCt;
	Line *lnp = bufp->b_lnp;

	nlines = byteCt = 0;
	while(lnp->l_nextp != NULL) {			// Count all lines except the last...
		++nlines;
		byteCt += lnp->l_used + 1;
		lnp = lnp->l_nextp;
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
int bappend(Buffer *bufp,char *text) {
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
			llink(lnp,bufp,bufp->b_lnp->l_prevp);

			// Check and adjust source pointers.
			if(*str1 == '\0')
				break;
			str0 = str1 + 1;
			}
		++str1;
		}

	return rc.status;
	}

// Read the nth next line from a buffer and store in rp.  Return status.
int bgets(Datum *rp,int n,Datum **argpp) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"

	return bufop(rp,n,NULL,BOpReadBuf,0);
	}

// Build and pop up a buffer containing a list of all visible buffers.  List hidden buffers as well if n argument, but exclude
// macros if n is -1.  Render buffer and return status.
int showBuffers(Datum *rp,int n,Datum **argpp) {
	Array *aryp;
	Datum *datp;
	char *str;
	Buffer *listp,*bufp;
	DStrFab rpt;
	int sizecol = 8;
	int filecol = 40;
	bool skipLine;
	char wkbuf[filecol + 16];
	struct bfinfo {				// Array of buffer flags.
		ushort bflag,sbchar;
		} *sbp;
	static struct bfinfo sb[] = {
		{BFActive,SBActive},		// Activated buffer -- file read in.
		{BFChanged,SBChanged},		// Changed buffer.
		{BFHidden,SBHidden},		// Hidden buffer.
		{BFMacro,SBMacro},		// Macro buffer.
		{BFConstrain,SBConstrain},	// Constrained macro.
		{BFPreproc,SBPreproc},		// Preprocessed buffer.
		{BFNarrowed,SBNarrowed},	// Narrowed buffer.
		{BFTermAttr,SBTermAttr},	// Terminal-attributes buffer.
		{0,0}};

	// Get new buffer and string-fab object.
	if(sysbuf(text159,&listp,BFTermAttr) != Success)
			// "Buffers"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;

	// Write headers.
	if(dputs(text30,&rpt) != 0 ||
	      // "~bACHMCPNT    Size       Buffer               File~0"
	 dputs("\n-------- --------- -------------------- -------------------------------",&rpt) != 0)
		goto LibFail;

	// Output the list of buffers.
	skipLine = false;
	aryp = &buftab;
	while((datp = aeach(&aryp)) != NULL) {
		bufp = bufptr(datp);

		// Skip hidden buffers if default n or just macros if n == -1.
		if(((bufp->b_flags & BFHidden) && n == INT_MIN) || (n == -1 && (bufp->b_flags & BFMacro)))
			continue;

		if(dputc('\n',&rpt) != 0)
			goto LibFail;
		if(skipLine) {
			if(dputc('\n',&rpt) != 0)
				goto LibFail;
			skipLine = false;
			}
		str = wkbuf;					// Start at left edge.

		// Output the buffer flag indicators.
		sbp = sb;
		do {
			*str++ = (bufp->b_flags & sbp->bflag) ? sbp->sbchar : ' ';
			} while((++sbp)->bflag != 0);

#if (MMDebug & Debug_BufWindCt) && 0
		sprintf(str,"%3hu",bufp->b_nwind);
		str += 3;
#else
		do {
			*str++ = ' ';
			} while(str < wkbuf + sizecol);
#endif
#if 1
		sprintf(str,"%9lu",buflength(bufp,NULL));	// 9-digit (minimum) buffer size.
#else
		sprintf(str,"%9lu",buflength(bufp,NULL) + 1023400000ul);	// Test it.
#endif
		str = strchr(str,'\0');
		*str++ = ' ';					// Gap.
#if 0
		if(strlen(bufp->b_bname) > 20) {		// Will buffer name fit in column?
			str = stplcpy(str,bufp->b_bname,20);	// No, truncate it and add '$'.
			str = stpcpy(str,"$");
			}
		else
#endif
			str = stpcpy(str,bufp->b_bname);	// Copy buffer name.

		if(bufp->b_fname != NULL)			// Pad if filename exists.
			do {
				*str++ = ' ';
				} while(str < wkbuf + filecol);
		*str = '\0';					// Save buffer and add filename.
		if(dputs(wkbuf,&rpt) != 0 ||
		 (bufp->b_fname != NULL && dputs(bufp->b_fname,&rpt) != 0))
			goto LibFail;
		}

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		goto LibFail;
	if(bappend(listp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n,listp,RendNewBuf | RendReset);
LibFail:
	return librcset(Failure);
	}
