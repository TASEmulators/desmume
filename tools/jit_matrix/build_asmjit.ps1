# Build the vendored asmjit (master, 3rdparty/asmjit) as a static library, then
# build the S2 skeleton probe against it.
#
# This does NOT touch the production build. It exists to answer one question
# before S2 starts rewriting arm_jit.cpp: do the asmjit master APIs that the
# migration plan relies on actually exist and behave as described?
#
# Usage:
#   pwsh -File tools/jit_matrix/build_asmjit.ps1              # lib + probe
#   pwsh -File tools/jit_matrix/build_asmjit.ps1 -LibOnly     # lib only
#
# Output: build_core/asmjit/asmjit_master.lib
#         build_core/asmjit_probe.exe

param(
    [ValidateSet('Debug', 'Release')][string]$Config = 'Release',
    [switch]$LibOnly,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$Repo   = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$AsmJit = Join-Path $Repo 'desmume\3rdparty\asmjit'
$Inc    = Join-Path $Repo 'desmume\3rdparty'
$Out    = Join-Path $Repo 'build_core\asmjit'
$Lib    = Join-Path $Out 'asmjit_master.lib'

if (-not (Test-Path $AsmJit)) { throw "missing vendored asmjit at $AsmJit" }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

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
$Cl  = Join-Path $BinDir 'cl.exe'
$Lib2 = Join-Path $BinDir 'lib.exe'

$env:INCLUDE = "$($Msvc)\include;$Sdk\ucrt;$Sdk\shared;$Sdk\um"
$env:LIB     = "$($Msvc)\lib\x64;$SdkLib\ucrt\x64;$SdkLib\um\x64"

# --- compile asmjit --------------------------------------------------------
# gen_asmjit_sources.py counts 54 compile units: everything except *_test.cpp
# and the experimental ujit directory (headers only, sources not built).
$sources = Get-ChildItem $AsmJit -Recurse -Filter *.cpp | Where-Object {
    $_.FullName -notmatch '\\ujit\\' -and $_.Name -notmatch '_test\.cpp$'
} | Sort-Object FullName

Write-Host "asmjit compile units: $($sources.Count)"
if ($sources.Count -ne 54) { throw "expected 54 compile units, found $($sources.Count)" }

$optArgs = if ($Config -eq 'Debug') { @('/Od', '/Zi') } else { @('/O2') }
$common = @('/c', '/nologo', '/EHsc', '/std:c++20', '/W1') + $optArgs + @(
    '/DASMJIT_STATIC', '/D_CRT_SECURE_NO_WARNINGS', "/I$Inc"
)

if ($Force -or -not (Test-Path $Lib)) {
    $objs = @()
    $i = 0
    foreach ($s in $sources) {
        $i++
        $rel = $s.FullName.Substring($AsmJit.Length + 1).Replace('\', '_')
        $obj = Join-Path $Out ('aj_' + [IO.Path]::GetFileNameWithoutExtension($rel) + '.obj')
        if ($Force -or -not (Test-Path $obj) -or
            ((Get-Item $obj).LastWriteTime -lt $s.LastWriteTime)) {
            & $Cl @common "/Fo$obj" $s.FullName
            if ($LASTEXITCODE -ne 0) { throw "compile failed: $($s.FullName)" }
            Write-Host ("  [{0,2}/{1}] {2}" -f $i, $sources.Count, $s.FullName.Substring($AsmJit.Length + 1))
        }
        $objs += $obj
    }
    & $Lib2 /nologo "/OUT:$Lib" @objs
    if ($LASTEXITCODE -ne 0) { throw 'lib failed' }
    Write-Host "built: $Lib"
} else {
    Write-Host "up to date: $Lib"
}

if ($LibOnly) { return }

# --- build the probe -------------------------------------------------------
$probe = Join-Path $PSScriptRoot 'asmjit_probe.cpp'
if (-not (Test-Path $probe)) { throw "missing $probe" }

$exe = Join-Path $Repo 'build_core\asmjit_probe.exe'
$map = Join-Path $Repo 'build_core\asmjit_probe.map'
$obj = Join-Path $Out 'asmjit_probe.obj'
$probeArgs = @('/nologo', '/EHsc', '/std:c++20', '/W2') + $optArgs + @(
    '/DASMJIT_STATIC', "/I$Inc", '/c', "/Fo$obj", $probe
)
& $Cl @probeArgs
if ($LASTEXITCODE -ne 0) { throw 'probe compile failed' }

& $Cl '/nologo' $obj "/Fe$exe" '/link' "/MAP:$map" $Lib 'kernel32.lib'
if ($LASTEXITCODE -ne 0) { throw 'probe link failed' }

Write-Host "built: $exe"
