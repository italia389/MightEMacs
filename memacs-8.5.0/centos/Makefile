# Unix makefile for MightEMacs.		Ver. 8.5.0

# Definitions.
BIN = mm
OBJD = obj
SRCD = ../src
INCD = ../../include
LIBD = ../../prolib-1.2.0
#LIBS = -ltermcap -lc $(LIBD)/libpro.a
#LIBS = -ltermcap -lc $(LIBD)/osx/libpro-debug.a
LIBS = -lcurses -lc $(LIBD)/libpro.a

# Options and arguments to the C compiler.
CC = cc
CFLAGS = -funsigned-char -W -Wall -Wunused\
 -Wno-comment -Wno-missing-field-initializers -Wno-missing-braces -Wno-pointer-sign -Wno-unused-parameter\
 -O2 -I$(INCD) -I$(SRCD)

# List of header files.
HFILES =\
 $(SRCD)/bind.h\
 $(SRCD)/cmd.h\
 $(SRCD)/english.h\
 $(SRCD)/exec.h\
 $(SRCD)/main.h\
 $(SRCD)/std.h\
 $(SRCD)/var.h

# List of object files.
OFILES =\
 $(OBJD)/bind.o\
 $(OBJD)/buffer.o\
 $(OBJD)/display.o\
 $(OBJD)/edit.o\
 $(OBJD)/eval.o\
 $(OBJD)/exec.o\
 $(OBJD)/expr.o\
 $(OBJD)/file.o\
 $(OBJD)/input.o\
 $(OBJD)/kill.o\
 $(OBJD)/main.o\
 $(OBJD)/misc.o\
 $(OBJD)/nav.o\
 $(OBJD)/parse.o\
 $(OBJD)/region.o\
 $(OBJD)/replace.o\
 $(OBJD)/search.o\
 $(OBJD)/unix.o\
 $(OBJD)/var.o\
 $(OBJD)/vterm.o

# Targets.
$(BIN): $(OFILES)
	$(CC) $(LDFLAGS) $(OFILES) $(LIBS) -o $(BIN)
	strip $(BIN)

$(OBJD)/bind.o: $(SRCD)/bind.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/bind.o $(SRCD)/bind.c
$(OBJD)/buffer.o: $(SRCD)/buffer.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/buffer.o $(SRCD)/buffer.c
$(OBJD)/display.o: $(SRCD)/display.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/display.o $(SRCD)/display.c
$(OBJD)/edit.o: $(SRCD)/edit.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/edit.o $(SRCD)/edit.c
$(OBJD)/eval.o: $(SRCD)/eval.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/eval.o $(SRCD)/eval.c
$(OBJD)/exec.o: $(SRCD)/exec.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/exec.o $(SRCD)/exec.c
$(OBJD)/expr.o: $(SRCD)/expr.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/expr.o $(SRCD)/expr.c
$(OBJD)/file.o: $(SRCD)/file.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/file.o $(SRCD)/file.c
$(OBJD)/input.o: $(SRCD)/input.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/input.o $(SRCD)/input.c
$(OBJD)/kill.o: $(SRCD)/kill.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/kill.o $(SRCD)/kill.c
$(OBJD)/main.o: $(SRCD)/main.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/main.o $(SRCD)/main.c
$(OBJD)/misc.o: $(SRCD)/misc.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/misc.o $(SRCD)/misc.c
$(OBJD)/nav.o: $(SRCD)/nav.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/nav.o $(SRCD)/nav.c
$(OBJD)/parse.o: $(SRCD)/parse.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/parse.o $(SRCD)/parse.c
$(OBJD)/region.o: $(SRCD)/region.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/region.o $(SRCD)/region.c
$(OBJD)/replace.o: $(SRCD)/replace.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/replace.o $(SRCD)/replace.c
$(OBJD)/search.o: $(SRCD)/search.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/search.o $(SRCD)/search.c
$(OBJD)/unix.o: $(SRCD)/unix.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/unix.o $(SRCD)/unix.c
$(OBJD)/var.o: $(SRCD)/var.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/var.o $(SRCD)/var.c
$(OBJD)/vterm.o: $(SRCD)/vterm.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $(OBJD)/vterm.o $(SRCD)/vterm.c

clean:
	@rm -f $(BIN) $(OBJD)/*.o
