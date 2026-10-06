#pragma once
#include "UtauRenderer.h"
#include "DiffSingerOptions.h"

namespace hachi::backend
{
class DiffSingerRenderer
{
public:
    static void openProjectCache(const juce::File& project = {});
    static bool saveProjectCache(const juce::File& project);
    static juce::String cacheSession();
    static void configure(const DiffSingerOptions&);
    static juce::var inferenceOptions(bool exporting = false);
    static juce::String inferenceStatus();
    static juce::var inferenceReport();
    static void shutdown();
    static bool isVoicebank(const juce::File& directory);
    static juce::var invoke(juce::var request, const std::function<bool()>& cancelled = {},
                            juce::AudioBuffer<float>* audio = nullptr, double* rate = nullptr);
    static juce::var requestJson(const UtauRenderRequest&, const juce::String& operation);
    static UtauRenderResult render(const UtauRenderRequest&);
};
}
