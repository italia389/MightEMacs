# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# javaLib.mm		Ver. 9.1.0
#	MightEMacs library for Java source code editing.

## Define regular expression that will match the first line of a method definition.  The javaFindMethod macro will replace the
## '%NAME%' string with the name of the method being searched for each time the RE is used.
#$JavaMethodRE = '^[ \t]*[a-z]+[ \l_\d]+\b%NAME%\(.+\) *\{[ \t]*$:re'

# Array of parameters used for finding classes in source files.
searchParams = [\
	/# Function search parameters #/\
['Method',							/# User prompt #/\
 '^[ \t]*[\l_].+\b%NAME%\(.+\{[ \t]*$:re',			/# Name RE #/\
 '*.java'],							/# Glob pattern(s) #/\
	/# Class search parameters #/\
['Class',							/# User prompt #/\
 '^[ \t]*[ \l_\d]*\bclass +%NAME%[ \l_\d]+\{[ \t]*$:re',	/# Name RE #/\
 '*.java'],							/# Glob pattern(s) #/\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Macros #####

# Find method definition, given name, in a file or current buffer.
macro javaFindMethod(0) {desc: "Prompt for a directory to search and the name of a method and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~0\
 options)."}
	$0 => langFindObj true
endmacro

# Find class definition, given name, in a file or current buffer.
macro javaFindClass(0) {desc: "Prompt for a directory to search and the name of a class and find its definition.  If directory\
 is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[1][2]}\" template are\
 searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~0 options)."}
	$0 => langFindObj false
endmacro

# Done.  Return array of key bindings for "public" macros and array of "method/class search" parameters (see comments at
# beginning of lang.mm for details).
[["javaFindMethod\tC-c C-f","javaFindClass\tC-c C-c"],\
 searchParams]
