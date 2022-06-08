# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# perlInit.ms		Ver. 9.6.0
#	MightEMacs initialization file for Perl scripting language.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'Perl',							/# Mode name #/\
 'Perl script auto-formatting.',				/# Mode description #/\
 'perl',							/# Routine and library name prefix #/\
 ['pl'],							/# Array of extensions used for source files #/\
 'perl',							/# Unix binary name, or array of names #/\
 'Fence1',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [[?=],								/# Left justification (chars) #/\
 [?}, ?), ?], 'continue', ['else', 'case'], 'elsif'],		/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline triggers #/\
 [[?{,?(,?[],							/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}, ?), ?]],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['continue', 'do', 'if', 'else', 'elsif', 'for', 'foreach',\
  'sub', 'unless', 'until', 'while'],				/# BOL indenting next (strings and/or keywords) #/\
 ['return']]							/# BOL outdenting next (keywords) #/\
 ]
