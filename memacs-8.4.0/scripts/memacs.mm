# memacs.mm		Ver. 8.4.0
#	MightEMacs startup file.

formerMsgState = -1 => alterGlobalMode 'msg'

##### Set constants. #####
$HelpFiles = 'memacs-help:memacs:memacs-guide:memacs-macros'
				# Names of help file, man page, and User Guide man page.
$LangInfo = 'c $ModeC|memacs $ModeMEMacs|perl $ModePerl|ruby $ModeRuby|shell $ModeShell'
				# Language names and masks.
$LangMask = $ModeC | $ModeMEMacs | $ModePerl | $ModeRuby | $ModeShell
				# Language modes' mask.
$LangNames = 'Toolbox:Init'	# Core names of language-toolbox script file and initialization macros; for example,
				# "rubyToolbox.mm" and "rubyInit".

##### Set variables. #####
$activeToolbox = nil		# Name of currently active language toolbox; for example, "ruby".  Used by tbActivate macro.

##### Define macros. #####

# Print working directory.
macro pwd,0
	print $workDir
endmacro

# Pop a buffer.
macro popBuf,1
	($0 == defn || $0 >= 0) and $0 = -2
	$ArgCount > 0 ? $0 => selectBuf($1) : $0 => run selectBuf
endmacro

# Pop a file.
macro popFile,1
	($0 == defn || $0 >= 0) and $0 = -2
	$ArgCount > 0 ? $0 => readFile($1) : $0 => run readFile
endmacro

# Pop a file listing.
macro popFileList,0
	switches = ($0 == defn || $0 > 0) ? 'la' : 'lad'
	template = prompt 'ls',nil,'f'
	nil?(template) || template == '.' and template = ''
	formerMsgState = -1 => alterGlobalMode 'msg'
	-1 => readPipe 'ls -',switches,' ',template
	formerMsgState => alterGlobalMode 'msg'
endmacro

# Rename file associated with current buffer.  If n <=0, rename file on disk only; otherwise, rename both file on disk and
# buffer filename.
macro renameFile,1

	# No current filename?
	if null? $bufFile
		return -1 => notice 'No current filename'
	endif

	# Get new filename.
	if $ArgCount > 0
		if void?(newFile = $1)
			return -1 => notice 'Filename cannot be null'
		endif
	elsif void?(newFile = prompt 'New filename',nil,'^f')
		return
	endif

	# Rename file on disk if it exists.
	(exists = stat?($bufFile,"f")) and shellCmd 'mv ',shQuote($bufFile),' ',shQuote(newFile)

	# Rename buffer file if applicable.
	if $0 == defn || $0 > 0
		setBufFile(newFile)
	elsif exists
		notice 'Disk file renamed'
	else
		-1 => notice "File \"#{$bufFile}\" does not exist, no changes made"
	endif
endmacro

# Switch to next or previous buffer in $grepList n times, given "forward?" argument.
macro pnGrepBuf,1

	# $grepList defined and not empty?
	if !defined?('$grepList') || void?($grepList)
		return -1 => notice 'No buffer list'
	elsif $0 == defn
		$0 = 1
	elsif $0 <= 0
		return
	endif

	# Set direction alias.
	if $1
		# Going forward.
		alias _pnBuf = shift
	else
		# Going backward.
		alias _pnBuf = pop
	endif

	# Current buffer in list?
	if include? $grepList,delim = "\t",$bufName
		list = $grepList
		while _pnBuf(list,delim) != $bufName
			nil
		endloop
	else
		list = ''
	endif

	# Get next buffer n times.
	loop
		null?(list) and list = $grepList
		bufName = _pnBuf list,delim
		if --$0 == 0
			break
		endif
	endloop
	deleteAlias _pnBuf

	# Switch to it.
	selectBuf bufName
endmacro

# Switch to next buffer in $grepList n times.
macro nextGrepBuf,0
	$0 => pnGrepBuf true
endmacro

# Switch to previous buffer in $grepList n times.
macro prevGrepBuf,0
	$0 => pnGrepBuf false
endmacro

# Show a variable.
macro showVar,0
	varName = prompt 'Show var',nil,'V'
	if !void? varName
		if defined?(varName) != 'variable'
			-1 => notice "No such variable '",varName,"'"
		else
			print sprintf '%s = %s',varName,1 => toString(eval varName)
		endif
	endif
endmacro

