/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __BOTHDEFS_H
#define __BOTHDEFS_H

// release version
#define FTE_VER_MAJOR 1
#define FTE_VER_MINOR 6

#if defined(__APPLE__) && defined(__MACH__)
	#define MACOSX
#endif

#if defined(__MINGW32_VERSION) || defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
	#define MINGW
#endif
#if !defined(MINGW) && defined(__GNUC__) && defined(_WIN32)
	#define MINGW	//Erm, why is this happening?
#endif

#ifdef ANDROID
	#define NO_PNG
	#define NO_JPEG
	#define NO_OGG
#endif

#ifdef _XBOX
	#define NO_PNG
	#define NO_JPEG
	#define NO_OGG
	#define NO_ZLIB
	#define NOMEDIA
	#define NO_FREETYPE
	#define HAVE_PACKET
#endif

#ifdef NACL
	#define NO_PNG
	#define NO_JPEG
	#define NO_OGG
	#define NO_ZLIB
#endif

#ifndef MULTITHREAD
	#if !defined(_WIN32) || defined(FTE_SDL) //win32 is annoying
		#define NO_MULTITHREAD
	#endif
#endif

#ifdef FTE_TARGET_WEB
	//no Sys_LoadLibrary support, so we might as well kill this stuff off.
	#define NO_PNG
	#define NO_JPEG
	#define NO_OGG
	#define NO_ZLIB
	#define NO_FREETYPE
#endif

#ifdef D3DQUAKE
	#define D3D9QUAKE
	//#define D3D11QUAKE
	#undef D3DQUAKE
#endif

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)

#ifndef CONFIG_FILE_NAME
	#ifdef HAVE_CONFIG_H
		#define CONFIG_FILE_NAME config.h
	#elif defined(NOLEGACY)
		#define CONFIG_FILE_NAME config_nolegacy.h
	#elif defined(MINIMAL)
		#define CONFIG_FILE_NAME config_minimal.h
	#else
		#define CONFIG_FILE_NAME config_fteqw.h
	#endif
#endif

#ifdef CONFIG_FILE_NAME
	#undef MULTITHREAD
	#undef BOTLIB_STATIC
	#define HEADLESSQUAKE	//usable renderers are normally specified via the makefile, but HEADLESS is considered a feature rather than an actual renderer, so usually gets forgotten about...

	//yup, C89 allows this (doesn't like C's token concat though).
	#include STRINGIFY(CONFIG_FILE_NAME)
#else
	#define QWSKINS	//disables qw .pcx skins, as well as enemy/team colour forcing.

	#ifndef NO_LIBRARIES
		#define AVAIL_OPENAL
		#define AVAIL_FREETYPE
	#endif

	#define AVAIL_OGGVORBIS
	#if defined(__CYGWIN__)
		#define AVAIL_ZLIB
	#else
		#define AVAIL_PNGLIB
		#define AVAIL_JPEGLIB
		#define AVAIL_ZLIB
		#define AVAIL_OGGVORBIS
	#endif

	#ifdef WINRT
		#define AVAIL_XAUDIO2
		#define AVAIL_WASAPI
	#elif !defined(NO_DIRECTX) && !defined(NODIRECTX) && defined(_WIN32)
		#define AVAIL_DINPUT
		#define AVAIL_DSOUND
		#define AVAIL_WASAPI
		//#define AVAIL_XAUDIO2 //gcc doesn't provide any headers
	#endif
	#define AVAIL_XZDEC

	#if !defined(MINIMAL) && !defined(NPFTE) && !defined(NPQTV)
		#if defined(_WIN32) && !defined(FTE_SDL) && !defined(WINRT) && !defined(_XBOX)
			#if !defined(_MSC_VER) || _MSC_VER > 1200
				#define HAVE_WINSSPI	//built in component, checks against windows' root ca database and revocations etc.
			#endif
		#elif (defined(__linux__) || defined(__CYGWIN__)) && !defined(ANDROID)
			#define HAVE_GNUTLS		//currently disabled as it does not validate the server's certificate, beware the mitm attack.
		#endif
	#endif

	//#define DYNAMIC_ZLIB
	//#define DYNAMIC_LIBPNG
	//#define DYNAMIC_LIBJPEG
	//#define LIBVORBISFILE_STATIC
	//#define SPEEX_STATIC

	#if defined(_WIN32) && defined(GLQUAKE)
		//#define USE_EGL
	#endif

	#if defined(_MSC_VER) && !defined(BOTLIB_STATIC) //too lazy to fix up the makefile
		#define BOTLIB_STATIC
	#endif

	#if (defined(_MSC_VER) && (_MSC_VER < 1500)) || defined(FTE_SDL)
		#undef AVAIL_WASAPI	//wasapi is available in the vista sdk, while that's compatible with earlier versions, its not really expected until 2008
	#endif

	#define HAVE_TCP		//says we can use tcp too (either ipv4 or ipv6)
	#define HAVE_PACKET		//if we have the socket api at all...
	#define HAVE_MIXER	//can be disabled if you have eg openal instead.

//set any additional defines or libs in win32
	#define LOADERTHREAD

	#define PACKAGE_Q1PAK
	#define PACKAGE_PK3
	#define AVAIL_GZDEC
	#define PACKAGE_TEXWAD	//quake's image wad support

	#ifdef GLQUAKE
		#define HEADLESSQUAKE
	#endif
	#define AVAIL_MP3_ACM	//microsoft's Audio Compression Manager api

	#ifdef NOLEGACY
		//these are only the features that really make sense in a more modern engine
		#define QUAKETC			//skip some legacy stuff
		#define SPRMODELS		//quake1 sprite models
		#define INTERQUAKEMODELS
		#define RTLIGHTS		//realtime lighting
		#define Q1BSPS			//quake 1 bsp support, because we're still a quake engine
		#define Q2BSPS			//quake 2 bsp support (a dependancy of q3bsp)
		#define Q3BSPS			//quake 3 bsp support
//		#define TERRAIN			//heightmap support
		#define WEBCLIENT		//http/ftp clients.
		#define IMAGEFMT_DDS	//a sort of image file format.
		#define PSET_SCRIPT
