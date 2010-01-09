# Microsoft Developer Studio Project File - Name="pfc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=pfc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pfc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pfc.mak" CFG="pfc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pfc - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pfc - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "pfc - Win32 Release ANSI" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pfc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_unicode"
# PROP Intermediate_Dir "Release_unicode"
# PROP Target_Dir ""
F90=df.exe
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "pfc" /D "UNICODE" /D "_UNICODE" /Yu"pfc.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "pfc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_unicode"
# PROP Intermediate_Dir "Debug_unicode"
# PROP Target_Dir ""
F90=df.exe
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "pfc" /D "UNICODE" /D "_UNICODE" /Yu"pfc.h" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "pfc - Win32 Release ANSI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "pfc___Win32_Release_ANSI0"
# PROP BASE Intermediate_Dir "pfc___Win32_Release_ANSI0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_ANSI"
# PROP Intermediate_Dir "Release_ANSI"
# PROP Target_Dir ""
F90=df.exe
MTL=midl.exe
# ADD BASE CPP /nologo /MD /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "pfc" /D "UNICODE" /D "_UNICODE" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "pfc" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "pfc - Win32 Release"
# Name "pfc - Win32 Debug"
# Name "pfc - Win32 Release ANSI"
# Begin Source File

SOURCE=.\array.h
# End Source File
# Begin Source File

SOURCE=.\bit_array.h
# End Source File
# Begin Source File

SOURCE=.\byte_order_helper.cpp
# End Source File
# Begin Source File

SOURCE=.\byte_order_helper.h
# End Source File
# Begin Source File

SOURCE=.\cfg_memblock.h
# End Source File
# Begin Source File

SOURCE=.\cfg_var.cpp
# End Source File
# Begin Source File

SOURCE=.\cfg_var.h
# End Source File
# Begin Source File

SOURCE=.\chainlist.h
# End Source File
# Begin Source File

SOURCE=.\critsec.h
# End Source File
# Begin Source File

SOURCE=.\guid.cpp
# End Source File
# Begin Source File

SOURCE=.\guid.h
# End Source File
# Begin Source File

SOURCE=.\list.h
# End Source File
# Begin Source File

SOURCE=.\mem_block.cpp
# End Source File
# Begin Source File

SOURCE=.\mem_block.h
# End Source File
# Begin Source File

SOURCE=.\other.cpp
# End Source File
# Begin Source File

SOURCE=.\other.h
# End Source File
# Begin Source File

SOURCE=.\pfc.h
# End Source File
# Begin Source File

SOURCE=.\profiler.cpp
# End Source File
# Begin Source File

SOURCE=.\profiler.h
# End Source File
# Begin Source File

SOURCE=.\ptr_list.h
# End Source File
# Begin Source File

SOURCE=.\stdafx.cpp

!IF  "$(CFG)" == "pfc - Win32 Release"

# ADD CPP /Yc"pfc.h"

!ELSEIF  "$(CFG)" == "pfc - Win32 Debug"

# ADD CPP /Yc

!ELSEIF  "$(CFG)" == "pfc - Win32 Release ANSI"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\string.cpp
# End Source File
# Begin Source File

SOURCE=.\string.h
# End Source File
# Begin Source File

SOURCE=.\utf8.cpp
# End Source File
# End Target
# End Project
