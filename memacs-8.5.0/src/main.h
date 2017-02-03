// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// main.h	Common data for MightEMacs defined in main.c.

// External functions not declared elsewhere.
extern int abortinp(void);
extern int abortOp(Datum *rp,int n,Datum **argpp);
extern int aboutMM(Datum *rp,int n,Datum **argpp);
extern void adispose(Datum *datp);
extern int allowedit(bool edit);
extern int apropos(Datum *mstrp,char *prmt,Datum **argpp);
extern int backch(int n);
extern int backln(int n);
extern int bactivate(Buffer *bufp);
extern int bappend(Buffer *bufp,char *text);
extern int bclear(Buffer *bufp,ushort flags);
extern int bcomplete(Datum *rp,char *prmt,char *defname,uint op,Buffer **bufpp,bool *createdp);
extern int bdelete(Buffer *bufp,ushort flags);
extern int begintxt(void);
extern int beline(Datum *rp,int n,bool end);
extern int bextend(Buffer *bufp);
extern int bfind(char *name,ushort cflags,ushort bflags,Buffer **bufpp,bool *createdp);
extern int bftab(int n);
extern void bftowf(struct Buffer *bufp,EWindow *winp);
extern int bhook(Datum *rp,bool exitHook);
extern int binary(char *key,char *(*tval)(int i),int tsize);
extern int bpop(struct Buffer *bufp,bool altmodeline,bool endprompt);
extern int bscratch(char *dest,Buffer **bufpp);
extern Buffer *bsrch(char *bname,Buffer **bufpp);
extern int bswitch(Buffer *bufp);
extern int bufcount(void);
extern long buflength(Buffer *bufp,long *lp);
extern int bufop(Datum *rp,int n,char *prmt,uint op,uint flag);
extern int caseregion(int n,char *trantab);
extern int changedir(Datum *rp,int n,Datum **argpp);
extern int chcase(int ch);
extern int chgtext(Datum *rp,int n,Buffer *bufp,uint t,CmdFunc *cfp);
#if Color
extern int chkcpy(char **destp,char *src,size_t destlen,char *emsg);
#endif
extern int clearBuf(Datum *rp,int n,Datum **argpp);
extern void clearKeyMacro(bool stop);
extern void clfname(Buffer *bufp);
extern int copyreg(Region *regp);
#if WordCount
extern int countWords(Datum *rp,int n,Datum **argpp);
#endif
extern void cpause(int n);
extern ushort ctoek(uint c);
extern int cycle_ring(int n,bool msg);
extern int deleteBuf(Datum *rp,int n,Datum **argpp);
extern int deleteMark(Datum *rp,int n,Datum **argpp);
extern int deleteScreen(Datum *rp,int n,Datum **argpp);
extern int deleteWind(Datum *rp,int n,Datum **argpp);
extern int dkregion(int n,bool kill);
extern int drcset(void);
#if MMDebug & Debug_ScrDump
extern void dumpscreens(char *msg);
#endif
extern ushort ektoc(ushort ek);
extern int eopendir(char *fspec,char **fp);
extern int ereaddir(void);
extern void faceinit(WindFace *wfp,struct Line *lnp,struct Buffer *bufp);
extern EWindow *findwind(Buffer *bufp);
extern int fmatch(ushort ch);
extern int forwch(int n);
extern int forwln(int n);
extern void garbpop(Datum *datp);
extern int getarg(Datum *rp,char *prmt,char *defval,uint terminator,uint maxlen,uint aflags,uint cflags);
extern int getbuflist(Datum *rp,int n);
extern int getccol(void);
extern long getlinenum(Buffer *bufp,Line *targlnp);
extern int getregion(Region *regp,bool *wholebufp);
extern int gettermsize(ushort *colp,ushort *rowp);
extern int getwid(ushort *widp);
extern int getwindlist(Datum *rp,int n);
extern int getwpos(EWindow *winp);
extern int growKeyMacro(void);
extern int gswind(Datum *rp,int n,int how);
extern int indentRegion(Datum *rp,int n,Datum **argpp);
extern void initchars(void);
extern int insertBuf(Datum *rp,int n,Datum **argpp);
extern bool inwind(EWindow *winp,Line *lnp);
extern bool inword(void);
extern uint iortext(Datum *srcp,int n,uint t,bool kill);
extern bool is_lower(int ch);
extern bool is_upper(int ch);
extern bool is_white(Line *lnp,int length);
extern bool isletter(int ch);
extern int joinWind(Datum *rp,int n,Datum **argpp);
extern void kcycle(void);
extern int kdctext(int n,int kdc,Region *regp);
extern void kdelete(Kill *kp);
extern int kinsert(Kill *kp,int direct,int c);
extern void kprep(bool kill);
extern int lalloc(int used,Line **lnpp);
extern void lchange(Buffer *bufp,ushort flags);
extern int ldelete(long n,ushort flags);
extern void lfree(Line *lnp);
extern int linsert(int n,int c);
extern int linstr(char *s);
extern int lnewline(void);
#if Color
extern int lookup_color(char *str);
#endif
extern int markBuf(Datum *rp,int n,Datum **argpp);
extern void mdelete(Buffer *bufp,ushort flags);
extern int mfind(ushort id,Mark **mkpp,ushort flags);
extern char *mklower(char *dest,char *src);
extern char *mkupper(char *dest,char *src);
extern int mlerase(ushort flags);
extern int mlprintf(ushort flags,char *fmt,...);
extern int mlputc(ushort flags,int c);
extern int mlputs(ushort flags,char *str);
extern int mlputv(ushort flags,Datum *datp);
extern int mlrestore(void);
extern int mlyesno(char *prmt,bool *resultp);
extern int movecursor(int row,int col);
extern int moveWindUp(Datum *rp,int n,Datum **argpp);
extern void mset(Mark *mkp,EWindow *winp);
extern int narrowBuf(Datum *rp,int n,Datum **argpp);
extern int newcol(int c,int col);
extern int newScreen(Datum *rp,int n,Datum **argpp);
extern int nextScreen(Datum *rp,int n,Datum **argpp);
extern int nextWind(Datum *rp,int n,Datum **argpp);
extern int onlyWind(Datum *rp,int n,Datum **argpp);
extern int otherfence(Region *regp);
extern int outdentRegion(Datum *rp,int n,Datum **argpp);
extern int overprep(int n);
extern char *pad(char *s,uint len);
extern int pathsearch(char **destp,char *name,bool hflag);
extern int pipeBuf(Datum *rp,int n,Datum **argpp);
extern int pnbuffer(Datum *rp,int n,bool prev);
extern int prevWind(Datum *rp,int n,Datum **argpp);
extern int rcclear(void);
extern int rcsave(void);
extern int rcset(int status,ushort flags,char *fmt,...);
extern int readBuf(Datum *rp,int n,Datum **argpp);
extern char *regcpy(char *buf,Region *regp);
#if 0
extern int regdatcpy(Datum *destp,Region *regp);
#endif
extern int reglines(int *np,bool *allp);
extern int render(Datum *rp,int n,struct Buffer *bufp,ushort flags);
extern int resetTermc(Datum *rp,int n,Datum **argpp);
extern int resizeWind(Datum *rp,int n,Datum **argpp);
extern int restorecursor(void);
extern int runcmd(Datum *dsinkp,DStrFab *cmdp,char *cname,char *arg,bool fullQuote);
extern void savecursor(void);
extern int scratchBuf(Datum *rp,int n,Datum **argpp);
extern int scrcount(void);
extern int selectBuf(Datum *rp,int n,Datum **argpp);
extern int setBufName(Datum *rp,int n,Datum **argpp);
extern int setccol(int pos);
extern int setfname(Buffer *bufp,char *fname);
extern int seti(Datum *rp,int n,Datum **argpp);
extern int setMark(Datum *rp,int n,Datum **argpp);
extern int setpath(char *path,bool prepend);
extern void settermsize(ushort ncol,ushort nrow);
extern int sfind(ushort scr_num,struct Buffer *bufp,EScreen **spp);
extern int showBuffers(Datum *rp,int n,Datum **argpp);
extern int showKillRing(Datum *rp,int n,Datum **argpp);
extern int showMarks(Datum *rp,int n,Datum **argpp);
extern int showScreens(Datum *rp,int n,Datum **argpp);
#if Color
extern int spal(char *cmd);
#endif
extern int spanwhite(bool end);
extern int splitWind(Datum *rp,int n,Datum **argpp);
extern int sswitch(EScreen *scrp);
extern char *strrev(char *s);
extern char *strsamp(char *src,size_t srclen,size_t maxlen);
extern int swapMark(Datum *rp,int n,Datum **argpp);
extern int swapmid(ushort id);
extern int sysbuf(char *root,Buffer **bufpp);
extern int tabstop(int n);
extern int terminp(Datum *rp,char *prmt,char *defval,uint terminator,uint maxlen,uint aflags,uint cflags);
extern char *timeset(void);
extern void tungetc(ushort ek);
extern int typahead(int *countp);
extern int update(bool force);
extern void uphard(void);
extern void upmode(struct Buffer *bufp);
extern int vtinit(void);
extern int vttidy(bool force);
extern void wftobf(EWindow *winp,struct Buffer *bufp);
extern int widenBuf(Datum *rp,int n,Datum **argpp);
extern int wincount(void);
extern EWindow *wnextis(EWindow *winp);
extern int wscroll(Datum *rp,int n,int (*winfunc)(Datum *rp,int n,Datum **argpp),
 int (*pagefunc)(Datum *rp,int n,Datum **argpp));
