# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# memacsLib.mm		Ver. 9.1.0
#	MightEMacs library for MightEMacs script editing.

# Array of parameters used for finding macros in script files.
searchParams = [\
	/# Function search parameters #/\
['Macro',							/# User prompt #/\
'^[ \ta-z]*macro +%NAME%\b:re',					/# Macro RE #/\
 '*.mm'],							/# Glob pattern(s) #/\
	/# Class search parameters #/\
nil,\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Macros #####

# Wrap "if 0" and "endif" around a block of lines.
macro memacsWrapIf0(0) {desc: 'Wrap ~#uif 0~U and ~uendif~U around [-]n lines (default 1).'}
	$0 => langWrapPrep false,'','0','endif'
endmacro

# Wrap "if 1" and "endif" around a block of lines.
macro memacsWrapIf1(0) {desc: 'Wrap ~#uif 1~U and ~uendif~U around [-]n lines (default 1).'}
	$0 => langWrapPrep false,'','1','endif'
endmacro

# Wrap "if 0" and "else" around a block of lines, duplicate them, and add "endif".
macro memacsWrapIfElse(0) {desc: 'Wrap ~#uif 0~U and ~uelse~U around [-]n lines (default 1), duplicate them, and add\
 ~uendif~U.'}
	$0 => langWrapIfElse ''
endmacro

# Find macro definition, given name, in a file or current buffer.
macro memacsFindMacro(0) {desc: "Prompt for a directory to search and the name of a macro and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~0\
 options)."}
	$0 => langFindObj true
endmacro

# If n < 0, return true if first keyword on line is a "begin block" directive; otherwise, false.  If n == 0, return true if null
# or comment line; otherwise, false.  If n > 0, return true if line begins with an "endXXX" (n == 1) or "else" (n > 1)
# directive; otherwise, false.
macro memacsIsLine?(0)
	if $0 == 0
		# Null or comment?
		return null?(line = strip $lineText) || substr(line,0,1) == '#'
	else
		beginText
		if subline(0,1) =~ '[a-z]'
			oldMsgState = -1 => chgGlobalMode 'RtnMsg'
			setMark
			endWord
			lineSym = $RegionText
			oldMsgState => chgGlobalMode 'RtnMsg'
			if $0 > 1
				return lineSym == 'else'
			else
				symList = ($0 == 1) ? ['endif','endloop','endmacro'] :\
				 ['if','for','loop','macro','until','while']
				for sym in symList
					if lineSym == sym
						return true
					endif
				endloop
			endif
		endif
	endif

	false
endmacro

# Go to matching block end point if current line begins with a block keyword.  Set mark '.' to current position, mark 'T' to
# top line of the block, mark 'E' to "else" line (if any), and mark 'B' to the bottom line.  If $0 == 0, be silent about errors.
macro memacsGotoBlockEnd(0) {desc: "Go to block keyword paired with one in current line and set marks '.' (previous position),\
 'T' (top line), 'E' (\"else\" line, if any), and 'B' (bottom line).  If n == 0, be silent about errors.  Returns: true if\
 successful; otherwise, false."}
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	wordChars = $wordChars
	$wordChars = 'A-Za-z0-9_'
	1 => setMark ??
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
			break 2						# Not found.
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
	else
		beep
		-1 => message nil,errorMsg
	endif
endmacro

# Done.  Return array of key bindings for "public" macros and array of "macro search" parameters (see comments at beginning
# of lang.mm for details).
[["memacsFindMacro\tC-c C-f","memacsGotoBlockEnd\tC-c C-g",\
 "memacsWrapIf0\tM-0","memacsWrapIf1\tM-1","memacsWrapIfElse\tM-2"],\
 searchParams]
