// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// parse.c	Routines dealing with statement and string parsing for MightEMacs.

#include "os.h"
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include "plexcep.h"
#include "pllib.h"
#include "std.h"
#include "exec.h"
#include "var.h"

/*** Local declarations ***/

static char identchars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
static KeywordInfo kwtab[] = {
	{"and",kw_and},
	{"break",kw_break},
	{"constrain",kw_constrain},
	{"defn",kw_defn},
	{"else",kw_else},
	{"elsif",kw_elsif},
	{"endif",kw_endif},
	{"endloop",kw_endloop},
	{"endmacro",kw_endmacro},
	{"false",kw_false},
	{"for",kw_for},
	{"force",kw_force},
	{"if",kw_if},
	{"in",kw_in},
	{"loop",kw_loop},
	{"macro",kw_macro},
	{"next",kw_next},
	{"nil",kw_nil},
	{"not",kw_not},
	{"or",kw_or},
	{"return",kw_return},
	{"true",kw_true},
	{"until",kw_until},
	{"while",kw_while}};

// Operator table.
typedef struct op {
	struct op *same;		// Character on same level.
	struct op *next;		// Character on next level.
	short ch;			// Character.
	enum e_sym sym;			// Resulting symbol.
	} Op;

static Op optab[] = {
	/* !	0 */	{optab + 3,optab + 1,'!',s_not},
	/* !=	1 */	{optab + 2,NULL,'=',s_ne},
	/* !~	2 */	{NULL,NULL,'~',s_rne},
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
	/* [	35 */	{optab + 36,NULL,'[',s_lbrkt},
	/* ]	36 */	{optab + 37,NULL,']',s_rbrkt},
	/* ^	37 */	{optab + 39,optab + 38,'^',s_bxor},
	/* ^=	38 */	{NULL,NULL,'=',s_asbxor},
	/* {	39 */	{optab + 40,NULL,'{',s_lbrace},
	/* }	40 */	{optab + 41,NULL,'}',s_rbrace},
	/* |	41 */	{optab + 44,optab + 42,'|',s_bor},
	/* ||	42 */	{optab + 43,NULL,'|',s_or},
	/* |=	43 */	{NULL,NULL,'=',s_asbor},
	/* ~	44 */	{NULL,NULL,'~',s_bnot},
	};

// binsearch() helper function for returning keyword name, given table (array) and index.
static char *kwname(void *table,ssize_t i) {

	return ((KeywordInfo *) table)[i].name;
	}

// Convert ASCII string to long integer, save in *resultp (if resultp not NULL), and return status or Boolean result.
// If invalid number, return error if query is false; otherwise, return false.  Any leading or trailing white space is ignored.
int asc_long(char *src,long *resultp,bool query) {
	long lval;
	char *str;

	if(*src == '\0')
		goto Err;
	errno = 0;
	lval = strtol(src,&str,0);
	if(errno == ERANGE)
		goto Err;
	while(*str == ' ' || *str == '\t')
		++str;
	if(*str != '\0')
Err:
		return query ? false : rcset(Failure,0,text38,src);
			// "Invalid number '%s'"
	if(resultp != NULL)
		*resultp = lval;
	return query ? true : rc.status;
	}

// Convert long integer to ASCII string and store in dest.  Return dest.
char *long_asc(long n,char *dest) {

	sprintf(dest,"%ld",n);
	return dest;
	}

// Set and return proper status from a failed ProLib library call.
int librcset(int status) {

	return rcset((plexcep.flags & ExcepMem) ? Panic : status,0,"%s",plexcep.msg);
	}

// Convert a value to an integer.  Return status.
int toint(Datum *datp) {
	long i;

	if(datp->d_type != dat_int) {
		if(asc_long(datp->d_str,&i,false) != Success)
			return rc.status;
		dsetint(i,datp);
		}
	return rc.status;
	}

