# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# msInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for MScript language.  Returns registration array as described in lang.ms.

{'LibName' => 'MScript',					/# Library name (if different from mode name) #/\
 'ModeName' => 'MScript',					/# Mode name #/\
 'ModeDesc' => 'MScript auto-formatting.',			/# Mode description #/\
 'LibFile' => 'msLib',						/# "Lib" filename without extension #/\
 'FileExts' => ['ms', 'mm'],					/# Array of extensions used for source files #/\
 'BinNames' => ['memacs', 'mm'],				/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence1',					/# Fence mode to activate #/\
 'LineComment' => '#',						/# Line comment symbol (string) #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => nil,						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [['else', 'elsif', 'endif', 'endloop',\
 'endroutine'],							/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [nil,						/# EOL indenting next (chars, strings, and/or keywords) #/\
 nil],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
\
	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [['command', 'if', 'else', 'elsif', 'for',\
  'function', 'loop', 'until', 'while'],			/# BOL indenting next (strings and/or keywords) #/\
 ['break', 'next', 'return']],					/# BOL outdenting next (keywords) #/\
	/# Block Boundary Parameters #/\
 'BeginBlkBOL' => {'command' => nil, 'function' => nil,\
  'for' => nil, 'loop' => nil, 'while' => nil, 'until' => nil,\
  'if' => nil},							/# Begin-block symbols at beginning of line #/\
 'BeginBlkEOL' => nil,						/# Begin-block symbols at end of line #/\
 'EndBlkBOL' => {'endroutine' => nil, 'endloop' => nil,\
  'endif' => nil}						/# End-block symbols at beginning of line #/\
 }
