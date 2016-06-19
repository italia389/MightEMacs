// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// search.c	Search functions for MightEMacs.
//
// These functions implement commands that search in the forward and backward directions.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

#define max(a,b)	((a < b) ? b : a)

// Check if given search pattern has trailing option characters and return results.  Flags in *flagsp are initially cleared,
// then if any options were found, the appropriate flags are set and the source string is truncated at the OPTCH_BEGIN
// character.  If the option lead-in character (OPTCH_BEGIN) is doubled up, as in "x::e", then one of the OPTCH_BEGIN characters
// is removed from the pattern and the SOPT_LIT flag is set (which is used by mkpat() to rebuild the original pattern).  The
// final pattern length is returned.
int chkopts(char *patp,ushort *flagsp) {
	char *optp = strrchr(patp,OPTCH_BEGIN);
	int patlen = strlen(patp);			// Assume no options exist.
	ushort flags;

	*flagsp &= ~SOPT_ALL;				// Clear flags.
	if(optp == NULL || optp == patp)		// No option lead-in char or at beginning of pattern?
		return patlen;				// Yep, it's a no-go.

	patp = ++optp;					// No, check for lower-case letters.
	while(is_lower(*patp))
		++patp;
	if(patp == optp || *patp != '\0')		// No lowercase letters or other chars follow?
		return patlen;				// Yep, it's a no go.

	// Possible option string found.  Parse it.
	patp = optp - 1;
	ushort c;
	struct {
		ushort optch;		// Option character.
		ushort flag;		// Option flag.
		ushort xflags;		// Duplicate and conflicting flags.
		} *flgp,flg[] = {
			{OPTCH_MULTI,SOPT_MULTI,SOPT_MULTI},
			{OPTCH_IGNORE,SOPT_IGNORE,SOPT_EXACT | SOPT_IGNORE},
			{OPTCH_EXACT,SOPT_EXACT,SOPT_EXACT | SOPT_IGNORE},
			{OPTCH_REGEXP,SOPT_REGEXP,SOPT_PLAIN | SOPT_REGEXP},
			{OPTCH_PLAIN,SOPT_PLAIN,SOPT_PLAIN | SOPT_REGEXP},
			{0,0,0}};
	flags = 0;
	do {
		c = *optp++;
		flgp = flg;
		do {
			if(c == flgp->optch) {
				if(flags & flgp->xflags)	// Duplicate or conflicting flags?
					break;			// Yes, bail out.
				flags |= flgp->flag;
				goto next;
				}
			} while((++flgp)->optch != 0);

		// Invalid char found ... assume it's part of the pattern.
		return patlen;
next:;
		} while(*optp != '\0');

	// Looks like a valid option string.  Check if OPTCH_BEGIN is doubled and fix up results if so.
	if(patp[-1] == OPTCH_BEGIN) {
		strcpy(patp,patp + 1);		// Shift option string left one byte.
		*flagsp |= SOPT_LIT;
		return patlen - 1;
		}

	// Success.  Truncate pattern, update flags, and return new length.
	*patp = '\0';
	*flagsp |= flags;
	return patlen - (optp - patp);
	}

// Return true if buffer Match record specifies Exact search mode.
bool exactbmode(void) {

	return (srch.m.flags & SOPT_EXACT) || ((modetab[MDR_GLOBAL].flags & MDEXACT) && !(srch.m.flags & SOPT_IGNORE));
	}

// Return true if buffer Match record specifies Regexp search mode.
bool rebmode(void) {

	return (srch.m.flags & SOPT_REGEXP) || ((modetab[MDR_GLOBAL].flags & MDREGEXP) && !(srch.m.flags & SOPT_PLAIN));
	}

// Return true if buffer Match record specifies plain text search (post-pattern-compile, if so).
bool plainsearch(void) {

	return !rebmode() || !(srch.m.flags & SREGICAL);
	}

// Delta1 table: delta1[c] contains the distance between the last character of pat and the rightmost occurrence of c in pat.  If
// c does not occur in pat, then delta1[c] = patlen.  If c is at string[i] and c != pat[patlen - 1], we can safely shift i over
// by delta1[c], which is the minimum distance needed to shift pat forward to get string[i] lined up with some character in pat.
// This algorithm runs in alphabet_len + patlen time.
static void mkdelta1(int *delta1,char *pat) {
	int i;
	bool exact = exactbmode();
	int patlen1 = srch.m.patlen - 1;

	// First, set all characters to the maximum jump.
	for(i = 0; i < HICHAR; ++i)
		delta1[i] = srch.m.patlen;

	// Now, set the characters in the pattern.  Set both cases of letters if "exact" mode is false.
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
 
	// If "Exact" mode is false, set values in this table to the minimum jump for each pattern index which will advance the
	// search by one position -- it can't be used in the usual way.
	if(!exactbmode()) {
		int i = srch.m.patlen;

		// Set the first "patlen" entries to zero.
		do {
			*delta2++ = i--;
			} while(i > 0);
		}
	else {
		int i;
		int patlen1 = srch.m.patlen - 1;
		int last_prefix_index = patlen1;

		// First loop.
		i = srch.m.patlen;
		do {
			if(isprefix(pat,srch.m.patlen,i))
				last_prefix_index = i;
			--i;
			delta2[i] = last_prefix_index + (patlen1 - i);
			} while(i > 0);

		// Second loop.
		for(i = 0; i < patlen1; ++i) {
			int slen = suffix_length(pat,srch.m.patlen,i);
			if(pat[i - slen] != pat[patlen1 - slen])
				delta2[patlen1 - slen] = patlen1 - i + slen;
			}
		}
	}

// Make delta tables for non-Regexp (plain text) search.  Return status.
void mkdeltas(void) {

	srch.m.grpct = 0;
	mkdelta1(srch.fdelta1,srch.m.pat);
	mkdelta2(srch.fdelta2,srch.m.pat);
	mkdelta1(srch.bdelta1,srch.bpat);
	mkdelta2(srch.bdelta2,srch.bpat);

	// Remember type of compile.
	if(exactbmode())
		srch.m.flags |= SCPL_EXACT;
	else
		srch.m.flags &= ~SCPL_EXACT;
	}

