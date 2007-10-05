DO_CC=$(CC) -ggdb -fPIC -Wall $(CFLAGS) -o $@ -c $<
OBJECTS=ezscript.o plugin.o qvm_api.o
HEADERS=../plugin.h
OUTFILE=../ezscriptx86.so
all: $(OBJECTS)
	$(CC) -ggdb -fPIC -shared $(OBJECTS) -o $(OUTFILE)

clean:
	rm -rf $(OBJECTS) $(OUTFILE)

ezscript.o: ezscript.c $(HEADERS)
	$(DO_CC)

plugin.o: ../plugin.c $(HEADERS)
	$(DO_CC)

qvm_api.o: ../qvm_api.c $(HEADERS)
	$(DO_CC)


