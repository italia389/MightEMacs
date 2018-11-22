// ProLib (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// datum.c		Datum object routines.

#include "os.h"
#include "plexcep.h"
#include "plstring.h"
#if DDebug
#include "plio.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

// Local definitions.
#if DSFTest
#define DChunkSz0	64			// Starting size of string-fab chunks/pieces.  *TEST VALUE*
#define DChunkSz4	256
#define DChunkSzMax	16384
#else
#define DChunkSz0	128			// Starting size of string-fab chunks/pieces.
#define DChunkSz4	1024			// Size at which to begin quadrupling until hit maximum.
#define DChunkSzMax	262144			// Maximum size (256K).
#endif

// Global variables.
Datum *datGarbp = NULL;			// Head of list of temporary datum objects, for "garbage collection".

#if DDebug
extern FILE *logfile;

// Dump datum object to log file.
void ddump(Datum *datp,char *tag) {
	char *label;
	char *membName;

	fprintf(logfile,"%s\n\taddr: %.8X\n",tag,(uint) datp);
	fflush(logfile);
	if(datp != NULL)
		switch(datp->d_type) {
			case dat_nil:
				label = "nil";
				goto Konst;
			case dat_false:
				label = "false";
				goto Konst;
			case dat_true:
				label = "true";
Konst:
				fprintf(logfile,"\t%s\n",label);
				break;
			case dat_int:
				fprintf(logfile,"\tint: %ld\n",datp->u.d_int);
				break;
			case dat_uint:
				fprintf(logfile,"\tuint: %lu\n",datp->u.d_uint);
				break;
			case dat_real:
				fprintf(logfile,"\treal: %.2lf\n",datp->u.d_real);
				break;
			case dat_miniStr:
				label = "MINI STR";
				goto string;
			case dat_soloStr:
			case dat_soloStrRef:
				label = (datp->d_type == dat_soloStr) ? "HEAP STR" : "REF STR";
				if(datp->u.d_solo == NULL) {
					membName = "u.d_solo";
					goto bad;
					}
string:
				if(datp->d_str == NULL) {
					membName = "d_str";
bad:
					fprintf(logfile,
					 "*** %s member of datum object passed to ddump() is NULL ***\n",membName);
					}
				else {
					short c;
					char *str = datp->d_str;
					fprintf(logfile,"\t%s (%hu): \"",label,datp->d_type);
					while((c = *str++) != '\0')
						fputs(vizc(c,VBaseDef),logfile);
					fputs("\"\n",logfile);
					}
				break;
			case dat_blob:
			case dat_blobRef:
				fprintf(logfile,"\t%s (size %lu): %.8X\n",datp->d_type == dat_blobRef ? "BLOBREF" : "BLOB",
				 datp->u.d_blob.b_size,(uint) datp->u.d_blob.b_memp);
				break;
			}
	}
#endif

