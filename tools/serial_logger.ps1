# Timestamped serial logger for a Teensy board that survives the board
# rebooting, being reflashed or being replugged: it reopens the port whenever it
# comes back. Every line goes to -Log with the time (HH:mm:ss.fff).
#
#   -Port      the COM port, e.g. COM13
#   -Log       the log file (appended to)
#   -StopFile  the logger stops, and frees the port, once this file exists --
#              create it before flashing (flash_teensy.ps1 aborts on a busy port)
#   -CmdFile   optional: when this file appears its text is sent to the board
#              as is and the file is deleted, e.g. "m1q" for one full ISOBUS
#              status dump (open the debug menu, 1, quit)
#
# Run it in the background, e.g. from PowerShell:
#   tools/serial_logger.ps1 -Port COM13 -Log bench.log -StopFile stop.flag -CmdFile cmd.txt
# A board on USB serial buffers its boot print until a host opens the port, so
# opening it after the boot still catches the boot print (the plough firmware
# also waits 3 s before it starts printing).
param(
    [Parameter(Mandatory = $true)][string] $Port,
    [Parameter(Mandatory = $true)][string] $Log,
    [Parameter(Mandatory = $true)][string] $StopFile,
    [string] $CmdFile
)

function Write-Log([string] $text) { $text | Out-File $Log -Append -Encoding utf8 }

Write-Log "=== $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') logger start on $Port"
while (-not (Test-Path $StopFile)) {
    $sp = $null
    try {
        $sp = New-Object System.IO.Ports.SerialPort $Port, 115200
        $sp.DtrEnable = $true
        $sp.ReadTimeout = 200
        $sp.Open()
        Write-Log "=== $(Get-Date -Format 'HH:mm:ss.fff') opened $Port"
        $buf = ''
        while (-not (Test-Path $StopFile)) {
            $buf += $sp.ReadExisting()
            while (($i = $buf.IndexOf("`n")) -ge 0) {
                $line = $buf.Substring(0, $i).TrimEnd("`r")
                $buf = $buf.Substring($i + 1)
                Write-Log "$(Get-Date -Format 'HH:mm:ss.fff') $line"
            }
            if ($CmdFile -and (Test-Path $CmdFile)) {
                $cmd = (Get-Content $CmdFile -Raw).TrimEnd()
                Remove-Item $CmdFile
                $sp.Write($cmd)
                Write-Log "$(Get-Date -Format 'HH:mm:ss.fff') >>> sent '$cmd'"
            }
            Start-Sleep -Milliseconds 50
        }
    } catch {
        if ($sp -and $sp.IsOpen) { Write-Log "=== $(Get-Date -Format 'HH:mm:ss.fff') lost $Port" }
    } finally {
        if ($sp) { try { $sp.Close() } catch { }; $sp.Dispose() }
    }
    Start-Sleep -Milliseconds 200
}
Write-Log "=== $(Get-Date -Format 'HH:mm:ss.fff') logger stopped"
