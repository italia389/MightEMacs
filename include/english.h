// (c) Copyright 2020 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// english.h	English language text strings for MightEMacs.

#ifdef MainData

// **** For main.c ****

// Help text.
char
	*Help0 = "    Ver. ",
	*Help1 = " -- Fast and full-featured Emacs-style text editor\n\n",
	*Usage[] = {
		"Usage:\n    ",
		" {-?, -usage | -C, -copyright | -help | -V, -version}\n    ",
		" [-d, -dir d] [-e, -exec stmt] [-G, -global-mode [^]m[, ...]]\n    "
		" [-i, -inp-delim d] [-N, -no-user-startup] [-n, -no-startup]\n    "
		" [-o, -otp-delim d] [-path list] [-R, -no-read] [-r] [-S, -shell]\n    "
		" [[@]file [{+|-}line] [-B, -buf-mode [^]m[, ...]] [-r | -rw]\n    "
		" [-s, -search pat] ...]"},
	*Help2 = "\n\n\
Command switches and arguments:\n\
  -?, -usage	    Display usage and exit.\n\
  -C, -copyright    Display copyright information and exit.\n\
  -d, -dir d	    Specify a working directory to change to before reading\n\
		    files.\n\
  -e, -exec stmt    Specify an expression statement to be executed.  Switch may\n\
		    be repeated.\n\
  -G, -global-mode [^]m[, ...]\n\
  		    Specify one or more global modes to clear (^) or set (no ^),\n\
		    separated by commas.  Mode names may be abbreviated and\n\
		    switch may be repeated.\n\
  -help		    Display detailed help and exit.\n\
  -i, -inp-delim d  Specify one or two characters (processed as a double-quoted\n\
		    \"...\" string) to use as file input record delimiter(s);\n\
		    otherwise, the first NL, CR-LF, or CR found when a file is\n\
		    read is used.\n\
  -N, -no-user-startup\n\
		    Do not load the user startup file.\n\
  -n, -no-startup   Do not load the site or user startup files.\n\
  -o, -otp-delim d  Specify one or two characters (processed as a double-quoted\n\
		    \"...\" string) to use as file output record delimiter(s);\n\
		    otherwise, delimiter(s) found in input file ($BufInpDelim)\n\
		    is used.\n\
  -path list	    Specify colon-separated list of script search directories to\n\
		    prepend to existing path.  If list begins with a circumflex\n\
		    (^), existing path is overwritten.\n\
  -R, -no-read	    Do not read first file at startup.\n\
  -r		    Read-only: open all files with \"ReadOnly\" buffer attribute\n\
		    ON by default; otherwise, OFF.\n\
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
  -B, -buf-mode [^]m[, ...]\n\
		    Specify one or more buffer modes to clear (^) or set (no ^)\n\
		    on data file, separated by commas.  Mode names may be\n\
		    abbreviated and switch may be repeated.\n\
  -r		    Read-only: open data file with \"ReadOnly\" buffer attribute\n\
		    ON.\n\
  -rw		    Read-write: open data file with \"ReadOnly\" buffer attribute\n\
		    OFF (overrides command-level -r switch).\n\
  -s, -search pat   Search for specified pattern in data file.\n\
\nNotes:\n\
 1. If the -no-read switch is not specified, one data file is read after all\n\
    switches and arguments are processed, determined as follows: if the + or -\n\
    (go to line) or -search switch is specified on a data file argument, the\n\
    corresponding file is read and the action is performed; otherwise, the first\n\
    data file specified is read.\n\
 2. The -r and -rw argument switches may not both be specified.\n\
 3. The script execution path is initialized to the MMPATH environmental\n\
    variable if it is defined; otherwise, \"" MMPath "\".  It may\n\
    subsequently be changed by the -path switch, -exec switch, or a @file script\n\
    invocation.";

// General text messages.
const char
 text0[] = "Error",
 text1[] = "-r and -rw switch conflict",
 text2[] = "Invalid character range '%.3s' in string '%s'",
 text3[] = "%s(): Unknown ID %d for variable '%s'!",
 text4[] = "%s expected (at token '%s')",
 text5[] = "No such group, %ld",
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
 text30[] = "AHUPNTBRC   Size      Buffer              File",
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
 text41[] = "Only one buffer exists",
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
 text60[] = "~bLine:~B %lu of %lu, ~bCol:~B %d of %d, ~bByte:~B %lu of %lu (%s%%); ~bchar~B = %s",
 text61[] = "%sconflicts with -no-read",
 text62[] = "User-defined",
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
 ~ur~U ~bRestart~B, ~uESC~U|~uq~U ~bStop here~B, ~u.~U ~bStop and go back~B, ~u?~U ~bHelp~B",
 text91[] = "Repeating match at same position detected",
 text92[] = "%d substitution%s",
 text93[] = "Current window too small to shrink by %d line%s",
 text94[] = "%s(): Out of memory!",
 text95[] = "Narrowed buffer... delete visible portion",
 text96[] = "written",
 text97[] = "Invalid %s argument",
 text98[] = "Wrap column not set",
 text99[] = "file",
#if WordCount
 text100[] = "Words: %ld, word chars: %ld, region chars: %ld, lines: %d, avg. chars/word: %.2f",
