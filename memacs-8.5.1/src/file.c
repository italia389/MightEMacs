// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// file.c	Disk file handling for MightEMacs.
//
// These routines handle the reading, writing, and management of disk files.
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
//	findFile command
//		* call getfile()
//	getfile(Datum *rp,int n,bool view)
//		* get a filename
//		* check if file is attached to an existing buffer
//		* if so, either return (do nothing) or switch to buffer
//		* if buffer not found, create new one and (optionally) set buffer to "read-only", read file via readin(),
//		  set filename, and switch to buffer
//		=> CALLED BY execCF() for findFile and viewFile commands.
//	ifile(char *fname)
//		* insert given file into current buffer before dot line
//		=> CALLED BY insertFile(), insertPipe()
//	insertFile()
//		* get a filename and call ifile()
//	rdfile(Datum *rp,int n,char *fname)
//		* create new buffer with name derived from filename if requested
//		* read given file into current buffer or new buffer
//		* create new window if requested, attach new buffer to window, and optionally, switch to new window
//		=> CALLED BY execCF() for readFile command, readPipe()
//	readFile command
//		* get a filename and call rdfile()
//	readin(Buffer *bufp,char *fname,bool keep)
//		* clear given buffer and read a file into it (if fname is NULL, use buffer's filename; otherwise, stdin)
//		* if keep is true, save filename and call read hook; otherwise, delete buffer's filename
//		=> CALLED BY bactivate(), dofile(), rdfile(), pipeBuf()
//	viewFile command
//		* call getfile()

#include "os.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include "std.h"
#include "lang.h"
#include "exec.h"
#include "main.h"

// Make selected global definitions local.
#define DATAfile
#include "file.h"

// Save given filename.
static int savefname(char *fn) {

	// Don't bother if it's already saved.
	if(fn == NULL || fi.fname == NULL || strcmp(fn,fi.fname) != 0) {
		if(fi.fname != NULL)
			(void) free((void *) fi.fname);
		if(fn == NULL)
			fn = "<stdin>";
		if((fi.fname = (char *) malloc(strlen(fn) + 1)) == NULL)
			return rcset(Panic,0,text155,"savefname",fn);
				// "%s(): Out of memory saving filename \"%s\"!"
		strcpy(fi.fname,fn);
		}

	return rc.status;
	}

// Free input line buffer.
static void ffbfree(void) {

	if(fi.lbuf != NULL) {
		(void) free((void *) fi.lbuf);
		fi.lbuf = NULL;
		}
	}

// Open a file for reading.  If fn is NULL, use standard input (via file descriptor saved in stdinfd).  Return status.
int ffropen(char *fn,bool required) {

	if(fn == NULL)
		fi.fd = stdinfd;
	else if((fi.fd = open(fn,O_RDONLY)) == -1)
		return (required || errno != ENOENT) ? rcset(Failure,0,text141,strerror(errno),fn) : IONSF;
							// "I/O ERROR: %s, file \"%s\""

	// Create initial line buffer.
	ffbfree();
	if((fi.lbuf = (char *) malloc(SizeLineBuf)) == NULL)
		return rcset(Panic,0,text94,"ffropen");
				// "%s(): Out of memory!"

	// Set EOF flag, initialize buffer pointers, and save filename.
	fi.eof = fi.fd < 0 ? true : false;
	fi.lbufz = fi.lbuf + SizeLineBuf;
	if((fi.idelim1 = (int) fi.inpdelim[0]) == 0)
		fi.idelim1 = fi.idelim2 = -1;
	else if((fi.idelim2 = (int) fi.inpdelim[1]) == 0)
		fi.idelim2 = -1;
	fi.iobufz = fi.iobufw = fi.iobuf;

	return savefname(fn);
	}

