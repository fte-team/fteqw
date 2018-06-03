CC=gcc
WINDRES=windres
STRIP?=echo strip

STRIPFLAGS=--strip-unneeded --remove-section=.comment

CPUOPTIMIZATIONS=

#canonicalize the source path. except emscripten warns about that like crazy. *sigh*
ifeq ($(FTE_TARGET),web)
	BASE_DIR:=.
else ifeq ($(FTE_TARGET),droid)
	BASE_DIR:=.
else
	BASE_DIR:=$(realpath .)
endif

ifeq ($(SVNREVISION),)
	SVNREVISION:=-DSVNREVISION=$(shell test -d $(BASE_DIR)/../.svn && svnversion $(BASE_DIR) || echo -)
endif
MAKE:=$(MAKE) SVNREVISION=$(SVNREVISION)

WHOAMI:=$(shell whoami)


#update these to download+build a different version. this assumes that the url+subdirs etc contain a consistant version everywhere.
JPEGVER=9b
ZLIBVER=1.2.11
PNGVER=1.6.34
OGGVER=1.3.3
VORBISVER=1.3.5
SDL2VER=2.0.7
SCINTILLAVER=373
OPUSVER=1.2.1
SPEEXVER=1.2.0
SPEEXDSPVER=1.2rc3
FREETYPEVER=2.9

#only limited forms of cross-making is supported
#only the following 3 are supported
#linux->win32 (FTE_TARGET=win32) RPM Package: "mingw32-gcc", DEB Package: "mingw32"
#linux->linux32 (FTE_TARGET=linux32)
#linux->linux64 (FTE_TARGET=linux64)
#linux->morphos (FTE_TARGET=morphos)
#linux->macosx (FTE_TARGET=macosx) or (FTE_TARGET=macosx_x86)
#linux->javascript (FTE_TARGET=web)
#linux->nacl (FTE_TARGET=nacl NARCH=x86_64)
#win32->nacl
#linux->droid (make droid)
#win32->droid (make droid)
#if you are cross compiling, you'll need to use FTE_TARGET=mytarget

#cygwin's make's paths confuses non-cygwin things
RELEASE_DIR=$(BASE_DIR)/release
DEBUG_DIR=$(BASE_DIR)/debug
PROFILE_DIR=$(BASE_DIR)/profile
COMPILE_SYS:=$(shell uname -o 2>&1)
NATIVE_ABSBASE_DIR:=$(realpath $(BASE_DIR))
ifeq ($(COMPILE_SYS),Cygwin)
	OUT_DIR?=.
	NATIVE_OUT_DIR:=$(shell cygpath -m $(OUT_DIR))
	NATIVE_BASE_DIR:=$(shell cygpath -m $(BASE_DIR))
	NATIVE_RELEASE_DIR:=$(shell cygpath -m $(RELEASE_DIR))
	NATIVE_DEBUG_DIR:=$(shell cygpath -m $(DEBUG_DIR))
	NATIVE_ABSBASE_DIR:=$(shell cygpath -m $(NATIVE_ABSBASE_DIR))
endif
NATIVE_OUT_DIR?=$(OUT_DIR)
NATIVE_BASE_DIR?=$(BASE_DIR)
NATIVE_RELEASE_DIR?=$(RELEASE_DIR)
NATIVE_DEBUG_DIR?=$(DEBUG_DIR)

EXE_NAME=fteqw

#include the appropriate games.
ifneq (,$(BRANDING))
	BRANDFLAGS+=-DBRANDING_INC=../game_$(BRANDING).h
	-include game_$(BRANDING).mak
endif
ifneq (,$(FTE_CONFIG))
	export FTE_CONFIG_EXTRA:=$(shell $(CC) -xc -E -P -DCOMPILE_OPTS common/config_$(FTE_CONFIG).h)
	BRANDFLAGS+=-DCONFIG_FILE_NAME=config_$(FTE_CONFIG).h $(FTE_CONFIG_EXTRA)
	EXE_NAME=$(FTE_CONFIG)
endif
ifeq (,$(findstring DNO_SPEEX,$(FTE_CONFIG_EXTRA)))
	USE_SPEEX=1
endif
ifeq (,$(findstring DNO_OPUS,$(FTE_CONFIG_EXTRA)))
	USE_OPUS=1
endif
ifeq (,$(findstring DNO_BOTLIB,$(FTE_CONFIG_EXTRA)))
	USE_BOTLIB=1
endif
ifeq (,$(findstring DNO_VORBISFILE,$(FTE_CONFIG_EXTRA)))
	USE_VORBISFILE=1
endif
#ifeq (,$(findstring DNO_FREETYPE,$(FTE_CONFIG_EXTRA)))
#	USE_FREETYPE=1
#endif
ifneq (,$(findstring -Os,$(FTE_CONFIG_EXTRA)))
	CPUOPTIMIZATIONS+=-Os
	BRANDFLAGS:=$(filter-out -O%,$(BRANDFLAGS))
endif

ifeq ($(BITS),64)
	CC:=$(CC) -m64
endif
ifeq ($(BITS),32)
	CC:=$(CC) -m32
endif