# Fence word(s); that is, put punctuation or fence characters around one or more words at point.
macro fenceWord,1
	fencePairs = '():[]:{}:<>'
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
		if $ArgCount > 0
			fence = $1
		elsif null?(fence = prompt 'Fence char','"','c')
			status = nil
			break
		endif
		leftFence = rightFence = nil
		until nil?(rightFence = shift fencePairs,':')
			if index(rightFence,fence) != nil
				leftFence = shift rightFence,''
				break
			endif
		endloop
		if nil? leftFence
			# If fence is a punctuation character, just double it up.
			if (o = ord(fence)) > 040 && o <= 0176 && (o == 0137 || !wordChar?(fence))
				leftFence = rightFence = fence
			else
				status = -1 => notice "Invalid fence '",fence,"'"
				break
			endif
		endif

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
# "screen-num\twind-num"; otherwise, search current screen only and return wind-num (integer).  In either case, return nil if
# no window found.
macro bufWind,1

	windList = $WindList
	while (bufName = shift windList,"\t") != nil
		screenNum = shift bufName,'|'
		windNum = shift bufName,'|'
		if bufName == $1
			if $0 > 0
				return join "\t",screenNum,windNum
			elsif toInt(screenNum) == $screenNum
				return toInt windNum
			endif
		endif
	endloop
	nil
endmacro

# Indent or outdent a block of lines specified by n argument, given Boolean value and number of tab stops.  If first argument is
# true, indent; otherwise, outdent.  If second argument is nil, prompt for number of tab stops.
macro iodent,2

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
macro indent,1
	$0 => iodent true,$ArgCount > 0 ? $1 : nil
endmacro

# Outdent a block of lines, given optional (when called interactively) number of tab stops.
macro outdent,1
	$0 => iodent false,$ArgCount > 0 ? $1 : nil
endmacro

# Apropos commands, functions, and variables.
macro apropos,0

	# Prompt for pattern.
	if (pat = prompt 'Apropos') != nil
		formerMsgState = -1 => alterGlobalMode 'msg'
		origBufName = $bufName
		first = true

		# Create hidden scratch buffer.
		1 => hideBuf (popBufName = 0 => scratchBuf)

		# Loop through "show" commands, adding results to pop buffer.
		sections = 'COMMANDS:Bindings,FUNCTIONS:Functions,VARIABLES:Variables'
		until nil?(cmd = shift sections,',')
			title = shift cmd,':'

			# Save "show" results in a buffer and copy to pop buffer, if not null.
			eval "showBufName = 1 => show#{cmd} #{quote pat}"
			if not 1 => bufBound?
				loop
					markBuf
					if title == 'VARIABLES'

						# Ignore any local variables.
						loop
							if backLine == false
								break 2	# No non-local variables matched.
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
					first ? (first = false) : 1 => newline
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

# Manage language toolbox.  If one argument is specified, return toolbox keyword associated with given buffer modes, or nil if
# none; othewise, activate or deactivate toolbox given name and action request.
macro tbUtil
	if $ArgCount == 1

		# Check if a language mode is active.
		toolList = $LangInfo
		until (toolMask = shift toolList,'|') == nil
			toolName = shift toolMask,' '
			toolMask = eval toolMask
			if $1 & toolMask
				return toolName
			endif
		endloop
	else
		# Activate or deactivate given toolbox per action request.
		LangInitName = $LangNames
		Toolbox = shift LangInitName,':'		# Toolbox
		ToolboxActivator = 'tb' & LangInitName		# tbInit
		activator = $1 & LangInitName			# memacsInit
		if defined?(activator)

			# Loaded toolbox found.  Activate or deactivate it.
			eval sprintf "%s '%s','%s'",activator,ToolboxActivator,$2
			$activeToolbox = $2 ? $1 : nil

		# Requested toolbox not loaded.  Load tool file if it exists and activation request.
		elsif $2 && (toolFile = xPathname $1 & Toolbox) != nil
			require toolFile
			eval activator," '",ToolboxActivator,"',true"
			$activeToolbox = $1
		endif
	endif

	nil
endmacro

# Load and/or activate language toolbox in the current buffer being entered if applicable.  Given argument (which is return
# value from "exitBuf" hook, if defined) is ignored.
macro tbActivate,1
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
macro initLang,4

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

