// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// file.c	Disk file handling for MightEMacs.
//
// These routines handle the reading, writing, and management of disk files.
//
// I/O COMMANDS AND C FUNCTIONS
// Input:
//	execFile, findFile, insertFile, insertPipe, popFile, readFile, readPipe, viewFile, xeqFile
//	fvfile(), ifile(), readFP(), readin()
// Output:
//	appendFile, saveFile, writeFile
//	awfile(), savebuf(), savebufs(), writeout()
// Filename management:
//	setBufFile
//	gtfilename(), setfname()
//
// CORE INPUT FUNCTIONS
//	findFile command
//		* call fvfile()
//	fvfile(Datum *rval,int n,bool view)
//		* get a filename
//		* check if file is attached to an existing buffer
//		* if so, either return (do nothing) or switch to buffer
//		* if buffer not found, create one, set filename, (optionally) set buffer to "read-only", and call render()
//		=> CALLED BY execCF() for findFile and viewFile commands.
//	ifile(char *fname,int n)
//		* insert given file into current buffer before current line, optionally leaving point before inserted text
//		=> CALLED BY insertFile(), insertPipe()
//	insertFile() -- command routine
//		* get a filename and call ifile()
//	readFP(Datum *rval,int n,char *fname,ushort flags)
//		* create new buffer with name derived from filename if requested
//		* read given file into current buffer or new buffer
//		* call render()
//		=> CALLED BY execCF() for readFile command, readPipe()
//	readFile command
//		* get a filename and call readFP()
//	readin(Buffer *buf,char *fname,ushort flags)
//		* clear given buffer and read a file into it (if fname is NULL, use buffer's filename; otherwise, stdin)
//		* if RWKeep flag is set, save filename and call read hook; otherwise, delete buffer's filename
//		* if RWExist flag is set, return error if file does not exist
//		=> CALLED BY bactivate(), execfile(), dopop(), readFP()
//	viewFile command
//		* call fvfile()

#include "os.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <glob.h>
#include "std.h"
#include "cmd.h"
#include "exec.h"

// Make selected global definitions local.
#define DATAfile
#include "file.h"

// Copy glob() results to an array.  Return status.
static int globcpy(Datum *rval,glob_t *gp) {
	Array *ary;

	if(mkarray(rval,&ary) != Success)
		return rc.status;
	if(gp->gl_pathc > 0) {
		Datum dat;
		char **pathp = gp->gl_pathv;
		dinit(&dat);
		do {
			if(dsetstr(*pathp,&dat) != 0 || apush(ary,&dat,true) != 0)
				return librcset(Failure);
			} while(*++pathp != NULL);
		dclear(&dat);
		globfree(gp);
		}
	return rc.status;
	}

// Do "glob" function: return a (possibly empty) array of file(s) and/or director(ies) that match given shell glob pattern.
// Return status.
int globpat(Datum *rval,int n,Datum **argv) {

	if(disnull(*argv)) {
		Array *ary;
		(void) mkarray(rval,&ary);
		}
	else {
		int r;
		glob_t glb;
		char *pat = argv[0]->d_str;

		glb.gl_pathc = 0;
		if((r = glob(pat,GLOB_MARK,NULL,&glb)) != 0 && r != GLOB_NOMATCH) {
			if(glb.gl_pathc > 0)
				globfree(&glb);
			(void) rcset(errno == GLOB_NOSPACE ? FatalError : Failure,0,text406,pat,strerror(errno));
								// "Glob of pattern \"%s\" failed: %s"
			}
		else
			(void) globcpy(rval,&glb);
		}

	return rc.status;
	}

// Do "pathname" function: return absolute pathname of given file or directory (or an array of absolute pathnames if argument is
// an array).  If n <= 0, symbolic links are not resolved.  Return status.
int aPathname(Datum *rval,int n,Datum **argv) {

	if(argv[0]->d_type != dat_blobRef)
		(void) getpath(argv[0]->d_str,n == INT_MIN || n > 0,rval);
	else {
		Array *ary0,*ary;
		Datum *el,*path,*epath;

		if(mkarray(rval,&ary0) == Success) {
			if(dnewtrk(&path) != 0 || dnewtrk(&epath) != 0)
				goto LibFail;
			ary = awptr(*argv)->aw_ary;
			while((el = aeach(&ary)) != NULL) {
				if(!(el->d_type & DStrMask))
					return rcset(Failure,0,text329,dtype(el,false));
						// "Unexpected %s argument"
				if(expath(epath,el) != Success || getpath(epath->d_str,n == INT_MIN || n > 0,path) != Success)
					break;
				if(apush(ary0,path,true) != 0)
LibFail:
					return librcset(Failure);
				}
			}
		}
	return rc.status;
	}

// Do xPathname function.  Return status.
int xPathname(Datum *rval,int n,Datum **argv) {
	char *myname = (cftab + cf_xPathname)->cf_name;
	static bool doGlob;
	static bool skipNull;
	static Option options[] = {
		{"^Glob",NULL,0,.u.ptr = (void *) &doGlob},
		{"^SkipNull",NULL,0,.u.ptr = (void *) &skipNull},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		0,text451,false,options};
			// "function option"

	// Get options and set flags.
	initBoolOpts(options);
	if(n != INT_MIN) {
		if(parseopts(&ohdr,NULL,argv[1],NULL) != Success)
			return rc.status;
		setBoolOpts(options);
		}

	if(doGlob) {
		glob_t glb;

		if(pathsearch(argv[0]->d_str,skipNull ? (XPGlobPat | XPSkipNull) : XPGlobPat,(void *) &glb,myname) == Success)
			(void) globcpy(rval,&glb);
		}
	else {
		char *result;

		if(pathsearch(argv[0]->d_str,skipNull ? XPSkipNull : 0,(void *) &result,myname) == Success) {
			if(result == NULL)
				dsetnil(rval);
			else if(dsetstr(result,rval) != 0)
				(void) librcset(Failure);
			}
		}
	return rc.status;
	}

// Save given filename.
static void savefname(char *fn) {

	fi.fname = (fn == NULL) ? "<stdin>" : fn;
	}

// Free input line buffer.
static void ffbfree(void) {

	if(fi.lbuf != NULL) {
		(void) free((void *) fi.lbuf);
		fi.lbuf = NULL;
		}
	}

