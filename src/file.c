// (c) Copyright 2026 Richard W. Marinelli
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
//	findViewFile(), ifile(), readFile(), readIn()
// Output:
//	appendFile, saveFile, writeFile
//	appendWriteFile(), saveBuf(), saveBufs(), writeOut()
// Filename management:
//	setBufFile
//	getFilename(), setFilename()
//
// CORE INPUT FUNCTIONS
//	findFile command
//		* call findViewFile()
//	findViewFile(Datum *pRtnVal, long n, bool view)
//		* get a filename
//		* check if file is attached to an existing buffer
//		* if so, either return (do nothing) or switch to buffer
//		* if buffer not found, create one, set filename, (optionally) set buffer to "read-only", and call render()
//		=> CALLED BY execCmdFunc() for findFile and viewFile commands.
//	f_ropen(char *filename, long n)
//		* insert given file into current buffer before current line, optionally leaving point before inserted text
//		=> CALLED BY insertFile(), insertPipe()
//	insertFile() -- command routine
//		* get a filename and call f_ropen()
//	readFile(Datum *pRtnVal, long n, char **args)
//		* get a filename
//		* create new buffer with name derived from filename if requested
//		* read given file into current buffer or new buffer via readIn()
//		* call render()
//	readIn(Buffer *pBuf, char *filename, ushort flags)
//		* clear given buffer and read a file into it (if filename is NULL, use buffer's filename, otherwise stdin)
//		* if RWKeep flag is set, save filename and call read hook; otherwise, delete buffer's filename
//		* if RWExist flag is set, return error if file does not exist
//		=> CALLED BY bactivate(), execFile(), doPop(), readFile()
//	viewFile command
//		* call findViewFile()

#include "std.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <glob.h>
#include "exec.h"
#include "bind.h"

// Make selected global definitions local.
#define FileData
#include "file.h"

static const char stdinFilename[] = "<stdin>";

// Copy glob() results to a new array.  Return status.
static int globtoa(Datum *pRtnVal, glob_t *pGlob) {
	Array *pArray;

	if(makeArray(pRtnVal, 0, &pArray) != Success)
		return sess.rtn.status;
	if(pGlob->gl_pathc > 0) {
		Datum *pDatum;
		char **pPath = pGlob->gl_pathv;
		do {
			if((pDatum = dnew()) == NULL || dsetstr(*pPath, pDatum) != 0 || apush(pArray, pDatum, OpAdopt) != 0)
				return libfail();
			} while(*++pPath != NULL);
		}
	globfree(pGlob);
	return sess.rtn.status;
	}

// Do "glob" function: return a (possibly empty) array of file(s) and/or director(ies) that match given shell glob pattern(s)
// (any of which may contain braces).  Return status.
int globPat(Datum *pRtnVal, long n, Datum **args) {
	Array *pArray;
	Datum *pDatum;
	char *pat;
	glob_t globObj;
	int r;
	uint argFlags = ArgFirst;
	uint globFlags = GLOB_MARK | GLOB_NOSORT | GLOB_BRACE | GLOB_TILDE;

	if((pDatum = dnewtrack()) == NULL)
		goto LibFail;

	// Loop through arguments, collecting pathnames.
	do {
		if(funcArg(pDatum, argFlags) != Success)
			goto Retn;
		pat = pDatum->str;
		if((r = glob(pat, globFlags, NULL, &globObj)) != 0 && r != GLOB_NOMATCH) {
			globfree(&globObj);
			(void) rsset(errno == GLOB_NOSPACE ? FatalError : Failure, 0, text406, pat, strerror(errno));
								// "Glob of pattern \"%s\" failed: %s"
			}
		argFlags = 0;
		if(globObj.gl_pathc > 0)
			globFlags |= GLOB_APPEND;
		} while(haveSym(s_comma, false));

	// Transfer results to an array.
	if(globtoa(pRtnVal, &globObj) != Success)
		goto Retn;

	// Remove duplicates, sort results, and return.
	pArray = dgetarrayptr(pRtnVal);
	if(pArray->used > 0)
		if(auniq(pArray, NULL, OpInPlace) == NULL || asort(pArray, acmp, OpInPlace) == NULL)
LibFail:
			return libfail();
Retn:
	return sess.rtn.status;
	}

// Do "pathname" function: return absolute pathname of given file or directory (or an array of absolute pathnames if argument is
// an array).  If n <= 0, symbolic links are not resolved.  Return status.
int absPathname(Datum *pRtnVal, long n, Datum **args) {

	if(!dtyparray(args[0]))
		(void) absPath(args[0]->str, n == DefN || n > 0, pRtnVal);
	else {
		ArrayEach each;
		Array *pArray0;
		Datum *pArrayEl, *path, *expandedPath;

		if(makeArray(pRtnVal, 0, &pArray0) == Success) {
			if((path = dnewtrack()) == NULL || (expandedPath = dnewtrack()) == NULL)
				goto LibFail;
			aeachinit(&each, dgetarrayptr((*args)));
			while((pArrayEl = aeach(&each)) != NULL) {
				if(!dtypstr(pArrayEl))
					return rsset(Failure, 0, text329, dtype(pArrayEl, false));
						// "Unexpected %s argument"
				if(expandPath(expandedPath, pArrayEl) != Success ||
				 absPath(expandedPath->str, n == DefN || n > 0, path) != Success)
					break;
				if(apush(pArray0, path, OpXfer) != 0)
LibFail:
					return libfail();
				}
			}
		}
	return sess.rtn.status;
	}

// Do xPathname function.  Return status.
int xPathname(Datum *pRtnVal, long n, Datum **args) {
	CmdFunc *pCmdFunc = cmdFuncTable + cf_xPathname;
	const char *myName = pCmdFunc->name;
	uint flags = pCmdFunc->pOptHdr->selFlags;

	if(flags & XP_GlobPat) {
		glob_t globObj;

		if(pathSearch(args[0]->str, flags, (void *) &globObj, myName) == Success)
			(void) globtoa(pRtnVal, &globObj);
		}
	else {
		char *result;

		if(pathSearch(args[0]->str, flags, (void *) &result, myName) == Success) {
			if(result == NULL)
				dsetnil(pRtnVal);
			else if(dsetstr(result, pRtnVal) != 0)
				(void) libfail();
			}
		}
	return sess.rtn.status;
	}

// Save given filename.
static void saveFilename(const char *filename) {

	fileInfo.filename = (filename == NULL) ? stdinFilename : filename;
	}

// Free input line buffer.
static void f_free(void) {

	if(fileInfo.lineBuf != NULL) {
		free((void *) fileInfo.lineBuf);
		fileInfo.lineBuf = NULL;
		}
	}

// Initialize FileInfo object for input.  Return status.
int inpInit(int fileHandle) {

	fileInfo.fileHandle = fileHandle;

	// Create initial line buffer.
	f_free();
	if((fileInfo.lineBuf = (char *) malloc(FILineBufSize)) == NULL)
		return rsset(Panic, 0, text94, "inpInit");
				// "%s(): Out of memory!"

	// Set EOF flag, initialize buffer pointers, and set actual line delimiter(s).
	fileInfo.flags = fileInfo.fileHandle < 0 ? FIEOF : 0;
	fileInfo.lineBufEnd = fileInfo.lineBuf + FILineBufSize;
	fileInfo.ioBufEnd = fileInfo.ioBufCur = fileInfo.dataBuf;
	if(fileInfo.userInpDelimSym == FDelimUndef)
		fileInfo.inpDelim1 = fileInfo.inpDelim2 = -1;
	else {
		fileInfo.inpDelim1 = fileInfo.userInpDelimSym == FDelimNL ? '\n' :
		 fileInfo.userInpDelimSym == FDelimNUL ? '\0' : '\r';
		fileInfo.inpDelim2 = fileInfo.userInpDelimSym == FDelimCRLF ? '\n' : -1;
		}

	return sess.rtn.status;
	}

// Open a file for reading.  If filename is NULL, use standard input (via file descriptor saved in fileInfo.stdInpFileHandle).
// Return status.
static int f_ropen(const char *filename, bool required) {
	int fileHandle;

	if(filename == NULL)
		fileHandle = fileInfo.stdInpFileHandle;
	else if((fileHandle = open(filename, O_RDONLY)) == -1)
		return (required || errno != ENOENT) ? rsset(Failure, RSHigh, text141, strerror(errno), filename) : IONSF;
							// "I/O Error: %s, file \"%s\""
	saveFilename(filename);
	return inpInit(fileHandle);
	}

// Set output delimiter(s) per symbol.
static void f_setOtpDelim(ushort sym) {

	fileInfo.pOtpDelim = sym == FDelimNL ? "\n" : sym == FDelimCRLF ? "\r\n" : sym == FDelimCR ? "\r" : "";
	fileInfo.otpDelimLen = sym == FDelimCRLF ? 2 : 1;
	}

