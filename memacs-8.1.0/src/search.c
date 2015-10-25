// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// search.c	Search functions for MightEMacs.
//
// These functions implement commands that search in the forward and backward directions.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

#define max(a, b)	((a < b) ? b : a)

// Local variables.
static GrpInfo groups[MAXGROUPS];		// Descriptors for each RE group match.

// Delta1 table: delta1[c] contains the distance between the last character of pat and the rightmost occurrence of c in pat.  If
// c does not occur in pat, then delta1[c] = patlen.  If c is at string[i] and c != pat[patlen - 1], we can safely shift i over
// by delta1[c], which is the minimum distance needed to shift pat forward to get string[i] lined up with some character in pat.
// This algorithm runs in alphabet_len + patlen time.
static void mkdelta1(int *delta1,char *pat) {
	int i;
	int exact = modetab[MDR_GLOBAL].flags & MDEXACT;
	int patlen1 = srch.patlen - 1;

	// First, set all characters to the maximum jump.
	for(i = 0; i < HICHAR; ++i)
		delta1[i] = srch.patlen;

	// Now, set the characters in the pattern.  Set both cases of letters if eXact mode is false.
	for(i = 0; i < patlen1; ++i) {
		delta1[(int) pat[i]] = patlen1 - i;
		if(!exact)
			delta1[chcase((int) pat[i])] = patlen1 - i;
		}
	}
 
// True if the suffix of word starting from word[pos] is a prefix of word.
static bool isprefix(char *word,int wordlen,int pos) {
	int i;
	int suffixlen = wordlen - pos;

	// Could also use the strncmp() library function here.
	for(i = 0; i < suffixlen; ++i) {
		if(word[i] != word[pos + i])
			return false;
		}
	return true;
	}
 
// Length of the longest suffix of word ending on word[pos].  suffix_length("dddbcabc",8,4) = 2.
static int suffix_length(char *word,int wordlen,int pos) {
	int i;

	// Increment suffix length i to the first mismatch or beginning of the word.
	for(i = 0; (word[pos - i] == word[wordlen - 1 - i]) && (i < pos); ++i);
	return i;
	}
 
// Delta2 table: given a mismatch at pat[pos], we want to align with the next possible full match, based on what we know about
// pat[pos + 1] to pat[patlen - 1].
//
// In case 1:
// pat[pos + 1] to pat[patlen - 1] does not occur elsewhere in pat and the next plausible match starts at or after the mismatch.
// If, within the subString pat[pos + 1 .. patlen - 1], lies a prefix of pat, the next plausible match is here (if there are
// multiple prefixes in the subString, pick the longest).  Otherwise, the next plausible match starts past the character aligned
// with pat[patlen - 1].
//
// In case 2:
// pat[pos + 1] to pat[patlen - 1] does occur elsewhere in pat.  The mismatch tells us that we are not looking at the end of a
// match.  We may, however, be looking at the middle of a match.
//
// The first loop, which takes care of case 1, is analogous to the KMP table, adapted for a 'backwards' scan order with the
// additional restriction that the subStrings it considers as potential prefixes are all suffixes. In the worst case scenario,
// pat consists of the same letter repeated, so every suffix is a prefix.  This loop alone is not sufficient, however.  Suppose
// that pat is "ABYXCDEYX", and text is ".....ABYXCDEYX".  We will match X, Y, and find B != E.  There is no prefix of pat in
// the suffix "YX", so the first loop tells us to skip forward by 9 characters.  Although superficially similar to the KMP
// table, the KMP table relies on information about the beginning of the partial match that the BM algorithm does not have.
//
// The second loop addresses case 2.  Since suffix_length may not be unique, we want to take the minimum value, which will tell
// us how far away the closest potential match is.
static void mkdelta2(int *delta2,char *pat) {
 
	// If eXact mode is false, set values in this table to the minimum jump for each pattern index which will advance the
	// search by one position -- it can't be used in the usual way.
	if((modetab[MDR_GLOBAL].flags & MDEXACT) == 0) {
		int i = srch.patlen;

		// Set the first "patlen" entries to zero.
		do {
			*delta2++ = i--;
			} while(i > 0);
		}
	else {
		int i;
		int patlen1 = srch.patlen - 1;
		int last_prefix_index = patlen1;

		// First loop.
		i = srch.patlen;
		do {
			if(isprefix(pat,srch.patlen,i))
				last_prefix_index = i;
			--i;
			delta2[i] = last_prefix_index + (patlen1 - i);
			} while(i > 0);

		// Second loop.
		for(i = 0; i < patlen1; ++i) {
			int slen = suffix_length(pat,srch.patlen,i);
			if(pat[i - slen] != pat[patlen1 - slen])
				delta2[patlen1 - slen] = patlen1 - i + slen;
			}
		}
	}

// Make delta tables for non-RegExp search.  Return status.
void mkdeltas(void) {

	mkdelta1(srch.fdelta1,srch.pat);
	mkdelta2(srch.fdelta2,srch.pat);
	mkdelta1(srch.bdelta1,srch.bpat);
	mkdelta2(srch.bdelta2,srch.bpat);
	}

