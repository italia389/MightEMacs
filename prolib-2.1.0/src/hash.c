// (c) Copyright 2019 Richard W. Marinelli
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

// Search for (string) key in hash table.  Return pointer to HashRec if found; otherwise, NULL.  If hrecpp is not NULL, set it
// to hash table entry (slot).
static HashRec *hsrch(Hash *htab,char *key,HashRec ***hrecpp) {
	HashRec *hrec;
	HashRec **hrecp = htab->table + hash(key,htab->hashSize);

	if(hrecpp != NULL)
		*hrecpp = hrecp;

	for(hrec = *hrecp; hrec != NULL; hrec = hrec->next) {
		if(strcmp(hrec->key,key) == 0)
			return hrec;
		}

	return NULL;
	}

// Walk through a hash returning each hash record in sequence, or NULL if none left.  "hashp" is an indirect pointer to the hash
// object and is modified by this routine.
HashRec *heach(Hash **hashp) {
	HashRec *hrec;
	static struct {
		HashRec **hrecp,**hrecpz,*hrec;
		} hstate = {
			NULL,NULL,NULL
			};

	// Validate and initialize control pointers.
	if(*hashp != NULL) {
		if((*hashp)->recCount == 0)
			goto Done;
		hstate.hrecpz = (hstate.hrecp = (*hashp)->table) + (*hashp)->hashSize;
		hstate.hrec = *hstate.hrecp;
		*hashp = NULL;
		}
	else if(hstate.hrecp == NULL)
		return NULL;

	// Get next record and return it, or NULL if none left.
	while(hstate.hrec == NULL) {
		if(++hstate.hrecp == hstate.hrecpz) {
Done:
			hstate.hrecp = NULL;
			return NULL;
			}
		hstate.hrec = *hstate.hrecp;
		}

	hrec = hstate.hrec;
	hstate.hrec = hrec->next;
	return hrec;
	}

// Create or rebuild given hash table.  Return status code.
static int hbuild(Hash *htab) {
	HashSize newHashSize;
	HashRec **newTable;

	// Determine new hash size.
	if(htab->recCount == 0) {
		if(htab->hashSize == 0)
			newHashSize = DefaultHashSize;
		else if((newHashSize = prime(htab->hashSize)) == 0)
			newHashSize = htab->hashSize;
		}
	else if((newHashSize = prime((uint) round(htab->recCount / htab->loadFactor))) == 0)
		return emsgf(-1,"Cannot resize hash table for %lu entries",htab->recCount);

	// Allocate array of NULL hash pointers.
	if((newTable = (HashRec **) calloc(newHashSize,sizeof(HashRec *))) == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}

	// If hash table not empty, rebuild it.
	if(htab->recCount > 0) {
		HashRec **hrecp;
		Hash *hp1 = htab;
		HashRec *hrec = heach(&hp1);
#ifdef HDebug
		fprintf(HDebug,"Rebuilding hash at recCount %lu, hashSize %lu -> %lu...\n",htab->recCount,htab->hashSize,
		 newHashSize);
#endif
		do {
			// Get slot and add record to front of linked list.
			hrecp = newTable + hash(hrec->key,newHashSize);
			hrec->next = *hrecp;
			*hrecp = hrec;
			} while((hrec = heach(&hp1)) != NULL);

		// Release old hash space.
		free((void *) htab->table);
#ifdef HDebug
		fputs("rebuild completed.\n",HDebug);
#endif
		}

	htab->hashSize = newHashSize;
	htab->table = newTable;
	return 0;
	}

