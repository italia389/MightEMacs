# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# perlInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for Perl scripting language.  Returns registration array as described in lang.ms.

{'LibName' => 'Perl',						/# Library name (if different from mode name) #/\
 'ModeName' => 'Perl',						/# Mode name #/\
 'ModeDesc' => 'Perl script auto-formatting.',			/# Mode description #/\
 'LibFile' => 'perlLib',					/# "Lib" filename without extension #/\
 'FileExts' => ['pl'],						/# Array of extensions used for source files #/\
 'BinNames' => 'perl',						/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence1',					/# Fence mode to activate #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => '=',						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [[?}, ?), ?], 'continue', ['else', 'case'],\
 'elsif'],							/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [[?{, ?(, ?[],					/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}, ?), ?]]],							/# EOL outdenting next (chars, strings, and/or keywords) #/\
\
	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [['continue', 'do', 'if', 'else', 'elsif',\
  'for', 'foreach', 'sub', 'unless', 'until', 'while'],		/# BOL indenting next (strings and/or keywords) #/\
 ['return']]							/# BOL outdenting next (keywords) #/\
 }
