[CmdletBinding()]
param()

. (Join-Path $PSScriptRoot 'common.ps1')
Write-Host "Project: $ProjectRoot"
Write-Host "Zephyr:  $env:ZEPHYR_BASE"
Write-Host "SDK:     $env:ZEPHYR_SDK_INSTALL_DIR"
foreach ($ProjectCommand in @('python', 'git', 'cmake', 'ninja', 'west', 'dfu-util', '7z', 'wget.exe', 'dtc', 'gperf')) {
    $ProjectResolved = Get-Command $ProjectCommand -ErrorAction SilentlyContinue
    if ($ProjectResolved) {
        Write-Host ("{0,-10} {1}" -f $ProjectCommand, $ProjectResolved.Source)
    } else {
        Write-Host ("{0,-10} MISSING" -f $ProjectCommand) -ForegroundColor Yellow
    }
}
if (Test-Path -LiteralPath (Join-Path $ProjectVenv 'python.exe')) {
    & (Join-Path $ProjectVenv 'python.exe') --version
    & (Join-Path $ProjectVenv 'python.exe') -m pip show west pyserial
}
if (Test-Path -LiteralPath (Join-Path $ProjectWorkspace '.west')) {
    Push-Location $ProjectWorkspace
    try {
        & (Join-Path $ProjectVenv 'west.exe') sdk list
    } finally {
        Pop-Location
    }
}
if (Get-Command dfu-util -ErrorAction SilentlyContinue) {
    & dfu-util --list
}
