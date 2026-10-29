[CmdletBinding()]
param([string]$Port)

. (Join-Path $PSScriptRoot 'common.ps1')
$ProjectPython = Join-Path $ProjectVenv 'python.exe'
if (-not $Port) {
    Invoke-Checked -Program $ProjectPython -Arguments @('-m', 'serial.tools.list_ports', '-v')
    Write-Host 'Select a port: .\scripts\monitor.ps1 -Port COM5'
    return
}
Write-Host '115200 8N1. Press Ctrl+] to exit. Reset the board to see boot messages.'
Invoke-Checked -Program $ProjectPython -Arguments @('-m', 'serial.tools.miniterm',
    $Port, '115200', '--raw')
