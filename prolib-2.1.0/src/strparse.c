// ProLib (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// strparse.c		Routine for parsing and returning delimited substrings from a string.

#include <stddef.h>

// Parse next token from string at *strp delimited by delim.  If token not found, return NULL; otherwise, (1), set *strp to next
// character past token, or NULL if terminating null was found; and (2), return pointer to trimmed result (which may be null).
char *strparse(char **strp,short delim) {
	short c;
	char *str1,*str2;
	char *str0 = *strp;

	// Get ready for scan.
	if(str0 == NULL)
		return NULL;
	while(*str0 == ' ' || *str0 == '\t')
		++str0;
	if(*str0 == '\0')
		return NULL;

	// Found beginning of next token... find end.
	str1 = str0;					// str0 points to beginning of token (first non-whitespace character).
	while((c = *str1) != '\0' && c != delim)
		++str1;
	str2 = str1;					// str1 points to token delimiter.
	while(str2 > str0 && (str2[-1] == ' ' || str2[-1] == '\t'))
		--str2;
	*str2 = '\0';					// str2 points to end of trimmed token.

	// Done.  Update *strp and return token pointer.
	*strp = (c == '\0') ? NULL : str1 + 1;
	return str0;
	}

