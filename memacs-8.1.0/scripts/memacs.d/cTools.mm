# cTools.mm	Ver. 8.0.0.5
#	MightEMacs macros for C source-file editing.

# Don't load this file if it has already been done so.
!if defined? 'c' & subString($LangNames,index($LangNames,':') + 1,999)
	!return
!endif

require 'toolbox'

# Set the following variable to a regular expression that will match the first line of a C function in a source file; that is,
# the line containing the name of the function.  The langFindRoutine macro will replace the '%NAME%' string with the name of the
# function being searched for each time the RE is used.  The langFindRoutine macro is called by the cFindFunc macro, which is
# bound to 'M-^]'.  The default RE here matches a line where the opening brace for the function is on the same line as the name.
$CFuncRE = "^[a-zA-Z_].+[^a-zA-Z_]%NAME%\\(.+{[ \t]*$"

##### Macros #####

# Wrap "#if 0" and "#endif" around a block of lines.
!macro cWrapIf0,0
	$0 => langWrapPrep false,'#','0','endif'
!endmacro

# Wrap "#if 1" and "#endif" around a block of lines.
!macro cWrapIf1,0
	$0 => langWrapPrep false,'#','1','endif'
!endmacro

# Wrap "#if 0" and "#else" around a block of lines, duplicate them, and add "#endif".
!macro cWrapIfElse,0
	$0 => langWrapIfElse '#'
!endmacro

# Prompt for macro name.
!macro cNamePrompt,0
	defined?('$cWrapName') && !null?($cWrapName) or $cWrapName = nil
	$cWrapName = prompt 'Macro name',$cWrapName
!endmacro

# Wrap "#if !<name>" and "#endif" around a block of lines.
!macro cWrapIfName0,0
	cNamePrompt
	nil?($cWrapName) || null?($cWrapName) || $0 => langWrapPrep false,'#','!' & $cWrapName,'endif'
!endmacro

# Wrap "#if <name>" and "#endif" around a block of lines.
!macro cWrapIfName1,0
	cNamePrompt
	nil?($cWrapName) || null?($cWrapName) || $0 => langWrapPrep false,'#',$cWrapName,'endif'
!endmacro

# Wrap "#if <name>" and "#else" around a block of lines, duplicate them, and add "#endif".
!macro cWrapIfElseName,0
	cNamePrompt
	!if !nil?($cWrapName) || null?($cWrapName)
		formerMsgState = -1 => alterGlobalMode 'msg'
		$0 => langWrapPrep true,'#',$cWrapName,'else'
		setMark
		yank
		insert "#endif\r"
		swapMark
		beginText
		formerMsgState => alterGlobalMode 'msg'
	!endif
!endmacro

# Find first file that contains requested function name and render it according to n argument if found.
!macro cFindFunc,0
	$0 => langFindRoutine '$CFuncRE','$cFindDir','*.c','Function'
!endmacro

# Go to matching #if... or #endif if current line begins with one of the two keywords.  Save current position in mark 0, set
# mark 1 to #if... line, mark 2 to #else (if it exists), and mark 3 to #endif.  If $0 == 0, be silent about errors.  Return true
# if successful; otherwise, false.
!macro cGotoIfEndif,0

	# Current line begin with #if... or #endif?
	x = subLine -$lineOffset,6
	!if subString(x,0,3) == '#if'
		isIf = true
	!elsif x == '#endif'
		isIf = false
	!else
		$0 == 0 || beep
		!return false
	!endif

	# if/endif found.  Prepare for forward or backward search.
	formerMsgState = -1 => alterGlobalMode 'msg'
	0 => setMark
	beginLine
	!if isIf
		alias _nextLine = forwLine
		this = '#if'
		thisLen = 3
		1 => setMark
		3 => clearMark
		that = '#endif'
		thatLen = 6
		thatMark = 3
	!else
		alias _nextLine = backLine
		this = '#endif'
		thisLen = 6
		3 => setMark
		1 => clearMark
		that = '#if'
		thatLen = 3
		thatMark = 1
	!endif

	level = 0
	foundElse = false
	found = true
	2 => clearMark

	# Scan lines until other keyword found.
	!loop
		!if _nextLine() == false
			0 => gotoMark				# Other keyword not found!
			!if $0 != 0
				beep
				0 => notice that,' not found'
			!endif
			found = false
			!break
		!endif

		!if subLine(0,thatLen) == that
			!if level == 0
				thatMark => setMark		# Found other keyword.
				!break
			!else
				--level
			!endif
		!elsif subLine(0,thisLen) == this
			++level
		!elsif subLine(0,5) == '#else' && level == 0
			foundElse = true
			2 => setMark
		!endif
	!endloop

	# Clean up.
	deleteAlias _nextLine
	formerMsgState => alterGlobalMode 'msg'
	found