// Initialize FileInfo object for output.
void otpInit(Buffer *pBuf, int fileHandle) {

	fileInfo.fileHandle = fileHandle;
	fileInfo.flags = 0;

	// Initialize buffer pointers and line delimiter.
	fileInfo.ioBufEnd = (fileInfo.ioBufCur = fileInfo.dataBuf) + sizeof(fileInfo.dataBuf);
	if(fileInfo.userOtpDelimSym != FDelimUndef)		// Use user-assigned output delimiter if specified...
		f_setOtpDelim(fileInfo.userOtpDelimSym);
	else if(pBuf->inpDelimSym != FDelimUndef)		// otherwise, buffer's input delimiter(s)...
		f_setOtpDelim(pBuf->inpDelimSym);
	else {							// otherwise, newline.
		fileInfo.pOtpDelim = "\n";
		fileInfo.otpDelimLen = 1;
		}
	}

// Open a file for writing or appending.  Return status.
static int f_wopen(Buffer *pBuf, const char *filename, short fmode) {
	int fileHandle;

	if((fileHandle = open(filename, O_WRONLY | O_CREAT | (fmode == 'w' ? O_TRUNC : O_APPEND), 0666)) == -1)
		return rsset(Failure, RSHigh, text141, strerror(errno), filename);
				// "I/O Error: %s, file \"%s\""
	saveFilename(filename);
	otpInit(pBuf, fileHandle);
	return sess.rtn.status;
	}

// Perform I/O on a file or pipe, with retries if applicable.  Return value returned from read() or write() call.
static int f_io(bool doRead, char *iobuf, size_t size) {
	int byteCount;
	int pipeDelay = FIPipeDelay;
	int loopCount = fileInfo.flags & FIRetry ? FIPipeAttempts : 1;
	ssize_t (*iofunc)(int fileHandle, void *iobuf, size_t size) = doRead ? read : write;

	for(;;) {
		if((byteCount = iofunc(fileInfo.fileHandle, iobuf, size)) != -1)
			break;

		// Read or write failed.  Set exception and return if unexpected error or maximum retries reached.
		if(errno != EAGAIN || --loopCount == 0) {
			(void) rsset(Failure, RSHigh, text141, strerror(errno), fileInfo.filename);
					// "I/O Error: %s, file \"%s\""
			break;
			}

		// Pause and try again.
		centiPause(pipeDelay++);
		}

	return byteCount;
	}

// Return delimiter symbol, given two delimiters.
static ushort delimCharsToSym(ushort delim1, ushort delim2) {

	switch(delim1) {
		case '\n':
			return FDelimNL;
		case '\r':
			return delim2 == '\n' ? FDelimCRLF : FDelimCR;
		case '\0':
			return FDelimNUL;
		}
	return FDelimUndef;
	}

// Flush I/O buffer to output file.  Return status.
static int f_flush(void) {

	if(f_io(false, fileInfo.dataBuf, fileInfo.ioBufCur - fileInfo.dataBuf) >= 0)
		fileInfo.ioBufCur = fileInfo.dataBuf;
	return sess.rtn.status;
	}

// Close current file, reset file information, and note buffer delimiters.
static int f_close(bool otpFile) {

	// Flush output if needed and close file.
	if(otpFile && fileInfo.ioBufCur > fileInfo.dataBuf && f_flush() != Success)
		(void) close(fileInfo.fileHandle);
	else if(fileInfo.fileHandle >= 0 && close(fileInfo.fileHandle) == -1)
		(void) rsset(Failure, RSHigh, text141, strerror(errno), fileInfo.filename);
				// "I/O Error: %s, file \"%s\""

	// Free line buffer and reset controls.
	f_free();
	fileInfo.fileHandle = -1;
	fileInfo.filename = NULL;

	// Store type of input delimiter used during read in buffer record (which can't be undefined).
	if(!otpFile)
		sess.cur.pBuf->inpDelimSym = delimCharsToSym(fileInfo.inpDelim1, fileInfo.inpDelim2);

	return sess.rtn.status;
	}

// Write bytes to the current (already opened) file with buffering.  Return status.
static int f_write(const char *iobuf, int bufLen) {

	// Time for buffer flush?
	if(fileInfo.ioBufCur > fileInfo.dataBuf && fileInfo.ioBufCur + bufLen > fileInfo.ioBufEnd) {

		// Yes, flush it.
		if(f_flush() != Success)
			return sess.rtn.status;
		}

	// Will string fit in buffer?
	if(fileInfo.ioBufCur + bufLen <= fileInfo.ioBufEnd) {

		// Yes, store it.
		memcpy(fileInfo.ioBufCur, iobuf, bufLen);
		fileInfo.ioBufCur += bufLen;
		}
	else
		// Won't fit.  Write it directly.
		(void) f_io(false, (char *) iobuf, bufLen);

	return sess.rtn.status;
	}

// Write given line to the current (already opened) file.  Return status.
static int f_putline(Line *pLine, bool writeDelim) {

	// Write the line and appropriate line delimiter(s), and check for errors.
	if(f_write(pLine->text, pLine->used) == Success && writeDelim)
		(void) f_write(fileInfo.pOtpDelim, fileInfo.otpDelimLen);

	return sess.rtn.status;
	}

// Add a character to fileInfo.lineBuf, expanding it as needed.  Return status.
static int f_putc(short c) {
	uint n1, n2;

	// Line buffer full?
	if(fileInfo.lineBufCur == fileInfo.lineBufEnd) {

		// Yes, get more space.
		n1 = n2 = fileInfo.lineBufCur - fileInfo.lineBuf;
		if((fileInfo.lineBuf = (char *) realloc((void *) fileInfo.lineBuf, n2 *= 2)) == NULL)
			return rsset(Panic, 0, text156, "f_putc", n2, fileInfo.filename);
				// "%s(): Out of memory allocating %u-byte line buffer for file \"%s\"!"
		fileInfo.lineBufEnd = (fileInfo.lineBufCur = fileInfo.lineBuf + n1) + n1;
		}

	// Append character and return sess.rtn.status;
	*fileInfo.lineBufCur++ = c;
	return sess.rtn.status;
	}

// Get next character from (opened) input file and store in *pChar.  Return status, including IOEOF if end of file reached.
static int f_getc(short *pChar) {
	int byteCount;

	// At EOF?
	if(fileInfo.flags & FIEOF)
		goto EOFRetn;

	// Buffer empty?
	if(fileInfo.ioBufCur == fileInfo.ioBufEnd) {

		// Yep, refill it.
		if((byteCount = f_io(true, fileInfo.dataBuf, FIFileBufSize)) == -1)
			return sess.rtn.status;

		// Hit EOF?
		if(byteCount == 0) {

			// Yep, note it...
			fileInfo.flags |= FIEOF;
EOFRetn:
			// and return it.
			*pChar = -1;
			return IOEOF;
			}

		// No, update buffer pointers.
		fileInfo.ioBufEnd = (fileInfo.ioBufCur = fileInfo.dataBuf) + byteCount;
		}

	*pChar = (short) *fileInfo.ioBufCur++;
	return sess.rtn.status;
	}

// Read a line from a file into fileInfo.lineBuf and store the byte count and delimiter status in the supplied pointers.  Check
// for I/O errors and return status.  *hasDelimp is not set unless at least one byte was read (so the returned value from a
// previous call will remain valid).
static int f_getline(uint *len, bool *pHasDelim) {
	int status;
	short c;
	uint lineLen;
	bool hasDelim = false;

	// If we are at EOF, bail out.
	if(fileInfo.flags & FIEOF)
		return IOEOF;

	// Initialize.
	fileInfo.lineBufCur = fileInfo.lineBuf;		// Current line buffer pointer.

	// Input line delimiters undefined?
	if(fileInfo.inpDelim1 == -1) {

		// Yes.  Read bytes until a NL or CR found, or EOF reached.
		for(;;) {
			if((status = f_getc(&c)) == IOEOF)
				goto EOLn;
			if(status != Success)
				return sess.rtn.status;
			if(c == '\n') {
				fileInfo.inpDelim1 = '\n';
				fileInfo.inpDelim2 = -1;
				goto DelimFound;
				}
			if(c == '\r') {
				fileInfo.inpDelim1 = '\r';
				if((status = f_getc(&c)) == IOEOF)
					goto CR;
				if(status != Success)
					return sess.rtn.status;
				if(c == '\n') {
					fileInfo.inpDelim2 = '\n';
					goto DelimFound;
					}

				// Not a CR-LF sequence; "unget" the last character.
				--fileInfo.ioBufCur;
CR:
				fileInfo.inpDelim2 = -1;
				goto DelimFound;
				}

			// No NL or CR found yet... onward.
			if(f_putc(c) != Success)
				return sess.rtn.status;
			}
		}

	// We have line delimiter(s)... read next line.
	for(;;) {
		if((status = f_getc(&c)) == IOEOF)
			goto EOLn;
		if(status != Success)
			return sess.rtn.status;
		if(c == fileInfo.inpDelim1) {

			// Got match on first delimiter.  Need two?
			if(fileInfo.inpDelim2 == -1)

				// Nope, we're done.
				break;

			// Yes, check next character.
			if((status = f_getc(&c)) != IOEOF) {
				if(status != Success)
					return sess.rtn.status;
				if(c == fileInfo.inpDelim2)

					// Second delimiter matches also, we're done.
					break;

				// Not a match; "unget" the last character.
				--fileInfo.ioBufCur;
				c = fileInfo.inpDelim1;
				}
			}

		// Delimiter not found, or two delimiters needed, but only one found.  Save the character and move onward.
		if(f_putc(c) != Success)
			return sess.rtn.status;
		}
DelimFound:
	hasDelim = true;
EOLn:
	// Hit EOF and nothing read?
	if((lineLen = fileInfo.lineBufCur - fileInfo.lineBuf) == 0 && fileInfo.flags & FIEOF)

		// Yes.
		return IOEOF;

	// Got a (possibly zero-length) line.  Save the length for the caller and return delimiter status if non-zero.
	if((*len = lineLen) > 0)
		*pHasDelim = hasDelim;

	return sess.rtn.status;
	}

