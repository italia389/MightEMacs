# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# swiftInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for Swift programming language.  Returns registration array as described in lang.ms.

{'LibName' => 'Swift',						/# Library name (if different from mode name) #/\
 'ModeName' => 'Swift',						/# Mode name #/\
 'ModeDesc' => 'Swift source code auto-formatting.',		/# Mode description #/\
 'LibFile' => 'swiftLib',					/# "Lib" filename without extension #/\
 'FileExts' => ['swift'],					/# Array of extensions used for source files #/\
 'BinNames' => nil,						/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence1',					/# Fence mode to activate #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => '#',						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [[?}, ?], ?), ['case', 'enum', 'switch'],\
 'default', 'else'],						/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [[?{, ?[, ?(, ?:],				/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}, ?], ?)]],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
\
	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [['class', 'else', 'for', 'func', 'if',\
  'repeat', 'switch', 'while'],					/# BOL indenting next (strings and/or keywords) #/\
 ['break', 'continue', 'fallthrough', 'return']]		/# BOL outdenting next (keywords) #/\
 }
