[CmdletBinding()]
param(
    [string]$VcpkgRoot='C:\vcpkg',
    [int]$Parallel=4,
    [switch]$SetupPythonEnv
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $PSScriptRoot
$qa=Join-Path $root 'build\v170_comprehensive_audit'
New-Item -ItemType Directory -Path $qa -Force | Out-Null
$fullTranscript=Join-Path $qa 'backend_full_console.log'
$diagnosis=Join-Path $qa 'backend_diagnosis.json'
Push-Location $root
$started=$false
$auditCode=1
try {
    Start-Transcript -LiteralPath $fullTranscript -Force | Out-Null
    $started=$true
    $auditArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',
                 '.\RUN_V170_COMPREHENSIVE_AUDIT.ps1',
                 '-VcpkgRoot',$VcpkgRoot,'-Parallel',"$Parallel")
    if($SetupPythonEnv){$auditArgs+='-SetupPythonEnv'}
    & powershell.exe @auditArgs
    $auditCode=$LASTEXITCODE
    Write-Host "Full audit process exit code: $auditCode"
} finally {
    if($started){Stop-Transcript | Out-Null}
    Pop-Location
}
$python=Get-Command py -ErrorAction SilentlyContinue
if($python){& py -3 (Join-Path $root 'tools\diagnose_build.py') --log $fullTranscript --out $diagnosis --expected-python 952}
else {& python (Join-Path $root 'tools\diagnose_build.py') --log $fullTranscript --out $diagnosis --expected-python 952}
$diagnosticCode=$LASTEXITCODE
Write-Host "Diagnostic JSON: $diagnosis"
if($auditCode -notin @(0,3)){
    Write-Error "Full regression failed (exit $auditCode). Diagnosis and native logs retained."
    exit 1
}
if($diagnosticCode -ne 0){
    Write-Error 'Diagnostic evidence is incomplete or contains errors; no release pass.'
    exit 1
}
Write-Host 'PASS: full automated audit and independent diagnostic summary.' -ForegroundColor Green
if($auditCode -eq 3){
    Write-Host 'MANUAL ACCEPTANCE PENDING: this is not an engineering-approved release.' -ForegroundColor Yellow
}
exit $auditCode
