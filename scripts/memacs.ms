# (c) Copyright 2020 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# memacs.ms		Ver. 9.5.1
#	MightEMacs startup file.

##### Global Variables #####
$lastFenceChar = ?"		# Last fence character used in "fenceWord" command.

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
		if defined?('Name', name) != 'buffer'
			if (path = 1 => xPathname('memacs', 'SkipNull')) &&\
			 stat? (path = sprintf('%s/../help/%s', 1 => dirname(path), name)), 'f'
				0 => readFile path
				1 => chgBufAttr name, 'Hidden, ReadOnly, TermAttr'
			else
				return 1 => message 'Fail, High', 'Cheat sheet not found "', path, '"'
			endif
		else
			# Find screen displaying buffer and switch to it.
			for windInfo in 1 => getInfo('Windows')
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
		setBufFile name, nil
		gotoLine 1
		1 => message 'TermAttr', 'Type ~#u%s~U again to return to previous screen' % binding('KeyList', $RunName)[0]
	endif
endroutine

# Set up tutorial.
command openTutor(0) {description: join(' ', 'Open', getInfo('Editor'), 'Tutorial.')}
	PlaceMarker = '%&'
	UserTutorFile = expandPath('~/%sTutorial' % getInfo('Editor'))

	# Determine if beginning, ending, or resuming tutorial and prompt user for confirmation.
	if (ending = bufInfo($bufname)[1] == UserTutorFile)
		p = (modified = bufAttr?($bufname, 'Changed')) ? 'Save and exit' : 'Exit'
	else
		p = (resume = stat?(UserTutorFile, 'e')) ? 'Resume' : 'Begin'
	endif
	if 1 => prompt('%s tutorial? (y/n)' % p, ?n, 'Type: Char') != ?y
		return
	endif

	# Action confirmed.  Process it.
	if ending
		insert PlaceMarker, $windLineNum, PlaceMarker
		saveFile
		bufname = $bufname
		restoreBuf
		delBuf bufname
		1 => message 'Force, TermAttr', 'Tutorial saved.  To exit the editor, press ~#u',\
		 binding('KeyList', 'exit')[0], '~U'
	else
		if !resume
			# Find tutorial file.
			TutorFile = 'Tutorial'
			if (path = 1 => xPathname('memacs', 'SkipNull')) &&\
			 stat? (path = sprintf('%s/../help/%s', 1 => dirname(path), TutorFile)), 'f'
				StockTutorFile = pathname(path)
			else
				return 1 => message 'Fail, High', 'Master tutorial file not found'
			endif
		endif

		# If currently in the Help System, go back to previous screen.
		if defined?('Name', '$HelpInfo') &&\
		 (index($bufname, $HelpInfo[0][0]) == 0 || index($bufname, $HelpInfo[1][0]) == 0)
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
				windLineNum = toInt($RegionText)
				length($RegionText) => delBackChar
				length(PlaceMarker) => delForwChar
				onlyWind
				windLineNum => reframeWind
				-1 => chgBufAttr $bufname, 'Changed'
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
	-1 => chgBufAttr $bufname, 'Changed'
endroutine

