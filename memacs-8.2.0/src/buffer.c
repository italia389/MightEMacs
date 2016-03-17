// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// buffer.c	Buffer management routines for MightEMacs.
//
// Some of the functions are internal, and some are attached to user keys.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"

// Clear a buffer's filename, if any.
void clfname(Buffer *bufp) {

	if(bufp->b_fname != NULL) {
		(void) free((void *) bufp->b_fname);
		bufp->b_fname = NULL;
		}
	}

// Set buffer filename.  Return status.
int setfname(Buffer *bufp,char *fnamep) {

	clfname(bufp);

	// Ignore null filename.
	if(fnamep != NULL && *fnamep != '\0') {
		if((bufp->b_fname = (char *) malloc(strlen(fnamep) + 1)) == NULL)
			return rcset(PANIC,0,text94,"setfname");
				// "%s(): Out of memory!"
		strcpy(bufp->b_fname,fnamep);
		}

	if(!(opflags & OPSCRIPT))
		(void) mlerase(0);

	return rc.status;
	}

// Get the default buffer (a guess) for various buffer commands.  Search backward if "backward" is true; otherwise, forward.
// Consider active buffers only if "active" is true.  Return a pointer to the first visible buffer found, or NULL if none exist.
Buffer *bdefault(bool backward,bool active) {
	Buffer *bufp;

	// Find the first buffer before or after the current one that is visible.
	bufp = backward ? curbp->b_prevp : curbp->b_nextp;
	for(;;) {
		if(bufp == NULL)
			bufp = backward ? btailp : bheadp;
		else if(bufp == curbp)
			return NULL;
		else if(!(bufp->b_flags & BFHIDDEN) && (!active || (bufp->b_flags & BFACTIVE)))
			break;
		else
			bufp = backward ? bufp->b_prevp : bufp->b_nextp;
		}

	return bufp;
	}

#if MMDEBUG & DEBUG_NARROW
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
	fflush(logfile);
	}
#endif

// Inactivate all user marks that are outside the current narrowed buffer by negating their dot offsets.
static void mrkoff(void) {
	Mark *mkp;
	Line *lnp;

	// First, inactivate all user marks, except any pointing to the buffer header line (EOB), which is always valid.
	mkp = &curbp->b_mroot;
	do {
		if(mkp->mk_id <= '~' && mkp->mk_dot.lnp != curbp->b_hdrlnp)
			mkp->mk_dot.off = -(mkp->mk_dot.off + 1);
		} while((mkp = mkp->mk_nextp) != NULL);

	// Now scan the buffer and reactivate the marks that are still in the narrowed region.
	for(lnp = lforw(curbp->b_hdrlnp); lnp != curbp->b_hdrlnp; lnp = lforw(lnp)) {

		// Any mark match this line?
		mkp = &curbp->b_mroot;
		do {
			if(mkp->mk_dot.lnp == lnp && mkp->mk_dot.off < 0)
				mkp->mk_dot.off = -mkp->mk_dot.off - 1;
			} while((mkp = mkp->mk_nextp) != NULL);
		}
	}

// Narrow to lines or region.  Makes all but the specified line(s) in the current buffer hidden and unchangable.  Set rp to
// buffer name and return status.
int narrowBuf(Value *rp,int n) {
	EScreen *scrp;
	EWindow *winp;		// Windows to fix up pointers in as well.
	Mark *mkp;
	Line *lnp;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Make sure we aren't already narrowed.
	if(curbp->b_flags & BFNARROW)
		return rcset(FAILURE,0,text71,text58,curbp->b_bname);
			// "%s '%s' is already narrowed","Buffer"

	// Save faces of all windows displaying current buffer in a mark so they can be restored when buffer is widened.
	// In all screens...
	scrp = sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp->w_bufp == curbp) {

				// Found window attached to current buffer.  Save its face using the window's mark id.
				if(mfind(winp->w_id,&mkp,MKOPT_CREATE) != SUCCESS)
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

		// Move back n lines max.
		n = 1;
		while(lback(dotp->lnp) != curbp->b_hdrlnp) {
			dotp->lnp = lback(dotp->lnp);
			++n;
			if(++i == 0)
				break;
			}
		}
	else if(n == 0 && reglines(&n,NULL) != SUCCESS)
		return rc.status;

	// Current line is now at top of area to be narrowed (possibly end-of-buffer) and n is the number of lines.
	lnp = dotp->lnp;

	// Archive the top fragment.
	if(lnp == curbp->b_hdrlnp->l_nextp)
		curbp->b_ntoplnp = NULL;
	else {
		curbp->b_ntoplnp = curbp->b_hdrlnp->l_nextp;			// Save old first line of buffer.
		(curbp->b_hdrlnp->l_nextp = lnp)->l_prevp->l_nextp = NULL;	// Set new first line and terminate fragment.
		lnp->l_prevp = curbp->b_hdrlnp;				// Terminate new first line backward.
		}

	// Move forward to the end.
	do {
		if((dotp->lnp = lforw(dotp->lnp)) == curbp->b_hdrlnp)
			break;
		} while(--n > 0);

	// Archive the bottom fragment.
	if(dotp->lnp == curbp->b_hdrlnp)
		curbp->b_nbotlnp = NULL;
	else {
		(curbp->b_nbotlnp = dotp->lnp)->l_prevp->l_nextp = curbp->b_hdrlnp;
								// Save first line of fragment and terminate line above it.
		curbp->b_hdrlnp->l_prevp->l_nextp = NULL;	// Terminate last line of fragment (last line of buffer).
		curbp->b_hdrlnp->l_prevp = curbp->b_nbotlnp->l_prevp;	// Set new last line of buffer.
		}
#if MMDEBUG & DEBUG_NARROW
	ldump("curbp->b_ntoplnp",curbp->b_ntoplnp);
	ldump("curbp->b_nbotlnp",curbp->b_nbotlnp);
	ldump("curbp->b_hdrlnp",curbp->b_hdrlnp);
	ldump("lnp",lnp);
#endif
	// Inactivate marks outside of narrowed region.
	mrkoff();

	// In all screens...
	scrp = sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp->w_bufp == curbp) {

				// Found window attached to narrowed buffer.  Update its buffer settings.
				winp->w_face.wf_toplnp = winp->w_face.wf_dot.lnp = lnp;
				winp->w_face.wf_dot.off = winp->w_face.wf_fcol = 0;
				winp->w_flags |= (WFHARD | WFMODE);
				}
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);
	opflags |= OPSCREDRAW;

	// and now remember we are narrowed.
	curbp->b_flags |= (BFNARROW | BFUNKFACE);
	return vsetstr(curbp->b_bname,rp) != 0 ? vrcset() : rcset(SUCCESS,0,text73,text58);
								// "%s narrowed","Buffer"
	}

