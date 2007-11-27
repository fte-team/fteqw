LCC_FLAGS=-DQ3_VM -S -Wf-target=bytecode -Wf-g

qvm: 
	lcc $(LCC_FLAGS) jabberclient.c; lcc $(LCC_FLAGS) ../memory.c; lcc $(LCC_FLAGS) ../plugin.c; lcc $(LCC_FLAGS) ../qvm_api.c; q3asm -f jabbercl
clean:
	rm -rf $(OBJECTS) $(OUTFILE) *.qvm *.asm

all: native

native:
	gcc -shared -fPIC -ggdb jabberclient.c ../qvm_api.c ../plugin.c -o jabberx86.so

install: native
	cp jabberx86.so /opt/quake/fte/plugins/
	
