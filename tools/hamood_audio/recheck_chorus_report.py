from pathlib import Path
import json, numpy as np, soundfile as sf

BASE=Path(__file__).resolve().parents[3]
WORK=BASE/'HAMOOD-副歌漏检复查'
ab=json.loads((WORK/'filter-ablation.json').read_text(encoding='utf-8'))
sep=json.loads((WORK/'separation-comparison.json').read_text(encoding='utf-8'))
pitch=json.loads((WORK/'pitch-comparison.json').read_text(encoding='utf-8'))
for section in pitch['sections']:
    assert '控制-mid' in section['routes'], 'Complete the matched-window control first'
validation=[]
for p in WORK.glob('*.wav'):
    if p.name.startswith('原曲'):continue
    a,sr=sf.read(p,dtype='float32',always_2d=True)
    assert sr==44100 and a.shape==(970200,2) and np.isfinite(a).all() and np.abs(a).max()<1
    validation.append(dict(path=str(p),frames=len(a),sample_rate=sr,channels=2,peak=float(np.abs(a).max())))
assert len(validation)==6
(WORK/'audition-validation.json').write_text(json.dumps(validation,ensure_ascii=False,indent=2),encoding='utf-8')

lines=['# 东京泰迪熊：副歌漏检复查', '',
'本轮结论：不能把稀疏结果简单归因于分离模型。旧流程在分离之后还有多道过严筛选，又把多数待复核候选静音，结果只代表少量高门槛候选，不能作为完整和声还原。', '',
'本轮做了全曲缓存音高特征的筛选消融，以及副歌中 45–67 秒、148–170 秒两段的重新人声提取对照。没有人工标注的真实和声谱，因此以下“候选数量 / 覆盖时长”不能当作准确率或真实召回率。', '',
'## 1. 已确认的流程问题', '',
'- 旧搜索范围限制为主轨 +12 后的 ±9 半音，且排除八度叠唱。原唱与主轨的音区、咬字和细微节奏差异都会影响匹配。',
'- 要求背景声部在拆分前的完整人声里也达到分数门槛，会排除被主唱掩盖的弱声部；这正是本应依赖分离去发现的内容。',
'- 要求“原唱主音恰好等于主轨 +12”的分数达标，导致错误连带排除其他声部。',
'- 一整个主轨音符只能取一个和声音高，且按音符内部平均分筛选，不能表示和声自己的节奏、换音和跨音符连线，也忽略主轨空隙里的和声。',
'- 用单音高 CREPE 结果复核多声部会把次要声部降级；它只能作为支持线索，不能否定同时存在的其他音高。',
'- 最终 32 个较强候选启用，63 个待复核候选默认静音，进一步造成听感稀疏。', '',
'## 2. 逐项去除硬门槛（整曲 478 个主轨音符位置）', '',
'以下只统计存在可疑音高证据的主轨位置，不是确认真实和声的数量。每行是在上一行基础上继续调整。', '',
'| 条件 | 有候选的主轨位置数 |','|---|---:|']
for key,value in ab['sections'][0]['ablation'].items():lines.append(f'| {key} | {value} |')
lines += ['', '再从“主轨逐音符”改为独立连续时间检测，允许多音高并存，CREPE 和疑似主唱残留仅作标记：', '',
'| 抽查片段 | 旧结果启用的较强候选 | 旧启用候选覆盖 | 连续检测线索覆盖 |', '|---|---:|---:|---:|']
for s in ab['sections'][1:]:lines.append(f"| {s['start']}–{s['end']} 秒 | {s['old_strong']} 个 | {s['old_strong_time_seconds']:.1f} 秒 | {s['continuous_candidate_time_seconds']:.1f} 秒 |")
lines += ['', '连续候选还可能包含主唱残留、泛音、八度误判；暂未覆盖的时间也可能是辅音、休止或模型漏检。不能用这些数证明“整段副歌已还原”。', '',
'## 3. 从原曲重新提取人声的实测', '',
'旧路线：已有 Vocals 分轨 → KARA2。新路线：原曲 → 本机 UVR-MDX-NET-Inst_HQ_3（残差人声）→ KARA2。为公平比较，又对旧人声使用完全相同的 26 秒裁切和 KARA2 窗口，两端各排除 2 秒。', '',
'新旧原始人声先按样本对齐。原曲与旧四分轨之和在六处的相关系数均超过 0.999，FFmpeg 解码时间轴无需另加延迟；项目人声裁切为 45,486 样本（1.0314286 秒）。这排除了原曲与分轨之间的大幅错位，但不代表主轨 MIDI 与歌手每个音节的时值完全一致。', '',
'| 片段 | 新背景声部相对旧控制版的 RMS | 旧控制版连续线索 | 新路线连续线索 | 左右声道分别检查后：旧 / 新 |',
'|---|---:|---:|---:|---:|']
for s,p in zip(sep['excerpts'],pitch['sections']):
    m=s['metrics'];db=20*np.log10(m['新KARA2-backing']['rms']/m['控制KARA2-backing']['rms'])
    r=p['routes']
    lines.append(f"| {p['start']}–{p['end']} 秒 | {db:.1f} dB | {r['控制-mid']['candidate_seconds']:.1f} 秒 | {r['新-mid']['candidate_seconds']:.1f} 秒 | {r['控制-stereo_max']['candidate_seconds']:.1f} / {r['新-stereo_max']['candidate_seconds']:.1f} 秒 |")
