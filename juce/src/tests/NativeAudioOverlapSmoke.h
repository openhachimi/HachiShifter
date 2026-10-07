#pragma once
#include "../NativeAudioOverlap.h"
#include "../NativeAudioClipboard.h"
namespace hachi
{
inline bool runNativeAudioOverlapSmoke(const juce::File& folder)
{
    folder.createDirectory();bool ok=true;int checks=0;
    const auto check=[&](const char* key,bool pass){ok &= pass;++checks;std::cout<<key<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-5;};
    juce::WavAudioFormat wav;
    const auto write=[&](const char* name,double hz){const auto file=folder.getChildFile(name);auto stream=file.createOutputStream();
        if(stream){stream->setPosition(0);stream->truncate();}
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),48000,1,24,{},0));if(!writer)return juce::File{};stream.release();
        juce::AudioBuffer<float> buffer(1,96000);for(int i=0;i<96000;++i)buffer.setSample(0,i,static_cast<float>(.18*(.8+.2*std::sin(i*.0001))*std::sin(i*2*juce::MathConstants<double>::pi*hz/48000)));
        if(!writer->writeFromAudioSampleBuffer(buffer,0,96000))return juce::File{};return file;};
    const auto firstFile=write("earlier.wav",220),secondFile=write("later.wav",330);check("two_sources_written",firstFile.existsAsFile()&&secondFile.existsAsFile());
    const auto clip=[&](const char* id,double start,const juce::File& source){ClipData c;c.id=id;c.startSeconds=start;c.durationSeconds=c.sourceDurationSeconds=1;c.sourceFile=source;
        NoteData n;n.id=juce::String(id)+"-note";n.label=id;n.durationSeconds=1;n.midiNote=n.sourceMidiCenter=57;n.sourcePitchMeasured=true;n.utauAutoPitchTransition=false;
        for(int i=0;i<=200;++i)n.contour.push_back({i*.005,0,0,true});bindNativeNoteSource(n,c);c.notes={n};return c;};
    TrackData track;track.id="native";track.name="Overlap test";track.pitchAlgorithm=PitchAlgorithm::world;track.normalizeVolume=false;
    track.clips={clip("A",1,firstFile),clip("B",2.4,secondFile)};ProjectData initial;initial.tracks={track};ProjectModel model;model.replace(initial);
    const auto get=[](const ProjectData& d,const char* id){for(const auto& t:d.tracks)for(const auto& c:t.clips)if(c.id==id)return c;return ClipData{};};
    const auto overlaps=[](const TrackData& t){const auto w=nativeAudioWindows(t);for(std::size_t i=0;i<w.size();++i)for(std::size_t j=i+1;j<w.size();++j)
        if(std::min(w[i].end,w[j].end)-std::max(w[i].start,w[j].start)>1.e-7)return true;return false;};
    check("default_prohibits_overlap",!model.snapshot().tracks[0].allowNativeAudioOverlap);
    check("note_move_stops_at_neighbor",model.moveNativeNotes({"B-note"},-1,0)&&near(get(model.snapshot(),"B").startSeconds,2)&&!overlaps(model.snapshot().tracks[0]));
    check("neighbor_stays_same_length_and_position",near(get(model.snapshot(),"A").startSeconds,1)&&near(get(model.snapshot(),"A").durationSeconds,1));
    model.replace(initial);model.moveNativeNotes({"B-note"},-3,0);check("cannot_jump_through_an_obstacle",near(get(model.snapshot(),"B").startSeconds,2));
    model.replace(initial);check("left_edge_stretch_stops_at_neighbor",model.resizeNativeNoteEdge("B-note",-.8,true)&&near(get(model.snapshot(),"B").startSeconds,2)
        &&near(get(model.snapshot(),"B").durationSeconds,1.4)&&!overlaps(model.snapshot().tracks[0]));
    model.replace(initial);check("right_edge_stretch_stops_at_neighbor",model.resizeNativeNoteEdge("A-note",2,false)&&near(get(model.snapshot(),"A").durationSeconds,1.4)&&!overlaps(model.snapshot().tracks[0]));
    model.replace(initial);model.moveClip("B",.2);check("timeline_region_move_cannot_pass_neighbor",near(get(model.snapshot(),"B").startSeconds,2));
    model.replace(initial);model.resizeClip("A",1,4);check("whole_clip_stretch_is_constrained",near(get(model.snapshot(),"A").durationSeconds,1.4)&&!overlaps(model.snapshot().tracks[0]));
    auto three=initial;three.tracks[0].clips.push_back(clip("C",4,firstFile));model.replace(three);model.moveNativeNotes({"A-note","B-note"},2,0);
    check("selected_group_keeps_spacing_and_stops_at_third",near(get(model.snapshot(),"A").startSeconds,1.6)&&near(get(model.snapshot(),"B").startSeconds,3)
        &&near(get(model.snapshot(),"C").startSeconds,4)&&!overlaps(model.snapshot().tracks[0]));
    auto cross=initial;auto target=track;target.id="target";target.clips={clip("C",2.4,firstFile)};cross.tracks.push_back(target);model.replace(cross);model.moveClips({"B"},0,1);
    check("cross_track_collision_keeps_source_on_original_row",model.snapshot().tracks[0].clips.size()==2&&model.snapshot().tracks[1].clips.size()==1);
    cross.tracks[1].clips.clear();model.replace(cross);model.moveClips({"B"},0,1);check("cross_track_move_to_free_space_works",model.snapshot().tracks[0].clips.size()==1&&model.snapshot().tracks[1].clips.size()==1);
    model.replace(initial);check("toggle_enables_overlap",model.setNativeAudioOverlap("native",true));
    check("enabled_note_drag_allows_overlap",model.moveNativeNotes({"B-note"},-.8,0)&&near(get(model.snapshot(),"B").startSeconds,1.6)&&overlaps(model.snapshot().tracks[0]));
    const auto overlapped=model.snapshot();juce::String error;const auto saved=folder.getChildFile("overlap-enabled.hjpx");ProjectModel reopened;
    check("overlap_option_and_positions_persist",model.save(saved,error)&&reopened.load(saved,error)&&reopened.snapshot().tracks[0].allowNativeAudioOverlap&&near(get(reopened.snapshot(),"B").startSeconds,1.6));
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack("native");roll.setFocusedClip("A");roll.setPixelsPerSecond(320);roll.setRowHeight(24);roll.setSize(1280,roll.getHeight());roll.diagnosticRefresh();
    const auto image=[&](const char* name){const auto pic=roll.createComponentSnapshot({0,static_cast<int>(roll.diagnosticYForMidi(57))-145,1280,320});
        juce::PNGImageFormat png;auto stream=folder.getChildFile(name).createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}
        return stream&&png.writeImageToStream(pic,*stream);};
    juce::Thread::sleep(200);check("overlap_preview_image_written",image("overlap-enabled.png"));check("overlap_has_two_source_waveform_card",roll.diagnosticOverlapCardCount()>=2);
    check("closing_overlap_crops_later_head",model.setNativeAudioOverlap("native",false));const auto cropped=model.snapshot();const auto a=get(cropped,"A"),b=get(cropped,"B");
    check("earlier_duration_source_and_end_unchanged",near(a.startSeconds,1)&&near(a.durationSeconds,1)&&near(a.sourceOffsetSeconds,0)&&near(a.sourceDurationSeconds,1));
    check("later_end_fixed_and_overlap_removed",near(b.startSeconds,2)&&near(b.durationSeconds,.6)&&near(b.startSeconds+b.durationSeconds,2.6)&&!overlaps(cropped.tracks[0]));
    check("later_source_head_cropped_without_restretch",near(b.sourceOffsetSeconds,.4)&&near(b.sourceDurationSeconds,.6)&&near(b.notes[0].durationSeconds,.6)
        &&near(b.notes[0].startSeconds,0)&&near(b.notes[0].contour[0].timeSeconds,0)&&near(b.notes[0].nativeSourceEndSeconds-b.notes[0].nativeSourceStartSeconds,.6));
    model.dispatchPendingMessages();roll.diagnosticRefresh();check("closed_preview_image_written",image("overlap-disabled.png"));check("closed_track_has_no_overlap_card",roll.diagnosticOverlapCardCount()==0);
    check("trimmed_project_save_reopen",model.save(folder.getChildFile("overlap-disabled.hjpx"),error)&&reopened.load(folder.getChildFile("overlap-disabled.hjpx"),error)
        &&!reopened.snapshot().tracks[0].allowNativeAudioOverlap&&near(get(reopened.snapshot(),"B").startSeconds,2));
    model.undo();check("one_undo_restores_overlap_toggle_and_audio",model.snapshot().tracks[0].allowNativeAudioOverlap&&near(get(model.snapshot(),"B").startSeconds,1.6)&&near(get(model.snapshot(),"B").durationSeconds,1));
    auto nonlinear=overlapped;auto& original=nonlinear.tracks[0].clips[1];original.sourceOffsetSeconds=.1;original.sourceDurationSeconds=.8;
    original.sourceTimeMap={{0,0},{.2,.1},{.7,.6},{1,.8}};original.gainEnvelope={{0,-3},{.5,0},{1,-6}};original.inheritedGainEnvelopes={{{0,-1},{1,-4}}};
    auto& n=original.notes[0];n.amplitudeEnvelope={{0,-2},{.5,0},{1,-4}};n.pitchControlPoints={{0,57},{.5,59},{1,58}};
    for(auto& p:n.contour){p.relativeCents=static_cast<float>(p.timeSeconds*50);p.withoutVibratoCents=p.relativeCents;p.hasManualTarget=true;p.manualTargetCents=static_cast<float>(100+p.timeSeconds*20);}
    bindNativeNoteSource(n,original);const auto oldClock=nativeClipClock(original);const auto oldNote=n;
    model.replace(nonlinear);model.setNativeAudioOverlap("native",false);const auto tail=get(model.snapshot(),"B");bool clockSame=true;
    for(double t=0;t<=tail.durationSeconds;t+=.005)clockSame &= near(tail.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(tail),t),original.sourceOffsetSeconds+nativeSourceTimeAt(oldClock,t+.4));
    check("nonlinear_stretch_clock_preserved_in_remaining_tail",clockSame);
    check("dense_pitch_boundary_interpolated_and_anchors_shifted",near(tail.notes[0].contour[0].relativeCents,20)&&near(tail.notes[0].contour[0].manualTargetCents,108)
        &&near(tail.notes[0].pitchControlPoints[0].timeSeconds,-.4)&&near(tail.notes[0].amplitudeEnvelope[0].timeSeconds,-.4)
        &&near(tail.notes[0].pitchControlPoints.back().timeSeconds,.6));
    auto contained=overlapped;contained.tracks[0].clips[1].durationSeconds=.2;contained.tracks[0].clips[1].notes[0].durationSeconds=.2;
    model.replace(contained);model.setNativeAudioOverlap("native",false);check("fully_covered_later_audio_removed",model.snapshot().tracks[0].clips.size()==1&&near(get(model.snapshot(),"A").durationSeconds,1));
    model.undo();check("fully_covered_removal_is_undoable",model.snapshot().tracks[0].clips.size()==2&&model.snapshot().tracks[0].allowNativeAudioOverlap);
    auto tiny=overlapped;tiny.tracks[0].clips[1].startSeconds=1.005;model.replace(tiny);model.setNativeAudioOverlap("native",false);
    check("short_remainder_not_artificially_stretched",near(get(model.snapshot(),"B").durationSeconds,.005)&&near(get(model.snapshot(),"B").startSeconds,2));
    auto chain=overlapped;chain.tracks[0].clips.push_back(clip("C",2.2,firstFile));model.replace(chain);model.setNativeAudioOverlap("native",false);
    check("multiple_overlaps_removed_in_time_order",!overlaps(model.snapshot().tracks[0])&&near(get(model.snapshot(),"B").startSeconds,2)&&near(get(model.snapshot(),"C").startSeconds,2.6)&&near(get(model.snapshot(),"C").startSeconds+get(model.snapshot(),"C").durationSeconds,3.2));
    auto linked=initial;linked.tracks[0].clips={assembledLinkedAudio(initial.tracks[0].clips),clip("C",4,firstFile)};model.replace(linked);model.resizeNativeNoteEdge("B-note",3,false);
    check("linked_group_cannot_stretch_into_external_audio",!overlaps(model.snapshot().tracks[0])&&near(model.snapshot().tracks[0].clips.front().startSeconds+model.snapshot().tracks[0].clips.front().durationSeconds,4));
    model.replace(initial);const auto copied=copyNativeAudioNotes(initial,{"A-note"});const auto pasted=model.insertNativeAudioClips("native",copied.clips,copied.connections,1.5);
    check("paste_finds_free_interval_without_changing_length",pasted.size()==1&&near(model.snapshot().tracks[0].clips.back().startSeconds,3.4)&&near(model.snapshot().tracks[0].clips.back().durationSeconds,1)&&!overlaps(model.snapshot().tracks[0]));
    model.replace(initial);const auto duplicate=model.duplicateClip("A",1.5);check("duplicate_avoids_existing_audio",duplicate.isNotEmpty()&&near(model.snapshot().tracks[0].clips.back().startSeconds,3.4)&&!overlaps(model.snapshot().tracks[0]));
    auto legacy=overlapped;legacy.tracks[0].allowNativeAudioOverlap=false;model.replace(legacy);check("explicit_close_cleans_legacy_overlap_even_if_flag_false",model.setNativeAudioOverlap("native",false)&&!overlaps(model.snapshot().tracks[0]));
    auto u=initial;u.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;model.replace(u);check("utau_does_not_use_native_overlap_toggle",!model.setNativeAudioOverlap("native",true));
    const auto bank=folder.getChildFile("ds-bank");bank.createDirectory();bank.getChildFile("dsconfig.yaml").replaceWithText("test: true");
    auto ds=u;ds.tracks[0].utauMode=UtauMode::mou;ds.tracks[0].voicebankDirectory=bank;model.replace(ds);check("diffsinger_does_not_use_native_overlap_toggle",!model.setNativeAudioOverlap("native",true));
    MainComponent main;main.setSize(1600,900);main.diagnosticProject().replace(overlapped);main.diagnosticProject().dispatchPendingMessages();main.diagnosticSelectTrack("native");main.diagnosticRefreshControls();
    auto& button=main.diagnosticNativeAudioOverlapButton();check("native_toolbar_has_enabled_overlap_toggle",button.isVisible()&&button.getToggleState()&&button.getWidth()>0);
    const auto view=main.createComponentSnapshot(main.getLocalBounds());juce::PNGImageFormat png;auto stream=folder.getChildFile("interface-enabled.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check("full_interface_preview_written",stream&&png.writeImageToStream(view,*stream));
    button.setToggleState(false,juce::sendNotification);check("toolbar_click_closes_and_crops_overlap",!main.diagnosticProject().snapshot().tracks[0].allowNativeAudioOverlap&&!overlaps(main.diagnosticProject().snapshot().tracks[0]));
    main.diagnosticProject().replace(u);main.diagnosticSelectTrack("native");main.diagnosticRefreshControls();check("overlap_toggle_hidden_for_utau",!button.isVisible());
    main.diagnosticProject().replace(ds);main.diagnosticSelectTrack("native");main.diagnosticRefreshControls();check("overlap_toggle_hidden_for_diffsinger",!button.isVisible());
    // Verify actual native output: the shared interval is a sum, closing removes
    // only the later voice there and retains its tail at the original position.
    auto raw=overlapped;raw.tracks[0].clips[1].notes[0].midiNote=raw.tracks[0].clips[1].notes[0].sourceMidiCenter=static_cast<float>(69+12*std::log2(330./440.));
    AudioEngine engine;engine.prepareToPlay(256,48000);WavExportOptions options;options.sampleRate=48000;options.channels=2;options.bitDepth=32;
    const auto render=[&](const ProjectData& d,const char* name){engine.syncProject(d);for(int i=0;i<1200&&engine.renderProgress();++i)juce::Thread::sleep(10);
        return engine.hasCurrentRenderedAudio()&&engine.exportWav(folder.getChildFile(name),error,"native",1,3,options);};
    model.replace(raw);model.setNativeAudioOverlap("native",false);const auto rawCropped=model.snapshot();auto onlyA=raw;onlyA.tracks[0].clips.resize(1);
    check("enabled_cropped_and_reference_audio_render",render(raw,"mix.wav")&&render(rawCropped,"cropped.wav")&&render(onlyA,"earlier-only.wav"));
    juce::AudioFormatManager formats;formats.registerBasicFormats();const auto read=[&](const char* name){std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(folder.getChildFile(name)));juce::AudioBuffer<float> samples;
        if(r){samples.setSize(2,static_cast<int>(r->lengthInSamples));r->read(&samples,0,samples.getNumSamples(),0,true,true);}return samples;};
    const auto mix=read("mix.wav"),closed=read("cropped.wav"),reference=read("earlier-only.wav");bool beforeSame=closed.getNumSamples()==76800&&reference.getNumSamples()==48000&&mix.getNumSamples()==76800;float firstDiff=0,tailDiff=0,removed=0;
    if(beforeSame){for(int i=30000;i<47000;++i){firstDiff=std::max(firstDiff,std::abs(closed.getSample(0,i)-reference.getSample(0,i)));removed=std::max(removed,std::abs(mix.getSample(0,i)-closed.getSample(0,i)));}
        for(int i=49000;i<76000;++i)tailDiff=std::max(tailDiff,std::abs(mix.getSample(0,i)-closed.getSample(0,i)));}
    check("closing_removes_only_overlapped_later_voice",beforeSame&&firstDiff<1.e-5&&removed>.05f);
    check("remaining_later_audio_matches_original_timeline",beforeSame&&tailDiff<1.e-5);std::cout<<"before_difference="<<firstDiff<<"; tail_difference="<<tailDiff<<std::endl;engine.releaseResources();
    std::cout<<"native_audio_overlap_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
