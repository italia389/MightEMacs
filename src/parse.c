// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// parse.c	Routines dealing with statement and string parsing for MightEMacs.

#include "std.h"
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include "cxl/lib.h"
#include "exec.h"
#include "var.h"

/*** Local declarations ***/

static char identChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
static KeywordInfo keywordTable[] = {
	{"and", kw_and},
	{"break", kw_break},
	{"command", kw_command},
	{"defn", kw_defn},
	{"else", kw_else},
	{"elsif", kw_elsif},
	{"endif", kw_endif},
	{"endloop", kw_endloop},
	{"endroutine", kw_endroutine},
	{"false", kw_false},
	{"for", kw_for},
	{"force", kw_force},
	{"function", kw_function},
	{"if", kw_if},
	{"in", kw_in},
	{"loop", kw_loop},
	{"next", kw_next},
	{"nil", kw_nil},
	{"not", kw_not},
	{"or", kw_or},
	{"return", kw_return},
	{"true", kw_true},
	{"until", kw_until},
	{"while", kw_while}};

// Operator table.
typedef struct Op {
	struct Op *same;		// Character on same level.
	struct Op *next;		// Character on next level.
	short c;			// Character.
	Symbol sym;			// Resulting symbol.
	} Op;

static Op opTable[] = {
	/* !	0 */	{opTable + 3, opTable + 1, '!', s_not},
	/* !=	1 */	{opTable + 2, NULL, '=', s_ne},
	/* !~	2 */	{NULL, NULL, '~', s_regNE},
	/* %	3 */	{opTable + 5, opTable + 4, '%', s_mod},
	/* %=	4 */	{NULL, NULL, '=', s_assignMod},
	/* &	5 */	{opTable + 8, opTable + 6, '&', s_bitAnd},
	/* &&	6 */	{opTable + 7, NULL, '&', s_and},
	/* &=	7 */	{NULL, NULL, '=', s_assignBitAnd},
	/* (	8 */	{opTable + 9, NULL, '(', s_leftParen},
	/* )	9 */	{opTable + 10, NULL, ')', s_rightParen},
	/* *	10 */	{opTable + 12, opTable + 11, '*', s_mult},
	/* *=	11 */	{NULL, NULL, '=', s_assignMult},
	/* +	12 */	{opTable + 15, opTable + 13, '+', s_plus},
	/* ++	13 */	{opTable + 14, NULL, '+', s_incr},
	/* +=	14 */	{NULL, NULL, '=', s_assignAdd},
	/* ,	15 */	{opTable + 16, NULL, ',', s_comma},
	/* -	16 */	{opTable + 19, opTable + 17, '-', s_minus},
	/* --	17 */	{opTable + 18, NULL, '-', s_decr},
	/* -=	18 */	{NULL, NULL, '=', s_assignSub},
	/* /	19 */	{opTable + 21, opTable + 20, '/', s_div},
	/* /=	20 */	{NULL, NULL, '=', s_assignDiv},
	/* :	21 */	{opTable + 22, NULL, ':', s_colon},
	/* <	22 */	{opTable + 26, opTable + 23, '<', s_lt},
	/* <<	23 */	{opTable + 25, opTable + 24, '<', s_leftShift},
	/* <<=	24 */	{NULL, NULL, '=', s_assignLeftShift},
	/* <=	25 */	{NULL, NULL, '=', s_le},
	/* =	26 */	{opTable + 30, opTable + 27, '=', s_assign},
	/* ==	27 */	{opTable + 28, NULL, '=', s_eq},
	/* =>	28 */	{opTable + 29, NULL, '>', s_nArg},
	/* =~	29 */	{NULL, NULL, '~', s_regEQ},
	/* >	30 */	{opTable + 34, opTable + 31, '>', s_gt},
	/* >=	31 */	{opTable + 32, NULL, '=', s_ge},
	/* >>	32 */	{NULL, opTable + 33, '>', s_rightShift},
	/* >>=	33 */	{NULL, NULL, '=', s_assignRightShift},
	/* ?	34 */	{opTable + 35, NULL, '?', s_hook},
	/* [	35 */	{opTable + 36, NULL, '[', s_leftBracket},
	/* ]	36 */	{opTable + 37, NULL, ']', s_rightBracket},
	/* ^	37 */	{opTable + 39, opTable + 38, '^', s_bitXOr},
	/* ^=	38 */	{NULL, NULL, '=', s_assignBitXOr},
	/* {	39 */	{opTable + 40, NULL, '{', s_leftBrace},
	/* }	40 */	{opTable + 41, NULL, '}', s_rightBrace},
	/* |	41 */	{opTable + 44, opTable + 42, '|', s_bitOr},
	/* ||	42 */	{opTable + 43, NULL, '|', s_or},
	/* |=	43 */	{NULL, NULL, '=', s_assignBitOr},
	/* ~	44 */	{NULL, NULL, '~', s_bitNot},
	};

