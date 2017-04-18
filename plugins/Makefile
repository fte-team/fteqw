
#windows is special as always, but we don't support itanium, and microsoft don't support anything else (not even arm with the nt win32 api)
ifeq ($(FTE_TARGET),win32)
	PLUG_NATIVE_EXT=_x86.dll
PLUG_LDFLAGS= 
PLUG_LDFLAGS_ZLIB=-L../engine/libs/mingw-libs -lzlib
	BITS=32
	PLUG_LDFLAGS_DL=
endif
ifeq ($(FTE_TARGET),win64)
	PLUG_NATIVE_EXT=_x64.dll
PLUG_LDFLAGS=-Wl,--support-old-code
PLUG_LDFLAGS_ZLIB=-L../engine/libs/mingw64-libs -lz
	BITS=64
	PLUG_LDFLAGS_DL=
endif

PLUG_LDFLAGS_DL?=-ldl
PLUG_LDFLAGS?=-L/usr/local/lib -Wl,-R/usr/local/lib -lm
PLUG_LDFLAGS_ZLIB?=-lz

ifneq ($(PLUG_NATIVE_EXT),)
	#if we're on windows, we'll put our windows-specific hacks here.
	PLUG_DEFFILE=plugin.def
	PLUG_CFLAGS=
	PLUG_CXXFLAGS=
endif

#cygwin uses dll naming.
ifeq ($(FTE_TARGET),cygwin)
	ifeq ($(BITS),64)
		PLUG_DEFFILE=plugin.def
		PLUG_NATIVE_EXT=_amd64.dll
	endif
	ifneq ($(BITS),64)
		PLUG_DEFFILE=plugin.def
		PLUG_NATIVE_EXT=_x86.dll
	endif
endif

#if they're not on windows, we'll try asking the compiler directly
#the check to see if its already set is to avoid asking msvc, which would probably break things.
ifeq ($(PLUG_NATIVE_EXT),)
	LIBRESOLV=-lresolv
	ifneq ($(shell echo|$(CC) -E -dM -|grep __amd64__),)
		#either x32 or x64 ABIs
		ifneq ($(shell echo|$(CC) -E -dM -|grep __ILP32__|grep 1),)
			PLUG_NATIVE_EXT=_x32.so
		else
			PLUG_NATIVE_EXT=_amd64.so
		endif
	endif
	ifneq ($(shell echo|$(CC) -E -dM -|grep __i386__),)
		PLUG_NATIVE_EXT=_x86.so
	endif
	ifneq ($(shell echo|$(CC) -E -dM -|grep __arm__),)
		#gnueabi[hf]
		ifneq ($(shell echo|$(CC) -E -dM -|grep __SOFTFP__),)
			PLUG_NATIVE_EXT=_arm.so
		else
			PLUG_NATIVE_EXT=_armhf.so
		endif
	endif
	ifneq ($(shell echo|$(CC) -E -dM -|grep __ppc__),)
		PLUG_NATIVE_EXT=_ppc.so
	endif
endif

#fallback
PLUG_NATIVE_EXT?=_unk.so

PLUG_DEFFILE?=
PLUG_CFLAGS?=-fPIC -Wl,--no-undefined -Bsymbolic -fvisibility=hidden
PLUG_CXXFLAGS?=-fPIC -Wl,--no-undefined -Bsymbolic -fvisibility=hidden
PLUG_LDFLAGS_ZLIB?=
ARCH:=$(shell $(CC) -dumpmachine)
PLUG_LDFLAGS:=-L../engine/libs-$(ARCH) $(PLUG_LDFLAGS)
PLUG_CFLAGS:=-I../engine/libs-$(ARCH) $(PLUG_CFLAGS)
PLUG_CXXFLAGS:=-I../engine/libs-$(ARCH) $(PLUG_CXXFLAGS)

all: ezscript qi hud irc

clean: ezscript-clean qi-clean hud-clean irc-clean

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

qi:
	$(MAKE) -C qi

qi-clean:
	$(MAKE) clean -C qi

irc:
	$(MAKE) -C irc

irc-clean:
	$(MAKE) clean -C irc

#small script to download+install avformat for windows cross compiles.
#linux users are expected to have the library installed locally already. If your version is too old or missing, run the following command to install it (to /usr/local), then delete the gz and directory.
#wget http://ffmpeg.org/releases/ffmpeg-1.2.tar.gz && cd tar xvfz ffmpeg-1.2.tar.gz && cd ffmpeg-1.2/ && ./configure --disable-yasm --enable-shared && make && sudo make install
#we use ffmpeg's version for some reason, as opposed to libav. not sure what the differences are meant to be, but libav seemed to have non-depricated functions defined, docs that say to use them, and these functions missing.
AV7Z_VER=ffmpeg-3.0.1
AV7Z_W32=$(AV7Z_VER)-win32-dev.7z
AV7Z_URL32=http://ffmpeg.zeranoe.com/builds/win32/dev/$(AV7Z_W32)
AV7Z_PRE32=$(AV7Z_VER)-win32-dev/
AV7Z_W64=$(AV7Z_VER)-win64-dev.7z
AV7Z_URL64=http://ffmpeg.zeranoe.com/builds/win64/dev/$(AV7Z_W64)
AV7Z_PRE64=$(AV7Z_VER)-win64-dev/
AV_BASE=$(OUT_DIR)/../fte_libav_$(AV7Z_VER)/


