# yeah, couldn't do more simple really

CC=gcc
CFLAGS=-g -Wall

default:	q3asm2

q3asm:	q3asm.c cmdlib.c
	$(CC) $(CFLAGS) -o $@ $^

q3asm2:	q3asm2.c ../engine/qclib/hash.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f q3asm2 q3asm *~ *.o

