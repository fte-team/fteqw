# Microsoft Developer Studio Project File - Name="ftequake" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=ftequake - Win32 GLSDLDebug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ftequake.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ftequake.mak" CFG="ftequake - Win32 GLSDLDebug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ftequake - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 GLDebug" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 GLRelease" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 MDebug" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 MRelease" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 MinGLDebug" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 MinGLRelease" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 Debug Dedicated Server" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 Release Dedicated Server" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 MinSW" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 GLDebugQ3" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 Debug Dedicated ServerQ3" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 D3DDebug" (based on "Win32 (x86) Application")
!MESSAGE "ftequake - Win32 GLSDLDebug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /GX /O2 /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "NDEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "GLQUAKE" /D "NEWBACKEND" /FR /Yu"quakedef.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /pdb:none /map /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteglqw_n.exe" /libpath:"../libs/dxsdk7/lib"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MT /W3 /Gm /GX /ZI /Od /I "../client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "GLQUAKE" /D "NEWBACKEND" /FR".\Debug/" /Fp".\Debug/qwcl.pch" /Yu"quakedef.h" /Fo".\Debug/" /Fd".\Debug/" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteglqw_n_dbg.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "GLDebug"
# PROP BASE Intermediate_Dir "GLDebug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "GLDebug"
# PROP Intermediate_Dir "GLDebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /G5 /MT /W3 /GX /ZI /Od /I "..\client" /I "../libs/freetype2/include" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /Yu"quakedef.h" /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteglqwvc6.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ftequake___Win32_GLRelease"
# PROP BASE Intermediate_Dir "ftequake___Win32_GLRelease"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ftequake___Win32_GLRelease"
# PROP Intermediate_Dir "ftequake___Win32_GLRelease"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I "..\client" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /Gr /MT /W3 /GX /O2 /Ob2 /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "NDEBUG" /D "_MBCS" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /FR /Yu"quakedef.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib ..\dxsdk\sdk\lib\dxguid.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /machine:I386 /out:"../../../fteqw.exe"
# ADD LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /pdb:none /map /machine:I386 /nodefaultlib:"msvcrt" /nodefaultlib:"libc" /out:"../../fteglqw.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_MDebug"
# PROP BASE Intermediate_Dir "ftequake___Win32_MDebug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\MDebug"
# PROP Intermediate_Dir ".\MDebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /ML /W3 /GX /ZI /Od /I "..\client\gltod3d\sdk7\include" /I "..\client\gltod3d\D3DFrame" /I "..\dxsdk\sdk\inc" /I "..\scitech\include" /I "..\client" /D "NQPROT" /D "_DEBUG" /D "GLQUAKE" /D "SERVERDLL" /D "WIN32" /D "_WINDOWS" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /YX /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# ADD CPP /nologo /G5 /MT /W3 /Gi /GX /ZI /Od /I "..\client" /I "../libs/dxsdk9/include" /I "../d3d" /I "../d3d9" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "GLQUAKE" /D "SWQUAKE" /D "USE_D3D" /D "D3DQUAKE" /Fr /Fp".\MDebug/qwcl.pch" /Yu"quakedef.h" /FD /c
# SUBTRACT CPP /WX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /pdb:".\GLDebug/dglqwcl.pdb" /debug /machine:I386 /out:"../../../fteglqw.exe"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteqw_dbg.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT LINK32 /profile /pdb:none

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ftequake___Win32_MRelease"
# PROP BASE Intermediate_Dir "ftequake___Win32_MRelease"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "MRelease"
# PROP Intermediate_Dir "MRelease"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I "..\client\gltod3d\sdk7\include" /I "..\client\gltod3d\D3DFrame" /I "..\dxsdk\sdk\inc" /I "..\scitech\include" /I "..\client" /D "NOSOUNDASM" /D "NDEBUG" /D "_MBCS" /D "GLQUAKE" /D "SERVERDLL" /D "NQPROT" /D "WIN32" /D "_WINDOWS" /FR /YX /FD /c
# ADD CPP /nologo /G6 /GX /O2 /I "..\client" /I "../libs/dxsdk9/include" /I "../d3d" /I "../d3d9" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "NDEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "GLQUAKE" /D "SWQUAKE" /D "USE_D3D" /D "D3DQUAKE" /Fr /Yu"quakedef.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /machine:I386 /out:"../../../fteglqw.exe"
# ADD LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /pdb:none /map /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteqw.exe" /libpath:"../libs/dxsdk7/lib"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_MinGLDebug"
# PROP BASE Intermediate_Dir "ftequake___Win32_MinGLDebug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "MinGLDebug"
# PROP Intermediate_Dir "MinGLDebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /ML /W3 /GX /ZI /Od /I "..\client\gltod3d\sdk7\include" /I "..\client\gltod3d\D3DFrame" /I "..\dxsdk\sdk\inc" /I "..\scitech\include" /I "..\client" /D "NQPROT" /D "_DEBUG" /D "GLQUAKE" /D "SERVERDLL" /D "WIN32" /D "_WINDOWS" /D "Q2SERVER" /D "DYNAMIC_ENTS" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /YX /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# ADD CPP /nologo /G5 /ML /W3 /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "MINIMAL" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /FR".\GLMinDebug/" /Fp".\GLMinDebug/qwcl.pch" /Yu"quakedef.h" /Fo".\GLMinDebug/" /Fd".\GLMinDebug/" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /pdb:".\GLDebug/dglqwcl.pdb" /debug /machine:I386 /out:"../../../fteglqw.exe"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /pdb:none /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteminglqw_dbg.exe" /libpath:"../libs/dxsdk7/lib"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ftequake___Win32_MinGLRelease"
# PROP BASE Intermediate_Dir "ftequake___Win32_MinGLRelease"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ftequake___Win32_MinGLRelease"
# PROP Intermediate_Dir "ftequake___Win32_MinGLRelease"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /Gr /W3 /GX /O2 /Ob2 /I "..\client\gltod3d\sdk7\include" /I "..\client\gltod3d\D3DFrame" /I "..\dxsdk\sdk\inc" /I "..\scitech\include" /I "..\client" /D "NOSOUNDASM" /D "NDEBUG" /D "_MBCS" /D "GLQUAKE" /D "SERVERDLL" /D "NQPROT" /D "WIN32" /D "_WINDOWS" /D "Q2SERVER" /D "DYNAMIC_ENTS" /FR /YX /FD /c
# ADD CPP /nologo /G6 /Gr /W3 /GX /O2 /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "MINIMAL" /D "NDEBUG" /D "_MBCS" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /FR"MinGLRelease/" /Fp"MinGLRelease/ftequake.pch" /Yu"quakedef.h" /Fo"MinGLRelease/" /Fd"MinGLRelease/" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /machine:I386 /out:"../../../fteglqw.exe"
# ADD LINK32 wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib ole32.lib /nologo /subsystem:windows /pdb:none /map /machine:I386 /out:"../../fteminglqw.exe" /libpath:"../libs/dxsdk7/lib"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_Debug_Dedicated_Server"
# PROP BASE Intermediate_Dir "ftequake___Win32_Debug_Dedicated_Server"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugServer"
# PROP Intermediate_Dir "DebugServer"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /ML /W3 /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /D "MINIMAL" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /YX /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# ADD CPP /nologo /G6 /ML /W3 /Gm /Gi /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "SERVERONLY" /FR".\DebugServer/" /Fp".\DebugServer/qwcl.pch" /Yu"quakedef.h" /Fo".\DebugServer/" /Fd".\DebugServer/" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /pdb:".\GLDebug/dglqwcl.pdb" /debug /machine:I386 /out:"../../../fteminglqw.exe"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:console /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteqwsv.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_Release_Dedicated_Server"
# PROP BASE Intermediate_Dir "ftequake___Win32_Release_Dedicated_Server"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "ftequake___Win32_Release_Dedicated_Server"
# PROP Intermediate_Dir "ftequake___Win32_Release_Dedicated_Server"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /ML /W3 /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /D "MINIMAL" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /D "SERVERONLY" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /YX /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# ADD CPP /nologo /G6 /ML /W3 /GX /O1 /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "SERVERONLY" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /Yu"quakedef.h" /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:console /pdb:".\GLDebug/dglqwcl.pdb" /debug /machine:I386 /out:"../../../fteminglqw.exe"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 winmm.lib wsock32.lib user32.lib shell32.lib Advapi32.lib /nologo /subsystem:console /pdb:none /map /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteqwsv.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_MinSW"
# PROP BASE Intermediate_Dir "ftequake___Win32_MinSW"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "ftequake___Win32_MinSW"
# PROP Intermediate_Dir "ftequake___Win32_MinSW"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /W3 /Gm /GX /ZI /Od /I "../client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "SWQUAKE" /FR".\Debug/" /Fp".\Debug/qwcl.pch" /YX /Fo".\Debug/" /Fd".\Debug/" /FD /c
# ADD CPP /nologo /G6 /W3 /Gm /GX /ZI /Od /I "../client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "SWQUAKE" /D "MINIMAL" /FR".\Debug/" /Fp".\Debug/qwcl.pch" /Yu"quakedef.h" /Fo".\Debug/" /Fd".\Debug/" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /out:"../../fteswqw_dbg.exe" /pdbtype:sept
# ADD LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /pdb:none /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteswqw_dbg.exe" /libpath:"../libs/dxsdk7/lib"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_GLDebugQ3"
# PROP BASE Intermediate_Dir "ftequake___Win32_GLDebugQ3"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "ftequake___Win32_GLDebugQ3"
# PROP Intermediate_Dir "ftequake___Win32_GLDebugQ3"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /ML /W3 /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /D "AVAIL_OGGVORBIS" /D "Q3CLIENT" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /YX /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# SUBTRACT BASE CPP /X
# ADD CPP /nologo /G5 /W3 /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /FR /Fp".\GLDebugQ3/qwcl.pch" /Yu"quakedef.h" /Fo".\GLDebugQ3/" /Fd".\GLDebugQ3/" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /fo"GLDebugQ3/winquake.res" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"GLDebugQ3/ftequake.bsc"
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /pdb:".\GLDebug/dglqwcl.pdb" /debug /machine:I386 /out:"../../fteglqw_dbg.exe"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteglqw_dbg.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_Debug_Dedicated_ServerQ3"
# PROP BASE Intermediate_Dir "ftequake___Win32_Debug_Dedicated_ServerQ3"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "ftequake___Win32_Debug_Dedicated_ServerQ3"
# PROP Intermediate_Dir "ftequake___Win32_Debug_Dedicated_ServerQ3"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /ML /W3 /Gm /Gi /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "SERVERONLY" /FR".\DebugServer/" /Fp".\DebugServer/qwcl.pch" /Yu"quakedef.h" /Fo".\DebugServer/" /Fd".\DebugServer/" /FD /c
# ADD CPP /nologo /G6 /ML /W3 /Gm /Gi /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "SERVERONLY" /D "Q3SERVER" /FR".\DebugServer/" /Fp".\DebugServer/qwcl.pch" /Yu"quakedef.h" /Fo".\DebugServer/" /Fd".\DebugServer/" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:console /pdb:none /debug /machine:I386 /out:"../../fteqwsv.exe" /libpath:"../libs/dxsdk7/lib"
# ADD LINK32 comctl32.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:console /pdb:none /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteqwsv.exe" /libpath:"../libs/dxsdk7/lib"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_D3DDebug"
# PROP BASE Intermediate_Dir "ftequake___Win32_D3DDebug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "D3DDebug"
# PROP Intermediate_Dir "D3DDebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /W3 /GX /ZI /Od /I "..\client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /Yu"quakedef.h" /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# SUBTRACT BASE CPP /X
# ADD CPP /nologo /G5 /W3 /GX /ZI /Od /I "..\client" /I "../d3d" /I "../libs/dxsdk9/include" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /I "../libs/freetype2/include" /D "_DEBUG" /D "D3DQUAKE" /D "WIN32" /D "_WINDOWS" /FR".\D3DDebug/" /Fp".\D3DDebug/qwcl.pch" /Yu"quakedef.h" /Fo".\D3DDebug/" /Fd".\D3DDebug/" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /map /debug /machine:I386 /out:"../../fteglqw_dbg.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fted3dqw_dbg.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ftequake___Win32_GLSDLDebug"
# PROP BASE Intermediate_Dir "ftequake___Win32_GLSDLDebug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "ftequake___Win32_GLSDLDebug"
# PROP Intermediate_Dir "ftequake___Win32_GLSDLDebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /MT /W3 /GX /ZI /Od /I "..\client" /I "../libs/sdl/include" /I "../libs/freetype2/include" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /Yu"quakedef.h" /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# SUBTRACT BASE CPP /X
# ADD CPP /nologo /G5 /MD /W3 /GX /ZI /Od /I "..\client" /I "../libs/sdl/include" /I "../libs/freetype2/include" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs/vc6-libs" /I "../libs" /I "../libs/speex" /I "../libs/dxsdk7/include" /D "_DEBUG" /D "GLQUAKE" /D "WIN32" /D "_WINDOWS" /D "_SDL" /FR".\GLDebug/" /Fp".\GLDebug/qwcl.pch" /Yu"quakedef.h" /Fo".\GLDebug/" /Fd".\GLDebug/" /FD /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"../../fteglqwvc6.exe" /libpath:"../libs/dxsdk7/lib"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 comctl32.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib sdl.lib sdlmain.lib /nologo /subsystem:windows /map /debug /machine:I386 /nodefaultlib:"libcmt" /out:"../../fteglqw_sdl.exe" /libpath:"../libs/dxsdk7/lib" /libpath:"../libs/vc6-libs"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "ftequake - Win32 Release"
# Name "ftequake - Win32 Debug"
# Name "ftequake - Win32 GLDebug"
# Name "ftequake - Win32 GLRelease"
# Name "ftequake - Win32 MDebug"
# Name "ftequake - Win32 MRelease"
# Name "ftequake - Win32 MinGLDebug"
# Name "ftequake - Win32 MinGLRelease"
# Name "ftequake - Win32 Debug Dedicated Server"
# Name "ftequake - Win32 Release Dedicated Server"
# Name "ftequake - Win32 MinSW"
# Name "ftequake - Win32 GLDebugQ3"
# Name "ftequake - Win32 Debug Dedicated ServerQ3"
# Name "ftequake - Win32 D3DDebug"
# Name "ftequake - Win32 GLSDLDebug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "server"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\server\net_preparse.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\pr_cmds.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\pr_q1qvm.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\savegame.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_ccmds.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_chat.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_demo.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_ents.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_init.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_main.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_master.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_move.c
# ADD CPP /Yu"quakedef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_mvd.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_nchan.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_phys.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_rankin.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_send.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\sv_user.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\svhl_game.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\svhl_phys.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\svhl_world.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\svq2_ents.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\svq2_game.c
# ADD CPP /Yu"qwsvdef.h"
# End Source File
# Begin Source File

