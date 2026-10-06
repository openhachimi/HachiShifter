# UTAU Feature Parity — Checklist

A static feature-parity checklist for the UTAU-specific feature set as it is
brought into the unified native version. It records which capabilities already
exist, which are migration/acceptance points, the correctness fixes required,
and the regression surface. Line references to source are intentionally omitted;
this is a behavioral checklist, not a source diff.

## 1. Existing capabilities (must not be reported as missing)

The unified version already provides: classic / extended (3-region) / 4-region
modes; timing and STP; library-level OTO waveform region editing and saving;
vibrato parameters, real-line display, and bake points; per-region flags and
reset; consonant reset; timing delete / insert gap; batch lyrics; pitch editing;
note split / merge; envelope presets; synthesized waveform display; UST import;
the external UTAU engine path; and `prefix.map`.

It additionally has native segment / native connection objects, versioned HJM,
and a Melodyne-provider layer that the UTAU-specific version does not — so a
whole-file replacement is not valid; capabilities are merged item by item.

## 2. Migration / acceptance points

| ID | Item | Requirement in the native layer |
| --- | --- | --- |
| U01 | Per-note OTO and "restore library OTO" | native local material override, stored in HJPX; only library-level editing writes OTO |
| U02 | OTO original-audio audition, stop, playhead | reuse the audio device; separate original audition from render audition; do not move the song position |
| U03 | Tab / Shift+Tab continuous lyric entry | same-track cross-clip order, focus, auto-scroll, single-edit undo |
| U04 | Hanzi → pinyin | apply a unified lyric command by selection/track without requiring a UTAU source; keep Latin / prefixes-suffixes |
| U05 | Add lead-in syllable | native lead-in pronunciation event / zero-duration semantics; not confused with the existing splice button |
| U06 | Envelope base value 0–200% | generic envelope parameter, implied envelope, multi-select, 0 = silent, serialized |
| U07 | Envelope presets and linear amplitude | keep old-project interpolation; add soft-attack / decay presets; implement standard / soft / fade shapes exactly |
| U08 | Envelope waveform background + live drag shaping | per-note real segment, preutterance alignment, envelope cache separate from timbre cache |
| U09 | Independent vibrato end point | an original enhancement (not one of the UST vibrato parameters); saved in HJPX; consistent when baked |
| U10 | Shared pitch line and valid-point range | integrate with native connection / unified curve evaluation; cross-note point drag affects real audio |
| U11 | Initialize a pitch line from a note | the unified pitch tool supports all sources; the initialize action keeps clear semantics |
| U12 | Extended flag-mode constraints | do not copy an editor-disabling gate; keep unified editing, do capability checks in the render protocol |
| U13 | Consonant handle / click threshold near syllable | a click with no movement must not clear a timing override; overlapping handles remain selectable |
| U14 | Voicebank background index | move to a material service; the UI never calls the renderer synchronously to read a whole library |
| U15 | UST replace-project / append-track with focus | new / append native project transactions, an explicit tempo policy, single undo |
| U16 | Single-track MIDI import and export | shared interchange: lyrics, time signature, tempo, and beat round-trip |
| U17 | UST cross-encoding and VBR | add an encoding fallback selector; do not treat heuristic detection as 100% accurate; keep the original parameters |
| U18 | UST pitch not altered by extra auto-transition | import as an explicit connection policy; after editing, do not branch by source |
| U19 | External-engine daemon prewarm | asynchronous and diagnosable per engine capability and configured interpreter; does not affect the native NSF backend |
| U20 | Index-file revision and alias-hash query | invalidate on OTO/prefix/extension and material commit; do not judge by whole-folder mtime |
| U21 | Extended parameter curves | store extended parameters in the native project; an adapter detects engine support |
| U22 | Waveform / envelope / syllable test entries | migrate the test intent into the unified model; do not copy old tests and keep a mode lock |

Other unrelated changes, including general view defaults, should not overwrite
the current native UI defaults without cause.

## 3. Correctness fixes and side-effect evidence

| ID | Problem | Fix direction |
| --- | --- | --- |
| D01 | Any HJM result shadows the whole OTO library | explicit material type/binding; do not preempt by directory presence |
| D02 | UTAU registration writes HJM | read-only registration by default; in-memory adaptation |
| D03 | Saving a classic OTO writes HJM again | library-level OTO transactions write OTO directly, no implicit HJM |
| D04 | HJM→OTO cutoff sign error | positive tail-trim or negative region length; verified round-trip |
| D05 | Single-audio OTO export replaces the whole file | export = new / merge / explicit replace; library editing uses row-level transactions |
| D06 | Native segments cause early return in lyric re-binding | reset local overrides on material-identity change, not by import type |
| D07 | Project lyric edits write the annotation file | separate project vs material commands and undo boundaries |
| D08 | Real Melodyne capability not connected | independent provider capability acceptance; do not substitute a self-developed algorithm for real Melodyne |

## 4. Regression surface

Important verification entry points for the migrated features (source presence
confirmed; behavior must be run and verified during implementation):

- `--smoke-oto-playback`, `--smoke-note-oto`
- `--smoke-lyric-tab`, `--smoke-hanzi-pinyin`
- `--smoke-prefix-jie`, `--smoke-prefix-note`
- `--smoke-envelope-base`, `--smoke-envelope-shapes`, `--smoke-amplitude-waveform`
- `--smoke-vibrato-end`, `--smoke-shared-pitch-line`
- `--smoke-waveform-alignment`, `--smoke-note-waveform`
- `--smoke-voicebank-index`, `--smoke-hf-prewarm`
- `--smoke-ust-pitch`, `--smoke-ust-vibrato`, `--smoke-ust-import`, `--smoke-ust-encoding`
- `--smoke-midi-track-import`, `--smoke-midi-export`
- `--smoke-flag-curve-mode` (accept at two levels: native editing available, and
  a valid engine protocol)

The existing `utau-mode / utau-waveform / utau-overlap / utau-selection /
utau-voicebank / utau` checks must keep passing and are not treated as new tests.
Also carry over the flag-priority, flag re-render, region-flags, rest, splice,
consonant-hold, envelope stretch/tail, region-count, and OTO-isolation tests as
the migration acceptance matrix.

Add during implementation: cross-source edit (UST/MPD/MIDI/native), full HJPX
field round-trip, repeated render-backend switching without data loss,
zero-HJM-write on register/render, library-edit vs local-override isolation, OTO
cutoff sign / encoding / alias round-trip, and no regression of existing native
segmentation / connection / NSF contiguous-group behavior.

## 5. Precise scope

- Pinyin conversion covers common single-character readings, lowercase without
  tone marks, `ü → v`; it has no polyphonic-context resolution and does not split
  a multi-character lyric into multiple notes.
- Per-note OTO editing opens on a single selection and needs to resolve an
  existing voicebank/entry; the native design allows keeping an override first,
  then prompting to complete the material binding.
- OTO audition plays the whole recording from the start; region / loop audition
  is a material-manager enhancement and is not reported as an existing feature.
- Neither an existing `PitchAlgorithm` value nor a VST3 instance-creation
  function is sufficient to claim real Celemony rendering is complete.
- Standard OTO cannot fully express all native segments, connections, and curves;
  format export must state the loss, while native HJPX still retains everything.
