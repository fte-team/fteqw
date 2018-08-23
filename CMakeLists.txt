#Note: this file was made primarily to support msvc and its project file incompatibilities nightmare.
#Its also useful for various other IDEs like QtCreator etc.
#It uses system libraries, so it will have dependancy issues with public releases where those dependancies are distro/version-specific.
#Public builds are still built using the (overcomplicated) traditional (g)makefile.

IF(${ANDROID})
    CMAKE_MINIMUM_REQUIRED(VERSION 3.6) #special dispensation for android crap.
ELSE()
    CMAKE_MINIMUM_REQUIRED(VERSION 3.9)
ENDIF()
PROJECT(fteqw)

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
    #android studio sucks and insists on using an outdated version of cmake instead of the system version.
ELSE()
    IF(NOT ${CMAKE_BUILD_TYPE} MATCHES "Debug")
        #use LTO where possible. reportedly requires cmake 3.9 to actually work
        INCLUDE(CheckIPOSupported)
        check_ipo_supported(RESULT result)
        IF(result)
            SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        ENDIF()
    ENDIF()
ENDIF()

if (CMAKE_C_COMPILER_ID MATCHES "Clang")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-sign")
endif()

IF(${ANDROID})
#	FIND_PACKAGE(Freetype REQUIRED)

#	INCLUDE_DIRECTORIES(	${FREETYPE_INCLUDE_DIRS} )

	SET(FTE_DEFINES ANDROID;GLQUAKE;VKQUAKE;DYNAMIC_LIBPNG;DYNAMIC_LIBJPEG;MULTITHREAD;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTE_LIBS android log EGL z m ${CMAKE_DL_LIBS})
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
	
	SET(FTE_LIBS z ole32 gdi32 wsock32 winmm)
	SET(FTE_DEFINES GLQUAKE;VKQUAKE;D3D9QUAKE;DYNAMIC_LIBPNG;DYNAMIC_LIBJPEG) #D3D11QUAKE not included.
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
	
	SET(FTESV_LIBS z wsock32 winmm)
	SET(FTESV_ARCH_FILES 
		engine/client/winquake.rc
		engine/common/sys_win_threads.c
		engine/common/net_ssl_winsspi.c
		engine/common/fs_win32.c
		engine/server/sv_sys_win.c
	)
