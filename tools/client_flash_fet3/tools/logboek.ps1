# Logboek van de ploegbesturing: schrijft alles wat de besturing via USB meldt,
# met de tijd erbij, naar logboek_<datum>_<tijd>.txt in de map erboven.
# Gestart door "Logboek starten.cmd". Toetsen: D = status opslaan, N = notitie, Q = stoppen.

$ErrorActionPreference = 'Stop'
$tools = Split-Path -Parent $MyInvocation.MyCommand.Path
$root  = Split-Path -Parent $tools
$file  = Join-Path $root ("logboek_{0}.txt" -f (Get-Date -Format 'yyyy-MM-dd_HHmmss'))
$log   = New-Object System.IO.StreamWriter($file, $true, [Text.Encoding]::UTF8)
$log.AutoFlush = $true

function Stamp { (Get-Date).ToString('HH:mm:ss.fff') }
function Note($text) {
    $log.WriteLine("$(Stamp) ## $text")
    Write-Host "  $(Get-Date -Format 'HH:mm:ss')  $text" -ForegroundColor Cyan
}

# De besturing (Teensy) met zijn COM-poort en USB-serienummer, of $null.
function Find-Board {
    foreach ($d in @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue)) {
        if ($d.PNPDeviceID -match '^USB\\VID_16C0&PID_0483\\(\w+)$') {
            $serial = $Matches[1]
            if ($d.Name -match '\((COM\d+)\)') { return @{ Port = $Matches[1]; Serial = $serial } }
        }
    }
    return $null
}

Write-Host ''
Write-Host '  Ploegbesturing logboek' -ForegroundColor White
Write-Host '  ======================' -ForegroundColor White
Write-Host '  Laat dit venster open tijdens alle tests.'
Write-Host '  D = status opslaan   N = notitie typen   Q = stoppen'
Write-Host ''
Note "Logboek gestart: $file"

$port = $null
$buf = ''
$waitNoted = $false
while ($true) {
    if (-not $port) {
        $b = Find-Board
        if (-not $b) {
            if (-not $waitNoted) { Note 'Geen besturing op USB. Controleer de kabel; het logboek wacht.'; $waitNoted = $true }
            Start-Sleep -Milliseconds 500
        } else {
            try {
                $port = New-Object System.IO.Ports.SerialPort $b.Port, 115200
                $port.ReadBufferSize = 1048576
                $port.DtrEnable = $true
                $port.Open()
                Note "Besturing gevonden op $($b.Port), USB-serienummer $($b.Serial)"
                $waitNoted = $false
            } catch {
                if (-not $waitNoted) { Note "$($b.Port) is bezet (staat het flash-venster nog open?). Het logboek wacht."; $waitNoted = $true }
                $port = $null
                Start-Sleep -Milliseconds 500
            }
        }
    }

    if ($port) {
        try {
            $data = $port.ReadExisting()
        } catch {
            Note 'Verbinding met de besturing kwijt, opnieuw zoeken...'
            try { $port.Close() } catch { }
            $port = $null
            $data = ''
        }
        if ($data) {
            $buf += $data
            $parts = $buf -split "`n"
            $buf = $parts[-1]
            if ($parts.Count -gt 1) {
                foreach ($line in $parts[0..($parts.Count - 2)]) {
                    $line = $line.TrimEnd("`r")
                    $log.WriteLine("$(Stamp) $line")
                    Write-Host "    $line" -ForegroundColor DarkGray
                }
            }
        }
    }

    while ([Console]::KeyAvailable) {
        $k = [Console]::ReadKey($true)
        switch ($k.Key) {
            'D' {
                if ($port) {
                    Note 'Status opgevraagd (D)'
                    # Debugmenu openen, volledige status, menu sluiten.
                    try { $port.Write('m1q') } catch { Note 'Status opvragen lukte niet.' }
                } else { Note 'Geen besturing: status niet opgevraagd.' }
            }
            'N' {
                $t = Read-Host '  Notitie'
                Note "NOTITIE: $t"
            }
            'Q' {
                Note 'Logboek gestopt (Q)'
                if ($port) { try { $port.Close() } catch { } }
                $log.Close()
                Write-Host ''
                Write-Host "  Opgeslagen: $file" -ForegroundColor Green
                Write-Host '  Stuur dit bestand naar MeijWorks.'
                exit 0
            }
        }
    }
    Start-Sleep -Milliseconds 50
}
