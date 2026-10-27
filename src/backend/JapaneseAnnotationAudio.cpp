#include "JapaneseAnnotationAudio.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>

namespace hachi::backend
{
bool AnnotationAudio::load(const juce::File& file, AnnotationAudio& out, juce::String& error,
                           const AnnotationCancel& cancel, double maximumSeconds)
{
    error.clear();
    if (cancel && cancel()) { error = "Annotation cancelled"; return false; }
    if (!file.existsAsFile()) { error = "Recording is missing"; return false; }
    const auto before = juce::SHA256(file).toHexString();
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || !std::isfinite(reader->sampleRate) || reader->sampleRate < 8000
        || reader->sampleRate > 384000 || !reader->numChannels || reader->numChannels > 32
        || reader->lengthInSamples < 1 || reader->lengthInSamples > std::numeric_limits<int>::max())
    { error = "Unsupported or invalid recording"; return false; }
    const auto duration = double(reader->lengthInSamples) / reader->sampleRate;
    if (duration < .05 || duration > maximumSeconds)
    { error = "Japanese annotation accepts recordings from 0.05 to " + juce::String(maximumSeconds, 0) + " seconds; split longer recordings explicitly"; return false; }
    AnnotationAudio candidate; candidate.file = file; candidate.fingerprint = before;
    candidate.sampleRate = reader->sampleRate; candidate.duration = duration;
    candidate.mono.setSize(1, int(reader->lengthInSamples)); candidate.mono.clear();
    juce::AudioBuffer<float> block(int(reader->numChannels), 8192);
    for (int offset = 0; offset < candidate.mono.getNumSamples(); offset += block.getNumSamples())
    {
        if (cancel && cancel()) { error = "Annotation cancelled"; return false; }
        const auto count = juce::jmin(block.getNumSamples(), candidate.mono.getNumSamples() - offset);
        if (!reader->read(&block, 0, count, offset, true, true))
        { error = "Could not decode recording"; return false; }
        for (int i = 0; i < count; ++i)
        {
            double sample = 0;
            for (int channel = 0; channel < block.getNumChannels(); ++channel)
            {
                const auto value = block.getSample(channel, i);
                if (!std::isfinite(value)) { error = "Recording contains non-finite samples"; return false; }
                sample += value;
            }
            candidate.mono.setSample(0, offset + i, float(sample / block.getNumChannels()));
        }
    }
    if (juce::SHA256(file).toHexString() != before)
    { error = "Recording changed while being read"; return false; }
    out = std::move(candidate); return true;
}

bool AnnotationAudio::resample(double rate, std::vector<float>& output, juce::String& error,
                               const AnnotationCancel& cancel) const
{
    if (mono.getNumSamples() < 1 || sampleRate <= 0 || rate < 8000 || rate > 192000)
    { error = "Invalid annotation sample rate"; return false; }
    const auto length = int(std::llround(mono.getNumSamples() * rate / sampleRate));
    output.resize(std::size_t(length));
    if (rate == sampleRate)
    { std::copy_n(mono.getReadPointer(0), length, output.data()); return !(cancel && cancel()); }
    // JUCE's resampling source includes anti-alias filtering. Both ASR and TIFA
    // consume this immutable decoded recording; the source file is never written.
    juce::AudioBuffer<float> memory(mono);
    juce::MemoryAudioSource source(memory, false);
    juce::ResamplingAudioSource converter(&source, false, 1);
    converter.setResamplingRatio(sampleRate / rate); converter.prepareToPlay(4096, rate);
    juce::AudioBuffer<float> block(1, 4096);
    for (int offset = 0; offset < length; offset += block.getNumSamples())
    {
        if (cancel && cancel()) { error = "Annotation cancelled"; output.clear(); return false; }
        const auto count = juce::jmin(block.getNumSamples(), length - offset);
        converter.getNextAudioBlock({&block, 0, count});
        std::copy_n(block.getReadPointer(0), count, output.data() + offset);
    }
    converter.releaseResources(); return true;
}
}