// binSearch() helper function for returning script keyword, given table (array) and index.
static const char *scriptKeyword(const void *table, ssize_t i) {

	return ((KeywordInfo *) table)[i].name;
	}

// Convert ASCII string to long integer, save in *result (if result not NULL), and return status or Boolean result.  If invalid
// number, return error if query is false; otherwise, return false.  Any leading or trailing white space is ignored.
int ascToLong(const char *src, long *result, bool query) {
	long longVal;
	char *str;

	if(*src == '\0')
		goto Err;
	errno = 0;
	longVal = strtol(src, &str, 0);
	if(errno == ERANGE)
		goto Err;
	while(*str == ' ' || *str == '\t')
		++str;
	if(*str != '\0')
Err:
		return query ? false : rsset(Failure, 0, text38, src);
			// "Invalid number '%s'"
	if(result != NULL)
		*result = longVal;
	return query ? true : sess.rtn.status;
	}

// Convert long integer to ASCII string and store in dest.  Return dest.
char *longToAsc(long n, char *dest) {

	sprintf(dest, "%ld", n);
	return dest;
	}

// Convert a value to an integer.  Return status.
int toInt(Datum *pDatum) {
	long i;

	if(pDatum->type != dat_int) {
		if(ascToLong(pDatum->str, &i, false) != Success)
			return sess.rtn.status;
		dsetint(i, pDatum);
		}
	return sess.rtn.status;
	}

// Check if return code from a CXL library datum-conversion function is greater than zero (indicating endless recursion during
// an array-to-string conversion) and set an error message if so.  Return status.
int endless(int rtnCode) {

	return rtnCode > 0 ? rsset(Failure, RSNoFormat, text195) : sess.rtn.status;
		// "Endless recursion detected (array contains itself)"
	}

// Convert a Datum object to a string in place, using default conversion method.  Return status.
int toStr(Datum *pDatum) {

#if MMDebug & Debug_Expr
	fputs("### toStr(): converted ", logfile);
	dumpVal(pDatum);
#endif
	if(disnil(pDatum))
		dsetnull(pDatum);
	else if(!dtypstr(pDatum)) {
		int rtnCode;
		Datum *pDatum1;

		if(dnewtrack(&pDatum1) != 0 || (rtnCode = dtos(pDatum1, pDatum, NULL, 0)) < 0)
			return libfail();
		if(endless(rtnCode) == Success) {
			dxfer(pDatum, pDatum1);
#if MMDebug & Debug_Expr
			fputs(" to string ", logfile);
			dumpVal(pDatum);
			fputc('\n', logfile);
#endif
			}
		}
	return sess.rtn.status;
	}

// Find first non-whitespace character in given string and return pointer to it (which may be the null terminator).  If
// skipInLine is true, in-line comments of form "/# ... #/" are detected and skipped as well.  Additionally, if an incomplete
// comment is found, an error is set and NULL is returned.
char *nonWhite(const char *s, bool skipInLine) {

	for(;;) {
		while(isspace(*s))
			++s;
		if(!skipInLine || s[0] != TokC_InlineComm0 || s[1] != TokC_InlineComm1)
			break;

		// Skip over in-line comment.
		s += 2;
		for(;;) {
			if(*s == '\0') {
				(void) rsset(Failure, RSNoFormat, text408);
					// "Unterminated /#...#/ comment"
				return NULL;
				}
			if(s[0] == TokC_InlineComm1 && s[1] == TokC_InlineComm0) {
				s += 2;
				break;
				}
			++s;
			}
		}
	return (char *) s;
	}

