# memacs.mm	Ver. 8.1.0.0
#	MightEMacs startup file.

formerMsgState = -1 => alterGlobalMode 'msg'

##### Set constants. #####
$HelpFiles = 'memacs-help:memacs:memacs-guide:memacs-macros'
				# Names of help file, man page, and User Guide man page.
$LangNames = 'Tools:Init'	# Core names of language-tools script file and initialization macros; for example,
				# "rubyTools.mm" and "rubyInit".

##### Define macros. #####

# Print working directory.
!macro pwd,0
	print $WorkDir
!endmacro

# Pop a buffer.
!macro popBuf,1
	($0 == defn || $0 >= 0) and $0 = -1
	!if $ArgCount > 0
		$0 => selectBuf $1
	!else
		$0 => run selectBuf
	!endif
!endmacro

# Pop a file.
!macro popFile,1
	($0 == defn || $0 >= 0) and $0 = -1
	!if $ArgCount > 0
		$0 => readFile $1
	!else
		$0 => run readFile
	!endif
!endmacro

# Apropos commands, functions, and variables.
!macro apropos,0

	# Prompt for pattern.
	!if (pat = prompt 'Apropos') != nil
		formerMsgState = -1 => alterGlobalMode 'msg'
		origBufName = $bufName
		first = true

		# Create hidden scratch buffer.
		1 => hideBuf (popBufName = 0 => scratchBuf)

		# Loop through "show" commands, adding results to pop buffer.
		sections = 'COMMANDS:Bindings,FUNCTIONS:Functions,VARIABLES:Variables'
		!until nil?(cmd = shift sections,',')
			title = shift cmd,':'

			# Save "show" results in a buffer and copy to pop buffer, if not null.
			eval "showBufName = 1 => show#{cmd} #{quote pat}"
			!if not 1 => bufBound?
				!loop
					markBuf
					!if title == 'VARIABLES'

						# Ignore any local variables.
						!loop
							!if backLine == false
								!break 2	# No non-local variables matched.
							!elsif $LineLen == 0
								!break
							!elsif subLine(0,1) == '$'
								forwLine
								!break
							!endif
						!endloop
					!endif
					copyRegion
					selectBuf popBufName
					first ? (first = false) : 1 => newline
					insert '--- ',title," ---\r"
					yank
					cycleKillRing
					!break
				!endloop
			!endif
			selectBuf origBufName
			deleteBuf showBufName
		!endloop
		formerMsgState => alterGlobalMode 'msg'

		# Pop results and delete scratch buffer.
		-1 => selectBuf popBufName
		deleteBuf popBufName
	!endif
!endmacro

# Load and/or activate language tools in the current buffer.
!macro initTools,0

	# Check if a language mode is active.
	tools1 = tools2 = 'c $ModeC|memacs $ModeMEMacs|perl $ModePerl|ruby $ModeRuby|shell $ModeShell'
	!until (toolMask = shift tools1,'|') == nil
		toolPrefix1 = shift toolMask,nil
		toolMask = eval toolMask
		!if $bufModes & toolMask
			LangInitName = $LangNames
			LangToolName = shift LangInitName,':'
			ToolActivator = 'tool' & LangInitName

			# Language mode found.  Deactivate any other tools.
			!until nil?(toolPrefix2 = shift tools2,'|')
				pop toolPrefix2,nil
				!if defined?(activator = toolPrefix2 & LangInitName)

					# Loaded tools found.  Same as this buffer's?
					!if toolPrefix2 == toolPrefix1

						# Yes.  Activate them if needed and bail out.
						eval activator," '",ToolActivator,"',true"
						!return

					# Not the same.  Deactivate them (if active).
					!elsif eval activator," '",ToolActivator,"',false"	# Deactivation successful?
						!break						# Yes, stop here.
					!endif
				!endif
			!endloop

			# Load new language tool file if it exists and is not already loaded.
			!if defined?(activator = toolPrefix1 & LangInitName)
				eval activator," '",ToolActivator,"',true"
			!elsif (toolFile = xPathname toolPrefix1 & LangToolName) != nil
				require toolFile
				eval activator," '",ToolActivator,"',true"
			!endif
			!break
		!endif
	!endloop

	true
