# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# ppLib.ms		Ver. 9.6.0
#	Shared MightEMacs preprocessor library for C and C-like source-file editing.

# Don't load this file if it has already been done so.
if defined? 'Name', 'ppGotoIfEndif'
	return nil
endif

##### Commands and Functions #####

# Wrap "#if 0" and "#endif" around a block of lines.
command ppWrapIf0(0) {description: 'Wrap ~#u#if 0~U and ~u#endif~U around [-]n lines (default 1).'}

	$0 => langWrapPrep false, '#', '0', 'endif'
endroutine

# Wrap "#if 1" and "#endif" around a block of lines.
command ppWrapIf1(0) {description: 'Wrap ~#u#if 1~U and ~u#endif~U around [-]n lines (default 1).'}
	$0 => langWrapPrep false, '#', '1', 'endif'
endroutine

# Wrap "#if 0" and "#else" around a block of lines, duplicate them, and add "#endif".
command ppWrapIfElse(0) {description: 'Wrap ~#u#if 0~U and ~u#else~U around [-]n lines (default 1), duplicate them, and add\
 ~u#endif~U.'}
	$0 => langWrapIfElse '#'
endroutine

# Prompt for preprocessor macro name.
function ppNamePrompt(0)
	defined?('Name', '$cWrapName') && !null?($cWrapName) or $cWrapName = nil
	$cWrapName = prompt('Macro name', nil, $cWrapName)
endroutine

# Wrap "#if !<name>" and "#endif" around a block of lines.
command ppWrapIfName0(0) {description: 'Prompt for a preprocessor macro name and wrap ~#u#if !~UNAME and ~u#endif~U around [-]n\
 lines (default 1).'}
	ppNamePrompt
	empty?($cWrapName) || $0 => langWrapPrep false, '#', '!' & $cWrapName, 'endif'
endroutine

# Wrap "#if <name>" and "#endif" around a block of lines.
command ppWrapIfName1(0) {description: 'Prompt for a preprocessor macro name and wrap ~u#if~U NAME and ~u#endif~U around [-]n\
 lines\
 (default 1).'}
	ppNamePrompt
	empty?($cWrapName) || $0 => langWrapPrep false, '#', $cWrapName, 'endif'
endroutine

# Wrap "#if <name>" and "#else" around a block of lines, duplicate them, and add "#endif".
command ppWrapIfElseName(0) {description: 'Prompt for a preprocessor macro name, wrap ~u#if~U NAME and ~u#else~U around [-]n\
 lines (default 1),\
 duplicate them, and add ~u#endif~U.'}
	ppNamePrompt
	if !empty?($cWrapName)
		$0 => langWrapPrep true, '#', $cWrapName, 'else'
		setMark
		yank
		delRingEntry 'Kill'
		insert "#endif\n"
		swapMark ?.
		beginText
	endif
endroutine

# Wrap a list of keywords in a line block specified by n argument at $wrapCol.  It is assumed that keywords are separated by a
# comma and a space.
command ppWrapList(0) {description: 'Format [-]n lines (default 1) containing identifiers separated by commas.  All lines in\
 the block are rewrapped so that they have the same indentation as the first line of the block (if any) but do not extend past\
 the current wrap column (set in $wrapCol).'}
	if $wrapCol == 0
		return 1 => message 'Fail, High', 'Wrap column not set'
	endif
	if (endCol = $wrapCol - 2) < 0
		endCol = 0
	endif

	($0 == defn ? 1 : $0) => joinLines ''
	loop
		$lineCol = endCol
		if $lineChar == 012 || subline(1, 1) == "\n"
			2 => beginLine
			break
		endif
		forwChar
		while $lineChar != ?,
			backChar
			if $lineOffset == 0
				return 1 => message 'Fail, High', 'Cannot find comma in line block'
			endif
		endloop
		forwChar
		newlineI
		$lineChar == ?\s && delForwChar
	endloop
endroutine

