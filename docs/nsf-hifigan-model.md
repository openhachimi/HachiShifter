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
Python runtime is required when using the installed editor. Keep the model
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