// Deselect all options in an option table.
void deselectOpts(Option *pOpt) {

	do {
		if(!(pOpt->ctrlFlags & OptIgnore))
			pOpt->ctrlFlags &= ~OptSelected;
		} while((++pOpt)->keyword != NULL);
	}

// Initialize all option table options (which are assumed to be Boolean types) to true if OptFalse flag set, otherwise false.
void initBoolOpts(Option *pOpt) {

	do {
		*((bool *) pOpt->u.ptr) = (pOpt->ctrlFlags & OptFalse) != 0;
		} while((++pOpt)->keyword != NULL);
	}

// Set Boolean values in option table after parseOpts() call.
void setBoolOpts(Option *pOpt) {

	do {
		if(pOpt->ctrlFlags & OptSelected)
			*((bool *) pOpt->u.ptr) = (pOpt->ctrlFlags & OptFalse) == 0;
		} while((++pOpt)->keyword != NULL);
	}

// Get and return flags from option table after parseOpts() call.
uint getFlagOpts(Option *pOpt) {
	uint flags = 0;

	do {
		if(pOpt->ctrlFlags & OptSelected)
			flags |= pOpt->u.value;
		} while((++pOpt)->keyword != NULL);
	return flags;
	}

// Parse next token from string at *pStr delimited by delimChar (which is assumed to not be a space or tab).  If first non-
// whitespace character found is terminating null, return NULL; otherwise, (1), set *pStr to next character past token, or NULL
// if terminating null was found; and (2), return pointer to trimmed result (which may be an empty string).  The source string
// is modified by this routine.
char *strparse(char **pStr, short delimChar) {
	short c;
	char *str1, *str2;
	char *str0 = *pStr;

	// Get ready for scan.
	if(str0 == NULL)
		return NULL;
	while(*str0 == ' ' || *str0 == '\t')
		++str0;
	if(*str0 == '\0')
		return NULL;

	// Found beginning of next token... find end.
	str1 = str0;					// str0 points to beginning of token (first non-whitespace character).
	while((c = *str1) != '\0' && c != delimChar)
		++str1;
	str2 = str1;					// str1 points to token delimiter.
	while(str2 > str0 &&
	 (str2[-1] == ' ' || str2[-1] == '\t'))
		--str2;
	*str2 = '\0';					// str2 points to end of trimmed token.

	// Done.  Update *pStr and return token pointer.
	*pStr = (c == '\0') ? NULL : str1 + 1;
	return str0;
	}

