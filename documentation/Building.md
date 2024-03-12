SET(FTE_TOOL_IQM true CACHE BOOL "Compile IQM Tool.")

ET(FTE_MENU_SYS true CACHE BOOL "Compile System Menu.")

SET(FTE_TOOL_QTV true CACHE BOOL "Compile qtv server.")

SET(FTE_TOOL_MASTER true CACHE BOOL "Compile master server.")

make httpserver

SET(FTE_TOOL_HTTPSV true CACHE BOOL "Compile small http server.")

SET(FTE_TOOL_QCC true CACHE BOOL "Compile commandline qc compiler.")

SET(FTE_MENU_SYS true CACHE BOOL "Compile System Menu.")

SET(FTE_PLUG_XMPP true CACHE BOOL "Compile xmpp/jabber instant-messenger plugin.")

SET(FTE_PLUG_CEF true CACHE BOOL "Compile libcef (webbrowser) plugin.")

SET(FTE_PLUG_OPENXR true CACHE BOOL "Compile openxr plugin (for vr suppor

SET(FTE_PLUG_TIMIDITY false CACHE BOOL "Compile timidity audio plug


SET(FTE_PLUG_CEF true CACHE BOOL "Compile libcef (webbrowser) plugin.")

SET(FTE_ENGINE_CLIENT_ONLY false CACHE BOOL "Compile ftedw-cl (client-only) engine binary.")

SET(FTE_ENGINE_SERVER_ONLY true CACHE BOOL "Compile fteqw-sv (server only) engine binary.")

SET(FTE_ENGINE true CACHE BOOL "Compile fteqw engine binary.")

SET(FTE_ENGINE_FTEDROID true CACHE BOOL "Compile ftedroid engine shared library.")


SET(FTE_PLUG_QUAKE3 true CACHE BOOL "Compile Quake3 plugin.")

SET(FTE_PLUG_X11SV false CACHE BOOL "Compile x11 server plu

SET(FTE_PLUG_MODELS true CACHE BOOL "Compile models formats plugin.")

SET(FTE_PLUG_HL2 true CACHE BOOL "Compile support for hl2 file formats.

SET(FTE_PLUG_ODE true CACHE STRING "Compile ODE rigid body physics plugin.")

SET(FTE_PLUG_BULLET true CACHE BOOL "Compile bullet rigid body physics plugin.")

SET(FTE_PLUG_MPQ false CACHE BOOL "Compile mpq 

SET(FTE_PLUG_TERRAINGEN false CACHE BOOL "Compile sample terrain generation plu

SET(FTE_PLUG_NAMEMAKER false CACHE BOOL "Compile namemaker plug

#OPENSSL
	#the openssl license is incompatible with the GPL, so while we have code to use it distributing the binaries built with it is not a (legal) option.
	#note that openssl 3.0.0 upwards are apache-2 licensed, which IS gpl-3 compatible (though not gpl-2). debian has not caught up with that yet, however.
	FIND_PACKAGE(OpenSSL)
	IF(OPENSSL_VERSION_MAJOR LESS 3)
		SET(FTE_PRIVATE_USE_ONLY false CACHE BOOL "Ignore license violations.")
	ENDIF()

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
	- [Easy Build Bot System (Linux)](#easy-build-bot-system)
	- [Windows Systems (cygwin)](#windows)
	- [Linux/BSD Systems](#linux-bsd)
	- [Android (FTEDroid) with cygwin](#android)
	- [Browser (emscripten)](#emscripten)
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

	CMAKE HERE

You only should need gcc and make installed in cygwin for this.

### Notes

It's currently not recommended to build using MSYS2, due to issues with zlib.

## Linux/BSD Systems

It's usually as straight-forward as:

	make makelibs
	make m-rel
or

	CMAKE HERE

### Notes

- You can also change `FTE_TARGET` to be `win32`, `SDL2` and on Linux systems `linux32` and `linux64`.

- On BSD, you don't need to pass anything specific, but they should also compile the `linux` targets as well as the `SDL2` target fine.

- Not building with `makelibs` will attempt to dynamically link against your system-level versions of dependencies.
  Sometimes you want this, sometimes you don't. You definitely want that if you're trying to link against the Steam runtime.

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

	CMAKE HERE


## FTEQCC

	make qcc-rel

or

	-DFTE_TOOL_QCC=TRUE

## FTEQCC GUI

	make qccgui-rel

or

	-DFTE_TOOL_QCCGUI=TRUE


## FTE Dedicated Server

	make sv-rel

or

	-DFTE_TOOL_QCCGUI=TRUE


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

## Plugins

To build all currently stable plugins, it's as simple as:

	make plugins-rel
or
	CMAKE HERE


You can specify which plugins get compiled by passing PLUGINS_NATIVE as an example:

	make plugins-rel NATIVE_PLUGINS="ffmpeg bullet irc"
or
	-DFTE_PLUG_QUAKE3=TRUE

The list of available plugins (Make / CMake):

- bullet (Bullet Physics) / 
- ezhud (EZQuake HUD Support) / 
- ffmpeg (Various File Formats & RTMP Supported via FFMPEG) / 
- irc (IRC Client API) / 
- ode (ODE Physics) / 
- openssl (OpenSSL) / -DFTE_PLUG_OPENSSL=TRUE
- qi (Quake Injector) / -DFTE_PLUG_QI=TRUE
- quake3 (Quake 3 Game Logic and VM Support) / 

### Notes

- You will need to compile FFMPEG with additional flags on some systems, see `Dependencies.md` for more info.