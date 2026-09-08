# Build and run export_pool.cpp with MSVC, producing a raw .iop file from
# VTObjectPool.cpp's real hand-authored pool -- open the result in
# AgIsoTerminalDesigner (https://open-agriculture.github.io/AgIsoTerminalDesigner/)
# to inspect/edit it visually.
#
# Mirrors test/native/run_tests_msvc.ps1's vcvars-detection pattern (this
# machine has no GCC on PATH, MSVC is the working native-build path here).
#
# Usage: .\tools\vt_pool\export_pool_msvc.ps1 [output .iop path]

$root   = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$stubs  = "$root\test\native\support"
$isobus = "$root\lib\PloegbesturingCore\src\isobus"
$src    = "$PSScriptRoot\export_pool.cpp"
# export_pool.cpp includes VTObjectPool.hpp via a relative path now (matches
# the rest of the project's no-search-path convention), so $isobus is only
# needed below to name VTObjectPool.cpp's own location for the compile line,
# not as an /I flag.
$out    = "$env:TEMP\vt_pool_export"

$outIop = if ($args.Count -gt 0) { $args[0] } else { "$root\tools\vt_pool\generated\plough_pool.iop" }

New-Item -ItemType Directory -Force -Path $out | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $outIop -Parent) | Out-Null

$vcvars = $null
$vcvarsArch = ""
$candidates = @(
    @{ path = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"; arch = "" },
    @{ path = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"; arch = "x64" }
)
foreach ($c in $candidates) {
    if (Test-Path $c.path) { $vcvars = $c.path; $vcvarsArch = $c.arch; break }
}
if (-not $vcvars) {
    Write-Error "No supported Visual Studio installation found."
    exit 1
}

# -D ARDUINO=1: VTObjectPool.hpp/.cpp gate their whole content behind
# #ifdef ARDUINO. Real Arduino/PlatformIO builds get this from the
# framework; the native stub deliberately doesn't define it (see
# test/native/support/Arduino.h's own comment), so it must be passed here.
$commonFlags = "/std:c++17 /Zc:preprocessor /EHsc /nologo /W1 /DARDUINO=1 /I`"$stubs`""

Write-Host ""
Write-Host "=== Building export_pool ===" -ForegroundColor Cyan

$cmd = "`"$vcvars`" $vcvarsArch && cl $commonFlags /Fo`"$out\\`" `"$isobus\VTObjectPool.cpp`" `"$src`" /Fe:`"$out\export_pool.exe`" && `"$out\export_pool.exe`" `"$outIop`""
cmd /c $cmd

exit $LASTEXITCODE
