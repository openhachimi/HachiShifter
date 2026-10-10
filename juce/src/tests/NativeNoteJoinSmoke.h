#pragma once
#include "../NativeNoteJoin.h"
namespace hachi
{
inline bool runNativeNoteJoinSmoke(const juce::File& folder)
{
    folder.createDirectory();bool ok=true;int checks=0;
    const auto check=[&](const char* name,bool pass){ok &= pass;++checks;std::cout<<name<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-5;};
    const auto file=folder.getChildFile("original.wav");juce::WavAudioFormat wav;auto out=file.createOutputStream();
    if(out){out->setPosition(0);out->truncate();}std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(out.get(),48000,1,24,{},0));
    if(!writer)return false;out.release();juce::AudioBuffer<float> samples(1,96000);
    for(int i=0;i<96000;++i)samples.setSample(0,i,static_cast<float>(.2*std::sin(i*juce::MathConstants<double>::twoPi*220/48000)));
    check("source_written",writer->writeFromAudioSampleBuffer(samples,0,96000));writer.reset();
    ClipData clip;clip.id="audio";clip.startSeconds=1;clip.durationSeconds=clip.sourceDurationSeconds=2;clip.sourceFile=file;
    NoteData note;note.id="whole";note.label="baip";note.durationSeconds=2;note.midiNote=note.sourceMidiCenter=57;
    note.sourcePitchMeasured=true;note.utauAutoPitchTransition=false;
    for(int i=0;i<=400;++i)note.contour.push_back({i*.005,static_cast<float>(20*std::sin(i*.02)),static_cast<float>(20*std::sin(i*.02)),true});
    // The generated fixture is deliberately flat acoustic F0 for a true PCM no-op.
    for(auto& p:note.contour)p.relativeCents=p.withoutVibratoCents=0;
    bindNativeNoteSource(note,clip);clip.notes={note};TrackData track;track.id="native";track.pitchAlgorithm=PitchAlgorithm::world;track.normalizeVolume=false;track.clips={clip};
    ProjectData initial;initial.tracks={track};ProjectModel model;model.replace(initial);
    const auto middle=model.splitNote("whole",.5),last=model.splitNote(middle,1);const auto split=model.snapshot();
    check("three_parts_created",last.isNotEmpty()&&split.tracks[0].clips[0].notes.size()==3);
    check("adjacent_split_parts_can_join",model.canJoinNativeNotes("whole",middle));
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setFocusedClip(clip.id);
    roll.setSize(1300,800);roll.setPixelsPerSecond(320);roll.setRowHeight(24);roll.setTool(PianoRollComponent::Tool::connect);roll.diagnosticRefresh();
    const auto rect=roll.diagnosticHitBounds(0);juce::Point<float> seam(rect.getRight(),rect.getCentreY());
    const auto event=[&](juce::Point<float> at,int clicks){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,
        juce::ModifierKeys::leftButtonModifier,0,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),at,juce::Time::getCurrentTime(),clicks,false);};
    const auto fingerprint=model.contentFingerprint();
    roll.mouseDown(event(seam,1));roll.mouseUp(event(seam,1));
    check("first_seam_click_does_not_toggle_connections",model.contentFingerprint()==fingerprint);
    roll.mouseDown(event(seam,2));roll.mouseDoubleClick(event(seam,2));roll.mouseUp(event(seam,2));model.dispatchPendingMessages();roll.diagnosticRefresh();
    auto joined=model.snapshot();const auto& notes=joined.tracks[0].clips[0].notes;
    check("double_click_removes_only_one_seam",notes.size()==2&&notes[0].id=="whole"&&notes[1].id==last);
    check("total_duration_and_timeline_unchanged",near(joined.tracks[0].clips[0].startSeconds,1)&&near(joined.tracks[0].clips[0].durationSeconds,2)
        &&near(notes[0].durationSeconds,1.5)&&near(notes[1].startSeconds,1.5));
    check("merged_note_keeps_lyric_and_source_pitch",notes[0].label=="baip"&&nativePitchIsUnedited(notes[0])&&near(notes[0].nativeSourceEndSeconds,1.5));
    model.undo();check("single_undo_restores_two_independent_sections",model.snapshot().tracks[0].clips[0].notes.size()==3&&model.contentFingerprint()==fingerprint);
    model.redo();check("redo_restores_join",model.snapshot().tracks[0].clips[0].notes.size()==2);
    check("unified_edge_stretch_succeeds",model.resizeNativeNoteEdge("whole",.3,false));
    auto stretched=model.snapshot().tracks[0].clips[0];
    check("unified_stretch_moves_next_part_without_compressing_it",near(stretched.notes[0].durationSeconds,1.8)&&near(stretched.notes[1].startSeconds,1.8)&&near(stretched.notes[1].durationSeconds,.5));
    check("old_split_no_longer_has_independent_timing",!model.resizeNativeNoteEdge(middle,.2,false));
    check("internal_source_anchor_scales_with_whole_span",near(nativeSourceTimeAt(nativeClipClock(stretched),.6),.5));
    model.replace(joined);juce::String error;ProjectModel reopen;
    check("joined_note_save_reopen",model.save(folder.getChildFile("joined.hjpx"),error)&&reopen.load(folder.getChildFile("joined.hjpx"),error)&&reopen.snapshot().tracks[0].clips[0].notes.size()==2);
    // Tuned pitch and loudness must survive; generic mergeNotes resets both.
    auto edited=split;auto& c=edited.tracks[0].clips[0];c.notes[0].pitchControlPoints={{0,58},{.5,59}};
    c.notes[1].pitchControlPoints={{0,59},{.45,61},{1,60}};c.notes[2].pitchControlPoints={{0,60},{.5,58}};
    c.notes[0].amplitudeEnvelope={{0,-12,true},{.5,-4,true}};c.notes[1].amplitudeEnvelope={{0,-4,true},{1,-6,true}};
    c.notes[2].amplitudeEnvelope={{0,-6,true},{.5,-10,true}};
    for(auto& n:c.notes)for(auto& p:n.contour){p.hasManualTarget=true;p.manualTargetCents=(evaluatePitchCurve(n.pitchControlPoints,p.timeSeconds)-n.midiNote)*100;}
    const auto beforePitch=sharedPitchLines(edited.tracks[0]);const auto beforeGain=nativeSharedEnvelopes(edited.tracks[0]);model.replace(edited);
    check("edited_split_join_succeeds",model.joinNativeNotes("whole",middle)=="whole");auto tuned=model.snapshot();
    const auto afterPitch=sharedPitchLines(tuned.tracks[0]);const auto afterGain=nativeSharedEnvelopes(tuned.tracks[0]);bool pitchStable=true,gainStable=true;
    for(double t=1;t<3;t+=.007){pitchStable &= near(beforePitch.memberFor("whole")->line->midiAt(t),afterPitch.memberFor("whole")->line->midiAt(t));
        gainStable &= near(nativeEnvelopeDbAt(*beforeGain.at("whole").points,t),nativeEnvelopeDbAt(*afterGain.at("whole").points,t));}
    check("shared_pitch_survives_join_including_other_neighbor",pitchStable);check("shared_loudness_survives_join",gainStable);
    const auto oldRequest=AudioEngine::diagnosticNativeRequest(edited.tracks[0].clips[0],edited.tracks[0]);
    const auto newRequest=AudioEngine::diagnosticNativeRequest(tuned.tracks[0].clips[0],tuned.tracks[0]);bool framesMatch=oldRequest.targetMidi.size()==newRequest.targetMidi.size();
    for(std::size_t i=0;framesMatch&&i<oldRequest.targetMidi.size();++i)
        framesMatch &= near(oldRequest.targetMidi[i],newRequest.targetMidi[i])&&near(oldRequest.noteGain[i],newRequest.noteGain[i]);
    check("edited_render_pitch_and_gain_frames_unchanged",framesMatch);
    // The merged note retains group handles in its unselected neighbour.
    // A long edge stretch must warp those handles once on the common clock.
    for(const bool leftEdge:{false,true})
    {
        model.replace(tuned);const double delta=leftEdge?-.6:4.5;
        check(leftEdge?"joined_long_left_stretch":"joined_long_right_stretch",model.resizeNativeNoteEdge("whole",delta,leftEdge));
        const auto longData=model.snapshot();const auto& longClip=longData.tracks[0].clips[0];
        const auto longPitch=sharedPitchLines(longData.tracks[0]);const auto longGain=nativeSharedEnvelopes(longData.tracks[0]);
        const auto target=[&](double old){return leftEdge?(old<=2.5?1.+delta+(old-1.)*(1.5-delta)/1.5:old)
            :(old<=2.5?1.+(old-1.)*4:old+delta);};
        bool handles=true,sourceClock=true,loudness=true;
        for(const auto& piece:afterPitch.memberFor("whole")->line->pieces)for(const auto& p:piece.points)
            handles &= std::abs(longPitch.memberFor("whole")->line->midiAt(target(p.timeSeconds))-p.targetMidi)<.001;
        for(double t=1;t<3;t+=.007)
        {
            sourceClock &= near(nativeSourceTimeAt(nativeClipClock(longClip),target(t)-longClip.startSeconds),t-1);
            loudness &= std::abs(nativeEnvelopeDbAt(*longGain.at("whole").points,target(t))-nativeEnvelopeDbAt(*afterGain.at("whole").points,t))<.001;
        }
        check(leftEdge?"long_left_shared_pitch_handles_follow_audio":"long_right_shared_pitch_handles_follow_audio",handles);
        check(leftEdge?"long_left_source_mapping_is_monotonic":"long_right_source_mapping_is_monotonic",sourceClock);
        check(leftEdge?"long_left_shared_loudness_follows_audio":"long_right_shared_loudness_follows_audio",loudness);
        check(leftEdge?"long_left_neighbor_stays_rigid":"long_right_neighbor_stays_rigid",near(longClip.notes[1].durationSeconds,.5)
            &&near(longClip.startSeconds+longClip.notes[1].startSeconds,target(2.5)));
        model.undo();check(leftEdge?"long_left_undo_restores_curves":"long_right_undo_restores_curves",
            model.contentFingerprint()==[&]{ProjectModel original;original.replace(tuned);return original.contentFingerprint();}());
    }
    auto simpleFade=split;
    for(auto& n:simpleFade.tracks[0].clips[0].notes)n.amplitudeEnvelope={{0,-12,true},{n.durationSeconds,-3,true}};
    const auto simpleBefore=nativeSharedEnvelopes(simpleFade.tracks[0]);model.replace(simpleFade);
    check("split_long_stretch_without_seam_loudness_handles",model.resizeNativeNoteEdge("whole",4.5,false));
    const auto simpleAfter=nativeSharedEnvelopes(model.snapshot().tracks[0]);bool fadeAligned=true;
    for(double t=1;t<3;t+=.007){const auto moved=t<=1.5?1+(t-1)*10:t+4.5;
        fadeAligned &= std::abs(nativeEnvelopeDbAt(*simpleBefore.at("whole").points,t)-nativeEnvelopeDbAt(*simpleAfter.at("whole").points,moved))<.001;}
    check("split_long_stretch_preserves_shared_fade_on_source_clock",fadeAligned);
    model.replace(initial);const auto vb=model.splitNote("whole",.5);auto expressive=model.snapshot();
    auto& expressiveNotes=expressive.tracks[0].clips[0].notes;
    for(auto& n:expressiveNotes){n.vibratoEnabled=true;n.vibratoLengthPercent=100;n.vibratoDepthCents=20;n.vibratoPhasePercent=13;
        n.nativeEnvelope.mode=1;n.nativeEnvelope.shape.mixed=false;n.nativeEnvelope.shape.startFraction=0;n.nativeEnvelope.shape.endFraction=1;
        n.nativeEnvelope.shape.startGain=1;n.nativeEnvelope.shape.endGain=.4f;}
    expressiveNotes[1].gain=.7f;model.replace(expressive);
    const auto beforeExpressive=AudioEngine::diagnosticNativeRequest(expressive.tracks[0].clips[0],expressive.tracks[0]);
    check("vibrato_and_shaped_gain_join",model.joinNativeNotes("whole",vb)=="whole");const auto expressiveJoined=model.snapshot();
    const auto afterExpressive=AudioEngine::diagnosticNativeRequest(expressiveJoined.tracks[0].clips[0],expressiveJoined.tracks[0]);
    bool expressivePitch=true,expressiveGain=true;
    for(std::size_t i=0;i<beforeExpressive.targetMidi.size();++i){expressivePitch &= std::abs(beforeExpressive.targetMidi[i]-afterExpressive.targetMidi[i])<.001f;
        expressiveGain &= std::abs(beforeExpressive.noteGain[i]-afterExpressive.noteGain[i])<.0001f;}
    check("vibrato_render_frames_preserved",expressivePitch);check("advanced_loudness_and_note_gain_preserved",expressiveGain);
    // Linked source children of the same recording must collapse too.
    model.replace(split);check("disconnect_fixture",model.disconnectNativeAudio({"whole",middle,last}));
    check("relink_fixture",model.linkNativeAudio({"whole",middle,last}));
    check("linked_children_seam_can_join",model.joinNativeNotes("whole",middle)=="whole");
    auto linked=model.snapshot();check("linked_children_preserve_all_notes_and_sources",linked.tracks[0].clips[0].notes.size()==2&&linked.tracks[0].clips[0].parts.size()==2);
    check("linked_child_join_still_allows_unified_stretch",model.resizeNativeNoteEdge("whole",.2,false)&&near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,1.7));
    auto gap=split;gap.tracks[0].clips[0].notes[1].startSeconds+=.1;model.replace(gap);check("gap_is_not_a_removable_seam",!model.canJoinNativeNotes("whole",middle));
    auto u=split;u.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;model.replace(u);check("utau_seam_behavior_unchanged",!model.canJoinNativeNotes("whole",middle));
    model.replace(split);model.disconnectNativeAudio({"whole",middle,last});check("disconnected_notes_do_not_join_implicitly",!model.canJoinNativeNotes("whole",middle));
    // Real audio output must be unchanged by removing a split boundary.
    AudioEngine engine;engine.prepareToPlay(256,48000);WavExportOptions options;options.sampleRate=48000;options.channels=2;options.bitDepth=32;
    const auto render=[&](const ProjectData& d,const char* name){engine.syncProject(d);for(int i=0;i<1500&&engine.renderProgress();++i)juce::Thread::sleep(10);
        return engine.hasCurrentRenderedAudio()&&engine.exportWav(folder.getChildFile(name),error,track.id,1,3,options);};
    check("before_and_after_pcm_render",render(split,"before.wav")&&render(joined,"after.wav"));
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(folder.getChildFile("before.wav"))),b(formats.createReaderFor(folder.getChildFile("after.wav")));
    bool matches=a&&b;double difference=0,energy=0;if(matches){juce::AudioBuffer<float> x(2,96000),y(2,96000);a->read(&x,0,96000,0,true,true);b->read(&y,0,96000,0,true,true);
        for(int i=480;i<95520;++i){const auto v=x.getSample(0,i),d=v-y.getSample(0,i);difference+=d*d;energy+=v*v;}matches=energy>1&&difference/energy<1.e-5;}
    check("joining_does_not_change_source_audio",matches);engine.releaseResources();
    std::cout<<"render_relative_error="<<difference/std::max(energy,1.e-12)<<std::endl;std::cout<<"native_note_join_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
