QCC_OBJS=qccmain.c qcc_cmdlib.c qcc_pr_comp.c qcc_pr_lex.c comprout.c hash.c qcd_main.c

all: qcc



qcc: $(QCC_OBJS)
	$(CC) -DQCCONLY -o fteqcc.bin -O3 -s $(QCC_OBJS)
