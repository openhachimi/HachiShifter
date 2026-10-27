#include "NativeSourceEdgePlan.h"
#include "NativeNoteTiming.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace hachi
{
bool resolveNativeSourceEdgePlan(const TrackData& track,const std::vector<juce::String>& selected,
                                 NativeSourceEdgePlan& output,juce::String& error)
{
    error.clear(); output={}; NativeSourceEdgePlan result;
    constexpr double epsilon=1.e-7;
    const auto fail=[&](const juce::String& text){error=text;return false;};
    std::set<juce::String> wanted(selected.begin(),selected.end()),seen;
    if(wanted.size()!=selected.size())return fail("Duplicate selected source-edge note identity");
    struct Node
    {
        const NoteData* note;const ClipData* parent;const ClipData* owner;
        const std::vector<SourceTimePoint>* clock;
        double first,last,start,end,origin;
        double alignmentSource=0;bool scoreAligned=false;
    };
    // An owner may contain thousands of slices. Its immutable clock is shared,
    // not regenerated and copied once per note.
    std::map<const ClipData*,std::vector<SourceTimePoint>> clocks;
    std::vector<Node> nodes;
    for(const auto& parent:track.clips)for(const auto& note:parent.notes)
    {
        if(!selected.empty()&&!wanted.count(note.id))continue;
        if(!seen.insert(note.id).second)return fail("Ambiguous source-edge note identity: "+note.id);
        if(!note.nativePronunciationEdges)continue;
        const ClipData* owner=parent.parts.empty()?&parent:nullptr;
        if(!parent.parts.empty())for(const auto& part:parent.parts)if(part.id==note.clipPartId)
        {if(owner)return fail("Ambiguous source-edge child owner");owner=&part;}
        if(!owner||owner->sourceFile==juce::File{}||note.materialAudioId.isEmpty()||note.materialRegionId.isEmpty())
            return fail("Opt-in source edges require exact owner/audio/region identities");
        auto [cached,inserted]=clocks.try_emplace(owner);
        if(inserted)
        {
            cached->second=nativeClipClock(*owner);
            const auto& map=cached->second;
            if(map.size()<2)return fail("Missing native source clock");
            for(std::size_t i=0;i<map.size();++i)
                if(!std::isfinite(map[i].sourceSeconds)||!std::isfinite(map[i].targetSeconds)
                    ||(i&&(map[i].sourceSeconds<=map[i-1].sourceSeconds||map[i].targetSeconds<=map[i-1].targetSeconds)))
                    return fail("Invalid native source clock");
        }
        const auto& clock=cached->second;
        const auto offset=parent.parts.empty()?0.0:owner->startSeconds;
        const auto local=note.startSeconds-offset;
        const auto origin=parent.startSeconds+offset;
        if(!std::isfinite(local)||!std::isfinite(note.durationSeconds)||note.durationSeconds<=0)
            return fail("Invalid source-edge musical note interval");
        double first=0,last=0,start=parent.startSeconds+note.startSeconds,end=start+note.durationSeconds,p=0;
        if(note.nativeScoreMaterialAligned)
        {
            auto childNote=note;childNote.startSeconds=local;NativeScoreMaterialSupport support;
            if(!nativeScoreMaterialAudioSupport(childNote,*owner,clock,support,error))return false;
            first=support.sourceBegin;last=support.sourceEnd;p=support.alignmentSource;
            start=origin+support.targetBegin;end=origin+support.targetEnd;
        }
        else
        {
            if(local<clock.front().targetSeconds-epsilon||local+note.durationSeconds>clock.back().targetSeconds+epsilon)
                return fail("Current note lies outside its source clock");
            first=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,local);
            last=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,local+note.durationSeconds);
        }
        const auto fullFirst=note.materialSourceStartSeconds>=0?note.materialSourceStartSeconds:first;
        const auto fullLast=note.materialSourceEndSeconds>=0?note.materialSourceEndSeconds:last;
        if(!validateNativePronunciationEdges(*note.nativePronunciationEdges,fullFirst,fullLast,error))return false;
        if(first<fullFirst-epsilon||last>fullLast+epsilon||last<=first)
            return fail("Current source coverage lies outside its pronunciation annotation");
        nodes.push_back({&note,&parent,owner,&clock,first,last,start,end,origin,p,note.nativeScoreMaterialAligned});
    }
    if(!selected.empty())for(const auto& id:wanted)if(!seen.count(id))return fail("Missing selected source-edge note: "+id);
    const auto count=nodes.size();std::vector<int> next(count,-1),previous(count,-1);
    for(std::size_t i=0;i<count;++i)for(std::size_t j=0;j<count;++j)
    {
        if(i==j)continue;const auto& a=nodes[i];const auto& b=nodes[j];
        if((a.note->nativeTransition&&a.note->pronunciationKind==PronunciationKind::transition)
            ||(b.note->nativeTransition&&b.note->pronunciationKind==PronunciationKind::transition))continue; // a typed transition is a complete cut
        const bool sameRow=a.note->materialRegionId==b.note->materialRegionId;
        const bool explicitNext=a.note->materialConnectedNextRegionId==b.note->materialRegionId;
        if((!sameRow&&!explicitNext)||a.note->materialAudioId!=b.note->materialAudioId
            ||a.owner->sourceFile!=b.owner->sourceFile)continue;
        if(!a.owner->materialSourceFingerprint.isEmpty()&&!b.owner->materialSourceFingerprint.isEmpty()
            &&a.owner->materialSourceFingerprint!=b.owner->materialSourceFingerprint)continue;
        const bool exactSeam=std::abs(a.last-b.first)<=epsilon&&std::abs(a.end-b.start)<=epsilon;
        const bool sharedScoreSupport=a.scoreAligned&&b.scoreAligned&&explicitNext&&a.owner==b.owner
            &&a.first<b.first-epsilon&&a.last<b.last-epsilon&&b.first<=a.last+epsilon;
        if(!exactSeam&&!sharedScoreSupport)continue;
        if(next[i]>=0||previous[j]>=0)return fail("Ambiguous native source continuity endpoint");
        next[i]=int(j);previous[j]=int(i);
    }
    std::vector<bool> visited(count,false);
    for(std::size_t root=0;root<count;++root)if(previous[root]<0)
    {
        std::vector<std::size_t> chain;
        for(auto cursor=int(root);cursor>=0;cursor=next[std::size_t(cursor)])
        {
            if(visited[std::size_t(cursor)])return fail("Cyclic native source continuity");
            visited[std::size_t(cursor)]=true;chain.push_back(std::size_t(cursor));
        }
        const auto& head=nodes[chain.front()];const auto& tail=nodes[chain.back()];
        std::vector<SourceTimePoint> anchors,componentClock;
        for(const auto index:chain)
        {
            const auto& n=nodes[index];anchors.push_back({n.start,n.first});
            const auto& clock=*n.clock;
            auto at=std::upper_bound(clock.begin(),clock.end(),n.start-n.origin,
                [](double time,const auto& point){return time<point.targetSeconds;});
            for(;at!=clock.end()&&n.origin+at->targetSeconds<n.end-epsilon;++at)
                anchors.push_back({n.origin+at->targetSeconds,n.owner->sourceOffsetSeconds+at->sourceSeconds});
            anchors.push_back({n.end,n.last});
        }
        // A score-aligned row can have support on both sides of its musical P.
        // Shared-owner overlaps are ONE actual clock, not duplicated audio.
        std::stable_sort(anchors.begin(),anchors.end(),[](const auto& a,const auto& b){return a.sourceSeconds<b.sourceSeconds;});
        for(const auto& anchor:anchors)
        {
            if(!componentClock.empty()&&std::abs(anchor.sourceSeconds-componentClock.back().sourceSeconds)<=epsilon)
            {
                if(std::abs(anchor.targetSeconds-componentClock.back().targetSeconds)>epsilon)
                    return fail("Selected source supports disagree on their shared clock");
                continue;
            }
            if(!componentClock.empty()&&anchor.targetSeconds<=componentClock.back().targetSeconds)
                return fail("Non-monotonic component source clock");
            componentClock.push_back(anchor);
        }
        std::vector<double> ownedFrom,ownedTo;
        for(const auto index:chain){ownedFrom.push_back(nodes[index].start);ownedTo.push_back(nodes[index].end);}
        for(std::size_t i=1;i<chain.size();++i)
        {
            const auto& a=nodes[chain[i-1]];const auto& b=nodes[chain[i]];
            if(b.start<a.end-epsilon)
            {
                if(!a.scoreAligned||!b.scoreAligned||a.owner!=b.owner)
                    return fail("Ambiguous audible ownership across source supports");
                // Stored incoming row begin is the authored cut. Incoming P is
                // later in that row and maps to its score beat, not the seam.
                const auto seamSource=b.first;
                const auto seam=nativeTargetTimeAt(componentClock,seamSource);
                ownedTo[i-1]=seam;ownedFrom[i]=seam;
            }
        }
        const auto entry=*head.note->nativePronunciationEdges,exit=*tail.note->nativePronunciationEdges;
        std::optional<NativePronunciationEdges> entryCoverage,exitCoverage;
        // A retained slice may remove part/all of an original edge. Project its
        // real coverage using the same explicit clipping policy as source edits.
        if(!clipNativePronunciationEdges(entry,entry.entryFromSourceSeconds,entry.terminationSourceSeconds,
            std::max(head.first,entry.entryFromSourceSeconds),std::min(tail.last,entry.terminationSourceSeconds),entryCoverage,error)
            ||!clipNativePronunciationEdges(exit,exit.entryFromSourceSeconds,exit.terminationSourceSeconds,
            std::max(head.first,exit.entryFromSourceSeconds),std::min(tail.last,exit.terminationSourceSeconds),exitCoverage,error))return false;
        const auto mapped=[&](double source){return nativeTargetTimeAt(componentClock,source);};
        const auto entryFrom=mapped(entryCoverage->entryFromSourceSeconds),entryTo=mapped(entryCoverage->entryToSourceSeconds);
        const auto exitFrom=mapped(exitCoverage->exitFromSourceSeconds),exitTo=mapped(exitCoverage->exitToSourceSeconds);
        const auto termination=mapped(exitCoverage->terminationSourceSeconds);
        NativeSourceEdgeComponent component;component.sourceFile=head.owner->sourceFile;
        const auto componentIndex=result.components.size();
        for(std::size_t position=0;position<chain.size();++position)
        {
            const auto index=chain[position];const auto& n=nodes[index];NativeSourceEdgeMember member;
            member.noteId=n.note->id;member.parentClipId=n.parent->id;member.ownerId=n.owner->id;
            member.expandedClipId=n.parent->parts.empty()?n.owner->id:n.parent->id+":"+n.owner->id;
            member.audioId=n.note->materialAudioId;member.regionId=n.note->materialRegionId;member.sourceFile=n.owner->sourceFile;
            member.component=componentIndex;member.exposedEntry=index==chain.front();member.exposedExit=index==chain.back();
            member.supportFromTargetSeconds=ownedFrom[position];member.supportToTargetSeconds=ownedTo[position];
            if(member.supportToTargetSeconds<=member.supportFromTargetSeconds)return fail("Empty audible ownership in source-edge component");
            member.entryWindowActive=entryTo>entryFrom&&member.supportFromTargetSeconds<entryTo&&member.supportToTargetSeconds>entryFrom;
            member.exitWindowActive=exitTo>exitFrom&&member.supportFromTargetSeconds<exitTo&&member.supportToTargetSeconds>exitFrom;
            if(member.entryWindowActive||member.exposedEntry){member.entryFromTargetSeconds=entryFrom;member.entryToTargetSeconds=entryTo;}
            if(member.exitWindowActive||member.exposedExit){member.exitFromTargetSeconds=exitFrom;member.exitToTargetSeconds=exitTo;member.terminationTargetSeconds=termination;}
            component.noteIds.push_back(member.noteId);result.members.push_back(std::move(member));
        }
        component.entryNoteId=component.noteIds.front();component.exitNoteId=component.noteIds.back();result.components.push_back(std::move(component));
    }
    if(std::any_of(visited.begin(),visited.end(),[](bool value){return !value;}))return fail("Cyclic native source continuity");
    output=std::move(result);return true;
}
}
