// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// lang.h	Language-specific definitions for MightEMacs.

// English is the only language currently supported.  However, others can be if english.h is translated to a new header
// file; for example, "spanish.h".
#if English
#include "english.h"
#define Language	"English"
//#elif Spanish
//#include "spanish.h"
//#define Language	"Spanish"
#else
#error 'English macro not defined or not set to true'
#endif