SOURCE=..\server\svq3_game.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1
# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1
# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1
# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# ADD BASE CPP /Yu"qwsvdef.h"
# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# End Group
# Begin Group "client"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\client\cd_win.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_cam.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_cg.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_demo.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_ents.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_ignore.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_input.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_main.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_parse.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_pred.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_screen.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_tent.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\cl_ui.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\clhl_game.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\clq2_cin.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\clq2_ents.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\clq3_parse.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\console.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\fragstats.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\image.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\in_sdl.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\in_win.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\keys.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\m_download.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\m_items.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\m_master.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\m_mp3.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\m_multi.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\m_options.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\m_script.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\m_single.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\menu.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\net_master.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\p_classic.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\p_null.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\p_script.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\pr_csqc.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Intermediate_Dir "MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\pr_menu.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Intermediate_Dir "MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1
# ADD CPP /WX

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1
# ADD BASE CPP /WX
# ADD CPP /WX

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\r_d3.c
# End Source File
# Begin Source File

SOURCE=..\client\r_part.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\r_partset.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\renderer.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\renderque.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\roq_read.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\sbar.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\skin.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\snd_al.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\snd_directx.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\snd_dma.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\snd_mem.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\snd_mix.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\snd_ov.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\snd_sdl.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\snd_win.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\sys_linux.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\sys_morphos.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\sys_sdl.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\sys_win.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\teamplay.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\textedit.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\valid.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\view.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\wad.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\zqtp.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# End Group
# Begin Group "gl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\gl\doomclip.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\doommap.h
# End Source File
# Begin Source File

