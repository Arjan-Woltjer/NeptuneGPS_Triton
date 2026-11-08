# Flasht de ploegbesturing (Teensy 4.0) met de firmware in ..\firmware.
# Gestart door "Firmware flashen.cmd" in de map erboven.

$ErrorActionPreference = 'Stop'
$tools = Split-Path -Parent $MyInvocation.MyCommand.Path
$root  = Split-Path -Parent $tools
$fwDir = Join-Path $root 'firmware'
$name  = 'ploegbesturing_teensy40_fet3'
$hex   = Join-Path $fwDir "$name.hex"

function Write-Step($text) { Write-Host ''; Write-Host "  $text" -ForegroundColor Cyan }
function Write-Ok($text)   { Write-Host "  $text" -ForegroundColor Green }
function Write-Fail($text) { Write-Host "  $text" -ForegroundColor Red }

# 'serial' (firmware draait, met COM-poort), 'bootloader' (klaar om te flashen) of 'none'.
function Get-TeensyState {
    $devs = @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.PNPDeviceID -match '^USB\\VID_16C0&PID_(0483|0478)' })
    $serial = $devs | Where-Object { $_.PNPDeviceID -match 'PID_0483' -and $_.Name -match '\((COM\d+)\)' } | Select-Object -First 1
    if ($serial) {
        $null = $serial.Name -match '\((COM\d+)\)'
        return @{ State = 'serial'; Port = $Matches[1] }
    }
    if ($devs | Where-Object { $_.PNPDeviceID -match 'PID_0478' }) { return @{ State = 'bootloader'; Port = $null } }
    return @{ State = 'none'; Port = $null }
}

