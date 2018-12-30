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

EXECUTE_PROCESS(COMMAND
	"svnversion"
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	OUTPUT_VARIABLE FTE_REVISON
	ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
)

IF (NOT "${FTE_REVISON}" STREQUAL "")
	SET(FTE_REVISON SVNREVISION=${FTE_REVISON})
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

FIND_PACKAGE(ZLIB)
IF(NOT ZLIB_FOUND)
	MESSAGE(WARNING "libz library NOT available. compressed pk3 will not be available.")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_ZLIB)
	SET(ZLIB_LIBRARY )
	SET(ZLIB_LIBRARIES )
ENDIF()

FIND_PACKAGE(BZip2)
IF(BZIP2_FOUND)
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};AVAIL_BZLIB)
	SET(FTE_LIBS ${FTE_LIBS} bz2)
	SET(FTESV_LIBS ${FTESV_LIBS} bz2)
	MESSAGE(STATUS "bzip2 library found. bz2-compressed pk3s will work for the price of extra bloat! yay!")
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
IF(NOT JPEG_FOUND)
	MESSAGE(WARNING "libjpeg library NOT available. Who cares?")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_JPEG)
ENDIF()

FIND_PACKAGE(PNG)
IF(NOT PNG_FOUND)
	MESSAGE(WARNING "libpng library NOT available. Good luck with screenshots.")
	SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_PNG)
ENDIF()
	
FIND_PACKAGE(Freetype)
IF(FREETYPE_FOUND)
	INCLUDE_DIRECTORIES( ${FREETYPE_INCLUDE_DIRS} )
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
endif()
IF(CMAKE_C_COMPILER_ID MATCHES "GNU")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-prototypes")			#
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wold-style-definition")		#k&r c is weird and can't cope with 64bit types.
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wold-style-declaration")		#
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wvla")							#msvc doesn't support vla
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdeclaration-after-statement")	#msvc doesn't allow defs after statements, and they're so very tempting...
	#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wc++-compat")	#lul
	#TODO SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes")			#for finding missing statics.
	#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")			#
ENDIF()
IF(CMAKE_BUILD_TYPE MATCHES "Debug")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89")
endif()

IF(${ANDROID})
#	FIND_PACKAGE(Freetype REQUIRED)

#	INCLUDE_DIRECTORIES(	${FREETYPE_INCLUDE_DIRS} )

	SET(FTE_DEFINES ${FTE_DEFINES};ANDROID;VKQUAKE;DYNAMIC_LIBPNG;DYNAMIC_LIBJPEG;MULTITHREAD;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTE_LIBS ${FTE_LIBS} android log EGL ${ZLIB_LIBRARIES} m ${CMAKE_DL_LIBS})
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
	
	SET(FTE_LIBS ${FTE_LIBS} ${ZLIB_LIBRARIES} ole32 gdi32 wsock32 winmm dxguid)
	SET(FTE_DEFINES ${FTE_DEFINES};D3D9QUAKE;D3D11QUAKE;DYNAMIC_LIBPNG;DYNAMIC_LIBJPEG)
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
	
	SET(FTESV_LIBS ${FTESV_LIBS} ${ZLIB_LIBRARIES} wsock32 winmm)
	SET(FTESV_ARCH_FILES 
		engine/client/winquake.rc
		engine/common/sys_win_threads.c
		engine/common/net_ssl_winsspi.c
		engine/common/fs_win32.c
		engine/server/sv_sys_win.c
	)