// Initialize a datum object to nil.  It is assumed any heap storage has already been freed.
#if DCaller
void dinit(Datum *datp,char *caller) {
#else
void dinit(Datum *datp) {
#endif
	datp->d_type = dat_nil;
	datp->d_str = NULL;
#if DCaller
	fprintf(logfile,"dinit(): initialized object %.8X for %s()\n",(uint) datp,caller);
#endif
	}

// Clear a datum object and set it to nil.
void dclear(Datum *datp) {

	// Free any string or blob storage.
	switch(datp->d_type) {
		case dat_soloStr:
#if DDebug
			fprintf(logfile,"dclear(): free [%.8X] SOLO ",(uint) datp->d_str);
			ddump(datp,"from...");
			free((void *) datp->d_str);
			fputs("dclear(): DONE.\n",logfile);
			fflush(logfile);
#else
			free((void *) datp->d_str);
#endif
			break;
		case dat_blob:
#if DDebug
			fprintf(logfile,"dclear(): free [%.8X] BLOB, size %lu",(uint) datp->u.d_blob.b_memp,
			 datp->u.d_blob.b_size);
			ddump(datp,"from...");
			if(datp->u.d_blob.b_memp != NULL)
				free((void *) datp->u.d_blob.b_memp);
			fputs("dclear(): DONE.\n",logfile);
			fflush(logfile);
#else
			if(datp->u.d_blob.b_memp != NULL)
				free((void *) datp->u.d_blob.b_memp);
#endif
		}
#if DCaller
	dinit(datp,"dclear");
#else
	dinit(datp);
#endif
	}

// Set a datum object to a null "mini" string.
void dsetnull(Datum *datp) {

	dclear(datp);
	*(datp->d_str = datp->u.d_miniBuf) = '\0';
	datp->d_type = dat_miniStr;
	}

// Set a Boolean value in a datum object.
void dsetbool(bool b,Datum *datp) {

	dclear(datp);
	datp->d_type = b ? dat_true : dat_false;
	datp->d_str = NULL;
	}

// Set a blob value in a datum object (copy to heap), given void pointer to object and size in bytes, which may be zero.  Return
// status code.
int dsetblob(void *memp,size_t size,Datum *datp) {
	DBlob *blobp = &datp->u.d_blob;

	dclear(datp);
	datp->d_type = dat_blob;
	datp->d_str = NULL;
	if((blobp->b_size = size) == 0)
		blobp->b_memp = NULL;
	else {
		if((blobp->b_memp = malloc(size)) == NULL) {
			plexcep.flags |= ExcepMem;
			return emsge(-1);
			}
		memcpy(blobp->b_memp,memp,size);
		}
	return 0;
	}

// Set a blob reference in a datum object, given void pointer to object and size in bytes.
void dsetblobref(void *memp,size_t size,Datum *datp) {
	DBlob *blobp = &datp->u.d_blob;

	dclear(datp);
	datp->d_type = dat_blobRef;
	datp->d_str = NULL;
	blobp->b_memp = memp;
	blobp->b_size = size;
	}

// Set a single-character (string) value in a datum object.
void dsetchr(short c,Datum *datp) {

	dclear(datp);
	(datp->d_str = datp->u.d_miniBuf)[0] = c;
	datp->u.d_miniBuf[1] = '\0';
	datp->d_type = dat_miniStr;
	}

// Set a signed integer value in a datum object.
void dsetint(long i,Datum *datp) {

	dclear(datp);
	datp->u.d_int = i;
	datp->d_type = dat_int;
	datp->d_str = NULL;
	}

// Set an unsigned integer value in a datum object.
void dsetuint(ulong u,Datum *datp) {

	dclear(datp);
	datp->u.d_uint = u;
	datp->d_type = dat_uint;
	datp->d_str = NULL;
	}

// Set a real number value in a datum object.
void dsetreal(double d,Datum *datp) {

	dclear(datp);
	datp->u.d_real = d;
	datp->d_type = dat_real;
	datp->d_str = NULL;
	}

// Get space for a string and set *strp to it.  If a mini string will work, use caller's mini buffer "minibufp" (if not NULL).
// Return status code.
static int salloc(char **strp,size_t len,char *minibufp) {

	if(minibufp != NULL && len <= sizeof(DBlob))
		*strp = minibufp;
	else if((*strp = (char *) malloc(len)) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	return 0;
	}

// Allocate a string value of given size in a datum object.  Return status code.
int dsalloc(Datum *datp,size_t len) {

	dsetnull(datp);
	if(len > sizeof(DBlob)) {
#if DDebug
		fprintf(logfile,"dsalloc(): getting heap string for %.8X for len %lu...\n",(uint) datp,len);
#endif
		if(salloc(&datp->u.d_solo,len,NULL) != 0)
			return -1;
		*(datp->d_str = datp->u.d_solo) = '\0';
		datp->d_type = dat_soloStr;
		}
#if DDebug
	else
		fprintf(logfile,"dsalloc(): retaining MINI string (in datum %.8X) for len %lu\n",(uint) datp,len);
#endif
	return 0;
	}

// Set a string reference in a datum object.
static void dsetrstr(char *str,Datum *datp,DatumType t) {

	dclear(datp);
	datp->d_str = datp->u.d_solo = str;
	datp->d_type = t;
	}

// Set a string value currently on the heap in a datum object.
void dsetmemstr(char *str,Datum *datp) {

	dsetrstr(str,datp,dat_soloStr);
	}

// Set a string reference in a datum object.
void dsetstrref(char *str,Datum *datp) {

	dsetrstr(str,datp,dat_soloStrRef);
	}

// Set a substring (which is assumed to not contain any embedded null bytes) in a datum object, given pointer and length.
// Routine also assumes that the source string may be all or part of the dest object.  Return status code.
int dsetsubstr(char *str,size_t len,Datum *datp) {
	char *str0;
	char wkbuf[sizeof(DBlob)];

	if(salloc(&str0,len + 1,wkbuf) != 0)	// Get space for string...
		return -1;
	stplcpy(str0,str,len + 1);		// and copy string to new buffer.
	if(str0 == wkbuf) {			// If mini string...
		dsetnull(datp);			// clear datp...
		strcpy(datp->u.d_miniBuf,wkbuf);// and copy it.
		}
	else
		dsetmemstr(str0,datp);		// Otherwise, set heap string in datp.
	return 0;
	}

// Set a string value in a datum object.  Return status code.
int dsetstr(char *str,Datum *datp) {

	return dsetsubstr(str,strlen(str),datp);
	}

// Transfer contents of one datum object to another.  Return destp.
Datum *datxfer(Datum *destp,Datum *srcp) {
	Datum *datp = destp->d_nextp;		// Save the "next" pointer...
#if DDebug
	static char xMsg[64];
	sprintf(xMsg,"datxfer(): srcp [%.8X] BEFORE...",(uint) srcp);
	ddump(srcp,xMsg);
	sprintf(xMsg,"datxfer(): destp [%.8X] BEFORE...",(uint) destp);
	ddump(destp,xMsg);
#endif
	dclear(destp);				// free dest...
	*destp = *srcp;				// copy the whole burrito...
	destp->d_nextp = datp;			// restore "next" pointer...
	if(destp->d_type == dat_miniStr)	// fix mini-string pointer...
		destp->d_str = destp->u.d_miniBuf;
#if DCaller
	dinit(srcp,"datxfer");
#else
	dinit(srcp);				// initialize the source...
#endif
#if DDebug
	ddump(srcp,"datxfer(): srcp AFTER...");
	ddump(destp,"datxfer(): destp AFTER...");
#endif
	return destp;				// and return result.
	}

// Return true if a datum object is a false Boolean value; otherwise, false.
bool disfalse(Datum *datp) {

	return datp->d_type == dat_false;
	}

// Return true if a datum object is nil; otherwise, false.
bool disnil(Datum *datp) {

	return datp->d_type == dat_nil;
	}

// Return true if a datum object is a null string; otherwise, false.
bool disnull(Datum *datp) {

	return (datp->d_type & DStrMask) && *datp->d_str == '\0';
	}

// Return true if a datum object is a true Boolean value; otherwise, false.
bool distrue(Datum *datp) {

	return datp->d_type == dat_true;
	}

// Create datum object and set *datpp to it.  If "track" is true, add it to the garbage collection stack.  Return status code.
#if DCaller
static int dmake(Datum **datpp,bool track,char *caller,char *dName) {
#else
static int dmake(Datum **datpp,bool track) {
#endif
	Datum *datp;

#if DDebug && 0
	fputs("dmake(): Dump object list...\n",logfile);
	for(datp = datGarbp; datp != NULL; datp = datp->d_nextp)
		ddump(datp,"dmake() DUMP");
#endif
	// Get new object...
	if((datp = (Datum *) malloc(sizeof(Datum))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
#if DDebug
#if DCaller
fprintf(logfile,"dmake(): Allocated %.8X, type %s for %s(), var '%s'\n",(uint) datp,track ? "TRACK" : "Perm",caller,dName);
#else
fprintf(logfile,"dmake(): Allocated %.8X, type %s\n",(uint) datp,track ? "TRACK" : "Perm");
#endif
#endif
	// add it to garbage collection stack (if applicable)...
	if(track) {
		datp->d_nextp = datGarbp;
		datGarbp = datp;
		}
	else
		datp->d_nextp = NULL;

	// initialize it...
#if DCaller
	dinit(datp,"dmake");
#else
	dinit(datp);
#endif
	// and return the spoils.
	*datpp = datp;
	return 0;
	}

// Create datum object and set *datpp to it.  Object is not added it to the garbage collection stack.  Return status code.
#if DCaller
int dnew(Datum **datpp,char *caller,char *dName) {

	return dmake(datpp,false,caller,dName);
#else
int dnew(Datum **datpp) {

	return dmake(datpp,false);
#endif
	}

// Create (tracked) datum object, set *datpp to it, and add it to the garbage collection stack.  Return status code.
#if DCaller
int dnewtrk(Datum **datpp,char *caller,char *dName) {

	return dmake(datpp,true,caller,dName);
#else
int dnewtrk(Datum **datpp) {

	return dmake(datpp,true);
#endif
	}

// Copy a byte string into a string-fab object's work buffer, left or right justified.  Source string may already be in the
// buffer.
static void sfcpy(DStrFab *sfp,char *str,size_t len) {

	if(len > 0) {
		if((sfp->sf_flags & SFModeMask) != SFPrepend) {

			// Appending to existing byte string: copy it into the work buffer.
			sfp->sf_bufp = (char *) memcpy((void *) sfp->sf_buf,(void *) str,len) + len;
			}
		else {
			// Prepending to existing byte string or right-shifting the data in the work buffer.
			char *dest = sfp->sf_bufpz;
			str += len;
			do {
				*--dest = *--str;
				} while(--len > 0);
			sfp->sf_bufp = dest;
			}
		}
	}

// Save a byte string to a string-fab object's chunk list.  Return status code.
static int sfsave(char *str,size_t len,DStrFab *sfp) {
	DChunk *ckp;

	// Get new chunk...
	if((ckp = (DChunk *) malloc(sizeof(DChunk))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	ckp->ck_blob.b_memp = (void *) str;
	ckp->ck_blob.b_size = len;
#if DDebug
	fputs("sfsave(",logfile);
	fvizs((void *) str,len > 32 ? 32 : len,logfile);
	fprintf(logfile," [%.8X],%lu,%.8X): done.\n",(uint) str,len,(uint) sfp);
#endif
	// and push it onto the stack in the string-fab object.
	ckp->ck_nextp = sfp->sf_stack;
	sfp->sf_stack = ckp;

	return 0;
	}

// Grow work buffer in a string-fab object.  Assume minSize is less than DChunkSzMax.  If not initial allocation and prepending,
// the data in the work buffer is shifted right after the work buffer is extended.  Return status code.
static int sfgrow(DStrFab *sfp,size_t minSize) {
	char *bp;
	size_t size,used;

	// Initial allocation?
	if(sfp->sf_buf == NULL) {
		size = (minSize < DChunkSz0) ? DChunkSz0 : (minSize < DChunkSz4) ? DChunkSz4 : DChunkSzMax;
		goto Init;
		}

	// Nope... called from dputc().  Work buffer is full.
	else {
		// Double or quadruple old size?
		size = used = sfp->sf_bufpz - (bp = sfp->sf_buf);
		if(size < DChunkSz4)
			size *= 2;
		else if(size < DChunkSzMax)
			size *= 4;

		// No, already at max.  Save current buffer and allocate new one.
		else {
			if(sfsave(bp,size,sfp) != 0)
				return -1;
Init:
			bp = NULL;
			used = 0;
			}
		}

	// Get more space and reset pointers.
#if DDebug
	fprintf(logfile,"sfgrow(%.8X,%lu): Calling realloc() at %.8X, size %lu, used %lu...\n",(uint) sfp,minSize,
	 (uint) bp,size,used);
#endif
	if((sfp->sf_buf = (char *) realloc((void *) bp,size)) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	sfp->sf_bufpz = sfp->sf_buf + size;
	if((sfp->sf_flags & SFModeMask) != SFPrepend)
		sfp->sf_bufp = sfp->sf_buf + used;
	else {
		sfp->sf_bufp = sfp->sf_bufpz - used;
		if(bp != NULL)

			// Extended old buffer in "prepend" mode... right-shift contents.
			sfcpy(sfp,sfp->sf_buf,used);
		}
#if DDebug
	fprintf(logfile,"sfgrow(): done.  sf_buf %.8X, sf_bufp %.8X, sf_bufpz %.8X\n",
	 (uint) sfp->sf_buf,(uint) sfp->sf_bufp,(uint) sfp->sf_bufpz);
#endif
	return 0;
	}

// Put a character to a string-fab object.  Return status code.
int dputc(short c,DStrFab *sfp) {

	if((sfp->sf_flags & SFModeMask) == SFPrepend) {

		// Save current chunk if no space left.
		if(sfp->sf_bufp == sfp->sf_buf && sfgrow(sfp,0) != 0)
			return -1;

		// Save the character.
		*--sfp->sf_bufp = c;
		}
	else {
		// Save current chunk if no space left.
		if(sfp->sf_bufp == sfp->sf_bufpz && sfgrow(sfp,0) != 0)
			return -1;

		// Save the character.
		*sfp->sf_bufp++ = c;
		}
#if DDebug
	fprintf(logfile,"dputc(): Saved char %s at %.8X, sf_buf %.8X, buflen %lu.\n",
	 vizc(c,VBaseDef),(uint) sfp->sf_bufp - ((sfp->sf_flags & SFModeMask) == SFPrepend) ? 0 : 1,(uint) sfp->sf_buf,
	 sfp->sf_bufpz - sfp->sf_buf);
#endif
	return 0;
	}

// "Unput" a character from a string-fab object.  Guaranteed to always work once, if at least one byte was previously put.
// Return status code, including error if work buffer is empty.
int dunputc(DStrFab *sfp) {

	// Any bytes in work buffer?
	if((sfp->sf_flags & SFModeMask) == SFPrepend) {
		if(sfp->sf_bufp < sfp->sf_bufpz) {
			++sfp->sf_bufp;
			return 0;
			}
		}
	else if(sfp->sf_bufp > sfp->sf_buf) {
		--sfp->sf_bufp;
		return 0;
		}

	// Empty buffer.  Return error.
	return emsg(-1,"No bytes left to \"unput\"");
	}

// Put a string to a string-fab object.  Return status code.
int dputs(char *str,DStrFab *sfp) {

	if((sfp->sf_flags & SFModeMask) == SFPrepend) {
		char *str1 = strchr(str,'\0');
		while(str1 > str)
			if(dputc(*--str1,sfp) != 0)
				return -1;
		}
	else {
		while(*str != '\0')
			if(dputc(*str++,sfp) != 0)
				return -1;
		}
	return 0;
	}

// Put a byte string to a string-fab object.  Return status code.
int dputmem(const void *mem,size_t len,DStrFab *sfp) {

	if((sfp->sf_flags & SFModeMask) == SFPrepend) {
		char *str = (char *) mem + len;
		while(len-- > 0)
			if(dputc(*--str,sfp) != 0)
				return -1;
		}
	else {
		char *str = (char *) mem;
		while(len-- > 0)
			if(dputc(*str++,sfp) != 0)
				return -1;
		}
	return 0;
	}

// Put the contents of a datum object to a string-fab object.  Return status code.
int dputd(Datum *datp,DStrFab *sfp) {
	char *str;
	char wkbuf[80];

	switch(datp->d_type) {
		case dat_nil:
			return 0;
		case dat_miniStr:
		case dat_soloStr:
		case dat_soloStrRef:
			str = datp->d_str;
			goto put;
		case dat_int:
			sprintf(str = wkbuf,"%ld",datp->u.d_int);
			goto put;
		case dat_uint:
			sprintf(str = wkbuf,"%lu",datp->u.d_uint);
			goto put;
		case dat_real:
			sprintf(str = wkbuf,"%lf",datp->u.d_real);
put:
			return dputs(str,sfp);
		case dat_blob:
		case dat_blobRef:
			return dputmem(datp->u.d_blob.b_memp,datp->u.d_blob.b_size,sfp);
		}

	// Invalid datum type.
	return emsgf(-1,"Unknown datum type (%hu)",datp->d_type);
	}

// Put formatted text to a string-fab object.  Return status code.
int dputf(DStrFab *sfp,char *fmt,...) {
	char *str;
	va_list argp;

	va_start(argp,fmt);
	(void) vasprintf(&str,fmt,argp);
	va_end(argp);
	if(str == NULL)
		return emsge(-1);

	int rcode = dputs(str,sfp);
	free((void *) str);
	return rcode;
	}

// Flags for string-fabrication-object creation (combined with flags defined in header file).
#define SFTrack		0x0004			// Create tracked datum object.

// Prepare for writing to a string-fab object, given pointer to list object to initialize, (possibly NULL) pointer to
// destination datum object, and flags.  If datp is NULL, create a new datum object (which can be retrieved by the caller from
// the sfp record).  If the SFTrack flag is set, create a tracked object; otherwise, untracked.  If datp is not NULL and the
// SFPrepend or SFAppend flag is set, keep existing string value in datp (and prepend or append to it); otherwise, clear it.
// Return status code.
#if DCaller
static int dprep(DStrFab *sfp,Datum *datp,ushort flags,char *caller,char *dName) {
#else
static int dprep(DStrFab *sfp,Datum *datp,ushort flags) {
#endif

#if DDebug
	fprintf(logfile,"*dprep(...,%.8X,%s,...): BEGIN\n",(uint) datp,(flags & SFAppend) ? "append" :
	 (flags & SFPrepend) ? "prepend" : "clear");
#endif
	sfp->sf_stack = NULL;
	sfp->sf_buf = NULL;

	// Initialize Datum object.
	if(datp == NULL) {
#if DCaller
		if(dmake(&datp,flags & SFTrack,"dprep","datp") != 0)
#else
		if(dmake(&datp,flags & SFTrack) != 0)
#endif
			return -1;
		goto SetNull;
		}
	else if((flags & SFModeMask) == SFClear || !(datp->d_type & DStrMask)) {
		if(datp->d_type != dat_nil)
			flags = (flags & ~SFModeMask) | SFClear;	// Not string or nil: force "clear" mode.
SetNull:
		dsetnull(datp);
		}

	sfp->sf_datp = datp;
	sfp->sf_flags = flags;

	// Initialize string-fab buffers, preserving existing string value in Datum object if appending or prepending.
	if((flags & SFModeMask) != SFClear) {
		size_t used = strlen(datp->d_str);
		if(used < DChunkSzMax) {
			if(sfgrow(sfp,used) != 0)		// Allocate a work buffer.
				return -1;
			if(used > 0) {				// If existing string not null...
				sfcpy(sfp,datp->d_str,used);	// copy it to work buffer, left or right justified...
				dsetnull(datp);			// and set Datum object to null.
				}
#if DDebug
			goto DumpIt;
#else
			return 0;
#endif
			}

		// Appending or prepending to existing string >= DChunkSzMax in length.  Save it in a DChunk object and
		// re-initialize Datum object (without calling free()).
		if(sfsave(datp->d_str,used,sfp) != 0)
			return -1;
		dinit(datp);
		dsetnull(datp);
		}
#if DDebug
	if(sfgrow(sfp,0) != 0)
		return -1;
DumpIt:
#if DCaller
	fprintf(logfile,"dprep(): Initialized SF %.8X for %s(), var '%s': sf_bufp %.8X, sf_buf %.8X\n",(uint) sfp,caller,
	 dName,(uint) sfp->sf_bufp,(uint) sfp->sf_buf);
#else
	fprintf(logfile,"dprep(): Initialized SF %.8X: sf_bufp %.8X, sf_buf %.8X\n",(uint) sfp,(uint) sfp->sf_bufp,
	 (uint) sfp->sf_buf);
#endif
	ddump(datp,"dprep(): post-SF-init...");
	fflush(logfile);
	return 0;
#else
	return sfgrow(sfp,0);
#endif
	}

// Open a string-fab object via dprep().  An untracked datum object will be created.  Return status code.
#if DCaller
int dopen(DStrFab *sfp,char *caller,char *dName) {
#else
int dopen(DStrFab *sfp) {
#endif
	return dprep(sfp,NULL,SFClear);
	}

// Open a string-fab object via dprep().  A tracked datum object will be created.  Return status code.
#if DCaller
int dopentrk(DStrFab *sfp,char *caller,char *dName) {
#else
int dopentrk(DStrFab *sfp) {
#endif
	return dprep(sfp,NULL,SFTrack | SFClear);
	}

// Open a string-fab object via dprep() with given datum object (or NULL) and append flag.  Return status code.
#if DCaller
int dopenwith(DStrFab *sfp,Datum *datp,ushort mode,char *caller,char *dName) {
#else
int dopenwith(DStrFab *sfp,Datum *datp,ushort mode) {
#endif
	return (datp == NULL) ? emsg(-1,"Datum pointer cannot be NULL") : dprep(sfp,datp,mode);
	}

// Return true if a string-fab object is empty; otherwise, false.
bool disempty(DStrFab *sfp) {

	return sfp->sf_bufp == (((sfp->sf_flags & SFModeMask) == SFPrepend) ? sfp->sf_bufpz : sfp->sf_buf) &&
	 sfp->sf_stack == NULL;
	}

// End a string-fab object write operation and convert target datum object to a string or blob, according to "type".  Return
// status code.
int dclose(DStrFab *sfp,DCloseType type) {

	// If still at starting point (no bytes written), change datum object to empty blob if force; otherwise, leave as
	// a null string.
	if(disempty(sfp)) {
		if(type == sf_forceBlob) {
			dsetblobref(NULL,0,sfp->sf_datp);
			sfp->sf_datp->d_type = dat_blob;
			}
		free((void *) sfp->sf_buf);
		}

	// At least one byte was saved.  If still on first chunk, check if it contains any imbedded null bytes and save as
	// correct type or return error as appropriate.
	else {
		char *str;
		size_t len;

		if((sfp->sf_flags & SFModeMask) == SFPrepend) {
			str = sfp->sf_bufp;
			len = sfp->sf_bufpz - str;
			}
		else {
			str = sfp->sf_buf;
			len = sfp->sf_bufp - str;
			}
		if(sfp->sf_stack == NULL) {
			bool isBinary = (memchr(str,'\0',len) != NULL);

			if(isBinary && type == sf_string)
				goto BinErr;
			if(((isBinary || type == sf_forceBlob) ? dsetblob((void *) str,len,sfp->sf_datp) :
			 dsetsubstr(str,len,sfp->sf_datp)) != 0)
				return -1;
			free((void *) sfp->sf_buf);
			}
		else {
			char *buf0;

			// Not first chunk.  Add last one (which can't be empty) to stack and convert to a solo string or blob.
			// Remember pointer to work buffer in buf0 if prepending and work buffer is not full so that pointer can
			// be passed to free() later instead of "str".
			buf0 = (str != sfp->sf_buf) ? sfp->sf_buf : NULL;
			if(sfsave(str,len,sfp) != 0)
				return -1;
			else {
				DChunk *ckp;
				size_t size;
				char *str0;
				bool isBinary;
				Datum *datp = sfp->sf_datp;
				char wkbuf[sizeof(DBlob)];

				// Get total length.
				len = 0;
				ckp = sfp->sf_stack;
				do {
					len += ckp->ck_blob.b_size;
					} while((ckp = ckp->ck_nextp) != NULL);

				// Get heap space.
				if(salloc(&str,len + 1,wkbuf) != 0)
					return -1;
				str0 = str;

				// Copy string pieces into it and check for any imbedded null bytes.
				isBinary = false;
				ckp = sfp->sf_stack;
				if((sfp->sf_flags & SFModeMask) == SFPrepend) {

					// Prepending: copy chunks from beginning to end.
					do {
						size = ckp->ck_blob.b_size;
						if(!isBinary && memchr(ckp->ck_blob.b_memp,'\0',size) != NULL)
							isBinary = true;
						memcpy((void *) str,(void *) ckp->ck_blob.b_memp,size);
						str += size;
						if(buf0 != NULL) {
							free((void *) buf0);
							buf0 = NULL;
							}
						else
							free((void *) ckp->ck_blob.b_memp);
						} while((ckp = ckp->ck_nextp) != NULL);
					*str = '\0';
					}
				else {
					// Not prepending: copy chunks from end to beginning.
					str += len + 1;
					*--str = '\0';
					do {
						size = ckp->ck_blob.b_size;
						if(!isBinary && memchr(ckp->ck_blob.b_memp,'\0',size) != NULL)
							isBinary = true;
						str -= size;
						memcpy((void *) str,(void *) ckp->ck_blob.b_memp,size);
						free((void *) ckp->ck_blob.b_memp);
						} while((ckp = ckp->ck_nextp) != NULL);
					}

				// Set byte string in datum object if able.
				if(isBinary && type == sf_string)
BinErr:
					return emsg(-1,"Cannot convert binary data to string");
				if(isBinary || type == sf_forceBlob) {
					if(str0 == wkbuf)
						return dsetblob((void *) str0,len,datp);
					dsetblobref((void *) str0,len,datp);
					datp->d_type = dat_blob;
					}
				else if(str0 == wkbuf) {
					dsetnull(datp);
					strcpy(datp->u.d_miniBuf,wkbuf);
					}
				else
					dsetmemstr(str0,datp);
				}
			}
		}
#if DDebug
	ddump(sfp->sf_datp,"dclose(): Finalized string...");
#if 0
	fprintf(logfile,"\tSTRING LIST (%hu):\n",datp->d_type);
	for(ckp = datp->u.d_sHeadp; ckp != NULL; ckp = ckp->ck_nextp) {
		fputs("\t\t\"",logfile);
		fvizs(ckp->ck_blob.b_memp,ckp->ck_len,logfile);
		fputs("\"\n",logfile);
		}
#endif
#endif
	return 0;
	}

// Stop tracking a Datum object by removing it from the garbage collection stack (if present).
void duntrk(Datum *datp) {

	if(datp == datGarbp)
		datGarbp = datGarbp->d_nextp;
	else {
		Datum *datp1;
		for(datp1 = datGarbp; datp1 != NULL; datp1 = datp1->d_nextp)
			if(datp1->d_nextp == datp) {
				datp1->d_nextp = datp->d_nextp;
				break;
				}
		}
	}

// Copy one value to another.  Return status code.
int datcpy(Datum *destp,Datum *srcp) {

	switch(srcp->d_type) {
		case dat_nil:
		case dat_false:
		case dat_true:
			dclear(destp);
			destp->d_type = srcp->d_type;
			destp->d_str = NULL;
			break;
		case dat_int:
			dsetint(srcp->u.d_int,destp);
			break;
		case dat_uint:
			dsetuint(srcp->u.d_uint,destp);
			break;
		case dat_real:
			dsetreal(srcp->u.d_real,destp);
			break;
		case dat_miniStr:
		case dat_soloStr:
		case dat_soloStrRef:
			return dsetstr(srcp->d_str,destp);
		case dat_blob:
			return dsetblob(srcp->u.d_blob.b_memp,srcp->u.d_blob.b_size,destp);
		default:	// dat_blobRef
			dsetblobref(srcp->u.d_blob.b_memp,srcp->u.d_blob.b_size,destp);
		}
	return 0;
	}

// Compare one Datum to another and return true if values are equal; otherwise, false.  If "ignore" is true, ignore case in
// string comparisons.
bool dateq(Datum *datp1,Datum *datp2,bool ignore) {

	switch(datp1->d_type) {
		case dat_nil:
		case dat_false:
		case dat_true:
			return datp2->d_type == datp1->d_type;
		case dat_int:
			return (datp2->d_type == dat_int && datp2->u.d_int == datp1->u.d_int) ||
			 (datp2->d_type == dat_uint && datp1->u.d_int >= 0 && datp2->u.d_uint == (ulong) datp1->u.d_int) ||
			 (datp2->d_type == dat_real && datp2->u.d_real == (double) datp1->u.d_int);
		case dat_uint:
			return (datp2->d_type == dat_uint && datp2->u.d_uint == datp1->u.d_uint) ||
			 (datp2->d_type == dat_int && datp2->u.d_int >= 0 && (ulong) datp2->u.d_int == datp1->u.d_uint) ||
			 (datp2->d_type == dat_real && datp2->u.d_real == (double) datp1->u.d_uint);
		case dat_real:
			return (datp2->d_type == dat_real && datp2->u.d_real == datp1->u.d_real) ||
			 (datp2->d_type == dat_int && (double) datp2->u.d_int == datp1->u.d_real) ||
			 (datp2->d_type == dat_uint && (double) datp2->u.d_uint == datp1->u.d_real);
		case dat_miniStr:
		case dat_soloStr:
		case dat_soloStrRef:
			if(datp2->d_type & DStrMask) {
				int (*cmp)(const char *s1,const char *s2) = ignore ? strcasecmp : strcmp;
				return cmp(datp1->d_str,datp2->d_str) == 0;
				}
			return false;
		}

	// dat_blob, dat_blobRef
	return (datp2->d_type & DBlobMask) && datp1->u.d_blob.b_size == datp2->u.d_blob.b_size &&
	 memcmp(datp1->u.d_blob.b_memp,datp2->u.d_blob.b_memp,datp1->u.d_blob.b_size) == 0;
	}

// Delete given datum object.  It assumed that either the object was permanent and not in the garbage collection list, or the
// caller is removing it from the list; for example, when called from dgarbpop().
#if DDebug
#if DCaller
void ddelete(Datum *datp,char *caller) {
#else
void ddelete(Datum *datp) {
#endif
	ddump(datp,"ddelete(): About to free()...");
	fprintf(logfile,"ddelete(): calling dclear(%.8X)",(uint) datp);
#if DCaller
	fprintf(logfile," on object from %s()",caller);
#endif
	fputs("...\n",logfile);
	fflush(logfile);
	dclear(datp);
	fprintf(logfile,"ddelete(): calling free(%.8X)... ",(uint) datp);
	fflush(logfile);
	free((void *) datp);
	fputs("done.\n",logfile); fflush(logfile);
#else
void ddelete(Datum *datp) {

	dclear(datp);
	free((void *) datp);
#endif
	}

// Pop datGarbp to given pointer, releasing heap space and laughing all the way.
void dgarbpop(Datum *datp) {
	Datum *datp1;

#if DDebug
	ddump(datp,"dgarbpop(): Popping to...");
	while(datGarbp != datp) {
		if((datp1 = datGarbp) == NULL) {
			fputs("datGarbp is NULL! Bailing out...\n",logfile);
			break;
			}
		datGarbp = datGarbp->d_nextp;
#if DCaller
		ddelete(datp1,"dgarbpop");
#else
		ddelete(datp1);
#endif
		}
	fputs("dgarbpop(): END\n",logfile);
	fflush(logfile);
#else
	while(datGarbp != datp) {
		datp1 = datGarbp;
		datGarbp = datGarbp->d_nextp;
		ddelete(datp1);
		}
#endif
	}

// Copy a character to sfp (an active string-fab object) in visible form.  Pass "flags" to vizc().  Return status code.
int dvizc(short c,ushort flags,DStrFab *sfp) {
	char *str;

	return (str = vizc(c,flags)) == NULL || dputs(str,sfp) != 0 ? -1 : 0;
	}

// Copy bytes from str to sfp (an active string-fab object), exposing all invisible characters.  Pass "flags" to vizc().  If len
// is zero, assume str is a null-terminated string; otherwise, copy exactly len bytes.  Return status code.
int dvizs(const void *str,size_t len,ushort flags,DStrFab *sfp) {
	char *str1 = (char *) str;
	size_t n = (len == 0) ? strlen(str1) : len;

	for(; n > 0; --n)
		if(dvizc(*str1++,flags,sfp) != 0)
			return -1;
	return 0;
	}

// Copy bytes from str to datp via dvizs().  Return status code.
int dviz(const void *str,size_t len,ushort flags,Datum *datp) {
	DStrFab dest;

	return (dopenwith(&dest,datp,false) != 0 || dvizs(str,len,flags,&dest) != 0 || dclose(&dest,sf_string) != 0) ? -1 : 0;
	}

// Copy string from str to datp, adding a single quote (') at beginning and end and converting single quote characters to '\''. 
// Return status.
int dshquote(char *str,Datum *datp) {
	ptrdiff_t len;
	char *str0,*str1;
	bool apostrophe;
	DStrFab dest;

	str0 = str;
	if(dopenwith(&dest,datp,false) != 0)
		return -1;
	for(;;) {
		if(*str == '\0')				// If null...
			break;					// we're done.
		if(*str == '\'') {				// If ' ...
			apostrophe = true;			// convert it.
			len = 0;
			++str;
			}
		else {
			apostrophe = false;
			if((str1 = strchr(str,'\'')) == NULL)	// Search for next ' ...
				str1 = strchr(str,'\0');	// or null if not found.
			len = str1 - str;			// Length of next chunk to copy.
			str1 = str;
			str += len;
			}

		if(apostrophe) {				// Copy conversion literal.
			if(dputmem("\\'",2,&dest) != 0)
				return -1;
			}					// Copy chunk.
		else if(dputc('\'',&dest) != 0 || dputmem(str1,len,&dest) != 0 || dputc('\'',&dest) != 0)
			return -1;
		}
	if(*str0 == '\0' && dputmem("''",2,&dest) != 0)		// Wrap it up.
		return -1;
	return dclose(&dest,sf_string);
	}

// Return a Datum object as a string, if possible.  If not, set an error and return NULL.
char *dtos(Datum *datp,bool viznil) {
	static char wkbuf[80];

	switch(datp->d_type) {
		case dat_nil:
			return viznil ? "nil" : "";
		case dat_false:
			return "false";
		case dat_true:
			return "true";
		case dat_int:
			sprintf(wkbuf,"%ld",datp->u.d_int);
			break;
		case dat_uint:
			sprintf(wkbuf,"%lu",datp->u.d_uint);
			break;
		case dat_real:
			sprintf(wkbuf,"%lf",datp->u.d_real);
			break;
		case dat_miniStr:
		case dat_soloStr:
		case dat_soloStrRef:
			return datp->d_str;
		case dat_blob:
		case dat_blobRef:
			(void) emsg(-1,"Cannot convert blob to string");
			return NULL;
		}
	return wkbuf;
	}
