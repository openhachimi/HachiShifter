#pragma once
#include "../ClipParts.h"
namespace hachi
{
inline bool runNativeLinkedPitchSmoke(const juce::File& folder)
{
    folder.createDirectory(); bool ok = true;
    const auto check = [&](const char* name, bool pass)
    { ok &= pass; std::cout << name << '=' << pass << std::endl; };
    const auto near = [](double a, double b) { return std::abs(a - b) < 1.0e-4; };
    ProjectData data; TrackData track; track.id = "linked-pitch";
    track.pitchAlgorithm = PitchAlgorithm::world; track.normalizeVolume = false;
    for (int i = 0; i < 3; ++i)
    {
        ClipData clip; clip.id = "region" + juce::String(i); clip.startSeconds = 1 + i * .6;
        clip.durationSeconds = clip.sourceDurationSeconds = .6;
        clip.sourceFile = folder.getChildFile("source" + juce::String(i) + ".wav");
        juce::AudioBuffer<float> samples(1, 86400);
        for (int sample = 0; sample < samples.getNumSamples(); ++sample)
            samples.setSample(0, sample, static_cast<float>(.15 * std::sin(sample * 2 * juce::MathConstants<double>::pi * 220 / 48000)));
        juce::WavAudioFormat wav; auto stream = clip.sourceFile.createOutputStream();
        if (!stream) return false;
        stream->setPosition(0); stream->truncate();
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 48000, 1, 24, {}, 0));
        if (!writer) return false; stream.release();
        check("native_linked_pitch_source_written", writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples()));
        writer.reset();
        NoteData note; note.id = "part" + juce::String(i); note.label = "a";
        note.durationSeconds = .6; note.midiNote = 60 + static_cast<float>(i);
        note.sourceMidiCenter = 57; note.sourcePitchMeasured = true;
        note.pitchControlPoints = {{0, note.midiNote}, {.25, note.midiNote + 1}, {.6, note.midiNote}};
        for (int frame = 0; frame <= 120; ++frame)
        {
            PitchPoint point; point.timeSeconds = frame * .005;
            point.hasManualTarget = true;
            point.manualTargetCents = (evaluatePitchCurve(note.pitchControlPoints, point.timeSeconds) - note.midiNote) * 100;
            note.contour.push_back(point);
        }
        clip.notes.push_back(note); track.clips.push_back(clip);
    }
    data.tracks.push_back(track); ProjectModel model; model.replace(data);
    check("native_pitch_link_three_recordings", model.linkNativeAudio({"part0", "part1", "part2"}));
    auto linked = model.snapshot(); const auto fingerprint = model.contentFingerprint();
    I18n strings; PianoRollComponent roll(model, strings);
    roll.setBounds(0, 0, 1100, 800); roll.setPixelsPerSecond(300);
    roll.setFocusedTrack(track.id); roll.setFocusedClip(linked.tracks[0].clips[0].id);
    roll.setTool(PianoRollComponent::Tool::points);
    roll.setShowNativeWaveforms(false); roll.setShowOriginalPitchLine(false); roll.diagnosticRefresh();
    const auto common = sharedPitchLines(linked.tracks[0]);
    check("native_pitch_one_curve_for_all_parts", common.memberFor("part0") && common.memberFor("part1")
        && common.memberFor("part2") && common.memberFor("part0")->line == common.memberFor("part2")->line);
    bool spans = true, seams = true;
    for (int i = 0; i < 2; ++i)
    {
        const auto left = roll.diagnosticPitchLineSpan("part" + juce::String(i));
        const auto right = roll.diagnosticPitchLineSpan("part" + juce::String(i + 1));
        const auto seam = 1 + (i + 1) * .6;
        spans &= left && right && near(left->second, right->first) && near(left->second, seam);
        const auto a = roll.diagnosticPitchLineAt("part" + juce::String(i), seam);
        const auto b = roll.diagnosticPitchLineAt("part" + juce::String(i + 1), seam);
        seams &= a && b && near(*a, *b);
    }
    check("native_pitch_no_independent_onset_gaps", spans);
    check("native_pitch_seams_have_one_value", seams);
    check("native_pitch_internal_tail_not_offered_twice", roll.diagnosticOfferedPitchAnchors("part0").size() == 2
        && roll.diagnosticOfferedPitchAnchors("part1").size() == 2
        && roll.diagnosticOfferedPitchAnchors("part2").size() == 3);
    auto expanded = linked; expandProjectClipParts(expanded);
    bool renderMatches = true;
    for (const auto& clip : expanded.tracks[0].clips)
    {
        const auto request = AudioEngine::diagnosticNativeRequest(clip, expanded.tracks[0]);
        for (std::size_t frame = 0; frame < request.targetMidi.size(); ++frame)
        {
            const auto time = clip.startSeconds + std::min(clip.durationSeconds, frame * .005);
            const auto shown = roll.diagnosticPitchLineAt(clip.notes[0].id, time);
            renderMatches &= shown && near(*shown, request.targetMidi[frame]);
        }
    }
    check("native_pitch_display_matches_each_source_render", renderMatches);
    auto phraseTrack = expanded.tracks[0];
    std::vector<const ClipData*> phrase;
    for (std::size_t i = 0; i < phraseTrack.clips.size(); ++i)
    {
        auto& clip = phraseTrack.clips[i];
        clip.sourceFile = phraseTrack.clips[0].sourceFile; clip.sourceOffsetSeconds = i * .6;
        phrase.push_back(&clip);
    }
    const auto merged = AudioEngine::mergedRequestFor(phrase, phraseTrack, {}, {});
    bool mergedMatches = true;
    for (std::size_t frame = 0; frame < merged.targetMidi.size(); ++frame)
        mergedMatches &= near(merged.targetMidi[frame], common.memberFor("part0")->line->midiAt(1 + std::min(1.8, frame * .005)));
    check("native_pitch_merged_decode_uses_same_curve", mergedMatches);
    auto independentMerge = linked; independentMerge.tracks[0].clips[0].nativeAudioLinked = false;
    check("native_pitch_ordinary_merge_keeps_sources_independent", sharedPitchLines(independentMerge.tracks[0]).byNote.empty());
    const auto firstKey = AudioEngine::diagnosticUtauRenderKey(expanded, expanded.tracks[0].clips[0].id);
    const auto firstWaveHash = AudioEngine::nativeClipWaveformHash(expanded.tracks[0].clips[0], expanded.tracks[0]);
    auto views = linked; expandProjectClipParts(views, true);
    check("native_pitch_waveform_view_and_render_hash_match", firstWaveHash
        == AudioEngine::nativeClipWaveformHash(views.tracks[0].clips[0], views.tracks[0]));
    check("native_pitch_display_does_not_edit_source_project", fingerprint == model.contentFingerprint());

    const auto event = [&](juce::Point<float> at, juce::Point<float> down)
    { return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0, 0, 0, 0,
        &roll, &roll, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, at != down); };
    const auto handle = juce::Point<float>(static_cast<float>(roll.diagnosticEdgeX(1.6)),
        roll.diagnosticYForMidi(61));
    const auto moved = handle.translated(0, -22);
    roll.mouseDown(event(handle, handle)); check("native_pitch_shared_joint_is_draggable", roll.diagnosticDraggingAnchor());
    roll.mouseDrag(event(moved, handle));
    const auto previewLeft = roll.diagnosticPitchLineAt("part0", 1.6);
    const auto previewRight = roll.diagnosticPitchLineAt("part1", 1.6);
    check("native_pitch_drag_preview_is_continuous", previewLeft && previewRight && near(*previewLeft, *previewRight)
        && !near(*previewRight, 61));
    roll.mouseUp(event(moved, handle)); model.dispatchPendingMessages(); roll.diagnosticRefresh();
    auto edited = model.snapshot(); expandProjectClipParts(edited);
    check("native_pitch_neighbor_render_cache_refreshes", firstKey != AudioEngine::diagnosticUtauRenderKey(edited, edited.tracks[0].clips[0].id));
    check("native_pitch_neighbor_waveform_cache_refreshes", firstWaveHash != AudioEngine::nativeClipWaveformHash(edited.tracks[0].clips[0], edited.tracks[0]));
    const auto committedLeft = roll.diagnosticPitchLineAt("part0", 1.6), committedRight = roll.diagnosticPitchLineAt("part1", 1.6);
    check("native_pitch_committed_joint_is_continuous", committedLeft && committedRight && near(*committedLeft, *committedRight));
    check("native_pitch_drag_undo_restores_whole_curve", model.undo() && model.contentFingerprint() == fingerprint);
    model.replace(linked); juce::String error; const auto file = folder.getChildFile("linked-pitch.hjpx"); ProjectModel reopened;
    check("native_pitch_saved_group_reopens", model.save(file, error) && reopened.load(file, error)
        && sharedPitchLines(reopened.snapshot().tracks[0]).memberFor("part1") != nullptr);
    check("native_pitch_disconnect_restores_independent_lines", model.disconnectNativeAudio({"part0", "part1", "part2"})
        && sharedPitchLines(model.snapshot().tracks[0]).byNote.empty());
    // Existing unedited recordings retain their exact source targets after
    // linking; display grouping alone must not introduce a synthetic glide.
    auto original = linked;
    for (auto& note : original.tracks[0].clips[0].notes)
    {
        note.midiNote = note.sourceMidiCenter = 57; note.pitchControlPoints.clear();
        for (auto& point : note.contour) { point.hasManualTarget = false; point.manualTargetCents = 0; }
    }
    const auto originalLines = sharedPitchLines(original.tracks[0]);
    auto raw = original; expandProjectClipParts(raw);
    bool preserved = true, mask = true;
    for (auto& clip : raw.tracks[0].clips)
    {
        const auto request = AudioEngine::diagnosticNativeRequest(clip, raw.tracks[0]);
        preserved &= originalLines.memberFor(clip.notes[0].id) && !originalLines.memberFor(clip.notes[0].id)->renderSharedCurve
            && request.preserveUneditedSource && request.sourceMidi == request.targetMidi;
        clip.notes[0].contour[50].voiced = false;
        const auto silent = AudioEngine::diagnosticNativeRequest(clip, raw.tracks[0]);
        mask &= silent.targetMidi[50] == 0;
    }
    check("native_pitch_unedited_source_targets_preserved", preserved);
    check("native_pitch_preserves_unvoiced_mask", mask);
    model.replace(original); model.dispatchPendingMessages(); roll.diagnosticRefresh();
    const auto automatic = roll.diagnosticPitchLineSpan("part1");
    check("native_pitch_derived_line_has_no_repeat_onset", automatic && near(automatic->first, 1.6) && near(automatic->second, 2.2));
    model.replace(linked); model.dispatchPendingMessages(); roll.diagnosticRefresh();
    for (const auto tool : {PianoRollComponent::Tool::note, PianoRollComponent::Tool::points})
    {
        roll.setTool(tool); auto output = folder.getChildFile(tool == PianoRollComponent::Tool::note ? "linked-note.png" : "linked-points.png").createOutputStream();
        if (output) { output->setPosition(0); output->truncate(); }
        const auto left = roll.diagnosticEdgeX(1.0) - 40;
        const auto top = roll.diagnosticYForMidi(65) - 40;
        const auto area = juce::Rectangle<int>(static_cast<int>(left), static_cast<int>(top),
            static_cast<int>(roll.diagnosticEdgeX(2.8) - left + 40),
            static_cast<int>(roll.diagnosticYForMidi(58) - top + 40));
        check("native_pitch_snapshot_contains_all_parts", roll.diagnosticNotes().size() == 3);
        check("native_pitch_preview_written", output && juce::PNGImageFormat().writeImageToStream(roll.createComponentSnapshot(area), *output));
    }
    model.replace(linked);
    model.flattenNotePitch({"part0", "part1", "part2"}, true);
    const auto flattened = model.snapshot();
    const auto& flatTrack = flattened.tracks[0];
    check("native_flatten_keeps_independent_bodies_and_gain", flatTrack.clips[0].notes[1].nativeIndependentPitch
        && nativeSharedEnvelopes(flatTrack).size() == 3
        && flatTrack.clips[0].nativeAudioLinked);
    check("native_flatten_independence_persists", model.save(file, error) && reopened.load(file, error)
        && reopened.snapshot().tracks[0].clips[0].notes[1].nativeIndependentPitch);
    model.dispatchPendingMessages(); roll.diagnosticRefresh();
    const auto boundaryLines = sharedPitchLines(flatTrack);
    bool handles = true, connected = true, bodyFlat = true, renderBridge = true;
    auto expandedFlat = flattened; expandProjectClipParts(expandedFlat);
    for (int i = 0; i < 2; ++i)
    {
        const auto leftId = "part" + juce::String(i), rightId = "part" + juce::String(i+1);
        const auto leftHandles = roll.diagnosticOfferedPitchAnchors(leftId);
        const auto rightHandles = roll.diagnosticOfferedPitchAnchors(rightId);
        const auto seam = 1.6 + .6*i;
        handles &= !leftHandles.empty() && !rightHandles.empty()
            && near(.6 + rightHandles.front().timeSeconds-leftHandles.back().timeSeconds, .001);
        const auto* left = boundaryLines.memberFor(leftId); const auto* right = boundaryLines.memberFor(rightId);
        connected &= left && right && left->line == right->line
            && near(left->line->midiAt(seam), 60.5+i)
            && near(left->line->midiAt(seam-.0005), 60+i)
            && near(right->line->midiAt(seam+.0005), 61+i);
        bodyFlat &= left && right && near(left->line->midiAt(seam-.01),60+i)
            && near(right->line->midiAt(seam+.01),61+i);
        const auto& clip = expandedFlat.tracks[0].clips[i];
        const auto request = AudioEngine::diagnosticNativeRequest(clip, expandedFlat.tracks[0]);
        renderBridge &= near(request.targetMidi.back(), 60.5+i);
    }
    check("native_boundary_handles_one_millisecond_apart", handles);
    check("native_boundary_bridge_continuous_linear", connected);
    check("native_boundary_body_not_pulled", bodyFlat);
    check("native_boundary_render_matches_display", renderBridge);
    auto separated = expandedFlat;
    separated.tracks[0].clips[1].startSeconds += .01;
    check("native_boundary_does_not_bridge_gap_or_overlap", sharedPitchLines(separated.tracks[0]).byNote.empty());
    model.setNotePitchCurve("part1", {{0, 66.f}, {.3, 69.f}, {.6, 64.f}}, true);
    model.dispatchPendingMessages(); roll.diagnosticRefresh();
    bool neighboursFlat = true;
    for (const auto& n : flatTrack.clips[0].notes)
        if (n.id != "part1")
            for (double u : {.01, .5, .99})
            {
                const auto shown = roll.diagnosticPitchLineAt(n.id,
                    flatTrack.clips[0].startSeconds + n.startSeconds + n.durationSeconds * u);
                neighboursFlat &= shown && near(*shown, n.midiNote);
            }
    check("native_flatten_later_edit_does_not_pull_neighbours", neighboursFlat);
    const auto pic = roll.createComponentSnapshot({0, 600, 1100, 650});
    auto picture = folder.getChildFile("independent-pitch.png").createOutputStream();
    if (picture) { picture->setPosition(0); picture->truncate(); }
    check("native_flatten_independent_preview_written", picture && juce::PNGImageFormat().writeImageToStream(pic, *picture));
    // Endpoints are ordinary placed handles, not audio-boundary markers.
    model.replace(flattened); model.dispatchPendingMessages(); roll.diagnosticRefresh();
    roll.setTool(PianoRollComponent::Tool::points);
    const auto dragEndpoint = [&](const juce::String& id, bool head, double local) {
        model.dispatchPendingMessages(); roll.diagnosticRefresh();
        const auto beforeHandles=roll.diagnosticOfferedPitchAnchors(id);
        if(beforeHandles.empty()) return false;
        auto before=model.snapshot();
        const auto& notes=before.tracks[0].clips[0].notes;
        const auto n=std::find_if(notes.begin(),notes.end(),[&](const auto& x){return x.id==id;});
        if(n==notes.end()) return false;
        const auto absoluteStart=before.tracks[0].clips[0].startSeconds+n->startSeconds;
        const auto p=head ? beforeHandles.front() : beforeHandles.back();
        const auto at=juce::Point<float>(float(roll.diagnosticEdgeX(absoluteStart+p.timeSeconds)),
            roll.diagnosticYForMidi(p.targetMidi));
        const auto to=juce::Point<float>(float(roll.diagnosticEdgeX(absoluteStart+local)),at.y);
        const auto beforeFingerprint=model.contentFingerprint();
        roll.mouseDown(event(at,at));const auto grabbed=roll.diagnosticDraggingAnchor();
        roll.mouseDrag(event(to,at));
        const auto preview=roll.diagnosticPitchLineAt(id,absoluteStart+local);
        roll.mouseUp(event(to,at));model.dispatchPendingMessages();roll.diagnosticRefresh();
        const auto afterHandles=roll.diagnosticOfferedPitchAnchors(id);
        const auto committed=roll.diagnosticPitchLineAt(id,absoluteStart+local);
        if(afterHandles.empty()) return false;
        const auto movedPoint=head ? afterHandles.front() : afterHandles.back();
        const auto placed=model.snapshot().tracks[0].clips[0].notes[std::size_t(n-notes.begin())];
        const auto afterFingerprint=model.contentFingerprint();
        const auto undo=model.undo() && model.contentFingerprint()==beforeFingerprint;
        const auto redo=model.redo() && model.contentFingerprint()==afterFingerprint;
        model.dispatchPendingMessages();roll.diagnosticRefresh();
        return grabbed && near(movedPoint.timeSeconds,local)
            && afterHandles.size()==beforeHandles.size() && placed.pitchControlPoints.size()==beforeHandles.size()
            && placed.nativePitchHandlesPlaced && near(placed.durationSeconds,n->durationSeconds)
            && near(placed.nativeSourceStartSeconds,n->nativeSourceStartSeconds)
            && near(placed.nativeSourceEndSeconds,n->nativeSourceEndSeconds)
            && preview && committed && near(*preview,*committed) && undo && redo;
    };
    check("native_head_moves_right_without_new_point",dragEndpoint("part1",true,.12));
    check("native_tail_moves_left_without_new_point",dragEndpoint("part0",false,.44));
    const auto placedLines=sharedPitchLines(model.snapshot().tracks[0]);
    const auto* placedMember=placedLines.memberFor("part0");
    check("native_bridge_follows_placed_endpoints",placedMember
        && near(placedMember->line->midiAt(1.58),60.5));
    auto placedProject=model.snapshot();expandProjectClipParts(placedProject);
    const auto placedRequest=AudioEngine::diagnosticNativeRequest(placedProject.tracks[0].clips[0],placedProject.tracks[0]);
    check("native_decoder_reads_interpolated_line",near(placedRequest.targetMidi[116],60.5));
    check("native_head_moves_left_without_new_point",dragEndpoint("part1",true,.03));
    check("native_tail_moves_right_without_new_point",dragEndpoint("part0",false,.55));
    check("native_outer_head_can_cross_audio_start",dragEndpoint("part0",true,-.1));
    check("native_outer_tail_can_cross_audio_end",dragEndpoint("part2",false,.7));
    check("native_placed_endpoints_save_reload",model.save(file,error) && reopened.load(file,error)
        && reopened.snapshot().tracks[0].clips[0].notes[0].nativePitchHandlesPlaced);
    const auto savedFingerprint=reopened.contentFingerprint();
    model.replace(reopened.snapshot());model.dispatchPendingMessages();roll.diagnosticRefresh();
    check("native_reloaded_handles_do_not_regenerate",roll.diagnosticOfferedPitchAnchors("part0").size()==2
        && near(roll.diagnosticOfferedPitchAnchors("part0").front().timeSeconds,-.1)
        && model.contentFingerprint()==savedFingerprint);
    // Joined rendering must not carry a vowel into a following analysed UV gap.
    auto uvTrack=expanded.tracks[0];
    for(auto& clip:uvTrack.clips) for(auto& note:clip.notes)
        for(auto& point:note.contour)
            if(point.timeSeconds>=.2 && point.timeSeconds<=.4) point.voiced=false;
    std::vector<const ClipData*> uvClips;
    for(const auto& clip:uvTrack.clips) uvClips.push_back(&clip);
    const auto uvRequest=AudioEngine::mergedRequestFor(uvClips,uvTrack,{},{});
    bool uvKept=true, vowelsKept=true;
    for(std::size_t i=0;i<uvTrack.clips.size();++i) {
        const auto at=std::size_t(std::llround((i*.6+.3)/.005));
        const auto vowel=std::size_t(std::llround((i*.6+.1)/.005));
        uvKept &= at<uvRequest.targetMidi.size() && uvRequest.targetMidi[at]==0 && uvRequest.sourceMidi[at]==0;
        vowelsKept &= vowel<uvRequest.targetMidi.size() && uvRequest.targetMidi[vowel]>0;
    }
    check("joined_phrase_never_fills_unvoiced_gaps",uvKept);
    check("joined_phrase_keeps_vowel_targets",vowelsKept);
    return ok;
}
}