// Create hash table and return pointer to it, or NULL if error.  hashSize is the initial size of the table, loadFactor is used
// to calculate the new hash size when the table is built or rebuilt, and rebuildTrig is the minimum load factor which triggers
// a rebuild.  All three parameters are saved in the Hash object.  If hashSize, loadFactor, and/or rebuildTrig is zero, a
// default value is used.
Hash *hnew(HashSize hashSize,float loadFactor,float rebuildTrig) {
	Hash *htab;

	// Create hash record.
	if((htab = (Hash *) malloc(sizeof(Hash))) == NULL) {
		plexcep.flags |= ExcepMem;
		emsge(-1);
		}
	else {
		// Set hash table parameters and do sanity checks.
		htab->loadFactor = (loadFactor == 0.0) ? InitialLoadFactor : loadFactor;
		htab->rebuildTrig = (rebuildTrig == 0.0) ? DefaultRebuildTrigger : rebuildTrig;
		if(htab->loadFactor < 0.0)
			emsgf(-1,"Initial hash table load factor %.2f cannot be less than zero",htab->loadFactor);
		else if(htab->rebuildTrig < 0.0)
			emsgf(-1,"Hash table rebuild trigger %.2f cannot be less than zero",htab->rebuildTrig);
		else if(htab->loadFactor > MaxLoadFactor)
			emsgf(-1,"Initial hash table load factor %.2f cannot be greater than %.2f",htab->loadFactor,
			 MaxLoadFactor);
		else if(htab->rebuildTrig <= htab->loadFactor)
			emsgf(-1,"Hash table rebuild trigger %.2f must be greater than initial load factor %.2f",
			 htab->rebuildTrig,htab->loadFactor);
		else {
			htab->hashSize = hashSize;
			htab->recCount = 0;

			// Let hbuild() do the rest.
			if(hbuild(htab) == 0)
				return htab;
			}
		}

	// Error detected.
	return NULL;
	}

// Save key in given hash record (on heap) and add record to given hash table.  Return status code.
static int hsave(Hash *htab,HashRec **hrecp,HashRec *hrec,char *key) {

	// Save key.
	char *str = (char *) malloc(strlen(key) + 1);
	if(str == NULL) {
		plexcep.flags |= ExcepMem;
		return emsge(-1);
		}
	hrec->key = strcpy(str,key);

	// Add record to front of linked list.
	hrec->next = *hrecp;
	*hrecp = hrec;
	++htab->recCount;

	return 0;
	}

// Store a Datum object (or a copy if "copy" is true) in given hash table, given key, and return pointer to its hash record or
// NULL if error.  If datum is NULL, create and store a nil Datum object.
HashRec *hset(Hash *htab,char *key,Datum *datum,bool copy) {
	HashRec *hrec,**hrecp1;
	bool newEntry = false;

	// Value given?
	if(datum == NULL)
		copy = true;

	// Does key exist?
	if((hrec = hsrch(htab,key,&hrecp1)) == NULL) {

		// Nope, create new record.
		if((hrec = (HashRec *) malloc(sizeof(HashRec))) == NULL) {
			plexcep.flags |= ExcepMem;
			emsge(-1);
			return NULL;
			}

		// Add record to table.
		if((copy && dnew(&hrec->value) != 0) || hsave(htab,hrecp1,hrec,key) != 0)
			return NULL;
		newEntry = true;
		}

	// Yes, clear existing entry.
	else if(copy)
		dclear(hrec->value);
	else
		ddelete(hrec->value);

	// Save value if given.
	if(datum != NULL) {
		if(!copy)
			hrec->value = datum;
		else if(datcpy(hrec->value,datum) != 0)
			return NULL;
		}

	// Return result.  If hash table is too big, rebuild it.
	return (newEntry && (double) htab->recCount / htab->hashSize >= htab->rebuildTrig && hbuild(htab) != 0) ? NULL : hrec;
	}

// Compare keys of two hash records and return result ... for use by qsort().
int hcmp(const void *h1,const void *h2) {

	return strcmp((*(HashRec **) h1)->key,(*(HashRec **) h2)->key);
	}

// Remove hash entry, given key.  If entry does not exist, return NULL; otherwise, delete key, remove entry from table, and
// return hash record (which will have invalid "key" and "next" members).
static HashRec *hremove(Hash *htab,char *key) {
	HashRec *hrec0,*hrec1;
	HashRec **hrecp0 = htab->table + hash(key,htab->hashSize);

	hrec0 = NULL;
	for(hrec1 = *hrecp0; hrec1 != NULL; hrec1 = hrec1->next) {
		if(strcmp(hrec1->key,key) == 0) {

			// Found entry.  Remove it from linked list.
			if(hrec0 == NULL)
				*hrecp0 = hrec1->next;
			else
				hrec0->next = hrec1->next;

			// Clear key and return record.
			free((void *) hrec1->key);
			--htab->recCount;
			return hrec1;
			}
		hrec0 = hrec1;
		}

	// Not found.
	return NULL;
	}

// Delete hash entry, given key, and return its Datum object or NULL if entry does not exist.
Datum *hdelete(Hash *htab,char *key) {
	HashRec *hrec = hremove(htab,key);
	if(hrec == NULL)
		return NULL;
	Datum *datum = hrec->value;
	free((void *) hrec);
	return datum;
	}

