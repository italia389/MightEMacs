# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# lang.ms		Ver. 9.6.0
#	MightEMacs startup file for managing programming language libraries.

##### Initialization #####
1 => editMode 'RFIndt', 'Description: Use Ratliff-style indentation of right fence character.', 'Global: true', 'Hidden: true'

##### Global Constants #####

# Define array of programming language mode information.  Each element is a "descriptor" array read from an initialization file
# (for example, "rubyInit.ms") containing the following elements:
#   x -> 0: Header array:
#	x -> 0: "Lib" file was searched for and loaded if found (Boolean).
#	0 -> 1: Library name (if different from mode name).
#	1 -> 2: Mode name.
#	2 -> x: Mode description.
#	3 -> 3: Routine and library name prefix (usually mode name in lower case).
#	4 -> 4: Array of file extension(s), or nil if none.
#	5 -> 5: Array of Unix binary name(s), or nil if none.
#	6 -> 6: Fence mode to activate ("Fence1" or "Fence2").
#	x -> 7: Array of key bindings for library's commands, or nil if none.
#	x -> 8: Array of parameters for searching for a function or class name (or similar), or nil if none.
#		0: Array of "function search" parameters:
#			0: User prompt.
#			1: Regular expression that matches a function name for given language.
#			2: Glob pattern that matches source files to search.
#		1: Array of "class search" parameters:
#			0: User prompt.
#			1: Regular expression that matches a class name for given language.
#			2: Glob pattern that matches source files to search.
#		2: Directory to search (obtained from user).
#   8 -> 1: Array of "preKey" hook triggers:
#	0: Array of trigger characters for left justifying current line, or nil if none.
#	1: Array of trigger characters and/or strings for outdenting current line, or nil if none.
#	2: Array of trigger strings for same-denting current line, or nil if none.
#   9 -> 2: Array of "newline" triggers:
#	0. Array of trigger characters, strings, and/or keywords at EOL for indenting next line, or nil if none.
#	1. Array of trigger characters, strings, and/or keywords at EOL for outdenting next line, or nil if none.
#	2: Array of trigger strings and/or keywords at BOL for indenting next line, or nil if none.
#	3. Array of trigger keywords at BOL for outdenting next line, or nil if none.
# Notes:
#  1. Mode description (element 2) is not included in the array that is stored in $LangModes.
#  2. If a trigger array allows multiple types of values, all characters must be specified first, then strings, then keywords.
#  3. Items of type 'string' in the newline trigger arrays are considered keywords if they begin with a word character,
#     otherwise strings.
#  4. All trigger arrays except the first are converted to form [trigger-chars, [items]] to speed up processing at runtime,
#     where first element is all unique characters at beginning or end (as appropriate) of each string or keyword item in string
#     form.
#  5. No action is taken for any item in a "preKey" trigger array or any character in the second "newline" trigger array
#     (element 1, EOL outdenting) unless the item is preceded by white space in the buffer, and for a "preKey" trigger array,
#     also unless point is at end of line.  Hence, the entire buffer line minus any leading white space is checked (as a string)
#     against all non-character "preKey" items and therefore keywords (which begin and end on a word boundary) are not used.
#  6. Any character in the second "preKey" trigger array (element 1, outdenting) is assumed to be a right fence; that is, '}',
#     ']', or ')', and is outdented (or same-dented) to match the line containing the corresponding left fence.
#  7. Any string in the second "preKey" trigger array (element 1, outdenting) may be an array of form [string, keyword,...]
#     instead.  In this case, the string is ignored if the first word on the previous line matches any specified keyword.
#     For example, in Ruby mode, the first "when" clause should not be outdented past the "case" in the previous line, so
#     ['when', 'case'] would be used to prevent that.
#  8. Triggers at EOL take precedence over those at BOL.
#  9. When a library file for a language is loaded (for example, "cLib.ms"), it may contain an array of parameters used for
#     finding a function, method, or other syntactic element in a source file (or nil if none).  The array elements are as
#     follows:
#	0: Type of object being searched for; for example, "Function".  Used as prompt string (with " name" appended) for
#	   obtaining the object name from the user.
#	1: A regular expression that will match a line containing the object's declaration in a source file (for example, a C
#	   function or class definition in Ruby) with the string '%NAME%' embedded within it as a placeholder for the name.  The
#	   langFindObj function that is invoked will replace the '%NAME%' string with the name of the object being searched for
#	   each time the RE is used.  The langFindObj function is called by a stub command in the language's library file,
#	   which is usually bound to 'C-c C-f'.
#	2: A filename template matching the files to be searched (for example, '*.c' for C).
#	3: The last directory path entered by the user (initially nil).
$LangModes = []

