# Builds the self-flash zip a client uses to update a Teensy 4.0 plough board
# with the FET 1 workaround (env teensy40_isobus_fet3, #302) without MeijWorks
# on site.
#
#   -Hex   the teensy40_isobus_fet3 .hex to ship (e.g. from Firmware builds\...)
#   -Out   the zip to write, e.g. ...\Ploegbesturing_FET3_update_<commit>.zip
#   -LoaderCli  teensy_loader_cli.exe to ship (default: the one
#          tools/build_teensy_loader_cli.ps1 builds, v2.3)
#   -TeensyTools  PlatformIO's tool-teensy folder, for teensy.exe,
#          teensy_post_compile.exe and teensy_reboot.exe (the fallbacks)
#
# The package is tools/client_flash_fet3/ as it is in the repo, plus the hex
# (as firmware\ploegbesturing_teensy40_fet3.hex, the name flash.ps1 expects)
# and the four executables, which are not kept in git.
#
# Before each release: update client_flash_fet3/LEESMIJ.txt ("Wat er verandert",
# the fallback zip name). Test the zip on a spare Teensy 4.0 before sending it:
# extract it, run "Firmware flashen.cmd" with the board running (COM port up),
# and check the boot print's settings are the same before and after.
#
# Why our own teensy_loader_cli: PlatformIO's v2.2 build found HalfKay but
# failed with "error writing to Teensy" on a Teensy 4.0 (2026-11-08); the v2.3
# MSVC build is statically linked (only Windows system DLLs) and flashed the
# same board in 8 s.
param(
    [Parameter(Mandatory = $true)][string] $Hex,
    [Parameter(Mandatory = $true)][string] $Out,
    [string] $LoaderCli   = (Join-Path $env:USERPROFILE "tools\teensy_loader_cli\teensy_loader_cli.exe"),
    [string] $TeensyTools = (Join-Path $env:USERPROFILE ".platformio\packages\tool-teensy")
)
$ErrorActionPreference = "Stop"

$template = Join-Path $PSScriptRoot "client_flash_fet3"
foreach ($path in @($Hex, $LoaderCli, $template,
                    (Join-Path $TeensyTools "teensy.exe"),
                    (Join-Path $TeensyTools "teensy_post_compile.exe"),
                    (Join-Path $TeensyTools "teensy_reboot.exe"))) {
    if (-not (Test-Path $path)) { throw "not found: $path" }
}
if (Test-Path $Out) { throw "$Out exists; remove it or pick another name" }

$stage = Join-Path ([IO.Path]::GetTempPath()) ("client_flash_" + [guid]::NewGuid().ToString("N"))
$root  = Join-Path $stage "Ploegbesturing_FET3_update"
try {
    New-Item -ItemType Directory -Force $root | Out-Null
    Copy-Item -Recurse (Join-Path $template "*") $root
    New-Item -ItemType Directory -Force (Join-Path $root "firmware") | Out-Null
    Copy-Item $Hex (Join-Path $root "firmware\ploegbesturing_teensy40_fet3.hex")
    Copy-Item $LoaderCli (Join-Path $root "tools\teensy_loader_cli.exe")
    foreach ($exe in "teensy.exe", "teensy_post_compile.exe", "teensy_reboot.exe") {
        Copy-Item (Join-Path $TeensyTools $exe) (Join-Path $root "tools\$exe")
    }
    Compress-Archive -Path $root -DestinationPath $Out
    Write-Host "Wrote $Out"
    Get-ChildItem -Recurse -File $root | ForEach-Object { "{0,9}  {1}" -f $_.Length, $_.FullName.Substring($stage.Length + 1) }
} finally {
    Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
}
