# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# memacs.ms		Ver. 10.0.0
#	MightEMacs startup file.

##### Global Variables #####
$lastFenceChar = ?"		# Last fence character used by "wrapWord" command.
$findParams = {			/# File search parameters and results. #/\
	'FindStacks' => [],\
	'Title' => "                    ~bF I L E   A N D   B U F F E R   F I N D   R E S U L T S~B\n\n",\
	'Header' =>\
	 '~b~rLevel~Z  ~b~r           Search Patterns           ~Z  ~b~r                  Buffers Found                   ~Z',\
	'None' => "\n\n\n       (No results found)",\
	'SepLine' => '-----  -------------------------------------  --------------------------------------------------',\
	'EntryFmt' => "\n%3d    %-37s  %2u: %s"\
	}

##### Commands and Functions #####

# Display MightEMacs cheat sheet on separate screen.
command showCheatSheet(0) {description: 'Display %s cheat sheet on a separate screen (or return to previous screen if it is\
 already being displayed).' % getInfo('Editor')}
	name = 'CheatSheet'

	# Displaying cheat sheet?
	if $bufname == name
		restoreScreen
	else
		# Read file if needed.
		if !isBuf?(name)
			if (path = 1 => xPathname('memacs', 'SkipNull')) &&\
			 stat? (path = sprintf('%s/../help/%s', 1 => dirname(path), name)), 'f'
				0 => readFile path
			else
				return 1 => message 'Fail, High', 'Cheat sheet not found "', path, '"'
			endif
		else
			# Find screen displaying buffer and switch to it.
			for windInfo in getInfo('Windows')
				screenNum, windNum, bufname = windInfo
				if bufname == name
					saveScreen
					selectScreen screenNum
					return
				endif
			endloop
		endif

		# Buffer not being displayed... create a screen for it.
		saveScreen
		selectScreen 0
		selectBuf name
		setBufFile nil, name
		1 => chgBufAttr 'Hidden, ReadOnly, TermAttr', name
		beginBuf
		1 => message 'TermAttr', 'Type ~#u%s~U again to return to previous screen' % eval('bindingsof ', RunName)[0]
	endif
endroutine

# Set up tutorial.
command openTutor(0) {description: join(' ', 'Open', getInfo('Editor'), 'Tutorial.')}
	if !isConstVar? 'PlaceMarker'
		PlaceMarker = '%&'
		UserTutorFile = expandPath('~/%sTutorial' % getInfo('Editor'))
	endif

	# Determine if beginning, ending, or resuming tutorial and prompt user for confirmation.
	if (ending = bufInfo($bufname)['Filename'] == UserTutorFile)
		p = (modified = bufAttr?('Changed')) ? 'Save and exit' : 'Exit'
	else
		p = (resume = stat?(UserTutorFile, 'e')) ? 'Resume' : 'Begin'
	endif
	if prompt('%s tutorial? (y/n)' % p, 'Type: Char', ?n) != ?y
		return
	endif

	# Action confirmed.  Process it.
	if ending
		insert PlaceMarker, $windLineNum, PlaceMarker
		saveFile
		bufname = $bufname
		restoreBuf
		delBuf bufname
		1 => message 'Force, TermAttr', 'Tutorial saved.  To exit the editor, press ~#u', bindingsof(exit)[0], '~U'
	else
		if !resume
			if isConstVar? 'StockTutorFile'
				if !stat?(StockTutorFile, 'f')
					return 1 => message 'Fail, High', 'Master tutorial file not found'
				endif
			else
				# Find tutorial file.
				isConstVar?('TutorFile') or TutorFile = 'Tutorial'
				if (path = 1 => xPathname('memacs', 'SkipNull')) &&\
				 stat? (path = sprintf('%s/../help/%s', 1 => dirname(path), TutorFile)), 'f'
					StockTutorFile = pathname(path)
				else
					return 1 => message 'Fail, High', 'Master tutorial file not found'
				endif
			endif
		endif

		# If currently in the Help System, go back to previous screen.
		if isConstVar?('HelpInfo') &&\
		 (index($bufname, HelpInfo[0][0]) == 0 || index($bufname, HelpInfo[1][0]) == 0)
			restoreScreen
		endif

		# Save current buffer so it can be returned to later.
		saveBuf

		# Open tutorial in current window and set it up.
		findFile UserTutorFile
		if resume
			if searchForw PlaceMarker
				length(PlaceMarker) => delBackChar
				setMark
				endWord
				windLineNum = toint(RegionText)
				length(RegionText) => delBackChar
				length(PlaceMarker) => delForwChar
				onlyWind
				windLineNum => reframeWind
				-1 => chgBufAttr 'Changed', $bufname
			else
				onlyWind
			endif
			delRingEntry 'Search'
		else
			onlyWind
			0 => insertFile StockTutorFile
			writeFile UserTutorFile
		endif
		1 => message 'Force', 'Tutorial ', resume ? 'resumed' : 'started'
	endif
endroutine

# Delete patterns from search ring until given pattern is on top.
function restoreSearchRing(1)
	while !null?($searchPat) && $searchPat != $1
		delRingEntry 'Search'
	endloop
endroutine

# Unchange current buffer.
command unchangeBuf(0) {description: 'Clear the "changed" attribute of the current buffer.'}
	-1 => chgBufAttr 'Changed', $bufname
endroutine