// Initialize search parameters for new pattern.
void newpat(int len) {

	srch.patlen = (len >= 0) ? len : strlen(srch.pat);
	strrev(strcpy(srch.bpat,srch.pat));
	srch.fdelta1[0] = -1;			// Clear delta tables.
	mcclear();				// Clear RegExp meta table.
	}

// Read a search or replacement pattern delimited by sdelim and stash it in global variable pat (srchpat is true) or rpat
// (srchpat is false).  If it is the search string, create the reverse pattern and the magic (RE) pattern, assuming we are in
// Regexp mode (and #defined that way).  The pattern variable is not updated if the user types in an empty line.  If that
// happens and there is no old pattern, it is an error.  Display the old pattern in the style of Jeff Lomicka.  Return status.
int readpattern(char *promptp,int srchpat) {
	char *apat;
	Value *tpatp;

	apat = srchpat ? srch.pat : srch.rpat;
	if(vnew(&tpatp,false) != 0)
		return vrcset();

	// Read a pattern.  Either we get one, or we just get the input terminator and use the previous pattern.
	if(opflags & OPSCRIPT) {
		if(havesym(s_any,false) && (macarg(tpatp,srchpat ? ARG_FIRST | ARG_STR : ARG_STR) != SUCCESS))
			return rc.status;
		}
	else if(termarg(tpatp,promptp,apat,srch.sdelim,0) != SUCCESS)
		return rc.status;

	// Check search string.
	if(visnull(tpatp) && srchpat)
		return rcset(FAILURE,0,text80);		// Null search string not allowed.
				// "No pattern set"
	// New pattern?
	else if(strcmp(tpatp->v_strp,apat) != 0) {
		int len;

		// Too long?
		if((len = strlen(tpatp->v_strp)) > NPAT)
			return rcset(FAILURE,0,text281,text283,NPAT);
				// "%s cannot exceed %d characters","Pattern"

		// Save the new pattern, make the reverse version, and clear the dependent search structures.
		strcpy(apat,tpatp->v_strp);
		if(srchpat)
			newpat(len);
		else
			rmcclear();
		}

	return rc.status;
	}

// Return true if given dot position is at a boundary (beginning of end of buffer); otherwise, false.
int boundary(Dot *dotp,int dir) {

	return (dir == FORWARD) ? dotp->lnp == curbp->b_hdrlnp : dotp->off == 0 && lback(dotp->lnp) == curbp->b_hdrlnp;
	}

// Create search tables if needed.
int mktab(void) {

	if((modetab[MDR_GLOBAL].flags & MDREGEXP) && srch.mcpat[0].mc_type == MCE_NIL && mccompile() != SUCCESS)
		return rc.status;
	if(((modetab[MDR_GLOBAL].flags & MDREGEXP) == 0 || (srch.flags & SREGICAL) == 0) && srch.fdelta1[0] == -1)
		mkdeltas();
	return rc.status;
	}

// Search forward.  Get a search string from the user, and search for it.  If found, reset dot to be just after the matched
// string, and (perhaps) repaint the display.
int searchForw(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return searchBack(rp,-n);	// Search backward.

	// Ask the user for the text of a pattern.  If the response is SUCCESS, search for the pattern up to n times, as long as
	// the pattern is there to be found.
	if(readpattern(text78,true) == SUCCESS)
			// "Search"
		(void) huntForw(rp,n);

	return rc.status;
	}

// Search forward for a previously acquired search string.  If found, dot is left pointing just after the matched string and
// the matched string is returned; otherwise, false is returned.
int huntForw(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return huntBack(rp,-n);		// Search backward.

	// Make sure a pattern exists, or that we didn't switch into Regexp mode after we entered the pattern.
	if(srch.pat[0] == '\0')
		return rcset(FAILURE,0,text80);
			// "No pattern set"

	// Create search tables if needed.
	if(mktab() != SUCCESS)
		return rc.status;

	// Perform appropriate search and return result.
	return vsetstr(((((srch.flags & SREGICAL) && (modetab[MDR_GLOBAL].flags & MDREGEXP)) ? mcscan(n,FORWARD,PTEND) :
	 scan(n,FORWARD,PTEND)) == NOTFOUND) ? val_false : srch.patmatch,rp) != 0 ? vrcset() : rc.status;
	}

// Reverse search.  Get a search string from the user and, starting at dot, search toward the top of the buffer.  If match is
// found, dot is left pointing at the first character of the pattern (the last character that was matched).
int searchBack(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return searchForw(rp,-n);	// Search forward.

	// Ask the user for the text of a pattern.  If the response is SUCCESS, search for the pattern up to n times, as long as
	// the pattern is there to be found.
	if(readpattern(text81,true) == SUCCESS)
			// "Reverse search"
		(void) huntBack(rp,n);

	return rc.status;
	}

// Reverse search for a previously acquired search string, starting at dot and proceeding toward the top of the buffer.  If
// found, dot is left pointing at the first character of the matched string (the last character that was matched) and the
// matched string is returned; otherwise, false is returned.
int huntBack(Value *rp,int n) {

	if(n == INT_MIN)
		n = 1;
	else if(n < 0)
		return huntForw(rp,-n);		// Search forward.

	// Make sure a pattern exists, or that we didn't switch into Regexp mode after we entered the pattern.
	if(srch.bpat[0] == '\0')
		return rcset(FAILURE,0,text80);
			// "No pattern set"

	// Create search tables if needed.
	if(mktab() != SUCCESS)
		return rc.status;

	// Perform appropriate search and return result.
	return vsetstr(((((srch.flags & SREGICAL) && (modetab[MDR_GLOBAL].flags & MDREGEXP)) ? mcscan(n,BACKWARD,PTBEGIN) :
	 scan(n,BACKWARD,PTBEGIN)) == NOTFOUND) ? val_false : srch.patmatch,rp) != 0 ? vrcset() : rc.status;
	}

