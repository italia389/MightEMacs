# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# cToolbox.mm		Ver. 9.0.0
#	MightEMacs macros for C source-file editing.

# Don't load this file if it has already been done so.
if defined? 'cInit'
	return nil
endif

require 'toolbox'

# Set the following variable to a regular expression that will match the first line of a C function in a source file; that is,
# the line containing the name of the function.  The langFindRoutine macro will replace the '%NAME%' string with the name of the
# function being searched for each time the RE is used.  The langFindRoutine macro is called by the cFindFunc macro, which is
# bound to 'C-c C-f'.  The default RE here matches a line where the opening brace for the function is on the same line as the
# name.
$CFuncInfo = ['Function','^[\l_].+\b%NAME%\(.+{[ \t]*$:re','*.c',nil]

##### Macros #####

# Wrap "#if 0" and "#endif" around a block of lines.
macro cWrapIf0(0) {desc: 'Wrap ~#u#if 0~U and ~u#endif~U around [-]n lines (default 1).'}

	$0 => langWrapPrep false,'#','0','endif'
endmacro

# Wrap "#if 1" and "#endif" around a block of lines.
macro cWrapIf1(0) {desc: 'Wrap ~#u#if 1~U and ~u#endif~U around [-]n lines (default 1).'}
	$0 => langWrapPrep false,'#','1','endif'
endmacro

# Wrap "#if 0" and "#else" around a block of lines, duplicate them, and add "#endif".
macro cWrapIfElse(0) {desc: 'Wrap ~#u#if 0~U and ~u#else~U around [-]n lines (default 1), duplicate them, and add ~u#endif~U.'}
	$0 => langWrapIfElse '#'
endmacro

# Prompt for macro name.
macro cNamePrompt(0)
	defined?('$cWrapName') && !null?($cWrapName) or $cWrapName = nil
	$cWrapName = prompt 'Macro name',$cWrapName
endmacro

# Wrap "#if !<name>" and "#endif" around a block of lines.
macro cWrapIfName0(0) {desc: 'Prompt for a preprocessor macro name and wrap ~#u#if !~UNAME and ~u#endif~U around [-]n lines\
 (default 1).'}
	cNamePrompt
	empty?($cWrapName) || $0 => langWrapPrep false,'#','!' & $cWrapName,'endif'
endmacro

# Wrap "#if <name>" and "#endif" around a block of lines.
macro cWrapIfName1(0) {desc: 'Prompt for a preprocessor macro name and wrap ~u#if~U NAME and ~u#endif~U around [-]n lines\
 (default 1).'}
	cNamePrompt
	empty?($cWrapName) || $0 => langWrapPrep false,'#',$cWrapName,'endif'
endmacro

# Wrap "#if <name>" and "#else" around a block of lines, duplicate them, and add "#endif".
macro cWrapIfElseName(0) {desc: 'Prompt for a preprocessor macro name, wrap ~u#if~U NAME and ~u#else~U around [-]n lines\
 (default 1),\
 duplicate them, and add ~u#endif~U.'}
	cNamePrompt
	if !empty?($cWrapName)
		formerMsgState = -1 => alterGlobalMode 'Msg'
		$0 => langWrapPrep true,'#',$cWrapName,'else'
		setMark
		yank
		deleteKill
		insert "#endif\n"
		swapMark
		beginText
		formerMsgState => alterGlobalMode 'Msg'
	endif
endmacro

# Find first file that contains given function name and render it according to n argument if found.
macro cFindFunc(0) {desc: "Prompt for a directory to search and the name of a function and find first file matching\
 \"#{$CFuncInfo[2]}\" template that contains the function declaration (with ~bselectBuf~0 options)."}
	$0 => langFindRoutine '$CFuncInfo'
endmacro

# Wrap a list of keywords in a line block specified by n argument at $wrapCol.  It is assumed that keywords are separated by
# commas with no intervening spaces.
macro cWrapList(0) {desc: 'Format [-]n lines (default 1) containing identifiers separated by commas.  All lines in the block\
 are rewrapped so that they have the same indentation as the first line of the block (if any) but do not extend past the\
 current wrap column (set in $wrapCol).'}
	if $wrapCol == 0
		return failure 'Wrap column not set'
	endif
	if (endCol = $wrapCol - 2) < 0
		endCol = 0
	endif

	$0 => markBlock
	0 => joinLines nil
	loop
		$lineCol = endCol
		if $lineChar == "\n" || subline(1,1) == "\n"
			2 => beginLine
			break
		endif
		forwChar
		while $lineChar != ','
			backChar
			if $lineOffset == 0
				return failure 'Cannot find comma in line block'
			endif
		endloop
		forwChar
		newlineI
	endloop
endmacro