// Parse one or more keyword options from given Datum object (pOptions != NULL), the next command-line argument (pOptions ==
// NULL, using optHdr->argFlags in funcArg() call), or build a prompt and obtain interactively, if applicable, and return
// status.  If pOptions is not NULL, it may be nil (in which case it is processed as a null string); otherwise, it is assumed to
// contain a string.  Keywords are specified in given pOptHdr->optTable table, which is assumed to be a NULL-terminated array of
// Option objects.  Options that are found are marked in table by setting the OptSelected flag.  If an unknown option is found
// or if no options are specified and ArgNil1 flag is not set in optHdr->argFlags, an error is returned; otherwise, *count (if
// count not NULL) is set to the number of options found, which will be zero if interactive and nothing was entered.
int parseOpts(OptHdr *pOptHdr, const char *basePrompt, Datum *pOptions, int *count) {
	Datum *pOptStr;
	Option *pOpt;
	int found = 0;
	const char *keyword;
	char *str;

	if((!(sess.opFlags & OpScript) || pOptions == NULL) && dnewtrack(&pOptStr) != 0)
		goto LibFail;

	// Deselect all options in table.
	deselectOpts(pOptHdr->optTable);

	if(sess.opFlags & OpScript) {

		// Get string to parse.  Use caller-supplied options if available.
		if(pOptions != NULL)
			pOptStr = pOptions;
		else if(funcArg(pOptStr, pOptHdr->argFlags) != Success)
			return sess.rtn.status;

		if(!disnil(pOptStr)) {
			// Parse keywords from argument string.
			char *str0, *str1;
			char workBuf[32];
			char optBuf[strlen(str0 = pOptStr->str) + 1];
			strcpy(optBuf, str0);
			while((str1 = strparse(&str0, ',')) != NULL) {
				if(*str1 == '\0')
					continue;

				// Scan table for a match.
				pOpt = pOptHdr->optTable;
				do {
					if(!(pOpt->ctrlFlags & OptIgnore)) {

						// Copy keyword to temporary buffer with '^' character removed.
						keyword = pOpt->keyword;
						str = workBuf;
						do {
							if(*keyword != '^')
								*str++ = *keyword;
							} while(*++keyword != '\0');
						*str = '\0';

						// Check for match.
						if(strcasecmp(str1, workBuf) == 0)
							goto Found;
						}
					} while((++pOpt)->keyword != NULL);

				// Match not found... return error.
				goto ErrRtn;
Found:
				// Match found.  Count it, validate it, and mark table entry.
				if(++found > 1 && pOptHdr->single) {
					str1 = optBuf;
ErrRtn:
					return rsset(Failure, 0, text447, pOptHdr->type, str1);
						// "Invalid %s '%s'"
					}
				pOpt->ctrlFlags |= OptSelected;
				}
			}

		// Error if none found and ArgNil1 flag not set.
		if(found == 0 && !(pOptHdr->argFlags & ArgNil1))
			return rsset(Failure, 0, text455, pOptHdr->type);
				// "Missing required %s",
		}
	else {
		// Build prompt.
		DFab prompt;
		char *lead = " (";
		bool optLetter = false;
		short c;

		if(dopenwith(&prompt, pOptStr, FabClear) != 0 || dputs(basePrompt, &prompt, 0) != 0)
			goto LibFail;
		pOpt = pOptHdr->optTable;
		do {
			if(!(pOpt->ctrlFlags & OptIgnore)) {
				if(dputs(lead, &prompt, 0) != 0)
					goto LibFail;

				// Copy keyword to prompt with letter following '^' in bold and underlined.
				keyword = (term.cols >= 80 || pOpt->abbr == NULL) ? pOpt->keyword : pOpt->abbr;
				do {
					if(*keyword == '^') {
						if(dputs("~u~b", &prompt, 0) != 0)
							goto LibFail;
						optLetter = true;
						++keyword;
						}
					if(dputc(*keyword++, &prompt, 0) != 0)
						goto LibFail;
					if(optLetter) {
						if(dputs("~Z", &prompt, 0) != 0)
							goto LibFail;
						optLetter = false;
						}
					} while(*keyword != '\0');
				lead = ", ";
				}
			} while((++pOpt)->keyword != NULL);
		if(dputc(')', &prompt, 0) != 0 || dclose(&prompt, FabStr) != 0)
			goto LibFail;

		// Get option letter(s) from user.  If single letter (int) is input, convert it to a string.
		if(termInp(pOptStr, pOptStr->str, ArgNotNull1 | ArgNil1,
		 pOptHdr->single ? Term_LongPrmt | Term_Attr | Term_OneChar : Term_LongPrmt | Term_Attr, NULL) != Success)
			return sess.rtn.status;
		if(disnil(pOptStr))
			goto Retn;
		if(pOptHdr->single)
			dconvchr(lowCase[pOptStr->u.intNum], pOptStr);
		else
			makeLower(pOptStr->str, pOptStr->str);

		// Have one or more letters in pOptStr.  Scan table and mark matches.
		pOpt = pOptHdr->optTable;
		do {
			if(!(pOpt->ctrlFlags & OptIgnore)) {

				// Find option letter, check if in user string, and if so, invalidate letter and mark
				// table entry.
				c = lowCase[(int) strchr(pOpt->keyword, '^')[1]];
				if((str = strchr(pOptStr->str, c)) != NULL) {
					++found;
					*str = 0xFF;
					pOpt->ctrlFlags |= OptSelected;
					}
				else
					pOpt->ctrlFlags &= ~OptSelected;
				}
			} while((++pOpt)->keyword != NULL);

		// Any leftover letters in user string?
		str = pOptStr->str;
		do {
			if(*str != 0xFF) {
				char workBuf[2];

				workBuf[0] = *str;
				workBuf[1] = '\0';
				return rsset(Failure, 0, text447, pOptHdr->type, workBuf);
						// "Invalid %s '%s'"
				}
			} while(*++str != '\0');
		}
Retn:
	// Scan completed and table entries marked.  Return number of matches.
	if(count != NULL)
		*count = found;
	return sess.rtn.status;
LibFail:
	return libfail();
	}

