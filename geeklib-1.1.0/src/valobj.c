// GeekLib (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// valobj.c - Value object routines.

#include "os.h"
#include "gl_valobj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

// Global variables.
Value *vgarbp = NULL;			// Head of list of temporary value objects, for "garbage collection".

#if VDebug
extern FILE *logfile;

// Dump value object to log file.
void vdump(Value *vp,char *tagp) {
	char *labelp;
	char *nmemb;
	SubStr *ssp;

	fprintf(logfile,"%s\n\taddr: %.8x\n",tagp,(uint) vp);
	fflush(logfile);
	if(vp != NULL)
		switch(vp->v_type) {
			case VALNIL:
				fputs("\tnil\n",logfile);
				break;
			case VALINT:
				fprintf(logfile,"\tint: %ld\n",vp->u.v_int);
				break;
			case VALMINI:
				labelp = "MINI STR";
				goto string;
			case VALSTR:
				labelp = "HEAP STR";
				if(vp->u.v_solop == NULL) {
					nmemb = "u.v_solop";
					goto bad;
					}
string:
				if(vp->v_strp == NULL) {
					nmemb = "v_strp";
bad:
					fprintf(logfile,
					 "*** %s member of value object passed to vdump() is NULL ***\n",nmemb);
					}
				else {
					int c;
					char *strp = vp->v_strp;
					fprintf(logfile,"\t%s (%hu): \"",labelp,vp->v_type);
					while((c = *strp++) != '\0')
						fputs(chlit(c,false),logfile);
					fputs("\"\n",logfile);
					}
				break;
			case VALSLIST:
				fprintf(logfile,"\tSTRING LIST (%hu):\n",vp->v_type);
				for(ssp = vp->u.v_sheadp; ssp != NULL; ssp = ssp->ss_nextp)
					fprintf(logfile,"\t\t\"%s\"\n",ssp->ss_text);
			}
	}
#endif

