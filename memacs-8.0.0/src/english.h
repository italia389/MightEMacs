// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// english.h	English language text strings for MightEMacs.

#define SCRATCH		"scratch"
#define BUFFER1		"untitled"

// General text literals (for initializers).
#define LITERAL1	"[arg,...]"
#define LITERAL2	"name = cm"
#define LITERAL3	"mode,..."
#define LITERAL4	"name"
#define LITERAL5	"name,arg,..."
#define LITERAL6	"[str]"
#define LITERAL7	"key-seq,name"
#define LITERAL8	"name,..."
#define LITERAL9	"[name]"
#define LITERAL10	"arg,..."
#define LITERAL11	"str,repl"
#define LITERAL12	"str"
#define LITERAL13	"val"
#define LITERAL14	"[val[,incr[,fmt]]]"
#define LITERAL15	"f"
#define LITERAL16	"key-seq"
#define LITERAL17	"name[,arg,...]"
#define LITERAL18	"str,from,to"
#define LITERAL19	"str,sub"
#define LITERAL20	"dlm,val1,val2,..."
#define LITERAL21	"n"
#define LITERAL22	"val,len"
#define LITERAL23	"var,dlm"
#define LITERAL24	"p[,def[,dlm[,t]]]"
#define LITERAL25	"var,dlm,val"
#define LITERAL26	"[-]pos,[-]len"
#define LITERAL27	"str,[-]pos,[-]len"
#define LITERAL28	"str,n"
#define LITERAL29	"list,dlm,val"
#define LITERAL30	"chars"
#define LITERAL31	"pref,chars"
#define LITERAL32	"fmt,..."

#ifdef DATAmain

// **** For main.c ****

// Help text.
char
	*help0 = "    Ver. ",
	*help1 = " -- Light and fast Emacs text editor\n\n",
	*usage[] = {
		"Usage:\n    ",
		" [-? | -h | -V] [-D<[!]mode>[,...]] [-d<dir>] [-e<stmt>]\n    ",
		" [-G<[!]mode>[,...]] [{-g | +}<line>[:<pos>]] [-i<input-delim>] [-n] [-R]\n    ",
		" [-r] [-s<string>] [-X<exec-path>] [@<script-file>] [file-spec ...]",
		NULL},
	*help2[] = {
"\n\nSwitches and arguments:\n",
"  -?		   Display usage and exit.\n",
"  -D<[!]modes>	   Specify one or more default buffer modes to clear (!) or set\n",
"		   (no !), separated by commas.  Switch may be repeated.\n",
"  -d<dir>	   Specify a (working) directory to change to.  Switch may be\n",
"		   repeated.\n",
"  -e<stmt>	   Specify an expression statement to be executed.  Switch may\n",
"		   be repeated.\n",
"  -G<[!]modes>	   Specify one or more global modes to clear (!) or set (no !),\n",
"		   separated by commas.  Switch may be repeated.\n",
"  {-g | +}<line>[:<pos>]\n",
"		   Go to specified line number and optional character position\n",
"		   on line in buffer of first file after reading it.  Line\n",
"		   numbers begin at 0 (end of buffer) and character positions\n",
"		   begin at 1.\n",
"  -h		   Display detailed help and exit.\n",
"  -i<input-delim>  Specify zero, one, or two characters (processed as a double\n",
"		   (\") quoted string) to use as file input record delimiter(s).\n",
"		   If none specified (the default), then the first NL, CR-LF, or\n",
"		   CR found when a file is read will be used.\n",
"  -n		   Do not load the site or user startup file.\n",
"  -R		   Not Read-only: open files that follow with Read mode OFF.\n",
"		   Switch may be repeated.\n",
"  -r		   Read-only: open files that follow with Read mode ON.  Switch\n",
"		   may be repeated.\n",
"  -s<string>	   Search for specified string in buffer of first file after\n",
"		   reading it.\n",
"  -V		   Display program version and exit.\n",
"  -X<exec-path>    Specify colon-separated list of script search directories to\n",
"		   prepend to existing path.\n",
"  @<script-file>   Execute specified script file (in path) before processing\n",
"		   argument(s) that follow it.  May appear anywhere on command\n",
"		   line and may be repeated.\n",
"  file-spec	   Zero or more files to open for viewing or editing.  If \"-\" is\n",
"		   specified in place of a filename, data is read from standard\n",
"		   input into buffer \"",
BUFFER1,
"\".\n\n",
"Notes:\n",
" 1. Arguments are processed in the order given before any files are opened or\n",
"    the first file is read, unless noted otherwise.\n",
" 2. Switches -g and + are mutually exclusive with -s.\n",
" 3. The script execution path is initialized to\n",
"    \"", MMPATH_DEFAULT, "\" or the MMPATH environmental\n",
"    variable if it is defined.  It may subsequently be changed by the -X switch,\n",
"    -e switch, or a @<script-file> invocation.",
		NULL};

// General text messages.
char
 *text0 = "Error",
 *text1 = "Unknown switch, %s",
 *text2 = "Invalid character range '%.3s' in tr string",
 *text3 = "%s(): Unknown id %d for var '%s'!",
 *text4 = "%s expected (at token '%s')",
 *text5 = "Group number %ld must be between 0 and %d",
 *text6 = "About",
 *text7 = "Go to line",
 *text8 = "Aborted!",
 *text9 = "Mark %d set",
 *text10 = "Mark %d cleared",
 *text11 = "No mark %d in this window",
 *text12 = "%s '%d' must be between %d and %d",
 *text13 = "Show key ",
 *text14 = "%s not bound",
 *text15 = "Bind key ",
 *text16 = "%s(): No such entry '%s' to delete!",
 *text17 = "Cannot bind a key sequence '%s' to '%s' command",
 *text18 = "Unbind key ",
 *text19 = "No such kill %d (max %d)",
 *text20 = "Apropos",
 *text21 = "BindingList",
 *text22 = "Extraneous token '%s'",
 *text23 = "Missing '=' in %s command (at token '%s')",
 *text24 = "Use",
 *text25 = "That name is already in use.  New name",
 *text26 = "Delete",
 *text27 = "Pop",
 *text28 = "%s is being displayed",
 *text29 = "Change buffer name to",
 *text30 = "ACHMPTN    Modes        Size       Buffer               File",
 *text31 = "global",
 *text32 = "Discard changes",
 *text33 = "Cannot get %s of file '%s': %s",
 *text34 = "File: ",
 *text35 = "Line count cannot be 1",
 *text36 = "Unknown RE option '%c'",
 *text37 = "pathname",
 *text38 = "Invalid number '%s'",
 *text39 = "%s (%d) must be %d or greater",
 *text40 = "Script file '%s' not found%s",
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
#if COLOR
 *text54 = "$palette value '%s' is invalid",
