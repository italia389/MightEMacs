# (c) Copyright 2022 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# blockFormat.ms	Ver. 9.6.0
#	Routines for formatting comment blocks and numbered item lists.

# Don't load this file if it has already been done so.
if defined? 'Name', 'bfRephrase'
	return
endif

# Set global constants.
$BFInfo = ['^ *\d+\.[ \t]+[^ \t]:r', '^[ \t]*$:r', ' *\(\d+\),:r']	# ItemPrefixRE, BlankLineRE, PhrasePrefixRE.

##### Commands and Functions #####

# Renumber phrases within narrowed region.  It is assumed that the buffer is already narrowed to the line block.  Return with
# point at beginning of buffer.
function bfRephrase(0)
	PhrasePrefixRE = $BFInfo[2]

	# Renumber phrases within item, if any.
	beginBuf
	seti 1, '%u', 1
	$searchPat = PhrasePrefixRE
	loop
		if !huntForw
			break
		endif
		2 => backChar
		-1 => delWord
		inserti
	endloop
	beginBuf
endroutine

# Format a list item, given name of scratch buffer or nil.  It is assumed that the current buffer is already narrowed to the
# list item.  Return new number of lines in list item with buffer widened and point at end of last line.
function bfFormatOneItem(1)

	# Consider list item as a sequence of one or more partial blocks, each of which consists of two elements: (1), one or
	# more lines of a "paragraph"; and (2), a bulleted list (which may be null).  The first paragraph of the first partial
	# block begins with an item number.  Subsequent paragraphs (which follow a bulleted list) are identified by locating a
	# line which does not begin with white space.

	# Set up and loop through partial blocks.
	searchForw '. :pe'				# Save item number text...
	itemNum = subline(-(itemNumLen = $lineOffset), $lineOffset)
	itemWhite = sprintf('%*s', itemNumLen, '')
	0 => delToBreak					# and delete it.
	delWhite
	bufname = $bufname				# Remember this buffer.
	workBuf = nil?($1) ? (0 => scratchBuf)[0] : $1	# Get a scratch buffer...
	-1 => clearBuf workBuf				# clear it...
	selectBuf workBuf				# switch to it...
	bufAttr?('TermAttr', bufname) and\
	 1 => chgBufAttr($bufname, 'TermAttr')		# set 'TermAttr' if needed...
	0 => insertBuf bufname				# and insert item text.
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
				for bulletPrefix in $BulletList		# Yes, check if it begins with a bullet.
					prefixLen = length(bulletPrefix)
					if subline(0, prefixLen + 1) =~ sprintf('\%s[ \t]', bulletPrefix)
						-1 => endLine
						break 2
					endif
				endloop
			endif
		endloop

		# Now on last line of paragraph.  Fill paragraph and widen buffer.
		0 => narrowBuf
		bfRephrase				# Renumber phrases within item, if any.
		999999999 => wrapLine nil, $EndSentence	# Rewrap all visible lines.
		widenBuf

		# At end of buffer?
		if 1 => bufBound?
			break
		endif

		# Not at end of buffer... process bulleted list.
		2 => beginText
		bulletOffset = $lineOffset
		prefixLen => overwriteCol ' '		# Overwrite bullet with space(s).
		forwChar				# Grab indentation.
		bulletIndent = subline(-$lineOffset, $lineOffset)
		$lineOffset = bulletOffset

		# Find and process next bullet.
		loop
			prefixLen => delForwChar	# Delete bullet.
			delWhite
			0 => insert bulletIndent
			setMark
			loop
				2 => beginText

				# Stop if hit end of buffer, have no leading white space, or hit another bullet.
				if 1 => bufBound? || $lineOffset == 0 || subline(0, prefixLen) == bulletPrefix
					break
				endif
			endloop

			# Now at next bulleted item or end of buffer.
			1 => bufBound? or -1 => endLine
			0 => narrowBuf
			999999999 => wrapLine nil, $EndSentence	# Rewrap all visible lines.
			beginBuf
			bulletOffset => forwChar
			0 => overwriteCol bulletPrefix	# Restore bullet on first line.

			# All bullets processed?
			endBuf
			widenBuf
			if 1 => bufBound?
				break 2
			endif
			2 => beginText
			if subline(0, prefixLen) != bulletPrefix
				break
			endif
		endloop

		# All bullets processed.  Do next partial block.
		delWhite
	endloop

	# Finished processing list item.
	beginBuf
	0 => overwriteCol itemNum			# Restore item number text.
	endBuf
	bufLines = $bufLineNum
	selectBuf bufname				# Return to original (narrowed) buffer.
	-1 => clearBuf $bufname
	insertBuf workBuf
	widenBuf
	nil?($1) && -1 => delBuf workBuf

	bufLines
endroutine

