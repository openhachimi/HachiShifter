param(
    [string]$ToolchainRoot = 'E:\hjs-tc',
    [int]$Jobs = 4,
    [switch]$ConfigureOnly
)
$ErrorActionPreference = 'Stop'
$env:HACHI_TC_ROOT = $ToolchainRoot
$env:TEMP = Join-Path $ToolchainRoot 'tmp'
$env:TMP = $env:TEMP
$tools = Join-Path $ToolchainRoot 'packages\clang64\bin'
$env:PATH = "$tools;$env:PATH"
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
$cmake = Join-Path $tools 'cmake.exe'
$source = Join-Path $PSScriptRoot 'juce'
$build = Join-Path $ToolchainRoot 'build\hachishifter-release'
$cache = Join-Path $ToolchainRoot 'fetch'
& $cmake -S $source -B $build -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_TOOLCHAIN_FILE=$source\cmake\windows-msys2-xwin-native.cmake" "-DFETCHCONTENT_BASE_DIR=$cache" '-DHACHI_ENABLE_ONNX_ANALYSIS=ON'
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
if (-not $ConfigureOnly) {
    & $cmake --build $build --target HachiShifterNext --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
}
