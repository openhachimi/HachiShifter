# Native Windows Build (MSYS2 + xwin)

This guide documents how to build the UTAU-specific version natively on
Windows using an MSYS2-provided LLVM/Clang toolchain and `xwin`-provisioned
Microsoft SDK/CRT. It uses native Windows executables only — no WSL, Wine, or
Visual Studio installation is required.

## Overview

The build targets `x86_64-pc-windows-msvc` with:

- Clang / clang-cl and LLD from an MSYS2 `clang64` package set.
- CMake and Ninja from the same package set.
- The Microsoft CRT and Windows SDK headers/libraries provisioned locally by
  `xwin` (no Visual Studio required).
- ONNX Runtime and DirectML enabled for the neural (NSF-HiFiGAN) backend.

`clang-cl` is used in CL driver mode and links with LLD. The MSYS2 package's
default GNU/MinGW target must **not** be used for the JUCE application.

## Prerequisites

Pick a working directory on any drive with a few GB free and export it as the
toolchain root. All generated files and caches stay under it, so the build
never touches the system PATH or machine environment.

```powershell
# Choose any folder; this doc calls it TOOLCHAIN_ROOT.
$env:TOOLCHAIN_ROOT = 'X:\toolchain'
```

Install/place under `TOOLCHAIN_ROOT`:

| Component | Location under `TOOLCHAIN_ROOT` |
| --- | --- |
| Clang/LLD, CMake, Ninja (MSYS2 `clang64`) | `packages\clang64\bin` |
| `xwin` executable | `xwin\xwin.exe` |
| `xwin`-provisioned MSVC + Windows SDK | `sdk` (x64 desktop) |
| Package DB / cache | `pacman-db`, `pacman-cache` |
| CMake dependency cache | `fetch` |
| Build directory | `build\release` |
| Temp files | `tmp` |

An existing MSYS2 `pacman` may be reused to install the `clang64` group; only
its packages are used, not its MinGW compiler target.

Provision the SDK once with `xwin` (MSVC headers/libraries and the Windows SDK,
x64 desktop only), pointing its output at `sdk`.

## Build

From the repository directory:

```powershell
.\build-windows-native.ps1
# Configure only:
.\build-windows-native.ps1 -ConfigureOnly
# Parallel jobs:
.\build-windows-native.ps1 -Jobs 4
```

The script uses `juce/cmake/windows-msys2-xwin-native.cmake`, keeps generated
files and dependency downloads under `TOOLCHAIN_ROOT`, and enables ONNX
Runtime / DirectML. Models are never downloaded or bundled.

Close the built application before re-linking, or the linker cannot replace the
running executable.

Output:

```text
<TOOLCHAIN_ROOT>\build\release\<app>_artefacts\Release\<app>.exe
```

Keep `onnxruntime.dll`, `onnxruntime_providers_shared.dll`, and `DirectML.dll`
alongside the executable.

## Compatibility notes

- JUCE's helper-tool bootstrap does not, by default, pass the parent toolchain
  to its child configure step. A local CMake dependency patch builds the helper
  with the same clang-cl / xwin setup.
- Recent clang-cl exposes an invalid array-layout forwarding constructor; the
  same dependency patch preserves array bounds and restores a matching
  `AudioProcessor` forwarding overload. Neither patch changes the application
  ABI.
- A separate Linux cross-compilation toolchain file remains available for CI;
  this native build uses its own toolchain file.

## Software-rendering fallback

The Settings page offers a software-rendering fallback. To force it before
launch (for example on a device whose GPU path misbehaves):

```powershell
$env:HACHI_SOFTWARE_RENDERING = '1'
& '<TOOLCHAIN_ROOT>\build\release\<app>_artefacts\Release\<app>.exe'
```

Remove the environment variable to return to the saved setting / default GPU
mode.

## Disk footprint

Approximate logical sizes after removing the `xwin` extraction/download cache
and package downloads:

| Component | MiB |
| --- | ---: |
| MSYS2 native tools and dependencies | ~1080 |
| xwin SDK / CRT | ~620 |
| JUCE / ONNX / DirectML dependency cache (incl. helper) | ~710 |
| Windows build (EXE + DLLs) | ~100 |

Keep `sdk`, `packages`, and `pacman-db` for future builds; `fetch` and the build
folder support incremental builds and should normally be retained.