#endif
 text101[] = "%sconflict",
 text102[] = "Beginning value",
 text103[] = "Saving %s...",
 text104[] = "Modified buffers exist.  Exit anyway",
 text105[] = "%s already active, cancelled",
 text106[] = "Begin %s recording",
 text107[] = "%s not active",
 text108[] = "saved",
 text109[] = "%s is %s",
 text110[] = "Default value length (%lu) exceeds maximum input length (%u)",
 text111[] = "'~b%s~B' value must be %d or greater",
 text112[] = "Maximum number of loop iterations (%d) exceeded!",
 text113[] = "Switch to",
 text114[] = "Prior loop execution level not found while rewinding stack",
 text115[] = "Word",
 text116[] = "No such function '%s'",
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
 text128[] = "buffer name",
 text129[] = "Execute script file",
 text130[] = "No such command '%s'",
 text131[] = "Read",
 text132[] = "Insert",
 text133[] = "Find",
 text134[] = "View",
 text135[] = "Old buffer",
 text136[] = " in path \"%s\"",
 text137[] = "Repeat count",
 text138[] = "New file",
 text139[] = "Reading data...",
 text140[] = "Read",
 text141[] = "I/O Error: %s, file \"%s\"",
 text142[] = "Cannot change a character past end of buffer '%s'",
 text143[] = "Line number",
 text144[] = "Write to",
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
 text168[] = "Delete all other %sbuffers",
 text169[] = "Clear",
 text170[] = "Read pipe",
 text171[] = "String expected",
 text172[] = "Token expected",
 text173[] = "Unbalanced %c%c%c string parameter",
 text174[] = "Created screen %hu",
 text175[] = "-search, + or - switch ",
 text176[] = "Function '~b%s~B' failed",
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
 text197[] = "endroutine",
 text198[] = "Misplaced '~b%s~B' keyword",
 text199[] = "if",
 text200[] = "No %s defined",
 text201[] = "End: ",
 text202[] = "%s ring is empty",
 text203[] = "Searching...",
 text204[] = "Too many arguments for '[bs]printf' function",
 text205[] = "line",
 text206[] = "Command or function argument count",
 text207[] = "Cannot get %d line%s from adjacent window",
 text208[] = "Saved %s not found",
 text209[] = "Arg: %d",
 text210[] = "~#u%s~U is only binding to core command '~b%s~B' -- cannot delete or reassign",
 text211[] = "Screen refreshed",
 text212[] = "Variable '~b%s~B' not an integer",
 text213[] = "Comma",
 text214[] = "%s cannot be nil",
 text215[] = "Create alias ",
 text216[] = "Delete user routine",
 text217[] = "'break' level '%ld' must be 1 or greater",
 text218[] = "Append to",
 text219[] = "Script failed",
 text220[] = "Parent block of loop block at line %ld not found during buffer scan",
 text221[] = "%s in RE pattern '%s'",
 text222[] = "Case of text",
 text223[] = "pop option",
 text224[] = "Binding set",
 text225[] = "Too many break levels (%d short) from inner 'break'",
 text226[] = "Cannot %s %s buffer '%s'",
 text227[] = "Terminal dimensions set to %hu x %hu",
 text228[] = "%s ring cleared",
 text229[] = ", in",
 text230[] = "at line",
 text231[] = "Toggle",
 text232[] = "Command or function name '%s' cannot exceed %d characters",
 text233[] = "Mark ~u%c~U set to previous position",
 text234[] = "Increment",
 text235[] = "Format string",
 text236[] = "i increment cannot be zero!",
 text237[] = "Invalid i format '%s' (must contain exactly one %%d, %%o, %%u, %%x, or %%X)",
 text238[] = "executing",
 text239[] = "No such window '%d'",
 text240[] = "No such screen '%d'",
 text241[] = "Maximum ring size (%ld) less than current size (%d)",
 text242[] = "Option(s) conflict with prompt type",
 text243[] = "\n* Changed buffer",
 text244[] = "No such command or alias '%s'",
 text245[] = "Division by zero is undefined (%ld/0)",
 text246[] = "Cannot write to current buffer",
 text247[] = "function",
 text248[] = "an executing",
 text249[] = "Insert pipe",
 text250[] = "Unknown pop option '%s'",
 text251[] = "%s delimiter '%s' cannot be more than %d character(s)",
 text252[] = " delimited by ",
 text253[] = "(return status %d)\n",
 text254[] = "key literal",
 text255[] = "%s, matching RE pattern '%s'",
 text256[] = "%s tab size %d must be between 2 and %d",
 text257[] = ", original file renamed to \"",
 text258[] = "~uESC~U|~uq~U ~bStop here~B, ~u.~U ~bStop and go back~B, ~uu~U ~bUndo last~B, ~ur~U ~bRestart~B, ~u?~U ~bHelp~B",
 text259[] = "No text selected",
 text260[] = "changed",
 text261[] = "Text",
 text262[] = "copied",
 text263[] = "delete",
 text264[] = "clear",
 text265[] = "Cannot change directory to \"%s\": %s",
 text266[] = "Regular expression",
 text267[] = "No such command, function, or alias '%s'",
 text268[] = "Cannot %s buffer: name '%s' cannot begin with %c",
 text269[] = "Delete alias",
 text270[] = "Cannot %s command or function buffer: name '%s' must begin with %c",
 text271[] = "No such alias '%s'",
 text272[] = "%s or %s has %hu alias(es)",
 text273[] = "Command and function buffer names (only) begin with ",
 text274[] = "WD: ",
 text275[] = "rename",
 text276[] = "modify",
 text277[] = "Change directory",
 text278[] = " set to ",
 text279[] = "Variable",
 text280[] = "%s name cannot be null or exceed %d characters",
 text281[] = "Missing spec in '%' format string",
 text282[] = "Cannot invoke '~b%s~B' command directly in a script (use '~brun~B')",
 text283[] = ", new value",
 text284[] = "Cannot %s %s buffer",
 text285[] = "Call argument",
 text286[] = "identifier",
 text287[] = "i variable set to %d",
 text288[] = "Character expected",
 text289[] = "Unexpected token '%s'",
 text290[] = "Length argument",
 text291[] = " without delimiter at EOF",
 text292[] = "variable",
 text293[] = "Cannot split a %d-line window",
 text294[] = "Only one window",
 text295[] = "Unknown prompt type '%s'",
 text296[] = "Clear all and set",
 text297[] = "Current value: ",
 text298[] = "No previous wrap column set",
 text299[] = "Pop",
 text300[] = "False return",
 text301[] = " (expression)",
 text302[] = "No such group (ref: %hu, have: %hu) in replacement pattern '%s'",
 text303[] = "Invalid wrap prefix \"%s\"",
 text304[] = "Scan completed.  Press ~uq~U to quit or ~u.~U to go back.",
 text305[] = "Ring",
 text306[] = "Pipe command",
 text307[] = "%d line%s %s",
 text308[] = "Narrowed",
 text309[] = "Directory unknown for",
 text310[] = "%s multi-screen buffer '%s'",
 text311[] = " pattern",
 text312[] = "No such command or function '%s'",
 text313[] = "command or function",
 text314[] = "Unmatched '~bcommand~B' or '~bfunction~B' keyword",
 text315[] = " Hook     Set to Function      Numeric Argument     Function Arguments",
 text316[] = "Hooks",
 text317[] = "No such hook '%s'",
 text318[] = "attribute keyword",
 text319[] = "Maximum %s recursion depth (%d) exceeded",
 text320[] = "More than one spec in '%' format string",
 text321[] = "Unknown format spec '%%%s'",
 text322[] = "Column number",
 text323[] = "Indentation exceeds wrap column (%d)",
 text324[] = ".  New name",
 text325[] = "of",
 text326[] = "Begin",
 text327[] = "User function bound to '%s' hook - cannot delete",
 text328[] = "tr \"from\" string",
 text329[] = "Unexpected %s argument",
 text330[] = "EntryText",
 text331[] = "window",
 text332[] = "%s tab size set to %d",
 text333[] = "pseudo command",
 text334[] = ", setting variable '~b%s~B'",
 text335[] = "File test code(s)",
 text336[] = "system function",
 text337[] = "Invalid escape sequence \"%.*s\"",
 text338[] = "Cannot %s a %s from a %s, cancelled",
 text339[] = "to",
 text340[] = "~bCol:~B %d of %d; ~bchar~B = %s",
 text341[] = "Cannot use key sequence ~#u%s~U as %s delimiter",
 text342[] = "prompt",
 text343[] = "search",
 text344[] = "Unknown %s argument '%s'",
 text345[] = " invalid",
 text346[] = "%s mark",
 text347[] = "Swap point with",
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
 text374[] = "buffer attribute",
 text375[] = "Attribute(s) changed",
 text376[] = "Cannot enable terminal attributes in a user command or function buffer",
 text377[] = "%s '%s' is empty",
 text378[] = "Line offset value %ld out of range",
 text379[] = "'~b%s~B' value must be between %d and %d",
 text380[] = "screen",
 text381[] = "Buffer '%s' created",
 text382[] = "\" ~bwith~B \"",
 text383[] = "(self insert)",
 text384[] = "~bi:~B %d, ~bformat:~B \"%s\", ~binc:~B %d",
 text385[] = "Change buffer name to",
 text386[] = "Command or function declaration contains invalid argument range",
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
 text410[] = "command option",
 text411[] = "Alias",
 text412[] = "User command",
 text413[] = "Function '~b%s~B' argument count (%hd, %hd) does not match hook's (%hd)",
 text414[] = "system command",
 text415[] = " interactively",
 text416[] = " not allowed",
 text417[] = "user command",
 text418[] = "user function",
 text419[] = "Nested routine%s",
 text420[] = "Hook may not be set to %s '~b%s~B'",
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
 text440[] = "No such user routine '%s'",
 text441[] = "Preprocessed",
 text442[] = "Terminal attributes enabled",
 text443[] = "display item",
 text444[] = " ring item",
 text445[] = " or 0 for new",
 text446[] = "Valid %s not specified",
 text447[] = "Invalid %s '%s'",
 text448[] = "Options",
 text449[] = "User routine",
 text450[] = "type keyword",
 text451[] = "function option",
 text452[] = "visible ",
 text453[] = "hidden ",
 text454[] = "Conflicting %ss",
 text455[] = "Missing required %s",
 text456[] = "Wrong type of mode '%s'",
 text457[] = "prompt option",
 text458[] = ", %u narrowed buffer%s skipped",
 text459[] = "Read only",
 text460[] = "read-only",
 text461[] = "Cannot set %s buffer to %s",
 text462[] = "Background (not being displayed)",
 text463[] = "Truncate to %s of buffer",
 text464[] = "beginning",
 text465[] = "end",
 text466[] = "macro",
 text467[] = "renamed",
 text468[] = "%s '%s' already exists",
 text469[] = "Save to",
 text470[] = "%s is identical to '%s'",
 text471[] = "Macro",
 text472[] = "discarded",
 text473[] = "operate on",
 text474[] = "sort option",
 text475[] = "Sorted",
 text476[] = ".  Discard",
 text477[] = "%s name cannot contain \"%s\"",
 text478[] = "New name",
 text479[] = "Directory changed.  Write buffer '%s' to file \"%s/%s\"",
 text480[] = "%s buffer '%s'",
 text481[] = "Operation not permitted on a user command or function buffer",
 text482[] = "Show",
 text483[] = "Previous",
 text484[] = "Next",
 text485[] = "User function",
 text486[] = "ring",
 text487[] = "Ring size",
 text488[] = "Ring name",
 text489[] = "op",
 text490[] = "~bRing~B: %s, ~bentries~B: %hu, ~bsize~B: %hu",
 text491[] = "Current size: %hu, new size";

// General text literals.
const char
 text800[] = "[arg, ...]",
 text801[] = "name = cf",
 text802[] = "bufname, modes[, opts]",
 text803[] = "name",
 text804[] = "bufname, arg, ...",
 text805[] = "pat",
 text806[] = "key-lit, name",
 text807[] = "name, ...",
 text808[] = "[bufname]",
 text809[] = "arg, ...",
 text810[] = "pat1, pat2",
 text811[] = "str",
 text812[] = "expr",
 text813[] = "val[, fmt[, incr]]",
 text814[] = "path",
 text815[] = "key-lit",
 text816[] = "bufname[, arg, ...]",
 text817[] = "str, from, to",
 text818[] = "str, {pat | c}[, opts]",
 text819[] = "dlm, val, ...",
 text820[] = "N",
 text821[] = "[N]",
 text822[] = "array",
 text823[] = "p[, def][, opt, ...]",
 text824[] = "array, expr",
 text825[] = "[-]pos[, [-]len]",
 text826[] = "str, [-]pos[, [-]len]",
 text827[] = "str, n",
 text828[] = "[size[, val]]",
 text829[] = "chars",
 text830[] = "pref, chars",
 text831[] = "fmt, ...",
 text832[] = "name, fn",
 text833[] = "N[, bufname]",
 text834[] = "m",
 text835[] = "file, type",
 text836[] = "{op | bufname}[, bufname, ...]",
 text837[] = "dir",
 text838[] = "dlm, str[, limit]",
 text839[] = "var, dlm",
 text840[] = "var, dlm, val",
 text841[] = "expr[, opts]",
 text842[] = "bufname[, opts]",
 text843[] = "type",
 text844[] = "[opts]",
 text845[] = "bufname, attrs",
 text846[] = "[opts,] array, expr, ...",
 text847[] = "bufname, modes",
 text848[] = "op[, {arg | opts}]",
 text849[] = "old, new",
 text850[] = "bufname, file",
 text851[] = "[m]",
 text852[] = "bufname, fmt, ...",
 text853[] = "mode[, attr, ...]",
 text854[] = "group[, attr, ...]",
 text855[] = "bufname, group[, opt]",
 text856[] = "op, {key-lit | name}",
 text857[] = "[c]",
 text858[] = "{file | array}",
 text859[] = "{file | glob}[, opts]",
 text860[] = "c",
 text861[] = "[opts,] arg, ...",
 text862[] = "tab-size",
 text863[] = "bufname, attr",
 text864[] = "pair-no, fg-color, bg-color",
 text865[] = "type, colors",
 text866[] = "type, name",
 text867[] = "bufname",
 text868[] = "file",
 text869[] = "file[, arg, ...]",
 text870[] = "file[, opts]",
 text871[] = "[name]",
 text872[] = "[name, ...]",
 text873[] = "pat[, opts]",
 text874[] = "ring",
 text875[] = "ring[, size]",
 text876[] = "ring[, name]";

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
#define CFLit_alias		"Create alias of command or function cf (and replace any existing alias if n > 0)."
#define CFLit_appendFile	"Append current buffer to named file and set buffer's state to unchanged."
#define CFLit_apropos		"Generate list of commands, functions, aliases, and variables in a new buffer and render it per\
 ~bselectBuf~B options.  If default n, all system and user commands, system functions, user functions that have help text,\
 aliases, and variables are displayed in a pop-up window if their names match given pattern; otherwise, one or more of the\
 following comma-separated option(s) may be specified in string argument opts to control which commands and functions are\
 selected: \"System\" -> select system commands and functions matching pattern; \"User\" -> select user commands matching\
 pattern and user functions that match pattern and have help text; and \"All\" -> include user functions not having help text. \
 If pattern is plain text, match is successful if name contains pat.  If pattern is nil or a null string, all names of type\
 selected by n argument and any specified options are listed.  Returns: ~bselectBuf~B values."
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
#define CFLit_beginLine		"Move point to beginning of [-]nth line (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_beginMacro	"Begin recording a macro.  [Interactive only]"
#define CFLit_beginText		"Move point to beginning of text on [-]nth line (default 1).  Returns: false if hit a buffer\
 boundary; otherwise, true."