// Convert first argument to string form, set result as return value, and return status.  Conversion options are parsed from
// optional second argument:
//	ShowNil		-> convert nil to "nil", otherwise "".
//	SkipNil		-> skip nil arguments in arrays.
//	Quote1		-> enclose strings in single (') quotes.
//	Quote2		-> enclose strings in double (") quotes.
//	Quote		-> quote characters and strings depending on type and flags.
//	VizChar		-> convert invisible characters in strings to visible form via vizc().
//	EscChar		-> convert invisible characters in strings to escaped-character form.
//	ThouSep		-> insert commas in thousands' places in integers.
//	Delim		-> insert comma delimiter between array elements.
//	Brackets	-> wrap array elements in brackets [...].
// Additionally, a string delimiter may be specified as the third argument (and the second argument may be nil).
int toString(Datum *pRtnVal, int n, Datum **args) {
	ushort cflags = 0;
	char *delim = NULL;
	static Option options[] = {
		{"Show^Nil", NULL, 0, DCvtShowNil},
		{"S^kipNil", NULL, 0, ACvtSkipNil},
		{"Quote^1", NULL, 0, DCvtQuote1},
		{"Quote^2", NULL, 0, DCvtQuote2},
		{"^Quote", NULL, 0, DCvtQuote},
		{"^VizChar", NULL, 0, DCvtVizChar},
		{"^EscChar", NULL, 0, DCvtEscChar},
		{"^ThouSep", NULL, 0, DCvtThouSep},
		{"^Delim", NULL, 0, ACvtDelim},
		{"^Brackets", NULL, 0, ACvtBrkts},
		{"^Lang", NULL, 0, DCvtLang},
		{"Viz^Str", NULL, 0, DCvtVizStr},
		{NULL, NULL, 0, 0}};
	static OptHdr optHdr = {
		0, text451, false, options};
			// "function option"

	// Get options and set flags.
	if(args[1] != NULL) {
		if(!disnil(args[1])) {
			if(parseOpts(&optHdr, NULL, args[1], NULL) != Success)
				return sess.rtn.status;
			cflags = getFlagOpts(options);
			}

		// Get delimiter argument, if present.
		if(args[2] != NULL)
			delim = args[2]->str;
		}

	// Do conversion and return results.
	return dtos(pRtnVal, args[0], delim, cflags) < 0 ? libfail() : sess.rtn.status;
	}

// Find a token in a string and return it, given destination pointer, pointer to source pointer, and delimiter character (or
// -1).  If a token is found, set it in pDest, update *pSrc to point to the next character past the token, and return status;
// otherwise, set pDest to a null string, update *pSrc to point to the end of the string, and return NotFound (bypassing
// rsset()).
//
// Note: This is a very basic string-splitting routine: (1), the delimiter must be a single character (including null) or -1 for
// white space; (2), if the delimiter is null, each source character is a token; otherwise, if the delimiter is not -1, tokens
// will include any surrounding white space; otherwise, leading and trailing white space in the string is ignored and every
// sequence of one or more isspace() characters is treated as a single delimiter; and (3), quotes, backslashes, and all
// characters except the delimiter are treated as ordinary characters.
int parseTok(Datum *pDest, char **pSrc, short delimChar) {
	short c;
	char *src0, *src;
	ptrdiff_t offset = 0;

	// Check if source is null.
	src = (delimChar == -1) ? nonWhite(*pSrc, false) : *pSrc;
	if((c = *src) == '\0')
		return NotFound;

	// Get next token.
	src0 = src;
	if(delimChar == '\0')
		++src;
	else if(delimChar == -1) {
		while((c = *++src) != '\0')
			if(isspace(c)) {
				offset = 1;
				break;
				}
		}
	else
		do {
			if(c == delimChar) {
				offset = 1;
				break;
				}
			} while((c = *++src) != '\0');

	// Save the token and return results.
	if(dsetsubstr(src0, src - src0, pDest) != 0)
		return libfail();
#if MMDebug & Debug_Token
	fprintf(logfile, "parseTok(): Parsed token \"%s\"\n", pDest->str);
#endif
	*pSrc = src + offset;
	return sess.rtn.status;
	}

