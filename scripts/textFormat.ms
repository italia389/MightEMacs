# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# textFormat.ms		Ver. 10.0.0
#	Routines for formatting blocks of text such as comment blocks and numbered list items.

# Don't load this file if it has already been done so.
if isExec? 'tfLineSearch'
	return
endif

# Get required arguments:
#  1: [Array] Comment prefixes.
#  2: [String] Bullet characters.
if length(ArgList) != 2 || typeof(ArgList[0]) != 'array' || typeof(ArgList[1]) != 'string' ||\
 empty?(ArgList[0]) || empty?(ArgList[1])
	return 1 => message('Fail', 'Missing or invalid package-execution argument(s): comment-prefixes bullet-chars')
endif

# Set global constants.
TFBoundary =	0x0001
TFBlank =	0x0002
TFNumList =	0x0004
TFBullet =	0x0008
TFComment =	0x0010
TFPlain =	0x0020
TFNonSpecial =	0x0040

TFConfig = [\
 '^\h*$:re',					/# 0:BlankRE #/\
 '^ *\d+\. +[^ ]:re',				/# 1:NumListRE #/\
 nil,						/# 2:BulletRE #/\
 nil,						/# 3:CommentRE #/\
 ' *\(\d+\),:re',				/# 4:PhraseRE #/\
 ArgList[0]]					/# 5:Comment prefixes #/

# Make RE for bullet line and insert into TFConfig.  Move dash character to front if present in string so it will be interpreted
# as a plain character in RE bracketed expression (instead of a range operator).
if (i = index(bullets = ArgList[1], ?-, 'Char')) != nil && i > 0
	bullets = sprintf('-%s%s', bullets[0, i], bullets[i + 1,])
endif
TFConfig[2] = '^\h*[%s] +[^ ]:re' % bullets

# Make RE for comment line and insert into TFConfig.
re = '^\h*('
delim = nil
for prefix in ArgList[0]
	strPush re, delim, prefix
	delim = '|'
endloop
TFConfig[3] = re & '):re'

##### Commands and Functions #####

# Renumber phrases within current region.  Return with point at end.
function tfRephrase(0)

	# Renumber phrases within block, if any.
	0 => narrowBuf
	seti 1, '%u', 1
	$searchPat = TFConfig[4]
	loop
		if !huntForw
			break
		endif
		2 => backChar
		-1 => delWord
		inserti
	endloop
	delRingEntry 'Search'
	endBuf
	widenBuf
endroutine

# Reset search ring and return a message.  Argument(s):
#  1: [String] Old $searchPat value.
#  2: [Boolean] Operation successful?
#  3: [String] Return message.
function tfCleanup(3)
	restoreSearchRing $1
	1 => message($2 ? 'High' : 'Fail, High', $3)
endroutine

# Check if given column is at or past wrap column.  If so, set error and return false; otherwise, return true.
function tfWrapOkay?(1)
	if $1 >= $wrapCol
		1 => message('Fail, High', "Indentation of line block (#{$1}) at or past wrap column (#{$wrapCol})")
	else
		true
	endif
endroutine

# Format a numbered list item.  Point is assumed to be at beginning of region.
function tfFormatNumList(0)
	setMark ?_
	-1 => searchForw '. :pe'			# Save item number text...
	itemNumLen = $lineOffset
	if tfWrapOkay?(itemNumLen)
		itemNum = subline(-itemNumLen, itemNumLen)
		beginLine				# and overwrite it with spaces.
		itemNumLen => overwriteChar ' '

		0 => wrapLine nil, EndSentence
		setMark
		-1 => gotoMark ?_
		0 => overwriteChar itemNum		# Restore item number text.
		swapMark ?.
	endif
endroutine

# Format a bulleted item.  Point is assumed to be at beginning of region.
function tfFormatBullet(0)
	beginText
	bulletOffset = $lineOffset
	if tfWrapOkay?(bulletOffset + 2)
		bulletChar = $lineChar
		$lineChar = ?\s				# Replace bullet with space.

		# Rewrap line block and move to end of last line.
		beginLine
		setMark ?_
		0 => wrapLine nil, EndSentence
		setMark
		-1 => gotoMark ?_
		bulletOffset => forwChar
		$lineChar = bulletChar			# Restore bullet character.
		beginLine
		swapMark ?.
	endif
