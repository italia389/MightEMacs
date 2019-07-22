// (c) Copyright 2019 Richard W. Marinelli
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

// Query replace control data.
typedef struct {
	Datum *matchStr;		// Last match string.
	Point matchPoint;		// Point position of last match.
	int matchLen;			// Last match length.
	Point replPoint;		// Point position after last replacement.
	int replen;			// Length of replacement string.
	} LastMatch;			// Info about last replacement (for undo).
typedef struct {
	Datum *rpat;			// Constant replacement pattern or NULL.
	int numSub;			// Number of substitutions.
	Mark origPoint;			// Original point position and offset (for . query response).
	Match *match;			// Match object pointer.
	LastMatch last;			// Match information.
	ushort flags;			// Control flags.
	} QueryControl;

#define QR_Forever	0x0001		// Do replacements until end of buffer.
#define QR_GoBack	0x0002		// Return to starting position when done.
#define QR_LastWasNo	0x0004		// Last query response was "no".
#define QR_LastWasNL	0x0008		// Last replacement char was a newline.
#define QR_LastHitEOB	0x0010		// Last search matched newline at end of buffer.
#define QR_Regexp	0x0020		// Performing RE search.
#define QR_UseRMP	0x0040		// Using replacement meta-pattern.

// Free up any strings in the Regexp replacement arrays and initialize them.
static void rmcclear(Match *match) {
	ReplMetaChar *rmc;

	rmc = match->rmcpat;

	while(rmc->mc_type != MCE_Nil) {
		if(rmc->mc_type == MCE_LitString)
			free((void *) rmc->u.rstr);
		++rmc;
		}

	match->rmcpat[0].mc_type = MCE_Nil;
	}

// Delete a specified length from the current point, then either insert the string directly, or make use of replacement
// meta-array if available.  If qc not NULL, set replen in LastMatch record to length of replacement string and set or clear
// QR_LastWasNL flag if last replacement character was or was not a newline.
static int delins(int dlen,char *instr,bool useRMP,QueryControl *qc) {
	char *rstr;
	int rlen = 0;

	// Zap what we gotta, and insert its replacement.
	if(ldelete((long) dlen,0) != Success)
		return rc.status;

	if(useRMP) {
		Match *match = &srch.m;
		ReplMetaChar *rmc = match->rmcpat;
		while(rmc->mc_type != MCE_Nil) {
			if(linstr(rstr = rmc->mc_type == MCE_LitString ? rmc->u.rstr :
			 rmc->mc_type == MCE_Match ? match->match->d_str :
			 fixnull(match->groups[rmc->u.grpnum].match->d_str)) != Success)
				return rc.status;
			rlen += strlen(rstr);
			++rmc;
			}
		}
	else {
		if(linstr(instr) != Success)
			return rc.status;
		rlen = strlen(rstr = instr);
		}
	if(qc != NULL) {
		qc->last.replen = rlen;
		if(*rstr != '\0' && strchr(rstr,'\0')[-1] == '\n')
			qc->flags |= QR_LastWasNL;
		else
			qc->flags &= ~QR_LastWasNL;
		}

	return rc.status;
	}

// Create a MCE_LitString object in the replacement RE array.
static int rmclit(ReplMetaChar *rmc,char *src,int len) {

	rmc->mc_type = MCE_LitString;
	if((rmc->u.rstr = (char *) malloc(len)) == NULL)
		return rcset(Panic,0,text94,"rmclit");
			// "%s(): Out of memory!"
	stplcpy(rmc->u.rstr,src,len);

	return rc.status;
	}

