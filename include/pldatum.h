// ProLib (c) Copyright 2019 Richard W. Marinelli
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
	void *b_mem;
	} DBlob;

// Chunk object: used for holding chunks of memory for string-fab (DStrFab) objects.
typedef struct DChunk {
	struct DChunk *ck_next;		// Link to next item in list.
	DBlob ck_blob;				// Blob object.
	} DChunk;

// Datum object: general purpose structure for holding a nil value, Boolean value, signed or unsigned long integer, real number,
// string of any length, or blob.
typedef ushort DatumType;
typedef struct Datum {
	struct Datum *d_next;			// Link to next item in list.
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
	Datum *sf_datum;				// Datum pointer.
	DChunk *sf_stack;			// Chunk stack (linked list).
	char *sf_buf;				// Next byte to store in sf_wkbuf.
	char *sf_bufz;				// Ending byte position in sf_wkbuf.
	char *sf_wkbuf;				// Work buffer.
	ushort sf_flags;			// Operation mode.
	} DStrFab;

// String-fab close types used by dclose().
typedef enum {
	sf_string = -1,				// String-fab object may not contain null bytes.
	sf_auto,				// Both string and blob types allowed.
	sf_forceBlob				// Force blob type.
	} DCloseType;

// External variables.
extern Datum *datGarb;				// Head of list of temporary Datum records ("garbage collection").

// External function declarations and aliases.
extern int datcpy(Datum *dest,Datum *src);
extern bool dateq(Datum *datum1,Datum *datum2,bool ignore);
extern Datum *datxfer(Datum *dest,Datum *src);
extern void dclear(Datum *datum);
extern int dclose(DStrFab *sfab,DCloseType type);
#if DCaller
extern void ddelete(Datum *datum,char *caller);
#else
extern void ddelete(Datum *datum);
#endif
extern void duntrk(Datum *datum);
#if DDebug
extern void ddump(Datum *datum,char *label);
#endif
extern void dgarbpop(Datum *datum);
#if DCaller
extern void dinit(Datum *datum,char *caller);
#else
extern void dinit(Datum *datum);
#endif
extern bool disempty(DStrFab *sfab);
extern bool disfalse(Datum *datum);
extern bool disnil(Datum *datum);
extern bool disnull(Datum *datum);
extern bool distrue(Datum *datum);
#if DCaller
extern int dnew(Datum **datump,char *caller,char *dName);
extern int dnewtrk(Datum **datump,char *caller,char *dName);
extern int dopen(DStrFab *sfab,char *caller,char *dName);
extern int dopentrk(DStrFab *sfab,char *caller,char *dName);
extern int dopenwith(DStrFab *sfab,Datum *datum,ushort mode,char *caller,char *dName);
#else
extern int dnew(Datum **datump);
extern int dnewtrk(Datum **datump);
extern int dopen(DStrFab *sfab);
extern int dopentrk(DStrFab *sfab);
extern int dopenwith(DStrFab *sfab,Datum *datum,ushort mode);
#endif
extern int dputc(short c,DStrFab *sfab);
extern int dputd(Datum *datum,DStrFab *sfab);
extern int dputf(DStrFab *sfab,char *fmt,...);
extern int dputmem(const void *mem,size_t len,DStrFab *sfab);
extern int dputs(char *str,DStrFab *sfab);
extern int dsalloc(Datum *datum,size_t len);
extern int dsetblob(void *mem,size_t size,Datum *datum);
extern void dsetblobref(void *mem,size_t size,Datum *datum);
extern void dsetbool(bool b,Datum *datum);
extern void dsetchr(short c,Datum *datum);
extern void dsetint(long i,Datum *datum);
extern void dsetmemstr(char *str,Datum *datum);
#define dsetnil(datum)		dclear(datum)
extern void dsetnull(Datum *datum);
extern void dsetreal(double d,Datum *datum);
extern int dsetstr(char *str,Datum *datum);
extern void dsetstrref(char *str,Datum *datum);
extern int dsetsubstr(char *str,size_t len,Datum *datum);
extern void dsetuint(ulong u,Datum *datum);
extern int dshquote(char *str,Datum *datum);
extern char *dtos(Datum *datum,bool viznil);
extern int dunputc(DStrFab *sfab);
extern int dviz(const void *str,size_t len,ushort flags,Datum *datum);
extern int dvizc(short c,ushort flags,DStrFab *sfab);
extern int dvizs(const void *str,size_t len,ushort flags,DStrFab *sfab);
#endif
