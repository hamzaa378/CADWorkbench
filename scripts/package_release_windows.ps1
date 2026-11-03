[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) '..\build_v148_combined'),
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) '..\outputs\CADWorkbench_v1_55_Release')
)

$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$exe = Join-Path $BuildDirectory 'Release\CADWorkbench.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Release executable not found: $exe" }
if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}

$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot) {
    $candidate = Join-Path (Split-Path -Parent $sourceRoot) 'CADWorkbench_v1_9\vcpkg_installed\x64-windows'
    if (Test-Path -LiteralPath $candidate -PathType Container) { $vcpkgRoot = $candidate }
}
if (-not $vcpkgRoot) { throw "Set VCPKG_ROOT or provide the project's x64-windows vcpkg installed tree." }
$installed = if ((Split-Path -Leaf $vcpkgRoot) -eq 'x64-windows') { $vcpkgRoot } else { Join-Path $vcpkgRoot 'installed\x64-windows' }
$binDirectory = Join-Path $installed 'bin'
$windeployqt = Join-Path $installed 'tools\Qt6\bin\windeployqt.exe'
if (-not (Test-Path -LiteralPath $binDirectory -PathType Container) -or
    -not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    throw "Qt or vcpkg runtime files are missing under $installed"
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('CADWorkbench_v1_55_' + [guid]::NewGuid().ToString('N'))
$stageApp = Join-Path $tempRoot 'app'
$payload = Join-Path $tempRoot 'setup-payload'
New-Item -ItemType Directory -Path $stageApp,$payload -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $stageApp 'CADWorkbench.exe')

$oldPath = $env:PATH
try {
    $env:PATH = "$binDirectory;$oldPath"
    & $windeployqt --release --no-translations --compiler-runtime --dir $stageApp (Join-Path $stageApp 'CADWorkbench.exe')
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }
} finally { $env:PATH = $oldPath }

# Bundle the manifest's runtime DLL directory so OCCT and Qt dependencies are
# present on clean Windows machines even when their DLL search path is empty.
Get-ChildItem -LiteralPath $binDirectory -Filter '*.dll' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $stageApp $_.Name) -Force
}

$licenseDirectory = Join-Path $stageApp 'licenses'
New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null
$shareDirectory = Join-Path $installed 'share'
if (Test-Path -LiteralPath $shareDirectory -PathType Container) {
    Get-ChildItem -LiteralPath $shareDirectory -Directory | ForEach-Object {
        $copyright = Join-Path $_.FullName 'copyright'
        if (Test-Path -LiteralPath $copyright -PathType Leaf) {
            Copy-Item -LiteralPath $copyright -Destination (Join-Path $licenseDirectory ($_.Name + '.txt'))
        }
    }
}
@'
CADWorkbench v1.55.0

CADWorkbench is built with Qt 6 and Open CASCADE Technology. Third-party
license notices for packages in the bundled vcpkg runtime are in this folder.
'@ | Set-Content -LiteralPath (Join-Path $licenseDirectory 'README.txt') -Encoding utf8

$portableZip = Join-Path $OutputDirectory 'CADWorkbench_v1_55_Windows_Portable.zip'
if (Test-Path -LiteralPath $portableZip) { Remove-Item -LiteralPath $portableZip -Force }
Compress-Archive -Path (Join-Path $stageApp '*') -DestinationPath $portableZip -CompressionLevel Optimal

$installScript = @'
$ErrorActionPreference = 'Stop'
$sourceArchive = Join-Path $PSScriptRoot 'CADWorkbench_v1_55_Windows_Portable.zip'
if (-not (Test-Path -LiteralPath $sourceArchive -PathType Leaf)) { throw 'The CADWorkbench application payload is missing.' }
$installRoot = Join-Path $env:LOCALAPPDATA 'Programs\CADWorkbench'
if (-not (Test-Path -LiteralPath $installRoot -PathType Container)) { New-Item -ItemType Directory -Path $installRoot -Force | Out-Null }
Expand-Archive -LiteralPath $sourceArchive -DestinationPath $installRoot -Force
$app = Join-Path $installRoot 'CADWorkbench.exe'
if (-not (Test-Path -LiteralPath $app -PathType Leaf)) { throw 'CADWorkbench.exe was not installed correctly.' }
$shell = New-Object -ComObject WScript.Shell
$startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\CADWorkbench'
New-Item -ItemType Directory -Path $startMenu -Force | Out-Null
$shortcut = $shell.CreateShortcut((Join-Path $startMenu 'CADWorkbench.lnk'))
$shortcut.TargetPath = $app
$shortcut.WorkingDirectory = $installRoot
$shortcut.Save()
$desktopShortcut = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'CADWorkbench.lnk'))
$desktopShortcut.TargetPath = $app
$desktopShortcut.WorkingDirectory = $installRoot
$desktopShortcut.Save()
$uninstall = @(
    "`$ErrorActionPreference = 'Stop'",
    "`$installRoot = Join-Path `$env:LOCALAPPDATA 'Programs\CADWorkbench'",
    "`$startMenu = Join-Path `$env:APPDATA 'Microsoft\Windows\Start Menu\Programs\CADWorkbench'",
    "Remove-Item -LiteralPath `$startMenu -Recurse -Force -ErrorAction SilentlyContinue",
    "Remove-Item -LiteralPath (Join-Path ([Environment]::GetFolderPath('Desktop')) 'CADWorkbench.lnk') -Force -ErrorAction SilentlyContinue",
    "Remove-Item -LiteralPath `$installRoot -Recurse -Force -ErrorAction SilentlyContinue"
)
$uninstall = $uninstall -join [Environment]::NewLine
$uninstall | Set-Content -LiteralPath (Join-Path $installRoot 'Uninstall-CADWorkbench.ps1') -Encoding utf8
$uninstallLink = $shell.CreateShortcut((Join-Path $startMenu 'Uninstall CADWorkbench.lnk'))
$uninstallLink.TargetPath = 'powershell.exe'
$uninstallLink.Arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $installRoot 'Uninstall-CADWorkbench.ps1') + '"'
$uninstallLink.WorkingDirectory = $installRoot
$uninstallLink.Save()
Write-Host "CADWorkbench installed to $installRoot"
'@
$installScriptPath = Join-Path $payload 'Install-CADWorkbench.ps1'
$installScript | Set-Content -LiteralPath $installScriptPath -Encoding utf8
$payloadZip = Join-Path $payload 'CADWorkbench_v1_55_Windows_Portable.zip'
Copy-Item -LiteralPath $portableZip -Destination $payloadZip

