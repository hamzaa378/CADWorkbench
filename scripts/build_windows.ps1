[CmdletBinding()]
param(
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
    throw "Set VCPKG_ROOT to your bootstrapped vcpkg folder, for example: `$env:VCPKG_ROOT = 'C:\vcpkg'"
}
$vcpkgExe = Join-Path $env:VCPKG_ROOT 'vcpkg.exe'
$toolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'
if (-not (Test-Path -LiteralPath $vcpkgExe) -or -not (Test-Path -LiteralPath $toolchain)) {
    throw "VCPKG_ROOT does not point to a bootstrapped vcpkg checkout: $env:VCPKG_ROOT"
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakeExe = $null
if ($cmakeCommand) { $cmakeExe = $cmakeCommand.Source }
if (-not $cmakeExe) {
    $vsRoot = Join-Path $env:ProgramFiles 'Microsoft Visual Studio'
    $cmakeExe = Get-ChildItem -LiteralPath $vsRoot -Filter cmake.exe -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake\.exe$' } |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $cmakeExe) {
    throw 'CMake was not found. Install the CMake tools with the Visual Studio C++ workload or add CMake to PATH.'
}
$ctestExe = Join-Path (Split-Path -Parent $cmakeExe) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctestExe)) {
    $ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
    if ($ctestCommand) { $ctestExe = $ctestCommand.Source }
}
if (-not $ctestExe) {
    throw 'CTest was not found beside CMake or on PATH.'
}

$buildDir = Join-Path $repoRoot 'build'
Push-Location $repoRoot
try {
    & $cmakeExe -S $repoRoot -B $buildDir `
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
        '-DVCPKG_TARGET_TRIPLET=x64-windows' `
        '-DCMAKE_BUILD_TYPE=Release'
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

    & $cmakeExe --build $buildDir --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw "Windows Release build failed with exit code $LASTEXITCODE" }

    if (-not $SkipTests) {
        & $ctestExe --test-dir $buildDir -C Release --output-on-failure --parallel 4
        if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE" }
    }

    Write-Host "CADWorkbench.exe: $(Join-Path $buildDir 'Release\CADWorkbench.exe')"
    if ($SkipTests) { Write-Host 'Regression tests were skipped.' }
}
finally {
    Pop-Location
}
