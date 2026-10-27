#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
// Does the generated glide actually BEND the rendered audio, and only where it
// should? Request-level tests cannot answer that; this measures real output F0.
inline bool runNativeVirtualGlideAudioSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    // Two different recordings at steady, clearly different pitches.
    const std::array<double,2> hz{220.,330.};
    for(int i=0;i<2;++i)
    {
        const auto file=output.getChildFile(i?"high.wav":"low.wav");
        // A pure sine is a poor subject for vocoder analysis; use a voice-like
        // harmonic stack with a formant-ish envelope instead.
        juce::AudioBuffer<float> pcm(1,96000);double phase=0;
        for(int s=0;s<pcm.getNumSamples();++s)
        {
            phase+=juce::MathConstants<double>::twoPi*hz[std::size_t(i)]/48000.;
            double value=0;
            for(int h=1;h<=12;++h)value+=std::sin(phase*h)/h*std::exp(-h*.22);
            pcm.setSample(0,s,float(.25*value));
        }
        auto stream=file.createOutputStream();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));
        if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;
    }
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;
    track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;
    const std::array<float,2> midi{57.f,64.f}; // A3 and E4, matching the recordings
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
    const auto render=[&](const ProjectData& data,juce::AudioBuffer<float>& result)
    {
        AudioEngine engine(false);engine.prepareToPlay(512,48000);engine.syncProject(data);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+90'000.;
        while(juce::Time::getMillisecondCounterHiRes()<deadline)
        {const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(10);}
        if(const auto p=engine.renderProgress();p&&*p<.999){error="Glide fixture render timed out";return false;}
        const auto warning=engine.activeRenderWarnings();if(warning.isNotEmpty()){error=warning;return false;}
        const int count=48000*2;result.setSize(2,count);result.clear();engine.setPosition(0);engine.play();
        for(int offset=0;offset<count;offset+=512)engine.getNextAudioBlock({&result,offset,std::min(512,count-offset)});
        engine.stop();return true;
    };
    // Autocorrelation F0 over a short window of the rendered mix.
    const auto measure=[](const juce::AudioBuffer<float>& buffer,double centre)->double
    {
        const int window=2048,start=juce::roundToInt(centre*48000.)-window/2;
        if(start<0||start+window>=buffer.getNumSamples())return 0;
        std::vector<double> x(static_cast<std::size_t>(window),0.);double mean=0;
        for(int i=0;i<window;++i){x[std::size_t(i)]=buffer.getSample(0,start+i);mean+=x[std::size_t(i)];}
        mean/=window;double energy=0;
        for(auto& v:x){v-=mean;energy+=v*v;}
        if(energy<1.e-9)return 0;
        double bestScore=0;int bestLag=0;
        for(int lag=48000/700;lag<=48000/70;++lag)
        {
            double sum=0,norm=0;
            for(int i=lag;i<window;++i){sum+=x[std::size_t(i)]*x[std::size_t(i-lag)];norm+=x[std::size_t(i-lag)]*x[std::size_t(i-lag)];}
            const auto score=norm>1.e-12?sum/std::sqrt(norm*energy):0;
            if(score>bestScore){bestScore=score;bestLag=lag;}
        }
        return bestLag>0&&bestScore>.3?48000./bestLag:0;
    };
    const auto toMidi=[](double f){return f>0?69.+12.*std::log2(f/440.):0.;};
    juce::AudioBuffer<float> plain;
    check("baseline_two_source_render",render(seed,plain));
    const auto leftBase=toMidi(measure(plain,.5)),rightBase=toMidi(measure(plain,1.5));
    std::cout<<"baseline_left_midi="<<leftBase<<" right_midi="<<rightBase<<std::endl;
    check("baseline_sources_sound_at_their_own_pitches",
        std::abs(leftBase-57.)<1.&&std::abs(rightBase-64.)<1.);
    // Control: a plain transpose with no connection at all. If this is also
    // erratic, the fault is in the stretch/render path, not the glide.
    {
        auto transposed=seed;transposed.tracks[0].clips[0].notes[0].midiNote=59.f;
        juce::AudioBuffer<float> shifted;
        if(render(transposed,shifted))
        {
            std::cout<<"control_plain_transpose_t0.5="<<toMidi(measure(shifted,.5))
                <<" t0.7="<<toMidi(measure(shifted,.7))<<std::endl;
            log+="control_plain_transpose_midi="+juce::String(toMidi(measure(shifted,.5)),3)+"\n";
        }
    }
    ProjectModel model;model.resetDocument(seed);
    const auto group=model.createNativeVirtualConnection({"left","right"},true,error,.25);
    check("automatic_glide_connection_created",group.isNotEmpty());
    if(group.isEmpty()){output.getChildFile("verification.txt").replaceWithText(log);return false;}
    ProjectData projected;std::vector<NativeVirtualRenderGroup> groups;
    const auto prepared=prepareNativeVirtualRender(model.snapshot(),projected,groups,error);
    check("glide_curve_actually_generated",prepared&&groups.size()==1&&groups[0].pitch.active&&groups[0].pitch.points.size()>=2);
    if(prepared&&!groups.empty())
    {
        const auto resolved=resolvedNativeVirtualPitch(groups[0].pitch);
        std::cout<<"curve_points="<<groups[0].pitch.points.size()
            <<" affected="<<resolved.affectedNoteIds.size()<<std::endl;
        for(const auto& p:groups[0].pitch.points)
            std::cout<<"curve t="<<p.timeSeconds<<" midi="<<p.targetMidi<<std::endl;
    }
    juce::AudioBuffer<float> glided;
    check("glided_render_completed",render(model.snapshot(),glided));
    const auto rmsAt=[](const juce::AudioBuffer<float>& buffer,double centre)
    {
        const int window=2048,start=juce::roundToInt(centre*48000.)-window/2;
        if(start<0||start+window>=buffer.getNumSamples())return 0.;
        double sum=0;for(int i=0;i<window;++i){const auto v=buffer.getSample(0,start+i);sum+=double(v)*v;}
        return std::sqrt(sum/window);
    };
    for(double t=.25;t<1.75;t+=.1)
        std::cout<<"glide_scan t="<<t<<" baseline="<<toMidi(measure(plain,t))
            <<" glided="<<toMidi(measure(glided,t))
            <<" baseRms="<<rmsAt(plain,t)<<" glideRms="<<rmsAt(glided,t)<<std::endl;
    const auto mid=toMidi(measure(glided,.95));
    const auto before=toMidi(measure(glided,.5)),after=toMidi(measure(glided,1.5));
    std::cout<<"glided_before="<<before<<" mid="<<mid<<" after="<<after<<std::endl;
    log+="glided_before="+juce::String(before,3)+" mid="+juce::String(mid,3)+" after="+juce::String(after,3)+"\n";
    check("glide_audibly_bends_between_the_two_sources",
        mid>0&&mid>std::min(before,after)+.4&&mid<std::max(before,after)-.4);
    check("far_from_the_connection_the_sources_keep_their_own_pitch",
        std::abs(before-leftBase)<.6&&std::abs(after-rightBase)<.6);
    // Switching the group off must restore the original audio exactly.
    juce::AudioBuffer<float> off;
    model.setNativeVirtualConnectionEnabled(group,false,error);
    const auto offRendered=render(model.snapshot(),off);
    bool identical=offRendered&&plain.getNumSamples()==off.getNumSamples();
    for(int ch=0;identical&&ch<2;++ch)
        identical&=std::equal(plain.getReadPointer(ch),plain.getReadPointer(ch)+plain.getNumSamples(),off.getReadPointer(ch));
    check("disabled_glide_restores_original_PCM_bit_exact",identical);
    // Write A/B material so the optional glide can be judged by ear.
    const auto save=[&](const juce::AudioBuffer<float>& buffer,const char* name)
    {
        auto stream=output.getChildFile(name).createOutputStream();if(!stream)return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,2,24,{},0));
        return writer&&writer->writeFromAudioSampleBuffer(buffer,0,buffer.getNumSamples());
    };
    check("A_B_listening_material_written",
        save(plain,"A-glide-off.wav")&&save(glided,"B-glide-on.wav"));
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
