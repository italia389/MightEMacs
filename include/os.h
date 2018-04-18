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
#define LINUX		0		// Linux system (CentOS, Debian, GNU/Linux, Red Hat, Ubuntu, and others).
#define MACOS		0		// Apple macOS for Macintosh.
#define USG		0		// Generic Unix System V.

// OS-dependent definitions.
#if !BSD && !LINUX && !MACOS && !USG
#error	"Unknown OS (a supported Unix platform was not selected in os.h)"
#endif

#if LINUX
#define _GNU_SOURCE
#include <features.h>
#endif

#define MaxPathname 	PATH_MAX	/* Maximum pathname length */
#define MaxFilename	NAME_MAX	/* Maximum filename length */

#endif
