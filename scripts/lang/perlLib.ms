# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# perlLib.ms		Ver. 10.0.0
#	MightEMacs library for Perl script editing.

# Parameters for finding subroutines in source files.

# Subroutine search parameters.
subSearch = ['Subroutine',					/# User prompt #/\
 '^[ \t]*sub +%NAME% +\{[ \t]*$:re']				/# Subroutine RE #/

# Class search parameters.
classSearch = nil

# Other parameters.
dirGlob = [nil,							/# Slot for search directory obtained from user #/\
 '*.pl']							/# Glob pattern(s) #/

##### Commands and Functions #####

# Find subroutine definition, given name, in a file or current buffer.
command perlFindSub(0) {description: "Prompt for a directory to search and the name of a subroutine, and find its definition. \
 If directory is blank, current buffer is searched (only); otherwise, all files in directory matching \"#{dirGlob[1]}\"\
 template are searched, and first file found containing definition is rendered according to n argument (with ~bselectBuf~B\
 options)."}
	$0 => langFindObj true
endroutine

# Done.  Return array of key bindings for "public" commands and search parameters (see comments at beginning of lang.ms for
# details).
[["perlFindSub\tC-c C-f"],\
 subSearch, classSearch, dirGlob]
