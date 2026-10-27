#pragma once
#include "../AudioEngine.h"
#include "../NativeSourceEdgePlan.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
namespace hachi
{
// Actual normal AudioEngine mixer/PCM checks; generated sources are not a voice,
// ASR or neural-quality oracle. No renderer/phonemizer approximation is used.
inline bool runNativeSourceEdgeMixerSmoke(const juce::File& output)
{
    const auto base=output.getChildFile("native-edge-mixer-"+juce::Uuid().toString());if(base.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";};
    const auto source=base.getChildFile("abc.wav");
    juce::AudioBuffer<float> pcm(1,144000);for(int i=0;i<144000;++i)pcm.setSample(0,i,.1f*std::sin(juce::MathConstants<double>::twoPi*173*i/48000.0));
    auto stream=source.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),48000,1,16,{},0):nullptr);
    check("physical_PCM_fixture",writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));writer.reset();if(!ok)return false;
    const auto sourceHash=juce::SHA256(source).toHexString();
    const auto trackFor=[&](int first,int count,bool profiles)
    {
        TrackData track;track.id="edge-track";track.trackType=TrackType::hachi;track.compose=true;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.normalizeVolume=false;track.smoothOverlaps=true;
        ClipData clip;clip.id="edge-clip";clip.sourceFile=source;clip.sourceOffsetSeconds=first;clip.sourceDurationSeconds=clip.durationSeconds=count;clip.sourceTimeMap={{0,0},{double(count),double(count)}};
        for(int j=0;j<count;++j)
        {
            const int i=first+j;NoteData note;note.id=juce::String::charToString(char('a'+i));note.label=note.id;note.startSeconds=j;note.durationSeconds=1;note.nativeUnpitched=true;
            note.materialAudioId="authored-registration";note.materialRegionId=note.id;note.materialSourceStartSeconds=note.nativeSourceStartSeconds=i;note.materialSourceEndSeconds=note.nativeSourceEndSeconds=i+1;
            if(i<2)note.materialConnectedNextRegionId=juce::String::charToString(char('b'+i));
            if(profiles){NativePronunciationEdges edge;edge.entryFromSourceSeconds=i;edge.entryToSourceSeconds=i+.125;edge.exitFromSourceSeconds=i+.75;edge.exitToSourceSeconds=edge.terminationSourceSeconds=i+1;edge.provenance="explicit-test-source-anchors";note.nativePronunciationEdges=edge;}
            clip.notes.push_back(note);
        }
        track.clips={clip};return track;
    };
    const auto curve=[](double x){x=juce::jlimit(0.0,1.0,x);return float(x*x*(3-2*x));};
    const auto wait=[&](AudioEngine& engine)
    {const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto progress=engine.renderProgress();if(!progress||*progress>=.999)return engine.activeRenderWarnings().isEmpty();juce::Thread::sleep(2);}return false;};
    const auto render=[&](TrackData track,juce::AudioBuffer<float>& result)
    {
        AudioEngine engine(false);engine.prepareToPlay(257,48000);ProjectData project;project.tracks={std::move(track)};engine.syncProject(project);
        if(!wait(engine))return false;const int samples=juce::roundToInt(project.tracks[0].clips[0].durationSeconds*48000);result.setSize(2,samples);result.clear();engine.setPosition(0);engine.play();
        for(int offset=0;offset<samples;offset+=257){const int count=std::min(257,samples-offset);engine.getNextAudioBlock({&result,offset,count});}
        engine.stop();return engine.activeRenderWarnings().isEmpty();
    };
    for(const auto selection: {std::pair{0,3},std::pair{0,2},std::pair{1,1}})
    {
        const auto track=trackFor(selection.first,selection.second,true);ProjectData project;project.tracks={track};AudioEngine engine(false);engine.syncProject(project);
        bool exact=true;const double duration=selection.second;
        for(int i=0;i<juce::roundToInt(duration*48000);++i)
        {
            const double t=i/48000.0;float expected=curve(t/.125);
            expected*=curve((duration-t)/.25);
            const auto actual=engine.diagnosticNativeSourceEdgeGain("edge-clip",t);exact&=actual&&std::abs(*actual-expected)<1.e-6f;
        }
        check(selection.second==3?"abc_exposed_a_entry_c_exit_internal_flat_every_sample":selection.second==2?"ab_recomputed_b_exit_every_sample":"b_single_both_exposed_edges_every_sample",exact);
        juce::AudioBuffer<float> before,after;const bool ready=render(trackFor(selection.first,selection.second,false),before)&&render(track,after);
        bool pcmExact=ready&&before.getNumSamples()==after.getNumSamples();
        for(int i=0;pcmExact&&i<after.getNumSamples();++i){const double t=i/48000.0;const float gain=curve(t/.125)*curve((duration-t)/.25);for(int c=0;c<2;++c)pcmExact&=std::abs(after.getSample(c,i)-before.getSample(c,i)*gain)<1.e-6f;}
        check(selection.second==3?"abc_real_mixer_PCM_matches_existing_fade_authority":selection.second==2?"ab_real_mixer_PCM_matches_existing_fade_authority":"b_real_mixer_PCM_matches_existing_fade_authority",pcmExact);
    }
    auto authored=trackFor(0,3,true);authored.clips[0].fadeInSeconds=.25;authored.clips[0].fadeOutSeconds=.5;authored.clips[0].crossfadeInSeconds=.5;
    {AudioEngine engine(false);ProjectData data;data.tracks={authored};engine.syncProject(data);const auto gain=engine.diagnosticNativeSourceEdgeGain("edge-clip",.0625);check("intentional_author_fades_multiplicative",gain&&std::abs(*gain-curve(.5)*curve(.25)*.125f)<1.e-7f);}
    // Three different physical sources, including a real transition identity:
    // explicit link labels never turn them into one source-continuous component.
    auto cross=trackFor(0,3,true);const auto original=cross.clips[0];cross.clips.clear();
    for(int i=0;i<3;++i){auto clip=original;clip.id="owner"+juce::String(i);clip.startSeconds=i;clip.durationSeconds=clip.sourceDurationSeconds=1;clip.sourceOffsetSeconds=i;clip.sourceTimeMap={{0,0},{1,1}};clip.notes={original.notes[std::size_t(i)]};clip.notes[0].startSeconds=0;clip.sourceFile=base.getChildFile(juce::String(i==0?"so_re":i==1?"e_g":"ga")+".wav");source.copyFileTo(clip.sourceFile);if(i==1)clip.notes[0].pronunciationKind=PronunciationKind::transition;cross.clips.push_back(std::move(clip));}
    {AudioEngine engine(false);ProjectData data;data.tracks={cross};engine.syncProject(data);bool exposed=true;for(int i=0;i<3;++i){const auto gain=engine.diagnosticNativeSourceEdgeGain("owner"+juce::String(i),.0625);exposed&=gain&&std::abs(*gain-.5f)<1.e-7f;}check("different_physical_transition_sources_keep_each_exposed_entry",exposed);}
    auto invalid=trackFor(0,3,true);invalid.clips[0].notes[0].nativePronunciationEdges->terminationSourceSeconds=4;
    {AudioEngine engine(false);ProjectData data;data.tracks={invalid};engine.syncProject(data);check("invalid_profile_refused_without_legacy_fallback",!engine.diagnosticNativeSourceEdgeGain("edge-clip",.5)&&engine.activeRenderWarnings().isNotEmpty());}
    auto old=trackFor(0,3,false);juce::AudioBuffer<float> oldA,oldB;check("profile_absent_repeat_PCM_identical_no_historical_oracle",render(old,oldA)&&render(old,oldB)&&oldA.getNumSamples()==oldB.getNumSamples()&&std::equal(oldA.getReadPointer(0),oldA.getReadPointer(0)+oldA.getNumSamples(),oldB.getReadPointer(0)));
    {ProjectModel legacy;ProjectData data;data.tracks={old};legacy.resetDocument(data);check("profile_absent_historical_comparison_fixture",legacy.save(base.getChildFile("legacy-absent.hjpx"),error));}
    check("source_PCM_bytes_unchanged",juce::SHA256(source).toHexString()==sourceHash);
    const auto written=base.getChildFile("verification.txt").replaceWithText(log);std::cout<<"native_source_edge_mixer="<<ok<<"|fixture="<<base.getFullPathName()<<std::endl;if(!ok)std::cerr<<log<<std::endl;return ok&&written;
}
}