// Initialize FileInfo object for input.  Return status.
int rinit(int fd) {

	fi.fd = fd;

	// Create initial line buffer.
	ffbfree();
	if((fi.lbuf = (char *) malloc(FISizeLineBuf)) == NULL)
		return rcset(Panic,0,text94,"ffropen");
				// "%s(): Out of memory!"

	// Set EOF flag and initialize buffer pointers.
	fi.flags = fi.fd < 0 ? FIEOF : 0;
	fi.lbufz = fi.lbuf + FISizeLineBuf;
	if((fi.idelim1 = (int) fi.inpdelim[0]) == 0)
		fi.idelim1 = fi.idelim2 = -1;
	else if((fi.idelim2 = (int) fi.inpdelim[1]) == 0)
		fi.idelim2 = -1;
	fi.iobufz = fi.iobufc = fi.iobuf;

	return rc.status;
	}

// Open a file for reading.  If fn is NULL, use standard input (via file descriptor saved in fi.stdinfd).  Return status.
static int ffropen(char *fn,bool required) {
	int fd;

	if(fn == NULL)
		fd = fi.stdinfd;
	else if((fd = open(fn,O_RDONLY)) == -1)
		return (required || errno != ENOENT) ? rcset(Failure,0,text141,strerror(errno),fn) : IONSF;
							// "I/O Error: %s, file \"%s\""
	savefname(fn);
	return rinit(fd);
	}

// Initialize FileInfo object for output.
void winit(Buffer *buf,int fd) {
	fi.fd = fd;

	// Initialize buffer pointers and record delimiter.
	fi.iobufz = (fi.iobufc = fi.iobuf) + sizeof(fi.iobuf);
	if(*fi.otpdelim != '\0') {			// Use user-assigned output delimiter if specified.
		fi.odelim = fi.otpdelim;
		fi.odelimlen = fi.otpdelimlen;
		}
	else if(*buf->b_inpdelim != '\0') {		// Otherwise, use buffer's input delimiter(s).
		fi.odelim = buf->b_inpdelim;
		fi.odelimlen = buf->b_inpdelimlen;
		}
	else {						// Default to a newline.
		fi.odelim = "\n";
		fi.odelimlen = 1;
		}
	}

// Open a file for writing or appending.  Return status.
static int ffwopen(Buffer *buf,char *fn,short mode) {
	int fd;

	if((fd = open(fn,O_WRONLY | O_CREAT | (mode == 'w' ? O_TRUNC : O_APPEND),0666)) == -1)
		return rcset(Failure,0,text141,strerror(errno),fn);
				// "I/O Error: %s, file \"%s\""
	savefname(fn);
	winit(buf,fd);
	return rc.status;
	}

// Flush I/O buffer to output file.  Return status.
static int ffflush(void) {

	if(write(fi.fd,fi.iobuf,fi.iobufc - fi.iobuf) == -1)
		return rcset(Failure,0,text141,strerror(errno),fi.fname);
				// "I/O Error: %s, file \"%s\""
	fi.iobufc = fi.iobuf;
	return rc.status;
	}

// Close current file, reset file information, and note buffer delimiters.
static int ffclose(bool otpfile) {

	// Flush output if needed and close file.
	if(otpfile && fi.iobufc > fi.iobuf && ffflush() != Success)
		(void) close(fi.fd);
	else if(fi.fd >= 0 && close(fi.fd) == -1)
		(void) rcset(Failure,0,text141,strerror(errno),fi.fname);
				// "I/O Error: %s, file \"%s\""

	// Free line buffer and reset controls.
	ffbfree();
	fi.fd = -1;
	fi.fname = NULL;

	// Store delimiter(s) used for I/O in buffer record.
	if(!otpfile) {
		char *str = si.curbuf->b_inpdelim;

		si.curbuf->b_inpdelimlen = 0;
		if(fi.idelim1 != -1) {
			*str++ = (char) fi.idelim1;
			si.curbuf->b_inpdelimlen = 1;
			if(fi.idelim2 != -1) {
				*str++ = (char) fi.idelim2;
				si.curbuf->b_inpdelimlen = 2;
				}
			}
		*str = '\0';
		}

	return rc.status;
	}

// Write bytes to the current (already opened) file with buffering.  Return status.
static int ffwrite(char *buf,int buflen) {

	// Time for buffer flush?
	if(fi.iobufc > fi.iobuf && fi.iobufc + buflen > fi.iobufz) {

		// Yes, flush it.
		if(ffflush() != Success)
			return rc.status;
		}

	// Will string fit in buffer?
	if(fi.iobufc + buflen <= fi.iobufz) {

		// Yes, store it.
		memcpy(fi.iobufc,buf,buflen);
		fi.iobufc += buflen;
		}
	else {
		// Won't fit.  Write it directly.
		if(write(fi.fd,buf,buflen) == -1)
			return rcset(Failure,0,text141,strerror(errno),fi.fname);
					// "I/O Error: %s, file \"%s\""
		}

	return rc.status;
	}

// Write given line to the current (already opened) file.  Return status.
static int ffputline(Line *lnp,bool writeDelim) {

	// Write the line and appropriate line delimiter(s), and check for errors.
	if(ffwrite(lnp->l_text,lnp->l_used) == Success && writeDelim)
		(void) ffwrite(fi.odelim,fi.odelimlen);

	return rc.status;
	}

// Add a character to fi.lbuf, expanding it as needed.  Return status.
static int ffputc(short c) {
	uint n1,n2;

	// Line buffer full?
	if(fi.lbufc == fi.lbufz) {

		// Yes, get more space.
		n1 = n2 = fi.lbufc - fi.lbuf;
		if((fi.lbuf = (char *) realloc((void *) fi.lbuf,n2 *= 2)) == NULL)
			return rcset(Panic,0,text156,"ffputc",n2,fi.fname);
				// "%s(): Out of memory allocating %u-byte line buffer for file \"%s\"!"
		fi.lbufz = (fi.lbufc = fi.lbuf + n1) + n1;
		}

	// Append character and return rc.status;
	*fi.lbufc++ = c;
	return rc.status;
	}

