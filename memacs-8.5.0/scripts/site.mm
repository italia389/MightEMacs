# (c) Copyright 2017 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# site.mm	Ver. 8.5.0
#	MightEMacs user-customizable site-wide startup file.

# This file is loaded by the "memacs.mm" site startup file.  It contains site-wide settings and user-defined macros.
$BulletList = ['*','-']		# List of bullet prefixes recognized by the formatOneItem macro in the 'blockFormat' package.
$CommentList = ['#','//']	# List of single-line comment prefixes recognized by the formatItem macro in the 'blockFormat'
				# package.
$EndSentence = '.?!'		# End-of-sentence characters.  Set to '' if extra space not wanted when lines are joined.
				# Used by wpJoinLines and wpWrapLine macros (defined in "memacs.mm" if this variable is not
				# nil or null).  Also used by the formatItem macro in the 'blockFormat' package.
#$ExtraIndent = $ModeShell | $ModeC | $ModePerl
				# Language modes where extra indentation of closing fence ('}') is desired (comment out or set
				# to zero for none).  Used by initLang macro, which enables 'xindt' buffer mode accordingly.

# System variables.  Uncomment any of the following lines to change the default.
#$fencePause = 30		# Centiseconds to pause when displaying matching fence (default 26).
#$horzJump = 15			# Percentage of window to jump horizontally when scrolling long lines (when in 'hscrl' mode;
				# default 1 for smooth scrolling).
#$vertJump = 30			# Percentage of window to jump vertically when scrolling (default 0 for smooth scrolling).
#$searchDelim = 'RTN'		# Use "return" to terminate search strings interactively (default ESC).
#$TermCols => setWrapCol		# Set wrap column to full terminal width (default 74, 0 to disable).

# Global modes.  Uncomment any of the following lines to change the default.
#1 => alterGlobalMode 'asave'	# Auto-save files.
#1 => alterGlobalMode 'bak'	# Create backup file when saving.
#-1 => alterGlobalMode 'exact'	# Searches are NOT case-sensitive by default.
#-1 => alterGlobalMode 'hscrl'	# DO NOT horizontally scroll all window lines simultaneously.
#-1 => alterGlobalMode 'esc8'	# DO NOT escape 8-bit characters.
#1 => alterGlobalMode 'safe'	# Copy buffer to temporary file when saving.
#1 => alterGlobalMode 'clob'	# Allow (silent) overwrite of a macro buffer.
#1 => alterGlobalMode 'line'	# Display line position of point on current mode line.
#1 => alterGlobalMode 'col'	# Display column position of point on current mode line.

# Optional packages.  Uncomment any of the following lines to load.
#require 'blockFormat'		# Package for formatting comment blocks and numbered item lists.
#require 'keyMacro'		# Package for naming and saving keyboard macros.
