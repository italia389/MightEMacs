# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# rubyInit.ms		Ver. 9.5.0
#	MightEMacs initialization file for Ruby scripting language.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'Ruby',							/# Mode name #/\
 'Ruby script auto-formatting.',				/# Mode description #/\
 'ruby',							/# Routine and library name prefix #/\
 ['rb'],							/# Array of extensions used for source files #/\
 'ruby',							/# Unix binary name, or array of names #/\
 'Fence1',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [[?=],								/# Left justification (chars) #/\
 [?}, ?], 'end', 'else', 'elsif', 'ensure', 'rescue',\
 ['when', 'case']],						/# Outdenting (chars and/or strings) #/\
 ['then']],							/# Same-denting (strings) #/\
	/# newline triggers #/\
 [[?{,?[,?|,'do', 'then'],					/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}, ?]],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['begin', 'class', 'def', 'else', 'elsif', 'ensure', 'for',\
  'if', 'loop', 'module', 'rescue', 'unless', 'until', 'when',\
  'while'],							/# BOL indenting next (strings and/or keywords) #/\
 ['break', 'next', 'return']]					/# BOL outdenting next (keywords) #/\
 ]