// Open a file for writing or appending.  Return status.
int ffwopen(Buffer *bufp,char *fn,int mode) {

	if((fi.fd = open(fn,O_WRONLY | O_CREAT | (mode == 'w' ? O_TRUNC : O_APPEND),0666)) == -1)
		return rcset(Failure,0,text141,strerror(errno),fn);
				// "I/O ERROR: %s, file \"%s\""

	// Initialize buffer pointers, record delimiter, and save filename.
	fi.iobufz = (fi.iobufw = fi.iobuf) + sizeof(fi.iobuf);
	if(*fi.otpdelim != '\0') {			// Use user-assigned output delimiter if specified.
		fi.odelim = fi.otpdelim;
		fi.odelimlen = fi.otpdelimlen;
		}
	else if(*bufp->b_inpdelim != '\0') {		// Otherwise, use buffer's input delimiter(s).
		fi.odelim = bufp->b_inpdelim;
		fi.odelimlen = bufp->b_inpdelimlen;
		}
	else {						// Default to a newline.
		fi.odelim = "\n";
		fi.odelimlen = 1;
		}
	return savefname(fn);
	}

// Flush I/O buffer to output file.  Return status.
static int ffflush(void) {

	if(write(fi.fd,fi.iobuf,fi.iobufw - fi.iobuf) == -1)
		return rcset(Failure,0,text141,strerror(errno),fi.fname);
				// "I/O ERROR: %s, file \"%s\""
	fi.iobufw = fi.iobuf;
	return rc.status;
	}

// Close current file, reset file information, and note buffer delimiters.
int ffclose(bool otpfile) {

	// Flush output if needed and close file.
	if(otpfile && fi.iobufw > fi.iobuf && ffflush() != Success)
		(void) close(fi.fd);
	else if(fi.fd >= 0 && close(fi.fd) == -1)
		(void) rcset(Failure,0,text141,strerror(errno),fi.fname);
				// "I/O ERROR: %s, file \"%s\""
	fi.fd = -1;

	// Free line and filename buffers.
	ffbfree();
	(void) free((void *) fi.fname);
	fi.fname = NULL;

	// Store delimiter(s) used for I/O in buffer record.
	if(!otpfile) {
		char *str = curbp->b_inpdelim;

		curbp->b_inpdelimlen = 0;
		if(fi.idelim1 != -1) {
			*str++ = (char) fi.idelim1;
			curbp->b_inpdelimlen = 1;
			if(fi.idelim2 != -1) {
				*str++ = (char) fi.idelim2;
				curbp->b_inpdelimlen = 2;
				}
			}
		*str = '\0';
		}

	return rc.status;
	}

// Write bytes to the current (already opened) file with buffering.  Return status.
static int ffwrite(char *buf,int buflen) {

	// Time for buffer flush?
	if(fi.iobufw > fi.iobuf && fi.iobufw + buflen > fi.iobufz) {

		// Yes, flush it.
		if(ffflush() != Success)
			return rc.status;
		}

	// Will string fit in buffer?
	if(fi.iobufw + buflen <= fi.iobufz) {

		// Yes, store it.
		memcpy(fi.iobufw,buf,buflen);
		fi.iobufw += buflen;
		}
	else {
		// Won't fit.  Write it directly.
		if(write(fi.fd,buf,buflen) == -1)
			return rcset(Failure,0,text141,strerror(errno),fi.fname);
					// "I/O ERROR: %s, file \"%s\""
		}

	return rc.status;
	}

// Write given line to the current (already opened) file.  Return status.
int ffputline(char *buf,int buflen) {

	// Write the line and appropriate line terminator(s), and check for errors.
	if(ffwrite(buf,buflen) == Success)
		(void) ffwrite(fi.odelim,fi.odelimlen);

	return rc.status;
	}

// Add a character to fi.lbuf, expanding it as needed.  Return status.
static int ffputc(int c) {
	uint n1,n2;

	// Line buffer full?
	if(fi.lbufw == fi.lbufz) {

		// Yes, get more space.
		n1 = n2 = fi.lbufw - fi.lbuf;
		if((fi.lbuf = (char *) realloc((void *) fi.lbuf,n2 *= 2)) == NULL)
			return rcset(Panic,0,text156,"ffputc",n2,fi.fname);
				// "%s(): Out of memory allocating %u-byte line buffer for file \"%s\"!"
		fi.lbufz = (fi.lbufw = fi.lbuf + n1) + n1;
		}

	// Append character and return rc.status;
	*fi.lbufw++ = c;
	return rc.status;
	}

