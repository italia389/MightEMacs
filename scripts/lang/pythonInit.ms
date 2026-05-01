# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# pythonInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for Python scripting language.  Returns registration array as described in lang.ms.

{'LibName' => 'Python',						/# Library name (if different from mode name) #/\
 'ModeName' => 'Python',					/# Mode name #/\
 'ModeDesc' => 'Python script auto-formatting.',		/# Mode description #/\
 'LibFile' => 'pythonLib',					/# "Lib" filename without extension #/\
 'FileExts' => ['py'],						/# Array of extensions used for source files #/\
 'BinNames' => 'python',					/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence1',					/# Fence mode to activate #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => nil,						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [[?}, ?], ?), 'elif', 'else', 'except',\
 'finally'],							/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [[?:, ?{, ?[],					/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}, ?], ?)]],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
\
	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [['class', 'def', 'else', 'elif', 'finally',\
  'if', 'for', 'while'],					/# BOL indenting next (strings and/or keywords) #/\
 ['break', 'continue', 'pass', 'return']]			/# BOL outdenting next (keywords) #/\
 }
