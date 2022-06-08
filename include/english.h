// (c) Copyright 2022 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// english.h	English language text strings for MightEMacs.

#ifdef MainData

// **** For main.c ****

// Help text.
char
	*Help1 = " -- Fast and full-featured Emacs-style text editor\n\n",
	*Usage[] = {
		"Usage:\n    ",
		" {-?, -usage | -C, -copyright | -help | -V, -version}\n    ",
		" [-D] [-d, -dir d] [-e, -exec stmt] [-G, -global-mode [^]m[, ...]]\n    "
		" [-i, -inp-delim d] [-N, -no-user-startup] [-n, -no-startup]\n    "
		" [-o, -otp-delim d] [-path list] [-R, -no-read] [-r] [-S, -shell]\n    "
		" [-x, -xeq file] [file [{+|-}line] [-B, -buf-mode [^]m[, ...]]\n    "
		" [-r | -rw] [-s, -search pat] ...]"},
	*Help2 = "\n\n\
Command switches and arguments:\n\
  -?, -usage	    Display usage and exit.\n\
  -C, -copyright    Display copyright information and exit.\n\
  -D		    Change working directory to directory component of first\n\
		    file argument if present, before reading files.\n\
  -d, -dir d	    Specify a working directory to change to before reading\n\
		    files.\n\
  -e, -exec stmt    Specify an expression statement to be executed.  Switch may\n\
		    be repeated.\n\
  -G, -global-mode [^]m[, ...]\n\
  		    Specify one or more global modes to disable (^) or enable\n\
		    (no ^), separated by commas.  Mode names may be abbreviated\n\
		    and switch may be repeated.\n\
  -help		    Display detailed help and exit.\n\
  -i, -inp-delim d  Specify one or two characters (processed as a double-quoted\n\
		    \"...\" string) to use as file input record delimiter(s),\n\
		    otherwise the first NL, CR-LF, or CR found when a file is\n\
		    read is used.\n\
  -N, -no-user-startup\n\
		    Do not load the user startup file.\n\
  -n, -no-startup   Do not load the site or user startup files.\n\
  -o, -otp-delim d  Specify one or two characters (processed as a double-quoted\n\
		    \"...\" string) to use as file output record delimiter(s),\n\
		    otherwise delimiter(s) found in input file ($BufInpDelim)\n\
		    is used.\n\
  -path list	    Specify colon-separated list of directories to set as script\n\
		    execution path.\n\
  -R, -no-read	    Do not read first file at startup.\n\
  -r		    Enable \"ReadOnly\" global mode at startup.\n\
  -S, -shell	    Denote file (beginning with \"#!/usr/local/bin/memacs -S\")\n\
		    as an executable " ProgName " script.  Shell arguments are\n\
		    passed to script when file is executed.\n\
  -V, -version	    Display program version and exit.\n\
  -x, -xeq file	    Execute specified MScript file (in path) before processing\n\
		    file argument(s).\n\
\n  file		    File to open for viewing or editing.  If \"-\" is specified\n\
		    as the filename, data is read from standard input into\n\
		    buffer \"" Buffer1 "\".\n\
Argument switches:\n\
  {+|-}line	    Go to specified line number from beginning (+) or end (-) of\n\
		    data file, or to end of file if line number is zero.\n\
  -B, -buf-mode [^]m[, ...]\n\
		    Specify one or more buffer modes to disable (^) or enable\n\
		    (no ^) on data file, separated by commas.  Mode names may be\n\
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
 3. The -dir, -no-user-startup, -no-startup, and -path switches are processed\n\
    before startup file(s) are loaded.  All other switches are processed\n\
    afterward.\n\
 4. The script execution path is initialized to the -path switch value (relative\n\
    to the -dir directory, if specifed), the MSPATH environmental variable if it\n\
    is defined, or \"" MMPath "\" otherwise.  It may be\n\
    subsequently changed by the -exec switch or a -xeq script invocation.";

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
 text40[] = "Line overflow, wrapping descriptive text for name '~b%.*s~B'",
 text41[] = "Only one buffer exists",
 text42[] = "%s ring cycled",
 text43[] = "Ring name",
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
 text114[] = "~bRing~B: %s, ~bentries~B: %hu, ~bsize~B: %hu",
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
 text126[] = "Current size: %hu, new size",
 text127[] = "alias",
 text128[] = "buffer name",
 text129[] = "Execute MScript file",
 text130[] = "No such command '%s'",
 text131[] = "Read",
 text132[] = "Insert",
 text133[] = "Find",
 text134[] = "View",
 text135[] = "Old buffer",
 text136[] = "No such character class '%s'",
 text137[] = "Repeat count",
 text138[] = "New file",
 text139[] = "Reading data...",
 text140[] = "Buffer does not exist.  Create it",
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
 text155[] = "Ring size",
 text156[] = "%s(): Out of memory allocating %u-byte line buffer for file \"%s\"!",
 text157[] = "ring",
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
 text183[] = "User function",
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
 text250[] = "Show",
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
 text278[] = "changed to",
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
 text344[] = "Operation not permitted on a %s buffer",
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
 text368[] = "%d buffer%s deleted",
 text369[] = " in buffer '%s'",
 text370[] = "Array element %d not an integer",
 text371[] = "Array expected",
 text372[] = "Buffer '%s' deleted",
 text373[] = "Illegal value for '~b%s~B' variable",
 text374[] = "buffer attribute",
 text375[] = "Attribute(s) changed",
 text376[] = "user command or function",
 text377[] = "%s '%s' is empty",
 text378[] = "Line offset value %ld out of range",
 text379[] = "'~b%s~B' value must be between %d and %d",
 text380[] = "screen",
 text381[] = "Buffer '%s' created",
 text382[] = "\" ~bwith~B \"",
 text383[] = "(self insert)",
 text384[] = "~bi:~B %d, ~bformat:~B \"%s\", ~bincr:~B %d",
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
 text404[] = "Cannot resolve path \"%s\": %s",
 text405[] = "Incomplete line \"%.*s\"",
 text406[] = "Glob of pattern \"%s\" failed: %s",
 text407[] = "Invalid argument for %s glob search",
 text408[] = "Unterminated /#...#/ comment",
 text409[] = "CWD: %s",
 text410[] = "command option",
 text411[] = "Alias",
 text412[] = "User command",
 text413[] = "Function '~b%s~B' argument count (%hd, %hd) does not match hook's (%hd)",
 text414[] = "system command",
 text415[] = "%s buffer '%s'",
 text416[] = " not allowed",
 text417[] = "user command",
 text418[] = "user function",
 text419[] = "Nested routine%s",
 text420[] = "Hook may not be set to %s '~b%s~B'",
 text421[] = "initializing ncurses",
 text422[] = "Invalid color pair value(s)",
 text423[] = "Directory changed.  Write buffer '%s' to file \"%s/%s\"",
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
 text446[] = "Screen %d and %d homed buffer%s deleted",
 text447[] = "Invalid %s '%s'",
 text448[] = "Options",
 text449[] = "User routine",
 text450[] = "type keyword",
 text451[] = "function option",
 text452[] = "visible ",
 text453[] = "hidden ",
 text454[] = "Conflicting %ss",
 text455[] = "Missing required %s",
 text456[] = "\n\n\t(No entries found)",
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
 text479[] = "File already exists.  %s",
 text480[] = "Allow overwrite",
 text481[] = "Overwrite",
 text482[] = "0 => readPipe \"for f in",
 text483[] = "; do if [ -d \\\"$f\\\" ]; then echo \\\"$f/\\\"; elif [ -L \\\"$f\\\" ] || [ -e \\\"$f\\\" ]; then"
		" echo \\\"$f\\\"; fi; done\"",
 text484[] = "Cannot %s file \"%s\": %s",
 text485[] = "Buffer and file \"%s\" %s",
 text486[] = "File \"%s\" %s",
 text487[] = "Make hard link from existing",
 text488[] = "Make symbolic link from existing",
 text489[] = "to new",
 text490[] = "Cannot create link file \"%s\": %s",
 text491[] = "Link file \"%s\" created",
 text492[] = "Rename file in",
 text493[] = "No filename set for %s '%s'",
 text494[] = "Disk file",
 text495[] = "File",
 text496[] = "~bCurrent wrap col:~B %d, ~bprevious wrap col:~B %d",
 text497[] = "parameter name",
 text498[] = "previous";

// General text literals.
const char
 text800[] = "[arg, ...]",
 text801[] = "name = cf",
 text802[] = "mode[, bufname]",
 text803[] = "name",
 text804[] = "bufname, arg, ...",
 text805[] = "pat",
 text806[] = "key-lit, name",
 text807[] = "name, ...",
 text808[] = "[bufname]",
 text809[] = "arg, ...",
 text810[] = "spat, rpat",
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
 text823[] = "[prmt[, opts[, def]]]",
 text824[] = "array, expr",
 text825[] = "[-]pos[, [-]len]",
 text826[] = "str, [-]pos[, [-]len]",
 text827[] = "str, n",
 text828[] = "[size[, val]]",
 text829[] = "chars",
 text830[] = "pref, chars",
 text831[] = "fmt[, arg, ...]",
 text832[] = "name, func",
 text833[] = "N[, bufname]",
 text834[] = "m",
 text835[] = "file, type",
 text836[] = "{bufname, ... | opts}",
 text837[] = "dir",
 text838[] = "dlm, str[, limit]",
 text839[] = "var, dlm",
 text840[] = "var, dlm, val",
 text841[] = "expr[, opts[, dlm]]",
 text842[] = "bufname[, opts]",
 text843[] = "type",
 text844[] = "[opts]",
 text845[] = "bufname, attrs",
 text846[] = "array, i[, len]",
 text847[] = "bufname, modes",
 text848[] = "op[, {arg | opts}]",
 text849[] = "old, new",
 text850[] = "bufname, file",
 text851[] = "[m]",
 text852[] = "bufname, fmt[, arg, ...]",
 text853[] = "mode[, attr, ...]",
 text854[] = "group[, attr, ...]",
 text855[] = "group[, bufname]",
 text856[] = "op, {key-lit | name}",
 text857[] = "[c]",
 text858[] = "{file | array}",
 text859[] = "{file | glob}[, opts]",
 text860[] = "class, {c | str}",
 text861[] = "[opts,] arg, ...",
 text862[] = "tab-size",
 text863[] = "attr[, bufname]",
 text864[] = "pair-no, fg-color, bg-color",
 text865[] = "type, colors",
 text866[] = "type, obj",
 text867[] = "bufname",
 text868[] = "file",
 text869[] = "file[, arg, ...]",
 text870[] = "file[, opts]",
 text871[] = "[name]",
 text872[] = "[name, ...]",
 text873[] = "pat[, opts]",
 text874[] = "ring",
 text875[] = "ring[, size]",
 text876[] = "ring[, name]",
 text877[] = "array, val, i, len",
 text878[] = "array, val, i",
 text879[] = "[opts,] {file | bufname}",
 text880[] = "[opts,] file1, file2",
 text881[] = "[opts,] {file | bufname}, new-file",
 text882[] = "param, val";

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
 options (in a pop-up window if default n).\n\nReturns: ~bselectBuf~B values."
