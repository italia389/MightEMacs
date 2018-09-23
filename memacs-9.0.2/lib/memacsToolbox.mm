# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# memacsToolbox.mm	Ver. 9.0.0
#	MightEMacs macros for MightEMacs script editing.

# Don't load this file if it has already been done so.
if defined? 'memacsInit'
	return nil
endif

require 'toolbox'

# Set the following variable to a regular expression that will match the first line of a macro in a script file; that is, the
# line containing the name of the macro.  The langFindRoutine macro will replace the '%NAME%' string with the name of the macro
# being searched for each time the RE is used.  The langFindRoutine macro is called by the memacsFindMacro macro, which is bound
# to 'C-c C-f'.
$MemacsMacroInfo = ['Macro','^[ \t]*macro +%NAME%\b:re','*.mm',nil]

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

# Find first file that contains given macro name and render it according to n argument if found.
macro memacsFindMacro(0) {desc: "Prompt for a directory to search and the name of a macro and find first file matching\
 \"#{$MemacsMacroInfo[2]}\" template that contains the macro definition (with ~bselectBuf~0 options)."}
	$0 => langFindRoutine '$MemacsMacroInfo'
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
			formerMsgState = -1 => alterGlobalMode 'Msg'
			setMark
			endWord
			lineSym = $RegionText
			formerMsgState => alterGlobalMode 'Msg'
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
	formerMsgState = -1 => alterGlobalMode 'Msg'
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
			break 2						# Not found.
		endloop

		# Block-end line found.  Prepare for forward or backward search.
		beginText
		if isEnd
			direc = -1					# Go backward.
			1 => setMark 'B'
			deleteMark 'T'
			thatMark = 'T'
		else
			direc = 1					# Go forward.
			1 => setMark 'T'
			deleteMark 'B'
			thatMark = 'B'
		endif

		level = 0
		foundElse = false
		deleteMark 'E'

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
	formerMsgState => alterGlobalMode 'Msg'
	if found
		-1 => setMark
		gotoMark thatMark
		success nil
	elsif $0 == 0
		true
	else
		beep
		failure errorMsg
	endif
endmacro

# Activate or deactivate tools, given Boolean argument.
macro memacsInit(1)

	# Proceed only if activation request and tools not active or deactivation request and tools active.
	if $1 != (binding('C-c C-g') == 'memacsGotoBlockEnd')
		$wordChars = $1 ? 'A-Za-z0-9_?' : ''
		bindings = ["memacsFindMacro\tC-c C-f","memacsGotoBlockEnd\tC-c C-g",\
		 "memacsWrapIf0\tM-0","memacsWrapIf1\tM-1","memacsWrapIfElse\tM-2"]
		eval "tbInit 'MightEMacs',bindings,$1"
	endif
endmacro

# Done!
true
