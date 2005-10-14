CC=gcc

CPUOPTIMIZATIONS=

BASE_DIR=.

ifeq ($(FTE_TARGET),) 	#user didn't specify prefered target
	ifneq ($(shell gcc -v 2>&1 | grep mingw),)
		FTE_TARGET=win32
	else
		ifeq ($(shell uname),Linux)
			FTE_TARGET=linux
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
		#else I've no idea what it is you're running
	endif

	FTE_TARGET ?= sdl	#so go for sdl.
endif


CLIENT_DIR=$(BASE_DIR)/client
GL_DIR=$(BASE_DIR)/gl
SW_DIR=$(BASE_DIR)/sw
SERVER_DIR=$(BASE_DIR)/server
COMMON_DIR=$(BASE_DIR)/common
HTTP_DIR=$(BASE_DIR)/http
#LIBS_DIR=$(BASE_DIR)/libs
LIBS_DIR?=.
PROGS_DIR=$(BASE_DIR)/qclib
SNDCODEC_DIR=$(BASE_DIR)/sndcodec
MP3_DIR=$(BASE_DIR)/../mp3/libmad
QUX_DIR=$(BASE_DIR)/qux

RELEASE_DIR=$(BASE_DIR)/release
DEBUG_DIR=$(BASE_DIR)/debug

DO_CC=$(CC) $(CFLAGS) $(BASE_CFLAGS) $(WCFLAGS) -o $@ -c $<
ifeq ($(USEASM),true)
	ifdef windir
		DO_AS=$(CC) $(BASE_CFLAGS) $(WCFLAGS) -x assembler-with-cpp -o $@ -c $< $(CFLAGS)
	else
		DO_AS=$(CC) $(BASE_CFLAGS) $(WCFLAGS) -x assembler-with-cpp -DELF -o $@ -c $< $(CFLAGS)
	endif
endif
BASELDFLAGS=-lm
#BASELDFLAGS=-lm  -lz
GLXLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lpng -ljpeg -lXxf86vm
GLSLDFLAGS=-L/usr/X11R6/lib -lMesaGL -lglide -lvga
XLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lpng -ljpeg
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

BASE_CFLAGS=$(BASE_ASM_CFLAGS) -Wall -Dstrnicmp=strncasecmp -Dstricmp=strcasecmp -I$(CLIENT_DIR) -I$(SERVER_DIR) -I$(COMMON_DIR) -I$(SW_DIR) -I$(GL_DIR) -I$(PROGS_DIR) -I$(LIBS_DIR) -I$(QUX_DIR) -I$(LIBS_DIR)/dxsdk7/include -I/usr/X11R6/include -I$(LIBS_DIR)/sdl/include -I$(LIBS_DIR)/sdl/include/SDL -D_vsnprintf=vsnprintf -D_snprintf=snprintf
CLIENT_ONLY_CFLAGS=-DCLIENTONLY
SERVER_ONLY_CFLAGS=-DSERVERONLY
JOINT_CFLAGS=
DEBUG_CFLAGS=-ggdb -g
RELEASE_CFLAGS=-O6 -s -fno-strict-aliasing -ffast-math -funroll-loops -fexpensive-optimizations $(CPUOPTIMIZATIONS)
GLCFLAGS=-DGLQUAKE
SWCFLAGS=-DSWQUAKE

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
	pr_csqc.o	\
	qvm.o	\
	ircclient.o	\
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
	gl_backend.o	\
	gl_shader.o		\
	gl_warp.o		\
	gl_ppl.o		\
	gl_vidcommon.o	\
	gl_hlmdl.o

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
	svmodel.o	\
	sv_sys_win.o

COMMON_OBJS = $(COMMON_ASM_OBJS) \
	common.o 		\
	cvar.o 		\
	cmd.o 		\
	crc.o 		\
	fs.o			\
	mathlib.o 		\
	huff.o		\
	md4.o 		\
	log.o 		\
	net_chan.o 		\
	net_wins.o 		\
	zone.o 		\
	gl_q2bsp.o		\
	glmod_doom.o 	\
	pmove.o		\
	pmovetst.o		\
	iwebiface.o		\
	translate.o		\
	plugin.o		\
	q1bsp.o		\
	q2pmove.o


