# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# toolbox.mm		Ver. 9.0.0
#	General purpose tools for MightEMacs macros.

# Don't load this file if it has already been done so.
if defined? 'tbInit'
	return nil
endif

# Determine boundaries of a line block using n argument in same manner as the xxxLine commands.  Set mark ' ' to last line of
# block and dot to first line.
macro markBlock(0) {desc: "Find block boundaries per n argument; set mark ' ' to last line and move point to first line."}
	formerMsgState = -1 => alterGlobalMode 'Msg'
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
	formerMsgState => alterGlobalMode 'Msg'
endmacro

# Do wrap preparation, given (1), "copy block" flag; (2), lead-in character ('#' or ''); (3), "if" argument; and (4), ending
# conditional keyword ('else' or 'endif').  First, determine boundaries of line block using n argument in same manner as the
# built-in line commands.  Then do some common insertions and optionally, copy line block to kill buffer.
macro langWrapPrep(4)
	formerMsgState = -1 => alterGlobalMode 'Msg'
	$0 => markBlock					# Mark block boundaries and position dot.
	$1 && 0 => copyLine				# Copy block to kill buffer if requested.
	beginLine
	openLine					# Insert "{#|!}if 0" or "{#|!}if 1".
	insert $2,'if ',$3
	swapMark
	endLine
	insert "\n#{$2}#{$4}"				# Insert "{#|!}else" or "{#|!}endif".
	forwChar
	formerMsgState => alterGlobalMode 'Msg'
endmacro

# Wrap "{#|}if 0" and "{#|}else" around a block of lines, duplicate them, and add "{#|}endif", given (1), lead-in character
# ('#' or '').
macro langWrapIfElse(1)
	formerMsgState = -1 => alterGlobalMode 'Msg'
	$0 => langWrapPrep true,$1,'0','else'
	setMark
	yank
	deleteKill
	insert $1,"endif\n"
	swapMark
	beginText
	formerMsgState => alterGlobalMode 'Msg'
endmacro

# Find first file that contains a routine name (obtained from user) and render it according to n argument if found.  Arguments:
# (1), name of "info" global variable for specific language that contains a four-element array:
#	0: Prompt string (type of routine).
#	1: RE that matches a routine name and contains '%NAME%' as a placeholder for it.
#	2: Filename template that matches files to search.
#	3: Slot for search directory obtained from user.
# Elements 3 and 4 (initially nil) are updated by this routine and used as defaults for subseqent calls.
macro langFindRoutine(1)
	info = eval $1

	# Get directory to search and update array in global variable.
	dir = prompt 'Directory to search',info[3],'f'
	if empty?(dir)
		return false
	endif
	info[3] = substr(dir,-1,1) == '/' && length(dir) > 1 ? substr(dir,0,-1) : dir

	# Get routine name.
	routineName = prompt('%s name' % info[0])
	if empty?(routineName)
		return false
	endif

	# Call file scanner (dir,glob,pat) and render buffer if found.
	if not result = locateFile(info[3],info[2],sub info[1],'%NAME%',routineName)
		failure 'Not found'
	else
		bufName,mark = result

		# Leave buffer as is?
		if $0 == 0
			success 'Found in ',mark ? 'existing ' : '',"buffer '",bufName,"'"
		else
			# Render the buffer.
			result = $0 => selectBuf bufName
			if $0 == -1
				true
			else
				windNum = result[2]
				if(windNum != $windNum)
					saveWind
					selectWind windNum
					mark and -1 => gotoMark mark
					(size = $windSize / 5) == 0 and size = 1
					size => reframeWind
					restoreWind
				else
					mark and -1 => gotoMark mark
					(size = $windSize / 5) == 0 and size = 1
					size => reframeWind
				endif
				mark ? success("Mark '.' set to previous position") : true
			endif
		endif
	endif
endmacro

# Activate or deactivate a toolbox package, given (1), toolbox name; (2), binding list; and (3), Boolean action.  Return Boolean
# result.
macro tbInit(3) {usage: 'name,bindings,action',desc: 'Activate or deactivate a toolbox package.  Returns: true if successful;\
 otherwise, false.'}
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

	success "#{$1} toolbox #{$3 ? 'activated' : 'deactivated'}"
	result
endmacro

# Done!
true

