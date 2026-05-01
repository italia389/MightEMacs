// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// file.h	File management definitions for MightEMacs.

// Flags for ioStat() function.
#define IOS_OtpFile	0x0001		// Writing to a file.
#define IOS_NoDelim	0x0002		// No delimiter at EOF.
#define IOS_RSHigh	0x0004		// Set RSHigh flag in return message.

// Flags for selected buffer and file operations, used by opConfirm() and setFilename() functions.
#define BF_UpdBufDir	0x0001		// Update buffer's home directory.
#define BF_WarnExists	0x0002		// Warn user if file exists.
#define BF_Overwrite	0x0004		// Confirm file overwrite.
#define BF_CreateBuf	0x0008		// Confirm buffer creation.
#define BF_DeriveName	0x0010		// Derive new buffer name from filename.

// Flags for fileExists() function.
#define FTypRegular	0x0001		// Regular file.
#define FTypSymLink	0x0002		// Symbolic link.
#define FTypDir		0x0004		// Directory.
#define FTypOther	0x0008		// Other file type.

// File information.  Any given file is opened, processed, and closed before the next file is dealt with; therefore, the file
// handle (and control variables) can be shared among all files and I/O functions.  Note however, that userInpDelim and
// userOtpDelim are permanent and maintained by the user.
#define FIEOF		0x0001		// Hit EOF on input.
#define FIRetry		0x0002		// Read from or write to a pipe with retries.

#define FIFileBufSize	32768		// Size of file input buffer.
#define FILineBufSize	256		// Initial size of line input buffer.
#define FIPipeDelay	3		// Initial number of centiseconds to pause between I/O attempts (via a pipe).
#define FIPipeAttempts	5		// Number of I/O attempts (via a pipe) before giving up.

// Line delimiter types.
#define FDelimUndef	0		// No delimiter defined.
#define FDelimNUL	1		// Null byte delimiter.
#define FDelimCR	2		// Carriage return delimiter.
#define FDelimCRLF	3		// Carriage return + line feed delimiter.
#define FDelimNL	4		// Newline delimiter.

typedef struct {
	const char *filename;		// Filename passed to f_ropen() or f_wopen().
	int fileHandle;			// File descriptor.
	int stdInpFileHandle;		// File descriptor to use for file read from standard input.
	ushort flags;			// File flags, including EOF indicator.
	ushort userInpDelimSym;		// User-assigned input line delimiter symbol.
	short inpDelim1, inpDelim2;	// Actual input line delimiter(s) for file being read.
	ushort userOtpDelimSym;		// User-assigned output line delimiter symbol.
	ushort otpDelimLen;		// Actual output line delimiter length for file being written.
	char *pOtpDelim;		// Pointer to actual output line delimiter string for file being written.
	char *lineBuf;			// Pointer to input line buffer (on heap).
	char *lineBufCur, *lineBufEnd;	// Line buffer pointers.
	char dataBuf[FIFileBufSize];	// Bulk I/O buffer.
	char *ioBufCur, *ioBufEnd;	// Bulk I/O buffer pointers.
	} FileInfo;

// Data-insertion object used by insertData() function.
typedef struct {
	Buffer *pTargBuf;		// Target buffer.
	Point *pTargPoint;		// Target line.
	const char *msg;		// Progress message.
	int status;			// Returned status.
	uint lineCt;			// Returned line count.
	bool finalDelim;		// Returned "last line has delimiter" flag.
	} DataInsert;

// External function declarations.
extern int absPath(char *filename, bool resolve, Datum *pPath);
extern int absPathname(Datum *pRtnVal, long n, Datum **args);
extern int appendWriteFile(Datum *pRtnVal, long n, const char *prompt, short fmode);
extern int delFile(Datum *pRtnVal, long n, Datum **args);
extern const char *delimSymToStr(ushort type);
extern uint fileExists(const char *filename);
extern int findViewFile(Datum *pRtnVal, long n, bool view);
extern int getFilename(Datum *pRtnVal, const char *basePrompt, const char *defName, uint flags);
extern int globPat(Datum *pRtnVal, long n, Datum **args);
extern int inpInit(int fileHandle);
extern int insertData(long n, Buffer *pSrcBuf, DataInsert *pDataInsert);
extern int insertFile(Datum *pRtnVal, long n, Datum **args);
extern int insertLine(const char *src, int len, bool hasDelim, Buffer *pBuf, Point *pPoint);
extern int ioStat(DFab *rtnMsg, uint flags, Datum *pBakName, int status, const char *filename, const char *action, uint lineCt);
extern int linkFile(Datum *pRtnVal, long n, Datum **args);
extern bool opConfirm(uint flags);
extern void otpInit(Buffer *pBuf, int fileHandle);
extern int readFile(Datum *pRtnVal, long n, Datum **args);
extern int readIn(Buffer *pBuf, const char *filename, uint flags);
extern int readPrep(Buffer *pBuf, uint flags);
#if USG
extern int rename(const char *file1, const char *file2);
#endif
extern int renameFile(Datum *pRtnVal, long n, Datum **args);
extern int saveBufs(long n, uint flags);
extern int setBufFile(Datum *pRtnVal, long n, Datum **args);
extern int writeDiskPipe(Buffer *pBuf, uint *pLineCount);
extern int xPathname(Datum *pRtnVal, long n, Datum **args);

#ifdef FileData

// **** For file.c ****

// Global variables.
FileInfo fileInfo = {			// File I/O information.
	NULL, -1, -1, 0, FDelimUndef, -1, -1, FDelimUndef, 0, NULL, NULL, NULL, NULL, "", NULL, NULL
	};
Hash fileLinkKeepers;			// Symbolic and hard links to preserve (per user) on output.
#else

// **** For all the other .c files ****

// External variable declarations.
extern FileInfo fileInfo;
extern Hash fileLinkKeepers;
#endif
