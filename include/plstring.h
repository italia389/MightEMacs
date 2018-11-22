// ProLib (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// plstring.h		Header file for ProLib string routines.

#ifndef plstring_h
#define plstring_h

#include "pldatum.h"

// Definitions for vizc() routine.
#define VBaseDef	0x0000		// Use default base (VBaseHEX).
#define VBaseOctal	0x0001		// Octal base.
#define VBaseHex	0x0002		// Hex base (lower case letters).
#define VBaseHEX	0x0003		// Hex base (upper case letters).
#define VBaseMask	0x0003
#define VBaseMax	VBaseHEX

#define VSpace		0x0004		// Make space character visible.

// Definitions for split() routine.
typedef struct {
	uint itemCount;
	char *ary[1];			// Array length is set when malloc() is called.
	} StrArray;

// External function declarations.
extern int join(Datum *destp,StrArray *srcp,char *delim);
extern int memcasecmp(const void *str1,const void *str2,size_t len);
extern void *memzcpy(void *dest,const void *src,size_t len);
extern StrArray *split(char *src,short delim,int limit);
extern char *stplcpy(char *dest,char *src,size_t size);
extern int strconv(Datum *destp,char *src,char **endp,short termch,DCloseType type);
extern char *strfit(char *dest,size_t maxlen,char *src,size_t srclen);
extern char *strpspn(const char *s1,const char *s2);
extern char *vizc(short c,ushort flags);
#endif
