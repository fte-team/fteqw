EXTRACFLAGS=-Wall -O2
CC=gcc $(EXTRACFLAGS)
STRIP=strip

STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = netchan.o parse.o qw.o source.o bsp.o rcon.o

qtv: $(OBJS) qtv.h
	$(CC) $(CFLAGS) $(OBJS) -o $@.db -lm $(LDFLAGS)
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

qtv.exe:
	$(MAKE) qtv CFLAGS=-mno-cygwin LDFLAGS="-lwsock32 -lwinmm"
	mv qtv qtv.exe

clean:
	rm -rf qtv qtv.db *.o
