// GeekLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// stplcpy.c

#include <stddef.h>

// Copy a string ... with length restrictions.  Null terminate result if size is greater than 0.  size is the actual size of
// the destination buffer.  Return pointer to terminating null (or dst if size is 0).
char *stplcpy(char *destp,char *srcp,size_t size) {

	if(size == 0)
		return destp;

	while(--size > 0 && *srcp != '\0')
		*destp++ = *srcp++;
	*destp = '\0';

	return destp;
	}
