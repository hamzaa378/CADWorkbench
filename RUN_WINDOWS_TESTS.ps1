[CmdletBinding()]
param(
    [string]$VcpkgRoot = 'C:\vcpkg',
    [switch]$SkipSourceHashCheck
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:VCPKG_ROOT = $VcpkgRoot

Write-Host 'CADWorkbench v1.70 current-state verification' -ForegroundColor Cyan
Write-Host "Project root: $root"

if (-not $SkipSourceHashCheck) {
    $manifest = Join-Path $root 'SHA256SUMS.txt'
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "Source manifest missing: $manifest"
    }
    $checked = 0
    foreach ($line in (Get-Content -LiteralPath $manifest)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line -notmatch '^([a-fA-F0-9]{64})  (.+)$') {
            throw "Malformed hash entry: $line"
        }
        $expected = $Matches[1]
        $relative = $Matches[2]
        $path = Join-Path $root $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Missing source file: $relative"
        }
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if (-not $actual.Equals($expected, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Modified or corrupted source: $relative"
        }
        $checked++
    }
    if ($checked -lt 200) { throw "Only $checked files were checked: manifest may be incomplete." }
    Write-Host "Source integrity passed: $checked files" -ForegroundColor Green
}

Write-Host 'Configuring and building Windows Release, then running all native CTests...' -ForegroundColor Cyan
& (Join-Path $root 'scripts\verify_release_windows.ps1') -ExpectedTests 130
if ($LASTEXITCODE -ne 0) { throw "Native verification failed: exit code $LASTEXITCODE" }

Write-Host 'Native verification passed. For a GUI smoke test, launch:' -ForegroundColor Green
Write-Host (Join-Path $root 'build\Release\CADWorkbench.exe')
Write-Host 'Then test new/open/save, sketch edits, feature rebuild, assembly refresh, drawing regeneration and undo/redo manually.'
