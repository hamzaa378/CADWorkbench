[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceRoot = [IO.Path]::GetFullPath((Split-Path -Parent $scriptRoot))
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $sourceRoot '..\outputs'
}
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
function Get-SourceRelativePath([string]$path) {
    $prefix = $sourceRoot + [IO.Path]::DirectorySeparatorChar
    $full = [IO.Path]::GetFullPath($path)
    if (-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Source file is outside the package root: $full"
    }
    return $full.Substring($prefix.Length)
}
$projectText = Get-Content -LiteralPath (Join-Path $sourceRoot 'CMakeLists.txt') -Raw
if ($projectText -match 'project\(CADWorkbench\s+VERSION\s+(\d+)\.(\d+)\.(\d+)') {
    $archiveVersion = 'v{0}_{1}' -f $Matches[1], $Matches[2]
} else {
    throw 'Could not read the CADWorkbench project version from CMakeLists.txt.'
}
if ($outputRoot.StartsWith($sourceRoot + [IO.Path]::DirectorySeparatorChar,
                           [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Put the source ZIP outside the source tree so it cannot be included in itself.'
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$zipPath = Join-Path $outputRoot "CADWorkbench_${archiveVersion}_Current_Source_$stamp.zip"
$workingZipPath = "$zipPath.partial"
$rootName = "CADWorkbench_${archiveVersion}_Source"

$sourceFiles = @(
    Get-ChildItem -LiteralPath $sourceRoot -File -Force
    foreach ($folder in @('src','tests','docs','scripts')) {
        $directory = Join-Path $sourceRoot $folder
        if (Test-Path -LiteralPath $directory -PathType Container) {
            Get-ChildItem -LiteralPath $directory -File -Recurse -Force
        }
    }
) | Where-Object {
    $relative = Get-SourceRelativePath $_.FullName
    $components = $relative -split '[\\/]'
    -not ($components | Where-Object { $_ -in @('.git','__pycache__','build','vcpkg_installed') }) -and
    $_.Extension -notin @('.pyc','.obj','.lib','.pdb','.ilk','.exe','.zip','.7z') -and
    $_.Name -notmatch '\.log$' -and
    $_.Name -ne 'SOURCE_MANIFEST.sha256'
} | Sort-Object FullName -Unique

Add-Type -AssemblyName System.IO.Compression
$expectedHashes = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
$stream = [IO.File]::Open($workingZipPath,[IO.FileMode]::CreateNew)
try {
    $archive = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
    try {
        $manifestLines = [Collections.Generic.List[string]]::new()
        foreach ($file in $sourceFiles) {
            $relative = (Get-SourceRelativePath $file.FullName).Replace('\','/')
            $entry = $archive.CreateEntry("$rootName/$relative",[IO.Compression.CompressionLevel]::Optimal)
            $entryStream = $entry.Open()
            try {
                $fileStream = [IO.File]::OpenRead($file.FullName)
                try { $fileStream.CopyTo($entryStream) } finally { $fileStream.Dispose() }
            } finally { $entryStream.Dispose() }
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $expectedHashes.Add("$rootName/$relative", $hash)
            $manifestLines.Add("$hash  $relative")
        }
        $manifest = $archive.CreateEntry("$rootName/SOURCE_MANIFEST.sha256",[IO.Compression.CompressionLevel]::Optimal)
        $writer = [IO.StreamWriter]::new($manifest.Open(),[Text.UTF8Encoding]::new($false))
        try { foreach ($line in $manifestLines) { $writer.WriteLine($line) } }
        finally { $writer.Dispose() }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }

$zipReadStream = [IO.File]::OpenRead($workingZipPath)
$reader = [IO.Compression.ZipArchive]::new($zipReadStream,[IO.Compression.ZipArchiveMode]::Read)
try {
    $verified = 0
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        foreach ($entry in $reader.Entries) {
            if ($entry.FullName -eq "$rootName/SOURCE_MANIFEST.sha256") { continue }
            if (-not $expectedHashes.ContainsKey($entry.FullName)) {
                throw "Unexpected file in source ZIP: $($entry.FullName)"
            }
            $entryStream = $entry.Open()
            try { $actual = [BitConverter]::ToString($sha.ComputeHash($entryStream)).Replace('-','').ToLowerInvariant() }
            finally { $entryStream.Dispose() }
            if ($actual -ne $expectedHashes[$entry.FullName]) {
                throw "Source ZIP integrity check failed: $($entry.FullName)"
            }
            $verified++
        }
    } finally { $sha.Dispose() }
    if ($verified -ne $sourceFiles.Count) { throw 'Source ZIP is missing one or more files.' }
    $manifestEntry = $reader.GetEntry("$rootName/SOURCE_MANIFEST.sha256")
    if (-not $manifestEntry) { throw 'Source ZIP is missing its hash manifest.' }
    $manifestReader = [IO.StreamReader]::new($manifestEntry.Open(),[Text.UTF8Encoding]::new($false))
    try { $actualManifest = $manifestReader.ReadToEnd() }
    finally { $manifestReader.Dispose() }
    $expectedManifest = ($manifestLines -join [Environment]::NewLine) + [Environment]::NewLine
    if ($actualManifest -ne $expectedManifest) { throw 'Source ZIP hash manifest does not match its source files.' }
} finally { $reader.Dispose(); $zipReadStream.Dispose() }
Move-Item -LiteralPath $workingZipPath -Destination $zipPath

Write-Host "Source ZIP: $zipPath"
Write-Host "Source files verified: $verified"
Write-Host "Archive SHA-256: $((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant())"
