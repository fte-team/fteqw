DO_CC=$(CC) -fPIC $(CFLAGS) -o $@ -c $<
OBJECTS=ircclient.o plugin.o qvm_api.o
HEADERS=../plugin.h
OUTFILE=../ircx86.so
all: $(OBJECTS)
	gcc --shared -ldl $(OBJECTS) -o $(OUTFILE)

clean:
	rm -rf $(OBJECTS) $(OUTFILE)

ircclient.o: ircclient.c $(HEADERS)
	$(DO_CC)

plugin.o: ../plugin.c $(HEADERS)
	$(DO_CC)

qvm_api.o: ../qvm_api.c $(HEADERS)
	$(DO_CC)

