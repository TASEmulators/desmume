# Build the DeSmuME core (everything except the GUI front ends) into a static
# library, without MSBuild.
#
# Why: the differential harness links against the emulator core, and MSBuild is
# not available in every environment this repo gets built in. Compiling to a
# .lib also means the linker only pulls the objects a harness actually reaches,
# so the movie/archive code (7z, unrar) never gets dragged in.
#
# Usage:
#   pwsh -File tools/jit_matrix/build_core.ps1            # build
#   pwsh -File tools/jit_matrix/build_core.ps1 -Clean     # wipe obj dir first
#
# Output:
#   build_core/obj/*.obj
#   build_core/desmume_core.lib
#   build_core/failures.txt   (only when something failed)

param(
    [switch]$Clean,
    [ValidateSet('Debug', 'Release')][string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'

$Repo    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project = Join-Path $Repo 'desmume'
$Src     = Join-Path $Project 'src'
$Vcxproj = Join-Path $Src 'frontend\windows\DeSmuME.vcxproj'
$Out     = Join-Path $Repo 'build_core'
$ObjDir  = Join-Path $Out 'obj'

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
$Sdk = Join-Path $SdkRoot "Include\$SdkVer"
$SdkLib = Join-Path $SdkRoot "Lib\$SdkVer"

$BinDir = Join-Path $Msvc 'bin\Hostx64\x64'
$Cl  = Join-Path $BinDir 'cl.exe'
$Lib = Join-Path $BinDir 'lib.exe'

Write-Host "toolchain : MSVC $MsvcVer  SDK $SdkVer"
Write-Host "project   : $Vcxproj"

$env:INCLUDE = "$($Msvc)\include;$Sdk\ucrt;$Sdk\shared;$Sdk\um;$Sdk\winrt"
$env:LIB     = "$($Msvc)\lib\x64;$SdkLib\ucrt\x64;$SdkLib\um\x64"

if ($Clean -and (Test-Path $ObjDir)) { Remove-Item $ObjDir -Recurse -Force }
foreach ($d in @($Out, $ObjDir)) {
    if (-not (Test-Path $d)) { New-Item -ItemType Directory -Force -Path $d | Out-Null }
}

# --- source list, taken from the shipping project so it cannot drift -------
# Items may carry <ExcludedFromBuild>true</ExcludedFromBuild> per x64 config
# (e.g. the AltiVec/AVX2/SSE2 colour-space variants). Those must be honoured,
# otherwise we compile translation units the real build never touches.
$vcxLines = Get-Content $Vcxproj
$includes = @()
for ($n = 0; $n -lt $vcxLines.Count; $n++) {
    if ($vcxLines[$n] -notmatch '<ClCompile Include="([^"]+)"') { continue }
    $include = $Matches[1]
    $selfClosing = $vcxLines[$n] -match '/>\s*$'

    $excluded = $false
    if (-not $selfClosing) {
        for ($m = $n + 1; $m -lt [Math]::Min($n + 12, $vcxLines.Count); $m++) {
            if ($vcxLines[$m] -match '</ClCompile>') { break }
            if ($vcxLines[$m] -match 'ExcludedFromBuild[^>]*>\s*true\s*<') { $excluded = $true; break }
        }
    }
    if (-not $excluded) { $includes += $include }
}

$baseDir = Join-Path $Src 'frontend\windows'
$frontendDir = (Join-Path $Src 'frontend') + [IO.Path]::DirectorySeparatorChar

# Core-adjacent files that only the GUI scripting front end needs. They live
# outside src/frontend/, so the path filter above does not catch them.
$excludeNames = @('lua-engine.cpp')

$sources = foreach ($inc in $includes) {
    $full = [IO.Path]::GetFullPath((Join-Path $baseDir $inc))
    # GUI translation units are not part of the core library.
    if ($full.StartsWith($frontendDir, [StringComparison]::OrdinalIgnoreCase)) { continue }
    if ($excludeNames -contains [IO.Path]::GetFileName($full)) { continue }
    if (-not (Test-Path $full)) { Write-Warning "missing source: $inc"; continue }
    $full
}
$sources = $sources | Sort-Object -Unique
Write-Host "sources   : $($sources.Count)"

# --- include paths (only the ones that exist) ------------------------------
$incCandidates = @(
    $Src
    (Join-Path $Src 'libretro-common\include')
    $baseDir
    (Join-Path $baseDir 'zlib128')
    (Join-Path $baseDir 'zziplib')
    (Join-Path $baseDir 'glib-2.20.1\build')
    (Join-Path $baseDir 'agg\include')
    (Join-Path $baseDir 'agg\examples')
    (Join-Path $baseDir 'directx')
    (Join-Path $baseDir 'winpcap')
    (Join-Path $baseDir 'userconfig')
    (Join-Path $baseDir 'defaultconfig')
    (Join-Path $Project '3rdparty\wgpu')
)
$incArgs = @($incCandidates | Where-Object { Test-Path $_ } | ForEach-Object { "/I$_" })

# No HAVE_LIBAGG / HAVE_LUA / ENABLE_WGPU: the core must not depend on the GUI.
$defArgs = @(
    '/D_CRT_SECURE_NO_DEPRECATE'
    '/DWIN32'
    '/DNOMINMAX'
    '/DHAVE_JIT'
)

$optArgs = if ($Config -eq 'Debug') { @('/Od', '/Zi') } else { @('/O2') }
$baseArgs = @('/c', '/nologo', '/EHsc', '/W1', '/std:c++14') + $optArgs + $defArgs + $incArgs

# --- compile ---------------------------------------------------------------
$failures = @()
$i = 0
$sw = [Diagnostics.Stopwatch]::StartNew()

foreach ($s in $sources) {
    $i++
    $rel = $s.Substring($Src.Length + 1)
    $objName = ('{0:d3}_{1}' -f $i, ($rel -replace '[\\/:*?"<>|]', '_')) -replace '\.cpp$', '.obj'
    $obj = Join-Path $ObjDir $objName

    $log = & $Cl @baseArgs "/Fo$obj" $s 2>&1
    if ($LASTEXITCODE -ne 0) {
        $failures += [pscustomobject]@{ file = $rel; log = ($log | Select-Object -Last 8) -join "`n" }
        Write-Host ("  FAIL  {0}" -f $rel) -ForegroundColor Red
    } elseif ($i % 25 -eq 0) {
        Write-Host ("  ok {0}/{1}  {2:n0}s" -f $i, $sources.Count, $sw.Elapsed.TotalSeconds)
    }
}

$sw.Stop()
Write-Host ""
Write-Host ("compiled {0}/{1} in {2:n0}s, {3} failed" -f ($sources.Count - $failures.Count), $sources.Count, $sw.Elapsed.TotalSeconds, $failures.Count)

if ($failures.Count) {
    $failures | ForEach-Object { "=== $($_.file)`n$($_.log)" } | Set-Content (Join-Path $Out 'failures.txt') -Encoding utf8
    Write-Host "see build_core/failures.txt" -ForegroundColor Yellow
    exit 1
}

# --- archive ---------------------------------------------------------------
# NB: PowerShell variables are case-insensitive, so this must not be called
# $lib -- that would silently clobber $Lib (lib.exe) from above.
$libPath = Join-Path $Out 'desmume_core.lib'
if (Test-Path $libPath) { Remove-Item $libPath -Force }
$objs = Get-ChildItem $ObjDir -Filter *.obj | ForEach-Object { $_.FullName }
$rsp = Join-Path $Out 'lib.rsp'
$objs | Set-Content $rsp -Encoding ascii

& $Lib /NOLOGO /OUT:"$libPath" "@$rsp"
if ($LASTEXITCODE -ne 0) { throw "lib.exe failed" }

Write-Host ("archived  : $libPath  ({0:n1} MB, {1} objects)" -f ((Get-Item $libPath).Length / 1MB), $objs.Count)
