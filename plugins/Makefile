
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
AV_VER=ffmpeg-3.2.4
ifeq (0,1)
    AV_ARCHIVEEXT=.z7
    AV_EXTRACT=7z e -y
else
    AV_ARCHIVEEXT=.zip
    AV_EXTRACT=unzip -ju
endif
AV_W32=$(AV_VER)-win32-dev$(AV_ARCHIVEEXT)
AV_W64=$(AV_VER)-win64-dev$(AV_ARCHIVEEXT)
AV_URL32=http://ffmpeg.zeranoe.com/builds/win32/dev/$(AV_W32)
AV_URL64=http://ffmpeg.zeranoe.com/builds/win64/dev/$(AV_W64)
AV_PRE32=$(AV_VER)-win32-dev/
AV_PRE64=$(AV_VER)-win64-dev/
ifeq ($(findstring win,$(FTE_TARGET)),win)
  AV_BASE=$(abspath $(OUT_DIR)/../fte_libav_$(AV_VER))/
endif

ifneq ($(AV_BASE),)
  AV_DEP=$(AV_BASE)libavformat/avformat.h
  AV_CFLAGS=-I$(AV_BASE)
  AV_LDFLAGS=-L$(AV_BASE)lib$(BITS) -lavcodec -lavformat -lavutil -lswscale
else
  AV_LDFLAGS=-lavcodec -lavformat -lavutil -lswscale
endif


AVPLUG_OBJS= avplug/avaudio.c avplug/avencode.c avplug/avdecode.c plugin.c qvm_api.c
ifeq ($(FTE_TARGET),win32)
#NATIVE_PLUGINS+=ffmpeg
endif
ifeq ($(FTE_TARGET),win64)
#NATIVE_PLUGINS+=ffmpeg
endif

$(AV_BASE)libavformat/avformat.h:
	mkdir -p $(AV_BASE)
	cd $(AV_BASE) && wget -N $(AV_URL32)
	mkdir -p $(AV_BASE)libavformat && cd $(AV_BASE)libavformat && $(AV_EXTRACT) ../$(AV_W32) $(AV_PRE32)include/libavformat/* && cd -
	mkdir -p $(AV_BASE)libavcodec  && cd $(AV_BASE)libavcodec  && $(AV_EXTRACT) ../$(AV_W32) $(AV_PRE32)include/libavcodec/*  && cd -
	mkdir -p $(AV_BASE)libavutil   && cd $(AV_BASE)libavutil   && $(AV_EXTRACT) ../$(AV_W32) $(AV_PRE32)include/libavutil/*   && cd -
	mkdir -p $(AV_BASE)libswscale  && cd $(AV_BASE)libswscale  && $(AV_EXTRACT) ../$(AV_W32) $(AV_PRE32)include/libswscale/*  && cd -
	mkdir -p $(AV_BASE)lib32 && cd $(AV_BASE)lib32 && $(AV_EXTRACT) ../$(AV_W32) $(AV_PRE32)lib/avformat.lib $(AV_PRE32)lib/avcodec.lib $(AV_PRE32)lib/avutil.lib $(AV_PRE32)lib/swscale.lib && cd -
	#rm $(AV_BASE)$(AV_W32)
	cd $(AV_BASE) && wget -N $(AV_URL64)
	mkdir -p $(AV_BASE)lib64 && cd $(AV_BASE)lib64 && $(AV_EXTRACT) ../$(AV_W64) $(AV_PRE64)lib/avformat.lib $(AV_PRE64)lib/avcodec.lib $(AV_PRE64)lib/avutil.lib $(AV_PRE64)lib/swscale.lib && cd -
	#rm $(AV_BASE)$(AV_W64)

distclean:
	rm $(AV_BASE)libavformat/avformat.h

$(OUT_DIR)/fteplug_ffmpeg$(PLUG_NATIVE_EXT): $(AV_DEP)

$(OUT_DIR)/fteplug_ffmpeg$(PLUG_NATIVE_EXT): $(AVPLUG_OBJS)
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -s -o $@ -shared $(PLUG_CFLAGS) $(AV_CFLAGS) $(AVPLUG_OBJS) $(PLUG_DEFFILE) $(PLUG_LDFLAGS) $(AV_LDFLAGS)

#small script for ode
#FIXME: ode fails to compile under cygwin
#FIXME: race condition if you try compiling debug+release at the same time, as this makefile is invoked twice by the engine's one
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
	cd $(OUT_DIR)/.. && wget -N $(ODE_URL)
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
	wget -N $(BULLET_URL) -O $@
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


#Regarding CEF Versions: Cef 2526 is reportedly the most recent _WORKING_ version of libcef. Later versions have screwed webgl etc, and are just generally unstable.
#However, that's now impossible to get hold of since the old cefbuilds server went down. New builds are hosted by spotify and they're all randomly broken, so we might as well just use whatever seems fairly recent.

#WARNING: Changing CEF_VER requires updating downloadables.php etc
ifeq ($(FTE_TARGET),win32)
	CEF_ARCH=windows32
endif
ifeq ($(FTE_TARGET),win64)
	CEF_ARCH=windows64
endif
ifeq ($(FTE_TARGET),linux32)
	#CEF_ARCH=linux32
endif
ifeq ($(FTE_TARGET),linux64)
	#CEF_ARCH=linux64
endif
#ifeq ($(FTE_TARGET),macosx64)
	#CEF_ARCH=macosx64
#endif
CEF_VER=3.3163.1666.g052c2d2
CEF_NAME=cef_binary_$(CEF_VER)_$(CEF_ARCH)_minimal
CEF_URL=http://opensource.spotify.com/cefbuilds/$(CEF_NAME).tar.bz2

ifneq ($(CEF_ARCH),)
cef/$(CEF_NAME)/include/cef_version.h:
	cd cef && wget -N $(CEF_URL)
	cd cef && tar -xjf $(CEF_NAME).tar.bz2
cef/$(CEF_NAME)/rel.zip: cef/$(CEF_NAME)/include/cef_version.h
	cd cef/$(CEF_NAME)/Release && zip -9 ../rel.zip *.dll *.bin
	cd cef/$(CEF_NAME)/Resources && zip -r9 ../rel.zip .
$(OUT_DIR)/cef_$(CEF_VER).zip: cef/$(CEF_NAME)/rel.zip
	cp cef/$(CEF_NAME)/rel.zip $@

CEF_SOURCES=cef/cef.c plugin.c qvm_api.c
$(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT): $(CEF_SOURCES) $(OUT_DIR)/cef_$(CEF_VER).zip cef/$(CEF_NAME)/include/cef_version.h
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -DFTEPLUGIN -o $(OUT_DIR)/fteplug_cef$(PLUG_NATIVE_EXT) -shared $(PLUG_CFLAGS) -Icef/$(CEF_NAME) $(CEF_SOURCES) $(PLUG_DEFFILE) $(PLUG_LDFLAGS) -Wl,-rpath,. $(PLUG_LDFLAGS_DL)
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
