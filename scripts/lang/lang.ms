# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# lang.ms		Ver. 10.0.0
#	MightEMacs startup file for managing programming language libraries.

##### Initialization #####
1 => editMode 'RFIndt', 'Description: Use Ratliff-style indentation of right fence character.', 'Global: true', 'Hidden: true'

##### Global Variables #####

# Define hash of programming language information.  Hash is built from hashes read from initialization files (e.g.,
# "rubyInit.ms").  Each hash read from an initialization file contains the following keys and values:
#  LibName	[String] Library name (usually mode name).
#  ModeName	[String] Mode name.
#  ModeDesc	[String] Mode description.
#  LibFile	[String] Name of file (without extension) containing library routines; e.g., "rubyLib".
#  FileExts	[Array] File extension(s), or nil if none.
#  BinNames	[Array] Unix binary name(s), or nil if none.
#  FenceMode	[String] Fence mode to activate for language ("Fence1" or "Fence2").
#  LineComment	[String] Line comment symbol (for example, "//" for C and C++), or nil if none. [OPTIONAL]
#  LeftJust	[String] Chars for left justifying line being typed, or nil if none.
#  PreKeyTrigs	[Array] "preKey" hook triggers:
#			0: [Array] Trigger chars and/or strings for outdenting current line, or nil if none.
#			1: [Array] Trigger strings for same-denting current line, or nil if none.
#  NLTrigsEOL	[Array] Newline triggers at end of line:
#			0: [Array] Trigger chars, strings, and/or keywords at EOL for indenting next line, or nil if none.
#			1: [Array] Trigger chars, strings, and/or keywords at EOL for outdenting next line, or nil if none.
#  NLTrigsBOL	[Array] Newline triggers at beginning of line:
#			0: [Array] Trigger strings and/or keywords at BOL for indenting next line, or nil if none.
#			1: [Array] Trigger keywords at BOL for outdenting next line, or nil if none.
#  BeginBlkBOL	[Hash]	Begin-block symbols at beginning of line, or nil if none. [OPTIONAL]
#  EndBlkBOL	[Hash]	End-block symbols at beginning of line, or nil if none. [OPTIONAL]
#  BeginBlkEOL	[Hash]	Begin-block symbols at end of line, or nil if none. [OPTIONAL]
#
# A "mode dictionary" hash is built from each initialization file and saved in $langParams using mode name as key.  It contains
# all hash keys above except "ModeDesc", plus the following:
#  LibLoaded	[Boolean] Library file state.  Associated "Lib" file (e.g., "rubyLib.ms") was searched for and loaded if true.
#  Bindings	[Array] Key bindings for library's commands, or nil if none.
#  FuncSearch	[Array] "Function search" parameters:
#			0: [String] User prompt prefix.
#			1: [String] Regular expression that matches a function name for given language.
#  ClassSearch	[Array] "Class search" parameters:
#			0: [String] User prompt prefix.
#			1: [String] Regular expression that matches a class name for given language.
#  DirGlob	[Array] Search directory and glob pattern to use for finding function or class object above.
#			0: [String] Directory to search (obtained from user).
#			1: [String] Glob pattern that matches source files to search.
# Notes:
#  1. If a trigger array allows multiple types of values, all characters must be specified first, then strings, then keywords.
#  2. Items of type 'string' in the newline trigger arrays are considered keywords if they begin with a word character,
#     otherwise strings.
#  3. All trigger arrays are converted to form [trigger-chars, [items]] to speed up processing at runtime, where first element
#     is all unique characters at beginning or end (as appropriate) of each string or keyword item in string form.
#  4. No action is taken for any item in a "preKey" trigger array or any character in the second "newline" EOL trigger array
#     (element 1, EOL outdenting) unless the item is preceded by white space in the buffer, and for a "preKey" trigger array,
#     also unless point is at end of line.  Hence, the entire buffer line minus any leading white space is checked (as a string)
#     against all non-character "preKey" items and therefore keywords (which begin and end on a word boundary) are not used.
#  5. Any character in the first "preKey" trigger array (element 0, outdenting) is assumed to be a right fence; that is, '}',
#     ']', or ')', and is outdented (or same-dented) to match the line containing the corresponding left fence.
#  6. Any string in the first "preKey" trigger array (element 0, outdenting) may be an array of form [string, keyword,...]
#     instead.  In this case, the string is ignored if the first word on the previous line matches any specified keyword.
#     For example, in Ruby mode, the first "when" clause should not be outdented past the "case" in the previous line, so
#     ['when', 'case'] would be used to prevent that.
#  7. Triggers at EOL take precedence over those at BOL.
#  8. When a library file for a language is loaded (for example, "cLib.ms"), it may contain arrays of parameters used for
#     finding a function, method, or other syntactic element in a source file (or nil if none) inside a hash.  The array
#     elements for the "FuncSearch" and "ClassSearch" hash keys are as follows:
#	0: Type of object being searched for; for example, "Function".  Used as prompt string (with " name" appended) for
#	   obtaining the object name from the user.
#	1: A regular expression that will match a line containing the object's declaration in a source file (for example, a C
#	   function or class definition in Ruby) with the string '%NAME%' embedded within it as a placeholder for the name.  The
#	   langFindObj function that is invoked will replace the '%NAME%' string with the name of the object being searched for
#	   each time the RE is used.  The langFindObj function is called by a stub command in the language's library file,
#	   which is usually bound to 'C-c C-f'.
#     The array elements for the "DirGlob" hash key are as follows:
#	0: A filename template matching the files to be searched (for example, '*.c' for C).
#	1: The last directory path entered by the user (initially nil).

