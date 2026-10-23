#pragma once
#include "ProjectModel.h"

namespace hachi {
inline bool isDiffSingerParameter(const juce::String& key)
{ return key.startsWith("DS:ABS:") || key.startsWith("DS:REF:") || key.startsWith("DS:PTS:") || key.startsWith("DS:AUTO:"); }

template<class Function> inline void mapDiffSingerParameterTimes(NoteData& note, Function map)
{
    for(auto& curve:note.utauFlagCurves) if(isDiffSingerParameter(curve.flag))
        for(auto& point:curve.points) point.timeSeconds=map(point.timeSeconds);
}

inline std::vector<FlagCurvePoint> diffSingerParameterHandles(const NoteData& note, const juce::String& key)
{
    auto handles = flagCurvePointsFor(note, "DS:PTS:"+key.fromLastOccurrenceOf(":", false, false));
    if (!handles.empty()) return handles;
    const auto full = flagCurvePointsFor(note, key);
    if (full.size() < 3) return full;
    // Sparse handles only; the complete original frames remain the audible base.
    std::vector<size_t> chosen{0,full.size()-1};
    while (chosen.size()<24 && chosen.size()<full.size()) {
        double best = 0; size_t at = 0;
        for (size_t j=1;j<chosen.size();++j) {
            const auto a=chosen[j-1], b=chosen[j];
            const auto span=full[b].timeSeconds-full[a].timeSeconds;
            if (span<=0) continue;
            for (size_t i=a+1;i<b;++i) {
                const auto u=(full[i].timeSeconds-full[a].timeSeconds)/span;
                const auto error=std::abs(full[i].value-(full[a].value+(full[b].value-full[a].value)*u));
                if (error>best) {best=error;at=i;}
            }
        }
        if (best < (key.endsWith("TENC") ? .005 : .05)) break;
        chosen.push_back(at); std::sort(chosen.begin(),chosen.end());
    }
    for (auto i:chosen) handles.push_back(full[i]);
    return handles;
}

inline std::vector<FlagCurvePoint> spliceDiffSingerParameterHandles(
    const NoteData& note, const juce::String& key, std::vector<FlagCurvePoint> edits)
{
    const auto old=diffSingerParameterHandles(note,key), full=flagCurvePointsFor(note,key);
    if (edits.empty() || old.empty() || full.empty()) return edits;
    std::stable_sort(edits.begin(),edits.end(),[](auto& a,auto& b){return a.timeSeconds<b.timeSeconds;});
    auto equal=[](const auto& a,const auto& b) {return a.timeSeconds==b.timeSeconds && a.value==b.value
        && a.shape==b.shape && a.bezierX1==b.bezierX1 && a.bezierY1==b.bezierY1
        && a.bezierX2==b.bezierX2 && a.bezierY2==b.bezierY2;};
    size_t prefix=0,suffix=0;
    while(prefix<old.size() && prefix<edits.size() && equal(old[prefix],edits[prefix])) ++prefix;
    while(suffix<old.size()-prefix && suffix<edits.size()-prefix
        && equal(old[old.size()-1-suffix],edits[edits.size()-1-suffix])) ++suffix;
    if(prefix==old.size() && prefix==edits.size()) return full;
    const auto from=prefix ? old[prefix-1].timeSeconds : std::min(old.front().timeSeconds,edits.front().timeSeconds);
    const auto to=suffix ? old[old.size()-suffix].timeSeconds : std::max(old.back().timeSeconds,edits.back().timeSeconds);
    std::vector<FlagCurvePoint> result;
    for(const auto& p:full) if(p.timeSeconds<from) result.push_back(p);
    for(const auto& p:edits) if(p.timeSeconds>=from && p.timeSeconds<=to) result.push_back(p);
    for(const auto& p:full) if(p.timeSeconds>to) result.push_back(p);
    return result;
}
}
