/***
*
*   Copyright (c) 2000-2019, Vera Visions. All rights reserved.
*
****/

/* branding defaults */
#define DISTRIBUTION "VTW"
#define DISTRIBUTIONLONG "Vera Visions"
#define FULLENGINENAME "The Wastes"
#define ENGINEWEBSITE "https://www.vera-visions.com/"
#define BRANDING_ICON "wastes.ico"

/* FS rebranding */
#define GAME_SHORTNAME		"wastes"
#define GAME_FULLNAME		FULLENGINENAME
#define GAME_BASEGAMES		"logos","wastes"
#define GAME_PROTOCOL		"The-Wastes"
#define GAME_DEFAULTPORT	23000

#ifndef GLQUAKE
#define GLQUAKE
#endif

/*
#ifndef VKQUAKE
#define VKQUAKE
#endif
*/
#undef VKQUAKE /* not yet, needs more testing */

 /* disable quake specific hacks and overrides */
#define QUAKETC
#define NOBUILTINMENUS
#define NOLEGACY

/* engine behaviour */
#define PLUGINS /* enables fteplug_ files */
#define AVAIL_ZLIB /* we need this for pk3 and ogg vorbis */
#define CL_MASTER /* allows for serverbrowser builtins */
#define CSQC_DAT /* clientside qcvm */
#define MENU_DAT /* persistent qcvm */
#define PSET_SCRIPT /* scripts defining particles */
#define RTLIGHTS /* only way FTE does dlights on Q3BSP basically */
#define LOADERTHREAD /* multithreading related */
#define USEAREAGRID /* leave it on, improves performance */
#define AVAIL_DINPUT /* input for Windows */
#define AVAIL_FREETYPE	/* for truetype font rendering */
#define AVAIL_STBI /* avoid libpng/libjpeg dependancies */
#define ENGINE_ROUTING /* engine-side, fast routing */

/* uncompressed textures */
#define IMAGEFMT_BMP /* sprays */
#define IMAGEFMT_TGA /* disable for RELEASE? */

/* compressed textures */
#define IMAGEFMT_DDS 
#define DECOMPRESS_ETC2
#define DECOMPRESS_RGTC
#define DECOMPRESS_S3TC

/* To be able to comm with Frag-Net.com */
#define HAVE_PACKET
#define SUPPORT_ICE
#define HAVE_TCP
#define HAVE_GNUTLS /* linux tls/dtls support */
#define HAVE_WINSSPI /* windows tls/dtls support */

#ifndef MULTITHREAD
#define MULTITHREAD
#endif

#ifndef DEBUG
/* if 2, disables writing fteextensions.qc completely. */
#define NOQCDESCRIPTIONS 2
#endif

/* various package formats */
#define PACKAGE_PK3
#define PACKAGE_TEXWAD
#define PACKAGE_Q1PAK

/* level formats */
#define Q3BSPS
#define Q1BSPS
#define TERRAIN

/* audio */
#define AVAIL_OPENAL
#define AVAIL_OGGVORBIS
#define HAVE_OPUS
#define VOICECHAT

/* Model formats, IQM/VVM and HLMDL for legacy maps */
#define INTERQUAKEMODELS
#define HALFLIFEMODELS

