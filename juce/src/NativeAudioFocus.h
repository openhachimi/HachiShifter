#pragma once
#include "NativeAudioOverlap.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace hachi
{
inline bool nativeAudioClipsOverlap(const ClipData& a,const ClipData& b)
{
    const auto windows=[](const ClipData& c){std::vector<juce::Range<double>> result;
        const auto add=[&](const ClipData& source,double base){if(source.audioLength()>1.e-9&&source.sourceFile!=juce::File{})
            result.emplace_back(base+source.audioStartSeconds,base+source.audioStartSeconds+source.audioLength());};
        if(c.parts.empty())add(c,c.startSeconds);else for(const auto& part:c.parts)add(part,c.startSeconds+part.startSeconds);
        return result;};
    for(const auto& x:windows(a))for(const auto& y:windows(b))if(x.getIntersectionWith(y).getLength()>1.e-7)return true;
    return false;
}
// View children of a linked parent share a clip ID. A note ID distinguishes
// their source ownership; the arrangement instead focuses a complete region.
inline float nativeOverlapFocusOpacity(const TrackData& track,const ClipData& candidate,
                                       const juce::String& noteId,const juce::String& clipId)
{
    if(!trackShowsAllNativeRegions(track))return 1.f;
    const auto hasNote=[&](const auto& c){return noteId.isNotEmpty()&&std::any_of(c.notes.begin(),c.notes.end(),[&](const auto& n){return n.id==noteId;});};
    const auto noteFound=std::any_of(track.clips.begin(),track.clips.end(),hasNote);
    const auto active=[&](const auto& c){return noteFound?hasNote(c):clipId.isNotEmpty()&&c.id==clipId;};
    if(active(candidate))return 1.f;
    for(const auto& c:track.clips)if(active(c)&&nativeAudioClipsOverlap(candidate,c))return .27f;
    return 1.f;
}
inline std::vector<const ClipData*> nativeAudioFocusOrder(const TrackData& track,const juce::String& noteId,const juce::String& clipId)
{
    std::vector<const ClipData*> result;for(const auto& c:track.clips)result.push_back(&c);
    std::stable_partition(result.begin(),result.end(),[&](const auto* c){return nativeOverlapFocusOpacity(track,*c,noteId,clipId)<1.f;});
    return result;
}
struct NativeAudioFocusLayer
{
    juce::Graphics& graphics;bool dim;
    NativeAudioFocusLayer(juce::Graphics& g,float opacity):graphics(g),dim(opacity<.999f){if(dim)graphics.beginTransparencyLayer(opacity);}
    ~NativeAudioFocusLayer(){if(dim)graphics.endTransparencyLayer();}
};
}
