// (c) Copyright 2018 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// replace.c	Search and replace functions for MightEMacs.

#include "os.h"
#include "std.h"
#include "bind.h"
#include "exec.h"
#include "search.h"

// Free up any strings in the Regexp replacement arrays and initialize them.
static void rmcclear(Match *mtp) {
	ReplMetaChar *rmcp;

	rmcp = mtp->rmcpat;

	while(rmcp->mc_type != MCE_Nil) {
		if(rmcp->mc_type == MCE_LitString)
			free((void *) rmcp->u.rstr);
		++rmcp;
		}

	mtp->rmcpat[0].mc_type = MCE_Nil;
	}

// Delete a specified length from the current point, then either insert the string directly, or make use of replacement
// meta-array if available.  If replenp is not NULL, set *replenp to length of replacement string.  If lastwasnlp is not NULL,
// set *lastwasnlp to true if last replacement character was a newline; otherwise, false.
static int delins(int dlen,char *instr,bool useRMP,int *replenp,bool *lastwasnlp) {
	char *rstr;
	int replen = 0;

	// Zap what we gotta, and insert its replacement.
	if(ldelete((long) dlen,0) != Success)
		return rc.status;

	if(useRMP) {
		Match *mtp = &srch.m;
		ReplMetaChar *rmcp = mtp->rmcpat;
		while(rmcp->mc_type != MCE_Nil) {
			if(linstr(rstr = rmcp->mc_type == MCE_LitString ? rmcp->u.rstr :
			 rmcp->mc_type == MCE_Match ? mtp->matchp->d_str :
			 fixnull(mtp->groups[rmcp->u.grpnum].matchp->d_str)) != Success)
				return rc.status;
			replen += strlen(rstr);
			++rmcp;
			}
		}
	else {
		if(linstr(instr) != Success)
			return rc.status;
		replen = strlen(rstr = instr);
		}
	if(replenp != NULL)
		*replenp = replen;
	if(lastwasnlp != NULL)
		*lastwasnlp = (*rstr != '\0' && strchr(rstr,'\0')[-1] == '\n');

	return rc.status;
	}

// Create a MCE_LitString object in the replacement RE array.
static int rmclit(ReplMetaChar *rmcp,char *src,int len) {

	rmcp->mc_type = MCE_LitString;
	if((rmcp->u.rstr = (char *) malloc(len)) == NULL)
		return rcset(Panic,0,text94,"rmclit");
			// "%s(): Out of memory!"
	stplcpy(rmcp->u.rstr,src,len);

	return rc.status;
	}

// Set up the replacement RE array.  Note that if there are no meta-characters encountered in the replacement string,
// the array is never actually created -- we will just use the character array rpat[] as the replacement string.  Return status.
int rmccompile(Match *mtp) {
	ReplMetaChar *rmcp;
	char *pat;
	int pc;
	int mj;

	pat = mtp->rpat;
	rmcp = mtp->rmcpat;
	mtp->flags &= ~RRegical;
	mj = 0;

	while(*pat) {
		switch(*pat) {
			case MC_Ditto:

				// If there were non-meta characters in the string before reaching this character, plunk
				// them into the replacement array first.
				if(mj != 0) {
					if(rmclit(rmcp,pat - mj,mj + 1) != Success)
						return rc.status;
					++rmcp;
					mj = 0;
					}
				rmcp->mc_type = MCE_Match;
				++rmcp;
				mtp->flags |= RRegical;
				break;
			case MC_Esc:
				pc = *(pat + 1);		// Peek at next char.
				if(pc >= '0' && pc <= '9') {
					if(mj != 0) {
						if(rmclit(rmcp,pat - mj,mj + 1) != Success)
							return rc.status;
						++rmcp;
						mj = 0;
						}
					rmcp->mc_type = MCE_Group;

					// Group number reference out of range?
					if((rmcp->u.grpnum = pc - '0') > mtp->grpct) {
						(void) rcset(Failure,0,text302,rmcp->u.grpnum,mtp->grpct,mtp->rpat);
							// "No such group (ref: %d, have: %d) in replacement pattern '%s'"
						rmcp->mc_type = MCE_Nil;
						rmcclear(mtp);
						return rc.status;
						}
					++pat;
					}
				else {
					// We use mj + 2 here instead of 1, because we have to count the current character.
					if(rmclit(rmcp,pat - mj,mj + 2) != Success)
						return rc.status;

					// If MC_Esc is not the last character in the string, find out what it is escaping,
					// and overwrite the last character with it.
					if(pc != '\0') {

						// Check for \c.
						switch(pc) {
							case MC_Tab:
								pc = '\t';
								break;
							case MC_CR:
								pc = '\r';
								break;
							case MC_NL:
								pc = '\n';
								break;
							case MC_FF:
								pc = '\f';
								break;
							}
						rmcp->u.rstr[mj] = pc;
						++pat;
						}
					mj = 0;
					}

				++rmcp;
				mtp->flags |= RRegical;
				break;
			default:
				++mj;
			}
		++pat;
		}

	if((mtp->flags & RRegical) && mj > 0) {
		if(rmclit(rmcp,pat - mj,mj + 1) != Success)
			return rc.status;
		++rmcp;
		}

	rmcp->mc_type = MCE_Nil;
	return rc.status;
	}

