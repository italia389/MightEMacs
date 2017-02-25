// (c) Copyright 2017 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// english.h	English language text strings for MightEMacs.

#define Scratch		"scratch"
#define Buffer1		"untitled"

// General text literals.
#define Literal1	"[arg,...]"
#define Literal2	"name = cfm"
#define Literal3	"mode,..."
#define Literal4	"name"
#define Literal5	"name,arg,..."
#define Literal6	"[str]"
#define Literal7	"key-seq,name"
#define Literal8	"name,..."
#define Literal9	"[name]"
#define Literal10	"arg,..."
#define Literal11	"str,repl"
#define Literal12	"str"
#define Literal13	"val"
#define Literal14	"[val[,incr[,fmt]]]"
#define Literal15	"f"
#define Literal16	"key-seq"
#define Literal17	"name[,arg,...]"
#define Literal18	"str,from,to"
#define Literal19	"str,pat"
#define Literal20	"dlm,val,..."
#define Literal21	"N"
#define Literal22	"[m]"
#define Literal23	"array"
#define Literal24	"p[,def[,t[,dlm]]]"
#define Literal25	"array,val"
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
#define Literal36	"f,t"
#define Literal37	"[name,...]"
#define Literal38	"dir"
#define Literal39	"dlm,str[,limit]"
#define Literal40	"var,dlm"
#define Literal41	"var,dlm,val"
#define Literal42	"--- COMMANDS ---"
#define Literal43	"\n--- MACROS ---"
#define Literal44	"\n--- ALIASES ---"

#if defined(DATAmain)

// **** For main.c ****

// Help text.
char
	*Help0 = "    Ver. ",
	*Help1 = " -- Fast and full-featured Emacs text editor\n\n",
	*Usage[] = {
		"Usage:\n    ",
		" {-? | -C | -h | -V}\n    ",
		" [-D<[^]mode>[,...]] [-d<dir>] [-e<stmt>] [-G<[^]mode>[,...]]\n    "
		" [{-g | +}<line>[:<pos>]] [-i<input-delim>] [-N] [-n] [-R] [-r] [-S]\n    "
		" [-s<string>] [-X<exec-path>] [@<script-file>] [file-spec ...]"},
	*Help2 = "\n\n\
Switches and arguments:\n\
  -?		   Display usage and exit.\n\
  -C		   Display copyright information and exit.\n\
  -D<[^]modes>	   Specify one or more default buffer modes to clear (^) or set\n\
		   (no ^), separated by commas.  Switch may be repeated.\n\
  -d<dir>	   Specify a (working) directory to change to.  Switch may be\n\
		   repeated.\n\
  -e<stmt>	   Specify an expression statement to be executed.  Switch may\n\
		   be repeated.\n\
  -G<[^]modes>	   Specify one or more global modes to clear (^) or set (no ^),\n\
		   separated by commas.  Switch may be repeated.\n\
  {-g | +}<line>[:<pos>]\n\
		   Go to specified line number and optional character position\n\
		   on line in buffer of first file after reading it.  Line\n\
		   numbers begin at 0 (end of buffer) and character positions\n\
		   begin at 1.\n\
  -h		   Display detailed help and exit.\n\
  -i<input-delim>  Specify zero, one, or two characters (processed as a double\n\
		   (\") quoted string) to use as file input record delimiter(s).\n\
		   If none specified (the default), then the first NL, CR-LF, or\n\
		   CR found when a file is read will be used.\n\
  -N		   Do not read first file at startup.\n\
  -n		   Do not load the site or user startup file.\n\
  -R		   Not Read-only: open files that follow with Read mode OFF.\n\
		   Switch may be repeated.\n\
  -r		   Read-only: open files that follow with Read mode ON.  Switch\n\
		   may be repeated.\n\
  -S		   Denote file (beginning with \"#!/usr/local/bin/memacs -S\") as\n\
		   an executable " ProgName " script.  Shell arguments are passed\n\
		   to script when file is executed.\n\
  -s<string>	   Search for specified string in buffer of first file after\n\
		   reading it.\n\
  -V		   Display program version and exit.\n\
  -X<exec-path>    Specify colon-separated list of script search directories to\n\
		   prepend to existing path.\n\
  @<script-file>   Execute specified script file (in path) before processing\n\
		   argument(s) that follow it.  May appear anywhere on command\n\
		   line and may be repeated.\n\
\n  file-spec	   Zero or more files to open for viewing or editing.  If \"-\" is\n\
		   specified in place of a filename, data is read from standard\n\
		   input into buffer \"" Buffer1 "\".\n\n\
Notes:\n\
 1. Arguments are processed in the order given before any files are opened or\n\
    the first file is read, unless noted otherwise.\n\
 2. Switches -g and + are mutually exclusive with -s.\n\
 3. The script execution path is initialized to\n\
    \""  MMPath_Default  "\" or the MMPATH environmental\n\
    variable if it is defined.  It may subsequently be changed by the -X switch,\n\
    -e switch, or a @<script-file> invocation.";