# Define hash of language parameters (mode dictionaries and state information).
$langParams = {\
 '%ActiveDict' => nil,		/# Mode dictionary for active language mode in current buffer, or nil if none #/\
 '%FormerFenceState' => nil,	/# Former state of "FenceN" global mode #/\
 '%ModeGroup' => 'Lang'		/# Mode group name for language modes #/\
 }

##### Set up library environment.

# Load language initialization files and register each language in $langParams hash with mode name as key.
for initFile in 1 => xPathname('lang/*Init.ms', 'Glob, SkipNull')

	# Initialization script found.  Create programming language group if needed.
	if !isModeGrp?($langParams['%ModeGroup'])
		1 => editModeGrp $langParams['%ModeGroup'], 'Description: Programming language modes.'
	endif

	# Load hash in script, create buffer mode for language, and save hash in $langParams hash.
	dictionary = xeqFile(initFile)
	modeName = dictionary['ModeName']
	1 => editMode modeName, 'Buffer: true', 'Description: %s' % hdelete(dictionary, 'ModeDesc'),\
	 'Group: %s' % $langParams['%ModeGroup']
	dictionary['LibLoaded'] = false
	$langParams[modeName] = dictionary
endloop

# Bail out here if no initialization files were found (nothing left to do).
if !isModeGrp?($langParams['%ModeGroup'])
	return
endif

##### Commands and Functions #####

# Do wrap preparation.  Argument(s):
#  1: [Boolean] "Copy block" flag.
#  2: [String] Lead-in character ('#' or '').
#  3: [String] "if" argument.
#  4: [String] Ending conditional keyword ('else' or 'endif').
# First, determine boundaries of line block using n argument in same manner as the built-in line commands.  Then do some common
# insertions and optionally, copy line block to kill buffer.  If first argument is true and third argument is not '0', also set
# mark ?< at beginning of line block.
function langWrapPrep(4)
	lineCt = selectText($0)				# Get line count and position point.
	$1 && lineCt => copyLine			# Copy block to kill buffer if requested.
	openLine
	insert $2, 'if ', $3				# Insert "[#]if {0 | name}" or "[#]if {1 | name}".
	forwChar
	if $1 && $3 != '0'
		beginText
		setMark ?<
	endif
	lineCt => endLine
	insertf "\n%s%s", $2, $4			# Insert "[#]else" or "[#]endif".
	1 => bufBound? ? -1 => newline : forwChar
endroutine

# Wrap "{#|}if 0" and "{#|}else" around a block of lines, duplicate them, and add "{#|}endif".  Argument(s):
#  1: [String] Lead-in character ('#' or '').
function langWrapIfElse(1)
	$0 => langWrapPrep true, $1, '0', 'else'
	setMark
	yank
	delRingEntry 'Kill'
	insert $1, "endif\n"
	swapMark ?.
	beginText
endroutine

# Return "word" at beginning of current line or nil, if too short.
function langGetWord(0)
	beginText
	setMark
	endWord
	length(word = RegionText) > 1 ? word : nil
endroutine

