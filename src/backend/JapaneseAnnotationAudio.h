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
    // Decode only an exact original-source sample interval for note-level ASR.
    // It remains in memory and never requires a temporary WAV.
    static bool loadSlice(const juce::File&,juce::int64 firstSample,juce::int64 endSample,
                          AnnotationAudio&,juce::String& error,const AnnotationCancel& cancel={});
    bool resample(double rate, std::vector<float>& output, juce::String& error,
                  const AnnotationCancel& cancel = {}) const;
};
}