ELSEIF(${UNIX})	#linux(ish)
				#openbsd will have issues with snd_linux.c
	FIND_PACKAGE(Freetype REQUIRED)

	INCLUDE_DIRECTORIES(	${FREETYPE_INCLUDE_DIRS} )

	SET(FTE_DEFINES GLQUAKE;VKQUAKE;DYNAMIC_LIBPNG;DYNAMIC_LIBJPEG;DYNAMIC_SDL;MULTITHREAD;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTE_LIBS z m ${CMAKE_DL_LIBS} pthread ${SDL2_LIBRARIES})
	SET(FTE_ARCH_FILES
		engine/client/sys_linux.c
		engine/common/sys_linux_threads.c
		engine/common/net_ssl_gnutls.c

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
	ENDIF()

	SET(FTESV_DEFINES MULTITHREAD;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTESV_ARCH_FILES 
		engine/server/sv_sys_unix.c
		engine/common/sys_linux_threads.c
		engine/common/net_ssl_gnutls.c
	)
	SET(FTESV_LIBS z m ${CMAKE_DL_LIBS} pthread)
ELSEIF(1)	#SDL
	FIND_PACKAGE(Freetype REQUIRED)
#	INCLUDE_DIRECTORIES(engine/libs engine/libs/freetype2/include)

	FIND_PACKAGE(PkgConfig REQUIRED)
	PKG_SEARCH_MODULE(sdl2 REQUIRED sdl2)

#	FIND_PACKAGE(SDL2 REQUIRED)

	INCLUDE_DIRECTORIES(${FREETYPE_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
	
	#SDL2.0.7 supports vulkan, so lets use it.
	SET(FTE_DEFINES GLQUAKE;VKQUAKE;FTE_SDL;DYNAMIC_LIBPNG;DYNAMIC_LIBJPEG;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTE_LIBS z m ${CMAKE_DL_LIBS} ${SDL2_LIBRARIES})
	SET(FTE_ARCH_FILES
		engine/client/sys_sdl.c
		engine/client/snd_al.c
		engine/client/snd_sdl.c
		engine/client/in_sdl.c
		engine/client/cd_sdl.c
		engine/gl/gl_vidsdl.c
	)

	SET(FTESV_DEFINES FTE_SDL;stricmp=strcasecmp;strnicmp=strncasecmp)
	SET(FTESV_LIBS z m ${CMAKE_DL_LIBS} ${SDL2_LIBRARIES})

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

#These are GL-specific
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

IF(${ANDROID})
    #android sucks. everything is a library. so we build the engine as a shared library and completely ignore dedicated servers+tools

    ADD_LIBRARY(ftedroid MODULE
        ${FTE_ARCH_FILES}
        ${FTE_COMMON_FILES}
        ${FTE_CLIENT_FILES}
    )
    SET_TARGET_PROPERTIES(ftedroid PROPERTIES COMPILE_DEFINITIONS "${FTE_DEFINES};${FTE_REVISON}")
    TARGET_LINK_LIBRARIES(ftedroid ${FTE_LIBS} )

ELSE()
    #systems that actually have executables...
    ADD_EXECUTABLE(fteqw WIN32
        ${FTE_ARCH_FILES}
        ${FTE_COMMON_FILES}
        ${FTE_CLIENT_FILES}
    )

    SET_TARGET_PROPERTIES(fteqw PROPERTIES COMPILE_DEFINITIONS "${FTE_DEFINES};${FTE_REVISON}")
    TARGET_LINK_LIBRARIES(fteqw ${FTE_LIBS} )

    ADD_EXECUTABLE(fteqw-sv
        ${FTESV_ARCH_FILES}
        ${FTE_COMMON_FILES}
    )
    SET_TARGET_PROPERTIES(fteqw-sv PROPERTIES COMPILE_DEFINITIONS "SERVERONLY;${FTESV_DEFINES};${FTE_REVISON}")
    TARGET_LINK_LIBRARIES(fteqw-sv ${FTESV_LIBS})


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
    TARGET_LINK_LIBRARIES(fteqcc z m)


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
        TARGET_LINK_LIBRARIES(fteqccgui z shlwapi ole32 comctl32 comdlg32)
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
SET_TARGET_PROPERTIES(qi PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN")
SET_TARGET_PROPERTIES(qi PROPERTIES PREFIX "fteplug_")

#Bullet Physics library plugin
FIND_PACKAGE(Bullet)
IF (${BULLET_FOUND})
	ADD_LIBRARY(bullet MODULE
		plugins/qvm_api.c
		plugins/plugin.c
		plugins/bullet/bulletplug.cpp
	)
	TARGET_INCLUDE_DIRECTORIES(bullet PUBLIC ${BULLET_INCLUDE_DIRS})
	SET_TARGET_PROPERTIES(bullet PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN")
	SET_TARGET_PROPERTIES(bullet PROPERTIES PREFIX "fteplug_")
ENDIF()

#Bullet Physics library plugin
#FIND_PACKAGE(ode REQUIRED)
ADD_LIBRARY(ode MODULE
	plugins/qvm_api.c
	plugins/plugin.c
	engine/common/com_phys_ode.c
)
TARGET_INCLUDE_DIRECTORIES(ode PUBLIC ${BULLET_INCLUDE_DIRS})
SET_TARGET_PROPERTIES(ode PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN;ODE_STATIC")
SET_TARGET_PROPERTIES(ode PROPERTIES PREFIX "fteplug_")

#EzQuake Hud port plugin
ADD_LIBRARY(ezhud MODULE
	plugins/qvm_api.c
	plugins/plugin.c
	plugins/ezhud/ezquakeisms.c
	plugins/ezhud/hud.c
	plugins/ezhud/hud_common.c
	plugins/ezhud/hud_editor.c
)
SET_TARGET_PROPERTIES(ezhud PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN")
SET_TARGET_PROPERTIES(ezhud PROPERTIES PREFIX "fteplug_")

#IRC client plugin
ADD_LIBRARY(irc MODULE
	plugins/qvm_api.c
	plugins/plugin.c
	plugins/irc/ircclient.c
)
SET_TARGET_PROPERTIES(irc PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN")
SET_TARGET_PROPERTIES(irc PROPERTIES PREFIX "fteplug_")

IF(NOT ${ANDROID})
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
    SET_TARGET_PROPERTIES(xmpp PROPERTIES COMPILE_DEFINITIONS "FTEPLUGIN")
    SET_TARGET_PROPERTIES(xmpp PROPERTIES PREFIX "fteplug_")
ENDIF()

#ffmpeg plugin
#cef plugin
