// (c) Copyright 2018 Richard W. Marinelli
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
// I/O:
//	filterBuf
// Filename management:
//	setBufFile
//	gtfilename(), setfname()
//
// CORE INPUT FUNCTIONS
//	findFile command
//		* call fvfile()
//	fvfile(Datum *rp,int n,bool view)
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
//	readFP(Datum *rp,int n,char *fname,ushort flags)
//		* create new buffer with name derived from filename if requested
//		* read given file into current buffer or new buffer
//		* call render()
//		=> CALLED BY execCF() for readFile command, readPipe()
//	readFile command
//		* get a filename and call readFP()
//	readin(Buffer *bufp,char *fname,ushort flags)
//		* clear given buffer and read a file into it (if fname is NULL, use buffer's filename; otherwise, stdin)
//		* if RWKeep flag is set, save filename and call read hook; otherwise, delete buffer's filename
//		* if RWExist flag is set, return error if file does not exist
//		=> CALLED BY bactivate(), execfile(), dopop(), filterBuf(), readFP()
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
static int globcpy(Datum *rp,glob_t *gp) {
	Array *aryp;

	if(mkarray(rp,&aryp) != Success)
		return rc.status;
	if(gp->gl_pathc > 0) {
		Datum dat;
		char **pathp = gp->gl_pathv;
		dinit(&dat);
		do {
			if(dsetstr(*pathp,&dat) != 0 || apush(aryp,&dat,true) != 0)
				return librcset(Failure);
			} while(*++pathp != NULL);
		dclear(&dat);
		globfree(gp);
		}
	return rc.status;
	}

// Do "glob" function: return a (possibly empty) array of file(s) and/or director(ies) that match given shell glob pattern.
// Return status.
int globpat(Datum *rp,int n,Datum **argpp) {

	if(disnull(*argpp)) {
		Array *aryp;
		(void) mkarray(rp,&aryp);
		}
	else {
		int r;
		glob_t glb;
		char *pat = argpp[0]->d_str;

		glb.gl_pathc = 0;
		if((r = glob(pat,GLOB_MARK,NULL,&glb)) != 0 && r != GLOB_NOMATCH) {
			if(glb.gl_pathc > 0)
				globfree(&glb);
			(void) rcset(errno == GLOB_NOSPACE ? FatalError : Failure,0,text406,pat,strerror(errno));
								// "Glob of pattern \"%s\" failed: %s"
			}
		else
			(void) globcpy(rp,&glb);
		}

	return rc.status;
	}

// Do "pathname" function: return absolute pathname of given file or directory (or an array of absolute pathnames if argument is
// an array).  If n <= 0, symbolic links are not resolved.  Return status.
int aPathname(Datum *rp,int n,Datum **argpp) {

	if(argpp[0]->d_type != dat_blobRef)
		(void) getpath(argpp[0]->d_str,n == INT_MIN || n > 0,rp);
	else {
		Array *aryp0,*aryp;
		Datum *datp,*pathp;

		if(mkarray(rp,&aryp0) == Success) {
			if(dnewtrk(&pathp) != 0)
				goto LibFail;
			aryp = awptr(*argpp)->aw_aryp;
			while((datp = aeach(&aryp)) != NULL) {
				if(!(datp->d_type & DStrMask))
					return rcset(Failure,0,text329,dtype(datp,false));
						// "Unexpected %s argument"
				if(getpath(datp->d_str,n == INT_MIN || n > 0,pathp) != Success)
					break;
				if(apush(aryp0,pathp,true) != 0)
LibFail:
					return librcset(Failure);
				}
			}
		}
	return rc.status;
	}

// Do xPathname function: if default n or n < 0, search for given file or directory in $execPath directories and return pathname
// if found; otherwise, nil.  If n >= 0, search for file(s) and director(ies) in $execPath directories that match given shell
// glob pattern and return a (possibly empty) array of pathnames that match.  Null directories in $execPath are skipped if
// n <= 0.  Also, if the given argument begins with a slash (/) and a glob search is not being performed, it is searched for
// as an absolute pathname, bypassing the directories in $execPath; otherwise, an error is returned.  Return status.
int xPathname(Datum *rp,int n,Datum **argpp) {
	char *myname = (cftab + cf_xPathname)->cf_name;

	if(n < 0) {
		char *result;

		if(pathsearch(argpp[0]->d_str,n == INT_MIN ? 0 : XPSkipNull,(void *) &result,myname) == Success) {
			if(result == NULL)
				dsetnil(rp);
			else if(dsetstr(result,rp) != 0)
				(void) librcset(Failure);
			}
		}
	else {
		glob_t glb;

		if(pathsearch(argpp[0]->d_str,n == 0 ? (XPGlobPat | XPSkipNull) : XPGlobPat,(void *) &glb,myname) == Success)
			(void) globcpy(rp,&glb);
		}
	return rc.status;
	}

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

// Open a file for reading.  If fn is NULL, use standard input (via file descriptor saved in fi.stdinfd).  Return status.
static int ffropen(char *fn,bool required) {

	if(fn == NULL)
		fi.fd = fi.stdinfd;
	else if((fi.fd = open(fn,O_RDONLY)) == -1)
		return (required || errno != ENOENT) ? rcset(Failure,0,text141,strerror(errno),fn) : IONSF;
							// "I/O Error: %s, file \"%s\""
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
	fi.iobufz = fi.iobufc = fi.iobuf;

	return savefname(fn);
	}

