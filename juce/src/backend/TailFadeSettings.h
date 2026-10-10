#pragma once
#include <cmath>
#include <vector>
namespace hachi::backend
{
struct EnvelopeBezier
{
    double control1Time=1.0/3.0,control1Progress=0;
    double control2Time=2.0/3.0,control2Progress=1;
    bool valid() const
    {
        return std::isfinite(control1Time)&&std::isfinite(control2Time)
            &&std::isfinite(control1Progress)&&std::isfinite(control2Progress)
            &&control1Time>=0&&control1Time<=control2Time&&control2Time<=1
            &&control1Progress>=0&&control1Progress<=control2Progress&&control2Progress<=1;
    }
    bool operator==(const EnvelopeBezier&) const = default;
};
struct EnvelopeKnot
{
    double time = .5, gain = .5; // Relative time inside the selected OTO range.
    bool curvedToNext = false;
    EnvelopeBezier bezier;
    bool operator==(const EnvelopeKnot&) const = default;
};
inline bool validEnvelopeKnots(const std::vector<EnvelopeKnot>& points)
{
    if(points.size()>128)return false;
    double previous=0;
    for(const auto& p:points)
    {
        if(!std::isfinite(p.time)||!std::isfinite(p.gain)||p.time<=previous
            ||p.time>=1||p.gain<0||p.gain>2||!p.bezier.valid())return false;
        previous=p.time;
    }
    return true;
}
// Fractions follow the allocated OTO tail, never fixed timeline seconds.
struct HeadEnvelopeSettings
{
    double startFraction = 0.0, endFraction = 1.0;
    double startGain = 0.0, endGain = 1.0;
    int mode = 0;
    double curvePower = 1.0;
    bool customCurve = false;
    double control1Time = 1.0/3.0, control1Progress = 0.0;
    double control2Time = 2.0/3.0, control2Progress = 1.0;
    bool mixed = false, firstSegmentCurved = false;
    std::vector<EnvelopeKnot> knots;
    EnvelopeBezier firstBezier;
    bool valid() const
    {
        return firstBezier.valid() && validEnvelopeKnots(knots) && std::isfinite(startFraction) && std::isfinite(endFraction)
            && std::isfinite(startGain) && std::isfinite(endGain) && std::isfinite(curvePower)
            && startFraction >= 0 && endFraction <= 1 && endFraction-startFraction >= .001
            && startGain >= 0 && startGain <= 2 && endGain >= 0 && endGain <= 2 && mode >= 0 && mode <= 2
            && curvePower >= .25 && curvePower <= 4
            && std::isfinite(control1Time) && std::isfinite(control2Time)
            && std::isfinite(control1Progress) && std::isfinite(control2Progress)
            && control1Time >= 0 && control1Time <= control2Time && control2Time <= 1
            && control1Progress >= 0 && control1Progress <= control2Progress && control2Progress <= 1;
    }
    bool operator==(const HeadEnvelopeSettings& b) const
    { return mode==b.mode && startFraction==b.startFraction && endFraction==b.endFraction
        && startGain==b.startGain && endGain==b.endGain && curvePower==b.curvePower
        && customCurve==b.customCurve && control1Time==b.control1Time && control2Time==b.control2Time
        && control1Progress==b.control1Progress && control2Progress==b.control2Progress
        && mixed==b.mixed && firstSegmentCurved==b.firstSegmentCurved && knots==b.knots && firstBezier==b.firstBezier; }
    bool operator!=(const HeadEnvelopeSettings& b) const { return !(*this==b); }
};
struct TailFadeSettings
{
    // Appended below the historical aggregate fields to preserve their initializers.
    double startFraction = 0.0, endFraction = 1.0;
    double startGain = 1.0, endGain = 0.0;
    double curvePower = 1.0;
    bool customCurve = false;
    double control1Time = 1.0/3.0, control1Progress = 0.0;
    double control2Time = 2.0/3.0, control2Progress = 1.0;
    HeadEnvelopeSettings head;
    bool mixed = false, firstSegmentCurved = false;
    std::vector<EnvelopeKnot> knots;
    EnvelopeBezier firstBezier;
    bool allowRise=false;
    bool valid() const
    {
        return head.valid() && firstBezier.valid() && validEnvelopeKnots(knots) && std::isfinite(startFraction) && std::isfinite(endFraction)
            && std::isfinite(startGain) && std::isfinite(endGain) && std::isfinite(curvePower)
            && startFraction >= 0 && endFraction <= 1 && endFraction-startFraction >= .001
            && startGain >= 0 && startGain <= 2 && endGain >= 0 && endGain <= (mixed||allowRise?2:startGain)
            && curvePower >= .25 && curvePower <= 4
            && std::isfinite(control1Time) && std::isfinite(control2Time)
            && std::isfinite(control1Progress) && std::isfinite(control2Progress)
            && control1Time >= 0 && control1Time <= control2Time && control2Time <= 1
            && control1Progress >= 0 && control1Progress <= control2Progress && control2Progress <= 1;
    }
    bool operator==(const TailFadeSettings& b) const
    { return allowRise==b.allowRise && head==b.head && startFraction==b.startFraction && endFraction==b.endFraction
        && startGain==b.startGain && endGain==b.endGain && curvePower==b.curvePower
        && customCurve==b.customCurve && control1Time==b.control1Time && control2Time==b.control2Time
        && control1Progress==b.control1Progress && control2Progress==b.control2Progress
        && mixed==b.mixed && firstSegmentCurved==b.firstSegmentCurved && knots==b.knots && firstBezier==b.firstBezier; }
    bool operator!=(const TailFadeSettings& b) const { return !(*this==b); }
};
template<class From,class To> void copyEnvelopeShape(const From& a,To& b)
{
    b.startFraction=a.startFraction;b.endFraction=a.endFraction;b.startGain=a.startGain;b.endGain=a.endGain;
    b.curvePower=a.curvePower;b.customCurve=a.customCurve;b.control1Time=a.control1Time;b.control1Progress=a.control1Progress;
    b.control2Time=a.control2Time;b.control2Progress=a.control2Progress;
    b.mixed=a.mixed;b.firstSegmentCurved=a.firstSegmentCurved;b.knots=a.knots;
    b.firstBezier=a.firstBezier;
}

}
