# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# cppLib.ms		Ver. 9.6.0
#	MightEMacs library for C++ source-file editing.

# Array of parameters used for finding functions in script files.  The function RE here matches a line where the opening brace
# for the function is on the same line as the name.
searchParams = [\
	/# Function search parameters #/\
['Function',							/# User prompt #/\
 '^[\a_].+\b%NAME%\(.+\{[ \t]*$:re',				/# Name RE #/\
 ['*.cpp', '*.cc', '*.C', '*.CPP', '*.cxx', '*.c++']],		/# Glob pattern(s) #/\
	/# Class search parameters #/\
['Class',							/# User prompt #/\
 '^class +%NAME%\b.* \{[ \t]*$:re',				/# Name RE #/\
 ['*.cpp', '*.cc', '*.C', '*.CPP', '*.cxx', '*.c++']],		/# Glob pattern(s) #/\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Commands and Functions #####

# Find function definition, given name, in a file or current buffer.
command cppFindFunc(0) {description: "Prompt for a directory to search and the name of a function and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B\
 options)."}
	$0 => langFindObj true
endroutine

# Find class definition, given name, in a file or current buffer.
command cppFindClass(0) {description: "Prompt for a directory to search and the name of a class and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B\
 options)."}
	$0 => langFindObj false
endroutine

require 'ppLib'

# Done.  Return array of key bindings for "public" commands and array of "function/class search" parameters (see comments at
# beginning of lang.ms for details).
[["cppFindFunc\tC-c C-f", "cppFindClass\tC-c C-c", "ppGotoIfEndif\tC-c C-g", "ppWrapList\tC-c ,",\
 "ppWrapIf0\tESC 0", "ppWrapIf1\tESC 1", "ppWrapIfElse\tESC 2",\
 "ppWrapIfName0\tC-c 0", "ppWrapIfName1\tC-c 1", "ppWrapIfElseName\tC-c 2",\
 "ppDeletePPLines\tC-c #"],\
 searchParams]