// Open a file for writing or appending.  Return status.
static int ffwopen(Buffer *bufp,char *fn,int mode) {

	if((fi.fd = open(fn,O_WRONLY | O_CREAT | (mode == 'w' ? O_TRUNC : O_APPEND),0666)) == -1)
		return rcset(Failure,0,text141,strerror(errno),fn);
				// "I/O Error: %s, file \"%s\""

	// Initialize buffer pointers, record delimiter, and save filename.
	fi.iobufz = (fi.iobufc = fi.iobuf) + sizeof(fi.iobuf);
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
	fi.fd = -1;

	// Free line and filename buffers.
	ffbfree();
	(void) free((void *) fi.fname);
	fi.fname = NULL;

	// Store delimiter(s) used for I/O in buffer record.
	if(!otpfile) {
		char *str = si.curbp->b_inpdelim;

		si.curbp->b_inpdelimlen = 0;
		if(fi.idelim1 != -1) {
			*str++ = (char) fi.idelim1;
			si.curbp->b_inpdelimlen = 1;
			if(fi.idelim2 != -1) {
				*str++ = (char) fi.idelim2;
				si.curbp->b_inpdelimlen = 2;
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
	if(fi.eof)
		goto EOFRetn;

	// Buffer empty?
	if(fi.iobufc == fi.iobufz) {

		// Yep, refill it.
		if((n = read(fi.fd,fi.iobuf,SizeFileBuf)) == -1)
			return rcset(Failure,0,text141,strerror(errno),fi.fname);
					// "I/O Error: %s, file \"%s\""
		// Hit EOF?
		if(n == 0) {

			// Yep, note it...
			fi.eof = true;
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
// errors and return status.  *hasDelimp is not set unless at least one byte was read (so the returned value from a previous call
// will remain valid).
static int ffgetline(uint *lenp,bool *hasDelimp) {
	int status;
	short c;
	uint llen;
	bool hasDelim = false;

	// If we are at EOF, bail out.
	if(fi.eof)
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
	if((llen = fi.lbufc - fi.lbuf) == 0 && fi.eof)

		// Yes.
		return IOEOF;

	// Got a (possibly zero-length) line.  Save the length for the caller and return delimiter status if non-zero.
	if((*lenp = llen) > 0)
		*hasDelimp = hasDelim;

	return rc.status;
	}

// Insert a line into a buffer before point.  Return status.
static int insline(char *src,int len,bool hasDelim,Buffer *bufp,Dot *dotp) {
	Line *lnp;
	int size;

	// Allocate new buffer line for text to be inserted plus a portion of the dot line, depending on dot's position.
	size = len + (hasDelim ? dotp->off : dotp->lnp->l_used);
	if(lalloc(size,&lnp) != Success)
		return rc.status;					// Fatal error.
	if(hasDelim && dotp->off == 0)
		memcpy(lnp->l_text,src,len);				// Copy line.
	else {
		char *str,*strz;
		char *dest = lnp->l_text;
		if(dotp->off > 0) {
			strz = (str = dotp->lnp->l_text) + dotp->off;	// Copy text before dot.
			do
				*dest++ = *str++;
				while(str < strz);
			}
		strz = src + len;					// Copy line.
		while(src < strz)
			*dest++ = *src++;
		if(hasDelim) {
			if(dotp->off == dotp->lnp->l_used)		// Shift any remaining text to BOL.
				dotp->off = dotp->lnp->l_used = 0;	// Nothing left.
			else {
				str = (dest = dotp->lnp->l_text) + dotp->off;
				strz = dotp->lnp->l_text + dotp->lnp->l_used;
				do
					*dest++ = *str++;
					while(str < strz);
				dotp->lnp->l_used -= dotp->off;
				dotp->off = 0;
				}
			}
		else {
			if(dotp->off < dotp->lnp->l_used) {		// Copy remaining text.
				str = dotp->lnp->l_text + dotp->off;
				strz = dotp->lnp->l_text + dotp->lnp->l_used;
				do
					*dest++ = *str++;
					while(str < strz);
				}
			dotp->off += len;
			lreplace1(dotp->lnp,bufp,lnp);			// Replace dot line with new one.
			dotp->lnp = lnp;
			return rc.status;
			}
		}
	llink(lnp,bufp,dotp->lnp);					// Link new line before dot line.
	return rc.status;
	}

// Read lines from a buffer or file and insert at point in target buffer.  Return status.  *np is n value; it is updated to
// number of lines inserted.  If srcbufp is not NULL, lines are "read" from that buffer; otherwise, they are read from current
// (opened) input file, *statusp is set to result, and *finalDelimp is set to true if last line read had a delimiter; otherwise,
// false.  If targdotp is not NULL, target buffer is not marked as changed after lines are inserted; if targdotp is NULL, dot
// from current window is used and (1), inserted lines are marked as a region; (2), target buffer is marked as changed if any
// lines were inserted; and (3), point is moved to beginning of region if *np is zero.
//
// Called by:
//	insertBuf()	inslines(&lineCt,bufp,si.curbp,NULL,NULL,NULL)				insertBuf
//	ifile()		inslines(&lineCt,NULL,si.curbp,NULL,&status,&finalDelim)		insertFile, insertPipe
//	readin()	inslines(&lineCt,NULL,bufp,&bufp->b_face.wf_dot,&status,&finalDelim)	(various)
int inslines(uint *np,Buffer *srcbufp,Buffer *targbufp,Dot *targdotp,int *statusp,bool *finalDelimp) {
	Dot *dotp,dot0;
	Line *dotlnp0;
	int wpos;
	int len;
	uint lineCt = 0;
	bool hasDelim = true;

	// Dot given?
	if(targdotp == NULL) {

		// No, use one from current window and save state of target buffer for region manipulation later.
		dotp = &si.curwp->w_face.wf_dot;
		dot0.lnp = (dotp->lnp == targbufp->b_lnp) ? NULL : dotp->lnp->l_prevp;
		wpos = getwpos(si.curwp);
		}
	else
		// Yes, use it.
		dotp = targdotp;
	dot0.off = dotp->off;
	dotlnp0 = dotp->lnp;

	// Insert each line from the buffer or file before dot line in target buffer.
	if(srcbufp != NULL) {
		Line *buflnp = srcbufp->b_lnp;				// First line of source buffer.
		do {
			hasDelim = (buflnp->l_nextp != NULL);
			if(insline(buflnp->l_text,len = buflnp->l_used,hasDelim,targbufp,dotp) != Success)
				return rc.status;
			++lineCt;
			} while((buflnp = buflnp->l_nextp) != NULL);	// and check for EOB in source buffer.
		}
	else {
		// Read lines from current input file.  If a line is read that has no delimiter, it is the last line of the file
		// (and len > 0).  If dot was not specified, insert line's contents at point via linsert() being that dot is
		// from the current window; otherwise, replace target dot line with line read.  In the latter case, it is
		// assumed that the dot line is last line of the buffer.
		while((*statusp = ffgetline(&len,&hasDelim)) == Success) {
			if(insline(fi.lbuf,len,hasDelim,targbufp,dotp) != Success)
				return rc.status;
			++lineCt;
			}

		// Last read was unsuccessful, check for errors.
		if(*statusp <= FatalError)
			return rc.status;				// Bail out.
		if(*statusp >= Success)
			*statusp = ffclose(false);			// Read was successful; preserve close status.
		else
			(void) ffclose(false);				// Read error; ignore close status.
		*finalDelimp = hasDelim;
		}

	// Update line pointers if last line inserted had no delimiter (causing dot line to be reallocated by insline()).
	if(!hasDelim) {
		if(targbufp->b_nwind > 0)
			fixwface(dot0.off,len,dotlnp0,dotp->lnp);
		fixbface(targbufp,dot0.off,len,dotlnp0,dotp->lnp);
		}

	// All lines inserted.  Set mark RegMark to first inserted line and restore original buffer framing, if applicable.
	if(targdotp == NULL) {
		targbufp->b_mroot.mk_dot.lnp = (dot0.lnp == NULL) ? targbufp->b_lnp : dot0.lnp->l_nextp;
		targbufp->b_mroot.mk_dot.off = dot0.off;
		targbufp->b_mroot.mk_rfrow = wpos;

		// If n is zero, swap point and RegMark.
		if(*np == 0)
			(void) swapmid(RegMark);
		}

	// Update window flags and return line count.
	if(lineCt > 0) {
		bchange(targbufp,WFHard);
		if(targdotp != NULL)
			targbufp->b_flags &= ~BFChanged;
		else if(targbufp == si.curbp && *np != 0) {
			si.curwp->w_rfrow = 0;
			si.curwp->w_flags |= WFReframe;
			}
		}
	*np = lineCt;

	return rc.status;
	}

// Does "fn" exist on disk?  Return -1 (false), 0 (true and not a directory), or 1 (true and is a directory).
int fexist(char *fn) {
	struct stat s;

	return stat(fn,&s) != 0 ? -1 : (s.st_mode & S_IFDIR) ? 1 : 0;
	}

// Flags for iostat().
#define IOS_OtpFile	0x0001		// Writing to a file.
#define IOS_NoDelim	0x0002		// No delimiter at EOF.

// Report I/O results as a return message, given active string-fab object.  Return status.
static int iostat(DStrFab *sfp,ushort flags,Datum *bknamep,int status,char *fname,char *lcmsg,uint lineCt) {
	char *str;

	// Report any non-fatal read or close error.
	if(!(flags & IOS_OtpFile) && status < Success) {
		if(dputf(sfp,text141,strerror(errno),fname == NULL ? "(stdin)" : fname) != 0 || dputc(' ',sfp) != 0)
				// "I/O Error: %s, file \"%s\""
			goto LibFail;
		si.curbp->b_flags |= BFActive;		// Don't try to read file again.
		}

	// Begin wrapped message and report I/O operation.
	if(dputc('[',sfp) != 0 || dputs(lcmsg,sfp) != 0)
		goto LibFail;

	// Report line count.
	if(dputf(sfp," %u %s%s",lineCt,text205,lineCt != 1 ? "s" : "") != 0)
				// "line"
		goto LibFail;

	// Non-standard line delimiter(s)?
	str = (flags & IOS_OtpFile) ? fi.odelim : si.curbp->b_inpdelim;
	if(str[0] != '\0' && (str[0] != '\n' || str[1] != '\0')) {
		if(dputs(text252,sfp) != 0 || dvizs(str,0,VBaseDef,sfp) != 0)
				// " delimited by "
			goto LibFail;
		}

	// Missing line delimiter at EOF?
	if((flags & IOS_NoDelim) && dputs(text291,sfp) != 0)
			// " without delimiter at EOF"
		goto LibFail;

	// Insert?
	if(lcmsg == text154 && dputs(text355,sfp) != 0)
			// " and marked as region"
		goto LibFail;

	// Backup file created on output?
	if(bknamep != NULL) {
		if(dputs(text257,sfp) != 0 || dputs(fbasename(bknamep->d_str,true),sfp) != 0 || dputc('"',sfp) != 0)
				// ", original file renamed to \""
			goto LibFail;
		}

	if(dputc(']',sfp) != 0 || dclose(sfp,sf_string) != 0)
		goto LibFail;
	if(mlerase() != Success)
		return rc.status;
	return rcset(status >= Success ? Success : status,RCNoFormat | RCNoWrap,sfp->sf_datp->d_str);
LibFail:
	return librcset(Failure);
	}

// Prompt for a filename (which cannot be null), using def as default (which may be NULL).  Return status.
int gtfilename(Datum *rp,char *prmt,char *def,uint cflags) {

	if(si.opflags & OpScript)
		(void) funcarg(rp,ArgFirst | ArgNotNull1);
	else {
		TermInp ti = {def,RtnKey,MaxPathname,NULL};
		(void) terminp(rp,prmt,ArgNotNull1 | ArgNil1,Term_C_Filename | cflags,&ti);
		}
	return rc.status;
	}

// Read given file into a buffer (for readFile and readPipe commands).  If default n, use the current buffer.  If not default n,
// create a buffer and either (1), use a scratch name for the buffer and don't save the filename in the buffer header if
// RWScratch flag is set; or (2), derive the buffer name from the filename and save the filename in the buffer header if
// RWScratch flag is not set.  Render buffer and return status.
int readFP(Datum *rp,int n,char *fname,ushort flags) {
	Buffer *bufp;

	if(n == INT_MIN)
		bufp = si.curbp;						// Use current buffer by default.
	else {									// Otherwise...
		if(flags & RWScratch) {						// if scratch buffer requested...
			char bname[MaxBufName + 1];

			if(bscratch(bname,&bufp) != Success)			// get one...
				return rc.status;
			}							// otherwise, create a buffer.
		else if(bfind(fname,BS_Create | BS_Hook | BS_File | BS_Force,0,&bufp,NULL) != Success)
			return rc.status;
		}
	if(!(flags & RWScratch))
		flags |= RWKeep;
	if(readin(bufp,fname,flags | RWStats) != Success || (n == INT_MIN &&	// Read the file and rename current buffer
	 !(flags & RWScratch) && brename(rp,1,si.curbp) != Success))		// if a buffer was not created.
		return rc.status;

	return render(rp,n == INT_MIN ? 1 : n,bufp,				// Render the buffer.
	 n == INT_MIN ? 0 : RendNewBuf | RendNotify);
	}

// Insert file "fname" into the current buffer and set current region to inserted lines.  If n == 0, leave point before the
// inserted lines; otherwise, after.  Return the final status of the read.
int ifile(char *fname,int n) {
	int status;
	DStrFab sf;
	bool finalDelim;
	uint lineCt = n;

	if(dopentrk(&sf) != 0)
		return librcset(Failure);

	// Open the file...
	if(ffropen(fname,true) != Success)
		return rc.status;

	// let user know what's up...
	if(mlputs(MLHome | MLWrap | MLFlush,text153) != Success)
			// "Inserting data..."
		return rc.status;

	// and insert the file.
	if(inslines(&lineCt,NULL,si.curbp,NULL,&status,&finalDelim) == Success)
		(void) iostat(&sf,finalDelim ? 0 : IOS_NoDelim,NULL,status,fname,text154,lineCt);
						// "Inserted"
	return rc.status;
	}

// Insert a file into the current buffer.  If zero argument, leave point before the inserted lines; otherwise, after.  This
// is really easy; all you do it find the name of the file, and call the standard "insert a file into the current buffer" code.
int insertFile(Datum *rp,int n,Datum **argpp) {

	if(gtfilename(rp,text132,NULL,Term_NoDef) != Success || rp->d_type == dat_nil)
			// "Insert file"
		return rc.status;

	// Insert the file.
	(void) ifile(rp->d_str,n);
	dclear(rp);
	return rc.status;
	}

// Check if a buffer's filename matches given filename.  Return status, including NotFound if no match.
static int filecmp(int n,Buffer *bufp,char *fname,EScreen *scrp) {

	if(bufp->b_fname != NULL && strcmp(bufp->b_fname,fname) == 0) {

		// Match found.  Check directory.
		if(*fname == '/' || scrp == NULL || strcmp(scrp->s_wkdir,si.cursp->s_wkdir) == 0) {
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
int fvfile(Datum *rp,int n,bool view) {
	Datum *datp;
	Buffer *bufp;
	Array *aryp;
	EScreen *scrp;
	EWindow *winp;
	int status;
	bool created = false;

	// Get filename.
	if(gtfilename(rp,view ? text134 : text133,NULL,n < 0 || n > 1 ? Term_NoDef : (Term_NoDef | Term_C_NoAuto)) != Success ||
	 rp->d_type == dat_nil)
			// "View file","Find file"
		return rc.status;

	// Check if an existing buffer is attached to the file.
	scrp = si.sheadp;
	do {							// In all screens...
		winp = scrp->s_wheadp;
		do {						// In all windows...
			if((status = filecmp(n,bufp = winp->w_bufp,rp->d_str,scrp)) < NotFound)
				return rc.status;
			if(status == Success)
				goto ViewChk;
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);
	aryp = &buftab;						// Scan buffer list.
	while((datp = aeach(&aryp)) != NULL) {
		bufp = bufptr(datp);
		if(bufp->b_nwind == 0) {			// Skip if already checked.
			if((status = filecmp(n,bufp,rp->d_str,bufp->b_lastscrp)) < NotFound)
				return rc.status;
			if(status == Success)
				goto ViewChk;
			}
		}

	// No buffer found... create one.
	if(bfind(rp->d_str,BS_Create | BS_Hook | BS_File | BS_Force,0,&bufp,&created) != Success)
		return rc.status;

	// Set filename and flag as inactive.
	if(setfname(bufp,rp->d_str,NULL) != Success)
		return rc.status;
	bufp->b_flags = 0;
ViewChk:
	// Enable "read only" mode if view is true.  No need to run filename hook -- it will be run when buffer is activated.
	if(view && bmset(bufp,mi.cache[MdIdxRdOnly],false,NULL) != Success)
		return rc.status;

	return render(rp,n == INT_MIN ? 1 : n,bufp,created ? RendNewBuf : 0);
	}

// Read a file into the given buffer, blowing away any text found there.  If fname is NULL, use the buffer's filename if set;
// otherwise, standard input.  If RWKeep flag is not set, delete the buffer's filename if set; otherwise, save the filename in
// the buffer record (if it is not narrowed) and, if RWNoHooks flag is not set, (a), run the read hook after the file is read;
// and (b), run the filename hook if either the RWFNHook flag is set or the buffer filename was changed.  If RWExist flag is
// set, return an error if the file does not exist.  If RWStats flag set, set a return message containing input statistics.
// Return status, including Cancelled if buffer was not cleared (user did not "okay" it) and IONSF if file does not exist.
int readin(Buffer *bufp,char *fname,ushort flags) {
	EWindow *winp;
	int status;
	uint lineCt;
	bool finalDelim;
	DStrFab sf;
	bool fnChange = false;

	if((flags & RWStats) && dopentrk(&sf) != 0)
		return librcset(Failure);
	if(bclear(bufp,0) != Success)
		return rc.status;					// Error or user got cold feet.
	if(bufp->b_flags & BFNarrowed)					// Mark as changed if narrowed.
		bufp->b_flags |= BFChanged;

	// Determine filename.
	if(fname == NULL)
		fname = bufp->b_fname;
	else if((flags & RWKeep) && !(bufp->b_flags & BFNarrowed) &&
	 setfname(bufp,fname,&fnChange) != Success)			// Save given filename.
		return rc.status;

	// Open the file.
	if((status = ffropen(fname,flags & RWExist)) <= FatalError)
		return rc.status;					// Bail out.
	if(status != Success)
		goto Retn;

	// Let user know what's up.
	if(mlputs(MLHome | MLWrap | MLFlush,text139) != Success)
			// "Reading data..."
		return rc.status;

	// Read the file.
	lineCt = 1;
	if(inslines(&lineCt,NULL,bufp,&bufp->b_face.wf_dot,&status,&finalDelim) != Success)
		return rc.status;

	// Report results.
	if((flags & RWStats) && iostat(&sf,finalDelim ? 0 : IOS_NoDelim,NULL,status,fname,text140,lineCt) != Success)
											// "Read"
		return rc.status;
Retn:
	// Make sure buffer is active.
	bufp->b_flags |= BFActive;

	// Erase filename and run read and/or filename hooks if needed.
	if(!(flags & RWKeep))
		clfname(bufp);
	else if(status >= Success && !(flags & RWNoHooks) && !(bufp->b_flags & BFMacro)) {
		if(exechook(NULL,INT_MIN,hooktab + HkRead,2,bufp->b_bname,bufp->b_fname) <= FatalError ||
		 (((flags & RWFNHook) || fnChange) && fnhook(bufp) <= FatalError))
			return rc.status;
		}

	// Update buffer and window pointers.
	faceinit(&bufp->b_face,bufp->b_lnp,bufp);
	winp = si.wheadp;
	do {
		if(winp->w_bufp == bufp) {
			faceinit(&winp->w_face,bufp->b_lnp,NULL);
			winp->w_flags |= (WFHard | WFMode);
			}
		} while((winp = winp->w_nextp) != NULL);

	// Return status.
	return (status == IONSF) ? rcset(Success,RCNoFormat,text138) : mlerase();
							// "New file"
	}

// Check screens for multiple working directories.  If bufp is NULL, return true if multiple screens exist and two or more have
// different working directories; otherwise, return true if the given buffer is being displayed on at least two screens having
// different working directories.  In the latter case, it is assumed that different directories exist.
static bool wdstat(Buffer *bufp) {
	EScreen *scrp = si.sheadp;
	char *wkdir = NULL;
	if(bufp == NULL) {						// Check for multiple working directories.
		if(scrp->s_nextp == NULL)
			return false;					// Only one screen.
		do {							// In all screens...
			if(wkdir == NULL)
				wkdir = scrp->s_wkdir;
			else if(strcmp(wkdir,scrp->s_wkdir) != 0)
				return true;				// Different directory found.
			} while((scrp = scrp->s_nextp) != NULL);
		}
	else {								// Check if buffer is multi-homed.
		EWindow *winp;

		do {							// In all screens...
			winp = scrp->s_wheadp;				// Buffer on this screen?
			do {
				if(winp->w_bufp == bufp) {
					if(wkdir == NULL)
						wkdir = scrp->s_wkdir;
					else if(strcmp(wkdir,scrp->s_wkdir) != 0)
						return true;		// Different directory found.
					break;
					}
				} while((winp = winp->w_nextp) != NULL);
			} while((scrp = scrp->s_nextp) != NULL);
		}

	return false;
	}

// Check given buffer flags and verify with user before writing to a file.  Return status.
static int bufcheck(Buffer *bufp) {

	// Complain about narrowed buffer.
	if(bufp->b_flags & BFNarrowed) {
		bool yep;
		DStrFab prompt;

		if(dopentrk(&prompt) != 0 ||
		 dputf(&prompt,text147,text308,bufp->b_bname) != 0 || dclose(&prompt,sf_string) != 0)
				// "%s buffer '%s'... write it out","Narrowed"
			(void) librcset(Failure);
		else if(terminpYN(prompt.sf_datp->d_str,&yep) == Success && !yep)
			(void) rcset(UserAbort,0,NULL);
		}

	return rc.status;
	}

// Do appendFile or writeFile command.  Ask for a filename, and write or append the contents of the current buffer to that file.
// Update the remembered filename if writeFile and return status.
int awfile(Datum *rp,int n,char *prmt,int mode) {

	// Get the filename.
	if(gtfilename(rp,prmt,NULL,Term_C_NoAuto) != Success || rp->d_type == dat_nil)
		return rc.status;

	// Complain about narrowed buffers.
	if(bufcheck(si.curbp) != Success)
		return rc.status;

	// It's a go... write the file.
	if(writeout(si.curbp,rp->d_str,mode,RWStats) != Success)
		return rc.status;

	// Update filename and buffer name if write (not append).
	if(mode == 'w') {
		bool fnChange;

		if(setfname(si.curbp,rp->d_str,&fnChange) == Success) {
			if(((n != INT_MIN && n <= 0) || brename(NULL,1,si.curbp) == Success) && fnChange)
				(void) fnhook(si.curbp);
			}
		}
	dclear(rp);

	return rc.status;
	}

// Save given buffer to disk if possible.  *wkdirp contains current directory and is updated if directory is changed.
static int savebuf(Buffer *bufp,ushort flags,char **wkdirp,char *wkdir) {

	bufp->b_flags &= ~BFQSave;					// Clear "saved" flag.
	if(!(bufp->b_flags & BFChanged) ||				// If not changed...
	 ((bufp->b_flags & BFMacro) && (flags & SVBAll)))		// or a macro and saving all...
		return Cancelled;					// skip it.
	if(bufp->b_fname == NULL)					// Must have a filename.
		return rcset(Failure,0,text145,bufp->b_bname);
			// "No filename associated with buffer '%s'"
	if(bufcheck(bufp) != Success)					// Complain about narrowed buffers.
		return rc.status;

	// Change working directory if needed.
	if(*bufp->b_fname != '/') {					// If relative pathname...
		if(wkdir == NULL) {					// buffer is in the background...
			if(bufp->b_lastscrp == NULL) {			// and "last screen" pointer not set...
				bool yep;				// get permission from user to use current screen's
				DStrFab prompt;				// directory.

				if(dopentrk(&prompt) != 0 || dputf(&prompt,text147,text309,bufp->b_bname) != 0 ||
								// "%s buffer '%s'... write it out","Directory unknown for"
				 dclose(&prompt,sf_string) != 0)
					return librcset(Failure);
				if(terminpYN(prompt.sf_datp->d_str,&yep) != Success)
					return rc.status;
				if(!yep)				// Permission given?
					return rcset(UserAbort,0,NULL);	// No, user abort.
				wkdir = si.cursp->s_wkdir;			// Yes, use current directory.
				bufp->b_lastscrp = si.cursp;
				}
			else
				wkdir = bufp->b_lastscrp->s_wkdir;	// Have "last screen" pointer... use it.
			}
		else if((flags & SVBMultiDir) && wdstat(bufp))		// Multi-homed buffer (in foreground)?
			return rcset(Failure,0,text310,bufp->b_bname);	// Yes, error.
				// "Cannot save multi-homed buffer '%s'"

		// It's a go... change directory if different.
		if(strcmp(*wkdirp,wkdir) != 0 && chgdir(*wkdirp = wkdir) != Success)
			return rc.status;
		}

	// Save the buffer.
	if(mlprintf(MLHome | MLWrap | MLFlush,text103,bufp->b_fname) != Success)
			// "Saving %s..."
		return rc.status;
	if((flags & (SVBAll | SVBQExit)) == SVBAll)
		cpause(50);
	if(writeout(bufp,bufp->b_fname,'w',RWStats) != Success)
		return rc.status;
	si.gacount = si.gasave;					// Reset $autoSave...
	if(flags & SVBQExit)
		bufp->b_flags |= BFQSave;			// and mark as saved for main() to display.
	return rc.status;
	}

// Save the contents of the current buffer (or all buffers if n > 0) to their associated files.  Do nothing if nothing has
// changed.  Error if there is no filename set for a buffer.  If SVBQExit flag is set, marked saved buffers for quickExit
// command.
int savebufs(int n,ushort flags) {
	Datum **blp,**blpz;
	Buffer *bufp;
	int status;
	char *wkdir = si.cursp->s_wkdir;
	uint scount = 0;			// Count of saved buffers.

	if(n > 0)
		flags |= SVBAll;
	if(wdstat(NULL))
		flags |= SVBMultiDir;

	// Search for changed buffers one screen at a time if updating all to simplify tracking and minimize frequency of
	// chdir() calls when multiple screen directories exist.
	if(flags & SVBAll) {
		EWindow *winp;
		EScreen *scrp = si.sheadp;
		do {						// In all screens...
			winp = scrp->s_wheadp;
			do {					// In all windows...
				if((status = savebuf(winp->w_bufp,flags,&wkdir,scrp->s_wkdir)) <= UserAbort)
					return rc.status;
				if(status == Success)
					++scount;
				} while((winp = winp->w_nextp) != NULL);
			} while((scrp = scrp->s_nextp) != NULL);
		}

	// Scan buffer list for changed buffers.
	blpz = (blp = buftab.a_elpp) + buftab.a_used;
	bufp = (flags & SVBAll) ? bufptr(*blp) : si.curbp;
	for(;;) {
		// Skip any buffer being displayed if screen traversal was done so that any multi-homed buffers that were
		// skipped (and counted) will be skipped here also.
		if(!(flags & SVBAll) || bufp->b_nwind == 0) {
			if((status = savebuf(bufp,flags,&wkdir,bufp == si.curbp ? si.cursp->s_wkdir : NULL)) <= UserAbort)
				return rc.status;
			if(status == Success)
				++scount;
			}
		if(!(flags & SVBAll) || ++blp == blpz)
			break;
		bufp = bufptr(*blp);
		}

	// Return to original working directory if needed and set return message.
	if(strcmp(wkdir,si.cursp->s_wkdir) != 0)
		(void) chgdir(si.cursp->s_wkdir);
	if((flags & (SVBAll | SVBQExit)) == SVBAll && rc.status == Success) {
		(void) rcset(Success,RCForce,text167,scount,scount != 1 ? "s" : "");
					// "%u buffer%s saved"
		}
	return rc.status;
	}

// Write a buffer to disk, given buffer, filename and mode ('w' = write, 'a' = append) and return status.  Before the file is
// written, a user routine (write hook) can be run.  The number of lines written, and possibly other status information is
// displayed.  If "ATerm" mode is enabled and the buffer does not have a terminating line delimiter, one is added before the
// buffer is written.  If "Safe" or "Bak" mode is enabled, the buffer is written out to a temporary file, the old file is
// unlinked or renamed to "<file>.bak" (if "<file>.bak" does not already exist), and the temporary is renamed to the original.
int writeout(Buffer *bufp,char *fn,int mode,ushort flags) {
	Line *lnp,*lnpz;
	uint lineCt;			// Number of lines written.
	int status;			// Return status.
	ushort sflag = 0;		// Are we safe saving? (0x01 bit = 'safe' save, 0x02 bit = 'bak' save)
	DStrFab sf;
	struct stat st;			// File permision info.
	Datum *tnamep;			// Temporary filename.
	Datum *bknamep;			// Backup filename.

	if(dopentrk(&sf) != 0)
		goto LibFail;

	// Run user-assigned write hook.
	if(!(bufp->b_flags & (BFHidden | BFMacro)) && exechook(NULL,INT_MIN,hooktab + HkWrite,2,bufp->b_bname,fn) != Success)
		return rc.status;

	// Determine if we will use the "safe save" method.
	if((modeset(MdIdxBak,bufp) || modeset(MdIdxSafe,bufp)) && fexist(fn) >= 0 && mode == 'w') {
		if(modeset(MdIdxBak,bufp)) {

			// 'bak' mode enabled.  Create backup version of filename.
			if(dnewtrk(&bknamep) != 0 || dsalloc(bknamep,strlen(fn) + strlen(Backup_Ext) + 1) != 0)
				goto LibFail;
			sprintf(bknamep->d_str,"%s%s",fn,Backup_Ext);

			// Enable 'bak' save if backup file does not already exist.
			if(fexist(bknamep->d_str) < 0)
				sflag = 0x02;
			}
		if(modeset(MdIdxSafe,bufp))
			sflag |= 0x01;
		}

	// Open output file.
	if(sflag) {
		char *suffix;
		size_t len;

		// Duplicate original file pathname and keep first letter of filename.
		len = fbasename(fn,true) - fn + 1;
		if(dnewtrk(&tnamep) != 0 || dsalloc(tnamep,len + 6) != 0)
			goto LibFail;
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
	if(status != Success || mlputs(MLHome | MLWrap | MLFlush,text148) != Success)
					// "Writing data..."
		return rc.status;

	// Append a line delimiter to the buffer if needed.
	lnpz = (lnp = bufp->b_lnp)->l_prevp;
	if(lnpz->l_used > 0 && modeset(MdIdxATerm,bufp)) {
		Line *lnp0;

		if(lalloc(0,&lnp0) != Success)		// Empty line.
			return rc.status;		// Fatal error.
		llink(lnp0,bufp,NULL);			// Append empty line (newline) to end of buffer...
		bchange(bufp,WFHard);			// set "change" flags...
		lnpz = lnp->l_prevp;			// and update "last line" pointer.
		}

	// Write the current buffer's lines to the open disk file.
	lineCt = 0;
	while(lnp != lnpz || lnp->l_used > 0) {
		if(ffputline(lnp,lnp != lnpz) != Success) {

			// Write or close error: clean up and get out.
			(void) ffclose(true);
			goto Retn;
			}
		++lineCt;
		if((lnp = lnp->l_nextp) == NULL)
			break;
		}

	// Write was successful: clear "changed" flag, update window flags, and close output file.
	bufp->b_flags &= ~BFChanged;
	supd_wflags(bufp,WFMode);
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
					// "I/O Error: %s, file \"%s\"","(file saved as \"%s\") "
				goto LibFail;
			status = Failure;	// Failed.
			}
		}

	// Report lines written and return status.
	return (flags & RWStats) ?
	 iostat(&sf,lineCt > 0 && bufp->b_lnp->l_prevp->l_used > 0 ? (IOS_OtpFile | IOS_NoDelim) : IOS_OtpFile,
	  status == Success && (sflag & 0x02) ? bknamep : NULL,status,fn,text149,lineCt) : rc.status;
									// "Wrote"
LibFail:
	return librcset(Failure);
	}

// Set the filename associated with a buffer and call the read hook if appropriate.  If default n, use current buffer for target
// buffer and first argument for filename; otherwise, use first argument for target buffer and second argument for filename.  If
// buffer name is nil, use current buffer.  If the new filename is nil or null, set the filename to null; otherwise, if n > 0 or
// is the default, change the buffer name to match the filename.  rp is set to a two-element array containing new buffer name
// and new filename.  Return status.
int setBufFile(Datum *rp,int n,Datum **argpp) {
	Buffer *bufp;
	bool fnChange;

	// Get buffer.
	if(n == INT_MIN)
		bufp = si.curbp;
	else if(si.opflags & OpScript) {
		if(!strval(argpp[0]))
			return rc.status;
		else if((bufp = bsrch(argpp[0]->d_str,NULL)) == NULL)
			return rcset(Failure,0,text118,argpp[0]->d_str);
				// "No such buffer '%s'"
		++argpp;
		}
	else if(bcomplete(rp,text151,(bufp = bdefault(BDefTwo)) != NULL ? bufp->b_bname : NULL,OpDelete,&bufp,NULL) != Success
	 || bufp == NULL)
			// "Set filename in"
		return rc.status;

	// We cannot modify an executing buffer.
	if(bufp->b_mip != NULL && bufp->b_mip->mi_nexec > 0)
		return rcset(Failure,0,text284,text276,text248);
			// "Cannot %s %s buffer","modify","an executing"

	// Get filename.
	if(si.opflags & OpScript)
		datxfer(rp,*argpp);
	else {
		TermInp ti = {bufp->b_fname,RtnKey,MaxPathname,NULL};
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
		if(terminp(rp,str,ArgNil1,Term_C_Filename | Term_C_NoAuto,&ti) != Success)
			return rc.status;
		}

	// Save new filename (which may be null or nil).
	if(rp->d_type == dat_nil)
		dsetnull(rp);
	if(setfname(bufp,rp->d_str,&fnChange) != Success)
		return rc.status;

	// Change buffer name if applicable.
	if(bufp->b_fname != NULL && (n == INT_MIN || n > 0) && brename(NULL,1,bufp) != Success)
		return rc.status;

	// Set rp to results (an array).
	Array *aryp;
	if((aryp = anew(2,NULL)) == NULL || dsetstr(bufp->b_bname,aryp->a_elpp[0]) != 0)
		goto LibFail;
	if(*rp->d_str == '\0')
		dsetnil(aryp->a_elpp[1]);
	else if(dsetstr(rp->d_str,aryp->a_elpp[1]) != 0)
LibFail:
		return librcset(Failure);
	if(awrap(rp,aryp) != Success)
		return rc.status;

	// Update mode lines and run filename hook.
	supd_wflags(bufp,WFMode);
	return (bufp->b_flags & BFMacro) || !fnChange ? rc.status : fnhook(bufp);
	}