!endmacro

# Remove conditional preprocessor lines from current buffer and return custom result message, given (1), Boolean "keep 'not'
# lines".  Used by cNukePPLines macro.
!macro cNukePPLines1,1
	formerMsgState = -1 => alterGlobalMode 'msg'
	ifCount = 0
	!until huntForw() == false				# Found #if line.
		isNot = match(1) == '!'				# Grab '!' if present.
		!if 0 => cGotoIfEndif				# Find #else (mark 2) and #endif (mark 3).
			1 => gotoMark				# Move to beginning of #if line.
			++ifCount

			# Delete #if block if applicable.
			!if $1 != isNot			# (Keeping "not" lines) != ('!' present)?
				0 => setMark			# Yes, delete.
				!if defined? 2			# Have #else?
					3 => gotoMark		# Yes, go to #endif ...
					deleteLine		# nuke it ...
					2 => gotoMark		# and go to #else.
				!else
					3 => gotoMark		# No #else.  Go to #endif.
				!endif
				0 => deleteLine			# Delete #if block.
			!else					# Not deleting #if block.
				!if defined? 2			# Have #else?
					2 => gotoMark		# Yes, go to #else ...
					0 => setMark
					3 => gotoMark		# go to #endif ...
					0 => deleteLine		# and delete #else block.
				!else				# No #else.
					3 => gotoMark		# Go to #endif ...
					deleteLine		# and delete it.
				!endif
				1 => gotoMark			# Return to #if ...
				deleteLine			# and delete it.
			!endif
		!else
			# No matching #endif found.  Bail out.
			formerMsgState => alterGlobalMode 'msg'
			!return -1 => notice '#endif not found!'
		!endif
	!endloop

	# No (more) #if lines found in current buffer.
	formerMsgState => alterGlobalMode 'msg'
	saveBuf
	"#{ifCount} '#if' blocks resolved"
!endmacro

# Scan multiple buffers (files) and remove conditional preprocessor lines.  #if lines must be in form "#if MACRO" or
# "#if !MACRO".
!macro cNukePPLines,0

	# Get list of buffers to search: either all visible buffers or only those also in $grepList.
	!if (searchList = $0 => getBufList) == false
		!return false
	!endif
	whichBufs = shift searchList,"\e"
	whichBufs = whichBufs ? 'all' : 'selected'

	# Prompt for Boolean-qualified macro name.
	macName = prompt "In #{whichBufs} buffers, keep lines for macro name (!NAME or NAME)"
	!if nil?(macName) || null?(macName) || macName == '!'
		!return
	!endif

	# Get ready for scan.
	globalModes = $globalModes
	1 => alterGlobalMode 'regexp'
	search = $search
	!if subString(macName,0,1) == '!'
		notMatch = true
		macPlain = subString macName,1,999
	!else
		notMatch = false
		macPlain = macName
	!endif
	$search = "^#if[ \t]+(!?)#{macPlain}[ \t]*$"

	# Process buffers.
	result = doBufList searchList,'cNukePPLines1',notMatch

	# Clean up.
	$search = search
	$globalModes = globalModes
	result
!endmacro

# Activate or deactivate tools, given tool activator name and Boolean argument.
!macro cInit,2

	# Proceed only if activation request and tools not active or deactivation request and tools active.
	!if $2 != (binding('C-^G') == 'cGotoIfEndif')
		$wordChars = ''
		bindings = join "\r","cFindFunc\tM-^]","cGotoIfEndif\tC-^G","cNukePPLines\tC-#"
		bindings = join "\r",bindings,"cWrapIf0\tM-0","cWrapIf1\tM-1","cWrapIfElse\tM-2"
		bindings = join "\r",bindings,"cWrapIfName0\tC-0","cWrapIfName1\tC-1","cWrapIfElseName\tC-2"
		eval "#{$1} 'C',bindings,$2"
	!endif
!endmacro

# Done!
notice 'C tools loaded'
