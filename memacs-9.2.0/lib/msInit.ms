# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# msInit.ms		Ver. 9.2.0
#	MightEMacs initialization file for MScript language.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'MScript',							/# Mode name #/\
 'MScript auto-formatting.',					/# Mode description #/\
 'ms',								/# Macro and library name prefix #/\
 ['mm','ms'],							/# Array of extensions used for source files #/\
 ['memacs','mm'],						/# Unix binary name, or array of names #/\
 'A-Za-z0-9_?',							/# Word characters #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 ['else','elsif','endif','endloop','endmacro'],			/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [nil,								/# EOL indenting next (chars, strings, and/or keywords) #/\
 nil,								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['constrain','if','else','elsif','for','loop','macro','until',\
  'while'],							/# BOL indenting next (strings and/or keywords) #/\
 ['break','next','return']]					/# BOL outdenting next (keywords) #/\
 ]
