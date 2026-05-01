// (c) Copyright 2026 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// arrayhash.c		Array and hash routines for MightEMacs.

#include "std.h"
#include "bind.h"
#include "exec.h"
#include "var.h"

/*** Local declarations ***/

static void
	*ahTrackHead = NULL,		// Head of array and hash tracking ("garbage collection") list.
	*ahTrackTail = NULL;		// Tail of array and hash tracking ("garbage collection") list.

// Rebuild hash node array in given OrdHash object if needed and set *ppAry to beginning.  Return status code.
int ohAryPrep(OrdHash *pOrdHash, HashNode ***ppAry) {

	if(pOrdHash->flags & OH_Changed) {
		if(pOrdHash->pAry != NULL) {
			free((void *) pOrdHash->pAry);
			pOrdHash->pAry = NULL;
			}
		if(hsort(&pOrdHash->table, hcmp, &pOrdHash->pAry) != 0)
			return -1;
		pOrdHash->flags &= ~OH_Changed;
		}
	*ppAry = pOrdHash->pAry;
	return 0;
	}

// Check if return code from a CXL library datum-conversion function is greater than zero (indicating endless recursion during
// an array-to-string or hash-to-string conversion) and set an error message if so.  Return status.
int endless(int rtnCode) {

	return rtnCode > 0 ? rsset(Failure, RSNoFormat, text195) : sess.rtn.status;
		// "Endless recursion detected (array or hash contains itself)"
	}

#if MMDebug & Debug_AryHshLog
// Write Datum information to log file.
static int datUsage(Datum *pDatum, Datum *pWork) {

	if(dtyparray(pDatum)) {
		Array *pArray = dgetarrayptr(pDatum);
		fprintf(logfile, "ARRAY [%.8x], size %ld\n", (uint) pArray, pArray->used);
		}
	else if(dtyphash(pDatum)) {
		Hash *pHash = dgethashptr(pDatum);
		fprintf(logfile, "HASH [%.8x], size %lu\n", (uint) pHash, pHash->nodeCount);
		}
	else if(dtos(pWork, pDatum, NULL, CvtEscChar | CvtQuote | CvtShowNil) < 0)
		return libfail();
	else
		fprintf(logfile, "%s\n", pWork->str);
	return sess.rtn.status;
	}

// Write array-hash-usage report to log file.
static int ahUsage(int step) {

	if(ahTrackHead != NULL) {
		Array *pTrack = ahTrackHead;
		ssize_t i;
		Datum datum;

		dinit(&datum);
		if(step > 1)
			fputc('\n', logfile);
		fprintf(logfile, "ahfree(): Array/Hash List: STEP %d\n\n", step);
		do {
			fprintf(logfile, "\tFound %s %.8x\n\t uflags: %.4hd\n", pTrack->uflags & DHashMask ? "hash" : "array",
			 (uint) pTrack, pTrack->uflags);
			if(pTrack->uflags & DArrayMask) {
				if(pTrack->elements == NULL)
					fputs("\t\tEMPTY\n", logfile);
				else {
					Datum **ppArrayEl, **ppArrayElEnd;

					ppArrayElEnd = (ppArrayEl = pTrack->elements) + pTrack->used;
					i = 0;
					while(ppArrayEl < ppArrayElEnd) {
						fprintf(logfile, "\t\t%2ld [%.4hx]: ", i++, (*ppArrayEl)->type);
						if(datUsage(*ppArrayEl, &datum) != Success)
							goto Retn;
						++ppArrayEl;
						}
					}
				}
			else {
				if(((Hash *) pTrack)->nodeCount == 0)
					fputs("\t\tEMPTY\n", logfile);
				else {
					HashNode *pNode, **pAry, **pAryEnd;
					Hash *pHash = (Hash *) pTrack;
					pAryEnd = (pAry = pHash->pAry) + pHash->size;
					do {
						for(pNode = *pAry; pNode != NULL; pNode = pNode->next) {
							fprintf(logfile, "\t\t%16s [%.4hx]: ", pNode->key, pNode->pVal->type);
							if(datUsage(pNode->pVal, &datum) != Success)
								goto Retn;
							}
						} while(++pAry < pAryEnd);
					}
				}
			} while((pTrack = pTrack->next) != NULL);
		dclear(&datum);
		}
Retn:
	return sess.rtn.status;
	}