# Go to matching #if... or #endif if current line begins with one of the two keywords.  Save current position in mark '.', set
# mark 'T' to #if... line, mark 'E' to #else (if it exists), and mark 'B' to #endif.  If $0 == 0, be silent about errors.
# Return true if successful, otherwise false.
command ppGotoIfEndif(0) {description: "Go to ~u#if...~U or ~u#endif~U paired with one in current line and set marks '.'\
 (previous position), 'T' (top macro line), 'E' (#else macro line, if any), and 'B' (bottom macro line).\n\nReturns: true if\
 successful, otherwise false."}

	# Current line begin with #if... or #endif?
	x = subline(-$lineOffset, 6)
	if substr(x, 0, 3) == '#if'
		isIf = true
	elsif x == '#endif'
		isIf = false
	else
		$0 == 0 || beep
		return false
	endif

	# if/endif found.  Prepare for forward or backward search.
	1 => setMark ?`
	beginLine
	if isIf
		alias _nextLine = forwLine
		this = '#if'
		thisLen = 3
		1 => setMark ?T
		delMark ?B
		that = '#endif'
		thatLen = 6
		thatMark = ?B
	else
		alias _nextLine = backLine
		this = '#endif'
		thisLen = 6
		1 => setMark ?B
		delMark ?T
		that = '#if'
		thatLen = 3
		thatMark = ?T
	endif

	level = 0
	foundElse = false
	found = true
	delMark ?E

	# Scan lines until other keyword found.
	loop
		if _nextLine() == false
			gotoMark ?.				# Other keyword not found!
			if $0 != 0
				beep
				1 => message 'Fail, High', that, ' not found'
			endif
			found = false
			break
		endif

		if subline(0, thatLen) == that
			if level == 0
				1 => setMark thatMark		# Found other keyword.
				break
			else
				--level
			endif
		elsif subline(0, thisLen) == this
			++level
		elsif subline(0, 5) == '#else' && level == 0
			foundElse = true
			1 => setMark ?E
		endif
	endloop

	# Clean up.
	delAlias _nextLine
	found
endroutine

# Remove conditional preprocessor lines from current buffer and return custom result message, given (1), Boolean "keep 'not'
# lines".  Called from ppDeletePPLines command.
function ppDeletePPLines1(1)
	ifCount = 0
	while huntForw						# Found #if line.
		isNot = (1 => match 1) == '!'			# Grab '!' if present.
		if 0 => ppGotoIfEndif				# Find #else (mark 'E') and #endif (mark 'B').
			gotoMark ?T				# Move to beginning of #if line.
			++ifCount

			# Delete #if block, if applicable.
			if $1 != isNot				# (Keeping "not" lines) != ('!' present)?
				setMark				# Yes, delete.
				if defined? 'ActiveMark', ?E	# Have #else?
					gotoMark ?B		# Yes, go to #endif ...
					delLine			# nuke it ...
					gotoMark ?E		# and go to #else.
				else
					gotoMark ?B		# No #else.  Go to #endif.
				endif
				0 => delLine			# Delete #if block.
			else					# Not deleting #if block.
				if defined? 'ActiveMark', ?E	# Have #else?
					gotoMark ?E		# Yes, go to #else ...
					setMark
					gotoMark ?B		# go to #endif ...
					0 => delLine		# and delete #else block.
				else				# No #else.
					gotoMark ?B		# Go to #endif ...
					delLine			# and delete it.
				endif
				gotoMark ?T			# Return to #if ...
				delLine				# and delete it.
			endif
		else
			# No matching #endif found.  Bail out.
			return 1 => message 'Fail, High', '#endif not found!'
		endif
	endloop

	# No (more) #if lines found in current buffer.
	saveBuf
	"#{ifCount} '#if' blocks resolved"
endroutine

# Scan multiple buffers (files) and remove conditional preprocessor lines.  #if lines must be in form "#if MACRO" or
# "#if !MACRO".
command ppDeletePPLines(0) {description: "Remove conditional preprocessor lines from all visible buffers, or just those that\
 matched the most recent ~bfindFiles~B invocation if n <= 0.  ~u#if~U lines must be in form ~u#if~U MACRO or ~#u#if !~UMACRO. \
 The user is prompted for the preprocessor macro name and given the option to continue or quit after each buffer is scanned.\
\n\nReturns: false if error occurs, otherwise true."}

	# Get list of buffers to search: either all visible buffers or only those also in $grepList.
	if (searchList = $0 => getBufList) == false
		return false
	endif
	whichBufs = ashift(searchList) ? 'all' : 'selected'
	searchList = ashift(searchList)

	# Prompt for Boolean-qualified macro name.
	macName = prompt("In #{whichBufs} buffers, keep lines for macro name (!NAME or NAME)")
	if empty?(macName) || macName == '!'
		return
	endif

	# Get ready for scan.
	searchPat = $searchPat
	if substr(macName, 0, 1) == '!'
		notMatch = true
		macPlain = substr(macName, 1)
	else
		notMatch = false
		macPlain = macName
	endif
	$searchPat = "^#if[ \\t]+(!?)#{macPlain}[ \\t]*$:re"

	# Process buffers.
	result = doBufList(searchList, 'ppDeletePPLines1', notMatch)

	# Clean up.
	restoreSearchRing searchPat
	result
endroutine
