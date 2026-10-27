#pragma once
#include "../backend/WorldRenderer.h"
#include "../backend/NsfHifiganRenderer.h"
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <memory>

namespace hachi::tests
{
// Deterministic WORLD-only assembly coverage; no NN model, Main, or disk source.
inline bool runWorldSourceAssemblySmoke(const juce::File& folder)
{
    folder.createDirectory();
    juce::StringArray failures;
    const auto check = [&](bool ok, const juce::String& why) { if (!ok) failures.add(why); };
    constexpr double rate = 48000.0;
    constexpr int n = 24000;
    auto a = std::make_shared<juce::AudioBuffer<float>>(1, n);
    auto b = std::make_shared<juce::AudioBuffer<float>>(1, n);
    for (int i = 0; i < n; ++i) {
        a->setSample(0, i, 0.22f * std::sin(float(juce::MathConstants<double>::twoPi * 220.0 * i / rate)));
        b->setSample(0, i, 0.13f * std::sin(float(juce::MathConstants<double>::twoPi * 330.0 * i / rate)));
    }
    const auto hash = [](const juce::AudioBuffer<float>& x) {
        return juce::SHA256(x.getReadPointer(0), size_t(x.getNumSamples()) * sizeof(float)).toHexString();
    };
    const auto ah = hash(*a), bh = hash(*b);
    backend::NsfHifiganSourcePart first, second;
    first.source = a; first.sampleRate = rate; first.supportFrom = 0.10; first.supportTo = 0.50;
    first.timeMap = {{0.10, 0.00}, {0.25, 0.11}, {0.50, 0.50}};
    first.gainAt = [](double t) { return float(0.65 + 0.20 * t); };
    first.sourceMidiAt = [](double) { return 60.0f; };
    second.source = b; second.sampleRate = 44100.0; second.supportFrom = 0.70; second.supportTo = 1.05;
    second.timeMap = {{0.70, 0.00}, {0.82, 0.31}, {1.05, 0.50}};
    second.gainAt = [](double t) { return float(1.30 - 0.20 * (t - 0.70)); };
    second.sourceMidiAt = [](double) { return 67.0f; };
    std::vector<float> midi(240, 64.0f), formant(240, 0.0f);
    juce::AudioBuffer<float> noise;
    auto out = backend::WorldRenderer::renderAssembly({ first, second }, 50400, rate, 5.0, midi, formant, &noise);
    check(out.getNumSamples() == 50400 && out.getNumChannels() == 1, "WORLD assembly dimensions");
    check(noise.getNumSamples() == out.getNumSamples(), "noiseOutput is final WORLD noise, not NSF data");
    double gap = 0.0, voiced = 0.0;
    for (int i = 0; i < out.getNumSamples(); ++i) {
        const double t = i / rate, v = std::abs(out.getSample(0, i));
        if (t > 0.55 && t < 0.65) gap = std::max(gap, v);
        if ((t > 0.20 && t < 0.45) || (t > 0.78 && t < 1.00)) voiced = std::max(voiced, v);
    }
    check(gap < 1.0e-7, "unsupported group gap is true silence");
    check(voiced > 1.0e-5, "overlap/source contributions reach one synthesis");
    check(ah == hash(*a) && bh == hash(*b), "source PCM remains immutable");
    folder.getChildFile("world-source-assembly-result.txt").replaceWithText(failures.isEmpty() ? "PASS\n" : failures.joinIntoString("\n"));
    return failures.isEmpty();
}
}
