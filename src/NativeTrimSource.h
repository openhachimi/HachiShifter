#pragma once
#include "NativeNoteTiming.h"
#include "NativePitchIdentity.h"

namespace hachi
{
inline double nativeClockExtrapolated(const std::vector<SourceTimePoint>& clock, double time, bool inverse=false)
{
    if(clock.size()<2) return time;
    const auto x=[&](const auto& p){return inverse?p.sourceSeconds:p.targetSeconds;};
    const auto y=[&](const auto& p){return inverse?p.targetSeconds:p.sourceSeconds;};
    auto right=std::upper_bound(clock.begin(),clock.end(),time,[&](double t,const auto& p){return t<x(p);});
    if(right==clock.begin())++right;
    if(right==clock.end())--right;
    const auto& a=*std::prev(right);const auto& b=*right;
    const auto span=x(b)-x(a);
    return span>1.e-12?y(a)+(y(b)-y(a))*(time-x(a))/span:y(a);
}

inline std::shared_ptr<const std::vector<NativeSourcePitchPoint>> nativeSourcePitchReference(
    const std::vector<NoteData>& notes, const std::function<double(double)>& sourceAt)
{
    auto points=std::make_shared<std::vector<NativeSourcePitchPoint>>();
    std::optional<double> previousEnd;
    for(const auto& note:notes)
    {
        const auto from=sourceAt(note.startSeconds);
        if(previousEnd && from>*previousEnd+1.e-5)
        {
            points->push_back({*previousEnd+1.e-8,0,0,false});
            points->push_back({from-1.e-8,0,0,false});
        }
        previousEnd=sourceAt(note.startSeconds+note.durationSeconds);
        const auto centre=note.sourceMidiCenter>=0?note.sourceMidiCenter:note.midiNote;
        for(const auto& p:note.contour)
            points->push_back({sourceAt(note.startSeconds+p.timeSeconds),centre+p.relativeCents/100,
                centre+p.withoutVibratoCents/100,p.voiced});
    }
    std::stable_sort(points->begin(),points->end(),[](const auto& a,const auto& b){return a.sourceSeconds<b.sourceSeconds;});
    std::vector<NativeSourcePitchPoint> unique;
    for(const auto& p:*points)
        if(!unique.empty()&&std::abs(unique.back().sourceSeconds-p.sourceSeconds)<1.e-8)unique.back()=p;
        else unique.push_back(p);
    *points=std::move(unique);return points;
}

inline void rememberNativeTrimSource(ClipData& clip)
{
    const auto clock=nativeClipClock(clip);
    // A later stretch may introduce new breakpoints at the retained edges.
    // The current audio clock is authoritative inside its window; retain only
    // the hidden anchors outside it, then splice in the current source map.
    if(!clock.empty())
    {
        const auto first=clock.front(),last=clock.back();
        std::vector<SourceTimePoint> complete;
        for(const auto& p:clip.nativeTrimClock)
            if((p.targetSeconds<first.targetSeconds-1.e-8&&p.sourceSeconds<clip.sourceOffsetSeconds+first.sourceSeconds-1.e-8)
                ||(p.targetSeconds>last.targetSeconds+1.e-8&&p.sourceSeconds>clip.sourceOffsetSeconds+last.sourceSeconds+1.e-8))complete.push_back(p);
        for(auto p:clock){p.sourceSeconds+=clip.sourceOffsetSeconds;complete.push_back(p);}
        std::stable_sort(complete.begin(),complete.end(),[](const auto& a,const auto& b){return a.targetSeconds<b.targetSeconds;});
        clip.nativeTrimClock=std::move(complete);
    }
    if(!clip.nativeSourcePitch || clip.nativeSourcePitch->empty())
        clip.nativeSourcePitch=nativeSourcePitchReference(clip.notes,[&](double t){return clip.sourceOffsetSeconds+nativeSourceTimeAt(clock,t);});
}

inline void rememberNativeTrimSources(ClipData& clip)
{
    if(clip.parts.empty()){rememberNativeTrimSource(clip);return;}
    for(auto source:expandedClipParts(clip))
    {
        rememberNativeTrimSource(source);
        for(auto& part:clip.parts)if(clip.id+":"+part.id==source.id)
        {
            part.nativeSourcePitch=source.nativeSourcePitch;
            part.nativeSourcePitchComplete=source.nativeSourcePitchComplete;
            part.nativeTrimClock=source.nativeTrimClock;
            const auto shift=source.startSeconds-clip.startSeconds-part.startSeconds;
            for(auto& p:part.nativeTrimClock)p.targetSeconds+=shift;
        }
    }
}

inline std::optional<NativeSourcePitchPoint> nativeReferencePitchAt(const ClipData& clip,double source)
{
    if(!clip.nativeSourcePitch || clip.nativeSourcePitch->empty())return {};
    const auto& points=*clip.nativeSourcePitch;
    if(source<points.front().sourceSeconds-1.e-7 || source>points.back().sourceSeconds+1.e-7)return {};
    auto right=std::lower_bound(points.begin(),points.end(),source,[](const auto& p,double t){return p.sourceSeconds<t;});
    if(right==points.end())return points.back();
    if(right==points.begin()||std::abs(right->sourceSeconds-source)<1.e-8)return *right;
    const auto& a=*std::prev(right);auto p=*right;
    const auto u=static_cast<float>((source-a.sourceSeconds)/(p.sourceSeconds-a.sourceSeconds));
    p.midi=a.midi+(p.midi-a.midi)*u;p.withoutVibratoMidi=a.withoutVibratoMidi+(p.withoutVibratoMidi-a.withoutVibratoMidi)*u;
    p.voiced &= a.voiced;return p;
}

inline void refreshNativeTrimPitch(NoteData& note,ClipData& clip)
{
    const auto clock=nativeClipClock(clip);
    const auto centre=note.sourceMidiCenter>=0?note.sourceMidiCenter:note.midiNote;
    for(auto& p:note.contour)
    {
        const auto source=clip.sourceOffsetSeconds+nativeSourceTimeAt(clock,note.startSeconds+p.timeSeconds);
        const auto ref=nativeReferencePitchAt(clip,source);
        p.voiced=ref&&ref->voiced;
        if(ref){p.relativeCents=(ref->midi-centre)*100;p.withoutVibratoCents=(ref->withoutVibratoMidi-centre)*100;}
    }
    note.sourceMidiCenter=centre;note.sourcePitchMeasured=true;
}

inline void extendNativeTrimContour(NoteData& note,const NoteData& before,ClipData& clip,double headAdded)
{
    const auto clock=nativeClipClock(clip);
    const auto centre=before.sourceMidiCenter>=0?before.sourceMidiCenter:before.midiNote;
    const auto unedited=nativePitchIsUnedited(before);
    std::vector<PitchPoint> points;
    const auto add=[&](double t)
    {
        const auto local=t-headAdded;
        if(local>=-1.e-8&&local<=before.durationSeconds+1.e-8)return;
        PitchPoint p;p.timeSeconds=t;
        const auto source=clip.sourceOffsetSeconds+nativeSourceTimeAt(clock,note.startSeconds+t);
        const auto ref=nativeReferencePitchAt(clip,source);p.voiced=ref&&ref->voiced;
        if(ref){p.relativeCents=(ref->midi-centre)*100;p.withoutVibratoCents=(ref->withoutVibratoMidi-centre)*100;}
        else if(!clip.nativeSourcePitchComplete)clip.nativeSourcePitchPending=true;
        if(!unedited&&!before.contour.empty())
        {
            const auto& boundary=local<0?before.contour.front():before.contour.back();
            p.hasManualTarget=true;p.manualTargetCents=renderedPitchCents(before,boundary);
        }
        points.push_back(p);
    };
    for(double t=0;t<note.durationSeconds-1.e-9;t+=.005)add(t);
    add(note.durationSeconds);
    for(auto p:before.contour){p.timeSeconds+=headAdded;points.push_back(p);}
    std::stable_sort(points.begin(),points.end(),[](const auto& a,const auto& b){return a.timeSeconds<b.timeSeconds;});
    note.contour=std::move(points);
}

inline void writeNativeTrimReference(juce::ValueTree& tree,const ClipData& clip)
{
    if(clip.nativeSourcePitch && !clip.nativeSourcePitch->empty())
    {
        juce::MemoryOutputStream buffer;buffer.writeInt(static_cast<int>(clip.nativeSourcePitch->size()));
        for(const auto& p:*clip.nativeSourcePitch)
        {buffer.writeDouble(p.sourceSeconds);buffer.writeFloat(p.midi);buffer.writeFloat(p.withoutVibratoMidi);buffer.writeBool(p.voiced);}
        juce::ValueTree reference("NativeSourcePitchReference");reference.setProperty("data",juce::var(buffer.getMemoryBlock()),nullptr);
        reference.setProperty("complete",clip.nativeSourcePitchComplete,nullptr);tree.addChild(reference,-1,nullptr);
    }
    if(clip.nativeSourcePitchPending)tree.setProperty("nativeSourcePitchPending",true,nullptr);
    for(const auto& p:clip.nativeTrimClock)
    {juce::ValueTree point("NativeTrimClockPoint");point.setProperty("target",p.targetSeconds,nullptr);point.setProperty("source",p.sourceSeconds,nullptr);tree.addChild(point,-1,nullptr);}
}

inline void readNativeTrimReference(const juce::ValueTree& tree,ClipData& clip)
{
    clip.nativeSourcePitchPending=static_cast<bool>(tree.getProperty("nativeSourcePitchPending",false));
    for(const auto child:tree)
    {
        if(child.hasType("NativeTrimClockPoint"))
        {
            const auto t=static_cast<double>(child.getProperty("target")),s=static_cast<double>(child.getProperty("source"));
            if(std::isfinite(t)&&std::isfinite(s)&&s>=0)clip.nativeTrimClock.push_back({t,s});
        }
        if(child.hasType("NativeSourcePitchReference"))
        {
            const auto data=child.getProperty("data");const auto* bytes=data.getBinaryData();if(!bytes)continue;
            juce::MemoryInputStream input(*bytes,false);const auto count=input.readInt();
            if(count<1||count>2000000||bytes->getSize()<4+static_cast<std::size_t>(count)*17)continue;
            auto points=std::make_shared<std::vector<NativeSourcePitchPoint>>();points->reserve(static_cast<std::size_t>(count));
            for(int i=0;i<count;++i)
            {
                NativeSourcePitchPoint p{input.readDouble(),input.readFloat(),input.readFloat(),input.readBool()};
                if(!std::isfinite(p.sourceSeconds)||!std::isfinite(p.midi)||!std::isfinite(p.withoutVibratoMidi)
                    ||p.sourceSeconds<0||(!points->empty()&&p.sourceSeconds<=points->back().sourceSeconds))break;
                points->push_back(p);
            }
            if(points->size()==static_cast<std::size_t>(count))
            {clip.nativeSourcePitch=points;clip.nativeSourcePitchComplete=static_cast<bool>(child.getProperty("complete",false));}
        }
    }
    std::stable_sort(clip.nativeTrimClock.begin(),clip.nativeTrimClock.end(),[](const auto& a,const auto& b){return a.targetSeconds<b.targetSeconds;});
    for(std::size_t i=1;i<clip.nativeTrimClock.size();++i)
        if(clip.nativeTrimClock[i].targetSeconds<=clip.nativeTrimClock[i-1].targetSeconds
            ||clip.nativeTrimClock[i].sourceSeconds<=clip.nativeTrimClock[i-1].sourceSeconds){clip.nativeTrimClock.clear();break;}
}
}
