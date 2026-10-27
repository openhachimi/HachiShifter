#pragma once
#include "NativeVirtualConnectionPlan.h"
#include "NativeSplitTransitionDetection.h"

namespace hachi
{
struct NativeVirtualAutoConnectOptions
{
    bool connectDifferentSources=false;
    bool generatePitch=false;
};
struct NativeVirtualAutoConnectReport
{
    int created=0; // successful route commits, including extension of a new component
    juce::StringArray skipped;
};
// Import-snapshot optimization only. All actual group creation delegates to the
// same ProjectModel authority used by manual actions; no parallel graph writer.
// Disabled is a literal no-op (no model copy, annotations or audio inspection).
inline NativeVirtualAutoConnectReport autoConnectNativeVirtualImport(ProjectData& input,
    NativeVirtualAutoConnectOptions options={},const std::vector<NativeSplitTransitionEvidence>& sourceJoins={})
{
    NativeVirtualAutoConnectReport report;if(!options.connectDifferentSources)return report;
    struct Candidate{std::vector<juce::String> ids;std::optional<NativeSplitTransitionEvidence> split;};std::vector<Candidate> candidates;
    std::set<juce::String> reserved;
    for(const auto& group:input.nativeVirtualGroups)for(const auto& id:group.orderedNoteIds)reserved.insert(id);
    if(!sourceJoins.empty())for(const auto& split:detectMelodyneSplitTransitions(input,sourceJoins))
    {
        if(std::any_of(split.orderedNoteIds.begin(),split.orderedNoteIds.end(),[&](const auto& id){return reserved.count(id)>0;}))continue;
        reserved.insert(split.evidence.frontNoteId);reserved.insert(split.evidence.backNoteId);
        candidates.push_back({split.orderedNoteIds,split.evidence});
    }
    for(const auto& track:input.tracks)
    {
        if(!trackShowsAllNativeRegions(track)||!track.compose||track.accompaniment)continue;
        struct Placed{const NoteData* note;double from;};std::vector<Placed> notes;
        for(const auto& clip:track.clips)for(const auto& note:clip.notes)notes.push_back({&note,clip.startSeconds+note.startSeconds});
        std::stable_sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.from<b.from;});
        const auto candidate=[&](std::vector<juce::String> ids)
        {
            if(std::any_of(ids.begin(),ids.end(),[&](const auto& id){return reserved.count(id)>0;}))return;
            const auto voiced=[&](const juce::String& id)
            {
                const auto found=std::find_if(notes.begin(),notes.end(),[&](const auto& p){return p.note->id==id;});
                if(found==notes.end()||found->note->nativeUnpitched)return false;
                const auto& c=found->note->contour;
                for(std::size_t j=1;j<c.size();++j)if(c[j-1].voiced&&c[j].voiced&&std::isfinite(c[j-1].timeSeconds)&&std::isfinite(c[j].timeSeconds)
                    &&c[j].timeSeconds-c[j-1].timeSeconds>=.001&&c[j].timeSeconds-c[j-1].timeSeconds<=.030)return true;
                return false;
            };
            if(!voiced(ids.front())||!voiced(ids.back()))return;
            NativeVirtualConnectionPlan plan;juce::String error;
            if(!planNativeVirtualConnection(track,ids,plan,error)||!nativeVirtualOwnersIndependentlyRenderable(track,plan,error))return;
            // Avoid identifying long simultaneous notes as sequential melody.
            // This is candidate confidence only; established groups are never
            // disconnected by such a threshold after the user moves a member.
            const auto& first=plan.members.front();const auto& last=plan.members.back();
            const auto overlap=std::max(0.,std::min(first.to,last.to)-std::max(first.from,last.from));
            if(overlap>std::min(first.to-first.from,last.to-last.from)*.5)
            {report.skipped.add("Ambiguous simultaneous source bodies: "+first.noteId+" / "+last.noteId);return;}
            // New route endpoints may be shared by the following hop. Reserve
            // bridge interiors only; the common model transaction rejects
            // branches/cycles and merges a valid continuation atomically.
            for(std::size_t i=1;i+1<ids.size();++i)reserved.insert(ids[i]);
            candidates.push_back({std::move(ids)});
        };
        // Typed, explicitly bounded bridges first. An arbitrary alias works;
        // a file called e_g with ordinary kind is not a transition candidate.
        for(std::size_t i=1;i+1<notes.size();++i)
            if(notes[i].note->pronunciationKind==PronunciationKind::transition&&notes[i].note->nativeTransition
                &&notes[i-1].note->pronunciationKind==PronunciationKind::normal&&notes[i+1].note->pronunciationKind==PronunciationKind::normal)
                candidate({notes[i-1].note->id,notes[i].note->id,notes[i+1].note->id});
        for(std::size_t i=1;i<notes.size();++i)
            if(notes[i-1].note->pronunciationKind==PronunciationKind::normal&&notes[i].note->pronunciationKind==PronunciationKind::normal
                &&!notes[i-1].note->nativeUnpitched&&!notes[i].note->nativeUnpitched)
                candidate({notes[i-1].note->id,notes[i].note->id});
    }
    if(candidates.empty())return report;
    ProjectModel staged;staged.resetDocument(input);
    for(const auto& candidate:candidates)
    {
        juce::String error;
        if(staged.setNativeVirtualConnectionForNotes(candidate.ids,true,options.generatePitch,error,.1,true,candidate.split?&*candidate.split:nullptr))++report.created;
        else if(error.isNotEmpty())report.skipped.add(error);
    }
    if(report.created>0)input=staged.snapshot();
    return report;
}
}
