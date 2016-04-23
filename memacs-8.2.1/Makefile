# Intermediate Unix makefile for MightEMacs.		Ver. 8.0.0

# Definitions.
PLATFORM_FILE = ../.platform
BIN = mm

all:	memacs link

memacs:
	@platform=`cat $(PLATFORM_FILE)` || exit $$?;\
	if [ ! -f $$platform/$(BIN) ]; then \
		echo 'Building MightEMacs ...' 1>&2;\
		cd $$platform || exit $$?;\
		exec make;\
	fi

link:
	@platform=`cat $(PLATFORM_FILE)` || exit $$?;\
	rm -f $(BIN); ln -s $$platform/$(BIN) $(BIN);\
	echo "'$(BIN)' link file created." 1>&2

clean:
	@for x in *; do \
		if [ -f "$$x/Makefile" ]; then \
			cd $$x || exit $$?;\
			make $@; cd ..;\
			rm -f $(BIN) 2>/dev/null || :;\
			d=`pwd`; echo "'`basename "$$d"`/$$x' binaries deleted." 1>&2;\
		fi;\
	done