# Helper function for "langGotoBlkBound" command.  Get symbol at beginning and/or end of current line, which is defined as (1),
# a word at beginning of line; (2) a single punctuation character at beginning of line; or a (3), word; or (4), single
# punctuation character at end of line.  Note that a line may have a symbol at both its beginning and end.  Returned bits are:
#	0x01	Begin symbol found.
#	0x02	End symbol found.
#	0x04	"else" symbol found at beginning of line.
#
# If symbol found at beginning of line:
#	If it exists in optional (argument 2) "begin block" hash given, return 0x01.
#	If it exists in "end block" (argument 3) hash given, return 0x02.
# If symbol found at end of line:
#	If it exists in optional (argument 4) "begin block" hash given, return 0x01.
# If both of the above, return 0x03.
# If "else" word found, return 0x04.
# If none of the above, return zero.  Argument(s):
#  1: [String] Begin-line-comment symbol, or nil if none.
#  2: [Hash] Begin-block symbols at beginning of line, or nil if none.
#  3: [Hash] End-block symbols at beginning of line.
#  4: [Hash] Begin-block symbols at end of line, or nil if none.
function langHasSym?(4)
	skip = foundBegin = foundEnd = false

	# Skip blank or comment line.
	if !beginText || !nil?($1) && subline(0, length($1)) == $1
		skip = true

	# Skip current line if previous line ends with a backslash.
	elsif -1 => endLine
		LineLen > 0 && subline(-1, 1) == '\\' and skip = true
		forwChar
	endif

	if !skip
		# Get symbol at beginning of line and check it.
		beginText
		if (word = getWord)
			if $2 && hasKey?($2, word)
				foundBegin = true
			elsif hasKey?($3, word)
				foundEnd = true
			elsif word == 'else'
				return 0x04
			endif
		endif
		if !foundEnd
			beginText
			if hasKey?($3, chr($lineChar))
				return 0x02
			endif
		endif

		# Get symbol at end of line and check it.
		if $4 && !foundBegin
			endLine
			c = subline(-1, 1)
			if isClass? 'word', c
				if hasKey? $4, -1 => getWord
					foundBegin = true
				endif
			elsif hasKey? $4, c
				foundBegin = true
			endif
		endif

		# Return result.
		if foundBegin
			return foundEnd ? 0x03 : 0x01
		elsif foundEnd
			return 0x02
		endif
	endif

	# Match not found.
	0
endroutine

# Go to matching block boundary if current line contains a block boundary symbol (keyword or character).  Set mark '`' to
# current position, mark '^' to top line of the block, mark ':' to "else" line (if any), and mark '$' to the bottom line.
# Argument(s):
#  1: [Hash] Language hash containing block begin/end symbols.
function langGotoBlkBound(1)

	# Get language hashes and initialize controls.
	lineComment = $1['LineComment']
	beginBlkBOL = $1['BeginBlkBOL']
	endBlkBOL = $1['EndBlkBOL']
	beginBlkEOL = $1['BeginBlkEOL']

	# direc, startMark, startType, otherMark, otherType -- forward and backward.
	direcParams = [[2, ?^, 0x01, ?$, 0x02], [-1, ?$, 0x02, ?^, 0x01]]

	foundOther = foundElse = false
	beginWord = nil
	setMark ?`

	loop
		# Check current line for symbol.
		flags = langHasSym?(lineComment, beginBlkBOL, endBlkBOL, beginBlkEOL)
		if flags
			# Symbol found.  Set up search parameters.
			beginText
			direc, startMark, startType, otherMark, otherType = direcParams[flags & 0x01 ? 0 : 1]
			setMark startMark
			startType == 0x01 and beginWord = langGetWord()
			delMark(otherMark)
			level = 1
			delMark ?:, otherMark
		else
			# Boundary symbol not found.
			errorMsg = 'Not on block begin/end line'
			break
		endif

		# Scan lines until matching symbol found or hit buffer boundary.
		loop
			if direc => beginLine() == false

				# Hit buffer boundary... it's a bust.
				gotoMark ?`
				errorMsg = 'Matching block begin/end line not found'
				break 2
			endif

			# Check current line for symbol.
			flags = langHasSym?(lineComment, beginBlkBOL, endBlkBOL, beginBlkEOL)
			if flags
				# Symbol found.  Ignore line if both begin and end.
				if flags & 0x03 == 0x03
					next
				elsif flags == otherType
					if --level == 0

						# Found other symbol at top level.
						if otherMark == ?^
							beginWord = langGetWord()
						endif
						break
					endif
				elsif flags == startType
					++level
				endif

			# Symbol not found.  Mark current line if top level and "else".
			elsif level == 1 && beginText && getWord() == 'else'
				foundElse = true
				setMark ?:
			endif
		endloop

		# Success.
		beginText
		setMark otherMark
		foundOther = true
		break
	endloop

	# Clean up.
	if foundOther
		msgPrefix = foundElse ? 'if/else block' : nil?(beginWord) ? 'Block begin/end markers' :\
		 '"%s" block' % beginWord
		1 => message('TermAttr', msgPrefix, ' found.  Mark ~u`~U set to previous position')
	else
		gotoMark ?`
		interactive? and beep
		1 => message 'Fail, High', errorMsg
	endif
endroutine

# Reframe current window after a "langFindObj" call so that the found routine is displayed at the top of the window, if
# possible.  Argument(s):
#  1: [Integer] Mark from "locateFile" call.
function langReframeWind(1)
	$1 and -1 => gotoMark $1
	setMark ??

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

# Find a routine name or class name per Boolean argument and render buffer according to n argument if found.  User is prompted
# for directory to search and the routine or class name.  If directory is not specified (nil or null), current buffer (only) is
# searched, otherwise files in specified directory.  In the latter case, the first file found that contains the routine name is
# selected.  Search parameters for the current programming mode are obtained from the mode dictionary hash.  Additionally, the
# search directory (initially nil) is updated by this routine and used as default for subseqent calls.  Argument(s):
#  1: [Boolean] Find routine name if true, otherwise class name.
# Returns:
#	[Boolean] true if successful, otherwise false.
function langFindObj(1)
	prmt, re = (activeDict = $langParams['%ActiveDict'])[$1 ? 'FuncSearch' : 'ClassSearch']
	dir, globPat = (dirGlob = activeDict['DirGlob'])

	# Get directory to search.
	dir = prompt('Directory to search', 'Type: File', dir)
	if !nil?(dir) && length(dir) > 1
		dir = chomp(dir, ?/)
	endif

	# Get routine name.
	routineName = prompt('%s name' % prmt, 'Type: SearchRing')
	if empty?(routineName)
		return false
	endif

	# Save search directory and glob for subsequent invocation.  If it's nil or null, search current buffer; otherwise,
	# search files.
	markMsg = 'Mark ~u`~U set to previous position'
	notFound = 'Not found'
	searchPat = '\<%s\>:re' % routineName
	if empty?(dirGlob[0] = dir)

		# Search current buffer.
		$searchPat = sub(re, '%NAME%', routineName)
		setMark ?`
		beginBuf
		if huntForw
			beginLine
			setMark ?_
			langReframeWind ?_
			result = 1 => message('High, TermAttr', markMsg)
		else
			gotoMark ?`
			result = 1 => message('Fail, High', notFound)
		endif
		delRingEntry 'Search'
	else
		# Search files.  Call file scanner (dir, glob, pat) and render buffer if found.
		if not result = locateFile(dir, globPat, sub(re, '%NAME%', routineName))
			result = 1 => message('Fail, High', notFound)
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
					result = mark ? 1 => message('TermAttr', markMsg) : true
				endif
			endif
		endif
	endif

	$searchPat = searchPat
	result
