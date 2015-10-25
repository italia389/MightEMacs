// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// file.c	Disk file handling for MightEMacs.
//
// These routines handle the reading, writing, and lookup of disk files.  All of details about reading from and writing to disk
// are in "fileio.c".
//
// USAGE OF SELECTED FILE AND BUFFER COMMANDS
// 
//							Unchg	Create	Hide	Switch	Read	Move	New	Switch to
// Command		n	Action		Force	buf	buf	buf	to buf	file	dot	wind	new wind
// --------------	------	--------------	------	------	------	------	------	------	------	------	---------
// clearBuf			Erase buffer
//			DEF			N
//			<any>			Y
//
// find/viewFile*		Find or view file		Y	
//			DEF					*		Y	Y
//			0	(background)			*		N	N
// 
// hideBuf			Hide buffer			N		N
//			DEF					*	Y	*
//
// insertFile/Pipe		Insert file/pipe output		N		N	Y
//			DEF					*		*	*	Y
//			< 0					*		*	*	N
// 
// readFile/Pipe*		Read file or pipe				N	Y
//			DEF					N		*	*		N	N
//			0	(background)			Y		*	*		N	N
//			1					Y		*	*		Y	N
//			> 1					Y		*	*		Y	Y
// 
// scratchBuf*			Create unique buffer		Y	
//			DEF					*		Y
//			0	(background)			*		N
// 
// selectBuf*			Switch to buffer			
//			DEF					Y		Y
//			< 0					N		Y
//			0	(background)			Y		N
//
// unchangeBuf			Unchange buffer			N		N
//			DEF				Y	*		*
//
// unhideBuf			Unhide buffer			N		N
//			DEF					*	unhide	*
//
// * command may create a buffer.
//
// CORE INPUT FUNCTIONS
//	findFile()
//		* get a filename and call getfile()
//	getfile(Value *rp,int n,bool view)
//		* check if existing buffer is attached to file
//		* if so, either return (do nothing) or switch to buffer
//		* if buffer not found, create new one and (optionally) set buffer to "read-only", read file via readin(),
//		  set filename, and switch to buffer
//	ifile(char *fname)
//		* insert given file into current buffer before dot line
//	insertFile()
//		* get a filename and call ifile()
//	rdfile(Value *rp,int n,char *fname)
//		* create new buffer with name derived from filename if requested
//		* read given file into current buffer or new buffer
//		* create new window if requested, attach new buffer to window, and optionally, switch to new window
//	readFile()
//		* get a filename and call rdfile()
//	readin(Buffer *bufp,char *fname,bool keep)
//		* clear given buffer and read a file into it (if fname is NULL, use buffer's filename; otherwise, stdin)
//		* if keep is true, save filename and call read hook; otherwise, delete buffer's filename
//	viewFile()
//		* get a filename and call getfile()
// Note: readin() is called by bswitch(), getfile(), pipeBuf(), readFile(), readPipe(), and startbuf().

#include "os.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

// Report I/O results as a return message, given active string list.  Return status.
static int iostat(StrList *slp,bool otpfile,Value *bknamep,int status,char *fname,char *lcmsg,uint nline) {
	char *strp;

	// Report any non-fatal read or close error.
	if(!otpfile && status < SUCCESS) {
		if(vputf(slp,text141,strerror(errno),fname) != 0 || vputc(' ',slp) != 0)
				// "I/O ERROR: %s, file '%s'"
			return vrcset();
		curbp->b_flags |= BFTRUNC;
		}

	// Begin wrapped message and report I/O operation.
	if(vputc('[',slp) != 0 || vputs(lcmsg,slp) != 0)
		return vrcset();

	// Report line count.
	if(vputf(slp," %u %s%s",nline,text205,nline != 1U ? "s" : "") != 0)
				// "line"
		return vrcset();

	// Non-standard line delimiter(s)?
	strp = otpfile ? curbp->b_otpdelim : curbp->b_inpdelim;
	if(strp[0] != '\0' && (strp[0] != '\n' || strp[1] != '\0')) {
		if(vputs(text252,slp) != 0 || vstrlit(slp,strp,0) != 0)
				// ", delimited by "
			return vrcset();
		}

	// Backup file created on output?
	if(bknamep != NULL) {
		if(vputs(text257,slp) != 0 || vputs(fbasename(bknamep->v_strp,true),slp) != 0 || vputc('\'',slp) != 0)
				// ", original file renamed to '"
			return vrcset();
		}

	if(vputc(']',slp) != 0 || vclose(slp) != 0)
		return vrcset();
	mlerase(0);
	return rcset(status >= SUCCESS ? SUCCESS : status,RCNOWRAP,slp->sl_vp->v_strp);
	}

