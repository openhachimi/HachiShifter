#include "PronunciationTiming.h"
#include "SampleSettings.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace hachi
{
bool validateNativeTransitionShape(const NativeTransitionShape& shape,double begin,double end,juce::String& error)
{
    if(shape.version!=1||!std::isfinite(begin)||!std::isfinite(end)||!std::isfinite(shape.boundarySourceSeconds)
        ||begin<0||end<=begin||shape.boundarySourceSeconds<=begin||shape.boundarySourceSeconds>=end)
    {error="Transition boundary must lie strictly inside its source coverage";return false;}
    return true;
}
juce::var nativeTransitionShapeValue(const NativeTransitionShape& shape)
{juce::Array<juce::var> values;values.add(shape.version);values.add(shape.boundarySourceSeconds);values.add(shape.provenance);return juce::var(values);}
bool nativeTransitionShapeFromValue(const juce::var& value,NativeTransitionShape& output,juce::String& error)
{
    const auto* values=value.getArray();if(!values||values->size()!=3||!(*values)[0].isInt()||int((*values)[0])!=1
        ||!((*values)[1].isDouble()||(*values)[1].isInt()||(*values)[1].isInt64())||!(*values)[2].isString()||!std::isfinite(double((*values)[1])))
    {error="Invalid versioned transition metadata";return false;}
    output={1,double((*values)[1]),(*values)[2].toString()};return true;
}
bool editNativeTransitionShape(SampleRegionSetting& row,double boundary,juce::String& error)
{
    NativeTransitionShape candidate{1,boundary,"manual-transition-boundary"};
    if(!validateNativeTransitionShape(candidate,row.regionStartSeconds,row.regionEndSeconds,error))return false;
    if(row.nativePronunciationEdges&&(boundary<row.nativePronunciationEdges->entryToSourceSeconds||boundary>row.nativePronunciationEdges->exitFromSourceSeconds))
    {error="Transition middle must lie between its left and right fade windows";return false;}
    row.nativeTransition=std::move(candidate);return true;
}
bool validateNativePronunciationEdges(const NativePronunciationEdges& p,double begin,double end,juce::String& error)
{
    const double values[]{p.entryFromSourceSeconds,p.entryToSourceSeconds,p.exitFromSourceSeconds,p.exitToSourceSeconds,p.terminationSourceSeconds};
    if(p.version!=1||!std::isfinite(begin)||!std::isfinite(end)||begin<0||end<begin)
    {error="Invalid native pronunciation edge profile/version";return false;}
    for(const auto value:values)if(!std::isfinite(value)||value<begin||value>end)
    {error="Native pronunciation edge lies outside its physical source coverage";return false;}
    if(p.entryToSourceSeconds<p.entryFromSourceSeconds||p.exitToSourceSeconds<p.exitFromSourceSeconds
        ||p.terminationSourceSeconds<p.exitToSourceSeconds)
    {error="Reversed native pronunciation edge window";return false;}
    if(std::abs(p.entryFromSourceSeconds-begin)>1.e-9||std::abs(p.exitToSourceSeconds-end)>1.e-9
        ||std::abs(p.terminationSourceSeconds-end)>1.e-9)
    {error="Version 1 native edges must anchor the full source coverage";return false;}
    return true;
}
juce::var nativePronunciationEdgesValue(const NativePronunciationEdges& p)
{
    juce::Array<juce::var> values;
    values.add(p.version);values.add(p.entryFromSourceSeconds);values.add(p.entryToSourceSeconds);
    values.add(p.exitFromSourceSeconds);values.add(p.exitToSourceSeconds);values.add(p.terminationSourceSeconds);values.add(p.provenance);
    return juce::var(values);
}
bool nativePronunciationEdgesFromValue(const juce::var& value,NativePronunciationEdges& out,juce::String& error)
{
    const auto* values=value.getArray();
    if(!values||values->size()!=7||!(*values)[0].isInt()||int((*values)[0])!=1||!(*values)[6].isString())
    {error="Invalid versioned native pronunciation edge metadata";return false;}
    NativePronunciationEdges p;double* numbers[]{&p.entryFromSourceSeconds,&p.entryToSourceSeconds,&p.exitFromSourceSeconds,&p.exitToSourceSeconds,&p.terminationSourceSeconds};
    for(int i=0;i<5;++i)
    {
        const auto& n=(*values)[i+1];
        if(!(n.isDouble()||n.isInt()||n.isInt64())){error="Native pronunciation edge must be numeric";return false;}
        *numbers[i]=double(n);
    }
    p.provenance=(*values)[6].toString();
    if(!validateNativePronunciationEdges(p,p.entryFromSourceSeconds,p.exitToSourceSeconds,error))return false;
    out=std::move(p);return true;
}

namespace
{
bool validEdgeEditCoverage(double begin,double end,juce::String& error)
{
    if(!std::isfinite(begin)||!std::isfinite(end)||begin<0||end<=begin)
    {error="Invalid source coverage for native edge edit";return false;}
    return true;
}
}
bool affineNativePronunciationEdges(const std::optional<NativePronunciationEdges>& old,
    double oldBegin,double oldEnd,double begin,double end,std::optional<NativePronunciationEdges>& output,juce::String& error)
{
    error.clear();
    if(!validEdgeEditCoverage(oldBegin,oldEnd,error)||!validEdgeEditCoverage(begin,end,error))return false;
    if(old&&!validateNativePronunciationEdges(*old,oldBegin,oldEnd,error))return false;
    if(!old){output.reset();return true;}
    auto candidate=*old;
    const auto at=[&](double source){return std::lerp(begin,end,(source-oldBegin)/(oldEnd-oldBegin));};
    candidate.entryFromSourceSeconds=begin;candidate.entryToSourceSeconds=at(old->entryToSourceSeconds);
    candidate.exitFromSourceSeconds=at(old->exitFromSourceSeconds);candidate.exitToSourceSeconds=end;candidate.terminationSourceSeconds=end;
    if(!validateNativePronunciationEdges(candidate,begin,end,error))return false;
    output=std::move(candidate);return true;
}
bool clipNativePronunciationEdges(const std::optional<NativePronunciationEdges>& old,
    double oldBegin,double oldEnd,double begin,double end,std::optional<NativePronunciationEdges>& output,juce::String& error)
{
    error.clear();
    if(!validEdgeEditCoverage(oldBegin,oldEnd,error)||!validEdgeEditCoverage(begin,end,error))return false;
    if(old&&!validateNativePronunciationEdges(*old,oldBegin,oldEnd,error))return false;
    if(begin<oldBegin||end>oldEnd){error="Native edge clip must stay inside its old physical coverage";return false;}
    if(!old){output.reset();return true;}
    auto candidate=*old;candidate.entryFromSourceSeconds=begin;candidate.exitToSourceSeconds=candidate.terminationSourceSeconds=end;
    candidate.entryToSourceSeconds=std::clamp(old->entryToSourceSeconds,begin,end);
    candidate.exitFromSourceSeconds=std::clamp(old->exitFromSourceSeconds,begin,end);
    if(!validateNativePronunciationEdges(candidate,begin,end,error))return false;
    output=std::move(candidate);return true;
}
bool mergeNativePronunciationEdges(const std::vector<SampleRegionSetting>& rows,
    std::optional<NativePronunciationEdges>& output,juce::String& error)
{
    error.clear();if(rows.empty()){error="No source rows to merge native edges";return false;}
    bool any=false;
    for(std::size_t i=0;i<rows.size();++i)
    {
        const auto& row=rows[i];
        if(!validEdgeEditCoverage(row.regionStartSeconds,row.regionEndSeconds,error))return false;
        if(i&&std::abs(rows[i-1].regionEndSeconds-row.regionStartSeconds)>1.e-7)
        {error="Native edge merge requires ordered adjacent source rows";return false;}
        if(row.nativePronunciationEdges)
        {if(!validateNativePronunciationEdges(*row.nativePronunciationEdges,row.regionStartSeconds,row.regionEndSeconds,error))return false;any=true;}
    }
    if(!any){output.reset();return true;}
    const auto& first=rows.front();const auto& last=rows.back();NativePronunciationEdges candidate;
    candidate.entryFromSourceSeconds=first.regionStartSeconds;
    candidate.entryToSourceSeconds=first.nativePronunciationEdges?first.nativePronunciationEdges->entryToSourceSeconds:first.regionStartSeconds;
    candidate.exitFromSourceSeconds=last.nativePronunciationEdges?last.nativePronunciationEdges->exitFromSourceSeconds:last.regionEndSeconds;
    candidate.exitToSourceSeconds=candidate.terminationSourceSeconds=last.regionEndSeconds;
    candidate.provenance=first.nativePronunciationEdges?first.nativePronunciationEdges->provenance
        :last.nativePronunciationEdges?last.nativePronunciationEdges->provenance:juce::String("merged-native-edges");
    if(!validateNativePronunciationEdges(candidate,first.regionStartSeconds,last.regionEndSeconds,error))return false;
    output=std::move(candidate);return true;
}
bool defaultNativePronunciationEdges(const SampleRegionSetting& row,NativePronunciationEdges& output,juce::String& error)
{
    if(row.pronunciationKind==PronunciationKind::transition&&row.nativeTransition)return defaultNativeTransitionEdges(row,output,error);
    error.clear();
    if(!validEdgeEditCoverage(row.regionStartSeconds,row.regionEndSeconds,error)
        ||!std::isfinite(row.overlapSeconds)||!std::isfinite(row.fixedDurationSeconds)||!std::isfinite(row.alignmentSeconds))
    {if(error.isEmpty())error="Invalid P/C/overlap for native edge defaults";return false;}
    NativePronunciationEdges candidate;candidate.entryFromSourceSeconds=row.regionStartSeconds;
    // Explicit creation projects legal legacy timing into NEW bounded audible
    // windows; it never clamps or repairs a stored profile or the raw P/C/O.
    const auto length=row.regionEndSeconds-row.regionStartSeconds;
    candidate.entryToSourceSeconds=std::min(row.regionEndSeconds,row.regionStartSeconds+std::clamp(row.overlapSeconds,0.0,length));
    candidate.exitFromSourceSeconds=std::min(row.regionEndSeconds,row.regionStartSeconds+std::clamp(row.fixedDurationSeconds,0.0,length));
    candidate.exitToSourceSeconds=candidate.terminationSourceSeconds=row.regionEndSeconds;
    candidate.provenance=row.provenance.isNotEmpty()?row.provenance:juce::String("native-cvvc-default-v1");
    if(!validateNativePronunciationEdges(candidate,row.regionStartSeconds,row.regionEndSeconds,error))return false;
    output=std::move(candidate);return true;
}
bool editNativePronunciationEdges(SampleRegionSetting& row,bool enabled,double entryTo,double exitFrom,juce::String& error)
{
    error.clear();if(!enabled){row.nativePronunciationEdges.reset();return true;}
    NativePronunciationEdges candidate;
    if(row.nativePronunciationEdges)candidate=*row.nativePronunciationEdges;
    else if(!defaultNativePronunciationEdges(row,candidate,error))return false;
    candidate.entryFromSourceSeconds=row.regionStartSeconds;candidate.entryToSourceSeconds=entryTo;
    candidate.exitFromSourceSeconds=exitFrom;candidate.exitToSourceSeconds=candidate.terminationSourceSeconds=row.regionEndSeconds;
    if(!validateNativePronunciationEdges(candidate,row.regionStartSeconds,row.regionEndSeconds,error))return false;
    if(row.pronunciationKind==PronunciationKind::transition&&row.nativeTransition
        &&(candidate.entryToSourceSeconds>row.nativeTransition->boundarySourceSeconds||candidate.exitFromSourceSeconds<row.nativeTransition->boundarySourceSeconds))
    {error="Transition fade windows must remain on their respective sides of the middle";return false;}
    row.nativePronunciationEdges=std::move(candidate);return true;
}

bool defaultNativeTransitionEdges(const SampleRegionSetting& row,NativePronunciationEdges& out,juce::String& error)
{
    if(!row.nativeTransition||!validateNativeTransitionShape(*row.nativeTransition,row.regionStartSeconds,row.regionEndSeconds,error))return false;
    const auto split=row.nativeTransition->boundarySourceSeconds;
    NativePronunciationEdges edges;edges.entryFromSourceSeconds=row.regionStartSeconds;
    edges.entryToSourceSeconds=row.regionStartSeconds+std::min(.005,(split-row.regionStartSeconds)*.2);
    edges.exitFromSourceSeconds=row.regionEndSeconds-std::min(.005,(row.regionEndSeconds-split)*.2);
    edges.exitToSourceSeconds=edges.terminationSourceSeconds=row.regionEndSeconds;edges.provenance="cvvc-transition-source-default-v1";
    if(!validateNativePronunciationEdges(edges,row.regionStartSeconds,row.regionEndSeconds,error))return false;
    out=std::move(edges);return true;
}

juce::String pronunciationKindName(PronunciationKind kind)
{
    switch (kind)
    {
        case PronunciationKind::transition: return "transition";
        case PronunciationKind::breath: return "breath";
        default: return "normal";
    }
}

PronunciationKind parsePronunciationKind(const juce::String& value)
{
    const auto name = value.trim().toLowerCase();
    if (name == "transition" || name == "t") return PronunciationKind::transition;
    if (name == "breath" || name == "b") return PronunciationKind::breath;
    return PronunciationKind::normal;
}

PronunciationTiming pronunciationTimingFor(const SampleRegionSetting& row)
{
    PronunciationTiming timing;
    timing.sourceStartSeconds = std::isfinite(row.regionStartSeconds) ? std::max(0.0, row.regionStartSeconds) : 0.0;
    timing.sourceEndSeconds = std::isfinite(row.regionEndSeconds) && row.regionEndSeconds > timing.sourceStartSeconds
        ? row.regionEndSeconds : timing.sourceStartSeconds + .001;
    timing.alignmentSeconds = juce::jlimit(timing.sourceStartSeconds, timing.sourceEndSeconds,
        std::isfinite(row.alignmentSeconds) ? row.alignmentSeconds : timing.sourceStartSeconds);
    timing.consonantEndSeconds = juce::jlimit(timing.sourceStartSeconds, timing.sourceEndSeconds,
        timing.sourceStartSeconds + (std::isfinite(row.fixedDurationSeconds) ? row.fixedDurationSeconds : 0.0));
    // A registered transition pronunciation has no lead-in. Its identity kind
    // governs P; internal C/V segment roles must not impersonate this property.
    if(row.pronunciationKind==PronunciationKind::transition)timing.alignmentSeconds=timing.consonantEndSeconds=timing.sourceStartSeconds;
    timing.overlapSeconds = std::isfinite(row.overlapSeconds) ? row.overlapSeconds : 0.0;
    timing.simplifiedConsonant = row.simplifiedConsonant;
    if (timing.simplifiedConsonant) timing.consonantEndSeconds = std::max(timing.alignmentSeconds, timing.consonantEndSeconds);
    return timing;
}

double protectedPronunciationSourceEnd(const PronunciationTiming& timing)
{
    return std::max(timing.alignmentSeconds, timing.consonantEndSeconds);
}

double consonantAfterAlignmentSeconds(const PronunciationTiming& timing)
{
    return std::max(0.0, timing.consonantEndSeconds - timing.alignmentSeconds);
}

namespace
{
constexpr double timingEpsilon = 1.e-9;

bool validSourceZones(const SampleRegionSetting& row, juce::String& error)
{
    if (!row.vowelSplitsExplicit) return true; // raw analysis epochs are not authoring zones
    std::vector<std::pair<double, double>> zones;
    for (const auto& segment : row.segments)
    {
        if (!std::isfinite(segment.sourceStartSeconds) || !std::isfinite(segment.sourceEndSeconds)
            || segment.sourceEndSeconds <= segment.sourceStartSeconds)
        { error = "Invalid native source zone"; return false; }
        zones.emplace_back(segment.sourceStartSeconds, segment.sourceEndSeconds);
    }
    std::sort(zones.begin(), zones.end());
    for (std::size_t i = 1; i < zones.size(); ++i)
        if (zones[i].first < zones[i - 1].second - timingEpsilon)
        { error = "Native source zones overlap"; return false; }
    return true;
}

std::vector<NativeSegment> canonicalAbsoluteSegments(const SampleRegionSetting& row, const PronunciationTiming& timing)
{
    const auto start = timing.sourceStartSeconds, end = timing.sourceEndSeconds;
    std::vector<double> cuts{start, end, timing.consonantEndSeconds, protectedPronunciationSourceEnd(timing)};
    const auto authoredZone = [&](const NativeSegment& segment)
    {
        if (!row.vowelSplitsExplicit || segment.role == NativeSegmentRole::consonant) return false;
        return segment.role != NativeSegmentRole::vowel
            || (segment.alias.trim().isNotEmpty() && segment.alias.trim() != "-");
    };
    for (const auto& segment : row.segments)
        if (authoredZone(segment) && std::isfinite(segment.sourceStartSeconds) && std::isfinite(segment.sourceEndSeconds)
            && segment.sourceEndSeconds > segment.sourceStartSeconds + timingEpsilon)
        {
            // Named vowel subdivisions do not split the canonical C prefix.
            for (const auto boundary : {segment.sourceStartSeconds, segment.sourceEndSeconds})
                if (segment.role != NativeSegmentRole::vowel || boundary > timing.consonantEndSeconds + timingEpsilon)
                    cuts.push_back(juce::jlimit(start, end, boundary));
        }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double a, double b) { return std::abs(a - b) <= timingEpsilon; }), cuts.end());
    struct Piece { NativeSegment segment; int owner = -1; };
    std::vector<Piece> pieces;
    for (std::size_t i = 1; i < cuts.size(); ++i)
    {
        if (cuts[i] - cuts[i - 1] <= timingEpsilon) continue;
        const auto middle = (cuts[i] + cuts[i - 1]) * .5;
        int owner = -1;
        if (row.vowelSplitsExplicit)
        {
            for (std::size_t j = 0; j < row.segments.size(); ++j)
                if (authoredZone(row.segments[j]) && row.segments[j].sourceStartSeconds <= middle && row.segments[j].sourceEndSeconds >= middle)
                { owner = static_cast<int>(j); break; }
        }
        // A default pronunciation is ONE C and ONE logical V, regardless of
        // how many pitch-analysis epochs happen to exist in its raw metadata.
        // Reusing one prior ID is harmless; preserving all prior cuts is not.
        if (owner < 0 && middle < timing.consonantEndSeconds)
        {
            for (std::size_t j = 0; j < row.segments.size(); ++j)
                if (row.segments[j].role == NativeSegmentRole::consonant
                    && row.segments[j].sourceStartSeconds < timing.consonantEndSeconds
                    && row.segments[j].sourceEndSeconds > start)
                { owner = static_cast<int>(j); break; }
        }
        else if (owner < 0 && !row.vowelSplitsExplicit)
        {
            for (std::size_t j = row.segments.size(); j > 0; --j)
                if (row.segments[j - 1].role == NativeSegmentRole::vowel
                    && row.segments[j - 1].sourceEndSeconds > timing.consonantEndSeconds
                    && row.segments[j - 1].sourceStartSeconds < end)
                { owner = static_cast<int>(j - 1); break; }
        }
        else if (owner < 0)
        {
            // A stale C tag outside the canonical prefix does not add a cut,
            // but an already-required interval may retain its old stable ID.
            for (std::size_t j = 0; j < row.segments.size(); ++j)
                if (row.segments[j].role == NativeSegmentRole::consonant
                    && row.segments[j].sourceStartSeconds <= middle && row.segments[j].sourceEndSeconds >= middle)
                { owner = static_cast<int>(j); break; }
        }
        NativeSegment segment;
        if (owner >= 0) segment = row.segments[static_cast<std::size_t>(owner)];
        else
        {
            segment.alias = row.name.trim().isEmpty() ? juce::String("-") : row.name.trim();
            segment.provenance = row.provenance.isEmpty() ? juce::String("estimated") : row.provenance;
            segment.confidence = row.confidence;
        }
        if (!row.vowelSplitsExplicit)
        {
            segment.alias = row.name.trim().isEmpty() ? juce::String("-") : row.name.trim();
            segment.stretchWeight = 1.0;
        }
        // Whole-pronunciation identity is not an internal zone role. Only an
        // explicit authoring snapshot may retain advanced non-C/V source zones.
        const bool otherZone = row.vowelSplitsExplicit && owner >= 0
            && segment.role != NativeSegmentRole::consonant && segment.role != NativeSegmentRole::vowel;
        if (!otherZone) segment.role = middle < timing.consonantEndSeconds ? NativeSegmentRole::consonant : NativeSegmentRole::vowel;
        segment.sourceStartSeconds = cuts[i - 1]; segment.sourceEndSeconds = cuts[i];
        segment.alignmentSeconds = timing.alignmentSeconds;
        segment.overlapSeconds = timing.overlapSeconds; // signed offset, not an absolute marker
        segment.stretchable = segment.role == NativeSegmentRole::vowel && middle >= protectedPronunciationSourceEnd(timing);
        if (!pieces.empty() && pieces.back().owner == owner && pieces.back().segment.role == segment.role
            && pieces.back().segment.stretchable == segment.stretchable
            && std::abs(pieces.back().segment.sourceEndSeconds - segment.sourceStartSeconds) <= timingEpsilon)
            pieces.back().segment.sourceEndSeconds = segment.sourceEndSeconds;
        else pieces.push_back({std::move(segment), owner});
    }
    // Keep each original ID on its best surviving semantic piece, preferring
    // the elastic remainder of a split vowel. Newly introduced cuts get IDs
    // using the existing segment_N convention without colliding with old IDs.
    std::set<juce::String> reserved, used;
    for (const auto& segment : row.segments) if (segment.id.isNotEmpty()) reserved.insert(segment.id);
    for (auto& piece : pieces) piece.segment.id.clear();
    for (std::size_t j = 0; j < row.segments.size(); ++j)
    {
        const auto& original = row.segments[j];
        if (original.id.isEmpty() || used.contains(original.id)) continue;
        int best = -1, score = -1;
        for (std::size_t i = 0; i < pieces.size(); ++i)
            if (pieces[i].owner == static_cast<int>(j))
            {
                const int candidate = (pieces[i].segment.role == original.role ? 2 : 0)
                    + (original.role == NativeSegmentRole::vowel && pieces[i].segment.stretchable ? 1 : 0);
                if (candidate > score) { score = candidate; best = static_cast<int>(i); }
            }
        if (best >= 0) { pieces[static_cast<std::size_t>(best)].segment.id = original.id; used.insert(original.id); }
    }
    int number = 1;
    std::vector<NativeSegment> result;
    for (auto& piece : pieces)
    {
        if (piece.segment.id.isEmpty())
        {
            juce::String id;
            do { id = "segment_" + juce::String(number++); } while (reserved.contains(id) || used.contains(id));
            piece.segment.id = id; used.insert(id);
        }
        result.push_back(std::move(piece.segment));
    }
    return result;
}
}

