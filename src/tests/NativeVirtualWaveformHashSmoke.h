#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
// The drawn waveform must follow the audio. A generated glide changes the
// rendered PCM, so the waveform identity has to change with it.
inline bool runNativeVirtualWaveformHashSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const std::array<double,2> hz{220.,330.};
    for(int i=0;i<2;++i)
    {
        const auto file=output.getChildFile(i?"high.wav":"low.wav");
        juce::AudioBuffer<float> pcm(1,96000);double phase=0;
        for(int s=0;s<pcm.getNumSamples();++s)
        {
            phase+=juce::MathConstants<double>::twoPi*hz[std::size_t(i)]/48000.;
            double value=0;for(int h=1;h<=12;++h)value+=std::sin(phase*h)/h*std::exp(-h*.22);
            pcm.setSample(0,s,float(.25*value));
        }
        auto stream=file.createOutputStream();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));
        if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;
    }
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;
    track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;
    const std::array<float,2> midi{57.f,64.f};
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"right-owner":"left-owner";clip.sourceFile=output.getChildFile(i?"high.wav":"low.wav");
        clip.startSeconds=i?.9:.2;clip.durationSeconds=clip.sourceDurationSeconds=.8;
        clip.sourceTimeMap={{0,0},{.8,.8}};
        NoteData note;note.id=i?"right":"left";note.label="-";note.durationSeconds=.8;
        note.midiNote=note.sourceMidiCenter=midi[std::size_t(i)];note.sourcePitchMeasured=true;
        for(int f=0;f<=160;++f)note.contour.push_back({f*.005,0,0,true});
        clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};
    const auto hashesOf=[](const ProjectData& data)
    {
        std::vector<std::uint64_t> hashes;
        for(const auto& t:data.tracks)for(const auto& c:t.clips)
            hashes.push_back(AudioEngine::nativeClipWaveformHash(c,t,&data.nativeVirtualGroups));
        return hashes;
    };
    const auto before=hashesOf(seed);
    ProjectModel model;model.resetDocument(seed);
    const auto group=model.createNativeVirtualConnection({"left","right"},true,error,.25);
    check("glide_connection_created",group.isNotEmpty());
    if(group.isEmpty()){output.getChildFile("verification.txt").replaceWithText(log);return false;}
    const auto connected=model.snapshot();
    // The glide really does change the audio: confirm via the render projection.
    ProjectData projected;std::vector<NativeVirtualRenderGroup> groups;
    const auto prepared=prepareNativeVirtualRender(connected,projected,groups,error);
    check("glide_curve_is_active_so_audio_differs",prepared&&groups.size()==1&&groups[0].pitch.active);
    std::cout<<"groups="<<connected.nativeVirtualGroups.size();
    for(const auto& g:connected.nativeVirtualGroups)
        std::cout<<" id="<<g.id<<" track="<<g.trackId<<" enabled="<<g.enabled
            <<" auto="<<g.automaticPitch<<" members="<<g.orderedNoteIds.size();
    std::cout<<std::endl;
    const auto after=hashesOf(connected);
    bool anyChanged=before.size()==after.size();
    if(anyChanged){bool same=true;for(std::size_t i=0;i<before.size();++i)same&=before[i]==after[i];anyChanged=!same;}
    for(std::size_t i=0;i<before.size()&&i<after.size();++i)
        std::cout<<"clip"<<i<<" before="<<before[i]<<" after="<<after[i]
            <<(before[i]==after[i]?" UNCHANGED":" changed")<<std::endl;
    check("generated_glide_invalidates_the_drawn_waveform",anyChanged);
    // Disabling the group restores the original audio, so the identity too.
    model.setNativeVirtualConnectionEnabled(group,false,error);
    const auto disabled=hashesOf(model.snapshot());
    bool restored=disabled.size()==before.size();
    for(std::size_t i=0;restored&&i<before.size();++i)restored&=disabled[i]==before[i];
    check("disabled_group_restores_the_original_waveform_identity",restored);
    // An explicit manual curve must also be reflected.
    model.setNativeVirtualConnectionEnabled(group,true,error);
    // Endpoints must stay on the live source anchors; only the interior moves.
    ProjectData shaped;std::vector<NativeVirtualRenderGroup> shapedGroups;
    bool edited=false;
    if(prepareNativeVirtualRender(model.snapshot(),shaped,shapedGroups,error)&&!shapedGroups.empty())
    {
        auto manual=shapedGroups[0].pitch.points;
        if(manual.size()>=3)
        {
            for(std::size_t i=1;i+1<manual.size();++i)manual[i].targetMidi+=1.5f;
            edited=model.setNativeVirtualPitchCurve(group,manual,error);
        }
    }
    if(edited)
    {
        const auto manualHashes=hashesOf(model.snapshot());
        bool differs=manualHashes.size()==after.size();
        if(differs){bool same=true;for(std::size_t i=0;i<after.size();++i)same&=manualHashes[i]==after[i];differs=!same;}
        check("manual_curve_also_invalidates_the_drawn_waveform",differs);
    }
    else {log+="manual_curve_setter_rejected: "+error+"\n";error.clear();}
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