# Set language mode when a buffer filename is changed, given (1), buffer name; and (2), filename.
macro setLangMode,2
	if nil? $2
		return true
	endif

	formerMsgState = -1 => alterGlobalMode 'msg'
	bufFile0 = bufFile = basename $2
	mode = nil

	# Loop through all filename extensions from right to left until a match is found or none left.  Recognized extensions:
	# .c .h (C), .C (C++), .java (Java), .class (Java class), .js (JavaScript), .m (Objective C), .mm (MightEMacs script),
	# .php (PHP), pl (Perl), .py (Python), .rb (Ruby), and .sh (Shell).
	eTab = 'C:c|c:c|class:c|h:c|java:c|js:c|m:c|mm:memacs|php:c|pl:perl|py:shell|rb:ruby|sh:shell'
	loop
		if nil?(i = 1 => index bufFile,'.')

			# No extension: check first line of file.
			1 => setMark '~'
			beginBuf
			str = $lineText
			-1 => gotoMark '~'
			if subString(str,0,3) == '#!/'
				binList = 'ruby,sh:shell,memacs,perl'
				while (m = shift binList,',') != nil
					bin = shift m,':'
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
			extTab = eTab
			until (mode = shift extTab,'|') == nil
				if str == shift(mode,':')
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

# Perform set intersection on two lists.  Return list of all elements in second list which are (if default n or n > 0) or are
# not (if n < 0) in first list, using second list's delimiter.  Arguments: (1), first list; (2), first delimiter; (3), second
# list; (4), second delimiter.
macro select,4
	exclude = $0 <= 0 && $0 != defn
	if null? $1
		return exclude ? $3 : ''
	endif
	if null? $3
		return ''
	endif

	result = ''
	while (item = shift $3,$4) != nil
		include?($1,$2,item) != exclude && push result,$4,item
	endloop
	result
endmacro

# Get a buffer list to process: either all visible buffers (default n or n > 0) or only those also in $grepList (n <= 0).
# Return false if error; otherwise, Boolean "all buffers selected", an ESC delimiter, and the tab-delimited buffer list.
macro getBufList,0
	TabDelim = "\t"

	if $0 != defn && $0 <= 0
		if !defined? '$grepList'
			return -1 => notice 'No buffer list found to search'
		endif
		searchList = select $BufList,TabDelim,$grepList,TabDelim
		allBufs = false
	else
		searchList = $BufList
		allBufs = true
	endif
	null?(searchList) ? -1 => notice('No matching buffers found to search') : join "\e",allBufs,searchList
endmacro

# Process a buffer list, given (1), buffer list; (2), name of macro to invoke on each buffer; and (3...), optional argument(s)
# to pass to macro.  Prompt user to continue or quit before each buffer is processed.  Return a message and false if an error
# occurs; otherwise, true.  The macro may return false and set an exception message if an error occurs, or an informational
# message (which is typically a blurb about what happened, like "3 substitutions"), or nil (no message).
macro doBufList

	# Do sanity checks.
	if $ArgCount < 2
		return -1 => notice 'Wrong number of arguments'
	elsif null?($1)
		return -1 => notice 'Empty buffer list'
	elsif defined?($2) != 'macro'
		return -1 => notice "No such macro '#{$2}'"
	endif

	# Build argument list.
	argList = ''
	$argIndex = 3
	until nil?(arg = nextArg)
		push argList,',',quote arg
	endloop

	# If current buffer is in list, cycle buffer list so it's in the front.
	Tab = "\t"
	if include? $1,Tab,$bufName
		bufList = ''
		while (bufName = shift $1,Tab) != $bufName
			push bufList,Tab,bufName
		endloop
		$1 = 0 => join Tab,$bufName,$1,bufList
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
			nxtBuf = shift $1,Tab

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
				if (returnMsg = eval join ' ',$2,argList) == false
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
macro queryReplaceOne,0
	if not queryReplace $searchPat,$replacePat
		return false
	endif
	(i = index $ReturnMsg,',') != nil ? subString($ReturnMsg,0,i) : $ReturnMsg
endmacro

# Query-replace multiple buffers (files).  Use buffer list in $grepList if n <= 0; otherwise, all visible buffers.
macro queryReplaceAll,0

	# Get list of buffers to search: either all visible buffers or only those also in $grepList.
	if (searchList = $0 => getBufList) == false
		return false
	endif
	whichBufs = shift searchList,"\e"
	whichBufs = whichBufs ? 'all' : 'selected'

	# Prompt for search and replace strings.
	varInfo = "$searchPat:In #{whichBufs} buffers, query replace|$replacePat:with"
	until nil? (newValue = shift(varInfo,'|'))
		srVar = shift newValue,':'
		newValue = prompt newValue,"#{eval srVar}",'s','^['
		if null?(newValue) && srVar == '$searchPat'		# Null string (^K) entered for $search?
			return						# Yes, bail out.
		endif
		eval join ' ',srVar,'=',quote newValue			# No, assign new value.
	endloop

	# Process buffers.
	doBufList searchList,'queryReplaceOne'
endmacro