# Trim all lines in current buffer.
command trimBuf(0) {description: 'Trim white space from end of all lines in current buffer.'}
	markBuf
	oldMsgState = 1 => chgMode(nil, 'RtnMsg')
	0 => trimLine
	oldMsgState => chgMode nil, 'RtnMsg'
	gotoMark ?`
endroutine

# Enumerate a line block.
command enumLine(0) {description: 'Insert sequential numbers at beginning of all lines in a block.'}
	run seti
	lineCt = selectLine($0)				# Get line count and position point.
	while lineCt--
		inserti
		2 => beginLine
	endloop
	nil
endroutine

# Pop a file listing.
command popFileList(0) {description: 'Display a file listing in a pop-up window in long form.  User is prompted for the shell\
 glob pattern and zero or more of the following options which modify the default listing format: "AllFiles" -> include files\
 that begin with a period (.); "NoExpand" -> do not expand directories that match the shell template; and "MultiCol" -> display\
 in multiple columns (condensed form).'}
	lsSwitches1 = ''		# Do not include hidden files.
	lsSwitches2 = ''		# Expand directories.
	lsSwitches3 = 'l'		# Long form.

	# Get glob pattern.
	template = prompt('ls', 'Type: File')
	template = (nil?(template) || template == '.') ? '' : 2 => sub(template, ' ', '\ ')

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
	1 => shellCmd 'Shift', 'ls -', lsSwitches1, lsSwitches2, lsSwitches3, ' ', template
endroutine

# Rename a file on disk.
command renameFile(2, 3) {arguments: '{filename | bufname}, new-name[, opts]', description: 'Rename a file on disk.  If n\
 argument, one or both of the following comma-separated option(s) may be specified in string arguments opts: "FromBuffer" ->\
 use file associated with named buffer given as first argument as the one to rename; and "DiskOnly" -> if "FromBuffer"\
 specified, do not set the buffer\'s file to the new filename.  Case of option keywords is ignored.'}
	fromBuffer = diskOnly = false

	# Get options.
	if $0 != defn
		if !empty?(options = prompt('Options (From~b~uB~Zuffer, ~b~uD~ZiskOnly)', 'TermAttr'))
			if (optionsLC = lowerCaseStr(options)) !~ '^[bd]+$'
				return 1 => message 'Fail', 'Invalid options "', options, '"'
			endif

			loop
				option = strShift(optionsLC, nil)
				if option == 'b'
					fromBuffer = true
				else	# option == 'd'
					diskOnly = true
				endif
				if empty? optionsLC
					break
				endif
			endloop
		endif
	endif

	# Get old filename from buffer if applicable.
	if fromBuffer
		if interactive?
			if empty?(bufname = 1 => prompt('Rename file in buffer', $bufname, 'Type: Buffer'))
				return
			endif
		elsif type?(bufname = $1) != 'string'
			return 1 => message 'Fail, High', 'Invalid buffer name ', 1 => toStr(bufname, 'Visible, Quote2')
		endif

		# Have buffer name.  Get associated filename.
		if bufname == $bufname
			oldFilename = $bufFile
		elsif defined?('Name', bufname) == 'buffer'
			oldFilename = bufInfo(bufname)[1]
		else
			return 1 => message 'Fail, High', "No such buffer '", bufname, "'"
		endif

		if empty? oldFilename
			return 1 => message 'Fail, High', "No filename set in buffer '", bufname, "'"
		endif

	# Get disk filename.
	elsif interactive?
		if empty?(oldFilename = prompt('Rename disk file', 'NoAuto', 'Type: File'))
			return
		endif
	elsif type?(oldFilename = $1) != 'string'
		return 1 => message 'Fail, High', 'Invalid filename ', 1 => toStr(oldFilename, 'Visible, Quote2')
	endif

	# Have old filename.  Get new one.
	if interactive?
		if empty?(newFilename = 1 => prompt($0 == defn ? 'to' : diskOnly ? 'New disk filename' : 'New filename',\
		 oldFilename, 'Type: File', 'NoAuto'))
			return
		endif
	elsif type?(newFilename = $2) != 'string'
		return 1 => message 'Fail, High', 'Invalid filename ', 1 => toStr(newFilename, 'Visible, Quote2')
	endif

	# Rename file on disk if it exists.
	if (exists = stat?(oldFilename, 'f'))
		if not 1 => shellCmd('NoPop', 'mv ', shQuote(oldFilename), ' ', shQuote(newFilename))
			return false
		endif
	elsif $0 == defn || diskOnly
		return 1 => message 'Fail, High', 'No such file "', oldFilename, '"'
	endif

	# Rename buffer file, if applicable.
	fromBuffer && !diskOnly and setBufFile(bufname, newFilename)

	# Return result message.
	message (!exists ? 'File "%s" does not exist, buffer filename changed' % oldFilename :\
	 diskOnly ? 'Disk file renamed' : 'File renamed')
endroutine

# Delete a file on disk and optionally, an associated buffer.
command delFile(1) {arguments: '{bufname | filename}',\
 description: 'Delete a file on disk (or delete a buffer and its associated file if n argument).'}
	if $0 == defn
		# Get filename.
		if !interactive?
			filename = $1
		elsif empty?(filename = prompt('Delete file', 'NoAuto', 'Type: File'))
			return
		endif

		# Delete file.
		if not stat? filename, 'e'
			1 => message 'Fail, High', 'No such file "', filename, '"'
		else
			kind = stat?(filename, 'd') ? 'Directory' : 'File'
			if 1 => shellCmd 'NoPop', 'rm ', shQuote(filename)
				message kind, ' deleted'
			endif
		endif
	else
		# Get buffer name.
		if !interactive?
			bufname = $1
		elsif empty?(bufname = prompt('Buffer name', 'Type: Buffer'))
			return
		endif

		# Check if buffer exists.
		if defined?('Name', bufname) != 'buffer'
			return 1 => message 'Fail, High', 'No such buffer "', bufname, '"'
		endif

		# Have valid buffer name.  Get associated filename.
		filename = bufInfo(bufname)[1]

		# Try to delete buffer (which may fail).
		if delBuf(bufname) == 0
			return
		endif

		# Buffer deleted.  Now delete file if it exists.
		if !nil?(filename) && stat?(filename, 'e')
			if 1 => shellCmd 'NoPop', 'rm ', shQuote(filename)
				message 'Buffer and file "', filename, '" deleted'
			endif
		else
			message 'Buffer deleted'
		endif
	endif
endroutine

# Delete output file before write if file is a symbolic or hard link and user okays it, given (1), buffer name; and (2),
# filename.  This is done so that symbolic links will not be followed and hard links will be broken on update, which will
# effectively create a new file and preserve the original file.  Note that links will also be broken if 'safe' mode is enabled,
# so do nothing in that case.
function hkWrite(2) {description: 'Delete output file before write if symbolic or hard link and user okays it.'}
	if !mode?(nil, 'Safe')

		# Check if output file exists, is a symbolic or hard link, and is not in the $linksToKeep list.
		if stat?($2, 'Ll') && !include?($linksToKeep, filename = 0 => pathname($2))

			# Outfile is a link and not previously brought to user's attention.  Ask user if the link should be
			# broken.  If yes, delete the output file; otherwise, remember response by adding absolute pathname to
			# $linksToKeep.
			type = stat?(filename, 'L') ? 'symbolic' : 'hard'
			p = sprintf('Break %s link for file "%s" on output? (y, n)', type, $2)
			if 1 => prompt(p, ?n, 'Type: Char') == ?y
				1 => shellCmd 'NoPop', 'rm ', filename
			else
				push $linksToKeep, filename
			endif
		endif
	endif
endroutine
setHook 'write', hkWrite		# Set write hook.
$linksToKeep = []			# Link pathnames to leave in place, per user's request.

# Switch to next or previous visible buffer that has same home directory as current screen n times, given "forward?" argument.
# If n < 0, delete current buffer after buffer switch.  Return name of last buffer switched to.
function pnHomeBuf(1)
	nukeBuf = nil

	# Check n argument.
	if $0 == defn
		$0 = 1
	elsif $0 == 0
		return
	elsif $0 < 0
		nukeBuf = $bufname		# Delete current buffer after buffer switch.
		$0 = 1
	endif

	# Switch to next or previous "home" buffer n times.
	loop
		if nil?($1 ? 0 => nextBuf : 0 => prevBuf)
			return			# Only one "home" buffer.
		endif
		if --$0 == 0
			break
		endif
	endloop

	# Delete buffer that was exited if requested.
	if nukeBuf
		force delBuf nukeBuf
		if defined?('Name', nukeBuf) != 'buffer'
			message "Buffer '#{nukeBuf}' deleted"
		endif
	endif

	$bufname
endroutine

# Switch to next visible buffer that has same home directory as current screen n times.
command nextHomeBuf(0) {description: 'Switch to next visible buffer that has same home directory as current screen n times\
 (default 1, or switch once and delete buffer that was exited from if n < 0).  Returns: name of last buffer switched to, or nil\
 if no switch occurred.'}
	$0 => pnHomeBuf true
endroutine

# Switch to previous visible buffer that has same home directory as current screen n times.
command prevHomeBuf(0) {description: 'Switch to previous visible buffer that has same home directory as current screen n times\
 (default 1, or switch once and delete buffer that was exited from if n < 0).  Returns: name of last buffer switched to, or nil\
 if no switch occurred.'}
	$0 => pnHomeBuf false
endroutine

# Get buffer list from $bufListInfo for current screen and switch to next or previous buffer in that list n times, given
# "forward?" argument.  If n < 0, delete current buffer after buffer switch and remove buffer name from list in $bufListInfo.
# Return name of last buffer switched to.
function pnListBuf(1)
	nukeBuf = nil

	# Buffer list in $bufListInfo defined and not empty?
	if !defined?('Name', '$bufListInfo') || length($bufListInfo) < $screenNum ||\
	 nil?(screenRec = $bufListInfo[$screenNum - 1]) || empty?(bufList = screenRec[1])
		return 1 => message 'Fail, High', 'No buffer list'
	elsif $0 == defn
		$0 = 1
	elsif $0 == 0
		return
	elsif $0 < 0
		nukeBuf = $bufname		# Delete current buffer after buffer switch.
	endif

	# Set scanning parameters such that i + incr yields next or previous buffer in list.
	incr = $1 ? 1 : -1
	if include? bufList, $bufname
		i = -1
		for name in bufList
			++i
			if name == $bufname
				break
			endif
		endloop
	else
		i = $1 ? -1 : length(bufList)
	endif

	# Get next or previous buffer n times.
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
		force delBuf nukeBuf
		if defined?('Name', nukeBuf) != 'buffer'

			# Buffer was deleted.  Remove it from the list if present.
			if include? bufList, nukeBuf
				if (j = length(bufList) - 1) > 0
					i = -1
					for name in bufList
						++i
						if name == nukeBuf
							break
						endif
					endloop
					while i < j
						bufList[i] = bufList[i + 1]
						++i
					endloop
				endif
				pop bufList
			endif
			message "Buffer '#{nukeBuf}' deleted"
		endif
	endif
	$bufname
endroutine

# Switch to next buffer in $bufListInfo n times.
command nextListBuf(0) {description: 'Switch to next buffer in list created from most recent ~bfindFiles~B invocation n times. \
 If n < 0, switch once and delete buffer that was exited.  Returns: name of last buffer switched to.'}
	$0 => pnListBuf true
endroutine

# Switch to previous buffer in $bufListInfo n times.
command prevListBuf(0) {description: 'Switch to previous buffer in list created from most recent ~bfindFiles~B invocation n\
 times.  If n < 0, switch once and delete buffer that was exited.  Returns: name of last buffer switched to.'}
	$0 => pnListBuf false
endroutine

# Show a variable.
command showVar(0) {description: 'Show a variable and its value on the message line.'}
	varName = prompt('Show variable', 'Type: Var')
	if !empty? varName
		if defined?('Name', varName) != 'variable'
			1 => message 'Fail, High', "No such variable '", varName, "'"
		else
			1 => message 'NoWrap', varName, ' = ', quote eval varName
		endif
	endif
endroutine

# Fence word(s); that is, put punctuation or fence characters around one or more words at point.
command fenceWord(1) {arguments: 'c', description: 'Wrap a pair of quotes, fences ~#u() [] {} <>~U, or punctuation characters\
 around [-]n words (default 1) at point.  Negative n selects word(s) backward and positive n selects word(s) forward.'}

	# Determine left and right fences.
	if !interactive?
		fence = $1
	elsif empty?(fence = 1 => prompt('Fence char', $lastFenceChar, 'Type: Char'))
		return nil
	endif

	# It's a go...
	loop
		# Move point to starting point.
		$0 == defn || $0 == 0 and $0 = 1
		1 => setMark ??
		if 0 => wordChar? $lineChar
			if $0 < 0
				status = endWord
				--$0
			else
				status = ($lineOffset == 0 || !wordChar?(ord subline -1, 1)) ? true : backWord
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
			if 1 => index(rightFence, fence, 'Char') != nil
				leftFence = ord(strShift rightFence, nil)
				rightFence = ord(rightFence)
				break
			endif
		endloop
		if nil? leftFence
			# If fence is a punctuation character, just double it up.
			if fence > ?\s && fence <= ?~ && (fence == ?_ || !wordChar?(fence))
				leftFence = rightFence = fence
			else
				status = 1 => message 'Fail, High', "Invalid fence '", chr(fence), "'"
				break
			endif
		endif

		# Save fence that was entered if interactive.
		interactive? and $lastFenceChar = fence

		# Try to move to other end.
		1 => setMark ?_
		if ($0 < 0 ? abs($0) => backWord : $0 => endWord) == false
			status = 1 => message 'Fail, High', 'Too many words to fence'
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
			1 => setMark ??
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

# Indent or outdent a block of lines specified by n argument, given Boolean value and number of tab stops.  If first argument is
# true, indent; otherwise, outdent.  If second argument is nil, prompt for number of tab stops.
function iodent(2)

	# Get number of tab stops.
	if !nil? $2
		stops = $2
	elsif empty?(stops = 1 => prompt('Tab stops', 1))
		return nil
	elsif !numeric? stops
		return 1 => message 'Fail, High', "Invalid number '", stops, "'"
	elsif (stops = toInt(stops)) < 0
		return 1 => message 'Fail, High', 'Repeat count (', stops, ') must be 0 or greater'
	endif
	if stops > 0

		# Set region and indent or outdent it.
		$0 == 0 or -1 => selectLine($0)

		# Indent or outdent region.
		if $1
			eval 'stops => ', binding('Name', 'ESC )')
		else
			stops => outdentRegion
		endif
	endif
endroutine

# Indent a block of lines, given optional (when called interactively) number of tab stops.
command indentLine(1) {arguments: 'tab-stops', description: 'Indent [-]n lines (default 1).  User is prompted for number of tab\
 stops if interactive.'}
	$0 => iodent true, interactive? ? nil : $1
endroutine

# Outdent a block of lines, given optional (when called interactively) number of tab stops.
command outdentLine(1) {arguments: 'tab-stops', description: 'Outdent [-]n lines (default 1).  User is prompted for number of\
 tab stops if interactive.'}
	$0 => iodent false, interactive? ? nil : $1
endroutine

# Change indentation at beginning of current line without moving point.
command chgIndent(0) {description: 'Change indentation at beginning of current line without moving point.  If n < 0, abs(n)\
 tabs are deleted; if n > 0, n tabs are inserted; otherwise (n == 0), all white space is deleted.  n defaults to 1.'}
	$0 == defn and $0 = 1
	1 => setMark ??
	beginLine

	if $0 == 0
		delWhite
	elsif $0 < 0
		$0 => delBackTab
	else
		$0 => tab
	endif

	-1 => gotoMark ??
endroutine

# Perform set intersection on two arrays.  Return array of all elements in second array which are (if default n or n > 0) or are
# not (if n <= 0) in first array.  Arguments: (1), first array; (2), second array.
function select(2) {arguments: 'array1, array2', description: 'Perform set intersection on two arrays (non-matching if n < 0). \
 Returns: array of all elements in second array which are (if default n or n > 0) or are not (if n <= 0) in first array.'}
	exclude = $0 <= 0 && $0 != defn
	if empty? $1
		return exclude ? $2 : []
	endif
	if empty? $2
		return []
	endif

	result = []
	for item in $2
		include?($1, item) != exclude && push result, item
	endloop
	result
endroutine

# Get a buffer list to process: either all visible buffers (default n or n > 0) or only those also in buffer list in
# $bufListInfo for current screen (n <= 0).  Return false if error; otherwise, a two-element array containing Boolean "all
# buffers selected" and the buffer list (array).
function getBufList(0) {description: 'Get list of all visible buffers (or only those also in buffer list in $bufListInfo for\
 current screen if n <= 0).  Returns: false if error; otherwise, a two-element array containing Boolean "all buffers selected"\
 and the buffer list (array).'}

	if $0 != defn && $0 <= 0
		if !defined?('Name', '$bufListInfo') || length($bufListInfo) < $screenNum ||\
		 nil?(screenRec = $bufListInfo[$screenNum - 1])
			return 1 => message 'Fail, High', 'No buffer list found to search'
		endif
		searchList = select(1 => bufInfo(nil, 'Visible, Brief'), screenRec[1])
		allBufs = false
	else
		searchList = 1 => bufInfo(nil, 'Visible, Brief')
		allBufs = true
	endif
	empty?(searchList) ? 1 => message('Fail, High', 'No matching buffers found to search') : [allBufs, searchList]
endroutine

# Process a buffer list, given (1), buffer list; (2), name of user function to invoke on each buffer; and (3...), optional
# argument(s) to pass to function.  Prompt user to continue or quit before each buffer is processed.  If user selects "do rest",
# remaining buffers are processed without prompting.  A message and false is returned if an error occurs; otherwise, true.  The
# function (a), must operate non-interactively if n > 0; and (b), may return false and set an exception message if an error
# occurs, or an informational message (which is typically a blurb about what happened, like "3 substitutions"), or nil (no
# message).
function doBufList(2,) {arguments: 'list, function[, ...]', description: 'Invoke function interactively (with specified\
 arguments, if any) on given buffer list.  Returns: false if error occurs; otherwise, true.'}

	# Do sanity checks.
	if empty?($1)
		return 1 => message 'Fail, High', 'Empty buffer list'
	elsif defined?('Name', $2) != 'user function'
		return 1 => message 'Fail, High', "No such function '", $2, "'"
	endif

	bufList = shift($ARGV)
	funcName = shift($ARGV)

	# Build argument list.
	argList = ''
	for arg in $ARGV
		strPush argList, ', ', quote arg
	endloop

	# If current buffer is in list, cycle buffer list so it's in the front.
	if include? bufList, $bufname
		tempList = []
		while (bufname = shift(bufList)) != $bufname
			push tempList, bufname
		endloop
		bufList = [$bufname] & bufList & tempList
	endif

	# Save current buffer, position, and loop through buffers.
	origBuf = $bufname
	1 => setMark ??
	goBack = true
	opMode = defn
	procCount = 0
	returnMsg = nxtBuf = nil
	loop
		loop
			thisBuf = nxtBuf
			nxtBuf = shift(bufList)
			termAttr = false

			# If first time through loop and first buffer to process is not original one, get user confirmation.
			if nil? thisBuf
				if nxtBuf == origBuf
					next
				endif
				msg = 'First'
			else
				# Display progress if in "do the rest" mode.
				if opMode == 1
					print(procMsg = sprintf("Processing buffer '%s'...", thisBuf))
				endif
				opMode == 1 and print("Processing buffer '", thisBuf, "'...")

				# Switch to buffer and call processor.
				selectBuf thisBuf
				beginBuf
				if (returnMsg = eval(join ' ', opMode, '=>', funcName, argList)) == false
					return false
				endif
				++procCount

				# Display result if in "do the rest" mode.
				opMode == 1 and print(procMsg, ' ', returnMsg ? returnMsg : 'done', '.')

				# Last buffer?
				if nil? nxtBuf
					break
				elsif opMode != 1

					# Not last buffer and interactive.  Build prompt with return message (if any).
					msg = returnMsg ? "#{returnMsg}.  Next" : 'Next'
					updateScreen
				endif
			endif

			# If in interactive mode...
			if opMode != 1

				# Finish prompt and get confirmation to continue.
				msg = "'#{msg} buffer: '#{nxtBuf}'  Continue? "
	                        loop
	                        	reply = termAttr ? prompt(msg, 'Type: Char', 'TermAttr') : prompt(msg, 'Type: Char')
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
		gotoMark ??
		break
	endloop
	delMark ??

	# Return result.
	if !goBack
		1 => message 'Force', nil
	else
		opMode == 1 and returnMsg = '%u buffers processed' % procCount
		1 => message 'High, NoWrap', nil?(returnMsg) ? 'Done!' : '%s.  Done!' % returnMsg
	endif
	true
endroutine

# Perform replace (n > 0) or queryReplace (otherwise) on current buffer and return custom result message.  Called from
# queryReplaceAll command.
function queryReplaceOne(0)
	oldMsgState = 1 => chgMode nil, 'RtnMsg'
	1 => message 'Force', nil
	if $0 > 0
		replace $searchPat, $replacePat
	elsif not queryReplace $searchPat, $replacePat
		oldMsgState => chgMode nil, 'RtnMsg'
		return false
	endif
	oldMsgState => chgMode nil, 'RtnMsg'
	(i = 1 => index($ReturnMsg, ?,, 'Char')) != nil ? substr($ReturnMsg, 0, i) : $ReturnMsg
endroutine

# Query-replace multiple buffers (files).  Use buffer list in $bufListInfo if n <= 0; otherwise, all visible buffers.
command queryReplaceAll(0) {description: 'Invoke ~bqueryReplace~B command on all visible buffers (or just those that matched\
 the most recent ~bfindFiles~B invocation if n <= 0).  Returns: false if error occurs; otherwise, true.'}

	# Get list of buffers to search: either all visible buffers or only those also in buffer list in $bufListInfo.
	if (searchList = $0 => getBufList) == false
		return false
	endif
	whichBufs = shift(searchList) ? 'all' : 'selected'
	searchList = shift(searchList)

	# Prompt for search and replace strings.
	for type in ["$searchPat:In #{whichBufs} buffers, query replace:SearchRing", '$replacePat:with:ReplaceRing']
		srVar = strShift(type, ?:)
		newValue = strShift(type, ?:)
		newValue = 1 => prompt(newValue, eval(srVar), 'Type: %s' % type, 'Delim: %s' % $searchDelim)
		if empty?(newValue) && srVar == '$searchPat'		# Nil or null string entered for $searchPat?
			return						# Yes, bail out.
		endif
		eval join ' ', srVar, '=', quote newValue		# No, assign new value.
	endloop

	# Process buffers.
	doBufList searchList, 'queryReplaceOne'
endroutine

# Scan files that match given template in given directory.  Return [buffer-name, mark] of first file that contains given search
# pattern, or false if not found.  Any file that cannot be read (because of permissions, for example) is ignored.  Arguments:
# (1), directory; (2), filename template, or array of templates; (3), search pattern.
function locateFile(3) {arguments: 'dir, glob, pat', description: 'Find files matching glob (or array of glob patterns) in\
 given directory that contain search pattern.  Returns: false if pattern not found, or a two-element array containing: buffer\
 name of first file that contains the search pattern, and the mark that was set at the position where the pattern was found (or\
 nil if buffer was created).'}

	# Prepare for search in new screen.
        $searchPat = $3
	saveScreen
	selectScreen 0
	startBuf = $bufname

	# Get glob patterns into an array, then loop through them.
	type?($2) == 'array' or $2 = [$2]
	for globPat in $2

		# Get file list for current glob pattern and scan the files (if any).
		bufname = mark = nil
		for filename in glob($1 == '.' ? globPat : sprintf('%s/%s', $1, globPat))
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
				isNewBuf or 1 => setMark ?`
				beginBuf
				if huntForw
					# Found!
					beginLine
					isNewBuf or 1 => setMark(mark = ?_)
					break 2
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
	endloop

	# Clean up.
	delRingEntry 'Search'
	screenNum = $screenNum
	restoreScreen
	delScreen screenNum

	# Return result if search string found; otherwise, false.
	nil?(bufname) ? 1 => message('Fail, High', 'Not found') : [bufname, mark]
