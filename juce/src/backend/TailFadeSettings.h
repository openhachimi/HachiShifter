#pragma once
#include <cmath>
namespace hachi::backend
{
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
    bool valid() const
    {
        return std::isfinite(startFraction) && std::isfinite(endFraction)
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
        && control1Progress==b.control1Progress && control2Progress==b.control2Progress; }
    bool operator!=(const HeadEnvelopeSettings& b) const { return !(*this==b); }
};
struct TailFadeSettings
{
    double startFraction = 0.0, endFraction = 1.0;
    double startGain = 1.0, endGain = 0.0;
    double curvePower = 1.0;
    bool customCurve = false;
    double control1Time = 1.0/3.0, control1Progress = 0.0;
    double control2Time = 2.0/3.0, control2Progress = 1.0;
    HeadEnvelopeSettings head;
    bool valid() const
    {
        return head.valid() && std::isfinite(startFraction) && std::isfinite(endFraction)
            && std::isfinite(startGain) && std::isfinite(endGain) && std::isfinite(curvePower)
            && startFraction >= 0 && endFraction <= 1 && endFraction-startFraction >= .001
            && startGain >= 0 && startGain <= 2 && endGain >= 0 && endGain <= startGain
            && curvePower >= .25 && curvePower <= 4
            && std::isfinite(control1Time) && std::isfinite(control2Time)
            && std::isfinite(control1Progress) && std::isfinite(control2Progress)
            && control1Time >= 0 && control1Time <= control2Time && control2Time <= 1
            && control1Progress >= 0 && control1Progress <= control2Progress && control2Progress <= 1;
    }
    bool operator==(const TailFadeSettings& b) const
    { return head==b.head && startFraction==b.startFraction && endFraction==b.endFraction
        && startGain==b.startGain && endGain==b.endGain && curvePower==b.curvePower
        && customCurve==b.customCurve && control1Time==b.control1Time && control2Time==b.control2Time
        && control1Progress==b.control1Progress && control2Progress==b.control2Progress; }
    bool operator!=(const TailFadeSettings& b) const { return !(*this==b); }
};
template<class From,class To> void copyEnvelopeShape(const From& a,To& b)
{
    b.startFraction=a.startFraction;b.endFraction=a.endFraction;b.startGain=a.startGain;b.endGain=a.endGain;
    b.curvePower=a.curvePower;b.customCurve=a.customCurve;b.control1Time=a.control1Time;b.control1Progress=a.control1Progress;
    b.control2Time=a.control2Time;b.control2Progress=a.control2Progress;
}

}
