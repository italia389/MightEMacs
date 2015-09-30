// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// input.c	Various user input routines for MightEMacs.
//
// MightEMacs's kernel processes two distinct forms of characters.  One of these is a standard unsigned character which is
// used in the edited text.  The other form, called an EMACS Extended Character is a two-byte value which contains both an
// ASCII value and flags for certain prefixes/events.
//
//	Bit	Usage
//	---	-----
//	0-7	Standard 8 bit ascii character
//	8	Control key flag
//	9	META prefix flag
//	10	^X prefix flag
//	11	^H prefix flag
//	12	Alterate prefix (ALT key on PCs)
//	13	Shifted flag (not needed on alpha shifted characters)
//	14	Function key flag
//	15	Mouse prefix
//
// The machine-dependent driver is responsible for returning a byte stream from the various input devices with various
// prefixes/events embedded as escape codes.  Zero is used as the value indicating an escape sequence is next.  The
// format of an escape sequence is as follows:
//
//	0		Escape indicator
//	<prefix byte>	Upper byte of extended character
//	{<col><row>}	Col, row position if the prefix byte indicated a mouse event or a menu selection in which case these
// 			form a 16 bit menu ID
//	<event code>	Value of event
//
// A ^<space> sequence (0/1/32) is generated when an actual null is being input from the control-space key under many
// unix systems.  These values are then interpreted by getkey() to construct the proper extended character sequences to
// pass to the MightEMacs kernel.

#include "os.h"
#include <sys/types.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"
#include "ebind.h"

// "Unget" a key from getkey().
void tungetc(int c) {

	kentry.chpending = c;
	kentry.ispending = true;
	}

// Get one keystroke from the pending queue or the terminal driver, resolve any keyboard macro action, and return it in *keyp as
// an extended character.  The legal prefixes here are the FKEY and CTRL.  Return status.
int getkey(int *keyp) {
	int c;		// Next input character.
	int upper;	// Upper byte of the extended sequence.

	// If a character is pending ...
	if(kentry.ispending) {

		// get it and return it.
		c = kentry.chpending;
		kentry.ispending = false;
		goto retn;
		}

	// Otherwise, if we are playing a keyboard macro back ...
	if(kmacro.km_state == KMPLAY) {

		// End of macro?
		if(kmacro.km_slotp == kmacro.km_endp) {

			// Yes.  Repeat?
			if(--kmacro.km_n <= 0) {

				// Non-positive counter.  Error if hit loop maximum.
				if(kmacro.km_n < 0 && abs(kmacro.km_n) > loopmax)
					return rcset(FAILURE,0,text112,loopmax);
						// "Maximum number of loop iterations (%d) exceeded!"
				if(kmacro.km_n == 0) {

					// Last rep.  Stop playing and update screen.
					kmacro.km_state = KMSTOP;
#if !VISMAC
					(void) update(false);
#endif
					goto fetch;
					}
				}

			// Not last rep.  Reset the macro to the begining for the next one.
			kmacro.km_slotp = kmacro.km_buf;
			}

		// Return the next key.
		*keyp = *kmacro.km_slotp++;
		return rc.status;
		}
fetch:
	// Otherwise, fetch a character from the terminal driver.
	if(TTgetc(&c) != SUCCESS)
		return rc.status;

	// If it's an escape sequence, process it.
	if(c == 0) {

		// Get the event type and code.
		if(TTgetc(&upper) != SUCCESS || TTgetc(&c) != SUCCESS)
			return rc.status;

		// If it is a function key ... map it.
		c = (upper << 8) | c;
		}

	// Normalize the control prefix.
	if(((c & 0xff) >= 0x00 && (c & 0xff) <= 0x1f) || (c & 0xff) == 0x7f)
		c = CTRL | (c ^ 0x40);

	// Record it for abortOp() function and elsewhere.
	kentry.lastread = c;

	// Save it if we need to ...
	if(kmacro.km_state == KMRECORD) {

		// Don't overrun the buffer.
		if(kmacro.km_slotp == kmacro.km_buf + NKBDM) {
			kmacro.km_endp = kmacro.km_slotp;
			kmacro.km_state = KMSTOP;
			curwp->w_flags |= WFMODE;
			(void) TTbeep();
			}
		else
			*kmacro.km_slotp++ = c;
		}

retn:
	// and finally, return the extended character.
	*keyp = c;
	return rc.status;
	}

