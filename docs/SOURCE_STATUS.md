# Rolling development source status

Date: 2026-10-27. Public label: **dev**. Numeric CMake/Windows tuple: **0.0.0**. WCSNDM: **0.0803**.

This dev update continues `e2e9c7be829d23881eab15d82bbd94d49ed335a2` in the existing flat source layout. It synchronizes the current development implementation through update 204 rather than restoring the old nested `juce/` snapshot. The repository owner's independent README policy is unchanged.

## Included work

- Native material/OTTO editing and registration, source-aware note/clip operations and current UI work.
- Virtual connections: permanent seam edit authority, one displayed/request target F0 and NSF/WORLD source assembly on supported paths.
- Legacy HJPX source-use identity restoration using existing explicit physical connection evidence.
- Melodyne import option controls and source-time alignment from the real element `anchorPoint`, not sibilants or voiced onset.
- File-only five-language translation catalog (1509 rows), packaged alongside the executable.
- NSF-HiFiGAN and splice-before-synthesis processing-order defaults; saved explicit choices and the independent stretch-algorithm default are retained.
- Existing cloud Windows build and separate application-only/model-bundle artifacts, now with dev version checks and mandatory external translations.

## Validation scope

The current working-source dev executable was compiled locally; runtime version and Windows FileVersion/ProductVersion report `dev`. Track-type and engine-view CI gates plus focused defaults, import options, connected editing, single F0, shared pitch and translation-file/no-file regressions passed. Prior update 203 additionally verified real MPD anchor pixel/unit/request equality and bit-exact complete WORLD/NSF audio outputs. Those reports are historical local evidence, not a claim that every source feature or third-party model was retested for this version-only publication.

The exact flattened source is configured/built separately before publication. The GitHub Actions run triggered by the push is an independent cloud result; see its actual status rather than treating these source notes as a successful cloud package report.

## Known boundaries

Virtual source assembly currently rejects unsupported WCS/HiFiShifter/robust-pitch/separated-component/voicebank combinations rather than silently reverting to per-source pitch transformation. User data, model provenance/licensing and compatible external engines remain independent concerns. See the [update history](updates.md), [architecture](AI_INTERFACE_ARCHITECTURE.md) and [cloud packaging guide](cloud-build.md).

## Distribution and provenance

Only source, required scripts/dictionaries, documentation, notices and necessary artwork are Git content. No user projects/media/settings, build caches, executables, DLLs or model weights are added. The old tracked optional English G2P checkpoint is removed from this source revision; the existing approved resource bundle still supplies it for the with-models package. Git history is not rewritten. Model resource URL/hash and upstream licenses are preserved.

This commit is authored and committed as `funnymdzz <funnymdzz@gmail.com>` under the owner's explicit request. Only `dev` is updated; no force push or main/r modification is used.
