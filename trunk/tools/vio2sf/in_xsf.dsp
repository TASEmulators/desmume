# Microsoft Developer Studio Project File - Name="in_xsf" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=in_xsf - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "in_xsf.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "in_xsf.mak" CFG="in_xsf - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "in_xsf - Win32 Release" ("Win32 (x86) Dynamic-Link Library" 用)
!MESSAGE "in_xsf - Win32 Debug" ("Win32 (x86) Dynamic-Link Library" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "in_xsf - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_in_xsf"
# PROP Intermediate_Dir "Release_in_xsf"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_XSF_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_XSF_EXPORTS" /FD /c
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
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:none /machine:I386 /out:"in_vio2sf.dll"

!ELSEIF  "$(CFG)" == "in_xsf - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_in_xsf"
# PROP Intermediate_Dir "Debug_in_xsf"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_XSF_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "src/aosdk" /I "src/aosdk/zlib" /D "LSB_FIRST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IN_XSF_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"C:\Program Files\Winamp\Plugins/in_vio2sf.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "in_xsf - Win32 Release"
# Name "in_xsf - Win32 Debug"
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
