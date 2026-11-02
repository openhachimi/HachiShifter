#pragma once
#include "../AudioEngine.h"
#include "../Mld5ContinuousSource.h"
#include <iostream>
namespace hachi
{
inline bool runMld5ContinuousAudioSmoke(const juce::File& root)
{
    root.createDirectory();bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log<<name<<"="<<(pass?"PASS":"FAIL")<<" "<<error<<"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto file=root.getChildFile("low-level-common-source.wav");
    {juce::AudioBuffer<float> pcm(1,48000);for(int i=0;i<48000;++i){double value=0;for(int h=1;h<=5;++h)value+=.015/h*std::sin(2*juce::MathConstants<double>::pi*261.6255653*h*i/48000);pcm.setSample(0,i,float(value));}juce::WavAudioFormat wav;auto stream=file.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,48000))return false;}
    const auto clip=[&](const char* id,double from,double end)
    {
        ClipData c;c.id=id;c.sourceFile=file;c.startSeconds=from;c.sourceOffsetSeconds=from;c.durationSeconds=c.sourceDurationSeconds=end-from;c.materialSourceUsageGroupId="original-source-pass";c.sourceTimeMap={{0,0},{c.durationSeconds,c.durationSeconds}};
        NoteData n;n.id=juce::String(id)+"-note";n.label="a";n.midiNote=65;n.sourceMidiCenter=60;n.sourcePitchMeasured=true;n.durationSeconds=c.durationSeconds;n.contour={{0,0,0,true},{n.durationSeconds,0,0,true}};c.notes={n};return c;
    };
    TrackData track;track.id="MLD5";track.trackType=TrackType::hachi;track.compose=true;track.pitchAlgorithm=PitchAlgorithm::mld5;track.renderOrder=RenderOrder::processThenSplice;track.mergeContiguousSource=true;track.mld5RenderPolicyInitialized=true;track.normalizeVolume=false;track.smoothOverlaps=false;track.volume=1;
    const auto render=[&](std::vector<ClipData> clips,juce::AudioBuffer<float>& out,int& phrases,bool merge=true,RenderOrder order=RenderOrder::processThenSplice)
    {
        auto t=track;t.clips=std::move(clips);t.mergeContiguousSource=merge;t.renderOrder=order;ProjectData p;p.tracks={t};
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(p);const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000.;
        while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto progress=engine.renderProgress();if(!progress||*progress>=.999)break;juce::Thread::sleep(5);}
        if(const auto progress=engine.renderProgress();progress&&*progress<.999){error="MLD5 continuous render timeout";return false;}
        if(const auto warnings=engine.activeRenderWarnings();warnings.isNotEmpty()){error=warnings;return false;}
        phrases=engine.diagnosticMergedPhraseCount();out.setSize(2,48002);out.clear();engine.setPosition(0);engine.play();for(int i=0;i<out.getNumSamples();i+=257)engine.getNextAudioBlock({&out,i,std::min(257,out.getNumSamples()-i)});engine.stop();return out.getMagnitude(0,out.getNumSamples())>1.e-6f;
    };
    const auto difference=[](const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,int from=480,int to=47520,float gain=1.f)
    {if(a.getNumSamples()<to||b.getNumSamples()<to)return 100.;double worst=0;for(int ch=0;ch<2;++ch)for(int i=from;i<to;++i)worst=std::max(worst,std::abs(double(a.getSample(ch,i)-b.getSample(ch,i)*gain)));return worst;};
    auto whole=clip("whole",0,1);whole.sourceTimeMap={{0,0},{.49,.49},{.51,.51},{1,1}};
    auto a=clip("a",0,.51),b=clip("b",.49,1);juce::AudioBuffer<float> reference,shared,independent,withFades,half;
    int singleCount=0,sharedCount=0,independentCount=0,otherCount=0;
    check("low_level_full_source_reference_renders",render({whole},reference,singleCount));
    check("overlap_uses_one_recovered_continuous_source_session",render({a,b},shared,sharedCount)&&sharedCount==1);
    const auto overlapError=difference(shared,reference);log+="overlap_single_pass_max_error="+juce::String(overlapError,12)+"\n";
    check("overlap_reassembly_matches_one_pass_without_plus6dB_or_phase_reset",overlapError<2.e-6);
    check("legacy_per_note_order_remains_explicitly_selectable",render({a,b},independent,independentCount,false)&&independentCount==0);
    const auto legacyError=difference(independent,reference,22560,25440);log+="per_note_seam_error="+juce::String(legacyError,12)+"\n";
    check("default_continuous_session_removes_old_seam_residual",legacyError>overlapError+1.e-5);
    a.crossfadeOutSeconds=.02;b.crossfadeInSeconds=.02;
    check("authored_complementary_fades_survive_group_slicing",render({a,b},withFades,otherCount)&&difference(withFades,reference)<2.e-6);
    {
        auto left=clip("touch-left",0,.5),right=clip("touch-right",.5,1),full=whole;full.sourceTimeMap={{0,0},{.5,.5},{1,1}};
        left.crossfadeOutSeconds=right.crossfadeInSeconds=.005;left.glideConnectedToNext=right.glideConnectedFromPrevious=true;
        juce::AudioBuffer<float> expected,actual,authorFades;int sessions=0;const bool complete=render({full},expected,sessions)&&render({left,right},actual,sessions);
        const auto errorAtJoin=difference(actual,expected,23520,24480);log+="touching_join_PCM_error="+juce::String(errorAtJoin,12)+"\n";
        check("same_stream_touching_successive_join_does_not_fade_to_silence",complete&&sessions==1&&errorAtJoin<2.e-6);
        left.fadeOutSeconds=right.fadeInSeconds=.005;
        check("explicit_author_fades_remain_separate_from_redundant_join_weights",render({left,right},authorFades,sessions)&&difference(authorFades,expected,23520,24480)>1.e-5);
    }
    a.crossfadeOutSeconds=0;b.crossfadeInSeconds=0;a.gain=.5;b.gain=.5;
    check("author_clip_gain_is_applied_once_not_cancelled_or_squared",render({a,b},half,otherCount)&&difference(half,reference,480,47520,.5f)<2.e-6);
    a.gain=1;b.gain=1;b.materialSourceUsageGroupId="separate-source-pass";
    juce::AudioBuffer<float> separate;check("independent_same_WAV_instances_are_not_silently_merged",render({a,b},separate,otherCount)&&otherCount==0);
    b.materialSourceUsageGroupId=a.materialSourceUsageGroupId;
    for(double fraction:{.25,.5,.75})
    {
        auto x=a,y=b,z=whole;const auto shift=fraction/48000.;x.startSeconds+=shift;y.startSeconds+=shift;z.startSeconds+=shift;
        juce::AudioBuffer<float> expected,actual;int n=0;const bool rendered=render({z},expected,n)&&render({x,y},actual,n);const auto delta=difference(actual,expected);
        log+="fraction_"+juce::String(fraction)+"_max_error="+juce::String(delta,12)+"\n";
        check("fractional_clip_boundaries_keep_common_phase_and_single_coverage",rendered&&n==1&&delta<2.e-6);
    }
    // One compatible source component uses the same native S/dS/R core in both
    // layouts. There must be no extra Signalsmith/RMS/window cascade in order2.
    auto nonlinear=whole;nonlinear.sourceTimeMap={{0,0},{.3,.2},{.7,.8},{1,1}};
    juce::AudioBuffer<float> sourceFirst,spliceFirst;int count=0;const bool sourceOk=render({nonlinear},sourceFirst,count,true);
    const bool spliceOk=render({nonlinear},spliceFirst,count,false,RenderOrder::stretchSpliceThenPitch);
    const auto orderDelta=difference(sourceFirst,spliceFirst);log+="nonlinear_order_PCM_delta="+juce::String(orderDelta,12)+"\n";
    check("single_component_native_layouts_do_not_add_a_second_spectral_stage",sourceOk&&spliceOk&&orderDelta<2.e-6);
    root.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