!endmacro
$enterBufHook = 'initTools'

# Do initialization tasks for buffer language mode.  Called whenever a mode is changed (via $modeHook), given (1) prior global
# modes; (2), prior show modes; (3), prior default modes; and (4), prior buffer modes.
!macro initLang,4

	# Nothing to do unless a buffer mode has changed (in the current buffer).
	!if $bufModes != $4
		!if defined? '$ExtraIndent'

			# Set extra indentation option.
			!if $bufModes & $ExtraIndent
				$bufModes & $ModeExtraIndent || 1 => alterBufMode 'xindt'
			!elsif $bufModes & (($ModeC | $ModeMEMacs | $ModePerl | $ModeRuby | $ModeShell) & ~$ExtraIndent)
				$bufModes & $ModeExtraIndent && -1 => alterBufMode 'xindt'
			!endif
		!endif

		# Do tool initialization.
		initTools
	!endif

	true
!endmacro
$modeHook = 'initLang'

# Set language mode when a buffer filename is changed, given (1), buffer name; and (2), filename.
!macro setLangMode,2
	formerMsgState = -1 => alterGlobalMode 'msg'
	bufFile0 = bufFile = basename $2
	mode = nil

	# Loop through all filename extensions from right to left until a match is found or none left.  Recognized extensions:
	# .c .h (C), .C (C++), .java (Java), .class (Java class), .js (JavaScript), .m (Objective C), .mm (MightEMacs script),
	# .php (PHP), pl (Perl), .py (Python), .rb (Ruby), and .sh (Shell).
	eTab = 'C:c|c:c|class:c|h:c|java:c|js:c|m:c|mm:memacs|php:c|pl:perl|py:shell|rb:ruby|sh:shell'
	!loop
		!if nil?(i = 1 => index bufFile,'.')

			# No extension: check first line of file.
			9 => setMark
			beginBuf
			str = $lineText
			9 => gotoMark
			!if subString(str,0,3) == '#!/'
				!if index(str,'ruby') != nil
					mode = 'ruby'
				!elsif index(str,'sh') != nil
					mode = 'shell'
				!elsif index(str,'perl') != nil
					mode = 'perl'
				!endif
				!break
			!endif

			# No special first-line comment.  Set shell mode if root filename begins with "makefile" (ignoring case)
			# or original filename has ".bak" extension or no extension, does not contain a space or period, and
			# begins with a lower case letter.
			str = bufFile0
			subString(str,-4,4) == '.bak' and str = subString(str,0,-4)
			!if index(lcString(bufFile),'makefile') == 0 || str == tr(str,'. ','') && ord(str) == ord(lcString str)
				mode = 'shell'
			!endif
			!break
		!else
			# Get extension.
			str = subString(bufFile,i + 1,999)
			bufFile = subString(bufFile,0,i)

			# Set language mode for known file extensions.
			extTab = eTab
			!until (mode = shift extTab,'|') == nil
				!if str == shift(mode,':')
					!break 2
				!endif
			!endloop
		!endif
	!endloop

	# Set buffer mode if applicable.  If target buffer is not current buffer, make it current (temporarily).
	!if mode
		!if $1 == $bufName
			1 => alterBufMode mode
		!else
			oldBuf = $bufName
			selectBuf $1
			1 => alterBufMode mode
			selectBuf oldBuf
		!endif
	!endif
	formerMsgState => alterGlobalMode 'msg'
	true
!endmacro
$readHook = 'setLangMode'

