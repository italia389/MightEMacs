# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# memacs.mm		Ver. 9.1.0
#	MightEMacs startup file.

##### Global variables. #####
$lastFenceChar = ?"		# Last fence character used in "fenceWord" macro.

oldMsgState = -1 => chgGlobalMode 'RtnMsg'

##### Macros #####

# Delete patterns from search ring until given pattern is on top.
constrain macro restoreSearchRing(1)
	while !empty?($searchPat) && $searchPat != $1
		deleteSearchPat
	endloop
	1 => message nil,nil
endmacro

# Unchange current buffer.
macro unchangeBuf(0) {desc: 'Clear the "changed" attribute of the current buffer.'}
	-1 => alterBufAttr $bufName,'Changed'
endmacro

# Trim all lines in current buffer.
macro trimBuf(0) {desc: 'Trim white space from end of all lines in current buffer.'}
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	markBuf
	oldMsgState => chgGlobalMode 'RtnMsg'
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
macro popFileList(0) {desc: 'Display file listing in a pop-up window in long form (or short form if n <= 0, without expanding\
 directories that match shell template if n != 0).'}
	lsSwitches1 = ($0 == defn || $0 > 0) ? 'l' : 'CF'	# Long or short form.
	lsSwitches2 = ($0 != defn && $0 != 0) ? 'ad' : 'a'	# Expand directories.
	template = prompt 'ls',nil,'f'
	nil?(template) || template == '.' and template = ''
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	-1 => readPipe 'ls -',lsSwitches1,lsSwitches2,' ',template
	oldMsgState => chgGlobalMode 'RtnMsg'
endmacro

# Rename a file on disk.
macro renameFile(2) {usage: 'name,new-name',desc: "Rename a file on disk.  If n argument, prompt for a buffer name and use its\
 filename; otherwise, prompt for the filename.  Rename file on disk only if n < 0."}
	negN = $0 < 0 && $0 != defn

	# Get old filename from buffer if applicable.
	if $0 != defn
		if interactive?
			if empty?(bufName = prompt 'Rename file in buffer',$bufName,'b')
				return
			endif
		elsif type?(bufName = $1) != 'string'
			return -1 => message nil,"Invalid buffer name '",0 => toStr(bufName),"'"
		endif

		# Have buffer name.  Get associated filename.
		if bufName == $bufName
			oldFilename = $bufFile
		elsif defined?(bufName) == 'buffer'
			selectBuf bufName
			oldFilename = $bufFile
			lastBuf
		else
			return -1 => message nil,"No such buffer '",bufName,"'"
		endif

		if null? oldFilename
			return -1 => message nil,"No filename set in buffer '",bufName,"'"
		endif

	# Get disk filename.
	elsif interactive?
		if empty?(oldFilename = prompt 'Rename disk file',nil,'^f')
			return
		endif
	elsif type?(oldFilename = $1) != 'string'
		return -1 => message nil,'Invalid filename "',0 => toStr(oldFilename),'"'
	endif

	# Have old filename.  Get new one.
	if interactive?
		if empty?(newFilename = prompt ($0 == defn ? 'to' : negN ? 'New disk filename' : 'New filename'),\
		 oldFilename,'^f')
			return
		endif
	elsif type?(newFilename = $2) != 'string'
		return -1 => message nil,'Invalid filename "',0 => toStr(newFilename),'"'
	endif

	# Rename file on disk if it exists.
	if (exists = stat?(oldFilename,'f'))
		if !shellCmd('mv ',shQuote(oldFilename),' ',shQuote(newFilename))
			return false
		endif
	elsif negN
		return -1 => message nil,'No such file "',oldFilename,'"'
	endif

	# Rename buffer file, if applicable.
	$0 >= 0 and 1 => setBufFile bufName,newFilename

	# Return result message.
	message nil,(!exists ? 'File "%s" does not exist, buffer filename changed' % oldFilename :\
	 negN ? 'Disk file renamed' : 'File renamed')
endmacro

