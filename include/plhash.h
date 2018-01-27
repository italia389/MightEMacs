// ProLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// plhash.h		Header file for hash() routine.

#ifndef plhash_h
#define plhash_h

#include "pldatum.h"

typedef struct HashRec {
	struct HashRec *nextp;
	char *key;
	Datum value;
	} HashRec;
typedef uint HashSize;
typedef struct {
	HashSize hashSize;
	size_t recCount;
	HashRec **table;
	} Hash;

// External function declarations.
extern void hclear(Hash *hp);
extern int hcmp(const void *t1,const void *t2);
extern int hcreate(Hash *hp,char *key,HashRec **hrpp,bool *isNew);
extern bool hdelete(Hash *hp,char *key);
extern HashRec *heach(Hash **hpp);
extern void hfree(Hash *hp);
extern int hnew(Hash **hpp,HashSize hashSize);
extern HashRec *hsearch(Hash *hp,char *key);
#if 0
extern int hashtoarray(Hash *hp,int (*hcmp)(const void *hrp1,const void *hrp2),HashRec ***hrppp);
#else
extern int hsort(Hash *hp,int (*hcmp)(const void *hrp1,const void *hrp2),HashRec ***hrppp);
#endif

#endif
