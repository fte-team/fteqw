CC=$(TOOLPREFIX)gcc
AR=$(TOOLPREFIX)ar
RANLIB=$(TOOLPREFIX)ranlib
STRIP=$(TOOLPREFIX)strip

SPIKEISALAZYBUGGERCFLAGS=-Wall -O2
STRIPFLAGS=--strip-unneeded --remove-section=.comment

# 'cause Microsoft suck.
CLIBNAME = c

VPATH=../engine/common

OBJS = netchan.o parse.o msg.o qw.o source.o bsp.o rcon.o mdfour.o crc.o control.o forward.o pmove.o menu.o httpsv.o sha1.o

all: qtv

%.o: %.c
	$(CC) $(SPIKEISALAZYBUGGERCFLAGS) $(CFLAGS) $< -c -o $@

libqtvc:
	$(MAKE) -C libqtvc CC="$(CC)" CFLAGS="$(CFLAGS) $(SPIKEISALAZYBUGGERCFLAGS)" AR="$(AR)" RANLIB="$(RANLIB)"

libqtvc/libqtvc.a: libqtvc

qtv: libqtvc/libqtvc.a $(OBJS) qtv.h
	$(CC) $(SPIKEISALAZYBUGGERCFLAGS) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@.db -lm -l$(CLIBNAME) -Llibqtvc -lqtvc
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

qtv.exe: *.c *.h
	$(MAKE) qtv CFLAGS=-mno-cygwin LDFLAGS="-lwsock32 -lwinmm" CLIBNAME=msvcrt
	mv qtv qtv.exe

clean:
	rm -rf qtv qtv.exe qtv.db *.o
	$(MAKE) -C libqtvc clean

.PHONY: libqtvc