##### Global Variables #####

# Define array of language parameters (constants and state information).
$langParams = [\
 'Lang',		/# 0: Mode group name for language modes #/\
 nil,			/# 1: Mode descriptor from $LangModes for active language mode in current buffer, or nil if none #/\
 nil			/# 2: Former state of "FenceN" global mode #/\
 ]

##### Set up library environment.

# Load language initialization files and register each language in $LangModes variable.
for initFile in 1 => xPathname('*Init.ms', 'Glob, SkipNull')

	# Initialization script found.  Create programming language group if needed.
	defined?('ModeGroup', $langParams[0]) or 1 => editModeGroup $langParams[0], 'Description: Programming language modes.'

	# Load array in script.  Library name, extensions array, and binary array may be nil.
	libName, modeName, modeDesc, macPrefix, fileExts, binNames, fenceMode, preKeyTrigs, nlTrigs = xeqFile(initFile)
	1 => editMode modeName, 'Buffer: true', 'Description: %s' % modeDesc, 'Group: %s' % $langParams[0]
	apush $LangModes, [[false, nil?(libName) ? modeName : libName, modeName, macPrefix, fileExts, binNames, fenceMode, nil,\
	 nil], preKeyTrigs, nlTrigs]
endloop

# Bail out here if no initialization files were found (nothing left to do).
if not defined?('ModeGroup', $langParams[0])
	return
endif

##### Commands and Functions #####

# Do wrap preparation, given (1), "copy block" flag; (2), lead-in character ('#' or ''); (3), "if" argument; and (4), ending
# conditional keyword ('else' or 'endif').  First, determine boundaries of line block using n argument in same manner as the
# built-in line commands.  Then do some common insertions and optionally, copy line block to kill buffer.
function langWrapPrep(4)
	lineCt = selectLine($0)				# Get line count and position point.
	$1 && lineCt => copyLine			# Copy block to kill buffer if requested.
	openLine
	insert $2, 'if ', $3				# Insert "[#]if 0" or "[#]if 1".
	(lineCt + 1) => endLine
	insertf "\n%s%s", $2, $4			# Insert "[#]else" or "[#]endif".
	1 => bufBound? ? -1 => newline : forwChar
endroutine

# Wrap "{#|}if 0" and "{#|}else" around a block of lines, duplicate them, and add "{#|}endif", given (1), lead-in character
# ('#' or '').
function langWrapIfElse(1)
	$0 => langWrapPrep true, $1, '0', 'else'
	setMark
	yank
	delRingEntry 'Kill'
	insert $1, "endif\n"
	swapMark ?.
	beginText
endroutine

# Reframe current window after a "langFindObj" call so that the found routine is displayed at the top of the window, if
# possible.  Arguments: (1), mark from "locateFile" call.
function langReframeWind(1)
	$1 and -1 => gotoMark $1
	1 => setMark ??

	loop
		# Search backward for a blank line.
		beginLine
		while backLine
			if $lineText =~ '^\s*$:r'
				forwLine
				1 => reframeWind
				-1 => gotoMark ??
				break 2
			endif
		endloop

		# Blank line not found... so just put current line near top of window.
		-1 => gotoMark ??
		(lineNum = $windSize / 5) == 0 and lineNum = 1
		lineNum => reframeWind
		break
	endloop
endroutine

