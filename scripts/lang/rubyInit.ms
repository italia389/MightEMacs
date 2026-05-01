# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# rubyInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for Ruby scripting language.  Returns registration array as described in lang.ms.

{'LibName' => 'Ruby',						/# Library name (if different than mode name) #/\
 'ModeName' => 'Ruby',						/# Mode name #/\
 'ModeDesc' => 'Ruby script auto-formatting.',			/# Mode description #/\
 'LibFile' => 'rubyLib',					/# "Lib" filename without extension #/\
 'FileExts' => ['rb'],						/# Array of extensions used for source files #/\
 'BinNames' => 'ruby',						/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence1',					/# Fence mode to activate #/\
 'LineComment' => '#',						/# Line comment symbol (string) #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => '=',						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [[?}, ?], 'end', 'else', 'elsif', 'ensure',\
 'rescue', ['when', 'case']],					/# Outdenting (chars and/or strings) #/\
 ['then']],							/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [[?{, ?[, ?|, 'do', 'then'],			/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}, ?]]],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
\
	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [['begin', 'class', 'def', 'else', 'elsif',\
  'ensure', 'for', 'if', 'loop', 'module', 'rescue', 'unless',\
  'until', 'when', 'while'],					/# BOL indenting next (strings and/or keywords) #/\
 ['break', 'next', 'return']],					/# BOL outdenting next (keywords) #/\
	/# Block Boundary Parameters #/\
 'BeginBlkBOL' => {'begin' => nil, 'case' => nil, 'class' => nil,\
  'def' => nil, 'for' => nil, 'if' => nil, 'loop' => nil,\
  'module' => nil, 'unless' => nil, 'until' => nil,\
  'while' => nil},						/# Begin-block symbols at beginning of line #/\
 'BeginBlkEOL' => {'|' => nil, 'do' => nil},			/# Begin-block symbols at end of line #/\
 'EndBlkBOL' => {'end' => nil}					/# End-block symbols at beginning of line #/\
 }
