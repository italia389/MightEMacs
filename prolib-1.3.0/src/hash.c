// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// hash.c		Hash table routines.

#define HDebug			0	// Debugging mode.

#include "os.h"
#include "plexcep.h"
#include "pllib.h"
#if HDebug
#include <stdio.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "plhash.h"

#define DefaultHashSize		97	// Starting hash size if not specified by caller.
#define BeginSlotSize		3	// Initial number of entries in a linked-list slot (when rebuilt).
#define MaxSlotSize		20	// Average maximum number of entries in a linked-list slot (which triggers a rebuild).

// Hash a key and return integer result.
static HashSize hash(char *key,HashSize hashSize) {
	size_t k;

	k = 0;
	while(*key != '\0')
		k = (k << 2) + *key++;
	return k % hashSize;
	}

// Search for (string) key in hash table.  Return pointer to HashRec if found; otherwise, NULL.  If tablep is not NULL, set it
// to hash table entry (slot).
static HashRec *hsrch(Hash *hp,char *key,HashRec ***tablep) {
	HashRec *hrp;
	HashRec **hrpp = hp->table + hash(key,hp->hashSize);

	if(tablep != NULL)
		*tablep = hrpp;

	for(hrp = *hrpp; hrp != NULL; hrp = hrp->nextp) {
		if(strcmp(hrp->key,key) == 0)
			return hrp;
		}

	return NULL;
	}

// Walk through a hash returning each hash record in sequence, or NULL if none left.  "hpp" is an indirect pointer to the hash
// object and is modified by this routine.
HashRec *heach(Hash **hpp) {
	HashRec *hrp;
	static struct {
		HashRec **hrpp,**hrppz,*hrp;
		} hstate = {
			NULL,NULL,NULL
			};

	// Validate and initialize control pointers.
	if(*hpp != NULL) {
		if((*hpp)->recCount == 0)
			goto Done;
		hstate.hrppz = (hstate.hrpp = (*hpp)->table) + (*hpp)->hashSize;
		hstate.hrp = *hstate.hrpp;
		*hpp = NULL;
		}
	else if(hstate.hrpp == NULL)
		return NULL;

	// Get next record and return it, or NULL if none left.
	while(hstate.hrp == NULL) {
		if(++hstate.hrpp == hstate.hrppz) {
Done:
			hstate.hrpp = NULL;
			return NULL;
			}
		hstate.hrp = *hstate.hrpp;
		}

	hrp = hstate.hrp;
	hstate.hrp = hrp->nextp;
	return hrp;
	}

