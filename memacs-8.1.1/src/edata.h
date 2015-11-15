// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// edata.h	Global variable definitions for MightEMacs.

#ifdef DATAmain

// **** For main.c ****

// Global variables (defined in main.c).
Alias *aheadp = NULL;			// Head of alias list.
char *alcaller;
Buffer *bheadp = NULL;			// Head of buffer list.
ModeSpec bmodeinfo[] = {		// Buffer mode table.
	{"c","C",'C',MDC},
	{"col","cOl",'O',MDCOL},
	{"line","Line",'L',MDLINE},
	{"memacs","Memacs",'M',MDMEMACS},
	{"over","oVer",'V',MDOVER},
	{"perl","Perl",'P',MDPERL},
	{"rdonly","Rdonly",'R',MDRDONLY},
	{"repl","rEpl",'E',MDREPL},
	{"ruby","ruBy",'B',MDRUBY},
	{"shell","Shell",'S',MDSHELL},
	{"wrap","Wrap",'W',MDWRAP},
	{"xindt","Xindt",'X',MDXINDT},
	{NULL,NULL,-1,0}};
Buffer *btailp = NULL;			// Tail of buffer list.
char buffer1[] = BUFFER1;		// Name of first buffer ("untitled").
CoreKeys ckeys;
#if COLOR
const char *cname[] = {			// Names of colors.
	"black","red","green","yellow","blue","magenta","cyan","grey",
	"gray","lred","lgreen","lyellow","lblue","lmagenta","lcyan","white"};
#endif
char *copyright = "(c) Copyright 2015 Richard W. Marinelli";
CAMRec *crheadp = NULL;			// Head of CAM list.
Buffer *curbp;				// Current buffer.
EScreen *cursp;				// Current screen.
EWindow *curwp;				// Current window.
#if COLOR
int deskcolor =	0;			// Desktop background color.
#endif
DirName dirtab[] = {			// Directive name table.
	{"if",DIF},
	{"elsif",DELSIF},
	{"else",DELSE},
	{"endif",DENDIF},
	{"return",DRETURN},
	{"macro",DMACRO},
	{"endmacro",DENDMACRO},
	{"while",DWHILE},
	{"until",DUNTIL},
	{"loop",DLOOP},
	{"endloop",DENDLOOP},
	{"break",DBREAK},
	{"next",DNEXT},
	{"force",DFORCE},
	{NULL,0}};
char *execpath = NULL;			// Search path for command files.
int fencepause = FPAUSE;		// Centiseconds to pause for fence matching.
FInfo fi = {				// File I/O information.
	NULL,-1,false,"",-1,-1,"",0U,NULL,0U,NULL,NULL,NULL,"",NULL,NULL
	};
int gasave = NASAVE;			// Global ASAVE size.
#if COLOR
int gbcolor = 0;			// Global backgrnd color (black).
int gfcolor = 7;			// Global forgrnd color (white).
#endif
ModeSpec gmodeinfo[] = {		// Global mode table.
	{"asave","Asave",'A',MDASAVE},
	{"bak","Bak",'B',MDBAK},
	{"clob","Clob",'C',MDCLOB},
	{"esc8","esc8",'8',MDESC8},
	{"exact","Exact",'E',MDEXACT},
	{"hscrl","Hscrl",'H',MDHSCRL},
	{"kecho","Kecho",'K',MDKECHO},
	{"msg","Msg",'M',MDMSG},
	{"noupd","noUpd",'U',MDNOUPD},
	{"rd1st","rd1st",'1',MDRD1ST},
	{"regexp","Regexp",'R',MDREGEXP},
	{"safe","Safe",'S',MDSAFE},
	{"wkdir","wkDir",'D',MDWKDIR},
	{NULL,NULL,-1,0}};
int hjump = 1;				// Horizontal jump size - percentage.
int hjumpcols = 1;			// Horizontal jump size - columns.
HookRec hooktab[] = {			// Hook table with invocation argument(s) in comments.
	{"enter buffer",{PTRNUL,NULL}},	// No arguments.
	{"exit buffer",{PTRNUL,NULL}},	// No arguments.
	{"help",{PTRNUL,NULL}},		// No arguments.
	{"mode",{PTRNUL,NULL}},		// (1), old global modes; (2), old show modes; (3) old default modes;
					// (4), old buffer modes.
	{"post-key",{PTRNUL,NULL}},	// (1), (encoded) key to be executed (same as pre-key).
	{"pre-key",{PTRNUL,NULL}},	// (1), (encoded) key to be executed.
	{"read file",{PTRNUL,NULL}},	// (1), buffer name; (2), filename (or nil if none; i.e., standard input).
	{"word wrap",{PTRCMD,cftab + cf_wrapWord}},	// No arguments.
	{"write file",{PTRNUL,NULL}},	// (1), buffer name; (2), filename.
	{NULL,{PTRNUL,NULL}}
	};
int htabsize = 8;			// Current hard tab size.
char identchars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
IVar ivar = {0,1};			// "i" variable.
KeyEntry kentry = {0,-1,false,0,false,0,0};
					// Terminal key entry variables.