#correct the gcc build when cross compiling
ifneq (,$(findstring win32,$(FTE_TARGET)))
	ifeq ($(shell $(CC) -v 2>&1 | grep mingw),)
		#CC didn't state that it was mingw... so try fixing that up
		#old/original mingw project, headers are not very up to date.
		ifneq ($(shell which i586-mingw32msvc-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=i586-mingw32msvc-gcc
			AR=i586-mingw32msvc-ar
			WINDRES=i586-mingw32msvc-windres
			STRIP=i586-mingw32msvc-strip
#			BITS?=32
		endif
		#mingw64 provides a 32bit toolchain too, which has more up to date header files than the mingw32 project. so favour that if its installed.
		ifneq ($(shell which i686-w64-mingw32-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=i686-w64-mingw32-gcc
			AR=i686-w64-mingw32-ar
			WINDRES=i686-w64-mingw32-windres
			STRIP=i686-w64-mingw32-strip
#			BITS?=32
		endif
	endif
endif

#correct the gcc build when cross compiling
ifneq (,$(findstring win64,$(FTE_TARGET)))
	ifeq ($(shell $(CC) -v 2>&1 | grep mingw),)
		#CC didn't state that it was mingw... so try fixing that up
		ifneq ($(shell which x86_64-w64-mingw32-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=x86_64-w64-mingw32-gcc -m64
			AR=x86_64-w64-mingw32-ar
			WINDRES=x86_64-w64-mingw32-windres
			STRIP=x86_64-w64-mingw32-strip
#			BITS=64
		endif
		ifneq ($(shell which amd64-mingw32msvc-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=amd64-mingw32msvc-gcc -m64
			AR=amd64-mingw32msvc-ar
			WINDRES=amd64-mingw32msvc-windres
			STRIP=amd64-mingw32msvc-strip
#			BITS=64
		endif
	endif
endif

ifeq ($(FTE_TARGET),win32_sdl)
	FTE_TARGET=win32_SDL
endif

USER_TARGET:=$(FTE_TARGET)

#make droid-rel doesn't get the right stuff
#add a small default config file. its only small. and some other stuff, because we can. This makes it much easier to get it up and running.
DROID_PACKSU?= $(BASE_DIR)/droid/fte.cfg $(BASE_DIR)/droid/default.fmf $(BASE_DIR)/droid/configs/touch.cfg
ANDROID_HOME?=~/android-sdk-linux
#ANDROID_NDK_ROOT?=~/android-ndk-r8e
#ANDROID_NDK_ROOT?=$(ANDROID_HOME)/ndk-bundle
ANDROID_NDK_ROOT=$(ANDROID_HOME)/android-ndk-r14b
ANDROID_ZIPALIGN?=$(ZIPALIGN)
ANDROID_ZIPALIGN?=$(ANDROID_HOME)/tools/zipalign
ANT?=ant
JAVA_HOME?=/usr
JAVATOOL=$(JAVA_HOME)/bin/
ANDROID_SCRIPT=android

ifeq ($(DROID_ARCH),)
	#armeabi armeabi-v7a arm64-v8a x86 x86_64 mips mips64
	DROID_ARCH=x86 armeabi-v7a
endif
ifeq ($(FTE_TARGET),droid)
	#figure out the host system, required to find a usable compiler
	ifneq ($(shell uname -o 2>&1 | grep Cygwin),)
#		ifeq ($(shell uname -m 2>&1), i686)
#			ANDROID_HOSTSYSTEM?=windows
#		else
#			ANDROID_HOSTSYSTEM?=windows-$(shell uname -m)
#		endif
		ANDROID_HOSTSYSTEM?=windows-x86_64
	else
		ANDROID_HOSTSYSTEM?=linux-$(shell uname -m)
	endif
	DROID_ABI_VER?=4.9

	#omfg why the FUCK do we need all this bullshit? Why isn't there some sane way to do this that actually works regardless of ndk updates?!?
	#name is some random subdir that someone at google arbitrarily picked
	#arch is some random other name for a group of ABIs...
	#prefix is the 'standard' tupple that the toolchain was compiled to target (by default)
	#ver is whatever gcc version it is, or clang. so yeah, pretty much random.
	#cflags is whatever is needed to actually target that abi properly with the specific toolchain... -m64 etc.
	DROID_ABI_NAME___armeabi=arm-linux-androideabi
	DROID_ABI_PREFIX_armeabi=arm-linux-androideabi
	DROID_ABI_ARCH___armeabi=arm
	DROID_ABI_VER____armeabi?=$(DROID_ABI_VER)
	DROID_ABI_CFLAGS_armeabi=-march=armv5te -mtune=xscale -msoft-float
	DROID_ABI_NAME___armeabi-v7a=$(DROID_ABI_NAME___armeabi)
	DROID_ABI_PREFIX_armeabi-v7a=$(DROID_ABI_PREFIX_armeabi)
	DROID_ABI_ARCH___armeabi-v7a=$(DROID_ABI_ARCH___armeabi)
	DROID_ABI_VER____armeabi-v7a=$(DROID_ABI_VER____armeabi)
	DROID_ABI_CFLAGS_armeabi-v7a=-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16
	DROID_ABI_NAME___arm64-v8a=$(DROID_ABI_NAME___armeabi)
	DROID_ABI_PREFIX_arm64-v8a=$(DROID_ABI_PREFIX_armeabi)
	DROID_ABI_ARCH___arm64-v8a=$(DROID_ABI_ARCH___armeabi)
	DROID_ABI_VER____arm64-v8a=$(DROID_ABI_VER____armeabi)
	DROID_ABI_CFLAGS_arm64-v8a=-m64
	DROID_ABI_NAME___x86=x86
	DROID_ABI_PREFIX_x86=i686-linux-android
	DROID_ABI_ARCH___x86=x86
	DROID_ABI_VER____x86?=$(DROID_ABI_VER)
	DROID_ABI_CFLAGS_x86=-march=i686 -mssse3 -mfpmath=sse -m32 -Os
	DROID_ABI_NAME___x86_64=$(DROID_ABI_NAME___x86)
	DROID_ABI_PREFIX_x86_64=$(DROID_ABI_PREFIX_x86)
	DROID_ABI_ARCH___x86_64=$(DROID_ABI_ARCH___x86)
	DROID_ABI_VER____x86_64=$(DROID_ABI_VER____x86)
	DROID_ABI_CFLAGS_x86_64=-march=x86-64 -msse4.2 -mpopcnt -m64 -Os
	#DROID_ABI_NAME___mips=mipsel-linux-android
	#DROID_ABI_PREFIX_mips=mipsel-linux-android
	#DROID_ABI_ARCH___mips=mips
	#DROID_ABI_VER____mips?=$(DROID_ABI_VER)
	#DROID_ABI_CFLAGS_mips=
	#DROID_ABI_NAME___mips64=$(DROID_ABI_NAME___mips)
	#DROID_ABI_PREFIX_mips64=$(DROID_ABI_PREFIX_mips)
	#DROID_ABI_ARCH___mips64=$(DROID_ABI_ARCH___mips)
	#DROID_ABI_VER____mips64=$(DROID_ABI_VER____mips)
	#DROID_ABI_CFLAGS_mips64=-m64

	ifeq (1,$(words [$(DROID_ARCH)]))
		#try and make sense of the above nonsense.
		DROID_ABI:=$(DROID_ABI_CFLAGS_$(DROID_ARCH))
		TOOLCHAINPATH:=$(ANDROID_NDK_ROOT)/toolchains/$(DROID_ABI_NAME___$(DROID_ARCH))-$(DROID_ABI_VER____$(DROID_ARCH))/prebuilt/$(ANDROID_HOSTSYSTEM)/bin/
		TOOLCHAIN:=$(TOOLCHAINPATH)$(DROID_ABI_PREFIX_$(DROID_ARCH))-

		#4 is the min that fte requires
		DROID_API_LEVEL?=9
		ifeq ($(DROID_ARCH),x86)
			#google fecked up. anything before api_level 9 will fail to compile on x86
			DROID_API_LEVEL=9
		endif
		DROID_API_NAME?=android-$(DROID_API_LEVEL)
		DROID_PLAT_INC=arch-$(DROID_ABI_ARCH___$(DROID_ARCH))
		DROIDSYSROOT=$(realpath $(ANDROID_NDK_ROOT)/platforms/$(DROID_API_NAME)/$(DROID_PLAT_INC))
		ifeq ($(DROIDSYSROOT),)	#its possible that google removed whatever api we're trying to target, just switch up to the new default.
			BITCHANDMOAN:=$(shell echo targetting \"android-9\" instead of \"android-$(DROID_API_LEVEL)\" 1>&2)
			DROID_API_LEVEL=9
			DROID_API_NAME=android-$(DROID_API_LEVEL)

			DROIDSYSROOT=$(realpath $(ANDROID_NDK_ROOT)/platforms/$(DROID_API_NAME)/$(DROID_PLAT_INC))
			ifeq ($(DROIDSYSROOT),)	#its possible that google removed whatever api we're trying to target, just switch up to the new default.
				BITCHANDMOAN:=$(shell echo $(DROID_API_NAME) not available either - $(DROID_ARCH) 1>&2)
			endif
		endif
		DROIDSYSROOT:=$(DROIDSYSROOT)
	endif

	#if we're running under windows, then we want to run some other binary
	ifeq ($(shell uname -o 2>&1 | grep Cygwin),)
		#set up for linux/mingw
		TOOLOVERRIDES=PATH="/usr/bin:$(realpath $(TOOLCHAINPATH))" CFLAGS=--sysroot="$(realpath $(ANDROID_NDK_ROOT)/platforms/$(DROID_API_NAME)/$(DROID_PLAT_INC))" CPPFLAGS=--sysroot="$(realpath $(ANDROID_NDK_ROOT)/platforms/$(DROID_API_NAME)/$(DROID_PLAT_INC))" 
		CONFIGARGS= --with-sysroot="$(realpath $(ANDROID_NDK_ROOT)/platforms/$(DROID_API_NAME)/$(DROID_PLAT_INC))" 
	else
		#we're running upon cygwin
		#FIXME: support mingw too...

		ANDROID_SCRIPT=android.bat
		#make can't cope with absolute win32 paths in dependancy files
		DEPCC=

		ifneq ($(realpath $(TOOLCHAINPATH)),)	#don't invoke cygpath when realpath returns nothing due to dodgy paths (which happens when stuff isn't set up right for the top-level makefile)
			#configure hates android, with its broken default sysroot and lack of path etc
			DROIDSYSROOT:=$(shell cygpath -m $(DROIDSYSROOT))
			TOOLOVERRIDES=PATH="/usr/bin:$(shell cygpath -u $(realpath $(TOOLCHAINPATH)))" CFLAGS=--sysroot="$(DROIDSYSROOT)" CPPFLAGS=--sysroot="$(DROIDSYSROOT)" 
			CONFIGARGS= --with-sysroot="$(shell cygpath -u $(realpath $(ANDROID_NDK_ROOT)/platforms/$(DROID_API_NAME)/$(DROID_PLAT_INC)))" 
		endif
	endif

	CC:=$(TOOLCHAIN)gcc --sysroot="$(DROIDSYSROOT)" -DANDROID $(DROID_ABI) -fno-strict-aliasing
	DO_LD=$(DO_ECHO) $(CC) -Wl,-soname,libftedroid.so -shared -Wl,--no-undefined -Wl,-z,noexecstack -o $@ $(LTO_LD) $(WCFLAGS) $(BRANDFLAGS) $(CFLAGS) -llog -lc -lm -lz
	LD:=$(TOOLCHAIN)ld
	AR:=$(TOOLCHAIN)ar
	STRIP=$(TOOLCHAIN)strip
endif


ifeq ($(FTE_TARGET),win64_sdl)
	FTE_TARGET=win64_SDL
endif

#crosscompile macosx from linux, default target ppc 32bit
ifeq ($(FTE_TARGET),macosx)
	ifeq ($(shell $(CC) -v 2>&1 | grep apple),)
		ifneq ($(shell which powerpc-apple-darwin8-gcc 2> /dev/null),)
			CC=powerpc-apple-darwin8-gcc
			STRIP=powerpc-apple-darwin8-strip
			#seems, macosx has a more limited version of strip
			STRIPFLAGS=
			BITS=32
			EXTENSION=_ppc
		endif
	endif
endif

ifeq ($(FTE_TARGET),macosx_ppc64)
	ifeq ($(shell $(CC) -v 2>&1 | grep apple),)
		ifneq ($(shell which powerpc-apple-darwin8-gcc 2> /dev/null),)
			FTE_TARGET=macosx
			CC=powerpc-apple-darwin8-gcc -arch ppc64
			STRIP=powerpc-apple-darwin8-strip
			#seems, macosx has a more limited version of strip
			STRIPFLAGS=
			BITS=64
			EXTENSION=_ppc
		endif
	endif
endif

ifeq ($(FTE_TARGET),macosx_x86)
	ifeq ($(shell $(CC) -v 2>&1 | grep apple),)
		ifneq ($(shell which i686-apple-darwin8-gcc 2> /dev/null),)
			FTE_TARGET=macosx
			# i686-apple-darwin8-gcc's default target is i386, powerpc-apple-darwin8-gcc -arch i386 just invokes i686-apple-darwin8-gcc anyway
			CC=i686-apple-darwin8-gcc
			STRIP=i686-apple-darwin8-strip
			#seems, macosx has a more limited version of strip
			STRIPFLAGS=
			EXTENSION=_x86
		endif
	endif
endif

#crosscompile morphos from linux
ifeq ($(FTE_TARGET),morphos)
	ifeq ($(shell $(CC) -v 2>&1 | grep morphos),)
		ifneq ($(shell which ppc-morphos-gcc 2> /dev/null),)
			CC=ppc-morphos-gcc
			#morphos strip has a 'feature', it strips permissions
			STRIP=ppc-morphos-strip
		endif
	endif
endif

ifeq ($(FTE_TARGET),dos)
	#at least from dos.
	CC=i586-pc-msdosdjgpp-gcc
	CFLAGS+=-DNO_ZLIB
endif

#if you have an x86, you can get gcc to build binaries using 3 different ABIs, instead of builds for just the default ABI
ifeq ($(FTE_TARGET),linux32)
	FTE_TARGET=linux
	CC=gcc -m32
	BITS=32
endif
ifeq ($(FTE_TARGET),linuxarmhf)
	#debian's armhf is armv7, but armv6 works on RPI too.
	FTE_TARGET=linux
	CC=arm-linux-gnueabihf-gcc -marm -march=armv6 -mfpu=vfp -mfloat-abi=hard
	BITS=armhf
endif
ifeq ($(FTE_TARGET),linuxx32)
	#note: the x32 abi is still not finished or something.
	#at the current time, you will need to edit your kernel's commandline to allow this stuff to run
	FTE_TARGET=linux
	CC=gcc -mx32
	BITS=x32
endif
ifeq ($(FTE_TARGET),linux64)
	FTE_TARGET=linux
	CC=gcc -m64
	BITS=64
endif
ifeq ($(FTE_TARGET),cygwin)
	FTE_TARGET=cyg
endif


ifeq ($(FTE_TARGET),) 	#user didn't specify prefered target
	ifneq ($(shell uname 2>&1 | grep CYGWIN),)
		FTE_TARGET=cyg
		ANDROID_SCRIPT=android.bat
	endif
	ifneq ($(shell $(CC) -v 2>&1 | grep mingw),)
		FTE_TARGET=win32
	endif
	ifeq ($(FTE_TARGET),) 	#still not set
		UNAME_SYSTEM:=$(shell uname)
		ifeq ($(UNAME_SYSTEM),Linux)
			FTE_TARGET=linux
		endif
		ifeq ($(UNAME_SYSTEM),Darwin)
			FTE_TARGET=macosx
		endif
		ifeq ($(UNAME_SYSTEM),FreeBSD)
			FTE_TARGET=bsd
		endif
		ifeq ($(UNAME_SYSTEM),NetBSD)
			FTE_TARGET=bsd
		endif
		ifeq ($(UNAME_SYSTEM),OpenBSD)
			FTE_TARGET=bsd
		endif
		ifeq ($(UNAME_SYSTEM),MorphOS)
			FTE_TARGET=morphos
		endif
		#else I've no idea what it is you're running
	endif

	FTE_TARGET ?= unk	#so go for sdl.
endif

ifneq ($(shell ls|grep config.h),)
	HAVECONFIG=-DHAVE_CONFIG_H
endif

CLIENT_DIR=$(BASE_DIR)/client
GL_DIR=$(BASE_DIR)/gl
D3D_DIR=$(BASE_DIR)/d3d
VK_DIR=$(BASE_DIR)/vk
SW_DIR=$(BASE_DIR)/sw
SERVER_DIR=$(BASE_DIR)/server
COMMON_DIR=$(BASE_DIR)/common
HTTP_DIR=$(BASE_DIR)/http
#LIBS_DIR=$(BASE_DIR)/libs
LIBS_DIR?=.
PROGS_DIR=$(BASE_DIR)/qclib
NACL_DIR=$(BASE_DIR)/nacl
BOTLIB_DIR=$(BASE_DIR)/botlib

ifeq ($(NOCOMPAT),1)
	NCCFLAGS=-DNOLEGACY -DOMIT_QCC
	NCDIRPREFIX=nc
endif
ALL_CFLAGS=$(HAVECONFIG) $(VISIBILITY_FLAGS) $(BRANDFLAGS) $(CFLAGS) $(BASE_CFLAGS) $(WCFLAGS) $(ARCH_CFLAGS) $(NCCFLAGS) -I$(ARCHLIBS)

#cheap compile-everything-in-one-unit (compile becomes preprocess only)
ifneq ($(WPO),)
	LTO_CC= -E
	LTO_LD= -combine -fwhole-program -x c
	LTO_END=ltoxnone
	LTO_START=ltoxc
endif
#proper/consistant link-time optimisations (requires gcc 4.5+ or so)
ifneq ($(LTO),)
	LTO_CC=-flto -fvisibility=hidden
	LTO_LD=-flto
endif

#DO_ECHO=@echo $< && 
DO_ECHO=@
#DO_ECHO=
DO_CC=$(DO_ECHO) $(CC) $(LTO_CC) $(ALL_CFLAGS) -o $@ -c $<

ifeq ($(FTE_TARGET),vc)
	BASELDFLAGS=
endif
ifeq ($(FTE_TARGET),cyg)
	BASELDFLAGS=-lm
endif
ifeq ($(FTE_TARGET),dos)
	BASELDFLAGS=-lm
endif
ifeq ($(FTE_TARGET),morphos)
	BASELDFLAGS=-lm
endif
ifeq ($(FTE_TARGET),bsd)
	BASELDFLAGS=-lm
	VISIBILITY_FLAGS=-fvisibility=hidden
endif
ifeq ($(FTE_TARGET),linux)
	VISIBILITY_FLAGS=-fvisibility=hidden
endif
ifeq ($(FTE_TARGET),droid)
	VISIBILITY_FLAGS=-fvisibility=hidden
endif
ifeq ($(FTE_TARGET),macosx)
	VISIBILITY_FLAGS=-fvisibility=hidden
endif
BASELDFLAGS ?= -lm -ldl -lpthread

ifeq (win,$(findstring cyg,$(FTE_TARGET))$(findstring win,$(FTE_TARGET)))
	BASELDFLAGS=-lm
	MINGW_LIBS_DIR=$(LIBS_DIR)/mingw-libs

	ifeq ($(shell echo $(FTE_TARGET)|grep -v win64),)
		MINGW_LIBS_DIR=$(LIBS_DIR)/mingw64-libs
	endif

	IMAGELDFLAGS=$(MINGW_LIBS_DIR)/libpng.a $(MINGW_LIBS_DIR)/libz.a $(MINGW_LIBS_DIR)/libjpeg.a
	OGGVORBISLDFLAGS=$(MINGW_LIBS_DIR)/libvorbisfile.a $(MINGW_LIBS_DIR)/libvorbis.a $(MINGW_LIBS_DIR)/libogg.a
endif

#try to statically link
ifeq ($(COMPILE_SYS),Darwin)
	ifneq (,$(findstring SDL,$(FTE_TARGET)))
		IMAGELDFLAGS = $(shell pkg-config libpng --variable=libdir)/libpng.a $(shell pkg-config libjpeg --variable=libdir)/libjpeg.a
		OGGVORBISLDFLAGS = $(shell pkg-config vorbisfile --variable=libdir)/libvorbisfile.a $(shell pkg-config vorbis --variable=libdir)/libvorbis.a $(shell pkg-config ogg --variable=libdir)/libogg.a
	endif
endif
IMAGELDFLAGS ?= -lpng -ljpeg
OGGVORBISLDFLAGS ?= -lvorbisfile -lvorbis -logg
VISIBILITY_FLAGS?=

#BASELDFLAGS=-lm  -lz
XLDFLAGS=-L$(ARCHLIBS) $(IMAGELDFLAGS)

#hack some other arguments based upon the toolchain
ifeq ($(FTE_TARGET),vc)
	WARNINGFLAGS=-W3 -D_CRT_SECURE_NO_WARNINGS
	GNUC_FUNCS=
else
	WARNINGFLAGS=-Wall -Wno-pointer-sign -Wno-unknown-pragmas -Wno-format-zero-length -Wno-strict-aliasing
	GNUC_FUNCS= -Dstrnicmp=strncasecmp -Dstricmp=strcasecmp
endif

SDL_INCLUDES=
#-I$(LIBS_DIR)/sdl/include -I/usr/include/SDL -I$(LIBS_DIR)/sdl/include/SDL
BASE_CFLAGS=$(WARNINGFLAGS) $(GNUC_FUNCS) -I$(CLIENT_DIR) -I$(SERVER_DIR) -I$(COMMON_DIR) -I$(GL_DIR) -I$(D3D_DIR) -I$(PROGS_DIR) -I. -I$(LIBS_DIR) -I$(LIBS_DIR)/dxsdk9/include -I$(LIBS_DIR)/dxsdk7/include $(SDL_INCLUDES) -I./libs/freetype2/include -I./libs/freetype2/include/freetype $(BOTLIB_CFLAGS) $(SVNREVISION)
CLIENT_ONLY_CFLAGS=-DCLIENTONLY
SERVER_ONLY_CFLAGS=-DSERVERONLY
JOINT_CFLAGS=
DEBUG_CFLAGS?=-ggdb -g
DEBUG_CFLAGS+=-DDEBUG
RELEASE_CFLAGS?=-O3 -ffast-math $(CPUOPTIMIZATIONS)

ifeq ($(FTE_TARGET),vc)
	#msvc doesn't do -dumpmachine.
	#we might as well get it to reuse the mingw libraries, if only because that makes those libraries easier to compile...
	ifeq ($(BITS),64)
		ARCH?=x86_64-w64-mingw32
	else
		ARCH?=i686-w64-mingw32
	endif
else
	#some idiot decided that -dumpmachine shouldn't respect -m32 etc.
	#at the same time, -print-multiarch is not present, buggy, or just screwed in many gcc builds (ones that target a single arch will unhelpfully just give an empty string).
	#so try multiarch first, and if that fails risk dumpmachine giving the wrong values.
	#really we want dumpmachine's more specific cpu arch included here, so lets hope that idiot burns for all eternity. or something equally melodramatic.
	ARCH:=$(shell $(CC) -print-multiarch 2>/dev/null)
	ifneq ($(words $(ARCH)),1)
		ARCH:=$(shell $(CC) -dumpmachine 2>/dev/null)
	endif
	#foo:=$(shell echo ARCH is $(ARCH) 1>&2 )
endif
ARCHLIBS=$(NATIVE_ABSBASE_DIR)/libs-$(ARCH)

#incase our compiler doesn't support it (mingw)
ifeq ($(shell LANG=c $(CC) -rdynamic 2>&1 | grep unrecognized),)
	DEBUG_CFLAGS+= -rdynamic
endif

PROFILE_CFLAGS=-pg

DX7SDK=-I./libs/dxsdk7/include/

GLCFLAGS?=-DGLQUAKE
D3DCFLAGS?=-DD3D9QUAKE -DD3D11QUAKE
VKCFLAGS?=-DVKQUAKE
NPFTECFLAGS=-DNPFTE

CLIENT_OBJS = \
	textedit.o	\
	fragstats.o	\
	zqtp.o	\
	cl_demo.o	\
	cl_ents.o	\
	clq2_ents.o	\
	cl_input.o	\
	in_generic.o	\
	cl_main.o	\
	cl_parse.o	\
	cl_pred.o	\
	cl_tent.o	\
	cl_cam.o	\
	cl_screen.o	\
	pr_clcmd.o	\
	cl_ui.o	\
	cl_ignore.o \
	cl_cg.o \
	clq3_parse.o	\
	pr_csqc.o	\
	console.o	\
	image.o	\
	keys.o	\
	menu.o	\
	m_master.o	\
	m_multi.o	\
	m_items.o	\
	m_options.o	\
	m_single.o	\
	m_script.o	\
	m_native.o	\
	m_mp3.o	\
	roq_read.o	\
	clq2_cin.o	\
	r_part.o	\
	p_script.o	\
	p_null.o	\
	p_classic.o	\
	r_partset.o	\
	renderer.o	\
	renderque.o	\
	sbar.o	\
	skin.o	\
	snd_al.o	\
	snd_dma.o	\
	snd_mem.o	\
	snd_mix.o	\
	snd_mp3.o	\
	snd_ov.o	\
	valid.o	\
	vid_headless.o	\
	view.o	\
	wad.o			\
				\
	ftpclient.o		\
				\
				\
	pr_menu.o

VKQUAKE_OBJS =		\
	vk_init.o	\
	vk_backend.o

GLQUAKE_OBJS =		\
	gl_draw.o		\
	gl_backend.o		\
	gl_rmain.o		\
	gl_rmisc.o		\
	gl_rsurf.o		\
	gl_screen.o		\
	gl_bloom.o		\
	gl_vidcommon.o		\
	$(VKQUAKE_OBJS)

D3DQUAKE_OBJS =		\
	d3d8_backend.o \
	d3d8_image.o	\
	vid_d3d8.o	\
	d3d_backend.o \
	d3d_image.o	\
	d3d_shader.o	\
	vid_d3d.o	\
	d3d11_backend.o \
	d3d11_image.o	\
	d3d11_shader.o	\
	vid_d3d11.o
	
D3DGL_OBJS =		\
	gl_font.o \
	gl_ngraph.o		\
	gl_shader.o	\
	gl_shadow.o	\
	gl_rlight.o	\
	gl_warp.o	\
	ltface.o	\
	r_surf.o	\
	r_2d.o

MP3_OBJS =			\
	fixed.o		\
	bit.o			\
	timer.o		\
	stream.o		\
	frame.o		\
	synth.o		\
	decoder.o		\
	layer12.o		\
	layer3.o		\
	huffman.o		\
	mymad.o

QCC_OBJS=	\
	comprout.o		\
	hash.o		\
	qcc_cmdlib.o	\
	qccmain.o		\
	qcc_pr_comp.o	\
	qcc_pr_lex.o	\
	qcd_main.o
PROGS_OBJS =		\
	$(QCC_OBJS)	\
	initlib.o		\
	pr_bgcmd.o		\
	pr_skelobj.o		\
	pr_edict.o		\
	pr_exec.o		\
	pr_multi.o		\
	pr_x86.o		\
	qcdecomp.o

SERVER_OBJS = 		\
	pr_cmds.o 		\
	pr_q1qvm.o	\
	pr_lua.o	\
	sv_master.o 	\
	sv_init.o 		\
	sv_main.o 		\
	sv_nchan.o 		\
	sv_ents.o 		\
	sv_send.o 		\
	sv_user.o		\
	sv_sql.o		\
	sv_mvd.o		\
	sv_ccmds.o 		\
	sv_cluster.o		\
	sv_rankin.o 	\
	sv_chat.o 		\
	sv_demo.o		\
	net_preparse.o 	\
	savegame.o		\
	svq2_ents.o 	\
	svq2_game.o 	\
	svq3_game.o	\
	webgen.o		\
	ftpserver.o		\
	httpserver.o

SERVERONLY_OBJS =		\
	sv_sys_unix.o		\
	sys_linux_threads.o

WINDOWSSERVERONLY_OBJS = \
	net_ssl_winsspi.o \
	sv_sys_win.o	\
	sys_win_threads.o

WINDOWS_OBJS = \
	snd_win.o \
	snd_directx.o \
	snd_xaudio.o \
	snd_wasapi.o \
	cd_win.o \
	fs_win32.o \
	in_win.o \
	sys_win.o \
	sys_win_threads.o \
	net_ssl_winsspi.o \
	$(LTO_END) resources.o $(LTO_START)

COMMON_OBJS = \
	gl_alias.o		\
	gl_hlmdl.o	\
	gl_heightmap.o		\
	gl_model.o	\
	com_mesh.o	\
	common.o 		\
	cvar.o 		\
	cmd.o 		\
	crc.o 		\
	net_ssl_gnutls.o \
	net_master.o	\
	fs.o			\
	fs_stdio.o		\
	fs_pak.o		\
	fs_zip.o		\
	fs_dzip.o		\
	fs_xz.o		\
	m_download.o	\
	mathlib.o 		\
	huff.o		\
	md4.o 		\
	sha1.o		\
	log.o 		\
	net_chan.o 		\
	net_wins.o 		\
	net_ice.o 		\
	httpclient.o 	\
	zone.o 		\
	qvm.o	\
	r_d3.o	\
	gl_q2bsp.o		\
	glmod_doom.o 	\
	q3common.o	\
	world.o 		\
	sv_phys.o 		\
	sv_move.o 		\
	pmove.o		\
	pmovetst.o		\
	iwebiface.o		\
	translate.o		\
	plugin.o		\
	q1bsp.o		\
	q2pmove.o

ifeq (1,$(USE_BOTLIB))
	BOTLIB_CFLAGS=-I$(BOTLIB_DIR) -DBOTLIB -DBOTLIB_STATIC
	BOTLIB_OBJS = 			\
		be_aas_bspq3.o		\
		be_aas_cluster.o	\
		be_aas_debug.o		\
		be_aas_entity.o		\
		be_aas_file.o		\
		be_aas_main.o		\
		be_aas_move.o		\
		be_aas_optimize.o	\
		be_aas_reach.o		\
		be_aas_route.o		\
		be_aas_routealt.o	\
		be_aas_sample.o		\
		be_ai_char.o		\
		be_ai_chat.o		\
		be_ai_gen.o		\
		be_ai_goal.o		\
		be_ai_move.o		\
		be_ai_weap.o		\
		be_ai_weight.o		\
		be_ea.o			\
		be_interface.o		\
		l_crc.o			\
		l_libvar.o		\
		l_log.o			\
		l_memory.o		\
		l_precomp.o		\
		l_script.o		\
		l_struct.o	
endif

ifeq (1,$(USE_OPUS))
	LIBOPUS_STATIC=-DOPUS_STATIC
	LIBOPUS_LDFLAGS=-lopus
endif
ifeq (1,$(USE_SPEEX))
	LIBSPEEX_STATIC=-DSPEEX_STATIC
	LIBSPEEX_LDFLAGS=-lspeex -lspeexdsp
endif

ifeq (1,$(USE_VORBISFILE))
	OGGVORBISFILE_STATIC=-DLIBVORBISFILE_STATIC
else
	OGGVORBISLDFLAGS=
	OGGVORBISFILE_STATIC=
endif
ifeq (1,$(USE_FREETYPE))
	LIBFREETYPE_STATIC=-DFREETYPE_STATIC
	LIBFREETYPE_LDFLAGS=-lfreetype
endif

COMMONLIBFLAGS=
CLIENTLIBFLAGS=$(COMMONLIBFLAGS) $(LIBOPUS_STATIC) $(LIBSPEEX_STATIC) $(OGGVORBISFILE_STATIC) $(LIBFREETYPE_STATIC)
SERVERLIBFLAGS=$(COMMONLIBFLAGS)
COMMONLDDEPS=
CLIENTLDDEPS=$(COMMONLDDEPS) $(LIBOPUS_LDFLAGS) $(LIBSPEEX_LDFLAGS) $(OGGVORBISLDFLAGS) $(LIBFREETYPE_LDFLAGS)
SERVERLDDEPS=$(COMMONLDDEPS)

#the defaults for sdl come first
#CC_MACHINE:=$(shell $(CC) -dumpmachine)
ifeq ($(FTE_TARGET),SDL2)
	SDLCONFIG?=sdl2-config
	FTE_FULLTARGET?=sdl2$(BITS)
endif
ifeq ($(FTE_TARGET),SDL1)
	SDLCONFIG?=sdl-config
	FTE_FULLTARGET?=sdl1$(BITS)
endif
ifeq ($(FTE_TARGET),SDL)
	FTE_FULLTARGET?=sdl$(BITS)
endif
SDLCONFIG?=sdl-config
FTE_FULLTARGET?=sdl$(FTE_TARGET)$(BITS)

GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o
GL_EXE_NAME=../$(EXE_NAME)-gl$(FTE_FULLTARGET)
GLCL_EXE_NAME=../$(EXE_NAME)cl-gl$(FTE_FULLTARGET)

#SDLCONFIG:=libs/sdl2_mingw/$(CC_MACHINE)/bin/sdl2-config --prefix=libs/sdl2_mingw/$(CC_MACHINE)
ifdef windir
	GL_LDFLAGS=$(GLLDFLAGS) -lmingw32 -lws2_32 `$(SDLCONFIG) --static-libs`
	VK_LDFLAGS=$(VKLDFLAGS) -lmingw32 -lws2_32 `$(SDLCONFIG) --static-libs`
	M_LDFLAGS=$(MLDFLAGS) -lmingw32 -lws2_32 `$(SDLCONFIG) --static-libs`
	SV_LDFLAGS=`$(SDLCONFIG) --static-libs`
else
	GL_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(CLIENTLDDEPS) `$(SDLCONFIG) --static-libs`
	VK_LDFLAGS=$(VKLDFLAGS) $(IMAGELDFLAGS) $(CLIENTLDDEPS) `$(SDLCONFIG) --static-libs`
	M_LDFLAGS=$(MLDFLAGS) $(IMAGELDFLAGS) $(CLIENTLDDEPS) `$(SDLCONFIG) --static-libs`
	SV_LDFLAGS=`$(SDLCONFIG) --static-libs`
endif
GL_CFLAGS=-DFTE_SDL $(GLCFLAGS) `$(SDLCONFIG) --cflags`
GLB_DIR=gl_$(FTE_FULLTARGET)
GLCL_DIR=glcl_$(FTE_FULLTARGET)
SV_DIR?=sv_$(FTE_FULLTARGET)

VKCL_OBJS=$(VKQUAKE_OBJS) $(BOTLIB_OBJS) snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(D3DGL_OBJS) $(LTO_END) 
VK_CFLAGS=-DFTE_SDL $(VKCFLAGS) `$(SDLCONFIG) --cflags`

SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS) $(BOTLIB_OBJS) 
SV_EXE_NAME=../$(EXE_NAME)-sv$(FTE_FULLTARGET)
SV_CFLAGS=-DFTE_SDL `$(SDLCONFIG) --cflags` $(SERVER_ONLY_CFLAGS)

MINGL_DIR=mingl_$(FTE_FULLTARGET)
MINGL_EXE_NAME=../$(EXE_NAME)-mingl$(FTE_FULLTARGET)

MB_DIR=m_$(FTE_FULLTARGET)
M_EXE_NAME=../$(EXE_NAME)-$(FTE_FULLTARGET)
MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o 
M_CFLAGS=-DFTE_SDL $(GLCFLAGS) `$(SDLCONFIG) --cflags`

QCC_DIR=qcc$(BITS)

ifeq (,$(findstring NO_ZLIB,$(CFLAGS)))
	SV_LDFLAGS+=-lz
	GL_LDFLAGS+=-lz
	VK_LDFLAGS+=-lz
	M_LDFLAGS+=-lz
	QCC_LDFLAGS+=-L$(ARCHLIBS) -lz
endif



#specific targets override those defaults as needed.
#google native client
ifeq ($(FTE_TARGET),nacl)
	CLIENTLDDEPS=

	NARCH ?= x86_32
	ifeq ($(shell uname -o 2>&1 | grep Cygwin),)
		MYOS=linux
	else
		MYOS=win
	endif

	CC=
	STRIP=
	NACLLIBC=glibc
	ifeq ($(NARCH),x86_32)
		CC=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_$(NACLLIBC)/bin/i686-nacl-gcc -DNACL -m32
		STRIP=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_$(NACLLIBC)/bin/i686-nacl-strip
		BITS=
		NACLLIBS=$(NACLLIBC)_x86_32/Release
	endif
	ifeq ($(NARCH),x86_64)
		CC=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_$(NACLLIBC)/bin/i686-nacl-gcc -DNACL -m64
		STRIP=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_$(NACLLIBC)/bin/i686-nacl-strip
		BITS=
		NACLLIBS=$(NACLLIBC)_x86_64/Release
	endif
	ifeq ($(NARCH),arm)
		CC=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_arm_$(NACLLIBC)/bin/arm-nacl-gcc -DNACL
		STRIP=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_arm_$(NACLLIBC)/bin/arm-nacl-strip
		BITS=
		NACLLIBS=$(NACLLIBC)_arm/Release
	endif
	ifeq ($(NARCH),pnacl)
		CC=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_pnacl/bin/pnacl-clang -DNACL
		STRIP=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_pnacl/bin/pnacl-strip
		STRIPFLAGS=
		BITS=
		NACLLIBS=pnacl/Release
	endif

	BASELDFLAGS = -lm -lppapi_gles2 -lnosys -lppapi
	IMAGELDFLAGS =

	GL_CFLAGS=$(GLCFLAGS)
	GL_CFLAGS+=-I$(realpath $(NACL_SDK_ROOT)/include)
	BASELDFLAGS+=-L$(realpath $(NACL_SDK_ROOT)/lib/$(NACLLIBS))

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) sys_ppapi.o cd_null.o gl_vidppapi.o fs_ppapi.o snd_ppapi.o 

	GL_LDFLAGS=$(GLLDFLAGS) $(CLIENTLDDEPS)
	M_LDFLAGS=$(GLLDFLAGS) $(CLIENTLDDEPS)
	
	GLB_DIR=gl_nacl_$(NARCH)
	MINGL_DIR=mingl_nacl_$(NARCH)
	ifeq ($(NARCH),pnacl)
		GL_EXE_NAME=../$(EXE_NAME).pexe
		GLCL_EXE_NAME=../$(EXE_NAME)-cl.pexe
		MINGL_EXE_NAME=../$(EXE_NAME)-mingl.pexe
	else
		GL_EXE_NAME=../$(EXE_NAME)-$(NARCH).nexe
		GLCL_EXE_NAME=../$(EXE_NAME)-cl-$(NARCH).nexe
		MINGL_EXE_NAME=../$(EXE_NAME)-mingl-$(NARCH).nexe
	endif
endif

#FTE_TARGET=win32_SDL | FTE_TARGET=win64_SDL (MinGW32 + SDL | MinGW64 + SDL)
ifeq (win_SDL,$(findstring win,$(FTE_TARGET))$(findstring _SDL,$(FTE_TARGET)))
	LIBS_DIR=./libs

	ifneq (,$(findstring win64,$(FTE_TARGET)))
		BITS=64
	endif

	EXEPOSTFIX=.exe

	CC_MACHINE:=$(shell $(CC) -dumpmachine)
	ARCH_PREDEP=$(BASE_DIR)/libs/SDL2-$(SDL2VER)/$(CC_MACHINE)/bin/sdl2-config
	SDLCONFIG=$(ARCH_PREDEP) --prefix=$(BASE_DIR)/libs/SDL2-$(SDL2VER)/$(CC_MACHINE)
	ARCH_CFLAGS=`$(SDLCONFIG) --cflags`
	
	#the defaults for sdl come first
	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(LTO_END) resources.o $(LTO_START)
	GL_EXE_NAME=../$(EXE_NAME)-sdl-gl$(BITS)$(EXEPOSTFIX)
	GLCL_EXE_NAME=../$(EXE_NAME)-sdl-glcl$(BITS)$(EXEPOSTFIX)
	ifdef windir
		GL_LDFLAGS=$(GLLDFLAGS) -lmingw32 -lws2_32 `$(SDLCONFIG) --static-libs`
		VK_LDFLAGS=$(GLLDFLAGS) -lmingw32 -lws2_32 `$(SDLCONFIG) --static-libs`
		M_LDFLAGS=$(MLDFLAGS) -lmingw32 -lws2_32 `$(SDLCONFIG) --static-libs`
		SV_LDFLAGS=$(MINGW_LIBS_DIR)/libz.a -lm -lmingw32 -lws2_32 -lwinmm `$(SDLCONFIG) --static-libs`
		QCC_LDFLAGS=$(MINGW_LIBS_DIR)/libz.a
	else
		GL_LDFLAGS=$(IMAGELDFLAGS) $(CLIENTLDDEPS) -lws2_32 -lmingw32 $(SDL_LDFLAGS) -mwindows -ldxguid -lwinmm -lole32 $(GLLDFLAGS) `$(SDLCONFIG) --libs` 
		VK_LDFLAGS=$(IMAGELDFLAGS) $(CLIENTLDDEPS) -lws2_32 -lmingw32 $(SDL_LDFLAGS) -mwindows -ldxguid -lwinmm -lole32 $(GLLDFLAGS) `$(SDLCONFIG) --libs` 
		M_LDFLAGS=$(IMAGELDFLAGS) $(CLIENTLDDEPS) -lws2_32 -lmingw32 $(SDL_LDFLAGS) -mwindows -ldxguid -lwinmm -lole32 $(MLDFLAGS) `$(SDLCONFIG) --libs`
		SV_LDFLAGS=$(MINGW_LIBS_DIR)/libz.a -lm -lmingw32 -lws2_32 -lwinmm `$(SDLCONFIG) --libs`
		QCC_LDFLAGS=$(MINGW_LIBS_DIR)/libz.a
	endif
	
	GL_CFLAGS=-DFTE_SDL -I$(MINGW_LIBS_DIR)/ -I$(MINGW_LIBS_DIR) -I$(LIBS_DIR) $(GLCFLAGS) $(CLIENTLIBFLAGS) $(DX7SDK)
	
	GLB_DIR=gl_mgw_sdl$(BITS)
	GLCL_DIR=glcl_mgw_sdl$(BITS)
	
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(WINDOWSSERVERONLY_OBJS) $(BOTLIB_OBJS) $(LTO_END) resources.o $(LTO_START)
	SV_EXE_NAME=../$(EXE_NAME)-sdl-sv$(BITS)$(EXEPOSTFIX)
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) -DFTE_SDL

	MINGL_DIR=mingl_sdlwin$(BITS)
	MINGL_EXE_NAME=../$(EXE_NAME)-sdl-mingl$(BITS)$(EXEPOSTFIX)

	MB_DIR=m_mgw_sdl$(BITS)
	M_EXE_NAME=../$(EXE_NAME)-sdl$(BITS)$(EXEPOSTFIX)
#with d3d...
	#MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(D3DQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(LTO_END) resources.o $(LTO_START)
	#M_CFLAGS=$(D3DCFLAGS) $(VKCFLAGS) $(GLCFLAGS) -DFTE_SDL -I$(LIBS_DIR) -I$(LIBS_DIR) -I$(MINGW_LIBS_DIR)/ -I$(MINGW_LIBS_DIR) $(CLIENTLIBFLAGS) $(DX7SDK)
#without d3d...
	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(LTO_END) resources.o $(LTO_START)
	M_CFLAGS=$(VKCFLAGS) $(GLCFLAGS) -DFTE_SDL -I$(LIBS_DIR) -I$(LIBS_DIR) -I$(MINGW_LIBS_DIR)/ -I$(MINGW_LIBS_DIR) $(CLIENTLIBFLAGS) $(DX7SDK)

	D3DCL_OBJS=$(D3DQUAKE_OBJS) $(BOTLIB_OBJS) snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(D3DGL_OBJS) $(LTO_END) resources.o $(LTO_START)
	D3D_EXE_NAME=../$(EXE_NAME)-sdl-d3d$(BITS)$(EXEPOSTFIX)
	D3DCL_EXE_NAME=../$(EXE_NAME)-sdl-d3dcl$(BITS)$(EXEPOSTFIX)
	D3D_LDFLAGS=$(IMAGELDFLAGS) $(CLIENTLDDEPS) -lws2_32 -lmingw32 $(SDL_LDFLAGS) -mwindows -ldxguid -lwinmm -lole32
	D3D_CFLAGS=$(D3DCFLAGS) -DFTE_SDL -DNO_XFLIP -I$(LIBS_DIR) -I$(MINGW_LIBS_DIR)/  -I$(MINGW_LIBS_DIR) $(CLIENTLIBFLAGS) $(DX7SDK)
	D3DB_DIR=sdl_d3d_mgw$(BITS)
	D3DCL_DIR=sdl_d3dcl_mgw$(BITS)


	VKCL_OBJS=$(VKQUAKE_OBJS) $(BOTLIB_OBJS) gl_bloom.o gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(D3DGL_OBJS) $(LTO_END) resources.o $(LTO_START)
	VK_EXE_NAME=../$(EXE_NAME)-sdl-vk$(BITS)$(EXEPOSTFIX)
	VKCL_EXE_NAME=../$(EXE_NAME)-sdl-vkcl$(BITS)$(EXEPOSTFIX)
	VK_CFLAGS=$(VKCFLAGS) -DFTE_SDL -DNO_XFLIP -I$(LIBS_DIR) -I$(MINGW_LIBS_DIR)/  -I$(MINGW_LIBS_DIR) $(CLIENTLIBFLAGS) $(DX7SDK)
	VKB_DIR=sdl_vk_mgw$(BITS)
	VKCL_DIR=sdl_vkcl_mgw$(BITS)
	
	ifeq ($(shell echo $(FTE_TARGET)|grep -E -i -v "win32.*sdl"),)
		GL_CFLAGS+= -D_MINGW_VFPRINTF
		VK_CFLAGS+= -D_MINGW_VFPRINTF
		D3D_CFLAGS+= -D_MINGW_VFPRINTF
		M_CFLAGS+= -D_MINGW_VFPRINTF
	endif
endif

#FTE_TARGET=vc (Visual C)
ifeq ($(FTE_TARGET),vc)
	DEBUG_CFLAGS=
	MSVCDIR=Microsoft Visual Studio 10.0

	ifeq ($(WINRT),1)
		WINDOWSSDKDIR=C:/Program Files (x86)/Windows Kits/8.1

		ifeq ($(BITS),64)
			WINDRES=x86_64-w64-mingw32-windres
			MSVCPATH=C:/Program Files (x86)/$(MSVCDIR)/VC/BIN/amd64/


		else
			WINDRES=i686-w64-mingw32-windres
			MSVCPATH=C:/Program Files (x86)/$(MSVCDIR)/VC/BIN/

			SDKINC=-I"$(WINDOWSSDKDIR)\Include\shared" -I"$(WINDOWSSDKDIR)\Include\um"
			MSVCINC=-I"C:\Program Files (x86)\$(MSVCDIR)\VC\INCLUDE"
#-I"C:\Program Files (x86)\$(MSVCDIR)\VC\ATLMFC\INCLUDE" 
# -I"C:\Program Files (x86)\$(MSVCDIR)\VC\PlatformSDK\include" -I"C:\Program Files (x86)\$(MSVCDIR)\SDK\v2.0\include"

			MSVCLIB=/LIBPATH:"C:\Program Files (x86)\$(MSVCDIR)\VC\ATLMFC\LIB" /LIBPATH:"C:\Program Files (x86)\$(MSVCDIR)\VC\LIB" /LIBPATH:"$(WINDOWSSDKDIR)/lib/winv6.3/um/x86" /LIBPATH:"C:\Program Files (x86)\$(MSVCDIR)\SDK\v2.0\LIB"
			JPEGLIB=libs/jpeg.lib
		endif
	else
		WINDOWSSDKDIR=C:/Program Files/Microsoft SDKs/Windows/v7.1

		ifeq ($(BITS),64)
			WINDRES=x86_64-w64-mingw32-windres

			MSVCPATH=C:/Program Files (x86)/$(MSVCDIR)/VC/BIN/amd64/

			MSVCINC=-I"C:\Program Files (x86)\$(MSVCDIR)\VC\ATLMFC\INCLUDE" -I"C:\Program Files (x86)\$(MSVCDIR)\VC\INCLUDE" -I"$(WINDOWSSDKDIR)/Include" -I"C:\Program Files (x86)\$(MSVCDIR)\VC\PlatformSDK\include" -I"C:\Program Files (x86)\$(MSVCDIR)\SDK\v2.0\include"
			MSVCLIB=/LIBPATH:"C:\Program Files (x86)\$(MSVCPATH)\VC\ATLMFC\LIB\amd64" /LIBPATH:"C:\Program Files (x86)\$(MSVCDIR)\VC\LIB\amd64" /LIBPATH:"$(WINDOWSSDKDIR)\lib\amd64" /LIBPATH:"C:\Program Files (x86)\$(MSVCDIR)\SDK\v2.0\LIB\AMD64" /LIBPATH:"$(WINDOWSSDKDIR)\lib\x64" 
			JPEGLIB=libs/libjpeg$(BITS).lib
		else
			WINDRES=i686-w64-mingw32-windres
			MSVCPATH=C:/Program Files (x86)/$(MSVCDIR)/VC/BIN/

			MSVCINC=-I"C:\Program Files (x86)\$(MSVCDIR)\VC\ATLMFC\INCLUDE" -I"C:\Program Files (x86)\$(MSVCDIR)\VC\INCLUDE" -I"$(WINDOWSSDKDIR)/Include" -I"C:\Program Files (x86)\$(MSVCDIR)\VC\PlatformSDK\include" -I"C:\Program Files (x86)\$(MSVCDIR)\SDK\v2.0\include"
			MSVCLIB=/LIBPATH:"C:\Program Files (x86)\$(MSVCDIR)\VC\ATLMFC\LIB" /LIBPATH:"C:\Program Files (x86)\$(MSVCDIR)\VC\LIB" /LIBPATH:"$(WINDOWSSDKDIR)\lib" /LIBPATH:"C:\Program Files (x86)\$(MSVCDIR)\SDK\v2.0\LIB"
			JPEGLIB=libs/jpeg.lib
		endif
	endif
	STRIP=@echo strip
	EXEPOSTFIX=.exe

	CC=PATH="C:\Program Files (x86)\$(MSVCDIR)\Common7\IDE" "$(MSVCPATH)cl" $(SDKINC) $(MSVCINC) -D_CRT_SECURE_NO_WARNINGS
	DEBUG_CFLAGS ?= -Od $(CPUOPTIMIZATIONS) /fp:fast
	PROFILE_CFLAGS = -O2 -Ot -Ox -GL $(CPUOPTIMISATIONS) /fp:fast
	PROFILE_LDFLAGS = /LTCG:PGINSTRUMENT
	RELEASE_CFLAGS = -O2 -Ot -Ox -GL -GS- -Gr $(CPUOPTIMIZATIONS) /fp:fast
	RELEASE_LDFLAGS = /LTCG
# /LTCG:PGOPTIMIZE

	DO_CC=$(DO_ECHO) $(CC) /nologo $(ALL_CFLAGS) -Fo$(shell cygpath -m $@) -c $(shell cygpath -m $<)
	DO_LD=$(DO_ECHO) PATH="C:\Program Files (x86)\$(MSVCDIR)\Common7\IDE" "$(MSVCPATH)link" /nologo /out:"$(shell cygpath -m $@)" /nodefaultlib:libc.lib /LARGEADDRESSAWARE /nodefaultlib:MSVCRT $(MSVCLIB) $(SDKLIB) /manifest:no /OPT:REF  wsock32.lib user32.lib kernel32.lib advapi32.lib winmm.lib libs/zlib$(BITS).lib shell32.lib
	PRECOMPHEADERS = 
	DEPCC=

	LIBS_DIR=./libs/

	BASE_CFLAGS:=$(WARNINGFLAGS) $(GNUC_FUNCS) -I$(shell cygpath -m $(CLIENT_DIR)) -I$(shell cygpath -m $(SERVER_DIR)) -I$(shell cygpath -m $(COMMON_DIR)) -I$(shell cygpath -m $(GL_DIR)) -I$(shell cygpath -m $(D3D_DIR)) -I$(shell cygpath -m $(PROGS_DIR)) -I. -I$(LIBS_DIR) -I$(LIBS_DIR)/dxsdk9/include -I$(LIBS_DIR)/dxsdk7/include $(SDL_INCLUDES) -I./libs/freetype2/include -I./libs/freetype2/include/freetype $(BOTLIB_CFLAGS) $(SVNREVISION)

	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) $(W32_CFLAGS) -DMULTITHREAD -DMSVCLIBPATH=libs/
	SV_EXE_NAME=../$(EXE_NAME)-sv$(BITS)$(EXEPOSTFIX)
	SV_DIR=sv_vc$(BITS)
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(BOTLIB_OBJS) $(PROGS_OBJS) $(WINDOWSSERVERONLY_OBJS) fs_win32.o resources.o
	SV_LDFLAGS=/subsystem:console

	GL_EXE_NAME=../$(EXE_NAME)-gl$(BITS)$(EXEPOSTFIX)
	GLCL_EXE_NAME=../$(EXE_NAME)-mingl$(BITS)
	GLB_DIR=gl_vc$(BITS)
	GLCL_DIR=glcl_vc$(BITS)
	GL_LDFLAGS=$(GLLDFLAGS) $(JPEGLIB) libs/libpng$(BITS).lib uuid.lib gdi32.lib ole32.lib /subsystem:windows
	GL_CFLAGS=$(GLCFLAGS) $(W32_CFLAGS) -DMULTITHREAD -DMSVCLIBPATH=libs/
	GLCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidnt.o $(WINDOWS_OBJS)
	GL_OBJS=

	MINGL_DIR=mingl_vc$(BITS)
	MINGL_EXE_NAME=../$(EXE_NAME)-mingl$(BITS)$(EXEPOSTFIX)

	D3DCL_OBJS=$(D3DQUAKE_OBJS) $(D3DGL_OBJS) $(BOTLIB_OBJS) $(WINDOWS_OBJS)
	D3D_EXE_NAME=../$(EXE_NAME)-d3d$(BITS)$(EXEPOSTFIX)
	D3DCL_EXE_NAME=../$(EXE_NAME)-d3dcl$(BITS)$(EXEPOSTFIX)
	D3D_LDFLAGS=$(JPEGLIB) libs/libpng$(BITS).lib uuid.lib gdi32.lib ole32.lib /subsystem:windows
	D3D_CFLAGS=$(D3DCFLAGS) $(W32_CFLAGS) $(DX7SDK) -DMULTITHREAD -DMSVCLIBPATH=libs/
	D3DB_DIR=d3d_vc$(BITS)
	D3DCL_DIR=d3dcl_vc$(BITS)
	
	M_EXE_NAME=../$(EXE_NAME)$(BITS)$(EXEPOSTFIX)
	MCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(D3DQUAKE_OBJS) $(GLQUAKE_OBJS) gl_vidnt.o $(BOTLIB_OBJS) $(WINDOWS_OBJS)
	M_CFLAGS=$(D3DCFLAGS) $(GLCFLAGS) $(W32_CFLAGS) $(D3DCFLAGS) -DMULTITHREAD -DMSVCLIBPATH=libs/
	MB_DIR=m_vc$(BITS)
	M_LDFLAGS=$(GLLDFLAGS) $(JPEGLIB) libs/libpng$(BITS).lib uuid.lib gdi32.lib ole32.lib /subsystem:windows
endif

#FTE_TARGET=win32 | FTE_TARGET=win64 (MinGW32 | MinGW64)
ifeq (win,$(findstring win,$(FTE_TARGET))$(findstring _SDL,$(FTE_TARGET)))
	# The extra object file called resources.o is specific for MinGW to link the icon in

	#cygwin's gcc requires an extra command to use mingw instead of cygwin (default paths, etc).
	ifneq ($(shell $(CC) -dumpmachine 2>&1 | grep cygwin),)
		W32_CFLAGS=-mno-cygwin
	endif
	
	ifeq ($(FTE_TARGET),win64)
		BITS=64
	endif
	QCC_DIR=winqcc$(BITS)
	QCC_LDFLAGS=$(MINGW_LIBS_DIR)/libz.a

	BASELDFLAGS=


	# Allow 32bit FTE to access beyond the 2GB address space
	ifeq ($(FTE_TARGET),win32)
		BASELDFLAGS=-Wl,--large-address-aware
	endif

	BASELDFLAGS+=-lcomctl32
	EXEPOSTFIX=.exe

	LIBS_DIR = $(BASE_DIR)/libs
	SV_EXE_NAME=../$(EXE_NAME)sv$(BITS)$(EXEPOSTFIX)
	SV_LDFLAGS=$(MINGW_LIBS_DIR)/libz.a -lws2_32 -lwinmm
	SV_DIR=sv_mingw$(BITS)
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(WINDOWSSERVERONLY_OBJS) $(BOTLIB_OBJS) fs_win32.o $(LTO_END) resources.o $(LTO_START)
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) $(W32_CFLAGS)


	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidnt.o $(WINDOWS_OBJS)
	GL_EXE_NAME=../fteglqw$(BITS)$(EXEPOSTFIX)
	GLCL_EXE_NAME=../fteglqwcl$(BITS)$(EXEPOSTFIX)
	GL_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(CLIENTLDDEPS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32 -Wl,--subsystem,windows
	GL_CFLAGS=$(GLCFLAGS) $(W32_CFLAGS) $(DX7SDK) -DMULTITHREAD $(CLIENTLIBFLAGS)
	GLB_DIR=gl_mgw$(BITS)
	GLCL_DIR=glcl_mgw$(BITS)

	NPFTE_OBJS=httpclient.o image.o sys_win_threads.o sys_npfte.o sys_axfte.o sys_plugfte.o $(LTO_END) npplug.o ../../ftequake/npapi.def $(LTO_START)
	NPFTE_DLL_NAME=../npfte$(BITS).dll
	NPFTE_LDFLAGS=-Wl,--enable-stdcall-fixup $(IMAGELDFLAGS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32 -loleaut32 -luuid -lstdc++ -shared -Wl,--subsystem,windows
	NPFTE_CFLAGS=$(NPFTECFLAGS) $(W32_CFLAGS) -DMULTITHREAD
	NPFTEB_DIR=npfte_mgw$(BITS)

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(D3DQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidnt.o gl_videgl.o $(WINDOWS_OBJS)
	M_EXE_NAME=../$(EXE_NAME)$(BITS)$(EXEPOSTFIX)
	MCL_EXE_NAME=../$(EXE_NAME)cl$(BITS)$(EXEPOSTFIX)
	M_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(CLIENTLDDEPS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32 -Wl,--subsystem,windows
	M_CFLAGS=$(GLCFLAGS) $(W32_CFLAGS) $(D3DCFLAGS) $(DX7SDK) $(VKCFLAGS) -DMULTITHREAD $(CLIENTLIBFLAGS)
	MB_DIR=m_mgw$(BITS)
	MCL_DIR=mcl_mgw$(BITS)

	D3DCL_OBJS=$(D3DQUAKE_OBJS) $(D3DGL_OBJS) $(BOTLIB_OBJS) $(WINDOWS_OBJS)
	D3D_EXE_NAME=../fted3dqw$(BITS)$(EXEPOSTFIX)
	D3DCL_EXE_NAME=../fted3dclqw$(BITS)$(EXEPOSTFIX)
	D3D_LDFLAGS=$(IMAGELDFLAGS) $(CLIENTLDDEPS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32 -Wl,--subsystem,windows
	D3D_CFLAGS=$(D3DCFLAGS) $(W32_CFLAGS) $(DX7SDK) -DMULTITHREAD $(CLIENTLIBFLAGS)
	D3DB_DIR=d3d_mgw$(BITS)
	D3DCL_DIR=d3dcl_mgw$(BITS)

	VKCL_OBJS=$(GLQUAKE_OBJS) $(D3DGL_OBJS) $(BOTLIB_OBJS) $(WINDOWS_OBJS) gl_vidnt.o
	VK_EXE_NAME=../ftevkqw$(BITS)$(EXEPOSTFIX)
	VKCL_EXE_NAME=../ftevkclqw$(BITS)$(EXEPOSTFIX)
	VK_LDFLAGS=$(IMAGELDFLAGS) $(CLIENTLDDEPS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32 -Wl,--subsystem,windows
	VK_CFLAGS=$(VKCFLAGS) $(W32_CFLAGS) $(DX7SDK) -DMULTITHREAD $(CLIENTLIBFLAGS)
	VKB_DIR=vk_mgw$(BITS)
	VKCL_DIR=vkcl_mgw$(BITS)

	MINGL_EXE_NAME=../fteminglqw$(BITS)$(EXEPOSTFIX)
	MINGL_DIR=mingl_mgw$(BITS)

	ifeq ($(NOCOMPAT),1)
		SV_EXE_NAME=../engine-sv$(BITS)$(EXEPOSTFIX)
		GL_EXE_NAME=../engine-gl$(BITS)$(EXEPOSTFIX)
		VK_EXE_NAME=../engine-vk$(BITS)$(EXEPOSTFIX)
		M_EXE_NAME=../engine$(BITS)$(EXEPOSTFIX)
		D3D_EXE_NAME=../engine-d3d$(BITS)$(EXEPOSTFIX)
		MINGL_EXE_NAME=../engine-mingl$(BITS)$(EXEPOSTFIX)
	endif
endif

ifeq ($(FTE_TARGET),bsd)
	#mostly uses the linux stuff.
	#oss, X, etc.

	SV_DIR=sv_bsd
	SV_EXE_NAME=../$(EXE_NAME)-sv$(BITS)
	SV_LDFLAGS=-lpthread
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) -DMULTITHREAD

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_linux_threads.o
	GL_EXE_NAME=../$(EXE_NAME)-gl
	GLCL_EXE_NAME=../$(EXE_NAME)-glcl
	GL_LDFLAGS= -L/usr/local/lib $(GLLDFLAGS) $(XLDFLAGS) -lpthread
	GL_CFLAGS=$(GLCFLAGS) -I/usr/local/include -I/usr/X11R6/include 
	GLB_DIR=gl_bsd
	GLCL_DIR=glcl_bsd

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_linux_threads.o
	M_EXE_NAME=../$(EXE_NAME)
	MCL_EXE_NAME=../$(EXE_NAME)-cl
	M_LDFLAGS=$(GLLDFLAGS) $(XLDFLAGS) -lpthread
	M_CFLAGS=$(VKCFLAGS) $(GLCFLAGS) -I/usr/X11R6/include -DMULTITHREAD
	MB_DIR=m_bsd
	MCL_DIR=mcl_bsd

	MINGL_EXE_NAME=../$(EXE_NAME)-mingl
	MINGL_DIR=mingl_bsd

	#openbsd has a special library for oss emulation.
	ifeq ($(shell uname -s),OpenBSD)
		GL_LDFLAGS+= -lossaudio
		VK_LDFLAGS+= -lossaudio
		M_LDFLAGS+= -lossaudio
	endif

	ifeq (,$(findstring NO_ZLIB,$(CFLAGS)))
		SV_LDFLAGS+= -lz
		GL_LDFLAGS+= -lz
		VK_LDFLAGS+= -lz
		M_LDFLAGS+= -lz
	endif
endif
ifneq (,$(findstring linux,$(FTE_TARGET)))
	SV_DIR=sv_linux$(BITS)
	SV_EXE_NAME=../$(EXE_NAME)-sv$(BITS)
	SV_LDFLAGS=
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) -DMULTITHREAD

	ifneq ("$(wildcard $(/usr/include/wayland-client.h))","")
		HAVE_WAYLAND=-DWAYLANDQUAKE
	else
		HAVE_WAYLAND=
	endif
	ifneq ("$(wildcard $(/usr/include/EGL/egl.h))","")
		HAVE_EGL=-DUSE_EGL
	else
		HAVE_EGL=
	endif

	CL_CFLAGS=-DMULTITHREAD -DDYNAMIC_SDL $(HAVE_EGL) $(HAVE_WAYLAND) -DX11QUAKE
	
	QCC_DIR=linqcc$(BITS)
	
	NPFTE_OBJS=httpclient.o image.o sys_linux_threads.o sys_npfte.o sys_axfte.o sys_plugfte.o
	NPFTE_DLL_NAME=../npfte$(BITS).so
	NPFTE_LDFLAGS=-shared -Wl,-z,defs -ldl -lpthread
	NPFTE_CFLAGS=$(NPFTECFLAGS) $(W32_CFLAGS) -DMULTITHREAD -fPIC -DDYNAMIC_LIBPNG -DDYNAMIC_LIBJPEG
	NPFTEB_DIR=npfte_linux$(BITS)

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o gl_vidwayland.o gl_videgl.o snd_alsa.o snd_linux.o snd_sdl.o cd_linux.o sys_linux.o sys_linux_threads.o
	GL_EXE_NAME=../$(EXE_NAME)-gl$(BITS)
	GLCL_EXE_NAME=../$(EXE_NAME)-glcl$(BITS)
	GL_LDFLAGS=$(GLLDFLAGS) $(XLDFLAGS) $(CLIENTLDDEPS)
	GL_CFLAGS=$(GLCFLAGS) -I/usr/X11R6/include $(CL_CFLAGS) $(CLIENTLIBFLAGS)
	GLB_DIR=gl_linux$(BITS)
	GLCL_DIR=glcl_linux$(BITS)

	VKCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o gl_vidwayland.o gl_videgl.o snd_alsa.o snd_linux.o snd_sdl.o cd_linux.o sys_linux.o sys_linux_threads.o
	VK_EXE_NAME=../$(EXE_NAME)-vk$(BITS)
	VKCL_EXE_NAME=../$(EXE_NAME)-vkcl$(BITS)
	VK_LDFLAGS=$(GLLDFLAGS) $(XLDFLAGS) $(CLIENTLDDEPS)
	VK_CFLAGS=$(VKCFLAGS) -I/usr/X11R6/include $(CL_CFLAGS) $(CLIENTLIBFLAGS)
	VKB_DIR=vk_linux$(BITS)
	VKCL_DIR=vkcl_linux$(BITS)

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o gl_vidwayland.o gl_videgl.o snd_linux.o snd_sdl.o snd_alsa.o cd_linux.o sys_linux.o sys_linux_threads.o
	M_EXE_NAME=../$(EXE_NAME)$(BITS)
	MCL_EXE_NAME=../$(EXE_NAME)-cl$(BITS)
	M_LDFLAGS=$(GL_LDFLAGS)
	M_CFLAGS=$(VKCFLAGS) $(GL_CFLAGS) $(CLIENTLIBFLAGS)
	MB_DIR=m_linux$(BITS)
	MCL_DIR=mcl_linux$(BITS)

	ifeq (,$(findstring NO_ZLIB,$(CFLAGS)))
		SV_LDFLAGS+= -lz
		GL_LDFLAGS+= -lz
		VK_LDFLAGS+= -lz
		M_LDFLAGS+= -lz
	endif


	MINGL_EXE_NAME=../$(EXE_NAME)-mingl$(BITS)
	MINGL_DIR=mingl_linux$(BITS)

	ifeq ($(NOCOMPAT),1)
		SV_EXE_NAME=../engine-sv$(BITS)$(EXEPOSTFIX)
		GL_EXE_NAME=../engine-gl$(BITS)$(EXEPOSTFIX)
		VK_EXE_NAME=../engine-vk$(BITS)$(EXEPOSTFIX)
		M_EXE_NAME=../engine$(BITS)$(EXEPOSTFIX)
		D3D_EXE_NAME=../engine-d3d$(BITS)$(EXEPOSTFIX)
		MINGL_EXE_NAME=../engine-mingl$(BITS)$(EXEPOSTFIX)
	endif
endif
ifneq (,$(findstring rpi,$(FTE_TARGET)))
	#These next two lines enable cross compiling. If you're compiling natively you can just kill the two.
	RPI_SYSROOT:=$(realpath $(shell echo ~)/rpi/rpi-sysroot/)
	CC=~/rpi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-gcc --sysroot=$(RPI_SYSROOT)
	SDLCONFIG=$(RPI_SYSROOT)/usr/bin/sdl-config --prefix=$(RPI_SYSROOT)/usr
	GL_CFLAGS+= -I$(RPI_SYSROOT)/opt/vc/include -I$(RPI_SYSROOT)/opt/vc/include/interface/vmcs_host/linux -I$(RPI_SYSROOT)/opt/vc/include/interface/vcos/pthreads -DFTE_RPI -DUSE_EGL
	GL_LDFLAGS+= -L$(RPI_SYSROOT)/opt/vc/lib -Wl,--sysroot=$(RPI_SYSROOT),-rpath=/opt/vc/lib,-rpath-link=$(RPI_SYSROOT)/opt/vc/lib -lbcm_host
	GLCL_OBJS+=gl_vidrpi.o
endif
ifneq (,$(findstring fbdev,$(FTE_TARGET)))
	GL_CFLAGS+=-DUSE_EGL
	GLCL_OBJS+=gl_vidfbdev.o
	MCL_OBJS+=gl_vidfbdev.o
endif
ifneq ($(shell echo $(FTE_TARGET)|grep macosx),)
	SV_DIR=sv_macosx$(EXTENSION)$(BITS)
	GLB_DIR=gl_macosx$(EXTENSION)$(BITS)
	GLCL_DIR=glcl_macosx$(EXTENSION)$(BITS)
	MINGL_DIR=mingl_macosx$(EXTENSION)$(BITS)
	
	CLIENTLDDEPS=

	GL_CFLAGS=$(GLCFLAGS) -D__MACOSX__ -L/sw/lib -I/sw/include -L/opt/local/lib -I/opt/local/include -I$(LIBS_DIR)	
	ifeq ($(FTE_TARGET),macosx_x86)
		GL_CFLAGS=$(GLCFLAGS) -D__MACOSX__ -L/sw/lib -I/sw/include -L/opt/local/lib -I/opt/local/include -I$(LIBS_DIR)
	endif

	GL_LDFLAGS=-framework AGL -framework OpenGL -framework Cocoa -framework AudioUnit -lz -lpng -ljpeg
	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidcocoa.mo gl_vidmacos.o sys_linux.o cd_null.o snd_macos.o sys_linux_threads.o

	GL_EXE_NAME=../$(EXE_NAME)-macosx-gl$(EXTENSION)$(BITS)
	GLCL_EXE_NAME=../$(EXE_NAME)cl-macosx-gl$(EXTENSION)$(BITS)
	M_EXE_NAME=../$(EXE_NAME)-macosx$(EXTENSION)$(BITS)
	MCL_EXE_NAME=../$(EXE_NAME)-macosx-cl$(EXTENSION)$(BITS)
	MINGL_EXE_NAME=../$(EXE_NAME)-macosx-mingl$(EXTENSION)$(BITS)
	MINGL_DIR=mingl_macosx$(EXTENSION)$(BITS)
	
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(BOTLIB_OBJS) $(SERVERONLY_OBJS)
	SV_EXE_NAME=../$(EXE_NAME)-macosx-sv$(EXTENSION)$(BITS)
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS)
	SV_LDFLAGS=-lz	

	#seems, macosx has a more limited version of strip
	STRIPFLAGS=
endif
ifeq ($(FTE_TARGET),morphos)
	#-Wno-pointer-sign unrecognised 
	WARNINGFLAGS=-Wall

	CFLAGS+=-D__MORPHOS_SHAREDLIBS

	SV_DIR=sv_morphos
	SV_LDFLAGS=-ldl -lz
	CLIENTLDDEPS=

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidmorphos.o in_morphos.o snd_morphos.o cd_null.o sys_morphos.o
	GL_EXE_NAME=../$(EXE_NAME)-morphos-gl
	GLCL_EXE_NAME=../$(EXE_NAME)-morphos-glcl
	GL_LDFLAGS=$(GLLDFLAGS) -ldl $(IMAGELDFLAGS) -lz
	GL_CFLAGS=$(GLCFLAGS) -noixemul -I./
	GLB_DIR=gl_morphos
	GLCL_DIR=glcl_morphos

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) gl_vidmorphos.o vid_morphos.o in_morphos.o snd_morphos.o cd_null.o sys_morphos.o
	M_EXE_NAME=../$(EXE_NAME)-morphos
	MCL_EXE_NAME=../$(EXE_NAME)-morphos-cl
	M_LDFLAGS=$(GLLDFLAGS)
	M_CFLAGS=$(GLCFLAGS)
	MB_DIR=m_morphos
	MCL_DIR=mcl_morphos

	MINGL_EXE_NAME=../$(EXE_NAME)-morphos-mingl
	MINGL_DIR=mingl_morphos
	
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS) $(BOTLIB_OBJS)
	SV_EXE_NAME=../$(EXE_NAME)-morphos-sv$(BITS)
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS)	
endif

ifeq ($(FTE_TARGET),dos)
	EXEPOSTFIX=.exe
	SV_DIR=sv_dos
	GLB_DIR=gl_dos
	MB_DIR=m_dos
	MCL_DIR=mcl_dos
	MINGL_DIR=mingl_dos
	VKB_DIR=vk_dos
	VKCL_DIR=vkcl_dos

	IMAGELDFLAGS=
	CLIENTLDDEPS=

	SOFTWARE_OBJS=sw_rast.o sw_backend.o sw_image.o

	M_LDFLAGS=
	M_CFLAGS=-DSWQUAKE -DNO_ZLIB
	MCL_OBJS=$(SOFTWARE_OBJS) $(D3DGL_OBJS) sw_viddos.o cd_null.o sys_dos.o snd_sblaster.o
	M_EXE_NAME=../$(EXE_NAME)$(EXEPOSTFIX)
	SV_EXE_NAME=../$(EXE_NAME)sv$(BITS)$(EXEPOSTFIX)
	VK_EXE_NAME=../$(EXE_NAME)-vk$(BITS)$(EXEPOSTFIX)

	VKCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) cd_null.o sys_dos.o snd_sblaster.o
endif

ifeq ($(FTE_TARGET),cyg)
	SV_DIR=sv_cygwin
	SV_LDFLAGS=-lz
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS)

	EXEPOSTFIX=.exe
	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_linux_threads.o
	GL_EXE_NAME=../$(EXE_NAME)-cyg-gl$(EXEPOSTFIX)
	GLCL_EXE_NAME=../$(EXE_NAME)-cyg-glcl$(EXEPOSTFIX)
	GL_LDFLAGS=$(GLLDFLAGS) $(XLDFLAGS) $(CLIENTLDDEPS) -lz -lltdl
	GL_CFLAGS=$(GLCFLAGS) -I/usr/X11R6/include $(CLIENTLIBFLAGS) -DUSE_LIBTOOL
	GLB_DIR=gl_cygwin
	GLCL_DIR=glcl_cygwin

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_linux_threads.o
	M_EXE_NAME=../$(EXE_NAME)-cyg$(EXEPOSTFIX)
	MCL_EXE_NAME=../$(EXE_NAME)-cyg-cl$(EXEPOSTFIX)
	M_LDFLAGS=$(GLLDFLAGS) $(XLDFLAGS) $(CLIENTLDDEPS) -lz -lltdl
	M_CFLAGS=$(GLCFLAGS) $(CLIENTLIBFLAGS) -DUSE_LIBTOOL
	MB_DIR=m_cygwin
	MCL_DIR=mcl_cygwin

	LIBS_DIR = $(BASE_DIR)/libs

	MINGL_EXE_NAME=../$(EXE_NAME)-cyg-mingl$(EXEPOSTFIX)
	MINGL_DIR=mingl_cygwin
endif

ifeq ($(FTE_TARGET),droid)
	BASELDFLAGS=-lz

	SYS_DROID_O=sys_droid.o sys_linux_threads.o
	GL_DROID_O=gl_viddroid.o $(SYS_DROID_O)

	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) $(W32_CFLAGS)
	SV_LDFLAGS=
	SV_DIR=sv_droid-$(DROID_ARCH)
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(BOTLIB_OBJS) $(SYS_DROID_O)
	SV_EXE_NAME=libftedroid.so

	GL_CFLAGS=$(GLCFLAGS)
	GL_LDFLAGS=$(GLLDFLAGS) -landroid -lEGL
	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) $(GL_DROID_O) cd_null.o snd_droid.o
	GLB_DIR=gl_droid-$(DROID_ARCH)
	GL_EXE_NAME=libftedroid.so	

	M_CFLAGS=$(VKCFLAGS) $(GLCFLAGS) -DMULTITHREAD
	M_LDFLAGS=$(GLLDFLAGS) -landroid -lEGL
	MCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) $(GL_DROID_O) cd_null.o snd_droid.o
	MB_DIR=m_droid-$(DROID_ARCH)
	M_EXE_NAME=libftedroid.so
