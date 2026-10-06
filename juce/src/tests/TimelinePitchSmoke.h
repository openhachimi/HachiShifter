#pragma once

namespace hachi
{
inline bool timelinePitchSmoke()
{
    auto ok = true;
    int checks = 0;
    const auto expect = [&](const char* name, bool passed)
    {
        ++checks;
        ok = ok && passed;
        std::cout << name << '=' << passed << '\n';
    };
    const auto point = [](double t, float midi)
    {
        PitchCurveEditPoint p { t, midi };
        p.shape = PitchCurveShape::smooth;
        return p;
    };
    const auto note = [&](const char* id, double start, double duration, float midi,
                          std::vector<PitchCurveEditPoint> points)
    {
        NoteData n;
        n.id = id; n.label = id; n.startSeconds = start; n.durationSeconds = duration;
        n.midiNote = midi; n.sourceMidiCenter = midi; n.utauAutoPitchTransition = false;
        n.pitchControlPoints = std::move(points);
        n.contour = { { 0.0, 0.0f, 0.0f, true }, { duration, 0.0f, 0.0f, true } };
        return n;
    };
    const auto project = [](std::vector<NoteData> notes)
    {
        ProjectData p;
        TrackData t; t.id = "track"; t.compose = true; t.pitchAlgorithm = PitchAlgorithm::utau;
        ClipData c; c.id = "clip"; c.durationSeconds = 12.0; c.notes = std::move(notes);
        t.clips.push_back(std::move(c)); p.tracks.push_back(std::move(t));
        return p;
    };
    const auto midiAt = [](const backend::UtauNoteRenderSpec& n, double local)
    {
        return n.midiNote + (n.timelinePitchCents ? n.timelinePitchCents(local) : 0.0f) / 100.0f;
    };
    auto p = project({ note("a", 2.0, .5, 60, { point(0, 60), point(.4, 60), point(.7, 64) }),
                       note("b", 2.5, .5, 65, {}) });
    auto sent = AudioEngine::diagnosticUtauRequestNotes(p, "clip");
    const auto expected = evaluatePitchCurve(p.tracks[0].clips[0].notes[0].pitchControlPoints, .6);
    expect("outgoing_curve_reaches_unedited_neighbour", sent.size() == 2
        && sent[1].timelinePitchCents && std::abs(midiAt(sent[1], .1) - expected) < .001f);
    expect("same_time_same_pitch", std::abs(midiAt(sent[0], .6) - midiAt(sent[1], .1)) < .001f);
    expect("stored_points_untouched", p.tracks[0].clips[0].notes[0].pitchControlPoints.size() == 3
        && p.tracks[0].clips[0].notes[1].pitchControlPoints.empty());
    auto three = p;
    three.tracks[0].clips[0].notes[0].pitchControlPoints.back().timeSeconds = 1.2;
    three.tracks[0].clips[0].notes.push_back(note("c", 3, .5, 67, {}));
    const auto threeSent = AudioEngine::diagnosticUtauRequestNotes(three, "clip");
    expect("curve_crosses_multiple_unedited_notes", threeSent[2].timelinePitchCents
        && std::abs(midiAt(threeSent[2], .1)
            - evaluatePitchCurve(three.tracks[0].clips[0].notes[0].pitchControlPoints, 1.1)) < .001f);
    {
        ProjectModel model; model.replace(p);
        I18n strings; PianoRollComponent roll(model, strings);
        roll.setBounds(0, 0, 1200, 600); roll.setFocusedTrack("track");
        roll.setTool(PianoRollComponent::Tool::points); roll.diagnosticRefresh();
        const auto drawn = roll.diagnosticPitchLineAt("a", 2.6);
        expect("display_and_render_share_evaluator", drawn && std::abs(*drawn - midiAt(sent[1], .1)) < .001f);
        const juce::Point<float> at(roll.diagnosticEdgeX(2.6), roll.diagnosticYForMidi(expected));
        const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(), at,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &roll, &roll,
            juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 2, false);
        roll.mouseDoubleClick(click);
        const auto edited = model.snapshot();
        expect("visible_cross_boundary_curve_accepts_point", edited.tracks[0].clips[0].notes[0].pitchControlPoints.size() == 4
            && edited.tracks[0].clips[0].notes[1].pitchControlPoints.empty());
        model.undo();
        expect("cross_boundary_edit_is_undoable", model.snapshot().tracks[0].clips[0].notes[0].pitchControlPoints.size() == 3);
    }
    auto touch = project({ note("a", 2, .5, 60, { point(0, 60), point(.5, 61) }),
                           note("b", 2.5, .5, 65, { point(0, 61), point(.5, 65) }) });
    auto touchSent = AudioEngine::diagnosticUtauRequestNotes(touch, "clip");
    expect("exact_boundary_shares_line", touchSent[0].timelinePitchCents && touchSent[1].timelinePitchCents
        && std::abs(midiAt(touchSent[0], .6) - midiAt(touchSent[1], .1)) < .001f);
    auto gap = p;
    gap.tracks[0].clips[0].notes[1].startSeconds = 2.55;
    expect("gap_is_not_bridged", !sharedPitchLines(gap.tracks[0]).memberFor("b"));
    auto overlap = p;
    overlap.tracks[0].clips[0].notes[1].startSeconds = 2.4;
    expect("independent_overlap_is_not_bridged", !sharedPitchLines(overlap.tracks[0]).memberFor("b"));
    auto rest = p;
    rest.tracks[0].clips[0].notes.insert(rest.tracks[0].clips[0].notes.begin() + 1,
                                     note("RR", 2.5, 0.0, 60, {}));
    expect("explicit_rest_breaks_line", !sharedPitchLines(rest.tracks[0]).memberFor("b"));

