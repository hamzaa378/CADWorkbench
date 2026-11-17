[CmdletBinding()]
param([string]$Label='Two-hour backend development block')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $python=Get-Command py -ErrorAction SilentlyContinue
    if($python){ & py -3 .\tools\dev_session.py start --root . --label $Label }
    else { & python .\tools\dev_session.py start --root . --label $Label }
    if($LASTEXITCODE -ne 0){throw 'Could not create clean-baseline development session.'}
    Write-Host 'Begin focused coding. End by running FINISH_TWO_HOUR_DEV_SESSION.ps1.' -ForegroundColor Cyan
} finally {Pop-Location}
