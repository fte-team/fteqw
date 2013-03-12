CC=gcc
WINDRES=windres
STRIP=strip

STRIPFLAGS=--strip-unneeded --remove-section=.comment

CPUOPTIMIZATIONS=

BASE_DIR=.

SVNREVISION:=-DSVNREVISION=$(shell test -d .svn && svnversion || echo -)

WHOAMI=$(shell whoami)

#only limited forms of cross-making is supported
#only the following 3 are supported
#linux->win32 (FTE_TARGET=win32) RPM Package: "mingw32-gcc", DEB Package: "mingw32"
#linux->linux32 (FTE_TARGET=linux32)
#linux->linux64 (FTE_TARGET=linux64)
#linux->morphos (FTE_TARGET=morphos) Contact Moodles for crosscompiler
#linux->macosx (FTE_TARGET=macosx) or (FTE_TARGET=macosx_x86) Contact Moodles for crosscompiler, only works with Mac OSX 10.4 Tiger.
#if you are cross compiling, you'll need to use FTE_TARGET=mytaget

#correct the gcc build when cross compiling
ifeq ($(FTE_TARGET),win32)
	ifeq ($(shell $(CC) -v 2>&1 | grep mingw),)
		#CC didn't state that it was mingw... so try fixing that up
		ifneq ($(shell which i586-mingw32msvc-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=i586-mingw32msvc-gcc
			WINDRES=i586-mingw32msvc-windres
			STRIP=i586-mingw32msvc-strip
#			BITS?=32
		endif
	endif
endif

#correct the gcc build when cross compiling
ifeq ($(FTE_TARGET),win64)
	ifeq ($(shell $(CC) -v 2>&1 | grep mingw),)
		#CC didn't state that it was mingw... so try fixing that up
		ifneq ($(shell which x86_64-w64-mingw32-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=x86_64-w64-mingw32-gcc -m64
			WINDRES=x86_64-w64-mingw32-windres
			STRIP=x86_64-w64-mingw32-strip
#			BITS=64
		endif
		ifneq ($(shell which amd64-mingw32msvc-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=amd64-mingw32msvc-gcc -m64
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

DROID_NDK_PATH?=~/droid/android-ndk-r6b
DROID_SDK_PATH?=~/droid/android-sdk-linux_x86
ANT?=ant
JAVATOOL="$(JAVA_HOME)"/bin/
ANDROID_SCRIPT=android

ifeq ($(WHOAMI),moodles)
	ANT_HOME=/home/moodles/apache-ant-1.8.2/
	ANT=/home/moodles/apache-ant-1.8.2/bin/ant
	JAVA_HOME=/home/moodles/jdk1.6.0_25/
	DROID_NDK_PATH=/home/moodles/android_ndk
	DROID_SDK_PATH=/home/moodles/android_sdk
	JARSIGNARGS="-storepass fteqw1"
endif

ifeq ($(DROID_ARCH),)
	DROID_ARCH=armeabi
endif
ifeq ($(FTE_TARGET),droid)
	#if we're running under windows, then we want to run some other binary
	ifeq ($(shell uname -o 2>&1 | grep Cygwin),)
		#set up for linux
		NDK_PATH:=$(shell echo $(DROID_NDK_PATH))
		SDK_PATH:=$(shell echo $(DROID_SDK_PATH))
		TOOLCHAIN:=$(NDK_PATH)/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/arm-linux-androideabi-
	else
		#we're running upon windows
		NDK_PATH:=$(shell cygpath -m $(DROID_NDK_PATH))
		SDK_PATH:=$(shell cygpath -m $(DROID_SDK_PATH))
		ifeq ($(DROID_ARCH),x86)
			TOOLCHAIN:=$(NDK_PATH)/toolchains/x86-4.4.3/prebuilt/windows/bin/i686-android-linux-
		else
			TOOLCHAIN:=$(NDK_PATH)/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows/bin/arm-linux-androideabi-
		endif
		ANDROID_SCRIPT=android.bat
		#make can't cope with absolute paths in dependancy files
		NODEPS = 1
	endif

	DROID_API_LEVEL=4

	ifeq ($(DROID_ARCH),x86)
		DROID_PLAT_INC=arch-x86
		#google fecked up. anything before api_level 9 will fail to compile on x86
		DROID_API_LEVEL=9
	else
		DROID_PLAT_INC=arch-arm

		#there are 3 ABI targets
		#armv5 (works on all arm droids)
		#armv7 (more common on 2.2+ droids)
		#armv7+neon
		DROID_ABI?=-mfloat-abi=softfp
	endif

	STRIP=$(TOOLCHAIN)strip
	CC=$(TOOLCHAIN)gcc -I$(NDK_PATH)/platforms/android-$(DROID_API_LEVEL)/$(DROID_PLAT_INC)/usr/include/ -DANDROID $(DROID_ABI) -fno-strict-aliasing
	DO_LD=$(DO_ECHO) $(CC) -Wl,-soname,libftedroid.so -shared -Wl,--no-undefined -Wl,-z,noexecstack --sysroot=$(NDK_PATH)/platforms/android-$(DROID_API_LEVEL)/$(DROID_PLAT_INC) -L$(NDK_PATH)/platforms/android-$(DROID_API_LEVEL)/$(DROID_PLAT_INC)/usr/lib -o $@ $(LTO_LD) $(WCFLAGS) $(CFLAGS) -llog -lc -lz -lm
endif

#correct the gcc build when cross compiling
ifeq ($(FTE_TARGET),win32_SDL)
	ifeq ($(shell $(CC) -v 2>&1 | grep mingw),)
		#CC didn't state that it was mingw... so try fixing that up
		ifneq ($(shell which i586-mingw32msvc-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=i586-mingw32msvc-gcc
			WINDRES=i586-mingw32msvc-windres
			STRIP=i586-mingw32msvc-strip
#			BITS=32
		endif
	endif
endif

ifeq ($(FTE_TARGET),win64_sdl)
	FTE_TARGET=win64_SDL
endif

#correct the gcc build when cross compiling
ifeq ($(FTE_TARGET),win64_SDL)
	ifeq ($(shell $(CC) -v 2>&1 | grep mingw),)
		#CC didn't state that it was mingw... so try fixing that up
		ifneq ($(shell which amd64-mingw32msvc-gcc 2> /dev/null),)
			#yup, the alternative exists (this matches the one debian has)
			CC=amd64-mingw32msvc-gcc -m64
			WINDRES=amd64-mingw32msvc-windres
			STRIP=amd64-mingw32msvc-strip
#			BITS=64
		endif
	endif
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

#if you have an x86, you can get gcc to build 32bit or 64bit specific builds, instead of builds for the native platform
ifeq ($(FTE_TARGET),linux32)
	FTE_TARGET=linux
	CC=gcc -m32
	BITS=32
endif
ifeq ($(FTE_TARGET),linux64)
	FTE_TARGET=linux
	CC=gcc -m64
	BITS=64
endif


ifeq ($(FTE_TARGET),) 	#user didn't specify prefered target
	ifneq ($(shell $(CC) -v 2>&1 | grep mingw),)
		FTE_TARGET=win32
	endif
	ifneq ($(shell $(CC) -v 2>&1 | grep cygwin),)
		FTE_TARGET=cygwin
		ANDROID_SCRIPT=android.bat
	endif
	ifeq ($(FTE_TARGET),) 	#still not set

		ifeq ($(shell uname),Linux)
			FTE_TARGET=linux
		endif
		ifeq ($(shell uname),Darwin)
			FTE_TARGET=macosx
		endif
		ifeq ($(shell uname),FreeBSD)
			FTE_TARGET=bsd
		endif
		ifeq ($(shell uname),NetBSD)
			FTE_TARGET=bsd
		endif
		ifeq ($(shell uname),OpenBSD)
			FTE_TARGET=bsd
		endif
		ifeq ($(shell uname),MorphOS)
			FTE_TARGET=morphos
		endif
		#else I've no idea what it is you're running
	endif

	FTE_TARGET ?= sdl	#so go for sdl.
endif

ifneq ($(shell ls|grep config.h),)
	HAVECONFIG=-DHAVE_CONFIG_H
endif

CLIENT_DIR=$(BASE_DIR)/client
GL_DIR=$(BASE_DIR)/gl
D3D_DIR=$(BASE_DIR)/d3d
SERVER_DIR=$(BASE_DIR)/server
COMMON_DIR=$(BASE_DIR)/common
HTTP_DIR=$(BASE_DIR)/http
#LIBS_DIR=$(BASE_DIR)/libs
LIBS_DIR?=.
PROGS_DIR=$(BASE_DIR)/qclib
NACL_DIR=$(BASE_DIR)/nacl
BOTLIB_DIR=$(BASE_DIR)/botlib

RELEASE_DIR=$(BASE_DIR)/release
DEBUG_DIR=$(BASE_DIR)/debug
PROFILE_DIR=$(BASE_DIR)/profile

ALL_CFLAGS=$(HAVECONFIG) $(CFLAGS) $(BASE_CFLAGS) $(WCFLAGS)

#cheap compile-everything-in-one-unit (compile becomes preprocess only)
ifneq ($(WPO),)
	LTO_CC= -E
	LTO_LD= -combine -fwhole-program -x c
	LTO_END=ltoxnone
	LTO_START=ltoxc
endif
#proper/consistant link-time optimisations (requires gcc 4.5+ or so)
ifneq ($(LTO),)
	LTO_CC=-flto
	LTO_LD=-flto
endif

#DO_ECHO=@echo $< && 
DO_ECHO=@
#DO_ECHO=
DO_CC=$(DO_ECHO) $(CC) $(LTO_CC) $(ALL_CFLAGS) -o $@ -c $<

ifeq ($(FTE_TARGET),vc)
	BASELDFLAGS=
endif
ifeq ($(FTE_TARGET),cygwin)
	BASELDFLAGS=-lm
endif
ifeq ($(FTE_TARGET),morphos)
	BASELDFLAGS=-lm
endif
ifeq ($(FTE_TARGET),bsd)
	BASELDFLAGS=-lm
endif
BASELDFLAGS ?= -lm -ldl -lpthread

ifeq ($(shell echo $(FTE_TARGET)|grep -v win),)
	BASELDFLAGS=-lm
	MINGW_LIBS_DIR=$(LIBS_DIR)/mingw-libs

	ifeq ($(shell echo $(FTE_TARGET)|grep -v win64),)
		MINGW_LIBS_DIR=$(LIBS_DIR)/mingw64-libs
	endif

	IMAGELDFLAGS=$(MINGW_LIBS_DIR)/libpng.a $(MINGW_LIBS_DIR)/libz.a $(MINGW_LIBS_DIR)/libjpeg.a
	OGGVORBISLDFLAGS=$(MINGW_LIBS_DIR)/libvorbisfile.a $(MINGW_LIBS_DIR)/libvorbis.a $(MINGW_LIBS_DIR)/libogg.a

	ifeq ($(shell echo $(FTE_TARGET)|grep -v -i _SDL),)
		RELEASE_CFLAGS+= -D_SDL
		SDL_LDFLAGS=$(MINGW_LIBS_DIR)/libSDL.a $(MINGW_LIBS_DIR)/libSDLmain.a -L./libs/mingw64-libs
	endif
endif

IMAGELDFLAGS ?= -lpng -ljpeg
OGGVORBISLDFLAGS ?= -lvorbisfile -lvorbis -logg

#BASELDFLAGS=-lm  -lz
GLXLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext $(IMAGELDFLAGS)
GLSLDFLAGS=-L/usr/X11R6/lib -lMesaGL -lglide -lvga
XLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext $(IMAGELDFLAGS)
SLDFLAGS=-lvga

ifeq ($(FTE_TARGET),vc)
	WARNINGFLAGS=-W3 -D_CRT_SECURE_NO_WARNINGS
	GNUC_FUNCS=
else
	WARNINGFLAGS=-Wall -Wno-pointer-sign -Wno-unknown-pragmas -Wno-format-zero-length

	GNUC_FUNCS= -Dstrnicmp=strncasecmp -Dstricmp=strcasecmp
endif

BASE_CFLAGS=$(WARNINGFLAGS) $(GNUC_FUNCS) -I$(CLIENT_DIR) -I$(SERVER_DIR) -I$(COMMON_DIR) -I$(GL_DIR) -I$(D3D_DIR) -I$(PROGS_DIR) -I$(BOTLIB_DIR) -I. -I$(LIBS_DIR) -I$(LIBS_DIR)/dxsdk9/include -I$(LIBS_DIR)/dxsdk7/include -I$(LIBS_DIR)/sdl/include -I/usr/include/SDL -I$(LIBS_DIR)/sdl/include/SDL -I./libs/freetype2/include -I./libs/freetype2/include/freetype -I./libs/speex -DBOTLIB $(SVNREVISION)
CLIENT_ONLY_CFLAGS=-DCLIENTONLY
SERVER_ONLY_CFLAGS=-DSERVERONLY
JOINT_CFLAGS=
DEBUG_CFLAGS=-ggdb -g
RELEASE_CFLAGS?=-O3 -ffast-math $(CPUOPTIMIZATIONS)

#incase our compiler doesn't support it (mingw)
ifeq ($(shell LANG=c $(CC) -rdynamic 2>&1 | grep unrecognized),)
	DEBUG_CFLAGS+= -rdynamic
endif

OBJS+=$(SPEEX_OBJS)
PROFILE_CFLAGS=-pg

DX7SDK=-I./libs/dxsdk7/include/

GLCFLAGS=-DGLQUAKE
D3DCFLAGS=-DD3D9QUAKE
NPFTECFLAGS=-DNPFTE
SPEEXCFLAGS=-DSPEEX_STATIC -I$(BASE_DIR)/libs/speex/include -DFIXED_POINT -DUSE_KISS_FFT -DEXPORT=""

SPEEX_OBJS = \
	bits.o \
	buffer.o \
	cb_search.o \
	exc_10_16_table.o \
	exc_10_32_table.o \
	exc_20_32_table.o \
	exc_5_256_table.o \
	exc_5_64_table.o \
	exc_8_128_table.o \
	fftwrap.o \
	filterbank.o \
	filters.o \
	gain_table.o \
	gain_table_lbr.o \
	hexc_10_32_table.o \
	hexc_table.o \
	high_lsp_tables.o \
	jitter.o \
	kiss_fft.o \
	kiss_fftr.o \
	lpc.o \
	lsp.o \
	lsp_tables_nb.o \
	ltp.o \
	mdf.o \
	modes.o \
	modes_wb.o \
	nb_celp.o \
	preprocess.o \
	quant_lsp.o \
	resample.o \
	sb_celp.o \
	scal.o \
	smallft.o \
	speex.o \
	speex_callbacks.o \
	speex_header.o \
	stereo.o \
	vbr.o \
	vq.o \
	window.o

CLIENT_OBJS = \
	textedit.o	\
	fragstats.o	\
	teamplay.o	\
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
	m_download.o	\
	m_master.o	\
	m_multi.o	\
	m_items.o	\
	m_options.o	\
	m_single.o	\
	m_script.o	\
	m_mp3.o	\
	roq_read.o	\
	clq2_cin.o	\
	net_master.o	\
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
	view.o	\
	wad.o			\
				\
	ftpclient.o		\
	httpclient.o 	\
				\
				\
	pr_menu.o

GLQUAKE_OBJS =		\
	gl_draw.o		\
	gl_backend.o		\
	gl_rmain.o		\
	gl_rmisc.o		\
	gl_rsurf.o		\
	gl_screen.o		\
	gl_bloom.o		\
	gl_vidcommon.o

D3DQUAKE_OBJS =		\
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
	gl_model.o	\
	gl_shader.o	\
	gl_shadow.o	\
	gl_rlight.o	\
	gl_hlmdl.o	\
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

PROGS_OBJS =		\
	comprout.o		\
	hash.o		\
	initlib.o		\
	pr_bgcmd.o		\
	pr_skelobj.o		\
	pr_edict.o		\
	pr_exec.o		\
	pr_multi.o		\
	pr_x86.o		\
	qcc_cmdlib.o	\
	qccmain.o		\
	qcc_pr_comp.o	\
	qcc_pr_lex.o	\
	qcdecomp.o		\
	qcd_main.o

SERVER_OBJS = 		\
	pr_cmds.o 		\
	pr_q1qvm.o	\
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
	svmodel.o		\
	sv_sys_unix.o

WINDOWSSERVERONLY_OBJS = \
	svmodel.o	\
	sv_sys_win.o	\
	sys_win_threads.o

COMMON_OBJS = \
	gl_alias.o		\
	gl_heightmap.o		\
	com_mesh.o	\
	com_phys_ode.o	\
	common.o 		\
	cvar.o 		\
	cmd.o 		\
	crc.o 		\
	fs.o			\
	fs_stdio.o		\
	fs_pak.o		\
	fs_zip.o		\
	mathlib.o 		\
	huff.o		\
	md4.o 		\
	sha1.o		\
	log.o 		\
	net_chan.o 		\
	net_wins.o 		\
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

#the defaults for sdl come first
GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o
GL_EXE_NAME=../fteqw_sdl.gl$(BITS)
GLCL_EXE_NAME=../fteqwcl_sdl.gl$(BITS)
ifdef windir
	GL_LDFLAGS=$(GLLDFLAGS) -lmingw32 -lws2_32 `sdl-config --libs`
else
	GL_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(OGGVORBISLDFLAGS) `sdl-config --libs`
endif
GL_CFLAGS=$(GLCFLAGS) `sdl-config --cflags`
GLB_DIR=gl_sdl$(FTE_TARGET)$(BITS)
GLCL_DIR=glcl_sdl$(FTE_TARGET)$(BITS)

SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS)
SV_EXE_NAME=../fteqw_sdl.sv$(BITS)
SV_CFLAGS=$(SERVER_ONLY_CFLAGS)
SV_LDFLAGS=-lz

MINGL_DIR=mingl_sdl$(FTE_TARGET)$(BITS)
MINGL_EXE_NAME=../fteqw_sdl.mingl$(BITS)

MB_DIR=m_sdl$(FTE_TARGET)$(BITS)
M_EXE_NAME=../fteqw_sdl$(BITS)
MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o 
M_CFLAGS=$(GLCFLAGS) `sdl-config --cflags` -D_MERGED_SDL
M_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(OGGVORBISLDFLAGS)

ifdef windir
	M_LDFLAGS=$(MLDFLAGS) -lmingw32 -lws2_32 -lSDLmain -lSDL
else
	#pthread is needed because of SDL.
	M_LDFLAGS=$(MLDFLAGS) `sdl-config --libs` $(IMAGELDFLAGS) $(OGGVORBISLDFLAGS)
endif


#specific targets override those defaults as needed.
#google native client
ifeq ($(FTE_TARGET),nacl)
	OGGVORBISLDFLAGS=

	NARCH ?= x86_32
	ifeq ($(shell uname -o 2>&1 | grep Cygwin),)
		MYOS=linux
	else
		MYOS=win
	endif

	CC=
	STRIP=
	ifeq ($(NARCH),x86_32)
		CC=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_newlib/bin/i686-nacl-gcc -DNACL -m32
		STRIP=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_newlib/bin/i686-nacl-strip
		BITS=
	endif
	ifeq ($(NARCH),x86_64)
		CC=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_newlib/bin/i686-nacl-gcc -DNACL -m64
		STRIP=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_newlib/bin/i686-nacl-strip
		BITS=
	endif
	ifeq ($(NARCH),arm)
		CC=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_arm_newlib/bin/arm-nacl-gcc -DNACL
		STRIP=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_arm_newlib/bin/arm-nacl-strip
		BITS=
	endif
	ifeq ($(NARCH),pnacl)
		CC=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_pnacl/newlib/bin/pnacl-clang -DNACL
		STRIP=$(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_pnacl/newlib/bin/pnacl-strip
		STRIPFLAGS=
		DO_LD =/cygdrive/c/Windows/system32/cmd /C $(NACL_SDK_ROOT)/toolchain/$(MYOS)_x86_pnacl/newlib/bin/pnacl-clang -o $@ $(LTO_LD) $(WCFLAGS) $(CFLAGS)
		BITS=
	endif

	BASELDFLAGS = -lm -lppapi_gles2 -lnosys -lppapi
	IMAGELDFLAGS =

	GL_CFLAGS+=$(SPEEXCFLAGS)

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SPEEX_OBJS) sys_ppapi.o cd_null.o gl_vidppapi.o fs_ppapi.o snd_ppapi.o 

	GLB_DIR=gl_nacl_$(NARCH)
	MINGL_DIR=mingl_nacl_$(NARCH)
	ifeq ($(NARCH),pnacl)
		GL_EXE_NAME=../fteqw.pexe
		GLCL_EXE_NAME=../fteqwcl.pexe
		MINGL_EXE_NAME=../fteqw_mingl.pexe
	else
		GL_EXE_NAME=../fteqw_$(NARCH).nexe
		GLCL_EXE_NAME=../fteqwcl_$(NARCH).nexe
		MINGL_EXE_NAME=../fteqw_mingl_$(NARCH).nexe
	endif
endif

#FTE_TARGET=win32_SDL | FTE_TARGET=win64_SDL (MinGW32 + SDL | MinGW64 + SDL)
ifeq ($(shell echo $(FTE_TARGET)|grep -E -i -v "win(32|64)_sdl$$"),)	
	LIBS_DIR=./libs
	
	ifeq ($(shell echo $(FTE_TARGET)|grep -i -v "win64_sdl"),)
		BITS=64
	endif
	
	#the defaults for sdl come first
	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) $(SPEEX_OBJS)  gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(LTO_END) resources.o $(LTO_START)
	GL_EXE_NAME=../fteqw_sdl_gl$(BITS).exe
	GLCL_EXE_NAME=../fteqwcl_sdl$(BITS).exe
	ifdef windir
		GL_LDFLAGS=$(GLLDFLAGS) -lmingw32 -lws2_32 `sdl-config --libs`
	else
		GL_LDFLAGS=$(IMAGELDFLAGS) $(OGGVORBISLDFLAGS) -lws2_32 -lmingw32 $(SDL_LDFLAGS) -mwindows -ldxguid -lwinmm -lole32 $(GLLDFLAGS) `sdl-config --libs` 
	endif
	
	GL_CFLAGS=-D_SDL -I$(MINGW_LIBS_DIR)/ -I$(MINGW_LIBS_DIR) -I$(LIBS_DIR) $(GLCFLAGS) -DLIBVORBISFILE_STATIC `sdl-config --cflags` $(DX7SDK) $(SPEEXCFLAGS)
	ifeq ($(shell echo $(FTE_TARGET)|grep -E -i -v "win32.*sdl"),)
		GL_CFLAGS+= -D_MINGW_VFPRINTF
	endif
	
	GLB_DIR=gl_mgw_sdl$(BITS)
	GLCL_DIR=glcl_mgw_sdl$(BITS)
	
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(WINDOWSSERVERONLY_OBJS) $(BOTLIB_OBJS) $(LTO_END) resources.o $(LTO_START)
	SV_EXE_NAME=../fteqw_sdl_sv$(BITS).exe
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) -D_SDL
	SV_LDFLAGS=$(MINGW_LIBS_DIR)/libz.a -lm -lmingw32 -lws2_32 -lwinmm

	MINGL_DIR=mingl_sdlwin$(BITS)
	MINGL_EXE_NAME=../fteqw_sdl_mingl$(BITS).exe

	MB_DIR=m_mgw_sdl$(BITS)
	M_EXE_NAME=../fteqw_sdl$(BITS).exe
	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(D3DQUAKE_OBJS) $(BOTLIB_OBJS) $(SPEEX_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(LTO_END) resources.o $(LTO_START)
	M_CFLAGS=$(D3DCFLAGS) -D_SDL -I$(LIBS_DIR) -I$(MINGW_LIBS_DIR)/ -I$(MINGW_LIBS_DIR) $(GLCFLAGS) -DLIBVORBISFILE_STATIC `sdl-config --cflags` -D_MERGED_SDL $(DX7SDK) $(SPEEXCFLAGS)

	ifeq ($(shell echo $(FTE_TARGET)|grep -E -i -v "win32.*sdl"),)
		M_CFLAGS+= -D_MINGW_VFPRINTF
	endif

	M_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(OGGVORBISLDFLAGS)
	
	ifdef windir
		M_LDFLAGS=$(MLDFLAGS) -lmingw32 -lws2_32 -lSDLmain -lSDL
	else
		#pthread is needed because of SDL.
		M_LDFLAGS=$(IMAGELDFLAGS) $(OGGVORBISLDFLAGS) -lws2_32 -lmingw32 $(MINGW_LIBS_DIR)/libSDL.a $(MINGW_LIBS_DIR)/libSDLmain.a -mwindows -ldxguid -lwinmm -lole32 $(MLDFLAGS) `sdl-config --libs` $(IMAGELDFLAGS)
	endif

	D3DCL_OBJS=$(D3DQUAKE_OBJS) $(SPEEX_OBJS) snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o snd_directx.o $(D3DGL_OBJS) $(LTO_END) resources.o $(LTO_START)
	D3D_EXE_NAME=../fted3d_sdl_qw$(BITS).exe
	D3DCL_EXE_NAME=../fted3d_sdl_clqw$(BITS).exe
	D3D_LDFLAGS=$(IMAGELDFLAGS) $(OGGVORBISLDFLAGS) -lws2_32 -lmingw32 $(SDL_LDFLAGS) -mwindows -ldxguid -lwinmm -lole32
	D3D_CFLAGS=$(D3DCFLAGS) -D_SDL -DNO_XFLIP -I$(LIBS_DIR) -I$(MINGW_LIBS_DIR)/  -I$(MINGW_LIBS_DIR) -DLIBVORBISFILE_STATIC `sdl-config --cflags` $(DX7SDK) $(SPEEXCFLAGS)
	ifeq ($(shell echo $(FTE_TARGET)|grep -E -i -v "win32.*sdl"),)
		D3D_CFLAGS+= -D_MINGW_VFPRINTF
	endif
	D3DB_DIR=sdl_d3d_mgw$(BITS)
	D3DCL_DIR=sdl_d3dcl_mgw$(BITS)
	
endif

#FTE_TARGET=vc (Visual C)
ifeq ($(FTE_TARGET),vc)
	ifeq ($(BITS),64)
		WINDRES=x86_64-w64-mingw32-windres

		MSVCPATH=C:/Program Files (x86)/Microsoft Visual Studio 8/VC/BIN/amd64/

		MSVCINC=-I"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\ATLMFC\INCLUDE" -I"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\INCLUDE" -I"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\PlatformSDK\include" -I"C:\Program Files (x86)\Microsoft Visual Studio 8\SDK\v2.0\include"
		MSVCLIB=/LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\ATLMFC\LIB\amd64" /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\LIB\amd64" /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\PlatformSDK\lib\amd64" /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 8\SDK\v2.0\LIB\AMD64"
		JPEGLIB=libs/libjpeg$(BITS).lib
	else
		MSVCPATH=C:/Program Files (x86)/Microsoft Visual Studio 8/VC/BIN/

		MSVCINC=-I"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\ATLMFC\INCLUDE" -I"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\INCLUDE" -I"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\PlatformSDK\include" -I"C:\Program Files (x86)\Microsoft Visual Studio 8\SDK\v2.0\include"
		MSVCLIB=/LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\ATLMFC\LIB" /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\LIB" /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\PlatformSDK\lib" /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 8\SDK\v2.0\LIB"
		JPEGLIB=libs/jpeg.lib
	endif
	STRIP=@echo strip

	CC="$(MSVCPATH)cl" $(MSVCINC) -D_CRT_SECURE_NO_WARNINGS
	DEBUG_CFLAGS ?= -Od $(CPUOPTIMIZATIONS) /fp:fast
	PROFILE_CFLAGS = -O2 -Ot -Ox -GL $(CPUOPTIMISATIONS) /fp:fast 
	PROFILE_LDFLAGS = /LTCG:PGINSTRUMENT
	RELEASE_CFLAGS = -O2 -Ot -Ox -GL $(CPUOPTIMIZATIONS) /fp:fast
	RELEASE_LDFLAGS = /LTCG:PGOPTIMIZE

	DO_CC=@$(CC) /nologo $(ALL_CFLAGS) -Fo$@ -c $<
	DO_LD=$(DO_ECHO) "$(MSVCPATH)link" /nologo /out:"$@" /nodefaultlib:libc.lib /LARGEADDRESSAWARE /nodefaultlib:MSVCRT $(MSVCLIB) /manifest:no /OPT:REF  wsock32.lib user32.lib advapi32.lib winmm.lib libs/zlib$(BITS).lib shell32.lib
	PRECOMPHEADERS = 
	NODEPS = 1

	LIBS_DIR=./libs/

	SPEEXCFLAGS+= -Dinline=_inline -D_USE_MATH_DEFINES


	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) $(W32_CFLAGS) -DMULTITHREAD -DMSVCLIBSPATH="libs/"
	SV_EXE_NAME=../fteqwsv$(BITS).exe
	SV_DIR=sv_vc$(BITS)
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(WINDOWSSERVERONLY_OBJS) fs_win32.o resources.o
	SV_LDFLAGS=/subsystem:console

	GL_EXE_NAME=../fteglqw$(BITS).exe
	GLCL_EXE_NAME=../minfteqw.gl$(BITS)
	GLB_DIR=gl_vc$(BITS)
	GLCL_DIR=glcl_vc$(BITS)
	GL_LDFLAGS=$(GLLDFLAGS) $(JPEGLIB) libs/libpng$(BITS).lib uuid.lib gdi32.lib ole32.lib /subsystem:windows
	GL_CFLAGS=$(GLCFLAGS) $(W32_CFLAGS) -DMULTITHREAD $(SPEEXCFLAGS) -DMSVCLIBSPATH="libs/"
	GLCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SPEEX_OBJS) gl_vidnt.o snd_win.o snd_directx.o cd_win.o fs_win32.o in_win.o sys_win.o sys_win_threads.o resources.o
	GL_OBJS=

	MINGL_DIR=mingl_vc$(BITS)
	MINGL_EXE_NAME=../fteminglqw$(BITS).exe

	D3DCL_OBJS=$(D3DQUAKE_OBJS) $(SPEEX_OBJS) snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_win_threads.o $(D3DGL_OBJS) fs_win32.o $(LTO_END) resources.o $(LTO_START)
	D3D_EXE_NAME=../fted3dqw$(BITS).exe
	D3DCL_EXE_NAME=../fted3dclqw$(BITS).exe
	D3D_LDFLAGS=$(JPEGLIB) libs/libpng$(BITS).lib uuid.lib gdi32.lib ole32.lib /subsystem:windows
	D3D_CFLAGS=$(D3DCFLAGS) $(W32_CFLAGS) $(DX7SDK) -DMULTITHREAD $(SPEEXCFLAGS) -DMSVCLIBSPATH="libs/"
	D3DB_DIR=d3d_vc$(BITS)
	D3DCL_DIR=d3dcl_vc$(BITS)
	
	M_EXE_NAME=../fteqw$(BITS).exe
	MCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(D3DQUAKE_OBJS) $(GLQUAKE_OBJS) $(SPEEX_OBJS) fs_win32.o gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_win_threads.o resources.o
	M_CFLAGS=$(D3DCFLAGS) $(GLCFLAGS) $(W32_CFLAGS) $(D3DCFLAGS) -DMULTITHREAD $(SPEEXCFLAGS) -DMSVCLIBSPATH="libs/"
	MB_DIR=m_vc$(BITS)
	M_LDFLAGS=$(GLLDFLAGS) $(JPEGLIB) libs/libpng$(BITS).lib uuid.lib gdi32.lib ole32.lib /subsystem:windows
endif

#FTE_TARGET=win32 | FTE_TARGET=win64 (MinGW32 | MinGW64)
ifeq ($(shell echo $(FTE_TARGET)|grep -E -v "win(32|64)$$"),)
	# The extra object file called resources.o is specific for MinGW to link the icon in

	#cygwin's gcc requires an extra command to use mingw instead of cygwin (default paths, etc).
	ifneq ($(shell $(CC) -v 2>&1 | grep cygwin),)
		W32_CFLAGS=-mno-cygwin
	endif
	
	ifeq ($(FTE_TARGET),win64)
		BITS=64
	endif

	BASELDFLAGS=

	# Allow 32bit FTE to access beyond the 2GB address space
	ifeq ($(FTE_TARGET),win32)
		BASELDFLAGS=-Wl,--large-address-aware
	endif

	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) $(W32_CFLAGS)

	LIBS_DIR = $(BASE_DIR)/libs
	SV_EXE_NAME=../fteqwsv$(BITS).exe
	SV_LDFLAGS=$(MINGW_LIBS_DIR)/libz.a -lws2_32 -lwinmm
	SV_DIR=sv_mingw$(BITS)
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(WINDOWSSERVERONLY_OBJS) $(BOTLIB_OBJS) fs_win32.o $(LTO_END) resources.o $(LTO_START)


	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) $(SPEEX_OBJS) fs_win32.o gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_win_threads.o $(LTO_END) resources.o $(LTO_START)
	GL_EXE_NAME=../fteglqw$(BITS).exe
	GLCL_EXE_NAME=../fteglqwcl$(BITS).exe
	GL_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(OGGVORBISLDFLAGS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32
	GL_CFLAGS=$(GLCFLAGS) $(W32_CFLAGS) $(DX7SDK) -DMULTITHREAD -DLIBVORBISFILE_STATIC $(SPEEXCFLAGS)
	GLB_DIR=gl_mgw$(BITS)
	GLCL_DIR=glcl_mgw$(BITS)

	NPFTE_OBJS=httpclient.o image.o sys_win_threads.o sys_npfte.o sys_axfte.o sys_plugfte.o $(LTO_END) npplug.o ../../ftequake/npapi.def $(LTO_START)
	NPFTE_DLL_NAME=../npfte$(BITS).dll
	NPFTE_LDFLAGS=-Wl,--enable-stdcall-fixup $(IMAGELDFLAGS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32 -loleaut32 -luuid -lstdc++ -shared
	NPFTE_CFLAGS=$(NPFTECFLAGS) $(W32_CFLAGS) -DMULTITHREAD
	NPFTEB_DIR=npfte_mgw$(BITS)

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(D3DQUAKE_OBJS) $(BOTLIB_OBJS) $(SPEEX_OBJS) fs_win32.o gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_win_threads.o $(LTO_END) resources.o $(LTO_START)
	M_EXE_NAME=../fteqw$(BITS).exe
	MCL_EXE_NAME=../fteqwcl$(BITS).exe
	M_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) $(OGGVORBISLDFLAGS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32
	M_CFLAGS=$(GLCFLAGS) $(W32_CFLAGS) $(D3DCFLAGS) $(DX7SDK) -DMULTITHREAD -DLIBVORBISFILE_STATIC $(SPEEXCFLAGS)
	MB_DIR=m_mgw$(BITS)
	MCL_DIR=mcl_mgw$(BITS)

	D3DCL_OBJS=$(D3DQUAKE_OBJS) $(SPEEX_OBJS) snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_win_threads.o $(D3DGL_OBJS) fs_win32.o $(LTO_END) resources.o $(LTO_START)
	D3D_EXE_NAME=../fted3dqw$(BITS).exe
	D3DCL_EXE_NAME=../fted3dclqw$(BITS).exe
	D3D_LDFLAGS=$(IMAGELDFLAGS) $(OGGVORBISLDFLAGS) -ldxguid -lws2_32 -lwinmm -lgdi32 -lole32
	D3D_CFLAGS=$(D3DCFLAGS) $(W32_CFLAGS) $(DX7SDK) -DMULTITHREAD -DLIBVORBISFILE_STATIC $(SPEEXCFLAGS)
	D3DB_DIR=d3d_mgw$(BITS)
	D3DCL_DIR=d3dcl_mgw$(BITS)

	MINGL_EXE_NAME=../fteminglqw$(BITS).exe
	MINGL_DIR=mingl_mgw$(BITS)
endif

ifeq ($(FTE_TARGET),bsd)
	#mostly uses the linux stuff.
	#oss, X, etc.

	SV_DIR=sv_linux
	SV_LDFLAGS=-lz -ldl

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_linux_threads.o
	GL_EXE_NAME=../fteqw.gl
	GLCL_EXE_NAME=../fteqwcl.gl
	GL_LDFLAGS= -L/usr/local/lib $(GLLDFLAGS) $(GLXLDFLAGS) -lpthread
	GL_CFLAGS=$(GLCFLAGS) -I/usr/local/include -I/usr/X11R6/include 
	GLB_DIR=gl_bsd
	GLCL_DIR=glcl_bsd

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_linux_threads.o
	M_EXE_NAME=../fteqw
	MCL_EXE_NAME=../fteqwcl
	M_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS) -lpthread
	M_CFLAGS=$(GLCFLAGS) -I/usr/X11R6/include
	MB_DIR=m_bsd
	MCL_DIR=mcl_bsd

	MINGL_EXE_NAME=../fteqw.mingl
	MINGL_DIR=mingl_bsd
endif
ifneq ($(shell echo $(FTE_TARGET)|grep linux),)

	SV_DIR=sv_linux$(BITS)
	SV_LDFLAGS=-lz
	SV_EXE_NAME=../fteqw.sv$(BITS)
	
	NPFTE_OBJS=httpclient.o image.o sys_linux_threads.o sys_npfte.o sys_axfte.o sys_plugfte.o
	NPFTE_DLL_NAME=../npfte$(BITS).so
	NPFTE_LDFLAGS=-shared -Wl,-z,defs -lpthread
	NPFTE_CFLAGS=$(NPFTECFLAGS) $(W32_CFLAGS) -DMULTITHREAD -fPIC -DDYNAMIC_LIBPNG -DDYNAMIC_LIBJPEG
	NPFTEB_DIR=npfte_linux$(BITS)

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) $(SPEEX_OBJS) gl_vidlinuxglx.o gl_videgl.o snd_alsa.o snd_linux.o cd_linux.o sys_linux.o sys_linux_threads.o
	GL_EXE_NAME=../fteqw.gl$(BITS)
	GLCL_EXE_NAME=../fteqwcl.gl$(BITS)
	GL_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS) $(OGGVORBISLDFLAGS) -lz
	GL_CFLAGS=$(GLCFLAGS) -I/usr/X11R6/include $(SPEEXCFLAGS) -DMULTITHREAD -DLIBVORBISFILE_STATIC
	GLB_DIR=gl_linux$(BITS)
	GLCL_DIR=glcl_linux$(BITS)

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) $(SPEEX_OBJS) gl_vidlinuxglx.o snd_linux.o snd_alsa.o cd_linux.o sys_linux.o sys_linux_threads.o
	M_EXE_NAME=../fteqw$(BITS)
	MCL_EXE_NAME=../fteqwcl$(BITS)
	M_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS) $(OGGVORBISLDFLAGS) -lz
	M_CFLAGS=$(GLCFLAGS) -I/usr/X11R6/include $(SPEEXCFLAGS) -DLIBVORBISFILE_STATIC
	MB_DIR=m_linux$(BITS)
	MCL_DIR=mcl_linux$(BITS)



	MINGL_EXE_NAME=../fteqw.mingl$(BITS)
	MINGL_DIR=mingl_linux$(BITS)
