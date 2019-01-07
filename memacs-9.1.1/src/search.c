// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// search.c	Search functions for MightEMacs.
//
// These functions implement commands that search in the forward and backward directions.

#include "os.h"
#include "std.h"
#include "exec.h"
#include "var.h"

// Make selected global definitions local.
#define DATAsearch
#include "search.h"

#define max(a,b)	((a < b) ? b : a)

// Check if given search pattern has trailing option characters and return results.  Flags in *flagsp are initially cleared,
// then if any options were found, the appropriate flags are set and the source string is truncated at the OptCh_Begin
// character.  The final pattern length is returned.  The rule for interpreting a pattern is as follows: "If a pattern ends with
// a colon followed by one or more valid option letters, and the colon is not the first character of the pattern, then
// everything preceding the colon is the pattern.  In all other cases, the entire string is the pattern (with no options)."
int chkopts(char *pat,ushort *flagsp) {
	char *opt = strrchr(pat,OptCh_Begin);
	int patlen = strlen(pat);			// Assume no options exist.
	ushort flags;

	*flagsp &= ~SOpt_All;				// Clear flags.
	if(opt == NULL || opt == pat)			// No option lead-in char or at beginning of pattern?
		return patlen;				// Yep, it's a no-go.

	pat = ++opt;					// No, check for lower-case letters.
	while(is_lower(*pat))
		++pat;
	if(pat == opt || *pat != '\0')			// No lowercase letters or other chars follow?
		return patlen;				// Yep, it's a no go.

	// Possible option string found.  Parse it.
	pat = opt - 1;
	short c;
	struct flginfo {
		ushort optch;		// Option character.
		ushort flag;		// Option flag.
		ushort xflags;		// Duplicate and conflicting flags.
		} *flgp;
	static struct flginfo flg[] = {
		{OptCh_Multi,SOpt_Multi,SOpt_Multi},
		{OptCh_Ignore,SOpt_Ignore,SOpt_Exact | SOpt_Ignore},
		{OptCh_Exact,SOpt_Exact,SOpt_Exact | SOpt_Ignore},
		{OptCh_Regexp,SOpt_Regexp,SOpt_Plain | SOpt_Regexp},
		{OptCh_Plain,SOpt_Plain,SOpt_Plain | SOpt_Regexp},
		{0,0,0}};

	flags = 0;
	do {
		c = *opt++;
		flgp = flg;
		do {
			if(c == flgp->optch) {
				if(flags & flgp->xflags)	// Duplicate or conflicting flags?
					break;			// Yes, bail out.
				flags |= flgp->flag;
				goto Next;
				}
			} while((++flgp)->optch != 0);

		// Invalid char found... assume it's part of the pattern.
		return patlen;
Next:;
		} while(*opt != '\0');

	// Valid option string.  Truncate pattern, update flags, and return new length.
	*pat = '\0';
	*flagsp |= flags;
	return patlen - (opt - pat);
	}

// Return true if buffer Match record specifies Exact search mode.
bool exactbmode(void) {

	return (srch.m.flags & SOpt_Exact) || (modeset(MdIdxExact,NULL) && !(srch.m.flags & SOpt_Ignore));
	}

// Return true if buffer Match record specifies Regexp search mode.
bool rebmode(void) {

	return (srch.m.flags & SOpt_Regexp) || (modeset(MdIdxRegexp,NULL) && !(srch.m.flags & SOpt_Plain));
	}

