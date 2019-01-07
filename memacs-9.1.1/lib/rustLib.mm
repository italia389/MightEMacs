# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# rustLib.mm		Ver. 9.1.0
#	MightEMacs library for Rust source-file editing.

# Array of parameters used for finding functions in source files.
searchParams = [\
	/# Function search parameters #/\
['Function',							/# User prompt #/\
 '^.*\bfn +%NAME%\(.+\{[ \t]*$:re',				/# Function RE #/\
 '*.rs'],							/# Glob pattern(s) #/\
	/# Module search parameters #/\
['Module',							/# User prompt #/\
 '^.*\bmod +%NAME% +\{[ \t]*$:re',				/# Module RE #/\
 '*.rs'],							/# Glob pattern(s) #/\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Macros #####

# Find function definition, given name, in a file or current buffer.
macro rustFindFunc(0) {desc: "Prompt for a directory to search and the name of a function and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~0\
 options)."}
	$0 => langFindObj true
endmacro

# Find module definition, given name, in a file or current buffer.
macro rustFindMod(0) {desc: "Prompt for a directory to search and the name of a module and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~0\
 options)."}
	$0 => langFindObj false
endmacro

# Done.  Return array of key bindings for "public" macros and array of "function/module search" parameters (see comments at
# beginning of lang.mm for details).
[["rustFindFunc\tC-c C-f","rustFindMod\tC-c C-c"],\
 searchParams]
