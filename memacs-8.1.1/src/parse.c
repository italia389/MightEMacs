// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// parse.c	Routines dealing with statement and string parsing for MightEMacs.

#include "os.h"
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include "edef.h"
#include "efunc.h"
#include "elang.h"
#include "edata.h"

typedef struct {
	char *name;			// Keyword.
	enum e_sym s;			// Value.
	} KeywordInfo;

// Local data.
static KeywordInfo kwtab[] = {
	{"true",kw_true},
	{"false",kw_false},
	{"nil",kw_nil},
	{"defn",kw_defn},
	{"and",kw_and},
	{"or",kw_or},
	{"not",kw_not},
#if 0
	{"if",kw_if},
	{"elsif",kw_elsif},
	{"else",kw_else},
	{"endif",kw_endif},
	{"macro",kw_macro},
	{"endmacro",kw_endmacro},
	{"while",kw_while},
	{"until",kw_until},
	{"loop",kw_loop},
	{"endloop",kw_endloop},
	{"return",kw_return},
	{"break",kw_break},
	{"next",kw_next},
	{"force",kw_force},
#endif
	{NULL}};

// Operator table.
typedef struct op {
	struct op *same;		// Character on same level.
	struct op *next;		// Character on next level.
	int ch;				// Character.
	enum e_sym sym;			// Resulting symbol.
	} Op;

static Op optab[] = {
	/* !	0 */	{optab + 3,optab + 1,'!',s_not},
	/* !=	1 */	{optab + 2,NULL,'=',s_ne},
	/* !~	2 */	{NULL,NULL,'=',s_rne},
	/* %	3 */	{optab + 5,optab + 4,'%',s_mod},
	/* %=	4 */	{NULL,NULL,'=',s_asmod},
	/* &	5 */	{optab + 8,optab + 6,'&',s_band},
	/* &&	6 */	{optab + 7,NULL,'&',s_and},
	/* &=	7 */	{NULL,NULL,'=',s_asband},
	/* (	8 */	{optab + 9,NULL,'(',s_lparen},
	/* )	9 */	{optab + 10,NULL,')',s_rparen},
	/* *	10 */	{optab + 12,optab + 11,'*',s_mul},
	/* *=	11 */	{NULL,NULL,'=',s_asmul},
	/* +	12 */	{optab + 15,optab + 13,'+',s_plus},
	/* ++	13 */	{optab + 14,NULL,'+',s_incr},
	/* +=	14 */	{NULL,NULL,'=',s_asadd},
	/* ,	15 */	{optab + 16,NULL,',',s_comma},
	/* -	16 */	{optab + 19,optab + 17,'-',s_minus},
	/* --	17 */	{optab + 18,NULL,'-',s_decr},
	/* -=	18 */	{NULL,NULL,'=',s_assub},
	/* /	19 */	{optab + 21,optab + 20,'/',s_div},
	/* /=	20 */	{NULL,NULL,'=',s_asdiv},
	/* :	21 */	{optab + 22,NULL,':',s_colon},
#if 0
	/* <	22 */	{optab + 27,optab + 23,'<',s_lt},
	/* <<	23 */	{optab + 25,optab + 24,'<',s_lsh},
	/* <<=	24 */	{NULL,NULL,'=',s_aslsh},
	/* <=	25 */	{NULL,optab + 26,'=',s_le},
	/* <=>	26 */	{NULL,NULL,'>',s_cmp},
	/* =	27 */	{optab + 31,optab + 28,'=',s_assign},
	/* ==	28 */	{optab + 29,NULL,'=',s_eq},
	/* =>	29 */	{optab + 30,NULL,'>',s_narg},
	/* =~	30 */	{NULL,NULL,'~',s_req},
	/* >	31 */	{optab + 35,optab + 32,'>',s_gt},
	/* >=	32 */	{optab + 33,NULL,'=',s_ge},
	/* >>	33 */	{NULL,optab + 34,'>',s_rsh},
	/* >>=	34 */	{NULL,NULL,'=',s_asrsh},
	/* ?	35 */	{optab + 36,NULL,'?',s_hook},
	/* ^	36 */	{optab + 38,optab + 37,'^',s_bxor},
	/* ^=	37 */	{NULL,NULL,'=',s_asbxor},
	/* |	38 */	{optab + 41,optab + 39,'|',s_bor},
	/* ||	39 */	{optab + 40,NULL,'|',s_or},
	/* |=	40 */	{NULL,NULL,'=',s_asbor},
	/* ~	41 */	{NULL,NULL,'~',s_bnot},
#else
	/* <	22 */	{optab + 26,optab + 23,'<',s_lt},
	/* <<	23 */	{optab + 25,optab + 24,'<',s_lsh},
	/* <<=	24 */	{NULL,NULL,'=',s_aslsh},
	/* <=	25 */	{NULL,NULL,'=',s_le},
	/* =	26 */	{optab + 30,optab + 27,'=',s_assign},
	/* ==	27 */	{optab + 28,NULL,'=',s_eq},
	/* =>	28 */	{optab + 29,NULL,'>',s_narg},
	/* =~	29 */	{NULL,NULL,'~',s_req},
	/* >	30 */	{optab + 34,optab + 31,'>',s_gt},
	/* >=	31 */	{optab + 32,NULL,'=',s_ge},
	/* >>	32 */	{NULL,optab + 33,'>',s_rsh},
	/* >>=	33 */	{NULL,NULL,'=',s_asrsh},
	/* ?	34 */	{optab + 35,NULL,'?',s_hook},
	/* ^	35 */	{optab + 37,optab + 36,'^',s_bxor},
	/* ^=	36 */	{NULL,NULL,'=',s_asbxor},
	/* |	37 */	{optab + 40,optab + 38,'|',s_bor},
	/* ||	38 */	{optab + 39,NULL,'|',s_or},
	/* |=	39 */	{NULL,NULL,'=',s_asbor},
	/* ~	40 */	{NULL,NULL,'~',s_bnot},
#endif
	};