# Trim all lines in current buffer.
command trimBuf(0) {description: 'Trim white space from end of all lines in current buffer.'}
	markBuf
	oldMsgState = 1 => chgMode('RtnMsg')
	0 => trimLine
	oldMsgState => chgMode 'RtnMsg'
	gotoMark ?`
endroutine

# Enumerate a line block.
command enumLine(0) {description: 'Insert sequential numbers at beginning of all lines in a block.'}
	run seti
	lineCt = selectText($0)				# Get line count and position point.
	while lineCt--
		inserti
		2 => beginLine
	endloop
	nil
endroutine

# Pop a file listing.
command popFileList(0) {description: "Display a file listing in a pop-up window in long form.  User is prompted for zero or\
 more shell glob pattern(s), and zero or more of the following options which modify the default listing format:\n\
\tAllFiles\tInclude files that begin with a period (.).\n\
\tNoExpand\tDo not expand directories that match the shell template.\n\
\tMultiCol\tDisplay in multiple columns (condensed form)."}
	lsSwitches1 = ''		# Do not include hidden files.
	lsSwitches2 = ''		# Expand directories.
	lsSwitches3 = 'l'		# Long form.

	# Get glob pattern.
	globPat = prompt('ls', 'Type: File')
	if nil?(globPat) || globPat == '.'
		globPat = ''
	endif

	# Get options.
	if !empty?(options = prompt('Options (~b~uA~ZllFiles, No~b~uE~Zxpand, Multi~b~uC~Zol)', 'TermAttr'))
		if (optionsLC = lowerCaseStr(options)) !~ '^[aec]+$'
			return 1 => message 'Fail', 'Invalid options "', options, '"'
		endif

		loop
			option = strShift(optionsLC, nil)
			if option == 'a'
				lsSwitches1 = 'a'	# Include hidden files.
			elsif option == 'e'
				lsSwitches2 = 'd'	# Do not expand directories.
			else	# option == 'c'
				lsSwitches3 = 'CF'	# Short form.
			endif
			if empty? optionsLC
				break
			endif
		endloop
	endif

	# Pop file listing.
	1 => shellCmd 'Shift', 'ls -', lsSwitches1, lsSwitches2, lsSwitches3, ' ', globPat
endroutine

# Return find dictionary for current screen, or nil if not found.
function getFindDict(0)
	findStacks = $findParams['FindStacks']
	length(findStacks) >= $screenNum && !empty?(screenStack = findStacks[$screenNum - 1]) ? screenStack[-1] : nil
endroutine

# Get a buffer list to process: either all visible buffers (default n or n > 0) or only those also in buffer list in "find
# files" hash for current screen (n <= 0).  In the latter case, update list in "find files" hash if it contains any buffers that
# no longer exist.  Returns: [Array]
#	0: [Boolean] All buffers selected?
#	1: [Array] Buffer list (names).
function getBufList(0) {description: "Get list of all visible buffers (or only those also in buffer list in $findParams for\
 current screen if n <= 0).\n\n\
Returns: false if error, otherwise array of [all-buffers-selected?, [bufname, ...]]."}
	if $0 == defn || $0 > 0
		return [true, getInfo('Buffers')]
	endif

 	# Restricted buffer list request.  Break out of loop and return "no list" error if have empty list at any time during
 	# list refinement.  Also, update list in dictionary if it contains any non-existent buffers.
	loop
		if nil?(findDict = getFindDict())			# Error if buffer list does not exist.
			break
		elsif empty?(bufList = findDict['BufList'])		# Error if array is empty.
			break
		else							# Not empty.  Remove any non-existent buffers.
			searchList = bufList * getInfo('Buffers')
			if size(searchList) != size(bufList)
				findDict['BufList'] = bufList = searchList
			endif
			if empty? searchList				# Error if empty search list.
				break
			endif
			return [false, bufList]				# Success!
		endif
	endloop
	1 => message 'Fail, High', 'No buffer list'
endroutine

# Get buffer list from "find files" hash for current screen and either switch to first or last buffer in that list, or switch to
# next or previous buffer n times.  Argument(s):
#  1: [Integer] Switch to first (0), last (1), previous (2), or next (3) buffer.
# If n < 0, delete current buffer after buffer switch and remove buffer name from list.
# Returns:
#	[String] Name of current buffer after switch(es) completed.
function selectListBuf(1)
	nukeBuf = nil

	# Does a non-empty buffer list exist in "find files" hash?
	if !(bufList = -1 => getBufList)
		return false
	elsif $0 == defn
		$0 = 1
	elsif $0 == 0
		return
	elsif $0 < 0
		# Delete current buffer after buffer switch.  Error if not in list.
		if ainclude? bufList[1], $bufname
			nukeBuf = $bufname
		else
			return 1 => message 'Fail, High', 'Current buffer not in buffer list'
		endif
	endif
	bufList = bufList[1]

	# Set scanning parameters such that i + incr yields index of next or previous buffer in list.
	if $1 <= 1
		incr = $1 == 0 ? 1 : -1
		i = $1 == 0 ? -1 : length(bufList)
		$0 = 1
	else
		incr = $1 == 3 ? 1 : -1
		if nil?(i = aindex(bufList, $bufname))
			i = $1 == 3 ? -1 : length(bufList)
		endif
	endif

	# Get index of next or previous buffer n times.
	loop
		i += incr
		if i == length(bufList)
			i = 0
		elsif i < 0
			i = length(bufList) - 1
		endif
		if nukeBuf || --$0 == 0
			break
		endif
	endloop

	# Switch to it.
	selectBuf bufList[i]

	# Delete buffer that was exited if requested.
	if nukeBuf
		if delBuf(nukeBuf) == 1

			# Buffer was deleted.  Remove it from the list if present.
			adeleteif bufList, nukeBuf
			message "Buffer '#{nukeBuf}' deleted"
		endif
	endif
	$bufname
endroutine

# Switch to first buffer in "find files" hash.
command firstListBuf(0) {description: "Switch to first buffer in list created from most recent ~bfindFiles~B invocation.  If\
 n < 0, also delete buffer that was exited from.\n\n\
Returns: name of buffer switched to."}
	$0 => selectListBuf 0
endroutine

# Switch to last buffer in "find files" hash.
command lastListBuf(0) {description: "Switch to last buffer in list created from most recent ~bfindFiles~B invocation.  If\
 n < 0, also delete buffer that was exited from.\n\n\
Returns: name of buffer switched to."}
	$0 => selectListBuf 1
endroutine

# Switch to previous buffer in "find files" hash n times.
command prevListBuf(0) {description: "Switch to previous buffer in list created from most recent ~bfindFiles~B invocation n\
 times.  If n < 0, switch once and delete buffer that was exited from.\n\n\
Returns: name of current buffer after switch(es) completed."}
	$0 => selectListBuf 2
endroutine

# Switch to next buffer in "find files" hash n times.
command nextListBuf(0) {description: "Switch to next buffer in list created from most recent ~bfindFiles~B invocation n times. \
 If n < 0, switch once and delete buffer that was exited from.\n\n\
Returns: name of current buffer after switch(es) completed."}
	$0 => selectListBuf 3
endroutine

# Get buffer list from "find files" hash for current screen, delete all buffers in list not being displayed (if possible), and
# remove each name from list.  Return number of buffers deleted.
command delListBufs(0) {description: "Delete all buffers in list created from most recent ~bfindFiles~B\
 invocation that are not being displayed on current screen.\n\n\
Returns: number of buffers deleted."}

	# Is buffer list in "find files" hash and not empty?
	if nil?(findDict = getFindDict()) || empty?(bufList = findDict['BufList'])
		return 1 => message 'Fail, High', 'No buffer list'
	endif

	# Loop through the list and delete buffers not being displayed.
	count = 0
	newBufList = []
	for bufname in bufList
		if nil?(bufWind bufname) && delBuf(bufname) == 1
			++count
		else
			apush newBufList, bufname
		endif
	endloop
	findDict['BufList'] = newBufList
	1 => message 'High', sprintf("%u buffer%s deleted", count, count == 1 ? '' : 's')
	count
endroutine

# Wrap word(s); that is, put punctuation or fence characters around one or more words at point.  Argument(s):
#  1: [Char] Fence character.
command wrapWords(1) {arguments: 'c', description: 'Wrap a pair of quotes, fences ( ), [ ], { }, < >, or punctuation characters\
 around [-]n words (default 1) at point.  Negative n selects word(s) backward and positive n selects word(s) forward.  If\
 interactive, prompt for wrap character; otherwise, use character c.'}

	# Determine left and right fences.
	if !interactive?
		wrapChar = $1
	elsif empty?(wrapChar = prompt('Wrap or fence char', 'Type: Char', $lastFenceChar))
		return nil
	endif

	# It's a go...
	loop
		# Move point to starting point.
		$0 == defn || $0 == 0 and $0 = 1
		setMark ??
		if isClass? 'word', $lineChar
			if $0 < 0
				status = endWord
				--$0
			else
				status = ($lineOffset == 0 || !isClass?('word', subline -1, 1)) ? true : backWord
			endif
		elsif $0 > 0
			status = forwWord
		elsif((status = backWord) != false)
			status = endWord
		endif

		# Any word(s) in buffer?
		if status == false
			1 => message 'Fail, High', 'No word found'
			break
		endif

		# Point is now at starting point.  Determine left and right fences.
		leftFence = rightFence = nil
		for rightFence in ['()', '[]', '{}', '<>']
			if index(rightFence, wrapChar, 'Char') != nil
				leftFence = ord(strShift rightFence, nil)
				rightFence = ord(rightFence)
				break
			endif
		endloop
		if nil? leftFence
			# If fence is a punctuation character, just double it up.
			if wrapChar > ?\s && wrapChar <= ?~ && (wrapChar == ?_ || !isClass?('word', wrapChar))
				leftFence = rightFence = wrapChar
			else
				status = 1 => message 'Fail, High', "Invalid wrap character '", chr(wrapChar), "'"
				break
			endif
		endif

		# Save wrap character that was entered if interactive.
		interactive? and $lastFenceChar = wrapChar

		# Try to move to other end.
		setMark ?_
		if ($0 < 0 ? abs($0) => backWord : $0 => endWord) == false
			status = 1 => message 'Fail, High', 'Too many words to wrap'
			delMark ?_
			break
		endif

		# All is well.  Insert left or right fence, move back to starting point and insert other fence.
		if $0 < 0
			insert chr leftFence
			-1 => gotoMark ?_
			insert chr rightFence
			delMark ??
		else
			insert chr rightFence
			setMark ??
			-1 => gotoMark ?_
			insert chr leftFence
			-1 => gotoMark ??
		endif
		return true
	endloop

	# Error or user cancelled.
	-1 => gotoMark ??
	status
endroutine

# Process a buffer list.  Argument(s):
#  1: [Array] Buffer list.
#  2: [String] Name of user function to invoke on each buffer.
#  3... Optional argument(s) to pass to function.
# Prompt user to continue or quit before each buffer is processed.  If user selects "do rest", remaining buffers are processed
# without prompting.  The given function (1), must operate non-interactively if n > 0; and (2), may return false and set an
# exception message if an error occurs, or an informational message (which is typically a blurb about what happened, like "3
# substitutions"), or nil (no message).
# Returns:
#	False (and sets message) if an error occurs, otherwise true.
function doBufList(2,) {arguments: 'list, function[, ...]', description: "Invoke function interactively (with specified\
 arguments, if any) on given buffer list.\n\n\
Returns: false if error occurs, otherwise true."}

	# Do sanity checks.
	if empty?($1)
		return 1 => message 'Fail, High', 'Empty buffer list'
	elsif isExec?($2) != 'user function'
		return 1 => message 'Fail, High', "No such user function '", $2, "'"
	endif

	bufList = aclone(ashift ArgList)
	funcName = ashift(ArgList)

	# Build argument list.
	argList = ''
	for arg in ArgList
		strPush argList, ', ', quote arg
	endloop

	# If current buffer is in list, cycle buffer list so it's in the front.
	if ainclude? bufList, $bufname
		tempList = []
		while (bufname = ashift(bufList)) != $bufname
			apush tempList, bufname
		endloop
		bufList = [$bufname] & bufList & tempList
	endif

	# Save current buffer, position, and loop through buffers.
	origBuf = $bufname
	setMark ?%
	goBack = true
	opMode = defn
	procCount = 0
	returnMsg = nxtBuf = nil
	loop
		loop
			thisBuf = nxtBuf
			nxtBuf = ashift(bufList)
			termAttr = false

			# If first time through loop and first buffer to process is not original one, get user confirmation.
			if nil? thisBuf
				if nxtBuf == origBuf
					next
				endif
				msg = 'First'
			else
				# Switch to buffer and call processor.
				selectBuf thisBuf
				beginBuf
				returnMsg = eval(join ' ', opMode == defn ? 'defn' : opMode, '=>', funcName, argList)
				if returnMsg == false
					return false
				endif
				++procCount

				# Last buffer?
				if nil? nxtBuf
					break
				elsif opMode != 1

					# Not last buffer and interactive.  Build prompt with return message (if any).
					msg = returnMsg ? "#{returnMsg}.  Next" : 'Next'
					updateScreen()
				endif
			endif

			# If in interactive mode...
			if opMode != 1

				# Finish prompt and get confirmation to continue.
				msg = "'#{msg} buffer: '#{nxtBuf}'  Continue? "
	                        loop
	                        	reply = prompt(msg, termAttr ? 'Type: Char, TermAttr' : 'Type: Char')
					returnMsg = nil
					if reply == ?\s || reply == ?y
						break 1
					elsif reply == ?!
						opMode = 1
						break 1
					elsif reply == ?\e || reply == ?q
						goBack = false
						break 3
					elsif reply == ?. || reply == ?n
						break 2
					else
						msg = '"~uSPC~U|~uy~U ~bYes~B, ~u!~U ~bDo rest~B, ~uESC~U|~uq~U ~bStop here~B,\
 ~u.~U|~un~U ~bStop and go back~B, ~u?~U ~bHelp~B: '
						termAttr = true
						reply == ?? or beep
					endif
				endloop
			endif
		endloop

		# All buffers processed or "stop and go back".
		selectBuf origBuf
		gotoMark ?%
		break
	endloop
	delMark ?%

	# Return result.
	if !goBack
		clearMsgLine
	else
		opMode == 1 and returnMsg = '%u buffers processed' % procCount
		1 => message 'High, NoWrap', nil?(returnMsg) ? 'Done!' : '%s.  Done!' % returnMsg
	endif
	true
endroutine

# Perform replace (n > 0) or queryReplace (otherwise) on current buffer and return custom result message.  Called from
# queryReplaceAll command.
function queryReplaceOne(0)
	oldMsgState = 1 => chgMode 'RtnMsg'
	clearMsgLine
	if $0 > 0
		replace $searchPat, $replacePat
	elsif not queryReplace $searchPat, $replacePat
		oldMsgState => chgMode 'RtnMsg'
		return false
	endif
	oldMsgState => chgMode 'RtnMsg'
	(i = index(ReturnMsg, ?,, 'Char')) != nil ? ReturnMsg[0, i] : ReturnMsg
endroutine

# Query-replace multiple buffers (files).  Use buffer list in "find files" hash if n <= 0, otherwise all visible buffers.
command queryReplaceAll(0) {description: "Invoke ~bqueryReplace~B command on all visible buffers (or just those that matched\
 the most recent ~bfindFiles~B invocation if n <= 0).\n\n\
Returns: false if error occurs, otherwise true."}

	# Get list of buffers to search: either all visible buffers or only those also in buffer list in "find files" hash.
	if !(searchList = $0 => getBufList)
		return false
	endif
	whichBufs = ashift(searchList) ? 'all' : 'selected'
	searchList = ashift(searchList)

	# Prompt for search and replacement strings.
	for type in ["$searchPat:In #{whichBufs} buffers, query replace:SearchRing", '$replacePat:with:ReplaceRing']
		srVar = strShift(type, ?:)
		newValue = strShift(type, ?:)
		newValue = prompt(newValue, sprintf('Type: %s, Delim: %s', type, $searchDelim), eval(srVar))
		if empty?(newValue) && srVar == '$searchPat'		# Nil or null string entered for $searchPat?
			return						# Yes, bail out.
		endif
		eval join ' ', srVar, '=', quote newValue		# No, assign new value.
	endloop

	# Process buffers.
	doBufList searchList, 'queryReplaceOne'
endroutine

# Pop find dictionary from current screen's stack in $findParams hash and restore previous search pattern if available.
command popFindList(0) {description: 'Pop buffer list associated with the previous ~bfindFiles~B invocation from stack and\
 restore associated search pattern if available.'}
	findStacks = $findParams['FindStacks']
	loop
		if length(findStacks) >= $screenNum
			if empty?(screenStack = findStacks[$screenNum - 1])
				break
			endif
			findDict = apop(screenStack)
			if !nil?(searchPat = findDict['SearchPat'])
				$searchPat = searchPat
				delRingEntry 'Search'
				if !empty?(screenStack) && !nil?(searchPat = screenStack[-1]['SearchPat'])
					$searchPat = searchPat
				endif
			endif
			return message "Buffer list popped: search pattern now '%s'" % $searchPat
		endif
		break
	endloop
	1 => message 'Fail, High', 'No buffer list found to pop'
endroutine

# Display "find" stacks in pop-up window (or just the stack for the current screen if n <= 0).
command showFindStacks(0) {description: 'Display ~bfindFiles~B stacks for all screens in pop-up window (or just the stack for\
 the current screen if n <= 0).'}

# $findParams	:=	{'FindStacks' => find-stacks, 'None', 'SepLine', 'EntryFmt'}	# "Find files" parameters.
# find-stacks	:=	[screen-stack, ...]						# One stack per screen.
# screen-stack	:=	[find-dict, ...]						# Zero or more find dictionaries.
# find-dict	:=	{'GlobPat', 'AproposPat', 'SearchPat', 'ExcludePat',
#			 'BufList' => [bufname, ...]}					# Search patterns and results.

	0 => selectBuf(bufname = '.FindStacks')
	-1 => clearBuf bufname
	none = true
	if !empty? (findStacks = $findParams['FindStacks'])
		for screenStack in findStacks
			if !empty? screenStack
				none = false
				break
			endif
		endloop
	endif
	if none
		bprint bufname, $findParams['None']
	else
		bprint bufname, $findParams['Title'], $findParams['Header']
		screenCount = ScreenCount
		sepLine = $findParams['SepLine']
		entryFmt = $findParams['EntryFmt']
		screenNum = 0
		while screenNum < screenCount
			++screenNum
			bprintf bufname, "\n%s", sepLine
			if screenCount > 1
				bprintf(bufname, "\nSCREEN %u", screenNum)
			endif
			if length(findStacks) >= screenNum && !empty?(screenStack = findStacks[screenNum - 1])
				i = length(screenStack)
				n = 1
				loop
					findDict = screenStack[--i]
					patterns = [findDict['GlobPat'], findDict['AproposPat'], findDict['SearchPat'],\
					 findDict['ExcludePat']]
					bufList = findDict['BufList']
					bprintf bufname, entryFmt, --n, tostr(patterns, 'ShowNil, Quote2, Delim'),\
					 length(bufList), tostr(bufList, 'Delim')
					if i == 0
						break
					endif
				endloop
			endif
		endloop
	endif

	-1 => chgBufAttr 'Changed', bufname
	popBuf bufname, 'Delete, TermAttr'
endroutine

# Scan files that match given glob pattern in given directory.  Any file that cannot be read (because of permissions, for
# example) is ignored.  Arguments:
#  1: [String] Directory.
#  2: [String] File glob pattern.
#  3: [String] Search pattern.
function locateFile(3) {arguments: 'dir, glob, pat', description: "Find first file matching glob pattern in given directory\
 that contains search pattern.\n\n\
Returns: false if no file found, otherwise a two-element array containing:\
 buffer name of first file that contained the search pattern, and either the mark that was set at the position where the\
 pattern was found (in existing buffer), or nil (if buffer was created)."}

	# Prepare for search in new screen.
        $searchPat = $3
	saveScreen
	selectScreen 0
	startBuf = $bufname
	bufname = mark = nil

	# Get file list for glob pattern and scan the files (if any).
	for filename in glob($1 == '.' ? $2 : join('/', $1, $2))
		force bufname, isNewBuf = findFile(filename)
		if nil? bufname

			# File read failed... skip it.
			printf 'Cannot read file "%s" - skipping...', filename
			pause 1
			if $bufFile == filename && bempty?
				bufname = $bufname
				selectBuf startBuf
				delBuf bufname
			else
				selectBuf startBuf
			endif
		else
			isNewBuf or setMark ?`
			beginBuf
			if huntForw
				# Found!
				beginLine
				isNewBuf or setMark(mark = ?!)
				break
			endif

			if isNewBuf
				selectBuf startBuf
				delBuf bufname
			else
				gotoMark ?`
				selectBuf startBuf
			endif
		endif
		bufname = nil
	endloop

	# Clean up.
	delRingEntry 'Search'
	screenNum = $screenNum
	restoreScreen
	delScreen screenNum

	# Return result if search string found, otherwise false.
	nil?(bufname) ? 1 => message('Fail, High', 'Not found') : [bufname, mark]
endroutine

# Find buffers in given list matching an apropos or search pattern and return them.  Argument(s):
#  1: [String] Buffer list.
#  2: [String] Apropos or search pattern, or nil for all.
#  3: [Boolean] Match buffer names with apropos pattern if true; otherwise, match buffer contents with search pattern.
# Returns:
#	[Array] Buffer names that matched.
function matchBuffers(3) {arguments: 'buf-list, pat, match-bufname', description: "Find buffers in given array whose names\
 match given apropos pattern (if match-bufname is true), or whose contents match given search pattern (if match-bufname is\
 false).  If an element in buf-list is an array, it is assumed to be of form [bufname, created?]; otherwise, it is assumed to\
 be a buffer name.  If pat is nil or null, all buffers are considered to be a match.\n\n\
Returns: array of matching buffer\
 names."}
	if empty?($2) && (empty?($1) || typeof($1[0]) == 'string')
		$1
	else
		bufList = []

		if $3
			# Find matching buffer names.  Add ":i" (ignore case) to pattern if no option characters specified.
			if !empty?($2) && $2 !~ '^.+:[eifmpr]+$'
				$2 &= ':i'
			endif

			for buf in $1
				bufname = typeof(buf) == 'array' ? buf[0] : buf
				if empty?($2) || !nil?(index bufname, $2)
					apush bufList, bufname
				endif
			endloop
		else
			# Search buffer contents.
			if !empty?($2)
				$searchPat = $2
				setMark ?_
				saveBuf
			endif
			for buf in $1
				bufname = typeof(buf) == 'array' ? buf[0] : buf
				if empty?($2)
					found = true
				else
					selectBuf bufname
					setMark ??
					beginBuf
					found = huntForw
					0 => gotoMark ??
				endif
				found && apush(bufList, bufname)
			endloop
			if !empty?($2)
				restoreBuf
				0 => gotoMark ?_
				delRingEntry 'Search'
			endif
		endif
		bufList
	endif
endroutine

# Open file(s) in background given space-separated glob pattern(s) argument, which is passed to "eval" command.  Argument(s):
#  1: [String] glob pattern(s).
# Returns:
#	Array of [bufname, created?] elements (which may be empty).
function openFiles(1) {arguments: 'glob-pats', description: "Open list of files matching space-separated shell glob pattern(s)\
 in background.\n\n\
Returns: nil if error occurs, otherwise array of [buffer-name, created?] elements (which may be empty)."}

	# Build "glob" command from pattern argument, which may contain multiple space-separated patterns and "\ " strings.
	result = []
	for pat in split(sub($1, '\ ', chr(1)), ?\s, -1)
		apush result, sub(pat, chr(1), '\\\\ ')
	endloop

	# Open the "regular" files (if any) that match the glob pattern(s) in the background.
	bufList = []
	for filename in eval 'glob ', tostr(result, 'Quote2, EscChar, Delim')
		if stat?(filename, 'f')
			apush bufList, 0 => findFile(filename)
		endif
	endloop

	# Return result.
	bufList
endroutine

# Find files or buffers that match a glob or apropos pattern and optionally, a search pattern, and open matching files in the
# background.  Save search parameters and resulting buffer list (array) in $findParams['FindStacks'] hash using current screen
# number - 1 as index.  ** Interactive only **
command findFiles(0) {description: "Find all files or buffers matching a glob pattern or apropos pattern and optionally, also\
 matching a search pattern, and open matching files in the background.  If n is not the default, user is prompted to select\
 zero or more of the following options:\n\
\tFindBuffers\t\tPrompt for a buffer apropos pattern instead of a filename glob pattern.\n\
\tExclude\t\t\tPrompt for an exclusion search pattern.\n\
\tPush\t\t\tPush current find results onto stack before starting new search (which can be popped later with the ~bpopFindList~B\
 command).\n\n\
If the \"FindBuffers\" option is not selected or n is the default, user is prompted for a filename glob pattern and a search\
 pattern.  If a search pattern is entered, only files matching the glob pattern that contain at least one occurrence of the\
 pattern are selected.  (However, if a file matching the glob pattern is already opened in the editor, its current buffer is\
 searched, not the file on disk.)  Conversely, if the \"FindBuffers\" option is selected, user is prompted for an apropos\
 pattern and a search pattern.  In this case, all visible buffers whose names match the apropos pattern and that contain at\
 least one occurrence of the search pattern are selected.  In either case, if the \"Exclude\" option is specified, the user is\
 prompted for a second search pattern.  All selected buffers that contain at least one occurrence of the exclusion pattern are\
 omitted.\n\n\
Note that the apropos pattern, search pattern, and/or exclusion pattern (but not the filename glob pattern) may be null, in\
 which case it has no effect on the buffer selection process (except that all visible buffers are selected if the apropos\
 pattern is null).  The names of the resulting buffers are saved in a global variable so that they can be subsequently searched\
 by the ~bqueryReplaceAll~B or ~bppDelPPLines~B command, stepped through with the ~bprevListBuf~B and ~bnextListBuf~B commands,\
 or deleted with the ~bdelListBufs~B command.\n\n\
Returns: true if any files or buffers were found, otherwise false."}

	# Get options if n argument.
	aproposBuf = excludeBuf = doPush = false
	if $0 != defn
		if empty?(options = prompt('Options (Find~b~uB~Zuffers, ~b~uE~Zxclude, ~b~uP~Zush)', 'TermAttr'))
			return
		else
			if (optionsLC = lowerCaseStr(options)) !~ '^[bep]+$'
				return 1 => message 'Fail', 'Invalid options "', options, '"'
			endif

			loop
				option = strShift(optionsLC, nil)
				if option == 'b'
					aproposBuf = true
				elsif option == 'e'
					excludeBuf = true
				else # option == 'p'
					doPush = true
				endif
				if empty? optionsLC
					break
				endif
			endloop
		endif
	endif

# $findParams	:=	{'FindStacks' => find-stacks, 'None', 'SepLine', 'EntryFmt'}	# "Find files" parameters.
# find-stacks	:=	[screen-stack, ...]						# One stack per screen.
# screen-stack	:=	[find-dict, ...]						# Zero or more find dictionaries.
# find-dict	:=	{'GlobPat', 'AproposPat', 'SearchPat', 'ExcludePat',
#			 'BufList' => [bufname, ...]}					# Search patterns and results.

	# Set up stack for current screen, which is an array containing a hash dictionary (findDict) for each find request
	# that is pushed onto it.  Each dictionary contains all search parameters and results (with keys above).  The dictionary
	# at the top of the stack is the current one and is overwritten by this routine unless a push is requested.
	findStacks = $findParams['FindStacks']
	if length(findStacks) < $screenNum || nil?(screenStack = findStacks[$screenNum - 1])
		findStacks[$screenNum - 1] = screenStack = []
	endif
	findDict = doPush || empty?(screenStack) ? {} : screenStack[-1]
	newDict = empty?(findDict)

	# Get ready.
	if aproposBuf
		key = 'AproposPat'
		otherKey = 'GlobPat'
		promptStr = 'Apropos bufname'
		type = 'String'
	else
		key = 'GlobPat'
		otherKey = 'AproposPat'
		promptStr = 'Space-separated glob pattern(s)'
		type = 'File'
	endif

	# Prompt for filename glob pattern(s) or apropos pattern, using last one as default.
	pat = prompt(promptStr, 'Type: %s' % type, findDict[key])
	if empty?(pat) && !aproposBuf
		return
	endif
	findDict[key] = pat
	newDict and findDict[otherKey] = nil

	# Prompt for search pattern.
	findDict['SearchPat'] = searchPat =\
	 prompt('Search pattern', 'Type: SearchRing, Delim: %s' % $searchDelim, findDict['SearchPat'])

	# Find files and buffers.  Break out of loop if none found.
	loop
		if aproposBuf
			# Find buffers whose names match apropos pattern.  bufList contains: [bufname, ...]
			bufList = getInfo('Buffers')
			if !empty?(pat = findDict['AproposPat']) && empty?(bufList = matchBuffers(bufList, pat, true))
				break
			endif
		else
			# Open files that match glob pattern(s) in background.  bufList contains: [[bufname, created?], ...]
			if empty?(globList = openFiles(findDict['GlobPat']))
				break
			endif
			bufList = globList
		endif

		# If a search pattern was specified, scan for matching buffers.
		bufList = matchBuffers(bufList, searchPat, false)

		# Prompt for exclusion search pattern if requested, using last one as default.
		if excludeBuf
			pat = findDict['ExcludePat']
			if !empty?(findDict['ExcludePat'] = pat = prompt('Exclude',\
			 'Type: SearchRing, Delim: %s' % $searchDelim, pat)) &&\
			 !empty?(xBufList = matchBuffers(bufList, pat, false))

				# Exclusion buffers found.  Remove them from match list.
				bufList -= xBufList
			endif
		elsif newDict
			findDict['ExcludePat'] = nil
		endif

		# If a search pattern was entered, add it to search ring for user's convenience.
		if !empty?(searchPat)
			$searchPat = searchPat
		endif

		# Lastly, if file search was performed, remove buffers that were created but not selected.
		if !aproposBuf
			for bufRec in globList
				if bufRec[1] && !ainclude?(bufList, bufRec[0])
					delBuf bufRec[0]
				endif
			endloop
		endif

		# Done.  Remember patterns and buffer list and report results.
		if newDict
			apush screenStack, findDict
		endif
		findDict['BufList'] = bufList
		if((count = length(bufList)) == 0)
			break
		endif
		itemList = ''
		for bufname in bufList
			strPush itemList, ', ', aproposBuf ? bufname : basename(bufInfo(bufname)['Filename'])
		endloop
		return 1 => message 'Force',\
		 sprintf('%u %s%s found: %s', count, aproposBuf ? 'buffer' : 'file', count == 1 ? '' : 's', itemList)
	endloop

	1 => message 'Fail, High', 'Not found'
endroutine

# Find a file and open it in a new screen associated with a specified directory.  If n <= 0, set its buffer to read-only.
command getFile(0) {description: 'Find a file and open it in a new screen associated with a specified directory. \
 If n <= 0, also set its buffer to read-only.'}
	saveScreen

	# Prompt for directory.
	if (dir = prompt('In directory', 'Type: File')) != nil
		oldMsgState = 1 => chgMode('RtnMsg')
		selectScreen 0
		print '[%s]' % ReturnMsg
		pause 1
		oldMsgState => chgMode 'RtnMsg'
		clearMsgLine
		chdir dir

		# Prompt for filename.
		if $0 == defn || $0 > 0
			prmt = 'Find file'
			cmd = 'findFile'
		else
			prmt = 'View file'
			cmd = 'viewFile'
		endif
		if nil?(filename = prompt(prmt, 'Type: File'))
			restoreScreen
		else
			eval cmd, ' "', filename, '"'
		endif
	endif
endroutine

# Cycle delete ring.
command cycleDeleteRing(0) {description: 'Cycle delete ring [-]n times (default 1).'}
	1 => chgMode 'RtnMsg'
	$0 => cycleRing 'Delete'
endroutine

# Cycle kill ring.
command cycleKillRing(0) {description: 'Cycle kill ring [-]n times (default 1).'}
	1 => chgMode 'RtnMsg'
	$0 => cycleRing 'Kill'
endroutine

# Cycle replace ring.
command cycleReplaceRing(0) {description: 'Cycle replace ring [-]n times (default 1).'}
	1 => chgMode 'RtnMsg'
	$0 => cycleRing 'Replace'
endroutine

# Cycle search ring.
command cycleSearchRing(0) {description: 'Cycle search ring [-]n times (default 1).'}
	1 => chgMode 'RtnMsg'
	$0 => cycleRing 'Search'
endroutine

# Delete entri(es) from delete ring.
command delDeleteEntry(0) {description: 'Delete the most recent n entries (default 1) from delete ring (or entry number n if\
 n < 0, all entries if n == 0).'}
	1 => chgMode 'RtnMsg'
	$0 => delRingEntry 'Delete'
endroutine

# Delete entri(es) from kill ring.
command delKillEntry(0) {description: 'Delete the most recent n entries (default 1) from kill ring (or entry number n if\
 n < 0, all entries if n == 0).'}
	1 => chgMode 'RtnMsg'
	$0 => delRingEntry 'Kill'
endroutine

# Delete entri(es) from replace ring.
command delReplaceEntry(0) {description: 'Delete the most recent n pattern entries (default 1) from replace ring (or entry\
 number n if n < 0, all patterns if n == 0).  The most recent remaining entry is subsequently set as the current replace\
 pattern.'}
	1 => chgMode 'RtnMsg'
	$0 => delRingEntry 'Replace'
endroutine

# Delete entri(es) from search ring.
command delSearchEntry(0) {description: 'Delete the most recent n pattern entries (default 1) from search ring (or entry number\
 n if n < 0, all patterns if n == 0).  The most recent remaining entry is subsequently set as the current search pattern.'}
	1 => chgMode 'RtnMsg'
	$0 => delRingEntry 'Search'
endroutine

# Display delete ring.
command showDeleteRing(0) {description: "Generate list of delete ring entries in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).\n\n\
Returns: ~bselectBuf~B values."}
	$0 => showRing 'Delete'
endroutine

# Display kill ring.
command showKillRing(0) {description: "Generate list of kill ring entries in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).\n\n\
Returns: ~bselectBuf~B values."}
	$0 => showRing 'Kill'
endroutine

# Display replace ring.
command showReplaceRing(0) {description: "Generate list of replace ring patterns in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).\n\n\
Returns: ~bselectBuf~B values."}
	$0 => showRing 'Replace'
endroutine

# Display search ring.
command showSearchRing(0) {description: "Generate list of search ring patterns in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).\n\n\
Returns: ~bselectBuf~B values."}
	$0 => showRing 'Search'
endroutine

# Display man page for word at point.
command showMan(0) {description: 'Display man page for word at point.'}
	if nil?(word = getWord)
		1 => message 'Fail, High', 'Not in a word'
	else
		cols, rows = TermSize
		1 => shellCmd 'NoHdr', 'MANWIDTH=', cols, ' man ', word, ' | col -b'
	endif
endroutine

# Join line(s) with no spacing.
command joinLines0(0) {description: 'Join [-]n lines (default -1) with no spacing in between.'}
	$0 => joinLines nil
endroutine

# Add key to report buffer, if applicable.  Called from "showBindings" command.  Argument(s):
#  1: [String] Buffer name.
#  2: [String] Key sequence prefix (e.g., 'C-h ') or nil.
#  3: [Char] Raw 8-bit key.
#  4: [String] Raw key in display form.
#  5: [String] Apropos pattern.
function addKey(5)
	keyLit = tostr($2) & $4
	if nil?($5) || index(keyLit, $5) != nil
		if nil?(cmdname = binding(keyLit))
			bprint $1, keyLit, $3 == ?~ ? "~\n" : "\n"
		else
			if $3 == ?~
				pad = 14
				keyLit &= '~'
			else
				pad = 13
			endif
			prefix = isExec?(cmdname)	# cmdname can be "(self insert)".
			prefix = nil?(prefix) || nil?(index(prefix, 'user')) ? ' ' : '@'
			bprintf $1, "%-*s%s%s\n", pad, keyLit, prefix, cmdname
		endif
	endif
endroutine

# Create and pop a "help file not found" buffer.  Argument(s):
#  1: Filename.
function hsNotFound(1)

	# Main script file not found.  Give user the bad news.
	1 => chgBufAttr 'Hidden, TermAttr', (bufname = (0 => scratchBuf)[0])
	bprint bufname, "~bHelp files not found!~B\n\n\nFile \"", $1, "\" was not found in $execPath:\n    ",\
	 $execPath, "\n\nPlease check your shell startup script.  MMPATH may need to be set.\nConsult file ",\
	 '"Install.txt" for more information.'
	bprint bufname, "\n\n\n\n\n\nPress ~uq~U (quit) to dismiss this window.\n\nAfter returning to your",\
	 " editing session, you can enter ~#uESC e~U to exit\n", getInfo('Editor'), ' if desired (press the ~uesc~U',\
	 ' key, then ~ue~U).'
	-1 => chgBufAttr 'Changed', bufname
	popBuf bufname, 'Delete'
	nil
endroutine

# Offer help options.
command help(0) {description: 'Enter Help System (or Advanced Help System if n argument).'}
	if !isConstVar? 'HelpSys'
		HelpSys = 'help.ms'
		MainMenu = 'hsMain'
	endif

	# Load help system if needed.
	if !isExec?(MainMenu) && (!(helpPath = xPathname(HelpSys)) || !require(helpPath))
		return hsNotFound HelpSys
	endif

	# Enter help system.
	eval '$0 => %s' % MainMenu
endroutine

##### Bindings and Aliases #####
bindKey 'ESC -', delListBufs
bindKey 'ESC #', enumLine
bindKey 'ESC C-w', wrapWords
bindKey 'C-x f', findFiles
bindKey 'C-c ^', firstListBuf
bindKey 'C-x C-g', getFile
bindKey 'ESC ?', help
bindKey 'C-x C-j', joinLines0
bindKey 'C-c $', lastListBuf
bindKey 'C-c ]', nextListBuf
bindKey 'C-h t', openTutor
bindKey 'C-h l', popFileList
bindKey 'C-c -', popFindList
bindKey 'C-c [', prevListBuf
bindKey 'ESC C-q', queryReplaceAll
bindKey 'C-h c', showCheatSheet
bindKey 'C-h F', showFindStacks
bindKey 'C-h N', showMan
bindKey 'ESC C-\\', trimBuf
bindKey 'ESC ~', unchangeBuf

bindKey 'C-c d', cycleDeleteRing
bindKey 'C-c k', cycleKillRing
bindKey 'C-c r', cycleReplaceRing
bindKey 'C-c s', cycleSearchRing
bindKey 'C-x d', delDeleteEntry
bindKey 'C-x k', delKillEntry
bindKey 'C-x r', delReplaceEntry
bindKey 'C-x s', delSearchEntry
bindKey 'C-h d', showDeleteRing
bindKey 'C-h k', showKillRing
bindKey 'C-h R', showReplaceRing
bindKey 'C-h s', showSearchRing

##### Load programming language libraries.
if (path = xPathname('lang/lang'))
	require path
endif

##### Load site-wide preferences.
if (path = xPathname('site'))
	require path

	# Create "word processing" line-join commands if EndSentence variable not empty.
	if !empty?(EndSentence)

		# Join line(s) with extra spacing.
		command wpJoinLines(0) {description: 'Run the ~bjoinLines~B command with the EndSentence variable as an\
 argument to obtain extra spacing between joined lines.'}
			$0 => joinLines EndSentence
		endroutine

		# Wrap line(s) with extra spacing.
		command wpWrapLine(0) {description: 'Run the ~bwrapLine~B command with the EndSentence variable as an\
 argument to obtain extra spacing between lines when the line block is rewrapped.'}
			$0 => wrapLine nil, EndSentence
		endroutine

		bindKey 'C-c C-j', wpJoinLines
		bindKey 'C-c RTN', wpWrapLine
	endif
endif

##### Set help message if no command-line arguments were specified.
if length(ArgList) == 0
	helpBinding = bindingsof(help)[0]
	quitBinding = bindingsof(exit)[0]
	1 => message 'High, TermAttr', sprintf('Press ~#u%s~U or ~uhelp~U key for help, ~#u%s~U to quit',\
	 helpBinding, quitBinding)
endif

# Bind "help" key here so mysterious function key binding doesn't end up in message above.
bindKey 'FNL', help