// Get next character from (opened) input file and store in *str.  Return status, including IOEOF if end of file reached.
static int ffgetc(int *str) {
	int n;

	// At EOF?
	if(fi.eof)
		goto EOFRetn;

	// Buffer empty?
	if(fi.iobufw == fi.iobufz) {

		// Yep, refill it.
		if((n = read(fi.fd,fi.iobuf,SizeFileBuf)) == -1)
			return rcset(Failure,0,text141,strerror(errno),fi.fname);
					// "I/O ERROR: %s, file \"%s\""
		// Hit EOF?
		if(n == 0) {

			// Yep, note it...
			fi.eof = true;
EOFRetn:
			// and return it.
			*str = -1;
			return IOEOF;
			}

		// No, update buffer pointers.
		fi.iobufz = (fi.iobufw = fi.iobuf) + n;
		}

	*str = (int) *fi.iobufw++;
	return rc.status;
	}

// Read a line from a file into fi.lbuf and store the byte count in the supplied uint pointer.  Check for I/O errors and
// return status.
int ffgetline(uint *lenp) {
	int status;
	int c;		// Current character read.
	uint llen;	// Line length.

	// If we are at EOF, bail out.
	if(fi.eof)
		return IOEOF;

	// Initialize line pointer.
	fi.lbufw = fi.lbuf;

	// Input line delimiters undefined?
	if(fi.idelim1 == -1) {

		// Yes.  Read bytes until a NL or CR found, or EOF reached.
		for(;;) {
			if((status = ffgetc(&c)) == IOEOF)
				goto EOLn;
			if(status != Success)
				return rc.status;
			if(c == '\n') {
				fi.idelim1 = '\n';
				fi.idelim2 = -1;
				goto EOLn;
				}
			if(c == '\r') {
				fi.idelim1 = '\r';
				if((status = ffgetc(&c)) == IOEOF)
					goto EOLn;
				if(status != Success)
					return rc.status;
				if(c == '\n') {
					fi.idelim2 = '\n';
					goto EOLn;
					}

				// Not a CR-LF sequence; "unget" the last character.
				--fi.iobufw;
				fi.idelim2 = -1;
				goto EOLn;
				}

			// No NL or CR found yet... onward.
			if(ffputc(c) != Success)
				return rc.status;
			}
		}

	// We have line delimiter(s)... read next line.
	for(;;) {
		if((status = ffgetc(&c)) == IOEOF)
			break;
		if(status != Success)
			return rc.status;
		if(c == fi.idelim1) {

			// Got match on first delimiter.  Need two?
			if(fi.idelim2 == -1)

				// Nope, we're done.
				break;

			// Yes, check next character.
			if((status = ffgetc(&c)) == IOEOF)
				break;
			if(status != Success)
				return rc.status;
			if(c == fi.idelim2)

				// Second delimiter matches also, we're done.
				break;

			// Not a match; "unget" the last character.
			--fi.iobufw;
			c = fi.idelim1;
			}

		// Delimiter not found, or two delimiters needed, but only one found.  Save the character and move onward.
		if(ffputc(c) != Success)
			return rc.status;
		}
EOLn:
	// Hit EOF and nothing read?
	if((llen = fi.lbufw - fi.lbuf) == 0U && fi.eof)

		// Yes.
		return IOEOF;

	// Got a (possibly zero-length) line.  Save the length for the caller.
	*lenp = llen;

	return rc.status;
	}

// Does "fn" exist on disk?  Return -1 (false), 0 (true and not a directory), or 1 (true and is a directory).
int fexist(char *fn) {
	struct stat s;

	return stat(fn,&s) != 0 ? -1 : (s.st_mode & S_IFDIR) ? 1 : 0;
	}

