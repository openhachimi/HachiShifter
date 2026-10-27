#pragma once
#include "../AudioEngine.h"
#include "../NativeSourceEdgePlan.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>

namespace hachi
{
inline bool runNativeSourceSliceEdgeSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("source-slice-edges-"+juce::Uuid().toString());
    if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;
    const auto check=[&](const char* key,bool passed){ok&=passed;log+=juce::String(key)+"="+(passed?"PASS":"FAIL")+"\n";std::cout<<key<<'='<<passed<<std::endl;};
    const auto source=root.getChildFile("source.wav");
    juce::AudioBuffer<float> wave(1,48000);for(int i=0;i<48000;++i)wave.setSample(0,i,.12f*std::sin(juce::MathConstants<double>::twoPi*197*i/48000.0));
    auto stream=source.createOutputStream();juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),48000,1,16,{},0):nullptr);
    check("control_PCM_fixture",writer&&writer->writeFromAudioSampleBuffer(wave,0,48000));writer.reset();if(!ok)return false;
    const auto hash=juce::SHA256(source).toHexString();
    TrackData track;track.id="slice-track";track.trackType=TrackType::hachi;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=true;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;
    ClipData clip;clip.id="source-owner";clip.sourceFile=source;clip.durationSeconds=clip.sourceDurationSeconds=1;clip.sourceTimeMap={{0,0},{1,1}};
    NoteData note;note.id="whole";note.label="a";note.durationSeconds=1;note.nativeUnpitched=true;
    note.nativeSourceStartSeconds=note.materialSourceStartSeconds=0;note.nativeSourceEndSeconds=note.materialSourceEndSeconds=1;
    note.materialAudioId="recording";note.materialRegionId="syllable";
    NativePronunciationEdges edge;edge.entryFromSourceSeconds=0;edge.entryToSourceSeconds=.4;edge.exitFromSourceSeconds=.6;edge.exitToSourceSeconds=edge.terminationSourceSeconds=1;edge.provenance="authored-slice-fixture";
    note.nativePronunciationEdges=edge;clip.notes={note};track.clips={clip};
    const auto pull=[&](const TrackData& input,juce::AudioBuffer<float>& data)
    {
        AudioEngine engine(false);engine.prepareToPlay(251,48000);ProjectData project;project.tracks={input};engine.syncProject(project);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;
        while(juce::Time::getMillisecondCounterHiRes()<deadline)
        {const auto progress=engine.renderProgress();if(!progress||*progress>=.999)break;juce::Thread::sleep(2);}
        if(engine.activeRenderWarnings().isNotEmpty()){error=engine.activeRenderWarnings();return false;}
        data.setSize(2,48000);data.clear();engine.setPosition(0);engine.play();
        for(int offset=0;offset<48000;offset+=251)engine.getNextAudioBlock({&data,offset,std::min(251,48000-offset)});
        engine.stop();return engine.activeRenderWarnings().isEmpty();
    };
    juce::AudioBuffer<float> wholePCM,splitPCM,partPCM;
    check("actual_whole_mixer",pull(track,wholePCM));
    ProjectModel model;ProjectData initial;initial.tracks={track};model.resetDocument(initial);
    const auto second=model.splitNote("whole",.1);const auto third=model.splitNote(second,.5);const auto fourth=model.splitNote(third,.9);
    check("actual_model_split_operations",second.isNotEmpty()&&third.isNotEmpty()&&fourth.isNotEmpty());
    const auto split=model.snapshot().tracks[0];NativeSourceEdgePlan plan;
    check("split_component_resolves",resolveNativeSourceEdgePlan(split,{},plan,error)&&plan.components.size()==1&&plan.members.size()==4);
    check("entry_and_exit_windows_cross_retained_slices",plan.members.size()==4&&plan.members[0].entryWindowActive&&plan.members[1].entryWindowActive
        &&plan.members[2].exitWindowActive&&plan.members[3].exitWindowActive&&!plan.members[1].exposedEntry&&!plan.members[2].exposedExit);
    check("actual_split_mixer",pull(split,splitPCM));
    const auto equalPCM=[&](const auto& a,const auto& b)
    {if(a.getNumSamples()!=b.getNumSamples())return false;for(int c=0;c<2;++c)for(int i=0;i<a.getNumSamples();++i)if(std::abs(a.getSample(c,i)-b.getSample(c,i))>1.e-7f)return false;return true;};
    check("split_inside_both_fades_preserves_actual_PCM",wholePCM.getNumSamples()>0&&equalPCM(wholePCM,splitPCM));
    auto parts=track;auto& parent=parts.clips[0];parent.nativeAudioLinked=true;parent.parts.clear();parent.notes.clear();
    for(int i=0;i<2;++i)
    {
        const auto begin=i==0?0.0:.2,end=i==0?.2:1.0;
        auto owner=clip;owner.id="part"+juce::String(i);owner.startSeconds=begin;owner.durationSeconds=owner.sourceDurationSeconds=end-begin;owner.sourceOffsetSeconds=begin;owner.sourceTimeMap={{0,0},{end-begin,end-begin}};owner.notes.clear();
        auto partNote=note;partNote.id="part-note"+juce::String(i);partNote.clipPartId=owner.id;partNote.startSeconds=begin;partNote.durationSeconds=end-begin;
        partNote.nativeSourceStartSeconds=begin;partNote.nativeSourceEndSeconds=end;
        parent.parts.push_back(owner);parent.notes.push_back(partNote);
    }
    check("cross_owner_component_clock",resolveNativeSourceEdgePlan(parts,{},plan,error)&&plan.components.size()==1&&plan.members.size()==2&&plan.members[1].entryWindowActive);
    check("actual_cross_owner_mixer",pull(parts,partPCM));
    {
        AudioEngine engine(false);ProjectData data;data.tracks={parts};engine.syncProject(data);bool gains=true;
        const auto curve=[](double x){x=juce::jlimit(0.0,1.0,x);return float(x*x*(3-2*x));};
        for(int i=0;i<48000;++i)
        {
            const auto time=i/48000.0;const bool secondPart=i>=9600;
            const auto gain=engine.diagnosticNativeSourceEdgeGain(secondPart?"source-owner:part1":"source-owner:part0",time-(secondPart?.2:0));
            const auto expected=curve(time/.4)*curve((1-time)/.4);
            if(!gain||std::abs(*gain-expected)>2.e-7f){gains=false;log+="gain_mismatch_sample="+juce::String(i)+"\n";break;}
        }
        check("cross_child_every_sample_uses_component_window",gains);
    }
    if(wholePCM.getNumSamples()==partPCM.getNumSamples())
    {
        double maximum=0;int first=-1;
        for(int i=0;i<wholePCM.getNumSamples();++i){const auto difference=std::abs(double(wholePCM.getSample(0,i))-partPCM.getSample(0,i));maximum=std::max(maximum,difference);if(first<0&&difference>1.e-7)first=i;}
        log+="cross_child_max_PCM_difference="+juce::String(maximum,12)+" first_sample="+juce::String(first)+"\n";
    }
    check("cross_child_entry_window_negative_local_anchor_preserves_PCM",wholePCM.getNumSamples()>0&&equalPCM(wholePCM,partPCM));
    auto retained=split;retained.clips[0].notes.erase(retained.clips[0].notes.begin());
    check("removed_head_derives_clipped_exposed_window",resolveNativeSourceEdgePlan(retained,{},plan,error)&&plan.members.size()==3
        &&std::abs(plan.members.front().entryFromTargetSeconds-.1)<1.e-9&&std::abs(plan.members.front().entryToTargetSeconds-.4)<1.e-9);
    const auto saved=root.getChildFile("split.hjpx");ProjectModel cold;
    check("split_save_cold_reopen",model.save(saved,error)&&cold.load(saved,error)&&resolveNativeSourceEdgePlan(cold.snapshot().tracks[0],{},plan,error)&&plan.members.size()==4);
    check("source_PCM_unchanged",juce::SHA256(source).toHexString()==hash);
    if(!ok)log+="error="+error+"\n";
    return root.getChildFile("verification.txt").replaceWithText(log)&&ok;
}
}