# Perform set intersection on two lists.  Return list of all elements in second list which are (if default n or n > 0) or are
# not (if n < 0) in first list, using second list's delimiter.  Arguments: (1), first list; (2), first delimiter; (3), second
# list; (4), second delimiter.
!macro select,4
	exclude = $0 <= 0 && $0 != defn
	!if null? $1
		!return exclude ? $3 : ''
	!endif
	!if null? $3
		!return ''
	!endif

	result = ''
	!while (item = shift $3,$4) != nil
		include?($1,$2,item) != exclude && push result,$4,item
	!endloop
	result
!endmacro

# Get a buffer list to process: either all visible buffers (default n or n > 0) or only those also in $grepList (n <= 0).
# Return false if error; otherwise, Boolean "all buffers selected", an ESC delimiter, and the tab-delimited buffer list.
!macro getBufList,0
	TabDelim = "\t"

	!if $0 != defn && $0 <= 0
		!if !defined? '$grepList'
			!return -1 => notice 'No buffer list found to search'
		!endif
		searchList = select $BufList,TabDelim,$grepList,TabDelim
		allBufs = false
	!else
		searchList = $BufList
		allBufs = true
	!endif
	null?(searchList) ? -1 => notice('No matching buffers found to search') : join "\e",allBufs,searchList
!endmacro

# Process a buffer list, given (1), buffer list; (2), name of macro to invoke on each buffer; and (3 ...), optional argument(s)
# to pass to macro.  Prompt user to continue or quit before each buffer is processed.  Return a message and false if an error
# occurs; otherwise, true.  The macro may return false and set an exception message if an error occurs, or an informational
# message (which is typically a blurb about what happened, like "3 substitutions"), or nil (no message).
!macro doBufList

	# Do sanity checks.
	!if $ArgCount < 2
		!return -1 => notice 'Wrong number of arguments'
	!elsif null?($1)
		!return -1 => notice 'Empty buffer list'
	!elsif defined?($2) != 'macro'
		!return -1 => notice "No such macro '#{$2}'"
	!endif

	# Build argument list.
	argList = ''
	$argIndex = 3
	!until nil?(arg = nextArg)
		push argList,',',quote arg
	!endloop

	# If current buffer is in list, cycle buffer list so it's in the front.
	Tab = "\t"
	!if include? $1,Tab,$bufName
		bufList = ''
		!while (bufName = shift $1,Tab) != $bufName
			push bufList,Tab,bufName
		!endloop
		$1 = 0 => join Tab,$bufName,$1,bufList
	!endif

	# Save current buffer, position, and loop through buffers.
	origBuf = $bufName
	9 => setMark
	Esc = "\e"
	goBack = true
	returnMsg = nxtBuf = nil
	!loop
		!loop
			thisBuf = nxtBuf
			nxtBuf = shift $1,Tab

			# If first time through loop and first buffer to process is not original one, get user confirmation.
			!if nil? thisBuf
				!if nxtBuf == origBuf
					!next
				!endif
				msg = 'First'
			!else
				# Switch to buffer and call processor.
				selectBuf thisBuf
				beginBuf
				!if (returnMsg = eval join ' ',$2,argList) == false
					!return false
				!endif

				# Last buffer?
				!if nil? nxtBuf
					!break
				!endif

				# Not last buffer.  Build prompt with return message (if any).
				msg = returnMsg ? "#{returnMsg}.  Next" : 'Next'
				updateScreen
			!endif

			# Finish prompt and get confirmation to continue.
			msg = "'#{msg} buffer: '#{nxtBuf}'  Continue? "
                        !loop
				reply = prompt msg,nil,nil,'c'
				!if reply == ' ' || reply == 'y'
					!break 1
				!elsif reply == Esc || reply == 'q'
					goBack = false
					!break 3
				!elsif reply == '.' || reply == 'n'
					!break 2
				!else
					msg = '(<SP>,y) Yes (<ESC>,q) Stop here (.,n) Stop and go back (?) Help: '
					reply == '?' or beep
				!endif
			!endloop
		!endloop

		# All buffers processed or "stop and go back".
		selectBuf origBuf
		9 => gotoMark
		!break
	!endloop

	# Return result.
	!if !goBack
		clearMsg
	!elsif nil? returnMsg
		0 => notice 'Done!'
	!else
		0 => notice returnMsg,'.  Done!'
	!endif
	true
