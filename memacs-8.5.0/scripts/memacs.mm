# (c) Copyright 2017 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# memacs.mm		Ver. 8.5.0
#	MightEMacs startup file.

formerMsgState = -1 => alterGlobalMode 'msg'

##### Set constants. #####
$HelpFiles = ['memacs-help','memacs','memacs-guide','memacs-macros']
				# Names of help file, man page, and User Guide man page.
$LangInfo = ['c:$ModeC','memacs:$ModeMEMacs','perl:$ModePerl','ruby:$ModeRuby','shell:$ModeShell']
				# Language names and masks.
$LangMask = $ModeC | $ModeMEMacs | $ModePerl | $ModeRuby | $ModeShell
				# Language modes' mask.

##### Set variables. #####
$activeToolbox = nil		# Name of currently active language toolbox; for example, "ruby".  Used by tbActivate macro.
$lastFenceChar = '"'		# Last fence character used in "fenceWord" macro.

##### Define macros. #####

# Print working directory.
macro pwd(0) {desc: 'Print working directory'}
	print $workDir
endmacro

# Pop a buffer.
macro popBuf(1) {usage: 'name',desc: 'Display given buffer in a pop-up window'}
	($0 == defn || $0 >= 0) and $0 = -2
	length($ArgVector) > 0 ? $0 => selectBuf($1) : $0 => run selectBuf
endmacro

# Pop a file.
macro popFile(1) {usage: 'filename',desc: 'Display given file in a pop-up window'}
	($0 == defn || $0 >= 0) and $0 = -2
	length($ArgVector) > 0 ? $0 => readFile($1) : $0 => run readFile
endmacro

# Pop a file listing.
macro popFileList(0) {desc: 'Display file listing in a pop-up window'}
	switches = ($0 == defn || $0 > 0) ? 'la' : 'lad'
	template = prompt 'ls',nil,'f'
	nil?(template) || template == '.' and template = ''
	formerMsgState = -1 => alterGlobalMode 'msg'
	-1 => readPipe 'ls -',switches,' ',template
	formerMsgState => alterGlobalMode 'msg'
endmacro

# Rename file associated with current buffer.  If n <= 0, rename file on disk only; otherwise, rename both file on disk and
# buffer filename.
macro renameFile(1) {usage: 'new-name',desc: 'Rename file associated with current buffer (on disk only if n <= 0)'}


	# No current filename?
	if null? $bufFile
		return -1 => notice 'No current filename'
	endif

	# Get new filename.
	bothNames = ($0 == defn || $0 > 0)
	if length($ArgVector) > 0
		if empty?(newFile = $1)
			return -1 => notice 'Filename cannot be null'
		endif
	elsif empty?(newFile = prompt (bothNames ? 'New filename' : 'New disk filename'),nil,'^f')
		return
	endif

	# Rename file on disk if it exists.
	(exists = stat?($bufFile,"f")) and shellCmd 'mv ',shQuote($bufFile),' ',shQuote(newFile)

	# Rename buffer file if applicable.
	if bothNames
		setBufFile(newFile)
	elsif exists
		notice 'Disk file renamed'
	else
		-1 => notice "File \"#{$bufFile}\" does not exist, no changes made"
	endif
endmacro

# Switch to next or previous buffer in $grepList n times, given "forward?" argument.  If n < 0, delete current buffer after
# buffer switch.
macro pnGrepBuf(1)
	nukeBuf = nil

	# $grepList defined and not empty?
	if !defined?('$grepList') || empty?($grepList)
		return -1 => notice 'No buffer list'
	elsif $0 == defn
		$0 = 1
	elsif $0 == 0
		return
	elsif $0 < 0
		nukeBuf = $bufName		# Delete current buffer after buffer switch.
	endif

	# Set scanning parameters such that i + incr yields next or previous buffer in list.
	incr = $1 ? 1 : -1
	if include? $grepList,$bufName
		i = -1
		for name in $grepList
			++i
			if name == $bufName
				break
			endif
		endloop
	else
		i = $1 ? -1 : length($grepList)
	endif

	# Get next or previous buffer n times.
	loop
		i += incr
		if i == length($grepList)
			i = 0
		elsif i < 0
			i = length($grepList) - 1
		endif
		if nukeBuf || --$0 == 0
			break
		endif
	endloop

	# Switch to it.
	selectBuf $grepList[i]

	# Delete buffer that was exited if requested.
	if nukeBuf
		force deleteBuf nukeBuf
		defined?(nukeBuf) or notice "Deleted buffer '#{nukeBuf}'"
	endif