#define CFLit_beginWhite	"Move point to beginning of white space at or immediately before point."
#define CFLit_bemptyQ		"Return true if current buffer is empty (or named buffer if n argument); otherwise, false."
#define CFLit_bgets		"Return nth next line (default 1) without trailing newline from named buffer beginning at\
 point.  If no lines left, nil is returned."
#define CFLit_binding		"Return key binding information for given op and value.  Operation keywords are: \"Name\" ->\
 name of command bound to given key-lit binding, or nil if none; \"Validate\" -> key-lit in standardized form, or nil if it is\
 invalid; or \"KeyList\" -> array of key binding(s) bound to given command.  Case of keywords is ignored."
#define CFLit_bindKey		"Bind named command to a key sequence (or single key if interactive and n <= 0)."
#define CFLit_bprint		"Write argument(s) to named buffer n times (default 1) and return string result, with\
 ~binsert~B options.  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_bprintf		"Write string built from specified format string and argument(s) to named buffer n times\
 (default 1) and return string result, with ~binsert~B options."
#define CFLit_bufAttrQ		"Check if given attribute is set in named buffer and return Boolean result.  Attributes are:\
 \"Active\" (file was read), \"Changed\", \"Command\" (user command), \"Function\" (user function), \"Hidden\", \"Narrowed\",\
 \"ReadOnly\", and \"TermAttr\" (terminal attributes enabled)."
#define CFLit_bufBoundQ		"Return true if point at beginning (n < 0), middle (n == 0), end (n > 0), or either end\
 (default) of current buffer; otherwise, false."
#define CFLit_bufInfo		"Return information for named buffer or multiple buffers, with comma-separated option(s)\
 specified in string argument opts if n argument.  If opts not specified, an array of form [bufname, filename, home-dir, bytes,\
 lines] is returned for named buffer, where the second element is the filename associated with the buffer (or nil if none), the\
 third element is the home directory associated with the buffer (or nil if none), and the fourth and fifth elements are the\
 total number of bytes and lines in the buffer.  Otherwise, if opts contains keyword \"Extended\", an array of form [bufname,\
 filename, home-dir, bytes, lines, [buf-attr, ...], [buf-mode, ...]] is returned instead, where [buf-attr, ...] and [buf-mode,\
 ...] are lists of enabled attributes (as described for the ~bbufAttr?~B function) and modes for the specified buffer.  If\
 bufname is nil, multiple arrays are returned (or buffer names only, if \"Brief\" is specified), nested within a single array. \
 One array or buffer name is returned for each buffer, which are selected by specifying one or more of the following attribute\
 keywords in opts: \"Visible\", \"Hidden\", \"Command\", and \"Function\".  Case of option keywords is ignored."
#define CFLit_bufWind		"Return ordinal number of first window on current screen (other than current window if n\
 argument) displaying named buffer, or nil if none."
#define CFLit_chgBufAttr	"Clear (n < 0), toggle (n == 0, default), set (n == 1), or clear all and set (n > 1) all of the\
 comma-separated attribute(s) specified in string argument attrs in named buffer.  Attributes are: \"Changed\", \"Hidden\",\
 \"ReadOnly\", and \"TermAttr\" (terminal attributes enabled).  Case of attribute keywords is ignored.  If n > 1, attrs may be\
 nil; otherwise, at least one attribute must be specified.  If interactive and default n, one attribute is toggled in current\
 buffer.  Returns: former state (-1 or 1) of last attribute changed or zero if n > 1 and no attributes were specified."
#define CFLit_chgDir		"Change working directory.  Returns: absolute pathname of new directory."
#define CFLit_chgMode		"Clear (n < 0), toggle (n == 0, default), set (n == 1), or clear all and set (n > 1) zero or\
 more buffer modes in named buffer (or global modes if bufname is nil).  If n > 1, modes argument may be nil; otherwise, it\
 must be either a string containing one or more comma-separated mode names or an array of mode names.  Case of mode name(s) is\
 ignored.  If interactive and default n, one buffer mode in current buffer or one global mode is toggled.  Returns: former\
 state (-1 or 1) of last mode changed, or zero if n > 1 and no modes were specified."
#define CFLit_chr		"Return string form of ASCII (int) character N."
#define CFLit_clearBuf		"Clear current buffer (or named buffer if n argument, ignoring changes if n < 0).  Returns:\
 false if buffer not cleared; otherwise, true."
#define CFLit_clearHook		"Clear named hook(s) (or clear all hooks if n <= 0)."
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
#define CFLit_countWords	"Scan region text and display counts of lines, words, and characters on message line. \
 [Interactive only]"
#endif
#define CFLit_cycleRing		"Cycle given ring [-]n times (default 1)."
#define CFLit_definedQ		"Check if an object of given type is defined and return result.  Type keywords are: \"Name\"\
 -> an object referenced by an identifier (such as a command or variable); \"Mark\" -> a mark defined anywhere in current\
 buffer; \"ActiveMark\" -> a mark defined in visible portion of narrowed buffer; \"Mode\"; and \"ModeGroup\".  Case of keywords\
 is ignored.  If \"Name\" or \"Mode\" is specified, an object type string is returned if object is found; otherwise, nil.  If\
 one of the other types is specified, a Boolean result is returned.  If \"Mark\" is specified, name argument is assumed to be\
 an ASCII (int) character; otherwise, a string.  Returned \"Name\" object type strings are: \"alias\", \"buffer\", \"pseudo\
 command\" (prefix key), \"system command\", \"system function\", \"user command\", \"user function\", or \"variable\". \
 Returned \"Mode\" object type strings are: \"buffer\" or \"global\", indicating the type of mode."
#define CFLit_delAlias		"Delete named alias(es).  Returns: zero if failure; otherwise, number of aliases deleted."
#define CFLit_delBackChar	"Delete backward [-]n characters (default 1)."
#define CFLit_delBackTab	"Delete backward [-]n hard tabs (or soft tabs if $softTabSize > 0, default 1)."
#define CFLit_delBlankLines	"Delete all blank lines above and below point, or immediately below it (or above it if n < 0)\
 if current line not blank."
#define CFLit_delBuf		"Delete named buffer(s) (and ignore changes if n < 0).  If n > 0, delete multiple buffers as\
 selected by one or more of the following comma-separated option(s) specified in string argument op: \"Visible\" -> select\
 all visible buffers; \"Inactive\" -> selected inactive, visible buffers only; \"Unchanged\" -> selected unchanged, visible\
 buffers only; \"Displayed\" and/or \"Hidden\" -> include displayed and/or hidden buffers in selection; \"Confirm\" -> ask for\
 confirmation on every buffer; and \"Force\" -> delete all selected buffers, ignoring changes, with initial confirmation.  Case\
 of option keywords is ignored.  Either \"Visible\", \"Inactive\", or \"Unchanged\" must be specified, and \"Confirm\" and\
 \"Force\" may not both be specified.  If neither of the latter two options is specified, buffers are deleted with confirmation\
 on changed ones only.  User command and function buffers are skipped unconditionally.  Buffers being displayed and hidden ones\
 are also skipped unless \"Displayed\" and/or \"Hidden\" are specified.  Returns: zero if failure; otherwise, number of buffers\
 deleted."
#define CFLit_delFencedRegion	"Delete text from point to matching ( ), [ ], { }, or < > fence character."
#define CFLit_delForwChar	"Delete forward [-]n characters (default 1)."
#define CFLit_delForwTab	"Delete forward [-]n hard tabs (or soft tabs if $softTabSize > 0, default 1)."
#define CFLit_delLine		"Delete [-]n lines (default 1, region lines if n == 0)."
#define CFLit_delMark		"Delete mark m (or all marks if n argument).  No error if script mode and mark does not exist."
#define CFLit_delRegion		"Delete region text."
#define CFLit_delRingEntry	"Delete entri(es) from given ring.  If macro ring and default n, delete named macro from the\
 ring; if not macro ring or not default n, delete the most recent n entries (default 1) from given ring (or entry number n if\
 n < 0, all entries if n == 0).  If deletion(s) were from search or replace ring, the most recent remaining entry is\
 subsequently set as the current search or replace pattern."
#define CFLit_delRoutine	"Delete named user command(s) and/or function(s).  Returns: zero if failure; otherwise, number\
 of routines deleted."
#define CFLit_delScreen		"Delete screen N (and switch to first screen if N is current screen)."
#define CFLit_delToBreak	"Delete from point through [-]n lines (default 1), not including delimiter of last line.  If\
 n == 0, delete from point to beginning of current line.  If n == 1 and point is at end of line, delete delimiter only."
#define CFLit_delWhite		"Delete white space (and non-word characters if n > 0) surrounding or immediately before point."
#define CFLit_delWind		"Delete current window and join upward.  If n argument, one or more of the following comma-\
separated option(s) may be specified in string argument opts: \"JoinDown\" -> join downward instead of upward; \"ForceJoin\" ->\
 force \"wrap around\" when joining; and \"DelBuf\" -> delete buffer after join."
#define CFLit_delWord		"Delete [-]n words (default 1, without trailing white space if n == 0).  Non-word characters\
 between point and first word are included."
#define CFLit_detabLine		"Change hard tabs to spaces in [-]n lines (default 1, region lines if n == 0) given tab size,\
 and mark line block as new region.  If tab size is nil, $hardTabSize is used."
