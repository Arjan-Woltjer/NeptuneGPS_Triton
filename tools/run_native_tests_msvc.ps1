# Shared MSVC runner for every Triton project's native AUnit tests: for Windows
# machines without a host GCC, where `pio test -e native` cannot run. Each
# project's test/native/run_tests_msvc.ps1 calls this with only what is its own
# (sources, include folders, defines); everything they used to repeat lives here:
# the Visual Studio lookup, AUnit's sources, the compiler flags, the test_*.cpp
# glob, the driver and native_main.cpp, the output folder and the summary.
#
#   -ProjectRoot  the project folder (the one holding platformio.ini)
#   -Name         the driver and binary name, e.g. KipperbesturingNativeTests:
#                 test/native/<Name>.cpp, built to <Name>.exe
#   -Includes     include folders, relative to ProjectRoot (or absolute)
#   -Sources      source files, relative to ProjectRoot (or absolute); wildcards
#                 are expanded, so a whole library folder can be passed as *.cpp
#   -Defines      preprocessor defines, without /D (EPOXY_DUINO=1 is always set)
#
# The output folder is $env:TEMP\msvc_test\<project folder>-<hash of its full
# path>: two projects, or two worktrees of one project, can run at once without
# overwriting each other's objects (they all shared $env:TEMP\msvc_test before).
param(
    [Parameter(Mandatory = $true)][string]   $ProjectRoot,
    [Parameter(Mandatory = $true)][string]   $Name,
    [string[]] $Includes = @(),
    [string[]] $Sources  = @(),
    [string[]] $Defines  = @()
)

function Resolve-ProjectPath([string] $path) {
    if ([System.IO.Path]::IsPathRooted($path)) { return $path }
    return Join-Path $ProjectRoot $path
}

function Quote([string] $path) { return "`"$path`"" }

$aunit = Resolve-ProjectPath ".pio\libdeps\native\AUnit\src"
$stubs = Resolve-ProjectPath "test\native\support"
$tests = Resolve-ProjectPath "test\native\tests"
$driver = Resolve-ProjectPath "test\native\$Name.cpp"

if (-not (Test-Path "$aunit\AUnit.h")) {
    Write-Error "AUnit not found at $aunit -- run: pio pkg install -e native (in $ProjectRoot)"
    exit 1
}

$hasher = [System.Security.Cryptography.SHA1]::Create()
$hash = ($hasher.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($ProjectRoot.ToLowerInvariant())) |
    Select-Object -First 4 | ForEach-Object { $_.ToString("x2") }) -join ''
$out = Join-Path $env:TEMP ("msvc_test\" + (Split-Path $ProjectRoot -Leaf) + "-" + $hash)
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

$aunitSources = @("Assertion", "Compare", "FCString", "MetaAssertion", "print64", "Printer",
                  "string_util", "Test", "TestAgain", "TestOnce", "TestRunner") |
    ForEach-Object { Quote "$aunit\aunit\$_.cpp" }

$projectSources = foreach ($s in $Sources) {
    $resolved = Resolve-ProjectPath $s
    if ($resolved.Contains('*')) {
        Get-ChildItem $resolved | ForEach-Object { Quote $_.FullName }
    }
    else {
        Quote $resolved
    }
}
$testSources = Get-ChildItem "$tests\test_*.cpp" | ForEach-Object { Quote $_.FullName }

# /Zc:preprocessor switches on MSVC's conformant preprocessor -- required for
# AUnit's two-argument test(SuiteName, testName) macros: they dispatch on
# argument count via GET_TEST(__VA_ARGS__, TEST2, TEST1)(__VA_ARGS__), which
# MSVC's legacy (default) preprocessor expands wrong.
$flags = @("/std:c++17", "/Zc:preprocessor", "/EHsc", "/nologo", "/W1", "/DEPOXY_DUINO=1")
$flags += $Defines | ForEach-Object { "/D$_" }
# The native stubs come first, so a same-named fake (Arduino.h, EEPROM.h,
# VehicleSensors.hpp, ...) always wins over the real header it stands in for.
$flags += @($aunit, $stubs) + ($Includes | ForEach-Object { Resolve-ProjectPath $_ }) | ForEach-Object { "/I" + (Quote $_) }

# One binary for every test_*.cpp under test/native/tests/, as platformio.ini's
# [env:native] build_src_filter builds it. The arguments go through a response
# file: with AgIsoStack's sources the command line passes cmd's 8 KB limit.
$clArgs = @($flags) + @("/Fo" + (Quote "$out\\")) + @($projectSources) + @($testSources) +
          @((Quote $driver), (Quote "$stubs\native_main.cpp")) + @($aunitSources) +
          @("/Fe:" + (Quote "$out\$Name.exe"))
$rsp = Join-Path $out "cl_args.rsp"
Set-Content -Path $rsp -Value ($clArgs -join ' ') -Encoding ascii

Write-Host ""
Write-Host "=== Building $Name ===" -ForegroundColor Cyan
Write-Host "Running..." -ForegroundColor Cyan
cmd /c "`"$vcvars`" $vcvarsArch && cl @`"$rsp`" && `"$out\$Name.exe`""

$overallExit = if ($LASTEXITCODE -ne 0) { 1 } else { 0 }

Write-Host ""
if ($overallExit -eq 0) {
    Write-Host "All test suites PASSED." -ForegroundColor Green
}
else {
    Write-Host "One or more test suites FAILED." -ForegroundColor Red
}
exit $overallExit
