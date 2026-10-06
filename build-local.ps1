# Local MSVC build of the integrated edition. Keep this script ASCII for PS 5.1.
param(
    [string]$Config = 'RelWithDebInfo',
    [string]$SourceLink = '',
    [string]$DependencyRoot = '',
    [int]$Jobs = 4,
    [switch]$Reconfigure
)
$ErrorActionPreference = 'Stop'
$Cmake = 'F:\VS\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $Cmake)) {
    $Cmake = (Get-Command cmake -ErrorAction Stop).Source
}
$Cache = Join-Path $PSScriptRoot 'build-integrated\CMakeCache.txt'
if (-not $SourceLink -and (Test-Path -LiteralPath $Cache)) {
    $CacheSource = Get-Content -LiteralPath $Cache |
        Where-Object { $_.StartsWith('CMAKE_HOME_DIRECTORY:INTERNAL=') } |
        Select-Object -First 1
    if ($CacheSource) { $SourceLink = Split-Path ($CacheSource -split '=', 2)[1] }
}
if (-not $SourceLink) { $SourceLink = $PSScriptRoot }
if ($SourceLink -match '[^\x00-\x7F]') {
    throw 'Pass -SourceLink with an ASCII junction pointing to THIS repository (see docs/local-integration-20260924.md).'
}
if (-not (Test-Path -LiteralPath (Join-Path $SourceLink 'juce\CMakeLists.txt'))) {
    throw "Source link is missing: $SourceLink"
}
# Avoid accidentally building the old checkout through its existing junction.
$ThisSource = Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'juce\CMakeLists.txt')
$LinkedSource = Get-FileHash -LiteralPath (Join-Path $SourceLink 'juce\CMakeLists.txt')
if ($ThisSource.Hash -ne $LinkedSource.Hash) { throw 'SourceLink points to another source tree.' }
$Build = Join-Path $SourceLink 'build-integrated'
if ($Reconfigure -or -not (Test-Path -LiteralPath $Cache)) {
    $ConfigureArgs = @('-S', (Join-Path $SourceLink 'juce'), '-B', $Build,
        '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DHACHI_ENABLE_ONNX_ANALYSIS=ON')
    $PrivateJuce = Join-Path $SourceLink '.deps\juce-src'
    if (Test-Path -LiteralPath $PrivateJuce) {
        $ConfigureArgs += "-DFETCHCONTENT_SOURCE_DIR_JUCE=$PrivateJuce"
    }
    if ($DependencyRoot) {
        $ConfigureArgs += "-DFETCHCONTENT_SOURCE_DIR_ONNXRUNTIME=$DependencyRoot/onnxruntime-src"
        $ConfigureArgs += "-DFETCHCONTENT_SOURCE_DIR_DIRECTML_RUNTIME=$DependencyRoot/directml_runtime-src"
    }
    & $Cmake @ConfigureArgs
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
}
& $Cmake --build $Build --config $Config --target HachiShifterNext --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed.' }
Write-Host (Join-Path $Build "HachiShifterNext_artefacts\$Config\HachiShifter Next.exe")
