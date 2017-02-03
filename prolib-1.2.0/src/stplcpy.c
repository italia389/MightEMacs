// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// stplcpy.c		Routine for copying a string with overflow prevention.

#include <stddef.h>

// Copy a string ... with length restrictions.  Null terminate result if "size" is greater than zero.  "size" is the actual size
// of the destination buffer.  Return pointer to terminating null (or "dest" if "size" is zero).
char *stplcpy(char *dest,char *src,size_t size) {

	if(size == 0)
		return dest;

	while(--size > 0 && *src != '\0')
		*dest++ = *src++;
	*dest = '\0';

	return dest;
	}
