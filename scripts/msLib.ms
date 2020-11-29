# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# msLib.ms		Ver. 9.5.1
#	MightEMacs library for MightEMacs script editing.

# Array of parameters used for finding user commands and functions in script files.
searchParams = [\
	/# Function search parameters #/\
['Command or function',						/# User prompt #/\
 '^\h*(?:command|function) +%NAME%\>:re',			/# User routine RE #/\
 '*.ms'],							/# Glob pattern(s) #/\
	/# Class search parameters #/\
nil,\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Commands and Functions #####

# Wrap "if 0" and "endif" around a block of lines.
command msWrapIf0(0) {description: 'Wrap ~#uif 0~U and ~uendif~U around [-]n lines (default 1).'}
	$0 => langWrapPrep false, '', '0', 'endif'
endroutine

# Wrap "if 1" and "endif" around a block of lines.
command msWrapIf1(0) {description: 'Wrap ~#uif 1~U and ~uendif~U around [-]n lines (default 1).'}
	$0 => langWrapPrep false, '', '1', 'endif'
endroutine

# Wrap "if 0" and "else" around a block of lines, duplicate them, and add "endif".
command msWrapIfElse(0) {description: 'Wrap ~#uif 0~U and ~uelse~U around [-]n lines (default 1), duplicate them, and add\
 ~uendif~U.'}
	$0 => langWrapIfElse ''
endroutine

# Find user command or function definition, given name, in a file or current buffer.
command msFindRoutine(0) {description: "Prompt for a directory to search and the name of a user command or function and find\
 its definition.  If directory is blank, current buffer is searched (only); otherwise, all files in directory matching\
 \"#{searchParams[0][2]}\" template are searched, and first file found containing definition is rendered according to n\
 argument (with ~bselectBuf~B options)."}
	$0 => langFindObj true
endroutine

# If n < 0, return true if first keyword on line is a "begin block" directive; otherwise, false.  If n == 0, return true if null
# or comment line; otherwise, false.  If n > 0, return true if line begins with an "endXXX" (n == 1) or "else" (n > 1)
# directive; otherwise, false.
function msIsLine?(0)
	if $0 == 0
		# Null or comment?
		return null?(line = strip($lineText)) || substr(line, 0, 1) == '#'
	else
		beginText
		if subline(0, 1) =~ '[a-z]'
			setMark
			endWord
			lineSym = $RegionText
			if $0 > 1
				return lineSym == 'else'
			else
				symList = ($0 == 1) ? ['endif', 'endloop', 'endroutine'] :\
				 ['command', 'function', 'if', 'for', 'loop', 'until', 'while']
				for sym in symList
					if lineSym == sym
						return true
					endif
				endloop
			endif
		endif
	endif

	false
endroutine

# Go to matching block end point if current line begins with a block keyword.  Set mark '.' to current position, mark 'T' to
# top line of the block, mark 'E' to "else" line (if any), and mark 'B' to the bottom line.  If $0 == 0, be silent about errors.
command msGotoBlockEnd(0) {description: "Go to block keyword paired with one in current line and set marks '.' (previous\
 position), 'T' (top line), 'E' (\"else\" line, if any), and 'B' (bottom line).  If n == 0, be silent about errors.  Returns:\
 true if successful; otherwise, false."}
	1 => setMark ??
	found = false

	loop
		loop
			# Examine current line.
			if !(0 => msIsLine?)				# Null or comment?
				if -1 => msIsLine?			# No, block begin directive?
					isEnd = false			# Yes ...
					break				# onward.
				endif
				if 1 => msIsLine?			# No, block end directive?
					isEnd = true			# Yes ...
					break				# onward.
				endif
			endif
			errorMsg = 'No directive on current line'
			break 2						# Not found.
		endloop

		# Block-end line found.  Prepare for forward or backward search.
		beginText
		if isEnd
			direc = -1					# Go backward.
			1 => setMark ?B
			delMark ?T
			thatMark = ?T
		else
			direc = 1					# Go forward.
			1 => setMark ?T
			delMark ?B
			thatMark = ?B
		endif

		level = 0
		foundElse = false
		delMark ?E

		# Scan lines until matching directive found or hit buffer boundary.
		loop
			if direc => forwLine() == false
				errorMsg = 'Matching directive not found'
				break 2					# Not found.
			endif
			if 0 => msIsLine?
				next					# Blank line or comment ... skip it.
			endif
			if (isEnd ? -1 : 1) => msIsLine?
				if level == 0
					break				# Found it!
				else
					--level
				endif
			elsif (isEnd ? 1 : -1) => msIsLine?
				++level
			elsif 2 => msIsLine? && level == 0
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
	-1 => gotoMark ??
	if found
		1 => setMark ?`
		gotoMark thatMark
	elsif $0 == 0
		true
	else
		beep
		1 => message 'Fail, High', errorMsg
	endif
endroutine

# Done.  Return array of key bindings for "public" commands and array of "user routine search" parameters (see comments at
# beginning of lang.ms for details).
[["msFindRoutine\tC-c C-f", "msGotoBlockEnd\tC-c C-g",\
 "msWrapIf0\tM-0", "msWrapIf1\tM-1", "msWrapIfElse\tM-2"],\
 searchParams]