// Compare two characters ("bc" from the buffer, and "pc" from the pattern) and return bool result.  If we are not in EXACT
// mode, fold the case.
static bool eq(int bc,int pc) {

	if((modetab[MDR_GLOBAL].flags & MDEXACT) == 0) {
		bc = lowcase[bc];
		pc = lowcase[pc];
		}

	return bc == pc;
	}

// Retrieve the next/previous character in the buffer, and advance/retreat the point.  The order in which this is done is
// significant and depends upon the direction of the search.  Forward searches fetch and advance; reverse searches back up and
// fetch.  Return -1 if a buffer boundary is hit.
static int nextch(Dot *dotp,int dir) {
	int c;

	if(dir == FORWARD) {					// Going forward?
		if(dotp->lnp == curbp->b_hdrlnp)		// Yes.  At bottom of buffer?
			c = -1;					// Yes, return -1;
		else if(dotp->off == lused(dotp->lnp)) {		// No.  At EOL?
			dotp->lnp = lforw(dotp->lnp);		// Yes, skip to next line ...
			dotp->off = 0;
			c = '\r';				// and return a <NL>.
			}
		else
			c = lgetc(dotp->lnp,dotp->off++);	// Otherwise, return current char.
		}
	else if(dotp->off == 0) {				// Going backward.  At BOL?
		if(lback(dotp->lnp) == curbp->b_hdrlnp)		// Yes.  At top of buffer?
			c = -1;					// Yes, return -1.
		else {
			dotp->lnp = lback(dotp->lnp);		// No.  Skip to previous line ...
			dotp->off = lused(dotp->lnp);
			c = '\r';				// and return a <NL>.
			}
		}
	else
		c = lgetc(dotp->lnp,--dotp->off);		// Not at BOL ... return previous char.

	return c;
	}

// Move the buffer scanning dot by jumpsz characters.  Return false if a boundary is hit (and we may therefore search no
// further); otherwise, update dot and return true.
static bool bjump(int jumpsz,Dot *scandotp,int direct) {

	if(direct == FORWARD) {						// Going forward.
		int spare;

		if(scandotp->lnp == curbp->b_hdrlnp)
			return false;					// Hit end of buffer.

		scandotp->off += jumpsz;
		spare = scandotp->off - lused(scandotp->lnp);		// Overflow size.

		while(spare > 0) {					// Skip to next line.
			if((scandotp->lnp = lforw(scandotp->lnp)) == curbp->b_hdrlnp && spare > 1)
				return false;				// Hit end of buffer.
			scandotp->off = spare - 1;			// Line terminator.
			spare = scandotp->off - lused(scandotp->lnp);
			}
		}
	else {								// Going backward.
		scandotp->off -= jumpsz;
		while(scandotp->off < 0) {				// Skip back a line.
			if((scandotp->lnp = lback(scandotp->lnp)) == curbp->b_hdrlnp)
				return false;				// Hit beginning of buffer.
			scandotp->off += lused(scandotp->lnp) + 1;
			}
		}

	return true;
	}

// If we found the pattern, save it away.  Return status.
static int savematch(void) {
	int j;
	GrpInfo *gip;
	Region *regp;

	// Set up group 0 (the entire match).
	srch.grpmatch[0] = srch.patmatch;		// Set old pointer (so heap space can be released).
	gip = groups;
	regp = &gip->region;
	regp->r_dot = srch.matchdot;			// Set actual match position ...
	regp->r_size = srch.matchlen;			// and length.
	gip->elen = 0;

	// Save the pattern match (group 0) and all the RE groups, if applicable.
	for(j = 0; j <= srch.grpct; ++j) {
		gip = groups + j;
		regp = &gip->region;
		regp->r_size += gip->elen;

		if(srch.grpmatch[j] != NULL)
			free((void *) srch.grpmatch[j]);

		if((srch.grpmatch[j] = (char *) malloc(regp->r_size + 1)) == NULL)
			return rcset(PANIC,0,text94,"savematch");
					// "%s(): Out of memory!"
		regcpy(srch.grpmatch[j],regp);
		}
	srch.patmatch = srch.grpmatch[0];		// Set new (heap) pointer.

	return rc.status;
	}

#define SHOW_COMPARES	0

