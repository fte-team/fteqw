CFLAGS+=-Wall -O2
CC=gcc
STRIP=strip

STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = netchan.o parse.o msg.o qw.o source.o bsp.o rcon.o mdfour.o crc.o control.o forward.o pmove.o httpsv.o

qtv: $(OBJS) qtv.h
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@.db -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

qtv.exe: *.c *.h
	$(MAKE) qtv CFLAGS=-mno-cygwin LDFLAGS="-lwsock32 -lwinmm"
	mv qtv qtv.exe

clean:
	rm -rf qtv qtv.exe qtv.db *.o