// Insert a line into a buffer before point.  Return status.
int insertLine(const char *src, int len, bool hasDelim, Buffer *pBuf, Point *pPoint) {
	Line *pLine;
	int size;

	// Allocate new buffer line for text to be inserted plus a portion of the point line, depending on point's position.
	size = len + (hasDelim ? pPoint->offset : pPoint->pLine->used);
	if(lalloc(size, &pLine) != Success)
		return sess.rtn.status;						// Fatal error.
	if(hasDelim && pPoint->offset == 0)
		memcpy(pLine->text, src, len);					// Copy line.
	else {
		const char *str, *strEnd;
		char *dest = pLine->text;
		if(pPoint->offset > 0) {
			strEnd = (str = pPoint->pLine->text) + pPoint->offset;	// Copy text before point.
			do {
				*dest++ = *str++;
				} while(str < strEnd);
			}
		strEnd = src + len;						// Copy line.
		while(src < strEnd)
			*dest++ = *src++;
		if(hasDelim) {
			if(pPoint->offset == pPoint->pLine->used)		// Shift any remaining text to BOL.
				pPoint->offset = pPoint->pLine->used = 0;	// Nothing left.
			else {
				str = (dest = pPoint->pLine->text) + pPoint->offset;
				strEnd = pPoint->pLine->text + pPoint->pLine->used;
				do {
					*dest++ = *str++;
					} while(str < strEnd);
				pPoint->pLine->used -= pPoint->offset;
				pPoint->offset = 0;
				}
			}
		else {
			if(pPoint->offset < pPoint->pLine->used) {		// Copy remaining text.
				str = pPoint->pLine->text + pPoint->offset;
				strEnd = pPoint->pLine->text + pPoint->pLine->used;
				do {
					*dest++ = *str++;
					} while(str < strEnd);
				}
			pPoint->offset += len;
			lreplace1(pPoint->pLine, pBuf, pLine);			// Replace point line with new one.
			pPoint->pLine = pLine;
			return sess.rtn.status;
			}
		}
	llink(pLine, pBuf, pPoint->pLine);					// Link new line before point line.
	return sess.rtn.status;
	}

// Read data from a buffer or file, insert at point in target buffer, and return status.  DataInsert object contains additional
// input and output parameters.  If pSrcBuf is not NULL, lines are "read" from that buffer; otherwise, they are read from
// current (opened) input file, status parameter is set to result, and finalDelim parameter is set to true if last line read had
// a delimiter, otherwise false.  If pTargPoint parameter is not NULL, target buffer is not marked as changed after lines are
// inserted; if pTargPoint parameter is NULL, point from current window is used and (1), inserted lines are marked as a region;
// (2), target buffer is marked as changed if any lines were inserted; and (3), point is moved to beginning of region if n is
// zero.  In all cases, "n" parameter is updated to number of lines inserted.
int insertData(long n, Buffer *pSrcBuf, DataInsert *pDataInsert) {
	Point *pPoint, point0;
	Line *pLine0;
	int windPos;
	int len;
	bool hasDelim = true;

	// Point given?
	if(pDataInsert->pTargPoint == NULL) {

		// No, use one from current window and save state of target buffer for region manipulation later.
		pPoint = &sess.cur.pFace->point;
		point0.pLine = (pPoint->pLine == pDataInsert->pTargBuf->pFirstLine) ? NULL : pPoint->pLine->prev;
		windPos = getWindPos(sess.pCurWind);
		}
	else
		// Yes, use it.
		pPoint = pDataInsert->pTargPoint;
	point0.offset = pPoint->offset;
	pLine0 = pPoint->pLine;

	// Insert each line from the buffer or file before point line in target buffer.
	pDataInsert->lineCt = 0;
	if(pSrcBuf != NULL) {
		Line *pBufLine = pSrcBuf->pFirstLine;				// First line of source buffer.
		do {
			hasDelim = (pBufLine->next != NULL);
			if(insertLine(pBufLine->text, len = pBufLine->used, hasDelim, pDataInsert->pTargBuf, pPoint) != Success)
				return sess.rtn.status;
			++pDataInsert->lineCt;
			} while((pBufLine = pBufLine->next) != NULL);		// Check for EOB in source buffer.
		}
	else {
		// Read lines from current input file.  If a line is read that has no delimiter, it is the last line of the file
		// (and len > 0).  If point was not specified, insert line's contents at point via einsertc() being that point
		// is from the current window; otherwise, replace target point line with line read.  In the latter case, it is
		// assumed that the point line is last line of the buffer.
		if(mlputs(pDataInsert->msg, MLHome | MLWrap | MLFlush) != Success)		// Let user know what's up...
			return sess.rtn.status;
		while((pDataInsert->status = f_getline(&len, &hasDelim)) == Success) {		// and read the file.
			if(insertLine(fileInfo.lineBuf, len, hasDelim, pDataInsert->pTargBuf, pPoint) != Success)
				return sess.rtn.status;
			++pDataInsert->lineCt;
			}

		// Last read was unsuccessful, check for errors.
		if(pDataInsert->status <= FatalError)
			return sess.rtn.status;				// Bail out.
		if(pDataInsert->status >= Success)
			pDataInsert->status = f_close(false);		// Read was successful; preserve close status.
		else
			(void) f_close(false);				// Read error; ignore close status.
		pDataInsert->finalDelim = hasDelim;
		}

	// Update line pointers if last line inserted had no delimiter (causing point line to be reallocated by insertLine()).
	if(!hasDelim) {
		if(pDataInsert->pTargBuf->windCount > 0)
			fixFace(point0.offset, len, pLine0, pPoint->pLine);
		fixBufFace(pDataInsert->pTargBuf, point0.offset, len, pLine0, pPoint->pLine);
		}

	// All lines inserted.  Set mark RegionMark to first inserted line and restore original window framing, if applicable.
	if(pDataInsert->pTargPoint == NULL) {
		pDataInsert->pTargBuf->markHdr.point.pLine = (point0.pLine == NULL) ? pDataInsert->pTargBuf->pFirstLine :
		 point0.pLine->next;
		pDataInsert->pTargBuf->markHdr.point.offset = point0.offset;
		pDataInsert->pTargBuf->markHdr.reframeRow = windPos;

		// If n is zero, swap point and RegionMark.
		if(n == 0)
			(void) swapMarkId(RegionMark);
		}

	// Update window flags and return line count.
	if(pDataInsert->lineCt > 0) {
		bchange(pDataInsert->pTargBuf, WFHard);
		if(pDataInsert->pTargBuf == sess.cur.pBuf && n != 0) {
			sess.pCurWind->reframeRow = 0;
			sess.pCurWind->face.wflags |= WFReframe;
			}
		}

	return sess.rtn.status;
	}

// Does "filename" exist on disk?  Return one of the following flags:
//	0		File does not exist.
//	FTypRegular	Regular file.
//	FTypSymLink	Symbolic link.
//	FTypDir		Directory.
//	FTypOther	Other file type.
uint fileExists(const char *filename) {
	struct stat s;

	if(lstat(filename, &s) == 0)
		switch(s.st_mode & S_IFMT) {
			case S_IFREG:
				return FTypRegular;
			case S_IFLNK:
				return FTypSymLink;
			case S_IFDIR:
				return FTypDir;
			default:
				return FTypOther;
			}
	return 0;
	}

// Convert a line delimiter symbol to string form and return it.
const char *delimSymToStr(ushort sym) {

	return sym == FDelimNL ? "NL" : sym == FDelimCRLF ? "CRLF" : sym == FDelimCR ? "CR" : sym == FDelimNUL ? "NUL" : "";
	}