#define CFLit_abs		"Return absolute value of N."
#define CFLit_aclone		"Return copy of given array."
#define CFLit_acompact		"Return copy of given array with nil elements removed (or if n argument, remove nil elements\
 from array and return it)."
#define CFLit_adelete		"Delete element(s) from given array.  If optional len argument is not specified, the element at\
 index i is removed and returned; otherwise, len elements beginning at index i are removed from the array and returned as a new\
 array.  If len is negative or greater than the number of elements following the index position, all elements from the index\
 position to the end of the array are selected.  In any case, if the index is negative, the referenced element is selected by\
 counting backward from the end of the array, where -1 is the last element; otherwise, the referenced element is selected from\
 the beginning of the array with the first being 0."
#define CFLit_adeleteif		"Delete all elements from given array that match expression value.  Value and array elements\
 may be any data types, including array.\n\nReturns: number of elements deleted."
#define CFLit_afill		"Replace len elements in given array beginning at index i with copies of val and return array. \
 If len is negative, all elements from the index position to the end of the array are replaced.  If len is non-negative but\
 greater than the number of elements following the index position, array is auto-extended to fill the referenced slice.  If the\
 index is negative, the starting element of the slice is selected by counting backward from the end of the array, where -1 is\
 the last element; otherwise, the starting element is selected from the beginning of the array with the first being 0. \
 Expression value and array elements may be any data types, including array."
#define CFLit_aincludeQ		"Return true if expression value matches any element in array, otherwise false.  Expression\
 value and array elements may be any data types, including array."
#define CFLit_aindex		"Return index of first element (or last element if n > 0) that matches expression value,\
 otherwise nil.  Array elements begin at index 0.  Expression value and array elements may be any data types, including array."
#define CFLit_ainsert		"Insert val into given array at index i and return array.  If the index is negative, the\
 insertion position is determined by counting backward from the end of the array (where -1 is the last element), and the value\
 is inserted after the referenced position.  If the index is non-negative, the position is relative to the beginning of the\
 array with the first being 0.  In this case, the value is inserted before the referenced position.  Expression value may be\
 any data type, including array."
#define CFLit_alias		"Create alias of command or function cf (and replace any existing alias if n > 0)."
#define CFLit_apop		"Remove last element from given array and return it, or nil if none left."
#define CFLit_appendFile	"Append current buffer to named file and set buffer's state to unchanged."
#define CFLit_apropos		"Generate list of commands, functions, aliases, and variables in a new buffer and render it per\
 ~bselectBuf~B options.  If default n, all system and user commands, system functions, user functions that have help text,\
 aliases, and variables are displayed in a pop-up window if their names match given pattern; otherwise, one or more of the\
 following comma-separated option(s) may be specified in string argument opts to control the selection\
 process:\n\tSystem\t\tSelect system commands and functions matching pattern.\n\tUser\t\tSelect user commands matching pattern\
 and user functions that match pattern and have help text.\n\tAll\t\t\tInclude user functions not having help text.\nIf\
 pattern is plain text, match is successful if name contains pat.  If pattern is nil or a null string, all names of type\
 selected by n argument and any specified options are listed.\n\nReturns: ~bselectBuf~B values."
#define CFLit_apush		"Append expression value to given array and return array."
#define CFLit_array		"Create array and return it, given optional size (default zero) and initial value of each\
 element (default nil).  Initializer value may be any data type, including array, and is copied into each element."
#define CFLit_ashift		"Remove first element from given array and return it, or nil if none left."
#define CFLit_aunshift		"Prepend expression value to given array and return array."
#define CFLit_backChar		"Move point backward [-]n characters (default 1).\n\nReturns: false if hit a buffer boundary,\
 otherwise true."
#define CFLit_backLine		"Move point backward [-]n lines (default 1).\n\nReturns: false if hit a buffer boundary,\
 otherwise true."
#define CFLit_backPage		"Scroll current window backward n pages (or half a page if n < 0, default 1)."
#define CFLit_backPageNext	"Scroll next window backward n pages (or half a page if n < 0, default 1)."
#define CFLit_backPagePrev	"Scroll previous window backward n pages (or half a page if n < 0, default 1)."
#define CFLit_backTab		"Move point backward [-]n tab stops (default 1)."
#define CFLit_backWord		"Move point backward [-]n words (default 1).\n\nReturns: false if hit a buffer boundary,\
 otherwise true."
#define CFLit_backspace		"Delete backward [-]n characters (or soft tabs if $softTabSize > 0, default 1)."
#define CFLit_basename		"Return filename component of given pathname (without extension if n <= 0)."
#define CFLit_beep		"Sound beeper n times (default 1)."
#define CFLit_beginBuf		"Move point to beginning of current buffer (or named buffer if n argument)."
#define CFLit_beginLine		"Move point to beginning of [-]nth line (default 1).\n\nReturns: false if hit a buffer\
 boundary, otherwise true."
#define CFLit_beginMacro	"Begin recording a macro.  [Interactive only]"
#define CFLit_beginText		"Move point to beginning of text on [-]nth line (default 1).\n\nReturns: false if hit a buffer\
 boundary, otherwise true."
#define CFLit_beginWhite	"Move point to beginning of white space at or immediately before point."
#define CFLit_bemptyQ		"Return true if current buffer is empty (or named buffer if n argument), otherwise false."
#define CFLit_bgets		"Return nth next line (default 1) without trailing newline from named buffer beginning at\
 point, and move point to beginning of next line.  If no lines left, nil is returned."
#define CFLit_binding		"Return key binding information for given op and argument.  Operation keywords are:\n\tName\t\t\
Return name of command bound to given key-lit binding, or nil if none.\n\tValidate\tReturn key-lit in standardized form, or\
 nil if it is invalid.\n\tKeyList\t\tReturn array of key binding(s) bound to given command name.\nCase of keywords is ignored."
#define CFLit_bindKey		"Bind named command to a key sequence (or single key if interactive and n <= 0)."
#define CFLit_textArgs		"  Argument(s) are converted to string and concatenated to form the text object."
#define CFLit_bprint		"Insert argument(s) into named buffer at point n times (default 1) and return string result,\
 with ~binsert~B options." CFLit_textArgs
#define CFLit_bprintf		"Insert string built from specified format string and argument(s) into named buffer at point n\
 times (default 1) and return string result, with ~binsert~B options."
#define CFLit_bufAttrQ		"Check if given attribute is set in current buffer (or named buffer if optional bufname\
 argument given) and return Boolean result.  Attributes are: \n\tActive\t\tFile was read.\n\tChanged\t\tBuffer is\
 changed.\n\tCommand\t\tUser command buffer.\n\tFunction\tUser function buffer.\n\tHidden\t\tBuffer is hidden from\
 view.\n\tNarrowed\tBuffer is in a narrowed state.\n\tReadOnly\tBuffer is read-only (may not be\
 modified).\n\tTermAttr\tTerminal attributes enabled in buffer."
#define CFLit_bufBoundQ		"Return true if point at beginning (n < 0), middle (n == 0), end (n > 0), or either end\
 (default) of current buffer, otherwise false."
#define CFLit_bufInfo		"Return information for named buffer or multiple buffers, subject to comma-separated option(s)\
 specified in optional string argument opts.  If opts not specified, an array is returned for named buffer, containing the\
 following elements:\n\t[0]\tbufname\t\t\t\tBuffer name.\n\t[1]\tfilename\t\t\tFilename associated with buffer, or nil if none.\
\n\t[2]\thome-dir\t\t\tHome directory associated with buffer, or nil if none.\n\t[3]\tbytes\t\t\t\tTotal number of bytes in\
 buffer.\n\t[4]\tlines\t\t\t\tTotal number of lines in buffer.\nAlternatively, if opts is specified and contains keyword\
 \"Extended\", the following additional elements are returned: \n\t[5]\t[buf-attr, ...]\t\tList of enabled attributes (as\
 described for the ~bbufAttr?~B function).\n\t[6]\t[buf-mode, ...]\t\tList of enabled buffer modes.\nIf bufname is nil,\
 multiple arrays are returned instead (or buffer names only, if \"Brief\" option is specified), nested within a single array. \
 One array or buffer name is returned for each buffer, which are selected by specifying one or more of the following attribute\
 keywords in opts:\n\tVisible\t\t\tSelect visible buffers.\n\tHidden\t\t\tSelect hidden buffers.\n\tCommand\t\t\tSelect user\
 command buffers.\n\tFunction\t\tSelect user function buffers.\nCase of option and attribute keywords is ignored."
#define CFLit_bufWind		"Return ordinal number of first window on current screen (other than current window if n\
 argument) displaying named buffer, or nil if none."
