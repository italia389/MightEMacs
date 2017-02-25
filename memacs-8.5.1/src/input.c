// (c) Copyright 2017 Richard W. Marinelli
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
#include "std.h"
#include "lang.h"
#include "cmd.h"
#include "bind.h"
#include "exec.h"
#include "main.h"
#include "var.h"

// "Unget" a key from getkey().
void tungetc(ushort ek) {

	kentry.chpending = ek;
	kentry.ispending = true;
	}

// Get one keystroke from the pending queue or the terminal driver, resolve any keyboard macro action, and return it in *keyp as
// an extended key.  The legal prefixes here are the FKey and Ctrl.  Return status.
int getkey(ushort *keyp) {
	ushort ek;		// Next input character.
	ushort upper;		// Upper byte of the extended sequence.

	// If a character is pending...
	if(kentry.ispending) {

		// get it and return it.
		ek = kentry.chpending;
		kentry.ispending = false;
		goto Retn;
		}

	// Otherwise, if we are playing a keyboard macro back...
	if(kmacro.km_state == KMPlay) {

		// End of macro?
		if(kmacro.km_slotp == kmacro.km_endp) {

			// Yes.  Repeat?
			if(--kmacro.km_n <= 0) {

				// Non-positive counter.  Error if hit loop maximum.
				if(kmacro.km_n < 0 && maxloop > 0 && abs(kmacro.km_n) > maxloop)
					return rcset(Failure,0,text112,maxloop);
						// "Maximum number of loop iterations (%d) exceeded!"
				if(kmacro.km_n == 0) {

					// Last rep.  Stop playing and update screen.
					kmacro.km_state = KMStop;
#if !VizMacro
					(void) update(false);
#endif
					goto Fetch;
					}
				}

			// Not last rep.  Reset the macro to the begining for the next one.
			kmacro.km_slotp = kmacro.km_buf;
			}

		// Return the next key.
		ek = *kmacro.km_slotp++;
		goto Retn;
		}
Fetch:
	// Otherwise, fetch a character from the terminal driver.
	if(TTgetc(&ek) != Success)
		return rc.status;

	// If it's an escape sequence, process it.
	if(ek == 0) {

		// Get the event type and code...
		if(TTgetc(&upper) != Success || TTgetc(&ek) != Success)
			return rc.status;

		// and convert to an extended key.
		ek = (upper << 8) | ek;
		}

	// Normalize the control prefix.
	if((ek & 0xff) <= 0x1f || (ek & 0xff) == 0x7f)
		ek = Ctrl | (ek ^ 0x40);

	// Save it if we need to...
	if(kmacro.km_state == KMRecord) {

		// Don't overrun the buffer.
		if(kmacro.km_slotp == kmacro.km_buf + kmacro.km_size)
			if(growKeyMacro() != Success)
				return rc.status;
		*kmacro.km_slotp++ = ek;
		}
Retn:
	// and finally, return the extended key.
	*keyp = ek;
	return rc.status;
	}

// Get a key sequence from the keyboard and return it in *keyp.  Process all applicable prefix keys.  If kdpp is not NULL, set
// *kdpp to the getbind() return pointer.  Return status.
int getkseq(ushort *keyp,KeyDesc **kdpp) {
	ushort ek;		// Fetched keystroke.
	ushort p;		// Prefix mask.
	KeyDesc *kdp;		// Pointer to a key entry.

	// Get initial key.
	if(getkey(&ek) != Success)
		return rc.status;
	kdp = getbind(ek);

	// Get another key if first one was a prefix.
	if(kdp != NULL && kdp->k_cfab.p_type == PtrPseudo) {
		switch((enum cfid) (kdp->k_cfab.u.p_cfp - cftab)) {
			case cf_metaPrefix:
				p = Meta;
				goto Next;
			case cf_prefix1:
				p = Pref1;
				goto Next;
			case cf_prefix2:
				p = Pref2;
				goto Next;
			case cf_prefix3:
				p = Pref3;
Next:
				if(getkey(&ek) != Success)
					return rc.status;
				ek = upcase[ek & 0xff] | (ek & ~0xff) | p;	// Force to upper case and add prefix.
				if(kdpp != NULL) {
					kdp = getbind(ek);			// Get new binding if requested.
					goto Retn;
					}
				break;
			default:		// Do nothing.
				break;
			}
		}

	if(kdpp != NULL) {
Retn:
		*kdpp = kdp;
		}

	// Return it.
	*keyp = kentry.lastkseq = ek;
	return rc.status;
	}

