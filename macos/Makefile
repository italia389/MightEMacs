# Unix makefile for MightEMacs.		Ver. 9.5

# Definitions.
BinName = mm
ObjDir = obj
SrcDir = ../src
MemacsInclDir = ../include
LibDir =
CXLib = $(LibDir)/cxlib/libcx.a
XRELib = $(LibDir)/xrelib/libxre.a
StdLibs = -lncurses -lc -lm

ifneq ($(LibDir),)
  InclFlags = -I$(MemacsInclDir) -I$(LibDir)/include
else ifneq ($(LibIncl),)
  InclFlags = -I$(MemacsInclDir) $(LibIncl)
else
  InclFlags = -I$(MemacsInclDir)
endif

# Options and arguments to the C compiler.
CC = cc
CFLAGS = -std=c99 -funsigned-char -Wall -Wextra -Wsometimes-uninitialized -Wunused\
 -Wno-comment -Wno-missing-field-initializers -Wno-missing-braces -Wno-parentheses\
 -Wno-pointer-sign -Wno-unused-parameter $(COPTS)\
 -O2 $(InclFlags)

# List of header files.
HdrFiles =\
 $(MemacsInclDir)/bind.h\
 $(MemacsInclDir)/cmd.h\
 $(MemacsInclDir)/english.h\
 $(MemacsInclDir)/exec.h\
 $(MemacsInclDir)/file.h\
 $(MemacsInclDir)/lang.h\
 $(MemacsInclDir)/search.h\
 $(MemacsInclDir)/std.h\
 $(MemacsInclDir)/var.h

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
.PHONY: all clean

all: $(ObjDir) $(BinName)

$(ObjDir):
	@if [ ! -d $@ ]; then \
		mkdir $@;\
		echo "Created directory '$@'." 1>&2;\
	fi

$(BinName): $(ObjFiles)
	$(CC) $(LDFLAGS) $(ObjFiles) $(XRELib) $(CXLib) $(StdLibs) -o $@
	strip $@

$(ObjDir)/bind.o: $(SrcDir)/bind.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/bind.o $(SrcDir)/bind.c
$(ObjDir)/buffer.o: $(SrcDir)/buffer.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/buffer.o $(SrcDir)/buffer.c
$(ObjDir)/display.o: $(SrcDir)/display.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/display.o $(SrcDir)/display.c
$(ObjDir)/edit.o: $(SrcDir)/edit.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/edit.o $(SrcDir)/edit.c
$(ObjDir)/eval.o: $(SrcDir)/eval.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/eval.o $(SrcDir)/eval.c
$(ObjDir)/exec.o: $(SrcDir)/exec.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/exec.o $(SrcDir)/exec.c
$(ObjDir)/expr.o: $(SrcDir)/expr.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/expr.o $(SrcDir)/expr.c
$(ObjDir)/file.o: $(SrcDir)/file.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/file.o $(SrcDir)/file.c
$(ObjDir)/help.o: $(SrcDir)/help.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/help.o $(SrcDir)/help.c
$(ObjDir)/input.o: $(SrcDir)/input.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/input.o $(SrcDir)/input.c
$(ObjDir)/kill.o: $(SrcDir)/kill.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/kill.o $(SrcDir)/kill.c
$(ObjDir)/main.o: $(SrcDir)/main.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/main.o $(SrcDir)/main.c
$(ObjDir)/misc.o: $(SrcDir)/misc.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/misc.o $(SrcDir)/misc.c
$(ObjDir)/nav.o: $(SrcDir)/nav.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/nav.o $(SrcDir)/nav.c
$(ObjDir)/parse.o: $(SrcDir)/parse.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/parse.o $(SrcDir)/parse.c
$(ObjDir)/region.o: $(SrcDir)/region.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/region.o $(SrcDir)/region.c
$(ObjDir)/replace.o: $(SrcDir)/replace.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/replace.o $(SrcDir)/replace.c
$(ObjDir)/search.o: $(SrcDir)/search.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/search.o $(SrcDir)/search.c
$(ObjDir)/unix.o: $(SrcDir)/unix.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/unix.o $(SrcDir)/unix.c
$(ObjDir)/var.o: $(SrcDir)/var.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/var.o $(SrcDir)/var.c
$(ObjDir)/vterm.o: $(SrcDir)/vterm.c $(HdrFiles)
	$(CC) $(CFLAGS) -c -o $(ObjDir)/vterm.o $(SrcDir)/vterm.c

clean:
	@rm -f $(BinName) $(ObjDir)/*.o