# Narrow buffer to one list item, given (1), item prefix search pattern; and (2), blank line search pattern.  Point is assumed
# to be on first line.
function bfNarrowItem(2)

	# Find last line of item.
	beginLine
	setMark
	endLine
	if !searchForw($1)				# Found next prefix?
		if !searchForw($2)			# Nope.  Found a blank line?
			endBuf				# Nope.  Move to last line of buffer.
			$LineLen == 0 and backLine
		else
			backLine			# Yes, back up a line.
		endif
		endLine
		0 => narrowBuf				# Narrow to region.
	else						# Found next item prefix.
		backLine				# Back up one line...
		endLine
		0 => narrowBuf				# narrow to region...
		if searchForw($2)			# and search for a blank line.  Found one?
			widenBuf			# Yes, re-narrow.
			-1 => endLine
			0 => narrowBuf
		endif
	endif
endroutine

# Reset some system variables and return a status and message, given (1), old $searchPat value; (2), success? Boolean value;
# and (3), return message.
function bfCleanup(3)
	restoreSearchRing $1
	1 => message($2 ? 'High' : 'Fail, High', $3)
endroutine

# Format a block of comment lines, one list item, or a plain line block.  Point must be within line group.  $CommentList is used
# to locate comment lines and $EndSentence is used to control spacing when lines are joined.
command bfFormatItem(0) {description: join(nil, 'Format a block of comment lines, one list item, or a plain line block.  Point\
 must be within the line block.  If the first non-whitespace character(s) on the current line match any of the text strings in\
 the $CommentList global variable, the current line is assumed to be part of a comment block; otherwise, if the current line is\
 not blank and a backward search for the pattern "', $BFInfo[0], "\" succeeds without passing over a blank line, the line\
 matching the RE is assumed to be the first line of a numbered list item; otherwise, if a blank line is found either preceding\
 or following the current line, the current line is assumed to be part of a plain line block.\n\nAll lines in a comment block\
 or plain line block are rewrapped (using the $EndSentence global variable to control spacing between joined lines) so that\
 they all have the same indentation as the first line of the block but do not extend past the current wrap column set in\
 $wrapCol.  All lines in a numbered list item are similarly rewrapped so that the first line begins with \"nn. \" (where \"nn\"\
 is a one or two digit number) and all subsequent lines are indented by four spaces.  Additionally, any text within either a\
 comment block or numbered list item matching the pattern \"", $BFInfo[2], "\" is assumed to be a numbered \"phrase\", and all\
 such numbers are resequenced beginning at 1.\n\nLine blocks are bounded by beginning of buffer, end of buffer, a blank line,\
 or the first line of a numbered list item.")}

	# Check wrap column.
	if $wrapCol == 0
		return 1 => message 'Fail, High', 'Wrap column not set'
	endif

	# Check if buffer is narrowed.
	if bufAttr? 'Narrowed'
		return 1 => message 'Fail, High', 'Buffer is narrowed'
	endif

	searchPat = $searchPat				# Save current search pattern.
	rtnMsg = ''

	# Current line a comment (prefix in $CommentList)?
	beginText
	loop
		for commentPrefix in $CommentList
			prefixLen = length(commentPrefix)
			if subline(0, prefixLen) == commentPrefix

				# Prefix found.  Find beginning of comment line block.
				loop
					beginLine
					if -1 => bufBound?
						break
					endif
					-1 => beginText
					if subline(0, prefixLen) != commentPrefix
						forwLine
						break
					endif
				endloop

				# Save window line number and check if past wrap column.
				windLineNum = $windLineNum
				beginText
				if $lineCol + prefixLen + 1 >= $wrapCol
					return bfCleanup searchPat, false,\
					 "Wrap column (#{$wrapCol}) too far left for this comment block"
				endif

				# Find end of comment block.
				beginLine
				setMark
				loop
					endLine
					if 1 => bufBound?
						break
					endif
					2 => beginText
					if subline(0, prefixLen) != commentPrefix
						-1 => endLine
						break
					endif
				endloop

				# Narrow buffer and renumber phrases, if any.
				0 => narrowBuf
				bfRephrase

				# Rewrap line block.
				999999999 => wrapLine commentPrefix & ' ', $EndSentence
				bufLines = $bufLineNum
				widenBuf
				rtnMsg = 'Block formatted'
				break 2
			endif
		endloop

		# Not a comment line.  Scan backward for blank line, list item, or buffer boundary.
		ItemPrefixRE, BlankLineRE = $BFInfo
		setMark						# Save current position.
		beginLine
		firstLine = true
		loop
			if $lineText =~ ItemPrefixRE
				status = 1
			elsif $lineText =~ BlankLineRE
				status = 0
			elsif !backLine
				status = -1
			else
				firstLine = false
				next
			endif
			break
		endloop
		rtnMsg = 'Not in a line block, comment, or list item'
		if status <= 0					# Item prefix found searching backward?
			if status == 0				# Nope, assume plain line block.  Get bounds.
				if firstLine			# Current line blank?
					return bfCleanup searchPat, false, rtnMsg	# Yep, error.
				endif
				2 => beginLine			# Found prior blank line... it's a go.
				setMark
				searchForw(BlankLineRE)		# Can't fail (matches EOB).
				-1 => endLine
				0 => wrapLine nil, $EndSentence
			else					# No prior blank line.  Check forward.
				searchForw(BlankLineRE)		# Can't fail (matches EOB).
				if 1 => bufBound?
					gotoMark ?.		# Hit EOB.  It's a bust.
					return bfCleanup searchPat, false, rtnMsg
				endif
				-1 => beginLine			# Found blank line... it's a go.
				-999999999 => wrapLine nil, $EndSentence
			endif
			rtnMsg = 'Block formatted'
			bufLines = 0
			break
		endif

		windLineNum = $windLineNum			# Item prefix found.  It's a go.
		bfNarrowItem ItemPrefixRE, BlankLineRE		# Narrow buffer to this item...
		bufLines = bfFormatOneItem(nil)			# and format it.
		rtnMsg = 'Item formatted'
		break
	endloop

	# Clean up.
	bufLines > 0 and windLineNum + bufLines - 1 => reframeWind
	bfCleanup searchPat, true, rtnMsg