endmacro

# Switch to next buffer in $grepList n times.
macro nextGrepBuf(0) {desc: 'Switch to next buffer in $grepList n times'}
	$0 => pnGrepBuf true
endmacro

# Switch to previous buffer in $grepList n times.
macro prevGrepBuf(0) {desc: 'Switch to previous buffer in $grepList n times'}
	$0 => pnGrepBuf false
endmacro

# Show a variable.
macro showVar(0) {desc: 'Show a variable and its value on the message line'}
	varName = prompt 'Show var',nil,'V'
	if !empty? varName
		if defined?(varName) != 'variable'
			-1 => notice "No such variable '",varName,"'"
		else
			print varName,' = ',quote eval varName
		endif
	endif
endmacro

# Fence word(s); that is, put punctuation or fence characters around one or more words at point.
macro fenceWord(1) {usage: 'char',desc: 'Put given punctuation character or fence pairs around [-]n words at point'}
	formerMsgState = -1 => alterGlobalMode 'msg'
	loop
		# Move dot to starting point.
		$0 == defn || $0 == 0 and $0 = 1
		1 => setMark '?'
		if wordChar? $lineChar
			status = ($lineOffset == 0 || !wordChar?(subLine -1,1)) ? true : ($0 > 0 ? backWord : endWord)
		elsif $0 > 0
			status = forwWord
		elsif((status = backWord) != false)
			status = endWord
		endif

		# Any word(s) in buffer?
		if status == false
			-1 => notice 'No word found'
			break
		endif

		# Dot is now at starting point.  Determine left and right fences.
		if length($ArgVector) > 0
			fence = $1
		elsif null?(fence = prompt 'Fence char',$lastFenceChar,'c')
			status = nil
			break
		endif
		leftFence = rightFence = nil
		for rightFence in ['()','[]','{}','<>']
			if index(rightFence,fence) != nil
				leftFence = strShift rightFence,''
				break
			endif
		endloop
		if nil? leftFence
			# If fence is a punctuation character, just double it up.
			if (o = ord(fence)) > 040 && o <= 0176 && (fence == '_' || !wordChar?(fence))
				leftFence = rightFence = fence
			else
				status = -1 => notice "Invalid fence '",fence,"'"
				break
			endif
		endif

		# Save fence that was entered if interactive.
		length($ArgVector) == 0 and $lastFenceChar = fence

		# Try to move to other end.
		1 => setMark '_'
		if ($0 < 0 ? abs($0) => backWord : $0 => endWord) == false
			status = -1 => notice 'Too many words to fence'
			break
		endif

		# All is well.  Insert left or right fence, move back to starting point and insert other fence.
		if $0 < 0
			insert leftFence
			1 => gotoMark '_'
			insert rightFence
			deleteMark '?'
		else
			insert rightFence
			1 => setMark '?'
			1 => gotoMark '_'
			insert leftFence
			1 => gotoMark '?'
		endif
		formerMsgState => alterGlobalMode 'msg'
		return true
	endloop

	# Error or user cancelled.
	1 => gotoMark '?'
	formerMsgState => alterGlobalMode 'msg'
	status
endmacro

# Find first window and (optionally) screen displaying given buffer.  If n > 0, search all screens and return
# [screen-num,wind-num]; otherwise, search current screen only and return wind-num (integer).  In either case, return nil if
# no window found.
macro bufWind(1)
	for windInfo in 1 => windList
		screenNum,windNum,bufName = windInfo
		if bufName == $1
			if $0 > 0
				return [screenNum,windNum]
			elsif screenNum == $screenNum
				return windNum
			endif
		endif
	endloop
	nil
endmacro

# Indent or outdent a block of lines specified by n argument, given Boolean value and number of tab stops.  If first argument is
# true, indent; otherwise, outdent.  If second argument is nil, prompt for number of tab stops.
macro iodent(2)

	# Get number of tab stops.
	if !nil? $2
		stops = $2
	elsif null?(stops = prompt 'Tab stops','1')
		return nil
	elsif !numeric? stops
		return -1 => notice "Invalid number '",stops,"'"
	elsif (stops = toInt stops) < 0
		return -1 => notice 'Repeat count (',stops,') must be 0 or greater'
	endif
	if stops > 0
		formerMsgState = -1 => alterGlobalMode 'msg'

		# Set region.
		if $0 != 0
			$0 == defn and $0 = 1
			setMark
			($0 > 0 ? $0 - 1 : $0) => forwLine
		endif

		# Indent or outdent region.
		eval "stops => #{$1 ? 'in' : 'out'}dentRegion"

		formerMsgState => alterGlobalMode 'msg'
	endif