// Display the query-replace prompt.  If rpat is not NULL (constant replacement pattern), use it directly in the prompt;
// otherwise, build one.  Return status.
static int mlrquery(Datum *rpatp) {
	DStrFab msg;
	Datum *datp;
	Match *mtp = &srch.m;
	char buf[TT_MaxCols + 1];

	// Display the matched string in roughly half the terminal width.
	if(dnewtrk(&datp) != 0 || dopenwith(&msg,datp,SFClear) != 0 || dvizs(mtp->matchp->d_str,0,VBaseDef,&msg) != 0 ||
	 dclose(&msg,sf_string) != 0)
		goto LibFail;
	strfit(buf,(size_t) term.t_ncol / 2 - 9,datp->d_str,0);
	if(mlputs(MLHome | MLTermAttr,text87) != Success || mlputs(0,buf) != Success ||
				// "~bReplace~0 \""
	 mlputs(MLTermAttr,text382) != Success)
			// "\" ~bwith~0 \""
		return rc.status;

	// Display the replacement string.  If not Regexp matching or replacement pattern contains no metacharacters, rpatp
	// (from caller) will contain the replacement string (which is constant); otherwise, need to build it from the matched
	// string.
	if(rpatp == NULL) {
		ReplMetaChar *rmcp = mtp->rmcpat;

		// Save entire replacement string in a string-fab object (temporarily).
		if(dopenwith(&msg,datp,SFClear) != 0)
			goto LibFail;
		while(rmcp->mc_type != MCE_Nil) {
			if(dvizs(rmcp->mc_type == MCE_LitString ? rmcp->u.rstr : rmcp->mc_type == MCE_Match ?
			 mtp->matchp->d_str : mtp->groups[rmcp->u.grpnum].matchp->d_str,0,VBaseDef,&msg) != 0)
				goto LibFail;
			++rmcp;
			}
		if(dclose(&msg,sf_string) != 0)
LibFail:
			return librcset(Failure);
		rpatp = datp;
		}
	strfit(buf,(size_t) term.t_ncol - si.mlcol - 3,rpatp->d_str,0);
	if(mlputs(0,buf) == Success)
		(void) mlputs(MLNoEOL | MLFlush,"\"?");
	return rc.status;
	}