//		#define PLUGINS			//qvm/dll plugins.
//		#define SUPPORT_ICE		//Interactive Connectivity Establishment protocol, for peer-to-peer connections
		#define CSQC_DAT		//support for csqc
//		#define VOICECHAT

		#undef AVAIL_JPEGLIB
		#undef AVAIL_XZDEC

	#elif defined(MINIMAL)
		#define QUAKESTATS
		#define QUAKEHUD
		#define CL_MASTER		//this is useful

		#undef AVAIL_JPEGLIB	//no jpeg support
		#undef AVAIL_PNGLIB		//no png support
		#undef AVAIL_OPENAL		//just bloat...
		#undef AVAIL_GZDEC

		#define Q1BSPS
		#define SPRMODELS		//quake1 sprite models
		#define MD1MODELS		//quake ain't much use without this
		#define MD3MODELS		//we DO want to use quake3 alias models. This might be a minimal build, but we still want this.
		#define PLUGINS
		#define NOQCDESCRIPTIONS 2	//trim space from no fteextensions.qc info

		#define PSET_CLASSIC

		//#define CSQC_DAT	//support for csqc

		#ifndef SERVERONLY	//don't be stupid, stupid.
			#ifndef CLIENTONLY
				#define CLIENTONLY
			#endif
		#endif
	#else
		#define NETPREPARSE
		#define QUAKESTATS
		#define QUAKEHUD
		#define SVRANKING
		#define USE_SQLITE
		#ifdef SERVERONLY
//			#define USE_MYSQL	//allow mysql in dedicated servers.
		#endif
		#if defined(_WIN32) && !defined(FTE_SDL) && !defined(WINRT)
			#define SUBSERVERS	//use subserver code.
		#elif defined(__linux__) && !defined(ANDROID) && !defined(FTE_SDL)
			#define SUBSERVERS	//use subserver code.
		#endif

		#define SIDEVIEWS	4	//enable secondary/reverse views.

//		#define DSPMODELS		//doom sprites (only needs PACKAGE_DOOMWAD to generate the right wad file names)
		#define SPRMODELS		//quake1 sprite models
		#define SP2MODELS		//quake2 sprite models
		#define MD1MODELS		//quake1 alias models
		#define MD2MODELS		//quake2 alias models
		#define MD3MODELS		//quake3 alias models
		#define MD5MODELS		//doom3 models
		#define ZYMOTICMODELS	//zymotic skeletal models.
		#define DPMMODELS		//darkplaces model format (which I've never seen anyone use)
//		#define PSKMODELS		//PSK model format (ActorX stuff from UT, though not the format the game itself uses)
		#define HALFLIFEMODELS	//halflife model support (experimental)
		#define INTERQUAKEMODELS
		#define RAGDOLL

		#define USEAREAGRID		//world collision optimisation. REQUIRED for performance with xonotic. hopefully it helps a few other mods too.
		#define HUFFNETWORK		//huffman network compression
//		#define PACKAGE_DOOMWAD	//doom wad support (maps+sprites are separate)
//		#define MAP_DOOM		//doom map support
//		#define MAP_PROC		//doom3/quake4 map support
		//#define WOLF3DSUPPORT	//wolfenstein3d map support (not started yet)
		#define Q1BSPS			//quake 1 bsp support, because we're still a quake engine
		#define Q2BSPS			//quake 2 bsp support
		#define Q3BSPS			//quake 3 bsp support
		#define RFBSPS			//rogue(sof+jk2o)+qfusion bsp support
		#define TERRAIN			//heightmap support
//		#define SV_MASTER		//starts up a master server
		#define SVCHAT			//serverside npc chatting. see sv_chat.c
		#define Q2SERVER		//server can run a q2 game dll and switches to q2 network and everything else.
		#define Q2CLIENT		//client can connect to q2 servers
		#define Q3CLIENT
		#define Q3SERVER
		#define HEXEN2			//mostly server only, but also includes some hud+menu stuff, and effects
//		#define HLCLIENT 7		//we can run HL gamecode (not protocol compatible, set to 6 or 7)
//		#define HLSERVER 140	//we can run HL gamecode (not protocol compatible, set to 138 or 140)
		#define NQPROT			//server and client are capable of using quake1/netquake protocols. (qw is still prefered. uses the command 'nqconnect')
		#define PACKAGE_DZIP	//support for the dzip format, common with the speed-demos-archive site
//		#define WEBSERVER		//http server
		#define FTPSERVER		//ftp server
		#define WEBCLIENT		//http clients.
		#define RUNTIMELIGHTING	//calculate lit/lux files the first time the map is loaded and doesn't have a loadable lit.
//		#define QTERM			//qterm... adds a console command that allows running programs from within quake - bit like xterm.
		#define CL_MASTER		//query master servers and stuff for a dynamic server listing.
		#define R_XFLIP			//allow view to be flipped horizontally
		#define TEXTEDITOR
		#define IMAGEFMT_KTX	//Khronos TeXture. common on gles3 devices for etc2 compression
		#define IMAGEFMT_PKM	//file format generally written by etcpack or android's etc1tool
		#define IMAGEFMT_DDS	//a sort of image file format.
		#define IMAGEFMT_BLP	//a sort of image file format.
		#define DECOMPRESS_ETC2	//decompress etc2(core in gles3/gl4.3) if the graphics driver doesn't support it (eg d3d or crappy gpus with vulkan).
//		#define DECOMPRESS_S3TC	//allows bc1-3 to work even when drivers don't support it. This is probably only an issue on mobile chips. WARNING: not entirely sure if all patents expired yet...
		#define DECOMPRESS_RGTC	//bc4+bc5
		//would be nice to have BPTC decompression too, for gl<4.2, d3d9, or d3d11_level10, but frankly its overcomplicated. I'm not going to bother with ASTC either.
		#ifndef RTLIGHTS
				#define RTLIGHTS		//realtime lighting
		#endif
		//#define SHADOWDBG_COLOURNOTDEPTH	//for debugging. renders shadowmaps to a colour buffer instead of a depth buffer. resulting in projected textures instead of actual shadows (the glsl only picks up the red component, but whatever)

