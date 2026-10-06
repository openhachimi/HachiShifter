#!/usr/bin/env bash
# The regression suite: every --smoke- check in Main.cpp, with the fixtures it
# needs.  Kept in the repository -- the previous copy lived in %TEMP% and
# Windows' Storage Sense deleted it along with its fixtures.
#
# Driven from bash, not PowerShell: every Start-Process variant hangs on this
# GUI exe (-NoNewWindow, -RedirectStandardOutput, and plain, all of them),
# while the very same command from bash returns in under a second.  Each case
# gets its own timeout so a wedged one is reported rather than stalling the
# run, and its own log so nothing is read while the next case writes it.
#
# usage: tools/regress.sh [limit] [timeout_seconds]

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"
EXE="$ROOT/build-win/HachiShifterNext_artefacts/RelWithDebInfo/HachiShifter Next.exe"
FIX="$HERE/fixtures"
OUT="$FIX/out"
LOGS="$FIX/logs"

PROJ="$FIX/big100.hjpx"
WIDE="$FIX/wide.hjpx"
PACKAGE="$FIX/package/HachiShifter Next.exe"
USTENC="$FIX/ust-encoding"
MOUBANK="$FIX/moubank"
BANK="F:/UTAU/voice/New Geping UTAU Database"
CVVCBANK="F:/UTAU/voice/东方栀子_Era_CVVCHN"
RESAMPLER="D:/人声合成论文/utau化/build/WCSNDM.exe"
PYTHONW="D:/anaconda/pythonw.exe"
WAV="$BANK/a-c.wav"
# Checks that read a path without unquoting it need one with no spaces in it.
SAMPLE="$FIX/sample.wav"
UST1="F:/UTAU/voice/Hikigaya Hachiman/雪融2.ust"
UST2="F:/UTAU/voice/dsm/猫中毒.ust"
UST3="F:/UTAU/voice/凄美地（1）改1.ust"

LIMIT="${1:-0}"
LIMIT_SEC="${2:-120}"

# Two suites at once wedge each other, so never start beside one.
powershell -NoProfile -Command "Get-Process -ErrorAction SilentlyContinue | Where-Object { \$_.Name -like 'HachiShifter*' } | ForEach-Object { \$_.Kill() }" >/dev/null 2>&1

if [ ! -f "$EXE" ]; then echo "build the editor first: $EXE"; exit 1; fi
if [ ! -f "$PROJ" ] || [ ! -d "$USTENC" ]; then
  echo "fixtures missing; rebuilding"
  PYTHONIOENCODING=utf-8 python "$HERE/make_fixtures.py" || { echo "could not rebuild the fixtures"; exit 1; }
fi
mkdir -p "$LOGS" "$OUT"