// General text messages.
char
 *text0 = "Error",
 *text1 = "Unknown switch, %s",
 *text2 = "Invalid character range '%.3s' in string '%s'",
 *text3 = "%s(): Unknown id %d for var '%s'!",
 *text4 = "%s expected (at token '%s')",
 *text5 = "Group number %ld must be between 0 and %d",
 *text6 = "About",
 *text7 = "Go to",
 *text8 = "Aborted",
 *text9 = "Mark '%c' %s",
 *text10 = "deleted",
 *text11 = "No mark '%c' in this buffer",
 *text12 = "%s (%d) must be between %d and %d",
 *text13 = "Show key ",
 *text14 = "'%s' not bound",
 *text15 = "Bind key ",
 *text16 = "%s(): No such entry '%s' to delete!",
 *text17 = "Cannot bind a key sequence '%s' to '%s' command",
 *text18 = "Unbind key ",
 *text19 = "No such kill %d (max %d)",
 *text20 = "Apropos",
 *text21 = "BindingList",
 *text22 = "Extraneous token '%s'",
 *text23 = "Missing '=' in %s command (at token '%s')",
 *text24 = "Switch to",
 *text25 = "That name is already in use.  New name",
 *text26 = "Delete",
 *text27 = "Pop",
 *text28 = "%s is being displayed",
 *text29 = "Change buffer name to",
 *text30 = "ACHMPN    Size       Buffer               File",
 *text31 = "global",
 *text32 = "Discard changes",
 *text33 = "Cannot get %s of file \"%s\": %s",
 *text34 = "File: ",
 *text35 = "$autoSave not set",
 *text36 = "Invalid pattern option '%c' for %s operator",
 *text37 = "pathname",
 *text38 = "Invalid number '%s'",
 *text39 = "%s (%d) must be %d or greater",
 *text40 = "Script file \"%s\" not found%s",
 *text41 = "Enter \"%s\" for help, \"%s\" to quit",
 *text42 = "Kill ring cycled",
 *text43 = " specified with -%c switch",
 *text44 = "calling %s() from %s() function",
 *text45 = "%s switch requires a value",
 *text46 = "Input",
 *text47 = "Output",
 *text48 = "[Not bound]",
 *text49 = "Hard",
 *text50 = "Soft",
 *text51 = "Assign variable",
 *text52 = "No such variable '%s'",
 *text53 = "Value",
 *text54 = "Key echo required for completion",
 *text55 = "Insert",
 *text56 = "VariableList",
 *text57 = "Argument expected",
 *text58 = "Buffer",
 *text59 = "Wrap column",
 *text60 = "Line %lu/%lu, Col %d/%d, Char %lu/%lu (%s%%), char = %s",
 *text61 = ", switch '%s'",
 *text62 = "default",
 *text63 = " mode",
 *text64 = "Set",
 *text65 = "Clear",
 *text66 = "Unknown or ambiguous mode '%s'",
 *text67 = "Function call",
 *text68 = "Identifier",
 *text69 = "Wrong number of arguments (at token '%s')",
 *text70 = "Region copied",
 *text71 = "%s '%s' is already narrowed",
 *text72 = "Cannot execute a narrowed buffer",
 *text73 = "%s narrowed",
 *text74 = "%s '%s' is not narrowed",
 *text75 = "%s widened",
 *text76 = "Mark (%d) must be between %d and %d",
 *text77 = "%s() bug: Mark '%c' not found in buffer '%s'!",
 *text78 = "Search",
 *text79 = "Not found",
 *text80 = "No pattern set",
 *text81 = "Reverse search",
 *text82 = "Variable name",
 *text83 = "buffer",
 *text84 = "Replace",
 *text85 = "Query replace",
 *text86 = "with",
 *text87 = "Replace '%s' with '",
 *text88 = "Cannot read directory \"%s\": %s",
 *text89 = "Screen Window      Buffer                File",
 *text90 = "(SPC,y) Yes (n) No (!) Do rest (u) Undo last (ESC,q) Stop here (.) Stop and go back (?) Help",
 *text91 = "Repeating match at same position detected",
 *text92 = "%d substitution%s",
 *text93 = "Current window too small to shrink by %d line%s",
 *text94 = "%s(): Out of memory!",
 *text95 = "Narrowed buffer... delete visible portion",
 *text96 = "Empty character class",
 *text97 = "Character class not ended",
 *text98 = "Wrap column not set",
 *text99 = "file",
#if WordCount
 *text100 = "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f",