// Convert ASCII string to long integer, save in *resultp (if resultp not NULL), and return status or boolean result.
// If invalid number, return error if query is false; otherwise, return false.
int asc_long(char *srcp,long *resultp,bool query) {
	long lval;
	char *strp;

	errno = 0;
	lval = strtol(srcp,&strp,0);
	if(errno == ERANGE || *srcp == '\0' || *strp != '\0')
		return query ? false : rcset(FAILURE,0,text38,srcp);
			// "Invalid number '%s'"
	if(resultp != NULL)
		*resultp = lval;
	return query ? true : rc.status;
	}

// Convert long integer to ASCII string and store in destp.  Return destp.
char *long_asc(long n,char *destp) {

	sprintf(destp,"%ld",n);
	return destp;
	}

#if 0
// Convert ASCII string to integer and return status or boolean result.  If invalid number, return error if query is false;
// otherwise, return false.
int asc_int(char *srcp,int *resultp,bool query) {
	long lval;
	char *strp;

	errno = 0;
	lval = strtol(srcp,&strp,0);
	if(errno == ERANGE || *srcp == '\0' || *strp != '\0' || lval < INT_MIN || lval > INT_MAX)
		return query ? false : rcset(FAILURE,0,text38,srcp);
			// "Invalid number '%s'"
	*resultp = (int) lval;
	return query ? true : rc.status;
	}

// Convert integer to ASCII string and store in destp.  Return destp.
char *int_asc(int n,char *destp) {

	sprintf(destp,"%d",n);
	return destp;
	}
#endif

// Set and return proper status from a failed value object function call.
int vrcset(void) {

	return rcset(excep.code == -2 ? PANIC : FATALERROR,0,"%s",excep.msg);
	}

