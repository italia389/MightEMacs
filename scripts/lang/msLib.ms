# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# msLib.ms		Ver. 10.0.0
#	MightEMacs library for MightEMacs script editing.

# Parameters for finding user commands and functions in script files.

# Function search parameters.
funcSearch = ['Command or function',				/# User prompt #/\
 '^\h*(?:command|function) +%NAME%\>:re']			/# User routine RE #/

# Class search parameters.
classSearch = nil

# Other parameters.
dirGlob = [nil,							/# Slot for search directory obtained from user #/\
 '*.ms']							/# Glob pattern(s) #/

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
command msFindRoutine(0) {description: "Prompt for a directory to search and the name of a user command or function, and find\
 its definition.  If directory is blank, current buffer is searched (only); otherwise, all files in directory matching\
 \"#{dirGlob[1]}\" template are searched, and first file found containing definition is rendered according to n\
 argument (with ~bselectBuf~B options)."}
	$0 => langFindObj true
endroutine

# Go to other end of block if current line contains a block begin/end symbol, such as "if".  Set mark '`' to current position,
# mark '^' to top line of block, mark ':' to "else" line (if any), and mark '$' to bottom line.
command msGotoBlkBound(0) {description: "Go to other end of block if current line contains a block begin/end symbol (such as\
 \"if\").  Set mark ~u`~U to current position, mark ~u^~U to top line of block, mark ~u:~U to \"else\" line (if any), and mark\
 ~u$~U to bottom line.\n\n\
Returns: true if successful, otherwise false."}
	$0 => langGotoBlkBound $langParams['MScript']
endroutine

# Done.  Return array of key bindings for "public" commands and search parameters (see comments at beginning of lang.ms for
# details).
[["msFindRoutine\tC-c C-f", "msGotoBlkBound\tC-c C-g",\
 "msWrapIf0\tM-0", "msWrapIf1\tM-1", "msWrapIfElse\tM-2"],\
 funcSearch, classSearch, dirGlob]
