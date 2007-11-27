DO_CC=$(CC) -ggdb -fPIC -Wall $(CFLAGS) -o $@ -c $<
OBJECTS=ui_sbar.o plugin.o qvm_api.o
HEADERS=../plugin.h
OUTFILE=../hudx86.so
LCC_FLAGS=-DQ3_VM -S -Wf-target=bytecode -Wf-g
all: $(OBJECTS)
	$(CC) -ggdb -fPIC -shared $(OBJECTS) -o $(OUTFILE)

qvm: 
	lcc $(LCC_FLAGS) ui_sbar.c; lcc $(LCC_FLAGS) ../plugin.c; lcc $(LCC_FLAGS) ../qvm_api.c; q3asm -f qwui
clean:
	rm -rf $(OBJECTS) $(OUTFILE) *.qvm *.asm

ui_sbar.o: ui_sbar.c $(HEADERS)
	$(DO_CC)

plugin.o: ../plugin.c $(HEADERS)
	$(DO_CC)

qvm_api.o: ../qvm_api.c $(HEADERS)
	$(DO_CC)