SOURCE=..\gl\gl_alias.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_backend.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_bloom.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_draw.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_font.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_heightmap.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_hlmdl.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_model.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_ngraph.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\common\gl_q2bsp.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_rlight.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_rmain.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_rmisc.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_rsurf.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_screen.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_shader.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_shadow.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_vidcommon.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_vidnt.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_vidsdl.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\gl_warp.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\glmod_doom.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\gl\LTFACE.C

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\r_2d.c
# End Source File
# Begin Source File

SOURCE=..\client\r_surf.c
# End Source File
# Begin Source File

SOURCE=..\gl\shader.h
# End Source File
# End Group
# Begin Group "common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\client\cl_plugin.inc
# End Source File
# Begin Source File

SOURCE=..\common\cmd.c
# End Source File
# Begin Source File

SOURCE=..\common\com_mesh.c
# End Source File
# Begin Source File

SOURCE=..\common\com_phys_ode.c
# End Source File
# Begin Source File

SOURCE=..\common\common.c
# ADD CPP /Yc"quakedef.h"
# End Source File
# Begin Source File

SOURCE=..\common\crc.c
# End Source File
# Begin Source File

SOURCE=..\common\cvar.c
# End Source File
# Begin Source File

SOURCE=..\common\fs.c
# End Source File
# Begin Source File

