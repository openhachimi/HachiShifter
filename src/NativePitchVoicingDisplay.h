#pragma once
#include "NativeSourceTimeMap.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
#include <memory>

namespace hachi
{
using NativeNoiseRanges = std::vector<std::pair<double, double>>;

// Optional visual evidence, never project data or a render input. One worker
// shares each source analysis across its split notes, including older projects
// whose pitch detector assigned an F0 to frication, and pitched breathy vowels.
// Silence is excluded. No decoding during paint, no effect on renderer UV masks.
class NativePitchVoicingCache final : public juce::ChangeBroadcaster
{
public:
    NativePitchVoicingCache();
    ~NativePitchVoicingCache() override;
    void request(const std::vector<juce::File>& files);
    [[nodiscard]] std::shared_ptr<const NativeNoiseRanges> rangesFor(const juce::File&) const;
    [[nodiscard]] bool pending() const;
private:
    struct State;
    struct Job;
    std::shared_ptr<State> state;
    juce::ThreadPool pool { 1 };
};

// Convert the independent source mask with the exact clock used by native
// synthesis. Results are absolute timeline ranges, clipped to this note's audio.
[[nodiscard]] NativeNoiseRanges nativeNoiseDisplayRanges(
    const ClipData&, const NoteData&, const NativeNoiseRanges& sourceRanges);
[[nodiscard]] NativeNoiseRanges measuredUnvoicedDisplayRanges(const ClipData&, const NoteData&);
// Measured voiced intervals only. Authored target points never supply missing
// source F0; silence and UV inside an otherwise pitched note leave a gap.
[[nodiscard]] NativeNoiseRanges nativePitchDisplayRanges(const ClipData&, const NoteData&);
[[nodiscard]] NativeNoiseRanges nativeAuthoredPitchDisplayRanges(const ClipData&, const NoteData&);
bool reduceNativePitchClip(juce::Graphics&, const NativeNoiseRanges& voicedRanges,
                          float pixelsPerSecond, float timeZeroX);
// Authored geometry in a UV region is a guide, not measured pitch. Draw real
// circular dots, with a different rhythm from the breath display's short dashes.
void strokeNativeAuthoredPitchDots(juce::Graphics&, const juce::Path&,
    const NativeNoiseRanges& voicedRanges, const NativeNoiseRanges& authoredRanges,
    float pixelsPerSecond, float timeZeroX);
void mergeNativeNoiseRanges(NativeNoiseRanges&);

// Reuse the full path on both sides of the mask: dashing must never change a
// Bezier, its control points, or the phase of a shared pitch line.
void strokeNativePitchWithNoise(juce::Graphics&, const juce::Path&,
    const NativeNoiseRanges& timelineRanges, float pixelsPerSecond, float timeZeroX,
    bool dashedOnly = false);
}