#endif

#if MMDebug & Debug_AryHshBuf
// Write array contents to a fabrication object in abbreviated form.  Return status.
static int agUsage1(ArrayWrapper *pArray, DFab *pFab) {
	Datum **ppArrayEl = pArray->pArray->elements;
	Datum **ppArrayElEnd = ppArrayEl + pArray->pArray->used;
	bool first = true;

	if(dputc('[', pFab, 0) != 0)
		goto LibFail;
	while(ppArrayEl < ppArrayElEnd) {
		if(first)
			first = false;
		else if(dputc(',', pFab, 0) != 0)
			goto LibFail;
		if(dtyparray(*ppArrayEl)) {
			Array *pArray = dgetarrayptr((*ppArrayEl));
			if(dputf(pFab, 0, "[%.8x:%.8x:%d]", (uint) (*ppArrayEl), (uint) pArray, pArray->used) != 0)
				goto LibFail;
			}
		else if(dputd(*ppArrayEl, pFab, NULL, CvtLang) < 0)
			goto LibFail;
		++ppArrayEl;
		}
	if(dputc(']', pFab, 0) != 0)
LibFail:
		(void) libfail();

	return sess.rtn.status;
	}

// Write array-usage report to a buffer.
static int agUsage(DFab *pFab, int step) {

	if(step <= 0) {
		UserVar *pUserVar;
		ArrayWrapper *pArray = arrayGarbHead;

		// Dump array info.
		if((step < 0 && dopentrack(pFab) != 0) ||
		 dputf(pFab, 0, "ahfree(): Array List: *%s*\n\n", step < 0 ? "BEFORE" : "AFTER") != 0)
			goto LibFail;
		do {
			if(dputf(pFab, 0, "%.8x   %.8x   %c", (uint) pArray, (uint) pArray->pArray,
			 pArray->uflags ? '*' : ' ') != 0)
				goto LibFail;
			if(agUsage1(pArray, pFab) != Success)
				return sess.rtn.status;
			if(dputc('\n', pFab, 0) != 0)
				goto LibFail;
			} while((pArray = pArray->next) != NULL);

		if(dputs("\nGLOBAL VARIABLES\n\n", pFab, 0) != 0)
			goto LibFail;
		for(pUserVar = globalVarRoot; pUserVar != NULL; pUserVar = pUserVar->next) {
			if(dtyparray(pUserVar->pVal)) {
				pArray = dgetarrayptr(pUserVar->pVal);
				if(dputf(pFab, 0, "%-18s %.8x   %.8x   %c", pUserVar->name, (uint) pArray,
				 (uint) pArray->pArray, pArray->uflags ? '*' : ' ') != 0)
					goto LibFail;
				if(agUsage1(dgetarrayptr(pUserVar->pVal)), pFab) != Success)
					return sess.rtn.status;
				if(dputc('\n', pFab, 0) != 0)
					goto LibFail;
				}
			}
		}
	else {
		Buffer *pBuf;
		bool created;

		if(dclose(pFab, FabStr) != 0)
			goto LibFail;

		// Create debug buffer, clear it, and append results.
		if(bfind("Debug", BN_Create, 0, &pBuf, &created) != Success ||
		 (!created && bclear(pBuf, BC_IgnChgd) != Success))
			return sess.rtn.status;
		if(bappend(pBuf, pFab->pDatum->str) == Success) {
			Datum d;

			dinit(&d);
			(void) render(&d, 2, pBuf, RendRewind);
			dclear(agRelease(&d));
			}
		}
	return sess.rtn.status;
LibFail:
	return libfail();
	}
#endif

