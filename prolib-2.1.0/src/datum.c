// ProLib (c) Copyright 2019 Richard W. Marinelli
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
Datum *datGarb = NULL;			// Head of list of temporary datum objects, for "garbage collection".

#if DDebug
extern FILE *logfile;

// Dump datum object to log file.
void ddump(Datum *datum,char *tag) {
	char *label;
	char *membName;

	fprintf(logfile,"%s\n\taddr: %.8X\n",tag,(uint) datum);
	fflush(logfile);
	if(datum != NULL)
		switch(datum->d_type) {
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
				fprintf(logfile,"\tint: %ld\n",datum->u.d_int);
				break;
			case dat_uint:
				fprintf(logfile,"\tuint: %lu\n",datum->u.d_uint);
				break;
			case dat_real:
				fprintf(logfile,"\treal: %.2lf\n",datum->u.d_real);
				break;
			case dat_miniStr:
				label = "MINI STR";
				goto string;
			case dat_soloStr:
			case dat_soloStrRef:
				label = (datum->d_type == dat_soloStr) ? "HEAP STR" : "REF STR";
				if(datum->u.d_solo == NULL) {
					membName = "u.d_solo";
					goto bad;
					}
string:
				if(datum->d_str == NULL) {
					membName = "d_str";
bad:
					fprintf(logfile,
					 "*** %s member of datum object passed to ddump() is NULL ***\n",membName);
					}
				else {
					short c;
					char *str = datum->d_str;
					fprintf(logfile,"\t%s (%hu): \"",label,datum->d_type);
					while((c = *str++) != '\0')
						fputs(vizc(c,VBaseDef),logfile);
					fputs("\"\n",logfile);
					}
				break;
			case dat_blob:
			case dat_blobRef:
				fprintf(logfile,"\t%s (size %lu): %.8X\n",datum->d_type == dat_blobRef ? "BLOBREF" : "BLOB",
				 datum->u.d_blob.b_size,(uint) datum->u.d_blob.b_mem);
				break;
			}
	}
#endif

