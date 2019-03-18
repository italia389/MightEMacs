# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# genInit.ms		Ver. 9.2.0
#	MightEMacs initialization file for generic scripting language.  Returns registration array as described in lang.ms.

['Generic Script',						/# Library name (if different from mode name) #/\
 'Gen',								/# Mode name #/\
 'Generic script auto-formatting.',				/# Mode description #/\
 'gen',								/# Macro and library name prefix #/\
 ['asm','cl','go','latex','lisp','m','md','r','S','s','scala',\
  'scm','tex','xml'],						/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 nil,								/# Word characters #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 nil,								/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [nil,								/# EOL indenting next (chars, strings, and/or keywords) #/\
 nil,								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 nil,								/# BOL indenting next (strings and/or keywords) #/\
 nil]								/# BOL outdenting next (keywords) #/\
 ]
