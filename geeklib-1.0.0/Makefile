LIB = libgeek.a

all:	geeklib link

geeklib:
	@platform=`cat ../.platform` || exit $$?; \
	cd $$platform || exit $$?; exec make

link:
	@platform=`cat ../.platform` || exit $$?; \
	rm -f $(LIB); ln -s $$platform/$(LIB) $(LIB); \
	echo "'$(LIB)' link file created." 1>&2

clean:
	@platform=`cat ../.platform` || exit $$?; \
	cd $$platform || exit $$?; make $@; cd ..; rm -f $(LIB); \
	x=`pwd`; echo "'`basename "$$x"`' binaries deleted." 1>&2