cases=(
"--smoke-oto-playback"
"--smoke-oto-editor	$BANK	a"
"--smoke-oto-editor	$BANK	a	jie"
"--smoke-oto-editor	$MOUBANK	si	mou"
"--smoke-splice-envelope	$PROJ"
"--smoke-dialog-enter"
"--smoke-source-edit-view	$PROJ"
"--smoke-consonant-hold"
"--smoke-mou-panel-counts	$MOUBANK	$OUT/mou-panel.png"
"--smoke-mou-consonant-ticks	$MOUBANK	si"
"--smoke-mou-boundary-drag	$MOUBANK	si"
"--smoke-prefix-map	$CVVCBANK	a"
"--smoke-mou-oto-file	$MOUBANK"
"--smoke-utau-mode	$PROJ"
"--smoke-lane-marquee	$PROJ"
"--smoke-flag-lane	$PROJ	$OUT/flag-lane.png"
"--smoke-keyboard-labels	$OUT/keys.png"
"--smoke-lane-labels"
"--smoke-export-targets"
"--smoke-audition-position	$WAV"
"--smoke-lyric-envelope	$PROJ	$BANK	ban"
"--smoke-lyric-timing	$PROJ	$BANK	ba	ban"
"--smoke-vibrato-bake	$PROJ"
"--smoke-mou-note-regions	$WIDE	$MOUBANK	si"
"--smoke-region-guides	$PROJ"
"--smoke-rest-playback	$PROJ	$BANK	$RESAMPLER	a"
"--smoke-rest-lyric	$PROJ	$BANK	$OUT/rest-lyric.png"
"--smoke-flag-curve-mode	$RESAMPLER"
"--smoke-flag-point-value	$PROJ"
"--smoke-flag-overlap	$PROJ	$BANK"
"--smoke-load-timing	$PROJ"
"--smoke-flatten-pitch-line"
"--smoke-note-across-tracks"
"--smoke-clip-across-tracks"
"--smoke-reference-track"
"--smoke-native-pitch-points"
"--smoke-paste-at-pointer"
"--smoke-note-menu-split"
"--smoke-lyric-gate"
"--smoke-lyric-tab"
"--smoke-syllable-cuts"
"--smoke-single-syllable"
"--smoke-deleted-notes-silent"
"--smoke-delete-clip"
"--smoke-draw-unit"
"--smoke-new-track-draw"
"--smoke-draw-drag"
"--smoke-draw-overlap"
"--smoke-dropdown-arrow"
"--smoke-pitch-line"
"--smoke-view-menu"
"--smoke-batch-lyrics"
"--smoke-envelope-lanes"
"--smoke-wheel"
"--smoke-waveform-toggle"
"--smoke-utau-waveform	$BANK	$RESAMPLER	a"
"--smoke-note-waveform	$BANK	$RESAMPLER	si	a"
"--smoke-waveform-alignment	$BANK	$RESAMPLER"
"--smoke-shared-pitch-line"
"--smoke-hanzi-pinyin"
"--smoke-vibrato-end"
"--smoke-amplitude-waveform"
"--smoke-envelope-base"
"--smoke-voicebank-index	$BANK	$CVVCBANK"
"--smoke-ust-pitch	$UST1	$UST2"
"--smoke-hf-prewarm	$PYTHONW"
"--smoke-ust-vibrato	$UST1	$UST2	$UST3"
"--smoke-ust-import"
"--smoke-midi-export"
"--smoke-midi-track-import"
"--smoke-missing-alias-piano"
"--smoke-forward-bend"
"--smoke-dragged-transition"
"--smoke-timeline-pit"
"--smoke-mcp-schema"
"--smoke-mcp-roots"
"--smoke-ust-encoding	$USTENC"
"--smoke-ust	$UST3"
"--smoke-active-resampler"
"--smoke-bundled-resampler"
"--smoke-anchor-frequency"
"--smoke-note-oto	$RESAMPLER"
"--smoke-prefix-jie	$RESAMPLER"
"--smoke-prefix-note"
"--smoke-utau-overlap	$RESAMPLER"
"--smoke-note-stp"
"--smoke-track-toggle-tips"
"--smoke-track-area-menu"
"--smoke-stretch-items"
"--smoke-render-order"
"--smoke-llsm2-length"
"--smoke-glide-seam"
"--smoke-gap-menu	$PROJ"
"--smoke-insert-gap	$PROJ"
"--smoke-ripple-delete	$PROJ"
"--smoke-paste-notes	$PROJ"
"--smoke-consonant-edge	$PROJ"
"--smoke-consonant-reset	$PROJ"
"--smoke-region-split	$BANK	jie"
"--smoke-region-split	$BANK	xiang"
"--smoke-region-split	$BANK	- a"
"--smoke-vibrato-presets"
"--smoke-follow-rule"
"--smoke-play-until	$PROJ"
"--smoke-flags-field	$PROJ	g-5Mt50BRE20P86b40Y10H20"
"--smoke-vertical-drag	$PROJ"
"--smoke-scroll-sync	$PROJ"
"--smoke-edit-follow	$PROJ"
"--smoke-note-drag-threshold	$PROJ"
"--smoke-roll-drag	$PROJ"
"--smoke-playhead-band	$PROJ"
"--smoke-roll-cull	$PROJ"
"--smoke-roll-timing	$PROJ	5"
"--smoke-envelope-shapes	$RESAMPLER"
"--smoke-envelope-presets	$OUT/envelope.txt"
"--smoke-piano-roll	$PROJ	20	40	80"
"--smoke-export	$SAMPLE	$OUT/export.wav"
"--smoke-mld5	$SAMPLE	2	1.0	$OUT/mld5.wav"
"--smoke-utau-selection	$PROJ	$RESAMPLER"
"--smoke-utau-voicebank	$BANK	$RESAMPLER	a	a"
"--smoke-utau	$FIX/out-utau.wav"
)

