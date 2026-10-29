[CmdletBinding()]
param()

. (Join-Path $PSScriptRoot 'common.ps1')
Assert-ProjectEnvironment
if (-not (Test-Path -LiteralPath (Join-Path $ProjectRoot 'build\zephyr\zephyr.bin'))) {
    throw 'Firmware is missing. Run .\scripts\build.ps1 first.'
}
if (-not (Get-Command dfu-util -ErrorAction SilentlyContinue)) {
    throw 'dfu-util is missing. Add its directory to PATH or install the ESP-IDF dfu-util tool.'
}
Write-Host 'Flashing firmware. The board must already be in USB DFU mode.'
Push-Location $ProjectWorkspace
try {
    Invoke-Checked -Program (Join-Path $ProjectVenv 'west.exe') -Arguments @(
        'flash', '-d', (Join-Path $ProjectRoot 'build'), '--runner', 'dfu-util')
} finally {
    Pop-Location
}
