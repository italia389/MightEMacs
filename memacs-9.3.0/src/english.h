// (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// english.h	English language text strings for MightEMacs.

// General text literals.
#define Literal1	"[arg,...]"
#define Literal2	"name = cfm"
#define Literal3	"mode,..."
#define Literal4	"name"
#define Literal5	"name,arg,..."
#define Literal6	"[pat]"
#define Literal7	"key-lit,name"
#define Literal8	"name,..."
#define Literal9	"[name]"
#define Literal10	"arg,..."
#define Literal11	"pat1,pat2"
#define Literal12	"str"
#define Literal13	"expr"
#define Literal14	"[val[,incr[,fmt]]]"
#define Literal15	"path"
#define Literal16	"key-lit"
#define Literal17	"name[,arg,...]"
#define Literal18	"str,from,to"
#define Literal19	"str,{pat | c}"
#define Literal20	"dlm,val,..."
#define Literal21	"N"
#define Literal22	"[N]"
#define Literal23	"array"
#define Literal24	"p[,def[,t[,dlm]]]"
#define Literal25	"array,expr"
#define Literal26	"[-]pos[,[-]len]"
#define Literal27	"str,[-]pos[,[-]len]"
#define Literal28	"str,n"
#define Literal29	"[size[,val]]"
#define Literal30	"chars"
#define Literal31	"pref,chars"
#define Literal32	"fmt,..."
#define Literal33	"name,cm"
#define Literal34	"N[,name]"
#define Literal35	"m"
#define Literal36	"file,type"
#define Literal37	"[name,...]"
#define Literal38	"dir"
#define Literal39	"dlm,str[,limit]"
#define Literal40	"var,dlm"
#define Literal41	"var,dlm,val"
#define Literal42	"pat"
#define Literal43	"name[,opt]"
#define Literal44	"kw"
#define Literal45	"name,mode,..."
#define Literal46	"name[,attr,...]"
#define Literal47	"array,expr,..."
#define Literal48	"name[,mode,...]"
#define Literal49	"[mode,...]"
#define Literal50	"name1[,name2]"
#define Literal51	"[name,]filename"
#define Literal52	"[m]"
#define Literal53	"name,fmt,..."
#define Literal54	"mode[,attr,...]"
#define Literal55	"group[,attr,...]"
#define Literal56	"group[,name]"
#define Literal57	"{key-lit | name}"
#define Literal58	"[c]"
#define Literal59	"{file | array}"
#define Literal60	"{file | glob}"
#define Literal61	"c"
#define Literal62	"opts,arg,..."
#define Literal63	"tab-size"
#define Literal64	"name,attr,..."
#define Literal65	"pair-no,fg-color,bg-color"
#define Literal66	"kw,colors"

#if defined(DATAmain)

// **** For main.c ****

// Help text.
char
	*Help0 = "    Ver. ",
	*Help1 = " -- Fast and full-featured Emacs text editor\n\n",
	*Usage[] = {
		"Usage:\n    ",
		" {-?, -usage | -C, -copyright | -help | -V, -version}\n    ",
		" [-d, -dir d] [-e, -exec stmt] [-G, -global-mode [^]m[,...]]\n    "
		" [-i, -inp-delim d] [-N, -no-read] [-n, -no-startup] [-o, -otp-delim d]\n    "
		" [-path list] [-r] [-S, -shell] [[@]file [{+|-}line]\n    "
		" [-B, -buf-mode [^]m[,...]] [-r | -rw] [-s, -search pat] ...]"},
	*Help2 = "\n\n\
Command switches and arguments:\n\
  -?, -usage	    Display usage and exit.\n\
  -C, -copyright    Display copyright information and exit.\n\
  -d, -dir d	    Specify a working directory to change to before reading\n\
		    files.\n\
  -e, -exec stmt    Specify an expression statement to be executed.  Switch may\n\
		    be repeated.\n\
  -G, -global-mode [^]m[,...]\n\
  		    Specify one or more global modes to clear (^) or set (no ^),\n\
		    separated by commas.  Mode names may be abbreviated and\n\
		    switch may be repeated.\n\
  -help		    Display detailed help and exit.\n\
  -i, -inp-delim d  Specify one or two characters (processed as a double-quoted\n\
		    \"...\" string) to use as file input record delimiter(s);\n\
		    otherwise, the first NL, CR-LF, or CR found when a file is\n\
		    read is used.\n\
  -N, -no-read	    Do not read first file at startup.\n\
  -n, -no-startup   Do not load the site or user startup file.\n\
  -o, -otp-delim d  Specify one or two characters (processed as a double-quoted\n\
		    \"...\" string) to use as file output record delimiter(s);\n\
		    otherwise, delimiter(s) found in input file ($BufInpDelim)\n\
		    is used.\n\
  -path list	    Specify colon-separated list of script search directories to\n\
		    prepend to existing path.\n\
  -r		    Read-only: open all files with RdOnly buffer mode ON by\n\
		    default; otherwise, OFF.\n\
  -S, -shell	    Denote file (beginning with \"#!/usr/local/bin/memacs -S\")\n\
		    as an executable " ProgName " script.  Shell arguments are\n\
		    passed to script when file is executed.\n\
  -V, -version	    Display program version and exit.\n\
\n  @file		    Execute specified script file (in path) before processing\n\
		    argument(s) that follow it.  Multiple script files may be\n\
		    specified intermixed with data files.\n\
  file		    File to open for viewing or editing.  If \"-\" is specified\n\
		    as the filename, data is read from standard input into\n\
		    buffer \"" Buffer1 "\".  Multiple data files may be specified\n\
		    intermixed with script files.\n\
Argument switches:\n\
  {+|-}line	    Go to specified line number from beginning (+) or end (-) of\n\
		    data file, or to end of file if line number is zero.\n\
  -B, -buf-mode [^]m[,...]\n\
		    Specify one or more buffer modes to clear (^) or set (no ^)\n\
		    on data file, separated by commas.  Mode names may be\n\
		    abbreviated and switch may be repeated.\n\
  -r		    Read-only: open data file with RdOnly buffer mode ON.\n\
  -rw		    Read-write: open data file with RdOnly buffer mode OFF\n\
		    (overrides command-level -r switch).\n\
  -s, -search pat   Search for specified pattern in data file.\n\
\nNotes:\n\
 1. If the -no-read switch is not specified, one data file is read after all\n\
    switches and arguments are processed, determined as follows: if the + or -\n\
    (go to line) or -search switch is specified on a data file argument, the\n\
    corresponding file is read and the action is performed; otherwise, the first\n\
    data file specified is read.\n\
 2. The -r and -rw argument switches may not both be specified.\n\
 3. The script execution path is initialized to the MMPATH environmental\n\
    variable if it is defined; otherwise, \"" MMPath "\".  It may subsequently\n\
    be changed by the -path switch, -exec switch, or a @file script\n\
    invocation.";

// General text messages.
char
 text0[] = "Error",
 text1[] = "-r and -rw switch conflict",
 text2[] = "Invalid character range '%.3s' in string '%s'",
 text3[] = "%s(): Unknown ID %d for variable '%s'!",
 text4[] = "%s expected (at token '%s')",
 text5[] = "Group number %ld must be between 0 and %d",
 text6[] = "About",
 text7[] = "Go to",
 text8[] = "Aborted",
 text9[] = "Mark ",
 text10[] = "deleted",
 text11[] = "No mark ~u%c~U in this buffer",
 text12[] = "%s (%d) must be between %d and %d",
 text13[] = "Show key ",
 text14[] = "~#u%s~U not bound",
 text15[] = "Bind key ",
 text16[] = "%s(): No such entry '%s' to update or delete!",
 text17[] = "Cannot bind key sequence ~#u%s~U to '~b%s~B' command",
 text18[] = "Unbind key ",
 text19[] = "No such ring entry %d (max %d)",
 text20[] = "Apropos",
 text21[] = "system variable",
 text22[] = "Extraneous token '%s'",
 text23[] = "Missing '=' in ~b%s~B command (at token '%s')",
 text24[] = "Select",
 text25[] = "That name is already in use.  New name",
 text26[] = "Delete",
 text27[] = "Pop",
 text28[] = "%s is being displayed",
 text29[] = "Rename",
 text30[] = "ACHMCPNT   Size      Buffer              File",
 text31[] = "Active",
 text32[] = "Discard changes",
 text33[] = "Cannot get %s of file \"%s\": %s",
 text34[] = "File: ",
 text35[] = "$autoSave not set",
 text36[] = "Invalid pattern option '%c' for '%s' operator",
 text37[] = "pathname",
 text38[] = "Invalid number '%s'",
 text39[] = "%s (%d) must be %d or greater",
 text40[] = "Script file \"%s\" not found%s",
 text41[] = "Enter ~#u%s~U for help, ~#u%s~U to quit",
 text42[] = "%s ring cycled",
 text43[] = "-%s switch: ",
 text44[] = "calling %s() from %s() function",
 text45[] = "Multiple \"-\" arguments",
 text46[] = "Input",
 text47[] = "Output",
 text48[] = "[Not bound]",
 text49[] = "Hard",
 text50[] = "Soft",
 text51[] = "Assign variable",
 text52[] = "No such variable '%s'",
 text53[] = "Value",
 text54[] = "Key echo required for completion",
 text55[] = "Insert",
 text56[] = "user variable",
 text57[] = "Only one screen",
 text58[] = "Buffer",
 text59[] = "Wrap column",
 text60[] = "Line %lu/%lu, Col %d/%d, Char %lu/%lu (%s%%), char = %s",
 text61[] = "%sconflicts with -no-read",
 text62[] = "User defined",
 text63[] = " mode",
 text64[] = "Set",
 text65[] = "Clear",
 text66[] = "Unknown or ambiguous mode '%s'",
 text67[] = "Function call",
 text68[] = "Identifier",
 text69[] = "Wrong number of arguments (at token '%s')",
 text70[] = "Region copied",
 text71[] = "%s '%s' is already narrowed",
 text72[] = "Cannot execute a narrowed buffer",
 text73[] = "%s narrowed",
 text74[] = "%s '%s' is not narrowed",
 text75[] = "%s widened",
 text76[] = " (key)",
 text77[] = "%s() bug: Mark '%c' not found in buffer '%s'!",
 text78[] = "Search",
 text79[] = "Not found",
 text80[] = "No pattern set",
 text81[] = "Reverse search",
 text82[] = "Variable name",
 text83[] = "buffer",
 text84[] = "Replace",
 text85[] = "Query replace",
 text86[] = "with",
 text87[] = "~bReplace~B \"",
 text88[] = "Cannot read directory \"%s\": %s",
 text89[] = "ScreenWindow  Buffer              File",
 text90[] = "~uSPC~U|~uy~U ~bYes~B, ~un~U ~bNo~B, ~uY~U ~bYes and stop~B, ~u!~U ~bDo rest~B, ~uu~U ~bUndo last~B,\
 ~uESC~U|~uq~U ~bStop here~B, ~u.~U ~bStop and go back~B, ~u?~U ~bHelp~B",
 text91[] = "Repeating match at same position detected",
 text92[] = "%d substitution%s",
 text93[] = "Current window too small to shrink by %d line%s",
 text94[] = "%s(): Out of memory!",
 text95[] = "Narrowed buffer... delete visible portion",
 text96[] = "Empty character class",
 text97[] = "Character class not ended",
 text98[] = "Wrap column not set",
 text99[] = "file",
