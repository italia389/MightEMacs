// ProLib (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// pldatum.h		Header file for Datum object routines.

#ifndef pldatum_h
#define pldatum_h

#include "pldef.h"

// Definitions for creating and managing strings, integers, and other data types allocated from heap space.

// Definitions for dopenwith() function.
#define SFClear		0			// Clear data in caller's Datum object.
#define SFAppend	1			// Append to caller's Datum object.
#define SFPrepend	2			// Prepend to caller's Datum object.
#define SFModeMask	0x0003			// Bits for mode value.

// Definitions for debugging.
#define DDebug		0			// Include debugging code for datum objects.
#define DCaller		0			// Use "caller-tracking" code and sanity checks for datum objects.
#define DSFTest		0			// Include string-fab test code.

// Blob object: used for holding generic chunks of memory, such as a struct or byte string.
typedef struct {
	size_t b_size;				// Size in bytes.
	void *b_memp;
	} DBlob;

// Chunk object: used for holding chunks of memory for string-fab (DStrFab) objects.
typedef struct DChunk {
	struct DChunk *ck_nextp;		// Link to next item in list.
	DBlob ck_blob;				// Blob object.
	} DChunk;

// Datum object: general purpose structure for holding a nil value, Boolean value, signed or unsigned long integer, real number,
// string of any length, or blob.
typedef ushort DatumType;
typedef struct Datum {
	struct Datum *d_nextp;			// Link to next item in list.
	DatumType d_type;			// Type of value.
	char *d_str;				// String value if dat_miniStr, dat_soloStr, or dat_soloStrRef; otherwise, NULL.
	union {					// Current value.
		long d_int;			// Signed integer.
		ulong d_uint;			// Unsigned integer.
		double d_real;			// Real number.
		char d_miniBuf[sizeof(DBlob)];	// Self-contained mini-string.
		char *d_solo;			// Solo string (on heap).
		DBlob d_blob;			// Blob object.
		} u;
	} Datum;

// Datum types.
#define dat_nil		0x0000			// Nil value.
#define dat_false	0x0001			// False value.
#define dat_true	0x0002			// True value.
#define dat_int		0x0004			// Signed integer type.
#define dat_uint	0x0008			// Unsigned integer type.
#define dat_real	0x0010			// Real number (double) type.
#define dat_miniStr	0x0020			// Mini string.
#define dat_soloStr	0x0040			// Solo string by value.
#define dat_soloStrRef	0x0080			// Solo string by reference.
#define dat_blob	0x0100			// Blob object by value.
#define dat_blobRef	0x0200			// Blob object by reference.

#define DBoolMask	(dat_false | dat_true)	// Boolean types.
#define DStrMask	(dat_miniStr | dat_soloStr | dat_soloStrRef)	// String types.
#define DBlobMask	(dat_blob | dat_blobRef)	// Blob types.

// String fabrication object: used to build a string in pieces, forward or backward.
typedef struct {
	Datum *sf_datp;				// Datum pointer.
	DChunk *sf_stack;			// Chunk stack (linked list).
	char *sf_bufp;				// Next byte to store in sf_buf.
	char *sf_bufpz;				// Ending byte position in sf_buf.
	char *sf_buf;				// Work buffer.
	ushort sf_flags;			// Operation mode.
	} DStrFab;

// String-fab close types used by dclose().
typedef enum {
	sf_string = -1,				// String-fab object may not contain null bytes.
	sf_auto,				// Both string and blob types allowed.
	sf_forceBlob				// Force blob type.
	} DCloseType;

// External variables.
extern Datum *datGarbp;				// Head of list of temporary Datum records ("garbage collection").

// External function declarations and aliases.
extern int datcpy(Datum *destp,Datum *srcp);
extern bool dateq(Datum *datp1,Datum *datp2,bool ignore);
extern Datum *datxfer(Datum *destp,Datum *srcp);
extern void dclear(Datum *datp);
extern int dclose(DStrFab *sfp,DCloseType type);
#if DCaller
extern void ddelete(Datum *datp,char *caller);
#else
extern void ddelete(Datum *datp);
#endif
extern void duntrk(Datum *datp);
#if DDebug
extern void ddump(Datum *datp,char *label);
#endif
extern void dgarbpop(Datum *datp);
#if DCaller
extern void dinit(Datum *datp,char *caller);
#else
extern void dinit(Datum *datp);
#endif
extern bool disempty(DStrFab *sfp);
extern bool disfalse(Datum *datp);
extern bool disnil(Datum *datp);
extern bool disnull(Datum *datp);
extern bool distrue(Datum *datp);
#if DCaller
extern int dnew(Datum **datpp,char *caller,char *dName);
extern int dnewtrk(Datum **datpp,char *caller,char *dName);
extern int dopen(DStrFab *sfp,char *caller,char *dName);
extern int dopentrk(DStrFab *sfp,char *caller,char *dName);
extern int dopenwith(DStrFab *sfp,Datum *datp,ushort mode,char *caller,char *dName);
#else
extern int dnew(Datum **datpp);
extern int dnewtrk(Datum **datpp);
extern int dopen(DStrFab *sfp);
extern int dopentrk(DStrFab *sfp);
extern int dopenwith(DStrFab *sfp,Datum *datp,ushort mode);
#endif
extern int dputc(short c,DStrFab *sfp);
extern int dputd(Datum *datp,DStrFab *sfp);
extern int dputf(DStrFab *sfp,char *fmt,...);
extern int dputmem(const void *mem,size_t len,DStrFab *sfp);
extern int dputs(char *str,DStrFab *sfp);
extern int dsalloc(Datum *datp,size_t len);
extern int dsetblob(void *memp,size_t size,Datum *datp);
extern void dsetblobref(void *memp,size_t size,Datum *datp);
extern void dsetbool(bool b,Datum *datp);
extern void dsetchr(short c,Datum *datp);
extern void dsetint(long i,Datum *datp);
extern void dsetmemstr(char *str,Datum *datp);
#define dsetnil(datp)		dclear(datp)
extern void dsetnull(Datum *datp);
extern void dsetreal(double d,Datum *datp);
extern int dsetstr(char *str,Datum *datp);
extern void dsetstrref(char *str,Datum *datp);
extern int dsetsubstr(char *str,size_t len,Datum *datp);
extern void dsetuint(ulong u,Datum *datp);
extern int dshquote(char *str,Datum *datp);
extern char *dtos(Datum *datp,bool viznil);
extern int dunputc(DStrFab *sfp);
extern int dviz(const void *str,size_t len,ushort flags,Datum *datp);
extern int dvizc(short c,ushort flags,DStrFab *sfp);
extern int dvizs(const void *str,size_t len,ushort flags,DStrFab *sfp);
#endif
