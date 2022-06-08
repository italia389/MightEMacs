// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// search.h	Search and replace definitions for MightEMacs.

#include "xre.h"

#define CharScanCount	50000000	// Number of scanned regexp characters which triggers display of progress message.
#define Metacharacters	"^$([{.*+?|\\"	// Metacharacters in a regular expression.

// Metacharacters.
#define MC_Any		'.'		// 'Any' character (except newline).
#define MC_HexChar	'x'		// Beginning of hexadecimal character literal.
#define MC_HexBegin	'{'		// Hex literal - Beginning delimiter.
#define MC_HexEnd	'}'		// Hex literal - Ending delimiter.
#define MC_CCBegin	'['		// Beginning of character class.
#define MC_NegCC	'^'		// Negated character class.
#define MC_CCRange	'-'		// Range indicator in character class.
#define MC_CCEnd	']'		// End of character class.
#define MC_BOL		'^'		// Beginning of line.
#define MC_EOL		'$'		// End of line.
#define MC_Closure0	'*'		// Closure - zero or more characters.
#define MC_Closure1	'+'		// Closure - one or more characters.
#define MC_Closure01	'?'		// Closure - zero or one characters, and modifier to match minimum number possible.
#define MC_ClBegin	'{'		// Closure - beginning of count.
#define MC_ClEnd	'}'		// Closure - end of count.
#define MC_GrpBegin	'('		// Beginning of group (no backslash prefix).
#define MC_GrpEnd	')'		// End of group (no backslash prefix).
#define MC_Escape	'\\'		// Escape - suppress meta-meaning.
#define MC_OrBar	'|'		// Or bar - subexpression separator.
#define MC_OptBegin	'?'		// Begin group option.
#define MC_OptOff	'-'		// Turn following options OFF.
#define MC_NonCapt	':'		// Non-capturing group.
#define MC_Comment	'#'		// Comment.

#define MC_BOW		'<'		// Beginning of word.
#define MC_EOW		'>'		// End of word.

// Element types in a replacement pattern.
#define RPE_LitString	1		// Literal string.
#define RPE_GrpMatch	2		// String of group match.

#define OptCh_Begin	':'		// Beginning of options (not part of pattern).
#define OptCh_Exact	'e'		// Case-sensitive matches.
#define OptCh_Fuzzy	'f'		// Fuzzy matches.
#define OptCh_Ignore	'i'		// Ignore case.
#define OptCh_Multi	'm'		// Multiline mode.
#define OptCh_Plain	'p'		// Plain-text mode.
#define OptCh_Regexp	'r'		// Regexp mode.
#define OptCh_N		7		// Number of option characters.

#define MaxGroups	10		// Maximum number of RE groups (#0 reserved for entire match).

// Scan "point" and type definitions.
typedef struct {
	ushort type;
	union {
		Point bufPoint;		// Buffer point.
		char *strPoint;		// String point.
		} u;
	} ScanPoint;

#define ScanPt_Buf	0
#define ScanPt_Str	1

// String match-location object.
typedef struct {
	char *strPoint;
	long len;
	} StrLoc;

// Match text.
typedef union {
	Region region;			// Buffer location of the beginning of the match and length.
	StrLoc strLoc;			// String pointer to the beginning of the match and length.
	} MatchLoc;

// Regexp scanning parameters.
typedef struct {
	regmatch_t *grpList;		// Group match positions (array).
	ScanPoint startPoint;		// Scan starting point in buffer or string.
	ushort direct;			// Scanning direction -- Forward or Backward.
	} RegMatch;

// Structure for a regular expression pattern and its variants.
typedef struct {
	char *backPat;			// Backward pattern string (atom-by-atom).
	regex_t compPat;		// Compiled forward pattern.
	regex_t compBackPat;		// Compiled backward (reversed) pattern.
	} RegPat;

// Meta-character structure for a replacment pattern element.
typedef struct ReplPat {
	struct ReplPat *next;
	ushort type;
	union {
		ushort grpNum;		// Group match reference number.
		char replStr[1];	// Static string.
		} u;
	} ReplPat;

// Structure for regular expression group matches.
typedef struct {
	ushort size;			// Size of "groups" array.
	Datum *groups;			// Array of Datum objects (or NULL if "size" is zero).
	} GrpMatch;