# Find a routine name (argument is true) or class name (argument is false) and render buffer according to n argument if found.
# User is prompted for directory to search and the routine or class name.  If directory is not specified (nil or null), current
# buffer (only) is searched, otherwise files in specified directory.  In the latter case, the first file found that contains
# the routine name is selected.  A three-element array of search parameters for the current programming mode is obtained from
# the mode header array.  Array element 2 (initially nil) is updated by this routine and used as default for subseqent calls.
function langFindObj(1)
	funcParams, classParams, lastDir = (searchParams = $langParams[1][0][8])
	prmt, re, template = searchParams[$1 ? 0 : 1]

	# Get directory to search.
	dir = prompt('Directory to search', 'Type: File', lastDir)
	if !empty?(dir) && substr(dir, -1, 1) == '/' && length(dir) > 1
		dir = substr(dir, 0, -1)
	endif

	# Get routine name.
	routineName = prompt('%s name' % prmt, 'Type: SearchRing')
	if empty?(routineName)
		return false
	endif

	# Save search directory for subsequent invocation.  If it's nil or null, search current buffer; otherwise, search files.
	MarkMsg = 'Mark ~u`~U set to previous position'
	NotFound = 'Not found'
	SearchPat = '\<%s\>:re' % routineName
	if empty?(searchParams[2] = dir)

		# Search current buffer.
		$searchPat = sub(re, '%NAME%', routineName)
		1 => setMark ?`
		beginBuf
		if huntForw
			beginLine
			1 => setMark ?_
			langReframeWind ?_
			result = 1 => message('High, TermAttr', MarkMsg)
		else
			gotoMark ?`
			result = 1 => message('Fail, High', NotFound)
		endif
		delRingEntry 'Search'
	else
		# Search files.  Call file scanner (dir, glob, pat) and render buffer if found.
		if not result = locateFile(dir, template, sub(re, '%NAME%', routineName))
			result = 1 => message('Fail, High', NotFound)
		else
			bufname, mark = result

			# Leave buffer as is?
			if $0 == 0
				result = message('Found in ', mark ? 'existing ' : '', "buffer '", bufname, "'")
			else
				# Render the buffer.
				result = $0 => selectBuf bufname
				if $0 == -1
					result = true
				else
					windNum = result[2]
					if(windNum != $windNum)
						saveWind
						selectWind windNum
						langReframeWind mark
						restoreWind
					else
						langReframeWind mark
					endif
					result = mark ? 1 => message('TermAttr', MarkMsg) : true
				endif
			endif
		endif
	endif

	$searchPat = SearchPat
	result
endroutine

# Manage programming language library.
#
# If one argument is specified:
#	Find a language mode and return its descriptor array, or nil if none, given (1), array of buffer modes.
# If two arguments specified:
#	Activate or deactivate a library, given (1), mode descriptor array; and (2), Boolean action.
function libUtil(1, 2)
	if length($ArgList) == 1

		# Check if a language mode is active.
		for modeInfo in $LangModes
			if ainclude? $1, modeInfo[0][2]
				return modeInfo
			endif
		endloop
		nil
	elsif length($ArgList) == 2
		modeHdr = $1[0]

		# Load library if needed.
		if $2 && !modeHdr[0]

			# Create trigger strings in descriptor arrays.
			for i in [1, 2]
				j = 0
				for trigAry in $1[i]
					if i > 1 || j > 0		# Skip "left justification" array.
						if trigAry != nil
							trigStr = ''
							for item in trigAry
								c = (type?(item) == 'int') ? chr(item) :\
								 substr(type?(item) == 'array' ? item[0] : item,\
								 i == 1 || j <= 1 ? -1 : 0, 1)
								nil?(index trigStr, c) and trigStr &= c
							endloop
							$1[i][j] = [trigStr, trigAry]
						endif
					endif
					++j
				endloop
			endloop

			# Load library if it exists and save its key bindings and search parameters.
			if (libFile = xPathname(modeHdr[3] & 'Lib')) != nil	# For example, "memacsLib".
				modeHdr[7], modeHdr[8] = xeqFile(libFile, $1)
			endif
			modeHdr[0] = true
		endif

		# Now activate or deactivate library if (1), activation request and library not active; or (2), deactivation
		# request and library active.
		if $2 != ((modeInfo = $langParams[1]) != nil && modeInfo[0][2] == modeHdr[2])

			# Loop through the binding list, if it exists.
			if modeHdr[7]
				for key in modeHdr[7]
					name = strShift(key, ?\t)
					if $2
						eval 'bindKey ', quote(key), ', ', name
					else
						1 => unbindKey key
					endif
				endloop
			endif

			# Save/unsave mode descriptor array and set command binding(s).
			if $2
				params = [$1, 'activated', [['newline', 'langNewline', true],\
				 ['indentRegion', 'langIndentRegion', $1[1][0]]]]
				$langParams[2] = 1 => chgMode(nil, modeHdr[6])
			else
				params = [nil, 'deactivated', [['langNewline', 'newline', true],\
				 ['langIndentRegion', 'indentRegion', $1[1][0]]]]
				$langParams[2] => chgMode(nil, modeInfo[0][6])
				$langParams[2] = nil
			endif

			$langParams[1] = params[0]
			for cmdInfo in params[2]
				if cmdInfo[2]
					for key in binding('KeyList', cmdInfo[0])
						eval 'bindKey ', quote(key), ', ', cmdInfo[1]
					endloop
				endif
			endloop
			message modeHdr[1], ' library ', params[1]
		endif
	endif
