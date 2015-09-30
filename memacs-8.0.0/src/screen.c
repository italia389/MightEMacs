// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// screen.c	Screen manipulation commands for MightEMacs.

#include "os.h"
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

#if MMDEBUG & DEBUG_SCRDUMP
// Return line information.
static char *lninfo(Line *lnp) {
	int i;
	char *strp;
	static char wkbuf[32];

	if(lnp == NULL) {
		strp = "(NULL)";
		i = 12;
		}
	else {
		strp = lnp->l_text;
		i = lnp->l_used < 12 ? lnp->l_used : 12;
		}
	sprintf(wkbuf,"%.08x '%.*s'",(uint) lnp,i,strp);

	return wkbuf;
	}

// Write buffer information to log file.
static void dumpbuffer(Buffer *bufp) {
	int i;

	fprintf(logfile,"Buffer '%s' [%.08x]:\n\tb_prevp: %.08x '%s'\n\tb_nextp: %.08x '%s'\n",
	 bufp->b_bname,(uint) bufp,(uint) bufp->b_prevp,bufp->b_prevp == NULL ? "" : bufp->b_prevp->b_bname,
	 (uint) bufp->b_nextp,bufp->b_nextp == NULL ? "" : bufp->b_nextp->b_bname);
	fprintf(logfile,"\tb_face.wf_toplnp: %s\n\tb_hdrlnp: %s\n\tb_ntoplnp: %s\n\tb_nbotlnp: %s\n",
	 lninfo(bufp->b_face.wf_toplnp),lninfo(bufp->b_hdrlnp),lninfo(bufp->b_ntoplnp),lninfo(bufp->b_nbotlnp));

	fprintf(logfile,"\tb_face.wf_dot.lnp: %s\n\tb_face.wf_dot.off: %d\n",
	 lninfo(bufp->b_face.wf_dot.lnp),bufp->b_face.wf_dot.off);
	fputs("\tMarks:\n",logfile);
	i = 0;
	do {
		fprintf(logfile,"\t\t%2d: [%s] %d %hd\n",i,lninfo(bufp->b_face.wf_mark[i].mk_dot.lnp),
		 bufp->b_face.wf_mark[i].mk_dot.off,bufp->b_face.wf_mark[i].mk_force);
		} while(++i < NMARKS);

	fprintf(logfile,"\tb_face.wf_fcol: %d\n\tb_nwind: %hu\n\tb_nexec: %hu\n\tb_nalias: %hu\n\tb_flags: %.04x\n\tb_modes: %.04x\n",
	 bufp->b_face.wf_fcol,bufp->b_nwind,bufp->b_nexec,bufp->b_nalias,(uint) bufp->b_flags,(uint) bufp->b_modes);
	fprintf(logfile,"\tb_inpdelimlen: %d\n\tb_inpdelim: '%s'\n\tb_otpdelim: '%s'\n\tb_fname: '%s'\n",
	 bufp->b_inpdelimlen,bufp->b_inpdelim,bufp->b_otpdelim,bufp->b_fname);
	}

// Write window information to log file.
static void dumpwindow(EWindow *winp,int windnum) {
	int i;

	fprintf(logfile,"\tWindow %d [%.08x]:\n\t\tw_nextp: %.08x\n\t\tw_bufp: %.08x '%s'\n\t\tw_face.wf_toplnp: %s\n",
	 windnum,(uint) winp,(uint) winp->w_nextp,(uint) winp->w_bufp,winp->w_bufp->b_bname,lninfo(winp->w_face.wf_toplnp));
	fprintf(logfile,"\t\tw_face.wf_dot.lnp: %s\n\t\tw_face.wf_dot.off: %d\n\t\tw_toprow: %hu\n\t\tw_nrows: %hu\n\t\tw_force: %hu\n",
	 lninfo(winp->w_face.wf_dot.lnp),winp->w_face.wf_dot.off,winp->w_toprow,winp->w_nrows,winp->w_force);
	fprintf(logfile,"\t\tw_flags: %.04x\n\t\tw_face.wf_fcol: %d\n",
	 (uint) winp->w_flags,winp->w_face.wf_fcol);
#if COLOR
	fprintf(logfile,"\t\tw_face.wf_fcolor: %hu\n\t\tw_bcolor: %hu\n",
	 (uint) winp->w_face.wf_fcolor,(uint) winp->w_bcolor);
#endif
	fputs("\t\tMarks:\n",logfile);
	i = 0;
	do {
		fprintf(logfile,"\t\t\t%2d: [%s] %d %hd\n",i,lninfo(winp->w_face.wf_mark[i].mk_dot.lnp),
		 winp->w_face.wf_mark[i].mk_dot.off,winp->w_face.wf_mark[i].mk_force);
		} while(++i < NMARKS);
	}

