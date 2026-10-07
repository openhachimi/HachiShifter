#pragma once
#include "AnalysisService.h"
#include "FcpeAnalyzer.h"
#include <juce_graphics/juce_graphics.h>
#include <atomic>
#include <memory>
#include <mutex>

namespace hachi::backend
{
struct OtoAudioAnalysisData
{
    juce::Image spectrogram;
    std::vector<FcpeFrame> pitch;
    double durationSeconds = 0.0;
    double minimumHz = 40.0, maximumHz = 12000.0;
    juce::String pitchBackend, warning, error;
    bool complete = false;
    double fractionForHz(double hz) const;
    double hzForFraction(double fraction) const;
};

struct OtoAudioAnalysisRequest
{
    std::mutex mutex;
    std::shared_ptr<const OtoAudioAnalysisData> data;
    std::atomic<bool> cancelled { false };
    std::shared_ptr<const OtoAudioAnalysisData> snapshot();
};

// Immutable recording-time data, shared between aliases of the same WAV.
// Workers never touch Components. Closing a window cancels its pending work.
class OtoAudioAnalysis final
{
public:
    static AnalysisConfig editorConfig();
    static std::shared_ptr<OtoAudioAnalysisRequest> request(
        const juce::File& audio, const AnalysisConfig& config);
};
}
