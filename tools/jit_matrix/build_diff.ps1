# Build the JIT differential harness.
#
# Links against the core static library produced by build_core.ps1, so run that
# first (or let this script report that it is missing).
#
# Usage:
#   pwsh -File tools/jit_matrix/build_diff.ps1
#
# Output: build_core/jit_diff.exe

param(
    [ValidateSet('Debug', 'Release')][string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'

$Repo    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project = Join-Path $Repo 'desmume'
$Src     = Join-Path $Project 'src'
$Out     = Join-Path $Repo 'build_core'
$CoreLib = Join-Path $Out 'desmume_core.lib'
$AsmJitLib = Join-Path $Out 'asmjit\asmjit_master.lib'

if (-not (Test-Path $CoreLib)) {
    throw "missing $CoreLib - run tools/jit_matrix/build_core.ps1 first"
}
if (-not (Test-Path $AsmJitLib)) {
    throw "missing $AsmJitLib - run tools/jit_matrix/build_asmjit.ps1 first"
}

# --- toolchain -------------------------------------------------------------
$VsRoot = $null
foreach ($c in @('C:\Program Files\Microsoft Visual Studio\18\Insiders',
                 'C:\Program Files\Microsoft Visual Studio\2022\Community',
                 'C:\Program Files\Microsoft Visual Studio\2022\Professional',
                 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise')) {
    if (Test-Path (Join-Path $c 'VC\Tools\MSVC')) { $VsRoot = $c; break }
}
if (-not $VsRoot) { throw 'no Visual Studio with C++ tools found' }

$MsvcVer = (Get-ChildItem (Join-Path $VsRoot 'VC\Tools\MSVC') -Directory |
            Sort-Object Name -Descending | Select-Object -First 1).Name
$Msvc = Join-Path $VsRoot "VC\Tools\MSVC\$MsvcVer"

$SdkRoot = 'C:\Program Files (x86)\Windows Kits\10'
$SdkVer  = (Get-ChildItem (Join-Path $SdkRoot 'Include') -Directory |
            Sort-Object Name -Descending | Select-Object -First 1).Name
$Sdk     = Join-Path $SdkRoot "Include\$SdkVer"
$SdkLib  = Join-Path $SdkRoot "Lib\$SdkVer"

$BinDir = Join-Path $Msvc 'bin\Hostx64\x64'
$Cl = Join-Path $BinDir 'cl.exe'

$env:INCLUDE = "$($Msvc)\include;$Sdk\ucrt;$Sdk\shared;$Sdk\um;$Sdk\winrt"
$env:LIB     = "$($Msvc)\lib\x64;$SdkLib\ucrt\x64;$SdkLib\um\x64"

# --- sources ---------------------------------------------------------------
$DiffDir = Join-Path $Src 'utils\jit_diff'
$sources = Get-ChildItem $DiffDir -Filter *.cpp | ForEach-Object { $_.FullName }
if (-not $sources) { throw "no sources in $DiffDir" }

# zlib's crc32, which NDS_LoadROM references. The libretro-common copy is a
# standalone test program (it has its own main), so the zlib source is used.
$crc32 = Join-Path $Project '3rdparty\zlib-1.4\crc32.c'
if (Test-Path $crc32) { $sources += $crc32 }

$incArgs = @(
    "/I$Src"
    "/I$(Join-Path $Src 'libretro-common\include')"
    "/I$(Join-Path $Src 'frontend\windows')"
    "/I$(Join-Path $Project '3rdparty\zlib-1.4')"
)

$optArgs = if ($Config -eq 'Debug') { @('/Od', '/Zi') } else { @('/O2') }
$baseArgs = @('/c', '/nologo', '/EHsc', '/W2', '/std:c++14') + $optArgs + @(
    '/D_CRT_SECURE_NO_DEPRECATE', '/DWIN32', '/DNOMINMAX', '/DHAVE_JIT'
) + $incArgs

$objs = @()
foreach ($s in $sources) {
    $obj = Join-Path $Out ('diff_' + [IO.Path]::GetFileNameWithoutExtension($s) + '.obj')
    & $Cl @baseArgs "/Fo$obj" $s
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $s" }
    $objs += $obj
}

# System libraries the core references through Windows APIs.
$sysLibs = @(
    'winmm.lib', 'dsound.lib', 'dxguid.lib', 'ole32.lib', 'uuid.lib',
    'advapi32.lib', 'user32.lib', 'gdi32.lib', 'shell32.lib', 'comdlg32.lib',
    'version.lib', 'ws2_32.lib'
)

$exe = Join-Path $Out 'jit_diff.exe'
# /MAP: the harness reports fault addresses, and without a map a fault is just a
# number. RVAs are enough -- no PDB needed to name the function that faulted.
$map = Join-Path $Out 'jit_diff.map'
& $Cl /nologo @objs "/Fe$exe" "/link" "/MAP:$map" "/LIBPATH:$Out" 'desmume_core.lib' $AsmJitLib @sysLibs
if ($LASTEXITCODE -ne 0) { throw 'link failed' }

Write-Host "built: $exe"