// Convert a datum object to a string in place, using default conversion method.  Return status.
int tostr(Datum *datp) {

	if(datp->d_type == dat_int) {
		char wkbuf[LongWidth];
#if MMDebug & Debug_Expr
		fputs("### tostr(): converted ",logfile);
		dumpval(datp);
#endif
		long_asc(datp->u.d_int,wkbuf);
		if(dsetstr(wkbuf,datp) != 0)
			(void) librcset(Failure);
#if MMDebug & Debug_Expr
		fputs(" to string ",logfile);
		dumpval(datp);
		fputc('\n',logfile);
#endif
		}
	else if(!(datp->d_type & DStrMask)) {
		if(datp->d_type == dat_nil) {
#if MMDebug & Debug_Expr
			fputs("### tostr(): converted ",logfile);
			dumpval(datp);
#endif
			dsetnull(datp);
#if MMDebug & Debug_Expr
			fputs(" to string ",logfile);
			dumpval(datp);
			fputc('\n',logfile);
#endif
			}
		else if(datp->d_type & DBoolMask) {
			if(dsetstr(datp->d_type == dat_true ? viz_true : viz_false,datp) != 0)
				(void) librcset(Failure);
			}
		else {
			DStrFab sf;

			if(dopentrk(&sf) != 0)
				goto LibFail;
			if(atosfclr(&sf,datp,NULL,0) == Success) {
				if(dclose(&sf,sf_string) != 0)
LibFail:
					(void) librcset(Failure);
				else
					datxfer(datp,sf.sf_datp);
				}
			}
		}
	return rc.status;
	}

// Find first non-whitespace character in given string and return pointer it (which may be the null terminator).  If skipInLine
// is true, in-line comments of form "/# ... #/" are detected and skipped as well.  Additionally, if an incomplete comment is
// found, an error is set and NULL is returned.
char *nonwhite(char *s,bool skipInLine) {

	for(;;) {
		while(*s == ' ' || *s == '\t')
			++s;
		if(!skipInLine || s[0] != TokC_ComInline0 || s[1] != TokC_ComInline1)
			return s;

		// Skip over in-line comment.
		s += 2;
		for(;;) {
			if(*s == '\0') {
				(void) rcset(Failure,RCNoFormat,text408);
					// "Unterminated /#...#/ comment"
				return NULL;
				}
			if(s[0] == TokC_ComInline1 && s[1] == TokC_ComInline0) {
				s += 2;
				break;
				}
			++s;
			}
		}
	}

#if 0
// Find first whitespace character in given string and return pointer it (which may be the null terminator).
char *white(char *s) {

	while(*s != '\0' && *s != ' ' && *s != '\t')
		++s;
	return s;
	}
#endif

// Find a token in a string and return it, given destination pointer, pointer to source pointer, and delimiter character (or
// -1).  If a token is found, set it in dest, update *srcp to point to the next character past the token, and return status;
// otherwise, set dest to a null string, update *srcp to point to the end of the string, and return NotFound (bypassing
// rcset()).
//
// Note: This is a very basic string-splitting routine: (1), the delimiter must be a single character (including null) or -1 for
// white space; (2), if the delimiter is null, each source character is a token; otherwise, if the delimiter is not -1, tokens
// will include any surrounding white space; otherwise, leading and trailing white space in the string is ignored and every
// sequence of one or more tabs or spaces is treated as a single delimiter; and (3), quotes, backslashes, and all characters
// except the delimiter are treated as ordinary characters.
int parsetok(Datum *destp,char **srcp,short delim) {
	short delim1,delim2,c;
	char *src0,*src;

	// Check if source is null.
	src = (delim == -1) ? nonwhite(*srcp,false) : *srcp;
	if(*src == '\0')
		return NotFound;

	// Get ready.
	src0 = src;
	if(delim == -1) {
		delim1 = ' ';
		delim2 = '\t';
		}
	else
		delim1 = delim2 = delim;

	// Get next token.
	while((c = *src++) != '\0' && c != delim1 && c != delim2 && delim1 != '\0');

	// Save the token and return results.
	if(dsetsubstr(src0,src - (delim1 == '\0' ? 0 : 1) - src0,destp) != 0)
		return librcset(Failure);
#if MMDebug & Debug_Token
	fprintf(logfile,"parsetok(): Parsed token \"%s\"\n",destp->d_str);
#endif
	*srcp = src[-1] == '\0' ? src - 1 : src;

	return rc.status;
	}

