# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# keyMacro.mm		Ver. 9.0.0
#	Macros for managing multiple keyboard macros, stored in a file in the HOME directory.

# Don't load this file if it has already been done so.
if defined? 'kmKeyBuf?'
	return
endif

$KeyMacroInfo = ['/.memacs-key',nil]

# Read user's keyboard macro file if it exists.
path = env('HOME') & $KeyMacroInfo[0]
if stat? path,'e'
	saveBuf
	findFile path
	1 => alterBufAttr ($KeyMacroInfo[1] = $bufName),'Hidden'
	restoreBuf
endif

# Check if keyboard macro buffer is active.
macro kmKeyBuf?(0)
	$KeyMacroInfo[1] ? true : failure('No saved keyboard macros')
endmacro

# Prompt for a keyboard macro name and return it, or nil if none.  Arguments: (1), prompt prefix; (2), "display existing?".
macro kmGetKeyName(2)
	p = $1

	# Get possibilities, if requested and available.
	if $2 && $KeyMacroInfo[1]
		formerMsgState = -1 => alterGlobalMode 'Msg'
		saveBuf
		selectBuf $KeyMacroInfo[1]
		gotoLine 3
		$searchPat = '^(\S+):re'
		dlm = ' ('
		while huntForw
			strPush p,dlm,1 => match(1)
			dlm = ', '
		endloop
		dlm == ', ' and p &= ')'
		deleteSearchPat
		restoreBuf
		formerMsgState => alterGlobalMode 'Msg'
	endif

	# Prompt for name.
	name = prompt p
	empty?(name) ? nil : tr(name,' ','_')
endmacro

# Find keyboard macro, given (1), name; and (2), Boolean value.  If macro is found, stay in KM buffer and return two-element
# array of macro name and its contents; otherwise, restore buffer and set error message unless second argument is true, and
# return nil.
macro kmFindKeyMacro(2)
	formerMsgState = -1 => alterGlobalMode 'Msg'
	saveBuf
	selectBuf $KeyMacroInfo[1]
	gotoLine 3
	if not nil?(result = searchForw('^%s :re' % $1) ? subline(0) : nil)
		result = [$1,-1 => strip result]
	elsif not $2
		restoreBuf
		failure 'No such keyboard macro "',$1,'"'
	endif
	deleteSearchPat
	formerMsgState => alterGlobalMode 'Msg'
	result
endmacro
		
# Fetch a saved keyboard macro, given prompt prefix.  If found, load it and return its name; otherwise, return nil.
macro kmFetch(1)
	name = nil
	if !kmKeyBuf? || nil?(name = kmGetKeyName $1,true) || !(kmInfo = kmFindKeyMacro name,false)
		return nil
	endif

	restoreBuf
	$keyMacro = kmInfo[1]
	kmInfo[0]
endmacro

# Select a saved keyboard macro.
macro kmSelectKeyMacro(0) {desc: 'Select and load a saved keyboard macro.'}
	name = kmFetch('Load') and success name,' loaded'
	nil
endmacro

# Execute a saved keyboard macro.
macro kmXeqKeyMacro(0) {desc: 'Load and execute a saved keyboard macro.'}
	kmFetch('Execute') && xeqKeyMacro
	nil
endmacro

# Save keyboard macro buffer to disk.
macro kmSaveFile(0)
	formerMsgState = -1 => alterGlobalMode 'Msg'
	beginBuf
	2 => killLine
	-1 => alterBufAttr $bufName,'Changed'
	filterBuf 'sort'
	beginBuf
	0 => yank
	deleteKill
	formerMsgState => alterGlobalMode 'Msg'
	saveFile
	restoreBuf
endmacro

# Delete a saved keyboard macro.
macro kmDeleteKeyMacro(0) {desc: 'Delete a saved keyboard macro.'}
	if !kmKeyBuf? || nil?(name = kmGetKeyName 'Delete',true) || !kmFindKeyMacro(name,false)
		return
	endif

	deleteLine
	kmSaveFile
	success name,' deleted'
endmacro

# Rename a saved keyboard macro.
macro kmRenameKeyMacro(0) {desc: 'Rename a saved keyboard macro.'}
	if !kmKeyBuf? || nil?(name = kmGetKeyName 'Rename',true) || !(kmInfo = kmFindKeyMacro(name,false))
		return
	endif

	# Build prompt with old name and get new one.
	if empty?(newName = prompt 'to')
		restoreBuf
		nil
	else
		beginLine
		deleteToBreak
		newName = tr(newName,' ','_')
		insert '%-24s' % newName,kmInfo[1]
		kmSaveFile
		success name,' renamed to ',newName
	endif
endmacro

# Save a keyboard macro.
macro kmSaveKeyMacro(0) {desc: "Give current keyboard macro a name and save to disk (in file ~~#{$KeyMacroInfo[0]})."}

	# Get name.
	if nil?(name = kmGetKeyName 'Save macro as',false)
		return
	endif

	# Create buffer if needed and position point at correct line.
	if !$KeyMacroInfo[1]
		formerMsgState = -1 => alterGlobalMode 'Msg'
		saveBuf
		findFile(env('HOME') & $KeyMacroInfo[0])
		1 => alterBufAttr ($KeyMacroInfo[1] = $bufName),'Hidden'
		insert "KEYBOARD MACROS\n"
		formerMsgState => alterGlobalMode 'Msg'
	else
		kmFindKeyMacro(name,true) ? deleteLine : endBuf
		backChar
	endif

	# Write current keyboard macro to buffer and save to disk.
	insert "\n",'%-24s' % name,$keyMacro
	kmSaveFile
endmacro

		
# Display saved keyboard macros in a pop-up window.
macro kmShowKeyMacros(0) {desc: 'Display saved keyboard macros in a pop-up window.'}
	if !kmKeyBuf?
		return
	endif

	# Get scratch buffer for formatted listing and copy header line.
	formerMsgState = -1 => alterGlobalMode 'Msg'
	listBuf = (0 => scratchBuf)[0]
	kmBuf = $KeyMacroInfo[1]
	1 => beginBuf kmBuf
	bprint listBuf,bgets(kmBuf),"\n"
	bgets kmBuf

	# Copy macro lines, flagging active one if found.
	keyMacro = $keyMacro
	found = false
	until nil?(line = bgets kmBuf)
		name = strShift line,' '
		contents = strShift line,' '
		(isMatch = contents == keyMacro) and found = true
		bprintf listBuf,"\n %s~b%-24s~0%s",isMatch ? '*' : ' ',name,2 => sub(contents,'~','~~')
	endloop
	found && bprint listBuf,"\n\n* Active"
	-1 => alterBufAttr listBuf,'Changed'

	# Pop listing and delete buffer.
	1 => popBuf listBuf,'dt'
	formerMsgState => alterGlobalMode 'Msg'
endmacro

bindKey 'ESC K',kmSelectKeyMacro
bindKey 'ESC C-s',kmSaveKeyMacro
bindKey 'C-x K',kmDeleteKeyMacro
bindKey 'ESC R',kmRenameKeyMacro
bindKey 'C-h K',kmShowKeyMacros
bindKey 'C-c K',kmXeqKeyMacro
