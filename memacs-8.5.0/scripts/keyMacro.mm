# (c) Copyright 2017 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# keyMacro.mm		Ver. 8.5.0
#	Macros for managing multiple keyboard macros, stored in a file in the HOME directory.

# Don't load this file if it has already been done so.
if defined? 'kmKeyBuf?'
	return
endif

$KeyMacroFile = env('HOME') & '/.memacs-key'
$KeyMacroBuf = nil

# Read user's keyboard macro file if it exists.
if stat? $KeyMacroFile,'e'
	saveBuf
	findFile $KeyMacroFile
	$KeyMacroBuf = $bufName
	hideBuf
	restoreBuf
endif

# Check if keyboard macro buffer is active.
macro kmKeyBuf?(0)
	$KeyMacroBuf ? true : -1 => notice('No saved keyboard macros')
endmacro

# Prompt for a keyboard macro name and return it, or nil if none.
macro kmGetKeyName(0)
	p = 'Name'

	# Get possibilities, if available.
	if $KeyMacroBuf
		saveBuf
		selectBuf $KeyMacroBuf
		beginBuf
		forwLine
		maxLen = $TermCols * 7 / 10
		dlm = ' ('
		$searchPat = '^(\S+):re'
		while huntForw
			strPush p,dlm,1 => match(1)
			if length(p) > maxLen
				break
			endif
			dlm = ', '
		endloop
		dlm == ', ' and p &= ')'
		restoreBuf
	endif

	# Prompt for name.
	name = prompt p
	empty?(name) ? nil : 2 => sub(name,' ','_')
endmacro

# Find keyboard macro, given (1), name (or nil); and (2), Boolean value.  If n < 0, find by number; otherwise, find by name.  If
# macro is found, stay in KM buffer and return two-element array of macro name and contents; otherwise, restore buffer unless
# second argument is true, and return false.
macro kmFindKeyMacro(2)
	saveBuf
	selectBuf $KeyMacroBuf
	beginBuf
	2 => forwLine
	result = nil
	if $0 != defn
		if (selector = $0) < 0
			until 1 => bufBound?
				if ++$0 == 0
					result = $lineText
					$1 = strShift result,' '
					break
				endif
				forwLine
			endloop
		endif
	else
		result = searchForw("^#{selector = $1} :re") ? subLine(0) : nil
		notice nil
	endif
	if result == nil
		$2 || restoreBuf()
		-1 => notice "No such keyboard macro '#{selector}'"
	else
		[$1,-1 => strip(result)]
	endif
endmacro
		
# Fetch a saved keyboard macro.  If found, load it and return its name; otherwise, return nil.
macro kmFetch(0)
	name = nil
	if !kmKeyBuf? || $0 == defn && nil?(name = kmGetKeyName) || !(km = $0 => kmFindKeyMacro(name,false))
		return nil
	endif

	restoreBuf
	$keyMacro = km[1]
	km[0]
endmacro

# Select a saved keyboard macro.  If n < 0, find by number; otherwise, find by name.
macro kmSelectKeyMacro(0) {desc: 'Select a saved keyboard macro'}
	name = $0 => kmFetch and notice "#{name} loaded"
endmacro

# Execute a saved keyboard macro.  If n < 0, find by number; otherwise, find by name.
macro kmXeqKeyMacro(0) {desc: 'Execute a saved keyboard macro'}
	$0 => kmFetch && xeqKeyMacro
	nil
endmacro

# Save keyboard macro buffer to disk.
macro kmSaveFile(0)
	beginBuf
	2 => killLine
	pipeBuf 'sort'
	beginBuf
	yank
	cycleKillRing
	notice nil
	saveFile
	restoreBuf
endmacro

# Delete a saved keyboard macro.
macro kmDeleteKeyMacro(0) {desc: 'Delete a saved keyboard macro'}
	if !kmKeyBuf? || nil?(name = kmGetKeyName) || !kmFindKeyMacro(name,false)
		return
	endif

	deleteLine
	kmSaveFile
	notice "#{name} deleted"
endmacro

# Save a keyboard macro.
macro kmSaveKeyMacro(0) {desc: 'Save a keyboard macro (to a file)'}

	# Get name.
	if nil?(name = kmGetKeyName)
		return
	endif

	# Create buffer is needed.
	if !$KeyMacroBuf
		saveBuf
		findFile $KeyMacroFile
		$KeyMacroBuf = $bufName
		hideBuf
		insert "KEYBOARD MACROS\n"
	else
		kmFindKeyMacro(name,true) ? deleteLine : endBuf
		backChar
	endif

	# Save current keyboard macro in buffer and save to disk.
	insert "\n",'%-24s' % name,$keyMacro
	kmSaveFile
endmacro

		
# Display saved keyboard macros in a pop-up window.
macro kmShowKeyMacros(0) {desc: 'Display saved keyboard macros in a pop-up window'}
	if !kmKeyBuf?
		return
	endif

	# Get scratch buffer for numbered listing and copy header line.
	listBuf = 0 => scratchBuf
	1 => beginBuf $KeyMacroBuf
	writeBuf listBuf,readBuf($KeyMacroBuf),"\n"
	readBuf $KeyMacroBuf

	# Copy macro lines, flagging active one if found.
	keyMacro = $keyMacro
	i = 0
	found = false
	until nil?(line = readBuf $KeyMacroBuf)
		j = index(line,' ')
		writeBuf listBuf,"\n",'%-6d' % --i
		(isMatch = strip(subString(line,j)) == $keyMacro) and found = true
		writeBuf listBuf,isMatch ? '*' : ' ',line
	endloop
	found && writeBuf listBuf,"\n\n* Active"

	# Pop listing and delete buffer.
	-1 => selectBuf listBuf
	-1 => deleteBuf listBuf
endmacro

bindKey 'C-c k',kmSelectKeyMacro
bindKey 'C-c C-s',kmSaveKeyMacro
bindKey 'C-c C-d',kmDeleteKeyMacro
bindKey 'C-h y',kmShowKeyMacros
bindKey 'C-c e',kmXeqKeyMacro
