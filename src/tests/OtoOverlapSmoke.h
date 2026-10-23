#pragma once
#include "../OtoWaveformEditorComponent.h"
#include <iostream>

namespace hachi
{
inline bool otoOverlapSmoke(const juce::File& folder)
{
    folder.createDirectory();
    bool ok = true;
    juce::Array<juce::var> checks;
    const auto check = [&](const juce::String& name, bool passed)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty("name", name); item->setProperty("ok", passed);
        checks.add(juce::var(item)); ok &= passed;
        std::cout << name << '=' << passed << std::endl;
    };
    const auto tone = folder.getChildFile("tone.wav");
    {
        juce::AudioBuffer<float> audio(1, 44100);
        for (int i = 0; i < audio.getNumSamples(); ++i)
            audio.setSample(0, i, .15f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 220 * i / 44100)));
        auto stream = tone.createOutputStream();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 44100, 1, 16, {}, 0));
        if (!writer || !writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples())) return false;
    }
    HachiLookAndFeel look;
    for (int mode = 0; mode < 3; ++mode)
    for (int count = 2; count <= (mode == 0 ? 2 : 4); ++count)
    {
        const auto prefix = juce::String(mode) + "-" + juce::String(count) + ":";
        const auto bank = folder.getChildFile("bank-" + prefix.dropLastCharacters(1));
        bank.createDirectory(); tone.copyFileTo(bank.getChildFile("tone.wav"));
        const auto offset = mode == 0 ? 0.0 : 50.0;
        bank.getChildFile("oto.ini").replaceWithText("tone.wav=tone," + juce::String(offset) + ",100,-450,60,0\n");
        juce::StringArray warnings;
        auto rows = SampleSettings::loadVoicebankOto(bank, warnings, mode != 0, mode == 2);
        if (rows.empty()) { check(prefix + "load", false); continue; }
        OtoWaveformEditorComponent editor(rows.front(), mode != 0, mode == 2, [] {});
        editor.setLookAndFeel(&look); editor.setBounds(0, 0, 1040, 620);
        if (mode != 0) editor.diagnosticSetRegionCount(count);
        editor.diagnosticSetAnalysisLayers(mode != 0, mode != 0);
        editor.diagnosticZoomOut(); editor.diagnosticZoomOut();
        auto& view = editor.diagnosticWaveformComponent();
        const auto event = [&](juce::Point<float> at, juce::Point<float> down)
        {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 0, 0, 0, 0, 0,
                &view, &view, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, at != down);
        };
        const auto drag = [&](double fromMs, double toMs, float y, float grabPixels = 0.0f)
        {
            const juce::Point<float> from(editor.diagnosticXForTime(fromMs) + grabPixels, y);
            const juce::Point<float> to(editor.diagnosticXForTime(toMs) + grabPixels, y);
            view.mouseDown(event(from, from));
            // Grabbing the side of a label must not make its line jump.
            view.mouseDrag(event(from, from));
            view.mouseDrag(event(to, from)); view.mouseUp(event(to, from));
        };
        const auto value = [&](int index) { return editor.diagnosticParameterText(index).getDoubleValue(); };
        const auto near = [](double a, double b) { return std::abs(a - b) < .01; };
        const auto waveY = static_cast<float>(editor.diagnosticWaveformBounds().getCentreY());
        const auto spectrumY = mode != 0 ? static_cast<float>(editor.diagnosticSpectrumBounds().getCentreY()) : waveY;
        const auto firstBoundary = editor.diagnosticBoundaryMs(0);
        drag(offset, -80, waveY);
        check(prefix + "overlap_crosses_recording_start", near(value(4), -80 - offset));
        check(prefix + "other_markers_stay_put", near(value(0), offset) && near(value(3), 60)
            && near(editor.diagnosticBoundaryMs(0), firstBoundary));
        drag(-80, -140, spectrumY);
        check(prefix + "negative_overlap_moves_further_left", near(value(4), -140 - offset));
        // All timing markers coincide; select the visible green badge, away
        // from the line hit tolerance, instead of the first coincident line.
        editor.diagnosticTypeParameter(3, "0"); editor.diagnosticTypeParameter(4, "0");
        drag(offset, -100, 9, 13);
        check(prefix + "overlap_badge_selects_own_marker", near(value(4), -100 - offset)
            && near(value(3), 0) && near(value(0), offset));
        drag(offset, -60, 25, 13);
        check(prefix + "preutterance_badge_selects_own_marker", near(value(3), -60 - offset)
            && near(value(4), -100 - offset));
        drag(-60, 1100, waveY);
        check(prefix + "signed_timing_can_pass_recording_end", near(value(3), 1100 - offset));
        drag(1100, -60, waveY);
        editor.diagnosticSave();
        rows = SampleSettings::loadVoicebankOto(bank, warnings, mode != 0, mode == 2);
        check(prefix + "saved_negative_timing_roundtrips", !rows.empty()
            && near(rows.front().overlapMs, -100 - offset) && near(rows.front().preutteranceMs, -60 - offset));
        if (mode == 2 && count == 4)
        {
            juce::PNGImageFormat png;
            auto stream = folder.getChildFile("negative-overlap.png").createOutputStream();
            check("snapshot", stream && png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream));
        }
        drag(offset, -120, waveY);
        check(prefix + "sample_offset_still_bounded", near(value(0), 0));
        drag(offset + 450, 1200, waveY);
        check(prefix + "sample_end_still_bounded", near(value(0) - value(2), 1000));
        if (mode != 0)
        {
            const auto boundary = editor.diagnosticXForBoundary(0);
            const juce::Point<float> from(boundary, waveY), to(editor.diagnosticXForTime(-150), waveY);
            view.mouseDown(event(from, from)); view.mouseDrag(event(to, from)); view.mouseUp(event(to, from));
            check(prefix + "region_boundary_still_bounded", editor.diagnosticBoundaryMs(0) >= 0);
        }
        editor.setLookAndFeel(nullptr);
    }
    auto* report = new juce::DynamicObject(); report->setProperty("ok", ok); report->setProperty("checks", checks);
    folder.getChildFile("oto-overlap-validation.json").replaceWithText(juce::JSON::toString(juce::var(report), true));
    return ok;
}
}
