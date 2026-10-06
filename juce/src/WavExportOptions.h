#pragma once

namespace hachi
{
enum class WavExportComponent { full = 0, breath = 1, nonBreath = 2 };

struct WavExportOptions
{
    // Zero follows the playback device; the UI resolves it before pre-rendering.
    int sampleRate = 0;
    int channels = 2;
    // JUCE writes 16/24-bit PCM and 32-bit IEEE floating-point WAV.
    int bitDepth = 24;
    // Per-export choice; the next export always starts with the complete voice.
    WavExportComponent component = WavExportComponent::full;

    bool isValid() const
    {
        return (sampleRate == 0 || (sampleRate >= 8000 && sampleRate <= 192000))
            && (channels == 1 || channels == 2)
            && (bitDepth == 16 || bitDepth == 24 || bitDepth == 32)
            && (component == WavExportComponent::full || component == WavExportComponent::breath
                || component == WavExportComponent::nonBreath);
    }
};
}