/* we don't need any of these */
#undef IMAGEFMT_PCX
#undef USE_INTERNAL_BULLET
#undef USE_INTERNAL_ODE
#undef PACKAGE_DOOMWAD
#undef PACKAGE_VPK
#undef DOOMWADS
#undef MAP_PROC
#undef Q2BSPS
#undef RFBSPS
#undef SPRMODELS
#undef SP2MODELS
#undef DSPMODELS
#undef MD1MODELS
#undef MD2MODELS
#undef MD3MODELS
#undef MD5MODELS
#undef ZYMOTICMODELS
#undef DPMMODELS
#undef PSKMODELS
#undef MENU_NATIVECODE	/* native menu replacing menuQC */
#undef MVD_RECORDING	/* server can record MVDs. */
#undef D3D9QUAKE	/* MICROS~1 trash */
#undef D3D11QUAKE	/* MICROS~1 trash */
#undef D3D8QUAKE	/* MICROS~1 trash */
#undef AVAIL_WASAPI	/* windows advanced sound api */
#undef AVAIL_DSOUND	/* MICROS~1 trash */
#undef BOTLIB_STATIC	/* q3 botlib */
#undef AVAIL_XZDEC	/* .xz decompression */
#undef HAVE_SPEEX	/* .xz decompression */
#undef AVAIL_GZDEC	/* .gz decompression */
#undef PACKAGE_DZIP	/* .dzip special-case archive support */
#undef AVAIL_PNGLIB	/* .png image format support (read+screenshots) */
#undef AVAIL_JPEGLIB	/* .jpeg image format support (read+screenshots) */
#undef AVAIL_MP3_ACM	/* .mp3 support (in windows). */
#undef IMAGEFMT_KTX
#undef IMAGEFMT_PKM
#undef IMAGEFMT_BLP
#undef IMAGEFMT_VTF
#undef NETPREPARSE	/* allows for running both nq+qw on the same server (if not, protocol used must match gamecode) */
#undef USE_SQLITE	/* sql-database-as-file support */
#undef QUAKESTATS	/* defines STAT_HEALTH etc. if omitted, you'll need to provide that functionality yourself */
#undef QUAKEHUD		/* support for drawing the vanilla hud */
#undef QWSKINS		/* disabling this means no qw .pcx skins nor enemy/team skin/colour forcing */
#undef SVRANKING	/* legacy server-side ranking system */
#undef USERBE 		/* required for physics */
#undef RAGDOLL		/* ragdoll support. requires RBE support */
#undef HUFFNETWORK	/* crappy network compression. probably needs reseeding */
#undef SVCHAT		/* ancient lame builtin to support NPC-style chat.. */
#undef VM_Q1		/* q1qvm implementation, to support ktx */
#undef Q2SERVER		/* q2 server+gamecode */
#undef Q2CLIENT		/* q2 client. file formats enabled separately */
#undef Q3CLIENT		/* q3 client stuff */
#undef Q3SERVER		/* q3 server stuff */
#undef HEXEN2		/* runs hexen2 gamecode, supports hexen2 file formats */
#undef NQPROT		/* act as an nq client/server, with nq gamecode */
#undef WEBSERVER	/* sv_ftp + sv_http cvars */
#undef WEBCLIENT	/* uri_get+any internal downloads etc */
#undef RUNTIMELIGHTING	/* automatic generation of .lit files */
#undef R_XFLIP		/* old silly thing */
#undef TEXTEDITOR	/* because emacs */
#undef TCPCONNECT	/* support for playing over tcp sockets, instead of just udp. compatible with qizmo */
#undef IRCCONNECT	/* lame support for routing game packets via irc server. not a good idea */
#undef PSET_CLASSIC	/* support the 'classic' particle system, for that classic quake feel */
#undef HAVE_CDPLAYER	/* Redbook CD Audio */
#undef QTERM
#undef SIDEVIEWS
#undef MAX_SPLITS
#undef SUBSERVERS		/* multi-map */
#undef HAVE_MIXER		/* OpenAL only */
#undef VM_LUA			/* lua game-logic */
#undef HLCLIENT			/* regressed, unfinished*/
#undef HLSERVER			/* regressed, unfinished */
#undef FTPSERVER
#undef HAVE_JUKEBO		/* includes built-in jukebox */
#undef HAVE_MEDIA_DECODER	/* can play cin/roq, more with plugins */
#undef HAVE_MEDIA_ENCODER	/* capture/capturedemo work */
#undef HAVE_SPEECHTOTEXT	/* Windows speech-to-text thing */
#undef SAVEDGAMES
#undef PACKAGEMANAGER		/* enable/disable/download packages and plugins */
#undef HEADLESSQUAKE
#undef WAYLANDQUAKE
#undef SERVER_DEMO_PLAYBACK	/* deprecated */
#undef DECOMPRESS_BPTC
#undef HAVE_OPENSSL		/* incomplete */
#undef IMAGEFMT_HDR
#undef IMAGEFMT_PBM
#undef IMAGEFMT_PSD
#undef IMAGEFMT_XCF
#undef IMAGEFMT_LMP
#undef IMAGEFMT_PNG
#undef IMAGEFMT_JPG
#undef IMAGEFMT_GIF
#undef IMAGEFMT_EXR
#undef IPLOG
#undef AVAIL_BOTLIB
#undef AVAIL_BZLIB
#undef DECOMPRESS_ASTC
#undef IMAGEFMT_ASTC
#undef HAVE_HTTPSV
#undef MODELFMT_MDX
#undef MODELFMT_OBJ
#undef MODELFMT_GLTF

#ifdef COMPILE_OPTS
/* things to configure qclib, which annoyingly doesn't include this
 * file itself */
-DOMIT_QCC	/* disable the built-in qcc */
-DSIMPLE_QCVM	/* disable qc debugging and 32bit opcodes */
#ifndef AVAIL_ZLIB
-DNO_ZLIB	/* disable zlib */
#endif
#ifdef AVAIL_PNGLIB
-DLINK_PNG
#endif
#ifdef AVAIL_JPEGLIB
-DLINK_JPEG
#endif
#ifdef AVAIL_FREETYPE
-DLINK_FREETYPE
#endif

/* makefile will respond to this by trying to link bullet into the
 * engine itself, instead of as a plugin. */
#ifdef USE_INTERNAL_BULLET
-DLINK_INTERNAL_BULLET
#endif

/* disable static speex */
#ifdef HAVE_SPEEX
-DNO_SPEEX
#endif

/* disable static botlib */
#ifndef BOTLIB_STATIC
-DNO_BOTLIB
#endif

-DLIBVORBISFILE_STATIC

/* optimise for size instead of speed. less cpu cache needed means that
 * its sometimes faster.*/
-Os
#endif