// Prompt for a filename (which cannot be null).
int gtfilename(Value *rp,char *promptp,uint flags) {

	// Get a filename; default to current buffer's.
	return complete(rp,promptp,curbp->b_fname,CMPL_FILENAME | flags,NPATHINP,ARG_NOTNULL);
	}

// Read given file into a buffer.  If default n, use the current buffer.  If not default n, create a buffer and either (1), use
// a scratch name for the buffer and don't save the filename in the buffer header if "scratch" is true; or (2), derive the
// buffer name from the filename and save the filename in the buffer header if "scratch" is false.  Render buffer and return
// status.
int rdfile(Value *rp,int n,char *fname,bool scratch) {
	Buffer *bufp;

	if(n == INT_MIN)
		bufp = curbp;							// Use current buffer by default.
	else {									// Otherwise ...
		if(scratch) {							// if scratch buffer requested ...
			char bname[NBUFN + 1];

			if(bscratch(bname,&bufp) != SUCCESS)			// get one ...
				return rc.status;
			}							// otherwise, create a buffer.
		else if(bfind(fname,CRBCREATE | CRBFILE | CRBUNIQ,0,&bufp,NULL) != SUCCESS)
			return rc.status;
		}
	if(readin(bufp,fname,!scratch) != SUCCESS)				// Read the file.
		return rc.status;

	return render(rp,n == INT_MIN ? 1 : n >= 0 ? n : -2,bufp,		// Render the buffer.
	 n != INT_MIN && n < -1 ? RENDALTML : 0);
	}

// Insert file "fname" into the current buffer and set current region to inserted lines.  If n == 0, leave point before the
// inserted lines; otherwise, after.  Return the final status of the read.
int ifile(char *fname,int n) {
	int status;
	Line *lnp0,*lnp1,*lnp2;
	uint nline;
	int nbytes;
	StrList sl;
	WindFace *wfp = &curwp->w_face;

	if(vopen(&sl,NULL,false) != 0)
		return vrcset();

	// Open the file.
	if(ffropen(fname,true) != SUCCESS)
		goto out;

	// Let user know what's up.
	if(mlputs(MLHOME | MLWRAP,text153,vz_show) != SUCCESS)
			// "Inserting data ..."
		return rc.status;

	// Prepare buffer.
	curbp->b_flags |= BFCHGD;			// Mark as changed.

	// Back up a line and save dot in mark 0.
	wfp->wf_mark[0].mk_force = getwpos();		// Keep existing framing.
	wfp->wf_dot.lnp = lback(wfp->wf_dot.lnp);
	wfp->wf_dot.off = 0;
	wfp->wf_mark[0].mk_dot = wfp->wf_dot;

	// Read the file into the buffer at point.
	nline = 0;
	while((status = ffgetline(&nbytes)) == SUCCESS) {
		if(lalloc(nbytes,&lnp1) != SUCCESS)
			return rc.status;		// Fatal error.
		lnp0 = wfp->wf_dot.lnp;			// Line before insert.
		lnp2 = lnp0->l_nextp;			// Line after insert.

		// Re-link new line between lnp0 and lnp2 ...
		lnp2->l_prevp = lnp1;
		lnp0->l_nextp = lnp1;
		lnp1->l_prevp = lnp0;
		lnp1->l_nextp = lnp2;

		// and advance and save the line.
		wfp->wf_dot.lnp = lnp1;
		memcpy(lnp1->l_text,fi.lbuf,nbytes);
		++nline;
		}

	// Last read was unsuccessful, check for errors.
	if(status <= FATALERROR)
		return rc.status;			// Bail out.
	(void) ffclose(false);				// Ignore any error (for now).

	// Adjust mark 0 to point to first inserted line (if any).
	wfp->wf_mark[0].mk_dot.lnp = lforw(wfp->wf_mark[0].mk_dot.lnp);

	// Report results.
	if(iostat(&sl,false,NULL,status,fname,text154,nline) != SUCCESS)
					// "Inserted"
		return rc.status;
out:
	// Advance dot to the next line (the end of the inserted text).
	wfp->wf_dot.lnp = lforw(wfp->wf_dot.lnp);

	// If n is zero, swap point and mark 0.
	if(n == 0)
		swapMark(NULL,0);

	lchange(curbp,WFHARD | WFMODE);
	return rc.status;
	}

