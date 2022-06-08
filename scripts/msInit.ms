# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# msInit.ms		Ver. 9.6.0
#	MightEMacs initialization file for MScript language.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'MScript',							/# Mode name #/\
 'MScript auto-formatting.',					/# Mode description #/\
 'ms',								/# Routine and library name prefix #/\
 ['ms', 'mm'],							/# Array of extensions used for source files #/\
 ['memacs', 'mm'],						/# Unix binary name, or array of names #/\
 'Fence1',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 ['else', 'elsif', 'endif', 'endloop', 'endroutine'],		/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline triggers #/\
 [nil,								/# EOL indenting next (chars, strings, and/or keywords) #/\
 nil,								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['command', 'if', 'else', 'elsif', 'for', 'function', 'loop',\
  'until', 'while'],						/# BOL indenting next (strings and/or keywords) #/\
 ['break', 'next', 'return']]					/# BOL outdenting next (keywords) #/\
 ]