// Report I/O results as a return message, given active string-fab object.  Return status.
static int iostat(DStrFab *sfp,bool otpfile,Datum *bknamep,int status,char *fname,char *lcmsg,uint nline) {
	char *str;

	// Report any non-fatal read or close error.
	if(!otpfile && status < Success) {
		if(dputf(sfp,text141,strerror(errno),fname) != 0 || dputc(' ',sfp) != 0)
				// "I/O ERROR: %s, file \"%s\""
			return drcset();
		curbp->b_flags |= BFActive;		// Don't try to read file again.
		}

	// Begin wrapped message and report I/O operation.
	if(dputc('[',sfp) != 0 || dputs(lcmsg,sfp) != 0)
		return drcset();

	// Report line count.
	if(dputf(sfp," %u %s%s",nline,text205,nline != 1U ? "s" : "") != 0)
				// "line"
		return drcset();

	// Non-standard line delimiter(s)?
	str = otpfile ? fi.odelim : curbp->b_inpdelim;
	if(str[0] != '\0' && (str[0] != '\n' || str[1] != '\0')) {
		if(dputs(text252,sfp) != 0 || dvizs(str,0,VBaseDef,sfp) != 0)
				// " delimited by "
			return drcset();
		}

	// Insert?
	if(lcmsg == text154 && dputs(text355,sfp) != 0)
			// " and marked as region"
		return drcset();

	// Backup file created on output?
	if(bknamep != NULL) {
		if(dputs(text257,sfp) != 0 || dputs(fbasename(bknamep->d_str,true),sfp) != 0 || dputc('"',sfp) != 0)
				// ", original file renamed to \""
			return drcset();
		}

	if(dputc(']',sfp) != 0 || dclose(sfp,sf_string) != 0)
		return drcset();
	mlerase(0);
	return rcset(status >= Success ? Success : status,RCNoFormat | RCNoWrap,sfp->sf_datp->d_str);
	}

// Prompt for a filename (which cannot be null).
int gtfilename(Datum *rp,char *prmt,uint cflags) {

	// Get a filename; default to current buffer's.
	return getarg(rp,prmt,curbp->b_fname,RtnKey,MaxPathname,Arg_First | CFNotNull1,Term_C_Filename | cflags);
	}

// Read given file into a buffer.  If default n, use the current buffer.  If not default n, create a buffer and either (1), use
// a scratch name for the buffer and don't save the filename in the buffer header if "scratch" is true; or (2), derive the
// buffer name from the filename and save the filename in the buffer header if "scratch" is false.  Render buffer and return
// status.
int rdfile(Datum *rp,int n,char *fname,bool scratch) {
	Buffer *bufp;

	if(n == INT_MIN)
		bufp = curbp;							// Use current buffer by default.
	else {									// Otherwise...
		if(scratch) {							// if scratch buffer requested...
			char bname[NBufName + 1];

			if(bscratch(bname,&bufp) != Success)			// get one...
				return rc.status;
			}							// otherwise, create a buffer.
		else if(bfind(fname,CRBCreate | CRBFile | CRBForce,0,&bufp,NULL) != Success)
			return rc.status;
		}
	if(readin(bufp,fname,!scratch) != Success ||				// Read the file and rename current buffer,
	 (n == INT_MIN && !scratch && setBufName(rp,1,NULL) != Success))	// if applicable.
		return rc.status;

	return render(rp,n == INT_MIN ? 1 : n >= 0 ? n : -2,bufp,		// Render the buffer.
	 n != INT_MIN && n < -1 ? RendAltML : 0);
	}