// Set up the replacement RE array.  Note that if there are no meta-characters encountered in the replacement string,
// the array is never actually created -- we will just use the character array rpat[] as the replacement string.  Return status.
int rmccompile(Match *match) {
	ReplMetaChar *rmc;
	char *pat;
	int pc;
	int mj;

	pat = match->rpat;
	rmc = match->rmcpat;
	match->flags &= ~RRegical;
	mj = 0;

	while(*pat) {
		switch(*pat) {
			case MC_Ditto:

				// If there were non-meta characters in the string before reaching this character, plunk
				// them into the replacement array first.
				if(mj != 0) {
					if(rmclit(rmc,pat - mj,mj + 1) != Success)
						return rc.status;
					++rmc;
					mj = 0;
					}
				rmc->mc_type = MCE_Match;
				++rmc;
				match->flags |= RRegical;
				break;
			case MC_Esc:
				pc = *(pat + 1);		// Peek at next char.
				if(pc >= '0' && pc <= '9') {
					if(mj != 0) {
						if(rmclit(rmc,pat - mj,mj + 1) != Success)
							return rc.status;
						++rmc;
						mj = 0;
						}
					rmc->mc_type = MCE_Group;

					// Group number reference out of range?
					if((rmc->u.grpnum = pc - '0') > match->grpct) {
						(void) rcset(Failure,0,text302,rmc->u.grpnum,match->grpct,match->rpat);
							// "No such group (ref: %d, have: %d) in replacement pattern '%s'"
						rmc->mc_type = MCE_Nil;
						rmcclear(match);
						return rc.status;
						}
					++pat;
					}
				else {
					// We use mj + 2 here instead of 1, because we have to count the current character.
					if(rmclit(rmc,pat - mj,mj + 2) != Success)
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
						rmc->u.rstr[mj] = pc;
						++pat;
						}
					mj = 0;
					}

				++rmc;
				match->flags |= RRegical;
				break;
			default:
				++mj;
			}
		++pat;
		}

	if((match->flags & RRegical) && mj > 0) {
		if(rmclit(rmc,pat - mj,mj + 1) != Success)
			return rc.status;
		++rmc;
		}

	rmc->mc_type = MCE_Nil;
	return rc.status;
	}

// Display the query-replace prompt.  If rpat is not NULL (constant replacement pattern), use it directly in the prompt;
// otherwise, build one.  Return status.
static int mlrquery(Datum *rpat) {
	DStrFab msg;
	Datum *datum;
	Match *match = &srch.m;
	char buf[TT_MaxCols + 1];

	// Display the matched string in roughly half the terminal width.
	if(dnewtrk(&datum) != 0 || dopenwith(&msg,datum,SFClear) != 0 || dvizs(match->match->d_str,0,VBaseDef,&msg) != 0 ||
	 dclose(&msg,sf_string) != 0)
		goto LibFail;
	strfit(buf,(size_t) term.t_ncol / 2 - 9,datum->d_str,0);
	if(mlputs(MLHome | MLTermAttr,text87) != Success || mlputs(0,buf) != Success ||
				// "~bReplace~B \""
	 mlputs(MLTermAttr,text382) != Success)
			// "\" ~bwith~B \""
		return rc.status;

	// Display the replacement string.  If not Regexp matching or replacement pattern contains no metacharacters, rpat
	// (from caller) will contain the replacement string (which is constant); otherwise, need to build it from the matched
	// string.
	if(rpat == NULL) {
		ReplMetaChar *rmc = match->rmcpat;

		// Save entire replacement string in a string-fab object (temporarily).
		if(dopenwith(&msg,datum,SFClear) != 0)
			goto LibFail;
		while(rmc->mc_type != MCE_Nil) {
			if(dvizs(rmc->mc_type == MCE_LitString ? rmc->u.rstr : rmc->mc_type == MCE_Match ?
			 match->match->d_str : match->groups[rmc->u.grpnum].match->d_str,0,VBaseDef,&msg) != 0)
				goto LibFail;
			++rmc;
			}
		if(dclose(&msg,sf_string) != 0)
LibFail:
			return librcset(Failure);
		rpat = datum;
		}
	strfit(buf,(size_t) term.t_ncol - term.mlcol - 3,rpat->d_str,0);
	if(mlputs(0,buf) == Success)
		(void) mlputs(MLNoEOL | MLFlush,"\"?");
	return rc.status;
	}

// Return true if point has moved; otherwise, false.
static bool newpos(Point *point,QueryControl *qc) {

	return point->lnp != qc->origPoint.mk_point.lnp || point->off != qc->origPoint.mk_point.off;
	}

// Get response (key) from user and store in *ek.  Return status.
static int response(ushort *ek,QueryControl *qc) {

	// Loop until response not undo or valid undo...
	for(;;) {
		if(getkey(true,ek,false) != Success)	// Get a key.
			return rc.status;
		if(*ek != 'u' || qc->last.replPoint.lnp != NULL)
			break;
		ttbeep();				// Nothing to undo.
		}

	// Clear the prompt and respond appropriately.
	if(mlerase() != Success)
		return rc.status;
	return (*ek == corekeys[CK_Abort].ek) ? abortinp() : rc.status;
	}

