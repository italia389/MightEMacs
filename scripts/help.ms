# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# help.ms		Ver. 9.5.0
#	MightEMacs help system.

##### Set global constants. #####
$HelpInfo = [['*Help', '*Help-home'], ['*Adv', '*AdvMainMenu'], 'C-h g']
				# Parameters for novice, expert, and common.

# Find help directory.
if stat? (dir = '%s/../help' % (1 => dirname($RunFile))), 'dL'
	$HelpDir = pathname(dir)
else
	# Directory not found.
	return false
endif

##### Commands and Functions #####

# Read given help file into current window and set its attributes.
function hsLoadFile(1)
	if defined?('Name', $1) == 'buffer'
		selectBuf $1
	else
		bufname = (viewFile join('/', $HelpDir, $1))[0]
		setBufFile bufname, nil
		2 => chgMode bufname, nil
		1 => chgBufAttr bufname, 'Hidden, ReadOnly, TermAttr'
	endif
endroutine

# Return true if character argument is one of "A-Za-z0-9-"; otherwise, false.
function hsWordChar?(1)
	$1 == ?- || ($1 != ?_ && wordChar?($1))
endroutine

# Move point forward one "link word".
function hsForwWord(0)
	while hsWordChar? $lineChar
		forwChar
	endloop
endroutine

# Move point backward one "link word".
function hsBackWord(0)
	while hsWordChar?(ord subline(-1, 1))
		backChar
	endloop
endroutine

# Return link name if point on a link; otherwise, nil.
function hsOnLink?(0)
	1 => setMark ??
	result = nil
	loop
		if hsWordChar? $lineChar
			if $lineOffset == 0
				break
			endif
			hsBackWord
			if $lineOffset == 0 || subline(-1, 1) != '<'
				break
			endif
			setMark
			hsForwWord
			if $lineChar == ord('>')
				result = $RegionText
				break
			endif
		elsif $lineChar == ord('<')
			if !hsWordChar?(ord subline(1, 1))
				break
			endif
			forwChar
			setMark
			hsForwWord
			if $lineChar != ord('>')
				break
			endif
			result = $RegionText
			break
		elsif $lineChar == ord('>')
			if $lineOffset == 0 || !hsWordChar?(ord subline(-1, 1))
				break
			endif
			setMark
			hsBackWord
			if $lineOffset == 0 || subline(-1, 1) != '<'
				break
			endif
			result = $RegionText
			break
		endif
		break
	endloop

	# Return result.
	-1 => gotoMark ??
	result
endroutine

# Go to current link or perform appropriate action.
command hsGoto(0) {description: 'Go to a help link.'}
	helpBufPrefix = $HelpInfo[0][0]

	if index($bufname, helpBufPrefix) != 0			# In a help buffer?
		run help					# No, enter Help System.
	else							# Yes, initialize.
		mainBuf = $HelpInfo[0][1]

		if nil?(link = hsOnLink?)			# Point on a link?
			beep
			1 => message 'Force, NoWrap', 'Cursor not on a link.  Please try again.'
		else
			if link == 'exit'			# Yes, exit?
				restoreScreen
			elsif link == 'home'			# No, home?
				if $bufname == mainBuf
					1 => message 'Force, NoWrap', 'Already on HOME screen'
				else
					selectBuf mainBuf
				endif
			else
				hsLoadFile join '-', helpBufPrefix, link
			endif
		endif
	endif
endroutine

# Main menu.  Called by "help" command after Help System (this file) is loaded.
function hsMain(0)

	# Get common parameter(s).
	helpKey = $HelpInfo[2]
	helpBufPrefix, mainBuf = $HelpInfo[$0 == defn ? 0 : 1]

	# Set up and switch to help screen.
	curScreenNum = $screenNum
	loop
		# If novice level, find screen displaying a buffer whose name begins with helpBufPrefix.
		if($0 == defn)
			for windInfo in 1 => getInfo('Windows')
				screenNum, windNum, bufname = windInfo
				if index(bufname, helpBufPrefix) == 0
					if screenNum == curScreenNum
						1 => message 'Force, NoWrap', 'Already in Help System'
						return
					endif
					saveScreen
					selectScreen screenNum
					break 2
				endif
			endloop
		endif

		# Expert level or screen not found... create one.
		saveScreen
		selectScreen 0
		hsLoadFile mainBuf
		break
	endloop

	# Now on help screen.  Do processing for requested help level.
	if $0 == defn
		# Novice level.  Bind hsGoto command.
		bindKey helpKey, hsGoto
	else
		# Expert level.		<n> item, [b]ack, [q]uit:
		$bufname == mainBuf || selectBuf(mainBuf)
		1 => unbindKey helpKey
		searchPat = $searchPat

		# Display help menus and get response.  Each page contains the following footer (navigation) lines:
		# back:<bufname> -OR- back:^ (for QUIT)
		# 1:F:<popFilename>
		# 2:M:<menuFilename>
		# 3:m:<manPage>
		# ...
		loop
			beginBuf
			updateScreen
			nil?(choice = prompt('~u~bNN~Z menu item, ~u~bb~Zack, ~u~bq~Zuit [b]: ', 'TermAttr')) and\
			 choice = 'b'
			if choice == 'q'
				break
			else
				endBuf
				if choice == 'b'
					0 => searchBack "\nback:"	# Retain previous search pattern.
					if !huntBack
						break			# Not found!  Just quit.
					endif
					endWord
					if (bufname = subline(1)) == '^'
						break
					endif
					selectBuf bufname
					next
				elsif numeric? choice
					0 => searchBack "\n#{choice}:"
					if huntBack
						2 => forwWord
						if $lineChar == ord('F')

							# Pop file.
							1 => popFile join('/', $HelpDir, subline(2)),\
							 'AltModeLine, Delete, TermAttr'
						elsif $lineChar == ord('M')

							# Display submenu.
							hsLoadFile subline 2
						else
							# Display man page.  Try to find MighEMacs man directory in $execPath
							# first in case man search path is not configured properly.
							page = subline(2)
							execPath = $execPath
							until nil?(dir = strShift(execPath, ?:))
								if !null?(dir) && (i = index(dir, '/lib/memacs')) != nil
									manPath = join('/', substr(dir, 0, i),\
									 'share/man/man1', '%s.1' % page)
									stat?(manPath, 'f') and page = manPath
									break
								endif
							endloop
							cols, rows = $TermSize
							1 => shellCmd 'NoHdr', 'MANWIDTH=', cols, ' man ', page, ' | col -b'
						endif
						next
					endif
				endif
				print "Unknown option \"#{choice}\""
				pause 1
			endif
		endloop

		# Help exit.  Delete current screen and all buffers whose name begins with helpBufPrefix.
		screenNum = $screenNum
		restoreScreen
		delScreen screenNum
		for bufname in 1 => bufInfo(nil, 'Visible, Hidden, Brief')
			index(bufname, helpBufPrefix) == 0 && delBuf(bufname)
		endloop
		restoreSearchRing searchPat
	endif
endroutine

true