// Get next character from (opened) input file and store in *cp.  Return status, including IOEOF if end of file reached.
static int ffgetc(short *cp) {
	int n;

	// At EOF?
	if(fi.flags & FIEOF)
		goto EOFRetn;

	// Buffer empty?
	if(fi.iobufc == fi.iobufz) {
		int loopct = (fi.flags & FIUseTimeout) ? FIReadAttempts : 1;

		// Yep, refill it.  Use timeout if applicable.
		for(;;) {
			if((n = read(fi.fd,fi.iobuf,FISizeFileBuf)) != -1)
				break;

			// Read failed.  Return exception if unexpected error or maximum retries reached.
			if(errno != EAGAIN || --loopct == 0)
				return rcset(Failure,0,text141,strerror(errno),fi.fname);
						// "I/O Error: %s, file \"%s\""

			// Pause and try again.
			cpause(FIReadTimeout);
			}

		// Hit EOF?
		if(n == 0) {

			// Yep, note it...
			fi.flags |= FIEOF;
EOFRetn:
			// and return it.
			*cp = -1;
			return IOEOF;
			}

		// No, update buffer pointers.
		fi.iobufz = (fi.iobufc = fi.iobuf) + n;
		}

	*cp = (short) *fi.iobufc++;
	return rc.status;
	}

// Read a line from a file into fi.lbuf and store the byte count and delimiter status in the supplied pointers.  Check for I/O
// errors and return status.  *hasDelimp is not set unless at least one byte was read (so the returned value from a previous
// call will remain valid).
static int ffgetline(uint *len,bool *hasDelimp) {
	int status;
	short c;
	uint llen;
	bool hasDelim = false;

	// If we are at EOF, bail out.
	if(fi.flags & FIEOF)
		return IOEOF;

	// Initialize.
	fi.lbufc = fi.lbuf;		// Current line buffer pointer.

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
				goto DelimFound;
				}
			if(c == '\r') {
				fi.idelim1 = '\r';
				if((status = ffgetc(&c)) == IOEOF)
					goto CR;
				if(status != Success)
					return rc.status;
				if(c == '\n') {
					fi.idelim2 = '\n';
					goto DelimFound;
					}

				// Not a CR-LF sequence; "unget" the last character.
				--fi.iobufc;
CR:
				fi.idelim2 = -1;
				goto DelimFound;
				}

			// No NL or CR found yet... onward.
			if(ffputc(c) != Success)
				return rc.status;
			}
		}

	// We have line delimiter(s)... read next line.
	for(;;) {
		if((status = ffgetc(&c)) == IOEOF)
			goto EOLn;
		if(status != Success)
			return rc.status;
		if(c == fi.idelim1) {

			// Got match on first delimiter.  Need two?
			if(fi.idelim2 == -1)

				// Nope, we're done.
				goto DelimFound;

			// Yes, check next character.
			if((status = ffgetc(&c)) != IOEOF) {
				if(status != Success)
					return rc.status;
				if(c == fi.idelim2)

					// Second delimiter matches also, we're done.
					goto DelimFound;

				// Not a match; "unget" the last character.
				--fi.iobufc;
				c = fi.idelim1;
				}
			}

		// Delimiter not found, or two delimiters needed, but only one found.  Save the character and move onward.
		if(ffputc(c) != Success)
			return rc.status;
		}
DelimFound:
	hasDelim = true;
EOLn:
	// Hit EOF and nothing read?
	if((llen = fi.lbufc - fi.lbuf) == 0 && (fi.flags & FIEOF))

		// Yes.
		return IOEOF;

	// Got a (possibly zero-length) line.  Save the length for the caller and return delimiter status if non-zero.
	if((*len = llen) > 0)
		*hasDelimp = hasDelim;

	return rc.status;
	}

// Insert a line into a buffer before point.  Return status.
int insline(char *src,int len,bool hasDelim,Buffer *buf,Point *point) {
	Line *lnp;
	int size;

	// Allocate new buffer line for text to be inserted plus a portion of the point line, depending on point's position.
	size = len + (hasDelim ? point->off : point->lnp->l_used);
	if(lalloc(size,&lnp) != Success)
		return rc.status;					// Fatal error.
	if(hasDelim && point->off == 0)
		memcpy(lnp->l_text,src,len);				// Copy line.
	else {
		char *str,*strz;
		char *dest = lnp->l_text;
		if(point->off > 0) {
			strz = (str = point->lnp->l_text) + point->off;	// Copy text before point.
			do
				*dest++ = *str++;
				while(str < strz);
			}
		strz = src + len;					// Copy line.
		while(src < strz)
			*dest++ = *src++;
		if(hasDelim) {
			if(point->off == point->lnp->l_used)		// Shift any remaining text to BOL.
				point->off = point->lnp->l_used = 0;	// Nothing left.
			else {
				str = (dest = point->lnp->l_text) + point->off;
				strz = point->lnp->l_text + point->lnp->l_used;
				do
					*dest++ = *str++;
					while(str < strz);
				point->lnp->l_used -= point->off;
				point->off = 0;
				}
			}
		else {
			if(point->off < point->lnp->l_used) {		// Copy remaining text.
				str = point->lnp->l_text + point->off;
				strz = point->lnp->l_text + point->lnp->l_used;
				do
					*dest++ = *str++;
					while(str < strz);
				}
			point->off += len;
			lreplace1(point->lnp,buf,lnp);			// Replace point line with new one.
			point->lnp = lnp;
			return rc.status;
			}
		}
	llink(lnp,buf,point->lnp);					// Link new line before point line.
	return rc.status;
	}