# Delete a file on disk and optionally, an associated buffer.
macro deleteFile(1) {usage: 'name',desc: 'Delete a file on disk if n <= 0 (and use "rm -rf" command if n < 0), or delete a\
 buffer and its associated file if n > 0.  n defaults to zero.'}
	$0 == defn and $0 = 0

	if $0 <= 0
		# Get filename.
		if interactive?
			if empty?(filename = prompt ($0 < 0 ? 'Force delete file or directory' : 'Delete file'),nil,'^f')
				return
			endif
		elsif type?(filename = $1) != 'string'
			return -1 => message nil,'Invalid filename "',0 => toStr(filename),'"'
		endif

		# Delete file.
		if not stat? filename,'e'
			-1 => message nil,'No such file "',filename,'"'
		else
			kind = stat?(filename,'d') ? 'Directory' : 'File'
			if shellCmd ($0 < 0 ? 'rm -rf ' : 'rm '),shQuote(filename)
				message nil,kind,' deleted'
			endif
		endif
	else
		# Get buffer name.
		if interactive?
			if empty?(bufName = prompt 'Buffer name',nil,'b')
				return
			endif
		elsif type?(bufName = $1) != 'string'
			return -1 => message nil,"Invalid buffer name '",0 => toStr(bufName),"'"
		endif

		# Have buffer name.  Get associated filename.
		if bufName != $bufName && defined?(bufName) == 'buffer'
			selectBuf bufName
			filename = $bufFile
			lastBuf
		else
			filename = nil
		endif

		# Try to delete buffer (which may fail).
		if deleteBuf(bufName) == 0
			return
		endif

		# Buffer deleted.  Now delete file if it exists.
		if !nil?(filename) && stat? filename,'e'
			if shellCmd 'rm ',shQuote(filename)
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
	if !globalMode?('Safe')

		# Check if output file exists, is a symbolic or hard link, and is not in the $linksToKeep list.
		if stat?($2,'Ll') && !include?($linksToKeep,filename = 0 => pathname($2))

			# Outfile is a link and not previously brought to user's attention.  Ask user if the link should be
			# broken.  If yes, delete the output file; otherwise, remember response by adding absolute pathname to
			# $linksToKeep.
			type = stat?(filename,'L') ? 'symbolic' : 'hard'
			p = sprintf('Break %s link for file "%s" on output? (y,n)',type,$2)
			if prompt(p,?n,'c') == ?y
				shellCmd 'rm ',filename
			else
				push $linksToKeep,filename
			endif
		endif
	endif
endmacro
setHook 'write',hkWrite			# Set write hook.
$linksToKeep = []			# Link pathnames to leave in place, per user's request.

# Get buffer list from $grepInfo for current screen and switch to next or previous buffer in that list n times, given "forward?"
# argument.  If n < 0, delete current buffer after buffer switch and remove buffer name from list in $grepInfo.  Return name of
# last buffer switched to.
constrain macro pnGrepBuf(1)
	nukeBuf = nil

	# Buffer list in $grepInfo defined and not empty?
	if !defined?('$grepInfo') || length($grepInfo) < $screenNum || nil?(grepRec = $grepInfo[$screenNum - 1]) ||\
	 empty?(grepList = grepRec[1])
		return -1 => message nil,'No buffer list'
	elsif $0 == defn
		$0 = 1
	elsif $0 == 0
		return
	elsif $0 < 0
		nukeBuf = $bufName		# Delete current buffer after buffer switch.
	endif

	# Set scanning parameters such that i + incr yields next or previous buffer in list.
	incr = $1 ? 1 : -1
	if include? grepList,$bufName
		i = -1
		for name in grepList
			++i
			if name == $bufName
				break
			endif
		endloop
	else
		i = $1 ? -1 : length(grepList)
	endif

	# Get next or previous buffer n times.
	loop
		i += incr
		if i == length(grepList)
			i = 0
		elsif i < 0
			i = length(grepList) - 1
		endif
		if nukeBuf || --$0 == 0
			break
		endif
	endloop

	# Switch to it.
	selectBuf grepList[i]

	# Delete buffer that was exited if requested.
	if nukeBuf
		force deleteBuf nukeBuf
		if defined?(nukeBuf) != 'buffer'

			# Buffer was deleted.  Remove it from the list if present.
			if include? grepList,nukeBuf
				if (j = length(grepList) - 1) > 0
					i = -1
					for name in grepList
						++i
						if name == nukeBuf
							break
						endif
					endloop
					while i < j
						grepList[i] = grepList[i + 1]
						++i
					endloop
				endif
				pop grepList
			endif
			message nil,"Buffer '#{nukeBuf}' deleted"
		endif
	endif
	$bufName
