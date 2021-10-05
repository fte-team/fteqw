#Note: this file was made primarily to support msvc and its project file incompatibilities nightmare.
#Its also useful for various other IDEs like QtCreator etc.
#It uses system libraries, so it will have dependancy issues with public releases where those dependancies are distro/version-specific.
#Public builds are still built using the (overcomplicated) traditional (g)makefile.

CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
cmake_policy(SET CMP0063 NEW)

PROJECT(FTEQuake)

INCLUDE_DIRECTORIES(
	engine/common
	engine/client
	engine/qclib
	engine/gl
	engine/server
	engine
)

IF (EXISTS ${CMAKE_SOURCE_DIR}/.svn)
	EXECUTE_PROCESS(COMMAND
		"svnversion"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		OUTPUT_VARIABLE FTE_REVISON
		ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	IF (NOT "${FTE_REVISON}" STREQUAL "")
		MESSAGE(STATUS "FTE SVN Revision ${FTE_REVISON}")
		IF(FTE_REVISON MATCHES "M")
			MESSAGE(STATUS "--- PRIVATE CHANGES DETECTED ---")
			SET(FTE_REVISON SVNREVISION=${FTE_REVISON})
		ELSE()
			MESSAGE(STATUS "No local changes")
			EXECUTE_PROCESS(COMMAND
				svn info --show-item last-changed-date --no-newline
				WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
				OUTPUT_VARIABLE FTE_DATE
			)

			SET(FTE_REVISON SVNREVISION=${FTE_REVISON} SVNDATE=${FTE_DATE})
		ENDIF()
	ENDIF()
ENDIF()
IF (EXISTS ${CMAKE_SOURCE_DIR}/.git)
	EXECUTE_PROCESS(COMMAND
		git describe --always --long --dirty
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		OUTPUT_VARIABLE FTE_REVISON_GIT
		ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	EXECUTE_PROCESS(COMMAND
		git log -1 --format=%cs
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		OUTPUT_VARIABLE FTE_DATE
		ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	EXECUTE_PROCESS(COMMAND
		git branch --show-current
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		OUTPUT_VARIABLE FTE_BRANCH
		ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	MESSAGE(STATUS "FTE GIT ${FTE_BRANCH} Revision git-${FTE_REVISON_GIT}, ${FTE_DATE}")
	SET(FTE_REVISON SVNREVISION=git-${FTE_REVISON_GIT} SVNDATE=${FTE_DATE} FTE_BRANCH=${FTE_BRANCH})
ENDIF()

#plugins need visibility hidden in order to avoid conflicts with function names that match the engine.
#this is consistent with how windows works so no great loss.
#plus it means that gcc can inline more (with LTO), including optimising args.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

IF(${CMAKE_VERSION} VERSION_LESS "3.9.0")
	MESSAGE(STATUS "no LTO - old cmake.")
ELSE()
	cmake_policy(SET CMP0069 NEW)
	IF(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
		#use LTO where possible. reportedly requires cmake 3.9 to actually work
		INCLUDE(CheckIPOSupported)
		check_ipo_supported(RESULT result)
		IF(result)
			SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
			MESSAGE(STATUS "Using LTO.")
		ELSE()
			MESSAGE(STATUS "no LTO - not supported.")
		ENDIF()
	ELSE()
		MESSAGE(STATUS "no LTO - debug.")
	ENDIF()
ENDIF()

SET(FTE_BUILD_CONFIG ${CMAKE_HOME_DIRECTORY}/engine/common/config_fteqw.h CACHE FILEPATH "Which build config file to use to control supported features.")
SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};CONFIG_FILE_NAME=${FTE_BUILD_CONFIG})
SET(FTE_USE_SDL false CACHE BOOL "Force the use of SDL instead of using native builds.")

IF(NOT WIN32)
	SET(SYS_LIBS ${SYS_LIBS} m)
ELSE()
	SET(SYS_LIBS ${SYS_LIBS})
ENDIF()

FIND_PACKAGE(ZLIB)
IF(ZLIB_FOUND)
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};ZLIB_STATIC)
	SET(FTE_LIBS ${FTE_LIBS} ${ZLIB_LIBRARIES})
	SET(FTESV_LIBS ${FTESV_LIBS} ${ZLIB_LIBRARIES})
	SET(FTEQCC_LIBS ${FTEQCC_LIBS} ${ZLIB_LIBRARIES})
ELSE()
	MESSAGE(WARNING "libz library NOT available. compressed pk3 will not be available.")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_ZLIB)
ENDIF()

FIND_PACKAGE(BZip2)
IF(BZIP2_FOUND)
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};AVAIL_BZLIB;BZLIB_STATIC)
	SET(FTE_LIBS ${FTE_LIBS} bz2)
	SET(FTESV_LIBS ${FTESV_LIBS} bz2)
#	MESSAGE(STATUS "bzip2 library found. bz2-compressed pk3s will work for the price of extra bloat! yay!")
ELSE()
	MESSAGE(WARNING "bzip2 library NOT available. bz2-compressed pk3s will not be available, as if anyone cares.")
ENDIF()

SET(OpenGL_GL_PREFERENCE LEGACY)
FIND_PACKAGE(OpenGL)
IF(OpenGL_FOUND)
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};GLQUAKE)
ELSE()
	MESSAGE(WARNING "opengl library NOT available. Will depend upon vulkan.")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_OPENGL)
ENDIF()

FIND_PACKAGE(JPEG)
IF(JPEG_FOUND)
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};LIBJPEG_STATIC)
	SET(FTE_LIBS ${FTE_LIBS} ${JPEG_LIBRARIES})
ELSE()
	MESSAGE(WARNING "libjpeg library NOT available. Who cares?")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_JPEG)
ENDIF()

FIND_PACKAGE(PNG)
IF(PNG_FOUND)
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};LIBPNG_STATIC)
	SET(FTE_LIBS ${FTE_LIBS} ${PNG_LIBRARIES})
ELSE()
	MESSAGE(WARNING "libpng library NOT available. Good luck with screenshots.")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_PNG)
ENDIF()

FIND_PACKAGE(Freetype)
IF(FREETYPE_FOUND)
	INCLUDE_DIRECTORIES( ${FREETYPE_INCLUDE_DIRS} )
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};LIBFREETYPE_STATIC)
	SET(FTE_LIBS ${FTE_LIBS} ${FREETYPE_LIBRARIES})
ELSE()
	MESSAGE(WARNING "freetype library NOT available. I hope you're okay with ascii.")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_FREETYPE)
ENDIF()

FIND_PATH(VULKAN_INCLUDE_DIR vulkan/vulkan.h)
IF(VULKAN_INCLUDE_DIR)
	INCLUDE_DIRECTORIES( ${VULKAN_INCLUDE_DIR} )
	SET(FTE_DEFINES ${FTE_DEFINES};VKQUAKE) #no libs required, thankfully
ELSE()
	MESSAGE(WARNING "Vulkan headers NOT available.")
ENDIF()

FIND_LIBRARY(VORBISFILE_LIBRARY NAMES vorbisfile)
IF(NOT VORBISFILE_LIBRARY)
	MESSAGE(WARNING "libvorbisfile library NOT available. Who listens to the bgm anyway?")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_OGG)
ENDIF()

IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-sign")

	IF(CMAKE_BUILD_TYPE MATCHES "Debug")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-pointer-sign -Wno-unknown-pragmas -Wno-format-zero-length -Wno-strict-aliasing -Wno-error=cpp")
	ELSE()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
	ENDIF()
endif()
IF(CMAKE_C_COMPILER_ID MATCHES "GNU")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-prototypes")			#
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wold-style-definition")		#k&r c is weird and can't cope with 64bit types.
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wold-style-declaration")		#
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith")				#void* stuff
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wvla")							#msvc doesn't support vla
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdeclaration-after-statement")	#msvc doesn't allow defs after statements, and they're so very tempting...
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat-truncation=1")
	#TODO SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes")			#for finding missing statics.
	#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")			#
#	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wswitch-enum")					#to warn about omitted enums despite default.

	#might as well do this, public builds use the regular Makefile.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
	IF(CMAKE_BUILD_TYPE MATCHES "Debug")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-pointer-sign -Wno-unknown-pragmas -Wno-format-zero-length -Wno-strict-aliasing -Wno-error=cpp")
	ELSE()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
	ENDIF()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--warn-common")
	#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wc++-compat")	#lul, thousands of errors!
