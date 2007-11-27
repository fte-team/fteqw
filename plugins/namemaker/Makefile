LCC_FLAGS=-DQ3_VM -S -Wf-target=bytecode -Wf-g

qvm: 
	lcc $(LCC_FLAGS) namemaker.c; lcc $(LCC_FLAGS) ../plugin.c; lcc $(LCC_FLAGS) ../qvm_api.c; q3asm -f namemaker
clean:
	rm -rf $(OBJECTS) $(OUTFILE) *.qvm *.asm
