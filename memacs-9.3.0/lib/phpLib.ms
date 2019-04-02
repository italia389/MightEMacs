# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# phpLib.ms		Ver. 9.2.0
#	MightEMacs library for PHP script editing.

# Array of parameters used for finding functions in source files.
searchParams = [\
	/# Function search parameters #/\
['Function',							/# User prompt #/\
 '^[ \t]*function +%NAME%\(.+\{[ \t]*$:re',			/# Function RE #/\
 '*.php'],							/# Glob pattern(s) #/\
	/# Class search parameters #/\
['Class',							/# User prompt #/\
 '^[ \t]*class +%NAME% +\{[ \t]*$:re',				/# Class RE #/\
 '*.php'],							/# Glob pattern(s) #/\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Macros #####

# Find function definition, given name, in a file or current buffer.
macro phpFindFunc(0) {desc: "Prompt for a directory to search and the name of a function and find its definition.  If directory\
 is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\" template are\
 searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B options)."}
	$0 => langFindObj true
endmacro

# Find class definition, given name, in a file or current buffer.
macro phpFindClass(0) {desc: "Prompt for a directory to search and the name of a class and find its definition.  If directory\
 is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[1][2]}\" template are\
 searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B options)."}
	$0 => langFindObj false
endmacro

# Done.  Return array of key bindings for "public" macros and array of "function search" parameters (see comments at beginning
# of lang.ms for details).
[["phpFindFunc\tC-c C-f","phpFindClass\tC-c C-c"],\
 searchParams]
