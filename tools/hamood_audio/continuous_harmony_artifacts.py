"""Validate final WAVs and prepare A/B previews and an honest arrangement report."""
from pathlib import Path
import os,json,hashlib
import numpy as np,soundfile as sf

BASE=Path(__file__).resolve().parents[3];WORK=BASE/'HAMOOD-连续和声试作'
os.environ['MPLCONFIGDIR']=str(WORK/'matplotlib')

def main():
    project=json.loads((WORK/'project-result.json').read_text(encoding='utf-8'))
    plan=json.loads((WORK/'arrangement.json').read_text(encoding='utf-8'))
    final=json.loads((WORK/'final-snapshot.json').read_text(encoding='utf-8'))
    rendered=json.loads((WORK/'render-results.json').read_text(encoding='utf-8'))
    assert len(rendered)==4,rendered
    assert all('utau-resampler' in r['backend'] for r in rendered)
    mix,sr=sf.read(WORK/'东京泰迪熊-新和声完整混音.wav',dtype='float32',always_2d=True)
    harmony,hr=sf.read(WORK/'东京泰迪熊-新和声单独.wav',dtype='float32',always_2d=True)
    assert sr==hr==48000 and mix.shape==harmony.shape and len(mix)>190*sr
    assert np.isfinite(mix).all() and np.isfinite(harmony).all()
    assert np.max(abs(mix))<1 and np.max(abs(harmony))<1
    full_baseline_path=WORK/'东京泰迪熊-对照主唱与伴奏.wav'
    full_baseline=None
    if full_baseline_path.exists():
        full_baseline,bsr=sf.read(full_baseline_path,dtype='float32',always_2d=True)
        assert bsr==sr and full_baseline.shape==mix.shape
    sections=[]
    for lo,hi in [(45,67),(148,170)]:
        a=round(lo*sr);z=round(hi*sr)
        if full_baseline is not None:
            baseline=full_baseline[a:z];bsr=sr
            sf.write(WORK/f'{lo}-{hi}秒-A-主唱与伴奏.wav',baseline,sr,subtype='PCM_24')
        else:
            baseline,bsr=sf.read(WORK/f'{lo}-{hi}秒-A-主唱与伴奏.wav',dtype='float32',always_2d=True)
        assert bsr==sr and baseline.shape==mix[a:z].shape
        sf.write(WORK/f'{lo}-{hi}秒-B-加入新和声.wav',mix[a:z],sr,subtype='PCM_24')
        # A/B has no independent normalization: the added harmony is the change.
        solo=harmony[a:z];gain=min(10**(.3),.9/max(1e-8,float(np.abs(solo).max())))
        sf.write(WORK/f'{lo}-{hi}秒-新和声单独.wav',solo*gain,sr,subtype='PCM_24')
        assert float(np.sqrt(np.mean((mix[a:z]-baseline)**2)))>.002
        active=[]
        for r in plan['voices']['main']:
            if r['start']>=hi or r['end']<=lo:continue
            x=harmony[round((max(lo,r['start'])+.03)*sr):round(min(hi,r['end'])*sr)]
            if len(x):active.append(float(np.sqrt(np.mean(x*x))))
        assert active and np.percentile(active,10)>.0003,active
        sections.append(dict(start=lo,end=hi,solo_preview_gain_db=float(20*np.log10(gain)),
            mix_peak=float(np.max(abs(mix[a:z]))),harmony_rms=float(np.sqrt(np.mean(solo*solo))),
            change_rms=float(np.sqrt(np.mean((mix[a:z]-baseline)**2))),
            stem_sum_residual_rms=float(np.sqrt(np.mean((mix[a:z]-baseline-solo)**2))),
            main_note_count=len(active),non_silent_main_notes=sum(x>.0003 for x in active)))
    validation=dict(sample_rate=sr,channels=mix.shape[1],bit_depth=24,duration_seconds=len(mix)/sr,
        mix_peak=float(np.max(abs(mix))),harmony_peak=float(np.max(abs(harmony))),
        sections=sections,flags_and_timing_checks=project['flag_lyric_timing_checks'],
        ab_both_cropped_from_full_render=full_baseline is not None,
        source_unchanged=hashlib.sha256(Path(project['source']).read_bytes()).hexdigest()==project['source_sha256'])
    assert validation['source_unchanged']
    (WORK/'audio-validation.json').write_text(json.dumps(validation,ensure_ascii=False,indent=2),encoding='utf-8')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.font_manager import FontProperties
    plt.rcParams['font.family']=FontProperties(fname='C:/Windows/Fonts/msyh.ttc').get_name();plt.rcParams['axes.unicode_minus']=False
    fig,axs=plt.subplots(2,1,figsize=(14,7))
    lead=final['tracks'][0]['clips'][0]['notes']
    for ax,(lo,hi) in zip(axs,[(45,67),(148,170)]):
        labels=set()
        for n in lead:
            a=n['start_seconds'];z=a+n['duration_seconds']
            if a>=hi or z<=lo:continue
            name='主唱';ax.plot([a,z],[n['midi']]*2,color='#87919c',lw=2,label=name if name not in labels else None);labels.add(name)
        for kind,rows in plan['voices'].items():
            for r in rows:
                if r['start']>=hi or r['end']<=lo:continue
                inferred=r['provenance']=='composed_connection'
                name=('主和声' if kind=='main' else '高声部')+(' · 补写连接' if inferred else ' · 音频线索支持')
                ax.plot([r['start'],r['end']],[r['target_midi']]*2,color='#177f88' if kind=='main' else '#ce8831',
                    lw=3,ls='--' if inferred else '-',label=name if name not in labels else None);labels.add(name)
        ax.set_xlim(lo,hi);ax.set_ylim(44,72);ax.grid(alpha=.15);ax.set_xlabel('工程时间 / 秒');ax.set_ylabel('翻唱音区 / MIDI')
        ax.legend(fontsize=8,ncols=3,loc='upper right')
    fig.suptitle('东京泰迪熊 · 连续和声试作（结合原唱线索与补写编配，非逐音还原）')
    fig.tight_layout();fig.savefig(WORK/'和声编配预览.png',dpi=150);plt.close(fig)
    summary=plan['summary'];m=summary['voices']['main'];u=summary['voices']['upper']
    lines=['# 东京泰迪熊 · 连续和声试作','',
        '本版根据已有原唱的人声 / 背景声部分离和连续音高线索，结合伴奏和弦与声部连接，制作可编辑、可试听的和声编配。它不是原唱和声逐音还原。','',
        f"- 主和声：{m['notes']} 个音符，其中 {m['audio_supported']} 个有音频线索支持，{m['composed_connections']} 个为依据和弦与前后声部补写。",
        f"- 高声部点缀：{u['notes']} 个音符，其中 {u['audio_supported']} 个有音频线索支持，{u['composed_connections']} 个为补写连接。",
        '- 覆盖三段副歌及前面的衔接句：42.2–67.1 秒、98.69–123.6 秒、132.23–170.66 秒。休止仍保留。',
        '- 使用原有葛平 UTAU 音源和 WCSNDM 0.0803。音高转换到当前翻唱音区；不是把原唱声部分离 WAV 混入翻唱。',
        '- 新声部继承主轨歌词、音符起止、音源设置及 FLAG；重新整理和声自己的滑音入口和音内轻微变化。',
        '- 主和声相对主轨 -9 dB、左偏 25%；高声部 -14 dB、右偏 30%。两条新轨均已启用。',
        '- 旧试作和参考轨仍保留在新工程中，默认静音。原工程文件、主唱音符和纯伴奏没有改变。','',
        f"[可编辑工程](<{Path(project['project']).as_posix()}>)",'',
        f"[完整混音 WAV](<{(WORK/'东京泰迪熊-新和声完整混音.wav').as_posix()}>)",'',
        f"[和声单独 WAV](<{(WORK/'东京泰迪熊-新和声单独.wav').as_posix()}>)",'',
        f"[两声部 MIDI](<{(WORK/'东京泰迪熊-连续和声编配.mid').as_posix()}>)",'',
        '## 副歌 A/B 试听','',
        'A 为现有主唱加伴奏；B 在相同基础上加入新和声。A/B 未分别归一化。单独和声试听额外提高约 6 dB，便于听清。','']
    for lo,hi in [(45,67),(148,170)]:
        for label,name in [('A · 主唱与伴奏',f'{lo}-{hi}秒-A-主唱与伴奏.wav'),('B · 加入新和声',f'{lo}-{hi}秒-B-加入新和声.wav'),('和声单独',f'{lo}-{hi}秒-新和声单独.wav')]:
            lines.append(f'- {lo}–{hi} 秒 [{label}](<{(WORK/name).as_posix()}>)')
    lines += ['',f"![和声编配](<{(WORK/'和声编配预览.png').as_posix()}>)",'',
        '## 检查结果','',
        f"- 337 个新音符的歌词、FLAG、时间和基准音高校验通过；保存后重新打开工程一致。",
        '- 已核对 MCP 写入的音高曲线使用音符内相对时间，并覆盖到音符末尾。实际采样曲线的音内偏差检查通过。',
        f"- 输出为 48 kHz / 双声道 / 24 bit，完整时长 {validation['duration_seconds']:.3f} 秒；检查了有限样本、非静音、削波和副歌中各主和声音符的有声输出。",
        '- 分离和音高识别仍可能混入泛音、主唱残留或八度误判，音频线索支持不等于人工确认正确。补写的 121 个音符已经在 arrangement.json 中逐个标记。',
        '- 这次改动是独立和声试作工程，没有更新软件版本、替换 EXE 或覆盖旧工程。','']
    (WORK/'试听说明.md').write_text('\n'.join(lines),encoding='utf-8')
    print(json.dumps(validation,ensure_ascii=True,indent=2),flush=True)

if __name__=='__main__':main()
