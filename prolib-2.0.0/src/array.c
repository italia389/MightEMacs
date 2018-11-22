// ProLib (c) Copyright 2018 Richard W. Marinelli
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

// Initialize an Array object as an empty array and return pointer to it.
Array *ainit(Array *aryp) {

	aryp->a_used = aryp->a_size = 0;
	aryp->a_elpp = NULL;
	return aryp;
	}

// Clear an array: free element storage (if any), set used size to zero, and return a pointer to the reinitialized Array object
// (to be freed or reused by the caller).
Array *aclear(Array *aryp) {
	Datum **elpp = aryp->a_elpp;
	if(elpp != NULL) {
		Datum **elppz = elpp + aryp->a_used;
		while(elpp < elppz)
			ddelete(*elpp++);
		free((void *) aryp->a_elpp);
		}
	return ainit(aryp);
	}

// Plug nil values into array slot(s), given index and length.  It is assumed that all slots have been allocated and are
// available for reassignment.  Return status code.
static int aplugnil(Array *aryp,ArraySize index,ArraySize len) {

	if(len > 0) {
		Datum **datpp = aryp->a_elpp + index;
		Datum **datppz = datpp + len;
		do {
			if(dnew(datpp++) != 0)
				return -1;
			} while(datpp < datppz);
		}
	return 0;
	}

