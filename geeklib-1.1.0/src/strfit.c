// GeekLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// strfit.c

#include "gl_string.h"
#include <string.h>

// Copy and shrink string by inserting an ellipsis in the middle (if necessary), given destination pointer, maximum destination
// length (excluding the trailing null), source pointer, and source length.  Destination buffer is assumed to be at least
// maxlen + 1 bytes in size.  If source length is zero, source string is assumed to be null terminated.  Return dest.
char *strfit(char *destp,size_t maxlen,char *srcp,size_t srclen) {
	size_t slen,len,cutsize;
	char *strp,*ellipsis;

	// Check for minimum shrinking parameters.
	slen = (srclen == 0) ? strlen(srcp) : srclen;
	if(maxlen < 5 || slen <= maxlen)

		// Dest or source too short, so just copy the maximum.
		(void) stplcpy(destp,srcp,(slen < maxlen ? slen : maxlen) + 1);
	else {
		// Determine maximum number of characters can copy from source.
		if(maxlen < 30) {
			ellipsis = "..";			// Use shorter ellipsis for small dest.
			len = 2;
			}
		else {
			ellipsis = "...";
			len = 3;
			}
		cutsize = slen - maxlen + len;			// Number of characters to leave out of middle of source.
		len = (slen - cutsize) / 2;			// Length of initial segment (half of what will be copied).
								// Include white space at end first segment if present.
		if(((strp = srcp + len)[0] == ' ' || strp[0] == '\t') && strp[-1] != ' ' && strp[-1] != '\t')
			++len;
		strp = stplcpy(destp,srcp,len + 1);		// Copy initial segment.
		strp = stpcpy(strp,ellipsis);			// Add ellipsis.
								// Copy last segment.
		stplcpy(strp,srcp + cutsize + len,slen - cutsize - len + 1);
		}

	return destp;
	}
