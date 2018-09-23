# Unix makefile for MightEMacs.		Ver. 9.0.1

# Definitions.
BinName = mm
ObjDir = obj
SrcDir = ../src
InclDir = ../../include
LibDir = ../../prolib-1.3.0
#Libs = $(LibDir)/libpro.a -ltermcap -lc -lm
Libs = $(LibDir)/libpro.a -lcurses -lc -lm

# Options and arguments to the C compiler.
CC = cc
CFLAGS = -funsigned-char -W -Wall -Wunused\
 -Wno-comment -Wno-missing-field-initializers -Wno-missing-braces -Wno-parentheses\
 -Wno-pointer-sign -Wno-unused-parameter -Wno-unused-result $(COPTS)\
 -O2 -I$(InclDir) -I$(SrcDir)

# List of header files.
HdrFiles =\
 $(SrcDir)/bind.h\
 $(SrcDir)/cmd.h\
 $(SrcDir)/english.h\
 $(SrcDir)/exec.h\
 $(SrcDir)/file.h\
 $(SrcDir)/lang.h\
 $(SrcDir)/search.h\
 $(SrcDir)/std.h\
 $(SrcDir)/var.h

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
$(BinName): $(ObjFiles)
	$(CC) $(LDFLAGS) $(ObjFiles) $(Libs) -o $(BinName)
	strip $(BinName)

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
