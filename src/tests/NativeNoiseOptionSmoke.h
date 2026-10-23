#pragma once
#include "../backend/NsfHifiganRenderer.h"
#include "../backend/RenderService.h"
#include <atomic>
#include <iostream>

namespace hachi
{
inline bool runNativeNoiseOptionSmoke(const juce::File& folder, const juce::File& directory)
{
    using namespace backend;
    folder.createDirectory(); bool ok = true; juce::Array<juce::var> checks;
    const auto check = [&](const char* name, bool pass)
    {
        auto* row = new juce::DynamicObject(); row->setProperty("name", name); row->setProperty("passed", pass);
        checks.add(row); ok &= pass; std::cout << name << '=' << pass << std::endl;
    };
    constexpr int rate = 44100; constexpr double seconds = 2;
    juce::AudioBuffer<float> source(1, rate * 2); juce::Random random(138); float previous = 0;
    for (int i = 0; i < source.getNumSamples(); ++i)
    {
        const auto t = double(i) / rate; const auto noise = random.nextFloat() * 2 - 1;
        source.setSample(0, i, t < .25 || (t >= 1.35 && t < 1.6) ? (noise - previous) * .1f
            : float(.18 * std::sin(juce::MathConstants<double>::twoPi * 220 * t) + .025 * noise));
        previous = noise;
    }
    const OrtExecutionConfig execution{InferenceBackend::cpu, -1, 2};
    const auto render = [&](bool protect, double targetSeconds = 2.0,
                            const std::vector<NsfHifiganTimeMapPoint>& map = std::vector<NsfHifiganTimeMapPoint>{{0,0},{2,2}})
    {
        return NsfHifiganRenderer::render(source, rate, int(targetSeconds * rate), 5,
            std::vector<float>(std::size_t(targetSeconds * 200) + 1, 69), {}, map, directory, execution,
            NsfHifiganStretchOrder::hifiShifterMel, false, {}, {}, protect, false, true);
    };
    const auto valid = [](const auto& r, int samples)
    {
        if (!r.usedModel || r.buffer.getNumSamples() != samples) return false;
        for (int i = 0; i < samples; ++i) if (!std::isfinite(r.buffer.getSample(0, i))) return false;
        return r.buffer.getMagnitude(0, samples) > .01f;
    };
    auto on = render(true), off = render(false), offWarm = render(false);
    check("real_model_renders_both_states", valid(on, rate * 2) && valid(off, rate * 2));
    check("on_separates_harmonics_and_protects_clear_unvoiced", on.usedHarmonicNoise && on.protectedUnvoicedFrames > 20);
    check("off_skips_separation_and_pcm_restoration", !off.usedHarmonicNoise && off.protectedUnvoicedFrames == 0);
    double dryError = 0, offDifference = 0, repeatError = 0;
    if (on.usedModel && off.usedModel && offWarm.usedModel)
        for (int i = 0; i < source.getNumSamples(); ++i)
        {
            repeatError = std::max(repeatError, double(std::abs(off.buffer.getSample(0, i) - offWarm.buffer.getSample(0, i))));
            if (i > rate * .06 && i < rate * .18)
            {
                dryError = std::max(dryError, double(std::abs(on.buffer.getSample(0, i) - source.getSample(0, i))));
                offDifference += std::abs(off.buffer.getSample(0, i) - source.getSample(0, i));
            }
        }
    check("on_restores_original_clear_unvoiced_wave", on.usedModel && dryError < 2.e-5);
    check("off_reaches_audio_not_only_label", off.usedModel && offDifference > .01);
    check("off_retains_incremental_render_cache", offWarm.usedModel && offWarm.inferredChunks == 0 && offWarm.reusedChunks > 0 && repeatError == 0);
    auto stretched = render(true, 2.8, {{0,0},{.5,.25},{1.9,1.65},{2.8,2}});
    check("on_handles_nonlinear_stretch", valid(stretched, int(2.8 * rate)) && stretched.usedHarmonicNoise);
    double mappedError = 0;
    if (stretched.usedModel) for (int i = int(rate * .14); i < int(rate * .35); ++i)
    {
        const auto position = i * .5; const auto index = int(position);
        const auto expected = source.getSample(0, index) + float(position - index)
            * (source.getSample(0, index + 1) - source.getSample(0, index));
        mappedError = std::max(mappedError, double(std::abs(stretched.buffer.getSample(0, i) - expected)));
    }
    check("source_noise_and_clear_pcm_follow_time_map", stretched.usedModel && mappedError < 2.e-5);
    const auto again = render(true);
    check("reenabling_restores_protected_path", again.usedModel && again.usedHarmonicNoise && again.reusedChunks > 0);
    const auto file = folder.getChildFile("fricative-vowel.wav");
    {
        auto stream = file.createOutputStream(); if (stream) { stream->setPosition(0); stream->truncate(); }
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(stream ? format.createWriterFor(stream.release(), rate, 1, 24, {}, 0) : nullptr);
        check("source_fixture_written", writer && writer->writeFromAudioSampleBuffer(source, 0, source.getNumSamples()));
    }
    RenderService service; Mld5FileRenderRequest request;
    request.sourceFile = file; request.sourceDurationSeconds = request.targetDurationSeconds = seconds;
    request.sourceMidi.assign(401, 57); request.targetMidi.assign(401, 69);
    request.pitchBackend = PitchRenderBackend::nsfHifigan; request.hifiganModelDirectory = directory;
    request.inference = execution; request.stretchAlgorithm = 5; request.nsfSmoothPitchTransitions = false;
    const auto runService = [&](bool protect, bool original)
    {
        auto req = request; req.nsfNoiseProtection = protect; req.preserveUneditedSource = original;
        if (original) req.targetMidi = req.sourceMidi;
        std::atomic<bool> done{false}; RenderedAudio result;
        service.renderMld5File(req, [&](auto audio) { result = std::move(audio); done.store(true, std::memory_order_release); });
        const auto until = juce::Time::getMillisecondCounterHiRes() + 30000;
        while (!done.load(std::memory_order_acquire) && juce::Time::getMillisecondCounterHiRes() < until) juce::Thread::sleep(2);
        service.cancelAll();
        return result;
    };
    const auto routedOn = runService(true, false), routedOff = runService(false, false);
    check("service_on_uses_harmonic_noise_path", routedOn.buffer.getNumSamples() == rate * 2 && routedOn.backend.contains("+hnsep"));
    check("service_off_uses_full_audio_path", routedOff.buffer.getNumSamples() == rate * 2 && routedOff.backend.contains("nsf")
        && !routedOff.backend.contains("+hnsep") && !routedOff.backend.contains("+source-uv"));
    const auto originalOn = runService(true, true), originalOff = runService(false, true);
    bool originalsEqual = originalOn.buffer.getNumSamples() == rate * 2 && originalOff.buffer.getNumSamples() == rate * 2;
    if (originalsEqual) for (int i = 0; i < rate * 2; ++i)
        originalsEqual &= originalOn.buffer.getSample(0, i) == originalOff.buffer.getSample(0, i);
    check("unedited_original_pcm_bypasses_both_switch_states", originalsEqual
        && originalOn.backend.contains("native-source-preserved") && originalOff.backend.contains("native-source-preserved"));
    auto* report = new juce::DynamicObject(); report->setProperty("passed", ok); report->setProperty("checks", checks);
    report->setProperty("protected_pcm_max_error", dryError); report->setProperty("mapped_pcm_max_error", mappedError);
    report->setProperty("disabled_original_difference", offDifference);
    return folder.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(report), true)) && ok;
}
}
