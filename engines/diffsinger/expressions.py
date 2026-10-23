"""DiffSinger expression curves, in OpenUtau display units.

Times are seconds relative to each note. Curves of different expressions are
independent; omitted curves retain the model/default prediction. This module
does not interpret traditional UTAU resampler flags.
"""
import numpy as np

# code, label, minimum, maximum, neutral value
SPECS = (
    ("DYN", "响度（0.1 dB）", -240, 120, 0),
    ("GENC", "共振峰 / 性别", -100, 100, 0),
    ("VELC", "发音速度", 0, 200, 100),
    ("ENE", "能量偏移", -100, 100, 0),
    ("BREC", "气声偏移", -100, 100, 0),
    ("TENC", "张力偏移", -100, 100, 0),
    ("VOIC", "实声偏移（100 为零偏移）", 0, 100, 100),
    ("PEXP", "音高表现（生成 pitch 时）", 0, 100, 100),
    ("SHFC", "音区偏移（音分）", -1200, 1200, 0),
)
VARIANCE = {"ENE": "energy", "BREC": "breathiness", "TENC": "tension", "VOIC": "voicing"}


def sample_absolute(notes, times, supported, onsets):
    """NaN marks frames still using live prediction; stored bases are independent of FLAG enable."""
    result = {}
    for i, note in enumerate(notes):
        left = onsets[i] if i else -np.inf
        right = onsets[i+1] if i+1 < len(notes) else np.inf
        mask = (times >= left) & (times < right)
        for code, points in note.get('parameters', {}).items():
            if code not in VARIANCE or 'DS:'+code not in supported:
                raise ValueError(f'当前音源不支持实参 {code}，请先清除该实参或恢复原音源')
            p = np.asarray(points, np.float64)
            if p.ndim != 2 or p.shape[1] != 2 or not len(p) or not np.isfinite(p).all() or np.any(np.diff(p[:,0]) <= 0):
                raise ValueError(f'{code} 实参包含无效标点')
            lo, hi = (-10, 10) if code == 'TENC' else (-96, 0)
            values = result.setdefault('DS:ABS:'+code, np.full(len(times), np.nan, np.float32))
            values[mask] = np.interp(times[mask]-note['start'], p[:,0], np.clip(p[:,1], lo, hi))
    return result


def specifications(speakers):
    return {"DS:" + code: dict(code=code, label=label, minimum=lo, maximum=hi, default=default)
            for code, label, lo, hi, default in SPECS} | {
        "DS:CLR:" + speaker: dict(code="CLR", label="音色 · " + speaker.split("/")[-1],
                                  minimum=0, maximum=100, default=0)
        for speaker in speakers}


def capabilities(bank):
    cfg = bank.main.config
    pitch = bank.model("dspitch")
    variance = bank.model("dsvariance")
    _, vocoder = bank.vocoder_config(required=False)
    result = []
    for key, spec in specifications(cfg.get("speakers", [])).items():
        code = spec["code"]
        supported, reason = True, ""
        if code == "GENC":
            supported = bool(cfg.get("use_key_shift_embed"))
        elif code == "VELC":
            supported = bool(cfg.get("use_speed_embed"))
        elif code in VARIANCE:
            feature = VARIANCE[code]
            supported = bool(cfg.get(f"use_{feature}_embed") and variance
                and variance.config.get(f"predict_{feature}", feature in ("energy", "breathiness")))
        elif code == "PEXP":
            supported = bool(pitch and pitch.config.get("use_expr"))
        elif code == "SHFC":
            supported = bool(vocoder.get("pitch_controllable", False))
        elif code == "CLR":
            supported = len(cfg.get("speakers", [])) > 1
        if not supported:
            reason = ("当前声码器不支持" if code == "SHFC" else
                      "音源只有一个音色" if code == "CLR" else "当前音源不支持")
        result.append(dict(key=key, **spec, supported=supported, reason=reason))
    return result


def sample(notes, times, specs, supported, onsets=None):
    """No carry-over from a curve into the next note's neutral settings.

    A note's first/last value holds outside its handles, through its tail up
    to the next note. Phrase padding uses the first/last note, like pitch.
    """
    onsets = onsets if onsets is not None else [n["start"] for n in notes]
    arrays = {key: np.full(len(times), spec["default"], np.float32) for key, spec in specs.items()}
    for i, note in enumerate(notes):
        curves = note.get("expressions", {})
        if not isinstance(curves, dict):
            raise ValueError("DiffSinger 参数曲线必须是对象")
        left = onsets[i] if i else -np.inf
        right = onsets[i+1] if i+1 < len(notes) else np.inf
        mask = (times >= left) & (times < right)
        for key, points in curves.items():
            if key not in specs:
                raise ValueError(f"音源不支持该 DiffSinger 参数/音色：{key}")
            if not points:
                continue
            p = np.asarray(points, np.float64)
            if p.ndim != 2 or p.shape[1] != 2 or not np.isfinite(p).all():
                raise ValueError(f"{key} 曲线包含无效标点")
            if np.any(np.diff(p[:, 0]) <= 0):
                raise ValueError(f"{key} 曲线时间必须递增")
            spec = specs[key]
            values = np.clip(p[:, 1], spec["minimum"], spec["maximum"])
            if key not in supported and np.any(np.abs(values-spec["default"]) > 1e-5):
                raise ValueError(f"当前音源/声码器不支持 {key}；请重置或关闭该曲线")
            arrays[key][mask] = np.interp(times[mask]-note["start"], p[:, 0], values)
    return arrays


def gender(values, config):
    limits = config.get("augmentation_args", {}).get("random_pitch_shifting", {}).get("range", [-12, 12])
    low, high = map(float, limits)
    return np.where(values < 0, -values * (12/high/100 if high else 0),
                    -values * (-12/low/100 if low else 0)).astype(np.float32)


def variance_delta(name, predicted, values):
    if name == "tension":
        return np.clip(predicted + values/20, -10, 10)
    offset = values-100 if name == "voicing" else values
    return np.clip(predicted + offset*12/100, -96, 0)


def speaker_weights(curves, speakers, default_speaker):
    if not speakers:
        return None
    n = len(curves["DS:DYN"])
    weights = np.stack([curves.get("DS:CLR:"+s, np.zeros(n, np.float32))/100 for s in speakers], axis=-1)
    total = weights.sum(axis=-1)
    weights /= np.maximum(1, total[:, None])
    default = speakers.index(default_speaker) if default_speaker in speakers else 0
    weights[:, default] += np.maximum(0, 1-total)
    return weights
