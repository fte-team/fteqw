QCC_OBJS=qccmain.c qcc_cmdlib.c qcc_pr_comp.c qcc_pr_lex.c comprout.c hash.c qcd_main.c

all: qcc


win_nocyg: $(QCC_OBJS) qccgui.c qccguistuff.c
	$(CC) -DQCCONLY -o fteqcc.exe -O3 -s $(QCC_OBJS) -mno-cygwin -mwindows
nocyg: $(QCC_OBJS) qccgui.c qccguistuff.c
	$(CC) -DQCCONLY -o fteqcc.exe -O3 -s $(QCC_OBJS) -mno-cygwin
win: $(QCC_OBJS) qccgui.c qccguistuff.c
	$(CC) -DQCCONLY -o fteqcc.exe -O3 -s $(QCC_OBJS) -mwindows
qcc: $(QCC_OBJS)
	$(CC) -DQCCONLY -o fteqcc.bin -O3 -s $(QCC_OBJS)