lines += ['', '这次更换第一阶段模型没有获得更多音高线索。更弱的输出可能同时意味着背景人声衰减、乐器残留减少等，单凭能量和候选计数不能认定哪版更准。两级提取是合理路线，但首阶段必须保留背景人声，不能把“重新提取”本身当作保证。', '',
'左右声道相关性为正，本例没有大面积反相抵消证据；分声道检查能补回部分线索，但不是稀疏结果的唯一原因。', '',
'## 4. 可直接试听', '',
'A 是原有 Vocals 分轨经相同裁切重新运行 KARA2；B 是这次从原曲重新提取人声后运行 KARA2。共同提升 6 dB 的 A/B 保留相对响度；“匹配 A 音量”的 B 便于比较内容。所有文件为工程对应区间的 22 秒、44.1 kHz、双声道、24 bit WAV，非静音、无削波检查通过。', '']
for lo,hi in [(45,67),(148,170)]:
    for label,name in [('A：原有分轨',f'{lo}-{hi}秒-A-原有分轨后的背景声部-共同提升6dB.wav'),
                       ('B：从原曲重提人声（匹配 A 音量）',f'{lo}-{hi}秒-B-重提人声后-匹配A音量.wav')]:
        lines.append(f'- {lo}–{hi} 秒 [{label}](<{(WORK/name).as_posix()}>)')
lines += ['',f'![副歌检测对比](<{(WORK/"副歌漏检对比.png").as_posix()}>)','',
'## 5. 下一版分析应采用的流程', '',
'原曲 → 保留全部人声的分离 → 同时保留完整人声、主唱、背景声部 → 左 / 右 / 中间声道分别提取多音高 → 连续追踪声部 → 用主旋律、和弦和声部连续性评分并标记歧义 → 再映射到工程音符。', '',
'本轮连续检测草稿已允许独立节奏、多音高与八度候选，取消“完整人声也必须达标”和“CREPE 只能支持一个音高”的否决规则。它是待复核的高覆盖候选，不是定稿；未直接覆盖用户工程，也没有把放宽阈值的所有候选当作确认和声。', '',
f'[连续候选 MIDI（适配翻唱低八度；八度候选单列，全部待复核）](<{(WORK/"连续和声候选-待复核-低八度.mid").as_posix()}>)', '',
'## 6. 复现与依据', '',
'原始诊断数据：filter-ablation.json、separation-comparison.json、pitch-comparison.json、audition-validation.json。',
'脚本：HachiShifter-integrated/tools/hamood_audio/recheck_chorus_*.py。沿用本机已有模型，没有下载额外模型，也没有改动软件公开版本。', '',
'- [Basic Pitch 官方说明](https://github.com/spotify/basic-pitch)：支持复音，但最佳场景是单一乐器；并非专门的多歌手和声转录器。',
'- [torchcrepe 官方项目](https://github.com/maxrmorrison/torchcrepe)：F0 / 周期性提供单条基频证据，不能否定复音中的其余声部。',
'- [UVR 官方 MDX 模型参数](https://github.com/Anjok07/ultimatevocalremovergui/blob/master/models/MDX_Net_Models/model_data/model_data.json)：Inst_HQ_3 使用本机 UVR 哈希匹配到的 FFT 6144、频带 3072、补偿 1.022；KARA2 为 FFT 5120、频带 2048、补偿 1.065。', '']
(WORK/'复查结果.md').write_text('\n'.join(lines),encoding='utf-8')
print(json.dumps(dict(verified_previews=len(validation),report=str(WORK/'复查结果.md')),ensure_ascii=True))