// Read data from a buffer or file, insert at point in target buffer, and return status.  DataInsert object contains additional
// input and output parameters.  If srcbuf is not NULL, lines are "read" from that buffer; otherwise, they are read from current
// (opened) input file, status parameter is set to result, and finalDelim parameter is set to true if last line read had a
// delimiter; otherwise, false.  If targpoint parameter is not NULL, target buffer is not marked as changed after lines are
// inserted; if targpoint parameter is NULL, point from current window is used and (1), inserted lines are marked as a region;
// (2), target buffer is marked as changed if any lines were inserted; and (3), point is moved to beginning of region if n is
// zero.  In all cases, "n" parameter is updated to number of lines inserted.
int idata(int n,Buffer *srcbuf,DataInsert *dip) {
	Point *point,dot0;
	Line *dotlnp0;
	int wpos;
	int len;
	bool hasDelim = true;

	// Point given?
	if(dip->targpoint == NULL) {

		// No, use one from current window and save state of target buffer for region manipulation later.
		point = &si.curwin->w_face.wf_point;
		dot0.lnp = (point->lnp == dip->targbuf->b_lnp) ? NULL : point->lnp->l_prev;
		wpos = getwpos(si.curwin);
		}
	else
		// Yes, use it.
		point = dip->targpoint;
	dot0.off = point->off;
	dotlnp0 = point->lnp;

	// Insert each line from the buffer or file before point line in target buffer.
	dip->lineCt = 0;
	if(srcbuf != NULL) {
		Line *buflnp = srcbuf->b_lnp;				// First line of source buffer.
		do {
			hasDelim = (buflnp->l_next != NULL);
			if(insline(buflnp->l_text,len = buflnp->l_used,hasDelim,dip->targbuf,point) != Success)
				return rc.status;
			++dip->lineCt;
			} while((buflnp = buflnp->l_next) != NULL);	// and check for EOB in source buffer.
		}
	else {
		// Read lines from current input file.  If a line is read that has no delimiter, it is the last line of the file
		// (and len > 0).  If point was not specified, insert line's contents at point via linsert() being that point is
		// from the current window; otherwise, replace target point line with line read.  In the latter case, it is
		// assumed that the point line is last line of the buffer.
		if(mlputs(MLHome | MLWrap | MLFlush,dip->msg) != Success)		// Let user know what's up...
			return rc.status;
		while((dip->status = ffgetline(&len,&hasDelim)) == Success) {		// and read the file.
			if(insline(fi.lbuf,len,hasDelim,dip->targbuf,point) != Success)
				return rc.status;
			++dip->lineCt;
			}

		// Last read was unsuccessful, check for errors.
		if(dip->status <= FatalError)
			return rc.status;				// Bail out.
		if(dip->status >= Success)
			dip->status = ffclose(false);			// Read was successful; preserve close status.
		else
			(void) ffclose(false);				// Read error; ignore close status.
		dip->finalDelim = hasDelim;
		}

	// Update line pointers if last line inserted had no delimiter (causing point line to be reallocated by insline()).
	if(!hasDelim) {
		if(dip->targbuf->b_nwind > 0)
			fixwface(dot0.off,len,dotlnp0,point->lnp);
		fixbface(dip->targbuf,dot0.off,len,dotlnp0,point->lnp);
		}

	// All lines inserted.  Set mark RegMark to first inserted line and restore original buffer framing, if applicable.
	if(dip->targpoint == NULL) {
		dip->targbuf->b_mroot.mk_point.lnp = (dot0.lnp == NULL) ? dip->targbuf->b_lnp : dot0.lnp->l_next;
		dip->targbuf->b_mroot.mk_point.off = dot0.off;
		dip->targbuf->b_mroot.mk_rfrow = wpos;

		// If n is zero, swap point and RegMark.
		if(n == 0)
			(void) swapmid(RegMark);
		}

	// Update window flags and return line count.
	if(dip->lineCt > 0) {
		bchange(dip->targbuf,WFHard);
		if(dip->targbuf == si.curbuf && n != 0) {
			si.curwin->w_rfrow = 0;
			si.curwin->w_flags |= WFReframe;
			}
		}

	return rc.status;
	}

// Does "fn" exist on disk?  Return -1 (false), 0 (true and not a directory), or 1 (true and is a directory).
int fexist(char *fn) {
	struct stat s;

	return stat(fn,&s) != 0 ? -1 : (s.st_mode & S_IFDIR) ? 1 : 0;
	}

// Report I/O results as a return message, given active string-fab object.  Return status.
int iostat(DStrFab *rmsg,ushort flags,Datum *bkname,int status,char *fname,char *lcmsg,uint lineCt) {
	char *str;

	// Report any non-fatal read or close error.
	if(!(flags & IOS_OtpFile) && status < Success) {
		if(dputf(rmsg,text141,strerror(errno),fname == NULL ? "(stdin)" : fname) != 0 || dputc(' ',rmsg) != 0)
				// "I/O Error: %s, file \"%s\""
			goto LibFail;
		si.curbuf->b_flags |= BFActive;		// Don't try to read file again.
		}

	// Begin wrapped message and report I/O operation.
	if(dputc('[',rmsg) != 0 || dputs(lcmsg,rmsg) != 0)
		goto LibFail;

	// Report line count.
	if(dputf(rmsg," %u %s%s",lineCt,text205,lineCt != 1 ? "s" : "") != 0)
				// "line"
		goto LibFail;

	// Non-standard line delimiter(s)?
	str = (flags & IOS_OtpFile) ? fi.odelim : si.curbuf->b_inpdelim;
	if(str[0] != '\0' && (str[0] != '\n' || str[1] != '\0')) {
		if(dputs(text252,rmsg) != 0 || dvizs(str,0,VBaseDef,rmsg) != 0)
				// " delimited by "
			goto LibFail;
		}

	// Missing line delimiter at EOF?
	if((flags & IOS_NoDelim) && dputs(text291,rmsg) != 0)
			// " without delimiter at EOF"
		goto LibFail;

	// Insert?
	if(lcmsg == text154 && dputs(text355,rmsg) != 0)
			// " and marked as region"
		goto LibFail;

	// Backup file created on output?
	if(bkname != NULL) {
		if(dputs(text257,rmsg) != 0 || dputs(fbasename(bkname->d_str,true),rmsg) != 0 || dputc('"',rmsg) != 0)
				// ", original file renamed to \""
			goto LibFail;
		}

	if(dputc(']',rmsg) != 0 || dclose(rmsg,sf_string) != 0)
		goto LibFail;
	if(mlerase() != Success)
		return rc.status;
	return rcset(status >= Success ? Success : status,RCNoFormat | RCNoWrap,rmsg->sf_datum->d_str);
LibFail:
	return librcset(Failure);
	}

// Prompt for a filename (which cannot be null), using def as default (which may be NULL).  Return status.
int gtfilename(Datum *rval,char *prmt,char *def,uint cflags) {

	if(si.opflags & OpScript)
		(void) funcarg(rval,ArgFirst | ArgNotNull1 | ArgPath);
	else {
		TermInp ti = {def,RtnKey,MaxPathname,NULL};
		(void) terminp(rval,prmt,ArgNotNull1 | ArgNil1,Term_C_Filename | cflags,&ti);
		}
	return rc.status;
	}

