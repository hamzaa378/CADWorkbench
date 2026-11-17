[CmdletBinding()]
param([string]$VcpkgRoot='C:\vcpkg')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    $buildFolder=Join-Path $root 'build'
    if (-not (Test-Path -LiteralPath $buildFolder)) {
        New-Item -Path $buildFolder -ItemType Directory -Force | Out-Null
    }
    $scorecard=Join-Path $buildFolder 'V170_ACCEPTANCE_RESULTS.csv'
    if (-not (Test-Path -LiteralPath $scorecard)) {
        Copy-Item -LiteralPath (Join-Path $root 'docs/V170_ACCEPTANCE_RESULTS.csv') `
            -Destination $scorecard
    }
    $junit=Join-Path $buildFolder 'cadworkbench_release_ctest.xml'
    if (Test-Path -LiteralPath $junit) {
        Remove-Item -LiteralPath $junit -Force # never accept a stale previous-run report
    }
    $nativeOkay=$false
    try {
        & (Join-Path $root 'RUN_WINDOWS_TESTS.ps1') -VcpkgRoot $VcpkgRoot
        $nativeOkay=$true
    } catch {
        Write-Host "Windows native build/CTest BLOCKED: $_" -ForegroundColor Red
    }
    $python=Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) { throw 'Python is required to score the 60-scenario acceptance CSV.' }
    & $python.Source (Join-Path $root 'tools/score_v170_gate.py') `
        --csv $scorecard `
        --junit $junit `
        --output (Join-Path $root 'build/v170_acceptance_gate.json')
    if (-not $nativeOkay -or $LASTEXITCODE -ne 0) {
        throw 'Release gate NOT passed. Read build/v170_acceptance_gate.json and the CTest JUnit.'
    }
    Write-Host 'Native tests and documented 85/85/65 acceptance gate passed.' -ForegroundColor Green
} finally { Pop-Location }
