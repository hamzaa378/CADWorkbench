[CmdletBinding()]
param([switch]$PythonFocused)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root 'build'
if(-not (Test-Path -LiteralPath (Join-Path $build 'CMakeCache.txt'))){
    throw 'No configured Windows CMake build found. Run the full audit once on this source first.'
}
Push-Location $root
try {
    # Fast, targeted feedback during a coding block. Does not edit manifests.
    & cmake --build $build --config Release --target `
        feature_dependency_plan_v166_tests workshop_engineering_v164_tests --parallel 2
    if($LASTEXITCODE -ne 0){throw 'Focused C++ compilation failed.'}
    & ctest --test-dir $build -C Release --output-on-failure `
        -R '^(cadcore_v166_sparse_dependency_planner|cadcore_v164_tolerance_stacks)$'
    if($LASTEXITCODE -ne 0){throw 'Focused CTest regression failed.'}
    if($PythonFocused){
        & py -3 -m unittest discover -s tests -p 'test_backend_build_diagnostics.py' -q
        if($LASTEXITCODE -ne 0){throw 'Build-diagnostic Python tests failed.'}
    }
    Write-Host 'PASS focused checks; full native + Python audit still required before releasing.' -ForegroundColor Green
} finally {Pop-Location}
