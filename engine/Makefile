CC=gcc
WINDRES=windres
STRIP=strip

STRIPFLAGS=--strip-unneeded --remove-section=.comment

CPUOPTIMIZATIONS=

BASE_DIR=.

#only limited forms of cross-making is supported
#only the following 3 are supported
#linux->win32
#linux->linux32
#linux->linux64
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
SW_DIR=$(BASE_DIR)/sw
D3D7_DIR=$(BASE_DIR)/d3d
D3D9_DIR=$(BASE_DIR)/d3d9
SERVER_DIR=$(BASE_DIR)/server
COMMON_DIR=$(BASE_DIR)/common
HTTP_DIR=$(BASE_DIR)/http
#LIBS_DIR=$(BASE_DIR)/libs
LIBS_DIR?=.
PROGS_DIR=$(BASE_DIR)/qclib
SNDCODEC_DIR=$(BASE_DIR)/sndcodec

RELEASE_DIR=$(BASE_DIR)/release
DEBUG_DIR=$(BASE_DIR)/debug

ALL_CFLAGS=$(HAVECONFIG) $(CFLAGS) $(BASE_CFLAGS) $(WCFLAGS)

DO_CC=$(CC) $(ALL_CFLAGS) -o $@ -c $<
ifeq ($(USEASM),true)
	ifdef windir
		DO_AS=$(CC) $(BASE_CFLAGS) $(WCFLAGS) -x assembler-with-cpp -o $@ -c $< $(CFLAGS)
	else
		DO_AS=$(CC) $(BASE_CFLAGS) $(WCFLAGS) -x assembler-with-cpp -DELF -o $@ -c $< $(CFLAGS)
	endif
endif

ifeq ($(FTE_TARGET),win32)
	BASELDFLAGS=-lm
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

ifeq ($(FTE_TARGET),win32)
	IMAGELDFLAGS=$(LIBS_DIR)/mingw-libs/libpng.a $(LIBS_DIR)/mingw-libs/libz.a $(LIBS_DIR)/mingw-libs/libjpeg.a
else
	IMAGELDFLAGS=-lpng -ljpeg
endif

#BASELDFLAGS=-lm  -lz
GLXLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext $(IMAGELDFLAGS)
GLSLDFLAGS=-L/usr/X11R6/lib -lMesaGL -lglide -lvga
XLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext $(IMAGELDFLAGS)
SLDFLAGS=-lvga

ifeq ($(USEASM),true)
	COMMON_ASM_OBJS = 	\
		worlda.o 		\
		math.o

	CLIENT_ASM_OBJS = 	\
		snd_mixa.o

	SOFTWARE_ASM_OBJS =	\
		r_aclipa.o	\
		r_aliasa.o	\
		r_drawa.o	\
		r_edgea.o	\
		r_varsa.o	\
		d_draw.o	\
		d_draw16.o	\
		d_parta.o	\
		d_polysa.o	\
		d_scana.o	\
		d_spr8.o	\
		d_varsa.o	\
		surf16.o	\
		surf8.o

else
	BASE_ASM_CFLAGS = -DNOASM
endif

BASE_CFLAGS=$(BASE_ASM_CFLAGS) -Wall -Dstrnicmp=strncasecmp -Dstricmp=strcasecmp -I$(CLIENT_DIR) -I$(SERVER_DIR) -I$(COMMON_DIR) -I$(SW_DIR) -I$(GL_DIR) -I$(D3D9_DIR) -I$(D3D7_DIR) -I$(PROGS_DIR) -I$(LIBS_DIR) -I$(LIBS_DIR)/dxsdk7/include -I$(LIBS_DIR)/sdl/include -I$(LIBS_DIR)/sdl/include/SDL -D_vsnprintf=vsnprintf -D_snprintf=snprintf
CLIENT_ONLY_CFLAGS=-DCLIENTONLY
SERVER_ONLY_CFLAGS=-DSERVERONLY
JOINT_CFLAGS=
DEBUG_CFLAGS=-ggdb -g

ifeq ($(FTE_TARGET),win32)
	RELEASE_CFLAGS=-fno-strict-aliasing -ffast-math -fexpensive-optimizations $(CPUOPTIMIZATIONS)
