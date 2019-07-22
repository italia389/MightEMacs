# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# memacs.ms		Ver. 9.4.0
#	MightEMacs startup file.

##### Global Variables #####
$lastFenceChar = ?"		# Last fence character used in "fenceWord" macro.

oldMsgState = -1 => chgMode(nil,'RtnMsg')

##### Macros #####

# Display MightEMacs cheat sheet on separate screen.
macro showCheatSheet(0) {desc: 'Display %s cheat sheet on a separate screen (or return to previous screen if it is already\
 being displayed).' % getInfo('Editor')}
	name = 'CheatSheet'

	# Displaying cheat sheet?
	if $bufname == name
		restoreScreen
	else
		# Read file if needed.
		if defined?('Name',name) != 'buffer'
			if (path = xPathname('help/' & name))
				oldMsgState = -1 => chgMode(nil,'RtnMsg')
				0 => readFile path
				1 => chgBufAttr name,'Hidden,ReadOnly,TermAttr'
				oldMsgState => chgMode nil,'RtnMsg'
			else
				return -1 => message 'High','Cheat sheet not found'
			endif
		endif

		# Find screen displaying buffer.
		for windInfo in 1 => getInfo('Windows')
			screenNum,windNum,bufname = windInfo
			if bufname == name
				saveScreen
				selectScreen screenNum
				return
			endif
		endloop

		# Buffer not being displayed... create a screen for it.
		oldMsgState = -1 => chgMode(nil,'RtnMsg')
		saveScreen
		selectScreen 0
		selectBuf name
		setBufFile name,nil
		gotoLine 1
		oldMsgState => chgMode nil,'RtnMsg'
		message 'TermAttr','Type ~#u%s~U again to return to previous screen' % binding('KeyList',$RunName)[0]
	endif
endmacro

# Delete patterns from search ring until given pattern is on top.
constrain macro restoreSearchRing(1)
	while !empty?($searchPat) && $searchPat != $1
		delSearchPat
	endloop
	message 'Force',nil
endmacro

# Unchange current buffer.
macro unchangeBuf(0) {desc: 'Clear the "changed" attribute of the current buffer.'}
	-1 => chgBufAttr $bufname,'Changed'
endmacro

