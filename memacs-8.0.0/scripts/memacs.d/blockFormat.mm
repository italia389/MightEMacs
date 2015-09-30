# blockFormat.mm	Ver. 8.0.0.0
#	Macros for formatting comment blocks and numbered item lists.

# Don't load this file if it has already been done so.
!if defined? 'bfRephrase'
	!return
!endif

# Renumber phrases within narrowed region.  It is assumed that (1), the buffer is already narrowed to the line group; and (2),
# Regexp mode is set.  Return with cursor at beginning of buffer.
!macro bfRephrase,0
	PhrasePrefixRE = ' *\([0-9]+\),'
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Renumber phrases within item, if any.
	beginBuf
	seti 1,1,'%u'
	$search = PhrasePrefixRE
	!loop
		!if huntForw == false
			!break
		!endif
		2 => backChar
		-1 => deleteWord
		inserti
	!endloop
	beginBuf
	formerMsgState => alterGlobalMode 'msg'
!endmacro

# Format a list item.  It is assumed that (1), the buffer is already narrowed to the list item; and (2), Regexp mode is set.
# Return with buffer widened and cursor at beginning of line after list item.
!macro bfFormatOneItem,0
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Renumber phrases within item, if any.
	bfRephrase

	# Fill item block.
	itemNum = subLine 0,4			# Save item number text.
	0 => overwrite '    '			# Replace it with spaces.
	999999999 => wrapLine nil,'.?!'		# Rewrap all visible lines.
	beginBuf
	0 => overwrite itemNum			# Restore item number text.
	endBuf
	widenBuf
	formerMsgState => alterGlobalMode 'msg'
!endmacro

# Narrow buffer to one list item, given (1), item prefix RE; and (2), blank line RE.  Cursor is assumed to be on first line.
!macro bfNarrowItem,2
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Find last line of item.
	beginLine
	setMark
	endLine
	!if searchForw($1) == false		# Found next prefix?
		!if searchForw($2) == false	# Nope.  Found a blank line?
			endBuf			# Nope.  Move to end of buffer.
		!endif
		backLine			# Back up one line ...
		endLine
		0 => narrowBuf			# and narrow region.
	!else					# Found next item prefix.
		backLine			# Back up one line ...
		endLine
		0 => narrowBuf			# narrow region ...
		!if searchForw($2) != false	# and search for a blank line.  Found one?
			widenBuf		# Yes, re-narrow.
			backLine
			endLine
			0 => narrowBuf
		!endif
	!endif
	formerMsgState => alterGlobalMode 'msg'
!endmacro

# Reset some system variables and return a status and message, given (1), old $search value; (2), old $globalModes value; (3),
# success? flag; and (4), return message.
!macro bfCleanup,4
	$search = $1
	$globalModes = $2
	($3 ? 1 : -1) => notice $4
!endmacro

