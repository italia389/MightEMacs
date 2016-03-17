// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// input.c	Various user input routines for MightEMacs.
//
// MightEMacs's kernel processes two distinct forms of characters.  One of these is a standard unsigned character which is
// used in the edit buffer.  The other form, called an Extended Key is a two-byte value which contains both an ASCII value
// and flags for certain prefixes/events:
//
//	Bit	Usage
//	---	-----
//	0-7	Standard 8-bit ASCII character
//	8-15	Flags for control, meta, prefix 1-3, shift, and function keys.
//
// The machine-dependent driver is responsible for returning a byte stream from the various input devices with various
// prefixes/events embedded as escape codes.  Zero is used as the value indicating an escape sequence is next.  The
// format of an escape sequence is as follows:
//
//	0		Escape indicator
//	<prefix byte>	Upper byte of extended key
//	{<col><row>}	Col, row position if the prefix byte indicated a mouse event or a menu selection in which case these
// 			form a 16 bit menu ID
//	<event code>	Value of event
//
// A ^<space> sequence (0/1/32) is generated when an actual null is being input from the control-space key under many
// Unix systems.  These values are then interpreted by getkey() to construct the proper extended key sequences to
// pass to the MightEMacs kernel.

#include "os.h"
#include <sys/types.h>
#include <pwd.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "ecmd.h"
#include "edata.h"
#include "ebind.h"
#include "evar.h"

// "Unget" a key from getkey().
void tungetc(uint ek) {

	kentry.chpending = ek;
	kentry.ispending = true;
	}