endmacro

# Indent a block of lines, given optional (when called interactively) number of tab stops.
macro indent(1) {usage: 'tab-stops',desc: 'Indent a block of lines (default 1)'}
	$0 => iodent true,length($ArgVector) > 0 ? $1 : nil
endmacro

# Outdent a block of lines, given optional (when called interactively) number of tab stops.
macro outdent(1) {usage: 'tab-stops',desc: 'Outdent a block of lines (default 1)'}
	$0 => iodent false,length($ArgVector) > 0 ? $1 : nil
endmacro

# Apropos commands, functions, and variables.
macro apropos(0) {desc: 'Perform apropos search on commands, functions, and variables'}

	# Prompt for pattern.
	if (pat = prompt 'Apropos') != nil
		formerMsgState = -1 => alterGlobalMode 'msg'
		origBufName = $bufName

		# Create hidden buffer via showBindings command.
		1 => hideBuf(popBufName = 0 => showBindings pat)
		1 => gotoLine 0,popBufName

		# Loop through other "show" commands, adding results to pop buffer.
		for cmd in ['FUNCTIONS:Functions','VARIABLES:Variables']
			title = strShift cmd,':'

			# Save "show" results in a buffer and copy to pop buffer, if not null.
			eval "showBufName = 1 => show#{cmd} #{quote pat}"
			if not 1 => bufBound?
				loop
					markBuf
					if title == 'VARIABLES'

						# Ignore any local variables.
						loop
							if backLine == false
								break 2		# No non-local variables matched.
							elsif $LineLen == 0
								break
							elsif subLine(0,1) == '$'
								forwLine
								break
							endif
						endloop
					endif
					copyRegion
					selectBuf popBufName
					-1 => bufBound? or 1 => newline
					insert '--- ',title," ---\n"
					yank
					cycleKillRing
					break
				endloop
			endif
			selectBuf origBufName
			deleteBuf showBufName
		endloop
		formerMsgState => alterGlobalMode 'msg'

		# Pop results and delete scratch buffer.
		-1 => selectBuf popBufName
		deleteBuf popBufName
	endif
endmacro

# Wrap hook.
macro wrapper
	wrapWord
endmacro
setHook 'wrap',wrapper

# Manage language toolbox.  If one argument is specified, return toolbox keyword associated with given buffer modes (one of
# which may be a language mode), or nil if none; otherwise, activate or deactivate toolbox given name and action request.
macro tbUtil {desc: 'Manage language toolbox'}
	if length($ArgVector) == 1

		# Check if a language mode is active.
		for toolMask in $LangInfo
			toolName = strShift toolMask,':'
			toolMask = eval toolMask
			if $1 & toolMask
				return toolName
			endif
		endloop
	else
		# Activate or deactivate given toolbox per action request.
		activator = $1 & 'Init'				# memacsInit
		if defined?(activator)

			# Loaded toolbox found.  Activate or deactivate it.
			eval sprintf "%s %s",activator,toString($2)
			$activeToolbox = $2 ? $1 : nil

		# Requested toolbox not loaded.  Load tool file if it exists and activation request.
		elsif $2 && (toolFile = xPathname $1 & 'Toolbox') != nil
			require toolFile
			eval activator,' true'
			$activeToolbox = $1
		endif
	endif

	nil
endmacro

# Load and/or activate language toolbox in the current buffer being entered if applicable.  Given argument (which is return
# value from "exitBuf" hook, if defined) is ignored.
macro tbActivate(1) {desc: 'Load and/or activate language toolbox in the current buffer being entered if applicable'}
	newToolbox = tbUtil $bufModes

	# Deactivate current tools if applicable.
	if !nil?($activeToolbox)
		if newToolbox == $activeToolbox
			return
		else
			tbUtil $activeToolbox,false
		endif
	endif

	# Activate new tools if applicable.
	nil?(newToolbox) || tbUtil(newToolbox,true)

	nil