ELSEIF(${UNIX})	#linux(ish)
				#openbsd will have issues with snd_linux.c

	#linux-only packages
	FIND_PACKAGE(GnuTLS)
	IF(NOT GNUTLS_FOUND)
		MESSAGE(WARNING "gnutls library NOT available. HTTPS/DTLS will not be available.")
		SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_GNUTLS)
	ENDIF()

	FIND_PACKAGE(ALSA)
	IF(NOT ALSA_FOUND)
		MESSAGE(WARNING "asound (alsa) library NOT available.")
		SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_ALSA)
	ENDIF()

	FIND_PACKAGE(X11)
	IF(X11_FOUND)
		IF (NOT X11_Xcursor_FOUND)
			SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_X11_CURSOR)
			MESSAGE(WARNING "Xcursor library NOT available.")
		ENDIF()
	ELSE()
		MESSAGE(WARNING "x11 library NOT available.")
		SET(FTE_LIB_DEFINES ${FTE_LIB_DEFINES};NO_X11)
	ENDIF()

	SET(FTE_DEFINES ${FTE_DEFINES};DYNAMIC_LIBPNG;DYNAMIC_LIBJPEG;DYNAMIC_SDL;MULTITHREAD;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTE_LIBS ${FTE_LIBS} ${ZLIB_LIBRARIES} m ${CMAKE_DL_LIBS} pthread ${SDL2_LIBRARIES})
	SET(FTE_ARCH_FILES
		engine/client/sys_linux.c
		engine/common/sys_linux_threads.c
		engine/common/net_ssl_gnutls.c
#		engine/common/net_ssl_openssl.c

		engine/client/snd_al.c
		engine/client/snd_alsa.c
		engine/client/snd_linux.c
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

	#on linux, use wayland.
	FIND_LIBRARY(
		WAYLAND_CLIENT_LIBRARY
		NAMES wayland-client libwayland-client
	)
	IF(WAYLAND_CLIENT_LIBRARY)
		SET(FTE_DEFINES ${FTE_DEFINES};WAYLANDQUAKE;USE_EGL)
		SET(FTE_ARCH_FILES ${FTE_ARCH_FILES}
			engine/gl/gl_vidwayland.c
		)
	ELSE()
		MESSAGE(WARNING "Wayland library NOT available")
		IF(NOT X11_FOUND)
			MESSAGE(WARNING "No renderers supported!")
			SET(FTE_NO_RENDERERS 1)
		ENDIF()
	ENDIF()

	SET(FTESV_DEFINES MULTITHREAD;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTESV_ARCH_FILES 
		engine/server/sv_sys_unix.c
		engine/common/sys_linux_threads.c
		engine/common/net_ssl_gnutls.c
#		engine/common/net_ssl_openssl.c
	)
	SET(FTESV_LIBS ${FTESV_LIBS} ${ZLIB_LIBRARIES} m ${CMAKE_DL_LIBS} pthread)

#	SET(FTE_DEFINES ${FTE_DEFINES};HAVE_OPENSSL)
#	SET(FTESV_DEFINES ${FTESV_DEFINES};HAVE_OPENSSL)
#	SET(FTE_LIBS ${FTE_LIBS} ssl crypto)
#	SET(FTESV_LIBS ${FTE_LIBS} ssl crypto)
ELSEIF(1)	#SDL
#	FIND_PACKAGE(Freetype REQUIRED)
#	INCLUDE_DIRECTORIES(engine/libs engine/libs/freetype2/include)

	FIND_PACKAGE(PkgConfig REQUIRED)
	PKG_SEARCH_MODULE(sdl2 REQUIRED sdl2)

#	FIND_PACKAGE(SDL2 REQUIRED)

	INCLUDE_DIRECTORIES(${FREETYPE_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
	
	#SDL2.0.7 supports vulkan, so lets use it.
	SET(FTE_DEFINES ${FTE_DEFINES};FTE_SDL;DYNAMIC_LIBPNG;DYNAMIC_LIBJPEG;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTE_LIBS ${FTE_LIBS} ${ZLIB_LIBRARIES} m ${CMAKE_DL_LIBS} ${SDL2_LIBRARIES})
	SET(FTE_ARCH_FILES
		engine/client/sys_sdl.c
		engine/client/snd_al.c
		engine/client/snd_sdl.c
		engine/client/in_sdl.c
		engine/client/cd_sdl.c
		engine/gl/gl_vidsdl.c
	)

	SET(FTESV_DEFINES FTE_SDL;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTESV_LIBS ${FTESV_LIBS} ${ZLIB_LIBRARIES} m ${CMAKE_DL_LIBS} ${SDL2_LIBRARIES})

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

SET(FTE_COMMON_FILES
	#these files are common to both server-only and client+server builds.
	engine/common/cmd.c
	engine/common/com_mesh.c
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
	engine/common/translate.c
	engine/common/zone.c

	engine/common/config_fteqw.h
	engine/common/config_minimal.h
	engine/common/config_nocompat.h
	engine/common/config_wastes.h
	
	#sigh
	engine/client/pr_skelobj.c
	engine/client/m_download.c
	engine/client/net_master.c
	
	#these are here because of hitmodel etc
	engine/gl/gl_heightmap.c
	engine/gl/gl_hlmdl.c
	engine/gl/gl_model.c
	
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
	engine/server/sv_move.c
	engine/server/sv_mvd.c
	engine/server/sv_nchan.c
	engine/server/sv_phys.c
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
	engine/server/world.c
	
	engine/qclib/comprout.c
	engine/qclib/hash.c
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
	
	engine/http/httpclient.c
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
	engine/client/vid_headless.c
	engine/client/view.c
	engine/client/wad.c
	engine/client/zqtp.c


#These are generic renderer files
	engine/gl/gl_alias.c
	engine/gl/gl_font.c
	engine/gl/gl_ngraph.c
	engine/gl/gl_rlight.c
	engine/gl/gl_shader.c
	engine/gl/gl_shadow.c
	engine/gl/gl_warp.c
	engine/gl/ltface.c

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
	
	engine/vk/vk_backend.c
	engine/vk/vk_init.c
)

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
	FIND_PACKAGE(Bullet)
	IF (BULLET_FOUND)
		ADD_LIBRARY(bullet MODULE
			plugins/qvm_api.c
			plugins/plugin.c
			plugins/bullet/bulletplug.cpp
		)
		TARGET_INCLUDE_DIRECTORIES(bullet PUBLIC ${BULLET_INCLUDE_DIRS})
		SET_TARGET_PROPERTIES(bullet PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN")
		SET_TARGET_PROPERTIES(bullet PROPERTIES PREFIX "fteplug_")
		SET_TARGET_PROPERTIES(bullet PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
		TARGET_LINK_LIBRARIES(bullet m ${BULLET_LIBRARIES})
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
ELSE()
	#systems that actually have executables...
	ADD_EXECUTABLE(fteqw WIN32
		${FTE_ARCH_FILES}
		${FTE_COMMON_FILES}
		${FTE_CLIENT_FILES}
	)
	SET_TARGET_PROPERTIES(fteqw PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES};${FTE_DEFINES};${FTE_REVISON}")
	TARGET_LINK_LIBRARIES(fteqw ${FTE_LIBS} )

	ADD_EXECUTABLE(fteqw-sv
		${FTESV_ARCH_FILES}
		${FTE_COMMON_FILES}
	)
	SET_TARGET_PROPERTIES(fteqw-sv PROPERTIES COMPILE_DEFINITIONS "SERVERONLY;${FTE_LIB_DEFINES};${FTESV_DEFINES};${FTE_REVISON}")
	TARGET_LINK_LIBRARIES(fteqw-sv ${FTESV_LIBS})

	ADD_EXECUTABLE(iqmtool
		iqm/iqm.cpp
		iqm/iqm.h
	)
	SET_TARGET_PROPERTIES(iqmtool PROPERTIES COMPILE_DEFINITIONS "${FTE_REVISON}")

	IF(1)
		ADD_EXECUTABLE(ftemaster
			${FTESV_ARCH_FILES}
			engine/server/sv_master.c
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
	ENDIF()

	ADD_EXECUTABLE(httpserver
		engine/common/fs_stdio.c
		engine/http/httpserver.c
		engine/http/iwebiface.c
		engine/http/ftpserver.c
	)
	SET_TARGET_PROPERTIES(httpserver PROPERTIES COMPILE_DEFINITIONS "WEBSERVER;WEBSVONLY;${FTE_REVISON};stricmp=strcasecmp;strnicmp=strncasecmp")
	IF(WIN32)
		TARGET_LINK_LIBRARIES(httpserver ${ZLIB_LIBRARIES} ws2_32)
	ENDIF()

	ADD_EXECUTABLE(fteqcc
		engine/qclib/qcctui.c
		engine/qclib/comprout.c
		engine/qclib/hash.c
		engine/qclib/qcc_cmdlib.c
		engine/qclib/qcc_pr_comp.c
		engine/qclib/qcc_pr_lex.c
		engine/qclib/qccmain.c
		engine/qclib/qcd_main.c
	)
	SET_TARGET_PROPERTIES(fteqcc PROPERTIES COMPILE_DEFINITIONS "${FTE_LIB_DEFINES};${FTE_REVISON}")
	TARGET_LINK_LIBRARIES(fteqcc ${ZLIB_LIBRARIES} m)


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
		TARGET_LINK_LIBRARIES(fteqccgui ${ZLIB_LIBRARIES} shlwapi ole32 comctl32 comdlg32)
	ENDIF()
ENDIF()


#Quake Injector Alike plugin
ADD_LIBRARY(qi MODULE
	plugins/qvm_api.c
	plugins/plugin.c
	plugins/qi/qi.c
	plugins/emailnot/md5.c
	plugins/jabber/xml.c
)
SET_TARGET_PROPERTIES(qi PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
SET_TARGET_PROPERTIES(qi PROPERTIES PREFIX "fteplug_")
SET_TARGET_PROPERTIES(qi PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
TARGET_LINK_LIBRARIES(qi m)

#ODE Physics library plugin
FIND_PATH(LIBODE_INCLUDE_DIR ode/ode.h)
FIND_LIBRARY(LIBODE_LIBRARY ode)
IF (LIBODE_INCLUDE_DIR)
	ADD_LIBRARY(ode MODULE
		plugins/qvm_api.c
		plugins/plugin.c
		engine/common/com_phys_ode.c
		engine/common/mathlib.c
	)
	TARGET_INCLUDE_DIRECTORIES(ode PUBLIC ${ODE_INCLUDE_DIRS})
	SET_TARGET_PROPERTIES(ode PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;ODE_STATIC")
	SET_TARGET_PROPERTIES(ode PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(ode PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(ode m ${LIBODE_LIBRARY})
ENDIF()

#EzQuake Hud port plugin
ADD_LIBRARY(ezhud MODULE
	plugins/qvm_api.c
	plugins/plugin.c
	plugins/ezhud/ezquakeisms.c
	plugins/ezhud/hud.c
	plugins/ezhud/hud_common.c
	plugins/ezhud/hud_editor.c
)
SET_TARGET_PROPERTIES(ezhud PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
SET_TARGET_PROPERTIES(ezhud PROPERTIES PREFIX "fteplug_")
SET_TARGET_PROPERTIES(ezhud PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
TARGET_LINK_LIBRARIES(ezhud m)

#IRC client plugin
ADD_LIBRARY(irc MODULE
	plugins/qvm_api.c
	plugins/plugin.c
	plugins/irc/ircclient.c
)
SET_TARGET_PROPERTIES(irc PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
SET_TARGET_PROPERTIES(irc PROPERTIES PREFIX "fteplug_")
SET_TARGET_PROPERTIES(irc PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
TARGET_LINK_LIBRARIES(irc m)

#ffmpeg client plugin. no proper way to detect dependancies right now, so I've gotta try the manual way.
FIND_PATH(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
FIND_PATH(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
FIND_PATH(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
FIND_PATH(AVSWSCALE_INCLUDE_DIR libswscale/swscale.h)
IF(AVFORMAT_INCLUDE_DIR)
	FIND_LIBRARY(AVCODEC_LIBRARY avcodec)
	FIND_LIBRARY(AVFORMAT_LIBRARY avformat)
	FIND_LIBRARY(AVUTIL_LIBRARY avutil)
	FIND_LIBRARY(AVSWSCALE_LIBRARY swscale)

	ADD_LIBRARY(ffmpeg MODULE
		plugins/qvm_api.c
		plugins/plugin.c
		plugins/avplug/avaudio.c
		plugins/avplug/avdecode.c
		plugins/avplug/avencode.c
	)
	TARGET_INCLUDE_DIRECTORIES(ffmpeg PUBLIC ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVSWSCALE_INCLUDE_DIR})
	SET_TARGET_PROPERTIES(ffmpeg PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	TARGET_LINK_LIBRARIES(ffmpeg m ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} ${AVSWSCALE_LIBRARY})
	SET_TARGET_PROPERTIES(ffmpeg PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(ffmpeg PROPERTIES PREFIX "fteplug_")
ELSE()
	MESSAGE(WARNING "ffmpeg library NOT available. Quake shouldn't be playing fmv anyway.")
ENDIF()

IF(NOT ANDROID)
	#XMPP/jabber client plugin
	ADD_LIBRARY(xmpp MODULE
		plugins/qvm_api.c
		plugins/plugin.c
		plugins/jabber/jabberclient.c
		plugins/jabber/xml.c
		plugins/jabber/jingle.c
		plugins/jabber/sift.c
		engine/common/sha1.c
		plugins/emailnot/md5.c
	)
	SET_TARGET_PROPERTIES(xmpp PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;${FTE_LIB_DEFINES}")
	SET_TARGET_PROPERTIES(xmpp PROPERTIES PREFIX "fteplug_")
	SET_TARGET_PROPERTIES(xmpp PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
	IF(${WIN32})
	ELSE()
		TARGET_LINK_LIBRARIES(xmpp m resolv)
	ENDIF()
ENDIF()

#cef plugin
