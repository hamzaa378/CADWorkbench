# Run from a freshly extracted Sprint 6 source archive, in a new PowerShell.
# Fail EARLY on the 3 changed native areas before the slow full application build.
# A targeted CTest pass does not replace the complete 130 CTests, Python suite,
# or 60 manual design-acceptance cases.
[CmdletBinding()]
param(
    [string]$VcpkgRoot='C:\vcpkg',
    [ValidateRange(1,8)][int]$Parallel=2,
    [switch]$TriageOnly,
    [switch]$SkipPythonEnvSetup
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$env:VCPKG_ROOT=$VcpkgRoot
$build=Join-Path $root 'build'
$toolchain=Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$qa=Join-Path $build 'v170_comprehensive_audit'
New-Item -ItemType Directory -Force -Path $qa | Out-Null
Push-Location -LiteralPath $root
try {
    if(!(Test-Path -LiteralPath $toolchain -PathType Leaf)) {throw "Vcpkg toolchain missing: $toolchain"}
    $python=Get-Command py -ErrorAction SilentlyContinue
    if($python){$pythonArgs=@('-3') ;$pythonExe=$python.Source}
    else{$pythonExe=(Get-Command python -ErrorAction Stop).Source;$pythonArgs=@()}
    # Pure-source defects cost seconds, not a whole native rebuild.
    & $pythonExe @pythonArgs .\tools\preflight_major_build.py --root .
    if($LASTEXITCODE -ne 0){throw 'Source integrity preflight failed; native compilation was not started.'}
    & $pythonExe @pythonArgs .\tools\audit_full_source.py --root .
    if($LASTEXITCODE -ne 0){throw 'Full source audit failed; native compilation was not started.'}
    & $pythonExe @pythonArgs -m unittest discover -s tests -p 'test_sprint6_integrated_source_contract.py' -q
    if($LASTEXITCODE -ne 0){throw 'Sprint 6 source-contract tests failed.'}
    # This configure is identical to scripts/build_windows.ps1 and does not
    # compile the massive Qt GUI translation unit.
    & cmake -S $root -B $build "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
        '-DVCPKG_TARGET_TRIPLET=x64-windows' '-DCMAKE_BUILD_TYPE=Release'
    if($LASTEXITCODE -ne 0){throw 'CMake configure failed; inspect compiler/vcpkg diagnostics.'}
    # Build one target at a time: concurrent vcpkg Qt DLL deployment from
    # multiple MSBuild targets has caused MSB3075/locked-file errors.
    foreach($target in @('cadcore','face_lineage_v170_tests',
                         'shape_feature_tree_tests','document_audit_tests',
                         'interleaved_history_v170_tests')) {
        Write-Host "Early compile: $target" -ForegroundColor Cyan
        & cmake --build $build --config Release --target $target --parallel 1
        if($LASTEXITCODE -ne 0){throw "Early native compile failed at $target. Full app build was skipped."}
    }
    $pattern='^(cadcore_v170_face_lineage|cadcore_universal_feature_tree|cadcore_document_audit|cadcore_v170_interleaved_history)$'
    & ctest --test-dir $build -C Release --output-on-failure -R $pattern
    if($LASTEXITCODE -ne 0){throw 'Focused native regeneration/topology/drawing tests FAILED; full app build skipped.'}
    Write-Host 'PASS: early native compilation and focused CTests.' -ForegroundColor Green
    if($TriageOnly) {
        Write-Warning 'TRIAGE ONLY: full 130 CTests, Python inventory and 60 manual cases are NOT passed.'
        exit 4
    }
    $runner=Join-Path $root 'scripts\RUN_BACKEND_FULL_REGRESSION_WITH_DIAGNOSTICS.ps1'
    $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$runner,
            '-VcpkgRoot',$VcpkgRoot,'-Parallel',"$Parallel")
    if(-not $SkipPythonEnvSetup){$args+='-SetupPythonEnv'}
    & powershell.exe @args
    $code=$LASTEXITCODE
    if($code -eq 0 -or $code -eq 3) {exit $code}
    throw "Full native regression failed ($code); inspect $qa"
} finally {Pop-Location}