# Trim all lines in current buffer.
macro trimBuf(0) {desc: 'Trim white space from end of all lines in current buffer.'}
	oldMsgState = -1 => chgMode(nil,'RtnMsg')
	markBuf
	oldMsgState => chgMode nil,'RtnMsg'
	0 => trimLine
	gotoMark ?`
endmacro

# Enumerate a line block.
macro enumLine(0) {desc: 'Insert sequential numbers at beginning of all lines in a block.'}
	run seti
	lineCt = selectLine($0)				# Get line count and position point.
	while lineCt--
		inserti
		2 => beginLine
	endloop
	nil
endmacro

# Pop a file listing.
macro popFileList(0) {desc: 'Display a file listing in a pop-up window in long form.  User is prompted for the shell glob\
 pattern and zero or more of the following options which modify the default listing format: "AllFiles" -> include files that\
 begin with a period (.); "NoExpand" -> do not expand directories that match the shell template; and "MultiCol" -> display in\
 multiple columns (condensed form).'}
	lsSwitches1 = ''		# Do not include hidden files.
	lsSwitches2 = ''		# Expand directories.
	lsSwitches3 = 'l'		# Long form.

	# Get glob pattern.
	template = prompt('ls','Type: File')
	nil?(template) || template == '.' and template = ''

	# Get options.
	if !empty?(options = prompt('Options (~b~uA~ZllFiles, No~b~uE~Zxpand, Multi~b~uC~Zol)','TermAttr'))
		if (optionsLC = lowerCaseStr(options)) !~ '^[aec]+$'
			return -1 => message nil,'Invalid options "',options,'"'
		endif

		loop
			option = strShift(optionsLC,nil)
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
	shellCmd 'Shift','ls -',lsSwitches1,lsSwitches2,lsSwitches3,' ',template
endmacro

# Rename a file on disk.
macro renameFile(2,3) {usage: '{filename | bufname},new-name[,opts]',desc: 'Rename a file on disk.  If n argument, one or both\
 of the following comma-separated option(s) may be specified in string arguments opts: "FromBuffer" -> use file associated with\
 named buffer given as first argument as the one to rename; and "DiskOnly" -> if "FromBuffer" specified, do not set the\
 buffer\'s file to the new filename.  Case of option keywords is ignored.'}
	fromBuffer = diskOnly = false

	# Get options.
	if $0 != defn
		if !empty?(options = prompt('Options (From~b~uB~Zuffer, ~b~uD~ZiskOnly)','TermAttr'))
			if (optionsLC = lowerCaseStr(options)) !~ '^[bd]+$'
				return -1 => message nil,'Invalid options "',options,'"'
			endif

			loop
				option = strShift(optionsLC,nil)
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
			if empty?(bufname = 1 => prompt('Rename file in buffer',$bufname,'Type: Buffer'))
				return
			endif
		elsif type?(bufname = $1) != 'string'
			return -1 => message 'High','Invalid buffer name ',1 => toStr(bufname,'Visible,Quote2')
		endif

		# Have buffer name.  Get associated filename.
		if bufname == $bufname
			oldFilename = $bufFile
		elsif defined?('Name',bufname) == 'buffer'
			oldFilename = bufInfo(bufname)[1]
		else
			return -1 => message 'High',"No such buffer '",bufname,"'"
		endif

		if empty? oldFilename
			return -1 => message 'High',"No filename set in buffer '",bufname,"'"
		endif

	# Get disk filename.
	elsif interactive?
		if empty?(oldFilename = prompt('Rename disk file','NoAuto','Type: File'))
			return
		endif
	elsif type?(oldFilename = $1) != 'string'
		return -1 => message 'High','Invalid filename ',1 => toStr(oldFilename,'Visible,Quote2')
	endif

	# Have old filename.  Get new one.
	if interactive?
		if empty?(newFilename = 1 => prompt($0 == defn ? 'to' : diskOnly ? 'New disk filename' : 'New filename',\
		 oldFilename,'Type: File','NoAuto'))
			return
		endif
	elsif type?(newFilename = $2) != 'string'
		return -1 => message 'High','Invalid filename ',1 => toStr(newFilename,'Visible,Quote2')
	endif

	# Rename file on disk if it exists.
	if (exists = stat?(oldFilename,'f'))
		if not shellCmd('NoPop','mv ',shQuote(oldFilename),' ',shQuote(newFilename))
			return false
		endif
	elsif $0 == defn || diskOnly
		return -1 => message 'High','No such file "',oldFilename,'"'
	endif

	# Rename buffer file, if applicable.
	fromBuffer && !diskOnly and setBufFile(bufname,newFilename)

	# Return result message.
	message nil,(!exists ? 'File "%s" does not exist, buffer filename changed' % oldFilename :\
	 diskOnly ? 'Disk file renamed' : 'File renamed')
endmacro

# Delete a file on disk and optionally, an associated buffer.
macro delFile(1) {usage: '{bufname | filename}',\
 desc: 'Delete a file on disk (or delete a buffer and its associated file if n argument).'}
	if $0 == defn
		# Get filename.
		if !interactive?
			filename = $1
		elsif empty?(filename = prompt('Delete file','NoAuto','Type: File'))
			return
		endif

		# Delete file.
		if not stat? filename,'e'
			-1 => message 'High','No such file "',filename,'"'
		else
			kind = stat?(filename,'d') ? 'Directory' : 'File'
			if shellCmd 'NoPop','rm ',shQuote(filename)
				message nil,kind,' deleted'
			endif
		endif
	else
		# Get buffer name.
		if !interactive?
			bufname = $1
		elsif empty?(bufname = prompt('Buffer name','Type: Buffer'))
			return
		endif

		# Check if buffer exists.
		if defined?('Name',bufname) != 'buffer'
			return -1 => message 'High','No such buffer "',bufname,'"'
		endif

		# Have valid buffer name.  Get associated filename.
		filename = (bufname == $bufname) ? nil : bufInfo(bufname)[1]

		# Try to delete buffer (which may fail).
		if delBuf(bufname) == 0
			return
		endif

		# Buffer deleted.  Now delete file if it exists.
		if !nil?(filename) && stat? filename,'e'
			if shellCmd 'NoPop','rm ',shQuote(filename)
				message nil,'Buffer and file "',filename,'" deleted'
			endif
		else
			message nil,'Buffer deleted'
		endif
	endif
endmacro

# Delete output file before write if file is a symbolic or hard link and user okays it, given (1), buffer name; and (2),
# filename.  This is done so that symbolic links will not be followed and hard links will be broken on update, which will
# effectively create a new file and preserve the original file.  Note that links will also be broken if 'safe' mode is enabled,
# so do nothing in that case.
constrain macro hkWrite(2) {desc: 'Delete output file before write if symbolic or hard link and user okays it.'}
	if !mode?(nil,'Safe')

		# Check if output file exists, is a symbolic or hard link, and is not in the $linksToKeep list.
		if stat?($2,'Ll') && !include?(nil,$linksToKeep,filename = 0 => pathname($2))

			# Outfile is a link and not previously brought to user's attention.  Ask user if the link should be
			# broken.  If yes, delete the output file; otherwise, remember response by adding absolute pathname to
			# $linksToKeep.
			type = stat?(filename,'L') ? 'symbolic' : 'hard'
			p = sprintf('Break %s link for file "%s" on output? (y,n)',type,$2)
			if 1 => prompt(p,?n,'Type: Char') == ?y
				shellCmd 'NoPop','rm ',filename
			else
				push $linksToKeep,filename
			endif
		endif
	endif
endmacro
setHook 'write',hkWrite			# Set write hook.
$linksToKeep = []			# Link pathnames to leave in place, per user's request.

# Get buffer list from $fileListInfo for current screen and switch to next or previous buffer in that list n times, given
# "forward?" argument.  If n < 0, delete current buffer after buffer switch and remove buffer name from list in $fileListInfo.
# Return name of last buffer switched to.
constrain macro pnListBuf(1)
	nukeBuf = nil

	# Buffer list in $fileListInfo defined and not empty?
	if !defined?('Name','$fileListInfo') || length($fileListInfo) < $screenNum ||\
	 nil?(screenRec = $fileListInfo[$screenNum - 1]) || empty?(bufList = screenRec[1])
		return -1 => message 'High','No buffer list'
	elsif $0 == defn
		$0 = 1
	elsif $0 == 0
		return
	elsif $0 < 0
		nukeBuf = $bufname		# Delete current buffer after buffer switch.
	endif

	# Set scanning parameters such that i + incr yields next or previous buffer in list.
	incr = $1 ? 1 : -1
	if include? nil,bufList,$bufname
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
		if defined?('Name',nukeBuf) != 'buffer'

			# Buffer was deleted.  Remove it from the list if present.
			if include? nil,bufList,nukeBuf
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
			message nil,"Buffer '#{nukeBuf}' deleted"
		endif
	endif
	$bufname
endmacro

# Switch to next buffer in $fileListInfo n times.
macro nextListBuf(0) {desc: 'Switch to next buffer in list created from most recent ~bfindFiles~B invocation n times.  If\
 n < 0, switch once and delete buffer that was exited.  Returns: name of last buffer switched to.'}
	$0 => pnListBuf true
endmacro

# Switch to previous buffer in $fileListInfo n times.
macro prevListBuf(0) {desc: 'Switch to previous buffer in list created from most recent ~bfindFiles~B invocation n times.  If\
 n < 0, switch once and delete buffer that was exited.  Returns: name of last buffer switched to.'}
	$0 => pnListBuf false
endmacro

# Show a variable.
macro showVar(0) {desc: 'Show a variable and its value on the message line.'}
	varName = prompt('Show variable','Type: Var')
	if !empty? varName
		if defined?('Name',varName) != 'variable'
			-1 => message 'High',"No such variable '",varName,"'"
		else
			message 'NoWrap',varName,' = ',quote eval varName
		endif
	endif
endmacro

# Fence word(s); that is, put punctuation or fence characters around one or more words at point.
macro fenceWord(1) {usage: 'c',desc: 'Wrap a pair of quotes, fences ~#u() [] {} <>~U, or punctuation characters around\
 [-]n words (default 1) at point.  Negative n selects word(s) backward and positive n selects word(s) forward.'}

	# Determine left and right fences.
	if !interactive?
		fence = $1
	elsif empty?(fence = 1 => prompt('Fence char',$lastFenceChar,'Type: Char'))
		return nil
	endif

	# It's a go...
	oldMsgState = -1 => chgMode(nil,'RtnMsg')
	loop
		# Move dot to starting point.
		$0 == defn || $0 == 0 and $0 = 1
		1 => setMark ??
		if 0 => wordChar? $lineChar
			if $0 < 0
				status = endWord
				--$0
			else
				status = ($lineOffset == 0 || !wordChar?(ord subline -1,1)) ? true : backWord
			endif
		elsif $0 > 0
			status = forwWord
		elsif((status = backWord) != false)
			status = endWord
		endif

		# Any word(s) in buffer?
		if status == false
			-1 => message 'High','No word found'
			break
		endif

		# Dot is now at starting point.  Determine left and right fences.
		leftFence = rightFence = nil
		for rightFence in ['()','[]','{}','<>']
			if 1 => index(rightFence,fence,'Char') != nil
				leftFence = ord(strShift rightFence,nil)
				rightFence = ord(rightFence)
				break
			endif
		endloop
		if nil? leftFence
			# If fence is a punctuation character, just double it up.
			if fence > ?\s && fence <= ?~ && (fence == ?_ || !wordChar?(fence))
				leftFence = rightFence = fence
			else
				status = -1 => message 'High',"Invalid fence '",chr(fence),"'"
				break
			endif
		endif

		# Save fence that was entered if interactive.
		interactive? and $lastFenceChar = fence

		# Try to move to other end.
		1 => setMark ?_
		if ($0 < 0 ? abs($0) => backWord : $0 => endWord) == false
			status = -1 => message 'High','Too many words to fence'
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
		oldMsgState => chgMode nil,'RtnMsg'
		return true
	endloop

	# Error or user cancelled.
	-1 => gotoMark ??
	oldMsgState => chgMode nil,'RtnMsg'
	status
endmacro

# Indent or outdent a block of lines specified by n argument, given Boolean value and number of tab stops.  If first argument is
# true, indent; otherwise, outdent.  If second argument is nil, prompt for number of tab stops.
constrain macro iodent(2)

	# Get number of tab stops.
	if !nil? $2
		stops = $2
	elsif empty?(stops = 1 => prompt('Tab stops',1))
		return nil
	elsif !numeric? stops
		return -1 => message 'High',"Invalid number '",stops,"'"
	elsif (stops = toInt(stops)) < 0
		return -1 => message 'High','Repeat count (',stops,') must be 0 or greater'
	endif
	if stops > 0
		oldMsgState = -1 => chgMode(nil,'RtnMsg')

		# Set region and indent or outdent it.
		$0 == 0 or -1 => selectLine($0)

		# Indent or outdent region.
		if $1
			eval 'stops => ',binding('Name','ESC )')
		else
			stops => outdentRegion
		endif
		oldMsgState => chgMode nil,'RtnMsg'
	endif
endmacro

# Indent a block of lines, given optional (when called interactively) number of tab stops.
macro indentLine(1) {usage: 'tab-stops',desc: 'Indent [-]n lines (default 1).  User is prompted for number of tab stops if\
 interactive.'}
	$0 => iodent true,interactive? ? nil : $1
endmacro

# Outdent a block of lines, given optional (when called interactively) number of tab stops.
macro outdentLine(1) {usage: 'tab-stops',desc: 'Outdent [-]n lines (default 1).  User is prompted for number of tab stops if\
 interactive.'}
	$0 => iodent false,interactive? ? nil : $1
endmacro

# Change indentation at beginning of current line without moving point.
macro chgIndent(0) {desc: 'Change indentation at beginning of current line without moving point.  If n < 0, abs(n) tabs are\
 deleted; if n > 0, n tabs are inserted; otherwise (n == 0), all white space is deleted.  n defaults to 1.'}
	$0 == defn and $0 = 1
	oldMsgState = -1 => chgMode(nil,'RtnMsg')
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
	oldMsgState => chgMode nil,'RtnMsg'
endmacro

# Wrap hook stub.
constrain macro hkWrap
	wrapWord
endmacro
setHook 'wrap',hkWrap

# Perform set intersection on two lists.  Return list of all elements in second list which are (if default n or n > 0) or are
# not (if n <= 0) in first list.  Arguments: (1), first list; (2), second list.
constrain macro select(2) {usage: 'list1,list2',desc: 'Perform set intersection on two lists (non-matching if n < 0).  Returns:\
 list of all elements in second list which are (if default n or n > 0) or are not (if n <= 0) in first list.'}
	exclude = $0 <= 0 && $0 != defn
	if empty? $1
		return exclude ? $2 : []
	endif
	if empty? $2
		return []
	endif

	result = []
	for item in $2
		include?(nil,$1,item) != exclude && push result,item
	endloop
	result
endmacro

# Get a buffer list to process: either all visible buffers (default n or n > 0) or only those also in buffer list in
# $fileListInfo for current screen (n <= 0).  Return false if error; otherwise, a two-element array containing Boolean "all
# buffers selected" and the buffer list (array).
constrain macro getBufList(0) {desc: 'Get list of all visible buffers (or only those also in buffer list in $fileListInfo for\
 current screen if n <= 0).  Returns: false if error; otherwise, a two-element array containing Boolean "all buffers selected"\
 and the buffer list (array).'}

	if $0 != defn && $0 <= 0
		if !defined?('Name','$fileListInfo') || length($fileListInfo) < $screenNum ||\
		 nil?(screenRec = $fileListInfo[$screenNum - 1])
			return -1 => message 'High','No buffer list found to search'
		endif
		searchList = select(1 => bufInfo(nil,'Brief'),screenRec[1])
		allBufs = false
	else
		searchList = 1 => bufInfo(nil,'Brief')
		allBufs = true
	endif
	empty?(searchList) ? -1 => message('High','No matching buffers found to search') : [allBufs,searchList]
endmacro

# Process a buffer list, given (1), buffer list; (2), name of macro to invoke on each buffer; and (3...), optional argument(s)
# to pass to macro.  Prompt user to continue or quit before each buffer is processed.  If user selects "do rest", remaining
# buffers are processed without prompting.  A message and false is returned if an error occurs; otherwise, true.  The macro (a),
# must operate non-interactively if n > 0; and (b), may return false and set an exception message if an error occurs, or an
# informational message (which is typically a blurb about what happened, like "3 substitutions"), or nil (no message).
constrain macro doBufList(2,) {usage: 'list,macro[,...]', desc: 'Invoke macro interactively (with specified arguments, if any)\
 on given buffer list.  Returns: false if error occurs; otherwise, true.'}

	# Do sanity checks.
	if empty?($1)
		return -1 => message 'High','Empty buffer list'
	elsif defined?('Name',$2) != 'macro'
		return -1 => message 'High',"No such macro '",$2,"'"
	endif

	oldMsgState = -1 => chgMode(nil,'RtnMsg')
	bList = shift($ARGV)
	macName = shift($ARGV)

	# Build argument list.
	argList = ''
	for arg in $ARGV
		strPush argList,',',quote arg
	endloop

	# If current buffer is in list, cycle buffer list so it's in the front.
	if include? nil,bList,$bufname
		tempList = []
		while (bufname = shift(bList)) != $bufname
			push tempList,bufname
		endloop
		bList = [$bufname] & bList & tempList
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
			nxtBuf = shift(bList)
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
					print(procMsg = sprintf("Processing buffer '%s'...",thisBuf))
				endif
				opMode == 1 and print("Processing buffer '",thisBuf,"'...")

				# Switch to buffer and call processor.
				selectBuf thisBuf
				beginBuf
				if (returnMsg = eval(join ' ',opMode,'=>',macName,argList)) == false
					oldMsgState => chgMode nil,'RtnMsg'
					return false
				endif
				++procCount

				# Display result if in "do the rest" mode.
				opMode == 1 and print(procMsg,' ',returnMsg ? returnMsg : 'done','.')

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
	                        	reply = termAttr ? prompt(msg,'Type: Char','TermAttr') : prompt(msg,'Type: Char')
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
	oldMsgState => chgMode nil,'RtnMsg'

	# Return result.
	if !goBack
		message 'Force',nil
	else
		opMode == 1 and returnMsg = '%u buffers processed' % procCount
		message 'High,NoWrap',nil?(returnMsg) ? 'Done!' : '%s.  Done!' % returnMsg
	endif
	true
endmacro

# Perform replace (n > 0) or queryReplace (otherwise) on current buffer and return custom result message.  Called from
# queryReplaceAll macro.
constrain macro queryReplaceOne(0)
	oldMsgState = 1 => chgMode nil,'RtnMsg'
	message 'Force',nil
	if $0 > 0
		replace $searchPat,$replacePat
	elsif not queryReplace $searchPat,$replacePat
		oldMsgState => chgMode nil,'RtnMsg'
		return false
	endif
	oldMsgState => chgMode nil,'RtnMsg'
	(i = 1 => index($ReturnMsg,?,,'Char')) != nil ? substr($ReturnMsg,0,i) : $ReturnMsg
endmacro

# Query-replace multiple buffers (files).  Use buffer list in $fileListInfo if n <= 0; otherwise, all visible buffers.
macro queryReplaceAll(0) {desc: 'Invoke ~bqueryReplace~B command on all visible buffers (or just those that matched the most\
 recent ~bfindFiles~B invocation if n <= 0).  Returns: false if error occurs; otherwise, true.'}

	# Get list of buffers to search: either all visible buffers or only those also in buffer list in $fileListInfo.
	if (searchList = $0 => getBufList) == false
		return false
	endif
	whichBufs = shift(searchList) ? 'all' : 'selected'
	searchList = shift(searchList)

	# Prompt for search and replace strings.
	for type in ["$searchPat:In #{whichBufs} buffers, query replace:SearchRing",'$replacePat:with:ReplaceRing']
		srVar = strShift(type,?:)
		newValue = strShift(type,?:)
		newValue = 1 => prompt(newValue,eval(srVar),'Type: %s' % type,'Delim: %s' % $searchDelim)
		if empty?(newValue) && srVar == '$searchPat'		# Nil or null string entered for $searchPat?
			return						# Yes, bail out.
		endif
		eval join ' ',srVar,'=',quote newValue			# No, assign new value.
	endloop

	# Process buffers.
	doBufList searchList,'queryReplaceOne'
endmacro

# Scan files that match given template in given directory.  Return [buffer-name,mark] of first file that contains given search
# pattern, or false if not found.  Any file that cannot be read (because of permissions, for example) is ignored.  Arguments:
# (1), directory; (2), filename template, or array of templates; (3), search pattern.
constrain macro locateFile(3) {usage: 'dir,glob,pat',desc: 'Find files matching glob (or array of glob patterns) in given\
 directory that contain search pattern.  Returns: false if pattern not found, or a two-element array containing: buffer name of\
 first file that contains the search pattern, and the mark that was set at the position where the pattern was found (or nil if\
 buffer was created).'}
	oldMsgState = -1 => chgMode(nil,'RtnMsg')

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
		for filename in glob($1 == '.' ? globPat : sprintf('%s/%s',$1,globPat))
			force bufname,isNewBuf = findFile(filename)
			if nil? bufname

				# File read failed... skip it.
				printf 'Cannot read file "%s" - skipping...',filename
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
	delSearchPat
	screenNum = $screenNum
	restoreScreen
	delScreen screenNum
	oldMsgState => chgMode nil,'RtnMsg'

	# Return result if search string found; otherwise, false.
	nil?(bufname) ? -1 => message('High','Not found') : [bufname,mark]
endmacro

# Open list of files returned from given shell command in background.  Return false if error; otherwise, list of buffer names.
constrain macro openFiles(1) {usage: 'shell-cmd',desc: 'Open list of files in background.  Returns: false if error occurs;\
 otherwise, list of buffer names.'}

	# Get file list into scratch buffer (one filename per line).
	if !(listBuf = 0 => readPipe $1)
		return -1 => message 'High','Not found'
	endif
	listBuf = listBuf[0]

	# Open the files (if any) in the background.
	oldMsgState = -1 => chgMode(nil,'RtnMsg')
	count = 0
	bList = []
	fileList = ''
	until nil? (filename = bgets(listBuf))
		if null? filename
			break
		endif
		fileInfo = 0 => findFile filename
		count++
		strPush fileList,', ',basename filename
		push bList,fileInfo[0]
	endloop

	# Clean up.
	delBuf listBuf
	oldMsgState => chgMode nil,'RtnMsg'
	message 'High',sprintf '%u file%s found%s',count,count == 1 ? '' : 's',count == 0 ? '' : ': %s' % fileList
	bList
endmacro

# Find files that match a shell template and optionally, a pattern, and open them in the background.  Save resulting buffer list
# (array) in $fileListInfo variable using current screen number - 1 as index.  The pattern is checked for the usual trailing
# option characters (:eipr) if any, and processed accordingly.
macro findFiles(0) {desc: 'Open all files matching a shell template and optionally, also matching a search pattern.  The search\
 pattern may contain zero or more of the trailing option characters "eipr" preceded by a colon.  The user is prompted for both\
 strings unless n <= 0, in which case the search pattern prompt is skipped.  If a search pattern is entered, the portion\
 preceding the trailing colon (if any) is assumed to be an extended regular expression and passed to the egrep shell command if\
 the "r" option is specified; otherwise, the pattern is passed to fgrep.  In either case, the command is invoked with the -i\
 switch if the "i" option is specified.  The files that match are opened in the background and their buffer names are saved in\
 a global variable so that they can be subsequently searched by the ~bqueryReplaceAll~B or ~bcNukePPLines~B macro, or stepped\
 through with the ~bprevListBuf~B and ~bnextListBuf~B macros.  Returns: false if error occurs; otherwise, true.'}

	# Prompt for filename template.  Use last one as default.  $fileListInfo is an array containing a nested array for each
	# screen.  Each nested array contains two arrays: [[filename-template,RE-pattern],[buf-name,...]].
	defined?('Name','$fileListInfo') or $fileListInfo = []
	if length($fileListInfo) < $screenNum || nil?(screenRec = $fileListInfo[$screenNum - 1])
		$fileListInfo[$screenNum - 1] = screenRec = array(2,[])
	endif
	fileTemplate = screenRec[0]
	fileTemplate[0] = 1 => prompt('Filename template',fileTemplate[0],'Type: File')
	if empty?(fileTemplate[0])
		return
	endif
	substr(fileTemplate[0],-1,1) == '/' and fileTemplate[0] &= '*'

	# Prompt for search pattern unless n <= 0.  If prompt is skipped or value is null, get all files that match template;
	# otherwise, save pattern in $searchPat for user's convenience, parse any trailing option characters, and convert to
	# correct grep command and options.
	if $0 <= 0 && $0 != defn
		pat = nil
	else
		x = 'f'
		swt = ''
		fileTemplate[1] = pat = 1 => prompt('Search pattern',fileTemplate[1],'Type: SearchRing',\
		 'Delim: %s' % $searchDelim)
		if not empty? pat
			if ($searchPat = pat) =~ '^(.+):([a-z]+)$' && !null?(match 1)
				pat = match(1)
				opts = match(2)
				until nil?(opt = strShift(opts,nil))
					if opt == 'i'
						swt = ' -i'
					elsif opt == 'r'
						x = 'e'
					elsif opt != 'e' && opt != 'p'
						return -1 => message 'High',"Unknown search option '",opt,"'"
					endif
				endloop
			endif
		endif
	endif

	# Open files and save buffer list in $fileListInfo.
	bList = openFiles(empty?(pat) ? (fileTemplate[0] == '*' ? 'ls -1' : "ls -1d #{fileTemplate[0]}") :\
	 "#{x}grep -l#{swt} #{shQuote pat} #{fileTemplate[0]}")
	if bList == false
		false
	else
		screenRec[1] = bList
		true
	endif
endmacro

# Find a file and open it in a new screen.  If n <= 0, set its buffer to read-only.
macro getFile(0) {desc: 'Find a file and open it in a new screen.  If n <= 0, also set its buffer to read-only.'}

	# Prompt for filename.
	if $0 == defn || $0 > 0
		prmt = 'Find file'
		cmd = 'findFile'
	else
		prmt = 'View file'
		cmd = 'viewFile'
	endif
	if (filename = prompt(prmt,'Type: File')) != nil
		selectScreen 0
		print '[%s]' % $ReturnMsg
		pause 1
		message 'Force',nil
		eval cmd,' "',filename,'"'
	endif
endmacro

# Display man page for word at point.
macro showMan(0) {desc: 'Display man page for word at point.'}
	if nil?(word = getWord)
		-1 => message 'High','Not in a word'
	else
		cols,rows = $TermSize
		shellCmd 'NoHdr','MANWIDTH=',cols,' man ',word,' | col -b'
	endif
endmacro

# Join line(s) with no spacing.
macro joinLines0(0) {desc: 'Join [-]n lines (default -1) with no spacing in between.'}
	$0 => joinLines nil
endmacro

# Add key to report buffer if applicable, given (1), buffer name; (2), prefix; (3), raw key; (4), suffix; and (5), apropos
# pattern.  Called from "showBindings" macro.
constrain macro addKey(5)
	keyLit = toStr($2) & $4
	if nil?($5) || index(keyLit,$5) != nil
		if nil?(name = binding('Name',keyLit))
			bprint $1,keyLit,$3 == ?~ ? "~\n" : "\n"
		else
			if $3 == ?~
				pad = 14
				keyLit &= '~'
			else
				pad = 13
			endif
			bprintf $1,"%-*s%s%s\n",pad,keyLit,defined?('Name',name) == 'macro' ? '@' : ' ',name
		endif
	endif
endmacro

# Create listing of all possible key bindings and what they are bound to.
macro showBindings(1) {usage: 'pat',desc: 'Generate list of key bindings matching given pattern in a new buffer and render it\
 per ~bselectBuf~B options (in a pop-up window if default n).  If pattern is plain text, match is successful if key literal\
 contains pat.  If pat is nil or a null string, all possible key bindings are listed.  Returns: ~bselectBuf~B values.'}
	oldMsgState = -1 => chgMode(nil,'RtnMsg')
	argCount = length($ARGV)

	# Get apropos search pattern and validate it.
	$0 == defn and $0 = -1
	pat = interactive? ? prompt('Apropos binding') : $1
	empty?(pat) and pat = nil

	# Get a buffer and initialize it.
	0 => selectBuf(bufname = '.Bindings')
	-1 => clearBuf bufname
	1 => chgBufAttr bufname,'TermAttr,Hidden'
	bprintf bufname,"~b%-13s%s~B\n",'Binding','Command or Macro'

	# Do the five groups.
	aMinus1 = ?a - 1
	bangMinus1 = ?! - 1
	for prefix in [nil,'C-c ','C-h ','C-x ','ESC ']
		bprintf bufname,"\n============= ~b%sPrefix~B =============\n",nil?(prefix) ? 'NO  ' : prefix
		c = -1
		while ++c <= 0x7F
			keyLit1 = c == 0 ? 'C-SPC' : c == 011 ? 'TAB' : c == 015 ? 'RTN' : c == 033 ? 'ESC' :\
			 c == 040 ? 'SPC' : c == 0x7F ? 'DEL' : c < 32 ? ('C-' & chr(aMinus1 + (c > 26 ? c - 32 : c))) : chr(c)
			addKey bufname,prefix,c,keyLit1,pat
		endloop

		shiftPrefix = nil
		for keyLit1 in [nil,'S-TAB']
			bprint bufname,"--------------------------------------\n"
			c = bangMinus1
			while ++c <= ?~
				if keyLit1
					--c
					shiftPrefix = 'S-'
				else
					keyLit1 = toStr(shiftPrefix) & 'FN' & chr(c)
				endif
				addKey bufname,prefix,c,keyLit1,pat
				keyLit1 = nil
			endloop
		endloop
	endloop

	# Render buffer.
	-1 => chgBufAttr bufname,'Changed'
	if $0 == -1
		1 => popBuf bufname,'Delete'
	else
		-1 => chgBufAttr bufname,'Hidden'
		1 => gotoLine 2,bufname
		$0 => selectBuf bufname
	endif
	oldMsgState => chgMode nil,'RtnMsg'
endmacro

# Create and pop a "help file not found" buffer, given (1), filename.
constrain macro hsNotFound(1)

	# Main script file not found.  Give user the bad news.
	1 => chgBufAttr (bufname = (0 => scratchBuf)[0]),'Hidden,TermAttr'
	bprint bufname,"~bHelp files not found!~B\n\n\nFile \"",$1,"\" was not found in $execPath:\n    ",\
	 $execPath,"\n\nPlease check your shell startup script.  MMPATH may need to be set.\nConsult file ",\
	 '"Install.txt" for more information.'
	bprint bufname,"\n\n\n\n\n\nPress ~uq~U (quit) to dismiss this window.\n\nAfter returning to your",\
	 " editing session, you can enter ~#uESC e~U to exit\n",getInfo('Editor'),' if desired (press the ~uesc~U',\
	 ' key, then ~ue~U).'
	-1 => chgBufAttr bufname,'Changed'
	1 => popBuf bufname,'Delete'
	nil
endmacro

# Offer help options.
constrain macro hkHelp(0) {desc: 'Enter Help System (or Advanced Help System if n argument).'}
	HelpSys = 'help.ms'
	MainMenu = 'hsMain'

	# Load help system if needed.
	if !defined?('Name',MainMenu) && (!(helpPath = xPathname(HelpSys)) || !require(helpPath))
		return hsNotFound HelpSys
	endif

	# Enter help system.
	eval '$0 => %s' % MainMenu
endmacro
setHook 'help',hkHelp

##### Bindings and Aliases #####
bindKey 'C-c TAB',chgIndent
bindKey 'C-x D',delFile
bindKey 'ESC #',enumLine
bindKey 'ESC "',fenceWord
bindKey 'C-x f',findFiles
bindKey 'C-x C-g',getFile
bindKey 'C-c )',indentLine
bindKey 'C-x C-j',joinLines0
bindKey 'C-c ]',nextListBuf
bindKey 'C-c (',outdentLine
bindKey 'C-h l',popFileList
bindKey 'C-c [',prevListBuf
bindKey 'ESC C-q',queryReplaceAll
bindKey 'C-x R',renameFile
bindKey 'C-h n',showBindings
bindKey 'C-h e',showCheatSheet
bindKey 'C-h M',showMan
bindKey 'C-h =',showVar
bindKey 'ESC C-\\',trimBuf
bindKey 'ESC ~',unchangeBuf

##### Load programming language libraries.
(path = xPathname('lang')) && require path

##### Load site-wide preferences.
if (path = xPathname('site'))
	require path

	# Create "word processing" line-join macros if $EndSentence variable not empty.
	if !empty?($EndSentence)

		# Join line(s) with extra spacing.
		macro wpJoinLines(0) {desc: 'Run the ~bjoinLines~B command with the $EndSentence global variable as an argument\
 to obtain extra spacing between joined lines.'}
			$0 => joinLines $EndSentence
		endmacro

		# Wrap line(s) with extra spacing.
		macro wpWrapLine(0) {desc: 'Run the ~bwrapLine~B command with the $EndSentence global variable as an argument\
 to obtain extra spacing between lines when the line block is rewrapped.'}
			$0 => wrapLine nil,$EndSentence
		endmacro

		bindKey 'C-c C-j',wpJoinLines
		bindKey 'C-c RTN',wpWrapLine
	endif
endif

oldMsgState => chgMode nil,'RtnMsg'
