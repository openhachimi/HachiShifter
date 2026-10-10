# OpenUtau Chinese CVVC phonemizer

`juce/src/backend/ChineseCvvcPhonemizer.h` adapts the alias-selection and timing
rules from OpenUtau's `OpenUtau.Plugin.Builtin/ChineseCVVCPhonemizer.cs`:
https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Plugin.Builtin/ChineseCVVCPhonemizer.cs

Reviewed 2026-10-08. OpenUtau is distributed under the MIT license; see LICENSE.txt.
The editor uses a native C++ adapter, its own OTO lookup, time/pitch/envelope model,
and its existing single-character Mandarin pinyin table. It does not load OpenUtau
plugins or ship OpenUtau's language dictionaries. Context-sensitive polyphone
conversion and OpenUtau subbank voice-colour metadata are not implemented here.

Changes include strict alias lookup, explicit-alias bypass, missing-CV silence
with diagnostics, retained parent-note ownership, and shared classic/HiFisampler
rendering. Generated phonemes do not modify the authored score.