// Find the previous token in a string and return it, given destination pointer, pointer to source pointer, pointer to beginning
// of source string, and delimiter character (or -1).  This routine works like "parseTok", except that it searches backward
// instead of forward.  It is assumed that *pSrc will point to the trailing null of "base" for the initial call, or the
// (delimiter) character immediately preceding the last token parsed otherwise.
int revParseTok(Datum *pDest, char **pSrc, char *base, short delimChar) {
	char *src, *srcEnd;

	// Check if no tokens left or source is null.
	src = *pSrc;
	if(delimChar == -1) {
		--src;
		while(src >= base && isspace(*src))
			--src;
		if(src < base)
			return NotFound;
		srcEnd = src + 1;
		}
	else {
		if((srcEnd = src) < base)
			return NotFound;
		--src;
		}

	// We have a token (which may be null).  src points to the last character of the next token to parse and srcEnd points
	// to the character after that.  Find beginning of next token.
	if(delimChar == '\0')
		--src;
	else if(delimChar == -1)
		do {
			if(--src < base)
				break;
			} while(!isspace(*src));
	else
		while(src >= base && *src != delimChar)
			--src;

	// Save the token and return status.
	if(dsetsubstr(src + 1, srcEnd - (src + 1), pDest) != 0)
		return libfail();
#if MMDebug & Debug_Token
	fprintf(logfile, "revParseTok(): Parsed token \"%s\"\n", pDest->str);
#endif
	*pSrc = (delimChar == '\0') ? src + 1 : src;

	return sess.rtn.status;
	}

// Find end of a string literal or #{} sequence, given indirect pointer to leading character ('), ("), or ({) and terminator
// character.  Set *pSrc to terminator (or trailing null if terminator not found) and return symbol.
static Symbol getStrLit(char **pSrc, int termChar) {
	int c;
	char *src = *pSrc + 1;

	// Scan string.  Example: ' "a#{\'b\'}c" xyz'
	while((c = *src) != '\0' && c != termChar) {
		switch(c) {
			case '\\':
				if(src[1] == '\0')
					goto Retn;		// Error return.
				if(termChar != TokC_ExprEnd)
					++src;
				break;
			case '\'':
			case '"':
				if(termChar == TokC_ExprEnd) {

					// Found embedded string (within #{}).  Begin new scan.
					(void) getStrLit(&src, c);
					if(*src == '\0')
						goto Retn;
					}
				// One type of string embedded within the other... skip it.
				break;
			case TokC_Expr:
				if(termChar != '"' || src[1] != TokC_ExprBegin)
					break;

				// Found #{.  Begin new scan, looking for }.
				++src;
				(void) getStrLit(&src, TokC_ExprEnd);
				if(*src == '\0')
					goto Retn;
			}
		++src;
		}
Retn:
	// Return results.
	*pSrc = src;
	return (termChar == TokC_ExprEnd) ? s_nil : s_strLit;
	}

// Get a symbol consisting of special characters.  If found, set *pSrc to first invalid character and return symbol; otherwise,
// return s_nil.
static Symbol getSpecial(char **pSrc) {
	Op *pCurOp, *pPrevOp;
	char *src = *pSrc;

	pCurOp = opTable;
	pPrevOp = NULL;

	// Loop until find longest match.
	for(;;) {
		// Does current character match?
		if(*src == pCurOp->c) {

			// Advance to next level.
			++src;
			pPrevOp = pCurOp;
			if((pCurOp = pCurOp->next) == NULL)
				break;
			}

		// Try next character on same level.
		else if((pCurOp = pCurOp->same) == NULL)
			break;
		}

	*pSrc = src;
	return pPrevOp ? pPrevOp->sym : s_nil;
	}

