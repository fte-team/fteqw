# FTEQW README

This file contains the following sections:

- [0. PREFACE](#0-preface)
- [1. ABOUT](#1-about)
- [2. FEATURES](#2-features)
- [3. INSTALLING](#3-installing)
- [4. NETWORK QUICK-START](#4-network-quick-start)
- [5. TWEAKS](#5-tweaks)
- [6. TROUBLESHOOTING](#6-troubleshooting)
- [7. FURTHER READING](#7-further-reading)
- [8. CONTACT](#8-contact)
- [9. LICENSE](#9-license)


# 0. PREFACE

Thank you for downloading FTEQW, we hope you enjoy the many advanced features and ways to play. We understand the engine can be a bit daunting at times, so we hope this will help you get up and running with little to no hasle.

# 1. ABOUT

FTEQW is an advanced portable Quake engine.

It supports multiple games running on idTech, plus its own set of
games that developers have created. Due to the vast amount of supported
formats, features and innovations inside the engine and its very own
QuakeC compiler (fteqcc), it's very much considered the swiss-army
knife of Quake engines.

# 2. FEATURES

- Portable engine that runs on x86, amd64, ARM/64, PPC64LE and Web
- Hybrid protocol engine that supports multiple games
- Rendering API support for D3D8, D3D9, D3D11, OpenGL, Vulkan
- Splitscreen support for Quake and most mods
- In-game voice chat powered by either Opus or Speex
- Advanced renderer features, powered by a strong material system
  and support for both HLSL and GLSL shader code
- Multiple audio backends, from OSS to SDL_Sound and OpenAL Soft,
  plus API so developers can take advantage of AL EAX reverb features.
  And yes, DirectSound.
- Integrated next-generation QuakeC compiler and debugger
  with support for breakpoints, real-time ingame attribute debugging
  and much more
- Support for IPv4 and IPv6
- Video output presets, to make your games either look like the
  original versions, or more modern with real-time lighting and more
  accurate shading
- Support for CD-DA/Red Book music replacement in a variety of formats,
  such as Vorbis, MPEG-3, WAVE and FLAC (ffmpeg plugin required)

# 3. INSTALLING

Put the engine binary and desired plugins for your platform into the
root of the game directory.

Supported games:

- Quake and its Missionpacks
- QuakeWorld
- Quake II and its Missionpacks
- Quake III Arena and Team Arena
- HeXen II and its Missionpack
- Half-Life 1 & 2 (via the Rad Therapy project)

If you want to be explicit about the game you're starting, you can pass
the command-line parameters (read below for a complete list) for the
respective game. This will make sure that in a crowded universal game
directory, FTE starts the right game.

If you want to install music replacement files, you put them into the
`music` folder with the `trackXX` naming convention, starting with `track02`.

## Important note regarding Quake II based support

If you're running a 64-bit version of FTEQW, then you also need 64-bit
game-logic for Quake II. We recommend getting the game .dll/.so from the
Yamagi Quake II project for your respective platform. It's recommended
that you do for win32 as well, as that will ensure that save games work
properly and you can stop worrying about them becoming incompatible
between other machines.

## Android Installation

When using the Android app, you need to copy your gamedata folders
in the `Android/data/com.fteqw/files` folder.

An install of Quake should look like this for example:

	Android/data/com.fteqw/files/id1/pak0.pak
	Android/data/com.fteqw/files/id1/pak1.pak

> [!WARNING] 
> FTEQW is included in other Android APKs such as idTech4a++,
> please consult their documentation and help forums as we do not
> provide support to these unofficial third-party ports.

## Steam

FTEQW can detect and load data from the `/SteamApps/common/GAMEHERE` folders.
This includes the Quake I and II Remasters.

## Linux/Unix System Games Folder

FTEQW supports loading the legacy Unix game folders and will look for
gamedata in the following folders:

- /usr/share/games/quake
- /usr/share/games/quake2
- /usr/share/games/quake3
- /usr/share/games/hexen2
- /usr/share/games/halflife
- /usr/share/games/halflife2

## 4. NETWORK QUICK-START

Upon launching FTEQW, you can use the multiplayer menu's own built-in
server browser to join and connect to a vast array of matches across all
the different protocols. No more QuakeSpy/GameSpy 3D client required!

If you want to host a game, you can either run a listen server with
the ports forwarded, or host a listen server using frag-net.com's online
service. Start a new multiplayer server, change the "Public" setting to
"Holepunch" and players will automatically see your game in their
server-browser once it's started.

You can also set FTEQW to run in a terminal/command-prompt for hosting
a dedicated server session.
Simply pass the command-line argument like so:

	fteqw -dedicated

This will create an interactive shell reminiscent to the console that's
accessible in-game.

You can host a game directly with frag-net.com without having to worry
about port-forwarding and have a map up and running like so:

	fteqw +set sv_public 2 +set sv_playerslots 8 +map dm4

# 5. TWEAKS

You can apply tweaks by opening the console (SHIFT+ESC) and entering
commands into the line buffer. In there you can enter console variables
(cvars) that affect how the game behaves, as well as enter console
commands that trigger an action in the engine. 

You can always find a list of both of these with the console commands 
`cvarlist` and `cmdlist` respectively.

## Some example commands

`bind <key> <command>`	Binds a command to a key, e.g. `bind F12 quit`
`map <mapname>`		Starts a new game on <mapname>, e.g. `map dm4`
`connect <address>`	Establishes a connection to the specified 
			IP/hostname address.
`disconnect`		Closes and remote or local game session.
`cfg_save`		Save amy unsaved configuration changes.
`quit`			Quits the game.

## About console variables

Unlike other Quake engines, FTEs console variable system is more akin to
those of later idTech engines. Console variables can be set temporarily
(until the next engine restart) or permanently.

`set sv_port 26000`	Sets the current port to 26000 for this session
`seta sv_port 26000`	Sets the current port gets set to 26000 and makes
			sure it will get "archived", aka saved.

Sometimes, you'll be able to change a cvar without entering `set` or
`seta` beforehand. This is due to the cvar/cmd suggestion system.
When you simply set a cvar that way, `set` is assumed.
So those changes are only temporary.

## Commandline arguments

You can pass cvars and commands directly to the engine binary, prefixed
with a plus symbol like so:

	fteqw +set sv_port 26000 +map dm4

There's also other interesting commandline only arguments you can pass:

`-nohome`		Don't attempt to save configs, saves, screenshots in the
			home or user directory.
`-basedir <path>`	Specifies the root game directory path.
`-basegame <dir>`	Specifies which folder to look in for main game data.
`-game	<dir>`		Specifies which mod folder to load over the game data
`-window`		Tells the renderer to run in a window
`-manifest <fmf>`	Specifies a game manifest to load.
			This is for advanced game and mod switching.
`-dedicated`		Run the engine in dedicated server mode, no video out.

## Changing games and mods

Launching a Quake 1 mod can be done like so:

	fteqw -game fortress

That will assume a basegame of `id1` and the mod `fortress` will be
loaded on top of it.

However, if you're playing a game that uses no data from Quake 1:

	fteqw -basegame openquartz

Then it'll never even touch or peek into the folder `id1`.
Of course you can pass `-game` after that, too.

If you want to specify a supported game for more accurate behavior
and automatic data path finding, then you can pass these arguements
to load a specific game and behaviour:

	-quake
	-quake2
	-quake3
	-hexen2
	-halflife
	-halflife2

> [!NOTE]
> Half-Life 1 & 2 will download the Rad-Therapy project in place of their game logic, and HL2 also requires the Source formats plugin.

## Understanding manifests (advanced users)

You can setup custom game configurations with FTE's manifest files.
Those can be quite advanced, as they might inherit multiple directories,
change the name of the window title, set binds, aliases and cvars ahead
of time and much more.

	FTEMANIFEST 1
	GAME funky
	NAME "Funky QW Game"
	BASEGAME id1
	BASEGAME qw
	BASEGAME funky

An example manifest that will load id1, qw and then funky.
If you wanted, you can set default cvars and aliases in there like so:

	-set sv_example 1234
	-seta cl_foobar 5678
	-alias funky1 "impulse 416"
	+bind g funky1

Note the dash and plus symbols, they actually notate when to execute the
command in question. '-' means before the engine loads the game config
whereas '+' notates it will override anything that will usually be set
by the game.

You'd then save this as, for example, funky.fmf and load the manifest
via the command-line:

	fteqw -manifest funky.fmf

# 6. TROUBLESHOOTING

If you're running FTEQW on an older machine with Intel GMA graphics,
you probably want to try running the engine in D3D9 mode if you're
encountering any graphical issues:

	fteqw +set vid_renderer d3d9

If you can only run OpenGL but still have graphical issues, try forcing
support for the builtin GLSL off:

	fteqw +set gl_blacklist_debug_glsl 1

If FTEQW is seemingly not saving your settings, make sure you tell it to
save your config when you quit the game. If it still does not work
somehow, enter the console command `cfg_save` into the console. It
should output where the file gets saved or if there's any problems
writing the configuration file.

If OpenAL is causing crashes at launch (happens with some distributions,
that is out of our control) then try starting FTEQW with:

	 fteqw +set s_al_disable 1

# 7. FURTHER READING

Here's a small selection of some links we recommend for more info on FTEQW:

- [Spirit's excellent FTEQW wiki](https://quakewiki.org/wiki/FTEQW_Wiki)
- [@eukara's must read FTEQCC manual/QuakeC help](https://icculus.org/~marco/txtfiles.html)
- [@Shpoike's old FTEQW website and wiki](https://fte.triptohell.info/wiki/index.php/Main_Page)

# 8. CONTACT

If you need more help, have suggestions or want to hang out with the
developers that make FTEQW what it is, join us on a platform listed below!

Bug reports are welcomed! See our [public bug tracker](https://github.com/fte-team/fteqw/issues).

### Matrix

https://matrix.to/#/#fte:matrix.org

### IRC

**Server:** irc.quakenet.org

**Channel:** #fte

### Forums

**[Spike](https://forums.insideqc.com/memberlist.php?mode=viewprofile&u=26)** and **[eukara](https://forums.insideqc.com/memberlist.php?mode=viewprofile&u=949)** can be found on [insideqc.com](https://forums.insideqc.com/)

### Discord

https://discord.gg/p2ag7x6Ca6

# 9. LICENSE

Copyright (c) 2004-2025 FTE's team and its contributors
Quake source (c) 1999 id Software

FTEQW is supplied to you under the terms of the same license as the
original Quake sources, the GNU General Public License Version 2.
Please read the `LICENSE` file for details.

The latest source & binaries are always available at:

[fteqw.org](https://fteqw.org)

[fteqcc.org](https://fteqcc.org)
