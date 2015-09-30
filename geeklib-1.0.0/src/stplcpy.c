// GeekLib (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
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