endif
ifneq ($(shell echo $(FTE_TARGET)|grep macosx),)
	SV_DIR=sv_macosx$(EXTENSION)$(BITS)
	GLB_DIR=gl_macosx$(EXTENSION)$(BITS)
	GLCL_DIR=glcl_macosx$(EXTENSION)$(BITS)
	MINGL_DIR=mingl_macosx$(EXTENSION)$(BITS)
	
	OGGVORBISLDFLAGS=

	GL_CFLAGS=$(GLCFLAGS) -D__MACOSX__ -L/sw/lib -I/sw/include -L/opt/local/lib -I/opt/local/include -I$(LIBS_DIR)
	ifeq ($(WHOAMI),moodles)
		GL_CFLAGS+= -I/home/moodles/mac/include/ -L/home/moodles/mac/lib 
	endif
	
	ifeq ($(FTE_TARGET),macosx_x86)
		GL_CFLAGS=$(GLCFLAGS) -D__MACOSX__ -L/sw/lib -I/sw/include -L/opt/local/lib -I/opt/local/include -I$(LIBS_DIR)
		ifeq ($(WHOAMI),moodles)
			GL_CFLAGS+= -I/home/moodles/mac/x86/include/ -L/home/moodles/mac/x86/lib
		endif
	endif

	GL_LDFLAGS=-framework AGL -framework OpenGL -framework Cocoa -framework AudioUnit -lz -lpng -ljpeg
	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) gl_vidcocoa.mo gl_vidmacos.o sys_linux.o cd_null.o snd_macos.o sys_linux_threads.o

	GL_EXE_NAME=../macosx_fteqw.gl$(EXTENSION)$(BITS)
	GLCL_EXE_NAME=../macosx_fteqwcl.gl$(EXTENSION)$(BITS)
	M_EXE_NAME=../macosx_fteqw$(EXTENSION)$(BITS)
	MCL_EXE_NAME=../macosx_fteqwcl$(EXTENSION)$(BITS)
	MINGL_EXE_NAME=../macosx_fteqw.mingl$(EXTENSION)$(BITS)
	MINGL_DIR=mingl_macosx$(EXTENSION)$(BITS)
	
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS)
	SV_EXE_NAME=../macosx_fteqw.sv$(EXTENSION)$(BITS)
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
	OGGVORBISLDFLAGS=

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_vidmorphos.o in_morphos.o snd_morphos.o cd_null.o sys_morphos.o
	GL_EXE_NAME=../morphos_fteqw.gl
	GLCL_EXE_NAME=../morphos_fteqwcl.gl
	GL_LDFLAGS=$(GLLDFLAGS) -ldl $(IMAGELDFLAGS) -lz
	GL_CFLAGS=$(GLCFLAGS) -noixemul -I./libs/speex -I./
	GLB_DIR=gl_morphos
	GLCL_DIR=glcl_morphos

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) gl_vidmorphos.o vid_morphos.o in_morphos.o snd_morphos.o cd_null.o sys_morphos.o
	M_EXE_NAME=../morphos_fteqw
	MCL_EXE_NAME=../morphos_fteqwcl
	M_LDFLAGS=$(GLLDFLAGS)
	M_CFLAGS=$(GLCFLAGS)
	MB_DIR=m_morphos
	MCL_DIR=mcl_morphos

	MINGL_EXE_NAME=../morphos_fteqw.mingl
	MINGL_DIR=mingl_morphos
	
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS) $(BOTLIB_OBJS)
	SV_EXE_NAME=../morphos_fteqw.sv$(BITS)
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS)
	
	ifeq ($(WHOAMI),moodles)
		SV_CFLAGS+=-I/home/moodles/zlib/zlib-1.2.5 -L/home/moodles/zlib/zlib-1.2.5 -I/home/moodles/fteqw/trunk/engine/libs -D__MORPHOS_SHAREDLIBS -I/home/moodles/morphos/os-include/
	endif
