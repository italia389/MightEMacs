# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# macroUtil.ms		Ver. 9.6.0
#	Macro management routines, including ones for saving and restoring macros from a file in the HOME directory.

# Don't load this file if it has already been done so.
if defined? 'Name', 'macExist?'
	return
endif

##### Global variables #####

$MacroFileSuffix = '/.memacs-macros'

# Read user's macro file if it exists and save in macro ring.
path = env('HOME') & $MacroFileSuffix
if stat? path, 'e'
	viewFile path
	1 => chgBufAttr $bufname, 'Hidden'
	endBuf
	while -1 => beginLine
		(line = $lineText) =~ '^\W.' and 1 => manageMacro('Set', line)
	endloop
	-1 => lastBuf
endif

##### Commands and Functions #####

# Check if any macros exist.
function macExist?(0)
	ringSize('Macro')[0] == 0 ? 1 => message('Fail, High', 'No macro defined') : true
endroutine

# Prompt for a macro name and return it, or nil if none.  Arguments: (1), prompt prefix.
function getMacroName(1)
	name = prompt($1 & ' macro', 'Type: Macro')
	empty?(name) ? nil : name
endroutine

# Edit a macro.
command editMacro(0) {description: 'Edit a macro.'}
	if macExist? && !nil?(name = getMacroName('Edit'))
		if !manageMacro('Select', name)
			1 => message 'High', "No such macro '", name, "'"
		else
			# Put macro into a temporary buffer.
			scratchBuf
			insert "delRingEntry 'Macro', \"", name, "\"\n\n",\
			 "# Edit next line to rename macro and/or change its contents.\n",\
			 "1 => manageMacro 'Set', '", manageMacro('Get'), "'\n\n",\
			 "# When done editing line above, enter \"C-x x\" to execute this buffer and save\n",\
			 "# your changes.  You can also enter \"C-_ C-x \\\" after executing the buffer to\n",\
			 "# return to previous buffer and delete this one.\n"
			gotoLine 4
			3 => forwWord
		endif
	endif
endroutine

# Save macros to disk.
command saveMacros(0) {description: "Save macro ring to disk (in file ~~#{$MacroFileSuffix})."}
	if macExist?
		# Create buffer if needed and clear it.
		findFile(env('HOME') & $MacroFileSuffix)
		1 => chgBufAttr $bufname, 'Hidden'
		-1 => clearBuf $bufname

		# Write macro ring to buffer and sort it.
		for splitMacro in 1 => manageMacro('Get', 'All')

			# Move delimiter to end of line.
			insert splitMacro, substr(splitMacro[1], 0, 1), "\n"
		endloop
		markBuf
		sortRegion

		# Move delimiter back to beginning of each line.
		while -1 => endLine
			backChar
			c = $lineChar
			delForwChar
			beginLine
			insert chr c
		endloop

		# Save to disk and delete buffer.
		insert "MIGHTEMACS MACROS\n\n"
		oldMsgState = 1 => chgMode nil, 'RtnMsg'
		saveFile
		oldMsgState => chgMode nil, 'RtnMsg'
		-1 => lastBuf
	endif
endroutine
		
bindKey 'ESC C-s', saveMacros
bindKey 'ESC M', editMacro