endroutine

# Format a comment line block.  Point is assumed to be at beginning of region.
function tfFormatComment(0)
	setMark ?_
	beginText
	for prefix in TFConfig[5]			# Comment prefixes.
		prefixLen = length(prefix)
		if subline(0, prefixLen) == prefix
			break
		endif
	endloop

	# Check if indentation past wrap column.
	if tfWrapOkay?($lineCol + prefixLen + 1)

		# Rewrap line block and move to end of last line.
		0 => wrapLine prefix & ' ', EndSentence
		setMark
		-1 => gotoMark ?_
		swapMark ?.
	endif
endroutine

# Search forward or backward for a block-begin line or block boundary (buffer boundary or blank line), beginning at current
# line.  Arguments:
#  1: [Integer] Type flags (bit masks) specifying what line types to search for OR special line-matching pattern.
#
# N argument must be -1 (search backward), 1 (check current line only), or 2 (search forward).  If argument is a pattern, search
# stops at first line not matching pattern, otherwise at first line matching any RE indicated by type flags.
#
# Returns:
#	1: [Integer] Type of line found.
#	2: [Integer] Number of lines from starting line where match found.
#
# Note that the search will always succeed and point will be moved to beginning of line that is found if searching backward, or
# end of line if searching forward.  (If blank line is found and it is not the current line, point will be moved to previous
# line that was passed over first.)  If blank line other than current line was found, or buffer boundary is hit before finding a
# line, TFBoundary is returned, otherwise type flag.
function tfLineSearch(1)
	flags = pat = stopType = nil
	if typeof($1) == 'string'
		pat = $1
	else
		flags = $1
	endif
	blankRE, numListRE, bulletRE, commentRE = TFConfig

	# Scan forward or backward for line that matches one of specified REs, beginning at current line.
	lineOffset = 0
	loop
		# Check line type if (1), n argument is 1; (2), not on current line; or (3), searching backward and not a
		# special pattern search; otherwise, skip this (first) line.
		if $0 == 1 || lineOffset > 0 || $0 < 0 && !pat
			lineText = $lineText
			if lineText =~ blankRE
				if lineOffset == 0
					stopType = TFBlank
				else
					stopType = TFBoundary
					$0 == 2 ? backLine : forwLine
					--lineOffset
				endif
			elsif pat
				if lineText !~ pat
					stopType = TFNonSpecial
				endif
			elsif (flags & TFNumList) && lineText =~ numListRE
				stopType = TFNumList
			elsif (flags & TFBullet) && lineText =~ bulletRE
				stopType = TFBullet
			elsif (flags & TFComment) && lineText =~ commentRE
				stopType = TFComment
			endif
		endif

		if stopType					# Found match?
			break					# Yes, done.
		elsif $0 == 1
			stopType = TFPlain
			break
		elsif not $0 => beginLine			# No, onward to next line.  Hit buffer boundary?
			stopType = TFBoundary			# Yes.
			break
		else
			++lineOffset
		endif
	endloop

	$0 == 2 ? endLine : beginLine
	[stopType, lineOffset]
endroutine