#if WordCount
 text100[] = "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f",
#endif
 text101[] = "%sconflict",
 text102[] = "Beginning value",
 text103[] = "Saving %s...",
 text104[] = "Modified buffers exist.  Leave anyway",
 text105[] = "Keyboard macro already active, cancelled",
 text106[] = "Begin keyboard macro",
 text107[] = "Keyboard macro not active",
 text108[] = "End keyboard macro",
 text109[] = "%s is in read-only mode",
 text110[] = "Default value length (%lu) exceeds maximum input length (%u)",
 text111[] = "'~b%s~B' value must be %d or greater",
 text112[] = "Maximum number of loop iterations (%d) exceeded!",
 text113[] = "Switch to",
 text114[] = "Prior loop execution level not found while rewinding stack",
 text115[] = "Word",
 text116[] = "No such macro '%s'",
 text117[] = "Execute",
 text118[] = "No such buffer '%s'",
 text119[] = "Pause duration",
 text120[] = "'break' or 'next' outside of any loop block",
 text121[] = "Unmatched '~b%s~B' keyword",
 text122[] = "loop",
 text123[] = "Unterminated string %s",
 text124[] = "Cannot insert current buffer into itself",
 text125[] = "Completions",
 text126[] = "Script loop boundary line not found",
 text127[] = "alias",
 text128[] = "Invalid buffer name '%s'",
 text129[] = "Execute script file",
 text130[] = "No such command or macro '%s'",
 text131[] = "Read file",
 text132[] = "Insert file",
 text133[] = "Find file",
 text134[] = "View file",
 text135[] = "Old buffer",
 text136[] = " in path \"%s\"",
 text137[] = "Repeat count",
 text138[] = "New file",
 text139[] = "Reading data...",
 text140[] = "Read",
 text141[] = "I/O Error: %s, file \"%s\"",
 text142[] = "Cannot change a character past end of buffer '%s'",
 text143[] = "Line number",
 text144[] = "Write file",
 text145[] = "No filename associated with buffer '%s'",
 text146[] = "global",
 text147[] = "%s buffer '%s'... write it out",
 text148[] = "Writing data...",
 text149[] = "Wrote",
 text150[] = "(file saved as \"%s\") ",
 text151[] = "Set filename in",
 text152[] = "No such file \"%s\"",
 text153[] = "Inserting data...",
 text154[] = "Inserted",
 text155[] = "%s(): Out of memory saving filename \"%s\"!",
 text156[] = "%s(): Out of memory allocating %u-byte line buffer for file \"%s\"!",
 text157[] = "%s(): key map space (%d entries) exhausted!",
 text158[] = "command",
 text159[] = "Buffers",
 text160[] = "Screens",
 text161[] = " (disabled '%s' hook)",
 text162[] = " (y/n)?",
 text163[] = "status",
 text164[] = "Cannot modify read-only variable '~b%s~B'",
 text165[] = "Name '~b%s~B' already in use",
 text166[] = "Integer expected",
 text167[] = "%u buffer%s saved",
 text168[] = "Delete all other visible buffers",
 text169[] = "Clear",
 text170[] = "Read pipe",
 text171[] = "String expected",
 text172[] = "Token expected",
 text173[] = "Unbalanced %c%c%c string parameter",
 text174[] = "Created screen %hu",
 text175[] = "-search, + or - switch ",
 text176[] = "Macro '~b%s~B' failed",
 text177[] = "%s(): Screen number %d not found in screen list!",
 text178[] = "Screen %d deleted",
 text179[] = "Fence char",
 text180[] = "create",
 text181[] = "%s name '%s' already in use",
 text182[] = "Environmental variable 'TERM' not defined!",
 text183[] = "Unknown terminal type '%s'!",
 text184[] = "Overlap %ld must be between 0 and %d",
 text185[] = "Version",
 text186[] = "attribute",
 text187[] = "%s cannot be null",
 text188[] = "End",
 text189[] = "Abort",
 text190[] = "Terminal size %hu x %hu is too small to run %s",
 text191[] = "Wrong type of operand for '%s'",
 text192[] = "This terminal (%s) does not have sufficient capabilities to run %s",
 text193[] = "Saved file \"%s\"\n",
 text194[] = "Shell command \"%s\" failed",
 text195[] = "Endless recursion detected (array contains itself)",
 text196[] = "endloop",
 text197[] = "endmacro",
 text198[] = "Misplaced '~b%s~B' keyword",
 text199[] = "if",
 text200[] = "No keyboard macro defined",
 text201[] = "End: ",
 text202[] = "%s ring is empty",
 text203[] = "Searching...",
 text204[] = "Too many arguments for '[bs]printf' function",
 text205[] = "line",
 text206[] = "Macro argument count",
 text207[] = "Cannot get %d line%s from adjacent window",
 text208[] = "Saved %s not found",
 text209[] = "Arg: %d",
 text210[] = "~#u%s~U is only binding to core command '~b%s~B' -- cannot delete or reassign",
 text211[] = "Screen refreshed",
 text212[] = "Variable '~b%s~B' not an integer",
 text213[] = "Comma",
 text214[] = "%s cannot be nil",
 text215[] = "Create alias ",
 text216[] = "Delete macro",
 text217[] = "'break' level '%ld' must be 1 or greater",
 text218[] = "Append file",
 text219[] = "Script failed",
 text220[] = "Parent block of loop block at line %ld not found during buffer scan",
 text221[] = "Too many groups in RE pattern '%s' (maximum is %d)",
 text222[] = "RE group not ended",
 text223[] = "Options (~u~ba~Zlt mode line, ~u~bd~Zelete buffer, ~u~bs~Zhift long lines, ~u~bt~Zermattr)",
 text224[] = "Opts (~u~ba~Zltml, ~u~bd~Zel, ~u~bs~Zhift, ~u~bt~Zattr)",
 text225[] = "Too many break levels (%d short) from inner 'break'",
 text226[] = "Cannot %s %s buffer '%s'",
 text227[] = "Terminal dimensions set to %hu x %hu",
 text228[] = "%s ring cleared",
 text229[] = ", in",
 text230[] = "at line",
 text231[] = "Toggle",
 text232[] = "Macro name '~b%s~B' cannot exceed %d characters",
 text233[] = "Mark ~u%c~U set to previous position",
 text234[] = "Increment",
 text235[] = "Format string",
 text236[] = "i increment cannot be zero!",
 text237[] = "Invalid i format '%s' (must contain exactly one %%d, %%o, %%u, %%x, or %%X)",
 text238[] = "executing",
 text239[] = "No such window '%d'",
 text240[] = "No such screen '%d'",
 text241[] = "Maximum ring size (%ld) less than current size (%d)",
 text242[] = "Invalid argument for 'o' or 'O' prompt type",
 text243[] = "\n* Changed buffer",
 text244[] = "No such command, alias, or macro '%s'",
 text245[] = "Division by zero is undefined (%ld/0)",
 text246[] = "Help hook not set",
 text247[] = "function",
 text248[] = "an executing",
 text249[] = "Insert pipe",
 text250[] = "Unknown pop option '%s'",
 text251[] = "%s delimiter '%s' cannot be more than %d character(s)",
 text252[] = " delimited by ",
 text253[] = "(return status %d)\n",
 text254[] = "Invalid key literal '%s'",
 text255[] = "Invalid repetition operand in RE pattern '%s'",
 text256[] = "%s tab size %d must be between 2 and %d",
 text257[] = ", original file renamed to \"",
 text258[] = "Unmatched right paren in RE pattern '%s'",
 text259[] = "No text selected",
 text260[] = "changed",
 text261[] = "Text",
 text262[] = "copied",
 text263[] = "delete",
 text264[] = "clear",
 text265[] = "Cannot change directory to \"%s\": %s",
 text266[] = "Regular expression",
 text267[] = "command or macro",
 text268[] = "Cannot %s buffer: name '%s' cannot begin with %c",
 text269[] = "Delete alias",
 text270[] = "Cannot %s macro buffer: name '%s' must begin with %c",
 text271[] = "No such alias '%s'",
 text272[] = "Macro has %hu alias(es)",
 text273[] = "Macro buffer names (only) begin with ",
 text274[] = "WD: ",
 text275[] = "rename",
 text276[] = "modify",
 text277[] = "Change directory",
 text278[] = " set to ",
 text279[] = "Variable",
 text280[] = "%s name cannot be null or exceed %d characters",
 text281[] = "Missing spec in '%' format string",
 text282[] = "'~b%s~B' command not allowed in a script (use '~brun~B')",
 text283[] = ", new value",
 text284[] = "Cannot %s %s buffer",
 text285[] = "Call argument",
 text286[] = "Invalid identifier '%s'",
 text287[] = "i variable set to %d",
 text288[] = "Character expected",
 text289[] = "Unexpected token '%s'",
 text290[] = "Length argument",
 text291[] = " without delimiter at EOF",
 text292[] = "variable",
 text293[] = "Cannot split a %d-line window",
 text294[] = "Only one window",
 text295[] = "Prompt type '%s' must be one of \"bcfKOoRSsVv\"",
 text296[] = "Clear all and set",
 text297[] = "Current value: ",
 text298[] = "No previous wrap column set",
 text299[] = "Pop file",
 text300[] = "False return",
 text301[] = " (expression)",
 text302[] = "No such group (ref: %d, have: %d) in replacement pattern '%s'",
 text303[] = "Invalid wrap prefix \"%s\"",
 text304[] = "Closure on group not supported in RE pattern '%s'",
 text305[] = "%sRing",
 text306[] = "Pipe command",
 text307[] = "%d line%s %s",
 text308[] = "Narrowed",
 text309[] = "Directory unknown for",
 text310[] = "Cannot save multi-homed buffer '%s'",
 text311[] = " pattern",
 text312[] = "No such command, function, or macro '%s'",
 text313[] = "command, function, or macro",
 text314[] = "Cannot run ",
 text315[] = " Hook     Set to Macro         Numeric Argument     Macro Arguments",
 text316[] = "Hooks",
 text317[] = "No such hook '%s'",
 text318[] = "Set hook ",
 text319[] = "Maximum %s recursion depth (%d) exceeded",
 text320[] = "More than one spec in '%' format string",
 text321[] = "Unknown format spec '%%%s'",
 text322[] = "Column number",
 text323[] = "Indentation exceeds wrap column (%d)",
 text324[] = ".  New name",
 text325[] = "of",
 text326[] = "Begin",
 text327[] = "Macro bound to '%s' hook - cannot delete",
 text328[] = "tr \"from\" string",
 text329[] = "Unexpected %s argument",
 text330[] = "EntryText",
 text331[] = "window",
 text332[] = "%s tab size set to %d",
 text333[] = "pseudo-command",
 text334[] = ", setting variable '~b%s~B'",
 text335[] = "File test code(s)",
 text336[] = "macro",
 text337[] = "Invalid escape sequence \"%.*s\"",
 text338[] = "Cannot access '$keyMacro' from a keyboard macro, cancelled",
 text339[] = "to",
 text340[] = "Col %d/%d, char = %s",
 text341[] = "Cannot use key sequence ~#u%s~U as %s delimiter",
 text342[] = "prompt",
 text343[] = "search",
 text344[] = "Unknown %s argument '%s'",
 text345[] = " invalid",
 text346[] = "%s mark",
 text347[] = "Swap dot with",
 text348[] = "Save point in",
 text349[] = " (char)",
 text350[] = "set",
 text351[] = "All marks deleted",
 text352[] = "Cannot delete mark ~u%c~U",
 text353[] = "Marks",
 text354[] = "MarkOffset  Line Text",
 text355[] = " and marked as region",
 text356[] = "Too many windows (%u)",
 text357[] = "%s%s%s deleted",
 text358[] = "Illegal use of %s value",
 text359[] = "nil",
 text360[] = "Boolean",
 text361[] = "No mark found to delete",
 text362[] = "Unknown file test code '%c'",
 text363[] = "Modes",
 text364[] = "GLOBAL MODES",
 text365[] = "BUFFER MODES",
 text366[] = "Scope-locked",
 text367[] = "Deleting buffer '%s'...",
 text368[] = "%u buffer%s deleted",
 text369[] = " in buffer '%s'",
 text370[] = "Array element %d not an integer",
 text371[] = "Array expected",
 text372[] = "Buffer '%s' deleted",
 text373[] = "Illegal value for '~b%s~B' variable",
 text374[] = "Invalid buffer attribute '%s'",
 text375[] = "Attribute(s) changed",
 text376[] = "Cannot enable terminal attributes in a macro buffer",
 text377[] = "%s '%s' is empty",
 text378[] = "Line offset value %ld out of range",
 text379[] = "'~b%s~B' value must be between %d and %d",
 text380[] = "screen",
 text381[] = "Buffer '%s' created",
 text382[] = "\" ~bwith~B \"",
 text383[] = "(self insert)",
 text384[] = "i = %d, inc = %d, format = '%s'",
 text385[] = "Change buffer name to",
 text386[] = "Macro declaration contains invalid argument range",
 text387[] = "Mode",
 text388[] = "Group",
 text389[] = "mode",
 text390[] = "group",
 text391[] = "setting",
 text392[] = "created",
 text393[] = "Tab size",
 text394[] = "attribute changed",
 text395[] = "No such %s '%s'",
 text396[] = "Cannot delete built-in %s '%s'",
 text397[] = "Invalid %s %s '%s'",
 text398[] = "Scope of %s '%s' must match %s '%s'",
 text399[] = "%s '%s' has permanent scope",
 text400[] = "Hidden",
 text401[] = "MODE GROUPS",
 text402[] = "Edit",
 text403[] = "Create or edit",
 text404[] = "'%s' is not a %s group",
 text405[] = "Incomplete line \"%.*s\"",
 text406[] = "Glob of pattern \"%s\" failed: %s",
 text407[] = "Invalid argument for %s glob search",
 text408[] = "Unterminated /#...#/ comment",
 text409[] = "Delimiter",
 text410[] = "Invalid message option '%s'",
 text411[] = "Alias",
 text412[] = "Macro",
 text413[] = "'macro' keyword expected",
 text414[] = "constrained macro",
 text415[] = " interactively",
 text416[] = " not allowed",
 text417[] = " on ",
 text418[] = "Key binding",
 text419[] = "Nested macro%s",
 text420[] = "Hook may not be set to non-",
 text421[] = "initializing ncurses",
 text422[] = "Invalid color pair value(s)",
 text423[] = "array",
 text424[] = "ShellCmd",
 text425[] = "Color pair %ld must be between 0 and %hd",
 text426[] = "Color no. %ld must be between -1 and %hd",
 text427[] = "Terminal does not support color",
 text428[] = "Colors",
 text429[] = "COLOR PALETTE - Page %d of %d\n\n",
 text430[] = "~bColor#         Foreground Samples                   Background Samples~B",
 text431[] = "\"A text sample\"",
 text432[] = "With white text",
 text433[] = "With black text",
 text434[] = "COLOR PAIRS DEFINED\n\n~bPair#        Sample~B",
 text435[] = "REC",
 text436[] = "Kill",
 text437[] = "AUHL Name          Description",
 text438[] = " / Members",
 text439[] = "Changed",
 text440[] = "Constrained",
 text441[] = "Preprocessed",
 text442[] = "Terminal attributes enabled",
 text443[] = "Invalid display item '%s'";
