#!/bin/bash
START=$(date +%s)

SVNROOT=$(cd "$(dirname "$(readlink "$BASH_SOURCE")")" && pwd)
FTECONFIG=$SVNROOT/build.cfg

HOME=`echo ~`
BASE=$SVNROOT/..
#set this if you want non-default branding, for customised builds.
#export BRANDING=wastes

#defaults, if we're not set up properly.
#should be overriden in build.cfg
BUILDFOLDER=$HOME/htdocs
BUILDLOGFOLDER=$BUILDFOLDER/build_logs
SVNROOT=$BASE/fteqw-code
BUILD_LINUXx86=y
BUILD_LINUXx64=y
BUILD_WINDOWS=y
BUILD_ANDROID=y
BUILD_WEB=y
PLUGINS_LINUXx86="qi ezhud xmpp irc"
PLUGINS_LINUXx64="qi ezhud xmpp irc"
PLUGINS_LINUXx32="qi ezhud xmpp irc"
PLUGINS_WINDOWS="avplug ode qi ezhud xmpp irc"
THREADS="-j 4"

########### NaCL stuff
NACL_SDK_ROOT=/opt/nacl_sdk/pepper_31/

if [ -e $FTECONFIG ]; then
	. $FTECONFIG
else
	echo "WARNING: $FTECONFIG does not exist yet."
fi

export NACL_SDK_ROOT

########### Emscripten / Web Stuff
export EMSDK=$EMSCRIPTENROOT
#export WEB_PREJS="--pre-js $HOME/prejs.js"

########### Android Stuff. so messy...
#This is some android password that you should keep private. You should keep the keystore file private too, of course. Frankly, that part is more important than this small random number.
KEYPASSFILE=$BASE/.fte_keypass
if [ ! -e $KEYPASSFILE ]; then
	dd if=/dev/urandom count=9 bs=1 2>/dev/null | base64 > $KEYPASSFILE
	chmod 400 $KEYPASSFILE
fi
KEYPASS=`cat $KEYPASSFILE`
export JAVA_HOME=/usr
if [ ! -z "$ANDROIDROOT" ]; then 
	export ANDROID_HOME=$ANDROIDROOT
fi
if [ ! -z "$ANDROIDNDKROOT" ]; then 
	export ANDROID_NDK_ROOT=$ANDROIDNDKROOT
else
	export ANDROID_NDK_ROOT=$ANDROID_HOME/ndk-bundle
fi
export KEYTOOLARGS="-keypass $KEYPASS -storepass $KEYPASS -dname \"CN=fteqw.com, OU=ID, O=FTE, L=Unknown, S=Unknown, C=GB\""
export JARSIGNARGS="-storepass $KEYPASS"

########### Various Output etc Paths
QCCBUILDFOLDER=$BUILDFOLDER/fteqcc
SVNFOLDER=$SVNROOT/engine/release
ARCHIVEFOLDER=$BUILDFOLDER/archive
SVNDBGFOLDER=$SVNROOT/engine/debug
WARNINGLEVEL="-w"
FILELOCK=$BASE/.fte_buildlock

#./ccache-alias.sh

exec 9>$FILELOCK
if ! flock -n 9 ; then
	echo "Build script is already running!";
	exit 1
fi

mkdir -p $BUILDLOGFOLDER
if [ ! -d $SVNROOT ]; then
	#just in case...
	svn checkout https://svn.code.sf.net/p/fteqw/code/trunk $SVNROOT
fi

cd $SVNROOT/

echo "SVN Update"
svn update

cd engine

date > $BUILDLOGFOLDER/buildlog.txt
echo "Starting build" >> $BUILDLOGFOLDER/buildlog.txt