#endif
 *text101 = "Cannot search and goto at the same time!",
 *text102 = "Beginning value",
 *text103 = "Saving %s...",
 *text104 = "Modified buffers exist.  Leave anyway",
 *text105 = "Keyboard macro already active, cancelled",
 *text106 = "Begin keyboard macro",
 *text107 = "Keyboard macro not active",
 *text108 = "End keyboard macro",
 *text109 = "%s is in read-only mode",
 *text110 = "Prompt string required for",
 *text111 = "'%s' value must be %d or greater",
 *text112 = "Maximum number of loop iterations (%d) exceeded!",
 *text113 = "%s hidden",
 *text114 = "Prior loop execution level not found while rewinding stack",
 *text115 = "Word",
 *text116 = "No such macro '%s'",
 *text117 = "Execute",
 *text118 = "No such buffer '%s'",
 *text119 = "Pause duration",
 *text120 = "'break' or 'next' outside of any loop block",
 *text121 = "Unmatched '%s' keyword",
 *text122 = "loop",
 *text123 = "Unterminated string %s",
 *text124 = "Cannot insert current buffer into itself",
 *text125 = "CompletionList",
 *text126 = "Script loop boundary line not found",
 *text127 = "alias",
 *text128 = "Invalid buffer name '%s'",
 *text129 = "Execute script file",
 *text130 = "No such command or macro '%s'",
 *text131 = "Read file",
 *text132 = "Insert file",
 *text133 = "Find file",
 *text134 = "View file",
 *text135 = "Old buffer",
 *text136 = " in path \"%s\"",
 *text137 = "Repeat count",
 *text138 = "New file",
 *text139 = "Reading data...",
 *text140 = "Read",
 *text141 = "I/O ERROR: %s, file \"%s\"",
 *text142 = "Cannot change a character past end of buffer '%s'",
 *text143 = "Line number",
 *text144 = "Write file",
 *text145 = "No filename associated with buffer '%s'",
 *text146 = "Truncated file in buffer '%s'... write it out",
 *text147 = "Narrowed buffer '%s'... write it out",
 *text148 = "Writing data...",
 *text149 = "Wrote",
 *text150 = "(file saved as \"%s\") ",
 *text151 = "Change filename to",
 *text152 = "No such file \"%s\"",
 *text153 = "Inserting data...",
 *text154 = "Inserted",
 *text155 = "%s(): Out of memory saving filename \"%s\"!",
 *text156 = "%s(): Out of memory allocating %u-byte line buffer for file \"%s\"!",
 *text157 = "%s(): key map space (%d entries) exhausted!",
 *text158 = "command",
 *text159 = "Buffers",
 *text160 = "Screens",
 *text161 = " (disabled '%s' hook)",
 *text162 = " (y/n)?",
 *text163 = "status",
 *text164 = "Cannot modify read-only variable '%s'",
 *text165 = "Name '%s' already in use",
 *text166 = "Integer expected",
 *text167 = "%u buffer%s saved",
 *text168 = "Nuke all other visible buffers",
 *text169 = "Clear",
 *text170 = "Read pipe",
 *text171 = "String expected",
 *text172 = "Token expected",
 *text173 = "Unbalanced %c%c%c string parameter",
 *text174 = "Created screen %hu",
 *text175 = "Command '%s' failed",
 *text176 = "Macro '%s' failed",
 *text177 = "%s(): Screen number %d not found in screen list!",
 *text178 = "Deleted screen %d",
 *text179 = "%s(): Unknown id %d for variable '%s'!",
 *text180 = "create",
 *text181 = "%s name '%s' already in use",
 *text182 = "Environmental variable 'TERM' not defined!",
 *text183 = "Unknown terminal type '%s'!",
 *text184 = "Overlap %ld must be between 0 and %d",
 *text185 = "Version",
 *text186 = "Unhide",
 *text187 = "%s cannot be null",
 *text188 = "End",
 *text189 = "Abort",
 *text190 = "Terminal size %hu x %hu is too small to run %s",
 *text191 = "Wrong type of operand for '%s'",
 *text192 = "This terminal (type '%s') does not have sufficient capabilities to run %s",
 *text193 = "Saved file \"%s\"\n",
 *text194 = "Shell command '%s' failed",
 *text195 = "Hide",
 *text196 = "endloop",
 *text197 = "endmacro",
 *text198 = "Misplaced '%s' keyword",
 *text199 = "if",
 *text200 = "No keyboard macro defined",
 *text201 = "End: ",
 *text202 = "(SPC,f",
 *text203 = ") +page (b",
 *text204 = ") -page (d) +half (u) -half",
 *text205 = "line",
 *text206 = " (g) first (G) last (ESC,q) quit (?) help: ",
 *text207 = "Cannot get %d line%s from adjacent window",
 *text208 = "No saved %s to restore",
 *text209 = "Arg: %d",
 *text210 = "'%s' is only binding to core command '%s' -- cannot delete or reassign",
 *text211 = "FunctionList",
 *text212 = "Variable '%s' not an integer",
 *text213 = "Comma",
 *text214 = "'prompt' function",
 *text215 = "Create alias ",
 *text216 = "Delete macro",
 *text217 = "'break' level '%ld' must be 1 or greater",
 *text218 = "Append file",
 *text219 = "Script failed",
 *text220 = "Parent block of loop block at line %ld not found during buffer scan",
 *text221 = "Too many groups in RE pattern '%s' (maximum is %d)",
 *text222 = "RE group not ended",
 *text223 = "Size argument",
 *text224 = "Line offset value %ld out of range",
 *text225 = "Too many break levels (%d short) from inner 'break'",
 *text226 = "Cannot %s %s buffer '%s'",
 *text227 = "Terminal dimensions set to %hu x %hu",
 *text228 = "Kill ring cleared",
 *text229 = ", in",
 *text230 = "at line",
 *text231 = "Toggle",
 *text232 = "Macro name '%s' cannot exceed %d characters",
 *text233 = "Mark '%c' set to previous position",
 *text234 = "Increment",
 *text235 = "Format string",
 *text236 = "i increment cannot be zero!",
 *text237 = "Invalid i format '%s' (must contain exactly one %%d, %%o, %%u, %%x, or %%X)",
 *text238 = "executing",
 *text239 = "No such window '%d'",
 *text240 = "No such screen '%d'",
 *text241 = "Screen is being displayed",
 *text242 = "No arguments found following %s switch",
 *text243 = "Delete screen",
 *text244 = "No such command, alias, or macro '%s'",
 *text245 = "Division by zero is undefined (%ld/0)",
 *text246 = "Help hook not set",
 *text247 = "function",
 *text248 = "an executing",
 *text249 = "Insert pipe",
 *text250 = "Unchange",
 *text251 = "%s delimiter '%s' cannot be more than %d character(s)",
 *text252 = " delimited by ",
 *text253 = "(return status %d)\n",
 *text254 = "Invalid key literal '%s'",
 *text255 = "Invalid repetition operand in RE pattern '%s'",
 *text256 = "%s tab size %ld must be between 2 and %d",
 *text257 = ", original file renamed to \"",
 *text258 = "Unmatched right paren in RE pattern '%s'",
 *text259 = "No text selected",
 *text260 = "Line",
 *text261 = "Text",
 *text262 = "copied",
 *text263 = "delete",
 *text264 = "clear",
 *text265 = "Cannot change to directory \"%s\": %s",
 *text266 = "Regular expression",
 *text267 = "command or macro",
 *text268 = "Cannot %s buffer: name '%s' cannot begin with %c",
 *text269 = "Delete alias",
 *text270 = "Cannot %s macro buffer: name '%s' must begin with %c",
 *text271 = "No such alias '%s'",
 *text272 = "Macro has %hu alias(es)",
 *text273 = "Macro buffer names (only) begin with ",
 *text274 = "WD: ",
 *text275 = "rename",
 *text276 = "modify",
 *text277 = "Change directory",
 *text278 = " set to ",
 *text279 = "Variable",
 *text280 = "%s name cannot be null or exceed %d characters",
 *text281 = "Missing spec in '%' format string",
 *text282 = "'%s' command not allowed in a script (use \"run\")",
 *text283 = "Numeric prefix",
 *text284 = "Cannot %s %s buffer",
 *text285 = "Call argument",
 *text286 = "Invalid identifier '%s'",
 *text287 = "i variable set to %d",
 *text288 = "Function",
 *text289 = "Unexpected token '%s'",
 *text290 = "Length argument",
 *text291 = "Delimiter '%s' must be a single character",
 *text292 = "variable",
 *text293 = "Cannot split a %d-line window",
 *text294 = "Only one window",
 *text295 = "prompt type '%s' must be b, c, f, s, v, or V",
 *text296 = "show",
 *text297 = "modes",
 *text298 = "Unknown or conflicting bit(s) in mode word '0x%.8x'",
 *text299 = "Pop file",
 *text300 = "False return",
 *text301 = "Expression",
 *text302 = "No such group (ref: %d, have: %d) in replacement pattern '%s'",
 *text303 = "Invalid wrap prefix \"%s\"",
 *text304 = "Closure on group not supported in RE pattern '%s'",
 *text305 = "KillList",
#if MMDebug & Debug_ShowRE
 *text306 = "RegexpList",
 *text307 = "Forward",
 *text308 = "Backward",
 *text309 = "SEARCH",
 *text310 = "REPLACEMENT",
 *text311 = "PATTERN",
#endif
 *text312 = "No such command, function, or macro '%s'",
 *text313 = "command, function, or macro",
 *text314 = "Cannot execute alias '%s' interactively",
 *text315 = "Hook Name    Set to",
 *text316 = "Hooks",
 *text317 = "No such hook '%s'",
 *text318 = "Set hook ",
 *text319 = "Maximum %s recursion depth (%d) exceeded",
 *text320 = "More than one spec in '%' format string",
 *text321 = "Unknown format spec '%%%c'",
 *text322 = "Column number",
 *text323 = "Indentation exceeds wrap column (%d)",
 *text324 = ".  New name",
 *text325 = "for",
 *text326 = "Begin",
 *text327 = "Macro bound to '%s' hook",
 *text328 = "tr \"from\" string",
 *text329 = "Unexpected %s argument",
 *text330 = "Kill  Text",
 *text331 = "window",
 *text332 = "Soft tab size set to %d",
 *text333 = "pseudo-command",
 *text334 = ", setting variable '%s'",
 *text335 = "File test code(s)",
 *text336 = "macro",
 *text337 = "Invalid \\nn sequence in string %s",
 *text338 = "Cannot access '$keyMacro' from a keyboard macro, cancelled",
 *text339 = "to",
 *text340 = "Col %d/%d, char = %s",
 *text341 = "Cannot use key sequence '%s' as %s delimiter",
 *text342 = "prompt",
 *text343 = "search",
 *text344 = "%s unhidden",
 *text345 = "' must be a printable character",
 *text346 = "%s mark",
 *text347 = "Swap dot with",
 *text348 = "Save dot in",
 *text349 = "Mark value '",
 *text350 = "set",
 *text351 = "All marks deleted",
 *text352 = "Cannot delete mark '%c'",
 *text353 = "MarkList",
 *text354 = "Mark  Offset  Line text",
 *text355 = " and marked as region",
 *text356 = "Too many windows (%u)",
 *text357 = "Wrong type of argument for '%s' function",
 *text358 = "Illegal use of %s value",
 *text359 = "nil",
 *text360 = "Boolean",
 *text361 = "No mark found to delete",
 *text362 = "Unknown file test code '%c'",
 *text363 = "ModeList",
 *text364 = "GLOBAL MODES",
 *text365 = "BUFFER MODES",
 *text366 = "\n\n* Active global or buffer mode\n+ Active show or default mode",
 *text367 = "Deleting buffer '%s'...",
 *text368 = "%u buffer%s deleted",
 *text369 = " in buffer '%s'",
 *text370 = "Array element %d not an integer",
 *text371 = "Array expected",
 *text372 = "Deleted buffer '%s'",
 *text373 = "Illegal value for '%s' variable",
 *text374 = "Endless recursion detected (array contains itself)",
 *text375 = "usage",
 *text376 = "'%s' keyword outside of macro block",
 *text377 = "Too many arguments for 'sprintf' function",
 *text378 = "Macro argument count",
 *text379 = "Searching...";
