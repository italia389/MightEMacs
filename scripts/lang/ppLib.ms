# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# ppLib.ms		Ver. 10.0.0
#	Shared MightEMacs preprocessor library for C and C-like source-file editing.

# Don't load this file if it has already been done so.
if isExec? 'ppGotoIfEndif'
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
	isConstVar?('$ppMacroName') or $ppMacroName = nil
	$ppMacroName = prompt('Macro name', 'Type: SearchRing', $ppMacroName)
endroutine

# Wrap "#if !<name>" and "#endif" around a block of lines.
command ppWrapIfName0(0) {description: 'Prompt for a preprocessor macro name and wrap ~#u#if !~UNAME and ~u#endif~U around [-]n\
 lines (default 1).'}
	ppNamePrompt
	empty?($ppMacroName) || $0 => langWrapPrep false, '#', '!' & $ppMacroName, 'endif'
endroutine

# Wrap "#if <name>" and "#endif" around a block of lines.
command ppWrapIfName1(0) {description: 'Prompt for a preprocessor macro name and wrap ~u#if~U NAME and ~u#endif~U around [-]n\
 lines\
 (default 1).'}
	ppNamePrompt
	empty?($ppMacroName) || $0 => langWrapPrep false, '#', $ppMacroName, 'endif'
endroutine

# Wrap "#if <name>" and "#else" around a block of lines, duplicate them, and add "#endif".
command ppWrapIfElseName(0) {description: 'Prompt for a preprocessor macro name, wrap ~u#if~U NAME and ~u#else~U around [-]n\
 lines (default 1),\
 duplicate them, and add ~u#endif~U.'}
	ppNamePrompt
	if !empty?($ppMacroName)
		$0 => langWrapPrep true, '#', $ppMacroName, 'else'
		yank
		delRingEntry 'Kill'
		insert "#endif\n"
		-1 => gotoMark ?<
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

# Check current line for #if/#else/#endif at beginning and return 1/2/3 if found, otherwise zero.  Argument(s):
#  1: [Boolean] Set error message if keyword not found.
function ppHasIfEndif?(1)
	if beginText && $lineChar == ?#
		forwChar
		if (word = getWord)
			if word == 'if'
				return 1
			elsif word == 'endif'
				return 3
			elsif word == 'else'
				return 2
			endif
		endif
	endif
	$1 and 1 => message 'Fail, High', 'Not on #if/#else/#endif line'
	0
endroutine

# Go to matching #if... or #endif if current line begins with #if..., #else, or #endif, ignoring any leading white space, and
# set marks for current position and lines where keywords are found.  Return true if successful, otherwise false.
command ppGotoIfEndif(0) {description: "Go to line containing #if... or #endif paired with one on current line, or go to\
 matching #if... line if current line begins with #else.  Set mark ~u`~U to current position, mark ~u^~U to #if... line, mark\
 ~u:~U to #else line (if any), and mark ~u$~U to #endif line.  If n == 0, suppress error message if keyword not found on\
 current line.\n\n\
Returns: true if successful, otherwise false."}
	setMark ?`

	# Check current line.  Error if no valid keyword found.
	if !(origId = ppHasIfEndif?($0 != 0))
		return false
	endif

	# direc, startId, otherMark, otherId -- forward and backward.
	direcParams = [[2, 1, ?$, 3], [-1, 3, ?^, 1]]

	# Prepare for search(es).
	delMark ?:
	foundElse = foundOther = false
	beginText
	if (id = origId) == 1		# #if...
		setMark ?^
		delMark ?$
	elsif origId == 2		# #else
		setMark ?:
		foundElse = true
	else				# #endif
		setMark ?$
		delMark ?^
	endif

	loop
		direc, startId, otherMark, otherId = direcParams[id == 3 ? 1 : 0]
		level = 1

		# Scan lines until matching keyword found or hit buffer boundary.
		loop
			if !(direc => beginLine)

				# Hit buffer boundary... it's a bust.
				gotoMark ?`
				errorMsg = 'Matching "%s" keyword not found' % (otherId == 3 ? '#endif' : '#if')
				break 2
			endif

			if (id = ppHasIfEndif?(false))
				if id == otherId
					if --level == 0

						# Found other keyword at top level.
						break
					endif
				elsif id == startId
					++level
				elsif level == 1 && id == 2
					foundElse = true
					beginText
					setMark ?:
				endif
			endif
		endloop

		# Success.
		beginText
		setMark otherMark

		# If began on #else line and searched forward, go back to it and search backward for #if; otherwise, done.
		if origId == 2 && direc == 2
			gotoMark ?:
			id = 3
		else
			foundOther = true
			break
		endif
	endloop

	# Clean up.
	if foundOther
		1 => message('TermAttr', foundElse ? '#if/#else' : '#if', '/#endif found.  Mark ~u`~U set to previous position')
	else
		interactive? and beep
		1 => message 'Fail, High', errorMsg
	endif
endroutine

