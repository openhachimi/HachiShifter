"""OpenVPI sampling units and load-time validation (no inference is run)."""
import math
from pathlib import Path
import numpy as np

TYPES = {'tensor(float)', 'tensor(int64)', 'tensor(bool)', 'tensor(double)'}
MEL_DEFAULTS = dict(sample_rate=44100, hop_size=512, win_size=2048, fft_size=2048,
                    num_mel_bins=128, mel_fmin=40, mel_fmax=16000, mel_scale='slaney')


def sampling_inputs(config, inputs, steps=20, depth=1.0):
    inputs = {i.name: i.type for i in inputs}
    result = {}
    steps = int(steps)
    if not 1 <= steps <= 1000:
        raise ValueError('采样步数必须在 1–1000 之间')
    if not math.isfinite(depth) or not 0 < depth <= 1:
        raise ValueError('采样深度必须大于 0 且不超过 1')
    if 'depth' in inputs:
        maximum = float(config.get('max_depth', 1.0))
        if not math.isfinite(maximum) or not 0 < maximum <= 1:
            raise ValueError('max_depth 必须大于 0 且不超过 1')
        depth = min(depth, maximum)
        if inputs['depth'] not in ('tensor(float)', 'tensor(double)', 'tensor(int64)'):
            raise ValueError('不支持的 depth 输入类型：' + inputs['depth'])
    else:
        depth = 1.0
    if 'steps' in inputs:
        result['steps'] = [steps]
    if 'speedup' in inputs:
        # Legacy diffusion uses an integer timeline of 1000, whereas YAML
        # max_depth is always a fraction. Keep integer depth divisible by stride.
        timeline = max(1, round(depth * 1000))
        speedup = max(1, timeline // steps)
        if 'depth' not in inputs:
            while 1000 % speedup:
                speedup -= 1
        result['speedup'] = [speedup]
    if 'depth' in inputs:
        if inputs['depth'] == 'tensor(int64)':
            value = max(1, round(depth * 1000))
            stride = result.get('speedup', [1])[0]
            result['depth'] = [value // stride * stride]
        else:
            result['depth'] = [depth]
    return result


def validate_vocoder_specs(config, vocoder):
    for label, cfg in [('声学模型', config), ('声码器', vocoder)]:
        for key in ('sample_rate', 'hop_size', 'win_size', 'fft_size', 'num_mel_bins'):
            value = cfg.get(key, MEL_DEFAULTS[key])
            if not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0 or int(value) != value:
                raise ValueError(f'{label} 的 {key} 必须是正整数')
        if str(cfg.get('mel_base', '10')) not in ('e', '10'):
            raise ValueError(f'{label} 的 mel_base 必须是 e 或 10')
        if cfg.get('mel_scale', 'slaney') not in ('slaney', 'htk'):
            raise ValueError(f'{label} 的 mel_scale 必须是 slaney 或 htk')
    for key, default in MEL_DEFAULTS.items():
        if config.get(key, default) != vocoder.get(key, default):
            raise ValueError(f'声学模型与声码器的 {key} 不匹配：{config.get(key, default)} / {vocoder.get(key, default)}')


def require_file(path, label):
    path = Path(path)
    if not path.is_file() or path.stat().st_size == 0:
        raise ValueError(f'{label} 缺失或为空：{path}')


def validate_session(session, allowed, label, config=None):
    for item in session.get_inputs():
        if item.name not in allowed:
            raise ValueError(f'{label} 有不支持的模型输入：{item.name}')
        if item.type not in TYPES:
            raise ValueError(f'{label} 有不支持的输入类型：{item.name} / {item.type}')
    if not session.get_outputs():
        raise ValueError(f'{label} 没有输出')
    if config is not None:
        sampling_inputs(config, session.get_inputs())


def preflight(bank):
    errors, warnings = [], []
    def check(label, fn):
        try:
            fn()
        except Exception as error:
            errors.append(f'{label}：{error}')

    def inventory(model):
        if not isinstance(model.tokens, dict) or not model.tokens:
            raise ValueError(f'音素表为空或格式错误：{model.root}')
        if any(not isinstance(p, str) or not isinstance(i, int) or i < 0 for p, i in model.tokens.items()):
            raise ValueError(f'音素表须为“音素: 非负整数”映射：{model.root}')
        if 'SP' not in model.tokens:
            raise ValueError(f'音素表缺少休止音素 SP：{model.root}')
        if model.config.get('use_lang_id') and (not isinstance(model.languages, dict) or not model.languages):
            raise ValueError(f'缺少有效语言 ID 映射：{model.root}')
        for speaker in model.config.get('speakers', []):
            path = model.root / (speaker + '.emb')
            require_file(path, '音色 embedding')
            values = np.fromfile(path, dtype='<f4')
            if values.size != model.config.get('hidden_size', 256) or not np.isfinite(values).all():
                raise ValueError(f'音色 embedding 长度或数值错误：{path}')
        if not math.isfinite(model.dt) or model.dt <= 0:
            raise ValueError(f'采样率或 hop_size 无效：{model.root}')

    def model_check(model, name):
        inventory(model)
        require_file(model.root / str(model.config.get(name, '')), name + ' 模型')
        extras = {'languages', 'spk_embed', 'steps', 'speedup', 'depth'}
        if name == 'acoustic':
            allowed = extras | {'tokens', 'durations', 'f0', 'gender', 'velocity', 'energy', 'breathiness', 'voicing', 'tension'}
        else:
            require_file(model.root / str(model.config.get('linguistic', '')), 'linguistic 模型')
            ling = model.session('linguistic')
            validate_session(ling, {'tokens', 'ph_dur', 'word_div', 'word_dur', 'languages'}, str(model.root / 'linguistic'))
            allowed = extras | {o.name for o in ling.get_outputs()} | {
                'ph_midi', 'ph_dur', 'note_midi', 'note_dur', 'note_rest', 'note_glide',
                'pitch', 'retake', 'expr', 'energy', 'breathiness', 'voicing', 'tension'}
        session = model.session(name)
        validate_session(session, allowed, str(model.root / name), model.config)
        names = {i.name for i in session.get_inputs()}
        if name == 'acoustic':
            features = {'gender':'use_key_shift_embed', 'velocity':'use_speed_embed',
                        **{k:f'use_{k}_embed' for k in ('energy','breathiness','voicing','tension')}}
            for feature, flag in features.items():
                if bool(model.config.get(flag)) != (feature in names):
                    raise ValueError(f'{flag} 与模型输入 {feature} 不一致')
        if 'languages' in names and not model.config.get('use_lang_id'):
            raise ValueError('模型需要 languages，但配置未启用 use_lang_id')
        if any(i.name == 'spk_embed' for i in session.get_inputs()) and not model.config.get('speakers'):
            raise ValueError(f'{name} 需要音色 embedding，但没有配置 speakers')

    check('声学模型', lambda: model_check(bank.main, 'acoustic'))
    available = {}
    for folder, name in [('dsdur', 'dur'), ('dspitch', 'pitch'), ('dsvariance', 'variance')]:
        def inspect_optional(folder=folder, name=name):
            location = bank.root / folder
            if location.is_dir() and not (location / 'dsconfig.yaml').is_file():
                raise ValueError(f'目录存在但缺少 dsconfig.yaml：{location}')
            model = bank.model(folder)
            available[folder] = model is not None
            if model is not None:
                model_check(model, name)
        check(folder, inspect_optional)
    if not available.get('dsdur'):
        warnings.append('没有时长预测模型：使用规则时长。')
    if not available.get('dspitch'):
        warnings.append('没有 pitch 预测模型：可使用已有 MIDI/pitch 合成，不能自动生成 pitch。')
    required = [k for k in ('energy', 'breathiness', 'voicing', 'tension') if bank.main.config.get(f'use_{k}_embed')]
    if required and not available.get('dsvariance'):
        errors.append('声学模型需要 ' + ', '.join(required) + '，但缺少 dsvariance 模型。')
    if required and available.get('dsvariance'):
        variance = bank.model('dsvariance')
        def variance_outputs():
            outputs = {o.name.removesuffix('_pred') for o in variance.session('variance').get_outputs()}
            missing = [k for k in required if k not in outputs]
            if missing: raise ValueError('没有输出声学模型需要的 ' + ', '.join(missing))
        check('variance 输出', variance_outputs)
    if not bank.dictionary:
        warnings.append(f'没有可用的 {bank.language} 歌词词典：请使用音素或 [音素 音素]。')
    else:
        invalid = []
        for lyric, phones in bank.dictionary.items():
            if isinstance(phones, str): phones = phones.split()
            if not isinstance(phones, list) or not phones or any(p not in bank.main.tokens for p in phones):
                invalid.append(lyric)
        if invalid:
            warnings.append(f'词典有 {len(invalid)} 个条目含不支持的音素，例如：' + '、'.join(invalid[:5]))
    def vocoder_check():
        folder, cfg = bank.vocoder_config()
        require_file(folder / cfg.get('model', 'model.onnx'), '声码器模型')
        validate_vocoder_specs(bank.main.config, cfg)
        validate_session(bank.vocoder_session(), {'mel', 'f0'}, str(folder))
    check('声码器', vocoder_check)
    return dict(ok=not errors, errors=errors, warnings=warnings,
                error='音源预检失败：\n'+'\n'.join(errors) if errors else '',
                has_pitch=available.get('dspitch', False), has_duration=available.get('dsdur', False))