// Set a value object to nil (val_nil) and return status.
int vnilmm(Value *vp) {

	return (vsetstr(val_nil,vp) != 0) ? vrcset() : rc.status;
	}

// Return true if a value object is a true, false, or nil literal (val_xxx); otherwise, false.
bool vistfn(Value *vp,int tfn) {

	return (vp->v_type & VALSMASK) &&
	 strcmp(vp->v_strp,tfn == VNIL ? val_nil : tfn == VFALSE ? val_false : val_true) == 0;
	}

// Convert a value to an integer.  Return status.
int toint(Value *vp) {
	long i;

	if(vp->v_type != VALINT) {
		if(asc_long(vp->v_strp,&i,false) != SUCCESS)
			return rc.status;
		vsetint(i,vp);
		}
	return rc.status;
	}

// Convert a value to a string.  Return status.
int tostr(Value *vp) {

	if(vp->v_type == VALINT) {
		char wkbuf[LONGWIDTH];

		long_asc(vp->u.v_int,wkbuf);
		if(vsetstr(wkbuf,vp) != 0)
			(void) vrcset();
		}
	return rc.status;
	}

// Return true if a value object is an integer; otherwise, set an error and return false.
bool intval(Value *vp) {

	if(vp->v_type == VALINT)
		return true;
	(void) rcset(FAILURE,0,text166,vp->v_strp);
			// "Invalid integer '%s'"
	return false;
	}

// Return true if a value object is a string; otherwise, set an error and return false.
bool strval(Value *vp) {

	if(vp->v_type != VALINT)
		return true;
	(void) rcset(FAILURE,0,text171,vp->u.v_int);
			// "Invalid string (%ld)"
	return false;
	}

// Find first non-whitespace character in given string and return pointer it (which may be the null terminator).
char *nonwhite(char *s) {

	while(*s == ' ' || *s == '\t')
		++s;
	return s;
	}

// Find first whitespace character in given string and return pointer it (which may be the null terminator).
char *white(char *s) {

	while(*s != '\0' && *s != ' ' && *s != '\t')
		++s;
	return s;
	}

// Find a token in a string and return it, given destination pointer, pointer to source pointer, and delimiter character (or
// -1).  If a token is found, set it in dest, update *strpp to point to the next character past the token, and return status;
// otherwise, set dest to a null string, update *strpp to point to the end of the string, and return NOTFOUND (bypassing
// rcset()).
//
// Note: This is a very basic string-splitting routine: (1), the delimiter must be a single character (including null) or -1 for
// white space; (2), if the delimiter is null, each source character is a token; otherwise, if the delimiter is not -1, tokens
// will include any surrounding white space; otherwise, leading and trailing white space in the string is ignored and every
// sequence of one or more tabs or spaces is treated as a single delimiter; and (3), quotes, backslashes, and all characters
// except the delimiter are treated as ordinary characters.
int parsetok(Value *destp,char **srcpp,int delim) {
	int delim1,delim2,c;
	char *srcp0,*srcp;

	// Check if source is null.
	srcp = (delim == -1) ? nonwhite(*srcpp) : *srcpp;
	if(*srcp == '\0')
		return NOTFOUND;

	// Get ready.
	srcp0 = srcp;
	if(delim == -1) {
		delim1 = ' ';
		delim2 = '\t';
		}
	else
		delim1 = delim2 = delim;

	// Get next token.
	while((c = *srcp++) != '\0' && c != delim1 && c != delim2 && delim1 != '\0');

	// Save the token and return results.
	if(vsetfstr(srcp0,srcp - (delim1 == '\0' ? 0 : 1) - srcp0,destp) != 0)
		return vrcset();
#if MMDEBUG & DEBUG_TOKEN
	fprintf(logfile,"parsetok(): Parsed token \"%s\"\n",destp->v_strp);
#endif
	*srcpp = srcp[-1] == '\0' ? srcp - 1 : srcp;

	return rc.status;
	}