// Pattern-matching control variables.  Note that group elements in "mcpat" point to array elements in "groups".
typedef struct {
	ushort flags;			// RE flags (below).
	uint searchTableSize;		// Size of search arrays.
	uint replTableSize;		// Size of replacement arrays.
	ushort grpCount;		// Number of groups in RE pattern, not counting group 0.
	int patLen;			// Length of search pattern (RE and non-RE) without trailing option characters.
	char *pat;			// Forward search pattern (RE and non-RE) without trailing option characters.
	RegPat regPat;			// Compiled RE search patterns (forward and backward).
	char *replPat;			// Replacement pattern (RE and non-RE).
	ReplPat *compReplPat;		// Compiled RE replacement pattern.
	GrpMatch grpMatch;		// Datum objects for each RE group string found in search.  Group 0 contains
					// entire matched text from source object (buffer or string).
	} Match;

// Search control variables -- for search and replace commands.
typedef struct {
	ushort inpDelim;		// Search prompt terminator.
	char *backPat;			// Backward (reversed) search pattern for plain text buffer searches.
	int forwDelta1[256];		// Forward non-RE delta1 table.
	int backDelta1[256];		// Backward non-RE delta1 table.
	int *forwDelta2;		// Forward non-RE delta2 table.
	int *backDelta2;		// Backward non-RE delta2 table.
	Match match;			// Match information.
	} SearchCtrl;

#define SRegical	0x0001		// Metacharacters found in search string.
#define	RRegical	0x0002		// Metacharacters found in replacement string.

// The following pattern options must be different than Forward and Backward bit(s).
#define SOpt_Exact	0x0004		// Case-sensitive matches.
#define SOpt_Fuzzy	0x0008		// Fuzzy matches.
#define SOpt_Ignore	0x0010		// Ignore case.
#define SOpt_Multi	0x0020		// Multiline mode ('.' and [^...] match '\n').
#define SOpt_Plain	0x0040		// Plain-text mode.
#define SOpt_Regexp	0x0080		// Regexp mode.
#define SOpt_All	(SOpt_Exact | SOpt_Fuzzy | SOpt_Ignore | SOpt_Multi | SOpt_Plain | SOpt_Regexp)

#define SCpl_PlainExact	0x1000		// Plain text compile done in Exact mode?
#define SCpl_RegExact	0x2000		// RE compile done in Exact mode?
#define SCpl_ForwardRE	0x4000		// Forward RE compiled?
#define SCpl_BackwardRE	0x8000		// Backward RE compiled?

// External function declarations.
extern int boundary(Point *pPoint, int dir);
extern bool bufPlainSearch(void);
extern bool bufRESearch(void);
extern int checkOpts(char *pat, ushort *flags);
extern int compileRE(Match *pMatch, ushort flags);
extern int compileRepl(Match *pMatch);
extern bool exactSearch(Match *pMatch);
extern void freeRE(Match *pMatch);
extern void freeReplPat(Match *pMatch);
extern void freeSearchPat(Match *pMatch);
extern int getPat(Datum **args, const char *prompt, bool searchPat);
extern void grpFree(Match *pMatch);
extern int huntBack(Datum *pRtnVal, int n, Datum **args);
extern int huntForw(Datum *pRtnVal, int n, Datum **args);
extern void initInfoColors(void);
extern void minit(Match *pMatch);
extern void makeDeltas(void);
extern char *makePat(char *dest, Match *pMatch);
extern int newReplPat(const char *pat, Match *pMatch, bool addToRing);
extern int newSearchPat(char *pat, Match *pMatch, ushort *flags, bool addToRing);
extern int regcmp(Datum *pSrc, int scanOffset, Match *pMatch, regmatch_t *result);
extern int regScan(int n, int *pLineBreakLimit, ushort direct, long *pMatchLen);
extern int replStr(Datum *pRtnVal, int n, Datum **args, bool qRepl);
extern int saveMatch(Match *pMatch, MatchLoc *pMatchLoc, RegMatch *pRegMatch);
extern int scan(int n, int *pLineBreakLimit, ushort direct, long *pMatchLen);
extern int searchBack(Datum *pRtnVal, int n, Datum **args);
extern int searchForw(Datum *pRtnVal, int n, Datum **args);
#if MMDebug & Debug_ShowRE
extern int showRegexp(Datum *pRtnVal, int n, Datum **args);
#endif
extern int strIndex(Datum *pRtnVal, ushort flags, Datum *pSrc, Datum *pPat, Match *pMatch);

#ifdef SearchData

// **** For search.c ****

// Global variables.
Match matchRE;				// Match results for =~ and !~ operators and the 'index' function.
SearchCtrl searchCtrl = {
	Ctrl | '['
	};

#else

// **** For all the other .c files ****

// External variable declarations.
extern Match matchRE;
extern SearchCtrl searchCtrl;
#endif