// Add an array or hash and any nested arrays or hashes recursively to the tracking list (for subsequent garbage collection) if
// not already there.  Or if "scan" is true, scan array or hash at top level for any nested arrays or hashes only.  An array or
// hash that contains itself either directly or indirectly is allowed.
void ahtrack(Datum *pDatum, bool scan) {
	Array *pArray = (Array *) dgetptr(pDatum);	// Pointer for both array and hash (initially).
#if MMDebug & (Debug_ArrayHash | Debug_AryHshBuf | Debug_AryHshLog | Debug_Datum)
	if(!scan) {
		fputs("ahtrack():", logfile);
		dumpVal(pDatum, false);
		fprintf(logfile, "\n\tnext %.8x, trackHead %.8x, trackTail %.8x\n",
		 (uint) pArray->next, (uint) ahTrackHead, (uint) ahTrackTail);
		}
#endif
	// Convert and add array or hash if it's not already on the list.
	if(scan || (pArray->next == NULL && pArray != ahTrackTail)) {
		if(!scan) {
			(void) drelease(pDatum);
			pArray->uflags = pDatum->type;
			if(ahTrackHead == NULL)
				ahTrackHead = ahTrackTail = dgetptr(pDatum);
			else {
				pArray->next = ahTrackHead;
				ahTrackHead = dgetptr(pDatum);
				}
			}

		// Convert and add any array elements or hash nodes, too.
		if(dtyparray(pDatum)) {
			if(pArray->elements != NULL) {
				Datum **ppArrayEl, **ppArrayElEnd;
				ppArrayElEnd = (ppArrayEl = pArray->elements) + pArray->used;
				while(ppArrayEl < ppArrayElEnd) {
					if(dtypah(*ppArrayEl))
						ahtrack(*ppArrayEl, false);
					++ppArrayEl;
					}
				}
			}
		else {
			HashNode *pNode, **pAry, **pAryEnd;
			Hash *pHash = dgetptr(pDatum);
			if(pHash->nodeCount > 0) {
				pAryEnd = (pAry = pHash->pAry) + pHash->size;
				do {
					for(pNode = *pAry; pNode != NULL; pNode = pNode->next)
						if(dtypah(pNode->pVal))
							ahtrack(pNode->pVal, false);
					} while(++pAry < pAryEnd);
				}
			}
#if MMDebug & (Debug_AryHshBuf | Debug_AryHshLog | Debug_Datum)
		fprintf(logfile, "  %s %.8x -> next %.8x, trackHead %.8x, trackTail %.8x\nSTACK:", dtyphash(pDatum) ? "hash" :
		 "array", (uint) pArray, (uint) pArray->next, (uint) ahTrackHead, (uint) ahTrackTail);
		pArray = ahTrackHead;
		do {
			fprintf(logfile, "\t%.8x -> %.8x\n", (uint) pArray, (uint) pArray->next);
			} while((pArray = pArray->next) != NULL);
#endif
		}
	}

// Stash a new array in a datum and add it to the tracking list.
void astash(Datum *pDatum, Array *pArray) {

#if MMDebug & (Debug_AryHshBuf | Debug_AryHshLog | Debug_Datum)
	fprintf(logfile, "astash(): array %.8x\n", (uint) pArray);
#endif
	dsetarrayref(pArray, pDatum);			// Store array in Datum object...
	ahtrack(pDatum, false);				// and add it to tracking list.
	}

// Stash a new hash in a datum and add it to the tracking list.
void hstash(Datum *pDatum, Hash *pHash) {

#if MMDebug & (Debug_AryHshBuf | Debug_AryHshLog | Debug_Datum)
	fprintf(logfile, "hstash(): hash %.8x\n", (uint) pHash);
#endif
	dsethashref(pHash, pDatum);			// Store hash reference in Datum object...
	ahtrack(pDatum, false);				// and add it to tracking list.
	}

// Scan the array-hash tracking list and clear all the tags.
static void ahuntag(void) {
	Array *pArray = ahTrackHead;
	while(pArray != NULL) {
		pArray->uflags &= ~Tag;
		pArray = pArray->next;
		}
	}