#if MMDebug & Debug_ShowRE
char
 text994[] = "SEARCH",
 text995[] = "REPLACE",
 text996[] = "RegexpInfo",
 text997[] = "Forward",
 text998[] = "Backward",
 text999[] = "PATTERN";
#endif

// Command and function help text.
#define CFLit_abort		"Return \"abort\" status and optional message (with terminal attributes enabled if n argument).\
  Argument(s) are converted to string and concatenated to form the message."
#define CFLit_about		"Generate \"about the editor\" information in a new buffer and render it per ~bselectBuf~B\
 options (in a pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_abs		"Return absolute value of N."
#define CFLit_alias		"Create alias of command, function, or macro cfm (and replace any existing alias if n > 0)."
#define CFLit_alterBufAttr	"Clear (n < 0), toggle (n == 0, default), set (n == 1), or clear all and set (n > 1) zero or\
 more of the following attributes in named buffer: \"Changed\", \"Hidden\", \"TermAttr\" (terminal attributes enabled).  If\
 n <= 1, at least one attribute argument must be specified.  If interactive and default n, one attribute is toggled in current\
 buffer.  Returns: former state (-1 or 1) of last attribute altered."
#define CFLit_appendFile	"Append current buffer to named file."
#define CFLit_apropos		"Generate list of commands, macros, functions, aliases, and variables whose names match pat\
 (or all names if pat is nil) in a new buffer and render it per ~bselectBuf~B options (in a pop-up window if default n).  If\
 pattern is plain text, match is successful if name contains pat.  Returns: ~bselectBuf~B values."
#define CFLit_array	"Create array and return it, given optional size and initial value of each element (default nil)."
#define CFLit_backChar		"Move point backward [-]n characters (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_backLine		"Move point backward [-]n lines (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_backPage		"Scroll current window backward n pages (or half a page if n < 0, default 1)."
#define CFLit_backPageNext	"Scroll next window backward n pages (or half a page if n < 0, default 1)."
#define CFLit_backPagePrev	"Scroll previous window backward n pages (or half a page if n < 0, default 1)."
#define CFLit_backTab		"Move point backward [-]n tab stops (default 1)."
#define CFLit_backWord		"Move point backward [-]n words (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_backspace		"Delete backward [-]n characters (or soft tabs if $softTabSize > 0, default 1)."
#define CFLit_basename		"Return filename component of given pathname (without extension if n <= 0)."
#define CFLit_beep		"Sound beeper n times (default 1)."
#define CFLit_beginBuf		"Move point to beginning of current buffer (or named buffer if n argument)."
#define CFLit_beginKeyMacro	"Begin recording a keyboard macro."
#define CFLit_beginLine		"Move point to beginning of [-]nth line (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_beginText		"Move point to beginning of text on [-]nth line (default 1).  Returns: false if hit a buffer\
 boundary; otherwise, true."
#define CFLit_beginWhite	"Move point to beginning of white space at or immediately before point."
#define CFLit_bgets		"Return nth next line (default 1) without trailing newline from named buffer beginning at\
 point.  If no lines left, nil is returned."
#define CFLit_binding		"Return name of command or macro bound to given key binding, or nil if none (or if n <= 0,\
 return key-lit in standardized form, or nil if it is invalid; or if n > 0, return array of key binding(s) bound to given\
 command or macro."
#define CFLit_bindKey		"Bind named command or macro to a key sequence (or single key if interactive and n <= 0)."
#define CFLit_bprint		"Write argument(s) to named buffer n times (default 1) and return string result, with\
 ~binsert~B options.  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_bprintf		"Write string built from specified format string and argument(s) to named buffer n times\
 (default 1) and return string result, with ~binsert~B options."
#define CFLit_bufAttrQ		"Check if any (or all if n argument) given attribute(s) are set in named buffer and return\
 Boolean result.  Attributes are: \"Active\" (file was read), \"Changed\", \"Constrained\", \"Hidden\", \"Macro\",\
 \"Narrowed\", and \"TermAttr\" (terminal attributes enabled)."
#define CFLit_bufBoundQ		"Return true if point at beginning (n < 0), middle (n == 0), end (n > 0), or either end\
 (default) of buffer; otherwise, false."
#define CFLit_bufModeQ		"Check if any (or all if n >= 0) of given buffer mode(s) are set in named buffer and return\
 Boolean result.  Arguments following buffer name may be mode names or arrays of mode names.  If n <= 0, non-existent modes are\
 ignored."
#define CFLit_bufSize		"Return current buffer size in lines (or bytes if n argument)."
#define CFLit_bufWind		"Return ordinal number of first window on current screen (other than current window if n\
 argument) displaying given buffer, or nil if none."
#define CFLit_chgBufMode	"Clear (n < 0), toggle (n == 0, default), set (n == 1), or clear all and set (n > 1) zero or\
 more buffer modes in named buffer.  Arguments following buffer name may be mode names or arrays of mode names.  If n <= 1, at\
 least one mode argument must be specified.  If interactive and default n, one mode is toggled in current buffer.  Returns:\
 former state (-1 or 1) of last mode changeed or zero if n > 1 and no mode arguments were specified."
#define CFLit_chgDir		"Change working directory.  Returns: absolute pathname of new directory."
#define CFLit_chgGlobalMode	"Clear (n < 0), toggle (n == 0, default), set (n == 1), or clear all and set (n > 1) zero or\
 more global modes.  Arguments may be mode names or arrays of mode names.  If n <= 1, at least one argument must be specified.\
  Returns: former state (-1 or 1) of last mode changed or zero if n > 1 and no arguments were specified."
