# blockFormat.mm	Ver. 8.2.0
#	Macros for formatting comment blocks and numbered item lists.

# Don't load this file if it has already been done so.
if defined? 'bfRephrase'
	return
endif

# Renumber phrases within narrowed region.  It is assumed that the buffer is already narrowed to the line group.  Return with
# cursor at beginning of buffer.
macro bfRephrase,0
	PhrasePrefixRE = ' *\(\d+\),:r'
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Renumber phrases within item, if any.
	beginBuf
	seti 1,1,'%u'
	$search = PhrasePrefixRE
	loop
		if huntForw == false
			break
		endif
		2 => backChar
		-1 => deleteWord
		inserti
	endloop
	beginBuf
	formerMsgState => alterGlobalMode 'msg'
endmacro

# Format a list item, given name of scratch buffer (or nil).  It is assumed that the current buffer is already narrowed to the
# list item.  Return new number of lines in list item with buffer widened and cursor at beginning of line after item.
macro bfFormatOneItem,1
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Consider list item as a sequence of one or more partial blocks, each of which consists of two elements: (1), one or
	# more lines of a "paragraph"; and (2), a bulleted list (which may be null).  The first paragraph of the first partial
	# block begins with an item number.  Subsequent paragraphs (which follow a bulleted list) are identified by locating a
	# line which does not begin with white space.

	# Set up and loop through partial blocks.
	searchForw '. :pe'				# Save item number text...
	itemNum = subLine -(itemNumLen = $lineOffset),$lineOffset
	itemWhite = sprintf '%*s',itemNumLen,''
	0 => deleteToBreak				# and delete it.
	deleteWhite
	bufName = $bufName				# Remember this buffer.
	workBuf = nil?($1) ? 0 => scratchBuf : $1	# Get a scratch buffer...
	1 => clearBuf workBuf				# clear it...
	selectBuf workBuf				# switch to it...
	0 => insertBuf bufName				# and insert item text.
	loop
		insert itemWhite			# Ensure first line of paragraph begins with "itemNumLen" spaces.

		# Search forward for a bulleted line.
		beginLine
		setMark
		loop
			2 => beginText
			if 1 => bufBound?		# Hit end of buffer?
				break			# Yes.
			elsif $lineOffset > 0		# No, has leading white space?
				list = $BulletList	# Yes, check if it begins with a bullet.
				until nil?(bulletPrefix = shift list,"\t")
					prefixLen = length(bulletPrefix)
					if subLine(0,prefixLen + 1) =~ sprintf('%s[ \t]',bulletPrefix)
						break 2
					endif
				endloop
			endif
		endloop

		# Now at line just past paragraph.  Fill paragraph.
		-1 => endLine
		0 => narrowBuf
		bfRephrase				# Renumber phrases within item, if any.
		999999999 => wrapLine nil,$EndSentence	# Rewrap all visible lines.

		# At end of buffer?
		widenBuf
		if 1 => bufBound?
			break
		endif

		# Not at end of buffer ... process bulleted list.
		beginText
		bulletOffset = $lineOffset
		prefixLen => overwrite ' '		# Overwrite bullet with space(s).
		forwChar				# Grab indentation.
		bulletIndent = subLine(-$lineOffset,$lineOffset)
		$lineOffset = bulletOffset

		# Find and process next bullet.
		loop
			prefixLen => deleteForwChar	# Delete bullet.
			deleteWhite
			0 => insert bulletIndent
			setMark
			loop
				2 => beginText

				# Stop if hit end of buffer, have no leading white space, or hit another bullet.
				if 1 => bufBound? || $lineOffset == 0 || subLine(0,prefixLen) == bulletPrefix
					break
				endif
			endloop

			# Now at next bulleted item or end of buffer.
			-1 => endLine
			0 => narrowBuf
			999999999 => wrapLine nil,$EndSentence	# Rewrap all visible lines.
			beginBuf
			bulletOffset => forwChar
			0 => overwrite bulletPrefix	# Restore bullet on first line.

			# All bullets processed?
			endBuf
			widenBuf
			if 1 => bufBound?
				break 2
			endif
			beginText
			if subLine(0,prefixLen) != bulletPrefix
				break
			endif
		endloop

		# All bullets processed.  Do next partial block.
	endloop

	# Finished processing list item.
	beginBuf
	0 => overwrite itemNum				# Restore item number text.
	endBuf
	bufLines = $bufLineNum - 1
	selectBuf bufName				# Return to original (narrowed) buffer.
	-1 => clearBuf
	insertBuf workBuf
	widenBuf
	nil?($1) && 1 => deleteBuf workBuf

	formerMsgState => alterGlobalMode 'msg'
	bufLines