else
	RELEASE_CFLAGS=-O2 -fno-strict-aliasing $(CPUOPTIMIZATIONS)
	#RELEASE_CFLAGS=-O6 -fno-strict-aliasing -ffast-math -funroll-loops -fexpensive-optimizations $(CPUOPTIMIZATIONS)
endif

GLCFLAGS=-DGLQUAKE
SWCFLAGS=-DSWQUAKE
D3DCFLAGS=-DD3DQUAKE
NPQTVCFLAGS=-DNPQTV

CLIENT_OBJS = $(CLIENT_ASM_OBJS) \
	textedit.o	\
	fragstats.o	\
	teamplay.o	\
	zqtp.o	\
	cl_demo.o	\
	cl_ents.o	\
	clq2_ents.o	\
	cl_input.o	\
	cl_main.o	\
	cl_parse.o	\
	cl_pred.o	\
	cl_tent.o	\
	cl_cam.o	\
	cl_screen.o	\
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
	r_bulleten.o	\
	r_part.o	\
	p_script.o	\
	p_null.o	\
	p_classic.o	\
	r_partset.o	\
	r_efrag.o	\
	renderer.o	\
	renderque.o	\
	sbar.o	\
	skin.o	\
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


SOFTWARE_OBJS = $(SOFTWARE_ASM_OBJS) \
	sw_screen.o	\
	sw_draw.o	\
	sw_model.o	\
	r_aclip.o	\
	r_alias.o	\
	r_bsp.o	\
	r_draw.o	\
	r_edge.o	\
	r_light.o	\
	r_main.o	\
	r_misc.o	\
	r_sky.o	\
	r_sprite.o	\
	r_surf.o	\
	r_vars.o	\
	d_edge.o	\
	d_fill.o	\
	d_init.o	\
	d_modech.o	\
	d_part.o	\
	d_polyse.o	\
	d_scan.o	\
	d_sky.o	\
	d_sprite.o	\
	d_surf.o	\
	d_trans.o	\
	d_vars.o	\
	d_zpoint.o	\
	nonintel.o

GLQUAKE_OBJS =		\
	gl_alias.o		\
	gl_draw.o		\
	gl_model.o		\
	gl_ngraph.o		\
	gl_rlight.o		\
	gl_rmain.o		\
	gl_rmisc.o		\
	gl_heightmap.o		\
	gl_rsurf.o		\
	ltface.o		\
	gl_screen.o		\
	gl_bloom.o		\
	gl_backend.o	\
	gl_shader.o		\
	gl_warp.o		\
	gl_ppl.o		\
	gl_vidcommon.o	\
	gl_hlmdl.o

D3DQUAKE_OBJS =		\
	d3d_draw.o	\
	d3d_mesh.o	\
	d3d_rmain.o	\
	d3d_rsurf.o	\
	vid_d3d.o	\
	d3d9_draw.o	\
	d3d9_mesh.o	\
	d3d9_rmain.o	\
	d3d9_rsurf.o	\
	vid_d3d9.o

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
	pr_edict.o		\
	pr_exec.o		\
	pr_multi.o		\
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
	sv_move.o 		\
	sv_phys.o 		\
	sv_user.o		\
	sv_mvd.o		\
	sv_ccmds.o 		\
	sv_rankin.o 	\
	sv_chat.o 		\
	sv_demo.o		\
	world.o 		\
	net_preparse.o 	\
	savegame.o		\
	svq2_ents.o 	\
	svq2_game.o 	\
	svq3_game.o	\
	snd_vc.o		\
	snd_voicecodecs.o	\
	g711.o		\
	g721.o		\
	g723_24.o		\
	g723_40.o		\
	g72x.o		\
	webgen.o		\
	ftpserver.o		\
	httpserver.o

SERVERONLY_OBJS =		\
	gl_alias.o		\
	svmodel.o		\
	sv_sys_unix.o

WINDOWSSERVERONLY_OBJS = \
	gl_alias.o		\
	svmodel.o	\
	sv_sys_win.o

