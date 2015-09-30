// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// replace.c	Search and replace functions for MightEMacs.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

// Delete a specified length from the current point, then either insert the string directly, or make use of replacement
// meta-array.  If replenp is not NULL, set *replenp to length of replacement string.  If lastwasnlp is not NULL, set
// *lastwasnlp to true if last replacement character was a newline; otherwise, false.
static int delins(int dlength,char *instr,bool use_rmc,int *replenp,bool *lastwasnlp) {
	char *rstr;
	ReplMetaChar *rmcp;
	int replen = 0;

	// Zap what we gotta, and insert its replacement.
	if(ldelete((long) dlength,false) != SUCCESS)
		return rc.status;

	if(use_rmc && (modetab[MDR_GLOBAL].flags & MDREGEXP)) {
		rmcp = srch.rmcpat;
		while(rmcp->mc_type != MCE_NIL) {
			if(linstr(rstr = rmcp->mc_type == MCE_LITSTRING ? rmcp->u.rstr :
			 rmcp->mc_type == MCE_DITTO ? srch.patmatch :
			 fixnull(srch.grpmatch[rmcp->u.grpnum])) != SUCCESS)
				return rc.status;
			replen += strlen(rstr);
			++rmcp;
			}
		}
	else {
		if(linstr(instr) != SUCCESS)
			return rc.status;
		replen = strlen(rstr = instr);
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
static int rmccompile(void) {
	ReplMetaChar *rmcp;
	char *patp;
	int pc;
	int mj;

	patp = srch.rpat;
	rmcp = srch.rmcpat;
	mj = 0;

	while(*patp) {
		switch(*patp) {
			case MC_DITTO:

				// If there were non-meta characters in the string before reaching this character, plunk
				// them into the replacement array before processing the current meta-character.
				if(mj != 0) {
					if(rmclit(rmcp,patp - mj,mj + 1) != SUCCESS)
						return rc.status;
					++rmcp;
					mj = 0;
					}
				rmcp->mc_type = MCE_DITTO;
				++rmcp;
				srch.flags |= RREGICAL;
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
					if((rmcp->u.grpnum = pc - '0') > srch.grpct) {
						(void) rcset(FAILURE,0,text302,rmcp->u.grpnum,srch.grpct,srch.rpat);
							// "Group reference %d exceeds maximum (%d) in replacement pattern '%s'"
						rmcp->mc_type = MCE_NIL;
						rmcclear();
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
						rmcp->u.rstr[mj] = pc;
						++patp;
						}
					mj = 0;
					}

				++rmcp;
				srch.flags |= RREGICAL;
				break;
			default:
				++mj;
			}
		++patp;
		}

	if((srch.flags & RREGICAL) && mj > 0) {
		if(rmclit(rmcp,patp - mj,mj + 1) != SUCCESS)
			return rc.status;
		++rmcp;
		}

	rmcp->mc_type = MCE_NIL;
	return rc.status;
	}

// Display the query-replace prompt.  Return status.
static int mlrquery(void) {
	char buf[TT_MAXCOLS + 1];

	// Display the matched string in roughly half the terminal width.
	strfit(buf,(size_t) term.t_ncol / 2 - 9,srch.patmatch,srch.matchlen);
	if(mlprintf(MLHOME | MLFORCE,text87,buf) != SUCCESS)
				// "Replace '%s' with '"
		return rc.status;

	// Display the replacement string.
	if((modetab[MDR_GLOBAL].flags & MDREGEXP) && (srch.flags & RREGICAL)) {
		StrList msg;
		ReplMetaChar *rmcp = srch.rmcpat;

		// Save entire replacement string on the heap (temporarily).
		if(vopen(&msg,NULL,false) != 0)
			return vrcset();
		while(rmcp->mc_type != MCE_NIL) {
			if(vputs(rmcp->mc_type == MCE_LITSTRING ? rmcp->u.rstr : rmcp->mc_type == MCE_DITTO ?
			 srch.patmatch : srch.grpmatch[rmcp->u.grpnum],&msg) != 0)
				return vrcset();
			++rmcp;
			}
		if(vclose(&msg) != 0)
			return vrcset();

		// Now display it via strfit().
		strfit(buf,(size_t) term.t_ncol - ml.ttcol - 2,msg.sl_vp->v_strp,0);
		}
	else
		strfit(buf,(size_t) term.t_ncol - ml.ttcol - 2,srch.rpat,0);
	mlputs(MLFORCE,buf,vz_show);
	mlputs(MLFORCE,"'? ",vz_show);
	return rc.status;
	}

// Search for a string and replace it with another string.  If "rp" is not NULL, do query replace and set rp to false if search
// ends prematurely; otherwise, true.  If dot has moved after search ends, set mark 0 to original position and notify user of
// such.  Return status.
int replstr(Value *rp,int n) {
	bool qrepl = rp != NULL;		// True if querying.
	bool forever;				// True if default n.
	bool lastwasno;				// True if last query response was 'no'.
	bool goback;				// Return to starting position when done.
	bool lastwasnl;				// True if last replacement char was a newline.
	bool lasthiteob = false;		// True if last search matched CR at end-of-buffer.
	int lastmatchlen;			// Last match length (for adjusting dot on exit).
	int status;				// Success flag on pattern inputs.
	int nummatch;				// Number of found matches.
	int numsub;				// Number of substitutions.
	int c;					// Input char for query.
	Mark origdot;				// Original dot position and offset (for . query response).
	Dot lastrepldot;			// Last replacement line and offset (for undo).
	int lastreplmatchlen;			// Closure may alter the match length.
	int replen;				// Length of replacement string.
	static char *oldpatmatch = NULL;	// Allocated memory for undo.
	Dot *dotp = &curwp->w_face.wf_dot;

	// Check repeat count.
	forever = goback = false;
	if(n == INT_MIN) {
		forever = true;
		n = 1;
		}
	else if(n < 0)
		return rcset(FAILURE,0,text39,text137,n,0);
				// "%s (%d) must be %d or greater","Command repeat count"

	// Ask the user for a pattern and replacement string.
	if(readpattern(qrepl ? text85 : text84,true) != SUCCESS || readpattern(text86,false) != SUCCESS)
				// "Query replace" or "Replace"			"with"
		return rc.status;

	if(n == 0)
		return rc.status;		// Nothing else to do.

	// Create search tables if needed.
	if(modetab[MDR_GLOBAL].flags & MDREGEXP)
		if((srch.mcpat[0].mc_type == MCE_NIL && mccompile() != SUCCESS) ||
		 (srch.rmcpat[0].mc_type == MCE_NIL && rmccompile() != SUCCESS))
			return rc.status;
	if(((modetab[MDR_GLOBAL].flags & MDREGEXP) == 0 || (srch.flags & SREGICAL) == 0) && srch.fdelta1[0] == -1)
		mkdeltas();

	// Save original point position and initialize counters.
	origdot.mk_dot = curwp->w_face.wf_dot;
	origdot.mk_force = getwpos();
	lastmatchlen = numsub = nummatch = 0;
	lastrepldot.lnp = NULL;
	lastwasno = false;

	// Scan the buffer until we find the nth match or hit a buffer boundary.  The basic loop is to find the next match and
	// process it, jump forward in the buffer past the match (if it was not replaced), and repeat.
	for(;;) {
		// Search for the pattern.  The scanning routines will set srch.matchlen to the true length of the matched
		// string.
		if((modetab[MDR_GLOBAL].flags & MDREGEXP) && (srch.flags & SREGICAL)) {
			if((status = mcscan(1,FORWARD,PTEND)) == NOTFOUND)
				break;			// All done.
			}
		else if((status = scan(1,FORWARD,PTEND)) == NOTFOUND)
			break;				// All done.
		if(status != SUCCESS)
			return rc.status;

		// Match found.  Count it, set flag if dot is now at end-of-buffer (so we can force loop exit after this
		// iteration), save length, and move to beginning of match.
		++nummatch;
		if(dotp->lnp == curbp->b_hdrlnp)
			lasthiteob = true;
		(void) backch(lastmatchlen = srch.matchlen);

		// Check for query.
		if(qrepl) {
			// Build the query prompt and display it.
			if(mlrquery() != SUCCESS)
				return rc.status;

			lastwasno = false;
qprompt:
			// Show the proposed place to change and update the position on the mode line if needed.
			if(modetab[MDR_GLOBAL].flags & (MDLINE | MDCOL))
				upmode(curbp);
			if(update(true) != SUCCESS)
				return rc.status;

			// Loop until response not undo or valid undo ...
			for(;;) {
				if(getkey(&c) != SUCCESS)	// Get a key.
					return rc.status;
				if(c != 'u' || lastrepldot.lnp != NULL)
					break;
				if(TTbeep() != SUCCESS)		// Nothing to undo.
					return rc.status;
				}

			// Clear the prompt and respond appropriately.
			mlerase(MLFORCE);
			if(c == ckeys.abort)			// Abort and stay at current buffer position.
				return abortinp();
			if(c == 'q' || c == (CTRL | '['))	// Quit or escape key ...
				break;				// stop and stay at current buffer position.
			switch(c) {
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
#if 0
					(void) forwch(srch.matchlen);
#else
					(void) forwch(1);
#endif
					lastwasno = true;
					continue;
				case '!':			// Yes, stop asking.
					qrepl = false;
					break;
				case 'u':			// Undo last and re-prompt.
					// Restore old position.
					curwp->w_face.wf_dot = lastrepldot;
					lastrepldot.lnp = NULL;
					lastrepldot.off = 0;

					// Delete the new string, restore the old match.
					(void) backch(replen);
					if(delins(replen,oldpatmatch,false,NULL,NULL) != SUCCESS)
						return rc.status;

					// Decrement substitution counter, back up, save our place, and reprompt.
					--numsub;
					(void) backch(lastreplmatchlen);
					continue;
				case '.':	// Stop and return to original buffer position.
					goback = true;
					goto out;
				default:	// Complain and beep.
					if(TTbeep() != SUCCESS)
						return rc.status;
				case '?':	// Help me.
					if(mlprintf(MLHOME | MLFORCE,text90) != SUCCESS)
			// "(<SP>,y) Yes (n) No (!) Do rest (u) Undo last (<ESC>,q) Stop here (.) Stop and go back (?) Help"
						return rc.status;
					goto qprompt;
				}	// End of switch.
			}	// End of "if qrepl"

		// Do replacement.  If current line is the point origin line, set flag so we a can update the line pointer after
		// the substitution in case the line is reallocated.
		if(dotp->lnp == origdot.mk_dot.lnp) {
			origdot.mk_dot.lnp = NULL;
			lastrepldot.lnp = lback(dotp->lnp);
			}

		// Delete the sucker, insert its replacement, and count it.
		if(delins(srch.matchlen,srch.rpat,srch.flags & RREGICAL,&replen,&lastwasnl) != SUCCESS)
			return rc.status;
		++numsub;

		// Update origin line pointer if needed.
		if(origdot.mk_dot.lnp == NULL)
			origdot.mk_dot.lnp = lforw(lastrepldot.lnp);

		// Save our position, the match length, and the string matched if we are query-replacing, so that we may undo
		// the replacement if requested.
		lastrepldot = curwp->w_face.wf_dot;
		if(qrepl) {
			lastreplmatchlen = srch.matchlen;

			if(oldpatmatch != NULL)
				free((void *) oldpatmatch);
			if((oldpatmatch = (char *) malloc(srch.matchlen + 1)) == NULL)
				return rcset(PANIC,0,text94,"replstr");
					// "%s(): Out of memory!"
			strcpy(oldpatmatch,srch.patmatch);
			}

		// Make sure that we didn't replace an empty string (possible in Regexp mode) because we'll go into an infinite
		// loop.
		if(srch.matchlen == 0)
			return rcset(FAILURE,0,text91);
				// "Null string replaced, stopping"

		// If the last match included the CR at the end of the buffer, we're done.  Delete any extra line and break out.
		if(lasthiteob) {
			if(lastwasnl)
				(void) ldelete(1L,false);
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
#if 0
		(void) backch(lastmatchlen);
#else
		(void) backch(1);
#endif
		curwp->w_flags |= WFMOVE;
		}

	// Report the results.
	(void) rcset(SUCCESS,RCFORCE,text92,numsub,numsub == 1 ? "" : "s");
		// "%d substitution%s"
	if(dotp->lnp != origdot.mk_dot.lnp || dotp->off != origdot.mk_dot.off) {
		StrList msg;

		if(vopen(&msg,&rc.msg,true) != 0 || vputs(", ",&msg) != 0 ||
		 vputc(chcase(*text233),&msg) != 0 || vputf(&msg,text233 + 1,0) != 0 || vclose(&msg) != 0)
				// "Mark %d set to previous position"
			return vrcset();
		curwp->w_face.wf_mark[0] = origdot;
		}

	return (rp != NULL && vsetstr(status == NOTFOUND ? val_true : val_false,rp) != 0) ? vrcset() : rc.status;
	}

// Free up any strings, and MCE_NIL the ReplMetaChar array.
void rmcclear(void) {
	ReplMetaChar *rmcp;

	rmcp = srch.rmcpat;

	while(rmcp->mc_type != MCE_NIL) {
		if(rmcp->mc_type == MCE_LITSTRING)
			free((void *) rmcp->u.rstr);
		++rmcp;
		}

	srch.rmcpat[0].mc_type = MCE_NIL;
	srch.flags &= ~RREGICAL;
	}
