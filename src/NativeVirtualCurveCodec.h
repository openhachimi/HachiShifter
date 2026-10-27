#pragma once
#include "ProjectModel.h"
#include <array>
#include <algorithm>
#include <cmath>
namespace hachi
{
// V2 stores original interpolation controls with normalized time and pitch
// offset from the first anchor. Endpoint adaptation is an affine shear of the
// entire curve, NOT re-interpolating just its vertices (which changes shape).
inline bool decodeNativeVirtualCurveV2(const NativeVirtualGroup& group,double from,double to,float low,float high,
    std::vector<PitchCurveEditPoint>& output,juce::String& error)
{
    output.clear();if(group.pitchCurve.size()<2||!std::isfinite(from)||!std::isfinite(to)||std::abs(to-from)<.001)
    {error="Virtual controls need at least 1 ms of actual target span";return false;}
    struct Point
    {
        double x=0,y=0;Point()=default;Point(double a,double b):x(a),y(b){}
        Point operator+(Point b)const{return {x+b.x,y+b.y};}
        Point operator-(Point b)const{return {x-b.x,y-b.y};}
        Point operator*(double v)const{return {x*v,y*v};}
    };
    using Cubic=std::array<Point,4>;
    const auto& original=group.pitchCurve;const double tilt=double(high)-low-original.back().targetMidi;
    const auto transformed=[&](Point p){return Point(from+(to-from)*p.x,double(low)+p.y+tilt*p.x);};
    const auto cubicAt=[](const Cubic& c,double u){const auto v=1-u;return c[0]*(v*v*v)+c[1]*(3*v*v*u)+c[2]*(3*v*u*u)+c[3]*(u*u*u);};
    const auto inRange=[&](const Cubic& c)
    {
        double minimum=std::min(c[0].y,c[3].y),maximum=std::max(c[0].y,c[3].y);
        const auto a=-c[0].y+3*c[1].y-3*c[2].y+c[3].y,b=2*(c[0].y-2*c[1].y+c[2].y),d=c[1].y-c[0].y;
        const auto inspect=[&](double t){if(t>0&&t<1){const auto y=cubicAt(c,t).y;minimum=std::min(minimum,y);maximum=std::max(maximum,y);}};
        if(std::abs(a)<1.e-15){if(std::abs(b)>1.e-15)inspect(-d/b);}
        else{const auto discriminant=b*b-4*a*d;if(discriminant>=0){const auto root=std::sqrt(discriminant);inspect((-b-root)/(2*a));inspect((-b+root)/(2*a));}}
        return std::isfinite(minimum)&&std::isfinite(maximum)&&minimum>=-1.e-5&&maximum<=127.+1.e-5;
    };
    const auto emit=[&](auto&& self,Cubic c,int depth)->bool
    {
        if(c[3].x<c[0].x)std::reverse(c.begin(),c.end());
        if(!inRange(c)){error="Virtual curve exceeds the valid pitch range between controls";return false;}
        const auto span=c[3].x-c[0].x,dy=c[3].y-c[0].y;if(span<=1.e-10){error="Virtual Bezier time collapsed";return false;}
        const auto deviation=std::max(std::abs(c[1].y-c[0].y),std::abs(c[2].y-c[0].y));
        if(std::abs(dy)<1.e-6&&deviation>1.e-6)
        {
            if(depth>12){error="Degenerate virtual Bezier controls";return false;}
            // Equal endpoint pitch can still contain a real arch/S curve after
            // a live chord changes. Exact de Casteljau subdivision avoids a
            // zero-height normalized Bezier without flattening or sampling it.
            double u=.5;if(std::abs(cubicAt(c,u).y-c[0].y)<1.e-6)u=.25;
            const auto a=c[0]+(c[1]-c[0])*u,b=c[1]+(c[2]-c[1])*u,d=c[2]+(c[3]-c[2])*u;
            const auto e=a+(b-a)*u,f=b+(d-b)*u,m=e+(f-e)*u;
            return self(self,Cubic{c[0],a,e,m},depth+1)&&self(self,Cubic{m,f,d,c[3]},depth+1);
        }
        if(output.empty())output.push_back({c[0].x,float(c[0].y),PitchCurveShape::linear});
        PitchCurveEditPoint end{c[3].x,float(c[3].y),PitchCurveShape::customBezier};end.highPrecisionBezier=true;
        if(std::abs(dy)<1.e-6){end.shape=PitchCurveShape::linear;end.highPrecisionBezier=false;}
        else
        {
            end.bezierX1=float(std::clamp((c[1].x-c[0].x)/span,0.,1.));end.bezierX2=float(std::clamp((c[2].x-c[0].x)/span,0.,1.));
            end.bezierY1=float((c[1].y-c[0].y)/dy);end.bezierY2=float((c[2].y-c[0].y)/dy);
        }
        output.push_back(end);return true;
    };
    // Increasing project-time output even when members have crossed. Original
    // directed member IDs are untouched; reversing controls is an exact map.
    for(std::size_t step=1;step<original.size();++step)
    {
        const auto i=to>=from?step:original.size()-step;const auto& a=original[i-1];const auto& b=original[i];
        const auto span=b.timeSeconds-a.timeSeconds;Cubic c{Point(a.timeSeconds,a.targetMidi),{}, {},Point(b.timeSeconds,b.targetMidi)};
        if(b.shape==PitchCurveShape::customBezier)
        {const auto dy=double(b.targetMidi)-a.targetMidi;c[1]={a.timeSeconds+span*b.bezierX1,a.targetMidi+dy*b.bezierY1};c[2]={a.timeSeconds+span*b.bezierX2,a.targetMidi+dy*b.bezierY2};}
        else
        {
            // All existing non-Bezier shapes are cubics or lower in TIME.
            // Recover their exact cubic via the existing interpolation oracle,
            // not a new tangent/smoothing algorithm and not a sampled polyline.
            const double y0=a.targetMidi,y3=b.targetMidi,y1=evaluatePitchCurve(original,a.timeSeconds+span/3),y2=evaluatePitchCurve(original,a.timeSeconds+span*2/3);
            c[1]={a.timeSeconds+span/3,3*y1-1.5*y2-y0*5/6+y3/3};c[2]={a.timeSeconds+span*2/3,3*y2-1.5*y1+y0/3-y3*5/6};
        }
        for(auto& p:c)p=transformed(p);
        if(to>from&&std::abs(tilt)<1.e-8)
        {
            if(!inRange(c)){error="Virtual curve exceeds the valid pitch range between controls";return false;}
            if(output.empty()){auto start=a;start.timeSeconds=c[0].x;start.targetMidi=float(c[0].y);output.push_back(start);}
            auto end=b;end.timeSeconds=c[3].x;end.targetMidi=float(c[3].y);output.push_back(end);
        }
        else if(b.shape==PitchCurveShape::linear)
        {
            if(c[3].x<c[0].x)std::reverse(c.begin(),c.end());
            if(!inRange(c)){error="Virtual linear controls exceed pitch range";return false;}
            if(output.empty())output.push_back({c[0].x,float(c[0].y),PitchCurveShape::linear});output.push_back({c[3].x,float(c[3].y),PitchCurveShape::linear});
        }
        else if(!emit(emit,c,0))return false;
    }
    return !output.empty();
}
// Convert absolute editor grips through the SAME exact cubic/de Casteljau
// codec. A unit-height reference avoids dividing by the live endpoint delta,
// so a two-point flat segment can be raised without inventing a new DSP shape.
inline bool nativeVirtualCurveFromAbsoluteGrips(const PitchCurveEditPoint& first,const PitchCurveEditPoint& last,
    double x1,double y1,double x2,double y2,std::vector<PitchCurveEditPoint>& output,juce::String& error)
{
    const auto span=last.timeSeconds-first.timeSeconds;
    if(!std::isfinite(span)||span<=1.e-9){error="Invalid Bezier editor span";return false;}
    const auto u1=std::clamp((x1-first.timeSeconds)/span,0.,1.),u2=std::clamp((x2-first.timeSeconds)/span,0.,1.);
    const double shear=double(last.targetMidi)-first.targetMidi-1.;
    NativeVirtualGroup reference;reference.pitchCurveEncoding=2;
    PitchCurveEditPoint end{1.,1.f,PitchCurveShape::customBezier};end.highPrecisionBezier=true;
    end.bezierX1=float(u1);end.bezierX2=float(u2);
    end.bezierY1=float(y1-first.targetMidi-shear*u1);end.bezierY2=float(y2-first.targetMidi-shear*u2);
    reference.pitchCurve={{0.,0.f,PitchCurveShape::linear},end};
    return decodeNativeVirtualCurveV2(reference,first.timeSeconds,last.timeSeconds,first.targetMidi,last.targetMidi,output,error);
}
}
