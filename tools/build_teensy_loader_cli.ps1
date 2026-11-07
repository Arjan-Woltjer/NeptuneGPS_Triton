# Builds PJRC's teensy_loader_cli for Windows with MSVC, for tools/flash_teensy.ps1.
#
# Why: `pio -t upload` hands the hex to the Teensy Loader GUI through
# teensy_post_compile, which finds the running loader by looking for its
# LISTENING socket on 127.0.0.1:3149 in Windows' TCP table. Windows 10.0.26300
# (26H2) reports most listening sockets with an unknown state there (netstat
# hides them too), so teensy_post_compile reports "Unable find Teensy Loader"
# and the loader opens with no file, not in Auto, and never uploads.
# teensy_loader_cli talks to the HalfKay bootloader over HID directly and needs
# no loader GUI. PlatformIO's Windows tool-teensy package does not ship it.
#
#   -Dest    where the source is cloned and the .exe built
#            (default: $env:USERPROFILE\tools\teensy_loader_cli)
#   -Commit  the upstream commit to build (default: 03fca41, version 2.3)
#
# The upstream Windows build is for MinGW; MSVC needs three header shims
# (unistd.h, ddk/hidsdi.h, ddk/hidclass.h), WIN32 defined for its Sleep() branch,
# and hid/setupapi/winmm linked. Nothing in the source is changed.
param(
    [string] $Dest   = (Join-Path $env:USERPROFILE "tools\teensy_loader_cli"),
    [string] $Commit = "03fca41"
)
$ErrorActionPreference = "Stop"

if (-not (Test-Path (Join-Path $Dest ".git"))) {
    git clone -q https://github.com/PaulStoffregen/teensy_loader_cli.git $Dest
    if ($LASTEXITCODE -ne 0) { throw "git clone failed" }
}
git -C $Dest -c advice.detachedHead=false checkout -q $Commit
if ($LASTEXITCODE -ne 0) { throw "git checkout $Commit failed" }

$shim = Join-Path $Dest "msvc_shim"
New-Item -ItemType Directory -Force (Join-Path $shim "ddk") | Out-Null
Set-Content -Encoding ascii -Path (Join-Path $shim "unistd.h") -Value "/* MSVC shim: included, nothing from it is used on Win32 */"
Set-Content -Encoding ascii -Path (Join-Path $shim "ddk\hidsdi.h") -Value "#include <hidsdi.h>"
Set-Content -Encoding ascii -Path (Join-Path $shim "ddk\hidclass.h") -Value "#include <hidclass.h>"

$vcvars = @(
    "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) { throw "Visual Studio (vcvars64.bat) not found" }

$exe = Join-Path $Dest "teensy_loader_cli.exe"
Remove-Item -Force -ErrorAction SilentlyContinue $exe
cmd /c "`"$vcvars`" >nul 2>&1 && cd /d `"$Dest`" && cl /nologo /O2 /W3 /DUSE_WIN32 /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I msvc_shim teensy_loader_cli.c /Fe:teensy_loader_cli.exe setupapi.lib hid.lib winmm.lib"
if (-not (Test-Path $exe)) { throw "build failed: $exe not produced" }
Write-Host "Built $exe (teensy_loader_cli $Commit)"
