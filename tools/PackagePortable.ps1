param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$SkyboxDirectory,
    [Parameter(Mandatory=$true)][string]$FfmpegDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$destination=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $destination){throw 'Output already exists; choose a fresh directory.'}
$zip=$destination+'.zip'
if(Test-Path -LiteralPath $zip){throw 'ZIP already exists; choose a fresh directory.'}
foreach($required in @($Executable,(Join-Path $FfmpegDirectory 'bin/ffmpeg.exe'),$SkyboxDirectory)){
    if(!(Test-Path -LiteralPath $required)){throw "Missing package input: $required"}
}
New-Item -ItemType Directory -Path $destination | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $destination 'Cadence.exe')
foreach($folder in @('skyboxes','tools/licenses','Cadence Assets/licenses','Cadence Assets/settings','Cadence Assets/scopes','Cadence Assets/sky')){
    New-Item -ItemType Directory -Path (Join-Path $destination $folder) -Force | Out-Null
}
# Whitelist runtime sky assets; do not copy incidental personal imgui.ini files.
$skyRoot=(Resolve-Path -LiteralPath $SkyboxDirectory).Path
foreach($file in Get-ChildItem -LiteralPath $skyRoot -Recurse -File -Filter '*.iwi'){
    $relative=[IO.Path]::GetRelativePath($skyRoot,$file.FullName)
    $target=Join-Path (Join-Path $destination 'skyboxes') $relative
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target
}
Copy-Item -LiteralPath (Join-Path $repo 'Cadence Assets/scopes/scope_overlay_dsr50_1024.png') -Destination (Join-Path $destination 'Cadence Assets/scopes')
foreach($name in @('eso0932a.jpg','CREDITS.md')){Copy-Item -LiteralPath (Join-Path $repo "Cadence Assets/sky/$name") -Destination (Join-Path $destination 'Cadence Assets/sky')}
Copy-Item -LiteralPath (Join-Path $repo 'packaging/portable.flag') -Destination (Join-Path $destination 'Cadence Assets/settings')
Copy-Item -LiteralPath (Join-Path $FfmpegDirectory 'bin/ffmpeg.exe') -Destination (Join-Path $destination 'tools')
Copy-Item -LiteralPath (Join-Path $FfmpegDirectory 'LICENSE') -Destination (Join-Path $destination 'tools/licenses/FFmpeg-GPLv3.txt')
Copy-Item -LiteralPath (Join-Path $FfmpegDirectory 'README.txt') -Destination (Join-Path $destination 'tools/licenses/FFmpeg-build-and-source.txt')
foreach($entry in @(
    @('build-ui/_deps/glfw-src/LICENSE.md','glfw.txt'),
    @('build-ui/_deps/imgui-src/LICENSE.txt','imgui.txt'),
    @('external/cgltf/LICENSE','cgltf.txt'),
    @('external/libwebp/COPYING','libwebp.txt'),
    @('external/libwebp/PATENTS','libwebp-patents.txt'),
    @('external/libwebp/AUTHORS','libwebp-authors.txt')
)){
    Copy-Item -LiteralPath (Join-Path $repo $entry[0]) -Destination (Join-Path $destination ('Cadence Assets/licenses/'+$entry[1]))
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($destination,$zip,[IO.Compression.CompressionLevel]::Optimal,$true)
Get-Item -LiteralPath $zip | Select-Object FullName,Length