endroutine

# Find buffers in given list matching an apropos or search pattern and return them.
function matchBuffers(3) {arguments: 'buf-list, pat, match-bufname', description: 'Find buffers in given array whose names\
 match given apropos pattern (if match-bufname is true), or whose contents match given search pattern (if match-bufname is\
 false).  If an element in buf-list is an array, it is assumed to be of form [bufname, created?]; otherwise, it is assumed to\
 be a buffer name.  If pat is nil or null, all buffers are considered to be a match.  Returns: array of matching buffer names.'}
	if empty?($2) && (empty?($1) || type?($1[0]) == 'string')
		$1
	else
		bufList = []

		if $3
			# Find matching buffer names.  Add ":i" (ignore case) to pattern if no option characters specified.
			if !empty?($2) && $2 !~ '^.+:[eifmpr]+$'
				$2 &= ':i'
			endif

			for buf in $1
				bufname = type?(buf) == 'array' ? buf[0] : buf
				if empty?($2) || !nil?(index bufname, $2)
					push bufList, bufname
				endif
			endloop
		else
			# Search buffers.
			if !empty?($2)
				$searchPat = $2
				1 => setMark ?_
				saveBuf
			endif
			for buf in $1
				bufname = type?(buf) == 'array' ? buf[0] : buf
				if empty?($2)
					found = true
				else
					selectBuf bufname
					1 => setMark ??
					beginBuf
					found = huntForw
					0 => gotoMark ??
				endif
				found && push(bufList, bufname)
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