// Restore a buffer to pre-narrowed state.
static void unnarrow(Buffer *bufp) {
	EScreen *scrp;
	EWindow *winp;
	Line *lnp;
	Mark *mkp;

	// Recover the top fragment.
	if(bufp->b_ntoplnp != NULL) {
		lnp = bufp->b_ntoplnp;
		while(lnp->l_nextp != NULL)
			lnp = lnp->l_nextp;
		lnp->l_nextp = bufp->b_hdrlnp->l_nextp;
		lnp->l_nextp->l_prevp = lnp;
		bufp->b_hdrlnp->l_nextp = bufp->b_ntoplnp;
		bufp->b_ntoplnp->l_prevp = bufp->b_hdrlnp;
		bufp->b_ntoplnp = NULL;
		}

	// Recover the bottom fragment.
	if(bufp->b_nbotlnp != NULL) {
		if(bufp == curbp) {
			Dot *dotp = &curwp->w_face.wf_dot;

			// If the point is at EOF, move it to the beginning of the bottom fragment.
			if(dotp->lnp == bufp->b_hdrlnp) {
				dotp->lnp = bufp->b_nbotlnp;
				dotp->off = 0;
				}
			}
		else {
			// If the point is at EOF, move it to the beginning of the bottom fragment.
			if(bufp->b_face.wf_dot.lnp == bufp->b_hdrlnp) {
				bufp->b_face.wf_dot.lnp = bufp->b_nbotlnp;
				bufp->b_face.wf_dot.off = 0;
				}
			}

		// If any marks are at EOF, move them to the beginning of the bottom fragment.
		mkp = &bufp->b_mroot;
		do {
			if(mkp->mk_dot.lnp == bufp->b_hdrlnp) {
				mkp->mk_dot.lnp = bufp->b_nbotlnp;
				mkp->mk_dot.off = mkp->mk_force = 0;
				}
			} while((mkp = mkp->mk_nextp) != NULL);

		// Connect the bottom fragment.
		lnp = bufp->b_nbotlnp;
		while(lnp->l_nextp != NULL)
			lnp = lnp->l_nextp;
		lnp->l_nextp = bufp->b_hdrlnp;
		bufp->b_hdrlnp->l_prevp->l_nextp = bufp->b_nbotlnp;
		bufp->b_nbotlnp->l_prevp = bufp->b_hdrlnp->l_prevp;
		bufp->b_hdrlnp->l_prevp = lnp;
		bufp->b_nbotlnp = NULL;
		}

	// Activate all marks in buffer.
	mkp = &bufp->b_mroot;
	do {
		if(mkp->mk_dot.off < 0)
			mkp->mk_dot.off = -mkp->mk_dot.off - 1;
		} while((mkp = mkp->mk_nextp) != NULL);

	// Restore faces of all windows displaying current buffer from the window's mark if it exists.
	// In all screens...
	scrp = sheadp;
	do {
		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp != curwp && winp->w_bufp == curbp) {

				// Found window attached to widened buffer.  Restore its face.
				(void) mfind(winp->w_id,&mkp,MKOPT_WIND);	// Can't return an error.
				if(mkp != NULL) {
					winp->w_face.wf_dot = mkp->mk_dot;
					winp->w_force = mkp->mk_force;
					winp->w_flags |= WFFORCE;
					}
				}
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In front screen only...
	winp = cursp->s_wheadp;
	do {
		if(winp->w_bufp == bufp)
			winp->w_flags |= (WFHARD | WFMODE);
		} while((winp = winp->w_nextp) != NULL);

	// and now forget that we are narrowed.
	bufp->b_flags &= ~(BFNARROW | BFUNKFACE);
	}

// Widen (restore) a narrowed buffer.  Set rp to buffer name and return status.
int widenBuf(Value *rp,int n) {

	// Make sure we are narrowed.
	if(!(curbp->b_flags & BFNARROW))
		return rcset(FAILURE,0,text74,text58,curbp->b_bname);
			// "%s '%s' is not narrowed","Buffer"

	// Restore current buffer to pre-narrowed state.
	unnarrow(curbp);
	if(vsetstr(curbp->b_bname,rp) != 0)
		return vrcset();
	(void) rcset(SUCCESS,0,text75,text58);
		// "%s widened","Buffer"
	return feval(rp,INT_MIN,cftab + cf_redrawScreen);
	}

// Search the buffer list for given name and return pointer to slot if found; otherwise, NULL.  In either case, set *bufpp to
// prior slot if bufpp not NULL;
Buffer *bsrch(char *bnamep,Buffer **bufpp) {
	Buffer *bufp1,*bufp2;
	int result;

	// Scan the buffer list.
	bufp1 = NULL;
	bufp2 = bheadp;
	while(bufp2 != NULL) {
		if((result = strcmp(bufp2->b_bname,bnamep)) == 0)
			break;
		if(result > 0) {
			bufp2 = NULL;
			break;
			}
		bufp1 = bufp2;
		bufp2 = bufp2->b_nextp;
		}

	if(bufpp != NULL)
		*bufpp = bufp1;
	return bufp2;
	}

// Generate a valid buffer name from a pathname.  bnamep is assumed to point to a buffer that is NBNAME + 1 or greater in size.
static char *fbname(char *bnamep,char *fnamep) {
	char *strp;

	// Get file basename and validate it.  Keep filename extension if it's numeric.
	stplcpy(bnamep,fbasename(fnamep,(strp = strrchr(fnamep,'.')) != NULL && asc_long(strp + 1,NULL,true) ? true : false),
	 NBNAME + 1);
	if(*bnamep == ' ' || *bnamep == SBMACRO)	// Convert any leading space or macro character...
		*bnamep = ALTBUFCH;
	stripstr(bnamep,1);				// remove any trailing white space...
	strp = bnamep;					// convert any non-printable characters...
	do {
		if(*strp < ' ' || *strp > '~')
			*strp = ALTBUFCH;
		} while(*++strp != '\0');
	return bnamep;					// and return result.
	}

// Generate a unique buffer name (from fnamep if not NULL) by appending digit(s) if needed and return it.  The string buffer
// that bnamep points to is assumed to be of size NBNAME + 1 or greater.
static char *bunique(char *bnamep,char *fnamep) {
	int n,i;
	char *strp0,*strp1,*strpz = bnamep + NBNAME;
	char wkbuf[sizeof(int) * 3];

	// Begin with file basename.
	if(fnamep != NULL)
		fbname(bnamep,fnamep);

	// Check if name is already in use.
	while(bsrch(bnamep,NULL) != NULL) {

		// Name already exists.  Strip off trailing digits, if any.
		strp1 = strchr(bnamep,'\0');
		while(strp1 > bnamep && strp1[-1] >= '0' && strp1[-1] <= '9')
			--strp1;
		strp0 = strp1;
		n = i = 0;
		while(*strp1 != '\0') {
			n = n * 10 + (*strp1++ - '0');
			i = 1;
			}

		// Add 1 to numeric suffix (or begin at 0), and put it back.  Assume that NBNAME is >= number of bytes needed to
		// encode an int in decimal form (10 for 32-bit int) and that we will never have more than MAX_INT buffers
		// (> 2 billion).
		long_asc((long) (n + i),wkbuf);
		n = strlen(wkbuf);
		strcpy(strp0 + n > strpz ? strpz - n : strp0,wkbuf);
		}

	return bnamep;
	}

