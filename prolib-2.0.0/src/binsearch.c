// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// binsearch.c		Routine for performing a binary search on a sorted array.

#include "pldef.h"
#include <string.h>

// Perform binary search on a sorted array (which may be empty) given key string, array pointer, number of elements, comparison
// function, fetch function, and index pointer.  Return true if match found; otherwise, false.  In either case, set
// *indexp to index of matching element or slot where key should be.
bool binsearch(char *key,void *table,ssize_t n,int (*cmp)(const char *s1,const char *s2),
 char *(*fetch)(void *table,ssize_t i),ssize_t *indexp) {
	int cresult;
	ssize_t lo,hi;
	ssize_t i = 0;
	bool result = false;

	// Set current search limit to entire array.
	i = lo = 0;
	hi = n - 1;

	// Loop until a match found or array shrinks to zero items.
	while(hi >= lo) {

		// Get the midpoint.
		i = (lo + hi) >> 1;

		// Do the comparison and check result.
		if((cresult = cmp(key,fetch(table,i))) == 0) {
			result = true;
			break;
			}
		if(cresult < 0)
			hi = i - 1;
		else
			lo = ++i;
		}

	// Return results.
	*indexp = i;
	return result;
	}
