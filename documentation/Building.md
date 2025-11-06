> Yay, you found out the secrit location to download the sauce code from!
>
> Right, urm, now what?
> Yeah, good question.
>
> Urm.

# Preface

Welcome to the building guide for FTEQW, as there are many systems supported, there is a number of ways to build the engine.

This should cover most, if not all the supported systems and methods.

### Contents

- [Repo Layout](#repo-layout)
- [Compiling](#compiling)
- [Easy Build Bot System (Linux)](#easy-build-bot-system-linux)
- [Windows Systems (cygwin)](#windows-systems-cygwin)
- [Linux/BSD Systems](#linux-bsd-system)
- [Android (FTEDroid) with cygwin](#android-ftedroid-with-cygwin)
- [Browser (emscripten)](#browser-emscripten)
- [FTEQCC](#fteqcc)
- [FTEIMG](#fteimg)
- [FTEIQM](#fteiqm)
- [FTEQTV](#fteqtv)
- [Plugins](#plugins)

# Repo Layout

- `.github/workflows:` Github Actions source files.

- `documentation:` General help and introduction.

- `engine:` FTEQW game engine itself. Both client and dedicated server.

- `engine/release:` The Makefile writes its release-build binaries here. Intermediate files are contained within a sub-directory.

- `engine/debug:` The Makefile writes its debug-build binaries here. Intermediate files are contained within a sub-directory.

- `fteqtv:` The QTV proxy server program.

- `plugins:` several optional plugins that do various interesting things, though not so interesting.

- `q3asm2:` Spike's quick hack at a QVM Assembler which is not horribly slow. Ignore it.

- `quakec:` Various QuakeC mods. Some interesting, some not.

- `quakec/basemod:` TimeServ's attempt to bugfix and modify vanilla Quake.

- `quakec/csaddon:` In-game CSQC-controlled editors. Currently contains the camquake featureset (thanks Jogi), rtlights editor, terrain editor ui, particle editor.

- `quakec/csqctest:` Spike's CSQC sample mod. Originally created as a feature testbed for the CSQC API. Useful as a reference/sample, but you perhaps don't want to use it as a base.

- `specs:` Modder/Advanced documentation and samples.

# Compiling

Compiling FTEQW is straightforward once you have the bare minimum of build dependencies (see `Dependencies.md` for more info).

For the binaries hosted here, we choose to statically link against many of the dependencies for portability reasons, while also linking against recent versions 
of libc for security reasons.

All binaries hosted here were built inside the `engine` dir of the src tree using GNU make, aka gmake.

### Build Systems

You have the choice of two build systems:

- Make
- CMake

This guide will show commands for both.

## Easy Build Bot System (Linux)

If you want to set up a Linux box that cross-compiles each target with your own private customisations, then you can run the `build_setup.sh` script to set up which targets you wish to support.

You can then just run the `build_wip.sh` script any time your code changes to have it rebuild every target you previously picked.

The setup script will install **android+emscripten** dependancies for you, so you're likely to find this an easier way to deal with those special targets.

### Notes

- The Android SDK can be a big download, while installing emscripten may require several hours to compile clang and about **40gb** of disk space if emscripten doesn't provide prebuilt stuff for your distro.

- The script can also be run from cygwin, but does not support compiling for Linux then.

## Windows Systems (cygwin)

If you want to compile a Win64 build in cygwin, it should be as simple as:

	make makelibs FTE_TARGET=win64
	make gl-rel FTE_TARGET=win64

or

	TODO

You only should need gcc and make installed in cygwin for this.

### Notes

It's currently not recommended to build using MSYS2, due to issues with zlib.

## Linux/BSD Systems

It's usually as straight-forward as:

	make makelibs
	make m-rel
or

	TODO

### Notes

- You can also change `FTE_TARGET` to be `win32`, `SDL2` and on Linux systems `linux32` and `linux64`.

- On BSD, you don't need to pass anything specific, but they should also compile the `linux` targets as well as the `SDL2` target fine.

- Not building with `makelibs` will attempt to dynamically link against your system-level versions of dependencies.
  Sometimes you want this, sometimes you don't. You definitely want that if you're trying to link against the Steam runtime.

##  Renders

### Vulkan

	make vk-rel

### OpenGL 

	make gl-rel

	make glcl-rel

	make mingl-rel

make mcl-rel

### DirectX

	make d3d-rel
  
## Android (FTEDroid) with cygwin

The phone port requires the Android SDk and can be compiled with the following command:

	make droid-rel PATH=C:\Cygwin\bin\ DROID_SDK_PATH=/path/to/android-sdk DROID_NDK_PATH=/path/to/android-ndk-r7 ANT=/path/to/apache-ant-1.8.2/bin/ant JAVATOOL="/path/to/jdk1.7.0_02/bin/" DROID_ARCH="armeabi x86" -j4 DROID_PACKSU=/path/to/pak0.pak

or

	-DFTE_ENGINE_FTEDROID=TRUE

On Linux/Unix systems you can omit the `PATH`, `ANT`, and `JAVATOOL` parts as they should already be in the path.

The `DROID_PACKSU` part is used to include the PAK file within the android package. Ideally you would use a PK3 file instead. 

Also you would use something that will not violate id Software's copyright. THIS IS AN EXAMPLE ONLY. You can omit the setting entirely if you require the user to provide their own packages.

Finally, install the `FTEDroid.apk` file on your Android device which should be located under the `release` folder.

### Notes

- There is no way to install the package with a different name at this time.

- Touchscreen controls are built-in.

- The APK looks for game data under:

	Android/data/com.fteqw/files

- Configs may be located at:

	/fte

or

	sdcard/fte

## Browser (emscripten)

	make FTE_TARGET=web web-rel

or

	-DFTE_PLUG_CEF=TRUE

## FTEQCC

	make qcc-rel

or

	-DFTE_TOOL_QCC=TRUE

## FTEQCC GUI

	make qccgui-rel

or

	-DFTE_TOOL_QCCGUI=TRUE


## Standalone QCVM

	TODO

or

	-DFTE_TOOL_QCVM=TRUE

## FTE Dedicated Server

	make sv-rel

or

	FTE_ENGINE_SERVER_ONLY=TRUE

## FTE Master Server

	make master-rel

or

	-DFTE_TOOL_MASTER=TRUE

## FTE Image Tool

	make imgtool-rel

or

	-DFTE_TOOL_IMAGE=TRUE

## FTE IQM Tool

	make iqm-rel

or

	-DFTE_TOOL_IQM=TRUE

## FTEQTV

	make qtv-rel

or

	-DFTE_TOOL_QTV=TRUE

## Small HTTP Server

	make httpserver

or

	-DFTE_TOOL_HTTPSV=TRUE

## Plugins

To build all currently stable plugins, it's as simple as:

	make plugins-rel

or

	TODO

You can specify which plugins get compiled by passing PLUGINS_NATIVE as an example:

	make plugins-rel NATIVE_PLUGINS="ffmpeg bullet irc"

or

	-DFTE_PLUG_FFMPEG=TRUE
	-DFTE_PLUG_BULLET=TRUE
	-DFTE_PLUG_IRC=TRUE

The list of available plugins:

> [!IMPORTANT] 
> Some plugins will require additional dependencies or flags on some systems, see `Dependencies.md` for more info.

- Bullet Physics
> Provides Rigid Body Physics.

	bullet
	-DFTE_PLUG_BULLET=TRUE

- Call of Duty (1 & 2) Format Support
> Provides compatability with Call Of Duty's file formats.

	cod
	-DFTE_PLUG_COD=TRUE

- EzHUD
> Provides compat with ezquake's hud scripts.

	ezhud
	-DFTE_PLUG_EZHUD=TRUE

- FFMPEG Video Decoding & RTMP Streaming
> Provides support for more audio formats, as well as video playback and better capture support.

	ffmpeg
	-DFTE_PLUG_FFMPEG=TRUE

-  GnuTLS
> Provides GnuTLS support for dtls/tls/https support. The crypto library that is actually used is controlled via the tls_provider cvar.

	gnutls
	-DFTE_PLUG_GNUTLS=TRUE

- Half-Life 2
> Adds support for reading various file formats used by Half-Life 2.

	hl2
	-DFTE_PLUG_HL2=TRUE

- IRC
> Allows you to chat on IRC without tabbing out.

	irc
	-DFTE_PLUG_IRC=TRUE

- libcef(Browser) Plugin
>This plugin provides support for an in-game web browser.

	libcef
	-DFTE_PLUG_CEF=TRUE

- Name Maker Plugin
> Provides a lame UI for selecting arbitrary non-ascii glyphs as part of your nickname.

	namemaker
	-DFTE_PLUG_NAMEMAKER=TRUE

- MPQ Archive Plugin
> Adds support for reading .mpq files (Diablo 1 + 2, World of Warcraft).

	mpq
	-DFTE_PLUG_MPQ=TRUE

- ODE Physics
> Provides Rigid Body Physics behaviours.

	ode
	-DFTE_PLUG_ODE=TRUE

- OpenSSL
> Provides OpenSSL support for dtls/tls/https support. The crypto library that is actually used is controlled via the tls_provider cvar.

	openssl
	-DFTE_PLUG_OPENSSL=TRUE

- OpenXR Support
> Provides support for Virtual Reality headsets and input devices.

	openxr
	-DFTE_PLUG_OPENXR=TRUE

- Quaddicted Map Database (Quake Injector)
> Provides easy access to the quaddicted map database. Once installed you can use eg 'map qi_dopa:start' to begin playing dopa, or load it via the menus.

	qi
	-DFTE_PLUG_QI=TRUE

- Quake 3 Game Logic and VM Support
> Provides compatability with Quake3's gamecode.

	quake3
	-DFTE_PLUG_QUAKE3=TRUE

- TerrainGen Plugin
> A lame example plugin for randomised terrain generation.

	terraingen
	-DFTE_PLUG_TERRAINGEN=TRUE

- Timidity Plugin
> Provides support for playback of midi files.

	timidity
	-DFTE_PLUG_TIMIDITY=TRUE

- XMPP/Jabber Protocol Support
> XMPP/Jabber instant messenger plugin for chatting without tabbing out.

	xmpp
	-DFTE_PLUG_XMPP=TRUE

- X11 Display Server (Standalone)
> Provides a primitive X11 server in the form of a video decoder plugin.

	x11server
	-DFTE_PLUG_X11SV=TRUE
