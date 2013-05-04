
#windows is special as always, but we don't support itanium, and microsoft don't support anything else (not even arm with the nt win32 api)
ifeq ($(FTE_TARGET),win32)
PLUG_NATIVE_EXT=x86.dll
PLUG_LDFLAGS=-Lavplug/lib32 -L../engine/libs/mingw-libs -lzlib
endif
ifeq ($(FTE_TARGET),win64)
PLUG_NATIVE_EXT=amd.dll
PLUG_LDFLAGS=-Lavplug/lib64 -L../engine/libs/mingw64-libs -lz -Wl,--support-old-code
endif

PLUG_LDFLAGS?=-L/usr/local/lib -Wl,-R/usr/local/lib -lz

ifneq ($(PLUG_NATIVE_EXT),)
#if we're on windows, we'll put our windows-specific hacks here.
PLUG_DEFFILE=plugin.def
PLUG_CFLAGS=

$(OUT_DIR)/fteplug_avplug$(PLUG_NATIVE_EXT): avplug/libavformat/avformat.h
endif

#if they're not on windows, we'll try asking the compiler directly
#the check to see if its already set is to avoid asking msvc, which would probably break things.
ifeq ($(PLUG_NATIVE_EXT),)
ifneq ($(shell echo|$(CC) -E -dM -|grep __amd64__),)
PLUG_NATIVE_EXT=amd.so
endif
ifneq ($(shell echo|$(CC) -E -dM -|grep __i386__),)
PLUG_NATIVE_EXT=x86.so
endif
ifneq ($(shell echo|$(CC) -E -dM -|grep __arm__),)
PLUG_NATIVE_EXT=arm.so
endif
ifneq ($(shell echo|$(CC) -E -dM -|grep __ppc__),)
PLUG_NATIVE_EXT=ppc.so
endif
endif

#fallback
PLUG_NATIVE_EXT?=unk.so

PLUG_DEFFILE?=
PLUG_CFLAGS?=-fPIC -Wl,--no-undefined
PLUG_LDFLAGS?=

all: ezscript hud irc

clean: ezscript-clean hud-clean irc-clean

.PHONY: all ezscript hud irc native distclean clean

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

#small script to download+install avformat for windows cross compiles.
#linux users are expected to have the library installed locally already. If your version is too old or missing, run the following command to install it (to /usr/local), then delete the gz and directory.
#wget http://ffmpeg.org/releases/ffmpeg-1.2.tar.gz && cd tar xvfz ffmpeg-1.2.tar.gz && cd ffmpeg-1.2/ && ./configure --disable-yasm --enable-shared && make && sudo make install
#we use ffmpeg's version for some reason, as opposed to libav. not sure what the differences are meant to be, but libav seemed to have non-depricated functions defined, docs that say to use them, and these functions missing.
AV7Z_VER=ffmpeg-1.2
AV7Z_W32=$(AV7Z_VER)-win32-dev.7z
AV7Z_URL32=http://ffmpeg.zeranoe.com/builds/win32/dev/$(AV7Z_W32)
AV7Z_PRE32=$(AV7Z_VER)-win32-dev/
AV7Z_W64=$(AV7Z_VER)-win64-dev.7z
AV7Z_URL64=http://ffmpeg.zeranoe.com/builds/win64/dev/$(AV7Z_W64)
AV7Z_PRE64=$(AV7Z_VER)-win64-dev/
avplug/libavformat/avformat.h:
	wget $(AV7Z_URL32)
	mkdir -p avplug/libavformat && cd avplug/libavformat && 7z e -y ../../$(AV7Z_W32) $(AV7Z_PRE32)include/libavformat/ && cd -
	mkdir -p avplug/libavcodec && cd avplug/libavcodec && 7z e -y ../../$(AV7Z_W32) $(AV7Z_PRE32)include/libavcodec/ && cd -
	mkdir -p avplug/libavutil && cd avplug/libavutil && 7z e -y ../../$(AV7Z_W32) $(AV7Z_PRE32)include/libavutil/ && cd -
	mkdir -p avplug/libswscale && cd avplug/libswscale && 7z e -y ../../$(AV7Z_W32) $(AV7Z_PRE32)include/libswscale/ && cd -
	mkdir -p avplug/lib32 && cd avplug/lib32 && 7z e -y ../../$(AV7Z_W32) $(AV7Z_PRE32)lib/avformat.lib $(AV7Z_PRE32)lib/avcodec.lib $(AV7Z_PRE32)lib/avutil.lib $(AV7Z_PRE32)lib/swscale.lib && cd -
	rm $(AV7Z_W32)
	wget $(AV7Z_URL64)
	mkdir -p avplug/lib64 && cd avplug/lib64 && 7z e -y ../../$(AV7Z_W64) $(AV7Z_PRE64)lib/avformat.lib $(AV7Z_PRE64)lib/avcodec.lib $(AV7Z_PRE64)lib/avutil.lib $(AV7Z_PRE64)lib/swscale.lib && cd -
	rm $(AV7Z_W64)
distclean:
	rm avplug/libavformat/avformat.h

$(OUT_DIR)/fteplug_avplug$(PLUG_NATIVE_EXT): avplug/avencode.c avplug/avdecode.c plugin.c
	$(CC) $(BASE_CFLAGS) -DFTEPLUGIN -s -o $(OUT_DIR)/fteplug_avplug$(PLUG_NATIVE_EXT) -shared $(PLUG_CFLAGS) -Iavplug/msvc_lib $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS) -lavcodec -lavformat -lavutil -lswscale
native: $(OUT_DIR)/fteplug_avplug$(PLUG_NATIVE_EXT)

$(OUT_DIR)/fteplug_mpq$(PLUG_NATIVE_EXT): mpq/fs_mpq.c mpq/blast.c plugin.c qvm_api.c
	$(CC) $(BASE_CFLAGS) -DFTEPLUGIN -o $(OUT_DIR)/fteplug_mpq$(PLUG_NATIVE_EXT) -shared $(PLUG_CFLAGS) -Impq $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS)
native: $(OUT_DIR)/fteplug_mpq$(PLUG_NATIVE_EXT)

native: 
