#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
// What you hear must be what you ship. Playback and exportWav are separate
// paths, so a connection/glide that only exists in playback is a real defect.
inline bool runNativeVirtualExportParitySmoke(const juce::File& output)
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
    // Render through playback AND through exportWav, from the same project.
    const auto renderBoth=[&](const ProjectData& data,juce::AudioBuffer<float>& played,
        juce::AudioBuffer<float>& exported,const juce::File& file)
    {
        AudioEngine engine(false);engine.prepareToPlay(512,48000);engine.syncProject(data);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+90'000.;
        while(juce::Time::getMillisecondCounterHiRes()<deadline)
        {const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(10);}
        if(const auto p=engine.renderProgress();p&&*p<.999){error="Export parity render timed out";return false;}
        const auto warning=engine.activeRenderWarnings();if(warning.isNotEmpty()){error=warning;return false;}
        const int count=48000*2;played.setSize(2,count);played.clear();
        engine.setPosition(0);engine.play();
        for(int offset=0;offset<count;offset+=512)engine.getNextAudioBlock({&played,offset,std::min(512,count-offset)});
        engine.stop();
        juce::String issue;if(!engine.exportWav(file,issue)){error=issue;return false;}
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if(!reader||reader->lengthInSamples<=0){error="Exported file is unreadable or empty";return false;}
        exported.setSize(int(reader->numChannels),int(std::min<juce::int64>(reader->lengthInSamples,count)));
        return reader->read(&exported,0,exported.getNumSamples(),0,true,true);
    };
    const auto measure=[](const juce::AudioBuffer<float>& buffer,double centre)->double
    {
        const int window=2048,start=juce::roundToInt(centre*48000.)-window/2;
        if(start<0||start+window>=buffer.getNumSamples())return 0;
        std::vector<double> x(static_cast<std::size_t>(window),0.);double mean=0;
        for(int i=0;i<window;++i){x[std::size_t(i)]=buffer.getSample(0,start+i);mean+=x[std::size_t(i)];}
        mean/=window;double energy=0;for(auto& v:x){v-=mean;energy+=v*v;}
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
    const auto compare=[&](const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,double& worst)
    {
        worst=0;const auto count=std::min(a.getNumSamples(),b.getNumSamples());
        if(count<48000)return false;
        for(int ch=0;ch<std::min(a.getNumChannels(),b.getNumChannels());++ch)
            for(int s=0;s<count;++s)worst=std::max(worst,double(std::abs(a.getSample(ch,s)-b.getSample(ch,s))));
        return true;
    };
    ProjectModel model;model.resetDocument(seed);
    const auto group=model.createNativeVirtualConnection({"left","right"},true,error,.25);
    check("connection_with_glide_created",group.isNotEmpty());
    if(group.isEmpty()){output.getChildFile("verification.txt").replaceWithText(log);return false;}
    juce::AudioBuffer<float> played,exported;
    check("connected_project_renders_and_exports",
        renderBoth(model.snapshot(),played,exported,output.getChildFile("connected-export.wav")));
    double worst=0;const auto comparable=compare(played,exported,worst);
    std::cout<<"connected_playback_vs_export_max_difference="<<worst<<std::endl;
    log+="connected_playback_vs_export_max_difference="+juce::String(worst,9)+"\n";
    check("export_matches_playback_sample_for_sample",comparable&&worst<1.e-6);
    const auto exportedMid=toMidi(measure(exported,.95));
    const auto exportedBefore=toMidi(measure(exported,.5)),exportedAfter=toMidi(measure(exported,1.5));
    std::cout<<"exported_before="<<exportedBefore<<" mid="<<exportedMid<<" after="<<exportedAfter<<std::endl;
    log+="exported_before="+juce::String(exportedBefore,3)+" mid="+juce::String(exportedMid,3)
        +" after="+juce::String(exportedAfter,3)+"\n";
    check("exported_file_really_contains_the_glide",
        exportedMid>0&&exportedMid>std::min(exportedBefore,exportedAfter)+.4
        &&exportedMid<std::max(exportedBefore,exportedAfter)-.4);
    // With the group disabled the export must equal the untouched original.
    juce::AudioBuffer<float> plainPlayed,plainExported;
    check("original_project_renders_and_exports",
        renderBoth(seed,plainPlayed,plainExported,output.getChildFile("original-export.wav")));
    model.setNativeVirtualConnectionEnabled(group,false,error);
    juce::AudioBuffer<float> offPlayed,offExported;
    check("disabled_project_renders_and_exports",
        renderBoth(model.snapshot(),offPlayed,offExported,output.getChildFile("disabled-export.wav")));
    double offWorst=0;const auto offComparable=compare(plainExported,offExported,offWorst);
    std::cout<<"disabled_vs_original_export_max_difference="<<offWorst<<std::endl;
    log+="disabled_vs_original_export_max_difference="+juce::String(offWorst,9)+"\n";
    check("disabled_group_exports_the_original_audio_bit_exact",offComparable&&offWorst==0.);
    double changed=0;compare(plainExported,exported,changed);
    check("enabled_export_is_genuinely_different_from_original",changed>1.e-4);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
