# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# jsInit.ms		Ver. 9.5.0
#	MightEMacs initialization file for JavaScript language.  Returns registration array as described in lang.ms.

['JavaScript',							/# Library name (if different from mode name) #/\
 'JS',								/# Mode name #/\
 'JavaScript auto-formatting.',					/# Mode description #/\
 'js',								/# Routine and library name prefix #/\
 ['js'],							/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 'Fence1',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 [?}, ['case', 'switch'], 'catch', 'else', 'finally'],		/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline triggers #/\
 [[?{,?:],							/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['do', 'else', 'for', 'if', 'switch', 'while'],		/# BOL indenting next (strings and/or keywords) #/\
 ['break', 'continue', 'return']]				/# BOL outdenting next (keywords) #/\
 ]
