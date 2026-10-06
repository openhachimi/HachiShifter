#!/usr/bin/env bash
# Break tests: put a fault in, and see the check say so.
#
# A check that passes proves nothing on its own -- it has to fail when the
# thing it watches is broken.  This runs one fault at a time: apply it, build,
# run the check, expect a failure, put the source back.  The last thing it does
# is rebuild, because otherwise the exe left behind is the sabotaged one and
# whatever is run next -- the suite, a deploy -- is reading a lie.
#
# usage: tools/run_breaks.sh <break-script.py> -- <check> [args...]
#        tools/run_breaks.sh tools/breaks/ust_vibrato.py -- --smoke-ust-vibrato a.ust
#        tools/run_breaks.sh tools/breaks/mcp_roots_wiring.py -- python tools/check_mcp_roots.py

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$HERE")"
EXE="$ROOT/build-win/HachiShifterNext_artefacts/RelWithDebInfo/HachiShifter Next.exe"
LOGS="$HERE/fixtures/logs"

SCRIPT="${1:-}"
shift || true
[ "${1:-}" = "--" ] && shift
if [ -z "$SCRIPT" ] || [ "$#" -eq 0 ]; then
  sed -n '2,12p' "${BASH_SOURCE[0]}"
  exit 2
fi
mkdir -p "$LOGS"

build() {
  (cd "$ROOT" && powershell -NoProfile -ExecutionPolicy Bypass -File ./build-local.ps1) \
    > "$LOGS/break-build.log" 2>&1
  grep -q "\[build\] OK" "$LOGS/break-build.log"
}

run_check() {
  # A --smoke- case runs in the editor; anything else is a command of its own,
  # which is how the checks that drive a server over stdio are run.
  if [ "${1#--smoke}" != "$1" ]; then
    timeout 300 "$EXE" "$@" > "$LOGS/break-check.log" 2>&1
  else
    timeout 300 "$@" > "$LOGS/break-check.log" 2>&1
  fi
  return $?
}

# Windows line endings come back with it, and a case with one on its end
# matches nothing in the break script.
cases=$(python "$SCRIPT" list | tr -d "\r") || { echo "the break script cannot list its cases"; exit 1; }
python "$SCRIPT" check > /dev/null || { echo "the source is not in its whole state"; exit 1; }

echo "baseline: building"
build || { echo "the build failed before a single break"; exit 1; }
if ! run_check "$@"; then
  echo "the check does not pass on whole source -- nothing can be proved by breaking it"
  tail -3 "$LOGS/break-check.log"
  exit 1
fi
echo "baseline: the check passes"

uncaught=""
for case in $cases; do
  python "$SCRIPT" break "$case" > /dev/null || { echo "could not apply $case"; exit 1; }
  if build; then
    if run_check "$@"; then
      uncaught="$uncaught $case"
      printf 'UNCAUGHT %-22s the check still passed\n' "$case"
    else
      printf 'caught   %-22s %s\n' "$case" \
        "$(tr -d '\r' < "$LOGS/break-check.log" | tr '\n' ' ' | grep -o '[a-z_]*=0[^0-9]' | tr '\n' ' ' | cut -c1-90)"
    fi
  else
    printf 'BUILD    %-22s the break does not compile\n' "$case"
    uncaught="$uncaught $case(build)"
  fi
  python "$SCRIPT" restore "$case" > /dev/null || { echo "could not restore $case"; exit 1; }
done

python "$SCRIPT" check > /dev/null || { echo "the source did not come back whole"; exit 1; }
echo "restored: rebuilding"
build || { echo "the rebuild failed -- the exe is still a broken one"; exit 1; }
run_check "$@" && echo "the check passes again" || { echo "the check fails on restored source"; exit 1; }
echo "---"
if [ -n "$uncaught" ]; then echo "uncaught:$uncaught"; exit 1; else echo "every break was caught"; fi
