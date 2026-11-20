# Sprint 16 MSVC refold-include fix. Test this complete Sprint 16 tree ONLY.
# Keep the user's Sprint 11 verified build as rollback.
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
# Python 3.14 on Windows may default to cp1252; all project source is UTF-8.
# Keep this for child audit processes and project-local Python venvs as well.
$env:PYTHONUTF8='1'
$env:PYTHONIOENCODING='utf-8'
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
  if($LASTEXITCODE -ne 0){throw 'Sprint 16 source manifest preflight failed'}
  & $pythonExe @pythonPrefix .\tools\audit_full_source.py --root .
  if($LASTEXITCODE -ne 0){throw 'Sprint 16 full-tree static audit failed'}
  & $pythonExe @pythonPrefix .\tools\verify_130_tests.py
  if($LASTEXITCODE -ne 0){throw 'Historical native test inventory changed'}
  & $pythonExe @pythonPrefix -m unittest discover -s tests -p 'test_sprint16_integrated_readiness.py' -q
  if($LASTEXITCODE -ne 0){throw 'Sprint 16 integrated revision workflow tests failed'}
  & cmake -S $root -B $build "-DCMAKE_TOOLCHAIN_FILE=$toolchain" '-DVCPKG_TARGET_TRIPLET=x64-windows' '-DCMAKE_BUILD_TYPE=Release'
  if($LASTEXITCODE -ne 0){throw 'Sprint 16 CMake configuration failed'}
  # Every executable referenced by the focused CTest filter must be built.
  # In the previous Sprint 16 launcher, articulated_assembly_v170_tests was
  # omitted, so its CTest was NOT_RUN even though CADWorkbench.exe built.
  foreach($target in @('cadcore','articulated_assembly_v170_tests',
                       'articulated_assembly_scene_v170_tests',
                       'document_audit_tests','cadcore_tests','CADWorkbench')) {
    Write-Host "Early native compile: $target" -ForegroundColor Cyan
    & cmake --build $build --config Release --target $target --parallel 1
    if($LASTEXITCODE -ne 0){throw "Sprint 16 early MSVC build failed: $target"}
  }
  & ctest --test-dir $build -C Release --output-on-failure `
      -R '^(cadcore_v170_articulated_scene_transaction|cadcore_document_audit|cadcore_folded_sheet_metal|cadcore_v170_articulated_closed_loop)$'
  if($LASTEXITCODE -ne 0){throw 'Sprint 16 focused native regression failed'}
  Write-Host 'PASS: four Sprint 16 focused native regression tests' -ForegroundColor Green
  if(!$FullAudit){
    Write-Warning 'TARGETED ONLY: full 130 CTests and acceptance remain pending (GUI target was compiled).' 
    exit 4
  }
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
      (Join-Path $root 'scripts\RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1') `
      -VcpkgRoot $VcpkgRoot -Parallel $Parallel -SetupPythonEnv
  exit $LASTEXITCODE
} finally {Pop-Location}