extern void wswitch(EWindow *winp);
extern void wupd_modeline(EWindow *winp,struct Buffer *popbuf);
extern bool wupd_newtop(EWindow *winp,struct Line *lnp,int n);
extern long xorshift64star(long max);
extern int yankPop(Datum *rp,int n,Datum **argpp);

#ifdef DATAmain

// **** For main.c ****

// Forward.
CmdFunc cftab[];

// Global variables.
Buffer *bheadp = NULL;			// Head of buffer list.
ModeSpec bmodeinfo[] = {		// Buffer mode table.
	{"c","C",MdC,MLit_ModeC},
	{"col","Col",MdCol,MLit_ModeColDisp},
	{"line","Line",MdLine,MLit_ModeLineDisp},
	{"memacs","Memacs",MdMemacs,MLit_ModeMEMacs},
	{"over","Over",MdOver,MLit_ModeOver},
	{"perl","Perl",MdPerl,MLit_ModePerl},
	{"rdonly","RdOnly",MdRdOnly,MLit_ModeReadOnly},
	{"repl","Repl",MdRepl,MLit_ModeReplace},
	{"ruby","Ruby",MdRuby,MLit_ModeRuby},
	{"shell","Shell",MdShell,MLit_ModeShell},
	{"wrap","Wrap",MdWrap,MLit_ModeWrap},
	{"xindt","XIndt",MdXIndt,MLit_ModeExtraIndent},
	{NULL,NULL,0,NULL}};