# Open list of files returned from given shell glob pattern in background.  Return (possibly empty) array of
# [bufname, created?] elements.
function openFiles(1) {arguments: 'glob-pat', description: 'Open list of files matching shell glob pattern in background. \
 Returns: array of buffer names (which may be empty).'}
	bufList = []

	# Open the "regular" files (if any) in the background.
	for filename in glob($1)
		if stat?(filename, 'f')
			fileInfo = 0 => findFile(filename)
			push bufList, fileInfo
		endif
	endloop

	# Return result.
	bufList
endroutine

# Find files or buffers that match a template and optionally, a search pattern, and open matching files in the background.  Save
# resulting buffer list (array) in $bufListInfo variable using current screen number - 1 as index.
command findFiles(0) {description: 'Find all files or buffers matching a template and optionally, also matching a search\
 pattern, and open matching files in the background.  If n is not the default, user is prompted to select zero or more of the\
 following options: "FindBuffers" -> prompt for a buffer apropos pattern instead of a filename template; and "Exclude" ->\
 prompt for an exclusion search pattern.  If the "FindBuffers" option is not selected or n is the default, user is prompted for\
 a filename template and a search pattern.  If a search pattern is entered, only files matching the template that contain at\
 least one occurrence of the pattern are selected.  (However, if a file matching the template is already opened in the editor,\
 its current buffer is searched, not the file on disk.)  Conversely, if the "FindBuffers" option is selected, user is prompted\
 for an apropos pattern and a search pattern.  In this case, all visible buffers whose names match the apropos pattern and that\
 contain at least one occurrence of the search pattern are selected.  In either case, if the "Exclude" option is specified, the\
 user is prompted for a second search pattern.  All selected buffers that contain at least one occurrence of the exclusion\
 pattern are omitted.  Note that the apropos pattern, search pattern, and/or exclusion pattern (but not the filename template)\
 may be null, in which case it has no effect on the buffer selection process (except that all visible buffers are selected if\
 the apropos pattern is null).  The names of the resulting buffers are saved in a global variable so that they can be\
 subsequently searched by the ~bqueryReplaceAll~B or ~bppDeletePPLines~B command, or stepped through with the ~bprevListBuf~B\
 and ~bnextListBuf~B commands.  Returns: true if any files or buffers were found; otherwise, false.'}

	# Set up $bufListInfo global variable, which is an array containing a nested array for each screen.  Each nested array
	# contains two arrays: [[glob-pat, apropos-pat, search-pat, exclude-pat], [bufname, ...]].
	defined?('Name', '$bufListInfo') or $bufListInfo = []
	if length($bufListInfo) < $screenNum || nil?(screenRec = $bufListInfo[$screenNum - 1])
		$bufListInfo[$screenNum - 1] = screenRec = array(2, [])
	endif
	bufSelector = screenRec[0]
	empty?(bufSelector) and bufSelector[3] = nil

	# Get options if n argument.
	aproposBuf = excludeBuf = false
	if $0 != defn
		if empty?(options = prompt('Options (Find~b~uB~Zuffers, ~b~uE~Zxclude)', 'TermAttr'))
			return
		else
			if (optionsLC = lowerCaseStr(options)) !~ '^[be]+$'
				return 1 => message 'Fail', 'Invalid options "', options, '"'
			endif

			loop
				option = strShift(optionsLC, nil)
				if option == 'b'
					aproposBuf = true
				else	# option == 'e'
					excludeBuf = true
				endif
				if empty? optionsLC
					break
				endif
			endloop
		endif
	endif

	# Get ready.
	if aproposBuf
		i = 1
		promptStr = 'Apropos bufname'
		type = 'String'
	else
		i = 0
		promptStr = 'Filename template'
		type = 'File'
	endif

	# Prompt for filename template or apropos pattern, using last one as default.
	bufSelector[i] = 1 => prompt(promptStr, bufSelector[i], 'Type: %s' % type)
	if empty?(bufSelector[i]) && !aproposBuf
		return
	endif
	if !aproposBuf
		substr(bufSelector[0], -1, 1) == '/' and bufSelector[0] &= '*'
		bufSelector[0] = 2 => sub(bufSelector[0], ' ', '\ ')
	endif

	# Prompt for search pattern.
	bufSelector[2] = searchPat = 1 => prompt('Search pattern', bufSelector[2], 'Type: SearchRing',\
	 'Delim: %s' % $searchDelim)

	# Find files and buffers.  Break out of loop if none found.
	loop
		if aproposBuf
			# Find buffers whose names match apropos pattern.  bufList contains: [bufname, ...]
			bufList = 1 => bufInfo(nil, 'Visible, Brief')
			if !empty?(bufSelector[1]) && empty?(bufList = matchBuffers(bufList, bufSelector[1], true))
				break
			endif
		else
			# Open files that match template in background.  bufList contains: [[bufname, created?], ...]
			if empty?(globList = openFiles(bufSelector[0]))
				break
			endif
			bufList = globList
		endif

		# If a search pattern was specified, scan for matching buffers.
		bufList = matchBuffers(bufList, searchPat, false)

		# Prompt for exclusion search pattern if requested, using last one as default.
		if excludeBuf
			if !empty?(bufSelector[3] = 1 => prompt('Exclude', bufSelector[3], 'Type: SearchRing',\
			 'Delim: %s' % $searchDelim)) && !empty?(xBufList = matchBuffers(bufList, bufSelector[3], false))

				# Exclusion buffers found.  Remove them from match list.
				bufList = -1 => select(xBufList, bufList)
			endif
		endif

		# If a search pattern was entered, add it to search ring for user's convenience.
		empty?(searchPat) or $searchPat = searchPat

		# Lastly, if file search was performed, remove buffers that were created but not selected.
		if !aproposBuf
			for bufRec in globList
				if bufRec[1] && !include?(bufList, bufRec[0])
					delBuf bufRec[0]
				endif
			endloop
		endif

		# Done.  Remember buffer list and report results.
		screenRec[1] = bufList
		if((count = length(bufList)) == 0)
			break
		endif
		itemList = ''
		for bufname in bufList
			strPush itemList, ', ', aproposBuf ? bufname : basename(bufInfo(bufname)[1])
		endloop
		return 1 => message 'High',\
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
		oldMsgState = 1 => chgMode(nil, 'RtnMsg')
		selectScreen 0
		print '[%s]' % $ReturnMsg
		pause 1
		oldMsgState => chgMode nil, 'RtnMsg'
		1 => message 'Force', nil
		chgDir dir

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
	1 => chgMode nil, 'RtnMsg'
	$0 => cycleRing 'Delete'
