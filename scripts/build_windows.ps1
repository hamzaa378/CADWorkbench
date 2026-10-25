param(
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
Push-Location $root
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $installations = @()
    if (Test-Path -LiteralPath $vswhere) {
        $installationPaths = @(& $vswhere -all -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
        $installationVersions = @(& $vswhere -all -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion)
        for ($index = 0; $index -lt $installationPaths.Count; $index++) {
            $installations += [pscustomobject]@{
                installationPath = $installationPaths[$index]
                installationVersion = $installationVersions[$index]
            }
        }
    }

    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    $cmakeExe = if ($cmakeCommand) { $cmakeCommand.Source } else { $null }
    if (-not $cmakeExe) {
        foreach ($installation in $installations) {
            $candidate = Join-Path $installation.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $candidate) { $cmakeExe = $candidate; break }
        }
    }
    if (-not $cmakeExe) { throw 'CMake was not found. Install the Visual Studio C++ workload with CMake tools, or add cmake.exe to PATH.' }
    $ctestExe = Join-Path (Split-Path $cmakeExe -Parent) 'ctest.exe'
    if (-not (Test-Path -LiteralPath $ctestExe)) {
        $ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
        if ($ctestCommand) { $ctestExe = $ctestCommand.Source }
    }

    $availableGenerators = @([regex]::Matches((& $cmakeExe --help | Out-String), '(?m)^\s*\*?\s*(Visual Studio \d+ \d{4})\s*=') | ForEach-Object { $_.Groups[1].Value })
    $generator = $null
    foreach ($installation in $installations) {
        $major = ($installation.installationVersion -split '\.')[0]
        $generator = $availableGenerators | Where-Object { $_ -match "^Visual Studio $major " } | Select-Object -First 1
        if ($generator) { break }
    }
    if (-not $generator) { throw 'No installed Visual Studio C++ toolset has a matching CMake generator. Install the C++ workload and CMake tools.' }
    if (-not $SkipTests -and -not (Test-Path -LiteralPath $ctestExe)) { throw 'CTest was not found. Install CMake tools or run with -SkipTests to build without running the test suite.' }

    if (-not $env:VCPKG_ROOT) { throw 'Set VCPKG_ROOT to a bootstrapped vcpkg checkout. See README.md.' }
    $toolchain = Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'
    if (-not (Test-Path $toolchain)) { throw "Missing vcpkg toolchain: $toolchain" }

    # A vcpkg manifest project installs into its own project-local vcpkg_installed folder.
    # Do NOT assume the classic-mode C:\vcpkg\installed directory.
    $installed = Join-Path $root 'vcpkg_installed'
    $runtime = Join-Path $installed 'x64-windows/bin'
    Write-Host "Configuring Windows x64 with $generator + vcpkg manifest dependencies..." -ForegroundColor Cyan
    & $cmakeExe -S . -B build -G $generator -A x64 `
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
        "-DVCPKG_INSTALLED_DIR=$installed" `
        '-DVCPKG_TARGET_TRIPLET=x64-windows' `
        '-DBUILD_TESTING=ON'
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }

    Write-Host 'Compiling Release target...' -ForegroundColor Cyan
    & $cmakeExe --build build --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed. No release approved.' }

    $qtDeploy = Join-Path $installed 'x64-windows/tools/Qt6/bin/windeployqt.exe'
    if (-not (Test-Path -LiteralPath $qtDeploy)) {
        $found = Get-ChildItem (Join-Path $installed 'x64-windows') -Filter windeployqt.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $found) { throw 'windeployqt was not found in the vcpkg triplet; the Qt platform plugin cannot be staged.' }
        $qtDeploy = $found.FullName
    }
    $executable = Join-Path $root 'build/Release/CADWorkbench.exe'
    Write-Host 'Staging Qt runtime plugins beside CADWorkbench.exe...' -ForegroundColor Cyan
    & $qtDeploy --release --no-translations $executable
    if ($LASTEXITCODE -ne 0) { throw 'Qt runtime deployment failed. Check the Qt platform plugin files.' }

    if (-not $SkipTests) {
        if (Test-Path $runtime) { $env:PATH = "$runtime;$env:PATH" }
        Write-Host 'Running native C++ geometry regression tests...' -ForegroundColor Cyan
        & $ctestExe --test-dir build -C Release --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'CTest failed. No release approved.' }
    }
    Write-Host 'Build completed: build\Release\CADWorkbench.exe' -ForegroundColor Green
    Write-Host 'Next: manual validation in docs\QA_MATRIX.md. Not a verified release until Windows GUI tests pass.'
}
finally { Pop-Location }
