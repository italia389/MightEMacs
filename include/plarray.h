// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// plarray.h	Definitions and declarations for Array library routines.

#ifndef plarray_h
#define plarray_h

#include "pldatum.h"

#define AChunkSz	8		// Starting size of array element chunks.
#define ArraySizeMax	INT_MAX		// Maximum number of elements in an array.

// Array object (stored in a Datum object as a blob).
typedef int ArraySize;
typedef struct {
	ArraySize a_size;		// Number of elements allocated.
	ArraySize a_used;		// Number of elements currently in use.
	Datum **a_elpp;			// Array of (Datum *) elements.
	} Array;

// External function declarations.
extern void aclear(Array *aryp);
extern Array *aclone(Array *aryp);
extern Datum *aeach(Array **arypp);
extern bool aeq(Array *aryp1,Array *aryp2);
extern void afree(Array *aryp);
extern Datum *aget(Array *srcp,ArraySize index,bool force);
extern Array *agraph(Array *aryp1,Array *aryp2);
extern int ajoin(Datum *destp,Array *srcp,char *delim);
extern Array *anew(ArraySize len,Datum *initp);
extern Datum *apop(Array *aryp);
extern int apush(Array *destp,Datum *srcp);
extern Datum *ashift(Array *aryp);
extern Array *aslice(Array *aryp,ArraySize index,ArraySize len);
extern Array *asplit(uint delim,char *src,int limit);
extern int aunshift(Array *destp,Datum *srcp);
#endif