// Check if array needs to grow so that it contains given array index (which mandates that array contain at least index + 1
// elements) or to accomodate given increase to "used" size.  Get more space if needed: use AChunkSz if first allocation, double
// old size for second and third, then increase old size by 7/8 thereafter (to prevent the array from growing too fast for large
// numbers).  Return error if size needed exceeds maximum.  Any slots allocated beyond the current "used" size are left
// undefined if "fill" is false; otherwise, new slots up to and including the index are set to nil and the "used" size is
// updated.  It is assumed that only one of "growSize" or "index" will be valid, so only one is checked.
static int aneed(Array *aryp,ArraySize growSize,ArraySize index,bool fill) {
	ArraySize minSize;

	// Increasing "used" size?
	if(growSize > 0) {
		if(growSize <= aryp->a_size - aryp->a_used)	// Is unused portion of array big enough?
			goto CheckFill;				// Yes, check if fill requested.
		if(growSize > ArraySizeMax - aryp->a_used)	// No, too small.  Does request exceed maximum?
			goto TooMuch;				// Yes, error.
		minSize = aryp->a_used + growSize;		// No, compute new minimum size.
		}
	else {
		if(index < aryp->a_used)			// Is index within "used" bounds?
			return 0;				// Yes, nothing to do.
		growSize = index + 1 - aryp->a_used;		// No, compute grow size.
		if(index < aryp->a_size)			// Is index within "size" bounds?
			goto CheckFill;				// Yes, check if fill requested.
		if(index == ArraySizeMax || aryp->a_size == ArraySizeMax) // Need more space.  Error if request exceeds maximum.
TooMuch:
			return emsgf(-1,"Cannot grow array beyond maximum size (%d)",ArraySizeMax);
		minSize = index + 1;
		}

	// Expansion needed.  Double old size until it's big enough, without causing an integer overflow.
	ArraySize newSize = aryp->a_size;
	do {
		newSize = (newSize == 0) ? AChunkSz : (newSize < AChunkSz * 4) ? newSize * 2 :
		 (ArraySizeMax - newSize < newSize) ? ArraySizeMax : newSize + (newSize >> 3) * 7;
		} while(newSize < minSize);

	// Get more space.
	if((aryp->a_elpp = (Datum **) realloc((void *) aryp->a_elpp,newSize * sizeof(Datum *))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsgf(-1,"%s, allocating %d-element array",strerror(errno),newSize);
		}
	aryp->a_size = newSize;
CheckFill:
	if(fill) {
		// Set new elements to nil and increase "used" size.
		if(aplugnil(aryp,aryp->a_used,growSize) != 0)
			return -1;
		aryp->a_used += growSize;
		}

	return 0;
	}

// Validate and normalize array slice parameters (convert to non-negative values).  Return status code.
static int normalize(Array *aryp,ArraySize *indexp,ArraySize *lenp,bool allowNegLen,bool allowBigLen) {
	ArraySize index = *indexp;
	ArraySize len = *lenp;

	// Validate and normalize index.
	if(index < 0) {
		if(-index > aryp->a_used)
			goto IndexErr;
		index += aryp->a_used;
		}
	else if(index >= aryp->a_used)
IndexErr:
		return emsgf(-1,"Array index %d out of range (array size %d)",index,aryp->a_used);

	// Validate and normalize length.
	if(len < 0) {
		if(!allowNegLen || -len > aryp->a_used)
			goto RangeErr;
		ArraySize index2 = len + aryp->a_used;
		if(index2 < index)
			goto RangeErr;
		len = index2 - index;
		}

	// Validate slice span.
	if(index + len > aryp->a_used) {
		if(allowBigLen)
			len = aryp->a_used - index;
		else
RangeErr:
			return emsgf(-1,"Array slice values [%d,%d] out of range (array size %d)",*indexp,*lenp,aryp->a_used);
		}

	// All is well.  Return normalized slice values.
	*indexp = index;
	*lenp = len;
	return 0;
	}

// Open slot in an array at given index by shifting higher elements to the right one position (so that a value can be inserted
// there by the caller).  If "plug" is true, set slot to a nil value.  Update "used" size and return status code.  Index is
// assumed to be <= "used" size.
static int aspread(Array *aryp,ArraySize index,bool plug) {

	if(aneed(aryp,1,-1,false) != 0)				// Ensure a slot exists at end of used portion.
		return -1;
	if(index < aryp->a_used) {				// If not inserting at end of array...
		Datum **elpp0,**elpp1;

		// 012345678	012345678	index = 1
		// abcdef...	a.bcdef..
		//  0    1
		elpp0 = aryp->a_elpp + index;			// set slot pointers...
		elpp1 = aryp->a_elpp + aryp->a_used;
		do {						// and shift elements to the right.
			elpp1[0] = elpp1[-1];
			} while(--elpp1 > elpp0);
		}
	++aryp->a_used;						// Bump "used" size...
	return plug ? aplugnil(aryp,index,1) : 0;		// and set slot to nil if requested.
	}

// Remove one or more contiguous elements from an array at given location.  It is assumed that the Datum object(s) in the slice
// have already been saved or freed and that "index" and "len" are in range.
static void ashrink(Array *aryp,ArraySize index,ArraySize len) {

	if(len > 0) {
		if(index + len < aryp->a_used) {		// Slice at end of array?
			Datum **elpp0,**elpp1,**elpp2;		// No, close the gap.

			// 01234567	01234567	index = 1, len = 3
			// abcdef..	aef.....
			//  0  1   2
			elpp1 = (elpp0 = aryp->a_elpp + index) + len;	// Set slot pointers...
			elpp2 = aryp->a_elpp + aryp->a_size;
			do {					// and shift elements to the left.
				*elpp0++ = *elpp1++;
				} while(elpp1 < elpp2);
			}
		aryp->a_used -= len;				// Update "used" length.
		}
	}

// Remove nil elements from an array (in place) and shift elements left to fill the hole(s).
void acompact(Array *aryp) {

	if(aryp->a_used > 0) {
		Datum **elpp0,**elpp1,**elppz;
		elppz = (elpp0 = elpp1 = aryp->a_elpp) + aryp->a_used;
		do {
			if((*elpp1)->d_type == dat_nil)		// Nil element?
				ddelete(*elpp1);		// Yes, delete it.
			else
				*elpp0++ = *elpp1;		// No, shift it left.
			} while(++elpp1 < elppz);
		aryp->a_used = elpp0 - aryp->a_elpp;		// Update "used" length.
		}
	}

// Get an array element and return it, given signed index.  Return NULL if error.  If the index is negative, elements are
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
	else if(aneed(srcp,0,index,true) != 0)
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
	ainit(aryp);						// initialize empty array...
	if(len > 0) {						// allocate elements if requested...
		if(aneed(aryp,0,len - 1,true) != 0)
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

// Create an array from a slice of another and return it (or NULL if error), given signed index and length.  If "force" is true,
// allow length to exceed number of elements in array beyond index.  If "cut" is true, delete sliced elements from source array.
Array *aslice(Array *aryp,ArraySize index,ArraySize len,bool force,bool cut) {
	Array *aryp1;

	if(normalize(aryp,&index,&len,true,force) != 0 || (aryp1 = anew(cut ? 0 : len,NULL)) == NULL)
		return NULL;
	if(len > 0) {
		if(cut) {
			if(aneed(aryp1,len,-1,false) != 0)
				return NULL;
			aryp1->a_used = len;
			}
		Datum **elpp = aryp->a_elpp + index;
		Datum **elppz = elpp + len;
		Datum **elpp1 = aryp1->a_elpp;
		do {
			if(cut)
				*elpp1 = *elpp;
			else if(datcpy(*elpp1,*elpp) != 0)
				return NULL;
			++elpp1;
			} while(++elpp < elppz);
		}

	// Shrink original array if needed and return new array.
	if(cut)
		ashrink(aryp,index,len);
	return aryp1;
	}

// Clone an array and return the copy, or NULL if error.
Array *aclone(Array *aryp) {

	return aryp->a_used == 0 ? anew(0,NULL) : aslice(aryp,0,aryp->a_used,false,false);
	}

// Remove a Datum object from an array at given index, shrink array by one, and return removed element.  If no elements left,
// return NULL.
static Datum *acut(Array *aryp,ArraySize index) {

	if(aryp->a_used == 0)
		return NULL;

	// Grab element, abandon slot, and return it.
	Datum *datp = aryp->a_elpp[index];
	ashrink(aryp,index,1);
	return datp;
	}

// Insert a Datum object (or a copy if "copy" is true) into an array.  Return status code.
static int aput(Array *aryp,ArraySize index,Datum *datp,bool copy) {

	if(aspread(aryp,index,copy) != 0)
		return -1;
	if(!copy)
		aryp->a_elpp[index] = datp;
	else if(datcpy(aryp->a_elpp[index],datp) != 0)
		return -1;
	return 0;
	}

// Remove a Datum object from end of an array, shrink array by one, and return removed element.  If no elements left, return
// NULL.
Datum *apop(Array *aryp) {

	return acut(aryp,aryp->a_used - 1);
	}

// Append a Datum object (or a copy if "copy" is true) to an array.  Return status code.
int apush(Array *aryp,Datum *datp,bool copy) {

	return aput(aryp,aryp->a_used,datp,copy);
	}

// Remove a Datum object from beginning of an array, shrink array by one, and return removed element.  If no elements left,
// return NULL.
Datum *ashift(Array *aryp) {

	return acut(aryp,0);
	}

// Prepend a Datum object (or a copy if "copy" is true) to an array.  Return status code.
int aunshift(Array *aryp,Datum *datp,bool copy) {

	return aput(aryp,0,datp,copy);
	}

// Remove a Datum object from an array at given signed index, shrink array by one, and return removed element.  If index out of
// range, set error and return NULL.
Datum *adelete(Array *aryp,ArraySize index) {
	ArraySize len = 1;

	return (normalize(aryp,&index,&len,false,false) != 0) ? NULL : acut(aryp,index);
	}

// Insert a Datum object (or a copy if "copy" is true) into an array, given signed index (which may be equal to "used" size).
// Return status code.
int ainsert(Array *aryp,ArraySize index,Datum *datp,bool copy) {
	ArraySize len = 1;

	if(index == aryp->a_used)
		return apush(aryp,datp,copy);
	if(normalize(aryp,&index,&len,false,false) != 0)
		return -1;
	return aput(aryp,index,datp,copy);
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
// white space, or no delimiter, as indicated by integer value "delim":
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
Array *asplit(short delim,char *src,int limit) {
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
bool aeq(Array *aryp1,Array *aryp2,bool ignore) {
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
		if(!dateq(*elpp1++,*elpp2++,ignore))
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

		if(aneed(aryp1,used2,-1,true) != 0)
			return NULL;
		destpp = aryp1->a_elpp + used1;
		srcpp = aryp2->a_elpp;
		do {
			if(datcpy(*destpp++,*srcpp++) != 0)
				return NULL;
			} while(--used2 > 0);
		}

	return aryp1;
	}