$sourceZip = Join-Path $OutputDirectory 'CADWorkbench_v1_55_GitHub_Source.zip'
if (Test-Path -LiteralPath $sourceZip) { Remove-Item -LiteralPath $sourceZip -Force }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipStream = [IO.File]::Open($sourceZip,[IO.FileMode]::CreateNew)
try {
    $archive = [IO.Compression.ZipArchive]::new($zipStream,[IO.Compression.ZipArchiveMode]::Create)
    try {
        Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Force | ForEach-Object {
            $relative = [IO.Path]::GetRelativePath($sourceRoot,$_.FullName)
            $parts = $relative -split '[\\/]'
            if ($parts -contains '.git' -or $parts -contains '__pycache__' -or
                $parts -contains '.venv-cadquery' -or $parts -contains 'build' -or
                $parts -contains 'vcpkg_installed' -or $_.Extension -in @('.obj','.lib','.pdb','.ilk') -or
                $_.Name -match '\.(log|zip|7z|exe)$') { return }
            if ($relative -match '(^|[\\/])(out|cmake-build[^\\/]*)([\\/]|$)') { return }
            $entryName = 'CADWorkbench_v1_55_Source/' + ($relative -replace '\\','/')
            $entry = $archive.CreateEntry($entryName,[IO.Compression.CompressionLevel]::Optimal)
            $entryStream = $entry.Open()
            try {
                $fileStream = [IO.File]::OpenRead($_.FullName)
                try { $fileStream.CopyTo($entryStream) } finally { $fileStream.Dispose() }
            } finally { $entryStream.Dispose() }
        }
    } finally { $archive.Dispose() }
} finally { $zipStream.Dispose() }

$sedPath = Join-Path $tempRoot 'CADWorkbench_Setup.sed'
$setupExe = Join-Path $OutputDirectory 'CADWorkbench_v1_55_Windows_Setup.exe'
$sed = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=0
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=1
CAB_FixedSize=0
CAB_ResvSize=0
RebootMode=N
InstallPrompt=
DisplayLicense=
FinishMessage=
TargetName=$setupExe
FriendlyName=CADWorkbench v1.55.0 Setup
AppLaunched=powershell.exe -NoProfile -ExecutionPolicy Bypass -File Install-CADWorkbench.ps1
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
[SourceFiles]
SourceFiles0=$payload
[SourceFiles0]
%FILE0%=$installScriptPath
%FILE1%=$payloadZip
[Strings]
FILE0=Install-CADWorkbench.ps1
FILE1=CADWorkbench_v1_55_Windows_Portable.zip
"@
$sed | Set-Content -LiteralPath $sedPath -Encoding ascii
$iexpress = Join-Path $env:WINDIR 'System32\iexpress.exe'
if (-not (Test-Path -LiteralPath $iexpress -PathType Leaf)) { throw 'Windows IExpress was not found; the portable and source ZIPs were still created.' }
$startedAt = Get-Date
if (Test-Path -LiteralPath $setupExe) { Remove-Item -LiteralPath $setupExe -Force }
$iexpressProcess = Start-Process -FilePath $iexpress -ArgumentList @('/N','/Q',$sedPath) -PassThru
$null = $iexpressProcess.WaitForExit(30000)
$lastSize = -1L
$stableSamples = 0
$deadline = (Get-Date).AddMinutes(5)
while ((Get-Date) -lt $deadline -and $stableSamples -lt 3) {
    Start-Sleep -Seconds 1
    if (-not (Test-Path -LiteralPath $setupExe -PathType Leaf)) { continue }
    $current = Get-Item -LiteralPath $setupExe
    if ($current.LastWriteTime -ge $startedAt -and $current.Length -gt 1MB -and $current.Length -eq $lastSize) { $stableSamples++ }
    else { $stableSamples = 0 }
    $lastSize = $current.Length
}
if ($stableSamples -lt 3) {
    throw 'IExpress could not create the Windows setup executable. The portable and source ZIPs were still created.'
}

# IExpress leaves intermediate cabinet directives beside the output. Keep the
# distributable setup only; the final EXE contains the embedded cabinet.
foreach ($suffix in @('CAB','DDF','LAYOUT.INF','RPT')) {
    $intermediate = Join-Path $OutputDirectory ('~CADWorkbench_v1_55_Windows_Setup.' + $suffix)
    if (Test-Path -LiteralPath $intermediate -PathType Leaf) { Remove-Item -LiteralPath $intermediate -Force }
}

Write-Host "Source ZIP: $sourceZip"
Write-Host "Portable app: $portableZip"
Write-Host "Windows installer: $setupExe"



