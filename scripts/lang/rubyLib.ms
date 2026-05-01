# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# rubyLib.ms		Ver. 10.0.0
#	MightEMacs library for Ruby script editing.

# Parameters for finding methods, classes, and modules in script files.

# Method search parameters.
methodSearch = ['Method',					/# User prompt #/\
 '^[ \t]*def +[\a_\d.]*?\b%NAME%\b:re']				/# Name RE #/

# Class search parameters.
classSearch = ['Class or module',				/# User prompt #/\
 '^[ \t]*[cm][lo][ad][su][sl]e? +%NAME%\b:re']			/# Name RE #/

# Other parameters.
dirGlob = [nil,							/# Slot for search directory obtained from user #/\
 '*.rb']							/# Glob pattern(s) #/

##### Commands and Functions #####

# Wrap "=begin" and "=end" around a block of lines.
command rubyWrapBeginEnd(0) {description: 'Wrap ~u=begin~U and ~u=end~U around [-]n lines (default 1).'}
	-1 => selectLine $0				# Mark block boundaries and position point.
	openLine					# Insert "=begin".
	insert '=begin'
	swapMark ?.
	endLine
	insert "\n=end"					# Insert "=end".
	forwChar
endroutine

# Find method definition, given name, in a file or current buffer.
command rubyFindMethod(0) {description: "Prompt for a directory to search and the name of a method, and find its definition. \
 If directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{dirGlob[1]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B\
 options)."}
	$0 => langFindObj true
endroutine

# Find class or module definition, given name, in a file or current buffer.
command rubyFindCM(0) {description: "Prompt for a directory to search and the name of a class or module, and find its\
 definition.  If directory is blank, current buffer is searched (only); otherwise, all files in directory matching\
 \"#{dirGlob[1]}\" template are searched, and first file found containing definition is rendered according to n argument (with\
 ~bselectBuf~B options)."}
	$0 => langFindObj false
endroutine

# Go to other end of block if current line contains a block begin/end symbol, such as "if".  Set mark '`' to current position,
# mark '^' to top line of block, mark ':' to "else" line (if any), and mark '$' to bottom line.
command rubyGotoBlkBound(0) {description: "Go to other end of block if current line contains a block begin/end symbol (such\
 as \"if\").  Set mark ~u`~U to current position, mark ~u^~U to top line of block, mark ~u:~U to \"else\" line (if any), and\
 mark ~u$~U to bottom line.\n\n\
Returns: true if successful, otherwise false."}
	$0 => langGotoBlkBound $langParams['Ruby']
endroutine

# Done.  Return array of key bindings for "public" commands and search parameters (see comments at beginning of lang.ms for
# details).
[["rubyFindMethod\tC-c C-f", "rubyFindCM\tC-c C-c", "rubyGotoBlkBound\tC-c C-g", "rubyWrapBeginEnd\tESC 0"],\
 methodSearch, classSearch, dirGlob]
