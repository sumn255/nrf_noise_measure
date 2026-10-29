[CmdletBinding()]
param([switch]$SkipSdk)

. (Join-Path $PSScriptRoot 'common.ps1')

if (-not $SkipSdk -and -not (Get-Command wget.exe -CommandType Application -ErrorAction SilentlyContinue)) {
    throw 'SDK setup needs wget.exe (not the PowerShell alias). Run .\scripts\install-wget.ps1 first.'
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Install Git and open a new terminal first.'
}
if (-not (Test-Path -LiteralPath (Join-Path $ProjectVenv 'python.exe'))) {
    Invoke-Checked -Program 'python' -Arguments @('-m', 'venv', (Join-Path $ProjectRoot '.venv'))
}
$ProjectPython = Join-Path $ProjectVenv 'python.exe'
Invoke-Checked -Program $ProjectPython -Arguments @('-m', 'pip', 'install',
    '-r', (Join-Path $ProjectRoot 'requirements-bootstrap.txt'))

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Invoke-Checked -Program $ProjectPython -Arguments @('-m', 'pip', 'install', 'cmake==3.31.6')
}
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Invoke-Checked -Program $ProjectPython -Arguments @('-m', 'pip', 'install', 'ninja==1.12.1')
}

$ProjectWest = Join-Path $ProjectVenv 'west.exe'
if (-not (Test-Path -LiteralPath (Join-Path $ProjectWorkspace '.west'))) {
    Invoke-Checked -Program $ProjectWest -Arguments @('init', '-m',
        'https://github.com/zephyrproject-rtos/zephyr.git', '--mr', 'v4.2.0', $ProjectWorkspace)
}

Push-Location $ProjectWorkspace
try {
    $ProjectTag = & git -C $env:ZEPHYR_BASE describe --tags --exact-match
    if ($LASTEXITCODE -ne 0 -or $ProjectTag -ne 'v4.2.0') {
        throw 'Expected Zephyr v4.2.0. Refusing to change an existing checkout automatically.'
    }
    Invoke-Checked -Program $ProjectWest -Arguments @('update', '--narrow', '--fetch-opt=--depth=1')
    Invoke-Checked -Program $ProjectPython -Arguments @('-m', 'pip', 'install', '-r',
        (Join-Path $env:ZEPHYR_BASE 'scripts\requirements-base.txt'))

    if (-not $SkipSdk) {
        if (-not (Get-Command 7z -ErrorAction SilentlyContinue)) {
            throw '7-Zip is required for the Windows SDK archive. Install 7-Zip, then rerun setup.'
        }
        Invoke-Checked -Program $ProjectWest -Arguments @('sdk', 'install', '--version', '0.17.2',
            '--install-dir', $ProjectSdk, '--toolchains', 'riscv64-zephyr-elf')
    }
} finally {
    Pop-Location
}

. (Join-Path $PSScriptRoot 'env.ps1')
Write-Host 'Setup finished. Run .\scripts\doctor.ps1, then .\scripts\build.ps1.'
Write-Host 'If west reused an existing SDK elsewhere, set ZEPHYR_SDK_INSTALL_DIR to that directory.'