#if Color
char
 *text500 = "Invalid palette value '%s'",
 *text501 = "No such color '%s'",
 *test502 = "Palette",
 *text503 = "%s string '%s' too long";
#endif

// Function help text.
#define FLit_abort		"Return \"abort\" status and optional message"
#define FLit_about		"Pop-up \"about the editor\" information (with \"selectBuf\" options)"
#define FLit_abs		"Absolute value of N"
#define FLit_alias		"Create alias name for command, function, or macro cfm"
#define FLit_alterBufMode	"Clear (n < 0), toggle (n == 0, default), or set (n > 0) current buffer mode(s)"
#define FLit_alterDefMode	"Clear (n < 0), toggle (n == 0, default), or set (n > 0) default buffer mode(s)"
#define FLit_alterGlobalMode	"Clear (n < 0), toggle (n == 0, default), or set (n > 0) global mode(s)"
#define FLit_alterShowMode	"Clear (n < 0), toggle (n == 0, default), or set (n > 0) global mode(s) to show on mode line"
#define FLit_appendFile		"Append current buffer to named file"
#define FLit_array		"Create array, given optional size and initial value of each element"
#define FLit_backChar		"Move point backward [-]n characters (default 1)"
#define FLit_backLine		"Move point backward [-]n lines (default 1)"
#define FLit_backPage		"Scroll current window backward [-]n pages (default 1)"
#define FLit_backPageNext	"Scroll next window backward [-]n pages (default 1)"
#define FLit_backPagePrev	"Scroll previous window backward [-]n pages (default 1)"
#define FLit_backTab		"Move point backward [-]n tab stops (default 1)"
#define FLit_backWord		"Move point backward [-]n words (default 1)"
#define FLit_basename		"Filename component of pathname f (without extension if n <= 0)"
#define FLit_beep		"Sound beeper n times (default 1)"
#define FLit_beginBuf		"Move point to beginning of current buffer (or named buffer if n arg)"
#define FLit_beginKeyMacro	"Begin recording keyboard macro"
#define FLit_beginLine		"Move point to beginning of [-]nth line"
#define FLit_beginText		"Move point to beginning of [-]nth line text"
#define FLit_beginWhite		"Move point to beginning of white space surrounding point on current line"
#define FLit_binding		"Name of command or macro bound to given key, or nil if none"
#define FLit_bindKey		"Bind named command or macro to a key sequence (or single key if n <= 0)"
#define FLit_bufBoundQ		"Point at beginning, middle, end, or either end of buffer for -, 0, +, or default n?"
#define FLit_bufList		"List of visible buffers (plus hidden if n <= 0, all if n > 0)"
#define FLit_bufSize		"Current buffer size in lines (or bytes if n arg)"
#define FLit_chDir		"Change working directory"
#define FLit_chr		"String form of an ASCII (int) character"
#define FLit_clearBuf		"Clear current buffer (or named buffer if n >= 0, force if n != 0)"
#define FLit_clearKillRing	"Clear all entries in kill ring"
#define FLit_clearMsg		"Clear the message line (force if n > 0)"
#define FLit_clone		"Copy of given array"
#define FLit_copyFencedText	"Copy text from point to matching ( ), [ ], or { } to kill buffer"
#define FLit_copyLine		"Copy [-]n lines to kill buffer (default 1, region lines if n == 0)"
#define FLit_copyRegion		"Copy text in current region to kill buffer"
#define FLit_copyToBreak	"Copy from point through [-]n lines to kill buffer (default 1)"
#define FLit_copyWord		"Copy [-]n words to kill buffer (default 1, without trailing white space if n == 0)"
#if WordCount
#define FLit_countWords		"Show counts in current region"
#endif
#define FLit_cycleKillRing	"Cycle kill ring [-]n times (default 1)"
#define FLit_definedQ	"Name defined? (or mark if n arg, active mark if n > 0); returns \"alias...variable|nil|true|false\""
#define FLit_deleteAlias	"Delete named alias(es)"
#define FLit_deleteBackChar	"Delete backward [-]n characters (default 1)"
#define FLit_deleteBackTab	"Delete backward [-]n hard tabs or \"chunks\" of space characters (default 1)"
#define FLit_deleteBlankLines	"Delete blank lines around point (or below it if current line not blank)"
#define FLit_deleteBuf		"Delete named buffer(s) (and force, all unchanged, all, force all for -, 0, 1, or other n)"
#define FLit_deleteFencedText	"Delete text from point to matching ( ), [ ], or { }"
#define FLit_deleteForwChar	"Delete forward [-]n characters (default 1)"
#define FLit_deleteForwTab	"Delete forward [-]n hard tabs or \"chunks\" of space characters (default 1)"
#define FLit_deleteLine		"Delete [-]n lines (default 1, region lines if n == 0)"
#define FLit_deleteMacro	"Delete named macro(s)"
#define FLit_deleteMark		"Delete mark m (or all marks if n arg)"
#define FLit_deleteRegion	"Delete text in current region"
#define FLit_deleteScreen	"Delete screen n"
#define FLit_deleteToBreak	"Delete from point through [-]n lines (default 1)"
#define FLit_deleteWhite	"Delete white space (and non-word characters if n > 0) surrounding point on current line"
#define FLit_deleteWind		"Delete current window and join upward (or downward if n > 0, and delete buffer if n == -1)"
#define FLit_deleteWord		"Delete [-]n words (default 1, without trailing white space if n == 0)"
#define FLit_detabLine		"Change tabs to spaces in [-]n lines (default 1, region lines if n == 0)"
#define FLit_dirname		"Directory component of pathname f (\"\" if none, \".\" if none and n arg)"
#define FLit_dupLine		"Duplicate [-]n lines (default 1, region lines if n == 0)"
#define FLit_emptyQ		"Nil value, null string, or empty array?"
#define FLit_endBuf		"Move point to end of current buffer (or named buffer if n arg)"
#define FLit_endKeyMacro	"End recording keyboard macro"
#define FLit_endLine		"Move point to end of [-]nth line"
#define FLit_endWhite		"Move point to end of white space surrounding point on current line"
#define FLit_endWord		"Move point to end of [-]nth word (default 1)"
#define FLit_entabLine		"Change spaces to tabs in [-]n lines (default 1, region lines if n == 0)"
#define FLit_env		"Value of given environmental variable"
#define FLit_eval		"Concatenate arguments (or get string if interactive, expression if n arg) and execute result"
#define FLit_exit		"Exit editor with optional message (and ignore changes if n != 0, force -1 return if n < 0)"
#define FLit_findFile		"Find named file and read (if n != 0) into new buffer (with \"selectBuf\" options)"
#define FLit_forwChar		"Move point forward [-]n characters (default 1)"
#define FLit_forwLine		"Move point forward [-]n lines (default 1)"
#define FLit_forwPage		"Scroll current window forward [-]n pages (default 1)"
#define FLit_forwPageNext	"Scroll next window forward [-]n pages (default 1)"
#define FLit_forwPagePrev	"Scroll previous window forward [-]n pages (default 1)"
#define FLit_forwTab		"Move point forward [-]n tab stops (default 1)"
#define FLit_forwWord		"Move point forward [-]n words (default 1)"
#define FLit_getKey	"Get one keystroke (or key sequence if n > 1, and return ASCII (int) character if n <= 0, default 1)"
#define FLit_gotoFence		"Move point to matching ( ), [ ], { }, or < >"
#define FLit_gotoLine		"Move point to beginning of line N (or to end of buffer if N == 0, in named buffer if n arg)"
#define FLit_gotoMark		"Move point to mark m (and delete mark if n arg)"
#define FLit_growWind		"Increase size of current window by n lines (default 1 at bottom, at top if n < 0)"
#define FLit_help		"Run macro assigned to \"help\" hook"
#define FLit_hideBuf		"Set the \"hidden\" flag of the current buffer (or named buffer if n arg)"
#define FLit_huntBack		"Repeat search backward [-]n times (default 1)"
#define FLit_huntForw		"Repeat search forward [-]n times (default 1)"
#define FLit_includeQ		"Value in array?"
#define FLit_indentRegion	"Indent lines in region by n tab stops (default 1)"
#define FLit_index	"Position of pattern in str, or nil if not found (first position is 0, rightmost occurrence if n > 0)"
#define FLit_insert	"Insert concatenated args at point n times (without moving point if n == 0, literal newline if n < 0)"
#define FLit_insertBuf		"Insert named buffer before current line (without moving point if n == 0) and mark as region"
#define FLit_insertFile		"Insert named file before current line (without moving point if n == 0) and mark as region"
#define FLit_inserti		"Format, insert at point, and add increment to i variable [-]n times"
#define FLit_insertLineI	"Insert and indent a line before the [-]nth line (default 1)"
#define FLit_insertPipe		"Execute shell command and insert result before current line (without moving point if n == 0)"
#define FLit_insertSpace	"Insert n spaces ahead of point"
#define FLit_join	"Join all arguments with given delimiter (ignoring nil values if n == 0, and null values if n < 0)"
#define FLit_joinLines		"Join [-]n lines, replacing white space with single space (default -1, region lines if n == 0)"
#define FLit_joinWind		"Join current window with lower window (or upper window if n < -1)"
#define FLit_kill		"Text from kill number N (where N <= 0)"
#define FLit_killFencedText	"Delete/save text from point to matching ( ), [ ], or { } to kill buffer"
#define FLit_killLine		"Delete/save [-]n lines to kill buffer (default 1, region lines if n == 0)"
#define FLit_killRegion		"Delete/save text in current region to kill buffer"
#define FLit_killToBreak	"Delete/save from point through [-]n lines to kill buffer (default 1)"
#define FLit_killWord		"Delete/save [-]n words to kill buffer (default 1, without trailing white space if n == 0)"
#define FLit_lastBuf		"Switch to last buffer exited from (and delete current if n < 0)"
#define FLit_lcLine		"Change [-]n lines to lower case (default 1, region lines if n == 0)"
#define FLit_lcRegion		"Change all letters in current region to lower case"
#define FLit_lcString		"Lower-cased string"
#define FLit_lcWord		"Change n words to lower case (default 1)"
#define FLit_length		"Length of string or size of an array"
#define FLit_let		"Assign a value (or an expression if n arg) to a system or global variable"
#define FLit_markBuf		"Save point position in mark '.' (or mark m if n >= 0) and mark whole buffer as a region"
#define FLit_match	"Text matching group N from last =~ or index op (0 for entire match, from last buffer search if n arg)"
#define FLit_metaPrefix		"Meta prefix key"
#define FLit_moveWindDown	"Move window frame [-]n lines down (default 1)"
#define FLit_moveWindUp		"Move window frame [-]n lines up (default 1)"
#define FLit_narrowBuf		"Keep [-]n lines (default 1, region lines if n == 0) and hide and preserve remainder"
#define FLit_negativeArg	"Enter universal negative-trending n argument"
#define FLit_newline		"Insert n lines (default 1, with auto-formatting if language or wrap mode active)"
#define FLit_newlineI		"Insert n lines with same indentation as previous line (default 1)"
#define FLit_newScreen		"Create new screen and switch to it"
#define FLit_nextBuf		"Switch to next buffer n times (or get its name only if n == 0, delete current if n < 0)"
#define FLit_nextScreen		"Switch to next screen (or #n if n > 0, nth from last if n < 0)"
#define FLit_nextWind		"Switch to next window ([-]nth from bottom or top if n arg)"
#define FLit_nilQ		"Nil value?"
#define FLit_notice	"Return concatenated message (not in brackets if n <= 0) and true value (or false value if n < 0)"
#define FLit_nullQ		"Null string?"
#define FLit_numericQ		"Numeric string?"
#define FLit_onlyWind		"Make current window the only window on screen"
#define FLit_openLine		"Insert n lines ahead of point (default 1)"
#define FLit_ord		"ASCII integer value of a string character"
#define FLit_outdentRegion	"Outdent lines in region by n tab stops (default 1)"
#define FLit_overwrite		"Overwrite text at point n times with concatenated arguments (with \"insert\" options)"
#define FLit_pathname		"Absolute pathname of file f (without resolving a symbolic link if n <= 0)"
#define FLit_pause		"Pause execution for n centiseconds (default 100)"
#define FLit_pipeBuf		"Feed current buffer to shell command and replace with result"
#define FLit_pop		"Remove last element from given array and return it, or nil if none left"
#define FLit_prefix1		"Prefix 1 key"
#define FLit_prefix2		"Prefix 2 key"
#define FLit_prefix3		"Prefix 3 key"
#define FLit_prevBuf		"Switch to previous buffer n times (or get its name only if n == 0, delete current if n < 0)"
#define FLit_prevScreen		"Switch to previous screen (or #n if n > 0, nth from last if n < 0)"
#define FLit_prevWind		"Switch to previous window (or [-]nth from bottom or top if n arg)"
#define FLit_print		"Write concatenated arguments to message line (with literal nil if n <= 0, force if n != 0)"
#define FLit_prompt	"Term inp w/prompt,default,type([^]no auto,[b]uf,[c]har,[f]ile,[s]tr,[v|V]ar),delim (no echo if n == 0)"
#define FLit_push		"Append value to given array and return new array value"
#define FLit_queryReplace	"Replace n occurrences of one string with another interactively (default all)"
#define FLit_quickExit		"Exit editor, saving any modified buffers"
#define FLit_quote	"Value converted to string form which can be returned to the original value via \"eval\" (force if n > 0)"
#define FLit_quoteChar		"Insert a character read from the keyboard literally n times (default 1)"
#define FLit_rand		"Pseudo-random integer in range 1..N (where N > 1)"
#define FLit_readBuf		"Return nth next line from named buffer (default 1, from point position), or nil if none left"
#define FLit_readFile		"Read named file into current buffer (or new buffer if n arg, with \"selectBuf\" options)"
#define FLit_readPipe		"Execute shell command and save result in current buffer (with \"readFile\" options)"
#define FLit_redrawScreen	"Redraw screen (with point at: current, +nth, -nth, center window line if 0, +, -, default n)"
#define FLit_replace		"Replace n occurrences of one string with another (default all)"
#define FLit_replaceText	"Replace text at point n times with concatenated arguments (with \"insert\" options)"
#define FLit_resetTerm		"Update all screens and windows to match current terminal dimensions (force if n > 0)"
#define FLit_resizeWind		"Change size of current window to n lines (or make all windows the same size if n == 0)"
#define FLit_restoreBuf		"Switch to remembered buffer in current window"
#define FLit_restoreWind	"Switch to remembered window on current screen"
#define FLit_run		"Execute named command, alias, or macro interactively with arg n"
#define FLit_saveBuf		"Remember current buffer"
#define FLit_saveFile		"Save current buffer (if changed) to its associated file (or all changed buffers if n > 0)"
#define FLit_saveWind		"Remember current window"
#define FLit_scratchBuf		"Create and switch to a buffer with unique name (with \"selectBuf\" options)"
#define FLit_searchBack		"Search backward for the [-]nth occurrence of a string (default 1)"
#define FLit_searchForw		"Search forward for the [-]nth occurrence of a string (default 1)"
#define FLit_selectBuf	"(Create|switch to) named buffer (and alt-pop|pop|stay|switch|\"windowize\" if <-1|-1|0|1|>1 n)"
#define FLit_setBufFile		"Associate file with current buffer and derive new buffer name from it (unless n <= 0)"
#define FLit_setBufName		"Set name of current buffer (or derive from associated filename if n > 0)"
#define FLit_seti		"Set i variable to a value (or n arg), increment, and printf format string"
#define FLit_setHook		"Set named hook to command or macro cm"
#define FLit_setMark		"Set mark ' ', '.', or m to point position for default, -, or other n"
#define FLit_setWrapCol		"Set wrap column to n (default 0)"
#define FLit_shell		"Start a shell session"
#define FLit_shellCmd		"Execute a shell command and (if interactive) pause to display result"
#define FLit_shift		"Remove first element from given array and return it, or nil if none left"
#define FLit_showBindings	"Pop-up command and binding list (containing str if n arg, with \"selectBuf\" options)"
#define FLit_showBuffers	"Pop-up visible buffers list (all if n arg, with \"selectBuf\" options)"
#define FLit_showFunctions	"Pop-up system function list (containing str if n arg, with \"selectBuf\" options)"
#define FLit_showHooks		"Pop-up hook list (with \"selectBuf\" options)"
#define FLit_showKey		"Display name of command or macro bound to key sequence (or single key if n <= 0)"
#define FLit_showKillRing	"Pop-up kill ring entries (with \"selectBuf\" options)"
#define FLit_showMarks		"Pop-up buffer-mark list (with \"selectBuf\" options)"
#define FLit_showModes		"Pop-up mode list (with \"selectBuf\" options)"
#if MMDebug & Debug_ShowRE
#define FLit_showRegexp		"Pop-up search and replacement metacharacter arrays (with \"selectBuf\" options)"
#endif
#define FLit_showScreens	"Pop-up screen list (with \"selectBuf\" options)"
#define FLit_showVariables	"Pop-up variable list (containing str if n arg, with \"selectBuf\" options)"
#define FLit_shQuote		"Value enclosed in ' quotes with ' characters converted to \\' (for literal shell argument)"
#define FLit_shrinkWind		"Decrease size of current window by n lines (default 1 at bottom, at top if n < 0)"
#define FLit_space		"Insert n spaces (default 1, with auto-wrap if wrap mode active)"
#define FLit_split	"Split string into an array using given delimiter (' ' for white space) and optional max. no. elements"
#define FLit_splitWind		"Split current window (and shrink upper, go to non-default, set upper size if -, 0, + n)"
#define FLit_sprintf		"String built from specified format string and arguments"
#define FLit_statQ	"Test file f for t(s): [d]ir,[e]xists,[f]ile,sym[L]ink,hard[l]ink,[r]ead,[s]ize > 0,[w]rite,[x]eq?"
#define FLit_strPop	"Remove last portion of string variable var using delimiter dlm (' ' for white space) and return it"
#define FLit_strPush		"Append value to string variable var using delimiter dlm and return new variable value"
#define FLit_strShift	"Remove first portion of string variable var using delimiter dlm (' ' for white space) and return it"
#define FLit_strUnshift		"Prepend value to string variable var using delimiter dlm and return new variable value"
#define FLit_stringFit		"Condensed string with embedded ellipsis (max size n)"
#define FLit_strip		"Whitespace-stripped string (or left end only, right end only if -, + n)"
#define FLit_sub	"Result of first from pattern found in str replaced with to pattern (or all occurrences if n > 1)"
#define FLit_subLine		"Extract substring from current line at point (first pos is 0, infinite length if no len)"
#define FLit_subString		"Extract substring from a string (first pos is 0, infinite length if no len)"
#define FLit_suspend		"Suspend editor (put in background) and return to shell session"
#define FLit_swapMark		"Swap point position and mark ' ', '.', or m for default, -, or other n"
#define FLit_sysInfo		"Operating system, language of text messages, editor name, or version for default, -, 0, or + n"
#define FLit_tab		"Insert tab character or spaces n times (or set soft tab size to abs(n) if n <= 0)"
#define FLit_tcString		"Title-cased string"
#define FLit_tcWord		"Change n words to title case (default 1)"
#define FLit_toInt		"Numeric string literal converted to integer"
#define FLit_toString		"Value as string (with literal nil if n < 0, in visible form if n == 0, plus ' quotes if n > 0)"
#define FLit_tr			"Translated string (\"to\" string may be nil or null, or shorter than \"from\" string)"
#define FLit_traverseLine	"Move point forward or backward $travJump characters (or to far right if n == 0)"
#define FLit_trimLine		"Trim white space from end of [-]n lines (default 1, region lines if n == 0)"
#define FLit_truncBuf		"Delete all text from point to end of buffer"
#define FLit_typeQ		"Returns type of value: \"array\", \"bool\", \"int\", \"nil\", or \"string\""
#define FLit_ucLine		"Change [-]n lines to upper case (default 1, region lines if n == 0)"
#define FLit_ucRegion		"Change all letters in current region to upper case"
#define FLit_ucString		"Upper-cased string"
#define FLit_ucWord		"Change n words to upper case (default 1)"
#define FLit_unbindKey		"Unbind a key sequence (or single key if n <= 0; ignore any error if script mode and n > 0)"
#define FLit_unchangeBuf	"Clear the \"changed\" flag of the current buffer (or named buffer if n arg)"
#define FLit_undelete		"Insert text from last delete command (without moving point if n == 0)"
#define FLit_unhideBuf		"Clear the \"hidden\" flag of the current buffer (or named buffer if n arg)"
#define FLit_universalArg	"Enter universal positive-trending n argument"
#define FLit_unshift		"Prepend value to given array and return new array value"
#define FLit_updateScreen	"Redraw screen (force if n > 0)"
#define FLit_viewFile		"Find named file and read (if n != 0) into new read-only buffer (with \"selectBuf\" options)"
#define FLit_whence		"Display buffer position and current character info (for current line only if n arg)"
#define FLit_widenBuf		"Expose all hidden lines"
#define FLit_windList		"Window list in form [[wind,bufName],...] (or form [[screen,wind,bufName],...] if n arg)"
#define FLit_wordCharQ		"String character in $wordChars?"
#define FLit_wrapLine		"Rewrap [-]n lines at $wrapCol (default 1, region lines if n == 0)"
#define FLit_wrapWord		"Move word at point (if past column n) or following it (otherwise) to beginning of next line"
#define FLit_writeBuf		"Write concatenated arguments to named buffer n times (with \"insert\" options)"
#define FLit_writeFile		"Write current buffer to named file and derive new buffer name from it (unless n <= 0)"
#define FLit_xeqBuf		"Execute named buffer as a macro with arg n"
#define FLit_xeqFile		"Execute named file (possibly in $execPath) as a macro with arg n"
#define FLit_xeqKeyMacro	"Execute saved keyboard macro n times (default 1, infinitely if n == 0)"
#define FLit_xPathname		"Pathname of file f in $execPath, or nil if not found"
#define FLit_yank		"Insert text from last kill n times (nth older kill if n < 0, without moving point if n == 0)"
#define FLit_yankPop		"Insert next older kill in place of yanked text, retaining point position"

