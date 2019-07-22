// (c) Copyright 2019 Richard W. Marinelli
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
#include "pllib.h"
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

// "Unget" a key from getkey() for reprocessing in editloop().
void tungetc(ushort ek) {

	kentry.chpending = ek;
	kentry.ispending = true;
	}

// Get one keystroke from the terminal driver, resolve any keyboard macro action, and return it in *key as an extended key.
// The legal prefixes here are the FKey and Ctrl.  If savekey is true, store extended key in kentry.lastkseq.  Return status.
int getkey(bool ml,ushort *key,bool savekey) {
	ushort ek;

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
		if(kmacro.km_slot == kmacro.km_end) {

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
			kmacro.km_slot = kmacro.km_buf;
			}

		// Return the next key.
		ek = *kmacro.km_slot++;
		goto Retn;
		}
Fetch:
	// Otherwise, fetch a character from the terminal driver.
	if(ttgetc(ml,&ek) != Success)
		return rc.status;

	// Save it if we need to...
	if(kmacro.km_state == KMRecord) {

		// Don't overrun the buffer.
		if(kmacro.km_slot == kmacro.km_buf + kmacro.km_size)
			if(growKeyMacro() != Success)
				return rc.status;
		*kmacro.km_slot++ = ek;
		}
Retn:
	// and finally, return the extended key.
	*key = ek;
	if(savekey)
		kentry.lastkseq = ek;
	return rc.status;
	}

// Get a key sequence from the keyboard and return it in *key.  Process all applicable prefix keys.  If kbindp is not NULL, set
// *kbindp to the getbind() return pointer.  If savekey is true, store extended key in kentry.lastkseq.  Return status.
int getkseq(bool ml,ushort *key,KeyBind **kbindp,bool savekey) {
	ushort ek;		// Fetched keystroke.
	ushort p;		// Prefix mask.
	KeyBind *kbind;		// Pointer to a key entry.

	// Get initial key.
	if(getkey(ml,&ek,false) != Success)
		return rc.status;
	kbind = getbind(ek);

	// Get another key if first one was a prefix.
	if(kbind != NULL && kbind->k_targ.p_type == PtrPseudo) {
		switch((enum cfid) (kbind->k_targ.u.p_cfp - cftab)) {
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
				if(getkey(ml,&ek,false) != Success)
					return rc.status;
				ek |= p;				// Add prefix.
				if(kbindp != NULL) {
					kbind = getbind(ek);		// Get new binding if requested.
					goto Retn;
					}
				break;
			default:		// Do nothing.
				break;
			}
		}

	if(kbindp != NULL) {
Retn:
		*kbindp = kbind;
		}

	// Return it.
	*key = ek;
	if(savekey)
		kentry.lastkseq = ek;
	return rc.status;
	}

// Do "getKey" function.  Return status.
int getKeyF(Datum *rval,int n,Datum **argv) {
	ushort ek;
	char keybuf[16];
	static bool keyseq;		// Get key sequence; otherwise, one keystroke.
	static bool keylit;		// Return key literal; otherwise, ASCII character (integer).
	static bool doAbort;		// Abort key aborts function; otherwise, processed as any other key.
	static Option options[] = {
		{"^KeySeq",NULL,0,.u.ptr = (void *) &keyseq},
		{"^Char",NULL,OptFalse,.u.ptr = (void *) &keylit},
		{"No^Abort",NULL,OptFalse,.u.ptr = (void *) &doAbort},
		{NULL,NULL,0,0}};
	static OptHdr ohdr = {
		0,text451,false,options};
			// "function option"

	// Get options and set flags.
	initBoolOpts(options);
	if(n != INT_MIN) {
		if(parseopts(&ohdr,NULL,argv[0],NULL) != Success)
			return rc.status;
		setBoolOpts(options);
		}

	// Get key or key sequence and return it.
	if((keyseq ? getkseq(true,&ek,NULL,true) : getkey(true,&ek,true)) != Success)
		return rc.status;
	if(doAbort && ek == corekeys[CK_Abort].ek)
		return abortinp();
	if(!keylit)
		dsetint(ektoc(ek,true),rval);
	else if(dsetstr(ektos(ek,keybuf,false),rval) != 0)
		return librcset(Failure);

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

#if MMDebug & Debug_MsgLine
fprintf(logfile,"mli_write(...,bcol: %d,boff: %hu,%s,\"%s\",ecol: %d): ibufc: '%s'...\n",
 bcol,boff,extChar ? "true" : "false",str,ecol,isp->ibufc == isp->ibufe ? "EOL" : vizc(isp->ibufc[-boff].c,VBaseDef));
#endif
	// Move cursor and write leading '$'.
	if(bcol != term.mlcol && mlmove(bcol) != Success)
		return rc.status;
	if(extChar)
		mlputc(MLRaw,LineExt);

	// Display string.
	if(str == NULL) {
		InpChar *icp = isp->ibufc - boff;
		while(icp < isp->ibufe) {
			mlputc(0,icp->c);
			if(term.mlcol >= term.t_ncol)
				goto Finish;
			++icp;
			}
		}
	else if(mlputs(MLRaw | MLFlush,extChar ? str + 1 : str) != Success)
		return rc.status;

	if(term.mlcol < term.t_ncol)
		mleeol();
Finish:
	// Reposition cursor if needed.
	if(term.mlcol != ecol)
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

#if MMDebug & Debug_MsgLine
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
#if MMDebug & Debug_MsgLine
fprintf(logfile,"\tMoving cursor to column %d...\n",cpos2 + isp->icol0);
#endif
		// Cursor will still be in visible range... just move it.
		(void) mlmove(cpos2 + isp->icol0);
		}
	else {
		int n;
		char *str;
		char wkbuf[(isp->ibufe - isp->ibuf) * 5 + 2];

		// wkbuf (above) holds space for largest possible display of whole input line; that is, five bytes for each
		// character currently in input buffer (e.g., "<ESC>").  NOTE: Need two extra bytes instead of one for
		// terminating null for some reason -- program crashes otherwise!  Perhaps a bug in stpcpy() called below?

#if MMDebug & Debug_MsgLine
fprintf(logfile,"\tRedisplaying line (isize %d, jumpcols %d, wkbuf %lu)...\n",isp->isize,isp->jumpcols,sizeof(wkbuf));
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
#if MMDebug & Debug_MsgLine
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

#if MMDebug & Debug_MsgLine
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
		if(isp->flags & Term_NoKeyEcho)
			ttbeep();
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
			if(isp->lshift > 0 && term.mlcol - n <= isp->icol0) {
				if(mli_update(isp,false) != Success)
					return rc.status;
				goto Flush;
				}
			if(isp->ibufc == isp->ibufe && (isp->lshift == 0 || isp->clen0 > 0 || term.mlcol - isp->icol0 > 1)) {
#if MMDebug & Debug_MsgLine
fprintf(logfile,"mli_del(): quick delete of %d char(s).\n",n);
#endif
				do {
					mlputc(MLRaw,'\b');
					mlputc(MLRaw,' ');
					mlputc(MLRaw,'\b');
					} while(--n > 0);
				goto Flush;
				}
			n = term.mlcol - n;
			}
		else {
			if(isp->ibufc == isp->ibufe && n == 1 &&
			 (isp->lshift == 0 || isp->clen0 > 0 || term.mlcol - isp->icol0 > 1)) {
				mlputc(MLRaw,' ');
				mlputc(MLRaw,'\b');
				goto Flush;
				}
			n = term.mlcol;
			}

		// Not simple adjustment or special case... redo-from-cursor needed.
		if(mli_write(isp,n,0,n == isp->icol0 && isp->lshift > 0,NULL,n) == Success)