Buffer *btailp = NULL;			// Tail of buffer list.
char buffer1[] = Buffer1;		// Name of first buffer ("untitled").
char *Copyright = "(c) Copyright 2017 Richard W. Marinelli";
Buffer *curbp;				// Current buffer.
EScreen *cursp;				// Current screen.
EWindow *curwp;				// Current window.
#if Color
const char *cname[] = {			// Names of colors.
	"black","red","green","yellow","blue","magenta","cyan","grey",
	"gray","lred","lgreen","lyellow","lblue","lmagenta","lcyan","white"};
int deskcolor =	0;			// Desktop background color.
#endif
int fencepause = FPause;		// Centiseconds to pause for fence matching.
int gacount = NASave;			// Global ASave counter (for all buffers).
int gasave = NASave;			// Global ASave size.
#if Color
int gbcolor = 0;			// Global backgrnd color (black).
int gfcolor = 7;			// Global forgrnd color (white).
#endif
ModeSpec gmodeinfo[] = {		// Global mode table.
	{"asave","ASave",MdASave,MLit_ModeAutoSave},
	{"bak","Bak",MdBak,MLit_ModeBackup},
	{"clob","Clob",MdClob,MLit_ModeClobber},
	{"esc8","Esc8",MdEsc8,MLit_ModeEsc8Bit},
	{"exact","Exact",MdExact,MLit_ModeExact},
	{"hscrl","HScrl",MdHScrl,MLit_ModeHorzScroll},
	{"msg","Msg",MdMsg,MLit_ModeMsgDisp},
	{"noupd","NoUpd",MdNoUpd,MLit_ModeNoUpdate},
	{"regexp","Regexp",MdRegexp,MLit_ModeRegexp},
	{"safe","Safe",MdSafe,MLit_ModeSafeSave},
	{"wkdir","WkDir",MdWkDir,MLit_ModeWorkDir},
	{NULL,NULL,0,NULL}};
int hjump = 1;				// Horizontal jump size - percentage.
int hjumpcols = 1;			// Horizontal jump size - columns.
HookRec hooktab[] = {			// Hook table with invocation argument(s) in comments.
	{"chDir",HLit_chDir,NULL},	// No arguments.
	{"enterBuf",HLit_enterBuf,NULL},// No arguments.
	{"exitBuf",HLit_exitBuf,NULL},	// No arguments.
	{"help",HLit_help,NULL},	// No arguments.
	{"mode",HLit_mode,NULL},	// (1), old global modes; (2), old show modes;
					// (3) old default modes; (4), old buffer modes.
	{"postKey",HLit_postKey,NULL},	// (1), (encoded) key to be executed (same as pre-key).
	{"preKey",HLit_preKey,NULL},	// (1), (encoded) key to be executed.
	{"read",HLit_read,NULL},	// (1), buffer name; (2), filename (or nil if none; i.e., standard input).
	{"wrap",HLit_wrap,NULL},	// No arguments.
	{"write",HLit_write,NULL},	// (1), buffer name; (2), filename.
	{NULL,NULL,NULL}
	};
