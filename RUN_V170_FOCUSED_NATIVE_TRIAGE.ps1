[CmdletBinding()]
param([string]$VcpkgRoot='C:\vcpkg')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$env:VCPKG_ROOT=$VcpkgRoot
Push-Location $root
try {
    Write-Host 'Building the full Windows Release application (not a test bypass)...' -ForegroundColor Cyan
    & (Join-Path $root 'scripts\build_windows.ps1') -SkipTests
    if($LASTEXITCODE -ne 0){throw "Windows Release compilation failed: $LASTEXITCODE"}
    $ctest=Get-Command ctest -ErrorAction SilentlyContinue
    $ctestExe=if($ctest){$ctest.Source}else{$null}
    if(-not $ctestExe){
        $vsRoot=Join-Path $env:ProgramFiles 'Microsoft Visual Studio'
        $ctestExe=Get-ChildItem -LiteralPath $vsRoot -Filter ctest.exe -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\ctest\.exe$' } |
            Select-Object -First 1 -ExpandProperty FullName
    }
    if(-not $ctestExe){throw 'CTest was not found. Install Visual Studio CMake tools.'}
    $pattern='^(cadcore_v170_articulated_scene_transaction|cadcore_v170_assembly_mate_document|cadcore_parametric_foundation|cadcore_folded_sheet_metal)$'
    $directory=Join-Path $root 'build'
    $log=Join-Path $directory 'v170_four_failure_diagnostics.log'
    $junit=Join-Path $directory 'v170_four_failure_diagnostics.xml'
    if(Test-Path -LiteralPath $junit){Remove-Item -LiteralPath $junit -Force}
    # Verbose CTest retains the last [folded phase] checkpoint before a kernel crash.
    & $ctestExe --test-dir $directory -C Release -R $pattern -V --output-on-failure --output-junit $junit --parallel 1 2>&1 | Tee-Object -FilePath $log
    $exitCode=$LASTEXITCODE
    Write-Host "Detailed log: $log"
    Write-Host "JUnit: $junit"
    if($exitCode -ne 0){throw "One or more focused native cases still fail (CTest exit $exitCode). Do not approve release."}
    Write-Host 'All four previously failing cases passed this focused run. Run the entire release suite next.' -ForegroundColor Green
} finally {Pop-Location}