// Find the previous token in a string and return it, given destination pointer, pointer to source pointer, pointer to beginning
// of source string, and delimiter character (or -1).  This routine uses the same scheme as "parsetok", but searches in reverse
// instead of forward.  It is assumed that *srcpp will point to the trailing null of basep for the initial call, or the
// (delimiter) character immediately preceding the last token parsed otherwise.
int rparsetok(Value *destp,char **srcpp,char *basep,int delim) {
	int delim1,delim2,c;
	char *srcp,*srcpz;

	// Check if no tokens left or source is null.
	srcp = *srcpp;
	if(delim != -1) {
		if((srcpz = srcp) < basep)
			return NOTFOUND;
		--srcp;
		}
	else {
		--srcp;
		while(srcp >= basep && (*srcp == ' ' || *srcp == '\t'))
			--srcp;
		if(srcp < basep)
			return NOTFOUND;
		srcpz = srcp + 1;
		}

	// We have a token (which may be null).  srcp points to the last character of the next token to parse and srcpz points
	// to the character after that.  Prepare to parse the token.
	if(delim == -1) {
		delim1 = ' ';
		delim2 = '\t';
		}
	else
		delim1 = delim2 = delim;

	// Find beginning of next token.
	while(srcp >= basep && (c = *srcp) != delim1 && c != delim2) {
		--srcp;
		if(delim1 == '\0')
			break;
		}

	// Save the token and return status.
	if(vsetfstr(srcp + 1,srcpz - (srcp + 1),destp) != 0)
		return vrcset();
#if MMDEBUG & DEBUG_TOKEN
	fprintf(logfile,"rparsetok(): Parsed token \"%s\"\n",destp->v_strp);
#endif
	*srcpp = (delim1 == '\0' && srcp >= basep) ? srcp + 1 : srcp;

	return rc.status;
	}

// Initialize "last" global variable for new command line and get first symbol.  Return status.
int parsebegin(Parse *newp,char *clp,int termch) {

	last = newp;
	last->p_clp = clp;
	last->p_termch = termch;
	last->p_sym = s_any;
#if VDebug
	vinit(&last->p_tok,"parsebegin");
#else
	vinit(&last->p_tok);
#endif
	last->p_vgarbp = vgarbp;
#if MMDEBUG & DEBUG_VALUE
fprintf(logfile,"parsebegin(): saved garbage pointer %.8x\n",(uint) vgarbp);
#endif
	(void) getsym();
	return rc.status;
	}

// End a parsing "instance".
void parseend(Parse *oldp) {

	vgarbpop(last->p_vgarbp);
	vnull(&last->p_tok);
	last = oldp;
	}		

// Find end of a string literal or #{} sequence, given indirect pointer to leading character ('), ("), or ({) and terminator
// character.  Set *srcpp to terminator or trailing null and return symbol.
static enum e_sym getslit(char **srcpp,int termch) {
	int c;
	char *srcp = *srcpp + 1;

	// Scan string.  Example: ' "a#{\'b\'}c" xyz'
	while((c = *srcp) != '\0' && c != termch) {
		switch(c) {
			case '\\':
				if(srcp[1] == '\0')
					goto out;
				if(termch != TKC_EXPREND)
					++srcp;
				break;
			case '\'':
			case '"':
				if(termch == TKC_EXPREND) {

					// Found embedded string (within #{}).  Begin new scan.
					(void) getslit(&srcp,c);
					if(*srcp == '\0')
						goto out;
					}
				break;
			case TKC_EXPR:
				if(termch != '"' || srcp[1] != TKC_EXPRBEG)
					break;

				// Found #{.  Begin new scan, looking for }.
				++srcp;
				(void) getslit(&srcp,TKC_EXPREND);
				if(*srcp == '\0')
					goto out;
			}
		++srcp;
		}
out:
	// Return results.
	*srcpp = srcp;
	return (termch == TKC_EXPREND) ? s_nil : s_slit;
	}

