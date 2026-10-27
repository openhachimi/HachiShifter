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
    bool virtualContributionsApplied=false; // per-source route weight, not track/pan/final mix
};
struct NativeRenderedWaveform
{
    std::uint64_t audioHash = 0;
    std::shared_ptr<const NativeRenderedPeaks> peaks;
};
// Raw peaks are measured from decoder output before ready. Optional derived
// route weights are measured off the audio thread from that SAME immutable PCM.
// Float peaks retain quiet audio; all channels contribute. No GUI file decoding.
template<class Weight>
inline std::shared_ptr<const NativeRenderedPeaks> measureWeightedNativeRenderedPeaks(
    const juce::AudioBuffer<float>& buffer,double sampleRate,Weight&& weight,bool virtualContributionsApplied=false)
{
    auto result = std::make_shared<NativeRenderedPeaks>();
    if (sampleRate <= 0.0 || buffer.getNumSamples() <= 0) return result;
    result->sampleRate = sampleRate; result->sampleCount = buffer.getNumSamples();result->virtualContributionsApplied=virtualContributionsApplied;
    const auto count = (buffer.getNumSamples()+NativeRenderedPeaks::samplesPerBucket-1)
        / NativeRenderedPeaks::samplesPerBucket;
    result->minima.assign(static_cast<std::size_t>(count),0.0f);
    result->maxima.assign(static_cast<std::size_t>(count),0.0f);
    for(int i=0;i<buffer.getNumSamples();++i)
    {
        const auto gain=float(weight(i));const auto bucket=static_cast<std::size_t>(i/NativeRenderedPeaks::samplesPerBucket);
        for(int channel=0;channel<buffer.getNumChannels();++channel)
        {
            const auto sample=buffer.getSample(channel,i)*gain;
            result->minima[bucket]=std::min(result->minima[bucket],sample);
            result->maxima[bucket]=std::max(result->maxima[bucket],sample);
        }
    }
    return result;
}
inline std::shared_ptr<const NativeRenderedPeaks> measureNativeRenderedPeaks(const juce::AudioBuffer<float>& buffer,double sampleRate)
{return measureWeightedNativeRenderedPeaks(buffer,sampleRate,[](int){return 1.f;});}
}