// Insert a file into the current buffer.  If zero argument, leave point before the inserted lines; otherwise, after.  This
// is really easy; all you do it find the name of the file, and call the standard "insert a file into the current buffer" code.
int insertFile(Value *rp,int n) {

	if(gtfilename(rp,text132,0) != SUCCESS || ((opflags & OPSCRIPT) == 0 && vistfn(rp,VNIL)))
			// "Insert file"
		return rc.status;

	// Insert the file.
	return ifile(rp->v_strp,n);
	}

// Find a file and optionally, read it into a buffer.  If n == 0, just create a buffer for the file if one does not already
// exist (and stay in current buffer).  If n != 0 and the file is not currently attached to a buffer, create a new buffer and
// set to read-only if "view" is true.  Render buffer and return status.
int getfile(Value *rp,int n,bool view) {
	Buffer *bufp;
	bool created = false;

	// Get filename.
	if(gtfilename(rp,view ? text134 : text133,view || n != 1 ? 0 : CMPL_NOAUTO) != SUCCESS ||
	 ((opflags & OPSCRIPT) == 0 && vistfn(rp,VNIL)))
			// "View file","Find file"
		return rc.status;

	// Check if an existing visible buffer is attached to the file.
	bufp = bheadp;
	do {
		if((bufp->b_flags & BFHIDDEN) == 0 && bufp->b_fname != NULL && strcmp(bufp->b_fname,rp->v_strp) == 0) {

			// Visible buffer found.  Return if not reading; otherwise, switch to the buffer.
			if((n == INT_MIN || n > 0) && mlputs(MLHOME | MLWRAP,text135,vz_show) != SUCCESS)
									// "Old buffer"
				return rc.status;
			goto vcheck;
			}
		} while((bufp = bufp->b_nextp) != NULL);

	// No buffer found ... create one.
	if(bfind(rp->v_strp,CRBCREATE | CRBFILE | CRBUNIQ,0,&bufp,&created) != SUCCESS)
		return rc.status;

	// Set filename and flag as inactive.
	if(setfname(bufp,rp->v_strp) != SUCCESS)
		return rc.status;
	bufp->b_flags = 0;
vcheck:
	// Put buffer in read-only mode if view is true.
	if(view)
		bufp->b_modes |= MDRDONLY;

	return render(rp,n == INT_MIN ? 1 : n >= 0 ? n : created ? -2 : -1,bufp,
	 RENDBOOL | (created ? RENDTRUE : 0) | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}

// Read a file into the given buffer, blowing away any text found there.  If fname is NULL, use the buffer's filename if set;
// otherwise, standard input.  If "keep" is true, save the filename in the buffer record (if it is not narrowed) and call the
// read hook after the file is read; otherwise, delete the buffer's filename if set.
int readin(Buffer *bufp,char *fname,bool keep) {
	Line *lnp1,*lnp2;
	EWindow *winp;
	bool cleared;
	int status,nbytes;
	uint nline;
	StrList sl;

	if(vopen(&sl,NULL,false) != 0)
		return vrcset();
	if(bclear(bufp,0,&cleared) != SUCCESS || !cleared)
		return rc.status;				// Error or user got cold feet.
	if(bufp->b_flags & BFNARROW)				// Mark as changed if narrowed.
		bufp->b_flags |= BFCHGD;
	bufp->b_modes |= modetab[MDR_DEFAULT].flags;		// Add current default buffer modes.

	// Determine filename.
	if(fname == NULL)
		fname = bufp->b_fname;
	else if(keep && (bufp->b_flags & BFNARROW) == 0 &&
	 setfname(bufp,fname) != SUCCESS)			// Save given filename.
		return rc.status;

	// Open the file.
	if((status = ffropen(fname,false)) != SUCCESS)
		goto out;

	// Let user know what's up.
	if(mlputs(MLHOME | MLWRAP,text139,vz_show) != SUCCESS)
			// "Reading data ..."
		return rc.status;

	// Read it in.
	nline = 0U;
	while((status = ffgetline(&nbytes)) == SUCCESS) {
		if(lalloc(nbytes,&lnp1) != SUCCESS)
			return rc.status;			// Fatal error.
		lnp2 = lback(bufp->b_hdrlnp);
		lnp2->l_nextp = lnp1;
		lnp1->l_nextp = bufp->b_hdrlnp;
		lnp1->l_prevp = lnp2;
		bufp->b_hdrlnp->l_prevp = lnp1;
		memcpy(lnp1->l_text,fi.lbuf,nbytes);
		++nline;
		}

	// Last read was unsuccessful, check for errors.
	if(status <= FATALERROR)
		return rc.status;				// Bail out.
	if(status >= SUCCESS)
		status = ffclose(false);			// Read was successful; preserve close status.
	else
		(void) ffclose(false);				// Read error; ignore close status.

	// Report results.
	if(iostat(&sl,false,NULL,status,fname,text140,nline) != SUCCESS)
					// "Read"
		return rc.status;
out:
	// Erase filename or run user-assigned read hook.
	if(!keep)
		clfname(bufp);
	else if(!(bufp->b_flags & (BFHIDDEN | BFMACRO)) &&
	 exechook(NULL,INT_MIN,hooktab + HKREAD,2,bufp->b_bname,defnil(bufp->b_fname)) <= FATALERROR)
		return rc.status;

	// Make sure buffer is active.
	bufp->b_flags |= BFACTIVE;

	// Update buffer and window pointers.
	faceinit(&bufp->b_face,lforw(bufp->b_hdrlnp));
	winp = wheadp;
	do {
		if(winp->w_bufp == bufp) {
			faceinit(&winp->w_face,lforw(bufp->b_hdrlnp));
			winp->w_flags |= WFMODE | WFHARD;
			}
		} while((winp = winp->w_nextp) != NULL);

	// Return status.
	return (status == IONSF) ? rcset(SUCCESS,0,text138) : rc.status;
					// "New file"
	}

// Check given buffer flags and verify with user before writing to a file.  Return status.
static int bufcheck(Buffer *bufp) {
	bool yep;
	StrList prompt;
	struct {
		ushort flag;
		char *msg;
		} *checkp,checktab[] = {
			{BFTRUNC,text146},
				// "Truncated file in buffer '%s' ... write it out"
			{BFNARROW,text147},
				// "Narrowed buffer '%s' ... write it out"
			{0,NULL}};

	checkp = checktab;
	do {
		// Complain about truncated files and narrowed buffers.
		if((bufp->b_flags & checkp->flag) != 0) {
			if(vopen(&prompt,NULL,false) != 0 ||
			 vputf(&prompt,checkp->msg,bufp->b_bname) != 0 || vclose(&prompt) != 0)
				return vrcset();
			if(mlyesno(prompt.sl_vp->v_strp,&yep) != SUCCESS)
				return rc.status;
			if(!yep)
				return rcset(USERABORT,0,NULL);
			}
		} while((++checkp)->flag != 0);

	return rc.status;
	}

// Ask for a filename, and write the contents of the current buffer to that file.  Update the remembered filename and clear
// the buffer changed flag.  Return status.  This handling of filenames is different from the earlier versions and is more
// compatable with Gosling EMACS than with ITS EMACS.
int fileout(Value *rp,char *promptp,int mode) {

	// Get the filename.
	if(gtfilename(rp,promptp,CMPL_NOAUTO) != SUCCESS || ((opflags & OPSCRIPT) == 0 && vistfn(rp,VNIL)))
		return rc.status;

	// Complain about truncated files and narrowed buffers.
	if(bufcheck(curbp) != SUCCESS)
		return rc.status;

	// It's a go ... write the file.
	if(writeout(curbp,rp->v_strp,mode) != SUCCESS)
		return rc.status;

	if(mode == 'w' && setfname(curbp,rp->v_strp) != SUCCESS)	// Update filename if write (not append).
		return rc.status;
	upmode(curbp);							// Update mode line(s).

	return rc.status;
	}

// Save the contents of the current buffer (or all buffers if n > 0) to their associated files.  Do nothing if nothing has
// changed.  Error if there is no remembered filename for a buffer.
int savebufs(int n,bool edexit) {
	Buffer *bufp;
	int count = 0;

	bufp = (n <= 0) ? curbp : bheadp;
	do {
		bufp->b_flags &= ~BFQSAVE;			// Clear "saved" flag.
		if((bufp->b_flags & BFCHGD) == 0 ||		// Not changed ...
		 ((bufp->b_flags & BFHIDDEN) != 0 && n > 0))	// or hidden and saving all ...
			continue;				// so skip it.
		if(bufp->b_fname == NULL)			// Must have a filename.
			return rcset(FAILURE,0,text145,bufp->b_bname);
				// "No filename associated with buffer '%s'"

		// Complain about truncated files and narrowed buffers.
		if(bufcheck(bufp) != SUCCESS)
			return rc.status;

		// It's a go ... save the buffer.
		if(mlprintf(MLHOME | MLWRAP,text103,bufp->b_fname) != SUCCESS)
				// "Saving %s ..."
			return rc.status;
		if(!edexit && n > 0)
			cpause(50);
		if(writeout(bufp,bufp->b_fname,'w') != SUCCESS)
			return rc.status;
		bufp->b_acount = gasave;			// Reset $autoSave ...
		upmode(bufp);					// update mode line(s) ...
		if(edexit)
			bufp->b_flags |= BFQSAVE;		// mark as saved for main() to display ...
		++count;					// and count it.
		} while(n > 0 && (bufp = bufp->b_nextp) != NULL);

	return (!edexit && n > 0) ? rcset(SUCCESS,RCFORCE,text167,count,count != 1 ? "s" : "") : rc.status;
						// "%d buffer%s saved"
	}

// Write a buffer to disk, given buffer, filename and mode ('w' = write, 'a' = append) and return status.  This function uses
// the file management routines in "fileio.c".  Before the file is written, a user routine (in $writeHook) can be run.  The
// number of lines written, and possibly other status information is displayed.  When $safeSave is true or the 'bak' global mode
// is enabled, the buffer is written out to a temporary file, the old file is unlinked or renamed to "<file>.bak" (if
// "<file>.bak" does not already exist), and the temporary is renamed to the original.
int writeout(Buffer *bufp,char *fn,int mode) {
	Line *lnp;			// Line to scan while writing.
	uint nline;			// Number of lines written.
	int status;			// Return status.
	uint sflag = 0;			// Are we safe saving? (0x01 bit = 'safe' save, 0x02 bit = 'bak' save)
	StrList sl;
	struct stat st;			// File permision info.
	Value *tnamep;			// Temporary filename.
	Value *bknamep;			// Backup filename.

	if(vopen(&sl,NULL,false) != 0)
		return vrcset();

	// Run user-assigned write hook.
	if(!(bufp->b_flags & (BFHIDDEN | BFMACRO)) && exechook(NULL,INT_MIN,hooktab + HKWRITE,2,bufp->b_bname,fn) != SUCCESS)
		return rc.status;

	// Determine if we will use the "safe save" method.
	if((modetab[MDR_GLOBAL].flags & (MDBAK | MDSAFE)) && fexist(fn) >= 0 && mode == 'w') {
		if(modetab[MDR_GLOBAL].flags & MDBAK) {

			// 'bak' mode enabled.  Create backup version of filename.
			if(vnew(&bknamep,false) != 0 || vsalloc(bknamep,strlen(fn) + strlen(BACKUP_EXT) + 1) != 0)
				return vrcset();
			sprintf(bknamep->v_strp,"%s%s",fn,BACKUP_EXT);

			// Enable 'bak' save if backup file does not already exist.
			if(fexist(bknamep->v_strp) < 0)
				sflag = 0x02;
			}
		if(modetab[MDR_GLOBAL].flags & MDSAFE)
			sflag |= 0x01;
		}

	// Open output file.
	if(sflag) {
		char *suffixp;
		size_t len;

		// Duplicate original file pathname and keep first letter of filename.
		len = fbasename(fn,true) - fn + 1;
		if(vnew(&tnamep,false) != 0 || vsalloc(tnamep,len + 6) != 0)
			return vrcset();
		stplcpy(tnamep->v_strp,fn,len + 1);
		suffixp = tnamep->v_strp + len;

		// Create a unique name, using random numbers.
		do {
			long_asc((long) (ernd() & 0xffff),suffixp);
			} while(fexist(tnamep->v_strp) >= 0);

		// Open the temporary file.
		status = ffwopen(tnamep->v_strp,mode);
		}
	else
		status = ffwopen(fn,mode);

	// If the open failed, abort mission.
	if(status != SUCCESS || mlputs(MLHOME | MLWRAP,text148,vz_show) != SUCCESS)
					// "Writing data ..."
		return rc.status;

	// Write the current buffer's lines to the open disk file.
	lnp = lforw(bufp->b_hdrlnp);	// Start at the first line.
	nline = 0U;			// Count lines.
	while(lnp != bufp->b_hdrlnp) {
		if(ffputline(lnp->l_text,lused(lnp)) != SUCCESS) {

			// Write or close error: clean up and get out.
			(void) ffclose(true);
			goto out;
			}
		++nline;
		lnp = lforw(lnp);
		}

	// Write was successful: clear "changed" flag and close output file.
	bufp->b_flags &= ~BFCHGD;
	if((status = ffclose(true)) != SUCCESS) {
out:
		if(sflag)
			(void) unlink(tnamep->v_strp);
		return rc.status;
		}

	// Close was successful: do file manipulations if safe save.
	if(sflag) {
		// Get the permisions of the original file.
		if((status = stat(fn,&st)) != 0)
			status = errno;

		// Rename or erase original file and rename temp to original.
		if(sflag & 0x02) {
			if(rename(fn,bknamep->v_strp) != 0)
				status = -1;
			}
		else if(unlink(fn) != 0)
			status = -1;
		if(status == 0 && rename(tnamep->v_strp,fn) == 0) {
			if(chmod(fn,st.st_mode) != 0 || chown(fn,st.st_uid,st.st_gid) != 0)
				status = errno;
			}

		// Report any errors.
		if(status != 0) {
			if(vputf(&sl,text141,strerror(errno),fn) != 0 || (status == -1 &&
			 (vputc(' ',&sl) != 0 || vputf(&sl,text150,tnamep->v_strp) != 0)))
					// "I/O ERROR: %s, file '%s'","(file saved as '%s') "
				return vrcset();
			status = FAILURE;	// Failed.
			}
		}

	// Report lines written and return status.
	return iostat(&sl,true,status == SUCCESS && (sflag & 0x02) ? bknamep : NULL,status,fn,text149,nline);
					// "Wrote"
	}

// Modify the filename associated with the current buffer.  The operation is simple; just zap the name in the Buffer structure,
// mark the windows as needing an update, and call the read hook.  The new name can be null.
int setBufFile(Value *rp,int n) {

	// We cannot modify an executing buffer.
	if(curbp->b_nexec > 0)
		return rcset(FAILURE,0,text284,text276,text248);
			// "Cannot %s %s buffer","modify","an executing"

	// Get filename.
	if(complete(rp,text151,NULL,CMPL_FILENAME | CMPL_NOAUTO,NPATHINP,0) != SUCCESS)
			// "Change filename to"
		return rc.status;

	// Save new filename (which may be null or nil).
	if(setfname(curbp,vistfn(rp,VNIL) ? NULL : rp->v_strp) != SUCCESS)
		return rc.status;

	// Update mode lines.
	upmode(curbp);

	return (curbp->b_flags & (BFHIDDEN | BFMACRO)) ? rc.status :
	 exechook(NULL,INT_MIN,hooktab + HKREAD,2,curbp->b_bname,defnil(curbp->b_fname));
	}
