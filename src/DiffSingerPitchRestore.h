#pragma once
#include "ProjectModel.h"
#include <algorithm>
#include <cmath>

namespace hachi
{
// Freeze the original Hermite tangent on an untouched boundary segment before
// replacing its neighbour. Its curve is identical, even after sampling density changes.
inline PitchCurveEditPoint frozenPitchSegment(const std::vector<PitchCurveEditPoint>& points, size_t right)
{
    auto point=points[right];
    if(right==0 || point.shape!=PitchCurveShape::natural) return point;
    const auto tangent=[&](size_t i) {
        if(i==0 || i+1>=points.size()) return 0.f;
        const auto a=std::max(1e-9,points[i].timeSeconds-points[i-1].timeSeconds);
        const auto b=std::max(1e-9,points[i+1].timeSeconds-points[i].timeSeconds);
        const auto x=(points[i].targetMidi-points[i-1].targetMidi)/static_cast<float>(a);
        const auto y=(points[i+1].targetMidi-points[i].targetMidi)/static_cast<float>(b);
        if(x==0 || y==0 || std::signbit(x)!=std::signbit(y)) return 0.f;
        return static_cast<float>((3*(a+b))/((2*b+a)/x+(b+2*a)/y));
    };
    const auto dy=point.targetMidi-points[right-1].targetMidi;
    if(std::abs(dy)<1e-8f) {point.shape=PitchCurveShape::linear;return point;}
    const auto duration=static_cast<float>(point.timeSeconds-points[right-1].timeSeconds);
    point.shape=PitchCurveShape::customBezier;
    point.bezierX1=1.f/3;point.bezierX2=2.f/3;
    point.bezierY1=tangent(right-1)*duration/(3*dy);
    point.bezierY2=1-tangent(right)*duration/(3*dy);
    return point;
}

inline std::vector<PitchCurveEditPoint> restoredDiffSingerPitchPoint(
    const NoteData& note, const std::vector<PitchCurveEditPoint>& current, int index)
{
    const auto& reference=note.diffSingerPitchReference;
    if(reference.empty() || index<0 || index>=static_cast<int>(current.size())
        || current[static_cast<size_t>(index)].diffSingerRestoreSupport) return {};
    auto previous=index-1,next=index+1;
    while(previous>=0 && current[static_cast<size_t>(previous)].diffSingerRestoreSupport) --previous;
    while(next<static_cast<int>(current.size()) && current[static_cast<size_t>(next)].diffSingerRestoreSupport) ++next;
    const auto hasLeft=previous>=0, hasRight=next<static_cast<int>(current.size());
    const auto from=hasLeft ? current[static_cast<size_t>(previous)].timeSeconds
        : std::min({0.0,current.front().timeSeconds,reference.front().timeSeconds});
    const auto to=hasRight ? current[static_cast<size_t>(next)].timeSeconds
        : std::max({note.durationSeconds,current.back().timeSeconds,reference.back().timeSeconds});
    if(to-from<1e-7) return {};
    const auto clicked=current[static_cast<size_t>(index)].timeSeconds;
    const auto leftFade=hasLeft ? std::min(.01,std::max(0.,clicked-from)*.25) : 0.;
    const auto rightFade=hasRight ? std::min(.01,std::max(0.,to-clicked)*.25) : 0.;
    const auto leftDelta=hasLeft ? current[static_cast<size_t>(previous)].targetMidi-evaluatePitchCurve(reference,from) : 0.f;
    const auto rightDelta=hasRight ? current[static_cast<size_t>(next)].targetMidi-evaluatePitchCurve(reference,to) : 0.f;
    const auto value=[&](double t) {
        auto midi=evaluatePitchCurve(reference,t);
        const auto tail=[](double u) {u=juce::jlimit(0.,1.,u);return static_cast<float>(1-u*u*(3-2*u));};
        if(leftFade>0 && t<from+leftFade) midi+=leftDelta*tail((t-from)/leftFade);
        if(rightFade>0 && t>to-rightFade) midi+=rightDelta*tail((to-t)/rightFade);
        return juce::jlimit(0.f,127.f,midi);
    };
    std::vector<PitchCurveEditPoint> result;
    for(int i=0;i<=previous;++i)
        result.push_back(i==previous ? frozenPitchSegment(current,static_cast<size_t>(i)) : current[static_cast<size_t>(i)]);
    const auto add=[&](double t,float midi) {
        PitchCurveEditPoint p{t,midi,PitchCurveShape::linear};p.diffSingerRestoreSupport=true;result.push_back(p);
    };
    if(!hasLeft) add(from,value(from));
    std::vector<double> breaks{from,to,clicked};
    if(leftFade>0)breaks.push_back(from+leftFade);
    if(rightFade>0)breaks.push_back(to-rightFade);
    for(const auto& p:reference)if(p.timeSeconds>from && p.timeSeconds<to)breaks.push_back(p.timeSeconds);
    std::sort(breaks.begin(),breaks.end());
    breaks.erase(std::unique(breaks.begin(),breaks.end(),[](double a,double b){return std::abs(a-b)<1e-9;}),breaks.end());
    const auto sample=[&](auto&& self,double a,double b,int depth)->void {
        const auto va=value(a),vb=value(b);auto error=0.f;
        for(const auto u:{.25,.5,.75})error=std::max(error,std::abs(value(a+(b-a)*u)-static_cast<float>(va+(vb-va)*u)));
        if(depth<16 && b-a>1e-5 && (error>.0005f || b-a>.02))
        {const auto mid=(a+b)*.5;self(self,a,mid,depth+1);self(self,mid,b,depth+1);}
        else add(b,vb);
    };
    for(size_t i=1;i<breaks.size();++i)sample(sample,breaks[i-1],breaks[i],0);
    if(hasRight)
    {
        result.back()=current[static_cast<size_t>(next)];result.back().shape=PitchCurveShape::linear;
        for(size_t i=static_cast<size_t>(next+1);i<current.size();++i)
            result.push_back(i==static_cast<size_t>(next+1) ? frozenPitchSegment(current,i) : current[i]);
    }
    return result;
}
}