endroutine

# Build trigger array index.  Argument(s):
#  1: [Hash] Work hash.
#  2: [Array] Trigger array.
#  3: [Boolean] Use character at EOL if true, otherwise BOL.
# Returns:
#	[Hash] Work hash with keys as index.
function trigIndex(3)
	for item in $2
		if typeof(item) == 'array'
			$1 = trigIndex($1, item, $3)
		else
			$1[typeof(item) == 'int' ? chr(item) : item[$3 ? -1 : 0, 1]] = nil
		endif
	endloop
	$1
endroutine

# Manage programming language library.
#
# If one argument is specified, find a language mode and return its dictionary, or nil if none.  Argument(s):
#  1: [Array] Buffer modes.
# If two arguments specified, activate or deactivate a library.  Argument(s):
#  1: [Hash] Mode dictionary.
#  2: [Boolean] Activate library if true, otherwise deactivate.
function libUtil(1, 2)
	if length(ArgList) == 1

		# Check if a language mode is active.
		for mode in $1
			if !nil?(modeDict = $langParams[mode])
				return modeDict
			endif
		endloop
		nil
	else
		# If activation request and library not yet been loaded, create trigger array indices.
		if $2 && !$1['LibLoaded']

			# Create trigger-chars string index for each trigger array and replace array with [index, [trig-ary]] to
			# speed up processing.
			for key in ['PreKeyTrigs', 'NLTrigsEOL', 'NLTrigsBOL']
				j = 0
				for trigAry in $1[key]
					if trigAry != nil
						hashIndex = trigIndex({}, trigAry, key != 'NLTrigsBOL')
						$1[key][j] = [join(nil, hkeys hashIndex), trigAry]
					endif
					++j
				endloop
			endloop

			# Load library file if it exists (e.g., "memacsLib.ms") and save its key bindings and search parameters.
			if (libFile = $1['LibFile']) != nil && (libFile = xPathname('lang/%s' % libFile)) != nil
				$1['Bindings'], $1['FuncSearch'], $1['ClassSearch'], $1['DirGlob'] = xeqFile(libFile, $1)
			endif
			$1['LibLoaded'] = true
		endif

		# Now activate or deactivate library if (1), activation request and library not active; or (2), deactivation
		# request and library active.
		if $2 != ((activeDict = $langParams['%ActiveDict']) != nil && activeDict['ModeName'] == $1['ModeName'])

			# Bind/unbind keys in library's binding list if it exists.
			if $1['Bindings']
				for key in $1['Bindings']
					name = strShift(key, ?\t)
					if $2
						eval 'bindKey ', quote(key), ', ', name
					else
						1 => unbindKey key
					endif
				endloop
			endif

			# Activate or deactivate mode dictionary hash and special command binding(s).
			leftJustChars = $1['LeftJust']
			if $2
				# Activate dictionary.
				newDict = $1
				action = 'activated'
				cmdList = [['newline', 'langNewline', true],\
				 ['indentLine', 'langIndentLine', leftJustChars],\
				 ['indentRegion', 'langIndentRegion', leftJustChars]]

				# Save current state of library's "FenceN" mode and enable it.
				$langParams['%FormerFenceState'] = 1 => chgMode($1['FenceMode'])
			else
				# Deactivate dictionary.
				newDict = nil
				action = 'deactivated'
				cmdList = [['langNewline', 'newline', true],\
				 ['langIndentLine', 'indentLine', leftJustChars],\
				 ['langIndentRegion', 'indentRegion', leftJustChars]]

				# Restore former state of library's "FenceN" mode.
				$langParams['%FormerFenceState'] => chgMode(activeDict['FenceMode'])
				$langParams['%FormerFenceState'] = nil
			endif

			$langParams['%ActiveDict'] = newDict
			for cmdInfo in cmdList
				oldCmd, newCmd, needed = cmdInfo
				if needed
					for key in eval('bindingsof ', oldCmd)
						eval 'bindKey ', quote(key), ', ', newCmd
					endloop
				endif
			endloop
			message $1['LibName'], ' library ', action
		endif
	endif
