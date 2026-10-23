# Run inside the x64 VS2022 developer environment supplied by the workflow.
[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build-windows',
    [string]$FetchDirectory = '',
    [ValidateRange(1, 3)][int]$Jobs = 2,
    [switch]$ConfigureOnly
)
$ErrorActionPreference = 'Stop'
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = if ([IO.Path]::IsPathRooted($BuildDirectory)) {
    [IO.Path]::GetFullPath($BuildDirectory)
} else { [IO.Path]::GetFullPath((Join-Path $source $BuildDirectory)) }
if (-not $env:VCToolsInstallDir -or -not $env:WindowsSdkDir) {
    throw 'Initialize the x64 Visual Studio developer environment before this script.'
}
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$clang = (Get-Command clang-cl -ErrorAction Stop).Source
$null = Get-Command ninja -ErrorAction Stop
$env:CMAKE_BUILD_PARALLEL_LEVEL = "$Jobs"
# A build tree embeds its source/compiler paths and must not be borrowed.
$cache = Join-Path $build 'CMakeCache.txt'
if (Test-Path -LiteralPath $cache) {
    $homeEntry = Select-String -LiteralPath $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$'
    if ($homeEntry -and [IO.Path]::GetFullPath($homeEntry.Matches[0].Groups[1].Value) -ne $source) {
        throw 'BuildDirectory belongs to another source tree; choose a fresh directory.'
    }
}
$configure = @('-S', $source, '-B', $build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_C_COMPILER=$clang", "-DCMAKE_CXX_COMPILER=$clang",
    '-DHACHI_ENABLE_ONNX_ANALYSIS=ON', '-DHACHI_COPY_ANALYSIS_MODELS=OFF')
if ($FetchDirectory) {
    $fetch = if ([IO.Path]::IsPathRooted($FetchDirectory)) { $FetchDirectory } else { Join-Path $source $FetchDirectory }
    $configure += "-DFETCHCONTENT_BASE_DIR=$([IO.Path]::GetFullPath($fetch))"
}
& $cmake @configure
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
if ($ConfigureOnly) { return }
& $cmake --build $build --target HachiShifterNext --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "CMake build failed ($LASTEXITCODE)" }
$artifactDir = Join-Path $build 'HachiShifterNext_artefacts'
$releaseDir = Join-Path $artifactDir 'Release'
$exe = Join-Path $releaseDir 'HachiShifter Next.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Missing expected executable: $exe" }
foreach ($dll in @('onnxruntime.dll', 'onnxruntime_providers_shared.dll', 'DirectML.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $releaseDir $dll) -PathType Leaf)) {
        throw "Missing required runtime: $dll"
    }
}
if ($env:GITHUB_OUTPUT) {
    "artifactDir=$artifactDir" | Out-File -LiteralPath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "releaseDir=$releaseDir" | Out-File -LiteralPath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "exePath=$exe" | Out-File -LiteralPath $env:GITHUB_OUTPUT -Encoding utf8 -Append
}
[pscustomobject]@{ artifactDir = $artifactDir; releaseDir = $releaseDir; exePath = $exe }
