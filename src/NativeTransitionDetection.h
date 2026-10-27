#pragma once
#include "NativeNoteTiming.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <map>

namespace hachi
{
struct NativeTransitionCandidate
{
    juce::String noteId;
    NativeTransitionShape shape;
    float confidence=0;
};
// Conservative source-energy classification, not phoneme/F0 inference. The
// caller must be importing a Melodyne project; filenames never decide a type.
inline std::vector<NativeTransitionCandidate> detectMelodyneTransitions(const ProjectData& project)
{
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::map<juce::String,std::unique_ptr<juce::AudioFormatReader>> readers;
    std::vector<NativeTransitionCandidate> result;
    for(const auto& track:project.tracks)for(const auto& parent:track.clips)for(const auto& note:parent.notes)
    {
        if(note.pronunciationKind!=PronunciationKind::normal||note.nativeTransition||note.materialLocalOverride
            ||note.connectedToPrevious||note.connectedToNext||note.melodyneConsonantCandidate
            ||note.durationSeconds<.008||note.durationSeconds>.20)continue;
        bool joined=false;
        for(const auto& connection:project.nativeConnections)if(connection.leftNoteId==note.id||connection.rightNoteId==note.id){joined=true;break;}
        if(joined)continue;
        const ClipData* owner=parent.parts.empty()?&parent:nullptr;
        if(!parent.parts.empty())for(const auto& part:parent.parts)if(part.id==note.clipPartId){if(owner){owner=nullptr;break;}owner=&part;}
        if(!owner||owner->sourceFile==juce::File{})continue;
        if(owner->glideConnectedFromPrevious||owner->glideConnectedToNext)continue;
        const auto offset=parent.parts.empty()?0.:owner->startSeconds;
        // Actual overlap/joins are not a completely cut element. Independent
        // source owners can still carry the imported amplitude crossfade.
        for(const auto& other:parent.notes)if(other.id!=note.id&&other.clipPartId==note.clipPartId
            &&std::max(other.startSeconds,note.startSeconds)<std::min(other.startSeconds+other.durationSeconds,note.startSeconds+note.durationSeconds)-1.e-7){joined=true;break;}
        if(joined)continue;
        const auto clock=nativeClipClock(*owner);
        const auto begin=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,note.startSeconds-offset);
        const auto end=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,note.startSeconds-offset+note.durationSeconds);
        const auto span=end-begin;if(!std::isfinite(begin)||!std::isfinite(end)||begin<0||span<.008||span>.22)continue;
        const auto key=owner->sourceFile.getFullPathName();auto [it,inserted]=readers.try_emplace(key);
        if(inserted)it->second.reset(formats.createReaderFor(owner->sourceFile));auto* reader=it->second.get();
        if(!reader||reader->sampleRate<=0||reader->numChannels==0)continue;
        const auto first=juce::int64(std::floor(begin*reader->sampleRate));const auto length=juce::int64(std::ceil(end*reader->sampleRate))-first;
        if(first<0||length<64||first+length>reader->lengthInSamples||length>reader->sampleRate*.23)continue;
        const auto count=int(length);
        juce::AudioBuffer<float> pcm(int(reader->numChannels),count);if(!reader->read(&pcm,0,count,first,true,true))continue;
        constexpr int bins=32;std::array<double,bins> power{};
        for(int i=0;i<bins;++i){const int from=i*count/bins,to=(i+1)*count/bins;double sum=0;for(int c=0;c<pcm.getNumChannels();++c)for(int s=from;s<to;++s){const auto value=pcm.getSample(c,s);sum+=double(value)*value;}power[std::size_t(i)]=sum/std::max(1,(to-from)*pcm.getNumChannels());}
        double left=0,right=0;for(int i=0;i<8;++i){left+=power[std::size_t(i)];right+=power[std::size_t(31-i)];}left/=8;right/=8;
        const auto side=std::min(left,right);if(side<1.e-7||std::max(left,right)>side*9)continue;
        int valley=-1;double lowest=1.e9;
        for(int i=10;i<=21;++i){double mean=0;for(int d=-2;d<=2;++d)mean+=power[std::size_t(i+d)];mean/=5;if(mean<lowest){lowest=mean;valley=i;}}
        if(valley<0||lowest>side*.25)continue; // both shoulders >=6dB above a sustained middle valley
        int voiced=0;float low=1.e9f,high=-1.e9f;
        for(const auto& p:note.contour)if(p.voiced){++voiced;low=std::min(low,p.relativeCents);high=std::max(high,p.relativeCents);}
        // A fully voiced, stable short vowel is ambiguous; do not relabel it
        // just because it has an amplitude dip. Unreliable F0 is NOT required.
        if(!note.contour.empty()&&voiced>int(note.contour.size()*.9)&&high-low<25)continue;
        const auto boundary=begin+span*(valley+.5)/bins;
        const auto confidence=float(juce::jlimit(.5,.98,1-lowest/side));
        result.push_back({note.id,{1,boundary,"melodyne-short-energy-valley-v1"},confidence});
    }
    return result;
}
// Identity annotation ONLY: do not canonicalize the imported render snapshot's
// segments, consonant timing, F0, gain, fades, source clock or edge activation.
inline int annotateMelodyneTransitions(ProjectData& project,const std::vector<NativeTransitionCandidate>& candidates)
{
    int count=0;for(auto& track:project.tracks)for(auto& clip:track.clips)for(auto& note:clip.notes)
        for(const auto& candidate:candidates)if(candidate.noteId==note.id)
        {note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=candidate.shape;++count;break;}
    return count;
}
}