// Tag given array or hash and all nested arrays or hashes (if any).  An array or hash that contains itself is allowed.
static void ahtag(Datum *pDatum) {
	Array *pArray = dgetptr(pDatum);
	if(!(pArray->uflags & Tag)) {
		pArray->uflags |= Tag;
		if(dtyparray(pDatum)) {
			if(pArray->elements != NULL) {
				Datum **ppArrayEl, **ppArrayElEnd;
				ppArrayElEnd = (ppArrayEl = pArray->elements) + pArray->used;
				while(ppArrayEl < ppArrayElEnd) {
					if(dtypah(*ppArrayEl))
						ahtag(*ppArrayEl);
					++ppArrayEl;
					}
				}
			}
		else {
			HashNode *pNode, **pAry, **pAryEnd;
			Hash *pHash = dgetptr(pDatum);
			if(pHash->nodeCount > 0) {
				pAryEnd = (pAry = pHash->pAry) + pHash->size;
				do {
					for(pNode = *pAry; pNode != NULL; pNode = pNode->next)
						if(dtypah(pNode->pVal))
							ahtag(pNode->pVal);
					} while(++pAry < pAryEnd);
				}
			}
		}
	}

// Free all unused arrays and hashes; that is, any array or hash not tied to a global constant or variable.
int ahfree(void) {

	if(ahTrackHead != NULL) {
		Array *pArray;
		HashEach each;
		HashNode *pNode;
		GConstVar *pGConstVar;
		bool foundGlobal;
#if MMDebug & Debug_AryHshBuf
		DFab fab;
#endif
#if MMDebug & Debug_AryHshLog
		if(ahUsage(0) != Success)
			return sess.rtn.status;
#endif

		// Step 1: Scan global constant/variable table and add any arrays and hashes found to tracking list to complete
		// the list of all known arrays and hashes.  This is necessary because a global array or hash currently not on
		// the list may have had an array or hash added to it (for example) that would then not be tagged as a "keeper"
		// and would be freed in error.
		foundGlobal = false;
		heachinit(&each, &gblConstVarTable);
		while((pNode = heach(&each)) != NULL) {
			pGConstVar = (GConstVar *) dgetmemptr(pNode->pVal);			
			if(dtypah(pGConstVar->pVal)) {
				ahtrack(pGConstVar->pVal, false);
				foundGlobal = true;
				}
			}

		// Step 2: Clear all tags in array-hash list, which now contains all known arrays and hashes "flattened".
		ahuntag();

#if MMDebug & Debug_AryHshLog
		if(ahUsage(2) != Success)
			return sess.rtn.status;
#elif MMDebug & Debug_AryHshBuf
		if(agUsage(&fab, -1) != Success)
			return sess.rtn.status;
#endif
		// Step 3: If any arrays or hashes were found in global constant/variable table, scan table again and tag them
		// as "keepers".
		if(foundGlobal) {
			heachinit(&each, &gblConstVarTable);
			while((pNode = heach(&each)) != NULL) {
				pGConstVar = (GConstVar *) dgetmemptr(pNode->pVal);			
				if(dtypah(pGConstVar->pVal))
					ahtag(pGConstVar->pVal);
				}
#if MMDebug & Debug_AryHshLog
			if(ahUsage(3) != Success)
				return sess.rtn.status;
#elif MMDebug & Debug_AryHshBuf
			if(agUsage(&fab, 0) != Success)
				return sess.rtn.status;
#endif
			}

		// Step 4: Do final scan of tracking list, freeing any non-keeper arrays and hashes, then clear list.
		do {
			pArray = ahTrackHead;
			ahTrackHead = ((Array *) ahTrackHead)->next;
			if(pArray->uflags & Tag) {

				// Keeper array or hash found.  Reset its controls for next ahfree() call.
				pArray->next = NULL;
				pArray->uflags = 0;
				}
			else {
				// Non-keeper array found.  Nuke it.
#if MMDebug & Debug_AryHshLog
				fprintf(logfile, "Freeing %s %.8x...\n", pArray->uflags & DHashMask ? "hash" : "array",
				 (uint) pArray);
#elif MMDebug & Debug_AryHshBuf
				if(dputf(&fab, 0, "Freeing %s %.8x...\n", pArray->uflags & DHashMask ? "hash" : "array",
				 (uint) pArray) != 0)
					return libfail();
#endif
				pArray->uflags & DHashMask ? hfree((Hash *) pArray) : afree(pArray);
				}
			} while(ahTrackHead != NULL);
		ahTrackTail = NULL;
#if MMDebug & Debug_AryHshBuf
		(void) agUsage(&fab, 1);
#endif
		}

	return sess.rtn.status;
	}