// Find the previous token in a string and return it, given destination pointer, pointer to source pointer, pointer to beginning
// of source string, and delimiter character (or -1).  This routine uses the same scheme as "parsetok", but searches in reverse
// instead of forward.  It is assumed that *srcp will point to the trailing null of "base" for the initial call, or the
// (delimiter) character immediately preceding the last token parsed otherwise.
int rparsetok(Datum *destp,char **srcp,char *base,short delim) {
	short delim1,delim2,c;
	char *src,*srcz;

	// Check if no tokens left or source is null.
	src = *srcp;
	if(delim != -1) {
		if((srcz = src) < base)
			return NotFound;
		--src;
		}
	else {
		--src;
		while(src >= base && (*src == ' ' || *src == '\t'))
			--src;
		if(src < base)
			return NotFound;
		srcz = src + 1;
		}

	// We have a token (which may be null).  src points to the last character of the next token to parse and srcz points
	// to the character after that.  Prepare to parse the token.
	if(delim == -1) {
		delim1 = ' ';
		delim2 = '\t';
		}
	else
		delim1 = delim2 = delim;

	// Find beginning of next token.
	while(src >= base && (c = *src) != delim1 && c != delim2) {
		--src;
		if(delim1 == '\0')
			break;
		}

	// Save the token and return status.
	if(dsetsubstr(src + 1,srcz - (src + 1),destp) != 0)
		return librcset(Failure);
#if MMDebug & Debug_Token
	fprintf(logfile,"rparsetok(): Parsed token \"%s\"\n",destp->d_str);
#endif
	*srcp = (delim1 == '\0' && src >= base) ? src + 1 : src;

	return rc.status;
	}

// Find end of a string literal or #{} sequence, given indirect pointer to leading character ('), ("), or ({) and terminator
// character.  Set *srcp to terminator (or trailing null if terminator not found) and return symbol.
static enum e_sym getslit(char **srcp,int termch) {
	int c;
	char *src = *srcp + 1;

	// Scan string.  Example: ' "a#{\'b\'}c" xyz'
	while((c = *src) != '\0' && c != termch) {
		switch(c) {
			case '\\':
				if(src[1] == '\0')
					goto Retn;		// Error return.
				if(termch != TokC_ExprEnd)
					++src;
				break;
			case '\'':
			case '"':
				if(termch == TokC_ExprEnd) {

					// Found embedded string (within #{}).  Begin new scan.
					(void) getslit(&src,c);
					if(*src == '\0')
						goto Retn;
					}
				// One type of string embedded within the other... skip it.
				break;
			case TokC_Expr:
				if(termch != '"' || src[1] != TokC_ExprBegin)
					break;

				// Found #{.  Begin new scan, looking for }.
				++src;
				(void) getslit(&src,TokC_ExprEnd);
				if(*src == '\0')
					goto Retn;
			}
		++src;
		}
Retn:
	// Return results.
	*srcp = src;
	return (termch == TokC_ExprEnd) ? s_nil : s_slit;
	}

// Get a symbol consisting of special characters.  If found, set *srcp to first invalid character and return symbol; otherwise,
// return s_nil.
static enum e_sym getspecial(char **srcp) {
	Op *curp,*prevp;
	char *src = *srcp;

	curp = optab;
	prevp = NULL;

