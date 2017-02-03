// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// array.c	Array-handling routines.

#include "os.h"
#include "plexcep.h"
#include "pldatum.h"
#include "plarray.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>

// Clear an array (free array-element storage, if any).
void aclear(Array *aryp) {
	Datum **elpp = aryp->a_elpp;
	ArraySize n = aryp->a_used;
	while(n-- > 0)
		dclear(*elpp++);
	aryp->a_used = 0;
	}

// Free an array's storage and delete it.
void afree(Array *aryp) {
	Datum **elpp = aryp->a_elpp;
	ArraySize n = aryp->a_used;
	while(n-- > 0)
		ddelete(*elpp++);
	if(aryp->a_elpp != NULL)
		free((void *) aryp->a_elpp);
	free((void *) aryp);
	}

// Plug nil values into array slot(s), given index and length.  It is assumed that all slots have been allocated and are
// available for reassignment.  Return status code.
static int aplugnil(Array *aryp,ArraySize index,ArraySize len) {
	Datum **datpp = aryp->a_elpp + index;
	Datum **datppz = datpp + len;
	do {
		if(dnew(datpp) != 0)
			return -1;
		dsetnil(*datpp++);
		} while(datpp < datppz);
	return 0;
	}

// Check if array needs to grow so that it contains given array index (which mandates that array contain at least index + 1
// elements) or to accomodate given increase to "used" size.  Get more space if needed: either double old size or use AChunkSz
// if first allocation.  Return error if size needed exceeds maximum.  Any slots allocated beyond the current "used" size are
// left undefined if growSize > 0; otherwise, new slots up to and including the index are set to nil.  It is assumed that only
// one of "growSize" or "index" will be valid, so only one is checked.
static int aneed(Array *aryp,ArraySize growSize,ArraySize index) {
	ArraySize minSize;

	// Increasing "used" size?
	if(growSize > 0) {
		if(growSize <= aryp->a_size - aryp->a_used)	// Is unused portion of array big enough?
			return 0;				// Yes, nothing to do.
		if(growSize > ArraySizeMax - aryp->a_used)	// No, too small.  Does request exceed maximum?
			goto TooMuch;				// Yes, error.
		minSize = aryp->a_used + growSize;		// No, compute new minimum size.
		}
	else {
		if(index < aryp->a_used)			// Is index within "used" bounds?
			return 0;				// Yes, nothing to do.
		if(index < aryp->a_size)			// Is index within "size" bounds?
			goto PlugNil;				// Yes, expand "used" portion of array.
		if(index == ArraySizeMax || aryp->a_size == ArraySizeMax) // Need more space.  Error if request exceeds maximum.
TooMuch:
			return emsgf(-1,"Cannot grow array beyond maximum size (%d)",ArraySizeMax);
		minSize = index + 1;
		}

	// Expansion needed.  Double old size until it's big enough, without causing an integer overflow.
	ArraySize newSize = aryp->a_size;
	do {
		newSize = (newSize == 0) ? AChunkSz : (ArraySizeMax - newSize < newSize) ? ArraySizeMax : newSize * 2;
		} while(newSize < minSize);

	// Get more space.
	if((aryp->a_elpp = (Datum **) realloc((void *) aryp->a_elpp,newSize * sizeof(Datum *))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsgf(-1,"%s, allocating %d-element array",strerror(errno),newSize);
		}
	aryp->a_size = newSize;

	// Set some or all of new elements to nil, if applicable.
	if(index >= 0) {
PlugNil:
		if(aplugnil(aryp,aryp->a_used,index + 1 - aryp->a_used) != 0)
			return -1;
		aryp->a_used = index + 1;
		}

	return 0;
	}

// Insert one or more contiguous nil elements into an array at given location.  Return status code.
static int aspread(Array *aryp,ArraySize index,ArraySize len) {

	if(aneed(aryp,len,-1) != 0)				// Ensure len slots exist at end of used portion...
		return -1;
	if(aryp->a_used > 0) {
		Datum **elpp0,**elpp1,**elpp2;

		// 012345678	012345678	index = 1, len = 3
		// abcdef...	a...bcdef
		//  0    1  2
		elpp0 = aryp->a_elpp + index;			// set slot pointers...
		elpp2 = (elpp1 = aryp->a_elpp + aryp->a_used) + len;
		do {						// shift elements to the right...
			*--elpp2 = *--elpp1;
			} while(elpp1 > elpp0);
		}
	aryp->a_used += len;
	return aplugnil(aryp,index,len);			// and fill gap with nil values.
	}

// Get an array element and return it, given a signed index.  Return NULL if error.  If the index is negative, elements are
// selected from the end of the array such that the last element is -1, the second-to-last is -2, etc.; otherwise, elements are
// selected from the beginning with the first being 0.  If the index is zero or positive and "force" is true, the array will be
// enlarged if necessary to satisfy the request.  Any array elements that are created are set to nil values.  Otherwise, the
// referenced element must already exist.
Datum *aget(Array *srcp,ArraySize index,bool force) {

	if(index < 0) {
		if(-index > srcp->a_used)
			goto NoSuch;
		index += srcp->a_used;
		}
	else if(!force) {
		if(index >= srcp->a_used) {
NoSuch:
			emsgf(-1,"No such array element %d (array size %d)",index,srcp->a_used);
			return NULL;
			}
		}
	else if(aneed(srcp,0,index) != 0)
		return NULL;

	return srcp->a_elpp[index];
	}

// Create an array (in heap space) and return pointer to it, or NULL if error.  If len > 0, pre-allocate that number of nil (or
// *initp) elements; otherwise, leave empty.
Array *anew(ArraySize len,Datum *initp) {
	Array *aryp;

	if(len < 0) {
		emsgf(-1,"Invalid array length (%d)",len);
		goto ErrRtn;
		}

	if((aryp = (Array *) malloc(sizeof(Array))) == NULL) {	// Get space for array object...
		plexcep.flags |= ExcepMem;
		emsge(-1);
		goto ErrRtn;
		}
	aryp->a_size = aryp->a_used = 0;			// initialize empty array...
	aryp->a_elpp = NULL;

	if(len > 0) {						// allocate elements if requested...
		if(aneed(aryp,0,len - 1) != 0)
			goto ErrRtn;
		if(initp != NULL) {
			Datum **elpp = aryp->a_elpp;
			do {
				if(datcpy(*elpp++,initp) != 0)
					goto ErrRtn;
				} while(--len > 0);
			}
		}
	return aryp;						// and return it.
ErrRtn:
	return NULL;
	}

// Validate and normalize array slice values.  Return status code.
static int normalize(Array *aryp,ArraySize *indexp,ArraySize *lenp) {
	ArraySize index = *indexp;
	ArraySize len = *lenp;

	// Validate and normalize index.
	if(index < 0) {
		if(-index > aryp->a_used)
			goto RangeErr;
		index += aryp->a_used;
		}

	// Validate and normalize length.
	if(len < 0) {
		if(-len > aryp->a_used)
			goto RangeErr;
		ArraySize index2 = len + aryp->a_used;
		if(index2 < index)
			goto RangeErr;
		len = index2 - index;
		}

	// Validate slice.
	if(index >= aryp->a_used || index + len > aryp->a_used)
RangeErr:
		return emsgf(-1,"Array slice values [%d,%d] out of range (array size %d)",*indexp,*lenp,aryp->a_used);

	// All is well.  Return normalized slice values.
	*indexp = index;
	*lenp = len;
	return 0;
	}

// Create an array from a slice of another and return it (or NULL if error), given signed index and length.
Array *aslice(Array *aryp,ArraySize index,ArraySize len) {
	Array *aryp1;

	if(normalize(aryp,&index,&len) != 0 || (aryp1 = anew(len,NULL)) == NULL)
		return NULL;
	if(len > 0) {
		Datum **elpp0 = aryp->a_elpp + index;
		Datum **elpp1 = aryp1->a_elpp;
		do {
			if(datcpy(*elpp1++,*elpp0++) != 0)
				return NULL;
			} while(--len > 0);
		}

	// Return new array.
	return aryp1;
	}

// Clone an array and return the copy, or NULL if error.
Array *aclone(Array *aryp) {

	return aryp->a_used == 0 ? anew(0,NULL) : aslice(aryp,0,aryp->a_used);
	}

// Remove a Datum object from end of an array, shrink array by one, and return removed element.  If no elements left, return
// NULL.
Datum *apop(Array *aryp) {

	if(aryp->a_used == 0)
		return NULL;

	// Return (pointer) contents of last element and abandon slot, effectively shrinking the array.
	return aryp->a_elpp[--aryp->a_used];
	}

// Append a Datum object to an array.  Return status code.
int apush(Array *destp,Datum *srcp) {
	Datum *datp;

	return ((datp = aget(destp,destp->a_used,true)) == NULL || datcpy(datp,srcp) != 0) ? -1 : 0;
	}

// Remove a Datum object from beginning of an array, shrink array by one, and return removed element.  If no elements left,
// return NULL.
Datum *ashift(Array *aryp) {

	if(aryp->a_used == 0)
		return NULL;

	// Return (pointer) contents of first element and shift remaining ones left by one slot.
	Datum *datp = aryp->a_elpp[0];
	if(aryp->a_used > 1) {
		Datum **datpp0 = aryp->a_elpp;
		Datum **datpp1 = datpp0 + 1;
		Datum **datpp2 = aryp->a_elpp + aryp->a_used;

		do {
			*datpp0++ = *datpp1++;
			} while(datpp1 < datpp2);
		}
	--aryp->a_used;
	return datp;
	}

// Prepend a Datum object to an array.  Return status code.
int aunshift(Array *destp,Datum *srcp) {

	return aspread(destp,0,1) != 0 || datcpy(destp->a_elpp[0],srcp) != 0 ? -1 : 0;
	}

// Step through an array, returning each element in sequence, or NULL if none left.  "arypp" is an indirect pointer to the array
// object and is modified by this routine.
Datum *aeach(Array **arypp) {
	static struct {
		Datum **elpp,**elppz;
		} astate = {
			NULL,NULL
			};

	// Validate and initialize control pointers.
	if(*arypp != NULL) {
		if((*arypp)->a_size == 0)
			goto Done;
		astate.elppz = (astate.elpp = (*arypp)->a_elpp) + (*arypp)->a_used;
		*arypp = NULL;
		}
	else if(astate.elpp == NULL)
		return NULL;

	// Get next element and return it, or NULL if none left.
	if(astate.elpp == astate.elppz) {
Done:
		astate.elpp = NULL;
		return NULL;
		}
	return *astate.elpp++;
	}

// Join all array elements into destp with given delimiter.  Return status code.
int ajoin(Datum *destp,Array *srcp,char *delim) {
	char *str;

	if(srcp->a_used == 0)
		dsetnull(destp);
	else if(srcp->a_used == 1) {
		if((str = dtos(srcp->a_elpp[0],false)) == NULL || dsetstr(str,destp) != 0)
			return -1;
		}
	else {
		DStrFab sf;
		Datum **elpp = srcp->a_elpp;
		ArraySize n = srcp->a_used;

		if(dopenwith(&sf,destp,false) != 0)
			return -1;
		do {
			if((str = dtos(*elpp++,false)) == NULL || dputs(str,&sf) != 0 || dputs(delim,&sf) != 0)
				return -1;
			} while(--n > 0);
		if(dunputc(&sf) != 0 || dclose(&sf,sf_string) != 0)
			return -1;
		}
	return 0;
	}

// Split a string into an array using given field delimiter and limit value.  Substrings are separated by a single character,
// white space, or no delimiter, as indicated by unsigned integer value "delim":
//	VALUE		MEANING
//	0 or ' '	Delimiter is white space.  One or more of the characters (' ', '\t', '\n', '\r', '\f', '\v') are treated
//			as a single delimiter.  Additionally, if the delimiter is ' ', leading white space is ignored.
//	1 - 0xff	ASCII value of a character to use as the delimiter (except ' ').
//	> 0xff		No delimiter is defined.  A single-element array is returned containing the original string, unmodified.
//
// The "limit" argument controls the splitting process, as follows:
//	VALUE		MEANING
//	< 0		Each delimiter found is significant and delineates two substrings, either or both of which may be null.
//	0		Trailing null substrings are suppressed.
//	> 0		Maximum number of array elements to return.  The last element of the array will contain embedded
//			delimiter(s) if the number of delimiters in the string is equal to or greater than the "limit" value.
//
// Routine saves results in a new array and returns a pointer to the object, or NULL if error.
Array *asplit(uint delim,char *src,int limit) {
	Array *aryp;

	// Create an empty array.
	if((aryp = anew(0,NULL)) == NULL)
		return NULL;

	// Find tokens and push onto array.
	if(*src != '\0') {
		Datum *datp;
		char *str0,*str1,*str;
		size_t len;
		int itemCount = 0;
		char delims[7];

		// Set actual delimiter string.
		if(delim > 0xff)
			delims[0] = '\0';
		else if(delim > 0 && delim != ' ') {
			delims[0] = delim;
			delims[1] = '\0';
			}
		else
			strcpy(delims," \t\n\r\f\v");

		// Skip leading white space if requested.
		str = src;
		if(delim == ' ')
			for(;;) {
				if(strchr(delims,*str) == NULL)
					break;
				if(*++str == '\0')
					goto Done;
				}

		// Scan string for delimiters, creating an element at the end of each loop, if applicable.
		for(;;) {
			// Save beginning position and find next delimiter.
			str0 = str;

			// Use all remaining characters as next element if limit reached limit or no delimiters left.
			++itemCount;
			if((limit > 0 && itemCount == limit) || (str = strpbrk(str0,delims)) == NULL)
				len = (str = strchr(str0,'\0')) - str0;

			// Found delimiter.  Check for run if delimter is white space or limit is zero.  Bail out or adjust str
			// pointer if needed.
			else {
				len = str - str0;
				if(delims[1] != '\0' || limit == 0)
					for(str1 = str; ; ++str1) {
						if(str1[1] == '\0') {

							// Hit delimiter at end of string.  If limit is zero, bail out if
							// delimiter run is entire string; otherwise, mark as last element.
							if(limit == 0) {
								if(len == 0)
									goto Done;
								str = str1 + 1;
								}
							else
								str = str1;
							break;
							}
						if(strchr(delims,str1[1]) == NULL) {

							// Not at end of string.  Ignore run if not white space.
							if(delims[1] != '\0')
								str = str1;
							break;
							}
						}
				}

			// Push substring onto array.
			if((datp = aget(aryp,aryp->a_used,true)) == NULL || dsetsubstr(str0,len,datp) != 0)
				return NULL;

			// Onward.
			if(*str++ == '\0')
				break;
			}
		}
Done:
	return aryp;
	}

// Compare one array to another and return true if they are identical; otherwise, false.
bool aeq(Array *aryp1,Array *aryp2) {
	ArraySize size = aryp1->a_used;

	// Do sanity checks.
	if(size != aryp2->a_used)
		return false;
	if(size == 0)
		return true;

	// Both arrays have at least one element and the same number.  Compare them.
	Datum **elpp1 = aryp1->a_elpp;
	Datum **elpp2 = aryp2->a_elpp;
	do {
		if(!dateq(*elpp1++,*elpp2++))
			return false;
		} while(--size > 0);

	return true;
	}

// Graph ary2 onto ary1 (by copying elements) and return ary1, or NULL if error.
Array *agraph(Array *aryp1,Array *aryp2) {
	ArraySize used2;

	if((used2 = aryp2->a_used) > 0) {
		Datum **srcpp,**destpp;
		ArraySize used1 = aryp1->a_used;

		if(aneed(aryp1,used2,-1) != 0 || aplugnil(aryp1,used1,used2) != 0)
			return NULL;
		aryp1->a_used += used2;
		destpp = aryp1->a_elpp + used1;
		srcpp = aryp2->a_elpp;
		do {
			if(datcpy(*destpp++,*srcpp++) != 0)
				return NULL;
			} while(--used2 > 0);
		}

	return aryp1;
	}