#define CFLit_dirname		"Return directory component of given pathname (\"\" if none, \".\" if none and n argument)."
#define CFLit_dupLine		"Duplicate [-]n lines (default 1, region lines if n == 0) and move point to beginning of text\
 of duplicated block."
#define CFLit_editMode		"Create, delete, or edit a mode, given name and zero or more attributes to set or change.  If\
 mode does not exist, it is created if n > 0; otherwise, an error is returned.  If n <= 0, mode is deleted and no attribute\
 arguments are allowed; otherwise, zero or more may be specified.  Attribute arguments are strings of form \"keyword: value\"\
 where keyword is either a Boolean attribute (\"Global\", \"Buffer\", \"Visible\", \"Hidden\") or string attribute\
 (\"Description\", \"Group\"), and value is \"true\", \"false\", descriptive text, or a group name; for example, \"Hidden:\
 true\" or \"Group: ProgLang\".  \"Group: nil\" may also be specified to remove a mode from a group.  Defaults for a new mode\
 are \"Buffer: true\", \"Visible: true\".  Case of keywords and extraneous white space are ignored."
#define CFLit_editModeGroup	"Create, delete, or edit a mode group, given name and zero or more attributes to set or change.\
  If group does not exist, it is created if n > 0; otherwise, an error is returned.  If n <= 0, group is deleted and no\
 attribute arguments are allowed; otherwise, zero or more may be specified.  Attribute arguments are strings of form \"keyword:\
 value\" where keyword is a string attribute (\"Description\", \"Modes\") and value is descriptive text or a comma-separated\
 list of mode names; for example, \"Modes: C, Ruby\", which adds the specified modes to the group.  \"Modes: nil\" may also be\
 specified to clear all modes.  Case of keywords and extraneous white space are ignored."
#define CFLit_emptyQ		"Return true if expression value is nil, an ASCII null (int) character, a null string, or empty\
 array; otherwise, false."
#define CFLit_endBuf		"Move point to end of current buffer (or named buffer if n argument)."
#define CFLit_endLine		"Move point to end of [-]nth line (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_endMacro		"End recording a macro.  [Interactive only]"
#define CFLit_endWhite		"Move point to end of white space at point."
#define CFLit_endWord		"Move point to end of [-]nth word (default 1).  Returns: false if hit a buffer boundary;\
 otherwise, true."
#define CFLit_entabLine		"Change spaces to hard tabs in [-]n lines (default 1, region lines if n == 0) given tab size,\
 and mark line block as new region.  If tab size is nil, $hardTabSize is used."
#define CFLit_env	"Return value of given environmental variable.  If variable does not exist, a null string is returned."
#define CFLit_eval		"Concatenate argument(s) (or prompt for string if interactive) and execute result as an\
 expression.  Returns: result of evaluation."
#define CFLit_exit		"Exit editor with optional message (and ignore changes if n != 0, force return code 1 if\
 n < 0).  Argument(s) are converted to string and concatenated to form the message."
#define CFLit_expandPath	"Return given pathname with \"~~/\", \"~~user/\", \"$var\", and \"${var}\" expanded in same\
 manner as a shell command line argument."
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
#define CFLit_getInfo		"Return informational item for given type keyword: \"Colors\" -> list of display item colors\
 in form [[item-kw, [fg-color, bg-color]], ...] where color pair array is nil if color not set for item (or number of colors\
 and color pairs available in form [colors, pairs] if n argument); \"Editor\" -> editor name; \"Hooks\" -> hook list in form\
 [[hook-name, func-name], ...] where func-name is nil if hook not set; \"Language\" -> language of text messages; \"Modes\"\
 -> mode list in form [[mode-name, group-name, user?, global?, hidden?, scope-locked?, active?], ...] where group-name is nil\
 if mode is not in a group, and last five elements are Boolean values; \"OS\" -> operating system name; \"Screens\" -> screen\
 list in form [[screen-num, wind-count, work-dir], ...]; \"Version\" -> editor version; or \"Windows\" -> list of windows in\
 current screen in form [[wind-num, bufname], ...] (or all screens in form [[screen-num, wind-num, bufname], ...] if n\
 argument).  Case of keywords is ignored."
#define CFLit_getKey		"Get one keystroke interactively and return it as a key literal.  If n argument, one or more of\
 the following comma-separated option(s) may be specified in string argument opts: \"Char\" -> return ASCII (int) character\
 instead of key literal; \"KeySeq\" -> get a key sequence instead of one keystroke; and \"NoAbort\" -> process abort key\
 literally.  Case of option keywords is ignored.  If \"NoAbort\" is not specified, function is aborted if abort key is pressed."
#define CFLit_getWord		"Get previous (n < 0), current (n == 0, default), or next (n > 0) word from current line and\
 return it.  If indicated word does not exist or n == 0 and point is not in a word, nil is returned.  Point is left at the\
 beginning of the word if n < 0 and at the end if n >= 0.  When going backward, point movement mimics the ~bbackWord~B command\
 and therefore, the \"previous\" word is the current word if point is in a word at the start and past its first character. \
 Additionally, lines are spanned during search if n < -1 or n > 1."
#define CFLit_glob		"Return a (possibly empty) array of file(s) and/or director(ies) that match given shell glob\
 pattern."
#define CFLit_gotoFence		"Move point to matching ( ), [ ], { }, or < > fence character from one at point (or one given\
 as an argument if n argument).  Returns: true if matching fence found; otherwise, false."
#define CFLit_gotoLine	"Move point to beginning of line N (or to end of buffer if N == 0, in named buffer if n argument)."
#define CFLit_gotoMark		"Move point to mark m and restore its window framing if mark is outside of current window (or\
 force reframing if n >= 0).  If n <= 0, also delete mark after point is moved."
#define CFLit_groupModeQ	"Check if any buffer mode in given group is set in named buffer (or any global mode in given\
 group is set if bufname is nil) and return its name if so; otherwise, nil.  Case of group name is ignored.  A non-existent\
 group name is ignored if n argument given and \"Ignore\" is specified for opt."
#define CFLit_growWind		"Increase size of current window by abs(n) lines (default 1 at bottom, at top if n < 0)."
#define CFLit_huntBack	"Repeat most recent search backward n times (default 1).  Returns: string found, or false if not found."
#define CFLit_huntForw	"Repeat most recent search forward n times (default 1).  Returns: string found, or false if not found."
#define CFLit_includeQ		"Return true if any expression value(s) are in array; otherwise, false.  Expression value(s)\
 and array elements may be any data types, including array.  If n argument, one or more comma-separated options may be\
 specified in string argument opts; otherwise, the first argument must be omitted.  Options are: \"All\" -> check if all\
 expressions are in array instead of any; and \"Ignore\" -> ignore case in string comparisons.  Case of option keywords is\
 ignored."
#define CFLit_indentRegion	"Indent lines in region by n tab stops (default 1) and mark line block as new region."
#define CFLit_index		"Return position of pattern in given string, or nil if not found.  If n argument, one or both\
 of the following comma-separated option(s) may be specified in string argument opts: \"Char\" -> return position of character\
 c instead of pattern pat; and \"Last\" -> find last (rightmost) occurrence instead of first.  Case of option keywords is\
 ignored.  Beginning of string is position 0."
#define CFLit_insert		"Insert argument(s) at point n times (default 1) and return string result (without moving point\
 if n == 0, with literal newline if n < 0).  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_insertBuf		"Insert named buffer at point (without moving point if n == 0) and mark as region."
#define CFLit_insertFile	"Insert named file at point (without moving point if n == 0) and mark as region."
#define CFLit_inserti		"Format, insert at point, and add increment to i variable abs(n) times (default 1).  If n < 0,\
 point is moved back to original position after each iteration."
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
#define CFLit_joinWind		"Join current window with upper window.  If n argument, one or more of the following comma-\
separated option(s) may be specified in string argument opts: \"JoinDown\" -> join with lower window (downward) instead of\
 upper one; \"ForceJoin\" -> force \"wrap around\" when joining; and \"DelBuf\" -> delete buffer in other window after join."
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
#define CFLit_let		"Assign a value (or an expression if n argument) to a system or global variable.  [Interactive\
 only]"
#define CFLit_lowerCaseLine	"Change [-]n lines to lower case (default 1, region lines if n == 0) and mark line block as\
 new region."
#define CFLit_lowerCaseRegion	"Change all letters in region to lower case."
#define CFLit_lowerCaseStr	"Return string converted to lower case."
#define CFLit_lowerCaseWord	"Change n words to lower case (default 1)."
#define CFLit_manageMacro	"Perform macro operation per op keyword and return result.  Operation keywords are: \"Select\"\
 -> search for named macro (specified in arg) and, if found, move it to top of macro ring, activate it, and return true;\
 otherwise, return false; \"Set\" -> set given macro (specified in arg as an encoded string, or as [name, value]) and return\
 nil (or return error message if macro is a duplicate by name or value, force overwrite if n > 0); or \"Get\" -> get current\
 macro, with one or more comma-separated option(s) specified in string argument opts if n argument, and return it in encoded\
 string form, or nil if macro ring is empty.  \"Get\" options are: \"Split\" -> return current macro in form [name, value]; and\
 \"All\" -> return all macros on macro ring in form [[name, value], ...]."
#define CFLit_markBuf	"Save point position in mark " WorkMarkStr " (or mark m if n argument) and mark whole buffer as region."
#define CFLit_match		"Return text matching captured group N from last =~~ expression or index() call using a regular\
 expression as the pattern (or from last buffer search if n argument).  N must be >= 0; group 0 is entire match.  If N is out\
 of bounds, nil is returned."
#define CFLit_message		"Set a return message and return true (default) or false.  The \"RtnMsg\" global mode is\
 ignored.  If n argument, one or more comma-separated options may be specified in string argument opts; otherwise, the first\
 argument must be omitted.  Argument(s) following optional first argument are converted to string and concatenated to form the\
 message.  By default, a message is set only if one has not already been set, and is wrapped in brackets (thus informational). \
 Options are: \"Fail\" -> do not wrap message in brackets and return false instead of true; \"Force\" -> any existing message\
 is replaced unconditionally (forced); \"High\" -> message is considered high priority and will replace any existing message\
 that is not high priority and was not forced; \"NoWrap\" -> do not wrap message in brackets; and \"TermAttr\" -> enable\
 terminal attributes in message.  Case of option keywords is ignored.  If \"Force\" is specified and message is nil or a null\
 string, any existing message is cleared."