#define CFLit_chgBufAttr	"Toggle, set, or clear all of the comma-separated attribute(s) specified in string argument\
 attrs in named buffer per n argument:\n\tn < 0\t\tClear attribute(s).\n\tn == 0\t\tToggle attribute(s) (default).\n\tn ==\
 1\t\tSet attribute(s).\n\tn > 1\t\tClear all, then set attribute(s).\nAttributes are:\n\tChanged\t\tBuffer is\
 changed.\n\tHidden\t\tBuffer is hidden from view.\n\tReadOnly\tBuffer is read-only (may not be\
 modified).\n\tTermAttr\tTerminal attributes enabled in buffer.\nCase of attribute keywords is ignored.  If n > 1, attrs may\
 be nil, otherwise at least one attribute must be specified.  If interactive and default n, one attribute is toggled in current\
 buffer.\n\nReturns: former state (-1 or 1) of last attribute changed or zero if n > 1 and no attributes were specified.  (If\
 n is -1 or 1 and one attribute is specified, return value may be used to restore former state of the attribute by using it as\
 the n argument of a subsequent ~bchgBufAttr~B command.)"
#define CFLit_chgDir		"Change working directory.\n\nReturns: absolute pathname of new directory."
#define CFLit_chgMode		"Toggle, enable, or disable buffer or global mode(s) per n argument:\n\tn < 0\t\t\
Disable mode(s).\n\tn == 0\t\tToggle mode(s) (default).\n\tn == 1\t\tEnable mode(s).\n\tn > 1\t\tDisable all of type being\
 processed, then enable zero or more mode(s) of same type.\nIf bufname is nil, global mode(s) are processed, otherwise buffer\
 mode(s).  If n > 1, modes argument may be nil; otherwise, it must be either a string containing one or more comma-separated\
 mode names or an array of mode names.  Case of mode name(s) is ignored.  If interactive and default n, one buffer mode in\
 current buffer or one global mode is toggled.\n\nReturns: former state (-1 or 1) of last mode changed, or zero if n > 1 and no\
 modes were specified.  (If n is -1 or 1 and one mode is specified, return value may be used to restore former state of the\
 mode by using it as the n argument of a subsequent ~bchgMode~B command.)"
#define CFLit_chr		"Return 8-bit (integer) character N in string form."
#define CFLit_clearBuf		"Clear current buffer (or named buffer if n argument, ignoring changes if n < 0).\n\nReturns:\
 false if buffer not cleared, otherwise true."
#define CFLit_clearHook		"Clear named hook(s) (or clear all hooks if n <= 0)."
#define CFLit_clearMsgLine	"Clear the message line."
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
#define CFLit_definedQ		"Check if an object of a given type is defined and return\
 string or Boolean result if found, otherwise nil.  Type keywords are as follows:\
\n\tName\t\tCheck if string object is an identifier name (such as a command or variable).\
\n\tMark\t\tCheck if ASCII character object is a mark defined anywhere in current buffer.\
\n\tActiveMark\tCheck if ASCII character object is a mark defined in visible portion of narrowed buffer.\
\n\tMode\t\tCheck if string object is a defined mode name.\
\n\tModeGroup\tCheck if string object is a defined mode group name.\
\nCase of keywords is ignored.  If type is \"Name\", a string phrase describing the object is returned, as follows:\
\n\talias\t\t\t\tAlias name.\
\n\tbuffer\t\t\t\tBuffer name.\
\n\tpseudo command\t\tSpecial \"command\" name, such as ~buniversalArg~B.\
\n\tprefix key\t\t\tPrefix key command name, such as ~bmetaPrefix~B.\
\n\tsystem command\t\tSystem command name.\
\n\tsystem function\t\tSystem function name.\
\n\tuser command\t\tUser command name.\
\n\tuser function\t\tUser function name.\
\n\tvariable\t\t\tVariable name.\
\nIf type is \"Mode\", a string keyword is returned, as follows:\
\n\tbuffer\t\t\t\tBuffer mode name.\
\n\tglobal\t\t\t\tGlobal mode name.\
\nIf type is \"Mark\", \"ActiveMark\", or \"ModeGroup\", a Boolean result is returned."
#define CFLit_delAlias		"Delete named alias(es).\n\nReturns: zero if failure, otherwise number of aliases deleted."
#define CFLit_delBackChar	"Delete backward [-]n characters (default 1)."
#define CFLit_delBackTab	"Delete backward [-]n hard tabs (or soft tabs if $softTabSize > 0, default 1)."
#define CFLit_delBlankLines	"Delete all blank lines above and below point, or immediately below it (or above it if n < 0)\
 if current line not blank."
#define CFLit_delBuf		"Delete named buffer(s) with confirmation if changed (or ignore changes if n < 0). \
 Alternatively if n > 0, delete multiple buffers as selected by the comma-separated option(s) specified in string argument\
 opts, as follows:\n\tOne (only) of the following options must be specified:\n\t\tVisible\t\tSelect all visible, non-displayed\
 buffers.\n\t\tUnchanged\tSelect all unchanged, visible, non-displayed buffers.\n\t\tHomed\t\tSelect all visible, non-displayed\
 buffers \"homed\" to current screen.\n\t\tInactive\tSelect all inactive, visible, non-displayed buffers.\n\tZero or more of\
 the following options may also be specified:\n\t\tDisplayed\tInclude displayed buffers in selection.\n\t\tHidden\t\tInclude\
 hidden buffers in selection.\n\t\tConfirm\t\tAsk for confirmation on every buffer.\n\t\tForce\t\tDelete all selected buffers,\
 ignoring changes, with initial confirmation.\nCase of option keywords is ignored.  \"Confirm\" and \"Force\" may not both be\
 specified.  If neither is specified, buffers are deleted with confirmation on changed ones only.  User command and function\
 buffers are skipped unconditionally.\n\nReturns: zero if failure, otherwise number of buffers deleted."
#define CFLit_delFencedRegion	"Delete text from point to matching ( ), [ ], { }, or < > fence character."
#define CFLit_delFile		"Delete a file on disk.  If n argument, one or both of the following comma-separated options\
 may be specified in string argument opts; otherwise, the first argument must be omitted:\n\
\tWithBuf\t\t\tDelete named buffer and its associated file.\n\
\tIgnoreErr\t\tIgnore any file-deletion error.\n\
Case of option keywords is ignored."
#define CFLit_delForwChar	"Delete forward [-]n characters (default 1)."
#define CFLit_delForwTab	"Delete forward [-]n hard tabs (or soft tabs if $softTabSize > 0, default 1)."
#define CFLit_delLine		"Delete [-]n lines (default 1, region lines if n == 0)."
#define CFLit_delMark		"Delete mark m (or all marks if n argument).  No error if script mode and mark does not exist."
#define CFLit_delRegion		"Delete region text."
#define CFLit_delRingEntry	"Delete entri(es) from given ring.  If macro ring and default n, delete named macro from the\
 ring; if not macro ring or not default n, delete the most recent n entries (default 1) from given ring (or entry number n if\
 n < 0, all entries if n == 0).  If deletion(s) were from search or replace ring, the most recent remaining entry is\
 subsequently set as the current search or replacement pattern."
#define CFLit_delRoutine	"Delete named user command(s) and/or function(s).\n\nReturns: zero if failure, otherwise number\
 of routines deleted."
#define CFLit_delScreen		"Delete screen N (and switch to first screen if N is current screen).  If n < 0, also delete\
 all buffers not being displayed that are \"homed\" to screen being deleted."
#define CFLit_delToBreak	"Delete from point through [-]n lines (default 1), not including delimiter of last line.  If\
 n == 0, delete from point to beginning of current line.  If n == 1 and point is at end of line, delete delimiter only."
#define CFLit_delWhite		"Delete white space (and non-word characters if n > 0) surrounding or immediately before point."
#define CFLit_delWind		"Delete current window and join upward.  Alternatively if n argument, one or more of the\
 following comma-separated option(s) may be specified in string argument opts to control the deletion\
 process:\n\tJoinDown\t\tJoin downward instead of upward.\n\tForceJoin\t\tForce \"wrap around\" when\
 joining.\n\tDelBuf\t\t\tDelete buffer after join."
#define CFLit_delWord		"Delete [-]n words (default 1, without trailing white space if n == 0).  Non-word characters\
 between point and first word are included."
#define CFLit_detabLine		"Change hard tabs to spaces in [-]n lines (default 1, region lines if n == 0) given tab size,\
 and mark line block as new region.  If tab size is nil, $hardTabSize is used."
#define CFLit_dirname		"Return directory component of given pathname (\"\" if none, \".\" if none and n argument)."
#define CFLit_dupLine		"Duplicate [-]n lines (default 1, region lines if n == 0) and move point to beginning of text\
 of duplicated block."
#define CFLit_editMode		"Create, delete, or edit a mode, given name and zero or more attributes to set or change.  If\
 mode does not exist, it is created if n > 0; otherwise, an error is returned.  If n <= 0, mode is deleted and no attribute\
 arguments are allowed; otherwise, zero or more may be specified.  Attribute arguments are strings of form \"keyword: value\",\
 as follows:\n\tGlobal: bool\t\tMake mode global scope if true, otherwise buffer scope.\n\tBuffer: bool\t\tMake mode buffer\
 scope if true, otherwise global scope.\n\tVisible: bool\t\tMake mode visible if true, otherwise hidden.\n\tHidden:\
 bool\t\tMake mode hidden if true, otherwise visible.\n\tDescription: text\tSet mode description to specified text.\n\tGroup:\
 name\t\t\tMake mode member of named group.\n\tGroup: nil\t\t\tRemove mode from all groups.\nExamples: \"Visible: false\" or\
 \"Group: ProgLang\".  Defaults for a new mode are \"Buffer: true\", \"Visible: true\".  Case of keywords and extraneous white\
 space are ignored."
