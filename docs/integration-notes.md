# UTAU / Melodyne Integration Notes

Notes on integrating the UTAU-specific feature set and Melodyne-compatible
editing into the unified native version. This is a source integration record;
comments and bundled smoke tests do not by themselves establish passing tests.

## Scope reviewed before integration

The UTAU-specific version contributes:

- Classic / extended-region / 4-region modes, UST parsing, an external resampler
  and an internal wavtool-style mixer, voicebank selection, and OTO / region
  editors.
- Sparse pitch anchors with segment shapes and Bezier handles, amplitude and
  flag curves, vibrato parameters / presets / baking, note context menus, view
  toggles, selection playback, reference tracks, and tempo changes.
- Project persistence for those fields, per-note render caching, rendered UTAU
  waveform snapshots, region / track export, and command-line smoke checks.

NSF phrase scheduling is refactored into contiguous-group / merged-request
helpers; the NSF model implementation itself is unchanged.

UTAU edits are score / voicebank oriented: insertion and merge can ripple
subsequent notes, lyrics select a sample, and negative pitch-anchor times
address preutterance. These are not the rules for editing an existing recording,
so they are kept as behaviors of the unified model rather than a separate editor.

## Melodyne compatibility requirements

1. Preserve imported clip start, duration, selected source range, and time map.
   Never extend a source selection to the end of its file to fill silence.
2. Preserve each note's pitch centre, source centre, local contour, and
   expression. A connection is not permission to concatenate incompatible
   relative contours.
3. UTAU ripple editing and voicebank / lyric substitution stay on UTAU tracks.
4. Pitch-point editing must retain measured source F0; source-edit mode must keep
   using source coordinates instead of project coordinates.
5. Preserve the overlap-smoothing default (off) and stereo 24-bit WAV export.
6. Continuous phrase slices must finish on error as well as success; an
   unreadable member must not leave orphaned pending slices.
7. Render-order controls must select a supported backend, never silently routing
   a non-NSF track to NSF.
8. A Melodyne project may represent a consonant with no clear pitch line as a
   full note immediately before its vowel. On import/convert, treat it as the
   following vowel's consonant candidate, merge it into the vowel's consonant
   region, and adjust the target preutterance instead of rendering it as an
   independent pitched vowel.
9. Melodyne consonant / vowel boundaries map to UTAU preutterance semantics, and
   Melodyne note amplitude decay may inform overlap gain / envelope. Melodyne
   does not expose fixed / stretch region boundaries, so they are not invented as
   imported facts; any stretch planning is a visible, editable conversion
   suggestion derived later from OTO and target duration.
10. The self-developed Melodyne importer and merged renderer continue in
    parallel, but incomplete reverse-engineered algorithms stay disabled by
    default and do not appear in normal user-facing algorithm pickers.
11. Melodyne-native import/render should prefer detecting and calling the user's
    installed, licensed Melodyne through a supported host/API path. No Melodyne
    DLLs or licence material are copied into this repository.

## UI integration status

- Pitch draw, line, and point tools are visible on both ordinary audio tracks and
  UTAU tracks.
- Note-menu vibrato items are shared between UTAU and ordinary audio tracks;
  UTAU-only items remain limited to timing, OTO/STP, region flags, lyric entry,
  and gap / ripple operations.
- Vibrato handles and real-line display are no longer gated on UTAU tracks.
- The experimental Melodyne algorithm remains loadable from project data for
  compatibility but is hidden from the toolbar and from Melodyne-import default
  algorithm settings until its render path is validated.
- Defaults: new / legacy-missing tracks use `smoothOverlaps = false`, and project
  WAV export writes stereo 24-bit audio.
- UTAU and Melodyne-compatible tracks share the same editor geometry: tools stay
  in a fixed common row and mode-specific fields occupy a fixed second row.
  Melodyne tracks show common expression controls there; UTAU tracks replace them
  with voicebank / alias / flag controls.
- Melodyne executable / VST3 discovery is discovery only; no DLL or licence is
  loaded. A provider layer gates the import button and merged-render scheduler,
  and both native-provider and experimental operations stay disabled until an
  actual supported host/API contract is implemented.
- First-time Melodyne import seeds the existing per-audio HJM sidecar through the
  shared save path: note range, pitch centres, drift, modulation, formant,
  amplitude, sibilance, and consonant/attack timing are stored there, and
  subsequent audio and UTAU workflows consume the same project annotation format
  rather than introducing a second annotation file.
- Sidecar seeding maps note-local target time back through the clip's source time
  map; a conservative adjacent pitchless-note rule folds such a note into the
  following voiced note's HJM region. The source Melodyne notes remain intact in
  the in-memory import result; this conversion affects only the first generated
  material annotation.
- VST3 probing goes through the plugin host boundary. It can report a Windows
  VST3 as discovered but not hostable off Windows; a Windows build can proceed to
  plugin description and instance creation. This does not claim ARA support or
  traditional MPD compatibility.

## Validation strategy

- Keep the NSF model regression tests and add the UTAU, UST, pitch-point, menu,
  waveform, envelope, and OTO checks.
- Test native and UTAU editing separately: overlap, resize, split, merge,
  source-F0 preservation, and save / reload.
- Check the source tree / build registration and syntax before a full
  application build. Build and test results and outstanding limits are recorded
  after verification, not inferred from file sizes or changed audio hashes.
