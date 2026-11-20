[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ExistingRoot)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
$target=(Resolve-Path -LiteralPath $ExistingRoot -ErrorAction Stop).Path
if ($source -eq $target) { throw 'Source and target are identical. Run the UI workflow script directly.' }
$exe=Join-Path $target 'build\Release\CADWorkbench.exe'
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Verified CADWorkbench executable missing: $exe" }
$manifest=Join-Path $target 'SOURCE_MANIFEST_SHA256.txt'
if (!(Test-Path -LiteralPath $manifest -PathType Leaf)) { throw 'Original Sprint 16 source manifest missing' }

# Verify immutable Windows native baseline before changing anything.
$verified=0
foreach ($line in (Get-Content -LiteralPath $manifest -Encoding UTF8)) {
  if (-not $line.Trim()) { continue }
  if ($line -notmatch '^([0-9a-f]{64})  (.+)$') { throw "Invalid Sprint 16 manifest entry: $line" }
  $expected=$Matches[1]
  $relative=$Matches[2].Replace('/','\')
  if ([IO.Path]::IsPathRooted($relative) -or $relative.Split('\') -contains '..') {
    throw "Unsafe source manifest path: $relative"
  }
  $path=Join-Path $target $relative
  if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing original Sprint 16 file: $relative" }
  $actual=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($expected -ne $actual) { throw "Sprint 16 base source modified: $relative" }
  $verified++
}
if ($verified -lt 690) { throw "Incomplete Sprint 16 source manifest: $verified" }

# Only previously installed Sprint 17 overlay entries may be replaced.
# Explicit old checksums prevent a silent overwrite of user edits.
$old=@{
 'automation\windows_ui_backend.py' = '76e17a5e09fecbc207464aaea4f333aa2c9d4667a1b1ba861a4b6ed490af3f11'
 'RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1' = '81490699ea6396465469146632567f3f1827f62697568048f4b764227776c62b'
 'SPRINT17_UI_AUTOMATION_GUIDE.md' = 'fe43b993504f947322c5ec08a7bf457816fc43b1e1a6e136d04730e731e3e947'
}
$files=@(
 'automation\windows_ui_backend.py',
 'automation\qt_dialog_discovery.py',
 'RUN_SPRINT17_UI_WORKFLOWS_WINDOWS.ps1',
 'SPRINT17_UI_AUTOMATION_GUIDE.md',
 'SPRINT17_QT_DIALOG_FIX_20261118.md',
 'tests\test_sprint17_qt_dialog_fix.py'
)
foreach ($relative in $files) {
  $origin=Join-Path $source $relative
  $destination=Join-Path $target $relative
  if (!(Test-Path -LiteralPath $origin -PathType Leaf)) { throw "Missing repair source file: $relative" }
  if (Test-Path -LiteralPath $destination -PathType Leaf) {
    $current=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
    $fixed=(Get-FileHash -LiteralPath $origin -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($current -ne $fixed -and (!$old.ContainsKey($relative) -or $current -ne $old[$relative])) {
      throw "Refusing to overwrite modified UI overlay file: $relative"
    }
  }
}
$backup=Join-Path $target ('build\sprint17_qt_modal_fix_backup_'+(Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Force -Path $backup | Out-Null
$copied=0
foreach ($relative in $files) {
  $origin=Join-Path $source $relative
  $destination=Join-Path $target $relative
  $fixed=(Get-FileHash -LiteralPath $origin -Algorithm SHA256).Hash.ToLowerInvariant()
  $existingHash = ''
  if (Test-Path -LiteralPath $destination -PathType Leaf) {
    $existingHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
  }
  if ($existingHash -eq $fixed) { continue }
  if (Test-Path -LiteralPath $destination -PathType Leaf) {
    $backupDestination=Join-Path $backup $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $backupDestination) | Out-Null
    Copy-Item -LiteralPath $destination -Destination $backupDestination -Force
  }
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
  Copy-Item -LiteralPath $origin -Destination $destination -Force
  $copied++
  if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $fixed) {
    throw "Copy hash verification failed: $relative"
  }
}
Write-Host "PASS: $verified original Sprint 16 source hashes remain verified"
Write-Host "PASS: Sprint 17 Qt dialog overlay repaired ($copied updated files)" -ForegroundColor Green
Write-Host "PASS: Verified Sprint 16 CADWorkbench.exe and build directory retained"
Write-Host "BACKUP: $backup"
Write-Host "ROOT: $target"