// Report I/O results as a return message, given active fabrication object.  Return status.
int ioStat(DFab *rtnMsg, uint flags, Datum *pBakName, int status, const char *filename, const char *action, uint lineCt) {

	// Report any non-fatal read or close error.
	if(!(flags & IOS_OtpFile) && status < Success) {
		if(dputf(rtnMsg, 0, text141, strerror(errno), filename == NULL ? stdinFilename : filename) != 0 ||
				// "I/O Error: %s, file \"%s\""
		 dputc(' ', rtnMsg, 0) != 0)
			goto LibFail;
		sess.cur.pBuf->flags |= BFActive;		// Don't try to read file again.
		}

	// Begin wrapped message and report I/O operation.
	if(dputc('[', rtnMsg, 0) != 0 || dputs(action, rtnMsg, 0) != 0)
		goto LibFail;

	// Report line count.
	if(dputf(rtnMsg, 0, " %u %s%s", lineCt, text205, lineCt != 1 ? "s" : "") != 0)
				// "line"
		goto LibFail;

	// Non-standard line delimiter(s)?
	if(status == Success) {
		ushort sym;
		bool stdDelim = true;
		if(flags & IOS_OtpFile) {
			if(fileInfo.pOtpDelim[0] != '\n' || fileInfo.pOtpDelim[1] != '\0') {
				stdDelim = false;
				sym = delimCharsToSym(fileInfo.pOtpDelim[0], fileInfo.pOtpDelim[1]);
				}
			}
		else if(sess.cur.pBuf->inpDelimSym != FDelimUndef)
			if(fileInfo.inpDelim1 != '\n' || fileInfo.inpDelim2 != -1) {
				stdDelim = false;
				sym = sess.cur.pBuf->inpDelimSym;
				}
		if(!stdDelim)
			if(dputs(text252, rtnMsg, 0) != 0 || dputs(delimSymToStr(sym), rtnMsg, 0) != 0)
					// " delimited by "
				goto LibFail;
		}

	// Missing line delimiter at EOF?
	if(flags & IOS_NoDelim && dputs(text291, rtnMsg, 0) != 0)
			// " without delimiter at EOF"
		goto LibFail;

	// Insert?
	if(action == text154 && dputs(text355, rtnMsg, 0) != 0)
			// " and marked as region"
		goto LibFail;

	// Backup file created on output?
	if(pBakName != NULL) {
		if(dputs(text257, rtnMsg, 0) != 0 ||
			// ", original file renamed to \""
		 dputs(cname(pBakName->str, CNBasename | CNPtrOnly), rtnMsg, 0) != 0 || dputc('"', rtnMsg, 0) != 0)
			goto LibFail;
		}

	if(dputc(']', rtnMsg, 0) != 0 || dclose(rtnMsg, FabStr) != 0)
		goto LibFail;
	if(mlerase(0) != Success)
		return sess.rtn.status;
	return rsset(status >= Success ? Success : status, flags & IOS_RSHigh ? RSHigh | RSNoFormat | RSNoWrap :
	 RSNoFormat | RSNoWrap, rtnMsg->pDatum->str);
LibFail:
	return libfail();
	}

// Prompt for a filename (which cannot be null), using "defName" as default (which may be NULL).  ArgFirst and/or Term_* flags
// may be specified in "flags" argument.  Return status.
int getFilename(Datum *pRtnVal, const char *basePrompt, const char *defName, uint flags) {

	if(sess.opFlags & OpScript)
		(void) funcArg(pRtnVal, (flags & ArgFirst) | ArgNotNull1 | ArgPath);
	else {
		TermInpCtrl termInpCtrl = {defName, RtnKey, MaxPathname, NULL};
		char promptBuf[WorkBufSize];
		sprintf(promptBuf, "%s %s", basePrompt, text99);
						// "file"
		(void) termInp(pRtnVal, promptBuf, ArgNotNull1 | ArgNil1, TermC_Filename | (flags & ~ArgFirst), &termInpCtrl);
		}
	return sess.rtn.status;
	}

// Delete a file on disk (possibly with options) and return status.  Return filename if deletion was successful, otherwise nil.
int delFile(Datum *pRtnVal, long n, Datum **args) {
	uint flags = cmdFuncTable[cf_delFile].pOptHdr->selFlags;

	// If no options were selected interactively, bail out (user cancelled).
	if(n != DefN && !(sess.opFlags & OpScript) && flags == 0)
		return sess.rtn.status;

	if(flags & File_WithBuf) {

		// Get buffer name.
		uint oflags;
		Buffer *pBuf;
		if(sess.opFlags & OpScript) {
			dxfer(pRtnVal, args[0]);		// Buffer name.
			oflags = OpDelete | OpHaveBName;
			}
		else
			oflags = OpDelete | OpDefBName;
		if(getBufname(pRtnVal, text26, oflags, &pBuf, NULL) != Success || pBuf == NULL)
				// "Delete"
			return sess.rtn.status;

		// Save filename and delete buffer.  Error if no filename set.
		if(pBuf->filename == NULL)
			return rsset(Failure, 0, text493, text83, pBuf->bufname);
				// "No filename set for %s '%s'", "buffer"
		if(dsetstr(pBuf->filename, pRtnVal) != 0)
			return libfail();
		if(bdelete(pBuf, 0) != Success)
			return sess.rtn.status;
		}
	else if(sess.opFlags & OpScript)
		dxfer(pRtnVal, args[0]);			// Filename.
	else {
		// Get filename.
		if(getFilename(pRtnVal, text26, NULL, ArgFirst | TermC_NoAuto) != Success || disnil(pRtnVal))
				// "Delete"
			return sess.rtn.status;
		}

	// Have filename in pRtnVal.  Delete it.
	if(fileExists(pRtnVal->str)) {
		if(unlink(pRtnVal->str) == 0)
			(void) rsset(Success, RSHigh, flags & File_WithBuf ? text485 : text486, pRtnVal->str, text10);
					// "Buffer and file \"%s\" %s", "File \"%s\" %s", "deleted"
		else if(!(flags & File_IgnoreErr))
			(void) rsset(Failure, 0, text484, text263, pRtnVal->str, strerror(errno));
				// "Cannot %s file \"%s\": %s", "delete"
		}
	else if(flags & File_WithBuf) {
		dsetnil(pRtnVal);
		(void) rsset(Success, RSHigh, "%s %s", text58, text10);
			// "Buffer", "deleted"
		}
	else {
		if(!(flags & File_IgnoreErr))
			(void) rsset(Failure, 0, text52, text99, pRtnVal->str);
					// "No such %s \"%s\"", "file"
		dsetnil(pRtnVal);
		}

	return sess.rtn.status;
	}

// Create hard or symbolic link to a file (possibly with options) and return status.
int linkFile(Datum *pRtnVal, long n, Datum **args) {
	Datum *pOldFile, *pNewFile;
	uint flags = cmdFuncTable[cf_linkFile].pOptHdr->selFlags;

	// If no options were selected interactively, bail out (user cancelled).
	if(n != DefN && !(sess.opFlags & OpScript) && flags == 0)
		return sess.rtn.status;

	if((pOldFile = dnewtrack()) == NULL || (pNewFile = dnewtrack()) == NULL)
		return libfail();

	// Get old filename.  Error if hard link and file does not exist.
	if(sess.opFlags & OpScript)
		dxfer(pOldFile, args[0]);
	else if(getFilename(pOldFile, flags & File_Symbolic ? text488 : text487, NULL,
			// "Make symbolic link from existing", "Make hard link from existing"
	 TermC_NoAuto) != Success || disnil(pOldFile))
		return sess.rtn.status;
	if(!(flags & File_Symbolic) && !fileExists(pOldFile->str))
		return rsset(Failure, 0, text52, text99, pOldFile->str);
				// "No such %s \"%s\"", "file"

	// Get new filename.
	if(sess.opFlags & OpScript)
		dxfer(pNewFile, args[1]);
	else if(getFilename(pNewFile, text489, NULL, TermC_NoAuto) != Success || disnil(pNewFile))
				// "to new"
		return sess.rtn.status;

	// Delete new file if force and it already exists.
	if(flags & File_ForceLink && fileExists(pNewFile->str) && unlink(pNewFile->str) != 0)
		return rsset(Failure, 0, text484, text263, pNewFile->str, strerror(errno));
				// "Cannot %s file \"%s\": %s", "delete"

	// Ready to roll.  Create link.
	if((flags & File_Symbolic ? symlink(pOldFile->str, pNewFile->str) : link(pOldFile->str, pNewFile->str)) != 0)
		return rsset(Failure, 0, text490, pNewFile->str, strerror(errno));
			// "Cannot create link file \"%s\": %s"

	// Success.
	dxfer(pRtnVal, pNewFile);
	return rsset(Success, RSHigh, text491, pRtnVal->str);
			// "Link file \"%s\" created"
	}

// Check if changing a buffer's filename is allowed.  Return status.
static int bfchange(Buffer *pBuf) {

	// We cannot set a filename on a read-only buffer, user command or function buffer, or an executing buffer.
	if(pBuf->flags & (BFReadOnly | BFCmdFunc))
		return rsset(Failure, 0, text344, pBuf->flags & BFCmdFunc ? text376 : text460);
			// "Operation not permitted on a %s buffer", "user command or function", "read-only"
	if(pBuf->pCallHdr != NULL && pBuf->pCallHdr->execCount > 0)
		return rsset(Failure, 0, text284, text276, text238);
			// "Cannot %s %s buffer", "modify", "executing"
	return sess.rtn.status;
	}

