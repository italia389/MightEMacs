BIN = mm

all:	memacs link

memacs:
	@platform=`cat ../.platform` || exit $$?; \
	cd $$platform || exit $$?; exec make

link:
	@platform=`cat ../.platform` || exit $$?; \
	rm -f $(BIN); ln -s $$platform/$(BIN) $(BIN); \
	echo "'$(BIN)' link file created." 1>&2

strip:
	@platform=`cat ../.platform` || exit $$?; \
	cd $$platform || exit $$?; exec make $@

clean:
	@platform=`cat ../.platform` || exit $$?; \
	cd $$platform || exit $$?; make $@; cd ..; rm -f $(BIN); \
	x=`pwd`; echo "'`basename "$$x"`' binaries deleted." 1>&2