SOURCE=..\common\fs_pak.c
# End Source File
# Begin Source File

SOURCE=..\common\fs_stdio.c
# End Source File
# Begin Source File

SOURCE=..\common\fs_win32.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\common\fs_zip.c
# End Source File
# Begin Source File

SOURCE=..\common\huff.c
# End Source File
# Begin Source File

SOURCE=..\common\log.c
# End Source File
# Begin Source File

SOURCE=..\common\mathlib.c
# End Source File
# Begin Source File

SOURCE=..\common\md4.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\common\net_chan.c
# End Source File
# Begin Source File

SOURCE=..\common\net_wins.c
# End Source File
# Begin Source File

SOURCE=..\common\plugin.c
# End Source File
# Begin Source File

SOURCE=..\common\pmove.c
# End Source File
# Begin Source File

SOURCE=..\common\pmovetst.c
# End Source File
# Begin Source File

SOURCE=..\common\pr_bgcmd.c
# End Source File
# Begin Source File

SOURCE=..\common\q1bsp.c
# End Source File
# Begin Source File

SOURCE=..\common\q2pmove.c
# End Source File
# Begin Source File

SOURCE=..\common\q3common.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\common\qvm.c
# End Source File
# Begin Source File

SOURCE=..\common\translate.c
# End Source File
# Begin Source File

