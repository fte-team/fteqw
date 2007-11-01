QCC_OBJS=qccmain.o qcc_cmdlib.o qcc_pr_comp.o qcc_pr_lex.o comprout.o hash.o qcd_main.o
GTKGUI_OBJS=qcc_gtk.o qccguistuff.o
WIN32GUI_OBJS=qccgui.o qccguistuff.o
LIB_OBJS=

CC=gcc -Wall

DO_CC=$(CC) $(BASE_CFLAGS) -o $@ -c $< $(CFLAGS)

all: qcc

USEGUI_CFLAGS=
# set to -DUSEGUI when compiling the GUI
BASE_CFLAGS=-ggdb -DQCCONLY $(USEGUI_CFLAGS)

lib: 

R_win_nocyg: $(QCC_OBJS) $(WIN32GUI_OBJS)
	$(CC) $(BASE_CFLAGS) -o fteqcc.exe -O3 -s $(QCC_OBJS) $(WIN32GUI_OBJS) -mno-cygwin -mwindows -lcomctl32
R_nocyg: $(QCC_OBJS) $(WIN32GUI_OBJS)
	$(CC) $(BASE_CFLAGS) -o fteqcc.exe -O3 -s $(QCC_OBJS) $(WIN32GUI_OBJS) -mno-cygwin -lcomctl32
R_win: $(QCC_OBJS) $(WIN32GUI_OBJS)
	$(CC) $(BASE_CFLAGS) -o fteqcc.exe -O3 -s $(QCC_OBJS) $(WIN32GUI_OBJS) -mwindows -lcomctl32
win_nocyg:
	$(MAKE) USEGUI_CFLAGS=-DUSEGUI R_win_nocyg
nocyg:
	$(MAKE) USEGUI_CFLAGS=-DUSEGUI R_nocyg
win:
	$(MAKE) USEGUI_CFLAGS=-DUSEGUI R_win

qcc: $(QCC_OBJS)
	$(CC) $(BASE_CFLAGS) -o fteqcc.bin -O3 -s $(QCC_OBJS)

qccmain.o: qccmain.c qcc.h
	$(DO_CC)

qcc_cmdlib.o: qcc_cmdlib.c qcc.h
	$(DO_CC)

qcc_pr_comp.o: qcc_pr_comp.c qcc.h
	$(DO_CC)

qcc_pr_lex.o: qcc_pr_lex.c qcc.h
	$(DO_CC)

comprout.o: comprout.c qcc.h
	$(DO_CC)

hash.o: hash.c qcc.h
	$(DO_CC)

qcd_main.o: qcd_main.c qcc.h
	$(DO_CC)

qccguistuff.o: qccguistuff.c qcc.h
	$(DO_CC)

qcc_gtk.o: qcc_gtk.c qcc.h
	$(DO_CC) `pkg-config --cflags gtk+-2.0`

R_gtkgui: $(QCC_OBJS) $(GTKGUI_OBJS)
	$(CC) $(BASE_CFLAGS) -DQCCONLY -DUSEGUI -o fteqccgui.bin -O3 $(GTKGUI_OBJS) $(QCC_OBJS) `pkg-config --libs gtk+-2.0`
gtkgui:
	$(MAKE) USEGUI_CFLAGS=-DUSEGUI R_gtkgui

clean:
	$(RM) fteqcc.bin fteqcc.exe $(QCC_OBJS) $(GTKGUI_OBJS) $(WIN32GUI_OBJS)
