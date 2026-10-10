#pragma once

namespace hachi
{
// Work only on the caller's disposable fixture: analysis can persist its HJM.
inline bool runNativeImportPerformanceSmoke(const juce::File& source, const juce::File& models)
{
    bool ok = true;
    const auto check = [&](const char* name, bool pass)
    { ok &= pass; std::cout << name << '=' << pass << std::endl; };
    const auto timed = [](const char* name, auto action)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        action();
        const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
        std::cout << name << "_ms=" << ms << std::endl;
        return ms;
    };
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    if (!reader) return false;
    const auto duration = reader->lengthInSamples / reader->sampleRate;
    ProjectModel model; juce::String id;
    timed("import", [&] { id = model.addAudioFile(source, duration); });
    const auto initial = model.snapshot().tracks.front().clips.front();
    auto initialTrack = model.snapshot().tracks.front();
    initialTrack.pitchAlgorithm = PitchAlgorithm::nsfHifigan; initialTrack.nativeNsfAudio = true;
    const auto pendingRequest = AudioEngine::diagnosticNativeRequest(initial, initialTrack);
    const auto pendingPreserved = backend::canPreserveNativeSource(pendingRequest, reader->sampleRate,
        static_cast<int>(reader->lengthInSamples), static_cast<int>(reader->lengthInSamples));
    check("pending_import_preserves_source", pendingPreserved);
    if (!initial.notes.empty())
    {
        for (int edit = 0; edit < 5; ++edit)
        {
            auto changed = initial;
            auto& n = changed.notes.front();
            if (edit == 0) n.midiNote += 1.f;
            if (edit == 1) n.modulation = 0.f;
            if (edit == 2) n.drift = .5f;
            if (edit == 3) n.vibratoEnabled = true;
            if (edit == 4) n.pitchControlPoints = {{0, 60.f}, {n.durationSeconds, 60.f}};
            check(("pending_pitch_edit_not_bypassed_" + juce::String(edit)).toRawUTF8(),
                !AudioEngine::diagnosticNativeRequest(changed, initialTrack).preserveUneditedSource);
        }
    }
    if (pendingPreserved)
    {
        backend::RenderService service; backend::RenderedAudio rendered;
        juce::WaitableEvent finished;
        timed("pending_original_render", [&]
        {
            service.renderMld5File(pendingRequest, [&](auto value)
            { rendered = std::move(value); finished.signal(); });
            check("pending_render_completed", finished.wait(10000));
            service.cancelAll();
        });
        juce::AudioBuffer<float> original(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&original, 0, original.getNumSamples(), 0, true, true);
        auto exact = rendered.backend == "native-source-preserved"
            && rendered.buffer.getNumChannels() == original.getNumChannels()
            && rendered.buffer.getNumSamples() == original.getNumSamples();
        if (exact) for (int ch = 0; ch < original.getNumChannels(); ++ch)
            for (int i = 0; i < original.getNumSamples(); ++i)
                exact &= rendered.buffer.getSample(ch, i) == original.getSample(ch, i);
        check("pending_stereo_pcm_exact", exact);
    }
    backend::AnalysisConfig config;
    config.gameModelDirectory = models.getChildFile("game/medium");
    config.fcpeModelPath = models.getChildFile("fcpe/fcpe.onnx");
    config.inference = backend::InferenceBackend::cpu;
    juce::String error;
    decltype(backend::AnalysisService::analyse(source, config, error)) analysis;
    timed("analysis", [&] { analysis = backend::AnalysisService::analyse(source, config, error); });
    if (analysis.notes.empty()) { std::cout << error << std::endl; return false; }
    check("real_game_fcpe_used", analysis.status.activeBackend == "GAME+FCPE");
    timed("apply_analysis", [&] { model.setClipAudioAnalysis(id, analysis.notes, initial); });
    auto data = model.snapshot(); auto& track = data.tracks.front();
    track.pitchAlgorithm = PitchAlgorithm::nsfHifigan; track.nativeNsfAudio = true;
    model.replace(data); model.dispatchPendingMessages();
    std::size_t frames = 0;
    for (const auto& note : track.clips.front().notes) frames += note.contour.size();
    std::cout << "notes=" << track.clips.front().notes.size() << ";frames=" << frames << std::endl;
    timed("waveform_hash", [&] { juce::ignoreUnused(AudioEngine::nativeClipWaveformHash(track.clips.front(), track)); });
    timed("render_request", [&] { juce::ignoreUnused(AudioEngine::diagnosticNativeRequest(track.clips.front(), track)); });
    I18n strings; PianoRollComponent roll(model, strings); TimelineComponent lane(model);
    timed("layout", [&] { roll.setFocusedTrack(track.id); roll.setFocusedClip(id); roll.setPixelsPerSecond(100); roll.setRowHeight(24); roll.diagnosticRefresh(); });
    lane.setPixelsPerSecond(100); lane.setRowHeight(110);
    juce::Thread::sleep(500);
    const auto paint = [&](juce::Component& component) { return component.createComponentSnapshot({0, 0, 1900, component.getHeight()}); };
    timed("first_roll_paint", [&] { juce::ignoreUnused(paint(roll)); });
    timed("steady_roll_paint", [&] { juce::ignoreUnused(paint(roll)); });
    roll.setShowNativeWaveforms(false);
    timed("roll_without_waveform", [&] { juce::ignoreUnused(paint(roll)); });
    roll.setShowPitchLine(false); roll.setShowOriginalPitchLine(false);
    timed("roll_without_pitch", [&] { juce::ignoreUnused(paint(roll)); });
    timed("timeline_paint", [&] { juce::ignoreUnused(paint(lane)); });
    roll.setShowNativeWaveforms(true); roll.setShowPitchLine(true);
    const auto softwarePaint = [&](juce::Component& component)
    {
        juce::Image image(juce::Image::ARGB, 1900, component.getHeight(), true, juce::SoftwareImageType{});
        juce::Graphics g(image); component.paintEntireComponent(g, true);
    };
    timed("software_roll_paint", [&] { softwarePaint(roll); });
    timed("software_timeline_paint", [&] { softwarePaint(lane); });
    juce::Image viewport(juce::Image::ARGB, 1400, 500, true, juce::SoftwareImageType{});
    const auto y = std::max(0, static_cast<int>(roll.diagnosticNoteY(0)) - 180);
    const auto repaintViewport = [&]
    { juce::Graphics g(viewport); g.setOrigin(0, -y); roll.paintEntireComponent(g, true); };
    repaintViewport();
    const auto framesMs = timed("ten_visible_repaints", [&] { for (int i = 0; i < 10; ++i) repaintViewport(); });
    check("visible_repaints_under_100ms_each", framesMs < 1000.0);
    return ok;
}
}