endmacro

# Narrow buffer to one list item, given (1), item prefix search pattern; and (2), blank line search pattern.  Cursor is assumed
# to be on first line.
macro bfNarrowItem,2
	formerMsgState = -1 => alterGlobalMode 'msg'

	# Find last line of item.
	beginLine
	setMark
	endLine
	if searchForw($1) == false			# Found next prefix?
		if searchForw($2) == false		# Nope.  Found a blank line?
			endBuf				# Nope.  Move to end of buffer...
			backLine			# and back up one line.
		else
			2 => backLine			# Yes, back up two lines.
		endif
		endLine
		0 => narrowBuf				# Narrow to region.
	else						# Found next item prefix.
		backLine				# Back up one line ...
		endLine
		0 => narrowBuf				# narrow to region ...
		if searchForw($2) != false		# and search for a blank line.  Found one?
			widenBuf			# Yes, re-narrow.
			2 => backLine
			endLine
			0 => narrowBuf
		endif
	endif
	formerMsgState => alterGlobalMode 'msg'
endmacro

# Reset some system variables and return a status and message, given (1), old $search value; (2), success? flag; and (3), return
# message.
macro bfCleanup,3
	$search = $1
	($2 ? 1 : -1) => notice $3
endmacro

# Format a block of comment lines or one list item.  Cursor must be within line group.  Use $CommentList to locate comment
# lines and $EndSentence to control spacing when lines are joined.
macro bfFormatItem,0

	# Check wrap column.
	if $wrapCol == 0
		return -1 => notice 'Wrap column not set'
	endif

	search = $search				# Save search string.
	rtnMsg = ''

	# Current line a comment (prefix in $CommentList)?
	beginText
	list = $CommentList
	until nil?(commentPrefix = shift list,"\t")
		prefixLen = length commentPrefix
		if subLine(0,prefixLen) == commentPrefix
			break
		endif
	endloop
	if !nil? commentPrefix

		# Prefix found.  Find beginning of comment line block.
		while $bufLineNum > 1
			backLine
			beginText
			if subLine(0,prefixLen) != commentPrefix
				forwLine
				break
			endif
		endloop

		# Save window line number and check if past wrap column.
		windLineNum = $windLineNum
		beginText
		if $lineCol + prefixLen + 1 >= $wrapCol
		    return bfCleanup search,false,"Wrap column (#{$wrapCol}) too far left for this comment block"
		endif

		# Find end of comment block.
		endLine
		setMark
		loop
			forwLine
			beginText
			if subLine(0,prefixLen) != commentPrefix
				backLine
				beginLine
				break
			endif
		endloop

		# Narrow buffer and renumber phrases, if any.
		0 => narrowBuf
		bfRephrase

		# Rewrap line block.
		999999999 => wrapLine commentPrefix & ' ',$EndSentence
		bufLines = $bufLineNum - 1
		widenBuf
		rtnMsg = 'Block formatted'
	else
		# Not a comment line, check for a list item.
		rtnMsg = 'Not in a comment or list item'
		if $lineChar == "\r"				# Current line blank?
			return bfCleanup search,false,rtnMsg	# Yep, error.
		endif

		ItemPrefixRE = '^ *\d+\. +.:r'			# Item assumed.  Find first line.
		BlankLineRE = '\r[ \t]*\r:r'
		endLine
		setMark						# Save current position.
		if searchBack(ItemPrefixRE) == false		# Item prefix found searching backward?
			gotoMark ' '				# Nope.  It's a bust.
			return bfCleanup search,false,rtnMsg
		endif

		windLineNum = $windLineNum			# Check if passed over a blank line.
		0 => narrowBuf
		if searchForw(BlankLineRE) != false		# Found one?
			widenBuf				# Yep.  It's a bust.
			gotoMark ' '
			return bfCleanup search,false,rtnMsg
		endif

		widenBuf					# It's a go.
		bfNarrowItem ItemPrefixRE,BlankLineRE		# Narrow buffer to this item ...
		bufLines = bfFormatOneItem nil			# and format it.
		rtnMsg = 'Item formatted'
	endif

	# Clean up.
	windLineNum + bufLines => redrawScreen
	bfCleanup search,true,rtnMsg
