#pragma once
#include "AdvancedEnvelope.h"
#include "MixedEnvelopeIO.h"
namespace hachi::backend
{
struct NativeEnvelopeSettings
{
    int mode=0;
    TailFadeSettings shape=[] {TailFadeSettings s;s.endGain=1;s.mixed=true;s.allowRise=true;return s;}();
    bool valid() const {return mode>=0&&mode<=2&&shape.valid()&&shape.head.mode==0;}
    bool operator==(const NativeEnvelopeSettings&) const = default;
};
inline float nativeEnvelopeGain(const NativeEnvelopeSettings& s,double time,double duration)
{return s.mode==0?1.0f:tailFadeGain(s.mode,time,0,duration,s.shape);}
inline juce::var nativeEnvelopeToVar(const NativeEnvelopeSettings& s)
{
    auto* o=new juce::DynamicObject();o->setProperty("mode",s.mode);const auto& v=s.shape;
    o->setProperty("range",juce::var(juce::Array<juce::var>{v.startFraction,v.endFraction,v.startGain,v.endGain,v.curvePower}));
    o->setProperty("custom",v.customCurve);
    o->setProperty("curve",envelopeBezierToVar({v.control1Time,v.control1Progress,v.control2Time,v.control2Progress}));
    o->setProperty("mixed",mixedEnvelopeToVar(v));return juce::var(o);
}
inline NativeEnvelopeSettings nativeEnvelopeFromVar(const juce::var& value)
{
    NativeEnvelopeSettings s;if(!value.isObject())return s;
    const auto range=value.getProperty("range",{});if(!range.isArray()||range.size()!=5)return s;
    for(const auto& v:*range.getArray())if(!v.isDouble()&&!v.isInt()&&!v.isInt64())return s;
    s.mode=(int)value.getProperty("mode",0);auto& v=s.shape;
    v.startFraction=(double)range[0];v.endFraction=(double)range[1];v.startGain=(double)range[2];v.endGain=(double)range[3];v.curvePower=(double)range[4];
    v.customCurve=(bool)value.getProperty("custom",false);EnvelopeBezier b;
    if(!envelopeBezierFromVar(value.getProperty("curve",{}),b)||!mixedEnvelopeFromVar(value.getProperty("mixed",{}),v))return {};
    v.control1Time=b.control1Time;v.control1Progress=b.control1Progress;v.control2Time=b.control2Time;v.control2Progress=b.control2Progress;
    return s.valid()?s:NativeEnvelopeSettings{};
}
// Crop the sparse cubic itself, so split/trim does not restart its fade.
inline NativeEnvelopeSettings slicedNativeEnvelope(const NativeEnvelopeSettings& original,double from,double to)
{
    from=std::clamp(from,0.0,1.0);to=std::clamp(to,from,1.0);
    if(original.mode==0||(from<=1.e-12&&to>=1-1.e-12)||to-from<1.e-12)return original;
    struct P {double x,y;};struct Segment {P a,b,c,d;bool curved;};
    const auto lerp=[](P a,P b,double t){return P{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};};
    const auto split=[&](Segment s,double t){const auto ab=lerp(s.a,s.b,t),bc=lerp(s.b,s.c,t),cd=lerp(s.c,s.d,t),abc=lerp(ab,bc,t),bcd=lerp(bc,cd,t),m=lerp(abc,bcd,t);return std::pair{Segment{s.a,ab,abc,m,s.curved},Segment{m,bcd,cd,s.d,s.curved}};};
    const auto param=[&](const Segment& s,double x){double lo=0,hi=1;for(int i=0;i<40;++i){const auto t=(lo+hi)*.5;if(split(s,t).first.d.x<x)lo=t;else hi=t;}return (lo+hi)*.5;};
    std::vector<Segment> segments;const auto& v=original.shape;
    const auto add=[&](double a,double b,double ga,double gb,bool curved,EnvelopeBezier c={}){
        if(b-a<1.e-12)return;
        if(!curved)c={1.0/3,1.0/3,2.0/3,2.0/3};
        segments.push_back({{a,ga},{a+(b-a)*c.control1Time,ga+(gb-ga)*c.control1Progress},{a+(b-a)*c.control2Time,ga+(gb-ga)*c.control2Progress},{b,gb},curved});
    };
    add(0,v.startFraction,v.startGain,v.startGain,false);
    if(original.mode==2&&v.mixed)
    {
        double t=0,g=v.startGain;bool curved=v.firstSegmentCurved;auto b=v.firstBezier;
        for(size_t i=0;i<=v.knots.size();++i){const auto nt=i<v.knots.size()?v.knots[i].time:1.0,ng=i<v.knots.size()?v.knots[i].gain:v.endGain;
            add(v.startFraction+(v.endFraction-v.startFraction)*t,v.startFraction+(v.endFraction-v.startFraction)*nt,g,ng,curved,b);
            if(i<v.knots.size()){t=nt;g=ng;curved=v.knots[i].curvedToNext;b=v.knots[i].bezier;}}
    }
    else if(original.mode==2&&!v.customCurve&&v.curvePower!=1)
    {
        for(int i=0;i<48;++i){const auto a=v.startFraction+(v.endFraction-v.startFraction)*i/48.0,b=v.startFraction+(v.endFraction-v.startFraction)*(i+1)/48.0;add(a,b,tailFadeGain(original.mode,a,0,1,v),tailFadeGain(original.mode,b,0,1,v),false);}
    }
    else add(v.startFraction,v.endFraction,v.startGain,v.endGain,original.mode==2,v.customCurve?EnvelopeBezier{v.control1Time,v.control1Progress,v.control2Time,v.control2Progress}:EnvelopeBezier{});
    add(v.endFraction,1,v.endGain,v.endGain,false);
    NativeEnvelopeSettings result;result.mode=2;auto& out=result.shape;bool first=true;
    for(auto s:segments)
    {
        if(s.d.x<=from||s.a.x>=to)continue;
        if(s.d.x>to)s=split(s,param(s,to)).first;
        if(s.a.x<from)s=split(s,param(s,from)).second;
        const auto dx=s.d.x-s.a.x,dy=s.d.y-s.a.y;
        EnvelopeBezier b{std::clamp((s.b.x-s.a.x)/dx,0.0,1.0),std::abs(dy)>1.e-12?std::clamp((s.b.y-s.a.y)/dy,0.0,1.0):0,
            std::clamp((s.c.x-s.a.x)/dx,0.0,1.0),std::abs(dy)>1.e-12?std::clamp((s.c.y-s.a.y)/dy,0.0,1.0):1};
        if(first){out.startGain=s.a.y;out.firstSegmentCurved=s.curved;out.firstBezier=b;first=false;}
        else out.knots.push_back({(s.a.x-from)/(to-from),s.a.y,s.curved,b});
        out.endGain=s.d.y;
    }
    return result;
}
template<class Point> std::vector<Point> nativeEnvelopePicture(const std::vector<Point>& base,const NativeEnvelopeSettings& settings,double duration)
{
    if(settings.mode==0||base.empty()||duration<=0)return base;
    std::vector<double> times;for(const auto& p:base)times.push_back(p.timeSeconds);
    const auto& s=settings.shape;
    std::vector<double> edges{0,s.startFraction};for(const auto& p:s.knots)edges.push_back(s.startFraction+(s.endFraction-s.startFraction)*p.time);edges.push_back(s.endFraction);edges.push_back(1);
    for(size_t i=1;i<edges.size();++i)for(int j=0;j<=64;++j)times.push_back(duration*(edges[i-1]+(edges[i]-edges[i-1])*j/64.0));
    std::sort(times.begin(),times.end());times.erase(std::unique(times.begin(),times.end()),times.end());std::vector<Point> result;
    for(const auto t:times)
    {
        const auto after=std::upper_bound(base.begin(),base.end(),t,[](double at,const auto& p){return at<p.timeSeconds;});
        auto db=base.front().gainDb;if(after==base.end())db=base.back().gainDb;else if(after!=base.begin()){const auto& a=*std::prev(after);db=envelopeDbBetween(a.gainDb,after->gainDb,(float)((t-a.timeSeconds)/(after->timeSeconds-a.timeSeconds)),a.linearToNext);}
        result.push_back({t,envelopeDbFromGain(envelopeGainFromDb(db)*nativeEnvelopeGain(settings,t,duration)),true});
    }
    return result;
}
}
