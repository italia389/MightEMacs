# memacsTools.mm	Ver. 8.0.0.2
#	MightEMacs macros for MightEMacs script editing.

# Don't load this file if it has already been done so.
!if defined? 'memacs' & subString($LangNames,index($LangNames,':') + 1,999)
	!return
!endif

require 'toolbox'

# Set the following variable to a regular expression that will match the first line of a macro in a script file; that is, the
# line containing the name of the macro.  The langFindRoutine macro will replace the '%NAME%' string with the name of the macro
# being searched for each time the RE is used.  The langFindRoutine macro is called by the memacsFindMacro macro, which is bound
# to 'M-^]'.
$MemacsMacroRE = "^[ \t]*!macro +%NAME%,?[0-9]*?$"

##### Macros #####

# Wrap "!if 0" and "!endif" around a block of lines.
!macro memacsWrapIf0,0
	$0 => langWrapPrep false,'!','0','endif'
!endmacro

# Wrap "!if 1" and "!endif" around a block of lines.
!macro memacsWrapIf1,0
	$0 => langWrapPrep false,'!','1','endif'
!endmacro

# Wrap "!if 0" and "!else" around a block of lines, duplicate them, and add "!endif".
!macro memacsWrapIfElse,0
	$0 => langWrapIfElse '!'
!endmacro

# Find first file that contains requested macro name and render it according to n argument if found.
!macro memacsFindMacro,0
	$0 => langFindRoutine '$MemacsMacroRE','$memacsFindDir','*.mm','Macro'
!endmacro

# If n < 0, return true if first keyword on line is a "begin block" directive; otherwise, false.  If n == 0, return true if null
# or comment line; otherwise, false.  If n > 0, return true if line begins with an "!endXXX" (n == 1) or "!else" (n > 1)
# directive; otherwise, false.
!macro memacsIsLine?,0
	!if $0 == 0
		# Null or comment?
		!return null?(line = strip $lineText) || subString(line,0,1) == '#'
	!else
		beginText
		!if subLine(0,1) == '!' && index($wordChars,subLine 1,1) != nil
			formerMsgState = -1 => alterGlobalMode 'msg'
			forwChar
			setMark
			endWord
			lineSym = $RegionText
			formerMsgState => alterGlobalMode 'msg'
			!if $0 > 0
				!return $0 == 1 ? subString(lineSym,0,3) == 'end' : lineSym == 'else'
			!else
				beginSyms = 'if,loop,macro,until,while'
				!until nil?(beginSym = shift beginSyms,',')
					!if lineSym == beginSym
						!return true
					!endif
				!endloop
			!endif
		!endif
	!endif

	false
!endmacro

# Go to matching block end point if current line begins with a block directive.  Set mark 0 to current position, mark 1 to top
# line of the block, mark 2 to "!else" line (if any), and mark 3 to the bottom line.  If $0 == 0, be silent about errors.
!macro memacsGotoBlockEnd,0
	formerMsgState = -1 => alterGlobalMode 'msg'
	wordChars = $wordChars
	$wordChars = 'A-Za-z'
	9 => setMark
	found = false

	!loop
		!loop
			# Examine current line.
			!if !(0 => memacsIsLine?)			# Null or comment?
				!if -1 => memacsIsLine?			# No, block begin directive?
					isEnd = false			# Yes ...
					!break				# onward.
				!endif
				!if 1 => memacsIsLine?			# No, block end directive?
					isEnd = true			# Yes ...
					!break				# onward.
				!endif
			!endif
			errorMsg = 'No directive on current line'
			!break 2					# Not found.
		!endloop

		# Block-end line found.  Prepare for forward or backward search.
		beginText
		!if isEnd
			direc = -1					# Go backward.
			3 => setMark
			1 => clearMark
			thatMark = 1
		!else
			direc = 1					# Go forward.
			1 => setMark
			3 => clearMark
			thatMark = 3
		!endif

		level = 0
		foundElse = false
		2 => clearMark

		# Scan lines until matching directive found or hit buffer boundary.
		!loop
			!if direc => forwLine() == false
				errorMsg = 'Matching directive not found'
				!break 2				# Not found.
			!endif
			!if 0 => memacsIsLine?
				!next					# Blank line or comment ... skip it.
			!endif
			!if (isEnd ? -1 : 1) => memacsIsLine?
				!if level == 0
					!break				# Found it!
				!else
					--level
				!endif
			!elsif (isEnd ? 1 : -1) => memacsIsLine?
				++level
			!elsif 2 => memacsIsLine? && level == 0
				foundElse = true
				beginText
				2 => setMark
			!endif
		!endloop

		# Success.
		beginText
		thatMark => setMark
		found = true
		!break
	!endloop

	# Clean up.
	$wordChars = wordChars
	9 => gotoMark
	formerMsgState => alterGlobalMode 'msg'
	!if found
		0 => setMark
		thatMark => gotoMark
		notice nil
	!elsif $0 == 0
		true
	!else
		beep
		-1 => notice errorMsg
	!endif
!endmacro

# Activate or deactivate tools, given tool activator name and Boolean argument.
!macro memacsInit,2

	# Proceed only if activation request and tools not active or deactivation request and tools active.
	!if $2 != (binding('C-^G') == 'memacsGotoBlockEnd')
		$wordChars = $2 ? 'A-Za-z0-9_?' : ''
		bindings = join "\r","memacsFindMacro\tM-^]","memacsGotoBlockEnd\tC-^G"
		bindings = join "\r",bindings,"memacsWrapIf0\tM-0","memacsWrapIf1\tM-1","memacsWrapIfElse\tM-2"
		eval "#{$1} 'MightEMacs',bindings,$2"
	!endif
!endmacro

# Done!
notice 'MightEMacs tools loaded'
