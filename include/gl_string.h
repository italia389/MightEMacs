// GeekLib (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// gl_string.h - Header file for GeekLib string routines.

#ifndef gl_string_h
#define gl_string_h

#include "gl_def.h"

// External function declarations.
extern char *chlit(int c,bool wrap);
extern char *stplcpy(char *destp,char *srcp,size_t size);
extern char *strfit(char *destp,size_t maxlen,char *srcp,size_t srclen);
#endif