endroutine

##### Library Helper Routines -- general.

# Return true if all white space before point on current line, otherwise false.
function libBlankPrefix(0)
	offset = $lineOffset
	beginText
	$lineOffset == offset
endroutine

# Return true if current line is null or blank, otherwise false.
function libBlankLine(0)
	beginText
	$lineOffset == $LineLen
endroutine

# Search backward for first non-blank line and get its indentation.  If n argument, begin search at current line, otherwise
# previous one.  A two-element array is returned containing the indentation (or nil if not found), and the number of tabs to add
# to the indentation when it is inserted into the buffer.  The second value is determined as follows: if the line from which the
# indentation was taken (1), contains a right fence character by itself; (2), the character exists in the EOL outdenting
# "newline" trigger array; and (3), 'RFIndt' global mode is disabled; one is returned, otherwise zero.
function libGetIndent(0)
	beginLine
	n = ($0 == defn) ? 0 : 1
	loop
		if n-- <= 0 && !backLine
			break
		endif
		if not libBlankLine
			extraTabs = !nil?(trigAry = $langParams[1][2][1]) && $lineText =~ '^\s*([}\])])$' &&\
			 index(trigAry[0], match 1) != nil && !mode?('RFIndt') ? 1 : 0

			# Get indentation and remove any trailing spaces from it if hard tabs in effect.
			if !null?(indent = subline(-$lineOffset, $lineOffset)) && $softTabSize == 0 &&\
			 (i = 1 => index(indent, ?\t, 'Char, Last')) != nil && i < length(indent) - 1
				indent = substr(indent, 0, i + 1)
			endif
			return [indent, extraTabs]		# Indentation may be a null string.
		endif
	endloop
	[nil, 0]
endroutine

# Get indentation, beginning at the current line, copy to next line, and (1), decrease it one level (n < 0); (2), keep it the
# same (n == 0, default); or (3), increase it one level (n > 0).  It is assumed that mark '?' has been set at point.
function libIndentNext(0)
	$0 == defn and $0 = 0
	indent, ct = 0 => libGetIndent
	gotoMark ??
	delWhite
	-1 => newline
	insert indent

	# Adjust indentation per n argument and extra tab count returned from libGetIndent.
	if $0 > 0
		(ct + 1) => tab
	elsif $0 < 0
		(1 - ct) => delBackTab
	elsif ct > 0
		tab
	endif
endroutine

##### Library Helper Routines -- preKey hook.

# Clear line into which a special character (like '#' or '=') will be entered.  It is assumed that mark '?' has been set at
# point.
function pkLeftJustChar(0)

	# White space before point?
	if libBlankPrefix

		# Yes, erase it.
		delWhite
	else
		gotoMark ??
	endif
endroutine

# Set proper indentation for line into which a right fence will be entered ('}', ']', or ')').  It is assumed that mark '?' has
# been set at point.
function pkIndentRFence(0)

	# White space before point?
	if libBlankPrefix

		# Yes, go to matching (left) fence and get indentation from that line.
		if 1 => gotoFence $LastKey
			beginText
			indent = subline(-$lineOffset, $lineOffset)	# May be a null string.
			gotoMark ??
			beginLine
			delWhite
			insert indent
			mode?('RFIndt') and tab
			return
		endif
	endif
	gotoMark ??
