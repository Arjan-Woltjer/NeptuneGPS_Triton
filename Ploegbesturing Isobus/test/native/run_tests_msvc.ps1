# Run all AUnit tests using MSVC (Visual Studio 2022).
# Use this on Windows when GCC / pio run -e native is not available.
#
# Usage: .\test\native\run_tests_msvc.ps1

$root    = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$aunit   = "$root\.pio\libdeps\native\AUnit\src"
$stubs   = "$root\test\native\support"
$lib     = "$root\lib\PloegbesturingCore\src"
$guidance = "$root\..\MeijWorks Libs\VehicleGuidance"   # shared GuidanceSource + parsers, NeptuneGPS_Triton#76
$implement = "$root\lib\PloegbesturingCore\src\implement"
# The real AgIsoStack, compiled for the host (NeptuneGPS_Triton#98). Its core
# is portable C++17; only flex_can_t4_plugin.cpp is Teensy-bound and is
# filtered out below. Fetched by "pio pkg install -e native".
$agisostack = "$root\.pio\libdeps\native\AgIsoStack\src"
$test    = "$root\test\native\tests"
$driver  = "$root\test\native\PloegbesturingNativeTests.cpp"
$out     = "$env:TEMP\msvc_test"

New-Item -ItemType Directory -Force -Path $out | Out-Null

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

$aunitSources = @(
    "`"$aunit\aunit\Assertion.cpp`"",
    "`"$aunit\aunit\Compare.cpp`"",
    "`"$aunit\aunit\FCString.cpp`"",
    "`"$aunit\aunit\MetaAssertion.cpp`"",
    "`"$aunit\aunit\print64.cpp`"",
    "`"$aunit\aunit\Printer.cpp`"",
    "`"$aunit\aunit\string_util.cpp`"",
    "`"$aunit\aunit\Test.cpp`"",
    "`"$aunit\aunit\TestAgain.cpp`"",
    "`"$aunit\aunit\TestOnce.cpp`"",
    "`"$aunit\aunit\TestRunner.cpp`""
) -join ' '

# /Zc:preprocessor switches on MSVC's conformant preprocessor -- required for
# AUnit's two-argument test(SuiteName, testName) macros used throughout
# test_*.cpp: they dispatch on argument count via
# GET_TEST(__VA_ARGS__, TEST2, TEST1)(__VA_ARGS__), which MSVC's legacy
# (default) preprocessor expands wrong (see Salacia's/Spuitcomputer LD's platformio.ini/
# test README for the same issue, hit there first).
# The only PloegbesturingCore -I needed is $lib itself -- every cross-file
# include inside the library is a relative path (matching Salacia's
# SalaciaFirmwareCore convention), and $lib is only for the test_*.cpp files
# below reaching in via library-root-relative paths ("implement/
# ImplementPlough.hpp" etc.).
$commonFlags = "/std:c++17 /Zc:preprocessor /EHsc /nologo /W1 /DEPOXY_DUINO=1 /DISOBUS /DCAN_STACK_DISABLE_THREADS /I`"$aunit`" /I`"$agisostack`" /I`"$stubs`" /I`"$lib`" /I`"$guidance`""

# ---- combined native test binary --------------------------------------------
# One binary for every test_*.cpp under test/native/tests/ -- matches
# platformio.ini's [env:native] build_src_filter (single driver file,
# PloegbesturingNativeTests.cpp, provides setup()/loop()).
Write-Host ""
Write-Host "=== Building PloegbesturingNativeTests ===" -ForegroundColor Cyan

$testSources = (Get-ChildItem "$test\test_*.cpp" | ForEach-Object { "`"$($_.FullName)`"" }) -join ' '

# Every AgIsoStack core source except the one Teensy-bound plugin, matching
# platformio.ini's glob-plus-exclusion for [env:native].
$agisostackSources = (Get-ChildItem "$agisostack\*.cpp" |
    Where-Object { $_.Name -ne 'flex_can_t4_plugin.cpp' } |
    ForEach-Object { "`"$($_.FullName)`"" }) -join ' '

# Through a response file, not the command line: AgIsoStack adds 40 source
# paths and the whole thing runs past cmd.exe's length limit ("The command
# line is too long"). cl accepts @file with identical parsing.
$clArgs = @(
    $commonFlags
    "/Fo`"$out\\`""
    "`"$lib\isobus\IsobusTcInterface.cpp`""
    "`"$lib\isobus\IsobusGuidanceChannel.cpp`""
    "`"$lib\isobus\IsobusVtInterface.cpp`""
    $agisostackSources
    "`"$implement\ImplementPlough.cpp`""
    "`"$lib\InterfacePlough.cpp`""
    "`"$lib\calibration\CalibrationPlough.cpp`""
    "`"$lib\isobus\VTObjectPool.cpp`""
    "`"$guidance\NmeaParser.cpp`""
    "`"$guidance\TrimbleParser.cpp`""
    "`"$guidance\CanSerialParser.cpp`""
    "`"$guidance\SerialGuidanceChannel.cpp`""
    "`"$guidance\IsobusPgnDecode.cpp`""
    "`"$guidance\CanFrameGuidanceChannel.cpp`""
    $testSources
    "`"$driver`""
    "`"$stubs\native_main.cpp`""
    $aunitSources
    "/Fe:`"$out\PloegbesturingNativeTests.exe`""
) -join ' '

$rsp = "$out\cl_args.rsp"
Set-Content -Path $rsp -Value $clArgs -Encoding ascii

$cmd = "`"$vcvars`" $vcvarsArch && cl @`"$rsp`" && `"$out\PloegbesturingNativeTests.exe`""
Write-Host "Running..." -ForegroundColor Cyan
cmd /c $cmd

$overallExit = if ($LASTEXITCODE -ne 0) { 1 } else { 0 }

# ---- Summary ---------------------------------------------------------------
Write-Host ""
if ($overallExit -eq 0) {
    Write-Host "All test suites PASSED." -ForegroundColor Green
} else {
    Write-Host "One or more test suites FAILED." -ForegroundColor Red
}
exit $overallExit