endif

ifeq ($(FTE_TARGET),web)
	COMMON_OBJS+=sys_web.o fs_web.o
	WEB_PREJS ?= --pre-js web/prejs.js
#	WEB_MEMORY?=402653184	#384mb
#	ASMJS_MEMORY?=16777216	#16mb
#	ASMJS_MEMORY?=33554432	#32mb
	ASMJS_MEMORY?=268435456	#256mb
#	ASMJS_MEMORY?=536870912	#512mb
#	ASMJS_MEMORY?=1073741824 #1025mb
#	ASMJS_MEMORY?=2147483648 #2048mb
	WEB_MEMORY?=$(ASMJS_MEMORY)
	JSLIBS=--js-library web/ftejslib.js -s LEGACY_GL_EMULATION=0
	EMCC_ARGS=$(JSLIBS) $(WEB_PREJS) -s ERROR_ON_UNDEFINED_SYMBOLS=1
	RELEASE_CFLAGS=-DOMIT_QCC -DGL_STATIC -DFTE_TARGET_WEB
	DEBUG_CFLAGS=-g -DOMIT_QCC -DGL_STATIC -DFTE_TARGET_WEB
	RELEASE_LDFLAGS=-s ASM_JS=1 -O3	-s TOTAL_MEMORY=$(ASMJS_MEMORY) $(EMCC_ARGS)
