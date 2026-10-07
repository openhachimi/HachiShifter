# Native NSF-HiFiGAN model installation

The editor needs an ONNX model in addition to ONNX Runtime. The external
WCSNDM engine's PyTorch checkpoint does not supply the editor's native model.

Download `pc_nsf_hifigan_44.1k_hop512_128bin_2025.02.oudep` from the
[official OpenVPI release](https://github.com/openvpi/vocoders/releases/tag/pc-nsf-hifigan-44.1k-hop512-128bin-2025.02), then run:

```powershell
python tools/prepare-nsf-model.py RELEASE.oudep 'APP/models/nsf_hifigan'
```

Replace `APP` with the directory containing the executable. The helper checks
the official archive SHA256, copies the unmodified model and original notices,
and maps the release's vocoder.yaml audio parameters into config.json. No
Python runtime is required for native NSF-HiFiGAN inference in the installed editor. Keep the model
directory with the executable when distributing or moving the application.
The weights retain the upstream CC BY-NC-SA 4.0 license.

The model is discovered at `models/nsf_hifigan/pc_nsf_hifigan.onnx` beside the
executable, together with `config.json`. Alternatively, the algorithm settings
accept a model directory or an explicit ONNX file path with a sibling
`config.json`; an explicit filename does not have to use the canonical name.

The renderer inspects ONNX input metadata to accept the official
`[batch, frames, mel_bands]` layout as well as legacy custom exports using
`[batch, mel_bands, frames]`. Incompatible Mel dimensions produce a render
error instead of feeding a tensor with the wrong dimensions.

Verification with a supplied model:

```powershell
$env:HACHISHIFTER_NSF_HIFIGAN_MODEL_DIR = 'APP/models/nsf_hifigan'
& 'APP/HachiShifter Next.exe' --smoke-nsf-picker
python juce/tests/nsf_hifigan_model_smoke.py 'APP/HachiShifter Next.exe' 'APP/models/nsf_hifigan'
```

The picker diagnostic checks WORLD → NSF → LLSM2 → NSF on a selected track,
including refresh and isolation from other tracks. The model smoke test checks
real neural rendering and WAV export through the editor, including its two
Mel stretching orders and merged clips.

## UTAU, Jie and Mou voicebank synthesis

Use the ordinary `UTAU`, `界·UTAU` or `谋·UTAU` mode and bind a UTAU
voicebank. Right-click the track or its region and choose the output engine
`HiFisampler (PC-NSF-HiFiGAN)`. Tracks without an override follow the output
engine selected in the application settings. The former separate PC-NSF
Jie/Mou algorithm entries are now part of these ordinary modes; existing
projects retain their backend selection when loaded. See
[output engine selection](utau-output-engine.md).

The Jie path reads independent `oto.jie.ini` timing when present and its
`oto4.ini` region boundaries. The Mou path also reads `otomou.ini`, including
two/three/four-region C/V/S classes. Subdirectories, note-local OTO and STP use
the same loader as the UTAU backend.

Each source region maps independently to its allocated output interval before
neural inference. Dragged region boundaries and consonant velocity affect that
mapping; first-region timing stays tied to preutterance. Zero-length Jie spelling
notes read only the first two regions. The region display uses the same allocation
and the audible crossfade handover. Missing region annotations retain ordinary
OTO timing. Sustained regions stretch once by default; `He` explicitly opts
into the upstream looping behavior.

The built-in renderer supports the upstream HiFisampler flags `g`, `Hb`, `Hv`,
`Ht`, `HG`, `P`, `t`, `A`, `G` and `He`, including time-varying flag automation.
See [flag values, dependencies and compatibility](hifisampler-flags.md).
Independent harmonic/noise control and tone shift require the optional
`models/nsf_hifigan/hnsep/model.onnx` model described there. This native renderer does not
start a Python render service; external WCSNDM/HF resamplers may require their
own Python environment. Services started by the editor are reclaimed on exit;
a pre-existing external service remains owned by its original caller.