endmacro
setHook 'enterBuf',tbActivate

# Do initialization tasks for buffer language mode.  Called whenever a mode is changed (via $modeHook), given (1) prior global
# modes; (2), prior show modes; (3), prior default modes; and (4), prior buffer modes.
macro initLang(4) {desc: 'Do initialization tasks for buffer language mode'}

	# Nothing to do unless a buffer mode has changed (in the current buffer).
	if $bufModes != $4
		if defined? '$ExtraIndent'

			# Set extra indentation option.
			if $bufModes & $ExtraIndent
				$bufModes & $ModeExtraIndent || 1 => alterBufMode 'xindt'
			elsif $bufModes & (($ModeC | $ModeMEMacs | $ModePerl | $ModeRuby | $ModeShell) & ~$ExtraIndent)
				$bufModes & $ModeExtraIndent && -1 => alterBufMode 'xindt'
			endif
		endif

		# Do tool initialization if a language mode has changed.
		if $4 & $LangMask != $bufModes & $LangMask

			# Deactivate prior mode tools.
			(toolName = tbUtil($4)) && tbUtil toolName,false

			# Activate current mode tools.
			(toolName = tbUtil($bufModes)) && tbUtil toolName,true
		endif
	endif

	true
endmacro
setHook 'mode',initLang

# Set language mode when a buffer filename is changed, given (1), buffer name; and (2), filename.  This macro is set to the
# "read" hook.
macro setLangMode(2) {desc: 'Set language mode when a buffer filename is changed'}
	if nil? $2
		return true
	endif

	formerMsgState = -1 => alterGlobalMode 'msg'
	bufFile0 = bufFile = basename $2
	mode = nil

	# Loop through all filename extensions from right to left until a match is found or none left.  Recognized extensions:
	# .c .h (C), .C (C++), .java (Java), .class (Java class), .js (JavaScript), .m (Objective C), .mm (MightEMacs script),
	# .php (PHP), pl (Perl), .py (Python), .rb (Ruby), and .sh (Shell).
	extTab = ['C:c','c:c','class:c','h:c','java:c','js:c','m:c','mm:memacs','php:c','pl:perl','py:shell','rb:ruby',\
	 'sh:shell']
	loop
		if nil?(i = 1 => index bufFile,'.')

			# No extension: check first line of file.
			1 => setMark '~'
			beginBuf
			str = $lineText
			-1 => gotoMark '~'
			if subString(str,0,3) == '#!/'
				for m in ['ruby','sh:shell','memacs','perl']
					bin = strShift m,':'
					null?(m) and m = bin
					if index(str,'/' & bin) != nil
						mode = m
						break
					endif
				endloop
				break
			endif

			# No special first-line comment.  Set shell mode if root filename begins with "makefile" (ignoring case)
			# or original filename has ".bak" extension or no extension, does not contain a space or period, and
			# begins with a lower case letter.
			str = bufFile0
			subString(str,-4,4) == '.bak' and str = subString(str,0,-4)
			if index(lcString(bufFile),'makefile') == 0 || str == tr(str,'. ','') && ord(str) == ord(lcString str)
				mode = 'shell'
			endif
			break
		else
			# Get extension.
			str = subString(bufFile,i + 1)
			bufFile = subString(bufFile,0,i)

			# Set language mode for known file extensions.
			for mode in extTab
				if str == strShift(mode,':')
					break 2
				endif
			endloop
		endif
	endloop

	# Set buffer mode if applicable.  If target buffer is not current buffer, make it current (temporarily).
	if mode
		if $1 == $bufName
			1 => alterBufMode mode
		else
			oldBuf = $bufName
			selectBuf $1
			1 => alterBufMode mode
			selectBuf oldBuf
		endif
	endif
	formerMsgState => alterGlobalMode 'msg'
	true
endmacro
setHook 'read',setLangMode

