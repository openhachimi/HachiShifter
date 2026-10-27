#pragma once
#include "NativeSplitTransitionPlan.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>

namespace hachi
{
struct NativeSplitTransitionCandidate
{
    std::vector<juce::String> orderedNoteIds;
    NativeSplitTransitionEvidence evidence;
    double middleSourceSeconds=0;
    float confidence=0;
    double sourceFromSeconds=0,sourceToSeconds=0;
};
// Import-only conservative classifier. Raw graph continuity is necessary but
// NOT sufficient: a candidate also needs vowel-like leading evidence, a short
// continuous source pair with a real cut-centred energy valley, and exactly one
// valid left/right normal-pronunciation context. Names never decide identity.
inline std::vector<NativeSplitTransitionCandidate> detectMelodyneSplitTransitions(const ProjectData& project,
    const std::vector<NativeSplitTransitionEvidence>& rawJoins)
{
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::map<juce::String,std::unique_ptr<juce::AudioFormatReader>> readers;
    std::vector<NativeSplitTransitionCandidate> result;
    const auto stableVoiced=[](const NoteData& note)
    {
        if(note.nativeUnpitched)return false;
        for(std::size_t i=1;i<note.contour.size();++i)
        {const auto& a=note.contour[i-1];const auto& b=note.contour[i];if(a.voiced&&b.voiced&&std::isfinite(a.timeSeconds)&&std::isfinite(b.timeSeconds)&&b.timeSeconds-a.timeSeconds>=.001&&b.timeSeconds-a.timeSeconds<=.03)return true;}
        return false;
    };
    for(const auto& track:project.tracks)
    {
        if(!trackShowsAllNativeRegions(track)||!track.compose||track.accompaniment)continue;
        struct Endpoint{const NoteData* note;const ClipData* parent;double from,to;};std::vector<Endpoint> placed;
        for(const auto& parent:track.clips)for(const auto& note:parent.notes)placed.push_back({&note,&parent,parent.startSeconds+note.startSeconds,parent.startSeconds+note.startSeconds+note.durationSeconds});
        for(const auto& raw:rawJoins)
        {
            if(!raw.joinsPitches||!raw.joinsAmplitudes||!raw.joinsPhases)continue;
            const Endpoint *front=nullptr,*back=nullptr;bool ambiguous=false;
            for(const auto& p:placed){if(p.note->id==raw.frontNoteId){ambiguous|=front!=nullptr;front=&p;}if(p.note->id==raw.backNoteId){ambiguous|=back!=nullptr;back=&p;}}
            if(ambiguous||!front||!back||!stableVoiced(*front->note)||front->note->pronunciationKind!=PronunciationKind::normal||back->note->pronunciationKind!=PronunciationKind::normal
                ||front->note->materialLocalOverride||back->note->materialLocalOverride||front->note->melodyneConsonantCandidate||back->note->melodyneConsonantCandidate)continue;
            const auto duration=back->to-front->from;
            if(front->to-front->from<.004||back->to-back->from<.004||duration<.016||duration>.22||std::abs(front->to-back->from)>1.e-6)continue;
            // Stable voiced-vowel + stable same-pitch vowel is not a reliable
            // transition merely because its volume dips at a source join.
            int all=0,voiced=0;float low=1.e9f,high=-1.e9f;
            for(const auto* note:{front->note,back->note})for(const auto& p:note->contour)
            {++all;if(p.voiced){++voiced;const auto midi=(note->sourceMidiCenter>=0?note->sourceMidiCenter:note->midiNote)+p.relativeCents/100;low=std::min(low,midi);high=std::max(high,midi);}}
            if(all>0&&voiced>int(all*.9)&&high-low<.25f)continue;
            std::vector<const Endpoint*> lefts,rights;
            for(const auto& p:placed)
            {
                if(p.note==front->note||p.note==back->note||p.note->pronunciationKind!=PronunciationKind::normal||!stableVoiced(*p.note))continue;
                if(p.from<front->from-1.e-7&&p.to>=front->from-1.e-7)lefts.push_back(&p);
                if(p.to>back->to+1.e-7&&p.from<=back->to+1.e-7)rights.push_back(&p);
            }
            // Bound candidate work and fail closed rather than choosing the
            // nearest of many potentially polyphonic contexts.
            if(lefts.empty()||rights.empty()||lefts.size()>4||rights.size()>4)continue;
            auto evidence=raw;evidence.transitionIdentified=true;
            std::vector<NativeSplitTransitionCandidate> valid;
            for(const auto* left:lefts)for(const auto* right:rights)
            {
                if(left==right)continue;
                NativeSplitTransitionPlan plan;juce::String error;
                if(!planNativeSplitTransition(track,left->note->id,evidence,right->note->id,plan,error))continue;
                const auto& a=plan.physical[1];const auto& b=plan.physical[2];
                const ClipData* owner=front->parent->parts.empty()?front->parent:nullptr;
                for(const auto& part:front->parent->parts)if(part.id==a.ownerId)owner=&part;
                if(!owner)continue;
                const auto offset=front->parent->parts.empty()?0.:owner->startSeconds;
                const auto clock=nativeClipClock(*owner);
                const auto begin=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,a.from-front->parent->startSeconds-offset);
                const ClipData* backOwner=back->parent->parts.empty()?back->parent:nullptr;
                for(const auto& part:back->parent->parts)if(part.id==b.ownerId)backOwner=&part;
                if(!backOwner)continue;
                const auto backOffset=back->parent->parts.empty()?0.:backOwner->startSeconds;
                const auto end=backOwner->sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(*backOwner),b.to-back->parent->startSeconds-backOffset);
                const auto span=end-begin,middle=plan.middleSourceSeconds;
                if(!std::isfinite(span)||span<.016||span>.23||middle<=begin+span*.15||middle>=end-span*.15)continue;
                auto [it,inserted]=readers.try_emplace(a.source.getFullPathName());if(inserted)it->second.reset(formats.createReaderFor(a.source));auto* reader=it->second.get();
                if(!reader||reader->sampleRate<=0||reader->numChannels==0)continue;
                const auto first=juce::int64(std::floor(begin*reader->sampleRate)),last=juce::int64(std::ceil(end*reader->sampleRate));
                if(first<0||last>reader->lengthInSamples||last-first<64||last-first>reader->sampleRate*.24)continue;
                juce::AudioBuffer<float> pcm(int(reader->numChannels),int(last-first));if(!reader->read(&pcm,0,pcm.getNumSamples(),first,true,true))continue;
                const auto power=[&](int from,int to)
                {from=std::clamp(from,0,pcm.getNumSamples());to=std::clamp(to,0,pcm.getNumSamples());double sum=0;for(int ch=0;ch<pcm.getNumChannels();++ch)for(int s=from;s<to;++s){const auto v=pcm.getSample(ch,s);sum+=double(v)*v;}return sum/std::max(1,(to-from)*pcm.getNumChannels());};
                const auto samples=pcm.getNumSamples(),edge=std::max(8,samples/5);const auto centre=juce::roundToInt((middle-begin)*reader->sampleRate),radius=std::max(4,std::min(samples/12,juce::roundToInt(reader->sampleRate*.008)));
                const auto shoulder=std::min(power(0,edge),power(samples-edge,samples));const auto valley=power(centre-radius,centre+radius);
                if(shoulder<1.e-7||valley>shoulder*.25)continue;
                valid.push_back({{left->note->id,raw.frontNoteId,raw.backNoteId,right->note->id},evidence,middle,float(std::clamp(1.-valley/shoulder,.5,.98)),begin,end});
            }
            if(valid.size()==1)result.push_back(std::move(valid.front()));
        }
    }
    // A physical half cannot belong to two independently inferred logical
    // bridges. Leave overlapping hypotheses unresolved for manual inspection.
    std::map<juce::String,int> membership;for(const auto& candidate:result){++membership[candidate.evidence.frontNoteId];++membership[candidate.evidence.backNoteId];}
    std::erase_if(result,[&](const auto& candidate){return membership[candidate.evidence.frontNoteId]!=1||membership[candidate.evidence.backNoteId]!=1;});
    return result;
}
// Add source-annotation identity only. Do not change either physical note's
// kind/P/C/UV/clock: a full logical middle lies ON the edge of each half and
// would be invalid as two independent NativeTransitionShape records.
inline int annotateMelodyneSplitTransitions(ProjectData& project,const std::vector<NativeSplitTransitionCandidate>& candidates)
{
    int count=0;
    for(const auto& candidate:candidates)
    {
        if(!candidate.evidence.transitionIdentified)continue;
        const auto exists=std::any_of(project.logicalTransitions.begin(),project.logicalTransitions.end(),[&](const auto& row)
        {return row.evidence.frontNoteId==candidate.evidence.frontNoteId||row.evidence.backNoteId==candidate.evidence.backNoteId;});
        if(exists)continue;
        NativeTransitionShape shape{1,candidate.middleSourceSeconds,"melodyne-two-piece-transition"};juce::String error;
        if(!validateNativeTransitionShape(shape,candidate.sourceFromSeconds,candidate.sourceToSeconds,error))continue;
        project.logicalTransitions.push_back({candidate.evidence,candidate.sourceFromSeconds,candidate.sourceToSeconds,candidate.middleSourceSeconds,candidate.confidence});++count;
    }
    return count;
}
}