// Read given file into a buffer (for readFile and readPipe commands).  If default n, use the current buffer.  If not default n,
// create a buffer and either (1), use a scratch name for the buffer and don't save the filename in the buffer header if
// RWScratch flag is set; or (2), derive the buffer name from the filename and save the filename in the buffer header if
// RWScratch flag is not set.  Render buffer and return status.
int readFP(Datum *rval,int n,char *fname,ushort flags) {
	Buffer *buf;

	if(n == INT_MIN)
		buf = si.curbuf;						// Use current buffer by default.
	else {									// Otherwise...
		if(flags & RWScratch) {						// if scratch buffer requested...
			if(bscratch(&buf) != Success)				// get one...
				return rc.status;
			}							// otherwise, create a buffer.
		else if(bfind(fname,BS_Create | BS_Hook | BS_Derive | BS_Force,0,&buf,NULL) != Success)
			return rc.status;
		}
	if(!(flags & RWScratch))
		flags |= RWKeep;
	if(readin(buf,fname,flags | RWStats) != Success || (n == INT_MIN &&	// Read the file and rename current buffer
	 !(flags & RWScratch) && brename(NULL,BR_Auto,si.curbuf) != Success))	// if a buffer was not created.
		return rc.status;

	return render(rval,n == INT_MIN ? 1 : n,buf,				// Render the buffer.
	 n == INT_MIN ? 0 : RendNewBuf | RendNotify);
	}

// Insert a file into the current buffer.  If zero argument, leave point before the inserted lines; otherwise, after.  This
// is really easy; all you do it find the name of the file, and call the standard "insert a file into the current buffer" code.
int insertFile(Datum *rval,int n,Datum **argv) {

	if(gtfilename(rval,text132,NULL,Term_NoDef) != Success || rval->d_type == dat_nil)
			// "Insert file"
		return rc.status;

	// Open the file and insert the data.
	if(ffropen(rval->d_str,true) == Success) {
		DStrFab sfab;
		DataInsert di = {
			si.curbuf,	// Target buffer.
			NULL,		// Target line.
			text153		// "Inserting data..."
			};

		if(idata(n,NULL,&di) == Success) {
			if(dopentrk(&sfab) != 0)
				return librcset(Failure);
			(void) iostat(&sfab,di.finalDelim ? 0 : IOS_NoDelim,NULL,di.status,rval->d_str,text154,di.lineCt);
												// "Inserted"
			}
		}
	dclear(rval);
	return rc.status;
	}

// Check if a buffer's filename matches given filename.  Return status, including NotFound if no match.
static int filecmp(int n,Buffer *buf,char *fname,EScreen *scr) {

	if(buf->b_fname != NULL && strcmp(buf->b_fname,fname) == 0) {

		// Match found.  Check directory.
		if(*fname == '/' || scr == NULL || strcmp(scr->s_wkdir,si.curscr->s_wkdir) == 0) {
			if(n == INT_MIN || n >= 0)
				(void) rcset(Success,RCNoFormat,text135);
							// "Old buffer"
			return rc.status;
			}
		}
	return NotFound;
	}

// Find a file and optionally, read it into a buffer (for findFile and viewFile commands).  If n == 0, just create a buffer for
// the file if one does not already exist and stay in current buffer.  If n != 0 and the file is not currently attached to a
// buffer, create a buffer and set to "read only" if "view" is true.  Render buffer and return status.
int fvfile(Datum *rval,int n,bool view) {
	Datum *el;
	Buffer *buf;
	Array *ary;
	EScreen *scr;
	EWindow *win;
	int status;
	bool created = false;

	// Get filename.
	if(gtfilename(rval,view ? text134 : text133,NULL,n < 0 || n > 1 ? Term_NoDef : (Term_NoDef | Term_C_NoAuto))
	 != Success || rval->d_type == dat_nil)
			// "View file","Find file"
		return rc.status;

	// Check if an existing buffer is attached to the file.
	scr = si.shead;
	do {							// In all screens...
		win = scr->s_whead;
		do {						// In all windows...
			if((status = filecmp(n,buf = win->w_buf,rval->d_str,scr)) < NotFound)
				return rc.status;
			if(status == Success)
				goto ViewChk;
			} while((win = win->w_next) != NULL);
		} while((scr = scr->s_next) != NULL);
	ary = &buftab;						// Scan buffer list.
	while((el = aeach(&ary)) != NULL) {
		buf = bufptr(el);
		if(buf->b_nwind == 0) {			// Skip if already checked.
			if((status = filecmp(n,buf,rval->d_str,buf->b_lastscr)) < NotFound)
				return rc.status;
			if(status == Success)
				goto ViewChk;
			}
		}

	// No buffer found... create one.
	if(bfind(rval->d_str,BS_Create | BS_Hook | BS_Derive | BS_Force,0,&buf,&created) != Success)
		return rc.status;

	// Set filename and flag as inactive.
	if(setfname(buf,rval->d_str) != Success)
		return rc.status;
	buf->b_flags = 0;
ViewChk:
	// Set "read only" buffer attribute if view is true.  No need to run filename hook -- it will be run when buffer is
	// activated.  If file being viewed is associated with an existing buffer that has been modified, notify user of such.
	if(view) {
		if(buf->b_flags & BFChanged)
			(void) rcset(Success,RCForce | RCNoWrap,text461,text260,text460);
					// "Cannot set %s buffer to %s","changed","read-only"
		else {
			buf->b_flags |= BFReadOnly;
			supd_wflags(buf,WFMode);
			}
		}

	return render(rval,n == INT_MIN ? 1 : n,buf,created ? RendNewBuf : 0);
	}

// Prepare a buffer for reading.  "flags" is passed to bclear().  Return status.
int readprep(Buffer *buf,ushort flags) {

	if(bclear(buf,flags) == Success) {
		if(buf->b_flags & BFNarrowed)				// Mark as changed if narrowed.
			buf->b_flags |= BFChanged;
		}
	return rc.status;
	}

