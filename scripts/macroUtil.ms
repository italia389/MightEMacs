# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# macroUtil.ms		Ver. 10.0.0
#	Macro management routines, including ones for saving and restoring macros from a file in the HOME directory.

# Don't load this file if it has already been done so.
if isExec? 'macExist?'
	return
endif

##### Global constants and variables #####

MacroFile = expandPath('~/.memacs-macros')

# Read user's macro file if it exists and save in macro table.
if stat? MacroFile, 'e'
	viewFile MacroFile
	1 => chgBufAttr 'Hidden', $bufname
	2 => beginLine
	$searchPat = '^([[:graph:]]+)\h+(.+)$:r'
	while huntForw
		loadMacro bufMatchGrp(1), bufMatchGrp(2)
	endloop
	delRingEntry 'Search'
	-1 => lastBuf
endif

##### Commands and Functions #####

# Check if any macros exist.
function macExist?(0)
	getInfo('MacroCount') == 0 ? 1 => message('Fail, High', 'No macro defined') : true
endroutine

# Save macro(s) to disk.
command saveMacros(0) {description: "Save macro table to disk (in file #{MacroFile})."}
	if macExist?
		# Create buffer if needed and clear it.
		findFile MacroFile
		1 => chgBufAttr 'Hidden', $bufname
		-1 => clearBuf $bufname

		# Write macro table to buffer.
		for macroInfo in getMacro()
			name, macro = macroInfo
			insert name, "\t"
			while $lineCol < 24
				insert "\t"
			endloop
			insert macro, "\n"
		endloop

		# Save to disk and delete buffer.
		beginBuf
		insert "MIGHTEMACS MACROS\n\n"
		oldMsgState = 1 => chgMode 'RtnMsg'
		saveFile
		oldMsgState => chgMode 'RtnMsg'
		-1 => lastBuf
		1 => message 'High', 'Macros saved to file "', MacroFile, '"'
	endif
endroutine
		
bindKey 'C-c C-s', saveMacros