// Get one keystroke from the pending queue or the terminal driver, resolve any keyboard macro action, and return it in *keyp as
// an extended key.  The legal prefixes here are the FKEY and CTRL.  Return status.
int getkey(uint *keyp) {
	uint ek;	// Next input character.
	uint upper;	// Upper byte of the extended sequence.

	// If a character is pending...
	if(kentry.ispending) {

		// get it and return it.
		ek = kentry.chpending;
		kentry.ispending = false;
		goto retn;
		}

	// Otherwise, if we are playing a keyboard macro back...
	if(kmacro.km_state == KMPLAY) {

		// End of macro?
		if(kmacro.km_slotp == kmacro.km_endp) {

			// Yes.  Repeat?
			if(--kmacro.km_n <= 0) {

				// Non-positive counter.  Error if hit loop maximum.
				if(kmacro.km_n < 0 && maxloop > 0 && abs(kmacro.km_n) > maxloop)
					return rcset(FAILURE,0,text112,maxloop);
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
	if(TTgetc(&ek) != SUCCESS)
		return rc.status;

	// If it's an escape sequence, process it.
	if(ek == 0) {

		// Get the event type and code.
		if(TTgetc(&upper) != SUCCESS || TTgetc(&ek) != SUCCESS)
			return rc.status;

		// If it is a function key ... map it.
		ek = (upper << 8) | ek;
		}

	// Normalize the control prefix.
	if((ek & 0xff) <= 0x1f || (ek & 0xff) == 0x7f)
		ek = CTRL | (ek ^ 0x40);

	// Record it for abortOp() function and elsewhere.
	kentry.lastread = ek;

	// Save it if we need to...
	if(kmacro.km_state == KMRECORD) {

		// Don't overrun the buffer.
		if(kmacro.km_slotp == kmacro.km_buf + kmacro.km_size)
			if(growKeyMacro() != SUCCESS)
				return rc.status;
		*kmacro.km_slotp++ = ek;
		}

retn:
	// and finally, return the extended key.
	*keyp = ek;
	return rc.status;
	}

// Get a key sequence from the keyboard and return it in *keyp.  Process all applicable prefix keys.  If kdpp is not NULL, set
// *kdpp to the getbind() return pointer.  Return status.
int getkseq(uint *keyp,KeyDesc **kdpp) {
	uint ek;		// Fetched keystroke.
	uint p;			// Prefix mask.
	KeyDesc *kdp;		// Pointer to a key entry.

	// Get initial key.
	if(getkey(&ek) != SUCCESS)
		return rc.status;
	kdp = getbind(ek);

	// Get another key if first one was a prefix.
	if(kdp != NULL && kdp->k_cfab.p_type == PTRPSEUDO) {
		switch((enum cfid) (kdp->k_cfab.u.p_cfp - cftab)) {
			case cf_metaPrefix:
				p = META;
				goto next;
			case cf_prefix1:
				p = PREF1;
				goto next;
			case cf_prefix2:
				p = PREF2;
				goto next;
			case cf_prefix3:
				p = PREF3;
next:
				if(getkey(&ek) != SUCCESS)
					return rc.status;
				ek = upcase[ek & 0xff] | (ek & ~0xff) | p;	// Force to upper case and add prefix.
				if(kdpp != NULL) {
					kdp = getbind(ek);			// Get new binding if requested.
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
	*keyp = kentry.lastkseq = ek;
	return rc.status;
	}

// Build a prompt, shrink it if necessary to fit in the current terminal width, save the result in destp buffer (>= current
// terminal width), and display it.  If the prompt string begins with a letter or a default value is given, append ": ";
// otherwise, if the prompt string begins with a single (') or double (") quote character, strip if off and use the remainder as
// is; however, move any trailing white space to the very end.  Return status.
static int buildprompt(char *destp,char *prmtp,char *defvalp,uint terminator) {
	bool addSpace;
	bool literal = false;
	bool addColon = false;
	char *srcp,*prmtpz;
	StrList prompt;

	if(vopen(&prompt,NULL,false) != 0)
		return vrcset();
	srcp = strchr(prmtp,'\0');

	if(isletter(*prmtp))
		addColon = true;
	else if(*prmtp == '\'' || *prmtp == '"') {
		++prmtp;
		literal = true;
		while(srcp[-1] == ' ')
			--srcp;
		}

	// Save initial prompt.
	if(vputfs(prmtp,srcp - prmtp,&prompt) != 0)
		return vrcset();
	addSpace = *prmtp != '\0' && srcp[-1] != ' ';
	if(defvalp != NULL) {
		if(addSpace) {
			if(vputc(' ',&prompt) != 0)
				return vrcset();
			addSpace = false;
			}
		if(vputc('[',&prompt) != 0 || vputs(defvalp,&prompt) != 0 || vputc(']',&prompt) != 0)
			return vrcset();
		addColon = true;
		}
	if(terminator != (CTRL | 'M')) {
		if(addSpace && vputc(' ',&prompt) != 0)
			return vrcset();
		if(vputc(ektoc(terminator),&prompt) != 0)
			return vrcset();
		addColon = true;
		}
	if(*srcp == ' ' && vputs(srcp,&prompt) != 0)
		return vrcset();

	// Add two characters to make room for ": " (if needed) and close the string list.
	if(vputs("xx",&prompt) != 0 || vclose(&prompt) != 0)
		return vrcset();
	prmtpz = strchr(prmtp = prompt.sl_vp->v_strp,'\0');
	prmtpz[-2] = '\0';

	// Add ": " if needed.
	if(*srcp != ' ' && addColon && !literal && prmtpz[-3] != ' ')
		strcpy(prmtpz - 2,": ");

	// Make sure prompt fits on the screen.
	strfit(destp,96 * term.t_ncol / 100,prmtp,0);
	return mlputs(MLHOME | MLFORCE | MLTRACK,destp);
	}

// Attempt a completion on a buffer name, given pointer to message line buffer (containing partial name to complete) and pointer
// to variable containing next character position in the buffer.  Return rc.status (SUCCESS) if unique name matched; otherwise,
// return NOTFOUND (bypassing rcset()).
static int comp_buffer(char *namep,uint *cposp) {
	Buffer *bufp;		// Trial buffer to complete.
	Buffer *bmatchp;	// Last buffer that matched string.
	bool comflag;		// Was there a completion at all?

	// Start attempting completions, one character at a time, but not more than the maximum buffer size.
	comflag = false;
	while(*cposp < NBNAME) {

		// Start at the first buffer and scan the list.
		bmatchp = NULL;
		bufp = bheadp;
		do {
			// Is this a match?
			if(*cposp > 0 && strncmp(namep,bufp->b_bname,*cposp) != 0)
				goto nextBuf;

			// A match.  If this is the first match, simply record it.
			if(bmatchp == NULL) {
				bmatchp = bufp;
				namep[*cposp] = bufp->b_bname[*cposp];	// Save next char.
				}
			else if(namep[*cposp] != bufp->b_bname[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto fail;
nextBuf:;
			// On to the next buffer.
			} while((bufp = bufp->b_nextp) != NULL);

		// With no match, we are done.
		if(bmatchp == NULL) {

			// Beep if we never matched.
			if(!comflag)
				(void) TTbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(namep[*cposp] == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		if(mlputc(MLRAW | MLTRACK,namep[(*cposp)++]) != SUCCESS || TTflush() != SUCCESS)
			return rc.status;
		}
fail:
	// Match not found.
	return NOTFOUND;
	}

// Attempt a completion on a command, alias, or macro name, given pointer to message line buffer (containing partial name to
// complete) and pointer to variable containing next character position in the buffer.  Return rc.status (SUCCESS) if unique
// name matched; otherwise, return NOTFOUND (bypassing rcset()).
static int comp_cfab(char *namep,uint *cposp,uint selector) {
	CFAMRec *frp;		// CFAM record pointer.
	CFAMRec *ematchp;	// Last entry that matched string.
	bool comflag;		// Was there a completion at all?

	// Start attempting completions, one character at a time.
	comflag = false;
	for(;;) {

		// Start at the first CFAM entry and scan the list.
		ematchp = NULL;
		frp = frheadp;
		do {
			// Is this a match?
			if(!(frp->fr_type & selector) || (*cposp > 0 && strncmp(namep,frp->fr_name,*cposp) != 0))
				goto nextentry;

			// A match.  If this is the first match, simply record it.
			if(ematchp == NULL) {
				ematchp = frp;
				namep[*cposp] = frp->fr_name[*cposp];	// Save next char.
				}
			else if(namep[*cposp] != frp->fr_name[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto fail;
nextentry:;
			// On to the next entry.
			} while((frp = frp->fr_nextp) != NULL);

		// With no match, we are done.
		if(ematchp == NULL) {

			// Beep if we never matched.
			if(!comflag)
				(void) TTbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(namep[*cposp] == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		if(mlputc(MLRAW | MLTRACK,namep[(*cposp)++]) != SUCCESS || TTflush() != SUCCESS)
			return rc.status;
		}
fail:
	// Match not found.
	return NOTFOUND;
	}

// Attempt a completion on a filename, given pointer to message line buffer (containing partial name to complete) and pointer to
// variable containing next character position in the buffer.  Return rc.status (SUCCESS) if unique name matched; otherwise,
// return NOTFOUND (bypassing rcset()).
static int comp_file(char *namep,uint *cposp) {
	char *fname;			// Trial file to complete.
	uint i;				// Index into strings to compare.
	uint matches;			// Number of matches for name.
	uint longestlen;		// Length of longest match (always > *cposp).
	char longestmatch[MaxPathname + 1];// Temp buffer for longest match.

	// Open the directory.
	namep[*cposp] = '\0';
	if(eopendir(namep,&fname) != SUCCESS)
		goto fail;

	// Start at the first file and scan the list.
	matches = 0;
	while(ereaddir() == SUCCESS) {

		// Is this a match?
		if(*cposp == 0 || strncmp(namep,fname,*cposp) == 0) {

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
			while(*cposp < MaxPathname && *cposp < longestlen) {
				namep[*cposp] = longestmatch[*cposp];
				if(mlputc(MLRAW | MLTRACK,namep[*cposp]) != SUCCESS)
					return rc.status;
				++*cposp;
				}

			namep[*cposp] = '\0';
			if(TTflush() != SUCCESS)
				return rc.status;

			// If only one file matched, it was completed unless the last character was a '/', indicating a
			// directory.  In that case we do NOT want to signal a complete match.
			if(matches == 1 && namep[*cposp - 1] != '/')
				return rc.status;
			}
		}
fail:
	// Match not found.
	return NOTFOUND;
	}

// Attempt a completion on a variable name, given pointer to message line buffer (containing partial name to complete), pointer
// to variable containing next character position in the buffer, variable list vector, and size.  Return rc.status (SUCCESS) if
// unique name matched; otherwise, return NOTFOUND (bypassing rcset()).
static int comp_var(char *namep,uint *cposp,char *vlistv[],uint vct) {
	char **namev;		// Trial name to complete.
	char *matchp;		// Last name that matched string.
	bool comflag;		// Was there a completion at all?

	// Start attempting completions, one character at a time, but not more than the maximum variable name size.
	comflag = false;
	while(*cposp < NVNAME) {

		// Start at the first name and scan the list.
		matchp = NULL;
		namev = vlistv;
		do {
			// Is this a match?
			if(*cposp > 0 && strncmp(namep,*namev,*cposp) != 0)
				goto nextName;

			// A match.  If this is the first match, simply record it.
			if(matchp == NULL) {
				matchp = *namev;
				namep[*cposp] = matchp[*cposp];		// Save next char.
				}
			else if(namep[*cposp] != (*namev)[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto fail;
nextName:;
			// On to the next name.
			} while(++namev < vlistv + vct);

		// With no match, we are done.
		if(matchp == NULL) {

			// Beep if we never matched.
			if(!comflag)
				(void) TTbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(namep[*cposp] == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		if(mlputc(MLRAW | MLTRACK,namep[(*cposp)++]) != SUCCESS || TTflush() != SUCCESS)
			return rc.status;
		}
fail:
	// Match not found.
	return NOTFOUND;
	}

// Make a completion list based on a partial buffer name, given pointer to input line buffer (containing partial name to
// complete), pointer to variable containing next character position, and indirect work buffer pointer.  Return status.
static int clist_buffer(char *namep,uint cpos,Buffer **bufpp) {
	Buffer *bufp;		// Trial buffer to complete.

	// Start at the first buffer and scan the list.
	bufp = bheadp;
	do {
		// Is this a match?
		if(cpos == 0 || strncmp(namep,bufp->b_bname,cpos) == 0)
			if(bappend(*bufpp,bufp->b_bname) != SUCCESS)
				return rc.status;

		// On to the next buffer.
		} while((bufp = bufp->b_nextp) != NULL);

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial name, given pointer to input line buffer (containing partial name to complete),
// pointer to variable containing next character position, indirect work buffer pointer, and selector mask.  Return status.
static int clist_cfab(char *namep,uint cpos,Buffer **bufpp,uint selector) {
	CFAMRec *frp;		// Trial name to complete.

	// Start at the first entry and scan the list.
	frp = frheadp;
	do {
		// Add name to the buffer if it's a match...
		if((frp->fr_type & selector) && (cpos == 0 || strncmp(namep,frp->fr_name,cpos) == 0))
			if(bappend(*bufpp,frp->fr_name) != SUCCESS)
				return rc.status;

		// and on to the next entry.
		} while((frp = frp->fr_nextp) != NULL);

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial filename, given pointer to input line buffer (containing partial name to complete),
// pointer to variable containing next character position, and indirect work buffer pointer.  Return status.
static int clist_file(char *namep,uint cpos,Buffer **bufpp) {
	char *fname;		// Trial file to complete.

	// Open the directory.
	namep[cpos] = '\0';
	if(eopendir(namep,&fname) != SUCCESS)
		return rc.status;

	// Start at the first file and scan the list.
	while(ereaddir() == SUCCESS) {

		// Is this a match?
		if(cpos == 0 || strncmp(namep,fname,cpos) == 0)
			if(bappend(*bufpp,fname) != SUCCESS)
				return rc.status;
		}

	// Error?
	if(rc.status != SUCCESS)
		return rc.status;

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial variable name, given pointer to input line buffer (containing partial name to
// complete), pointer to variable containing next character position, indirect work buffer pointer, variable list vector, and
// size.  Return status.
static int clist_var(char *namep,uint cpos,Buffer **bufpp,char *vlistv[],uint vct) {
	char **namev;		// Trial buffer to complete.

	// Start at the first name and scan the list.
	namev = vlistv;
	do {
		// Is this a match?
		if(cpos == 0 || strncmp(namep,*namev,cpos) == 0)
			if(bappend(*bufpp,*namev) != SUCCESS)
				return rc.status;

		// On to the next name.
		} while(++namev < vlistv + vct);

	return bpop(*bufpp,false,false);
	}

// Get one key from terminal.
static int terminp1(Value *rp,char *defvalp,uint terminator,uint flags) {
	uint ek;

	if(getkey(&ek) != SUCCESS)
		return rc.status;

	// Clear the message line.
	mlerase(MLFORCE);

	// Terminator?
	if(ek == terminator) {

		// Yes, return default value or nil.
		if(defvalp == NULL)
			goto nilrtn;
		return vsetstr(defvalp,rp);
		}

	// No, ^K?
	if(ek == (CTRL | 'K')) {

		// Yes, return nil or null string and current status.
		if(flags & ARG_NOTNULL)
nilrtn:
			(void) vnilmm(rp);
		else
			vnull(rp);
		return rc.status;
		}

	// No, user abort?
	if(ek == corekeys[CK_ABORT].ek) {

		// Yes, ignore buffer and return USERABORT.
		return abortinp();
		}

	// Non-printable and ARG_PRINT set?
	if((flags & ARG_PRINT) && (ek < ' ' || ek > '~')) {
		char keybuf[16];
		return rcset(FAILURE,0,"%s%s%s",text349,ektos(ek,keybuf),text345);
				//  "Mark key '","' must be a printable character"
		}

	// No, save key and return.
	vsetchr(ektoc(ek),rp);
	return rc.status;
	}

// Expand ~, ~username, or $var in input buffer if possible.  Return status.
int replvar(char *inpbufp,uint *cposp,uint maxlen) {
	char *strp;

	if(inpbufp[0] == '~') {
		if(*cposp == 1) {

			// Solo '~'.
			strp = "HOME";
			goto evar;
			}
		else {
			// Have ~username ... lookup the home directory.
			inpbufp[*cposp] = '\0';
			struct passwd *pwd = getpwnam(inpbufp + 1);
			if(pwd == NULL)
				goto beep;
			strp = pwd->pw_dir;
			}
		}
	else {
		// Expand environmental variable.
		inpbufp[*cposp] = '\0';
		strp = inpbufp + 1;
evar:
		if((strp = getenv(strp)) == NULL || strlen(strp) >= maxlen) {
beep:
			if(TTbeep() == SUCCESS)
				(void) TTflush();
			return rc.status;
			}
		}

	// Expansion successful.  Erase the chars on-screen...
	do {
		if(mlputc(MLRAW | MLTRACK,'\b') != SUCCESS)
			return rc.status;
		} while(--*cposp > 0);

	// display the variable, store in the buffer...
	while(*strp != '\0') {
		if(mlputc(MLTRACK,*strp) != SUCCESS)
			return rc.status;
		inpbufp[(*cposp)++] = *strp++;
		}

	// and add a trailing slash.
	if(mlputc(MLRAW | MLTRACK,'/') != SUCCESS || TTflush() != SUCCESS)
		return rc.status;
	inpbufp[(*cposp)++] = '/';

	return rc.status;
	}

// Get input from the terminal, possibly with completion, given result pointer, prompt, default value (which may be NULL), input
// terminator, maximum input length (<= NTERMINP), and flags.  Return status.  Flags are: type of completion + selector mask for
// TERM_C_CFAM calls + argument flags for determining return value.  If completion is requested, the terminator is forced to be
// CR.  If maxlen is zero, it is set to the maximum.
int terminp(Value *rp,char *prmtp,char *defvalp,uint terminator,uint maxlen,uint flags) {
	char prompt[term.t_ncol];

	// Error if doing a completion but echo is off.
	if(flags & TERM_C_MASK) {
		if(flags & TERM_NOKECHO)
			return rcset(FAILURE,0,text54);
				// "Key echo required for completion"
		terminator = CTRL | 'M';
		}

	// Build and display prompt.
	if(buildprompt(prompt,prmtp,defvalp,terminator) != SUCCESS)
		return rc.status;

	// One key?
	if(flags & TERM_ONEKEY)
		return terminp1(rp,defvalp,terminator,flags);

	// Not a single-key request.  Get multiple characters, with completion if applicable.
	int c;
	uint ek;			// Current extended input character.
	int status;
	uint cpos;			// Current character position in input buffer.
	bool quotef = false;		// Quoting next character?
	bool kill = false;		// ^K entered?
	uint vct;			// Variable list size.
	char *vlist[vct = flags & (TERM_C_VAR | TERM_C_SVAR) ? varct(flags) : 1];	// Variable name list.
	char inpbuf[NTERMINP + 1];	// Input buffer.

	if(maxlen == 0)
		maxlen = NTERMINP;
	if(flags & (TERM_C_VAR | TERM_C_SVAR))
		varlist(vlist,vct,flags);

	// Start at the beginning of the input buffer.
	cpos = 0;

	// Get a string from the user.
	for(;;) {
		// Get a keystroke and decode it.
		if(getkey(&ek) != SUCCESS)
			return rc.status;

		// If the line terminator or ^K was entered, wrap it up.
		if(ek == terminator && !quotef) {
wrapup:
			if(cpos > 0 || kill || defvalp == NULL)
				inpbuf[cpos] = '\0';

			// Clear the message line.
			mlerase(MLFORCE);

			// If nothing was entered, return default value, null, or nil.
			if(cpos == 0) {
				if(kill) {
					if(flags & ARG_NOTNULL)
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
			if(ek == corekeys[CK_ABORT].ek)			// Abort ... bail out.
				return abortinp();
			if(ek & FKEY)					// Function key ... blow it off.
				continue;
			if(ek == (CTRL | '?')) {			// Rubout/erase ... back up.
				if(cpos > 0) {
					if(!(flags & TERM_NOKECHO) &&
					 (mlputc(MLRAW | MLTRACK,'\b') != SUCCESS || TTflush() != SUCCESS))
						return rc.status;
					--cpos;
					}
				continue;
				}
			if(ek == (CTRL | 'K')) {			// ^K ... return null string.
				cpos = 0;
				kill = true;
				goto wrapup;
				}
			if(ek == (CTRL | 'U')) {			// ^U ... kill buffer and continue.
				if(flags & TERM_NOKECHO)
					cpos = 0;
				else {
					while(cpos > 0) {
						if(mlputc(MLRAW | MLTRACK,'\b') != SUCCESS)
							return rc.status;
						--cpos;
						}
					if(TTflush() != SUCCESS)
						return rc.status;
					}
				continue;
				}
			if(ek == corekeys[CK_QUOTE].ek) {		// ^Q ... quote next character.
				quotef = true;
				continue;
				}

			// Check completion special keys.
			if(flags & TERM_C_MASK) {
				if(ek == (CTRL | '[')) {		// Escape ... cancel.
					if(vnilmm(rp) == SUCCESS)
						(void) mlerase(MLFORCE);
					return rc.status;
					}
				if(ek == (CTRL | 'I')) {		// Tab ... attempt a completion.
					switch(flags & TERM_C_MASK) {
						case TERM_C_BUFFER:
							status = comp_buffer(inpbuf,&cpos);
							break;
						case TERM_C_CFAM:
							status = comp_cfab(inpbuf,&cpos,flags);
							break;
						case TERM_C_FNAME:
							status = comp_file(inpbuf,&cpos);
							break;
						default:	// TERM_C_VAR or TERM_C_SVAR
							status = comp_var(inpbuf,&cpos,vlist,vct);
						}
					if(TTflush() != SUCCESS)
						return rc.status;
					if(status == SUCCESS && !(flags & TERM_C_NOAUTO)) {	// Success!
						cpause(fencepause);
						break;
						}
					goto clist;
					}
				if(ek == '/' && (flags & TERM_C_FNAME) && ((inpbuf[0] == '~' && cpos > 0) ||
				 (inpbuf[0] == '$' && cpos > 1))) {
					if(replvar(inpbuf,&cpos,maxlen) != SUCCESS)
						return rc.status;
					continue;
					}
				if(ek == '?') {
					Buffer *bufp;
clist:
					// Make a completion list and pop it (without key input).
					if(sysbuf(text125,&bufp) != SUCCESS)
							// "CompletionList"
						return rc.status;
					switch(flags & TERM_C_MASK) {
						case TERM_C_BUFFER:
							status = clist_buffer(inpbuf,cpos,&bufp);
							break;
						case TERM_C_CFAM:
							status = clist_cfab(inpbuf,cpos,&bufp,flags);
							break;
						case TERM_C_FNAME:
							status = clist_file(inpbuf,cpos,&bufp);
							break;
						default:	// TERM_C_VAR or TERM_C_SVAR
							status = clist_var(inpbuf,cpos,&bufp,vlist,vct);
						}

					// Check status, delete the pop-up buffer, and reprompt the user.
					if(status != SUCCESS || bdelete(bufp,0) != SUCCESS)
						return rc.status;
					inpbuf[cpos] = '\0';
					if(mlprintf(MLHOME | MLFORCE,"%s%s",prompt,inpbuf) != SUCCESS)
						return rc.status;

					// Pause so that user can see the window.
					if(getkey(&ek) != SUCCESS)
						return rc.status;

					// "Unget" the key for reprocessing if it's not an escape character.
					if(ek != (CTRL | '['))
						tungetc(ek);

					// Update the screen, restore cursor position on message line, and continue.
					if(update(true) != SUCCESS || mlrestore() != SUCCESS)
						return rc.status;
					continue;
					}
				}
			}

		// Non-special or quoted character.
		quotef = false;

		// If it is a (quoted) function key, insert it's name.
		if(ek & (FKEY | SHFT)) {
			char *kp,keybuf[16];

			ektos(ek,keybuf);
			kp = keybuf;
			while(*kp != '\0') {
				if(cpos == maxlen)
					break;
				if(!(flags & TERM_NOKECHO) && mlputc(MLRAW | MLTRACK,*kp) != SUCCESS)
					return rc.status;
				inpbuf[cpos++] = *kp++;
				}
			if(TTflush() != SUCCESS)
				return rc.status;
			continue;
			}

		// Plain character .. insert it into the buffer if room and 7-bit; otherwise, ignore it and beep the beeper.
		if(cpos < maxlen && (c = ektoc(ek)) >= '\0' && c < 0x7f) {
			inpbuf[cpos++] = c;
			if(!(flags & TERM_NOKECHO) && (mlputc(MLTRACK,c) != SUCCESS || TTflush() != SUCCESS))
				return rc.status;
			}
		else if(TTbeep() != SUCCESS || TTflush() != SUCCESS)
			return rc.status;
		}

	// Terminal input completed.  Evaluate result if applicable and return.
	if(flags & TERM_EVAL)
		(void) doestmt(rp,inpbuf,TKC_COMMENT,NULL);
	else if(vsetstr(inpbuf,rp) != 0)
		(void) vrcset();

	return rc.status;
	}

// Ask a yes or no question on the message line and set *resultp to the answer (true or false).  Used any time a confirmation is
// required.  Return status, including USERABORT, if user pressed the abort key.
int mlyesno(char *prmtp,bool *resultp) {
	Value *vp;
	StrList prompt;

	// Build prompt.
	if(vnew(&vp,false) != 0 || vopen(&prompt,NULL,false) != 0 ||
	 vputs(prmtp,&prompt) != 0 || vputs(text162,&prompt) != 0 || vclose(&prompt) != 0)
						// " (y/n)?"
		return vrcset();

	// Prompt user, get the response, and check it.
	for(;;) {
		if(terminp(vp,prompt.sl_vp->v_strp,"n",CTRL | 'M',0,TERM_ONEKEY) != SUCCESS)
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

// Main routine for getting the next argument from the terminal or a command line, given result pointer, prompt, default value,
// input terminator, maxiumum terminal input length, and argument flags.  If in script mode, call funcarg(); otherwise, call
// terminp() possibly with completion and/or other terminal input options (specified in aflags).  Return status and value per
// the following table:
//
//	Input Result			Return status	Return Value
//	============================	==============	============
//	Interactive: Terminator only	SUCCESS		Default value if specified; otherwise, nil
//	Interactive: ^K			SUCCESS		nil if ARG_NOTNULL; otherwise, null string
//	Interactive: Char(s) entered	SUCCESS		Input string
//
//	Cmdline: No arg			FAILURE		(Undefined)
//	Cmdline: Null string		FAILURE		Null string
//	Cmdline: Non-null string	SUCCESS		Argument string
//
// Always return SUCCESS in interactive mode so that an error is not displayed on the message line when the user enters just the
// terminator (usually RETURN) at a command prompt, effectively cancelling the operation.  Invalid input can be determined by
// checking for a nil return value.
int getarg(Value *rp,char *prmtp,char *defvalp,uint terminator,uint maxlen,uint aflags) {

	return (opflags & OPSCRIPT) ? funcarg(rp,aflags) : terminp(rp,prmtp,defvalp,terminator,maxlen,aflags);
	}

// Convert extended key to character.  Collapse the CTRL and FKEY flags back into an ASCII code.
int ektoc(uint ek) {

	if(ek == (CTRL | ' '))
		return 0;		// Null char.
	if(ek & CTRL)
		ek ^= (CTRL | 0x40);	// Actual control char.
	if(ek & (FKEY | SHFT))
		ek &= 0xff;		// Remove FKEY and/or SHFT bits.
	return ek;
	}

// Convert character to extended key.  Change control character to CTRL prefix (upper bit).
int ctoec(int c) {

	if(c == 0)
		c = CTRL | ' ';
	else if(c < ' ' || c == 0x7f)
		c = CTRL | (c ^ 0x40);
	return c;
	}

// Check if given string is a command, pseudo-command, function, alias, buffer, or macro, according to selector masks.  If not
// found, return -1; otherwise, if wrong (alias) type, return 1; otherwise, set "cfabp" (if not NULL) to result and return 0.
int cfabsearch(char *strp,CFABPtr *cfabp,uint selector) {
	CFABPtr cfab;

	// Figure out what the string is.
	if((selector & (PTRCMDTYP | PTRFUNC)) &&
	 (cfab.u.p_cfp = ffind(strp)) != NULL) {			// Is it a command or function?
		ushort foundtype = (cfab.u.p_cfp->cf_flags & CFFUNC) ? PTRFUNC :
		 (cfab.u.p_cfp->cf_flags & CFHIDDEN) ? PTRPSEUDO : PTRCMD;
		if(!(selector & foundtype))				// Yep, correct type?
			return -1;					// No.
		cfab.p_type = foundtype;				// Yes, set it.
		}
	else if((selector & PTRALIAS) &&
	 afind(strp,OPQUERY,NULL,&cfab.u.p_aliasp)) {			// Not a function ... is it an alias?
		ushort foundtype = cfab.u.p_aliasp->a_type;
		if(!(selector & foundtype))				// Yep, correct type?
			return 1;					// No.
		cfab.p_type = cfab.u.p_aliasp->a_type;			// Yes, set it.
		}
	else if((selector & PTRBUF) &&					// No, is it a buffer?
	 (cfab.u.p_bufp = bsrch(strp,NULL)) != NULL)			// Yep.
		cfab.p_type = PTRBUF;
	else if(selector & PTRMACRO) {					// No, is it a macro?
		char mac[NBNAME + 1];

		sprintf(mac,MACFORMAT,NBNAME - 1,strp);
		if((cfab.u.p_bufp = bsrch(mac,NULL)) != NULL)		// Yep.
			cfab.p_type = PTRMACRO;
		else
			return -1;					// No, it's a bust.
		}
	else
		return -1;

	// Found it.
	if(cfabp != NULL)
		*cfabp = cfab;
	return 0;
	}

// Get a completion from the user for a command, function, alias, or macro name, given prompt, selector mask, result pointer,
// "not found" error message, and "wrong (alias) type" error message (if applicable; otherwise, NULL).
int getcfam(char *prmtp,uint selector,CFABPtr *cfabp,char *emsg0,char *emsg1) {
	Value *vp;

	if(vnew(&vp,false) != 0)
		return vrcset();
	if(getarg(vp,prmtp,NULL,CTRL | 'M',NTERMINP,ARG_FIRST | ARG_STR | TERM_C_CFAM | selector) != SUCCESS)
		return rc.status;
	if((opflags & OPSCRIPT) || !vistfn(vp,VNIL)) {
		int rcode;

		// Convert the string to a pointer.
		return (rcode = cfabsearch(vp->v_strp,cfabp,selector)) == 0 ? rc.status :
		 rcset(FAILURE,0,rcode < 0 ? emsg0 : emsg1,vp->v_strp);
		}

	cfabp->p_type = PTRNUL;
	cfabp->u.p_voidp = NULL;

	return rc.status;
	}

// Get a completion from the user for a buffer name, given result pointer, prompt, default value (which may be NULL), operation
// flag, result pointer, and "buffer was created" pointer (which also may be NULL).  "createdp" is passed to bfind().
int bcomplete(Value *rp,char *prmtp,char *defname,uint op,Buffer **bufpp,bool *createdp) {
	int status;
	char pbuf[32];

	sprintf(pbuf,"%s %s",prmtp,text83);
				// "buffer"
	if(getarg(rp,pbuf,defname,CTRL | 'M',NBNAME,ARG_FIRST | ARG_STR | TERM_C_BUFFER) != SUCCESS)
		return rc.status;
	if(!vistfn(rp,VNIL)) {
		status = bfind(rp->v_strp,op == OPCREATE ? CRBCREATE : CRBQUERY,0,bufpp,createdp);
		if(op == OPCREATE || status) {
			if(!(opflags & OPSCRIPT))
				(void) mlerase(0);
			return rc.status;
			}

		// Buffer not found.
		if(op == OPQUERY && !(opflags & OPSCRIPT)) {
			(void) mlerase(0);
			return NOTFOUND;
			}
		}
	else if(!(opflags & OPSCRIPT)) {
		(void) mlerase(0);
		*bufpp = NULL;
		return rc.status;
		}

	// Return error.
	return rcset(FAILURE,0,text118,rp->v_strp);
		// "No such buffer '%s'"
	}