// Read a file into the given buffer, blowing away any text found there.  If fname is NULL, use the buffer's filename if set;
// otherwise, standard input.  If RWKeep flag is not set, delete the buffer's filename if set; otherwise, save the filename in
// the buffer record (if it is not narrowed) and, if RWNoHooks flag is not set, (a), run the read hook after the file is read;
// and (b), run the filename hook if the buffer filename was changed.  If RWExist flag is set, return an error if the file does
// not exist.  If RWStats flag set, set a return message containing input statistics.  Return status, including Cancelled if
// buffer was not cleared (user did not "okay" it) and IONSF if file does not exist.
int readin(Buffer *buf,char *fname,ushort flags) {
	EWindow *win;
	DataInsert di = {
		buf,			// Target buffer.
		&buf->b_face.wf_point,	// Target line.
		text139			// "Reading data..."
		};

	if(readprep(buf,0) != Success)
		return rc.status;					// Error or user got cold feet.

	// Determine filename.
	if(fname == NULL)
		fname = buf->b_fname;
	else if((flags & RWKeep) && !(buf->b_flags & BFNarrowed) &&
	 setfname(buf,fname) != Success)				// Save given filename.
		return rc.status;

	// Open the file.
	if((di.status = ffropen(fname,flags & RWExist)) <= FatalError)
		return rc.status;					// Bail out.
	if(di.status != Success)
		goto Retn;

	// Read the file and "unchange" the buffer.
	if(idata(1,NULL,&di) != Success)
		return rc.status;
	buf->b_flags &= ~BFChanged;

	// Report results.
	if(flags & RWStats) {
		DStrFab sfab;

		if(dopentrk(&sfab) != 0)
			return librcset(Failure);
		if(iostat(&sfab,di.finalDelim ? 0 : IOS_NoDelim,NULL,di.status,fname,text140,di.lineCt) != Success)
										// "Read"
			return rc.status;
		}
Retn:
	// Make sure buffer is flagged as active.
	buf->b_flags |= BFActive;

	// Update buffer and window pointers.
	faceinit(&buf->b_face,buf->b_lnp,buf);
	win = si.whead;
	do {
		if(win->w_buf == buf) {
			faceinit(&win->w_face,buf->b_lnp,NULL);
			win->w_flags |= (WFHard | WFMode);
			}
		} while((win = win->w_next) != NULL);

	// Erase filename and run read hook if needed.
	if(!(flags & RWKeep))
		clfname(buf);
	else if(di.status >= Success && !(flags & RWNoHooks) && !(buf->b_flags & BFMacro))
		if(exechook(NULL,INT_MIN,hooktab + HkRead,2,buf->b_bname,buf->b_fname) <= FatalError)
			return rc.status;

	// Return status.
	return (di.status == IONSF) ? rcset(Success,RCNoFormat,text138) : mlerase();
							// "New file"
	}

// Check screens for multiple working directories.  If buf is NULL, return true if multiple screens exist and two or more have
// different working directories; otherwise, return true if the given buffer is being displayed on at least two screens having
// different working directories.  In the latter case, it is assumed that different directories exist.
static bool wdstat(Buffer *buf) {
	EScreen *scr = si.shead;
	char *wkdir = NULL;
	if(buf == NULL) {						// Check for multiple working directories.
		if(scr->s_next == NULL)
			return false;					// Only one screen.
		do {							// In all screens...
			if(wkdir == NULL)
				wkdir = scr->s_wkdir;
			else if(strcmp(wkdir,scr->s_wkdir) != 0)
				return true;				// Different directory found.
			} while((scr = scr->s_next) != NULL);
		}
	else {								// Check if buffer is multi-homed.
		EWindow *win;

		do {							// In all screens...
			win = scr->s_whead;				// Buffer on this screen?
			do {
				if(win->w_buf == buf) {
					if(wkdir == NULL)
						wkdir = scr->s_wkdir;
					else if(strcmp(wkdir,scr->s_wkdir) != 0)
						return true;		// Different directory found.
					break;
					}
				} while((win = win->w_next) != NULL);
			} while((scr = scr->s_next) != NULL);
		}

	return false;
	}

// Check given buffer flags and verify with user before writing to a file.  Return status.
static int bufcheck(Buffer *buf) {

	// Complain about narrowed buffer.
	if(buf->b_flags & BFNarrowed) {
		bool yep;
		DStrFab prompt;

		if(dopentrk(&prompt) != 0 ||
		 dputf(&prompt,text147,text308,buf->b_bname) != 0 || dclose(&prompt,sf_string) != 0)
				// "%s buffer '%s'... write it out","Narrowed"
			(void) librcset(Failure);
		else if(terminpYN(prompt.sf_datum->d_str,&yep) == Success && !yep)
			(void) rcset(UserAbort,0,NULL);
		}

	return rc.status;
	}

// Do appendFile or writeFile command.  Ask for a filename, and write or append the contents of the current buffer to that file.
// Update the remembered filename if writeFile and default n or n > 0.  Return status.
int awfile(Datum *rval,int n,char *prmt,short mode) {

	// Get the filename.
	if(gtfilename(rval,prmt,NULL,Term_C_NoAuto) != Success || rval->d_type == dat_nil)
		return rc.status;

	// Complain about narrowed buffers.
	if(bufcheck(si.curbuf) != Success)
		return rc.status;

	// It's a go... write the file.
	if(writeout(si.curbuf,rval->d_str,mode,RWStats) != Success)
		return rc.status;

	// Update filename and buffer name if write (not append) and not suppressed by n value (<= 0).
	if(mode == 'w' && (n == INT_MIN || n > 0) && setfname(si.curbuf,rval->d_str) > FatalError)
		if(!(si.curbuf->b_flags & BFMacro))
			(void) brename(NULL,BR_Auto,si.curbuf);
	dclear(rval);

	return rc.status;
	}

// Save given buffer to disk if possible.  *cwd contains current directory and is updated if directory is changed.  If BS_QExit
// flag is set, buffer is flagged (with BFQSave) for "quickExit" command if saved.
static int savebuf(Buffer *buf,ushort flags,char **cwd,char *wkdir,uint *scount,uint *ncount) {

	buf->b_flags &= ~BFQSave;					// Clear "saved" flag.
	if(!(buf->b_flags & BFChanged) ||				// If not changed...
	 ((buf->b_flags & BFMacro) && (flags & BS_All)))		// or a macro and saving all...
		return Cancelled;					// skip it.
	if(buf->b_fname == NULL)					// Must have a filename.
		return rcset(Failure,0,text145,buf->b_bname);
			// "No filename associated with buffer '%s'"
	if((buf->b_flags & BFNarrowed) && (flags & BS_All)) {		// If narrowed and saving all...
		++*ncount;						// count it and...
		return Cancelled;					// skip it.
		}
	if(bufcheck(buf) != Success)					// Complain about narrowed buffers.
		return rc.status;

	// Change working directory if needed.
	if((flags & BS_MultiDir) && *buf->b_fname != '/') {		// If multiple directories and have relative pathname...
		if(wkdir == NULL) {					// buffer is in the background...
			if(buf->b_lastscr == NULL) {			// and "last screen" pointer not set...
				bool yep;				// get permission from user to use current screen's
				DStrFab prompt;				// directory.

				if(dopentrk(&prompt) != 0 || dputf(&prompt,text147,text309,buf->b_bname) != 0 ||
								// "%s buffer '%s'... write it out","Directory unknown for"
				 dclose(&prompt,sf_string) != 0)
					return librcset(Failure);
				if(terminpYN(prompt.sf_datum->d_str,&yep) != Success)
					return rc.status;
				if(!yep)				// Permission given?
					return rcset(UserAbort,0,NULL);	// No, user abort.
				wkdir = si.curscr->s_wkdir;		// Yes, use current directory.
				buf->b_lastscr = si.curscr;
				}
			else
				wkdir = buf->b_lastscr->s_wkdir;	// Have "last screen" pointer... use it.
			}
		else if(wdstat(buf))					// Multi-homed buffer (in foreground)?
			return rcset(Failure,0,text310,text309,buf->b_bname);	// Yes, error.
				// "%s multi-screen buffer '%s'","Directory unknown for"

		// It's a go... change directory if different.
		if(strcmp(*cwd,wkdir) != 0 && chgdir(*cwd = wkdir) != Success)
			return rc.status;
		}

	// Save the buffer.
	if(mlprintf(MLHome | MLWrap | MLFlush,text103,buf->b_fname) != Success)
			// "Saving %s..."
		return rc.status;
	if((flags & (BS_All | BS_QExit)) == BS_All)
		cpause(50);
	if(writeout(buf,buf->b_fname,'w',RWStats) != Success)
		return rc.status;
	si.gacount = si.gasave;					// Reset $autoSave...
	++*scount;						// increment "save" count...
	if(flags & BS_QExit)
		buf->b_flags |= BFQSave;			// and flag as saved for main() to display on quick exit.
	return rc.status;
	}

