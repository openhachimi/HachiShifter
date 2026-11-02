[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ReleaseDirectory,
    [string]$OutputDirectory = 'dist',
    [string]$FetchDirectory = '',
    [hashtable]$DependencyDirectories = @{},
    [string]$CrtDirectory = '',
    [string]$SourceCommit = '',
    [string]$PackageName = 'HachiShifterNext-windows-x64-app'
)
$ErrorActionPreference = 'Stop'
function Get-PackageRelativePath([string]$Base,[string]$File) {
    $prefix = [IO.Path]::GetFullPath($Base).TrimEnd([char[]]'/\') + [IO.Path]::DirectorySeparatorChar
    $full = [IO.Path]::GetFullPath($File)
    if (-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Package path is outside its declared root' }
    return $full.Substring($prefix.Length)
}
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$release = [IO.Path]::GetFullPath($ReleaseDirectory)
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $root $OutputDirectory }
if ($PackageName -notmatch '^[A-Za-z0-9._-]+$') { throw 'Unsafe package name' }
$stage = Join-Path $out $PackageName
if (Test-Path -LiteralPath $stage) { throw "Refusing to merge an existing package: $stage" }
New-Item -ItemType Directory -Force $stage | Out-Null
# Explicit whitelist: never recurse through models, engines or the runtime folder.
foreach ($name in @('HachiShifter Next.exe','onnxruntime.dll','onnxruntime_providers_shared.dll','DirectML.dll')) {
    $source = Join-Path $release $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required runtime missing: $name" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage $name)
}
$catalog = Join-Path $release 'lang/translations.tsv'
if (-not (Test-Path -LiteralPath $catalog -PathType Leaf)) { throw 'External translations missing' }
New-Item -ItemType Directory -Force (Join-Path $stage 'lang') | Out-Null
Copy-Item -LiteralPath $catalog -Destination (Join-Path $stage 'lang/translations.tsv')
if ((Get-Item -LiteralPath (Join-Path $stage 'HachiShifter Next.exe')).VersionInfo.ProductVersion -ne 'dev') { throw 'Expected public version dev' }
if (-not $CrtDirectory -and $env:VCToolsRedistDir) {
    $crt = Get-ChildItem -LiteralPath (Join-Path $env:VCToolsRedistDir 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
    if ($crt) { $CrtDirectory = $crt.FullName }
}
if (-not $CrtDirectory) { $CrtDirectory = [Environment]::SystemDirectory }
foreach ($name in @('msvcp140.dll','msvcp140_1.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    $source = Join-Path $CrtDirectory $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "VC++ x64 redistributable missing: $name" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage $name)
}
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'LICENSE')
$licenses = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Force $licenses | Out-Null
foreach ($relativeRoot in @('docs/licenses','third_party','assets/toolbar-icons')) {
    $base = Join-Path $root $relativeRoot
    if (-not (Test-Path -LiteralPath $base)) { continue }
    foreach ($file in Get-ChildItem -LiteralPath $base -Recurse -File | Where-Object { $_.Name -match '^(LICENSE|COPYING|NOTICE|COPYRIGHT)' }) {
        $relative = (Get-PackageRelativePath $root $file.FullName)
        $destination = Join-Path $licenses $relative
        New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination
    }
}
if (-not $FetchDirectory) { $FetchDirectory = $env:FETCH_CACHE }
if ($FetchDirectory -or $DependencyDirectories.Count) {
    foreach ($dependency in @('juce','yaml_cpp','onnxruntime','directml_runtime')) {
        $base = if ($DependencyDirectories.ContainsKey($dependency)) { $DependencyDirectories[$dependency] } else { Join-Path $FetchDirectory "$dependency-src" }
        if (-not (Test-Path -LiteralPath $base)) { throw "Dependency notices missing: $dependency" }
        foreach ($file in Get-ChildItem -LiteralPath $base -Recurse -File | Where-Object { $_.Name -match '(?i)(licen[cs]e|copying|notice|copyright)' }) {
            if ($file.Extension -in @('.exe','.dll','.pyd','.onnx','.npz','.bin')) { continue }
            $destination = Join-Path (Join-Path $licenses $dependency) ((Get-PackageRelativePath $base $file.FullName))
            New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $destination
        }
    }
}
if (-not $SourceCommit) { $SourceCommit = $env:GITHUB_SHA }
if (-not $SourceCommit) { $SourceCommit = (& git -C $root rev-parse HEAD).Trim() }
$sourceUrl = "https://github.com/openhachimi/HachiShifter/tree/$SourceCommit"
@("Corresponding source: $sourceUrl",'Lightweight program update: no models, Python, DiffSinger engine, voicebanks or user projects.', 'Preserve your existing models/engines folders when applying this update. MLD5 DSP is statically linked.', 'Neural analysis/NSF and DiffSinger still require separately installed resources.', 'Application LICENSE is not a blanket license for JUCE/WORLD/libLLSM2/ORT/DirectML/MSVC or other dependencies. See licenses/.', 'Microsoft Visual C++ redistributable components remain subject to Microsoft terms.') | Set-Content -LiteralPath (Join-Path $stage 'SOURCE-NOTICE.txt') -Encoding utf8
$info = [ordered]@{ publicVersion='dev'; numericVersion='0.0.0'; update=207; wcsndmVersion='0.0803'; sourceCommit=$SourceCommit; source=$sourceUrl; modelsIncluded=$false; diffSingerEngineIncluded=$false; pythonIncluded=$false; translationSHA256=(Get-FileHash -LiteralPath $catalog -Algorithm SHA256).Hash.ToLowerInvariant(); executableSHA256=(Get-FileHash -LiteralPath (Join-Path $stage 'HachiShifter Next.exe') -Algorithm SHA256).Hash.ToLowerInvariant() }
$info | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'build-info.json') -Encoding utf8
foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File) {
    $relative = (Get-PackageRelativePath $stage $file.FullName).Replace('\','/')
    if ($relative -match '(^|/)(models|engines|site-packages|__pycache__)/' -or $file.Extension -in @('.onnx','.npz','.pt','.pth','.ckpt','.safetensors','.mpd','.hjpx','.wav')) { throw "Forbidden payload: $relative" }
}
$manifest = @(Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object { [ordered]@{ path=(Get-PackageRelativePath $stage $_.FullName).Replace('\','/'); bytes=$_.Length; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() } })
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $stage 'file-manifest.json') -Encoding utf8
$zip = Join-Path $out "$PackageName.zip"
if (Test-Path -LiteralPath $zip) { throw 'Refusing to overwrite an existing archive' }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + (Split-Path -Leaf $zip)) | Set-Content -LiteralPath "$zip.sha256" -Encoding ascii
if ($env:GITHUB_OUTPUT) { "package=$zip" | Out-File -LiteralPath $env:GITHUB_OUTPUT -Append -Encoding utf8; "stage=$stage" | Out-File -LiteralPath $env:GITHUB_OUTPUT -Append -Encoding utf8 }
Write-Output "package=$zip"
Write-Output "stage=$stage"