// Rename hash entry, given old and new keys.  If resultp not NULL, set *resultp to zero if entry was renamed; otherwise, set to
// -1 if old key does not exist or 1 if new key already exists.  Return status code.
int hrename(Hash *htab,char *oldkey,char *newkey,int *resultp) {
	HashRec *hrec,**hrecp;
	int result;
	int rc = 0;

	// Does new key already exist?
	if(hsrch(htab,newkey,&hrecp) != NULL)
		result = 1;

	// Delete old key if it exists.
	else if((hrec = hremove(htab,oldkey)) == NULL)
		result = -1;

	// Old key was found (and deleted).  Add new key.
	else {
		result = 0;
		rc = hsave(htab,hrecp,hrec,newkey);
		}

	if(resultp != NULL)
		*resultp = result;
	return rc;
	}

// Clear given hash table.
void hclear(Hash *htab) {
	HashRec *hrec0,*hrec1,**hrecp,**hrecpz;

	hrecpz = (hrecp = htab->table) + htab->hashSize;
	do {
		if(*hrecp != NULL) {
			hrec0 = *hrecp;
			do {
				hrec1 = hrec0->next;
				ddelete(hrec0->value);
				free((void *) hrec0->key);
				free((void *) hrec0);
				} while((hrec0 = hrec1) != NULL);
			*hrecp = NULL;
			}
		} while(++hrecp < hrecpz);
	htab->recCount = 0;
	}

// Free given hash table.
void hfree(Hash *htab) {

	hclear(htab);
	free((void *) htab->table);
	free((void *) htab);
	}

// Search for key in hash table.  Return pointer to HashRec if found; otherwise, NULL.
HashRec *hsearch(Hash *htab,char *key) {

	return hsrch(htab,key,NULL);
	}

// Sort given hash and return result as an array of record pointers in *hrecpp.  "hcmp" is the entry comparison routine to pass
// to qsort().  If *hrecpp is not NULL, it is assumed to be a pointer to an array of htab->recCount elements, which will be used
// for the result.  Otherwise, an array will be allocated on the heap (if the hash is not empty) and returned in *hrecpp.  The
// pointer to the array should be passed to free() to release the allocated storage when it is no longer needed.  In either
// case, if the hash is empty, *hrecpp is set to NULL.  Return status code.
int hsort(Hash *htab,int (*hcmp)(const void *hrec1,const void *hrec2),HashRec ***hrecpp) {

	if(htab->recCount == 0)
		*hrecpp = NULL;
	else {
		Hash *hp1;
		HashRec **dest,**dest0,*hrec;

		// Allocate array if needed and copy pointers.
		if(*hrecpp != NULL)
			dest0 = *hrecpp;
		else if((dest0 = (HashRec **) malloc(sizeof(HashRec *) * htab->recCount)) == NULL) {
			plexcep.flags |= ExcepMem;
			return emsge(-1);
			}
		hp1 = htab;
		dest = dest0;
		while((hrec = heach(&hp1)) != NULL)
			*dest++ = hrec;

		// Sort array and return result.
		qsort((void *) dest0,htab->recCount,sizeof(*dest0),hcmp);
		*hrecpp = dest0;
		}

	return 0;
	}

#ifdef HDebug
void hstats(Hash *htab) {

	fprintf(HDebug,
	 "\n### HASH STATISTICS\n%20s %lu\n%20s %lu\n%20s %.2f\n%20s %.2f\n","Size:",htab->hashSize,"Entries:",htab->recCount,
	 "Initial load factor:",htab->loadFactor,"Rebuild trigger:",htab->rebuildTrig);
	if(htab->recCount > 0) {
		HashRec *hrec,**hrecp = htab->table,**hrecpz = hrecp + htab->hashSize;
		size_t count,maxSlot = 0,slotCounts[10] = {0,0,0,0,0,0,0,0,0,0};
		do {
			count = 0;
			for(hrec = *hrecp++; hrec != NULL; hrec = hrec->next)
				++count;
			if(count > maxSlot)
				maxSlot = count;
			if(count > 9)
				count = 9;
			++slotCounts[count];
			} while(hrecp < hrecpz);
		fprintf(HDebug,"%20s %lu\n%20s %.2f\n%20s\n","Max slot size:",maxSlot,"Current load factor:",
		 (float) htab->recCount / htab->hashSize,"Slot counts:");
		count = 0;
		do {
			fprintf(HDebug,"%19lu: %lu\n",count,slotCounts[count]);
			} while(++count < 10);
		}
	}
#endif