endroutine

# Set proper indentation for line into which the last letter of a string or keyword (like "</ol" or "else") will be entered.  It
# is assumed that mark '?' was set at original point location.  If n < 0 (default), keyword is outdented one level; otherwise,
# indented at same level.
function pkOutdentStr(0)

	# Get indentation from last non-blank line.
	indent, ct = libGetIndent
	if indent
		gotoMark ??
		beginLine
		delWhite
		insert indent
		n = $0 < 0 ? ct - 1 : ct
		if n < 0
			delBackTab
		elsif n > 0
			tab
		endif
	endif
	gotoMark ??
endroutine

# Examine keys entered by the user and indent as appropriate if a programming language mode is active and certain conditions are
# met.
function hkPreKey(0) {description: "Examine keys that are entered and do auto-indentation as appropriate if a programming\
 language mode is active.\n\nReturns: nil."}
	if $LastKey >= 0 && $lineOffset == $LineLen && !nil?(activeLang = $langParams[1])
		1 => setMark ??
		trigArys = activeLang[1]
		loop
			# Left justify character?
			if !nil?(trigAry = trigArys[0]) && ainclude?(trigAry, $LastKey)
				pkLeftJustChar
				break
			endif

			# No, on first line?
			beginLine
			if -1 => bufBound?

				# Yes, nothing to do.
				gotoMark ??
				break
			endif
			gotoMark ??

			# No, check for outdenting and same-denting.
			lineStr = nil
			for i in [1, 2]
				if !nil?(trigAry = trigArys[i]) && !nil?(1 => index trigAry[0], $LastKey, 'Char')
					for item in trigAry[1]

						# Check for right fence first.
						if i == 1 && type?(item) == 'int'
							if $LastKey == item
								pkIndentRFence
								break 3
							else
								next
							endif
						endif

						# No go... check strings.
						nil?(lineStr) and lineStr = strip($lineText) & chr($LastKey)
						if type?(item) == 'array'
							if lineStr == item[0]
								-1 => beginText			# Can't fail.
								lineStr = getWord
								2 => endLine
								for notKW in item[1, length(item) - 1]
									if lineStr == notKW
										break 4
									endif
								endloop
								(i - 2) => pkOutdentStr
								break 3
							endif
						elsif lineStr == item
							(i - 2) => pkOutdentStr
							break 3
						endif
					endloop
				endif
			endloop
			gotoMark ??
			break
		endloop
		delMark ??
	endif
	nil
endroutine
setHook 'preKey', hkPreKey

##### Library Helper Routines -- newline command.

# Check characters and strings at end of current line and indent or outdent next line accordingly, given newline triggers
# (array).  Point is assumed to be at end of line.  Returns: true if formatting was done, otherwise false.
function nlChStrCheckEOL(1)
	backChar
	c = $lineChar
	for i in [0, 1]
		if !nil?(trigAry = $1[i])
			for item in trigAry[1]
				if type?(item) == 'int'
					if c == item
						if i == 0 || libBlankPrefix
							(i == 0 ? 1 : -1) => libIndentNext
							return true
						else
							endLine
							backChar
							return false
						endif
					endif
				elsif isClass?('word', ord item)
					break
				else
					len = length(item) - 1
					if subline(-len) == item
						(i == 0 ? 1 : -1) => libIndentNext
						return true
					endif
				endif
			endloop
		endif
	endloop
	false
endroutine

# Check keywords at end of current line and indent or outdent next line accordingly, given newline triggers (array).  Point is
# assumed to be on last character of line.  Returns: true if formatting was done, otherwise false.
function nlKWCheckEOL(1)
	c = $lineChar
	if (lineWord = getWord)
		for i in [0, 1]
			if !nil?(trigAry = $1[i]) && !nil?(1 => index trigAry[0], c, 'Char')
				for item in trigAry[1]
					if lineWord == item
						(i == 0 ? 1 : -1) => libIndentNext
						return true
					endif
				endloop
			endif
		endloop
	endif
	false
endroutine