//		#define QWOVERQ3		//allows qw servers with q3 clients. requires specific cgame.

		#define VM_Q1			//q1 qvm gamecode interface
		//#define	VM_LUA			//q1 lua gamecode interface

		#define TCPCONNECT		//a tcpconnect command, that allows the player to connect to tcp-encapsulated qw protocols.
//		#define IRCCONNECT		//an ircconnect command, that allows the player to connect to irc-encapsulated qw protocols... yeah, really.

		#define PLUGINS			//qvm/dll plugins.
		#define SUPPORT_ICE		//Interactive Connectivity Establishment protocol, for peer-to-peer connections

		#define CSQC_DAT	//support for csqc
		#define MENU_DAT	//support for menu.dat

		#define PSET_SCRIPT
		#define PSET_CLASSIC


		#define HAVE_CDPLAYER	//includes cd playback. actual cds. faketracks are supported regardless.
		#define HAVE_JUKEBOX	//includes built-in jukebox crap
		#define HAVE_MEDIA_DECODER	//can play cin/roq, more with plugins
		#define HAVE_MEDIA_ENCODER	//capture/capturedemo work.
		#define HAVE_SPEECHTOTEXT	//windows speech-to-text thing

		#define VOICECHAT

#if defined(_WIN32) && !defined(FTE_SDL) && !defined(MULTITHREAD) //always thread on win32 non-minimal builds
		#define MULTITHREAD
#endif
	#endif



	#ifdef QUAKETC
		#define NOBUILTINMENUS	//kill engine menus (should be replaced with ewither csqc or menuqc)
		#undef Q2CLIENT	//not useful
		#undef Q2SERVER	//not useful
		#undef Q3CLIENT	//not useful
		#undef Q3SERVER	//not useful
		#undef HLCLIENT	//not useful
		#undef HLSERVER	//not useful
		#undef VM_Q1	//not useful
		#undef VM_LUA	//not useful
		#undef HALFLIFEMODELS	//yuck
		#undef RUNTIMELIGHTING	//presumably not useful
		#undef HEXEN2
	#endif

#endif


	#ifndef MSVCLIBSPATH
	#ifdef MSVCLIBPATH
		#define MSVCLIBSPATH STRINGIFY(MSVCLIBPATH)
	#elif _MSC_VER == 1200
		#define MSVCLIBSPATH "../libs/vc6-libs/"
	#else
		#define MSVCLIBSPATH "../libs/"
	#endif
	#endif

#if defined(SERVERONLY) && defined(CLIENTONLY)
	#undef CLIENTONLY	//impossible build. assume the config had CLIENTONLY and they tried building a dedicated server
#endif

//software rendering is just too glitchy, don't use it - unless its the only choice.
#if defined(SWQUAKE) && !defined(_DEBUG) && !defined(__DJGPP__)
	#undef SWQUAKE
#endif
#if defined(USE_EGL) && !defined(GLQUAKE)
	#undef USE_EGL
#endif
#if defined(WAYLANDQUAKE) && !(defined(__linux__) && (defined(VKQUAKE) || (defined(GLQUAKE) && defined(USE_EGL))))
	#undef WAYLANDQUAKE
#endif

#ifdef NO_MULTITHREAD
	#undef MULTITHREAD
#endif
#ifdef NO_LIBRARIES //catch-all...
	#define NO_DIRECTX
	#define NO_PNG
	#define NO_JPEG
	#define NO_ZLIB
	#define NO_OGG
	#define NO_FREETYPE
#endif
#ifdef NO_OPENAL
	#undef AVAIL_OPENAL
#endif
#ifdef NO_PNG
	#undef AVAIL_PNGLIB
#endif
#ifdef NO_JPEG
	#undef AVAIL_JPEGLIB
#endif
#ifdef NO_OGG
	#undef AVAIL_OGGVORBIS
#endif
#ifdef NO_FREETYPE
	#undef AVAIL_FREETYPE
#endif
#ifdef NO_ZLIB
	#undef AVAIL_ZLIB
	#undef AVAIL_PNGLIB
	#undef AVAIL_XZDEC
	#undef AVAIL_GZDEC
#endif

//include a file to update the various configurations for game-specific configs (hopefully just names)
#ifdef BRANDING_INC
	#include STRINGIFY(BRANDING_INC)
#endif
#ifndef DISTRIBUTION
	#define DISTRIBUTION "FTE"	//short name used to identify this engine. must be a single word
#endif
#ifndef DISTRIBUTIONLONG
	#define DISTRIBUTIONLONG "Forethought Entertainment"	//effectively the 'company' name
#endif
#ifndef FULLENGINENAME
	#define FULLENGINENAME "FTE Quake"	//the posh name for the engine
#endif
#ifndef ENGINEWEBSITE
	#define ENGINEWEBSITE "http://fte.triptohell.info"	//url for program
#endif

#if !defined(_WIN32) || defined(WINRT)
	#undef HAVE_SPEECHTOTEXT
	#undef AVAIL_MP3_ACM
	#undef AVAIL_DSOUND
	#undef AVAIL_XAUDIO2
	#undef AVAIL_WASAPI
#endif

#if !(defined(__linux__) || defined(__CYGWIN__)) || defined(ANDROID)
	#undef HAVE_GNUTLS
#endif
#if !defined(_WIN32) || (defined(_MSC_VER) && (_MSC_VER < 1300)) || defined(FTE_SDL)
	#undef HAVE_WINSSPI
#endif
//subservers only has code for win32 threads and linux
#if !((defined(_WIN32) && !defined(FTE_SDL) && !defined(WINRT)) || (defined(__linux__) && !defined(ANDROID) && !defined(FTE_SDL)))
	#undef SUBSERVERS
#endif

#ifndef HAVE_MIXER
	//disable various sound drivers if we can't use them anyway.
	#undef AVAIL_DSOUND
	#undef AVAIL_XAUDIO2
	#undef AVAIL_WASAPI
#endif