COMMON_OBJS = $(COMMON_ASM_OBJS) \
	com_mesh.o	\
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
	log.o 		\
	net_chan.o 		\
	net_wins.o 		\
	zone.o 		\
	qvm.o	\
	gl_q2bsp.o		\
	glmod_doom.o 	\
	q3common.o	\
	pmove.o		\
	pmovetst.o		\
	iwebiface.o		\
	translate.o		\
	plugin.o		\
	q1bsp.o		\
	q2pmove.o


#the defaults for sdl come first
GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o
GL_EXE_NAME=../fteqw_sdl.gl$(BITS)
GLCL_EXE_NAME=../fteqwcl_sdl.gl$(BITS)
ifdef windir
	GL_LDFLAGS=$(GLLDFLAGS) -lmingw32 -lwsock32 `sdl-config --libs`
else
	GL_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) `sdl-config --libs`
endif
GL_CFLAGS=$(GLCFLAGS) `sdl-config --cflags`
GLB_DIR=gl_sdl$(BITS)
GLCL_DIR=glcl_sdl$(BITS)

SWCL_OBJS=$(SOFTWARE_OBJS) vid_sdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o
SW_EXE_NAME=../fteqw_sdl.sw$(BITS)
SWCL_EXE_NAME=../fteqwcl_sdl.sw$(BITS)
ifdef windir
SW_LDFLAGS=$(SWLDFLAGS) -lmingw32 -lwsock32 -lSDLmain -lSDL
else
#pthread is needed because of SDL.
SW_LDFLAGS=$(SWLDFLAGS) `sdl-config --libs` $(IMAGELDFLAGS)
endif
SW_CFLAGS=$(SWCFLAGS) `sdl-config --cflags`
SWB_DIR=sw_sdl$(BITS)
SWCL_DIR=swcl_sdl$(BITS)

SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS)
SV_EXE_NAME=../fteqw.sv$(BITS)
SV_CFLAGS=$(SERVER_ONLY_CFLAGS)

#specific targets override those defaults as needed.

ifeq ($(FTE_TARGET),win32)
	# The extra object file called resources.o is specific for MinGW to link the icon in

	#cygwin's gcc requires an extra command to use mingw instead of cygwin (default paths, etc).
	ifneq ($(shell $(CC) -v 2>&1 | grep cygwin),)
		W32_CFLAGS=-mno-cygwin
	endif

	SV_CFLAGS=$(SERVER_ONLY_CFLAGS) $(W32_CFLAGS)


	LIBS_DIR = $(BASE_DIR)/libs
	SV_EXE_NAME=../fteqwsv.exe
	SV_LDFLAGS=libs/zlib.lib -lwsock32 -lwinmm
	SV_DIR=sv_mingw
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(WINDOWSSERVERONLY_OBJS) resources.o


ifeq ($(USEASM),true)
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_dosa.o resources.o
else
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o resources.o
endif
	GL_EXE_NAME=../fteglqw.exe
	GLCL_EXE_NAME=../fteglqwcl.exe
	GL_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) -ldxguid -lwsock32 -lwinmm -lgdi32 -lole32
	GL_CFLAGS=$(GLCFLAGS) $(W32_CFLAGS)
	GLB_DIR=gl_mgw
	GLCL_DIR=glcl_mgw

ifeq ($(USEASM),true)
	NPQTVCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_dosa.o npplug.o sys_npqtv.o ../../ftequake/npapi.def
else
	NPQTVCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o npplug.o sys_npqtv.o ../../ftequake/npapi.def
endif
	NPQTV_DLL_NAME=../npqtv.dll
	NPQTVCL_DLL_NAME=../npqtvcl.dll
	NPQTV_LDFLAGS=--enable-stdcall-fixup $(GLLDFLAGS) $(IMAGELDFLAGS) -ldxguid -lwsock32 -lwinmm -lgdi32 -lole32 -shared
	NPQTV_CFLAGS=$(NPQTVCFLAGS) $(GLCFLAGS) $(W32_CFLAGS)
	NPQTVB_DIR=npqtv_mgw
	NPQTVCL_DIR=npqtvcl_mgw

