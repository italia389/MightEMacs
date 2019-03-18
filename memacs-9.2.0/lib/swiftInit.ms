# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# swiftInit.ms		Ver. 9.2.0
#	MightEMacs initialization file for Swift programming language.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'Swift',							/# Mode name #/\
 'Swift source code auto-formatting.',				/# Mode description #/\
 'swift',							/# Macro and library name prefix #/\
 ['swift'],							/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 nil,								/# Word characters #/\
	/# preKey hook triggers #/\
 [[?#],								/# Left justification (chars) #/\
 [?},?],?),['case','enum','switch'],'default','else'],		/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [[?{,?[,?(,?:],						/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?},?],?)],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['class','else','for','func','if','repeat','switch','while'],	/# BOL indenting next (strings and/or keywords) #/\
 ['break','continue','fallthrough','return']]			/# BOL outdenting next (keywords) #/\
 ]
