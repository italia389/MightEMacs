# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# perlLib.ms		Ver. 9.6.0
#	MightEMacs library for Perl script editing.

# Array of parameters used for finding subroutines in source files.
searchParams = [\
	/# Subroutine search parameters #/\
['Subroutine',							/# User prompt #/\
 '^[ \t]*sub +%NAME% +\{[ \t]*$:re',				/# Subroutine RE #/\
 '*.pl'],							/# Glob pattern(s) #/\
	/# Class search parameters #/\
nil,\
	/# Other parameters #/\
nil]								/# Slot for search directory obtained from user #/

##### Commands and Functions #####

# Find subroutine definition, given name, in a file or current buffer.
command perlFindSub(0) {description: "Prompt for a directory to search and the name of a subroutine and find its definition.  If\
 directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{searchParams[0][2]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B\
 options)."}
	$0 => langFindObj true
endroutine

# Done.  Return array of key bindings for "public" commands and array of "subroutine search" parameters (see comments at beginning
# of lang.ms for details).
[["perlFindSub\tC-c C-f"],\
 searchParams]
