# Microsoft Developer Studio Project File - Name="vio2sf" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=vio2sf - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "vio2sf.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "vio2sf.mak" CFG="vio2sf - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "vio2sf - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "vio2sf - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "vio2sf - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_vio2sf"
# PROP Intermediate_Dir "Release_vio2sf"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "AOSSFDRV_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /O2 /I "src/vio2sf/desmume" /I "src/vio2sf/zlib" /D "LSB_FIRST" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "AOSSFDRV_EXPORTS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:none /machine:I386 /out:"C:\Program Files\winamp\Plugins\vio2sf.bin"

!ELSEIF  "$(CFG)" == "vio2sf - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_vio2sf"
# PROP Intermediate_Dir "Debug_vio2sf"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "AOSSFDRV_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "src/vio2sf/desmume" /I "src/vio2sf/zlib" /D "LSB_FIRST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "AOSSFDRV_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"C:\Program Files\winamp\Plugins\vio2sf.bin" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "vio2sf - Win32 Release"
# Name "vio2sf - Win32 Debug"
# Begin Group "vio2sf"

# PROP Default_Filter ""
# Begin Group "zlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\vio2sf\zlib\adler32.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\crc32.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\crc32.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\crypt.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\infback.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\inffast.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\inffixed.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\inflate.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\inflate.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\uncompr.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\zconf.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\zlib.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\zutil.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\zutil.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\zlib\contrib\masmx86\inffas32.obj
# End Source File
# End Group
# Begin Group "desmume"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\vio2sf\desmume\ARM9.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\arm_instructions.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\arm_instructions.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\armcpu.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\armcpu.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\bios.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\bios.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\bits.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\cflash.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\config.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\cp15.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\cp15.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\debug.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\Disassembler.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\dscard.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\FIFO.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\FIFO.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\GPU.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\GPU.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\instruction_tabdef.inc
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\matrix.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\matrix.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\mc.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\mc.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\mem.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\MMU.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\MMU.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\NDSSystem.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\NDSSystem.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\registers.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\SPU.cpp
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\SPU.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\thumb_instructions.c
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\thumb_instructions.h
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\thumb_tabdef.inc
# End Source File
# Begin Source File

SOURCE=.\src\vio2sf\desmume\types.h
# End Source File
# End Group
# End Group
# Begin Group "xsfc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\xsfc\tagget.h
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\xsfdrv.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\src\vio2sf\drvimpl.c
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\drvimpl.h
# End Source File
# Begin Source File

SOURCE=.\src\pversion.h
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\xsfdrv.c
# End Source File
# End Target
# End Project
