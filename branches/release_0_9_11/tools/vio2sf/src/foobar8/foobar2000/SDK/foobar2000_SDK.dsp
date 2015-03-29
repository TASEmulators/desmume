# Microsoft Developer Studio Project File - Name="foobar2000_SDK" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=foobar2000_SDK - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "foobar2000_SDK.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "foobar2000_SDK.mak" CFG="foobar2000_SDK - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "foobar2000_SDK - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "foobar2000_SDK - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "foobar2000_SDK - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
F90=df.exe
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /O1 /Oy /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "UNICODE" /D "_UNICODE" /Yu"foobar2000.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "foobar2000_SDK - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
F90=df.exe
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "UNICODE" /D "_UNICODE" /Yu"foobar2000.h" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "foobar2000_SDK - Win32 Release"
# Name "foobar2000_SDK - Win32 Debug"
# Begin Source File

SOURCE=.\audio_chunk.cpp
# End Source File
# Begin Source File

SOURCE=.\audio_chunk.h
# End Source File
# Begin Source File

SOURCE=.\commandline.cpp
# End Source File
# Begin Source File

SOURCE=.\commandline.h
# End Source File
# Begin Source File

SOURCE=.\component.h
# End Source File
# Begin Source File

SOURCE=.\componentversion.h
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\config_var.h
# End Source File
# Begin Source File

SOURCE=.\console.cpp
# End Source File
# Begin Source File

SOURCE=.\console.h
# End Source File
# Begin Source File

SOURCE=.\core_api.h
# End Source File
# Begin Source File

SOURCE=.\coreversion.h
# End Source File
# Begin Source File

SOURCE=.\cvt_float_to_linear.h
# End Source File
# Begin Source File

SOURCE=.\diskwriter.cpp
# End Source File
# Begin Source File

SOURCE=.\diskwriter.h
# End Source File
# Begin Source File

SOURCE=.\dsp.cpp
# End Source File
# Begin Source File

SOURCE=.\dsp.h
# End Source File
# Begin Source File

SOURCE=.\dsp_manager.h
# End Source File
# Begin Source File

SOURCE=.\file_info.cpp
# End Source File
# Begin Source File

SOURCE=.\file_info.h
# End Source File
# Begin Source File

SOURCE=.\file_info_helper.h
# End Source File
# Begin Source File

SOURCE=.\foobar2000.h
# End Source File
# Begin Source File

SOURCE=.\guids.cpp
# End Source File
# Begin Source File

SOURCE=.\initquit.h
# End Source File
# Begin Source File

SOURCE=.\input.cpp
# End Source File
# Begin Source File

SOURCE=.\input.h
# End Source File
# Begin Source File

SOURCE=.\input_helpers.cpp
# End Source File
# Begin Source File

SOURCE=.\input_helpers.h
# End Source File
# Begin Source File

SOURCE=.\menu_item.h
# End Source File
# Begin Source File

SOURCE=.\menu_manager.cpp
# End Source File
# Begin Source File

SOURCE=.\menu_manager.h
# End Source File
# Begin Source File

SOURCE=.\metadb.cpp
# End Source File
# Begin Source File

SOURCE=.\metadb.h
# End Source File
# Begin Source File

SOURCE=.\metadb_handle.cpp
# End Source File
# Begin Source File

SOURCE=.\metadb_handle.h
# End Source File
# Begin Source File

SOURCE=.\modeless_dialog.cpp
# End Source File
# Begin Source File

SOURCE=.\modeless_dialog.h
# End Source File
# Begin Source File

SOURCE=.\output.cpp
# End Source File
# Begin Source File

SOURCE=.\output.h
# End Source File
# Begin Source File

SOURCE=.\output_manager.h
# End Source File
# Begin Source File

SOURCE=.\packet_decoder.cpp
# End Source File
# Begin Source File

SOURCE=.\packet_decoder.h
# End Source File
# Begin Source File

SOURCE=.\play_callback.h
# End Source File
# Begin Source File

SOURCE=.\play_control.cpp
# End Source File
# Begin Source File

SOURCE=.\play_control.h
# End Source File
# Begin Source File

SOURCE=.\playable_location.cpp
# End Source File
# Begin Source File

SOURCE=.\playable_location.h
# End Source File
# Begin Source File

SOURCE=.\playback_core.h
# End Source File
# Begin Source File

SOURCE=.\playlist.cpp
# End Source File
# Begin Source File

SOURCE=.\playlist.h
# End Source File
# Begin Source File

SOURCE=.\playlist_entry.h
# End Source File
# Begin Source File

SOURCE=.\playlist_loader.cpp
# End Source File
# Begin Source File

SOURCE=.\playlist_loader.h
# End Source File
# Begin Source File

SOURCE=.\playlist_switcher.h
# End Source File
# Begin Source File

SOURCE=.\reader.cpp
# End Source File
# Begin Source File

SOURCE=.\reader.h
# End Source File
# Begin Source File

SOURCE=.\reader_helper.h
# End Source File
# Begin Source File

SOURCE=.\reader_helper_mem.h
# End Source File
# Begin Source File

SOURCE=.\replaygain.h
# End Source File
# Begin Source File

SOURCE=.\resampler.h
# End Source File
# Begin Source File

SOURCE=.\service.cpp
# End Source File
# Begin Source File

SOURCE=.\service.h
# End Source File
# Begin Source File

SOURCE=.\service_helper.h
# End Source File
# Begin Source File

SOURCE=.\service_impl.h
# End Source File
# Begin Source File

SOURCE=.\stdafx.cpp

!IF  "$(CFG)" == "foobar2000_SDK - Win32 Release"

# ADD CPP /Yc

!ELSEIF  "$(CFG)" == "foobar2000_SDK - Win32 Debug"

# ADD CPP /Yc"foobar2000.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\tagread.cpp
# End Source File
# Begin Source File

SOURCE=.\tagread.h
# End Source File
# Begin Source File

SOURCE=.\titleformat.cpp
# End Source File
# Begin Source File

SOURCE=.\titleformat.h
# End Source File
# Begin Source File

SOURCE=.\ui.cpp
# End Source File
# Begin Source File

SOURCE=.\ui.h
# End Source File
# Begin Source File

SOURCE=.\unpack.h
# End Source File
# Begin Source File

SOURCE=.\utf8api.cpp
# End Source File
# Begin Source File

SOURCE=.\utf8api.h
# End Source File
# Begin Source File

SOURCE=.\vis.h
# End Source File
# End Target
# End Project