#define CFLit_chr		"Return string form of ASCII (int) character N."
#define CFLit_clearBuf		"Clear current buffer (or named buffer if n >= 0, ignoring changes if n != 0).  Returns: false\
 if buffer not cleared; otherwise, true."
#define CFLit_clearMsgLine	"Clear the message line."
#define CFLit_clone		"Return copy of given array."
#define CFLit_copyFencedRegion	"Copy text from point to matching ( ), [ ], { }, or < > fence character to kill ring."
#define CFLit_copyLine		"Copy [-]n lines to kill ring (default 1, region lines if n == 0)."
#define CFLit_copyRegion	"Copy region text to kill ring."
#define CFLit_copyToBreak	"Copy from point through [-]n lines (default 1) to kill ring, not including delimiter of last\
 line.  If n == 0, copy from point to beginning of current line.  If n == 1 and point is at end of line, copy delimiter only."
#define CFLit_copyWord		"Copy [-]n words to kill ring (default 1, without trailing white space if n == 0).  Non-word\
 characters between point and first word are included."
#if WordCount
#define CFLit_countWords	"Scan region text and display counts of lines, words, and characters on message line."
#endif
#define CFLit_cycleKillRing	"Cycle kill ring [-]n times (default 1)."
#define CFLit_cycleReplaceRing	"Cycle replace ring [-]n times (default 1)."
#define CFLit_cycleSearchRing	"Cycle search ring [-]n times (default 1)."
#define CFLit_definedQ		"Check if name (default), mark (n < 0), active mark (n == 0), mode (n == 1), or mode group\
 (n > 1) is defined.  If name specified, return keyword result if found; otherwise, nil.  If n argument, return  Boolean\
 result.  If n <= 0, mark name is interpreted as an ASCII (int) character; otherwise, a string.  Returned keywords are:\
 \"alias\", \"buffer\", \"command\", \"function\", \"macro\", \"pseudo-command\" (prefix key), or \"variable\"."
#define CFLit_deleteAlias	"Delete named alias(es).  Returns: zero if failure; otherwise, number of aliases deleted."
#define CFLit_deleteBackChar	"Delete backward [-]n characters (default 1)."
#define CFLit_deleteBackTab	"Delete backward [-]n hard tabs (or soft tabs if $softTabSize > 0, default 1)."
#define CFLit_deleteBlankLines	"Delete all blank lines above and below point, or immediately below it if current line not\
 blank."
#define CFLit_deleteBuf		"Delete named buffer(s) (and ignore changes if n < 0).  If n == 0, delete all unchanged\
 buffer(s); if n == 1, delete all buffer(s) with prompting if changed; if n > 1, delete all buffer(s), ignoring changes, with\
 initial confirmation.  If n >= 0, hidden buffers and buffers being displayed are skipped and no arguments are allowed. \
 Returns: zero if failure; otherwise, number of buffers deleted."
#define CFLit_deleteFencedRegion	"Delete text from point to matching ( ), [ ], { }, or < > fence character."
#define CFLit_deleteForwChar	"Delete forward [-]n characters (default 1)."
#define CFLit_deleteForwTab	"Delete forward [-]n hard tabs (or soft tabs if $softTabSize > 0, default 1)."
#define CFLit_deleteKill	"Delete the most recent n entries (default 1) from kill ring (or entry number n if n < 0, all\
 entries if n == 0)."
#define CFLit_deleteLine	"Delete [-]n lines (default 1, region lines if n == 0)."
#define CFLit_deleteMacro	"Delete named macro(s).  Returns: zero if failure; otherwise, number of macros deleted."
#define CFLit_deleteMark	"Delete mark m (or all marks if n argument).  No error if script mode and mark does not exist."
#define CFLit_deleteRegion	"Delete region text."
#define CFLit_deleteScreen	"Delete screen N (and switch to first screen if N is current screen)."
#define CFLit_deleteSearchPat	"Delete the most recent n entries (default 1) from search ring (or entry number n if n < 0, all\
 entries if n == 0).  The most recent remaining entry is subsequently set as the current search pattern."
#define CFLit_deleteReplacePat	"Delete the most recent n entries (default 1) from replace ring (or entry number n if n < 0,\
 all entries if n == 0).  The most recent remaining entry is subsequently set as the current replace pattern."
#define CFLit_deleteToBreak	"Delete from point through [-]n lines (default 1), not including delimiter of last line.  If\
 n == 0, delete from point to beginning of current line.  If n == 1 and point is at end of line, delete delimiter only."
#define CFLit_deleteWhite	"Delete white space (and non-word characters if n > 0) surrounding or immediately before point."
#define CFLit_deleteWind	"Delete current window and join upward (or downward if n > 0, force \"wrap around\" if\
 abs(n) >= 2, delete buffer if n == -1)."
#define CFLit_deleteWord	"Delete [-]n words (default 1, without trailing white space if n == 0).  Non-word characters\
 between point and first word are included."
#define CFLit_detabLine		"Change hard tabs to spaces in [-]n lines (default 1, region lines if n == 0) given tab size,\
 and mark line block as new region.  If tab size is nil, $hardTabSize is used."
#define CFLit_dirname		"Return directory component of given pathname (\"\" if none, \".\" if none and n argument)."
#define CFLit_dupLine		"Duplicate [-]n lines (default 1, region lines if n == 0) and move point to beginning of text\
 of duplicated block."
#define CFLit_editMode		"Create, delete, or edit a mode, given name and zero or more attributes to set or change.  If\
 mode does not exist, it is created if n > 0; otherwise, an error is returned.  If n <= 0, mode is deleted and no attribute\
 arguments are allowed; otherwise, zero or more may be specified.  Attribute arguments are strings in form \"keyword: value\"\
 where keyword is either a Boolean attribute (\"global\", \"buffer\", \"visible\", \"hidden\") or string attribute\
 (\"description\", \"group\"), and value is \"true\", \"false\", descriptive text, or a group name; for example, \"hidden:\
 true\" or \"group: ProgLang\".  \"group: nil\" may also be specified to remove a mode from a group.  Defaults for a new mode\
 are \"buffer: true\", \"visible: true\".  Case of keywords and extraneous white space are ignored."
#define CFLit_editModeGroup	"Create, delete, or edit a mode group, given name and zero or more attributes to set or change.\
  If group does not exist, it is created if n > 0; otherwise, an error is returned.  If n <= 0, group is deleted and no\
 attribute arguments are allowed; otherwise, zero or more may be specified.  Attribute arguments are strings in form \"keyword:\
 value\" where keyword is a string attribute (\"description\", \"modes\") and value is descriptive text or a comma-separated\
 list of mode names; for example, \"modes: C, Ruby\", which adds the specified modes to the group.  \"modes: nil\" may also be\
 specified to clear all modes.  Case of keywords and extraneous white space are ignored."
#define CFLit_emptyQ		"Return true if expression value is nil, an ASCII null (int) character, a null string, or empty\
 array; otherwise, false."
#define CFLit_endBuf		"Move point to end of current buffer (or named buffer if n argument)."
#define CFLit_endKeyMacro	"End recording a keyboard macro."
#define CFLit_endLine		"Move point to end of [-]nth line (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_endWhite		"Move point to end of white space at point."
#define CFLit_endWord		"Move point to end of [-]nth word (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_entabLine		"Change spaces to hard tabs in [-]n lines (default 1, region lines if n == 0) given tab size,\
 and mark line block as new region.  If tab size is nil, $hardTabSize is used."
#define CFLit_env		"Return value of given environmental variable (or a null string if variable does not exist)."
#define CFLit_eval		"Concatenate argument(s) (or prompt for string if interactive) and execute result as an\
 expression.  Returns: result of evaluation."
#define CFLit_exit		"Exit editor with optional message (and ignore changes if n != 0, force return code 1 if\
 n < 0).  Argument(s) are converted to string and concatenated to form the message."
#define CFLit_findFile		"Find named file and read (if n != 0) into a buffer with ~bselectBuf~B options (without\
 auto-completion if interactive and n == 0 or n == 1).  If a buffer already exists with the same filename, its buffer is\
 selected; otherwise, a buffer is created.  Returns: ~bselectBuf~B values."
#define CFLit_forwChar		"Move point forward [-]n characters (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_forwLine		"Move point forward [-]n lines (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_forwPage		"Scroll current window forward n pages (or half a page if n < 0, default 1)."
#define CFLit_forwPageNext	"Scroll next window forward n pages (or half a page if n < 0, default 1)."
#define CFLit_forwPagePrev	"Scroll previous window forward n pages (or half a page if n < 0, default 1)."
#define CFLit_forwTab		"Move point forward [-]n tab stops (default 1)."
#define CFLit_forwWord		"Move point forward [-]n words (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_getInfo		"Return informational item for given keyword: \"Buffers\" -> names of visible buffers as an\
 array (plus hidden ones if n <= 0, all if n > 0), \"Colors\" -> list of display item colors in form [[item-kw,[fg-color,\
bg-color]],...] where color pair array is nil if color not set for item (or number of colors and color pairs available in form\
 [colors,pairs] if n argument), \"Editor\" -> editor name, \"Hooks\" -> hook list in form [[hook-name,macro-name],...] where\
 macro-name is nil if hook not set, \"Language\" -> language of text messages, \"Modes\" -> mode list in form [[mode-name,\
group-name,user?,global?,hidden?,scope-locked?,active?],...] where group-name is nil if mode is not in a group, and last five\
 elements are Boolean values, \"OS\" -> operating system name, \"Screens\" -> screen list in form [[screen-num,wind-count,\
work-dir],...], \"Version\" -> editor version, or \"Windows\" -> window list in form [[wind-num,buf-name],...] (or form\
 [[screen-num,wind-num,buf-name],...] if n argument)."
#define CFLit_getKey		"Get one keystroke (or a key sequence if n > 1) and return it as a key literal (or return ASCII\
 (int) character if n <= 0)."
#define CFLit_getWord		"Get previous (n < 0), current (n == 0, default), or next (n > 0) word from current line and\
 return it.  If indicated word does not exist or n == 0 and point is not in a word, nil is returned.  Point is left at the\
 beginning of the word if n < 0 and at the end if n >= 0.  When going backward, point movement mimics the ~bbackWord~B command\
 and therefore, the \"previous\" word is the current word if point is in a word at the start and past its first character. \
 Additionally, lines are spanned during search if n < -1 or n > 1."
#define CFLit_glob		"Return a (possibly empty) array of file(s) and/or director(ies) that match given shell glob\
 pattern."
#define CFLit_globalModeQ	"Check if any (or all if n >= 0) of given global mode(s) are set and return Boolean result. \
 Arguments may be mode names or arrays of mode names.  If n <= 0, non-existent modes are ignored."
#define CFLit_gotoFence		"Move point to matching ( ), [ ], { }, or < > fence character from one at point (or one given\
 as an argument if n argument)."