endif


ifeq ($(FTE_TARGET),cygwin)

	SV_DIR=sv_cygwin
	SV_LDFLAGS=-lz

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) $(SPEEX_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_linux_threads.o
	GL_EXE_NAME=../fteqwglcyg.exe
	GLCL_EXE_NAME=../fteqwclglcyg.exe
	GL_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS) $(OGGVORBISLDFLAGS)
	GL_CFLAGS=$(GLCFLAGS) -I/usr/X11R6/include $(SPEEXCFLAGS) -DLIBVORBISFILE_STATIC
	GLB_DIR=gl_cygwin
	GLCL_DIR=glcl_cygwin

	MCL_OBJS=$(D3DGL_OBJS) $(GLQUAKE_OBJS) $(SOFTWARE_OBJS) $(BOTLIB_OBJS) $(SPEEX_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_linux_threads.o
	M_EXE_NAME=../fteqwcyg.exe
	MCL_EXE_NAME=../fteqwclcyg.exe
	M_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS) $(OGGVORBISLDFLAGS)
	M_CFLAGS=$(GLCFLAGS) $(SPEEXCFLAGS) -DLIBVORBISFILE_STATIC
	MB_DIR=m_cygwin
	MCL_DIR=mcl_cygwin



	MINGL_EXE_NAME=../fteqwminglcyg.exe
	MINGL_DIR=mingl_cygwin
