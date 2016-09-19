# rubyToolbox.mm	Ver. 8.3.1
#	MightEMacs macros for Ruby source-file editing.

# Don't load this file if it has already been done so.
if defined? 'ruby' & subString($LangNames,index($LangNames,':') + 1)
	return
endif

require 'toolbox'

# Set the following variable to a regular expression that will match the first line of a method in a script file; that is, the
# line containing the name of the method.  The langFindRoutine macro will replace the '%NAME%' string with the name of the
# method being searched for each time the RE is used.  The langFindRoutine macro is called by the cFindFunc macro, which is
# bound to 'C-c C-f'.
$RubyMethodRE = '^[ \t]*def *?[\l_\d.]*?[^\l_\d]%NAME%[^\l_\d]:rem'

##### Macros #####

# Wrap "=begin" and "=end" around a block of lines.
macro rubyWrapBeginEnd,0
	$0 => markBlock					# Mark block boundaries and position dot.
	beginLine
	openLine					# Insert "=begin".
	insert '=begin'
	swapMark
	endLine
	insert "\r=end"					# Insert "=end".
	forwChar
endmacro

# Find first file that contains requested method name and render it according to n argument if found.
macro rubyFindMethod,0
	$0 => langFindRoutine '$RubyMethodRE','$rubyFindDir','*.rb','Method'
endmacro

# If n < 0, return true if first or last symbol on line is a "begin block" symbol; otherwise, false.  If n == 0, return true if
# null or comment line; otherwise, false.  If n > 0, return true if "end" (n == 1) or "else" (n > 1) line; otherwise, false.
macro rubyIsLine?,0
	if $0 == 0
		# Null or comment?
		return null?(line = strip $lineText) || subString(line,0,1) == '#'
	endif

	formerMsgState = -1 => alterGlobalMode 'msg'
	if $0 < 0
		beginSyms = 'begin,case,class,def,if,module,unless,until,while'
		beginText
		if index($wordChars,subLine 0,1) != nil
			setMark
			endWord
			if include?(beginSyms,',',$RegionText)
				formerMsgState => alterGlobalMode 'msg'
				return true
			endif
		endif
	endif

	# Find end of line ... before any comment.  Assume "#{" is part of a double-quoted string.
	beginLine
	if (i = 1 => index $lineText,'#') != nil && ++i < $LineLen && subLine(i,1) != '{'
		$lineOffset = i = i - 2
		beginWhite
		$lineOffset == i && forwChar
	else
		endLine
	endif
	c = subLine(-1,1)

	if $0 < 0
		endSyms = '|,do'
		if include? endSyms,',',c
			result = true
		elsif nil?(index $wordChars,c)
			result = false
		else
			setMark
			backWord
			result = include?(endSyms,',',$RegionText)
		endif
	else
		result = strip(subLine -$lineOffset,$lineOffset) =~ ($0 == 1 ? '\Aend\.?' : '\Aelse\Z')
	endif
	formerMsgState => alterGlobalMode 'msg'
	result
endmacro

# Go to matching block end point if current line contains a solo "end" or begins or ends with one of its matching keywords.  Set
# mark '.' to current position, mark 'T' to top line of the block, mark 'E' to "else" line (if any), and mark 'B' to the bottom
# line.  If $0 == 0, be silent about errors.
macro rubyGotoBlockEnd,0
	formerMsgState = -1 => alterGlobalMode 'msg'
	wordChars = $wordChars
	$wordChars = 'A-Za-z'
	1 => setMark '?'
	found = false

	loop
		loop
			# Examine current line.
			if !(0 => rubyIsLine?)				# Null or comment?
				if 1 => rubyIsLine?			# No, solo "end"?
					isEnd = true			# Yes ...
					break				# onward.
				endif
				if -1 => rubyIsLine?			# No, first or last symbol a "block begin"?
					isEnd = false			# Yes ...
					break				# onward.
				endif
			endif
			errorMsg = 'No block keyword on current line'
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

		# Scan lines until other symbol found or hit buffer boundary.
		loop
			if direc => forwLine() == false
				errorMsg = 'Matching symbol not found'
				break 2				# Not found.
			endif
			if 0 => rubyIsLine?
				next					# Blank line or comment ... skip it.
			endif
			if (isEnd ? -1 : 1) => rubyIsLine?
				if level == 0
					break				# Found it!
				else
					--level
				endif
			elsif (isEnd ? 1 : -1) => rubyIsLine?
				++level
			elsif 2 => rubyIsLine? && level == 0
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
	elsif $0 != 0
		beep
		-1 => notice errorMsg
	endif
endmacro

# Activate or deactivate tools, given tool activator name and Boolean argument.
macro rubyInit,2

	# Proceed only if activation request and tools not active or deactivation request and tools active.
	if $2 != (binding('C-c C-g') == 'rubyGotoBlockEnd')
		$wordChars = $2 ? 'A-Za-z0-9_?!' : ''
		bindings = join "\r","rubyFindMethod\tC-c C-f","rubyGotoBlockEnd\tC-c C-g","rubyWrapBeginEnd\tM-0"
		eval "#{$1} 'Ruby',bindings,$2"
	endif
endmacro

# Done!
notice 'Ruby tools loaded'
