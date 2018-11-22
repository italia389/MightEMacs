# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# rubyLib.mm		Ver. 9.1.0
#	MightEMacs library for Ruby script editing.

## Define regular expression that will match the first line of a method definition.  The rubyFindMethod macro will replace the
## '%NAME%' string with the name of the method being searched for each time the RE is used.
#$RubyMethodRE = '^[ \t]*def +[\l_\d.]*?\b%NAME%\b:re'

# Array of parameters used for finding classes and modules in script files.
searchParams = [\
	/# Function search parameters #/\
['Method',							/# User prompt #/\
 '^[ \t]*def +[\l_\d.]*?\b%NAME%\b:re',				/# Name RE #/\
 '*.rb'],							/# Glob pattern(s) #/\
	/# Class search parameters #/\
['Class or module',						/# User prompt #/\
 '^[ \t]*[cm][lo][ad][su][sl]e? +%NAME%\b:re',			/# Name RE #/\
 '*.rb'],							/# Glob pattern(s) #/\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Macros #####

# Wrap "=begin" and "=end" around a block of lines.
macro rubyWrapBeginEnd(0) {desc: 'Wrap ~u=begin~U and ~u=end~U around [-]n lines (default 1).'}
	selectLine $0					# Mark block boundaries and position point.
	openLine					# Insert "=begin".
	insert '=begin'
	swapMark
	endLine
	insert "\n=end"					# Insert "=end".
	forwChar
endmacro

# Find method definition, given name, in a file or current buffer.
macro rubyFindMethod(0) {desc: "Prompt for a directory to search and the name of a method and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~0\
 options)."}
	$0 => langFindObj true
endmacro

# Find class or module definition, given name, in a file or current buffer.
macro rubyFindCM(0) {desc: "Prompt for a directory to search and the name of a class or module and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[1][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~0\
 options)."}
	$0 => langFindObj false
endmacro

# If n < 0, return true if first or last symbol on line is a "begin block" symbol; otherwise, false.  If n == 0, return true if
# null or comment line; otherwise, false.  If n > 0, return true if "end" (n == 1) or "else" (n > 1) line; otherwise, false.
macro rubyIsLine?(0)
	if $0 == 0
		# Null or comment?
		return null?(line = strip $lineText) || substr(line,0,1) == '#'
	endif

	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	if $0 < 0
		beginSyms = ['begin','case','class','def','if','module','unless','until','while']
		beginText
		if index($wordChars,subline 0,1) != nil
			setMark
			endWord
			if include?(beginSyms,$RegionText)
				oldMsgState => chgGlobalMode 'RtnMsg'
				return true
			endif
		endif
	endif

	# Find end of line ... before any comment.  Assume "#{" is part of a double-quoted string.
	beginLine
	if (i = 1 => index $lineText,'#') != nil && ++i < $LineLen && subline(i,1) != '{'
		$lineOffset = i = i - 2
		beginWhite
		$lineOffset == i && forwChar
	else
		endLine
	endif
	c = subline(-1,1)

	if $0 < 0
		endSyms = ['|','do']
		if include? endSyms,c
			result = true
		elsif nil?(index $wordChars,c)
			result = false
		else
			setMark
			backWord
			result = include?(endSyms,$RegionText)
		endif
	else
		result = strip(subline -$lineOffset,$lineOffset) =~ ($0 == 1 ? '\Aend\.?' : '\Aelse\Z')
	endif
	oldMsgState => chgGlobalMode 'RtnMsg'
	result
endmacro

# Go to matching block end point if current line contains a solo "end" or begins or ends with one of its matching keywords.  Set
# mark '.' to current position, mark 'T' to top line of the block, mark 'E' to "else" line (if any), and mark 'B' to the bottom
# line.  If $0 == 0, be silent about errors.
macro rubyGotoBlockEnd(0) {desc: "Go to block keyword paired with one in current line and set marks '.' (previous position),\
 'T' (top line), 'E' (\"else\" line, if any), and 'B' (bottom line).  If n == 0, be silent about errors.  Returns: true if\
 successful; otherwise, false."}
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	wordChars = $wordChars
	$wordChars = 'A-Za-z'
	1 => setMark ??
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
			1 => setMark ?B
			deleteMark ?T
			thatMark = ?T
		else
			direc = 1					# Go forward.
			1 => setMark ?T
			deleteMark ?B
			thatMark = ?B
		endif

		level = 0
		foundElse = false
		deleteMark ?E

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
				1 => setMark ?E
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
	-1 => gotoMark ??
	oldMsgState => chgGlobalMode 'RtnMsg'
	if found
		-1 => setMark
		gotoMark thatMark
		1 => message nil,nil
	elsif $0 == 0
		true
	elsif $0 != 0
		beep
		-1 => message nil,errorMsg
	endif
endmacro

# Done.  Return array of key bindings for "public" macros and array of "method/class/module search" parameters (see comments at
# beginning of lang.mm for details).
[["rubyFindMethod\tC-c C-f","rubyFindCM\tC-c C-c","rubyGotoBlockEnd\tC-c C-g","rubyWrapBeginEnd\tESC 0"],\
 searchParams]