endroutine

# Format an ordered list, which consists of one or more contiguous item blocks.  An item block begins with a line that matches
# the following regular expression:
#	/^ *\d+\.[ \t]+[^ \t]/
# and ends at the next item block, a blank line, or end-of-buffer.  An item block may itself be an ordered list, which consists
# of one or more phrases that begin with the following regular expression:
#	/ +\(\d+\), */
# and end with a semicolon or period.  All text in item blocks above and below the point is wrapped as needed and blocks and
# phrases are renumbered beginning with 1 by default.  If a non-negative n argument if given, it is used as the number of the
# first item block.
command bfFormatList(0) {description: 'Format a sequence of numbered list items (as described for the ~bbfFormatItem~B command)\
 and resequence all such items beginning at number n (default 1).'}

	# Check wrap column, current line, and n argument.
	if $wrapCol == 0
		return 1 => message 'Fail, High', 'Wrap column not set'
	endif
	beginText
	if $lineChar == 012
		return 1 => message 'Fail, High', 'Not in a list'
	endif
	if $0 == defn
		$0 = 1
	elsif $0 < 0
		return 1 => message 'Fail, High', 'First item number (%d) must be 0 or greater' % $0
	endif

	ItemPrefixRE, BlankLineRE = $BFInfo

	# Find top of item list.
	1 => setMark ?`					# Save current position.
	($windLineNum - 1) => backLine			# Move to top line of window...
	1 => setMark ??					# and set a mark (for later).
	gotoMark ?`
	searchPat = $searchPat				# Save current search pattern.
	if !searchBack(BlankLineRE)			# Blank line found searching backward?
		beginBuf				# No, move to first line of buffer.
	else
		forwLine				# Yes, move to first possible item line.
	endif

	# Check if current line or next line is the beginning of an item block.
	2 => narrowBuf
	if !searchForw(ItemPrefixRE)			# Item prefix found searching forward?
		widenBuf				# Not found... it's a bust.
		delMark ??
		gotoMark ?`
		return bfCleanup searchPat, false, 'Not in a list'
	endif

	# First line of first item found.  Begin formatting loop.
	widenBuf
	itemNum = $0
	workBuf = (0 => scratchBuf)[0]
	setMark
	searchBack '.'
	fmt = '%%%uu. ' % $lineOffset			# Use indentation of first line as template.
	gotoMark ?.
	loop
		# Point is at end of prefix.
		backChar				# Back up one character...
		0 => delToBreak				# and nuke everything before it.
		seti itemNum++, fmt, 1
		inserti					# Generate new prefix.
		bfNarrowItem ItemPrefixRE, BlankLineRE	# Narrow buffer to this item...
		bfFormatOneItem workBuf			# and format it.

		if 1 => bufBound?			# Now at EOB?
			break				# Yes, we're done.
		else
			forwChar
			if $LineLen == 0		# Now on blank line?
				break			# Yes, we're done.
			elsif $lineText !~ ItemPrefixRE	# No, next item?
				break			# No, we're done.
			endif
		endif
		length(match 0) => forwChar		# Yes, continue.
	endloop

	# Restore original framing if possible.
	setMark
	-1 => gotoMark ??
	1 => reframeWind
	gotoMark ?.

	# Clean up.
	-1 => delBuf workBuf
	bfCleanup searchPat, true, 'List formatted'
endroutine

##### Bindings and aliases #####
bindKey 'ESC .', bfFormatItem
bindKey 'ESC L', bfFormatList