#define CFLit_editModeGroup	"Create, delete, or edit a mode group, given name and zero or more attributes to set or change.\
  If group does not exist, it is created if n > 0; otherwise, an error is returned.  If n <= 0, group is deleted and no\
 attribute arguments are allowed; otherwise, zero or more may be specified.  Attribute arguments are strings of form \"keyword:\
 value\", as follows:\n\tDescription: text\tSet mode group description to specified text.\n\tModes: list\t\t\tAdd specified\
 comma-separated list of mode names to mode group; for example, \"Modes: C, Ruby\".\n\tModes: nil\t\t\tRemove all modes from\
 group.\nCase of keywords and extraneous white space are ignored."
#define CFLit_emptyQ		"Return true if expression evaluates to nil, an ASCII null character, a null string, or empty\
 array, otherwise false."
#define CFLit_endBuf		"Move point to end of current buffer (or named buffer if n argument)."
#define CFLit_endLine		"Move point to end of [-]nth line (default 1).\n\nReturns: false if hit a buffer boundary,\
 otherwise true."
#define CFLit_endMacro		"End recording a macro.  [Interactive only]"
#define CFLit_endWhite		"Move point to end of white space at point."
#define CFLit_endWord		"Move point to end of [-]nth word (default 1).\n\nReturns: false if hit a buffer boundary,\
 otherwise true."
#define CFLit_entabLine		"Change spaces to hard tabs in [-]n lines (default 1, region lines if n == 0) given tab size,\
 and mark line block as new region.  If tab size is nil, $hardTabSize is used."
#define CFLit_env	"Return value of given environmental variable.  If variable does not exist, a null string is returned."
#define CFLit_eval		"Concatenate argument(s) (or prompt for string if interactive) and execute result as an\
 expression.\n\nReturns: result of evaluation."
#define CFLit_exit		"Exit editor with optional message (and ignore changes if n != 0, force return code 1 if\
 n < 0).  Argument(s) are converted to string and concatenated to form the message."
#define CFLit_expandPath	"Return given pathname with \"~~/\", \"~~user/\", \"$var\", and \"${var}\" expanded in same\
 manner as a shell command line argument."
#define CFLit_findFile		"Find named file and read (if n != 0) into a buffer with ~bselectBuf~B options (without\
 auto-completion if interactive and n == 0 or n == 1).  If a buffer already exists with the same filename, its buffer is\
 selected; otherwise, a buffer is created.\n\nReturns: ~bselectBuf~B values."
#define CFLit_forwChar		"Move point forward [-]n characters (default 1).\n\nReturns: false if hit a buffer boundary,\
 otherwise true."
#define CFLit_forwLine		"Move point forward [-]n lines (default 1).\n\nReturns: false if hit a buffer boundary,\
 otherwise true."
#define CFLit_forwPage		"Scroll current window forward n pages (or half a page if n < 0, default 1)."
#define CFLit_forwPageNext	"Scroll next window forward n pages (or half a page if n < 0, default 1)."
#define CFLit_forwPagePrev	"Scroll previous window forward n pages (or half a page if n < 0, default 1)."
#define CFLit_forwTab		"Move point forward [-]n tab stops (default 1)."
#define CFLit_forwWord		"Move point forward [-]n words (default 1).\n\nReturns: false if hit a buffer boundary,\
 otherwise true."
#define CFLit_getInfo		"Return informational item for given type keyword:\n\
\tColors\t\tList of display item colors in form [[item-kw, [fg-color, bg-color]], ...] where color pair array is nil if color\
 not set for item (or number of colors and color pairs available in form [colors, pairs] if n argument).\n\
\tDefaults\tList of default screen parameters in form [[param-name, value], ...] where value is -1 if no value set.\n\
\tEditor\t\tEditor name.\n\
\tHooks\t\tHook list in form [[hook-name, func-name], ...] where func-name is nil if hook not set.\n\
\tLanguage\tLanguage of text messages.\n\
\tModes\t\tMode list in form [[mode-name, group-name, user?, global?, hidden?, scope-locked?, active?], ...] where group-name\
 is nil if mode is not in a group, and last five elements are Boolean values.\n\
\tOS\t\t\tOperating system name.\n\
\tScreens\t\tScreen list in form [[screen-num, wind-count, work-dir], ...].\n\
\tVersion\t\tEditor version.\n\
\tWindows\t\tList of windows in current screen in form [[wind-num, bufname], ...] (or all screens in form [[screen-num,\
 wind-num, bufname], ...] if n argument).\n\
Case of keywords is ignored."
#define CFLit_getKey		"Get one keystroke interactively and return it as a key literal.  Alternatively, one or more of\
 the following comma-separated option(s) may be specified in optional string argument opts to determine the type of input and\
 return value:\n\tChar\t\t\tReturn ASCII (integer) character instead of key literal.\n\tKeySeq\t\t\tGet a key sequence instead\
 of one keystroke.\n\tLitAbort\t\tProcess abort key literally.\nCase of option keywords is ignored.  If \"LitAbort\" not\
 specified, function is aborted if abort key is pressed."
#define CFLit_getWord		"Get previous (n < 0), current (n == 0, default), or next (n > 0) word from current line and\
 return it.  If indicated word does not exist or n == 0 and point is not in a word, nil is returned.  Point is left at the\
 beginning of the word if n < 0 and at the end if n >= 0.  When going backward, point movement mimics the ~bbackWord~B command\
 and therefore, the \"previous\" word is the current word if point is in a word at the start and past its first character. \
 Additionally, lines are spanned during search if n < -1 or n > 1."
#define CFLit_glob		"Return a (possibly empty) array of file(s) and/or director(ies) that match given shell glob\
 pattern."
#define CFLit_gotoFence		"Move point to matching ( ), [ ], { }, or < > fence character from one at point (or one given\
 as an argument if n argument).\n\nReturns: true if matching fence found, otherwise false."
#define CFLit_gotoLine	"Move point to beginning of line N (or to end of buffer if N == 0, in named buffer if n argument)."
#define CFLit_gotoMark		"Move point to mark m and restore its window framing if mark is outside of current window (or\
 force reframing if n >= 0).  If n <= 0, also delete mark after point is moved."
#define CFLit_groupModeQ	"Check if any mode in given group is enabled and return name of first one found if so,\
 otherwise nil.  If group contains buffer modes, they are checked in current buffer (or named buffer if optional bufname\
 argument given).  If group does not exist, nil is returned unless n > 0, in which case it is an error.  Case of group name is\
 ignored."
#define CFLit_growWind		"Increase size of current window by abs(n) lines (default 1 at bottom, at top if n < 0)."
#define CFLit_searchReturn	"\n\nReturns: string found, or false if not found."
#define CFLit_restrictBack	"  If n == 0, restrict search to text between point and beginning of current\
 line.  If n < 0, restrict search to text between point and beginning of abs(nth) previous line."
#define CFLit_restrictForw	" restrict search to text between point and end of abs(nth) next line (where current line is n\
 == 1), not including delimiter of last line."
#define CFLit_huntBack		"Repeat most recent search backward n times if n > 0, otherwise once." CFLit_restrictBack\
 CFLit_searchReturn
#define CFLit_huntForw		"Repeat most recent search forward n times if n >= 0, otherwise once.  If n < 0,"\
 CFLit_restrictForw CFLit_searchReturn
#define CFLit_indentRegion	"Indent lines in region by n tab stops (default 1) and mark line block as new region."
#define CFLit_index		"Return position of pattern pat in given string, or nil if not found.  Alternatively, one or\
 both of the following comma-separated option(s) may be specified in optional string argument opts to select the type of search\
 operation:\n\tChar\tReturn position of character c instead of pattern pat.\n\tLast\tFind last (rightmost) occurrence instead\
 of first.\nCase of option keywords is ignored.  Beginning of string is position 0."
#define CFLit_insertText	" argument(s) into current buffer at point n times (default 1) and return string result\
 (or insert once without moving point if n == 0, with literal newline if n < 0)."
#define CFLit_insert		"Insert" CFLit_insertText CFLit_textArgs
#define CFLit_insertBuf		"Insert named buffer at point (without moving point if n == 0) and mark as region."
#define CFLit_insertf		"Insert string built from specified format string and" CFLit_insertText
#define CFLit_insertFile	"Insert named file at point (without moving point if n == 0) and mark as region."
#define CFLit_inserti		"Format, insert at point, and add increment to i variable abs(n) times (default 1).  If n < 0,\
 point is moved back to original position after each iteration."
#define CFLit_insertPipe	"Execute shell pipeline and insert its output at point (without moving point if n == 0). \
 Argument(s) are converted to string and concatenated to form the command.\n\nReturns: false if failure, otherwise true."
#define CFLit_insertSpace	"Insert abs(n) spaces ahead of point (default 1, ignoring \"Over\" and \"Repl\" buffer modes if\
 n < 0)."
#define CFLit_interactiveQ	"Return true if current script is being executed interactively, otherwise false."
#define CFLit_isClassQ		"Return true if character c or every character in string str is in the given ASCII character\
 class, otherwise false.  The class argument is any of the following keywords:\n\
\talnum\t\tAlphabetic character or digit.\n\
\talpha\t\tAlphabetic character.\n\
\tblank\t\tSpace or tab.\n\
\tcntrl\t\tControl character: any ASCII character not in \"print\" class.\n\
\tdigit\t\tDecimal digit: any of \"0-9\".\n\
\tgraph\t\tPrinting character except space: any of \"!\"#$%&'()*+,-./0-9:;<=>?@A-Z[\\]^_`a-z{|}~~\".\n\
\tlower\t\tLower case letter.\n\
\tprint\t\tPrinting character, including space: any character in \"graph\" class or ?\\s.\n\
\tpunct\t\tPrinting character except space, digit, or letter.\n\
\tspace\t\tWhite space character: any of ?\\s, ?\\t, ?\\n, ?\\v, ?\\f, or ?\\r.\n\
\tupper\t\tUpper case letter.\n\
\tword\t\tLetter, digit, or underscore character: any of \"A-Za-z0-9_\".\n\
\txdigit\t\tHexadecimal digit: any of \"0-9A-Fa-f\"."
#define CFLit_join		"Join one or more values with given string delimiter and return string result (skipping nil\
 values if n <= 0).  All values are converted to string and each element of an array (processed recursively) is treated as a\
 separate value."
