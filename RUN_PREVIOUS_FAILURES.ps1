[CmdletBinding()]
param([string]$VcpkgRoot = 'C:\vcpkg', [switch]$OnlyInterleavedHistory)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:VCPKG_ROOT = $VcpkgRoot
$manifest = Join-Path $root 'SHA256SUMS.txt'
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
    throw "Source hash manifest is missing: $manifest"
}
$count = 0
foreach ($line in Get-Content -LiteralPath $manifest) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line -notmatch '^([A-Fa-f0-9]{64})  (.+)$') { throw "Bad source manifest entry: $line" }
    $expect = $Matches[1]
    $file = Join-Path $root $Matches[2]
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing source: $file" }
    $actual = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    if (-not $actual.Equals($expect,[System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Source integrity mismatch: $file"
    }
    $count++
}
if ($count -lt 200) { throw "Incomplete source manifest ($count entries)" }
Write-Host "Verified $count source checksums" -ForegroundColor Green

& (Join-Path $root 'scripts\build_windows.ps1') -SkipTests
if ($LASTEXITCODE -ne 0) { throw "Windows build failed: $LASTEXITCODE" }

$ctestCmd = Get-Command ctest -ErrorAction SilentlyContinue
$ctestExe = if ($ctestCmd) { $ctestCmd.Source } else { $null }
if (-not $ctestExe) {
    $vsRoot = Join-Path $env:ProgramFiles 'Microsoft Visual Studio'
    $ctestExe = Get-ChildItem -LiteralPath $vsRoot -Filter ctest.exe -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\ctest\.exe$' } |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $ctestExe) { throw 'CTest is unavailable' }

$pattern = if ($OnlyInterleavedHistory) {
    '^cadcore_v170_interleaved_history$'
} else {
    '^cadcore_v170_(interleaved_history|assembly_mate_document|mixed_feature_history|curved_face_lineage|inplace_sketch_persistence|inplace_two_stage_corruption)$'
}
$reportName = if ($OnlyInterleavedHistory) { 'cadworkbench_interleaved_history.xml' } else { 'cadworkbench_six_prior_failures.xml' }
$report = Join-Path (Join-Path $root 'build') $reportName
& $ctestExe --test-dir (Join-Path $root 'build') -C Release -R $pattern --output-on-failure --parallel 2 --output-junit $report
if ($LASTEXITCODE -ne 0) { throw "Native regression failed. See $report" }
if ($OnlyInterleavedHistory) {
    Write-Host 'The interleaved-history CTest passed.' -ForegroundColor Green
} else {
    Write-Host 'All six previously failing CTests passed.' -ForegroundColor Green
}
Write-Host 'Next run the full gate: .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg'
