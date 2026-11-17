# Sprint 9, separate from the user's running Sprint 8 build.
# First test cheap source/target compilation; optionally perform the full gate.
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
$env:VCPKG_ROOT=$VcpkgRoot
$py=(Get-Command py -ErrorAction SilentlyContinue)
if($py){$pythonExe=$py.Source;$pythonPrefix=@('-3')}
else{$pythonExe=(Get-Command python -ErrorAction Stop).Source;$pythonPrefix=@()}
Push-Location -LiteralPath $root
try {
  & $pythonExe @pythonPrefix .\tools\preflight_major_build.py --root .
  if($LASTEXITCODE -ne 0){throw 'Sprint 9 manifest/source preflight failed'}
  & $pythonExe @pythonPrefix .\tools\audit_full_source.py --root .
  if($LASTEXITCODE -ne 0){throw 'Sprint 9 static source audit failed'}
  & $pythonExe @pythonPrefix -m unittest discover -s tests -p 'test_sprint9_inspection_motion_contract.py' -q
  if($LASTEXITCODE -ne 0){throw 'Sprint 9 source integration tests failed'}
  & cmake -S $root -B $build "-DCMAKE_TOOLCHAIN_FILE=$toolchain" '-DVCPKG_TARGET_TRIPLET=x64-windows' '-DCMAKE_BUILD_TYPE=Release'
  if($LASTEXITCODE -ne 0){throw 'Sprint 9 CMake configuration failed'}
  # Independent targets are built sequentially to avoid concurrent MSBuild/vcpkg DLL collisions.
  foreach($target in @('cadcore','workshop_engineering_v164_tests','articulated_assembly_v170_tests')) {
    Write-Host "Early native compile: $target" -ForegroundColor Cyan
    & cmake --build $build --config Release --target $target --parallel 1
    if($LASTEXITCODE -ne 0){throw "Native compile failed at $target"}
  }
  & ctest --test-dir $build -C Release --output-on-failure `
    -R '^(cadcore_v164_tolerance_stacks|cadcore_v170_articulated_closed_loop)$'
  if($LASTEXITCODE -ne 0){throw 'Sprint 9 focused native CTests failed'}
  Write-Host 'PASS: Sprint 9 early compilation and both focused native CTests.' -ForegroundColor Green
  if(!$FullAudit) {
    Write-Warning 'TARGETED ONLY: the full MSVC application build, 130 native CTests and 60 manual acceptance cases have NOT run.'
    exit 4
  }
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
     (Join-Path $root 'scripts\RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1') `
     -VcpkgRoot $VcpkgRoot -Parallel $Parallel -SetupPythonEnv
  exit $LASTEXITCODE
} finally { Pop-Location }
