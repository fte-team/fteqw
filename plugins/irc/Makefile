DO_CC=$(CC) -fPIC $(CFLAGS) -o $@ -c $<
OBJECTS=ircclient.o plugin.o qvm_api.o
HEADERS=../plugin.h
OUTFILE=../ircx86.so
ASM=q3asm
#ASM=q3asm -vq3 #Uncomment this if you are using Icculus q3asm
LCC=q3lcc
LCC_FLAGS=-DQ3_VM -I/usr/include -S -Wf-target=bytecode -Wf-g
all: $(OBJECTS)
	$(CC) --shared -ldl $(OBJECTS) -o $(OUTFILE)

qvm: 
	$(LCC) $(LCC_FLAGS) ircclient.c; $(LCC) $(LCC_FLAGS) ../plugin.c; $(LCC) $(LCC_FLAGS) ../qvm_api.c; $(ASM) -f ircclient
clean:
	rm -rf $(OBJECTS) $(OUTFILE) *.qvm *.asm

ircclient.o: ircclient.c $(HEADERS)
	$(DO_CC)

plugin.o: ../plugin.c $(HEADERS)
	$(DO_CC)

qvm_api.o: ../qvm_api.c $(HEADERS)
	$(DO_CC)

