# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# swiftLib.mm		Ver. 9.1.0
#	MightEMacs library for Swift source code editing.

# Array of parameters used for finding functions in source files.
searchParams = [\
	/# Function search parameters #/\
['Function',							/# User prompt #/\
 '^[ \t]*func +%NAME%[<(].+\{[ \t]*$:re',			/# Function RE #/\
 '*.swift'],							/# Glob pattern(s) #/\
	/# Class search parameters #/\
nil,\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Macros #####

# Find function definition, given name, in a file or current buffer.
macro swiftFindFunc(0) {desc: "Prompt for a directory to search and the name of a function and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~0\
 options)."}
	$0 => langFindObj true
endmacro

require 'ppLib'

# Done.  Return array of key bindings for "public" macros and array of "function search" parameters (see comments at beginning
# of lang.mm for details).
[["swiftFindFunc\tC-c C-f","ppGotoIfEndif\tC-c C-g","ppWrapList\tC-c ,",\
 "ppWrapIf0\tESC 0","ppWrapIf1\tESC 1","ppWrapIfElse\tESC 2",\
 "ppWrapIfName0\tC-c 0","ppWrapIfName1\tC-c 1","ppWrapIfElseName\tC-c 2",\
 "ppDeletePPLines\tC-c #"],\
 searchParams]