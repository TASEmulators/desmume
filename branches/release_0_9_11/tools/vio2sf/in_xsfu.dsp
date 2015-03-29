# Microsoft Developer Studio Project File - Name="in_xsfu" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=in_xsfu - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "in_xsfu.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "in_xsfu.mak" CFG="in_xsfu - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "in_xsfu - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "in_xsfu - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "in_xsfu - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_in_xsfu"
# PROP Intermediate_Dir "Release_in_xsfu"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_XSF_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "src/aosdk" /I "src/aosdk/zlib" /I "src" /D ENABLE_UNICODE_PLUGIN=1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_XSF_EXPORTS" /FD /c
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
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:none /machine:I386 /out:"OUTPUT\winamp5\plugins\in_vio2sfu.dll"

!ELSEIF  "$(CFG)" == "in_xsfu - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_in_xsfu"
# PROP Intermediate_Dir "Debug_in_xsfu"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_XSF_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "src/aosdk" /I "src/aosdk/zlib" /I "src" /D ENABLE_UNICODE_PLUGIN=1 /D "LSB_FIRST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_XSF_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"in_vio2sfu.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "in_xsfu - Win32 Release"
# Name "in_xsfu - Win32 Debug"
# Begin Group "loadpe"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\loadpe\loadpe.c
# End Source File
# Begin Source File

SOURCE=.\src\loadpe\loadpe.h
# End Source File
# End Group
# Begin Group "xsfc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\src\xsfc\in_xsf.cpp
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\in_xsfcfg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\in_xsfcfg.h
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\leakchk.h
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\tagget.h
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\xsfc.cpp
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\xsfc.h
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\xsfcfg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\xsfcfg.h
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\xsfdrv.h
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\xsfui.rc
# End Source File
# End Group
# Begin Source File

SOURCE=.\src\pversion.h
# End Source File
# End Target
# End Project