// Remove buffer from buffer list.
static void delistbuf(Buffer *bufp) {

	if(bufp == bheadp) {
		if((bheadp = bufp->b_nextp) != NULL)	// More than one buffer?
			bheadp->b_prevp = NULL;
		}
	else if(bufp == btailp) {
		btailp = bufp->b_prevp;
		btailp->b_nextp = NULL;
		}
	else {
		bufp->b_prevp->b_nextp = bufp->b_nextp;
		bufp->b_nextp->b_prevp = bufp->b_prevp;
		}
	}

// Add buffer to buffer list, given pointers to preceding buffer and one to add.
static void enlistbuf(Buffer *bufp1,Buffer *bufp2) {

	if(bufp1 == NULL) {

		// Insert at the beginning.
		bufp2->b_nextp = bheadp;
		bufp2->b_prevp = NULL;
		if(bheadp == NULL)
			btailp = bufp2;
		else
			bheadp->b_prevp = bufp2;
		bheadp = bufp2;
		}
	else {
		// Insert after bufp1.
		bufp2->b_nextp = bufp1->b_nextp;
		bufp2->b_prevp = bufp1;
		if(bufp1 == btailp)
			btailp = bufp1->b_nextp = bufp2;
		else
			bufp1->b_nextp = bufp1->b_nextp->b_prevp = bufp2;
		}
	}

// Reposition current buffer in buffer list using its new name.
static void relistbuf(void) {
	Buffer *bufp1,*bufp2;

	// Remove from buffer list.
	delistbuf(curbp);

	// Scan the buffer list and find the reinsertion point.
	bufp1 = NULL;
	bufp2 = bheadp;
	while(bufp2 != NULL) {
		if(strcmp(bufp2->b_bname,curbp->b_bname) > 0)
			break;
		bufp1 = bufp2;
		bufp2 = bufp2->b_nextp;
		}

	// Put it back in the right place.
	enlistbuf(bufp1,curbp);
	}

// Initialize dot position, marks, first column position, and I/O delimiters of a buffer.
static void bufinit(Buffer *bufp,Line *lnp) {

	faceinit(&bufp->b_face,lnp,bufp);
	*bufp->b_inpdelim = *bufp->b_otpdelim = '\0';
	bufp->b_inpdelimlen = 0;
	}

// Check if given buffer name is valid and return Boolean result.
static bool isbname(char *namep) {
	char *strp;

	// Null?
	if(*namep == '\0')
		return false;

	// All printable characters?
	strp = namep;
	do {
		if(*strp < ' ' || *strp > '~')
			return false;
		} while(*++strp != '\0');

	// First or last character a space?
	if(*namep == ' ' || strp[-1] == ' ')
		return false;

	// All checks passed.
	return true;
	}

/* Places where a buffer may be created via bfind().
sysbuf()
bscratch()
execbuf()
dofile()
rdfile()
getfile()
bcomplete()
edinit2()
docmdline()

* Places where bunique() may be called.
bfind()
setBufName()
*/

// Find a buffer by name and return status or boolean result.  Actions taken depends on cflags:
//	If CRBFILE is set:
//		Use the base filename of "namep" as the default buffer name; otherwise, use "namep" directly.
//	If CRBUNIQ is set:
//		Create a unique buffer name derived from the default one.  (CRBCREATE is assumed to also be set.)
//	If CRBCREATE is set:
//		If the buffer is found:
//			Set *bufpp (if bufpp not NULL) to the buffer pointer, set *createdp (if createdp not NULL) to false,
//			and return status.
//		If the buffer is not found:
//			Create a buffer, set it's flag word to bflags, set *bufpp (if bufpp not NULL) to the buffer pointer, set
//			*createdp (if createdp not NULL) to true, and return status.
//	If CRBCREATE is not set:
//		If the buffer is found:
//			Set *bufpp (if bufpp not NULL) to the buffer pointer and return true, ignoring createdp.
//		If the buffer is not found:
//			Return false, ignoring bufpp and createdp.
int bfind(char *namep,uint cflags,uint bflags,Buffer **bufpp,bool *createdp) {
	Buffer *bufp1,*bufp2;
	char *bnamep,wkbuf[NBNAME + 1];

	// Set default buffer name.
	if(cflags & CRBUNIQ)
		(void) bsrch(bnamep = (cflags & CRBFILE) ? bunique(wkbuf,namep) : bunique(strcpy(wkbuf,namep),NULL),&bufp1);
	else {
		if(cflags & CRBFILE)
			fbname(bnamep = wkbuf,namep);
		else
			bnamep = namep;

		// Search for the buffer.
		if((bufp2 = bsrch(bnamep,&bufp1)) != NULL) {

			// Found it.  Return results.
			if(bufpp != NULL)
				*bufpp = bufp2;
			if(cflags & CRBCREATE) {
				if(createdp != NULL)
					*createdp = false;
				return rc.status;
				}
			return true;
			}
		}

	// No such buffer exists, create it?
	if(cflags & CRBCREATE) {
		Line *lnp;

		// Valid buffer name?
		if(!isbname(bnamep))
			return rcset(FAILURE,0,text128,bnamep);
					// "Invalid buffer name '%s'"
		// Macro name?
		if(*bnamep == SBMACRO && !(bflags & BFMACRO))
			return rcset(FAILURE,0,text268,bnamep,SBMACRO);
					// "Non-macro buffer name '%s' cannot begin with %c"

		// Allocate the memory for the buffer and the "magic" (header) line.
		if((bufp2 = (Buffer *) malloc(sizeof(Buffer))) == NULL)
			return rcset(PANIC,0,text94,"bfind");
				// "%s(): Out of memory!"
		if(lalloc(0,&lnp) != SUCCESS)
			return rc.status;	// Fatal error.
		lnp->l_nextp = lnp->l_prevp = lnp;
		lnp->l_text[0] = '\0';

		// Insert the buffer into the list (using prior slot pointer from bsrch() call)...
		enlistbuf(bufp1,bufp2);

		// and set up the other buffer fields.
		bufp2->b_mroot.mk_nextp = NULL;
		bufinit(bufp2,bufp2->b_hdrlnp = lnp);
		bufp2->b_ntoplnp = bufp2->b_nbotlnp = NULL;
		bufp2->b_flags = bflags | BFACTIVE;
		bufp2->b_modes = modetab[MDR_DEFAULT].flags;
		bufp2->b_nwind = bufp2->b_nexec = bufp2->b_nalias = 0;
		bufp2->b_nargs = -1;
		bufp2->b_acount = gasave;
		bufp2->b_execp = NULL;
		bufp2->b_fname = NULL;
		strcpy(bufp2->b_bname,bnamep);

		// Add macro name to the CFAM list and return results.
		if(bflags & BFMACRO)
			(void) amfind(bufp2->b_bname + 1,OPCREATE,PTRMACRO);
		if(bufpp != NULL)
			*bufpp = bufp2;
		if(createdp != NULL)
			*createdp = true;
		return rc.status;
		}

	// Buffer not found and not creating.
	return false;
	}