ifeq ($(FTE_TARGET),win32)
#NATIVE_PLUGINS+=avplug
endif
ifeq ($(FTE_TARGET),win64)
#NATIVE_PLUGINS+=avplug
endif

$(AV_BASE)libavformat/avformat.h:
	mkdir -p $(AV_BASE)
	wget $(AV7Z_URL32) -O $(AV_BASE)$(AV7Z_W32)
	mkdir -p $(AV_BASE)libavformat && cd $(AV_BASE)libavformat && 7z e -y ../$(AV7Z_W32) $(AV7Z_PRE32)include/libavformat/ && cd -
	mkdir -p $(AV_BASE)libavcodec && cd $(AV_BASE)libavcodec && 7z e -y ../$(AV7Z_W32) $(AV7Z_PRE32)include/libavcodec/ && cd -
	mkdir -p $(AV_BASE)libavutil && cd $(AV_BASE)libavutil && 7z e -y ../$(AV7Z_W32) $(AV7Z_PRE32)include/libavutil/ && cd -
	mkdir -p $(AV_BASE)libswscale && cd $(AV_BASE)libswscale && 7z e -y ../$(AV7Z_W32) $(AV7Z_PRE32)include/libswscale/ && cd -
	mkdir -p $(AV_BASE)lib32 && cd $(AV_BASE)lib32 && 7z e -y ../$(AV7Z_W32) $(AV7Z_PRE32)lib/avformat.lib $(AV7Z_PRE32)lib/avcodec.lib $(AV7Z_PRE32)lib/avutil.lib $(AV7Z_PRE32)lib/swscale.lib && cd -
	rm $(AV_BASE)$(AV7Z_W32)
	wget $(AV7Z_URL64) -O $(AV_BASE)$(AV7Z_W64)
	mkdir -p $(AV_BASE)lib64 && cd $(AV_BASE)lib64 && 7z e -y ../$(AV7Z_W64) $(AV7Z_PRE64)lib/avformat.lib $(AV7Z_PRE64)lib/avcodec.lib $(AV7Z_PRE64)lib/avutil.lib $(AV7Z_PRE64)lib/swscale.lib && cd -
	rm $(AV_BASE)$(AV7Z_W64)
distclean:
	rm $(AV_BASE)libavformat/avformat.h
$(OUT_DIR)/fteplug_avplug$(PLUG_NATIVE_EXT): $(AV_BASE)libavformat/avformat.h

$(OUT_DIR)/fteplug_avplug$(PLUG_NATIVE_EXT): avplug/avencode.c avplug/avdecode.c plugin.c qvm_api.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -s -o $@ -shared $(PLUG_CFLAGS) -L$(AV_BASE)lib$(BITS) -I$(AV_BASE) $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS) -lavcodec -lavformat -lavutil -lswscale

#small script for ode
#FIXME: ode fails to compile under cygwin
ODE_ARCH=$(FTE_TARGET)
ifeq ($(ODE_ARCH),)
  ODE_ARCH=unknown
endif
ODE_VER=0.14
ODE_URL=https://bitbucket.org/odedevs/ode/downloads/ode-$(ODE_VER).tar.gz
ODE_BASE=$(OUT_DIR)/../ode-$(ODE_VER)_$(ODE_ARCH)/
ODE_LIB=$(ODE_BASE)ode-$(ODE_VER)/ode/src/.libs/libode.a
$(OUT_DIR)/../ode-$(ODE_VER).tar.gz:
	mkdir -p $(ODE_BASE)
	wget $(ODE_URL) -O $@
$(ODE_LIB): $(OUT_DIR)/../ode-$(ODE_VER).tar.gz
	mkdir -p $(ODE_BASE) && cd $(ODE_BASE) && tar xvfz $<
	cd $(ODE_BASE)ode-$(ODE_VER)/ && ./bootstrap && ./configure --enable-double-precision --disable-demos --without-x --with-pic CC="$(CC) $(PLUG_CXXFLAGS)" CXX="$(CC) $(PLUG_CXXFLAGS)" --host=`$(CC) -dumpmachine` && make

ODE_FILES=../engine/common/com_phys_ode.c ../engine/common/mathlib.c plugin.c qvm_api.c $(ODE_LIB)
$(OUT_DIR)/fteplug_ode$(PLUG_NATIVE_EXT): $(ODE_FILES)
	$(CC) -flto -s $(BASE_CFLAGS) $(CFLAGS) -Os -DFTEPLUGIN -DODE_STATIC -o $@ -shared $(PLUG_CFLAGS) -I$(ODE_BASE)ode-$(ODE_VER)/include $(ODE_FILES) $(PLUG_DEFFILE) $(PLUG_LDFLAGS) -static-libgcc `$(CC) -print-file-name=libstdc++.a` -lpthread