endmacro

# Switch to next buffer in $grepInfo n times.
macro nextGrepBuf(0) {desc: 'Switch to next buffer in list created from most recent ~bgrepFiles~0 invocation n times.  If\
 n < 0, switch once and delete buffer that was exited.  Returns: name of last buffer switched to.'}
	$0 => pnGrepBuf true
endmacro

# Switch to previous buffer in $grepInfo n times.
macro prevGrepBuf(0) {desc: 'Switch to previous buffer in list created from most recent ~bgrepFiles~0 invocation n times.  If\
 n < 0, switch once and delete buffer that was exited.  Returns: name of last buffer switched to.'}
	$0 => pnGrepBuf false
endmacro

# Show a variable.
macro showVar(0) {desc: 'Show a variable and its value on the message line.'}
	varName = prompt 'Show variable',nil,'V'
	if !empty? varName
		if defined?(varName) != 'variable'
			-1 => message nil,"No such variable '",varName,"'"
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
	elsif empty?(fence = prompt 'Fence char',$lastFenceChar,'c')
		return nil
	endif

	# It's a go...
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
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
			-1 => message nil,'No word found'
			break
		endif

		# Dot is now at starting point.  Determine left and right fences.
		leftFence = rightFence = nil
		for rightFence in ['()','[]','{}','<>']
			if -1 => index(rightFence,fence) != nil
				leftFence = ord strShift rightFence,nil
				rightFence = ord rightFence
				break
			endif
		endloop
		if nil? leftFence
			# If fence is a punctuation character, just double it up.
			if fence > ?\s && fence <= ?~ && (fence == ?_ || !wordChar?(fence))
				leftFence = rightFence = fence
			else
				status = -1 => message nil,"Invalid fence '",chr(fence),"'"
				break
			endif
		endif

		# Save fence that was entered if interactive.
		interactive? and $lastFenceChar = fence

		# Try to move to other end.
		1 => setMark ?_
		if ($0 < 0 ? abs($0) => backWord : $0 => endWord) == false
			status = -1 => message nil,'Too many words to fence'
			deleteMark ?_
			break
		endif

		# All is well.  Insert left or right fence, move back to starting point and insert other fence.
		if $0 < 0
			insert chr leftFence
			-1 => gotoMark ?_
			insert chr rightFence
			deleteMark ??
		else
			insert chr rightFence
			1 => setMark ??
			-1 => gotoMark ?_
			insert chr leftFence
			-1 => gotoMark ??
		endif
		oldMsgState => chgGlobalMode 'RtnMsg'
		return true
	endloop

	# Error or user cancelled.
	-1 => gotoMark ??
	oldMsgState => chgGlobalMode 'RtnMsg'
	status
endmacro

# Indent or outdent a block of lines specified by n argument, given Boolean value and number of tab stops.  If first argument is
# true, indent; otherwise, outdent.  If second argument is nil, prompt for number of tab stops.
constrain macro iodent(2)

	# Get number of tab stops.
	if !nil? $2
		stops = $2
	elsif empty?(stops = prompt 'Tab stops','1')
		return nil
	elsif !numeric? stops
		return -1 => message nil,"Invalid number '",stops,"'"
	elsif (stops = toInt stops) < 0
		return -1 => message nil,'Repeat count (',stops,') must be 0 or greater'
	endif
	if stops > 0
		oldMsgState = -1 => chgGlobalMode 'RtnMsg'

		# Set region and indent or outdent it.
		$0 == 0 or -1 => selectLine($0)

		# Indent or outdent region.
		if $1
			eval 'stops => ',binding('ESC )')
		else
			stops => outdentRegion
		endif
		oldMsgState => chgGlobalMode 'RtnMsg'
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
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	1 => setMark ??
	beginLine

	if $0 == 0
		deleteWhite
	elsif $0 < 0
		$0 => deleteBackTab
	else
		$0 => tab
	endif

	-1 => gotoMark ??
	oldMsgState => chgGlobalMode 'RtnMsg'
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
		include?($1,item) != exclude && push result,item
	endloop
	result