std::vector<NativeSegment> pronunciationSegmentsFor(const SampleRegionSetting& row)
{
    const auto timing = pronunciationTimingFor(row);
    if(row.pronunciationKind==PronunciationKind::transition&&row.nativeTransition)
    {
        juce::String error;if(!validateNativeTransitionShape(*row.nativeTransition,row.regionStartSeconds,row.regionEndSeconds,error))return {};
        std::vector<NativeSegment> result;
        const double cuts[]{0.0,row.nativeTransition->boundarySourceSeconds-row.regionStartSeconds,row.regionEndSeconds-row.regionStartSeconds};
        for(int i=0;i<2;++i){NativeSegment segment;segment.id=(row.materialRegionId.isEmpty()?juce::String("transition"):row.materialRegionId)+(i?"-right":"-left");segment.alias=row.name;segment.role=NativeSegmentRole::transition;segment.sourceStartSeconds=cuts[i];segment.sourceEndSeconds=cuts[i+1];segment.alignmentSeconds=0;segment.stretchable=true;segment.stretchWeight=1;result.push_back(segment);}return result;
    }
    auto segments = canonicalAbsoluteSegments(row, timing);
    for (auto& segment : segments)
    {
        segment.sourceStartSeconds -= timing.sourceStartSeconds;
        segment.sourceEndSeconds -= timing.sourceStartSeconds;
        segment.alignmentSeconds -= timing.sourceStartSeconds;
    }
    return segments;
}

