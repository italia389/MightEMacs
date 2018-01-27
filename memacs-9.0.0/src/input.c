// (c) Copyright 2018 Richard W. Marinelli
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
#include <ctype.h>
#include "std.h"
#include "cmd.h"
#include "bind.h"
#include "exec.h"
#include "search.h"
#include "var.h"

// Local definitions.

// Line editing commands.
enum e_termcmd {
	tc_ins,tc_bs,tc_del,tc_left,tc_right,tc_bol,tc_eol,tc_erase,tc_kill
	};

// One input character.
typedef struct {
	char c;				// Raw character.
	char len;			// Length of visible form of character.
	} InpChar;

// Terminal input state information, initialized by mli_open().
typedef struct {
	char *prompt;			// Prompt string.
	InpChar *ibuf;			// Pointer to caller's input buffer (of type InpChar [NTermInp]).
	InpChar *ibufc;			// Pointer to current position in ibuf.
	InpChar *ibufe;			// Pointer to end-of-input position in ibuf.
	InpChar *ibufz;			// Pointer to end-of-buffer position in ibuf.
	int lshift;			// Current left-shift of ibuf on message line.
	int clen0;			// Length of partial character displayed at beginning of input area.
	uint maxlen;			// Maximum length of user input.
	int icol0;			// Column in message line where user input begins (just past prompt).
	int isize;			// Size of input area (from icol0 to right edge of screen).
	int isize75;			// 75% of input area size.
	int jumpcols;			// Number of columns to shift input line when cursor moves out of input area.
	uint flags;			// Echo and terminal attribute flags.
	} InpState;

// "Unget" a key from getkey().
void tungetc(ushort ek) {

	kentry.chpending = ek;
	kentry.ispending = true;
	}

