# Verify the AArch64 primitive layer after the asmjit migration.
#
# The AArch64 backend cannot be executed on an x86 host, so equivalence is
# established on the generated code instead: every primitive is dumped once with
# the asmjit encoder and compared, instruction by instruction, against the frozen
# output of the hand-written encoder (a64_golden.txt).
#
# Usage:
#   pwsh -File tools/jit_matrix/check_a64.ps1            # rebuild + check
#   pwsh -File tools/jit_matrix/check_a64.ps1 -SkipLib   # reuse the asmjit lib
#
# Output: 0 when all cases match, 1 otherwise.

param([switch]$SkipLib)

$ErrorActionPreference = 'Stop'

$Repo   = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$AsmJit = Join-Path $Repo 'desmume\3rdparty\asmjit'
$Inc    = Join-Path $Repo 'desmume\3rdparty'
$Out    = Join-Path $Repo 'build_core'
$LibDir = Join-Path $Out 'asmjit_clang'
$Lib    = Join-Path $LibDir 'libasmjit.a'

$Clang  = 'C:\msys64\clang64\bin\clang++.exe'
$Ar     = 'C:\msys64\clang64\bin\llvm-ar.exe'
$Python = 'C:\Users\zhang\scoop\apps\python\current\python.exe'

New-Item -ItemType Directory -Force -Path $LibDir | Out-Null

# --- asmjit, built with the same compiler as the tools ---------------------
# The MSVC-built asmjit_master.lib cannot be linked by clang, and the MSVC link
# step is not available in this environment, so the verification tools get their
# own copy. Production still uses asmjit_master.lib via build_diff.ps1.
if (-not $SkipLib) {
    $srcs = Get-ChildItem -Recurse -Path $AsmJit -Filter *.cpp |
            Where-Object { $_.Name -notlike '*_test.cpp' -and $_.DirectoryName -notlike '*\ujit' }
    $objs = @()
    foreach ($s in $srcs) {
        $o = Join-Path $LibDir ($s.BaseName + '.o')
        & $Clang -std=c++20 -O1 -w -I $Inc -DASMJIT_STATIC -c -o $o $s.FullName
        if ($LASTEXITCODE -ne 0) { throw "asmjit: $($s.Name) failed" }
        $objs += $o
    }
    if (Test-Path $Lib) { Remove-Item $Lib -Force }
    & $Ar rcs $Lib $objs
    if ($LASTEXITCODE -ne 0) { throw 'asmjit archive failed' }
    Write-Output "asmjit: $($srcs.Count) TUs -> $Lib"
}

# --- the two checks --------------------------------------------------------
$common = @('-std=c++20','-O1','-w','-I',$Inc,'-DASMJIT_STATIC')

& $Clang @common -o (Join-Path $Out 'a64_dump_new.exe') `
    (Join-Path $Repo 'tools\jit_matrix\a64_dump_new.cpp') $Lib
if ($LASTEXITCODE -ne 0) { throw 'a64_dump_new failed to build' }

$dump = Join-Path $Out 'a64_new.txt'
& (Join-Path $Out 'a64_dump_new.exe') > $dump 2> (Join-Path $Out 'a64_new.err.txt')
if ($LASTEXITCODE -ne 0) { throw 'a64_dump_new failed to run' }

$stderr = Get-Content (Join-Path $Out 'a64_new.err.txt') -Raw
if ($stderr -and $stderr.Trim().Length -gt 0) {
    Write-Output "encoder reported errors:`n$stderr"
    exit 1
}

& $Python (Join-Path $Repo 'tools\jit_matrix\check_a64.py') `
    (Join-Path $Repo 'tools\jit_matrix\a64_golden.txt') $dump
$encodings = $LASTEXITCODE

& $Clang @common -o (Join-Path $Out 'a64_shape_probe.exe') `
    (Join-Path $Repo 'tools\jit_matrix\a64_shape_probe.cpp') $Lib
if ($LASTEXITCODE -ne 0) { throw 'a64_shape_probe failed to build' }

& (Join-Path $Out 'a64_shape_probe.exe') | Out-Null
$shape = $LASTEXITCODE

if ($encodings -ne 0 -or $shape -ne 0) {
    Write-Output "FAILED (encodings=$encodings shape=$shape)"
    exit 1
}

Write-Output 'ok: encodings and block shape both match'
exit 0
