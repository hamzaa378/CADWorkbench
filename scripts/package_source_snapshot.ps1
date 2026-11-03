[CmdletBinding()]
param(
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) '..\outputs')
)

$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if ($outputRoot.StartsWith($sourceRoot + [IO.Path]::DirectorySeparatorChar,
                           [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Put the source ZIP outside the source tree so it cannot be included in itself.'
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$zipPath = Join-Path $outputRoot "CADWorkbench_v1_55_Current_Source_$stamp.zip"
$rootName = 'CADWorkbench_v1_55_Source'

$sourceFiles = @(
    Get-ChildItem -LiteralPath $sourceRoot -File -Force
    foreach ($folder in @('src','tests','docs','scripts')) {
        $directory = Join-Path $sourceRoot $folder
        if (Test-Path -LiteralPath $directory -PathType Container) {
            Get-ChildItem -LiteralPath $directory -File -Recurse -Force
        }
    }
) | Where-Object {
    $relative = [IO.Path]::GetRelativePath($sourceRoot,$_.FullName)
    $components = $relative -split '[\\/]'
    -not ($components | Where-Object { $_ -in @('.git','__pycache__','build','vcpkg_installed') }) -and
    $_.Extension -notin @('.pyc','.obj','.lib','.pdb','.ilk','.exe','.zip','.7z') -and
    $_.Name -notmatch '\.log$'
} | Sort-Object FullName -Unique

Add-Type -AssemblyName System.IO.Compression
$stream = [IO.File]::Open($zipPath,[IO.FileMode]::CreateNew)
try {
    $archive = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
    try {
        $manifestLines = [Collections.Generic.List[string]]::new()
        foreach ($file in $sourceFiles) {
            $relative = [IO.Path]::GetRelativePath($sourceRoot,$file.FullName).Replace('\','/')
            $entry = $archive.CreateEntry("$rootName/$relative",[IO.Compression.CompressionLevel]::Optimal)
            $entryStream = $entry.Open()
            try {
                $fileStream = [IO.File]::OpenRead($file.FullName)
                try { $fileStream.CopyTo($entryStream) } finally { $fileStream.Dispose() }
            } finally { $entryStream.Dispose() }
            $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $manifestLines.Add("$hash  $relative")
        }
        $manifest = $archive.CreateEntry("$rootName/SOURCE_MANIFEST.sha256",[IO.Compression.CompressionLevel]::Optimal)
        $writer = [IO.StreamWriter]::new($manifest.Open(),[Text.UTF8Encoding]::new($false))
        try { foreach ($line in $manifestLines) { $writer.WriteLine($line) } }
        finally { $writer.Dispose() }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }

Write-Host "Source ZIP: $zipPath"
Write-Host "Source files: $($sourceFiles.Count)"
Write-Host "Archive SHA-256: $((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant())"
