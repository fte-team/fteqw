LCC_FLAGS=-DQ3_VM -S -Wf-target=bytecode -Wf-g

qvm: 
	lcc $(LCC_FLAGS) imapnoti.c; lcc $(LCC_FLAGS) md5.c; lcc $(LCC_FLAGS) pop3noti.c; lcc $(LCC_FLAGS) ../memory.c; lcc $(LCC_FLAGS) ../plugin.c; lcc $(LCC_FLAGS) ../qvm_api.c; q3asm -f emailnot
clean:
	rm -rf $(OBJECTS) $(OUTFILE) *.qvm *.asm