# (c) Copyright 2017 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# toolbox.mm		Ver. 8.5.0
#	General purpose tools for MightEMacs macros.

# Don't load this file if it has already been done so.
if defined? 'tbInit'
	return
endif

# Render an existing buffer according to n argument, given (1), buffer name; (2), dot line number; and (3), Boolean "is new?".
macro renderBuf(3) {usage: 'name,dot-line,new?',\
 desc: 'Render an existing buffer (with "selectBuf" options)'}

	# Leave it as is?
	if $0 == 0
		notice "Found in #{$3 ? '' : 'old '}buffer '#{$1}'"
	else
		# Buffer being displayed in current window?
		if $1 == $bufName
			-1 => setMark
			gotoLine $2
			$windSize / 5 => redrawScreen
			notice "Mark '.' set to previous position"
		else
			# Buffer being displayed in another window on current screen?
			$0 == defn and $0 = 1
			if $WindCount > 1 && (otherWind = bufWind $1) != nil
				saveWind
				otherWind => nextWind
				gotoLine $2
				$windSize / 5 => redrawScreen
				if $0 == 1 || $0 == 3
					return
				endif
				restoreWind
				if $0 < 0
					return notice 'Buffer is being displayed'
				endif
			endif

			# All special cases dealt with.  Just select it.
			$0 => selectBuf $1
			$0 == 1 && $windSize / 5 => redrawScreen
			$3 || notice 'Old buffer'
		endif
	endif
endmacro

# Determine boundaries of a line block using n arg in same manner as the xxxLine commands.  Set mark ' ' to last line of block
# and dot to first line.
macro markBlock(0) {desc: "Find block boundaries per n argument; set mark ' ' to last line, dot to first line"}
	formerMsgState = -1 => alterGlobalMode 'msg'
	if $0 == 0					# Region already marked?
		lineNum = $bufLineNum			# Yes, figure out where top line is.
		swapMark
		$bufLineNum > lineNum && swapMark
							# Not marked ... so mark it.
	elsif ($0 = ($0 == defn) ? 1 : $0) > 0
		beginLine				# Forward.
		setMark
		$0 - 1 => forwLine
		endLine
		swapMark
	else						# Backward.
		endLine
		setMark
		beginLine
		$0 => forwLine				# Move backward.
	endif
	formerMsgState => alterGlobalMode 'msg'
endmacro

# Do wrap preparation, given (1), "copy block" flag; (2), lead-in character ('#' or ''); (3), "if" argument; and (4), ending
# conditional keyword ('else' or 'endif').  First, determine boundaries of line block using n argument in same manner as the
# built-in line commands.  Then do some common insertions and optionally, copy line block to kill buffer.
macro langWrapPrep(4)
	formerMsgState = -1 => alterGlobalMode 'msg'
	$0 => markBlock					# Mark block boundaries and position dot.
	$1 && 0 => copyLine				# Copy block to kill buffer if requested.
	beginLine
	openLine					# Insert "{#|!}if 0" or "{#|!}if 1".
	insert $2,'if ',$3
	swapMark
	endLine
	insert "\n#{$2}#{$4}"				# Insert "{#|!}else" or "{#|!}endif".
	forwChar
	formerMsgState => alterGlobalMode 'msg'
endmacro

# Wrap "{#|}if 0" and "{#|}else" around a block of lines, duplicate them, and add "{#|}endif", given (1), lead-in character
# ('#' or '').
macro langWrapIfElse(1)
	formerMsgState = -1 => alterGlobalMode 'msg'
	$0 => langWrapPrep true,$1,'0','else'
	setMark
	yank
	insert $1,"endif\n"
	swapMark
	beginText
	formerMsgState => alterGlobalMode 'msg'
endmacro

# Find first file that contains requested routine name and render it according to n argument if found.  Arguments: (1), name of
# RE global variable for specific language that matches a routine name and contains '%NAME%' as a placeholder for it; (2),
# name of "search directory" global variable; (3), filename template; and (4), prompt string (type of routine).
macro langFindRoutine(4)

	# Get directory to search and update global variable.
	haveDefault = defined? $2
	dir = prompt 'Directory to search',haveDefault ? (eval $2) : nil,'f'
	if nil?(dir) && !haveDefault || null?(dir)
		clearMsg
		return false
	endif
	eval "#{$2} = subString(dir,-1,1) == '/' && length(dir) > 1 ? subString(dir,0,-1) : dir"

	# Get routine name.
	routineName = prompt "#{$4} name"
	if empty?(routineName)
		clearMsg
		return false
	endif

	# Call file scanner.
	if (result = eval "locateFile #{$2},#{quote $3},#{quote sub eval($1),'%NAME%',routineName}") == false
		-1 => notice 'Not found'
	else
		# Render buffer.  result is: [bufName,lineNum,isNewBuf].
		$0 => renderBuf result[0],result[1],result[2]
	endif
endmacro

# Activate or deactivate a toolbox package, given (1), toolbox name; (2), binding list; and (3), Boolean action.  Return Boolean
# result.
macro tbInit(3) {usage: 'name,bindings,action',desc: 'Activate or deactivate a toolbox package'}
	result = true

	# Cycle through the binding list.
	for key in $2
		name = strShift key,"\t"
		if $3
			eval "bindKey #{quote key},#{name}"
		else
			result = 1 => unbindKey key
		endif
	endloop

	notice "#{$1} toolbox #{$3 ? 'activated' : 'deactivated'}"
	return result
endmacro