// Get one keystroke from the pending queue or the terminal driver, resolve any keyboard macro action, and return it in *keyp as
// an extended key.  The legal prefixes here are the FKey and Ctrl.  Return status.
int getkey(ushort *keyp) {
	ushort ek,c;

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

			// Yes, check if last iteration.
			if(--kmacro.km_n <= 0) {

				// Non-positive counter.  Error if infinite reps and hit loop maximum.
				if(kmacro.km_n < 0 && maxloop > 0 && abs(kmacro.km_n) > maxloop)
					return rcset(Failure,0,text112,maxloop);
						// "Maximum number of loop iterations (%d) exceeded!"
				if(kmacro.km_n == 0) {

					// Finished last repetition.  Stop playing and update screen.
					kmacro.km_state = KMStop;
					if(update(INT_MIN) != Success)
						return rc.status;
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
		if(TTgetc(&c) != Success || TTgetc(&ek) != Success)
			return rc.status;

		// and convert to an extended key.
		ek = (c << 8) | ek;
		}

	// Normalize the control prefix.
	if((c = (ek & 0xFF)) <= 0x1F || c == 0x7F)
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
				ek |= p;				// Add prefix.
				if(kdpp != NULL) {
					kdp = getbind(ek);		// Get new binding if requested.
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

// Shift rules:		$SC> a^T xyz ^R end<ES$
//			    ^		   ^
// Far right: term.t_ncol - 2 (or before partial char); far left: icol0 + 1 (or past partial char).
//
// OLD CURSOR		EVENT				SHIFT	NEW CURSOR (smooth)	NEW CURSOR (jump)
// ---------------	--------------------------	-----	-------------------	-----------------
// Far right		Char entered or ^F		Left	Far right		Far right - jump
// Anywhere		^E, cursor past far right	Left	3/4 input width
// Far left		Backspace or ^B			Right	Far left		Far left + jump

// Write a string to the message line, given beginning cursor column, buffer offset, "write leading LineExt ($)" flag, string
// pointer, and ending cursor column.  If "str" is NULL, write from current input buffer position - offset to end of input via
// non-raw mlputc() calls.  Characters are written until the cursor moves past the right edge of the screen or end of string is
// reached.  Return status.
static int mli_write(InpState *isp,int bcol,ushort boff,bool extChar,char *str,int ecol) {

#if MMDebug & Debug_Temp
fprintf(logfile,"mli_write(...,bcol: %d,boff: %hu,%s,\"%s\",ecol: %d): ibufc: '%s'...\n",
 bcol,boff,extChar ? "true" : "false",str,ecol,isp->ibufc == isp->ibufe ? "EOL" : vizc(isp->ibufc[-boff].c,VBaseDef));
#endif
	// Move cursor and write leading '$'.
	if((bcol != mlcol && mlmove(bcol) != Success) || (extChar && mlputc(MLRaw | MLForce,LineExt) != Success))
		return rc.status;

	// Display string.
	if(str == NULL) {
		InpChar *icp = isp->ibufc - boff;
		while(icp < isp->ibufe) {
			if(mlputc(MLForce,icp->c) != Success)
				return rc.status;
			if(mlcol >= term.t_ncol)
				goto Finish;
			++icp;
			}
		}
	else if(mlputs(MLRaw | MLForce | MLFlush,extChar ? str + 1 : str) != Success)
		return rc.status;

	if(mlcol < term.t_ncol && TTeeol() != Success)
		return rc.status;
Finish:
	// Reposition cursor if needed.
	if(mlcol != ecol)
		(void) mlmove(ecol);

	return rc.status;
	}

// Redisplay input buffer on message line.  This routine is called when:
//	1. A navigation key is entered, like ^A or ^F.
//	2. A character is inserted when cursor is at right edge of screen.
//	3. Backspace is entered when line is shifted and cursor is at or near beginning of input area.
//	4. Prompt and input buffer need to be redisplayed during completion in terminp().
// Cursor position in message line is recalculated so that cursor stays in visible range.  If input line needs to be shifted
// left as a result, character at home position is displayed as LineExt ($); if input line extends beyond right edge of screen,
// last character is displayed as LineExt ($).  Return status.
static int mli_update(InpState *isp,bool force) {
	int cpos1,cpos2;
	InpChar *icp;

#if MMDebug & Debug_Temp
fputs("mli_update()...\n",logfile);
#endif
	// Calculate new cursor position and determine if moving the cursor (only) is sufficient.  cpos1 is cursor position in
	// input area if line is not shifted; cpos2 is cursor position in input area keeping current shift value (which may be
	// zero).
	cpos1 = 0;
	for(icp = isp->ibuf; icp < isp->ibufc; ++icp)
		cpos1 += icp->len;
	cpos2 = cpos1 - isp->lshift;
	if(!force && (cpos2 > 0 || (cpos2 == 0 && isp->lshift == 0)) && cpos2 < isp->isize - 1) {
#if MMDebug & Debug_Temp
fprintf(logfile,"\tMoving cursor to column %d...\n",cpos2 + isp->icol0);
#endif
		// Cursor will still be in visible range... just move it.
		(void) mlmove(cpos2 + isp->icol0);
		}
	else {
		int n;
		char *str,wkbuf[isp->isize * 2];

#if MMDebug & Debug_Temp
fprintf(logfile,"\tRedisplaying line (isize %d, jumpcols %d)...\n",isp->isize,isp->jumpcols);
#endif
		// New cursor position out of visible range or a force... recalculate left shift.
		if(cpos2 < 0 || (cpos2 == 0 && isp->lshift > 0)) {

			// Current position at or before home column (line shifted too far left)... decrease shift amount.
			isp->lshift = (cpos1 <= 1) ? 0 : (isp->jumpcols == 1) ? cpos1 - 1 : isp->jumpcols >= cpos1 ? 0 :
			 cpos1 - isp->jumpcols;
			}
		else if(isp->ibufc == isp->ibufe) {

			// Current position at end of input... place cursor at 3/4 spot.  If shift result is negative (which
			// can happen on a force), set it to zero.
			if((isp->lshift = cpos1 - isp->isize75) < 0)
				isp->lshift = 0;
			}
		else {
			// Current position at or past right edge of screen... shift line left.
			isp->lshift = cpos1 - (isp->jumpcols == 1 ? isp->isize : isp->isize - isp->jumpcols) + 2;
			}

		// Redisplay visible portion of buffer on message line.
		isp->clen0 = n = 0;
		icp = isp->ibuf;
		for(;;) {
			if(n + icp->len > isp->lshift)
				break;
			n += icp->len;
			++icp;
			}
		cpos2 = isp->lshift - n;	// Beginning portion of current char to skip over (if any).
		if(cpos2 > 0)
			isp->clen0 = icp->len - cpos2;
		str = wkbuf;
		while(icp < isp->ibufe) {
			str = stpcpy(str,vizc(icp->c,VBaseDef));
			++icp;
			}
#if MMDebug & Debug_Temp
fprintf(logfile,"\tlshift: %d, clen0: %d\n",isp->lshift,isp->clen0);
#endif
		if(str > wkbuf)
			(void) mli_write(isp,isp->icol0,0,isp->lshift > 0,wkbuf + cpos2,isp->icol0 + cpos1 - isp->lshift);
		}
	return rc.status;
	}

// Convert buffer in InpState object to a string.  Return dest.
static char *ibuftos(char *dest,InpState *isp) {
	InpChar *icp = isp->ibuf;
	char *dest0 = dest;

	while(icp < isp->ibufe) {
		*dest++ = icp->c;
		++icp;
		}
	*dest = '\0';
	return dest0;
	}

// Delete a character from the message line input buffer.
static int mli_del(InpState *isp,enum e_termcmd cmd) {
	InpChar *icp1,*icp2;
	int n;

#if MMDebug & Debug_Temp
fprintf(logfile,"mli_del(...,cmd: %d)...\n",cmd);
#endif
	if(cmd == tc_bs) {				// Backspace key.
		if(isp->ibufc == isp->ibuf)
			goto BoundChk;
		--isp->ibufc;
		goto DelChar;
		}
	if(isp->ibufc == isp->ibufe) {			// ^D key.
BoundChk:
		if((isp->flags & Term_NoKeyEcho) && TTbeep() == Success)
			goto Flush;
		return rc.status;
		}
DelChar:
	icp1 = isp->ibufc;

	// Save display length of character being nuked, then remove it from buffer by left-shifting
	// characters that follow it (if any).
	n = icp1->len;
	icp2 = icp1 + 1;
	while(icp2 < isp->ibufe)
		*icp1++ = *icp2++;

	--isp->ibufe;

	// Adjust message line if echoing.
	if(!(isp->flags & Term_NoKeyEcho)) {

		// At or near beginning or end of line?
		if(cmd == tc_bs) {
			if(isp->lshift > 0 && mlcol - n <= isp->icol0) {
				if(mli_update(isp,false) != Success)
					return rc.status;
				goto Flush;
				}
			if(isp->ibufc == isp->ibufe && (isp->lshift == 0 || isp->clen0 > 0 || mlcol - isp->icol0 > 1)) {
#if MMDebug & Debug_Temp
fprintf(logfile,"mli_del(): quick delete of %d char(s).\n",n);
#endif
				do {
					if(mlputc(MLRaw | MLForce,'\b') != Success || mlputc(MLRaw | MLForce,' ') != Success ||
					 mlputc(MLRaw | MLForce,'\b') != Success)
						return rc.status;
					} while(--n > 0);
				goto Flush;
				}
			n = mlcol - n;
			}
		else {
			if(isp->ibufc == isp->ibufe && n == 1 &&
			 (isp->lshift == 0 || isp->clen0 > 0 || mlcol - isp->icol0 > 1)) {
				if(mlputc(MLRaw | MLForce,' ') == Success)
					(void) mlputc(MLRaw | MLForce,'\b');
				goto Flush;
				}
			n = mlcol;
			}

		// Not simple adjustment or special case... redo-from-cursor needed.
		if(mli_write(isp,n,0,n == isp->icol0 && isp->lshift > 0,NULL,n) == Success)
Flush:
			(void) TTflush();
		}
	return rc.status;
	}

// Insert a character into the message line input buffer.  If flush is true, call TTflush() if echoing.
static int mli_putc(short c,InpState *isp,bool flush) {

	if(isp->ibufe < isp->ibufz && c < 0x7F) {
#if MMDebug & Debug_Temp
fprintf(logfile,"mli_putc('%s',...)...\n",vizc(c,VBaseDef));
#endif
		// Shift input buffer right one position if insertion point not at end.
		if(isp->ibufc < isp->ibufe) {
			InpChar *icp1,*icp2;

			icp1 = (icp2 = isp->ibufe) - 1;
			do {
				*icp2-- = *icp1--;
				} while(icp2 > isp->ibufc);
			}
		isp->ibufc->c = c;
		++isp->ibufe;

		// Update message line.
		if((isp->flags & Term_NoKeyEcho))
			++isp->ibufc;
		else {
			char *str = vizc(c,VBaseDef);
			int len = strlen(str);
			isp->ibufc->len = len;
			++isp->ibufc;
			if(((mlcol + len < term.t_ncol - 1) ?
			 mli_write(isp,mlcol,1,false,NULL,mlcol + len) : mli_update(isp,false)) != Success)
				return rc.status;
			goto Flush;
			}
		}
	else if(TTbeep() == Success)
Flush:
		if(flush)
			(void) TTflush();
	return rc.status;
	}

// Insert a (possibly null) string into the message line input buffer.  Called only when "echo" is enabled.
static int mli_puts(char *str,InpState *isp) {

	while(*str != '\0')
		if(mli_putc(*str++,isp,false) != Success)
			return rc.status;
	return TTflush();
	}

// Move cursor in message line input buffer.
static int mli_nav(InpState *isp,enum e_termcmd cmd) {

	switch(cmd) {
		case tc_left:
			if(isp->ibufc == isp->ibuf)
				goto BoundChk;
			--isp->ibufc;
			break;
		case tc_right:
			if(isp->ibufc == isp->ibufe) {
BoundChk:
				if((isp->flags & Term_NoKeyEcho) && TTbeep() == Success)
					goto Flush;
				return rc.status;
				}
			++isp->ibufc;
			break;
		case tc_bol:
			if(isp->ibufc == isp->ibuf)
				return rc.status;
			isp->ibufc = isp->ibuf;
			break;
		default:	// tc_eol
			if(isp->ibufc == isp->ibufe)
				return rc.status;
			isp->ibufc = isp->ibufe;
		}

	if(!(isp->flags & Term_NoKeyEcho) && mli_update(isp,false) == Success)
Flush:
		(void) TTflush();
	return rc.status;
	}

// Truncate line in the message line input buffer.
static int mli_trunc(InpState *isp,enum e_termcmd cmd) {

	if(cmd == tc_kill) {
		if(isp->ibufe > isp->ibufc) {
			isp->ibufe = isp->ibufc;
			if(!(isp->flags & Term_NoKeyEcho))
				goto Trunc;
			}
		}
	else if(isp->ibufe > isp->ibuf) {
		isp->ibufc = isp->ibufe = isp->ibuf;
		if(!(isp->flags & Term_NoKeyEcho)) {
			isp->lshift = 0;
			if(mlmove(isp->icol0) == Success)
Trunc:
				if(TTeeol() == Success)
					(void) TTflush();
			}
		}
	return rc.status;
	}

// Erase message line, display prompt, and update "icol0" in InpState object.  Return status.
static int mli_prompt(InpState *isp) {

	// Clear message line, display prompt, and save "home" column.
	if(mlputs((isp->flags & Term_Attr) ? MLHome | MLForce | MLTermAttr | MLFlush :
	 MLHome | MLForce | MLFlush,isp->prompt) != Success)
		return rc.status;
	isp->icol0 = mlcol;
	return rc.status;
	}

// Redisplay prompt and current text in input buffer on message line.
static int mli_reset(InpState *isp) {

	if(mli_prompt(isp) != Success)
		return rc.status;
	isp->ibufc = isp->ibufe;
	isp->lshift = 0;
	return !(isp->flags & Term_NoKeyEcho) ? mli_update(isp,true) : rc.status;
	}

// Initialize given InpState object.  Return status.
static int mli_open(InpState *isp,char *prompt,InpChar *ibp,char *defval,uint maxlen,uint flags) {

#if MMDebug & Debug_Temp
fprintf(logfile,"mli_open(...,prompt: \"%s\",defval: \"%s\"): hjump %d, hjumpcols: %d\n",
 prompt,defval,vtc.hjump,vtc.hjumpcols);
#endif
	isp->prompt = prompt;
	isp->maxlen = maxlen;
	isp->ibufz = (isp->ibuf = isp->ibufc = isp->ibufe = ibp) + maxlen;
	isp->flags = flags;
	isp->clen0 = isp->lshift = 0;

	if(mli_prompt(isp) == Success) {
		isp->isize75 = (isp->isize = term.t_ncol - isp->icol0) * 75 / 100;
		if((isp->jumpcols = vtc.hjump * isp->isize / 100) == 0)
			isp->jumpcols = 1;

		// Copy default value to input buffer and display it, if present.
		if(defval != NULL && *defval != '\0')
			(void) mli_puts(defval,isp);
		}

	return rc.status;
	}

// Convert input buffer to a string and save in *datpp.
static int mli_close(InpState *isp,Datum **datpp) {

#if MMDebug & Debug_Temp
fprintf(logfile,"mli_close(): ibufc: +%lu, ibufe: +%lu\n",isp->ibufc - isp->ibuf,isp->ibufe - isp->ibuf);
#endif
	if(dnewtrk(datpp) != 0 || dsalloc(*datpp,(isp->ibufe - isp->ibuf) + 1) != 0)
		return librcset(Failure);
	ibuftos((*datpp)->d_str,isp);
	return rc.status;
	}

// Build a prompt and save the result in "dest" (assumed to be >= current terminal width in size).  Any non-printable characters
// in the prompt string are ignored.  The result is shrunk if necessary to fit in size "prmtPct * current terminal width".  If
// the prompt string begins with a letter, ": " is appended; otherwise, the prompt string is used as is, however any leading
// single (') or double (") quote character is stripped off first and any trailing white space is moved to the very end.  Return
// status.
static int buildprompt(char *dest,char *prmt,int maxpct,char *defval,ushort delim,uint cflags) {
	bool addSpace;
	bool literal = false;
	bool addColon = false;
	char *str,*src,*prmtz;
	int prmtlen,maxlen;
	DStrFab prompt;

	if(dopentrk(&prompt) != 0)
		return librcset(Failure);
	src = strchr(prmt,'\0');

	if(isletter(*prmt))
		addColon = true;
	else if(*prmt == '\'' || *prmt == '"') {
		++prmt;
		literal = true;
		while(src[-1] == ' ')
			--src;
		}

	// Save adjusted prompt, minus any invisible characters.
	for(str = prmt; str < src; ++str)
		if(*str >= ' ' && *str <= '~' && dputc(*str,&prompt) != 0)
			return librcset(Failure);
	addSpace = *prmt != '\0' && src[-1] != ' ';
	if(defval != NULL) {
		if(addSpace) {
			if(dputc(' ',&prompt) != 0)
				return librcset(Failure);
			addSpace = false;
			}
		if(dputc('[',&prompt) != 0 || dputs(defval,&prompt) != 0 || dputc(']',&prompt) != 0)
			return librcset(Failure);
		addColon = true;
		}
	if(delim != RtnKey && delim != AltRtnKey) {
		if(addSpace && dputc(' ',&prompt) != 0)
			return librcset(Failure);
		if(dputc(ektoc(delim,false),&prompt) != 0)
			return librcset(Failure);
		addColon = true;
		}
	if(*src == ' ' && dputs(src,&prompt) != 0)
		return librcset(Failure);

	// Add two characters to make room for ": " (if needed) and close the string-fab object.
	if(dputs("xx",&prompt) != 0 || dclose(&prompt,sf_string) != 0)
		return librcset(Failure);
	prmtz = strchr(prmt = prompt.sf_datp->d_str,'\0');
	prmtz[-2] = '\0';

	// Add ": " if needed.
	if(*src != ' ' && addColon && !literal && prmtz[-3] != ' ')
		strcpy(prmtz - 2,": ");

	// Make sure prompt fits on the screen.
	maxlen = maxpct * term.t_ncol / 100;
	prmtlen = strlen(prmt);
	strfit(dest,maxlen + (cflags & Term_Attr ? attrCount(prmt,prmtlen,maxlen) : 0),prmt,prmtlen);
	return rc.status;
	}

// Compare a string to the message line input buffer and return Boolean result.
static bool ibufcmp(InpState *isp,char *str) {
	InpChar *icp = isp->ibuf;

	// If str is shorter than the input buffer, the trailing null in str won't match, so need need to check for it.
	while(icp < isp->ibufe) {
		if(icp->c != *str++)
			return false;
		++icp;
		}
	return true;
	}

// Compare a string to the message line input buffer, ignoring case, and return Boolean result.
static bool ibufcasecmp(InpState *isp,char *str) {
	InpChar *icp = isp->ibuf;

	// If str is shorter than the input buffer, the trailing null in str won't match, so need need to check for it.
	while(icp < isp->ibufe) {
		if(tolower(icp->c) != tolower(*str++))
			return false;
		++icp;
		}
	return true;
	}

// Attempt a completion on a buffer name, given pointer to InpState object (containing partial name to complete).  Add (and
// display) completed characters to buffer and return rc.status (Success) if unique name matched; otherwise, return NotFound
// (bypassing rcset()).
static int cMatchBuf(InpState *isp) {
	short c;
	Buffer *bufp;			// Trial buffer to complete.
	Buffer *bmatchp;		// Last buffer that matched string.
	bool comflag = false;		// Was there a completion at all?
	uint n = isp->ibufe - isp->ibuf;

	// Start attempting completions, one character at a time, but not more than the maximum buffer size.
	while(isp->ibufe <= isp->ibufz) {

		// Start at the first buffer and scan the list.
		bmatchp = NULL;
		bufp = bheadp;
		do {
			// Is this a match?
			if(n > 0 && !ibufcmp(isp,bufp->b_bname))
				goto NextBuf;

			// A match.  If this is the first match, simply record it.
			if(bmatchp == NULL) {
				bmatchp = bufp;
				c = bufp->b_bname[n];		// Save next char.
				}
			else if(bufp->b_bname[n] != c)

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextBuf:;
			// On to the next buffer.
			} while((bufp = bufp->b_nextp) != NULL);

		// With no match, we are done.
		if(bmatchp == NULL) {

			// Beep if we never matched.
			if(!comflag && TTbeep() == Success)
				(void) TTflush;
			break;
			}

		// If we have completed all the way, return success.
		if(c == '\0' || isp->ibufe == isp->ibufz)
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		isp->ibufc = isp->ibufe;
		if(mli_putc(c,isp,false) != Success || TTflush() != Success)
			return rc.status;
		++n;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a command, alias, or macro name, given pointer to InpState object (containing partial name to
// complete), and selector mask(s).  Add (and display) completed characters to buffer and return rc.status (Success) if unique
// name matched; otherwise, return NotFound (bypassing rcset()).
static int cMatchCFAB(InpState *isp,uint selector) {
	short c;
	CFAMRec *frp;		// CFAM record pointer.
	CFAMRec *ematchp;	// Last entry that matched string.
	bool comflag;		// Was there a completion at all?
	uint n = isp->ibufe - isp->ibuf;

	// Start attempting completions, one character at a time.
	comflag = false;
	for(;;) {
		// Start at the first CFAM entry and scan the list.
		ematchp = NULL;
		frp = frheadp;
		do {
			// Is this a match?
			if(!(frp->fr_type & selector) || (n > 0 && !ibufcmp(isp,frp->fr_name)))
				goto NextEntry;

			// A match.  If this is the first match, simply record it.
			if(ematchp == NULL) {
				ematchp = frp;
				c = frp->fr_name[n];	// Save next char.
				}
			else if(frp->fr_name[n] != c)

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextEntry:;
			// On to the next entry.
			} while((frp = frp->fr_nextp) != NULL);

		// With no match, we are done.
		if(ematchp == NULL) {

			// Beep if we never matched.
			if(!comflag && TTbeep() == Success)
				(void) TTflush;
			break;
			}

		// If we have completed all the way, return success.
		if(c == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		isp->ibufc = isp->ibufe;
		if(mli_putc(c,isp,false) != Success || TTflush() != Success)
			return rc.status;
		++n;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a filename, given pointer to InpState object (containing partial name to complete).  Add (and
// display) completed characters to buffer and return rc.status (Success) if unique name matched; otherwise, return NotFound
// (bypassing rcset()).
static int cMatchFile(InpState *isp) {
	char *fname;			// Trial file to complete.
	uint i;				// Index into strings to compare.
	uint matches;			// Number of matches for name.
	uint longestlen;		// Length of longest match.
	uint n = isp->ibufe - isp->ibuf;
	char longestmatch[MaxPathname + 1];// Temp buffer for longest match.

	// Open the directory.
	ibuftos(longestmatch,isp);
	if(eopendir(longestmatch,&fname) != Success)
		goto Fail;

	// Start at the first file and scan the list.
	matches = 0;
	while(ereaddir() == Success) {

		// Is this a match?
		if(ibufcmp(isp,fname)) {

			// Count the number of matches.
			++matches;

			// If this is the first match, simply record it.
			if(matches == 1) {
				strcpy(longestmatch,fname);
				longestlen = strlen(longestmatch);
				}
			else {
				if(longestmatch[n] != fname[n])

					// A difference, stop here.  Can't auto-extend message line any further.
					goto Fail;

				// Update longestmatch.
				for(i = n + 1; i < longestlen; ++i)
					if(longestmatch[i] != fname[i]) {
						longestlen = i;
						longestmatch[i] = '\0';
						break;
						}
				}
			}
		}

	// Success?
	if(rc.status == Success) {

		// Beep if we never matched.
		if(matches == 0 && TTbeep() == Success)
			(void) TTflush;
		else {
			// The longestmatch buffer contains the longest match, so copy and display it.
			isp->ibufc = isp->ibufe;
			if(mli_puts(longestmatch + n,isp) != Success)
				return rc.status;
			n = isp->ibufe - isp->ibuf;

			// If only one file matched, it was completed unless the last character was a '/', indicating a
			// directory.  In that case we do NOT want to signal a complete match.
			if(matches == 1 && isp->ibufe[-1].c != '/')
				return rc.status;
			}
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a buffer or global mode name, given pointer to InpState object (containing partial name to complete),
// and mode selector.  Add (and display) completed characters to buffer and return rc.status (Success) if unique name matched;
// otherwise, return NotFound (bypassing rcset()).
static int cMatchMode(InpState *isp,uint selector) {
	short c;
	ModeSpec *msp;			// Mode record pointer.
	ModeSpec *ematchp;		// Last entry that matched string.
	bool comflag = false;		// Was there a completion at all?
	uint modetype = (selector & Term_C_GMode) ? MdGlobal : 0;
	uint n = isp->ibufe - isp->ibuf;

	// Start attempting completions, one character at a time.
	for(;;) {
		// Start at the first ModeSpec entry and scan the list.
		ematchp = NULL;
		msp = modeinfo;
		do {
			// Skip if wrong type of mode.
			if((selector & (Term_C_GMode | Term_C_BMode)) != (Term_C_GMode | Term_C_BMode) &&
			 (msp->mask & MdGlobal) != modetype)
				continue;

			// Is this a match?
			if(n > 0 && !ibufcasecmp(isp,msp->mlname))
				goto NextEntry;

			// A match.  If this is the first match, simply record it.
			if(ematchp == NULL) {
				ematchp = msp;
				c = msp->mlname[n];	// Save next char.
				}
			else if(msp->mlname[n] != c)

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextEntry:;
			// On to the next entry.
			} while((++msp)->mlname != NULL);

		// With no match, we are done.
		if(ematchp == NULL) {

			// Beep if we never matched.
			if(!comflag && TTbeep() == Success)
				(void) TTflush;
			break;
			}

		// If we have completed all the way, return success.
		if(c == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		isp->ibufc = isp->ibufe;
		if(mli_putc(c,isp,false) != Success || TTflush() != Success)
			return rc.status;
		++n;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a variable name, given pointer to InpState object (containing partial name to complete), variable
// list vector, and size.  Add (and display) completed characters to buffer and return rc.status (Success) if unique name
// matched; otherwise, return NotFound (bypassing rcset()).
static int cMatchVar(InpState *isp,char *vlistv[],uint vct) {
	short c;
	char **namev;			// Trial name to complete.
	char *match;			// Last name that matched string.
	bool comflag = false;		// Was there a completion at all?
	uint n = isp->ibufe - isp->ibuf;

	// Start attempting completions, one character at a time, but not more than the maximum variable name size.
	while(isp->ibufe <= isp->ibufz) {

		// Start at the first name and scan the list.
		match = NULL;
		namev = vlistv;
		do {
			// Is this a match?
			if(n > 0 && !ibufcmp(isp,*namev))
				goto NextName;

			// A match.  If this is the first match, simply record it.
			if(match == NULL) {
				match = *namev;
				c = match[n];		// Save next char.
				}
			else if((*namev)[n] != c)

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextName:;
			// On to the next name.
			} while(++namev < vlistv + vct);

		// With no match, we are done.
		if(match == NULL) {

			// Beep if we never matched.
			if(!comflag && TTbeep() == Success)
				(void) TTflush;
			break;
			}

		// If we have completed all the way, return success.
		if(c == '\0' || isp->ibufe == isp->ibufz)
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		isp->ibufc = isp->ibufe;
		if(mli_putc(c,isp,false) != Success || TTflush() != Success)
			return rc.status;
		++n;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Make a completion list based on a partial buffer name, given pointer to InpState object (containing partial name), indirect
// work buffer pointer, and "force all" flag.  Return status.
static int cListBuf(InpState *isp,Buffer **bufpp,bool forceall) {
	Buffer *bufp;			// Trial buffer to complete.
	bool havePartial = (isp->ibufe > isp->ibuf);

	// Start at the first buffer and scan the list.
	bufp = bheadp;
	do {
		// Is this a match?  Skip hidden buffers if no partial name.
		if((!havePartial && (forceall || !(bufp->b_flags & BFHidden))) ||
		 (havePartial && ibufcmp(isp,bufp->b_bname)))
			if(bappend(*bufpp,bufp->b_bname) != Success)
				return rc.status;

		// On to the next buffer.
		} while((bufp = bufp->b_nextp) != NULL);

	return bpop(*bufpp,RendShift);
	}

// Make a completion list based on a partial name, given pointer to InpState object (containing partial name), indirect work
// buffer pointer, and selector mask.  Return status.
static int cListCFAB(InpState *isp,Buffer **bufpp,uint selector) {
	CFAMRec *frp;			// Trial name to complete.

	// Start at the first entry and scan the list.
	frp = frheadp;
	do {
		// Add name to the buffer if it's a match...
		if((frp->fr_type & selector) && ibufcmp(isp,frp->fr_name))
			if(bappend(*bufpp,frp->fr_name) != Success)
				return rc.status;

		// and on to the next entry.
		} while((frp = frp->fr_nextp) != NULL);

	return bpop(*bufpp,RendShift);
	}

// Make a completion list based on a partial filename, given pointer to InpState object (containing partial name) and indirect
// work buffer pointer.  Return status.
static int cListFile(InpState *isp,Buffer **bufpp) {
	char *fname;			// Trial file to complete.
	char dirbuf[MaxPathname + 1];

	// Open the directory.
	ibuftos(dirbuf,isp);
	if(eopendir(dirbuf,&fname) != Success)
		return rc.status;

	// Start at the first file and scan the list.
	while(ereaddir() == Success) {

		// Is this a match?
		if(ibufcmp(isp,fname))
			if(bappend(*bufpp,fname) != Success)
				return rc.status;
		}

	// Error?
	if(rc.status != Success)
		return rc.status;

	return bpop(*bufpp,RendShift);
	}

// Make a completion list based on a partial mode name, given pointer to InpState object (containing partial name), indirect
// work buffer pointer, and selector mask.  Return status.
static int cListMode(InpState *isp,Buffer **bufpp,uint selector) {
	ModeSpec *msp;			// Trial name to complete.
	uint modetype = (selector & Term_C_GMode) ? MdGlobal : 0;

	// Start at the first entry and scan the list.
	msp = modeinfo;
	do {
		// Skip if wrong type of mode.
		if((selector & (Term_C_GMode | Term_C_BMode)) != (Term_C_GMode | Term_C_BMode) &&
		 (msp->mask & MdGlobal) != modetype)
			continue;

		// Add name to the buffer if it's a match...
		if(ibufcasecmp(isp,msp->mlname))
			if(bappend(*bufpp,msp->mlname) != Success)
				return rc.status;

		// and on to the next entry.
		} while((++msp)->mlname != NULL);

	return bpop(*bufpp,RendShift);
	}

// Make a completion list based on a partial variable name, given pointer to InpState object (containing partial name),
// indirect work buffer pointer, variable list vector, and size.  Return status.
static int cListVar(InpState *isp,Buffer **bufpp,char *vlistv[],uint vct) {
	char **namev;			// Trial buffer to complete.

	// Start at the first name and scan the list.
	namev = vlistv;
	do {
		// Is this a match?
		if(ibufcmp(isp,*namev))
			if(bappend(*bufpp,*namev) != Success)
				return rc.status;

		// On to the next name.
		} while(++namev < vlistv + vct);

	return bpop(*bufpp,RendShift);
	}

// Get one key from terminal and save in rp.  Return status.
static int terminp1(Datum *rp,char *defval,ushort delim,uint delim2,uint aflags,uint cflags) {
	ushort ek;

	if(((cflags & Term_KeyMask) == Term_OneKeySeq ?
	 getkseq(&ek,NULL) : getkey(&ek)) != Success ||		// Get a key...
	 mlerase(MLForce) != Success)				// and clear the message line.
		return rc.status;
	if((cflags & Term_KeyMask) == Term_OneChar) {
		if(ek == delim || ek == delim2) {		// Terminator?
			if(defval == NULL)			// Yes, return default value or nil.
				dsetnil(rp);
			else
				(void) dsetstr(defval,rp);
			return rc.status;
			}
		if(ek == (Ctrl | ' ')) {			// No, C-SPC?
			if(aflags & CFNotNull1)			// Yes, return error or null string.
				return rcset(Failure,0,text187,text53);
					// "%s cannot be null","Value"
			dsetnull(rp);
			return rc.status;
			}
		if(ek == corekeys[CK_Abort].ek)			// No, user abort?
			return abortinp();			// Yes, return UserAbort status.
		dsetchr(ektoc(ek,false),rp);			// No, save (raw) key and return.
		}
	else {
		char keybuf[16];

		ektos(ek,keybuf,false);
		if(dsetstr(keybuf,rp) != 0)
			return librcset(Failure);
		}

	return rc.status;
	}

// Expand ~, ~username, and $var in input buffer if possible.  Return status.
static int replvar(InpState *isp) {
	char *str;

	if(isp->ibuf->c == '~') {
		if(isp->ibufe == isp->ibuf + 1) {

			// Solo '~'.
			str = "HOME";
			goto ExpVar;
			}
		else {
			// Have ~username... lookup the home directory.
			char dir[MaxPathname + 1];
			ibuftos(dir,isp);
			struct passwd *pwd = getpwnam(dir + 1);
			if(pwd == NULL)
				goto Beep;
			str = pwd->pw_dir;
			}
		}
	else {
		// Expand environmental variable.
		char wkbuf[NTermInp + 1];
		ibuftos(wkbuf,isp);
		str = wkbuf + 1;
ExpVar:
		if((str = getenv(str)) == NULL || strlen(str) > isp->maxlen) {
Beep:
			if(TTbeep() == Success)
				(void) TTflush();
			return rc.status;
			}
		}

	// Expansion successful.  Replace input buffer with new value and add a trailing slash.
	if(mli_trunc(isp,tc_erase) == Success && mli_puts(str,isp) == Success)
		(void) mli_putc('/',isp,true);

	return rc.status;
	}

// Get input from the terminal, possibly with completion, and return in "rp" as a string (or nil) given result pointer, prompt,
// argument flags, control flags, and parameters pointer.  If last argument is NULL, default parameter (TermInp) values are
// used.  Argument flags are: validation flags (CFNotNull1 only).  Control flags are: prompt options and type of completion +
// selector mask for Term_C_CFAM calls.  If completion is requested, the delimiter is forced to be "return"; otherwise, if
// delimiter is "return" or "newline", it's partner is also accepted as a delimiter; otherwise, "return" is converted to
// "newline".  If maxlen is zero, it is set to the maximum.  Return status and value per the following table:
//
//	Input Result		Return Status		Return Value
//	===============		==============		============
//	Terminator only		Success/Failure		Error if CFNil1 and CFNIS1 not set; otherwise, nil.
//	C-SPC			Success/Failure		Error if CFNotNull1; otherwise, null string.
//	Char(s) entered		Success			Input string.
int terminp(Datum *rp,char *prmt,uint aflags,uint cflags,TermInp *tip) {
	ushort delim2;
	TermInp ti = {NULL,RtnKey,0,NULL};
	char prompt[term.t_ncol];

	if(tip != NULL)
		ti = *tip;

	// Error if doing a completion but echo is off.
	if(cflags & Term_C_Mask) {
		if(cflags & Term_NoKeyEcho)
			return rcset(Failure,RCNoFormat,text54);
				// "Key echo required for completion"
		ti.delim = RtnKey;
		}

	// Error if length of default value exceeds maximum input length.
	if(ti.maxlen == 0)
		ti.maxlen = NTermInp;
	if(ti.defval != NULL && !(cflags & Term_NoDef)) {
		size_t deflen = strlen(ti.defval);
		uint max = (cflags & Term_KeyMask) ? 1 : ti.maxlen;
		if(deflen > max)
			return rcset(Failure,0,text110,deflen,max);
				// "Default value length (%lu) exceeds maximum input length (%u)"
		}

	// If delimiter is CR or NL, accept it's partner as a delimiter also.
	delim2 = (ti.delim == RtnKey) ? AltRtnKey : (ti.delim == AltRtnKey) ? RtnKey : 0xFFFF;

	// Build prompt.
	if(buildprompt(prompt,prmt,(cflags & Term_LongPrmt) ? 90 : maxprmt,(cflags & Term_KeyMask) == Term_OneChar ? ti.defval :
	 NULL,ti.delim,cflags) != Success)
		return rc.status;

	// One key?
	if(cflags & Term_KeyMask) {

		// Display prompt and get a key.
		if(mlputs((cflags & Term_Attr) ? MLHome | MLForce | MLTermAttr | MLFlush :
		 MLHome | MLForce | MLFlush,prompt) != Success)
			return rc.status;
		return terminp1(rp,ti.defval,ti.delim,delim2,aflags,cflags);
		}

	// Not a single-key request.  Get multiple characters, with completion if applicable.
	ushort ek;			// Current extended input character.
	int n;
	bool forceall;			// Show hidden buffers?
	bool quotef = false;		// Quoting next character?
	bool popup = false;		// Completion pop-up window being displayed?
	bool refresh = false;		// Update screen before getting next key.
	enum e_termcmd cmd;
	uint vct;			// Variable list size.
	Datum *datp;
	int (*func)(InpState *isp,enum e_termcmd cmd);
	InpState istate;
	InpChar inpbuf[NTermInp + 1];	// Input buffer.
	char *vlist[vct = cflags & (Term_C_Var | Term_C_SVar) ? varct(cflags) : 1];	// Variable name list.

	// Get ready for terminal input.
	if(mli_open(&istate,prompt,inpbuf,(cflags & Term_NoDef) ? NULL : ti.defval,ti.maxlen,
	 cflags & (Term_Attr | Term_NoKeyEcho)) != Success)
		return rc.status;
	if(cflags & (Term_C_Var | Term_C_SVar))
		varlist(vlist,vct,cflags);

	// Get a string from the user with line-editing capabilities (via control characters and arrow keys).  Process each key
	// via mli_xxx() routines, which manage the cursor on the message line and edit characters in the inpbuf array.  Loop
	// until a line delimiter or abort key is entered.
	goto JumpStart;
	for(;;) {
		// Update screen if pop-up window is being displayed (from a completion) and refresh requested.
		if(popup && refresh) {
			if(update(1) != Success || mlrestore() != Success)
				return rc.status;
			popup = false;
			}
		refresh = false;
JumpStart:
		// Get a keystroke and decode it.
		if(getkey(&ek) != Success)
			return rc.status;

		// If the line delimiter or C-SPC was entered, wrap it up.
		if((ek == ti.delim || ek == delim2 || ek == (Ctrl | ' ')) && !quotef) {

			// Clear the message line.
			if(mlerase(MLForce) != Success)
				return rc.status;

			// If C-SPC was entered, return null; otherwise, if nothing was entered, return nil; otherwise, break
			// out of key-entry loop (and return characters in input buffer).
			if(ek == (Ctrl | ' ')) {
				if(aflags & CFNotNull1)
					return rcset(Failure,0,text187,text53);
						// "%s cannot be null","Value"
				dsetnull(rp);
				return rc.status;
				}
			if(istate.ibufe == istate.ibuf) {
				if(!(aflags & (CFNil1 | CFNIS1)))
					return rcset(Failure,0,text214,text53);
						// "%s cannot be nil","Value"
				dsetnil(rp);
				return rc.status;
				}
			break;
			}

#if MMDebug & Debug_Temp
fprintf(logfile,"terminp(): Got key %.4X '%s'\n",(uint) ek,vizc(ek & 0x7F,VBaseDef));
#endif
		// If not in quote mode, check key that was entered.
		if(!quotef) {
			if(ek == corekeys[CK_Abort].ek)			// Abort... bail out.
				return abortinp();
			switch(ek) {
				case Ctrl | '?':			// Rubout/erase... back up.
					cmd = tc_bs;
					goto DelChar;
				case Ctrl | 'D':			// Delete... erase current char.
				case FKey | 'D':
					cmd = tc_del;
DelChar:
					func = mli_del;
					goto Call;
				case Ctrl | 'B':			// ^B or left arrow... move cursor left.
				case FKey | 'B':
					cmd = tc_left;
					goto Nav;
				case Ctrl | 'F':			// ^F or right arrow... move cursor right.
				case FKey | 'F':
					cmd = tc_right;
					goto Nav;
				case Ctrl | 'P':			// ^P, or up arrow... get previous ring entry or
				case FKey | 'P':			// move cursor to BOL.
					if(ti.ringp != NULL) {
						n = 1;
						goto GetRing;
						}
					// Fall through.
				case Ctrl | 'A':			// ^A... move cursor to BOL.
					cmd = tc_bol;
					goto Nav;
				case Ctrl | 'N':			// ^N, or down arrow... get next ring entry or
			 	case FKey | 'N':			// move cursor to EOL.
					if(ti.ringp != NULL) {
						n = -1;
GetRing:
						if(ti.ringp->r_size == 0)
							goto BadKey;
						(void) rcycle(ti.ringp,n,false);	// Can't fail.
						if(mli_trunc(&istate,tc_erase) != Success ||
						 mli_puts(ti.ringp->r_entryp->re_data.d_str,&istate) != Success)
							return rc.status;
						refresh = true;
						continue;
						}
					// Fall through.
				case Ctrl | 'E':			// ^E... move cursor to EOL.
					cmd = tc_eol;
Nav:
					func = mli_nav;
					goto Call;
				case Ctrl | 'K':			// ^K... truncate line and continue.
					cmd = tc_kill;
					goto Trunc;
				case Ctrl | 'U':			// ^U... erase buffer and continue.
					cmd = tc_erase;
Trunc:
					refresh = true;
					func = mli_trunc;
Call:
					if(func(&istate,cmd) != Success)
						return rc.status;
					continue;
				}
			if(ek & FKey)					// Non-arrow function key... ignore it.
				continue;
			if(ek == corekeys[CK_Quote].ek) {		// ^Q... quote next character.
				quotef = true;
				continue;
				}
			if(ek == RtnKey)				// CR... convert to newline.
				ek = AltRtnKey;

			// Check completion special keys.
			else if(cflags & Term_C_Mask) {
				if(ek == (Ctrl | 'I')) {		// Tab... attempt a completion.
					switch(cflags & Term_C_Mask) {
						case Term_C_Buffer:
							n = cMatchBuf(&istate);
							break;
						case Term_C_CFAM:
							n = cMatchCFAB(&istate,cflags);
							break;
						case Term_C_Filename:
							n = cMatchFile(&istate);
							break;
						case Term_C_SVar:
						case Term_C_Var:
							n = cMatchVar(&istate,vlist,vct);
							break;
						default:	// Term_C_BMode and/or Term_C_GMode
							n = cMatchMode(&istate,cflags);
						}
					if(n == Success && !(cflags & Term_C_NoAuto)) {	// Success!
						cpause(fencepause);
						break;
						}
					forceall = false;
					goto CompList;
					}
				if(ek == '/' && (cflags & Term_C_Filename) && istate.ibufc == istate.ibufe) {
					size_t sz = istate.ibufc - istate.ibuf;
					if((sz > 0 && istate.ibuf->c == '~') || (sz > 1 && istate.ibuf->c == '$')) {
						if(replvar(&istate) != Success)
							return rc.status;
						continue;
						}
					}
				if(ek == '?') {
					Buffer *bufp;
					forceall = true;
CompList:
					// Make a completion list and pop it (without key input).
					if(sysbuf(text125,&bufp,0) != Success)
							// "Completions"
						return rc.status;
					switch(cflags & Term_C_Mask) {
						case Term_C_Buffer:
							n = cListBuf(&istate,&bufp,forceall);
							break;
						case Term_C_CFAM:
							n = cListCFAB(&istate,&bufp,cflags);
							break;
						case Term_C_Filename:
							n = cListFile(&istate,&bufp);
							break;
						case Term_C_SVar:
						case Term_C_Var:
							n = cListVar(&istate,&bufp,vlist,vct);
							break;
						default:	// Term_C_BMode and/or Term_C_GMode
							n = cListMode(&istate,&bufp,cflags);
						}
					popup = true;

					// Check status, delete the pop-up buffer, and reprompt the user.
					if(n != Success || bdelete(bufp,0) != Success || mli_reset(&istate) != Success)
						return rc.status;
					continue;
					}
				}
			}

		// Non-special or quoted character.
		quotef = false;

		// If it is a (quoted) function key, ignore it and beep the beeper.
		if(ek & (FKey | Shft)) {
BadKey:
			if(TTbeep() != Success)
				return rc.status;
			goto Flush;
			}

		// Plain character... insert it into the buffer.
		else {
			if(mli_putc(ektoc(ek,false),&istate,true) != Success)
				return rc.status;
			continue;
			}

		// Finished processing one key... flush tty output if echoing.
		if(!(istate.flags & Term_NoKeyEcho))
Flush:
			if(TTflush() != Success)
				return rc.status;
		}

	// Terminal input completed.  Convert input buffer to a string, evaluate result if applicable, and return.
	if(mli_close(&istate,&datp) == Success) {
		if(cflags & Term_Eval)
			(void) execestmt(rp,datp->d_str,TokC_Comment,NULL,NULL);
		else
			datxfer(rp,datp);
		}
	return rc.status;
	}

// Ask a yes or no question on the message line and set *resultp to the answer (true or false).  Used any time a confirmation is
// required.  Return status, including UserAbort, if user pressed the abort key.
int terminpYN(char *prmt,bool *resultp) {
	Datum *datp;
	DStrFab prompt;
	TermInp ti = {"n",RtnKey,0,NULL};


	// Build prompt.
	if(dnewtrk(&datp) != 0 || dopentrk(&prompt) != 0 ||
	 dputs(prmt,&prompt) != 0 || dputs(text162,&prompt) != 0 || dclose(&prompt,sf_string) != 0)
						// " (y/n)?"
		return librcset(Failure);

	// Prompt user, get the response, and check it.
	for(;;) {
		if(terminp(datp,prompt.sf_datp->d_str,0,Term_OneChar,&ti) != Success)
			return rc.status;
		if(*datp->d_str == 'y') {
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

// Get a required numeric argument (via prompt if interactive) and save in rp as an integer.  Return status.  If script mode,
// prmt may be NULL.  If interactive and nothing was entered, rp will be nil.
int getnarg(Datum *rp,char *prmt) {

	if(opflags & OpScript) {
		if(funcarg(rp,Arg_First | CFInt1) != Success)
			return rc.status;
		}
	else if(terminp(rp,prmt,CFNil1,0,NULL) != Success || rp->d_type == dat_nil)
		return rc.status;

	// Convert to integer.
	return toint(rp);
	}

// Get a completion from the user for a command, function, alias, or macro name, given prompt, selector mask, result pointer,
// "not found" error message, and "wrong (alias) type" error message (if applicable; otherwise, NULL).  emsg1 may contain
// terminal attribute specifications.
int getcfam(char *prmt,uint selector,CFABPtr *cfabp,char *emsg0,char *emsg1) {
	Datum *datp;

	if(dnewtrk(&datp) != 0)
		return librcset(Failure);
	if(opflags & OpScript) {
		if(funcarg(datp,Arg_First | CFNotNull1) != Success)
			return rc.status;
		}
	else if(terminp(datp,prmt,CFNotNull1 | CFNil1,Term_C_CFAM | selector,NULL) != Success)
		return rc.status;

	if(datp->d_type != dat_nil) {
		int rcode;

		// Convert the string to a pointer.
		return (rcode = cfabsearch(datp->d_str,cfabp,selector)) == 0 ? rc.status :
		 rcset(Failure,rcode < 0 ? 0 : RCTermAttr,rcode < 0 ? emsg0 : emsg1,datp->d_str);
		}

	cfabp->p_type = PtrNul;
	cfabp->u.p_voidp = NULL;

	return rc.status;
	}

// Get a completion from the user for a buffer name, given result pointer, prompt, default value (which may be NULL), operation
// flags, buffer-return pointer, and "buffer was created" pointer (which also may be NULL).  "createdp" is passed to bfind().
// If interactive and nothing is entered, *bufpp is set to NULL.  Return status.
int bcomplete(Datum *rp,char *prmt,char *defname,uint flags,Buffer **bufpp,bool *createdp) {

	// Get buffer name.
	if(!(opflags & OpScript)) {
		char pbuf[32];
		TermInp ti = {defname,RtnKey,NBufName,NULL};
		sprintf(pbuf,"%s %s",prmt,text83);
				// "buffer"
		if(terminp(rp,pbuf,CFNotNull1 | CFNil1,Term_C_Buffer,&ti) != Success)
			return rc.status;
		if(rp->d_type == dat_nil) {
			*bufpp = NULL;
			goto Retn;
			}
		}
	else if(funcarg(rp,Arg_First | CFNotNull1) != Success)
		return rc.status;

	// Got buffer name.  Find it or create it.
	int status = bfind(rp->d_str,(flags & OpCreate) ? CRBCreate | CRBHook : CRBQuery,0,bufpp,createdp);
	if((flags & OpCreate) || status)
		goto Retn;

	// Buffer not found.  Return NotFound or error.
	if((flags & OpQuery) && !(opflags & OpScript)) {
		(void) mlerase(0);
		return NotFound;
		}
	else
		return rcset(Failure,0,text118,rp->d_str);
			// "No such buffer '%s'"
Retn:
	if(!(opflags & OpScript))
		(void) mlerase(0);
	return rc.status;
	}

// Process "prompt" user function.  Suppress echo if n <= 0; enable terminal attributes in prompt string if n >= 0.  Save result
// in rp and return status.
int uprompt(Datum *rp,int n,Datum **argpp) {
	Datum *prmtp,*defargp;
	uint cflags = 0;			// Completion flags.
	TermInp ti = {NULL,RtnKey,0,NULL};	// Default value, delimiter, maximum input length, Ring pointer.

	if(n != INT_MIN) {
		if(n <= 0)
			cflags |= Term_NoKeyEcho;
		if(n >= 0)
			cflags |= Term_Attr;
		}

	// Get prompt string into prmtp (which may be null or nil).
	if(dnewtrk(&prmtp) != 0)
		return librcset(Failure);
	if(funcarg(prmtp,Arg_First | CFNil1) != Success)
		return rc.status;

	// Have "default" argument?
	if(havesym(s_comma,false)) {

		// Yes, get it and use it unless it's nil.
		if(dnewtrk(&defargp) != 0)
			return librcset(Failure);
		if(funcarg(defargp,CFNIS1) != Success)
			return rc.status;
		if(defargp->d_type != dat_nil) {
			if(tostr(defargp) != Success)
				return rc.status;
			ti.defval = defargp->d_str;
			}

		// Have "type" argument?
		if(havesym(s_comma,false)) {
			char *str;
			bool getAnother = true;		// Get one more (optional) argument?

			// Yes, get it (into rp temporarily) and check it.
			if(funcarg(rp,CFNotNull1) != Success)
				return rc.status;
			str = rp->d_str;
			if(str[0] == '^') {
				cflags |= Term_C_NoAuto;
				++str;
				}
			if(str[0] == '\0' || str[1] != '\0')
				goto PrmtErr;
			switch(*str) {
				case 'c':
					cflags |= Term_OneChar;
					break;
				case 's':
					break;
				case 'o':
					cflags |= Term_OneKey;
					goto OneKey;
				case 'O':
					cflags |= Term_OneKeySeq;
OneKey:
					if(ti.defval != NULL)
						return rcset(Failure,RCNoFormat,text242);
							// "Invalid argument for 'o' or 'O' prompt type"
					break;
				case 'K':
					ti.ringp = &kring;
					break;
				case 'R':
					ti.ringp = &rring;
					break;
				case 'S':
					ti.ringp = &sring;
					break;
				case 'b':
					cflags |= Term_C_Buffer;
					ti.maxlen = NBufName;
					goto PrmtComp;
				case 'f':
					cflags |= Term_C_Filename;
					ti.maxlen = MaxPathname;
					goto PrmtComp;
				case 'v':
					cflags |= Term_C_SVar;
					goto PrmtVar;
				case 'V':
					cflags |= Term_C_Var;
PrmtVar:
					ti.maxlen = NVarName;
PrmtComp:
					// Using completion... no more arguments allowed.
					getAnother = false;
					break;
				default:
PrmtErr:
					return rcset(Failure,0,text295,rp->d_str);
						// "Prompt type '%s' must be one of \"bcfKOoRSsVv\""
				}

			// Have "delimiter" argument?
			if(getAnother && havesym(s_comma,false)) {

				// Yes, get it (into rp temporarily).
				if(funcarg(rp,CFNotNull1) != Success)
					return rc.status;
				if((cflags & Term_KeyMask) >= Term_OneKey)
					return rcset(Failure,RCNoFormat,text242);
						// "Invalid argument for 'o' or 'O' prompt type"
				if(stoek(rp->d_str,&ti.delim) != Success)
					return rc.status;
				if(ti.delim & Prefix) {
					char wkbuf[16];

					return rcset(Failure,RCTermAttr,text341,ektos(ti.delim,wkbuf,true),text342);
						// "Cannot use key sequence ~#u%s~U as %s delimiter","prompt"
					}
				}
			}
		}

	// Prompt for input.
	return terminp(rp,prmtp->d_type == dat_nil ? "" : prmtp->d_str,CFNil1,cflags,&ti);
	}