// Search for a pattern in either direction.  If found, position dot at the start or just after the match string and (perhaps)
// repaint the display.  Fast version using code from Boyer and Moore, Software Practice and Experience, vol. 10 (1980).  "n" is
// repeat count, "direct" is FORWARD or BACKWARD, and "ptpos" is PTBEGIN or PTEND.  Return NOTFOUND (bypassing rcset()) if
// search failure.
int scan(int n,int direct,int ptpos) {
	int bc,pc;		// Buffer char and pattern char.
	int pati;		// Pattern index.
	char *patp;		// Pointer into pattern.
	char *pattern;		// String to scan for.
	Dot scandot;		// Current line and offset during scanning.
	int sdirect;		// Scan direction.
	int *delta1,*delta2;
	int jumpsz;		// Next offset.
#if SHOW_COMPARES
	int compares = 0;
#endif
	// If we are going backward, then the 'end' is actually the beginning of the pattern.  Toggle ending-point indicator.
	// Also, buffer scanning for a pattern match is always in the opposite direction of the search.
	ptpos ^= direct;
	sdirect = direct ^ 1;

	// Another directional problem: if we are searching forward, use the backward (reversed) pattern in bpat[], and the
	// forward delta tables; otherwise, use the forward pattern in pat[], and the backward delta tables.  This is so the
	// pattern can always be scanned from left to right (until the terminating null) during comparisions.
	if(direct == FORWARD) {
		pattern = srch.bpat;
		delta1 = srch.fdelta1;
		delta2 = srch.fdelta2;
		}
	else {
		pattern = srch.pat;
		delta1 = srch.bdelta1;
		delta2 = srch.bdelta2;
		}

	// Set local scan variable to dot.
	scandot = curwp->w_face.wf_dot;

	// Scan the buffer until we find the nth match or hit a buffer boundary.  The basic loop is to jump forward or backward
	// in the buffer (so that comparisons are done in reverse), check for a match, get the next jump size (delta) on a
	// failure, and repeat.  The initial jump is the full pattern length so that the first character returned by nextch() is
	// at the far end of a possible match, depending on the search direction.
	jumpsz = srch.matchlen = srch.patlen;
	while(bjump(jumpsz,&scandot,direct)) {

		// Save the current position in case we match the search string at this point.
		srch.matchdot = scandot;

		// Scan through the pattern for a match.  Note that nextch() will never hit a buffer boundary (and return -1)
		// because bjump() checks for that.
		patp = pattern;
		pati = srch.patlen;
#if SHOW_COMPARES
		++compares;
#endif
		while((pc = *patp++) != '\0') {
			--pati;
			if(!eq(pc,bc = nextch(&scandot,sdirect))) {

				// No match.  Jump forward or backward in buffer as far as possible and try again.
				// Use delta + 1 so that nextch() begins at the right spot.
				jumpsz = max(delta1[bc],delta2[pati]) + 1;
				goto fail;
				}
			}

		// A SUCCESSFULL MATCH!  Flag that we have moved, update the dot pointers, and save the match.  Need to do this
		// now because the nth match may not be found, in which case we want to leave the dot at the last one.
		curwp->w_flags |= WFMOVE;
		if(ptpos == PTEND)			// Leave point at end of string.
			curwp->w_face.wf_dot = srch.matchdot;
		else					// Leave point at beginning of string.
			curwp->w_face.wf_dot = scandot;

		// If we were heading forward, set the "match" pointers to the beginning of the string.
		if(direct == FORWARD)
			srch.matchdot = scandot;
		if(savematch() != SUCCESS)
			return rc.status;

		// Return if nth match found.
		if(--n <= 0)
#if SHOW_COMPARES
			return rcset(SUCCESS,0,"%d comparisons",compares);
#else
			return rc.status;
#endif

		// Nth match not found.  Reset and continue scan.
		jumpsz = srch.patlen * 2;
fail:;
		}

	// No match found.
#if SHOW_COMPARES
	(void) rcset(SUCCESS,RCNOWRAP,"Not found (%d comparisons)",compares);
#else
	(void) rcset(SUCCESS,RCNOWRAP,text79);
			// "Not found"
#endif
	return NOTFOUND;
	}

// Set a bit (ON only) in the bitmap.
static void setbit(int bc,EBitMap *cclmap) {

	if((unsigned) bc < HICHAR)
		*((char *) cclmap + (bc >> 3)) |= BIT(bc & 7);
	}

// Create the bitmap for the character class.  patpp is left pointing to the end-of-character-class character, so that a
// loop may automatically increment with safety.  Return status.
static int cclmake(char **patpp,MetaChar *mcp) {
	EBitMap *bmap;
	char *patp;
	int pchr,ochr;

	if((bmap = (EBitMap *) malloc(sizeof(EBitMap))) == NULL)
		return rcset(PANIC,0,text94,"cclmake");
			// "%s(): Out of memory!"

	memset((void *) bmap,0,sizeof(EBitMap));
	mcp->u.cclmap = bmap;
	patp = *patpp;
	ochr = MC_CCLBEGIN;

	// Test the initial character(s) in ccl for special cases: negate ccl, or an end ccl character as a first character.
	// Anything else gets set in the bitmap.
	if(*++patp == MC_NCCL) {
		++patp;
		mcp->mc_type = MCE_NCCL;
		}
	else
		mcp->mc_type = MCE_CCL;

	if((pchr = *patp) == MC_CCLEND)
		return rcset(FAILURE,0,text96);
			// "No characters in character class"

	while(pchr != MC_CCLEND && pchr != '\0') {
		switch(pchr) {

			// Range character loses its meaning if it is the first or last character in the class.  We also treat
			// it as ordinary if the order is wrong, e.g. "z-a".
			case MC_CCLRANGE:
				pchr = *(patp + 1);
				if(ochr == MC_CCLBEGIN || pchr == MC_CCLEND || ochr > pchr)
					setbit(MC_CCLRANGE,bmap);
				else {
					do {
						setbit(++ochr,bmap);
						} while(ochr < pchr);
					++patp;
					}
				break;
			case MC_ESC:
				pchr = *++patp;
				// Fall through.
			default:
				setbit(pchr,bmap);
				break;
			}
		ochr = pchr;
		pchr = *++patp;
		}

	*patpp = patp;
	if(pchr == '\0')
		return rcset(FAILURE,0,text97);
			// "Character class not ended"

	return rc.status;
	}

