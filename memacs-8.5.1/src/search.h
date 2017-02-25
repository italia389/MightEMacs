// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// search.h	Search and replace definitions for MightEMacs.

#define ProgressLoopCt	100000		// Number of search loops which triggers display of progress message.

// Regular expression macro definitions.
//
// HiChar - 1 is the largest character we will deal with.
#define HiChar		256

// Metacharacter element types for searches (in the mc_type member of MetaChar below).  MCE_Nil is used for both search and
// replacement.
#define MCE_Nil		0		// Like the '\0' for strings.
#define MCE_LitChar	1		// Literal character.
#define MCE_Any		2		// Any character but newline.
#define MCE_CCL		3		// Character class.
#define MCE_NCCL	4		// Negated character class.
#define MCE_BOL		5		// Beginning of line.
#define MCE_EOL		6		// End of line.
#define MCE_BOS		7		// Beginning of string.
#define MCE_EOSAlt	8		// End of string or just before trailing newline.
#define MCE_EOS		9		// End of string.
#define MCE_WordBnd	10		// Word boundary.
#define MCE_GrpBegin	11		// Beginning of group.
#define MCE_GrpEnd	12		// End of group.

// Metacharacter element types for replacements (in the mc_type member of ReplMetaChar below).
#define MCE_LitString	13		// Literal string.
#define MCE_Group	14		// String of group match.
#define MCE_Match	15		// String of entire match.

// Element type masks.
#define MCE_Closure	0x0100		// Element has closure.
#define MCE_MinClosure	0x0200		// Match the minimum number possible; otherwise, maximum.
#define MCE_Not		0x0400		// Negate meaning of element type (e.g., not a word boundary).

#define MCE_BaseType	0x00ff

// Metacharacters.
#define MC_Any		'.'		// 'Any' character (except newline).
#define MC_CCLBegin	'['		// Beginning of character class.
#define MC_NCCL		'^'		// Negated character class.
#define MC_CCLRange	'-'		// Range indicator in character class.
#define MC_CCLEnd	']'		// End of character class.
#define MC_BOL		'^'		// Beginning of line.
#define MC_EOL		'$'		// End of line.
#define MC_Closure0	'*'		// Closure - zero or more characters.
#define MC_Closure1	'+'		// Closure - one or more characters.
#define MC_Closure01	'?'		// Closure - zero or one characters, and modifier to match minimum number possible.
#define MC_ClBegin	'{'		// Closure - beginning of count.
#define MC_ClEnd	'}'		// Closure - end of count.
#define MC_Ditto	'&'		// Matched string (in replacement pattern).
#define MC_GrpBegin	'('		// Beginning of group (no backslash prefix).
#define MC_GrpEnd	')'		// End of group (no backslash prefix).
#define MC_Esc		'\\'		// Escape - suppress meta-meaning.

#define MC_BOS		'A'		// Beginning of string (\A).
#define MC_EOSAlt	'Z'		// End of string or just before trailing newline (\Z).
#define MC_EOS		'z'		// End of string (\z).
#define MC_WordBnd	'b'		// Word boundary (\b).
#define MC_NWordBnd	'B'		// Not a word boundary (\B).
#define MC_Tab		't'		// Tab character (\t).
#define MC_CR		'r'		// Carriage return (\r).
#define MC_NL		'n'		// Newline (\n).
#define MC_FF		'f'		// Form feed (\f).
#define MC_Digit	'd'		// Digit [0-9] (\d).
#define MC_NDigit	'D'		// Non-digit [^0-9] (\D).
#define MC_Letter	'l'		// Letter [a-zA-Z] (\l).
#define MC_NLetter	'L'		// Non-letter [^a-zA-Z] (\L).
#define MC_Space	's'		// Space character [ \t\r\n\f] (\s).
#define MC_NSpace	'S'		// Non-space character [^ \t\r\n\f] (\S).
#define MC_Word		'w'		// Word character [a-zA-Z0-9_] (\w).
#define MC_NWord	'W'		// Non-word character [^a-zA-Z0-9_] (\W).

#define OptCh_Begin	':'		// Beginning of options (not part of pattern).
#define OptCh_Exact	'e'		// Case-sensitive matches.
#define OptCh_Ignore	'i'		// Ignore case.
#define OptCh_Multi	'm'		// Multiline mode.
#define OptCh_Plain	'p'		// Plain-text mode.
#define OptCh_Regexp	'r'		// Regexp mode.
#define OptCh_N		6		// Number of option characters.

#define MaxGroups	10		// Maximum number of RE groups + 1 (#0 reserved for entire match).
#define Bit(n)		(0x80 >> (n))	// An 8-bit integer with one bit set.

// Bit map element type.
typedef char EBitMap[HiChar >> 3];

// String "dot" for RE scanning.
typedef struct {
	char *str0;			// Initial string pointer (needed for '^' matching).
	char *str;			// Current string pointer during scan.
	} StrDot;

// Scan "dot" and type definitions.
typedef struct {
	ushort type;
	union {
		Dot bd;			// Buffer dot.
		StrDot sd;		// String dot.
		} u;
	} ScanDot;

#define ScanDot_Buf	0
#define ScanDot_Str	1

// String match-location object.
typedef struct {
	StrDot sd;
	long len;
	} StrLoc;

