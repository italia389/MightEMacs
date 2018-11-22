// ProLib (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// plarray.h	Definitions and declarations for Array library routines.

#ifndef plarray_h
#define plarray_h

#include "pldatum.h"

#define AChunkSz	8		// Starting size of array element chunks.
#define ArraySizeMax	LONG_MAX	// Maximum number of elements in an array.

// Array object.
typedef ssize_t ArraySize;
typedef struct {
	ArraySize a_size;		// Number of elements allocated.
	ArraySize a_used;		// Number of elements currently in use.
	Datum **a_elpp;			// Array of (Datum *) elements.  Elements 0..(a_used -1) always point to allocated
					// Datum objects and elements a_used..(a_size - 1) are always undefined (un-allocated).
	} Array;

// External function declarations.
extern Array *aclear(Array *aryp);
extern Array *aclone(Array *aryp);
extern void acompact(Array *aryp);
extern Datum *adelete(Array *aryp,ArraySize index);
extern Datum *aeach(Array **arypp);
extern bool aeq(Array *aryp1,Array *aryp2,bool ignore);
extern Datum *aget(Array *srcp,ArraySize index,bool force);
extern Array *agraph(Array *aryp1,Array *aryp2);
extern Array *ainit(Array *aryp);
extern int ainsert(Array *aryp,ArraySize index,Datum *datp,bool copy);
extern int ajoin(Datum *destp,Array *srcp,char *delim);
extern Array *anew(ArraySize len,Datum *initp);
extern Datum *apop(Array *aryp);
extern int apush(Array *aryp,Datum *datp,bool copy);
extern Datum *ashift(Array *aryp);
extern Array *aslice(Array *aryp,ArraySize index,ArraySize len,bool force,bool cut);
extern Array *asplit(short delim,char *src,int limit);
extern int aunshift(Array *aryp,Datum *datp,bool copy);
#endif