# Go to matching #if... or #endif if current line begins with one of the two keywords.  Save current position in mark '.', set
# mark 'T' to #if... line, mark 'E' to #else (if it exists), and mark 'B' to #endif.  If $0 == 0, be silent about errors.
# Return true if successful; otherwise, false.
macro cGotoIfEndif(0) {desc: "Go to ~u#if...~U or ~u#endif~U paired with one in current line and set marks '.' (previous\
 position), 'T' (top macro line), 'E' (#else macro line, if any), and 'B' (bottom macro line).  Returns: true if successful;\
 otherwise, false."}

	# Current line begin with #if... or #endif?
	x = subline -$lineOffset,6
	if substr(x,0,3) == '#if'
		isIf = true
	elsif x == '#endif'
		isIf = false
	else
		$0 == 0 || beep
		return false
	endif

	# if/endif found.  Prepare for forward or backward search.
	formerMsgState = -1 => alterGlobalMode 'Msg'
	-1 => setMark
	beginLine
	if isIf
		alias _nextLine = forwLine
		this = '#if'
		thisLen = 3
		1 => setMark 'T'
		deleteMark 'B'
		that = '#endif'
		thatLen = 6
		thatMark = 'B'
	else
		alias _nextLine = backLine
		this = '#endif'
		thisLen = 6
		1 => setMark 'B'
		deleteMark 'T'
		that = '#if'
		thatLen = 3
		thatMark = 'T'
	endif

	level = 0
	foundElse = false
	found = true
	deleteMark 'E'

	# Scan lines until other keyword found.
	loop
		if _nextLine() == false
			gotoMark '.'				# Other keyword not found!
			if $0 != 0
				beep
				-1 => success that,' not found'
			endif
			found = false
			break
		endif

		if subline(0,thatLen) == that
			if level == 0
				1 => setMark thatMark		# Found other keyword.
				break
			else
				--level
			endif
		elsif subline(0,thisLen) == this
			++level
		elsif subline(0,5) == '#else' && level == 0
			foundElse = true
			1 => setMark 'E'
		endif
	endloop

	# Clean up.
	deleteAlias _nextLine
	formerMsgState => alterGlobalMode 'Msg'
	found
endmacro

# Remove conditional preprocessor lines from current buffer and return custom result message, given (1), Boolean "keep 'not'
# lines".  Used by cNukePPLines macro.
macro cNukePPLines1(1)
	formerMsgState = -1 => alterGlobalMode 'Msg'
	ifCount = 0
	while huntForw						# Found #if line.
		isNot = (1 => match 1) == '!'			# Grab '!' if present.
		if 0 => cGotoIfEndif				# Find #else (mark 'E') and #endif (mark 'B').
			gotoMark 'T'				# Move to beginning of #if line.
			++ifCount

			# Delete #if block, if applicable.
			if $1 != isNot				# (Keeping "not" lines) != ('!' present)?
				setMark				# Yes, delete.
				if 1 => defined? 'E'		# Have #else?
					gotoMark 'B'		# Yes, go to #endif ...
					deleteLine		# nuke it ...
					gotoMark 'E'		# and go to #else.
				else
					gotoMark 'B'		# No #else.  Go to #endif.
				endif
				0 => deleteLine			# Delete #if block.
			else					# Not deleting #if block.
				if 1 => defined? 'E'		# Have #else?
					gotoMark 'E'		# Yes, go to #else ...
					setMark
					gotoMark 'B'		# go to #endif ...
					0 => deleteLine		# and delete #else block.
				else				# No #else.
					gotoMark 'B'		# Go to #endif ...
					deleteLine		# and delete it.
				endif
				gotoMark 'T'			# Return to #if ...
				deleteLine			# and delete it.
			endif
		else
			# No matching #endif found.  Bail out.
			formerMsgState => alterGlobalMode 'Msg'
			return failure '#endif not found!'
		endif
	endloop

	# No (more) #if lines found in current buffer.
	formerMsgState => alterGlobalMode 'Msg'
	saveBuf
	"#{ifCount} '#if' blocks resolved"
endmacro

# Scan multiple buffers (files) and remove conditional preprocessor lines.  #if lines must be in form "#if MACRO" or
# "#if !MACRO".
macro cNukePPLines(0) {desc: 'Remove conditional preprocessor lines from all visible buffers, or just those that matched the\
 most recent ~bgrepFiles~0 invocation if n <= 0.  ~u#if~U lines must be in form ~u#if~U MACRO or ~#u#if !~UMACRO.  The user is\
 prompted for the preprocessor macro name and given the option to continue or quit after each buffer is scanned.  Returns:\
 false if error occurs; otherwise, true.'}

	# Get list of buffers to search: either all visible buffers or only those also in $grepList.
	if (searchList = $0 => getBufList) == false
		return false
	endif
	whichBufs = shift(searchList) ? 'all' : 'selected'
	searchList = shift searchList

	# Prompt for Boolean-qualified macro name.
	macName = prompt "In #{whichBufs} buffers, keep lines for macro name (!NAME or NAME)"
	if empty?(macName) || macName == '!'
		return
	endif

	# Get ready for scan.
	search = $searchPat
	if substr(macName,0,1) == '!'
		notMatch = true
		macPlain = substr macName,1
	else
		notMatch = false
		macPlain = macName
	endif
	$searchPat = "^#if[ \\t]+(!?)#{macPlain}[ \\t]*$:re"

	# Process buffers.
	result = doBufList searchList,'cNukePPLines1',notMatch

	# Clean up.
	$searchPat = search
	result
endmacro

# Activate or deactivate tools, given Boolean argument.
macro cInit(1)

	# Proceed only if activation request and tools not active or deactivation request and tools active.
	if $1 != (binding('C-c C-g') == 'cGotoIfEndif')
		$wordChars = ''
		bindings = ["cFindFunc\tC-c C-f","cGotoIfEndif\tC-c C-g","cNukePPLines\tC-c #",\
		 "cWrapIf0\tM-0","cWrapIf1\tM-1","cWrapIfElse\tM-2","cWrapIfName0\tC-c 0",\
		 "cWrapIfName1\tC-c 1","cWrapIfElseName\tC-c 2","cWrapList\tC-c ,"]
		eval "tbInit 'C',bindings,$1"
	endif
endmacro

# Done!
true
