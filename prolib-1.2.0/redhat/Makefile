# Unix makefile for ProLib.		Ver. 1.2.0

# Definitions.
LIB = libpro.a
SRCD = ../src
OBJD = obj
OBJS =\
 $(OBJD)/array.o\
 $(OBJD)/datum.o\
 $(OBJD)/excep.o\
 $(OBJD)/memzcpy.o\
 $(OBJD)/stplcpy.o\
 $(OBJD)/strfit.o\
 $(OBJD)/vizc.o

INCD = ../../include

# Options and arguments to the C compiler.
CC = cc
CFLAGS = -funsigned-char -W -Wall -Wunused\
 -Wno-comment -Wno-missing-field-initializers -Wno-missing-braces -Wno-pointer-sign -Wno-unused-parameter -O2 -I$(INCD)

$(LIB): $(OBJS)
	-[ -f $(LIB) ] && rm $(LIB)
	@if [ "`uname -s`" = Darwin ]; then \
		echo 'libtool -static -o $(LIB) $(OBJS)' 1>&2;\
		libtool -static -o $(LIB) $(OBJS);\
	else \
		echo 'ar r $(LIB) $(OBJS)' 1>&2;\
		ar r $(LIB) $(OBJS);\
		echo 'ranlib $(LIB)' 1>&2;\
		ranlib $(LIB);\
	fi

$(OBJD)/array.o: $(SRCD)/array.c
	$(CC) -c -o $(OBJD)/array.o $(CFLAGS) $(SRCD)/array.c

$(OBJD)/datum.o: $(SRCD)/datum.c
	$(CC) -c -o $(OBJD)/datum.o $(CFLAGS) $(SRCD)/datum.c

$(OBJD)/excep.o: $(SRCD)/excep.c
	$(CC) -c -o $(OBJD)/excep.o $(CFLAGS) $(SRCD)/excep.c

$(OBJD)/memzcpy.o: $(SRCD)/memzcpy.c
	$(CC) -c -o $(OBJD)/memzcpy.o $(CFLAGS) $(SRCD)/memzcpy.c

$(OBJD)/stplcpy.o: $(SRCD)/stplcpy.c
	$(CC) -c -o $(OBJD)/stplcpy.o $(CFLAGS) $(SRCD)/stplcpy.c

$(OBJD)/strfit.o: $(SRCD)/strfit.c
	$(CC) -c -o $(OBJD)/strfit.o $(CFLAGS) $(SRCD)/strfit.c

$(OBJD)/vizc.o: $(SRCD)/vizc.c
	$(CC) -c -o $(OBJD)/vizc.o $(CFLAGS) $(SRCD)/vizc.c

clean:
	@rm -f $(LIB) $(OBJD)/*.o