SOURCE=..\server\world.c
# ADD CPP /Yu"quakedef.h"
# End Source File
# Begin Source File

SOURCE=..\common\zone.c
# End Source File
# End Group
# Begin Group "QCLIB"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\QCLIB\Comprout.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\qclib\execloop.h

!IF  "$(CFG)" == "ftequake - Win32 Release"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# Begin Custom Build
InputDir=\ftetgcvs\wip\engine\qclib
InputPath=..\qclib\execloop.h

BuildCmds= \
	copy "$(InputPath)" "$(InputDir)\execloop16d.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32.h" \
	copy "$(InputPath)" "$(InputDir)\execloop32d.h" \
	

"$(InputDir)\execloop16d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputDir)\execloop32d.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\QCLIB\hash.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\QCLIB\initlib.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\QCLIB\pr_edict.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\QCLIB\Pr_exec.c
# ADD CPP /D "SEPARATEINCLUDES"
# SUBTRACT CPP /WX /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\QCLIB\pr_multi.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\qclib\pr_x86.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\QCLIB\progtype.h
# End Source File
# Begin Source File

SOURCE=..\QCLIB\qcc_cmdlib.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\QCLIB\qcc_pr_comp.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\QCLIB\qcc_pr_lex.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\QCLIB\QccMain.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\QCLIB\qcd_main.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\QCLIB\qcdecomp.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# End Group
# Begin Group "asm"

