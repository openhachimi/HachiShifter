"""Run the self-contained smoke cases without touching a user's running app.

Optional arguments select specific --smoke-* entries; otherwise discover all
zero-argument entries in Main.cpp. Logs and settings stay in integration-tests.
"""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import sys
import time

root = Path(__file__).resolve().parent.parent
exe = root / 'build-integrated/HachiShifterNext_artefacts/RelWithDebInfo/HachiShifter Next.exe'
out = root / 'integration-tests'
out.mkdir(exist_ok=True)
env = dict(os.environ)
settings_run = out / 'settings-runs' / str(time.time_ns())
env['PYTHONIOENCODING'] = 'utf-8'
source = (root / 'juce/src/Main.cpp').read_text(encoding='utf-8')
cases = sys.argv[1:] or list(dict.fromkeys(re.findall(
    r'if \((?:arguments.size\(\) >= 1|!arguments.isEmpty\(\)) && arguments\[0\] == "(--smoke-[^"]+)"\)', source)))
results = []
# These fixtures distinguish source regions by their original tone frequency.
# A real pitch-shifting resampler deliberately makes both regions the same pitch,
# so run them through the built-in backend, with an isolated copy of the binary.
# All other cases (including active-resampler) use the fully bundled executable.
# Missing-model error-path checks must also run without the bundled NSF model.
internal_cases = {'--smoke-note-oto', '--smoke-note-stp',
                  '--smoke-nsf-utau-phrase', '--smoke-nsf-utau-synth'}
internal_exe = out / 'internal-backend' / exe.name
if internal_cases.intersection(cases):
    internal_exe.parent.mkdir(exist_ok=True)
    shutil.copy2(exe, internal_exe)
    for dll in exe.parent.glob('*.dll'):
        shutil.copy2(dll, internal_exe.parent / dll.name)
for case in cases:
    # A GUI smoke saves zoom/layout preferences on destruction. Isolate both
    # cases and runs so repeated wheel tests cannot inherit a zoom limit.
    env['HACHI_TEST_SETTINGS_DIR'] = str(settings_run / case.removeprefix('--'))
    start = time.monotonic()
    try:
        case_exe = internal_exe if case in internal_cases else exe
        run = subprocess.run([str(case_exe), case], stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, env=env, cwd=root, timeout=50,
            creationflags=subprocess.CREATE_NO_WINDOW)
        code, log = run.returncode, run.stdout
    except subprocess.TimeoutExpired as exc:
        code, log = 'timeout', exc.stdout or b''
    (out / (case.removeprefix('--') + '.log')).write_bytes(log)
    results.append({'case': case, 'code': code, 'seconds': round(time.monotonic()-start, 2),
                    'backend_fixture': 'internal' if case in internal_cases else 'bundled'})
    (out / 'results-latest.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print(case, code, flush=True)
print('PASS', sum(r['code'] == 0 for r in results), '/', len(results), flush=True)
sys.exit(0 if all(r['code'] == 0 for r in results) else 1)