// This routine blows away all of the text in a buffer.  If it's marked as changed, it will not be cleared unless CLBIGNCHGD is
// set in "flags" or the user gives the okay.  This to save the user the grief of losing text.  If CLBCLFNAME is set, the
// filename associated with the buffer is set to null.  If the buffer is narrowed and CLBUNNARROW is set, the buffer is silently
// unnarrowed before being cleared; otherwise, the user is prompted to give the okay before proceeding.  The window chain is
// nearly always wrong if this routine is called; the caller must arrange for the updates that are required.
//
// Set *clearedp (if clearedp not NULL) to true if buffer is erased; otherwise, false, if user changed his or her mind.  Return
// status.
int bclear(Buffer *bufp,uint flags,bool *clearedp) {
	Line *lnp;
	bool yep;
	bool bufErased = false;
	bool eraseML = false;

	// Executing buffer?
	if(bufp->b_nexec > 0)
		return rcset(FAILURE,0,text226,text264,text238,bufp->b_bname);
			// "Cannot %s %s buffer '%s'","clear","executing"

	// Visible buffer and changed?  Skip if need "narrowed buffer" confirmation as well (which preempts this).
	if(!(bufp->b_flags & BFHIDDEN) && (bufp->b_flags & BFCHGD) && !(flags & CLBIGNCHGD) &&
	 (!(bufp->b_flags & BFNARROW) || (flags & CLBUNNARROW))) {
		eraseML = true;

		// Get user confirmation.
		if(mlyesno(text32,&yep) != SUCCESS)
				// "Discard changes"
			return rc.status;
		if(!yep)
			goto clearML;
		}

	// Narrowed buffer?
	if(bufp->b_flags & BFNARROW) {

		// Force?
		if(flags & CLBUNNARROW)

			// Yes, restore buffer to pre-narrowed state.
			unnarrow(bufp);
		else if(!(flags & CLBIGNCHGD)) {

			// Not a force.  Get user confirmation (and leave narrowed).
			eraseML = true;
			if(mlyesno(text95,&yep) != SUCCESS)
					// "Narrowed buffer ... delete current portion"
				return rc.status;
			if(!yep)
				goto clearML;
			}
		}

	// It's a go ... erase it.
	if(flags & CLBCLFNAME)
		clfname(bufp);					// Zap any filename.
	while((lnp = lforw(bufp->b_hdrlnp)) != bufp->b_hdrlnp)	// Free all Line storage.
		lfree(lnp);
	lchange(bufp,WFHARD);					// Update window flags.
	bufp->b_flags &= ~(BFCHGD | BFTRUNC);			// Mark as not changed or truncated...
	if(bufp->b_flags & BFNARROW)
		bufp->b_flags = (bufp->b_flags | BFCHGD) & ~BFUNKFACE;	// but mark as changed and clear face flag if narrowed.
	bufinit(bufp,bufp->b_hdrlnp);				// Fix dot and remove marks.
	bufErased = true;

	if(eraseML)
clearML:
		(void) mlerase(0);
	if(clearedp != NULL)
		*clearedp = bufErased;
	return rc.status;
	}

// Get number of visible buffers.  (Mainly for macro use.)
int bufcount(void) {
	Buffer *bufp;
	int count;

	count = 0;
	bufp = bheadp;
	do {
		if(!(bufp->b_flags & BFHIDDEN))
			++count;
		} while((bufp = bufp->b_nextp) != NULL);
	return count;
	}