endroutine

# Cycle kill ring.
command cycleKillRing(0) {description: 'Cycle kill ring [-]n times (default 1).'}
	1 => chgMode nil, 'RtnMsg'
	$0 => cycleRing 'Kill'
endroutine

# Cycle macro ring.
command cycleMacroRing(0) {description: 'Cycle macro ring [-]n times (default 1).'}
	1 => chgMode nil, 'RtnMsg'
	$0 => cycleRing 'Macro'
endroutine

# Cycle replace ring.
command cycleReplaceRing(0) {description: 'Cycle replace ring [-]n times (default 1).'}
	1 => chgMode nil, 'RtnMsg'
	$0 => cycleRing 'Replace'
endroutine

# Cycle search ring.
command cycleSearchRing(0) {description: 'Cycle search ring [-]n times (default 1).'}
	1 => chgMode nil, 'RtnMsg'
	$0 => cycleRing 'Search'
endroutine

# Delete entri(es) from delete ring.
command delDeleteEntry(0) {description: 'Delete the most recent n entries (default 1) from delete ring (or entry number n if\
 n < 0, all entries if n == 0).'}
	1 => chgMode nil, 'RtnMsg'
	$0 => delRingEntry 'Delete'
endroutine

# Delete entri(es) from kill ring.
command delKillEntry(0) {description: 'Delete the most recent n entries (default 1) from kill ring (or entry number n if\
 n < 0, all entries if n == 0).'}
	1 => chgMode nil, 'RtnMsg'
	$0 => delRingEntry 'Kill'
