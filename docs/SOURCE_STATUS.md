# Rolling development source status

Date: 2026-11-02. Public label **dev**, internal numeric tuple **0.0.0**, WCSNDM **0.0803**. This snapshot contains the approved implementation through update207 and follows commit905c13267adb1eb5ad17a0fdd330914870d54223 on `dev`.

## Source and authoring scope

The existing flat root layout is retained: CMakeLists.txt, src, resources, assets, third_party, engines (bridge SOURCE and dictionaries only), tools and docs. No README is supplied; the repository owner maintains it separately. Business source files are copied byte-for-byte from the current compiled working tree; only build/packaging/tool/document paths are adapted to the flattened layout.

This includes the independent statically linked MLD5 DSP and native source-clock adapter, the real vowel false-UV coverage fix, evidence-gated legacy source-pitch recovery, and the same-stream touching-join dropout fix. The user listened to and approved the two-te repair. Fresh MPD source support, actual output F0, analytic signal oracles, same-stream join invariance, explicit author-fade preservation, UI/defaults and retained Japanese/outer-trim regressions were tested locally. Installed output was whole-file SHA-identical to the approved render. These results do not assert full Melodyne equivalence, every possible source/voicebank, or newly tested ASR accuracy.

Original source audio, project edits, source clocks and user settings are not distributed. Independent same-file layers are not merged merely by filename; uncertain/nonstationary legacy voicing is not blindly filled. Cross-source virtual assembly and source separation retain their documented capability limits.

## Lightweight release policy

This publication includes source and an already-built lightweight Windows update package, not model weights or a DiffSinger/Python runtime. Optional English G2P `checkpoint20.npz` is excluded before copying. Native DLL dependencies, external lang/translations.tsv and notices accompany the executable. Preserve an existing installation's models/engines when applying the program update. Neural features require separately supplied compatible models; DiffSinger requires its separately installed runtime/voicebank. No large runtime or resource archive is copied into this new checkout.

The cloud workflow now produces the same lightweight app by default. Its historical full-resource route remains an explicit manual opt-in (`include_models`); push builds do not download models or portable Python. Existing checksum-pinned resource metadata remains unchanged, not republished as new weights.

The application/root MIT license is not a blanket license for all independent dependencies. Existing license and provenance notices are retained. No original Melodyne DLL, private reference audio, IDA dump, or analysis model is required by the independent MLD5 production library.

## Publication identity

Author AND committer: `funnymdzz <funnymdzz@gmail.com>`, per the owner's explicit request. Only `dev` is updated, with an ordinary fast-forward push. No force rewriting or modification of main/r or earlier version releases is intended. Binary/source archives are release assets, never model or executable Git blobs.

See [update207](updates.md), [architecture](AI_INTERFACE_ARCHITECTURE.md) and [cloud packaging](cloud-build.md). The cloud run is independent evidence: report its actual status, not an assumed success.
