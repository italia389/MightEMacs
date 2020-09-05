# Root Unix makefile for MightEMacs.		Ver. 9.5.0

# Definitions.
MAKEFLAGS = --no-print-directory
PlatformFile = .platform
PlatformList = .platforms
PlatformTemplate = centos
ProjName = memacs
BinName1 = mm
BinName2 = memacs
CXLibName = libcx.a
XRELibName = libxre.a
SiteMS = site.ms
UserMS = .memacs
Install1 = /usr/local
Install2 = /usr

.PHONY:	all platform build link uninstall install user-install clean

all: platform build link

platform: $(PlatformFile)
	@platform=`cat $?` || exit $$?;\
	umask 022;\
	for d in $$platform $$platform/obj; do \
		if [ ! -d "$$d" ]; then \
			mkdir $$d || exit $$?;\
			echo "Created '$$d' directory." 1>&2;\
		fi;\
	done;\
	if [ ! -f $$platform/Makefile ]; then \
		cp "$(PlatformTemplate)/Makefile" $$platform || exit $$?;\
		echo "Created '$$platform/Makefile' (from '$(PlatformTemplate)' platform)." 1>&2;\
	fi;\

$(PlatformFile):
	@sed -n -e '1,/^\/\/ OS-dependent/d; s/^#define \([^	][^	]*\)		*1$$/\1/p; /^$$/q' include/stdos.h |\
	 tr '[A-Z]' '[a-z]' >$(PlatformList);\
	if [ -z '$(PLATFORM)' ]; then \
		printf 'Platform not specified.  Syntax is:\n\n\t$$ make PLATFORM=<platform> CXL=<dir> XRE=<dir>\n\nwhere <platform> is one of:\n' 1>&2;\
		sed -e 's/^/	/' $(PlatformList) 1>&2;\
		echo 1>&2;\
		rm $(PlatformList);\
		exit 1;\
	elif ! egrep -qi "$(PLATFORM)" $(PlatformList); then \
		printf 'Error: Unknown platform "%s".  Platform must be one of:\n' "$(PLATFORM)" 1>&2;\
		sed -e 's/^/	/' $(PlatformList) 1>&2;\
		echo 1>&2;\
		rm $(PlatformList);\
		exit 1;\
	else \
		echo "$(PLATFORM)" | tr '[A-Z]' '[a-z]' >$@;\
	fi

build:
	@platform=`cat $(PlatformFile)` || exit $$?;\
	if [ ! -f $$platform/$(BinName1) ]; then \
		echo 'Building MightEMacs...' 1>&2;\
		cd $$platform || exit $$?;\
		if [ -f "$(CXL)/lib/$(CXLibName)" ]; then \
			cxLib="$(CXL)/lib/$(CXLibName)";\
		else \
			cxLib="$(CXL)/$(CXLibName)";\
		fi;\
		if [ -f "$(XRE)/lib/$(XRELibName)" ]; then \
			xreLib="$(XRE)/lib/$(XRELibName)";\
		else \
			xreLib="$(XRE)/$(XRELibName)";\
		fi;\
		for lib in "$$cxLib" "$$xreLib"; do \
			[ -f "$$lib" ] || { echo "Error: Library '$$lib' does not exist" 1>&2; exit 1; };\
		done;\
		make LibIncl="-I$(CXL)/include -I$(XRE)/include" CXLib="$$cxLib" XRELib="$$xreLib";\
	fi

uninstall:
	@echo 'Uninstalling...' 1>&2;\
	if [ -n "$(INSTALL)" ] && [ -x "$(INSTALL)/bin/$(BinName2)" ]; then \
		mainDir="$(INSTALL)";\
	elif [ -x "$(Install1)/bin/$(BinName2)" ]; then \
		mainDir=$(Install1);\
	elif [ -x "$(Install2)/bin/$(BinName2)" ]; then \
		mainDir=$(Install2);\
	else \
		mainDir=;\
	fi;\
	if [ -z "$$mainDir" ]; then \
		echo "'$(BinName2)' binary not found." 1>&2;\
	else \
		for f in "$$mainDir/bin/$(BinName1)" "$$mainDir/bin/$(BinName2)" "$$mainDir/lib/$(ProjName)"\
		 "$$mainDir/share/$(ProjName)" "$$mainDir/share/man/man1/$$ProjName"*; do \
			if [ -e "$$f" ]; then \
				rm -rf "$$f" && echo "Deleted '$$f'" 1>&2;\
			fi;\
		done;\
	fi;\
	echo 'Done.' 1>&2

link:
	@platform=`cat $(PlatformFile)` || exit $$?;\
	rm -f $(BinName1);\
	ln -s $$platform/$(BinName1) $(BinName1);\
	echo "'$(BinName1)' link file created." 1>&2

