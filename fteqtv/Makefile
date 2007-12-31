CC=$(TOOLPREFIX)gcc
AR=$(TOOLPREFIX)ar
RANLIB=$(TOOLPREFIX)ranlib
STRIP=$(TOOLPREFIX)strip

SPIKEISALAZYBUGGERCFLAGS=-Wall -O2
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = netchan.o parse.o msg.o qw.o source.o bsp.o rcon.o mdfour.o crc.o control.o forward.o pmove.o httpsv.o

all: qtv

%.o: %.c
	$(CC) $(SPIKEISALAZYBUGGERCFLAGS) $(CFLAGS) $< -c -o $@

libqtvc:
	$(MAKE) -C libqtvc CC="$(CC)" CFLAGS="$(CFLAGS) $(SPIKEISALAZYBUGGERCFLAGS)" AR="$(AR)" RANLIB="$(RANLIB)"

libqtvc/libqtvc.a: libqtvc

qtv: libqtvc/libqtvc.a $(OBJS) qtv.h
	$(CC) $(SPIKEISALAZYBUGGERCFLAGS) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@.db -lm -lc -L./libqtvc -lqtvc
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

qtv.exe: *.c *.h
	$(MAKE) qtv CFLAGS=-mno-cygwin LDFLAGS="-lwsock32 -lwinmm"
	mv qtv qtv.exe

clean:
	rm -rf qtv qtv.exe qtv.db *.o
	$(MAKE) -C libqtvc clean

.PHONY: libqtvc