Flush:
			(void) mlflush();
		}
	return rc.status;
	}

// Insert a character into the message line input buffer.  If echoing and "update" is true, update virtual message line also.
static int mli_putc(short c,InpState *isp,bool update) {

	if(isp->ibufe < isp->ibufz && c < 0x7F) {
#if MMDebug & Debug_MsgLine
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
			if(update) {
				if(term.mlcol + len < term.t_ncol - 1)
					(void) mli_write(isp,term.mlcol,1,false,NULL,term.mlcol + len);
				else
					(void) mli_update(isp,false);
				}
			}
		}
	else
		ttbeep();

	return rc.status;
	}

// Insert a (possibly null) string into the message line input buffer.  Called only when "echo" is enabled.
static int mli_puts(char *str,InpState *isp) {
	int max = isp->ibufz - isp->ibufc + 1;		// Space left in input buffer + 1.

	while(*str != '\0' && max-- > 0)
		if(mli_putc(*str++,isp,false) != Success)
			return rc.status;
	return mli_update(isp,true);
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
				if(isp->flags & Term_NoKeyEcho)
					ttbeep();
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
		(void) mlflush();
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
			if(mlmove(isp->icol0) == Success) {
Trunc:
				mleeol();
				(void) mlflush();
				}
			}
		}
	return rc.status;
	}

