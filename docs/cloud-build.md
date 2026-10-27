# Windows cloud build and portable packages

The `dev` branch is the rolling development source through update 204 (2026-10-27), published in the existing flattened format: root `CMakeLists.txt`, `src/`, `engines/`, `resources/`, `third_party/`, `tools/` and `docs/`. Existing cloud-native compiler patches and split package delivery are retained. No README is supplied; the repository owner maintains it separately. Public application/version-resource text is **dev**. CMake and Windows use the legal internal numeric tuple **0.0.0**; this is not a release label. WCSNDM remains **0.0803**.

## Build

GitHub Actions runs on Windows Server 2022 with the VS2022 x64 SDK, clang-cl and Ninja. Neural C++ analysis is enabled. Dependencies are versioned/hash checked by CMake; the nested JUCE helper inherits the selected compiler. Build parallelism is bounded to avoid the large diagnostic translation unit exhausting runner memory.

From a VS x64 developer shell:

```powershell
./tools/build-windows-cloud.ps1 -BuildDirectory build-windows -Jobs 2
```

The workflow runs on pushes to `dev` and supports manual dispatch. It produces:

- `HachiShifterNext-windows-x64-app`: executable, **external `lang/translations.tsv`**, native runtime DLLs, VC CRT, engine scripts/dictionaries, portable Python dependencies and notices; no large analysis/synthesis weights or optional English G2P checkpoint. Packaging checks the dev executable metadata and required five-language import-option entries.
- `HachiShifterNext-windows-x64-with-models`: the same built application plus the owner-approved checksum-pinned resource bundle. This includes GAME medium, FCPE, NSF-HiFiGAN, the native float32 HNSEP graph, English G2P, and the requested ASR/TIFA resources, dictionaries/configuration and licenses.

Each artifact contains a ZIP and its SHA256. Preserve the portable directory layout; do not move only the executable. The app-only package requires model files to be supplied before neural features can be used. Neither package supplies user voicebanks, personal audio/projects, commercial Melodyne, or independently installed third-party engines.

## Resource integrity and feature scope

`.github/model-resources.json` pins the owner's model-resource Release URL and archive SHA256. The ZIP has `resource-manifest.json`, with per-file size/SHA/source/license. Packaging verifies every listed resource and required group. Model weights are not Git blobs. The repository owner explicitly authorized distribution of the provided models; that authorization does not invent or replace the upstream licenses.

The current development tree contains Japanese annotation/ASR/TIFA implementation code. Resource availability, upstream model compatibility and actual inference quality are separate from source inclusion; this version-label/publication change does not claim fresh ASR/TIFA runtime validation. The resource archive URL and checksums remain the existing owner-approved pinned bundle, not a newly relabeled or silently replaced set of weights.

GAME/TIFA weights and other resources have their own terms, including non-commercial/share-alike conditions where documented. Local ASR weight provenance does not grant an unspecified open-source license. See all bundle notices before redistributing. Native HNSEP is exported offline from the pinned official checkpoint/wrapper; it is not an incompatible graph renamed to satisfy a path.

DiffSinger uses the existing Python bridge plus the user's compatible voicebank. Other optional external WCS/HAMOOD/tempo/separation installations are not falsely represented as complete merely because these native model files are included. No voicebank acoustic models are downloaded implicitly by this workflow.

## Licensing and corresponding source

The root application license is not a blanket license for the binary and all its dependencies. JUCE, libLLSM2, WORLD, Ciglet, Signalsmith, ONNX Runtime, DirectML, Python, engine dictionaries and neural weights have separate terms. Packages retain notices and link to the exact source commit. License-only files from the old source-root `third_party` directory are retained under `docs/licenses`, without importing that unrelated directory layout.
