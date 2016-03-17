// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// replace.c	Search and replace functions for MightEMacs.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

// Free up any strings in the Regexp replacement arrays and initialize them.
static void rmcclear(Match *mtp) {
	ReplMetaChar *rmcp;

	rmcp = mtp->rmcpat;

	while(rmcp->mc_type != MCE_NIL) {
		if(rmcp->mc_type == MCE_LITSTRING)
			free((void *) rmcp->u.rstr);
		++rmcp;
		}

	mtp->rmcpat[0].mc_type = MCE_NIL;
	}

// Delete a specified length from the current point, then either insert the string directly, or make use of replacement
// meta-array if available.  If replenp is not NULL, set *replenp to length of replacement string.  If lastwasnlp is not NULL,
// set *lastwasnlp to true if last replacement character was a newline; otherwise, false.
static int delins(int dlen,char *instrp,bool useRMP,int *replenp,bool *lastwasnlp) {
	char *rstr;
	int replen = 0;

	// Zap what we gotta, and insert its replacement.
	if(ldelete((long) dlen,0) != SUCCESS)
		return rc.status;

	if(useRMP) {
		Match *mtp = &srch.m;
		ReplMetaChar *rmcp = mtp->rmcpat;
		while(rmcp->mc_type != MCE_NIL) {
			if(linstr(rstr = rmcp->mc_type == MCE_LITSTRING ? rmcp->u.rstr :
			 rmcp->mc_type == MCE_MATCH ? mtp->matchp->v_strp :
			 fixnull(mtp->groups[rmcp->u.grpnum].matchp->v_strp)) != SUCCESS)
				return rc.status;
			replen += strlen(rstr);
			++rmcp;
			}
		}
	else {
		if(linstr(instrp) != SUCCESS)
			return rc.status;
		replen = strlen(rstr = instrp);
		}
	if(replenp != NULL)
		*replenp = replen;
	if(lastwasnlp != NULL)
		*lastwasnlp = (*rstr != '\0' && strchr(rstr,'\0')[-1] == '\r');

	return rc.status;
	}

// Create a MCE_LITSTRING object in the replacement RE array.
static int rmclit(ReplMetaChar *rmcp,char *src,int len) {

	rmcp->mc_type = MCE_LITSTRING;
	if((rmcp->u.rstr = (char *) malloc(len)) == NULL)
		return rcset(PANIC,0,text94,"rmclit");
			// "%s(): Out of memory!"
	stplcpy(rmcp->u.rstr,src,len);

	return rc.status;
	}