// Search for a string in the current buffer and replace it with another string.  If script mode, argpp contains arguments;
// otherwise, get them interactively.  If rp is not NULL, do query replace and, if script mode, set rp to false if search ends
// prematurely; otherwise, true.  If dot has moved after search ends, set mark WrkMark to original position and notify user of
// such.  Return status.
int replstr(Datum *rp,int n,bool qrepl,Datum **argpp) {
	Match *mtp = &srch.m;
	Region *regp = &mtp->groups[0].ml.reg;
	Dot *dotp = &si.curwp->w_face.wf_dot;
	Datum *rpatp = NULL;			// Constant replacement pattern or NULL.
	bool forever;				// True if default n.
	bool lastwasno;				// True if last query response was 'no'.
	bool goback;				// Return to starting position when done.
	bool lastwasnl;				// True if last replacement char was a newline.
	bool lasthiteob = false;		// True if last search matched newline at end-of-buffer.
	bool mcsearch;				// True if using RE search.
	bool useRMP;				// True if using replacement meta-pattern.
	int status;				// Success flag on pattern inputs.
	int numsub;				// Number of substitutions.
	ushort ek;				// Query input extended key.
	Mark origdot;				// Original dot position and offset (for . query response).
	struct {
		Datum *matchstrp;		// Last match string.
		Dot matchdot;			// Dot position of last match.
		int matchlen;			// Last match length.
		Dot repldot;			// Dot position after last replacement.
		int replen;			// Length of replacement string.
		} last;				// Info about last replacement (for undo).

	// Get the pattern and replacement string.
	if(getpat(argpp,qrepl ? text85 : text84,true) != Success || getpat(argpp + 1,text86,false) != Success)
				// "Query replace" or "Replace"			"with"
		return rc.status;

	// Check repeat count.
	forever = goback = false;
	if(n == INT_MIN || n == 0)
		forever = true;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"

	// Clear search groups.
	grpclear(mtp);

	// Create search tables if needed.  First, compile patterns as REs if requested.
	if(rebmode()) {
		if((mtp->mcpat[0].mc_type == MCE_Nil && mccompile(mtp) != Success) ||
		 (mtp->rmcpat[0].mc_type == MCE_Nil && rmccompile(mtp) != Success))
			return rc.status;
		}

	// Compile as a plain-text pattern if not an RE request or not really an RE (SRegical not set).
	mcsearch = !plainsearch();
	if(!mcsearch && (srch.fdelta1[0] == -1 || (((mtp->flags & SCpl_Exact) != 0) != exactbmode())))
		mkdeltas();

	// Create visible form of replacement pattern if possible.
	if(!rebmode() || !(srch.m.flags & RRegical)) {
		DStrFab sf;

		if(dopentrk(&sf) != 0 || dvizs(srch.m.rpat,0,VBaseDef,&sf) != 0 || dclose(&sf,sf_string) != 0)
			goto LibFail;
		rpatp = sf.sf_datp;
		}

	// Save original point position and initialize counters.
	if(!qrepl)
		last.matchdot.lnp = NULL;
	else if(dnewtrk(&last.matchstrp) != 0)
		goto LibFail;
	origdot.mk_dot = si.curwp->w_face.wf_dot;
	origdot.mk_rfrow = getwpos(si.curwp);
	numsub = 0;
	last.repldot.lnp = NULL;
	lastwasno = false;
	useRMP = rebmode() && (mtp->flags & RRegical);
	if(n == 0)
		goto Rpt;

	// Scan the buffer until we find the nth match or hit a buffer boundary.  The basic loop is to find the next match and
	// process it, jump forward in the buffer past the match (if it was not replaced), and repeat.
	for(;;) {
		// Search for the pattern.  The scanning routines will set regp->r_size to the true length of the
		// matched string.
		if(mcsearch) {
			if((status = mcscan(1,Forward)) == NotFound)
				break;			// All done.
			}
		else if((status = scan(1,Forward)) == NotFound)
			break;				// All done.
		if(status != Success)
			return rc.status;

		// Match found.  Set flag if dot is now at end-of-buffer (so we can force loop exit after this iteration),
		// save length, and move to beginning of match.
		if(bufend(dotp))
			lasthiteob = true;
		(void) movech(-regp->r_size);

		// If we are not doing query-replace, make sure that we are not at the same buffer position as the last match or
		// replacement if we have currently matched an empty string (possible in Regexp mode) because we'll go into an
		// infinite loop.
		if(!qrepl) {
			if(regp->r_size == 0 && ((dotp->lnp == last.matchdot.lnp && dotp->off == last.matchdot.off) ||
			 (dotp->lnp == last.repldot.lnp && dotp->off == last.repldot.off)))
				return rcset(Failure,RCNoFormat,text91);
					// "Repeating match at same position detected"
			last.matchdot = *dotp;
			}
		else {
			// Build the query prompt and display it.
			if(mlrquery(rpatp) != Success)
				return rc.status;

			lastwasno = false;

			// Update the position on the mode line if needed.
			if(modeset(MdIdxLine,NULL) || modeset(MdIdxCol,NULL))
				si.curwp->w_flags |= WFMode;
QPrompt:
			// Show the proposed place to change.  Set hard update flag so that any previous replacement that is
			// still visible will be updated on screen.
			si.curwp->w_flags |= WFHard;
			if(update(INT_MIN) != Success)
				return rc.status;

			// Loop until response not undo or valid undo...
			for(;;) {
				if(getkey(&ek,false) != Success)	// Get a key.
					return rc.status;
				if(ek != 'u' || last.repldot.lnp != NULL)
					break;
				if(TTbeep() != Success)		// Nothing to undo.
					return rc.status;
				}

			// Clear the prompt and respond appropriately.
			if(mlerase() != Success)
				return rc.status;
			if(ek == corekeys[CK_Abort].ek)		// Abort and stay at current buffer position.
				return abortinp();
			if(ek == 'q' || ek == (Ctrl | '['))	// Quit or escape key...
				break;				// stop and stay at current buffer position.
			switch(ek) {
				case 'Y':			// Yes, substitute and stop.
					forever = false;
					n = numsub + 1;
				case 'y':			// Yes, substitute.
				case ' ':
					break;
				case 'n':			// No, onward.
					// Skip past the current match.
					(void) movech(1);
					lastwasno = true;
					continue;
				case '!':			// Yes, stop asking.
					qrepl = false;
					break;
				case 'u':			// Undo last and re-prompt.
					// Restore old position.
					si.curwp->w_face.wf_dot = last.repldot;
					last.repldot.lnp = NULL;

					// Delete the new string, restore the old match.
					(void) movech(-last.replen);
					if(delins(last.replen,last.matchstrp->d_str,false,NULL,NULL) != Success)
						return rc.status;

					// Decrement substitution counter, back up (to beginning of prior match), and reprompt.
					--numsub;
					(void) movech(-last.matchlen);
					continue;
				case '.':	// Stop and return to original buffer position.
					goback = true;
					goto Retn;
				default:	// Complain and beep.
					if(TTbeep() != Success)
						return rc.status;
				case '?':	// Help me.
					if(mlputs(MLHome | MLTermAttr | MLFlush,text90) != Success)
			// "~uSPC~U|~uy~U ~bYes~0, ~un~U ~bNo~0, ~uY~U ~bYes and stop~0, ~u!~U ~bDo rest~0, ~uu~U ~bUndo last~0,
			// ~uESC~U|~uq~U ~bStop here~0, ~u.~U ~bStop and go back~0, ~u?~U ~bHelp~0",
						return rc.status;
					goto QPrompt;
				}	// End of switch.
			}	// End of "if qrepl"

		// Do replacement.  If current line is the point origin line, set flag (and use last.repldot.lnp temporarily) so
		// we a can update the line pointer after the substitution in case the line was reallocated by delins().
		if(dotp->lnp == origdot.mk_dot.lnp) {
			origdot.mk_dot.lnp = NULL;
			last.repldot.lnp = (dotp->lnp == si.curbp->b_lnp) ? NULL : dotp->lnp->l_prevp;
			}

		// Delete the sucker, insert its replacement, and count it.
		if(delins(regp->r_size,mtp->rpat,useRMP,&last.replen,&lastwasnl) != Success)
			return rc.status;
		++numsub;

		// Update origin line pointer if needed.
		if(origdot.mk_dot.lnp == NULL)
			origdot.mk_dot.lnp = (last.repldot.lnp == NULL) ? si.curbp->b_lnp : last.repldot.lnp->l_nextp;

		// Save our position, the match length, and the string matched if we are query-replacing, so that we may undo
		// the replacement if requested.
		last.repldot = si.curwp->w_face.wf_dot;
		if(qrepl) {
			last.matchlen = regp->r_size;
			if(dsetsubstr(mtp->matchp->d_str,regp->r_size,last.matchstrp) != 0)
				goto LibFail;
			}

		// If the last match included the newline at the end of the buffer, we're done.  Delete any extra line and break
		// out.
		if(lasthiteob) {
			if(lastwasnl)
				(void) ldelete(1L,0);
			break;
			}

		// n matches replaced?
		if(!forever && numsub == n)
			break;
		}
Retn:
	// Adjust dot if needed.
	if(goback) {
		si.curwp->w_face.wf_dot = origdot.mk_dot;
		si.curwp->w_rfrow = origdot.mk_rfrow;
		si.curwp->w_flags |= WFReframe;
		}
	else if(qrepl) {
		if(lastwasno) {
			(void) movech(-1);
			si.curwp->w_flags |= WFMove;
			}
		}
Rpt:
	// Report the results.
	(void) rcset(Success,RCForce | RCTermAttr,text92,numsub,numsub == 1 ? "" : "s");
		// "%d substitution%s"
	if(dotp->lnp != origdot.mk_dot.lnp || dotp->off != origdot.mk_dot.off) {
		Mark *mkp;
		DStrFab msg;

		if(mfind(WrkMark,&mkp,MkOpt_Create) != Success)
			return rc.status;
		mkp->mk_dot = origdot.mk_dot;
		mkp->mk_rfrow = origdot.mk_rfrow;
		if(dopenwith(&msg,&rc.msg,SFAppend) != 0 || dputs(", ",&msg) != 0 ||
		 dputc(chcase(*text233),&msg) != 0 || dputf(&msg,text233 + 1,WrkMark) != 0 || dclose(&msg,sf_string) != 0)
				// "Mark ~u%c~U set to previous position"
LibFail:
			return librcset(Failure);
		}

	if(si.opflags & OpScript)
		dsetbool((qrepl && (status == NotFound || numsub == n)) ||
		 (!qrepl && (n == INT_MIN || numsub >= n)),rp);
	return rc.status;
	}

