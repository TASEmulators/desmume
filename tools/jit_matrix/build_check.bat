@echo off
REM ---------------------------------------------------------------
REM S0 prerequisite check: can the JIT front end + the vendored asmjit
REM compile with the installed MSVC toolchain, without going through
REM MSBuild?
REM
REM Why not MSBuild: this only answers "does the toolchain accept the
REM code", which is the actual migration risk. It does not link and it
REM does not touch the GUI front end.
REM
REM Usage:  build_check.bat [Release^|Debug]
REM Exit:   0 = every translation unit compiled
REM ---------------------------------------------------------------
setlocal enabledelayedexpansion

set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

REM --- locate the newest MSVC toolset and Windows SDK -----------------
set VSDIR=
for /d %%D in ("C:\Program Files\Microsoft Visual Studio\18\Insiders" "C:\Program Files\Microsoft Visual Studio\2022\*") do (
  if exist "%%~D\VC\Tools\MSVC" set VSDIR=%%~D
)
if "%VSDIR%"=="" (
  echo error: no Visual Studio with C++ tools found
  exit /b 1
)

set MSVCVER=
for /d %%D in ("%VSDIR%\VC\Tools\MSVC\*") do set MSVCVER=%%~nxD
set MSVC=%VSDIR%\VC\Tools\MSVC\%MSVCVER%

set SDKROOT=C:\Program Files (x86)\Windows Kits\10
set SDKVER=
for /d %%D in ("%SDKROOT%\Include\*") do set SDKVER=%%~nxD

echo toolchain: MSVC %MSVCVER%  SDK %SDKVER%
if not exist "%MSVC%\bin\Hostx64\x64\cl.exe" (
  echo error: cl.exe not found under %MSVC%
  exit /b 1
)

set INCLUDE=%MSVC%\include;%SDKROOT%\Include\%SDKVER%\ucrt;%SDKROOT%\Include\%SDKVER%\shared;%SDKROOT%\Include\%SDKVER%\um;%SDKROOT%\Include\%SDKVER%\winrt
set LIB=%MSVC%\lib\x64;%SDKROOT%\Lib\%SDKVER%\ucrt\x64;%SDKROOT%\Lib\%SDKVER%\um\x64
set PATH=%MSVC%\bin\Hostx64\x64;%PATH%

set ROOT=%~dp0..\..
set SRC=%ROOT%\desmume\src
set OUT=%ROOT%\build_check
if not exist "%OUT%" mkdir "%OUT%"

REM arm_jit.cpp uses C++20 through the vendored asmjit headers; asmjit 1.0
REM (formerly src\utils\AsmJit) was removed after the migration (plan S7.2).
set CFLAGS=/c /nologo /EHsc /W1 /std:c++20 /D_CRT_SECURE_NO_DEPRECATE /DWIN32 /DNOMINMAX /DASMJIT_STATIC
set DEFS=/DHAVE_JIT /DHAVE_LIBZ /DHAVE_ZLIB_DEFLATE
set INCS=/I"%ROOT%\desmume\3rdparty" /I"%SRC%" /I"%SRC%\libretro-common\include" /I"%SRC%\frontend\windows" /I"%SRC%\frontend\windows\zlib128"

if /I "%CONFIG%"=="Debug" (
  set CFLAGS=%CFLAGS% /Od /Zi /D_DEBUG
) else (
  set CFLAGS=%CFLAGS% /O2 /DRELEASE /DNDEBUG
)

set FAILED=0

echo.
echo === [1/2] vendored asmjit (3rdparty\asmjit, via build_asmjit.ps1) ===
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_asmjit.ps1" -Config %CONFIG% -LibOnly
if errorlevel 1 (
  echo   FAIL asmjit build
  set FAILED=1
) else (
  echo   OK   asmjit build
)

echo.
echo === [2/2] arm_jit.cpp (x86 host backend) ===
cl %CFLAGS% %DEFS% %INCS% "%SRC%\arm_jit.cpp" /Fo"%OUT%\arm_jit.obj" > "%OUT%\log.tmp" 2>&1
findstr /i /c:"error" "%OUT%\log.tmp" >nul
if errorlevel 1 (
  echo   OK   arm_jit.cpp
) else (
  echo   FAIL arm_jit.cpp
  findstr /i /c:"error" "%OUT%\log.tmp"
  set FAILED=1
)

echo.
if "%FAILED%"=="0" (
  echo RESULT: PASS - vendored asmjit + arm_jit.cpp compiled
  exit /b 0
) else (
  echo RESULT: FAIL - see messages above
  exit /b 1
)