// Erase message line, display prompt, and update "icol0" in InpState object.  Return status.
static int mli_prompt(InpState *isp) {

	// Clear message line, display prompt, and save "home" column.
	if(mlputs((isp->flags & Term_Attr) ? MLHome | MLTermAttr | MLFlush : MLHome | MLFlush,isp->prompt) != Success)
		return rc.status;
	isp->icol0 = term.mlcol;
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

#if MMDebug & Debug_MsgLine
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

// Convert input buffer to a string and save in *datump.
static int mli_close(InpState *isp,Datum **datump) {

#if MMDebug & Debug_MsgLine
fprintf(logfile,"mli_close(): ibufc: +%lu, ibufe: +%lu\n",isp->ibufc - isp->ibuf,isp->ibufe - isp->ibuf);
#endif
	if(dnewtrk(datump) != 0 || dsalloc(*datump,(isp->ibufe - isp->ibuf) + 1) != 0)
		return librcset(Failure);
	ibuftos((*datump)->d_str,isp);
	return rc.status;
	}

// Build a prompt and save the result in "dest" (assumed to be >= current terminal width in size).  Any non-printable characters
// in the prompt string are ignored.  The result is shrunk if necessary to fit in size "prmtPct * current terminal width".  If
// the prompt string begins with a letter, ": " is appended; otherwise, the prompt string is used as is, however any leading
// single (') or double (") quote character is stripped off first and any trailing white space is moved to the very end.  Return
// status.
static int buildprompt(char *dest,char *prmt,int maxpct,short defval,ushort delim,uint cflags) {
	bool addSpace;
	bool literal = false;
	bool addColon = false;
	char *str,*src,*prmtz;
	int prmtlen,maxlen;
	DStrFab prompt;

	if(dopentrk(&prompt) != 0)
		goto LibFail;
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
			goto LibFail;
	addSpace = *prmt != '\0' && src[-1] != ' ';
	if(defval >= 0) {
		if(addSpace) {
			if(dputc(' ',&prompt) != 0)
				goto LibFail;
			addSpace = false;
			}
		if(dputc('[',&prompt) != 0 || dputs(vizc(defval,VBaseDef),&prompt) != 0 || dputc(']',&prompt) != 0)
			goto LibFail;
		addColon = true;
		}
	if(delim != RtnKey && delim != AltRtnKey) {
		if(addSpace && dputc(' ',&prompt) != 0)
			goto LibFail;
		if(dputc(ektoc(delim,false),&prompt) != 0)
			goto LibFail;
		addColon = true;
		}
	if(*src == ' ' && dputs(src,&prompt) != 0)
		goto LibFail;

	// Add two characters to make room for ": " (if needed) and close the string-fab object.
	if(dputs("xx",&prompt) != 0 || dclose(&prompt,sf_string) != 0)
		goto LibFail;
	prmtz = strchr(prmt = prompt.sf_datum->d_str,'\0');
	prmtz[-2] = '\0';

	// Add ": " if needed.
	if(*src != ' ' && addColon && !literal && prmtz[-3] != ' ')
		strcpy(prmtz - 2,": ");

	// Make sure prompt fits on the screen.
	maxlen = maxpct * term.t_ncol / 100;
	prmtlen = strlen(prmt);
	strfit(dest,maxlen + (cflags & Term_Attr ? attrCount(prmt,prmtlen,maxlen) : 0),prmt,prmtlen);
	return rc.status;
LibFail:
	return librcset(Failure);
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
	Datum *el;
	Array *ary;
	Buffer *buf;			// Trial buffer to complete.
	Buffer *bmatch;		// Last buffer that matched string.
	bool comflag = false;		// Was there a completion at all?
	uint n = isp->ibufe - isp->ibuf;

	// Start attempting completions, one character at a time, but not more than the maximum buffer size.
	while(isp->ibufe <= isp->ibufz) {

		// Start at the first buffer and scan the list.
		bmatch = NULL;
		ary = &buftab;
		while((el = aeach(&ary)) != NULL) {
			buf = bufptr(el);

			// Is this a match?
			if(n > 0 && !ibufcmp(isp,buf->b_bname))
				continue;

			// A match.  If this is the first match, simply record it.
			if(bmatch == NULL) {
				bmatch = buf;
				c = buf->b_bname[n];		// Save next char.
				}
			else if(buf->b_bname[n] != c)

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
			}

		// With no match, we are done.
		if(bmatch == NULL) {

			// Beep if we never matched.
			if(!comflag)
				ttbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(c == '\0' || isp->ibufe == isp->ibufz)
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		isp->ibufc = isp->ibufe;
		if(mli_putc(c,isp,true) != Success || mlflush() != Success)
			return rc.status;
		++n;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Attempt a completion on a command, alias, or macro name, given pointer to InpState object (containing partial name to
// complete), selector mask(s), and sorted array of exectab entries.  Add (and display) completed characters to buffer and
// return rc.status (Success) if unique name matched; otherwise, return NotFound (bypassing rcset()).
static int cMatchCFAB(InpState *isp,uint selector,HashRec **hrecp) {
	short c;
	HashRec **hrecp1;
	UnivPtr *univp;
	UnivPtr *ematch;	// Last entry that matched string.
	bool comflag;		// Was there a completion at all?
	uint n = isp->ibufe - isp->ibuf;
	HashRec **hrecpz = hrecp + exectab->recCount;

	// Start attempting completions, one character at a time.
	comflag = false;
	for(;;) {
		// Start at the first CFAM entry and scan the list.
		ematch = NULL;
		hrecp1 = hrecp;
		do {
			univp = univptr(*hrecp1);

			// Is this a match?
			if(!(univp->p_type & selector) || (n > 0 && !ibufcmp(isp,(*hrecp1)->key)))
				goto NextEntry;

			// A match.  If this is the first match, simply record it.
			if(ematch == NULL) {
				ematch = univp;
				c = (*hrecp1)->key[n];	// Save next char.
				}
			else if((*hrecp1)->key[n] != c)

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextEntry:;
			// On to the next entry.
			} while(++hrecp1 < hrecpz);

		// With no match, we are done.
		if(ematch == NULL) {

			// Beep if we never matched.
			if(!comflag)
				ttbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(c == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		isp->ibufc = isp->ibufe;
		if(mli_putc(c,isp,true) != Success || mlflush() != Success)
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
		if(matches == 0)
			ttbeep();
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
	ModeSpec *mspec;			// Mode record pointer.
	ModeSpec *ematch;		// Last entry that matched string.
	bool comflag = false;		// Was there a completion at all?
	Datum *el;
	Array *ary;
	ushort modetype = (selector & Term_C_GMode) ? MdGlobal : 0;
	uint n = isp->ibufe - isp->ibuf;

	// Start attempting completions, one character at a time.
	for(;;) {
		// Start at the first ModeSpec entry and scan the list.
		ematch = NULL;
		ary = &mi.modetab;
		while((el = aeach(&ary)) != NULL) {
			mspec = msptr(el);

			// Skip if wrong type of mode.
			if((selector & (Term_C_GMode | Term_C_BMode)) != (Term_C_GMode | Term_C_BMode) &&
			 (mspec->ms_flags & MdGlobal) != modetype)
				continue;

			// Is this a match?
			if(n > 0 && !ibufcasecmp(isp,mspec->ms_name))
				goto NextEntry;

			// A match.  If this is the first match, simply record it.
			if(ematch == NULL) {
				ematch = mspec;
				c = mspec->ms_name[n];	// Save next char.
				}
			else if(mspec->ms_name[n] != c)

				// A difference, stop here.  Can't auto-extend message line any further.
				goto Fail;
NextEntry:;
			// On to the next entry.
			}

		// With no match, we are done.
		if(ematch == NULL) {

			// Beep if we never matched.
			if(!comflag)
				ttbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(c == '\0')
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		isp->ibufc = isp->ibufe;
		if(mli_putc(c,isp,true) != Success || mlflush() != Success)
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
			if(!comflag)
				ttbeep();
			break;
			}

		// If we have completed all the way, return success.
		if(c == '\0' || isp->ibufe == isp->ibufz)
			return rc.status;

		// Remember we matched, and complete one character.
		comflag = true;
		isp->ibufc = isp->ibufe;
		if(mli_putc(c,isp,true) != Success || mlflush() != Success)
			return rc.status;
		++n;
		}
Fail:
	// Match not found.
	return NotFound;
	}

// Make a completion list based on a partial buffer name, given pointer to InpState object (containing partial name), indirect
// work buffer pointer, and "force all" flag.  Return status.
static int cListBuf(InpState *isp,Buffer **bufp,bool forceall) {
	Datum *el;
	Array *ary;
	Buffer *buf;			// Trial buffer to complete.
	bool havePartial = (isp->ibufe > isp->ibuf);

	// Start at the first buffer and scan the list.
	ary = &buftab;
	while((el = aeach(&ary)) != NULL) {
		buf = bufptr(el);

		// Is this a match?  Skip hidden buffers if no partial name.
		if((!havePartial && (forceall || !(buf->b_flags & BFHidden))) ||
		 (havePartial && ibufcmp(isp,buf->b_bname)))
			if(bappend(*bufp,buf->b_bname) != Success)
				return rc.status;
		}

	return bpop(*bufp,RendShift);
	}

// Make a completion list based on a partial name, given pointer to InpState object (containing partial name), indirect work
// buffer pointer, and selector mask.  Return status.
static int cListCFAB(InpState *isp,Buffer **bufp,uint selector,HashRec **hrecp) {
	UnivPtr *univp;
	HashRec **hrecpz = hrecp + exectab->recCount;

	// Start at the first entry and scan the list.
	do {
		univp = univptr(*hrecp);

		// Add name to the buffer if it's a match...
		if((univp->p_type & selector) && ibufcmp(isp,(*hrecp)->key))
			if(bappend(*bufp,(*hrecp)->key) != Success)
				return rc.status;

		// and on to the next entry.
		} while(++hrecp < hrecpz);

	return bpop(*bufp,RendShift);
	}

// Compare two filenames (for qsort()).
static int fncmp(const void *fnp1,const void *fnp2) {

	return strcmp(*((char **) fnp1),*((char **) fnp2));
	}

// Make a completion list based on a partial filename, given pointer to InpState object (containing partial name) and indirect
// work buffer pointer.  Return status.
static int cListFile(InpState *isp,Buffer **bufp) {
	char *fname,*str;
	char **fnlist = NULL;		// Array of matching filenames (to be sorted after scan).
	char **fnl;
	uint fnct = 0;
	uint fnsize = 32;
	char dirbuf[MaxPathname + 1];
	static char myname[] = "cListFile";

	// Open the directory.
	ibuftos(dirbuf,isp);
	if(eopendir(dirbuf,&fname) != Success)
		return rc.status;

	// Start at the first file and scan the list.
	while(ereaddir() == Success) {

		// Is this a match?
		if(ibufcmp(isp,fname)) {
			if(fnlist == NULL || fnct == fnsize) {
				fnsize *= 2;
				if((fnlist = (char **) realloc(fnlist,sizeof(char *) * fnsize)) == NULL)
					return rcset(Panic,0,text94,myname);
						// "%s(): Out of memory!"
				fnl = fnlist + fnct;
				}

			// Get space for filename and store in array.
			if((str = (char *) malloc(strlen(fname) + 1)) == NULL)
				return rcset(Panic,0,text94,myname);
					// "%s(): Out of memory!"
			strcpy(str,fname);
			*fnl++ = str;
			++fnct;
			}
		}

	// Error?
	if(rc.status != Success)
		return rc.status;

	// Have all matching filenames.  Sort the list if two or more.
	if(fnct > 1)
		qsort((void *) fnlist,fnct,sizeof(char *),fncmp);

	// Copy sorted list to completion buffer and free the array.
	if(fnct > 0) {
		char **fnlz = (fnl = fnlist) + fnct;
		do {
			if(bappend(*bufp,*fnl) != Success)
				return rc.status;
			free((void *) *fnl++);
			} while(fnl < fnlz);
		}
	free((void *) fnlist);

	return bpop(*bufp,RendShift);
	}

// Make a completion list based on a partial mode name, given pointer to InpState object (containing partial name), indirect
// work buffer pointer, and selector mask.  Return status.
static int cListMode(InpState *isp,Buffer **bufp,uint selector) {
	ModeSpec *mspec;
	Datum *el;
	Array *ary = &mi.modetab;
	ushort modetype = (selector & Term_C_GMode) ? MdGlobal : 0;

	// Start at the first entry and scan the list.
	while((el = aeach(&ary)) != NULL) {
		mspec = msptr(el);

		// Skip if wrong type of mode.
		if((selector & (Term_C_GMode | Term_C_BMode)) != (Term_C_GMode | Term_C_BMode) &&
		 (mspec->ms_flags & MdGlobal) != modetype)
			continue;

		// Add name to the buffer if it's a match...
		if(ibufcasecmp(isp,mspec->ms_name))
			if(bappend(*bufp,mspec->ms_name) != Success)
				return rc.status;
		}

	return bpop(*bufp,RendShift);
	}

// Make a completion list based on a partial variable name, given pointer to InpState object (containing partial name),
// indirect work buffer pointer, variable list vector, and size.  Return status.
static int cListVar(InpState *isp,Buffer **bufp,char *vlistv[],uint vct) {
	char **namev;			// Trial buffer to complete.

	// Start at the first name and scan the list.
	namev = vlistv;
	do {
		// Is this a match?
		if(ibufcmp(isp,*namev))
			if(bappend(*bufp,*namev) != Success)
				return rc.status;

		// On to the next name.
		} while(++namev < vlistv + vct);

	return bpop(*bufp,RendShift);
	}

// Get one key from terminal and save in rval as a key literal or character (int).  Return status.
static int terminp1(Datum *rval,char *defval,ushort delim,uint delim2,uint aflags,uint cflags) {
	ushort ek;
	bool quote = false;

	if(((cflags & Term_KeyMask) == Term_OneKeySeq ?
	 getkseq(true,&ek,NULL,true) : getkey(true,&ek,true)) != Success) // Get a key.
		return rc.status;
	if(((cflags & Term_KeyMask) == Term_OneChar) &&			// If C-q...
	 ek == (Ctrl | 'Q')) {
		quote = true;						// get another key.
		if(getkey(true,&ek,true) != Success)
			return rc.status;
	 	}
	if(mlerase() != Success)					// Clear the message line.
		return rc.status;
	if((cflags & Term_KeyMask) == Term_OneChar) {
		if(quote)						// Quoted key?
			goto SetKey;					// Yes, set it.
		if(ek == delim || ek == delim2) {			// Terminator?
			if(defval == NULL)				// Yes, return default value or nil.
				dsetnil(rval);
			else
				dsetint((long) *defval,rval);
			}
		else if(ek == (Ctrl | ' ')) {				// No, C-SPC?
			if(aflags & ArgNotNull1)			// Yes, return error or null character.
				(void) rcset(Failure,0,text187,text53);
					// "%s cannot be null","Value"
			else
				dsetint(0L,rval);
			}
		else if(ek == corekeys[CK_Abort].ek)			// No, user abort?
			(void) abortinp();				// Yes, return UserAbort status.
		else
SetKey:
			dsetint(ektoc(ek,false),rval);			// No, save (raw) key and return.
		}
	else {
		char keybuf[16];

		ektos(ek,keybuf,false);
		if(dsetstr(keybuf,rval) != 0)
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
			ttbeep();
			return rc.status;
			}
		}

	// Expansion successful.  Replace input buffer with new value and add a trailing slash.
	if(mli_trunc(isp,tc_erase) == Success && mli_puts(str,isp) == Success && mli_putc('/',isp,true) == Success)
		(void) mlflush();

	return rc.status;
	}

// Get input from the terminal, possibly with completion, and return in "rval" as a string (or nil) given result pointer,
// prompt, argument flags, control flags, and parameters pointer.  If last argument is NULL, default parameter (TermInp) values
// are used.  Recognized argument flags are ArgNil1, ArgNIS1, and ArgNotNull1.  Control flags are prompt options + type of
// completion + selector mask for Term_C_CFAM calls.  If completion is requested, the delimiter is forced to be "return";
// otherwise, if delimiter is "return" or "newline", it's partner is also accepted as a delimiter; otherwise, "return" is
// converted to "newline".  If maxlen is zero, it is set to the maximum.  Return status and value per the following table:
//
//	Input Result		Return Status		Return Value
//	===============		==============		============
//	Terminator only		Success/Failure		Error if ArgNil1 and ArgNIS1 not set; otherwise, nil.
//	C-SPC			Success/Failure		Error if ArgNotNull1; otherwise, null string or character.
//	Char(s) entered		Success			Input string.
int terminp(Datum *rval,char *prmt,uint aflags,uint cflags,TermInp *tip) {
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
	if(buildprompt(prompt,prmt,(cflags & Term_LongPrmt) ? 90 : term.maxprmt,(cflags & Term_KeyMask) == Term_OneChar &&
	 ti.defval != NULL ? *ti.defval : -1,ti.delim,cflags) != Success)
		return rc.status;

	// One key?
	if(cflags & Term_KeyMask) {

		// Display prompt and get a key.
		if(mlputs((cflags & Term_Attr) ? MLHome | MLTermAttr | MLFlush : MLHome | MLFlush,prompt) != Success)
			return rc.status;
		return terminp1(rval,ti.defval,ti.delim,delim2,aflags,cflags);
		}

	// Not a single-key request.  Get multiple characters, with completion if applicable.
	ushort ek;			// Current extended input character.
	int n;
	bool forceall;			// Show hidden buffers?
	bool quotef = false;		// Quoting next character?
	bool popup = false;		// Completion pop-up window being displayed?
	bool srefresh = false;		// Update screen before getting next key.
	bool cycleRing = false;		// Don't cycle search or replacement ring for first C-p key.
	enum e_termcmd cmd;
	uint vct;			// Variable list size.
	Datum *datum;
	HashRec **hrecp;		// For command completion.
	int (*func)(InpState *isp,enum e_termcmd cmd);
	InpState istate;
	InpChar inpbuf[NTermInp + 1];	// Input buffer.
	char *vlist[vct = cflags & (Term_C_Var | Term_C_SVar) ? varct(cflags) : 1];	// Variable name list.
	HashRec *clist[cflags & Term_C_CFAM ? exectab->recCount : 1];			// Command name list.

	// Get ready for terminal input.
	if(mli_open(&istate,prompt,inpbuf,(cflags & Term_NoDef) ? NULL : ti.defval,ti.maxlen,
	 cflags & (Term_Attr | Term_NoKeyEcho)) != Success)
		return rc.status;
	if(cflags & (Term_C_Var | Term_C_SVar))
		varlist(vlist,vct,cflags);
	else if(cflags & Term_C_CFAM) {
		hrecp = clist;
		if(hsort(exectab,hcmp,&hrecp) != 0)
			return librcset(Failure);
		}

	// Get a string from the user with line-editing capabilities (via control characters and arrow keys).  Process each key
	// via mli_xxx() routines, which manage the cursor on the message line and edit characters in the inpbuf array.  Loop
	// until a line delimiter or abort key is entered.
	goto JumpStart;
	for(;;) {
		// Update screen if pop-up window is being displayed (from a completion) and refresh requested.
		if(popup && srefresh) {
			if(update(1) != Success || mlrestore() != Success)
				return rc.status;
			popup = false;
			}
		srefresh = false;
JumpStart:
		// Get a keystroke and decode it.
		if(getkey(true,&ek,false) != Success)
			return rc.status;

		// If the line delimiter or C-SPC was entered, wrap it up.
		if((ek == ti.delim || ek == delim2 || ek == (Ctrl | ' ')) && !quotef) {

			// Clear the message line.
			if(mlerase() != Success)
				return rc.status;

			// If C-SPC was entered, return null; otherwise, if nothing was entered, return nil; otherwise, break
			// out of key-entry loop (and return characters in input buffer).
			if(ek == (Ctrl | ' ')) {
				if(aflags & ArgNotNull1)
					return rcset(Failure,0,text187,text53);
						// "%s cannot be null","Value"
				dsetnull(rval);
				return rc.status;
				}
			if(istate.ibufe == istate.ibuf) {
				if(!(aflags & (ArgNil1 | ArgNIS1)))
					return rcset(Failure,0,text214,text53);
						// "%s cannot be nil","Value"
				dsetnil(rval);
				return rc.status;
				}
			break;
			}

#if MMDebug & Debug_MsgLine
fprintf(logfile,"terminp(): Got extended key 0x%.4X '%s'\n",(uint) ek,vizc(ek & 0x7F,VBaseDef));
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
					if(ti.ring != NULL) {
						n = 1;
						goto GetRing;
						}
					// Fall through.
				case Ctrl | 'A':			// ^A... move cursor to BOL.
					cmd = tc_bol;
					goto Nav;
				case Ctrl | 'N':			// ^N, or down arrow... get next ring entry or
			 	case FKey | 'N':			// move cursor to EOL.
					if(ti.ring != NULL) {
						n = -1;
GetRing:
						if(ti.ring->r_size == 0)
							goto BadKey;
						if(cycleRing || n != 1)
							(void) rcycle(ti.ring,n,false);	// Can't fail.
						cycleRing = true;
						if(mli_trunc(&istate,tc_erase) != Success ||
						 mli_puts(ti.ring->r_entry->re_data.d_str,&istate) != Success)
							return rc.status;
						srefresh = true;
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
					srefresh = true;
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
							n = cMatchCFAB(&istate,cflags,hrecp);
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
						cpause(si.fencepause);
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
					Buffer *buf;
					forceall = true;
CompList:
					// Make a completion list and pop it (without key input).
					if(sysbuf(text125,&buf,0) != Success)
							// "Completions"
						return rc.status;
					switch(cflags & Term_C_Mask) {
						case Term_C_Buffer:
							n = cListBuf(&istate,&buf,forceall);
							break;
						case Term_C_CFAM:
							n = cListCFAB(&istate,&buf,cflags,hrecp);
							break;
						case Term_C_Filename:
							n = cListFile(&istate,&buf);
							break;
						case Term_C_SVar:
						case Term_C_Var:
							n = cListVar(&istate,&buf,vlist,vct);
							break;
						default:	// Term_C_BMode and/or Term_C_GMode
							n = cListMode(&istate,&buf,cflags);
						}
					popup = true;

					// Check status, delete the pop-up buffer, and reprompt the user.
					if(n != Success || bdelete(buf,0) != Success || mli_reset(&istate) != Success)
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
			ttbeep();
			continue;
			}

		// Plain character... insert it into the buffer.
		else {
			if(mli_putc(ektoc(ek,false),&istate,true) != Success || mlflush() != Success)
				return rc.status;
			continue;
			}

		// Finished processing one key... flush tty output if echoing.
		if(!(istate.flags & Term_NoKeyEcho))
			if(mlflush() != Success)
				return rc.status;
		}

	// Terminal input completed.  Convert input buffer to a string and return.
	if(mli_close(&istate,&datum) == Success)
		datxfer(rval,datum);
	return rc.status;
	}

// Ask a yes or no question on the message line and set *result to the answer (true or false).  Used any time a confirmation is
// required.  Return status, including UserAbort, if user pressed the abort key.
int terminpYN(char *prmt,bool *result) {
	Datum *datum;
	DStrFab prompt;
	TermInp ti = {"n",RtnKey,0,NULL};


	// Build prompt.
	if(dnewtrk(&datum) != 0 || dopentrk(&prompt) != 0 ||
	 dputs(prmt,&prompt) != 0 || dputs(text162,&prompt) != 0 || dclose(&prompt,sf_string) != 0)
						// " (y/n)?"
		return librcset(Failure);

	// Prompt user, get the response, and check it.
	for(;;) {
		if(terminp(datum,prompt.sf_datum->d_str,0,Term_OneChar,&ti) != Success)
			return rc.status;
		if(datum->u.d_int == 'y') {
			*result = true;
			break;
			}
		if(datum->u.d_int == 'n') {
			*result = false;
			break;
			}
		ttbeep();
		if(mlerase() != Success)
			break;
		}
	return rc.status;
	}

// Get a required numeric argument (via prompt if interactive) and save in rval as an integer.  Return status.  If script mode,
// prmt may be NULL.  If interactive and nothing was entered, rval will be nil.
int getnarg(Datum *rval,char *prmt) {

	if(si.opflags & OpScript) {
		if(funcarg(rval,ArgFirst | ArgInt1) != Success)
			return rc.status;
		}
	else if(terminp(rval,prmt,ArgNil1,0,NULL) != Success || rval->d_type == dat_nil)
		return rc.status;

	// Convert to integer.
	return toint(rval);
	}

// Get a completion from the user for a command, function, alias, or macro name, given prompt, selector mask, result pointer,
// and "not found" error message.
int getcfam(char *prmt,uint selector,UnivPtr *univp,char *emsg) {
	Datum *datum;

	if(dnewtrk(&datum) != 0)
		return librcset(Failure);
	if(si.opflags & OpScript) {
		if(funcarg(datum,ArgFirst | ArgNotNull1) != Success)
			return rc.status;
		}
	else if(terminp(datum,prmt,ArgNotNull1 | ArgNil1,Term_C_CFAM | selector,NULL) != Success)
		return rc.status;

	if(datum->d_type != dat_nil) {

		// Get pointer object for name and return status.  Allow all pointer and macro types so that caller can
		// generate correct error message (if any) instead of "<name> does not exist", which may be incorrect.
		if(selector & PtrAlias)
			selector |= PtrAlias;
		if(selector & PtrMacro)
			selector |= PtrMacro;
		return execfind(datum->d_str,OpQuery,selector,univp) ? rc.status : rcset(Failure,0,emsg,datum->d_str);
		}

	univp->p_type = PtrNul;
	univp->u.p_void = NULL;

	return rc.status;
	}

// Get a completion from the user for a buffer name, given result pointer, prompt, default value (which may be NULL), operation
// flags, buffer-return pointer, and "buffer was created" pointer (which also may be NULL).  "created" is passed to bfind().
// If interactive and nothing is entered, *bufp is set to NULL.  Return status.
int bcomplete(Datum *rval,char *prmt,char *defname,uint flags,Buffer **bufp,bool *created) {

	// Get buffer name.
	if(!(si.opflags & OpScript)) {
		char pbuf[32];
		TermInp ti = {defname,RtnKey,MaxBufName,NULL};
		sprintf(pbuf,"%s %s",prmt,text83);
				// "buffer"
		if(terminp(rval,pbuf,ArgNotNull1 | ArgNil1,Term_C_Buffer,&ti) != Success)
			return rc.status;
		if(rval->d_type == dat_nil) {
			*bufp = NULL;
			goto Retn;
			}
		}
	else if(funcarg(rval,ArgFirst | ArgNotNull1) != Success)
		return rc.status;

	// Got buffer name.  Find it or create it.
	int status = bfind(rval->d_str,(flags & OpCreate) ? BS_Create | BS_Hook : BS_Query,0,bufp,created);
	if((flags & OpCreate) || status)
		goto Retn;

	// Buffer not found.  Return NotFound or error.
	if((flags & OpQuery) && !(si.opflags & OpScript)) {
		(void) mlerase();
		return NotFound;
		}
	else
		return rcset(Failure,0,text118,rval->d_str);
			// "No such buffer '%s'"
Retn:
	if(!(si.opflags & OpScript))
		(void) mlerase();
	return rc.status;
	}

// Prompt options.
#define Prm_Type		0
#define Prm_Delim		1
#define Prm_TermAttr		2
#define Prm_NoAuto		3
#define Prm_NoEcho		4

// Prompt option types.
#define Typ_Buffer		0
#define Typ_Char		1
#define Typ_DelRing		2
#define Typ_File		3
#define Typ_Key			4
#define Typ_KeySeq		5
#define Typ_KillRing		6
#define Typ_MutableVar		7
#define Typ_ReplaceRing		8
#define Typ_SearchRing		9
#define Typ_String		10
#define Typ_Var			11

// binsearch() helper function for returning a keyword, given table (array) and index.
static char *keywd(void *table,ssize_t i) {

	return ((char **) table)[i];
	}

// Process "prompt" function.  Save result in rval and return status.
int uprompt(Datum *rval,int n,Datum **argv) {
	Datum *option;				// Unmodified option string (for error reporting).
	Datum *prmt,*defarg = NULL;
	char *str0,*str1,**kwp;
	ssize_t i;
	uint id;
	uint cflagsT = 0;			// Prompt type flag.
	uint cflagsO = 0;			// Other control flags.
	bool delimAllowed = true;		// "Delim" option allowed (not a completion prompt type)?
	TermInp ti = {NULL,RtnKey,0,NULL};	// Default value, delimiter, maximum input length, Ring pointer.
	static char *kwlist[] = {"Type","Delim","TermAttr","NoAuto","NoEcho",NULL};
	static char *types[] = {"Buffer","Char","DelRing","File","Key","KeySeq","KillRing","MutableVar","ReplaceRing",
	 "SearchRing","String","Var"};

	if(dnewtrk(&option) != 0)
		goto LibFail;

	// Set prmt to prompt string (which may be null or nil).
	prmt = argv[0];

	// Get default value, if specified.
	if(n != INT_MIN) {
		if(funcarg(rval,ArgNIS1) != Success)
			return rc.status;
		if(dnewtrk(&defarg) != 0)
			goto LibFail;
		datxfer(defarg,rval);
		}

	// Get options, which override the defaults.
	while(havesym(s_comma,false)) {
		if(funcarg(rval,ArgNotNull1) != Success)
			return rc.status;
		if(datcpy(option,rval) != 0)				// Save original for error reporting.
			goto LibFail;

		// Process an option, which is a string in form "keywd[: value]".
		str0 = rval->d_str;
		str1 = strparse(&str0,':');				// Parse keyword.
		if(str1 == NULL || *str1 == '\0')
			goto ErrRtn;
		kwp = kwlist;						// Valid?
		do {
			if(strcasecmp(str1,*kwp) == 0)
				goto FoundKW;
			} while(*++kwp != NULL);
		goto ErrRtn;						// Nope... unknown.
FoundKW:
		if((id = kwp - kwlist) <= 1) {				// Yes, get option value, if applicable.
			str1 = strparse(&str0,'\0');
			if(str1 == NULL || *str1 == '\0')
				goto ErrRtn;
			}
		switch(id) {						// Process option.
			case Prm_Type:
				if(!binsearch(str1,(void *) types,sizeof(types) / sizeof(char *),strcasecmp,keywd,&i))
					return rcset(Failure,0,text295,str1);
						// "Unknown prompt type '%s'"
				switch(i) {				// Valid type... process it.
					case Typ_Char:
						cflagsT = Term_OneChar;
						break;
					case Typ_String:
						break;
					case Typ_Key:
						cflagsT = Term_OneKey;
						break;
					case Typ_KeySeq:
						cflagsT = Term_OneKeySeq;
						break;
					case Typ_KillRing:
						ti.ring = &kring;
						break;
					case Typ_ReplaceRing:
						ti.ring = &rring;
						break;
					case Typ_SearchRing:
						ti.ring = &sring;
						break;
					case Typ_DelRing:
						ti.ring = &dring;
						break;
					case Typ_Buffer:
						cflagsT = Term_C_Buffer;
						ti.maxlen = MaxBufName;
						goto PrmtComp;
					case Typ_File:
						cflagsT = Term_C_Filename;
						ti.maxlen = MaxPathname;
						goto PrmtComp;
					case Typ_MutableVar:
						cflagsT = Term_C_SVar;
						goto PrmtVar;
					default:	// Typ_Var
						cflagsT = Term_C_Var;
PrmtVar:
						ti.maxlen = MaxVarName;
PrmtComp:
						// Using completion... note it.
						delimAllowed = false;
					}
				break;
			case Prm_Delim:
				if(stoek(str1,&ti.delim) != Success)
					return rc.status;
				if(ti.delim & Prefix) {
					char wkbuf[16];

					return rcset(Failure,RCTermAttr,text341,ektos(ti.delim,wkbuf,true),text342);
						// "Cannot use key sequence ~#u%s~U as %s delimiter","prompt"
					}
				break;
			case Prm_TermAttr:
				cflagsO |= Term_Attr;
				break;
			case Prm_NoAuto:
				cflagsO |= Term_C_NoAuto;
				break;
			default:	// Prm_NoEcho
				cflagsO |= Term_NoKeyEcho;
			}
		}

	// Prepare default value, if any.
	if(defarg != NULL) {
		if(cflagsT == Term_OneChar) {
			if(!charval(defarg))
				return rc.status;
			dsetchr(defarg->u.d_int,defarg);
			}
		else if(tostr(defarg) != Success)
			return rc.status;
		ti.defval = defarg->d_str;
		}

	// Do final validation checks.
	if((ti.delim != RtnKey && !delimAllowed) ||
	 ((cflagsT & Term_KeyMask) >= Term_OneKey && (ti.defval != NULL || ti.delim != RtnKey)))
		return rcset(Failure,RCNoFormat,text242);
			// "Option(s) conflict with prompt type"

	// All is well... prompt for input.
	return terminp(rval,prmt->d_type == dat_nil ? "" : prmt->d_str,ArgNil1,cflagsT | cflagsO,&ti);
ErrRtn:
	return rcset(Failure,0,text447,text457,option->d_str);
		// "Invalid %s '%s'","prompt option"
LibFail:
	return librcset(Failure);
	}