// Find a window displaying given buffer and return it, giving preference to current screen and current window.
static EWindow *findwind(Buffer *bufp) {
	EScreen *scrp;
	EWindow *winp;

	// If current window is displaying the buffer, use it.
	if(curwp->w_bufp == bufp)
		return curwp;

	// In current screen...
	winp = cursp->s_wheadp;
	do {
		if(winp->w_bufp == bufp)
			goto retn;
		} while((winp = winp->w_nextp) != NULL);

	// In all other screens...
	scrp = sheadp;
	do {
		if(scrp == cursp)			// Skip current screen.
			continue;
		if(scrp->s_curwp->w_bufp == bufp)	// Use current window first.
			return scrp->s_curwp;

		// In all windows...
		winp = scrp->s_wheadp;
		do {
			if(winp->w_bufp == bufp)
				goto retn;
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);
retn:
	return winp;
	}

// Get a buffer name (if n not default) and perform operation on buffer according to "op" argument.  If prmtp == NULL, set rp
// to function return value; otherwise, buffer name.  Return status.
int bufop(Value *rp,int n,char *prmtp,uint op,uint flag) {
	Buffer *bufp = NULL;

	// Get the buffer name.  n will never be the default for a readBuf() call.
	if(n == INT_MIN)
		bufp = curbp;
	else {
		if(prmtp != NULL)
			bufp = bdefault(false,false);
		if(bcomplete(rp,prmtp,bufp != NULL ? bufp->b_bname : NULL,OPDELETE,&bufp,NULL) != SUCCESS || bufp == NULL)
			return rc.status;
		}

	// Perform requested operation.
	switch(op) {
		case BOPSETFLAG:
			bufp->b_flags |= flag;
			goto notice;
		case BOPCLRFLAG:
			bufp->b_flags &= ~flag;
notice:
			return (flag == BFHIDDEN) ? rcset(SUCCESS,0,op == BOPSETFLAG ? text113 : text344,text58) : rc.status;
									// "%s hidden","%s unhidden","Buffer"
		case BOPGOTOLN:
			if(flag == 0) {
				op = BOPBEGEND;
				flag = true;
				}
			// Fall through.
		default:	// BOPBEGEND, BOPGOTOLN, or BOPREADBUF
			// Move dot in buffer ... usually a massive adjustment.  Set "hard motion" flag.
			{Dot *dotp;
			EWindow *winp = NULL;

			// Grab dot.  If the buffer is displayed, get the window vars instead of the buffer vars.
			if(bufp->b_nwind > 0) {
				winp = findwind(bufp);
				winp->w_flags |= WFMOVE;
				dotp = &winp->w_face.wf_dot;
				}
			else
				dotp = &bufp->b_face.wf_dot;

			// Perform operation.
			switch(op) {
				case BOPBEGEND:
					// Go to beginning or end of buffer.
					dotp->off = 0;
					dotp->lnp = flag ? bufp->b_hdrlnp : lforw(bufp->b_hdrlnp);
					break;
				case BOPGOTOLN:
					// Go to beginning of buffer and count lines.
					dotp->lnp = lforw(bufp->b_hdrlnp);
					dotp->off = 0;
					if(bufp == curbp)
						return forwln(flag - 1);
					for(--flag; flag > 0; --flag) {
						if(dotp->lnp == bufp->b_hdrlnp)
							break;
						dotp->lnp = lforw(dotp->lnp);
						}
					return rc.status;
				default:	// BOPREADBUF
					// Read next buffer line n times.
					while(n-- > 0) {
						size_t len;

						// If we are at the end, return nil.
						if(dotp->lnp == bufp->b_hdrlnp)
							return vnilmm(rp);

						// Get the length of the current line (from dot to end)...
						len = lused(dotp->lnp) - dotp->off;

						// return the spoils...
						if(vsetfstr(ltext(dotp->lnp) + dotp->off,len,rp) != 0)
							return vrcset();

						// and step the buffer's line pointer ahead one line.
						dotp->lnp = lforw(dotp->lnp);
						dotp->off = 0;
						}
					return rc.status;
				}
			}
		}
	if(flag == BFCHGD)
		upmode(bufp);

	return rc.status;
	}

// Set name of a system (internal) buffer and call bfind();
int sysbuf(char *rootp,Buffer **bufpp) {
	char bname[strlen(rootp) + 2];

	bname[0] = BSYSLEAD;
	strcpy(bname + 1,rootp);
	return bfind(bname,CRBCREATE | CRBUNIQ,BFHIDDEN,bufpp,NULL);
	}

// Activate a buffer if needed.  Return status.
int bactivate(Buffer *bufp) {

	// Check if buffer is active.
	if(!(bufp->b_flags & BFACTIVE)) {

		// Not active: read it in.
		(void) readin(bufp,NULL,true);
		}

	return rc.status;
	}

// Insert a buffer into the current buffer and set current region to inserted lines.  If n == 0, leave point before the inserted
// lines; otherwise, after.  Return status.
int insertBuf(Value *rp,int n) {
	Buffer *bufp;
	Line *buflnp,*lnp0,*lnp1,*lnp2;
	uint nline;
	int nbytes;
	WindFace *wfp = &curwp->w_face;

	// Get the buffer name.  Reject if current buffer.
	bufp = bdefault(false,false);
	if(bcomplete(rp,text55,bufp != NULL ? bufp->b_bname : NULL,OPDELETE,&bufp,NULL) != SUCCESS || bufp == NULL)
			// "Insert"
		return rc.status;
	if(bufp == curbp)
		return rcset(FAILURE,0,text124);
			// "Cannot insert current buffer into itself"

	// Let user know what's up.
	if(mlputs(MLHOME | MLWRAP,text153) != SUCCESS)
			// "Inserting data..."
		return rc.status;

	// Prepare buffer to be inserted.
	if(bactivate(bufp) != SUCCESS)
		return rc.status;
	buflnp = bufp->b_hdrlnp;

	// Prepare current buffer.
	curbp->b_flags |= BFCHGD;			// Mark as changed.
	curbp->b_mroot.mk_force = getwpos(curwp);	// Keep existing framing.
	wfp->wf_dot.lnp = lback(wfp->wf_dot.lnp);	// Back up a line and save dot in mark RMARK.
	wfp->wf_dot.off = 0;
	curbp->b_mroot.mk_dot = wfp->wf_dot;

	// Insert each line from the buffer at point.
	nline = 0;
	while((buflnp = lforw(buflnp)) != bufp->b_hdrlnp) {
		if(lalloc(nbytes = lused(buflnp),&lnp1) != SUCCESS)
			return rc.status;		// Fatal error.
		lnp0 = wfp->wf_dot.lnp;			// Line before insert.
		lnp2 = lnp0->l_nextp;			// Line after insert.

		// Re-link new line between lnp0 and lnp2...
		lnp2->l_prevp = lnp1;
		lnp0->l_nextp = lnp1;
		lnp1->l_prevp = lnp0;
		lnp1->l_nextp = lnp2;

		// and advance and save the line.
		wfp->wf_dot.lnp = lnp1;
		memcpy(lnp1->l_text,buflnp->l_text,nbytes);
		++nline;
		}

	// Adjust mark RMARK to point to first inserted line (if any).
	curbp->b_mroot.mk_dot.lnp = lforw(curbp->b_mroot.mk_dot.lnp);

	// Advance dot to the next line (the end of the inserted text).
	wfp->wf_dot.lnp = lforw(wfp->wf_dot.lnp);

	// If n is zero, swap point and RMARK.
	if(n == 0)
		(void) swapmid(RMARK);

	lchange(curbp,WFHARD | WFMODE);

	// Report results.
	return rcset(SUCCESS,RCFORCE,"%s %u %s%s%s",text154,nline,text205,nline == 1 ? "" : "s",text355);
					// "Inserted","line"," and marked as region"
	}

// Attach a buffer to the current window, creating it if necessary (default).  Render buffer and return status.
int selectBuf(Value *rp,int n) {
	Buffer *bufp;
	bool created;

	// Get the buffer name.
	bufp = bdefault(false,false);
	if(bcomplete(rp,n < 0 && n != INT_MIN ? text27 : text24,bufp != NULL ? bufp->b_bname : (n == INT_MIN || n >= 0) ?
	 buffer1 : NULL,OPCREATE,&bufp,&created) != SUCCESS || bufp == NULL)
			// "Pop","Use"
		return rc.status;

	// Render the buffer.
	return render(rp,n == INT_MIN ? 1 : n >= 0 ? n : created ? -2 : -1,bufp,n != INT_MIN && n < -1 ? RENDALTML : 0);
	}

// Create a scratch buffer; that is, one with a unique name and no filename associated with it.  Set destp to name, *bufpp to
// buffer pointer, and return status.
int bscratch(char *destp,Buffer **bufpp) {
	uint maxtries = 100;
	Buffer *bufp;
	bool created;

	// Create a buffer with a unique name.
	do {
		sprintf(destp,"%s%d",SCRATCH,ernd() % 1000);
		if(bfind(destp,CRBCREATE,0,&bufp,&created) != SUCCESS)
			return rc.status;

		// Was a new buffer created?
		if(created)
			goto retn;	// Yes, use it.

		// Nope, a buffer with that name already exists.  Try again unless limit reached.
		} while(--maxtries > 0);

	// Random-number approach failed ... let bfind() "uniquify" it.
	(void) bfind(destp,CRBCREATE | CRBUNIQ,0,&bufp,NULL);
retn:
	*bufpp = bufp;
	return rc.status;
	}

// Create a scratch buffer.  Render buffer and return status.
int scratchBuf(Value *rp,int n) {
	Buffer *bufp;
	char bname[NBNAME + 1];

	// Create buffer...
	if(bscratch(bname,&bufp) != SUCCESS)
		return rc.status;

	// and render it.
	return render(rp,n == INT_MIN ? 1 : n >= 0 ? n : -2,bufp,n != INT_MIN && n < -1 ? RENDALTML : 0);
	}

// Switch to (unless n < 0) the previous or next buffer in the buffer list.  Set rp to name if successful.
// Return status.
int pnbuffer(Value *rp,int n,bool prev) {
	Buffer *bufp;

	if(n == INT_MIN)
		n = 1;

	// Cycle backward or forward thru buffers n times, or just once if n <= 0.
	do {
		if((bufp = bdefault(prev,false)) == NULL)
			return rc.status;		// Only one visible buffer.
		if(n <= 0)
			break;
		if(bswitch(bufp) != SUCCESS)
			return rc.status;
		} while(--n > 0);

	return vsetstr(bufp->b_bname,rp) != 0 ? vrcset() : rc.status;
	}

// Create tab-delimited list of visible buffer names in rp.  Return status.
int getbuflist(Value *rp) {
	Buffer *bufp;
	StrList sl;
	bool first = true;

	if(vopen(&sl,rp,false) != 0)
		return vrcset();

	// Cycle through buffers and make list.
	bufp = bheadp;
	do {
		if(!(bufp->b_flags & BFHIDDEN)) {
			if((!first && vputc('\t',&sl) != 0) || vputs(bufp->b_bname,&sl) != 0)
				return vrcset();
			first = false;
			}
		} while((bufp = bufp->b_nextp) != NULL);

	return vclose(&sl) != 0 ? vrcset() : rc.status;
	}

// Make given buffer current and return status.  The top line, dot, and column offset values from the current window are saved
// in the old buffer's header and replacement ones are fetched from the new (given) buffer's header.
int bswitch(Buffer *bufp) {
	Value *rp;

	if(vnew(&rp,false) != 0)
		return vrcset();
	if(vnilmm(rp) != SUCCESS)
		return rc.status;

	// Run exit-buffer user hook on current (old) buffer.
	if(!(curbp->b_flags & BFMACRO) && exechook(rp,INT_MIN,hooktab + HKEXITBUF,0) != SUCCESS)
		return rc.status;

	// Decrement window use count of current (old) buffer and save the current window settings.
	--curbp->b_nwind;
	wftobf(curwp,curbp);

	// Switch to new buffer.
	curwp->w_bufp = curbp = bufp;
	++curbp->b_nwind;

	// Active buffer.
	if(bactivate(curbp) <= MINEXIT)
		return rc.status;

	// Update window settings.
	bftowf(curbp,curwp);

	// Run enter-buffer user hook on current (new) buffer.
	if(rc.status == SUCCESS && !(curbp->b_flags & BFMACRO)) {
		bool ival = (rp->v_type == VALINT);
		(void) exechook(NULL,INT_MIN,hooktab + HKENTRBUF,ival ? 0x11 : 1,ival ? rp->u.v_int : rp->v_strp);
		}

	return rc.status;
	}

// Clear current buffer, or named buffer if n >= 0.  Force it if n != 0.  Set rp to false if buffer is not cleared; otherwise,
// true.  Return status.
int clearBuf(Value *rp,int n) {
	Buffer *bufp;
	bool cleared;

	// Get the buffer name to clear.
	if(n < 0)
		bufp = curbp;
	else {
		bufp = (n >= 0) ? bdefault(false,false) : NULL;
		if(bcomplete(rp,text169,bufp != NULL ? bufp->b_bname : NULL,OPDELETE,&bufp,NULL) != SUCCESS || bufp == NULL)
				// "Clear"
			return rc.status;
		}

	// Blow text away unless user got cold feet.
	if(bclear(bufp,n != 0 ? CLBIGNCHGD : 0,&cleared) == SUCCESS) {
		if(vsetstr(cleared ? val_true : val_false,rp) != 0)
			(void) vrcset();
		else if(n >= 0)
			(void) mlerase(0);		// Clear "default buffer name" prompt.
		}

	return rc.status;
	}

// Check if macro is bound to a hook and if so, set an error and return true; otherwise, return false.
static bool ishook(Buffer *bufp) {
	CFABPtr *cfabp;
	HookRec *hrp = hooktab;

	do {
		cfabp = &hrp->h_cfab;
		if(cfabp->p_type == PTRMACRO && cfabp->u.p_bufp == bufp) {
			(void) rcset(FAILURE,0,text327,hrp->h_name);
					// "Macro bound to '%s' hook"
			return true;
			}
		} while((++hrp)->h_name != NULL);

	return false;
	}

// Delete the buffer pointed to by bufp.  Don't allow if buffer is being displayed, executed, or aliased.  Pass "flags" with
// CLBUNNARROW set to bclear() to clear the buffer, then free the header line and the buffer block.  Also delete any key binding
// and remove the name from the CFAM list if it's a macro.  Return status.
int bdelete(Buffer *bufp,uint flags) {
	CFABPtr cfab;
	KeyDesc *kdp;
	bool yep;

	// We cannot nuke a displayed buffer.
	if(bufp->b_nwind > 0)
		return rcset(FAILURE,0,text28,text58);
			// "%s is being displayed","Buffer"

	// We cannot nuke an executing buffer.
	if(bufp->b_nexec > 0)
		return rcset(FAILURE,0,text226,text263,text238,bufp->b_bname);
			// "Cannot %s %s buffer '%s'","delete","executing"

	// We cannot nuke a aliased buffer.
	if(bufp->b_nalias > 0)
		return rcset(FAILURE,0,text272,bufp->b_nalias);
			// "Macro has %hu alias(es)"

	// We cannot nuke a macro bound to a hook.
	if((bufp->b_flags & BFMACRO) && ishook(bufp))
		return rc.status;

	// It's a go.  Blow text away (unless user got cold feet).
	if(bclear(bufp,flags | CLBCLFNAME | CLBUNNARROW,&yep) != SUCCESS || !yep)
		return rc.status;

	// Delete from CFAM list.
	if((bufp->b_flags & BFMACRO) && amfind(bufp->b_bname + 1,OPDELETE,0) != SUCCESS)
		return rc.status;

	mnuke(bufp,true);				// Delete all marks.
	cfab.u.p_bufp = bufp;				// Get key binding, if any.
	kdp = getpentry(&cfab);

	if(sbuffer == bufp)				// Unsave buffer if saved.
		sbuffer = NULL;
	free((void *) bufp->b_hdrlnp);			// Release header line.
	delistbuf(bufp);				// Remove from buffer list.
	free((void *) bufp);				// Release buffer block.
	if(kdp != NULL)
		unbindent(kdp);				// Delete buffer key binding.

	return rc.status;
	}

// Dispose of a buffer by name.  Ignore changes if n > 0.  Return status.
int deleteBuf(Value *rp,int n) {
	Buffer *bufp;
	uint aflags = ARG_FIRST;

	// If interactive, get buffer name from user.
	if(!(opflags & OPSCRIPT)) {
		bufp = bdefault(true,false);
		if(bcomplete(rp,text26,bufp != NULL ? bufp->b_bname : NULL,OPDELETE,&bufp,NULL) != SUCCESS || bufp == NULL)
				// "Delete"
			return rc.status;
		goto nukeit;
		}

	// Script mode: get buffer name(s) to delete.
	do {
		if(aflags == ARG_FIRST) {
			if(!havesym(s_any,true))
				return rc.status;		// Error.
			}
		else if(!havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(rp,aflags) != SUCCESS)
			return rc.status;
		aflags = 0;
		if((bufp = bsrch(rp->v_strp,NULL)) == NULL)
			return rcset(FAILURE,0,text118,rp->v_strp);
				// "No such buffer '%s'"
nukeit:
		// Nuke it.
		if(bdelete(bufp,n > 0 ? CLBIGNCHGD : 0) != SUCCESS)
			break;
		} while(opflags & OPSCRIPT);

	return rc.status;
	}

// Rename the current buffer.  If n > 0, derive the name from the attached filename and use it (without prompting).
int setBufName(Value *rp,int n) {
	Buffer *bufp;
	char *prmtp = text29;
		// "Change buffer name to"
	Value *bnamep;			// New buffer name.
	char askstr[NWORK];

	// We cannot rename an executing buffer.
	if((opflags & OPEVAL) && curbp->b_nexec > 0)
		return rcset(FAILURE,0,text284,text275,text248);
			// "Cannot %s %s buffer","rename","an executing"

	if(vnew(&bnamep,false) != 0)
		return vrcset();

	// Auto-rename if n > 0 (which indicates evaluation mode as well).  Do nothing if buffer name is
	// already the target name.
	if(n > 0) {
		if(curbp->b_fname == NULL)
			return rcset(FAILURE,0,text145,curbp->b_bname);
					// "No filename associated with buffer '%s'"
		if(vsalloc(bnamep,NBNAME + 1) != 0)
			return vrcset();
		if(strcmp(fbname(bnamep->v_strp,curbp->b_fname),curbp->b_bname) == 0)
			return rc.status;
		bunique(bnamep->v_strp,NULL);
		goto setnew;
		}
ask:
	// Get the new buffer name.
	if(getarg(bnamep,prmtp,NULL,CTRL | 'M',NBNAME,ARG_FIRST) != SUCCESS || (!(opflags & OPSCRIPT) && vistfn(bnamep,VNIL)) ||
	 tostr(bnamep,TSNOBOOLN) != SUCCESS)
		return rc.status;

	// Valid buffer name?
	if(!isbname(bnamep->v_strp)) {
		if(opflags & OPSCRIPT)
			return rcset(FAILURE,0,text128,bnamep->v_strp);
				// "Invalid buffer name '%s'"
		sprintf(prmtp = askstr,text128,bnamep->v_strp);
				// "Invalid buffer name '%s'"
		strcat(prmtp,text324);
				// ".  New name"
		goto ask;	// Try again.
		}

	// Check for duplicate name.
	bufp = bheadp;
	do {
		if(bufp != curbp) {

			// Other buffer with this name?
			if(strcmp(bnamep->v_strp,bufp->b_bname) == 0) {
				if(opflags & OPSCRIPT)
					return rcset(FAILURE,0,text181,text58,bnamep->v_strp);
						// "%s name '%s' already in use","Buffer"
				prmtp = text25;
					// "That name is already in use.  New name"
				goto ask;		// Try again.
				}

			// Macro buffer name?
			if((*curbp->b_bname == SBMACRO) != (*bnamep->v_strp == SBMACRO)) {
				if(opflags & OPSCRIPT)
					return rcset(FAILURE,0,*bnamep->v_strp == SBMACRO ? text268 : text270,bnamep->v_strp,
					 SBMACRO);
						// "Non-macro buffer name '%s' cannot begin with %c"
						// "Macro buffer name '%s' must begin with %c"
				sprintf(prmtp = askstr,"%s'%c'%s",text273,SBMACRO,text324);
					// "Macro buffer names (only) begin with ",".  New name"
				goto ask;		// Try again.
				}
			}
		} while((bufp = bufp->b_nextp) != NULL);

	// Valid macro name?
	if(*bnamep->v_strp == SBMACRO) {
		char *strp = bnamep->v_strp + 1;
		enum e_sym sym = getident(&strp,NULL);
		if((sym != s_ident && sym != s_identq) || *strp != '\0') {
			if(opflags & OPSCRIPT)
				return rcset(FAILURE,0,text286,bnamep->v_strp + 1);
					// "Invalid identifier '%s'"
			sprintf(prmtp = askstr,text286,bnamep->v_strp + 1);
					// "Invalid identifier '%s'"
			strcat(prmtp,text324);
					// ".  New name"
			goto ask;	// Try again.
			}
		}
setnew:
	// New name is unique and valid.  Rename the buffer.
	if((curbp->b_flags & BFMACRO) &&
	 amfind(curbp->b_bname + 1,OPDELETE,0) != SUCCESS)	// Remove old macro name from CFAM list...
		return rc.status;
	strcpy(curbp->b_bname,bnamep->v_strp);			// copy new buffer name to structure...
	relistbuf();						// reorder buffer in list...
	if((curbp->b_flags & BFMACRO) &&
	 amfind(curbp->b_bname + 1,OPCREATE,PTRMACRO) != SUCCESS)// add new macro name to CFAM list...
		return rc.status;
	upmode(curbp);						// and make all affected mode lines replot.
	if(!(opflags & OPSCRIPT))
		(void) mlerase(0);
	vxfer(rp,bnamep);

	return rc.status;
	}

#if 0
// Translate a long to ASCII form.
static void flong_asc(char *buf,int width,long num) {

	buf[width] = 0;				// End of string.
	while(num >= 10) {			// Conditional digits.
		buf[--width] = (int)(num % 10L) + '0';
		num /= 10L;
		}
	buf[--width] = (int) num + '0';		// Always 1 digit.
	while(width != 0)			// Pad with blanks.
		buf[--width] = ' ';
	}
#endif

// Get size of a buffer in lines and bytes.  Set *lp to line count if not NULL and return byte count.
long buflength(Buffer *bufp,long *lp) {
	long nlines,nbytes;
	Line *lnp;

	nlines = nbytes = 0;
	lnp = lforw(bufp->b_hdrlnp);
	while(lnp != bufp->b_hdrlnp) {
		++nlines;
		nbytes += lused(lnp) + 1;
		lnp = lforw(lnp);
		}

	if(lp != NULL)
		*lp = nlines;
	return nbytes;
	}

// Store character c in s len times and return pointer to terminating null.
char *dupchr(char *s,int c,int len) {

	while(len-- > 0)
		*s++ = c;
	*s = '\0';
	return s;
	}

// Add text (which may contain CRs) to the end of the indicated buffer.  Return status.  Note that this works on
// non-displayed buffers as well.
int bappend(Buffer *bufp,char *textp) {
	Line *lnp;
	int len;
	bool firstpass = true;
	char *strp0,*strp1;

	// Loop for each line segment found that ends with CR or null.
	strp0 = strp1 = textp;
	len = 0;
	for(;;) {
		if(*strp1 == '\r' || *strp1 == '\0') {

			// Allocate memory for the line and copy it over.
			if(lalloc(len = strp1 - strp0,&lnp) != SUCCESS)
				return rc.status;
			memcpy(lnp->l_text,strp0,len);

			// Add the new line to the end of the buffer.
			bufp->b_hdrlnp->l_prevp->l_nextp = lnp;
			lnp->l_prevp = bufp->b_hdrlnp->l_prevp;
			bufp->b_hdrlnp->l_prevp = lnp;
			lnp->l_nextp = bufp->b_hdrlnp;

			// If the point was at the end of the buffer, move it to the beginning of the new line.
			if(firstpass && bufp->b_face.wf_dot.lnp == bufp->b_hdrlnp)
				bufp->b_face.wf_dot.lnp = lnp;
			firstpass = false;

			// Adjust pointers.
			if(*strp1 == '\0')
				break;
			strp0 = strp1 + 1;
			}
		++strp1;
		}

	return rc.status;
	}

// Read the nth next line from a buffer and store in rp.  Return status.
int readBuf(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"

	return bufop(rp,n,NULL,BOPREADBUF,0);
	}

// Build and pop up a buffer containing a list of all buffers.  List hidden buffers as well if n arg.  Render buffer and return
// status.
int showBuffers(Value *rp,int n) {
	char *strp;
	Buffer *listp,*bufp;
	ModeSpec *msp;
	ModeRec *mrp;
	StrList rpt;
	int sizecol = 21;
	int bnamecol = 31;
	int filecol = 52;
	bool skipLine;
	char wkbuf[filecol + 16];
	static struct {				// Array of buffer flags.
		ushort bflag,sbchar;
		} *sbp,sb[] = {
			{BFACTIVE,SBACTIVE},	// Activated buffer -- file read in.
			{BFCHGD,SBCHGD},	// Changed buffer.
			{BFHIDDEN,SBHIDDEN},	// Hidden buffer.
			{BFMACRO,SBMACRO},	// Macro buffer.
			{BFPREPROC,SBPREPROC},	// Preprocessed buffer.
			{BFTRUNC,SBTRUNC},	// Truncated buffer.
			{BFNARROW,SBNARROW},	// Narrowed buffer.
			{0,0}};

	// Get new buffer and string list.
	if(sysbuf(text159,&listp) != SUCCESS)
			// "Buffers"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	// Write headers.
	if(vputs(text30,&rpt) != 0 ||
	      // "ACHMPTN    Modes        Size       Buffer               File"
	 vputs("\r------- ------------ --------- -------------------- -------------------------------",&rpt) != 0)
		return vrcset();

	// Build lines for global, show, and default modes.
	mrp = modetab;
	do {
		if(vputc('\r',&rpt) != 0)
			return vrcset();
		msp = (mrp - modetab) == MDR_DEFAULT ? bmodeinfo : gmodeinfo;
		strp = dupchr(wkbuf,' ',8);

		// Output the mode codes.
		do {
			*strp++ = (mrp->flags & msp->mask) ? msp->code : '.';
			} while((++msp)->name != NULL);
		do {
			*strp++ = ' ';
			} while(strp < wkbuf + bnamecol);
		strp = stpcpy(strp,mrp->cmdlabel);
		*strp++ = ' ';
		strcpy(strp,text297);
			// "modes"
		if(vputs(wkbuf,&rpt) != 0)
			return vrcset();
		} while((++mrp)->cmdlabel != NULL);

	// Output the list of buffers.
	bufp = bheadp;
	skipLine = true;
	do {
		// Skip hidden buffers if default n.
		if((bufp->b_flags & BFHIDDEN) && n == INT_MIN)
			continue;

		if(vputc('\r',&rpt) != 0)
			return vrcset();
		if(skipLine) {
			if(vputc('\r',&rpt) != 0)
				return vrcset();
			skipLine = false;
			}
		strp = wkbuf;					// Start at left edge.

		// Output the buffer flag indicators.
		sbp = sb;
		do {
			*strp++ = (bufp->b_flags & sbp->bflag) ? sbp->sbchar : ' ';
			} while((++sbp)->bflag != 0);

		*strp++ = ' ';					// Gap.

		// Output the buffer mode codes.
		msp = bmodeinfo;
		do {
			*strp++ = (bufp->b_modes & msp->mask) ? msp->code : '.';
			} while((++msp)->name != NULL);

#if (MMDEBUG & DEBUG_BWINDCT) && 0
		sprintf(strp,"%3hu",bufp->b_nwind);
		strp += 3;
#else
		do {
			*strp++ = ' ';
			} while(strp < wkbuf + sizecol);
#endif
#if 1
		sprintf(strp,"%9lu",buflength(bufp,NULL));	// 9-digit (minimum) buffer size.
#else
		sprintf(strp,"%9lu",buflength(bufp,NULL) + 1023400000ul);		// Test it.
#endif
		strp = strchr(strp,'\0');
		*strp++ = ' ';					// Gap.
#if 0
		if(strlen(bufp->b_bname) > 20) {		// Will buffer name fit in column?
			strp = stplcpy(strp,bufp->b_bname,20);	// No, truncate it and add '$'.
			strp = stpcpy(strp,"$");
			}
		else
#endif
			strp = stpcpy(strp,bufp->b_bname);	// Copy buffer name.

		if(bufp->b_fname != NULL)			// Pad if filename exists.
			do {
				*strp++ = ' ';
				} while(strp < wkbuf + filecol);
		*strp = '\0';					// Save buffer and add filename.
		if(vputs(wkbuf,&rpt) != 0 ||
		 (bufp->b_fname != NULL && vputs(bufp->b_fname,&rpt) != 0))
			return vrcset();

		// Onward.
		} while((bufp = bufp->b_nextp) != NULL);

	// Add the results to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(bappend(listp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,listp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}
