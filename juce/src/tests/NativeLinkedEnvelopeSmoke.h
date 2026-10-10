#pragma once
#include "../NativeSharedEnvelope.h"
#include "../NativeAudioClipboard.h"
namespace hachi
{
inline bool runNativeLinkedEnvelopeSmoke(const juce::File& folder)
{
    folder.createDirectory(); bool ok = true;
    const auto check = [&](const char* key, bool pass) { ok &= pass; std::cout << key << '=' << pass << std::endl; };
    const auto near = [](double a, double b) { return std::abs(a - b) < 2.e-4; };
    const auto file = folder.getChildFile("source.wav");
    juce::AudioBuffer<float> samples(1, 86400);
    for (int i = 0; i < samples.getNumSamples(); ++i)
        samples.setSample(0, i, .2f * static_cast<float>(std::sin(i * juce::MathConstants<double>::twoPi * 220 / 48000)));
    { auto out = file.createOutputStream(); if (!out) return false; out->setPosition(0); out->truncate();
      juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(out.release(),48000,1,24,{},0));
      check("linked_envelope_source_written", writer && writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples())); }
    ClipData clip; clip.id = "recording"; clip.startSeconds = 1; clip.durationSeconds = clip.sourceDurationSeconds = 1.8; clip.sourceFile = file;
    NoteData note; note.id = "whole"; note.label = "a"; note.durationSeconds = 1.8;
    note.midiNote = note.sourceMidiCenter = 57; note.sourcePitchMeasured = true;
    for (int i = 0; i <= 360; ++i) note.contour.push_back({i*.005,0,0,true});
    bindNativeNoteSource(note, clip); clip.notes = {note};
    TrackData track; track.id = "native"; track.pitchAlgorithm = PitchAlgorithm::world; track.normalizeVolume = false; track.clips = {clip};
    ProjectData data; data.tracks = {track}; ProjectModel model; model.resetDocument(data);
    const auto second = model.splitNote("whole", .6);
    const auto third = model.splitNote(second, .6);
    check("linked_envelope_three_split_parts", second.isNotEmpty() && third.isNotEmpty());
    const auto split = model.snapshot();
    const auto groups = nativeSharedEnvelopes(split.tracks[0]);
    check("linked_envelope_default_one_flat_curve", groups.size() == 3 && groups.at("whole").points == groups.at(third).points
        && groups.at("whole").points->size() == 2 && near(nativeEnvelopeDbAt(*groups.at(second).points,1.6),0));
    check("linked_envelope_default_request_unity", [&] { const auto r = AudioEngine::diagnosticNativeRequest(split.tracks[0].clips[0],split.tracks[0]);
        return std::all_of(r.noteGain.begin(), r.noteGain.end(), [](float g){return g == 1;}); }());
    I18n strings; PianoRollComponent roll(model,strings); roll.setSize(1200,700); roll.setPixelsPerSecond(300);
    roll.setFocusedTrack(track.id); roll.setFocusedClip(clip.id); roll.setTool(PianoRollComponent::Tool::amplitude); roll.diagnosticRefresh();
    const auto refresh = [&] {model.dispatchPendingMessages(); roll.diagnosticRefresh();};
    const auto event = [&](juce::Point<float> at, juce::Point<float> down, bool right=false)
    { return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
        juce::ModifierKeys(right ? juce::ModifierKeys::rightButtonModifier : juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,
        &roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at != down); };
    const auto y = roll.diagnosticAmplitudeLaneY(0);
    const juce::Point<float> seam(static_cast<float>(roll.diagnosticEdgeX(1.6)),y);
    roll.setSelectedNoteIds({second});
    roll.mouseDoubleClick(event(seam,seam)); refresh();
    check("linked_envelope_point_on_cut_retained_once", nativeSharedEnvelopes(model.snapshot().tracks[0]).at("whole").points->size() == 3);
    const auto fingerprint = model.contentFingerprint();
    const juce::Point<float> lowered(seam.x,roll.diagnosticAmplitudeLaneY(-6.0206f));
    roll.mouseDown(event(seam,seam)); roll.mouseDrag(event(lowered,seam));
    const auto previewA=roll.diagnosticDisplayEnvelope("whole"), previewB=roll.diagnosticDisplayEnvelope(second);
    check("linked_envelope_drag_preview_shared", near(PianoRollComponent::diagnosticAmplitudeDbAt(previewA,.6),-6.0206)
        && near(PianoRollComponent::diagnosticAmplitudeDbAt(previewB,0),-6.0206));
    check("linked_envelope_drag_is_draft", fingerprint == model.contentFingerprint());
    roll.mouseUp(event(lowered,seam)); refresh();
    const auto edited = model.snapshot(); const auto shared = nativeSharedEnvelopes(edited.tracks[0]);
    check("linked_envelope_commits_whole_group", near(nativeEnvelopeDbAt(*shared.at("whole").points,1.6),-6.0206)
        && shared.at("whole").points->size() == 3);
    const auto request = AudioEngine::diagnosticNativeRequest(edited.tracks[0].clips[0],edited.tracks[0]);
    bool matches=true; for (std::size_t i=0;i<request.noteGain.size();++i)
        matches &= near(request.noteGain[i],backend::envelopeGainFromDb(nativeEnvelopeDbAt(*shared.at("whole").points,1+std::min(1.8,i*.005))));
    check("linked_envelope_ui_and_render_match_all_frames", matches);
    check("linked_envelope_gain_keeps_source_pitch", request.sourceMidi == request.targetMidi);
    check("linked_envelope_one_undo_restores_group", model.undo() && model.contentFingerprint() == fingerprint);
    check("linked_envelope_redo", model.redo()); refresh();
    const auto beforeCross=model.contentFingerprint();
    const juce::Point<float> cross(static_cast<float>(roll.diagnosticEdgeX(2.05)),lowered.y);
    roll.mouseDown(event(lowered,lowered)); roll.mouseDrag(event(cross,lowered)); roll.mouseUp(event(cross,lowered)); refresh();
    const auto crossed = nativeSharedEnvelopes(model.snapshot().tracks[0]);
    check("linked_envelope_point_moves_across_cut", crossed.at("whole").points->size()==3
        && near(crossed.at("whole").points->at(1).timeSeconds,2.05));
    check("linked_envelope_cross_cut_undo", model.undo() && model.contentFingerprint()==beforeCross); refresh();
    const auto originalKey=AudioEngine::diagnosticUtauRenderKey(split,clip.id);
    check("linked_envelope_cache_invalidated", originalKey != AudioEngine::diagnosticUtauRenderKey(edited,clip.id));
    juce::String error; ProjectModel reopened; const auto saved=folder.getChildFile("linked-envelope.hjpx");
    check("linked_envelope_saved_and_reopened", model.save(saved,error) && reopened.load(saved,error));
    const auto reopenedCurve=nativeSharedEnvelopes(reopened.snapshot().tracks[0]);
    check("linked_envelope_seam_anchor_survives_reopen", reopenedCurve.at("whole").points->size()==3
        && near(nativeEnvelopeDbAt(*reopenedCurve.at("whole").points,1.6),-6.0206));
    check("linked_envelope_disconnect", model.disconnectNativeAudio({"whole",second,third}));
    const auto detached=model.snapshot(); check("linked_envelope_detached_independent",nativeSharedEnvelopes(detached.tracks[0]).empty());
    bool detachedMatches=true;
    for (const auto& c:detached.tracks[0].clips)
    { const auto r=AudioEngine::diagnosticNativeRequest(c,detached.tracks[0]);
      for(std::size_t i=0;i<r.noteGain.size();++i) detachedMatches &= near(r.noteGain[i],backend::envelopeGainFromDb(nativeEnvelopeDbAt(*shared.at("whole").points,c.startSeconds+std::min(c.durationSeconds,i*.005)))); }
    check("linked_envelope_disconnect_preserves_heard_curve",detachedMatches);
    check("linked_envelope_relink",model.linkNativeAudio({"whole",second,third}));
    const auto beforeBase=model.contentFingerprint();
    model.setNotesAmplitudeEnvelopeBase({second},150);
    const auto baseEdited=model.snapshot();
    check("linked_envelope_base_edits_whole_group",std::all_of(baseEdited.tracks[0].clips[0].notes.begin(),
        baseEdited.tracks[0].clips[0].notes.end(),[](const auto& n){return n.amplitudeEnvelopeBasePercent==150;}));
    check("linked_envelope_base_one_undo",model.undo()&&model.contentFingerprint()==beforeBase);
    auto linked=model.snapshot(); auto expanded=linked; expandProjectClipParts(expanded);
    const auto linkedCurve=nativeSharedEnvelopes(expanded.tracks[0]); bool sourcesMatch=true;
    std::vector<const ClipData*> phrase;
    for (const auto& c:expanded.tracks[0].clips)
    { phrase.push_back(&c); const auto r=AudioEngine::diagnosticNativeRequest(c,expanded.tracks[0]);
      for(std::size_t i=0;i<r.noteGain.size();++i) sourcesMatch &= near(r.noteGain[i],backend::envelopeGainFromDb(nativeEnvelopeDbAt(*linkedCurve.at("whole").points,c.startSeconds+std::min(c.durationSeconds,i*.005)))); }
    check("linked_envelope_each_source_reads_global_curve",sourcesMatch);
    const auto merged=AudioEngine::mergedRequestFor(phrase,expanded.tracks[0],{},{}); bool mergedMatches=true;
    for(std::size_t i=0;i<merged.noteGain.size();++i) mergedMatches &= near(merged.noteGain[i],backend::envelopeGainFromDb(nativeEnvelopeDbAt(*linkedCurve.at("whole").points,1+std::min(1.8,i*.005))));
    check("linked_envelope_phrase_reads_global_curve",mergedMatches);
    const auto copied=copyNativeAudioNotes(linked,{second});
    check("linked_envelope_copy_single_part_keeps_gain",copied.clips.size()==1 && near(backend::envelopeGainFromDb(copied.clips[0].notes[0].amplitudeEnvelope.front().gainDb),.5));
    const auto moveBefore = model.snapshot();
    check("linked_envelope_stretch_applies",model.resizeNativeNoteEdge("whole",-.3,true));
    const auto stretched=nativeSharedEnvelopes(model.snapshot().tracks[0]);
    check("linked_envelope_stretch_retains_continuity",stretched.size()==3 && stretched.at("whole").points==stretched.at(third).points);
    model.resetDocument(linked);refresh();
    {auto out=folder.getChildFile("shared-loudness.png").createOutputStream(); if(out){out->setPosition(0);out->truncate();}
     juce::PNGImageFormat png;check("linked_envelope_lane_snapshot",out&&png.writeImageToStream(roll.createComponentSnapshot(roll.getLocalBounds()),*out));}
    // Legacy separate note endpoints must not reintroduce silent cut edges.
    auto legacy=split;
    for(auto& n:legacy.tracks[0].clips[0].notes)n.amplitudeEnvelope={{0,-60,true},{n.durationSeconds,-60,true}};
    const auto legacyCurve=nativeSharedEnvelopes(legacy.tracks[0]);
    check("linked_envelope_legacy_internal_edges_removed",legacyCurve.at("whole").points->size()==2);
    auto utau=split.tracks[0];utau.pitchAlgorithm=PitchAlgorithm::utau;
    check("linked_envelope_utau_unaffected",nativeSharedEnvelopes(utau).empty());
    auto overlap=split.tracks[0];overlap.clips[0].notes[1].startSeconds=.4;overlap.clips[0].notes[2].startSeconds=.8;
    check("linked_envelope_overlapping_voices_independent",nativeSharedEnvelopes(overlap).empty());
    AudioEngine engine; engine.prepareToPlay(256,48000);
    WavExportOptions options; options.sampleRate=48000; options.channels=2; options.bitDepth=32;
    const auto render = [&](const ProjectData& d, const char* name)
    {
        engine.syncProject(d);
        for(int i=0;i<1500 && engine.renderProgress();++i) juce::Thread::sleep(10);
        return engine.hasCurrentRenderedAudio() && engine.exportWav(folder.getChildFile(name),error,track.id,1,2.8,options);
    };
    const auto exported=render(split,"original.wav") && render(edited,"shared-envelope.wav");
    check("linked_envelope_original_and_edited_exports",exported);
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(folder.getChildFile("original.wav"))),
        b(formats.createReaderFor(folder.getChildFile("shared-envelope.wav")));
    bool audioMatches=a&&b; double energy=0,difference=0;
    if(audioMatches)
    {
        juce::AudioBuffer<float> x(2,86400),yBuffer(2,86400); a->read(&x,0,86400,0,true,true); b->read(&yBuffer,0,86400,0,true,true);
        for(int i=480;i<85920;++i)
        {
            const auto expected=x.getSample(0,i)*backend::envelopeGainFromDb(nativeEnvelopeDbAt(*shared.at("whole").points,1+i/48000.));
            const auto e=expected-yBuffer.getSample(0,i); difference+=e*e; energy+=expected*expected;
        }
        audioMatches=energy>1&&difference/energy<2.e-4;
    }
    check("linked_envelope_export_audio_matches_shared_curve",audioMatches);
    engine.releaseResources();
    return ok;
}
}