ifeq ($(USEASM),true)
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_ddraw.o vid_dib.o vid_win2.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_dosa.o resources.o
else
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_ddraw.o vid_dib.o vid_win2.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o resources.o
endif
	SW_EXE_NAME=../fteswqw.exe
	SWCL_EXE_NAME=../fteswqwcl.exe
	SW_LDFLAGS=$(SWLDFLAGS) $(IMAGELDFLAGS) -ldxguid -lwsock32 -lwinmm -lgdi32 -lole32
	SW_CFLAGS=$(SWCFLAGS) $(W32_CFLAGS)
	SWB_DIR=sw_mgw
	SWCL_DIR=swcl_mgw

ifeq ($(USEASM),true)
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidnt.o vid_ddraw.o vid_dib.o vid_win2.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_dosa.o resources.o
else
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidnt.o vid_ddraw.o vid_dib.o vid_win2.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o resources.o
endif
	M_EXE_NAME=../fteqw.exe
	MCL_EXE_NAME=../fteqwcl.exe
	M_LDFLAGS=$(GLLDFLAGS) $(IMAGELDFLAGS) -ldxguid -lwsock32 -lwinmm -lgdi32 -lole32
	M_CFLAGS=$(SWCFLAGS) $(GLCFLAGS) $(W32_CFLAGS)
	MB_DIR=m_mgw
	MCL_DIR=mcl_mgw

	D3DCL_OBJS=$(D3DQUAKE_OBJS) snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o resources.o
	D3D_EXE_NAME=../fted3dqw.exe
	D3DCL_EXE_NAME=../fted3dclqw.exe
	D3D_LDFLAGS= -ldxguid -lwsock32 -lwinmm -lgdi32 -lole32
	D3D_CFLAGS=$(D3DCFLAGS) $(W32_CFLAGS) -DMINIMAL
	D3DB_DIR=d3d_mgw
	D3DCL_DIR=d3dcl_mgw

	MINGL_EXE_NAME=../fteminglqw.exe
	MINGL_DIR=mingl_mgw
endif
ifeq ($(FTE_TARGET),bsd)
	#mostly uses the linux stuff.
	#oss, X, etc.

	SV_DIR=sv_linux
	SV_LDFLAGS=-lz -ldl

ifeq ($(USEASM),true)
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o
endif
	GL_EXE_NAME=../fteqw.gl
	GLCL_EXE_NAME=../fteqwcl.gl
	GL_LDFLAGS= -L/usr/local/lib $(GLLDFLAGS) $(GLXLDFLAGS) -lXxf86vm -lpthread
	GL_CFLAGS=$(GLCFLAGS) -I/usr/local/include -I/usr/X11R6/include
	GLB_DIR=gl_bsd
	GLCL_DIR=glcl_bsd

ifeq ($(USEASM),true)
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o cd_null.o sys_linux.o
endif
	SW_EXE_NAME=../fteqw.sw
	SWCL_EXE_NAME=../fteqwcl.sw
	SW_LDFLAGS=-L/usr/local/lib $(SWLDFLAGS) $(XLDFLAGS) -lXxf86vm -lpthread
	SW_CFLAGS=$(SWCFLAGS) -I/usr/local/include -I/usr/X11R6/include
	SWB_DIR=sw_bsd
	SWCL_DIR=swcl_bsd

ifeq ($(USEASM),true)
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o cd_null.o sys_linux.o
endif
	M_EXE_NAME=../fteqw
	MCL_EXE_NAME=../fteqwcl
	M_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS) -lXxf86vm -lpthread
	M_CFLAGS=$(SWCFLAGS) $(GLCFLAGS) -I/usr/X11R6/include
	MB_DIR=m_bsd
	MCL_DIR=mcl_bsd

	MINGL_EXE_NAME=../fteqw.mingl
	MINGL_DIR=mingl_bsd
endif
ifneq ($(shell echo $(FTE_TARGET)|grep linux),)

	SV_DIR=sv_linux$(BITS)
	SV_LDFLAGS=-lz

	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o snd_alsa.oo cd_linux.o sys_linux.o
ifeq ($(USEASM),true)
	GLCL_OBJS+= sys_dosa.o
endif
	GL_EXE_NAME=../fteqw.gl$(BITS)
	GLCL_EXE_NAME=../fteqwcl.gl$(BITS)
	GL_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS)  -lXxf86vm -lXxf86dga
	GL_CFLAGS=$(GLCFLAGS) -I/usr/X11R6/include -DUSE_DGA
	GLB_DIR=gl_linux$(BITS)
	GLCL_DIR=glcl_linux$(BITS)

	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o snd_alsa.oo cd_linux.o sys_linux.o