#define CFLit_gotoLine	"Move point to beginning of line N (or to end of buffer if N == 0, in named buffer if n argument)."
#define CFLit_gotoMark		"Move point to mark m and restore its window framing (and delete mark if n argument)."
#define CFLit_groupModeQ	"Check if any buffer mode in given group is set in named buffer (or any global mode if n >= 0)\
 and return its name if so; otherwise, nil.  If n <= 0, a non-existent group is ignored."
#define CFLit_growWind		"Increase size of current window by abs(n) lines (default 1 at bottom, at top if n < 0)."
#define CFLit_help		"Run macro assigned to \"help\" hook, if any."
#define CFLit_huntBack	"Repeat most recent search backward n times (default 1).  Returns: string found, or false if not found."
#define CFLit_huntForw	"Repeat most recent search forward n times (default 1).  Returns: string found, or false if not found."
#define CFLit_includeQ		"Return true if any (or all if n != 0) expression value(s) are in array; otherwise, false.  If\
 n <= 0, case is ignored in string comparisons."
#define CFLit_indentRegion	"Indent lines in region by n tab stops (default 1) and mark line block as new region."
#define CFLit_index		"Return position of pattern (or character if n <= 0) in str, or nil if not found.  First\
 position is 0.  If n >= 0, rightmost occurrence is returned."
#define CFLit_insert		"Insert argument(s) at point n times (default 1) and return string result (without moving point\
 if n == 0, with literal newline if n < 0).  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_insertBuf		"Insert named buffer at point (without moving point if n == 0) and mark as region."
#define CFLit_insertFile	"Insert named file at point (without moving point if n == 0) and mark as region."
#define CFLit_inserti		"Format, insert at point, and add increment to i variable [-]n times (default 1).  If n < 0,\
 point is moved back to original position after each iteration."
#define CFLit_insertLineI	"Insert and indent a line before the [-]nth line (default 1)."
#define CFLit_insertPipe	"Execute shell pipeline and insert its output at point (without moving point if n == 0). \
 Argument(s) are converted to string and concatenated to form the command.  Returns: false if failure; otherwise, true."
#define CFLit_insertSpace	"Insert abs(n) spaces ahead of point (default 1, ignoring \"Over\" and \"Repl\" buffer modes if\
 n < 0)."
#define CFLit_interactiveQ	"Return true if script is being executed interactively; otherwise, false."
#define CFLit_join		"Join all values with given string delimiter and return string result (ignoring nil values if\
 n == 0, and null values if n < 0).  All values are converted to string and each element of an array (processed recursively) is\
 treated as a separate value."
#define CFLit_joinLines	"Join [-]n lines (default -1, region lines if n == 0) into a single line, replacing intervening white\
 space with zero spaces (\"chars\" is nil), one space (\"chars\" is a null string), or one or two spaces (\"chars\" is a list\
 of one or more \"end of sentence\" characters).  In the last case, two spaces are inserted between lines if the first line\
 ends with any of the given characters and the second line is not blank.  When command is invoked interactively, a null string\
 is used for the \"chars\" argument."
#define CFLit_joinWind		"Join current window with upper window (or lower window if n > 0, force \"wrap around\" if\
 abs(n) >= 2, delete buffer in other window if n == -1)."
#define CFLit_keyPendingQ	"Return true if type-ahead key(s) pending; otherwise, false."
#define CFLit_kill		"Return text from kill number N (where N <= 0)."
#define CFLit_killFencedRegion	"Delete and save text from point to matching ( ), [ ], { }, or < > fence character to kill\
 ring."
#define CFLit_killLine		"Delete and save [-]n lines to kill ring (default 1, region lines if n == 0)."
#define CFLit_killRegion	"Delete and save region text to kill ring."
#define CFLit_killToBreak	"Delete and save text from point through [-]n lines (default 1) to kill ring, not including\
 delimiter of last line.  If n == 0, delete and save text from point to beginning of current line.  If n == 1 and point is at\
 end of line, delete and save delimiter only."
#define CFLit_killWord		"Delete and save [-]n words to kill ring (default 1, without trailing white space if n == 0). \
 Non-word characters between point and first word are included."
#define CFLit_lastBuf		"Switch to last buffer exited from in current screen (and delete current buffer if n < 0). \
 Returns: ~bselectBuf~B value for n == 1."
#define CFLit_length		"Return length of string or size of an array."
#define CFLit_let		"Assign a value (or an expression if n argument) to a system or global variable interactively."
#define CFLit_lowerCaseLine	"Change [-]n lines to lower case (default 1, region lines if n == 0) and mark line block as\
 new region."
#define CFLit_lowerCaseRegion	"Change all letters in region to lower case."
#define CFLit_lowerCaseStr	"Return string converted to lower case."
#define CFLit_lowerCaseWord	"Change n words to lower case (default 1)."
#define CFLit_markBuf	"Save point position in mark " WrkMarkStr " (or mark m if n >= 0) and mark whole buffer as a region."
#define CFLit_match		"Return text matching group N from last =~~ expression or index() call using a regular\
 expression as the pattern (or from last buffer search if n argument).  Group 0 is entire match."
#define CFLit_message		"Set a return message with comma-separated options specified in string argument opts (or nil if\
 none), and return true (or false if n <= 0).  Argument(s) following opts are converted to string and concatenated to form the\
 message.  A message is set only if one has not already been set, and is wrapped in brackets (thus informational) if default n\
 or n > 0.  Case-insensitive options are: \"NoWrap\" (do not wrap message in brackets), and \"TermAttr\" (enable terminal\
 attributes in message).  If abs(n) == 1, message is considered high priority and will replace any existing message that is not\
 high priority and was not forced; if abs(n) > 1, any existing message is replaced unconditionally (forced).  If message is nil\
 or a null string, any existing message is cleared."
#define CFLit_metaPrefix	"Begin meta key sequence."
#define CFLit_moveWindDown	"Move window frame [-]n lines down (default 1)."
#define CFLit_moveWindUp	"Move window frame [-]n lines up (default 1)."
#define CFLit_narrowBuf	"Keep [-]n lines (default 1, region lines if n == 0) in current buffer and hide and preserve remainder."
#define CFLit_negativeArg	"Begin or continue entry of universal negative-trending n argument (in sequence -1,-2,...)."
#define CFLit_newline		"Insert n lines (default 1, with auto-formatting if \"Wrap\" buffer mode enabled) or insert\
 abs(n) lines without formatting if n < 0."
#define CFLit_newlineI		"Insert n lines with same indentation as previous line (default 1)."
#define CFLit_nextBuf		"Switch to next visible buffer n times (default 1, or switch once and delete current buffer if\
 n < 0).  If n == 0, switch once and include hidden, non-macro buffers.  Returns: name of last buffer switched to."
#define CFLit_nextScreen	"Switch to next screen n times (default 1)."
#define CFLit_nextWind		"Switch to next window n times (default 1)."
#define CFLit_nilQ		"Return true if expression value is nil; otherwise, false."
#define CFLit_nullQ		"Return true if string is null (empty); otherwise, false."
#define CFLit_numericQ	"Return true if string is a numeric literal which can be converted to an integer; otherwise, false."
#define CFLit_onlyWind		"Make current window the only window on screen (delete all others)."
#define CFLit_openLine		"Insert abs(n) lines ahead of point (default 1).  If n < 0, also move point to first empty line\
 inserted if possible."
#define CFLit_ord		"Return ASCII integer value of first character of given string."
#define CFLit_outdentRegion	"Outdent lines in region by n tab stops (default 1) and mark line block as new region."
#define CFLit_overwrite		"Overwrite text beginning at point n times (default 1) with argument(s) and return string\
 result, with ~binsert~B options.  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_pathname		"Return absolute pathname of given file or directory, or an array of absolute pathnames if\
 argument is an array (of pathnames).  If n <= 0, symbolic links are not resolved."
#define CFLit_pause		"Pause execution for N seconds (or centiseconds if n argument)."
#define CFLit_pipeBuf		"Feed current buffer through shell pipeline and read its output into current buffer, with\
 ~breadFile~B options.  Argument(s) are converted to string and concatenated to form the command.  Returns: false if failure;\
 otherwise, ~bselectBuf~B values."
#define CFLit_pop		"Remove last element from given array and return it, or nil if none left."
#define CFLit_popBuf		"Display named buffer in a pop-up window with one or more of the following options if n\
 argument: ~u~ba~Zlternate mode line, ~u~bd~Zelete buffer after displaying it, ~u~bs~Zhift long lines left, enable\
 ~u~bt~Zerminal attributes.  Returns: name of buffer popped if not deleted afterward; otherwise, nil."
#define CFLit_popFile		"Display named file in a pop-up window with ~bpopBuf~B options (and select existing buffer with\
 same filename if possible, if n <= 0).  If a buffer is created for the file, it is deleted after the file is displayed. \
 Returns: name of buffer popped if not deleted afterward; otherwise, nil."
#define CFLit_prefix1		"Begin prefix 1 key sequence."
#define CFLit_prefix2		"Begin prefix 2 key sequence."
#define CFLit_prefix3		"Begin prefix 3 key sequence."
#define CFLit_prevBuf		"Switch to previous visible buffer n times (default 1, or switch once and delete current buffer\
 if n < 0).  If n == 0, switch once and include hidden, non-macro buffers.  Returns: name of last buffer switched to."
#define CFLit_prevScreen	"Switch to previous screen n times (default 1)."
#define CFLit_prevWind		"Switch to previous window n times (default 1)."
#define CFLit_print		"Write argument(s) to message line (with terminal attributes enabled if n >= 0).  Argument(s)\
 are converted to string and concatenated to form the message."
#define CFLit_printf		"Write string built from specified format string and argument(s) to message line (with terminal\
 attributes enabled if n >= 0)."
#define CFLit_prompt		"Get terminal input and return string, ASCII (int) character, or nil result (with no echo if\
 n <= 0, with terminal attributes enabled in prompt string if n >= 0).  Arguments are: prompt (nil or string), default value\
 (nil, ASCII character, int, or string), type (string: ~u~bb~Zuffer completion, single ~u~bc~Zharacter, ~u~bf~Zile completion,\
 ~u~bK~Zill ring access, ~u~bo~Zne key, ~u~bO~Zne key sequence, ~u~bR~Zeplace ring access, ~u~bS~Zearch ring access,\
 ~u~bs~Ztring (default), mutable ~u~bv~Zariable completion, all ~u~bV~Zariables completion), and delimiter key literal.  All\
 arguments after the first are optional.  Any completion type letter may be preceded by ~u~b^~Z to suppress auto-completion. \
 If the prompt string begins with a letter, \": \" is appended; otherwise, it is used as is, however any leading ~u'~U or\
 ~u\"~U quote character is stripped off first and any trailing white space is moved to the very end of the final prompt string\
 that is built.  If a non-nil default value is specified, it is displayed after the prompt string between square brackets\
 ~#u[ ]~U if the prompt type is ~u~bc~Z, and must be an ASCII character in that case; otherwise, it must be string or int and\
 is stored in the input buffer in string form as the initial value.  A key literal is returned for types ~u~bo~Z and ~u~bO~Z;\
 otherwise, if the user enters just the delimiter key, the default value is returned as an ASCII character if it exists and the\
 prompt type is ~u~bc~Z; otherwise, nil is returned.  If the user presses ~uC-SPC~U at any time, a null string is returned\
 instead (or a null character if the prompt type is ~u~bc~Z); otherwise, the string in the input buffer is returned when the\
 delimiter key is entered."