endif

ifeq ($(FTE_TARGET),droid)
	BASELDFLAGS=

	SV_DIR=sv_droid-$(DROID_ARCH)
	SV_LDFLAGS=-lz
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(BOTLIB_OBJS) svmodel.o sys_droid.o 
	SV_EXE_NAME=libftedroid.so

	SV_LDFLAGS=

	GLCL_OBJS=$(GL_OBJS) $(D3DGL_OBJS) $(GLQUAKE_OBJS) $(BOTLIB_OBJS) gl_viddroid.o sys_droid.o cd_null.o snd_droid.o
    GL_LDFLAGS=$(GLLDFLAGS)
	GLB_DIR=gl_droid-$(DROID_ARCH)
	GL_EXE_NAME=libftedroid.so
endif

SV_DIR?=sv_sdl

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
	-$(MAKE) FTE_TARGET=win32 npfte-rel

autoconfig: clean
	/bin/bash makeconfig.sh y

config: clean
	/bin/bash makeconfig.sh

ifneq ($(OUT_DIR),)
-include $(OUT_DIR)/*.o.d
endif


VPATH = $(BASE_DIR) : $(CLIENT_DIR) : $(GL_DIR) : $(COMMON_DIR) : $(SERVER_DIR) : $(HTTP_DIR) : $(BASE_DIR)/irc : $(BASE_DIR)/email : $(QUX_DIR) : $(PROGS_DIR) : $(NACL_DIR) : $(D3D_DIR) : $(BOTLIB_DIR) : $(BASE_DIR)/libs/speex/libspeex

ifneq ($(findstring -DSPEEX_STATIC, $(CFLAGS)),)
	#add these to statically link libspeex
	VPATH += : $(BASE_DIR)/libs/speex/libspeex
	BASE_CFLAGS += -DSPEEX_STATIC -I$(BASE_DIR)/libs/speex/include -DFIXED_POINT -DUSE_KISS_FFT -DEXPORT=""
	CLIENT_OBJS += $(SPEEX_OBJS)
endif

# This is for linking the FTE icon to the MinGW target
$(OUT_DIR)/resources.o : winquake.rc
	$(WINDRES) -I$(CLIENT_DIR) -O coff $< $@

#npAPI stuff requires some extra resources
$(OUT_DIR)/npplug.o : ftequake/npplug.rc
	$(WINDRES) -I$(CLIENT_DIR) -O coff $< $@


#$(OUT_DIR)/%.d: %.c
#	@set -e; rm -f $@; \
#	$(CC) -MM $(ALL_CFLAGS) $< > $@.$$$$; \
#	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
#	rm -f $@.$$$$

$(OUT_DIR)/%.o $(OUT_DIR)/%.d : %.c
ifeq ($(NODEPS),)
	@set -e; rm -f $@.d; \
	 $(CC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.o[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	 rm -f $@.d.$$$$
endif
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.o $(OUT_DIR)/%.d : %.cpp
ifeq ($(NODEPS),)
	@set -e; rm -f $@.d; \
	$(CC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	sed 's,\($*\)\.o[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	rm -f $@.d.$$$$
endif
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.oo $(OUT_DIR)/%.d : %.c
ifeq ($(NODEPS),)
	@set -e; rm -f $@.d; \
	 $(CC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.oo[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	 rm -f $@.d.$$$$
endif
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.mo $(OUT_DIR)/%.d : %.m
	@set -e; rm -f $@.d; \
	 $(CC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.mo[ :]*,$@ $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 sed -e 's/.*://' -e 's/\\$$//' < $@.d.$$$$ | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $@.d; \
	 rm -f $@.d.$$$$
	$(DO_CC) -I$(OUT_DIR)

#enables use of precompiled headers in gcc 3.4 onwards.
$(OUT_DIR)/quakedef.h.gch : quakedef.h
	$(CC) -x c $(BASE_CFLAGS) $(WCFLAGS) -o $@ -c $< $(CFLAGS)
PRECOMPHEADERS ?= $(OUT_DIR)/quakedef.h.gch

#addprefix is to add the ./release/server/ part of the object name
#foreach is needed as the OBJS is a list of variable names containing object lists.
#which is needed as windows sucks too much for the chaining to carry a full list.
#god knows how gcc loads the list properly.
#or at least I hope he does. It makes no sence to mortals.

DO_LD ?=$(DO_ECHO) $(CC) -o $@ $(LTO_LD) $(WCFLAGS) $(CFLAGS)
$(OUT_DIR)/$(EXE_NAME):   $(PRECOMPHEADERS) $(foreach fn, $(CUSTOMOBJS) $(foreach ol, $(OBJS), $($(ol))),$(if $(findstring ltox,$(fn)),,$(OUT_DIR)/$(fn)))
	$(DO_LD) $(foreach fn, $(CUSTOMOBJS) $(foreach ol, $(OBJS) $(LTO_END), $($(ol))),$(if $(findstring ltox,$(fn)),$(subst ltox,-x ,$(fn)),$(OUT_DIR)/$(fn)) ) $(LDFLAGS)

_out-rel:
	@$(MAKE) $(OUT_DIR)/$(EXE_NAME) EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(WCFLAGS) $(RELEASE_CFLAGS)" LDFLAGS="$(BASELDFLAGS) $(LDFLAGS) $(RELEASE_LDFLAGS)" OBJS="$(OBJS)"
	$(STRIP) $(STRIPFLAGS) $(OUT_DIR)/$(EXE_NAME)

_out-dbg:
	@$(MAKE) $(OUT_DIR)/$(EXE_NAME) EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(WCFLAGS) $(DEBUG_CFLAGS)" LDFLAGS="$(BASELDFLAGS) $(LDFLAGS) $(DEBUG_LDFLAGS)" OBJS="$(OBJS)"

_out-profile:
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
	@$(MAKE) $(TYPE) OUT_DIR="$(OUT_DIR)" EXE_NAME="$(SV_EXE_NAME)" WCFLAGS="$(SV_CFLAGS)" LDFLAGS="$(SV_LDFLAGS) $(LDFLAGS)" OBJS="SV_OBJS"
sv-rel:
	@$(MAKE) sv-tmp TYPE=_out-rel OUT_DIR="$(RELEASE_DIR)/$(SV_DIR)"
sv-dbg:
	@$(MAKE) sv-tmp TYPE=_out-dbg OUT_DIR="$(DEBUG_DIR)/$(SV_DIR)"
sv-profile:
	@$(MAKE) sv-tmp TYPE=_out-profile OUT_DIR="$(PROFILE_DIR)/$(SV_DIR)"

d3dcl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(D3DCL_EXE_NAME)" WCFLAGS="$(D3D_CFLAGS)" LDFLAGS="$(D3D_LDFLAGS) $(LDFLAGS)" SOBJS="$(D3DCL_OBJS)"
d3d-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(D3D_EXE_NAME)" WCFLAGS="$(D3D_CFLAGS)" LDFLAGS="$(D3D_LDFLAGS) $(LDFLAGS)" SOBJS="$(D3DCL_OBJS)"

d3dcl-rel:
	@$(MAKE) d3dcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(D3DCL_DIR)"
d3dcl-dbg:
	@$(MAKE) d3dcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(D3DCL_DIR)"
d3dcl-profile:
	@$(MAKE) d3dcl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(D3DCL_DIR)"

d3d-rel:
	@$(MAKE) d3d-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(D3DB_DIR)"
d3d-dbg:
	@$(MAKE) d3d-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(D3DB_DIR)"
d3d-profile:
	@$(MAKE) d3d-tmp TYPE=_clsv-profile OUT_DIR="$(PROFILE_DIR)/$(D3DB_DIR)"


glcl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(GLCL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS)" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"
gl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(GL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS)" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"

glcl-rel:
	@$(MAKE) glcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(GLCL_DIR)"
glcl-dbg:
	@$(MAKE) glcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(GLCL_DIR)"
glcl-profile:
	@$(MAKE) glcl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(GLCL_DIR)"
gl-rel:
	@$(MAKE) gl-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(GLB_DIR)"
gl-dbg:
	@$(MAKE) gl-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(GLB_DIR)"
gl-profile:
	@$(MAKE) gl-tmp TYPE=_clsv-profile OUT_DIR="$(PROFILE_DIR)/$(GLB_DIR)"

mingl-tmp: reldir
	@$(MAKE) $(TYPE) OUT_DIR="$(OUT_DIR)" EXE_NAME="$(MINGL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS) -DMINIMAL" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"
mingl-rel:
	@$(MAKE) mingl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(MINGL_DIR)"
mingl-dbg:
	@$(MAKE) mingl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(MINGL_DIR)"
mingl-profile:
	@$(MAKE) mingl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(MINGL_DIR)"

mcl-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(MCL_EXE_NAME)" WCFLAGS="$(M_CFLAGS)" LDFLAGS="$(M_LDFLAGS) $(LDFLAGS)" SOBJS="$(MCL_OBJS)"
m-tmp:
	@$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(M_EXE_NAME)" WCFLAGS="$(M_CFLAGS)" LDFLAGS="$(M_LDFLAGS) $(LDFLAGS)" SOBJS="$(MCL_OBJS)"

mcl-rel:
	@$(MAKE) mcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(MCL_DIR)"
mcl-dbg:
	@$(MAKE) mcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(MCL_DIR)"
mcl-profile:
	@$(MAKE) mcl-tmp TYPE=_cl-profile OUT_DIR="$(PROFILE_DIR)/$(MCL_DIR)"
m-rel:
	@$(MAKE) m-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(MB_DIR)"
m-dbg:
	@$(MAKE) m-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(MB_DIR)"
m-profile:
	@$(MAKE) m-tmp TYPE=_clsv-profile OUT_DIR="$(PROFILE_DIR)/$(MB_DIR)"

.PHONY: m-tmp mcl-tmp mingl-tmp glcl-tmp gl-tmp sv-tmp _clsv-dbg _clsv-rel _cl-dbg _cl-rel _out-rel _out-dbg


ifdef windir
debugdir:
	@-mkdir -p $(subst /,\, $(OUT_DIR))
reldir:
	@-mkdir -p $(subst /,\, $(OUT_DIR))
else
reldir:
	@-mkdir -p $(RELEASE_DIR)
	@-mkdir -p $(OUT_DIR)
debugdir:
	@-mkdir -p $(DEBUG_DIR)
	@-mkdir -p $(OUT_DIR)
endif

plugins:
	ifneq ($(shell grep all ../plugins/Makefile),)
	$(MAKE) -C ../plugins
	else
	@-echo no plugins directory installed
	endif
plugins:

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
	@-echo "'npfte-???' (cross compiles QuakeTV Firefox/Netscape browser plugin)"
	@-echo "'nacl-???' (cross compiles QuakeTV Firefox/Netscape browser plugin)"
	@-echo ""
	@-echo "Cross targets can be specified with FTE_TARGET=blah"
	@-echo "linux32, linux64 specify specific x86 archs"
	@-echo "SDL - Attempt to use sdl for the current target"
	@-echo "win32 - Mingw compile for win32"
	@-echo "vc - Attempts to use msvc8+ to compile. Note: uses profile guided optimisations. You must build+run the relevent profile target before a release target will compile properly. Debug doesn't care."
	@-echo "android, npfte, nacl targets explicitly cross compile, and should generally not be given an FTE_TARGET."

install:
	-cp debug/*.* /opt/quake/
	-cp release/*.* /opt/quake/
	-cp profile/*.* /opt/quake

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
#android

#building for android will require:
#download android sdk+ndk
#ant installed
#DROID_NDK_PATH and DROID_SDK_PATH set, if they don't match the paths on my system.

#droid-dbg will install it on 'the current device', if you've got a device plugged in or an emulator running, it should just work.

#makes an ant project for us
droid/build.xml:
	-cd droid && PATH=$$PATH:$(DROID_SDK_PATH)/tools:$(DROID_NDK_PATH) $(ANDROID_SCRIPT) update project -t android-8 -p . -n FTEDroid

#build FTE as a library, then build the java+package (release)
droid/ftekeystore:
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

droid-rel: droid/build.xml droid/ftekeystore
	$(foreach a, $(DROID_ARCH), $(MAKE) FTE_TARGET=droid gl-rel DROID_ARCH=$a; )
	@$(foreach a, $(DROID_ARCH), mkdir -p droid/libs/$a; )
	-@$(foreach a, $(DROID_ARCH), cp $(RELEASE_DIR)/gl_droid-$a/libftedroid.so droid/libs/$a/libftedroid.so; )

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
	$(DROID_SDK_PATH)/tools/zipalign 4 droid/bin/FTEDroid-release-unsigned.apk $(RELEASE_DIR)/FTEDroid.apk

droid-opt: droid/build.xml droid/ftekeystore
	$(MAKE) FTE_TARGET=droid gl-rel
	mkdir -p droid/libs/armeabi
	@cp $(RELEASE_DIR)/libftedroid.so droid/libs/armeabi/
	@cd droid && $(ANT) release
	cp droid/bin/FTEDroid-unsigned.apk $(RELEASE_DIR)/FTEDroid.apk

#build FTE as a library, then build the java+package (release). also installs it onto the 'current' device.
droid-dbg: droid/build.xml
	$(foreach a, $(DROID_ARCH), $(MAKE) FTE_TARGET=droid gl-dbg DROID_ARCH=$a; )
	@$(foreach a, $(DROID_ARCH), mkdir -p droid/libs/$a; )
	-@$(foreach a, $(DROID_ARCH), cp $(DEBUG_DIR)/gl_droid-$a/libftedroid.so droid/libs/$a/libftedroid.so; )
	@cd droid && $(ANT) debug && $(ANT) debug install
	cp droid/bin/FTEDroid-debug.apk $(DEBUG_DIR)/FTEDroid.apk

droid-help:
	@-echo "make droid-dbg - compiles engine with debug info and signs package with debug key. Attempts to install onto emulator."
	@-echo "make droid-opt - compiles engine with optimisations, but does not sign package. Not useful."
	@-echo "make droid-rel - compiles engine with optimisations, adds custom data files, signs with private key, requires password."
	@-echo
	@-echo "Android Settings:
	@-echo "DROID_PACKSC: specifies additional pak or pk3 files to compress into the package, which avoids extra configuration. Only used in release builds. You probably shouldn't use this except for really small packages. Any file seeks will give really poor performance."
	@-echo "DROID_PACKSU: like DROID_PACKSC, but without compression. Faster loading times, but bigger. Use for anything that is already compressed (especially pk3s)."
	@-echo "DROID_SDK_PATH: path to the android sdk install path."
	@-echo "DROID_NDK_PATH: path to the android ndk install path."
	@-echo "ANT: path and name of apache ant. Probably doesn't need to be set if you're on linux."
	@-echo "JAVA_HOME: path of your java install. Commonly already set in environment settings."
	@-echo "JAVATOOL: path to your java install's bin directory. Doesn't need to be set if its already in your path."
	@-echo "JARSIGNARGS: Additional optional arguments to java's jarsigner program. You may want to put -storepass FOO in here, but what ever you do - keep it secure. Avoid bash history snooping, etc. If its not present, you will safely be prompted as required."
	@-echo
	@-echo "Note that 'make droid-rel' will automatically generate a keystore. If you forget the password, just do a 'make dist-clean'."