// Check string for a numeric literal.  If extended is true, allow form recognized by strtol() with base zero and no leading
// sign.  If valid literal found, set *pSrc to first invalid character and return symbol; otherwise, return s_nil.
static Symbol getNumLit(char **pSrc, bool extended) {
	char *src = *pSrc;
	if(!is_digit(*src++))
		return s_nil;

	if(extended && src[-1] == '0' && (*src == 'x' || *src == 'X')) {
		++src;
		while(is_digit(*src) || (*src >= 'a' && *src <= 'f') || (*src >= 'A' && *src <= 'F'))
			++src;
		}
	else
		while(is_digit(*src))
			++src;
	*pSrc = src;

	return s_numLit;
	}

// Check string for an identifier or keyword.  If found, set *pSrc to first invalid character, set *pWordLen to word length (if
// pWordLen not NULL), and return symbol; otherwise, return s_nil.
Symbol getIdent(char **pSrc, ushort *pWordLen) {
	ssize_t i;
	ushort len;
	Symbol sym;
	char *src, *src0 = *pSrc;

	if(!isIdent1(*src0))
		return s_nil;

	// Valid identifier found; find terminator.
	src = src0 + strspn(src0, identChars);

	// Query type?
	if(*src == TokC_Query) {
		*pSrc = src + 1;
		if(pWordLen != NULL)
			*pWordLen = *pSrc - src0;
		return s_identQuery;
		}

	// Check if keyword.
	sym = s_ident;
	char workBuf[(len = src - src0) + 1];
	stplcpy(workBuf, src0, len + 1);
	if(binSearch(workBuf, (const void *) keywordTable, elementsof(keywordTable), strcmp, scriptKeyword, &i)) {
		sym = keywordTable[i].sym;
		if(pWordLen != NULL)
			*pWordLen = len;
		}

	// Return results.
	*pSrc = src;
	return sym;
	}

// Parse the next symbol in the current command line and update the "last" global variable with the results.  If an invalid
// string is found, return an error; otherwise, if a symbol is found, save it in pLastParse->tok, set pLastParse->sym to the
// symbol type, update pLastParse->src to point to the next character past the symbol or the terminator character, and return
// current status (Success); otherwise, set pLastParse->tok to a null string, update pLastParse->src to point to the terminating
// null if ParseExprEnd flag not set in pLastParse->flags, or the "stop" character otherwise, and return NotFound (bypassing
// rsset()).
// Notes:
//   *	Symbols are delimited by white space or special characters and quoted strings are saved in pLastParse->tok in raw form
//	with the leading and trailing quote characters.
//   *	Command line ends at null terminator or possibly ParseStmtEnd (semicolon) and/or ParseExprEnd (right brace), depending
//      on parsing flags in pLastParse->flags.
//   *	The specific symbol type for every token is returned in pLastParse->sym.
int getSym(void) {
	short c;
	Symbol sym = s_nil;
	char *src0, *src;

	// Get ready.
	dsetnull(&pLastParse->tok);

	// Scan past any white space in the source string.
	if((src0 = nonWhite(pLastParse->src, true)) == NULL)
		return sess.rtn.status;
	src = src0;

	// Examine first character.
	switch(c = *src) {
		case '\0':
			break;
		case TokC_Comment:
			if(pLastParse->flags & ParseExprEnd)
				goto Unk;
			src = strchr(src, '\0');
			break;
		case TokC_StmtEnd:
			if(pLastParse->flags & ParseStmtEnd)
				break;
			goto Unk;
		case TokC_ExprEnd:
			if(pLastParse->flags & ParseExprEnd)
				break;
			goto Special;
		case '"':
		case '\'':
			sym = getStrLit(&src, c);		// String or character literal.
			if(*src != c)				// Unterminated string?
				return rsset(Failure, 0, text123,
				 strSamp(src0, strlen(src0), (size_t) term.cols * 3 / 10));
					// "Unterminated string %s"
			++src;
			goto SaveExit;
		case '?':
			if(src[1] == ' ' || src[1] == '\t' || src[1] == '\0')
				goto Special;
			if(*++src == '\\')
				evalCharLit(&src, NULL, true);
			else
				++src;
			sym = s_charLit;
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
			sym = s_numLit;				// Numeric literal.
			(void) getNumLit(&src, true);
			goto SaveExit;

		// User variable?
		case TokC_GlobalVar:
			if(is_digit(src[1])) {
				++src;
				sym = s_numVar;
				(void) getNumLit(&src, false);
				goto SaveExit;
				}
			sym = s_globalVar;
			++src;
			(void) getIdent(&src, NULL);
			if(src > src0 + 1)
				goto SaveExit;
			--src;
			goto Unk;

		// Identifier?
		default:
			if((sym = getIdent(&src, NULL)) != s_nil)
				goto SaveExit;
Special:
			if((sym = getSpecial(&src)) != s_nil) {
SaveExit:
				c = *src;
				*src = '\0';
				if(dsetstr(src0, &pLastParse->tok) != 0)
					return libfail();
				*src = c;
				break;
				}
Unk:
			// Unknown character.  Return error.
			dconvchr(*src, &pLastParse->tok);
#if MMDebug & Debug_Token
			(void) rsset(Failure, 0, text289, pLastParse->tok.str);
					// "Unexpected token '%s'"
			fprintf(logfile, "getSym(): RETURNING ERROR: %hd \"%s\", src '%s', tok '%s'.\n",
			 sess.rtn.status, sess.rtn.msg.str, src, pLastParse->tok.str);
			return sess.rtn.status;
#else
			return rsset(Failure, 0, text289, pLastParse->tok.str);
					// "Unexpected token '%s'"
#endif
		}
#if SanityCheck
	// Check if this function was called at end-of-parse-string (should never happen).
	if(sym == s_nil && pLastParse->sym == s_nil)
		return rsset(FatalError, 0, "getSym(): Called after end of string detected, parsing \"%s\"",
		 pLastParse->tok.str);
#endif
	// Return results.
	pLastParse->sym = sym;
	pLastParse->src = src;
#if MMDebug & Debug_Token
	fprintf(logfile, "### getSym(): Parsed symbol \"%s\" (%d), \"%s\" remains.\n",
	 pLastParse->tok.str, pLastParse->sym, pLastParse->src);
#endif
	return (sym == s_nil) ? NotFound : sess.rtn.status;
	}

