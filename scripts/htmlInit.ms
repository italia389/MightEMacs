# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# htmlInit.ms		Ver. 9.6.0
#	MightEMacs initialization file for HTML.  Returns registration array as described in lang.ms.

[nil,								/# Library name (if different from mode name) #/\
 'HTML',							/# Mode name #/\
 'HTML auto-formatting.',					/# Mode description #/\
 'html',							/# Routine and library name prefix #/\
 ['html', 'htm'],						/# Array of extensions used for source files #/\
 nil,								/# Unix binary name, or array of names #/\
 'Fence2',							/# Fence mode to activate #/\
	/# preKey hook triggers #/\
 [nil,								/# Left justification (chars) #/\
 [?}, '</body', '</dl', '</div', '</form', '</h1', '</h2',\
 '</h3', '</h4', '</h5', '</head', '</ol', '</pre', '</style',\
 '</table', '</tr', '</ul'],					/# Outdenting (chars and/or strings) #/\
 nil],								/# Same-denting (strings) #/\
	/# newline triggers #/\
 [[?{],								/# EOL indenting next (chars, strings, and/or keywords) #/\
 [?}],								/# EOL outdenting next (chars, strings, and/or keywords) #/\
 ['<body', '<dl', '<div', '<form', '<h1', '<h2', '<h3', '<h4',\
 '<h5', '<head', '<ol', '<pre', '<style', '<table', '<tr',\
 '<ul'],							/# BOL indenting next (strings and/or keywords) #/\
 nil]								/# BOL outdenting next (keywords) #/\
 ]
