#pragma once
#include "NativeNoteTiming.h"
#include <set>
namespace hachi
{
struct NativeConsonantMergeEvidence
{
    NativeSplitTransitionEvidence sourceJoin; // raw same-source join; NOT a logical transition
    bool sibilantIdentified=false; // supplied by a separate conservative detector
};
struct NativeConsonantMergePlan
{
    juce::String trackId,onsetClipId,vowelClipId,onsetNoteId,vowelNoteId;
    ClipData merged,onsetOriginal,vowelOriginal;
    double preutteranceSourceSeconds=0,consonantEndSourceSeconds=0;
};
// Imported single-element owners only. This is a pure same-source operation,
// not a cross-source connection and never goes through the virtual assembler.
// Retain exact originals for a later durable model undo/split-back archive.
inline bool planNativeConsonantMerge(const ProjectData& project,const NativeConsonantMergeEvidence& evidence,
    NativeConsonantMergePlan& output,juce::String& error)
{
    error.clear();const auto fail=[&](const juce::String& why){error=why;return false;};const auto& raw=evidence.sourceJoin;
    if(!evidence.sibilantIdentified||raw.transitionIdentified)return fail("An unvoiced join alone is not a sibilant merge decision");
    if(!raw.joinsPitches||!raw.joinsAmplitudes||!raw.joinsPhases||raw.frontNoteId.isEmpty()||raw.backNoteId.isEmpty()||raw.frontNoteId==raw.backNoteId
        ||raw.sourceObjectIdentity.isEmpty()||raw.sourceObjectIdentity!=raw.frontSourceObjectIdentity||raw.sourceObjectIdentity!=raw.backSourceObjectIdentity
        ||raw.frontElementIdentity.isEmpty()||raw.backElementIdentity.isEmpty()||raw.precedingElementIdentity!=raw.frontElementIdentity||raw.followingElementIdentity!=raw.backElementIdentity)
        return fail("Missing exact continuous source join evidence");
    const TrackData* track=nullptr;const ClipData *front=nullptr,*back=nullptr;
    for(const auto& candidate:project.tracks)for(const auto& clip:candidate.clips)for(const auto& note:clip.notes)
    {
        if(note.id!=raw.frontNoteId&&note.id!=raw.backNoteId)continue;
        if(track&&track!=&candidate)return fail("Consonant and vowel must share one track");track=&candidate;
        auto& endpoint=note.id==raw.frontNoteId?front:back;if(endpoint)return fail("Ambiguous consonant merge endpoint");endpoint=&clip;
    }
    if(!track||!front||!back||front==back||!trackShowsAllNativeRegions(*track)||!track->compose||track->accompaniment)return fail("Consonant merge needs two native melodic source owners");
    if(front->notes.size()!=1||back->notes.size()!=1||!front->parts.empty()||!back->parts.empty()||front->audioDurationSeconds>=0||back->audioDurationSeconds>=0
        ||std::abs(front->audioStartSeconds)>1.e-9||std::abs(back->audioStartSeconds)>1.e-9)return fail("Consonant merge needs exact complete single-element owners");
    const auto& onset=front->notes.front();const auto& vowel=back->notes.front();
    if(onset.pronunciationKind!=PronunciationKind::normal||vowel.pronunciationKind!=PronunciationKind::normal
        ||onset.materialLocalOverride||vowel.materialLocalOverride||onset.materialAudioId.isNotEmpty()||vowel.materialAudioId.isNotEmpty()
        ||onset.nativeScoreMaterialAligned||vowel.nativeScoreMaterialAligned)
        return fail("Do not merge breath, transitions or an edited/score-bound source region");
    for(const auto& row:project.logicalTransitions)if(row.evidence.frontNoteId==onset.id||row.evidence.backNoteId==onset.id||row.evidence.frontNoteId==vowel.id||row.evidence.backNoteId==vowel.id)
        return fail("A logical transition half is never a consonant onset candidate");
    for(const auto& group:project.nativeVirtualGroups)for(const auto& id:group.orderedNoteIds)if(id==onset.id||id==vowel.id)return fail("Keep existing virtual group members separate");
    if(front->sourceFile==juce::File{}||front->sourceFile!=back->sourceFile||front->sourceFile.hasFileExtension("ust;ustx;mid;midi;mpd;hjpx;hspx"))return fail("Consonant merge requires one unchanged physical recording");
    if(onset.contour.size()<2||std::any_of(onset.contour.begin(),onset.contour.end(),[](const auto& p){return p.voiced;}))return fail("Onset must have explicit entirely unvoiced evidence, not missing analysis");
    bool voiced=false;for(std::size_t i=1;i<vowel.contour.size();++i)if(vowel.contour[i-1].voiced&&vowel.contour[i].voiced&&vowel.contour[i].timeSeconds-vowel.contour[i-1].timeSeconds>=.001&&vowel.contour[i].timeSeconds-vowel.contour[i-1].timeSeconds<=.030)voiced=true;
    if(!voiced||vowel.nativeUnpitched)return fail("Missing stable voiced vowel evidence");
    if(std::abs(onset.startSeconds)>1.e-9||std::abs(vowel.startSeconds)>1.e-9||std::abs(onset.durationSeconds-front->durationSeconds)>1.e-7||std::abs(vowel.durationSeconds-back->durationSeconds)>1.e-7
        ||std::abs(front->startSeconds+front->durationSeconds-back->startSeconds)>1.e-6)return fail("Imported partial cut must be one exact target boundary");
    if(onset.durationSeconds<.004||onset.durationSeconds>.3)return fail("Consonant candidate duration outside conservative limits");
    const auto firstClock=nativeClipClock(*front),lastClock=nativeClipClock(*back);
    for(const auto* clock:{&firstClock,&lastClock})
    {
        if(clock->size()<2)return fail("Missing consonant merge source clock");
        for(std::size_t i=0;i<clock->size();++i)if(!std::isfinite((*clock)[i].targetSeconds)||!std::isfinite((*clock)[i].sourceSeconds)
            ||(i&&((*clock)[i].targetSeconds<=(*clock)[i-1].targetSeconds||(*clock)[i].sourceSeconds<=(*clock)[i-1].sourceSeconds)))return fail("Invalid consonant merge source clock");
    }
    const auto from=front->sourceOffsetSeconds+nativeSourceTimeAt(firstClock,0);
    const auto cut=front->sourceOffsetSeconds+nativeSourceTimeAt(firstClock,front->durationSeconds);
    const auto second=back->sourceOffsetSeconds+nativeSourceTimeAt(lastClock,0);
    const auto to=back->sourceOffsetSeconds+nativeSourceTimeAt(lastClock,back->durationSeconds);
    if(!std::isfinite(from)||!std::isfinite(to)||from<0||std::abs(cut-second)>1.e-6||to<=cut||cut<=from)return fail("Target adjacency does not prove the same physical source cut");
    const auto vowelTiming=nativeNoteSourceTiming(vowel,*back,lastClock);
    const auto c=std::max(cut,vowelTiming.alignmentSeconds);
    if(c-cut>std::min(.08,vowel.durationSeconds*.4))return fail("Vowel already has a substantial independent preutterance");
    for(const auto& clip:track->clips)for(const auto& other:clip.notes)if(other.id!=onset.id&&other.id!=vowel.id&&clip.sourceFile==front->sourceFile)
    {const auto a=clip.startSeconds+other.startSeconds,b=a+other.durationSeconds;if(a<back->startSeconds-1.e-7&&b>back->startSeconds+1.e-7)return fail("Another placement of the same recording crosses the explicit consonant-vowel cut");}
    // Initial safe scope: refuse unsupported expressive envelopes rather than
    // silently losing them. Exact originals are still copied into the plan.
    if(front->muted!=back->muted||front->gain!=back->gain||onset.gain!=vowel.gain||onset.formantSemitones!=vowel.formantSemitones||onset.tension!=vowel.tension||onset.breath!=vowel.breath
        ||!front->gainEnvelope.empty()||!back->gainEnvelope.empty()||!front->inheritedGainEnvelopes.empty()||!back->inheritedGainEnvelopes.empty()
        ||!onset.amplitudeEnvelope.empty()||!vowel.amplitudeEnvelope.empty()||onset.nativeEnvelope.mode!=0||vowel.nativeEnvelope.mode!=0)return fail("Differing expressive gains/envelopes need an explicit merge policy");
    NativeConsonantMergePlan plan;plan.trackId=track->id;plan.onsetClipId=front->id;plan.vowelClipId=back->id;plan.onsetNoteId=onset.id;plan.vowelNoteId=vowel.id;
    plan.onsetOriginal=*front;plan.vowelOriginal=*back;plan.preutteranceSourceSeconds=cut;plan.consonantEndSourceSeconds=c;
    auto merged=*back;merged.id=back->id;merged.startSeconds=front->startSeconds;merged.durationSeconds=front->durationSeconds+back->durationSeconds;
    merged.sourceOffsetSeconds=from;merged.sourceDurationSeconds=to-from;merged.sourceTimeMap.clear();
    const auto append=[&](const auto& points,const ClipData& owner)
    {for(const auto& p:points){SourceTimePoint q{owner.startSeconds+p.targetSeconds-merged.startSeconds,owner.sourceOffsetSeconds+p.sourceSeconds-from};if(!merged.sourceTimeMap.empty()&&std::abs(q.targetSeconds-merged.sourceTimeMap.back().targetSeconds)<1.e-7){if(std::abs(q.sourceSeconds-merged.sourceTimeMap.back().sourceSeconds)>1.e-6)return false;continue;}merged.sourceTimeMap.push_back(q);}return true;};
    if(!append(firstClock,*front)||!append(lastClock,*back))return fail("The two original source clocks cannot share one exact cut");
    merged.fadeInSeconds=front->fadeInSeconds;merged.crossfadeInSeconds=front->crossfadeInSeconds;merged.glideConnectedFromPrevious=front->glideConnectedFromPrevious;
    auto note=vowel;note.id=vowel.id;note.startSeconds=0;note.durationSeconds=merged.durationSeconds;note.clipPartId.clear();note.nativeUnpitched=false;
    note.connectedToPrevious=onset.connectedToPrevious;note.nativeProvenance="melodyne-consonant-merged";note.nativeSourceStartSeconds=from;note.nativeSourceEndSeconds=to;
    note.materialAudioId.clear();note.materialRegionId.clear();note.materialSourceStartSeconds=note.materialSourceEndSeconds=-1;
    note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=cut-from;
    note.consonantSeconds=front->durationSeconds+nativeTargetTimeAt(lastClock,c-back->sourceOffsetSeconds);
    note.contour=onset.contour;std::erase_if(note.contour,[&](const auto& p){return p.timeSeconds>=front->durationSeconds-1.e-9;});
    for(auto& p:note.contour){p.voiced=false;p.hasManualTarget=false;}
    for(auto p:vowel.contour){p.timeSeconds+=front->durationSeconds;note.contour.push_back(p);}
    note.pitchControlPoints=vowel.pitchControlPoints;for(auto& p:note.pitchControlPoints)p.timeSeconds+=front->durationSeconds;
    if(note.vibratoEnabled)
    {
        const auto originalReference=vowel.vibratoReferenceDurationSeconds>0?vowel.vibratoReferenceDurationSeconds:vowel.durationSeconds;
        const auto reference=originalReference+front->durationSeconds;
        note.vibratoReferenceDurationSeconds=reference;note.vibratoLengthPercent=vowel.vibratoLengthPercent*originalReference/reference;
        note.vibratoEndPercent=(originalReference*vowel.vibratoEndPercent+front->durationSeconds*100.)/reference;
    }
    note.sibilantMarkers=onset.sibilantMarkers;for(auto marker:vowel.sibilantMarkers)note.sibilantMarkers.push_back(marker+front->durationSeconds);
    SampleRegionSetting timing;timing.name=note.label;timing.regionStartSeconds=from;timing.regionEndSeconds=to;timing.alignmentSeconds=cut;timing.fixedDurationSeconds=c-from;
    if(!applyPronunciationTiming(timing,pronunciationTimingFor(timing),error))return false;note.nativeSegments=SampleSettings::nativeSegmentsFor(timing);
    merged.notes={std::move(note)};merged.nativeTrimClock.clear();plan.merged=std::move(merged);output=std::move(plan);return true;
}
}
