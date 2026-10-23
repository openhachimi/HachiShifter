#pragma once
#include "ProjectModel.h"
#include <algorithm>
#include <cmath>

namespace hachi
{
// The middle of the lane is unity. Its upper half covers boost to +12 dB,
// the lower half covers attenuation to silence. Interpolate this displayed
// level so inserting a point on a straight segment never changes its sound.
inline float trackGainLevelFromDb(float db)
{ return juce::jlimit(-1.0f, 1.0f, db/(db < 0.0f ? 60.0f : 12.0f)); }
inline float trackGainDbFromLevel(float level)
{ level=juce::jlimit(-1.0f,1.0f,level);return level*(level < 0.0f ? 60.0f : 12.0f); }
inline bool sameTrackGainEnvelope(const std::vector<TrackGainPoint>& a, const std::vector<TrackGainPoint>& b)
{
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y)
    {return std::abs(x.timeSeconds-y.timeSeconds)<1.0e-7 && std::abs(x.gainDb-y.gainDb)<1.0e-5f;});
}
inline std::vector<TrackGainPoint> normaliseTrackGainEnvelope(std::vector<TrackGainPoint> points)
{
    std::erase_if(points,[](const auto& p){return !std::isfinite(p.timeSeconds)||!std::isfinite(p.gainDb);});
    // Negative/outside points are retained for trimmed audio and synthesis pre-roll.
    // The editor only exposes points inside the region's visible boundaries.
    for(auto& p:points)p.gainDb=juce::jlimit(-60.0f,12.0f,p.gainDb);
    std::stable_sort(points.begin(),points.end(),[](const auto& a,const auto& b){return a.timeSeconds<b.timeSeconds;});
    std::vector<TrackGainPoint> result;
    for(const auto& p:points)
        if(!result.empty()&&std::abs(result.back().timeSeconds-p.timeSeconds)<1.0e-6)result.back()=p;
        else result.push_back(p);
    return result;
}
inline float trackGainEnvelopeDbAt(const std::vector<TrackGainPoint>& points,double seconds)
{
    if(points.empty())return 0.0f;
    const auto right=std::lower_bound(points.begin(),points.end(),seconds,[](const auto& p,double t){return p.timeSeconds<t;});
    if(right==points.begin())return right->gainDb;
    if(right==points.end())return points.back().gainDb;
    const auto& left=*(right-1);
    const auto amount=static_cast<float>(juce::jlimit(0.0,1.0,(seconds-left.timeSeconds)/std::max(1.0e-9,right->timeSeconds-left.timeSeconds)));
    const auto a=trackGainLevelFromDb(left.gainDb),b=trackGainLevelFromDb(right->gainDb);
    return trackGainDbFromLevel(a+(b-a)*amount);
}
inline float trackGainEnvelopeAt(const std::vector<TrackGainPoint>& points,double seconds)
{ return points.empty()?1.0f:juce::Decibels::decibelsToGain(trackGainEnvelopeDbAt(points,seconds),-60.0f); }

inline void shiftGainEnvelope(std::vector<GainEnvelopePoint>& points,double delta)
{ for(auto& point:points)point.timeSeconds+=delta; }
inline void shiftClipGainEnvelopes(ClipData& clip,double delta)
{
    shiftGainEnvelope(clip.gainEnvelope,delta);
    for(auto& layer:clip.inheritedGainEnvelopes)shiftGainEnvelope(layer,delta);
}
inline void anchorClipGainEnvelope(ClipData& clip)
{
    if(clip.gainEnvelope.empty())return;
    const auto start=trackGainEnvelopeDbAt(clip.gainEnvelope,0);
    const auto end=trackGainEnvelopeDbAt(clip.gainEnvelope,clip.durationSeconds);
    clip.gainEnvelope.push_back({0,start});clip.gainEnvelope.push_back({clip.durationSeconds,end});
    clip.gainEnvelope=normaliseTrackGainEnvelope(std::move(clip.gainEnvelope));
}
inline float clipGainEnvelopeAt(const ClipData& clip,double localSeconds)
{
    auto gain=trackGainEnvelopeAt(clip.gainEnvelope,localSeconds);
    for(const auto& layer:clip.inheritedGainEnvelopes)gain*=trackGainEnvelopeAt(layer,localSeconds);
    return gain;
}
inline void writeClipGainEnvelope(juce::ValueTree& tree,const ClipData& clip)
{
    const auto append=[](juce::ValueTree& target,const auto& points)
    {
        for(const auto& point:points)
        {
            juce::ValueTree child("ClipGainPoint");
            child.setProperty("timeSeconds",point.timeSeconds,nullptr);child.setProperty("gainDb",point.gainDb,nullptr);
            target.addChild(child,-1,nullptr);
        }
    };
    append(tree,clip.gainEnvelope);
    for(const auto& layer:clip.inheritedGainEnvelopes)
    {
        juce::ValueTree source("InheritedGainEnvelope");append(source,layer);tree.addChild(source,-1,nullptr);
    }
}
inline void readClipGainEnvelope(const juce::ValueTree& tree,ClipData& clip)
{
    const auto read=[](const juce::ValueTree& source)
    {
        std::vector<GainEnvelopePoint> points;
        for(const auto child:source)if(child.hasType("ClipGainPoint"))
            points.push_back({static_cast<double>(child.getProperty("timeSeconds",0.0)),static_cast<float>(child.getProperty("gainDb",0.0))});
        return normaliseTrackGainEnvelope(std::move(points));
    };
    clip.gainEnvelope=read(tree);
    for(const auto source:tree)if(source.hasType("InheritedGainEnvelope"))
    {auto points=read(source);if(!points.empty())clip.inheritedGainEnvelopes.push_back(std::move(points));}
}
}
