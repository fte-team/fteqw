# Microsoft Developer Studio Project File - Name="qtvprox" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=qtvprox - Win32 Viewer Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "qtvprox.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "qtvprox.mak" CFG="qtvprox - Win32 Viewer Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "qtvprox - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "qtvprox - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "qtvprox - Win32 Viewer Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "qtvprox - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /c
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "speex-1.2beta1\include" /I "viewer\d3drend\sdkinc" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /Yu"qtv.h" /FD /GZ /c
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "qtvprox___Win32_Viewer_Debug"
# PROP BASE Intermediate_Dir "qtvprox___Win32_Viewer_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "qtvprox___Win32_Viewer_Debug"
# PROP Intermediate_Dir "qtvprox___Win32_Viewer_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /Yu"qtv.h" /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "VIEWER" /FR /Yu"qtv.h" /FD /GZ /c
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "qtvprox - Win32 Release"
# Name "qtvprox - Win32 Debug"
# Name "qtvprox - Win32 Viewer Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "viewer"

# PROP Default_Filter ""
# Begin Group "d3d"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\viewer\d3drend\d3d_backend.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\d3drend\d3d_image.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\d3drend\d3d_video.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=..\viewer\cvar.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\gl_backend.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\gl_bsp29.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\gl_image.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\gl_mdl.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\gl_testgrid.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\gl_vidsdl.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\matrix.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\model.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\viewer\renderer.cpp

!IF  "$(CFG)" == "qtvprox - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# PROP Exclude_From_Build 1
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# PROP BASE Exclude_From_Build 1
# SUBTRACT BASE CPP /YX /Yc /Yu
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# End Group
# Begin Group "sound"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\sc_dsound.c
# End Source File
# Begin Source File

SOURCE=..\sp_dsound.c
# End Source File
# End Group
# Begin Source File

SOURCE=..\bsp.c
# End Source File
# Begin Source File

SOURCE=..\control.c
# End Source File
# Begin Source File

SOURCE=..\crc.c
# End Source File
# Begin Source File

SOURCE=..\forward.c
# End Source File
# Begin Source File

SOURCE=..\libqtvc\glibc_sucks.c

!IF  "$(CFG)" == "qtvprox - Win32 Release"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\httpsv.c
# End Source File
# Begin Source File

SOURCE=..\mdfour.c
# End Source File
# Begin Source File

SOURCE=..\menu.c
# End Source File
# Begin Source File

SOURCE=..\msg.c
# End Source File
# Begin Source File

SOURCE=..\libqtvc\msvc_sucks.c

!IF  "$(CFG)" == "qtvprox - Win32 Release"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\netchan.c

!IF  "$(CFG)" == "qtvprox - Win32 Release"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# ADD CPP /Yu"qtv.h"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# ADD BASE CPP /Yu"qtv.h"
# ADD CPP /Yu"qtv.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\parse.c

!IF  "$(CFG)" == "qtvprox - Win32 Release"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# ADD CPP /Yu"qtv.h"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# ADD BASE CPP /Yu"qtv.h"
# ADD CPP /Yu"qtv.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\pmove.c
# End Source File
# Begin Source File

SOURCE=..\qw.c

!IF  "$(CFG)" == "qtvprox - Win32 Release"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# ADD CPP /Yu"qtv.h"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# ADD BASE CPP /Yu"qtv.h"
# ADD CPP /Yu"qtv.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\rcon.c
# End Source File
# Begin Source File

SOURCE=..\source.c

!IF  "$(CFG)" == "qtvprox - Win32 Release"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Debug"

# ADD CPP /Yc"qtv.h"

!ELSEIF  "$(CFG)" == "qtvprox - Win32 Viewer Debug"

# ADD BASE CPP /Yc"qtv.h"
# ADD CPP /Yc"qtv.h"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\viewer\common.h
# End Source File
# Begin Source File

SOURCE=..\viewer\cvar.h
# End Source File
# Begin Source File

SOURCE=..\viewer\d3drend\d3drenderer.h
# End Source File
# Begin Source File

SOURCE=..\viewer\glrenderer.h
# End Source File
# Begin Source File

SOURCE=..\viewer\maths.h
# End Source File
# Begin Source File

SOURCE=..\qtv.h
# End Source File
# Begin Source File

SOURCE=..\viewer\renderer.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
