// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// plexcep.h		Header file for ProLib exception handling.

#ifndef plexcep_h
#define plexcep_h

#include "pldef.h"

// Definitions for core exception handling.
typedef struct {
	int code;			// Exception code: -1 (error) or 0 (success).
	ushort flags;			// Exception message flags.
	char *msg;			// Exception message.
	} ProLibExcep;

#define ExcepHeap	0x0001		// Message was allocated from heap.
#define ExcepMem	0x0002		// Error in memory-allocation function; e.g., malloc().

// Definitions for excep() function.
#define ExNotice	1		// Display "Notice".
#define ExWarning	2		// Display "Warning".
#define ExError		3		// Display "Error".
#define ExAbort		4		// Display "Abort".
#define ExSeverityMask	0x0007		// Severity bit mask.

#define ExErrNo		0x0008		// errno variable contains exception code.
#define ExMessage	0x0010		// plexcep.msg variable contains exception message.
#define	ExCustom	0x0020		// Display custom, formatted message.

#define ExExit		0x0040		// Force exit() call.
#define ExNoExit	0x0080		// Skip exit() call.
#define ExExitMask	(ExExit | ExNoExit)	// Exit flags.

// External variables.
extern ProLibExcep plexcep;

// External function declarations.
extern int emsg(int code,char *msg);
extern int emsge(int code);
extern int emsgf(int code,char *fmt,...);
extern int excep(uint flags,...);
#endif