    auto longLine = project({ note("a", 2, 2, 58, { point(0, 58), point(2, 62) }),
                             note("b", 4, 1, 64, { point(-1.1, 60), point(.5, 64) }),
                             note("c", 5, 1, 66, { point(-.4, 64), point(.8, 66) }) });
    auto longSent = AudioEngine::diagnosticUtauRequestNotes(longLine, "clip");
    // Decode the exact string passed to the external engine, not a preview
    // approximation. Test samples beyond both old fixed padding limits.
    const auto verifyEncoding = [&](backend::UtauNoteRenderSpec n, bool overrideHead)
    {
        n.preutteranceOverrideEnabled = overrideHead;
        n.preutteranceSeconds = 1.7;
        const auto encoded = backend::UtauRenderer::diagnosticPitchbend(n, 137.0, 1.3, 2.8);
        const juce::String alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        auto good = n.timelinePitchCents && encoded.length() > 100;
        for (int i = 0; i + 1 < encoded.length(); i += 2)
        {
            auto value = alphabet.indexOfChar(encoded[i]) * 64 + alphabet.indexOfChar(encoded[i + 1]);
            if (value >= 2048) value -= 4096;
            auto time = (i / 2) * 60.0 / (137.0 * 96.0) - 1.3;
            if (overrideHead && time < 0) time *= 1.7 / 1.3;
            const auto cents = n.timelinePitchCents(time) + (n.midiNote - std::round(n.midiNote)) * 100.0f;
            good = good && std::abs(value - cents) <= .501f;
        }
        return good;
    };
    expect("actual_PIT_long_head_tail", verifyEncoding(longSent[1], false));
    expect("actual_PIT_preutterance_override", verifyEncoding(longSent[1], true));
    const auto line = sharedPitchLines(longLine.tracks[0]).memberFor("b")->line;
    expect("far_head_reads_timeline", std::abs(midiAt(longSent[1], -.9) - line->midiAt(3.1)) < .001f);
    expect("far_tail_reads_timeline", std::abs(midiAt(longSent[1], 1.4) - line->midiAt(5.4)) < .001f);
    auto moved = longLine;
    moved.tracks[0].clips[0].notes[1].pitchControlPoints[0].targetMidi -= 3;
    const auto nextSent = AudioEngine::diagnosticUtauRequestNotes(moved, "clip");
    expect("snapshot_stays_immutable", std::abs(midiAt(longSent[0], 1.0) - midiAt(nextSent[0], 1.0)) > .1f
        && std::abs(midiAt(longSent[0], 1.0) - line->midiAt(3.0)) < .001f);

    // Put the two notes in different clips. A selected clip still depends on
    // edits in the neighbour, without dragging that neighbour into selection.
    auto crossClip = p;
    ClipData second = crossClip.tracks[0].clips[0]; second.id = "other";
    second.notes = { second.notes[1] };
    crossClip.tracks[0].clips[0].notes.resize(1);
    crossClip.tracks[0].clips.push_back(second);
    auto onlyB = AudioEngine::diagnosticUtauRequestNotes(crossClip, "other");
    expect("single_clip_reads_neighbour", onlyB.size() == 1 && onlyB[0].timelinePitchCents
        && std::abs(midiAt(onlyB[0], .1) - expected) < .001f);
    const auto key = AudioEngine::diagnosticUtauRenderKey(crossClip, "other");
    crossClip.tracks[0].clips[0].notes[0].pitchControlPoints.back().targetMidi += 2;
    expect("neighbour_edit_invalidates_clip_cache", key != AudioEngine::diagnosticUtauRenderKey(crossClip, "other"));
    std::cout << "checks=" << checks << "|ok=" << ok << std::endl;
    return ok;
}
}
