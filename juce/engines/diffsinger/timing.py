"""Editable token onsets, in note-local seconds, shared by every DS predictor.

Prediction remains immutable in the plan cache. Manual boundaries are applied
after prediction; rounding happens once at the voicebank's frame resolution.
"""
import hashlib
import json
import math
import numpy as np


def context(bank, note, tokens):
    return hashlib.sha256(json.dumps([str(bank.root), bank.language,
        note['lyric'], tokens], ensure_ascii=False).encode()).hexdigest()


def apply(bank, notes, predicted):
    plan = dict(predicted)
    phones, owners = plan['phones'], plan['owners']
    dt, origin = bank.main.dt, plan['origin']
    base = np.r_[0, np.cumsum(plan['ph_dur'])].astype(np.int64)
    bounds = base.copy()
    edits, contexts, tokens, local_indices = {}, {}, {}, {}
    ignored = []
    for ni, note in enumerate(notes):
        indices = [i for i, owner in enumerate(owners) if owner == ni]
        tokens[ni] = [phones[i] for i in indices]
        contexts[ni] = context(bank, note, tokens[ni])
        local_indices.update({i: j for j, i in enumerate(indices)})
        manual = note.get('timing') or {}
        if not manual:
            continue
        if not isinstance(manual, dict):
            raise ValueError('音素时长数据必须为对象')
        if manual.get('context') != contexts[ni] or manual.get('tokens') != tokens[ni]:
            ignored.append(str(note['id']))
            continue
        starts = manual.get('starts', [])
        if len(starts) != len(indices):
            raise ValueError('音素时长数据数量与音素不符')
        for i, value in zip(indices, starts):
            if value is None:
                continue
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                raise ValueError('音素边界不是有效秒数')
            edits[i] = round((note['start'] + value - origin) / dt)

    # Unedited boundaries remain anchors. Project each run of manually moved
    # boundaries between those anchors; no zero/negative durations are possible,
    # even after shortening notes or changing tempo in an existing project.
    fixed = [i for i in range(len(bounds)) if i not in edits]
    clamped = False
    for left, right in zip(fixed, fixed[1:]):
        for i in range(left + 1, right):
            value = max(int(bounds[i-1])+1, min(edits[i], int(base[right])-(right-i)))
            bounds[i] = value
            clamped |= value != edits[i]
    durations = np.diff(bounds).tolist()
    if any(d <= 0 for d in durations):
        raise ValueError('音符过短，无法容纳全部音素；请延长音符')
    plan['ph_dur'] = durations
    divisions = np.r_[0, np.cumsum(plan['word_div'])]
    plan['word_dur'] = [int(bounds[b]-bounds[a]) for a, b in zip(divisions, divisions[1:])]
    plan['voiced'] = np.repeat([p not in ('SP', 'AP') for p in phones], durations)
    rows = []
    for i, phone in enumerate(phones):
        ni = owners[i]
        note = notes[ni] if ni >= 0 else None
        rows.append(dict(id=str(note['id']) if note else '', index=local_indices.get(i, -1),
            token=phone, kind='S' if phone in ('SP', 'AP') else 'V' if phone in bank.vowels else 'C',
            start=origin+int(bounds[i])*dt, end=origin+int(bounds[i+1])*dt,
            predicted_start=origin+int(base[i])*dt, edited=i in edits,
            editable=note is not None and i > 0,
            note_start=note['start'] if note else 0,
            context=contexts.get(ni, ''), tokens=tokens.get(ni, [])))
    plan["note_onsets"] = [next((r["start"] for r in rows if r["id"] == str(note["id"])), note["start"])
                           for note in notes]
    return plan, rows, ignored, clamped
