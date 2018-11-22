// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// hash.c		Hash table routines.

#include "os.h"
#include "plexcep.h"
#include "pllib.h"
#include "plhash.h"
#ifdef HDebug
#include <stdio.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

// Default hash table parameters.  "Load factor" is n/k where "n" is number of entries, and "k" is number of slots (hash size).
// The hnew() function mandates that the initial load factor be <= 1.0 and the rebuild trigger be greater than the initial load
// factor, to avoid a table rebuild after every insert.
#define DefaultHashSize		67	// Starting hash size if not specified by caller.
#define InitialLoadFactor	0.5	// Initial load factor if not specified by caller (when table built or rebuilt).
#define MaxLoadFactor		1.0	// Maximum allowed value of initial load factor.
#define DefaultRebuildTrigger	1.65	// Default minimum load factor which triggers a rebuild.

#ifdef HDebug
extern FILE *HDebug;
#endif

// Hash a key and return integer result.
static HashSize hash(char *key,HashSize hashSize) {
	size_t k = 0;
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
	if(hp->recCount == 0) {
		if(hp->hashSize == 0)
			newHashSize = DefaultHashSize;
		else if((newHashSize = prime(hp->hashSize)) == 0)
			newHashSize = hp->hashSize;
		}
	else if((newHashSize = prime((uint) round(hp->recCount / hp->loadFactor))) == 0)
		return emsgf(-1,"Cannot resize hash table for %lu entries",hp->recCount);

	// Allocate array of NULL hash pointers.
	if((newTable = (HashRec **) calloc(newHashSize,sizeof(HashRec *))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}

	// If hash table not empty, rebuild it.
	if(hp->recCount > 0) {
		HashRec **hrpp;
		Hash *hp1 = hp;
		HashRec *hrp = heach(&hp1);
#ifdef HDebug
		fprintf(HDebug,"Rebuilding hash at recCount %lu, hashSize %lu -> %lu...\n",hp->recCount,hp->hashSize,
		 newHashSize);
#endif
		do {
			// Get slot and add record to front of linked list.
			hrpp = newTable + hash(hrp->key,newHashSize);
			hrp->nextp = *hrpp;
			*hrpp = hrp;
			} while((hrp = heach(&hp1)) != NULL);

		// Release old hash space.
		free((void *) hp->table);
#ifdef HDebug
		fputs("rebuild completed.\n",HDebug);
#endif
		}

	hp->hashSize = newHashSize;
	hp->table = newTable;
	return 0;
	}

// Create hash table and return pointer to it, or NULL if error.  hashSize is the initial size of the table, loadFactor is used
// to calculate the new hash size when the table is built or rebuilt, and rebuildTrig is the minimum load factor which triggers
// a rebuild.  All three parameters are saved in the Hash object.  If hashSize, loadFactor, and/or rebuildTrig is zero, a
// default value is used.
Hash *hnew(HashSize hashSize,float loadFactor,float rebuildTrig) {
	Hash *hp;

	// Create hash record.
	if((hp = (Hash *) malloc(sizeof(Hash))) == NULL) {
		plexcep.flags |= ExcepMem;
		emsge(-1);
		}
	else {
		// Set hash table parameters and do sanity checks.
		hp->loadFactor = (loadFactor == 0.0) ? InitialLoadFactor : loadFactor;
		hp->rebuildTrig = (rebuildTrig == 0.0) ? DefaultRebuildTrigger : rebuildTrig;
		if(hp->loadFactor < 0.0)
			emsgf(-1,"Initial hash table load factor %.2f cannot be less than zero",hp->loadFactor);
		else if(hp->rebuildTrig < 0.0)
			emsgf(-1,"Hash table rebuild trigger %.2f cannot be less than zero",hp->rebuildTrig);
		else if(hp->loadFactor > MaxLoadFactor)
			emsgf(-1,"Initial hash table load factor %.2f cannot be greater than %.2f",hp->loadFactor,
			 MaxLoadFactor);
		else if(hp->rebuildTrig <= hp->loadFactor)
			emsgf(-1,"Hash table rebuild trigger %.2f must be greater than initial load factor %.2f",
			 hp->rebuildTrig,hp->loadFactor);
		else {
			hp->hashSize = hashSize;
			hp->recCount = 0;

			// Let hbuild() do the rest.
			if(hbuild(hp) == 0)
				return hp;
			}
		}

	// Error detected.
	return NULL;
	}

// Save key in given hash record (on heap) and add record to given hash table.  Return status code.
static int hsave(Hash *hp,HashRec **hrpp,HashRec *hrp,char *key) {

	// Save key.
	char *str = (char *) malloc(strlen(key) + 1);
	if(str == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	hrp->key = strcpy(str,key);

	// Add record to front of linked list.
	hrp->nextp = *hrpp;
	*hrpp = hrp;
	++hp->recCount;

	return 0;
	}

// Store a Datum object (or a copy if "copy" is true) in given hash table, given key, and return pointer to its hash record or
// NULL if error.  If datp is NULL, create and store a nil Datum object.
HashRec *hset(Hash *hp,char *key,Datum *datp,bool copy) {
	HashRec *hrp,**hrpp1;
	bool newEntry = false;

	// Value given?
	if(datp == NULL)
		copy = true;

	// Does key exist?
	if((hrp = hsrch(hp,key,&hrpp1)) == NULL) {

		// Nope, create new record.
		if((hrp = (HashRec *) malloc(sizeof(HashRec))) == NULL) {
			plexcep.flags |= ExcepMem;
			emsge(-1);
			return NULL;
			}

		// Add record to table.
		if((copy && dnew(&hrp->valuep) != 0) || hsave(hp,hrpp1,hrp,key) != 0)
			return NULL;
		newEntry = true;
		}

	// Yes, clear existing entry.
	else if(copy)
		dclear(hrp->valuep);
	else
		ddelete(hrp->valuep);

	// Save value if given.
	if(datp != NULL) {
		if(!copy)
			hrp->valuep = datp;
		else if(datcpy(hrp->valuep,datp) != 0)
			return NULL;
		}

	// Return result.  If hash table is too big, rebuild it.
	return (newEntry && (double) hp->recCount / hp->hashSize >= hp->rebuildTrig && hbuild(hp) != 0) ? NULL : hrp;
	}

// Compare keys of two hash records and return result ... for use by qsort().
int hcmp(const void *h1,const void *h2) {

	return strcmp((*(HashRec **) h1)->key,(*(HashRec **) h2)->key);
	}

// Remove hash entry, given key.  If entry does not exist, return NULL; otherwise, delete key, remove entry from table, and
// return hash record (which will have invalid "key" and "nextp" members).
static HashRec *hremove(Hash *hp,char *key) {
	HashRec *hrp0,*hrp1;
	HashRec **hrpp0 = hp->table + hash(key,hp->hashSize);

	hrp0 = NULL;
	for(hrp1 = *hrpp0; hrp1 != NULL; hrp1 = hrp1->nextp) {
		if(strcmp(hrp1->key,key) == 0) {

			// Found entry.  Remove it from linked list.
			if(hrp0 == NULL)
				*hrpp0 = hrp1->nextp;
			else
				hrp0->nextp = hrp1->nextp;

			// Clear key and return record.
			free((void *) hrp1->key);
			--hp->recCount;
			return hrp1;
			}
		hrp0 = hrp1;
		}

	// Not found.
	return NULL;
	}

// Delete hash entry, given key, and return its Datum object or NULL if entry does not exist.
Datum *hdelete(Hash *hp,char *key) {
	HashRec *hrp = hremove(hp,key);
	if(hrp == NULL)
		return NULL;
	Datum *datp = hrp->valuep;
	free((void *) hrp);
	return datp;
	}

// Rename hash entry, given old and new keys.  If resultp not NULL, set *resultp to zero if entry was renamed; otherwise, set to
// -1 if old key does not exist or 1 if new key already exists.  Return status code.
int hrename(Hash *hp,char *oldkey,char *newkey,int *resultp) {
	HashRec *hrp,**hrpp;
	int result;
	int rc = 0;

	// Does new key already exist?
	if(hsrch(hp,newkey,&hrpp) != NULL)
		result = 1;

	// Delete old key if it exists.
	else if((hrp = hremove(hp,oldkey)) == NULL)
		result = -1;

	// Old key was found (and deleted).  Add new key.
	else {
		result = 0;
		rc = hsave(hp,hrpp,hrp,newkey);
		}

	if(resultp != NULL)
		*resultp = result;
	return rc;
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
				ddelete(hrp0->valuep);
				free((void *) hrp0->key);
				free((void *) hrp0);
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

// Sort given hash and return result as an array of record pointers in *hrppp.  "hcmp" is the entry comparison routine to pass
// to qsort().  If *hrppp is not NULL, it is assumed to be a pointer to an array of hp->recCount elements, which will be used
// for the result.  Otherwise, an array will be allocated on the heap (if the hash is not empty) and returned in *hrppp.  The
// pointer to the array should be passed to free() to release the allocated storage when it is no longer needed.  In either
// case, if the hash is empty, *hrppp is set to NULL.  Return status code.
int hsort(Hash *hp,int (*hcmp)(const void *hrp1,const void *hrp2),HashRec ***hrppp) {

	if(hp->recCount == 0)
		*hrppp = NULL;
	else {
		Hash *hp1;
		HashRec **destp,**destp0,*hrp;

		// Allocate array if needed and copy pointers.
		if(*hrppp != NULL)
			destp0 = *hrppp;
		else if((destp0 = (HashRec **) malloc(sizeof(HashRec *) * hp->recCount)) == NULL) {
			plexcep.flags |= ExcepMem;
			return emsge(-1);
			}
		hp1 = hp;
		destp = destp0;
		while((hrp = heach(&hp1)) != NULL)
			*destp++ = hrp;

		// Sort array and return result.
		qsort((void *) destp0,hp->recCount,sizeof(*destp0),hcmp);
		*hrppp = destp0;
		}

	return 0;
	}

#ifdef HDebug
void hstats(Hash *hp) {

	fprintf(HDebug,
	 "\n### HASH STATISTICS\n%20s %lu\n%20s %lu\n%20s %.2f\n%20s %.2f\n","Size:",hp->hashSize,"Entries:",hp->recCount,
	 "Initial load factor:",hp->loadFactor,"Rebuild trigger:",hp->rebuildTrig);
	if(hp->recCount > 0) {
		HashRec *hrp,**hrpp = hp->table,**hrppz = hrpp + hp->hashSize;
		size_t count,maxSlot = 0,slotCounts[10] = {0,0,0,0,0,0,0,0,0,0};
		do {
			count = 0;
			for(hrp = *hrpp++; hrp != NULL; hrp = hrp->nextp)
				++count;
			if(count > maxSlot)
				maxSlot = count;
			if(count > 9)
				count = 9;
			++slotCounts[count];
			} while(hrpp < hrppz);
		fprintf(HDebug,"%20s %lu\n%20s %.2f\n%20s\n","Max slot size:",maxSlot,"Current load factor:",
		 (float) hp->recCount / hp->hashSize,"Slot counts:");
		count = 0;
		do {
			fprintf(HDebug,"%19lu: %lu\n",count,slotCounts[count]);
			} while(++count < 10);
		}
	}
#endif

