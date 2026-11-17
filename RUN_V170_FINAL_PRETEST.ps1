[CmdletBinding()]
param([string]$VcpkgRoot='C:\vcpkg')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    $report=Join-Path $root 'build\cadworkbench_release_ctest.xml'
    if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report -Force}
    $nativePass=$false
    try {
        & (Join-Path $root 'RUN_WINDOWS_TESTS.ps1') -VcpkgRoot $VcpkgRoot
        $nativePass=$true
    } catch {
        Write-Host ('Native build or CTest did not pass: '+$_.Exception.Message) -ForegroundColor Yellow
    }
    $cases=@()
    $failed=@()
    $skipped=@()
    if(Test-Path -LiteralPath $report){
        [xml]$document=Get-Content -LiteralPath $report -Raw
        $cases=@($document.SelectNodes('//testcase'))
        $failed=@($cases|Where-Object { $_.SelectSingleNode('failure') -or $_.SelectSingleNode('error') })
        $skipped=@($cases|Where-Object { $_.SelectSingleNode('skipped') })
        $passed=$cases.Count-$failed.Count-$skipped.Count
        Write-Host ('Native tests: {0} passed, {1} failed, {2} skipped, {3} recorded.' -f $passed,$failed.Count,$skipped.Count,$cases.Count)
        foreach($item in $failed){Write-Host ('  FAILED: '+$item.GetAttribute('name')) -ForegroundColor Red}
        if($cases.Count -ne 130){Write-Host ('Expected 130 native cases; inventory mismatch or incomplete run: '+$cases.Count) -ForegroundColor Yellow}
        Write-Host ('JUnit: '+$report)
    } else {
        Write-Host 'No fresh JUnit report was generated; examine the compiler/build log.' -ForegroundColor Red
    }
    Write-Host 'Previously observed native failures (all still enabled): cadcore_v170_articulated_scene_transaction, cadcore_v170_assembly_mate_document, cadcore_parametric_foundation, cadcore_folded_sheet_metal.'
    Write-Host 'Do not approve a final release until all cases pass and the separate 60-scenario acceptance evidence exists.'
    if(-not $nativePass -or $cases.Count -ne 130 -or $failed.Count -ne 0 -or $skipped.Count -ne 0){exit 1}
} finally {Pop-Location}
