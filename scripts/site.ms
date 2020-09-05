# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# site.ms	Ver. 9.5.0
#	MightEMacs user-customizable site-wide startup file.

# This file is loaded by the "memacs.ms" main startup file.  It contains site-wide settings which may be customized by the user.
$BulletList = ['*', '-']	# List of bullet prefixes recognized by the bfFormatItem (ESC .) command in the 'blockFormat'
				# package.
$CommentList = ['#', '//']	# List of single-line comment prefixes recognized by the bfFormatItem command in the
				# 'blockFormat' package.
$EndSentence = '.?!'		# End-of-sentence characters.  Set to '' if extra space not wanted when lines are joined.
				# Used by wpJoinLines (C-c C-j) and wpWrapLine (C-c RTN) commands (defined in "memacs.ms" if
				# this variable is not nil or null).  Also used by the bfFormatItem command in the 'blockFormat'
				# package.

# Uncomment the following line to enable Ratliff-style indentation in all programming languages that use left and right braces
# to enclose a block of lines.  In this style, the right brace '}' has the same indentation as the nested line block it
# terminates instead of being aligned with the statement header.  This also enables the same indentation style for the right
# paren ')' and right bracket ']' when it is entered on a line by itself.
#1 => chgMode nil, 'RFIndt'

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
setWrapCol $TermSize[0]		# Set wrap column to full terminal width (default 76, 0 to disable).

# Global modes.  Uncomment any of the following lines to change the default.
#1 => chgMode nil, 'ASave'	# Auto-save files.
#-1 => chgMode nil, 'Exact'	# Searches are NOT case-sensitive by default.
#-1 => chgMode nil, 'HScrl'	# Do NOT horizontally scroll all window lines simultaneously.
#1 => chgMode nil, 'Safe'	# Copy buffer to temporary file when saving.
#1 => chgMode nil, 'Clob'	# Allow (silent) overwrite of a user command or function buffer.

# Optional packages.  Uncomment any of the following lines to load.
require 'blockFormat'		# Package for formatting comment blocks and numbered item lists.
#require 'macroUtil'		# Package for saving and restoring macros from a file.