#define CFLit_push		"Append expression value to given array and return new array contents."
#define CFLit_queryReplace	"Replace n occurrences of pat1 with pat2 interactively, beginning at point (or all occurrences\
 if default n).  Returns: false if search was stopped prematurely by user; otherwise, true."
#define CFLit_quickExit		"Save any changed buffers and exit editor.  If any changed buffer is not saved successfully,\
 exit is cancelled."
#define CFLit_quote		"Return expression value converted to a string form which can be converted back to the original\
 value via ~beval~B (and force through any array recursion if n > 0)."
#define CFLit_quoteChar		"Get one keystroke and insert it in raw form abs(n) times (default 1, ignoring \"Over\" and\
 \"Repl\" buffer modes if n < 0).  If a function key is entered, it is inserted as a key literal."
#define CFLit_rand		"Return pseudo-random integer in range 1-N (where N > 1)."
#define CFLit_readFile		"Read named file into current buffer (or new buffer if n argument) with ~bselectBuf~B options. \
 Returns: ~bselectBuf~B values."
#define CFLit_readPipe		"Execute shell pipeline and read its output into current buffer, with ~breadFile~B options. \
 Argument(s) are converted to string and concatenated to form the command.  Returns: false if failure; otherwise, ~bselectBuf~B\
 values."
#define CFLit_reframeWind	"Reframe current window with point at [-]nth (n != 0) or center (n == 0, default) window line."
#define CFLit_renameBuf		"Rename current buffer to \"name1\" (or rename buffer \"name1\" to \"name2\" if n <= 0; derive\
 new name from filename associated with buffer \"name1\" if n > 0).  Returns: new buffer name."
#define CFLit_replace		"Replace n occurrences of pat1 with pat2, beginning at point (or all occurrences if default n).\
  Returns: false if n > 0 and fewer than n replacements were made; otherwise, true."
#define CFLit_replaceText	"Replace text beginning at point n times (default 1) with argument(s) and return string result,\
 with ~binsert~B options.  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_resetTerm		"Update all screens and windows to match current terminal dimensions (force if n > 0)."
#define CFLit_resizeWind	"Change size of current window to abs(n) lines (growing or shrinking at bottom if n > 0, at top\
 if n < 0, or make all windows the same size if n == 0)."
#define CFLit_restoreBuf	"Switch to remembered buffer in current window and return its name."
#define CFLit_restoreScreen	"Switch to remembered screen and return its number."
#define CFLit_restoreWind	"Switch to remembered window on current screen and return its number."
#define CFLit_run		"Execute named command, alias, or macro interactively with numeric prefix n.  Returns:\
 execution result."
#define CFLit_saveBuf		"Remember current buffer."
#define CFLit_saveFile		"Save current buffer (if changed) to its associated file (or all changed buffers if n > 0)."
#define CFLit_saveScreen	"Remember current screen."
#define CFLit_saveWind		"Remember current window."
#define CFLit_scratchBuf	"Create and switch to a buffer with unique name, with ~bselectBuf~B options.  Returns:\
 ~bselectBuf~B values."
#define CFLit_searchBack	"Search backward for the nth occurrence of a pattern (default 1).  Returns: string\
 found, or false if not found."
#define CFLit_searchForw	"Search forward for the nth occurrence of a pattern (default 1).  Returns: string\
 found, or false if not found."
#define CFLit_selectBuf		"Select named buffer and either display it in a different window and switch to that window\
 (n < -2), display it in a different window but stay in current window (n == -2), pop it (n == -1), leave it as is (n == 0),\
 switch to it in current window (n == 1, default), display it in a new window but stay in the current window (n == 2), or\
 display it in a new window and switch to that window (n > 2).  If buffer does not exist and pop is requested, an error is\
 returned; otherwise, buffer is created.  If n <= -2 and only one window exists, a window is created; otherwise, the buffer is\
 displayed in a window already displaying the buffer, or the window immediately above the current window if possible.  Return\
 values: if n == -1: name of buffer that was popped if not deleted afterward; otherwise, nil; if n == 0: two-element array\
 containing buffer name and Boolean value indicating whether the buffer was created; or if n is any other value: four-element\
 array containing same first two elements as for n == 0 plus ordinal value of window displaying the buffer and Boolean value\
 indicating whether the window was created."
#define CFLit_selectLine	"Select text or line(s) around point, mark selected text as a region, move point to beginning\
 of region, and return number of lines selected.  Text is selected as follows: if n <= 0 (N is line selector), select [-]N\
 lines (default 1, region lines if N == 0), not including delimiter of last line if n < 0; if n > 0 (N is text selector),\
 select text from point through [-]N lines (default 1), not including delimiter of last line.  Additionally in the latter case,\
 if N == 0, select text from point to beginning of current line; if N == 1 and point is at end of line, select delimiter only. \
 n defaults to zero."
#define CFLit_selectScreen	"Switch to screen N (or create screen and switch to it if n > 0, redraw current screen only if\
 n <= 0)."
#define CFLit_selectWind	"Switch to window N (or nth from bottom if N < 0)."
#define CFLit_setBufFile	"Associate a file with current buffer (or named buffer if n argument) and derive new buffer\
 name from it unless n <= 0 or filename is nil or null.  If filename is nil or null, buffer's filename is cleared.  Returns:\
 two-element array containing new buffer name and new filename."
#define CFLit_setColorPair	"Assign a foreground and background color to color pair number pair-no (or next one available\
 if pair-no is zero).  Pair number can subsequently be used for ~un~U in a ~~~un~Uc terminal attribute specification.  Returns:\
 pair number assigned."
#define CFLit_setDispColor	"Set color for a display item, given item keyword and either a two-element array of foreground\
 and background color numbers, or nil for no color.  Keywords are \"Info\" for informational displays, \"ModeLine\" for mode\
 lines, and \"Record\" for keyboard macro recording indicator."
#define CFLit_setHook		"Set named hook to command or macro cm (or clear hook if cm is nil)."
#define CFLit_seti		"Set i variable, increment, and sprintf format string (or set i variable to n if n >= 0 and\
 leave increment and format string unchanged, or display parameters on message line only if n < 0)."
#define CFLit_setMark		"Save point position and window framing in mark SPC (default), . (n < 0), or m (n >= 0)."
#define CFLit_setWrapCol	"Set wrap column to N (or previous value if n argument)."
#define CFLit_shell		"Spawn an interactive shell session.  Returns: false if failure; otherwise, true."
#define CFLit_shellCmd		"Execute a shell command and display result in a pop-up window (with long lines shifted left if\
 n > 0, skipping pop-up if n <= 0).  Argument(s) are converted to string and concatenated to form the command.  Returns: false\
 if failure; otherwise, true."
#define CFLit_shift		"Remove first element from given array and return it, or nil if none left."
#define CFLit_showAliases	"Generate list of aliases in a new buffer and render it per ~bselectBuf~B options (in a pop-up\
 window if default n, matching pat if n argument).  If pattern is plain text, match is successful if name contains pat. \
 Returns: ~bselectBuf~B values."
#define CFLit_showBuffers	"Generate list of visible buffers (or all buffers if n argument, excluding macros if n == -1)\
 in a new buffer and render it per ~bselectBuf~B options (in a pop-up window if default n).  The first column in the list is\
 buffer attributes: ~u~bA~Zctive, ~u~bC~Zhanged, ~u~bH~Zidden, ~u~bM~Zacro, ~u~bC~Zonstrained, ~u~bP~Zrecompiled,\
 ~u~bN~Zarrowed, and ~u~bT~ZermAttr.  Returns: ~bselectBuf~B values."
#define CFLit_showColors	"Generate a palette of available colors or defined color pairs and display it in a pop-up\
 window.  If n <= 0, display color pairs; otherwise, color palette.  In the latter case, number of colors displayed is\
 dependent on terminal type and may be split into pages, selected by n: first page is displayed for n == 1 (default), second\
 page for n == 2, and so on until all colors shown."
#define CFLit_showCommands	"Generate list of commands and their bindings in a new buffer and render it per ~bselectBuf~B\
 options (in a pop-up window if default n, matching pat if n argument).  If pattern is plain text, match is successful if name\
 contains pat.  Returns: ~bselectBuf~B values."
#define CFLit_showDir		"Display working directory on message line.  Returns: absolute pathname of current directory."
#define CFLit_showFunctions	"Generate list of system functions in a new buffer and render it per ~bselectBuf~B options (in\
 a pop-up window if default n, matching pat if n argument).  If pattern is plain text, match is successful if name contains\
 pat.  Returns: ~bselectBuf~B values."
#define CFLit_showHooks		"Generate list of hooks in a new buffer and render it per ~bselectBuf~B options (in a pop-up\
 window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showKey		"Display name of command or macro bound to key sequence (or single key if n <= 0) in a pop-up\
 window (or on message line if n >= 0)."
#define CFLit_showKillRing	"Generate list of kill ring entries in a new buffer and render it per ~bselectBuf~B options (in\
 a pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showMacros	"Generate list of non-constrained macros (or all macros if n argument) and their bindings in a\
 new buffer and render it per ~bselectBuf~B options (in a pop-up window if default n, matching pat if n argument).  If pattern\
 is plain text, match is successful if name contains pat.  Returns: ~bselectBuf~B values."
#define CFLit_showMarks		"Generate list of buffer marks in a new buffer and render it per ~bselectBuf~B options (in a\
 pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showModes		"Generate list of modes in a new buffer and render it per ~bselectBuf~B options (in a pop-up\
 window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showPoint		"Display buffer position of point and current character information on message line (for\
 current line only if n argument)."
#if MMDebug & Debug_ShowRE
#define CFLit_showRegexp	"Generate list of search and replacement metacharacter arrays in a new buffer and render it per\
 ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values."
