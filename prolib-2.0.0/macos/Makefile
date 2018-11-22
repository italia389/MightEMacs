# Unix makefile for ProLib.		Ver. 1.3.0

# Definitions.
LibName = libpro.a
SrcDir = ../src
ObjDir = obj
ObjFiles =\
 $(ObjDir)/array.o\
 $(ObjDir)/binsearch.o\
 $(ObjDir)/datum.o\
 $(ObjDir)/excep.o\
 $(ObjDir)/getswitch.o\
 $(ObjDir)/hash.o\
 $(ObjDir)/memzcpy.o\
 $(ObjDir)/prime.o\
 $(ObjDir)/stplcpy.o\
 $(ObjDir)/strfit.o\
 $(ObjDir)/strpspn.o\
 $(ObjDir)/vizc.o

InclDir = ../../include

# Options and arguments to the C compiler.
CC = cc
CFLAGS = -funsigned-char -W -Wall -Wunused\
 -Wno-comment -Wno-missing-field-initializers -Wno-missing-braces -Wno-pointer-sign -Wno-unused-parameter -O2 -I$(InclDir)

$(LibName): $(ObjFiles)
	-[ -f $(LibName) ] && rm $(LibName)
	@if [ "`uname -s`" = Darwin ]; then \
		echo 'libtool -static -o $(LibName) $(ObjFiles)' 1>&2;\
		libtool -static -o $(LibName) $(ObjFiles);\
	else \
		echo 'ar r $(LibName) $(ObjFiles)' 1>&2;\
		ar r $(LibName) $(ObjFiles);\
		echo 'ranlib $(LibName)' 1>&2;\
		ranlib $(LibName);\
	fi

$(ObjDir)/array.o: $(SrcDir)/array.c
	$(CC) -c -o $(ObjDir)/array.o $(CFLAGS) $(SrcDir)/array.c

$(ObjDir)/binsearch.o: $(SrcDir)/binsearch.c
	$(CC) -c -o $(ObjDir)/binsearch.o $(CFLAGS) $(SrcDir)/binsearch.c

$(ObjDir)/datum.o: $(SrcDir)/datum.c
	$(CC) -c -o $(ObjDir)/datum.o $(CFLAGS) $(SrcDir)/datum.c

$(ObjDir)/excep.o: $(SrcDir)/excep.c
	$(CC) -c -o $(ObjDir)/excep.o $(CFLAGS) $(SrcDir)/excep.c

$(ObjDir)/getswitch.o: $(SrcDir)/getswitch.c
	$(CC) -c -o $(ObjDir)/getswitch.o $(CFLAGS) $(SrcDir)/getswitch.c

$(ObjDir)/hash.o: $(SrcDir)/hash.c
	$(CC) -c -o $(ObjDir)/hash.o $(CFLAGS) $(SrcDir)/hash.c

$(ObjDir)/memzcpy.o: $(SrcDir)/memzcpy.c
	$(CC) -c -o $(ObjDir)/memzcpy.o $(CFLAGS) $(SrcDir)/memzcpy.c

$(ObjDir)/prime.o: $(SrcDir)/prime.c
	$(CC) -c -o $(ObjDir)/prime.o $(CFLAGS) $(SrcDir)/prime.c

$(ObjDir)/stplcpy.o: $(SrcDir)/stplcpy.c
	$(CC) -c -o $(ObjDir)/stplcpy.o $(CFLAGS) $(SrcDir)/stplcpy.c

$(ObjDir)/strfit.o: $(SrcDir)/strfit.c
	$(CC) -c -o $(ObjDir)/strfit.o $(CFLAGS) $(SrcDir)/strfit.c

$(ObjDir)/strpspn.o: $(SrcDir)/strpspn.c
	$(CC) -c -o $(ObjDir)/strpspn.o $(CFLAGS) $(SrcDir)/strpspn.c

$(ObjDir)/vizc.o: $(SrcDir)/vizc.c
	$(CC) -c -o $(ObjDir)/vizc.o $(CFLAGS) $(SrcDir)/vizc.c

clean:
	@rm -f $(LibName) $(ObjDir)/*.o