endmacro

# Get a buffer list to process: either all visible buffers (default n or n > 0) or only those also in buffer list in $grepInfo
# for current screen (n <= 0).  Return false if error; otherwise, a two-element array containing Boolean "all buffers selected"
# and the buffer list (array).
constrain macro getBufList(0) {desc: 'Get list of all visible buffers (or only those also in buffer list in $grepInfo for\
 current screen if n <= 0).  Returns: false if error; otherwise, a two-element array containing Boolean "all buffers selected"\
 and the buffer list (array).'}

	if $0 != defn && $0 <= 0
		if !defined?('$grepInfo') || length($grepInfo) < $screenNum || nil?(grepRec = $grepInfo[$screenNum - 1])
			return -1 => message nil,'No buffer list found to search'
		endif
		searchList = select getInfo('Buffers'),grepRec[1]
		allBufs = false
	else
		searchList = getInfo('Buffers')
		allBufs = true
	endif
	empty?(searchList) ? -1 => message(nil,'No matching buffers found to search') : [allBufs,searchList]
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
		return -1 => message nil,'Empty buffer list'
	elsif defined?($2) != 'macro'
		return -1 => message nil,"No such macro '",$2,"'"
	endif

	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	bList = shift $ARGV
	macName = shift $ARGV

	# Build argument list.
	argList = ''
	for arg in $ARGV
		strPush argList,',',quote arg
	endloop

	# If current buffer is in list, cycle buffer list so it's in the front.
	if include? bList,$bufName
		tempList = []
		while (bufName = shift bList) != $bufName
			push tempList,bufName
		endloop
		bList = [$bufName] & bList & tempList
	endif

	# Save current buffer, position, and loop through buffers.
	origBuf = $bufName
	1 => setMark ??
	goBack = true
	opMode = defn
	procCount = 0
	returnMsg = nxtBuf = nil
	loop
		loop
			thisBuf = nxtBuf
			nxtBuf = shift bList
			nPrompt = defn

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
					oldMsgState => chgGlobalMode 'RtnMsg'
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
					reply = nPrompt => prompt msg,nil,'c'
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
						msg = '"~uSPC~U|~uy~U ~bYes~0, ~u!~U ~bDo rest~0, ~uESC~U|~uq~U ~bStop here~0,\
 ~u.~U|~un~U ~bStop and go back~0, ~u?~U ~bHelp~0: '
						nPrompt = 1
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
	deleteMark ??
	oldMsgState => chgGlobalMode 'RtnMsg'

	# Return result.
	if !goBack
		1 => message nil,nil
	else
		opMode == 1 and returnMsg = '%u buffers processed' % procCount
		1 => message 'NoWrap',nil?(returnMsg) ? 'Done!' : '%s.  Done!' % returnMsg
	endif
	true
endmacro

# Perform replace (n > 0) or queryReplace (otherwise) on current buffer and return custom result message.  Called from
# queryReplaceAll macro.
constrain macro queryReplaceOne(0)
	if $0 > 0
		replace $searchPat,$replacePat
	elsif not queryReplace $searchPat,$replacePat
		return false
	endif
	(i = index $ReturnMsg,',') != nil ? substr($ReturnMsg,0,i) : $ReturnMsg
endmacro