// Save the contents of the current buffer (or all buffers if n argument) to its associated file.  Do nothing if nothing has
// changed.  Error if there is no filename set for a buffer.
int savebufs(int n,ushort flags) {
	Datum **blp,**blpz;
	Buffer *buf;
	int status;
	char *wkdir = si.curscr->s_wkdir;
	uint scount = 0;			// Count of saved buffers.
	uint ncount = 0;			// Count of narrowed buffers that are skipped.

	if(n != INT_MIN)
		flags |= BS_All;
	if(wdstat(NULL))
		flags |= BS_MultiDir;

	// Search for changed buffers one screen at a time if updating all to simplify tracking and minimize frequency of
	// chdir() calls when multiple screen directories exist.
	if(flags & BS_All) {
		EWindow *win;
		EScreen *scr = si.shead;
		do {						// In all screens...
			win = scr->s_whead;
			do {					// In all windows...
				if((status = savebuf(win->w_buf,flags,&wkdir,scr->s_wkdir,&scount,&ncount)) <= UserAbort)
					return rc.status;
				} while((win = win->w_next) != NULL);
			} while((scr = scr->s_next) != NULL);
		}

	// Scan buffer list for changed buffers.
	blpz = (blp = buftab.a_elp) + buftab.a_used;
	buf = (flags & BS_All) ? bufptr(*blp) : si.curbuf;
	for(;;) {
		// Skip any buffer being displayed if screen traversal was done so that any multi-homed buffers that were
		// skipped (and counted) will be skipped here also.
		if(!(flags & BS_All) || buf->b_nwind == 0) {
			if((status = savebuf(buf,flags,&wkdir,buf == si.curbuf ? si.curscr->s_wkdir : NULL,
			 &scount,&ncount)) <= UserAbort)
				return rc.status;
			}
		if(!(flags & BS_All) || ++blp == blpz)
			break;
		buf = bufptr(*blp);
		}

	// Return to original working directory if needed and set return message.
	if(strcmp(wkdir,si.curscr->s_wkdir) != 0)
		(void) chgdir(si.curscr->s_wkdir);
	if((flags & (BS_All | BS_QExit)) == BS_All && rc.status == Success) {
		char wkbuf[NWork];

		sprintf(wkbuf,text167,scount,scount != 1 ? "s" : "");
				// "%u buffer%s saved"
		if(ncount > 0)
			sprintf(strchr(wkbuf,'\0'),text458,ncount,ncount != 1 ? "s" : "");
					// ", %u narrowed buffer%s skipped"
		(void) rcset(Success,RCForce | RCNoFormat,wkbuf);
		}
	return rc.status;
	}

// Write a buffer to an open file descriptor (disk file or pipe).  Return status.
int writefd(Buffer *buf,uint *lcp) {
	Line *lnp,*lnpz;
	uint lineCt;
	bool addNL = (buf->b_flags & BFNarrowed) && buf->b_nbotlnp != NULL;

	if(mlputs(MLHome | MLWrap | MLFlush,text148) != Success)
					// "Writing data..."
		return rc.status;

	// Check if buffer is narrowed and/or ATerm mode is set.  If buffer is narrowed and bottom portion exists, ignore ATerm
	// mode and add a newline to output file at EOB; otherwise, append a line delimiter to the buffer if ATerm mode set and
	// last line not empty.
	lnpz = (lnp = buf->b_lnp)->l_prev;
	if(!addNL && lnpz->l_used > 0 && modeset(MdIdxATerm,buf)) {
		Line *lnp0;

		if(lalloc(0,&lnp0) != Success)		// Empty line.
			return rc.status;		// Fatal error.
		llink(lnp0,buf,NULL);			// Append empty line (newline) to end of buffer...
		bchange(buf,WFHard);			// set "change" flags...
		lnpz = lnp->l_prev;			// and update "last line" pointer.
		}

	// Write the current buffer's lines to the open file.
	lineCt = 0;
	while(lnp != lnpz || addNL || lnp->l_used > 0) {
		if(ffputline(lnp,lnp != lnpz || addNL) != Success) {

			// Write or close error: clean up and get out.
			(void) ffclose(true);
			return rc.status;
			}
		++lineCt;
		if((lnp = lnp->l_next) == NULL)
			break;
		}

	// Write was successful: close output file and return.
	if(lcp != NULL)
		*lcp = lineCt;
	return ffclose(true);
	}