KMacro kmacro = {NULL,NULL,KMSTOP,0};	// Keyboard macro control variables.
Kill kring[NRING];			// Kill ring.
Kill *kringp;				// Current kill ring item.
Kill *kringz = kring + NRING;		// Pointer to end of kill array.
char language[] = LANGUAGE;		// Language of text messages.
Parse *last = NULL;			// Last symbol parsed from a command line.
int lbound = 0;				// Leftmost column of current line being displayed.
#if MMDEBUG
FILE *logfile;				// Log file for debugging.
#endif
char lowcase[HICHAR + 1];		// Upper-to-lower translation table.
int maxloop = MAXLOOP;			// Maximum number of iterations allowed in a loop block.
int maxrecurs = MAXRECURS;		// Maximum depth of recursion allowed during script execution.
MsgLine ml = {				// Message line controls.
	USHRT_MAX,NULL,NULL};
ModeRec modetab[] = {			// Global, show, and default mode settings.
	{MDESC8 | MDEXACT | MDHSCRL | MDKECHO | MDMSG | MDRD1ST,"Global"},
	{MDASAVE | MDBAK | MDEXACT | MDHSCRL | MDNOUPD | MDREGEXP | MDSAFE,"Show"},
	{0,"Default"},
	{0,NULL}};
uint mypid = 0U;			// Unix PID (for temporary filenames).
char myself[] = PROGNAME;		// Common name of program.
uint opflags = OPEVAL | OPSTARTUP | OPSCREDRAW;	// Operation flags (bit masks).
char osname[] = OSNAME;			// Operating system name.
int overlap = 2;			// Overlap when paging on a screen.
#if COLOR
char palstr[NPALETTE + 1] = "";		// Palette string.
#endif
int randseed = 1;			// Random number seed.
RtnCode rc = {SUCCESS,0,""};		// Return code record.
SampBuf sampbuf;			// "Sample" string buffer.
Buffer *sbuffer = NULL;			// Saved buffer pointer.
RtnCode scriptrc = {SUCCESS,0,""};		// Return code record for macro command.
ScriptRun *scriptrun = NULL;		// Running buffer (script) information.
EScreen *sheadp = NULL;			// Head of screen list.
SearchInfo srch = {
	0,CTRL | '[',{NULL,0},0,NULL,0,0,"","",""
	};
int stabsize = 0;			// Current soft tab size (0: use hard tabs).
int stdinfd = -1;			// File descriptor to use for file read from standard input.
EWindow *swindow = NULL;		// Saved window pointer.
char *termp;				// Value of TERM environmental variable.
int tjump = 14;				// Line-traversal jump size.
char upcase[HICHAR + 1];		// Lower-to-upper translation table.
Kill undelbuf = {NULL,NULL,0,KBLOCK};	// Undelete buffer.
long val_defn = INT_MIN;		// Value of defn.
char val_false[] = "_false_";		// Value of false.
char val_nil[] = "_nil_";		// Value of nil.
char val_true[] = "_true_";		// Value of true.
char version[] = VERSION;		// MightEMacs version.
int vjump = 0;				// Vertical jump size (zero for smooth scrolling).
EWindow *wheadp = NULL;			// Head of window list.
char wordlist[256];			// Characters considered "in words".
int wrapcol = 74;			// Current wrap column.

#else	// maindef.

// **** For all the other .c files ****

// External declarations for global variables defined above, with a few exceptions noted.
extern Alias *aheadp;
extern char *alcaller;
extern Buffer *bheadp;
extern char binname[];
extern ModeSpec bmodeinfo[];
extern Buffer *btailp;
extern char buffer1[];
extern CoreKeys ckeys;
#if COLOR
extern const char *cname[];
#endif
extern CAMRec *crheadp;
extern Buffer *curbp;
extern EScreen *cursp;
extern EWindow *curwp;
#if COLOR
extern int deskcolor;
#endif
extern DirName dirtab[];
extern char *execpath;
extern char falsem[];
extern int fencepause;
extern FInfo fi;
extern int gasave;
#if COLOR
extern int gbcolor;
extern int gfcolor;
#endif
extern ModeSpec gmodeinfo[];
extern int hjump;
extern int hjumpcols;
extern HookRec hooktab[];
extern int htabsize;
extern char identchars[];
extern IVar ivar;
extern KeyEntry kentry;
extern KMacro kmacro;
extern Kill kring[];
extern Kill *kringp;
extern Kill *kringz;
extern char language[];
extern Parse *last;
extern int lbound;
#if MMDEBUG
extern FILE *logfile;
#endif
extern char lowcase[];
extern int maxloop;
extern int maxrecurs;
extern MsgLine ml;
extern ModeRec modetab[];
extern uint mypid;
extern char myself[];
extern uint opflags;
extern char osname[];
extern int overlap;
#if COLOR
extern char palstr[];
#endif
extern int randseed;
extern RtnCode rc;
extern SampBuf sampbuf;
extern Buffer *sbuffer;
extern RtnCode scriptrc;
extern ScriptRun *scriptrun;
extern EScreen *sheadp;
extern SearchInfo srch;
extern int stabsize;
extern int stdinfd;
extern EWindow *swindow;
extern char *termp;
extern int tjump;
extern Kill undelbuf;
extern char upcase[];
extern long val_defn;
extern char val_false[];
extern char val_nil[];
extern char val_true[];
extern char version[];
extern int vjump;
extern EWindow *wheadp;
extern char wordlist[];
extern int wrapcol;

#endif	// maindef.

// External declarations for global variables defined in source files other than main.c.
extern ETerm term;		// Terminal table, defined in unix.c.