#ifdef NOMEDIA
	#undef HAVE_CDPLAYER		//includes cd playback. actual cds. faketracks are supported regardless.
	#undef HAVE_JUKEBOX			//includes built-in jukebox crap
	#undef HAVE_MEDIA_DECODER	//can play cin/roq, more with plugins
	#undef HAVE_MEDIA_ENCODER	//capture/capturedemo work.
	#undef AVAIL_MP3_ACM		//microsoft's Audio Compression Manager api
	#undef HAVE_SPEECHTOTEXT	//windows speech-to-text thing
#endif

#if defined(_XBOX)
	#define D3D8QUAKE
	#undef HAVE_TCP		//FIXME
	#undef HAVE_PACKET	//FIXME
	#undef SUPPORT_ICE	//screw that
	#undef PLUGINS		//would need LoadLibrary working properly.

	#undef AVAIL_DINPUT	//xbox apparently only really does controllers.
	#undef AVAIL_DSOUND	//FIXME
	#undef TEXTEDITOR	//its hard to edit text when you have just a controller (and no onscreen keyboard)
	#undef RAGDOLL		//needs a proper physics engine
	#undef AVAIL_MP3_ACM		//api not supported
	#undef AVAIL_OPENAL
	#undef HAVE_SPEECHTOTEXT	//api not supported
	#undef MULTITHREAD			//no CreateThread stuff.
	#undef SUBSERVERS			//single-process.
	#undef VOICECHAT
	#undef TERRAIN
	#undef Q2CLIENT
	#undef Q2SERVER
	#undef Q3CLIENT
	#undef Q3SERVER
	#undef HLCLIENT
	#undef HLSERVER
	#undef VM_Q1
	#undef VM_LUA
	#undef HALFLIFEMODELS
	#undef RUNTIMELIGHTING
	#undef HEXEN2
	#undef PACKAGE_DOOMWAD	
	#undef MAP_PROC	
	#undef Q1BSPS
	#undef Q2BSPS
	#undef Q3BSPS
	#undef RFBSPS
	#undef WEBSERVER		//http server
	#undef FTPSERVER		//ftp server
	#undef WEBCLIENT		//http client.
	#undef FTPCLIENT		//ftp client.
#endif

#ifdef __DJGPP__
	//no bsd sockets library.
	#undef HAVE_TCP
	#undef HAVE_PACKET
	#undef SUPPORT_ICE
	//too lazy to deal with no dlopen
	#undef PLUGINS
	#undef Q2SERVER
	#undef Q3SERVER
	#undef Q2CLIENT	//fixme...
	#undef Q3CLIENT	//might as well.
	//too lazy to write the code to boot up more cores. dosbox would probably hate it so why bother.
	#undef MULTITHREAD
	//too lazy to deal with various libraries
	#undef VOICECHAT
	#undef AVAIL_JPEGLIB
	#undef AVAIL_PNGLIB
	#undef AVAIL_OGGVORBIS
#endif

#ifdef FTE_TARGET_WEB
	//sandboxing means some stuff CANNOT work...
	#undef HAVE_TCP		//websockets are not real tcp.
	#undef HAVE_PACKET	//no udp support

	//try to trim the fat
	#undef VOICECHAT	//too lazy to compile speex
	#undef HLCLIENT		//dlls...
	#undef HLSERVER		//dlls...
	#undef CL_MASTER	//bah. use the site to specify the servers.
	#undef SV_MASTER	//yeah, because that makes sense in a browser
	#undef RAGDOLL		//no ode
	#undef TCPCONNECT	//err...
	#undef IRCCONNECT	//not happening
	#undef PLUGINS		//pointless
	#undef VM_Q1		//no dlls
	#undef MAP_PROC		//meh
	#undef HALFLIFEMODELS	//blurgh
	#undef SUPPORT_ICE	//requires udp, so not usable. webrtc could be used instead, but that logic is out of our hands.
	#undef HAVE_MIXER	//depend upon openal instead.

	//extra features stripped to try to reduce memory footprints
	#undef RUNTIMELIGHTING	//too slow anyway
	#undef Q2CLIENT
	#undef Q2SERVER	//requires a dll anyway.
	#undef Q3CLIENT
	#undef Q3SERVER //trying to trim memory use
//	#undef Q2BSPS	//emscripten can't cope with bss, leading to increased download time. too lazy to fix.
//	#undef Q3BSPS	//emscripten can't cope with bss, leading to increased download time. too lazy to fix.
	#undef TERRAIN
//	#undef PSET_SCRIPT	//bss+size
	#define GLSLONLY	//pointless having the junk
	#define GLESONLY	//should reduce the conditions a little
	#ifndef R_MAX_RECURSE
		#define R_MAX_RECURSE 2 //less bss
	#endif
//	#undef RTLIGHTS
	#undef HEADLESSQUAKE
	#define NO_FREETYPE
#endif
#ifdef WINRT
	//microsoft do not support winsock any more.
	#undef HAVE_TCP
	#undef HAVE_PACKET

	#undef TCPCONNECT	//err...
	#undef IRCCONNECT	//not happening
	#undef AVAIL_DSOUND	//yeah, good luck there
	#undef AVAIL_DINPUT	//nope, not supported.
	#undef SV_MASTER	//no socket interface
	#undef CL_MASTER	//no socket interface
	#undef MULTITHREAD
	#undef HEADLESSQUAKE
#endif
#ifdef ANDROID
	#define GLESONLY	//should reduce the conditions a little
//	#undef HEADLESSQUAKE
#endif
#if defined(NACL)
	//stuff is sandboxed.
	#undef HAVE_TCP		//websockets are not true tcp
	#undef HAVE_PACKET	//no udp support.

	#undef SUPPORT_ICE
	#undef CL_MASTER	//no sockets support
	#undef SV_MASTER	//noone uses this anyway
	#undef WEBSERVER		//http server
	#undef FTPSERVER		//ftp server
	#undef FTPCLIENT		//ftp client.
	#undef TCPCONNECT
	#undef IRCCONNECT
	#define GLSLONLY	//pointless having the junk
	#define GLESONLY	//should reduce the conditions a little
	#undef HEADLESSQUAKE
	#define NO_FREETYPE
#endif

#ifndef MULTITHREAD
	//database code requires threads to do stuff async.
	#undef USE_SQLITE
	#undef USE_MYSQL
#endif