#define CFLit_joinLines	"Join [-]n lines (default -1, region lines if n == 0) into a single line, replacing intervening white\
 space with zero spaces (\"chars\" is nil), one space (\"chars\" is a null string), or one or two spaces (\"chars\" is a list\
 of one or more \"end of sentence\" characters).  In the last case, two spaces are inserted between lines if the first line\
 ends with any of the given characters and the second line is not blank, otherwise one.  When command is invoked interactively,\
 a null string is used for the \"chars\" argument."
#define CFLit_joinWind		"Join current window with upper window.  Alternatively if n argument, one or more of the\
 following comma-separated option(s) may be specified in string argument opts to control the join process:\n\tJoinDown\t\tJoin\
 with lower window (downward) instead of upper one.\n\tForceJoin\t\tForce \"wrap around\" when joining.\n\tDelBuf\t\t\tDelete\
 buffer in other window after join."
#define CFLit_keyPendingQ	"Return true if type-ahead key(s) pending, otherwise false."
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
#define CFLit_linkFile		"Create file2 as a link to file1.  If n argument, one or both of the following comma-separated options\
 may be specified in string argument opts; otherwise, the first argument must be omitted:\n\
\tSymLink\t\tCreate symbolic link instead of hard link.\n\
\tForce\t\tReplace file2 if it already exists.\n\
Case of option keywords is ignored."
#define CFLit_lowerCaseLine	"Change [-]n lines to lower case (default 1, region lines if n == 0) and mark line block as\
 new region."
#define CFLit_lowerCaseRegion	"Change all letters in region to lower case."
#define CFLit_lowerCaseStr	"Return string converted to lower case."
#define CFLit_lowerCaseWord	"Change n words to lower case (default 1)."
#define CFLit_manageMacro	"Perform macro operation per op keyword and return result.  Operation keywords are:\n\tSelect\t\
Search for named macro (specified in arg) and, if found, move it to top of macro ring, activate it, and return true;\
 otherwise, return false.\n\tSet\t\tSet given macro (specified in arg as an encoded string, or as [name, value]) and return\
 nil (or return error message if macro is a duplicate by name or value, force overwrite if n > 0).\n\tGet\t\tGet current\
 macro, with one or more comma-separated option(s) specified in string argument opts if n argument, and return it in encoded\
 string form, or nil if macro ring is empty.\n\"Get\" options are:\n\tSplit\tReturn current macro in form [name, value].\n\t\
All\t\tReturn all macros on macro ring in form [[name, value], ...]."
#define CFLit_markBuf	"Save point position in mark " WorkMarkStr " (or mark m if n argument) and mark whole buffer as region."
#define CFLit_match		"Return text matching captured group N from last =~~ expression or index() call using a regular\
 expression as the pattern (or from last buffer search if n argument).  N must be >= 0; group 0 is entire match.  If N is out\
 of bounds, nil is returned."
#define CFLit_message		"Set a return message and return true (default) or false.  The \"RtnMsg\" global mode is\
 ignored.  If n argument, one or more comma-separated options may be specified in string argument opts; otherwise, the first\
 argument must be omitted.  Argument(s) following optional first argument are converted to string and concatenated to form the\
 message.  By default, a message is set only if one has not already been set, and is wrapped in brackets (thus informational). \
 Options are:\n\tFail\t\tDo not wrap message in brackets and return false instead of true.\n\tForce\t\tAny existing message\
 is replaced unconditionally (forced).\n\tHigh\t\tMessage is considered high priority and will replace any existing message\
 that is not high priority and was not forced.\n\tNoWrap\t\tDo not wrap message in brackets.\n\tTermAttr\tEnable\
 terminal attributes in message.\nCase of option keywords is ignored.  If \"Force\" is specified and message is nil or a null\
 string, any existing message is cleared."
#define CFLit_metaPrefix	"Begin meta key sequence."
#define CFLit_modeQ		"Check if given buffer or global mode is enabled and return Boolean result.  If mode is a\
 buffer mode, it is checked in current buffer (or named buffer if optional bufname argument given).  If mode does not exist,\
 false is returned unless n > 0, in which case it is an error.  Case of mode name is ignored."
#define CFLit_moveWindDown	"Move window frame [-]n lines down (default 1)."
#define CFLit_moveWindUp	"Move window frame [-]n lines up (default 1)."
#define CFLit_narrowBuf	"Keep [-]n lines (default 1, region lines if n == 0) in current buffer and hide and preserve remainder."
#define CFLit_negativeArg	"Begin or continue entry of universal negative-trending n argument (in sequence -1, -2, ...)."
#define CFLit_newline		"Insert n lines (default 1, with auto-formatting if \"Wrap\" buffer mode enabled) or insert\
 abs(n) lines without formatting if n < 0."
#define CFLit_newlineI		"Insert n lines with same indentation as previous line (default 1)."
#define CFLit_pnBuf1		" visible buffer (or switch to "
#define CFLit_pnBuf2		" buffer having same home directory as current screen if n >= 0).  If n <= 0, also delete\
 buffer that was exited from.\n\nReturns: name of buffer switched to, or nil if no switch occurred."
#define CFLit_nextBuf		"Switch to next" CFLit_pnBuf1 "next" CFLit_pnBuf2
#define CFLit_nextScreen	"Switch to next screen n times (default 1)."
#define CFLit_nextWind		"Switch to next window n times (default 1)."
#define CFLit_nilQ		"Return true if expression value is nil, otherwise false."
#define CFLit_nullQ		"Return true if string is null (empty), otherwise false."
#define CFLit_numericQ	"Return true if string is a numeric literal which can be converted to an integer, otherwise false."
#define CFLit_onlyWind		"Make current window the only window on screen (delete all others)."
#define CFLit_openLine		"Open abs(n) lines ahead of point (default 1).  If n < 0, also move point to first empty line\
 opened if possible."
#define CFLit_openLineI	"Open a line before the [-]nth line (default 1) with same indentation as line preceding target line."
#define CFLit_ord		"Return first character of given string as a character value (8-bit integer)."
#define CFLit_outdentRegion	"Outdent lines in region by n tab stops (default 1) and mark line block as new region."
#define CFLit_over		"Overwrite text in current buffer beginning at point n times (default 1) with argument(s) and\
 return string result, with ~binsert~B options." CFLit_textArgs "  Buffer text is overwritten by the text object in the same\
 manner as if it were entered interactively in "
#define CFLit_overwriteChar	CFLit_over "\"Repl\" mode."
#define CFLit_overwriteCol	CFLit_over "\"Over\" mode."
#define CFLit_pathname		"Return absolute pathname of given file or directory, or an array of absolute pathnames if\
 argument is an array (of pathnames).  If n <= 0, symbolic links are not resolved."
#define CFLit_pause		"Pause execution for N seconds (or centiseconds if n argument)."
#define CFLit_pipeBuf		"Feed current buffer through shell pipeline and read its output back into buffer, with\
 ~breadFile~B options.  Argument(s) are converted to string and concatenated to form the command.\n\nReturns: false if failure,\
 otherwise ~bselectBuf~B values."
#define CFLit_popBuf		"Display named buffer in a pop-up window.  Additionally if n argument, one or more of the\
 following comma-separated option(s) may be specified in string argument opts to control the pop-up\
 operation:\n\tAltModeLine\t\tUse alternate mode line containing name of buffer being popped.\n\tDelete\t\t\tDelete buffer\
 after it is displayed.\n\tShift\t\t\tShift long lines left.\n\tTermAttr\t\tEnable terminal attributes.\nCase of option\
 keywords is ignored.\n\nReturns: name of buffer popped if not deleted afterward, otherwise nil."
#define CFLit_popFile		"Display named file in a pop-up window with ~bpopBuf~B options if n argument, plus additional\
 option:\n\tExisting\t\tSelect existing buffer with same filename if possible.\nIf a buffer is created for the file, it is\
 deleted after the file is displayed.\n\nReturns: name of buffer popped if not deleted afterward, otherwise nil."
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
#define CFLit_prompt		"Get terminal input and return string, ASCII character, or nil result, given optional prompt\
 string, options' string, and default input value.  Nil may be specified for any argument and/or used as a placeholder so that\
 a subsequent argument may be specified.  By default, a string value is read from the message line and returned (without a\
 prompt if prmt not given or is nil); otherwise, prmt, def, and one or more comma-separated option(s) of form \"keyword\" or\
 \"keyword: value\" may be specified in opts to control the input process.  Options of the first form are:\n\tNoEcho\t\t\tDo\
 not echo key(s) that are typed.\n\tNoAuto\t\t\tSuppress auto-completion.\n\tTermAttr\t\tEnable terminal attributes in prompt\
 string.\nOptions of the second form are:\n\tDelim: lit\t\tSpecify an input delimiter lit (a key literal) to use instead of\
 the return key.\n\tType: t\t\t\tSpecify an input type t, which must be one of the following:\n\t\tBuffer\t\t\tBuffer name\
 completion.\n\t\tChar\t\t\tSingle character.\n\t\tDelRing\t\t\tDelete ring access.\n\t\tFile\t\t\tFilename\
 completion.\n\t\tKey\t\t\t\tOne key.\n\t\tKeySeq\t\t\tOne key sequence.\n\t\tKillRing\t\tKill ring\
 access.\n\t\tMacro\t\t\tMacro name completion and ring access.\n\t\tMode\t\t\tMode name completion.\n\t\tMutableVar\t\tMutable\
 variable name completion.\n\t\tReplaceRing\t\tReplace ring access.\n\t\tRingName\t\tRing name\
 completion.\n\t\tSearchRing\t\tSearch ring access.\n\t\tString\t\t\tString (default).\n\t\tVar\t\t\t\tVariable name completion\
 (mutable and immutable).\nIf \"Delim\" option is given, it must be the last option in opts, and all characters following it\
 are used as its value.\n\nIf a prompt string is not specified, input is read without a prompt; otherwise, the following steps\
 are taken to build the prompt string that is displayed:\n\t 1.\tIf the prompt string begins with a letter, a colon and a space\
 (\": \") are appended to the string; otherwise, it is used as is, however any leading ' or \" quote character is stripped off\
 first (which force the string to be used as is) and any trailing space characters are moved to the very end of the final\
 prompt string that is built.\n\t 2.\tIf a non-empty default input value is specified, it is displayed after the prompt string\
 between square brackets [ ] if the input type is \"Char\"; otherwise, it is stored in the input buffer as the initial\
 value.\nA key literal is returned for types \"Key\" and \"KeySeq\"; otherwise, if the input buffer is empty and the user\
 presses the delimiter key, the default value (if specified) is returned as an ASCII character if input type is \"Char\";\
 otherwise, nil is returned.  If the user presses ~uC-SPC~U at any time, a null string is returned instead (or null character\
 ~u?\\0~U if the input type is \"Char\"); otherwise, the string in the input buffer is returned when the delimiter key is\
 pressed."
#define CFLit_replaceN		" beginning at point.  If default n, replace all occurrences; if n >= 0, replace n occurrences;\
 if n < 0, also replace all occurrences but"
#define CFLit_searchPat		"\n\nThe search pattern is as described under ~bsearchForw~B."
#define CFLit_replacePat	"\n\nThe replacement pattern is a string which may contain references of form '\\~un~U' (where\
 ~un~U is a group number) to substring(s) in the text that were matched by the search pattern.  \\0 is the text that matched\
 the entire search pattern and can be used for both regular expression and plain text search patterns.  \\1, \\2, ... are used\
 for regular expression patterns only, and refer to the text that matched the nth group.  References may occur multiple times\
 in the replacement pattern and in any order.\n\n"