// Match text.
typedef union {
	Region reg;			// Buffer location of the beginning of the match and length.
	StrLoc str;			// String pointer to the beginning of the match and length.
	} MatchLoc;

// Structure for saving search results for group matches.  "elen" and "ml" are used only during scan; "matchp" is the scan
// result (or NULL if N/A) and remains until the next scan.
typedef struct {
	int elen;			// Length of text matched by whole RE pattern through end of group.
	MatchLoc ml;			// Text in scan object matched by RE group.  ml.reg.r_size and ml.str.len are initially
					// the negated length of text matched by whole RE pattern up to beginning of group.
					// The actual size of the group is computed by adding "elen".
	Datum *matchp;			// Pointer to heap copy of matched string.
	} GrpInfo;

// Meta-character structure for a search pattern element.
typedef struct {
	ushort mc_type;
	union {
		int lchar;
		GrpInfo *ginfo;
		EBitMap *cclmap;
		} u;
	struct {
		short min,max;		// Closure minimum and maximum.
		} cl;
	} MetaChar;

// Meta-character structure for a replacment pattern element.
typedef struct {
	ushort mc_type;
	union {
		int grpnum;
		char *rstr;
		} u;
	} ReplMetaChar;

// Pattern-matching control variables.  Note that grouping items in "mcpat" point to elements in "groups".
typedef struct {
	ushort flags;			// RE flags (below).
	uint ssize;			// Size of search arrays.
	uint rsize;			// Size of replacement arrays.
	Datum *matchp;			// Pointer to heap copy of matched string (taken from groups[0].matchp).
	int grpct;			// Number of groups in RE pattern.
	int patlen;			// Length of search pattern (RE and non-RE) without trailing option characters.
	char *pat;			// Forward search pattern (RE and non-RE) without trailing option characters.
	char *rpat;			// Replacement pattern (RE and non-RE).
	MetaChar *mcpat;		// Forward RE pattern.
	MetaChar *bmcpat;		// Backward (reversed) RE pattern.
	ReplMetaChar *rmcpat;		// Replacement RE pattern.
	GrpInfo groups[MaxGroups];	// Descriptors for each RE group string found in search.  Group 0 contains the location
					// of entire matched text in source object (buffer or string).
	} Match;

// Search control variables -- for search and replace commands.
typedef struct {
	ushort sdelim;			// Search prompt terminator.
	char *bpat;			// Backward (reversed) search pattern (RE and non-RE).
	int fdelta1[HiChar];		// Forward non-RE delta1 table.
	int bdelta1[HiChar];		// Backward non-RE delta1 table.
	int *fdelta2;			// Forward non-RE delta2 table.
	int *bdelta2;			// Backward non-RE delta2 table.
	Match m;			// Match information.
	} SearchInfo;

#define SRegical	0x0001		// Metacharacters found in search string.
#define	RRegical	0x0002		// Metacharacters found in replacement string.

// The following pattern options must be different than Forward and Backward bit(s).
#define SOpt_Exact	0x0004		// Case-sensitive comparisons.
#define SOpt_Ignore	0x0008		// Ignore case in comparisons.
#define SOpt_Multi	0x0010		// Multiline mode ('.' and [^...] match '\n').
#define SOpt_Plain	0x0020		// Plain-text mode.
#define SOpt_Regexp	0x0040		// Regexp mode.
#define SOpt_All	(SOpt_Exact | SOpt_Ignore | SOpt_Multi | SOpt_Plain | SOpt_Regexp)

#define SCpl_Exact	0x0100		// Plain text compile done in Exact mode?

#define SXeq_Exact	0x0200		// Exact mode in effect?
#define SXeq_All	SXeq_Exact

// External function declarations.
extern int boundary(Dot *dotp,int dir);
extern int chkopts(char *pat,ushort *flagsp);
extern bool exactbmode(void);
extern void freerpat(Match *mtp);
extern void freespat(Match *mtp);
extern int getpat(Datum **argpp,char *prmt,bool srchpat);
extern void grpclear(Match *mtp);
extern int huntBack(Datum *rp,int n,Datum **argpp);
extern int huntForw(Datum *rp,int n,Datum **argpp);
extern void mcclear(Match *mtp);
extern int mccompile(Match *mtp);
extern int mcscan(int n,int direct);
extern void mkdeltas(void);
extern char *mkpat(char *dest,Match *mtp);
extern int newrpat(char *pat,Match *mtp);
extern int newspat(char *pat,Match *mtp,ushort *flagsp);
extern bool plainsearch(void);
extern bool rebmode(void);
extern int recmp(Datum *sp,int scanoff,Match *mtp,int *resultp);
extern int replstr(Datum *rp,int n,Datum **argpp);
extern int rmccompile(Match *mtp);
extern int savematch(Match *mtp);
extern int scan(int n,int direct);
extern int searchBack(Datum *rp,int n,Datum **argpp);
extern int searchForw(Datum *rp,int n,Datum **argpp);
#if MMDebug & Debug_ShowRE
extern int showRegexp(Datum *rp,int n,Datum **argpp);
#endif

#ifdef DATAsearch

// **** For search.c ****

// Global variables.
Match rematch;				// Match results for =~ and !~ operators.
SearchInfo srch = {
	Ctrl | '['
	};

#else

// **** For all the other .c files ****

// External variable declarations.
extern Match rematch;
extern SearchInfo srch;
#endif
