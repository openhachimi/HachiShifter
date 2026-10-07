#pragma once
#include "NativeAudioLink.h"
namespace hachi
{
struct NativeAudioWindow { juce::String owner; double start=0,end=0; };
inline std::vector<NativeAudioWindow> nativeAudioWindows(const TrackData& track)
{
    std::vector<NativeAudioWindow> result;
    for(const auto& clip:track.clips)
    {
        const auto add=[&](const ClipData& source,double offset){if(source.audioLength()>1.e-9&&source.sourceFile!=juce::File{})
            result.push_back({clip.id,offset+source.audioStartSeconds,offset+source.audioStartSeconds+source.audioLength()});};
        if(clip.parts.empty())add(clip,clip.startSeconds);
        else for(const auto& part:clip.parts)add(part,clip.startSeconds+part.startSeconds);
    }
    return result;
}
inline double nativeCollisionScale(const std::vector<NativeAudioWindow>& before,const std::vector<NativeAudioWindow>& after)
{
    if(before.size()!=after.size())return 0;
    double scale=1;
    for(std::size_t i=0;i<before.size();++i)
    {
        if(std::abs(before[i].start-after[i].start)<1.e-9&&std::abs(before[i].end-after[i].end)<1.e-9)continue;
        for(std::size_t j=0;j<before.size();++j)
        {
            if(i==j)continue;
            const auto constrain=[&](double gap,double nextGap){if(gap>=-1.e-7&&nextGap< -1.e-9)
                scale=std::min(scale,std::max(0.0,gap)/(gap-nextGap));};
            if(before[i].end<=before[j].start+1.e-7)constrain(before[j].start-before[i].end,after[j].start-after[i].end);
            if(before[j].end<=before[i].start+1.e-7)constrain(before[i].start-before[j].end,after[i].start-after[j].end);
        }
    }
    return juce::jlimit(0.0,1.0,scale);
}
inline double constrainedNativeNoteDelta(const TrackData& track,const std::vector<juce::String>& ids,double delta,
                                         NativeNoteTimeEdit edit=NativeNoteTimeEdit::move)
{
    if(track.allowNativeAudioOverlap||!trackShowsAllNativeRegions(track)||std::abs(delta)<1.e-9)return delta;
    auto candidate=track;
    for(auto& clip:candidate.clips)if(auto plan=planNativeNoteMove(clip,ids,delta,0,edit))clip=std::move(plan->clip);
    return delta*nativeCollisionScale(nativeAudioWindows(track),nativeAudioWindows(candidate));
}
inline double constrainedNativeClipDelta(const TrackData& track,const std::vector<juce::String>& ids,double delta)
{
    if(track.allowNativeAudioOverlap||!trackShowsAllNativeRegions(track))return delta;
    auto candidate=track;for(auto& clip:candidate.clips)if(std::find(ids.begin(),ids.end(),clip.id)!=ids.end())clip.startSeconds+=delta;
    return delta*nativeCollisionScale(nativeAudioWindows(track),nativeAudioWindows(candidate));
}
inline void constrainNativeClipResize(const TrackData& track,const juce::String& id,double& start,double& duration)
{
    if(!trackShowsAllNativeRegions(track)||track.allowNativeAudioOverlap)return;
    auto candidate=track;const auto old=std::find_if(track.clips.begin(),track.clips.end(),[&](const auto& c){return c.id==id;});if(old==track.clips.end())return;
    for(auto& c:candidate.clips)if(c.id==id)
    {
        const auto ratio=duration/c.durationSeconds;c.startSeconds=start;c.durationSeconds=duration;c.audioStartSeconds*=ratio;
        if(c.audioDurationSeconds>=0)c.audioDurationSeconds*=ratio;
        for(auto& p:c.parts){p.startSeconds*=ratio;p.durationSeconds*=ratio;p.audioStartSeconds*=ratio;if(p.audioDurationSeconds>=0)p.audioDurationSeconds*=ratio;}
    }
    const auto scale=nativeCollisionScale(nativeAudioWindows(track),nativeAudioWindows(candidate));
    start=old->startSeconds+(start-old->startSeconds)*scale;duration=old->durationSeconds+(duration-old->durationSeconds)*scale;
}
inline bool nativeMovingClipsCollide(const ProjectData& data,const std::vector<juce::String>& ids,double delta,int rows)
{
    for(int row=0;row<static_cast<int>(data.tracks.size());++row)for(const auto& moving:data.tracks[static_cast<std::size_t>(row)].clips)
    {
        if(std::find(ids.begin(),ids.end(),moving.id)==ids.end())continue;
        const auto target=row+rows;if(target<0||target>=static_cast<int>(data.tracks.size()))return true;
        const auto& track=data.tracks[static_cast<std::size_t>(target)];if(!trackShowsAllNativeRegions(track)||track.allowNativeAudioOverlap)continue;
        auto one=track;one.clips={moving};one.clips[0].startSeconds+=delta;
        for(const auto& a:nativeAudioWindows(one))for(const auto& b:nativeAudioWindows(track))
            if(std::find(ids.begin(),ids.end(),b.owner)==ids.end()&&std::min(a.end,b.end)-std::max(a.start,b.start)>1.e-7)return true;
    }
    return false;
}
inline double constrainedNativeClipMoves(const ProjectData& data,const std::vector<juce::String>& ids,double delta,int& rows)
{
    if(rows!=0&&!nativeMovingClipsCollide(data,ids,delta,rows))return delta;
    rows=0;
    for(const auto& track:data.tracks){const auto safe=constrainedNativeClipDelta(track,ids,delta);delta=delta<0?std::max(delta,safe):std::min(delta,safe);}
    return delta;
}
inline double nativePasteStart(const TrackData& track,const std::vector<ClipData>& clips,double start)
{
    if(track.allowNativeAudioOverlap||!trackShowsAllNativeRegions(track))return start;
    auto candidate=track;candidate.clips=clips;const auto moving=nativeAudioWindows(candidate),fixed=nativeAudioWindows(track);
    for(std::size_t pass=0;pass<=moving.size()*fixed.size();++pass)
    {
        double advance=0;for(const auto& a:moving)for(const auto& b:fixed)
            if(std::min(a.end+start,b.end)-std::max(a.start+start,b.start)>1.e-7)advance=std::max(advance,b.end-a.start-start);
        if(advance<=1.e-9)break;start+=advance;
    }
    return start;
}
inline std::optional<ClipData> cropNativeAudioHead(ClipData original,double absoluteStart)
{
    const auto begin=absoluteStart-original.startSeconds;
    if(begin<=1.e-9)return original;
    if(begin>=original.durationSeconds-1.e-9)return {};
    original.startSeconds=0;original.sourceTimeMap=nativeClipClock(original);
    auto pieces=slicedClipParts({original},begin,original.durationSeconds,false,true);if(pieces.empty())return {};
    auto result=std::move(pieces.front());result.startSeconds=absoluteStart;result.nativeAudioLinked=false;
    for(auto note:original.notes)
    {
        const auto end=note.startSeconds+note.durationSeconds;
        if(end<=begin+1.e-9)continue;
        const auto cut=std::max(0.0,begin-note.startSeconds);
        if(cut>0)
        {
            // Negative automation anchors retain the exact incoming Bezier and
            // gain interpolation at the crop. Dense source pitch starts at zero.
            const auto shift=[&](auto& points){for(auto& p:points)p.timeSeconds-=cut;};
            if(!note.contour.empty())
            {
                auto boundary=note.contour.front();const auto next=std::lower_bound(note.contour.begin(),note.contour.end(),cut,
                    [](const auto& p,double t){return p.timeSeconds<t;});
                if(next==note.contour.end())boundary=note.contour.back();
                else if(next==note.contour.begin())boundary=*next;
                else {const auto& a=*(next-1);const auto& b=*next;const auto u=static_cast<float>((cut-a.timeSeconds)/(b.timeSeconds-a.timeSeconds));
                    boundary=a;boundary.relativeCents=a.relativeCents+(b.relativeCents-a.relativeCents)*u;
                    boundary.withoutVibratoCents=a.withoutVibratoCents+(b.withoutVibratoCents-a.withoutVibratoCents)*u;
                    boundary.manualTargetCents=a.manualTargetCents+(b.manualTargetCents-a.manualTargetCents)*u;
                    boundary.hasManualTarget=a.hasManualTarget&&b.hasManualTarget;boundary.voiced=a.voiced&&b.voiced;}
                std::erase_if(note.contour,[&](const auto& p){return p.timeSeconds<=cut+1.e-9;});shift(note.contour);
                boundary.timeSeconds=0;note.contour.insert(note.contour.begin(),boundary);
            }
            shift(note.pitchControlPoints);shift(note.amplitudeEnvelope);shift(note.diffSingerPitchReference);shift(note.diffSingerPitchOffset);
            for(auto& c:note.utauFlagCurves)shift(c.points);
            for(auto& t:note.sibilantMarkers)t-=cut;std::erase_if(note.sibilantMarkers,[](double t){return t<0;});
            note.startSeconds+=cut;note.durationSeconds-=cut;note.consonantSeconds=std::max(0.0,note.consonantSeconds-cut);
            note.connectedToPrevious=false;note.utauAutoPitchTransition=false;
            bindNativeNoteSource(note,original,&original.sourceTimeMap);
        }
        note.startSeconds-=begin;note.clipPartId.clear();result.notes.push_back(std::move(note));
    }
    return result;
}
inline bool removeNativeAudioOverlaps(TrackData& track)
{
    struct Source { ClipData clip;std::size_t owner; };
    std::vector<Source> units;for(std::size_t i=0;i<track.clips.size();++i)
        for(auto source:expandedClipParts(track.clips[i]))units.push_back({std::move(source),i});
    std::stable_sort(units.begin(),units.end(),[](const auto& a,const auto& b){return a.clip.startSeconds+a.clip.audioStartSeconds<b.clip.startSeconds+b.clip.audioStartSeconds;});
    double covered=0;bool changed=false;std::vector<std::vector<ClipData>> retained(track.clips.size());
    for(auto& unit:units)
    {
        auto source=unit.clip;
        if(source.audioLength()<=1.e-9||source.sourceFile==juce::File{}){retained[unit.owner].push_back(std::move(source));continue;}
        const auto end=source.startSeconds+source.audioStartSeconds+source.audioLength();
        if(source.audioLength()>1.e-9&&source.startSeconds+source.audioStartSeconds<covered-1.e-7)
        {
            changed=true;
            if(end<=covered+1.e-9)continue;
            auto cropped=cropNativeAudioHead(source,covered);if(!cropped)continue;source=std::move(*cropped);
        }
        covered=std::max(covered,end);retained[unit.owner].push_back(std::move(source));
    }
    if(!changed)return false;
    std::vector<ClipData> clips;
    for(std::size_t i=0;i<retained.size();++i)
    {
        if(retained[i].empty())continue;
        auto value=retained[i].size()==1?retained[i].front():assembledLinkedAudio(retained[i]);
        value.id=track.clips[i].id;value.nativeAudioLinked=track.clips[i].nativeAudioLinked&&value.parts.size()>1;
        clips.push_back(std::move(value));
    }
    track.clips=std::move(clips);return true;
}
}
