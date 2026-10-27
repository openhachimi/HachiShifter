#pragma once
#include "NativeVirtualPitchPlan.h"
// Derived target data only: never a second stored source/curve or DSP backend.
namespace hachi
{
inline std::vector<ResolvedConnectionPitchCurve> nativeVirtualContinuityCurves(const TrackData& track,
    const juce::String& groupId,const NativeVirtualConnectionPlan& plan,const SharedPitchLines& lines,
    const juce::String& replacedNoteId={},const std::vector<PitchCurveEditPoint>* replacedPoints=nullptr)
{
    struct Source {const NoteData* note=nullptr;double origin=0,from=0,to=0;std::vector<PitchCurveEditPoint> own;};
    const auto count=plan.members.size();if(count<2)return {};
    std::vector<Source> sources(count);double from=std::numeric_limits<double>::infinity(),to=-from;
    for(std::size_t i=0;i<count;++i)
    {
        auto& s=sources[i];const auto& member=plan.members[i];
        for(const auto& parent:track.clips)for(const auto& note:parent.notes)if(note.id==member.noteId)
        {s.note=&note;s.origin=parent.startSeconds+note.startSeconds;}
        if(!s.note)return {};
        s.from=std::min(member.from,s.origin);s.to=std::max(member.to,s.origin+s.note->durationSeconds);
        s.own=replacedPoints&&s.note->id==replacedNoteId&&!replacedPoints->empty()?*replacedPoints:ownPitchPoints(*s.note);for(auto& point:s.own)point.timeSeconds+=s.origin;
        from=std::min(from,s.from);to=std::max(to,s.to);
    }
    if(!std::isfinite(from)||!std::isfinite(to)||to<=from)return {};
    std::vector<bool> transition(count);for(std::size_t i=0;i<count;++i)transition[i]=plan.members[i].transition;
    for(std::size_t role=0;role<plan.contributionRanges.size()&&role<plan.contributionMembers.size();++role)
        if(plan.contributionMembers[role].transition)for(std::size_t i=plan.contributionRanges[role].first;i<=plan.contributionRanges[role].second&&i<count;++i)transition[i]=true;
    const auto eligible=[&](std::size_t i){return !sources[i].note->nativeUnpitched&&!sources[i].own.empty()&&!transition[i];};
    const auto baseAt=[&](std::size_t i,double t)
    {if(const auto* member=lines.memberFor(sources[i].note->id);member&&member->line)return member->line->midiAt(t);return evaluatePitchCurve(sources[i].own,t);};
    const auto vibrato=[&](std::size_t i,double t){return float(vibratoCentsAt(*sources[i].note,juce::jlimit(0.,sources[i].note->durationSeconds,t-sources[i].origin))/100.);};
    ResolvedConnectionPitchCurve curve;curve.connectionId=groupId+"/single-target";curve.active=true;curve.virtualSinglePitch=true;
    for(const auto& member:plan.members)curve.affectedNoteIds.push_back(member.noteId);
    curve.leftNoteId=curve.affectedNoteIds.front();curve.rightNoteId=curve.affectedNoteIds.back();
    curve.fromSeconds=from;curve.toSeconds=to;curve.anchorSeconds=to;
    // Samples are the final request target, including ONE owner's vibrato. The
    // authoritative group/hop curves take precedence; typed bridges never add
    // their unrelated source pitch as a third musical line.
    const auto steps=std::clamp(int(std::ceil((to-from)/.001)),1,200000);
    for(int step=0;step<=steps;++step)
    {
        const auto t=from+(to-from)*step/steps;std::optional<std::size_t> owner;double best=-1.;
        for(std::size_t i=0;i<count;++i)if(eligible(i)&&t>=sources[i].from-1.e-9&&t<=sources[i].to+1.e-9)
        {
            const auto weight=nativeVirtualMemberContribution(plan,i,t);
            if(weight>best+1.e-12){best=weight;owner=i;}
        }
        float pitch=0,modulation=0;
        if(owner){pitch=baseAt(*owner,t);modulation=vibrato(*owner,t);}
        else
        {
            // Reference through a typed/UV bridge or an empty interval: source
            // UV stays UV in applyNativeSharedPitch; this cannot generate sound.
            std::optional<std::size_t> left,right;
            for(std::size_t i=0;i<count;++i)if(eligible(i))
            {
                if(sources[i].to<=t&&(!left||sources[i].to>sources[*left].to))left=i;
                if(sources[i].from>=t&&(!right||sources[i].from<sources[*right].from))right=i;
            }
            if(left&&right)
            {
                const auto a=sources[*left].to,b=sources[*right].from;
                const std::vector<PitchCurveEditPoint> reference{{a,baseAt(*left,a),PitchCurveShape::linear},{b,baseAt(*right,b),PitchCurveShape::linear}};
                pitch=evaluatePitchCurve(reference,t);
                modulation=std::lerp(vibrato(*left,a),vibrato(*right,b),float(b>a?std::clamp((t-a)/(b-a),0.,1.):0.));
            }
            else if(left){pitch=baseAt(*left,t);modulation=vibrato(*left,t);}
            else if(right){pitch=baseAt(*right,t);modulation=vibrato(*right,t);}
            else return {}; // all members unvoiced: no invented pitch evidence
        }
        for(const auto& source:sources)
            if(const auto* member=lines.memberFor(source.note->id);member&&member->line&&std::any_of(member->connectionPitchWindows.begin(),member->connectionPitchWindows.end(),[&](const auto& w){return t>=w.getStart()-1.e-9&&t<=w.getEnd()+1.e-9;}))
            {pitch=member->line->midiAt(t);break;}
        curve.points.push_back({t,juce::jlimit(0.f,127.f,pitch+modulation),PitchCurveShape::linear});
    }
    return {std::move(curve)};
}
// Pointer identity and non-overlapping draw spans: each connected target is
// rendered once, not a stack of per-member paths at the same screen pixels.
inline void bindNativeVirtualSinglePitch(SharedPitchLines& lines,const TrackData& track,const juce::String& groupId,
    const NativeVirtualConnectionPlan& plan)
{
    juce::ignoreUnused(groupId);if(plan.members.empty())return;
    const SharedPitchLineMember* first=nullptr;
    for(const auto& member:plan.members)if(const auto* entry=lines.memberFor(member.noteId);entry&&entry->virtualSinglePitch){first=entry;break;}
    if(!first||!first->line)return;
    auto common=std::make_shared<SharedPitchLine>(*first->line);
    double end=-std::numeric_limits<double>::infinity();for(const auto& source:plan.members)end=std::max(end,source.to);
    for(const auto& clip:track.clips)for(const auto& note:clip.notes)if(std::find_if(plan.members.begin(),plan.members.end(),[&](const auto& m){return m.noteId==note.id;})!=plan.members.end())end=std::max(end,clip.startSeconds+note.startSeconds+note.durationSeconds);
    std::erase_if(common->pieces,[&](const auto& piece){return piece.from>=end-1.e-9;}); // keep the final common endpoint, not the first member's restore
    struct Draw {juce::String id;double from,to;};std::vector<Draw> temporal;
    for(const auto& member:plan.members)
    {
        double from=member.from,to=member.to;
        for(const auto& clip:track.clips)for(const auto& note:clip.notes)if(note.id==member.noteId){from=std::min(from,clip.startSeconds+note.startSeconds);to=std::max(to,clip.startSeconds+note.startSeconds+note.durationSeconds);}
        temporal.push_back({member.noteId,from,to});
    }
    std::stable_sort(temporal.begin(),temporal.end(),[](const auto& a,const auto& b){return a.from<b.from;});
    double drawnTo=-std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<temporal.size();++i)
    {
        auto& member=lines.byNote[temporal[i].id];member.line=common;member.virtualSinglePitch=true;
        member.nativeSharedCurve=true;member.renderSharedCurve=true;member.connectionPitchOnly=false;
        member.drawFrom=std::max(temporal[i].from,drawnTo);
        member.drawTo=i+1<temporal.size()?std::max(member.drawFrom,std::min(temporal[i].to,temporal[i+1].from)):std::max(member.drawFrom,temporal[i].to);
        drawnTo=member.drawTo;
    }
}
}