// Build a prompt, shrink it if necessary to fit in the current terminal width, save the result in dest buffer (>= current
// terminal width), and display it.  If the prompt string begins with a letter or a default value is given, append ": ";
// otherwise, if the prompt string begins with a single (') or double (") quote character, strip if off and use the remainder as
// is; however, move any trailing white space to the very end.  Return status.
static int buildprompt(char *dest,char *prmt,char *defval,uint terminator) {
	bool addSpace;
	bool literal = false;
	bool addColon = false;
	char *src,*prmtz;
	DStrFab prompt;

	if(dopentrk(&prompt) != 0)
		return drcset();
	src = strchr(prmt,'\0');

	if(isletter(*prmt))
		addColon = true;
	else if(*prmt == '\'' || *prmt == '"') {
		++prmt;
		literal = true;
		while(src[-1] == ' ')
			--src;
		}

	// Save initial prompt.
	if(dputmem((void *) prmt,src - prmt,&prompt) != 0)
		return drcset();
	addSpace = *prmt != '\0' && src[-1] != ' ';
	if(defval != NULL) {
		if(addSpace) {
			if(dputc(' ',&prompt) != 0)
				return drcset();
			addSpace = false;
			}
		if(dputc('[',&prompt) != 0 || dputs(defval,&prompt) != 0 || dputc(']',&prompt) != 0)
			return drcset();
		addColon = true;
		}
	if(terminator != RtnKey && terminator != AltRtnKey) {
		if(addSpace && dputc(' ',&prompt) != 0)
			return drcset();
		if(dputc(ektoc(terminator),&prompt) != 0)
			return drcset();
		addColon = true;
		}
	if(*src == ' ' && dputs(src,&prompt) != 0)
		return drcset();

	// Add two characters to make room for ": " (if needed) and close the string-fab object.
	if(dputs("xx",&prompt) != 0 || dclose(&prompt,sf_string) != 0)
		return drcset();
	prmtz = strchr(prmt = prompt.sf_datp->d_str,'\0');
	prmtz[-2] = '\0';

	// Add ": " if needed.
	if(*src != ' ' && addColon && !literal && prmtz[-3] != ' ')
		strcpy(prmtz - 2,": ");

	// Make sure prompt fits on the screen.
	strfit(dest,96 * term.t_ncol / 100,prmt,0);
	return mlputs(MLHome | MLForce | MLTrack,dest);
	}