// Initialize a datum object to nil.  It is assumed any heap storage has already been freed.
#if DCaller
void dinit(Datum *datum,char *caller) {
#else
void dinit(Datum *datum) {
#endif
	datum->d_type = dat_nil;
	datum->d_str = NULL;
#if DCaller
	fprintf(logfile,"dinit(): initialized object %.8X for %s()\n",(uint) datum,caller);
#endif
	}

// Clear a datum object and set it to nil.
void dclear(Datum *datum) {

	// Free any string or blob storage.
	switch(datum->d_type) {
		case dat_soloStr:
#if DDebug
			fprintf(logfile,"dclear(): free [%.8X] SOLO ",(uint) datum->d_str);
			ddump(datum,"from...");
			free((void *) datum->d_str);
			fputs("dclear(): DONE.\n",logfile);
			fflush(logfile);
#else
			free((void *) datum->d_str);
#endif
			break;
		case dat_blob:
#if DDebug
			fprintf(logfile,"dclear(): free [%.8X] BLOB, size %lu",(uint) datum->u.d_blob.b_mem,
			 datum->u.d_blob.b_size);
			ddump(datum,"from...");
			if(datum->u.d_blob.b_mem != NULL)
				free((void *) datum->u.d_blob.b_mem);
			fputs("dclear(): DONE.\n",logfile);
			fflush(logfile);
#else
			if(datum->u.d_blob.b_mem != NULL)
				free((void *) datum->u.d_blob.b_mem);
#endif
		}
#if DCaller
	dinit(datum,"dclear");
#else
	dinit(datum);
#endif
	}

// Set a datum object to a null "mini" string.
void dsetnull(Datum *datum) {

	dclear(datum);
	*(datum->d_str = datum->u.d_miniBuf) = '\0';
	datum->d_type = dat_miniStr;
	}

// Set a Boolean value in a datum object.
void dsetbool(bool b,Datum *datum) {

	dclear(datum);
	datum->d_type = b ? dat_true : dat_false;
	datum->d_str = NULL;
	}

// Set a blob value in a datum object (copy to heap), given void pointer to object and size in bytes, which may be zero.  Return
// status code.
int dsetblob(void *mem,size_t size,Datum *datum) {
	DBlob *blob = &datum->u.d_blob;

	dclear(datum);
	datum->d_type = dat_blob;
	datum->d_str = NULL;
	if((blob->b_size = size) == 0)
		blob->b_mem = NULL;
	else {
		if((blob->b_mem = malloc(size)) == NULL) {
			plexcep.flags |= ExcepMem;
			return emsge(-1);
			}
		memcpy(blob->b_mem,mem,size);
		}
	return 0;
	}

// Set a blob reference in a datum object, given void pointer to object and size in bytes.
void dsetblobref(void *mem,size_t size,Datum *datum) {
	DBlob *blob = &datum->u.d_blob;

	dclear(datum);
	datum->d_type = dat_blobRef;
	datum->d_str = NULL;
	blob->b_mem = mem;
	blob->b_size = size;
	}

// Set a single-character (string) value in a datum object.
void dsetchr(short c,Datum *datum) {

	dclear(datum);
	(datum->d_str = datum->u.d_miniBuf)[0] = c;
	datum->u.d_miniBuf[1] = '\0';
	datum->d_type = dat_miniStr;
	}

// Set a signed integer value in a datum object.
void dsetint(long i,Datum *datum) {

	dclear(datum);
	datum->u.d_int = i;
	datum->d_type = dat_int;
	datum->d_str = NULL;
	}

// Set an unsigned integer value in a datum object.
void dsetuint(ulong u,Datum *datum) {

	dclear(datum);
	datum->u.d_uint = u;
	datum->d_type = dat_uint;
	datum->d_str = NULL;
	}

// Set a real number value in a datum object.
void dsetreal(double d,Datum *datum) {

	dclear(datum);
	datum->u.d_real = d;
	datum->d_type = dat_real;
	datum->d_str = NULL;
	}

// Get space for a string and set *strp to it.  If a mini string will work, use caller's mini buffer "minibuf" (if not NULL).
// Return status code.
static int salloc(char **strp,size_t len,char *minibuf) {

	if(minibuf != NULL && len <= sizeof(DBlob))
		*strp = minibuf;
	else if((*strp = (char *) malloc(len)) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	return 0;
	}

// Allocate a string value of given size in a datum object.  Return status code.
int dsalloc(Datum *datum,size_t len) {

	dsetnull(datum);
	if(len > sizeof(DBlob)) {
#if DDebug
		fprintf(logfile,"dsalloc(): getting heap string for %.8X for len %lu...\n",(uint) datum,len);
#endif
		if(salloc(&datum->u.d_solo,len,NULL) != 0)
			return -1;
		*(datum->d_str = datum->u.d_solo) = '\0';
		datum->d_type = dat_soloStr;
		}
#if DDebug
	else
		fprintf(logfile,"dsalloc(): retaining MINI string (in datum %.8X) for len %lu\n",(uint) datum,len);
#endif
	return 0;
	}

// Set a string reference in a datum object.
static void dsetrstr(char *str,Datum *datum,DatumType t) {

	dclear(datum);
	datum->d_str = datum->u.d_solo = str;
	datum->d_type = t;
	}

// Set a string value currently on the heap in a datum object.
void dsetmemstr(char *str,Datum *datum) {

	dsetrstr(str,datum,dat_soloStr);
	}

// Set a string reference in a datum object.
void dsetstrref(char *str,Datum *datum) {

	dsetrstr(str,datum,dat_soloStrRef);
	}

// Set a substring (which is assumed to not contain any embedded null bytes) in a datum object, given pointer and length.
// Routine also assumes that the source string may be all or part of the dest object.  Return status code.
int dsetsubstr(char *str,size_t len,Datum *datum) {
	char *str0;
	char wkbuf[sizeof(DBlob)];

	if(salloc(&str0,len + 1,wkbuf) != 0)	// Get space for string...
		return -1;
	stplcpy(str0,str,len + 1);		// and copy string to new buffer.
	if(str0 == wkbuf) {			// If mini string...
		dsetnull(datum);			// clear datum...
		strcpy(datum->u.d_miniBuf,wkbuf);// and copy it.
		}
	else
		dsetmemstr(str0,datum);		// Otherwise, set heap string in datum.
	return 0;
	}

// Set a string value in a datum object.  Return status code.
int dsetstr(char *str,Datum *datum) {

	return dsetsubstr(str,strlen(str),datum);
	}

// Transfer contents of one datum object to another.  Return dest.
Datum *datxfer(Datum *dest,Datum *src) {
	Datum *datum = dest->d_next;		// Save the "next" pointer...
#if DDebug
	static char xMsg[64];
	sprintf(xMsg,"datxfer(): src [%.8X] BEFORE...",(uint) src);
	ddump(src,xMsg);
	sprintf(xMsg,"datxfer(): dest [%.8X] BEFORE...",(uint) dest);
	ddump(dest,xMsg);
#endif
	dclear(dest);				// free dest...
	*dest = *src;				// copy the whole burrito...
	dest->d_next = datum;			// restore "next" pointer...
	if(dest->d_type == dat_miniStr)	// fix mini-string pointer...
		dest->d_str = dest->u.d_miniBuf;
#if DCaller
	dinit(src,"datxfer");
#else
	dinit(src);				// initialize the source...
#endif
#if DDebug
	ddump(src,"datxfer(): src AFTER...");
	ddump(dest,"datxfer(): dest AFTER...");
#endif
	return dest;				// and return result.
	}

// Return true if a datum object is a false Boolean value; otherwise, false.
bool disfalse(Datum *datum) {

	return datum->d_type == dat_false;
	}

// Return true if a datum object is nil; otherwise, false.
bool disnil(Datum *datum) {

	return datum->d_type == dat_nil;
	}

// Return true if a datum object is a null string; otherwise, false.
bool disnull(Datum *datum) {

	return (datum->d_type & DStrMask) && *datum->d_str == '\0';
	}

// Return true if a datum object is a true Boolean value; otherwise, false.
bool distrue(Datum *datum) {

	return datum->d_type == dat_true;
	}

// Create datum object and set *datump to it.  If "track" is true, add it to the garbage collection stack.  Return status code.
#if DCaller
static int dmake(Datum **datump,bool track,char *caller,char *dName) {
#else
static int dmake(Datum **datump,bool track) {
#endif
	Datum *datum;

#if DDebug && 0
	fputs("dmake(): Dump object list...\n",logfile);
	for(datum = datGarb; datum != NULL; datum = datum->d_next)
		ddump(datum,"dmake() DUMP");
#endif
	// Get new object...
	if((datum = (Datum *) malloc(sizeof(Datum))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
#if DDebug
#if DCaller
fprintf(logfile,"dmake(): Allocated %.8X, type %s for %s(), var '%s'\n",(uint) datum,track ? "TRACK" : "Perm",caller,dName);
#else
fprintf(logfile,"dmake(): Allocated %.8X, type %s\n",(uint) datum,track ? "TRACK" : "Perm");
#endif
#endif
	// add it to garbage collection stack (if applicable)...
	if(track) {
		datum->d_next = datGarb;
		datGarb = datum;
		}
	else
		datum->d_next = NULL;

	// initialize it...
#if DCaller
	dinit(datum,"dmake");
#else
	dinit(datum);
#endif
	// and return the spoils.
	*datump = datum;
	return 0;
	}

// Create datum object and set *datump to it.  Object is not added it to the garbage collection stack.  Return status code.
#if DCaller
int dnew(Datum **datump,char *caller,char *dName) {

	return dmake(datump,false,caller,dName);
#else
int dnew(Datum **datump) {

	return dmake(datump,false);
#endif
	}

// Create (tracked) datum object, set *datump to it, and add it to the garbage collection stack.  Return status code.
#if DCaller
int dnewtrk(Datum **datump,char *caller,char *dName) {

	return dmake(datump,true,caller,dName);
#else
int dnewtrk(Datum **datump) {

	return dmake(datump,true);
#endif
	}

// Copy a byte string into a string-fab object's work buffer, left or right justified.  Source string may already be in the
// buffer.
static void sfcpy(DStrFab *sfab,char *str,size_t len) {

	if(len > 0) {
		if((sfab->sf_flags & SFModeMask) != SFPrepend) {

			// Appending to existing byte string: copy it into the work buffer.
			sfab->sf_buf = (char *) memcpy((void *) sfab->sf_wkbuf,(void *) str,len) + len;
			}
		else {
			// Prepending to existing byte string or right-shifting the data in the work buffer.
			char *dest = sfab->sf_bufz;
			str += len;
			do {
				*--dest = *--str;
				} while(--len > 0);
			sfab->sf_buf = dest;
			}
		}
	}

// Save a byte string to a string-fab object's chunk list.  Return status code.
static int sfsave(char *str,size_t len,DStrFab *sfab) {
	DChunk *chunk;

	// Get new chunk...
	if((chunk = (DChunk *) malloc(sizeof(DChunk))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	chunk->ck_blob.b_mem = (void *) str;
	chunk->ck_blob.b_size = len;
#if DDebug
	fputs("sfsave(",logfile);
	fvizs((void *) str,len > 32 ? 32 : len,logfile);
	fprintf(logfile," [%.8X],%lu,%.8X): done.\n",(uint) str,len,(uint) sfab);
#endif
	// and push it onto the stack in the string-fab object.
	chunk->ck_next = sfab->sf_stack;
	sfab->sf_stack = chunk;

	return 0;
	}

// Grow work buffer in a string-fab object.  Assume minSize is less than DChunkSzMax.  If not initial allocation and prepending,
// the data in the work buffer is shifted right after the work buffer is extended.  Return status code.
static int sfgrow(DStrFab *sfab,size_t minSize) {
	char *bp;
	size_t size,used;

	// Initial allocation?
	if(sfab->sf_wkbuf == NULL) {
		size = (minSize < DChunkSz0) ? DChunkSz0 : (minSize < DChunkSz4) ? DChunkSz4 : DChunkSzMax;
		goto Init;
		}

	// Nope... called from dputc().  Work buffer is full.
	else {
		// Double or quadruple old size?
		size = used = sfab->sf_bufz - (bp = sfab->sf_wkbuf);
		if(size < DChunkSz4)
			size *= 2;
		else if(size < DChunkSzMax)
			size *= 4;

		// No, already at max.  Save current buffer and allocate new one.
		else {
			if(sfsave(bp,size,sfab) != 0)
				return -1;
Init:
			bp = NULL;
			used = 0;
			}
		}

	// Get more space and reset pointers.
#if DDebug
	fprintf(logfile,"sfgrow(%.8X,%lu): Calling realloc() at %.8X, size %lu, used %lu...\n",(uint) sfab,minSize,
	 (uint) bp,size,used);
#endif
	if((sfab->sf_wkbuf = (char *) realloc((void *) bp,size)) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	sfab->sf_bufz = sfab->sf_wkbuf + size;
	if((sfab->sf_flags & SFModeMask) != SFPrepend)
		sfab->sf_buf = sfab->sf_wkbuf + used;
	else {
		sfab->sf_buf = sfab->sf_bufz - used;
		if(bp != NULL)

			// Extended old buffer in "prepend" mode... right-shift contents.
			sfcpy(sfab,sfab->sf_wkbuf,used);
		}
#if DDebug
	fprintf(logfile,"sfgrow(): done.  sf_wkbuf %.8X, sf_buf %.8X, sf_bufz %.8X\n",
	 (uint) sfab->sf_wkbuf,(uint) sfab->sf_buf,(uint) sfab->sf_bufz);
#endif
	return 0;
	}

// Put a character to a string-fab object.  Return status code.
int dputc(short c,DStrFab *sfab) {

	if((sfab->sf_flags & SFModeMask) == SFPrepend) {

		// Save current chunk if no space left.
		if(sfab->sf_buf == sfab->sf_wkbuf && sfgrow(sfab,0) != 0)
			return -1;

		// Save the character.
		*--sfab->sf_buf = c;
		}
	else {
		// Save current chunk if no space left.
		if(sfab->sf_buf == sfab->sf_bufz && sfgrow(sfab,0) != 0)
			return -1;

		// Save the character.
		*sfab->sf_buf++ = c;
		}
#if DDebug
	fprintf(logfile,"dputc(): Saved char %s at %.8X, sf_wkbuf %.8X, buflen %lu.\n",
	 vizc(c,VBaseDef),(uint) sfab->sf_buf - ((sfab->sf_flags & SFModeMask) == SFPrepend) ? 0 : 1,(uint) sfab->sf_wkbuf,
	 sfab->sf_bufz - sfab->sf_wkbuf);
#endif
	return 0;
	}

// "Unput" a character from a string-fab object.  Guaranteed to always work once, if at least one byte was previously put.
// Return status code, including error if work buffer is empty.
int dunputc(DStrFab *sfab) {

	// Any bytes in work buffer?
	if((sfab->sf_flags & SFModeMask) == SFPrepend) {
		if(sfab->sf_buf < sfab->sf_bufz) {
			++sfab->sf_buf;
			return 0;
			}
		}
	else if(sfab->sf_buf > sfab->sf_wkbuf) {
		--sfab->sf_buf;
		return 0;
		}

	// Empty buffer.  Return error.
	return emsg(-1,"No bytes left to \"unput\"");
	}

// Put a string to a string-fab object.  Return status code.
int dputs(char *str,DStrFab *sfab) {

	if((sfab->sf_flags & SFModeMask) == SFPrepend) {
		char *str1 = strchr(str,'\0');
		while(str1 > str)
			if(dputc(*--str1,sfab) != 0)
				return -1;
		}
	else {
		while(*str != '\0')
			if(dputc(*str++,sfab) != 0)
				return -1;
		}
	return 0;
	}

// Put a byte string to a string-fab object.  Return status code.
int dputmem(const void *mem,size_t len,DStrFab *sfab) {

	if((sfab->sf_flags & SFModeMask) == SFPrepend) {
		char *str = (char *) mem + len;
		while(len-- > 0)
			if(dputc(*--str,sfab) != 0)
				return -1;
		}
	else {
		char *str = (char *) mem;
		while(len-- > 0)
			if(dputc(*str++,sfab) != 0)
				return -1;
		}
	return 0;
	}

// Put the contents of a datum object to a string-fab object.  Return status code.
int dputd(Datum *datum,DStrFab *sfab) {
	char *str;
	char wkbuf[80];

	switch(datum->d_type) {
		case dat_nil:
			return 0;
		case dat_miniStr:
		case dat_soloStr:
		case dat_soloStrRef:
			str = datum->d_str;
			goto put;
		case dat_int:
			sprintf(str = wkbuf,"%ld",datum->u.d_int);
			goto put;
		case dat_uint:
			sprintf(str = wkbuf,"%lu",datum->u.d_uint);
			goto put;
		case dat_real:
			sprintf(str = wkbuf,"%lf",datum->u.d_real);
put:
			return dputs(str,sfab);
		case dat_blob:
		case dat_blobRef:
			return dputmem(datum->u.d_blob.b_mem,datum->u.d_blob.b_size,sfab);
		}

	// Invalid datum type.
	return emsgf(-1,"Unknown datum type (%hu)",datum->d_type);
	}

// Put formatted text to a string-fab object.  Return status code.
int dputf(DStrFab *sfab,char *fmt,...) {
	char *str;
	va_list arg;

	va_start(arg,fmt);
	(void) vasprintf(&str,fmt,arg);
	va_end(arg);
	if(str == NULL)
		return emsge(-1);

	int rcode = dputs(str,sfab);
	free((void *) str);
	return rcode;
	}

// Flags for string-fabrication-object creation (combined with flags defined in header file).
#define SFTrack		0x0004			// Create tracked datum object.

// Prepare for writing to a string-fab object, given pointer to list object to initialize, (possibly NULL) pointer to
// destination datum object, and flags.  If datum is NULL, create a new datum object (which can be retrieved by the caller from
// the sfab record).  If the SFTrack flag is set, create a tracked object; otherwise, untracked.  If datum is not NULL and the
// SFPrepend or SFAppend flag is set, keep existing string value in datum (and prepend or append to it); otherwise, clear it.
// Return status code.
#if DCaller
static int dprep(DStrFab *sfab,Datum *datum,ushort flags,char *caller,char *dName) {
#else
static int dprep(DStrFab *sfab,Datum *datum,ushort flags) {
#endif

#if DDebug
	fprintf(logfile,"*dprep(...,%.8X,%s,...): BEGIN\n",(uint) datum,(flags & SFAppend) ? "append" :
	 (flags & SFPrepend) ? "prepend" : "clear");
#endif
	sfab->sf_stack = NULL;
	sfab->sf_wkbuf = NULL;

	// Initialize Datum object.
	if(datum == NULL) {
#if DCaller
		if(dmake(&datum,flags & SFTrack,"dprep","datum") != 0)
#else
		if(dmake(&datum,flags & SFTrack) != 0)
#endif
			return -1;
		goto SetNull;
		}
	else if((flags & SFModeMask) == SFClear || !(datum->d_type & DStrMask)) {
		if(datum->d_type != dat_nil)
			flags = (flags & ~SFModeMask) | SFClear;	// Not string or nil: force "clear" mode.
SetNull:
		dsetnull(datum);
		}

	sfab->sf_datum = datum;
	sfab->sf_flags = flags;

	// Initialize string-fab buffers, preserving existing string value in Datum object if appending or prepending.
	if((flags & SFModeMask) != SFClear) {
		size_t used = strlen(datum->d_str);
		if(used < DChunkSzMax) {
			if(sfgrow(sfab,used) != 0)		// Allocate a work buffer.
				return -1;
			if(used > 0) {				// If existing string not null...
				sfcpy(sfab,datum->d_str,used);	// copy it to work buffer, left or right justified...
				dsetnull(datum);			// and set Datum object to null.
				}
#if DDebug
			goto DumpIt;
#else
			return 0;
#endif
			}

		// Appending or prepending to existing string >= DChunkSzMax in length.  Save it in a DChunk object and
		// re-initialize Datum object (without calling free()).
		if(sfsave(datum->d_str,used,sfab) != 0)
			return -1;
		dinit(datum);
		dsetnull(datum);
		}
#if DDebug
	if(sfgrow(sfab,0) != 0)
		return -1;
DumpIt:
#if DCaller
	fprintf(logfile,"dprep(): Initialized SF %.8X for %s(), var '%s': sf_buf %.8X, sf_wkbuf %.8X\n",(uint) sfab,caller,
	 dName,(uint) sfab->sf_buf,(uint) sfab->sf_wkbuf);
#else
	fprintf(logfile,"dprep(): Initialized SF %.8X: sf_buf %.8X, sf_wkbuf %.8X\n",(uint) sfab,(uint) sfab->sf_buf,
	 (uint) sfab->sf_wkbuf);
#endif
	ddump(datum,"dprep(): post-SF-init...");
	fflush(logfile);
	return 0;
#else
	return sfgrow(sfab,0);
#endif
	}

// Open a string-fab object via dprep().  An untracked datum object will be created.  Return status code.
#if DCaller
int dopen(DStrFab *sfab,char *caller,char *dName) {
#else
int dopen(DStrFab *sfab) {
#endif
	return dprep(sfab,NULL,SFClear);
	}

// Open a string-fab object via dprep().  A tracked datum object will be created.  Return status code.
#if DCaller
int dopentrk(DStrFab *sfab,char *caller,char *dName) {
#else
int dopentrk(DStrFab *sfab) {
#endif
	return dprep(sfab,NULL,SFTrack | SFClear);
	}

// Open a string-fab object via dprep() with given datum object (or NULL) and append flag.  Return status code.
#if DCaller
int dopenwith(DStrFab *sfab,Datum *datum,ushort mode,char *caller,char *dName) {
#else
int dopenwith(DStrFab *sfab,Datum *datum,ushort mode) {
#endif
	return (datum == NULL) ? emsg(-1,"Datum pointer cannot be NULL") : dprep(sfab,datum,mode);
	}

// Return true if a string-fab object is empty; otherwise, false.
bool disempty(DStrFab *sfab) {

	return sfab->sf_buf == (((sfab->sf_flags & SFModeMask) == SFPrepend) ? sfab->sf_bufz : sfab->sf_wkbuf) &&
	 sfab->sf_stack == NULL;
	}

// End a string-fab object write operation and convert target datum object to a string or blob, according to "type".  Return
// status code.
int dclose(DStrFab *sfab,DCloseType type) {

	// If still at starting point (no bytes written), change datum object to empty blob if force; otherwise, leave as
	// a null string.
	if(disempty(sfab)) {
		if(type == sf_forceBlob) {
			dsetblobref(NULL,0,sfab->sf_datum);
			sfab->sf_datum->d_type = dat_blob;
			}
		free((void *) sfab->sf_wkbuf);
		}

	// At least one byte was saved.  If still on first chunk, check if it contains any imbedded null bytes and save as
	// correct type or return error as appropriate.
	else {
		char *str;
		size_t len;

		if((sfab->sf_flags & SFModeMask) == SFPrepend) {
			str = sfab->sf_buf;
			len = sfab->sf_bufz - str;
			}
		else {
			str = sfab->sf_wkbuf;
			len = sfab->sf_buf - str;
			}
		if(sfab->sf_stack == NULL) {
			bool isBinary = (memchr(str,'\0',len) != NULL);

			if(isBinary && type == sf_string)
				goto BinErr;
			if(((isBinary || type == sf_forceBlob) ? dsetblob((void *) str,len,sfab->sf_datum) :
			 dsetsubstr(str,len,sfab->sf_datum)) != 0)
				return -1;
			free((void *) sfab->sf_wkbuf);
			}
		else {
			char *buf0;

			// Not first chunk.  Add last one (which can't be empty) to stack and convert to a solo string or blob.
			// Remember pointer to work buffer in buf0 if prepending and work buffer is not full so that pointer can
			// be passed to free() later instead of "str".
			buf0 = (str != sfab->sf_wkbuf) ? sfab->sf_wkbuf : NULL;
			if(sfsave(str,len,sfab) != 0)
				return -1;
			else {
				DChunk *chunk;
				size_t size;
				char *str0;
				bool isBinary;
				Datum *datum = sfab->sf_datum;
				char wkbuf[sizeof(DBlob)];

				// Get total length.
				len = 0;
				chunk = sfab->sf_stack;
				do {
					len += chunk->ck_blob.b_size;
					} while((chunk = chunk->ck_next) != NULL);

				// Get heap space.
				if(salloc(&str,len + 1,wkbuf) != 0)
					return -1;
				str0 = str;

				// Copy string pieces into it and check for any imbedded null bytes.
				isBinary = false;
				chunk = sfab->sf_stack;
				if((sfab->sf_flags & SFModeMask) == SFPrepend) {

					// Prepending: copy chunks from beginning to end.
					do {
						size = chunk->ck_blob.b_size;
						if(!isBinary && memchr(chunk->ck_blob.b_mem,'\0',size) != NULL)
							isBinary = true;
						memcpy((void *) str,(void *) chunk->ck_blob.b_mem,size);
						str += size;
						if(buf0 != NULL) {
							free((void *) buf0);
							buf0 = NULL;
							}
						else
							free((void *) chunk->ck_blob.b_mem);
						} while((chunk = chunk->ck_next) != NULL);
					*str = '\0';
					}
				else {
					// Not prepending: copy chunks from end to beginning.
					str += len + 1;
					*--str = '\0';
					do {
						size = chunk->ck_blob.b_size;
						if(!isBinary && memchr(chunk->ck_blob.b_mem,'\0',size) != NULL)
							isBinary = true;
						str -= size;
						memcpy((void *) str,(void *) chunk->ck_blob.b_mem,size);
						free((void *) chunk->ck_blob.b_mem);
						} while((chunk = chunk->ck_next) != NULL);
					}

				// Set byte string in datum object if able.
				if(isBinary && type == sf_string)
BinErr:
					return emsg(-1,"Cannot convert binary data to string");
				if(isBinary || type == sf_forceBlob) {
					if(str0 == wkbuf)
						return dsetblob((void *) str0,len,datum);
					dsetblobref((void *) str0,len,datum);
					datum->d_type = dat_blob;
					}
				else if(str0 == wkbuf) {
					dsetnull(datum);
					strcpy(datum->u.d_miniBuf,wkbuf);
					}
				else
					dsetmemstr(str0,datum);
				}
			}
		}
#if DDebug
	ddump(sfab->sf_datum,"dclose(): Finalized string...");
#if 0
	fprintf(logfile,"\tSTRING LIST (%hu):\n",datum->d_type);
	for(chunk = datum->u.d_sHeadp; chunk != NULL; chunk = chunk->ck_next) {
		fputs("\t\t\"",logfile);
		fvizs(chunk->ck_blob.b_mem,chunk->ck_len,logfile);
		fputs("\"\n",logfile);
		}
#endif
#endif
	return 0;
	}

// Stop tracking a Datum object by removing it from the garbage collection stack (if present).
void duntrk(Datum *datum) {

	if(datum == datGarb)
		datGarb = datGarb->d_next;
	else {
		Datum *datum1;
		for(datum1 = datGarb; datum1 != NULL; datum1 = datum1->d_next)
			if(datum1->d_next == datum) {
				datum1->d_next = datum->d_next;
				break;
				}
		}
	}

// Copy one value to another.  Return status code.
int datcpy(Datum *dest,Datum *src) {

	switch(src->d_type) {
		case dat_nil:
		case dat_false:
		case dat_true:
			dclear(dest);
			dest->d_type = src->d_type;
			dest->d_str = NULL;
			break;
		case dat_int:
			dsetint(src->u.d_int,dest);
			break;
		case dat_uint:
			dsetuint(src->u.d_uint,dest);
			break;
		case dat_real:
			dsetreal(src->u.d_real,dest);
			break;
		case dat_miniStr:
		case dat_soloStr:
		case dat_soloStrRef:
			return dsetstr(src->d_str,dest);
		case dat_blob:
			return dsetblob(src->u.d_blob.b_mem,src->u.d_blob.b_size,dest);
		default:	// dat_blobRef
			dsetblobref(src->u.d_blob.b_mem,src->u.d_blob.b_size,dest);
		}
	return 0;
	}

// Compare one Datum to another and return true if values are equal; otherwise, false.  If "ignore" is true, ignore case in
// string comparisons.
bool dateq(Datum *datum1,Datum *datum2,bool ignore) {

	switch(datum1->d_type) {
		case dat_nil:
		case dat_false:
		case dat_true:
			return datum2->d_type == datum1->d_type;
		case dat_int:
			return (datum2->d_type == dat_int && datum2->u.d_int == datum1->u.d_int) ||
			 (datum2->d_type == dat_uint && datum1->u.d_int >= 0 && datum2->u.d_uint == (ulong) datum1->u.d_int) ||
			 (datum2->d_type == dat_real && datum2->u.d_real == (double) datum1->u.d_int);
		case dat_uint:
			return (datum2->d_type == dat_uint && datum2->u.d_uint == datum1->u.d_uint) ||
			 (datum2->d_type == dat_int && datum2->u.d_int >= 0 && (ulong) datum2->u.d_int == datum1->u.d_uint) ||
			 (datum2->d_type == dat_real && datum2->u.d_real == (double) datum1->u.d_uint);
		case dat_real:
			return (datum2->d_type == dat_real && datum2->u.d_real == datum1->u.d_real) ||
			 (datum2->d_type == dat_int && (double) datum2->u.d_int == datum1->u.d_real) ||
			 (datum2->d_type == dat_uint && (double) datum2->u.d_uint == datum1->u.d_real);
		case dat_miniStr:
		case dat_soloStr:
		case dat_soloStrRef:
			if(datum2->d_type & DStrMask) {
				int (*cmp)(const char *s1,const char *s2) = ignore ? strcasecmp : strcmp;
				return cmp(datum1->d_str,datum2->d_str) == 0;
				}
			return false;
		}

	// dat_blob, dat_blobRef
	return (datum2->d_type & DBlobMask) && datum1->u.d_blob.b_size == datum2->u.d_blob.b_size &&
	 memcmp(datum1->u.d_blob.b_mem,datum2->u.d_blob.b_mem,datum1->u.d_blob.b_size) == 0;
	}

// Delete given datum object.  It assumed that either the object was permanent and not in the garbage collection list, or the
// caller is removing it from the list; for example, when called from dgarbpop().
#if DDebug
#if DCaller
void ddelete(Datum *datum,char *caller) {
#else
void ddelete(Datum *datum) {
#endif
	ddump(datum,"ddelete(): About to free()...");
	fprintf(logfile,"ddelete(): calling dclear(%.8X)",(uint) datum);
#if DCaller
	fprintf(logfile," on object from %s()",caller);
#endif
	fputs("...\n",logfile);
	fflush(logfile);
	dclear(datum);
	fprintf(logfile,"ddelete(): calling free(%.8X)... ",(uint) datum);
	fflush(logfile);
	free((void *) datum);
	fputs("done.\n",logfile); fflush(logfile);
#else
void ddelete(Datum *datum) {

	dclear(datum);
	free((void *) datum);
#endif
	}

// Pop datGarb to given pointer, releasing heap space and laughing all the way.
void dgarbpop(Datum *datum) {
	Datum *datum1;

#if DDebug
	ddump(datum,"dgarbpop(): Popping to...");
	while(datGarb != datum) {
		if((datum1 = datGarb) == NULL) {
			fputs("datGarb is NULL! Bailing out...\n",logfile);
			break;
			}
		datGarb = datGarb->d_next;
#if DCaller
		ddelete(datum1,"dgarbpop");
#else
		ddelete(datum1);
#endif
		}
	fputs("dgarbpop(): END\n",logfile);
	fflush(logfile);
#else
	while(datGarb != datum) {
		datum1 = datGarb;
		datGarb = datGarb->d_next;
		ddelete(datum1);
		}
#endif
	}

// Copy a character to sfab (an active string-fab object) in visible form.  Pass "flags" to vizc().  Return status code.
int dvizc(short c,ushort flags,DStrFab *sfab) {
	char *str;

	return (str = vizc(c,flags)) == NULL || dputs(str,sfab) != 0 ? -1 : 0;
	}

// Copy bytes from str to sfab (an active string-fab object), exposing all invisible characters.  Pass "flags" to vizc().  If len
// is zero, assume str is a null-terminated string; otherwise, copy exactly len bytes.  Return status code.
int dvizs(const void *str,size_t len,ushort flags,DStrFab *sfab) {
	char *str1 = (char *) str;
	size_t n = (len == 0) ? strlen(str1) : len;

	for(; n > 0; --n)
		if(dvizc(*str1++,flags,sfab) != 0)
			return -1;
	return 0;
	}

// Copy bytes from str to datum via dvizs().  Return status code.
int dviz(const void *str,size_t len,ushort flags,Datum *datum) {
	DStrFab dest;

	return (dopenwith(&dest,datum,false) != 0 || dvizs(str,len,flags,&dest) != 0 || dclose(&dest,sf_string) != 0) ? -1 : 0;
	}

// Copy string from str to datum, adding a single quote (') at beginning and end and converting single quote characters to '\''. 
// Return status.
int dshquote(char *str,Datum *datum) {
	ptrdiff_t len;
	char *str0,*str1;
	bool apostrophe;
	DStrFab dest;

	str0 = str;
	if(dopenwith(&dest,datum,false) != 0)
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
char *dtos(Datum *datum,bool viznil) {
	static char wkbuf[80];

	switch(datum->d_type) {
		case dat_nil:
			return viznil ? "nil" : "";
		case dat_false:
			return "false";
		case dat_true:
			return "true";
		case dat_int:
			sprintf(wkbuf,"%ld",datum->u.d_int);
			break;
		case dat_uint:
			sprintf(wkbuf,"%lu",datum->u.d_uint);
			break;
		case dat_real:
			sprintf(wkbuf,"%lf",datum->u.d_real);
			break;
		case dat_miniStr:
		case dat_soloStr:
		case dat_soloStrRef:
			return datum->d_str;
		case dat_blob:
		case dat_blobRef:
			(void) emsg(-1,"Cannot convert blob to string");
			return NULL;
		}
	return wkbuf;
	}