#endif
#define CFLit_showReplaceRing	"Generate list of replacement ring patterns in a new buffer and render it per ~bselectBuf~B\
 options (in a pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showScreens	"Generate list of screens and their associated windows and buffers in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showSearchRing	"Generate list of search ring patterns in a new buffer and render it per ~bselectBuf~B options\
 (in a pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showVariables	"Generate list of system and user variables in a new buffer and render it per ~bselectBuf~B\
 options (in a pop-up window if default n, matching pat if n argument).  If pattern is plain text, match is successful if name\
 contains pat.  Returns: ~bselectBuf~B values."
#define CFLit_shQuote		"Return expression converted to string and enclosed in ' quotes.  Apostrophe ' characters are\
 converted to \\' so that the string is converted back to its orignal form if processed as a shell argument."
#define CFLit_shrinkWind	"Decrease size of current window by abs(n) lines (default 1 at bottom, at top if n < 0)."
#define CFLit_space		"Insert n spaces (default 1, with auto-wrap if \"Wrap\" buffer mode enabled) or insert abs(n)\
 spaces without formatting if n < 0."
#define CFLit_split		"Split string into an array and return it, using given delimiter character (?\\s for white\
 space) and optional maximum number of elements."
#define CFLit_splitWind		"Split current window into two of equal size (or shrink upper if n < 0, go to non-default if\
 n == 0, or set upper size to n if n > 0).  Returns: ordinal number of new window not containing point."
#define CFLit_sprintf		"Return string built from specified format string and argument(s)."
#define CFLit_statQ		"Test given file for specified type(s) and return Boolean result.  Types are:\
 ~u~bd~Zirectory, ~u~be~Zxists, regular ~u~bf~Zile, symbolic ~u~bL~Zink, hard ~u~bl~Zink, ~u~br~Zeadable, ~u~bs~Zize > 0,\
 ~u~bw~Zriteable, and e~u~bx~Zecutable.  If multiple letters are specified, true is returned if any type matches (or if all\
 types match if n argument)."
#define CFLit_strFit		"Return condensed string, possibly with embedded ellipsis, of maximum length n."
#define CFLit_strPop		"Remove last portion of string variable var using character delimiter dlm (?\\s for white\
 space) and return it.  If delimiter is nil or ?\\0, last character is removed and returned."
#define CFLit_strPush		"Append value to string variable var using string delimiter dlm and return new variable value."
#define CFLit_strShift		"Remove first portion of string variable var using character delimiter dlm (?\\s for white\
 space) and return it.  If delimiter is nil or ?\\0, first character is removed and returned."
#define CFLit_strUnshift	"Prepend value to string variable var using string delimiter dlm and return new variable value."
#define CFLit_strip		"Return whitespace-stripped string (or left end only if n < 0, right end only if n > 0)."
#define CFLit_sub		"Replace first (or all if n > 1) occurrences of \"from\" pattern in str with \"to\" pattern and\
 return result."
#define CFLit_subline		"Extract substring of given length from current line beginning at point (position 0) + pos and\
 return it.  Length is infinite if not specified.  If length is negative, it specifies a position backward from end of line at\
 which to end the extraction, not including the ending character.  Delimiter at end of line is not included in result."
#define CFLit_substr		"Extract substring of given length from str beginning at pos and return it.  First position is\
 0.  Length is infinite if not specified.  If length is negative, it specifies a position backward from end of str at which to\
 end the extraction, not including the ending character."
#define CFLit_suspend		"Suspend editor (put in background) and return to shell session."
#define CFLit_swapMark	"Swap point position and mark SPC (default), . (n < 0), or m (n >= 0) and restore its window framing."
#define CFLit_tab	"Insert hard tab (or soft tab if $softTabSize > 0) n times (default 1, or set soft tab size to abs(n)\
 only if n <= 0)."
#define CFLit_titleCaseLine	"Change words in [-]n lines to title case (default 1, region lines if n == 0) and mark line\
 block as new region."
#define CFLit_titleCaseRegion	"Change all words in region to title case."
#define CFLit_titleCaseStr	"Return string with all words it contains converted to title case."
#define CFLit_titleCaseWord	"Change n words to title case (default 1)."
#define CFLit_toInt		"Return numeric string literal converted to integer, ignoring any leading or trailing white\
 space.  String may be in any form recognized by the ~bstrtol()~B C library function with a zero base."
#define CFLit_toStr		"Return expression value converted to string (with literal nil if n < 0, in visible form if\
 n == 0, plus ' quotes if n > 0)."
#define CFLit_tr		"Return translated string.  Each \"from\" character in string is replaced with corresponding\
 \"to\" character.  If \"to\" string is shorter than \"from\" string, the last \"to\" character is used for each additional\
 \"from\" character.  If \"to\" string is nil or null, all \"from\" characters in string are deleted."
#define CFLit_traverseLine	"Move point forward or backward $travJump characters (or to far right if n == 0).  If n != 0,\
 first traversal is accelerated forward (n > 0) or backward (n < 0)."
#define CFLit_trimLine		"Trim white space from end of [-]n lines (default 1, region lines if n == 0) and mark line\
 block as new region."
#define CFLit_truncBuf		"Delete all text from point to end of buffer (or from point to beginning of buffer if n <= 0)."
#define CFLit_typeQ		"Return type of expression: \"array\", \"bool\", \"int\", \"nil\", or \"string\"."
#define CFLit_upperCaseLine	"Change [-]n lines to upper case (default 1, region lines if n == 0) and mark line block as\
 new region."
#define CFLit_upperCaseRegion	"Change all letters in region to upper case."
#define CFLit_upperCaseStr	"Return string converted to upper case."
#define CFLit_upperCaseWord	"Change n words to upper case (default 1)."
#define CFLit_unbindKey		"Unbind a key sequence (or single key if interactive and n <= 0).  If called from a script and\
 default n or n <= 0, returns nil if successful; otherwise, an error is generated.  If called from a script and n > 0, returns\
 true if successful; otherwise, false, and no error is generated."
#define CFLit_undelete		"Insert text from last \"delete\" command (without moving point if n == 0)."
#define CFLit_universalArg	"Begin or continue entry of universal positive-trending n argument (in sequence 2,0,3,4,...)."
#define CFLit_unshift		"Prepend expression value to given array and return new array contents."
#define CFLit_updateScreen	"Update and redraw screen if altered and no keystrokes pending.  If n <= 0, redraw is\
 suppressed; if n != 0, pending keystrokes are ignored."
#define CFLit_viewFile		"Find named file and read (if n != 0) into a read-only buffer with ~bselectBuf~B options\
 (without auto-completion if interactive and n == 0 or n == 1).  If a buffer already exists with the same filename, its buffer\
 is selected; otherwise, a buffer is created.  Returns: ~bselectBuf~B values."
#define CFLit_widenBuf		"Expose all hidden lines in current buffer (unnarrow)."
#define CFLit_wordCharQ		"Return true if character c is in $wordChars; otherwise, false."
#define CFLit_wrapLine		"Rewrap [-]n lines (default 1, region lines if n == 0) at $wrapCol, given a \"line prefix\"\
 string and \"end of sentence\" characters to pass to the ~bjoinLines~B command.  All lines in the block are filled and/or\
 wrapped so that they have the same indentation as the first line and do not extend past the wrap column.  Additionally, if\
 the line prefix is not nil or a null string (for example, \"# \" for a comment block) it is inserted after any indentation in\
 each line that is created during the wrapping process.  Existing prefix strings (with or without trailing white space) are\
 also recognized and removed from each line of the block before ~bjoinLines~B is called.  When command is invoked\
 interactively, a null string is used for the \"chars\" argument."
#define CFLit_wrapWord		"Move word at point to beginning of next line if word is past column n (default 0); otherwise,\
 move following word."
#define CFLit_writeFile		"Write current buffer to named file, set buffer's filename to it, and derive new buffer name\
 from it (unless n <= 0)."
#define CFLit_xeqBuf		"Execute named buffer as a macro with numeric prefix n and optional arguments.  Returns:\
 execution result."
#define CFLit_xeqFile		"Search for named file with ~bxPathname~B function and execute file that is found as a macro\
 with numeric prefix n and optional arguments.  Returns: execution result."
#define CFLit_xeqKeyMacro	"Execute saved keyboard macro n times (default 1, with infinite repetitions if n == 0). \
 Execution stops after n or $maxLoop iterations (if not zero), or if any invoked command or macro returns false."
#define CFLit_xPathname		"If default n or n < 0, search for given file or directory in $execPath directories and return\
 pathname if found; otherwise, nil.  (Pathname is searched for verbatim first, then with \"" Script_Ext "\" appended unless the\
 filename already has that extension.)  If n >= 0, search for file(s) and director(ies) in $execPath directories that match\
 given shell glob pattern and return a (possibly empty) array of pathnames that match.  Null directories in $execPath are\
 skipped if n <= 0 (so that the current directory is not searched).  Also, if the given argument begins with a slash (/) and a\
 glob search is not being performed, it is searched for as an absolute pathname, bypassing the directories in $execPath;\
 otherwise, an error is returned."
#define CFLit_yank		"Insert last kill n times (default 1) and mark as region (or insert once without moving point\
 if n == 0, insert nth older kill once if n < 0)." 
#define CFLit_yankCycle		"Replace yanked text with next older kill (default) or nth kill (n != 0), cycle the kill ring\
 accordingly, and mark new text as a region (or delete previous yank only if n == 0)."

// Hook help text.
#define HLitN_defn		"defn."
#define HLitN_chgDir		"From \"chgDir\" command^ ~uOR~U defn at startup."
#define HLitN_help		"From \"help\" command."
#define HLitN_postKey		"Key prefix."
#define HLitN_preKey		HLitN_postKey

#define HLitArg_none		"~b0:~B (none)"
#define HLitArg_createBuf	"~b1:~B Buffer name."
#define HLitArg_enterBuf	"~b1:~B \"exitBuf\" return^  value."
#define HLitArg_mode		"~b2:~B nil,prior value of^  $GlobalModes ~uOR~U^  buffer name,prior^  value of $BufModes."
#define HLitArg_postKey		"~b1:~B \"preKey\" return^  value."
#define HLitArg_read		"~b2:~B Buffer name,filename^  (or nil if standard^  input)."
#define HLitArg_write		"~b2:~B Buffer name,filename."
#define HLitArg_filename	"~b2:~B Buffer name,filename^  (or nil if none)."

// "About MightEMacs" help text.
#define ALit_author		"(c) Copyright 2019 Richard W. Marinelli <italian389@yahoo.com>"
#define ALit_footer1		" runs on Unix and Linux platforms and is licensed under the"
#define ALit_footer2		"GNU General Public License (GPLv3), which can be viewed at"
#define ALit_footer3		"http://www.gnu.org/licenses/gpl-3.0.en.html"
#define ALit_buildInfo		"Build Information"
#define ALit_maxCols		"Maximum terminal columns"
#define ALit_maxRows		"Maximum terminal rows"
#define ALit_maxTab		"Maximum hard or soft tab size"
#define ALit_maxPathname	"Maximum length of file pathname"
#define ALit_maxBufName		"Maximum length of buffer name"
#define ALit_maxMGName		"Maximum length of mode or group name"
#define ALit_maxUserVar		"Maximum length of user variable name"
#define ALit_maxTermInp		"Maximum length of terminal input string"

