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
	Datum **a_elp;			// Array of (Datum *) elements.  Elements 0..(a_used -1) always point to allocated
					// Datum objects and elements a_used..(a_size - 1) are always undefined (un-allocated).
	} Array;

// External function declarations.
extern Array *aclear(Array *ary);
extern Array *aclone(Array *ary);
extern void acompact(Array *ary);
extern Datum *adelete(Array *ary,ArraySize index);
extern Datum *aeach(Array **aryp);
extern bool aeq(Array *ary1,Array *ary2,bool ignore);
extern Datum *aget(Array *src,ArraySize index,bool force);
extern Array *agraph(Array *ary1,Array *ary2);
extern Array *ainit(Array *ary);
extern int ainsert(Array *ary,ArraySize index,Datum *datum,bool copy);
extern int ajoin(Datum *dest,Array *src,char *delim);
extern Array *anew(ArraySize len,Datum *val);
extern Datum *apop(Array *ary);
extern int apush(Array *ary,Datum *datum,bool copy);
extern Datum *ashift(Array *ary);
extern Array *aslice(Array *ary,ArraySize index,ArraySize len,bool force,bool cut);
extern Array *asplit(short delim,char *src,int limit);
extern int aunshift(Array *ary,Datum *datum,bool copy);
#endif
