[CmdletBinding()]
param(
    [string]$VcpkgRoot='C:\vcpkg',
    [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$env:VCPKG_ROOT=$VcpkgRoot
Push-Location $root
try {
    if(-not $SkipBuild){
        Write-Host 'Compiling the unmodified application and all native test targets in Windows Release...' -ForegroundColor Cyan
        & (Join-Path $root 'scripts\build_windows.ps1') -SkipTests
        if($LASTEXITCODE -ne 0){throw "Native Windows compilation failed (exit $LASTEXITCODE)."}
    }
    $ctestCommand=Get-Command ctest -ErrorAction SilentlyContinue
    $ctestExe=if($ctestCommand){$ctestCommand.Source}else{$null}
    if(-not $ctestExe){
        $vsRoot=Join-Path $env:ProgramFiles 'Microsoft Visual Studio'
        $ctestExe=Get-ChildItem -LiteralPath $vsRoot -Filter ctest.exe -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {$_.FullName -match '\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\ctest\.exe$'} |
            Select-Object -First 1 -ExpandProperty FullName
    }
    if(-not $ctestExe){throw 'CTest not found. Install CMake via Visual Studio.'}
    $directory=Join-Path $root 'build'
    $log=Join-Path $directory 'v170_folded_edit_diagnostics.log'
    $junit=Join-Path $directory 'v170_folded_edit_diagnostics.xml'
    if(Test-Path -LiteralPath $junit){Remove-Item -LiteralPath $junit -Force}
    Write-Host 'Running the full folded sheet-metal native test without suppressing failures.' -ForegroundColor Cyan
    Write-Host 'Find the last [folded edit] marker to identify the crashing operation.'
    & $ctestExe --test-dir $directory -C Release -R '^cadcore_folded_sheet_metal$' -V --output-on-failure --output-junit $junit --parallel 1 2>&1 |
        Tee-Object -FilePath $log
    $ctestExit=$LASTEXITCODE
    Write-Host "Log: $log"
    Write-Host "JUnit: $junit"
    if($ctestExit -ne 0){
        throw "Folded sheet-metal test still fails (CTest exit $ctestExit). Share the log before attempting a kernel fix."
    }
    Write-Host 'Folded sheet-metal test passed. Next run RUN_V170_FINAL_PRETEST.ps1 for all 130 native tests.' -ForegroundColor Green
} finally {
    Pop-Location
}