// Get a key sequence from the keyboard and return it in *keyp.  Process all applicable prefix keys.  If kdpp is not NULL, set
// *kdpp to the getbind() return pointer.  Return status.
int getkseq(int *keyp,KeyDesc **kdpp) {
	int c;			// Fetched keystroke.
	int p;			// Prefix mask.
	KeyDesc *kdp;		// Pointer to a key entry.

	// Get initial key.
	if(getkey(&c) != SUCCESS)
		return rc.status;
	kdp = getbind(c);

	// Get another key if first one was a prefix.
	if(kdp && kdp->k_fab.p_type == PTRCMD) {
		switch((enum cfid) (kdp->k_fab.u.p_cfp - cftab)) {
			case cf_metaPrefix:
				p = META;
				goto next;
			case cf_xPrefix:
				p = XPREF;
				goto next;
			case cf_hPrefix:
				p = HPREF;
				goto next;
			case cf_cPrefix:
				p = CPREF;
next:
				if(getkey(&c) != SUCCESS)
					return rc.status;
				c = upcase[c & 0xff] | (c & ~0xff) | p;	// Force to upper case and add prefix.
				if(kdpp != NULL) {
					kdp = getbind(c);		// Get new binding if requested.
					goto retn;
					}
				break;
			default:		// Do nothing.
				break;
			}
		}

	if(kdpp != NULL) {
retn:
		*kdpp = kdp;
		}

	// Return it.
	*keyp = kentry.lastkseq = c;
	return rc.status;
	}

// Get a string from the user, given result pointer, default value, input terminator, and flags.  If the terminator is not CR
// ('\r'), CR will be echoed as "<CR>" by mlputc().  Return status dependent on aflags, as described in getarg() comments.
static int getstring(Value *rp,char *defvalp,int eolchar,uint aflags) {
	int cpos;		// Current character position in destination buffer.
	int clen;		// Bytes stored in buffer for last character entered.
	int c;			// Current input character.
	int ec;			// Current extended input character.
	bool quotef;		// Quoting next character?
	bool kill = false;	// ^K entered?
	char *kp;		// Pointer into key_name.
	char key_name[16];	// Name of a quoted key.
	char inpbuf[NTERMINP + 1];

	clen = cpos = 0;
	quotef = false;

	// Get a string from the user.
	for(;;) {
		if(getkey(&ec) != SUCCESS)
			return rc.status;

		// If the line terminator or ^K was entered, wrap it up.
		if(ec == eolchar && !quotef) {
wrapup:
			if(cpos > 0 || kill || defvalp == NULL)
				inpbuf[cpos] = '\0';

			// Clear the message line.
			mlerase(MLFORCE);

			// If nothing was entered, return default value or nil and current status (SUCCESS).
			if(cpos == 0) {
				if(kill) {
					if(aflags & ARG_NOTNULL)
						goto nilrtn;
					vnull(rp);
					}
				else if(defvalp == NULL)
nilrtn:
					(void) vnilmm(rp);
				else if(vsetstr(defvalp,rp) != 0)
					(void) vrcset();
				return rc.status;
				}
			break;
			}

		// If not in quote mode, check key that was entered.
		if(!quotef) {

			// Abort the input?
			if(ec == ckeys.abort)
				return abortinp();

			// If it is a function key, blow it off.
			if(ec & FKEY)
				continue;

			// Rubout/erase.
			if(ec == (CTRL | '?')) {
				if(cpos != 0) {
					if(mlputc('\b',vz_raw) != SUCCESS || TTflush() != SUCCESS)
						return rc.status;
					--cpos;
					}
				continue;
				}

			// ^K: return null string.
			if(ec == (CTRL | 'K')) {
				cpos = 0;
				kill = true;
				goto wrapup;
				}

			// ^U: kill buffer and continue.
			if(ec == (CTRL | 'U')) {
				while(cpos > 0) {
					if(mlputc('\b',vz_raw) != SUCCESS)
						return rc.status;
					--cpos;
					}
				if(TTflush() != SUCCESS)
					return rc.status;
				continue;
				}

			// Quoting next character?
			if(ec == ckeys.quote) {
				quotef = true;
				continue;
				}
			}

		// Non-special or quoted character.
		quotef = false;

		// If it is a (quoted) function key, insert it's name.
		if(ec & (FKEY | SHFT)) {
			ectos(ec,key_name,true);
			kp = key_name;
			while(*kp) {
				if(cpos < NTERMINP) {
					if((modetab[MDR_GLOBAL].flags & MDKECHO) && mlputc(*kp,vz_raw) != SUCCESS)
						return rc.status;
					inpbuf[cpos++] = *kp++;
					}
				}
			if(TTflush() != SUCCESS)
				return rc.status;
			continue;
			}

		// Insert the character into the string!
		if(cpos < NTERMINP) {
			inpbuf[cpos++] = c = ectoc(ec);
			if((modetab[MDR_GLOBAL].flags & MDKECHO) && (mlputc(c,vz_show) != SUCCESS || TTflush() != SUCCESS))
				return rc.status;
			}
		}

	// Save result and return.
	if(vsetstr(inpbuf,rp) != 0)
		(void) vrcset();

	return rc.status;
	}