// Check screens for multiple working directories.  If pBuf is NULL, return true if multiple screens exist and two or more have
// different working directories; otherwise, return true if the given buffer is being displayed on at least two screens having
// different working directories.  In the latter case, if pSaveDir not NULL, also set *pSaveDir to directory of last screen that
// buffer is displayed on (which will be unique if false is returned).
static bool multiHomeDir(Buffer *pBuf, const char **pSaveDir) {
	EScreen *pScrn = sess.scrnHead;
	const char *workDir = NULL;
	if(pBuf == NULL) {						// Check for multiple working directories.
		if(pScrn->next == NULL)
			return false;					// Only one screen.
		do {							// In all screens...
			if(workDir == NULL)
				workDir = pScrn->workDir;
			else if(workDir != pScrn->workDir)
				return true;				// Different directory found.
			} while((pScrn = pScrn->next) != NULL);
		}
	else {								// Check if buffer is multi-homed.
		EWindow *pWind;

		do {							// In all screens...
			pWind = pScrn->windHead;			// Buffer on this screen?
			do {
				if(pWind->pBuf == pBuf) {
					if(pSaveDir != NULL)
						*pSaveDir = pScrn->workDir;
					if(workDir == NULL)
						workDir = pScrn->workDir;
					else if(workDir != pScrn->workDir)
						return true;		// Different directory found.
					break;
					}
				} while((pWind = pWind->next) != NULL);
			} while((pScrn = pScrn->next) != NULL);
		}

	return false;
	}

// Set filename in buffer with flags and update buffer's home directory, if applicable.  Also derive new buffer name from
// filename if BF_DeriveName flag is set.  It is assumed that buffer is not a user command or function and BF_UpdBufDir flag is
// not set (and thus, setFilename() does not update buffer's home directory).  Return status.
static int setfile(Buffer *pBuf, Datum *pFile, uint flags) {

	if(setFilename(pBuf, pFile->str, flags) == Success) {

		// Update buffer directory if not in background and not multi-homed.
		if(pBuf->windCount > 0) {
			const char *homeDir;

			if(!multiHomeDir(pBuf, &homeDir))
				pBuf->homeDir = homeDir;
			}

		// Change buffer name if applicable.
		if(flags & BF_DeriveName && pBuf->filename != NULL)
			(void) brename(NULL, BR_Auto, pBuf);
		}

	return sess.rtn.status;
	}

// Rename a file on disk (possibly with options) and return status.  Return new filename if successful, otherwise nil.
int renameFile(Datum *pRtnVal, long n, Datum **args) {
	Datum *pDatum;
	Buffer *pBuf;
	uint flags = cmdFuncTable[cf_renameFile].pOptHdr->selFlags;

	// If no options were selected interactively, bail out (user cancelled).
	if(n != DefN && !(sess.opFlags & OpScript) && flags == 0)
		return sess.rtn.status;

	if((pDatum = dnewtrack()) == NULL)
		return libfail();

	// Get buffer name or filename into pDatum if script mode.
	if(sess.opFlags & OpScript)
		dxfer(pDatum, args[0]);

	if(flags & File_FromBuf) {

		// Get buffer name and check if filename change allowed.
		uint oflags = sess.opFlags & OpScript ? OpDelete | OpHaveBName : OpDelete | OpCurBName;
		if(getBufname(pDatum, text492, oflags, &pBuf, NULL) != Success || pBuf == NULL ||
		 (!(flags & File_DiskOnly) && bfchange(pBuf) != Success))
				// "Rename file in"
			return sess.rtn.status;

		// Error if no filename set in buffer; otherwise, save filename.
		if(pBuf->filename == NULL)
			return rsset(Failure, 0, text493, text83, pBuf->bufname);
				// "No filename set for %s '%s'", "buffer"
		if(dsetstr(pBuf->filename, pDatum) != 0)
			return libfail();

		// Activate buffer if needed.
		if(!(pBuf->flags & BFActive) && bactivate(pBuf) != Success)
			return sess.rtn.status;
		}
	else if(!(sess.opFlags & OpScript)) {

		// Get old filename.
		if(getFilename(pDatum, text29, NULL, TermC_NoAuto) != Success || disnil(pDatum))
				// "Rename"
			return sess.rtn.status;
		}

	// Check if file exists.
	if(!fileExists(pDatum->str))
		return rsset(Failure, 0, text52, text99, pDatum->str);
				// "No such %s \"%s\"", "file"
	// Get new filename.
	if(sess.opFlags & OpScript)
		dxfer(pRtnVal, args[1]);
	else if(getFilename(pRtnVal, text339, flags & File_FromBuf ? pBuf->filename : NULL, TermC_NoAuto) != Success ||
				// "to"
	 disnil(pRtnVal))
		return sess.rtn.status;

	// Have old and new filenames in *pDatum and *pRtnVal.  Rename file on disk.
	if(rename(pDatum->str, pRtnVal->str) != 0) {
		dsetnil(pRtnVal);
		return rsset(Failure, 0, text484, text275, pDatum->str, strerror(errno));
			// "Cannot %s file \"%s\": %s", "rename"
		}

	// Set buffer's filename if applicable.
	if((flags & (File_FromBuf | File_DiskOnly)) == File_FromBuf && setfile(pBuf, pRtnVal, BF_DeriveName) != Success) {
		dsetnil(pRtnVal);
		return sess.rtn.status;
		}

	// Success.
	return rsset(Success, RSHigh, "%s %s", flags & File_DiskOnly ? text494 : text495,  text467);
					// "Disk file", "File", "renamed"
	}

// Prepare a buffer for reading.  "flags" is passed to bclear().  Return status.
int readPrep(Buffer *pBuf, uint flags) {

	return pBuf->flags & BFCmdFunc ? rsset(Failure, 0, text344, text376) : bclear(pBuf, flags);
				// "Operation not permitted on a %s buffer", "user command or function"
	}

// Read a file into the given buffer, blowing away any text found there.  If filename is NULL, use the buffer's filename if set,
// otherwise standard input.  If RWKeep flag is not set, delete the buffer's filename if set; otherwise, save the filename in
// the buffer record (if it is not narrowed) and, if RWReadHook flag is set, (a), run the read hook after the file is read; and
// (b), run the filename hook if the buffer filename was changed.  If RWExist flag is set, return an error if the file does not
// exist.  If RWStats flag set, set a return message containing input statistics.  Return status, including Cancelled if buffer
// was not cleared (user did not "okay" it) and IONSF if file does not exist.
int readIn(Buffer *pBuf, const char *filename, uint flags) {
	EWindow *pWind;
	DataInsert dataInsert = {
		pBuf,			// Target buffer.
		&pBuf->face.point,	// Target line.
		text139			// "Reading data..."
		};

	if(readPrep(pBuf, 0) != Success)
		return sess.rtn.status;					// Error or user got cold feet.

	// Determine filename.
	if(filename == NULL)
		filename = pBuf->filename;
	else if(flags & RWKeep && !(pBuf->flags & BFNarrowed) &&
	 setFilename(pBuf, filename, 0) != Success)			// Save given filename.
		return sess.rtn.status;

	// Open the file.
	if((dataInsert.status = f_ropen(filename, flags & RWExist)) <= FatalError)
		return sess.rtn.status;					// Bail out.
	if(dataInsert.status != Success)
		goto Retn;

	// Read the file and "unchange" the buffer.
	if(insertData(1, NULL, &dataInsert) != Success)
		return sess.rtn.status;
	bchgflag(pBuf, false);

	// Report results.
	if(flags & RWStats) {
		DFab fab;

		if(dopentrack(&fab) != 0)
			return libfail();
		if(ioStat(&fab, dataInsert.finalDelim ? IOS_RSHigh : IOS_RSHigh | IOS_NoDelim, NULL, dataInsert.status,
		 filename, text131, dataInsert.lineCt) != Success)
				// "Read"
			return sess.rtn.status;
		}
Retn:
	// Make sure buffer is flagged as active.
	pBuf->flags |= BFActive;

	// Update buffer and window pointers.
	faceInit(&pBuf->face, pBuf->pFirstLine, pBuf);
	pWind = sess.windHead;
	do {
		if(pWind->pBuf == pBuf) {
			faceInit(&pWind->face, pBuf->pFirstLine, NULL);
			pWind->face.wflags |= (WFHard | WFMode);
			}
		} while((pWind = pWind->next) != NULL);

	// Erase filename and run read hook if requested.
	if(!(flags & RWKeep))
		clearBufFilename(pBuf);
	else if(dataInsert.status >= Success && flags & RWReadHook && !(pBuf->flags & BFCmdFunc))
		if(execHook(NULL, DefN, hookTable + HkRead, 2, pBuf->bufname, pBuf->filename) <= FatalError)
			return sess.rtn.status;

	// Return status.
	return (dataInsert.status == IONSF) ? rsset(Success, RSHigh | RSNoFormat, text138) : mlerase(0);
							// "New file"
	}