// Return true if buffer Match record specifies plain text search (post-pattern-compile, if so).
bool plainsearch(void) {

	return !rebmode() || !(srch.m.flags & SRegical);
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
	for(i = 0; i < HiChar; ++i)
		delta1[i] = srch.m.patlen;

	// Now, set the characters in the pattern.  Set both cases of letters if "exact" mode is false.
	for(i = 0; i < patlen1; ++i) {
		delta1[(int) pat[i]] = patlen1 - i;
		if(!exact)
			delta1[chcase(pat[i])] = patlen1 - i;
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
		srch.m.flags |= SCpl_Exact;
	else
		srch.m.flags &= ~SCpl_Exact;
	}

// Clear search groups in given match object.  Note that mtp->grpct is not set to zero because the RE pattern associated with
// this match object may still be compiled and usable (and uses mtp->grpct during scanning).  This can happen in replstr(), for
// example.
void grpclear(Match *mtp) {
	GrpInfo *gip,*gipz;

	// Do one loop minimum so that group 0 (the entire match) is freed.
	gipz = (gip = mtp->groups) + mtp->grpct;
	do {
		dsetnull(gip->matchp);
		} while(gip++ < gipz);
	}

// Free up any CCL bitmaps in the Regexp search arrays contained in the given Match record and mark it as cleared.
void mcclear(Match *mtp) {
	MetaChar *mcp;

	mcp = mtp->mcpat;
	while(mcp->mc_type != MCE_Nil) {
		if((mcp->mc_type == MCE_CCL) || (mcp->mc_type == MCE_NCCL))
			free((void *) mcp->u.cclmap);
		++mcp;
		}

	mtp->mcpat[0].mc_type = mtp->bmcpat[0].mc_type = MCE_Nil;
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
int newspat(char *pat,Match *mtp,ushort *flagsp) {

	// Get and strip search options, if any.
	if(flagsp == NULL)
		mtp->patlen = chkopts(pat,&mtp->flags);
	else {
		mtp->patlen = strlen(pat);
		mtp->flags = (mtp->flags & ~SOpt_All) | *flagsp;
		}

	// Free up arrays if too big.
	if(mtp->ssize > NPatMax || (mtp->ssize > 0 && (uint) mtp->patlen > mtp->ssize)) {
		freespat(mtp);
		if(mtp == &srch.m) {
			(void) free((void *) srch.bpat);
			(void) free((void *) srch.fdelta2);
			(void) free((void *) srch.bdelta2);
			}
		}

	// Get heap space for arrays if needed.
	if(mtp->ssize == 0) {
		mtp->ssize = mtp->patlen < NPatMin ? NPatMin : mtp->patlen;
		if((mtp->pat = (char *) malloc(mtp->ssize + 1)) == NULL ||
		 (mtp->mcpat = (MetaChar *) malloc((mtp->ssize + 1) * sizeof(MetaChar))) == NULL ||
		 (mtp->bmcpat = (MetaChar *) malloc((mtp->ssize + 1) * sizeof(MetaChar))) == NULL ||
		 (mtp == &srch.m &&
		 ((srch.bpat = (char *) malloc(mtp->ssize + 1)) == NULL ||
		 (srch.fdelta2 = (int *) malloc(mtp->ssize * sizeof(int))) == NULL ||
		 (srch.bdelta2 = (int *) malloc(mtp->ssize * sizeof(int))) == NULL)))
			return rcset(Panic,0,text94,"newspat");
					// "%s(): Out of memory!"
		mtp->mcpat[0].mc_type = mtp->bmcpat[0].mc_type = MCE_Nil;
		}

	// Save search pattern.
	strcpy(mtp->pat,pat);
	if(mtp == &srch.m) {
		char patbuf[mtp->patlen + OptCh_N + 1];
		strrev(strcpy(srch.bpat,pat));
		srch.fdelta1[0] = -1;					// Clear delta tables.
		if(rsetpat(mkpat(patbuf,mtp),&sring) != Success)	// Add pattern to search ring.
			return rc.status;
		}
	mcclear(mtp);							// Clear Regexp search table.
	return rc.status;
	}

// Make original search pattern from given Match record, store in dest, and return it.  *dest is assumed to be large enough to
// hold the result.
char *mkpat(char *dest,Match *mtp) {
	struct flginfo {
		ushort optch;
		ushort flag;
		} *flgp;
	static struct flginfo flg[] = {
		{OptCh_Ignore,SOpt_Ignore},
		{OptCh_Exact,SOpt_Exact},
		{OptCh_Regexp,SOpt_Regexp},
		{OptCh_Plain,SOpt_Plain},
		{OptCh_Multi,SOpt_Multi},
		{0,0}};
	char *str = stpcpy(dest,mtp->pat);
	if(mtp->flags & SOpt_All) {
		*str++ = OptCh_Begin;
		flgp = flg;
		do {
			if(mtp->flags & flgp->flag)
				*str++ = flgp->optch;
			} while((++flgp)->optch != 0);
		*str = '\0';
		}
	return dest;
	}

// Get a search or replacement pattern and stash it in global variable srch.pat (srchpat is true) or srch.rpat (srchpat is
// false) via newspat() or newrpat().  If script mode, *argpp contains the pattern; otherwise, get it interactively.  If the
// pattern is null, srchpat is true, and there is no old pattern, it is an error.  Return status.
int getpat(Datum **argpp,char *prmt,bool srchpat) {
	Datum *tpatp;

	if(dnewtrk(&tpatp) != 0)
		return librcset(Failure);

	// Read a pattern.  Either we get one, or we just get the input delimiter and use the previous pattern.
	if(si.opflags & OpScript)
		datxfer(tpatp,argpp[0]);
	else {
		TermInp ti = {NULL,srch.sdelim,0,srchpat ? &sring : &rring};
		if(terminp(tpatp,prmt,srchpat ? ArgNotNull1 | ArgNil1 : ArgNil1,0,&ti) != Success)
			return rc.status;
		if(tpatp->d_type == dat_nil)
			dsetnull(tpatp);
		}

	// Error if search pattern is null.
	if(srchpat) {
		if(disnull(tpatp))
			return rcset(Failure,RCNoFormat,text80);
					// "No pattern set"

		// New search pattern?
		ushort flags = 0;
		(void) chkopts(tpatp->d_str,&flags);
		if(flags != (srch.m.flags & SOpt_All) || strcmp(tpatp->d_str,srch.m.pat) != 0) {

			// Save the new search pattern.
			(void) newspat(tpatp->d_str,&srch.m,&flags);
			}
		}
	else if(strcmp(tpatp->d_str,srch.m.rpat) != 0) {

		// Save the new replacement pattern.
		(void) newrpat(tpatp->d_str,&srch.m);
		}

	return rc.status;
	}

// Create search tables if needed.
static int mktab(void) {

	// Clear search groups.
	grpclear(&srch.m);

	// Compile pattern as an RE if requested.
	if(rebmode() && srch.m.mcpat[0].mc_type == MCE_Nil && mccompile(&srch.m) != Success)
		return rc.status;

	// Compile as a plain-text pattern if not an RE request or not really an RE (SRegical not set).
	if(plainsearch() && (srch.fdelta1[0] == -1 || (((srch.m.flags & SCpl_Exact) != 0) != exactbmode())))
		mkdeltas();

	return rc.status;
	}

// Search forward or backward for a previously acquired search pattern.  If found, the matched string is returned; otherwise,
// false is returned.
static int hunt(Datum *rp,int n,int direct,char *pat) {

	if(n != 0) {			// Nothing to do if n is zero.
		if(n == INT_MIN)
			n = 1;
		else if(n < 0)
			return rcset(Failure,0,text39,text137,n,0);
					// "%s (%d) must be %d or greater","Repeat count"

		// Make sure a pattern exists, or that we didn't switch into Regexp mode after the pattern was entered.
		if(pat[0] == '\0')
			return rcset(Failure,RCNoFormat,text80);
				// "No pattern set"

		// Create search tables if needed.
		if(mktab() == Success) {

			// Perform appropriate search and return result.
			if(((rebmode() && (srch.m.flags & SRegical)) ? mcscan(n,direct) : scan(n,direct)) == NotFound)
				dsetbool(false,rp);
			else if(datcpy(rp,srch.m.matchp) != 0)
				(void) librcset(Failure);
			}
		}

	return rc.status;
	}

// Search forward for a previously acquired search pattern, starting at point and proceeding toward the bottom of the buffer.
// If found, point is left at the character immediately following the matched string and the matched string is returned;
// otherwise, false is returned.
int huntForw(Datum *rp,int n,Datum **argpp) {

	return hunt(rp,n,Forward,srch.m.pat);
	}

// Search forward.  Get a search pattern and, starting at point, search toward the bottom of the buffer.  If found, point is
// left at the character immediately following the matched string and the matched string is returned; otherwise, false is
// returned.
int searchForw(Datum *rp,int n,Datum **argpp) {

	// Get the pattern and perform the search up to n times.
	if(getpat(argpp,text78,true) == Success && n != 0)
			// "Search"
		(void) huntForw(rp,n,NULL);

	return rc.status;
	}

// Search backward for a previously acquired search pattern, starting at point and proceeding toward the top of the buffer.  If
// found, point is left at the first character of the matched string (the last character that was matched) and the
// matched string is returned; otherwise, false is returned.
int huntBack(Datum *rp,int n,Datum **argpp) {

	return hunt(rp,n,Backward,srch.bpat);
	}

// Search backward.  Get a search pattern and, starting at point, search toward the top of the buffer.  If found, point is left
// at the first character of the matched string (the last character that was matched) and the matched string is returned;
// otherwise, false is returned.
int searchBack(Datum *rp,int n,Datum **argpp) {

	// Get the pattern and perform the search up to n times.
	if(getpat(argpp,text81,true) == Success && n != 0)
			// "Reverse search"
		(void) huntBack(rp,n,NULL);

	return rc.status;
	}

// Compare two characters ("bc" from the buffer, and "pc" from the pattern) and return Boolean result.  If "exact" is false,
// fold the case.
static bool eq(ushort bc,ushort pc,bool exact) {

	if(!exact) {
		bc = lowcase[bc];
		pc = lowcase[pc];
		}

	return bc == pc;
	}

// Retrieve the next or previous character in a buffer or string, and advance or retreat the scanning dot.  The order in which
// this is done is important and depends upon the direction of the search.  Forward searches fetch and advance; reverse
// searches back up and fetch.  Return -1 if a buffer or string boundary is hit.
static int nextch(ScanDot *sdotp,int direct) {
	int c;

	if(sdotp->type == ScanDot_Str) {
		StrDot *sdp = &sdotp->u.sd;
		if(direct == Forward)
			c = (sdp->str[0] == '\0' && sdp->str > sdp->str0 && sdp->str[-1] == '\0') ? -1 : *sdp->str++;
		else
			c = (sdp->str == sdp->str0) ? -1 : *--sdp->str;
		}
	else {
		Dot *dotp = &sdotp->u.bdot;
		if(direct == Forward) {					// Going forward?
			if(bufend(dotp))				// Yes.  At bottom of buffer?
				c = -1;					// Yes, return -1;
			else if(dotp->off == dotp->lnp->l_used) {	// No.  At EOL?
				dotp->lnp = dotp->lnp->l_nextp;		// Yes, skip to next line...
				dotp->off = 0;
				c = '\n';				// and return a <NL>.
				}
			else
				c = dotp->lnp->l_text[dotp->off++];	// Otherwise, return current char.
			}
		else if(dotp->off == 0) {				// Going backward.  At BOL?
			if(dotp->lnp == si.curbp->b_lnp)		// Yes.  At top of buffer?
				c = -1;					// Yes, return -1.
			else {
				dotp->lnp = dotp->lnp->l_prevp;		// No.  Skip to previous line...
				dotp->off = dotp->lnp->l_used;
				c = '\n';				// and return a <NL>.
				}
			}
		else
			c = dotp->lnp->l_text[--dotp->off];		// Not at BOL... return previous char.
		}
	return c;
	}

// Move the buffer scanning dot by jumpsz characters.  Return false if a boundary is hit (and we may therefore search no
// further); otherwise, update dot and return true.
static bool bjump(int jumpsz,Dot *sdotp,int direct) {

	if(direct == Forward) {							// Going forward.
		if(bufend(sdotp))
			return false;						// Hit end of buffer.
		sdotp->off += jumpsz;
		while(sdotp->off > sdotp->lnp->l_used) {			// Skip to next line if overflow.
			sdotp->off -= sdotp->lnp->l_used + 1;
			if((sdotp->lnp = sdotp->lnp->l_nextp) == NULL)
				return false;					// Hit end of buffer.
			}
		}
	else {									// Going backward.
		sdotp->off -= jumpsz;
		while(sdotp->off < 0) {						// Skip back a line.
			if(sdotp->lnp == si.curbp->b_lnp)
				return false;					// Hit beginning of buffer.
			sdotp->lnp = sdotp->lnp->l_prevp;
			sdotp->off += sdotp->lnp->l_used + 1;
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
	StrLoc *sfp;

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
			sfp = &gip->ml.str;
			len = (sfp->len += gip->elen);
			}

		if((gip->matchp == NULL && dnewtrk(&gip->matchp) != 0) || dsalloc(gip->matchp,len + 1) != 0)
			return librcset(Failure);
		if(mtp == &srch.m)
			regcpy(gip->matchp->d_str,regp);
		else
			stplcpy(gip->matchp->d_str,sfp->sd.str,len + 1);
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
// repeat count and "direct" is Forward or Backward.  Return NotFound (bypassing rcset()) if search failure.
int scan(int n,int direct) {
	Match *mtp = &srch.m;
	Region *regp = &mtp->groups[0].ml.reg;
	ushort bc,pc;		// Buffer char and pattern char.
	int pati;		// Pattern index.
	char *pat,*patz;	// Pattern pointers.
	char *pattern;		// String to scan for.
	ScanDot sdot;		// Current line and offset during scanning.
	int sdirect;		// Scan direction.
	int *delta1,*delta2;
	int jumpsz;		// Next offset.
	uint loopCount = 0;
	bool exact = exactbmode();
#if SHOW_COMPARES
	int compares = 0;
#endif
	// Set scanning direction for pattern matching, which is always in the opposite direction of the search.
	sdirect = direct ^ 1;

	// Another directional problem: if we are searching forward, use the backward (reversed) pattern in bpat[], and the
	// forward delta tables; otherwise, use the forward pattern in pat[], and the backward delta tables.  This is so the
	// pattern can always be scanned from left to right (until patlen characters have been examined) during comparisions.
	if(direct == Forward) {
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
	sdot.type = ScanDot_Buf;
	sdot.u.bdot = si.curwp->w_face.wf_dot;

	// Scan the buffer until we find the nth match or hit a buffer boundary.  The basic loop is to jump forward or backward
	// in the buffer (so that comparisons are done in reverse), check for a match, get the next jump size (delta) on a
	// failure, and repeat.  The initial jump is the full pattern length so that the first character returned by nextch() is
	// at the far end of a possible match, depending on the search direction.
	regp->r_size = jumpsz = mtp->patlen;
	regp->r_linect = 0;					// Not used.
	patz = pattern + mtp->patlen;
	while(bjump(jumpsz,&sdot.u.bdot,direct)) {

		// Save the current position in case we match the search string at this point.
		regp->r_dot = sdot.u.bdot;

		// Scan through the pattern for a match.  Note that nextch() will never hit a buffer boundary (and return -1)
		// because bjump() checks for that.
		pat = pattern;
		pati = mtp->patlen;
#if SHOW_COMPARES
		++compares;
#endif
		do {
			pc = *pat;
			--pati;
			if(!eq(pc,bc = nextch(&sdot,sdirect),exact)) {

				// No match.  Jump forward or backward in buffer as far as possible and try again.
				// Use delta + 1 so that nextch() begins at the right spot.
				jumpsz = max(delta1[bc],delta2[pati]) + 1;
				goto Fail;
				}
			} while(++pat < patz);

		// A SUCCESSFULL MATCH!  Flag that we have moved, update the dot pointers, and save the match.  Need to do this
		// now because the nth match may not be found, in which case we want to leave the dot at the last one.
		movept(&regp->r_dot);				// Leave point at end of string.

		// If we were heading forward, set the "match" pointers to the beginning of the string.
		if(direct == Forward)
			regp->r_dot = sdot.u.bdot;
		if(savematch(&srch.m) != Success)
			return rc.status;

		// Return if nth match found.
		if(--n <= 0) {
#if SHOW_COMPARES
			return rcset(Success,0,"%d comparisons",compares);
#else
			if(loopCount >= ProgressLoopCt)
				(void) mlerase();
			return rc.status;
#endif
			}

		// Nth match not found.  Reset and continue scan.
		jumpsz = mtp->patlen * 2;
Fail:
		// If search is taking awhile, let user know.
		if(loopCount <= ProgressLoopCt) {
			if(loopCount++ == ProgressLoopCt)
				if(mlputs(MLHome | MLWrap | MLFlush,text203) != Success)
						// "Searching..."
					return rc.status;
			}
		}

	// No match found.
#if SHOW_COMPARES
	(void) rcset(Success,RCNoWrap,"Not found (%d comparisons)",compares);
#else
	(void) rcset(Success,RCNoFormat | RCNoWrap,text79);
			// "Not found"
#endif
	return NotFound;
	}

// Set a bit (ON only) in the bitmap.
static void setbit(int bc,EBitMap *cclmap) {

	if((unsigned) bc < HiChar)
		*((char *) cclmap + (bc >> 3)) |= Bit(bc & 7);
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
	setbit(MC_CCLRange,cclmap);
	}

// Create the bitmap for the character class.  patp is left pointing to the end-of-character-class character, so that a
// loop may automatically increment with safety.  Return status.
static int cclmake(char **patp,MetaChar *mcp) {
	EBitMap *bmap;
	char *pat;
	int pchr,ochr;

	if((bmap = (EBitMap *) malloc(sizeof(EBitMap))) == NULL)
		return rcset(Panic,0,text94,"cclmake");
			// "%s(): Out of memory!"

	memset((void *) bmap,0,sizeof(EBitMap));
	mcp->u.cclmap = bmap;
	pat = *patp;

	// Test the initial character(s) in ccl for special cases: negate ccl, or an end ccl character as a first character.
	// Anything else gets set in the bitmap.
	if(*++pat == MC_NCCL) {
		++pat;
		mcp->mc_type = MCE_NCCL;
		}
	else
		mcp->mc_type = MCE_CCL;

	if((pchr = *pat) == MC_CCLEnd)
		return rcset(Failure,RCNoFormat,text96);
			// "Empty character class"

	// Loop through characters.  Ranges are not processed until the atom (single character or \c) following the '-' is
	// parsed.  After each atom is determined, if ochr > 0 then it is the end of a range; otherwise, it's just a singleton.
	for(ochr = -1; pchr != MC_CCLEnd && pchr != '\0'; pchr = *++pat) {
		switch(pchr) {

			// The range character loses its meaning if it is the first or last character in the class.  We also
			// treat it as ordinary if the range is invalid or the order is wrong; e.g., "\d-X" or "z-a".
			case MC_CCLRange:
				if(pat[1] == '\0')
					goto ErrRetn;
				if(ochr < 0)
					goto Plain;
				if(pat[1] == MC_CCLRange) {		// [c--...]
					setnorange(ochr,bmap);
					goto Clear;
					}
				continue;
			case MC_Esc:
				switch(pchr = *++pat) {
					case MC_Tab:
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
					case MC_Digit:
						setbitrange('0','9',bmap);
						goto Mult;
					case MC_Letter:
						setbitrange('a','z',bmap);
						setbitrange('A','Z',bmap);
						goto Mult;
					case MC_Space:
						setbit(' ',bmap);
						setbit('\t',bmap);
						setbit('\r',bmap);
						setbit('\n',bmap);
						setbit('\f',bmap);
						goto Mult;
					case MC_Word:
						{char *str,*strz;

						// Find the current word chars and set those bits in the bit map.
						strz = (str = wordlist) + 256;
						do {
							if(*str)
								setbit(str - wordlist,bmap);
							} while(++str < strz);
						}
Mult:
						if(ochr > 0)
							setnorange(ochr,bmap);
						goto Clear;
					case '\0':
						goto ErrRetn;
					}
				// Fall through.
			default:
				if(ochr > 0) {					// Current char is end of a range.
					if(pchr < ochr) {			// Wrong order?
						setnorange(ochr,bmap);		// Yes, treat as plain chars.
						goto Plain;
						}
					setbitrange(ochr,pchr,bmap);		// Valid range... set bit(s).
					goto Clear;
					}
				if(pat[1] == MC_CCLRange)			// Not at end of range.  At beginning?
					ochr = pchr;				// Yes, save current char for later.
				else {						// No, process as plain char.
Plain:
					setbit(pchr,bmap);
Clear:
					ochr = -1;
					}
			}
		}
	if(ochr > 0)
		setnorange(ochr,bmap);		// Char class ended with MC_CCLRange.

	*patp = pat;
	if(pchr == '\0')
ErrRetn:
		(void) rcset(Failure,RCNoFormat,text97);
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
	char *pat,*str;
	ushort pc;
	int clmin,clmax;
	ushort typ;
	bool lastDoesClosure = false;
	int grpstack[MaxGroups];
	int stacklevel = 0;

	mcp = mtp->mcpat;
	pat = mtp->pat;
	mtp->flags &= ~SRegical;
	mtp->grpct = 0;

	for(; (pc = *pat) != '\0'; ++mcp,++pat) {
		switch(pc) {
			case MC_CCLBegin:
				if(cclmake(&pat,mcp) != Success)
					goto ClrExit;
				goto ChClass;
			case MC_BOL:
				typ = MCE_BOL;
				goto Set;
			case MC_EOL:
				typ = MCE_EOL;
				goto Set;
			case MC_Any:
				lastDoesClosure = true;
				typ = MCE_Any;
				goto Set;
			case MC_Closure0:	// *
				clmin = 0;
				goto ChkClos;
			case MC_Closure1:	// +
				clmin = 1;
ChkClos:
				// Does the closure symbol mean closure here?  If so, back up to the previous element and
				// mark it as such.
				if(lastDoesClosure) {
					clmax = -1;
					goto DoClos;
					}
				goto BadClos;
			case MC_Closure01:	// ?

				// Does the zero-or-one closure symbol mean closure here?
				if(lastDoesClosure) {
					clmin = 0;
					clmax = 1;
					}
				else {
					// Not closure.  Is it closure modification?  If so, back up to the previous element and
					// mark it as such.
					if(mcp == mtp->mcpat || !(mcp[-1].mc_type & MCE_Closure))
						goto BadClos;
					--mcp;
					mcp->mc_type |= MCE_MinClosure;
					break;
					}
				goto DoClos;
			case MC_ClBegin:

				// Does the begin-closure-count symbol mean closure here?
				if(!lastDoesClosure) {
					if(pat[1] >= '0' && pat[1] <= '9')
						goto BadClos;
					goto LitChar;
					}

				// Yes, get integer value(s).
				clmin = -1;
				for(;;) {
					if(*++pat == '\0' || *pat < '0' || *pat > '9')
						goto BadClos;
					clmax = 0;
					do {
						clmax = clmax * 10 + (*pat++ - '0');
						} while(*pat >= '0' && *pat <= '9');
					if(clmin == -1) {
						clmin = clmax;
						if(pat[0] != ',')
							break;
						if(pat[1] == MC_ClEnd) {
							clmax = -1;
							++pat;
							goto DoClos;
							}
						}
					else {
						if(clmax == 0 || clmax < clmin)
							goto BadClos;
						break;
						}
					}
				if(*pat != MC_ClEnd)
					goto BadClos;
DoClos:
				// Back up...
				--mcp;

				// and check for closure on a group (not supported).
				if(mcp->mc_type == MCE_GrpEnd) {
					(void) rcset(Failure,0,text304,mtp->pat);
							// "Closure on group not supported in RE pattern '%s'"
					goto ClrExit;
					}

				// All is well... add closure info.
				mcp->mc_type |= MCE_Closure;
				mcp->cl.min = clmin;
				mcp->cl.max = clmax;
				lastDoesClosure = false;
				goto SetReg;
BadClos:
				(void) rcset(Failure,0,text255,mtp->pat);
						// "Invalid repetition operand in RE pattern '%s'"
				goto ClrExit;
			case MC_GrpBegin:

				// Beginning of a group.  Check count.
				if(++mtp->grpct < MaxGroups) {
					mcp->u.ginfo = mtp->groups + mtp->grpct;
					grpstack[stacklevel++] = mtp->grpct;
					lastDoesClosure = false;
					typ = MCE_GrpBegin;
					goto Set;
					}
				(void) rcset(Failure,0,text221,mtp->pat,MaxGroups);
						// "Too many groups in RE pattern '%s' (maximum is %d)"
				goto ClrExit;
			case MC_GrpEnd:

				// End of a group.  Check if no group left to close.
				if(stacklevel > 0) {
					mcp->u.ginfo = mtp->groups + grpstack[--stacklevel];
					typ = MCE_GrpEnd;
					goto Set;
					}
				(void) rcset(Failure,0,text258,mtp->pat);
						// "Unmatched right paren in RE pattern '%s'"
				goto ClrExit;
			case MC_Esc:
				mtp->flags |= SRegical;
				switch(pc = *++pat) {
					case '\0':
						pc = '\\';
						--pat;
						break;
					case MC_Digit:
						str = "[\\d]";
						goto MakeChClass;
					case MC_NDigit:
						str = "[^\\d]";
						goto MakeChClass;
					case MC_Tab:
						pc = '\t';
						break;
					case MC_CR:
						pc = '\r';
						break;
					case MC_Space:
						str = "[\\s]";
						goto MakeChClass;
					case MC_NSpace:
						str = "[^\\s]";
						goto MakeChClass;
					case MC_Letter:
						str = "[\\l]";
						goto MakeChClass;
					case MC_NLetter:
						str = "[^\\l]";
						goto MakeChClass;
					case MC_Word:
						str = "[\\w]";
						goto MakeChClass;
					case MC_NWord:
						str = "[^\\w]";
MakeChClass:
						if(cclmake(&str,mcp) != Success)
							goto ClrExit;
ChClass:
						lastDoesClosure = true;
						goto SetReg;
					case MC_NL:
						pc = '\n';
						break;
					case MC_FF:
						pc = '\f';
						break;
					case MC_WordBnd:
						typ = MCE_WordBnd;
						goto Set;
					case MC_NWordBnd:
						typ = MCE_WordBnd | MCE_Not;
						goto Set;
					case MC_BOS:
						typ = MCE_BOS;
						goto Set;
					case MC_EOS:
						typ = MCE_EOS;
						goto Set;
					case MC_EOSAlt:
						typ = MCE_EOSAlt;
Set:
						mcp->mc_type = typ;
SetReg:
						mtp->flags |= SRegical;
						continue;
					}
				goto LitChar;
			default:	// Literal character.
LitChar:
				mcp->mc_type = MCE_LitChar;
				mcp->u.lchar = pc;
				lastDoesClosure = true;
				break;
			}
		}

	// Check RE group level.
	if(stacklevel) {
		(void) rcset(Failure,RCNoFormat,text222);
			// "RE group not ended"
		goto ClrExit;
		}

	// No error: terminate the forward array, make the reverse array, and return success.
	mcp->mc_type = MCE_Nil;
	bmcp = mtp->bmcpat;
	do {
		*bmcp++ = *--mcp;
		} while(mcp > mtp->mcpat);
	bmcp->mc_type = MCE_Nil;
	return rc.status;
ClrExit:
	// Error: clear the MetaChar array and return exception status.
	mcp[1].mc_type = MCE_Nil;
	mcclear(mtp);
	return rc.status;
	}

// Is the character set in the bitmap?
static bool biteq(int bc,EBitMap *cclmap) {

	if((unsigned) bc >= HiChar)
		return false;

	return ((*((char *) cclmap + (bc >> 3)) & Bit(bc & 7)) ? true : false);
	}


// Perform meta-character equality test with a character and return Boolean result.  In Kernighan & Plauger's Software Tools,
// this is the omatch() function.  The "buffer boundary" character (-1) never matches anything.
static bool mceq(short c,MetaChar *mt,uint flags) {
	bool result;
	short c1;

	if(c == -1 || c == 0)
		return false;

	switch(mt->mc_type & MCE_BaseType) {
		case MCE_LitChar:
			result = eq(c,mt->u.lchar,flags & SXeq_Exact);
			break;
		case MCE_Any:
			result = (c != '\n' || (flags & SOpt_Multi));
			break;
		case MCE_CCL:
			if(!(result = biteq(c,mt->u.cclmap))) {
				if(!(flags & SXeq_Exact) && (c1 = chcase(c)) != c)
					result = biteq(c1,mt->u.cclmap);
				}
			break;
		default:	// MCE_NCCL.
			result = (c != '\n' || (flags & SOpt_Multi)) && !biteq(c,mt->u.cclmap);
			if(!(flags & SXeq_Exact) && (c1 = chcase(c)) != c)
				result &= !biteq(c1,mt->u.cclmap);
		}

	return result;
	}

// Return true if no metacharacters in given list would cause a call to nextch().
static bool mcstill(MetaChar *mcp) {

	for(;;) {
		switch(mcp->mc_type) {
		case MCE_Nil:
			return true;
		case MCE_BOL:
		case MCE_EOL:
		case MCE_BOS:
		case MCE_EOSAlt:
		case MCE_EOS:
		case MCE_WordBnd:
		case MCE_GrpBegin:
		case MCE_GrpEnd:
			++mcp;
			break;
		default:
			goto Bust;
		}
		}
Bust:
	return false;
	}

// Return true if previous (n < 0) or current (n == 0) scan character is a word character.
static bool isWordCh(ScanDot *sdotp,int n) {

	if(sdotp->type == ScanDot_Buf) {
		Dot *dotp = &sdotp->u.bdot;
		if(n < 0)
			return dotp->off == 0 ? (dotp->lnp != si.curbp->b_lnp && wordlist['\n']) :
			 wordlist[(int) dotp->lnp->l_text[dotp->off - 1]];
		else
			return dotp->off == dotp->lnp->l_used ? dotp->lnp->l_nextp != NULL && wordlist['\n'] :
			 wordlist[(int) dotp->lnp->l_text[dotp->off]];
		}
	else {
		StrDot *sdp = &sdotp->u.sd;
		if(n < 0)
			return sdp->str > sdp->str0 && wordlist[(int) sdp->str[-1]];
		else
			return wordlist[(int) *sdp->str];
		}
	}

// Try to match a meta-pattern at the given position in either direction.  Based on the recursive routine amatch() (for
// "anchored match") in Kernighan & Plauger's "Software Tools".  "mcp" is the pattern, "flags" is Forward or Backward and any
// SOpt_All or SXeq_All flags, and "sdotp" contains a pointer to the scan dot.  If the given position matches, set *lp to the
// match length, update *sdotp, and return true; otherwise, return false.
static bool amatch(MetaChar *mcp,uint flags,long *lp,ScanDot *sdotp) {
	ScanDot sdot;		// Current dot location during scan.
	int prematchlen;	// Matchlen before a recursive amatch() call.
	int clmatchlen;		// Number of characters matched in a local closure.
	int direct = flags & ~(SOpt_All | SXeq_All);

	// Set up local scan pointers and a character counting variable which corrects matchlen on a partial match.
	sdot = *sdotp;
	clmatchlen = 0;

	// Loop through the meta-pattern, laughing all the way.
	while(mcp->mc_type != MCE_Nil) {

		// Is the current meta-character modified by a closure?
		if(mcp->mc_type & MCE_Closure) {
			if(mcp->mc_type & MCE_MinClosure) {
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
					goto Found;
				if(clmax < 0)
					clmax = INT_MAX;
				for(;;) {
					prematchlen = *lp;
					if(amatch(mcp + 1,flags,lp,&sdot))
						goto Found;
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
							goto Next;
						c = 0;
						--*lp;
						}
					else if(nextch(&sdot,direct ^ 1) == -1)
						--*lp;
					else {
						prematchlen = *lp;
						if(amatch(mcp,flags,lp,&sdot))
							goto Found;
						*lp = prematchlen - 1;
						}
					--clmatchlen;
					}
				}
			}
		else
			switch(mcp->mc_type & MCE_BaseType) {
			case MCE_GrpBegin:
				if(sdot.type == ScanDot_Buf) {
					Region *gregp = &mcp->u.ginfo->ml.reg;
					gregp->r_dot = sdot.u.bdot;
					gregp->r_size = (direct == Forward) ? -*lp : *lp;
					gregp->r_linect = 0;
					}
				else {
					StrLoc *gslp = &mcp->u.ginfo->ml.str;
					gslp->sd = sdot.u.sd;
					gslp->len = (direct == Forward) ? -*lp : *lp;
					}
				break;
			case MCE_GrpEnd:
				mcp->u.ginfo->elen = (direct == Forward) ? *lp : -*lp;
				break;
			case MCE_BOL:
			case MCE_EOL:
			case MCE_BOS:
			case MCE_EOS:
			case MCE_EOSAlt:
				if(sdot.type == ScanDot_Buf) {
					Dot *dotp = &sdot.u.bdot;

					// An empty buffer always matches.
					if(bempty(NULL))
						break;

					// If multi-line mode, treat whole buffer as single string.
					switch(mcp->mc_type) {
					case MCE_BOL:
						if(dotp->off != 0 && dotp->lnp->l_text[dotp->off - 1] != '\n')
							return false;
						break;
					case MCE_EOL:
						if(dotp->off != dotp->lnp->l_used && dotp->lnp->l_text[dotp->off] != '\n')
							return false;
						break;
					case MCE_BOS:
						if(flags & SOpt_Multi) {
							if(!bufbegin(dotp))
								return false;
							}
						else if(dotp->off != 0)
							return false;
						break;
					case MCE_EOS:
						if(flags & SOpt_Multi) {
							if(!bufend(dotp))
								return false;
							}
						else if(dotp->off == 0) {
							if(dotp->lnp == si.curbp->b_lnp)
								return false;
							}
						else if(!bufend(dotp))
							return false;
						break;
					default:	// MCE_EOSAlt
						if(flags & SOpt_Multi) {
							if(dotp->lnp->l_nextp == NULL) {
								if(dotp->lnp->l_used == 0 || dotp->off != dotp->lnp->l_used)
									return false;
								}
							else if(dotp->off != dotp->lnp->l_used ||
							 dotp->lnp != si.curbp->b_lnp->l_prevp->l_prevp)
								return false;
							}
						else if(dotp->off != dotp->lnp->l_used)
							return false;
					}
					}
				else {
					StrDot *sdp = &sdot.u.sd;

					// An empty string always matches.
					if(*sdp->str0 == '\0')
						break;

					switch(mcp->mc_type) {
					case MCE_BOL:
						if(sdp->str != sdp->str0 && sdp->str[-1] != '\n')
							return false;
						break;
					case MCE_EOL:
						if(*sdp->str != '\0' && *sdp->str != '\n')
							return false;
						break;
					case MCE_BOS:
						if(sdp->str != sdp->str0)
							return false;
						break;
					case MCE_EOS:
						if(*sdp->str != '\0')
							return false;
						break;
					default:	// MCE_EOSAlt
						if(*sdp->str == '\0') {
							if(sdp->str[-1] == '\n')
								return false;
							}
						else if(*sdp->str != '\n' || sdp->str[1] != '\0')
							return false;
					}
					}
				break;
			case MCE_WordBnd:
				if((isWordCh(&sdot,-1) == isWordCh(&sdot,0)) == ((mcp->mc_type & MCE_Not) == 0))
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
Next:;
		}
Found:
	// A SuccessFULL MATCH!  Update the dot pointers.
	*sdotp = sdot;
	return true;
	}

// Search for a meta-pattern in either direction.  If found, position dot at the start or just after the match string and
// (perhaps) repaint the display.  "n" is repeat count and "direct" is Forward or Backward.  Return NotFound (bypassing rcset())
// if search failure.
int mcscan(int n,int direct) {
	MetaChar *mcp;			// Pointer into pattern.
	ScanDot sdot;			// Current line and offset during scan.
	bool hitbb = false;		// True if hit buffer boundary.
	uint flags = direct | (srch.m.flags & SOpt_All);
	Region *regp = &srch.m.groups[0].ml.reg;
	uint loopCount = 0;

	if(exactbmode())
		flags |= SXeq_Exact;

	// If we are searching forward, use the forward pattern in mcpat[]; otherwise, use the backward (reversed) pattern in
	// bmcpat[].
	mcp = (direct == Forward) ? srch.m.mcpat : srch.m.bmcpat;

	// Set local scan variable to dot.
	sdot.type = ScanDot_Buf;
	sdot.u.bdot = si.curwp->w_face.wf_dot;

	// Scan the buffer until we find the nth match or hit a buffer boundary *twice* (so that ^, $, \A, \Z, and \z can
	// match a boundary).
	for(;;) {
		// Save the current position in case we need to restore it on a match, and initialize the match length to zero
		// so that amatch() can compute it.
		regp->r_dot = sdot.u.bdot;
		regp->r_size = (long)(regp->r_linect = 0);

		// Scan the buffer for a match.
		if(amatch(mcp,flags,&regp->r_size,&sdot)) {

			// A SUCCESSFULL MATCH!  Flag that we have moved, update the dot pointers, and save the match.
			movept(&sdot.u.bdot);				// Leave point at end of string.

			// If we were heading backward, set the "match" pointers to the beginning of the string.
			if(direct == Backward)
				regp->r_dot = sdot.u.bdot;
			if(savematch(&srch.m) != Success)
				return rc.status;

			// Return if nth match found.
			if(--n <= 0) {
				if(loopCount >= ProgressLoopCt)
					(void) mlerase();
				return rc.status;
				}
			}
		else
			// Advance the point.
			(void) nextch(&sdot,direct);

		// Done if hit buffer boundary twice.
		if(boundary(&sdot.u.bdot,direct)) {
			if(hitbb)
				break;
			hitbb = true;
			}

		// If search is taking awhile, let user know.
		if(loopCount <= ProgressLoopCt) {
			if(loopCount++ == ProgressLoopCt)
				if(mlputs(MLHome | MLWrap | MLFlush,text203) != Success)
						// "Searching..."
					return rc.status;
			}
		}

	// No match found.
	(void) rcset(Success,RCNoFormat | RCNoWrap,text79);
			// "Not found"
	return NotFound;
	}

// Compare given string in sp with the (non-null) meta-pattern in mtp.  If scanoff < 0, begin comparison at end of string and
// scan backward; otherwise, begin at scanoff and scan forward.  If a match is found, set *resultp to offset and save match
// results in *mtp; otherwise, set *resultp to -1.  Return status.
int recmp(Datum *sp,int scanoff,Match *mtp,int *resultp) {
	MetaChar *mcp;		// Pointer into pattern.
	StrLoc *sfp;
	ScanDot sdot;
	StrDot *sdp;
	uint flags = mtp->flags & SOpt_All;
	char *strz,str[strlen(sp->d_str) + 2];

	if(!(mtp->flags & SOpt_Ignore))
		flags |= SXeq_Exact;

	// Get ready.  Create local copy of string with two terminating nulls to match on so that nextch() and end-of-line
	// matches work properly.
	*resultp = -1;
	sdot.type = ScanDot_Str;
	sdp = &sdot.u.sd;
	*(strz = stpcpy(str,sp->d_str) + 1) = '\0';
	(sfp = &mtp->groups[0].ml.str)->sd.str0 = sdp->str0 = str;
	if(scanoff >= 0) {
		mcp = mtp->mcpat;
		sdp->str = str + scanoff;
		flags |= Forward;
		}
	else {
		mcp = mtp->bmcpat;
		sdp->str = strz - 1;
		flags |= Backward;
		}

	// Scan the string until we find a match or hit the end (second terminating null) or beginning.
	for(;;) {
		// Save the current scan position in the match record so that if a match is found, savematch() can save the
		// matched string (group 0) and the string offset of the match can be returned.  Also set the length to zero so
		// that amatch() can compute the match length.
		sfp->sd.str = sdp->str;
		sfp->len = 0;

		// Scan the string for a match.
		if(amatch(mcp,flags,&sfp->len,&sdot)) {

			// A SuccessFULL MATCH!  Save result and return offset.  If we were scanning backward, set the "match"
			// pointer to the beginning of the string first.
			if(flags & Backward)
				sfp->sd.str = sdp->str;
			(void) savematch(mtp);
			*resultp = ((flags & Backward) ? sdp->str : sfp->sd.str) - str;
			break;
			}

		// Advance the pointer.
		if(flags & Backward) {
			if(--sdp->str < str)
				break;
			}
		else if(++sdp->str == strz)
			break;
		}

	return rc.status;
	}

// Build and pop up a buffer containing all the strings in the search ring.  Render buffer and return status.
int showSearchRing(Datum *rp,int n,Datum **argpp) {

	return showRing(rp,n,&sring);
	}

#if MMDebug & Debug_ShowRE
// Build and pop up a buffer containing the search and replacement metacharacter arrays.  Render buffer and return status.
int showRegexp(Datum *rp,int n,Datum **argpp) {
	uint m;
	Buffer *srlistp;
	MetaChar *mcp;
	ReplMetaChar *rmcp;
	DStrFab rpt;
	char *bm,*bmz,*str,*lit,wkbuf[NWork];
	char patbuf[srch.m.patlen + OptCh_N + 1];
	struct mcinfo {
		char *hdr;
		MetaChar *pat;
		} *mcobjp;
	static struct mcinfo mcobj[] = {
		{text997,srch.m.mcpat},
			// "Forward"
		{text998,srch.m.bmcpat},
			// "Backward"
		{NULL,NULL}};

	// Get a buffer and open a string-fab object.
	if(sysbuf(text996,&srlistp,0) != Success)
			// "RegexpInfo"
		return rc.status;
	if(dopentrk(&rpt) != 0)
		goto LibFail;

	if(dputf(&rpt,"Match flags: %.4x\n\n",srch.m.flags) != 0)
		goto LibFail;
	mkpat(patbuf,&srch.m);
	do {
		// Construct the search header lines.
		if(mcobjp->pat == srch.m.bmcpat && dputs("\n\n",&rpt) != 0)
			goto LibFail;
		if(dputf(&rpt,"%s %s %s /",mcobjp->hdr,text994,text999) != 0 || dvizs(patbuf,0,VBaseDef,&rpt) != 0 ||
		 dputs("/\n",&rpt) != 0)
						// "SEARCH","PATTERN"
			goto LibFail;

		// Loop through search pattern.
		mcp = mcobjp->pat;
		for(;;) {
			str = stpcpy(wkbuf,"    ");

			// Any closure modifiers?
			if(mcp->mc_type & MCE_Closure) {
				str = stpcpy(wkbuf,"    ");
				sprintf(str,"%hd",mcp->cl.min);
				str = strchr(str,'\0');
				if(mcp->cl.max != mcp->cl.min) {
					if(mcp->cl.max < 0)
						str = stpcpy(str," or more");
					else {
						sprintf(str," to %hd",mcp->cl.max);
						str = strchr(str,'\0');
						}
					}
				*str++ = ' ';
				if(mcp->mc_type & MCE_MinClosure)
					str = stpcpy(str,"(minimum) ");
				strcpy(str,"of:\n");
				if(dputs(wkbuf,&rpt) != 0)
					goto LibFail;
				str = stpcpy(wkbuf,"        ");
				}

			// Describe metacharacter and any value.
			switch(mcp->mc_type & MCE_BaseType) {
				case MCE_Nil:
					lit = "NIL";
					goto Lit;
				case MCE_LitChar:
					sprintf(str,"%-14s'%c'","Char",mcp->u.lchar);
					break;
				case MCE_Any:
					lit = "Any";
					goto Lit;
				case MCE_CCL:
					lit = "ChClass      ";
					goto ChClass;
				case MCE_NCCL:
					lit = "NegChClass   ";
ChClass:
					if(dputs(wkbuf,&rpt) != 0 || dputs(lit,&rpt) != 0)
						goto LibFail;
					bmz = (bm = (char *) mcp->u.cclmap) + sizeof(EBitMap);
					m = 0;
					do {
						if(m ^= 1)
							if(dputc(' ',&rpt) != 0)
								goto LibFail;
						if(dputf(&rpt,"%.2x",*bm++) != 0)
							goto LibFail;
						} while(bm < bmz);
					if(dputc('\n',&rpt) != 0)
						goto LibFail;
					str = NULL;
					break;
				case MCE_WordBnd:
					lit = (mcp->mc_type & MCE_Not) ? "NotWordBoundary" : "WordBoundary";
					goto Lit;
				case MCE_BOL:
					lit = "BeginLine";
					goto Lit;
				case MCE_EOL:
					lit = "EndLine";
					goto Lit;
				case MCE_BOS:
					lit = "BeginString";
					goto Lit;
				case MCE_EOS:
					lit = "EndString";
					goto Lit;
				case MCE_EOSAlt:
					lit = "EndStringCR";
Lit:
					strcpy(str,lit);
					break;
				case MCE_GrpBegin:
					lit = "GroupBegin";
					goto Grp;
				case MCE_GrpEnd:
					lit = "GroupEnd";
Grp:
					sprintf(str,"%-14s%3u",lit,(uint) (mcp->u.ginfo - srch.m.groups));
				}

			// Add the line to the string-fab object.
			if(str != NULL && (dputs(wkbuf,&rpt) != 0 || dputc('\n',&rpt) != 0))
				goto LibFail;

			// Onward to the next element.
			if(mcp->mc_type == MCE_Nil)
				break;
			++mcp;
			}
		} while((++mcobjp)->hdr != NULL);

	// Construct the replacement header lines.
	if(dputf(&rpt,"\n\n%s %s /",text995,text999) != 0 || dvizs(srch.m.rpat,0,VBaseDef,&rpt) != 0 ||
	 dputs("/\n",&rpt) != 0)
				// "REPLACE","PATTERN"
		goto LibFail;

	// Loop through replacement pattern.
	rmcp = srch.m.rmcpat;
	for(;;) {
		str = stpcpy(wkbuf,"    ");

		// Describe metacharacter and any value.
		switch(rmcp->mc_type) {
			case MCE_Nil:
				strcpy(str,"NIL");
				break;
			case MCE_LitString:
				if(dputs(wkbuf,&rpt) != 0 || dputf(&rpt,"%-14s'","String") != 0 ||
				 dvizs(rmcp->u.rstr,0,VBaseDef,&rpt) != 0 || dputs("'\n",&rpt) != 0)
					goto LibFail;
				str = NULL;
				break;
			case MCE_Group:
				sprintf(str,"%-14s%3d","Group",rmcp->u.grpnum);
				break;
			case MCE_Match:
				strcpy(str,"Matched string");
			}

		// Add the line to the string-fab object.
		if(str != NULL && (dputs(wkbuf,&rpt) != 0 || dputc('\n',&rpt) != 0))
			goto LibFail;

		// Onward to the next element.
		if(rmcp->mc_type == MCE_Nil)
			break;
		++rmcp;
		}

	// Add the results to the buffer.
	if(dclose(&rpt,sf_string) != 0)
LibFail:
		return librcset(Failure);
	if(bappend(srlistp,rpt.sf_datp->d_str) != Success)
		return rc.status;

	// Display results.
	return render(rp,n,srlistp,RendNewBuf | RendReset);
	}
#endif
