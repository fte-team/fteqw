# Microsoft Developer Studio Project File - Name="npqtv" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=npqtv - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "npqtv.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "npqtv.mak" CFG="npqtv - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "npqtv - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "npqtv - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "npqtv - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "NPQTV_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs" /I "../libs/dxsdk7/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "NPQTV" /D "GLQUAKE" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib wsock32.lib /nologo /dll /machine:I386 /nodefaultlib:"libc" /out:"../../npqtv.dll" /libpath:"../libs/dxsdk7/lib"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "npqtv___Win32_Debug"
# PROP BASE Intermediate_Dir "npqtv___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "npqtv___Win32_Debug"
# PROP Intermediate_Dir "npqtv___Win32_Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "NPQTV_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../client" /I "../common" /I "../server" /I "../gl" /I "../sw" /I "../qclib" /I "../libs" /I "../libs/dxsdk7/include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "NPQTV" /D "GLQUAKE" /FR /Yu"quakedef.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib wsock32.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libcd" /out:"../../npqtv.dll" /pdbtype:sept /libpath:"../libs/dxsdk7/lib"

!ENDIF 

# Begin Target

# Name "npqtv - Win32 Release"
# Name "npqtv - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\common\cmd.c
# End Source File
# Begin Source File

SOURCE=..\common\com_mesh.c
# End Source File
# Begin Source File

SOURCE=..\common\common.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yc"quakedef.h"

!ENDIF 

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

SOURCE=..\common\fs_zip.c
# End Source File
# Begin Source File

SOURCE=..\common\gl_q2bsp.c
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

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

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
# End Source File
# Begin Source File

SOURCE=..\common\qvm.c
# End Source File
# Begin Source File

SOURCE=..\common\translate.c
# End Source File
# Begin Source File

SOURCE=..\common\zone.c
# End Source File
# End Group
# Begin Group "client"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\client\cd_null.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_cam.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_cg.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_demo.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_ents.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_ignore.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_input.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_main.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_parse.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_pred.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_screen.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_tent.c
# End Source File
# Begin Source File

SOURCE=..\client\cl_ui.c
# End Source File
# Begin Source File

SOURCE=..\client\clhl_game.c
# End Source File
# Begin Source File

SOURCE=..\client\clq2_cin.c
# End Source File
# Begin Source File

SOURCE=..\client\clq2_ents.c
# End Source File
# Begin Source File

SOURCE=..\client\clq3_parse.c
# End Source File
# Begin Source File

SOURCE=..\client\console.c
# End Source File
# Begin Source File

SOURCE=..\client\fragstats.c
# End Source File
# Begin Source File

SOURCE=..\client\image.c
# End Source File
# Begin Source File

SOURCE=..\client\in_win.c
# End Source File
# Begin Source File

SOURCE=..\client\keys.c
# End Source File
# Begin Source File

SOURCE=..\client\m_download.c
# End Source File
# Begin Source File

SOURCE=..\client\m_items.c
# End Source File
# Begin Source File

SOURCE=..\client\m_master.c
# End Source File
# Begin Source File

SOURCE=..\client\m_mp3.c
# End Source File
# Begin Source File

SOURCE=..\client\m_multi.c
# End Source File
# Begin Source File

SOURCE=..\client\m_options.c
# End Source File
# Begin Source File

SOURCE=..\client\m_script.c
# End Source File
# Begin Source File

SOURCE=..\client\m_single.c
# End Source File
# Begin Source File

SOURCE=..\client\menu.c
# End Source File
# Begin Source File

SOURCE=..\client\net_master.c
# End Source File
# Begin Source File

SOURCE=..\client\p_classic.c
# End Source File
# Begin Source File

SOURCE=..\client\p_null.c
# End Source File
# Begin Source File

SOURCE=..\client\p_script.c
# End Source File
# Begin Source File

SOURCE=..\client\pr_csqc.c
# End Source File
# Begin Source File

SOURCE=..\client\pr_menu.c
# End Source File
# Begin Source File

SOURCE=..\client\r_bulleten.c
# End Source File
# Begin Source File

SOURCE=..\client\r_efrag.c
# End Source File
# Begin Source File

SOURCE=..\client\r_part.c
# End Source File
# Begin Source File

SOURCE=..\client\r_partset.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\renderer.c
# End Source File
# Begin Source File