#define CFLit_metaPrefix	"Begin meta key sequence."
#define CFLit_modeQ		"Check if any of given buffer mode(s) are set in named buffer (or any of given global mode(s)\
 are set if bufname is nil) and return Boolean result.  Mode argument must be either a string containing one or more comma-\
separated mode names or an array of mode names.  If n argument, comma-separated option(s) specified in string argument opts are\
 used to control the matching process.  Options are: \"All\" -> check if all modes are set instead of any; and \"Ignore\" ->\
 ignore non-existent mode names.  Case of mode names and option keywords is ignored."
#define CFLit_moveWindDown	"Move window frame [-]n lines down (default 1)."
#define CFLit_moveWindUp	"Move window frame [-]n lines up (default 1)."
#define CFLit_narrowBuf	"Keep [-]n lines (default 1, region lines if n == 0) in current buffer and hide and preserve remainder."
#define CFLit_negativeArg	"Begin or continue entry of universal negative-trending n argument (in sequence -1, -2, ...)."
#define CFLit_newline		"Insert n lines (default 1, with auto-formatting if \"Wrap\" buffer mode enabled) or insert\
 abs(n) lines without formatting if n < 0."
#define CFLit_newlineI		"Insert n lines with same indentation as previous line (default 1)."
#define CFLit_pnBuf1		" visible buffer n times (default 1, or switch once and delete buffer that was exited from if\
 n < 0, or switch once and select "
#define CFLit_pnBuf2		" buffer having same home directory as current screen only if n == 0).  Returns: name of last\
 buffer switched to, or nil if no switch occurred."
#define CFLit_nextBuf		"Switch to next" CFLit_pnBuf1 "next" CFLit_pnBuf2
#define CFLit_nextScreen	"Switch to next screen n times (default 1)."
#define CFLit_nextWind		"Switch to next window n times (default 1)."
#define CFLit_nilQ		"Return true if expression value is nil; otherwise, false."
#define CFLit_nullQ		"Return true if string is null (empty); otherwise, false."
#define CFLit_numericQ	"Return true if string is a numeric literal which can be converted to an integer; otherwise, false."
#define CFLit_onlyWind		"Make current window the only window on screen (delete all others)."
#define CFLit_openLine		"Open abs(n) lines ahead of point (default 1).  If n < 0, also move point to first empty line\
 opened if possible."
#define CFLit_openLineI	"Open a line before the [-]nth line (default 1) with same indentation as line preceding target line."
#define CFLit_ord		"Return ASCII integer value of first character of given string."
#define CFLit_outdentRegion	"Outdent lines in region by n tab stops (default 1) and mark line block as new region."
#define CFLit_overwrite		"Overwrite text beginning at point n times (default 1) with argument(s) and return string\
 result, with ~binsert~B options.  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_pathname		"Return absolute pathname of given file or directory, or an array of absolute pathnames if\
 argument is an array (of pathnames).  If n <= 0, symbolic links are not resolved."
#define CFLit_pause		"Pause execution for N seconds (or centiseconds if n argument)."
#define CFLit_pipeBuf		"Feed current buffer through shell pipeline and read its output back into buffer, with\
 ~breadFile~B options.  Argument(s) are converted to string and concatenated to form the command.  Returns: false if failure;\
 otherwise, ~bselectBuf~B values."
#define CFLit_pop		"Remove last element from given array and return it, or nil if none left."
#define CFLit_popBuf		"Display named buffer in a pop-up window.  If n argument, one or more of the following comma-\
separated option(s) may be specified in string argument opts: \"AltModeLine\" -> use alternate mode line containing name of\
 buffer being popped; \"Delete\" -> delete buffer after it is displayed; \"Shift\" -> shift long lines left; and \"TermAttr\"\
 -> enable terminal attributes.  Case of option keywords is ignored.  Returns: name of buffer popped if not deleted afterward;\
 otherwise, nil."
#define CFLit_popFile		"Display named file in a pop-up window with ~bpopBuf~B options if n argument, plus additional\
 option: \"Existing\" -> select existing buffer with same filename if possible.  If a buffer is created for the file, it is\
 deleted after the file is displayed.  Returns: name of buffer popped if not deleted afterward; otherwise, nil."
#define CFLit_prefix1		"Begin prefix 1 key sequence."
#define CFLit_prefix2		"Begin prefix 2 key sequence."
#define CFLit_prefix3		"Begin prefix 3 key sequence."
#define CFLit_prevBuf		"Switch to previous" CFLit_pnBuf1 "previous" CFLit_pnBuf2
#define CFLit_prevScreen	"Switch to previous screen n times (default 1)."
#define CFLit_prevWind		"Switch to previous window n times (default 1)."
#define CFLit_print		"Write argument(s) to message line (with terminal attributes enabled if n argument). \
 Argument(s) are converted to string and concatenated to form the message."
#define CFLit_printf		"Write string built from specified format string and argument(s) to message line (with terminal\
 attributes enabled if n argument)."
#define CFLit_prompt		"Get terminal input and return string, ASCII (int) character, or nil result, given prompt\
 string p, optional default value def, and zero or more option arguments.  If n argument, the second argument is assumed to be\
 an ASCII character, int, string, or nil value to be used as the default input value; otherwise, it must be omitted.  Option\
 arguments that follow (if any) are strings of form \"option\" or \"keyword: value\".  Options of the first form are:\
 \"NoEcho\" -> do not echo key(s) that are typed; \"NoAuto\" -> suppress auto-completion; and \"TermAttr\" -> enable terminal\
 attributes in prompt string.  Keyword options are: \"Delim\" -> specify an input delimiter (key literal); and \"Type\" ->\
 specify one of the following input types as the keyword value: \"Buffer\" -> buffer name completion; \"Char\" -> single\
 character; \"DelRing\" -> delete ring access; \"File\" -> filename completion; \"Key\" -> one key; \"KeySeq\" -> one key\
 sequence; \"KillRing\" -> kill ring access; \"Macro\" -> macro name completion and ring access; \"Mode\" -> mode name\
 completion; \"MutableVar\" -> mutable variable name completion; \"ReplaceRing\" -> replace ring access; \"RingName\" -> ring\
 name completion; \"SearchRing\" -> search ring access; \"String\" -> string (default); or \"Var\" -> variable name completion\
 (mutable and immutable).  The prompt string may be nil or a null string for a null prompt; otherwise, the following steps are\
 taken to build the prompt string: if the prompt string begins with a letter, a colon and a space (\": \") are appended to the\
 string; otherwise, it is used as is, however any leading ~u'~U or ~u\"~U quote character is stripped off first (which force\
 the string to be used as is) and any trailing space characters are moved to the very end of the final prompt string that is\
 built.  If a non-empty default value is specified, it is displayed after the prompt string between square brackets ~#u[ ]~U if\
 the prompt type is \"Char\", and must be an ASCII character in that case; otherwise, it must be a string or int and is stored\
 in the input buffer in string form as the initial value.  A key literal is returned for types \"Key\" and \"KeySeq\";\
 otherwise, if the user presses just the delimiter key, the default value (if specified) is returned as an ASCII character if\
 prompt type is \"Char\"; otherwise, nil is returned.  If the user presses ~uC-SPC~U at any time, a null string is returned\
 instead (or a null character if the prompt type is \"Char\"); otherwise, the string in the input buffer is returned when the\
 delimiter key is pressed."
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
#define CFLit_renameBuf		"Rename buffer \"old\" to \"new\" (or if interactive and default n, rename current buffer). \
 Returns: new buffer name."
#define CFLit_renameMacro	"Rename macro \"old\" to \"new\".  The new name must not already be in use.  If n argument,\
 macro is moved to top of ring and loaded after it is renamed.  Returns: new macro name."
#define CFLit_replace		"Replace n occurrences of pat1 with pat2, beginning at point (or all occurrences if default n).\
  Returns: false if n > 0 and fewer than n replacements were made; otherwise, true."
#define CFLit_replaceText	"Replace text beginning at point n times (default 1) with argument(s) and return string result,\
 with ~binsert~B options.  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_resetTerm		"Update all screens and windows to match current terminal dimensions and redraw screen."
#define CFLit_resizeWind	"Change size of current window to abs(n) lines (growing or shrinking at bottom if n > 0, at top\
 if n < 0, or make all windows the same size if n == 0)."
#define CFLit_restoreBuf	"Switch to remembered buffer in current window and return its name."
#define CFLit_restoreScreen	"Switch to remembered screen and return its number."
#define CFLit_restoreWind	"Switch to remembered window on current screen and return its number."
#define CFLit_revertYank	"Revert text insertion from prior ~byank~B, ~byankCycle~B, ~bundelete~B, or ~bundeleteCycle~B\
 command."
#define CFLit_ringSize		"Get size information for given ring (or set maximum size to size if n argument).  If maximum\
 size of ring is zero, size is unlimited.   Returns: two-element array containing the current number of entries in the ring and\
 the maximum size."
#define CFLit_run		"Execute named command or alias interactively with numeric prefix n.  Returns: execution\
 result."
#define CFLit_saveBuf		"Remember current buffer."
#define CFLit_saveFile	"Save current buffer (if changed) to its associated file (or all changed buffers if n argument)."
#define CFLit_saveScreen	"Remember current screen."
#define CFLit_saveWind		"Remember current window."
#define CFLit_scratchBuf	"Create and switch to a buffer with unique name, with ~bselectBuf~B options.  Returns:\
 ~bselectBuf~B values."
#define CFLit_search		" for the nth occurrence of a pattern (default 1).  Pattern may contain a suffix of form\
 ':~bopts~B' for specifying options, where opts is one or more of the following letters: e -> exact matching; i -> ignore case;\
 f -> fuzzy (approximate) matching; m -> multi-line regular expression matching ('.' and '[^' match a newline); p -> interpret\
 pat as plain text; or r -> interpret pat as a regular expression; for example, '\\<\\w+\\>:re'.  If f or m is specified, r is\
 assumed.  An invalid suffix is assumed to be part of the pattern.  Options override the \"Exact\" and \"Regexp\" modes. \
 Returns: string found, or false if not found."
