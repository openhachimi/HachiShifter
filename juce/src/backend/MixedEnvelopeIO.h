#pragma once
#include <juce_core/juce_core.h>
#include "TailFadeSettings.h"
namespace hachi::backend
{
inline juce::var envelopeBezierToVar(const EnvelopeBezier& b)
{return juce::var(juce::Array<juce::var>{b.control1Time,b.control1Progress,b.control2Time,b.control2Progress});}
inline bool envelopeBezierFromVar(const juce::var& value,EnvelopeBezier& b)
{
    b={};if(value.isVoid())return true; // Previous mixed S curves have exactly these default handles.
    if(!value.isArray()||value.size()!=4)return false;
    for(const auto& v:*value.getArray())if(!v.isDouble()&&!v.isInt()&&!v.isInt64())return false;
    b={(double)value[0],(double)value[1],(double)value[2],(double)value[3]};return b.valid();
}
template<class Settings> juce::var mixedEnvelopeToVar(const Settings& s)
{
    auto* o=new juce::DynamicObject();o->setProperty("enabled",s.mixed);
    o->setProperty("firstCurved",s.firstSegmentCurved);
    o->setProperty("firstBezier",envelopeBezierToVar(s.firstBezier));
    juce::Array<juce::var> knots;
    for(const auto& p:s.knots)
    {
        auto* k=new juce::DynamicObject();k->setProperty("time",p.time);
        k->setProperty("gain",p.gain);k->setProperty("curved",p.curvedToNext);k->setProperty("bezier",envelopeBezierToVar(p.bezier));knots.add(juce::var(k));
    }
    o->setProperty("knots",knots);return juce::var(o);
}
template<class Settings> bool mixedEnvelopeFromVar(const juce::var& value,Settings& s)
{
    if(value.isVoid())return true; // Old projects and preset libraries.
    if(!value.isObject())return false;
    const auto rows=value.getProperty("knots",{});
    if(!rows.isArray()||rows.size()>128)return false;
    std::vector<EnvelopeKnot> knots;
    for(const auto& row:*rows.getArray())
    {
        const auto t=row.getProperty("time",{}),g=row.getProperty("gain",{});
        const auto numeric=[](const juce::var& v){return v.isDouble()||v.isInt()||v.isInt64();};
        if(!numeric(t)||!numeric(g))return false;
        knots.push_back({(double)t,(double)g,(bool)row.getProperty("curved",false)});
        if(!envelopeBezierFromVar(row.getProperty("bezier",{}),knots.back().bezier))return false;
    }
    if(!validEnvelopeKnots(knots))return false;
    EnvelopeBezier first;if(!envelopeBezierFromVar(value.getProperty("firstBezier",{}),first))return false;
    s.mixed=(bool)value.getProperty("enabled",false);
    s.firstSegmentCurved=(bool)value.getProperty("firstCurved",false);s.knots=std::move(knots);s.firstBezier=first;return true;
}
template<class Settings> juce::String mixedEnvelopeText(const Settings& s)
{return juce::JSON::toString(mixedEnvelopeToVar(s),true,17);}
}
