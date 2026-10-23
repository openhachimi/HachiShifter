#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <memory>
#include <vector>
#include <cstdint>

namespace hachi
{
struct NativeRenderedPeaks
{
    static constexpr int samplesPerBucket = 32;
    double sampleRate = 0.0;
    int sampleCount = 0;
    std::vector<float> minima, maxima;
};
struct NativeRenderedWaveform
{
    std::uint64_t audioHash = 0;
    std::shared_ptr<const NativeRenderedPeaks> peaks;
};
// Measured once from actual decoder output, before publishing the ready flag.
// Float peaks keep quiet audio; both channels contribute. No GUI file decoding.
inline std::shared_ptr<const NativeRenderedPeaks> measureNativeRenderedPeaks(
    const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    auto result = std::make_shared<NativeRenderedPeaks>();
    if (sampleRate <= 0.0 || buffer.getNumSamples() <= 0) return result;
    result->sampleRate = sampleRate; result->sampleCount = buffer.getNumSamples();
    const auto count = (buffer.getNumSamples()+NativeRenderedPeaks::samplesPerBucket-1)
        / NativeRenderedPeaks::samplesPerBucket;
    result->minima.assign(static_cast<std::size_t>(count),0.0f);
    result->maxima.assign(static_cast<std::size_t>(count),0.0f);
    for (int channel=0;channel<buffer.getNumChannels();++channel)
    {
        const auto* samples=buffer.getReadPointer(channel);
        for(int i=0;i<buffer.getNumSamples();++i)
        {
            const auto bucket=static_cast<std::size_t>(i/NativeRenderedPeaks::samplesPerBucket);
            result->minima[bucket]=std::min(result->minima[bucket],samples[i]);
            result->maxima[bucket]=std::max(result->maxima[bucket],samples[i]);
        }
    }
    return result;
}
}