// Convert the RE (string) in the global "pat" variable to a forward MetaChar array rooted in the global "mcpat" variable and a
// backward MetaChar array rooted in the global "bmcpat" variable.  Return status.
//
// A closure symbol is taken as a literal character when (1), it is the first character in the pattern; or (2), when preceded by
// a symbol that does not allow closure, such as beginning or end of line symbol, or another closure symbol.  However, it is
// treated as an error if preceded by a group-end symbol because it should be legal, but it is not supported by this
// implementation.
int mccompile(void) {
	MetaChar *mcp,*bmcp;
	char *patp,*strp;
	int pc,n;
	bool lastDoesClosure = false;
	int grpstack[MAXGROUPS];
	int stacklevel = 0;

	mcp = srch.mcpat;
	patp = srch.pat;

	while((pc = *patp) != '\0') {
		switch(pc) {
			case MC_CCLBEGIN:
				if(cclmake(&patp,mcp) != SUCCESS)
					goto clexit;
				lastDoesClosure = true;
				srch.flags |= SREGICAL;
				break;
			case MC_BOL:
				// If the BOL character isn't the first in the pattern, we assume it's a literal instead.
				if(mcp > srch.mcpat)
					goto litcase;
				mcp->mc_type = MCE_BOL;
				srch.flags |= SREGICAL;
				break;
			case MC_EOL:
				// If the EOL character isn't the last in the pattern, we assume it's a literal instead.
				if(patp[1] != '\0')
					goto litcase;
				mcp->mc_type = MCE_EOL;
				srch.flags |= SREGICAL;
				break;
			case MC_ANY:
				mcp->mc_type = MCE_ANY;
				lastDoesClosure = true;
				srch.flags |= SREGICAL;
				break;
			case MC_CLOSURE0:
				n = MCE_CLOSURE0;
				goto checkcl;
			case MC_CLOSURE1:
				n = MCE_CLOSURE1;
checkcl:
				// Does the closure symbol mean closure here?  If so, back up to the previous element and
				// mark it as such.
				if(lastDoesClosure)
					goto docl;
				goto litcase;
			case MC_CLOSURE01:

				// Does the zero-or-one closure symbol symbol mean closure here?
				if(lastDoesClosure)
					n = MCE_CLOSURE01;
				else {
					// Not closure.  Is it closure modification?  If so, back up to the previous element and
					// mark it as such.
					if(mcp == srch.mcpat || (mcp[-1].mc_type & MCE_ALLCLOSURE) == 0)
						goto litcase;
					--mcp;
					mcp->mc_type |= MCE_MINCLOSURE;
					break;
					}
docl:
				// Back up ...
				--mcp;

				// and check for closure on a group (not supported).
				if(mcp->mc_type == MCE_GRPEND) {
					(void) rcset(FAILURE,0,text304,srch.pat);
							// "Closure on group not supported in RE pattern '%s'"
					goto clexit;
					}

				// All is well ... add closure mask.
				mcp->mc_type |= n;
				srch.flags |= SREGICAL;
				lastDoesClosure = false;
				break;
			case MC_GRPBEGIN:
				srch.flags |= SREGICAL;

				// Start of a group.  Indicate it, and set regical.
				if(++srch.grpct < MAXGROUPS) {
					mcp->mc_type = MCE_GRPBEGIN;
					mcp->u.ginfo = groups + srch.grpct;
					grpstack[stacklevel++] = srch.grpct;
					lastDoesClosure = false;
					}
				else {
					(void) rcset(FAILURE,0,text221,srch.pat,MAXGROUPS);
							// "Too many groups in RE pattern '%s' (maximum is %d)"
					goto clexit;
					}
				break;
			case MC_GRPEND:

				// If we've no groups to close, assume a literal character.  Otherwise, indicate the end of a
				// group.
				if(stacklevel > 0) {
					mcp->mc_type = MCE_GRPEND;
					mcp->u.ginfo = groups + grpstack[--stacklevel];
					break;
					}
				goto litcase;
			case MC_ESC:
				srch.flags |= SREGICAL;
				pc = *++patp;
				if(pc == '\0') {
					pc = '\\';
					--patp;
					}
				goto litcase;
			case MC_OPT:
				// If options character is at beginning of pattern or not at end and followed by one or more
				// lower case letters, take it literally.
				if(patp > srch.pat) {
					strp = patp + 1;
					while(*strp >= 'a' && *strp <= 'z')
						++strp;
					if(strp > patp + 1 && *strp == '\0') {
						do {
							switch(*++patp) {
								case MCOPT_MULTI:
									srch.flags |= SMULTILINE;
									break;
								default:
									(void) rcset(FAILURE,0,text36,*patp);
											// "Unknown RE option '%c'"
									goto clexit;
								}
							} while(patp[1] != '\0');
						goto done;
						}
					}
				// Literal character -- fall through.
			default:
litcase:
				mcp->mc_type = MCE_LITCHAR;
				mcp->u.lchar = pc;
				lastDoesClosure = true;
				break;
			}		// End of switch.
		++mcp;
		++patp;
		}		// End of while.
done:
	// Check RE group level.
	if(stacklevel) {
		(void) rcset(FAILURE,0,text222);
			// "RE group not ended"
		goto clexit;
		}

	// No error: terminate the forward array, make the reverse array, and return success.
	mcp->mc_type = MCE_NIL;
	bmcp = srch.bmcpat;
	do {
		*bmcp = *--mcp;
		++bmcp;
		} while(mcp > srch.mcpat);
	bmcp->mc_type = MCE_NIL;

	return rc.status;
clexit:
	// Error: clear the MetaChar array and return exception status.
	mcp[1].mc_type = MCE_NIL;
	mcclear();
	return rc.status;
	}