SOURCE=..\client\renderque.c
# End Source File
# Begin Source File

SOURCE=..\client\roq_read.c
# End Source File
# Begin Source File

SOURCE=..\client\sbar.c
# End Source File
# Begin Source File

SOURCE=..\client\skin.c
# End Source File
# Begin Source File

SOURCE=..\client\snd_directx.c
# End Source File
# Begin Source File

SOURCE=..\client\snd_dma.c
# End Source File
# Begin Source File

SOURCE=..\client\snd_mem.c
# End Source File
# Begin Source File

SOURCE=..\client\snd_mix.c
# End Source File
# Begin Source File

SOURCE=..\client\snd_mp3.c
# End Source File
# Begin Source File

SOURCE=..\client\snd_ov.c
# End Source File
# Begin Source File

SOURCE=..\client\snd_win.c
# End Source File
# Begin Source File

SOURCE=..\client\sys_win.c
# End Source File
# Begin Source File

SOURCE=..\client\teamplay.c
# End Source File
# Begin Source File

SOURCE=..\client\textedit.c
# End Source File
# Begin Source File

SOURCE=..\client\valid.c
# End Source File
# Begin Source File

SOURCE=..\client\view.c
# End Source File
# Begin Source File

SOURCE=..\client\wad.c
# End Source File
# Begin Source File

SOURCE=..\client\zqtp.c
# End Source File
# End Group
# Begin Group "gl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\gl\gl_alias.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_backend.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_bloom.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_draw.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_heightmap.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_hlmdl.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_model.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_ngraph.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_ppl.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_rlight.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_rmain.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_rmisc.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_rsurf.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_screen.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_shader.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_vidcommon.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_vidnt.c
# End Source File
# Begin Source File

SOURCE=..\gl\gl_warp.c
# End Source File
# Begin Source File

SOURCE=..\gl\glmod_doom.c
# End Source File
# Begin Source File

SOURCE=..\gl\ltface.c
# End Source File
# End Group
# Begin Group "asm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\common\math.s

!IF  "$(CFG)" == "npqtv - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=..\common\math.s
InputName=math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /EP /I ..\common  $(InputPath) > $(OUTDIR)\$(InputName).spp 
	..\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# Begin Custom Build
OutDir=.\npqtv___Win32_Debug
InputPath=..\common\math.s
InputName=math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /EP /I ..\common  $(InputPath) > $(OUTDIR)\$(InputName).spp 
	..\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\client\sys_wina.s

!IF  "$(CFG)" == "npqtv - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=..\client\sys_wina.s
InputName=sys_wina

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /EP /I ..\common  $(InputPath) > $(OUTDIR)\$(InputName).spp 
	..\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# Begin Custom Build
OutDir=.\npqtv___Win32_Debug
InputPath=..\client\sys_wina.s
InputName=sys_wina

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /EP /I ..\common  $(InputPath) > $(OUTDIR)\$(InputName).spp 
	..\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\worlda.s

!IF  "$(CFG)" == "npqtv - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=..\server\worlda.s
InputName=worlda

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /EP /I ..\common  $(InputPath) > $(OUTDIR)\$(InputName).spp 
	..\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# Begin Custom Build
OutDir=.\npqtv___Win32_Debug
InputPath=..\server\worlda.s
InputName=worlda

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /EP /I ..\common  $(InputPath) > $(OUTDIR)\$(InputName).spp 
	..\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /c /Cp /coff /Fo$(OUTDIR)\$(InputName).obj /Zm /Zi $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "server"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\server\net_preparse.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\pr_cmds.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\pr_q1qvm.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\savegame.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_ccmds.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_chat.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_demo.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_ents.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_init.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_main.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_master.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_move.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_mvd.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_nchan.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_phys.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_rankin.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_send.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\sv_user.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\svhl_game.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\svhl_phys.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\svhl_world.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\svmodel.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\svq2_ents.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\svq2_game.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\svq3_game.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\server\world.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu"qwsvdef.h"

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=.\npapi.def
# End Source File
# Begin Source File

SOURCE=..\client\sys_npqtv.c

!IF  "$(CFG)" == "npqtv - Win32 Release"

!ELSEIF  "$(CFG)" == "npqtv - Win32 Debug"

# ADD CPP /Yu

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\npplug.rc
# End Source File
# End Group
# End Target
# End Project