ENDIF()
IF(CMAKE_BUILD_TYPE MATCHES "Debug")
	IF(NOT ${WIN32})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
	ENDIF()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEBUG")
ENDIF()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")

IF(${ANDROID})
#	FIND_PACKAGE(Freetype REQUIRED)

#	INCLUDE_DIRECTORIES(	${FREETYPE_INCLUDE_DIRS} )

	SET(FTE_DEFINES ${FTE_DEFINES};ANDROID;VKQUAKE;MULTITHREAD)
	SET(FTE_LIBS ${FTE_LIBS} android log EGL ${SYS_LIBS} ${CMAKE_DL_LIBS})
	SET(FTE_ARCH_FILES
		engine/client/sys_droid.c
		engine/common/sys_linux_threads.c
		engine/client/snd_droid.c
		engine/client/cd_null.c
		engine/gl/gl_viddroid.c
	)
ELSEIF(${WIN32})
	INCLUDE_DIRECTORIES(engine/libs engine/libs/freetype2/include)
#	LINK_DIRECTORIES(engine/libs/mingw64-libs)
	
	#	engine/server/sv_sys_win.c
	
	SET(FTE_LIBS ${FTE_LIBS} ole32 gdi32 wsock32 winmm dxguid)
	SET(FTE_DEFINES ${FTE_DEFINES};D3D9QUAKE;D3D11QUAKE)
	SET(FTE_ARCH_FILES 
		engine/client/winquake.rc
		engine/common/sys_win_threads.c
		engine/common/net_ssl_winsspi.c
		engine/common/fs_win32.c
		engine/client/cd_win.c
		engine/client/in_win.c
		engine/client/snd_al.c
		engine/client/snd_directx.c
		engine/client/snd_wasapi.c
		engine/client/snd_win.c
		engine/client/snd_xaudio.c
		engine/client/sys_win.c
		
		engine/gl/gl_vidnt.c

		engine/d3d/d3d_backend.c
		engine/d3d/d3d_image.c
		engine/d3d/d3d_shader.c
		engine/d3d/d3d11_backend.c
		engine/d3d/d3d11_image.c
		engine/d3d/d3d11_shader.c
		engine/d3d/d3d8_backend.c
		engine/d3d/d3d8_image.c
		engine/d3d/vid_d3d.c
		engine/d3d/vid_d3d11.c
		engine/d3d/vid_d3d8.c
	)
	
	SET(FTESV_LIBS ${FTESV_LIBS} wsock32 winmm)
	SET(FTESV_ARCH_FILES 
		engine/client/winquake.rc
		engine/common/sys_win_threads.c
		engine/common/net_ssl_winsspi.c
		engine/common/fs_win32.c
		engine/server/sv_sys_win.c
	)
ELSEIF(${UNIX} AND NOT FTE_USE_SDL)	#linux(ish)
				#openbsd will have issues with snd_linux.c

	FIND_PACKAGE(GnuTLS)
	IF(NOT GNUTLS_FOUND)
		MESSAGE(WARNING "gnutls library NOT available. HTTPS/DTLS will not be available.")
		SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_GNUTLS)
	ENDIF()

	#linux-only packages
	FIND_PACKAGE(ALSA)
	IF(ALSA_FOUND)
		SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};AUDIO_ALSA;AUDIO_PULSE)
	ELSE()
		MESSAGE(WARNING "asound (alsa) library NOT available.")
	ENDIF()

	FIND_PACKAGE(X11)
	IF(X11_FOUND)
		IF (NOT X11_Xcursor_FOUND)
			SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_X11_CURSOR)
			MESSAGE(WARNING "Xcursor library NOT available.")
		ENDIF()
		IF (NOT X11_Xrandr_FOUND)
			SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_X11_RANDR)
			MESSAGE(WARNING "Xrandr library NOT available.")
		ENDIF()
	ELSE()
		MESSAGE(WARNING "x11 library NOT available.")
		SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_X11)
	ENDIF()

	SET(FTE_DEFINES ${FTE_DEFINES};DYNAMIC_SDL;MULTITHREAD)
	SET(FTE_LIBS ${FTE_LIBS} ${SYS_LIBS} ${CMAKE_DL_LIBS} pthread ${SDL2_LIBRARIES})
	SET(FTE_ARCH_FILES ${FTE_ARCH_FILES}
		engine/client/sys_linux.c
		engine/common/sys_linux_threads.c
		engine/common/net_ssl_gnutls.c

		engine/client/snd_al.c
		engine/client/snd_alsa.c
		engine/client/snd_linux.c
		engine/client/snd_pulse.c
		engine/client/snd_sdl.c	#we use SDL audio even without sys_sdl, because of pulseaudio fucking over alsa, alsa fucking over oss3, and oss4 not being used. Either way, openal should be the default anyway.

		engine/client/cd_linux.c
		engine/gl/gl_vidlinuxglx.c
		engine/gl/gl_videgl.c

#		engine/gl/gl_vidrpi.c
#		engine/gl/gl_vidwayland.c
	)

	#openbsd uses a libossaudio library for all the oss stuff, use that to ensure that we still get sound
	FIND_LIBRARY(
		OSSAUDIO_LIBRARY
		NAMES ossaudio
	)
	IF(OSSAUDIO_LIBRARY)
		SET(FTE_LIBS ${FTE_LIBS} ${OSSAUDIO_LIBRARY})
	ENDIF()

	#on linux, use wayland (we normally dynamically link, but we still need headers).
	FIND_LIBRARY(
		WAYLAND_CLIENT_LIBRARY
		NAMES wayland-client libwayland-client
	)
	FIND_LIBRARY(
		WAYLAND_EGL
		NAMES wayland-egl
	)
	FIND_LIBRARY(
		HAVE_XKBCOMMON
		NAMES xkbcommon
	)
	IF(NOT HAVE_XKBCOMMON)
		MESSAGE(WARNING "xkbcommon library not found, needed for wayland to be usable.")
		UNSET(WAYLAND_CLIENT_LIBRARY)
	ENDIF()
	IF(WAYLAND_CLIENT_LIBRARY AND WAYLAND_EGL)
#		SET(FTE_LIBS ${FTE_LIBS} ${WAYLAND_CLIENT_LIBRARY} ${HAVE_XKBCOMMON} ${WAYLAND_EGL})
#		SET(FTE_DEFINES ${FTE_DEFINES};STATIC_WAYLAND)

		SET(FTE_DEFINES ${FTE_DEFINES};WAYLANDQUAKE;USE_EGL)
		SET(FTE_ARCH_FILES ${FTE_ARCH_FILES}
			engine/gl/gl_vidwayland.c
		)
	ELSE()
		MESSAGE(WARNING "Wayland library NOT available. X11 will live forever anyway.")
		IF(NOT X11_FOUND)
			MESSAGE(WARNING "No renderers supported!")
			SET(FTE_NO_RENDERERS 1)
		ENDIF()
	ENDIF()

	SET(FTESV_DEFINES MULTITHREAD)
	SET(FTESV_ARCH_FILES ${FTESV_ARCH_FILES}
		engine/server/sv_sys_unix.c
		engine/common/sys_linux_threads.c
		engine/common/net_ssl_gnutls.c
	)
	SET(FTESV_LIBS ${FTESV_LIBS} ${SYS_LIBS} ${CMAKE_DL_LIBS} pthread)