// Hook help text.
#define HLit_chDir		"Command or macro bound to change directory hook"
#define HLit_enterBuf		"Command or macro bound to enter-buffer hook"
#define HLit_exitBuf		"Command or macro bound to exit-buffer hook"
#define HLit_help		"Command or macro bound to help hook"
#define HLit_mode		"Command or macro bound to mode-change hook"
#define HLit_postKey		"Command or macro bound to post-key hook"
#define HLit_preKey		"Command or macro bound to pre-key hook"
#define HLit_read		"Command or macro bound to (post) read-file hook"
#define HLit_wrap		"Command or macro bound to wrap-word hook"
#define HLit_write		"Command or macro bound to (pre) write-file hook"

// "About MightEMacs" help text.
#define ALit_author		"(c) Copyright 2017 Richard W. Marinelli <italian389@yahoo.com>"
#define ALit_footer1		" runs on Unix and Linux platforms and is licensed under the"
#define ALit_footer2		"GNU General Public License (GPLv3), which can be viewed at"
#define ALit_footer3		"http://www.gnu.org/licenses/gpl-3.0.en.html"
#define ALit_buildInfo		"Build Information"
#define ALit_maxCols		"Maximum terminal columns"
#define ALit_maxRows		"Maximum terminal rows"
#define ALit_maxTab		"Maximum hard or soft tab size"
#define ALit_maxPathname	"Maximum length of file pathname"
#define ALit_maxBufName		"Maximum length of buffer name"
#define ALit_maxUserVar		"Maximum length of user variable name"
#define ALit_maxTermInp		"Maximum length of terminal input"
#define ALit_killRingSize	"Number of kill ring buffers"
#define ALit_typeAhead		"Type-ahead support"

