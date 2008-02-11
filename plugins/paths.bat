REM batch file that sets the path to your q3asm and lcc
REM building qvms is dependant upon the q3 source release.
REM the gpled version is safe to use, but you will need to compile your own q3asm.
REM I guess we should include our own copy of them.

set QuakeDir=\quake
set Q3SrcDir=\quake\quake3
set Path=%Q3SrcDir%\quake3-1.32b\lcc\bin;%Q3SrcDir%\quake3-1.32b\q3asm\debug;..\..\..\lcc;..\..\..\q3asm2

set PluginsDir=%QuakeDir%\fte\plugins

REM we might as well remove some other things at the same time, so that we don't get conflicts with gcc and stuff.
set include=
set lib=
set MSDevDir=