// Write screen, window, and buffer information to log file -- for debugging.
void dumpscreens(char *msg) {
	EScreen *scrp;
	EWindow *winp;
	Buffer *bufp;
	int windnum;

	fprintf(logfile,"### %s ###\n\n*SCREENS\n\n",msg);

	// Dump screens and windows.
	scrp = sheadp;
	do {
		fprintf(logfile,"Screen %hu [%.08x]:\n\ts_flags: %.04x\n\ts_nrow: %hu\n\ts_ncol: %hu\n\ts_curwp: %.08x\n",
		 scrp->s_num,(uint) scrp,(uint) scrp->s_flags,scrp->s_nrow,scrp->s_ncol,(uint) scrp->s_curwp);
		winp = scrp->s_wheadp;
		windnum = 0;
		do {
			dumpwindow(winp,++windnum);
			} while((winp = winp->w_nextp) != NULL);
		} while((scrp = scrp->s_nextp) != NULL);

	// Dump buffers.
	fputs("\n*BufferS\n\n",logfile);
	bufp = bheadp;
	do {
		dumpbuffer(bufp);
		} while((bufp = bufp->b_nextp) != NULL);
	}
#endif

// Find a screen, given number, (possibly NULL) pointer to buffer to attach to first window of screen, and (possibly NULL)
// pointer to result.  If the screen is not found and "scr_buf" is not NULL, create new screen and return rc.status; otherwise,
// return false, ignoring spp.
int sfind(ushort scr_num,Buffer *scr_buf,EScreen **spp) {
	EScreen *scrp1;
	ushort snum;
	static char myname[] = "gotoscr";

	// Scan the screen list.  Note that the screen list is empty at program launch.
	for(snum = 0, scrp1 = sheadp; scrp1 != NULL; scrp1 = scrp1->s_nextp) {
		if((snum = scrp1->s_num) == scr_num) {
			if(spp)
				*spp = scrp1;
			return (scr_buf == NULL) ? true : rc.status;
			}
		}

	// No such screen exists, create new one?
	if(scr_buf != NULL) {
		EScreen *scrp2;
		EWindow *winp;		// Pointer to first window of new screen.

		// Allocate memory for screen.
		if((scrp1 = (EScreen *) malloc(sizeof(EScreen))) == NULL)
			return rcset(PANIC,0,text94,myname);
				// "%s(): Out of memory!"

		// Set up screen fields.
		scrp1->s_num = snum + 1;
		scrp1->s_flags = 0;
		scrp1->s_nrow = term.t_nrow;
		scrp1->s_ncol = term.t_ncol;

		// Allocate its first window ...
		if((winp = (EWindow *) malloc(sizeof(EWindow))) == NULL)
			return rcset(PANIC,0,text94,myname);
				// "%s(): Out of memory!"
		scrp1->s_wheadp = scrp1->s_curwp = winp;

		// and setup the window's info.
		winp->w_nextp = NULL;
		winp->w_bufp = scr_buf;
		++scr_buf->b_nwind;
		winp->w_face = scr_buf->b_face;
		winp->w_toprow = 0;
#if COLOR
		// Initalize colors to global defaults.
		winp->w_face.wf_fcolor = gfcolor;
		winp->w_bcolor = gbcolor;
#endif
		winp->w_nrows = term.t_nrow - 2;		// "-2" for message and mode lines.
		winp->w_force = 0;
		winp->w_flags = WFMODE | WFHARD;		// Full refresh.

		// Insert new screen at end of screen list.
		scrp1->s_nextp = NULL;
		if((scrp2 = sheadp) == NULL)
			sheadp = scrp1;
		else {
			while(scrp2->s_nextp != NULL)
				scrp2 = scrp2->s_nextp;
			scrp2->s_nextp = scrp1;
			}

		// and return the new screen pointer.
		if(spp)
			*spp = scrp1;
		return rc.status;
		}

	// Screen not found and scr_buf is NULL.
	return false;
	}