# Delete output file before write if file is a symbolic or hard link and user okays it, given (1), buffer name; and (2),
# filename.  This is done so that symbolic links will not be followed and hard links will be broken on update, which will
# effectively create a new file and preserve the original file.  Note that links will also be broken if 'safe' mode is enabled,
# so do nothing in that case.  This macro is set to the "write" hook.
macro checkWrite(2) {desc: 'Delete output file before write if symbolic or hard link and user okays it'}
	if !($globalModes & $ModeSafeSave)

		# Check if output file exists, is a symbolic or hard link, and is not in the $linksToKeep list.
		if stat?($2,'Ll') && !include?($linksToKeep,filename = 0 => pathname($2))

			# Outfile is a link and not previously brought to user's attention.  Ask user if the link should be
			# broken.  If yes, delete the output file; otherwise, remember response by adding absolute pathname to
			# $linksToKeep.
			type = stat?(filename,'L') ? 'symbolic' : 'hard'
			p = sprintf('Break %s link for file "%s" on output? (y,n)',type,$2)
			if prompt(p,'n','c') == 'y'
				shellCmd 'rm ',filename
			else
				push $linksToKeep,filename
			endif
		endif
	endif
endmacro
setHook 'write',checkWrite		# Set write hook.
$linksToKeep = []			# Link pathnames to leave in place, per user's request.

# Perform set intersection on two lists.  Return list of all elements in second list which are (if default n or n > 0) or are
# not (if n < 0) in first list.  Arguments: (1), first list; (2), second list.
macro select(2) {usage: 'list1,list2',desc: 'Perform set intersection on two lists (non-matching if n < 0)'}
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

# Get a buffer list to process: either all visible buffers (default n or n > 0) or only those also in $grepList (n <= 0).
# Return false if error; otherwise, a two-element array containing Boolean "all buffers selected" and the buffer list (array).
macro getBufList(0) {desc: 'Get list of all visible buffers (or only those also in $grepList if n <= 0)'}

	if $0 != defn && $0 <= 0
		if !defined? '$grepList'
			return -1 => notice 'No buffer list found to search'
		endif
		searchList = select bufList(),$grepList
		allBufs = false
	else
		searchList = bufList()
		allBufs = true
	endif
	empty?(searchList) ? -1 => notice('No matching buffers found to search') : [allBufs,searchList]
endmacro

# Process a buffer list, given (1), buffer list; (2), name of macro to invoke on each buffer; and (3...), optional argument(s)
# to pass to macro.  Prompt user to continue or quit before each buffer is processed.  Return a message and false if an error
# occurs; otherwise, true.  The macro may return false and set an exception message if an error occurs, or an informational
# message (which is typically a blurb about what happened, like "3 substitutions"), or nil (no message).
macro doBufList {usage: 'list,macro[,...]',\
 desc: 'Invoke macro interactively (with specified arguments, if any) on given buffer list'}

	# Do sanity checks.
	if length($ArgVector) < 2
		return -1 => notice 'Wrong number of arguments'
	elsif empty?($1)
		return -1 => notice 'Empty buffer list'
	elsif defined?($2) != 'macro'
		return -1 => notice "No such macro '#{$2}'"
	endif

	bList = shift $ArgVector
	macName = shift $ArgVector

	# Build argument list.
	argList = ''
	for arg in $ArgVector
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
	1 => setMark '?'
	Esc = "\e"
	goBack = true
	returnMsg = nxtBuf = nil
	loop
		loop
			thisBuf = nxtBuf
			nxtBuf = shift bList

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
				if (returnMsg = eval join ' ',macName,argList) == false
					return false
				endif

				# Last buffer?
				if nil? nxtBuf
					break
				endif

				# Not last buffer.  Build prompt with return message (if any).
				msg = returnMsg ? "#{returnMsg}.  Next" : 'Next'
				updateScreen
			endif

			# Finish prompt and get confirmation to continue.
			msg = "'#{msg} buffer: '#{nxtBuf}'  Continue? "
                        loop
				reply = prompt msg,nil,'c'
				if reply == ' ' || reply == 'y'
					break 1
				elsif reply == Esc || reply == 'q'
					goBack = false
					break 3
				elsif reply == '.' || reply == 'n'
					break 2
				else
					msg = '(SPC,y) Yes (ESC,q) Stop here (.,n) Stop and go back (?) Help: '
					reply == '?' or beep
				endif
			endloop
		endloop

		# All buffers processed or "stop and go back".
		selectBuf origBuf
		gotoMark '?'
		break
	endloop
	deleteMark '?'

	# Return result.
	if !goBack
		clearMsg
	elsif nil? returnMsg
		0 => notice 'Done!'
	else
		0 => notice returnMsg,'.  Done!'
	endif
	true
endmacro

