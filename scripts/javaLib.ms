# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# javaLib.ms		Ver. 9.5.0
#	MightEMacs library for Java source code editing.

## Define regular expression that will match the first line of a method definition.  The javaFindMethod command will replace the
## '%NAME%' string with the name of the method being searched for each time the RE is used.
#$JavaMethodRE = '^[ \t]*[a-z]+[ \a_\d]+\b%NAME%\(.+\) *\{[ \t]*$:re'

# Array of parameters used for finding classes in source files.
searchParams = [\
	/# Function search parameters #/\
['Method',							/# User prompt #/\
 '^[ \t]*[\a_].+\b%NAME%\(.+\{[ \t]*$:re',			/# Name RE #/\
 '*.java'],							/# Glob pattern(s) #/\
	/# Class search parameters #/\
['Class',							/# User prompt #/\
 '^[ \t]*[ \a_\d]*\bclass +%NAME%[ \a_\d]+\{[ \t]*$:re',	/# Name RE #/\
 '*.java'],							/# Glob pattern(s) #/\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Commands and Functions #####

# Find method definition, given name, in a file or current buffer.
command javaFindMethod(0) {description: "Prompt for a directory to search and the name of a method and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B\
 options)."}
	$0 => langFindObj true
endroutine

# Find class definition, given name, in a file or current buffer.
command javaFindClass(0) {description: "Prompt for a directory to search and the name of a class and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[1][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B\
 options)."}
	$0 => langFindObj false
endroutine

# Done.  Return array of key bindings for "public" commands and array of "method/class search" parameters (see comments at
# beginning of lang.ms for details).
[["javaFindMethod\tC-c C-f", "javaFindClass\tC-c C-c"],\
 searchParams]
