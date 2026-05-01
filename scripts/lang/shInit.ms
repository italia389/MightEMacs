# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# shInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for shell scripts.  Returns registration array as described in lang.ms.

{'LibName' => 'Shell',						/# Library name (if different from mode name) #/\
 'ModeName' => 'Shell',						/# Mode name #/\
 'ModeDesc' => 'Shell script auto-formatting.',			/# Mode description #/\
 'LibFile' => 'shLib',						/# "Lib" filename without extension #/\
 'FileExts' => ['sh'],						/# Array of extensions used for source files #/\
 'BinNames' => ['ksh', 'sh', 'zsh'],				/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence1',					/# Fence mode to activate #/\
 'LineComment' => '#',						/# Line comment symbol (string) #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => nil,						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [[?}, 'done', 'elif', 'else', 'esac', 'fi'],	/# Outdenting (chars and/or strings) #/\
 ['do', 'then']],						/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [[?{, ?), 'do', 'then'],			/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}, ';;']],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
\
	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [['elif', 'else', 'for', 'if', 'sub', 'while'], /# BOL indenting next (strings and/or keywords) #/\
 ['break', 'continue', 'return']],				/# BOL outdenting next (keywords) #/\
	/# Block Boundary Parameters #/\
 'BeginBlkBOL' => {'case' => nil, 'if' => nil, 'for' => nil,\
  'while' => nil},						/# Begin-block symbols at beginning of line #/\
 'BeginBlkEOL' => {'{' => nil},					/# Begin-block symbols at end of line #/\
 'EndBlkBOL' => {'done' => nil, 'esac' => nil, 'fi' => nil,\
  '}' => nil}							/# End-block symbols at beginning of line #/\
 }
