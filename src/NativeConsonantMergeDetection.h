#pragma once
#include "NativeConsonantMergePlan.h"
#include "NativeSplitTransitionDetection.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
namespace hachi
{
struct NativeConsonantMergeCandidate{NativeConsonantMergeEvidence evidence;float confidence=0;};
// Conservative source-noise evidence, not a phoneme recognizer. Native kinds,
// identified logical bridge halves, original graph continuity, source timing
// and explicit F0 observations are checked BEFORE this short PCM inspection.
inline std::vector<NativeConsonantMergeCandidate> detectMelodyneConsonantMerges(const ProjectData& project,
    const std::vector<NativeSplitTransitionEvidence>& joins,const std::set<juce::String>& protectedHalves={})
{
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::map<juce::String,std::unique_ptr<juce::AudioFormatReader>> readers;
    std::vector<NativeConsonantMergeCandidate> result;
    for(const auto& join:joins)
    {
        if(protectedHalves.count(join.frontNoteId)||protectedHalves.count(join.backNoteId))continue;
        NativeConsonantMergeEvidence evidence{join,true};NativeConsonantMergePlan plan;juce::String error;
        if(!planNativeConsonantMerge(project,evidence,plan,error))continue;
        const auto& onset=plan.onsetOriginal;const auto& note=onset.notes[0];
        const auto clock=nativeClipClock(onset);const auto from=onset.sourceOffsetSeconds+nativeSourceTimeAt(clock,0.);
        const auto to=plan.preutteranceSourceSeconds,span=to-from;
        if(!std::isfinite(span)||span<.004||span>.5)continue;
        auto [it,inserted]=readers.try_emplace(onset.sourceFile.getFullPathName());if(inserted)it->second.reset(formats.createReaderFor(onset.sourceFile));auto* reader=it->second.get();
        if(!reader||reader->sampleRate<8000||reader->numChannels==0)continue;
        const auto first=juce::int64(std::floor(from*reader->sampleRate)),last=juce::int64(std::ceil(to*reader->sampleRate));
        if(first<0||last>reader->lengthInSamples||last-first<64||last-first>reader->sampleRate*.51)continue;
        juce::AudioBuffer<float> pcm(int(reader->numChannels),int(last-first));if(!reader->read(&pcm,0,pcm.getNumSamples(),first,true,true))continue;
        const auto alpha=std::exp(-juce::MathConstants<double>::twoPi*1500./reader->sampleRate);
        double power=0,highPower=0;int crossings=0;
        for(int ch=0;ch<pcm.getNumChannels();++ch)
        {
            double mean=0;for(int s=0;s<pcm.getNumSamples();++s)mean+=pcm.getSample(ch,s);mean/=pcm.getNumSamples();
            double previous=pcm.getSample(ch,0)-mean,high=0;
            for(int s=1;s<pcm.getNumSamples();++s)
            {const auto sample=pcm.getSample(ch,s)-mean;high=alpha*(high+sample-previous);power+=sample*sample;highPower+=high*high;if((sample>=0)!=(previous>=0))++crossings;previous=sample;}
        }
        const auto samples=std::max(1,(pcm.getNumSamples()-1)*pcm.getNumChannels());
        const auto rms=std::sqrt(power/samples),ratio=highPower/std::max(1.e-20,power),rate=crossings*reader->sampleRate/samples;
        bool marker=false;for(const auto time:note.sibilantMarkers)if(std::isfinite(time)&&time>=0&&time<=note.durationSeconds)marker=true;
        // A Melodyne sibilant marker is supporting evidence only. Without one,
        // require substantially stronger high-frequency, high-crossing noise.
        const bool noisy=marker?(ratio>=.28&&rate>=2500):(ratio>=.52&&rate>=6000);
        if(rms<.001||!noisy)continue;
        double periodicity=0;const int minimumLag=std::max(2,juce::roundToInt(reader->sampleRate*.001));
        const int maximumLag=std::min(pcm.getNumSamples()/3,juce::roundToInt(reader->sampleRate*.016));
        const int lagStep=std::max(1,(maximumLag-minimumLag)/192);
        for(int lag=minimumLag;lag<=maximumLag;lag+=lagStep)
        {
            double product=0,a=0,b=0;
            for(int s=lag;s<pcm.getNumSamples();s+=2)
            {const double x=pcm.getSample(0,s),y=pcm.getSample(0,s-lag);product+=x*y;a+=x*x;b+=y*y;}
            if(a>1.e-20&&b>1.e-20)periodicity=std::max(periodicity,product/std::sqrt(a*b));
        }
        if(periodicity>.80)continue; // missing F0 on a whistle/tone is not noise evidence
        result.push_back({evidence,float(std::clamp(.5+ratio*.4+(marker?.08:0),.5,.98))});
    }
    std::map<juce::String,int> membership;for(const auto& c:result){++membership[c.evidence.sourceJoin.frontNoteId];++membership[c.evidence.sourceJoin.backNoteId];}
    std::erase_if(result,[&](const auto& c){return membership[c.evidence.sourceJoin.frontNoteId]!=1||membership[c.evidence.sourceJoin.backNoteId]!=1;});return result;
}
struct NativeConsonantAutoMergeReport{int merged=0;juce::StringArray skipped;};
inline NativeConsonantAutoMergeReport autoMergeMelodyneConsonants(ProjectData& project,
    const std::vector<NativeSplitTransitionEvidence>& joins,bool enabled=false)
{
    NativeConsonantAutoMergeReport report;if(!enabled)return report;
    // Protect split bridges even if the user disabled publishing new transition
    // annotations: an optimization toggle must not reinterpret their back half.
    std::set<juce::String> protectedHalves;
    for(const auto& c:detectMelodyneSplitTransitions(project,joins))
    {protectedHalves.insert(c.evidence.frontNoteId);protectedHalves.insert(c.evidence.backNoteId);}
    const auto candidates=detectMelodyneConsonantMerges(project,joins,protectedHalves);
    if(candidates.empty())return report;ProjectModel staged;staged.resetDocument(project);
    for(const auto& candidate:candidates)
    {juce::String error;if(staged.mergeMelodyneConsonant(candidate.evidence,error).isNotEmpty())++report.merged;else if(error.isNotEmpty())report.skipped.add(error);}
    if(report.merged>0)project=staged.snapshot();return report;
}
}