// Free all replacement pattern heap space in given match object.
void freerpat(Match *mtp) {

	rmcclear(mtp);
	(void) free((void *) mtp->rpat);
	(void) free((void *) mtp->rmcpat);
	mtp->rsize = 0;
	}

// Initialize parameters for new replacement pattern, which may be null.
int newrpat(char *pat,Match *mtp) {
	int patlen = strlen(pat);

	// Free up arrays if too big.
	if(mtp->rsize > NPatMax || (mtp->rsize > 0 && (uint) patlen > mtp->rsize))
		freerpat(mtp);

	// Get heap space for arrays if needed.
	if(mtp->rsize == 0) {
		mtp->rsize = patlen < NPatMin ? NPatMin : patlen;
		if((mtp->rpat = (char *) malloc(mtp->rsize + 1)) == NULL ||
		 (mtp->rmcpat = (ReplMetaChar *) malloc((mtp->rsize + 1) * sizeof(ReplMetaChar))) == NULL)
			return rcset(Panic,0,text94,"newrpat");
					// "%s(): Out of memory!"
		mtp->rmcpat[0].mc_type = MCE_Nil;
		}

	// Save replacement pattern.
	strcpy(mtp->rpat,pat);
	if(rsetpat(pat,&rring) != Success)	// Add pattern to replacement ring.
		return rc.status;
	rmcclear(mtp);				// Clear Regexp replacement table.

	return rc.status;
	}

// Build and pop up a buffer containing all the strings in the replacement ring.  Render buffer and return status.
int showReplaceRing(Datum *rp,int n,Datum **argpp) {

	return showRing(rp,n,&rring);
	}
