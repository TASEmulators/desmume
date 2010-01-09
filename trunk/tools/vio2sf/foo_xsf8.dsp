# Microsoft Developer Studio Project File - Name="foo_xsf8" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=foo_xsf8 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "foo_xsf8.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "foo_xsf8.mak" CFG="foo_xsf8 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "foo_xsf8 - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "foo_xsf8 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "foo_xsf8 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "foo_xsf8___Win32_Release"
# PROP BASE Intermediate_Dir "foo_xsf8___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "foo_xsf8___Win32_Release"
# PROP Intermediate_Dir "foo_xsf8___Win32_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FOO_XSF8_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FOO_XSF8_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 pfc.lib foobar2000_SDK.lib foobar2000_sdk_helpers.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"foo_8_vio2sf.dll"

!ELSEIF  "$(CFG)" == "foo_xsf8 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "foo_xsf8___Win32_Debug"
# PROP BASE Intermediate_Dir "foo_xsf8___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "foo_xsf8___Win32_Debug"
# PROP Intermediate_Dir "foo_xsf8___Win32_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FOO_XSF8_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FOO_XSF8_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 pfcD.lib foobar2000_SDKD.lib foobar2000_sdk_helpersD.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"C:\Program Files\foobar2000_8\components\foo_8_vio2sf.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "foo_xsf8 - Win32 Release"
# Name "foo_xsf8 - Win32 Debug"
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

SOURCE=.\src\xsfc\foo_input_xsf8.cpp
# End Source File
# Begin Source File

SOURCE=.\src\xsfc\foo_input_xsfcfg8.cpp
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