endroutine

##### Library Helper Routines -- general.

# Return true if all white space before non-white character at point on current line, otherwise false.
function libBlankPrefix(0)
	offset = $lineOffset
	beginText
	$lineOffset == offset
endroutine

# Return true if current line is null or blank, otherwise false.
function libBlankLine(0)
	beginText
	$lineOffset == LineLen
endroutine

# Search backward for first non-blank line and get its indentation.  If n argument, begin search at current line, otherwise
# previous one.  Returns two-element array:
#	0: [String] Indentation found (or nil if not found).
#	1: [Integer] Number of tabs to add to the indentation when it is inserted into the buffer.
# The second value is determined as follows: if the line from which the indentation was taken (1), contains a right fence
# character by itself; (2), the character exists in the EOL outdenting "newline" trigger array; and (3), 'RFIndt' global mode is
# disabled; one is returned, otherwise zero.
function libGetIndent(0)
	beginLine
	n = ($0 == defn) ? 0 : 1
	loop
		if n-- <= 0 && !backLine
			break
		endif
		if not libBlankLine
			extraTabs = !nil?(trigAry = $langParams['%ActiveDict']['NLTrigsEOL'][1]) &&\
			 $lineText =~ '^\s*([}\])])$' && index(trigAry[0], strMatchGrp 1) != nil &&\
			 !modeActive?('RFIndt') ? 1 : 0

			# Get indentation and remove any trailing spaces from it if hard tabs in effect.
			if !null?(indent = subline(-$lineOffset, $lineOffset)) && $softTabSize == 0 &&\
			 (i = 1 => index(indent, ?\t, 'Char, Last')) != nil && i < length(indent) - 1
				indent = indent[0, i + 1]
			endif
			return [indent, extraTabs]		# Indentation may be a null string.
		endif
	endloop
	[nil, 0]
endroutine

# Get indentation, beginning at the current line, copy to next line, and (1), decrease it one level (n < 0); (2), keep it the
# same (n == 0, default); or (3), increase it one level (n > 0).  It is assumed that mark '%' has been set at point.
function libIndentNext(0)
	$0 == defn and $0 = 0
	indent, ct = 0 => libGetIndent
	gotoMark ?%
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

# Clear line into which a special character (like '#' or '=') will be entered.  It is assumed that mark '%' has been set at
# point.
function pkLeftJustChar(0)

	# White space before point?
	if libBlankPrefix

		# Yes, erase it.
		delWhite
	else
		gotoMark ?%
	endif
endroutine