// Switch to given screen.  Return status.
int sswitch(EScreen *scrp) {

	// Nothing to do if it is already current.
	if(scrp == cursp)
		return rc.status;

	// Save the current screen's concept of current window.
	cursp->s_curwp = curwp;
	cursp->s_nrow = term.t_nrow;
	cursp->s_ncol = term.t_ncol;

	// Reset the current screen, window and buffer.
	wheadp = (cursp = scrp)->s_wheadp;
	curbp = (curwp = scrp->s_curwp)->w_bufp;

	// Let the display driver know we need a full screen update.
	opflags |= OPSCREDRAW;
	uphard();

	return rc.status;
	}

// Bring the next screen in the linked screen list to the front and return its number.  Return status.
int nextScreen(Value *rp,int n) {
	EScreen *scrp;
	int nscreens;		// Total number of screens.

	// Check if n is out of range.
	nscreens = scrcount();
	if(n == 0 || (n != INT_MIN && abs(n) > nscreens))
		return rcset(FAILURE,0,text240,n);
				// "No such screen '%d'"
	// Argument given?
	if(n != INT_MIN) {

		// If the argument is negative, it is the nth screen from the end of the screen sequence.
		if(n < 0)
			n = nscreens + n + 1;
		}
	else if((n = cursp->s_num + 1) > nscreens)
		n = 1;

	// Find the screen ...
	scrp = sheadp;
	while(scrp->s_num != n)
		scrp = scrp->s_nextp;

	// return its number ...
	vsetint((long) scrp->s_num,rp);

	// and make it current.
	return sswitch(scrp);
	}

// Create new screen, switch to it, and return its number.  Return status.
int newScreen(Value *rp,int n) {
	EScreen *scrp;

	// Save the current window's settings.
	curbp->b_face = curwp->w_face;

	// Find screen "0" to force-create one and make it current.
	if(sfind(0,curbp,&scrp) != SUCCESS || sswitch(scrp) != SUCCESS)
		return rc.status;
	vsetint((long) scrp->s_num,rp);

	return rcset(SUCCESS,0,text174,scrp->s_num);
			// "Created screen %hu"
	}

// Free all resouces associated with a screen.
static void freescreen(EScreen *scrp) {
	EWindow *winp,*tp;

	// First, free the screen's windows.
	winp = scrp->s_wheadp;
	do {
		--winp->w_bufp->b_nwind;
		winp->w_bufp->b_face = winp->w_face;

		// On to the next window; free this one.
		tp = winp;
		winp = winp->w_nextp;
		free((void *) tp);
		} while(winp != NULL);

	// And now, free the screen itself.
	free((void *) scrp);
	}

// Remove screen from the list and renumber remaining ones.  Update modeline of bottom window if only one left.  Return status.
static int unlistscreen(EScreen *scrp) {
	ushort snum;
	EScreen *tp;

	if(scrp == sheadp)
		sheadp = sheadp->s_nextp;
	else {
		tp = sheadp;
		do {
			if(tp->s_nextp == scrp)
				goto found;
			} while((tp = tp->s_nextp) != NULL);

		// Huh?  Screen not found ... this is a bug.
		return rcset(FATALERROR,0,text177,"unlistscreen",(int) scrp->s_num);
				// "%s(): Screen number %d not found in screen list!"
found:
		tp->s_nextp = scrp->s_nextp;
		}

	// Renumber screens.
	snum = 0;
	tp = sheadp;
	do {
		tp->s_num = ++snum;
		} while((tp = tp->s_nextp) != NULL);

	// If only one screen left, flag mode line at bottom of screen for update.
	if(snum == 1) {

		// Go to last window and flag it.
		wnextis(NULL)->w_flags |= WFMODE;
		}

	return rc.status;
	}