# Wacht tot de besturing na het flashen weer opstart (weer een COM-poort heeft).
function Wait-Restart {
    $deadline = (Get-Date).AddSeconds(8)
    while ((Get-Date) -lt $deadline -and (Get-TeensyState).State -eq 'serial') { Start-Sleep -Milliseconds 250 }
    if ((Get-TeensyState).State -eq 'serial') { return $false }
    $deadline = (Get-Date).AddSeconds(90)
    while ((Get-Date) -lt $deadline) {
        if ((Get-TeensyState).State -eq 'serial') { return $true }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

Write-Host ''
Write-Host '  Ploegbesturing firmware-update: ventiel BREDER naar FET 3' -ForegroundColor White
Write-Host '  ==========================================================' -ForegroundColor White

if (-not (Test-Path $hex) -or -not (Test-Path (Join-Path $tools 'teensy.exe'))) {
    Write-Fail 'Bestanden niet gevonden. Pak het zip-bestand eerst helemaal uit'
    Write-Fail '(rechtsklik > Alles uitpakken) en start het vanuit die map.'
    exit 1
}
Get-ChildItem -Recurse $root -File | Unblock-File -ErrorAction SilentlyContinue

Write-Step 'Sluit de USB-kabel aan tussen de besturingskast en deze computer.'
while ($true) {
    $t = Get-TeensyState
    if ($t.State -ne 'none') { break }
    Write-Host '  Geen besturing gevonden op USB. Controleer de kabel en druk op Enter om opnieuw te zoeken.'
    $null = Read-Host
}
if ($t.State -eq 'serial') { Write-Ok "Besturing gevonden op $($t.Port)." } else { Write-Ok 'Besturing gevonden (staat klaar om te flashen).' }

# Eerst teensy_loader_cli: praat zelf met de bootloader, zonder Teensy Loader-venster.
# De loader wacht (-w) op de bootloader; dit script zet de besturing daarin met een
# 134-baud open van zijn COM-poort.
$cli = Join-Path $tools 'teensy_loader_cli.exe'
if (Test-Path $cli) {
    Write-Step 'Bezig met flashen, haal de USB-kabel er niet uit...'
    $out = Join-Path $env:TEMP 'ploegbesturing_flash.log'
    $proc = Start-Process -FilePath $cli -ArgumentList @('--mcu=TEENSY40', '-w', '-v', "`"$hex`"") `
        -NoNewWindow -PassThru -RedirectStandardOutput $out -RedirectStandardError "$out.err"
    $null = $proc.Handle   # zonder dit is ExitCode na WaitForExit(timeout) soms leeg
    Start-Sleep -Milliseconds 500
    $portBusy = $false
    if ($t.State -eq 'serial') {
        try {
            $sp = New-Object System.IO.Ports.SerialPort $t.Port, 134
            $sp.Open(); Start-Sleep -Milliseconds 200; $sp.Close()
        } catch { $portBusy = $true }
    }
    if ($portBusy) {
        try { $proc.Kill() } catch { }
        Write-Fail "$($t.Port) is bezet. Sluit het logboekvenster (Q) en start dit opnieuw."
        exit 1
    }
    if ($proc.WaitForExit(60000) -and $proc.ExitCode -eq 0) {
        # Na het flashen start de besturing opnieuw op en krijgt weer een COM-poort.
        $deadline = (Get-Date).AddSeconds(30)
        while ((Get-Date) -lt $deadline -and (Get-TeensyState).State -ne 'serial') { Start-Sleep -Milliseconds 250 }
        if ((Get-TeensyState).State -eq 'serial') {
            Write-Host ''
            Write-Ok 'Klaar: de nieuwe firmware staat erop en de besturing is opnieuw opgestart.'
            exit 0
        }
    } else {
        try { $proc.Kill() } catch { }
    }
    Get-Content $out, "$out.err" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "    $_" }
    Write-Step 'Dat lukte niet, we proberen het via de Teensy Loader.'
    $t = Get-TeensyState
}

# Automatisch: de Teensy Loader krijgt het bestand en zet de besturing zelf in flash-stand.
Write-Step 'Bezig met flashen, haal de USB-kabel er niet uit...'
# teensy_post_compile schrijft gewone meldingen naar stderr; met 'Stop' zou
# Windows PowerShell 5.1 daar het hele script op afbreken.
$ErrorActionPreference = 'Continue'
& (Join-Path $tools 'teensy_post_compile.exe') "-file=$name" "-path=$fwDir" "-tools=$tools" -board=TEENSY40 -reboot 2>&1 | ForEach-Object { Write-Host "    $_" }
$pcExit = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($pcExit -eq 0 -and (Wait-Restart)) {
    Write-Host ''
    Write-Ok 'Klaar: de nieuwe firmware staat erop en de besturing is opnieuw opgestart.'
    exit 0
}

# Handmatig: het Teensy Loader-venster laadt het bestand, dit script zet de besturing in flash-stand.
Write-Step 'Automatisch flashen lukte niet. We doen het via het Teensy Loader-venster.'
if (-not (Get-Process teensy -ErrorAction SilentlyContinue)) { Start-Process (Join-Path $tools 'teensy.exe') ; Start-Sleep -Seconds 2 }
Set-Clipboard -Value $hex
Write-Host '  1. Kies in het venster "Teensy" het menu File > Open HEX File.'
Write-Host '  2. Plak bij Bestandsnaam het pad (Ctrl+V, staat al klaar) en klik Openen:'
Write-Host "       $hex"
Write-Host '  3. Klik op de knop "Auto" in het venster, zodat hij groen is.'
Write-Host '  Het venster vraagt om op de knop van de Teensy te drukken: dat hoeft niet,'
Write-Host '  dit script zet de besturing zelf in flash-stand. Druk hier op Enter.'
$null = Read-Host

$t = Get-TeensyState
if ($t.State -eq 'serial') {
    try {
        $sp = New-Object System.IO.Ports.SerialPort $t.Port, 134
        $sp.Open(); Start-Sleep -Milliseconds 200; $sp.Close()
    } catch { }
}
Write-Step 'Bezig met flashen, haal de USB-kabel er niet uit...'
if (Wait-Restart) {
    Write-Host ''
    Write-Ok 'Klaar: de nieuwe firmware staat erop en de besturing is opnieuw opgestart.'
    exit 0
}
Write-Host ''
Write-Fail 'Flashen is niet gelukt. Bel MeijWorks en houd het Teensy-venster open.'
exit 1