// Clear search groups in given match object.  Note that mtp->grpct is not set to zero because the RE pattern associated with
// this match object may still be compiled and usable (and uses mtp->grpct during scanning).  This can happen in replstr(), for
// example.
void grpclear(Match *mtp) {
	GrpInfo *gip,*gipz;

	// Do one loop minimum so that group 0 (the entire match) is freed.
	gipz = (gip = mtp->groups) + mtp->grpct;
	do {
		vnull(gip->matchp);
		} while(gip++ < gipz);
	}

// Free up any CCL bitmaps in the Regexp search arrays contained in the given Match record and mark it as cleared.
void mcclear(Match *mtp) {
	MetaChar *mcp;

	mcp = mtp->mcpat;
	while(mcp->mc_type != MCE_NIL) {
		if((mcp->mc_type == MCE_CCL) || (mcp->mc_type == MCE_NCCL))
			free((void *) mcp->u.cclmap);
		++mcp;
		}

	mtp->mcpat[0].mc_type = mtp->bmcpat[0].mc_type = MCE_NIL;
	}

// Free all search pattern heap space in given match object.
void freespat(Match *mtp) {

	mcclear(mtp);
	(void) free((void *) mtp->pat);
	(void) free((void *) mtp->mcpat);
	(void) free((void *) mtp->bmcpat);
	mtp->ssize = 0;
	}

// Initialize parameters for new search pattern, which may be null.  If flagsp is NULL, check for pattern options via chkopts();
// otherwise, use *flagsp.  Return status.
int newspat(char *patp,Match *mtp,ushort *flagsp) {

	// Get and strip search options, if any.
	if(*patp != '\0') {
		if(flagsp == NULL)
			mtp->patlen = chkopts(patp,&mtp->flags);
		else {
			mtp->patlen = strlen(patp);
			mtp->flags = (mtp->flags & ~SOPT_ALL) | *flagsp;
			}
		}

	// Free up arrays if too big.
	if(mtp->ssize > NPATMAX || (mtp->ssize > 0 && (uint) mtp->patlen > mtp->ssize)) {
		freespat(mtp);
		if(mtp == &srch.m) {
			(void) free((void *) srch.bpat);
			(void) free((void *) srch.fdelta2);
			(void) free((void *) srch.bdelta2);
			}
		}

	// Get heap space for arrays if needed.
	if(mtp->ssize == 0) {
		mtp->ssize = mtp->patlen < NPATMIN ? NPATMIN : mtp->patlen;
		if((mtp->pat = (char *) malloc(mtp->ssize + 1)) == NULL ||
		 (mtp->mcpat = (MetaChar *) malloc((mtp->ssize + 1) * sizeof(MetaChar))) == NULL ||
		 (mtp->bmcpat = (MetaChar *) malloc((mtp->ssize + 1) * sizeof(MetaChar))) == NULL ||
		 (mtp == &srch.m &&
		 ((srch.bpat = (char *) malloc(mtp->ssize + 1)) == NULL ||
		 (srch.fdelta2 = (int *) malloc(mtp->ssize * sizeof(int))) == NULL ||
		 (srch.bdelta2 = (int *) malloc(mtp->ssize * sizeof(int))) == NULL)))
			return rcset(PANIC,0,text94,"newspat");
					// "%s(): Out of memory!"
		mtp->mcpat[0].mc_type = mtp->bmcpat[0].mc_type = MCE_NIL;
		}

	// Save search string.
	strcpy(mtp->pat,patp);
	if(mtp == &srch.m) {
		strrev(strcpy(srch.bpat,mtp->pat));
		srch.fdelta1[0] = -1;			// Clear delta tables.
		}
	mcclear(mtp);					// Clear Regexp search table.
	return rc.status;
	}

// Make original search pattern from given Match record and store in destp.  Return destp.
char *mkpat(char *destp,Match *mtp) {
	struct {
		ushort optch;
		ushort flag;
		} *flgp,flg[] = {
			{OPTCH_IGNORE,SOPT_IGNORE},
			{OPTCH_EXACT,SOPT_EXACT},
			{OPTCH_REGEXP,SOPT_REGEXP},
			{OPTCH_PLAIN,SOPT_PLAIN},
			{OPTCH_MULTI,SOPT_MULTI},
			{0,0}};
	char *strp = stpcpy(destp,mtp->pat);
	if(mtp->flags & SOPT_LIT) {
		char *strp1 = strp + 1;
		do
			*strp1-- = *strp--;
			while(*strp != OPTCH_BEGIN);
		*strp1 = OPTCH_BEGIN;
		}
	else if(mtp->flags & SOPT_ALL) {
		*strp++ = OPTCH_BEGIN;
		flgp = flg;
		do {
			if(mtp->flags & flgp->flag)
				*strp++ = flgp->optch;
			} while((++flgp)->optch != 0);
		*strp = '\0';
		}
	return destp;
	}

// Read a search or replacement pattern delimited by global variable srch.sdelim and stash it in srch.pat (srchpat is true) or
// srch.rpat (srchpat is false) via newspat() or newrpat().  The pattern variable is not updated if the user types in an empty
// line.  If that happens and there is no old pattern, it is an error.  Return status.
int readpattern(char *prmtp,bool srchpat) {
	Value *tpatp;
	char patbuf[srch.m.patlen + OPTCH_N + 1];

	if(vnew(&tpatp,false) != 0)
		return vrcset();

	// Read a pattern.  Either we get one, or we just get the input terminator and use the previous pattern.
	if(opflags & OPSCRIPT) {
		if(havesym(s_any,false) && (funcarg(tpatp,srchpat ? ARG_FIRST | ARG_STR : ARG_STR) != SUCCESS))
			return rc.status;
		if(vistfn(tpatp,VNIL))
			vnull(tpatp);
		}
	else if(terminp(tpatp,prmtp,srchpat ? mkpat(patbuf,&srch.m) : srch.m.rpat,srch.sdelim,0,0) != SUCCESS)
		return rc.status;

	// Validate pattern.
	if(vistfn(tpatp,VBOOL))
		return rcset(FAILURE,0,text358,text360);
			// "Illegal use of %s value","Boolean"

	// Check search string.
	if(visnull(tpatp) && srchpat)
		return rcset(FAILURE,0,text80);		// Null search string not allowed.
				// "No pattern set"
	// New search pattern?
	if(srchpat) {
		ushort flags = 0;
		(void) chkopts(tpatp->v_strp,&flags);
		if(flags != (srch.m.flags & SOPT_ALL) || strcmp(tpatp->v_strp,srch.m.pat) != 0) {

			// Save the new search pattern.
			(void) newspat(tpatp->v_strp,&srch.m,&flags);
			}
		}
	else if(strcmp(tpatp->v_strp,srch.m.rpat) != 0) {

		// Save the new replacement pattern.
		(void) newrpat(tpatp->v_strp,&srch.m);
		}

	return rc.status;
	}