# Perform queryReplace on current buffer and return custom result message.  Used by queryReplaceAll macro.
macro queryReplaceOne(0)
	if not queryReplace $searchPat,$replacePat
		return false
	endif
	(i = index $ReturnMsg,',') != nil ? subString($ReturnMsg,0,i) : $ReturnMsg
endmacro

# Query-replace multiple buffers (files).  Use buffer list in $grepList if n <= 0; otherwise, all visible buffers.
macro queryReplaceAll(0) {desc: 'Perform queryReplace on all visible buffers (or $grepList if n <= 0)'}

	# Get list of buffers to search: either all visible buffers or only those also in $grepList.
	if (searchList = $0 => getBufList) == false
		return false
	endif
	whichBufs = shift(searchList) ? 'all' : 'selected'
	searchList = shift searchList

	# Prompt for search and replace strings.
	for newValue in ["$searchPat:In #{whichBufs} buffers, query replace",'$replacePat:with']
		srVar = strShift newValue,':'
		newValue = prompt newValue,"#{eval srVar}",'s','^['
		if null?(newValue) && srVar == '$searchPat'		# Null string (^K) entered for $search?
			return						# Yes, bail out.
		endif
		eval join ' ',srVar,'=',quote newValue			# No, assign new value.
	endloop

	# Process buffers.
	doBufList searchList,'queryReplaceOne'
endmacro

# Scan files that match given template in given directory.  Return [buffer name,line number,"new buffer" flag] of first file
# that contains given search string, or false if not found.  Ignore any file that cannot be read (because of permissions, for
# example).  Arguments: (1), directory; (2), filename template; (3), search pattern.
macro locateFile(3) {usage: 'dir,glob,pat',desc: 'Find files matching glob in given directory that contain pat'}
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Get file list into scratch buffer.
	if (listBuf = 0 => readPipe 'ls -1 ',$1 == '.' ? '' : $1 & '/',$2) == false
		formerMsgState => alterGlobalMode 'msg'
		return false
	endif

	# Prepare for search in new screen.
	searchPat = $searchPat
        $searchPat = $3
	saveBuf
	screen1 = $screenNum
	newScreen
	selectBuf listBuf

	# Scan the files (if any).
	bufName = nil
	lineNum = 0
	while (filename = readBuf listBuf) != nil
		force bufName,isNewBuf = findFile filename
		if nil? bufName

			# File read failed... skip it.
			1 => print 'Cannot read file "%s" - skipping...' % filename
			pause
			if $bufFile == filename && 1 => bufSize == 0
				bufName = $bufName
				selectBuf listBuf
				deleteBuf bufName
			else
				selectBuf listBuf
			endif
		else
			isNewBuf or 1 => setMark('?')
			beginBuf
			if huntForw() != false
				beginLine
				lineNum = $bufLineNum
				break
			endif

			if isNewBuf
				selectBuf listBuf
				deleteBuf bufName
			else
				gotoMark '?'
				selectBuf listBuf
			endif
		endif
		bufName = nil
	endloop

	# Clean up.
	force deleteMark '?'
	$searchPat = searchPat
	restoreBuf
	deleteBuf listBuf
	screen2 = $screenNum
	screen1 => nextScreen
	screen2 => deleteScreen
	formerMsgState => alterGlobalMode 'msg'

	# Return buffer name and line number if search string found; otherwise, false.
	nil?(bufName) ? false : [bufName,lineNum,isNewBuf]
endmacro

# Open list of files returned from given shell command in background.  Return false if error; otherwise, list of buffer names.
macro openFiles(1) {usage: 'file-list',desc: 'Open list of files in background'}

	# Get file list into scratch buffer (one filename per line).
	listBuf = 0 => readPipe($1)
	if listBuf == false
		return -1 => notice 'Not found'
	endif

	# Open the files (if any) in the background.
	formerMsgState = -1 => alterGlobalMode 'msg'
	count = 0
	bList = []
	fileList = ''
	until nil? (fileName = readBuf listBuf)
		fileInfo = 0 => findFile fileName
		count++
		strPush fileList,', ',basename fileName
		push bList,fileInfo[0]
	endloop

	# Clean up.
	deleteBuf listBuf
	formerMsgState => alterGlobalMode 'msg'
	notice count,' files found: ',fileList
	bList
endmacro

