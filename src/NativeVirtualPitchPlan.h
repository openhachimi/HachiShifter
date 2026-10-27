#pragma once
#include "NativeVirtualConnectionPlan.h"
#include "NativeVirtualSharedPitch.h"
#include "NativeVirtualCurveCodec.h"
#include "NativeVirtualGroupPlan.h"

namespace hachi
{
inline bool validateNativeVirtualPitchEncoding(const NativeVirtualGroup& group,juce::String& error)
{
    error.clear();if(group.pitchCurveEncoding!=1&&group.pitchCurveEncoding!=2){error="Unsupported virtual pitch curve encoding";return false;}
    for(const auto& hop:group.hopPitchCurves)
    {
        NativeVirtualGroup payload;payload.pitchCurve=hop.pitchCurve;payload.pitchCurveEncoding=hop.pitchCurveEncoding;
        payload.pitchFromOffsetSeconds=hop.pitchFromOffsetSeconds;payload.pitchToOffsetSeconds=hop.pitchToOffsetSeconds;
        if(hop.leftNoteId.isEmpty()||hop.rightNoteId.isEmpty()||hop.pitchCurve.empty()||!validateNativeVirtualPitchEncoding(payload,error))return false;
    }
    if(group.pitchCurve.empty())return true;
    if(group.pitchCurve.size()<2||group.pitchCurve.size()>2048
        ||!std::isfinite(group.pitchFromOffsetSeconds)||!std::isfinite(group.pitchToOffsetSeconds)
        ||std::abs(group.pitchFromOffsetSeconds)>3600||std::abs(group.pitchToOffsetSeconds)>3600)
    {error="Invalid virtual pitch encoding bounds";return false;}
    double previous=-1.;
    for(const auto& p:group.pitchCurve)
    {
        if(!std::isfinite(p.timeSeconds)||p.timeSeconds<0||p.timeSeconds>1||p.timeSeconds<=previous
            ||!std::isfinite(p.targetMidi)||std::abs(p.targetMidi)>127
            ||!std::isfinite(p.bezierX1)||!std::isfinite(p.bezierY1)||!std::isfinite(p.bezierX2)||!std::isfinite(p.bezierY2)
            ||p.bezierX1<0||p.bezierX1>1||p.bezierX2<0||p.bezierX2>1
            ||int(p.shape)<int(PitchCurveShape::natural)||int(p.shape)>int(PitchCurveShape::customBezier))
        {error="Invalid normalized virtual pitch controls";return false;}
        previous=p.timeSeconds;
    }
    if(std::abs(group.pitchCurve.front().timeSeconds)>1.e-8||std::abs(group.pitchCurve.back().timeSeconds-1)>1.e-8
        ||std::abs(group.pitchCurve.front().targetMidi)>1.e-4||(group.pitchCurveEncoding==1&&std::abs(group.pitchCurve.back().targetMidi)>1.e-4))
    {error="Virtual pitch endpoints must follow the live source chord";return false;}
    return true;
}
struct NativeVirtualPitchPlan
{
    juce::String groupId,issue;
    std::vector<juce::String> noteIds;
    std::vector<PitchCurveEditPoint> points; // absolute project target time/MIDI
    bool active=false,manual=false,includeBaseCurve=false,conflictingWindows=false;
    std::vector<NativeVirtualPitchPlan> segments; // disjoint per-hop windows, never one whole-chain chord
};
inline void isolateNativeVirtualPitchWindowConflicts(NativeVirtualPitchPlan& plan)
{
    std::vector<bool> conflicts(plan.segments.size(),false);
    for(std::size_t i=0;i<plan.segments.size();++i)for(std::size_t j=0;j<i;++j)
    {
        const auto& a=plan.segments[i];const auto& b=plan.segments[j];
        if(!a.active||!b.active||a.points.empty()||b.points.empty())continue;
        const bool incident=std::any_of(a.noteIds.begin(),a.noteIds.end(),[&](const auto& id){return std::find(b.noteIds.begin(),b.noteIds.end(),id)!=b.noteIds.end();});
        if(incident&&std::min(a.points.back().timeSeconds,b.points.back().timeSeconds)>std::max(a.points.front().timeSeconds,b.points.front().timeSeconds)+1.e-7)
            conflicts[i]=conflicts[j]=true;
    }
    for(std::size_t i=0;i<conflicts.size();++i)if(conflicts[i])
    {
        plan.conflictingWindows=true;plan.segments[i].active=false;
        plan.segments[i].issue="Incident virtual hop curve windows overlap; shorten a window explicitly";
    }
    if(plan.conflictingWindows)plan.issue="Incident virtual hop curve windows overlap; bindings and authored controls retained, conflicting pitch windows suspended";
}
// One common target bridge for the selected route, not two competing pitch
// writers on a three-source overlap. No source voicing, music or clocks change.
// The eventual renderer must still retain the request's original unvoiced mask.
inline NativeVirtualPitchPlan planNativeVirtualPitch(const TrackData& track,
    const NativeVirtualGroup& group,const NativeVirtualConnectionPlan& geometry,
    double automaticFrom=-std::numeric_limits<double>::infinity(),double automaticTo=std::numeric_limits<double>::infinity())
{
    NativeVirtualPitchPlan out;out.groupId=group.id;out.manual=!group.pitchCurve.empty();
    if(!group.enabled||(!group.automaticPitch&&group.pitchCurve.empty()&&!group.sharedPitch&&group.hopPitchCurves.empty()))return out;
    if(!validateNativeVirtualPitchEncoding(group,out.issue))return out;
    if(!std::isfinite(group.pitchDurationSeconds)||group.pitchDurationSeconds<.002||group.pitchDurationSeconds>30)
    {out.issue="Invalid virtual glide duration";return out;}
    if(geometry.members.size()!=group.orderedNoteIds.size()||geometry.handovers.empty())
    {out.issue="Missing validated virtual geometry";return out;}
    const bool mapped=!geometry.contributionRanges.empty();
    const auto& roles=mapped?geometry.contributionMembers:geometry.members;
    if((roles.front().transition||roles.back().transition)&&group.pitchCurve.empty()&&group.hopPitchCurves.empty())
    {out.noteIds=group.orderedNoteIds;return out;} // incomplete hop: canonical single line holds the ordinary reference
    const bool oneBridge=roles.size()==3&&roles[1].transition;
    if(!group.sharedPitch&&group.pitchCurve.empty()&&((roles.size()>2&&!oneBridge)||!group.hopPitchCurves.empty()))
    {
        out.noteIds=group.orderedNoteIds;
        const auto hops=nativeVirtualPitchHops(group,geometry);
        for(const auto& hop:hops)
        {
            auto segment=planNativeVirtualPitch(track,hop.group,hop.geometry);
            if(segment.active)out.segments.push_back(std::move(segment));
            else if(segment.issue.isNotEmpty()){if(out.issue.isNotEmpty())out.issue+="; ";out.issue+=segment.issue;}
        }
        // Adjacent automatic margins can overlap by less than one frame on
        // a short middle syllable. Partition ONLY auto/auto windows, then seek
        // real voiced anchors again through the SAME local Hermite planner.
        // Never clip sampled PCM, trim authored controls, or bypass conflicts.
        std::vector<std::pair<double,double>> bounds(out.segments.size(),{-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()});
        for(std::size_t i=0;i<out.segments.size();++i)for(std::size_t j=0;j<i;++j)
        {
            const auto& a=out.segments[i];const auto& b=out.segments[j];if(a.manual||b.manual||a.points.empty()||b.points.empty())continue;
            const bool incident=std::any_of(a.noteIds.begin(),a.noteIds.end(),[&](const auto& id){return std::find(b.noteIds.begin(),b.noteIds.end(),id)!=b.noteIds.end();});
            const auto from=std::max(a.points.front().timeSeconds,b.points.front().timeSeconds),to=std::min(a.points.back().timeSeconds,b.points.back().timeSeconds);
            if(!incident||to<=from+1.e-7)continue;const auto seam=(from+to)*.5;
            const auto earlier=a.points.front().timeSeconds<b.points.front().timeSeconds?i:j,later=earlier==i?j:i;
            bounds[earlier].second=std::min(bounds[earlier].second,seam);bounds[later].first=std::max(bounds[later].first,seam);
        }
        for(std::size_t i=0;i<out.segments.size();++i)if(std::isfinite(bounds[i].first)||std::isfinite(bounds[i].second))
        {
            const auto hop=std::find_if(hops.begin(),hops.end(),[&](const auto& candidate){return candidate.group.id==out.segments[i].groupId;});
            if(hop==hops.end())continue;auto adjusted=planNativeVirtualPitch(track,hop->group,hop->geometry,bounds[i].first,bounds[i].second);
            if(adjusted.active)out.segments[i]=std::move(adjusted); // unsupported anchors retain explicit conflict diagnostics
        }
        isolateNativeVirtualPitchWindowConflicts(out);
        for(const auto& segment:out.segments)if(segment.active)out.points.insert(out.points.end(),segment.points.begin(),segment.points.end());
        std::stable_sort(out.points.begin(),out.points.end(),[](const auto& a,const auto& b){return a.timeSeconds<b.timeSeconds;});
        out.active=!out.points.empty();return out;
    }
    struct Endpoint {const NoteData* note=nullptr;double origin=0;};
    std::vector<Endpoint> endpoints;
    for(std::size_t i=0;i<geometry.members.size();++i)
    {
        const auto& member=geometry.members[i];
        if(member.noteId!=group.orderedNoteIds[i]){out.issue="Pitch route and source route disagree";return out;}
        Endpoint endpoint;
        for(const auto& parent:track.clips)for(const auto& note:parent.notes)if(note.id==member.noteId)
        {if(endpoint.note){out.issue="Ambiguous virtual pitch endpoint";return out;}endpoint={&note,parent.startSeconds+note.startSeconds};}
        if(!endpoint.note){out.issue="Missing virtual pitch endpoint";return out;}
        // A hand-edited note is never silently reshaped by import optimization.
        if(group.pitchCurve.empty()&&!group.sharedPitch&&(endpoint.note->pitchSupportAuthored||!endpoint.note->pitchControlPoints.empty()
            ||std::any_of(endpoint.note->contour.begin(),endpoint.note->contour.end(),[](const auto& p){return p.hasManualTarget;})))
        {out.issue="Manual note pitch is protected; edit the virtual curve explicitly";return out;}
        endpoints.push_back(endpoint);out.noteIds.push_back(member.noteId);
    }
    if(group.sharedPitch&&group.pitchCurve.empty())
    {
        const auto lines=nativeVirtualSharedPitchLines(track,group);
        const SharedPitchLineMember* shared=nullptr;
        for(const auto& id:group.orderedNoteIds)if(const auto* member=lines.memberFor(id)){shared=member;break;}
        if(shared&&shared->line)
        {
            double from=std::numeric_limits<double>::infinity(),to=-from;
            for(const auto& member:geometry.members){from=std::min(from,member.from);to=std::max(to,member.to);}
            if(std::isfinite(from)&&std::isfinite(to)&&to>from)
            {
                const auto count=std::clamp(int(std::ceil((to-from)/.001)),1,100000);
                for(int i=0;i<=count;++i){const auto t=from+(to-from)*i/count;out.points.push_back({t,shared->line->midiAt(t),PitchCurveShape::linear});}
            }
        }
        else out.issue="Shared virtual source line is unavailable";
        for(auto hop:nativeVirtualPitchHops(group,geometry))if(!hop.group.pitchCurve.empty())
        {
            hop.group.automaticPitch=false;auto segment=planNativeVirtualPitch(track,hop.group,hop.geometry);
            if(segment.active)out.segments.push_back(std::move(segment));
        }
        isolateNativeVirtualPitchWindowConflicts(out);
        const bool hasLocal=std::any_of(out.segments.begin(),out.segments.end(),[](const auto& segment){return segment.active;});
        out.includeBaseCurve=!out.points.empty()&&!out.segments.empty();out.active=!out.points.empty()||hasLocal;return out;
    }
    struct Anchor {double time=0;float midi=0;double slope=0;};
    const auto findAnchor=[&](const Endpoint& endpoint,double desired,double low,double high)->std::optional<Anchor>
    {
        const auto& note=*endpoint.note;if(note.nativeUnpitched||note.contour.size()<2)return {};
        const auto own=ownPitchPoints(note);if(own.empty())return {};
        std::optional<Anchor> best;double distance=std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i+1<note.contour.size();++i)
        {
            const auto& a=note.contour[i];const auto& b=note.contour[i+1];
            // Adjacent stable voiced evidence only, never an isolated spike,
            // an extrapolated empty contour or a long gap in detection.
            if(!a.voiced||!b.voiced||!std::isfinite(a.timeSeconds)||!std::isfinite(b.timeSeconds)
                ||!std::isfinite(a.relativeCents)||!std::isfinite(b.relativeCents)
                ||b.timeSeconds-a.timeSeconds<.001||b.timeSeconds-a.timeSeconds>.030)continue;
            const auto local=std::clamp(desired-endpoint.origin,a.timeSeconds,b.timeSeconds);
            const auto time=endpoint.origin+local;
            if(local<0||local>note.durationSeconds||time<low-1.e-8||time>high+1.e-8)continue;
            const auto midi=evaluatePitchCurve(own,local);
            const auto first=evaluatePitchCurve(own,a.timeSeconds),last=evaluatePitchCurve(own,b.timeSeconds);
            if(!std::isfinite(midi)||midi<0||midi>127||!std::isfinite(first)||!std::isfinite(last))continue;
            const auto d=std::abs(time-desired);if(d>=distance)continue;
            distance=d;best=Anchor{time,midi,(last-first)/(b.timeSeconds-a.timeSeconds)};
        }
        return best;
    };
    if(!group.pitchCurve.empty())
    {
        const auto from=geometry.members.front().to+group.pitchFromOffsetSeconds;
        const auto to=geometry.members.back().from+group.pitchToOffsetSeconds;
        const auto left=findAnchor(endpoints.front(),from,from-1.e-7,from+1.e-7);
        const auto right=findAnchor(endpoints.back(),to,to-1.e-7,to+1.e-7);
        if(!left||!right||(group.pitchCurveEncoding==2?std::abs(to-from):to-from)<.001){out.issue="Saved virtual curve retains its binding but needs supported voiced anchors";return out;}
        if(group.pitchCurveEncoding==2)
        {out.active=decodeNativeVirtualCurveV2(group,from,to,left->midi,right->midi,out.points,out.issue);return out;}
        for(auto p:group.pitchCurve)
        {
            const auto u=p.timeSeconds;p.timeSeconds=from+u*(to-from);p.targetMidi+=float(left->midi+(right->midi-left->midi)*u);
            if(!std::isfinite(p.targetMidi)||p.targetMidi<0||p.targetMidi>127){out.points.clear();out.issue="Moved virtual curve exceeds pitch range";return out;}
            out.points.push_back(p);
        }
        out.active=true;return out;
    }
    // Keep endpoints near the actual handover. For a wholly unvoiced bridge,
    // reference pitch may cross the bridge, while only real voiced requests
    // can consume it. Never seek arbitrarily far into an adjacent syllable.
    const auto half=group.pitchDurationSeconds*.5;
    const auto leftBoundary=geometry.handovers.front().from;
    const auto rightBoundary=geometry.handovers.back().to;
    const auto leftLow=std::max(leftBoundary-group.pitchDurationSeconds,automaticFrom),leftHigh=std::min(leftBoundary+half,automaticTo);
    const auto rightLow=std::max(rightBoundary-half,automaticFrom),rightHigh=std::min(rightBoundary+group.pitchDurationSeconds,automaticTo);
    if(leftLow>leftHigh||rightLow>rightHigh){out.issue="No supported automatic pitch margin between adjacent windows";return out;}
    const auto left=findAnchor(endpoints.front(),std::clamp(leftBoundary-half,leftLow,leftHigh),leftLow,leftHigh);
    const auto right=findAnchor(endpoints.back(),std::clamp(rightBoundary+half,rightLow,rightHigh),rightLow,rightHigh);
    if(!left||!right){out.issue="No stable voiced evidence near both virtual endpoints";return out;}
    const auto span=right->time-left->time;
    if(span<.001){out.issue="Virtual voiced anchors have no positive target span";return out;}
    // Cubic Hermite honours live target values and local trend. Bound only
    // pathological slopes; no global pitch-centre or vibrato rewrite occurs.
    const auto secant=(right->midi-left->midi)/span;
    const auto limit=std::max(12.,std::abs(secant)*3.);
    const auto m0=std::clamp(left->slope,-limit,limit),m1=std::clamp(right->slope,-limit,limit);
    const auto segments=std::clamp(int(std::ceil(span/.005-1.e-9)),2,2047);
    for(int i=0;i<=segments;++i)
    {
        const auto u=double(i)/segments,u2=u*u,u3=u2*u;
        const auto midi=(2*u3-3*u2+1)*left->midi+(u3-2*u2+u)*span*m0
            +(-2*u3+3*u2)*right->midi+(u3-u2)*span*m1;
        if(!std::isfinite(midi)||midi<0||midi>127){out.points.clear();out.issue="Virtual glide exceeds the valid pitch range";return out;}
        PitchCurveEditPoint p;p.timeSeconds=left->time+span*u;p.targetMidi=float(midi);p.shape=PitchCurveShape::linear;out.points.push_back(p);
    }
    out.active=true;return out;
}
inline bool prepareNativeVirtualPitchCurve(const TrackData& track,const NativeVirtualGroup& group,const NativeVirtualConnectionPlan& geometry,
    const std::vector<PitchCurveEditPoint>& points,NativeVirtualGroup& candidate,juce::String& error)
{
    error.clear();if(points.size()<2||points.size()>2048){error="A virtual curve needs 2..2048 points";return false;}
    const auto from=points.front().timeSeconds,to=points.back().timeSeconds;const auto low=points.front().targetMidi,high=points.back().targetMidi;
    if(!std::isfinite(from)||!std::isfinite(to)||to-from<.001){error="A virtual curve needs supported target time";return false;}
    candidate=group;candidate.pitchCurve.clear();candidate.pitchCurveEncoding=2;
    candidate.pitchFromOffsetSeconds=from-geometry.members.front().to;candidate.pitchToOffsetSeconds=to-geometry.members.back().from;
    double previous=-std::numeric_limits<double>::infinity();
    for(auto point:points)
    {
        if(!std::isfinite(point.timeSeconds)||!std::isfinite(point.targetMidi)||point.timeSeconds<=previous||point.targetMidi<0||point.targetMidi>127){error="Invalid virtual pitch controls";return false;}
        previous=point.timeSeconds;point.timeSeconds=(point.timeSeconds-from)/(to-from);point.targetMidi-=low;candidate.pitchCurve.push_back(point);
    }
    candidate.pitchCurve.front().timeSeconds=0;candidate.pitchCurve.back().timeSeconds=1;candidate.pitchCurve.front().targetMidi=0;candidate.pitchCurve.back().targetMidi=high-low;
    if(!validateNativeVirtualPitchEncoding(candidate,error))return false;
    const auto reconstructed=planNativeVirtualPitch(track,candidate,geometry);
    if(!reconstructed.active){error=reconstructed.issue;return false;}
    if(std::abs(reconstructed.points.front().targetMidi-low)>1.e-4||std::abs(reconstructed.points.back().targetMidi-high)>1.e-4){error="Keep the live endpoint anchors at supported times";return false;}
    return true;
}
template<class A,class B> inline bool sameNativeVirtualPitchControls(const A& a,const B& b)
{
    if(a.pitchCurveEncoding!=b.pitchCurveEncoding||a.pitchFromOffsetSeconds!=b.pitchFromOffsetSeconds||a.pitchToOffsetSeconds!=b.pitchToOffsetSeconds||a.pitchCurve.size()!=b.pitchCurve.size())return false;
    for(std::size_t i=0;i<a.pitchCurve.size();++i)
    {const auto& x=a.pitchCurve[i];const auto& y=b.pitchCurve[i];if(x.timeSeconds!=y.timeSeconds||x.targetMidi!=y.targetMidi||x.shape!=y.shape||x.bezierX1!=y.bezierX1||x.bezierY1!=y.bezierY1||x.bezierX2!=y.bezierX2||x.bezierY2!=y.bezierY2||x.highPrecisionBezier!=y.highPrecisionBezier)return false;}
    return true;
}
inline ResolvedConnectionPitchCurve resolvedNativeVirtualPitch(const NativeVirtualPitchPlan& plan)
{
    ResolvedConnectionPitchCurve curve;curve.connectionId=plan.groupId;curve.issue=plan.issue;
    if(!plan.active||plan.noteIds.size()<2||plan.points.size()<2)return curve;
    curve.active=true;curve.leftNoteId=plan.noteIds.front();curve.rightNoteId=plan.noteIds.back();
    curve.affectedNoteIds=plan.noteIds;curve.points=plan.points;curve.fromSeconds=plan.points.front().timeSeconds;
    curve.toSeconds=plan.points.back().timeSeconds;curve.anchorSeconds=curve.toSeconds;return curve;
}
inline std::vector<ResolvedConnectionPitchCurve> nativeVirtualPitchCurvesForTrack(const TrackData& track,const std::vector<NativeVirtualGroup>& groups,bool manualOnly=false)
{
    std::vector<ResolvedConnectionPitchCurve> curves;
    for(const auto& group:groups)if(group.enabled&&group.trackId==track.id&&(!manualOnly||!group.pitchCurve.empty()||!group.hopPitchCurves.empty()))
    {
        NativeVirtualConnectionPlan geometry;juce::String issue;if(!resolveNativeVirtualGroup(track,group,geometry,issue))continue;
        const auto pitch=planNativeVirtualPitch(track,group,geometry);if(!pitch.active)continue;
        if((pitch.segments.empty()||pitch.includeBaseCurve)&&(!manualOnly||pitch.manual))curves.push_back(resolvedNativeVirtualPitch(pitch));
        for(const auto& segment:pitch.segments)if(segment.active&&(!manualOnly||segment.manual))curves.push_back(resolvedNativeVirtualPitch(segment));
    }
    return curves;
}
// A group curve is a request input even though original notes remain unchanged.
// Hash only curves affecting this clip, never add a suffix to legacy/off paths.
inline std::string nativeVirtualPitchCacheSuffix(const std::vector<ResolvedConnectionPitchCurve>& curves,const ClipData& clip)
{
    juce::MemoryOutputStream stream;
    for(const auto& curve:curves)if(curve.active&&std::any_of(clip.notes.begin(),clip.notes.end(),[&](const auto& note)
        {return std::find(curve.affectedNoteIds.begin(),curve.affectedNoteIds.end(),note.id)!=curve.affectedNoteIds.end();}))
    {
        stream.writeString(curve.connectionId);stream.writeInt(int(curve.points.size()));
        for(const auto& p:curve.points)
        {stream.writeDouble(p.timeSeconds);stream.writeFloat(p.targetMidi);stream.writeInt(int(p.shape));stream.writeFloat(p.bezierX1);stream.writeFloat(p.bezierY1);stream.writeFloat(p.bezierX2);stream.writeFloat(p.bezierY2);stream.writeBool(p.highPrecisionBezier);}
    }
    if(stream.getDataSize()==0)return {};
    std::uint64_t hash=1469598103934665603ull;
    const auto* bytes=static_cast<const unsigned char*>(stream.getData());
    for(std::size_t i=0;i<stream.getDataSize();++i){hash^=bytes[i];hash*=1099511628211ull;}
    return "|virtual-pitch="+juce::String::toHexString(static_cast<juce::int64>(hash)).toStdString();
}
}