// Return true if given dot position is at a boundary (beginning or end of buffer); otherwise, false.
int boundary(Dot *dotp,int dir) {

	return (dir == FORWARD) ? dotp->lnp == curbp->b_hdrlnp : dotp->off == 0 && lback(dotp->lnp) == curbp->b_hdrlnp;
	}

// Create search tables if needed.
static int mktab(void) {

	// Clear search groups.
	grpclear(&srch.m);

	// Compile pattern as an RE if requested.
	if(rebmode() && srch.m.mcpat[0].mc_type == MCE_NIL && mccompile(&srch.m) != SUCCESS)
		return rc.status;

	// Compile as a plain-text pattern if not an RE request or not really an RE (SREGICAL not set).
	if(plainsearch() && (srch.fdelta1[0] == -1 || (((srch.m.flags & SCPL_EXACT) != 0) != exactbmode())))
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

	// Make sure a pattern exists, or that we didn't switch into Regexp mode after the pattern was entered.
	if(srch.m.pat[0] == '\0')
		return rcset(FAILURE,0,text80);
			// "No pattern set"

	// Create search tables if needed.
	if(mktab() != SUCCESS)
		return rc.status;

	// Perform appropriate search and return result.
	return
	 vsetstr((((rebmode() && (srch.m.flags & SREGICAL)) ? mcscan(n,FORWARD) : scan(n,FORWARD)) == NOTFOUND) ?
	  val_false : srch.m.matchp->v_strp,rp) != 0 ? vrcset() : rc.status;
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
	return
	 vsetstr((((rebmode() && (srch.m.flags & SREGICAL)) ? mcscan(n,BACKWARD) : scan(n,BACKWARD)) == NOTFOUND) ?
	  val_false : srch.m.matchp->v_strp,rp) != 0 ? vrcset() : rc.status;
	}

// Compare two characters ("bc" from the buffer, and "pc" from the pattern) and return Boolean result.  If "exact" is false,
// fold the case.
static bool eq(int bc,int pc,bool exact) {

	if(!exact) {
		bc = lowcase[bc];
		pc = lowcase[pc];
		}

	return bc == pc;
	}

