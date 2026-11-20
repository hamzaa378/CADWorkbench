[CmdletBinding()]
param(
  [string]$ExePath = "",
  [string[]]$Only = @(),
  [switch]$InIsolatedVM,
  [switch]$AllowActiveDesktop,
  [string]$Python = ""
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $ExePath) { $ExePath = Join-Path $root 'build\Release\CADWorkbench.exe' }
if (-not (Test-Path -LiteralPath $ExePath)) { throw "CADWorkbench.exe not found: $ExePath" }
if (-not ($InIsolatedVM -xor $AllowActiveDesktop)) {
  throw 'Select exactly one: -InIsolatedVM (inside a VM guest) or -AllowActiveDesktop (takes control of this desktop)'
}
if (-not $Python) {
  $localPy = Join-Path $root '.venv-ui\Scripts\python.exe'
  if (Test-Path $localPy) { $Python = $localPy }
  else {
    $cmd = Get-Command py -ErrorAction SilentlyContinue
    if ($cmd) { $Python = $cmd.Source } else { $Python = (Get-Command python -ErrorAction Stop).Source }
  }
}
# Fail before launching CAD if Windows UI modules or screenshot support are absent.
# The runner requires an interactive desktop; screenshot support is needed to
# diagnose inaccessible Qt widgets without guessing at CAD geometry results.
$preflight = @('import sys,struct,win32gui,pywinauto; from PIL import ImageGrab; assert struct.calcsize("P")==8; print("PASS: Windows UIA, Win32 and Pillow available")')
if ([IO.Path]::GetFileNameWithoutExtension($Python) -eq 'py') {
  & $Python -3 -c $preflight[0]
} else {
  & $Python -c $preflight[0]
}
if ($LASTEXITCODE -ne 0) {
  throw 'Sprint 17 UI dependencies missing: use standard 64-bit Python and install pywinauto pywin32 comtypes pillow into the chosen environment.'
}
$report = Join-Path $env:USERPROFILE ("Downloads\CADWorkbench_UI_Evidence\" + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
New-Item -Path (Split-Path -Parent $report) -ItemType Directory -Force | Out-Null
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$argsList = @()
if ([IO.Path]::GetFileNameWithoutExtension($Python) -eq 'py') { $argsList += '-3' }
$argsList += @((Join-Path $root 'automation\runner.py'), '--exe', $ExePath, '--report-dir', $report)
if ($InIsolatedVM) { $argsList += '--in-isolated-vm' } else { $argsList += '--allow-active-desktop' }
if ($Only.Count) { $argsList += '--only'; $argsList += $Only }
Write-Host 'CADWorkbench UI session:' $(if($InIsolatedVM){'ISOLATED VM'}else{'ACTIVE DESKTOP'})
Write-Host 'This mode controls the desktop of the current Windows session.'
Write-Host "Evidence destination: $report"
& $Python @argsList
$rc = $LASTEXITCODE
Write-Host "UI_RUNNER_EXIT: $rc"
Write-Host "REPORT_JSON: $(Join-Path $report 'report.json')"
Write-Host "REPORT_HTML: $(Join-Path $report 'report.html')"
exit $rc