// Insert file "fname" into the current buffer and set current region to inserted lines.  If n == 0, leave point before the
// inserted lines; otherwise, after.  Return the final status of the read.
int ifile(char *fname,int n) {
	int status;
	Line *lnp0,*lnp1,*lnp2;
	uint nline;
	int nbytes;
	DStrFab sf;
	WindFace *wfp = &curwp->w_face;

	if(dopentrk(&sf) != 0)
		return drcset();

	// Open the file.
	if(ffropen(fname,true) != Success)
		goto Retn;

	// Let user know what's up.
	if(mlputs(MLHome | MLWrap,text153) != Success)
			// "Inserting data..."
		return rc.status;

	// Prepare buffer.
	curbp->b_flags |= BFChgd;			// Mark as changed.

	// Back up a line and save dot in mark RMark.
	curbp->b_mroot.mk_force = getwpos(curwp);	// Keep existing framing.
	wfp->wf_dot.lnp = lback(wfp->wf_dot.lnp);
	wfp->wf_dot.off = 0;
	curbp->b_mroot.mk_dot = wfp->wf_dot;

	// Read the file into the buffer at point.
	nline = 0;
	while((status = ffgetline(&nbytes)) == Success) {
		if(lalloc(nbytes,&lnp1) != Success)
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
		memcpy(lnp1->l_text,fi.lbuf,nbytes);
		++nline;
		}

	// Last read was unsuccessful, check for errors.
	if(status <= FatalError)
		return rc.status;			// Bail out.
	(void) ffclose(false);				// Ignore any error (for now).

	// Adjust mark RMark to point to first inserted line (if any).
	curbp->b_mroot.mk_dot.lnp = lforw(curbp->b_mroot.mk_dot.lnp);

	// Report results.
	if(iostat(&sf,false,NULL,status,fname,text154,nline) != Success)
					// "Inserted"
		return rc.status;
Retn:
	// Advance dot to the next line (the end of the inserted text).
	wfp->wf_dot.lnp = lforw(wfp->wf_dot.lnp);

	// If n is zero, swap point and mark RMark.
	if(n == 0)
		(void) swapmid(RMark);

	lchange(curbp,WFHard | WFMode);
	return rc.status;
	}

// Insert a file into the current buffer.  If zero argument, leave point before the inserted lines; otherwise, after.  This
// is really easy; all you do it find the name of the file, and call the standard "insert a file into the current buffer" code.
int insertFile(Datum *rp,int n,Datum **argpp) {

	if(gtfilename(rp,text132,0) != Success || rp->d_type == dat_nil)
			// "Insert file"
		return rc.status;

	// Insert the file.
	return ifile(rp->d_str,n);
	}

// Find a file and optionally, read it into a buffer.  If n == 0, just create a buffer for the file if one does not already
// exist (and stay in current buffer).  If n != 0 and the file is not currently attached to a buffer, create a new buffer and
// set to read-only if "view" is true.  Render buffer and return status.
int getfile(Datum *rp,int n,bool view) {
	Buffer *bufp;
	bool created = false;

	// Get filename.
	if(gtfilename(rp,view ? text134 : text133,view || n != 1 ? 0 : Term_C_NoAuto) != Success ||
	 (!(opflags & OpScript) && rp->d_type == dat_nil))
			// "View file","Find file"
		return rc.status;

	// Check if an existing visible buffer is attached to the file.
	bufp = bheadp;
	do {
		if(!(bufp->b_flags & BFHidden) && bufp->b_fname != NULL && strcmp(bufp->b_fname,rp->d_str) == 0) {

			// Visible buffer found.  Return if not reading; otherwise, switch to the buffer.
			if((n == INT_MIN || n > 0) && mlputs(MLHome | MLWrap,text135) != Success)
									// "Old buffer"
				return rc.status;
			goto ViewChk;
			}
		} while((bufp = bufp->b_nextp) != NULL);

	// No buffer found... create one.
	if(bfind(rp->d_str,CRBCreate | CRBFile | CRBForce,0,&bufp,&created) != Success)
		return rc.status;

	// Set filename and flag as inactive.
	if(setfname(bufp,rp->d_str) != Success)
		return rc.status;
	bufp->b_flags = 0;
ViewChk:
	// Put buffer in read-only mode if view is true.
	if(view)
		bufp->b_modes |= MdRdOnly;

	return render(rp,n == INT_MIN ? 1 : n >= 0 ? n : created ? -2 : -1,bufp,
	 RendBool | (created ? RendTrue : 0) | (n != INT_MIN && n < -1 ? RendAltML : 0));
	}