ifeq ($(USEASM),true)
	SWCL_OBJS+= sys_dosa.o
endif
	SW_EXE_NAME=../fteqw.sw$(BITS)
	SWCL_EXE_NAME=../fteqwcl.sw$(BITS)
	SW_LDFLAGS=$(SWLDFLAGS) $(XLDFLAGS) -lXxf86vm -lXxf86dga
	SW_CFLAGS=$(SWCFLAGS) -I/usr/X11R6/include
	SWB_DIR=sw_linux$(BITS)
	SWCL_DIR=swcl_linux$(BITS)

	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o snd_alsa.oo cd_linux.o sys_linux.o
ifeq ($(USEASM),true)
	MCL_OBJS+= sys_dosa.o
endif
	M_EXE_NAME=../fteqw$(BITS)
	MCL_EXE_NAME=../fteqwcl$(BITS)
	M_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS) -lXxf86vm -lXxf86dga
	M_CFLAGS=$(SWCFLAGS) $(GLCFLAGS) -I/usr/X11R6/include
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
	SWB_DIR=sw_macosx$(EXTENSION)$(BITS)
	SWCL_DIR=sw_macosx$(EXTENSION)$(BITS)

	GL_CFLAGS=$(GLCFLAGS) -D__MACOSX__
	GL_LDFLAGS=-framework AGL -framework OpenGL -framework Cocoa -framework AudioUnit
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidcocoa.mo gl_vidmacos.o sys_linux.o in_macos.o cd_null.o snd_macos.o

	SW_CFLAGS=$(SWCFLAGS) -D__MACOSX__
	SW_LDFLAGS=
	SWCL_OBJS=$(SOFTWARE_OBJS) cd_null.o sys_linux.o vid_null.o

	GL_EXE_NAME=../macosx_fteqw.gl$(EXTENSION)$(BITS)
	GLCL_EXE_NAME=../macosx_fteqwcl.gl$(EXTENSION)$(BITS)
	SW_EXE_NAME=../macosx_fteqw.sw$(EXTENSION)$(BITS)
	SWCL_EXE_NAME=../macosx_fteqwcl.sw$(EXTENSION)$(BITS)
	M_EXE_NAME=../macosx_fteqw$(EXTENSION)$(BITS)
	MCL_EXE_NAME=../macosx_fteqwcl$(EXTENSION)$(BITS)
	MINGL_EXE_NAME=../macosx_fteqw.mingl$(EXTENSION)$(BITS)
	MINGL_DIR=mingl_macosx$(EXTENSION)$(BITS)
	
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS)
	SV_EXE_NAME=../macosx_fteqw.sv$(EXTENSION)$(BITS)
	SV_CFLAGS=$(SERVER_ONLY_CFLAGS)	

	#seems, macosx has a more limited version of strip
	STRIPFLAGS=
endif
ifeq ($(FTE_TARGET),morphos)

	SV_DIR=sv_morphos
	SV_LDFLAGS=-lz

	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidmorphos.o in_morphos.o snd_morphos.o cd_null.o sys_morphos.o
	GL_EXE_NAME=../morphos_fteqw.gl
	GLCL_EXE_NAME=../morphos_fteqwcl.gl
	GL_LDFLAGS=$(GLLDFLAGS) -ldl $(IMAGELDFLAGS) -lz
	GL_CFLAGS=$(GLCFLAGS) -noixemul
	GLB_DIR=gl_morphos
	GLCL_DIR=glcl_morphos

	SWCL_OBJS=$(SOFTWARE_OBJS) vid_morphos.o in_morphos.o snd_morphos.o cd_null.o sys_morphos.o
	SW_EXE_NAME=../morphos_fteqw.sw
	SWCL_EXE_NAME=../morphos_fteqwcl.sw
	SW_LDFLAGS=$(SWLDFLAGS) -ldl $(IMAGELDFLAGS) -lz
	SW_CFLAGS=$(SWCFLAGS) -noixemul
	SWB_DIR=sw_morphos
	SWCL_DIR=swcl_morphos

	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidmorphos.o vid_morphos.o in_morphos.o snd_morphos.o cd_null.o sys_morphos.o
	M_EXE_NAME=../morphos_fteqw
	MCL_EXE_NAME=../morphos_fteqwcl
	M_LDFLAGS=$(GLLDFLAGS)
	M_CFLAGS=$(SWCFLAGS) $(GLCFLAGS)
	MB_DIR=m_morphos
	MCL_DIR=mcl_morphos

	MINGL_EXE_NAME=../morphos_fteqw.mingl
	MINGL_DIR=mingl_morphos
	
	#morphos dedicated server doesnt compile: alloca.h: No such file or directory
	#SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS)
	#SV_EXE_NAME=../morphos_fteqw.sv$(BITS)
	#SV_CFLAGS=$(SERVER_ONLY_CFLAGS)	
