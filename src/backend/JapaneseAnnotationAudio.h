#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <vector>

namespace hachi::backend
{
using AnnotationCancel = std::function<bool()>;
struct AnnotationAudio
{
    juce::File file;
    juce::String fingerprint;
    double sampleRate = 0, duration = 0;
    juce::AudioBuffer<float> mono;
    static bool load(const juce::File&, AnnotationAudio&, juce::String& error,
                     const AnnotationCancel& cancel = {}, double maximumSeconds = 60);
    bool resample(double rate, std::vector<float>& output, juce::String& error,
                  const AnnotationCancel& cancel = {}) const;
};
}
