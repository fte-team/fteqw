# Dependencies

Here is a list of dependencies required for building FTEQW on several platforms.

## Debian / Raspbian

### Base

	apt-get install libgl-dev gnutls-dev

### SDL2

	apt-get install libsdl2-dev

### GLX / X11 (part of libsdl2-dev)

	apt-get install libx11-dev libxcursor-dev libxrender-dev

### Plugin: ODE

	apt-get install autoconf automake libtool

### Plugin: FFMPEG

	apt-get install libavformat-dev libswscale-dev

## OpenBSD

### SDL2

	pkg_add sdl2

### Plugin: FFMPEG

	pkg_add ffmpeg

## Arch Linux

### Base

	pacman -S make gcc Xorg

### Plugin: ODE

	pacman -S zip automake autoconf

### Plugin: FFMPEG

	pacman -S ffmpeg4.4

You must pass these flags to compile the plugin:

	make plugins-rel NATIVE_PLUGINS="ffmpeg" AV_BASE=/usr/include/ffmpeg4.4/ AV_LDFLAGS="-l:libavcodec.so.58 -l:libavformat.so.58 -l:libavutil.so.56 -l:libswscale.so.5"

### SDL2

	pacman -S sdl2

## OpenSUSE

### Base

	zypper in make gcc gcc-c++ mesa-libGL-devel libgnutls-devel alsa-devel libopus-devel speex-devel libvorbis-devel

### SDL2

	zypper in libSDL2-devel

### GLX / X11

	zypper in libX11-devel libXcursor-devel libXrandr-devel

### Plugin: ODE

	zypper in autoconf automake libtool zip

### Plugin: FFMPEG

	zypper in ffmpeg-4-libavformat-devel ffmpeg-4-libswscale-devel

## Fedora

	dnf install make gcc gcc-c++ mesa-libGL-devel gnutls-devel alsa-devel libopus-devel speex-devel libvorbis-devel

### SDL2

	dnf install SDL2-devel

### GLX / X11 (part of libsdl2-dev)

	dnf install libX11-devel libXcursor-devel libXrender-devel

### Plugin: ODE

	dnf install autoconf automake libtool zip

### Plugin: FFMPEG

You need to install the RPM Fusion repo if you don't have it. We recommend reading their official guide: https://rpmfusion.org/Configuration

Then you can install the required version of FFMPEG:

	dnf install compat-ffmpeg4-devel

Finally, you must pass these flags to compile the plugin:

	make plugins-rel NATIVE_PLUGINS="ffmpeg" AV_BASE=/usr/include/compat-ffmpeg4 AV_LDFLAGS="-l:libavcodec.so.58 -l:libavformat.so.58 -l:libavutil.so.56 -l:libswscale.so.5"
