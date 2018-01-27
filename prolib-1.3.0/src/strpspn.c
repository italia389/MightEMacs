// ProLib (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// strpspn.c		Routine for spanning a string.

#include "os.h"
#include <stddef.h>
#include <string.h>

// Find first character in s1 that does not occur in s2 and return pointer to it.  If all characters occur in s2, return NULL.
char *strpspn(const char *s1,const char *s2) {
	short c;

	if(*s2 == '\0')
		return (char *) s1;
	while((c = *s1) != '\0') {
		if(strchr(s2,c) == NULL)
			return (char *) s1;
		++s1;
		}
	return NULL;
	}