// Mode descriptions.
#define MLit_ModeAutoSave	"(G) Automatic file save"
#define MLit_ModeBackup		"(G) Create backup file when saving"
#define MLit_ModeC		"[b] C source code auto-formatting"
#define MLit_ModeClobber	"(G) Allow macros to be overwritten"
#define MLit_ModeColDisp	"[b] Display point column number on mode lines"
#define MLit_ModeEsc8Bit	"(G) Display 8-bit characters escaped"
#define MLit_ModeExact		"(G) Case-sensitive searches by default"
#define MLit_ModeExtraIndent	"[b] Extra indentation before right fence"
#define MLit_ModeHorzScroll	"(G) Horizontally scroll all window lines"
#define MLit_ModeLineDisp	"[b] Display point line number on mode lines"
#define MLit_ModeMEMacs		"[b] MightEMacs script auto-formatting"
#define MLit_ModeMsgDisp	"(G) Enable message-line display"
#define MLit_ModeNoUpdate	"(G) Suppress screen updates"
#define MLit_ModeOver		"[b] Column overwrite when typing"
#define MLit_ModePerl		"[b] Perl script auto-formatting"
#define MLit_ModeReadOnly	"[b] Read-only buffer"
#define MLit_ModeRegexp		"(G) Regular-expression searches by default"
#define MLit_ModeReplace	"[b] Direct character replacement when typing"
#define MLit_ModeRuby		"[b] Ruby script auto-formatting"
#define MLit_ModeSafeSave	"(G) Safe file save"
#define MLit_ModeShell		"[b] Shell script auto-formatting"
#define MLit_ModeWorkDir	"(G) Display working directory on mode lines"
#define MLit_ModeWrap		"[b] Automatic word wrap"