// Read a file into the given buffer, blowing away any text found there.  If fname is NULL, use the buffer's filename if set;
// otherwise, standard input.  If "keep" is true, save the filename in the buffer record (if it is not narrowed) and call the
// read hook after the file is read; otherwise, delete the buffer's filename if set.  Return status, including Cancelled if
// buffer was not cleared (user did not "okay" it).
int readin(Buffer *bufp,char *fname,bool keep) {
	Line *lnp1,*lnp2;
	EWindow *winp;
	int status,nbytes;
	uint nline;
	DStrFab sf;

	if(dopentrk(&sf) != 0)
		return drcset();
	if(bclear(bufp,0) != Success)
		return rc.status;				// Error or user got cold feet.
	if(bufp->b_flags & BFNarrow)				// Mark as changed if narrowed.
		bufp->b_flags |= BFChgd;
	bufp->b_modes |= modetab[MdRec_Default].flags;		// Add current default buffer modes.

	// Determine filename.
	if(fname == NULL)
		fname = bufp->b_fname;
	else if(keep && !(bufp->b_flags & BFNarrow) &&
	 setfname(bufp,fname) != Success)			// Save given filename.
		return rc.status;

	// Open the file.
	if((status = ffropen(fname,false)) <= FatalError)
		return rc.status;				// Bail out.
	if(status != Success)
		goto Retn;

	// Let user know what's up.
	if(mlputs(MLHome | MLWrap,text139) != Success)
			// "Reading data..."
		return rc.status;

	// Read it in.
	nline = 0U;
	while((status = ffgetline(&nbytes)) == Success) {
		if(lalloc(nbytes,&lnp1) != Success)
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
	if(status <= FatalError)
		return rc.status;				// Bail out.
	if(status >= Success)
		status = ffclose(false);			// Read was successful; preserve close status.
	else
		(void) ffclose(false);				// Read error; ignore close status.

	// Report results.
	if(iostat(&sf,false,NULL,status,fname,text140,nline) != Success)
					// "Read"
		return rc.status;
Retn:
	// Erase filename or run user-assigned read hook.
	if(!keep)
		clfname(bufp);
	else if(status >= Success && !(bufp->b_flags & (BFHidden | BFMacro)) &&
	 exechook(NULL,INT_MIN,hooktab + HkRead,2,bufp->b_bname,bufp->b_fname) <= FatalError)
		return rc.status;

	// Make sure buffer is active.
	bufp->b_flags |= BFActive;

	// Update buffer and window pointers.
	faceinit(&bufp->b_face,lforw(bufp->b_hdrlnp),bufp);
	winp = wheadp;
	do {
		if(winp->w_bufp == bufp) {
			faceinit(&winp->w_face,lforw(bufp->b_hdrlnp),NULL);
			winp->w_flags |= WFMode | WFHard;
			}
		} while((winp = winp->w_nextp) != NULL);

	// Return status.
	return (status == IONSF) ? rcset(Success,RCNoFormat,text138) : rc.status;
					// "New file"
	}

// Check given buffer flags and verify with user before writing to a file.  Return status.
static int bufcheck(Buffer *bufp) {
	bool yep;
	DStrFab prompt;

	// Complain about narrowed buffer.
	if(bufp->b_flags & BFNarrow) {
		if(dopentrk(&prompt) != 0 ||
		 dputf(&prompt,text147,bufp->b_bname) != 0 || dclose(&prompt,sf_string) != 0)
				// "Narrowed buffer '%s'... write it out"
			return drcset();
		if(mlyesno(prompt.sf_datp->d_str,&yep) != Success)
			return rc.status;
		if(!yep)
			return rcset(UserAbort,0,NULL);
		}

	return rc.status;
	}

// Ask for a filename, and write the contents of the current buffer to that file.  Update the remembered filename and clear
// the buffer changed flag.  Return status.
int fileout(Datum *rp,int n,char *prmt,int mode) {

	// Get the filename.
	if(gtfilename(rp,prmt,Term_C_NoAuto) != Success || (!(opflags & OpScript) && rp->d_type == dat_nil))
		return rc.status;

	// Complain about truncated files and narrowed buffers.
	if(bufcheck(curbp) != Success)
		return rc.status;

	// It's a go... write the file.
	if(writeout(curbp,rp->d_str,mode) != Success)
		return rc.status;

	// Update filename and buffer name if write (not append).
	if(mode == 'w') {
		Datum *dsinkp;

		if(dnewtrk(&dsinkp) != 0)
			return drcset();
		if(setfname(curbp,rp->d_str) != Success || ((n == INT_MIN || n > 0) && setBufName(dsinkp,1,NULL) != Success))
			return rc.status;
		}

	// Update mode line(s).
	upmode(curbp);

	return rc.status;
	}

// Save the contents of the current buffer (or all buffers if n > 0) to their associated files.  Do nothing if nothing has
// changed.  Error if there is no remembered filename for a buffer.
int savebufs(int n,bool edexit) {
	Buffer *bufp;
	uint count = 0;

	bufp = (n <= 0) ? curbp : bheadp;
	do {
		bufp->b_flags &= ~BFQSave;			// Clear "saved" flag.
		if(!(bufp->b_flags & BFChgd) ||			// Not changed...
		 ((bufp->b_flags & BFHidden) != 0 && n > 0))	// or hidden and saving all...
			continue;				// so skip it.
		if(bufp->b_fname == NULL)			// Must have a filename.
			return rcset(Failure,0,text145,bufp->b_bname);
				// "No filename associated with buffer '%s'"

		// Complain about truncated files and narrowed buffers.
		if(bufcheck(bufp) != Success)
			return rc.status;

		// It's a go... save the buffer.
		if(mlprintf(MLHome | MLWrap,text103,bufp->b_fname) != Success)
				// "Saving %s..."
			return rc.status;
		if(!edexit && n > 0)
			cpause(50);
		if(writeout(bufp,bufp->b_fname,'w') != Success)
			return rc.status;
		gacount = gasave;				// Reset $autoSave...
		upmode(bufp);					// update mode line(s)...
		if(edexit)
			bufp->b_flags |= BFQSave;		// mark as saved for main() to display...
		++count;					// and count it.
		} while(n > 0 && (bufp = bufp->b_nextp) != NULL);

	return (!edexit && n > 0) ? rcset(Success,RCForce,text167,count,count != 1 ? "s" : "") : rc.status;
						// "%u buffer%s saved"
	}

// Write a buffer to disk, given buffer, filename and mode ('w' = write, 'a' = append) and return status.  Before the file is
// written, a user routine (write hook) can be run.  The number of lines written, and possibly other status information is
// displayed.  If the 'safe' or 'bak' global mode is enabled, the buffer is written out to a temporary file, the old file is
// unlinked or renamed to "<file>.bak" (if "<file>.bak" does not already exist), and the temporary is renamed to the original.
int writeout(Buffer *bufp,char *fn,int mode) {
	Line *lnp;			// Line to scan while writing.
	uint nline;			// Number of lines written.
	int status;			// Return status.
	uint sflag = 0;			// Are we safe saving? (0x01 bit = 'safe' save, 0x02 bit = 'bak' save)
	DStrFab sf;
	struct stat st;			// File permision info.
	Datum *tnamep;			// Temporary filename.
	Datum *bknamep;			// Backup filename.

	if(dopentrk(&sf) != 0)
		return drcset();

	// Run user-assigned write hook.
	if(!(bufp->b_flags & (BFHidden | BFMacro)) && exechook(NULL,INT_MIN,hooktab + HkWrite,2,bufp->b_bname,fn) != Success)
		return rc.status;

	// Determine if we will use the "safe save" method.
	if((modetab[MdRec_Global].flags & (MdBak | MdSafe)) && fexist(fn) >= 0 && mode == 'w') {
		if(modetab[MdRec_Global].flags & MdBak) {

			// 'bak' mode enabled.  Create backup version of filename.
			if(dnewtrk(&bknamep) != 0 || dsalloc(bknamep,strlen(fn) + strlen(Backup_Ext) + 1) != 0)
				return drcset();
			sprintf(bknamep->d_str,"%s%s",fn,Backup_Ext);

			// Enable 'bak' save if backup file does not already exist.
			if(fexist(bknamep->d_str) < 0)
				sflag = 0x02;
			}
		if(modetab[MdRec_Global].flags & MdSafe)
			sflag |= 0x01;
		}

	// Open output file.
	if(sflag) {
		char *suffix;
		size_t len;

		// Duplicate original file pathname and keep first letter of filename.
		len = fbasename(fn,true) - fn + 1;
		if(dnewtrk(&tnamep) != 0 || dsalloc(tnamep,len + 6) != 0)
			return drcset();
		stplcpy(tnamep->d_str,fn,len + 1);
		suffix = tnamep->d_str + len;

		// Create a unique name, using random numbers.
		do {
			long_asc(xorshift64star(0xFFFF) - 1,suffix);
			} while(fexist(tnamep->d_str) >= 0);

		// Open the temporary file.
		status = ffwopen(bufp,tnamep->d_str,mode);
		}
	else
		status = ffwopen(bufp,fn,mode);

	// If the open failed, abort mission.
	if(status != Success || mlputs(MLHome | MLWrap,text148) != Success)
					// "Writing data..."
		return rc.status;

	// Write the current buffer's lines to the open disk file.
	lnp = lforw(bufp->b_hdrlnp);	// Start at the first line.
	nline = 0U;			// Count lines.
	while(lnp != bufp->b_hdrlnp) {
		if(ffputline(lnp->l_text,lused(lnp)) != Success) {

			// Write or close error: clean up and get out.
			(void) ffclose(true);
			goto Retn;
			}
		++nline;
		lnp = lforw(lnp);
		}

	// Write was successful: clear "changed" flag and close output file.
	bufp->b_flags &= ~BFChgd;
	if((status = ffclose(true)) != Success) {
Retn:
		if(sflag)
			(void) unlink(tnamep->d_str);
		return rc.status;
		}

	// Close was successful: do file manipulations if safe save.
	if(sflag) {
		// Get the permisions of the original file.
		if((status = stat(fn,&st)) != 0)
			status = errno;

		// Rename or erase original file and rename temp to original.
		if(sflag & 0x02) {
			if(rename(fn,bknamep->d_str) != 0)
				status = -1;
			}
		else if(unlink(fn) != 0)
			status = -1;
		if(status == 0 && rename(tnamep->d_str,fn) == 0) {
			if(chmod(fn,st.st_mode) != 0 || chown(fn,st.st_uid,st.st_gid) != 0)
				status = errno;
			}

		// Report any errors.
		if(status != 0) {
			if(dputf(&sf,text141,strerror(errno),fn) != 0 || (status == -1 &&
			 (dputc(' ',&sf) != 0 || dputf(&sf,text150,tnamep->d_str) != 0)))
					// "I/O ERROR: %s, file \"%s\"","(file saved as \"%s\") "
				return drcset();
			status = Failure;	// Failed.
			}
		}

	// Report lines written and return status.
	return iostat(&sf,true,status == Success && (sflag & 0x02) ? bknamep : NULL,status,fn,text149,nline);
					// "Wrote"
	}

// Modify the filename associated with the current buffer and call the read hook if appropriate.  If the new name is nil or
// null, set the filename to null; otherwise, if n > 0 or is the default, change the buffer name to match.  Return status.
int setBufFile(Datum *rp,int n,Datum **argpp) {

	// We cannot modify an executing buffer.
	if((opflags & OpEval) && curbp->b_mip != NULL && curbp->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text284,text276,text248);
			// "Cannot %s %s buffer","modify","an executing"

	// Get filename.
	if(opflags & OpScript)
		datxfer(rp,argpp[0]);
	else if(terminp(rp,text151,NULL,RtnKey,MaxPathname,0,Term_C_Filename | Term_C_NoAuto) != Success)
			// "Change filename to"
		return rc.status;

	// Save new filename (which may be null or nil).
	if(rp->d_type == dat_nil)
		dsetnull(rp);
	if(setfname(curbp,rp->d_str) != Success)
		return rc.status;

	// Change buffer name.
	if(curbp->b_fname != NULL && (n == INT_MIN || n > 0)) {
		Datum *dsinkp;

		if(dnewtrk(&dsinkp) != 0)
			return drcset();
		if(setBufName(dsinkp,1,NULL) != Success)
			return rc.status;
		}

	// Update mode lines.
	upmode(curbp);

	return (curbp->b_flags & (BFHidden | BFMacro)) ? rc.status :
	 exechook(NULL,INT_MIN,hooktab + HkRead,2,curbp->b_bname,curbp->b_fname);
	}
