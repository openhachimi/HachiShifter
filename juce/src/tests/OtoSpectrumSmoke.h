#pragma once
#include "../OtoWaveformEditorComponent.h"
#include <iostream>

namespace hachi
{
inline bool otoSpectrumSmoke(const juce::File& folder, const juce::File& vocal)
{
    using namespace backend;
    folder.createDirectory();
    juce::Array<juce::var> checks; bool ok = true;
    const auto check = [&](const juce::String& name, bool pass)
    {
        auto* row = new juce::DynamicObject(); row->setProperty("name", name); row->setProperty("ok", pass);
        checks.add(juce::var(row)); ok &= pass; std::cout << name << "=" << pass << std::endl;
    };
    const auto wait = [](auto ready)
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 60000;
        while (!ready() && juce::Time::getMillisecondCounterHiRes() < until) juce::Thread::sleep(10);
        return ready();
    };
    const auto writeWave = [](const juce::File& file, double seconds)
    {
        constexpr double rate = 44100;
        juce::AudioBuffer<float> buffer(1, std::max(1, static_cast<int>(rate * seconds)));
        buffer.clear();
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto t = i / rate;
            const bool first = t >= .15 && t < .95, second = t >= 1.35 && t < 2.3;
            const auto hz = first ? 220.0 : 440.0;
            if (first || second) buffer.setSample(0, i, static_cast<float>(
                .4 * std::sin(juce::MathConstants<double>::twoPi * hz * t)
                + .1 * std::sin(juce::MathConstants<double>::twoPi * hz * 2 * t)));
        }
        auto stream = file.createOutputStream(); if (!stream) return false;
        stream->setPosition(0); stream->truncate();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), rate, 1, 16, {}, 0));
        if (!writer) return false; stream.release(); return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    };
    const auto wav = folder.getChildFile("source-tones.wav");
    check("source_fixture_written", writeWave(wav, 2.5));
    const auto sourceHash = juce::SHA256(wav).toHexString();
    VoicebankOtoEntry entry;
    entry.audioFile = wav; entry.alias = "source"; entry.sourceName = wav.getFileName();
    entry.offsetMs = 100; entry.cutoffMs = -2250; entry.preutteranceMs = 150; entry.overlapMs = 40;
    entry.consonantMs = 300; entry.hasJieOto = true;
    entry.jieOnsetMs = 300; entry.jieGlideMs = 700; entry.jieNucleusMs = 1700;
    entry.mouClasses = "CVVC";
    HachiLookAndFeel look;
    const auto snapshot = [&](OtoWaveformEditorComponent& editor, const juce::String& name)
    {
        juce::PNGImageFormat png;
        auto stream = folder.getChildFile(name + ".png").createOutputStream();
        if (!stream) return false;
        stream->setPosition(0); stream->truncate();
        return png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f), *stream);
    };
    {
        OtoWaveformEditorComponent editor(entry, false, false, [] {});
        editor.setLookAndFeel(&look); editor.setBounds(0, 0, 1040, 620);
        check("background_analysis_finishes", wait([&] { return editor.diagnosticAnalysisReady(); }));
        const auto data = editor.diagnosticAnalysis();
        check("spectrum_and_fcpe_loaded", data && data->spectrogram.isValid() && data->pitchBackend == "FCPE"
            && data->error.isEmpty() && data->warning.isEmpty());
        if (data)
        {
            check("full_original_duration", std::abs(data->durationSeconds - 2.5) < .001);
            check("log_axis_roundtrip", std::abs(data->hzForFraction(data->fractionForHz(440)) - 440) < .001);
            const auto pitchAt = [&](double t)
            {
                auto it = std::lower_bound(data->pitch.begin(), data->pitch.end(), t,
                    [](const auto& point, double time) { return point.timeSeconds < time; });
                return it != data->pitch.end() ? *it : FcpeFrame{};
            };
            check("first_original_f0_220hz", pitchAt(.5).voiced && std::abs(pitchAt(.5).midi - 57) < .3);
            check("second_original_f0_440hz", pitchAt(1.8).voiced && std::abs(pitchAt(1.8).midi - 69) < .3);
            check("silence_has_no_f0", !pitchAt(1.15).voiced);
            const auto& image = data->spectrogram;
            const auto brightness = [&](double seconds, double hz)
            {
                const int x = juce::jlimit(0, image.getWidth()-1, static_cast<int>(seconds / data->durationSeconds * image.getWidth()));
                const int y = juce::jlimit(0, image.getHeight()-1, static_cast<int>((1-data->fractionForHz(hz))*image.getHeight()));
                return image.getPixelAt(x,y).getBrightness();
            };
            check("spectrum_matches_220hz_and_silence", brightness(.5,220) > brightness(.5,700) + .15f
                && brightness(.5,220) > brightness(1.15,220) + .3f);
        }
        const auto wave = editor.diagnosticWaveformBounds(), spectrum = editor.diagnosticSpectrumBounds();
        check("wave_and_spectrum_share_x_axis", wave.getX() == spectrum.getX() && wave.getWidth() == spectrum.getWidth());
        check("panels_do_not_overlap", wave.getBottom() < spectrum.getY());
        auto& view = editor.diagnosticWaveformComponent();
        const auto event = [&](juce::Point<float> at, juce::Point<float> down)
        {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 0, 0, 0, 0, 0,
                &view, &view, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, at != down);
        };
        const auto pitchPosition = [&](double seconds, double hz, bool upper)
        {
            const auto panel = upper ? editor.diagnosticWaveformBounds() : editor.diagnosticSpectrumBounds();
            return juce::Point<float>(editor.diagnosticXForTime(seconds * 1000),
                static_cast<float>(panel.getBottom() - data->fractionForHz(hz) * panel.getHeight()));
        };
        const auto click = [&](juce::Point<float> at)
        { view.mouseDown(event(at, at)); view.mouseUp(event(at, at)); };
        const auto readoutMatchesFrame = [&](double clickedTime, double expectedMidi)
        {
            if (!data || data->pitch.empty()) return false;
            const auto selectedTime = editor.diagnosticPitchTimeReadout().getDoubleValue();
            const auto frame = std::min_element(data->pitch.begin(), data->pitch.end(),
                [selectedTime](const auto& a, const auto& b)
                { return std::abs(a.timeSeconds - selectedTime) < std::abs(b.timeSeconds - selectedTime); });
            const auto predictedHz = 440.0 * std::pow(2.0, (frame->midi - 69.0) / 12.0);
            // The UI reports the actual inferred frame, not the ideal oscillator frequency.
            return frame->voiced && std::abs(frame->midi - expectedMidi) < .3
                && std::abs(frame->timeSeconds - selectedTime) <= .00051
                && std::abs(selectedTime - clickedTime) < .006
                && std::abs(editor.diagnosticPitchReadout().getDoubleValue() - predictedHz) <= .051;
        };
        check("f0_readout_initially_empty", editor.diagnosticPitchReadout() == "-- Hz");
        if (data)
        {
            click(pitchPosition(.5, 220, true));
            check("upper_click_reads_220hz_frame", readoutMatchesFrame(.5, 57));
            click(pitchPosition(1.65, 440, false));
            check("lower_click_reads_440hz_frame", readoutMatchesFrame(1.65, 69));
            const auto previousReadout = editor.diagnosticPitchTimeReadout();
            click(pitchPosition(1.15, 220, true));
            check("unvoiced_click_keeps_previous_frame", editor.diagnosticPitchTimeReadout() == previousReadout);
            auto blank = pitchPosition(.5, 220, true); blank.y = static_cast<float>(wave.getY() + 10);
            click(blank);
            check("off_curve_click_keeps_previous_frame", editor.diagnosticPitchTimeReadout() == previousReadout);
            check("selected_frame_snapshot", snapshot(editor, "oto-selected-frame"));
            const auto from = juce::Point<float>(editor.diagnosticXForTime(400), static_cast<float>(wave.getY() + 10));
            const auto to = juce::Point<float>(editor.diagnosticXForTime(450), from.y);
            view.mouseDown(event(from, from)); view.mouseDrag(event(to, from)); view.mouseUp(event(to, from));
            check("f0_readout_preserves_boundary_drag", std::abs(editor.diagnosticParameterText(1).getDoubleValue() - 350) < .01);
        }
        const auto before = editor.diagnosticXForTime(800) - editor.diagnosticXForTime(400);
        editor.diagnosticZoomIn();
        const auto after = editor.diagnosticXForTime(800) - editor.diagnosticXForTime(400);
        check("zoom_uses_shared_time_scale", std::abs(after / before - 1.4f) < .01f);
        const auto at = editor.diagnosticXForTime(800); editor.diagnosticScrollTo(400);
        check("scroll_moves_shared_time_axis", std::abs(editor.diagnosticXForTime(800) - at) > 1.0f);
        if (data)
        {
            const auto beforeRead = editor.diagnosticXForTime(800);
            const auto from = pitchPosition(.8, 220, true);
            const auto to = from.translated(30.0f, 0.0f);
            view.mouseDown(event(from, from)); view.mouseDrag(event(to, from)); view.mouseUp(event(to, from));
            check("pitch_click_does_not_pan_zoomed_view", std::abs(editor.diagnosticXForTime(800) - beforeRead) < .01f);
            check("zoomed_scrolled_click_reads_original_frame", readoutMatchesFrame(.8, 57));
        }
        editor.diagnosticDragOffsetTo(200);
        check("oto_drag_keeps_source_analysis", editor.diagnosticAnalysis() == data
            && editor.diagnosticParameterText(0).getDoubleValue() == 200);
        editor.diagnosticSetAnalysisLayers(false, true);
        check("f0_only_has_panel", !editor.diagnosticSpectrumBounds().isEmpty());
        check("f0_only_snapshot", snapshot(editor,"oto-f0-only"));
        editor.diagnosticSetAnalysisLayers(true, false);
        check("hide_f0_clears_readout", editor.diagnosticPitchReadout() == "-- Hz");
        if (data) click(pitchPosition(.8, 220, true));
        check("hidden_f0_cannot_be_selected", editor.diagnosticPitchReadout() == "-- Hz");
        check("spectrum_only_snapshot", snapshot(editor,"oto-spectrum-only"));
        editor.diagnosticSetAnalysisLayers(false, false);
        check("both_hidden_restore_full_waveform", editor.diagnosticSpectrumBounds().isEmpty()
            && editor.diagnosticWaveformBounds().getHeight() > wave.getHeight());
        editor.diagnosticSetAnalysisLayers(true, true);
        editor.setBounds(0,0,720,400);
        check("small_window_has_both_panels", editor.diagnosticSpectrumBounds().getHeight() > 60
            && editor.diagnosticWaveformBounds().getHeight() > 40);
        check("small_window_snapshot",snapshot(editor,"oto-small"));
        editor.setLookAndFeel(nullptr);
    }
    std::shared_ptr<const OtoAudioAnalysisData> cached;
    for (const auto mode : { 0, 1, 2 })
    {
        OtoWaveformEditorComponent editor(entry, mode > 0, mode == 2, [] {});
        editor.setLookAndFeel(&look);editor.setBounds(0,0,1040,620);
        check("cached_analysis_ready_mode_"+juce::String(mode), editor.diagnosticAnalysisReady());
        if (cached) check("aliases_share_analysis_mode_"+juce::String(mode), editor.diagnosticAnalysis()==cached);
        cached = editor.diagnosticAnalysis();
        if (mode > 0)
        {
            editor.diagnosticDragBoundary(0, 450);
            check("boundary_drag_works_mode_"+juce::String(mode), std::abs(editor.diagnosticBoundaryMs(0)-350)<.01);
        }
        check("mode_snapshot_"+juce::String(mode),snapshot(editor,"oto-mode-"+juce::String(mode)));
        editor.setLookAndFeel(nullptr);
    }
    check("original_wav_unchanged",juce::SHA256(wav).toHexString()==sourceHash);
    const auto config = OtoAudioAnalysis::editorConfig();
    auto missing = OtoAudioAnalysis::request(folder.getChildFile("missing.wav"),config);
    check("missing_file_completes",wait([&]{const auto d=missing->snapshot();return d&&d->complete;}));
    check("missing_file_reports_error",missing->snapshot()&&missing->snapshot()->error.isNotEmpty());
    const auto invalid = folder.getChildFile("invalid.onnx");invalid.replaceWithText("invalid model fixture");
    auto fallbackConfig=config;fallbackConfig.fcpeModelPath=invalid;
    auto fallback=OtoAudioAnalysis::request(wav,fallbackConfig);
    check("failed_fcpe_completes",wait([&]{const auto d=fallback->snapshot();return d&&d->complete;}));
    const auto native=fallback->snapshot();
    check("failed_fcpe_keeps_spectrum_and_native_f0",native&&native->spectrogram.isValid()
        &&native->pitchBackend=="native-hq"&&!native->pitch.empty()&&native->warning.isNotEmpty());
    const auto shortWav=folder.getChildFile("short.wav");writeWave(shortWav,.0001);
    auto tiny=OtoAudioAnalysis::request(shortWav,config);
    check("very_short_recording_completes",wait([&]{const auto d=tiny->snapshot();return d&&d->complete;}));
    check("very_short_spectrum_valid",tiny->snapshot()&&tiny->snapshot()->spectrogram.isValid());
    // A queued, discarded editor never leaves a callback to a freed Component.
    const auto cancelWav=folder.getChildFile("cancel.wav");writeWave(cancelWav,2.5);
    { OtoWaveformEditorComponent discarded({.audioFile=cancelWav},false,false,[]{}); }
    auto afterClose=OtoAudioAnalysis::request(cancelWav,config);
    check("analysis_after_closing_pending_window",wait([&]{const auto d=afterClose->snapshot();return d&&d->complete;}));
    const auto oldData=afterClose->snapshot();
    cancelWav.setLastModificationTime(juce::Time::getCurrentTime()+juce::RelativeTime::seconds(2));
    auto refreshed=OtoAudioAnalysis::request(cancelWav,config);
    check("changed_source_invalidates_cache",wait([&]{const auto d=refreshed->snapshot();return d&&d->complete;})
        &&refreshed->snapshot()!=oldData);
    if (vocal.existsAsFile())
    {
        auto e=entry;e.audioFile=vocal;e.sourceName=vocal.getFileName();e.alias=juce::String::fromUTF8("原始人声");
        OtoWaveformEditorComponent editor(e,true,true,[]{});
        editor.setLookAndFeel(&look);editor.setBounds(0,0,1040,620);
        check("real_vocal_analysis",wait([&]{return editor.diagnosticAnalysisReady();})
            &&editor.diagnosticAnalysis()->pitchBackend=="FCPE");
        check("real_vocal_snapshot",snapshot(editor,"oto-real-vocal"));
        editor.setLookAndFeel(nullptr);
    }
    auto* report=new juce::DynamicObject();report->setProperty("ok",ok);report->setProperty("checks",checks);
    folder.getChildFile("oto-spectrum-validation.json").replaceWithText(juce::JSON::toString(juce::var(report),true));
    return ok;
}
}
