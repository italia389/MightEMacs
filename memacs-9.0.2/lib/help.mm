# (c) Copyright 2018 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# help.mm		Ver. 9.0.0
#	MightEMacs help system.

##### Set global constants. #####
$HelpInfo = [['*Help','*Help-home'],['*Adv','*AdvMainMenu'],['A-Za-z0-9-','C-h C-h']]
				# Parameters for novice, expert, and common.

# Find help directory.
if stat? (dir = '%s/help' % (1 => dirname($RunFile))),'dL'
	$HelpDir = dir
else
	# Directory not found.
	return false
endif

##### Define macros. #####

# Read given help file into current window and set its attributes.
macro hsLoadFile(1)
	if defined?($1) == 'buffer'
		selectBuf $1
	else
		bufName = (viewFile join('/',$HelpDir,$1))[0]
		1 => setBufFile bufName,nil
		2 => alterBufMode bufName,'RdOnly'
		1 => alterBufAttr bufName,'Hidden','TermAttr'
	endif
endmacro

# Main menu.  Called by help hook macro after Help System (this file) is loaded.
macro hsMain(0) {desc: 'Main menu for help system.  Run in "expert" mode if n argument.'}
	formerMsgState = -1 => alterGlobalMode 'Msg'
	wordChars = $wordChars

	# Get common parameter(s).
	$wordChars,helpKey = $HelpInfo[2]
	helpBufPrefix,mainBuf = $HelpInfo[$0 == defn ? 0 : 1]

	# Set up and switch to help screen.
	loop
		# If novice level, find screen displaying a buffer whose name begins with helpBufPrefix.
		if($0 == defn)
			for windInfo in 1 => getInfo('Windows')
				screenNum,windNum,bufName = windInfo
				if index(bufName,helpBufPrefix) == 0
					saveScreen
					selectScreen screenNum
					break 2
				endif
			endloop
		endif

		# Expert level or screen not found... create one.
		saveScreen
		1 => selectScreen
		hsLoadFile mainBuf
		break
	endloop

	# Now on help screen.  Do processing for requested help level.
	if $0 == defn
		# Novice level.  Define hsGoto macro (and others) if needed.
		if !defined? 'hsGoto'

			# Return link name if point on a link; otherwise, nil.
			macro hsOnLink?(0)
				1 => setMark '?'
				result = nil
				loop
					if wordChar? $lineChar
						if $lineOffset == 0
							break
						endif
						wordChar?(subline(-1,1)) && backWord
						if $lineOffset == 0 || subline(-1,1) != '<'
							break
						endif
						setMark
						endWord
						if $lineChar == '>'
							result = $RegionText
							break
						endif
					elsif $lineChar == '<'
						if !wordChar? subline(1,1)
							break
						endif
						forwChar
						setMark
						endWord
						if $lineChar != '>'
							break
						endif
						result = $RegionText
						break
					elsif $lineChar == '>'
						if $lineOffset == 0 || !wordChar? subline(-1,1)
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
				-1 => gotoMark '?'
				result
			endmacro

			# Go to current link or perform appropriate action.
			macro hsGoto(0) {desc: 'Go to a help link.'}
				helpBufPrefix = $HelpInfo[0][0]

				if index($bufName,helpBufPrefix) != 0			# In a help buffer?
					run help					# No, enter Help System.
				else							# Yes, initialize.
					formerMsgState = -1 => alterGlobalMode 'Msg'
					wordChars = $wordChars
					$wordChars = $HelpInfo[2][0]
					mainBuf = $HelpInfo[0][1]

					if nil?(link = hsOnLink?)			# Point on a link?
						beep
						-1 => success 'Cursor not on a link.  Please try again.'
					else
						if link == 'exit'			# Yes, exit?
							restoreScreen
						elsif link == 'home'			# No, home?
							if $bufName == mainBuf
								-1 => success 'Already on HOME screen'
							else
								selectBuf mainBuf
							endif
						else
							hsLoadFile join '-',helpBufPrefix,link
						endif
					endif

					$wordChars = wordChars
					formerMsgState => alterGlobalMode 'Msg'
				endif
			endmacro
		endif
		bindKey helpKey,hsGoto
	else
		# Expert level.		<n> item, [b]ack, [q]uit:
		$bufName == mainBuf || selectBuf(mainBuf)
		1 => unbindKey helpKey

		# Display help menus and get response.  Each page contains the following footer (navigation) lines:
		# back:<bufName> -OR- back:^ (for QUIT)
		# 1:F:<popFilename>
		# 2:M:<menuFilename>
		# 3:m:<manPage>
		# ...
		loop
			beginBuf
			updateScreen
			nil?(choice = 1 => prompt '~u~bNN~0 menu item, ~u~bb~0ack, ~u~bq~0uit [~u~bb~0]: ') and choice = 'b'
			if choice == 'q'
				break
			else
				endBuf
				searchPat = $searchPat
				if choice == 'b'
					0 => searchBack "\nback:"	# Retain previous search pattern.
					if !huntBack
						break			# Not found!  Just quit.
					endif
					endWord
					if (bufName = subline(1)) == '^'
						break
					endif
					selectBuf bufName
					next
				elsif numeric? choice
					0 => searchBack "\n#{choice}:"
					if huntBack
						2 => forwWord
						if $lineChar == 'F'

							# Pop file.
							1 => popFile join('/',$HelpDir,subline(2)),'adt'
						elsif $lineChar == 'M'

							# Display submenu.
							hsLoadFile subline 2
						else
							# Display man page.  Try to find MighEMacs man directory in $execPath
							# first in case man search path is not configured properly.
							page = subline 2
							execPath = $execPath
							until nil?(dir = strShift execPath,':')
								if !null?(dir) && (i = index dir,'/lib/memacs') != nil
									manPath = join '/',substr(dir,0,i),\
									 'share/man/man1','%s.1' % page
									stat?(manPath,'f') and page = manPath
									break
								endif
							endloop
							shellCmd('man ',page) == false && prompt('End',nil,'c')
						endif
						next
					endif
				endif
				-1 => print "Unknown option \"#{choice}\""
				pause 1
			endif
		endloop

		# Help exit.  Delete current screen and all buffers whose name begins with helpBufPrefix.
		screenNum = $screenNum
		restoreScreen
		deleteScreen screenNum
		for bufName in 0 => getInfo('Buffers')
			index(bufName,helpBufPrefix) == 0 && deleteBuf(bufName)
		endloop
	endif

	$wordChars = wordChars
	formerMsgState => alterGlobalMode 'Msg'
endmacro

true
