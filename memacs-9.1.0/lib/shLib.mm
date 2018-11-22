# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# shLib.mm		Ver. 9.1.0
#	MightEMacs library for shell script editing.

##### Macros #####

# Wrap "if false; then" and "fi" around a block of lines.
macro shWrapIf0(0) {desc: 'Wrap ~#uif false; then~U and ~ufi~U around [-]n lines (default 1).'}

	$0 => langWrapPrep false,'','false; then','endif'
endmacro

# Wrap "if true; then" and "fi" around a block of lines.
macro shWrapIf1(0) {desc: 'Wrap ~#uif true; then~U and ~ufi~U around [-]n lines (default 1).'}
	$0 => langWrapPrep false,'','true; then','endif'
endmacro

# Wrap "if false; then" and "else" around a block of lines, duplicate them, and add "fi".
macro shWrapIfElse(0) {desc: 'Wrap ~#uif false; then~U and ~uelse~U around [-]n lines (default 1), duplicate them,\
 and add ~ufi~U.'}
	$0 => langWrapIfElse ''
endmacro

# Go to matching if... or fi if current line begins with one of the two keywords.  Save current position in mark '.', set
# mark 'T' to if... line, mark 'E' to else (if it exists), and mark 'B' to fi.  If $0 == 0, be silent about errors.
# Return true if successful; otherwise, false.
macro shGotoIfEndif(0) {desc: "Go to ~uif...~U or ~ufi~U paired with one in current line and set marks '.' (previous\
 position), 'T' (top macro line), 'E' (else macro line, if any), and 'B' (bottom macro line).  Returns: true if successful;\
 otherwise, false."}

	# Current line begin with if... or fi?
	x = subline -$lineOffset,6
	if substr(x,0,2) == 'if'
		isIf = true
	elsif x == 'fi'
		isIf = false
	else
		$0 == 0 || beep
		return false
	endif

	# if/fi found.  Prepare for forward or backward search.
	oldMsgState = -1 => chgGlobalMode 'RtnMsg'
	-1 => setMark
	beginLine
	if isIf
		alias _nextLine = forwLine
		this = 'if'
		thisLen = 2
		1 => setMark ?T
		deleteMark ?B
		that = 'fi'
		thatLen = 2
		thatMark = ?B
	else
		alias _nextLine = backLine
		this = 'fi'
		thisLen = 2
		1 => setMark ?B
		deleteMark ?T
		that = 'if'
		thatLen = 2
		thatMark = ?T
	endif

	level = 0
	foundElse = false
	found = true
	deleteMark ?E

	# Scan lines until other keyword found.
	loop
		if _nextLine() == false
			gotoMark ?.				# Other keyword not found!
			if $0 != 0
				beep
				1 => message 'NoWrap',that,' not found'
			endif
			found = false
			break
		endif

		if subline(0,thatLen) == that
			if level == 0
				1 => setMark thatMark		# Found other keyword.
				break
			else
				--level
			endif
		elsif subline(0,thisLen) == this
			++level
		elsif subline(0,5) == 'else' && level == 0
			foundElse = true
			1 => setMark ?E
		endif
	endloop

	# Clean up.
	deleteAlias _nextLine
	oldMsgState => chgGlobalMode 'RtnMsg'
	found
endmacro

# Done.  Return array of key bindings for "public" macros and array of "subroutine search" parameters (see comments at beginning
# of lang.mm for details).  The latter array is nil -- not used.
[["shGotoIfEndif\tC-c C-g","shWrapIf0\tESC 0","shWrapIf1\tESC 1","shWrapIfElse\tESC 2"],\
 nil]