// Ask a yes or no question on the message line and set *resultp to the answer (true or false).  Used any time a confirmation is
// required.  Return status, including USERABORT, if user pressed the abort key.
int mlyesno(char *promptp,bool *resultp) {
	Value *vp;
	StrList prompt;

	// Build prompt.
	if(vnew(&vp,false) != 0 || vopen(&prompt,NULL,false) != 0 ||
	 vputs(promptp,&prompt) != 0 || vputs(text162,&prompt) != 0 || vclose(&prompt) != 0)
						// " (y/n)?"
		return vrcset();

	// Prompt user, get the response, and check it.
	for(;;) {
		if(termarg(vp,prompt.sl_vp->v_strp,"n",CTRL | 'M',ARG_ONEKEY) != SUCCESS)
			return rc.status;
		if(*vp->v_strp ==
#if FRENCH
		 'o') {
#elif SPANISH
		 's') {
#else
		 'y') {
#endif
			*resultp = true;
			break;
			}
		if(*vp->v_strp == 'n') {
			*resultp = false;
			break;
			}
		if(TTbeep() != SUCCESS || mlerase(0) != SUCCESS)
			break;
		}
	return rc.status;
	}

// Build a prompt in destp (of size NTERMINP + 1) and display it.  Return status.
static int buildprompt(char *destp,char *promptp,char *defvalp,int terminator) {
	bool addSpace;
	bool literal = false;
	bool addColon = false;
	char *destp0,*destpz,*srcp;

	destpz = (destp0 = destp) + NTERMINP + 1;
	srcp = strchr(promptp,'\0');

	if(isletter(*promptp))
		addColon = true;
	else if(*promptp == '\'' || *promptp == '"') {
		++promptp;
		literal = true;
		while(srcp[-1] == ' ')
			--srcp;
		}

	// Save initial prompt.
	destp = stplcpy(destp,promptp,srcp - promptp + 1);
	addSpace = *promptp != '\0' && srcp[-1] != ' ';
	if(defvalp != NULL) {
		if(addSpace) {
			if(chkcpy(&destp," ",destpz - destp) != SUCCESS)
				return rc.status;
			addSpace = false;
			}
		if(chkcpy(&destp,"[",destpz - destp) != SUCCESS || chkcpy(&destp,defvalp,destpz - destp) != SUCCESS ||
		 chkcpy(&destp,"]",destpz - destp) != SUCCESS)
			return rc.status;
		addColon = true;
		}
	if(terminator != (CTRL | 'M')) {
		char wkbuf[2];

		if(addSpace && chkcpy(&destp," ",destpz - destp) != SUCCESS)
			return rc.status;
		wkbuf[0] = ectoc(terminator);
		wkbuf[1] = '\0';
		if(chkcpy(&destp,wkbuf,destpz - destp) != SUCCESS)
			return rc.status;
		addColon = true;
		}
	if(*srcp == ' ') {
		if(chkcpy(&destp,srcp,destpz - destp) != SUCCESS)
			return rc.status;
		}
	else if(addColon && !literal && destp[-1] != ' ' && chkcpy(&destp,": ",destpz - destp) != SUCCESS)
		return rc.status;
	return mlputs(MLHOME | MLFORCE,destp0,vz_show);
	}

// Get a terminal (interactive) argument, given result pointer, prompt, default value, input terminator, and argument flags.  If
// the prompt is not NULL, don't move the cursor; otherwise, home the cursor and write the prompt, default value (if not NULL),
// terminator (if not CR) and a colon to the message line (with exceptions).  Read back a response (and if ARG_ONEKEY bit set in
// aflags, get just one key sequence).  If nothing is entered and a default value is given, copy it to the destination buffer
// and return rc.status (SUCCESS); otherwise, return status dependent on aflags, as described in getarg() comments.
int termarg(Value *rp,char *promptp,char *defvalp,int terminator,uint aflags) {

	// If not evaluating, bail out.
	if((opflags & OPEVAL) == 0)
		return rc.status;

	// Build and display prompt.  If the prompt string begins with a letter or a default value is given, append
	// ": "; otherwise, if the prompt string begins with a single (') or double (") quote character, strip if off and use
	// the remainder as is; however, move any trailing white space to the very end.
	if(promptp != NULL) {
		char wkbuf[NTERMINP + 1];

		if(buildprompt(wkbuf,promptp,defvalp,terminator) != SUCCESS)
			return rc.status;
		}

	// One key sequence?
	if(aflags & ARG_ONEKEY) {
		int ec;

		// Yes, call getkey().
		if(getkey(&ec) != SUCCESS)
			return rc.status;

		// Clear the message line.
		mlerase(MLFORCE);

		// Terminator?
		if(ec == terminator) {

			// Yes, return default value or nil.
			if(defvalp == NULL)
				goto nilrtn;
			return vsetstr(defvalp,rp);
			}

		// No, ^K?
		if(ec == (CTRL | 'K')) {

			// Yes, return nil or null string and current status.
			if(aflags & ARG_NOTNULL)
nilrtn:
				(void) vnilmm(rp);
			else
				vnull(rp);
			return rc.status;
			}

		// No, user abort?
		if(ec == ckeys.abort) {

			// Yes, ignore buffer and return USERABORT.
			return abortinp();
			}

		// No, save key and return.
		vsetchr(ectoc(ec),rp);
		}

	// No, get multiple characters and evaluate result if requested.
	else {
		Value *vp;

		if((aflags & ARG_EVAL) == 0)
			vp = rp;
		else if(vnew(&vp,false) != 0)
			return vrcset();

		if(getstring(vp,defvalp,terminator,aflags) == SUCCESS && (aflags & ARG_EVAL))
			(void) doestmt(rp,vp->v_strp,TKC_COMMENT,NULL);
		}

	return rc.status;
	}

