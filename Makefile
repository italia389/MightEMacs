MDIR = memacs
PLATFORM = .platform
LIB = geeklib
BIN1 = mm
BIN2 = memacs
SITEMM = memacs.mm
SITEUSER = site.mm
HELP = memacs-help
INSTALL = /usr/local

$(LIB) $(BIN2):
	@[ -f $(PLATFORM) ] || { echo "Error: $(PLATFORM) file does not exist" 1>&2; exit -1; };\
	cd $@-[0-9]*[0-9] || exit $$?; exec make

platform:
	@platform=`cat $(PLATFORM)` || exit $$?;\
	for x in $(LIB) $(MDIR); do \
		dir=`echo $$x-[0-9]*[0-9]`;\
		[ -d "$$dir" ] || { echo "Error: $$x directory not found" 1>&2; exit -1; };\
		dir="$$dir/$$platform";\
		umask 022;\
		for d in $$dir $$dir/obj; do \
			if [ -d "$$d" ]; then \
				echo "Directory $$d already exists (okay)." 1>&2;\
			else \
				mkdir $$d || exit $$?;\
				echo "Created $$d directory." 1>&2;\
			fi;\
		done;\
	done

link:
	@cd $(MDIR)-[0-9]*[0-9] || exit $$?;\
	exec make $@

strip:
	@cd $(MDIR)-[0-9]*[0-9] || exit $$?;\
	exec make $@

install:
	@echo 'Beginning installation ...' 1>&2;\
	umask 022;\
	cd $(MDIR)-[0-9]*[0-9] || exit $$?;\
	[ -f $(BIN1) ] || { echo "Error: File '`pwd`/$(BIN1)' does not exist" 1>&2; exit -1; };\
	install -v -d -o 0 -g 0 -m 755 $(INSTALL)/bin;\
	install -v -o 0 -g 0 -m 755 $(BIN1) $(INSTALL)/bin;\
	ln -f $(INSTALL)/bin/$(BIN1) $(INSTALL)/bin/$(BIN2);\
	chown 0:0 $(INSTALL)/bin/$(BIN2);\
	chmod 755 $(INSTALL)/bin/$(BIN2);\
	install -v -d -o 0 -g 0 -m 755 $(INSTALL)/etc/memacs.d;\
	cd scripts || exit $$?;\
	install -v -C -o 0 -g 0 -m 644 $(SITEMM) $(INSTALL)/etc;\
	cd memacs.d || exit $$?;\
	for x in *.mm; do \
		bak=;\
		[ $$x == $(SITEUSER) ] && bak=-b;\
		install -v -C $$bak -o 0 -g 0 -m 644 $$x $(INSTALL)/etc/memacs.d;\
	done;\
	cd ../../doc || exit $$?;\
	install -v -d -o 0 -g 0 -m 755 $(INSTALL)/share/man/man1;\
	install -v -C -o 0 -g 0 -m 644 $(HELP) $(INSTALL)/etc/memacs.d;\
	for x in *.1; do \
		install -v -C -o 0 -g 0 -m 644 $$x $(INSTALL)/share/man/man1;\
	done;\
	echo "Done.  MightEMacs files installed in '$(INSTALL)'." 1>&2

clean:
	@for x in $(LIB) $(MDIR); do \
		cd $$x-[0-9]*[0-9] || exit $$?;\
		make $@; cd ..;\
	done