	// Loop until find longest match.
	for(;;) {
		// Does current character match?
		if(*src == curp->ch) {

			// Advance to next level.
			++src;
			prevp = curp;
			if((curp = curp->next) == NULL)
				break;
			}

		// Try next character on same level.
		else if((curp = curp->same) == NULL)
			break;
		}

	*srcp = src;
	return prevp ? prevp->sym : s_nil;
	}

// Check string for a numeric literal.  If extended is true, allow form recognized by strtol() with base zero and no leading
// sign.  If valid literal found, set *srcp to first invalid character and return symbol; otherwise, return s_nil.
static enum e_sym getnlit(char **srcp,bool extended) {
	char *src = *srcp;
	if(!isdigit(*src++))
		return s_nil;

	if(extended && src[-1] == '0' && (*src == 'x' || *src == 'X')) {
		++src;
		while(isdigit(*src) || (*src >= 'a' && *src <= 'f') || (*src >= 'A' && *src <= 'F'))
			++src;
		}
	else
		while(isdigit(*src))
			++src;
	*srcp = src;

	return s_nlit;
	}

// Check string for an identifier or keyword.  If found, set *srcp to first invalid character, set *lenp to word length (if
// lenp not NULL), and return symbol; otherwise, return s_nil.
enum e_sym getident(char **srcp,ushort *lenp) {
	ssize_t i;
	ushort len;
	enum e_sym sym;
	char *src,*src0 = *srcp;

	if(!isident1(*src0))
		return s_nil;

	// Valid identifier found; find terminator.
	src = src0 + strspn(src0,identchars);

	// Query type?
	if(*src == TokC_Query) {
		*srcp = src + 1;
		if(lenp != NULL)
			*lenp = *srcp - src0;
		return s_identq;
		}

	// Check if keyword.
	sym = s_ident;
	char idbuf[(len = src - src0) + 1];
	stplcpy(idbuf,src0,len + 1);
	if(binsearch(idbuf,(void *) kwtab,sizeof(kwtab) / sizeof(KeywordInfo),strcmp,kwname,&i)) {
		sym = kwtab[i].s;
		if(lenp != NULL)
			*lenp = len;
		}

	// Return results.
	*srcp = src;
	return sym;
	}

