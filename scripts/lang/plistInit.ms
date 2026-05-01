# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.plist.
#
# plistInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for property lists.  Returns registration array as described in lang.ms.

{'LibName' => 'Property List',					/# Library name (if different from mode name) #/\
 'ModeName' => 'PList',						/# Mode name #/\
 'ModeDesc' => 'Property List auto-formatting.',		/# Mode description #/\
 'LibFile' => 'plistLib',					/# "Lib" filename without extension #/\
 'FileExts' => ['plist'],					/# Array of extensions used for source files #/\
 'BinNames' => nil,						/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence2',					/# Fence mode to activate #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => nil,						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [["</array", "</dict"],			/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [nil,						/# EOL indenting next (chars, strings, and/or keywords) #/\
 nil],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
\
	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [["<array", "<dict"],				/# BOL indenting next (strings and/or keywords) #/\
 nil]								/# BOL outdenting next (keywords) #/\
 }
