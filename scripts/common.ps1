$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'env.ps1')

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed (exit $LASTEXITCODE). See the output above."
    }
}

function Assert-ProjectEnvironment {
    if (-not (Test-Path -LiteralPath (Join-Path $ProjectVenv 'west.exe'))) {
        throw 'west is missing. Run .\scripts\setup.ps1 first.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $ProjectWorkspace '.west'))) {
        throw 'Zephyr workspace is missing. Run .\scripts\setup.ps1 first.'
    }
}
