// GeekLib (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// chlit.c

#include "os.h"
#include "geeklib.h"
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
