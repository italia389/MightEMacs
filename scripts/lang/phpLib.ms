# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# phpLib.ms		Ver. 10.0.0
#	MightEMacs library for PHP script editing.

# Parameters for finding functions in source files.

# Function search parameters.
funcSearch = ['Function',					/# User prompt #/\
 '^[ \t]*function +%NAME%\(.+\{[ \t]*$:re']			/# Function RE #/

# Class search parameters.
classSearch = ['Class',						/# User prompt #/\
 '^[ \t]*class +%NAME% +\{[ \t]*$:re']				/# Class RE #/

# Other parameters.
dirGlob = [nil,							/# Slot for search directory obtained from user #/\
 '*.php']							/# Glob pattern(s) #/

##### Commands and Functions #####

# Find function definition, given name, in a file or current buffer.
command phpFindFunc(0) {description: "Prompt for a directory to search and the name of a function, and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{dirGlob[1]}\" template\
 are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B options)."}
	$0 => langFindObj true
endroutine

# Find class definition, given name, in a file or current buffer.
command phpFindClass(0) {description: "Prompt for a directory to search and the name of a class, and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{dirGlob[1]}\" template\
 are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B options)."}
	$0 => langFindObj false
endroutine

# Done.  Return array of key bindings for "public" commands and search parameters (see comments at beginning of lang.ms for
# details).
[["phpFindFunc\tC-c C-f", "phpFindClass\tC-c C-c"],\
 funcSearch, classSearch, dirGlob]