// Create or rebuild given hash table.  Return status code.
static int hbuild(Hash *hp) {
	HashSize newHashSize;
	HashRec **newTable;

	// Determine new hash size.
	if(hp->recCount == 0)
		newHashSize = (hp->hashSize == 0) ? DefaultHashSize : hp->hashSize;
	else if((newHashSize = prime(hp->recCount / BeginSlotSize)) == 0)
		return emsgf(-1,"Cannot resize hash table for %lu entries",hp->recCount);

	// Allocate array of (NULL) hash pointers.
	if((newTable = (HashRec **) calloc(newHashSize,sizeof(HashRec *))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}

	// If existing table, rebuild it.
	if(hp->recCount > 0) {
		HashRec **hrpp;
		Hash *hp1 = hp;
		HashRec *hrp = heach(&hp1);
#if HDebug
		fprintf(stderr,"Rebuilding hash at recCount %lu, hashSize %u -> %u...\n",hp->recCount,hp->hashSize,newHashSize);
#endif
		do {
			// Get slot and add record to front of linked list.
			hrpp = newTable + hash(hrp->key,newHashSize);
			hrp->nextp = *hrpp;
			*hrpp = hrp;
			} while((hrp = heach(&hp1)) != NULL);

		// Release old hash space.
		free((void *) hp->table);
		}

	hp->hashSize = newHashSize;
	hp->table = newTable;
	return 0;
	}

// Create hash table and set *hpp to it.  Return status code.
int hnew(Hash **hpp,HashSize hashSize) {
	Hash *hp;

	// Create hash record.
	if((hp = (Hash *) malloc(sizeof(Hash))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	hp->hashSize = hashSize;
	hp->recCount = 0;
	*hpp = hp;

	// Let hbuild() do the rest.
	return hbuild(hp);
	}

// Find existing hash table entry or create a nil entry, given key, and return result in *hrpp (if hrpp not NULL).  If "isNew"
// is not NULL, set *isNew to true if an entry was created; otherwise, false.  Return status code.
int hcreate(Hash *hp,char *key,HashRec **hrpp,bool *isNew) {
	HashRec *hrp,**hrpp1;
	bool newEntry = false;

	// Does key exist?
	if((hrp = hsrch(hp,key,&hrpp1)) == NULL) {
		char *str;

		// Nope, create new record.
		if((hrp = (HashRec *) malloc(sizeof(HashRec))) == NULL) {
			plexcep.flags |= ExcepMem;
			return emsge(-1);
			}
		dinit(&hrp->value);
		dsetnil(&hrp->value);

		// Save key.
		str = (char *) malloc(strlen(key) + 1);
		if(str == NULL) {
			plexcep.flags |= ExcepMem;
			return emsge(-1);
			}
		hrp->key = strcpy(str,key);

		// Add record to front of linked list.
		hrp->nextp = *hrpp1;
		*hrpp1 = hrp;
		++hp->recCount;
		newEntry = true;
		}

	// Return results.
	if(hrpp != NULL)
		*hrpp = hrp;
	if(isNew != NULL)
		*isNew = newEntry;

	// If hash table is too big, rebuild it.
	return (newEntry && hp->recCount > hp->hashSize * MaxSlotSize) ? hbuild(hp) : 0;
	}

// Compare keys of two hash records and return result ... for use by qsort().
int hcmp(const void *h1,const void *h2) {

	return strcmp((*(HashRec **) h1)->key,(*(HashRec **) h2)->key);
	}

// Clear and delete given hash entry.
static void hnuke(HashRec *hrp) {

	dsetnil(&hrp->value);
	free((void *) hrp->key);
	free((void *) hrp);
	}

// Delete hash entry, given key.  Return true if successful; otherwise, false.
bool hdelete(Hash *hp,char *key) {
	HashRec *hrp0,*hrp1;
	HashRec **hrpp = hp->table + hash(key,hp->hashSize);

	hrp0 = NULL;
	for(hrp1 = *hrpp; hrp1 != NULL; hrp1 = hrp1->nextp) {
		if(strcmp(hrp1->key,key) == 0) {

			// Found entry.  Remove it from linked list.
			if(hrp0 == NULL)
				*hrpp = hrp1->nextp;
			else
				hrp0->nextp = hrp1->nextp;

			// Clear it and delete it.
			hnuke(hrp1);
			--hp->recCount;
			return true;
			}
		hrp0 = hrp1;
		}

	// Not found.
	return false;
	}

// Clear given hash table.
void hclear(Hash *hp) {
	HashRec *hrp0,*hrp1,**hrpp,**hrppz;

	hrppz = (hrpp = hp->table) + hp->hashSize;
	do {
		if(*hrpp != NULL) {
			hrp0 = *hrpp;
			do {
				hrp1 = hrp0->nextp;
				hnuke(hrp0);
				} while((hrp0 = hrp1) != NULL);
			*hrpp = NULL;
			}
		} while(++hrpp < hrppz);
	hp->recCount = 0;
	}

// Free given hash table.
void hfree(Hash *hp) {

	hclear(hp);
	free((void *) hp->table);
	free((void *) hp);
	}

// Search for key in hash table.  Return pointer to HashRec if found; otherwise, NULL.
HashRec *hsearch(Hash *hp,char *key) {

	return hsrch(hp,key,NULL);
	}

#if 0
// Create array of hash record pointers, sort it via hcmp() if not NULL, and return it in *hrppp.  If hash is empty, set *hrppp
// to NULL.  Return status code.
int hashtoarray(Hash *hp,int (*hcmp)(const void *hrp1,const void *hrp2),HashRec ***hrppp) {
#else
// Sort given hash using "hcmp" routine and return result as an array of record pointers in *hrppp (containing hp->recCount
// elements).  The pointer to the array should be passed to free() to release the allocated storage when it is no longer needed.
// If the hash is empty, *hrppp is set to NULL.  Return status code.
int hsort(Hash *hp,int (*hcmp)(const void *hrp1,const void *hrp2),HashRec ***hrppp) {
#endif
	if(hp->recCount == 0)
		*hrppp = NULL;
	else {
		Hash *hp1;
		HashRec **destp,**destp0,*hrp;

		// Allocate array and copy pointers.
		if((destp0 = destp = (HashRec **) malloc(sizeof(HashRec *) * hp->recCount)) == NULL) {
			plexcep.flags |= ExcepMem;
			return emsge(-1);
			}
		hp1 = hp;
		while((hrp = heach(&hp1)) != NULL)
			*destp++ = hrp;

#if 0
		// Sort, if applicable.
		if(hcmp != NULL)
#else
		// Sort array.
#endif
			qsort((void *) destp0,hp->recCount,sizeof(*destp0),hcmp);

		// Return result.
		*hrppp = destp0;
		}

	return 0;
	}
