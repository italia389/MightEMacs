// ProLib (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// viz.c		Routines for converting characters to visible form.

#include "os.h"
#include "plexcep.h"
#include "plstring.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Return character c as a string, converting to visible form if non-text, as follows:
//	<NL>	012	Newline.
//	<CR>	015	Carriage return.
//	<ESC>	033	Escape.
//	<S>	040	Space (if VSpace flag is set).
//	^X		Non-printable 7-bit character.
//	<NN>		Ordinal value of 8-bit character in hexadecimal (default) or octal.
//	c		Character c as a string, if none of the above.
//
// If an error occurs, an exception message is set and NULL is returned.
char *vizc(short c,ushort flags) {
	ushort base;
	static char lit[6];

	// Valid base?
	if((base = flags & VBaseMask) > VBaseMax) {
		(void) emsgf(-1,"vizc(): Invalid base (%hu)",base);
		return NULL;
		}

	// Expose character.
	switch(c) {
		case '\n':
			return "<NL>";
		case '\r':
			return "<CR>";
		case 033:
			return "<ESC>";
		default:
			if(c == ' ') {
				if(flags & VSpace)
					return "<S>";
				goto DoChar;
				}
			if(c > ' ' && c <= '~') {
DoChar:
				lit[0] = c;
				lit[1] = '\0';
				break;
				}
			if(c < 0 || c > 0xff)
				return "<?>";
			else {
				char *fmt;
				if(c <= 0x7f) {
					fmt = "^%c";
					c ^= 0x40;
					}
				else
					fmt = (base == VBaseOctal) ? "<%.3o>" : (base == VBaseHex) ? "<%.2x>" : "<%.2X>";
				sprintf(lit,fmt,c);
				}
		}

	return lit;
	}

// Write a character to given file in visible form.  Pass "flags" to vizc().  Return status code.
int fvizc(short c,ushort flags,FILE *fp) {
	char *str;

	return (str = vizc(c,flags)) == NULL ? -1 : fputs(str,fp) == EOF ? emsge(-1) : 0;
	}

// Write str to given file, exposing all invisible characters.  Pass "flags" to vizc().  If len is zero, assume str is a
// null-terminated string; otherwise, write exactly len bytes.  Return status code.
int fvizs(const void *str,size_t len,ushort flags,FILE *fp) {
	char *str1 = (char *) str;
	size_t n = (len == 0) ? strlen(str1) : len;

	for(; n > 0; --n)
		if(fvizc(*str1++,flags,fp) != 0)
			return -1;
	return 0;
	}
