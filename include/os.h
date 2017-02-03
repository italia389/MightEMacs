// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// os.h - Standard Unix OS definitions for C code.

#ifndef os_h
#define os_h

// OS selection.  Set one of the following to "1".
#define BSD		0		// Unix BSD 4.2 and ULTRIX.
#define CENTOS		0		// CentOS Linux.
#define DEBIAN		0		// Debian Linux.
#define HPUX8		0		// HPUX HP 9000 ver. 8 and earlier.
#define HPUX		0		// HPUX HP 9000 ver. 9. and later.
#define MACOS		0		// Apple MacOS for Macintosh.
#define REDHAT		0		// Red Hat Linux.
#define SOLARIS		0		// Sun Solaris 2.5 and later.
#define USG		0		// Generic Unix System V.

// OS-dependent definitions.
#if !BSD && !CENTOS && !DEBIAN && !HPUX8 && !HPUX && !MACOS && !REDHAT && !SOLARIS && !USG
#error	"Unknown OS (a supported Unix platform was not selected in os.h)"
#endif

#if SOLARIS
#undef USG
#define USG		1
#endif

#if REDHAT && !CENTOS
#undef CENTOS
#define CENTOS		1
#endif

#if CENTOS || DEBIAN
#define _GNU_SOURCE
#include <features.h>
#endif

#define MaxPathname 	PATH_MAX	/* Maximum pathname length */
#define MaxFilename	NAME_MAX	/* Maximum filename length */

#endif