#the defaults for sdl come first
GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidsdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o
GL_EXE_NAME=../fteqw_sdl.gl
GLCL_EXE_NAME=../fteqwcl_sdl.gl
ifdef windir
	GL_LDFLAGS=$(GLLDFLAGS) -lmingw32 -lwsock32 `sdl-config --libs`
else
	GL_LDFLAGS=$(GLLDFLAGS) -lpng -ljpeg `sdl-config --libs`
endif
GL_CFLAGS=$(GLCFLAGS) `sdl-config --cflags`
GLB_DIR=gl_sdl
GLCL_DIR=glcl_sdl

SWCL_OBJS=$(SOFTWARE_OBJS) vid_sdl.o snd_sdl.o cd_sdl.o sys_sdl.o in_sdl.o
SW_EXE_NAME=../fteqw_sdl.sw
SWCL_EXE_NAME=../fteqwcl_sdl.sw
ifdef windir
SW_LDFLAGS=$(SWLDFLAGS) -lmingw32 -lwsock32 -lSDLmain -lSDL
else
#pthread is needed because of SDL.
SW_LDFLAGS=$(SWLDFLAGS) `sdl-config --libs` -lpng -ljpeg
endif
SW_CFLAGS=$(SWCFLAGS) `sdl-config --cflags`
SWB_DIR=sw_sdl
SWCL_DIR=swcl_sdl

SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(SERVERONLY_OBJS)
SV_EXE_NAME=../fteqw.sv
SV_CFLAGS=$(SERVER_ONLY_CFLAGS)

#specific targets override those defaults as needed.

ifeq ($(FTE_TARGET),win32)
	LIBS_DIR = $(BASE_DIR)/libs
	SV_EXE_NAME=../fteqwsv.exe
	SV_LDFLAGS=libs/zlib.lib -lwsock32
	SV_DIR=sv_mingw
	SV_OBJS=$(COMMON_OBJS) $(SERVER_OBJS) $(PROGS_OBJS) $(WINDOWSSERVERONLY_OBJS)

ifeq ($(USEASM),true)
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_dosa.o
else
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidnt.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o
endif
	GL_EXE_NAME=../fteglqw.exe
	GLCL_EXE_NAME=../fteglqwcl.exe
	GL_LDFLAGS=$(GLLDFLAGS) -ldxguid -lwsock32 -lwinmm -lgdi32 -lole32
	GL_CFLAGS=$(GLCFLAGS)
	GLB_DIR=gl_mgw
	GLCL_DIR=glcl_mgw

ifeq ($(USEASM),true)
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_ddraw.o vid_dib.o vid_win2.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_dosa.o
else
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_ddraw.o vid_dib.o vid_win2.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o
endif
	SW_EXE_NAME=../fteswqw.exe
	SWCL_EXE_NAME=../fteswqwcl.exe
	SW_LDFLAGS=$(SWLDFLAGS) -ldxguid -lwsock32 -lwinmm -lgdi32 -lole32
	SW_CFLAGS=$(SWCFLAGS)
	SWB_DIR=sw_mgw
	SWCL_DIR=swcl_mgw

ifeq ($(USEASM),true)
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidnt.o vid_ddraw.o vid_dib.o vid_win2.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o sys_dosa.o
else
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidnt.o vid_ddraw.o vid_dib.o vid_win2.o snd_win.o snd_directx.o cd_win.o in_win.o sys_win.o
endif
	M_EXE_NAME=../fteqw.exe
	MCL_EXE_NAME=../fteqwcl.exe
	M_LDFLAGS=$(GLLDFLAGS) -ldxguid -lwsock32 -lwinmm -lgdi32 -lole32
	M_CFLAGS=$(SWCFLAGS) $(GLCFLAGS)
	MB_DIR=m_mgw
	MCL_DIR=mcl_mgw

endif
ifeq ($(FTE_TARGET),bsd)
	#mostly uses the linux stuff.
	#oss, X, etc.

	SV_DIR=sv_linux
	SV_LDFLAGS=-lz

ifeq ($(USEASM),true)
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o cd_null.o sys_linux.o
endif
	GL_EXE_NAME=../fteqw.gl
	GLCL_EXE_NAME=../fteqwcl.gl
	GL_LDFLAGS= -L/usr/local/lib $(GLLDFLAGS) $(GLXLDFLAGS)
	GL_CFLAGS=$(GLCFLAGS) -I/usr/local/include
	GLB_DIR=gl_bsd
	GLCL_DIR=glcl_bsd