// Free up any CCL bitmaps, and MCE_NIL the MetaChar search arrays.
void mcclear(void) {
	MetaChar *mcp;
	int j;

	mcp = srch.mcpat;
	while(mcp->mc_type != MCE_NIL) {
		if((mcp->mc_type == MCE_CCL) || (mcp->mc_type == MCE_NCCL))
			free((void *) mcp->u.cclmap);
		++mcp;
		}
	srch.mcpat[0].mc_type = srch.bmcpat[0].mc_type = MCE_NIL;

	// Remember that srch.grpmatch[0] == patmatch.
	for(j = 0; j < MAXGROUPS; ++j) {
		if(srch.grpmatch[j] != NULL) {
			free((void *) srch.grpmatch[j]);
			srch.grpmatch[j] = NULL;
			}
		}

	srch.patmatch = NULL;
	srch.grpct = 0;
	srch.flags &= ~(SREGICAL | SMULTILINE);
	}

// Is the character set in the bitmap?
static bool biteq(int bc,EBitMap *cclmap) {

	if((unsigned) bc >= HICHAR)
		return false;

	return ((*((char *) cclmap + (bc >> 3)) & BIT(bc & 7)) ? true : false);
	}


// Perform meta-character equality test with a character and return boolean result.  In Kernighan & Plauger's Software Tools,
// this is the omatch() function.  The "buffer boundary" character (-1) never matches anything.
static bool mceq(int bc,MetaChar *mt) {
	bool result;
	int c;

	if(bc == -1)
		return false;

	switch(mt->mc_type & MCE_BASETYPE) {
		case MCE_LITCHAR:
			result = eq(bc,mt->u.lchar);
			break;
		case MCE_ANY:
			result = (bc != '\r' || (srch.flags & SMULTILINE));
			break;
		case MCE_CCL:
			if(!(result = biteq(bc,mt->u.cclmap))) {
				if((modetab[MDR_GLOBAL].flags & MDEXACT) == 0 && (c = chcase(bc)) != bc)
					result = biteq(c,mt->u.cclmap);
				}
			break;
		default:	// MCE_NCCL.
			result = (bc != '\r' || (srch.flags & SMULTILINE)) && !biteq(bc,mt->u.cclmap);
			if((modetab[MDR_GLOBAL].flags & MDEXACT) == 0 && (c = chcase(bc)) != bc)
				result &= !biteq(c,mt->u.cclmap);
		}

	return result;
	}

