# Root Unix makefile for MightEMacs.		Ver. 8.1.0.0

# Definitions.
MAKEFLAGS = --no-print-directory
MDIR = memacs
PFFILE = .platform
PFLIST = .platforms
PFTEMPLATE_DEFAULT = redhat
PFTEMPLATE_LIBTOOL = osx
LIB = geeklib
LIBNAME = libgeek.a
BIN1 = mm
BIN2 = memacs
SITEMM = memacs.mm
SITEUSER = site.mm
HELP = memacs-help
INSTALL = /usr/local

strip: $(LIB) $(BIN2)
	@cd $(MDIR)-[0-9]*[0-9] || exit $$?; exec make $@

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
			if [ $$x == $(LIB) ] && type libtool 1>/dev/null 2>&1; then \
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
	umask 022; owngrp=; [ `id -u` -eq 0 ] && owngrp='-o 0 -g 0';\
	cd $(MDIR)-[0-9]*[0-9] || exit $$?;\
	[ -f $(BIN1) ] || { echo "Error: File '`pwd`/$(BIN1)' does not exist" 1>&2; exit -1; };\
	install -v -d $$owngrp -m 755 $(INSTALL)/bin;\
	install -v $$owngrp -m 755 $(BIN1) $(INSTALL)/bin;\
	ln -f $(INSTALL)/bin/$(BIN1) $(INSTALL)/bin/$(BIN2);\
	[ -n "$$owngrp" ] && chown 0:0 $(INSTALL)/bin/$(BIN2);\
	chmod 755 $(INSTALL)/bin/$(BIN2);\
	install -v -d $$owngrp -m 755 $(INSTALL)/etc/memacs.d;\
	cd scripts || exit $$?;\
	install -v -C $$owngrp -m 644 $(SITEMM) $(INSTALL)/etc;\
	cd memacs.d || exit $$?;\
	for x in *.mm; do \
		bak=;\
		[ $$x == $(SITEUSER) ] && bak=-b;\
		install -v -C $$bak $$owngrp -m 644 $$x $(INSTALL)/etc/memacs.d;\
	done;\
	cd ../../doc || exit $$?;\
	install -v -d $$owngrp -m 755 $(INSTALL)/share/man/man1;\
	install -v -C $$owngrp -m 644 $(HELP) $(INSTALL)/etc/memacs.d;\
	for x in *.1; do \
		install -v -C $$owngrp -m 644 $$x $(INSTALL)/share/man/man1;\
	done;\
	echo "Done.  MightEMacs files installed in '$(INSTALL)'." 1>&2

clean:
	@for x in $(LIB) $(MDIR); do \
		cd $$x-[0-9]*[0-9] || exit $$?;\
		make $@; cd ..;\
	done;\
	rm -f $(PFFILE) $(PFLIST) 2>/dev/null || :
