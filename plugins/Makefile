
all: ezscript hud irc

clean: ezscript-clean hud-clean irc-clean

.PHONY: all ezscript hud irc

help:
	@-echo make a subdirectory

ezscript:
	$(MAKE) -C ezscript

ezscript-clean:
	$(MAKE) clean -C ezscript

hud:
	$(MAKE) -C hud

hud-clean:
	$(MAKE) clean -C hud

irc:
	$(MAKE) -C irc

irc-clean:
	$(MAKE) clean -C irc
	
native:
	@echo outdir = $(OUT_DIR)
	$(CC) -o $(OUT_DIR)/fteplug_avplugx86.dll -shared -Iavplug -Iavplug/libavformat -Iavplug/libavcodec -Iavplug/libavutil -Iavplug/libswscale -Iavplug/msvc_lib avplug/avencode.c avplug/avdecode.c plugin.def plugin.c -Lavplug/lib32 -lavcodec -lavformat -lavutil -lswscale -lwinmm
	$(CC) $(BASE_CFLAGS) -DFTEPLUGIN -o $(OUT_DIR)/fteplug_mpqx86.dll -shared -Impq mpq/fs_mpq.c mpq/blast.c plugin.def plugin.c qvm_api.c -Lavplug/lib32 -L../engine/libs/mingw-libs -lzlib