// Create an array in pRtnVal, given optional size and initializer.  Return status.
int array(Datum *pRtnVal, long n, Datum **args) {
	Array *pArray;
	ArraySize len = 0;
	Datum *pInitializer = NULL;

	// Get array size and initializer, if present.
	if(*args != NULL) {
		len = dgetint((*args++));
		if(*args != NULL)
			pInitializer = *args;
		}
	if((pArray = anew(len, pInitializer)) == NULL)
		return libfail();
	astash(pRtnVal, pArray);
	return sess.rtn.status;
	}


// Do "afill" system function and return status.
static int doFill(Datum *pRtnVal, Datum **args) {
	int rtnCode;
	Array *pArray = dgetarrayptr(args[0]);
	Datum **ppArrayEl, **ppArrayElEnd;
	Datum *pVal = args[3];

	// Call library function with OpObjCopy flag so that any referenced array or hash in fill value will be cloned into each
	// array element.
	rtnCode = afill(pArray, dgetint(args[1]), dgetint(args[2]), pVal, OpGrow | OpObjCopy);
	if(cxlExcep.flags & ExcepMem)
		goto LibFail;

	// Add any cloned array(s) or hash(es) in fill slice to tracking list.
	if(pVal->type & (DArrayMask | DHashMask)) {
		ppArrayElEnd = (ppArrayEl = pArray->elements + dgetint(args[1])) + dgetint(args[2]);
		while(ppArrayEl < ppArrayElEnd)
			ahtrack(*ppArrayEl++, false);
		}
	if(rtnCode != 0)
		goto LibFail;

	// Return result.
	dxfer(pRtnVal, args[0]);
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Do "asort", "hsort", or "htoa" function per pCmdFunc pointer.
static int sortAH(Datum *pRtnVal, Datum **args, CmdFunc *pCmdFunc) {
	Array *pArray;
	Hash *pHash;
	HashNode **pAry;
	CmdFuncId cmdFuncId = pCmdFunc - cmdFuncTable;
	uint flags = pCmdFunc->pOptHdr->selFlags;

	if(cmdFuncId == cf_asort) {

		// Sort array.
		uint flags1 = flags & ~OpInPlace;
		if((pArray = asort(dgetarrayptr(args[0]), flags1 == 0 ? acmp : flags1 == AHS_Descending ? acmpd :
		 flags1 == OpIgnoreCase ? acasecmp : acasecmpd, flags & OpInPlace ? OpInPlace : 0)) == NULL)
			goto LibFail;
		}
	else {
		// Create empty array if hash is empty.
		pHash = dgethashptr(args[0]);
		if(pHash->nodeCount == 0) {
			if((pArray = anew(0, NULL)) == NULL)
				goto LibFail;
			}
		else {
			// Hash not empty.  Convert it to node pointer list -- sorted, if applicable.
			pAry = NULL;
			if(cmdFuncId == cf_htoa) {
				if(htol(pHash, &pAry) != 0)
					goto LibFail;
				}
			else if(hsort(pHash, flags == 0 ? hcmp : flags == AHS_Descending ? hcmpd : flags == OpIgnoreCase ?
			 hcasecmp : hcasecmpd, &pAry) != 0)
				goto LibFail;

			// Convert node pointer list to array and free list.
			if((pArray = hltoa((const HashNode **) pAry)) == NULL)
				goto LibFail;
			free((void *) pAry);
			}
		}

	astash(pRtnVal, pArray);
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Sort lines in region and return status.
int sortRegion(Datum *pRtnVal, long n, Datum **args) {
	OptHdr *pOptHdr = cmdFuncTable[cf_sortRegion].pOptHdr;

	// Do nothing if interactive, non-default n, and no options entered.
	if(!(sess.opFlags & OpScript) && n != DefN && !(pOptHdr->ctrlFlags & OHdrFound))
		return sess.rtn.status;

	// Get number of lines in region and sort the line block.
	if(regionLines(&n) != Success)
		return sess.rtn.status;
	keyEntry.prevFlags &= ~SF_VertMove;			// Kill goal column.
	return bsort(sess.cur.pBuf, &sess.cur.pFace->point, n, pOptHdr->selFlags);
	}

// Split a string into an array and save in pRtnVal, given delimiter and optional limit value.  If n argument, strip all array
// elements by passing n to stripStr().  Return status.
int strSplit(Datum *pRtnVal, long n, Datum **args) {
	Array *pArray;
	short delimChar;
	const char *str;
	int limit = 0;

	// Get string, delimiter, and optional limit.
	str = (*args++)->str;
	if(disnil(*args))
		delimChar = '\0';
	else if(!isCharVal(*args))
		return sess.rtn.status;
	else
		delimChar = dgetint((*args));
	if(*++args != NULL)
		limit = dgetint((*args));

	if((pArray = asplit(str, delimChar, limit)) == NULL)
		return libfail();
	astash(pRtnVal, pArray);

	// Strip the elements if requested.
	if(n != DefN) {
		ArrayEach each;
		Datum *pDatum;

		aeachinit(&each, pArray);
		while((pDatum = aeach(&each)) != NULL)
			if(dsetstr(stripStr(pDatum->str, n), pDatum) != 0)
				return libfail();
		}

	return sess.rtn.status;
	}

// Clone an array.  Return status.
int arrayClone(Datum *pDest, const Datum *pSrc) {
	Array *pArray;

	if((pArray = aclone(dgetarrayptr(pSrc), OpObjCopy)) == NULL)
		return libfail();
	astash(pDest, pArray);

	return sess.rtn.status;
	}

// Clone a hash.  Return status.
int hashClone(Datum *pDest, const Datum *pSrc) {
	Hash *pHash;

	if((pHash = hclone(dgethashptr(pSrc), OpObjCopy)) == NULL)
		return libfail();
	hstash(pDest, pHash);

	return sess.rtn.status;
	}

// Execute selected array or hash function.
int execAH(Datum *pRtnVal, long n, Datum **args, CmdFunc *pCmdFunc) {
	Array *pArray;
	Hash *pHash;
	long longVal;
	CmdFuncId cmdFuncId;
	int i;
	uint flags = pCmdFunc->pOptHdr != NULL ? pCmdFunc->pOptHdr->selFlags : 0;

	// Process function.
	switch(cmdFuncId = pCmdFunc - cmdFuncTable) {
		case cf_aclear:
			dxfer(pRtnVal, args[0]);
			pArray = dgetarrayptr(pRtnVal);
			aclear(pArray);
			break;
		case cf_aclone:
			(void) arrayClone(pRtnVal, args[0]);
			break;
		case cf_acompact:
			dsetint(acompact(dgetarrayptr(args[0])), pRtnVal);
			break;
		case cf_adelete:
			if(args[2] == NULL) {

				// Delete an element.
				Datum *pDatum = adelete(dgetarrayptr(args[0]), dgetint(args[1]));
				if(pDatum == NULL)
					goto LibFail;
				if(dtyparray(pDatum))
					ahtrack(pDatum, false);	// Have array reference.
				dxfer(pRtnVal, pDatum);
				}
			else {
				// Cut a slice.
				if((pArray = aslice(dgetarrayptr(args[0]), dgetint(args[1]), dgetint(args[2]),
				 OpCut)) == NULL)
					goto LibFail;
				astash(pRtnVal, pArray);
				}
			break;
		case cf_adeleteif:
			dsetint(adeleteif(dgetarrayptr(args[0]), args[1], flags), pRtnVal);
			break;
		case cf_afill:
			(void) doFill(pRtnVal, args);
			break;
		case cf_aincludeQ:
			dsetbool(ainclude(dgetarrayptr(args[0]), args[1], flags), pRtnVal);
			break;
		case cf_aindex:
			if((i = aindex(dgetarrayptr(args[0]), args[1], flags)) < 0)
				dsetnil(pRtnVal);
			else
				dsetint(i, pRtnVal);
			break;
		case cf_ainsert:
			longVal = dgetint(args[1]);
			i = 2;
			goto AInsert;
		case cf_apop:
		case cf_ashift:
			pArray = dgetarrayptr(args[0]);
			if((args[1] = (cmdFuncId == cf_apop) ? apop(pArray) : ashift(pArray)) == NULL)
				dsetnil(pRtnVal);
			else {
				dxfer(pRtnVal, args[1]);
				if(dtyparray(pRtnVal))
					ahtrack(pRtnVal, false);
				}
			break;
		case cf_apush:
			longVal = dgetarrayptr(args[0])->used;
			i = 1;
			goto AInsert;
		case cf_asort:
		case cf_hsort:
		case cf_htoa:
			(void) sortAH(pRtnVal, args, pCmdFunc);
			break;
		case cf_atoh:
			{ushort pflags = flags & OpIgnoreCase ? PropIgnoreCase : 0;
			if((pHash = atoh(dgetarrayptr(args[0]), pflags)) == NULL)
				goto LibFail;
			hstash(pRtnVal, pHash);
			}
			break;
		case cf_aunshift:
			longVal = 0;
			i = 1;
AInsert:
			if(ainsert(dgetarrayptr(args[0]), longVal, args[i], OpAdopt) != 0)
				goto LibFail;
			duntrack(args[i]);
			dxfer(pRtnVal, args[0]);
			break;
		case cf_hash:
			{HashSize size;
			ushort pflags = flags & OpIgnoreCase ? PropIgnoreCase : 0;

			// Get hash size, if present.
			size = args[0] != NULL ? dgetint(args[0]) : 0;
			if((pHash = hnew(size, pflags)) == NULL)
				goto LibFail;
			hstash(pRtnVal, pHash);
			}
			break;
		case cf_hasKeyQ:
			dsetbool(hsearch(dgethashptr(args[0]), args[1]->str) != NULL, pRtnVal);
			break;
		case cf_hclear:
			dxfer(pRtnVal, args[0]);
			pHash = dgethashptr(pRtnVal);
			hclear(pHash, args[1] != NULL ? dgetint(args[1]) : 0);
			break;
		case cf_hclone:
			(void) hashClone(pRtnVal, args[0]);
			break;
		case cf_hcompact:
			longVal = hcompact(dgethashptr(args[0]));
			goto SetInt;
		case cf_hdeleteif:
			longVal = hdeleteif(dgethashptr(args[0]), args[1], flags);
SetInt:
			dsetint(longVal, pRtnVal);
			break;
		case cf_hfindnode:
		case cf_hincludeQ:
			{HashNode *pNode = hfindnode(dgethashptr(args[0]), args[1], flags);
			if(cmdFuncId == cf_hincludeQ)
				dsetbool(pNode != NULL, pRtnVal);
			else if(pNode != NULL) {
				if((pArray = hntoa(pNode)) == NULL)
					goto LibFail;
				astash(pRtnVal, pArray);
				}
			}
			break;
		case cf_hdelete:
			{Datum *pDatum = hdelete(dgethashptr(args[0]), args[1]->str);
			if(pDatum != NULL)
				dxfer(pRtnVal, pDatum);
			}
			break;
		case cf_hkeys:
		case cf_hvalues:
			pArray = hkvtoa(dgethashptr(args[0]), cmdFuncId == cf_hkeys);
			if(pArray == NULL)
				goto LibFail;
			astash(pRtnVal, pArray);
			break;
		}

	return sess.rtn.status;
LibFail:
	return libfail();
	}