// Set a value object to a null "mini" string.
#if VDebug
void vinit(Value *vp,char *callerp) {
#else
void vinit(Value *vp) {
#endif
	*(vp->v_strp = vp->u.v_str) = '\0';
	vp->v_type = VALMINI;
#if VDebug
	fprintf(logfile,"vinit(): initialized object %.8x for %s()\n",(uint) vp,callerp);
#endif
	}

// Clear a value object and initialize it via vinit().
void vnull(Value *vp) {

	// Free any string storage.
	if(vp->v_type == VALSLIST) {
		SubStr *ssp0;
		SubStr *ssp1 = vp->u.v_sheadp;

		while(ssp1 != NULL) {
			ssp0 = ssp1->ss_nextp;
#if VDebug
			fprintf(logfile,"vnull(): free(%.8x) SubStr ",(uint) ssp1);
			vdump(vp,"from ...");
			(void) free((void *) ssp1);
			fputs("vnull(): DONE.\n",logfile);
			fflush(logfile);
#else
			(void) free((void *) ssp1);
#endif
			ssp1 = ssp0;
			}
		}
	else if(vp->v_type == VALSTR) {
#if VDebug
		fprintf(logfile,"vnull(): free(%.8x) SOLO ",(uint) vp->v_strp);
		vdump(vp,"from ...");
		(void) free((void *) vp->v_strp);
		fputs("vnull(): DONE.\n",logfile);
		fflush(logfile);
#else
		(void) free((void *) vp->v_strp);
#endif
		}
#if VDebug
	vinit(vp,"vnull");
#else
	vinit(vp);
#endif
	}

// Set a nil value in a value object.
void vnil(Value *vp) {

	vnull(vp);
	vp->v_type = VALNIL;
	vp->v_strp = NULL;
	}

// Set a single-character (string) value in a value object.
void vsetchr(int c,Value *vp) {

	vnull(vp);
	vp->u.v_str[0] = c;
	vp->u.v_str[1] = '\0';
	}

// Set an integer value in a value object.
void vsetint(long i,Value *vp) {

	vnull(vp);
	vp->u.v_int = i;
	vp->v_type = VALINT;
	vp->v_strp = NULL;
	}

// Get space for a string and set *strpp to it.  If a mini string will work, use caller's mini buffer "minibufp"; otherwise, if
// malloc() error, set error message in geekExcep record and return -2, else return 0.
static int salloc(char **strpp,size_t len,char *minibufp) {

	if(minibufp != NULL && len <= sizeof(void *))
		*strpp = minibufp;
	else if((*strpp = (char *) malloc(len)) == NULL)
		return emsge(-2);
	return 0;
	}

// Allocate a string value of given size in a value object.  Return status code.
int vsalloc(Value *vp,size_t len) {

	vnull(vp);
	if(len > sizeof(void *)) {
#if VDebug
		fprintf(logfile,"vsalloc(): getting heap string for %.8x for len %lu ...\n",(uint) vp,len);
#endif
		if(salloc(&vp->u.v_solop,len,NULL) != 0)
			return excep.code;
		*(vp->v_strp = vp->u.v_solop) = '\0';
		vp->v_type = VALSTR;
		}
#if VDebug
	else
		fprintf(logfile,"vsalloc(): retaining MINI string %.8x for len %lu\n",(uint) vp,len);
#endif
	return 0;
	}

// Set a string value currently on the heap in a value object.
void vsethstr(char *strp,Value *vp) {

	vnull(vp);
	vp->v_strp = vp->u.v_solop = strp;
	vp->v_type = VALSTR;
	}

// Set a fixed length string (possible from heap) in a value object.  Assume that the source string may be all or part of the
// dest object.  Return status code.
int vsetfstr(char *strp,size_t len,Value *vp) {
	char *strp0;
	char wkbuf[sizeof(void *)];

	if(salloc(&strp0,len + 1,wkbuf) != 0)	// Get space for string ...
		return excep.code;
	stplcpy(strp0,strp,len + 1);		// and copy string to new buffer.
	if(strp0 == wkbuf) {			// If mini string ...
		vnull(vp);			// clear vp ...
		strcpy(vp->u.v_str,wkbuf);	// and copy it.
		}
	else
		vsethstr(strp0,vp);		// Otherwise, set heap string in vp.
	return 0;
	}

// Set a string value (possibly using heap space) in a value object.  Return status code.
int vsetstr(char *strp,Value *vp) {

	return vsetfstr(strp,strlen(strp),vp);
	}

// Transfer contents of one value object to another.  Return destp.
Value *vxfer(Value *destp,Value *srcp) {
	Value *vp = destp->v_nextp;		// Save the "next" pointer ...
#if VDebug
	static char xmsg[64];
	sprintf(xmsg,"vxfer(): srcp (%.8x) BEFORE ...",(uint) srcp);
	vdump(srcp,xmsg);
	sprintf(xmsg,"vxfer(): destp (%.8x) BEFORE ...",(uint) destp);
	vdump(destp,xmsg);
#endif
	vnull(destp);				// free dest ...
	*destp = *srcp;				// copy the whole burrito ...
	destp->v_nextp = vp;			// restore "next" pointer ...
	if(destp->v_type == VALMINI)		// fix mini-string pointer ...
		destp->v_strp = destp->u.v_str;
#if VDebug
	vinit(srcp,"vxfer");
	vdump(srcp,"vxfer(): srcp AFTER ...");
	vdump(destp,"vxfer(): destp AFTER ...");
#else
	vinit(srcp);				// initialize the source ...
#endif
	return destp;				// and return result.
	}

// Return true if a value object is nil; otherwise, false.
bool visnil(Value *vp) {

	return vp->v_type == VALNIL;
	}

// Return true if a value object is a null string; otherwise, false.
bool visnull(Value *vp) {

	return (vp->v_type & VALSMASK) && *vp->v_strp == '\0';
	}

// Create value object and set *vpp to it.  If perm is true, don't add it to the garbage collection stack.  If malloc() error,
// set error message in geekExcep record and return -2; otherwise, return 0.
#if VTest
int vnew(Value **vpp,bool perm,char *callerp,char *vnamep) {
#else
int vnew(Value **vpp,bool perm) {
#endif
	Value *vp;

#if VDebug && 0
	fputs("vnew(): Dump object list ...\n",logfile);
	for(vp = vgarbp; vp != NULL; vp = vp->v_nextp)
		vdump(vp,"vnew() DUMP");
#endif
	// Get new object ...
	if((vp = (Value *) malloc(sizeof(Value))) == NULL)
		return emsge(-2);
#if VDebug
#if VTest
fprintf(logfile,"vnew(): Allocated %.8x, type %s for %s(), var '%s'\n",(uint) vp,perm ? "PERM" : "Temp",callerp,vnamep);
#else
fprintf(logfile,"vnew(): Allocated %.8x, type %s\n",(uint) vp,perm ? "PERM" : "Temp");
#endif
#endif
	// add it to garbage collection stack (if applicable) ...
	if(perm)
		vp->v_nextp = NULL;
	else {
		vp->v_nextp = vgarbp;
		vgarbp = vp;
		}

	// initialize it ...
#if VDebug
	vinit(vp,"vnew");
#else
	vinit(vp);
#endif

	// and return the spoils.
	*vpp = vp;
	return 0;
	}

// Create string value object.  Shortcut routine for vnew(...,false) followed by vsetstr(...,0).  Return status code.
int vnewstr(Value **vpp,char *strp) {

#if VTest
	return (vnew(vpp,false,"vnewstr","*vpp") != 0) ? excep.code : vsetstr(strp,*vpp);
#else
	return (vnew(vpp,false) != 0) ? excep.code : vsetstr(strp,*vpp);
#endif
	}

// Save a substring (chunk) in a string list.  Return status code.
static int vslsave(Value *vp,char *strp) {
	size_t len = strlen(strp);
	SubStr *ssp;

	// Save string in a new chunk ...
	if((ssp = (SubStr *) malloc(sizeof(SubStr) + len)) == NULL)
		return emsge(-2);
	strcpy(ssp->ss_text,strp);
	ssp->ss_nextp = NULL;

	// and append it to the list in the value object.  Force it to be the first chunk if the substring is internal.
	if(strp == vp->v_strp || vp->u.v_sheadp == NULL)
		vp->u.v_sheadp = ssp;
	else {
		SubStr *ssp1 = vp->u.v_sheadp;
		while(ssp1->ss_nextp != NULL)
			ssp1 = ssp1->ss_nextp;
		ssp1->ss_nextp = ssp;
		}

	return 0;
	}

#if VGet
// Get the next character from a string list object and return it, or null if none left.
int vgetc(StrList *slp) {

	if(slp->sl_strp == slp->sl_strpz) {

		// Any chunks left?
		if(slp->sl_ssp == NULL)
			return '\0';
		slp->sl_strpz = (slp->sl_strp = slp->sl_ssp->ss_text) + strlen(slp->sl_strp);
		slp->sl_ssp = slp->sl_ssp->ss_nextp;
		}

	// Return the next character.
	return *slp->sl_strp++;
	}
#endif

// Put a character to a string list object.  Return status code.
int vputc(int c,StrList *slp) {

#if VTest
	if(c == '\0') {
		*slp->sl_strp = '\0';
		return emsgf(-1,"vputc(): Cannot store a null byte! (in buf '%.16s ...')",slp->sl_buf);
		}
#endif
	if(slp->sl_strp == slp->sl_buf + VALCHUNK - 1) {

		// Save current chunk ...
		*slp->sl_strp = '\0';
		if(vslsave(slp->sl_vp,slp->sl_buf) != 0)
			return excep.code;

		// and reset the string pointer.
		slp->sl_strp = slp->sl_buf;
		}

	// Save the character.
	*slp->sl_strp++ = c;
	return 0;
	}

// "Unput" a character from a string list object.  Guaranteed to always work once, if at least one byte was previously put.
// Return status code, including error if work buffer is empty.
int vunputc(StrList *slp) {

	// Any bytes in work buffer?
	if(slp->sl_strp > slp->sl_buf) {
		--slp->sl_strp;
		return 0;
		}

	// Empty buffer.  Return error.
	return emsg(-1,"vunputc(): No bytes left to \"unput\"");
	}

// Put a string to a string list object.  Return status code.
int vputs(char *strp,StrList *slp) {

	while(*strp != '\0')
		if(vputc(*strp++,slp) != 0)
			return excep.code;
	return 0;
	}

// Put a fixed length string to a string list object.  Return status code.
int vputfs(char *strp,size_t len,StrList *slp) {

	while(*strp != '\0' && len-- > 0)
		if(vputc(*strp++,slp) != 0)
			return excep.code;
	return 0;
	}

// Put a value object to a string list object.  Return status code.
int vputv(Value *vp,StrList *slp) {
	char *strp;

	switch(vp->v_type) {
		case VALNIL:
			break;
		case VALMINI:
		case VALSTR:
			strp = vp->v_strp;
			goto solo;
		case VALINT:
			{char wkbuf[sizeof(long) * 3];

			sprintf(strp = wkbuf,"%ld",vp->u.v_int);
solo:
			if(vputs(strp,slp) != 0)
				return excep.code;
			}
#if VTest
			break;
		default:	// VALSLIST
			return emsg(-1,"vputv(): Cannot put a string list!");
#endif
		}

	return 0;
	}

// Put formatted text to a string list object.  Return status code.
int vputf(StrList *slp,char *fmtp,...) {
	char *strp;
	va_list ap;

	va_start(ap,fmtp);
	(void) vasprintf(&strp,fmtp,ap);
	va_end(ap);
	if(strp == NULL)
		return emsge(-2);

	int rcode = vputs(strp,slp);
	(void) free((void *) strp);
	return rcode;
	}

// Convert a value object to a "solo" string type.  Return status code.
static int vmksolo(Value *vp) {

	switch(vp->v_type) {
		case VALNIL:
			vnull(vp);
			break;
		case VALMINI:
		case VALSTR:
			break;		// Nothing to do.
		case VALINT:
			{char wkbuf[sizeof(long) * 3];

			sprintf(wkbuf,"%ld",vp->u.v_int);
			if(vsetstr(wkbuf,vp) != 0)
				return excep.code;
			}
			break;
		default:	// VALSLIST
			{SubStr *ssp;
			size_t len = 0;
			char *strp0,*strp;
			char wkbuf[sizeof(void *)];

			// Get total length.
			for(ssp = vp->u.v_sheadp; ssp != NULL; ssp = ssp->ss_nextp)
				len += strlen(ssp->ss_text);
#if VTest
			if(len == 0)
				return emsg(-1,"vmksolo(): Zero length string list!");
#endif
			// Get heap space ...
			if(salloc(&strp,len + 1,wkbuf) != 0)
				return excep.code;
			strp0 = strp;

			// copy string pieces into it ...
			for(ssp = vp->u.v_sheadp; ssp != NULL; ssp = ssp->ss_nextp)
				strp = stpcpy(strp,ssp->ss_text);
			*strp = '\0';

			// and set it in value object.
			if(strp0 == wkbuf) {
				vnull(vp);
				strcpy(vp->u.v_str,wkbuf);
				}
			else
				vsethstr(strp0,vp);
			}
		}
	return 0;
	}

// Convert a value object to a string list.  Preserve existing solo string value if keep is true.  Return status code.
static int vslinit(Value *vp,bool keep) {
	int rcode = 0;

	if(keep) {
		rcode = vslsave(vp,vp->v_strp);
		(void) free((void *) vp->v_strp);
		}
	else
		vp->u.v_sheadp = NULL;
	vp->v_strp = NULL;
	vp->v_type = VALSLIST;

	return rcode;
	}

#if VGet
// Prepare for getting or putting a string list, given pointer to list object to initialize, pointer to source or destination
// value object, and flag.  If "put" is true, initialize slp for a put operation; otherwise, a get.  Return status code.
int vopen(StrList *slp,Value *vp,bool put) {

	if(put) {
		// Put operation.
		vnull(vp);
		slp->sl_vp = vp;
		if(vslinit(vp) != 0)
			return excep.code;
		slp->sl_strp = slp->sl_buf;
		}
	else {
		// Get operation.  (Value object's type is assumed to be VALSLIST.)
#if VTest
		if(vp->v_type != VALSLIST)
			return emsgf(-1,"vopen(): \"Get\" value object (type %hu, '%.16s ...') not a list!",vp->v_type,
			 vp->v_strp);
#endif
		slp->sl_vp = vp;
		slp->sl_ssp = vp->u.v_sheadp->ss_nextp;
		slp->sl_strpz = (slp->sl_strp = vp->u.v_sheadp->ss_text) + strlen(slp->sl_strp);
		}
	return 0;
	}
#else
// Prepare for putting to a string list, given pointer to list object to initialize, (possibly NULL) pointer to destination
// value object, and append flag.  If vp is NULL, create a new value object (which can be retrieved by the caller from the slp
// record).  If vp is not NULL and "append" is true, keep existing string value in vp; otherwise, clear it.  Return status code.
#if VTest
int vopen(StrList *slp,Value *vp,bool append,char *callerp,char *vnamep) {
#else
int vopen(StrList *slp,Value *vp,bool append) {
#endif
	char *strp = slp->sl_buf;
	bool app = false;

#if VDebug
	fprintf(logfile,"*vopen(...,%.8x,%s,...): BEGIN\n",(uint) vp,append ? "append" : "clear");
#endif
	if(vp != NULL) {
		if(!append || !(vp->v_type & VALSMASK))
			vnull(vp);

		// Appending.  Do the simple "mini string" case here, but let vslinit() do the "solo" string.
		else if(vp->v_type == VALMINI)
			strp = stpcpy(strp,vp->v_strp);
		else
			app = true;
		}
#if VTest
	else if(vnew(&vp,false,"vopen","vp") != 0)
#else
	else if(vnew(&vp,false) != 0)
#endif
		return excep.code;

	slp->sl_vp = vp;
	slp->sl_strp = strp;
#if VDebug
	if(vslinit(vp,app) != 0)
		return excep.code;
#if VTest
	fprintf(logfile,"vopen(): Initialized SL %.8x for %s(), var '%s': sl_strp %.8x, sl_buf %.8x\n",(uint) slp,callerp,
	 vnamep,(uint) slp->sl_strp,(uint) slp->sl_buf);
#else
	fprintf(logfile,"vopen(): Initialized SL %.8x: sl_strp %.8x, sl_buf %.8x\n",(uint) slp,(uint) slp->sl_strp,
	 (uint) slp->sl_buf);
#endif
	vdump(vp,"vopen(): post-SL-init ...");
	fflush(logfile);
	return 0;
#else
	return vslinit(vp,app);
#endif
	}
#endif

// Return true if a string list is empty; otherwise, false.
bool vempty(StrList *slp) {

	return slp->sl_strp == slp->sl_buf && slp->sl_vp->u.v_sheadp == NULL;
	}

// End a string list "put" operation.  Return status code.
int vclose(StrList *slp) {
	int rcode = 0;

	// If still at starting point (so no bytes saved), change value object to a null string.
	if(vempty(slp))
#if VDebug
		vinit(slp->sl_vp,"vclose");
#else
		vinit(slp->sl_vp);
#endif
	else {
		// At least one byte was saved.  Terminate the string.
		*slp->sl_strp = '\0';

		// Still on first chunk?
		if(slp->sl_vp->u.v_sheadp == NULL) {

			// Yes, save string as a solo.
			rcode = vsetstr(slp->sl_buf,slp->sl_vp);
			}
		else {
			// Not first chunk.  Add last one (which can't be empty) to list ...
			if((rcode = vslsave(slp->sl_vp,slp->sl_buf)) == 0) {

				// and convert to a solo string.
				rcode = vmksolo(slp->sl_vp);
				}
			}
		}
#if VDebug
	vdump(slp->sl_vp,"vclose(): Finalized string ...");
#endif
	return rcode;
	}

#if 0
// Remove a value object from the garbage collection stack.
void vdelist(Value *vp) {

#if VDebug
	vdump(vp,"vdelist(): Searching for ...");
#endif
	if(vp == vgarbp)
		vgarbp = vgarbp->v_nextp;
	else {
		Value *vp1;
		Value *vp2 = vgarbp;
		do {
			vp1 = vp2;
#if VDebug
	if(vp2->v_nextp == NULL) {
		fprintf(logfile,"vdelist() %.8x NOT FOUND!\n",(uint) vp);
		exit(-1);
		}
#endif
			} while((vp2 = vp2->v_nextp) != vp);
		vp1->v_nextp = vp2->v_nextp;
		}
#if VDebug
		fprintf(logfile,"vdelist(): Found it! (vgarbp now %0.8x)\n",(uint) vgarbp);
		fflush(logfile);
#endif
	}
#endif

// Copy one value to another.  The old value is assumed to NOT be a string list.  Return status code.
int vcpy(Value *destp,Value *srcp) {

	switch(srcp->v_type) {
		case VALNIL:
			vnil(destp);
			break;
		case VALINT:
			vsetint(srcp->u.v_int,destp);
			break;
		default:
			return vsetstr(srcp->v_strp,destp);
		}
	return 0;
	}

// Delete given value object.  It assumed that either the object was permanent and not in the garbage collection list, or the
// caller is removing it from the list; for example, when called from vgarbpop().
#if VDebug
void vdelete(Value *vp,char *callerp) {

	vdump(vp,"vdelete(): About to free() ...");
	fprintf(logfile,"vdelete(): calling vnull(%.8x) on object from %s() ...\n",(uint) vp,callerp);
	fflush(logfile);
	vnull(vp);
	fprintf(logfile,"vdelete(): calling free(%.8x) ... ",(uint) vp);
	fflush(logfile);
	(void) free((void *) vp);
	fputs("done.\n",logfile); fflush(logfile);
#else
void vdelete(Value *vp) {

	vnull(vp);
	(void) free((void *) vp);
#endif
	}

// Pop vgarbp to given pointer, releasing heap space and laughing all the way.
void vgarbpop(Value *vp) {
	Value *vp1;

#if VDebug
	vdump(vp,"vgarbpop(): Popping to ...");
	while(vgarbp != vp) {
		if((vp1 = vgarbp) == NULL) {
			fputs("vgarbp is NULL! Bailing out ...\n",logfile);
			break;
			}
		vgarbp = vgarbp->v_nextp;
		vdelete(vp1,"vgarbpop");
		}
	fputs("vgarbpop(): END\n",logfile);
	fflush(logfile);
#else
	while(vgarbp != vp) {
		vp1 = vgarbp;
		vgarbp = vgarbp->v_nextp;
		vdelete(vp1);
		}
#endif
	}

// Copy string from src to dest (an active string list), expanding all invisible characters.  If len > 0, copy a maximum of len
// bytes.  Return status code.
int vstrlit(StrList *destp,char *srcp,size_t len) {
	int c;
	size_t n = 0;

	while((c = *srcp++) != '\0' && (len == 0 || n++ < len))
		if(vputs(chlit(c,false),destp) != 0)
			return excep.code;

	return 0;
	}

// Copy string from src to dest, adding a single quote (') at beginning and end and converting single quote characters to '\''. 
// Return status.
int vshquote(Value *destp,char *srcp) {
	ptrdiff_t len;
	char *srcp0,*strp;
	bool apostrophe;
	StrList dest;

	srcp0 = srcp;
	if(vopen(&dest,destp,false) != 0)
		return excep.code;
	for(;;) {
		if(*srcp == '\0')					// If null ...
			break;						// we're done.
		if(*srcp == '\'') {					// If ' ...
			apostrophe = true;				// convert it.
			len = 0;
			++srcp;
			}
		else {
			apostrophe = false;
			if((strp = strchr(srcp,'\'')) == NULL)		// Search for next ' ...
				strp = strchr(srcp,'\0');		// or null if not found.
			len = strp - srcp;				// Length of next chunk to copy.
			strp = srcp;
			srcp += len;
			}

		if(apostrophe) {					// Copy conversion literal.
			if(vputfs("\\'",2,&dest) != 0)
				return excep.code;
			}						// Copy chunk.
		else if(vputc('\'',&dest) != 0 || vputfs(strp,len,&dest) != 0 || vputc('\'',&dest) != 0)
			return excep.code;
		}
	if(*srcp0 == '\0' && vputfs("''",2,&dest) != 0)			// Wrap it up.
		return excep.code;
	return vclose(&dest);
	}
