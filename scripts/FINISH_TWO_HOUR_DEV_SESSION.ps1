[CmdletBinding()]
param([string]$VcpkgRoot='C:\vcpkg',[int]$Parallel=4,[switch]$SetupPythonEnv)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $python=Get-Command py -ErrorAction SilentlyContinue
    $exe=if($python){'py'}else{'python'}
    $prefix=if($python){@('-3')}else{@()}
    & $exe @prefix .\tools\dev_session.py stage --root .
    if($LASTEXITCODE -ne 0){throw 'Staging failed; no audit was started.'}
    & $exe @prefix .\tools\preflight_major_build.py --root .
    if($LASTEXITCODE -ne 0){throw 'Source preflight failed.'}
    # The comprehensive audit is deliberately kept intact: it builds the
    # application, runs all 130 CTests and the entire Python/source inventory.
    # Exit 3 means all automation passed but manual acceptance is pending.
    $auditArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',
                 '.\scripts\RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1',
                 '-VcpkgRoot',$VcpkgRoot,'-Parallel',"$Parallel")
    if($SetupPythonEnv){$auditArgs+='-SetupPythonEnv'}
    & powershell.exe @auditArgs
    $auditCode=$LASTEXITCODE
    if($auditCode -notin @(0,3)){
        throw "FULL REGRESSION FAILED (exit $auditCode). Review build/v170_comprehensive_audit before restaging."
    }
    & $exe @prefix .\tools\dev_session.py finish --root . --audit-exit-code $auditCode
    if($LASTEXITCODE -ne 0){throw 'Post-test evidence verification failed. Session remains active.'}
    Write-Host 'PASS: complete backend coding session validated.' -ForegroundColor Green
    if($auditCode -eq 3){Write-Host 'MANUAL ACCEPTANCE STILL PENDING (NOT a final release).' -ForegroundColor Yellow}
} finally {Pop-Location}
