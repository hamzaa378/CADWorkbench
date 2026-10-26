$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$exe = Join-Path $root 'build/Release/CADWorkbench.exe'
if (-not (Test-Path $exe)) { throw 'Build the Release target first.' }
$triplet = Join-Path $root 'vcpkg_installed/x64-windows'
$bin = Join-Path $triplet 'bin'
if (-not (Test-Path $bin)) { throw "vcpkg manifest runtime missing: $bin" }
$dest = Join-Path $root 'dist/CADWorkbench_v1_0_Windows_x64_developer_preview'
New-Item -ItemType Directory -Force $dest | Out-Null
Copy-Item $exe $dest -Force
# Development package only: broad DLL copy (not curated for license/redeployment).
Get-ChildItem $bin -Filter '*.dll' | Copy-Item -Destination $dest -Force
$qtDeploy = Join-Path $triplet 'tools/Qt6/bin/windeployqt.exe'
if (-not (Test-Path $qtDeploy)) {
    $found = Get-ChildItem $triplet -Filter windeployqt.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $found) { throw 'windeployqt not found in vcpkg triplet.' }
    $qtDeploy = $found.FullName
}
& $qtDeploy --release --no-translations (Join-Path $dest 'CADWorkbench.exe')
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
$licenseDest = Join-Path $dest 'THIRD_PARTY_LICENSES'
New-Item -ItemType Directory -Force $licenseDest | Out-Null
$share = Join-Path $triplet 'share'
Get-ChildItem $share -Directory | ForEach-Object {
    $copyright = Join-Path $_.FullName 'copyright'
    if (Test-Path -LiteralPath $copyright) {
        Copy-Item -LiteralPath $copyright -Destination (Join-Path $licenseDest ($_.Name + '.txt')) -Force
    }
}
Write-Host "Developer preview staged to $dest. Test on a clean Windows VM before redistribution." -ForegroundColor Yellow