// Main routine for getting the next argument from the terminal or a command line, given result pointer, prompt, default value,
// input terminator, and argument flags.  If in script mode, call macarg(); otherwise, call termarg() (and if ARG_ONEKEY is set
// in aflags, get just one key).  Return status and value per the following table:
//
//					--------------- Argument Flag ----------------
//	Input Result			ARGREQ_NOTNULL	ARGREQ_NULLOK	ARG_OPTIONAL	Return Value
//	============================	==============	==============	==============	============
//	Interactive: Terminator only	SUCCESS		SUCCESS		SUCCESS		nil
//	Interactive: ^K			SUCCESS		SUCCESS		SUCCESS		nil if ARGREQ_NOTNULL; otherwise,
//											null string
//	Interactive: Char(s) entered	SUCCESS		SUCCESS		SUCCESS		input string
//
//	Cmdline: No arg			FAILURE		FAILURE		N/A		(undefined)
//	Cmdline: Null string		FAILURE		SUCCESS		N/A		null string
//	Cmdline: Non-null string	SUCCESS		SUCCESS		N/A		argument string
//
// Always return SUCCESS in interactive mode so that an error is not displayed on the message line when the user enters just the
// terminator (usually RETURN) at a command prompt, effectively cancelling the operation.  Invalid input can be determined by
// checking for a nil return value.
int getarg(Value *rp,char *promptp,char *defvalp,int terminator,uint aflags) {

	return (opflags & OPSCRIPT) == 0 ? termarg(rp,promptp,defvalp,terminator,aflags) : macarg(rp,aflags);
	}

// Shortcut routine for getting the first argument via getarg(), given result pointer and prompt.  A non-null argument is
// required.  If terminal mode, the argument is delimited by a CR with no default value.  Return status.
int getargCR(Value *rp,char *promptp) {

	return getarg(rp,promptp,NULL,CTRL | 'M',ARG_NOTNULL | ARG_FIRST);
	}

// Convert extended character to character.  Collapse the CTRL and FKEY flags back into an ASCII code.
int ectoc(int c) {

	if(c == (CTRL | ' '))
		return 0;		// Null char.
	if(c & CTRL)
		c ^= (CTRL | 0x40);	// Actual control char.
	if(c & (FKEY | SHFT))
		c &= 0xff;		// Remove FKEY and/or SHFT bits.
	return c;
	}

// Convert character to extended character.  Change control character to CTRL prefix (upper bit).
int ctoec(int c) {

	if(c == 0)
		c = CTRL | ' ';
	else if(c < ' ' || c == 0x7f)
		c = CTRL | (c ^ 0x40);
	return c;
	}

