# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# sqlInit.mm		Ver. 9.1.0
#	MightEMacs initialization file for SQL scripting language.  Returns registration array as described in lang.mm.

[nil,								/# Library name (if different from mode name) #/\
 'SQL',								/# Mode name #/\
 'SQL script auto-formatting.',					/# Mode description #/\
 'sql',								/# Macro and library name prefix #/\
 ['sql'],							/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 nil,								/# Word characters #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 [';','from','group','order','where'],				/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [['from','select','where'],					/# EOL indenting next (chars, strings, and/or keywords) #/\
 nil,								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['group','order'],						/# BOL indenting next (strings and/or keywords) #/\
 nil]								/# BOL outdenting next (keywords) #/\
 ]