endif


ifeq ($(FTE_TARGET),cygwin)

	SV_DIR=sv_cygwin
	SV_LDFLAGS=-lz

ifeq ($(USEASM),true)
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o
endif
	GL_EXE_NAME=../fteqwglcyg.exe
	GLCL_EXE_NAME=../fteqwclglcyg.exe
	GL_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS)
	GL_CFLAGS=$(GLCFLAGS) -I/usr/X11R6/include
	GLB_DIR=gl_cygwin
	GLCL_DIR=glcl_cygwin

ifeq ($(USEASM),true)
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o cd_null.o sys_linux.o
endif
	SW_EXE_NAME=../fteqwswcyg.exe
	SWCL_EXE_NAME=../fteqwclswcyg.exe
	SW_LDFLAGS=$(SWLDFLAGS) $(XLDFLAGS)
	SW_CFLAGS=$(SWCFLAGS)
	SWB_DIR=sw_cygwin
	SWCL_DIR=swcl_cygwin

ifeq ($(USEASM),true)
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o cd_null.o sys_linux.o
endif
	M_EXE_NAME=../fteqwcyg.exe
	MCL_EXE_NAME=../fteqwclcyg.exe
	M_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS)
	M_CFLAGS=$(SWCFLAGS) $(GLCFLAGS)
	MB_DIR=m_cygwin
	MCL_DIR=mcl_cygwin



	MINGL_EXE_NAME=../fteqwminglcyg.exe
	MINGL_DIR=mingl_cygwin
endif

SV_DIR?=sv_sdl

.default: help
all: rel
rel: sv-rel sw-rel gl-rel m-rel mingl-rel
dbg: sv-dbg sw-dbg gl-dbg m-dbg mingl-dbg
relcl: swcl-rel glcl-rel mcl-rel

releases:
	#this is for releasing things from a linux box
	#just go through compiling absolutly everything
	-$(MAKE) FTE_TARGET=linux32 rel
	-$(MAKE) FTE_TARGET=linux64 rel
	-$(MAKE) FTE_TARGET=win32 rel
#	-$(MAKE) FTE_TARGET=linux32 relcl
#	-$(MAKE) FTE_TARGET=linux64 relcl
#	-$(MAKE) FTE_TARGET=win32 relcl

autoconfig: clean
	/bin/bash makeconfig.sh y

config: clean
	/bin/bash makeconfig.sh

