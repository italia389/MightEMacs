// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// fileio.c	Low-level file I/O routines for MightEMacs.
//
// These routines read and write ASCII files from and to disk.  All of the knowledge about files is here.

#include "os.h"
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

// Save given filename.
static int savefname(char *fn) {

	// Don't bother if it's already saved.
	if(fn == NULL || fi.fname == NULL || strcmp(fn,fi.fname) != 0) {
		if(fi.fname != NULL)
			(void) free((void *) fi.fname);
		if(fn == NULL)
			fn = "<stdin>";
		if((fi.fname = (char *) malloc(strlen(fn) + 1)) == NULL)
			return rcset(PANIC,0,text155,"savefname",fn);
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
		return (required || errno != ENOENT) ? rcset(FAILURE,0,text141,strerror(errno),fn) : IONSF;
							// "I/O ERROR: %s, file \"%s\""

	// Create initial line buffer.
	ffbfree();
	if((fi.lbuf = (char *) malloc(LINE_BUFSIZE)) == NULL)
		return rcset(PANIC,0,text94,"ffropen");
				// "%s(): Out of memory!"

	// Clear EOF flag, initialize buffer pointers, and save filename.
	fi.eof = false;
	fi.lbufz = fi.lbuf + LINE_BUFSIZE;
	if((fi.idelim1 = (int) fi.inpdelim[0]) == 0)
		fi.idelim1 = fi.idelim2 = -1;
	else if((fi.idelim2 = (int) fi.inpdelim[1]) == 0)
		fi.idelim2 = -1;
	fi.iobufz = fi.iobufp = fi.iobuf;

	return savefname(fn);
	}

// Open a file for writing or appending.  Return status.
int ffwopen(char *fn,int mode) {

	if((fi.fd = open(fn,O_WRONLY | O_CREAT | (mode == 'w' ? O_TRUNC : O_APPEND),0666)) == -1)
		return rcset(FAILURE,0,text141,strerror(errno),fn);
				// "I/O ERROR: %s, file \"%s\""

	// Initialize buffer pointers, record delimiter, and save filename.
	fi.iobufz = (fi.iobufp = fi.iobuf) + sizeof(fi.iobuf);
	if(*fi.otpdelim != '\0') {			// Use user-assigned output delimiter if specified.
		fi.odelim = fi.otpdelim;
		fi.odelimlen = fi.otpdelimlen;
		}
	else if(*curbp->b_inpdelim != '\0') {		// Otherwise, use input delimiter(s).
		fi.odelim = curbp->b_inpdelim;
		fi.odelimlen = curbp->b_inpdelimlen;
		}
	else {						// Default to a newline.
		fi.odelim = "\n";
		fi.odelimlen = 1;
		}
	return savefname(fn);
	}

// Flush I/O buffer to output file.  Return status.
static int ffflush(void) {

	if(write(fi.fd,fi.iobuf,fi.iobufp - fi.iobuf) == -1)
		return rcset(FAILURE,0,text141,strerror(errno),fi.fname);
				// "I/O ERROR: %s, file \"%s\""
	fi.iobufp = fi.iobuf;
	return rc.status;
	}

// Close current file, reset file information, and note buffer delimiters.
int ffclose(bool otpfile) {

	// Flush output if needed and close file.
	if(otpfile && fi.iobufp > fi.iobuf && ffflush() != SUCCESS)
		(void) close(fi.fd);
	else if(close(fi.fd) == -1)
		(void) rcset(FAILURE,0,text141,strerror(errno),fi.fname);
				// "I/O ERROR: %s, file \"%s\""
	fi.fd = -1;

	// Free line and filename buffers.
	ffbfree();
	(void) free((void *) fi.fname);
	fi.fname = NULL;

	// Store delimiter(s) used for I/O in buffer record.
	if(otpfile)
		strcpy(curbp->b_otpdelim,fi.odelim);
	else {
		char *strp = curbp->b_inpdelim;

		curbp->b_inpdelimlen = 0;
		if(fi.idelim1 != -1) {
			*strp++ = (char) fi.idelim1;
			curbp->b_inpdelimlen = 1;
			if(fi.idelim2 != -1) {
				*strp++ = (char) fi.idelim2;
				curbp->b_inpdelimlen = 2;
				}
			}
		*strp = '\0';
		}

	return rc.status;
	}

// Write bytes to the current (already opened) file with buffering.  Return status.
static int ffwrite(char *buf,int buflen) {

	// Time for buffer flush?
	if(fi.iobufp > fi.iobuf && fi.iobufp + buflen > fi.iobufz) {

		// Yes, flush it.
		if(ffflush() != SUCCESS)
			return rc.status;
		}

	// Will string fit in buffer?
	if(fi.iobufp + buflen <= fi.iobufz) {

		// Yes, store it.
		memcpy(fi.iobufp,buf,buflen);
		fi.iobufp += buflen;
		}
	else {
		// Won't fit.  Write it directly.
		if(write(fi.fd,buf,buflen) == -1)
			return rcset(FAILURE,0,text141,strerror(errno),fi.fname);
					// "I/O ERROR: %s, file \"%s\""
		}

	return rc.status;
	}

// Write given line to the current (already opened) file.  Return status.
int ffputline(char *buf,int buflen) {

	// Write the line and appropriate line terminator(s), and check for errors.
	if(ffwrite(buf,buflen) == SUCCESS)
		(void) ffwrite(fi.odelim,fi.odelimlen);

	return rc.status;
	}

// Add a character to fi.lbuf, expanding it as needed.  Return status.
static int ffputc(int c) {
	uint n1,n2;

	// Line buffer full?
	if(fi.lbufp == fi.lbufz) {

		// Yes, get more space.
		n1 = n2 = fi.lbufp - fi.lbuf;
		if((fi.lbuf = (char *) realloc((void *) fi.lbuf,n2 *= 2)) == NULL)
			return rcset(PANIC,0,text156,"ffputc",n2,fi.fname);
				// "%s(): Out of memory allocating %u-byte line buffer for file \"%s\"!"
		fi.lbufz = (fi.lbufp = fi.lbuf + n1) + n1;
		}

	// Append character and return rc.status;
	*fi.lbufp++ = c;
	return rc.status;
	}

// Get next character from (opened) input file and store in *strp.  Return status, including IOEOF if end of file reached.
static int ffgetc(int *strp) {
	int n;

	// At EOF?
	if(fi.eof)
		goto eofretn;

	// Buffer empty?
	if(fi.iobufp == fi.iobufz) {

		// Yep, refill it.
		if((n = read(fi.fd,fi.iobuf,FILE_BUFSIZE)) == -1)
			return rcset(FAILURE,0,text141,strerror(errno),fi.fname);
					// "I/O ERROR: %s, file \"%s\""
		// Hit EOF?
		if(n == 0) {

			// Yep, note it...
			fi.eof = true;
eofretn:
			// and return it.
			*strp = -1;
			return IOEOF;
			}

		// No, update buffer pointers.
		fi.iobufz = (fi.iobufp = fi.iobuf) + n;
		}

	*strp = (int) *fi.iobufp++;
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
	fi.lbufp = fi.lbuf;

	// Input line delimiters undefined?
	if(fi.idelim1 == -1) {

		// Yes.  Read bytes until a NL or CR found, or EOF reached.
		for(;;) {
			if((status = ffgetc(&c)) == IOEOF)
				goto eol;
			if(status != SUCCESS)
				return rc.status;
			if(c == '\n') {
				fi.idelim1 = '\n';
				fi.idelim2 = -1;
				goto eol;
				}
			if(c == '\r') {
				fi.idelim1 = '\r';
				if((status = ffgetc(&c)) == IOEOF)
					goto eol;
				if(status != SUCCESS)
					return rc.status;
				if(c == '\n') {
					fi.idelim2 = '\n';
					goto eol;
					}

				// Not a CR-LF sequence; "unget" the last character.
				--fi.iobufp;
				fi.idelim2 = -1;
				goto eol;
				}

			// No NL or CR found yet ... onward.
			if(ffputc(c) != SUCCESS)
				return rc.status;
			}
		}

	// We have line delimiter(s) ... read next line.
	for(;;) {
		if((status = ffgetc(&c)) == IOEOF)
			break;
		if(status != SUCCESS)
			return rc.status;
		if(c == fi.idelim1) {

			// Got match on first delimiter.  Need two?
			if(fi.idelim2 == -1)

				// Nope, we're done.
				break;

			// Yes, check next character.
			if((status = ffgetc(&c)) == IOEOF)
				break;
			if(status != SUCCESS)
				return rc.status;
			if(c == fi.idelim2)

				// Second delimiter matches also, we're done.
				break;

			// Not a match; "unget" the last character.
			--fi.iobufp;
			c = fi.idelim1;
			}

		// Delimiter not found, or two delimiters needed, but only one found.  Save the character and move onward.
		if(ffputc(c) != SUCCESS)
			return rc.status;
		}
eol:
	// Hit EOF and nothing read?
	if((llen = fi.lbufp - fi.lbuf) == 0U && fi.eof)

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