#elif defined(DATAvar)

// **** For var.c ****

// System variable help text.
#define VLit_ArgVector		"Macro arguments"
#define VLit_BufFlagActive	"Active buffer (file was read)"
#define VLit_BufFlagChanged	"Changed buffer"
#define VLit_BufFlagHidden	"Hidden buffer"
#define VLit_BufFlagMacro	"Macro buffer (protected)"
#define VLit_BufFlagNarrowed	"Narrowed buffer"
#define VLit_BufInpDelim	"Line delimiter(s) used for last buffer read"
#define VLit_BufList		"List of visible buffer names"
#define VLit_Date		"Date and time"
#if TypeAhead
#define VLit_KeyPending		"Type-ahead key(s) pending?"
#endif
#define VLit_LineLen		"Current line length in characters"
#define VLit_Match		"Text matching last search or pattern match"
#define VLit_RegionText		"Current region text"
#define VLit_ReturnMsg		"Message returned by last command or script"
#define VLit_RunFile		"Pathname of running script"
#define VLit_RunName		"Name of running macro or buffer"
#define VLit_TermCols		"Terminal screen size in columns"
#define VLit_TermRows		"Terminal screen size in rows (lines)"
#define VLit_WindCount		"Number of windows on current screen"
#define VLit_autoSave		"Keystroke count that triggers auto-save"
#define VLit_bufFile		"Filename associated with current buffer"
#define VLit_bufFlags		"Current buffer flags (bit masks)"
#define VLit_bufLineNum		"Current buffer line number"
#define VLit_bufModes		"Current buffer modes (bit masks)"
#define VLit_bufName		"Current buffer name"
#define VLit_defModes		"Default buffer modes (bit masks)"
#if Color
#define VLit_desktopColor	"Desktop color"
#endif
#define VLit_execPath		"Script search directories"
#define VLit_fencePause		"Centiseconds to pause for fence matching"
#define VLit_globalModes	"Global modes (bit masks)"
#define VLit_hardTabSize	"Number of columns for tab character"
#define VLit_horzJump		"Horizontal window jump size (percentage)"
#define VLit_horzScrollCol	"First displayed column in current window"
#define VLit_inpDelim		"Input line delimiter(s) (or \"auto\")"
#define VLit_keyMacro		"Last keyboard macro entered (in encoded form)"
#define VLit_lastKeySeq		"Last key sequence entered"
#define VLit_lineChar		"Character at point as a string"
#define VLit_lineCol		"Column number of point"
#define VLit_lineOffset		"Number of characters in line before point"
#define VLit_lineText		"Current line text"
#define VLit_maxArrayDepth	"Maximum depth in array recursion (0 to disable)"
#define VLit_maxLoop		"Maximum iterations in loop block (0 to disable)"
#define VLit_maxMacroDepth	"Maximum depth in macro recursion (0 to disable)"
#define VLit_otpDelim		"Output line delimiter(s) (or $BufInpDelim)"
#define VLit_pageOverlap	"Number of lines to overlap when paging"
#if Color
#define VLit_palette		"Custom key-mapping command string"
#endif
#define VLit_randNumSeed	"Random number seed"
#define VLit_replacePat		"Replacement pattern"
#define VLit_screenNum		"Current screen ordinal number"
#define VLit_searchDelim	"Search pattern (input) delimiter character"
#define VLit_searchPat		"Search pattern"
#define VLit_showModes		"Global modes (bit masks) to show on mode line"
#define VLit_softTabSize	"Number of spaces for tab character"
#define VLit_travJump		"Line-traversal jump size (characters)"
#define VLit_vertJump		"Vertical window jump size (percentage)"
#define VLit_windLineNum	"Current window line number"
#define VLit_windNum		"Current window ordinal number"
#define VLit_windSize		"Current window size in lines"
#define VLit_wordChars		"Characters in a word (default \"A-Za-z0-9_\")"
#define VLit_workDir		"Working directory"
#define VLit_wrapCol		"Word wrap column"