// Return true if next character to parse is white space.
bool haveWhite(void) {

	return pLastParse->sym != s_nil && (*pLastParse->src == ' ' || *pLastParse->src == '\t');
	}

// Check if given symbol (or any symbol if sym is s_any) remains in command line being parsed..
// If a symbol is found:
//	If symbol matches "sym" or "sym" is s_any, return true.
//	Otherwise, set error if "required" is true and return false.
// If no symbols left:
//	Set error if "required" is true and return false.
bool haveSym(Symbol sym, bool required) {

	if(pLastParse->sym == s_nil) {

		// Nothing left.
		if(required)
			(void) rsset(Failure, RSNoFormat, text172);
				// "Token expected"
		return false;
		}

	// Correct symbol?
	if(sym == s_any || pLastParse->sym == sym)
		return true;

	// Nope.  Set error if required.
	if(required) {
		if(sym == s_ident || sym == s_identQuery || sym == s_comma)
			(void) rsset(Failure, 0, text4, sym == s_comma ? text213 : text68, pLastParse->tok.str);
					// "%s expected (at token '%s')", "Comma", "Identifier"
		else
			(void) rsset(Failure, 0, sym == s_numLit ? text38 : text289, pLastParse->tok.str);
						// "Invalid number '%s'", "Unexpected token '%s'"
		}
	return false;
	}

// Check if current symbol is sym.  Get next symbol and return true if found; otherwise, set error if "required" is true and
// return false.
bool needSym(Symbol sym, bool required) {

	if(haveSym(sym, required)) {
		(void) getSym();
		return true;
		}
	return false;
	}

// Check if any symbols remain in command line being parsed.  If there are none left, return false; otherwise, set error and
// return true.
bool extraSym(void) {

	if(haveSym(s_any, false)) {
		(void) rsset(Failure, 0, text22, pLastParse->tok.str);
				// "Extraneous token '%s'"
		return true;
		}
	return false;
	}