endroutine

# Delete entri(es) from macro ring.
command delMacroEntry(0) {description: 'Delete named macro from macro ring if default n; otherwise, delete the most recent n\
 entries, or entry number n if n < 0, or all entries if n == 0.'}
	1 => chgMode nil, 'RtnMsg'

	# Get macro name, if applicable.
	if $0 == defn
		if !empty?(macro = prompt('Delete macro', 'Type: Macro'))
			delRingEntry 'Macro', macro
		endif
	else
		$0 => delRingEntry 'Macro'
	endif
endroutine

# Delete entri(es) from replace ring.
command delReplaceEntry(0) {description: 'Delete the most recent n pattern entries (default 1) from replace ring (or entry\
 number n if n < 0, all patterns if n == 0).  The most recent remaining entry is subsequently set as the current replace\
 pattern.'}
	1 => chgMode nil, 'RtnMsg'
	$0 => delRingEntry 'Replace'
endroutine

# Delete entri(es) from search ring.
command delSearchEntry(0) {description: 'Delete the most recent n pattern entries (default 1) from search ring (or entry number\
 n if n < 0, all patterns if n == 0).  The most recent remaining entry is subsequently set as the current search pattern.'}
	1 => chgMode nil, 'RtnMsg'
	$0 => delRingEntry 'Search'
