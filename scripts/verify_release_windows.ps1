[CmdletBinding()]
param(
    [int]$ExpectedTests = 130,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build_windows.ps1') -SkipTests
    if ($LASTEXITCODE -ne 0) { throw 'Windows compilation failed.' }
}
$exe = Join-Path $build 'Release\CADWorkbench.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Missing Windows Release binary: $exe"
}
$ctest = Get-Command ctest -ErrorAction SilentlyContinue
$ctestExe = if ($ctest) { $ctest.Source } else { $null }
if (-not $ctestExe) {
    $vsRoot = Join-Path $env:ProgramFiles 'Microsoft Visual Studio'
    $ctestExe = Get-ChildItem -LiteralPath $vsRoot -Filter ctest.exe -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\ctest\.exe$' } |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $ctestExe) { throw 'CTest not found. Install CMake tools for Visual Studio.' }

$metadata = & $ctestExe --test-dir $build -C Release --show-only=json-v1 | Out-String
if ($LASTEXITCODE -ne 0) { throw 'CTest discovery failed.' }
$discovery = $metadata | ConvertFrom-Json
$registered = @($discovery.tests)
if ($registered.Count -ne $ExpectedTests) {
    throw "Expected $ExpectedTests native tests, but discovered $($registered.Count). No test was run."
}
$names = @($registered | ForEach-Object { $_.name })
if (@($names | Select-Object -Unique).Count -ne $ExpectedTests) {
    throw 'Duplicate native test names detected.'
}
$report = Join-Path $build 'cadworkbench_release_ctest.xml'
& $ctestExe --test-dir $build -C Release --output-on-failure --parallel 4 --output-junit $report
if ($LASTEXITCODE -ne 0) {
    throw "Release CTests failed; inspect $report"
}
if (-not (Test-Path -LiteralPath $report)) { throw 'CTest did not produce the expected JUnit report.' }
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Write-Host "Verified Windows Release executable: $exe"
Write-Host "Native tests passed: $ExpectedTests/$ExpectedTests"
Write-Host "Executable SHA-256: $hash"
Write-Host "JUnit evidence: $report"
