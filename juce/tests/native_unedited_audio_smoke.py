#!/usr/bin/env python3
"""Compare untouched imported native vocals to the original PCM, not just F0."""
from pathlib import Path
import json
import os
import shutil
import sys
import numpy as np
from scipy.io import wavfile
from utau4_mode_smoke import McpClient

def read_audio(path):
    rate,data=wavfile.read(path)
    if np.issubdtype(data.dtype,np.integer):
        data=data.astype(np.float64)/(2**(np.iinfo(data.dtype).bits-1))
    else:data=data.astype(np.float64)
    if data.ndim==1:data=data[:,None]
    return rate,data

def main():
    binary,models,vocal,output=[Path(x).resolve() for x in sys.argv[1:5]]
    baseline=len(sys.argv)>5 and sys.argv[5]=='--baseline'
    output.mkdir(parents=True,exist_ok=True)
    os.environ['HACHISHIFTER_MCP_ROOTS']=str(output)
    os.environ['HACHISHIFTER_NSF_HIFIGAN_MODEL_DIR']=str(models/'nsf_hifigan')
    os.environ['HACHISHIFTER_INFERENCE']='cpu'
    source=output/'vocal.wav';shutil.copy2(vocal,source)
    for suffix in ['.hjm.csv','.hachi.csv']:Path(str(source)+suffix).unlink(missing_ok=True)
    rate,pcm=read_audio(source);duration=len(pcm)/rate
    client=McpClient(binary);checks=[]
    try:
        response=client.call('import_audio',{'path':str(source),'game_model_dir':str(models/'game/medium'),
            'fcpe_model':str(models/'fcpe/fcpe.onnx'),'inference':'cpu'})
        snapshot=json.loads(client.call('project_snapshot'));track=snapshot['tracks'][0];clip=track['clips'][0]
        before=clip['notes'];print('backend='+response,flush=True)
        print('inferred_breath_max='+str(max((n['breath'] for n in before),default=0)),flush=True)
        algorithms=['nsf-hifigan'] if baseline else ['nsf-hifigan','world','llsm2']
        for algorithm in algorithms:
            client.call('set_track',{'track_id':track['id'],'pitch_algorithm':algorithm,'normalize_volume':False,
                'volume':.25,'pan':-1 if pcm.shape[1]==1 else 0})
            target=output/(algorithm+'-untouched.wav')
            client.call('export_wav',{'path':str(target),'track_id':track['id'],'from_seconds':0,
                'to_seconds':duration,'sample_rate':rate,'channels':2,'bit_depth':32})
            out_rate,audio=read_audio(target)
            actual=audio[:len(pcm),:pcm.shape[1]]/.25
            # Detection and display must preserve every original sample,
            # including both stereo channels and the first/last 2.5 ms.
            reference=pcm.astype(np.float32)
            error=np.abs(actual-reference);worst=float(error.max());rms=float(np.sqrt(np.mean(error**2)))
            ok=out_rate==rate and len(audio)==len(pcm) and worst<2e-7
            checks.append({'name':algorithm+'-untouched-source','ok':ok,'max_sample_error':worst,'rms_error':rms})
            print(algorithm+'_max_sample_error='+str(worst),flush=True)
            if not baseline:assert ok,checks[-1]
        if not baseline:
            client.call('set_track',{'track_id':track['id'],'pitch_algorithm':'nsf-hifigan'})
            saved=output/'untouched.hjpx';client.call('project_save',{'path':str(saved)})
            client.call('project_new');client.call('project_open',{'path':str(saved)})
            reopened=output/'reopened.wav'
            client.call('export_wav',{'path':str(reopened),'track_id':track['id'],'from_seconds':0,'to_seconds':duration,
                'sample_rate':rate,'channels':2,'bit_depth':32})
            _,audio=read_audio(reopened);reopened_error=float(np.abs(audio[:,:pcm.shape[1]]/.25-reference).max())
            assert reopened_error<2e-7
            checks.append({'name':'saved_source_pitch_keeps_original_audio','ok':True,'max_sample_error':reopened_error})
            # Explicit fades must still affect output; removing implicit fades
            # must not discard user-authored processing on otherwise raw PCM.
            client.call('set_clip',{'clip_id':clip['id'],'fade_in_seconds':.04,'fade_out_seconds':.06})
            faded=output/'explicit-fades.wav'
            client.call('export_wav',{'path':str(faded),'track_id':track['id'],'from_seconds':0,'to_seconds':duration,
                'sample_rate':rate,'channels':2,'bit_depth':32})
            _,audio=read_audio(faded)
            seconds=np.arange(len(pcm))/rate
            attack=np.clip(seconds/.04,0,1).astype(np.float32)
            release=np.clip((duration-seconds)/.06,0,1).astype(np.float32)
            shape=attack*attack*(3-2*attack)*release*release*(3-2*release)
            fade_error=float(np.abs(audio[:,:pcm.shape[1]]/.25-reference*shape[:,None]).max())
            assert fade_error<2e-7
            checks.append({'name':'explicit_clip_fades_still_apply','ok':True,'max_sample_error':fade_error})
            client.call('set_clip',{'clip_id':clip['id'],'fade_in_seconds':0,'fade_out_seconds':0})
            # A real edit must still reach the decoder rather than being bypassed.
            chosen=max(before,key=lambda n:n['duration_seconds'])
            client.call('transpose_note',{'note_id':chosen['id'],'semitones':3})
            for algorithm in ['nsf-hifigan','world']:
                client.call('set_track',{'track_id':track['id'],'pitch_algorithm':algorithm})
                edited=output/(algorithm+'-edited.wav')
                client.call('export_wav',{'path':str(edited),'track_id':track['id'],'from_seconds':0,'to_seconds':duration,
                    'sample_rate':rate,'channels':2,'bit_depth':32})
                _,changed=read_audio(edited);delta=float(np.sqrt(np.mean((changed[:len(pcm),0]/.25-pcm[:,0])**2)))
                assert delta>1e-4;checks.append({'name':algorithm+'-edited_pitch_still_renders','ok':True,'rms_difference':delta})
            assert all(n['breath']==0 for n in before)
            checks.append({'name':'acoustic_evidence_does_not_add_breath','ok':True})
    finally:client.close()
    report={'ok':all(c['ok'] for c in checks),'baseline':baseline,'binary':str(binary),'checks':checks}
    (output/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('pcm_validation_ok='+str(int(report['ok'])),flush=True)

if __name__=='__main__':main()