#define CFLit_searchBack	"Search backward" CFLit_search
#define CFLit_searchForw	"Search forward" CFLit_search
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
#define CFLit_selectLine	"Select text or line(s) around point, mark selected text as region, move point to beginning\
 of region, and return number of lines selected.  Text is selected as follows: if n <= 0 (N is line selector), select [-]N\
 lines (default 1, region lines if N == 0), not including delimiter of last line if n < 0; if n > 0 (N is text selector),\
 select text from point through [-]N lines (default 1), not including delimiter of last line.  Additionally in the latter case,\
 if N == 0, select text from point to beginning of current line; if N == 1 and point is at end of line, select delimiter only. \
 n defaults to zero."
#define CFLit_selectScreen	"Switch to screen N (or create screen and switch to it if N == 0)."
#define CFLit_selectWind	"Switch to window N (or nth from bottom if N < 0)."
#define CFLit_setBufFile	"Associate a file with named buffer (or current buffer if interactive and default n) and\
 derive new buffer name from it unless n <= 0 or filename is nil or null.  If filename is nil or null, buffer's filename is\
 cleared.  Returns: two-element array containing new buffer name and new filename."
#define CFLit_setColorPair	"Assign a foreground and background color to color pair number pair-no (or next one available\
 if pair-no is zero).  Pair number can subsequently be used for ~un~U in a ~~~un~Uc terminal attribute specification.  Returns:\
 pair number assigned."
#define CFLit_setDispColor	"Set color for a display item, given item type and either a two-element array of foreground\
 and background color numbers, or nil for no color.  Case-insensitive type keywords are \"Info\" for informational displays,\
 \"ModeLine\" for mode lines, and \"Record\" for macro recording indicator."
#define CFLit_setHook		"Set named hook to system or user function fn."
#define CFLit_seti		"Set i variable, sprintf format string, and increment if script mode or interactive and default\
 n.  If interactive and n >= 0, set i variable to n and leave format string and increment unchanged; if n < 0, display all\
 parameters on message line only."
#define CFLit_setMark		"Save point position and window framing in mark " RegionMarkStr " (or mark m if n argument)."
#define CFLit_setWrapCol	"Set wrap column to N (or previous value if n argument)."
#define CFLit_shell		"Spawn an interactive shell session.  Returns: false if failure; otherwise, true."
#define CFLit_shellCmd		"Execute a shell command and display result in a pop-up window.  If n argument, one or more\
 comma-separated options may be specified in string argument opts; otherwise, the first argument must be omitted.  Argument(s)\
 following optional first argument are converted to string and concatenated to form the command.  Options are: \"NoHdr\" -> do\
 not write command at top of result buffer; \"NoPop\" -> skip pop-up; and \"Shift\" -> shift long lines left in pop-up window. \
 Case of option keywords is ignored.  Returns: false if command fails; otherwise, true."
#define CFLit_shift		"Remove first element from given array and return it, or nil if none left."
#define CFLit_showAliases	"Generate list of aliases matching given pattern in a new buffer and render it per\
 ~bselectBuf~B options (in a pop-up window if default n).  If pattern is plain text, match is successful if name contains pat. \
 If pattern is nil or a null string, all aliases are listed.  Returns: ~bselectBuf~B values."
#define CFLit_showBuffers	"Generate list of buffers in a new buffer and render it per ~bselectBuf~B options.  If default\
 n, all visible buffers are displayed in a pop-up window; otherwise, one or more of the following comma-separated option(s) may\
 be specified in string argument opts: \"Visible\" -> select visible buffers; \"Hidden\" -> select hidden (non-\
command/function) buffers; \"Command\" -> select user command buffers;  \"Function\" -> select user function buffers; and\
 \"HomeDir\" -> include buffer's home directory.  The first column in the list is buffer attribute and state information:\
 ~u~bA~Zctive, ~u~bH~Zidden, ~u~bU~Zser command/function, ~u~bP~Zreprocessed, ~u~bN~Zarrowed, ~u~bT~ZermAttr, ~u~bB~Zackground,\
 ~u~bR~ZeadOnly, and ~u~bC~Zhanged.  Returns: ~bselectBuf~B values."
#define CFLit_showColors	"Generate a palette of available colors or defined color pairs and display it in a pop-up\
 window.  If n <= 0, display color pairs; otherwise, color palette.  In the latter case, number of colors displayed is\
 dependent on terminal type and may be split into pages, selected by n: first page is displayed for n == 1 (default), second\
 page for n == 2, and so on until all colors shown."
#define CFLit_showCommands	"Generate list of system and user commands and their bindings in a new buffer and render it per\
 ~bselectBuf~B options.  If default n, system and user commands matching given pattern are displayed in a pop-up window;\
 otherwise, one or both of the following comma-separated option(s) may be specified in string argument opts: \"System\" ->\
 select system commands matching pattern; and \"User\" -> select user commands matching pattern.  If pattern is plain text,\
 match is successful if name contains pat.  If pattern is nil or a null string, all commands of type selected by n argument and\
 any specified options are listed.  Returns: ~bselectBuf~B values."
#define CFLit_showDir		"Display working directory on message line.  Returns: absolute pathname of current directory."
#define CFLit_showFence		"Find ( ), [ ], { }, or < > fence character matching one at point (or one given as an argument\
 if n argument) and briefly display the cursor there if it is in the current window."
#define CFLit_showFunctions	"Generate list of system and user functions in a new buffer and render it per ~bselectBuf~B\
 options.  If default n, system functions matching given pattern and user functions matching given pattern that also have help\
 text are displayed in a pop-up window; otherwise, one or more of the following comma-separated option(s) may be specified in\
 string argument opts: \"System\" -> select system functions matching pattern; \"User\" -> select user functions that match\
 pattern and have help text; and \"All\" -> include user functions not having help text.  If pattern is plain text, match is\
 successful if name contains pat.  If pattern is nil or a null string, all functions of type selected by n argument and any\
 specified options are listed.  Returns: ~bselectBuf~B values."
#define CFLit_showHooks		"Generate list of hooks in a new buffer and render it per ~bselectBuf~B options (in a pop-up\
 window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showKey		"Display name of command bound to key sequence (or single key if n <= 0) in a pop-up window (or\
 on message line if n >= 0)."
#define CFLit_showMarks		"Generate list of buffer marks in a new buffer and render it per ~bselectBuf~B options (in a\
 pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showModes		"Generate list of modes in a new buffer and render it per ~bselectBuf~B options (in a pop-up\
 window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showPoint		"Display buffer position of point and current character information on message line (for\
 current line only if n argument).  [Interactive only]"
#if MMDebug & Debug_ShowRE
#define CFLit_showRegexp	"Generate list of search and replacement metacharacter arrays in a new buffer and render it per\
 ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values."
#endif
#define CFLit_showRing		"Generate list of entries from given ring in a new buffer and render it per ~bselectBuf~B\
 options (in a pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showScreens	"Generate list of screens and their associated windows and buffers in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values."
#define CFLit_showVariables	"Generate list of system and user variables matching given pattern in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  If pattern is plain text, match is successful if name\
 contains pat.  If pattern is nil or a null string, all variables are listed.  Returns: ~bselectBuf~B values."
#define CFLit_shQuote		"Return expression converted to string and enclosed in ' quotes.  Apostrophe ' characters are\
 converted to \\' so that the string is converted back to its orignal form if processed as a shell argument."
#define CFLit_shrinkWind	"Decrease size of current window by abs(n) lines (default 1 at bottom, at top if n < 0)."
#define CFLit_sortRegion	"Sort lines in region in ascending order and mark line block as new region.  If n argument, one\
 or both of the following comma-separated option(s) may be specified in string argument opts: \"Descending\" -> sort in\
 descending order; and \"Ignore\" -> ignore case in line comparisons.  Case of option keywords is ignored."
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
#define CFLit_strip		"Return string stripped of white space at both ends (or left end only if n < 0, right end only\
 if n > 0)."
#define CFLit_sub		"Replace first (or all if n > 1) occurrences of \"from\" pattern in str with \"to\" pattern and\
 return result."
#define CFLit_subline		"Extract substring of given length from current line beginning at point (position 0) + pos and\
 return it.  Length is infinite if not specified.  If length is negative, it specifies a position backward from end of line at\
 which to end the extraction, not including the ending character.  Delimiter at end of line is not included in result."
#define CFLit_substr		"Extract substring of given length from str beginning at pos and return it.  First position is\
 0.  Length is infinite if not specified.  If length is negative, it specifies a position backward from end of str at which to\
 end the extraction, not including the ending character."
#define CFLit_suspend		"Suspend editor (put in background) and return to shell session."
#define CFLit_swapMark		"Swap point position with mark m and restore its window framing if mark is outside of current\
 window (or force reframing if n >= 0)."
#define CFLit_tab		"Insert hard tab (or soft tab if $softTabSize > 0) n times (default 1).  If n <= 0, soft tab\
 size is set to abs(n) and no tabs are inserted."
#define CFLit_titleCaseLine	"Change words in [-]n lines to title case (default 1, region lines if n == 0) and mark line\
 block as new region."
#define CFLit_titleCaseRegion	"Change all words in region to title case.  First character in region is assumed to be the\
 beginning of a word if it is a word character."
#define CFLit_titleCaseStr	"Return string with all words it contains converted to title case."
#define CFLit_titleCaseWord	"Change n words to title case (default 1).  Character at point is assumed to be the beginning\
 of a word if it is a word character."
#define CFLit_toInt		"Return numeric string literal converted to integer, ignoring any leading or trailing white\
 space.  String may be in any form recognized by the ~bstrtol()~B C library function with a zero base."