#endif
 *text55 = "Insert",
 *text56 = "VariableList",
 *text57 = "Argument expected",
 *text58 = "Buffer",
 *text59 = "Wrap column",
 *text60 = "Line %lu/%lu, Col %d/%d, Char %lu/%lu (%.1f%%), char = %s",
 *text61 = ", switch '%s'",
 *text62 = "default",
 *text63 = " mode",
 *text64 = "Set",
 *text65 = "Clear",
 *text66 = "No such mode '%s'",
 *text67 = "Function call",
 *text68 = "Identifier",
 *text69 = "Wrong number of arguments (at token '%s')",
 *text70 = "Region copied",
 *text71 = "%s '%s' is already narrowed",
 *text72 = "Cannot narrow whole buffer",
 *text73 = "%s narrowed",
 *text74 = "%s '%s' is not narrowed",
 *text75 = "%s widened",
 *text76 = "Mark (%d) must be between %d and %d",
 *text77 = "%s() bug: Lost mark 0!",
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
 *text88 = "Cannot read directory '%s': %s",
 *text89 = "Screen Window      Buffer                File",
 *text90 = "(<SP>,y) Yes (n) No (!) Do rest (u) Undo last (<ESC>,q) Stop here (.) Stop and go back (?) Help",
 *text91 = "Null string replaced, stopping",
 *text92 = "%d substitution%s",
 *text93 = "Current window too small to shrink by %d line%s",
 *text94 = "%s(): Out of memory!",
 *text95 = "Narrowed buffer ... delete visible portion",
 *text96 = "No characters in character class",
 *text97 = "Character class not ended",
 *text98 = "Wrap column not set",
 *text99 = "file",
#if WORDCOUNT
 *text100 = "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f",
#endif
 *text101 = "Cannot search and goto at the same time!",
 *text102 = "Beginning value",
 *text103 = "Saving %s ...",
 *text104 = "Modified buffers exist.  Leave anyway",
 *text105 = "Macro already active, cancelled",
 *text106 = "Begin keyboard macro",
 *text107 = "Keyboard macro not active",
 *text108 = "End keyboard macro",
 *text109 = "%s is in read-only mode",
 *text110 = "Prompt string required for",
 *text111 = "'%s' value must be %d or greater",
 *text112 = "Maximum number of loop iterations (%d) exceeded!",
 *text113 = "Cannot create macro '%s'",
 *text114 = "Prior loop execution level not found while rewinding stack",
 *text115 = "Word",
 *text116 = "No such macro '%s'",
 *text117 = "Execute",
 *text118 = "No such buffer '%s'",
 *text119 = "Pause duration",
 *text120 = "!break or !next outside of any !while, !until, or !loop block",
 *text121 = "!endloop with no matching !while, !until, or !loop",
 *text122 = "!while, !until, or !loop with no matching !endloop",
 *text123 = "Unterminated string %s",
 *text124 = "Cannot insert current buffer into itself",
 *text125 = "CompletionList",
 *text126 = "Script loop boundary line not found",
 *text127 = "alias",
 *text128 = "Invalid buffer name '%s'",
 *text129 = "Execute macro file",
 *text130 = "No such command or macro '%s'",
 *text131 = "Read file",
 *text132 = "Insert file",
 *text133 = "Find file",
 *text134 = "View file",
 *text135 = "Old buffer",
 *text136 = " in path '%s'",
 *text137 = "Command repeat count",
 *text138 = "New file",
 *text139 = "Reading data ...",
 *text140 = "Read",
 *text141 = "I/O ERROR: %s, file '%s'",
 *text142 = "Cannot change a character past end of buffer '%s'",
 *text143 = "Line number",
 *text144 = "Write file",
 *text145 = "No filename associated with buffer '%s'",
 *text146 = "Truncated file in buffer '%s' ... write it out",
 *text147 = "Narrowed buffer '%s' ... write it out",
 *text148 = "Writing data ...",
 *text149 = "Wrote",
 *text150 = "(file saved as '%s') ",
 *text151 = "Change filename to",
 *text152 = "No such file '%s'",
 *text153 = "Inserting data ...",
 *text154 = "Inserted",
 *text155 = "%s(): Out of memory saving filename '%s'!",
 *text156 = "%s(): Out of memory allocating %u-byte line buffer for file '%s'!",
 *text157 = "%s(): key map space (%d entries) exhausted!",
 *text158 = "command",
 *text159 = "Buffers",
 *text160 = "Screens",
 *text161 = " (disabled '%s' hook)",
 *text162 = " (y/n)?",
 *text163 = "status",
 *text164 = "Cannot modify read-only variable '%s'",
 *text165 = "Name '%s' already in use",
 *text166 = "Invalid integer '%s'",
 *text167 = "%d buffer%s saved",
 *text168 = "if/loop nesting level (%d) too deep",
 *text169 = "Clear",
 *text170 = "Read pipe",
 *text171 = "Invalid string (%ld)",
 *text172 = "Token expected",
 *text173 = "Unbalanced %c%c%c string parameter",
 *text174 = "Created screen %hu",
 *text175 = "Command '%s' failed",
 *text176 = "Macro '%s' failed",
 *text177 = "%s(): Screen number %d not found in screen list!",
 *text178 = "Deleted screen %d",
 *text179 = "%s(): Unknown id %d for variable '%s'!",
 *text180 = "%s(): Unknown type %.8x for variable!",
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
 *text196 = "!macro with no matching !endmacro",
 *text197 = "!endmacro with no matching !macro",
 *text198 = "Misplaced %s directive",
 *text199 = "!if with no matching !endif",
 *text200 = "No keyboard macro defined",
 *text201 = "End: ",
 *text202 = "(<SP>,f",
 *text203 = ") +page (b",
 *text204 = ") -page (d) +half (u) -half",
 *text205 = "line",
 *text206 = " (g) first (G) last (<ESC>,q) quit (?) help: ",
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
 *text217 = "!break level '%ld' must be 1 or greater",
 *text218 = "Append file",
 *text219 = "Script failed",
 *text220 = "Parent loop block of loop directive at line %ld not found during buffer scan",
 *text221 = "Too many groups in RE pattern '%s' (maximum is %d)",
 *text222 = "RE group not ended",
 *text223 = "Size argument",
 *text224 = "Line offset value %ld out of range",
 *text225 = "Too many break levels (%d short) from inner !break",
 *text226 = "Cannot %s %s buffer '%s'",
 *text227 = "Terminal dimensions set to %hu x %hu",
 *text228 = "Kill ring cleared",
 *text229 = ", in",
 *text230 = "at line",
 *text231 = "Toggle",
 *text232 = "Macro name '%s' cannot exceed %d characters",
 *text233 = "Mark %d set to previous position",
 *text234 = "Increment",
 *text235 = "Format string",
 *text236 = "i increment cannot be zero!",
 *text237 = "Invalid i format '%s' (must contain exactly one %%d, %%o, %%u, %%x, or %%X)",
 *text238 = "executing",
 *text239 = "No such window '%d'",
 *text240 = "No such screen '%d'",
 *text241 = "Screen is being displayed",
