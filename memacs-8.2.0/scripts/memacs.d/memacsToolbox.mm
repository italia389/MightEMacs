# memacsToolbox.mm	Ver. 8.2.0
#	MightEMacs macros for MightEMacs script editing.

# Don't load this file if it has already been done so.
if defined? 'memacs' & subString($LangNames,index($LangNames,':') + 1)
	return
endif

require 'toolbox'

# Set the following variable to a regular expression that will match the first line of a macro in a script file; that is, the
# line containing the name of the macro.  The langFindRoutine macro will replace the '%NAME%' string with the name of the macro
# being searched for each time the RE is used.  The langFindRoutine macro is called by the memacsFindMacro macro, which is bound
# to 'M-^]'.
$MemacsMacroRE = '^[ \t]*macro +%NAME%,?\d*$:re'

##### Macros #####

# Wrap "if 0" and "endif" around a block of lines.
macro memacsWrapIf0,0
	$0 => langWrapPrep false,'','0','endif'
endmacro

# Wrap "if 1" and "endif" around a block of lines.
macro memacsWrapIf1,0
	$0 => langWrapPrep false,'','1','endif'
endmacro

# Wrap "if 0" and "else" around a block of lines, duplicate them, and add "endif".
macro memacsWrapIfElse,0
	$0 => langWrapIfElse ''
endmacro

# Find first file that contains requested macro name and render it according to n argument if found.
macro memacsFindMacro,0
	$0 => langFindRoutine '$MemacsMacroRE','$memacsFindDir','*.mm','Macro'
endmacro

# If n < 0, return true if first keyword on line is a "begin block" directive; otherwise, false.  If n == 0, return true if null
# or comment line; otherwise, false.  If n > 0, return true if line begins with an "endXXX" (n == 1) or "else" (n > 1)
# directive; otherwise, false.
macro memacsIsLine?,0
	if $0 == 0
		# Null or comment?
		return null?(line = strip $lineText) || subString(line,0,1) == '#'
	else
		beginText
		if subLine(0,1) =~ '[a-z]'
			formerMsgState = -1 => alterGlobalMode 'msg'
			setMark
			endWord
			lineSym = $RegionText
			formerMsgState => alterGlobalMode 'msg'
			if $0 > 1
				return lineSym == 'else'
			else
				symList = ($0 == 1) ? 'endif,endloop,endmacro' : 'if,loop,macro,until,while'
				until nil?(sym = shift symList,',')
					if lineSym == sym
						return true
					endif
				endloop
			endif
		endif
	endif

	false
endmacro

# Go to matching block end point if current line begins with a block directive.  Set mark '.' to current position, mark 'T' to
# top line of the block, mark 'E' to "else" line (if any), and mark 'B' to the bottom line.  If $0 == 0, be silent about errors.
macro memacsGotoBlockEnd,0
	formerMsgState = -1 => alterGlobalMode 'msg'
	wordChars = $wordChars
	$wordChars = 'A-Za-z0-9_'
	1 => setMark '?'
	found = false

	loop
		loop
			# Examine current line.
			if !(0 => memacsIsLine?)			# Null or comment?
				if -1 => memacsIsLine?			# No, block begin directive?
					isEnd = false			# Yes ...
					break				# onward.
				endif
				if 1 => memacsIsLine?			# No, block end directive?
					isEnd = true			# Yes ...
					break				# onward.
				endif
			endif
			errorMsg = 'No directive on current line'
			break 2					# Not found.
		endloop

		# Block-end line found.  Prepare for forward or backward search.
		beginText
		if isEnd
			direc = -1					# Go backward.
			1 => setMark 'B'
			force deleteMark 'T'
			thatMark = 'T'
		else
			direc = 1					# Go forward.
			1 => setMark 'T'
			force deleteMark 'B'
			thatMark = 'B'
		endif

		level = 0
		foundElse = false
		force deleteMark 'E'

		# Scan lines until matching directive found or hit buffer boundary.
		loop
			if direc => forwLine() == false
				errorMsg = 'Matching directive not found'
				break 2				# Not found.
			endif
			if 0 => memacsIsLine?
				next					# Blank line or comment ... skip it.
			endif
			if (isEnd ? -1 : 1) => memacsIsLine?
				if level == 0
					break				# Found it!
				else
					--level
				endif
			elsif (isEnd ? 1 : -1) => memacsIsLine?
				++level
			elsif 2 => memacsIsLine? && level == 0
				foundElse = true
				beginText
				1 => setMark 'E'
			endif
		endloop

		# Success.
		beginText
		1 => setMark thatMark
		found = true
		break
	endloop

	# Clean up.
	$wordChars = wordChars
	-1 => gotoMark '?'
	formerMsgState => alterGlobalMode 'msg'
	if found
		-1 => setMark
		gotoMark thatMark
		notice nil
	elsif $0 == 0
		true
	else
		beep
		-1 => notice errorMsg
	endif
endmacro

# Activate or deactivate tools, given tool activator name and Boolean argument.
macro memacsInit,2

	# Proceed only if activation request and tools not active or deactivation request and tools active.
	if $2 != (binding('C-c C-g') == 'memacsGotoBlockEnd')
		$wordChars = $2 ? 'A-Za-z0-9_?' : ''
		bindings = join "\r","memacsFindMacro\tM-^]","memacsGotoBlockEnd\tC-c C-g"
		bindings = join "\r",bindings,"memacsWrapIf0\tM-0","memacsWrapIf1\tM-1","memacsWrapIfElse\tM-2"
		eval "#{$1} 'MightEMacs',bindings,$2"
	endif
endmacro

# Done!
notice 'MightEMacs tools loaded'