// Write a buffer to disk, given buffer, filename and mode ('w' = write, 'a' = append) and return status.  Before the file is
// written, a user routine (write hook) can be run.  The number of lines written, and possibly other status information is
// displayed.  If "ATerm" mode is enabled and the buffer does not have a terminating line delimiter, one is added before the
// buffer is written.  If "Safe" or "Bak" mode is enabled, the buffer is written out to a temporary file, the old file is
// unlinked or renamed to "<file>.bak" (if "<file>.bak" does not already exist), and the temporary is renamed to the original.
int writeout(Buffer *buf,char *fn,short mode,ushort flags) {
	uint lineCt;			// Number of lines written.
	int status;			// Return status.
	ushort sflag = 0;		// Are we safe saving? (0x01 bit = 'safe' save, 0x02 bit = 'bak' save)
	DStrFab sfab;
	struct stat st;			// File permision info.
	Datum *tname;			// Temporary filename.
	Datum *bkname;			// Backup filename.

	if(dopentrk(&sfab) != 0)
		goto LibFail;

	// Run user-assigned write hook.
	if(!(buf->b_flags & (BFHidden | BFMacro)) && exechook(NULL,INT_MIN,hooktab + HkWrite,2,buf->b_bname,fn) != Success)
		return rc.status;

	// Determine if we will use the "safe save" method.
	if((modeset(MdIdxBak,buf) || modeset(MdIdxSafe,buf)) && fexist(fn) >= 0 && mode == 'w') {
		if(modeset(MdIdxBak,buf)) {

			// 'bak' mode enabled.  Create backup version of filename.
			if(dnewtrk(&bkname) != 0 || dsalloc(bkname,strlen(fn) + strlen(Backup_Ext) + 1) != 0)
				goto LibFail;
			sprintf(bkname->d_str,"%s%s",fn,Backup_Ext);

			// Enable 'bak' save if backup file does not already exist.
			if(fexist(bkname->d_str) < 0)
				sflag = 0x02;
			}
		if(modeset(MdIdxSafe,buf))
			sflag |= 0x01;
		}

	// Open output file.
	if(sflag) {
		char *suffix;
		size_t len;

		// Duplicate original file pathname and keep first letter of filename.
		len = fbasename(fn,true) - fn + 1;
		if(dnewtrk(&tname) != 0 || dsalloc(tname,len + 6) != 0)
			goto LibFail;
		stplcpy(tname->d_str,fn,len + 1);
		suffix = tname->d_str + len;

		// Create a unique name, using random numbers.
		do {
			long_asc(xorshift64star(0xFFFF) - 1,suffix);
			} while(fexist(tname->d_str) >= 0);

		// Open the temporary file.
		status = ffwopen(buf,tname->d_str,mode);
		}
	else
		status = ffwopen(buf,fn,mode);

	// If the open failed or write fails, abort mission.
	if(status != Success || (status = writefd(buf,&lineCt)) != Success) {
		if(sflag)
			(void) unlink(tname->d_str);
		return rc.status;
		}

	// Write was successful: clear "changed" flag and update window flags.
	buf->b_flags &= ~BFChanged;
	supd_wflags(buf,WFMode);

	// Do file manipulations if safe save.
	if(sflag) {
		// Get the permisions of the original file.
		if((status = stat(fn,&st)) != 0)
			status = errno;

		// Rename or erase original file and rename temp to original.
		if(sflag & 0x02) {
			if(rename(fn,bkname->d_str) != 0)
				status = -1;
			}
		else if(unlink(fn) != 0)
			status = -1;
		if(status == 0 && rename(tname->d_str,fn) == 0) {
			if(chmod(fn,st.st_mode) != 0 || chown(fn,st.st_uid,st.st_gid) != 0)
				status = errno;
			}

		// Report any errors.
		if(status != 0) {
			if(dputf(&sfab,text141,strerror(errno),fn) != 0 || (status == -1 &&
			 (dputc(' ',&sfab) != 0 || dputf(&sfab,text150,tname->d_str) != 0)))
					// "I/O Error: %s, file \"%s\"","(file saved as \"%s\") "
				goto LibFail;
			status = Failure;	// Failed.
			}
		}

	// Report lines written and return status.
	return (flags & RWStats) ?
	 iostat(&sfab,lineCt > 0 && (!(buf->b_flags & BFNarrowed) || buf->b_nbotlnp == NULL) &&
	  buf->b_lnp->l_prev->l_used > 0 ? (IOS_OtpFile | IOS_NoDelim) : IOS_OtpFile,
	  status == Success && (sflag & 0x02) ? bkname : NULL,status,fn,text149,lineCt) : rc.status;
									// "Wrote"
LibFail:
	return librcset(Failure);
	}

// Set the filename associated with a buffer and call the filename hook if appropriate.  If interactive mode and default n, use
// current buffer for target buffer.  If the new filename is nil or null, set the filename to null; otherwise, if n > 0 or is
// the default, change the buffer name to match the filename.  rval is set to a two-element array containing new buffer name and
// new filename.  Return status.
int setBufFile(Datum *rval,int n,Datum **argv) {
	Buffer *buf;

	// Get buffer.
	if(n == INT_MIN && !(si.opflags & OpScript))
		buf = si.curbuf;
	else if(bcomplete(rval,text151,(buf = bdefault()) != NULL ? buf->b_bname : NULL,OpDelete,&buf,NULL) != Success ||
			// "Set filename in"
	 buf == NULL)
		return rc.status;

	// We cannot modify an executing buffer.
	if(buf->b_mip != NULL && buf->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text284,text276,text248);
			// "Cannot %s %s buffer","modify","an executing"

	// Get filename.
	if(si.opflags & OpScript) {
		if(funcarg(rval,ArgNil1 | ArgPath) != Success)
			return rc.status;
		}
	else {
		TermInp ti = {buf->b_fname,RtnKey,MaxPathname,NULL};
		char *str,wkbuf[strlen(text151) + 16];

		if(n == INT_MIN) {
			str = stpcpy(wkbuf,text151);
				// "Set filename in"
			do {
				--str;
				} while(*str != ' ');
			strcpy(str + 1,text339);
				// "to"
			str = wkbuf;
			}
		else
			str = text339;
		if(terminp(rval,str,ArgNil1,Term_C_Filename | Term_C_NoAuto,&ti) != Success)
			return rc.status;
		}

	// Save new filename (which may be null or nil).
	if(rval->d_type == dat_nil)
		dsetnull(rval);
	if(setfname(buf,rval->d_str) != Success)
		return rc.status;

	// Change buffer name if applicable.
	if(buf->b_fname != NULL && !(buf->b_flags & BFMacro) && (n == INT_MIN || n > 0))
		if(brename(NULL,BR_Auto,buf) != Success)
			return rc.status;

	// Set rval to result (an array).
	Array *ary;
	if((ary = anew(2,NULL)) == NULL || dsetstr(buf->b_bname,ary->a_elp[0]) != 0)
		goto LibFail;
	if(*rval->d_str == '\0')
		dsetnil(ary->a_elp[1]);
	else if(dsetstr(rval->d_str,ary->a_elp[1]) != 0)
LibFail:
		return librcset(Failure);
	if(awrap(rval,ary) == Success)

		// Update mode line.
		supd_wflags(buf,WFMode);

	return rc.status;
	}
