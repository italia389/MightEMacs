// GeekLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// gexcep.c - Exception data and routines for geek library.

#include "os.h"
#include "gl_def.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

// Global variables.
GeekExcep excep = {0,0,""};

// Free exception message allocated from heap, if applicable.
static void emsgfree(void) {

	if(excep.flags & GEHEAP) {
		(void) free((void *) excep.msg);
		excep.flags &= ~GEHEAP;
		}
	}

// Set an exception code and message, freeing old message if it was allocated from heap.  Return status code.
int emsg(int code,char *msgp) {

	emsgfree();
	excep.msg = msgp;
	return excep.code = code;
	}

// Set an exception code and errno message.  Return status code.
int emsge(int code) {

	return emsg(code,strerror(errno));
	}

// Set an exception code and formatted message, freeing old message if it was allocated from heap.  Return status code.
int emsgf(int code,char *fmtp,...) {
	va_list ap;

	emsgfree();
	va_start(ap,fmtp);
	(void) vasprintf(&excep.msg,fmtp,ap);
	va_end(ap);
	if(excep.msg == NULL) {
		excep.msg = strerror(errno);
		return excep.code = -2;
		}
	excep.flags |= GEHEAP;
	return excep.code = code;
	}
