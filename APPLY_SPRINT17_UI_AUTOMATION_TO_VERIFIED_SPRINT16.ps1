[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ExistingRoot)
$ErrorActionPreference='Stop'
$source=Split-Path -Parent $MyInvocation.MyCommand.Path
$target=(Resolve-Path -LiteralPath $ExistingRoot -ErrorAction Stop).Path
if ($source -eq $target) { throw 'Source and target are identical: use the runner directly.' }
$exe=Join-Path $target 'build\Release\CADWorkbench.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "Existing Windows Release exe missing: $exe" }
if (!(Test-Path -LiteralPath (Join-Path $target 'SOURCE_MANIFEST_SHA256.txt'))) {
  throw 'Target does not have a source manifest; refusing unverified overlay'
}
# Verify the existing Sprint 16 source before adding extra automation files.
# A mismatch indicates a different or locally modified baseline; fail closed.
$manifest = Join-Path $target 'SOURCE_MANIFEST_SHA256.txt'
$verifiedCount = 0
foreach ($line in (Get-Content -LiteralPath $manifest -Encoding UTF8)) {
  if (-not $line.Trim()) { continue }
  if ($line -notmatch '^([0-9a-f]{64})  (.+)$') { throw "Invalid source manifest entry" }
  $wanted = $Matches[1]
  $relativeName = $Matches[2]
  $safeName = $relativeName.Replace('/', '\')
  if ([IO.Path]::IsPathRooted($safeName) -or $safeName.Split('\') -contains '..') {
    throw "Unsafe source manifest path: $relativeName"
  }
  $sourceFile = Join-Path $target $safeName
  if (!(Test-Path -LiteralPath $sourceFile -PathType Leaf)) { throw "Missing existing baseline file: $relativeName" }
  if ((Get-FileHash -LiteralPath $sourceFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $wanted) {
    throw "Modified or unverified Sprint 16 source: $relativeName"
  }
  $verifiedCount++
}
if ($verifiedCount -lt 690) { throw "Incomplete Sprint 16 source manifest ($verifiedCount entries)" }
Write-Host "PASS: $verifiedCount existing Sprint 16 manifest hashes verified"
# Crucial: do not modify any existing C++ source, native build artifacts or Sprint 16 manifest.
$files=@(
 'automation\runner.py',
 'automation\ui_workflow_engine.py',
 'automation\windows_ui_backend.py',
 'automation\sprint17_ui_scenarios.json',
 'RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1',
 'RUN_SPRINT17_BACKGROUND_REGRESSION_WINDOWS.ps1',
 'SPRINT17_UI_AUTOMATION_GUIDE.md',
 'tests\test_sprint17_ui_automation.py'
)
foreach ($relative in $files) {
  $origin=Join-Path $source $relative
  if (!(Test-Path -LiteralPath $origin)) { throw "Missing overlay source $origin" }
  $destination=Join-Path $target $relative
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
  if (Test-Path -LiteralPath $destination) {
    $old=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    $new=(Get-FileHash -LiteralPath $origin -Algorithm SHA256).Hash
    if ($old -ne $new) { throw "Overlay file already exists with different contents; will not overwrite: $destination" }
  }
}
foreach ($relative in $files) {
  Copy-Item -LiteralPath (Join-Path $source $relative) -Destination (Join-Path $target $relative) -Force
  $old=(Get-FileHash -LiteralPath (Join-Path $source $relative) -Algorithm SHA256).Hash
  $new=(Get-FileHash -LiteralPath (Join-Path $target $relative) -Algorithm SHA256).Hash
  if ($old -ne $new) { throw "Copy verification failed for $relative" }
}
Write-Host 'PASS: Sprint 17 UI automation overlay installed and verified.' -ForegroundColor Green
Write-Host 'The verified Sprint 16 binary, native build folder, C++ source and manifests were left unchanged.'
Write-Host "TARGET: $target"