// Search for a string in the current buffer and replace it with another string.  If script mode, argv contains arguments;
// otherwise, get them interactively.  If rval is not NULL, do query replace and, if script mode, set rval to false if search
// ends prematurely; otherwise, true.  If point has moved after search ends, set mark WrkMark to original position and notify
// user of such.  Return status.
int replstr(Datum *rval,int n,bool qrepl,Datum **argv) {
	int status;
	ushort ek;
	QueryControl qc;
	Region *reg;
	Point *point = &si.curwin->w_face.wf_point;

	// Get ready.
	qc.rpat = NULL;
	qc.flags = 0;
	qc.match = &srch.m;
	reg = &qc.match->groups[0].ml.reg;

	// Get the pattern and replacement string.
	if(getpat(argv,qrepl ? text85 : text84,true) != Success || getpat(argv + 1,text86,false) != Success)
				// "Query replace" or "Replace"			"with"
		return rc.status;

	// Check repeat count.
	if(n == INT_MIN || n == 0)
		qc.flags = QR_Forever;
	else if(n < 0)
		return rcset(Failure,0,text39,text137,n,0);
			// "%s (%d) must be %d or greater","Repeat count"

	// Clear search groups.
	grpclear(qc.match);

	// Create search tables if needed.  First, compile patterns as REs if requested.
	if(rebmode()) {
		if((qc.match->mcpat[0].mc_type == MCE_Nil && mccompile(qc.match) != Success) ||
		 (qc.match->rmcpat[0].mc_type == MCE_Nil && rmccompile(qc.match) != Success))
			return rc.status;
		}

	// Compile as a plain-text pattern if not an RE request or not really an RE (SRegical not set).
	if(!plainsearch())
		qc.flags |= QR_Regexp;
	if(!(qc.flags & QR_Regexp) && (srch.fdelta1[0] == -1 || (((qc.match->flags & SCpl_Exact) != 0) != exactbmode())))
		mkdeltas();

	// Create visible form of replacement pattern if possible.
	if(!rebmode() || !(srch.m.flags & RRegical)) {
		DStrFab sfab;

		if(dopentrk(&sfab) != 0 || dvizs(srch.m.rpat,0,VBaseDef,&sfab) != 0 || dclose(&sfab,sf_string) != 0)
			goto LibFail;
		qc.rpat = sfab.sf_datum;
		}

	// Save original point position and initialize counters.
	if(!qrepl)
		qc.last.matchPoint.lnp = NULL;
	else if(dnewtrk(&qc.last.matchStr) != 0)
		goto LibFail;
	qc.origPoint.mk_point = *point;
	qc.origPoint.mk_rfrow = getwpos(si.curwin);
	qc.numSub = 0;
	qc.last.replPoint.lnp = NULL;
	if(rebmode() && (qc.match->flags & RRegical))
		qc.flags |= QR_UseRMP;
	if(n == 0)
		goto Rpt;

	// Scan the buffer until we make the nth substitution or hit a buffer boundary.  If point has moved at that time and
	// interactive, prompt for action to take.
	for(;;) {
		// Scan the buffer.  The basic loop is to find the next match and process it, jump forward in the buffer past
		// the match (if it was not replaced), and repeat.
		for(;;) {
			// Search for the pattern.  The scanning routines will set reg->r_size to the true length of the
			// matched string.
			if(qc.flags & QR_Regexp) {
				if((status = mcscan(1,Forward)) == NotFound)
					break;			// All done.
				}
			else if((status = scan(1,Forward)) == NotFound)
				break;				// All done.
			if(status != Success)
				return rc.status;

			// Match found.  Set flag if point is now at end-of-buffer (so we can force loop exit after this
			// iteration), save length, and move to beginning of match.
			if(bufend(point))
				qc.flags |= QR_LastHitEOB;
			(void) movech(-reg->r_size);

			// If we are not doing query-replace, make sure that we are not at the same buffer position as the last
			// match or replacement if we have currently matched an empty string (possible in Regexp mode) because
			// we'll go into an infinite loop.
			if(!qrepl) {
				if(reg->r_size == 0 &&
				 ((point->lnp == qc.last.matchPoint.lnp && point->off == qc.last.matchPoint.off) ||
				 (point->lnp == qc.last.replPoint.lnp && point->off == qc.last.replPoint.off)))
					return rcset(Failure,RCNoFormat,text91);
						// "Repeating match at same position detected"
				qc.last.matchPoint = *point;
				}
			else {
				// Build the query prompt and display it.
				if(mlrquery(qc.rpat) != Success)
					return rc.status;

				qc.flags &= ~QR_LastWasNo;

				// Update the position on the mode line if needed.
				if(modeset(MdIdxLine,NULL) || modeset(MdIdxCol,NULL))
					si.curwin->w_flags |= WFMode;

				// Loop until get valid response.  Show the proposed place to change and set hard update flag so
				// that any previous replacement that is still visible will be updated on screen.
				si.curwin->w_flags |= WFHard;
				for(;;) {
					if(update(INT_MIN) != Success)
						return rc.status;
					if(response(&ek,&qc) != Success)
						return rc.status;
					switch(ek) {
						case 'Y':			// Yes, substitute and stop.
							qc.flags &= ~QR_Forever;
							n = qc.numSub + 1;
							// Fall through.
						case 'y':			// Yes, substitute.
						case ' ':
							goto Replace;
						case 'n':			// No, onward.
							(void) movech(1);	// Skip past the current match.
							qc.flags |= QR_LastWasNo;
							goto Onward;
						case '!':			// Yes, stop asking.
							qrepl = false;
							goto Replace;
						case '.':			// Stop and return to original buffer position.
							qc.flags |= QR_GoBack;
							goto Retn;
						case 'q':			// Quit or escape key...
						case Ctrl | '[':
							goto Retn;		// stop and stay at current buffer position.
						case 'r':			// Restart: go back and start over.
							goto Restart;
						case 'u':			// Undo last and re-prompt.
Undo:
							// Restore old position.
							si.curwin->w_face.wf_point = qc.last.replPoint;
							qc.last.replPoint.lnp = NULL;

							// Delete the new string, restore the old match.
							(void) movech(-qc.last.replen);
							if(delins(qc.last.replen,qc.last.matchStr->d_str,false,NULL) != Success)
								return rc.status;

							// Decrement substitution counter, back up (to beginning of prior
							// match), and reprompt.
							--qc.numSub;
							(void) movech(-qc.last.matchLen);
							goto Onward;
						default:			// Complain and beep.
							ttbeep();
							// Fall through.
						case '?':			// Display help.
							if(mlputs(MLHome | MLTermAttr | MLFlush,text90) != Success)
					// "~uSPC~U|~uy~U ~bYes~B, ~un~U ~bNo~B, ~uY~U ~bYes and stop~B, ~u!~U ~bDo rest~B,
					// ~uu~U ~bUndo last~B, ~ur~U ~bRestart~B, ~uESC~U|~uq~U ~bStop here~B,
					// ~u.~U ~bStop and go back~B, ~u?~U ~bHelp~B",
								return rc.status;
						}	// End of switch.
					}	// End of loop.
				}	// End of "if(qrepl)".
Replace:
			// Do replacement.  If current line is the point origin line, set flag (and use qc.last.replPoint.lnp
			// temporarily) so we a can update the line pointer after the substitution in case the line was
			// reallocated by delins().
			if(point->lnp == qc.origPoint.mk_point.lnp) {
				qc.origPoint.mk_point.lnp = NULL;
				qc.last.replPoint.lnp = (point->lnp == si.curbuf->b_lnp) ? NULL : point->lnp->l_prev;
				}

			// Delete the sucker, insert its replacement, and count it.
			if(delins(reg->r_size,qc.match->rpat,qc.flags & QR_UseRMP,&qc) != Success)
				return rc.status;
			++qc.numSub;

			// Update origin line pointer if needed.
			if(qc.origPoint.mk_point.lnp == NULL)
				qc.origPoint.mk_point.lnp = (qc.last.replPoint.lnp == NULL) ? si.curbuf->b_lnp :
				 qc.last.replPoint.lnp->l_next;

			// Save our position, the match length, and the string matched if we are query-replacing, so that we may
			// undo the replacement if requested.
			qc.last.replPoint = si.curwin->w_face.wf_point;
			if(qrepl) {
				qc.last.matchLen = reg->r_size;
				if(dsetsubstr(qc.match->match->d_str,reg->r_size,qc.last.matchStr) != 0)
					goto LibFail;
				}

			// If the last match included the newline at the end of the buffer, we're done.  Delete any extra line
			// and break out.
			if(qc.flags & QR_LastHitEOB) {
				if(qc.flags & QR_LastWasNL)
					(void) ldelete(1L,0);
				break;
				}

			// n matches replaced?
			if(!(qc.flags & QR_Forever) && qc.numSub == n)
				break;
Onward:;
			}

		// Inner loop completed.  Prompt user for final action to take if applicable.
		if(!qrepl || !newpos(point,&qc))
			break;
		if(mlputs(MLHome | MLTermAttr | MLFlush,text468) != Success)
					// "Scan completed.  Press ~uq~U to quit or ~u.~U to go back."
			return rc.status;
		if(qc.flags & QR_LastWasNo) {
			(void) movech(-1);
			si.curwin->w_flags |= WFMove;
			qc.flags &= ~QR_LastWasNo;
			}

		// Loop until get valid response.
		for(;;) {
			if(update(INT_MIN) != Success)
				return rc.status;
			if(response(&ek,&qc) != Success)
				return rc.status;
			switch(ek) {
				case 'q':			// Stop here.
				case Ctrl | '[':
					goto Retn;
				case '.':			// Stop and return to original buffer position.
					qc.flags |= QR_GoBack;
					goto Retn;
				case 'r':			// Restart, go back and start over.
					goto Restart;
				case 'u':			// Undo last and re-prompt.
					goto Undo;
				default:			// Complain and beep.
					ttbeep();
					// Fall through.
				case '?':			// Display help.
					if(mlputs(MLHome | MLTermAttr | MLFlush,text467) != Success)
	// "~uESC~U|~uq~U ~bStop here~B, ~u.~U ~bStop and go back~B, ~uu~U ~bUndo last~B, ~ur~U ~bRestart~B, ~u?~U ~bHelp~B"
						return rc.status;
				}
			}
Restart:
		// Return to original buffer position and start over.
		si.curwin->w_face.wf_point = qc.origPoint.mk_point;
		qc.last.replPoint.lnp = NULL;
		}
Retn:
	// Adjust point if needed.
	if(qc.flags & QR_GoBack) {
		si.curwin->w_face.wf_point = qc.origPoint.mk_point;
		si.curwin->w_rfrow = qc.origPoint.mk_rfrow;
		si.curwin->w_flags |= WFReframe;
		}
	else if(qrepl) {
		if(qc.flags & QR_LastWasNo) {
			(void) movech(-1);
			si.curwin->w_flags |= WFMove;
			}
		}
Rpt:
	// Report the results.
	(void) rcset(Success,RCHigh | RCTermAttr,text92,qc.numSub,qc.numSub == 1 ? "" : "s");
		// "%d substitution%s"
	if(newpos(point,&qc)) {
		Mark *mark;
		DStrFab msg;

		if(mfind(WrkMark,&mark,MkOpt_Create) != Success)
			return rc.status;
		mark->mk_point = qc.origPoint.mk_point;
		mark->mk_rfrow = qc.origPoint.mk_rfrow;

		// Append to return message if it was set successfully.
		if(rc.flags & RCMsgSet) {
			if(dopenwith(&msg,&rc.msg,SFAppend) != 0 || dputs(", ",&msg) != 0 || dputc(chcase(*text233),&msg) != 0
			 || dputf(&msg,text233 + 1,WrkMark) != 0 || dclose(&msg,sf_string) != 0)
					// "Mark ~u%c~U set to previous position"
LibFail:
				return librcset(Failure);
			}
		}

	if(si.opflags & OpScript)
		dsetbool((qrepl && (status == NotFound || qc.numSub == n)) ||
		 (!qrepl && (n == INT_MIN || qc.numSub >= n)),rval);
	return rc.status;
	}

