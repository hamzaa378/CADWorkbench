[CmdletBinding()]
param([switch]$Run, [switch]$Detach, [int]$Parallel = 4)
$ErrorActionPreference='Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root 'build'
if (-not (Test-Path -LiteralPath (Join-Path $build 'CTestTestfile.cmake'))) {
  throw 'Existing built CTest tree missing. Build CADWorkbench on Windows first.'
}
if ($Detach -and -not $Run) {
  $exe = (Get-Command powershell.exe -ErrorAction Stop).Source
  # Launch an independent process. Never send keyboard or mouse events.
  $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $MyInvocation.MyCommand.Path + '" -Run -Parallel ' + $Parallel
  $process = Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
  Write-Host "NONINTERACTIVE_REGRESSION_PROCESS_ID: $($process.Id)"
  Write-Host "Check later under: $(Join-Path $root 'build\background_regressions')"
  exit 0
}
$report = Join-Path $root ("build\background_regressions\"+(Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
New-Item -ItemType Directory -Force -Path $report | Out-Null
$env:PYTHONUTF8='1'
$env:PYTHONIOENCODING='utf-8'
$python = Join-Path $root '.venv-cadquery\Scripts\python.exe'
if (-not (Test-Path $python)) {
  $python = (Get-Command python -ErrorAction Stop).Source
}
$summary = [ordered]@{ mode='noninteractive-engine-regression'; acceptance='NOT_RUN'; native='NOT_RUN'; python='NOT_RUN'; started=(Get-Date).ToString('o') }
try {
  & ctest --test-dir $build -C Release --parallel $Parallel --output-on-failure --output-junit (Join-Path $report 'ctest.xml') *> (Join-Path $report 'ctest.log')
  $native = $LASTEXITCODE
  $summary.native = if($native -eq 0){'PASS'}else{"FAIL($native)"}
  Push-Location $root
  try {
    & $python -m unittest discover -s tests -p '*.py' -q *> (Join-Path $report 'python.log')
    $pythonRc = $LASTEXITCODE
  } finally { Pop-Location }
  $summary.python = if($pythonRc -eq 0){'PASS'}else{"FAIL($pythonRc)"}
} catch {
  $summary.error = $_.Exception.Message
  $summary.native = if($summary.native -eq 'NOT_RUN'){'BLOCKED'}else{$summary.native}
} finally {
  $summary.finished = (Get-Date).ToString('o')
  $summary | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $report 'summary.json') -Encoding UTF8
}
Write-Host "BACKGROUND_REPORT: $report"
Write-Host "NATIVE_STATUS: $($summary.native)"
Write-Host "PYTHON_STATUS: $($summary.python)"
if ($summary.native -eq 'PASS' -and $summary.python -eq 'PASS') { exit 0 }
exit 1
