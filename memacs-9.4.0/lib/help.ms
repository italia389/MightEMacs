# (c) Copyright 2019 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# help.ms		Ver. 9.4.0
#	MightEMacs help system.

##### Set global constants. #####
$HelpInfo = [['*Help','*Help-home'],['*Adv','*AdvMainMenu'],['A-Za-z0-9-','C-h g']]
				# Parameters for novice, expert, and common.

# Find help directory.
if stat? (dir = '%s/help' % (1 => dirname($RunFile))),'dL'
	$HelpDir = dir
else
	# Directory not found.
	return false
endif

##### Macros #####

# Read given help file into current window and set its attributes.
constrain macro hsLoadFile(1)
	if defined?('Name',$1) == 'buffer'
		selectBuf $1
	else
		bufname = (viewFile join('/',$HelpDir,$1))[0]
		setBufFile bufname,nil
		2 => chgMode bufname,nil
		1 => chgBufAttr bufname,'Hidden,ReadOnly,TermAttr'
	endif
endmacro

# Return link name if point on a link; otherwise, nil.
constrain macro hsOnLink?(0)
	1 => setMark ??
	result = nil
	loop
		if wordChar? $lineChar
			if $lineOffset == 0
				break
			endif
			wordChar?(ord subline(-1,1)) && backWord
			if $lineOffset == 0 || subline(-1,1) != '<'
				break
			endif
			setMark
			endWord
			if $lineChar == ord('>')
				result = $RegionText
				break
			endif
		elsif $lineChar == ord('<')
			if !wordChar? ord subline(1,1)
				break
			endif
			forwChar
			setMark
			endWord
			if $lineChar != ord('>')
				break
			endif
			result = $RegionText
			break
		elsif $lineChar == ord('>')
			if $lineOffset == 0 || !wordChar? ord subline(-1,1)
				break
			endif
			setMark
			backWord
			if $lineOffset == 0 || subline(-1,1) != '<'
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
endmacro

# Go to current link or perform appropriate action.
macro hsGoto(0) {desc: 'Go to a help link.'}
	helpBufPrefix = $HelpInfo[0][0]

	if index($bufname,helpBufPrefix) != 0			# In a help buffer?
		run help					# No, enter Help System.
	else							# Yes, initialize.
		oldMsgState = -1 => chgMode(nil,'RtnMsg')
		wordChars = $wordChars
		$wordChars = $HelpInfo[2][0]
		mainBuf = $HelpInfo[0][1]

		if nil?(link = hsOnLink?)			# Point on a link?
			beep
			message 'Force,NoWrap','Cursor not on a link.  Please try again.'
		else
			if link == 'exit'			# Yes, exit?
				restoreScreen
			elsif link == 'home'			# No, home?
				if $bufname == mainBuf
					message 'Force,NoWrap','Already on HOME screen'
				else
					selectBuf mainBuf
				endif
			else
				hsLoadFile join '-',helpBufPrefix,link
			endif
		endif

		$wordChars = wordChars
		oldMsgState => chgMode nil,'RtnMsg'
	endif
endmacro