# Set proper indentation for line into which a right fence will be entered ('}', ']', or ')').  It is assumed that mark '%' has
# been set at point.
function pkIndentRFence(0)

	# White space before point?
	if libBlankPrefix

		# Yes, go to matching (left) fence and get indentation from that line.
		if gotoFence LastKey
			beginText
			indent = subline(-$lineOffset, $lineOffset)	# May be a null string.
			gotoMark ?%
			beginLine
			delWhite
			insert indent
			modeActive?('RFIndt') and tab
			return
		endif
	endif
	gotoMark ?%
endroutine

# Set proper indentation for line into which the last letter of a string or keyword (like "</ol" or "else") will be entered.  It
# is assumed that mark '%' was set at original point location.  If n < 0 (default), keyword is outdented one level; otherwise,
# indented at same level.
function pkOutdentStr(0)

	# Get indentation from last non-blank line.
	indent, ct = libGetIndent
	if indent
		gotoMark ?%
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
	gotoMark ?%
endroutine

# Examine keys entered by the user and indent as appropriate if a programming language mode is active.
function hkPreKey(0) {description: "Examine keys that are entered and do auto-indentation as appropriate if a programming\
 language mode is active.\n\nReturns: nil."}
	if LastKey >= 0 && $lineOffset == LineLen && !nil?(activeDict = $langParams['%ActiveDict'])
		setMark ?%
		leftJustChars = activeDict['LeftJust']
		trigArys = activeDict['PreKeyTrigs']
		loop
			# Left justify character?
			if !nil?(leftJustChars) && !nil?(1 => index leftJustChars, LastKey, 'Char')
				pkLeftJustChar
				break
			endif

			# No, on first line?
			beginLine
			if -1 => bufBound?

				# Yes, nothing to do.
				gotoMark ?%
				break
			endif
			gotoMark ?%

			# No, check for outdenting and same-denting.
			lineStr = nil
			i = 0
			loop
				if !nil?(trigAry = trigArys[i]) && !nil?(1 => index trigAry[0], LastKey, 'Char')
					for item in trigAry[1]

						# Check for right fence first.
						if i == 0 && typeof(item) == 'int'
							if LastKey == item
								pkIndentRFence
								break 3
							else
								next
							endif
						endif

						# No go... check strings.
						nil?(lineStr) and lineStr = strip($lineText) & chr(LastKey)
						if typeof(item) == 'array'
							if lineStr == item[0]
								-1 => beginText			# Can't fail.
								lineStr = getWord
								2 => endLine
								for notKW in item[1, length(item) - 1]
									if lineStr == notKW
										break 4
									endif
								endloop
								(i - 1) => pkOutdentStr
								break 3
							endif
						elsif lineStr == item
							(i - 1) => pkOutdentStr
							break 3
						endif
					endloop
				endif
				if ++i == 2
					break
				endif
			endloop
			gotoMark ?%
			break
		endloop
		delMark ?%
	endif
	nil
endroutine
setHook 'preKey', hkPreKey

##### Library Helper Routines -- newline command.

# Check characters and strings at end of current line and indent or outdent next line accordingly.  Point is assumed to be at
# end of line.  Argument(s):
#  1: [Array] EOL newline triggers.
# Returns:
#	true if formatting was done, otherwise false.
function nlChStrCheckEOL(1)
	backChar
	c = $lineChar
	i = 0
	for trigAry in $1
		if !nil?(trigAry)
			for item in trigAry[1]
				if typeof(item) == 'int'
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
		++i
	endloop
	false
endroutine

# Check keywords at end of current line and indent or outdent next line accordingly.  Point is assumed to be on last character
# of line.  Argument(s):
#  1: [Array] EOL newline triggers.
# Returns:
#	true if formatting was done, otherwise false.
function nlKWCheckEOL(1)
	c = $lineChar
	if (lineWord = getWord)
		i = 0
		for trigAry in $1
			if !nil?(trigAry) && !nil?(1 => index trigAry[0], c, 'Char')
				for item in trigAry[1]
					if lineWord == item
						(i == 0 ? 1 : -1) => libIndentNext
						return true
					endif
				endloop
			endif
			++i
		endloop
	endif
	false
endroutine

