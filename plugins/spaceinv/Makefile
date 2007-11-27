LCC_FLAGS=-DQ3_VM -S -Wf-target=bytecode -Wf-g

qvm: 
	lcc $(LCC_FLAGS) spaceinv.c; lcc $(LCC_FLAGS) ../memory.c -DMEMSIZE=1048576; lcc $(LCC_FLAGS) ../plugin.c; lcc $(LCC_FLAGS) ../qvm_api.c; q3asm -f spaceinv
clean:
	rm -rf $(OBJECTS) $(OUTFILE) *.qvm *.asm