endroutine

# Display delete ring.
command showDeleteRing(0) {description: 'Generate list of delete ring entries in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values.'}
	$0 => showRing 'Delete'
endroutine

# Display kill ring.
command showKillRing(0) {description: 'Generate list of kill ring entries in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values.'}
	$0 => showRing 'Kill'
endroutine

# Display macro ring.
command showMacroRing(0) {description: 'Generate list of macro ring entries in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values.'}
	$0 => showRing 'Macro'
endroutine

# Display replace ring.
command showReplaceRing(0) {description: 'Generate list of replace ring patterns in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values.'}
	$0 => showRing 'Replace'
endroutine

# Display search ring.
command showSearchRing(0) {description: 'Generate list of search ring patterns in a new buffer and render\
 it per ~bselectBuf~B options (in a pop-up window if default n).  Returns: ~bselectBuf~B values.'}
	$0 => showRing 'Search'
endroutine

# Display man page for word at point.
command showMan(0) {description: 'Display man page for word at point.'}
	if nil?(word = getWord)
		1 => message 'Fail, High', 'Not in a word'
	else
		cols, rows = $TermSize
		1 => shellCmd 'NoHdr', 'MANWIDTH=', cols, ' man ', word, ' | col -b'
	endif
endroutine

# Join line(s) with no spacing.
command joinLines0(0) {description: 'Join [-]n lines (default -1) with no spacing in between.'}
	$0 => joinLines nil
endroutine

# Add key to report buffer if applicable, given (1), buffer name; (2), prefix; (3), raw key; (4), suffix; and (5), apropos
# pattern.  Called from "showBindings" command.
function addKey(5)
	keyLit = toStr($2) & $4
	if nil?($5) || index(keyLit, $5) != nil
		if nil?(name = binding('Name', keyLit))
			bprint $1, keyLit, $3 == ?~ ? "~\n" : "\n"
		else
			if $3 == ?~
				pad = 14
				keyLit &= '~'
			else
				pad = 13
			endif
			bprintf $1, "%-*s%s%s\n", pad, keyLit, nil?(index(defined?('Name', name), 'user')) ? ' ' : '@', name
		endif
	endif
endroutine

