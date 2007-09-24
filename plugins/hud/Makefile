
DO_CC=$(CC) -ggdb -fPIC -Wall $(CFLAGS) -o $@ -c $<
OBJECTS=ui_sbar.o plugin.o qvm_api.o
HEADERS=../plugin.h
all: $(OBJECTS)
	$(CC) -ggdb -fPIC -shared $(OBJECTS) -o ../hudx86.so

ui_sbar.o: ui_sbar.c $(HEADERS)
	$(DO_CC)

plugin.o: ../plugin.c $(HEADERS)
	$(DO_CC)

qvm_api.o: ../qvm_api.c $(HEADERS)
	$(DO_CC)

