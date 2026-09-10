# Quick Air780E / CH340 serial AT test (same as SSCOM would do)
param(
    [string]$Port = "COM6",
    [int[]]$BaudRates = @(115200, 9600, 57600, 921600)
)

Add-Type -AssemblyName System.IO.Ports

function Test-SerialAt {
    param([string]$Com, [int]$Baud)
    $port = New-Object System.IO.Ports.SerialPort
    $port.PortName = $Com
    $port.BaudRate = $Baud
    $port.Parity = [System.IO.Ports.Parity]::None
    $port.DataBits = 8
    $port.StopBits = [System.IO.Ports.StopBits]::One
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 2000
    $port.NewLine = "`r`n"
    $port.DtrEnable = $true
    $port.RtsEnable = $true

    try {
        $port.Open()
        Start-Sleep -Milliseconds 300
        # Drain boot spam if any
        if ($port.BytesToRead -gt 0) {
            $boot = $port.ReadExisting()
        } else { $boot = "" }

        $port.DiscardInBuffer()
        $port.Write("AT`r`n")
        Start-Sleep -Milliseconds 800
        $resp = ""
        if ($port.BytesToRead -gt 0) { $resp = $port.ReadExisting() }

        [PSCustomObject]@{
            Port = $Com
            Baud = $Baud
            BootBytes = $boot.Length
            BootPreview = if ($boot.Length -gt 120) { $boot.Substring(0,120) + "..." } else { $boot }
            AtResponse = $resp
            Ok = ($resp -match "OK")
        }
    } catch {
        [PSCustomObject]@{
            Port = $Com
            Baud = $Baud
            BootBytes = 0
            BootPreview = ""
            AtResponse = "ERROR: $($_.Exception.Message)"
            Ok = $false
        }
    } finally {
        if ($port.IsOpen) { $port.Close() }
        $port.Dispose()
    }
}

Write-Host "=== Air780E serial test on $Port ===" -ForegroundColor Cyan
Write-Host "Available ports: $([System.IO.Ports.SerialPort]::GetPortNames() -join ', ')"
Write-Host ""

$results = foreach ($b in $BaudRates) {
    $r = Test-SerialAt -Com $Port -Baud $b
    $status = if ($r.Ok) { "PASS" } else { "FAIL" }
    Write-Host "[$status] $Port @ $b baud | boot=$($r.BootBytes)B | AT -> $($r.AtResponse.Trim())"
    if ($r.BootPreview) { Write-Host "       boot: $($r.BootPreview.Replace("`r"," ").Replace("`n"," "))" }
    $r
}

Write-Host ""
if ($results | Where-Object Ok) {
    Write-Host "Result: Module responded to AT on at least one baud rate." -ForegroundColor Green
} else {
    Write-Host "Result: No AT response. Check wiring, power (5V), PK->GND, CH340 3V3 jumper." -ForegroundColor Yellow
}
