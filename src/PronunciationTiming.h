#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <optional>

namespace hachi
{
struct SampleRegionSetting;
struct NativeSegment;
struct SourceTimePoint;

// Identity of a complete pronunciation, never the C/V role of an internal zone.
enum class PronunciationKind { normal, transition, breath };
[[nodiscard]] juce::String pronunciationKindName(PronunciationKind);
[[nodiscard]] PronunciationKind parsePronunciationKind(const juce::String&);

// A transition has one e/g-style interior boundary, not C/V/P metadata.
// Edge overlap/fade windows remain solely NativePronunciationEdges below.
struct NativeTransitionShape
{
    int version=1;
    double boundarySourceSeconds=0;
    juce::String provenance;
    bool operator==(const NativeTransitionShape&) const = default;
};
bool validateNativeTransitionShape(const NativeTransitionShape&,double sourceBegin,double sourceEnd,juce::String& error);
[[nodiscard]] juce::var nativeTransitionShapeValue(const NativeTransitionShape&);
bool nativeTransitionShapeFromValue(const juce::var&,NativeTransitionShape&,juce::String& error);
bool editNativeTransitionShape(SampleRegionSetting&,double boundarySourceSeconds,juce::String& error);

// Optional audible boundary windows, NOT a replacement for native syllable
// coverage or the existing P/C/overlap authority. Absence is legacy behaviour.
struct NativePronunciationEdges
{
    int version = 1;
    double entryFromSourceSeconds = 0, entryToSourceSeconds = 0;
    double exitFromSourceSeconds = 0, exitToSourceSeconds = 0;
    double terminationSourceSeconds = 0;
    juce::String provenance;
    bool operator==(const NativePronunciationEdges&) const = default;
};
bool validateNativePronunciationEdges(const NativePronunciationEdges&, double sourceBegin,
                                      double sourceEnd, juce::String& error);
// Strict versioned array; shared by OTTO and binary ValueTree serialization.
[[nodiscard]] juce::var nativePronunciationEdgesValue(const NativePronunciationEdges&);
bool nativePronunciationEdgesFromValue(const juce::var&, NativePronunciationEdges&, juce::String& error);
// Pure edit planners: validate the old profile first; output is unchanged on
// failure. No source PCM, project clocks or user musical curves participate.
bool affineNativePronunciationEdges(const std::optional<NativePronunciationEdges>&,
    double oldBegin,double oldEnd,double newBegin,double newEnd,
    std::optional<NativePronunciationEdges>& output,juce::String& error);
bool clipNativePronunciationEdges(const std::optional<NativePronunciationEdges>&,
    double oldBegin,double oldEnd,double newBegin,double newEnd,
    std::optional<NativePronunciationEdges>& output,juce::String& error);
bool mergeNativePronunciationEdges(const std::vector<SampleRegionSetting>& orderedRows,
    std::optional<NativePronunciationEdges>& output,juce::String& error);
bool defaultNativePronunciationEdges(const SampleRegionSetting&,NativePronunciationEdges&,juce::String& error);
bool defaultNativeTransitionEdges(const SampleRegionSetting&,NativePronunciationEdges&,juce::String& error);
bool editNativePronunciationEdges(SampleRegionSetting&,bool enabled,double entryToSourceSeconds,
                                  double exitFromSourceSeconds,juce::String& error);

// Start/end/alignment/C-end positions are ABSOLUTE seconds in the unchanged
// recording. overlapSeconds alone remains the signed OTO offset from start;
// its source marker is start + overlapSeconds (negative/long values are valid).
// SampleRegionSetting keeps the classic consonant duration as its wire format;
// this is the one shared interpretation used by native and OTO editor views.
struct PronunciationTiming
{
    double sourceStartSeconds = 0.0;
    double sourceEndSeconds = 0.0;
    double alignmentSeconds = 0.0;
    double consonantEndSeconds = 0.0;
    double overlapSeconds = 0.0;
    bool simplifiedConsonant = false;
};

[[nodiscard]] PronunciationTiming pronunciationTimingFor(const SampleRegionSetting&);
[[nodiscard]] double protectedPronunciationSourceEnd(const PronunciationTiming&);
[[nodiscard]] double consonantAfterAlignmentSeconds(const PronunciationTiming&);
bool applyPronunciationTiming(SampleRegionSetting&, PronunciationTiming, juce::String& error);
bool editPronunciationAlignment(SampleRegionSetting&, double absoluteSourceSeconds, juce::String& error);
bool editPronunciationConsonant(SampleRegionSetting&, double absoluteSourceSeconds, juce::String& error);
// The relative position is encoded by the existing C/P values, not a second
// redundant persisted numeric authority. Pass -1.0 when toggling to retain
// the current nonnegative gap; a value >= 0 explicitly edits that gap.
bool setSimplifiedConsonant(SampleRegionSetting&, bool enabled,
                            double afterAlignmentSeconds, juce::String& error);

// Local-to-pronunciation segments. C and everything before P are fixed;
// existing internal vowel subdivisions/weights are retained beyond that zone.
[[nodiscard]] std::vector<NativeSegment> pronunciationSegmentsFor(const SampleRegionSetting&);
// Target-local / source-absolute map. Refuses impossible contractions rather
// than stretching protected consonants or silently throwing source audio away.
bool makePronunciationStretchMap(const SampleRegionSetting&, double targetDurationSeconds,
                                 std::vector<SourceTimePoint>& result, juce::String& error);
}