#if COLOR
 *text242 = "No such color '%s'",
#endif
 *text243 = "Delete screen",
 *text244 = "No such command, alias, or macro '%s'",
 *text245 = "Division by zero is undefined (%ld/0)",
 *text246 = "Help hook not set",
 *text247 = "function",
 *text248 = "an executing",
 *text249 = "Insert pipe",
 *text250 = "Unchange",
 *text251 = "%s delimiter '%s' cannot be more than %d character(s)",
 *text252 = ", delimited by ",
 *text253 = "(return status %d)\n",
 *text254 = "Invalid key literal '%s'",
 *text255 = "%s overflow copying string '%s'",
 *text256 = "%s tab size %ld must be between 2 and %d",
 *text257 = ", original file renamed to '",
#if NULREGERR
 *text258 = "Null region",
#endif
 *text259 = "No text selected",
 *text260 = "Line",
 *text261 = "Text",
 *text262 = "copied",
 *text263 = "delete",
 *text264 = "clear",
 *text265 = "Cannot change to directory '%s': %s",
 *text266 = "Maximum number of keyboard macro entries (%d) exceeded!",
 *text267 = "command or macro",
 *text268 = "Non-macro buffer name '%s' cannot begin with %c",
 *text269 = "Delete alias",
 *text270 = "Macro buffer name '%s' must begin with %c",
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
 *text281 = "%s cannot exceed %d characters",
 *text282 = "'%s' command not allowed in a macro (use \"run\")",
 *text283 = "Pattern",
 *text284 = "Cannot %s %s buffer",
 *text285 = "Command argument",
 *text286 = "Invalid identifier '%s'",
 *text287 = "i variable set to %d",
 *text288 = "Function",
 *text289 = "Unexpected token '%s'",
 *text290 = "Length argument",
 *text291 = "'%s' expected (at token '%s')",
 *text292 = "variable",
 *text293 = "Cannot split a %d-line window",
 *text294 = "Only one window",
 *text295 = "prompt function type '%s' must be b, c, f, or s",
 *text296 = "show",
 *text297 = "modes",
 *text298 = "Unknown or conflicting bit(s) in mode word '0x%.8x'",
 *text299 = "Pop file",
 *text300 = "False return",
 *text301 = "Expression",
 *text302 = "Group reference %d exceeds maximum (%d) in replacement pattern '%s'",
 *text303 = "Invalid wrap prefix \"%s\"",
 *text304 = "Closure on group not supported in RE pattern '%s'",
 *text305 = "KillList",
#if MMDEBUG & DEBUG_SHOWRE
 *text306 = "RegExpList",
 *text307 = "FORWARD",
 *text308 = "BACKWARD",
 *text309 = "SEARCH",
 *text310 = "REPLACEMENT",
 *text311 = "PATTERN",
#endif

 *text320 = "More than one argument specified for '%' expression",
 *text321 = "Unknown format spec '%%%c'",
 *text322 = "Column number",
 *text323 = "Indentation exceeds wrap column (%d)",
 *text324 = ".  New name",
 *text325 = "for",
 *text326 = "Begin",
 *text327 = "Macro bound to '%s' hook",
 *text328 = "tr \"from\" string",
 *text329 = "include? delimiter",
 *text330 = "Kill  Text",
 *text331 = "window",
 *text332 = "Soft tab size set to %d",
 *text333 = "Illegal value for '$%s' variable",
 *text334 = ", setting variable '%c%s'",
 *text335 = "directory",
 *text336 = "macro",
 *text337 = "Invalid \\nn sequence in string %s",
 *text338 = "Cannot access '$keyMacro' from a keyboard macro, cancelled",
 *text339 = "to";