# Find beginning and end of a line block and return its type.  Return zero if no block found, otherwise; set region and return
# block type.
function tfBlockBounds(0)
	blockType = nil

	# If n not default, it is block selector.
	if $0 != defn
		# Move to beginning of block and get type of first line (only).
		-1 => selectText($0)
		stopType, lineOffset = 1 => tfLineSearch(TFNumList | TFBullet | TFComment)
		if stopType == TFBlank
			return 0
		endif
		swapMark ?.
		blockType = stopType
	else
		# Beginning at unknown buffer location.  Search backward for any line type.
		stopType, lineOffset = -1 => tfLineSearch(TFNumList | TFBullet | TFComment)

		if stopType == TFBlank
			return 0						# Current line is blank.
		elsif stopType & (TFNumList | TFBullet)
			blockType = stopType
			setMark
			stopType, lineOffset = 2 => tfLineSearch(TFNumList | TFBullet | TFComment)
			if stopType & (TFNumList | TFBullet | TFComment)
				-1 => endLine					# Can't fail.
				--lineOffset
			endif
		elsif stopType == TFComment && lineOffset == 0
			blockType = stopType

			# Current line is a comment.  Find beginning and end of block.
			re = '^\h*%s:re' % strMatchGrp(1)
			stopType, lineOffset = -1 => tfLineSearch(re)
			if stopType == TFNonSpecial
				2 => beginLine					# Can't fail.
			endif
			setMark
			stopType, lineOffset = 2 => tfLineSearch(re)
			if stopType == TFNonSpecial
				-1 => endLine					# Can't fail.
				--lineOffset
			endif
		else
			# Hit comment or boundary going backward.
			if stopType == TFComment
				2 => beginLine					# Can't fail.
			endif
			blockType = TFPlain

			# Found one or more plain text lines.  Find last one.
			setMark
			stopType, lineOffset = 2 => tfLineSearch(TFNumList | TFBullet | TFComment)
			if lineOffset > 0 && stopType & (TFNumList | TFBullet | TFComment)
				-1 => endLine					# Can't fail.
				--lineOffset
			endif
		endif
	endif
	swapMark ?.
	blockType
endroutine

# Format a block of comment lines, one numbered list item, one bulleted list item, or a plain line block.  Point must be within
# line group.  comment-prefixes and bullet-chars arguments to this file are used to locate comment lines and control spacing
# when lines are joined.
command tfFormatItem(0) {description: join(nil, "Format a block of comment lines, one numbered list item, one bulleted list\
 item, or a plain line block (which may contain a mix of indented and non-indented lines).  Point must be within the line\
 block when command is invoked.\n\nAll lines in a comment block, bulleted list item, or plain line block are rewrapped (using\
 the EndSentence global variable to control spacing between joined lines) so that they all have the same indentation as the\
 first line of the block but do not extend past the current wrap column set in $wrapCol.  All lines in a numbered list item are\
 similarly rewrapped so that the first line begins with \"nn. \" (where \"nn\" is a one or two digit number) and all subsequent\
 lines are indented by four spaces.  Additionally, if command was invoked with \"ESC ,\" (instead of \"ESC .\"), any text\
 within a line block matching the pattern \"", TFConfig[4], "\" is assumed to be a numbered \"phrase\" (such as \"... (2),\
 ...\", and all such numbers are resequenced beginning at 1.\n\nLine blocks are bounded by buffer boundaries, blank lines, or\
 lines that are of a different type than the lines in the block.")}

	# Check wrap column.
	if $wrapCol == 0
		return 1 => message 'Fail, High', 'Wrap column not set'
	endif

	# Check if buffer is narrowed.
	if bufAttr? 'Narrowed'
		return 1 => message 'Fail, High', 'Buffer is narrowed'
	endif

	# All is well.  Save state parameters and begin search.
	setMark ??
	searchPat = $searchPat

	# Get block bounds around current line.
	if (blockType = $0 => tfBlockBounds) == 0
		0 => gotoMark ??
		return tfCleanup(searchPat, false, 'Not in a line block')
	endif

	# Found text block.  Process it.
	rtnMsg = 'Block formatted'
	if blockType == TFNumList
		success = tfFormatNumList
		rtnMsg = 'Item formatted'
	elsif blockType == TFBullet
		success = tfFormatBullet
		rtnMsg = 'Item formatted'
	elsif blockType == TFComment
		success = tfFormatComment
	else
		# TFPlain.
		if success = tfWrapOkay?($lineOffset)
			setMark ?_
			0 => wrapLine nil, EndSentence
			setMark
			-1 => gotoMark ?_
			swapMark ?.
		endif
	endif

	# Reformat phrases in line block if we were invoked with "ESC ,".
	if success && $lastKeySeq == 'ESC ,'
		tfRephrase
	endif

	# Clean up.
	delMark ??
	if success
		tfCleanup searchPat, true, rtnMsg
	else
		restoreSearchRing searchPat
		false
	endif
endroutine

##### Bindings and aliases #####
bindKey 'ESC .', tfFormatItem
