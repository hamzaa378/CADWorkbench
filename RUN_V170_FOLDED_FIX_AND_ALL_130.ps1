[CmdletBinding()]
param(
    [string]$VcpkgRoot='C:\vcpkg',
    [switch]$SetupPythonEnv,
    [int]$Parallel=4
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$build=Join-Path $root 'build'
$qa=Join-Path $build 'v170_comprehensive_audit'
$env:VCPKG_ROOT=$VcpkgRoot
$env:CAD_V170_FOLDED_TRACE='1'
$env:PYTHONUTF8='1'
Push-Location $root
try {
    # Do the full build and 130-test audit in one command, preserving results
    # even if the folded regression remains red.  No tests are excluded.
    $params=@{VcpkgRoot=$VcpkgRoot;Parallel=$Parallel}
    if($SetupPythonEnv){$params.SetupPythonEnv=$true}
    & (Join-Path $root 'RUN_V170_COMPREHENSIVE_AUDIT.ps1') @params
} finally {Pop-Location}
