# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# javaInit.mm		Ver. 9.1.0
#	MightEMacs initialization file for Java programming language.  Returns registration array as described in lang.mm.

[nil,								/# Library name (if different from mode name) #/\
 'Java',							/# Mode name #/\
 'Java source code auto-formatting.',				/# Mode description #/\
 'java',							/# Macro and library name prefix #/\
 ['java'],							/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 nil,								/# Word characters #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 [?},['case','switch'],'catch','else','finally'],		/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [[?{,?:],							/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['do','else','for','if','switch','while'],			/# BOL indenting next (strings and/or keywords) #/\
 ['break','continue','return']]					/# BOL outdenting next (keywords) #/\
 ]
