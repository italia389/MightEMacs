// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// buffer.c	Buffer management routines for MightEMacs.
//
// Some of the functions are internal, and some are attached to user keys.

#include "os.h"
#include "std.h"
#include "lang.h"
#include "cmd.h"
#include "bind.h"
#include "exec.h"
#include "file.h"
#include "main.h"

// Clear a buffer's filename, if any.
void clfname(Buffer *bufp) {

	if(bufp->b_fname != NULL) {
		(void) free((void *) bufp->b_fname);
		bufp->b_fname = NULL;
		}
	}

// Set buffer filename.  Return status.
int setfname(Buffer *bufp,char *fname) {

	clfname(bufp);

	// Ignore null filename.
	if(fname != NULL && *fname != '\0') {
		if((bufp->b_fname = (char *) malloc(strlen(fname) + 1)) == NULL)
			return rcset(Panic,0,text94,"setfname");
				// "%s(): Out of memory!"
		strcpy(bufp->b_fname,fname);
		}

	if(!(opflags & OpScript))
		(void) mlerase(0);

	return rc.status;
	}

// Get the default buffer (a guess) for various buffer commands.  Search backward if "backward" is true; otherwise, forward.
// Consider all visible buffers (active or inactive).  Return a pointer to the first buffer found, or NULL if none exist.
static Buffer *bdefault(bool backward) {
	Buffer *bufp;

	// Find the first buffer before or after the current one that is visible.
	bufp = backward ? curbp->b_prevp : curbp->b_nextp;
	for(;;) {
		if(bufp == NULL)
			bufp = backward ? btailp : bheadp;
		else if(bufp == curbp)
			return NULL;
		else if(!(bufp->b_flags & BFHidden))
			break;
		else
			bufp = backward ? bufp->b_prevp : bufp->b_nextp;
		}

	return bufp;
	}

#if MMDebug & Debug_Narrow
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
int narrowBuf(Datum *rp,int n,Datum **argpp) {
	EScreen *scrp;
	EWindow *winp;		// Windows to fix up pointers in as well.
	Mark *mkp;
	Line *lnp;
	Dot *dotp = &curwp->w_face.wf_dot;

	// Make sure we aren't already narrowed.
	if(curbp->b_flags & BFNarrow)
		return rcset(Failure,0,text71,text58,curbp->b_bname);
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

		// Move back n lines max.
		n = 1;
		while(lback(dotp->lnp) != curbp->b_hdrlnp) {
			dotp->lnp = lback(dotp->lnp);
			++n;
			if(++i == 0)
				break;
			}
		}
	else if(n == 0 && reglines(&n,NULL) != Success)
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
#if MMDebug & Debug_Narrow
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
				winp->w_flags |= (WFHard | WFMode);
				}
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);
	opflags |= OpScrRedraw;

	// and now remember we are narrowed.
	curbp->b_flags |= BFNarrow;

	return dsetstr(curbp->b_bname,rp) != 0 ? drcset() : rcset(Success,0,text73,text58);
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
				(void) mfind(winp->w_id,&mkp,MkOpt_Wind);	// Can't return an error.
				if(mkp != NULL) {
					winp->w_face.wf_dot = mkp->mk_dot;
					winp->w_force = mkp->mk_force;
					winp->w_flags |= WFForce;
					}
				}
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// In front screen only...
	winp = cursp->s_wheadp;
	do {
		if(winp->w_bufp == bufp)
			winp->w_flags |= (WFHard | WFMode);
		} while((winp = winp->w_nextp) != NULL);

	// and now forget that we are narrowed.
	bufp->b_flags &= ~BFNarrow;
	}

// Widen (restore) a narrowed buffer.  Set rp to buffer name and return status.
int widenBuf(Datum *rp,int n,Datum **argpp) {

	// Make sure we are narrowed.
	if(!(curbp->b_flags & BFNarrow))
		return rcset(Failure,0,text74,text58,curbp->b_bname);
			// "%s '%s' is not narrowed","Buffer"

	// Restore current buffer to pre-narrowed state.
	unnarrow(curbp);
	if(dsetstr(curbp->b_bname,rp) != 0)
		return drcset();
	(void) rcset(Success,0,text75,text58);
		// "%s widened","Buffer"
	return execCF(rp,INT_MIN,cftab + cf_redrawScreen,0,0);
	}