# Check keywords at beginning of current line and indent or outdent next line accordingly, given newline triggers (array).
# Returns: true if formatting was done, otherwise false.
function nlKWCheckBOL(1)
	beginText
	c = $lineChar
	if (lineWord = getWord)
		for i in [2, 3]
			if !nil?(trigAry = $1[i]) && !nil?(1 => index trigAry[0], c, 'Char')
				for keyword in trigAry[1]
					if lineWord == keyword
						(i == 2 ? 1 : -1) => libIndentNext
						return true
					endif
				endloop
			endif
		endloop
	endif
	false
endroutine

# Do auto-indentation if applicable, after a newline has been typed.  Bound to the "newline" command when a language mode is
# active.
command langNewline(0) {description: "Do auto-indentation as appropriate if a programming language mode is active and a newline\
 was typed."}
	$0 == defn and $0 = 1

	# If n is negative, just insert newlines.
	if $0 < 0
		$0 => newline
	elsif $0 > 0
		nlTrigs = $langParams[1][2]

		# For n times...
		loop
			1 => setMark ??

			# If blank line, clear it and same-indent next line.
			if libBlankLine
				delWhite
				libIndentNext
			else
				gotoMark ??

				# Line not blank.  If not at EOL, just insert newlines.
				if $lineOffset < $LineLen
					$0 => newline
					break
				endif

				# At EOL.  Do one iteration of following loop, breaking out at first change.
				loop
					# Check last character on current line.
					if nlChStrCheckEOL nlTrigs
						break
					endif

					# Check keywords at end of current line.
					if nlKWCheckEOL nlTrigs
						break
					endif

					# Check strings at beginning of current line.
					beginText
					c = $lineChar
					if !nil?(trigAry = nlTrigs[2]) && !nil?(1 => index trigAry[0], c, 'Char')
						for item in trigAry[1]
							if type?(item) == 'string'
								if isClass?('word', ord item)
									break
								endif
								if subline(0, length item) == item
									1 => libIndentNext
									break 2
								endif
							endif
						endloop
					endif

					# Check keywords at beginning of current line.
					if nlKWCheckBOL nlTrigs
						break
					endif

					# No keyword match found... keep same indentation.
					libIndentNext
					break
				endloop
			endif
			delMark ??
			if --$0 == 0
				break
			endif
		endloop
	endif
	nil
endroutine

# Indent lines in region by n tab stops (default 1), skipping any lines that begin with a "left justification" character defined
# for the active language.  Bound to the "indentRegion" command when a language mode is active.
command langIndentRegion(0) {description: 'Indent lines in region by n tab stops (default 1) if a programming language mode is\
 active, skipping any lines that begin with a \"left justification\" character defined for the active language.'}

	# If no left justification characters defined for current language or invalid n argument, call built-in command.
	if nil?(leftJustAry = $langParams[1][1][0]) || $0 <= 0 && $0 != defn
		return $0 => indentRegion
	endif

	# Select line block and process it.
	$0 == defn and $0 = 1
	if (lineCt = selectLine(0)) > 0
		setMark
		loop
			loop
				if $LineLen > 0
					c = $lineChar
					for leftJustChar in leftJustAry
						if c == leftJustChar
							break 2
						endif
					endloop
					$0 => tab
				endif
				break
			endloop
			2 => beginLine
			if --lineCt == 0
				break
			endif
		endloop
	endif
	nil
endroutine

##### Hook Routines.

# Load and/or activate language library in the current buffer that was entered, if applicable.  Given argument is return value
# from "exitBuf" hook (if any), which is ignored.
function hkEnterBuf(1) {description: 'Load and/or activate language library in the current buffer that was entered, if\
 applicable.'}
	oldModeDesc = $langParams[1]
	newModeDesc = libUtil($BufModes)

	# Deactivate current library, if applicable.  Nothing to do if old and new modes are the same.
	if newModeDesc && oldModeDesc && newModeDesc[0][2] == oldModeDesc[0][2]
		return
	elsif !nil? oldModeDesc
		libUtil oldModeDesc, false
	endif

	# Activate new library, if applicable.
	nil?(newModeDesc) or libUtil(newModeDesc, true)

	nil
