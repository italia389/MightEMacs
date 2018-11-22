# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# phpInit.mm		Ver. 9.1.0
#	MightEMacs initialization file for PHP scripting language.  Returns registration array as described in lang.mm.

[nil,								/# Library name (if different from mode name) #/\
 'PHP',								/# Mode name #/\
 'PHP script auto-formatting.',					/# Mode description #/\
 'php',								/# Macro and library name prefix #/\
 ['php'],							/# Array of extensions used for source files #/\
 'php',								/# Unix binary name, or array of names #/\
 nil,								/# Word characters #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 [?},?),['case','switch'],'catch','else'],			/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [[?{,?(,?:,'<?php'],						/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?},?)],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['class','do','else','elseif','for','foreach','function',\
  'if','switch','while'],					/# BOL indenting next (strings and/or keywords) #/\
 ['break','continue','return']]					/# BOL outdenting next (keywords) #/\
 ]
