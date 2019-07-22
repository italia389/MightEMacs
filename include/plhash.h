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
	struct HashRec *next;
	char *key;
	Datum *value;
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
extern void hclear(Hash *htab);
extern int hcmp(const void *t1,const void *t2);
extern Datum *hdelete(Hash *htab,char *key);
extern HashRec *heach(Hash **hashp);
extern void hfree(Hash *htab);
extern Hash *hnew(HashSize hashSize,float loadFactor,float rebuildTrig);
extern int hrename(Hash *htab,char *oldkey,char *newkey,int *resultp);
extern HashRec *hsearch(Hash *htab,char *key);
extern HashRec *hset(Hash *htab,char *key,Datum *datum,bool copy);
extern int hsort(Hash *htab,int (*hcmp)(const void *hrec1,const void *hrec2),HashRec ***hrecpp);
#ifdef HDebug
extern void hstats(Hash *htab);
#endif
#endif