# Format a block of comment lines or one list item.  Cursor must be within line group.  Use $CommentList to locate comment
# lines and $EndSentence to control spacing when lines are joined.
!macro bfFormatItem,0

	# Check wrap column.
	!if $wrapCol == 0
		!return -1 => notice 'Wrap column not set'
	!endif

	search = $search			# Save search string ...
	globalModes = $globalModes		# and global modes.
	rtnMsg = ''

	# Current line a comment (prefix in $CommentList)?
	beginText
	list = $CommentList
	!until nil?(commentPrefix = shift list,"\t")
		prefixLen = length commentPrefix
		!if subLine(0,prefixLen) == commentPrefix
			!break
		!endif
	!endloop
	!if !nil? commentPrefix

		# Prefix found.  Find beginning of comment line block.
		!while $bufLineNum > 1
			backLine
			beginText
			!if subLine(0,prefixLen) != commentPrefix
				forwLine
				!break
			!endif
		!endloop

		# Get indentation from first line and check if past wrap column.
		beginText
		!if $lineCol + prefixLen + 1 >= $wrapCol
		    !return bfCleanup search,globalModes,false,"Wrap column (#{$wrapCol}) too far left for this comment block"
		!endif
		indent = subLine -$lineOffset,$lineOffset

		# Find end of comment block.
		endLine
		setMark
		!loop
			forwLine
			beginText
			!if subLine(0,prefixLen) != commentPrefix
				backLine
				beginLine
				!break
			!endif
		!endloop

		# Narrow buffer and renumber phrases, if any.
		0 => narrowBuf
		1 => alterGlobalMode 'regexp'
		bfRephrase

		# Rewrap line block.
		999999999 => wrapLine commentPrefix & ' ',$EndSentence
		widenBuf
		rtnMsg = 'Block formatted'
	!else
		# Not a comment line, check for a list item.
		rtnMsg = 'Not in a comment or list item'
		!if $lineChar == 015				# Current line blank?
			!return bfCleanup search,globalModes,false,rtnMsg	# Yep, error.
		!endif

		ItemPrefixRE = '^ *[0-9]+\. +.'			# Item assumed.  Find first line.
		BlankLineRE = '^[ \t]*$'
		endLine
		setMark						# Save current position.
		1 => alterGlobalMode 'regexp'
		!if searchBack(ItemPrefixRE) == false		# Item prefix found searching backward?
			gotoMark				# Nope.  It's a bust.
			!return bfCleanup search,globalModes,false,rtnMsg
		!endif

		0 => narrowBuf					# Check if passed over a blank line.
		!if searchForw(BlankLineRE) != false		# Found one?
			widenBuf				# Yep.  It's a bust.
			gotoMark
			!return bfCleanup search,globalModes,false,rtnMsg
		!endif

		widenBuf					# It's a go.
		bfNarrowItem ItemPrefixRE,BlankLineRE		# Narrow buffer to this item ...
		bfFormatOneItem					# and format it.
		rtnMsg = 'Item formatted'
	!endif

	# Clean up.
	bfCleanup search,globalModes,true,rtnMsg
!endmacro

# Format an ordered list, which consists of one or more contiguous item blocks.  An item block begins with a line that matches
# the following regular expression:
#	/^ *[0-9]+\. +./
# and ends at the next item block, a blank line, or end-of-buffer.  An item block may itself be an ordered list, which consists
# of one or more phrases that begin with the following regular expression:
#	/ +\([0-9]+\), */
# and end with a semicolon or period.  All text in item blocks above and below the cursor is wrapped as needed and blocks and
# phrases are renumbered beginning with 1.
!macro bfFormatList,0

	# Check wrap column.
	!if $wrapCol == 0
		!return -1 => notice 'Wrap column not set'
	!endif

	# Current line blank?
	beginText
	!if $lineChar == 015
		!return -1 => notice 'Not in a list'
	!endif

	ItemPrefixRE = '^ *[0-9]+\. +.'
	BlankLineRE = '^[ \t]*$'

	# Find top of item list.
	lineNum = $bufLineNum				# Save current line ...
	search = $search				# current search string ...
	globalModes = $globalModes			# and global modes.
	1 => alterGlobalMode 'regexp'
	!if searchBack(BlankLineRE) == false		# Blank line found searching backward?
		beginBuf				# No, move to first line of buffer.
	!else
		forwLine				# Yes, move to first possible item line.
	!endif

	# Check if current line or next line is the beginning of an item block.
	2 => narrowBuf
	!if searchForw(ItemPrefixRE) == false		# Item prefix found searching forward?
		widenBuf				# Not found ... it's a bust.
		$bufLineNum = lineNum
		!return bfCleanup search,globalModes,false,'Not in a list'
	!endif

	# First line of first item found.  Begin formatting loop.
	widenBuf
	itemNum = 0
	!loop
		# Cursor is at end of prefix.
		backChar				# Back up one character ...
		0 => deleteToBreak			# and nuke everything before it.
		seti(++itemNum,1,'%2u. ')
		inserti					# Generate new prefix.
		bfNarrowItem ItemPrefixRE,BlankLineRE	# Narrow buffer to this item ...
		bfFormatOneItem				# and format it.

		!if $lineChar == 015			# Now on blank line?
			!break				# Yes, we're done.
		!endif
		searchForw ItemPrefixRE			# No, must be next item.
	!endloop

	# Clean up.
	bfCleanup search,globalModes,true,'List formatted'
!endmacro

##### Bindings and aliases #####
bindKey 'H-1',bfFormatItem
bindKey 'H-,',bfFormatList