ifeq ($(USEASM),true)
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o cd_null.o sys_linux.o
endif
	SW_EXE_NAME=../fteqw.sw
	SWCL_EXE_NAME=../fteqwcl.sw
	SW_LDFLAGS=-L/usr/local/lib $(SWLDFLAGS) $(XLDFLAGS)
	SW_CFLAGS=$(SWCFLAGS) -I/usr/local/include
	SWB_DIR=sw_bsd
	SWCL_DIR=swcl_bsd

ifeq ($(USEASM),true)
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o cd_null.o sys_linux.o sys_dosa.o
else
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o cd_null.o sys_linux.o
endif
	M_EXE_NAME=../fteqw
	MCL_EXE_NAME=../fteqwcl
	M_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS)
	M_CFLAGS=$(SWCFLAGS) $(GLCFLAGS)
	MB_DIR=m_bsd
	MCL_DIR=mcl_bsd
endif
ifeq ($(FTE_TARGET),linux)

	SV_DIR=sv_linux
	SV_LDFLAGS=-lz

ifeq ($(USEASM),true)
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o cd_linux.o sys_linux.o sys_dosa.o
else
	GLCL_OBJS=$(GL_OBJS) $(GLQUAKE_OBJS) gl_vidlinuxglx.o snd_linux.o cd_linux.o sys_linux.o
endif
	GL_EXE_NAME=../fteqw.gl
	GLCL_EXE_NAME=../fteqwcl.gl
	GL_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS)
	GL_CFLAGS=$(GLCFLAGS)
	GLB_DIR=gl_linux
	GLCL_DIR=glcl_linux

ifeq ($(USEASM),true)
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o cd_linux.o sys_linux.o sys_dosa.o
else
	SWCL_OBJS=$(SOFTWARE_OBJS) vid_x.o snd_linux.o cd_linux.o sys_linux.o
endif
	SW_EXE_NAME=../fteqw.sw
	SWCL_EXE_NAME=../fteqwcl.sw
	SW_LDFLAGS=$(SWLDFLAGS) $(XLDFLAGS)
	SW_CFLAGS=$(SWCFLAGS)
	SWB_DIR=sw_linux
	SWCL_DIR=swcl_linux

ifeq ($(USEASM),true)
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o cd_linux.o sys_linux.o sys_dosa.o
else
	MCL_OBJS=$(GLQUAKE_OBJS) $(SOFTWARE_OBJS) gl_vidlinuxglx.o vid_x.o snd_linux.o cd_linux.o sys_linux.o
endif
	M_EXE_NAME=../fteqw
	MCL_EXE_NAME=../fteqwcl
	M_LDFLAGS=$(GLLDFLAGS) $(GLXLDFLAGS)
	M_CFLAGS=$(SWCFLAGS) $(GLCFLAGS)
	MB_DIR=m_linux
	MCL_DIR=mcl_linux



	MINGL_EXE_NAME=../fteqw.mingl
	MINGL_DIR=mingl_linux
endif

SV_DIR?=sv_sdl

.default: help
all: sv-rel sw-rel gl-rel m-rel plugins

VPATH = $(BASE_DIR) : $(CLIENT_DIR) : $(GL_DIR) : $(COMMON_DIR) : $(SW_DIR) : $(SERVER_DIR) : $(HTTP_DIR) : $(BASE_DIR)/irc : $(BASE_DIR)/email : $(QUX_DIR) : $(PROGS_DIR) : $(SNDCODEC_DIR)


$(OUT_DIR)/%.o : %.c
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
	strip --strip-all $(OUT_DIR)/$(EXE_NAME)

_out-dbg:
	$(MAKE) $(OUT_DIR)/$(EXE_NAME) EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(WCFLAGS) $(DEBUG_CFLAGS)" LDFLAGS="$(BASELDFLAGS) $(LDFLAGS)" OBJS="$(OBJS)"

