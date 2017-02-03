// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// memzcpy.c		Routine for copying bytes in memory.

#include <stddef.h>

// Copy "len" bytes from "src" to "dest".  Return pointer to byte after last byte copied to dest.
void *memzcpy(void *dest,const void *src,size_t len) {
	char *dest1 = (char *) dest;
	char *src1 = (char *) src;

	while(len--)
		*dest1++ = *src1++;
	return (void *) dest1;
	}
