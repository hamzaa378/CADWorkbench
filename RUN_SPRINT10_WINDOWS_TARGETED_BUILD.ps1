# Sprint 10 is a separate source candidate. Preserve the user's verified Sprint 9 build.
# Run from ANY fresh Windows PowerShell window after extracting the complete ZIP.
[CmdletBinding()]
param(
  [string]$VcpkgRoot='C:\vcpkg',
  [ValidateRange(1,8)][int]$Parallel=2,
  [switch]$FullAudit
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$build=Join-Path $root 'build'
$toolchain=Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if(!(Test-Path -LiteralPath $toolchain -PathType Leaf)){throw "Missing vcpkg toolchain: $toolchain"}
if(!(Get-Command cmake.exe -ErrorAction SilentlyContinue)){throw 'CMake is missing from PATH'}
if(!(Get-Command ctest.exe -ErrorAction SilentlyContinue)){throw 'CTest is missing from PATH'}
$env:VCPKG_ROOT=$VcpkgRoot
$py=(Get-Command py -ErrorAction SilentlyContinue)
if($py){$pythonExe=$py.Source;$pythonPrefix=@('-3')}
else{$pythonExe=(Get-Command python -ErrorAction Stop).Source;$pythonPrefix=@()}
Push-Location -LiteralPath $root
try {
  & $pythonExe @pythonPrefix .\tools\preflight_major_build.py --root .
  if($LASTEXITCODE -ne 0){throw 'Sprint 10 source/hash preflight failed'}
  & $pythonExe @pythonPrefix .\tools\audit_full_source.py --root .
  if($LASTEXITCODE -ne 0){throw 'Sprint 10 static audit failed'}
  & $pythonExe @pythonPrefix -m unittest discover -s tests -p 'test_sprint10_workflow_contract.py' -q
  if($LASTEXITCODE -ne 0){throw 'Sprint 10 source integration contracts failed'}
  & cmake -S $root -B $build "-DCMAKE_TOOLCHAIN_FILE=$toolchain" '-DVCPKG_TARGET_TRIPLET=x64-windows' '-DCMAKE_BUILD_TYPE=Release'
  if($LASTEXITCODE -ne 0){throw 'Sprint 10 configure failed'}
  foreach($target in @('cadcore','workshop_engineering_v164_tests',
                       'document_audit_tests','articulated_assembly_scene_v170_tests')) {
    Write-Host "Early native compile: $target" -ForegroundColor Cyan
    & cmake --build $build --config Release --target $target --parallel 1
    if($LASTEXITCODE -ne 0){throw "Sprint 10 MSVC compilation failed: $target"}
  }
  & ctest --test-dir $build -C Release --output-on-failure `
     -R '^(cadcore_v164_tolerance_stacks|cadcore_document_audit|cadcore_v170_articulated_scene_transaction)$'
  if($LASTEXITCODE -ne 0){throw 'Sprint 10 focused native CTests failed'}
  Write-Host 'PASS: all three Sprint 10 focused Windows CTests' -ForegroundColor Green
  if(!$FullAudit){
    Write-Warning 'TARGETED ONLY: full CADWorkbench.exe and 130 native CTests still pending.'
    exit 4
  }
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
      (Join-Path $root 'scripts\RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1') `
      -VcpkgRoot $VcpkgRoot -Parallel $Parallel -SetupPythonEnv
  exit $LASTEXITCODE
} finally { Pop-Location }