#define CFLit_toStr		"Return expression value converted to string form.  If n argument, one or more of the following\
 comma-separated option(s) may be specified in string argument opts to control the conversion process: \"Delimiter\" ->\
 include comma delimiter between array elements if expression is an array; \"Quote1\" or \"Quote2\" -> enclose string values\
 in ' or \" quotes, respectively; \"ShowNil\" -> convert nil value(s) to \"nil\"; and \"Visible\" -> convert invisible\
 characters to visible form.  Case of option keywords is ignored.  \"Quote1\" and \"Quote2\" cannot both be specified and are\
 ignored if expression is an array and \"Delimiter\" is not also specified."
#define CFLit_tr		"Return translated string.  Each \"from\" character in string is replaced with corresponding\
 \"to\" character.  If \"to\" string is shorter than \"from\" string, the last \"to\" character is used for each additional\
 \"from\" character.  If \"to\" string is nil or null, all \"from\" characters in string are deleted."
#define CFLit_traverseLine	"Move point forward or backward $travJump characters (or to far right if n == 0).  If n != 0,\
 first traversal is accelerated forward (n > 0) or backward (n < 0)."
#define CFLit_trimLine		"Trim white space from end of [-]n lines (default 1, region lines if n == 0) and mark line\
 block as new region."
#define CFLit_truncBuf		"Delete all text from point to end of buffer (or from point to beginning of buffer if n <= 0),\
 with confirmation if interactive."
#define CFLit_typeQ		"Return type of expression: \"array\", \"bool\", \"int\", \"nil\", or \"string\"."
#define CFLit_upperCaseLine	"Change [-]n lines to upper case (default 1, region lines if n == 0) and mark line block as\
 new region."
#define CFLit_upperCaseRegion	"Change all letters in region to upper case."
#define CFLit_upperCaseStr	"Return string converted to upper case."
#define CFLit_upperCaseWord	"Change n words to upper case (default 1)."
#define CFLit_unbindKey		"Unbind a key sequence (or single key if interactive and n <= 0).  Returns: true if successful;\
 otherwise, false."
#define CFLit_undelete		"Insert text from last \"delete\" command n times (default 1) and mark as region (or insert\
 once without moving point if n == 0, insert nth older delete once if n < 0)."
#define CFLit_undeleteCycle	"Replace undeleted text with next older ring entry (default) or cycle delete ring [-]n times\
 and use topmost entry (n != 0).  New text is marked as region.  If n == 0 or last command was not an ~bundelete~B or\
 ~bundeleteCycle~B, no action is taken."
#define CFLit_universalArg	"Begin or continue entry of universal positive-trending n argument (in sequence 2, 0, 3, 4,\
 ...)."
#define CFLit_unshift		"Prepend expression value to given array and return new array contents."
#define CFLit_updateScreen	"Update and redraw screen if altered and no keystrokes pending.  If n argument, one or both of\
 the following comma-separated option(s) may be specified in string argument opts: \"Force\" -> pending keystrokes are ignored;\
 and \"NoRedraw\" -> redraw of physical screen is suppressed.  Case of option keywords is ignored."
#define CFLit_viewFile		"Find named file and read (if n != 0) into a read-only buffer with ~bselectBuf~B options\
 (without auto-completion if interactive and n == 0 or n == 1).  If a buffer already exists with the same filename, its buffer\
 is selected; otherwise, a buffer is created.  Returns: ~bselectBuf~B values."
#define CFLit_widenBuf		"Expose all hidden lines in current buffer (unnarrow)."
#define CFLit_wordCharQ		"Return true if character c is a letter, digit, or underscore (any of the characters\
 \"A-Za-z0-9_\"); otherwise, false."
#define CFLit_wrapLine		"Rewrap [-]n lines (default 1, region lines if n == 0) at $wrapCol, given a \"line prefix\"\
 string and \"end of sentence\" characters to pass to the ~bjoinLines~B command.  All lines in the block are filled and/or\
 wrapped so that they have the same indentation as the first line and do not extend past the wrap column.  Additionally, if\
 the line prefix is not nil or a null string (for example, \"# \" for a comment block) it is inserted after any indentation in\
 each line that is created during the wrapping process.  Existing prefix strings (with or without trailing white space) are\
 also recognized and removed from each line of the block before ~bjoinLines~B is called.  When command is invoked\
 interactively, a null string is used for the \"chars\" argument."
#define CFLit_wrapWord		"Move word at point to beginning of next line if word is past column n (default 0); otherwise,\
 move following word (if any) or start a new line."
#define CFLit_writeBuf		"Write lines in region (or characters in region if n >= 0) to named buffer.  If n <= 0, also\
 delete text that was written.  Returns: text written."
#define CFLit_writeFile		"Write current buffer to named file.  If default n or n > 0, also set buffer's filename to file\
 path, derive new buffer name from it, and set buffer's state to unchanged."
#define CFLit_xeqBuf		"Execute named buffer as a user command with numeric prefix n and optional arguments.  Returns:\
 execution result."
#define CFLit_xeqFile		"Search for named file with ~bxPathname~B function and execute file that is found as a user\
 command with numeric prefix n and optional arguments.  Returns: execution result."
#define CFLit_xeqMacro		"Execute current macro (or named macro if n <= 0) abs(n) times (default 1, with infinite\
 repetitions if n == 0).  Execution stops after n or $maxLoop iterations (if not zero), or if any invoked command or function\
 returns false."
#define CFLit_xPathname		"Search for given file or directory in $execPath directories and return\
 absolute pathname if found; otherwise, nil.  (Pathname is searched for verbatim first, then with \"" ScriptExt "\" appended\
 unless the filename already has that extension.)  If n argument, one or both of the following comma-separated option(s) may\
 be specified in string argument opts: \"Glob\" -> process first argument as a shell glob pattern instead of a file or\
 directory, and return a (possibly empty) array of absolute pathnames that match; and \"SkipNull\" -> skip null directories in\
 $execPath (so that the current directory is not searched).  Case of option keywords is ignored.  If the given file or\
 directory argument begins with a slash (/) and a glob search is being performed, an error is returned (use ~bglob~B function\
 instead); otherwise, it is searched for as an absolute pathname, bypassing the directories in $execPath."
#define CFLit_yank		"Insert last kill n times (default 1) and mark as region (or insert once without moving point\
 if n == 0, insert nth older kill once if n < 0)." 
#define CFLit_yankCycle		"Replace yanked text with next older ring entry (default) or cycle kill ring [-]n times and\
 use topmost entry (n != 0).  New text is marked as region.  If n == 0 or last command was not a ~byank~B or ~byankCycle~B,\
 no action is taken."

// Hook help text.
#define HLitN_defn		"defn."
#define HLitN_chgDir		"From ~bchgDir~B command^ ~uOR~U defn at startup."
#define HLitN_exit		"From ~bexit~B or^ ~bquickExit~B command."
#define HLitN_postKey		"Key prefix."
#define HLitN_preKey		HLitN_postKey

#define HLitArg_none		"~b0:~B (none)"
#define HLitArg_createBuf	"~b1:~B Buffer name."
#define HLitArg_enterBuf	"~b1:~B \"exitBuf\" return^  value."
#define HLitArg_mode		"~b2:~B nil, prior value of^  $GlobalModes ~uOR~U^  buffer name, prior^  value of $BufModes."
#define HLitArg_postKey		"~b1:~B \"preKey\" return^  value."
#define HLitArg_read		"~b2:~B Buffer name, filename^  (or nil if standard^  input)."
#define HLitArg_write		"~b2:~B Buffer name, filename."
#define HLitArg_filename	"~b2:~B Buffer name, filename^  (or nil if none)."

// Mode descriptions.
#define MLit_AutoSave		"Automatic file save."
#define MLit_AutoTerm		"Automatic termination of last line of buffer when saving."
#define MLit_Backup		"Create backup (" BackupExt ") file when saving."
#define MLit_Clobber		"Allow user commands and functions to be recreated."
#define MLit_ColDisp		"Display column number of point on mode line."
#define MLit_Exact		"Case-sensitive searches by default."
#define MLit_Fence1		"Show matching ( ), [ ], or { } fence when typing."
#define MLit_Fence2		"Show matching ( ), [ ], { }, or < > fence when typing."
#define MLit_HorzScroll		"Horizontally scroll all window lines simultaneously."
#define MLit_LineDisp		"Display line number of point on mode line."
#define MLit_Overwrite		"Overwrite columns when typing."
#define MLit_Regexp		"Regular expression searches by default."
#define MLit_Replace		"Replace characters when typing."
#define MLit_RtnMsg		"Save and display return messages."
#define MLit_SafeSave		"Safe file save (write temporary file first)."
#define MLit_WorkDir		"Display working directory on bottom mode line if room."
#define MLit_Wrap		"Automatic word wrap."

// Mode group descriptions.
#define MGLit_Fence		"Fence-matching modes."
#define MGLit_Typeover		"Typing-overwrite modes."

#elif defined(HelpData)

// **** For help.c ****

// "About MightEMacs" help text.
#define ALit_Author		"(c) Copyright 2020 Richard W. Marinelli <italian389@yahoo.com>"
#define ALit_Footer1		" runs on Unix and Linux platforms and is licensed under the"
#define ALit_Footer2		"GNU General Public License (GPLv3), which can be viewed at"
#define ALit_Footer3		"http://www.gnu.org/licenses/gpl-3.0.en.html"
#define ALit_BuildInfo		"Build Information"
#define ALit_MaxCols		"Maximum terminal columns"
#define ALit_MaxRows		"Maximum terminal rows"
#define ALit_MaxTabSize		"Maximum hard or soft tab size"
#define ALit_MaxPathname	"Maximum length of file pathname"
#define ALit_MaxBufname		"Maximum length of buffer name"
#define ALit_MaxModeGrpName	"Maximum length of mode or group name"
#define ALit_MaxUserVar		"Maximum length of user variable name"
#define ALit_MaxTermInp		"Maximum length of terminal input string"
#define ALit_LibInternals	"Internals library"
#define ALit_LibRegexp		"Regexp library"

#elif defined(VarData)

// **** For var.c ****

// System variable help text.
#define VLit_ARGV		"Array containing user command/function arguments, if any, when a user routine (or buffer) is\
 being executed."