# Check keywords at beginning of current line and indent or outdent next line accordingly.  Argument(s):
#  1: [Array] BOL newline triggers.
# Returns:
#	true if formatting was done, otherwise false.
function nlKWCheckBOL(1)
	beginText
	c = $lineChar
	if (lineWord = getWord)
		i = 0
		for trigAry in $1
			if !nil?(trigAry) && !nil?(1 => index trigAry[0], c, 'Char')
				for keyword in trigAry[1]
					if lineWord == keyword
						(i == 0 ? 1 : -1) => libIndentNext
						return true
					endif
				endloop
			endif
			++i
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
		nlTrigsEOL = $langParams['%ActiveDict']['NLTrigsEOL']
		nlTrigsBOL = $langParams['%ActiveDict']['NLTrigsBOL']

		# For n times...
		loop
			setMark ?%

			# If blank line, clear it and same-indent next line.
			if libBlankLine
				delWhite
				libIndentNext
			else
				gotoMark ?%

				# Line not blank.  If not at EOL, just insert newlines.
				if $lineOffset < LineLen
					$0 => newline
					break
				endif

				# At EOL.  Do one iteration of following loop, breaking out at first change.
				loop
					# Check last character on current line.
					if nlChStrCheckEOL nlTrigsEOL
						break
					endif

					# Check keywords at end of current line.
					if nlKWCheckEOL nlTrigsEOL
						break
					endif

					# Check strings at beginning of current line.
					beginText
					c = $lineChar
					if !nil?(trigAry = nlTrigsBOL[0]) && !nil?(1 => index trigAry[0], c, 'Char')
						for item in trigAry[1]
							if typeof(item) == 'string'
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
					if nlKWCheckBOL nlTrigsBOL
						break
					endif

					# No keyword match found... keep same indentation.
					libIndentNext
					break
				endloop
			endif
			delMark ?%
			if --$0 == 0
				break
			endif
		endloop
	endif
	nil
endroutine

# Indent lines in a block by n tab stops, skipping any lines that begin with a "left justification" character defined for the
# active language.  Called in place of the "indentLine" and "indentRegion" commands when a language mode is active.
# Argument(s):
#  1: [Integer] Number of lines.
#  2: [String] Left justification characters.
function langIndentBlock(2)

	# Process line block.
	if $1 > 0
		setMark
		loop
			loop
				if LineLen > 0
					if !nil?(1 => index $2, $lineChar, 'Char')
						break 1
					endif
					$0 => tab
				endif
				break
			endloop
			2 => beginLine
			if --$1 == 0
				break
			endif
		endloop
	endif
	nil
endroutine

# Indent -[n] lines (default 1, region lines if n == 0) by given number of tab stops, skipping any lines that begin with a "left
# justification" character defined for the active language.  Bound to the "indentLine" command when a language mode is active.
# Argument(s):
#  1: [Integer] Number of tab stops.
command langIndentLine(1) {arguments: 'tab-stops', description: 'Indent [-]n lines (default 1, region lines if n == 0) by given\
 number of tab stops if a programming language mode is active, skipping any lines that begin with a "left justification"\
 character defined for the active language, and mark line block as new region.'}

	# Get number of tab stops and validate it.
	if !interactive?
		tabStops = $1
	elsif nil?(tabStops = prompt('Tab stops', nil, '1'))
		return
	endif
	if !numeric?(tabStops) || (n = toint(tabStops, 10)) < 0
		return 1 => message 'Fail', "'", tabStops, "' not numeric or out of range"
	endif

	# If no left justification characters defined for current language or zero argument, call built-in command.
	if nil?(leftJustChars = $langParams['%ActiveDict']['LeftJust']) || n == 0
		return $0 => indentLine n
	endif

	# Select line block and process it.
	n => langIndentBlock selectText($0), leftJustChars
endroutine

# Indent lines in region by n tab stops (default 1), skipping any lines that begin with a "left justification" character defined
# for the active language.  Bound to the "indentRegion" command when a language mode is active.
command langIndentRegion(0) {description: 'Indent lines in region by n tab stops (default 1) if a programming language mode is\
 active, skipping any lines that begin with a "left justification" character defined for the active language, and mark line\
 block as new region.'}

	# If no left justification characters defined for current language or invalid n argument, call built-in command.
	if nil?(leftJustChars = $langParams['%ActiveDict']['LeftJust']) || $0 <= 0 && $0 != defn
		return $0 => indentRegion
	endif

	# Select line block and process it.
	$0 == defn and $0 = 1
	$0 => langIndentBlock selectText(0), leftJustChars
endroutine

##### Hook Routines.

# Load and/or activate language library in the current buffer that was entered, if applicable.  Argument(s):
#  1: Return value from "exitBuf" hook (if any), which is ignored.
function hkEnterBuf(1) {arguments: 'exitBuf-rtn', description: 'Load and/or activate language library in the current buffer\
 that was entered, if applicable.'}
	oldModeDict = $langParams['%ActiveDict']
	newModeDict = libUtil(BufModes)

	# Deactivate current library, if applicable.  Nothing to do if old and new modes are the same.
	if newModeDict && oldModeDict && newModeDict['ModeName'] == oldModeDict['ModeName']
		return
	elsif !nil? oldModeDict
		libUtil oldModeDict, false
	endif

	# Activate new library, if applicable.
	if !nil?(newModeDict)
		libUtil newModeDict, true
	endif

	nil
