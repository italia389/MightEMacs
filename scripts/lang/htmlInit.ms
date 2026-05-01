# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# htmlInit.ms		Ver. 10.0.0
#	MightEMacs initialization file for HTML.  Returns registration array as described in lang.ms.

{'LibName' => 'HTML',						/# Library name (if different from mode name) #/\
 'ModeName' => 'HTML',						/# Mode name #/\
 'ModeDesc' => 'HTML auto-formatting.',				/# Mode description #/\
 'LibFile' => 'htmlLib',					/# "Lib" filename without extension #/\
 'FileExts' => ['html', 'htm'],					/# Array of extensions used for source files #/\
 'BinNames' => nil,						/# Unix binary name, or array of names #/\
 'FenceMode' => 'Fence2',					/# Fence mode to activate #/\
	/# Auto-Formatting Parameters #/\
 'LeftJust' => nil,						/# Left justification (chars) #/\
\
	/# preKey hook triggers #/\
 'PreKeyTrigs' => [[?}, '</body', '</dl', '</div', '</form',\
 '</h1', '</h2', '</h3', '</h4', '</h5', '</head', '</ol',\
 '</pre', '</style', '</table', '</tr', '</ul'],		/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
\
	/# newline triggers at end of line #/\
 'NLTrigsEOL' => [[?{],						/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}]],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 	/# newline triggers at beginning of line #/\
 'NLTrigsBOL' => [['<body', '<dl', '<div', '<form', '<h1',\
 '<h2', '<h3', '<h4', '<h5', '<head', '<ol', '<pre', '<style',\
 '<table', '<tr', '<ul'],					/# BOL indenting next (strings and/or keywords) #/\
 nil]								/# BOL outdenting next (keywords) #/\
 }
