# Run all AUnit tests using MSVC (Visual Studio 2022).
# Use this on Windows when GCC / pio run -e native is not available.
#
# Usage: .\test\native\run_tests_msvc.ps1

$root    = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$aunit   = "$root\.pio\libdeps\native\AUnit\src"
$stubs   = "$root\test\native\support"
$lib     = "$root\lib\PloegbesturingCore\src"
$guidance = "$root\..\MeijWorks Libs\VehicleGuidance"   # shared GuidanceSource, NeptuneGPS_Triton#76
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
# (default) preprocessor expands wrong (see Salacia's/Loofdoes' platformio.ini/
# test README for the same issue, hit there first).
$commonFlags = "/std:c++17 /Zc:preprocessor /EHsc /nologo /W1 /DEPOXY_DUINO=1 /I`"$aunit`" /I`"$stubs`" /I`"$lib`" /I`"$guidance`""

# ---- combined native test binary --------------------------------------------
# One binary for every test_*.cpp under test/native/tests/ -- matches
# platformio.ini's [env:native] build_src_filter (single driver file,
# PloegbesturingNativeTests.cpp, provides setup()/loop()).
Write-Host ""
Write-Host "=== Building PloegbesturingNativeTests ===" -ForegroundColor Cyan

$testSources = (Get-ChildItem "$test\test_*.cpp" | ForEach-Object { "`"$($_.FullName)`"" }) -join ' '

$cmd = "`"$vcvars`" $vcvarsArch && cl $commonFlags /Fo`"$out\\`" `"$lib\ImplementPlough.cpp`" `"$lib\InterfacePlough.cpp`" `"$lib\CalibrationPlough.cpp`" $testSources `"$driver`" `"$stubs\native_main.cpp`" $aunitSources /Fe:`"$out\PloegbesturingNativeTests.exe`" && `"$out\PloegbesturingNativeTests.exe`""
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
