// ProLib (c) Copyright 2019 Richard W. Marinelli
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
Array *ainit(Array *ary) {

	ary->a_used = ary->a_size = 0;
	ary->a_elp = NULL;
	return ary;
	}

// Clear an array: free element storage (if any), set used size to zero, and return a pointer to the reinitialized Array object
// (to be freed or reused by the caller).
Array *aclear(Array *ary) {
	Datum **el = ary->a_elp;
	if(el != NULL) {
		Datum **elpz = el + ary->a_used;
		while(el < elpz)
			ddelete(*el++);
		free((void *) ary->a_elp);
		}
	return ainit(ary);
	}

// Plug nil values into array slot(s), given index and length.  It is assumed that all slots have been allocated and are
// available for reassignment.  Return status code.
static int aplugnil(Array *ary,ArraySize index,ArraySize len) {

	if(len > 0) {
		Datum **datump = ary->a_elp + index;
		Datum **datumpz = datump + len;
		do {
			if(dnew(datump++) != 0)
				return -1;
			} while(datump < datumpz);
		}
	return 0;
	}

// Check if array needs to grow so that it contains given array index (which mandates that array contain at least index + 1
// elements) or to accomodate given increase to "used" size.  Get more space if needed: use AChunkSz if first allocation, double
// old size for second and third, then increase old size by 7/8 thereafter (to prevent the array from growing too fast for large
// numbers).  Return error if size needed exceeds maximum.  Any slots allocated beyond the current "used" size are left
// undefined if "fill" is false; otherwise, new slots up to and including the index are set to nil and the "used" size is
// updated.  It is assumed that only one of "growSize" or "index" will be valid, so only one is checked.
static int aneed(Array *ary,ArraySize growSize,ArraySize index,bool fill) {
	ArraySize minSize;

	// Increasing "used" size?
	if(growSize > 0) {
		if(growSize <= ary->a_size - ary->a_used)	// Is unused portion of array big enough?
			goto CheckFill;				// Yes, check if fill requested.
		if(growSize > ArraySizeMax - ary->a_used)	// No, too small.  Does request exceed maximum?
			goto TooMuch;				// Yes, error.
		minSize = ary->a_used + growSize;		// No, compute new minimum size.
		}
	else {
		if(index < ary->a_used)			// Is index within "used" bounds?
			return 0;				// Yes, nothing to do.
		growSize = index + 1 - ary->a_used;		// No, compute grow size.
		if(index < ary->a_size)			// Is index within "size" bounds?
			goto CheckFill;				// Yes, check if fill requested.
		if(index == ArraySizeMax || ary->a_size == ArraySizeMax) // Need more space.  Error if request exceeds maximum.
TooMuch:
			return emsgf(-1,"Cannot grow array beyond maximum size (%d)",ArraySizeMax);
		minSize = index + 1;
		}

	// Expansion needed.  Double old size until it's big enough, without causing an integer overflow.
	ArraySize newSize = ary->a_size;
	do {
		newSize = (newSize == 0) ? AChunkSz : (newSize < AChunkSz * 4) ? newSize * 2 :
		 (ArraySizeMax - newSize < newSize) ? ArraySizeMax : newSize + (newSize >> 3) * 7;
		} while(newSize < minSize);

	// Get more space.
	if((ary->a_elp = (Datum **) realloc((void *) ary->a_elp,newSize * sizeof(Datum *))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsgf(-1,"%s, allocating %d-element array",strerror(errno),newSize);
		}
	ary->a_size = newSize;
CheckFill:
	if(fill) {
		// Set new elements to nil and increase "used" size.
		if(aplugnil(ary,ary->a_used,growSize) != 0)
			return -1;
		ary->a_used += growSize;
		}

	return 0;
	}

// Validate and normalize array slice parameters (convert to non-negative values).  Return status code.
static int normalize(Array *ary,ArraySize *index,ArraySize *len,bool allowNegLen,bool allowBigLen) {
	ArraySize aindex = *index;
	ArraySize alen = *len;

	// Validate and normalize index.
	if(aindex < 0) {
		if(-aindex > ary->a_used)
			goto IndexErr;
		aindex += ary->a_used;
		}
	else if(aindex >= ary->a_used)
IndexErr:
		return emsgf(-1,"Array index %d out of range (array size %d)",aindex,ary->a_used);

	// Validate and normalize length.
	if(alen < 0) {
		if(!allowNegLen || -alen > ary->a_used)
			goto RangeErr;
		ArraySize i2 = alen + ary->a_used;
		if(i2 < aindex)
			goto RangeErr;
		alen = i2 - aindex;
		}

	// Validate slice span.
	if(aindex + alen > ary->a_used) {
		if(allowBigLen)
			alen = ary->a_used - aindex;
		else
RangeErr:
			return emsgf(-1,"Array slice values [%d,%d] out of range (array size %d)",*index,*len,ary->a_used);
		}

	// All is well.  Return normalized slice values.
	*index = aindex;
	*len = alen;
	return 0;
	}

// Open slot in an array at given index by shifting higher elements to the right one position (so that a value can be inserted
// there by the caller).  If "plug" is true, set slot to a nil value.  Update "used" size and return status code.  Index is
// assumed to be <= "used" size.
static int aspread(Array *ary,ArraySize index,bool plug) {

	if(aneed(ary,1,-1,false) != 0)				// Ensure a slot exists at end of used portion.
		return -1;
	if(index < ary->a_used) {				// If not inserting at end of array...
		Datum **el0,**el1;

		// 012345678	012345678	index = 1
		// abcdef...	a.bcdef..
		//  0    1
		el0 = ary->a_elp + index;			// set slot pointers...
		el1 = ary->a_elp + ary->a_used;
		do {						// and shift elements to the right.
			el1[0] = el1[-1];
			} while(--el1 > el0);
		}
	++ary->a_used;						// Bump "used" size...
	return plug ? aplugnil(ary,index,1) : 0;		// and set slot to nil if requested.
	}

// Remove one or more contiguous elements from an array at given location.  It is assumed that the Datum object(s) in the slice
// have already been saved or freed and that "index" and "len" are in range.
static void ashrink(Array *ary,ArraySize index,ArraySize len) {

	if(len > 0) {
		if(index + len < ary->a_used) {		// Slice at end of array?
			Datum **el0,**el1,**el2;		// No, close the gap.

			// 01234567	01234567	index = 1, len = 3
			// abcdef..	aef.....
			//  0  1   2
			el1 = (el0 = ary->a_elp + index) + len;	// Set slot pointers...
			el2 = ary->a_elp + ary->a_size;
			do {					// and shift elements to the left.
				*el0++ = *el1++;
				} while(el1 < el2);
			}
		ary->a_used -= len;				// Update "used" length.
		}
	}

// Remove nil elements from an array (in place) and shift elements left to fill the hole(s).
void acompact(Array *ary) {

	if(ary->a_used > 0) {
		Datum **el0,**el1,**elpz;
		elpz = (el0 = el1 = ary->a_elp) + ary->a_used;
		do {
			if((*el1)->d_type == dat_nil)		// Nil element?
				ddelete(*el1);		// Yes, delete it.
			else
				*el0++ = *el1;		// No, shift it left.
			} while(++el1 < elpz);
		ary->a_used = el0 - ary->a_elp;		// Update "used" length.
		}
	}

// Get an array element and return it, given signed index.  Return NULL if error.  If the index is negative, elements are
// selected from the end of the array such that the last element is -1, the second-to-last is -2, etc.; otherwise, elements are
// selected from the beginning with the first being 0.  If the index is zero or positive and "force" is true, the array will be
// enlarged if necessary to satisfy the request.  Any array elements that are created are set to nil values.  Otherwise, the
// referenced element must already exist.
Datum *aget(Array *src,ArraySize index,bool force) {

	if(index < 0) {
		if(-index > src->a_used)
			goto NoSuch;
		index += src->a_used;
		}
	else if(!force) {
		if(index >= src->a_used) {
NoSuch:
			emsgf(-1,"No such array element %d (array size %d)",index,src->a_used);
			return NULL;
			}
		}
	else if(aneed(src,0,index,true) != 0)
		return NULL;

	return src->a_elp[index];
	}

// Create an array (in heap space) and return pointer to it, or NULL if error.  If len > 0, pre-allocate that number of nil (or
// *val) elements; otherwise, leave empty.
Array *anew(ArraySize len,Datum *val) {
	Array *ary;

	if(len < 0) {
		emsgf(-1,"Invalid array length (%d)",len);
		goto ErrRtn;
		}

	if((ary = (Array *) malloc(sizeof(Array))) == NULL) {	// Get space for array object...
		plexcep.flags |= ExcepMem;
		emsge(-1);
		goto ErrRtn;
		}
	ainit(ary);						// initialize empty array...
	if(len > 0) {						// allocate elements if requested...
		if(aneed(ary,0,len - 1,true) != 0)
			goto ErrRtn;
		if(val != NULL) {
			Datum **el = ary->a_elp;
			do {
				if(datcpy(*el++,val) != 0)
					goto ErrRtn;
				} while(--len > 0);
			}
		}
	return ary;						// and return it.
ErrRtn:
	return NULL;
	}

// Create an array from a slice of another and return it (or NULL if error), given signed index and length.  If "force" is true,
// allow length to exceed number of elements in array beyond index.  If "cut" is true, delete sliced elements from source array.
Array *aslice(Array *ary,ArraySize index,ArraySize len,bool force,bool cut) {
	Array *ary1;

	if(normalize(ary,&index,&len,true,force) != 0 || (ary1 = anew(cut ? 0 : len,NULL)) == NULL)
		return NULL;
	if(len > 0) {
		if(cut) {
			if(aneed(ary1,len,-1,false) != 0)
				return NULL;
			ary1->a_used = len;
			}
		Datum **el = ary->a_elp + index;
		Datum **elpz = el + len;
		Datum **el1 = ary1->a_elp;
		do {
			if(cut)
				*el1 = *el;
			else if(datcpy(*el1,*el) != 0)
				return NULL;
			++el1;
			} while(++el < elpz);
		}

	// Shrink original array if needed and return new array.
	if(cut)
		ashrink(ary,index,len);
	return ary1;
	}

// Clone an array and return the copy, or NULL if error.
Array *aclone(Array *ary) {

	return ary->a_used == 0 ? anew(0,NULL) : aslice(ary,0,ary->a_used,false,false);
	}

// Remove a Datum object from an array at given index, shrink array by one, and return removed element.  If no elements left,
// return NULL.
static Datum *acut(Array *ary,ArraySize index) {

	if(ary->a_used == 0)
		return NULL;

	// Grab element, abandon slot, and return it.
	Datum *datum = ary->a_elp[index];
	ashrink(ary,index,1);
	return datum;
	}

// Insert a Datum object (or a copy if "copy" is true) into an array.  Return status code.
static int aput(Array *ary,ArraySize index,Datum *datum,bool copy) {

	if(aspread(ary,index,copy) != 0)
		return -1;
	if(!copy)
		ary->a_elp[index] = datum;
	else if(datcpy(ary->a_elp[index],datum) != 0)
		return -1;
	return 0;
	}

// Remove a Datum object from end of an array, shrink array by one, and return removed element.  If no elements left, return
// NULL.
Datum *apop(Array *ary) {

	return acut(ary,ary->a_used - 1);
	}

// Append a Datum object (or a copy if "copy" is true) to an array.  Return status code.
int apush(Array *ary,Datum *datum,bool copy) {

	return aput(ary,ary->a_used,datum,copy);
	}

// Remove a Datum object from beginning of an array, shrink array by one, and return removed element.  If no elements left,
// return NULL.
Datum *ashift(Array *ary) {

	return acut(ary,0);
	}

// Prepend a Datum object (or a copy if "copy" is true) to an array.  Return status code.
int aunshift(Array *ary,Datum *datum,bool copy) {

	return aput(ary,0,datum,copy);
	}

// Remove a Datum object from an array at given signed index, shrink array by one, and return removed element.  If index out of
// range, set error and return NULL.
Datum *adelete(Array *ary,ArraySize index) {
	ArraySize len = 1;

	return (normalize(ary,&index,&len,false,false) != 0) ? NULL : acut(ary,index);
	}

// Insert a Datum object (or a copy if "copy" is true) into an array, given signed index (which may be equal to "used" size).
// Return status code.
int ainsert(Array *ary,ArraySize index,Datum *datum,bool copy) {
	ArraySize len = 1;

	if(index == ary->a_used)
		return apush(ary,datum,copy);
	if(normalize(ary,&index,&len,false,false) != 0)
		return -1;
	return aput(ary,index,datum,copy);
	}

// Step through an array, returning each element in sequence, or NULL if none left.  "ary" is an indirect pointer to the array
// object and is modified by this routine.
Datum *aeach(Array **ary) {
	static struct {
		Datum **el,**elpz;
		} astate = {
			NULL,NULL
			};

	// Validate and initialize control pointers.
	if(*ary != NULL) {
		if((*ary)->a_size == 0)
			goto Done;
		astate.elpz = (astate.el = (*ary)->a_elp) + (*ary)->a_used;
		*ary = NULL;
		}
	else if(astate.el == NULL)
		return NULL;

	// Get next element and return it, or NULL if none left.
	if(astate.el == astate.elpz) {
Done:
		astate.el = NULL;
		return NULL;
		}
	return *astate.el++;
	}

// Join all array elements into dest with given delimiter.  Return status code.
int ajoin(Datum *dest,Array *src,char *delim) {
	char *str;

	if(src->a_used == 0)
		dsetnull(dest);
	else if(src->a_used == 1) {
		if((str = dtos(src->a_elp[0],false)) == NULL || dsetstr(str,dest) != 0)
			return -1;
		}
	else {
		DStrFab sf;
		Datum **el = src->a_elp;
		ArraySize n = src->a_used;

		if(dopenwith(&sf,dest,false) != 0)
			return -1;
		do {
			if((str = dtos(*el++,false)) == NULL || dputs(str,&sf) != 0 || dputs(delim,&sf) != 0)
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
	Array *ary;

	// Create an empty array.
	if((ary = anew(0,NULL)) == NULL)
		return NULL;

	// Find tokens and push onto array.
	if(*src != '\0') {
		Datum *datum;
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
			if((datum = aget(ary,ary->a_used,true)) == NULL || dsetsubstr(str0,len,datum) != 0)
				return NULL;

			// Onward.
			if(*str++ == '\0')
				break;
			}
		}
Done:
	return ary;
	}

// Compare one array to another and return true if they are identical; otherwise, false.
bool aeq(Array *ary1,Array *ary2,bool ignore) {
	ArraySize size = ary1->a_used;

	// Do sanity checks.
	if(size != ary2->a_used)
		return false;
	if(size == 0)
		return true;

	// Both arrays have at least one element and the same number.  Compare them.
	Datum **el1 = ary1->a_elp;
	Datum **el2 = ary2->a_elp;
	do {
		if(!dateq(*el1++,*el2++,ignore))
			return false;
		} while(--size > 0);

	return true;
	}

// Graph ary2 onto ary1 (by copying elements) and return ary1, or NULL if error.
Array *agraph(Array *ary1,Array *ary2) {
	ArraySize used2;

	if((used2 = ary2->a_used) > 0) {
		Datum **src,**dest;
		ArraySize used1 = ary1->a_used;

		if(aneed(ary1,used2,-1,true) != 0)
			return NULL;
		dest = ary1->a_elp + used1;
		src = ary2->a_elp;
		do {
			if(datcpy(*dest++,*src++) != 0)
				return NULL;
			} while(--used2 > 0);
		}

	return ary1;
	}
