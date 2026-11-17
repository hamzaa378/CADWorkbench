[CmdletBinding()]
param([string]$VcpkgRoot='C:\vcpkg')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$env:VCPKG_ROOT=$VcpkgRoot
Push-Location $root
try {
    # A development-only verification subset. This DOES NOT approve v1.70;
    # the two known failing tests stay registered for the full release gate.
    & (Join-Path $root 'scripts\build_windows.ps1') -SkipTests
    if ($LASTEXITCODE -ne 0) { throw "Release build failed with exit code $LASTEXITCODE" }
    $ctest=Get-Command ctest -ErrorAction SilentlyContinue
    if (-not $ctest) { throw 'CTest was not found on PATH. Run from a VS Developer PowerShell.' }
    $pattern='^(cadcore_parametric_foundation|cadcore_parametric_blind_hole|cadcore_v170_articulated_scene_transaction|cadcore_sheet_metal_flat_pattern)$'
    $junit=Join-Path $root 'build\v170_new_workflows_ctest.xml'
    & $ctest.Source --test-dir (Join-Path $root 'build') -C Release `
        -R $pattern --output-on-failure --output-junit $junit --parallel 1
    if ($LASTEXITCODE -ne 0) { throw "New-workflow regression failure. See $junit" }
    Write-Host 'Three focused workflow regressions passed. FULL RELEASE GATE STILL REQUIRED.' -ForegroundColor Green
    Write-Host 'Known deferred failures are NOT suppressed in the full CTest inventory.' -ForegroundColor Yellow
} finally { Pop-Location }