#if defined(HAVE_WINSSPI) || defined(HAVE_GNUTLS)
	#define HAVE_SSL
#endif
#if defined(HAVE_GNUTLS) || defined(HAVE_WINSSPI)
	//FIXME: HAVE_WINSSPI does not work as a server.
	//FIXME: advertising dtls without a valid certificate will probably bug out if a client tries to auto-upgrade.
	//FIXME: we don't cache server certs
	#define HAVE_DTLS
#endif

#if defined(USE_SQLITE) || defined(USE_MYSQL)
	#define SQL
#endif

#if defined(AVAIL_GZDEC) && (!defined(AVAIL_ZLIB) || defined(NPFTE))
	//gzip needs zlib to work (pk3s can still contain non-compressed files)
	#undef AVAIL_GZDEC
#endif

#if defined(RFBSPS) && !defined(Q3BSPS)
	#define Q3BSPS	//rbsp might as well depend upon q3bsp - its the same thing but with more lightstyles (support for which can bog down the renderer a little).
#endif

#if defined(QWOVERQ3) && !defined(Q3SERVER)
	#undef QWOVERQ3
#endif

#if !defined(NQPROT) || defined(SERVERONLY) || !defined(AVAIL_ZLIB) || defined(DYNAMIC_ZLIB)
	#undef PACKAGE_DZIP
#endif

//fix things a little...
#ifdef NPQTV
	#define NPFTE
	#undef NPQTV
#endif
#ifdef NPFTE
	/*plugins require threads and stuff now, and http download support*/
	#ifndef MULTITHREAD
		#define MULTITHREAD
		#define WEBCLIENT
	#endif
	#undef SUBSERVERS
#endif

#if (defined(NOLOADERTHREAD) || !defined(MULTITHREAD)) && defined(LOADERTHREAD)
	#undef LOADERTHREAD
#endif

#ifndef _WIN32
	#undef QTERM	//not supported - FIXME: move to native plugin
#endif

#if defined(Q3BSPS) && !defined(Q2BSPS)
//	#define Q2BSPS	//FIXME: silently enable that as a dependancy, for now
#endif

#if (defined(Q2CLIENT) || defined(Q2SERVER))
	#ifndef Q2BSPS
		#error "Q2 game support without Q2BSP support. doesn't make sense"
	#endif
	#if !defined(MD2MODELS) || !defined(SP2MODELS)
		#error "Q2 game support without full Q2 model support. doesn't make sense"
	#endif
#endif

#ifdef NPFTE
	#undef TEXTEDITOR
	#undef WEBSERVER		//http server
	#undef FTPSERVER		//ftp server
	#undef FTPCLIENT		//ftp client.
#endif

#ifndef AVAIL_ZLIB
	#undef SUPPORT_ICE	//depends upon zlib's crc32 for fingerprinting. I cba writing my own.
#endif

#ifndef HAVE_TCP
	#undef TCPCONNECT
	#undef IRCCONNECT
	#undef WEBSERVER		//http server
	#undef FTPSERVER		//ftp server
	#undef FTPCLIENT		//ftp client.
	#if !defined(FTE_TARGET_WEB) && !defined(NACL)
		#undef WEBCLIENT
	#endif
#endif
#ifndef HAVE_PACKET
	#undef SV_MASTER
	#undef CL_MASTER
	#undef SUPPORT_ICE
#endif

#ifdef SERVERONLY	//remove options that don't make sense on only a server
	#undef Q2CLIENT
	#undef Q3CLIENT
	#undef HLCLIENT
	#undef VM_UI
	#undef VM_CG
	#undef TEXTEDITOR
	#undef RUNTIMELIGHTING

	#undef PSET_SCRIPT
	#undef PSET_CLASSIC
	#undef PSET_DARKPLACES
#endif
#ifdef CLIENTONLY	//remove optional server components that make no sence on a client only build.
	#undef Q2SERVER
	#undef Q3SERVER
	#undef HLSERVER
	#undef WEBSERVER
	#undef FTPSERVER
	#undef SUBSERVERS
	#undef VM_Q1
	#undef SQL
#endif


#if (defined(CSQC_DAT) || !defined(CLIENTONLY)) && defined(PLUGINS)	//use ode only if we have a constant world state, and the library is enbled in some form.
	#define USERBE
#endif

#if  defined(MD1MODELS) || defined(MD2MODELS) || defined(MD3MODELS)
	#define NONSKELETALMODELS
#endif
#if  defined(ZYMOTICMODELS) || defined(MD5MODELS) || defined(DPMMODELS) || defined(PSKMODELS) || defined(INTERQUAKEMODELS) 
	#define SKELETALMODELS	//defined if we have a skeletal model.
#endif
#if (defined(CSQC_DAT) || !defined(CLIENTONLY)) && defined(SKELETALMODELS)
	#define SKELETALOBJECTS	//the skeletal objects API is only used if we actually have skeletal models, and gamecode that uses the builtins.
#endif
#if !defined(USERBE) || !defined(SKELETALMODELS)
	#undef RAGDOLL	//not possible to ragdoll if we don't have certain other features.
#endif

#if !defined(RTLIGHTS)
	#undef MAP_PROC	//doom3 maps kinda NEED rtlights to look decent
#endif

#if !defined(Q3BSPS)
	#undef Q3CLIENT //reconsider this (later)
	#undef Q3SERVER //reconsider this (later)
#endif
#ifdef DEBUG
	#undef NOQCDESCRIPTIONS	//don't disable writing fteextensions.qc in debug builds, otherwise how would you ever build one? :o
#endif


#ifndef Q3CLIENT
	#undef VM_CG	// :(
	#undef VM_UI
#else
	#define VM_CG
	#define VM_UI
#endif

#if defined(VM_Q1) || defined(VM_UI) || defined(VM_CG) || defined(Q3SERVER) || defined(PLUGINS)
	#define VM_ANY
#endif

#if (defined(D3D8QUAKE) || defined(D3D9QUAKE) || defined(D3D11QUAKE)) && !defined(D3DQUAKE)
	#define D3DQUAKE	//shouldn't still matter
#endif

#define PROTOCOLEXTENSIONS