#NATIVE_PLUGINS+=ode


BULLET_VER=2.83.7
BULLET_URL=https://github.com/bulletphysics/bullet3/archive/$(BULLET_VER).tar.gz
BULLET_BASE=$(OUT_DIR)/../bullet3-$(ODE_VER)_$(FTE_TARGET)/
BULLET_LIB=$(ODE_BASE)bullet3-$(ODE_VER)/ode/src/.libs/libode.a
$(OUT_DIR)/../bullet3-$(BULLET_VER).tar.gz:
	mkdir -p $(BULLET_BASE)
	wget $(BULLET_URL) -O $@
$(BULLET_LIB): $(OUT_DIR)/../bullet3-$(BULLET_VER).tar.gz
	mkdir -p $(BULLET_BASE) && cd $(BULLET_BASE) && tar xvfz $<
	cd $(BULLET_BASE)bullet3-$(BULLET_VER)/ && ./bootstrap && ./configure --enable-double-precision --disable-demos --without-x CXX="$(CC)" CFLAGS="$(PLUG_CFLAGS)" CXXFLAGS="$(PLUG_CXXFLAGS)" --host=`$(CC) -dumpmachine` && make


$(OUT_DIR)/fteplug_bullet$(PLUG_NATIVE_EXT): bullet/bulletplug.c plugin.c qvm_api.c $(BULLET_LIB)
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -o $@ -shared $(PLUG_CFLAGS) $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS)
#NATIVE_PLUGINS+=bullet


-include Makefile.private

$(OUT_DIR)/fteplug_mpq$(PLUG_NATIVE_EXT): mpq/fs_mpq.c mpq/blast.c plugin.c qvm_api.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -o $@ -shared $(PLUG_CFLAGS) -Impq $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS_ZLIB) $(PLUG_LDFLAGS)
NATIVE_PLUGINS+=mpq

$(OUT_DIR)/fteplug_xmpp$(PLUG_NATIVE_EXT): jabber/jabberclient.c jabber/jingle.c jabber/sift.c jabber/xml.c plugin.c qvm_api.c ../engine/common/sha1.c emailnot/md5.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -o $@ -shared $(PLUG_CFLAGS) -Ijabber $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS) $(LIBRESOLV)
NATIVE_PLUGINS+=xmpp

$(OUT_DIR)/fteplug_qi$(PLUG_NATIVE_EXT): qi/qi.c jabber/xml.c plugin.c qvm_api.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -o $@ -shared $(PLUG_CFLAGS) -Ijabber $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS)
NATIVE_PLUGINS+=qi

ifeq ($(FTE_TARGET),win32)
#Yes, this is old, but it actually works without the gpu process crashing all the fucking time.
#multiple more recent versions are defective. and they don't support WinXP either.
CEF_VER=win32_2526
endif
ifeq ($(FTE_TARGET),win64)
#I'm just too lazy to test this minor platform.
#CEF_VER=win64_2704
endif

ifneq ($(CEF_VER),)
.PHONEY: $(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT)
$(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT): $(OUT_DIR)/fteplug_cef_$(BITS).zip

$(OUT_DIR)/fteplug_cef_$(BITS).zip: cef/cef.c plugin.c qvm_api.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -o $(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT) -shared $(PLUG_CFLAGS) -Icef/2623 $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS) -Wl,-rpath,. $(PLUG_LDFLAGS_DL)
	cp cef/2623/libcef_$(BITS).zip $@
	chmod 644 $@
	zip -9 -j $@ $(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT)
	rm $(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT)
#NATIVE_PLUGINS+=cef
else
.PHONEY: $(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT)
$(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT):
	echo cef plugin not supported on this arch
endif

#irc plugin can still be built as a qvm.
$(OUT_DIR)/fteplug_irc$(PLUG_NATIVE_EXT): irc/ircclient.c plugin.c qvm_api.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -o $@ -shared $(PLUG_CFLAGS) -Iirc $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS)
NATIVE_PLUGINS+=irc

#for compat with ezquake
$(OUT_DIR)/fteplug_ezhud$(PLUG_NATIVE_EXT): ezhud/ezquakeisms.c ezhud/hud.c ezhud/hud_common.c ezhud/hud_editor.c plugin.c qvm_api.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -o $@ -shared $(PLUG_CFLAGS) -Iezhud $^ $(PLUG_DEFFILE) $(PLUG_LDFLAGS)
NATIVE_PLUGINS+=ezhud

native: $(foreach FOO,$(NATIVE_PLUGINS), $(OUT_DIR)/fteplug_$(FOO)$(PLUG_NATIVE_EXT))
