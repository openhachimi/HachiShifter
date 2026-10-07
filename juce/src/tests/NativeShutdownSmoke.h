#pragma once
#include "../backend/NsfHifiganRenderer.h"
#include "../backend/RenderService.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <thread>

namespace hachi
{
// Exercise the real CPU vocoder and its editor queue.  A cancel watcher on a
// different thread confirms that the direct test reaches the ONNX Run guard;
// the queue tests wait for active work rather than cancelling pending jobs.
inline bool runNativeShutdownSmoke(const juce::File& folder,
                                   const juce::File& modelDirectory)
{
    using namespace backend;
    folder.createDirectory();
    bool ok = true;
    juce::Array<juce::var> checks;
    auto* report = new juce::DynamicObject();
    const auto check = [&](const char* name, bool pass)
    {
        auto* row = new juce::DynamicObject();
        row->setProperty("name", name); row->setProperty("ok", pass);
        checks.add(row); ok &= pass;
        std::cout << name << '=' << pass << std::endl;
    };
    const auto wait = [](auto predicate, double timeoutMs = 5000.0)
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + timeoutMs;
        while (!predicate() && juce::Time::getMillisecondCounterHiRes() < until)
            juce::Thread::sleep(2);
        return predicate();
    };
    const auto elapsed = [](double since)
    { return juce::Time::getMillisecondCounterHiRes() - since; };
    const auto finish = [&]
    {
        report->setProperty("ok", ok); report->setProperty("checks", checks);
        report->setProperty("model_directory", modelDirectory.getFullPathName());
        const auto written = folder.getChildFile("native-shutdown-validation.json")
            .replaceWithText(juce::JSON::toString(juce::var(report), true));
        std::cout << "native_shutdown_ok=" << ok << "; report_written=" << written << std::endl;
        return ok && written;
    };
    check("cpu_model_available", NsfHifiganRenderer::modelAvailable(modelDirectory));
    if (!ok) return finish();
    constexpr double rate = 44100.0;
    constexpr double sourceSeconds = 0.75;
    constexpr double shortSeconds = 0.3;
    constexpr double longSeconds = 12.0;
    const OrtExecutionConfig execution { InferenceBackend::cpu, -1, 2 };
    juce::AudioBuffer<float> source(1, static_cast<int>(std::lround(rate * sourceSeconds)));
    for (int i = 0; i < source.getNumSamples(); ++i)
    {
        const auto phase = juce::MathConstants<double>::twoPi * 220 * i / rate;
        source.setSample(0, i, static_cast<float>(0.18 * (std::sin(phase)
            + 0.2 * std::sin(2 * phase) + 0.1 * std::sin(3 * phase))));
    }
    const auto bank = folder.getChildFile("bank"); bank.createDirectory();
    const auto sample = bank.getChildFile("a.wav");
    {
        auto stream = sample.createOutputStream();
        if (stream) { stream->setPosition(0); stream->truncate(); }
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(stream
            ? format.createWriterFor(stream.release(), rate, 1, 16, {}, 0) : nullptr);
        check("fixture_written", writer && writer->writeFromAudioSampleBuffer(
            source, 0, source.getNumSamples()));
    }
    check("oto_written", bank.getChildFile("oto.ini")
        .replaceWithText("a.wav=a,0,30,-700,30,10\r\n"));
    if (!ok) return finish();
    const auto pitch = [](double seconds)
    { return std::vector<float>(static_cast<std::size_t>(std::ceil(seconds / .005)) + 1, 57.0f); };
    const auto direct = [&](double seconds, const std::function<bool()>& cancelled = {})
    {
        return NsfHifiganRenderer::render(source, rate, static_cast<int>(std::lround(seconds * rate)),
            5.0, pitch(seconds), {}, {{0, 0}, {seconds, sourceSeconds}}, modelDirectory,
            execution, NsfHifiganStretchOrder::fixedHop, false, {}, cancelled);
    };
    const auto valid = [](const auto& audio)
    {
        return audio.getNumChannels() > 0 && audio.getNumSamples() > 0
            && audio.getMagnitude(0, audio.getNumSamples()) > 1.0e-6f;
    };
    const auto warm = direct(shortSeconds);
    check("real_cpu_model_warmed", warm.usedModel && valid(warm.buffer)
        && warm.buffer.getNumSamples() == static_cast<int>(std::lround(shortSeconds * rate)));
    report->setProperty("warm_error", warm.error);
    if (!ok) return finish();
    {
        int polls = 0;
        const auto preCancelled = direct(longSeconds, [&] { ++polls; return true; });
        check("pre_cancelled_renderer_does_not_return_audio",
              polls > 0 && !preCancelled.usedModel && preCancelled.buffer.getNumSamples() == 0);
    }
    {
        NsfUtauSampleTiming timing;
        timing.endSeconds = .7; timing.fileSeconds = sourceSeconds;
        timing.consonantSeconds = .03; timing.preutteranceSeconds = .03; timing.overlapSeconds = .01;
        const auto plan = buildNsfUtauNotePlan(timing, .05, shortSeconds, 1, 0);
        int polls = 0;
        const auto preCancelled = synthesizeNsfUtauNote(sample, plan, 57, {},
            modelDirectory, execution, {}, nullptr, [&] { ++polls; return true; });
        check("pre_cancelled_voicebank_note_does_not_return_audio",
              plan.valid && polls > 0 && !preCancelled.usedModel && preCancelled.audio.getNumSamples() == 0);
    }
    {
        std::atomic<bool> cancel { false }, watcherSeen { false }, done { false };
        NsfHifiganRenderResult cancelledResult;
        std::thread renderThread([&]
        {
            const auto rendererThread = std::this_thread::get_id();
            cancelledResult = direct(longSeconds, [&, rendererThread]
            {
                if (std::this_thread::get_id() != rendererThread)
                    watcherSeen.store(true, std::memory_order_release);
                return cancel.load(std::memory_order_acquire);
            });
            done.store(true, std::memory_order_release);
        });
        const auto reached = wait([&] { return watcherSeen.load(std::memory_order_acquire)
            || done.load(std::memory_order_acquire); }, 15000);
        check("real_onnx_run_watch_started", reached && watcherSeen.load(std::memory_order_acquire)
            && !done.load(std::memory_order_acquire));
        const auto since = juce::Time::getMillisecondCounterHiRes();
        cancel.store(true, std::memory_order_release);
        const auto prompt = wait([&] { return done.load(std::memory_order_acquire); }, 2000);
        check("active_onnx_run_cancel_finishes_within_two_seconds", prompt);
        renderThread.join();
        report->setProperty("direct_onnx_cancel_ms", elapsed(since));
        report->setProperty("direct_onnx_cancel_error", cancelledResult.error);
        check("active_onnx_cancel_does_not_return_partial_audio",
              !cancelledResult.usedModel && cancelledResult.buffer.getNumSamples() == 0);
        const auto recovered = direct(shortSeconds);
        check("same_cpu_session_renders_after_onnx_cancel", recovered.usedModel && valid(recovered.buffer));
    }
    const auto nativeRequest = [&](double seconds)
    {
        Mld5FileRenderRequest request;
        request.sourceFile = sample; request.sourceDurationSeconds = sourceSeconds;
        request.targetDurationSeconds = seconds; request.pitchBackend = PitchRenderBackend::nsfHifigan;
        request.hifiganModelDirectory = modelDirectory; request.inference = execution;
        request.sourceMidi = pitch(seconds); request.targetMidi = pitch(seconds);
        request.timeMap = {{0, 0}, {seconds, sourceSeconds}};
        request.preserveUneditedSource = false;
        return request;
    };
    const auto voicebankRequest = [&](double seconds)
    {
        UtauRenderRequest request;
        request.voicebankDirectory = bank; request.targetDurationSeconds = seconds + .1;
        UtauNoteRenderSpec note;
        note.alias = "a"; note.midiNote = 57; note.startSeconds = .05;
        note.durationSeconds = seconds; note.flags = "g0";
        request.notes = { note };
        return request;
    };
    RenderService service;
    {
        std::atomic<int> completions { 0 };
        std::atomic<bool> discarded { false };
        service.renderMld5File(nativeRequest(longSeconds), [&](auto) { ++completions; },
            {"native-cancel", 0, longSeconds, [&] { discarded.store(true); }});
        check("native_file_job_is_active", wait([&] { return service.hasActiveJobs(); }));
        juce::Thread::sleep(150);
        check("native_file_cancel_targets_running_job", service.hasActiveJobs() && completions.load() == 0);
        const auto since = juce::Time::getMillisecondCounterHiRes();
        service.cancelAll();
        const auto ms = elapsed(since);
        report->setProperty("native_file_cancel_ms", ms);
        check("native_file_cancel_drains_within_two_seconds", ms < 2000 && !service.hasActiveJobs());
        check("native_file_cancel_discards_without_completion", discarded.load() && completions.load() == 0);
        std::atomic<bool> done { false };
        RenderedAudio recovered;
        service.renderMld5File(nativeRequest(shortSeconds), [&](auto audio)
        { recovered = std::move(audio); done.store(true, std::memory_order_release); });
        check("native_service_renders_again_after_cancel", wait([&]
            { return done.load(std::memory_order_acquire); }, 30000));
        service.cancelAll();
        check("native_service_recovery_uses_real_model_audio", recovered.backend.contains("nsf")
            && valid(recovered.buffer) && recovered.warning.isEmpty());
        report->setProperty("native_recovered_backend", recovered.backend);
    }
    {
        std::atomic<int> completions { 0 }, pieces { 0 };
        std::atomic<bool> discarded { false }, pitchEntered { false };
        auto request = voicebankRequest(longSeconds);
        request.notes.front().timelinePitchCents = [&](double)
        { pitchEntered.store(true, std::memory_order_release); return 0.0f; };
        request.notePiece = [&](std::size_t, const juce::AudioBuffer<float>&, double, double,
            const std::function<float(double)>&, const std::function<float(double)>&) { ++pieces; };
        service.renderNsfUtau(std::move(request), modelDirectory, execution,
            [&](auto) { ++completions; }, {"voicebank-cancel", 0, longSeconds,
                [&] { discarded.store(true); }});
        check("native_voicebank_job_builds_real_pitch_frames", wait([&]
            { return pitchEntered.load(std::memory_order_acquire); }));
        juce::Thread::sleep(150);
        check("native_voicebank_cancel_targets_running_job", service.hasActiveJobs() && completions.load() == 0);
        const auto since = juce::Time::getMillisecondCounterHiRes();
        service.cancelAll();
        const auto ms = elapsed(since);
        report->setProperty("native_voicebank_cancel_ms", ms);
        check("native_voicebank_cancel_drains_within_two_seconds", ms < 2000 && !service.hasActiveJobs());
        check("native_voicebank_cancel_publishes_no_piece_or_completion",
              discarded.load() && pieces.load() == 0 && completions.load() == 0);
        std::atomic<bool> done { false };
        RenderedAudio recovered;
        service.renderNsfUtau(voicebankRequest(shortSeconds), modelDirectory, execution,
            [&](auto audio) { recovered = std::move(audio); done.store(true, std::memory_order_release); });
        check("native_voicebank_service_renders_again_after_cancel", wait([&]
            { return done.load(std::memory_order_acquire); }, 30000));
        service.cancelAll();
        check("native_voicebank_recovery_uses_real_model_audio",
              recovered.backend == "hifisampler-native-voicebank" && valid(recovered.buffer)
              && recovered.warning.isEmpty());
        report->setProperty("voicebank_recovered_backend", recovered.backend);
    }
    return finish();
}
}