// Free all replacement pattern heap space in given match object.
void freerpat(Match *match) {

	rmcclear(match);
	(void) free((void *) match->rpat);
	(void) free((void *) match->rmcpat);
	match->rsize = 0;
	}

// Initialize parameters for new replacement pattern, which may be null.
int newrpat(char *pat,Match *match) {
	int patlen = strlen(pat);

	// Free up arrays if too big.
	if(match->rsize > NPatMax || (match->rsize > 0 && (uint) patlen > match->rsize))
		freerpat(match);

	// Get heap space for arrays if needed.
	if(match->rsize == 0) {
		match->rsize = patlen < NPatMin ? NPatMin : patlen;
		if((match->rpat = (char *) malloc(match->rsize + 1)) == NULL ||
		 (match->rmcpat = (ReplMetaChar *) malloc((match->rsize + 1) * sizeof(ReplMetaChar))) == NULL)
			return rcset(Panic,0,text94,"newrpat");
					// "%s(): Out of memory!"
		match->rmcpat[0].mc_type = MCE_Nil;
		}

	// Save replacement pattern.
	strcpy(match->rpat,pat);
	if(rsetpat(pat,&rring) != Success)	// Add pattern to replacement ring.
		return rc.status;
	rmcclear(match);				// Clear Regexp replacement table.

	return rc.status;
	}