#ifdef MINIMAL
	#define IFMINIMAL(x,y) x
#else
	#define IFMINIMAL(x,y) y
#endif

//#define PRE_SAYONE	2.487	//FIXME: remove.

// defs common to client and server

#ifndef PLATFORM
	#if defined(FTE_TARGET_WEB)
		#define PLATFORM		"Web"
	#elif defined(NACL)
		#define PLATFORM		"Nacl"
	#elif defined(_WIN32_WCE)
		#define PLATFORM		"WinCE"
		#define ARCH_DL_POSTFIX ".dll"
	#elif defined(_WIN32)
		#if defined(WINRT)
			#define PLATFORM	"WinRT"		/*those poor poor souls. maybe just maybe I'll actually get the tools for a port, its just a shame that I won't be able to release said port*/
		#elif defined(_XBOX)
			#define PLATFORM	"Xbox"
		#else
			#define PLATFORM	"Win"
		#endif
		#define ARCH_DL_POSTFIX ".dll"
	#elif defined(_WIN16)
		#define PLATFORM		"Win16"
		#define ARCH_DL_POSTFIX ".dll"
	#elif defined(__CYGWIN__)
		#define PLATFORM		"Cygwin"	/*technically also windows*/
		#define ARCH_DL_POSTFIX ".dll"
	#elif defined(ANDROID) || defined(__ANDROID__)
		#define PLATFORM		"Android"	/*technically also linux*/
	#elif defined(__linux__)
		#define PLATFORM		"Linux"
	#elif defined(__APPLE__)
		#include "TargetConditionals.h"
		#if TARGET_IPHONE_SIMULATOR
			 #define PLATFORM	"iOSSim"
		#elif TARGET_OS_IPHONE
			#define PLATFORM	"iOS"
		#elif TARGET_OS_MAC
			#define PLATFORM	"Mac"
		#else
			#define PLATFORM	"Apple"
		#endif
	#elif defined(__FreeBSD__)
		#define PLATFORM	"FreeBSD"
	#elif defined(__OpenBSD__)
		#define PLATFORM	"OpenBSD"
	#elif defined(__NetBSD__)
		#define PLATFORM	"NetBSD"
	#elif defined(BSD)
		#define PLATFORM	"BSD"
	#elif defined(__MORPHOS__)
		#define PLATFORM	"MorphOS"
	#elif defined(__amigaos__)
		#define PLATFORM	"AmigaOS"
	#elif defined(MACOSX)
		#define PLATFORM	"MacOS X"
	#elif defined(__DOS__)
		#define PLATFORM	"Dos"
	#else
		#define PLATFORM	"Unknown"
	#endif
#endif

#ifndef ARCH_DL_POSTFIX
	#define ARCH_DL_POSTFIX ".so"
#endif

#if defined(_M_AMD64) || defined(__amd64__) || defined(__x86_64__)
	#ifdef __ILP32__
		#define ARCH_CPU_POSTFIX "x32"	//32bit pointers, with 16 registers.
	#else
		#ifdef _WIN32
			#define ARCH_CPU_POSTFIX "x64"
		#else
			#define ARCH_CPU_POSTFIX "amd64"
		#endif
	#endif
#elif defined(_M_IX86) || defined(__i386__)
	#define ARCH_CPU_POSTFIX "x86"
#elif defined(__powerpc__) || defined(__ppc__)
	#define ARCH_CPU_POSTFIX "ppc"
#elif defined(__aarch64__)
	#define ARCH_CPU_POSTFIX "arm64"
#elif defined(__arm__)
	#ifdef __SOFTFP__
		#define ARCH_CPU_POSTFIX "arm"
	#else
		#define ARCH_CPU_POSTFIX "armhf"
	#endif
#else
	#define ARCH_CPU_POSTFIX "unk"
#endif

#ifdef _MSC_VER
	#define VARGS __cdecl
	#define MSVCDISABLEWARNINGS
	#if _MSC_VER >= 1300
		#define FTE_DEPRECATED __declspec(deprecated)
		#ifndef _CRT_SECURE_NO_WARNINGS
			#define _CRT_SECURE_NO_WARNINGS
		#endif
		#ifndef _CRT_NONSTDC_NO_WARNINGS
			#define _CRT_NONSTDC_NO_WARNINGS
		#endif
	#endif
	#define NORETURN __declspec(noreturn)
#endif
#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
	#define FTE_DEPRECATED  __attribute__((__deprecated__))	//no idea about the actual gcc version
	#ifdef _WIN32
		#define LIKEPRINTF(x) __attribute__((format(ms_printf,x,x+1)))
	#else
		#define LIKEPRINTF(x) __attribute__((format(printf,x,x+1)))
	#endif
#endif
#if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
	#define NORETURN __attribute__((noreturn))
#endif

//I'm making my own restrict, because msvc's headers can't cope if I #define restrict to __restrict, and quite possibly other platforms too
#if __STDC_VERSION__ >= 199901L
	#define fte_restrict restrict
#elif defined(_MSC_VER) && _MSC_VER >= 1400
	#define fte_restrict __restrict
#else
	#define fte_restrict
#endif

#if _MSC_VER >= 1300
	#define FTE_ALIGN(a) __declspec(align(a))
#elif defined(__clang__)
	#define FTE_ALIGN(a) __attribute__((aligned(a)))
#elif __GNUC__ >= 3
	#define FTE_ALIGN(a) __attribute__((aligned(a)))
#else
	#define FTE_ALIGN(a)
#endif

#if __STDC_VERSION__ >= 199901L
	//C99 specifies that an inline function is used as a hint. there should be an actual body/copy somewhere (extern inline foo).
	#define fte_inline inline	//must have non-line 'int foo();' somewhere
	#define fte_inlinebody extern inline
#elif defined(_MSC_VER)
	//msvc will inline like C++. and that's fine.
	#define fte_inline __inline //c++ style
	#define fte_inlinebody
#elif (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
	//gcc will generally inline where it can - so long as its static. but that doesn't stop it warning
	#define fte_inline __attribute__((unused)) static
	#define fte_inlinebody static
#else
	//make it static so we at least don't get errors (might still get warnings. see above)
	#define fte_inline static
	#define fte_inlinebody static
