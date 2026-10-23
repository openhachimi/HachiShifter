"""Variance retake with raw (pre-FLAG) context and bounded in-memory history."""
import hashlib
import json
import numpy as np
import expressions
import runtime

ORDER = ('energy', 'breathiness', 'voicing', 'tension')


def selection_mask(notes, plan, times, selected):
    mask = np.zeros(len(times), bool)
    for i, note in enumerate(notes):
        if str(note['id']) not in selected:
            continue
        left = plan['note_onsets'][i]
        right = plan['note_onsets'][i+1] if i+1 < len(notes) else note['start']+note['duration']
        mask |= (times >= left) & (times < right)
    if not mask.any():
        raise ValueError('所选音符的发声范围短于一个预测帧，无法局部重算实参')
    return mask


def structural_key(bank, model, plan, channels):
    data = {k: plan.get(k) for k in ('phones', 'ph_dur', 'word_div', 'word_dur',
        'origin', 'total', 'parameter_context')}
    data.update(channels=channels, steps=bank.execution.options['variance_steps'])
    return hashlib.sha256(json.dumps(data, sort_keys=True, default=lambda x: x.tolist()).encode()).digest()


def speaker_condition(model, curves, n):
    speakers = model.config.get('speakers', [])
    if not speakers:
        return np.zeros((n, 0), np.float32)
    speaker = getattr(model, 'speaker', '')
    if curves is None:
        result = np.zeros((n, len(speakers)), np.float32)
        result[:, speakers.index(speaker) if speaker in speakers else 0] = 1
        return result
    return expressions.speaker_weights(curves, speakers, speaker).astype(np.float32)


def predict(bank, plan, pitch, curves=None, existing=None, retake=None, force_full=False):
    n = plan['total']
    required = [name for name in ORDER if bank.main.config.get('use_'+name+'_embed')]
    if not required:
        return {}
    model = bank.model('dsvariance')
    if model is None:
        raise ValueError('声学模型需要 ' + ', '.join(required) + '，但音源缺少 dsvariance 模型')
    channels = [name for name in ORDER if model.config.get('predict_'+name, name in ('energy', 'breathiness'))]
    if any(name not in channels for name in required):
        raise ValueError('variance 配置未声明声学模型需要的预测参数')
    pitch = np.asarray(pitch, np.float32).reshape(-1)
    if pitch.shape != (n,) or not np.isfinite(pitch).all():
        raise ValueError('实参预测的 pitch 长度或数值无效')
    speaker = speaker_condition(model, curves, n)
    if speaker.shape[0] != n or not np.isfinite(speaker).all():
        raise ValueError('实参预测的音色曲线无效')
    if not hasattr(bank, 'variance_states'):
        bank.variance_states = runtime.Cache(limit=16*1024*1024, entries=16)
        bank.variance_exact = runtime.Cache(limit=16*1024*1024, entries=32)
    key = structural_key(bank, model, plan, channels)
    previous = bank.variance_states.get(key)
    base = np.full((n, len(channels)), np.nan, np.float32)
    if previous is not None:
        base[:] = previous['values']
    supplied = existing if existing is not None else (curves or {})
    known = np.zeros_like(base, bool)
    if not force_full:
        for code, name in expressions.VARIANCE.items():
            if name not in channels or 'DS:ABS:'+code not in supplied:
                continue
            values = np.asarray(supplied['DS:ABS:'+code], np.float32).reshape(-1)
            if values.shape != (n,) or np.isinf(values).any():
                raise ValueError(f'{code} 局部重算上下文长度或数值错误')
            column = channels.index(name)
            known[:,column] = np.isfinite(values)
            base[known[:,column], column] = values[known[:,column]]
    initialized = ~np.isfinite(base)
    manual = retake is not None
    if manual:
        requested = np.asarray(retake, bool)
        if requested.shape != (n,):
            raise ValueError('实参局部重算范围与预测帧数不符')
        mask = np.broadcast_to(requested[:,None], base.shape).copy() | initialized
    elif force_full or previous is None:
        mask = np.ones_like(base, bool) if force_full else ~known
    else:
        changed = np.abs(pitch-previous['pitch']) > 1e-4
        changed |= np.any(np.abs(speaker-previous['speaker']) > 1e-6, axis=1)
        mask = np.broadcast_to(changed[:,None], base.shape).copy() | initialized
        # Releasing a frozen/manual base returns those frames to automatic prediction.
        mask |= previous['frozen'] & ~known
        mask &= ~known
    # Exact condition cache keeps repeated preview / undo stable, before audio caching.
    digest = hashlib.sha256(key + pitch.tobytes() + speaker.tobytes() + known.tobytes())
    digest.update(np.where(known, base, 0).tobytes())
    exact_key = digest.digest()
    exact = bank.variance_exact.get(exact_key) if not manual and not force_full else None
    mode = 'selected' if manual else 'automatic'
    if force_full:
        mode = 'full'
    elif previous is None and not known.any():
        mode = 'initial'
    if exact is not None:
        base = exact['values'].copy()
        mask[:] = False
    fallback = False
    if mask.any():
        # A partial request must really condition on previous values, not just crop output.
        if (~mask).any():
            inputs = {i.name for i in model.session('variance').get_inputs()}
            can_lock = {'retake', *channels}.issubset(inputs)
            if not can_lock and manual:
                raise ValueError('该音源 variance 模型缺少 retake 或参数上下文输入，不能局部重算；请使用“重新生成整轨实参”')
            if not can_lock:
                mask[:] = True
                mode, fallback = 'fallback_full', True
                warning = '该音源 variance 模型不支持局部重算，自动预测已回退为整句'
                warnings = getattr(bank.execution, 'warnings', None)
                if warnings is not None and warning not in warnings:
                    warnings.append(warning)
        model.expression_curves = curves
        inputs = model.linguistic(plan['phones'], plan['ph_dur'], plan['word_div'], plan['word_dur'])
        inputs.update(ph_dur=[plan['ph_dur']], pitch=pitch[None], retake=mask[None])
        for i, name in enumerate(channels):
            inputs[name] = np.where(np.isfinite(base[:,i]), base[:,i], 0)[None].astype(np.float32)
        outputs = model.run('variance', inputs, n)
        predicted = []
        for name in channels:
            raw = outputs.get(name+'_pred', outputs.get(name))
            if raw is None or np.asarray(raw).size != n or not np.isfinite(raw).all():
                raise ValueError(f'variance 模型没有返回有效的 {name} 曲线')
            predicted.append(np.asarray(raw, np.float32).reshape(-1))
        # Some ONNX exports drift outside the retake mask. Enforce exact locks ourselves.
        base = np.where(mask, np.stack(predicted, axis=1), base)
    else:
        mode = 'cached'
    state = dict(values=base.copy(), pitch=pitch.copy(), speaker=speaker.copy(), frozen=known.copy())
    size = sum(v.nbytes for v in state.values())
    bank.variance_states.put(key, state, size)
    if not manual and not force_full:
        bank.variance_exact.put(exact_key, state, size)
    events = getattr(bank, 'parameter_events', None)
    if events is not None:
        events.append(dict(mode=mode, frames=n, changed_frames=int(mask.any(axis=1).sum()),
            locked_values=int((~mask).sum()), initialized_values=int(initialized.sum()),
            channels=channels, channel_frames={name:int(mask[:,i].sum()) for i,name in enumerate(channels)},
            fallback_full=fallback))
    return {name:base[:,channels.index(name)][None].copy() for name in required}