// Parse the next symbol in the current command line and update the "last" global variable with the results.  If an invalid
// string is found, return an error; otherwise, if a symbol is found, save it in last->p_tok, set last->p_sym to the symbol
// type, update last->p_cl to point to the next character past the symbol or the terminator character, and return current
// status (Success); otherwise, set last->p_tok to a null string, update last->p_cl to point to the terminating null if
// last->p_termch is TokC_ComLine, or the (TokC_ExprEnd) terminator otherwise, and return NotFound (bypassing rcset()).
// Notes:
//   *	Symbols are delimited by white space or special characters and quoted strings are saved in last->p_tok in raw form with
//	the leading and trailing quote characters.
//   *	Command line ends at last->p_termch or null terminator.
//   *	The specific symbol type for every token is returned in last->p_sym.
int getsym(void) {
	short c;
	enum e_sym sym = s_nil;
	char *src0,*src;

	// Get ready.
	dsetnull(&last->p_tok);

	// Scan past any white space in the source string.
	if((src0 = nonwhite(last->p_cl,true)) == NULL)
		return rc.status;
	src = src0;

	// Examine first character.
	if((c = *src) != '\0' && c != last->p_termch) {
		switch(c) {
			case '"':
			case '\'':
				sym = getslit(&src,c);			// String or character literal.
				if(*src != c)				// Unterminated string?
					return rcset(Failure,0,text123,
					 strsamp(src0,strlen(src0),(size_t) term.t_ncol * 3 / 10));
						// "Unterminated string %s"
				++src;
				goto SaveExit;
			case '?':
				if(src[1] == ' ' || src[1] == '\t' || src[1] == '\0')
					goto Special;
				if(*++src == '\\')
					evalclit(&src,NULL,true);
				else
					++src;
				sym = s_clit;
				goto SaveExit;
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
				(void) getnlit(&src,true);
				goto SaveExit;

			// User variable?
			case TokC_GVar:
				if(isdigit(src[1])) {
					++src;
					sym = s_nvar;
					(void) getnlit(&src,false);
					goto SaveExit;
					}
				sym = s_gvar;
				++src;
				(void) getident(&src,NULL);
				if(src > src0 + 1)
					goto SaveExit;
				--src;
				goto Unk;

			// Identifier?
			default:
				if((sym = getident(&src,NULL)) != s_nil)
					goto SaveExit;
Special:
				if((sym = getspecial(&src)) != s_nil) {
SaveExit:
					c = *src;
					*src = '\0';
					if(dsetstr(src0,&last->p_tok) != 0)
						return librcset(Failure);
					*src = c;
					break;
					}
Unk:
				// Unknown character.  Return error.
				dsetchr(*src,&last->p_tok);
#if MMDebug & Debug_Token
				fprintf(logfile,"getsym(): RETURNING ERROR: src '%s', tok '%s'.\n",src,last->p_tok.d_str);
#endif
				return rcset(Failure,0,text289,last->p_tok.d_str);
						// "Unexpected token '%s'"
			}
		}
#if 0
	// Check for call at end-of-string (should never happen).
	if(sym == s_nil && last->p_sym == s_nil)
		return rcset(FatalError,0,"getsym(): Unexpected end of string, parsing \"%s\"",last->p_tok.d_str);
#endif
	// Update source pointer and return results.
	last->p_sym = sym;
	last->p_cl = (*src == last->p_termch && last->p_termch == TokC_ComLine) ? strchr(src,'\0') : src;
#if MMDebug & Debug_Token
	fprintf(logfile,"### getsym(): Parsed symbol \"%s\" (%d), \"%s\" remains.\n",last->p_tok.d_str,last->p_sym,last->p_cl);
#endif
	return (sym == s_nil) ? NotFound : rc.status;
	}

// Return true if next character to parse is white space.
bool havewhite(void) {

	return last->p_sym != s_nil && (*last->p_cl == ' ' || *last->p_cl == '\t');
	}

// Check if given symbol (or any symbol if sym is s_any) remains in command line being parsed (in global variable "last").  If
// no symbols are left, set error if "required" is true and return false; otherwise, if "sym" is s_any or the last symbol parsed
// matches "sym", return true; otherwise, set error if "required" is true and return false.
bool havesym(enum e_sym sym,bool required) {

	if(last->p_sym == s_nil) {

		// Nothing left.
		if(required)
			(void) rcset(Failure,RCNoFormat,text172);
				// "Token expected"
		return false;
		}

	// Correct symbol?
	if(sym == s_any || last->p_sym == sym)
		return true;

	// Nope.  Set error if required.
	if(required) {
		if(sym == s_ident || sym == s_identq || sym == s_comma)
			(void) rcset(Failure,0,text4,sym == s_comma ? text213 : text68,last->p_tok.d_str);
					// "%s expected (at token '%s')","Comma","Identifier"
		else
			(void) rcset(Failure,0,sym == s_nlit ? text38 : text289,last->p_tok.d_str);
						// "Invalid number '%s'","Unexpected token '%s'"
		}
	return false;
	}

// Check if current symbol is sym.  Get next symbol and return true if found; otherwise, set error if "required" is true and
// return false.
bool needsym(enum e_sym sym,bool required) {

	if(havesym(sym,required)) {
		(void) getsym();
		return true;
		}
	return false;
	}

// Check if any symbols remain in command line being parsed.  If there are none left, return false; otherwise, set error and
// return true.
bool extrasym(void) {

	if(havesym(s_any,false)) {
		(void) rcset(Failure,0,text22,last->p_tok.d_str);
				// "Extraneous token '%s'"
		return true;
		}
	return false;
	}