bool applyPronunciationTiming(SampleRegionSetting& row, PronunciationTiming timing, juce::String& error)
{
    error.clear();
    if (!std::isfinite(timing.sourceStartSeconds) || !std::isfinite(timing.sourceEndSeconds)
        || !std::isfinite(timing.alignmentSeconds) || !std::isfinite(timing.consonantEndSeconds)
        || !std::isfinite(timing.overlapSeconds) || timing.sourceStartSeconds < 0
        || timing.sourceEndSeconds - timing.sourceStartSeconds <= timingEpsilon)
    { error = "Invalid pronunciation source timing"; return false; }
    if (!validSourceZones(row, error)) return false;
    timing.alignmentSeconds = row.pronunciationKind==PronunciationKind::transition?timing.sourceStartSeconds
        :juce::jlimit(timing.sourceStartSeconds, timing.sourceEndSeconds, timing.alignmentSeconds);
    timing.consonantEndSeconds = row.pronunciationKind==PronunciationKind::transition?timing.sourceStartSeconds
        :juce::jlimit(timing.sourceStartSeconds, timing.sourceEndSeconds, timing.consonantEndSeconds);
    if(row.pronunciationKind==PronunciationKind::transition)timing.simplifiedConsonant=false;
    if (timing.simplifiedConsonant) timing.consonantEndSeconds = std::max(timing.alignmentSeconds, timing.consonantEndSeconds);
    auto next = row;
    if(next.nativeTransition&&(row.regionStartSeconds!=timing.sourceStartSeconds||row.regionEndSeconds!=timing.sourceEndSeconds))
        next.nativeTransition->boundarySourceSeconds=std::lerp(timing.sourceStartSeconds,timing.sourceEndSeconds,(next.nativeTransition->boundarySourceSeconds-row.regionStartSeconds)/(row.regionEndSeconds-row.regionStartSeconds));
    next.regionStartSeconds = timing.sourceStartSeconds; next.regionEndSeconds = timing.sourceEndSeconds;
    next.alignmentSeconds = timing.alignmentSeconds;
    next.fixedDurationSeconds = timing.consonantEndSeconds - timing.sourceStartSeconds;
    next.overlapSeconds = timing.overlapSeconds;
    next.simplifiedConsonant = timing.simplifiedConsonant;
    if(next.pronunciationKind==PronunciationKind::transition&&next.nativeTransition)
    {
        if(!validateNativeTransitionShape(*next.nativeTransition,next.regionStartSeconds,next.regionEndSeconds,error))return false;
        next.segments=pronunciationSegmentsFor(next);for(auto& segment:next.segments){segment.sourceStartSeconds+=next.regionStartSeconds;segment.sourceEndSeconds+=next.regionStartSeconds;segment.alignmentSeconds+=next.regionStartSeconds;}
        next.role=NativeSegmentRole::transition;next.vowelSplitsExplicit=false;next.connectedNextRegionId.clear();
    }
    else next.segments = canonicalAbsoluteSegments(next, timing);
    row = std::move(next);
    return true;
}