// Function help text.
#define FLIT_abort		"Return \"abort\" status and optional message"
#define FLIT_about		"Pop-up \"about the editor\" information (with \"selectBuf\" options)"
#define FLIT_abs		"Absolute value"
#define FLIT_alias		"Create alias name for command or macro cm"
#define FLIT_alterBufMode	"Clear (n < 0), toggle (n == 0, default), or set (n > 0) current buffer mode(s)"
#define FLIT_alterDefMode	"Clear (n < 0), toggle (n == 0, default), or set (n > 0) default buffer mode(s)"
#define FLIT_alterGlobalMode	"Clear (n < 0), toggle (n == 0, default), or set (n > 0) global mode(s)"
#define FLIT_alterShowMode	"Clear (n < 0), toggle (n == 0, default), or set (n > 0) global mode(s) to show on mode line"
#define FLIT_appendFile		"Append current buffer to named file"
#define FLIT_backChar		"Move cursor backward [-]n characters (default 1)"
#define FLIT_backLine		"Move cursor backward [-]n lines (default 1)"
#define FLIT_backPage		"Scroll current window backward [-]n pages (default 1)"
#define FLIT_backPageNext	"Scroll next window backward [-]n pages (default 1)"
#define FLIT_backPagePrev	"Scroll previous window backward [-]n pages (default 1)"
#define FLIT_backTab		"Move cursor backward [-]n tab stops (default 1)"
#define FLIT_backWord		"Move cursor backward [-]n words (default 1)"
#define FLIT_basename		"Filename component of pathname f (without extension if n <= 0)"
#define FLIT_beep		"Sound beeper n times (default 1)"
#define FLIT_beginBuf		"Move cursor to beginning of buffer (or named buffer if n arg)"
#define FLIT_beginKeyMacro		"Begin recording keyboard macro"
#define FLIT_beginLine		"Move cursor to beginning of [-]nth line"
#define FLIT_beginText		"Move cursor to beginning of [-]nth line text"
#define FLIT_beginWhite		"Move cursor to beginning of white space surrounding cursor on current line"
#define FLIT_binding		"Name of command or macro bound to given key, or nil if none"
#define FLIT_bindKey		"Bind named command or macro to a key sequence (or single key if n <= 0)"
#define FLIT_bufBoundQ		"Cursor at beginning, middle, end, or either end of buffer for -, 0, +, or default n?"
#define FLIT_bufWind		"Window on current screen displaying named buffer, or nil if none"
#define FLIT_chDir		"Change working directory"
#define FLIT_chr		"Integer-to-ASCII character conversion"
#define FLIT_clearBuf		"Clear current buffer (or named buffer if n arg, force if n > 0)"
#define FLIT_clearKillRing	"Clear all entries in kill ring"
#define FLIT_clearMark		"Clear mark n (default 0)"
#define FLIT_clearMsg		"Clear the message line (force if n > 0)"
#define FLIT_copyFencedText	"Copy text from cursor to matching ( ), [ ], or { } to kill buffer"
#define FLIT_copyLine		"Copy [-]n lines to kill buffer (default 1, region lines if n == 0)"
#define FLIT_copyRegion		"Copy text in current region to kill buffer"
#define FLIT_copyToBreak	"Copy from cursor through [-]n lines to kill buffer (default 1)"
#define FLIT_copyWord		"Copy [-]n words to kill buffer (default 1, without trailing white space if n == 0)"
#if WORDCOUNT
#define FLIT_countWords		"Show counts in current region"
#endif
#define FLIT_cycleKillRing	"Cycle kill ring [-]n times (default 1)"
#define FLIT_definedQ	"Name or mark defined? (returns \"alias|buffer|command|function|macro|variable|nil|true|false\")"
#define FLIT_deleteAlias	"Delete named alias(es)"
#define FLIT_deleteBackChar	"Delete backward [-]n characters (default 1)"
#define FLIT_deleteBlankLines	"Delete blank lines around cursor (or below it if current line not blank)"
#define FLIT_deleteBuf		"Delete named buffer(s) (and ignore changes if n > 0)"
#define FLIT_deleteFencedText	"Delete text from cursor to matching ( ), [ ], or { }"
#define FLIT_deleteForwChar	"Delete forward [-]n characters (default 1)"
#define FLIT_deleteLine		"Delete [-]n lines (default 1, region lines if n == 0)"
#define FLIT_deleteMacro	"Delete named macro(s)"
#define FLIT_deleteRegion	"Delete text in current region"
#define FLIT_deleteScreen	"Delete screen n"
#define FLIT_deleteTab		"Delete [-]n hard tabs or \"chunks\" of space characters (default -1)"
#define FLIT_deleteToBreak	"Delete from cursor through [-]n lines (default 1)"
#define FLIT_deleteWhite	"Delete white space surrounding cursor on current line"
#define FLIT_deleteWind		"Delete current window and give space to upper window (or lower window if n > 0)"
#define FLIT_deleteWord		"Delete [-]n words (default 1, without trailing white space if n == 0)"
#define FLIT_detabLine		"Change tabs to spaces in [-]n lines (default 1, region lines if n == 0)"
#define FLIT_dirname		"Directory component of pathname f"
#define FLIT_endBuf		"Move cursor to end of buffer (or named buffer if n arg)"
#define FLIT_endKeyMacro		"End recording keyboard macro"
#define FLIT_endLine		"Move cursor to end of [-]nth line"
#define FLIT_endWhite		"Move cursor to end of white space surrounding cursor on current line"
#define FLIT_endWord		"Move cursor to end point of [-]nth word (default 1)"
#define FLIT_entabLine		"Change spaces to tabs in [-]n lines (default 1, region lines if n == 0)"
#define FLIT_env		"Value of given environmental variable"
#define FLIT_eval		"Concatenate arguments (or get string if interactive, expression if n arg) and execute result"
#define FLIT_exit		"Exit editor with optional message (and ignore changes if n != 0, force -1 return if n < 0)"
#define FLIT_fileExistsQ	"File f exists? (returns nil, \"file\", or \"directory\")"
#define FLIT_findFile		"Find named file and read (if n != 0) into new buffer (with \"selectBuf\" options)"
#define FLIT_forwChar		"Move cursor forward [-]n characters (default 1)"
#define FLIT_forwLine		"Move cursor forward [-]n lines (default 1)"
#define FLIT_forwPage		"Scroll current window forward [-]n pages (default 1)"
#define FLIT_forwPageNext	"Scroll next window forward [-]n pages (default 1)"
#define FLIT_forwPagePrev	"Scroll previous window forward [-]n pages (default 1)"
#define FLIT_forwTab		"Move cursor forward [-]n tab stops (default 1)"
#define FLIT_forwWord		"Move cursor forward [-]n words (default 1)"
#define FLIT_getKey		"Get one keystroke (or key sequence if n > 0)"
#define FLIT_gotoFence		"Move cursor to matching ( ), [ ], { }, or < >"
#define FLIT_gotoLine		"Move cursor to beginning of line n (or to end of buffer if n == 0)"
#define FLIT_gotoMark		"Move cursor to mark n (default 0)"
#define FLIT_growWind		"Increase size of current window by n lines (default 1 at bottom, at top if n < 0)"
#define FLIT_help		"Run macro assigned to \"help\" hook"
#define FLIT_hideBuf		"Set the \"hidden\" flag of the current buffer (or named buffer if n arg)"
#define FLIT_huntBack		"Repeat search backward [-]n times (default 1)"
#define FLIT_huntForw		"Repeat search forward [-]n times (default 1)"
#define FLIT_includeQ		"Value in list?"
#define FLIT_indentRegion	"Indent lines in region by n tab stops (default 1)"
#define FLIT_index	"Position of sub in str, or nil if not found (first position is 0, rightmost occurrence if n > 0)"
#define FLIT_insert		"Insert concatenated arguments at cursor n times (without moving cursor if n == 0)"
#define FLIT_insertBuf		"Insert named buffer before current line (without moving cursor if n == 0) and mark as region"
#define FLIT_insertFile		"Insert named file before current line (without moving cursor if n == 0) and mark as region"
#define FLIT_inserti		"Format, insert at cursor, and add increment to i variable n times"
#define FLIT_insertLineI	"Insert and indent [-]n lines (default -1)"
#define FLIT_insertPipe		"Execute shell command and insert result before current line (without moving cursor if n == 0)"
#define FLIT_insertSpace	"Insert n spaces ahead of cursor"
#define FLIT_intQ		"Integer value?"
#define FLIT_join		"Join all arguments after first using first as delimiter (ignoring null and nil args if n <= 0)"
#define FLIT_joinLines		"Join [-]n lines, replacing white space with single space (default -1, region lines if n == 0)"
#define FLIT_joinWind		"Join current window with lower window (or upper window if n < 0)"
#define FLIT_killFencedText	"Delete/save text from cursor to matching ( ), [ ], or { } to kill buffer"
#define FLIT_killLine		"Delete/save [-]n lines to kill buffer (default 1, region lines if n == 0)"
#define FLIT_killRegion		"Delete/save text in current region to kill buffer"
#define FLIT_killToBreak	"Delete/save from cursor through [-]n lines to kill buffer (default 1)"
#define FLIT_killWord		"Delete/save [-]n words to kill buffer (default 1, without trailing white space if n == 0)"
#define FLIT_lcLine		"Change [-]n lines to lower case (default 1, region lines if n == 0)"
#define FLIT_lcRegion		"Change all letters in current region to lower case"
#define FLIT_lcString		"Lower-cased string"
#define FLIT_lcWord		"Change n words to lower case (default 1)"
#define FLIT_length		"String length"
#define FLIT_let		"Assign a value (or an expression if n arg) to a system or global variable"
#define FLIT_markBuf		"Save cursor position as mark n (default 2) and mark whole buffer as current region"
#define FLIT_match		"Text matching group in search pattern (0 for entire match and 1,... if Regexp mode)"
#define FLIT_moveWindDown	"Move window frame [-]n lines down (default 1)"
#define FLIT_moveWindUp		"Move window frame [-]n lines up (default 1)"
#define FLIT_narrowBuf		"Keep [-]n lines (default 1, region lines if n == 0) and hide and preserve remainder"
#define FLIT_newline		"Insert n lines (default 1, with auto-formatting if language or wrap mode active)"
#define FLIT_newlineI		"Insert n lines with same indentation as previous line (default 1)"
#define FLIT_newScreen		"Create new screen and switch to it"
#define FLIT_nextArg		"Next macro argument"
#define FLIT_nextBuf		"Switch to next buffer n times (or return name of next buffer only if n <= 0)"
#define FLIT_nextScreen		"Switch to [-]nth next screen (numerically, [-]nth from bottom or top if n arg)"
#define FLIT_nextWind		"Switch to next window ([-]nth from bottom or top if n arg)"
#define FLIT_nilQ		"Nil value?"
#define FLIT_notice		"Return concatenated message (not in brackets if n <= 0) and true value (or false value if n < 0)"
#define FLIT_nullQ		"Null value?"
#define FLIT_numericQ		"Numeric string?"
#define FLIT_onlyWind		"Make current window the only window on screen"
#define FLIT_openLine		"Insert n lines ahead of cursor (default 1)"
#define FLIT_ord		"ASCII-to-integer character conversion"
#define FLIT_outdentRegion	"Outdent lines in region by n tab stops (default 1)"
#define FLIT_overwrite		"Overwrite text at cursor n times with concatenated arguments (without moving cursor if n == 0)"
#define FLIT_pad		"Right (len < 0) or left (len > 0) justified value as string, space-padded"
#define FLIT_pathname		"Absolute pathname of file f"
#define FLIT_pause		"Pause execution for n centiseconds (default 100)"
#define FLIT_pipeBuf		"Feed current buffer to shell command and replace with result"
#define FLIT_pop		"Remove last token from variable var using delimiter dlm (nil for white space) and return it"
#define FLIT_prevBuf		"Switch to previous buffer n times (or return name of previous buffer only if n <= 0)"
#define FLIT_prevScreen		"Switch to [-]nth previous screen (numerically, [-]nth from bottom or top if n arg)"
#define FLIT_prevWind		"Switch to previous window (or [-]nth from bottom or top if n arg)"
#define FLIT_print	"Write concatenated arguments to message line with invisible characters exposed (force if n > 0)"
#define FLIT_prompt	"User input with prompt p, default value def, delimiter dlm, type t ([b]uf,[c]har,[f|F]ile,[s]tring)"
#define FLIT_push		"Append value to variable var using delimiter dlm and return it"
#define FLIT_queryReplace	"Replace zero or more occurrences of one string with another interactively"
#define FLIT_quickExit		"Exit editor, saving any modified buffers"
#define FLIT_quote		"String enclosed in \" quotes with \\, \", and non-printable characters prefixed with \\"
#define FLIT_quoteChar		"Insert a character read from the keyboard literally n times (default 1)"
#define FLIT_rand		"Random integer (> 0)"
#define FLIT_readBuf		"Return next line of named buffer (beginning at cursor position), or nil if none left"
#define FLIT_readFile		"Read named file into current buffer (or new buffer if n arg, with \"selectBuf\" options)"
#define FLIT_readPipe		"Execute shell command and save result in current buffer (with \"readFile\" options)"
#define FLIT_redrawScreen	"Redraw screen (with cursor at: current, +nth, -nth, center window line if 0, +, -, default n)"
#define FLIT_replace		"Replace n occurrences of one string with another (default all)"
#define FLIT_replaceText	"Replace text at cursor n times with concatenated arguments (without moving cursor if n == 0)"
#define FLIT_resetTerm		"Update all screens and windows to match current terminal dimensions (force if n > 0)"
#define FLIT_resizeWind		"Change size of current window to n lines"
#define FLIT_restoreBuf		"Make current buffer remembered buffer"
#define FLIT_restoreWind	"Make current window remembered window"
#define FLIT_reverse		"Reversed string"
#define FLIT_run		"Execute named command, alias, or macro interactively with arg n"
#define FLIT_saveBuf		"Remember current buffer"
#define FLIT_saveFile		"Save current buffer (if changed) to its associated file (or all changed buffers if n > 0)"
#define FLIT_saveWind		"Remember current window"
#define FLIT_scratchBuf		"Create and switch to a buffer with unique name (with \"selectBuf\" options)"
#define FLIT_searchBack		"Search backward for the [-]nth occurrence of a string (default 1)"
#define FLIT_searchForw		"Search forward for the [-]nth occurrence of a string (default 1)"
#define FLIT_selectBuf	"(Create|switch to) named buffer (and alt-pop|pop|stay|switch|\"windowize\" if <-1|-1|0|1|>1 n)"
#define FLIT_setBufFile		"Associate current buffer with named file"
#define FLIT_setBufName		"Set name of current buffer (or derive from associated filename if n > 0)"
#define FLIT_seti		"Set i variable to a value (or n arg), increment, and printf format string"
#define FLIT_setMark		"Set mark n to cursor position (default 0)"
#define FLIT_setWrapCol		"Set wrap column to n (default 0)"
#define FLIT_shell		"Start a shell session"
#define FLIT_shellCmd		"Execute a shell command and (if interactive) pause to display result"
#define FLIT_shift		"Remove next token from variable var using delimiter dlm (nil for white space) and return it"
#define FLIT_showBindings	"Pop-up all commands and bindings (containing str if n arg, with \"selectBuf\" options)"
#define FLIT_showBuffers	"Pop-up visible buffers (all if n arg, with \"selectBuf\" options)"
#define FLIT_showFunctions	"Pop-up system functions (containing str if n arg, with \"selectBuf\" options)"
#define FLIT_showKey		"Display name of command or macro bound to key sequence (or single key if n <= 0)"
#define FLIT_showKillRing	"Pop-up kill ring entries (with \"selectBuf\" options)"
#if MMDEBUG & DEBUG_SHOWRE
#define FLIT_showRegExp		"Pop-up search and replacement metacharacter arrays (with \"selectBuf\" options)"
#endif
#define FLIT_showScreens	"Pop-up screens (with \"selectBuf\" options)"
#define FLIT_showVariables	"Pop-up all variables (containing str if n arg, with \"selectBuf\" options)"
#define FLIT_shQuote		"String enclosed in ' quotes with ' characters converted to \\' (for literal shell argument)"
#define FLIT_shrinkWind		"Decrease size of current window by n lines (default 1 at bottom, at top if n < 0)"
#define FLIT_space		"Insert n spaces (default 1, with auto-wrap if wrap mode active)"
#define FLIT_splitWind		"Split current window (and shrink upper, go to non-default, set upper size if -, 0, + n)"
#define FLIT_sprintf		"String built from specified format string and arguments"
#define FLIT_stringQ		"String value?"
#define FLIT_stringFit		"Condensed string with embedded ellipsis (max size n)"
#define FLIT_strip		"Whitespace-stripped string (or left end only, right end only if -, + n)"
#define FLIT_stringLit		"Literal (exposed) string"
#define FLIT_sub		"Replace first occurrence of one string in another (or all occurrences if n > 0)"
#define FLIT_subLine		"Extract substring from current line at cursor (first pos is 0)"
#define FLIT_subString		"Extract substring from a string (first pos is 0)"
#define FLIT_suspend		"Suspend editor (put in background) and start a shell session"
#define FLIT_swapMark		"Swap cursor position and mark n (default 0)"
#define FLIT_tab		"Insert tab character or spaces n times (or set soft tab size to abs(n) if n <= 0)"
#define FLIT_tcString		"Title-cased string"
#define FLIT_tcWord		"Change n words to title case (default 1)"
#define FLIT_toInt		"Integer converted from string"
#define FLIT_toString		"String converted from integer"
#define FLIT_tr			"Translated string (\"to\" string may be nil or null, or shorter than \"from\" string)"
#define FLIT_traverseLine	"Move cursor forward or backward $travJumpSize characters (or to far right if n == 0)"
#define FLIT_trimLine		"Trim white space from end of [-]n lines (default 1, region lines if n == 0)"
#define FLIT_truncBuf		"Delete all text from cursor to end of buffer"
#define FLIT_ucLine		"Change [-]n lines to upper case (default 1, region lines if n == 0)"
#define FLIT_ucRegion		"Change all letters in current region to upper case"
#define FLIT_ucString		"Upper-cased string"
#define FLIT_ucWord		"Change n words to upper case (default 1)"
#define FLIT_unbindKey		"Unbind a key sequence (or single key if n <= 0; ignore any error if script mode and n > 0)"
#define FLIT_unchangeBuf	"Clear the \"changed\" flag of the current buffer (or named buffer if n arg)"
#define FLIT_unhideBuf		"Clear the \"hidden\" flag of the current buffer (or named buffer if n arg)"
#define FLIT_unshift		"Prepend value to variable var using delimiter dlm and return it"
#define FLIT_updateScreen	"Redraw screen (force if n > 0)"
#define FLIT_viewFile		"Find named file and read (if n != 0) into new read-only buffer (with \"selectBuf\" options)"
#define FLIT_whence		"Display buffer position and the ordinal value of the current character"
#define FLIT_widenBuf		"Expose all hidden lines"
#define FLIT_wrapLine		"Rewrap [-]n lines at $wrapCol (default 1, region lines if n == 0)"
#define FLIT_wrapWord		"Move word at cursor (if past column n) or following it (otherwise) to beginning of next line"
#define FLIT_writeBuf		"Write concatenated arguments to named buffer n times (without moving point if n == 0)"
#define FLIT_writeFile		"Write current buffer to named file"
#define FLIT_xeqBuf		"Execute named buffer as a macro with arg n"
#define FLIT_xeqFile		"Execute named file (possibly in $execPath) as a macro with arg n"
#define FLIT_xeqKeyMacro	"Execute saved keyboard macro n times (default 1, infinity if n == 0)"
#define FLIT_xPathname		"Pathname of file f in $execPath, or nil if not found"
#define FLIT_yank		"Insert text from last kill n times (nth older kill if n < 0, without moving cursor if n == 0)"
#define FLIT_yankPop		"Insert next older kill in place of yanked text, retaining cursor position"

