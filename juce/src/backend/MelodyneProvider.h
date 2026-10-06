#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <optional>
#include <vector>

namespace hachi::backend
{
// This is deliberately a discovery-only boundary for now.  It does not load
// Melodyne binaries or inspect licence files.  A later provider can implement
// the supported host/API integration behind this value object.
struct MelodyneInstallation
{
    juce::String version;
    juce::File executable;
    juce::File vst3;
    juce::File coreBundle;

    [[nodiscard]] bool isUsableCandidate() const
    {
        return executable.existsAsFile() || vst3.exists();
    }
};

struct MelodyneVst3Probe
{
    bool candidateFound = false;
    bool hostPlatformSupported = false;
    bool pluginDescribed = false;
    juce::PluginDescription description;
    juce::String detail;
};

class MelodyneProvider final
{
public:
    [[nodiscard]] static std::optional<MelodyneInstallation> detect();
    // VST3 hosting is the first native integration route.  Traditional MPD
    // project import is deliberately not claimed here: it requires a separate
    // ARA/content-access capability check.
    [[nodiscard]] static MelodyneVst3Probe probeVst3();
    using Vst3InstanceCallback = std::function<void(
        std::unique_ptr<juce::AudioPluginInstance>, juce::String)>;
    static void createVst3InstanceAsync(double sampleRate, int blockSize,
                                        Vst3InstanceCallback callback);
    [[nodiscard]] static bool nativeImportAvailable();
    [[nodiscard]] static bool nativeRenderAvailable();
    [[nodiscard]] static bool experimentalSelfImportEnabled();
    [[nodiscard]] static bool experimentalMergedRenderEnabled();
    [[nodiscard]] static juce::String statusText();
};
}