bool editPronunciationAlignment(SampleRegionSetting& row, double absoluteSourceSeconds, juce::String& error)
{
    if (!std::isfinite(absoluteSourceSeconds)) { error = "Invalid alignment marker"; return false; }
    auto timing = pronunciationTimingFor(row);
    const auto delta = consonantAfterAlignmentSeconds(timing);
    timing.alignmentSeconds = juce::jlimit(timing.sourceStartSeconds, timing.sourceEndSeconds, absoluteSourceSeconds);
    if (timing.simplifiedConsonant) timing.consonantEndSeconds = std::min(timing.sourceEndSeconds, timing.alignmentSeconds + delta);
    return applyPronunciationTiming(row, timing, error);
}

bool editPronunciationConsonant(SampleRegionSetting& row, double absoluteSourceSeconds, juce::String& error)
{
    if (!std::isfinite(absoluteSourceSeconds)) { error = "Invalid consonant marker"; return false; }
    auto timing = pronunciationTimingFor(row);
    timing.consonantEndSeconds = juce::jlimit(timing.simplifiedConsonant ? timing.alignmentSeconds : timing.sourceStartSeconds,
                                             timing.sourceEndSeconds, absoluteSourceSeconds);
    return applyPronunciationTiming(row, timing, error);
}

bool setSimplifiedConsonant(SampleRegionSetting& row, bool enabled, double afterAlignmentSeconds, juce::String& error)
{
    auto timing = pronunciationTimingFor(row);
    // -1 captures the current derived gap; nonnegative values are an explicit
    // delta edit, including on first enable. No second numeric authority exists.
    if (!std::isfinite(afterAlignmentSeconds) || (afterAlignmentSeconds < 0 && afterAlignmentSeconds != -1.0))
    { error = "Consonant-after-alignment duration must be nonnegative or -1 to keep the current gap"; return false; }
    const auto delta = afterAlignmentSeconds == -1.0
        ? consonantAfterAlignmentSeconds(timing) : afterAlignmentSeconds;
    timing.simplifiedConsonant = enabled;
    if (enabled) timing.consonantEndSeconds = std::min(timing.sourceEndSeconds, timing.alignmentSeconds + delta);
    return applyPronunciationTiming(row, timing, error);
}