# Main menu.  Called by help hook macro after Help System (this file) is loaded.
constrain macro hsMain(0) {desc: 'Main menu for help system.  Run in "expert" mode if n argument.'}
	oldMsgState = -1 => chgMode(nil,'RtnMsg')
	wordChars = $wordChars

	# Get common parameter(s).
	$wordChars,helpKey = $HelpInfo[2]
	helpBufPrefix,mainBuf = $HelpInfo[$0 == defn ? 0 : 1]

	# Set up and switch to help screen.
	loop
		# If novice level, find screen displaying a buffer whose name begins with helpBufPrefix.
		if($0 == defn)
			for windInfo in 1 => getInfo('Windows')
				screenNum,windNum,bufname = windInfo
				if index(bufname,helpBufPrefix) == 0
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
		# Novice level.  Bind hsGoto macro.
		bindKey helpKey,hsGoto
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
			nil?(choice = prompt('~u~bNN~Z menu item, ~u~bb~Zack, ~u~bq~Zuit [b]: ','TermAttr')) and\
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
							1 => popFile join('/',$HelpDir,subline(2)),'AltModeLine,Delete,TermAttr'
						elsif $lineChar == ord('M')

							# Display submenu.
							hsLoadFile subline 2
						else
							# Display man page.  Try to find MighEMacs man directory in $execPath
							# first in case man search path is not configured properly.
							page = subline(2)
							execPath = $execPath
							until nil?(dir = strShift(execPath,?:))
								if !null?(dir) && (i = index(dir,'/lib/memacs')) != nil
									manPath = join('/',substr(dir,0,i),\
									 'share/man/man1','%s.1' % page)
									stat?(manPath,'f') and page = manPath
									break
								endif
							endloop
							cols,rows = $TermSize
							shellCmd 'NoHdr','MANWIDTH=',cols,' man ',page,' | col -b'
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
		for bufname in 1 => bufInfo(nil,'Brief,Hidden')
			index(bufname,helpBufPrefix) == 0 && delBuf(bufname)
		endloop
		restoreSearchRing searchPat
	endif

	$wordChars = wordChars
	oldMsgState => chgMode nil,'RtnMsg'
endmacro

# Set up tutorial.
macro hsTutor(0) {desc: join(' ','Open',getInfo('Editor'),'Tutorial.')}
	PlaceMarker = '%&'
	UserTutorFile = expandPath('~/%sTutorial' % getInfo('Editor'))
	oldMsgState = -1 => chgMode(nil,'RtnMsg')

	# Determine if beginning, ending, or resuming tutorial and prompt user for confirmation.
	if (ending = bufInfo($bufname)[1] == UserTutorFile)
		p = (modified = bufAttr?($bufname,'Changed')) ? 'Save and exit' : 'Exit'
	else
		p = (resume = stat?(UserTutorFile,'e')) ? 'Resume' : 'Begin'
	endif
	if 1 => prompt('%s tutorial? (y/n)' % p,?n,'Type: Char') != ?y
		return
	endif

	# Action confirmed.  Process it.
	if ending
		insert PlaceMarker,$windLineNum,PlaceMarker
		saveFile
		bufname = $bufname
		restoreBuf
		delBuf bufname
		oldMsgState => chgMode(nil,'RtnMsg')
		message 'Force,TermAttr','Tutorial saved.  To exit the editor, press ~#u',binding('KeyList','exit')[0],'~U'
	else
		if !resume
			# Find tutorial file.
			TutorFile = 'Tutorial'
			StockTutorFile = join('/',$HelpDir,TutorFile)
			if !(stat?(StockTutorFile,'e'))
				return hsNotFound StockTutorFile
			endif
		endif

		# If currently in the Help System, go back to previous screen.
		if index($bufname,$HelpInfo[0][0]) == 0 || index($bufname,$HelpInfo[1][0]) == 0
			restoreScreen
		endif

		# Save current buffer so it can be returned to later.
		saveBuf

		# Open tutorial in current window and set it up.
		findFile UserTutorFile
		if resume
			if searchForw PlaceMarker
				length(PlaceMarker) => delBackChar
				setMark
				endWord
				windLineNum = toInt($RegionText)
				length($RegionText) => delBackChar
				length(PlaceMarker) => delForwChar
				onlyWind
				windLineNum => reframeWind
				-1 => chgBufAttr $bufname,'Changed'
			else
				onlyWind
			endif
			delSearchPat
		else
			onlyWind
			0 => insertFile StockTutorFile
			writeFile UserTutorFile
		endif
		oldMsgState => chgMode(nil,'RtnMsg')
		message 'Force','Tutorial ',resume ? 'resumed' : 'started'
	endif
endmacro
bindKey 'C-h t',hsTutor

true