# Query-replace multiple buffers (files).  Use buffer list in $grepInfo if n <= 0; otherwise, all visible buffers.
macro queryReplaceAll(0) {desc: 'Invoke ~bqueryReplace~0 command on all visible buffers (or just those that matched the most\
 recent ~bgrepFiles~0 invocation if n <= 0).  Returns: false if error occurs; otherwise, true.'}

	# Get list of buffers to search: either all visible buffers or only those also in buffer list in $grepInfo.
	if (searchList = $0 => getBufList) == false
		return false
	endif
	whichBufs = shift(searchList) ? 'all' : 'selected'
	searchList = shift searchList

	# Prompt for search and replace strings.
	for type in ["$searchPat:In #{whichBufs} buffers, query replace:S",'$replacePat:with:R']
		srVar = strShift type,?:
		newValue = strShift type,?:
		newValue = prompt newValue,"#{eval srVar}",type,$searchDelim
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
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'

	# Prepare for search in new screen.
        $searchPat = $3
	saveScreen
	1 => selectScreen
	startBuf = $bufName

	# Get glob patterns into an array, then loop through them.
	type?($2) == 'array' or $2 = [$2]
	for globPat in $2

		# Get file list for current glob pattern and scan the files (if any).
		bufName = mark = nil
		for filename in glob($1 == '.' ? globPat : sprintf('%s/%s',$1,globPat))
			force bufName,isNewBuf = findFile filename
			if nil? bufName

				# File read failed... skip it.
				-1 => printf 'Cannot read file "%s" - skipping...',filename
				pause 1
				if $bufFile == filename && 1 => bufSize == 0
					bufName = $bufName
					selectBuf startBuf
					deleteBuf bufName
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
					deleteBuf bufName
				else
					gotoMark ?`
					selectBuf startBuf
				endif
			endif
			bufName = nil
		endloop
	endloop

	# Clean up.
	deleteSearchPat
	screenNum = $screenNum
	restoreScreen
	deleteScreen screenNum
	oldMsgState => chgGlobalMode 'RtnMsg'

	# Return result if search string found; otherwise, false.
	nil?(bufName) ? -1 => message(nil,'Not found') : [bufName,mark]
endmacro

# Open list of files returned from given shell command in background.  Return false if error; otherwise, list of buffer names.
constrain macro openFiles(1) {usage: 'shell-cmd',desc: 'Open list of files in background.  Returns: false if error occurs;\
 otherwise, list of buffer names.'}

	# Get file list into scratch buffer (one filename per line).
	if !(listBuf = 0 => readPipe $1)
		return -1 => message nil,'Not found'
	endif
	listBuf = listBuf[0]

	# Open the files (if any) in the background.
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	count = 0
	bList = []
	fileList = ''
	until nil? (filename = bgets listBuf)
		if null? filename
			break
		endif
		fileInfo = 0 => findFile filename
		count++
		strPush fileList,', ',basename filename
		push bList,fileInfo[0]
	endloop

	# Clean up.
	deleteBuf listBuf
	oldMsgState => chgGlobalMode 'RtnMsg'
	1 => message nil,sprintf '%u file%s found: %s',count,count == 1 ? '' : 's',fileList
	bList
endmacro

# Find files that match a shell template and optionally, a pattern, and open them in the background.  Save resulting buffer list
# (array) in $grepInfo variable using current screen number - 1 as index.  The pattern is checked for the usual trailing option
# characters (:eipr) if any, and processed accordingly.
macro grepFiles(0) {desc: 'Open all files matching a shell template and optionally, also matching a search pattern.  The user\
 is prompted for both strings.  The search pattern may contain zero or more of the trailing option characters "eipr" preceded\
 by a colon.  If a search pattern is entered, the portion preceding the trailing colon (if any) is assumed to be an extended\
 regular expression and passed to the egrep shell command if the "r" option is specified; otherwise, the pattern is passed to\
 fgrep.  In either case, the shell command is invoked with the -i switch if the "i" option is specified.  The files that match\
 are opened in the background and their buffer names are saved in a global variable so that they can be subsequently searched\
 by the ~bqueryReplaceAll~0 or ~bcNukePPLines~0 macro.  Returns: false if error occurs; otherwise, true.'}

	# Prompt for filename template.  Use last one as default.  $grepInfo is an array containing a nested array for each
	# screen.  Each nested array contains two arrays: [[filename-template,RE-pattern],[buf-name,...]].
	defined?('$grepInfo') or $grepInfo = []
	if length($grepInfo) < $screenNum || nil?(grepRec = $grepInfo[$screenNum - 1])
		$grepInfo[$screenNum - 1] = grepRec = array(2,[])
	endif
	grepTemplate = grepRec[0]
	grepTemplate[0] = prompt 'Filename template',grepTemplate[0],'f'
	if empty?(grepTemplate[0])
		return
	endif
	substr(grepTemplate[0],-1,1) == '/' and grepTemplate[0] &= '*'

	# Prompt for search pattern.  If null, get all files that match template; otherwise, save pattern in $searchPat for
	# user's convenience, parse any trailing option characters, and convert to correct grep command and options.
	x = 'f'
	swt = ''
	grepTemplate[1] = pat = prompt 'Search pattern',grepTemplate[1],'S',$searchDelim
	if not empty? pat
		if ($searchPat = pat) =~ '^(.+):([a-z]+)$' && !null?(match 1)
			pat = match 1
			opts = match 2
			until nil?(opt = strShift(opts,nil))
				if opt == 'i'
					swt = ' -i'
				elsif opt == 'r'
					x = 'e'
				elsif opt != 'e' && opt != 'p'
					return -1 => message nil,"Unknown search option '",opt,"'"
				endif
			endloop
		endif
	endif

	# Open files and save buffer list in $grepInfo.
	bList = openFiles(empty?(pat) ? (grepTemplate[0] == '*' ? 'ls -1' : "ls -1d #{grepTemplate[0]}") :\
	 "#{x}grep -l#{swt} #{shQuote pat} #{grepTemplate[0]}")
	if bList == false
		false
	else
		grepRec[1] = bList
		true
	endif
endmacro

# Join line(s) with no spacing.
macro joinLines0(0) {desc: 'Join [-]n lines (default -1) with no spacing in between.'}
	$0 => joinLines nil
endmacro

# Create listing of all possible key bindings and what they are bound to.
macro showBindings(0,1) {usage: '[pat]',desc: 'Generate list of all possible key bindings in a new buffer and render it per\
 ~bselectBuf~0 options (in a pop-up window if default n, matching pattern pat if n argument).  Returns: ~bselectBuf~0 values.'}
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	argCount = length $ARGV
	wrong = 'Wrong number of arguments'

	# Get apropos search pattern, if any.
	if $0 == defn
		if argCount == 0
			$0 = -1
			pat = nil
		else
			return -1 => message nil,wrong
		endif
	else
		if interactive?
			pat = prompt('Apropos binding')
		elsif argCount == 1
			pat = $1
		else
			return -1 => message nil,wrong
		endif

		# Validate pattern.
		if pat != nil && (type?(pat) != 'string' || null?(pat))
			return -1 => message nil,"Invalid apropos pattern '",0 => toStr(pat),"'"
		endif
	endif

	# Get a buffer and initialize it.
	0 => selectBuf(bufName = '.Bindings')
	1 => clearBuf bufName
	1 => alterBufAttr bufName,'TermAttr','Hidden'
	bprintf bufName,"~b%-13s%s~0\n",'Binding','Command or Macro'

	# Do the five groups.
	A = ord 'A'
	Z = ord 'Z'
	aMinus1 = (ord 'a') - 1
	bangMinus1 = (ord '!') - 1
	tilde = ord '~'
	for prefix in [nil,'C-c ','C-h ','C-x ','ESC ']
		bprintf bufName,"\n============= ~b%sPrefix~0 =============\n",nil?(prefix) ? 'NO  ' : prefix
		c = -1
		while ++c <= 0x7F
			keyLit1 = c == 0 ? 'C-SPC' : c == 011 ? 'TAB' : c == 015 ? 'RTN' : c == 033 ? 'ESC' :\
			 c == 040 ? 'SPC' : c == 0x7F ? 'DEL' : c < 32 ? ('C-' & chr(aMinus1 + (c > 26 ? c - 32 : c))) : chr(c)
			keyLit = toStr(prefix) & keyLit1
			if nil?(pat) || index(keyLit,pat) != nil
				if nil?(name = binding keyLit)
					bprint bufName,keyLit,"\n"
				else
					bprintf bufName,"%-13s%s%s\n",keyLit,defined?(name) == 'macro' ? '@' : ' ',name
				endif
			endif
		endloop

		shiftPrefix = nil
		for keyLit1 in [nil,'S-TAB']
			bprint bufName,"--------------------------------------\n"
			c = bangMinus1
			while ++c <= tilde
				if keyLit1
					--c
					shiftPrefix = 'S-'
				else
					keyLit1 = toStr(shiftPrefix) & 'FN' & chr(c)
				endif
				keyLit = toStr(prefix) & keyLit1
				if nil?(pat) || index(keyLit,pat) != nil
					if nil?(name = binding keyLit)
						bprint bufName,keyLit,"\n"
					else
						bprintf bufName,"%-13s%s%s\n",keyLit,defined?(name) == 'macro' ? '@' : ' ',name
					endif
				endif
				keyLit1 = nil
			endloop
		endloop
	endloop

	# Render buffer.
	-1 => alterBufAttr bufName,'Changed'
	if $0 == -1
		1 => popBuf bufName,'d'
	else
		-1 => alterBufAttr bufName,'Hidden'
		1 => gotoLine 2,bufName
		$0 => selectBuf bufName
	endif
	oldMsgState => chgGlobalMode 'RtnMsg'
endmacro

# Offer help options.
constrain macro hkHelp(0) {desc: 'Enter Help System (or Advanced Help System if n argument).'}
	HelpFile = 'help.mm'
	MainMenu = 'hsMain'

	# Load help system if needed.
	if !defined?(MainMenu) && (!(helpPath = xPathname HelpFile) || !require(helpPath))
		editor = getInfo 'Editor'

		# Main script file not found.  Give user the bad news.
		1 => alterBufAttr (bufName = (0 => scratchBuf)[0]),'Hidden','TermAttr'
		bprint bufName,"~bHelp files not found!~0\n\n\nFile \"",HelpFile,"\" was not found in $execPath:\n    ",\
		 $execPath,"\n\nPlease check your shell startup script.  MMPATH may need to be set.\nConsult file ",\
		 '"Install.txt" for more information.'
		bprint bufName,"\n\n\n\n\n\nPress ~uq~U (quit) to dismiss this window.\n\nAfter returning to your",\
		 " editing session, you can enter ~#uC-x C-c~U to exit\n",editor,' if desired (hold down the',\
		 ' ~ucontrol~U key, and press ~ux~U, then ~uc~U).'
		-1 => alterBufAttr bufName,'Changed'
		1 => popBuf bufName,'d'
		return
	endif

	# Enter help system.
	eval '$0 => %s' % MainMenu
endmacro
setHook 'help',hkHelp

##### Bindings and aliases #####
bindKey 'C-c TAB',chgIndent
bindKey 'C-x D',deleteFile
bindKey 'ESC #',enumLine
bindKey 'ESC {',fenceWord
bindKey 'C-x C-g',grepFiles
bindKey 'C-c )',indentLine
bindKey 'C-x C-j',joinLines0
bindKey 'C-c ]',nextGrepBuf
bindKey 'C-c (',outdentLine
bindKey 'C-h l',popFileList
bindKey 'C-c [',prevGrepBuf
bindKey 'ESC C-q',queryReplaceAll
bindKey 'C-x R',renameFile
bindKey 'C-h n',showBindings
bindKey 'C-h =',showVar
bindKey 'ESC C-\\',trimBuf
bindKey 'ESC ~',unchangeBuf

##### Load programming language libraries. #####
(path = xPathname 'lang') && require path

##### Load site-wide preferences. #####
if (path = xPathname 'site')
	require path

	# Create "word processing" line-join macros if $EndSentence variable not empty.
	if !empty?($EndSentence)

		# Join line(s) with extra spacing.
		macro wpJoinLines(0) {desc: 'Run the ~bjoinLines~0 command with the $EndSentence global variable as an argument\
 to obtain extra spacing between joined lines.'}
			$0 => joinLines $EndSentence
		endmacro

		# Wrap line(s) with extra spacing.
		macro wpWrapLine(0) {desc: 'Run the ~bwrapLine~0 command with the $EndSentence global variable as an argument\
 to obtain extra spacing between lines when the line block is rewrapped.'}
			$0 => wrapLine nil,$EndSentence
		endmacro

		bindKey 'C-c C-j',wpJoinLines
		bindKey 'C-c RTN',wpWrapLine
	endif
endif

oldMsgState => chgGlobalMode 'RtnMsg'
