# Optional fast path: apply this corrected Sprint 16 source over the user's
# *existing Sprint 16* Windows build tree without deleting build/ or vcpkg.
# Does not rebuild, run CTest, or change another Sprint.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ExistingRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$source = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$destination = (Resolve-Path -LiteralPath $ExistingRoot).Path
if ($source -eq $destination) { throw 'Supply the existing Sprint 16 build root, not this extracted repair source.' }
if (!(Test-Path -LiteralPath (Join-Path $destination 'build\CMakeCache.txt') -PathType Leaf)) {
    throw 'ExistingRoot must point to the existing Sprint 16 project with its built CMake cache.'
}
if (!(Test-Path -LiteralPath (Join-Path $destination 'RUN_SPRINT16_WINDOWS_TARGETED_BUILD.ps1') -PathType Leaf)) {
    throw 'ExistingRoot is not a Sprint 16 source tree.'
}

function Read-SourceManifest([string]$Path) {
    $map = @{}
    foreach ($line in (Get-Content -LiteralPath $Path -Encoding UTF8)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line -notmatch '^([a-f0-9]{64})  (.+)$') { throw "Invalid source manifest line in $Path" }
        $hash = $Matches[1]
        $relative = $Matches[2]
        if ($relative -match '(^/|^[A-Za-z]:|\\|(^|/)\.\.(/|$))') {
            throw "Unsafe manifest source path: $relative"
        }
        if ($map.ContainsKey($relative)) { throw "Duplicate source path: $relative" }
        $map[$relative] = $hash
    }
    return $map
}
function Confirm-SourceHashes([string]$Root, $Manifest) {
    foreach ($relative in $Manifest.Keys) {
        $file = Join-Path $Root $relative
        if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing source: $file" }
        $actual = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $Manifest[$relative]) { throw "Modified source, refusing overwrite: $file" }
    }
}
$srcMf = Join-Path $source 'SOURCE_MANIFEST_SHA256.txt'
$dstMf = Join-Path $destination 'SOURCE_MANIFEST_SHA256.txt'
$srcManifest = Read-SourceManifest $srcMf
$dstManifest = Read-SourceManifest $dstMf
if ((Get-Content -LiteralPath (Join-Path $destination 'SHA256SUMS.txt') -Raw -Encoding UTF8) -ne
    (Get-Content -LiteralPath $dstMf -Raw -Encoding UTF8)) {
    throw 'The existing Sprint 16 source manifests disagree.'
}
Confirm-SourceHashes $source $srcManifest
Confirm-SourceHashes $destination $dstManifest
foreach ($relative in $dstManifest.Keys) {
    if (!$srcManifest.ContainsKey($relative)) { throw "Unexpected source deletion in the repair: $relative" }
}
$changed = @($srcManifest.Keys | Where-Object {
    !$dstManifest.ContainsKey($_) -or $dstManifest[$_] -ne $srcManifest[$_]
} | Sort-Object)
$backup = Join-Path $destination ("build\sprint16_target_fix_backup_" + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
$oldPaths = @()
$newPaths = @()
try {
    foreach ($relative in (@($changed) + @('SHA256SUMS.txt','SOURCE_MANIFEST_SHA256.txt'))) {
        $targetFile = Join-Path $destination $relative
        $sourceFile = Join-Path $source $relative
        if (Test-Path -LiteralPath $targetFile -PathType Leaf) {
            $backupFile = Join-Path $backup $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $backupFile) -Force | Out-Null
            Copy-Item -LiteralPath $targetFile -Destination $backupFile -Force
            $oldPaths += $relative
        } else {
            $newPaths += $relative
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $targetFile) -Force | Out-Null
        Copy-Item -LiteralPath $sourceFile -Destination $targetFile -Force
    }
    $py = Get-Command py -ErrorAction SilentlyContinue
    Push-Location -LiteralPath $destination
    try {
        if ($py) { & $py.Source -3 .\tools\preflight_major_build.py --root . }
        else { & (Get-Command python -ErrorAction Stop).Source .\tools\preflight_major_build.py --root . }
        if ($LASTEXITCODE -ne 0) { throw 'Preflight failed after applying Sprint 16 repair.' }
    } finally { Pop-Location }
    Write-Host "PASS: Sprint 16 source repair applied in-place ($($changed.Count) source files changed)." -ForegroundColor Green
    Write-Host "Existing MSVC build and vcpkg cache retained: $destination\build" -ForegroundColor Green
    Write-Host "Backup of replaced files: $backup"
} catch {
    foreach ($relative in $oldPaths) {
        Copy-Item -LiteralPath (Join-Path $backup $relative) -Destination (Join-Path $destination $relative) -Force
    }
    foreach ($relative in $newPaths) {
        Remove-Item -LiteralPath (Join-Path $destination $relative) -Force -ErrorAction SilentlyContinue
    }
    throw
}