// Create inactive buffer for readFile() and findViewFile() functions.
static int createBuf(char *filename, Buffer **ppBuf, bool *pCreated) {

	if(bfind(filename, BN_Create | BN_CreateHook | BN_Derive | BN_Force, 0, ppBuf, pCreated) == Success) {

		// Set filename and flag as inactive.
		if(setFilename(*ppBuf, filename, BF_UpdBufDir) == Success)
			(*ppBuf)->flags &= ~BFActive;
		}
	return sess.rtn.status;
	}

// Read given file into a buffer.  If default n, use the current buffer.  If not default n, create a buffer, derive the buffer
// name from the filename, and save the filename in the buffer header.  In any case, set the buffer to "read only" if the
// "ReadOnly" global mode is enabled.  Render buffer and return status.
int readFile(Datum *pRtnVal, long n, Datum **args) {
	Buffer *pBuf;

	// Get the filename.
	if(getFilename(pRtnVal, n == -1 ? text27 : text131, sess.cur.pBuf->filename, ArgFirst) != Success || disnil(pRtnVal))
				// "Pop", "Read"
		return sess.rtn.status;

	// Use current buffer by default; otherwise, create one.
	if(n == DefN) {
		pBuf = sess.cur.pBuf;

		// Using current buffer.  Read the file and rename buffer.
		if(readIn(pBuf, pRtnVal->str, RWReadHook | RWExist | RWKeep | RWStats) != Success ||
		 brename(NULL, BR_Auto, pBuf) != Success)
			return sess.rtn.status;

		// Update buffer directory.
		pBuf->homeDir = sess.pCurScrn->workDir;
		}
	else if(createBuf(pRtnVal->str, &pBuf, NULL) != Success)
		return sess.rtn.status;

	// Set buffer to "read only" if "ReadOnly" global mode enabled.
	if(mode.cache[MdIdxReadOnly]->flags & MdActive) {
		pBuf->flags |= BFReadOnly;
		supd_windFlags(pBuf, WFMode);
		}

	// Render the buffer.
	return render(pRtnVal, n == DefN ? 1 : n, pBuf, n == DefN ? 0 : RendNewBuf | RendNotify);
	}

// Insert a file into the current buffer.  If zero argument, leave point before the inserted lines, otherwise after.  This
// is really easy; all you do it find the name of the file, and call the standard "insert a file into the current buffer" code.
int insertFile(Datum *pRtnVal, long n, Datum **args) {

	if(getFilename(pRtnVal, text132, NULL, ArgFirst | Term_NoDef) != Success || disnil(pRtnVal))
			// "Insert"
		return sess.rtn.status;

	// Open the file and insert the data.
	if(f_ropen(pRtnVal->str, true) == Success) {
		DFab fab;
		DataInsert dataInsert = {
			sess.cur.pBuf,	// Target buffer.
			NULL,		// Target line.
			text153		// "Inserting data..."
			};

		if(insertData(n, NULL, &dataInsert) == Success) {
			if(dopentrack(&fab) != 0)
				return libfail();
			(void) ioStat(&fab, dataInsert.finalDelim ? 0 : IOS_NoDelim, NULL, dataInsert.status, pRtnVal->str,
			 text154, dataInsert.lineCt);
				// "Inserted"
			}
		}
	dclear(pRtnVal);
	return sess.rtn.status;
	}

// Check if a buffer's filename matches given filename.  Return status, including NotFound if no match.
static int fileCompare(Buffer *pBuf, const char *filename) {

	if(pBuf->filename != NULL && strcmp(pBuf->filename, filename) == 0) {

		// Match found.  Check directory.
		if(*filename == '/' || pBuf->homeDir == sess.pCurScrn->workDir)
			return rsset(Success, RSNoFormat, text135);
						// "Old buffer"
		}
	return NotFound;
	}

// Find a file and optionally, read it into a buffer (for findFile and viewFile commands).  If n == 0, just create a buffer for
// the file if one does not already exist and stay in current buffer.  If n != 0 and the file is not currently attached to a
// buffer, create a buffer.  In all cases, set buffer to "read only" if it was created and either "view" is true or ReadOnly
// global mode is enabled.  Render buffer and return status.
int findViewFile(Datum *pRtnVal, long n, bool view) {
	Buffer *pBuf;
	HashNode *pNode;
	HashEach each;
	EScreen *pScrn;
	EWindow *pWind;
	int status;
	bool created = false;
	bool readOnly = view || mode.cache[MdIdxReadOnly]->flags & MdActive;

	// Get filename.
	if(getFilename(pRtnVal, view ? text134 : text133, NULL, n < 0 || n > 1 ? ArgFirst | Term_NoDef :
	 ArgFirst | Term_NoDef | TermC_NoAuto) != Success || disnil(pRtnVal))
			// "View", "Find"
		return sess.rtn.status;

	// Check if an existing buffer is attached to the file.  First, check buffers being displayed.
	pScrn = sess.scrnHead;
	do {								// In all screens...
		pWind = pScrn->windHead;
		do {							// In all windows...
			if((status = fileCompare(pBuf = pWind->pBuf, pRtnVal->str)) < NotFound)	// Save Buffer pointer.
				return sess.rtn.status;
			if(status == Success)
				goto ViewChk;				// Existing buffer found.
			} while((pWind = pWind->next) != NULL);
		} while((pScrn = pScrn->next) != NULL);

	// No match.  Now check background buffers.
	heachinit(&each, &buf.table);
	while((pNode = heach(&each)) != NULL) {
		pBuf = bufPtr(pNode->pVal);
		if(pBuf->windCount == 0) {				// Select if not being displayed.
			if((status = fileCompare(pBuf, pRtnVal->str)) < NotFound)
				return sess.rtn.status;
			if(status == Success)
				goto ViewChk;
			}
		}

	// No buffer found... create one.
	if(createBuf(pRtnVal->str, &pBuf, &created) != Success)
		return sess.rtn.status;
ViewChk:
	// Set "read only" buffer attribute if readOnly is true.  No need to run filename hook -- that will happen when the
	// buffer is activated.  If file being viewed is associated with an existing buffer that has been modified, notify user
	// of such.
	if(readOnly) {
		if(pBuf->flags & BFChanged) {
			if(view)
				(void) rsset(Success, RSForce | RSNoWrap, text461, text260, text460);
						// "Cannot set %s buffer to %s", "changed", "read-only"
			}
		else {
			pBuf->flags |= BFReadOnly;
			supd_windFlags(pBuf, WFMode);
			}
		}

	return render(pRtnVal, n == DefN ? 1 : n, pBuf, created ? RendNewBuf | RendNotify : 0);
	}

// Check given buffer flags and verify with user before writing to a file.  Return status.
static int bufStatusCheck(Buffer *pBuf) {

	// Complain about narrowed buffer.
	if(pBuf->flags & BFNarrowed) {
		bool yep;
		DFab prompt;

		if(dopentrack(&prompt) != 0 ||
		 dputf(&prompt, 0, text147, text308, pBuf->bufname) != 0 || dclose(&prompt, FabStr) != 0)
				// "%s buffer '%s'... write it out", "Narrowed"
			(void) libfail();
		else if(termInpYN(prompt.pDatum->str, &yep) == Success && !yep)
			(void) rsset(UserAbort, 0, NULL);
		}

	return sess.rtn.status;
	}

// Check if output file is a symbolic or hard link and ask user if it should be deleted before write (to break the link) if so.
static int linkCheck(Buffer *pBuf, const char *filename) {

	// Skip link check if safe mode (because output file will be deleted before write in that case).
	if(!modeSet(MdIdxSafe, pBuf)) {
		struct stat s;

		// Check if output file exists, is a symbolic or hard link, and is not in "fileLinkKeepers" hash.
		if(lstat(filename, &s) == 0 && ((s.st_mode & S_IFMT) == S_IFLNK ||
		 ((s.st_mode & S_IFMT) == S_IFREG && s.st_nlink > 1))) {
			Datum *pPath, *pPrompt;
			bool nukeIt;
			char wkBuf[strlen(filename) + 1];

			if((pPath = dnewtrack()) == NULL || (pPrompt = dnewtrack()) == NULL)
				goto LibFail;
			if(absPath(strcpy(wkBuf, filename), false, pPath) == Success &&
			 hsearch(&fileLinkKeepers, pPath->str) == NULL) {

				// Outfile is a link and not previously brought to user's attention.  Ask user if the link
				// should be broken.  If yes, delete the output file; otherwise, remember response by adding
				// absolute pathname to fileLinkKeepers hash (as key with nil value).
				if(dsetf(pPrompt, text537, (s.st_mode & S_IFMT) == S_IFLNK ? text216 : text269, filename) != 0)
						// "Break %s link for file \"%s\" on output", "symbolic", "hard"
					goto LibFail;
				if(termInpYN(pPrompt->str, &nukeIt) == Success) {
					if(nukeIt)
						(void) unlink(filename);
					else if(hnewnode(&fileLinkKeepers, pPath->str, NULL) == NULL)
LibFail:
						return libfail();
					}
			 	}
		 	}
		}

	return sess.rtn.status;
	}