ELSEIF(1)	#SDL
#	FIND_PACKAGE(Freetype REQUIRED)
#	INCLUDE_DIRECTORIES(engine/libs engine/libs/freetype2/include)

	FIND_PACKAGE(PkgConfig REQUIRED)
	PKG_SEARCH_MODULE(sdl2 REQUIRED sdl2)

	FIND_PACKAGE(SDL2 REQUIRED)

	INCLUDE_DIRECTORIES(${FREETYPE_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
	
	#SDL2.0.7 supports vulkan, so lets use it.
	SET(FTE_DEFINES ${FTE_DEFINES};FTE_SDL;MULTITHREAD)
	SET(FTE_LIBS ${FTE_LIBS} ${SYS_LIBS} ${CMAKE_DL_LIBS} ${SDL2_LIBRARIES})
	SET(FTE_ARCH_FILES
		engine/client/sys_sdl.c
		engine/client/snd_al.c
		engine/client/snd_sdl.c
		engine/client/in_sdl.c
		engine/client/cd_sdl.c
		engine/gl/gl_vidsdl.c
	)

	SET(FTESV_DEFINES FTE_SDL)
	SET(FTESV_LIBS ${FTESV_LIBS} ${SYS_LIBS} ${CMAKE_DL_LIBS} ${SDL2_LIBRARIES})

	IF(WIN32)
		SET(FTE_LIBS ${FTE_LIBS} wsock32 gdi32 ole32)
		SET(FTE_DEFINES ${FTE_DEFINES};NO_DIRECTX)
		SET(FTE_ARCH_FILES ${FTE_ARCH_FILES}
			engine/client/winquake.rc
			engine/common/net_ssl_winsspi.c
		)
		SET(FTESV_ARCH_FILES ${FTESV_ARCH_FILES}
			engine/client/winquake.rc
			engine/common/net_ssl_winsspi.c
			engine/server/sv_sys_win.c
		)
	ELSE()
		SET(FTE_ARCH_FILES ${FTE_ARCH_FILES}
			engine/common/net_ssl_gnutls.c
		)
		SET(FTESV_ARCH_FILES ${FTESV_ARCH_FILES}
			engine/common/net_ssl_gnutls.c
			engine/common/sys_linux_threads.c
			engine/server/sv_sys_unix.c
		)
	ENDIF()
ELSE()
#	engine/common/sys_linux_threads.c
#	engine/common/net_ssl_gnutls.c
#	engine/server/sv_sys_unix.c

#	engine/client/snd_alsa.c
#	engine/client/snd_droid.c
#	engine/client/snd_linux.c
#	engine/client/snd_macos.c
#	engine/client/snd_morphos.c
#	engine/client/snd_sblaster.c
#	engine/client/snd_sdl.c
#	engine/client/snd_sndio.c

#	engine/client/sys_dos.c
#	engine/client/sys_droid.c
#	engine/client/sys_linux.c
#	engine/client/sys_morphos.c
#	engine/client/sys_npfte.c
#	engine/client/sys_plugfte.c
#	engine/client/sys_sdl.c
#	engine/client/sys_xdk.c

#	engine/client/cd_linux.c
#	engine/client/cd_null.c
#	engine/client/cd_sdl.c
#	engine/client/in_morphos.c
#	engine/client/in_sdl.c
	
#	engine/gl/gl_viddroid.c
#	engine/gl/gl_videgl.c
#	engine/gl/gl_vidlinuxglx.c
#	engine/gl/gl_vidmacos.c
#	engine/gl/gl_vidmorphos.c
#	engine/gl/gl_vidnull.c
#	engine/gl/gl_vidrpi.c
#	engine/gl/gl_vidsdl.c
#	engine/gl/gl_vidtinyglstubs.c
#	engine/gl/gl_vidwayland.c
ENDIF()

SET(FTE_GL_FILES
#These are GL-specific, but can be left even if no gl is supported.
	engine/gl/gl_backend.c
	engine/gl/gl_bloom.c
	engine/gl/gl_draw.c
	engine/gl/gl_rmain.c
	engine/gl/gl_rmisc.c
	engine/gl/gl_rsurf.c
	engine/gl/gl_screen.c
	engine/gl/gl_vidcommon.c
	engine/gl/glmod_doom.c
)
SET(FTE_VK_FILES
	engine/vk/vk_backend.c
	engine/vk/vk_init.c
)
SET(FTE_QCVM_FILES
	engine/qclib/comprout.c
	engine/qclib/initlib.c
	engine/qclib/pr_edict.c
	engine/qclib/pr_exec.c
	engine/qclib/pr_multi.c
	engine/qclib/qcc_cmdlib.c
	engine/qclib/qcc_pr_comp.c
	engine/qclib/qcc_pr_lex.c
#	engine/qclib/decomp.c
#	engine/qclib/packager.c
#	engine/qclib/pr_x86.c
#	engine/qclib/qccgui.c
#	engine/qclib/qccguistuff.c
#	engine/qclib/qcctui.c
	engine/qclib/qccmain.c
	engine/qclib/qcd_main.c
	engine/qclib/qcdecomp.c
)
SET(FTE_COMMON_FILES
	#these files are common to both server-only and client+server builds.
	engine/common/cmd.c
	engine/common/com_mesh.c
	engine/common/com_bih.c
	engine/common/common.c
	engine/common/crc.c
	engine/common/cvar.c
	engine/common/fs.c
	engine/common/fs_dzip.c
	engine/common/fs_pak.c
	engine/common/fs_stdio.c
	engine/common/fs_xz.c
	engine/common/fs_zip.c
	engine/common/fs_vpk.c
	engine/common/gl_q2bsp.c
	engine/common/huff.c
	engine/common/log.c
	engine/common/mathlib.c
	engine/common/md4.c
	engine/common/net_chan.c
	engine/common/net_ice.c
	engine/common/net_wins.c
	engine/common/plugin.c
	engine/common/pmove.c
	engine/common/pmovetst.c
	engine/common/pr_bgcmd.c
	engine/common/q1bsp.c
	engine/common/q2pmove.c
	engine/common/q3common.c
	engine/common/qvm.c
	engine/common/sha1.c
	engine/common/sha2.c
	engine/common/translate.c
	engine/common/zone.c

	#important headers
	engine/common/bothdefs.h
	engine/common/config_fteqw.h
	engine/common/config_minimal.h
	engine/common/config_nocompat.h
	engine/common/config_wastes.h
	engine/common/config_freecs.h
	engine/common/config_fteqw_noweb.h

	#useless headers that I'll never search for
	engine/client/api_menu.h
	engine/client/cdaudio.h
	engine/client/client.h
	engine/client/cl_ignore.h
	engine/client/cl_master.h
	engine/client/clq3defs.h
	engine/client/input.h
	engine/client/keys.h
	engine/client/menu.h
	engine/client/merged.h
	engine/client/modelgen.h
	engine/client/quakedef.h
	engine/client/render.h
	engine/client/sbar.h
	engine/client/screen.h
	engine/client/sound.h
	engine/client/spritegn.h
#	engine/client/sys_plugfte.h
	engine/client/vid.h
	engine/client/view.h
	engine/client/wad.h
#	engine/client/winquake.h
	engine/common/bothdefs.h
	engine/common/bspfile.h
	engine/common/cmd.h
	engine/common/com_mesh.h
	engine/common/common.h
	engine/common/console.h
	engine/common/cvar.h
	engine/common/fs.h
	engine/common/mathlib.h
	engine/common/net.h
	engine/common/netinc.h
	engine/common/particles.h
	engine/common/pmove.h
	engine/common/pr_common.h
	engine/common/protocol.h
	engine/common/sys.h
	engine/common/translate.h
	engine/common/ui_public.h
	engine/common/vm.h
	engine/common/world.h
	engine/common/zone.h
	engine/gl/gl_draw.h
	engine/gl/gl_model.h
	engine/gl/glquake.h
	engine/gl/glsupp.h
	engine/gl/gl_terrain.h
	engine/gl/gl_videgl.h
	engine/gl/model_hl.h
	engine/gl/shader.h
	engine/http/iweb.h
	engine/qclib/cmdlib.h
	engine/qclib/execloop.h
	engine/qclib/gui.h
	engine/qclib/hash.h
	engine/qclib/pr_comp.h
	engine/qclib/progsint.h
	engine/qclib/progslib.h
	engine/qclib/progtype.h
	engine/qclib/qcc.h
	engine/qclib/qcd.h
	engine/server/botlib.h
	engine/server/progdefs.h
	engine/server/progs.h
	engine/server/q2game.h
	engine/server/q3g_public.h
	engine/server/server.h
	#engine/server/svhl_gcapi.h
	engine/server/sv_sql.h
	#engine/sw/sw.h
	#engine/sw/sw_spans.h
	engine/vk/vkrenderer.h
	engine/web/ftejslib.h


	#sigh
	engine/client/pr_skelobj.c
	engine/client/m_download.c
	engine/client/net_master.c
	
	#these are here because of hitmodel etc
	engine/gl/gl_heightmap.c
	engine/gl/gl_hlmdl.c
	engine/gl/gl_model.c
	
	engine/server/sv_move.c
	engine/server/sv_phys.c
	engine/server/world.c
	
	${FTE_QCVM_FILES}
	engine/qclib/hash.c
	
	engine/http/httpclient.c
)

SET(FTE_SERVER_FILES
	engine/server/net_preparse.c
	engine/server/pr_cmds.c
	engine/server/pr_lua.c
	engine/server/pr_q1qvm.c
	engine/server/savegame.c
	engine/server/sv_ccmds.c
	engine/server/sv_chat.c
	engine/server/sv_cluster.c
	engine/server/sv_demo.c
	engine/server/sv_ents.c
	engine/server/sv_init.c
	engine/server/sv_main.c
	engine/server/sv_master.c
	engine/server/sv_mvd.c
	engine/server/sv_nchan.c
	engine/server/sv_rankin.c
	engine/server/sv_send.c
	engine/server/sv_sql.c
	engine/server/sv_user.c
#	engine/server/svhl_game.c
#	engine/server/svhl_phys.c
#	engine/server/svhl_world.c
	engine/server/svq2_ents.c
	engine/server/svq2_game.c
	engine/server/svq3_game.c
)

#these files are only in the client
SET(FTE_CLIENT_FILES
	engine/client/cl_cam.c
	engine/client/cl_cg.c
	engine/client/cl_demo.c
	engine/client/cl_ents.c
	engine/client/cl_ignore.c
	engine/client/cl_input.c
	engine/client/cl_main.c
	engine/client/cl_parse.c
	engine/client/cl_pred.c
	engine/client/cl_screen.c
	engine/client/cl_tent.c
	engine/client/cl_ui.c
#	engine/client/clhl_game.c
	engine/client/clq2_cin.c
	engine/client/clq2_ents.c
	engine/client/clq3_parse.c
	engine/client/console.c
	engine/client/fragstats.c
	engine/client/image.c
	engine/client/in_generic.c
	engine/client/keys.c
	engine/client/m_items.c
	engine/client/m_master.c
	engine/client/m_mp3.c
	engine/client/m_multi.c
	engine/client/m_options.c
	engine/client/m_script.c
	engine/client/m_native.c
	engine/client/m_single.c
	engine/client/menu.c
	engine/client/p_classic.c
	engine/client/p_null.c
	engine/client/p_script.c
	engine/client/pr_clcmd.c
	engine/client/pr_csqc.c
	engine/client/pr_menu.c
	engine/client/r_2d.c
	engine/client/r_d3.c
	engine/client/r_part.c
	engine/client/r_partset.c
	engine/client/r_surf.c
	engine/client/renderer.c
	engine/client/renderque.c
	engine/client/roq_read.c
	engine/client/sbar.c
	engine/client/skin.c
	engine/client/snd_dma.c
	engine/client/snd_mem.c
	engine/client/snd_mix.c
	engine/client/snd_mp3.c
	engine/client/snd_ov.c
	engine/client/textedit.c
	engine/client/valid.c
	engine/client/view.c
	engine/client/wad.c
	engine/client/zqtp.c


#These are generic renderer files and no longer gl-specific (for the most part)
	engine/gl/gl_alias.c
	engine/gl/gl_font.c
	engine/gl/gl_ngraph.c
	engine/gl/gl_rlight.c
	engine/gl/gl_shader.c
	engine/gl/gl_shadow.c
	engine/gl/gl_warp.c
	engine/gl/ltface.c

#these are renderer-specific
	engine/client/vid_headless.c
	${FTE_GL_FILES}
	${FTE_VK_FILES}
)

ADD_LIBRARY(fteq3bot MODULE
	engine/botlib/be_aas_bspq3.c
	engine/botlib/be_aas_entity.c
	engine/botlib/be_aas_move.c
	engine/botlib/be_aas_routealt.c
	engine/botlib/be_ai_char.c
	engine/botlib/be_ai_goal.c
	engine/botlib/be_ai_weight.c
	engine/botlib/l_crc.c
	engine/botlib/l_memory.c
	engine/botlib/l_struct.c
	engine/botlib/be_aas_cluster.c
	engine/botlib/be_aas_file.c
	engine/botlib/be_aas_optimize.c
	engine/botlib/be_aas_route.c
	engine/botlib/be_ai_chat.c
	engine/botlib/be_ai_move.c
	engine/botlib/be_ea.c
	engine/botlib/l_libvar.c
	engine/botlib/l_precomp.c
	engine/botlib/be_aas_debug.c
	engine/botlib/be_aas_main.c
	engine/botlib/be_aas_reach.c
	engine/botlib/be_aas_sample.c
	engine/botlib/be_ai_gen.c
	engine/botlib/be_ai_weap.c
	engine/botlib/be_interface.c
	engine/botlib/l_log.c
	engine/botlib/l_script.c
	engine/botlib/standalone.c
)
SET_TARGET_PROPERTIES(fteq3bot PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES};${FTE_DEFINES};${FTE_REVISON};BOTLIB;EXTERNALBOTLIB")
TARGET_LINK_LIBRARIES(fteq3bot ${FTE_LIBS} )
SET_TARGET_PROPERTIES(fteq3bot PROPERTIES LINK_FLAGS "-Wl,--no-undefined")

FILE(STRINGS "${FTE_BUILD_CONFIG}" BULLET_INTERNAL REGEX "^#define[\t ]+USE_INTERNAL_BULLET")
IF(BULLET_INTERNAL)
	#Built-in bullet physics plugin...
	FIND_PACKAGE(Bullet REQUIRED)
	SET(FTE_COMMON_FILES ${FTE_COMMON_FILES} plugins/bullet/bulletplug.cpp)
	INCLUDE_DIRECTORIES( ${BULLET_INCLUDE_DIRS} )
	SET(FTE_LIBS ${FTE_LIBS} ${BULLET_LIBRARIES})
	SET(FTESV_LIBS ${FTESV_LIBS} ${BULLET_LIBRARIES})
ELSE()
	#Bullet Physics library plugin
	SET(FTE_PLUG_BULLET true CACHE BOOL "Compile bullet rigid body physics plugin.")
	IF(FTE_PLUG_BULLET)
		FIND_PACKAGE(Bullet)
		IF (BULLET_FOUND)
			ADD_LIBRARY(plug_bullet MODULE
				plugins/plugin.c
				plugins/bullet/bulletplug.cpp
			)
			TARGET_INCLUDE_DIRECTORIES(plug_bullet PUBLIC ${BULLET_INCLUDE_DIRS})
			SET_TARGET_PROPERTIES(plug_bullet PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
			SET_TARGET_PROPERTIES(plug_bullet PROPERTIES OUTPUT_NAME "bullet")
			SET_TARGET_PROPERTIES(plug_bullet PROPERTIES PREFIX "fteplug_")
			SET_TARGET_PROPERTIES(plug_bullet PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
			TARGET_LINK_LIBRARIES(plug_bullet ${SYS_LIBS} ${BULLET_LIBRARIES})
			SET(INSTALLTARGS ${INSTALLTARGS} plug_bullet)
		ELSE()
			MESSAGE(WARNING "bullet library not detected, skipping plugin")
		ENDIF()
	ENDIF()
ENDIF()

IF(ANDROID)
	#android sucks. everything is a library. so we build the engine as a shared library and completely ignore dedicated servers+tools
	ADD_LIBRARY(ftedroid MODULE
		${FTE_ARCH_FILES}
		${FTE_COMMON_FILES}
		${FTE_CLIENT_FILES}
	)
	SET_TARGET_PROPERTIES(ftedroid PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES};${FTE_DEFINES};${FTE_REVISON}")
	TARGET_LINK_LIBRARIES(ftedroid ${FTE_LIBS} )
	SET(INSTALLTARGS ${INSTALLTARGS} ftedroid)
ELSE()
	#systems that actually have executables...
	ADD_EXECUTABLE(fteqw WIN32
		${FTE_ARCH_FILES}
		${FTE_COMMON_FILES}
		${FTE_CLIENT_FILES}
		${FTE_SERVER_FILES}
	)
	SET_TARGET_PROPERTIES(fteqw PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES};${FTE_DEFINES};${FTE_REVISON}")
	TARGET_LINK_LIBRARIES(fteqw ${FTE_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} fteqw)

	ADD_EXECUTABLE(fteqw-sv
		${FTESV_ARCH_FILES}
		${FTE_COMMON_FILES}
		${FTE_SERVER_FILES}
	)
	SET_TARGET_PROPERTIES(fteqw-sv PROPERTIES COMPILE_DEFINITIONS "SERVERONLY;${FTE_LIB_DEFINES};${FTESV_DEFINES};${FTE_REVISON}")
	TARGET_LINK_LIBRARIES(fteqw-sv ${FTESV_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} fteqw-sv)

	IF(0)
		ADD_EXECUTABLE(fteqw-cl
			${FTE_ARCH_FILES}
			${FTE_COMMON_FILES}
			${FTE_CLIENT_FILES}
		)
		SET_TARGET_PROPERTIES(fteqw-cl PROPERTIES COMPILE_DEFINITIONS "CLIENTONLY;${FTE_LIB_DEFINES};${FTE_DEFINES};${FTE_REVISON}")
		TARGET_LINK_LIBRARIES(fteqw-cl ${FTE_LIBS})
		SET(INSTALLTARGS ${INSTALLTARGS} fteqw-cl)
	ENDIF()

	FIND_PACKAGE(SDL2)
	IF(SDL2_FOUND)
		SET(FTE_SDL2 FTE_SDL)
		SET(FTE_SDL2_INCLUDES ${SDL2_INCLUDE_DIRS})
	ENDIF()

	SET(FTE_TOOL_IQM true CACHE BOOL "Compile IQM Tool.")
	IF(FTE_TOOL_IQM)
		ADD_EXECUTABLE(iqmtool
			iqm/iqm.cpp
			plugins/models/gltf.c
			engine/client/image.c
			imgtool.c
			iqm/iqm.h
		)
		SET_TARGET_PROPERTIES(iqmtool PROPERTIES COMPILE_DEFINITIONS "IQMTOOL;${FTE_REVISON}")
		TARGET_LINK_LIBRARIES(iqmtool ${CMAKE_DL_LIBS})
		SET(INSTALLTARGS ${INSTALLTARGS} iqmtool)
	ENDIF()

	SET(FTE_TOOL_IMAGE true CACHE BOOL "Compile Image Tool.")
	IF(FTE_TOOL_IMAGE)
		ADD_EXECUTABLE(imgtool
			engine/client/image.c
			imgtool.c
		)
		TARGET_INCLUDE_DIRECTORIES(imgtool PUBLIC ${FTE_SDL2_INCLUDES})
		SET_TARGET_PROPERTIES(imgtool PROPERTIES COMPILE_DEFINITIONS "IMGTOOL;${FTE_LIB_DEFINES};${FTE_DEFINES};${FTE_REVISON};${FTE_SDL2}")
		TARGET_LINK_LIBRARIES(imgtool ${FTE_LIBS} )
		SET(INSTALLTARGS ${INSTALLTARGS} imgtool)
	ENDIF()

	SET(FTE_TOOL_QTV true CACHE BOOL "Compile qtv server.")
	IF(FTE_TOOL_QTV)
		ADD_EXECUTABLE(qtv
			fteqtv/netchan.c
			fteqtv/parse.c
			fteqtv/msg.c
			fteqtv/qw.c
			fteqtv/source.c
			fteqtv/bsp.c
			fteqtv/rcon.c
			fteqtv/mdfour.c
			fteqtv/crc.c
			fteqtv/control.c
			fteqtv/forward.c
			fteqtv/pmove.c
			fteqtv/menu.c
			fteqtv/httpsv.c
			fteqtv/libqtvc/glibc_sucks.c
			engine/common/sha1.c
		)
		SET_TARGET_PROPERTIES(qtv PROPERTIES COMPILE_DEFINITIONS "${FTE_REVISON}")
		IF(WIN32)
			TARGET_LINK_LIBRARIES(qtv ws2_32 winmm ${SYS_LIBS})
		ELSE()
			TARGET_LINK_LIBRARIES(qtv ${SYS_LIBS})
		ENDIF()
		SET(INSTALLTARGS ${INSTALLTARGS} qtv)
	ENDIF()

	SET(FTE_TOOL_MASTER true CACHE BOOL "Compile master server.")
	IF(FTE_TOOL_MASTER)
		IF(NOT WIN32)
			ADD_EXECUTABLE(ftemaster
				${FTESV_ARCH_FILES}
				engine/server/sv_master.c
				engine/common/net_ice.c		#for the stun responses.
				engine/common/net_wins.c
				engine/common/cvar.c
				engine/common/cmd.c
				engine/common/sha1.c		#for websockets
				engine/http/httpclient.c	#for the pipe stuff
				engine/common/log.c
				engine/common/fs.c
				engine/common/fs_stdio.c
				engine/common/common.c
				engine/common/translate.c
				engine/common/zone.c
				engine/qclib/hash.c
			)
			SET_TARGET_PROPERTIES(ftemaster PROPERTIES COMPILE_DEFINITIONS "MASTERONLY;${FTE_LIB_DEFINES};${FTESV_DEFINES};${FTE_REVISON}")
			TARGET_LINK_LIBRARIES(ftemaster ${FTESV_LIBS})
			SET(INSTALLTARGS ${INSTALLTARGS} ftemaster)
		ENDIF()
	ENDIF()

	SET(FTE_TOOL_HTTPSV true CACHE BOOL "Compile small http server.")
	IF(FTE_TOOL_HTTPSV)
		ADD_EXECUTABLE(httpserver
			engine/common/fs_stdio.c
			engine/http/httpserver.c
			engine/http/iwebiface.c
			engine/http/ftpserver.c
		)
		SET_TARGET_PROPERTIES(httpserver PROPERTIES COMPILE_DEFINITIONS "WEBSERVER;WEBSVONLY;${FTE_REVISON}")
		IF(WIN32)
			TARGET_LINK_LIBRARIES(httpserver ws2_32)
		ENDIF()
		#SET(INSTALLTARGS ${INSTALLTARGS} httpserver)
	ENDIF()

	SET(FTE_TOOL_QCC true CACHE BOOL "Compile commandline qc compiler.")
	IF(FTE_TOOL_QCC)
		ADD_EXECUTABLE(fteqcc
			engine/qclib/qcctui.c
			engine/qclib/comprout.c
			engine/qclib/hash.c
			engine/qclib/qcc_cmdlib.c
			engine/qclib/qcc_pr_comp.c
			engine/qclib/qcc_pr_lex.c
			engine/qclib/qccmain.c
			engine/qclib/qcd_main.c
			engine/qclib/packager.c
		)
		SET_TARGET_PROPERTIES(fteqcc PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES};${FTE_REVISON}")
		TARGET_LINK_LIBRARIES(fteqcc ${FTEQCC_LIBS} ${SYS_LIBS})
		SET(INSTALLTARGS ${INSTALLTARGS} fteqcc)
	ENDIF()

	SET(FTE_TOOL_QCCGUI true CACHE BOOL "Compile gui qc compiler.")
	IF(FTE_TOOL_QCCGUI)
		IF(${WIN32})
			ADD_EXECUTABLE(fteqccgui WIN32
				engine/qclib/qccgui.c
				engine/qclib/qccguistuff.c
				engine/qclib/comprout.c
				engine/qclib/hash.c
				engine/qclib/qcc_cmdlib.c
				engine/qclib/qcc_pr_comp.c
				engine/qclib/qcc_pr_lex.c
				engine/qclib/qccmain.c
				engine/qclib/decomp.c
				engine/qclib/packager.c
				engine/qclib/qcd_main.c
			)
			SET_TARGET_PROPERTIES(fteqccgui PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES};${FTE_REVISON}")
			TARGET_LINK_LIBRARIES(fteqccgui ${FTEQCC_LIBS} shlwapi ole32 comctl32 comdlg32)
			SET(INSTALLTARGS ${INSTALLTARGS} fteqccgui)
		ELSE()
			FIND_PACKAGE(Qt5Widgets)

			FIND_PATH(QSCINTILLA_INCLUDE_DIR
				NAMES Qsci/qsciglobal.h
				PATHS ${Qt5Widgets_INCLUDE_DIRS}
				PATH_SUFFIXES Qsci
			)
			FIND_LIBRARY(QSCINTILLA_LIBRARY
				NAMES qscintilla2_qt5
				PATHS
					${QT_LIBRARY_DIR}
					/usr/local/lib
					/usr/local/lib/qt5
					/usr/lib
			)

			IF (QSCINTILLA_INCLUDE_DIR AND QSCINTILLA_LIBRARY AND Qt5Widgets_FOUND)
				ADD_EXECUTABLE(fteqccgui
					engine/qclib/qccguiqt.cpp
					engine/qclib/qccguistuff.c
					engine/qclib/comprout.c
					engine/qclib/hash.c
					engine/qclib/qcc_cmdlib.c
					engine/qclib/qcc_pr_comp.c
					engine/qclib/qcc_pr_lex.c
					engine/qclib/qccmain.c
					engine/qclib/decomp.c
#					engine/qclib/packager.c
					engine/qclib/qcd_main.c
				)
				TARGET_INCLUDE_DIRECTORIES(fteqccgui PUBLIC ${Qt5Widgets_INCLUDE_DIRS} ${QSCINTILLA_INCLUDE_DIR})
				SET_TARGET_PROPERTIES(fteqccgui PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES};${FTE_REVISON};${Qt5Widgets_COMPILE_DEFINITIONS}")
				SET_PROPERTY(TARGET fteqccgui PROPERTY POSITION_INDEPENDENT_CODE TRUE)
				TARGET_LINK_LIBRARIES(fteqccgui ${FTEQCC_LIBS} ${Qt5Widgets_LIBRARIES} ${QSCINTILLA_LIBRARY})
				SET(INSTALLTARGS ${INSTALLTARGS} fteqccgui)
			ELSE()
				MESSAGE(WARNING "qscintilla/qt5widgets library not detected, no fteqccgui for you")
			ENDIF()
		ENDIF()
	ENDIF()
ENDIF()

IF(0)
	#software renderer plugin
	#not stable enough, and probably won't ever be
	ADD_LIBRARY(sw MODULE
		plugins/plugin.c
		engine/sw/sw_backend.c
		engine/sw/sw_image.c
		engine/sw/sw_rast.c
		#engine/sw/sw_viddos.c
#		engine/sw/sw_vidwin.c
		engine/common/mathlib.c
#		engine/client/in_win.c

		engine/sw/sw.h
		engine/sw/sw_spans.h
	)
	SET_TARGET_PROPERTIES(sw PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES};SWQUAKE")
	SET_TARGET_PROPERTIES(sw PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(sw PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(sw ${SYS_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} sw)
ENDIF()

#Quake Injector Alike plugin
SET(FTE_PLUG_QI true CACHE BOOL "Compile Quake-Injnector plugin.")
IF(FTE_PLUG_QI)
	ADD_LIBRARY(plug_qi MODULE
		plugins/plugin.c
			plugins/qi/qi.c
		plugins/emailnot/md5.c
		plugins/jabber/xml.c
	)
	SET_TARGET_PROPERTIES(plug_qi PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_qi PROPERTIES OUTPUT_NAME "qi")
	SET_TARGET_PROPERTIES(plug_qi PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_qi PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(plug_qi ${SYS_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} plug_qi)
ENDIF()

SET(FTE_PLUG_OPENSSL false CACHE BOOL "Compile OpenSSL (beware license).")
IF(FTE_PLUG_OPENSSL)
	#the openssl license is incompatible with the GPL, so while we have code to use it distributing the binaries built with it is not a (legal) option.
	#note that openssl 3.0.0 upwards are apache-2 licensed, which IS gpl-3 compatible (though not gpl-2). debian has not caught up with that yet, however.
	FIND_PACKAGE(OpenSSL)
	IF(NOT OPENSSL_FOUND)
		MESSAGE(WARNING "openssl library NOT available. you'll have to use some other library.")
	ELSE()
		IF(OPENSSL_VERSION_MAJOR LESS 3 AND NOT FTE_PRIVATE_USE_ONLY)
			MESSAGE(WARNING "openssl library version is not 3 or above. You may not distribute plugin binaries due to license conflict.")
		ELSE()
			MESSAGE(WARNING "Using openssl. Resulting plugin must be licensed as GPLv3.")
		ENDIF()
		SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES})

		ADD_LIBRARY(plug_openssl MODULE
			plugins/plugin.c
			plugins/net_ssl_openssl.c
		)
		SET_TARGET_PROPERTIES(plug_openssl PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
		SET_TARGET_PROPERTIES(plug_openssl PROPERTIES OUTPUT_NAME "openssl")
		SET_TARGET_PROPERTIES(plug_openssl PROPERTIES PREFIX "fteplug_")
		SET_TARGET_PROPERTIES(plug_openssl PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
		TARGET_LINK_LIBRARIES(plug_openssl ${SYS_LIBS} ${OPENSSL_LIBRARIES})
		SET(INSTALLTARGS ${INSTALLTARGS} plug_openssl)
	ENDIF()
ENDIF()

#ODE Physics library plugin
SET(FTE_PLUG_ODE true CACHE BOOL "Compile ODE rigid body physics plugin.")
IF(FTE_PLUG_ODE)
	FIND_PATH(LIBODE_INCLUDE_DIR ode/ode.h)
	IF (LIBODE_INCLUDE_DIR)
		FIND_LIBRARY(LIBODE_LIBRARY ode)
	ENDIF()
	IF (LIBODE_LIBRARY)
		ADD_LIBRARY(plug_ode MODULE
			plugins/plugin.c
			engine/common/com_phys_ode.c
			engine/common/mathlib.c
		)
		TARGET_INCLUDE_DIRECTORIES(plug_ode PUBLIC ${ODE_INCLUDE_DIRS})
		SET_TARGET_PROPERTIES(plug_ode PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;ODE_STATIC")
		SET_TARGET_PROPERTIES(plug_ode PROPERTIES OUTPUT_NAME "ode")
		SET_TARGET_PROPERTIES(plug_ode PROPERTIES PREFIX "fteplug_")
		SET_TARGET_PROPERTIES(plug_ode PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
		TARGET_LINK_LIBRARIES(plug_ode ${SYS_LIBS} ${LIBODE_LIBRARY})
		SET(INSTALLTARGS ${INSTALLTARGS} plug_ode)
	ELSE()
		MESSAGE(WARNING "ODE library not found, no ode plugin for you")
	ENDIF()
ENDIF()

#EzQuake Hud port plugin
SET(FTE_PLUG_EZHUD true CACHE BOOL "Compile MoreQuakeWorld Hud plugin .")
IF(FTE_PLUG_EZHUD)
	ADD_LIBRARY(plug_ezhud MODULE
		plugins/plugin.c
		plugins/ezhud/ezquakeisms.c
		plugins/ezhud/hud.c
		plugins/ezhud/hud_common.c
		plugins/ezhud/hud_editor.c
	)
	SET_TARGET_PROPERTIES(plug_ezhud PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_ezhud PROPERTIES OUTPUT_NAME "ezhud")
	SET_TARGET_PROPERTIES(plug_ezhud PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_ezhud PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(plug_ezhud ${SYS_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} plug_ezhud)
ENDIF()

#NameMaker string generation plugin
SET(FTE_PLUG_NAMEMAKER false CACHE BOOL "Compile namemaker plugin.")
IF(FTE_PLUG_NAMEMAKER)
	ADD_LIBRARY(plug_namemaker MODULE
		plugins/plugin.c
		plugins/namemaker/namemaker.c
	)
	SET_TARGET_PROPERTIES(plug_namemaker PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_namemaker PROPERTIES OUTPUT_NAME "namemaker")
	SET_TARGET_PROPERTIES(plug_namemaker PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_namemaker PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(plug_namemaker ${SYS_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} plug_namemaker)
ENDIF()

#Terrain Generation plugin
SET(FTE_PLUG_TERRAINGEN false CACHE BOOL "Compile sample terrain generation plugin.")
IF(FTE_PLUG_TERRAINGEN)
	ADD_LIBRARY(plug_terraingen MODULE
		plugins/plugin.c
		plugins/terrorgen/terragen.c
	)
	SET_TARGET_PROPERTIES(plug_terraingen PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_terraingen PROPERTIES OUTPUT_NAME "terragen")
	SET_TARGET_PROPERTIES(plug_terraingen PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_terraingen PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(plug_terraingen ${SYS_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} plug_terraingen)
ENDIF()

#IRC client plugin
SET(FTE_PLUG_IRC true CACHE BOOL "Compile irc plugin.")
IF(FTE_PLUG_IRC)
	ADD_LIBRARY(plug_irc MODULE
		plugins/plugin.c
		plugins/irc/ircclient.c
	)
	SET_TARGET_PROPERTIES(plug_irc PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_REVISON};${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_irc PROPERTIES OUTPUT_NAME "irc")
	SET_TARGET_PROPERTIES(plug_irc PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_irc PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(plug_irc ${SYS_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} plug_irc)
ENDIF()

#mpq package format plugin (blizzard games)
SET(FTE_PLUG_MPQ false CACHE BOOL "Compile mpq junk.")
IF(FTE_PLUG_MPQ)
	ADD_LIBRARY(plug_mpq MODULE
		plugins/plugin.c
		plugins/mpq/blast.c
		plugins/mpq/fs_mpq.c
	)
	SET_TARGET_PROPERTIES(plug_mpq PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_mpq PROPERTIES OUTPUT_NAME "mpq")
	SET_TARGET_PROPERTIES(plug_mpq PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_mpq PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(plug_mpq ${SYS_LIBS} ${ZLIB_LIBRARIES})
	SET(INSTALLTARGS ${INSTALLTARGS} plug_mpq)
ENDIF()

#model formats plugin
SET(FTE_PLUG_MODELS true CACHE BOOL "Compile models formats plugin.")
IF(FTE_PLUG_MODELS)
	ADD_LIBRARY(plug_models MODULE
		plugins/plugin.c
		plugins/models/models.c
		plugins/models/gltf.c
		plugins/models/exportiqm.c
	)
	SET_TARGET_PROPERTIES(plug_models PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_models PROPERTIES OUTPUT_NAME "models")
	SET_TARGET_PROPERTIES(plug_models PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_models PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(plug_models ${SYS_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} plug_models)
ENDIF()

SET(FTE_PLUG_X11SV false CACHE BOOL "Compile x11 server plugin.")
IF(FTE_PLUG_X11SV)
	#x11 server plugin (note: for displaying other programs)
	#not stable enough, and probably won't ever be
	ADD_LIBRARY(plug_x11sv MODULE
		plugins/plugin.c
		plugins/xsv/m_x.c
		plugins/xsv/x_reqs.c
		plugins/xsv/x_res.c
		engine/qclib/hash.c
	)
	SET_TARGET_PROPERTIES(plug_x11sv PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_x11sv PROPERTIES OUTPUT_NAME "x11sv")
	SET_TARGET_PROPERTIES(plug_x11sv PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_x11sv PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(plug_x11sv ${SYS_LIBS})
	SET(INSTALLTARGS ${INSTALLTARGS} plug_x11sv)
ENDIF()

#ffmpeg client plugin. no proper way to detect dependancies right now, so I've gotta try the manual way.
SET(FTE_PLUG_FFMPEG true CACHE BOOL "Compile ffmpeg media plugin.")
IF(FTE_PLUG_FFMPEG)
	FIND_PATH(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
	FIND_PATH(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
	FIND_PATH(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
	FIND_PATH(AVSWSCALE_INCLUDE_DIR libswscale/swscale.h)
	IF((AVFORMAT_INCLUDE_DIR) AND (AVSWSCALE_INCLUDE_DIR))
		FIND_LIBRARY(AVCODEC_LIBRARY avcodec)
		FIND_LIBRARY(AVFORMAT_LIBRARY avformat)
		FIND_LIBRARY(AVUTIL_LIBRARY avutil)
		FIND_LIBRARY(AVSWSCALE_LIBRARY swscale)

		ADD_LIBRARY(plug_ffmpeg MODULE
			plugins/plugin.c
			plugins/avplug/avaudio.c
			plugins/avplug/avdecode.c
			plugins/avplug/avencode.c
		)
		TARGET_INCLUDE_DIRECTORIES(plug_ffmpeg PUBLIC ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVSWSCALE_INCLUDE_DIR})
		SET_TARGET_PROPERTIES(plug_ffmpeg PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
		TARGET_LINK_LIBRARIES(plug_ffmpeg ${SYS_LIBS} ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} ${AVSWSCALE_LIBRARY})
		SET_TARGET_PROPERTIES(plug_ffmpeg PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
		SET_TARGET_PROPERTIES(plug_ffmpeg PROPERTIES OUTPUT_NAME "ffmpeg")
		SET_TARGET_PROPERTIES(plug_ffmpeg PROPERTIES PREFIX "fteplug_")
		SET(INSTALLTARGS ${INSTALLTARGS} plug_ffmpeg)
	ELSE()
		MESSAGE(WARNING "ffmpeg library NOT available. Quake shouldn't be playing fmv anyway.")
	ENDIF()
ENDIF()

SET(FTE_PLUG_TIMIDITY false CACHE BOOL "Compile timidity audio plugin.")
IF(FTE_PLUG_TIMIDITY)
	#timidity
	FIND_PATH(TIMIDITY_INCLUDE_DIR timidity/timidity.h)
	IF(TIMIDITY_INCLUDE_DIR)
		FIND_LIBRARY(TIMIDITY_LIBRARY timidity)

		ADD_LIBRARY(plug_timidity MODULE
			plugins/plugin.c
			plugins/timidity.c
		)
		TARGET_INCLUDE_DIRECTORIES(plug_timidity PUBLIC ${TIMIDITY_INCLUDE_DIR})
		SET_TARGET_PROPERTIES(plug_timidity PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
		TARGET_LINK_LIBRARIES(plug_timidity ${SYS_LIBS} ${TIMIDITY_LIBRARY})
		SET_TARGET_PROPERTIES(plug_timidity PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
		SET_TARGET_PROPERTIES(plug_timidity PROPERTIES OUTPUT_NAME "timidity")
		SET_TARGET_PROPERTIES(plug_timidity PROPERTIES PREFIX "fteplug_")
		SET(INSTALLTARGS ${INSTALLTARGS} plug_timidity)
	ELSE()
		MESSAGE(WARNING "timidity library NOT available. We'll just stick to fake-cd music for hexen2.")
	ENDIF()
ENDIF()

#openxr plugin
SET(FTE_PLUG_OPENXR true CACHE BOOL "Compile openxr plugin (for vr support).")
IF(FTE_PLUG_OPENXR)
	FIND_PACKAGE(PkgConfig)
	IF (PKGCONFIG_FOUND)
		IF (NOT CMAKE_CROSSCOMPILING)	#its picking up the linux headers then complaining that they're missing in mingw. also almost entirely untested so no great loss.
			PKG_SEARCH_MODULE(OPENXR openxr)
		ENDIF()
		IF (OPENXR_FOUND)
			ADD_LIBRARY(plug_openxr MODULE
				plugins/plugin.c
				plugins/openxr.c
			)
			TARGET_INCLUDE_DIRECTORIES(plug_openxr PRIVATE ${OPENXR_INCLUDE_DIRS} )

			SET_TARGET_PROPERTIES(plug_openxr PROPERTIES OUTPUT_NAME "openxr")
			SET_TARGET_PROPERTIES(plug_openxr PROPERTIES PREFIX "fteplug_")
			SET_TARGET_PROPERTIES(plug_openxr PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
			IF (1)	#dynamically link
				SET_TARGET_PROPERTIES(plug_openxr PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES};${FTE_DEFINES};XR_NO_PROTOTYPES")
				TARGET_LINK_LIBRARIES(plug_openxr ${SYS_LIBS})
			ELSE()	#statically link
				SET_TARGET_PROPERTIES(plug_openxr PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES};${FTE_DEFINES}")
				TARGET_LINK_LIBRARIES(plug_openxr ${SYS_LIBS} ${OPENXR_LIBRARIES})
			ENDIF()
			SET(INSTALLTARGS ${INSTALLTARGS} plug_openxr)
		ELSE()
			MESSAGE(WARNING "openxr library NOT available. Quake is already a reality anyway.")
		ENDIF()
	ENDIF()
ENDIF()

##cef plugin
##libcef itself can be obtained from https://cef-builds.spotifycdn.com/index.html#linux64 (minimal builds, which still ends up with a 1,162,752,744 byte libcef.so - yes, actual size)
##(be sure to manually strip the binary of its debug info)
##to get this cmake stuff to recognise the headers etc:
##	cd $FTEQW-REPO && ln -s $FOO/cef_binary_$FOO+chromium-$FOO_linux64_minimal plugins/cef/cef_linux64
##(note that other systems use other subdir names)
SET(FTE_PLUG_CEF true CACHE BOOL "Compile libcef (webbrowser) plugin.")
SET(CEF_PATH ${CEF_PATH} CACHE PATH "Base location of libcef for target platform.")
IF(FTE_PLUG_CEF)
	IF(CEF_PATH MATCHES "")
		UNSET(CEF_PATH CACHE)
		IF(WIN32)
			IF(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "AMD64")
				FIND_PATH (CEF_PATH include/cef_version.h plugins/cef/cef_windows64)
			ELSEIF(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86")
				FIND_PATH (CEF_PATH include/cef_version.h plugins/cef/cef_windows32)
			ELSEIF(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "ARM64")
				FIND_PATH (CEF_PATH include/cef_version.h plugins/cef/cef_windowsarm64)
			ENDIF()
		ELSEIF("${CMAKE_SYSTEM}" MATCHES "Linux")
			IF(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
				FIND_PATH (CEF_PATH include/cef_version.h plugins/cef/cef_linux64)
			ELSEIF(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "i686")
				FIND_PATH (CEF_PATH include/cef_version.h plugins/cef/cef_linux32)
			ENDIF()
		ENDIF()
	ENDIF()
	#FIND_LIBRARY(CEF_LIBRARIES cef ${CEF_PATH}/Release)
	IF (CEF_PATH)
		##statically link only for release builds. debug builds probably don't want to have to wait for ages for the debugger to finish loading debug info unless we're actually using this stuff.
		IF(CMAKE_BUILD_TYPE MATCHES "Release")
			SET(CEF_LIBRARIES "${CMAKE_BINARY_DIR}/libcef.so")
		ENDIF()
		ADD_LIBRARY(plug_cef MODULE
			plugins/plugin.c
			plugins/cef/cef.c
		)
		TARGET_INCLUDE_DIRECTORIES(plug_cef PRIVATE ${CEF_PATH})
		SET_TARGET_PROPERTIES(plug_cef PROPERTIES BUILD_RPATH_USE_ORIGIN true)
		if (CEF_LIBRARIES)
			SET_TARGET_PROPERTIES(plug_cef PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES};${FTE_DEFINES};LIBCEF_STATIC")
			TARGET_LINK_LIBRARIES(plug_cef ${SYS_LIBS} ${CEF_LIBRARIES} ${CMAKE_DL_LIBS})
		ELSE()
			SET_TARGET_PROPERTIES(plug_cef PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES};${FTE_DEFINES};LIBCEF_DYNAMIC")
			TARGET_LINK_LIBRARIES(plug_cef ${SYS_LIBS} ${CMAKE_DL_LIBS})
		ENDIF()

		IF(NOT ${UNIX})
			ADD_CUSTOM_COMMAND(
				TARGET plug_cef PRE_LINK
				COMMAND cp ${CEF_PATH}/Release/* ${CMAKE_BINARY_DIR}
				COMMAND cp ${CEF_PATH}/Resources/* ${CMAKE_BINARY_DIR}
				)
		ELSE()
			IF(CMAKE_BUILD_TYPE MATCHES "Release")
				#sigh, cef ain't stripped properly on linux.
				ADD_CUSTOM_COMMAND(
					TARGET plug_cef PRE_LINK
					COMMAND ln -f -s ${CEF_PATH}/Release/* ${CMAKE_BINARY_DIR}
					COMMAND strip ${CMAKE_BINARY_DIR}/libcef.so -o libcef.so
					COMMAND strip ${CMAKE_BINARY_DIR}/libEGL.so -o libEGL.so
					COMMAND strip ${CMAKE_BINARY_DIR}/libGLESv2.so -o libGLESv2.so
					COMMAND strip ${CMAKE_BINARY_DIR}/chrome-sandbox -o chrome-sandbox
					COMMAND ln -f -s ${CEF_PATH}/Resources/* ${CMAKE_BINARY_DIR}
					)
			ELSE()
				ADD_CUSTOM_COMMAND(
					TARGET plug_cef PRE_LINK
					COMMAND ln -f -s ${CEF_PATH}/Release/* ${CMAKE_BINARY_DIR}
					COMMAND ln -f -s ${CEF_PATH}/Resources/* ${CMAKE_BINARY_DIR}
					)
			ENDIF()
		ENDIF()

		SET_TARGET_PROPERTIES(plug_cef PROPERTIES PREFIX "fteplug_")
		SET_TARGET_PROPERTIES(plug_cef PROPERTIES OUTPUT_NAME "cef")
		SET_TARGET_PROPERTIES(plug_cef PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	ELSE()
		MESSAGE(WARNING "libcef library NOT available. no web browser support on walls.")
	ENDIF()
ENDIF()

IF(NOT ANDROID)	#libresolv issues.
SET(FTE_PLUG_XMPP true CACHE BOOL "Compile xmpp/jabber instant-messenger plugin.")
IF(FTE_PLUG_XMPP)
	#XMPP/jabber client plugin
	ADD_LIBRARY(plug_xmpp MODULE
		plugins/plugin.c
		plugins/jabber/jabberclient.c
		plugins/jabber/xml.c
		plugins/jabber/jingle.c
		plugins/jabber/sift.c
		engine/common/sha1.c
		engine/common/sha2.c
		plugins/emailnot/md5.c
	)
	SET_TARGET_PROPERTIES(plug_xmpp PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(plug_xmpp PROPERTIES OUTPUT_NAME "xmpp")
	SET_TARGET_PROPERTIES(plug_xmpp PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(plug_xmpp PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	IF(${WIN32})
	ELSE()
		TARGET_LINK_LIBRARIES(plug_xmpp ${SYS_LIBS} resolv)
	ENDIF()
	SET(INSTALLTARGS ${INSTALLTARGS} plug_xmpp)
ENDIF()
ENDIF()	#android

INCLUDE(GNUInstallDirs)
SET(FTE_INSTALL_BINDIR games CACHE STRING "Binary dir to install to.")
INSTALL(TARGETS ${INSTALLTARGS}
	RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/${FTE_INSTALL_BINDIR}"
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
)

IF (1)
	ADD_CUSTOM_TARGET(menusys ALL
		VERBATIM
		COMMAND fteqcc -srcfile "${CMAKE_CURRENT_SOURCE_DIR}/quakec/menusys/menu.src" -o "${CMAKE_CURRENT_BINARY_DIR}/menu.dat"
		BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/menu.dat" "${CMAKE_CURRENT_BINARY_DIR}/menu.lno"
		SOURCES
			quakec/menusys/menu.src
			quakec/menusys/fteextensions.qc
			quakec/menusys/menusys/mitems.qc
			quakec/menusys/menusys/mitems_common.qc
			quakec/menusys/menusys/mitem_frame.qc
			quakec/menusys/menusys/mitem_desktop.qc
			quakec/menusys/menusys/mitem_exmenu.qc
			quakec/menusys/menusys/mitem_edittext.qc
			quakec/menusys/menusys/mitem_tabs.qc
			quakec/menusys/menusys/mitem_colours.qc
			quakec/menusys/menusys/mitem_checkbox.qc
			quakec/menusys/menusys/mitem_slider.qc
			quakec/menusys/menusys/mitem_combo.qc
			quakec/menusys/menusys/mitem_bind.qc
			quakec/menusys/menusys/mitem_spinnymodel.qc
			quakec/menusys/menu/loadsave.qc
			quakec/menusys/menu/newgame.qc
			quakec/menusys/menu/options_basic.qc
			quakec/menusys/menu/options_effects.qc
			quakec/menusys/menu/options_keys.qc
			quakec/menusys/menu/options.qc
			quakec/menusys/menu/presets.qc
			quakec/menusys/menu/servers.qc
			quakec/menusys/menu/main.qc
			quakec/menusys/menu/mods.qc
			quakec/menusys/menu/cvars.qc
			quakec/menusys/menu/updates.qc
			quakec/menusys/menu/options_audio.qc
			quakec/menusys/menu/options_configs.qc
			quakec/menusys/menu/options_hud.qc
			quakec/menusys/menu/options_particles.qc
			quakec/menusys/menu/options_video.qc
			quakec/menusys/menu/quit.qc
	)
ENDIF()