# Flashes one Teensy, picked by its USB serial number, with teensy_loader_cli:
# no Teensy Loader GUI, no teensy_post_compile (broken on Windows 10.0.26300,
# see tools/build_teensy_loader_cli.ps1, which builds the loader this uses).
#
#   -Serial     the board's USB serial as Windows shows it, e.g. 13249100
#               (USB\VID_16C0&PID_0483\13249100)
#   -Hex        the .hex to write
#   -Mcu        teensy_loader_cli's MCU name (default TEENSY41; TEENSY40 for a 4.0)
#   -LoaderCli  path to teensy_loader_cli.exe
#               (default: $env:USERPROFILE\tools\teensy_loader_cli\teensy_loader_cli.exe)
#   -BannerSeconds  after the flash, read the board's serial port this long and
#               print what it says (0 = don't)
#
# Guards, each of which aborts before anything is written:
# - exactly ONE PJRC-ID (16C0) board on USB, and it is -Serial (running, or
#   already in HalfKay). teensy_loader_cli programs the first HalfKay device it
#   finds, not one by serial, so a second board could be the one written.
# - no Saturn (1D50:606F) on USB: in its own bootloader it is a 16C0:0478 too.
# - the board's COM port is free (a serial logger holding it stops the reboot).
#
# The board is put into HalfKay with a 134-baud open of its own COM port
# (teensy_loader_cli's soft reboot is not implemented on Windows). The script
# only reports success once the board is back on its runtime USB ID.
param(
    [Parameter(Mandatory = $true)][string] $Serial,
    [Parameter(Mandatory = $true)][string] $Hex,
    [string] $Mcu = "TEENSY41",
    [string] $LoaderCli = (Join-Path $env:USERPROFILE "tools\teensy_loader_cli\teensy_loader_cli.exe"),
    [int] $BannerSeconds = 8
)
$ErrorActionPreference = "Stop"

if (-not (Test-Path $LoaderCli)) { throw "teensy_loader_cli not found at $LoaderCli -- run tools/build_teensy_loader_cli.ps1" }
if (-not (Test-Path $Hex)) { throw "hex not found: $Hex" }
$Hex = (Resolve-Path $Hex).Path

# Teensy 4: the runtime serial is the board number * 10; HalfKay shows the board
# number in hex, 8 digits (13249100 -> 1324910 -> 0014376E).
$boardNumber = [int64]::Parse($Serial) / 10
$halfKaySerial = "{0:X8}" -f [int64][math]::Floor($boardNumber)
$runtimeId = "USB\VID_16C0&PID_0483\$Serial"

function Get-PjrcBoards {
    @(Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match '^USB\\VID_16C0&PID_[0-9A-F]{4}\\[^\\]+$' })
}

$boards = Get-PjrcBoards
if ($boards.Count -ne 1) {
    throw "ABORT: expected exactly one PJRC board on USB, found $($boards.Count): $($boards.InstanceId -join ', ')"
}
$id = $boards[0].InstanceId
$inHalfKay = $id -eq "USB\VID_16C0&PID_0478\$halfKaySerial"
if (-not $inHalfKay -and $id -ne $runtimeId) {
    throw "ABORT: the board on USB is $id, not $Serial"
}
if (@(Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'VID_1D50' }).Count -gt 0) {
    throw "ABORT: a 1D50 device (Saturn) is on USB; unplug it first"
}

$port = $null
if (-not $inHalfKay) {
    $ports = @(Get-PnpDevice -PresentOnly -Class Ports | Where-Object { $_.InstanceId -match '^USB\\VID_16C0&PID_0483' -and $_.FriendlyName -match '\((COM\d+)\)' })
    if ($ports.Count -ne 1) { throw "ABORT: expected one Teensy COM port, found $($ports.Count)" }
    $null = $ports[0].FriendlyName -match '\((COM\d+)\)'
    $port = $Matches[1]
    try { $sp = New-Object System.IO.Ports.SerialPort $port, 115200; $sp.Open(); $sp.Close() }
    catch { throw "ABORT: $port is busy (stop the serial logger): $($_.Exception.Message)" }
}

Write-Host "Flashing $Serial ($Mcu) on $(if ($port) { $port } else { 'HalfKay' }) with $Hex"

# The loader waits (-w) for HalfKay; start it first, then send the board there.
$out = Join-Path $env:TEMP "flash_teensy_$Serial.log"
$proc = Start-Process -FilePath $LoaderCli -ArgumentList @("--mcu=$Mcu", "-w", "-v", "`"$Hex`"") `
    -NoNewWindow -PassThru -RedirectStandardOutput $out -RedirectStandardError "$out.err"
Start-Sleep -Milliseconds 500
if ($port) {
    $sp = New-Object System.IO.Ports.SerialPort $port, 134
    try { $sp.Open(); Start-Sleep -Milliseconds 100; $sp.Close() } catch { }
}

if (-not $proc.WaitForExit(60000)) {
    $proc.Kill()
    throw "teensy_loader_cli did not finish in 60 s (board not in HalfKay? press its button and re-run)"
}
$log = (Get-Content $out -Raw) + (Get-Content "$out.err" -Raw -ErrorAction SilentlyContinue)
Write-Host $log.Trim()
if ($proc.ExitCode -ne 0) { throw "teensy_loader_cli failed (exit $($proc.ExitCode))" }

# Back on its runtime ID, not left in HalfKay.
$deadline = (Get-Date).AddSeconds(30)
do {
    Start-Sleep -Milliseconds 500
    $back = Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -eq $runtimeId -and $_.Status -eq 'OK' }
} until ($back -or (Get-Date) -gt $deadline)
if (-not $back) { throw "the board did not come back as $runtimeId within 30 s" }
Write-Host "OK: $Serial is running the new firmware."

if ($BannerSeconds -gt 0) {
    $ports = @(Get-PnpDevice -PresentOnly -Class Ports | Where-Object { $_.InstanceId -match '^USB\\VID_16C0&PID_0483' -and $_.FriendlyName -match '\((COM\d+)\)' })
    if ($ports.Count -eq 1) {
        $null = $ports[0].FriendlyName -match '\((COM\d+)\)'
        $sp = New-Object System.IO.Ports.SerialPort $Matches[1], 115200
        $sp.ReadTimeout = 200
        try {
            $sp.Open(); $sp.DtrEnable = $true
            $end = (Get-Date).AddSeconds($BannerSeconds); $text = ""
            while ((Get-Date) -lt $end) { try { $text += $sp.ReadExisting() } catch { }; Start-Sleep -Milliseconds 100 }
            $sp.Close()
            Write-Host "--- $($Matches[1]), first $BannerSeconds s ---"
            Write-Host $text
        } catch { Write-Host "(could not read the serial port: $($_.Exception.Message))" }
    }
}