// Retrieve the next or previous character in a buffer or string, and advance or retreat the scanning dot.  The order in which
// this is done is significant and depends upon the direction of the search.  Forward searches fetch and advance; reverse
// searches back up and fetch.  Return -1 if a buffer or string boundary is hit.
static int nextch(ScanDot *sdotp,int direct) {
	int c;

	if(sdotp->type == STRDOT) {
		StrDot *sdp = &sdotp->u.sd;
		if(direct == FORWARD)
			c = (sdp->strp[0] == '\0' && sdp->strp > sdp->strp0 && sdp->strp[-1] == '\0') ? -1 : *sdp->strp++;
		else
			c = (sdp->strp == sdp->strp0) ? -1 : *--sdp->strp;
		}
	else {
		Dot *dotp = &sdotp->u.bd;
		if(direct == FORWARD) {					// Going forward?
			if(dotp->lnp == curbp->b_hdrlnp)		// Yes.  At bottom of buffer?
				c = -1;					// Yes, return -1;
			else if(dotp->off == lused(dotp->lnp)) {	// No.  At EOL?
				dotp->lnp = lforw(dotp->lnp);		// Yes, skip to next line...
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
				dotp->lnp = lback(dotp->lnp);		// No.  Skip to previous line...
				dotp->off = lused(dotp->lnp);
				c = '\r';				// and return a <NL>.
				}
			}
		else
			c = lgetc(dotp->lnp,--dotp->off);		// Not at BOL ... return previous char.
		}
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

// Save the pattern that was found in given Match record.  Return status.
int savematch(Match *mtp) {
	int ct;
	GrpInfo *gip;
	long len;
	Region *regp;
	StrLoc *slp;

	// Set up group 0 (the entire match).
	gip = mtp->groups;
	gip->elen = 0;

	// Save the pattern match (group 0) and all the RE groups, if any.
	ct = 0;
	do {
		if(mtp == &srch.m) {
			regp = &gip->ml.reg;
			len = (regp->r_size += gip->elen);
			}
		else {
			slp = &gip->ml.str;
			len = (slp->len += gip->elen);
			}

		if((gip->matchp == NULL && vnew(&gip->matchp,false) != 0) || vsalloc(gip->matchp,len + 1) != 0)
			return vrcset();
		if(mtp == &srch.m)
			regcpy(gip->matchp->v_strp,regp);
		else
			stplcpy(gip->matchp->v_strp,slp->sd.strp,len + 1);
		++gip;
		} while(++ct <= mtp->grpct);

	mtp->matchp = mtp->groups[0].matchp;	// Set new (heap) pointer.
	if(mtp == &srch.m || mtp == &rematch)
		lastMatch = mtp->matchp;

	return rc.status;
	}

#define SHOW_COMPARES	0

// Search for a pattern in either direction.  If found, position dot at the start or just after the match string and (perhaps)
// repaint the display.  Fast version using code from Boyer and Moore, Software Practice and Experience, vol. 10 (1980).  "n" is
// repeat count and "direct" is FORWARD or BACKWARD.  Return NOTFOUND (bypassing rcset()) if search failure.
int scan(int n,int direct) {
	Match *mtp = &srch.m;
	Region *regp = &mtp->groups[0].ml.reg;
	int bc,pc;		// Buffer char and pattern char.
	int pati;		// Pattern index.
	char *patp,*patpz;	// Pattern pointers.
	char *pattern;		// String to scan for.
	ScanDot sdot;		// Current line and offset during scanning.
	int sdirect;		// Scan direction.
	int *delta1,*delta2;
	int jumpsz;		// Next offset.
	bool exact = exactbmode();
#if SHOW_COMPARES
	int compares = 0;
#endif
	// Set scanning direction for pattern matching, which is always in the opposite direction of the search.
	sdirect = direct ^ 1;

	// Another directional problem: if we are searching forward, use the backward (reversed) pattern in bpat[], and the
	// forward delta tables; otherwise, use the forward pattern in pat[], and the backward delta tables.  This is so the
	// pattern can always be scanned from left to right (until patlen characters have been examined) during comparisions.
	if(direct == FORWARD) {
		pattern = srch.bpat;
		delta1 = srch.fdelta1;
		delta2 = srch.fdelta2;
		}
	else {
		pattern = mtp->pat;
		delta1 = srch.bdelta1;
		delta2 = srch.bdelta2;
		}

	// Set local scan variable to dot.
	sdot.type = BUFDOT;
	sdot.u.bd = curwp->w_face.wf_dot;

	// Scan the buffer until we find the nth match or hit a buffer boundary.  The basic loop is to jump forward or backward
	// in the buffer (so that comparisons are done in reverse), check for a match, get the next jump size (delta) on a
	// failure, and repeat.  The initial jump is the full pattern length so that the first character returned by nextch() is
	// at the far end of a possible match, depending on the search direction.
	regp->r_size = jumpsz = mtp->patlen;
	patpz = pattern + mtp->patlen;
	while(bjump(jumpsz,&sdot.u.bd,direct)) {

		// Save the current position in case we match the search string at this point.
		regp->r_dot = sdot.u.bd;

		// Scan through the pattern for a match.  Note that nextch() will never hit a buffer boundary (and return -1)
		// because bjump() checks for that.
		patp = pattern;
		pati = mtp->patlen;
#if SHOW_COMPARES
		++compares;
#endif
		do {
			pc = *patp;
			--pati;
			if(!eq(pc,bc = nextch(&sdot,sdirect),exact)) {

				// No match.  Jump forward or backward in buffer as far as possible and try again.
				// Use delta + 1 so that nextch() begins at the right spot.
				jumpsz = max(delta1[bc],delta2[pati]) + 1;
				goto fail;
				}
			} while(++patp < patpz);

		// A SUCCESSFULL MATCH!  Flag that we have moved, update the dot pointers, and save the match.  Need to do this
		// now because the nth match may not be found, in which case we want to leave the dot at the last one.
		curwp->w_flags |= WFMOVE;
		curwp->w_face.wf_dot = regp->r_dot;		// Leave point at end of string.

		// If we were heading forward, set the "match" pointers to the beginning of the string.
		if(direct == FORWARD)
			regp->r_dot = sdot.u.bd;
		if(savematch(&srch.m) != SUCCESS)
			return rc.status;

		// Return if nth match found.
		if(--n <= 0)
#if SHOW_COMPARES
			return rcset(SUCCESS,0,"%d comparisons",compares);
#else
			return rc.status;
#endif

		// Nth match not found.  Reset and continue scan.
		jumpsz = mtp->patlen * 2;
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

// Set a bit range (ON only) in the bitmap.
static void setbitrange(int bc1,int bc2,EBitMap *cclmap) {

	do {
		setbit(bc1,cclmap);
		} while(++bc1 <= bc2);
	}

// Set beginning portion of bit range that turned out to not be one.
static void setnorange(int bc,EBitMap *cclmap) {

	setbit(bc,cclmap);
	setbit(MC_CCLRANGE,cclmap);
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
			// "Empty character class"

	// Loop through characters.  Ranges are not processed until the atom (single character or \c) following the '-' is
	// parsed.  After each atom is determined, if ochr > 0 then it is the end of a range; otherwise, it's just a singleton.
	for(ochr = -1; pchr != MC_CCLEND && pchr != '\0'; pchr = *++patp) {
		switch(pchr) {

			// The range character loses its meaning if it is the first or last character in the class.  We also
			// treat it as ordinary if the range is invalid or the order is wrong; e.g., "\d-X" or "z-a".
			case MC_CCLRANGE:
				if(patp[1] == '\0')
					goto err;
				if(ochr < 0)
					goto plain;
				if(patp[1] == MC_CCLRANGE) {		// [c--...]
					setnorange(ochr,bmap);
					goto clear;
					}
				continue;
			case MC_ESC:
				switch(pchr = *++patp) {
					case MC_TAB:
						pchr = '\t';
						break;
					case MC_CR:
						pchr = '\r';
						break;
					case MC_NL:
						pchr = '\n';
						break;
					case MC_FF:
						pchr = '\f';
						break;
					case MC_DIGIT:
						setbitrange('0','9',bmap);
						goto mult;
					case MC_LETTER:
						setbitrange('a','z',bmap);
						setbitrange('A','Z',bmap);
						goto mult;
					case MC_SPACE:
						setbit(' ',bmap);
						setbit('\t',bmap);
						setbit('\r',bmap);
						setbit('\n',bmap);
						setbit('\f',bmap);
						goto mult;
					case MC_WORD:
						{char *strp,*strpz;

						// Find the current word chars and set those bits in the bit map.
						strpz = (strp = wordlist) + 256;
						do {
							if(*strp)
								setbit(strp - wordlist,bmap);
							} while(++strp < strpz);
						}
mult:
						if(ochr > 0)
							setnorange(ochr,bmap);
						goto clear;
					case '\0':
						goto err;
					}
				// Fall through.
			default:
				if(ochr > 0) {					// Current char is end of a range.
					if(pchr < ochr) {			// Wrong order?
						setnorange(ochr,bmap);		// Yes, treat as plain chars.
						goto plain;
						}
					setbitrange(ochr,pchr,bmap);		// Valid range ... set bit(s).
					goto clear;
					}
				if(patp[1] == MC_CCLRANGE)			// Not at end of range.  At beginning?
					ochr = pchr;				// Yes, save current char for later.
				else {						// No, process as plain char.
plain:
					setbit(pchr,bmap);
clear:
					ochr = -1;
					}
			}
		}
	if(ochr > 0)
		setnorange(ochr,bmap);		// Char class ended with MC_CCLRANGE.

	*patpp = patp;
	if(pchr == '\0')
err:
		(void) rcset(FAILURE,0,text97);
			// "Character class not ended"

	return rc.status;
	}

// Convert the RE string in the given match record to a forward MetaChar array rooted in the "mcpat" variable and a backward
// MetaChar array rooted in the "bmcpat" variable.  Return status.
//
// Note: A closure symbol is taken as a literal character when (1), it is the first character in the pattern; or (2), when
// preceded by a symbol that does not allow closure, such as beginning or end of line symbol, or another closure symbol.
// However, it is treated as an error if preceded by a group-end symbol because it should be legal, but it is not supported by
// this implementation.
int mccompile(Match *mtp) {
	MetaChar *mcp,*bmcp;
	char *patp,*strp;
	int pc;
	int clmin,clmax;
	ushort typ;
	bool lastDoesClosure = false;
	int grpstack[MAXGROUPS];
	int stacklevel = 0;

	mcp = mtp->mcpat;
	patp = mtp->pat;
	mtp->flags &= ~SREGICAL;
	mtp->grpct = 0;

	for(; (pc = *patp) != '\0'; ++mcp,++patp) {
		switch(pc) {
			case MC_CCLBEGIN:
				if(cclmake(&patp,mcp) != SUCCESS)
					goto clexit;
				goto ccl;
			case MC_BOL:
				typ = MCE_BOL;
				goto set;
			case MC_EOL:
				typ = MCE_EOL;
				goto set;
			case MC_ANY:
				lastDoesClosure = true;
				typ = MCE_ANY;
				goto set;
			case MC_CLOSURE0:	// *
				clmin = 0;
				goto checkcl;
			case MC_CLOSURE1:	// +
				clmin = 1;
checkcl:
				// Does the closure symbol mean closure here?  If so, back up to the previous element and
				// mark it as such.
				if(lastDoesClosure) {
					clmax = -1;
					goto docl;
					}
				goto badcl;
			case MC_CLOSURE01:	// ?

				// Does the zero-or-one closure symbol mean closure here?
				if(lastDoesClosure) {
					clmin = 0;
					clmax = 1;
					}
				else {
					// Not closure.  Is it closure modification?  If so, back up to the previous element and
					// mark it as such.
					if(mcp == mtp->mcpat || !(mcp[-1].mc_type & MCE_CLOSURE))
						goto badcl;
					--mcp;
					mcp->mc_type |= MCE_MINCLOSURE;
					break;
					}
				goto docl;
			case MC_CLBEGIN:

				// Does the begin-closure-count symbol mean closure here?
				if(!lastDoesClosure) {
					if(patp[1] >= '0' && patp[1] <= '9')
						goto badcl;
					goto litchar;
					}

				// Yes, get integer value(s).
				clmin = -1;
				for(;;) {
					if(*++patp == '\0' || *patp < '0' || *patp > '9')
						goto badcl;
					clmax = 0;
					do {
						clmax = clmax * 10 + (*patp++ - '0');
						} while(*patp >= '0' && *patp <= '9');
					if(clmin == -1) {
						clmin = clmax;
						if(patp[0] != ',')
							break;
						if(patp[1] == MC_CLEND) {
							clmax = -1;
							++patp;
							goto docl;
							}
						}
					else {
						if(clmax == 0 || clmax < clmin)
							goto badcl;
						break;
						}
					}
				if(*patp != MC_CLEND)
					goto badcl;
docl:
				// Back up...
				--mcp;

				// and check for closure on a group (not supported).
				if(mcp->mc_type == MCE_GRPEND) {
					(void) rcset(FAILURE,0,text304,mtp->pat);
							// "Closure on group not supported in RE pattern '%s'"
					goto clexit;
					}

				// All is well ... add closure info.
				mcp->mc_type |= MCE_CLOSURE;
				mcp->cl.min = clmin;
				mcp->cl.max = clmax;
				lastDoesClosure = false;
				goto sreg;
badcl:
				(void) rcset(FAILURE,0,text255,mtp->pat);
						// "Invalid repetition operand in RE pattern '%s'"
				goto clexit;
			case MC_GRPBEGIN:

				// Beginning of a group.  Check count.
				if(++mtp->grpct < MAXGROUPS) {
					mcp->u.ginfo = mtp->groups + mtp->grpct;
					grpstack[stacklevel++] = mtp->grpct;
					lastDoesClosure = false;
					typ = MCE_GRPBEGIN;
					goto set;
					}
				(void) rcset(FAILURE,0,text221,mtp->pat,MAXGROUPS);
						// "Too many groups in RE pattern '%s' (maximum is %d)"
				goto clexit;
			case MC_GRPEND:

				// End of a group.  Check if no group left to close.
				if(stacklevel > 0) {
					mcp->u.ginfo = mtp->groups + grpstack[--stacklevel];
					typ = MCE_GRPEND;
					goto set;
					}
				(void) rcset(FAILURE,0,text258,mtp->pat);
						// "Unmatched right paren in RE pattern '%s'"
				goto clexit;
			case MC_ESC:
				mtp->flags |= SREGICAL;
				switch(pc = *++patp) {
					case '\0':
						pc = '\\';
						--patp;
						break;
					case MC_DIGIT:
						strp = "[\\d]";
						goto ch_ccl;
					case MC_NDIGIT:
						strp = "[^\\d]";
						goto ch_ccl;
					case MC_TAB:
						pc = '\t';
						break;
					case MC_CR:
						pc = '\r';
						break;
					case MC_SPACE:
						strp = "[\\s]";
						goto ch_ccl;
					case MC_NSPACE:
						strp = "[^\\s]";
						goto ch_ccl;
					case MC_LETTER:
						strp = "[\\l]";
						goto ch_ccl;
					case MC_NLETTER:
						strp = "[^\\l]";
						goto ch_ccl;
					case MC_WORD:
						strp = "[\\w]";
						goto ch_ccl;
					case MC_NWORD:
						strp = "[^\\w]";
ch_ccl:
						if(cclmake(&strp,mcp) != SUCCESS)
							goto clexit;
ccl:
						lastDoesClosure = true;
						goto sreg;
					case MC_NL:
						pc = '\n';
						break;
					case MC_FF:
						pc = '\f';
						break;
					case MC_WORDBND:
						typ = MCE_WORDBND;
						goto set;
					case MC_NWORDBND:
						typ = MCE_WORDBND | MCE_NOT;
						goto set;
					case MC_BOS:
						typ = MCE_BOS;
						goto set;
					case MC_EOS:
						typ = MCE_EOS;
						goto set;
					case MC_EOSALT:
						typ = MCE_EOSALT;
set:
						mcp->mc_type = typ;
sreg:
						mtp->flags |= SREGICAL;
						continue;
					}
				goto litchar;
			default:	// Literal character.
litchar:
				mcp->mc_type = MCE_LITCHAR;
				mcp->u.lchar = pc;
				lastDoesClosure = true;
				break;
			}
		}

	// Check RE group level.
	if(stacklevel) {
		(void) rcset(FAILURE,0,text222);
			// "RE group not ended"
		goto clexit;
		}

	// No error: terminate the forward array, make the reverse array, and return success.
	mcp->mc_type = MCE_NIL;
	bmcp = mtp->bmcpat;
	do {
		*bmcp++ = *--mcp;
		} while(mcp > mtp->mcpat);
	bmcp->mc_type = MCE_NIL;
	return rc.status;
clexit:
	// Error: clear the MetaChar array and return exception status.
	mcp[1].mc_type = MCE_NIL;
	mcclear(mtp);
	return rc.status;
	}

// Is the character set in the bitmap?
static bool biteq(int bc,EBitMap *cclmap) {

	if((unsigned) bc >= HICHAR)
		return false;

	return ((*((char *) cclmap + (bc >> 3)) & BIT(bc & 7)) ? true : false);
	}


// Perform meta-character equality test with a character and return Boolean result.  In Kernighan & Plauger's Software Tools,
// this is the omatch() function.  The "buffer boundary" character (-1) never matches anything.
static bool mceq(int c,MetaChar *mt,uint flags) {
	bool result;
	int c1;

	if(c == -1 || c == 0)
		return false;

	switch(mt->mc_type & MCE_BASETYPE) {
		case MCE_LITCHAR:
			result = eq(c,mt->u.lchar,flags & SXEQ_EXACT);
			break;
		case MCE_ANY:
			result = (c != '\r' || (flags & SOPT_MULTI));
			break;
		case MCE_CCL:
			if(!(result = biteq(c,mt->u.cclmap))) {
				if(!(flags & SXEQ_EXACT) && (c1 = chcase(c)) != c)
					result = biteq(c1,mt->u.cclmap);
				}
			break;
		default:	// MCE_NCCL.
			result = (c != '\r' || (flags & SOPT_MULTI)) && !biteq(c,mt->u.cclmap);
			if(!(flags & SXEQ_EXACT) && (c1 = chcase(c)) != c)
				result &= !biteq(c1,mt->u.cclmap);
		}

	return result;
	}

// Return true if no metacharacters in given list would cause a call to nextch().
static bool mcstill(MetaChar *mcp) {

	for(;;) {
		switch(mcp->mc_type) {
		case MCE_NIL:
			return true;
		case MCE_BOL:
		case MCE_EOL:
		case MCE_BOS:
		case MCE_EOSALT:
		case MCE_EOS:
		case MCE_WORDBND:
		case MCE_GRPBEGIN:
		case MCE_GRPEND:
			++mcp;
			break;
		default:
			goto bust;
		}
		}
bust:
	return false;
	}

// Return true if previous (n < 0) or current (n == 0) scan character is a word character.
static bool isWordCh(ScanDot *sdotp,int n) {

	if(sdotp->type == BUFDOT) {
		Dot *dotp = &sdotp->u.bd;
		if(n < 0)
			return dotp->off == 0 ? (lback(dotp->lnp) != curbp->b_hdrlnp && wordlist[015]) :
			 wordlist[(int) lgetc(dotp->lnp,dotp->off - 1)];
		else
			return dotp->off == lused(dotp->lnp) ? dotp->lnp != curbp->b_hdrlnp && wordlist[015] :
			 wordlist[(int) lgetc(dotp->lnp,dotp->off)];
		}
	else {
		StrDot *sdp = &sdotp->u.sd;
		if(n < 0)
			return sdp->strp > sdp->strp0 && wordlist[(int) sdp->strp[-1]];
		else
			return wordlist[(int) *sdp->strp];
		}
	}

// Try to match a meta-pattern at the given position in either direction.  Based on the recursive routine amatch() (for
// "anchored match") in Kernighan & Plauger's "Software Tools".  "mcp" is the pattern, "flags" is FORWARD or BACKWARD and any
// SOPT_ALL or SXEQ_ALL flags, and "scandotp" contains a pointer to the scan dot.  If the given position matches, set *lp to the
// match length, update *sdotp, and return true; otherwise, return false.
static bool amatch(MetaChar *mcp,uint flags,long *lp,ScanDot *sdotp) {
	ScanDot sdot;		// Current dot location during scan.
	int prematchlen;	// Matchlen before a recursive amatch() call.
	int clmatchlen;		// Number of characters matched in a local closure.
	int direct = flags & ~(SOPT_ALL | SXEQ_ALL);

	// Set up local scan pointers and a character counting variable which corrects matchlen on a partial match.
	sdot = *sdotp;
	clmatchlen = 0;

	// Loop through the meta-pattern, laughing all the way.
	while(mcp->mc_type != MCE_NIL) {

		// Is the current meta-character modified by a closure?
		if(mcp->mc_type & MCE_CLOSURE) {
			if(mcp->mc_type & MCE_MINCLOSURE) {
				int clmax;	// Maximum number of characters can match in closure.

				// First, match minimum number possible.
				if(mcp->cl.min > 0) {
					int n = mcp->cl.min;
					do {
						if(!mceq(nextch(&sdot,direct),mcp,flags))
							return false;		// It's a bust.
						++*lp;
						} while(--n > 0);
					}

				// So far, so good.  If max == min, we're done; otherwise, try to match the rest of the pattern.
				// Expand the closure by one if possible for each failure before giving up.
				if((clmax = mcp->cl.max) == mcp->cl.min)
					goto success;
				if(clmax < 0)
					clmax = INT_MAX;
				for(;;) {
					prematchlen = *lp;
					if(amatch(mcp + 1,flags,lp,&sdot))
						goto success;
					*lp = prematchlen;
					if(--clmax < 0 || !mceq(nextch(&sdot,direct),mcp,flags))
						return false;		// It's a bust.
					++*lp;
					}
				}
			else {
				int c;
				int clmin = mcp->cl.min;
				int clmax = mcp->cl.max;

				// First, match as many characters as possible against the current meta-character.
				while(mceq(c = nextch(&sdot,direct),mcp,flags)) {
					++clmatchlen;
					if(--clmax == 0) {
						c = nextch(&sdot,direct);
						break;
						}
					}

				// We are now at the character (c) that made us fail.  Try to match the rest of the pattern and
				// shrink the closure by one for each failure.  If the scan dot is now at a boundary (c == -1),
				// simulate a -1 for the first character returned by nextch() and don't move the scan dot so
				// that we loop in reverse properly.
				++mcp;
				*lp += clmatchlen;

				for(;;) {
					if(clmatchlen < clmin) {
						*lp -= clmatchlen;
						return false;
						}
					if(c == -1) {
						if(mcstill(mcp))
							goto next;
						c = 0;
						--*lp;
						}
					else if(nextch(&sdot,direct ^ 1) == -1)
						--*lp;
					else {
						prematchlen = *lp;
						if(amatch(mcp,flags,lp,&sdot))
							goto success;
						*lp = prematchlen - 1;
						}
					--clmatchlen;
					}
				}
			}
		else
			switch(mcp->mc_type & MCE_BASETYPE) {
			case MCE_GRPBEGIN:
				if(sdot.type == BUFDOT) {
					Region *gregp = &mcp->u.ginfo->ml.reg;
					gregp->r_dot = sdot.u.bd;
					gregp->r_size = (direct == FORWARD) ? -*lp : *lp;
					}
				else {
					StrLoc *gslp = &mcp->u.ginfo->ml.str;
					gslp->sd = sdot.u.sd;
					gslp->len = (direct == FORWARD) ? -*lp : *lp;
					}
				break;
			case MCE_GRPEND:
				mcp->u.ginfo->elen = (direct == FORWARD) ? *lp : -*lp;
				break;
			case MCE_BOL:
			case MCE_EOL:
			case MCE_BOS:
			case MCE_EOS:
			case MCE_EOSALT:
				if(sdot.type == BUFDOT) {
					Dot *dotp = &sdot.u.bd;

					// An empty buffer always matches.
					if(lforw(curbp->b_hdrlnp) == curbp->b_hdrlnp)
						break;

					switch(mcp->mc_type) {
					case MCE_BOL:
						if(dotp->off != 0 && lgetc(dotp->lnp,dotp->off - 1) != '\r')
							return false;
						break;
					case MCE_EOL:
						if(dotp->off != lused(dotp->lnp) && lgetc(dotp->lnp,dotp->off) != '\r')
							return false;
						break;
					case MCE_BOS:
						if(flags & SOPT_MULTI) {
							if(!boundary(dotp,BACKWARD))
								return false;
							}
						else if(dotp->off != 0)
							return false;
						break;
					case MCE_EOS:
						if(flags & SOPT_MULTI) {
							if(!boundary(dotp,FORWARD))
								return false;
							}
						else if(dotp->off != 0 || dotp->lnp == lforw(curbp->b_hdrlnp))
							return false;
						break;
					default:	// MCE_EOSALT
						if(flags & SOPT_MULTI) {
							if(boundary(dotp,FORWARD) || dotp->lnp != lback(curbp->b_hdrlnp) ||
							 dotp->off != lused(dotp->lnp))
								return false;
							}
						else if(dotp->off != lused(dotp->lnp))
							return false;
					}
					}
				else {
					StrDot *sdp = &sdot.u.sd;

					// An empty string always matches.
					if(*sdp->strp0 == '\0')
						break;

					switch(mcp->mc_type) {
					case MCE_BOL:
						if(sdp->strp != sdp->strp0 && sdp->strp[-1] != '\r')
							return false;
						break;
					case MCE_EOL:
						if(*sdp->strp != '\0' && *sdp->strp != '\r')
							return false;
						break;
					case MCE_BOS:
						if(sdp->strp != sdp->strp0)
							return false;
						break;
					case MCE_EOS:
						if(*sdp->strp != '\0')
							return false;
						break;
					default:	// MCE_EOSALT
						if(*sdp->strp == '\0') {
							if(sdp->strp[-1] == '\r')
								return false;
							}
						else if(*sdp->strp != '\r' || sdp->strp[1] != '\0')
							return false;
					}
					}
				break;
			case MCE_WORDBND:
				if((isWordCh(&sdot,-1) == isWordCh(&sdot,0)) == ((mcp->mc_type & MCE_NOT) == 0))
					return false;
				break;
			default:
				// A character to compare against the meta-character equal function.  If we still match,
				// increment the length.
				if(!mceq(nextch(&sdot,direct),mcp,flags))
					return false;
				++*lp;
				}
		++mcp;
next:;
		}
success:
	// A SUCCESSFULL MATCH!  Update the dot pointers.
	*sdotp = sdot;
	return true;
	}

// Search for a meta-pattern in either direction.  If found, position dot at the start or just after the match string and
// (perhaps) repaint the display.  "n" is repeat count and "direct" is FORWARD or BACKWARD.  Return NOTFOUND (bypassing rcset())
// if search failure.
int mcscan(int n,int direct) {
	MetaChar *mcp;			// Pointer into pattern.
	ScanDot sdot;			// Current line and offset during scan.
	bool hitbb = false;		// True if hit buffer boundary.
	uint flags = direct | (srch.m.flags & SOPT_ALL);
	Region *regp = &srch.m.groups[0].ml.reg;

	if(exactbmode())
		flags |= SXEQ_EXACT;

	// If we are searching forward, use the forward pattern in mcpat[]; otherwise, use the backward (reversed) pattern in
	// bmcpat[].
	mcp = (direct == FORWARD) ? srch.m.mcpat : srch.m.bmcpat;

	// Set local scan variable to dot.
	sdot.type = BUFDOT;
	sdot.u.bd = curwp->w_face.wf_dot;

	// Scan the buffer until we find the nth match or hit a buffer boundary *twice* (so that ^, $, \A, \Z, and \z can
	// match a boundary).
	for(;;) {
		// Save the current position in case we need to restore it on a match, and initialize the match length to zero
		// so that amatch() can compute it.
		regp->r_dot = sdot.u.bd;
		regp->r_size = 0;

		// Scan the buffer for a match.
		if(amatch(mcp,flags,&regp->r_size,&sdot)) {

			// A SUCCESSFULL MATCH!  Flag that we have moved, update the dot pointers, and save the match.
			curwp->w_flags |= WFMOVE;
			curwp->w_face.wf_dot = sdot.u.bd;		// Leave point at end of string.

			// If we were heading backward, set the "match" pointers to the beginning of the string.
			if(direct == BACKWARD)
				regp->r_dot = sdot.u.bd;
			if(savematch(&srch.m) != SUCCESS)
				return rc.status;

			// Return if nth match found.
			if(--n <= 0)
				return rc.status;
			}
		else
			// Advance the cursor.
			(void) nextch(&sdot,direct);

		// Done if hit buffer boundary twice.
		if(boundary(&sdot.u.bd,direct)) {
			if(hitbb)
				break;
			hitbb = true;
			}
		}

	// No match found.
	(void) rcset(SUCCESS,RCNOWRAP,text79);
			// "Not found"
	return NOTFOUND;
	}

// Compare given string in sp with the (non-null) meta-pattern in mtp.  If scanoff < 0, begin comparison at end of string and
// scan backward; otherwise, begin at scanoff and scan forward.  If a match is found, set *resultp to offset and save match
// results in *mtp; otherwise, set *resultp to -1.  Return status.
int recmp(Value *sp,int scanoff,Match *mtp,int *resultp) {
	MetaChar *mcp;		// Pointer into pattern.
	StrLoc *slp;
	ScanDot sdot;
	StrDot *sdp;
	uint flags = mtp->flags & SOPT_ALL;
	char *strpz,str[strlen(sp->v_strp) + 2];

	if(!(mtp->flags & SOPT_IGNORE))
		flags |= SXEQ_EXACT;

	// Get ready.  Create local copy of string with two terminating nulls to match on so that nextch() and end-of-line
	// matches work properly.
	*resultp = -1;
	sdot.type = STRDOT;
	sdp = &sdot.u.sd;
	*(strpz = stpcpy(str,sp->v_strp) + 1) = '\0';
	(slp = &mtp->groups[0].ml.str)->sd.strp0 = sdp->strp0 = str;
	if(scanoff >= 0) {
		mcp = mtp->mcpat;
		sdp->strp = str + scanoff;
		flags |= FORWARD;
		}
	else {
		mcp = mtp->bmcpat;
		sdp->strp = strpz - 1;
		flags |= BACKWARD;
		}

	// Scan the string until we find a match or hit the end (second terminating null) or beginning.
	for(;;) {
		// Save the current scan position in the match record so that if a match is found, savematch() can save the
		// matched string (group 0) and the string offset of the match can be returned.  Also set the length to zero so
		// that amatch() can compute the match length.
		slp->sd.strp = sdp->strp;
		slp->len = 0;

		// Scan the string for a match.
		if(amatch(mcp,flags,&slp->len,&sdot)) {

			// A SUCCESSFULL MATCH!  Save result and return offset.  If we were scanning backward, set the "match"
			// pointer to the beginning of the string first.
			if(flags & BACKWARD)
				slp->sd.strp = sdp->strp;
			(void) savematch(mtp);
			*resultp = ((flags & BACKWARD) ? sdp->strp : slp->sd.strp) - str;
			break;
			}

		// Advance the pointer.
		if(flags & BACKWARD) {
			if(--sdp->strp < str)
				break;
			}
		else if(++sdp->strp == strpz)
			break;
		}

	return rc.status;
	}

#if MMDEBUG & DEBUG_SHOWRE
// Build and pop up a buffer containing the search and replacement metacharacter arrays.  Render buffer and return status.
int showRegexp(Value *rp,int n) {
	uint m;
	Buffer *srlistp;
	MetaChar *mcp;
	ReplMetaChar *rmcp;
	StrList rpt;
	char *bmp,*bmpz,*strp,*litp,wkbuf[NWORK];
	char patbuf[srch.m.patlen + OPTCH_N + 1];
	struct {
		char *hdr;
		MetaChar *pat;
		} mcobj[] = {
			{text307,srch.m.mcpat},
				// "FORWARD"
			{text308,srch.m.bmcpat},
				// "BACKWARD"
 			{NULL,NULL}},*mcobjp = mcobj;

	// Get a buffer and open a string list.
	if(sysbuf(text306,&srlistp) != SUCCESS)
			// "RegexpList"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	if(vputf(&rpt,"Match flags: %.4x\r\r",srch.m.flags) != 0)
		return vrcset();
	mkpat(patbuf,&srch.m);
	do {
		// Construct the search header lines.
		if(mcobjp->pat == srch.m.bmcpat && vputs("\r\r",&rpt) != 0)
			return vrcset();
		if(vputf(&rpt,"%s %s %s /",mcobjp->hdr,text309,text311) != 0 || vstrlit(&rpt,patbuf,0) != 0 ||
		 vputs("/\r",&rpt) != 0)
						// "SEARCH","PATTERN"
			return vrcset();

		// Loop through search pattern.
		mcp = mcobjp->pat;
		for(;;) {
			strp = stpcpy(wkbuf,"    ");

			// Any closure modifiers?
			if(mcp->mc_type & MCE_CLOSURE) {
				strp = stpcpy(wkbuf,"    ");
				sprintf(strp,"%hd",mcp->cl.min);
				strp = strchr(strp,'\0');
				if(mcp->cl.max != mcp->cl.min) {
					if(mcp->cl.max < 0)
						strp = stpcpy(strp," or more");
					else {
						sprintf(strp," to %hd",mcp->cl.max);
						strp = strchr(strp,'\0');
						}
					}
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
					bmpz = (bmp = (char *) mcp->u.cclmap) + sizeof(EBitMap);
					m = 0;
					do {
						if(m ^= 1)
							if(vputc(' ',&rpt) != 0)
								return vrcset();
						if(vputf(&rpt,"%.2x",*bmp++) != 0)
							return vrcset();
						} while(bmp < bmpz);
					if(vputc('\r',&rpt) != 0)
						return vrcset();
					strp = NULL;
					break;
				case MCE_WORDBND:
					litp = (mcp->mc_type & MCE_NOT) ? "NotWordBoundary" : "WordBoundary";
					goto lit;
				case MCE_BOL:
					litp = "BeginLine";
					goto lit;
				case MCE_EOL:
					litp = "EndLine";
					goto lit;
				case MCE_BOS:
					litp = "BeginString";
					goto lit;
				case MCE_EOS:
					litp = "EndString";
					goto lit;
				case MCE_EOSALT:
					litp = "EndStringCR";
lit:
					strcpy(strp,litp);
					break;
				case MCE_GRPBEGIN:
					litp = "GroupBegin";
					goto grp;
				case MCE_GRPEND:
					litp = "GroupEnd";
grp:
					sprintf(strp,"%-14s%3u",litp,(uint) (mcp->u.ginfo - srch.m.groups));
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
	if(vputf(&rpt,"\r\r%s %s /",text310,text311) != 0 || vstrlit(&rpt,srch.m.rpat,0) != 0 ||
	 vputs("/\r",&rpt) != 0)
				// "SEARCH","PATTERN"
		return vrcset();

	// Loop through replacement pattern.
	rmcp = srch.m.rmcpat;
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
			case MCE_MATCH:
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
