[CmdletBinding()]
param([switch]$Pristine)

. (Join-Path $PSScriptRoot 'common.ps1')
Assert-ProjectEnvironment
$ProjectBuild = Join-Path $ProjectRoot 'build'
$ProjectPristine = if ($Pristine) { 'always' } else { 'auto' }

Push-Location $ProjectWorkspace
try {
    Invoke-Checked -Program (Join-Path $ProjectVenv 'west.exe') -Arguments @(
        'build', '-p', $ProjectPristine, '-b', 'longan_nano/gd32vf103',
        '-d', $ProjectBuild, $ProjectRoot)
} finally {
    Pop-Location
}