function build {
	BUILDSTART=$(date +%s)
	NAME=$1
	DEST=$2
	shift; shift
	make clean >> /dev/null
	echo -n "Making $NAME... "
	date > $BUILDLOGFOLDER/$DEST.txt
	echo make $THREADS $* >> $BUILDLOGFOLDER/$DEST.txt 2>&1
	make $THREADS $* >> $BUILDLOGFOLDER/$DEST.txt 2>&1
	if [ $? -eq 0 ]; then
		BUILDEND=$(date +%s)
		BUILDTIME=$(( $BUILDEND - $BUILDSTART ))
		echo "$BUILDTIME seconds"
		echo "$NAME done, took $BUILDTIME seconds" >> $BUILDLOGFOLDER/buildlog.txt
		rm -rf $BUILDFOLDER/$DEST >> /dev/null 2>&1
		mkdir $BUILDFOLDER/$DEST 2>> /dev/null
		mkdir $BUILDFOLDER/$DEST/debug 2>> /dev/null
		cp $SVNFOLDER/* $BUILDFOLDER/$DEST >> /dev/null 2>> /dev/null
		cp $SVNDBGFOLDER/* $BUILDFOLDER/$DEST/debug >> /dev/null 2>> /dev/null
		rm -rf $BUILDFOLDER/$DEST/*.a >> /dev/null 2>&1
		rm -rf $BUILDFOLDER/$DEST/debug/*.a >> /dev/null 2>&1
		rmdir $BUILDFOLDER/$DEST/debug 2>> /dev/null
	else
		echo "$NAME failed" >> $BUILDLOGFOLDER/buildlog.txt
		echo "failed"
	fi
}

function build_fteqcc {
	echo "--- no code ---"
}

echo "--- Engine builds ---"
#the -fno-finite-math-only is to avoid a glibc dependancy
if [ "$BUILD_LINUXx86" != "n" ]; then
	NATIVE_PLUGINS="$PLUGINS_LINUXx86" build "Linux 32-bit" linux_x86 FTE_TARGET=linux32 CPUOPTIMIZATIONS=-fno-finite-math-only qcc-rel rel dbg vk-rel plugins-rel plugins-dbg
fi
if [ "$BUILD_LINUXx64" != "n" ]; then
	NATIVE_PLUGINS="$PLUGINS_LINUXx64" build "Linux 64-bit" linux_amd64 FTE_TARGET=linux64 LDFLAGS="-Llibs/64" CPUOPTIMIZATIONS=-fno-finite-math-only qcc-rel rel dbg vk-rel plugins-rel plugins-dbg
fi
if [ "$BUILD_LINUXx32" != "n" ]; then
# 	CFLAGS="-DNO_JPEG"
	NATIVE_PLUGINS="$PLUGINS_LINUXx32" build "Linux x32" linux_x32 FTE_TARGET=linuxx32 CPUOPTIMIZATIONS=-fno-finite-math-only qcc-rel rel dbg plugins-rel plugins-dbg
fi
if [ "$BUILD_LINUXarmhf" != "n" ]; then
	#debian/ubuntu's armhf targets armv7. we instead target armv6, because that means we work on rpi too (but still with hard-float). It should be compatible although we likely need more ops.
	NATIVE_PLUGINS="$PLUGINS_LINUXarmhf" build "Linux ARMhf" linux_armhf FTE_TARGET=linuxarmhf CPUOPTIMIZATIONS=-fno-finite-math-only rel dbg qcc-rel plugins-rel plugins-dbg
fi
if [ "$BUILD_CYGWIN" != "n" ]; then
	NATIVE_PLUGINS="qi ezhud" build "Cygwin" cygwin qcc-rel rel dbg plugins-rel plugins-dbg
fi
if [ "$BUILD_WINDOWS" != "n" ]; then
	NATIVE_PLUGINS="$PLUGINS_WINDOWS" build "Windows 32-bit" win32 FTE_TARGET=win32 CFLAGS="$WARNINGLEVEL" sv-rel gl-rel vk-rel mingl-rel m-rel d3d-rel qcc-rel qccgui-scintilla qccgui-dbg gl-dbg sv-dbg plugins-dbg plugins-rel NATIVE_PLUGINS="$PLUGINS_WINDOWS"
	NATIVE_PLUGINS="$PLUGINS_WINDOWS" build "Windows 64-bit" win64 FTE_TARGET=win64 CFLAGS="$WARNINGLEVEL" sv-rel gl-rel vk-rel mingl-rel m-rel d3d-rel qcc-rel qccgui-scintilla qccgui-dbg gl-dbg sv-dbg plugins-dbg plugins-rel
fi
if [ "$BUILD_MSVC" != "n" ]; then
	NATIVE_PLUGINS="$PLUGINS_WINDOWS" build "Windows MSVC 32-bit" msvc FTE_TARGET=vc BITS=32 CFLAGS="$WARNINGLEVEL" sv-rel gl-rel vk-rel mingl-rel m-rel d3d-rel qcc-rel qccgui-scintilla qccgui-dbg gl-dbg sv-dbg plugins-dbg plugins-rel
	NATIVE_PLUGINS="$PLUGINS_WINDOWS" build "Windows MSVC 64-bit" msvc FTE_TARGET=vc BITS=64 CFLAGS="$WARNINGLEVEL" sv-rel gl-rel vk-rel mingl-rel m-rel d3d-rel qcc-rel qccgui-scintilla qccgui-dbg gl-dbg sv-dbg plugins-dbg plugins-rel
fi
export NATIVE_PLUGINS="qi ezhud xmpp irc"
if [ "$BUILD_ANDROID" != "n" ]; then
	build "Android" android droid-rel
fi
if [ "$BUILD_WINDOWS" != "n" ]; then
	build "NPFTE" npfte npfte-rel
fi
if [ "$BUILD_WEB" != "n" ]; then
	source $EMSDK/emsdk_env.sh >> /dev/null
	build "Emscripten" web FTE_TARGET=web gl-rel CC=emcc
fi
if [ "$BUILD_LINUX" != "n" ] && [ "$BUILD_SDL" != "n" ] && [ "$(uname -m)" != "x86_64" ]; then
	build "Linux 32-bit (SDL)" linux_x86_sdl FTE_TARGET=SDL BITS=32 LTO=1
fi
if [ "$BUILD_LINUX" != "n" ] && [ "$BUILD_SDL" != "n" ] && [ "$(uname -m)" == "x86_64" ]; then
	build "Linux 64-bit (SDL)" linux_amd64_sdl FTE_TARGET=SDL BITS=64 LDFLAGS="-Llibs/64" LTO=1
fi
if [ "$BUILD_WINDOWS" != "n" ] && [ "$BUILD_SDL" != "n" ]; then
	build "Windows 32-bit (SDL)" win32_sdl FTE_TARGET=win32_SDL gl-rel mingl-rel
	build "Windows 64-bit (SDL)" win64_sdl FTE_TARGET=win64_SDL LDFLAGS="-L./libs/mingw64-libs/" gl-rel mingl-rel
	CFLAGS="$WARNINGLEVEL -DNOLEGACY -DOMIT_QCC" build "Windows 32-bit nocompat" nocompat FTE_TARGET=win32 LTO=1 NOCOMPAT=1 BOTLIB_CFLAGS="" BOTLIB_OBJS="" gl-rel m-rel -k
fi
if [ "$BUILD_NACL" != "n" ]; then
	#non-pnacl is supported ONLY in chrome's store crap, but pnacl works anywhere.
#	build "Native Client 64-bit" nacl_amd64 FTE_TARGET=nacl NARCH=x86_64 gl-rel
#	build "Native Client 32-bit" nacl_x86 FTE_TARGET=nacl NARCH=x86_32 gl-rel
#internal compiler error	build "Native Client ARM" nacl_arm FTE_TARGET=nacl NARCH=arm gl-rel
	build "Native Client Portable" nacl_portable FTE_TARGET=nacl NARCH=pnacl gl-rel
fi
####build "MorphOS" morphos CFLAGS="-I$BASE/morphos/os-include/ -I$BASE/morphos/lib/ -L$BASE/morphos/lib/ -I$BASE/zlib/zlib-1.2.5 -L$BASE/zlib/zlib-1.2.5 -I./libs $WARNINGLEVEL" gl-rel mingl-rel sv-rel qcc-rel
if [ "$BUILD_MAC" != "n" ]; then
	#build "MacOSX" macosx_tiger CFLAGS="-I$BASE/mac/x86/include/ -L$BASE/mac/x86/lib -I./libs" FTE_TARGET=macosx_x86 sv-rel gl-rel mingl-rel qcc-rel
	#FIXME: figure out how to do universal binaries or whatever they're called
	build "MacOSX 32-bit" osx32 CC=o32-clang CXX=o32-clang++ FTE_TARGET=osx_x86 BITS=32 sv-rel gl-rel mingl-rel qcc-rel
	build "MacOSX 64-bit" osx64 CC=o64-clang CXX=o64-clang++ FTE_TARGET=osx_x86_64 BITS=64 sv-rel gl-rel mingl-rel qcc-rel
fi

#third party stuff / misc crap
if [ "$BUILD_WEB" != "n" ]; then
	cp $BASE/3rdparty/web/* $BUILDFOLDER/web/
fi
if [ "$BUILD_WINDOWS" != "n" ]; then
	cp $BASE/3rdparty/win32/3rdparty.zip $BUILDFOLDER/win32/3rdparty.zip
	cp $BASE/3rdparty/win64/3rdparty.zip $BUILDFOLDER/win64/3rdparty.zip
	if [ "$BUILD_SDL" != "n" ]; then
		cp $SVNROOT/engine/libs/SDL2-2.0.1/i686-w64-mingw32/bin/SDL2.dll $BUILDFOLDER/win32_sdl
		cp $SVNROOT/engine/libs/SDL2-2.0.1/x86_64-w64-mingw32/bin/SDL2.dll $BUILDFOLDER/win64_sdl
	fi
	cp $HOME/nocompat_readme.html $BUILDFOLDER/nocompat/README.html
fi

echo "--- QC builds ---"
rm -rf $QCCBUILDFOLDER 2>&1
mkdir -p $QCCBUILDFOLDER
if [ -e "$BUILDFOLDER/linux_x86/fteqw-gl32" ]
then
	echo "Making fteextensions.qc"
	mkdir -p ~/.fte/fte
	echo "pr_dumpplatform -o fteextensions" > ~/.fte/fte/minusargsaresilly.cfg
	echo "pr_dumpplatform -o csqcsysdefs -Tcs" >> ~/.fte/fte/minusargsaresilly.cfg
	echo "pr_dumpplatform -o menusysdefs -Tmenu" >> ~/.fte/fte/minusargsaresilly.cfg
	$BUILDFOLDER/linux_x86/fteqw-gl32 -basedir ~/.fte -nohome -quake +set snd_device none -nosound +set vid_renderer sv +exec minusargsaresilly.cfg +quit >> /dev/null
	mv ~/.fte/fte/src/fteextensions.qc $QCCBUILDFOLDER
	mv ~/.fte/fte/src/csqcsysdefs.qc $QCCBUILDFOLDER
	mv ~/.fte/fte/src/menusysdefs.qc $QCCBUILDFOLDER
else
	echo "Skipping FTE Extensions, no Linux gl32 build located"
fi


if [ -e $BUILDFOLDER/linux_x86/fteqcc32 ]; then
	echo "Making csaddon + qcmenu"
	mkdir -p $BUILDFOLDER/csaddon/
	cd $SVNROOT/quakec
	cd csaddon/src
	$BUILDFOLDER/linux_x86/fteqcc32 -srcfile csaddon.src > $BUILDLOGFOLDER/csaddon.txt
	mv ../csaddon.dat $BUILDFOLDER/csaddon/

	cd ../../menusys
	$BUILDFOLDER/linux_x86/fteqcc32 -srcfile menu.src > $BUILDLOGFOLDER/menu.txt
	rm fteqcc.log
	zip -q -9 -o -r $BUILDFOLDER/csaddon/menusys_src.zip .
	mv ../menu.dat $BUILDFOLDER/csaddon/
else
	echo "Skiping csaddon + qcmenu, no compiler build"
fi

cd $SVNROOT/engine/
svn info > $BUILDFOLDER/version.txt

if [ "$BUILD_LINUXx86" != "n" ]; then
	cp $BUILDFOLDER/linux_x86/fteqcc32 $QCCBUILDFOLDER/linux32-fteqcc
fi
if [ "$BUILD_LINUXx64" != "n" ]; then
	cp $BUILDFOLDER/linux_amd64/fteqcc64 $QCCBUILDFOLDER/linux64-fteqcc
fi
if [ "$BUILD_LINUXx32" != "n" ]; then
	cp $BUILDFOLDER/linux_x32/fteqccx32 $QCCBUILDFOLDER/linuxx32-fteqcc
fi
if [ "$BUILD_LINUXarmhf" != "n" ]; then
	cp $BUILDFOLDER/linux_armhf/fteqccarmhf $QCCBUILDFOLDER/linuxarmhf-fteqcc
fi
if [ "$BUILD_WINDOWS" != "n" ]; then
	cp $BUILDFOLDER/win32/fteqcc.exe $QCCBUILDFOLDER/win32-fteqcc.exe
	cp $BUILDFOLDER/win32/fteqccgui.exe $QCCBUILDFOLDER/win32-fteqccgui.exe
	cp $BUILDFOLDER/win64/fteqcc64.exe $QCCBUILDFOLDER/win64-fteqcc.exe
	cp $BUILDFOLDER/win64/fteqccgui64.exe $QCCBUILDFOLDER/win64-fteqccgui.exe
fi
#cp $BUILDFOLDER/morphos/fteqcc $QCCBUILDFOLDER/morphos-fteqcc
#cp $BUILDFOLDER/macosx_tiger/fteqcc $QCCBUILDFOLDER/macosx_tiger-fteqcc
cp $BUILDFOLDER/version.txt $QCCBUILDFOLDER/version.txt

if [ "$BUILD_NACL" != "n" ]; then
	mkdir -p $BUILDFOLDER/nacl
	NACL=$BUILDFOLDER/nacl/fteqw.nmf
	echo "{	\"program\":{" > $NACL
	if [ -e "$BUILDFOLDER/nacl_amd64/fteqw-x86_64.nexe" ]; then
		mv $BUILDFOLDER/nacl_amd64/fteqw-x86_64.nexe $BUILDFOLDER/nacl/fteqw-x86_64.nexe
		echo "	\"x86-64\":{\"url\":\"fteqw-x86_64.nexe\"}," >> $NACL
	fi
	if [ -e "$BUILDFOLDER/nacl_x86/fteqw-x86_32.nexe" ]; then
		mv $BUILDFOLDER/nacl_x86/fteqw-x86_32.nexe $BUILDFOLDER/nacl/fteqw-x86_32.nexe
		echo "	\"x86-32\":{\"url\":\"fteqw-x86_32.nexe\"}," >> $NACL
	fi
	if [ -e "$BUILDFOLDER/nacl_arm/fteqw-arm.nexe" ]; then
		mv $BUILDFOLDER/nacl_arm/fteqw-arm.nexe $BUILDFOLDER/nacl/fteqw-arm.nexe
		echo "	\"arm\":{\"url\":\"fteqw-arm.nexe\"}" >> $NACL
	fi
	$NACL_SDK_ROOT/toolchain/linux_pnacl/bin/pnacl-finalize -o $BUILDFOLDER/nacl/fteqw.pexe $BUILDFOLDER/nacl_portable/fteqw.pexe
	echo "	\"portable\":{\"pnacl-translate\":{\"url\":\"fteqw.pexe\"}}" >> $NACL
	echo "} }" >> $NACL
	rm -rf "$BUILDFOLDER/nacl_amd64"
	rm -rf "$BUILDFOLDER/nacl_x86"
	rm -rf "$BUILDFOLDER/nacl_arm"
	rm -rf "$BUILDFOLDER/nacl_portable"
fi

if [ "$BUILD_WINDOWS" != "n" ]; then
	echo Archiving output
	SVNVER=$(svnversion $SVNROOT)
	cd $BUILDFOLDER/
	zip -q -9 $ARCHIVEFOLDER/win_fteqw_$SVNVER.zip win32/fteglqw.exe win32/fteqwsv.exe win32/fteqccgui.exe win32/debug/fteglqw.exe win64/fteqw.exe win64/debug/fteglqw.exe


	cd $BUILDFOLDER/win32/
	zip -q -j -9 $BUILDFOLDER/fteqw_for_windows.zip fteglqw.exe fteqwsv.exe fteqccgui.exe fteplug_qi_x86.dll fteplug_xmpp_x86.dll fteplug_irc_x86.dll fteplug_ezhud_x86.dll
	cd $HOME/3rdparty_win32/
	zip -q -9 $BUILDFOLDER/fteqw_for_windows.zip ogg.dll vorbis.dll vorbisfile.dll freetype6.dll zlib1.dll
	mkdir -p $BASE/tmp/fte
	cd $BASE/tmp/
	cp $BUILDFOLDER/csaddon/menu.dat fte
	zip -q -9 $BUILDFOLDER/fteqw_for_windows.zip fte/menu.dat

	#~/afterquake/updatemini.sh
fi

echo "All done"

END=$(date +%s)
DIFF=$(( $END - $START ))
MINS=$(( $DIFF / 60 ))
echo "Total Compile Time: $MINS minutes" >> $BUILDLOGFOLDER/buildlog.txt
echo "Total Compile Time: $MINS minutes"

cd $HOME
#./errorlog.sh
#cd $HOME
#rm .bitchxrc
#cp ./fteqw/.bitchxrc ./
#./BitchX -a irc.quakenet.org -A -c "#fte" -n A_Gorilla
