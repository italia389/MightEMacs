# Root Unix makefile for MightEMacs.		Ver. 8.2.1

# Definitions.
MAKEFLAGS = --no-print-directory
MDIR = memacs
PFFILE = .platform
PFLIST = .platforms
PFTEMPLATE_DEFAULT = centos
PFTEMPLATE_LIBTOOL = osx
LIB = geeklib
LIBNAME = libgeek.a
BIN1 = mm
BIN2 = memacs
SITEMM = memacs.mm
USITEMM = site.mm
UMM = .memacs
HELP = memacs-help
INSTALL = /usr/local

all: $(LIB) $(BIN2)

$(LIB) $(BIN2): platform
	@cd $@-[0-9]*[0-9] || exit $$?; exec make

platform: $(PFFILE)
	@platform=`cat $?` || exit $$?;\
	for x in $(LIB) $(MDIR); do \
		dir=`echo $$x-[0-9]*[0-9]`;\
		[ -d "$$dir" ] || { echo "Error: $$x directory not found" 1>&2; exit -1; };\
		pdir="$$dir/$$platform";\
		umask 022;\
		for d in $$pdir $$pdir/obj; do \
			if [ ! -d "$$d" ]; then \
				mkdir $$d || exit $$?;\
				echo "Created '$$d' directory." 1>&2;\
			fi;\
		done;\
		if [ ! -f $$pdir/Makefile ]; then \
			if [ $$x = $(LIB) ] && type libtool 1>/dev/null 2>&1; then \
				tplatform=$(PFTEMPLATE_LIBTOOL);\
			else \
				tplatform=$(PFTEMPLATE_DEFAULT);\
			fi;\
			cp "$$dir/$$tplatform/Makefile" $$pdir || exit $$?;\
			echo "Created '$$pdir/Makefile' (from '$$tplatform' platform)." 1>&2;\
		fi;\
	done

$(PFFILE):
	@sed -n -e '1,/^\/\/ OS selection\./d; s/^#define \([^	][^	]*\).*/\1/p; /^$$/q' include/os.h |\
	 tr '[A-Z]' '[a-z]' >$(PFLIST);\
	if [ -z '$(PLATFORM)' ]; then \
		printf 'Platform not specified.  Syntax is:\n\n\t$$ make PLATFORM=<platform>\n\nwhere <platform> is one of:\n' 1>&2;\
		sed -e 's/^/	/' $(PFLIST) 1>&2;\
		echo 1>&2;\
		rm $(PFLIST);\
		exit -1;\
	elif ! egrep -qi "$(PLATFORM)" $(PFLIST); then \
		printf 'Error: Unknown platform "%s".  Platform must be one of:\n' "$(PLATFORM)" 1>&2;\
		sed -e 's/^/	/' $(PFLIST) 1>&2;\
		echo 1>&2;\
		rm $(PFLIST);\
		exit -1;\
	else \
		sed -e "s/^\(#define `echo $(PLATFORM) | tr '[a-z]' '[A-Z]'`		*\)0/\11/" include/os.h >_x-;\
		mv _x- include/os.h || exit $$?;\
		echo "$(PLATFORM)" | tr '[A-Z]' '[a-z]' >$@;\
	fi

install:
	@echo 'Beginning installation ...' 1>&2;\
	umask 022;\
	comp='-C';\
	if [ `id -u` -ne 0 ]; then \
		own= grp= owngrp=;\
		dmode=755 fmode=644;\
	elif [ `uname -s` = Darwin ]; then \
		own=0 grp=80 owngrp='-o 0 -g 80';\
		dmode=755 fmode=644;\
	else \
		own=`stat -c %u '$(INSTALL)'`;\
		grp=`stat -c %g '$(INSTALL)'`;\
		owngrp="-o $$own -g $$grp";\
		eval `stat -c %a '$(INSTALL)' |\
		 sed 's/^/000/; s/^.*\(.\)\(.\)\(.\)\(.\)$$/p3=\1 p2=\2 p1=\3 p0=\4/'`;\
		dmode=$$p3$$p2$$p1$$p0 fmode=$$p3$$(($$p2 & 6))$$(($$p1 & 6))$$(($$p0 & 6));\
		[ $$p3 -gt 0 ] && comp=;\
	fi;\
	cd $(MDIR)-[0-9]*[0-9] || exit $$?;\
	[ -f $(BIN1) ] || { echo "Error: File '`pwd`/$(BIN1)' does not exist" 1>&2; exit -1; };\
	install -v -d $$owngrp -m $$dmode $(INSTALL)/bin 1>&2;\
	install -v $$owngrp -m $$dmode $(BIN1) $(INSTALL)/bin 1>&2;\
	ln -f $(INSTALL)/bin/$(BIN1) $(INSTALL)/bin/$(BIN2);\
	[ -n "$$owngrp" ] && chown $$own:$$grp $(INSTALL)/bin/$(BIN2);\
	chmod $$dmode $(INSTALL)/bin/$(BIN2);\
	install -v -d $$owngrp -m $$dmode $(INSTALL)/etc/memacs.d 1>&2;\
	cd scripts || exit $$?;\
	install -v $$comp $$owngrp -m $$fmode $(SITEMM) $(INSTALL)/etc 1>&2;\
	cd memacs.d || exit $$?;\
	for x in *.mm; do \
		bak=;\
		[ $$x = $(USITEMM) ] && bak=-b;\
		install -v $$comp $$bak $$owngrp -m $$fmode $$x $(INSTALL)/etc/memacs.d 1>&2;\
	done;\
	cd ../../doc || exit $$?;\
	install -v -d $$owngrp -m $$dmode $(INSTALL)/share/man/man1 1>&2;\
	install -v $$comp $$owngrp -m $$fmode $(HELP) $(INSTALL)/etc/memacs.d 1>&2;\
	for x in *.1; do \
		install -v $$comp $$owngrp -m $$fmode $$x $(INSTALL)/share/man/man1 1>&2;\
	done;\
	echo "Done.  MightEMacs files installed in '$(INSTALL)'." 1>&2

uinstall:
	@echo 'Beginning user startup file installation ...' 1>&2;\
	cd $(MDIR)-[0-9]*[0-9]/scripts || exit $$?;\
	install -v -C -b -m 644 $(UMM) ~ 1>&2;\
	echo "Done.  User startup file '$(UMM)' installed in '`cd; pwd`'." 1>&2

clean:
	@for x in $(LIB) $(MDIR); do \
		cd $$x-[0-9]*[0-9] || exit $$?;\
		make $@; cd ..;\
	done;\
	rm -f $(PFFILE) $(PFLIST) 2>/dev/null || :
