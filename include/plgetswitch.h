// ProLib (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// plgetswitch.h	Header file for getswitch() routine.

#ifndef plgetswitch_h
#define plgetswitch_h

#include "pldef.h"

// Switch objects.
typedef struct {
	char **namev;				// Pointer to array of switch names.
	ushort flags;				// Descriptor flags.
	} Switch;
typedef struct {
	char *name;				// Pointer to source switch name.
	char *value;				// Pointer to switch argument if found; otherwise, NULL.
	} SwitchResult;
typedef struct {
	Switch *sw;				// Pointer to switch descriptor in switch table.
	uint foundCount;			// Number of occurrences found.
	} SwitchState;

// Switch descriptor flags.
#define SF_NumericSwitch	0x0001		// Numeric switch; otherwise, standard.
#define SF_RequiredSwitch	0x0002		// Required switch; otherwise, optional.
#define SF_PlusType		0x0004		// Numeric switch is +n type; otherwise, -n (does not accept an argument).
#define SF_NoArg		(1 << 2)	// Switch does not accept an argument.
#define SF_OptionalArg		(2 << 2)	// Switch may accept an argument.
#define SF_RequiredArg		(3 << 2)	// Switch requires an argument.
#define SF_ArgMask		(0x3 << 2)	// Mask for argument type.

#define SF_NumericArg		0x0010		// Switch argument must be a number.
#define SF_AllowSign		0x0020		// Numeric switch argument may have a leading sign (+ or -).
#define SF_AllowDecimal		0x0040		// Numeric switch argument may contain a decimal point.
#define SF_AllowRepeat		0x0080		// Switch may be repeated.
#define SF_AllowNullArg		0x0100		// Switch argument may be null.

#define NSMinusKey		"-zzz-"		// Dummy hash key to use for - numeric switch.
#define NSPlusKey		"+zzz+"		// Dummy hash key to use for + numeric switch.

// External function declarations.
extern int getswitch(int *argcp,char ***argvp,Switch **swp,SwitchResult *resultp);
#endif
