# Intermediate Unix makefile for GeekLib.	Ver. 8.0.0.0

# Definitions.
PLATFORM_FILE = ../.platform
LIB = libgeek.a

all:	geeklib link

geeklib:
	@platform=`cat $(PLATFORM_FILE)` || exit $$?;\
	if [ ! -f $$platform/$(LIB) ]; then \
		echo 'Building GeekLib ...' 1>&2;\
		cd $$platform || exit $$?;\
		exec make;\
	fi

link:
	@platform=`cat $(PLATFORM_FILE)` || exit $$?;\
	rm -f $(LIB); ln -s $$platform/$(LIB) $(LIB);\
	echo "'$(LIB)' link file created." 1>&2

clean:
	@for x in *; do \
		if [ -f "$$x/Makefile" ]; then \
			cd $$x || exit $$?;\
			make $@; cd ..;\
			rm -f $(LIB) 2>/dev/null || :;\
			d=`pwd`; echo "'`basename "$$d"`/$$x' binaries deleted." 1>&2;\
		fi;\
	done
