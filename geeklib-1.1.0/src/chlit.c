// GeekLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// chlit.c

#include "gl_def.h"
#include <stdlib.h>
#include <stdio.h>

// Convert a character to a string in printable form.  If wrap is true or 8-bit character, also wrap in "< >".  Return result.
char *chlit(int c,bool wrap) {
	static char wk[6];

	if(c == '\n')
		return "<NL>";
	if(c == '\r')
		return "<CR>";
	if(c == '\033')
		return "<ESC>";
	if(c < ' ' || c >= 0x7f) {
		char *fmtp;
		if(wrap || c > 0x7f)
			fmtp = "<%.2X>";
		else {
			fmtp = "^%c";
			c ^= 0x40;
			}
		sprintf(wk,fmtp,c);
		goto retn;
		}
	wk[0] = c;
	wk[1] = '\0';
retn:
	return wk;
	}