// Attempt a completion on a buffer name, given pointer to message line buffer (containing partial name to complete) and pointer
// to variable containing next character position in the buffer.  Return rc.status (Success) if unique name matched; otherwise,
// return NotFound (bypassing rcset()).
static int comp_buffer(char *name,uint *cposp) {
	Buffer *bufp;		// Trial buffer to complete.
	Buffer *bmatchp;	// Last buffer that matched string.
	bool comflag;		// Was there a completion at all?

	// Start attempting completions, one character at a time, but not more than the maximum buffer size.
	comflag = false;
	while(*cposp < NBufName) {

		// Start at the first buffer and scan the list.
		bmatchp = NULL;
		bufp = bheadp;
		do {
			// Is this a match?
			if(*cposp > 0 && strncmp(name,bufp->b_bname,*cposp) != 0)
				goto NextBuf;

			// A match.  If this is the first match, simply record it.
			if(bmatchp == NULL) {
				bmatchp = bufp;
				name[*cposp] = bufp->b_bname[*cposp];	// Save next char.
				}
			else if(name[*cposp] != bufp->b_bname[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextBuf:;
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
		if(name[*cposp] == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		if(mlputc(MLRaw | MLTrack,name[(*cposp)++]) != Success || TTflush() != Success)
			return rc.status;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a command, alias, or macro name, given pointer to message line buffer (containing partial name to
// complete), pointer to variable containing next character position in the buffer, and selector mask(s).  Return rc.status
// (Success) if unique name matched; otherwise, return NotFound (bypassing rcset()).
static int comp_cfab(char *name,uint *cposp,uint selector) {
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
			if(!(frp->fr_type & selector) || (*cposp > 0 && strncmp(name,frp->fr_name,*cposp) != 0))
				goto NextEntry;

			// A match.  If this is the first match, simply record it.
			if(ematchp == NULL) {
				ematchp = frp;
				name[*cposp] = frp->fr_name[*cposp];	// Save next char.
				}
			else if(name[*cposp] != frp->fr_name[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextEntry:;
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
		if(name[*cposp] == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		if(mlputc(MLRaw | MLTrack,name[(*cposp)++]) != Success || TTflush() != Success)
			return rc.status;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a filename, given pointer to message line buffer (containing partial name to complete) and pointer to
// variable containing next character position in the buffer.  Return rc.status (Success) if unique name matched; otherwise,
// return NotFound (bypassing rcset()).
static int comp_file(char *name,uint *cposp) {
	char *fname;			// Trial file to complete.
	uint i;				// Index into strings to compare.
	uint matches;			// Number of matches for name.
	uint longestlen;		// Length of longest match (always > *cposp).
	char longestmatch[MaxPathname + 1];// Temp buffer for longest match.

	// Open the directory.
	name[*cposp] = '\0';
	if(eopendir(name,&fname) != Success)
		goto Fail;

	// Start at the first file and scan the list.
	matches = 0;
	while(ereaddir() == Success) {

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
					goto Fail;

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
	if(rc.status == Success) {

		// Beep if we never matched.
		if(matches == 0)
			(void) TTbeep();
		else {
			// The longestmatch buffer contains the longest match, so copy and display it.
			while(*cposp < MaxPathname && *cposp < longestlen) {
				name[*cposp] = longestmatch[*cposp];
				if(mlputc(MLRaw | MLTrack,name[*cposp]) != Success)
					return rc.status;
				++*cposp;
				}

			name[*cposp] = '\0';
			if(TTflush() != Success)
				return rc.status;

			// If only one file matched, it was completed unless the last character was a '/', indicating a
			// directory.  In that case we do NOT want to signal a complete match.
			if(matches == 1 && name[*cposp - 1] != '/')
				return rc.status;
			}
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a buffer or global mode name, given pointer to message line buffer (containing partial name to
// complete), pointer to variable containing next character position in the buffer, and mode selector.  Return rc.status
// (Success) if unique name matched; otherwise, return NotFound (bypassing rcset()).
static int comp_mode(char *name,uint *cposp,uint selector) {
	ModeSpec *msp;		// Mode record pointer.
	ModeSpec *ematchp;	// Last entry that matched string.
	bool comflag;		// Was there a completion at all?

	// Start attempting completions, one character at a time.
	comflag = false;
	for(;;) {

		// Start at the first ModeSpec entry and scan the list.
		ematchp = NULL;
		msp = (selector & Term_C_GMode) ? gmodeinfo : bmodeinfo;
		do {
			// Is this a match?
			if(*cposp > 0 && strncasecmp(name,msp->mlname,*cposp) != 0)
				goto NextEntry;

			// A match.  If this is the first match, simply record it.
			if(ematchp == NULL) {
				ematchp = msp;
				name[*cposp] = msp->mlname[*cposp];	// Save next char.
				}
			else if(name[*cposp] != msp->mlname[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextEntry:;
			// On to the next entry.
			} while((++msp)->mlname != NULL);

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
		if(mlputc(MLRaw | MLTrack,name[(*cposp)++]) != Success || TTflush() != Success)
			return rc.status;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a variable name, given pointer to message line buffer (containing partial name to complete), pointer
// to variable containing next character position in the buffer, variable list vector, and size.  Return rc.status (Success) if
// unique name matched; otherwise, return NotFound (bypassing rcset()).
static int comp_var(char *name,uint *cposp,char *vlistv[],uint vct) {
	char **namev;		// Trial name to complete.
	char *match;		// Last name that matched string.
	bool comflag;		// Was there a completion at all?

	// Start attempting completions, one character at a time, but not more than the maximum variable name size.
	comflag = false;
	while(*cposp < NVarName) {

		// Start at the first name and scan the list.
		match = NULL;
		namev = vlistv;
		do {
			// Is this a match?
			if(*cposp > 0 && strncmp(name,*namev,*cposp) != 0)
				goto NextName;

			// A match.  If this is the first match, simply record it.
			if(match == NULL) {
				match = *namev;
				name[*cposp] = match[*cposp];		// Save next char.
				}
			else if(name[*cposp] != (*namev)[*cposp])

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextName:;
			// On to the next name.
			} while(++namev < vlistv + vct);

		// With no match, we are done.
		if(match == NULL) {

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
		if(mlputc(MLRaw | MLTrack,name[(*cposp)++]) != Success || TTflush() != Success)
			return rc.status;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Make a completion list based on a partial buffer name, given pointer to input line buffer (containing partial name to
// complete), pointer to variable containing next character position, and indirect work buffer pointer.  Return status.
static int clist_buffer(char *name,uint cpos,Buffer **bufpp,bool forceall) {
	Buffer *bufp;		// Trial buffer to complete.

	// Start at the first buffer and scan the list.
	bufp = bheadp;
	do {
		// Is this a match?  Skip hidden buffers if no partial name.
		if((cpos == 0 && (forceall || !(bufp->b_flags & BFHidden))) ||
		 (cpos > 0 && strncmp(name,bufp->b_bname,cpos) == 0))
			if(bappend(*bufpp,bufp->b_bname) != Success)
				return rc.status;

		// On to the next buffer.
		} while((bufp = bufp->b_nextp) != NULL);

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial name, given pointer to input line buffer (containing partial name to complete),
// pointer to variable containing next character position, indirect work buffer pointer, and selector mask.  Return status.
static int clist_cfab(char *name,uint cpos,Buffer **bufpp,uint selector) {
	CFAMRec *frp;		// Trial name to complete.

	// Start at the first entry and scan the list.
	frp = frheadp;
	do {
		// Add name to the buffer if it's a match...
		if((frp->fr_type & selector) && (cpos == 0 || strncmp(name,frp->fr_name,cpos) == 0))
			if(bappend(*bufpp,frp->fr_name) != Success)
				return rc.status;

		// and on to the next entry.
		} while((frp = frp->fr_nextp) != NULL);

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial filename, given pointer to input line buffer (containing partial name to complete),
// pointer to variable containing next character position, and indirect work buffer pointer.  Return status.
static int clist_file(char *name,uint cpos,Buffer **bufpp) {
	char *fname;		// Trial file to complete.

	// Open the directory.
	name[cpos] = '\0';
	if(eopendir(name,&fname) != Success)
		return rc.status;

	// Start at the first file and scan the list.
	while(ereaddir() == Success) {

		// Is this a match?
		if(cpos == 0 || strncmp(name,fname,cpos) == 0)
			if(bappend(*bufpp,fname) != Success)
				return rc.status;
		}

	// Error?
	if(rc.status != Success)
		return rc.status;

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial mode name, given pointer to input line buffer (containing partial name to
// complete), pointer to variable containing next character position, indirect work buffer pointer, and selector mask.  Return
// status.
static int clist_mode(char *name,uint cpos,Buffer **bufpp,uint selector) {
	ModeSpec *msp;		// Trial name to complete.

	// Start at the first entry and scan the list.
	msp = (selector & Term_C_GMode) ? gmodeinfo : bmodeinfo;
	do {
		// Add name to the buffer if it's a match...
		if(cpos == 0 || strncasecmp(name,msp->mlname,cpos) == 0)
			if(bappend(*bufpp,msp->mlname) != Success)
				return rc.status;

		// and on to the next entry.
		} while((++msp)->mlname != NULL);

	return bpop(*bufpp,false,false);
	}

// Make a completion list based on a partial variable name, given pointer to input line buffer (containing partial name to
// complete), pointer to variable containing next character position, indirect work buffer pointer, variable list vector, and
// size.  Return status.
static int clist_var(char *name,uint cpos,Buffer **bufpp,char *vlistv[],uint vct) {
	char **namev;		// Trial buffer to complete.

	// Start at the first name and scan the list.
	namev = vlistv;
	do {
		// Is this a match?
		if(cpos == 0 || strncmp(name,*namev,cpos) == 0)
			if(bappend(*bufpp,*namev) != Success)
				return rc.status;

		// On to the next name.
		} while(++namev < vlistv + vct);

	return bpop(*bufpp,false,false);
	}

// Get one key from terminal.
static int terminp1(Datum *rp,char *defval,uint terminator,uint terminator2,uint aflags) {
	ushort ek;

	if(getkey(&ek) != Success)
		return rc.status;

	// Clear the message line.
	mlerase(MLForce);

	// Terminator?
	if(ek == terminator || ek == terminator2) {

		// Yes, return default value or nil.
		if(defval == NULL)
			dsetnil(rp);
		else
			(void) dsetstr(defval,rp);
		return rc.status;
		}

	// No, ^K?
	if(ek == (Ctrl | 'K')) {

		// Yes, return error or null string.
		if(aflags & CFNotNull1)
			return rcset(Failure,0,text187,text53);
				// "%s cannot be null","Value"
		dsetnull(rp);
		return rc.status;
		}

	// No, user abort?
	if(ek == corekeys[CK_Abort].ek) {

		// Yes, ignore buffer and return UserAbort.
		return abortinp();
		}

	// No, save key and return.
	dsetchr(ektoc(ek),rp);
	return rc.status;
	}

// Expand ~, ~username, or $var in input buffer if possible.  Return status.
static int replvar(char *inpbuf,uint *cposp,uint maxlen) {
	char *str;

	if(inpbuf[0] == '~') {
		if(*cposp == 1) {

			// Solo '~'.
			str = "HOME";
			goto ExpVar;
			}
		else {
			// Have ~username... lookup the home directory.
			inpbuf[*cposp] = '\0';
			struct passwd *pwd = getpwnam(inpbuf + 1);
			if(pwd == NULL)
				goto Beep;
			str = pwd->pw_dir;
			}
		}
	else {
		// Expand environmental variable.
		inpbuf[*cposp] = '\0';
		str = inpbuf + 1;
ExpVar:
		if((str = getenv(str)) == NULL || strlen(str) >= maxlen) {
Beep:
			if(TTbeep() == Success)
				(void) TTflush();
			return rc.status;
			}
		}

	// Expansion successful.  Erase the chars on-screen...
	do {
		if(mlputc(MLRaw | MLTrack,'\b') != Success)
			return rc.status;
		} while(--*cposp > 0);

	// display the variable, store in the buffer...
	while(*str != '\0') {
		if(mlputc(MLTrack,*str) != Success)
			return rc.status;
		inpbuf[(*cposp)++] = *str++;
		}

	// and add a trailing slash.
	if(mlputc(MLRaw | MLTrack,'/') != Success || TTflush() != Success)
		return rc.status;
	inpbuf[(*cposp)++] = '/';

	return rc.status;
	}

// Get input from the terminal, possibly with completion, and return in "rp" as a string (or nil) given result pointer, prompt,
// default value (which may be NULL), input terminator, maximum input length (<= NTermInp), argument flags, and control flags.
// Return status.  Argument flags are: validation flags (CFNotNull1 only).  Control flags are: type of completion + selector
// mask for Term_C_CFAM calls.  If completion is requested, the terminator is forced to be "return"; otherwise, if terminator is
// "return" or "newline", it's partner is also accepted as a terminator; otherwise, "return" is converted to "newline".  If
// maxlen is zero, it is set to the maximum.
int terminp(Datum *rp,char *prmt,char *defval,ushort terminator,uint maxlen,uint aflags,uint cflags) {
	ushort terminator2;
	char prompt[term.t_ncol];

	// Error if doing a completion but echo is off.
	if(cflags & Term_C_Mask) {
		if(cflags & Term_NoKeyEcho)
			return rcset(Failure,RCNoFormat,text54);
				// "Key echo required for completion"
		terminator = RtnKey;
		}

	// If terminator is CR or NL, accept it's partner as a terminator also.
	terminator2 = (terminator == RtnKey) ? AltRtnKey : (terminator == AltRtnKey) ? RtnKey : 0xFFFF;

	// Build and display prompt.
	if(buildprompt(prompt,prmt,defval,terminator) != Success)
		return rc.status;

	// One key?
	if(cflags & Term_OneKey)
		return terminp1(rp,defval,terminator,terminator2,aflags);

	// Not a single-key request.  Get multiple characters, with completion if applicable.
	ushort c;
	ushort ek;			// Current extended input character.
	int status;
	uint cpos;			// Current character position in input buffer.
	bool forceall;			// Show hidden buffers?
	bool quotef = false;		// Quoting next character?
	bool kill = false;		// ^K entered?
	uint vct;			// Variable list size.
	char *vlist[vct = cflags & (Term_C_Var | Term_C_SVar) ? varct(cflags) : 1];	// Variable name list.
	char inpbuf[NTermInp + 1];	// Input buffer.

	if(maxlen == 0)
		maxlen = NTermInp;
	if(cflags & (Term_C_Var | Term_C_SVar))
		varlist(vlist,vct,cflags);

	// Start at the beginning of the input buffer.
	cpos = 0;

	// Get a string from the user.
	for(;;) {
		// Get a keystroke and decode it.
		if(getkey(&ek) != Success)
			return rc.status;

		// If the line terminator or ^K was entered, wrap it up.
		if((ek == terminator || ek == terminator2) && !quotef) {
WrapUp:
			if(cpos > 0 || kill || defval == NULL)
				inpbuf[cpos] = '\0';

			// Clear the message line.
			mlerase(MLForce);

			// If nothing was entered, return default value, null, or nil.
			if(cpos == 0) {
				if(kill) {
					if(aflags & CFNotNull1)
						return rcset(Failure,0,text187,text53);
							// "%s cannot be null","Value"
					dsetnull(rp);
					}
				else if(defval == NULL)
					dsetnil(rp);
				else if(dsetstr(defval,rp) != 0)
					(void) drcset();
				return rc.status;
				}
			break;
			}

		// If not in quote mode, check key that was entered.
		if(!quotef) {
			if(ek == corekeys[CK_Abort].ek)			// Abort... bail out.
				return abortinp();
			if(ek & FKey)					// Function key... blow it off.
				continue;
			if(ek == (Ctrl | '?')) {			// Rubout/erase... back up.
				if(cpos > 0) {
					if(!(cflags & Term_NoKeyEcho) &&
					 (mlputc(MLRaw | MLTrack,'\b') != Success || TTflush() != Success))
						return rc.status;
					--cpos;
					}
				continue;
				}
			if(ek == (Ctrl | 'K')) {			// ^K... return null string.
				cpos = 0;
				kill = true;
				goto WrapUp;
				}
			if(ek == (Ctrl | 'U')) {			// ^U... kill buffer and continue.
				if(cflags & Term_NoKeyEcho)
					cpos = 0;
				else {
					while(cpos > 0) {
						if(mlputc(MLRaw | MLTrack,'\b') != Success)
							return rc.status;
						--cpos;
						}
					if(TTflush() != Success)
						return rc.status;
					}
				continue;
				}
			if(ek == corekeys[CK_Quote].ek) {		// ^Q... quote next character.
				quotef = true;
				continue;
				}

			if(ek == RtnKey)				// CR... convert to newline.
				ek = AltRtnKey;

			// Check completion special keys.
			else if(cflags & Term_C_Mask) {
				if(ek == (Ctrl | '[')) {		// Escape... cancel.
					dsetnil(rp);
					return mlerase(MLForce);
					}
				if(ek == (Ctrl | 'I')) {		// Tab... attempt a completion.
					switch(cflags & Term_C_Mask) {
						case Term_C_Buffer:
							status = comp_buffer(inpbuf,&cpos);
							break;
						case Term_C_CFAM:
							status = comp_cfab(inpbuf,&cpos,cflags);
							break;
						case Term_C_Filename:
							status = comp_file(inpbuf,&cpos);
							break;
						case Term_C_BMode:
						case Term_C_GMode:
							status = comp_mode(inpbuf,&cpos,cflags);
							break;
						default:	// Term_C_Var or Term_C_SVar
							status = comp_var(inpbuf,&cpos,vlist,vct);
						}
					if(TTflush() != Success)
						return rc.status;
					if(status == Success && !(cflags & Term_C_NoAuto)) {	// Success!
						cpause(fencepause);
						break;
						}
					forceall = false;
					goto CompList;
					}
				if(ek == '/' && (cflags & Term_C_Filename) && ((inpbuf[0] == '~' && cpos > 0) ||
				 (inpbuf[0] == '$' && cpos > 1))) {
					if(replvar(inpbuf,&cpos,maxlen) != Success)
						return rc.status;
					continue;
					}
				if(ek == '?') {
					Buffer *bufp;
					forceall = true;
CompList:
					// Make a completion list and pop it (without key input).
					if(sysbuf(text125,&bufp) != Success)
							// "CompletionList"
						return rc.status;
					switch(cflags & Term_C_Mask) {
						case Term_C_Buffer:
							status = clist_buffer(inpbuf,cpos,&bufp,forceall);
							break;
						case Term_C_CFAM:
							status = clist_cfab(inpbuf,cpos,&bufp,cflags);
							break;
						case Term_C_Filename:
							status = clist_file(inpbuf,cpos,&bufp);
							break;
						case Term_C_BMode:
						case Term_C_GMode:
							status = clist_mode(inpbuf,cpos,&bufp,cflags);
							break;
						default:	// Term_C_Var or Term_C_SVar
							status = clist_var(inpbuf,cpos,&bufp,vlist,vct);
						}

					// Check status, delete the pop-up buffer, and reprompt the user.
					if(status != Success || bdelete(bufp,0) != Success)
						return rc.status;
					inpbuf[cpos] = '\0';
					if(mlprintf(MLHome | MLForce,"%s%s",prompt,inpbuf) != Success)
						return rc.status;

					// Pause so that user can see the window.
					if(getkey(&ek) != Success)
						return rc.status;

					// "Unget" the key for reprocessing if it's not an escape character.
					if(ek != (Ctrl | '['))
						tungetc(ek);

					// Update the screen, restore cursor position on message line, and continue.
					if(update(true) != Success || mlrestore() != Success)
						return rc.status;
					continue;
					}
				}
			}

		// Non-special or quoted character.
		quotef = false;

		// If it is a (quoted) function key, insert it's name.
		if(ek & (FKey | Shft)) {
			char *kp,keybuf[16];

			ektos(ek,keybuf);
			kp = keybuf;
			while(*kp != '\0') {
				if(cpos == maxlen)
					break;
				if(!(cflags & Term_NoKeyEcho) && mlputc(MLRaw | MLTrack,*kp) != Success)
					return rc.status;
				inpbuf[cpos++] = *kp++;
				}
			if(TTflush() != Success)
				return rc.status;
			continue;
			}

		// Plain character... insert it into the buffer if room and 7-bit; otherwise, ignore it and beep the beeper.
		if(cpos < maxlen && (c = ektoc(ek)) < 0x7f) {
			inpbuf[cpos++] = c;
			if(!(cflags & Term_NoKeyEcho) && (mlputc(MLTrack,c) != Success || TTflush() != Success))
				return rc.status;
			}
		else if(TTbeep() != Success || TTflush() != Success)
			return rc.status;
		}

	// Terminal input completed.  Evaluate result if applicable and return.
	if(cflags & Term_Eval)
		(void) doestmt(rp,inpbuf,TokC_Comment,NULL);
	else if(dsetstr(inpbuf,rp) != 0)
		(void) drcset();

	return rc.status;
	}

// Ask a yes or no question on the message line and set *resultp to the answer (true or false).  Used any time a confirmation is
// required.  Return status, including UserAbort, if user pressed the abort key.
int mlyesno(char *prmt,bool *resultp) {
	Datum *datp;
	DStrFab prompt;

	// Build prompt.
	if(dnewtrk(&datp) != 0 || dopentrk(&prompt) != 0 ||
	 dputs(prmt,&prompt) != 0 || dputs(text162,&prompt) != 0 || dclose(&prompt,sf_string) != 0)
						// " (y/n)?"
		return drcset();

	// Prompt user, get the response, and check it.
	for(;;) {
		if(terminp(datp,prompt.sf_datp->d_str,"n",RtnKey,0,0,Term_OneKey) != Success)
			return rc.status;
		if(*datp->d_str ==
#if FRENCH
		 'o') {
#elif Spanish
		 's') {
#else
		 'y') {
#endif
			*resultp = true;
			break;
			}
		if(*datp->d_str == 'n') {
			*resultp = false;
			break;
			}
		if(TTbeep() != Success || mlerase(0) != Success)
			break;
		}
	return rc.status;
	}

// Main routine for getting the next argument from the terminal or a command line, given result pointer, prompt, default value,
// input terminator, maxiumum terminal input length, argument flags, and control flags.  If in script mode, call funcarg();
// otherwise, call terminp() possibly with completion and/or other terminal input options (specified in aflags).  Return status
// and value per the following table:
//
//	Input Result			Return status	Return Value
//	============================	==============	============
//	Interactive: Terminator only	Success		Default value if specified; otherwise, nil
//	Interactive: ^K			Success/Failure	Error if CFNotNull1; otherwise, null string
//	Interactive: Char(s) entered	Success		Input string
//
//	Cmdline: No arg			Failure		(Undefined)
//	Cmdline: Null string		Failure		Null string
//	Cmdline: Non-null string	Success		Argument string
//
// Always return Success in interactive mode so that an error is not displayed on the message line when the user enters just the
// terminator (usually RETURN) at a command prompt, effectively cancelling the operation.  Invalid input can be determined by
// checking for a nil return value.
int getarg(Datum *rp,char *prmt,char *defval,ushort terminator,uint maxlen,uint aflags,uint cflags) {

	return (opflags & OpScript) ? funcarg(rp,aflags) : terminp(rp,prmt,defval,terminator,maxlen,aflags,cflags);
	}

// Convert extended key to character.  Collapse the Ctrl and FKey flags back into an ASCII code.
ushort ektoc(ushort ek) {

	if(ek == (Ctrl | ' '))
		return 0;		// Null char.
	if(ek & Ctrl)
		ek ^= (Ctrl | 0x40);	// Actual control char.
	if(ek & (FKey | Shft))
		ek &= 0xff;		// Remove FKey and/or Shft bits.
	return ek;
	}

// Convert character to extended key.  Change control character to Ctrl prefix (upper bit).
ushort ctoek(uint c) {

	if(c == 0)
		c = Ctrl | ' ';
	else if(c < ' ' || c == 0x7f)
		c = Ctrl | (c ^ 0x40);
	return c;
	}

// Get a completion from the user for a command, function, alias, or macro name, given prompt, selector mask, result pointer,
// "not found" error message, and "wrong (alias) type" error message (if applicable; otherwise, NULL).
int getcfam(char *prmt,uint selector,CFABPtr *cfabp,char *emsg0,char *emsg1) {
	Datum *datp;

	if(dnewtrk(&datp) != 0)
		return drcset();
	if(getarg(datp,prmt,NULL,RtnKey,NTermInp,Arg_First | CFNotNull1,Term_C_CFAM | selector) != Success)
		return rc.status;
	if((opflags & OpScript) || datp->d_type != dat_nil) {
		int rcode;

		// Convert the string to a pointer.
		return (rcode = cfabsearch(datp->d_str,cfabp,selector)) == 0 ? rc.status :
		 rcset(Failure,0,rcode < 0 ? emsg0 : emsg1,datp->d_str);
		}

	cfabp->p_type = PtrNul;
	cfabp->u.p_voidp = NULL;

	return rc.status;
	}

// Get a completion from the user for a buffer name, given result pointer, prompt, default value (which may be NULL), operation
// flag, result pointer, and "buffer was created" pointer (which also may be NULL).  "createdp" is passed to bfind().
int bcomplete(Datum *rp,char *prmt,char *defname,uint op,Buffer **bufpp,bool *createdp) {
	char pbuf[32];

	sprintf(pbuf,"%s %s",prmt,text83);
				// "buffer"
	if(getarg(rp,pbuf,defname,RtnKey,NBufName,Arg_First | CFNotNull1,Term_C_Buffer) != Success)
		return rc.status;
	if(rp->d_type != dat_nil) {
		int status = bfind(rp->d_str,op == OpCreate ? CRBCreate : CRBQuery,0,bufpp,createdp);
		if(op == OpCreate || status) {
			if(!(opflags & OpScript))
				(void) mlerase(0);
			return rc.status;
			}

		// Buffer not found.
		if(op == OpQuery && !(opflags & OpScript)) {
			(void) mlerase(0);
			return NotFound;
			}
		}
	else if(!(opflags & OpScript)) {
		(void) mlerase(0);
		*bufpp = NULL;
		return rc.status;
		}

	// Return error.
	return rcset(Failure,0,text118,rp->d_str);
		// "No such buffer '%s'"
	}
