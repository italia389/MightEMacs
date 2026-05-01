# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# sqlInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for SQL scripting language.  Returns registration array as described in lang.ms.

{'LibName' => 'SQL',						/# Library name (if different from mode name) #/\
 'ModeName' => 'SQL',						/# Mode name #/\
 'ModeDesc' => 'SQL script auto-formatting.',			/# Mode description #/\
 'LibFile' => 'sqlLib',						/# "Lib" filename without extension #/\
 'FileExts' => ['sql'],						/# Array of extensions used for source files #/\
 'BinNames' => nil,						/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence1',					/# Fence mode to activate #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => nil,						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [[';', 'from', 'group', 'order', 'where'],	/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [['from', 'select', 'where'],			/# EOL indenting next (chars, strings, and/or keywords) #/\
 nil],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
\
	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [['group', 'order'],				/# BOL indenting next (strings and/or keywords) #/\
 nil]								/# BOL outdenting next (keywords) #/\
 }
