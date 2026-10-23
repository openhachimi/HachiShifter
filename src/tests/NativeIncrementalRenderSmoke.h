#pragma once
#include "../backend/NsfHifiganRenderer.h"
#include "../backend/NsfRenderChunkCache.h"
#include "../backend/RenderService.h"
#include <atomic>
#include <iostream>
#include <thread>

namespace hachi
{
inline bool runNativeIncrementalRenderSmoke(const juce::File& folder, const juce::File& directory)
{
    using namespace backend;
    folder.createDirectory();
    bool ok = true;
    juce::Array<juce::var> checks, timings;
    const auto check = [&](const char* name, bool pass)
    {
        auto* row = new juce::DynamicObject(); row->setProperty("name", name); row->setProperty("ok", pass);
        checks.add(row); ok &= pass; std::cout << name << '=' << pass << std::endl;
    };
    const auto wait = [](auto predicate, int ms = 5000)
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + ms;
        while (!predicate() && juce::Time::getMillisecondCounterHiRes() < until) juce::Thread::sleep(2);
        return predicate();
    };
    {
        NsfRenderChunkCache cache(32, 2);
        const auto value = [](float n) { return std::make_shared<const std::vector<float>>(4, n); };
        cache.put("a", value(1)); cache.put("b", value(2));
        auto held = cache.get("a"); cache.put("c", value(3));
        check("bounded_cache_keeps_recent_and_evicts_old", held && cache.get("a") && !cache.get("b") && cache.get("c"));
        cache.put("oversized", std::make_shared<const std::vector<float>>(100, 0));
        check("oversized_not_retained", !cache.get("oversized") && cache.get("a"));
        cache.put("d", value(4)); cache.put("e", value(5));
        check("evicted_samples_remain_safe_for_reader", held && held->front() == 1 && !cache.get("a"));
    }
    check("real_model_available", NsfHifiganRenderer::modelAvailable(directory));
    constexpr double rate = 44100, seconds = 18, periodMs = 5;
    juce::AudioBuffer<float> source(1, static_cast<int>(rate * seconds));
    for (int i = 0; i < source.getNumSamples(); ++i)
    {
        const auto phase = juce::MathConstants<double>::twoPi * 220 * i / rate;
        source.setSample(0, i, float(.16 * (std::sin(phase) + .2 * std::sin(2 * phase))));
    }
    const OrtExecutionConfig execution{InferenceBackend::cpu, -1, 2};
    std::vector<float> pitch(static_cast<std::size_t>(seconds * 1000 / periodMs) + 1, 60);
    const auto render = [&](const char* name, const std::vector<float>& p,
                            const std::vector<float>& formants = std::vector<float>{})
    {
        const auto since = juce::Time::getMillisecondCounterHiRes();
        auto result = NsfHifiganRenderer::render(source, rate, source.getNumSamples(), periodMs,
            p, formants, {{0, 0}, {seconds, seconds}}, directory, execution,
            NsfHifiganStretchOrder::fixedHop, false, {}, {}, true, false);
        auto* row = new juce::DynamicObject(); row->setProperty("case", name);
        row->setProperty("milliseconds", juce::Time::getMillisecondCounterHiRes() - since);
        row->setProperty("inferred_chunks", result.inferredChunks); row->setProperty("reused_chunks", result.reusedChunks);
        row->setProperty("error", result.error); timings.add(row);
        std::cout << name << ": inferred=" << result.inferredChunks << ", reused=" << result.reusedChunks << std::endl;
        return result;
    };
    const auto sameRange = [&](const auto& a, const auto& b, double from, double to)
    {
        if (a.buffer.getNumSamples() != source.getNumSamples() || b.buffer.getNumSamples() != source.getNumSamples()) return false;
        for (int i = int(from * rate); i < int(to * rate); ++i)
            if (a.buffer.getSample(0, i) != b.buffer.getSample(0, i)) return false;
        return true;
    };
    if (ok)
    {
        NsfHifiganRenderer::shutdown();
        const auto cold = render("cold", pitch);
        check("cold_complete_audio", cold.usedModel && cold.inferredChunks >= 3 && cold.reusedChunks == 0
            && cold.buffer.getNumSamples() == source.getNumSamples() && cold.buffer.getMagnitude(0, source.getNumSamples()) > .01f);
        const auto warm = render("unchanged", pitch);
        check("unchanged_skips_all_neural_runs", warm.usedModel && warm.inferredChunks == 0 && warm.reusedChunks == cold.inferredChunks);
        check("unchanged_audio_exact", sameRange(cold, warm, 0, seconds));
        auto edited = pitch;
        for (std::size_t i = 1600; i < 1660; ++i) edited[i] = 65; // 8.0-8.3 s
        const auto local = render("local_pitch", edited);
        check("local_edit_reuses_unaffected_chunks", local.usedModel && local.inferredChunks > 0
            && local.inferredChunks < cold.inferredChunks && local.reusedChunks > 0);
        check("local_edit_preserves_distant_audio", sameRange(cold, local, .5, 4) && sameRange(cold, local, 13, 17));
        check("local_edit_changes_sound", !sameRange(cold, local, 8, 8.3));
        auto seam = pitch;
        for (std::size_t i = 1090; i < 1150; ++i) seam[i] = 64; // Across two overlapping cores.
        const auto boundary = render("boundary_pitch", seam);
        check("edit_across_seam_refreshes_both_contexts", boundary.usedModel && boundary.inferredChunks >= 2 && boundary.reusedChunks > 0);
        check("seam_has_no_silent_hole", boundary.usedModel && boundary.buffer.getMagnitude(0, int(5.5 * rate), int(.2 * rate)) > .01f);
        std::vector<float> formants(pitch.size(), 0);
        for (std::size_t i = 1600; i < 1660; ++i) formants[i] = 3;
        const auto timbre = render("local_formant", pitch, formants);
        check("mel_edit_invalidates_changed_chunk", timbre.usedModel && timbre.inferredChunks > 0 && timbre.reusedChunks > 0);
        std::atomic<bool> cancelled{false};
        std::thread stop([&] { juce::Thread::sleep(30); cancelled = true; });
        auto different = pitch; std::fill(different.begin(), different.end(), 72);
        const auto interrupted = NsfHifiganRenderer::render(source, rate, source.getNumSamples(), periodMs,
            different, {}, {{0, 0}, {seconds, seconds}}, directory, execution,
            NsfHifiganStretchOrder::fixedHop, false, {}, [&] { return cancelled.load(); }, true, false);
        stop.join();
        check("cancel_does_not_publish_partial_audio", !interrupted.usedModel && interrupted.buffer.getNumSamples() == 0);
        const auto recovery = render("after_cancel", pitch);
        check("cancel_preserves_valid_cached_audio", recovery.usedModel && recovery.inferredChunks == 0 && sameRange(cold, recovery, 0, seconds));
        NsfHifiganRenderer::shutdown();
        const auto reset = render("after_session_reset", pitch);
        check("session_reset_clears_decoded_cache", reset.usedModel && reset.reusedChunks == 0 && reset.inferredChunks == cold.inferredChunks);

        const auto sample = folder.getChildFile("source.wav");
        {
            auto stream = sample.createOutputStream();
            if (stream) { stream->setPosition(0); stream->truncate(); }
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer(stream
                ? format.createWriterFor(stream.release(), rate, 1, 16, {}, 0) : nullptr);
            check("queue_fixture_written", writer && writer->writeFromAudioSampleBuffer(source, 0, source.getNumSamples()));
        }
        RenderService service;
        Mld5FileRenderRequest old;
        old.sourceFile = sample; old.sourceDurationSeconds = seconds; old.targetDurationSeconds = seconds;
        old.framePeriodMs = periodMs; old.sourceMidi.assign(pitch.size(), 57); old.targetMidi = different;
        old.pitchBackend = PitchRenderBackend::nsfHifigan; old.hifiganModelDirectory = directory; old.inference = execution;
        std::atomic<int> obsoletePublished{0}, discarded{0}, currentPublished{0};
        service.renderMld5File(old, [&](auto) { ++obsoletePublished; }, {"obsolete", 0, seconds, [&] { ++discarded; }});
        check("real_obsolete_job_started", wait([&] { return service.hasActiveJobs(); }));
        service.beginUpdate();
        auto current = old; current.sourceDurationSeconds = current.targetDurationSeconds = .3;
        current.sourceMidi.assign(61, 57); current.targetMidi = current.sourceMidi; current.preserveUneditedSource = true;
        service.renderMld5File(current, [&](auto audio) { if (audio.buffer.getNumSamples() > 0) ++currentPublished; }, {"current", 0, .3, {}});
        const auto since = juce::Time::getMillisecondCounterHiRes();
        service.endUpdate({"current"});
        check("real_revision_update_is_nonblocking", juce::Time::getMillisecondCounterHiRes() - since < 100);
        check("real_replacement_published_promptly", wait([&] { return currentPublished == 1 && discarded == 1; }, 3000));
        service.cancelAll();
        check("real_obsolete_audio_not_published", obsoletePublished == 0);
    }
    auto* report = new juce::DynamicObject(); report->setProperty("ok", ok);
    report->setProperty("checks", checks); report->setProperty("timings", timings);
    return folder.getChildFile("native-incremental-validation.json").replaceWithText(juce::JSON::toString(juce::var(report), true)) && ok;
}
}
