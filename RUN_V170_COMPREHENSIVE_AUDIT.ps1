[CmdletBinding()]
param(
    [string]$VcpkgRoot = 'C:\vcpkg',
    [switch]$SkipBuild,
    [switch]$SetupPythonEnv,
    [int]$Parallel = 4
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if($Parallel -lt 1 -or $Parallel -gt 16){throw 'Parallel must be between 1 and 16.'}
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$build=Join-Path $root 'build'
$qa=Join-Path $build 'v170_comprehensive_audit'
$env:VCPKG_ROOT=$VcpkgRoot
$env:CAD_V170_FOLDED_TRACE='1'
New-Item -ItemType Directory -Force -Path $qa | Out-Null
Push-Location $root
try {
    Write-Host 'CADWorkbench v1.70 — FULL NATIVE + ACCEPTANCE EVIDENCE AUDIT' -ForegroundColor Cyan
    Write-Host 'Every registered CTest remains enabled. Failing tests will be reported, not hidden.'
    $manifest=Join-Path $root 'SHA256SUMS.txt'
    if(-not(Test-Path -LiteralPath $manifest -PathType Leaf)){throw 'SHA256SUMS.txt is missing.'}
    $checked=0
    foreach($line in (Get-Content -LiteralPath $manifest)) {
        if([string]::IsNullOrWhiteSpace($line)){continue}
        if($line -notmatch '^([a-fA-F0-9]{64})  (.+)$'){throw "Malformed hash entry: $line"}
        $file=Join-Path $root $Matches[2]
        if(-not(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing file in source manifest: $file"}
        $actual=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        if(-not $actual.Equals($Matches[1],[System.StringComparison]::OrdinalIgnoreCase)){
            throw "Source-integrity mismatch: $file"
        }
        $checked++
    }
    Write-Host "Verified $checked source hashes." -ForegroundColor Green
    $record=Join-Path $build 'V170_ACCEPTANCE_RESULTS.csv'
    if(-not(Test-Path -LiteralPath $record)){
        Copy-Item (Join-Path $root 'docs/V170_ACCEPTANCE_RESULTS.csv') $record
        Write-Host 'Created acceptance workbook CSV; all 60 cases are initially NOT_RUN.' -ForegroundColor Yellow
    }
    $buildOk=$true
    if(-not $SkipBuild){
        Write-Host 'Building the application and all native test executables...' -ForegroundColor Cyan
        # Windows PowerShell 5.1 converts native stderr to ErrorRecord when
        # redirected with 2>&1, sometimes turning a harmless CMake Warning
        # into a terminating exception.  Use a host transcript instead of
        # redirecting the native build's stderr through a PowerShell pipeline.
        # The build helper itself verifies BOTH CMake native exit codes.
        $transcriptStarted=$false
        try {
            Start-Transcript -Path (Join-Path $qa 'build_output.log') -Force | Out-Null
            $transcriptStarted=$true
            & (Join-Path $root 'scripts/build_windows.ps1') -SkipTests
            $buildExit=$LASTEXITCODE
            if($buildExit -ne 0){
                $buildOk=$false
                Write-Host "Windows build script returned exit code $buildExit." -ForegroundColor Red
            }
            if(-not(Test-Path -LiteralPath (Join-Path $build 'Release/CADWorkbench.exe') -PathType Leaf) -or
               -not(Test-Path -LiteralPath (Join-Path $build 'Release/cadcore_tests.exe') -PathType Leaf)){
                $buildOk=$false
                Write-Host 'Required Windows Release executables are missing.' -ForegroundColor Red
            }
        } catch {
            $buildOk=$false
            Write-Host ("Native build error: "+$_.Exception.Message) -ForegroundColor Red
        } finally {
            if($transcriptStarted){Stop-Transcript | Out-Null}
        }
    }
    $ctestCommand=Get-Command ctest -ErrorAction SilentlyContinue
    $ctestExe=if($ctestCommand){$ctestCommand.Source}else{$null}
    if(-not $ctestExe){
        $vsRoot=Join-Path $env:ProgramFiles 'Microsoft Visual Studio'
        $ctestExe=Get-ChildItem -LiteralPath $vsRoot -Filter 'ctest.exe' -Recurse -File -ErrorAction SilentlyContinue |
           Where-Object {$_.FullName -match '\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\ctest\.exe$'} |
           Select-Object -First 1 -ExpandProperty FullName
    }
    $report=Join-Path $build 'cadworkbench_release_ctest.xml'
    if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report -Force}
    $ctestExit=1
    if($buildOk -and $ctestExe){
        Write-Host 'Running ALL 130 native tests with folded-commit diagnostics enabled...' -ForegroundColor Cyan
        # Capture CTest output even when it returns nonzero; retain the JUnit and failed-case trace.
        $oldPreference=$ErrorActionPreference
        $ErrorActionPreference='Continue'
        try {
            & $ctestExe --test-dir $build -C Release --parallel $Parallel `
                --output-on-failure --output-junit $report 2>&1 |
                Tee-Object -FilePath (Join-Path $qa 'ctest_output.log')
            $ctestExit=$LASTEXITCODE
        } finally {$ErrorActionPreference=$oldPreference}
    } else {
        Write-Host 'Native tests NOT RUN: build did not succeed or CTest was not found.' -ForegroundColor Red
    }
    $pythonCommand=Get-Command python -ErrorAction SilentlyContinue
    if(-not $pythonCommand){throw 'Python 3 is required for the acceptance inventory/audit.'}
    $pythonExe=$pythonCommand.Source
    $referencePython=Join-Path $root '.venv-cadquery/Scripts/python.exe'
    # Opt in to an isolated environment; never install pip packages globally.
    if($SetupPythonEnv){
        if(-not(Test-Path -LiteralPath $referencePython -PathType Leaf)){
            Write-Host 'Creating a project-local Python geometry-reference venv...' -ForegroundColor Cyan
            & $pythonExe -m venv (Join-Path $root '.venv-cadquery')
            if($LASTEXITCODE -ne 0){throw 'Could not create project Python venv.'}
        }
        & $referencePython -m pip install -r (Join-Path $root 'requirements-kernel-reference.txt')
        if($LASTEXITCODE -ne 0){throw 'Could not install the optional Python geometry reference requirements.'}
    }
    if(Test-Path -LiteralPath $referencePython -PathType Leaf){$pythonExe=$referencePython}
    $env:PYTHONUTF8='1' # Source contracts must decode UTF-8 on Windows cp1252 hosts.
    Write-Host ('Python selected: '+$pythonExe) -ForegroundColor Cyan
    $oldPreference=$ErrorActionPreference
    $ErrorActionPreference='Continue'
    $depsOk=$false
    try {
        & $pythonExe -c 'import OCP; import cadquery; import pygments' 2>&1 | Out-Null
        $depsOk=($LASTEXITCODE -eq 0)
    } finally {$ErrorActionPreference=$oldPreference}
    $pythonExit=1
    if($depsOk){
        Write-Host 'Running the complete Python/source regression inventory...' -ForegroundColor Cyan
        $oldPreference=$ErrorActionPreference
        $ErrorActionPreference='Continue'
        try {
            & $pythonExe -m unittest discover -s tests -p '*.py' -q 2>&1 |
                Tee-Object -FilePath (Join-Path $qa 'python_source_tests.log')
            $pythonExit=$LASTEXITCODE
        } finally {$ErrorActionPreference=$oldPreference}
    } else {
        $message='Python tests NOT RUN: cadquery/OCP/Pygments dependency missing. Rerun with -SetupPythonEnv (creates or repairs an isolated project venv).'
        Write-Host $message -ForegroundColor Yellow
        Set-Content -LiteralPath (Join-Path $qa 'python_source_tests.log') -Value $message -Encoding UTF8
    }
    Write-Host ('Python/source test exit code: '+$pythonExit)
    Write-Host 'Generating 130-test and 60-scenario matrices...' -ForegroundColor Cyan
    & $pythonExe (Join-Path $root 'tools/v170_full_test_audit.py') `
        --root $root --junit $report --acceptance $record --output-dir $qa
    $auditExit=$LASTEXITCODE
    Write-Host 'Reports:' -ForegroundColor Cyan
    Write-Host (Join-Path $qa 'v170_remaining_work.md')
    Write-Host (Join-Path $qa 'v170_native_130_matrix.csv')
    Write-Host (Join-Path $qa 'v170_acceptance_60_matrix.csv')
    Write-Host (Join-Path $qa 'v170_full_audit.json')
    Write-Host 'Record manual scenario evidence in build/V170_ACCEPTANCE_RESULTS.csv and rerun the audit.'
    # A missing acceptance sign-off is not evidence of a failing native test.
    # Keep release blocked, but report the two outcomes separately.
    if(-not $buildOk -or $ctestExit -ne 0 -or $pythonExit -ne 0){
        throw 'Automated verification FAILED. Inspect build, CTest and Python logs; release remains blocked.'
    }
    $auditJson=Join-Path $qa 'v170_full_audit.json'
    if(-not(Test-Path -LiteralPath $auditJson -PathType Leaf)){
        throw 'Audit report missing; cannot assess the release gate.'
    }
    $auditData=Get-Content -LiteralPath $auditJson -Raw -Encoding UTF8 | ConvertFrom-Json
    if(-not $auditData.native_complete -or @($auditData.problems).Count -gt 0){
        throw 'Audit inventory or evidence validation FAILED. Inspect v170_full_audit.json.'
    }
    if(-not $auditData.release_gate_evidence_satisfied){
        Write-Host 'AUTOMATED VERIFICATION PASSED: 130 native CTests and Python/source suite.' -ForegroundColor Green
        Write-Host 'MANUAL ACCEPTANCE PENDING: release remains BLOCKED.' -ForegroundColor Yellow
        exit 3
    }
    if($auditExit -ne 0){throw 'Release audit returned nonzero despite a satisfied gate.'}
    Write-Host 'Automated and recorded acceptance gates passed; independent review is still required.' -ForegroundColor Green
} finally {Pop-Location}