// "About MightEMacs" help text.
#define ALIT_author		"(c) Copyright 2015 Richard W. Marinelli <italian389@yahoo.com>"
#define ALIT_footer1		" runs on Unix and Linux platforms and is licensed under the"
#define ALIT_footer2		"Creative Commons Attribution-NonCommercial 4.0 International License which"
#define ALIT_footer3		"can be viewed at http://creativecommons.org/licenses/by-nc/4.0/legalcode]"
#define ALIT_buildInfo		"Build Information"
#define ALIT_maxCols		"Maximum terminal columns"
#define ALIT_maxRows		"Maximum terminal rows"
#define ALIT_maxIfNest		"Maximum script if/loop nesting level"
#define ALIT_maxTab		"Maximum hard or soft tab size"
#define ALIT_maxPathname	"Maximum length of file pathname"
#define ALIT_maxBufName		"Maximum length of buffer name"
#define ALIT_maxUserVar		"Maximum length of user variable name"
#define ALIT_maxTermInp		"Maximum length of terminal input"
#define ALIT_maxPat		"Maximum length of RegExp pattern"
#define ALIT_maxKMacro		"Maximum number of keyboard macro keys"
#define ALIT_maxREGroups	"Maximum number of RegExp groups"
#define ALIT_killRingSize	"Number of kill ring buffers"
#define ALIT_maxMarks		"Number of marks per window"
#define ALIT_typeAhead		"Type-ahead support"