bool makePronunciationStretchMap(const SampleRegionSetting& row, double targetDurationSeconds,
                                 std::vector<SourceTimePoint>& result, juce::String& error)
{
    error.clear();
    if (!std::isfinite(targetDurationSeconds) || targetDurationSeconds <= 0)
    { error = "Target duration must be positive"; return false; }
    if (!std::isfinite(row.regionStartSeconds) || !std::isfinite(row.regionEndSeconds)
        || row.regionStartSeconds < 0 || row.regionEndSeconds <= row.regionStartSeconds + timingEpsilon
        || !std::isfinite(row.alignmentSeconds) || !std::isfinite(row.fixedDurationSeconds) || !std::isfinite(row.overlapSeconds))
    { error = "Invalid pronunciation source timing"; return false; }
    if (!validSourceZones(row, error)) return false;
    const auto timing = pronunciationTimingFor(row);
    const auto segments = pronunciationSegmentsFor(row);
    double fixed = 0, weightSum = 0;
    std::vector<double> lengths(segments.size()), weights(segments.size());
    std::vector<std::size_t> elastic;
    for (std::size_t i = 0; i < segments.size(); ++i)
    {
        const auto& segment = segments[i];
        lengths[i] = segment.sourceEndSeconds - segment.sourceStartSeconds;
        if ((segment.role == NativeSegmentRole::vowel || row.pronunciationKind==PronunciationKind::transition) && segment.stretchable)
        {
            if (!std::isfinite(segment.stretchWeight) || segment.stretchWeight <= 0)
            { error = "Elastic vowel weight must be positive"; return false; }
            weights[i] = lengths[i] * segment.stretchWeight;
            weightSum += weights[i]; elastic.push_back(i);
        }
        else fixed += lengths[i];
    }
    constexpr double vowelMinimum = 1.e-6;
    const auto sourceDuration = timing.sourceEndSeconds - timing.sourceStartSeconds;
    if (elastic.empty())
    {
        if (std::abs(targetDurationSeconds - sourceDuration) > 1.e-7)
        { error = "This pronunciation has no elastic vowel source"; return false; }
    }
    else
    {
        auto budget = targetDurationSeconds - fixed;
        if (budget < vowelMinimum * elastic.size() - 1.e-12 || weightSum <= 0 || !std::isfinite(weightSum))
        { error = "Target is shorter than the protected source and minimum vowels"; return false; }
        auto remaining = elastic;
        // Weighted source-duration allocation with positive vowel floors. Fixed
        // source zones retain their exact lengths, including early V before P.
        while (!remaining.empty())
        {
            bool floored = false;
            for (auto it = remaining.begin(); it != remaining.end();)
            {
                const auto i = *it;
                if (budget * weights[i] / weightSum < vowelMinimum)
                {
                    lengths[i] = vowelMinimum; budget -= vowelMinimum; weightSum -= weights[i];
                    it = remaining.erase(it); floored = true;
                }
                else ++it;
            }
            if (!floored)
            {
                for (const auto i : remaining) lengths[i] = budget * weights[i] / weightSum;
                break;
            }
        }
    }
    std::vector<SourceTimePoint> mapped{{0.0, timing.sourceStartSeconds}};
    double target = 0;
    for (std::size_t i = 0; i < segments.size(); ++i)
    {
        target += lengths[i];
        mapped.push_back({target, timing.sourceStartSeconds + segments[i].sourceEndSeconds});
    }
    if (!mapped.empty()) mapped.back().targetSeconds = targetDurationSeconds;
    result = std::move(mapped);
    return true;
}
}
