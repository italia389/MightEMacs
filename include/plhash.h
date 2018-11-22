// ProLib (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// plhash.h		Header file for hash() routine.

#ifndef plhash_h
#define plhash_h

//#define HDebug			stderr	// Debugging mode.
//#define HDebug			logfile	// Debugging mode.

#include "pldatum.h"

typedef struct HashRec {
	struct HashRec *nextp;
	char *key;
	Datum *valuep;
	} HashRec;
typedef size_t HashSize;
typedef struct {
	HashRec **table;		// Hash array.
	HashSize hashSize;		// Size of array -- number of slots (zero for default).
	size_t recCount;		// Current number of entries in table.
	float loadFactor;		// Initial load factor to use when table is built or rebuilt (zero for default).
	float rebuildTrig;		// Minimum load factor which triggers a rebuild (zero for default).
	} Hash;

// External function declarations.
extern void hclear(Hash *hp);
extern int hcmp(const void *t1,const void *t2);
extern Datum *hdelete(Hash *hp,char *key);
extern HashRec *heach(Hash **hpp);
extern void hfree(Hash *hp);
extern Hash *hnew(HashSize hashSize,float loadFactor,float rebuildTrig);
extern int hrename(Hash *hp,char *oldkey,char *newkey,int *resultp);
extern HashRec *hsearch(Hash *hp,char *key);
extern HashRec *hset(Hash *hp,char *key,Datum *datp,bool copy);
extern int hsort(Hash *hp,int (*hcmp)(const void *hrp1,const void *hrp2),HashRec ***hrppp);
#ifdef HDebug
extern void hstats(Hash *hp);
#endif
#endif