endroutine
setHook 'enterBuf', hkEnterBuf

# Do initialization tasks for buffer language mode.  Called whenever a mode is changed (via 'mode' hook), given (1) buffer name
# if a buffer mode was changed, otherwise nil; and (2), prior buffer or global modes.
function hkMode(2) {description: 'Do initialization tasks for buffer language mode.'}

	# Nothing to do unless a mode has changed in the current buffer.
	if $1 == $bufname
		bufModes = $BufModes

		# Do library initialization and/or activation if a language mode has changed.
		for modeInfo in $LangModes
			if ainclude?($2, modeInfo[0][2]) != mode?(modeInfo[0][2])

				# Deactivate prior mode library, if needed.
				(modeDesc = libUtil($2)) and libUtil modeDesc, false

				# Activate current mode library, if needed.
				(modeDesc = libUtil(bufModes)) and libUtil modeDesc, true

				break
			endif
		endloop
	endif

	nil
endroutine
setHook 'mode', hkMode

# Set language mode when a buffer filename is changed, given (1), buffer name; and (2), filename.
function hkFilename(2) {arguments: 'buf-name, filename', description: 'Set language mode when a buffer filename is changed.'}
	if nil? $2
		return nil
	endif

	bufFile0 = bufFile = basename($2)
	mode = nil

	# Loop through all of file's extensions from right to left until a match is found in $LangModes or none left.
	loop
		if nil?(i = 1 => index(bufFile, ?., 'Char, Last'))

			# No filename extension.  Set shell mode if (1), filename begins with "makefile" (ignoring case); or
			# (2), filename has ".bak" extension or no extension, does not contain a space or period, and begins
			# with a lower case letter; and file contains one of the following patterns: /^\s*\bif \[ /, /^\s*fi\b/,
			# /\b;;\b/, or /^\s*\bdone\b/.
			if defined? 'Mode', 'Shell'
				str = bufFile0
				substr(str, -4, 4) == '.bak' and str = substr(str, 0, -4)
				if index(lowerCaseStr(bufFile), 'makefile') == 0
					mode = 'Shell'
				elsif str == tr(str, '. ', '') && ord(str) == ord(lowerCaseStr str)

					# Have typical shell filename.  Check contents for shell syntactic element.
					1 => setMark ??
					beginBuf
					for pat in ['^\s*\bif \[ :re', '^\s*fi\b:re', '\b;;\b:re', '^\s*\bdone\b:re']
						found = searchForw(pat)
						delRingEntry 'Search'
						if found
							mode = 'Shell'
							break
						endif
					endloop
					-1 => gotoMark ??
				endif
			endif
			break
		else
			# Get filename extension.
			fileExt = substr(bufFile, i + 1)
			bufFile = substr(bufFile, 0, i)

			# Set language mode for known file extensions.
			for modeInfo in $LangModes
				if (extList = (modeHdr = modeInfo[0])[4]) != nil
					for knownExt in extList
						if fileExt == knownExt
							mode = modeHdr[2]
							break 3
						endif
					endloop
				endif
			endloop
		endif
	endloop

	# Set buffer mode, if applicable.
	mode && 1 => chgMode $1, mode
	nil
endroutine
setHook 'filename', hkFilename

# Set language mode from shebang line when a file is read info a buffer, given (1), buffer name; and (2), filename.
function hkRead(2) {arguments: 'bufname, filename', description: 'Set language mode when a file is read into a buffer.'}

	# Language mode already set in buffer?
	if not groupMode?($langParams[0], $1)

		# No, check first line of buffer (file) for shebang.
		if line1 = bgets($1)
			1 => beginBuf $1
			if substr(line1, 0, 3) == '#!/'

				# Shebang found.  Compare binary path against all known ones.
				for modeInfo in $LangModes
					if !nil?(binList = (modeHdr = modeInfo[0])[5])
						type?(binList) == 'array' or binList = [binList]
						for binName in binList
							if index(line1, '/' & binName) != nil
								1 => chgMode $1, modeHdr[2]
								break 2
							endif
						endloop
					endif
				endloop
			endif
		endif
	endif
	nil
endroutine
setHook 'read', hkRead
