# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# pythonInit.ms		Ver. 9.4.0
#	MightEMacs initialization file for Python scripting language.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'Python',							/# Mode name #/\
 'Python script auto-formatting.',				/# Mode description #/\
 'python',							/# Macro and library name prefix #/\
 ['py'],							/# Array of extensions used for source files #/\
 'python',							/# Unix binary name, or array of names #/\
 nil, 								/# Word characters #/\
 'Fence1',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 [?},?],?),'elif','else','except','finally'],			/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [[?:,?{,?[],							/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?},?],?)],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['class','def','else','elif','finally','if','for','while'],	/# BOL indenting next (strings and/or keywords) #/\
 ['break','continue','pass','return']]				/# BOL outdenting next (keywords) #/\
 ]
