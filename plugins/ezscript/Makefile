DO_CC=$(CC) -ggdb -fPIC -Wall $(CFLAGS) -o $@ -c $<
OBJECTS=ezscript.o plugin.o qvm_api.o
HEADERS=../plugin.h
OUTFILE=../ezscriptx86.so
LCC_FLAGS=-DQ3_VM -S -Wf-target=bytecode -Wf-g
all: $(OBJECTS)
	$(CC) -ggdb -fPIC -shared $(OBJECTS) -o $(OUTFILE)

clean:
	rm -rf $(OBJECTS) $(OUTFILE) *.qvm *.asm

qvm: 
	lcc $(LCC_FLAGS) ezscript.c; lcc $(LCC_FLAGS) ../plugin.c; lcc $(LCC_FLAGS) ../qvm_api.c; q3asm -f ezscript


ezscript.o: ezscript.c $(HEADERS)
	$(DO_CC)

plugin.o: ../plugin.c $(HEADERS)
	$(DO_CC)

qvm_api.o: ../qvm_api.c $(HEADERS)
	$(DO_CC)


