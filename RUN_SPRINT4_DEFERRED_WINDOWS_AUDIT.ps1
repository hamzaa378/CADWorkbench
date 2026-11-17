# Launch from a freshly extracted combined source archive in Windows PowerShell.
# This script runs preflight, the full 130-CTest gate, and diagnoses results.
# It does NOT automatically authorize manufacturing or mark manual acceptance.
[CmdletBinding()]
param(
    [string]$VcpkgRoot='C:\vcpkg',
    [ValidateRange(1,16)][int]$Parallel=4,
    [switch]$SkipPythonEnvSetup
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
Push-Location -LiteralPath $root
try {
    $python=Get-Command py -ErrorAction SilentlyContinue
    if($python) {
        & py -3 .\tools\preflight_major_build.py --root .
    } else {
        $python=Get-Command python -ErrorAction Stop
        & python .\tools\preflight_major_build.py --root .
    }
    if($LASTEXITCODE -ne 0) { throw 'Source preflight failed; stop before compiling.' }
    $runner=Join-Path $root 'scripts\RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1'
    $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$runner,
            '-VcpkgRoot',$VcpkgRoot,'-Parallel',"$Parallel")
    if(-not $SkipPythonEnvSetup){ $args+='-SetupPythonEnv' }
    & powershell.exe @args
    $auditCode=$LASTEXITCODE
    Write-Host "Full Windows audit exit code: $auditCode"
    # Diagnostic-only focused replay. A focused PASS is NEVER substituted for
    # the missing/failed cases in the complete CTest inventory.
    $ctest=Get-Command ctest.exe -ErrorAction SilentlyContinue
    if($ctest -and (Test-Path (Join-Path $root 'build\CTestTestfile.cmake'))) {
        Write-Host 'Focused replay: grouped edge native regression (#45)' -ForegroundColor Cyan
        & ctest.exe --test-dir build -C Release --output-on-failure `
          -R '^cadcore_universal_feature_tree$'
        Write-Host "Focused native CTest exit code: $LASTEXITCODE"
    }
    Write-Host 'Reports: build\v170_comprehensive_audit\' -ForegroundColor Cyan
    if($auditCode -eq 0) { exit 0 }
    if($auditCode -eq 3) {
        Write-Warning 'Automated tests passed, but the 60 manual engineering cases are not signed off.'
        exit 3
    }
    Write-Error 'Native or Python audit failed; retain build logs and do not publish a release.'
    exit 1
} finally {
    Pop-Location
}