// Mode descriptions.
#define MLit_AutoSave		"Automatic file save."
#define MLit_AutoTerm		"Automatic termination of last line of buffer when saving."
#define MLit_Backup		"Create backup (" Backup_Ext ") file when saving."
#define MLit_Clobber		"Allow macros to be recreated."
#define MLit_ColDisp		"Display column number of point on mode line."
#define MLit_Exact		"Case-sensitive searches by default."
#define MLit_Fence		"Show matching fence when typing."
#define MLit_HorzScroll		"Horizontally scroll all window lines."
#define MLit_LineDisp		"Display line number of point on mode line."
#define MLit_Overwrite		"Overwrite columns when typing."
#define MLit_ReadOnly		"Read-only buffer."
#define MLit_Regexp		"Regular expression searches by default."
#define MLit_Replace		"Replace characters when typing."
#define MLit_RtnMsg		"Save and display return messages."
#define MLit_SafeSave		"Safe file save (write temporary file first)."
#define MLit_WorkDir		"Display working directory on bottom mode line."
#define MLit_Wrap		"Automatic word wrap."

// Mode group descriptions.
#define MGLit_Typeover		"Typing-overwrite modes."

#elif defined(DATAvar)

// **** For var.c ****

// System variable help text.
#define VLit_ARGV		"Array containing macro arguments, if any, when a macro (or buffer) is being executed."
#define VLit_BufInpDelim	"Line delimiter(s) used for last file read into current buffer."
#define VLit_BufModes		"Array containing names of current buffer modes."
#define VLit_Date		"Current date and time."
#define VLit_GlobalModes	"Array containing names of active global modes."
#define VLit_HorzScrollCol	"First displayed column of current line."
#define VLit_LastKey		"Ordinal (integer) value of last key entered, or -1 if a key sequence was entered or key was\
 not a 7-bit ASCII character."
#define VLit_LineLen		"Length of current line in characters, not including newline delimiter."
#define VLit_Match		"String matching pattern from last buffer search, ~bindex~B function, or =~~ expression."
#define VLit_RegionText		"Text in current region."
#define VLit_ReturnMsg		"Message returned by last command or script."
#define VLit_RunFile		"Pathname of running script."
#define VLit_RunName		"Name of running macro or buffer."
#define VLit_ScreenCount	"Number of existing screens."
#define VLit_TermSize		"Array containing terminal screen size in columns and rows."
#define VLit_WindCount		"Number of windows on current screen."
#define VLit_autoSave		"Keystroke count that triggers auto-save.  Feature is disabled if value is zero."
#define VLit_bufFile		"Filename associated with current buffer."
#define VLit_bufLineNum		"Ordinal number of line containing point in current buffer."
#define VLit_bufName		"Name of current buffer."
#define VLit_execPath		"Colon-separated list of script search directories.  If a directory is null, the current\
 directory is searched."
#define VLit_fencePause		"Centiseconds to pause for fence matching (when Fence mode is enabled)."
#define VLit_hardTabSize	"Size of a hard tab (character) in columns."
#define VLit_horzJump		"Percentage of horizontal window size to jump when scrolling (0-" JumpMaxStr ", 0 for smooth\
 scrolling, default " HorzJumpStr ")."
#define VLit_inpDelim		"Global input line delimiter(s).  Must be one or two characters, or a null string for \"auto\"."
#define VLit_keyMacro		"Last keyboard macro recorded or assigned, in encoded form."
#define VLit_killRingSize	"Maximum size of kill ring (or unlimited if zero)."
#define VLit_lastKeySeq		"Last key sequence entered in string form."
#define VLit_lineChar		"Character at point as an ASCII (int) value."
#define VLit_lineCol		"Column number of point."
#define VLit_lineOffset		"Number of characters in line before point."
#define VLit_lineText		"Text of current line without newline delimiter."
#define VLit_maxArrayDepth	"Maximum depth allowed in array recursion (or unlimited if zero)."
#define VLit_maxLoop	"Maximum number of iterations allowed for a loop block or keyboard macro (or unlimited if zero)."
#define VLit_maxMacroDepth	"Maximum depth allowed in macro recursion (or unlimited if zero)."
#define VLit_maxPromptPct	"Maximum percentage of terminal width to use for a prompt string (in range 15-90)."
#define VLit_otpDelim	"Global output line delimiter(s).  Must be one or two characters.  If null, $BufInpDelim is used."
#define VLit_pageOverlap	"Number of lines to overlap when paging."
#define VLit_randNumSeed	"Random number seed (in range 1 to 2^32 - 1)  Auto-generated if set to zero."
#define VLit_replacePat		"Replacement pattern."
#define VLit_replaceRingSize	"Maximum size of replace ring (or unlimited if zero)."
#define VLit_screenNum		"Ordinal number of current screen."
#define VLit_searchDelim	"Terminal input delimiter key for search and replacement patterns."
#define VLit_searchPat		"Search pattern."
#define VLit_searchRingSize	"Maximum size of search ring (or unlimited if zero)."
#define VLit_softTabSize	"Number of spaces to use for a soft tab."
#define VLit_travJump		"Number of characters to jump when traversing current line with ~btravLine~B command (minimum\
 4, maximum 25% of terminal width, default " TravJumpStr ")."
#define VLit_vertJump		"Percentage of vertical window size to jump when scrolling (0-" JumpMaxStr ", 0 for smooth\
 scrolling, default " VertJumpStr ")."
#define VLit_windLineNum	"Ordinal number of line containing point in current window."
#define VLit_windNum		"Ordinal number of current window."
#define VLit_windSize		"Size of current window in lines, not including mode line."
#define VLit_wordChars		"Characters in a word.  Value may contain character ranges and/or non-printable characters.  If\
 variable is set to nil or a null string, it is set to the default value, \"A-Za-z0-9_\"."
#define VLit_workDir		"Working directory."
#define VLit_wrapCol		"Word wrap column."

#endif
#ifndef DATAmain

// **** For all the other .c files ****

extern char
 *help0,*help1,*help2[],*usage[],
 text0[],text1[],text2[],text3[],text4[],text5[],text6[],text7[],text8[],text9[],text10[],text11[],text12[],text13[],text14[],
 text15[],text16[],text17[],text18[],text19[],text20[],text21[],text22[],text23[],text24[],text25[],text26[],text27[],text28[],
 text29[],text30[],text31[],text32[],text33[],text34[],text35[],text36[],text37[],text38[],text39[],text40[],text41[],text42[],
 text43[],text44[],text45[],text46[],text47[],text48[],text49[],text50[],text51[],text52[],text53[],text54[],text55[],text56[],
 text57[],text58[],text59[],text60[],text61[],text62[],text63[],text64[],text65[],text66[],text67[],text68[],text69[],text70[],
 text71[],text72[],text73[],text74[],text75[],text76[],text77[],text78[],text79[],text80[],text81[],text82[],text83[],text84[],
 text85[],text86[],text87[],text88[],text89[],text90[],text91[],text92[],text93[],text94[],text95[],text96[],text97[],text98[],
 text99[],text100[],text101[],text102[],text103[],text104[],text105[],text106[],text107[],text108[],text109[],text110[],
 text111[],text112[],text113[],text114[],text115[],text116[],text117[],text118[],text119[],text120[],text121[],text122[],
 text123[],text124[],text125[],text126[],text127[],text128[],text129[],text130[],text131[],text132[],text133[],text134[],
 text135[],text136[],text137[],text138[],text139[],text140[],text141[],text142[],text143[],text144[],text145[],text146[],
 text147[],text148[],text149[],text150[],text151[],text152[],text153[],text154[],text155[],text156[],text157[],text158[],
 text159[],text160[],text161[],text162[],text163[],text164[],text165[],text166[],text167[],text168[],text169[],text170[],
 text171[],text172[],text173[],text174[],text175[],text176[],text177[],text178[],text179[],text180[],text181[],text182[],
 text183[],text184[],text185[],text186[],text187[],text188[],text189[],text190[],text191[],text192[],text193[],text194[],
 text195[],text196[],text197[],text198[],text199[],text200[],text201[],text202[],text203[],text204[],text205[],text206[],
 text207[],text208[],text209[],text210[],text211[],text212[],text213[],text214[],text215[],text216[],text217[],text218[],
 text219[],text220[],text221[],text222[],text223[],text224[],text225[],text226[],text227[],text228[],text229[],text230[],
 text231[],text232[],text233[],text234[],text235[],text236[],text237[],text238[],text239[],text240[],text241[],text242[],
 text243[],text244[],text245[],text246[],text247[],text248[],text249[],text250[],text251[],text252[],text253[],text254[],
 text255[],text256[],text257[],text258[],text259[],text260[],text261[],text262[],text263[],text264[],text265[],text266[],
 text267[],text268[],text269[],text270[],text271[],text272[],text273[],text274[],text275[],text276[],text277[],text278[],
 text279[],text280[],text281[],text282[],text283[],text284[],text285[],text286[],text287[],text288[],text289[],text290[],
 text291[],text292[],text293[],text294[],text295[],text296[],text297[],text298[],text299[],text300[],text301[],text302[],
 text303[],text304[],text305[],text306[],text307[],text308[],text309[],text310[],text311[],text312[],text313[],text314[],
 text315[],text316[],text317[],text318[],text319[],text320[],text321[],text322[],text323[],text324[],text325[],text326[],
 text327[],text328[],text329[],text330[],text331[],text332[],text333[],text334[],text335[],text336[],text337[],text338[],
 text339[],text340[],text341[],text342[],text343[],text344[],text345[],text346[],text347[],text348[],text349[],text350[],
 text351[],text352[],text353[],text354[],text355[],text356[],text357[],text358[],text359[],text360[],text361[],text362[],
 text363[],text364[],text365[],text366[],text367[],text368[],text369[],text370[],text371[],text372[],text373[],text374[],
 text375[],text376[],text377[],text378[],text379[],text380[],text381[],text382[],text383[],text384[],text385[],text386[],
 text387[],text388[],text389[],text390[],text391[],text392[],text393[],text394[],text395[],text396[],text397[],text398[],
 text399[],text400[],text401[],text402[],text403[],text404[],text405[],text406[],text407[],text408[],text409[],text410[],
 text411[],text412[],text413[],text414[],text415[],text416[],text417[],text418[],text419[],text420[],text421[],text422[],
 text423[],text424[],text425[],text426[],text427[],text428[],text429[],text430[],text431[],text432[],text433[],text434[],
 text435[],text436[],text437[],text438[],text439[],text440[],text441[],text442[],text443[];
#if MMDebug & Debug_ShowRE
extern char
 text994[],text995[],text996[],text997[],text998[],text999[];
#endif
#endif