#define VLit_BufInpDelim	"Line delimiter(s) used for last file read into current buffer."
#define VLit_BufModes		"Array containing names of active buffer modes in current buffer."
#define VLit_Date		"Current date and time."
#define VLit_GlobalModes	"Array containing names of active global modes."
#define VLit_HorzScrollCol	"First displayed column of current line."
#define VLit_LastKey		"Ordinal (integer) value of last key entered, or -1 if a key sequence was entered or key was\
 not a 7-bit ASCII character."
#define VLit_LineLen		"Length of current line in characters, not including newline delimiter."
#define VLit_Match		"String matching pattern from last buffer search, ~bindex~B function, or =~~ expression."
#define VLit_RegionText		"Text in current region."
#define VLit_ReturnMsg		"Message returned by last command or script."
#define VLit_RingNames		"Array containing names of rings."
#define VLit_RunFile		"Pathname of running script."
#define VLit_RunName		"Name of running user command, user function, or buffer."
#define VLit_ScreenCount	"Number of existing screens."
#define VLit_TermSize		"Array containing terminal screen size in columns and rows."
#define VLit_WindCount		"Number of windows on current screen."
#define VLit_autoSave		"Keystroke count that triggers auto-save.  Feature is disabled if value is zero."
#define VLit_bufFile		"Filename associated with current buffer."
#define VLit_bufLineNum		"Ordinal number of line containing point in current buffer."
#define VLit_bufname		"Name of current buffer."
#define VLit_execPath		"Colon-separated list of script search directories.  If a directory is null, the current\
 directory is searched."
#define VLit_fencePause		"Centiseconds to pause for fence matching (when \"Fence1\" or \"Fence2\" mode is enabled)."
#define VLit_hardTabSize	"Size of a hard tab (character) in columns."
#define VLit_horzJump		"Percentage of horizontal window size to jump when scrolling (0-" JumpMaxStr ", 0 for smooth\
 scrolling, default " HorzJumpStr ")."
#define VLit_inpDelim		"Global input line delimiter(s).  Must be one or two characters, or a null string for \"auto\"."
#define VLit_lastKeySeq		"Last key sequence entered in string form."
#define VLit_lineChar		"Character at point as an ASCII (int) value."
#define VLit_lineCol		"Column number of point."
#define VLit_lineOffset		"Number of characters in line before point."
#define VLit_lineText		"Text of current line without newline delimiter."
#define VLit_maxArrayDepth	"Maximum depth allowed in array recursion (or unlimited if zero)."
#define VLit_maxCallDepth	"Maximum depth allowed in user command or function recursion (or unlimited if zero)."
#define VLit_maxLoop		"Maximum number of iterations allowed for a loop block or macro (or unlimited if zero)."
#define VLit_maxPromptPct	"Maximum percentage of terminal width to use for a prompt string (in range 15-90)."
#define VLit_otpDelim	"Global output line delimiter(s).  Must be one or two characters.  If null, $BufInpDelim is used."
#define VLit_pageOverlap	"Number of lines to overlap when paging."
#define VLit_randNumSeed	"Random number seed (in range 1 to 2^32 - 1)  Auto-generated if set to zero."
#define VLit_replacePat		"Replacement pattern."
#define VLit_screenNum		"Ordinal number of current screen."
#define VLit_searchDelim	"Terminal input delimiter key for search and replacement patterns."
#define VLit_searchPat		"Search pattern."
#define VLit_softTabSize	"Number of spaces to use for a soft tab.  If greater than zero, soft tabs are used; otherwise,\
 hard tabs."
#define VLit_travJump		"Number of characters to jump when traversing current line with ~btravLine~B command (minimum\
 4, maximum 25% of terminal width, default " TravJumpStr ")."
#define VLit_vertJump		"Percentage of vertical window size to jump when scrolling (0-" JumpMaxStr ", 0 for smooth\
 scrolling, default " VertJumpStr ")."
#define VLit_windLineNum	"Ordinal number of line containing point in current window."
#define VLit_windNum		"Ordinal number of current window."
#define VLit_windSize		"Size of current window in lines, not including mode line."
#define VLit_workDir		"Working directory."
#define VLit_wrapCol		"Word wrap column."

#endif
#ifndef MainData

// **** For all the other .c files ****

extern const char
 *help0, *help1, *help2[], *usage[],
 text0[], text1[], text2[], text3[], text4[], text5[], text6[], text7[], text8[], text9[], text10[], text11[], text12[],
 text13[], text14[], text15[], text16[], text17[], text18[], text19[], text20[], text21[], text22[], text23[], text24[],
 text25[], text26[], text27[], text28[], text29[], text30[], text31[], text32[], text33[], text34[], text35[], text36[],
 text37[], text38[], text39[], text40[], text41[], text42[], text43[], text44[], text45[], text46[], text47[], text48[],
 text49[], text50[], text51[], text52[], text53[], text54[], text55[], text56[], text57[], text58[], text59[], text60[],
 text61[], text62[], text63[], text64[], text65[], text66[], text67[], text68[], text69[], text70[], text71[], text72[],
 text73[], text74[], text75[], text76[], text77[], text78[], text79[], text80[], text81[], text82[], text83[], text84[],
 text85[], text86[], text87[], text88[], text89[], text90[], text91[], text92[], text93[], text94[], text95[], text96[],
 text97[], text98[], text99[], text100[], text101[], text102[], text103[], text104[], text105[], text106[], text107[],
 text108[], text109[], text110[], text111[], text112[], text113[], text114[], text115[], text116[], text117[], text118[],
 text119[], text120[], text121[], text122[], text123[], text124[], text125[], text126[], text127[], text128[], text129[],
 text130[], text131[], text132[], text133[], text134[], text135[], text136[], text137[], text138[], text139[], text140[],
 text141[], text142[], text143[], text144[], text145[], text146[], text147[], text148[], text149[], text150[], text151[],
 text152[], text153[], text154[], text155[], text156[], text157[], text158[], text159[], text160[], text161[], text162[],
 text163[], text164[], text165[], text166[], text167[], text168[], text169[], text170[], text171[], text172[], text173[],
 text174[], text175[], text176[], text177[], text178[], text179[], text180[], text181[], text182[], text183[], text184[],
 text185[], text186[], text187[], text188[], text189[], text190[], text191[], text192[], text193[], text194[], text195[],
 text196[], text197[], text198[], text199[], text200[], text201[], text202[], text203[], text204[], text205[], text206[],
 text207[], text208[], text209[], text210[], text211[], text212[], text213[], text214[], text215[], text216[], text217[],
 text218[], text219[], text220[], text221[], text222[], text223[], text224[], text225[], text226[], text227[], text228[],
 text229[], text230[], text231[], text232[], text233[], text234[], text235[], text236[], text237[], text238[], text239[],
 text240[], text241[], text242[], text243[], text244[], text245[], text246[], text247[], text248[], text249[], text250[],
 text251[], text252[], text253[], text254[], text255[], text256[], text257[], text258[], text259[], text260[], text261[],
 text262[], text263[], text264[], text265[], text266[], text267[], text268[], text269[], text270[], text271[], text272[],
 text273[], text274[], text275[], text276[], text277[], text278[], text279[], text280[], text281[], text282[], text283[],
 text284[], text285[], text286[], text287[], text288[], text289[], text290[], text291[], text292[], text293[], text294[],
 text295[], text296[], text297[], text298[], text299[], text300[], text301[], text302[], text303[], text304[], text305[],
 text306[], text307[], text308[], text309[], text310[], text311[], text312[], text313[], text314[], text315[], text316[],
 text317[], text318[], text319[], text320[], text321[], text322[], text323[], text324[], text325[], text326[], text327[],
 text328[], text329[], text330[], text331[], text332[], text333[], text334[], text335[], text336[], text337[], text338[],
 text339[], text340[], text341[], text342[], text343[], text344[], text345[], text346[], text347[], text348[], text349[],
 text350[], text351[], text352[], text353[], text354[], text355[], text356[], text357[], text358[], text359[], text360[],
 text361[], text362[], text363[], text364[], text365[], text366[], text367[], text368[], text369[], text370[], text371[],
 text372[], text373[], text374[], text375[], text376[], text377[], text378[], text379[], text380[], text381[], text382[],
 text383[], text384[], text385[], text386[], text387[], text388[], text389[], text390[], text391[], text392[], text393[],
 text394[], text395[], text396[], text397[], text398[], text399[], text400[], text401[], text402[], text403[], text404[],
 text405[], text406[], text407[], text408[], text409[], text410[], text411[], text412[], text413[], text414[], text415[],
 text416[], text417[], text418[], text419[], text420[], text421[], text422[], text423[], text424[], text425[], text426[],
 text427[], text428[], text429[], text430[], text431[], text432[], text433[], text434[], text435[], text436[], text437[],
 text438[], text439[], text440[], text441[], text442[], text443[], text444[], text445[], text446[], text447[], text448[],
 text449[], text450[], text451[], text452[], text453[], text454[], text455[], text456[], text457[], text458[], text459[],
 text460[], text461[], text462[], text463[], text464[], text465[], text466[], text467[], text468[], text469[], text470[],
 text471[], text472[], text473[], text474[], text475[], text476[], text477[], text478[], text479[], text480[], text481[],
 text482[], text483[], text484[], text485[], text486[], text487[], text488[], text489[], text490[], text491[];
extern const char
 text800[], text801[], text802[], text803[], text804[], text805[], text806[], text807[], text808[], text809[], text810[],
 text811[], text812[], text813[], text814[], text815[], text816[], text817[], text818[], text819[], text820[], text821[],
 text822[], text823[], text824[], text825[], text826[], text827[], text828[], text829[], text830[], text831[], text832[],
 text833[], text834[], text835[], text836[], text837[], text838[], text839[], text840[], text841[], text842[], text843[],
 text844[], text845[], text846[], text847[], text848[], text849[], text850[], text851[], text852[], text853[], text854[],
 text855[], text856[], text857[], text858[], text859[], text860[], text861[], text862[], text863[], text864[], text865[],
 text866[], text867[], text868[], text869[], text870[], text871[], text872[], text873[], text874[], text875[], text876[];
#if MMDebug & Debug_ShowRE
extern char
 text994[], text995[], text996[], text997[], text998[], text999[];
#endif
#endif