ifneq ($(OUT_DIR),)
	-include $(OUT_DIR)/*.d
endif


VPATH = $(BASE_DIR) : $(CLIENT_DIR) : $(GL_DIR) : $(COMMON_DIR) : $(SW_DIR) : $(SERVER_DIR) : $(HTTP_DIR) : $(BASE_DIR)/irc : $(BASE_DIR)/email : $(QUX_DIR) : $(PROGS_DIR) : $(SNDCODEC_DIR) : $(D3D7_DIR) : $(D3D9_DIR)

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
	@set -e; rm -f $@.d; \
	 $(CC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.o[ :]*,\1.o $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 rm -f $@.d.$$$$
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.oo $(OUT_DIR)/%.d : %.c
	@set -e; rm -f $@.d; \
	 $(CC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.oo[ :]*,\1.oo $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 rm -f $@.d.$$$$
	$(DO_CC) -I$(OUT_DIR)

$(OUT_DIR)/%.mo $(OUT_DIR)/%.d : %.m
	@set -e; rm -f $@.d; \
	 $(CC) -MM $(ALL_CFLAGS) $< > $@.d.$$$$; \
	 sed 's,\($*\)\.mo[ :]*,\1.mo $@.d : ,g' < $@.d.$$$$ > $@.d; \
	 rm -f $@.d.$$$$
	$(DO_CC) -I$(OUT_DIR)

ifeq ($(USEASM),true)
$(OUT_DIR)/%.o : %.s sw/*.h
	$(DO_AS)
endif

#enables use of precompiled headers in gcc 3.4 onwards.
$(OUT_DIR)/quakedef.h.gch : quakedef.h
	$(CC) -x c $(BASE_CFLAGS) $(WCFLAGS) -o $@ -c $< $(CFLAGS)

#addprefix is to add the ./release/server/ part of the object name
#foreach is needed as the OBJS is a list of variable names containing object lists.
#which is needed as windows sucks too much for the chaining to carry a full list.
#god knows how gcc loads the list properly.
#or at least I hope he does. It makes no sence to mortals.

$(OUT_DIR)/$(EXE_NAME):   $(OUT_DIR)/quakedef.h.gch    $(addprefix $(OUT_DIR)/, $(CUSTOMOBJS) $(foreach ol, $(OBJS), $($(ol))))
	$(CC) $(WCFLAGS) -o $@ $(addprefix $(OUT_DIR)/, $(CUSTOMOBJS) $(foreach ol, $(OBJS), $($(ol)))) $(LDFLAGS) $(CFLAGS)

_out-rel:
	$(MAKE) $(OUT_DIR)/$(EXE_NAME) EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(WCFLAGS) $(RELEASE_CFLAGS)" LDFLAGS="$(BASELDFLAGS) $(LDFLAGS)" OBJS="$(OBJS)"
	$(STRIP) $(STRIPFLAGS) $(OUT_DIR)/$(EXE_NAME)

_out-dbg:
	$(MAKE) $(OUT_DIR)/$(EXE_NAME) EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(WCFLAGS) $(DEBUG_CFLAGS)" LDFLAGS="$(BASELDFLAGS) $(LDFLAGS)" OBJS="$(OBJS)"

_cl-rel: reldir
	$(MAKE) _out-rel EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(CLIENT_ONLY_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS"

_cl-dbg: debugdir
	$(MAKE) _out-dbg EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(CLIENT_ONLY_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS"

_clsv-rel: reldir
	$(MAKE) _out-rel EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(JOINT_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS SERVER_OBJS"

_clsv-dbg: debugdir
	$(MAKE) _out-dbg EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(JOINT_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS SERVER_OBJS"


sv-tmp: reldir debugdir
	$(MAKE) $(TYPE) OUT_DIR="$(OUT_DIR)" EXE_NAME="$(SV_EXE_NAME)" WCFLAGS="$(SV_CFLAGS)" LDFLAGS="$(SV_LDFLAGS) $(LDFLAGS)" OBJS="SV_OBJS"
sv-rel:
	$(MAKE) sv-tmp TYPE=_out-rel OUT_DIR="$(RELEASE_DIR)/$(SV_DIR)"
sv-dbg:
	$(MAKE) sv-tmp TYPE=_out-dbg OUT_DIR="$(DEBUG_DIR)/$(SV_DIR)"



d3dcl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(D3DCL_EXE_NAME)" WCFLAGS="$(D3D_CFLAGS)" LDFLAGS="$(D3D_LDFLAGS) $(LDFLAGS)" SOBJS="$(D3DCL_OBJS)"
d3d-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(D3D_EXE_NAME)" WCFLAGS="$(D3D_CFLAGS)" LDFLAGS="$(D3D_LDFLAGS) $(LDFLAGS)" SOBJS="$(D3DCL_OBJS)"

d3dcl-rel:
	$(MAKE) d3dcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(D3DCL_DIR)"
d3dcl-dbg:
	$(MAKE) d3dcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(D3DCL_DIR)"
d3d-rel:
	$(MAKE) d3d-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(D3DB_DIR)"
d3d-dbg:
	$(MAKE) d3d-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(D3DB_DIR)"


npqtvcl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(NPQTVCL_DLL_NAME)" WCFLAGS="$(NPQTV_CFLAGS)" LDFLAGS="$(NPQTV_LDFLAGS) $(LDFLAGS)" SOBJS="$(NPQTVCL_OBJS)"
npqtv-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(NPQTV_DLL_NAME)" WCFLAGS="$(NPQTV_CFLAGS)" LDFLAGS="$(NPQTV_LDFLAGS) $(LDFLAGS)" SOBJS="$(NPQTVCL_OBJS)"

npqtvcl-rel:
	$(MAKE) npqtvcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(NPQTVCL_DIR)"
npqtvcl-dbg:
	$(MAKE) npqtvcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(NPQTVCL_DIR)"
npqtv-rel:
	$(MAKE) npqtv-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(NPQTVB_DIR)"
npqtv-dbg:
	$(MAKE) npqtv-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(NPQTVB_DIR)"


glcl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(GLCL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS)" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"
gl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(GL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS)" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"

glcl-rel:
	$(MAKE) glcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(GLCL_DIR)"
glcl-dbg:
	$(MAKE) glcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(GLCL_DIR)"
gl-rel:
	$(MAKE) gl-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(GLB_DIR)"
gl-dbg:
	$(MAKE) gl-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(GLB_DIR)"


mingl-tmp: reldir
	$(MAKE) $(TYPE) OUT_DIR="$(OUT_DIR)" EXE_NAME="$(MINGL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS) -DMINIMAL" LDFLAGS="$(GL_LDFLAGS) $(LDFLAGS)" SOBJS="$(GLCL_OBJS)"
mingl-rel:
	$(MAKE) mingl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(MINGL_DIR)"


swcl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(SWCL_EXE_NAME)" WCFLAGS="$(SW_CFLAGS)" LDFLAGS="$(SW_LDFLAGS) $(LDFLAGS)" SOBJS="$(SWCL_OBJS)"
sw-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(SW_EXE_NAME)" WCFLAGS="$(SW_CFLAGS)" LDFLAGS="$(SW_LDFLAGS) $(LDFLAGS)" SOBJS="$(SWCL_OBJS)"

swcl-rel:
	$(MAKE) swcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(SWCL_DIR)"
swcl-dbg:
	$(MAKE) swcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(SWCL_DIR)"
sw-rel:
	$(MAKE) sw-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(SWB_DIR)"
sw-dbg:
	$(MAKE) sw-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(SWB_DIR)"

mcl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(MCL_EXE_NAME)" WCFLAGS="$(M_CFLAGS)" LDFLAGS="$(M_LDFLAGS) $(LDFLAGS)" SOBJS="$(MCL_OBJS)"
m-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(M_EXE_NAME)" WCFLAGS="$(M_CFLAGS)" LDFLAGS="$(M_LDFLAGS) $(LDFLAGS)" SOBJS="$(MCL_OBJS)"

mcl-rel:
	$(MAKE) mcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(MCL_DIR)"
mcl-dbg:
	$(MAKE) mcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(MCL_DIR)"
m-rel:
	$(MAKE) m-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(MB_DIR)"
m-dbg:
	$(MAKE) m-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(MB_DIR)"

.PHONY: m-tmp mcl-tmp sw-tmp swcl-tmp mingl-tmp glcl-tmp gl-tmp sv-tmp _clsv-dbg _clsv-rel _cl-dbg _cl-rel _out-rel _out-dbg

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
	@-echo ""
	@-echo "Normal targets:"
	@-echo "(each of these targets must have the postfix -rel or -dbg)"
	@-echo "sv"
	@-echo "gl"
	@-echo "sw"
	@-echo "m"
	@-echo "mingl"
	@-echo "npqtv (FTE_TARGET=win32 only, for now)"
	@-echo "d3d (for windows builds)"
	@-echo "mcl (currently broken)"
	@-echo "glcl (currently broken)"
	@-echo "swcl (currently broken)"

install:
	-cp debug/*.* /opt/quake/
	-cp release/*.* /opt/quake/

clean:
	-rm -f -r $(RELEASE_DIR)
	-rm -f -r $(DEBUG_DIR)