# Enumerate hexadecimal values in a block of #define lines of form "#define name 0x0001 ...".  Argument(s):
#  1: [Integer] Starting value.
command enumDefs(1) {arguments: 'start-val', description: 'Renumber hexadecimal values in a block of #define lines.  Each line\
 must be of form "#define name 0x0001 ...".'}

	# Get starting value and validate it.
	if !interactive?
		rawValue = $1
	elsif empty?(rawValue = prompt('Starting hex value', nil, 1))
		return nil
	endif
	if !numeric? rawValue, 16
		return 1 => message 'Fail, High', "Invalid hex number '", rawValue, "'"
	elsif (value = toint(rawValue, 16)) < 0
		return 1 => message 'Fail, High', 'Starting value (', rawValue, ') must be 0 or greater'
	endif

	# Get line count, position point, and remember search ring state.
	lineCt = selectText($0)
	searchPat = $searchPat

	# Break out of loop if parsing error.
	loop
		# Get "0[xX]" prefix, number of digits, and white space suffix (if any) from first (current) line.
		$searchPat = '\h(0[xX])([0-9a-fA-F]+)(\h+\H)?:re'
		if not -1 => huntForw
			break 1
		endif
		prefix = bufMatchGrp(1)
		numLen = length(bufMatchGrp 2)
		if !nil?(suffix = bufMatchGrp(3))
			suffix = suffix[0, -1]
		endif

		beginLine
		setMark ?`
		$searchPat = '\h0[xX][0-9a-fA-F]+:re'

		# Set format string.
		fmt = sprintf('%s%%.%ux', prefix, numLen)

		# Renumber lines.
		n = lineCt
		while n--
			if not -1 => huntForw
				break 2
			endif
			-1 => delWord
			insertf fmt, value
			if suffix != nil && subline(0) =~ '\h+\H:re'
				delWhite
				insert suffix
			endif

			2 => beginLine
			if value == 0
				value = 1
			else
				value <<= 1
			endif
		endloop

		# Done.
		restoreSearchRing searchPat
		return 1 => message 'TermAttr', lineCt, ' values renumbered, mark ~u`~U set to beginning of line block'
	endloop

	# Parsing error.
	restoreSearchRing searchPat
	1 => message 'Fail, High', 'Hexadecimal number not found on current line'
endroutine

# Remove conditional preprocessor lines from current buffer and return custom result message.  Argument(s):
#  1: [Boolean | nil] "Keep 'not' lines".
# If argument is nil, process "#if [01]" lines, otherwise "#if [!]NAME" lines.  Called from "ppDelPPLines" command.
function ppDelPPLines1(1)
	ifCount = 0
	while huntForw						# Find #if line.
		ifArg = bufMatchGrp(1)				# Grab '!' (if present), '0', or '1'.
		isNot = ifArg == '!'
		if 0 => ppGotoIfEndif				# Find #else (mark ':') and #endif (mark '$').
			gotoMark ?^				# Move to beginning of #if line.
			++ifCount

			# Delete #if block, if applicable.
			if nil?($1) && ifArg == '0' ||\
			 !nil?($1) && $1 != isNot		# #if 0 or (Keeping "not" lines) != ('!' present)?
				setMark				# Yes, delete.
				if isMark? ?:			# Have #else?
					gotoMark ?$		# Yes, go to #endif...
					delLine			# nuke it...
					gotoMark ?:		# and go to #else.
				else
					gotoMark ?$		# No #else.  Go to #endif.
				endif
				0 => delLine			# Delete #if block.
			else					# Not deleting #if block.
				if isMark? ?:			# Have #else?
					gotoMark ?:		# Yes, go to #else...
					setMark
					gotoMark ?$		# go to #endif...
					0 => delLine		# and delete #else block.
				else				# No #else.
					gotoMark ?$		# Go to #endif...
					delLine			# and delete it.
				endif
				gotoMark ?^			# Return to #if.
				delLine				# and delete it.
			endif
		else
			# No matching #endif found.  Bail out.
			return 1 => message 'Fail, High', '#endif not found!'
		endif
	endloop

	# No (more) #if lines found in current buffer.  Return status message.
	"#{ifCount} '#if' blocks resolved"
endroutine

# Scan multiple buffers (files) and remove conditional preprocessor lines.  #if lines must be in form "#if MACRO",
# "#if !MACRO", "#if 0", or "#if 1".
command ppDelPPLines(0) {description: "Remove conditional preprocessor lines from current buffer (default), all visible\
 buffers (n > 0), or just those that matched the most recent ~bfindFiles~B invocation (n <= 0).  ~u#if~U lines must be in form\
 ~u#if~U MACRO, ~#u#if !~UMACRO, ~u#if~U 0, or ~u#if~U 1.  The user is prompted for the preprocessor macro name (or \"1\") and\
 given the option to continue or quit after each buffer is scanned.\n\n\
Returns: false if error occurs, otherwise true."}

	# Get list of buffers to search: either current buffer, all visible buffers, or only those from findFiles call.
	if $0 == defn
		whichBufs = 'current buffer'
		searchList = [$bufname]
	else
		if !(searchList = $0 => getBufList)
			return false
		endif
		whichBufs = ashift(searchList) ? 'all buffers' : 'selected buffers'
		searchList = ashift(searchList)
	endif

	# Prompt for Boolean-qualified macro name.
	isConstVar?('$ppMacroName') or $ppMacroName = nil
	macName = prompt("In #{whichBufs}, keep lines for macro name (!NAME, NAME, or 1)", 'Type: SearchRing', $ppMacroName)
	if empty?(macName) || macName == '!'
		return
	endif
	$ppMacroName = macName

	# Get ready for scan.
	searchPat = $searchPat
	if macName == '1'
		notMatch = nil
		$searchPat = '^#if\h+([01])\h*$:re'
	else
		if ord(macName) == ?!
			notMatch = true
			macPlain = macName[1,]
		else
			notMatch = false
			macPlain = macName
		endif
		$searchPat = "^#if\\h+(!?)#{macPlain}\\h*$:re"
	endif

	# Process buffers.
	result = doBufList(searchList, 'ppDelPPLines1', notMatch)

	# Clean up.
	restoreSearchRing searchPat
	result
endroutine