// Get a symbol consisting of special characters.  If found, set *srcpp to first invalid character and return symbol; otherwise,
// return s_nil.
static enum e_sym getspecial(char **srcpp) {
	Op *curp,*prevp;
	char *srcp = *srcpp;

	curp = optab;
	prevp = NULL;

	// Loop until find longest match.
	for(;;) {
		// Does current character match?
		if(*srcp == curp->ch) {

			// Advance to next level.
			++srcp;
			prevp = curp;
			if((curp = curp->next) == NULL)
				break;
			}

		// Try next character on same level.
		else if((curp = curp->same) == NULL)
			break;
		}

	*srcpp = srcp;
	return prevp ? prevp->sym : s_nil;
	}

// Check string for a numeric literal.  If extended is true, allow form recognized by strtol() with base zero and no leading
// sign.  If valid literal found, set *srcpp to first invalid character and return symbol; otherwise, return s_nil.
static enum e_sym getnlit(char **srcpp,bool extended) {
	char *srcp = *srcpp;
	if(!isdigit(*srcp++))
		return s_nil;

	if(extended && srcp[-1] == '0' && (*srcp == 'x' || *srcp == 'X')) {
		++srcp;
		while(isdigit(*srcp) || (*srcp >= 'a' && *srcp <= 'f') || (*srcp >= 'A' && *srcp <= 'F'))
			++srcp;
		}
	else
		while(isdigit(*srcp))
			++srcp;
	*srcpp = srcp;

	return s_nlit;
	}

// Check string for an identifier or keyword.  If found, set *srcpp to first invalid character and return symbol; otherwise,
// return s_nil.
enum e_sym getident(char **srcpp) {
	KeywordInfo *kip;
	int len;
	enum e_sym sym;
	char *srcp0 = *srcpp;

	if(!isident1(*srcp0))
		return s_nil;

	// Valid identifier found; find terminator.
	*srcpp += strspn(srcp0,identchars);

	// Query type?
	if(**srcpp == TKC_QUERY) {
		++*srcpp;
		return s_identq;
		}

	// Check if keyword.
	len = *srcpp - srcp0;
	sym = s_ident;
	kip = kwtab;
	do {
		if(memcmp(kip->name,srcp0,len) == 0 && kip->name[len] == '\0') {
			sym = kip->s;
			break;
			}
		} while((++kip)->name != NULL);

	// Return spoils.
	return sym;
	}

