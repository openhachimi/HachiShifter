#pragma once
#include "NativeTrimSource.h"
#include "NativeSharedEnvelope.h"

namespace hachi
{
// Joining a seam is different from the lyric-less, flat UTAU note merge.
// The recording, its warp and its acoustic contour remain authoritative.
inline std::optional<ClipData> planNativeNoteJoin(const TrackData& track,const ClipData& original,
    const juce::String& firstId,const juce::String& secondId)
{
    if(!trackShowsAllNativeRegions(track)||firstId==secondId)return {};
    auto first=std::find_if(original.notes.begin(),original.notes.end(),[&](const auto& n){return n.id==firstId;});
    auto second=std::find_if(original.notes.begin(),original.notes.end(),[&](const auto& n){return n.id==secondId;});
    if(first==original.notes.end()||second==original.notes.end())return {};
    if(first->startSeconds>second->startSeconds)std::swap(first,second);
    const auto seam=first->startSeconds+first->durationSeconds;
    if(std::abs(seam-second->startSeconds)>1.e-7)return {};
    for(const auto& n:original.notes)if(n.id!=first->id&&n.id!=second->id
        &&n.startSeconds<second->startSeconds+second->durationSeconds-1.e-7
        &&n.startSeconds+n.durationSeconds>first->startSeconds+1.e-7)return {};
    const auto lines=sharedPitchLines(track);const auto* a=lines.memberFor(first->id);const auto* b=lines.memberFor(second->id);
    if(!a||!b||!a->nativeSharedCurve||a->line!=b->line)return {};
    auto sources=expandedClipParts(original);
    const auto owner=[&](const auto& id){return std::find_if(sources.begin(),sources.end(),[&](const auto& c){
        return std::any_of(c.notes.begin(),c.notes.end(),[&](const auto& n){return n.id==id;});});};
    auto left=owner(first->id),right=owner(second->id);if(left==sources.end()||right==sources.end())return {};
    // A seam between successive ranges of the same original recording can be
    // removed without baking audio or losing access to the hidden source file.
    if(left->sourceFile!=right->sourceFile||!left->sourceFile.existsAsFile())return {};
    const auto absolute=original.startSeconds+seam;
    const auto sourceLeft=left->sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(*left),absolute-left->startSeconds);
    const auto sourceRight=right->sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(*right),absolute-right->startSeconds);
    if(std::abs(sourceLeft-sourceRight)>1.e-6)return {};
    if(std::abs(first->formantSemitones-second->formantSemitones)>1.e-6
        ||std::abs(first->tension-second->tension)>1.e-6||std::abs(first->breath-second->breath)>1.e-6)return {};

    auto merged=*first;merged.durationSeconds=second->startSeconds+second->durationSeconds-first->startSeconds;
    if(first->label!=second->label)merged.label=first->label+second->label;
    merged.connectedToNext=second->connectedToNext;merged.utauAutoPitchTransition=false;
    merged.sourceMidiCenter=first->sourceMidiCenter>=0?first->sourceMidiCenter:first->midiNote;
    merged.sourcePitchMeasured=first->sourcePitchMeasured&&second->sourcePitchMeasured;
    merged.modulation=merged.drift=1;merged.contour.clear();merged.pitchControlPoints.clear();
    const auto origin=original.startSeconds+first->startSeconds;
    const auto vibrato=first->vibratoEnabled||second->vibratoEnabled;
    for(const auto* n:{&*first,&*second})for(auto p:n->contour)
    {
        const auto local=p.timeSeconds;const auto sourceCentre=n->sourceMidiCenter>=0?n->sourceMidiCenter:n->midiNote;
        const auto target=a->renderSharedCurve?a->line->midiAt(original.startSeconds+n->startSeconds+local)
            :n->midiNote+renderedPitchCents(*n,p)/100;
        p.manualTargetCents=(target-merged.midiNote)*100+static_cast<float>(vibratoCentsAt(*n,local));p.hasManualTarget=true;
        p.relativeCents+=(sourceCentre-merged.sourceMidiCenter)*100;p.withoutVibratoCents+=(sourceCentre-merged.sourceMidiCenter)*100;
        p.timeSeconds+=n->startSeconds-first->startSeconds;
        if(n==&*second&&std::abs(local)<1.e-7)continue;
        merged.contour.push_back(p);
    }
    if(!vibrato)
        for(const auto& piece:a->line->pieces)for(auto p:piece.points){p.timeSeconds-=origin;merged.pitchControlPoints.push_back(p);}
    merged.vibratoEnabled=false;merged.vibratoReferenceDurationSeconds=merged.vibratoTimeOffsetSeconds=0;
    merged.nativeSegments=first->nativeSegments;
    const auto sourceShift=sourceRight-(first->nativeSourceStartSeconds>=0?first->nativeSourceStartSeconds:
        left->sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(*left),origin-left->startSeconds));
    for(auto segment:second->nativeSegments)
    {segment.sourceStartSeconds+=sourceShift;segment.sourceEndSeconds+=sourceShift;segment.alignmentSeconds+=sourceShift;merged.nativeSegments.push_back(std::move(segment));}
    merged.nativeSourceEndSeconds=second->nativeSourceEndSeconds;
    merged.sibilantMarkers=first->sibilantMarkers;for(auto t:second->sibilantMarkers)merged.sibilantMarkers.push_back(t+first->durationSeconds);

    const auto envelopes=nativeSharedEnvelopes(track);const auto envelope=envelopes.find(first->id);
    if(envelope==envelopes.end())return {};
    merged.amplitudeEnvelopeBasePercent=100;merged.nativeEnvelope={};merged.gain=1;
    const auto shaped=first->nativeEnvelope.mode!=0||second->nativeEnvelope.mode!=0
        ||std::abs(first->gain-1)>1.e-6||std::abs(second->gain-1)>1.e-6;
    merged.amplitudeEnvelope=envelope->second.relativePoints();
    if(shaped)
    {
        auto points=merged.amplitudeEnvelope;merged.amplitudeEnvelope.clear();
        for(auto p:points)if(p.timeSeconds<0||p.timeSeconds>merged.durationSeconds)merged.amplitudeEnvelope.push_back(p);
        const auto add=[&](double t){const auto& n=t<=first->durationSeconds+1.e-9?*first:*second;
            const auto local=t-(n.startSeconds-first->startSeconds);
            const auto gain=n.gain*backend::nativeEnvelopeGain(n.nativeEnvelope,juce::jlimit(0.0,n.durationSeconds,local),n.durationSeconds);
            const auto db=nativeEnvelopeDbAt(*envelope->second.points,origin+t)+juce::Decibels::gainToDecibels(gain,-120.f);
            merged.amplitudeEnvelope.push_back({t,db,true,true});};
        for(double t=0;t<merged.durationSeconds;t+=.001)add(t);add(merged.durationSeconds);
        std::stable_sort(merged.amplitudeEnvelope.begin(),merged.amplitudeEnvelope.end(),[](const auto& x,const auto& y){return x.timeSeconds<y.timeSeconds;});
    }
    auto result=original;rememberNativeTrimSources(result);
    if(left==right)
    {
        std::erase_if(result.notes,[&](const auto& n){return n.id==first->id||n.id==second->id;});result.notes.push_back(std::move(merged));
        std::stable_sort(result.notes.begin(),result.notes.end(),[](const auto& x,const auto& y){return x.startSeconds<y.startSeconds;});return result;
    }
    if(!original.nativeAudioLinked)return {};
    // Source ranges split into separate children still form one recording.
    // Keep their original warp breakpoints when consolidating their owner.
    if(std::abs(left->startSeconds+left->audioStartSeconds+left->audioLength()-right->startSeconds-right->audioStartSeconds)>1.e-7
        ||std::abs(left->sourceOffsetSeconds+left->sourceDurationSeconds-right->sourceOffsetSeconds)>1.e-6
        ||std::abs(left->gain-right->gain)>1.e-6||left->muted!=right->muted
        ||!left->gainEnvelope.empty()||!right->gainEnvelope.empty()
        ||!left->inheritedGainEnvelopes.empty()||!right->inheritedGainEnvelopes.empty()
        ||left->fadeOutSeconds>0||right->fadeInSeconds>0||left->crossfadeOutSeconds>0||right->crossfadeInSeconds>0)return {};
    rememberNativeTrimSource(*left);rememberNativeTrimSource(*right);auto joined=*left;
    joined.durationSeconds=right->startSeconds+right->durationSeconds-joined.startSeconds;
    joined.audioDurationSeconds=right->startSeconds+right->audioStartSeconds+right->audioLength()-joined.startSeconds-joined.audioStartSeconds;
    joined.sourceDurationSeconds=right->sourceOffsetSeconds+right->sourceDurationSeconds-joined.sourceOffsetSeconds;
    joined.fadeOutSeconds=right->fadeOutSeconds;joined.crossfadeOutSeconds=right->crossfadeOutSeconds;joined.sourceTimeMap.clear();
    for(const auto* c:{&*left,&*right})for(auto p:nativeClipClock(*c))
    {p.targetSeconds+=c->startSeconds-joined.startSeconds;p.sourceSeconds+=c->sourceOffsetSeconds-joined.sourceOffsetSeconds;
        if(!joined.sourceTimeMap.empty()&&std::abs(joined.sourceTimeMap.back().targetSeconds-p.targetSeconds)<1.e-7)joined.sourceTimeMap.back()=p;
        else joined.sourceTimeMap.push_back(p);}
    joined.nativeTrimClock.clear();
    for(const auto& p:left->nativeTrimClock)if(p.targetSeconds<left->audioStartSeconds-1.e-8)joined.nativeTrimClock.push_back(p);
    for(auto p:joined.sourceTimeMap){p.sourceSeconds+=joined.sourceOffsetSeconds;joined.nativeTrimClock.push_back(p);}
    for(auto p:right->nativeTrimClock)if(p.targetSeconds>right->audioStartSeconds+right->audioLength()+1.e-8)
    {p.targetSeconds+=right->startSeconds-joined.startSeconds;joined.nativeTrimClock.push_back(p);}
    if(right->nativeSourcePitchComplete){joined.nativeSourcePitch=right->nativeSourcePitch;joined.nativeSourcePitchComplete=true;}
    joined.notes.clear();
    for(const auto* c:{&*left,&*right})for(auto n:c->notes)if(n.id!=first->id&&n.id!=second->id)
    {n.startSeconds+=c->startSeconds-joined.startSeconds;joined.notes.push_back(std::move(n));}
    merged.startSeconds+=original.startSeconds-joined.startSeconds;merged.clipPartId.clear();joined.notes.push_back(std::move(merged));
    const auto leftId=left->id,rightId=right->id;std::vector<ClipData> kept;
    for(auto c:sources)if(c.id!=leftId&&c.id!=rightId)kept.push_back(std::move(c));kept.push_back(std::move(joined));
    result=kept.size()==1?std::move(kept.front()):assembledLinkedAudio(kept);result.id=original.id;
    result.nativeAudioLinked=!result.parts.empty();result.showNoteHints=original.showNoteHints;result.showNormalDisplay=original.showNormalDisplay;
    return result;
}
}
