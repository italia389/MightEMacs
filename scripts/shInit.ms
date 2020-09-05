# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# shInit.ms		Ver. 9.5.0
#	MightEMacs initialization file for shell scripts.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'Shell',							/# Mode name #/\
 'Shell script auto-formatting.',				/# Mode description #/\
 'sh',								/# Routine and library name prefix #/\
 ['sh'],							/# Array of extensions used for source files #/\
 ['ksh', 'sh', 'zsh'],						/# Unix binary name, or array of names #/\
 'Fence1',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 [?}, 'done', 'elif', 'else', 'esac', 'fi'],			/# Outdenting (chars and/or strings) #/\
 ['do', 'then']],						/# Same-denting (strings) #/\
	/# newline triggers #/\
 [[?{,?), 'do', 'then'],					/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}, ';;'],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['elif', 'else', 'for', 'if', 'sub', 'while'],			/# BOL indenting next (strings and/or keywords) #/\
 ['break', 'continue', 'return']]				/# BOL outdenting next (keywords) #/\
 ]
