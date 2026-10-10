#pragma once
#include "../NativePitchVoicingDisplay.h"
#include <random>

namespace hachi
{
inline bool runNativeConsonantDisplaySmoke(const juce::File& folder, const juce::File& realSource = {})
{
    folder.createDirectory(); bool ok = true;
    const auto check = [&](const char* name, bool pass)
    { ok &= pass; std::cout << name << '=' << pass << std::endl; };
    const auto near = [](double a, double b) { return std::abs(a - b) < 1.e-6; };
    const auto source = folder.getChildFile("voiced-frication.wav");
    juce::AudioBuffer<float> audio(1, 96000);
    std::mt19937 rng(135); std::uniform_real_distribution<float> random(-.25f, .25f);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto t = i / 48000.;
        const auto noise = random(rng);
        const auto friction = (t > .35 && t < .8) || (t > 1.3 && t < 1.7);
        const auto breathy = t > .85 && t < 1.15;
        audio.setSample(0, i, friction ? noise
            : .18f * std::sin(float(t * 220 * juce::MathConstants<double>::twoPi))
                + (breathy ? noise * .2f : 0.f));
    }
    juce::WavAudioFormat wav; auto stream = source.createOutputStream();
    if (!stream) return false; stream->setPosition(0); stream->truncate();
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 48000, 1, 24, {}, 0));
    if (!writer) return false; stream.release();
    check("consonant_display_source_written", writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()));
    writer.reset();

    ProjectData data; TrackData track; track.id = "native"; track.pitchAlgorithm = PitchAlgorithm::world;
    ClipData clip; clip.id = "recording"; clip.sourceFile = source; clip.startSeconds = .3;
    clip.durationSeconds = clip.sourceDurationSeconds = 2;
    for (int i = 0; i < 2; ++i)
    {
        NoteData note; note.id = "part" + juce::String(i); note.label = "shu";
        note.startSeconds = i; note.durationSeconds = 1; note.midiNote = note.sourceMidiCenter = 57;
        note.sourcePitchMeasured = true; note.utauAutoPitchTransition = false;
        note.pitchControlPoints = {{0, 67}, {1, 67}};
        // Deliberately erroneous old F0: independent acoustic evidence must
        // work without replacing this contour or the user's placed points.
        for (int frame = 0; frame <= 200; ++frame)
            note.contour.push_back({frame * .005, 0, 0, true, 1000, true});
        clip.notes.push_back(note);
    }
    track.clips.push_back(clip); data.tracks.push_back(track);
    ProjectModel model; model.replace(data); const auto fingerprint = model.contentFingerprint();
    const auto revision = model.revisionNumber(); I18n strings; PianoRollComponent roll(model, strings);
    roll.setBounds(0, 0, 1000, 1700); roll.setPixelsPerSecond(300);
    roll.setFocusedTrack(track.id); roll.setFocusedClip(clip.id);
    roll.setShowOriginalPitchLine(false); roll.setShowNativeWaveforms(false);
    roll.setShowEnvelope(false); roll.setShowLyrics(false); roll.diagnosticRefresh();
    check("consonant_display_off_by_default", !roll.showsConsonantPitchDashed());
    const auto top = int(roll.diagnosticYForMidi(69) - 15);
    const auto area = juce::Rectangle<int>(0, top, 850, 350);
    const auto picture = [&] { return roll.createComponentSnapshot(area); };
    const auto equal = [](const juce::Image& a, const juce::Image& b)
    {
        if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight()) return false;
        for (int y = 0; y < a.getHeight(); ++y) for (int x = 0; x < a.getWidth(); ++x)
            if (a.getPixelAt(x,y) != b.getPixelAt(x,y)) return false;
        return true;
    };
    const auto ink = [&](const juce::Image& image, double from, double to)
    {
        int count = 0; const auto y = int(roll.diagnosticYForMidi(67)) - top;
        for (int x = int(roll.diagnosticEdgeX(from)); x < int(roll.diagnosticEdgeX(to)); ++x)
            for (int row = y - 2; row <= y + 2; ++row)
            { const auto pixel = image.getPixelAt(x,row); if (pixel.getRed() > 145 && pixel.getGreen() > 145 && pixel.getBlue() > 145) ++count; }
        return count;
    };
    const auto save = [&](const char* name, const juce::Image& image)
    {
        auto out = folder.getChildFile(name).createOutputStream();
        if (!out) return false; out->setPosition(0); out->truncate();
        return juce::PNGImageFormat().writeImageToStream(image, *out);
    };
    for (const auto tool : {PianoRollComponent::Tool::note, PianoRollComponent::Tool::points})
    {
        roll.setTool(tool); roll.setConsonantPitchDashed(false);
        const auto solid = picture();
        const auto start = juce::Time::getMillisecondCounter();
        roll.setConsonantPitchDashed(true);
        check("consonant_display_enabling_is_nonblocking", juce::Time::getMillisecondCounter() - start < 250);
        for (int i = 0; i < 1000 && roll.diagnosticConsonantAnalysisPending(); ++i) juce::Thread::sleep(5);
        check("consonant_display_background_analysis_finishes", !roll.diagnosticConsonantAnalysisPending());
        const auto dashed = picture();
        const auto a = ink(solid, .75, 1.0), b = ink(dashed, .75, 1.0);
        const auto c = ink(solid, 1.7, 1.9), d = ink(dashed, 1.7, 1.9);
        std::cout << "consonant_ink=" << a << '/' << b << ',' << c << '/' << d << std::endl;
        check("consonant_display_noise_has_visible_dashes_in_both_parts", b > 0 && b < a * .8 && d > 0 && d < c * .8);
        check("breath_display_marks_pitched_breathy_vowel", ink(dashed, 1.2, 1.38) > 0
            && ink(dashed, 1.2, 1.38) < ink(solid, 1.2, 1.38) * .8);
        check("consonant_display_voiced_pitch_stays_solid", ink(solid, .42, .58) == ink(dashed, .42, .58));
        check("consonant_display_preview_written", save(tool == PianoRollComponent::Tool::note ? "consonant-note.png" : "consonant-points.png", dashed));
        roll.setConsonantPitchDashed(false);
        check("consonant_display_off_restores_exact_picture", equal(solid, picture()));
    }
    check("consonant_display_never_changes_project_or_revision", model.contentFingerprint() == fingerprint && model.revisionNumber() == revision);

    roll.setTool(PianoRollComponent::Tool::note); roll.setShowPitchLine(false);
    const auto hidden = picture(); roll.setConsonantPitchDashed(true);
    check("consonant_display_respects_pitch_line_visibility", equal(hidden, picture()));
    roll.setShowPitchLine(true); roll.setConsonantPitchDashed(false);
    auto utau = data; utau.tracks[0].pitchAlgorithm = PitchAlgorithm::utau;
    model.replace(utau); roll.diagnosticRefresh(); const auto utauSolid = picture();
    roll.setConsonantPitchDashed(true);
    check("consonant_display_utau_is_unchanged", equal(utauSolid, picture()));

    auto masked = data; auto& raw = masked.tracks[0].clips[0]; raw.notes.resize(1);
    raw.durationSeconds = raw.sourceDurationSeconds = 1;
    raw.notes[0].pitchControlPoints.clear();
    for (auto& p : raw.notes[0].contour)
    { p.hasManualTarget = false; if (p.timeSeconds > .4 && p.timeSeconds < .7) p.voiced = false; }
    roll.setConsonantPitchDashed(false); model.replace(masked); roll.diagnosticRefresh();
    const auto missingF0 = picture(); roll.setConsonantPitchDashed(true);
    for (int i = 0; i < 1000 && roll.diagnosticConsonantAnalysisPending(); ++i) juce::Thread::sleep(5);
    const auto guide = picture();
    check("consonant_display_missing_f0_has_no_reference", ink(missingF0,.82,.94) == 0 && ink(guide,.82,.94) == 0);
    check("consonant_display_missing_f0_remains_unvoiced", !model.snapshot().tracks[0].clips[0].notes[0].contour[100].voiced);
    check("consonant_display_reference_preview_written", save("consonant-reference.png", guide));

    ClipData mapped = clip; mapped.startSeconds = 10; mapped.sourceOffsetSeconds = .2;
    mapped.sourceDurationSeconds = 1; mapped.sourceTimeMap = {{0,0},{1,.2},{2,1}};
    NoteData span; span.durationSeconds = 2;
    const auto stretched = nativeNoiseDisplayRanges(mapped, span, {{.3,.6}});
    check("consonant_display_source_offset_and_nonlinear_stretch", stretched.size() == 1
        && near(stretched[0].first,10.5) && near(stretched[0].second,11.25));
    mapped.audioStartSeconds = .4; mapped.audioDurationSeconds = 2; mapped.durationSeconds = 3;
    for (auto& p : mapped.sourceTimeMap) p.targetSeconds += .4;
    span.startSeconds = .4;
    const auto padded = nativeNoiseDisplayRanges(mapped, span, {{.3,.6}});
    check("consonant_display_empty_padding_is_not_source_audio", padded.size() == 1
        && near(padded[0].first,10.9) && near(padded[0].second,11.65));
    span.startSeconds = 1; span.durationSeconds = .2;
    const auto cropped = nativeNoiseDisplayRanges(mapped, span, {{.3,.6}});
    check("consonant_display_crop_does_not_reveal_removed_audio", cropped.size() == 1
        && near(cropped[0].first,11) && near(cropped[0].second,11.2));
    if (realSource.existsAsFile())
    {
        NativePitchVoicingCache cache; const auto start = juce::Time::getMillisecondCounter();
        cache.request({realSource});
        check("consonant_display_real_recording_request_nonblocking", juce::Time::getMillisecondCounter() - start < 250);
        for (int i = 0; i < 3000 && cache.pending(); ++i) juce::Thread::sleep(5);
        const auto ranges = cache.rangesFor(realSource); double duration = 0;
        if (ranges) for (const auto& span : *ranges) duration += span.second - span.first;
        check("consonant_display_real_recording_identifies_clear_noise", ranges && duration > .1);
        std::cout << "consonant_real_noise_seconds=" << duration << std::endl;
        cache.request({realSource});
        check("consonant_display_reuses_source_cache", cache.rangesFor(realSource) == ranges && !cache.pending());
        cache.request({}); check("consonant_display_disabled_releases_source_cache", !cache.pending() && !cache.rangesFor(realSource));
    }
    return ok;
}
}
