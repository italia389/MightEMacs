# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# cInit.ms		Ver. 9.4.0
#	MightEMacs initialization file for C programming language.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'C',								/# Mode name #/\
 'C source code auto-formatting.',				/# Mode description #/\
 'c',								/# Macro and library name prefix #/\
 ['c','h'],							/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 nil,								/# Word characters #/\
 'Fence1',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [[?#],								/# Left justification (chars) #/\
 [?},['case','switch'],'else'],					/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [[?{,?:],							/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['do','else','for','if','switch','while'],			/# BOL indenting next (strings and/or keywords) #/\
 ['break','continue','return']]					/# BOL outdenting next (keywords) #/\
 ]