!endmacro

# Perform queryReplace on current buffer and return custom result message.  Used by queryReplaceAll macro.
!macro queryReplaceOne,0
	!if not queryReplace $search,$replace
		!return false
	!endif
	(i = index $ReturnMsg,',') != nil ? subString($ReturnMsg,0,i) : $ReturnMsg
!endmacro

# Query-replace multiple buffers (files).  Use buffer list in $grepList if n <= 0; otherwise, all visible buffers.
!macro queryReplaceAll,0

	# Get list of buffers to search: either all visible buffers or only those also in $grepList.
	!if (searchList = $0 => getBufList) == false
		!return false
	!endif
	whichBufs = shift searchList,"\e"
	whichBufs = whichBufs ? 'all' : 'selected'

	# Prompt for search and replace strings.
	varInfo = "$search:In #{whichBufs} buffers, query replace|$replace:with"
	!until nil? (newValue = shift(varInfo,'|'))
		srVar = shift newValue,':'
		newValue = prompt newValue,"#{eval srVar}",'^['
		!if null?(newValue) && srVar == '$search'		# Null string (^K) entered for $search?
			!return						# Yes, bail out.
		!endif
		eval join ' ',srVar,'=',quote newValue			# No, assign new value.
	!endloop

	# Process buffers.
	doBufList searchList,'queryReplaceOne'
!endmacro

# Scan files that match given template in given directory.  Return buffer name, line number, and "new buffer" flag of first file
# that contains given search string, or false if not found.  Arguments: (1), directory; (2), filename template; (3), search
# string (default) or RE (n arg).
!macro locateFile,3
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Get file list into scratch buffer.
	!if (listBuf = 0 => readPipe 'ls -1 ',$1 == '.' ? '' : $1 & '/',$2) == false
		formerMsgState => alterGlobalMode 'msg'
		!return false
	!endif

	# Prepare for search in new screen.
	globalModes = $globalModes
	search = $search
	$globalModes = ($globalModes | ($0 == defn ? 0 : $ModeRegExp)) & ~$ModeMsgDisp
        $search = $3
	saveBuf
	screen1 = $screenNum
	newScreen
	selectBuf listBuf

	# Scan the files (if any).
	bufName = nil
	lineNum = 0
	!while (fileName = readBuf listBuf) != nil
		isNewBuf = findFile fileName
		bufName = shift isNewBuf,"\t"
		isNewBuf || 8 => setMark

		beginBuf
		!if huntForw() != false
			beginLine
			lineNum = $bufLineNum
			!break
		!endif

		isNewBuf || 8 => gotoMark
		selectBuf listBuf
		isNewBuf && deleteBuf bufName
		bufName = nil
	!endloop

	# Clean up.
	$globalModes = globalModes
	$search = search
	restoreBuf
	deleteBuf listBuf
	screen2 = $screenNum
	screen1 => nextScreen
	screen2 => deleteScreen
	formerMsgState => alterGlobalMode 'msg'

	# Return buffer name and line number if search string found; otherwise, false.
	nil?(bufName) ? false : join "\t",bufName,lineNum,isNewBuf
!endmacro