#define CFLit_queryReplace	"Replace zero or more occurrences of search pattern spat with replacement pattern rpat\
 interactively," CFLit_replaceN CFLit_restrictForw CFLit_searchPat CFLit_replacePat "Returns: false if search was stopped\
 prematurely by user, otherwise true."
#define CFLit_quickExit		"Save any changed buffers and exit editor.  If any changed buffer is not saved successfully,\
 exit is cancelled."
#define CFLit_quote		"Return expression value converted to a string form which can be converted back to the original\
 value via ~beval~B (and force through any array recursion if n > 0)."
#define CFLit_quoteChar		"Get one keystroke and insert it into the current buffer in raw form abs(n) times (default 1,\
 ignoring \"Over\" and \"Repl\" buffer modes if n < 0).  If a function key is entered, it is inserted as a key literal."
#define CFLit_rand		"Return pseudo-random integer in range 0 to N - 1.  If N <= 1, zero is returned."
#define CFLit_readFile		"Read named file into current buffer (or new buffer if n argument) with ~bselectBuf~B options. \
 Returns: ~bselectBuf~B values."
#define CFLit_readPipe		"Execute shell pipeline and read its output into current buffer, with ~breadFile~B options. \
 Argument(s) are converted to string and concatenated to form the command.\n\nReturns: false if failure, otherwise\
 ~bselectBuf~B values."
#define CFLit_reframeWind	"Reframe current window with point at [-]nth (n != 0) or center (n == 0, default) window line."
#define CFLit_renameBuf		"Rename buffer \"old\" to \"new\" (or if interactive and default n, rename current buffer). \
 Returns: new buffer name."
#define CFLit_renameFile	"Rename a file on disk.  If n argument, one or both of the following comma-separated option(s)\
 may be specified in string argument opts; otherwise, the first argument must be omitted:\n\
\tFromBuf\t\tUse file associated with named buffer given as first argument as the one to rename.\n\
\tDiskOnly\tIf \"FromBuf\" option specified, do not set the buffer's file to the new filename.\n\
Case of option keywords is ignored."
#define CFLit_renameMacro	"Rename macro \"old\" to \"new\".  The new name must not already be in use.  If n argument,\
 macro is moved to top of ring and loaded after it is renamed.\n\nReturns: new macro name."
#define CFLit_replace		"Replace zero or more occurrences of search pattern spat with replacement pattern rpat,"\
 CFLit_replaceN CFLit_restrictForw CFLit_searchPat CFLit_replacePat "Returns: false if n > 0 and fewer than n replacements were\
 made, otherwise true."
#define CFLit_resetTerm		"Update all screens and windows to match current terminal dimensions and redraw screen."
#define CFLit_resizeWind	"Change size of current window to abs(n) lines (growing or shrinking at bottom if n > 0, at top\
 if n < 0, or make all windows the same size if n == 0)."
#define CFLit_restoreBuf	"Switch to remembered buffer in current window and return its name."
#define CFLit_restoreScreen	"Switch to remembered screen and return its number."
#define CFLit_restoreWind	"Switch to remembered window on current screen and return its number."
#define CFLit_revertYank	"Revert text insertion from prior ~byank~B, ~byankCycle~B, ~bundelete~B, or ~bundeleteCycle~B\
 command."
#define CFLit_ringSize		"Get size information for given ring (or set maximum size to size if n argument).  If maximum\
 size of ring is zero, size is unlimited. \n\nReturns: two-element array containing the current number of entries in the ring\
 and the maximum size."
#define CFLit_run		"Execute named command or alias interactively with numeric prefix n.\n\nReturns: execution\
 result."
#define CFLit_saveBuf		"Remember current buffer."
#define CFLit_saveFile	"Save current buffer (if changed) to its associated file (or all changed buffers if n argument)."
#define CFLit_saveScreen	"Remember current screen."
#define CFLit_saveWind		"Remember current window."
#define CFLit_scratchBuf	"Create and switch to a buffer with unique name, with ~bselectBuf~B options.\n\nReturns:\
 ~bselectBuf~B values."
#define CFLit_searchBack	"Search backward for nth occurrence of a pattern if n > 0, otherwise first."\
 CFLit_restrictBack CFLit_searchPat CFLit_searchReturn
#define CFLit_searchForw	"Search forward for nth occurrence of a pattern if n >= 0, otherwise first.  If n < 0,"\
 CFLit_restrictForw "  Pattern may contain a suffix of form ':~bopts~B' for specifying options, where opts is one or more of\
 the following letters:\n\te\tExact matching.\n\ti\tIgnore case.\n\tf\tFuzzy (approximate) matching.\n\tm\tMulti-line regular\
 expression matching ('.' and '[^' match a newline).\n\tp\tInterpret pat as plain text.\n\tr\tInterpret pat as a regular\
 expression; for example, '\\<\\w+\\>:re'.\nIf f or m is specified, r is assumed.  An invalid suffix is assumed to be part of\
 the pattern.  Options override the \"Exact\" and \"Regexp\" modes.  " CFLit_searchReturn
#define CFLit_selectBuf		"Select named buffer and render it according to n argument:\n\
\tn < -2\t\tDisplay buffer in a different window and switch to that window.\n\
\tn == -2\t\tDisplay buffer in a different window but stay in current window.\n\
\tn == -1\t\tPop buffer.\n\
\tn == 0\t\tLeave buffer as is (create in background if it does not exist).\n\
\tn == 1\t\tSwitch to buffer in current window (default).\n\
\tn == 2\t\tDisplay buffer in a new window but stay in the current window.\n\
\tn > 2\t\tDisplay buffer in a new window and switch to that window.\n\
If buffer does not exist and pop is requested, an error is returned; otherwise, buffer is created (with user confirmation, if\
 interactive).  If n <= -2 and only one window exists, a window is created; otherwise, window is switched to one already\
 displaying the buffer, or buffer is displayed in the window immediately above the current window if possible.  Note that if n\
 >= 2, a window is always created for the buffer, but if n <= -2, an existing window is selected for the buffer, if\
 possible.\n\nReturns:\n\
\tn == -1\t\tName of buffer that was popped if not deleted afterward, otherwise nil.\n\
\tn == 0\t\tTwo-element array containing buffer name and Boolean value indicating whether the buffer was created.\n\
\tOther n\t\tFour-element array containing same first two elements as for n == 0 plus ordinal value of window displaying the\
 buffer and Boolean value indicating whether the window was created."
#define CFLit_selectLine	"Select text or line(s) around point, mark selected text as region, move point to beginning\
 of region, and return number of lines selected.  Text is selected per n argument (default zero) and N value, as follows:\n\
\tn <= 0\t\t(N is line selector) - Select [-]N lines (default 1, region lines if N == 0),\
 not including delimiter of last line if n < 0.\n\
\tn > 0\t\t(N is text selector) - Select text from point through [-]N lines (default 1), not including delimiter of last line. \
 Additionally, if N == 0, select text from point to beginning of current line;\
 if N == 1 and point is at end of line, select delimiter only."
#define CFLit_selectScreen	"Switch to screen N (or create screen and switch to it if N == 0)."
#define CFLit_selectWind	"Switch to window N (or nth from bottom if N < 0)."
#define CFLit_setBufFile	"Associate a file with named buffer (or current buffer if interactive and default n) and\
 derive new buffer name from it unless n <= 0 or filename is nil or null.  If filename is nil or null, buffer's filename is\
 cleared.\n\nReturns: two-element array containing new buffer name and new filename."
#define CFLit_setColorPair	"Assign a foreground and background color to color pair number pair-no (or next one available\
 if pair-no is zero).  Pair number can subsequently be used for ~un~U in a ~~~un~Uc terminal attribute\
 specification.\n\nReturns: pair number assigned."
#define CFLit_setDefault	"Set a default integer value to use for one of the following parameters when a screen is\
 created:\n\
