#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <functional>

namespace hachi::backend
{
// Project-level source/target time anchor.  Defined here (the lowest-level
// renderer header) so every renderer that needs a custom time warp shares the
// same value type instead of declaring its own private alias.

struct TimeMapPoint
{
    double targetSeconds = 0.0;
    double sourceSeconds = 0.0;
};

// Independent renderer request for the Melodyne-5 MULSS component engine.
// The fields here are the ones the M5 path consumes on its own: pitch-formant
// curves, formant offset, per-frame amplitude, time warp and target length.
// Tension and breath remain RenderService-level expression controls applied
// after the algorithm output (kept identical across all backends).

struct Mld5RenderRequest
{
    const juce::AudioBuffer<float>* input = nullptr;
    double sampleRate = 48'000.0;
    double framePeriodMs = 5.0;
    std::vector<float> sourceMidi;
    std::vector<float> targetMidi;
    std::vector<float> formantSemitones;
    std::vector<float> noteGain;
    std::vector<float> sourceMidiHint,targetMidiIntent;
    std::function<void(int)> sourceRecoveryObserver;
    std::vector<TimeMapPoint> timeMap;        // empty -> no time warp
    int targetSamples = 0;                     // 0 -> output length = input length
    // Scheduler facts only. Reader/file EOF does not establish an empty queue.
    bool terminalComponentKnown = false;
    bool pendingQueueEmpty = false;
    bool allowZeroRemainder = false;
};

class Mld5Renderer final
{
public:
    // Recovered period/rolling/FFT/OLA DSP supplied in G:/mld5. The Hachi host
    // adapts analysed F0, target curves and time maps; it is not a reconstruction
    // of Melodyne's source separator or all original project/control modes.
    static constexpr const char* implementationId = "mld5-recovered-period-v5-voicing-join";
    struct ControlSnapshot { double pitchRatio=1; float probeRatio=1,voicingGain=0; };
    [[nodiscard]] static ControlSnapshot diagnosticControlAt(const Mld5RenderRequest&,double seconds);
    [[nodiscard]] juce::AudioBuffer<float> render(const Mld5RenderRequest& request,
                                                 juce::String* error=nullptr) const;
};
}