endmacro

# Format an ordered list, which consists of one or more contiguous item blocks.  An item block begins with a line that matches
# the following regular expression:
#	/^ *\d+\. +./
# and ends at the next item block, a blank line, or end-of-buffer.  An item block may itself be an ordered list, which consists
# of one or more phrases that begin with the following regular expression:
#	/ +\(\d+\), */
# and end with a semicolon or period.  All text in item blocks above and below the cursor is wrapped as needed and blocks and
# phrases are renumbered beginning with 1 by default.  If a non-negative n argument if given, it is used as the number of the
# first item block.
macro bfFormatList,0

	# Check wrap column, current line, and n argument.
	if $wrapCol == 0
		return -1 => notice 'Wrap column not set'
	endif
	beginText
	if $lineChar == "\r"
		return -1 => notice 'Not in a list'
	endif
	if $0 == defn
		$0 = 1
	elsif $0 < 0
		return -1 => notice 'First item number (%d) must be 0 or greater' % $0
	endif

	ItemPrefixRE = '^ *\d+\. +.:r'
	BlankLineRE = '\r[ \t]*\r:r'

	# Find top of item list.
	lineNum = $bufLineNum				# Save current line number...
	search = $search				# and current search string.
	if searchBack(BlankLineRE) == false		# Blank line found searching backward?
		beginBuf				# No, move to first line of buffer.
	else
		2 => forwLine				# Yes, move to first possible item line.
	endif

	# Check if current line or next line is the beginning of an item block.
	2 => narrowBuf
	if searchForw(ItemPrefixRE) == false		# Item prefix found searching forward?
		widenBuf				# Not found ... it's a bust.
		$bufLineNum = lineNum
		return bfCleanup search,false,'Not in a list'
	endif

	# First line of first item found.  Begin formatting loop.
	widenBuf
	itemNum = $0
	workBuf = 0 => scratchBuf
	loop
		# Cursor is at end of prefix.
		backChar				# Back up one character ...
		0 => deleteToBreak			# and nuke everything before it.
		seti(itemNum++,1,'%2u. ')
		inserti					# Generate new prefix.
		bfNarrowItem ItemPrefixRE,BlankLineRE	# Narrow buffer to this item ...
		bfFormatOneItem workBuf			# and format it.

		if $lineChar == "\r"			# Now on blank line?
			break				# Yes, we're done.
		elsif $lineText !~ ItemPrefixRE		# No, next item?
			break				# No, we're done.
		endif
		length(match 0) => forwChar		# Yes, continue.
	endloop

	# Clean up.
	1 => deleteBuf workBuf
	bfCleanup search,true,'List formatted'
endmacro

##### Bindings and aliases #####
bindKey 'C-h 1',bfFormatItem
bindKey 'C-h ,',bfFormatList
