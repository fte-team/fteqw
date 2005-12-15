all: native

native:
	gcc -shared -fPIC -ggdb jabberclient.c ../qvm_api.c ../plugin.c -o jabberx86.so

install: native
	cp jabberx86.so /opt/quake/fte/plugins/
	