# PROP Default_Filter ""
# End Group
# Begin Group "http"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\http\ftpclient.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\http\ftpserver.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\http\httpclient.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\http\httpserver.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\http\iweb.h
# End Source File
# Begin Source File

SOURCE=..\http\iwebiface.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\http\webgen.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# End Group
# Begin Group "vc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\sndcodec\g711.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sndcodec\g721.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sndcodec\g723_24.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sndcodec\g723_40.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sndcodec\g72x.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sndcodec\snd_vc.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sndcodec\snd_voicecodecs.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sndcodec\voicechat.h

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "dedicated"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\server\sv_sys_win.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# ADD BASE CPP /Yu"qwsvdef.h"
# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\svmodel.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# ADD BASE CPP /Yu"qwsvdef.h"
# ADD CPP /Yu"qwsvdef.h"

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "d3d"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\d3d\d3d_backend.c
# End Source File
# Begin Source File

SOURCE=..\d3d\d3d_image.c
# End Source File
# Begin Source File

SOURCE=..\d3d\d3d_shader.c
# End Source File
# Begin Source File

SOURCE=..\d3d\vid_d3d.c
# End Source File
# End Group
# Begin Source File

SOURCE=..\client\winquake.rc

!IF  "$(CFG)" == "ftequake - Win32 Release"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\common\bothdefs.h
# End Source File
# Begin Source File

SOURCE=..\client\client.h
# End Source File
# Begin Source File

SOURCE=..\common\cvar.h
# End Source File
# Begin Source File

SOURCE=..\sw\d_ifacea.h
# End Source File
# Begin Source File

SOURCE=..\gl\gl_model.h
# End Source File
# Begin Source File

SOURCE=..\nqnet\net_dgrm.h
# End Source File
# Begin Source File

SOURCE=..\common\protocol.h
# End Source File
# Begin Source File

SOURCE=..\client\quakedef.h
# End Source File
# Begin Source File

SOURCE=..\client\render.h
# End Source File
# Begin Source File

SOURCE=..\server\svhl_gcapi.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\client\bymorphed.ico
# End Source File
# Begin Source File

SOURCE=..\client\q2.ico
# End Source File
# Begin Source File

SOURCE=..\client\qwcl2.ico
# End Source File
# End Group
# Begin Group "libmad"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\mp3\libmad\bit.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\decoder.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\fixed.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\frame.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\huffman.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\layer12.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\layer3.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\mymad.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\stream.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\synth.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\timer.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\mp3\libmad\version.c

!IF  "$(CFG)" == "ftequake - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinGLRelease"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Release Dedicated Server"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 MinSW"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLDebugQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 Debug Dedicated ServerQ3"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 D3DDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "ftequake - Win32 GLSDLDebug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
