#!/usr/bin/env python3
"""Prepare the verified OpenVPI 2025.02 ONNX release for the native renderer.

Usage: python tools/prepare-nsf-model.py release.oudep APP/models/nsf_hifigan
Download the .oudep asset from SOURCE below. No Python inference runtime is
required by the installed application. Model weights are copied unmodified.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

SOURCE = "https://github.com/openvpi/vocoders/releases/tag/pc-nsf-hifigan-44.1k-hop512-128bin-2025.02"
ARCHIVE_SHA256 = "ba7d43142d41f6900c8264b5662ca7125a50feb8760bb8b9615c61a8f5e6902e"
MODEL = "pc_nsf_hifigan_44.1k_hop512_128bin_2025.02.onnx"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    digest = hashlib.sha256(args.archive.read_bytes()).hexdigest()
    if digest != ARCHIVE_SHA256:
        raise SystemExit("Archive SHA256 differs from the official 2025.02 ONNX release")
    with zipfile.ZipFile(args.archive) as archive:
        model = archive.read(MODEL)
        args.destination.mkdir(parents=True, exist_ok=True)
        (args.destination / "pc_nsf_hifigan.onnx").write_bytes(model)
        for name in ("NOTICE.txt", "NOTICE.zh-CN.txt", "STATEMENTS.txt", "vocoder.yaml"):
            (args.destination / name).write_bytes(archive.read(name))
    # Map the official vocoder.yaml parameters to the editor's JSON schema.
    config = dict(sampling_rate=44100, num_mels=128, hop_size=512,
                  n_fft=2048, win_size=2048, fmin=40, fmax=16000)
    (args.destination / "config.json").write_text(
        json.dumps(config, indent=2) + "\n", encoding="utf-8")
    manifest = dict(source=SOURCE, archive_sha256=digest, original_model=MODEL,
                    model_sha256=hashlib.sha256(model).hexdigest(),
                    weights_modified=False, mel_layout="batch,frames,mel_bands",
                    license="CC BY-NC-SA 4.0")
    (args.destination / "model-info.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest))


if __name__ == "__main__":
    main()