// Write a buffer to disk, given buffer, filename, mode ('w' = write, 'a' = append), and save flags, and return status.  Before
// the file is written, a user routine (write hook) can be run.  The number of lines written, and possibly other status
// information is displayed.  If "ATerm" mode is enabled and the buffer does not have a terminating line delimiter, one is added
// before the buffer is written.  If "Safe" or "Bak" mode is enabled, the buffer is written out to a temporary file, the old
// file is unlinked or renamed to "<file>.bak" (if "<file>.bak" does not already exist), and the temporary is renamed to the
// original.  If BW_QuickExit flag is set and buffer is saved to disk, buffer is flagged (with BFQSave and possibly BFQBak) for
// "quickExit" command processing.  In all cases, call ioStat() with appropriate flags and arguments on exit.
static int writeOut(Buffer *pBuf, const char *filename, short fmode, uint flags) {
	uint lineCt;			// Number of lines written.
	int status;			// Return status.
	uint saveFlags = 0;		// Are we safe saving? (0x01 bit = 'safe' save, 0x02 bit = 'bak' save)
	DFab fab;
	struct stat s;			// File permision info.
	Datum *pTempName;		// Temporary filename.
	Datum *pBakName;		// Backup filename.

	if(dopentrack(&fab) != 0)
		goto LibFail;

	// Check for symbolic or hard link and run user-assigned write hook.
	if((fmode == 'w' && linkCheck(pBuf, filename) != Success) ||
	 execHook(NULL, DefN, hookTable + HkWrite, 0x103, pBuf->bufname, filename, (long) fmode) != Success)
		return sess.rtn.status;

	// Determine if we will use the "safe save" method.
	if(fmode == 'w' && (modeSet(MdIdxBak, pBuf) || modeSet(MdIdxSafe, pBuf)) && fileExists(filename)) {
		if(modeSet(MdIdxBak, pBuf)) {

			// 'Bak' mode enabled.  Create backup version of filename.
			if((pBakName = dnewtrack()) == NULL || dsalloc(pBakName, strlen(filename) + strlen(BackupExt) + 1) != 0)
				goto LibFail;
			sprintf(pBakName->str, "%s%s", filename, BackupExt);

			// Enable '.bak' save if backup file does not already exist.
			if(fileExists(pBakName->str) == 0)
				saveFlags = 0x02;
			}
		if(modeSet(MdIdxSafe, pBuf))
			saveFlags |= 0x01;
		}

	// Open output file.
	if(saveFlags) {
		char *suffix;
		size_t prefixLen;

		// Duplicate original pathname and keep first letter of filename.
		prefixLen = cname((char *) filename, CNBasename | CNPtrOnly) - filename + 1;
		if((pTempName = dnewtrack()) == NULL || dsalloc(pTempName, prefixLen + 6) != 0)
			goto LibFail;
		stplcpy(pTempName->str, filename, prefixLen + 1);
		suffix = pTempName->str + prefixLen;

		// Create a unique name, using random numbers.
		do {
			longToAsc(urand(0x10000), suffix);
			} while(fileExists(pTempName->str));

		// Open the temporary file.
		status = f_wopen(pBuf, pTempName->str, fmode);
		}
	else
		status = f_wopen(pBuf, filename, fmode);

	// If the open failed or write fails, abort mission.
	if(status != Success || (status = writeDiskPipe(pBuf, &lineCt)) != Success) {
		if(saveFlags)
			(void) unlink(pTempName->str);
		return sess.rtn.status;
		}

	// Write was successful: clear "changed" flag and update window flags.
	bchgflag(pBuf, false);
	supd_windFlags(pBuf, WFMode);

	// Do file "fixups" if safe save.
	if(saveFlags) {
		// Get the permisions of the original file.
		if((status = stat(filename, &s)) != 0)
			status = errno;

		// Rename or erase original file and rename temp to original.
		if(saveFlags & 0x02) {
			if(rename(filename, pBakName->str) != 0)
				status = -1;
			}
		else if(unlink(filename) != 0)
			status = -1;
		if(status == 0 && rename(pTempName->str, filename) == 0) {
			if(chmod(filename, s.st_mode) != 0 || chown(filename, s.st_uid, s.st_gid) != 0)
				status = errno;
			}

		// Report any errors.
		if(status != 0) {
			if(dputf(&fab, 0, text141, strerror(errno), filename) != 0 || (status == -1 &&
			 (dputc(' ', &fab, 0) != 0 || dputf(&fab, 0, text150, pTempName->str) != 0)))
					// "I/O Error: %s, file \"%s\"", "(file saved as \"%s\") "
				goto LibFail;
			status = Failure;	// Failed.
			}
		}

	// If all is well, flag buffer as saved for main() to display on quick exit.
	if(status == Success && flags & BW_QuickExit)
		pBuf->flags |= (saveFlags & 0x02 ? BFQSave | BFQBak : BFQSave);

	// Report lines written and return status.
	return ioStat(&fab, lineCt > 0 && (!(pBuf->flags & BFNarrowed) || pBuf->pNarBotLine == NULL) &&
	 pBuf->pFirstLine->prev->used > 0 ? IOS_OtpFile | IOS_NoDelim : IOS_OtpFile,
	 status == Success && saveFlags & 0x02 ? pBakName : NULL, status, filename, text149, lineCt);
										// "Wrote"
LibFail:
	return libfail();
	}

// Do appendFile or writeFile command.  Ask for a filename, and write or append the contents of the current buffer to that file.
// Update the remembered filename if writeFile and default n or n > 0.  Return status.
int appendWriteFile(Datum *pRtnVal, long n, const char *prompt, short fmode) {
	Buffer *pBuf = sess.cur.pBuf;
	bool tempWrite = n <= 0 && n != DefN;

	// Get the filename.
	if(getFilename(pRtnVal, prompt, NULL, ArgFirst | TermC_NoAuto) != Success || disnil(pRtnVal))
		return sess.rtn.status;

	// Complain about narrowed buffers.
	if(bufStatusCheck(pBuf) != Success)
		return sess.rtn.status;

	// Get confirmation if needed.
	if(fmode == 'w' && fileExists(pRtnVal->str) && opConfirm(BF_Overwrite) != Success)
		return sess.rtn.status;

	// It's a go... write the file.  Also save and restore "buffer changed" flag if append or temporary write.
	ushort chgFlag = pBuf->flags & BFChanged;
	if(writeOut(pBuf, pRtnVal->str, fmode, 0) != Success)
		return sess.rtn.status;
	if((fmode == 'a' || tempWrite) && chgFlag)
		bchgflag(pBuf, true);

	// Update filename, buffer name, and buffer directory if write operation (not append) and not temporary.
	if(fmode == 'w' && !tempWrite && !(pBuf->flags & BFCmdFunc)) {
		if(setFilename(pBuf, pRtnVal->str, 0) > FatalError) {
			(void) brename(NULL, BR_Auto, pBuf);
			pBuf->homeDir = sess.pCurScrn->workDir;
			}
		}
	dclear(pRtnVal);

	return sess.rtn.status;
	}

// Save given buffer to disk if possible.  *pCurDir contains current directory and is updated if directory is changed.  "flags"
// argument is passed to writeOut().
static int saveBuf(Buffer *pBuf, uint flags, const char **pCurDir, const char *workDir, uint *pSavedCount, uint *pNarCount) {

	pBuf->flags &= ~BFQSave;					// Clear "saved" flag.
	if(!(pBuf->flags & BFChanged) ||				// If not changed...
	 (pBuf->flags & BFCmdFunc && flags & BW_All))			// or a user command/function and saving all...
		return Cancelled;					// skip it.
	if(pBuf->filename == NULL)					// Must have a filename.
		return rsset(Failure, 0, text145, pBuf->bufname);
			// "No filename associated with buffer '%s'"
	if(pBuf->flags & BFNarrowed && flags & BW_All) {		// If narrowed and saving all...
		++*pNarCount;						// count it and...
		return Cancelled;					// skip it.
		}
	if(bufStatusCheck(pBuf) != Success)				// Complain about narrowed buffers.
		return sess.rtn.status;

	// Change working directory if needed.
	if(flags & BW_MultiDir && *pBuf->filename != '/') {		// If multiple directories and have relative pathname...
		if(workDir == NULL) {					// and buffer is in the background...
			if(pBuf->homeDir == NULL)			// home directory set?
				return rsset(Failure, 0, text415, text309, pBuf->bufname);	// No, return error.
					// "%s buffer '%s'", "Directory unknown for"
			else
				workDir = pBuf->homeDir;		// Yes, use it.
			}
		else if(multiHomeDir(pBuf, NULL))			// Multi-homed buffer (in foreground)?
			return rsset(Failure, 0, text310, text309, pBuf->bufname);	// Yes, error.
				// "%s multi-screen buffer '%s'", "Directory unknown for"
		else if(pBuf->homeDir != workDir) {			// No, buffer's home directory different than screen's?
			bool yep;					// Yes, get permission from user to use directory of
			DFab prompt;					// screen it is being displayed on.

			if(dopentrack(&prompt) != 0 ||
			 dputf(&prompt, 0, text423, pBuf->bufname, workDir, pBuf->filename) != 0 ||
				// "Directory changed.  Write buffer '%s' to file \"%s/%s\""
			 dclose(&prompt, FabStr) != 0)
				return libfail();
			if(termInpYN(prompt.pDatum->str, &yep) != Success)
				return sess.rtn.status;
			if(!yep)					// Permission given?
				return rsset(UserAbort, 0, NULL);	// No, user abort.
			pBuf->homeDir = workDir;			// Yes, use screen's directory.
			}

		// It's a go... change directory if different.
		if(*pCurDir != workDir && chgdir(*pCurDir = workDir) != Success)
			return sess.rtn.status;
		}

	// Save the buffer.
	if(mlprintf(MLHome | MLWrap | MLFlush, text103, pBuf->filename) != Success)
			// "Saving %s..."
		return sess.rtn.status;
	if((flags & (BW_All | BW_QuickExit)) == BW_All)
		centiPause(40);
	if(writeOut(pBuf, pBuf->filename, 'w', flags) == Success) {
		sess.autoSaveCount = sess.autoSaveTrig;			// Reset $autoSave...
		++*pSavedCount;						// and increment "save" count.
		}

	return sess.rtn.status;
	}