#	RELEASE_LDFLAGS=-s ASM_JS=0 -O1	-s TOTAL_MEMORY=$(WEB_MEMORY) $(EMCC_ARGS)
	DEBUG_LDFLAGS=-O0 -g4 -s TOTAL_MEMORY=$(WEB_MEMORY) $(EMCC_ARGS) -s SAFE_HEAP=1 -s ALIASING_FUNCTION_POINTERS=0 -s ASSERTIONS=2
	CC?=emcc
	#BASELDFLAGS=
	PRECOMPHEADERS=

	#mostly we inherit the sdl defaults. because we can, however emscripten does not support sdl cd code.
	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) gl_vidweb.o cd_null.o
	SDL_INCLUDES=

	SV_DIR=sv_web
	#SV_LDFLAGS=-lz
	#SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS)
	SV_EXE_NAME=../libftesv.js
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS)

	#SV_LDFLAGS=

	STRIP=echo
	#GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) cd_null.o
	#GL_LDFLAGS=$(GLLDFLAGS)
	GLB_DIR=gl_web
	GL_EXE_NAME=../ftewebgl.js

	GL_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(CLIENTLDDEPS)
	GL_CFLAGS=$(GLCFLAGS)

	IMAGELDFLAGS=
	CLIENTLDDEPS=

	BOTLIB_CFLAGS=
	#generate deps properly
	#DEPCC=