#endif


#ifndef FTE_DEPRECATED
#define FTE_DEPRECATED
#endif
#ifndef LIKEPRINTF
#define LIKEPRINTF(x)
#endif
#ifndef VARGS
#define VARGS
#endif
#ifndef NORETURN
#define NORETURN
#endif

#ifdef _WIN32
#define ZEXPORT VARGS
#define ZEXPORTVA VARGS
#endif


// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define CACHE_SIZE	32		// used to align key data structures

#define UNUSED(x)	(x = x)	// for pesky compiler / lint warnings

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#define	MAX_QPATH		128			// max length of a quake game pathname
#define	MAX_OSPATH		1024		// max length of a filesystem pathname (260 on windows, but needs to be longer for utf8)

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	MAX_NQMSGLEN	65536		// max length of a reliable message. FIXME: should be 8000 to play safe with proquake
#define MAX_Q2MSGLEN	1400
#define MAX_QWMSGLEN	1450
#define MAX_OVERALLMSGLEN	65536	// mvdsv sends packets this big
#define	MAX_DATAGRAM	1450		// max length of unreliable message
#define MAX_Q2DATAGRAM	MAX_Q2MSGLEN
#define	MAX_NQDATAGRAM	1024		// max length of unreliable message with vanilla nq protocol
#define MAX_OVERALLDATAGRAM MAX_DATAGRAM

#define MAX_BACKBUFLEN	1200

//
// per-level limits
//
#ifdef FTE_TARGET_WEB
#define MAX_EDICTS		((1<<15)-1)
#else
//#define	MAX_EDICTS		((1<<22)-1)			// expandable up to 22 bits
#define	MAX_EDICTS		((1<<18)-1)			// expandable up to 22 bits
#endif
#define	MAX_LIGHTSTYLES	255					// 8bit. 255 = 'invalid', and thus only 0-254 are the valid indexes.
#define MAX_STANDARDLIGHTSTYLES 64
#define	MAX_PRECACHE_MODELS		4096		// 14bit.
#define	MAX_PRECACHE_SOUNDS		2048		// 14bit.
#define MAX_SSPARTICLESPRE 1024				// 14bit. precached particle effect names, for server-side pointparticles/trailparticles.
#define MAX_VWEP_MODELS 32

#define	MAX_CSMODELS		1024			// these live entirly clientside
#define MAX_CSPARTICLESPRE	1024

#define	SAVEGAME_COMMENT_LENGTH	39

#define	MAX_STYLESTRING	64

#define MAX_Q2EDICTS 1024