// Save the contents of the current buffer (or all buffers if n argument) to its associated file.  Do nothing if nothing has
// changed.  Error if there is no filename set for a buffer.
int saveBufs(long n, uint flags) {
	HashNode *pNode;
	HashEach each;
	Buffer *pBuf;
	int status;
	const char *workDir = sess.pCurScrn->workDir;
	uint savedCount = 0;			// Count of saved buffers.
	uint narCount = 0;			// Count of narrowed buffers that are skipped.

	if(n != DefN)
		flags |= BW_All;
	if(multiHomeDir(NULL, NULL))
		flags |= BW_MultiDir;

	// Search for changed buffers one screen at a time if updating all to simplify tracking and minimize frequency of
	// chdir() calls when multiple screen directories exist.
	if(flags & BW_All) {
		EWindow *pWind;
		EScreen *pScrn = sess.scrnHead;
		do {						// In all screens...
			pWind = pScrn->windHead;
			do {					// In all windows...
				if((status = saveBuf(pWind->pBuf, flags, &workDir, pScrn->workDir, &savedCount,
				 &narCount)) <= UserAbort)
					return sess.rtn.status;
				} while((pWind = pWind->next) != NULL);
			} while((pScrn = pScrn->next) != NULL);
		}

	// Scan buffer list for changed buffers (in background only if updating all).
	if(flags & BW_All) {
		heachinit(&each, &buf.table);
		pNode = heach(&each);
		pBuf = bufPtr(pNode->pVal);
		}
	else
		pBuf = sess.cur.pBuf;
	for(;;) {
		// Skip any buffer being displayed if screen traversal was done so that any multi-homed buffers that were
		// skipped (and counted) will be skipped here also.
		if(!(flags & BW_All) || pBuf->windCount == 0) {
			if((status = saveBuf(pBuf, flags, &workDir, pBuf == sess.cur.pBuf ? sess.pCurScrn->workDir : NULL,
			 &savedCount, &narCount)) <= UserAbort)
				return sess.rtn.status;
			}
		if(!(flags & BW_All) || (pNode = heach(&each)) == NULL)
			break;
		pBuf = bufPtr(pNode->pVal);
		}

	// Return to original working directory if needed and set return message.
	if(workDir != sess.pCurScrn->workDir)
		(void) chgdir(sess.pCurScrn->workDir);
	if((flags & (BW_All | BW_QuickExit)) == BW_All && sess.rtn.status == Success) {
		char workBuf[WorkBufSize];

		sprintf(workBuf, text167, savedCount, savedCount != 1 ? "s" : "");
				// "%u buffer%s saved"
		if(narCount > 0)
			sprintf(strchr(workBuf, '\0'), text458, narCount, narCount != 1 ? "s" : "");
					// ", %u narrowed buffer%s skipped"
		(void) rsset(Success, RSForce | RSNoFormat, workBuf);
		}
	return sess.rtn.status;
	}

// Write a buffer to an open file descriptor (disk file or pipe).  Return status.
int writeDiskPipe(Buffer *pBuf, uint *pLineCount) {
	Line *pLine, *pLineEnd;
	uint lineCt;
	bool addNL = pBuf->flags & BFNarrowed && pBuf->pNarBotLine != NULL;

	if(mlputs(text148, MLHome | MLWrap | MLFlush) != Success)
			// "Writing data..."
		return sess.rtn.status;

	// Check if buffer is narrowed and/or ATerm mode is set.  If buffer is narrowed and bottom portion exists, ignore ATerm
	// mode and add a newline to output file at EOB; otherwise, append a line delimiter to the buffer if ATerm mode set and
	// last line not empty.
	pLineEnd = (pLine = pBuf->pFirstLine)->prev;
	if(!addNL && pLineEnd->used > 0 && modeSet(MdIdxATerm, pBuf)) {
		Line *pLine0;

		if(lalloc(0, &pLine0) != Success)	// Empty line.
			return sess.rtn.status;		// Fatal error.
		llink(pLine0, pBuf, NULL);		// Append empty line (newline) to end of buffer...
		bchange(pBuf, WFHard);			// set "change" flags...
		pLineEnd = pLine->prev;			// and update "last line" pointer.
		}

	// Write the current buffer's lines to the open file.
	lineCt = 0;
	while(pLine != pLineEnd || addNL || pLine->used > 0) {
		if(f_putline(pLine, pLine != pLineEnd || addNL) != Success) {

			// Write or close error: clean up and get out.
			(void) f_close(true);
			return sess.rtn.status;
			}
		++lineCt;
		if((pLine = pLine->next) == NULL)
			break;
		}

	// Write was successful: close output file and return.
	if(pLineCount != NULL)
		*pLineCount = lineCt;
	return f_close(true);
	}

// Set the filename associated with a buffer and call the filename hook if appropriate (or return an error if the buffer is
// read-only, a user command or function, or executing).  If interactive mode and default n, use current buffer for target
// buffer.  If the new filename is nil or null, set the filename to null; otherwise, if n > 0 or is the default, change the
// buffer name to match the filename.  pRtnVal is set to a two-element array containing new buffer name and new filename.
// Return status.
int setBufFile(Datum *pRtnVal, long n, Datum **args) {
	Buffer *pBuf;

	// Get buffer.
	if(n == DefN && !(sess.opFlags & OpScript))
		pBuf = sess.cur.pBuf;
	else if(getBufname(pRtnVal, text151, OpDelete | OpDefBName, &pBuf, NULL) != Success || pBuf == NULL)
			// "Set filename in"
		return sess.rtn.status;

	// Check if filename change allowed.
	if(bfchange(pBuf) != Success)
		return sess.rtn.status;

	// Get filename.
	if(sess.opFlags & OpScript)
		dxfer(pRtnVal, args[0]);
	else {
		TermInpCtrl termInpCtrl = {pBuf->filename, RtnKey, MaxPathname, NULL};
		char *str, workBuf[strlen(text151) + 16];

		if(n == DefN) {
			str = stpcpy(workBuf, text151);
				// "Set filename in"
			do {
				--str;
				} while(*str != ' ');
			strcpy(str + 1, text339);
				// "to"
			str = workBuf;
			}
		else
			str = (char *) text339;
		if(termInp(pRtnVal, str, ArgNil1, TermC_Filename | TermC_NoAuto, &termInpCtrl) != Success)
			return sess.rtn.status;
		}

	// Activate buffer if needed.
	if(!(pBuf->flags & BFActive) && bactivate(pBuf) != Success)
		return sess.rtn.status;

	// Set new filename (which may be null or nil).
	if(disnil(pRtnVal))
		dsetnull(pRtnVal);
	if(setfile(pBuf, pRtnVal, n == DefN || n > 0 ? BF_WarnExists | BF_DeriveName : BF_WarnExists) != Success)
		return sess.rtn.status;

	// Set pRtnVal to result (an array).
	Array *pArray;
	if((pArray = anew(2, NULL)) == NULL || dsetstr(pBuf->bufname, pArray->elements[0]) != 0)
		goto LibFail;
	if(*pRtnVal->str == '\0')
		dsetnil(pArray->elements[1]);
	else if(dsetstr(pRtnVal->str, pArray->elements[1]) != 0)
LibFail:
		return libfail();
	astash(pRtnVal, pArray);

	// Update mode line.
	supd_windFlags(pBuf, WFMode);

	return sess.rtn.status;
	}
