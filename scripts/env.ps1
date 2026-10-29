# Dot-source: . .\scripts\env.ps1
# Only changes this PowerShell process. Does not source ESP-IDF or edit system PATH.
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectVenv = Join-Path $ProjectRoot '.venv\Scripts'
$ProjectWorkspace = Join-Path $ProjectRoot '.zephyr-workspace'
$ProjectSdk = Join-Path $ProjectRoot '.tools\zephyr-sdk-0.17.2'

function Add-ProjectToolPath([string]$Directory) {
    if ((Test-Path -LiteralPath $Directory -PathType Container) -and
        (($env:PATH -split ';') -notcontains $Directory)) {
        $env:PATH = "$Directory;$env:PATH"
    }
}

# Prefer known usable ESP-IDF host tools. The ESP compiler/OpenOCD are not reused.
$ProjectIdfRoots = @(
    (Join-Path $env:USERPROFILE '.espressif\tools'),
    'C:\Espressif\tools'
)
foreach ($ProjectIdfRoot in $ProjectIdfRoots) {
    foreach ($ProjectPattern in @('cmake\*\bin\cmake.exe', 'ninja\*\ninja.exe',
                                  'dfu-util\*\dfu-util-*-win64\dfu-util.exe')) {
        $ProjectCandidates = @(Get-ChildItem -Path (Join-Path $ProjectIdfRoot $ProjectPattern) `
            -File -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)
        if ($ProjectCandidates.Count -gt 0) {
            Add-ProjectToolPath $ProjectCandidates[0].DirectoryName
        }
    }
}
Add-ProjectToolPath 'C:\Program Files\7-Zip'
Add-ProjectToolPath (Join-Path $ProjectRoot '.tools\bin')
# Always put this project's Python before any IDF virtual environment.
Add-ProjectToolPath $ProjectVenv
$env:VIRTUAL_ENV = Join-Path $ProjectRoot '.venv'
$env:PYTHONNOUSERSITE = '1'
$env:ZEPHYR_BASE = Join-Path $ProjectWorkspace 'zephyr'
$env:ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
if (Test-Path -LiteralPath $ProjectSdk) {
    $env:ZEPHYR_SDK_INSTALL_DIR = $ProjectSdk
}
