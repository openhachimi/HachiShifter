"""One-bank worker, bounded phrase caches, atomic local file IPC."""
from collections import OrderedDict
from pathlib import Path
import gc
import hashlib
import json
import os
import time
import inference


class Cache:
    def __init__(self, limit=64*1024*1024, entries=32):
        self.limit, self.entries, self.size = limit, entries, 0
        self.items = OrderedDict()
        self.hits = 0

    def get(self, key):
        found = self.items.pop(key, None)
        if found is None: return None
        self.items[key] = found
        self.hits += 1
        return found[0]

    def put(self, key, value, size):
        if size > self.limit: return
        previous = self.items.pop(key, None)
        if previous: self.size -= previous[1]
        while self.items and (self.size + size > self.limit or len(self.items) >= self.entries):
            _, (_, removed) = self.items.popitem(last=False)
            self.size -= removed
        self.items[key] = (value, size)
        self.size += size


def audio_key(plan, pitch, curves):
    key = hashlib.sha256()
    key.update(json.dumps([plan['phones'], plan['ph_dur'], plan['word_div'], plan['word_dur']], separators=(',', ':')).encode())
    key.update(pitch.tobytes())
    for name, value in sorted((curves or {}).items()):
        if name in ('DS:DYN', 'DS:PEXP'): continue
        key.update(name.encode()); key.update(value.tobytes())
    return key.digest()


def fingerprint(request):
    paths = {Path(request['voicebank']).resolve()}
    if request.get('vocoders'): paths.add(Path(request['vocoders']).resolve())
    records = []
    for root in sorted(paths):
        if root.is_dir():
            for path in sorted(root.rglob('*')):
                if path.is_file() and path.suffix.lower() in ('.yaml', '.yml', '.json', '.txt', '.onnx', '.emb', '.data', '.bin'):
                    stat = path.stat()
                    records.append((str(path), stat.st_size, stat.st_mtime_ns))
    execution = inference.options(request.get('inference'))
    return (execution['backend'], execution['device'], str(Path(request['voicebank']).resolve()), request.get('language', 'zh'),
            request.get('speaker', ''), str(request.get('vocoders', '')), tuple(records))


class Runtime:
    def __init__(self, factory):
        self.factory, self.bank, self.key = factory, None, None
        self.loads = 0

    def release(self):
        self.bank = None; self.key = None
        gc.collect()

    def prepare(self, request):
        key = fingerprint(request)
        reused = self.bank is not None and key == self.key
        if not reused:
            self.release()
            args = (request['voicebank'], request.get('language', 'zh'), request.get('speaker', ''), request.get('vocoders'))
            self.bank = self.factory(*args, settings=request['inference']) if 'inference' in request else self.factory(*args)
            self.key = key
            self.loads += 1
        return self.bank, reused


def atomic_json(path, data):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(data, ensure_ascii=False, allow_nan=False), encoding='utf-8')
    os.replace(temp, path)


def parent_alive(pid):
    if os.name == 'nt':
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.OpenProcess(0x00100000, False, pid)
        if not handle: return False
        try: return kernel.WaitForSingleObject(handle, 0) == 258
        finally: kernel.CloseHandle(handle)
    try: os.kill(pid, 0); return True
    except ProcessLookupError: return False


def serve(folder, parent, factory, execute):
    folder = Path(folder)
    runtime = Runtime(factory)
    idle = time.monotonic()
    # Tests may shorten the release interval; the normal editor uses 120 s.
    idle_seconds = max(1., float(os.environ.get('HACHI_DS_IDLE_SECONDS', '120')))
    atomic_json(folder/'ready.json', {'ok': True, 'pid': os.getpid()})
    while folder.is_dir() and parent_alive(parent):
        input_path = folder/'request.json'
        if not input_path.is_file():
            if runtime.bank is not None and time.monotonic()-idle >= idle_seconds:
                runtime.release()
            time.sleep(.02)
            continue
        request = None
        started = time.monotonic()
        try:
            request = json.loads(input_path.read_text(encoding='utf-8-sig'))
            input_path.unlink()
            bank, reused = runtime.prepare(request)
            result = execute(request, folder/'result.json', bank)
            result['runtime'] = dict(pid=os.getpid(), bank_reused=reused, bank_loads=runtime.loads,
                seconds=time.monotonic()-started, audio_cache_hits=bank.audio_cache.hits,
                audio_cache_bytes=bank.audio_cache.size, plan_cache_hits=bank.plan_cache.hits)
        except Exception as error:
            input_path.unlink(missing_ok=True)
            result = dict(ok=False, error=f'{type(error).__name__}: {error}')
        result['request_id'] = request.get('request_id', '') if isinstance(request, dict) else ''
        atomic_json(folder/'result.json', result)
        bank = None  # Allow idle unloading to release the last bank reference.
        idle = time.monotonic()
