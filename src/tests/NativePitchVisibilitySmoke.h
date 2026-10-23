#pragma once
#include "../NativePitchVoicingDisplay.h"
#include "../NativeNoteTiming.h"
#include "../Theme.h"

namespace hachi
{
inline bool runNativePitchVisibilitySmoke(const juce::File& folder)
{
    folder.createDirectory(); bool ok = true; int checks = 0;
    const auto check = [&](const char* name, bool pass)
    { ++checks; ok &= pass; std::cout << name << '=' << pass << std::endl; };
    Palette::applyTheme("dark", juce::Colour(0xff8470c7), juce::Colour(0xffaa94ef), juce::Colour(0xff64ffa3));
    ProjectData data; TrackData track; track.id = "visibility"; track.pitchAlgorithm = PitchAlgorithm::world;
    ClipData clip; clip.id = "recording"; clip.startSeconds = .3;
    clip.durationSeconds = clip.sourceDurationSeconds = 2.5;
    for (int i = 0; i < 3; ++i)
    {
        NoteData note; note.id = "part" + juce::String(i); note.label = "a";
        note.startSeconds = i == 0 ? 0 : i == 1 ? 1 : 1.5;
        note.durationSeconds = i == 1 ? .5 : 1;
        note.midiNote = note.sourceMidiCenter = i == 2 ? 64 : 57;
        note.sourcePitchMeasured = true; note.utauAutoPitchTransition = false;
        note.nativeUnpitched = i == 1;
        if (!note.nativeUnpitched)
            note.pitchControlPoints = {{0, note.midiNote + 3}, {note.durationSeconds, note.midiNote + 3}};
        for (int frame = 0; frame <= int(note.durationSeconds / .005); ++frame)
        {
            const auto t = frame * .005;
            const auto voiced = i == 0 ? !(frame >= 80 && frame <= 140)
                : i == 2 ? frame >= 50 && frame <= 160 : false;
            note.contour.push_back({t, 0, 0, voiced, 300, true});
        }
        clip.notes.push_back(note);
    }
    track.clips.push_back(clip); data.tracks.push_back(track);
    ProjectModel model; model.replace(data); I18n strings; PianoRollComponent roll(model, strings);
    roll.setBounds(0, 0, 1050, 1900); roll.setPixelsPerSecond(300);
    roll.setFocusedTrack(track.id); roll.setFocusedClip(clip.id);
    roll.setShowOriginalPitchLine(false); roll.setShowNativeWaveforms(false);
    roll.setShowWaveforms(false); roll.setShowEnvelope(false); roll.setShowLyrics(false);
    roll.setShowNoteLabels(false); roll.diagnosticRefresh();
    const auto top = int(roll.diagnosticYForMidi(70) - 15);
    const auto area = juce::Rectangle<int>(0, top, 1000, 450);
    const auto picture = [&] { return roll.createComponentSnapshot(area); };
    const auto changedInk = [&](const juce::Image& a, const juce::Image& b, double from, double to)
    {
        int count = 0;
        for (int x = int(std::ceil(roll.diagnosticEdgeX(from))); x < int(std::floor(roll.diagnosticEdgeX(to))); ++x)
            for (int y = 0; y < a.getHeight(); ++y)
                if (a.getPixelAt(x,y) != b.getPixelAt(x,y)) ++count;
        return count;
    };
    const auto save = [&](const char* name, const juce::Image& image)
    {
        auto stream = folder.getChildFile(name).createOutputStream();
        if (!stream) return false; stream->setPosition(0); stream->truncate();
        return juce::PNGImageFormat().writeImageToStream(image, *stream);
    };
    const auto verifyGaps = [&](const juce::Image& line, const juce::Image& hidden)
    {
        return changedInk(line, hidden, .76, .94) == 0 // Ordinary note's interior UV.
            && changedInk(line, hidden, 1.36, 1.74) == 0 // Grey marker between linked notes.
            && changedInk(line, hidden, 1.84, 2.0) == 0 // Ordinary note's UV onset.
            && changedInk(line, hidden, 2.64, 2.76) == 0; // Unpitched tail.
    };
    const auto verifyAuthored = [&](const juce::Image& line, const juce::Image& hidden, bool shared)
    {
        return changedInk(line, hidden, .76, .94) > 0
            && (shared ? changedInk(line, hidden, 1.36, 1.74) > 0
                       : changedInk(line, hidden, 1.36, 1.74) == 0)
            && changedInk(line, hidden, 1.84, 2.0) > 0
            && changedInk(line, hidden, 2.64, 2.76) > 0;
    };
    const auto fingerprint = model.contentFingerprint(); const auto revision = model.revisionNumber();
    const auto request = AudioEngine::diagnosticNativeRequest(clip, track);
    check("pitch_visibility_linked_curve_exists", sharedPitchLines(track).memberFor("part0") != nullptr);
    for (const auto tool : {PianoRollComponent::Tool::note, PianoRollComponent::Tool::points,
                            PianoRollComponent::Tool::draw, PianoRollComponent::Tool::line})
    {
        roll.setTool(tool); roll.setShowPitchLine(false); const auto hidden = picture();
        roll.setShowPitchLine(true); const auto line = picture();
        check("pitch_visibility_authored_uv_dots_in_all_tools", verifyAuthored(line, hidden, true));
        check("pitch_visibility_measured_pitches_remain_visible", changedInk(line, hidden, .34, .64) > 50
            && changedInk(line, hidden, 2.16, 2.44) > 50);
        if (tool == PianoRollComponent::Tool::note)
            check("pitch_visibility_preview_written", save("pitch-visibility.png", line));
    }
    roll.setTool(PianoRollComponent::Tool::note);
    roll.setShowPitchLine(false); const auto hidden = picture();
    roll.setShowOriginalPitchLine(true); const auto original = picture();
    check("pitch_visibility_original_reference_has_same_gaps", verifyGaps(original, hidden)
        && changedInk(original, hidden, .34, .64) > 0);
    roll.setShowOriginalPitchLine(false); roll.setShowPitchLine(true);
    const auto afterRequest = AudioEngine::diagnosticNativeRequest(model.snapshot().tracks[0].clips[0], track);
    check("pitch_visibility_does_not_modify_project_or_render", model.contentFingerprint() == fingerprint
        && model.revisionNumber() == revision && request.targetMidi == afterRequest.targetMidi);

    model.flattenNotePitch({"part0", "part2"}, true);
    check("pitch_visibility_batch_flatten_works", model.snapshot().tracks[0].clips[0].notes[0].nativeIndependentPitch
        && model.snapshot().tracks[0].clips[0].notes[2].nativeIndependentPitch);
    roll.diagnosticRefresh(); roll.setShowPitchLine(false); const auto flatHidden = picture();
    roll.setShowPitchLine(true); const auto flat = picture();
    check("pitch_visibility_flatten_has_dots_only_inside_authored_notes", verifyAuthored(flat, flatHidden, false)
        && changedInk(flat, flatHidden, .34, .64) > 0);
    check("pitch_visibility_flatten_preview_written", save("pitch-visibility-flat.png", flat));

    auto vibrato = data;
    for (auto& note : vibrato.tracks[0].clips[0].notes) if (!note.nativeUnpitched)
    { note.vibratoEnabled = true; note.vibratoRealLine = false; note.vibratoLengthPercent = 100; }
    model.replace(vibrato); roll.diagnosticRefresh();
    roll.setShowPitchLine(false); const auto vibratoHidden = picture();
    roll.setShowPitchLine(true); const auto swing = picture();
    bool blueInGap = false;
    for (int x = int(roll.diagnosticEdgeX(.76)); x < int(roll.diagnosticEdgeX(.94)); ++x)
        for (int y = 0; y < swing.getHeight(); ++y)
        {
            const auto p = swing.getPixelAt(x,y);
            blueInGap |= p != vibratoHidden.getPixelAt(x,y)
                && p.getBlue() > p.getRed() + 30 && p.getBlue() > p.getGreen() + 10;
        }
    check("pitch_visibility_vibrato_uv_has_only_authored_dots", !blueInGap && verifyAuthored(swing, vibratoHidden, true));

    auto unedited = data;
    for (auto& note : unedited.tracks[0].clips[0].notes)
    { note.pitchControlPoints.clear(); for (auto& p : note.contour) p.hasManualTarget = false; }
    model.replace(unedited); roll.diagnosticRefresh(); roll.setShowPitchLine(false);
    const auto contourHidden = picture(); roll.setShowPitchLine(true);
    check("pitch_visibility_unedited_shared_curve_has_same_gaps", verifyGaps(picture(), contourHidden));
    auto measured = clip; measured.notes.resize(1);
    const auto plan = planNativeNoteMove(measured, {"part0"}, 1.0, 0, NativeNoteTimeEdit::rightEdge);
    const auto ranges = plan ? nativePitchDisplayRanges(plan->clip, plan->clip.notes[0]) : NativeNoiseRanges{};
    check("pitch_visibility_stretch_follows_measured_clock", plan && ranges.size() == 2
        && std::abs(ranges[0].second - 1.09) < .001 && std::abs(ranges[1].first - 1.71) < .001);
    auto pending = clip.notes[0]; pending.sourcePitchMeasured = false; pending.sourceMidiCenter = -1;
    pending.midiNote = 60; pending.pitchControlPoints.clear(); pending.contour = {{0,0,0,true},{1,0,0,true}};
    pending.nativeSourceStartSeconds = 0; pending.nativeSourceEndSeconds = 1;
    check("pitch_visibility_pending_placeholder_is_not_a_pitch", nativePitchDisplayRanges(clip, pending).empty());
    auto fakeMarker = clip.notes[1]; for (auto& p : fakeMarker.contour) p.voiced = true;
    check("pitch_visibility_marker_never_has_a_pitch", nativePitchDisplayRanges(clip, fakeMarker).empty());

    // Short, round dots must look different from the breath display's dashes.
    juce::Path horizontal; horizontal.startNewSubPath(0, 10); horizontal.lineTo(180, 10);
    const auto guideImage = [&](bool dashed, int left = 0)
    {
        juce::Image image(juce::Image::RGB, 180, 20, true); juce::Graphics g(image);
        g.fillAll(juce::Colours::black); g.setColour(juce::Colours::white);
        if (left) g.reduceClipRegion(juce::Rectangle<int>(left, 0, 180-left, 20));
        if (dashed) strokeNativePitchWithNoise(g, horizontal, {{.6,1.2}}, 100, 0, true);
        else strokeNativeAuthoredPitchDots(g, horizontal, {{0,.6},{1.2,1.8}}, {{0,1.8}}, 100, 0);
        return image;
    };
    const auto dots = guideImage(false), dashes = guideImage(true), croppedDots = guideImage(false, 77);
    int dotColumns = 0, dashColumns = 0, longestDot = 0, run = 0; bool phase = true;
    for (int x = 64; x < 116; ++x)
    {
        bool dot = false, dash = false;
        for (int y = 0; y < 20; ++y)
        {
            dot |= dots.getPixelAt(x,y).getRed() > 100;
            dash |= dashes.getPixelAt(x,y).getRed() > 100;
            if (x >= 77) phase &= dots.getPixelAt(x,y) == croppedDots.getPixelAt(x,y);
        }
        dotColumns += dot; dashColumns += dash;
        run = dot ? run + 1 : 0; longestDot = std::max(longestDot, run);
    }
    check("pitch_visibility_dots_are_round_short_and_separated", dotColumns > 0 && longestDot <= 4);
    std::cout << "dot_columns=" << dotColumns << " dash_columns=" << dashColumns << " longest_dot=" << longestDot << std::endl;
    check("pitch_visibility_dots_differ_from_breath_dashes", dotColumns < dashColumns);
    check("pitch_visibility_scroll_does_not_shift_dot_phase", phase);
    check("pitch_visibility_dot_style_preview_written", save("pitch-dots-style.png", dots));

    // An explicit control curve can extend beyond the audio bounds; its UV
    // portion stays editable and is shown as a guide instead of being cut off.
    auto single = unedited; single.tracks[0].clips[0].notes.resize(1);
    auto& singleClip = single.tracks[0].clips[0]; singleClip.durationSeconds = singleClip.sourceDurationSeconds = 1;
    singleClip.notes[0].pitchControlPoints = {{0,60},{1.3,60}};
    singleClip.notes[0].nativePitchHandlesPlaced = true;
    model.replace(single); roll.diagnosticRefresh(); roll.setTool(PianoRollComponent::Tool::note);
    roll.setShowPitchLine(false); const auto outsideHidden = picture(); roll.setShowPitchLine(true);
    check("pitch_visibility_manual_endpoint_outside_audio_has_dots", changedInk(picture(), outsideHidden, 1.36, 1.54) > 0);
    roll.setTool(PianoRollComponent::Tool::points);
    check("pitch_visibility_original_endpoints_still_editable", roll.diagnosticPitchAnchors("part0").size() == 2
        && std::abs(roll.diagnosticPitchAnchors("part0").back().timeSeconds - 1.3) < 1.e-7);

    auto freehand = single; freehand.tracks[0].clips[0].notes[0].pitchControlPoints.clear();
    model.replace(freehand);
    check("pitch_visibility_freehand_target_stored", model.setNotePitchCurve("part0", {{.4,65},{.7,67}}, false));
    roll.diagnosticRefresh(); roll.setTool(PianoRollComponent::Tool::note);
    roll.setShowPitchLine(false); const auto freehandHidden = picture(); roll.setShowPitchLine(true);
    check("pitch_visibility_freehand_uv_target_is_visible_as_dots", changedInk(picture(), freehandHidden, .76, .94) > 0);
    const auto authoredData = model.snapshot(); const auto& authoredClip = authoredData.tracks[0].clips[0];
    const auto& authoredNote = authoredClip.notes[0];
    const auto uvRequest = AudioEngine::diagnosticNativeRequest(authoredClip, authoredData.tracks[0]);
    bool uv = true, guideStored = true;
    for (int i = 82; i < 138; ++i)
    {
        uv &= !authoredNote.contour[std::size_t(i)].voiced && uvRequest.targetMidi[std::size_t(i)] == 0;
        guideStored &= authoredNote.contour[std::size_t(i)].hasManualTarget;
    }
    check("pitch_visibility_authored_uv_guide_does_not_create_render_f0", uv && guideStored);
    check("pitch_visibility_freehand_preview_written", save("pitch-freehand-dots.png", picture()));
    model.undo(); roll.diagnosticRefresh(); roll.setShowPitchLine(false); const auto undoneHidden = picture();
    roll.setShowPitchLine(true);
    check("pitch_visibility_undo_removes_only_authored_guide", changedInk(picture(), undoneHidden, .76, .94) == 0);
    model.replace(unedited);
    const auto localEdits = model.setNotePitchCurve("part0", {{.4,65},{.7,67}}, false)
        && model.setNotePitchCurve("part2", {{.3,66},{.4,66}}, false);
    roll.diagnosticRefresh(); roll.setShowPitchLine(false); const auto localHidden = picture();
    roll.setShowPitchLine(true); const auto local = picture();
    check("pitch_visibility_shared_local_strokes_do_not_fill_untouched_uv", localEdits
        && changedInk(local, localHidden, .76, .94) > 0
        && changedInk(local, localHidden, 1.36, 1.74) == 0
        && changedInk(local, localHidden, 1.84, 2.0) == 0);
    std::cout << "checks=" << checks << " passed=" << ok << std::endl;
    return ok;
}
}
