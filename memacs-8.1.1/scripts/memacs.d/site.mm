# site.mm	Ver. 8.0.0.2
#	MightEMacs user-customizable site-wide startup file.

# This file is loaded by the "memacs.mm" site startup file.  It contains site-wide settings and user-defined macros.
$CommentList = "#\t//"		# Tab-delimited list of single-line comment prefixes recognized by the formatItem macro in the
				# 'blockFormat' package.
$EndSentence = '.?!'		# End-of-sentence characters.  Set to nil if extra space not wanted when lines are joined.
				# Used by wpJoinLines and wpWrapLine macros (defined in "memacs.mm" if this variable is set).
#$ExtraIndent = $ModeShell | $ModeC | $ModePerl
				# Language modes where extra indentation of closing fence ('}') is desired (comment out or set
				# to zero for none).  Used by initLang macro, which enables 'xindt' buffer mode accordingly.

# System variables.  Uncomment any of the following lines to change the default.
#$fencePause = 30		# Centiseconds to pause when displaying matching fence (default 26).
#$horzJump = 15			# Percentage of window to jump horizontally when scrolling long lines (when in 'hscrl' mode;
				# default 1 for smooth scrolling).
#$vertJump = 30			# Percentage of window to jump vertically when scrolling (default 0 for smooth scrolling).
#$searchDelim = '^M'		# Use CR to terminate search strings (when interactive; default ESC).
#$TermCols => setWrapCol		# Set wrap column to full terminal width (default 74, 0 to disable).

# Global modes.  Uncomment any of the following lines to change the default.
#-1 => alterGlobalMode 'rd1st'	# DO NOT read first file at startup.
#1 => alterGlobalMode 'asave'	# Auto-save files.
#1 => alterGlobalMode 'bak'	# Create backup file when saving.
#-1 => alterGlobalMode 'exact'	# Searches are NOT case-sensitive.
#-1 => alterGlobalMode 'hscrl'	# DO NOT horizontally scroll all window lines simultaneously.
#-1 => alterGlobalMode 'esc8'	# DO NOT escape 8-bit characters.
#1 => alterGlobalMode 'safe'	# Copy buffer to temporary file when saving.
#1 => alterGlobalMode 'clob'	# Allow (silent) overwrite of a macro buffer.
#1 => alterGlobalMode 'line'	# Display line position of point on current mode line.
#1 => alterGlobalMode 'col'	# Display column position of point on current mode line.

# Optional packages.  Uncomment any of the following lines to load.
#require 'blockFormat'		# Package for formatting comment blocks and numbered item lists.
#require 'keyMacro'		# Package for naming and saving keyboard macros.