// Delete a screen.  Return status.
int deleteScreen(Value *rp,int n) {
	EScreen *scrp;

	// Get the number of the screen to delete.
	if(n == INT_MIN && getnum(text243,&n) != SUCCESS)
				// "Delete screen"
		return rc.status;

	// Make sure it exists.
	if(!sfind((ushort) n,NULL,&scrp))
		return rcset(FAILURE,0,text240,n);
			// "No such screen '%d'"

	// It can't be current.
	if(scrp == cursp)
		return rcset(FAILURE,0,text241);
			// "Screen is being displayed"

	// Everything's cool ... nuke it.
	if(unlistscreen(scrp) != SUCCESS)
		return rc.status;
	freescreen(scrp);

	return rcset(SUCCESS,0,text178,n);
			// "Deleted screen %d"
	}

// Build and pop up the special buffer containing the list of all screens and their associated buffers (interactive only). 
// Render buffer and return status.
int showScreens(Value *rp,int n) {
	Buffer *slistp;
	EScreen *scrp;			// Pointer to current screen to list.
	EWindow *winp;			// Pointer into current screens window list.
	uint wnum;
	StrList rpt;
	int windcol = 7;
	int filecol = 37;
	char *strp,wkbuf[filecol + 16];

	// Get a buffer for the screen list and open a string list.
	if(sysbuf(text160,&slistp) != SUCCESS)
			// "Screens"
		return rc.status;
	if(vopen(&rpt,NULL,false) != 0)
		return vrcset();

	// Construct the header lines.
	if(vputs(text89,&rpt) != 0 || vputc('\r',&rpt) != 0 ||
	    // "Screen Window      Buffer                File"
	 vputs("------ ------  --------------------  -------------------------------",&rpt) != 0)
		return vrcset();

	// For all screens ...
	scrp = sheadp;
	do {
		// Store the screen number.
		sprintf(wkbuf,"\r%4hu   ",scrp->s_num);
		strp = strchr(wkbuf,'\0');

		// List screen's window numbers and buffer names.
		wnum = 0;
		winp = scrp->s_wheadp;
		do {
			Buffer *bufp = winp->w_bufp;

			// Indent if not first time through.
			if(wnum != 0) {
				wkbuf[0] = '\r';
				strp = wkbuf + 1;
				do {
					*strp++ = ' ';
					} while(strp <= wkbuf + windcol);
				}

			// Store window number, buffer name, and filename.
			sprintf(strp,"%4u   %c%s",++wnum,(bufp->b_flags & BFCHGD) ? '*' : ' ',bufp->b_bname);
			strp = strchr(strp,'\0');
			if(bufp->b_fname != NULL)			// Pad if filename exists.
				do {
					*strp++ = ' ';
					} while(strp <= wkbuf + filecol);
			*strp = '\0';					// Save buffer and add filename.
			if(vputs(wkbuf,&rpt) != 0 ||
			 (bufp->b_fname != NULL && vputs(bufp->b_fname,&rpt) != 0))
				return vrcset();

			// On to the next window.
			} while((winp = winp->w_nextp) != NULL);

		// On to the next screen.
		} while((scrp = scrp->s_nextp) != NULL);

	// Add the results to the buffer.
	if(vclose(&rpt) != 0)
		return vrcset();
	if(bappend(slistp,rpt.sl_vp->v_strp) != SUCCESS)
		return rc.status;

	// Display results.
	return render(rp,n < 0 ? -2 : n,slistp,RENDRESET | (n != INT_MIN && n < -1 ? RENDALTML : 0));
	}

// Get number of screens.  (Mainly for macro use.)
int scrcount(void) {
	EScreen *scrp;
	int count;

	count = 0;
	scrp = sheadp;
	do {
		++count;
		} while((scrp = scrp->s_nextp) != NULL);

	return count;
	}
