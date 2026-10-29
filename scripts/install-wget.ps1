[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectDownloads = Join-Path $ProjectRoot '.tools\downloads'
$ProjectExtract = Join-Path $ProjectRoot '.tools\wget-1.25.0'
$ProjectBin = Join-Path $ProjectRoot '.tools\bin'
$ProjectArchive = Join-Path $ProjectDownloads 'wget-1.25.0-win64.zip'
# SHA256 published by ScoopInstaller/Main, bucket/wget.json (64bit, version 1.25.0).
$ProjectExpectedHash = '7567cbc08a1616931162b1c05f1731316853c32c79a21af1e850ccb673d1620c'

if (-not [Environment]::Is64BitOperatingSystem -or
    $env:PROCESSOR_ARCHITECTURE -eq 'ARM64') {
    throw 'This installer is for x64 Windows only.'
}
New-Item -ItemType Directory -Force -Path $ProjectDownloads, $ProjectExtract, $ProjectBin | Out-Null
Invoke-WebRequest -UseBasicParsing -Uri `
    'https://eternallybored.org/misc/wget/releases/wget-1.25.0-win64.zip' `
    -OutFile $ProjectArchive
$ProjectActualHash = (Get-FileHash -LiteralPath $ProjectArchive -Algorithm SHA256).Hash
if ($ProjectActualHash -ne $ProjectExpectedHash) {
    throw "Wget archive SHA256 mismatch. Download was NOT extracted or executed. Actual: $ProjectActualHash"
}
Expand-Archive -LiteralPath $ProjectArchive -DestinationPath $ProjectExtract -Force
$ProjectExecutables = @(Get-ChildItem -LiteralPath $ProjectExtract -Filter wget.exe -File -Recurse)
if ($ProjectExecutables.Count -ne 1) {
    throw 'Expected exactly one wget.exe in the verified archive.'
}
Copy-Item -LiteralPath $ProjectExecutables[0].FullName -Destination (Join-Path $ProjectBin 'wget.exe') -Force
. (Join-Path $PSScriptRoot 'env.ps1')
& (Join-Path $ProjectBin 'wget.exe') --version
if ($LASTEXITCODE -ne 0) {
    throw 'wget.exe --version failed.'
}
Write-Host 'Verified wget installed in .tools\bin. Run .\scripts\setup.ps1 again.'
