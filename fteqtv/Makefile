CC=gcc
STRIP=strip

CFLAGS=-Wall -O2
STRIPFLAGS=--strip-unneeded --remove-section=.comment

OBJS = netchan.o parse.o qw.o source.o

qtv: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@.db -lm
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

clean:
	rm -rf qtv qtv.db *.o
