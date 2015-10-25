// GeekLib (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// os.h - Standard Unix OS definitions for C code.

#ifndef os_h
#define os_h

// OS selection.  Set one of the following to "1".
#define BSD		0		// Unix BSD 4.2 and ULTRIX.
#define HPUX8		0		// HPUX HP 9000 ver. 8 and earlier.
#define HPUX9		0		// HPUX HP 9000 ver. 9.
#define OSX		0		// Apple OS X for Macintosh.
#define REDHAT		0		// Red Hat Linux.
#define SOLARIS		0		// Sun Solaris 2.5 and later.
#define USG		0		// Generic Unix System V.

// OS-dependent definitions.
#if !BSD && !HPUX8 && !HPUX9 && !REDHAT && !OSX && !SOLARIS && !USG
#error	"Unknown OS (a supported Unix platform was not selected in os.h)"
#endif

#if SOLARIS
#undef USG
#define USG		1
#endif

#if REDHAT
#define _GNU_SOURCE
#include <features.h>
#endif

#define MaxPathname 	PATH_MAX	/* Maximum pathname length */
#define MaxFilename	NAME_MAX	/* Maximum filename length */

#endif