endif

SV_DIR?=sv_sdl
DEPCC?=$(CC)
ARCH:=$(ARCH)
BASELDFLAGS:=-L$(ARCHLIBS) $(BASELDFLAGS)

-include Makefile_private

.default: help
all: rel
rel: sv-rel gl-rel mingl-rel
dbg: sv-dbg gl-dbg mingl-dbg
relcl: glcl-rel mcl-rel
profile: sv-profile gl-profile mingl-profile

releases:
	#this is for releasing things from a linux box
	#just go through compiling absolutly everything
	-$(MAKE) FTE_TARGET=linux32 rel
	-$(MAKE) FTE_TARGET=linux64 rel
	-$(MAKE) FTE_TARGET=win32 rel
	-$(MAKE) FTE_TARGET=win64 rel
	-$(MAKE) FTE_TARGET=win32_SDL rel
	-$(MAKE) FTE_TARGET=win64_SDL rel
	-$(MAKE) FTE_TARGET=morphos rel
	-$(MAKE) FTE_TARGET=macosx rel
#	-$(MAKE) FTE_TARGET=linux32 relcl
#	-$(MAKE) FTE_TARGET=linux64 relcl
#	-$(MAKE) FTE_TARGET=win32 relcl
	-$(MAKE) droid-rel
	-$(MAKE) web-rel
	-$(MAKE) FTE_TARGET=win32 npfte-rel

