// (c) Copyright 2020 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// stdos.h - Standard Unix Operating System definitions for C source code.

#ifndef stdos_h
#define stdos_h

// OS-dependent definitions.
#if __gnu_linux__
#define _GNU_SOURCE
#include <features.h>
#endif
#if __APPLE__
#define MACOS	1
#elif __linux || __linux__ || linux
#define LINUX	1
#endif

// Standard include files.
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

// Standard macros.
#define MaxPathname 	PATH_MAX	// Maximum pathname length.
#define MaxFilename	NAME_MAX	// Maximum filename length.
#define elementsof(x) (sizeof(x) / sizeof(*x))

// Shortcuts for standard types.
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
#endif