// Attempt a completion on a buffer name, given pointer to mode line buffer (containing partial name to complete) and pointer to
// variable containing next character position in the buffer.  Return rc.status (SUCCESS) if unique name matched; otherwise,
// return NOTFOUND (bypassing rcset()).
static int comp_buffer(char *name,uint *cposp) {
	Buffer *bufp;		// Trial buffer to complete.
	Buffer *bmatchp;	// Last buffer that matched string.
	bool comflag;		// Was there a completion at all?

	// Start attempting completions, one character at a time, but not more than the maximum buffer size.
	comflag = false;
	while(*cposp < NBUFN) {

		// Start at the first buffer and scan the list.
		bmatchp = NULL;
		bufp = bheadp;
		do {
			// Is this a match?
			if(*cposp > 0 && strncmp(name,bufp->b_bname,*cposp) != 0)
				goto nextBuf;

			// A match.  If this is the first match, simply record it.
			if(bmatchp == NULL) {
				bmatchp = bufp;
				name[*cposp] = bufp->b_bname[*cposp];	// Save next char.
				}
			else if(name[*cposp] != bufp->b_bname[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto fail;
nextBuf:
			// On to the next buffer.
			bufp = bufp->b_nextp;
			} while(bufp != NULL);

		// With no match, we are done.
		if(bmatchp == NULL) {

			// Beep if we never matched.
			if(!comflag)
				(void) TTbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(name[*cposp] == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		if(mlputc(name[(*cposp)++],vz_raw) != SUCCESS || TTflush() != SUCCESS)
			return rc.status;
		}
fail:
	// Match not found.
	return NOTFOUND;
	}

// Attempt a completion on a command, alias, or macro name, given pointer to mode line buffer (containing partial name to
// complete) and pointer to variable containing next character position in the buffer.  Return rc.status (SUCCESS) if unique
// name matched; otherwise, return NOTFOUND (bypassing rcset()).
static int comp_fab(char *name,uint *cposp,uint selector) {
	CAMRec *crp;		// CAM record pointer.
	CAMRec *ematchp;	// Last entry that matched string.
	bool comflag;		// Was there a completion at all?

	// Start attempting completions, one character at a time.
	comflag = false;
	for(;;) {

		// Start at the first CAM entry and scan the list.
		ematchp = NULL;
		crp = crheadp;
		do {
			// Is this a match?
			if((crp->cr_type & selector) == 0 || (*cposp > 0 && strncmp(name,crp->cr_name,*cposp) != 0))
				goto nextentry;

			// A match.  If this is the first match, simply record it.
			if(ematchp == NULL) {
				ematchp = crp;
				name[*cposp] = crp->cr_name[*cposp];	// Save next char.
				}
			else if(name[*cposp] != crp->cr_name[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto fail;
nextentry:
			// On to the next entry.
			crp = crp->cr_nextp;
			} while(crp != NULL);

		// With no match, we are done.
		if(ematchp == NULL) {

			// Beep if we never matched.
			if(!comflag)
				(void) TTbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(name[*cposp] == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		if(mlputc(name[(*cposp)++],vz_raw) != SUCCESS || TTflush() != SUCCESS)
			return rc.status;
		}
fail:
	// Match not found.
	return NOTFOUND;
	}

// Attempt a completion on a filename, given pointer to mode line buffer (containing partial name to complete) and pointer to
// variable containing next character position in the buffer.  Return rc.status (SUCCESS) if unique name matched; otherwise,
// return NOTFOUND (bypassing rcset()).
static int comp_file(char *name,uint *cposp) {
	char *fname;			// Trial file to complete.
	uint i;				// Index into strings to compare.
	uint matches;			// Number of matches for name.
	uint longestlen;		// Length of longest match (always > *cposp).
	char longestmatch[MaxPathname + 1];// Temp buffer for longest match.

	// Open the directory.
	name[*cposp] = '\0';
	if(eopendir(name,&fname) != SUCCESS)
		goto fail;

	// Start at the first file and scan the list.
	matches = 0;
	while(ereaddir() == SUCCESS) {

		// Is this a match?
		if(*cposp == 0 || strncmp(name,fname,*cposp) == 0) {

			// Count the number of matches.
			++matches;

			// If this is the first match, simply record it.
			if(matches == 1) {
				strcpy(longestmatch,fname);
				longestlen = strlen(longestmatch);
				}
			else {
				if(longestmatch[*cposp] != fname[*cposp])

					// A difference, stop here.  Can't auto-extend message line any further.
					goto fail;

				// Update longestmatch.
				for(i = (*cposp) + 1; i < longestlen; ++i)
					if(longestmatch[i] != fname[i]) {
						longestlen = i;
						longestmatch[longestlen] = '\0';
						}
				}
			}
		}

	// Success?
	if(rc.status == SUCCESS) {

		// Beep if we never matched.
		if(matches == 0)
			(void) TTbeep();
		else {
			// The longestmatch buffer contains the longest match, so copy and display it.
			while(*cposp < NPATHINP && *cposp < longestlen) {
				name[*cposp] = longestmatch[*cposp];
				if(mlputc(name[*cposp],vz_raw) != SUCCESS)
					return rc.status;
				++*cposp;
				}

			name[*cposp] = '\0';
			if(TTflush() != SUCCESS)
				return rc.status;

			// If only one file matched, it was completed unless the last character was a '/', indicating a
			// directory.  In that case we do NOT want to signal a complete match.
			if(matches == 1 && name[*cposp - 1] != '/')
				return rc.status;
			}
		}
fail:
	// Match not found.
	return NOTFOUND;
	}

// Make a completion list based on a partial buffer name, given pointer to input line buffer (containing partial name to
// complete), pointer to variable containing next character position, and indirect file buffer pointer.  Return status.
static int clist_buffer(char *name,uint cpos,Buffer **bufpp) {
	Buffer *bufp;		// Trial buffer to complete.

	// Get a buffer for the completion list.
	if(sysbuf(text125,bufpp) != SUCCESS)
			// "CompletionList"
		return rc.status;

	// Start at the first buffer and scan the list.
	bufp = bheadp;
	do {

		// Is this a match?
		if(cpos == 0 || strncmp(name,bufp->b_bname,cpos) == 0)
			if(bappend(*bufpp,bufp->b_bname) != SUCCESS)
				return rc.status;

		// On to the next buffer.
		} while((bufp = bufp->b_nextp) != NULL);

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial name, given pointer to input line buffer (containing partial name to complete),
// pointer to variable containing next character position, indirect file buffer pointer, and selector mask.  Return status.
static int clist_fab(char *name,uint cpos,Buffer **bufpp,uint selector) {
	CAMRec *crp;		// Trial name to complete.

	// Get a buffer for the completion list.
	if(sysbuf(text125,bufpp) != SUCCESS)
			// "CompletionList"
		return rc.status;

	// Start at the first entry and scan the list.
	crp = crheadp;
	do {
		// Add name to the buffer if it's a match ...
		if((crp->cr_type & selector) && (cpos == 0 || strncmp(name,crp->cr_name,cpos) == 0))
			if(bappend(*bufpp,crp->cr_name) != SUCCESS)
				return rc.status;

		// and on to the next entry.
		} while((crp = crp->cr_nextp) != NULL);

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial filename, given pointer to input line buffer (containing partial name to complete),
// pointer to variable containing next character position, and indirect file buffer pointer.  Return status.
static int clist_file(char *name,uint cpos,Buffer **bufpp) {
	char *fname;		// Trial file to complete.

	// Get a buffer for the completion list.
	if(sysbuf(text125,bufpp) != SUCCESS)
			// "CompletionList"
		return rc.status;

	// Open the directory.
	name[cpos] = '\0';
	if(eopendir(name,&fname) != SUCCESS)
		return rc.status;

	// Start at the first file and scan the list.
	while(ereaddir() == SUCCESS) {

		// Is this a match?
		if(cpos == 0 || strncmp(name,fname,cpos) == 0)
			if(bappend(*bufpp,fname) != SUCCESS)
				return rc.status;
		}

	// Error?
	if(rc.status != SUCCESS)
		return rc.status;

	return bpop(*bufpp,false,false);
	}

// Complete a terminal entry, given result pointer, prompt, default value (which may be NULL), cflags (which specifies type of
// completion), maximum input length (<= NTERMINP), and (i), selector mask for CMPL_CAM calls; or (ii) macarg() flag(s) for
// CMPL_FILENAME calls.  Return status.
int complete(Value *rp,char *promptp,char *defvalp,uint cflags,uint maxlen,uint aflags) {

	// If we are executing a command line, get the next arg and return it.  Pass aflags if doing a filename completion.
	if(opflags & OPSCRIPT)
		(void) macarg(rp,ARG_FIRST | ARG_STR | ((cflags & CMPL_FILENAME) ? aflags : 0));
	else {
		Buffer *bufp;
		size_t prlen = strlen(promptp) + (defvalp == NULL ? 0 : strlen(defvalp)) + 16;
		char prbuf[prlen + NTERMINP + 1],*inpbufp = prbuf + prlen;	// Prompt and input buffers.
		int status;
		int ec;				// Current input character.
		uint cpos;			// Current column of input string echo, beginning at zero.
		char *hdirp;			// Pointer to home directory string.
		char *strp;			// String pointer.
		struct passwd *pwd;		// Password structure.

		// Display prompt.
		if(buildprompt(prbuf,promptp,defvalp,CTRL | 'M') != SUCCESS)
			return rc.status;

		// Start at the beginning of the input buffer.
		cpos = 0;

		// Get a string from the keyboard.
		for(;;) {
			// Get a keystroke and decode it.
			if(getkey(&ec) != SUCCESS)
				return rc.status;

			// If it is a function key, ignore it.
			if(ec & FKEY)
				continue;

			// If RETURN, finish up (we're done).
			if(ec == (CTRL | 'M')) {			// Return.
				if(cpos == 0) {
					if(defvalp == NULL) {
						if(vnilmm(rp) != SUCCESS || mlerase(0) != SUCCESS)
							return rc.status;
						}
					else if(vsetstr(defvalp,rp) != 0)
						(void) vrcset();
					return rc.status;
					}
				inpbufp[cpos] = '\0';
				break;
				}
			if(ec == (CTRL | '[')) {			// Escape, cancel.
				if(vnilmm(rp) == SUCCESS)
					(void) mlerase(0);
				return rc.status;
				}
			if(ec == ckeys.abort)				// ^G, bell, abort.
				return abortinp();
			if(ec == (CTRL | '?')) {			// Rubout/erase.
				if(cpos > 0) {
					if(mlputc('\b',vz_raw) != SUCCESS || TTflush() != SUCCESS)
						return rc.status;
					--cpos;
					}
				}
			else if(ec == (CTRL | 'U')) {			// ^U, kill.
				while(cpos > 0) {
					if(mlputc('\b',vz_raw) != SUCCESS)
						return rc.status;
					--cpos;
					}
				if(TTflush() != SUCCESS)
					return rc.status;
				}
			else if(ec == (CTRL | 'I')) {			// Tab.

				// Attempt a completion.
				switch(cflags & CMPL_MASK) {
					case CMPL_BUFFER:
						status = comp_buffer(inpbufp,&cpos);
						break;
					case CMPL_CAM:
						status = comp_fab(inpbufp,&cpos,aflags);
						break;
					case CMPL_FILENAME:
						status = comp_file(inpbufp,&cpos);
					}
				if(TTflush() != SUCCESS)
					return rc.status;
				if(status == SUCCESS && !(cflags & CMPL_NOAUTO))	// Success!
					break;
				goto clist;
				}
			else if(ec == (CTRL | 'K')) {			// ^K, return null string.
				inpbufp[0] = '\0';
				break;
				}
			else if(cpos > 0 && ec == '/' && (cflags & CMPL_FILENAME) && inpbufp[0] == '~' &&
			 (hdirp = getenv("HOME")) != NULL) {
				Value *unamep;

				// Save the username, if any.
				inpbufp[cpos] = '\0';
				if(vnewstr(&unamep,inpbufp + 1) != 0)
					return vrcset();

				// Erase the chars on-screen.
				while(cpos > 0) {
					if(mlputc('\b',vz_raw) != SUCCESS)
						return rc.status;
					--cpos;
					}

				// Lookup someone else's home directory.
				if(!visnull(unamep)) {
					pwd = getpwnam(unamep->v_strp);
					if(pwd != NULL && strlen(strp = pwd->pw_dir) < maxlen) {
						while(*strp != '\0') {
							if(mlputc(*strp,vz_show) != SUCCESS)
								return rc.status;
							inpbufp[cpos++] = *strp++;
							}
						}
					}
				if(cpos == 0) {
					if(strlen(strp = hdirp) < maxlen) {

						// Output the HOME directory.
						while(*strp != '\0') {
							if(mlputc(*strp,vz_show) != SUCCESS)
								return rc.status;
							inpbufp[cpos++] = *strp++;
							}

						// Was a username entered (presumably someone else's home directory)?
						if(!visnull(unamep)) {

							// Back up to the last directory separator.
							while((cpos > 0) && (inpbufp[cpos - 1] != '/')) {
								if(mlputc('\b',vz_raw) != SUCCESS)
									return rc.status;
								--cpos;
								}

							// and add the user's name.
							strp = unamep->v_strp;
							while(*strp != '\0') {
								if(mlputc(*strp,vz_show) != SUCCESS)
									return rc.status;
								inpbufp[cpos++] = *strp++;
								}
							}
						}
					}

				// and the last directory separator.
				if(cpos > 0 && inpbufp[cpos - 1] != '/') {
					if(mlputc('/',vz_raw) != SUCCESS)
						return rc.status;
					inpbufp[cpos++] = '/';
					}
				if(TTflush() != SUCCESS)
					return rc.status;
				}
			else if(cpos > 1 && ec == '/' && (cflags & CMPL_FILENAME) && inpbufp[0] == '$') {
				Value *evarp;

				// Expand an environmental variable reference.

				// Save the variable name.
				inpbufp[cpos] = '\0';
				if(vnewstr(&evarp,inpbufp + 1) != 0)
					return vrcset();

				// Erase the chars on-screen.
				while(cpos > 0) {
					if(mlputc('\b',vz_raw) != SUCCESS)
						return rc.status;
					--cpos;
					}

				// Get variable and display it.
				if((strp = getenv(evarp->v_strp)) != NULL && strlen(strp) < maxlen) {
					while(*strp != '\0') {
						if(mlputc(*strp,vz_show) != SUCCESS)
							return rc.status;
						inpbufp[cpos++] = *strp++;
						}
					}

				// and lastly, the directory separator.
				if(cpos > 0 && inpbufp[cpos - 1] != '/') {
					if(mlputc('/',vz_raw) != SUCCESS)
						return rc.status;
					inpbufp[cpos++] = '/';
					}
				if(TTflush() != SUCCESS)
					return rc.status;
				}
			else if(ec == '?') {
clist:
				// Make a completion list and pop it (without key input).
				switch(cflags & CMPL_MASK) {
					case CMPL_BUFFER:
						status = clist_buffer(inpbufp,cpos,&bufp);
						break;
					case CMPL_CAM:
						status = clist_fab(inpbufp,cpos,&bufp,aflags);
						break;
					case CMPL_FILENAME:
						status = clist_file(inpbufp,cpos,&bufp);
					}

				// Check status, delete the pop-up buffer, and reprompt the user.
				if(status != SUCCESS || bdelete(bufp,0) != SUCCESS)
					return rc.status;
				inpbufp[cpos] = '\0';
				(void) mlprintf(MLHOME | MLFORCE,"%s%s",prbuf,inpbufp);
				if(rc.status != SUCCESS)
					return rc.status;

				// Pause so that user can see the window.
				if(getkey(&ec) != SUCCESS)
					return rc.status;

				// "Unget" the key for reprocessing if it's not an escape character.
				if(ec != (CTRL | '['))
					tungetc(ec);

				// Update the screen, restore cursor position on message line, and continue.
				if(update(true) != SUCCESS || mlrestore() != SUCCESS)
					return rc.status;
				}
			else if(cpos < maxlen && ec >= ' ' && ec < 0x7f) {
				inpbufp[cpos++] = ec;
				if(mlputc(ec,vz_show) != SUCCESS || TTflush() != SUCCESS)
					return rc.status;
				}
			else {
				if(TTbeep() != SUCCESS || TTflush() != SUCCESS)
					return rc.status;
				}
			}

		// Entry completed successfully.
		if(vsetstr(inpbufp,rp) != 0)
			return vrcset();
		}

	return rc.status;
	}

// Check if given string is a command, other function, alias, buffer, or macro, according to selector masks.  If found, set
// "fabp" (if not NULL) to result and return true; otherwise, return false.
bool fabsearch(char *strp,FABPtr *fabp,uint selector) {
	FABPtr fab;

	// Figure out what the string is.
	if((selector & (PTRCMD | PTRFUNC)) &&
	 (fab.u.p_cfp = ffind(strp)) != NULL) {				// Is it a function?
		ushort foundtype = (fab.u.p_cfp->cf_flags & CFFUNC) ? PTRFUNC : PTRCMD;
		if(!(selector & foundtype))				// Yep, correct type?
			return false;					// No.
		fab.p_type = foundtype;					// Yes, set it.
		}
	else if((selector & PTRALIAS) &&
	 afind(strp,OPQUERY,NULL,&fab.u.p_aliasp))			// Not a function ... is it an alias?
		fab.p_type = PTRALIAS;					// Yep.
	else if((selector & PTRBUF) &&					// No, is it a buffer?
	 (fab.u.p_bufp = bsrch(strp,NULL)) != NULL)			// Yep.
		fab.p_type = PTRBUF;
	else if(selector & PTRMACRO) {					// No, is it a macro?
		char mac[NBUFN + 1];

		sprintf(mac,MACFORMAT,NBUFN - 1,strp);
		if((fab.u.p_bufp = bsrch(mac,NULL)) != NULL)		// Yep.
			fab.p_type = PTRMACRO;
		else
			return false;					// No, it's a bust.
		}
	else
		return false;

	// Found it.
	if(fabp != NULL)
		*fabp = fab;
	return true;
	}

// Get a completion from the user for a command, alias, or macro name, given prompt, selector mask, result pointer, and "not
// found" error message.
int getcam(char *promptp,uint selector,FABPtr *fabp,char *emsg) {
	Value *vp;

	if(vnew(&vp,false) != 0)
		return vrcset();
	if(complete(vp,promptp,NULL,CMPL_CAM,NTERMINP,selector) != SUCCESS)
		return rc.status;
	if((opflags & OPSCRIPT) || !vistfn(vp,VNIL))

		// Convert the string to a pointer.
		return fabsearch(vp->v_strp,fabp,selector) ? rc.status : rcset(FAILURE,0,emsg,vp->v_strp);

	fabp->p_type = PTRNUL;
	fabp->u.p_voidp = NULL;

	return rc.status;
	}

// Get a completion from the user for a buffer name, given result pointer, prompt, default value (which may be NULL), operation
// flag, result pointer, and "buffer was created" pointer (which also may be NULL).  "createdp" is passed to bfind().
int getcbn(Value *rp,char *promptp,char *defname,uint op,Buffer **bufpp,bool *createdp) {
	int status;
	char pbuf[32];

	sprintf(pbuf,"%s %s",promptp,text83);
				// "buffer"
	if(complete(rp,pbuf,defname,CMPL_BUFFER,NBUFN,0) != SUCCESS)
		return rc.status;
	if(!vistfn(rp,VNIL)) {
		status = bfind(rp->v_strp,op == OPCREATE ? CRBCREATE : CRBQUERY,0,bufpp,createdp);
		if(op == OPCREATE || status)
			return rc.status;

		// Buffer not found.
		if(op == OPQUERY && !(opflags & OPSCRIPT)) {
			mlerase(0);
			return NOTFOUND;
			}
		}
	else if(!(opflags & OPSCRIPT)) {
		*bufpp = NULL;
		return rc.status;
		}

	// Return error.
	return rcset(FAILURE,0,text118,rp->v_strp);
		// "No such buffer '%s'"
	}