failed=0
timedout=""
index=0
for case in "${cases[@]}"; do
  index=$((index + 1))
  [ "$LIMIT" -gt 0 ] && [ "$index" -gt "$LIMIT" ] && break
  IFS=$'\t' read -r -a argv <<< "$case"
  name="${argv[0]}"
  log=$(printf "%s/%02d.log" "$LOGS" "$index")
  # A short tag for the cases that vary by alias rather than by project.
  tag=""
  if [ "${#argv[@]}" -gt 1 ] && [[ "${argv[1]}" != */* ]]; then tag="${argv[1]}"; fi
  # The 谋 checks write to the bank they are handed -- they annotate entries
  # and drag boundaries, which is what they are about -- so each one gets its
  # own copy of it.  Sharing one made the answers depend on the order the
  # cases ran in, and a rerun of a single case disagree with the suite.
  for slot in "${!argv[@]}"; do
    if [ "${argv[$slot]}" = "$MOUBANK" ]; then
      rm -rf "$OUT/bank$index"
      cp -r "$MOUBANK" "$OUT/bank$index"
      argv[$slot]="$OUT/bank$index"
    fi
  done
  start=$SECONDS
  # The packaging question is asked of the package: what an installed copy
  # finds beside itself, not what the build folder happens to hold.
  run="$EXE"
  [ "$name" = "--smoke-active-resampler" ] && run="$PACKAGE"
  timeout "$LIMIT_SEC" "$run" "${argv[@]}" > "$log" 2>&1 < /dev/null
  code=$?
  secs=$((SECONDS - start))
  out=$(tr -d '\r' < "$log" | tr '\n' ' ')
  if [ "$code" -eq 124 ]; then
    failed=$((failed + 1))
    timedout="$timedout $name"
    printf 'TIMEOUT %-28s %-6s after %ss -- partial: %s\n' "$name" "$tag" "$secs" "$out"
  elif [ "$code" -eq 0 ]; then
    printf 'ok   %-30s %-6s [%ss] %s\n' "$name" "$tag" "$secs" "$out"
  else
    failed=$((failed + 1))
    printf 'FAIL %-30s %-6s [%ss] %s  (exit %s)\n' "$name" "$tag" "$secs" "$out" "$code"
  fi
done
# The MCP interface, checked from outside the exe: one reads the C++ as well as
# the running server, so a parameter renamed on one side and not the other is
# caught; the other drives a real server over stdio, which is the only way to
# see a refusal actually reached from a call.
if [ "$LIMIT" -eq 0 ]; then
  for lint in check_mcp_schemas.py check_mcp_roots.py; do
    log="$LOGS/${lint%.py}.log"
    start=$SECONDS
    if PYTHONIOENCODING=utf-8 python "$HERE/$lint" > "$log" 2>&1; then
      printf 'ok   %-30s %-6s [%ss] %s
' "$lint" "" "$((SECONDS - start))"         "$(tr -d '' < "$log" | head -1)"
    else
      failed=$((failed + 1))
      printf 'FAIL %-30s %-6s [%ss] %s
' "$lint" "" "$((SECONDS - start))"         "$(tr -d '' < "$log" | tr '
' ' ' | cut -c1-200)"
    fi
  done
fi

echo "---"
[ -n "$timedout" ] && echo "timed out (${LIMIT_SEC}s each):$timedout"
if [ "$failed" -eq 0 ]; then echo "all green"; else echo "$failed failed"; exit 1; fi
