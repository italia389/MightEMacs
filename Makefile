# Makefile for MightEMacs.		Ver. 9.6.0

# Definitions.
MAKEFLAGS = --no-print-directory
ProjName = MightEMacs
ProjBaseDir = memacs
ProjDir = share/$(ProjBaseDir)
BinName1 = mm
BinName2 = memacs
ObjDir = obj
SrcDir = src
InclDir = include
LibDir = lib
RootLibDir =
CXL = $(RootLibDir)/cxlib
CXLib = $(CXL)/lib/libcx.a
XRE = $(RootLibDir)/xrelib
XRELib = $(XRE)/lib/libxre.a
InclFlags = -I$(InclDir) -I$(CXL)/include -I$(XRE)/include
Libs = $(CXLib) $(XRELib) -lncurses -lc -lm

BinDir = bin
ManDir = share/man
ManPage = memacs.1
SiteMS = site.ms
UserMS = .memacs
Install1 = /usr/local

# Options and arguments to the C compiler.
CC = cc
CFLAGS = -std=c99 -funsigned-char -Wall -Wextra -Wunused\
 -Wno-comment -Wno-missing-field-initializers -Wno-missing-braces -Wno-parentheses\
 -Wno-pointer-sign -Wno-unused-parameter $(COPTS)\
 -O2 $(InclFlags)

# List of header files.
HdrFiles =\
 $(InclDir)/bind.h\
 $(InclDir)/cmd.h\
 $(InclDir)/english.h\
 $(InclDir)/exec.h\
 $(InclDir)/file.h\
 $(InclDir)/lang.h\
 $(InclDir)/search.h\
 $(InclDir)/std.h\
 $(InclDir)/var.h

# List of object files.
ObjFiles =\
 $(ObjDir)/bind.o\
 $(ObjDir)/buffer.o\
 $(ObjDir)/display.o\
 $(ObjDir)/edit.o\
 $(ObjDir)/eval.o\
 $(ObjDir)/exec.o\
 $(ObjDir)/expr.o\
 $(ObjDir)/file.o\
 $(ObjDir)/help.o\
 $(ObjDir)/input.o\
 $(ObjDir)/kill.o\
 $(ObjDir)/main.o\
 $(ObjDir)/misc.o\
 $(ObjDir)/nav.o\
 $(ObjDir)/parse.o\
 $(ObjDir)/region.o\
 $(ObjDir)/replace.o\
 $(ObjDir)/search.o\
 $(ObjDir)/unix.o\
 $(ObjDir)/var.o\
 $(ObjDir)/vterm.o

# Targets.
.PHONY:	all build-msg uninstall install user-install clean

all: build-msg $(BinName1)

build-msg:
	@if [ ! -f $(BinName1) ]; then \
		echo "Building $(ProjName)..." 1>&2;\
	fi

$(BinName1): $(ObjDir) $(ObjFiles)
	@for lib in "$(CXLib)" "$(XRELib)"; do \
		[ -f "$$lib" ] || { echo "Error: Library '$$lib' does not exist" 1>&2; exit 1; };\
	done
	$(CC) $(LDFLAGS) -o $@ $(ObjFiles) $(Libs)
	strip $@

$(ObjDir):
	@if [ ! -d $@ ]; then \
		mkdir $@ && echo "Created directory '$@'." 1>&2;\
	fi

$(ObjDir)/bind.o: $(SrcDir)/bind.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/bind.c
$(ObjDir)/buffer.o: $(SrcDir)/buffer.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/buffer.c
$(ObjDir)/display.o: $(SrcDir)/display.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/display.c
$(ObjDir)/edit.o: $(SrcDir)/edit.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/edit.c
$(ObjDir)/eval.o: $(SrcDir)/eval.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/eval.c
$(ObjDir)/exec.o: $(SrcDir)/exec.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/exec.c
$(ObjDir)/expr.o: $(SrcDir)/expr.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/expr.c
$(ObjDir)/file.o: $(SrcDir)/file.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/file.c
$(ObjDir)/help.o: $(SrcDir)/help.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/help.c
$(ObjDir)/input.o: $(SrcDir)/input.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/input.c
$(ObjDir)/kill.o: $(SrcDir)/kill.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/kill.c
$(ObjDir)/main.o: $(SrcDir)/main.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/main.c
$(ObjDir)/misc.o: $(SrcDir)/misc.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/misc.c
$(ObjDir)/nav.o: $(SrcDir)/nav.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/nav.c
$(ObjDir)/parse.o: $(SrcDir)/parse.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/parse.c
$(ObjDir)/region.o: $(SrcDir)/region.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/region.c
$(ObjDir)/replace.o: $(SrcDir)/replace.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/replace.c
$(ObjDir)/search.o: $(SrcDir)/search.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/search.c
$(ObjDir)/unix.o: $(SrcDir)/unix.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/unix.c
$(ObjDir)/var.o: $(SrcDir)/var.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/var.c
$(ObjDir)/vterm.o: $(SrcDir)/vterm.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $@ $(SrcDir)/vterm.c

