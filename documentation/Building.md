"Yay, you found out the secrit location to download the sauce code from!

Right, urm, now what?
Yeah, good question.

Urm."

# Preface

Welcome to the building guide for FTEQW, as there are many systems supported, there is a number of ways to build the engine. This should cover most, if not all the supported systems and methods.

# Repo Layout

- `engine:` FTEQW game engine itself. Both client and dedicated server.

- `engine/ftequake:` location of old msvc6 project file. Might not work.

- `engine/dotnet2005:` location of microsoft visual c 2005 project file. Most likely to be up to date, but also most likely to be specific to Spike's config.
engine/dotnet2010: location of microsoft visual c 2010 (express) project file. Might not work.

- `engine/release:` the makefile writes its release-build binaries here. Intermediate files are contained within a sub-directory.

- `engine/debug:` the makefile writes its debug-build binaries here. Intermediate files are contained within a sub-directory.

- `fteqtv:` the qtv proxy server program.
plugins: several optional plugins that do various interesting things, though not so interesting

- `q3asm2:` my quick hack at a qvm assembler which is not horribly slow. ignore it.

- `quakec:` Various quakec mods. Some interesting, some not.

- `quakec/basemod:` TimeServ's attempt to bugfix and modify vanilla quake

- `quakec/csaddon:` ingame csqc-controlled editors. Currently contains the camquake featureset (thanks Jogi), rtlights editor, terrain editor ui, particle editor.

- `quakec/csqctest:` my csqc sample mod. Originally created as a feature testbed for the csqc api. Useful as a reference/sample, but you perhaps don't want to use it as a base.

- `specs:` modder/advanced documentation and samples.

# Compiling

Compiling FTEQW is straightforward once you have the bare minimum of build dependencies (see `Dependencies.md` for more info).

For the binaries hosted here, we choose to statically link against many of the dependencies for portability reasons, while also linking against recent versions 
of libc for security reasons.

All binaries hosted here were built inside the `engine` dir of the src tree using GNU make, aka gmake.

## Windows Systems (cygwin)

If you want to compile a Win64 build in cygwin, it should be as simple as:

	make makelibs FTE_TARGET=win64
	make gl-rel FTE_TARGET=win64

You only should need gcc and make installed in cygwin for this.

*Note about MSYS2:* It's currently not recommended to build using MSYS2, due to issues with zlib.

## Linux/BSD Systems

It's usually as straight-forward as:

	make makelibs
	make m-rel

# Plugins

To build all currently stable plugins, it's as simple as:

	make plugins-rel

You can specify which plugins get compiled by passing PLUGINS_NATIVE as an example:

	make plugins-rel NATIVE_PLUGINS="ffmpeg bullet irc"

### Notes

You can also change `FTE_TARGET` to be `win32`, `SDL2` and on Linux systems `linux32` and `linux64`.

On BSD, you don't need to pass anything specific, but they should also compile the `linux` targets as well as the `SDL2` target fine.

*Note about makelibs:* Not building with makelibs will attempt to dynamically link against your system-level versions of dependencies.

Sometimes you want this, sometimes you don't. You definitely want that if you're trying to link against the Steam runtime.

## Android (FTEDroid) with cygwin

	make droid-rel PATH=C:\Cygwin\bin\ DROID_SDK_PATH=/path/to/android-sdk DROID_NDK_PATH=/path/to/android-ndk-r7 ANT=/path/to/apache-ant-1.8.2/bin/ant JAVATOOL="/path/to/jdk1.7.0_02/bin/" DROID_ARCH="armeabi x86" -j4 DROID_PACKSU=/path/to/pak0.pak

On Linux you can omit the `PATH`, `ANT`, and `JAVATOOL` parts as they should already be in the path.

The `DROID_PACKSU` part is used to include the pak file within the android package. Ideally you would use a PK3 file instead. 

Also you would use something that will not violate id Software's copyright. THIS IS AN EXAMPLE ONLY. You can omit the setting entirely if you require the user to provide their own packages.

Finally, install the `FTEDroid.apk` file on your Android device which should be located
under the `release` folder.

*Note:* There is no way to install the package with a different name at this time.

## Browser (emscripten)

TODO

## Easy Build Bot System

If you want to set up a Linux box that cross-compiles each target with your own private customisations, then you can run the `build_setup.sh` script to set up which targets you wish to support.

You can then just run the `build_wip.sh` script any time your code changes to have it rebuild every target you previously picked.

The setup script will install *android+emscripten* dependancies for you, so you're likely to find this an easier way to deal with those special targets.

### Notes

The Android SDK can be a big download, while installing emscripten may require several hours to compile clang and about *40gb* of disk space if emscripten doesn't provide prebuilt stuff for your distro.

The script can also be run from cygwin, but does not support compiling for Linux then.