# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# rustInit.ms		Ver. 9.4.0
#	MightEMacs initialization file for Rust source code.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'Rust',							/# Mode name #/\
 'Rust source code auto-formatting.',				/# Mode description #/\
 'rust',							/# Macro and library name prefix #/\
 ['rs'],							/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 'A-Za-z0-9_!',							/# Word characters #/\
 'Fence1',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 [?},'];','else'],						/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline macro triggers #/\
 [[?{,?[],							/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['else','for','if','loop','while'],				/# BOL indenting next (strings and/or keywords) #/\
 ['break','continue','return']]					/# BOL outdenting next (keywords) #/\
 ]
