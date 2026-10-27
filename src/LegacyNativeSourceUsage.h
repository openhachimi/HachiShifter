#pragma once
#include "NativeNoteTiming.h"
#include <map>
#include <set>
namespace hachi
{
// Load-only identity migration. Never infer a connection from proximity alone:
// both directions of the OLD saved note AND physical glide flags must exist.
// Explicit empty/new usage fields, disconnected flags and ambiguous voices win.
inline int restoreLegacyNativeSourceUsages(ProjectData& project,const juce::ValueTree& saved)
{
    std::set<juce::String> eligible,virtualNotes;
    const auto collect=[&](auto&& self,const juce::ValueTree& node)->void
    {
        if(node.hasType("Clip")&&!node.hasProperty("materialSourceUsageGroupId"))eligible.insert(node.getProperty("id").toString());
        for(const auto child:node)self(self,child);
    };collect(collect,saved);
    for(const auto& group:project.nativeVirtualGroups)for(const auto& id:group.orderedNoteIds)virtualNotes.insert(id);
    int restored=0;
    for(auto& track:project.tracks)
    {
        struct Span{std::size_t index;double from,to,sourceFrom,sourceTo;};std::vector<Span> spans;
        for(std::size_t i=0;i<track.clips.size();++i)
        {
            const auto& clip=track.clips[i];
            if(!eligible.count(clip.id)||clip.materialSourceUsageGroupId.isNotEmpty()||!clip.parts.empty()||clip.notes.size()!=1||clip.sourceFile==juce::File{}||virtualNotes.count(clip.notes[0].id))continue;
            const auto clock=nativeClipClock(clip);if(clock.size()<2)continue;bool valid=true;
            for(std::size_t p=0;p<clock.size();++p)if(!std::isfinite(clock[p].sourceSeconds)||!std::isfinite(clock[p].targetSeconds)||(p&&(clock[p].sourceSeconds<=clock[p-1].sourceSeconds||clock[p].targetSeconds<=clock[p-1].targetSeconds)))valid=false;
            if(!valid)continue;const auto& note=clip.notes[0];const auto from=note.startSeconds,to=from+note.durationSeconds;
            if(!std::isfinite(from)||!std::isfinite(to)||to<=from||from<clock.front().targetSeconds-1.e-6||to>clock.back().targetSeconds+1.e-6)continue;
            spans.push_back({i,clip.startSeconds+from,clip.startSeconds+to,clip.sourceOffsetSeconds+nativeSourceTimeAt(clock,from),clip.sourceOffsetSeconds+nativeSourceTimeAt(clock,to)});
        }
        std::vector<std::pair<std::size_t,std::size_t>> edges;std::map<std::size_t,int> incoming,outgoing;
        for(const auto& a:spans)for(const auto& b:spans)
        {
            if(a.index==b.index)continue;const auto& left=track.clips[a.index];const auto& right=track.clips[b.index];
            if(left.sourceFile!=right.sourceFile||!left.glideConnectedToNext||!right.glideConnectedFromPrevious||!left.notes[0].connectedToNext||!right.notes[0].connectedToPrevious)continue;
            if(std::abs(a.to-b.from)>1.e-6||std::abs(a.sourceTo-b.sourceFrom)>1.e-6)continue;
            edges.emplace_back(a.index,b.index);++outgoing[a.index];++incoming[b.index];
        }
        std::map<std::size_t,std::size_t> roots;for(const auto& span:spans)roots.emplace(span.index,span.index);
        const auto root=[&](std::size_t at){while(roots.at(at)!=at)at=roots.at(at);return at;};
        for(const auto& [a,b]:edges)if(outgoing[a]==1&&incoming[b]==1){const auto ar=root(a),br=root(b);if(ar!=br)roots[br]=ar;}
        std::map<std::size_t,std::vector<std::size_t>> components;for(const auto& [index,parent]:roots){juce::ignoreUnused(parent);components[root(index)].push_back(index);}
        for(const auto& [at,members]:components)if(members.size()>1)
        {
            const auto id="legacy-source-use/"+track.id+"/"+track.clips[at].notes[0].id;
            bool conflict=false;for(const auto& clip:track.clips)if(clip.materialSourceUsageGroupId==id)conflict=true;
            if(conflict)continue;for(const auto member:members)track.clips[member].materialSourceUsageGroupId=id;
            ++restored;
        }
    }
    return restored;
}
}