//
// stats are integers communicated to the client by the server
//
#define MAX_QW_STATS 32
enum {
#ifdef QUAKESTATS
STAT_HEALTH			= 0,
//STAT_FRAGS		= 1,
STAT_WEAPONMODELI	= 2,
STAT_AMMO			= 3,
STAT_ARMOR			= 4,
STAT_WEAPONFRAME	= 5,
STAT_SHELLS			= 6,
STAT_NAILS			= 7,
STAT_ROCKETS		= 8,
STAT_CELLS			= 9,
STAT_ACTIVEWEAPON	= 10,
STAT_TOTALSECRETS	= 11,
STAT_TOTALMONSTERS	= 12,
STAT_SECRETS		= 13,		// bumped on client side by svc_foundsecret
STAT_MONSTERS		= 14,		// bumped by svc_killedmonster
STAT_ITEMS			= 15,
STAT_VIEWHEIGHT		= 16,	//same as zquake
STAT_TIME			= 17,	//zquake
STAT_MATCHSTARTTIME = 18,
//STAT_UNUSED		= 19,
#ifdef SIDEVIEWS
STAT_VIEW2			= 20,
#endif
STAT_VIEWZOOM		= 21, // DP
#define STAT_VIEWZOOM_SCALE 255
//STAT_UNUSED		= 22,
//STAT_UNUSED		= 23,
//STAT_UNUSED		= 24,
STAT_IDEALPITCH		= 25,	//nq-emu
STAT_PUNCHANGLE_X	= 26,	//nq-emu
STAT_PUNCHANGLE_Y	= 27,	//nq-emu
STAT_PUNCHANGLE_Z	= 28,	//nq-emu
STAT_PUNCHVECTOR_X	= 29,
STAT_PUNCHVECTOR_Y	= 30,
STAT_PUNCHVECTOR_Z	= 31,

//these stats are used only when running a hexen2 mod/hud, and will never be used for a quake mod/hud/generic code.
STAT_H2_LEVEL	= 32,				// changes stat bar
STAT_H2_INTELLIGENCE,				// changes stat bar
STAT_H2_WISDOM,						// changes stat bar
STAT_H2_STRENGTH,					// changes stat bar
STAT_H2_DEXTERITY,					// changes stat bar
STAT_H2_BLUEMANA,					// changes stat bar
STAT_H2_GREENMANA,					// changes stat bar
STAT_H2_EXPERIENCE,					// changes stat bar
STAT_H2_CNT_TORCH,					// changes stat bar
STAT_H2_CNT_H_BOOST,				// changes stat bar
STAT_H2_CNT_SH_BOOST,				// changes stat bar
STAT_H2_CNT_MANA_BOOST,				// changes stat bar
STAT_H2_CNT_TELEPORT,				// changes stat bar
STAT_H2_CNT_TOME,					// changes stat bar
STAT_H2_CNT_SUMMON,					// changes stat bar
STAT_H2_CNT_INVISIBILITY,			// changes stat bar
STAT_H2_CNT_GLYPH,					// changes stat bar
STAT_H2_CNT_HASTE,					// changes stat bar
STAT_H2_CNT_BLAST,					// changes stat bar
STAT_H2_CNT_POLYMORPH,				// changes stat bar
STAT_H2_CNT_FLIGHT,					// changes stat bar
STAT_H2_CNT_CUBEOFFORCE,			// changes stat bar
STAT_H2_CNT_INVINCIBILITY,			// changes stat bar
STAT_H2_ARTIFACT_ACTIVE,
STAT_H2_ARTIFACT_LOW,
STAT_H2_MOVETYPE,
STAT_H2_CAMERAMODE,	//entity
STAT_H2_HASTED,
STAT_H2_INVENTORY,
STAT_H2_RINGS_ACTIVE,

STAT_H2_RINGS_LOW,
STAT_H2_ARMOUR1,
STAT_H2_ARMOUR2,
STAT_H2_ARMOUR3,
STAT_H2_ARMOUR4,
STAT_H2_FLIGHT_T,
STAT_H2_WATER_T,
STAT_H2_TURNING_T,
STAT_H2_REGEN_T,
STAT_H2_PUZZLE1,	//string
STAT_H2_PUZZLE2,	//string
STAT_H2_PUZZLE3,	//string
STAT_H2_PUZZLE4,	//string
STAT_H2_PUZZLE5,	//string
STAT_H2_PUZZLE6,	//string
STAT_H2_PUZZLE7,	//string
STAT_H2_PUZZLE8,	//string
STAT_H2_MAXHEALTH,
STAT_H2_MAXMANA,
STAT_H2_FLAGS,
STAT_H2_PLAYERCLASS,

STAT_H2_OBJECTIVE1,	//integer
STAT_H2_OBJECTIVE2,	//integer


STAT_MOVEVARS_AIRACCEL_QW_STRETCHFACTOR		= 220, // DP
STAT_MOVEVARS_AIRCONTROL_PENALTY			= 221, // DP
STAT_MOVEVARS_AIRSPEEDLIMIT_NONQW 			= 222, // DP
STAT_MOVEVARS_AIRSTRAFEACCEL_QW 			= 223, // DP
STAT_MOVEVARS_AIRCONTROL_POWER				= 224, // DP
STAT_MOVEFLAGS								= 225, // DP
STAT_MOVEVARS_WARSOWBUNNY_AIRFORWARDACCEL	= 226, // DP
STAT_MOVEVARS_WARSOWBUNNY_ACCEL				= 227, // DP
STAT_MOVEVARS_WARSOWBUNNY_TOPSPEED			= 228, // DP
STAT_MOVEVARS_WARSOWBUNNY_TURNACCEL			= 229, // DP
STAT_MOVEVARS_WARSOWBUNNY_BACKTOSIDERATIO	= 230, // DP
STAT_MOVEVARS_AIRSTOPACCELERATE				= 231, // DP
STAT_MOVEVARS_AIRSTRAFEACCELERATE			= 232, // DP
STAT_MOVEVARS_MAXAIRSTRAFESPEED				= 233, // DP
STAT_MOVEVARS_AIRCONTROL					= 234, // DP
STAT_FRAGLIMIT								= 235, // DP
STAT_TIMELIMIT								= 236, // DP
STAT_MOVEVARS_WALLFRICTION					= 237, // DP
STAT_MOVEVARS_FRICTION						= 238, // DP
STAT_MOVEVARS_WATERFRICTION					= 239, // DP
STAT_MOVEVARS_TICRATE						= 240, // DP
STAT_MOVEVARS_TIMESCALE						= 241, // DP
STAT_MOVEVARS_GRAVITY						= 242, // DP
STAT_MOVEVARS_STOPSPEED						= 243, // DP
STAT_MOVEVARS_MAXSPEED						= 244, // DP
STAT_MOVEVARS_SPECTATORMAXSPEED				= 245, // DP
STAT_MOVEVARS_ACCELERATE					= 246, // DP
STAT_MOVEVARS_AIRACCELERATE					= 247, // DP
STAT_MOVEVARS_WATERACCELERATE				= 248, // DP
STAT_MOVEVARS_ENTGRAVITY					= 249, // DP
STAT_MOVEVARS_JUMPVELOCITY					= 250, // DP
STAT_MOVEVARS_EDGEFRICTION					= 251, // DP
STAT_MOVEVARS_MAXAIRSPEED					= 252, // DP
STAT_MOVEVARS_STEPHEIGHT					= 253, // DP
STAT_MOVEVARS_AIRACCEL_QW					= 254, // DP
STAT_MOVEVARS_AIRACCEL_SIDEWAYS_FRICTION	= 255, // DP
#endif
	MAX_CL_STATS = 256
};

#ifdef QUAKEHUD
//
// item flags
//
#define	IT_SHOTGUN				(1u<<0)
#define	IT_SUPER_SHOTGUN		(1u<<1)
#define	IT_NAILGUN				(1u<<2)
#define	IT_SUPER_NAILGUN		(1u<<3)

#define	IT_GRENADE_LAUNCHER		(1u<<4)
#define	IT_ROCKET_LAUNCHER		(1u<<5)
#define	IT_LIGHTNING			(1u<<6)
#define	IT_SUPER_LIGHTNING		(1u<<7)

#define	IT_SHELLS				(1u<<8)
#define	IT_NAILS				(1u<<9)
#define	IT_ROCKETS				(1u<<10)
#define	IT_CELLS				(1u<<11)

#define	IT_AXE					(1u<<12)

#define	IT_ARMOR1				(1u<<13)
#define	IT_ARMOR2				(1u<<14)
#define	IT_ARMOR3				(1u<<15)

#define	IT_SUPERHEALTH			(1u<<16)

#define	IT_KEY1					(1u<<17)
#define	IT_KEY2					(1u<<18)

#define	IT_INVISIBILITY			(1u<<19)

#define	IT_INVULNERABILITY		(1u<<20)
#define	IT_SUIT					(1u<<21)
#define	IT_QUAD					(1u<<22)

#define	IT_SIGIL1				(1u<<28)

#define	IT_SIGIL2				(1u<<29)
#define	IT_SIGIL3				(1u<<30)
#define	IT_SIGIL4				(1u<<31)
#endif

//
// print flags
//
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages



//split screen stuff
#define MAX_SPLITS 4




//savegame vars
#define	SAVEGAME_COMMENT_LENGTH	39
#define	SAVEGAME_VERSION	667


#define PM_DEFAULTSTEPHEIGHT	18


#define dem_cmd			0
#define dem_read		1
#define dem_set			2
#define dem_multiple	3
#define	dem_single		4
#define dem_stats		5
#define dem_all			6


#endif	//ifndef __BOTHDEFS_H