\tHardTabSize\t\tHard tab size.\n\
\tSoftTabSize\t\tSoft tab size.\n\
\tWrapCol\t\t\tWrap column.\n\
Case of parameter names is ignored."
#define CFLit_setDispColor	"Set color for a display item, given item type and either a two-element array of foreground\
 and background color numbers, or nil for no color.  Case-insensitive type keywords are:\n\
\tInfo\t\tSet color for informational displays.\n\
\tModeLine\tSet color for mode lines.\n\
\tRecord\t\tSet color for macro recording indicator."
#define CFLit_setHook		"Set named hook to system or user function func."
#define CFLit_seti		"Set i variable, sprintf format string, and increment if script mode or interactive and default\
 n.  If interactive and n >= 0, set i variable to n and leave format string and increment unchanged; if n < 0, display\
 all parameters on message line and leave them unchanged."
#define CFLit_setMark		"Save point position and window framing in mark " RegionMarkStr " (or mark m if n argument)."
#define CFLit_setWrapCol	"Set wrap column to N if script mode or interactive and default n.  If n >= 0, current and\
 previous wrap columns are swapped; if interactive and n < 0 or new value is same as current value, current and previous wrap\
 columns are displayed on message line and values are left unchanged."
#define CFLit_shell		"Spawn an interactive shell session.\n\nReturns: false if failure, otherwise true."
#define CFLit_shellCmd		"Execute a shell command and display result in a pop-up window.  If n argument, one or more\
 comma-separated options may be specified in string argument opts; otherwise, the first argument must be omitted.  Argument(s)\
 following optional first argument are converted to string and concatenated to form the command.  Options are:\n\tNoHdr\t\tDo\
 not write command at top of result buffer.\n\tNoPop\t\tSkip pop-up window display.\n\tShift\t\tShift long lines left in pop-up\
 window.\nCase of option keywords is ignored.\n\nReturns: false if command fails, otherwise true."
#define CFLit_showAliases	"Generate list of aliases matching given pattern in a new buffer and render it per\
 ~bselectBuf~B options (in a pop-up window if default n).  If pattern is plain text, match is successful if name contains pat. \
 If pattern is nil or a null string, all aliases are listed.\n\nReturns: ~bselectBuf~B values."
#define CFLit_showBuffers	"Generate list of buffers in a new buffer and render it per ~bselectBuf~B options.  If default\
 n, all visible buffers are displayed in a pop-up window; otherwise, one or more of the following comma-separated option(s) may\
 be specified in string argument opts:\n\tVisible\t\tSelect visible buffers.\n\tHidden\t\tSelect hidden (non-\
command/function) buffers.\n\tCommand\t\tSelect user command buffers.\n\tFunction\tSelect user function buffers.\n\t\
HomeDir\t\tInclude buffer's home directory.\nThe first column in the list is buffer attribute and state information:\
 ~u~bA~Zctive, ~u~bH~Zidden, ~u~bU~Zser command/function, ~u~bP~Zreprocessed, ~u~bN~Zarrowed, ~u~bT~ZermAttr, ~u~bB~Zackground,\
 ~u~bR~ZeadOnly, and ~u~bC~Zhanged.\n\nReturns: ~bselectBuf~B values."
#define CFLit_showColors	"Generate a palette of available colors or defined color pairs and display it in a pop-up\
 window.  If n <= 0, display color pairs, otherwise color palette.  In the latter case, number of colors displayed is\
 dependent on terminal type and may be split into pages, selected by n: first page is displayed for n == 1 (default), second\
 page for n == 2, and so on until all colors shown."
#define CFLit_showCommands	"Generate list of system and user commands and their bindings in a new buffer and render it per\
 ~bselectBuf~B options.  If default n, system and user commands matching given pattern are displayed in a pop-up window;\
 otherwise, one or both of the following comma-separated option(s) may be specified in string argument opts:\n\tSystem\t\t\
Select system commands matching pattern.\n\tUser\t\tSelect user commands matching pattern.\nIf pattern is plain text,\
 match is successful if name contains pat.  If pattern is nil or a null string, all commands of type selected by n argument and\
 any specified options are listed.\n\nReturns: ~bselectBuf~B values."
#define CFLit_showDir		"Display working directory on message line.\n\nReturns: absolute pathname of current directory."
#define CFLit_showFence		"Find ( ), [ ], { }, or < > fence character matching one at point (or one given as an argument\
 if n argument) and briefly display the cursor there if it is in the current window."
#define CFLit_showFunctions	"Generate list of system and user functions in a new buffer and render it per ~bselectBuf~B\
 options.  If default n, system functions matching given pattern and user functions matching given pattern that also have help\
 text are displayed in a pop-up window; otherwise, one or more of the following comma-separated option(s) may be specified in\
 string argument opts:\n\tSystem\t\tSelect system functions matching pattern.\n\tUser\t\tSelect user functions that match\
 pattern and have help text.\n\tAll\t\t\tInclude user functions not having help text.\nIf pattern is plain text, match is\
 successful if name contains pat.  If pattern is nil or a null string, all functions of type selected by n argument and any\
 specified options are listed.\n\nReturns: ~bselectBuf~B values."
#define CFLit_showHooks		"Generate list of hooks in a new buffer and render it per ~bselectBuf~B options (in a pop-up\
 window if default n).\n\nReturns: ~bselectBuf~B values."
#define CFLit_showKey		"Display name of command bound to key sequence (or single key if n <= 0) in a pop-up window (or\
 on message line if n >= 0).  [Interactive only]"
#define CFLit_showMarks		"Generate list of buffer marks in a new buffer and render it per ~bselectBuf~B options (in a\
 pop-up window if default n).\n\nReturns: ~bselectBuf~B values."
#define CFLit_showModes		"Generate list of modes in a new buffer and render it per ~bselectBuf~B options (in a pop-up\
 window if default n).\n\nReturns: ~bselectBuf~B values."
#define CFLit_showPoint		"Display buffer position of point and current character information on message line (for\
 current line only if n argument).  [Interactive only]"
#if MMDebug & Debug_ShowRE
#define CFLit_showRegexp	"Generate list of search and replacement metacharacter arrays in a new buffer and render it per\
 ~bselectBuf~B options (in a pop-up window if default n).\n\nReturns: ~bselectBuf~B values."
#endif
#define CFLit_showRing		"Generate list of entries from given ring in a new buffer and render it per ~bselectBuf~B\
 options (in a pop-up window if default n).\n\nReturns: ~bselectBuf~B values."
#define CFLit_showScreens	"Generate list of screens and their associated windows and buffers in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).\n\nReturns: ~bselectBuf~B values."
#define CFLit_showVariables	"Generate list of system and user variables matching given pattern in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  If pattern is plain text, match is successful if name\
 contains pat.  If pattern is nil or a null string, all variables are listed.\n\nReturns: ~bselectBuf~B values."
#define CFLit_shQuote		"Return expression converted to string and enclosed in ' quotes.  Apostrophe ' characters are\
 converted to \\' so that the string is converted back to its orignal form if processed as a shell argument."
#define CFLit_shrinkWind	"Decrease size of current window by abs(n) lines (default 1 at bottom, at top if n < 0)."
#define CFLit_sortRegion	"Sort lines in region in ascending order and mark line block as new region.  Alternatively if n\
 argument, one or both of the following comma-separated option(s) may be specified in string argument opts to control the\
 sorting process:\n\tDescending\t\tSort in descending order.\n\tIgnore\t\t\tIgnore case in line comparisons.\nCase of option\
 keywords is ignored."
#define CFLit_space		"Insert n spaces (default 1, with auto-wrap if \"Wrap\" buffer mode enabled) or insert abs(n)\
 spaces without formatting if n < 0."
#define CFLit_split		"Split string str into an array and return it, using delimiter character dlm and optional\
 limit.\n\
\tPossible values of dlm are:\n\
\t\t?\\s\t\t\t\tOne or more contiguous white space characters in the string are treated as a single delimiter\
 and leading white space in the string is ignored.\n\
\t\t?\\0 or nil\t\tThe string is split into individual characters.\n\
\t\tOther char\t\tCharacter dlm is used as the delimiter.\n\
\tPossible values of limit are:\n\
\t\t< 0\t\t\t\tEach delimiter found is significant and delineates two substrings, either or both of which may be null.\n\
\t\t== 0 (default)\tTrailing null substrings are ignored.\n\
\t\t> 0\t\t\t\tA maximum of limit array elements are returned (and thus, the last element of the array will contain embedded\
 delimiter character(s) if the number of delimiters in the string is equal to or greater than limit).\n\
Note that if dlm is a null character (?\\0) or nil and limit <= 0, the limit value has no effect on the result.  Also, if n\
 argument given, white space is removed from each substring before it is appended to the array by passing n and the substring\
 to the ~bstrip~B function."
#define CFLit_splitWind		"Split current window into two.  Sizes and window selection are determined by n\
 argument:\n\tdefault\t\tSplit window into two of equal size and stay in current window.\n\tn <= -2\t\tSplit window just above\
 current line and move to other window.\n\tn == -1\t\tSplit window just above current line and stay in current window.\n\tn ==\
 0\t\tMove to other window after (default) split.\n\tn > 0\t\tSet size of upper window to n lines.\n\nReturns: ordinal number\
 of new window not containing point."
#define CFLit_sprintf		"Return string built from specified format string and argument(s)."
#define CFLit_statQ		"Test given file for specified type(s) and return Boolean result.  Types are:\n\
\t~u~bd~Zirectory\t\t\tsymbolic ~u~bL~Zink\t\t~u~bs~Zize > 0\n\
\t~u~be~Zxists\t\t\t\thard ~u~bl~Zink\t\t\t~u~bw~Zriteable\n\
\tregular ~u~bf~Zile\t\t~u~br~Zeadable\t\t\te~u~bx~Zecutable.\n\
If multiple letters are specified, true is returned if any type\
 matches (or if all types match if n argument)."
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
 space.  String may be in any form recognized by the ~bstrtol()~B Standard C Library function with a zero base."