# Find files that match a template and optionally, a pattern, and open them in the background.  Save resulting buffer list in
# $grepList variable as an array.  The pattern is interpreted as plain text, case-sensitive (default n), plain text,
# case-insensitive (n < 0), Regexp, case-insensitive (n == 0), or Regexp, case-sensitive (n > 0).
macro grepFiles(0) {desc: 'Find files and open in background (with RE pattern if n >= 0, ignoring case if n <= 0)'}

	# Prompt for filename template.  Use last one as default.
	!defined?('$grepTemplate') or empty?($grepTemplate) and $grepTemplate = nil
	$grepTemplate = prompt 'Filename template',$grepTemplate,'f'
	if empty?($grepTemplate)
		clearMsg
		return
	endif
	subString($grepTemplate,-1,1) == '/' and $grepTemplate &= '*'

	# Prompt for search pattern.  If null, get all files that match template.
	prmt = (rePat = $0 >= 0) ? 'Search RE' : 'Search pattern'
	(ignoreCase = $0 == 0 || ($0 < 0 && $0 != defn)) and prmt &= ' (ignoring case)'
	pat = prompt prmt,''

	# Open files and set $grepList to buffer list.
	x = rePat ? 'e' : 'f'
	swt = ignoreCase ? ' -i' : ''
	bList = openFiles(null?(pat) ? "ls -1 #{$grepTemplate}" : "#{x}grep -l#{swt} #{shQuote pat} #{$grepTemplate}")
	if bList == false
		false
	else
		$grepList = bList
		true
	endif
endmacro

# Join line(s) with no spacing.
macro joinLines0(0) {desc: 'Join line(s) with no spacing'}
	$0 => joinLines nil
endmacro

# Offer help options.
macro getHelp(0) {desc: 'Offer help options'}
	HelpFile,Man,UserGuide,MacroRef = $HelpFiles

	# Display help options and get response.
	loop
		choice = prompt 'Display summary, man page, User Guide, Macro Reference, or CANCEL (s,m,g,r,c)','s','c'
		if choice == 'c' || null?(choice)
			break
		elsif choice == 's'

			# Check if help file exists.
			if (helpPath = xPathname HelpFile) == nil
				0 => notice 'Help file "',HelpFile,'" is not available'
			else
				# File exists.  Pop it.
				-2 => popFile helpPath
			endif
			break
		elsif !nil?(index 'mgr',choice)

			# Display appropriate man page.  Try to find MighEMacs man directory in $execPath first in case man
			# search path is not configured properly.
			page = choice == 'r' ? MacroRef : choice == 'g' ? UserGuide : Man
			execPath = $execPath
			until nil?(dir = strShift execPath,':')
				if !null?(dir) && (i = index dir,'/lib/memacs') != nil
					if stat?(manPath = "#{subString dir,0,i}/share/man/man1/#{page}.1","f")
						page = manPath
					endif
					break
				endif
			endloop
			if shellCmd('man ',page) == false
				prompt('End',nil,'c')
				notice nil
			endif
			break
		else
			print "Unknown option '#{choice}' "
			pause
		endif
	endloop

	nil
endmacro
setHook 'help',getHelp

##### Bindings and aliases #####
bindKey 'M-a',apropos
bindKey 'C-c )',indent
bindKey 'C-c f',fenceWord
bindKey 'C-x C-g',grepFiles
bindKey 'C-x C-j',joinLines0
bindKey 'C-c ]',nextGrepBuf
bindKey 'C-c (',outdent
bindKey 'C-h p',popBuf
bindKey 'C-h C-p',popFile
bindKey 'C-h l',popFileList
bindKey 'C-c [',prevGrepBuf
bindKey 'C-h .',pwd
bindKey 'M-C-q',queryReplaceAll
bindKey 'C-x r',renameFile
bindKey 'C-h =',showVar

##### Load site-wide preferences. #####
if (sitePath = xPathname 'site')
	require sitePath

	# Create "word processing" line-join macros if $EndSentence variable defined.
	if !empty?($EndSentence)

		# Join line(s) with extra spacing.
		macro wpJoinLines(0) {desc: 'Join line(s) with extra spacing'}
			$0 => joinLines $EndSentence
		endmacro

		# Wrap line(s) with extra spacing.
		macro wpWrapLine(0) {desc: 'Wrap line(s) with extra spacing'}
			$0 => wrapLine nil,$EndSentence
		endmacro

		bindKey 'C-c C-j',wpJoinLines
		bindKey 'C-c RTN',wpWrapLine
	endif
endif

formerMsgState => alterGlobalMode 'msg'