// Try to match a meta-pattern at the given position in either direction.  Based on the recursive routine amatch() (for
// "anchored match") in Kernighan & Plauger's "Software Tools".  "mcp" is the pattern, "direct" is FORWARD or BACKWARD, and
// "scandotp" contains a pointer to the scan dot.  If the given position matches, update *scandotp and return true; otherwise,
// return false.
static bool amatch(MetaChar *mcp,int direct,Dot *scandotp) {
	Region *regp;
	Dot scandot;		// Current line and offset during scan.
	int pre_matchlen;	// Matchlen before a recursive amatch() call.
	int cl_matchlen;	// Number of characters matched in a local closure.
	int cl_type;		// Closure type.

	// Set up local scan pointers to dot and set up our local character counting variable, which can correct matchlen on
	// a failed partial match.
	scandot = *scandotp;
	cl_matchlen = 0;

	// Loop through the meta-pattern, laughing all the way.
	while(mcp->mc_type != MCE_NIL) {

		// Is the current meta-character modified by a closure?
		if((cl_type = mcp->mc_type & MCE_ALLCLOSURE) != 0) {
			if(mcp->mc_type & MCE_MINCLOSURE) {
				int cl_max;	// Maximum number of characters can match in closure.

				// Match minimum number possible (zero or one).  First, match one if that is the minimum.
				if(cl_type == MCE_CLOSURE1) {
					if(!mceq(nextch(&scandot,direct),mcp))
						return false;		// It's a bust.
					++srch.matchlen;
					}
				cl_max = (cl_type == MCE_CLOSURE01) ? 1 : INT_MAX;	// Fake it.

				// So far, so good.  Now try to match the rest of the pattern and expand the closure by one if
				// possible for each failure.
				for(;;) {
					pre_matchlen = srch.matchlen;
					if(amatch(mcp + 1,direct,&scandot))
						goto success;
					srch.matchlen = pre_matchlen;
					if(--cl_max < 0 || !mceq(nextch(&scandot,direct),mcp))
						return false;		// It's a bust.
					++srch.matchlen;
					}
				}
			else {
				int bc;		// Buffer character.
				int cl_min;	// Minimum number of characters must match in closure.

				if(cl_type == MCE_CLOSURE01) {
					cl_min = 0;

					if(mceq(bc = nextch(&scandot,direct),mcp)) {
						bc = nextch(&scandot,direct);
						++cl_matchlen;
						}
					}
				else {
					// Minimum number of characters that may match is 0 or 1.
					cl_min = (cl_type == MCE_CLOSURE1);

					// Match as many characters as possible against the current meta-character.
					while(mceq(bc = nextch(&scandot,direct),mcp))
						++cl_matchlen;
					}

				// We are now at the character (bc) that made us fail.  Try to match the rest of the pattern and
				// shrink the closure by one for each failure.  If the scan dot is now at a buffer boundary (bc
				// == -1), simulate a -1 for the first character returned by nextch() and don't move the scan
				// dot so that we loop in reverse properly.
				++mcp;
				srch.matchlen += cl_matchlen;

				for(;;) {
					if(cl_matchlen < cl_min) {
						srch.matchlen -= cl_matchlen;
						return false;
						}
					if(bc == -1) {
						if(mcp->mc_type == MCE_NIL)
							goto success;
						bc = 0;
						--srch.matchlen;
						}
					else if(nextch(&scandot,direct ^ 1) == -1)
						--srch.matchlen;
					else {
						pre_matchlen = srch.matchlen;
						if(amatch(mcp,direct,&scandot))
							goto success;
						srch.matchlen = pre_matchlen - 1;
						}
					--cl_matchlen;
					}
				}
			}
		else if(mcp->mc_type == MCE_GRPBEGIN) {
			regp = &mcp->u.ginfo->region;
			regp->r_dot = scandot;
			regp->r_size = (direct == FORWARD) ? -srch.matchlen : srch.matchlen;
			}
		else if(mcp->mc_type == MCE_GRPEND)
			mcp->u.ginfo->elen = (direct == FORWARD) ? srch.matchlen : -srch.matchlen;
		else if(mcp->mc_type == MCE_BOL) {
			if(scandot.off != 0)
				return false;
			}
		else if(mcp->mc_type == MCE_EOL) {
			if(scandot.off != lused(scandot.lnp))
				return false;
			}
		else {
			// A character to compare against the meta-character equal function.  If we still match, increment
			// the length.
			if(!mceq(nextch(&scandot,direct),mcp))
				return false;
			++srch.matchlen;
			}
		++mcp;
		}
success:
	// A SUCCESSFULL MATCH!  Update the dot pointers.
	*scandotp = scandot;
	return true;
	}

// Search for a meta-pattern in either direction.  If found, position dot at the start or just after the match string and
// (perhaps) repaint the display.  "n" is repeat count, "direct" is FORWARD or BACKWARD, and "ptpos" is PTBEGIN or PTEND. 
// Return NOTFOUND (bypassing rcset()) if search failure.
int mcscan(int n,int direct,int ptpos) {
	MetaChar *mcpatrn;		// Pointer into pattern.
	Dot scandot;		// Current line and offset during scan.

	// If we are going backward, then the 'end' is actually the beginning of the pattern.  Toggle it.
	ptpos ^= direct;

	// Another directional problem: if we are searching forward, use the forward pattern in mcpat[]; otherwise, use the
	// backward (reversed) pattern in bmcpat[].
	mcpatrn = (direct == FORWARD) ? srch.mcpat : srch.bmcpat;

	// Set local scan variable to dot.
	scandot = curwp->w_face.wf_dot;

	// Scan the buffer until we find the nth match or hit a buffer boundary.
	while(!boundary(&scandot,direct)) {

		// Save the current position in case we need to restore it on a match, and initialize matchlen to zero
		// so that amatch() can compute the match length.
		srch.matchdot = scandot;
		srch.matchlen = 0;

		// Scan the buffer for a match.
		if(amatch(mcpatrn,direct,&scandot)) {

			// A SUCCESSFULL MATCH!  Flag that we have moved, update the dot pointers, and save the match.
			// Leave point at end of string if PTEND; otherwise, beginning.
			curwp->w_flags |= WFMOVE;
			curwp->w_face.wf_dot = (ptpos == PTEND) ? scandot : srch.matchdot;

			// If we were heading backward, set the "match" pointers to the beginning of the string.
			if(direct == BACKWARD)
				srch.matchdot = scandot;
			if(savematch() != SUCCESS)
				return rc.status;

			// Return if nth match found.
			if(--n <= 0)
				return rc.status;
			}
		else
			// Advance the cursor.
			(void) nextch(&scandot,direct);
		}

	// No match found.
	(void) rcset(SUCCESS,RCNOWRAP,text79);
			// "Not found"
	return NOTFOUND;
	}

