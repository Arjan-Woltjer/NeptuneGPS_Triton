# Run all AUnit tests using MSVC (Visual Studio 2022).
# Use this on Windows when GCC / pio run -e native is not available.
#
# Usage: .\test\native\run_tests_msvc.ps1

$root      = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$aunit     = "$root\.pio\libdeps\native\AUnit\src"
$stubs     = "$root\test\native\support"
$lib       = "$root\lib\MotorcontrollerCore\src"
# The SteeringActuator git submodule (lib/SteeringActuator), matching what
# platformio.ini's [env:native] compiles. This used to point at the sibling
# "Shared Firmware Libs\SteeringActuator" directory the library lived in before it
# was extracted into its own repo -- which meant this script silently compiled a
# different checkout than CI and PlatformIO did (they were 6 commits apart on
# 2026-09-08), and could not work at all in a standalone clone of this repo, the
# very case the submodule migration existed to fix.
$sharedLib = "$root\lib\SteeringActuator\src"
$test      = "$root\test\native\tests"
$driver    = "$root\test\native\MotorcontrollerNativeTests.cpp"
$out       = "$env:TEMP\msvc_test"

New-Item -ItemType Directory -Force -Path $out | Out-Null

# An uninitialized submodule is an empty directory, which otherwise surfaces as a
# confusing MSVC C1083 "cannot open source file" much further down.
if (-not (Test-Path "$sharedLib\SteeringActuator.cpp")) {
    Write-Error "SteeringActuator sources not found at $sharedLib -- the submodule is not checked out. Run: git submodule update --init Motorcontroller/lib/SteeringActuator"
    exit 1
}

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
# (default) preprocessor expands wrong.
$commonFlags = "/std:c++17 /Zc:preprocessor /EHsc /nologo /W1 /DEPOXY_DUINO=1 /I`"$aunit`" /I`"$stubs`" /I`"$lib`" /I`"$sharedLib`""

Write-Host ""
Write-Host "=== Building MotorcontrollerNativeTests ===" -ForegroundColor Cyan

$testSources = (Get-ChildItem "$test\test_*.cpp" | ForEach-Object { "`"$($_.FullName)`"" }) -join ' '

$cmd = "`"$vcvars`" $vcvarsArch && cl $commonFlags /Fo`"$out\\`" `"$lib\InterfaceMotorcontroller.cpp`" `"$sharedLib\SteeringActuator.cpp`" $testSources `"$driver`" `"$stubs\native_main.cpp`" $aunitSources /Fe:`"$out\MotorcontrollerNativeTests.exe`" && `"$out\MotorcontrollerNativeTests.exe`""
Write-Host "Running..." -ForegroundColor Cyan
cmd /c $cmd

$overallExit = if ($LASTEXITCODE -ne 0) { 1 } else { 0 }

Write-Host ""
if ($overallExit -eq 0) {
    Write-Host "All test suites PASSED." -ForegroundColor Green
} else {
    Write-Host "One or more test suites FAILED." -ForegroundColor Red
}
exit $overallExit
