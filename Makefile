all: lib exe

ZLIB=-lz
LZMA=-llzma

SWFLIB=swflib.a
SWFLIB_DEPS=swf.o swf_lzma_reader.o swf_zlib_reader.o sliding_window.o
SWFLIB_EXTDEPS=$(ZLIB) $(LZMA)

TOOLS=c4_swf2svg c4_swfdump c4_swfjpegs c4_swfunzip

clean:
	rm -fv *.a *.o $(SWFLIB) $(SWFLIB_DEPS) $(TOOLS)

distclean: clean

$(SWFLIB): $(SWFLIB_DEPS)
	rm -fv $@
	ar r $@ $(SWFLIB_DEPS)

.c.o:
	gcc -std=gnu99 -Wall -Wextra -pedantic -c -o $@ $<

c4_swf2svg: c4_swf2svg.o
	gcc -o $@ $< $(SWFLIB) $(SWFLIB_EXTDEPS)

c4_swfdump: c4_swfdump.o
	gcc -o $@ $< $(SWFLIB) $(SWFLIB_EXTDEPS)

c4_swfjpegs: c4_swfjpegs.o
	gcc -o $@ $< $(SWFLIB) $(SWFLIB_EXTDEPS)

c4_swfunzip: c4_swfunzip.o
	gcc -o $@ $< $(SWFLIB) $(SWFLIB_EXTDEPS)

lib: $(SWFLIB)

exe: $(TOOLS)