install: uninstall
	@echo 'Beginning installation...' 1>&2;\
	umask 022;\
	myID=`id -u`;\
	comp='-C' permCheck=false mainDir=$(INSTALL);\
	if [ $$myID -ne 0 ]; then \
		if [ -z "$$mainDir" ]; then \
			if uname -v | fgrep -qi debian; then \
				echo 'Error: You must be root to perform a site-wide install' 1>&2;\
				exit 1;\
			fi;\
			permCheck=true;\
		fi;\
	elif [ -z "$$mainDir" ]; then \
		if uname -v | fgrep -qi debian; then \
			mainDir=$(Install2);\
		fi;\
		permCheck=true;\
	else \
		permCheck=true;\
	fi;\
	[ -z "$$mainDir" ] && mainDir=$(Install1);\
	if [ $$permCheck = false ]; then \
		own= grp= owngrp=;\
		dmode=755 fmode=644;\
	else \
		if [ `uname -s` = Darwin ]; then \
			fmt=-f perm='%p';\
		else \
			fmt=-c perm='%a';\
		fi;\
		eval `stat $$fmt 'own=%u grp=%g' "$$mainDir"`;\
		if [ $$own -ne $$myID ] && [ $$myID -ne 0 ]; then \
			owngrp=;\
		else \
			owngrp="-o $$own -g $$grp";\
		fi;\
		eval `stat $$fmt $$perm "$$mainDir" |\
		 sed 's/^/000/; s/^.*\(.\)\(.\)\(.\)\(.\)$$/p3=\1 p2=\2 p1=\3 p0=\4/'`;\
		dmode=$$p3$$p2$$p1$$p0 fmode=$$p3$$(($$p2 & 6))$$(($$p1 & 6))$$(($$p0 & 6));\
		[ $$p3 -gt 0 ] && comp=;\
	fi;\
	[ -f $(BinName1) ] || { echo "Error: File '`pwd`/$(BinName1)' does not exist" 1>&2; exit 1; };\
	[ -d "$$mainDir"/bin ] || install -v -d $$owngrp -m $$dmode "$$mainDir"/bin 1>&2;\
	install -v $$owngrp -m $$dmode $(BinName1) "$$mainDir"/bin 1>&2;\
	ln -f "$$mainDir"/bin/$(BinName1) "$$mainDir"/bin/$(BinName2);\
	[ -n "$$owngrp" ] && chown $$own:$$grp "$$mainDir"/bin/$(BinName2);\
	chmod $$dmode "$$mainDir"/bin/$(BinName2);\
	[ -d "$$mainDir/share/$(ProjName)/scripts" ] ||\
	 install -v -d $$owngrp -m $$dmode "$$mainDir/share/$(ProjName)/scripts" 1>&2;\
	cd scripts || exit $$?;\
	for x in *.ms; do \
		bak=;\
		[ $$x = $(SiteMS) ] && bak=-b;\
		install -v $$comp $$bak $$owngrp -m $$fmode $$x "$$mainDir/share/$(ProjName)/scripts" 1>&2;\
	done;\
	cd ..;\
	[ -d "$$mainDir"/share/man/man1 ] || install -v -d $$owngrp -m $$dmode "$$mainDir"/share/man/man1 1>&2;\
	cd doc || exit $$?;\
	for x in *.1; do \
		install -v $$comp $$owngrp -m $$fmode $$x "$$mainDir"/share/man/man1 1>&2;\
	done;\
	cd ..;\
	[ -d "$$mainDir"/share/$(ProjName)/help ] || install -v -d $$owngrp -m $$dmode "$$mainDir"/share/$(ProjName)/help 1>&2;\
	cd help || exit $$?;\
	for x in *; do \
		install -v $$comp $$owngrp -m $$fmode $$x "$$mainDir/share/$(ProjName)/help" 1>&2;\
	done;\
	echo "Done.  MightEMacs files installed in '$$mainDir'." 1>&2

user-install:
	@echo 'Beginning user startup file installation...' 1>&2;\
	cd scripts || exit $$?;\
	install -v -C -b -m 644 $(UserMS) ~ 1>&2;\
	echo "Done.  User startup file '$(UserMS)' installed in '`cd; pwd`'." 1>&2

clean:
	@for f in *; do \
		if [ -f "$$f/Makefile" ]; then \
			cd $$f || exit $$?;\
			make $@; cd ..;\
			rm -f $(BinName1) 2>/dev/null || :;\
			echo "'$$f' binaries deleted." 1>&2;\
		fi;\
	done;\
	rm -f $(PlatformFile) $(PlatformList) 2>/dev/null || :
