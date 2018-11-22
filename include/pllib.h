// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// pllib.h		Header file for miscellaneous library routines.

#ifndef pllib_h
#define pllib_h

#include "pldef.h"

// External function declarations.
extern bool binsearch(char *key,void *table,ssize_t n,int (*cmp)(const char *s1,const char *s2),
 char *(*fetch)(void *table,ssize_t i),ssize_t *indexp);
extern int getdel(char *spec,ushort *delimp);
extern char *intf(long i);
extern uint prime(uint n);
extern int estrtol(const char *s,int base,long *resultp);
extern int estrtoul(const char *s,int base,ulong *resultp);
extern char *sysdate(void);
extern char *systime(void);
#endif
