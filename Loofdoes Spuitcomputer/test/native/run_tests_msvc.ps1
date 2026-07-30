# Run all AUnit tests using MSVC (Visual Studio 2022).
# Use this on Windows when GCC / pio run -e native_* is not available.
#
# Usage: .\test\native\run_tests_msvc.ps1

$root   = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$aunit  = "$root\.pio\libdeps\native\AUnit\src"
$stubs  = "$root\test\native\support"
$lib    = "$root\lib\LoofdoesCore\src"
$test   = "$root\test\native\tests"
$out    = "$env:TEMP\msvc_test"

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

$commonFlags = "/std:c++17 /EHsc /nologo /W1 /DEPOXY_DUINO=1 /I`"$aunit`" /I`"$stubs`" /I`"$lib`""

$overallExit = 0

# ---- test_interface_sprayer ------------------------------------------------
Write-Host ""
Write-Host "=== Building test_interface_sprayer ===" -ForegroundColor Cyan

$cmd = "`"$vcvars`" $vcvarsArch && cl $commonFlags /Fo`"$out\\`" `"$lib\InterfaceSprayer.cpp`" `"$test\test_InterfaceSprayer.cpp`" `"$stubs\native_main.cpp`" $aunitSources /Fe:`"$out\test_InterfaceSprayer.exe`" && `"$out\test_InterfaceSprayer.exe`""
Write-Host "Running..." -ForegroundColor Cyan
cmd /c $cmd

if ($LASTEXITCODE -ne 0) { $overallExit = 1 }

# ---- test_ImplementSprayer ------------------------------------------------
Write-Host ""
Write-Host "=== Building test_ImplementSprayer ===" -ForegroundColor Cyan

$cmd = "`"$vcvars`" $vcvarsArch && cl $commonFlags /Fo`"$out\\`" `"$lib\InterfaceSprayer.cpp`" `"$lib\ImplementSprayer.cpp`" `"$test\test_ImplementSprayer.cpp`" `"$stubs\native_main.cpp`" $aunitSources /Fe:`"$out\test_ImplementSprayer.exe`" && `"$out\test_ImplementSprayer.exe`""
Write-Host "Running..." -ForegroundColor Cyan
cmd /c $cmd

if ($LASTEXITCODE -ne 0) { $overallExit = 1 }

# ---- Summary ---------------------------------------------------------------
Write-Host ""
if ($overallExit -eq 0) {
    Write-Host "All test suites PASSED." -ForegroundColor Green
} else {
    Write-Host "One or more test suites FAILED." -ForegroundColor Red
}
exit $overallExit