#else

// **** For all the other .c files ****

// System variable help text.
#define VLIT_ArgCount		"Number of arguments in macro call"
#define VLIT_BufCount		"Number of visible buffers"
#define VLIT_BufFlagActive	"Active buffer (file was read)"
#define VLIT_BufFlagChanged	"Changed buffer"
#define VLIT_BufFlagHidden	"Hidden buffer"
#define VLIT_BufFlagMacro	"Macro buffer (protected)"
#define VLIT_BufFlagNarrowed	"Narrowed buffer"
#define VLIT_BufFlagPreprocd	"Preprocessed (macro) buffer"
#define VLIT_BufFlagSystem	"System buffer (protected)"
#define VLIT_BufFlagTruncated	"Truncated buffer (partial file read)"
#define VLIT_BufInpDelim	"Line delimiter(s) used for last buffer read"
#define VLIT_BufList		"Tab delimited list of visible buffer names"
#define VLIT_BufOtpDelim	"Line delimiter(s) used for last buffer write"
#define VLIT_BufSize		"Current buffer size in bytes"
#define VLIT_Date		"Date and time"
#define VLIT_EditorName		"Text editor name"
#define VLIT_EditorVersion	"Text editor version"
#if TYPEAHEAD
#define VLIT_KeyPending		"Type-ahead key(s) pending?"
#endif
#define VLIT_KillText		"Most recent kill text"
#define VLIT_Language		"Language of text messages"
#define VLIT_LineLen		"Length of current line"
#define VLIT_Match		"Text matching last search"
#define VLIT_ModeAutoSave	"(Asave) Automatic file save"
#define VLIT_ModeBackup		"(Bak) Create backup file when saving"
#define VLIT_ModeC		"[C] C source code auto-formatting"
#define VLIT_ModeClobber	"(Clob) Allow macros to be overwritten"
#define VLIT_ModeColDisp	"[cOl] Display cursor column number on mode lines"
#define VLIT_ModeEsc8Bit	"(esc8) Display 8-bit characters escaped"
#define VLIT_ModeExact		"(Exact) Case-sensitive searches"
#define VLIT_ModeExtraIndent	"[Xindt] Extra indentation before right fence"
#define VLIT_ModeHorzScroll	"(Hscrl) Horizontally scroll all window lines"
#define VLIT_ModeKeyEcho	"(Kecho) Echo input characters at user prompt"
#define VLIT_ModeLineDisp	"[Line] Display cursor line number on mode lines"
#define VLIT_ModeMEMacs		"[Memacs] MightEMacs script auto-formatting"
#define VLIT_ModeMsgDisp	"(Msg) Display messages on message line"
#define VLIT_ModeNoUpdate	"(noUpd) Suppress screen updates"
#define VLIT_ModeOver		"[oVer] Column overwrite when typing"
#define VLIT_ModePerl		"[Perl] Perl script auto-formatting"
#define VLIT_ModeReadFirst	"(rd1st) Read first file at startup"
#define VLIT_ModeReadOnly	"[Rdonly] Read-only buffer"
#define VLIT_ModeRegExp		"(Regexp) Regular-expression searches"
#define VLIT_ModeReplace	"[rEpl] Direct character replacement when typing"
#define VLIT_ModeRuby		"[ruBy] Ruby script auto-formatting"
#define VLIT_ModeSafeSave	"(Safe) Safe file save"
#define VLIT_ModeShell		"[Shell] Shell script auto-formatting"
#define VLIT_ModeWorkDir	"(wkDir) Display working directory on mode lines"
#define VLIT_ModeWrap		"[Wrap] Automatic word wrap"
#define VLIT_OS			"Operating system running MightEMacs"
#define VLIT_RegionText		"Current region text"
#define VLIT_ReturnMsg		"Message returned by last script command or macro"
#define VLIT_RunFile		"Pathname of running script"
#define VLIT_RunName		"Name of running macro or buffer"
#define VLIT_TermCols		"Terminal screen size in columns"
#define VLIT_TermRows		"Terminal screen size in rows (lines)"
#define VLIT_WindCount		"Number of windows on current screen"
#define VLIT_WorkDir		"Working directory"
#define VLIT_argIndex		"Index of next macro argument (beginning at 1)"
#define VLIT_autoSave		"Keystroke count that triggers auto-save"
#define VLIT_bufFile		"Filename associated with current buffer"
#define VLIT_bufFlags		"Current buffer flags (bit masks)"
#define VLIT_bufLineNum		"Current buffer line number"
#define VLIT_bufModes		"Current buffer modes (bit masks)"
#define VLIT_bufName		"Current buffer name"
#define VLIT_defModes		"Default buffer modes (bit masks)"
#if COLOR
#define VLIT_desktopColor	"Desktop color"
#endif
#define VLIT_enterBufHook	"Command or macro bound to enter-buffer hook"
#define VLIT_execPath		"Script search directories"
#define VLIT_exitBufHook	"Command or macro bound to exit-buffer hook"
#define VLIT_fencePause		"Centiseconds to pause for fence matching"
#define VLIT_globalModes	"Global modes (bit masks)"
#define VLIT_hardTabSize	"Number of columns for tab character"
#define VLIT_helpHook		"Command or macro bound to help hook"
#define VLIT_horzJump		"Horizontal window jump size (percentage)"
#define VLIT_horzScrollCol	"First displayed column in current window"
#define VLIT_inpDelim		"Input line delimiter(s) (or \"auto\")"
#define VLIT_keyMacro		"Last keyboard macro entered (in encoded form)"
#define VLIT_lastKeySeq		"Last key sequence entered"
#define VLIT_lineChar		"Character under cursor (ordinal value)"
#define VLIT_lineCol		"Column number of cursor"
#define VLIT_lineOffset		"Number of characters in line before cursor"
#define VLIT_lineText		"Text of current line"
#define VLIT_loopMax		"Maximum iterations in macro loop"
#define VLIT_modeHook		"Command or macro bound to mode-change hook"
#define VLIT_otpDelim		"Output line delimiter(s) (or $BufInpDelim)"
#define VLIT_pageOverlap	"Number of lines to overlap when paging"
#if COLOR
#define VLIT_palette		"Custom key-mapping command string"
#endif
#define VLIT_postKeyHook	"Command or macro bound to post-key hook"
#define VLIT_preKeyHook		"Command or macro bound to pre-key hook"
#define VLIT_randNumSeed	"Random number seed"
#define VLIT_readHook		"Command or macro bound to (post) read-file hook"
#define VLIT_replace		"Replacement pattern"
#define VLIT_screenNum		"Current screen ordinal number"
#define VLIT_search		"Search pattern"
#define VLIT_searchDelim	"Search pattern (input) delimiter character"
#define VLIT_showModes		"Global modes (bit masks) to show on mode line"
#define VLIT_softTabSize	"Number of spaces for tab character"
#define VLIT_travJumpSize	"Line-traversal jump size"
#define VLIT_vertJump		"Vertical window jump size (percentage)"
#define VLIT_windLineNum	"Current window line number"
#define VLIT_windNum		"Current window ordinal number"
#define VLIT_windSize		"Current window size in lines"
#define VLIT_wordChars		"Characters in a word (default \"A-Za-z0-9_\")"
#define VLIT_wrapCol		"Word wrap column"
#define VLIT_wrapHook		"Command or macro bound to wrap-word hook"
#define VLIT_writeHook		"Command or macro bound to (pre) write-file hook"