int htabsize = 8;			// Current hard tab size.
IVar ivar = {0,1};			// "i" variable.
KMacro kmacro = {
	0,NULL,NULL,KMStop,0,NULL};	// Keyboard macro control variables.
Kill kring[NRing];			// Kill ring.
Kill *kringp;				// Current kill ring item.
Kill *kringz = kring + NRing;		// Pointer to end of kill array.
Buffer *lastbufp = NULL;		// Last buffer exited from.
int lbound = 0;				// Leftmost column of current line being displayed.
#if MMDebug
FILE *logfile;				// Log file for debugging.
#endif
char lowcase[HiChar + 1];		// Upper-to-lower translation table.
MsgLine ml = {				// Message line controls.
	USHRT_MAX,NULL,NULL};
ModeRec modetab[] = {			// Global, show, and default mode settings.
	{MdEsc8 | MdExact | MdHScrl | MdMsg,"Global"},
	{MdASave | MdBak | MdExact | MdHScrl | MdNoUpd | MdRegexp | MdSafe,"Show"},
	{0,"Default"},
	{0,NULL}};
uint mypid = 0U;			// Unix PID (for temporary filenames).
char Myself[] = ProgName;		// Common name of program.
uint opflags = OpEval | OpStartup | OpScrRedraw;	// Operation flags (bit masks).
int overlap = 2;			// Overlap when paging on a screen.
#if Color
char palstr[NPalette + 1] = "";		// Palette string.
#endif
ulong randseed = 1;			// Random number seed.
RtnCode rc = {Success,0,""};		// Return code record.
SampBuf sampbuf;			// "Sample" string buffer.
Buffer *savbufp = NULL;			// Saved buffer pointer.
EWindow *savwinp = NULL;		// Saved window pointer.
EScreen *sheadp = NULL;			// Head of screen list.
int stabsize = 0;			// Current soft tab size (0: use hard tabs).
char *termnam;				// Value of TERM environmental variable.
int tjump = 12;				// Line-traversal jump size.
char upcase[HiChar + 1];		// Lower-to-upper translation table.
Kill undelbuf = {NULL,NULL,0,KBlock};	// Undelete buffer.
char Version[] = ProgVer;		// MightEMacs version.
char viz_false[] = "false";		// Visual form of false.
char viz_nil[] = "nil";			// Visual form of nil.
char viz_true[] = "true";		// Visual form of true.
int vjump = 0;				// Vertical jump size (zero for smooth scrolling).
EWindow *wheadp = NULL;			// Head of window list.
int wrapcol = 74;			// Current wrap column.

#else

// **** For all the other .c files ****

// External declarations for global variables defined above, with a few exceptions noted.
extern Buffer *bheadp;
extern ModeSpec bmodeinfo[];
extern Buffer *btailp;
extern char buffer1[];
#if Color
extern const char *cname[];
#endif
extern Buffer *curbp;
#if Color
extern int deskcolor;
#endif
extern EScreen *cursp;
extern EWindow *curwp;
extern int fencepause;
extern int gacount;
extern int gasave;
#if Color
extern int gbcolor;
extern int gfcolor;
#endif
extern ModeSpec gmodeinfo[];
extern int hjump;
extern int hjumpcols;
extern HookRec hooktab[];
extern int htabsize;
extern IVar ivar;
extern KMacro kmacro;
extern Kill kring[];
extern Kill *kringp;
extern Kill *kringz;
extern Buffer *lastbufp;
extern int lbound;
#if MMDebug
extern FILE *logfile;
#endif
extern char lowcase[];
extern MsgLine ml;
extern ModeRec modetab[];
extern uint mypid;
extern char Myself[];
extern uint opflags;
extern int overlap;
#if Color
extern char palstr[];
#endif
extern ulong randseed;
extern RtnCode rc;
extern SampBuf sampbuf;
extern Buffer *savbufp;
extern EWindow *savwinp;
extern ulong seedinit(void);
extern EScreen *sheadp;
extern int stabsize;
extern char *termnam;
extern int tjump;
extern Kill undelbuf;
extern char upcase[];
extern char Version[];
extern char viz_false[];
extern char viz_nil[];
extern char viz_true[];
extern int vjump;
extern EWindow *wheadp;
extern int wrapcol;
#endif

// External declarations for global variables defined in C source files other than main.c.
extern ETerm term;		// Terminal table, defined in unix.c.