# Find files that match a template and optionally, a string (default) or RE (n arg).  Save resulting buffer list in $grepList
# variable, tab delimited.
!macro grepFiles,0

	# Prompt for filename template.  Use last one as default.
	!defined?('$grepTemplate') or null?($grepTemplate) and $grepTemplate = nil
	$grepTemplate = prompt 'Filename template',$grepTemplate,nil,'f'
	!if nil?($grepTemplate) || null?($grepTemplate)
		clearMsg
		!return
	!endif
	subString($grepTemplate,-1,1) == '/' and $grepTemplate &= '*'

	# Prompt for search pattern.  If null, get all files that match template.
	pat = prompt($0 == defn ? 'Search pattern' : 'Search RE','')

	# Get file list into scratch buffer (one filename per line).
	x = $0 == defn ? 'f' : 'e'
	listBuf = 0 => readPipe(null?(pat) ? "ls -1 #{$grepTemplate}" : "#{x}grep -l #{shQuote pat} #{$grepTemplate}")
	!if listBuf == false
		!return -1 => notice 'Not found'
	!endif

	# Open the files (if any), and in read-only mode if current buffer is set so.  Set $grepList to buffer list.
	TabDelim = "\t"
	formerMsgState = -1 => alterGlobalMode 'msg'
	eval "alias _getFile = #{$bufModes & $ModeReadOnly ? 'viewFile' : 'findFile'}"
	count = 0
	$grepList = fileList = ''
	!until nil? (fileName = readBuf listBuf)
		created = 0 => _getFile fileName
		bufName = shift created,TabDelim
		count++
		push fileList,TabDelim,basename fileName
		push $grepList,TabDelim,bufName
	!endloop

	# Clean up.
	deleteAlias _getFile
	deleteBuf listBuf
	formerMsgState => alterGlobalMode 'msg'
	notice count,' files found: ',1 => sub(fileList,TabDelim,', ')
!endmacro

# Join line(s) with no spacing.
!macro joinLines0,0
	$0 => joinLines nil
!endmacro

# Offer help options.
!macro getHelp,0
	MacroRef = $HelpFiles
	HelpFile = shift MacroRef,':'
	Man = shift MacroRef,':'
	UserGuide = shift MacroRef,':'

	# Display help options and get response.
	!loop
		choice = prompt 'Display summary, man page, User Guide, Macro Reference, or CANCEL (s,m,g,r,c)','s',nil,'c'
		!if choice == 'c' || null?(choice)
			!break
		!elsif choice == 's'

			# Check if help file exists.
			!if (helpPath = xPathname HelpFile) == nil
				0 => notice 'Help file "',HelpFile,'" is not online'
			!else
				# File exists.  Pop it.
				-2 => popFile helpPath
			!endif
			!break
		!elsif !nil?(index 'mgr',choice)

			# Display appropriate man page.  Try to find MighEMacs man directory in $execPath first in case man
			# search path is not configured properly.
			page = choice == 'r' ? MacroRef : choice == 'g' ? UserGuide : Man
			execPath = $execPath
			!until nil?(dir = shift execPath,':')
				!if !null?(dir) && (i = index dir,'/etc/memacs.d') != nil
					!if fileExists?(manPath = "#{subString dir,0,i}/share/man/man1/#{page}.1")
						page = manPath
					!endif
					!break
				!endif
			!endloop
			!if shellCmd('man ',page) == false
				prompt('End',nil,nil,'c')
				notice nil
			!endif
			!break
		!else
			print "Unknown option '#{choice}' "
			pause
		!endif
	!endloop

	nil
!endmacro
$helpHook = 'getHelp'

##### Bindings and aliases #####
bindKey 'M-A',apropos
bindKey 'M-^Q',queryReplaceAll
bindKey 'M-S',grepFiles
bindKey 'X-^',joinLines0
bindKey 'H-P',popBuf
bindKey 'H-^P',popFile
bindKey 'H-.',pwd

##### Load site-wide preferences. #####
!if (sitePath = xPathname 'site')
	require sitePath

	# Create "word processing" line-join macros if $EndSentence variable set.
	!if defined?('$EndSentence') && !null?($EndSentence) && !nil?($EndSentence)

		# Join line(s) with extra spacing.
		!macro wpJoinLines,0
			$0 => joinLines $EndSentence
		!endmacro

		# Wrap line(s) with extra spacing.
		!macro wpWrapLine,0
			$0 => wrapLine nil,$EndSentence
		!endmacro

		bindKey 'C-^',wpJoinLines
		bindKey 'C-^M',wpWrapLine
	!endif
!endif

formerMsgState => alterGlobalMode 'msg'
