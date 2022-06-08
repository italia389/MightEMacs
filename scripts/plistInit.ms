# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.plist.
#
# plistInit.ms		Ver. 9.6.0
#	MightEMacs initialization file for property lists.  Returns registration array as described in lang.ms.

['Property List',						/# Library name (if different from mode name) #/\
 'PList',							/# Mode name #/\
 'Property List auto-formatting.',				/# Mode description #/\
 'plist',							/# Routine and library name prefix #/\
 ['plist'],							/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 'Fence2',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 ["</array", "</dict"],						/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline triggers #/\
 [nil,								/# EOL indenting next (chars, strings, and/or keywords) #/\
 nil,								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ["<array", "<dict"],						/# BOL indenting next (strings and/or keywords) #/\
 nil]								/# BOL outdenting next (keywords) #/\
 ]