autoconfig: clean
	/bin/bash makeconfig.sh y

config: clean
	/bin/bash makeconfig.sh

ifneq ($(OUT_DIR),)
-include $(OUT_DIR)/*.o.d
endif


VPATH = $(BASE_DIR) : $(CLIENT_DIR) : $(GL_DIR) : $(SW_DIR) : $(COMMON_DIR) : $(SERVER_DIR) : $(HTTP_DIR) : $(BASE_DIR)/irc : $(BASE_DIR)/email : $(QUX_DIR) : $(PROGS_DIR) : $(NACL_DIR) : $(D3D_DIR) : $(VK_DIR) : $(BOTLIB_DIR) : $(BASE_DIR)/web

# This is for linking the FTE icon to the MinGW target
$(OUT_DIR)/resources.o : winquake.rc
	@$(WINDRES) $(BRANDFLAGS) -I$(CLIENT_DIR) -O coff $< $@
$(OUT_DIR)/fteqcc.o : fteqcc.rc
	@$(WINDRES) $(BRANDFLAGS) -I$(PROGS_DIR)  -O coff $< $@
#npAPI stuff requires some extra resources
$(OUT_DIR)/npplug.o : ftequake/npplug.rc
	@$(WINDRES) $(BRANDFLAGS) -I$(CLIENT_DIR) -O coff $< $@


#$(OUT_DIR)/%.d: %.c
#	@set -e; rm -f $@; \
#	$(CC) -MM $(ALL_CFLAGS) $< > $@.$$$$; \
#	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
#	rm -f $@.$$$$

$(OUT_DIR)/%.o $(OUT_DIR)/%.d : %.c
ifneq ($(DEPCC),)
	@-set -e; rm -f $@.d; \
	 $(DEPCC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.o[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	 rm -f $@.d.$$$$
endif
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.o $(OUT_DIR)/%.d : %.cpp
ifneq ($(DEPCC),)
	@-set -e; rm -f $@.d; \
	$(DEPCC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	sed 's,\($*\)\.o[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	rm -f $@.d.$$$$
endif
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.o $(OUT_DIR)/%.d : %.cxx
ifneq ($(DEPCC),)
	@-set -e; rm -f $@.d; \
	$(DEPCC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	sed 's,\($*\)\.o[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	rm -f $@.d.$$$$
endif
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.oo $(OUT_DIR)/%.d : %.c
ifneq ($(DEPCC),)
	@-set -e; rm -f $@.d; \
	 $(DEPCC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.oo[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	 rm -f $@.d.$$$$
endif
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.mo $(OUT_DIR)/%.d : %.m
	@-set -e; rm -f $@.d; \
	 $(DEPCC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.mo[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	 rm -f $@.d.$$$$
	$(DO_CC) -I$(OUT_DIR)

#enables use of precompiled headers in gcc 3.4 onwards.
$(OUT_DIR)/quakedef.h.gch : quakedef.h
	$(CC) -x c $(ALL_CFLAGS) -o $@ -c $<
PRECOMPHEADERS ?= $(OUT_DIR)/quakedef.h.gch

#addprefix is to add the ./release/server/ part of the object name
#foreach is needed as the OBJS is a list of variable names containing object lists.
#which is needed as windows sucks too much for the chaining to carry a full list.
#god knows how gcc loads the list properly.
#or at least I hope he does. It makes no sence to mortals.

DO_LD ?= $(DO_ECHO) $(CC) -o $@ $(LTO_LD) $(WCFLAGS) $(BRANDFLAGS) $(CFLAGS)
$(OUT_DIR)/$(EXE_NAME):   $(PRECOMPHEADERS) $(foreach fn, $(CUSTOMOBJS) $(foreach ol, $(OBJS), $($(ol))),$(if $(findstring ltox,$(fn)),,$(OUT_DIR)/$(fn)))
	$(DO_LD) $(foreach fn, $(CUSTOMOBJS) $(foreach ol, $(OBJS) $(LTO_END), $($(ol))),$(if $(findstring ltox,$(fn)),$(subst ltox,-x ,$(fn)),$(NATIVE_OUT_DIR)/$(fn)) ) $(LDFLAGS)

_out-rel: $(ARCH_PREDEP)
	@$(MAKE) $(OUT_DIR)/$(EXE_NAME) EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(WCFLAGS) $(RELEASE_CFLAGS)" LDFLAGS="$(BASELDFLAGS) $(LDFLAGS) $(RELEASE_LDFLAGS)" OBJS="$(OBJS)"
	$(STRIP) $(STRIPFLAGS) $(OUT_DIR)/$(EXE_NAME)

_out-dbg: $(ARCH_PREDEP)
	@$(MAKE) $(OUT_DIR)/$(EXE_NAME) EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(WCFLAGS) $(DEBUG_CFLAGS)" LDFLAGS="$(BASELDFLAGS) $(LDFLAGS) $(DEBUG_LDFLAGS)" OBJS="$(OBJS)"

_out-profile: $(ARCH_PREDEP)
	@$(MAKE) $(OUT_DIR)/$(EXE_NAME) EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(WCFLAGS) $(PROFILE_CFLAGS)" LDFLAGS="$(BASELDFLAGS) $(LDFLAGS) $(PROFILE_LDFLAGS)" OBJS="$(OBJS)"

_cl-rel: reldir
	@$(MAKE) _out-rel EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(CLIENT_ONLY_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS"

_cl-dbg: debugdir
	@$(MAKE) _out-dbg EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(CLIENT_ONLY_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS"

_cl-profile: reldir
	@$(MAKE) _out-profile EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(CLIENT_ONLY_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS"

_clsv-rel: reldir
	@$(MAKE) _out-rel EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(JOINT_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS SERVER_OBJS"

_clsv-dbg: debugdir
	@$(MAKE) _out-dbg EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(JOINT_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS SERVER_OBJS"

_clsv-profile: reldir
	@$(MAKE) _out-profile EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(JOINT_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS SERVER_OBJS"

sv-tmp: reldir debugdir
	@$(MAKE) $(TYPE) OUT_DIR="$(OUT_DIR)" EXE_NAME="$(SV_EXE_NAME)" WCFLAGS="$(SV_CFLAGS)" LDFLAGS="$(ARCH_LDFLAGS) $(SV_LDFLAGS) $(LDFLAGS)" OBJS="SV_OBJS"
sv-rel:
	@$(MAKE) sv-tmp TYPE=_out-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(SV_DIR)"
sv-dbg:
	@$(MAKE) sv-tmp TYPE=_out-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(SV_DIR)"
sv-profile:
	@$(MAKE) sv-tmp TYPE=_out-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(SV_DIR)"

d3dcl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(D3DCL_EXE_NAME)" WCFLAGS="$(D3D_CFLAGS)" LDFLAGS="$(D3D_LDFLAGS) $(LDFLAGS)" SOBJS="$(D3DCL_OBJS)"
d3d-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(D3D_EXE_NAME)" WCFLAGS="$(D3D_CFLAGS)" LDFLAGS="$(D3D_LDFLAGS) $(LDFLAGS)" SOBJS="$(D3DCL_OBJS)"

d3dcl-rel:
	@$(MAKE) d3dcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(D3DCL_DIR)"
d3dcl-dbg:
	@$(MAKE) d3dcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(D3DCL_DIR)"
d3dcl-profile:
	@$(MAKE) d3dcl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(D3DCL_DIR)"

d3d-rel:
	@$(MAKE) d3d-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(D3DB_DIR)"
d3d-dbg:
	@$(MAKE) d3d-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(D3DB_DIR)"
d3d-profile:
	@$(MAKE) d3d-tmp TYPE=_clsv-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(D3DB_DIR)"



vkcl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(VKCL_EXE_NAME)" WCFLAGS="$(VK_CFLAGS)" LDFLAGS="$(VK_LDFLAGS) $(LDFLAGS)" SOBJS="$(VKCL_OBJS)"
vk-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(VK_EXE_NAME)" WCFLAGS="$(VK_CFLAGS)" LDFLAGS="$(VK_LDFLAGS) $(LDFLAGS)" SOBJS="$(VKCL_OBJS)"

vkcl-rel:
	@$(MAKE) vkcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(VKCL_DIR)"
vkcl-dbg:
	@$(MAKE) vkcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(VKCL_DIR)"
vkcl-profile:
	@$(MAKE) vkcl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(VKCL_DIR)"

vk-rel:
	@$(MAKE) vk-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(VKB_DIR)"
vk-dbg:
	@$(MAKE) vk-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(VKB_DIR)"
vk-profile:
	@$(MAKE) vk-tmp TYPE=_clsv-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(VKB_DIR)"


glcl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(GLCL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS)" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"
gl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(GL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS)" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"

glcl-rel:
	@$(MAKE) glcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(GLCL_DIR)"
glcl-dbg:
	@$(MAKE) glcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(GLCL_DIR)"
glcl-profile:
	@$(MAKE) glcl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(GLCL_DIR)"
gl-rel:
	@$(MAKE) gl-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(GLB_DIR)"
gl-dbg:
	@$(MAKE) gl-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(GLB_DIR)"
gl-profile:
	@$(MAKE) gl-tmp TYPE=_clsv-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(GLB_DIR)"

mingl-tmp: reldir
	@$(MAKE) $(TYPE) OUT_DIR="$(OUT_DIR)" EXE_NAME="$(MINGL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS) -DMINIMAL" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"
mingl-rel:
	@$(MAKE) mingl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(MINGL_DIR)"
mingl-dbg:
	@$(MAKE) mingl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(MINGL_DIR)"
mingl-profile:
	@$(MAKE) mingl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(MINGL_DIR)"

mcl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(MCL_EXE_NAME)" WCFLAGS="$(M_CFLAGS)" LDFLAGS="$(M_LDFLAGS) $(LDFLAGS)" SOBJS="$(MCL_OBJS)"
m-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(M_EXE_NAME)" WCFLAGS="$(M_CFLAGS)" LDFLAGS="$(M_LDFLAGS) $(LDFLAGS)" SOBJS="$(MCL_OBJS)"

mcl-rel:
	@$(MAKE) mcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(MCL_DIR)"
mcl-dbg:
	@$(MAKE) mcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(MCL_DIR)"
mcl-profile:
	@$(MAKE) mcl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(MCL_DIR)"
m-rel:
	@$(MAKE) m-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(MB_DIR)"
m-dbg:
	@$(MAKE) m-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(MB_DIR)"
m-profile:
	@$(MAKE) m-tmp TYPE=_clsv-profile OUT_DIR="$(PROFILE_DIR)/$(NCDIRPREFIX)$(MB_DIR)"

.PHONY: m-tmp mcl-tmp mingl-tmp glcl-tmp gl-tmp sv-tmp _clsv-dbg _clsv-rel _cl-dbg _cl-rel _out-rel _out-dbg reldir debugdir makelibs wel-rel web-dbg httpserver


_qcc-tmp: $(REQDIR)
	@$(MAKE) $(TYPE) EXE_NAME="$(EXE_NAME)$(EXEPOSTFIX)" PRECOMPHEADERS="" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(CLIENT_ONLY_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS) $(QCC_LDFLAGS)" OBJS="QCC_OBJS SOBJS"
qcc-rel:
	@$(MAKE) _qcc-tmp TYPE=_out-rel REQDIR=reldir EXE_NAME="../fteqcc$(BITS)" OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(QCC_DIR)" SOBJS="qcctui.o $(if $(findstring win,$(FTE_TARGET)),fteqcc.o)"
qccgui-rel:
	@$(MAKE) _qcc-tmp TYPE=_out-rel REQDIR=reldir EXE_NAME="../fteqccgui$(BITS)" OUT_DIR="$(RELEASE_DIR)/$(NCDIRPREFIX)$(QCC_DIR)gui" SOBJS="qccgui.o qccguistuff.o packager.o decomp.o fteqcc.o" LDFLAGS="$(LDFLAGS) -lole32 -lcomdlg32 -lcomctl32 -lshlwapi -mwindows"
qcc-dbg:
	@$(MAKE) _qcc-tmp TYPE=_out-dbg REQDIR=debugdir EXE_NAME="../fteqcc$(BITS)" OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(QCC_DIR)" SOBJS="qcctui.o $(if $(findstring win,$(FTE_TARGET)),fteqcc.o)"
qccgui-dbg:
	@$(MAKE) _qcc-tmp TYPE=_out-dbg REQDIR=debugdir EXE_NAME="../fteqccgui$(BITS)" OUT_DIR="$(DEBUG_DIR)/$(NCDIRPREFIX)$(QCC_DIR)gui" SOBJS="qccgui.o qccguistuff.o packager.o decomp.o fteqcc.o" LDFLAGS="$(LDFLAGS) -lole32 -lcomdlg32 -lcomctl32 -lshlwapi -mwindows"


#scintilla is messy as fuck when building statically. but at least we can strip out the lexers we don't use this way.
#note that this is only used in the 'qccgui-scintilla' target.
SCINTILLA_FILES= AutoComplete.o CallTip.o CaseConvert.o CaseFolder.o CellBuffer.o CharacterCategory.o CharacterSet.o CharClassify.o ContractionState.o Decoration.o Document.o EditModel.o Editor.o EditView.o KeyMap.o Indicator.o LineMarker.o MarginView.o PerLine.o PlatWin.o PositionCache.o PropSetSimple.o RESearch.o RunStyles.o Selection.o Style.o UniConversion.o ViewStyle.o XPM.o ScintillaWin.o HanjaDic.o ScintillaBase.o Accessor.o Catalogue.o ExternalLexer.o LexerBase.o LexerModule.o LexerSimple.o StyleContext.o WordList.o LexCPP.o
SCINTILLA_ROOT=$(BASE_DIR)/scintilla$(SCINTILLAVER)/scintilla
SCINTILLA_DIRS=$(SCINTILLA_ROOT)/lexers:$(SCINTILLA_ROOT)/lexlib:$(SCINTILLA_ROOT)/src:$(SCINTILLA_ROOT)/win32
SCINTILLA_INC=-I$(SCINTILLA_ROOT)/include -I$(SCINTILLA_ROOT)/lexlib -I$(SCINTILLA_ROOT)/win32 -I$(SCINTILLA_ROOT)/src
$(RELEASE_DIR)/scintilla$(BITS).a: $(foreach f,$(SCINTILLA_FILES),$(OUT_DIR)/$(f))
	@$(AR) -r $@ $?
	@$(AR) -s $@
scintilla$(BITS)_static:
	@test -f scintilla$(SCINTILLAVER).tar.gz || wget http://prdownloads.sourceforge.net/scintilla/scintilla$(SCINTILLAVER).tgz?download -O scintilla$(SCINTILLAVER).tar.gz
	@-test -f $(SCINTILLA_ROOT) || (mkdir $(BASE_DIR)/scintilla$(SCINTILLAVER) && cd $(BASE_DIR)/scintilla$(SCINTILLAVER) && tar -xvzf ../scintilla$(SCINTILLAVER).tar.gz && cd scintilla && mv lexers/LexCPP.cxx . && rm lexers/Lex*.cxx && mv LexCPP.cxx lexers/ && cd scripts && python LexGen.py)
	@$(MAKE) reldir OUT_DIR=$(RELEASE_DIR)/$(QCC_DIR)scin
	@$(MAKE) $(RELEASE_DIR)/scintilla$(BITS).a VPATH="$(SCINTILLA_DIRS)" CFLAGS="$(SCINTILLA_INC) -DDISABLE_D2D -DSTATIC_BUILD -DSCI_LEXER -std=c++11" OUT_DIR=$(RELEASE_DIR)/$(QCC_DIR)scin WCFLAGS="$(WCFLAGS) -Os" WARNINGFLAGS=
qccgui-scintilla: scintilla$(BITS)_static
	@$(MAKE) _qcc-tmp TYPE=_out-rel REQDIR=reldir EXE_NAME="../fteqccgui$(BITS)" OUT_DIR="$(RELEASE_DIR)/$(QCC_DIR)scin" SOBJS="qccgui.o qccguistuff.o packager.o decomp.o fteqcc.o" WCFLAGS="$(WCFLAGS) -DSCISTATIC" LDFLAGS="$(LDFLAGS) $(RELEASE_DIR)/scintilla$(BITS).a -static -luuid -lole32 -limm32 -lstdc++ -loleaut32 -lcomdlg32 -lcomctl32 -lshlwapi -mwindows"

ifdef windir
debugdir:
	@-mkdir -p "$(subst /,\, $(OUT_DIR))"
reldir:
	@-mkdir -p "$(subst /,\, $(OUT_DIR))"
else
reldir:
	@-mkdir -p "$(RELEASE_DIR)"
	@-mkdir -p "$(OUT_DIR)"
debugdir:
	@-mkdir -p "$(DEBUG_DIR)"
	@-mkdir -p "$(OUT_DIR)"
endif

plugins-dbg:
	@-mkdir -p $(DEBUG_DIR)
	@if test -e ../plugins/Makefile; \
	then $(MAKE) native -C ../plugins  OUT_DIR="$(DEBUG_DIR)" CC="$(CC) $(W32_CFLAGS) $(DEBUG_CFLAGS)" ARCH="$(ARCH)" BASE_CFLAGS="$(BASE_CFLAGS)" FTE_TARGET="$(FTE_TARGET)"; \
	 else echo no plugins directory installed; \
	fi
plugins:

plugins-rel:
	@-mkdir -p $(RELEASE_DIR)
	@if test -e ../plugins/Makefile; \
	then $(MAKE) native -C ../plugins  OUT_DIR="$(RELEASE_DIR)" CC="$(CC) $(W32_CFLAGS) $(RELEASE_CFLAGS)" ARCH="$(ARCH)" BASE_CFLAGS="$(BASE_CFLAGS)" FTE_TARGET="$(FTE_TARGET)"; \
	 else echo no plugins directory installed; \
	fi
plugins-rel:

help:
	@-echo "Specfic targets:"
	@-echo "clean - removes all output (use make dirs afterwards)"
	@-echo "all - make all the targets possible"
	@-echo "rel - make the releases for the default system"
	@-echo "dbg - make the debug builds for the default system"
	@-echo "profile - make all the releases with profiling support for the default system"
	@-echo ""
	@-echo "Normal targets:"
	@-echo "(each of these targets must have the postfix -rel or -dbg)"
	@-echo "'sv-???' (Dedicated Server)"
	@-echo "'gl-???' (OpenGL rendering + Built-in Server)"
	@-echo "'m-???' (Merged client, OpenGL & D3D rendering + Dedicated server)"
	@-echo "'mingl-???' (Minimal featured OpenGL render)"
	@-echo "'d3d-???' (for windows builds)"
	@-echo "'mcl-???' (currently broken)"
	@-echo "'glcl-???' (currently broken)"
	@-echo "'droid-???' (cross compiles Android package)"
	@-echo "'web-???' (compiles javascript/emscripten page)"
	@-echo "'npfte-???' (cross compiles QuakeTV Firefox/Netscape browser plugin)"
	@-echo "'nacl-???' (cross compiles QuakeTV Firefox/Netscape browser plugin)"
	@-echo ""
	@-echo "Cross targets can be specified with FTE_TARGET=blah"
	@-echo "linux32, linux64 specify specific x86 archs"
	@-echo "SDL - Attempt to use sdl for the current target"
	@-echo "win32 - Mingw compile for win32"
	@-echo "vc - Attempts to use msvc8+ to compile. Note: uses profile guided optimisations. You must build+run the relevent profile target before a release target will compile properly. Debug doesn't care."
	@-echo "android, npfte, nacl targets explicitly cross compile, and should generally not be given an FTE_TARGET."

clean:
	-rm -f -r $(RELEASE_DIR)
	-rm -f -r $(DEBUG_DIR)
	-rm -f -r $(PROFILE_DIR)
	-rm -f -r droid/bin
	-rm -f -r droid/gen
	-rm -f -r droid/libs
	-rm -f droid/default.properties
	-rm -f droid/local.properties
	-rm -f droid/proguard.cfg
	-rm -f droid/build.xml

distclean: clean
	-rm -f droid/ftekeystore
	-rm -f -r libs/SDL2-$(SDL2VER)


#################################################
#npfte

npfte-tmprel: reldir
	@$(MAKE) $(OUT_DIR)/$(EXE_NAME)  OUT_DIR="$(OUT_DIR)" WCFLAGS="$(NPFTE_CFLAGS) $(RELEASE_CFLAGS)" LDFLAGS="$(NPFTE_LDFLAGS) $(LDFLAGS) $(RELEASE_LDFLAGS)" OBJS="NPFTE_OBJS"
npfte-tmpdbg: debugdir
	@$(MAKE) $(OUT_DIR)/$(EXE_NAME)  OUT_DIR="$(OUT_DIR)" WCFLAGS="$(NPFTE_CFLAGS) $(DEBUG_CFLAGS)" LDFLAGS="$(NPFTE_LDFLAGS) $(LDFLAGS) $(DEBUG_LDFLAGS)" OBJS="NPFTE_OBJS"
npfte-rel:
	-@$(MAKE) npfte-tmprel OUT_DIR="$(RELEASE_DIR)/$(NPFTEB_DIR)w32" EXE_NAME="../npfte.dll" PRECOMPHEADERS="" FTE_TARGET=win32
	-@$(MAKE) npfte-tmprel OUT_DIR="$(RELEASE_DIR)/$(NPFTEB_DIR)l32" EXE_NAME="../npfte32.so" PRECOMPHEADERS="" FTE_TARGET=linux32
	-@$(MAKE) npfte-tmprel OUT_DIR="$(RELEASE_DIR)/$(NPFTEB_DIR)l64" EXE_NAME="../npfte64.so" PRECOMPHEADERS="" FTE_TARGET=linux64
	-cp $(RELEASE_DIR)/npfte.dll npfte/plugins
	-cp $(RELEASE_DIR)/npfte32.so npfte/plugins
	-cp $(RELEASE_DIR)/npfte64.so npfte/plugins
	cd npfte && zip $(abspath $(RELEASE_DIR)/npfte.xpi) install.rdf plugins/npfte.dll plugins/npfte32.so plugins/npfte64.so
	rm -rf /tmp/npfte
	mkdir /tmp/npfte
	cp $(RELEASE_DIR)/npfte.dll /tmp/npfte
	cp ./npfte/manifest.json /tmp/npfte
	-cd $(RELEASE_DIR)/ && ../npfte/crxmake.sh /tmp/npfte ../npfte/chrome.pem
	rm -rf /tmp/npfte
npfte-dbg:
	@$(MAKE) npfte-tmpdbg OUT_DIR="$(DEBUG_DIR)/$(NPFTEB_DIR)w32" EXE_NAME="../npfte.dll" PRECOMPHEADERS="" FTE_TARGET=win32
	@$(MAKE) npfte-tmpdbg OUT_DIR="$(DEBUG_DIR)/$(NPFTEB_DIR)l32" EXE_NAME="../npfte32.so" PRECOMPHEADERS="" FTE_TARGET=linux32
	@$(MAKE) npfte-tmpdbg OUT_DIR="$(DEBUG_DIR)/$(NPFTEB_DIR)l64" EXE_NAME="../npfte64.so" PRECOMPHEADERS="" FTE_TARGET=linux64
npfte-profile:
	@$(MAKE) npfte-tmp TYPE=_npfte-profile OUT_DIR="$(PROFILE_DIR)/$(NPFTEB_DIR)"


#################################################
#nacl shortcut

nacl-rel:
	@$(MAKE) gl-rel FTE_TARGET=nacl NARCH=x86_32
	@$(MAKE) gl-rel FTE_TARGET=nacl NARCH=x86_64
	@$(MAKE) gl-rel FTE_TARGET=nacl NARCH=arm
	@$(MAKE) gl-rel FTE_TARGET=nacl NARCH=pnacl
nacl-dbg:
	@$(MAKE) gl-dbg FTE_TARGET=nacl NARCH=x86_32
	@$(MAKE) gl-dbg FTE_TARGET=nacl NARCH=x86_64
	@$(MAKE) gl-dbg FTE_TARGET=nacl NARCH=arm
	@$(MAKE) gl-dbg FTE_TARGET=nacl NARCH=pnacl

#################################################
#webgl helpers

#EMCC?=/opt/emsdk_portable/emscripten/master/emcc
EMCC?=emcc.bat --em-config $(shell cygpath -m $(USERPROFILE))/.emscripten
ifeq ($(EMSDK),)
	#just adds some extra paths (WINDOWS HOST ONLY)
	#assumes you installed the emscripten 1.22.0 sdk to EMSCRIPTENROOT
	#if you have a different version installed, you will need to fix up the paths yourself (or just use fte_target explicitly yourself).
	EMSCRIPTENROOT?=C:/Games/tools/Emscripten
	#EMSCRIPTENPATH=$(realpath $(EMSCRIPTENROOT)):$(realpath $(EMSCRIPTENROOT)/clang/e1.22.0_64bit):$(realpath $(EMSCRIPTENROOT)/node/0.10.17_64bit):$(realpath $(EMSCRIPTENROOT)/python/2.7.5.3_64bit):$(realpath $(EMSCRIPTENROOT)/emscripten/1.22.0):$(PATH)
	EMSCRIPTENPATH=$(realpath $(EMSCRIPTENROOT)):$(realpath $(EMSCRIPTENROOT)/clang/e1.35.0_64bit):$(realpath $(EMSCRIPTENROOT)/node/4.1.1_64bit/bin):$(realpath $(EMSCRIPTENROOT)/python/2.7.5.3_64bit):$(realpath $(EMSCRIPTENROOT)/emscripten/1.35.0):$(PATH)
else
	EMSCRIPTENPATH=$(PATH)
endif

web-rel:
	@PATH="$(EMSCRIPTENPATH)" $(MAKE) gl-rel FTE_TARGET=web CC="$(EMCC)"
	cp $(BASE_DIR)/web/fteshell.html $(RELEASE_DIR)/ftewebgl.html
	@gzip -f $(RELEASE_DIR)/ftewebgl.html
	@gzip -f $(RELEASE_DIR)/ftewebgl.js
	@gzip -f $(RELEASE_DIR)/ftewebgl.js.mem

web-dbg:
	@PATH="$(EMSCRIPTENPATH)" $(MAKE) gl-dbg FTE_TARGET=web CC="$(EMCC)"
	cp $(BASE_DIR)/web/fteshell.html $(DEBUG_DIR)/ftewebgl.html
	@gzip -f $(DEBUG_DIR)/ftewebgl.html
	@gzip -f $(DEBUG_DIR)/ftewebgl.js
	@gzip -f $(DEBUG_DIR)/ftewebgl.js.map

#################################################
#android

#building for android will require:
#download android sdk+ndk
#ant installed

#droid-dbg will install it on 'the current device', if you've got a device plugged in or an emulator running, it should just work.

#makes an ant project for us
droid/build.xml:
	-cd droid && PATH=$$PATH:$(realpath $(ANDROID_HOME)/tools):$(realpath $(ANDROID_NDK_ROOT)) $(ANDROID_SCRIPT) update project -t android-8 -p . -n FTEDroid

#build FTE as a library, then build the java+package (release)
droid/ftekeystore:
ifeq ($(KEYTOOLARGS),)
	@echo
	@echo In order to build a usable APK file it must be signed. That requires a private key.
	@echo Creation of a private key requries various bits of info...
	@echo You are expected to fill that stuff in now... By the way, don\'t forget the password!
	@echo Note that every time you use make droid-rel, you will be required to enter a password.
	@echo You can use \'make droid-opt\' instead if you wish to build an optimised build without signing,
	@echo  but such packages will require a rooted device \(or to be signed later\).
	@echo Just press control-c if you don\'t want to proceed.
	@echo Morality warning: never distribute droid/ftekeystore - always do make distclean before distributing.
	@echo
	$(JAVATOOL)keytool -genkey -v -keystore $@ -alias autogen -keyalg RSA -keysize 2048 -validity 10000
else
	@echo Generating keystore
	@$(JAVATOOL)keytool -genkey -keystore $@ -alias autogen -keyalg RSA -keysize 2048 -validity 10000 -noprompt $(KEYTOOLARGS)
endif

droid-rel:
	$(MAKE) FTE_TARGET=droid droid/build.xml droid/ftekeystore
	$(foreach a, $(DROID_ARCH), $(MAKE) FTE_TARGET=droid m-rel DROID_ARCH=$a; )
	@$(foreach a, $(DROID_ARCH), mkdir -p droid/libs/$a; )
	-@$(foreach a, $(DROID_ARCH), cp $(RELEASE_DIR)/m_droid-$a/libftedroid.so droid/libs/$a/libftedroid.so; )

	@cd droid && $(ANT) release
ifneq ($(DROID_PACKSU),)
		@echo
		@echo Adding custom data files - non-compressed
		@echo
		zip droid/bin/FTEDroid-release-unsigned.apk -0 -j $(DROID_PACKSU)
endif
ifneq ($(DROID_PACKSC),)
		@echo
		@echo Adding custom data files - compressed
		@echo
		zip droid/bin/FTEDroid-release-unsigned.apk -9 -j $(DROID_PACKSC)
endif
	@echo
	@echo
	@echo Signing package... I hope you remember your password.
	@echo
	@$(JAVATOOL)jarsigner $(JARSIGNARGS) -digestalg SHA1 -sigalg MD5withRSA -keystore droid/ftekeystore droid/bin/FTEDroid-release-unsigned.apk autogen
	-rm -f $(RELEASE_DIR)/FTEDroid.apk
	$(ANDROID_ZIPALIGN) 4 droid/bin/FTEDroid-release-unsigned.apk $(NATIVE_RELEASE_DIR)/FTEDroid.apk

droid-opt:
	$(MAKE) FTE_TARGET=droid droid/build.xml droid/ftekeystore
	$(MAKE) FTE_TARGET=droid gl-rel
	mkdir -p droid/libs/armeabi
	@cp $(RELEASE_DIR)/libftedroid.so droid/libs/armeabi/
	@cd droid && $(ANT) release
	cp droid/bin/FTEDroid-unsigned.apk $(RELEASE_DIR)/FTEDroid.apk

#build FTE as a library, then build the java+package (release). also installs it onto the 'current' device.
droid-dbg:
	$(MAKE) FTE_TARGET=droid droid/build.xml
	$(foreach a, $(DROID_ARCH), $(MAKE) FTE_TARGET=droid m-dbg DROID_ARCH=$a; )
	@$(foreach a, $(DROID_ARCH), mkdir -p droid/libs/$a; )
	-@$(foreach a, $(DROID_ARCH), cp $(DEBUG_DIR)/m_droid-$a/libftedroid.so droid/libs/$a/libftedroid.so; )
	@cd droid && $(ANT) debug #&& $(ANT) debug install
	cp droid/bin/FTEDroid-debug.apk $(DEBUG_DIR)/FTEDroid.apk

droid-help:
	@-echo "make droid-dbg - compiles engine with debug info and signs package with debug key. Attempts to install onto emulator."
	@-echo "make droid-opt - compiles engine with optimisations, but does not sign package. Not useful."
	@-echo "make droid-rel - compiles engine with optimisations, adds custom data files, signs with private key, requires password."
	@-echo
	@-echo "Android Settings:
	@-echo "DROID_PACKSC: specifies additional pak or pk3 files to compress into the package, which avoids extra configuration. Only used in release builds. You probably shouldn't use this except for really small packages. Any file seeks will give really poor performance."
	@-echo "DROID_PACKSU: like DROID_PACKSC, but without compression. Faster loading times, but bigger. Use for anything that is already compressed (especially pk3s)."
	@-echo "ANDROID_HOME: path to the android sdk install path."
	@-echo "ANDROID_NDK_ROOT: path to the android ndk install path."
	@-echo "ANT: path and name of apache ant. Probably doesn't need to be set if you're on linux."
	@-echo "JAVA_HOME: path of your java install. Commonly already set in environment settings."
	@-echo "JAVATOOL: path to your java install's bin directory. Doesn't need to be set if its already in your path."
	@-echo "JARSIGNARGS: Additional optional arguments to java's jarsigner program. You may want to put -storepass FOO in here, but what ever you do - keep it secure. Avoid bash history snooping, etc. If its not present, you will safely be prompted as required."
	@-echo
	@-echo "Note that 'make droid-rel' will automatically generate a keystore. If you forget the password, just do a 'make dist-clean'."

$(BASE_DIR)/libs/SDL2-$(SDL2VER)/i686-w64-mingw32/bin/sdl2-config:
	wget http://www.libsdl.org/release/SDL2-devel-$(SDL2VER)-mingw.tar.gz -O $(BASE_DIR)/sdl2.tar.gz
	cd $(BASE_DIR)/libs && tar -xvzf $(BASE_DIR)/sdl2.tar.gz
	rm $(BASE_DIR)/sdl2.tar.gz
$(BASE_DIR)/libs/SDL2-$(SDL2VER)/x86_64-w64-mingw32/bin/sdl2-config: $(BASE_DIR)/libs/SDL2-$(SDL2VER)/i686-w64-mingw32/bin/sdl2-config



#makes sure the configure scripts get the right idea.
AR?=$(ARCH)-ar


CONFIGARGS+= -host=$(ARCH) --enable-shared=no CC="$(CC)"
CONFIGARGS:= $(CONFIGARGS)
#--disable-silent-rules

TOOLOVERRIDES+=CFLAGS="$$CFLAGS -Os"


libs-$(ARCH)/libjpeg.a:
	test -f jpegsrc.v$(JPEGVER).tar.gz || wget http://www.ijg.org/files/jpegsrc.v$(JPEGVER).tar.gz
	-test -f libs-$(ARCH)/libjpeg.a || (mkdir -p libs-$(ARCH) && cd libs-$(ARCH) && tar -xvzf ../jpegsrc.v$(JPEGVER).tar.gz && cd jpeg-$(JPEGVER) && $(TOOLOVERRIDES) ./configure $(CONFIGARGS) && $(TOOLOVERRIDES) $(MAKE) && cp .libs/libjpeg.a ../ && $(TOOLOVERRIDES) $(AR) -s ../libjpeg.a && cp jconfig.h jerror.h jmorecfg.h jpeglib.h jversion.h ../ )

libs-$(ARCH)/libz.a:
	test -f zlib-$(ZLIBVER).tar.gz || wget http://zlib.net/zlib-$(ZLIBVER).tar.gz
	-test -f libs-$(ARCH)/libz.a || (mkdir -p libs-$(ARCH) && cd libs-$(ARCH) && tar -xvzf ../zlib-$(ZLIBVER).tar.gz && cd zlib-$(ZLIBVER) && $(TOOLOVERRIDES) ./configure --static && $(TOOLOVERRIDES) $(MAKE) libz.a CC="$(CC) $(W32_CFLAGS) -fPIC" && cp libz.a ../ && $(TOOLOVERRIDES) $(AR) -s ../libz.a && cp zlib.h zconf.h zutil.h ../ )

libs-$(ARCH)/libpng.a: libs-$(ARCH)/libz.a
	test -f libpng-$(PNGVER).tar.gz || wget http://prdownloads.sourceforge.net/libpng/libpng-$(PNGVER).tar.gz?download -O libpng-$(PNGVER).tar.gz
	-test -f libs-$(ARCH)/libpng.a || (mkdir -p libs-$(ARCH) && cd libs-$(ARCH) && tar -xvzf ../libpng-$(PNGVER).tar.gz && cd libpng-$(PNGVER) && $(TOOLOVERRIDES) ./configure CPPFLAGS=-I$(NATIVE_ABSBASE_DIR)/libs-$(ARCH)/ LDFLAGS=-L$(NATIVE_ABSBASE_DIR)/libs-$(ARCH)/ $(CONFIGARGS) --enable-static && $(TOOLOVERRIDES) $(MAKE) && cp .libs/libpng16.a ../libpng.a && cp png*.h ../ )

libs-$(ARCH)/libogg.a:
	test -f libogg-$(OGGVER).tar.gz || wget http://downloads.xiph.org/releases/ogg/libogg-$(OGGVER).tar.gz
	-test -f libs-$(ARCH)/libogg.a || (mkdir -p libs-$(ARCH) && cd libs-$(ARCH) && tar -xvzf ../libogg-$(OGGVER).tar.gz && cd libogg-$(OGGVER) && $(TOOLOVERRIDES) ./configure $(CONFIGARGS) && $(TOOLOVERRIDES) $(MAKE) && cp src/.libs/libogg.a ../ && $(TOOLOVERRIDES) $(AR) -s ../libogg.a && mkdir ../ogg && cp include/ogg/*.h ../ogg)

libs-$(ARCH)/libvorbis.a: libs-$(ARCH)/libogg.a
	test -f libvorbis-$(VORBISVER).tar.gz || wget http://downloads.xiph.org/releases/vorbis/libvorbis-$(VORBISVER).tar.gz
	-test -f libs-$(ARCH)/libvorbisfile.a || (mkdir -p libs-$(ARCH) && cd libs-$(ARCH) && tar -xvzf ../libvorbis-$(VORBISVER).tar.gz && cd libvorbis-$(VORBISVER) && $(TOOLOVERRIDES) ./configure PKG_CONFIG= $(CONFIGARGS) --disable-oggtest --with-ogg-libraries=.. --with-ogg-includes=$(NATIVE_ABSBASE_DIR)/libs-$(ARCH)/libogg-$(OGGVER)/include && $(TOOLOVERRIDES) $(MAKE) && cp lib/.libs/libvorbis.a ../ && cp lib/.libs/libvorbisfile.a ../  && mkdir ../vorbis && cp include/vorbis/*.h ../vorbis)

libs-$(ARCH)/libopus.a:
	test -f opus-$(OPUSVER).tar.gz || wget https://archive.mozilla.org/pub/opus/opus-$(OPUSVER).tar.gz
	-test -f libs-$(ARCH)/libopus.a || (mkdir -p libs-$(ARCH) && cd libs-$(ARCH) && tar -xvzf ../opus-$(OPUSVER).tar.gz && cd opus-$(OPUSVER) && CFLAGS="$(CFLAGS) -Os" $(TOOLOVERRIDES) ./configure $(CONFIGARGS) && $(TOOLOVERRIDES) $(MAKE) && cp .libs/libopus.a ../ && cp include/opus*.h ../)

libs-$(ARCH)/libspeex.a:
	test -f speex-$(SPEEXVER).tar.gz || wget http://downloads.us.xiph.org/releases/speex/speex-$(SPEEXVER).tar.gz
	-test -f libs-$(ARCH)/libspeex.a || (mkdir -p libs-$(ARCH)/speex && cd libs-$(ARCH) && tar -xvzf ../speex-$(SPEEXVER).tar.gz && cd speex-$(SPEEXVER) && CFLAGS="$(CFLAGS) -Os" $(TOOLOVERRIDES) ./configure $(CONFIGARGS) && $(TOOLOVERRIDES) $(MAKE) && cp libspeex/.libs/libspeex.a ../ && cp -r include/speex/*.h ../speex/)

libs-$(ARCH)/libspeexdsp.a:
	test -f speexdsp-$(SPEEXDSPVER).tar.gz || wget http://downloads.xiph.org/releases/speex/speexdsp-$(SPEEXDSPVER).tar.gz
	-test -f libs-$(ARCH)/libspeexdsp.a || (mkdir -p libs-$(ARCH)/speex && cd libs-$(ARCH) && tar -xvzf ../speexdsp-$(SPEEXDSPVER).tar.gz && cd speexdsp-$(SPEEXDSPVER) && CFLAGS="$(CFLAGS) -Os" $(TOOLOVERRIDES) ./configure $(CONFIGARGS) && $(TOOLOVERRIDES) $(MAKE) && cp libspeexdsp/.libs/libspeexdsp.a ../ && cp -r include/speex/*.h ../speex/)

libs-$(ARCH)/libfreetype.a:
	test -f freetype-$(FREETYPEVER).tar.gz || wget https://download.savannah.gnu.org/releases/freetype/freetype-$(FREETYPEVER).tar.gz
	-test -f libs-$(ARCH)/libfreetype.a || (mkdir -p libs-$(ARCH) && cd libs-$(ARCH) && tar -xvzf ../freetype-$(FREETYPEVER).tar.gz && cd freetype-$(FREETYPEVER) && CFLAGS="$(CFLAGS) -Os" $(TOOLOVERRIDES) ./configure $(CONFIGARGS) && $(TOOLOVERRIDES) $(MAKE) && cp objs/.libs/libfreetype.a ../ && cp -r include/ ../)

makelibs: libs-$(ARCH)/libjpeg.a libs-$(ARCH)/libz.a libs-$(ARCH)/libpng.a libs-$(ARCH)/libogg.a libs-$(ARCH)/libvorbis.a libs-$(ARCH)/libopus.a libs-$(ARCH)/libspeex.a libs-$(ARCH)/libspeexdsp.a libs-$(ARCH)/libfreetype.a

HTTP_OBJECTS=http/httpserver.c http/iwebiface.c common/fs_stdio.c http/ftpserver.c
$(RELEASE_DIR)/httpserver$(BITS): $(HTTP_OBJECTS)
	$(CC) -o $(RELEASE_DIR)/httpserver$(BITS) -Icommon -Iclient -Iqclib -Igl -Iserver -DWEBSERVER -DWEBSVONLY -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -DNO_PNG $(HTTP_OBJECTS)
httpserver: $(RELEASE_DIR)/httpserver$(BITS)

IQM_OBJECTS=../iqm/iqm.cpp
$(RELEASE_DIR)/iqm$(BITS): $(IQM_OBJECTS)
	$(CC) -o $(RELEASE_DIR)/iqm$(BITS) $(IQM_OBJECTS) -lstdc++ -lm
iqm: $(RELEASE_DIR)/iqm$(BITS)

utils: httpserver iqm

prefix ?= /usr/local
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
sbindir ?= $(exec_prefix)/sbin
INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= ${INSTALL} -m 644
install: sv-rel gl-rel mingl-rel qcc-rel
	$(INSTALL_PROGRAM) $(RELEASE_DIR)/$(EXE_NAME)-gl $(DESTDIR)$(bindir)/$(EXE_NAME)-gl
	$(INSTALL_PROGRAM) $(RELEASE_DIR)/$(EXE_NAME)-mingl $(DESTDIR)$(bindir)/$(EXE_NAME)-mingl
	$(INSTALL_PROGRAM) $(RELEASE_DIR)/$(EXE_NAME)-sv $(DESTDIR)$(bindir)/$(EXE_NAME)-sv
	$(INSTALL_PROGRAM) $(RELEASE_DIR)/fteqcc $(DESTDIR)$(bindir)/fteqcc