// Set up the replacement RE array.  Note that if there are no meta-characters encountered in the replacement string,
// the array is never actually created -- we will just use the character array rpat[] as the replacement string.  Return status.
int rmccompile(Match *mtp) {
	ReplMetaChar *rmcp;
	char *patp;
	int pc;
	int mj;

	patp = mtp->rpat;
	rmcp = mtp->rmcpat;
	mtp->flags &= ~RREGICAL;
	mj = 0;

	while(*patp) {
		switch(*patp) {
			case MC_DITTO:

				// If there were non-meta characters in the string before reaching this character, plunk
				// them into the replacement array first.
				if(mj != 0) {
					if(rmclit(rmcp,patp - mj,mj + 1) != SUCCESS)
						return rc.status;
					++rmcp;
					mj = 0;
					}
				rmcp->mc_type = MCE_MATCH;
				++rmcp;
				mtp->flags |= RREGICAL;
				break;
			case MC_ESC:
				pc = *(patp + 1);		// Peek at next char.
				if(pc >= '1' && pc <= '9') {
					if(mj != 0) {
						if(rmclit(rmcp,patp - mj,mj + 1) != SUCCESS)
							return rc.status;
						++rmcp;
						mj = 0;
						}
					rmcp->mc_type = MCE_GROUP;

					// Group number reference out of range?
					if((rmcp->u.grpnum = pc - '0') > mtp->grpct) {
						(void) rcset(FAILURE,0,text302,rmcp->u.grpnum,mtp->grpct,mtp->rpat);
							// "No such group (ref: %d, have: %d) in replacement pattern '%s'"
						rmcp->mc_type = MCE_NIL;
						rmcclear(mtp);
						return rc.status;
						}
					++patp;
					}
				else {
					// We use mj + 2 here instead of 1, because we have to count the current character.
					if(rmclit(rmcp,patp - mj,mj + 2) != SUCCESS)
						return rc.status;

					// If MC_ESC is not the last character in the string, find out what it is escaping,
					// and overwrite the last character with it.
					if(pc != '\0') {

						// Check for \c.
						switch(pc) {
							case MC_TAB:
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
						++patp;
						}
					mj = 0;
					}

				++rmcp;
				mtp->flags |= RREGICAL;
				break;
			default:
				++mj;
			}
		++patp;
		}

	if((mtp->flags & RREGICAL) && mj > 0) {
		if(rmclit(rmcp,patp - mj,mj + 1) != SUCCESS)
			return rc.status;
		++rmcp;
		}

	rmcp->mc_type = MCE_NIL;
	return rc.status;
	}

// Display the query-replace prompt.  Return status.
static int mlrquery(void) {
	Match *mtp = &srch.m;
	char buf[TT_MAXCOLS + 1];

	// Display the matched string in roughly half the terminal width.
	strfit(buf,(size_t) term.t_ncol / 2 - 9,mtp->matchp->v_strp,mtp->groups[0].ml.reg.r_size);
	if(mlprintf(MLHOME | MLFORCE,text87,buf) != SUCCESS)
				// "Replace '%s' with '"
		return rc.status;

	// Display the replacement string.
	if(rebmode() && (mtp->flags & RREGICAL)) {
		StrList msg;
		ReplMetaChar *rmcp = mtp->rmcpat;

		// Save entire replacement string on the heap (temporarily).
		if(vopen(&msg,NULL,false) != 0)
			return vrcset();
		while(rmcp->mc_type != MCE_NIL) {
			if(vputs(rmcp->mc_type == MCE_LITSTRING ? rmcp->u.rstr : rmcp->mc_type == MCE_MATCH ?
			 mtp->matchp->v_strp : mtp->groups[rmcp->u.grpnum].matchp->v_strp,&msg) != 0)
				return vrcset();
			++rmcp;
			}
		if(vclose(&msg) != 0)
			return vrcset();

		// Now display it via strfit().
		strfit(buf,(size_t) term.t_ncol - ml.ttcol - 2,msg.sl_vp->v_strp,0);
		}
	else
		strfit(buf,(size_t) term.t_ncol - ml.ttcol - 2,mtp->rpat,0);
	mlputs(MLFORCE,buf);
	mlputs(MLFORCE,"'? ");
	return rc.status;
	}

// Search for a string and replace it with another string.  If "rp" is not NULL, do query replace and set rp to false if search
// ends prematurely; otherwise, true.  If dot has moved after search ends, set mark '.' to original position and notify user of
// such.  Return status.
int replstr(Value *rp,int n) {
	Match *mtp = &srch.m;
	Region *regp = &mtp->groups[0].ml.reg;
	Dot *dotp = &curwp->w_face.wf_dot;
	bool qrepl = rp != NULL;		// True if querying.
	bool forever;				// True if default n.
	bool lastwasno;				// True if last query response was 'no'.
	bool goback;				// Return to starting position when done.
	bool lastwasnl;				// True if last replacement char was a newline.
	bool lasthiteob = false;		// True if last search matched CR at end-of-buffer.
	bool mcsearch;				// True if using RE search.
	bool useRMP;				// True if using replacement meta-pattern.
	int status;				// Success flag on pattern inputs.
	int nummatch;				// Number of found matches.
	int numsub;				// Number of substitutions.
	uint ek;				// Query input extended key.
	Mark origdot;				// Original dot position and offset (for . query response).
	struct {
		Value *matchstrp;		// Last match string.
		Dot matchdot;			// Dot position of last match.
		int matchlen;			// Last match length.
		Dot repldot;			// Dot position after last replacement.
		int replen;			// Length of replacement string.
		} last;				// Info about last replacement (for undo).

	// Check repeat count.
	forever = goback = false;
	if(n == INT_MIN) {
		forever = true;
		n = 1;
		}
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Repeat count"

	// Ask the user for a pattern and replacement string.
	if(readpattern(qrepl ? text85 : text84,true) != SUCCESS || readpattern(text86,false) != SUCCESS)
				// "Query replace" or "Replace"			"with"
		return rc.status;

	if(n == 0)
		return rc.status;		// Nothing else to do.

	// Clear search groups.
	grpclear(mtp);

	// Create search tables if needed.  First, compile patterns as REs if requested.
	if(rebmode())
		if((mtp->mcpat[0].mc_type == MCE_NIL && mccompile(mtp) != SUCCESS) ||
		 (mtp->rmcpat[0].mc_type == MCE_NIL && rmccompile(mtp) != SUCCESS))
			return rc.status;

	// Compile as a plain-text patterns if not an RE request or not really an RE (SREGICAL not set).
	mcsearch = !plainsearch();
	if(!mcsearch && (srch.fdelta1[0] == -1 || (((mtp->flags & SCPL_EXACT) != 0) != exactbmode())))
		mkdeltas();

	// Save original point position and initialize counters.
	if(!qrepl)
		last.matchdot.lnp = NULL;
	else {
		if(vnew(&last.matchstrp,false) != 0)
			return vrcset();

		// Initialize the screen if it's garbage.
		if((opflags & OPSCREDRAW) && update(true) != SUCCESS)
			return rc.status;
		}
	origdot.mk_dot = curwp->w_face.wf_dot;
	origdot.mk_force = getwpos(curwp);
	numsub = nummatch = 0;
	last.repldot.lnp = NULL;
	lastwasno = false;
	useRMP =  rebmode() && (mtp->flags & RREGICAL);

	// Scan the buffer until we find the nth match or hit a buffer boundary.  The basic loop is to find the next match and
	// process it, jump forward in the buffer past the match (if it was not replaced), and repeat.
	for(;;) {
		// Search for the pattern.  The scanning routines will set regp->r_size to the true length of the
		// matched string.
		if(mcsearch) {
			if((status = mcscan(1,FORWARD)) == NOTFOUND)
				break;			// All done.
			}
		else if((status = scan(1,FORWARD)) == NOTFOUND)
			break;				// All done.
		if(status != SUCCESS)
			return rc.status;

		// Match found.  Count it, set flag if dot is now at end-of-buffer (so we can force loop exit after this
		// iteration), save length, and move to beginning of match.
		++nummatch;
		if(dotp->lnp == curbp->b_hdrlnp)
			lasthiteob = true;
		(void) backch(regp->r_size);

		// If we are not doing query-replace, make sure that we are not at the same buffer position as the last match or
		// replacement if we have currently matched an empty string (possible in Regexp mode) because we'll go into an
		// infinite loop.
		if(!qrepl) {
			if(regp->r_size == 0 && ((dotp->lnp == last.matchdot.lnp && dotp->off == last.matchdot.off) ||
			 (dotp->lnp == last.repldot.lnp && dotp->off == last.repldot.off)))
				return rcset(FAILURE,0,text91);
					// "Repeating match at same position detected"
			last.matchdot = *dotp;
			}
		else {
			// Build the query prompt and display it.
			if(mlrquery() != SUCCESS)
				return rc.status;

			lastwasno = false;

			// Update the position on the mode line if needed.
			if(modetab[MDR_GLOBAL].flags & (MDLINE | MDCOL))
				upmode(curbp);
qprompt:
			// Show the proposed place to change.
			if(update(true) != SUCCESS)
				return rc.status;

			// Loop until response not undo or valid undo...
			for(;;) {
				if(getkey(&ek) != SUCCESS)	// Get a key.
					return rc.status;
				if(ek != 'u' || last.repldot.lnp != NULL)
					break;
				if(TTbeep() != SUCCESS)		// Nothing to undo.
					return rc.status;
				}

			// Clear the prompt and respond appropriately.
			mlerase(MLFORCE);
			if(ek == corekeys[CK_ABORT].ek)		// Abort and stay at current buffer position.
				return abortinp();
			if(ek == 'q' || ek == (CTRL | '['))	// Quit or escape key...
				break;				// stop and stay at current buffer position.
			switch(ek) {
#if FRENCH
				case 'o':			// Oui, substitute.
#elif SPANISH
				case 's':			// Si, substitute.
#else
				case 'y':			// Yes, substitute.
#endif
				case ' ':			// Yes, substitute.
					break;
				case 'n':			// No, onward.
					// Skip past the current match.
					(void) forwch(1);
					lastwasno = true;
					continue;
				case '!':			// Yes, stop asking.
					qrepl = false;
					break;
				case 'u':			// Undo last and re-prompt.
					// Restore old position.
					curwp->w_face.wf_dot = last.repldot;
					last.repldot.lnp = NULL;

					// Delete the new string, restore the old match.
					(void) backch(last.replen);
					if(delins(last.replen,last.matchstrp->v_strp,false,NULL,NULL) != SUCCESS)
						return rc.status;

					// Decrement substitution counter, back up (to beginning of prior match), and reprompt.
					--numsub;
					(void) backch(last.matchlen);
					continue;
				case '.':	// Stop and return to original buffer position.
					goback = true;
					goto out;
				default:	// Complain and beep.
					if(TTbeep() != SUCCESS)
						return rc.status;
				case '?':	// Help me.
					if(mlprintf(MLHOME | MLFORCE,text90) != SUCCESS)
			// "(SPC,y) Yes (n) No (!) Do rest (u) Undo last (ESC,q) Stop here (.) Stop and go back (?) Help"
						return rc.status;
					goto qprompt;
				}	// End of switch.
			}	// End of "if qrepl"

		// Do replacement.  If current line is the point origin line, set flag (and use last.repldot.lnp temporarily) so
		// we a can update the line pointer after the substitution in case the line was reallocated by delins().
		if(dotp->lnp == origdot.mk_dot.lnp) {
			origdot.mk_dot.lnp = NULL;
			last.repldot.lnp = lback(dotp->lnp);
			}

		// Delete the sucker, insert its replacement, and count it.
		if(delins(regp->r_size,mtp->rpat,useRMP,&last.replen,&lastwasnl) != SUCCESS)
			return rc.status;
		++numsub;

		// Update origin line pointer if needed.
		if(origdot.mk_dot.lnp == NULL)
			origdot.mk_dot.lnp = lforw(last.repldot.lnp);

		// Save our position, the match length, and the string matched if we are query-replacing, so that we may undo
		// the replacement if requested.
		last.repldot = curwp->w_face.wf_dot;
		if(qrepl) {
			last.matchlen = regp->r_size;
			if(vsetfstr(mtp->matchp->v_strp,regp->r_size,last.matchstrp) != 0)
				return vrcset();
			}

		// If the last match included the CR at the end of the buffer, we're done.  Delete any extra line and break out.
		if(lasthiteob) {
			if(lastwasnl)
				(void) ldelete(1L,0);
			break;
			}

		// n matches replaced?
		if(!forever && nummatch == n)
			break;
		}
out:
	// Adjust dot if needed.
	if(goback) {
		curwp->w_face.wf_dot = origdot.mk_dot;
		curwp->w_force = origdot.mk_force;
		curwp->w_flags |= WFFORCE;
		}
	else if(qrepl && lastwasno) {
		(void) backch(1);
		curwp->w_flags |= WFMOVE;
		}

	// Report the results.
	(void) rcset(SUCCESS,RCFORCE,text92,numsub,numsub == 1 ? "" : "s");
		// "%d substitution%s"
	if(dotp->lnp != origdot.mk_dot.lnp || dotp->off != origdot.mk_dot.off) {
		Mark *mkp;
		StrList msg;

		if(mfind(WMARK,&mkp,MKOPT_CREATE) != SUCCESS)
			return rc.status;
		mkp->mk_dot = origdot.mk_dot;
		mkp->mk_force = origdot.mk_force;
		if(vopen(&msg,&rc.msg,true) != 0 || vputs(", ",&msg) != 0 ||
		 vputc(chcase(*text233),&msg) != 0 || vputf(&msg,text233 + 1,WMARK) != 0 || vclose(&msg) != 0)
				// "Mark '%c' set to previous position"
			return vrcset();
		}

	return (rp != NULL && vsetstr(status == NOTFOUND ? val_true : val_false,rp) != 0) ? vrcset() : rc.status;
	}

// Free all replacement pattern heap space in given match object.
void freerpat(Match *mtp) {

	rmcclear(mtp);
	(void) free((void *) mtp->rpat);
	(void) free((void *) mtp->rmcpat);
	mtp->rsize = 0;
	}

// Initialize parameters for new replacement pattern, which may be null.
int newrpat(char *patp,Match *mtp) {
	int patlen = strlen(patp);

	// Free up arrays if too big.
	if(mtp->rsize > NPATMAX || (mtp->rsize > 0 && (uint) patlen > mtp->rsize))
		freerpat(mtp);

	// Get heap space for arrays if needed.
	if(mtp->rsize == 0) {
		mtp->rsize = patlen < NPATMIN ? NPATMIN : patlen;
		if((mtp->rpat = (char *) malloc(mtp->rsize + 1)) == NULL ||
		 (mtp->rmcpat = (ReplMetaChar *) malloc((mtp->rsize + 1) * sizeof(ReplMetaChar))) == NULL)
			return rcset(PANIC,0,text94,"newrpat");
					// "%s(): Out of memory!"
		mtp->rmcpat[0].mc_type = MCE_NIL;
		}

	// Save replacement string.
	strcpy(mtp->rpat,patp);
	rmcclear(mtp);				// Clear Regexp replacement table.

	return rc.status;
	}