endroutine
setHook 'enterBuf', hkEnterBuf

# Do initialization tasks for buffer language mode.  Called whenever a mode is changed (via 'mode' hook).  Argument(s):
#  1: [String] Buffer name if a buffer mode was changed, otherwise nil.
#  2: [Array] Prior buffer or global modes.
function hkMode(2) {arguments: 'buffer, prior-modes', description: 'Do initialization tasks for buffer language mode.'}

	# Nothing to do unless a mode has changed in the current buffer.
	if $1 == $bufname
		bufModes = BufModes

		# Do library initialization and/or activation if a language mode has changed.
		for node in $langParams
			key, modeDict = node
			if ord(key) != ?%
				modeName = modeDict['ModeName']
				if (old = ainclude?($2, modeName)) != (new = modeActive?(modeName))

					# Deactivate prior mode library, if needed.
					oldModeDict = libUtil($2)
					if old || oldModeDict != nil
						libUtil oldModeDict, false
					endif

					# Activate current mode library, if needed.
					newModeDict = libUtil(bufModes)
					if new || newModeDict != nil
						libUtil newModeDict, true
					endif

					break
				endif
			endif
		endloop
	endif

	nil
endroutine
setHook 'mode', hkMode

# Set language mode when a buffer filename is changed.  Argument(s):
#  1: [String] Buffer name.
#  2: [String] Filename.
function hkFilename(2) {arguments: 'buffer, file', description: 'Set language mode when a buffer filename is changed.'}
	if nil? $2
		return nil
	endif

	bufFile0 = bufFile = basename($2)
	mode = nil

	# Loop through all of file's extensions from right to left (for example, '.bak', '.rb' in "abc.rb.bak") until a match is
	# found in $langParams or none left.
	loop
		if nil?(i = 1 => index(bufFile, ?., 'Char, Last'))

			# No filename extension.  Set shell mode if (1), filename begins with "makefile" (ignoring case); or
			# (2), filename has ".bak" extension or no extension, does not contain a space or period, and begins
			# with a lower case letter; and file contents match any "fingerprint" pattern (in "for" loop below).
			if isMode?('Shell') == 'buffer'
				str = bufFile0
				str[-4, 4] == '.bak' and str = str[0, -4]
				if index(lowerCaseStr(bufFile), 'makefile') == 0
					mode = 'Shell'
				elsif str == tr(str, '. ', '') && ord(str) == ord(lowerCaseStr str)

					# Have typical shell filename.  Check contents for shell syntactic element.
					setMark ?%
					beginBuf
					for pat in ['^\s*\bif \[ :re', '^\s*fi\b:re', '\b;;\b:re', '^\s*\bdone\b:re']
						found = searchForw(pat)
						delRingEntry 'Search'
						if found
							mode = 'Shell'
							break
						endif
					endloop
					-1 => gotoMark ?%
				endif
			endif
			break
		else
			# Get filename extension.
			fileExt = bufFile[i + 1,]
			bufFile = bufFile[0, i]

			# Set language mode for known file extensions.
			for node in $langParams
				key, modeDict = node
				if ord(key) != ?%
					if (extList = modeDict['FileExts']) != nil
						for knownExt in extList
							if fileExt == knownExt
								mode = modeDict['ModeName']
								break 3
							endif
						endloop
					endif
				endif
			endloop
		endif
	endloop

	# Set buffer mode, if applicable.
	mode && 1 => chgMode mode, $1
	nil
endroutine
setHook 'filename', hkFilename

# Set language mode from shebang line when a file is read info a buffer.  Argument(s):
#  1: [String] Buffer name.
#  2: [String] Filename.
function hkRead(2) {arguments: 'buffer, file', description: 'Set language mode when a file is read into a buffer.'}

	# Language mode already set in buffer?
	if not grpModeActive?($langParams['%ModeGroup'], $1)

		# No, check first line of buffer (file) for shebang.
		if line1 = bgets($1)
			beginBuf $1
			if line1[0, 3] == '#!/'

				# Shebang found.  Compare binary path against all known ones.
				for node in $langParams
					key, modeDict = node
					if ord(key) != ?%
						if !nil?(binList = modeDict['BinNames'])
							typeof(binList) == 'array' or binList = [binList]
							for binName in binList
								if index(line1, '/' & binName) != nil
									1 => chgMode modeDict['ModeName'], $1
									break 2
								endif
							endloop
						endif
					endif
				endloop
			endif
		endif
	endif
	nil
endroutine
setHook 'read', hkRead
