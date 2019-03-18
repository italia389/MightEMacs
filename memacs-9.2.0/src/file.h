// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// file.h	File management definitions for MightEMacs.

// Flags for iostat() function.
#define IOS_OtpFile	0x0001		// Writing to a file.
#define IOS_NoDelim	0x0002		// No delimiter at EOF.

// File information.  Any given file is opened, processed, and closed before the next file is dealt with; therefore, the file
// handle (and control variables) can be shared among all files and I/O functions.  Note however, that inpdelim, otpdelim, and
// otpdelimlen are permanent and maintained by the user.
#define SizeFileBuf	32768		// Size of file input buffer.
#define SizeLineBuf	256		// Initial size of line input buffer.
typedef struct {
	char *fname;			// Filename passed to ffropen() or ffwopen().
	int fd;				// File descriptor.
	int stdinfd;			// File descriptor to use for file read from standard input.
	bool eof;			// End-of-file flag.
	char inpdelim[NDelim + 1];	// User-assigned input line delimiter(s).
	int idelim1,idelim2;		// Actual input line delimiter(s) for file being read.
	char otpdelim[NDelim + 1];	// User-assigned output line delimiter(s).
	ushort otpdelimlen;		// Length of user output delimiter string.
	char *odelim;			// Actual output line delimiter(s) for file being written.
	ushort odelimlen;		// Length of actual output delimiter string.
	char *lbuf;			// Pointer to input line buffer (on heap).
	char *lbufc,*lbufz;		// Line buffer pointers.
	char iobuf[SizeFileBuf];	// I/O buffer.
	char *iobufc,*iobufz;		// I/O buffer pointers.
	} FileInfo;

// Data-insertion object used by idata() function.
typedef struct {
	Buffer *targbufp;		// Target buffer.
	Dot *targdotp;			// Target line.
	char *msg;			// Progress message.
	int status;			// Returned status.
	uint lineCt;			// Returned line count.
	bool finalDelim;		// Returned "last line has delimiter" flag.
	} DataInsert;

// External function declarations.
extern int aPathname(Datum *rp,int n,Datum **argpp);
extern int awfile(Datum *rp,int n,char *prmt,short mode);
extern char *fbasename(char *name,bool withext);
extern char *fdirname(char *name,int n);
extern int fexist(char *fn);
extern int fvfile(Datum *rp,int n,bool view);
extern int getpath(char *fname,bool resolve,Datum *pathp);
extern int globpat(Datum *rp,int n,Datum **argpp);
extern int gtfilename(Datum *rp,char *prmt,char *def,uint flags);
extern int idata(int n,Buffer *srcbufp,DataInsert *dip);
extern int insertFile(Datum *rp,int n,Datum **argpp);
extern int insline(char *src,int len,bool hasDelim,Buffer *bufp,Dot *dotp);
extern int iostat(DStrFab *sfp,ushort flags,Datum *bknamep,int status,char *fname,char *lcmsg,uint lineCt);
extern int readFP(Datum *rp,int n,char *fname,ushort flags);
extern int readin(struct Buffer *bufp,char *fname,ushort flags);
extern int readprep(Buffer *bufp,ushort flags);
#if USG
extern int rename(const char *file1,const char *file2);
#endif
extern int rinit(int fd);
extern int savebufs(int n,ushort flags);
extern int setBufFile(Datum *rp,int n,Datum **argpp);
extern void winit(Buffer *bufp,int fd);
extern int writefd(Buffer *bufp,uint *lcp);
extern int writeout(struct Buffer *bufp,char *fn,short mode,ushort flags);
extern int xPathname(Datum *rp,int n,Datum **argpp);

#ifdef DATAfile

// **** For file.c ****

// Global variables.
FileInfo fi = {				// File I/O information.
	NULL,-1,-1,false,"",-1,-1,"",0U,NULL,0U,NULL,NULL,NULL,"",NULL,NULL
	};
#else

// **** For all the other .c files ****

// External variable declarations.
extern FileInfo fi;
#endif