#endif
#ifndef DATAmain

// **** For all the other .c files ****

extern char
 *help0,*help1,*help2[],*usage[],
 *text0,*text1,*text2,*text3,*text4,*text5,*text6,*text7,*text8,*text9,*text10,*text11,*text12,*text13,*text14,*text15,*text16,
 *text17,*text18,*text19,*text20,*text21,*text22,*text23,*text24,*text25,*text26,*text27,*text28,*text29,*text30,*text31,
 *text32,*text33,*text34,*text35,*text36,*text37,*text38,*text39,*text40,*text41,*text42,*text43,*text44,*text45,*text46,
 *text47,*text48,*text49,*text50,*text51,*text52,*text53,*text54,*text55,*text56,*text57,*text58,*text59,*text60,*text61,
 *text62,*text63,*text64,*text65,*text66,*text67,*text68,*text69,*text70,*text71,*text72,*text73,*text74,*text75,*text76,
 *text77,*text78,*text79,*text80,*text81,*text82,*text83,*text84,*text85,*text86,*text87,*text88,*text89,*text90,*text91,
 *text92,*text93,*text94,*text95,*text96,*text97,*text98,*text99,*text100,*text101,*text102,*text103,*text104,*text105,*text106,
 *text107,*text108,*text109,*text110,*text111,*text112,*text113,*text114,*text115,*text116,*text117,*text118,*text119,*text120,
 *text121,*text122,*text123,*text124,*text125,*text126,*text127,*text128,*text129,*text130,*text131,*text132,*text133,*text134,
 *text135,*text136,*text137,*text138,*text139,*text140,*text141,*text142,*text143,*text144,*text145,*text146,*text147,*text148,
 *text149,*text150,*text151,*text152,*text153,*text154,*text155,*text156,*text157,*text158,*text159,*text160,*text161,*text162,
 *text163,*text164,*text165,*text166,*text167,*text168,*text169,*text170,*text171,*text172,*text173,*text174,*text175,*text176,
 *text177,*text178,*text179,*text180,*text181,*text182,*text183,*text184,*text185,*text186,*text187,*text188,*text189,*text190,
 *text191,*text192,*text193,*text194,*text195,*text196,*text197,*text198,*text199,*text200,*text201,*text202,*text203,*text204,
 *text205,*text206,*text207,*text208,*text209,*text210,*text211,*text212,*text213,*text214,*text215,*text216,*text217,*text218,
 *text219,*text220,*text221,*text222,*text223,*text224,*text225,*text226,*text227,*text228,*text229,*text230,*text231,*text232,
 *text233,*text234,*text235,*text236,*text237,*text238,*text239,*text240,*text241,*test242,*text243,*text244,*text245,*text246,
 *text247,*text248,*text249,*text250,*text251,*text252,*text253,*text254,*text255,*text256,*text257,*text258,*text259,*text260,
 *text261,*text262,*text263,*text264,*text265,*text266,*text267,*text268,*text269,*text270,*text271,*text272,*text273,*text274,
 *text275,*text276,*text277,*text278,*text279,*text280,*text281,*text282,*text283,*text284,*text285,*text286,*text287,*text288,
 *text289,*text290,*text291,*text292,*text293,*text294,*text295,*text296,*text297,*text298,*text299,*text300,*text301,*text302,
 *text303,*text304,*text305,
#if MMDebug & Debug_ShowRE
 *text306,*text307,*text308,*text309,*text310,*text311,
#endif
 *text312,*text313,*text314,*text315,*text316,*text317,*text318,*text319,*text320,*text321,*text322,*text323,*text324,*text325,
 *text326,*text327,*text328,*text329,*text330,*text331,*text332,*text333,*text334,*text335,*text336,*text337,*text338,*text339,
 *text340,*text341,*text342,*text343,*text344,*text345,*text346,*text347,*text348,*text349,*text350,*text351,*text352,*text353,
 *text354,*text355,*text356,*text357,*text358,*text359,*text360,*text361,*text362,*text363,*text364,*text365,*text366,*text367,
 *text368,*text369,*text370,*text371,*text372,*text373,*text374,*text375,*text376,*text377,*text378,*text379;

#if Color
extern char
 *text500,*text501,*text502,*text503;
#endif

#endif