extern char
 *help0,*help1,*help2[],*usage[],
 *text0,*text1,*text2,*text3,*text4,*text5,*text6,*text7,*text8,*text9,*text10,*text11,*text12,*text13,*text14,*text15,*text16,
 *text17,*text18,*text19,*text20,*text21,*text22,*text23,*text24,*text25,*text26,*text27,*text28,*text29,*text30,*text31,
 *text32,*text33,*text34,*text35,*text36,*text37,*text38,*text39,*text40,*text41,*text42,*text43,*text44,*text45,*text46,
 *text47,*text48,*text49,*text50,*text51,*text52,*text53,
#if COLOR
 *text54,
#endif
 *text55,*text56,*text57,*text58,*text59,*text60,*text61,*text62,*text63,*text64,*text65,*text66,*text67,*text68,*text69,
 *text70,*text71,*text72,*text73,*text74,*text75,*text76,*text77,*text78,*text79,*text80,*text81,*text82,*text83,*text84,
 *text85,*text86,*text87,*text88,*text89,*text90,*text91,*text92,*text93,*text94,*text95,*text96,*text97,*text98,*text99,
 *text100,*text101,*text102,*text103,*text104,*text105,*text106,*text107,*text108,*text109,*text110,*text111,*text112,*text113,
 *text114,*text115,*text116,*text117,*text118,*text119,*text120,*text121,*text122,*text123,*text124,*text125,*text126,*text127,
 *text128,*text129,*text130,*text131,*text132,*text133,*text134,*text135,*text136,*text137,*text138,*text139,*text140,*text141,
 *text142,*text143,*text144,*text145,*text146,*text147,*text148,*text149,*text150,*text151,*text152,*text153,*text154,*text155,
 *text156,*text157,*text158,*text159,*text160,*text161,*text162,*text163,*text164,*text165,*text166,*text167,*text168,*text169,
 *text170,*text171,*text172,*text173,*text174,*text175,*text176,*text177,*text178,*text179,*text180,*text181,*text182,*text183,
 *text184,*text185,*text186,*text187,*text188,*text189,*text190,*text191,*text192,*text193,*text194,*text195,*text196,*text197,
 *text198,*text199,*text200,*text201,*text202,*text203,*text204,*text205,*text206,*text207,*text208,*text209,*text210,*text211,
 *text212,*text213,*text214,*text215,*text216,*text217,*text218,*text219,*text220,*text221,*text222,*text223,*text224,*text225,
 *text226,*text227,*text228,*text229,*text230,*text231,*text232,*text233,*text234,*text235,*text236,*text237,*text238,*text239,
 *text240,*text241,
#if COLOR
 *text242,
#endif
 *text243,*text244,*text245,*text246,*text247,*text248,*text249,*text250,*text251,*text252,*text253,*text254,*text255,*text256,
 *text257,
#if NULREGERR
 *text258,
#endif
 *text259,*text260,*text261,*text262,*text263,*text264,*text265,*text266,*text267,*text268,*text269,*text270,
 *text271,*text272,*text273,*text274,*text275,*text276,*text277,*text278,*text279,*text280,*text281,*text282,*text283,*text284,
 *text285,*text286,*text287,*text288,*text289,*text290,*text291,*text292,*text293,*text294,*text295,*text296,*text297,*text298,
 *text299,*text300,*text301,*text302,*text303,*text304,*text305,
#if MMDEBUG & DEBUG_SHOWRE
 *text306,*text307,*text308,*text309,*text310,*text311,
#endif

 *text320,*text321,*text322,*text323,*text324,*text325,
 *text326,*text327,*text328,*text329,*text330,*text331,*text332,*text333,*text334,*text335,*text336,*text337,*text338,*text339;

#endif