// Search the buffer list for given name and return pointer to slot if found; otherwise, NULL.  In either case, set *bufpp to
// prior slot if bufpp not NULL;
Buffer *bsrch(char *bname,Buffer **bufpp) {
	Buffer *bufp1,*bufp2;
	int result;

	// Scan the buffer list.
	bufp1 = NULL;
	bufp2 = bheadp;
	while(bufp2 != NULL) {
		if((result = strcmp(bufp2->b_bname,bname)) == 0)
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

// Generate a valid buffer name from a pathname.  bname is assumed to point to a buffer that is NBufName + 1 or greater in size.
static char *fbname(char *bname,char *fname) {
	char *str;

	// Get file basename and validate it.  Keep filename extension if it's numeric.
	stplcpy(bname,fbasename(fname,(str = strrchr(fname,'.')) != NULL && asc_long(str + 1,NULL,true) ? true : false),
	 NBufName + 1);
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
// that bname points to is assumed to be of size NBufName + 1 or greater.
static char *bunique(char *bname,char *fname) {
	int n,i;
	char *str0,*str1,*strz = bname + NBufName;
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

		// Add 1 to numeric suffix (or begin at 0), and put it back.  Assume that NBufName is >= number of bytes needed
		// to encode an int in decimal form (10 for 32-bit int) and that we will never have more than INT_MAX buffers
		// (> 2 billion).
		long_asc((long) (n + i),wkbuf);
		n = strlen(wkbuf);
		strcpy(str0 + n > strz ? strz - n : str0,wkbuf);
		}

	return bname;
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

// Create buffer-extension record (MacInfo) for given buffer.  Return status.
int bextend(Buffer *bufp) {
	MacInfo *mip;

	if((bufp->b_mip = mip = (MacInfo *) malloc(sizeof(MacInfo))) == NULL)
		return rcset(Panic,0,text94,"bextend");
			// "%s(): Out of memory!"
	mip->mi_nargs = -1;
	mip->mi_nexec = 0;
	mip->mi_execp = NULL;
	dinit(&mip->mi_usage);
	dinit(&mip->mi_desc);
	return rc.status;
	}

// Find a buffer by name and return status or boolean result.  Actions taken depend on cflags:
//	If CRBFile is set:
//		Use the base filename of "name" as the default buffer name; otherwise, use "name" directly.
//	If CRBForce is set:
//		Create a unique buffer name derived from the default one.  (CRBCreate is assumed to also be set.)
//	If CRBCreate is set:
//		If the buffer is found and CRBForce is not set:
//			Set *bufpp (if bufpp not NULL) to the buffer pointer, set *createdp (if createdp not NULL) to false,
//			and return status.
//		If the buffer is not found or CRBForce is set:
//			Create a buffer, set it's flag word to bflags, create a buffer-extension record if CRBExtend is set,
//			set *bufpp (if bufpp not NULL) to the buffer pointer, set *createdp (if createdp not NULL) to true,
//			and return status.
//	If CRBCreate is not set:
//		If the buffer is found:
//			Set *bufpp (if bufpp not NULL) to the buffer pointer and return true, ignoring createdp.
//		If the buffer is not found:
//			Return false, ignoring bufpp and createdp.
int bfind(char *name,ushort cflags,ushort bflags,Buffer **bufpp,bool *createdp) {
	Buffer *bufp1,*bufp2;
	char *bname,wkbuf[NBufName + 1];

	// Set default buffer name.
	if(cflags & CRBForce)
		(void) bsrch(bname = (cflags & CRBFile) ? bunique(wkbuf,name) : bunique(strcpy(wkbuf,name),NULL),&bufp1);
	else {
		if(cflags & CRBFile)
			fbname(bname = wkbuf,name);
		else
			bname = name;

		// Search for the buffer.
		if((bufp2 = bsrch(bname,&bufp1)) != NULL) {

			// Found it.  Return results.
			if(bufpp != NULL)
				*bufpp = bufp2;
			if(cflags & CRBCreate) {
				if(createdp != NULL)
					*createdp = false;
				return rc.status;
				}
			return true;
			}
		}

	// No such buffer exists, create it?
	if(cflags & CRBCreate) {
		Line *lnp;

		// Valid buffer name?
		if(!isbname(bname))
			return rcset(Failure,0,text128,bname);
					// "Invalid buffer name '%s'"
		// Macro name?
		if(*bname == SBMacro && !(bflags & BFMacro))
			return rcset(Failure,0,text268,text180,bname,SBMacro);
					// "Cannot %s buffer: name '%s' cannot begin with %c","create"

		// Allocate the memory for the buffer and the "magic" (header) line.
		if((bufp2 = (Buffer *) malloc(sizeof(Buffer))) == NULL)
			return rcset(Panic,0,text94,"bfind");
				// "%s(): Out of memory!"
		if(lalloc(0,&lnp) != Success)
			return rc.status;	// Fatal error.
		lnp->l_nextp = lnp->l_prevp = lnp;
		lnp->l_text[0] = '\0';

		// Create buffer extension if requested.
		if(!(cflags & CRBExtend))
			bufp2->b_mip = NULL;
		else if(bextend(bufp2) != Success)
			return rc.status;

		// Insert the buffer into the list (using prior slot pointer from bsrch() call)...
		enlistbuf(bufp1,bufp2);

		// and set up the other buffer fields.
		bufp2->b_mroot.mk_nextp = NULL;
		bufinit(bufp2,bufp2->b_hdrlnp = lnp);
		bufp2->b_ntoplnp = bufp2->b_nbotlnp = NULL;
		bufp2->b_flags = bflags | BFActive;
		bufp2->b_modes = modetab[MdRec_Default].flags;
		bufp2->b_nwind = bufp2->b_nalias = 0;
		bufp2->b_fname = NULL;
		strcpy(bufp2->b_bname,bname);

		// Add macro name to the CFAM list and return results.
		if(bflags & BFMacro)
			(void) amfind(bufp2->b_bname + 1,OpCreate,PtrMacro);
		if(bufpp != NULL)
			*bufpp = bufp2;
		if(createdp != NULL)
			*createdp = true;
		return rc.status;
		}

	// Buffer not found and not creating.
	return false;
	}

// This routine blows away all of the text in a buffer.  If it's marked as changed, it will not be cleared unless CLBIgnChgd is
// set in "flags" or the user gives the okay.  This to save the user the grief of losing text.  If CLBClrFilename is set, the
// filename associated with the buffer is set to null.  If the buffer is narrowed and CLBUnnarrow is set, the buffer is silently
// unnarrowed before being cleared; otherwise, the user is prompted to give the okay before proceeding.  The window chain is
// nearly always wrong if this routine is called; the caller must arrange for the updates that are required.
//
// Return success if buffer is erased; otherwise, "cancelled", if user changed his or her mind.
int bclear(Buffer *bufp,ushort flags) {
	Line *lnp;
	bool yep;
	bool bufErased = false;
	bool eraseML = false;

	// Executing buffer?
	if((opflags & OpEval) && bufp->b_mip != NULL && bufp->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text226,text264,text238,bufp->b_bname);
			// "Cannot %s %s buffer '%s'","clear","executing"

	// Visible buffer and changed?  Skip if need "narrowed buffer" confirmation as well (which preempts this).
	if(!(bufp->b_flags & BFHidden) && (bufp->b_flags & BFChgd) && !(flags & CLBIgnChgd) &&
	 (!(bufp->b_flags & BFNarrow) || (flags & CLBUnnarrow))) {
		char *str,prmt[NWork];
		eraseML = true;

		// Get user confirmation.
		if((opflags & OpScript) || (flags & CLBMulti)) {
			sprintf(stpcpy(prmt,text32),text369,bufp->b_bname);
				// "Discard changes"," in buffer '%s'"
			str = prmt;
			}
		else
			str = text32;
		if(mlyesno(str,&yep) != Success)
			return rc.status;
		if(!yep)
			goto ClearML;
		}

	// Narrowed buffer?
	if(bufp->b_flags & BFNarrow) {

		// Force?
		if(flags & CLBUnnarrow)

			// Yes, restore buffer to pre-narrowed state.
			unnarrow(bufp);
		else if(!(flags & CLBIgnChgd)) {

			// Not a force.  Get user confirmation (and leave narrowed).
			eraseML = true;
			if(mlyesno(text95,&yep) != Success)
					// "Narrowed buffer... delete current portion"
				return rc.status;
			if(!yep)
				goto ClearML;
			}
		}

	// It's a go... erase it.
	if(flags & CLBClrFilename)
		clfname(bufp);					// Zap any filename.
	while((lnp = lforw(bufp->b_hdrlnp)) != bufp->b_hdrlnp)	// Free all Line storage.
		lfree(lnp);
	lchange(bufp,WFHard);					// Update window flags.
	if(bufp->b_flags & BFNarrow)
		bufp->b_flags |= BFChgd;			// Mark as changed if narrowed...
	else
		bufp->b_flags &= ~BFChgd;			// otherwise, mark as not changed.
	bufinit(bufp,bufp->b_hdrlnp);				// Fix dot and remove marks.
	bufErased = true;

	if(eraseML)
ClearML:
		(void) mlerase(0);
	return bufErased ? rc.status : rcset(Cancelled,0,NULL);
	}

// Find a window displaying given buffer and return it, giving preference to current screen and current window.
EWindow *findwind(Buffer *bufp) {
	EScreen *scrp;
	EWindow *winp;

	// If current window is displaying the buffer, use it.
	if(curwp->w_bufp == bufp)
		return curwp;

	// In current screen...
	winp = cursp->s_wheadp;
	do {
		if(winp->w_bufp == bufp)
			goto Retn;
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
				goto Retn;
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);
Retn:
	return winp;
	}

// Get a buffer name (if n not default) and perform operation on buffer according to "op" argument.  If prmt == NULL, set rp
// to function return value; otherwise, buffer name.  Return status.
int bufop(Datum *rp,int n,char *prmt,uint op,uint flag) {
	Buffer *bufp = NULL;

	// Get the buffer name.  n will never be the default for a readBuf() call.
	if(n == INT_MIN)
		bufp = curbp;
	else {
		if(prmt != NULL)
			bufp = bdefault(false);
		if(bcomplete(rp,prmt,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success || bufp == NULL)
			return rc.status;
		}

	// Perform requested operation.
	switch(op) {
		case BOpSetFlag:
			bufp->b_flags |= flag;
			goto Notice;
		case BOpClrFlag:
			bufp->b_flags &= ~flag;
Notice:
			return (flag == BFHidden) ? rcset(Success,0,op == BOpSetFlag ? text113 : text344,text58) : rc.status;
									// "%s hidden","%s unhidden","Buffer"
		case BOpGotoLn:
			if(flag == 0) {
				op = BOpBeginEnd;
				flag = true;
				}
			// Fall through.
		default:	// BOpBeginEnd, BOpGotoLn, or BOpReadBuf

			// Move dot in buffer... usually a massive adjustment.  Set "hard motion" flag.
			{Dot *dotp;

			// Grab dot.  If the buffer is being displayed, get from window face instead of buffer face.
			if(bufp->b_nwind > 0) {
				EWindow *winp = findwind(bufp);
				winp->w_flags |= WFMove;
				dotp = &winp->w_face.wf_dot;
				}
			else
				dotp = &bufp->b_face.wf_dot;

			// Perform operation.
			switch(op) {
				case BOpBeginEnd:
					// Go to beginning or end of buffer.
					dotp->off = 0;
					dotp->lnp = flag ? bufp->b_hdrlnp : lforw(bufp->b_hdrlnp);
					break;
				case BOpGotoLn:
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
				default:	// BOpReadBuf
					// Read next line from buffer n times.
					while(n-- > 0) {
						size_t len;

						// If we are at the end, return nil.
						if(dotp->lnp == bufp->b_hdrlnp) {
							dsetnil(rp);
							return rc.status;
							}

						// Get the length of the current line (from dot to end)...
						len = lused(dotp->lnp) - dotp->off;

						// return the spoils...
						if(dsetsubstr(ltext(dotp->lnp) + dotp->off,len,rp) != 0)
							return drcset();

						// and step the buffer's line pointer ahead one line.
						dotp->lnp = lforw(dotp->lnp);
						dotp->off = 0;
						}
					return rc.status;
				}
			}
		}
	if(flag == BFChgd)
		upmode(bufp);

	return rc.status;
	}

// Set name of a system (internal) buffer and call bfind();
int sysbuf(char *root,Buffer **bufpp) {
	char bname[strlen(root) + 2];

	bname[0] = BSysLead;
	strcpy(bname + 1,root);
	return bfind(bname,CRBCreate | CRBForce,BFHidden,bufpp,NULL);
	}

// Activate a buffer if needed.  Return status.
int bactivate(Buffer *bufp) {

	// Check if buffer is active.
	if(!(bufp->b_flags & BFActive)) {

		// Not active: read it in.
		(void) readin(bufp,NULL,true);
		}

	return rc.status;
	}

// Insert a buffer into the current buffer and set current region to inserted lines.  If n == 0, leave point before the inserted
// lines; otherwise, after.  Return status.
int insertBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	Line *buflnp,*lnp0,*lnp1,*lnp2;
	uint nline;
	int nbytes;
	WindFace *wfp = &curwp->w_face;

	// Get the buffer name.  Reject if current buffer.
	bufp = bdefault(false);
	if(bcomplete(rp,text55,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success || bufp == NULL)
			// "Insert"
		return rc.status;
	if(bufp == curbp)
		return rcset(Failure,RCNoFormat,text124);
			// "Cannot insert current buffer into itself"

	// Let user know what's up.
	if(mlputs(MLHome | MLWrap,text153) != Success)
			// "Inserting data..."
		return rc.status;

	// Prepare buffer to be inserted.
	if(bactivate(bufp) != Success)
		return rc.status;
	buflnp = bufp->b_hdrlnp;

	// Prepare current buffer.
	curbp->b_flags |= BFChgd;			// Mark as changed.
	curbp->b_mroot.mk_force = getwpos(curwp);	// Keep existing framing.
	wfp->wf_dot.lnp = lback(wfp->wf_dot.lnp);	// Back up a line and save dot in mark RMark.
	wfp->wf_dot.off = 0;
	curbp->b_mroot.mk_dot = wfp->wf_dot;

	// Insert each line from the buffer at point.
	nline = 0;
	while((buflnp = lforw(buflnp)) != bufp->b_hdrlnp) {
		if(lalloc(nbytes = lused(buflnp),&lnp1) != Success)
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

	// Adjust mark RMark to point to first inserted line (if any).
	curbp->b_mroot.mk_dot.lnp = lforw(curbp->b_mroot.mk_dot.lnp);

	// Advance dot to the next line (the end of the inserted text).
	wfp->wf_dot.lnp = lforw(wfp->wf_dot.lnp);

	// If n is zero, swap point and RMark.
	if(n == 0)
		(void) swapmid(RMark);

	lchange(curbp,WFHard | WFMode);

	// Report results.
	return rcset(Success,RCForce,"%s %u %s%s%s",text154,nline,text205,nline == 1 ? "" : "s",text355);
					// "Inserted","line"," and marked as region"
	}

// Attach a buffer to the current window, creating it if necessary (default).  Render buffer and return status.
int selectBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	bool created;

	// Get the buffer name.
	bufp = bdefault(false);
	if(bcomplete(rp,n < 0 && n != INT_MIN ? text27 : text24,bufp != NULL ? bufp->b_bname : (n == INT_MIN || n >= 0) ?
	 buffer1 : NULL,OpCreate,&bufp,&created) != Success || bufp == NULL)
			// "Pop","Switch to"
		return rc.status;

	// Render the buffer.
	return render(rp,n == INT_MIN ? 1 : n >= 0 ? n : created ? -2 : -1,bufp,n != INT_MIN && n < -1 ? RendAltML : 0);
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
		if(bfind(dest,CRBCreate,0,&bufp,&created) != Success)
			return rc.status;

		// Was a new buffer created?
		if(created)
			goto Retn;	// Yes, use it.

		// Nope, a buffer with that name already exists.  Try again unless limit reached.
		} while(--maxtries > 0);

	// Random-number approach failed... let bfind() "uniquify" it.
	(void) bfind(dest,CRBCreate | CRBForce,0,&bufp,NULL);
Retn:
	*bufpp = bufp;
	return rc.status;
	}

// Create a scratch buffer.  Render buffer and return status.
int scratchBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	char bname[NBufName + 1];

	// Create buffer...
	if(bscratch(bname,&bufp) != Success)
		return rc.status;

	// and render it.
	return render(rp,n == INT_MIN ? 1 : n >= 0 ? n : -2,bufp,n != INT_MIN && n < -1 ? RendAltML : 0);
	}

// Switch to (unless n == 0) the previous or next buffer in the buffer list n times.  Set rp to name of final buffer if
// successful.  If n < 0, switch once and delete the current buffer.  Return status.
int pnbuffer(Datum *rp,int n,bool prev) {
	Buffer *bufp0,*bufp1;
	ushort bflags;

	if(n == INT_MIN)
		n = 1;

	// Cycle backward or forward through buffer list n times, or just once if n <= 0.
	do {
		if((bufp1 = bdefault(prev)) == NULL)
			return rc.status;		// Only one visible buffer.
		if(n == 0)
			break;
		bufp0 = curbp;
		bflags = bufp1->b_flags;
		if(bswitch(bufp1) != Success)
			return rc.status;
		if(n < 0) {
			char bname[NBufName + 1];
			strcpy(bname,bufp0->b_bname);
			if(bdelete(bufp0,0) != Success || mlprintf(MLHome | MLWrap,text372,bname) != Success)
									// "Deleted buffer '%s'"
				return rc.status;
			if(!(bflags & BFActive))
				cpause(50);
			}
		} while(--n > 0);

	return dsetstr(bufp1->b_bname,rp) != 0 ? drcset() : rc.status;
	}

// Create array of buffer names and save result in rp.  If default n, get visible buffers only; if n <= 0, include hidden ones;
// if n > 0; get all buffers.  Return status.
int getbuflist(Datum *rp,int n) {
	Buffer *bufp;
	Array *aryp;
	Datum *datp;
	ushort exclude = (n == INT_MIN) ? BFHidden : n <= 0 ? BFMacro : 0;

	if((aryp = anew(0,NULL)) == NULL)
		return drcset();
	if(awrap(rp,aryp) != Success)
		return rc.status;

	// Cycle through buffers and make list.
	bufp = bheadp;
	do {
		if(!(bufp->b_flags & exclude) &&
		 ((datp = aget(aryp,aryp->a_used,true)) == NULL || dsetstr(bufp->b_bname,datp) != 0))
			return drcset();
		} while((bufp = bufp->b_nextp) != NULL);

	return rc.status;
	}

// Run exit-buffer hook (and return result) or enter-buffer hook.  Return status.
int bhook(Datum *rp,bool exitHook) {

	if(!(curbp->b_flags & BFMacro)) {
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

// Make given buffer current, update "lastbufp" global variable, and return status.  The top line, dot, and column offset values
// from the current window are saved in the old buffer's header and replacement ones are fetched from the new (given) buffer's
// header.
int bswitch(Buffer *bufp) {
	Datum *rp;

	// Nothing to do if bufp is current buffer.
	if(bufp == curbp)
		return rc.status;

	// Run exit-buffer user hook on current (old) buffer.
	if(dnewtrk(&rp) != 0)
		return drcset();
	if(bhook(rp,true) != Success)
		return rc.status;

	// Decrement window use count of current (old) buffer and save the current window settings.
	--curbp->b_nwind;
	wftobf(curwp,curbp);
	lastbufp = curbp;

	// Switch to new buffer.
	curwp->w_bufp = curbp = bufp;
	++curbp->b_nwind;

	// Activate buffer.
	if(bactivate(curbp) <= MinExit)
		return rc.status;

	// Update window settings.
	bftowf(curbp,curwp);

	// Run enter-buffer user hook on current (new) buffer.
	if(rc.status == Success)
		(void) bhook(rp,false);

	return rc.status;
	}

// Clear current buffer, or named buffer if n >= 0.  Force it if n != 0.  Set rp to false if buffer is not cleared; otherwise,
// true.  Return status.
int clearBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;

	// Get the buffer name to clear.
	if(n < 0)
		bufp = curbp;
	else {
		bufp = (n >= 0) ? bdefault(false) : NULL;
		if(bcomplete(rp,text169,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success || bufp == NULL)
				// "Clear"
			return rc.status;
		}

	// Blow text away unless user got cold feet.
	if(bclear(bufp,n != 0 ? CLBIgnChgd : 0) >= Cancelled) {
		dsetbool(rc.status == Success,rp);
		if(n >= 0)
			(void) mlerase(0);		// Clear "default buffer name" prompt.
		}

	return rc.status;
	}

// Check if macro is bound to a hook and if so, set an error and return true; otherwise, return false.
static bool ishook(Buffer *bufp) {
	HookRec *hrp = hooktab;

	do {
		if(bufp == hrp->h_bufp) {
			(void) rcset(Failure,0,text327,hrp->h_name);
					// "Macro bound to '%s' hook"
			return true;
			}
		} while((++hrp)->h_name != NULL);

	return false;
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
	mkp->mk_id = RMark;
	mkp->mk_dot.lnp = bufp->b_hdrlnp;
	mkp->mk_dot.off = mkp->mk_force = 0;
	}

// Delete the buffer pointed to by bufp.  Don't allow if buffer is being displayed, executed, or aliased.  Pass "flags" with
// CLBUnnarrow set to bclear() to clear the buffer, then free the header line and the buffer block.  Also delete any key binding
// and remove the name from the CFAM list if it's a macro.  Return status, including Cancelled if user changed his or her mind.
int bdelete(Buffer *bufp,ushort flags) {
	CFABPtr cfab;
	KeyDesc *kdp;

	// We cannot nuke a displayed buffer.
	if(bufp->b_nwind > 0)
		return rcset(Failure,0,text28,text58);
			// "%s is being displayed","Buffer"

	// We cannot nuke an executing buffer.
	if((opflags & OpEval) && bufp->b_mip != NULL && bufp->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text226,text263,text238,bufp->b_bname);
			// "Cannot %s %s buffer '%s'","delete","executing"

	// We cannot nuke a aliased buffer.
	if(bufp->b_nalias > 0)
		return rcset(Failure,0,text272,bufp->b_nalias);
			// "Macro has %hu alias(es)"

	// We cannot nuke a macro bound to a hook.
	if((bufp->b_flags & BFMacro) && ishook(bufp))
		return rc.status;

	// It's a go.  Blow text away (unless user got cold feet).
	if(bclear(bufp,flags | CLBClrFilename | CLBUnnarrow) != Success)
		return rc.status;

	// Delete from CFAM list.
	if((bufp->b_flags & BFMacro) && amfind(bufp->b_bname + 1,OpDelete,0) != Success)
		return rc.status;

	mdelete(bufp,MkOpt_Wind);			// Delete all marks.
	cfab.u.p_bufp = bufp;				// Get key binding, if any.
	kdp = getpentry(&cfab);

	if(savbufp == bufp)				// Unsave buffer if saved.
		savbufp = NULL;
	if(lastbufp == bufp)				// Clear "last buffer".
		lastbufp = NULL;
	ppfree(bufp);					// Release any macro preprocessor storage.
	if(bufp->b_mip != NULL)
		free((void *) bufp->b_mip);		// Release buffer extension record.
	free((void *) bufp->b_hdrlnp);			// Release header line.
	delistbuf(bufp);				// Remove from buffer list.
	free((void *) bufp);				// Release buffer block.
	if(kdp != NULL)
		unbindent(kdp);				// Delete buffer key binding.

	return rc.status;
	}

// Delete buffer(s) and return status.  Operation is controlled by numeric argument:
//	default		Delete named buffer(s), no force.
//	n < 0		Force-delete named buffer(s).
//	n == 0		Delete all other unchanged buffers.
//	n == 1		Delete all other buffers, no force.
//	n > 1		Force-delete all other buffers with initial confirmation.
int deleteBuf(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;

	// Check if processing all buffers.
	if(n >= 0) {
		// Get confirmation if interactive and force-deleting all other buffers.
		if(!(opflags & OpScript) && n > 1) {
			bool yep;

			if(mlyesno(text168,&yep) != Success)
					// "Nuke all other visible buffers"
				return rc.status;
			if(!yep)
				return mlerase(MLForce);
			}

		// Loop through buffer list.
		Buffer *bufp1;
		uint count = 0;
		bufp = bheadp;
		do {
			bufp1 = bufp->b_nextp;

			// Skip if current buffer, hidden, or modified and n is zero.
			if(bufp == curbp || (bufp->b_flags & BFHidden) || ((bufp->b_flags & BFChgd) && n == 0))
				continue;

			// Nuke it.
			if(mlprintf(MLHome | MLWrap,text367,bufp->b_bname) != Success)
					// "Deleting buffer %s..."
				return rc.status;
			cpause(50);
			if(bdelete(bufp,n > 1 ? CLBIgnChgd : n == 1 ? CLBMulti : 0) < Cancelled)
				break;
			if(rc.status == Success)
				++count;
			else
				rcclear();
			} while((bufp = bufp1) != NULL);

		return rcset(Success,RCForce,text368,count,count != 1 ? "s" : "");
					// "%u buffer%s deleted"
		}

	// Not all... process named buffer(s).
	if(n == INT_MIN)
		n = 0;		// No force.

	// If interactive, get buffer name from user.
	if(!(opflags & OpScript)) {
		bufp = bdefault(true);
		if(bcomplete(rp,text26,bufp != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success || bufp == NULL)
				// "Delete"
			return rc.status;
		goto NukeIt;
		}

	// Script mode: get buffer name(s) to delete.
	uint aflags = Arg_First | CFNotNull1;
	do {
		if(aflags & Arg_First) {
			if(!havesym(s_any,true))
				return rc.status;		// Error.
			}
		else if(!havesym(s_comma,false))
			break;					// No arguments left.
		if(funcarg(rp,aflags) != Success)
			return rc.status;
		aflags = CFNotNull1;
		if((bufp = bsrch(rp->d_str,NULL)) == NULL)
			return rcset(Failure,0,text118,rp->d_str);
				// "No such buffer '%s'"
NukeIt:
		// Nuke it.
		if(bdelete(bufp,n < 0 ? CLBIgnChgd : 0) != Success)
			break;
		} while(opflags & OpScript);

	return rc.status;
	}

// Rename the current buffer.  If n > 0, derive the name from the attached filename and use it (without prompting).
int setBufName(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	char *prmt = text29;
		// "Change buffer name to"
	Datum *bnamep;			// New buffer name.
	char askstr[NWork];

	// We cannot rename an executing buffer.
	if((opflags & OpEval) && curbp->b_mip != NULL && curbp->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text284,text275,text248);
			// "Cannot %s %s buffer","rename","an executing"

	if(dnewtrk(&bnamep) != 0)
		return drcset();

	// Auto-rename if n > 0 (which indicates evaluation mode as well).  Do nothing if buffer name is
	// already the target name.
	if(n > 0) {
		if(curbp->b_fname == NULL)
			return rcset(Failure,0,text145,curbp->b_bname);
					// "No filename associated with buffer '%s'"
		if(dsalloc(bnamep,NBufName + 1) != 0)
			return drcset();
		if(strcmp(fbname(bnamep->d_str,curbp->b_fname),curbp->b_bname) == 0)
			return rc.status;
		bunique(bnamep->d_str,NULL);
		goto SetNew;
		}
Ask:
	// Get the new buffer name.
	if(getarg(bnamep,prmt,NULL,RtnKey,NBufName,Arg_First | CFNotNull1,0) != Success || (!(opflags & OpScript) &&
	 bnamep->d_type == dat_nil))
		return rc.status;

	// Valid buffer name?
	if(!isbname(bnamep->d_str)) {
		if(opflags & OpScript)
			return rcset(Failure,0,text128,bnamep->d_str);
				// "Invalid buffer name '%s'"
		sprintf(prmt = askstr,text128,bnamep->d_str);
				// "Invalid buffer name '%s'"
		strcat(prmt,text324);
				// ".  New name"
		goto Ask;	// Try again.
		}

	// Check for duplicate name.
	bufp = bheadp;
	do {
		if(bufp != curbp) {

			// Other buffer with this name?
			if(strcmp(bnamep->d_str,bufp->b_bname) == 0) {
				if(opflags & OpScript)
					return rcset(Failure,0,text181,text58,bnamep->d_str);
						// "%s name '%s' already in use","Buffer"
				prmt = text25;
					// "That name is already in use.  New name"
				goto Ask;		// Try again.
				}

			// Macro buffer name?
			if((*curbp->b_bname == SBMacro) != (*bnamep->d_str == SBMacro)) {
				if(opflags & OpScript)
					return rcset(Failure,0,*bnamep->d_str == SBMacro ? text268 : text270,text275,
					 bnamep->d_str,SBMacro);
						// "Cannot %s buffer: name '%s' cannot begin with %c","rename"
						// "Cannot %s macro buffer: name '%s' must begin with %c","rename"
				sprintf(prmt = askstr,"%s'%c'%s",text273,SBMacro,text324);
					// "Macro buffer names (only) begin with ",".  New name"
				goto Ask;		// Try again.
				}
			}
		} while((bufp = bufp->b_nextp) != NULL);

	// Valid macro name?
	if(*bnamep->d_str == SBMacro) {
		char *str = bnamep->d_str + 1;
		enum e_sym sym = getident(&str,NULL);
		if((sym != s_ident && sym != s_identq) || *str != '\0') {
			if(opflags & OpScript)
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
	if((curbp->b_flags & BFMacro) &&
	 amfind(curbp->b_bname + 1,OpDelete,0) != Success)	// Remove old macro name from CFAM list...
		return rc.status;
	strcpy(curbp->b_bname,bnamep->d_str);			// copy new buffer name to structure...
	relistbuf();						// reorder buffer in list...
	if((curbp->b_flags & BFMacro) &&
	 amfind(curbp->b_bname + 1,OpCreate,PtrMacro) != Success)// add new macro name to CFAM list...
		return rc.status;
	upmode(curbp);						// and make all affected mode lines replot.
	if(!(opflags & OpScript))
		(void) mlerase(0);
	datxfer(rp,bnamep);

	return rc.status;
	}

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

// Add text (which may contain newlines) to the end of the indicated buffer.  Return status.  Note that this works on
// non-displayed buffers as well.
int bappend(Buffer *bufp,char *text) {
	Line *lnp;
	int len;
	bool firstpass = true;
	char *str0,*str1;

	// Loop for each line segment found that ends with newline or null.
	str0 = str1 = text;
	len = 0;
	for(;;) {
		if(*str1 == '\n' || *str1 == '\0') {

			// Allocate memory for the line and copy it over.
			if(lalloc(len = str1 - str0,&lnp) != Success)
				return rc.status;
			memcpy(lnp->l_text,str0,len);

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
			if(*str1 == '\0')
				break;
			str0 = str1 + 1;
			}
		++str1;
		}

	return rc.status;
	}

// Read the nth next line from a buffer and store in rp.  Return status.
int readBuf(Datum *rp,int n,Datum **argpp) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"

	return bufop(rp,n,NULL,BOpReadBuf,0);
	}

// Build and pop up a buffer containing a list of all buffers.  List hidden buffers as well if n arg.  Render buffer and return
// status.
int showBuffers(Datum *rp,int n,Datum **argpp) {
	char *str;
	Buffer *listp,*bufp;
	DStrFab rpt;
	int sizecol = 7;
	int filecol = 38;
	bool skipLine;
	char wkbuf[filecol + 16];
	static struct {				// Array of buffer flags.
		ushort bflag,sbchar;
		} *sbp,sb[] = {
			{BFActive,SBActive},	// Activated buffer -- file read in.
			{BFChgd,SBChgd},	// Changed buffer.
			{BFHidden,SBHidden},	// Hidden buffer.
			{BFMacro,SBMacro},	// Macro buffer.
			{BFPreproc,SBPreproc},	// Preprocessed buffer.
			{BFNarrow,SBNarrow},	// Narrowed buffer.
			{0,0}};

	// Get new buffer and string-fab object.
	if(sysbuf(text159,&listp) != Success)
			// "Buffers"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		return drcset();

	// Write headers.
	if(dputs(text30,&rpt) != 0 ||
	      // "ACHMPN    Size       Buffer               File"
	 dputs("\n------ --------- -------------------- -------------------------------",&rpt) != 0)
		return drcset();

	// Output the list of buffers.
	bufp = bheadp;
	skipLine = false;
	do {
		// Skip hidden buffers if default n.
		if((bufp->b_flags & BFHidden) && n == INT_MIN)
			continue;

		if(dputc('\n',&rpt) != 0)
			return drcset();
		if(skipLine) {
			if(dputc('\n',&rpt) != 0)
				return drcset();
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
		sprintf(str,"%9lu",buflength(bufp,NULL) + 1023400000ul);		// Test it.
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
			return drcset();

		// Onward.
		} while((bufp = bufp->b_nextp) != NULL);

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
		return drcset();
	if(bappend(listp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,listp,RendReset | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}