// Parse the next symbol in the current command line and update the "last" global variable with the results.  If an invalid
// string is found, return an error; otherwise, if a symbol is found, save it in last->p_tok, set last->p_sym to the symbol
// type, update last->p_clp to point to the next character past the symbol or the terminator character, and return current
// status (SUCCESS); otherwise, set last->p_tok to a null string, update last->p_clp to point to the terminating null if
// last->p_termch is TKC_COMMENT, or the (TKC_EXPREND) terminator otherwise, and return NOTFOUND (bypassing rcset()).
// Notes:
//   *	Symbols are delimited by white space or special characters and quoted strings are saved in last->p_tok in raw form with
//	the leading and trailing quote characters.
//   *	Command line ends at last->p_termch or null terminator.
//   *	The specific symbol type for every token is returned in last->p_sym.
int getsym(void) {
	int c;
	enum e_sym sym = s_nil;
	char *srcp0,*srcp;

	// Get ready.
	vnull(&last->p_tok);

	// Scan past any white space in the source string.
	srcp0 = srcp = nonwhite(last->p_clp);

	// Examine first character.
	if((c = *srcp) != '\0' && c != last->p_termch) {
		switch(c) {
			case '"':
			case '\'':
				sym = getslit(&srcp,c);		// String literal.
				if(*srcp != c)			// Unterminated string?
					return rcset(FAILURE,0,text123,
					 strsamp(srcp0,strlen(srcp0),(size_t) term.t_ncol * 3 / 10));
						// "Unterminated string %s"
				++srcp;
				goto savexit;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				sym = s_nlit;			// Numeric literal.
				(void) getnlit(&srcp,true);
				goto savexit;

			// User variable?
			case TKC_GVAR:
				if(isdigit(srcp[1])) {
					++srcp;
					sym = s_nvar;
					(void) getnlit(&srcp,false);
					goto savexit;
					}
				sym = s_gvar;
				++srcp;
				(void) getident(&srcp);
				if(srcp > srcp0 + 1)
					goto savexit;
				--srcp;
				goto unk;

			// Identifier?
			default:
				if((sym = getident(&srcp)) != s_nil)
					goto savexit;
				if((sym = getspecial(&srcp)) != s_nil) {
savexit:
					c = *srcp;
					*srcp = '\0';
					if(vsetstr(srcp0,&last->p_tok) != 0)
						return vrcset();
					*srcp = c;
					break;
					}
unk:
				// Unknown character.  Return error.
				vsetchr(*srcp,&last->p_tok);
				return rcset(FAILURE,0,text289,last->p_tok.v_strp);
						// "Unexpected token '%s'"
			}
		}
#if 0
	// Check for call at end-of-string (should never happen).
	if(sym == s_nil && last->p_sym == s_nil)
		return rcset(FATALERROR,0,"getsym(): Unexpected end of string, parsing \"%s\"",last->p_tok.v_strp);
#endif
	// Update source pointer and return results.
	last->p_sym = sym;
	last->p_clp = (*srcp == last->p_termch && last->p_termch == TKC_COMMENT) ? strchr(srcp,'\0') : srcp;
#if MMDEBUG & DEBUG_TOKEN
	fprintf(logfile,"### getsym(): Parsed symbol \"%s\" (%d), \"%s\" remains.\n",last->p_tok.v_strp,last->p_sym,last->p_clp);
#endif
	return (sym == s_nil) ? NOTFOUND : rc.status;
	}

// Return true if next character to parse is white space.
bool havewhite(void) {

	return last->p_sym != s_nil && (*last->p_clp == ' ' || *last->p_clp == '\t');
	}

// Check if given symbol (or any symbol if sym is s_any) remains in command line being parsed (in global variable "last").  If
// no symbols are left, set error if "required" is true and return false; otherwise, if "sym" is s_any or the last symbol parsed
// matches "sym", return true; otherwise, set error if "required" is true and return false.
bool havesym(enum e_sym sym,bool required) {

	if(last->p_sym == s_nil) {

		// Nothing left.
		if(required)
			(void) rcset(FAILURE,0,text57);
				// "Argument expected"
		return false;
		}

	// Correct symbol?
	if(sym == s_any || last->p_sym == sym)
		return true;

	// Nope.  Set error if required.
	if(required) {
		if(sym == s_ident || sym == s_identq || sym == s_comma)
			(void) rcset(FAILURE,0,text4,sym == s_comma ? text213 : text68,last->p_tok.v_strp);
					// "%s expected (at token '%s')","Comma","Identifier"
		else
			(void) rcset(FAILURE,0,sym == s_nlit ? text38 : text289,last->p_tok.v_strp);
						// "Invalid number '%s'","Unexpected token '%s'"
		}
	return false;
	}

// Check if next symbol is a comma.  Get next symbol and return true if found; otherwise, set error if "required" is true and
// return false.
bool getcomma(bool required) {

	return havesym(s_comma,required) && (getsym() == SUCCESS || havesym(s_any,true));
	}

// Check if any symbols remain in command line being parsed.  If there are none left, return false; otherwise, set error and
// return true.
bool extrasym(void) {

	if(havesym(s_any,false)) {
		(void) rcset(FAILURE,0,text22,last->p_tok.v_strp);
				// "Extraneous token '%s'"
		return true;
		}
	return false;
	}