uninstall:
	@echo 'Uninstalling...' 1>&2;\
	if [ -n "$(INSTALL)" ] && [ -x "$(INSTALL)/$(BinDir)/$(BinName2)" ]; then \
		destDir="$(INSTALL)";\
	elif [ -x "$(Install1)/$(BinDir)/$(BinName2)" ]; then \
		destDir=$(Install1);\
	else \
		destDir=;\
	fi;\
	if [ -z "$$destDir" ]; then \
		echo "'$(BinName2)' binary not found." 1>&2;\
	else \
		for f in "$$destDir"/$(BinDir)/$(BinName1) "$$destDir"/$(BinDir)/$(BinName2)\
		 "$$destDir"/$(LibDir)/$(ProjBaseDir) "$$destDir"/$(ProjDir) "$$destDir"/$(ManDir)/man1/$(ManPage); do \
			if [ -e "$$f" ]; then \
				rm -rf "$$f" && echo "Deleted '$$f'" 1>&2;\
			fi;\
		done;\
	fi;\
	echo 'Done.' 1>&2

install: uninstall
	@echo 'Beginning installation...' 1>&2;\
	umask 022;\
	myID=`id -u`;\
	comp='-C' permCheck=true destDir=$(INSTALL);\
	if [ -z "$$destDir" ]; then \
		destDir=$(Install1);\
	else \
		[ $$myID -ne 0 ] && permCheck=false;\
		if [ ! -d "$$destDir" ]; then \
			mkdir "$$destDir" || exit $$?;\
		fi;\
		destDir=`cd "$$destDir"; pwd`;\
	fi;\
	if [ $$permCheck = false ]; then \
		own= grp= owngrp=;\
		dmode=755 fmode=644;\
	else \
		if [ `uname -s` = Darwin ]; then \
			fmt=-f perm='%p';\
		else \
			fmt=-c perm='%a';\
		fi;\
		eval `stat $$fmt 'own=%u grp=%g' "$$destDir"`;\
		if [ $$own -ne $$myID ] && [ $$myID -ne 0 ]; then \
			owngrp=;\
		else \
			owngrp="-o $$own -g $$grp";\
		fi;\
		eval `stat $$fmt $$perm "$$destDir" |\
		 sed 's/^/000/; s/^.*\(.\)\(.\)\(.\)\(.\)$$/p3=\1 p2=\2 p1=\3 p0=\4/'`;\
		dmode=$$p3$$p2$$p1$$p0 fmode=$$p3$$(($$p2 & 6))$$(($$p1 & 6))$$(($$p0 & 6));\
		[ $$p3 -gt 0 ] && comp=;\
	fi;\
	[ -f $(BinName1) ] || { echo "Error: File '`pwd`/$(BinName1)' does not exist" 1>&2; exit 1; };\
	[ -d "$$destDir"/$(BinDir) ] || install -v -d $$owngrp -m $$dmode "$$destDir"/$(BinDir) 1>&2;\
	install -v $$owngrp -m $$dmode $(BinName1) "$$destDir"/$(BinDir) 1>&2;\
	ln -f "$$destDir"/$(BinDir)/$(BinName1) "$$destDir"/$(BinDir)/$(BinName2);\
	[ -n "$$owngrp" ] && chown $$own:$$grp "$$destDir"/$(BinDir)/$(BinName2);\
	chmod $$dmode "$$destDir"/$(BinDir)/$(BinName2);\
	[ -d "$$destDir/$(ProjDir)/scripts" ] ||\
	 install -v -d $$owngrp -m $$dmode "$$destDir/$(ProjDir)/scripts" 1>&2;\
	cd scripts || exit $$?;\
	for x in *.ms; do \
		bak=;\
		[ $$x = $(SiteMS) ] && bak=-b;\
		install -v $$comp $$bak $$owngrp -m $$fmode $$x "$$destDir/$(ProjDir)/scripts" 1>&2;\
	done;\
	cd ..;\
	[ -d "$$destDir"/$(ManDir)/man1 ] || install -v -d $$owngrp -m $$dmode "$$destDir"/$(ManDir)/man1 1>&2;\
	install -v $$comp $$owngrp -m $$fmode doc/$(ManPage) "$$destDir"/$(ManDir)/man1 1>&2;\
	[ -d "$$destDir"/$(ProjDir)/help ] || install -v -d $$owngrp -m $$dmode "$$destDir"/$(ProjDir)/help 1>&2;\
	cd help || exit $$?;\
	for x in *; do \
		install -v $$comp $$owngrp -m $$fmode $$x "$$destDir/$(ProjDir)/help" 1>&2;\
	done;\
	echo "Done.  MightEMacs files installed in '$$destDir'." 1>&2

user-install:
	@echo 'Beginning user startup file installation...' 1>&2;\
	cd scripts || exit $$?;\
	install -v -C -b -m 644 $(UserMS) ~ 1>&2;\
	echo "Done.  User startup file '$(UserMS)' installed in '`cd; pwd`'." 1>&2

clean:
	@rm -f $(BinName1) $(ObjDir)/*.o;\
	echo '$(ProjName) binaries deleted.' 1>&2
