CC=gcc
SPIKEISALAZYBUGGERCFLAGS=-Wall -O2
STRIP=strip
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = netchan.o parse.o msg.o qw.o source.o bsp.o rcon.o mdfour.o crc.o control.o forward.o pmove.o httpsv.o

%.o: %.c
	$(CC) $(SPIKEISALAZYBUGGERCFLAGS) $(CFLAGS) $< -c -o $@

qtv: $(OBJS) qtv.h
	$(CC) $(SPIKEISALAZYBUGGERCFLAGS) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@.db -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

qtv.exe: *.c *.h
	$(MAKE) qtv CFLAGS=-mno-cygwin LDFLAGS="-lwsock32 -lwinmm"
	mv qtv qtv.exe

clean:
	rm -rf qtv qtv.exe qtv.db *.o