# Create listing of all possible key bindings and what they are bound to.
command showBindings(1) {arguments: 'pat', description: 'Generate list of key bindings matching given pattern in a new buffer\
 and render it per ~bselectBuf~B options (in a pop-up window if default n).  If pattern is plain text, match is successful if\
 key literal contains pat.  If pat is nil or a null string, all possible key bindings are listed.  Returns: ~bselectBuf~B\
 values.'}
	argCount = length($ARGV)

	# Get apropos search pattern and validate it.
	$0 == defn and $0 = -1
	pat = interactive? ? prompt('Apropos binding') : $1
	empty?(pat) and pat = nil

	# Get a buffer and initialize it.
	0 => selectBuf(bufname = '.Bindings')
	-1 => clearBuf bufname
	1 => chgBufAttr bufname, 'TermAttr, Hidden'
	bprintf bufname, "~b%-13s%s~B\n", 'Binding', 'Command'

	# Do the five groups.
	aMinus1 = ?a - 1
	bangMinus1 = ?! - 1
	for prefix in [nil, 'C-c ', 'C-h ', 'C-x ', 'ESC ']
		bprintf bufname, "\n============= ~b%sPrefix~B =============\n", nil?(prefix) ? 'NO  ' : prefix
		c = -1
		while ++c <= 0x7F
			keyLit1 = c == 0 ? 'C-SPC' : c == 011 ? 'TAB' : c == 015 ? 'RTN' : c == 033 ? 'ESC' :\
			 c == 040 ? 'SPC' : c == 0x7F ? 'DEL' : c < 32 ? ('C-' & chr(aMinus1 + (c > 26 ? c - 32 : c))) : chr(c)
			addKey bufname, prefix, c, keyLit1, pat
		endloop

		shiftPrefix = nil
		for keyLit1 in [nil, 'S-TAB']
			bprint bufname, "--------------------------------------\n"
			c = bangMinus1
			while ++c <= ?~
				if keyLit1
					--c
					shiftPrefix = 'S-'
				else
					keyLit1 = toStr(shiftPrefix) & 'FN' & chr(c)
				endif
				addKey bufname, prefix, c, keyLit1, pat
				keyLit1 = nil
			endloop
		endloop
	endloop

	# Render buffer.
	-1 => chgBufAttr bufname, 'Changed'
	if $0 == -1
		1 => popBuf bufname, 'Delete'
	else
		-1 => chgBufAttr bufname, 'Hidden'
		1 => gotoLine 2, bufname
		$0 => selectBuf bufname
	endif
endroutine

# Create and pop a "help file not found" buffer, given (1), filename.
function hsNotFound(1)

	# Main script file not found.  Give user the bad news.
	1 => chgBufAttr (bufname = (0 => scratchBuf)[0]), 'Hidden, TermAttr'
	bprint bufname, "~bHelp files not found!~B\n\n\nFile \"", $1, "\" was not found in $execPath:\n    ",\
	 $execPath, "\n\nPlease check your shell startup script.  MMPATH may need to be set.\nConsult file ",\
	 '"Install.txt" for more information.'
	bprint bufname, "\n\n\n\n\n\nPress ~uq~U (quit) to dismiss this window.\n\nAfter returning to your",\
	 " editing session, you can enter ~#uESC e~U to exit\n", getInfo('Editor'), ' if desired (press the ~uesc~U',\
	 ' key, then ~ue~U).'
	-1 => chgBufAttr bufname, 'Changed'
	1 => popBuf bufname, 'Delete'
	nil
endroutine

# Offer help options.
command help(0) {description: 'Enter Help System (or Advanced Help System if n argument).'}
	HelpSys = 'help.ms'
	MainMenu = 'hsMain'

	# Load help system if needed.
	if !defined?('Name', MainMenu) && (!(helpPath = xPathname(HelpSys)) || !require(helpPath))
		return hsNotFound HelpSys
	endif

	# Enter help system.
	eval '$0 => %s' % MainMenu
endroutine

##### Bindings and Aliases #####
bindKey 'C-c TAB', chgIndent
bindKey 'C-x D', delFile
bindKey 'ESC #', enumLine
bindKey 'ESC "', fenceWord
bindKey 'C-x f', findFiles
bindKey 'C-x C-g', getFile
bindKey 'ESC ?', help
bindKey 'C-c )', indentLine
bindKey 'C-x C-j', joinLines0
bindKey 'C-h ]', nextHomeBuf
bindKey 'C-c ]', nextListBuf
bindKey 'C-h t', openTutor
bindKey 'C-c (', outdentLine
bindKey 'C-h l', popFileList
bindKey 'C-h [', prevHomeBuf
bindKey 'C-c [', prevListBuf
bindKey 'ESC C-q', queryReplaceAll
bindKey 'C-x R', renameFile
bindKey 'C-h n', showBindings
bindKey 'C-h e', showCheatSheet
bindKey 'C-h N', showMan
bindKey 'C-h =', showVar
bindKey 'ESC C-\\', trimBuf
bindKey 'ESC ~', unchangeBuf

bindKey 'C-c d', cycleDeleteRing
bindKey 'C-c k', cycleKillRing
bindKey 'C-c M', cycleMacroRing
bindKey 'C-c r', cycleReplaceRing
bindKey 'C-c s', cycleSearchRing
bindKey 'C-x d', delDeleteEntry
bindKey 'C-x k', delKillEntry
bindKey 'C-x M', delMacroEntry
bindKey 'C-x r', delReplaceEntry
bindKey 'C-x s', delSearchEntry
bindKey 'C-h d', showDeleteRing
bindKey 'C-h k', showKillRing
bindKey 'C-h M', showMacroRing
bindKey 'C-h r', showReplaceRing
bindKey 'C-h s', showSearchRing

##### Load programming language libraries.
(path = xPathname('lang')) && require path

##### Load site-wide preferences.
if (path = xPathname('site'))
	require path

	# Create "word processing" line-join commands if $EndSentence variable not empty.
	if !empty?($EndSentence)

		# Join line(s) with extra spacing.
		command wpJoinLines(0) {description: 'Run the ~bjoinLines~B command with the $EndSentence global variable as an\
 argument to obtain extra spacing between joined lines.'}
			$0 => joinLines $EndSentence
		endroutine

		# Wrap line(s) with extra spacing.
		command wpWrapLine(0) {description: 'Run the ~bwrapLine~B command with the $EndSentence global variable as an\
 argument to obtain extra spacing between lines when the line block is rewrapped.'}
			$0 => wrapLine nil, $EndSentence
		endroutine

		bindKey 'C-c C-j', wpJoinLines
		bindKey 'C-c RTN', wpWrapLine
	endif
endif

##### Set help message if no command-line arguments were specified.
if length($ARGV) == 0
	helpBinding = binding('KeyList', 'help')[0]
	quitBinding = binding('KeyList', 'exit')[0]
	1 => message 'High, TermAttr', sprintf('Type ~#u%s~U for help, ~#u%s~U to quit', helpBinding, quitBinding)
endif
