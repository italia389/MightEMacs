// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// file.h	File management definitions for MightEMacs.

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
	char *lbufw,*lbufz;		// Line buffer pointers.
	char iobuf[SizeFileBuf];	// I/O buffer.
	char *iobufw,*iobufz;		// I/O buffer pointers.
	} FInfo;

// External function declarations.
extern int awfile(Datum *rp,int n,char *prmt,int mode);
extern char *fbasename(char *name,bool withext);
extern char *fdirname(char *name,int n);
extern int fexist(char *fn);
extern int ffclose(bool otpfile);
extern int ffgetline(uint *lenp);
extern int ffputline(char *buf,int buflen);
extern int ffropen(char *fn,bool required);
extern int ffwopen(Buffer *bufp,char *fn,int mode);
extern int getfile(Datum *rp,int n,bool view);
extern int getpath(Datum *pathp,int n,char *fname);
extern int gtfilename(Datum *rp,char *prmt,char *def,uint flags);
extern int ifile(char *fname,int n);
extern int insertFile(Datum *rp,int n,Datum **argpp);
extern int insertPipe(Datum *rp,int n,Datum **argpp);
extern int rdfile(Datum *rp,int n,char *fname,ushort flags);
extern int readin(struct Buffer *bufp,char *fname,ushort flags);
extern int readPipe(Datum *rp,int n,Datum **argpp);
#if USG
extern int rename(const char *file1,const char *file2);
#endif
extern int savebufs(int n,ushort flags);
extern int setBufFile(Datum *rp,int n,Datum **argpp);
extern int writeout(struct Buffer *bufp,char *fn,int mode,ushort flags);

#ifdef DATAfile

// **** For file.c ****

// Global variables.
FInfo fi = {				// File I/O information.
	NULL,-1,-1,false,"",-1,-1,"",0U,NULL,0U,NULL,NULL,NULL,"",NULL,NULL
	};
#else

// **** For all the other .c files ****

// External variable declarations.
extern FInfo fi;
#endif
