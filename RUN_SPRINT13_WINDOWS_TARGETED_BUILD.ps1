# Sprint 13 PRELIMINARY. Keep the user's Sprint 11 verified build as rollback.
# Targeted: run preflight and compile core + affected native suites before GUI.
# -FullAudit: full MSVC GUI build, 130 native tests, Python source tests and acceptance gate.
[CmdletBinding()]
param(
  [string]$VcpkgRoot='C:\vcpkg',
  [ValidateRange(1,8)][int]$Parallel=4,
  [switch]$FullAudit
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$build=Join-Path $root 'build'
$toolchain=Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if(!(Test-Path -LiteralPath $toolchain -PathType Leaf)){throw "Missing vcpkg toolchain: $toolchain"}
foreach($cmd in @('cmake.exe','ctest.exe')){
  if(!(Get-Command $cmd -ErrorAction SilentlyContinue)){throw "$cmd missing from PATH"}
}
$env:VCPKG_ROOT=$VcpkgRoot
$py=(Get-Command py -ErrorAction SilentlyContinue)
if($py){$pythonExe=$py.Source;$pythonPrefix=@('-3')}
else{$pythonExe=(Get-Command python -ErrorAction Stop).Source;$pythonPrefix=@()}
Push-Location -LiteralPath $root
try {
  & $pythonExe @pythonPrefix .\tools\preflight_major_build.py --root .
  if($LASTEXITCODE -ne 0){throw 'Sprint 13 source manifest preflight failed'}
  & $pythonExe @pythonPrefix .\tools\audit_full_source.py --root .
  if($LASTEXITCODE -ne 0){throw 'Sprint 13 full-tree static audit failed'}
  & $pythonExe @pythonPrefix .\tools\verify_130_tests.py
  if($LASTEXITCODE -ne 0){throw 'Historical native test inventory changed'}
  & $pythonExe @pythonPrefix -m unittest discover -s tests -p 'test_sprint13_flat_production_workflow.py' -q
  if($LASTEXITCODE -ne 0){throw 'Sprint 13 integrated revision workflow tests failed'}
  & cmake -S $root -B $build "-DCMAKE_TOOLCHAIN_FILE=$toolchain" '-DVCPKG_TARGET_TRIPLET=x64-windows' '-DCMAKE_BUILD_TYPE=Release'
  if($LASTEXITCODE -ne 0){throw 'Sprint 13 CMake configuration failed'}
  foreach($target in @('cadcore','cadcore_tests','document_audit_tests',
                       'workshop_engineering_v164_tests','shape_feature_tree_tests')) {
    Write-Host "Early native compile: $target" -ForegroundColor Cyan
    & cmake --build $build --config Release --target $target --parallel 1
    if($LASTEXITCODE -ne 0){throw "Sprint 13 early MSVC build failed: $target"}
  }
  & ctest --test-dir $build -C Release --output-on-failure `
      -R '^(cadcore_sheet_metal_flat_pattern|cadcore_folded_sheet_metal|cadcore_document_audit|cadcore_universal_feature_tree)$'
  if($LASTEXITCODE -ne 0){throw 'Sprint 13 focused native regression failed'}
  Write-Host 'PASS: four Sprint 13 focused native regression tests' -ForegroundColor Green
  if(!$FullAudit){
    Write-Warning 'TARGETED ONLY: complete CADWorkbench.exe, 130 native CTests and acceptance remain pending.'
    exit 4
  }
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
      (Join-Path $root 'scripts\RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1') `
      -VcpkgRoot $VcpkgRoot -Parallel $Parallel -SetupPythonEnv
  exit $LASTEXITCODE
} finally {Pop-Location}
