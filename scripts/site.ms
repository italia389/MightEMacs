# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# site.ms	Ver. 10.0.0
#	MightEMacs user-customizable site-wide startup file.

# This file is loaded by the "memacs.ms" main startup file.  It contains site-wide settings which may be customized by the user.
EndSentence = '.?!'		# End-of-sentence characters.  Set to '' if extra space not wanted when lines are joined.
				# Used by wpJoinLines (C-c C-j) and wpWrapLine (C-c RTN) commands (defined in "memacs.ms" if
				# this variable is not nil or null).  Also used by the tfFormatItem (ESC .) command in the
				# 'textFormat' package.

# Uncomment the following line to enable Ratliff-style indentation in all programming languages that use left and right braces
# to enclose a block of lines.  In this style, the right brace '}' has the same indentation as the nested line block it
# terminates instead of being aligned with the statement header.  This also enables the same indentation style for the right
# paren ')' and right bracket ']' when it is entered on a line by itself.
#1 => chgMode 'RFIndt'

# Uncomment any or all of the following lines to disable color.
#setDispColor 'Info', nil	# Disable colored informational displays (revert to bold text).
#setDispColor 'ModeLine', nil	# Disable colored mode lines (revert to reverse video).
#setDispColor 'Record', nil	# Disable colored macro recording indicator (revert to bold text).

# System variables.  Uncomment any of the following lines to change the default.
#$softTabSize = 4		# Enable soft tabs every four columns (default hard tabs).
#$fencePause = 30		# Centiseconds to pause when displaying matching fence (default 26).
#$horzJump = 0			# Set horizontal smooth scrolling (default 15% of window width).
#$vertJump = 0			# Set vertical smooth scrolling (default 20% of window height).
#$searchDelim = 'RTN'		# Use "return" to terminate search strings interactively (default ESC).

# Wrap column.  Uncomment the following line to set wrap column to full terminal width on first screen
# and any subsequent screens.  Default wrap column is "terminal width - 4".  Set to zero to disable.
#setDefault 'WrapCol', setWrapCol(TermSize[0])

# Global modes.  Uncomment any of the following lines to change the default.
#1 => chgMode 'ASave'		# Auto-save files.
#1 => chgMode 'BufCount'		# Display count of visible buffers on bottom mode line.
#-1 => chgMode 'Exact'		# Searches are NOT case-sensitive by default.
#-1 => chgMode 'HScrl'		# Do NOT horizontally scroll all window lines simultaneously.
#1 => chgMode 'Safe'		# Copy buffer to temporary file when saving.
#1 => chgMode 'Clob'		# Allow user commands and functions to be redefined.

# Uncomment the following lines to make 'ATerm' a global mode and enable it.
# This ensures that all files end with a newline when saved.
#editMode 'ATerm', 'Global: true', 'Hidden: true'
#1 => chgMode 'ATerm'

# Optional packages.  Uncomment any of the following lines to load.
require 'textFormat', ['#', '//'], '-*.'
				# Package for formatting comment and plain text blocks, numbered item lists, and bulleted lists.
				# Package arguments are (1), array of single-line comment prefixes; and (2), string list of
				# bullet characters, which comprise text characters that should be recognized by the
				# tfFormatItem command in the package when formatting text blocks.
#require 'macroUtil'		# Package for saving and restoring macros from a file.
