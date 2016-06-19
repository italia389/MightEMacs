// GeekLib (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// gl_def.h - Standard definitions for GeekLib routines.

#ifndef gl_def_h
#define gl_def_h

// Standard include files.
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

// Shortcuts for standard types.
typedef unsigned char
	uchar;
typedef unsigned short
	ushort;
typedef unsigned int
	uint;
typedef unsigned long
	ulong;

// Exception handling.
typedef struct {
	int code;			// Exception code: -2 (memory error) or -1 (other).
	ushort flags;			// Exception message flags.
	char *msg;			// Exception message.
	} GeekExcep;

#define GEHEAP		0x0001		// Message was allocated from heap.

// External variables.
extern GeekExcep excep;

// External functions.
extern int emsg(int code,char *msgp);
extern int emsge(int code);
extern int emsgf(int code,char *fmtp,...);
#endif
