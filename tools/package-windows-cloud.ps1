[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ReleaseDirectory,
    [string]$OutputDirectory = 'dist',
    [string]$ModelArchive = '',
    [string]$ModelArchiveSha256 = '',
    [switch]$WithModels
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { [IO.Path]::GetFullPath($OutputDirectory) } else { [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory)) }
$release = [IO.Path]::GetFullPath($ReleaseDirectory)
$name = if ($WithModels) { 'HachiShifterNext-windows-x64-with-models' } else { 'HachiShifterNext-windows-x64-app' }
$stage = Join-Path $out $name
if (Test-Path -LiteralPath $stage) { throw "Refusing to merge into an existing package: $stage" }
New-Item -ItemType Directory -Force $stage | Out-Null
foreach ($file in @('HachiShifter Next.exe','onnxruntime.dll','onnxruntime_providers_shared.dll','DirectML.dll')) {
    $source = Join-Path $release $file
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing runtime: $source" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage $file)
}
# Engine scripts and pronunciation dictionaries are runtime resources. Large
# weights, including the optional English G2P checkpoint, belong to the model zip.
$engineRoot = Join-Path $release 'engines'
if (-not (Test-Path -LiteralPath $engineRoot -PathType Container)) { throw 'Missing engine resources' }
foreach ($file in Get-ChildItem -LiteralPath $engineRoot -Recurse -File) {
    if ($file.Extension -in @('.onnx','.npz','.pt','.pth','.ckpt')) { continue }
    $relative = [IO.Path]::GetRelativePath($engineRoot,$file.FullName)
    $destination = Join-Path (Join-Path $stage 'engines') $relative
    New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
}
# A portable embedded interpreter is needed by the existing DiffSinger bridge.
$pythonZip = Join-Path $env:RUNNER_TEMP 'python-3.11.9-embed-amd64.zip'
$pythonHash = '009d6bf7e3b2ddca3d784fa09f90fe54336d5b60f0e0f305c37f400bf83cfd3b'
if (-not (Test-Path -LiteralPath $pythonZip)) { Invoke-WebRequest 'https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip' -OutFile $pythonZip }
if ((Get-FileHash -LiteralPath $pythonZip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pythonHash) { throw 'Portable Python checksum mismatch' }
$python = Join-Path $stage 'engines/python'
New-Item -ItemType Directory -Force $python | Out-Null
Expand-Archive -LiteralPath $pythonZip -DestinationPath $python
@('python311.zip','.','Lib/site-packages','import site') | Set-Content -LiteralPath (Join-Path $python 'python311._pth') -Encoding ascii
$site = Join-Path $python 'Lib/site-packages'
& python -m pip install --disable-pip-version-check --only-binary=:all: --no-compile --report (Join-Path $python 'dependencies.json') --target $site -r (Join-Path $stage 'engines/diffsinger/requirements.txt')
if ($LASTEXITCODE) { throw "Portable Python dependency installation failed ($LASTEXITCODE)" }
& (Join-Path $python 'python.exe') -c 'import numpy, onnxruntime, yaml, soundfile; print("portable_bridge_dependencies=ok")'
if ($LASTEXITCODE) { throw 'Portable Python import verification failed' }
$crtRoot = Join-Path $env:VCToolsRedistDir 'x64'
$crt = Get-ChildItem -LiteralPath $crtRoot -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
if (-not $crt) { throw 'MSVC redistributable CRT was not found' }
Copy-Item -Path (Join-Path $crt.FullName '*.dll') -Destination $stage
Copy-Item -Path (Join-Path $crt.FullName '*.dll') -Destination $python -Force
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'LICENSE')
$licenses = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Force $licenses | Out-Null
if (Test-Path -LiteralPath (Join-Path $root 'docs/licenses')) { Copy-Item -LiteralPath (Join-Path $root 'docs/licenses') -Destination (Join-Path $licenses 'project-notices') -Recurse }
foreach ($sourceDir in @('third_party','assets/toolbar-icons')) {
    $base = Join-Path $root $sourceDir
    foreach ($file in Get-ChildItem -LiteralPath $base -Recurse -File | Where-Object { $_.Name -match '^(LICENSE|COPYING|NOTICE|COPYRIGHT)' }) {
        $relative = [IO.Path]::GetRelativePath($root,$file.FullName)
        $destination = Join-Path $licenses $relative
        New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination
    }
}
# FetchContent libraries are compiled/linked but live outside the Git tree.
# Preserve their own notices too, not merely the application's root license.
foreach ($dependency in @('juce','yaml_cpp','onnxruntime','directml_runtime')) {
    $dependencyRoot = Join-Path $env:FETCH_CACHE "$dependency-src"
    if (-not (Test-Path -LiteralPath $dependencyRoot -PathType Container)) { throw "Missing fetched dependency notices: $dependencyRoot" }
    $legalFiles = Get-ChildItem -LiteralPath $dependencyRoot -Recurse -File | Where-Object {
        $_.Name -match '(?i)(licen[cs]e|copying|notice|copyright)' -or
        ($_.Name -match '^(?i:readme)' -and $_.FullName -match '(?i)(3rdparty|third_party)')
    }
    foreach ($file in $legalFiles) {
        $relative = [IO.Path]::GetRelativePath($dependencyRoot,$file.FullName)
        if ($file.Name -match '^(?i:readme)') { $relative += '.legal-notice.txt' }
        $destination = Join-Path (Join-Path $licenses $dependency) $relative
        New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination
    }
}
$sourceUrl = if ($env:GITHUB_REPOSITORY -and $env:GITHUB_SHA) { "https://github.com/$env:GITHUB_REPOSITORY/tree/$env:GITHUB_SHA" } else { 'https://github.com/openhachimi/HachiShifter/tree/dev' }
@("Corresponding source: $sourceUrl",'Includes separately licensed JUCE, libLLSM2, WORLD, ONNX Runtime, DirectML, Python and other dependencies.','The application LICENSE is not a blanket license for bundled libraries, model weights or user voicebanks.','See licenses/ and the license files within engines/python and the engine pronunciation data.') | Set-Content -LiteralPath (Join-Path $stage 'SOURCE-NOTICE.txt') -Encoding utf8
if ($WithModels) {
    if (-not $ModelArchive -or $ModelArchiveSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'A checksum-pinned model archive is required' }
    if ((Get-FileHash -LiteralPath $ModelArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ModelArchiveSha256.ToLowerInvariant()) { throw 'Model archive SHA256 mismatch' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($ModelArchive))
    try {
        $prefix = [IO.Path]::GetFullPath($stage) + [IO.Path]::DirectorySeparatorChar
        foreach ($entry in $zip.Entries) {
            $target = [IO.Path]::GetFullPath((Join-Path $stage $entry.FullName))
            if (-not $target.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw "Unsafe model archive entry: $($entry.FullName)" }
        }
    } finally { $zip.Dispose() }
    Expand-Archive -LiteralPath $ModelArchive -DestinationPath $stage -Force
    $manifest = Get-Content -LiteralPath (Join-Path $stage 'resource-manifest.json') -Raw | ConvertFrom-Json
    foreach ($entry in $manifest.files) {
        $file = Join-Path $stage $entry.path
        if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -ne $entry.bytes) { throw "Missing or truncated model resource: $($entry.path)" }
        if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256.ToLowerInvariant()) { throw "Model resource checksum mismatch: $($entry.path)" }
    }
    foreach ($group in @('game/medium','fcpe','nsf_hifigan','nsf_hifigan/hnsep','japanese_annotation/asr','japanese_annotation/tifa')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage "models/$group") -PathType Container)) { throw "Missing required model group: $group" }
    }
}
$info = [ordered]@{ source=$sourceUrl; publicVersion='0.25'; wcsndmVersion='0.0803'; variant=$name; portablePython='3.11.9'; portablePythonSha256=$pythonHash; llvm=(& clang-cl --version | Select-Object -First 1); sdk=$env:WindowsSDKVersion; msvc=$env:VCToolsVersion; models=$WithModels.IsPresent; sourceBaseline='r/HachiShifter-0.2.6-dev/source'; annotationFeatureInThisBaseline=$false }
$info | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'build-info.json') -Encoding utf8
$zipPath = Join-Path $out "$name.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -CompressionLevel Optimal
(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant() | Set-Content -LiteralPath "$zipPath.sha256" -Encoding ascii
if ($env:GITHUB_OUTPUT) { "package=$zipPath" | Out-File -LiteralPath $env:GITHUB_OUTPUT -Encoding utf8 -Append; "stage=$stage" | Out-File -LiteralPath $env:GITHUB_OUTPUT -Encoding utf8 -Append }
Write-Output "package=$zipPath"
