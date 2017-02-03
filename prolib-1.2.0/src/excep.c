// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// excep.c		Core exception data and routines.

#include "os.h"
#include "plexcep.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#define	DefaultExitCode	-1		// Value to use for exit() call if not available elsewhere.

// Global variables.
ProLibExcep plexcep = {0,0,""};

// Free exception message allocated from heap, if applicable.
static void emsgfree(void) {

	if(plexcep.flags & ExcepHeap) {
		(void) free((void *) plexcep.msg);
		plexcep.flags &= ~ExcepHeap;
		}
	}

// Set an exception code and message, freeing old message if it was allocated from heap.  Return status code.
int emsg(int code,char *msg) {

	emsgfree();
	plexcep.msg = msg;
	return plexcep.code = code;
	}

// Set an exception code and errno message.  Return status code.
int emsge(int code) {

	return emsg(code,strerror(errno));
	}

// Set an exception code and formatted message, freeing old message if it was allocated from heap.  Return status code.
int emsgf(int code,char *fmt,...) {
	va_list ap;

	emsgfree();
	va_start(ap,fmt);
	(void) vasprintf(&plexcep.msg,fmt,ap);
	va_end(ap);
	if(plexcep.msg == NULL) {
		plexcep.msg = strerror(errno);
		return plexcep.code = -1;
		}
	plexcep.flags |= ExcepHeap;
	return plexcep.code = code;
	}

// Handle program exception; routine's behavior is controlled by supplied flag word.  Flag bit masks are:
//
//   ExAbort		Abort message, call exit().
//   ExError		Error message, call exit().
//   ExWarning		Warning message, do not call exit(), return non-zero.
//   ExNotice		Notice message, do not call exit(), return non-zero.
//
//   ExErrNo		errno variable contains exception code.
//   ExMessage		plexcep.msg variable contains exception message.
//   ExCustom		Formatted message (no prefix), do not call exit(), return non-zero.
//
//   ExExit		Force exit() call.
//   ExNoExit		Skip exit() call, return non-zero.
//
// Notes:
//  1. Flags in severity and exit groups are mutally exclusive.
//  2. Routine builds and prints exception message to standard error and, optionally, exits.
//  3. Arguments are processed as follows:
//	1. Print message prefix per severity flag in first argument, if specified.
//	2. If ExMessage flag is set, print plexcep.msg; otherwise, if ExErrNo flag is set, print strerror(errno) message.
//	3. If ExErrNo flag is set and errno is non-zero, use errno for exit code; otherwise, use DefaultExitCode.
//	4. If ExCustom flag is set, call printf with remaining arguments.
//	5. Call exit, if applicable; otherwise, return DefaultExitCode.
int excep(uint flags,...) {
	va_list ap;
	static struct {
		bool callExit;
		char *severityPrefix;
		} severityTable[] = {
			{false,NULL},
			{false,"Notice"},
			{false,"Warning"},
			{true,"Error"},
			{true,"Abort"}
			};
	int exitCode;
	uint f;
	bool callExit,needComma = false;

	va_start(ap,flags);

	// Print severity prefix and set default exit action.
	if((f = flags & ExSeverityMask))
		fprintf(stderr,"%s: ",severityTable[f].severityPrefix);
	callExit = severityTable[f].callExit;

	// Check geek message and errno and print message.
	if(flags & (ExMessage | ExErrNo)) {
		fputs((flags & ExMessage) ? plexcep.msg : strerror(errno),stderr);
		needComma = true;
		}

	// Check errno and set exit code.
	exitCode = ((flags & ExErrNo) && errno) ? errno : DefaultExitCode;

	// Print formatted message, if requested.
	if(flags & ExCustom) {
		char *fmt;

		if(needComma)
			fputs(", ",stderr);
		fmt = va_arg(ap,char *);
		vfprintf(stderr,fmt,ap);
		}

	// Finish up.
	fputc('\n',stderr);
	va_end(ap);

	// Call exit(), if applicable.
	switch(flags & ExExitMask) {
	case ExExit:
		callExit = true;
		break;
	case ExNoExit:
		callExit = false;
	}
	if(callExit)
		exit(exitCode);

	return DefaultExitCode;
	}