_cl-rel:
	$(MAKE) _out-rel EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(CLIENT_ONLY_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS"

_cl-dbg:
	$(MAKE) _out-dbg EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(CLIENT_ONLY_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS"

_clsv-rel: reldir
	$(MAKE) _out-rel EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(JOINT_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS SERVER_OBJS"

_clsv-dbg: debugdir
	$(MAKE) _out-dbg EXE_NAME="$(EXE_NAME)" OUT_DIR="$(OUT_DIR)" WCFLAGS="$(JOINT_CFLAGS) $(WCFLAGS)" LDFLAGS="$(LDFLAGS)" SOBJS="$(SOBJS)" OBJS="SOBJS COMMON_OBJS CLIENT_OBJS PROGS_OBJS SERVER_OBJS"


sv-tmp: reldir debugdir
	$(MAKE) $(TYPE) OUT_DIR="$(OUT_DIR)" EXE_NAME="$(SV_EXE_NAME)" WCFLAGS="$(SV_CFLAGS)" LDFLAGS="$(SV_LDFLAGS)" OBJS="SV_OBJS"
sv-rel:
	$(MAKE) sv-tmp TYPE=_out-rel OUT_DIR="$(RELEASE_DIR)/$(SV_DIR)"
sv-dbg:
	$(MAKE) sv-tmp TYPE=_out-dbg OUT_DIR="$(DEBUG_DIR)/$(SV_DIR)"





glcl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(GLCL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS)" LDFLAGS="$(GL_LDFLAGS)" SOBJS="$(GLCL_OBJS)"
gl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(GL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS)" LDFLAGS="$(GL_LDFLAGS)" SOBJS="$(GLCL_OBJS)"

glcl-rel:
	$(MAKE) glcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(GLCL_DIR)"
glcl-dbg:
	$(MAKE) glcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(GLCL_DIR)"
gl-rel:
	$(MAKE) gl-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(GLB_DIR)"
gl-dbg:
	$(MAKE) gl-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(GLB_DIR)"


mingl-tmp: reldir
	$(MAKE) $(TYPE) OUT_DIR="$(OUT_DIR)" EXE_NAME="$(MINGL_EXE_NAME)" WCFLAGS="$(GL_CFLAGS) -DMINIMAL" LDFLAGS="$(GL_LDFLAGS)" SOBJS="$(GLCL_OBJS)"
mingl-rel:
	$(MAKE) mingl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(MINGL_DIR)"


swcl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(SWCL_EXE_NAME)" WCFLAGS="$(SW_CFLAGS)" LDFLAGS="$(SW_LDFLAGS)" SOBJS="$(SWCL_OBJS)"
sw-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(SW_EXE_NAME)" WCFLAGS="$(SW_CFLAGS)" LDFLAGS="$(SW_LDFLAGS)" SOBJS="$(SWCL_OBJS)"

swcl-rel:
	$(MAKE) swcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(SWCL_DIR)"
swcl-dbg:
	$(MAKE) swcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(SWCL_DIR)"
sw-rel:
	$(MAKE) sw-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(SWB_DIR)"
sw-dbg:
	$(MAKE) sw-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(SWB_DIR)"

mcl-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(MCL_EXE_NAME)" WCFLAGS="$(M_CFLAGS)" LDFLAGS="$(M_LDFLAGS)" SOBJS="$(MCL_OBJS)"
m-tmp:
	$(MAKE) $(TYPE)   OUT_DIR="$(OUT_DIR)" EXE_NAME="$(M_EXE_NAME)" WCFLAGS="$(M_CFLAGS)" LDFLAGS="$(M_LDFLAGS)" SOBJS="$(MCL_OBJS)"

mcl-rel:
	$(MAKE) mcl-tmp TYPE=_cl-rel OUT_DIR="$(RELEASE_DIR)/$(MCL_DIR)"
mcl-dbg:
	$(MAKE) mcl-tmp TYPE=_cl-dbg OUT_DIR="$(DEBUG_DIR)/$(MCL_DIR)"
m-rel:
	$(MAKE) m-tmp TYPE=_clsv-rel OUT_DIR="$(RELEASE_DIR)/$(MCL_DIR)"
m-dbg:
	$(MAKE) m-tmp TYPE=_clsv-dbg OUT_DIR="$(DEBUG_DIR)/$(MCL_DIR)"

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
	@-echo ""
	@-echo "Normal targets:"
	@-echo "(each of these targets must have the postfix -rel or -dbg)"
	@-echo "sv"
	@-echo "gl"
	@-echo "sw"

clean:
	-rm -f -r $(RELEASE_DIR)
	-rm -f -r $(DEBUG_DIR)

