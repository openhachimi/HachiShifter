#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
namespace hachi
{
// Controlled PCM through the normal AudioEngine callback. These unpitched
// fixtures verify routing/weights, not perceptual quality or neural singing.
inline bool runNativeVirtualMixerSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    std::vector<juce::String> hashes;
    for(int i=0;i<3;++i)
    {
        const juce::String id=i==0?"left":i==1?"any-identified-transition":"right";
        ClipData clip;clip.id="owner-"+id;clip.sourceFile=output.getChildFile(id+".wav");clip.startSeconds=i==0?0.:i==1?.8:1.;
        clip.durationSeconds=clip.sourceDurationSeconds=i==0?1.2:i==1?.6:1.;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        juce::AudioBuffer<float> pcm(1,juce::roundToInt(clip.durationSeconds*48000));
        for(int sample=0;sample<pcm.getNumSamples();++sample)pcm.setSample(0,sample,.08f*std::sin(juce::MathConstants<double>::twoPi*(173+73*i)*sample/48000.));
        auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;writer.reset();hashes.push_back(juce::SHA256(clip.sourceFile).toHexString());
        NoteData note;note.id=id;note.label="-";note.durationSeconds=clip.durationSeconds;note.nativeUnpitched=true;
        if(i==1){note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=NativeTransitionShape{1,.3,"fixture-explicit-middle"};}
        clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};
    const auto render=[&](const ProjectData& project,juce::AudioBuffer<float>& result)
    {
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(project);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;
        while(juce::Time::getMillisecondCounterHiRes()<deadline)
        {const auto progress=engine.renderProgress();if(!progress||*progress>=.999)break;juce::Thread::sleep(2);}
        if(const auto progress=engine.renderProgress();progress&&*progress<.999){error="Fixture render exceeded 10 seconds";return false;}
        if(!engine.activeRenderWarnings().isEmpty()){error=engine.activeRenderWarnings();return false;}
        result.setSize(2,96000);result.clear();engine.setPosition(0);engine.play();
        for(int offset=0;offset<96000;offset+=257)engine.getNextAudioBlock({&result,offset,std::min(257,96000-offset)});
        engine.stop();return engine.activeRenderWarnings().isEmpty();
    };
    std::array<juce::AudioBuffer<float>,3> solo;bool ready=true;
    for(int i=0;i<3;++i){auto one=data;one.tracks[0].clips={track.clips[std::size_t(i)]};ready&=render(one,solo[std::size_t(i)]);}
    check("independent_real_PCM_sources_ready",ready);if(!ready)return false;
    ProjectModel model;model.resetDocument(data);const auto id=model.createNativeVirtualConnection({"left","any-identified-transition","right"},false,error);
    check("triple_virtual_group_created",id.isNotEmpty());if(id.isEmpty())return false;
    NativeVirtualConnectionPlan plan;check("triple_plan_uses_explicit_transition",planNativeVirtualConnection(track,{"left","any-identified-transition","right"},plan,error));
    juce::AudioBuffer<float> routed;const auto audible=render(model.snapshot(),routed);bool exact=audible;double maximum=0;
    for(int s=0;audible&&s<96000;++s)
    {
        const auto weights=nativeVirtualContributions(plan,s/48000.);
        for(int ch=0;ch<2;++ch)
        {
            double expected=0;for(int i=0;i<3;++i)expected+=weights[std::size_t(i)]*solo[std::size_t(i)].getSample(ch,s);
            maximum=std::max(maximum,std::abs(double(routed.getSample(ch,s))-expected));
        }
    }
    std::cout<<"maximum_mixer_difference="<<maximum<<std::endl;exact&=maximum<2.e-6;
    check("normal_audio_callback_uses_one_group_partition_every_sample",exact);
    juce::AudioBuffer<float> baseline,disabled;const auto raw=render(data,baseline);model.setNativeVirtualConnectionEnabled(id,false,error);
    bool restored=raw&&render(model.snapshot(),disabled);for(int ch=0;restored&&ch<2;++ch)restored&=std::equal(baseline.getReadPointer(ch),baseline.getReadPointer(ch)+96000,disabled.getReadPointer(ch));
    check("disabled_group_restores_original_PCM_bit_exact",restored);
    bool unchanged=true;for(int i=0;i<3;++i)unchanged&=juce::SHA256(track.clips[std::size_t(i)].sourceFile).toHexString()==hashes[std::size_t(i)];check("physical_source_bytes_never_rewritten",unchanged);
    auto projected=data;auto compatibility=data;NativeConnection old;old.id="old-direct";old.leftNoteId="left";old.rightNoteId="right";old.type="pitch-portamento";old.pitchCurve={{1.,60},{1.2,64}};
    compatibility.nativeConnections={old};compatibility.nativeVirtualGroups=model.snapshot().nativeVirtualGroups;compatibility.nativeVirtualGroups[0].enabled=true;
    compatibility.tracks[0].clips[0].crossfadeOutSeconds=.2;compatibility.tracks[0].clips[2].crossfadeInSeconds=.2;
    const auto authorFade=.04;compatibility.tracks[0].clips[0].fadeOutSeconds=authorFade;
    std::vector<NativeVirtualRenderGroup> groups;
    check("render_projection_suppresses_old_direct_route_only_temporarily",prepareNativeVirtualRender(compatibility,projected,groups,error)&&projected.nativeConnections.empty()&&compatibility.nativeConnections.size()==1&&projected.tracks[0].clips[0].crossfadeOutSeconds==0&&compatibility.tracks[0].clips[0].crossfadeOutSeconds==.2&&projected.tracks[0].clips[0].fadeOutSeconds==authorFade);
    // da_re shares one physical/render owner: connect only re. da must retain
    // its exact original waveform, outer authored gain and all timing bits.
    auto multiple=data;auto& multiOwner=multiple.tracks[0].clips[0];
    auto da=multiOwner.notes[0];da.id="unselected-da";da.durationSeconds=.6;
    multiOwner.notes[0].startSeconds=.6;multiOwner.notes[0].durationSeconds=.6;
    multiOwner.notes.insert(multiOwner.notes.begin(),da);multiOwner.fadeInSeconds=.03;multiOwner.gain=.8f;
    ProjectModel multiModel;multiModel.resetDocument(multiple);
    const auto multiId=multiModel.createNativeVirtualConnection({"left","any-identified-transition","right"},false,error);
    check("re_can_connect_without_splitting_or_baking_da_re_owner",multiId.isNotEmpty()&&multiModel.snapshot().tracks[0].clips[0].notes.size()==2&&multiModel.snapshot().tracks[0].clips[0].id==multiOwner.id);
    if(multiId.isNotEmpty())
    {
        juce::AudioBuffer<float> originalMulti,groupMulti;const bool real=render(multiple,originalMulti)&&render(multiModel.snapshot(),groupMulti);
        bool daExact=real;for(int ch=0;daExact&&ch<2;++ch)daExact&=std::equal(originalMulti.getReadPointer(ch),originalMulti.getReadPointer(ch)+28800,groupMulti.getReadPointer(ch));
        check("unselected_da_PCM_bit_exact_outside_virtual_member_support",daExact);
        NativeVirtualConnectionPlan multiPlan;const bool planned=planNativeVirtualConnection(multiple.tracks[0],{"left","any-identified-transition","right"},multiPlan,error);
        std::array<juce::AudioBuffer<float>,3> multiSolos;bool solosReady=true;
        for(int i=0;i<3;++i){auto one=multiple;one.tracks[0].clips={multiple.tracks[0].clips[std::size_t(i)]};solosReady&=render(one,multiSolos[std::size_t(i)]);}
        bool scoped=real&&planned&&solosReady;
        for(int s=0;scoped&&s<96000;++s)
        {
            const auto time=s/48000.;auto w=nativeVirtualContributions(multiPlan,time);
            // da isn't in this group: its existing source is unity, not zero.
            if(time<.6)w[0]=1.;
            for(int ch=0;ch<2;++ch)
            {double sum=0;for(int i=0;i<3;++i)sum+=w[std::size_t(i)]*multiSolos[std::size_t(i)].getSample(ch,s);scoped&=std::abs(groupMulti.getSample(ch,s)-sum)<2.e-6;}
        }
        check("mixed_owner_virtual_weight_is_scoped_to_re_every_sample",scoped);
        multiModel.setNativeVirtualConnectionEnabled(multiId,false,error);juce::AudioBuffer<float> off;bool offExact=real&&render(multiModel.snapshot(),off);
        for(int ch=0;offExact&&ch<2;++ch)offExact&=std::equal(originalMulti.getReadPointer(ch),originalMulti.getReadPointer(ch)+96000,off.getReadPointer(ch));
        check("multi_note_owner_disable_restores_original_PCM_exact",offExact);
    }
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