#define CFLit_toStr		"Return expression value converted to string form, optionally with zero or more of the\
 following comma-separated option(s) specified in string argument opts to control the conversion process:\n\tShowNil\t\tConvert\
 nil value(s) to \"nil\".\n\tQuote1 or\n\t Quote2\t\tEnclose string values in ' or \" quotes, respectively.\n\tQuote\t\tQuote\
 strings automatically based on other options specified.\n\tVizChar or\n\t EscChar\tConvert invisible characters to visible or\
 escaped-character form, respectively.\n\tThouSep\t\tInsert commas in thousands' places in numbers.\nIn addition, the following\
 options may be specified for array expressions:\n\tSkipNil\t\tSkip nil arguments.\n\tDelim\t\tInsert comma delimiter between\
 elements.\n\tBrackets\tEnclose array elements in brackets [...].\nAlso, either of the following \"shortcut\" options may be\
 specified in lieu of (or in addition to) the options described above:\n\tLang\t\tUse \"language expression\" conversion\
 options \"ShowNil\", \"EscChar\", \"Quote\", \"Brackets\", and \"Delim\".\n\tVizStr\t\tUse \"human readable\" conversion\
 options \"ShowNil\", \"VizChar\", \"Quote1\", \"Brackets\", and \"Delim\".\nCase of option keywords is ignored.  An array-\
element delimiter may also be specified in optional string argument dlm (and opts may be nil in this case), which overrides the\
 \"Delim\" option."
#define CFLit_tr		"Return translated string.  Each \"from\" character in string is replaced with corresponding\
 \"to\" character.  If \"to\" string is shorter than \"from\" string, the last \"to\" character is used for each additional\
 \"from\" character.  If \"to\" string is nil or null, all \"from\" characters in string are deleted."
#define CFLit_traverseLine	"Move point forward or backward $travJump characters (or to far right if n == 0).  If n != 0,\
 first traversal is accelerated forward (n > 0) or backward (n < 0)."
#define CFLit_trimLine		"Trim white space from end of [-]n lines (default 1, region lines if n == 0) and mark line\
 block as new region."
#define CFLit_truncBuf		"Delete all text from point to end of buffer (or from point to beginning of buffer if n <= 0). \
 If interactive, user is prompted for confirmation unless n >= 2 or n <= -2."
#define CFLit_typeQ		"Return type of expression: \"array\", \"bool\", \"int\", \"nil\", or \"string\"."
#define CFLit_upperCaseLine	"Change [-]n lines to upper case (default 1, region lines if n == 0) and mark line block as\
 new region."
#define CFLit_upperCaseRegion	"Change all letters in region to upper case."
#define CFLit_upperCaseStr	"Return string converted to upper case."
#define CFLit_upperCaseWord	"Change n words to upper case (default 1)."
#define CFLit_unbindKey		"Unbind a key sequence (or single key if interactive and n <= 0).\n\nReturns: true if\
 successful, otherwise false."
#define CFLit_undelete		"Insert text from last \"delete\" command n times (default 1) and mark as region (or insert\
 once without moving point if n == 0, insert nth older delete once if n < 0)."
#define CFLit_undeleteCycle	"Replace undeleted text with next older ring entry (default) or cycle delete ring [-]n times\
 and use topmost entry (n != 0).  New text is marked as region.  If n == 0 or last command was not an ~bundelete~B or\
 ~bundeleteCycle~B, no action is taken."
#define CFLit_universalArg	"Begin or continue entry of universal positive-trending n argument (in sequence 2, 0, 3, 4,\
 ...)."
#define CFLit_updateScreen	"Update and redraw screen if altered and no keystrokes pending.  Additionally, one or both of\
 the following comma-separated option(s) may be specified in optional string argument opts to control the update\
 process:\n\tForce\t\tPending keystrokes are ignored.\n\tNoRedraw\tRedraw of physical screen is suppressed.\nCase of option\
 keywords is ignored."
#define CFLit_viewFile		"Find named file and read (if n != 0) into a read-only buffer with ~bselectBuf~B options\
 (without auto-completion if interactive and n == 0 or n == 1).  If a buffer already exists with the same filename, its buffer\
 is selected; otherwise, a buffer is created.\n\nReturns: ~bselectBuf~B values."
#define CFLit_widenBuf		"Expose all hidden lines in current buffer (unnarrow)."
#define CFLit_wrapLine		"Rewrap [-]n lines (default 1, region lines if n == 0) at $wrapCol, given a \"line prefix\"\
 string and \"end of sentence\" characters to pass to the ~bjoinLines~B command.  All lines in the block are filled and/or\
 wrapped so that they have the same indentation as the first line and do not extend past the wrap column.  Additionally, if\
 the line prefix is not nil or a null string (for example, \"# \" for a comment block) it is inserted after any indentation in\
 each line that is created during the wrapping process.  Existing prefix strings (with or without trailing white space) are\
 also recognized and removed from each line of the block before ~bjoinLines~B is called.  When command is invoked\
 interactively, a null string is used for the \"chars\" argument."
#define CFLit_wrapWord		"Move word at point to beginning of next line if word is past column n (default 0); otherwise,\
 move following word (if any) or start a new line."
#define CFLit_writeBuf		"Write lines in region (or characters in region if n >= 0) to named buffer at point.  If n <=\
 0, also delete text that was written.\n\nReturns: text written."
#define CFLit_writeFile		"Write current buffer to named file.  If default n or n > 0, also set buffer's filename to file\
 path, derive new buffer name from it, and set buffer's state to unchanged."
#define CFLit_xeqBuf		"Execute named buffer as a user command with numeric prefix n and optional\
 arguments.\n\nReturns: execution result."
#define CFLit_xeqFile		"Search for named file with ~bxPathname~B function and execute file that is found as a user\
 command with numeric prefix n and optional arguments.\n\nReturns: execution result."
#define CFLit_xeqMacro		"Execute current macro (or named macro if n <= 0) abs(n) times (default 1, with infinite\
 repetitions if n == 0).  Execution stops after n or $maxLoop iterations (if not zero), or if any invoked command or function\
 returns false."
#define CFLit_xPathname		"Search for given file or directory in $execPath directories and return\
 absolute pathname if found, otherwise nil.  Pathname is searched for verbatim first, then with \"" ScriptExt "\" appended\
 unless the filename already has that extension.  Alternatively, one or both of the following comma-separated option(s) may\
 be specified in optional string argument opts to control the search process:\n\tGlob\t\tProcess first argument as a shell glob\
 pattern instead of a file or directory, and return a (possibly empty) array of absolute pathnames that\
 match.\n\tSkipNull\tSkip null directories in $execPath (so that the current directory is not searched).\nCase of option\
 keywords is ignored.  If the given file or directory argument begins with a slash (/) and a glob search is being performed, an\
 error is returned (use ~bglob~B function instead); otherwise, it is searched for as an absolute pathname, bypassing the\
 directories in $execPath."
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
#define MLit_ReadOnly		"Open all files in read-only mode."
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
#define ALit_Author		"(c) Copyright 2022 Richard W. Marinelli <italian389@yahoo.com>"
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
#define VLit_ArgList		"Array containing user command or function arguments, if any, when a user routine (or buffer)\
 is being executed."
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
#define VLit_hardTabSize	"Size of a hard tab (character) in columns for current screen."
#define VLit_horzJump		"Percentage of horizontal window size to jump when scrolling (0-" JumpMaxStr ", 0 for smooth\
 scrolling, default " HorzJumpStr ")."
#define VLit_inpDelim		"Global input line delimiter(s).  Must be one or two characters, or a null string for \"auto\"."
#define VLit_lastKeySeq		"Last key sequence entered in string form."
#define VLit_lineChar		"Character at point as an ASCII (int) value."
#define VLit_lineCol		"Column number of point."
#define VLit_lineOffset		"Number of characters in line before point."
#define VLit_lineText		"Text of current line without newline delimiter."
#define VLit_maxCallDepth	"Maximum depth allowed in user command or function recursion (or unlimited if zero)."
#define VLit_maxLoop		"Maximum number of iterations allowed for a loop block or macro (or unlimited if zero)."
#define VLit_maxPromptPct	"Maximum percentage of terminal width to use for a prompt string (in range 15-90)."
#define VLit_otpDelim	"Global output line delimiter(s).  Must be one or two characters.  If null, $BufInpDelim is used."
#define VLit_pageOverlap	"Number of lines to overlap when paging."
#define VLit_replacePat		"Replacement pattern."
#define VLit_screenNum		"Ordinal number of current screen."
#define VLit_searchDelim	"Terminal input delimiter key for search and replacement patterns."
#define VLit_searchPat		"Search pattern."
#define VLit_softTabSize	"Size of a soft tab (spaces) in columns for current screen.  If greater than zero, soft tabs\
 are used, otherwise hard tabs."
#define VLit_travJump		"Number of characters to jump when traversing current line with ~btravLine~B command (minimum\
 4, maximum 25% of terminal width, default " TravJumpStr ")."
#define VLit_vertJump		"Percentage of vertical window size to jump when scrolling (0-" JumpMaxStr ", 0 for smooth\
 scrolling, default " VertJumpStr ")."
#define VLit_windLineNum	"Ordinal number of line containing point in current window."
#define VLit_windNum		"Ordinal number of current window."
#define VLit_windSize		"Size of current window in lines, not including mode line."
#define VLit_workDir		"Working directory of current screen."
#define VLit_wrapCol		"Word or text wrap column of current screen."

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
 text119[], text120[], text121[], text122[], text123[], text124[], text125[], text126[],text127[], text128[], text129[],
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
 text482[], text483[], text484[], text485[], text486[], text487[], text488[], text489[], text490[], text491[], text492[],
 text493[], text494[], text495[], text496[], text497[],	text498[];
extern const char
 text803[];
#if MMDebug & Debug_ShowRE
extern char
 text994[], text995[], text996[], text997[], text998[], text999[];
#endif
#endif