#if MMDEBUG & DEBUG_SHOWRE
// Build and pop up a buffer containing the search and replacement metacharacter arrays.  Render buffer and return status.
int showRegExp(Value *rp,int n) {
	uint m;
	Buffer *srlistp;
	MetaChar *mcp;
	ReplMetaChar *rmcp;
	StrList rpt;
	char *bmp,*strp,*litp,wkbuf[NWORK];
	struct {
		char *hdr;
		MetaChar *pat;
		} mcobj[] = {
			{text307,srch.mcpat},
				// "FORWARD"
			{text308,srch.bmcpat},
				// "BACKWARD"
 			{NULL,NULL}},*mcobjp = mcobj;

	// Get a buffer for the listing and a string list.
	if(sysbuf(text306,&srlistp) != SUCCESS)
			// "RegExpList"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	do {
		// Construct the search header lines.
		if(mcobjp->pat == srch.bmcpat && vputs("\r\r",&rpt) != 0)
			return vrcset();
		if(vputf(&rpt,"%s %s %s /",mcobjp->hdr,text309,text311) != 0 || vstrlit(&rpt,srch.pat,0) != 0 ||
		 vputs("/\r",&rpt) != 0)
						// "SEARCH","PATTERN"
			return vrcset();

		// Loop through search pattern.
		mcp = mcobjp->pat;
		for(;;) {
			strp = stpcpy(wkbuf,"    ");

			// Any closure modifiers?
			if(mcp->mc_type & MCE_ALLCLOSURE) {
				strp = stpcpy(wkbuf,"    ");
				strp = stpcpy(strp,(mcp->mc_type & MCE_CLOSURE0) ? "Zero or more" :
				 (mcp->mc_type & MCE_CLOSURE1) ? "One or more" : "Zero or one");
				*strp++ = ' ';
				if(mcp->mc_type & MCE_MINCLOSURE)
					strp = stpcpy(strp,"(minimum) ");
				strcpy(strp,"of:\r");
				if(vputs(wkbuf,&rpt) != 0)
					return vrcset();
				strp = stpcpy(wkbuf,"        ");
				}

			// Describe metacharacter and any value.
			switch(mcp->mc_type & MCE_BASETYPE) {
				case MCE_NIL:
					litp = "NIL";
					goto lit;
				case MCE_LITCHAR:
					sprintf(strp,"%-14s'%c'","Char",mcp->u.lchar);
					break;
				case MCE_ANY:
					litp = "Any";
					goto lit;
				case MCE_CCL:
					litp = "ChClass      ";
					goto ccl;
				case MCE_NCCL:
					litp = "NegChClass   ";
ccl:
					if(vputs(wkbuf,&rpt) != 0 || vputs(litp,&rpt) != 0)
						return vrcset();
					bmp = (char *) mcp->u.cclmap + sizeof(EBitMap);
					m = 0;
					do {
						if(m ^= 1)
							if(vputc(' ',&rpt) != 0)
								return vrcset();
						if(vputf(&rpt,"%.2x",*--bmp) != 0)
							return vrcset();
						} while(bmp > (char *) mcp->u.cclmap);
					if(vputc('\r',&rpt) != 0)
						return vrcset();
					strp = NULL;
					break;
				case MCE_BOL:
					litp = "BeginLine";
					goto lit;
				case MCE_EOL:
					litp = "EndLine";
lit:
					strcpy(strp,litp);
					break;
				case MCE_GRPBEGIN:
					litp = "GroupBegin";
					goto grp;
				case MCE_GRPEND:
					litp = "GroupEnd";
grp:
					sprintf(strp,"%-14s%3u",litp,(uint) (mcp->u.ginfo - groups));
				}

			// Add the line to the string list.
			if(strp != NULL && (vputs(wkbuf,&rpt) != 0 || vputc('\r',&rpt) != 0))
				return vrcset();

			// Onward to the next element.
			if(mcp->mc_type == MCE_NIL)
				break;
			++mcp;
			}
		} while((++mcobjp)->hdr != NULL);

	// Construct the replacement header lines.
	if(vputf(&rpt,"\r\r%s %s /",text310,text311) != 0 || vstrlit(&rpt,srch.rpat,0) != 0 ||
	 vputs("/\r",&rpt) != 0)
				// "SEARCH","PATTERN"
		return vrcset();

	// Loop through replacement pattern.
	rmcp = srch.rmcpat;
	for(;;) {
		strp = stpcpy(wkbuf,"    ");

		// Describe metacharacter and any value.
		switch(rmcp->mc_type) {
			case MCE_NIL:
				strcpy(strp,"NIL");
				break;
			case MCE_LITSTRING:
				if(vputs(wkbuf,&rpt) != 0 || vputf(&rpt,"%-14s'","String") != 0 ||
				 vstrlit(&rpt,rmcp->u.rstr,0) != 0 || vputs("'\r",&rpt) != 0)
					return vrcset();
				strp = NULL;
				break;
			case MCE_GROUP:
				sprintf(strp,"%-14s%3d","Group",rmcp->u.grpnum);
				break;
			case MCE_DITTO:
				strcpy(strp,"Matched string");
			}

		// Add the line to the string list.
		if(strp != NULL && (vputs(wkbuf,&rpt) != 0 || vputc('\r',&rpt) != 0))
			return vrcset();

		// Onward to the next element.
		if(rmcp->mc_type == MCE_NIL)
			break;
		++rmcp;
		}

	// Add the results to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(bappend(srlistp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,srlistp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}
#endif
