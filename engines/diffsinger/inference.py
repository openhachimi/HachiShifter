"""Per-request sampling and serialized CPU/DirectML sessions.

DirectML requires sequential execution and disabled memory patterns. Auto may
retry an unsupported GPU model on CPU; explicit DirectML never silently falls
back. CPU nodes within a DirectML session remain allowed for unsupported ops.
"""
import math
import os
from pathlib import Path

PRESETS = {'fast': (8, 8, 8), 'standard': (20, 20, 20), 'high': (50, 40, 40)}


def options(value=None):
    value = value or {}
    backend = value.get('backend', 'cpu')
    if backend not in ('cpu', 'auto', 'directml'):
        raise ValueError('DS 推理设备必须为 auto、cpu 或 directml')
    device = value.get('device', 0)
    if isinstance(device, bool) or int(device) != device or not 0 <= device <= 31:
        raise ValueError('DS GPU 编号必须在 0–31 之间')
    quality = value.get('quality', 'standard')
    if quality not in (*PRESETS, 'custom'):
        raise ValueError('未知 DS 质量档位')
    defaults = PRESETS.get(quality, PRESETS['standard'])
    result = dict(backend=backend, device=int(device), quality=quality)
    for name, default in zip(('acoustic_steps', 'pitch_steps', 'variance_steps'), defaults):
        v = value.get(name, default) if quality == 'custom' else default
        if isinstance(v, bool) or not math.isfinite(float(v)) or int(v) != v or not 1 <= v <= 1000:
            raise ValueError('DS 采样步数必须是 1–1000 之间的整数')
        result[name] = int(v)
    depth = float(value.get('depth', 1.0)) if quality == 'custom' else 1.0
    if not math.isfinite(depth) or not 0 < depth <= 1:
        raise ValueError('DS 声学深度必须大于 0 且不超过 1')
    result['depth'] = depth
    return result


class Context:
    def __init__(self, value=None):
        self.options = options(value)
        self.sessions = []
        self.warnings = []
        self.sampling = {}

    def update(self, value):
        updated = options(value)
        if any(updated[k] != self.options[k] for k in ('backend', 'device')):
            raise ValueError('切换 DS 推理设备需要重新加载模型')
        self.options = updated
        self.sampling = {}

    def session(self, path, label):
        session = Session(self, path, label)
        self.sessions.append(session)
        return session

    def report(self):
        gpu = any('DmlExecutionProvider' in s.get_providers() for s in self.sessions)
        return dict(options=dict(self.options), backend='DirectML + CPU' if gpu else 'CPU',
                    sessions={s.label: s.get_providers() for s in self.sessions},
                    sampling=dict(self.sampling), warnings=list(self.warnings))


class Session:
    def __init__(self, context, path, label):
        self.context, self.path, self.label = context, str(path), label
        import onnxruntime as ort
        requested = context.options['backend']
        gpu = requested != 'cpu'
        if gpu and 'DmlExecutionProvider' not in ort.get_available_providers():
            if requested == 'directml':
                raise ValueError('DS DirectML 运行库未安装，请使用包含 GPU 运行库的完整配布目录，或切换 CPU / 自动。')
            self.warn('DirectML 不可用，已使用 CPU')
            gpu = False
        try:
            self.inner = self.create(gpu)
            if gpu and 'DmlExecutionProvider' not in self.inner.get_providers():
                raise RuntimeError('DirectML 未能启用；请检查 GPU 编号和显卡驱动')
        except Exception as error:
            if not gpu or requested != 'auto':
                raise RuntimeError(f'{label} 加载失败：{error}') from error
            self.warn(f'GPU 加载失败，已回退 CPU：{error}')
            self.inner = self.create(False)

    def warn(self, message):
        message = f'{self.label}：{message}'
        if message not in self.context.warnings:
            self.context.warnings.append(message)

    def create(self, gpu):
        import onnxruntime as ort
        opts = ort.SessionOptions()
        opts.intra_op_num_threads = 4
        opts.inter_op_num_threads = 1
        opts.log_severity_level = 3
        # Some DS graphs stall in ORT's generic optimizer. DML's own graph
        # compilation still takes place with generic optimizations disabled.
        opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_DISABLE_ALL
        opts.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        opts.enable_mem_pattern = not gpu
        profile = os.environ.get('HACHI_DS_PROFILE_DIR')
        if profile:
            Path(profile).mkdir(parents=True, exist_ok=True)
            opts.enable_profiling = True
            opts.profile_file_prefix = str(Path(profile) / self.label.replace('/', '_'))
        providers = [('DmlExecutionProvider', {'device_id': self.context.options['device']}), 'CPUExecutionProvider'] if gpu else ['CPUExecutionProvider']
        session = ort.InferenceSession(self.path, sess_options=opts, providers=providers)
        session.disable_fallback()
        return session

    def get_inputs(self): return self.inner.get_inputs()
    def get_outputs(self): return self.inner.get_outputs()
    def get_providers(self): return self.inner.get_providers()

    def run(self, output_names, feed):
        try:
            return self.inner.run(output_names, feed)
        except Exception as error:
            if self.context.options['backend'] != 'auto' or 'DmlExecutionProvider' not in self.get_providers():
                raise
            self.warn(f'GPU 执行失败，已回退 CPU：{error}')
            self.inner = self.create(False)
            return self.inner.run(output_names, feed)