# Scan files that match given template in given directory.  Return buffer name, line number, and "new buffer" flag of first file
# that contains given search string, or false if not found.  Arguments: (1), directory; (2), filename template; (3), search
# pattern.
macro locateFile,3
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Get file list into scratch buffer.
	if (listBuf = 0 => readPipe 'ls -1 ',$1 == '.' ? '' : $1 & '/',$2) == false
		formerMsgState => alterGlobalMode 'msg'
		return false
	endif

	# Prepare for search in new screen.
	search = $searchPat
        $searchPat = $3
	saveBuf
	screen1 = $screenNum
	newScreen
	selectBuf listBuf

	# Scan the files (if any).
	bufName = nil
	lineNum = 0
	while (filename = readBuf listBuf) != nil
		isNewBuf = findFile filename
		bufName = shift isNewBuf,"\t"
		isNewBuf || 1 => setMark('?')
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
		bufName = nil
	endloop

	# Clean up.
	force deleteMark '?'
	$searchPat = search
	restoreBuf
	deleteBuf listBuf
	screen2 = $screenNum
	screen1 => nextScreen
	screen2 => deleteScreen
	formerMsgState => alterGlobalMode 'msg'

	# Return buffer name and line number if search string found; otherwise, false.
	nil?(bufName) ? false : join "\t",bufName,lineNum,isNewBuf
endmacro

# Find files that match a template and optionally, a pattern, and open them in the background.  Save resulting buffer list in
# $grepList variable, tab delimited.  The pattern is interpreted as plain text, case-sensitive (default n), plain text,
# case-insensitive (n < 0), Regexp, case-insensitive (n == 0), or Regexp, case-sensitive (n > 0).
macro grepFiles,0

	# Prompt for filename template.  Use last one as default.
	!defined?('$grepTemplate') or null?($grepTemplate) and $grepTemplate = nil
	$grepTemplate = prompt 'Filename template',$grepTemplate,'f'
	if void?($grepTemplate)
		clearMsg
		return
	endif
	subString($grepTemplate,-1,1) == '/' and $grepTemplate &= '*'

	# Prompt for search pattern.  If null, get all files that match template.
	prmt = (rePat = $0 >= 0) ? 'Search RE' : 'Search pattern'
	(ignoreCase = $0 == 0 || ($0 < 0 && $0 != defn)) and prmt &= ' (ignoring case)'
	pat = prompt prmt,''

	# Get file list into scratch buffer (one filename per line).
	x = rePat ? 'e' : 'f'
	swt = ignoreCase ? ' -i' : ''
	listBuf = 0 => readPipe(null?(pat) ? "ls -1 #{$grepTemplate}" : "#{x}grep -l#{swt} #{shQuote pat} #{$grepTemplate}")
	if listBuf == false
		return -1 => notice 'Not found'
	endif

	# Open the files (if any) in the background.  Set $grepList to buffer list.
	TabDelim = "\t"
	formerMsgState = -1 => alterGlobalMode 'msg'
	count = 0
	$grepList = fileList = ''
	until nil? (fileName = readBuf listBuf)
		created = 0 => findFile fileName
		bufName = shift created,TabDelim
		count++
		push fileList,', ',basename fileName
		push $grepList,TabDelim,bufName
	endloop

	# Clean up.
	deleteBuf listBuf
	formerMsgState => alterGlobalMode 'msg'
	notice count,' files found: ',fileList
endmacro

# Join line(s) with no spacing.
macro joinLines0,0
	$0 => joinLines nil
endmacro

# Offer help options.
macro getHelp,0
	MacroRef = $HelpFiles
	HelpFile = shift MacroRef,':'
	Man = shift MacroRef,':'
	UserGuide = shift MacroRef,':'

	# Display help options and get response.
	loop
		choice = prompt 'Display summary, man page, User Guide, Macro Reference, or CANCEL (s,m,g,r,c)','s','c'
		if choice == 'c' || null?(choice)
			break
		elsif choice == 's'

			# Check if help file exists.
			if (helpPath = xPathname HelpFile) == nil
				0 => notice 'Help file "',HelpFile,'" is not online'
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
			until nil?(dir = shift execPath,':')
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

	# Create "word processing" line-join macros if $EndSentence variable not void.
	if !void?($EndSentence)

		# Join line(s) with extra spacing.
		macro wpJoinLines,0
			$0 => joinLines $EndSentence
		endmacro

		# Wrap line(s) with extra spacing.
		macro wpWrapLine,0
			$0 => wrapLine nil,$EndSentence
		endmacro

		bindKey 'C-c C-j',wpJoinLines
		bindKey 'C-c RTN',wpWrapLine
	endif
endif

formerMsgState => alterGlobalMode 'msg'
