# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# site.mm	Ver. 9.1.0
#	MightEMacs user-customizable site-wide startup file.

# This file is loaded by the "memacs.mm" main startup file.  It contains site-wide settings which may be customized by the user.
$BulletList = ['*','-']		# List of bullet prefixes recognized by the bfFormatItem (ESC .) macro in the 'blockFormat'
				# package.
$CommentList = ['#','//']	# List of single-line comment prefixes recognized by the bfFormatItem macro in the 'blockFormat'
				# package.
$EndSentence = '.?!'		# End-of-sentence characters.  Set to '' if extra space not wanted when lines are joined.
				# Used by wpJoinLines (C-c C-j) and wpWrapLine (C-c RTN) macros (defined in "memacs.mm" if this
				# variable is not nil or null).  Also used by the bfFormatItem macro in the 'blockFormat'
				# package.

# Uncomment the following two lines to enable Ratliff-style indentation in all programming languages that use left and right
# braces to enclose a block of lines.  In this style, the right brace '}' has the same indentation as the nested line block it
# terminates instead of being aligned with the statement header.  This also enables the same indentation style for the right
# paren ')' and right bracket ']' when it is entered on a line by itself.
#1 => editMode 'RFIndt','description: Use Ratliff-style indentation of right fence character.','global: true','hidden: true'
#1 => chgGlobalMode 'RFIndt'

# System variables.  Uncomment any of the following lines to change the default.
#$softTabSize = 4		# Enable soft tabs every four columns (default hard tabs).
#$fencePause = 30		# Centiseconds to pause when displaying matching fence (default 26).
#$horzJump = 15			# Percentage of window to jump horizontally when scrolling long lines (when in 'HScrl' mode;
				# default 0 for smooth scrolling).
#$vertJump = 30			# Percentage of window to jump vertically when scrolling (default 0 for smooth scrolling).
#$searchDelim = 'RTN'		# Use "return" to terminate search strings interactively (default ESC).
setWrapCol $TermCols		# Set wrap column to full terminal width (default 76, 0 to disable).

# Global modes.  Uncomment any of the following lines to change the default.
#1 => chgGlobalMode 'ASave'	# Auto-save files.
#-1 => chgGlobalMode 'Exact'	# Searches are NOT case-sensitive by default.
#-1 => chgGlobalMode 'HScrl'	# Do NOT horizontally scroll all window lines simultaneously.
#1 => chgGlobalMode 'Safe'	# Copy buffer to temporary file when saving.
#1 => chgGlobalMode 'Clob'	# Allow (silent) overwrite of a macro buffer.

# Optional packages.  Uncomment any of the following lines to load.
require 'blockFormat'		# Package for formatting comment blocks and numbered item lists.
#require 'keyMacro'		# Package for naming and saving keyboard macros.
