#pragma once
#include "../NativeNoteTiming.h"
namespace hachi
{
inline bool MainComponent::diagnosticNativeNoteMove(const juce::File& folder)
{
    folder.createDirectory(); stopTimer(); bool ok=true; int checks=0;
    const auto check=[&](const char* name,bool pass){ok &= pass;++checks;std::cout<<name<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-5;};
    const auto source=folder.getChildFile("three-part-source.wav");
    juce::AudioBuffer<float> samples(1,57600);
    for(int i=0;i<samples.getNumSamples();++i)
    {
        const auto t=i/48000.;const auto level=.25+.08*std::sin(t*7);
        samples.setSample(0,i,static_cast<float>(level*(std::sin(t*2*juce::MathConstants<double>::pi*220)
            +.25*std::sin(t*2*juce::MathConstants<double>::pi*440))));
    }
    juce::WavAudioFormat format;auto stream=source.createOutputStream();
    if(stream){stream->setPosition(0);stream->truncate();}
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),48000,1,24,{},0));
    if(!writer)return false;stream.release();check("source_written",writer->writeFromAudioSampleBuffer(samples,0,57600));writer.reset();
    ClipData clip;clip.id="native-split";clip.sourceFile=source;clip.startSeconds=1;
    clip.durationSeconds=clip.sourceDurationSeconds=1.2;
    NoteData note;note.id="head";note.label="baip";note.durationSeconds=1.2;note.consonantSeconds=.1;
    note.midiNote=note.sourceMidiCenter=57;note.sourcePitchMeasured=true;
    note.nativeSourceStartSeconds=0;note.nativeSourceEndSeconds=1.2;
    note.nativeSegments={{"c","baip",NativeSegmentRole::consonant,0,.1},
                         {"v","baip",NativeSegmentRole::vowel,.1,1.2}};
    for(int i=0;i<=240;++i)note.contour.push_back({i*.005,0,0,true});
    clip.notes={note};TrackData track;track.id="native";track.pitchAlgorithm=PitchAlgorithm::world;
    track.normalizeVolume=false;track.volume=.1f;track.clips={clip};ProjectData data;data.tracks={track};
    ProjectModel model;model.replace(data);const auto middle=model.splitNote("head",.3);
    const auto tail=model.splitNote(middle,.6);auto split=model.snapshot();
    check("split_creates_three_contiguous_notes",split.tracks[0].clips[0].notes.size()==3);
    auto utauFixture=split;utauFixture.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;
    ProjectModel utauModel;utauModel.replace(utauFixture);juce::String utauError;
    check("utau_drag_fixture_saved",utauModel.save(folder.getChildFile("utau-drag-threshold.hjpx"),utauError));
    const auto& pieces=split.tracks[0].clips[0].notes;
    check("split_source_intervals_are_independent",near(pieces[0].nativeSourceEndSeconds,.3)
        &&near(pieces[1].nativeSourceStartSeconds,.3)&&near(pieces[1].nativeSourceEndSeconds,.9)
        &&near(pieces[2].nativeSourceStartSeconds,.9)&&near(pieces[2].nativeSourceEndSeconds,1.2));
    check("split_metadata_is_cropped",pieces[1].nativeSegments.size()==1&&pieces[2].nativeSegments.size()==1
        &&near(pieces[1].nativeSegments[0].sourceStartSeconds,0)
        &&near(pieces[1].nativeSegments[0].sourceEndSeconds,.6));
    PianoRollComponent roll(model,strings);roll.setFocusedTrack("native");roll.setFocusedClip(clip.id);
    roll.setTool(PianoRollComponent::Tool::note);roll.setPixelsPerSecond(400);roll.setRowHeight(26);roll.setSize(1200,roll.getHeight());
    roll.setShowNativeWaveforms(true);roll.setShowWaveforms(false);roll.setPlayheadSeconds(3);
    juce::Thread::sleep(120);
    const auto event=[](juce::Component& c,juce::Point<float> at,juce::Point<float> down,int mods)
    {return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(mods),0,0,0,0,0,
        &c,&c,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    const auto crop=[&]{return juce::Rectangle<int>(330,static_cast<int>(roll.diagnosticNoteY(0))-48,730,110);};
    const auto capture=[&]{return roll.createComponentSnapshot(crop());};
    const auto save=[&](const char* name,const juce::Image& image){auto out=folder.getChildFile(juce::String(name)+".png").createOutputStream();
        if(out){out->setPosition(0);out->truncate();return juce::PNGImageFormat().writeImageToStream(image,*out);}return false;};
    const auto drag=[&](std::size_t index,double delta,const char* imageName){
        (void)capture();const auto begin=juce::Point<float>(roll.diagnosticHitX(index)+45,roll.diagnosticNoteY(index));
        const auto end=begin.translated(static_cast<float>(delta*400),0);
        const auto mods=juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier;
        const auto before=model.snapshot();roll.mouseDown(event(roll,begin,begin,mods));roll.mouseDrag(event(roll,end,begin,mods));
        const auto preview=capture();check("drag_preview_does_not_commit",near(model.snapshot().tracks[0].clips[0].startSeconds,before.tracks[0].clips[0].startSeconds));
        check(imageName,save(imageName,preview));roll.mouseUp(event(roll,end,begin,mods));model.dispatchPendingMessages();roll.diagnosticRefresh();
        return preview;};
    drag(0,-.2,"head-forward-preview");auto moved=model.snapshot().tracks[0].clips[0];
    check("body_drag_moves_head_before_region_origin",near(moved.startSeconds,.8)&&near(moved.notes[0].durationSeconds,.3));
    check("middle_stretches_and_tail_stays_fixed",near(moved.notes[1].durationSeconds,.8)
        &&near(moved.startSeconds+moved.notes[2].startSeconds,1.9));
    drag(2,.25,"tail-backward-preview");moved=model.snapshot().tracks[0].clips[0];
    check("tail_drag_keeps_tail_length",near(moved.notes[2].durationSeconds,.3)
        &&near(moved.startSeconds+moved.notes[2].startSeconds,2.15));
    check("both_drags_extend_middle_without_gaps",near(moved.notes[1].durationSeconds,1.05)
        &&near(moved.notes[0].startSeconds+moved.notes[0].durationSeconds,moved.notes[1].startSeconds)
        &&near(moved.notes[1].startSeconds+moved.notes[1].durationSeconds,moved.notes[2].startSeconds));
    check("source_clock_stretches_middle_without_looping",near(nativeSourceTimeAt(nativeClipClock(moved),.3),.3)
        &&near(nativeSourceTimeAt(nativeClipClock(moved),.825),.6)
        &&near(nativeSourceTimeAt(nativeClipClock(moved),1.35),.9));
    check("f0_scales_with_stretched_middle",near(moved.notes[1].contour.back().timeSeconds,1.05)
        &&moved.notes[1].contour.size()==pieces[1].contour.size());
    check("timing_edit_invalidates_waveform_cache",AudioEngine::nativeClipWaveformHash(moved,track)
        !=AudioEngine::nativeClipWaveformHash(split.tracks[0].clips[0],track));
    check("no_default_envelope_edit_added",std::all_of(moved.notes.begin(),moved.notes.end(),[](const auto& n){return n.amplitudeEnvelope.empty();}));
    model.undo();check("undo_tail_restores_middle",near(model.snapshot().tracks[0].clips[0].notes[1].durationSeconds,.8));
    model.undo();check("undo_head_restores_original_timing",near(model.snapshot().tracks[0].clips[0].startSeconds,1)
        &&near(model.snapshot().tracks[0].clips[0].notes[1].durationSeconds,.6));
    model.redo();model.redo();
    const auto file=folder.getChildFile("linked-timing.hachi");juce::String error;
    check("project_saved",model.save(file,error));ProjectModel reopened;check("project_reopened",reopened.load(file,error));
    check("saved_clock_and_seams_preserved",near(reopened.snapshot().tracks[0].clips[0].notes[1].durationSeconds,1.05)
        &&near(nativeSourceTimeAt(nativeClipClock(reopened.snapshot().tracks[0].clips[0]),.825),.6));
    // Excessive reverse travel stops before the middle can collapse or cross.
    model.moveNativeNotes({tail},-10,0);auto limited=model.snapshot().tracks[0].clips[0];
    check("reverse_drag_stops_at_minimum_middle_length",near(limited.notes[1].durationSeconds,.01)
        &&near(limited.notes[1].startSeconds+.01,limited.notes[2].startSeconds));
    model.replace(split);model.moveNativeNotes({"head"},-10,0);limited=model.snapshot().tracks[0].clips[0];
    check("head_cannot_cross_timeline_zero",near(limited.startSeconds,0)&&near(limited.notes[0].durationSeconds,.3));
    model.replace(split);model.moveNativeNotes({"head",middle,tail},.2,0);limited=model.snapshot().tracks[0].clips[0];
    check("whole_selection_moves_rigidly",near(limited.notes[1].durationSeconds,.6)&&near(limited.notes[2].durationSeconds,.3)
        &&near(limited.startSeconds+limited.notes[0].startSeconds,1.2));
    // Existing projects with cloned segment metadata use the same source clock.
    auto legacy=split;for(auto& n:legacy.tracks[0].clips[0].notes){n.nativeSourceStartSeconds=0;n.nativeSourceEndSeconds=1.2;n.nativeSegments=note.nativeSegments;}
    model.replace(legacy);check("old_split_project_still_draggable",model.moveNativeNotes({tail},.25,0));
    check("old_split_metadata_rebound_to_source",near(model.snapshot().tracks[0].clips[0].notes[2].nativeSourceStartSeconds,.9));
    // A merged region must retain each source child's gain and media clock.
    auto merged=split;auto child=merged.tracks[0].clips[0];child.id="child";child.startSeconds=0;child.gain=.7f;child.notes.clear();
    merged.tracks[0].clips[0].parts={child};merged.tracks[0].clips[0].gain=.5f;
    for(auto& n:merged.tracks[0].clips[0].notes)n.clipPartId="child";
    model.replace(merged);check("merged_child_timing_is_editable",model.moveNativeNotes({tail},.25,0));
    const auto mergedMoved=model.snapshot().tracks[0].clips[0];
    check("merged_child_does_not_duplicate_parent_gain",near(mergedMoved.parts[0].gain,.7)&&near(mergedMoved.gain,.5)
        &&near(mergedMoved.notes[1].durationSeconds,.85));
    // Play the stretched recording through the actual renderer, and inspect
    // the middle and tail rather than accepting a timing-only mock.
    auto audioData=split;audioData.tracks[0].clips={moved};AudioEngine engine;engine.prepareToPlay(256,48000);engine.syncProject(audioData);
    for(int i=0;i<1000&&engine.renderProgress();++i)juce::Thread::sleep(10);
    check("actual_stretched_audio_rendered",engine.hasCurrentRenderedAudio());
    WavExportOptions options;options.sampleRate=48000;options.channels=2;options.bitDepth=32;
    const auto rendered=folder.getChildFile("stretched.wav");check("stretched_audio_exported",engine.exportWav(rendered,error,"native",.8,2.45,options));
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(rendered));
    if(reader)
    {
        juce::AudioBuffer<float> audio(2,static_cast<int>(reader->lengthInSamples));reader->read(&audio,0,audio.getNumSamples(),0,true,true);
        check("rendered_duration_matches_extended_clock",std::abs(audio.getNumSamples()-79200)<=1);
        float minimum=1.f;for(int i=4800;i+4800<audio.getNumSamples();i+=4800)minimum=std::min(minimum,audio.getRMSLevel(0,i,4800));
        check("stretched_middle_and_tail_remain_audible",minimum>.002f);
    }
    else check("stretched_audio_readable",false);
    engine.releaseResources();
    // Edge stretching differs from whole-note movement: the opposite edge
    // stays fixed and the stretched region itself takes the extra time.
    auto edgeData=split;
    edgeData.tracks[0].clips[0].notes[0].amplitudeEnvelope={{0,-3},{.15,-1},{.3,-3}};
    model.replace(edgeData);model.dispatchPendingMessages();roll.diagnosticRefresh();
    roll.setTool(PianoRollComponent::Tool::note);roll.setFocusedClip(clip.id);
    const auto edgeDrag=[&](std::size_t index,bool left,double delta,const char* imageName,bool free=true){
        (void)capture();const auto bounds=roll.diagnosticHitBounds(index);
        const auto begin=juce::Point<float>(left?bounds.getX()+2:bounds.getRight()-2,bounds.getCentreY());
        const auto end=begin.translated(static_cast<float>(delta*400),0);
        const auto mods=juce::ModifierKeys::leftButtonModifier|(free?juce::ModifierKeys::altModifier:0);
        const auto revision=model.revisionNumber();roll.mouseDown(event(roll,begin,begin,mods));roll.mouseDrag(event(roll,end,begin,mods));
        check(imageName,save(imageName,capture()));
        check("edge_preview_does_not_modify_project",model.revisionNumber()==revision);
        roll.mouseUp(event(roll,end,begin,mods));model.dispatchPendingMessages();roll.diagnosticRefresh();
    };
    edgeDrag(0,true,-.2,"head-left-edge-stretch-preview");auto headStretched=model.snapshot().tracks[0].clips[0];
    check("head_left_edge_stretches_before_region_origin",near(headStretched.startSeconds,.8)
        &&near(headStretched.notes[0].durationSeconds,.5));
    check("head_stretch_keeps_right_edge_and_remaining_notes_fixed",near(headStretched.startSeconds+headStretched.notes[0].durationSeconds,1.3)
        &&near(headStretched.startSeconds+headStretched.notes[1].startSeconds,1.3)
        &&near(headStretched.notes[1].durationSeconds,.6)&&near(headStretched.startSeconds+headStretched.notes[2].startSeconds,1.9));
    check("head_source_is_stretched_instead_of_padded",near(nativeSourceTimeAt(nativeClipClock(headStretched),.25),.15)
        &&near(headStretched.notes[0].nativeSourceEndSeconds,.3));
    check("head_pitch_and_envelope_follow_stretch",near(headStretched.notes[0].contour.back().timeSeconds,.5)
        &&near(headStretched.notes[0].amplitudeEnvelope[1].timeSeconds,.25)
        &&near(headStretched.notes[0].amplitudeEnvelope.back().timeSeconds,.5));
    model.undo();check("head_edge_stretch_undo",near(model.snapshot().tracks[0].clips[0].startSeconds,1)
        &&near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,.3));
    model.redo();model.dispatchPendingMessages();roll.diagnosticRefresh();
    edgeDrag(2,false,.2,"tail-right-edge-stretch-preview");const auto bothEdges=model.snapshot().tracks[0].clips[0];
    check("tail_right_edge_stretches_without_moving_its_start",near(bothEdges.startSeconds+bothEdges.notes[2].startSeconds,1.9)
        &&near(bothEdges.notes[2].durationSeconds,.5)&&near(bothEdges.notes[1].durationSeconds,.6));
    check("tail_source_is_stretched_instead_of_looped",near(nativeSourceTimeAt(nativeClipClock(bothEdges),1.35),1.05)
        &&near(bothEdges.sourceDurationSeconds,1.2)&&near(bothEdges.audioLength(),1.6));
    const auto edgeFile=folder.getChildFile("edge-stretched.hjpx");
    check("edge_stretch_saved_and_reopened",model.save(edgeFile,error)&&reopened.load(edgeFile,error)
        &&near(reopened.snapshot().tracks[0].clips[0].notes[0].durationSeconds,.5)
        &&near(reopened.snapshot().tracks[0].clips[0].notes[2].durationSeconds,.5));
    model.replace(edgeData);model.resizeNativeNoteEdge("head",-10,true);
    check("head_stretch_cannot_cross_timeline_zero",near(model.snapshot().tracks[0].clips[0].startSeconds,0)
        &&near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,1.3));
    model.replace(edgeData);model.resizeNativeNoteEdge("head",10,true);
    check("head_shortening_keeps_minimum_length",near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,.01));
    model.replace(edgeData);model.resizeNativeNoteEdge("head",.2,false);const auto internal=model.snapshot().tracks[0].clips[0];
    check("internal_edge_expansion_moves_neighbors_without_compression",near(internal.notes[0].durationSeconds,.5)
        &&near(internal.notes[1].startSeconds,.5)&&near(internal.notes[1].durationSeconds,.6)
        &&near(internal.notes[2].startSeconds,1.1)&&near(internal.notes[2].durationSeconds,.3));
    model.replace(merged);check("merged_child_head_edge_stretches",model.resizeNativeNoteEdge("head",-.2,true)
        &&near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,.5)
        &&near(model.snapshot().tracks[0].clips[0].parts[0].gain,.7));
    auto edgeAudio=edgeData;edgeAudio.tracks[0].clips={bothEdges};AudioEngine edgeEngine;
    edgeEngine.prepareToPlay(256,48000);edgeEngine.syncProject(edgeAudio);
    for(int i=0;i<1000&&edgeEngine.renderProgress();++i)juce::Thread::sleep(10);
    check("actual_head_and_tail_stretch_rendered",edgeEngine.hasCurrentRenderedAudio());
    const auto edgeWav=folder.getChildFile("head-tail-stretched.wav");
    check("actual_edge_stretch_exported",edgeEngine.exportWav(edgeWav,error,"native",.8,2.4,options));
    reader.reset(formats.createReaderFor(edgeWav));
    if(reader){juce::AudioBuffer<float> audio(2,static_cast<int>(reader->lengthInSamples));reader->read(&audio,0,audio.getNumSamples(),0,true,true);
        check("edge_stretch_audio_has_exact_duration",std::abs(audio.getNumSamples()-76800)<=1);
        check("head_extension_contains_real_audio",audio.getRMSLevel(0,2400,4800)>.002f);
        check("tail_extension_contains_real_audio",audio.getRMSLevel(0,67200,4800)>.002f);}
    else check("edge_stretch_audio_readable",false);
    edgeEngine.releaseResources();
    // Stretching the middle moves the outer pieces rigidly. Use real edge
    // gestures to verify that a shared boundary edits the intended note.
    auto middleData=split;
    middleData.tracks[0].clips[0].notes[1].amplitudeEnvelope={{0,-3},{.3,-1},{.6,-3}};
    model.replace(middleData);model.dispatchPendingMessages();roll.diagnosticRefresh();
    edgeDrag(1,true,-.2,"middle-left-stretch-preview");const auto middleLeft=model.snapshot().tracks[0].clips[0];
    check("middle_left_stretch_moves_head_rigidly",near(middleLeft.startSeconds,.8)
        &&near(middleLeft.notes[0].durationSeconds,.3)&&near(middleLeft.notes[1].durationSeconds,.8)
        &&near(middleLeft.startSeconds+middleLeft.notes[1].startSeconds,1.1));
    check("middle_left_stretch_keeps_tail_fixed",near(middleLeft.startSeconds+middleLeft.notes[2].startSeconds,1.9)
        &&near(middleLeft.notes[2].durationSeconds,.3));
    check("middle_left_stretch_preserves_source_clock",near(nativeSourceTimeAt(nativeClipClock(middleLeft),.15),.15)
        &&near(nativeSourceTimeAt(nativeClipClock(middleLeft),.7),.6));
    edgeDrag(1,false,.25,"middle-right-stretch-preview");const auto middleBoth=model.snapshot().tracks[0].clips[0];
    check("middle_right_stretch_moves_tail_rigidly",near(middleBoth.startSeconds+middleBoth.notes[2].startSeconds,2.15)
        &&near(middleBoth.notes[2].durationSeconds,.3)&&near(middleBoth.notes[1].durationSeconds,1.05)
        &&near(middleBoth.startSeconds+middleBoth.notes[0].startSeconds,.8));
    check("middle_stretch_keeps_both_seams_connected",near(middleBoth.notes[0].startSeconds+.3,middleBoth.notes[1].startSeconds)
        &&near(middleBoth.notes[1].startSeconds+1.05,middleBoth.notes[2].startSeconds));
    check("middle_stretch_updates_pitch_envelope_and_cache",near(middleBoth.notes[1].contour.back().timeSeconds,1.05)
        &&near(middleBoth.notes[1].amplitudeEnvelope[1].timeSeconds,.525)
        &&AudioEngine::nativeClipWaveformHash(middleBoth,track)!=AudioEngine::nativeClipWaveformHash(middleData.tracks[0].clips[0],track));
    model.undo();check("middle_stretch_undo_restores_tail",near(model.snapshot().tracks[0].clips[0].notes[1].durationSeconds,.8)
        &&near(model.snapshot().tracks[0].clips[0].startSeconds+model.snapshot().tracks[0].clips[0].notes[2].startSeconds,1.9));
    model.redo();const auto middleFile=folder.getChildFile("middle-edge-stretched.hjpx");
    check("middle_stretch_saved_and_reopened",model.save(middleFile,error)&&reopened.load(middleFile,error)
        &&near(reopened.snapshot().tracks[0].clips[0].notes[1].durationSeconds,1.05)
        &&near(reopened.snapshot().tracks[0].clips[0].notes[0].durationSeconds,.3)
        &&near(reopened.snapshot().tracks[0].clips[0].notes[2].durationSeconds,.3));
    model.replace(middleData);model.resizeNativeNoteEdge(middle,.15,true);auto shorter=model.snapshot().tracks[0].clips[0];
    check("middle_left_shortening_moves_head_without_stretching",near(shorter.notes[0].durationSeconds,.3)
        &&near(shorter.startSeconds+shorter.notes[0].startSeconds,1.15)&&near(shorter.notes[1].durationSeconds,.45)
        &&near(shorter.startSeconds+shorter.notes[2].startSeconds,1.9));
    model.replace(middleData);model.resizeNativeNoteEdge(middle,-.15,false);shorter=model.snapshot().tracks[0].clips[0];
    check("middle_right_shortening_moves_tail_without_stretching",near(shorter.notes[2].durationSeconds,.3)
        &&near(shorter.startSeconds+shorter.notes[2].startSeconds,1.75)&&near(shorter.notes[1].durationSeconds,.45));
    model.replace(middleData);model.resizeNativeNoteEdge(middle,-10,true);shorter=model.snapshot().tracks[0].clips[0];
    check("middle_left_stretch_respects_head_timeline_zero",near(shorter.startSeconds,0)
        &&near(shorter.notes[0].durationSeconds,.3)&&near(shorter.notes[1].durationSeconds,1.6));
    model.replace(middleData);model.resizeNativeNoteEdge(middle,10,true);shorter=model.snapshot().tracks[0].clips[0];
    check("middle_left_shortening_stops_at_minimum",near(shorter.notes[1].durationSeconds,.01)
        &&near(shorter.notes[0].durationSeconds,.3)&&near(shorter.notes[2].durationSeconds,.3));
    model.replace(middleData);model.resizeNativeNoteEdge(middle,-10,false);shorter=model.snapshot().tracks[0].clips[0];
    check("middle_right_shortening_stops_at_minimum",near(shorter.notes[1].durationSeconds,.01)
        &&near(shorter.notes[0].durationSeconds,.3)&&near(shorter.notes[2].durationSeconds,.3));
    model.replace(merged);check("merged_child_middle_stretch_keeps_outer_lengths",model.resizeNativeNoteEdge(middle,.2,false)
        &&near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,.3)
        &&near(model.snapshot().tracks[0].clips[0].notes[2].durationSeconds,.3)
        &&near(model.snapshot().tracks[0].clips[0].parts[0].gain,.7));
    auto middleAudio=middleData;middleAudio.tracks[0].clips={middleBoth};AudioEngine middleEngine;
    middleEngine.prepareToPlay(256,48000);middleEngine.syncProject(middleAudio);
    for(int i=0;i<1000&&middleEngine.renderProgress();++i)juce::Thread::sleep(10);
    check("actual_middle_stretch_rendered",middleEngine.hasCurrentRenderedAudio());
    const auto middleWav=folder.getChildFile("middle-edge-stretched.wav");
    check("actual_middle_stretch_exported",middleEngine.exportWav(middleWav,error,"native",.8,2.45,options));
    reader.reset(formats.createReaderFor(middleWav));
    if(reader){juce::AudioBuffer<float> audio(2,static_cast<int>(reader->lengthInSamples));reader->read(&audio,0,audio.getNumSamples(),0,true,true);
        check("middle_stretch_audio_has_exact_duration",std::abs(audio.getNumSamples()-79200)<=1);
        float minimum=1.f;for(int i=2400;i+4800<audio.getNumSamples();i+=4800)minimum=std::min(minimum,audio.getRMSLevel(0,i,4800));
        check("middle_stretch_and_rigid_neighbors_remain_audible",minimum>.002f);}
    else check("middle_stretch_audio_readable",false);
    middleEngine.releaseResources();
    // Imported analysis leaves small original-audio margins outside the note.
    // Neither a rigid move nor a long stretch may be capped by those margins.
    auto paddedData=data;auto& padded=paddedData.tracks[0].clips[0].notes[0];
    padded.startSeconds=.02;padded.durationSeconds=1.16;padded.consonantSeconds=0;
    padded.nativeSourceStartSeconds=.02;padded.nativeSourceEndSeconds=1.18;
    padded.nativeSegments.clear();padded.contour.clear();
    for(int i=0;i<=232;++i)padded.contour.push_back({i*.005,0,0,true});
    model.replace(paddedData);model.dispatchPendingMessages();roll.diagnosticRefresh();
    drag(0,.75,"whole-audio-right-preview");auto whole=model.snapshot().tracks[0].clips[0];
    check("whole_audio_moves_right_beyond_source_margin",near(whole.startSeconds,1.75)
        &&near(whole.notes[0].startSeconds,.02)&&near(whole.notes[0].durationSeconds,1.16)
        &&near(whole.audioLength(),1.2)&&near(whole.audioStartSeconds,0));
    drag(0,-.9,"whole-audio-left-preview");whole=model.snapshot().tracks[0].clips[0];
    check("whole_audio_moves_left_beyond_source_margin",near(whole.startSeconds,.85)
        &&near(whole.notes[0].startSeconds,.02)&&near(whole.durationSeconds,1.2));
    model.replace(paddedData);model.dispatchPendingMessages();roll.diagnosticRefresh();
    edgeDrag(0,false,4.8,"whole-audio-long-stretch-preview");const auto longAudio=model.snapshot().tracks[0].clips[0];
    check("whole_audio_can_stretch_to_five_times_duration",near(longAudio.startSeconds,1)
        &&near(longAudio.notes[0].durationSeconds,5.96)&&near(longAudio.audioLength(),6)
        &&near(longAudio.durationSeconds,6));
    check("long_stretch_preserves_outer_source_margins",near(nativeSourceTimeAt(nativeClipClock(longAudio),.01),.01)
        &&near(nativeSourceTimeAt(nativeClipClock(longAudio),5.99),1.19)
        &&near(nativeSourceTimeAt(nativeClipClock(longAudio),3),.6));
    check("long_stretch_scales_measured_pitch",near(longAudio.notes[0].contour.back().timeSeconds,5.96));
    const auto longFile=folder.getChildFile("whole-audio-long-stretched.hjpx");
    check("whole_audio_long_stretch_saved_and_reopened",model.save(longFile,error)&&reopened.load(longFile,error)
        &&near(reopened.snapshot().tracks[0].clips[0].audioLength(),6));
    model.replace(paddedData);model.dispatchPendingMessages();roll.diagnosticRefresh();
    edgeDrag(0,true,-.6,"whole-audio-left-stretch-preview");const auto leftAudio=model.snapshot().tracks[0].clips[0];
    check("whole_audio_left_stretch_moves_outer_margin",near(leftAudio.startSeconds,.4)
        &&near(leftAudio.notes[0].durationSeconds,1.76)&&near(leftAudio.audioLength(),1.8)
        &&near(leftAudio.startSeconds+leftAudio.notes[0].startSeconds+leftAudio.notes[0].durationSeconds,2.18));
    model.replace(paddedData);model.moveNativeNotes({"head"},-10,0);whole=model.snapshot().tracks[0].clips[0];
    check("whole_audio_move_keeps_original_audio_after_zero",near(whole.startSeconds,0)
        &&near(whole.notes[0].startSeconds,.02)&&near(whole.audioLength(),1.2));
    model.replace(merged);model.resizeNativeNoteEdge(middle,.2,false);
    // Exercise the large-stretch source map with real synthesis and inspect
    // the end, where an incorrect map would pad several seconds of silence.
    auto longProject=paddedData;longProject.tracks[0].clips={longAudio};AudioEngine longEngine;
    longEngine.prepareToPlay(256,48000);longEngine.syncProject(longProject);
    for(int i=0;i<1500&&longEngine.renderProgress();++i)juce::Thread::sleep(10);
    check("actual_long_stretch_rendered",longEngine.hasCurrentRenderedAudio());
    const auto longWav=folder.getChildFile("whole-audio-long-stretched.wav");
    check("actual_long_stretch_exported",longEngine.exportWav(longWav,error,"native",1,7,options));
    reader.reset(formats.createReaderFor(longWav));
    if(reader){juce::AudioBuffer<float> audio(2,static_cast<int>(reader->lengthInSamples));reader->read(&audio,0,audio.getNumSamples(),0,true,true);
        check("long_stretch_audio_has_exact_duration",std::abs(audio.getNumSamples()-288000)<=1);
        float minimum=1.f;for(int i=2400;i+4800<audio.getNumSamples();i+=4800)minimum=std::min(minimum,audio.getRMSLevel(0,i,4800));
        check("long_stretch_remains_audible_through_end",minimum>.002f);}
    else check("long_stretch_audio_readable",false);
    longEngine.releaseResources();
    const auto nsfDirectory=juce::SystemStats::getEnvironmentVariable("HACHI_TEST_NSF_MODEL_DIR",{});
    if(nsfDirectory.isNotEmpty())
    {
        longProject.tracks[0].pitchAlgorithm=PitchAlgorithm::nsfHifigan;
        longProject.tracks[0].nativeNsfAudio=true;
        AudioEngine neural;neural.setHifiganModelDirectory(juce::File(nsfDirectory));
        neural.setInferenceConfiguration(backend::InferenceBackend::cpu,0);neural.prepareToPlay(256,48000);neural.syncProject(longProject);
        for(int i=0;i<5000&&neural.renderProgress();++i)juce::Thread::sleep(10);
        check("actual_nsf_long_stretch_uses_model",neural.hasCurrentRenderedAudio()&&neural.activeRenderBackends().contains("nsf-hifigan"));
        const auto neuralWav=folder.getChildFile("nsf-long-stretched.wav");
        check("actual_nsf_long_stretch_exported",neural.exportWav(neuralWav,error,"native",1,7,options));
        reader.reset(formats.createReaderFor(neuralWav));
        check("nsf_long_stretch_audio_has_exact_duration",reader&&std::abs(reader->lengthInSamples-288000)<=1);
        float minimum=0;
        if(reader){juce::AudioBuffer<float> audio(2,static_cast<int>(reader->lengthInSamples));reader->read(&audio,0,audio.getNumSamples(),0,true,true);
            minimum=1;for(int i=2400;i+4800<audio.getNumSamples();i+=4800)minimum=std::min(minimum,audio.getRMSLevel(0,i,4800));}
        check("nsf_long_stretch_remains_audible_through_end",minimum>.002f);
        neural.releaseResources();
    }
    // Default edge drags use the same fine note-edit step in both directions,
    // relative to the original edge rather than the coarse drawing grid.
    auto snappedData=paddedData;snappedData.gridDivision="1/4";snappedData.noteEditDivision=64;
    snappedData.tempoChanges={{8,240}};snappedData.tracks[0].clips[0].startSeconds=1.213;
    const auto install=[&](const ProjectData& value){model.replace(value);model.dispatchPendingMessages();roll.diagnosticRefresh();};
    roll.setPlayheadSeconds(5);
    install(snappedData);edgeDrag(0,true,-.02,"default-left-fine-step-preview",false);
    const auto snappedLeft=model.snapshot().tracks[0].clips[0];
    check("default_left_drag_uses_fine_step_without_coarse_jump",near(snappedLeft.notes[0].durationSeconds,1.16+3./128.)
        &&near(snappedLeft.startSeconds+snappedLeft.notes[0].startSeconds,1.233-3./128.));
    install(snappedData);edgeDrag(0,false,.02,"default-right-fine-step-preview",false);
    check("default_right_drag_matches_left_at_local_tempo",near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,
        snappedLeft.notes[0].durationSeconds));
    auto fineGrid=snappedData;fineGrid.gridDivision="1/64";install(fineGrid);
    edgeDrag(0,true,-.02,"drawing-grid-does-not-change-stretch-preview",false);
    check("drawing_grid_does_not_change_native_stretch_amount",near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,
        snappedLeft.notes[0].durationSeconds));
    auto coarseEdit=snappedData;coarseEdit.noteEditDivision=32;install(coarseEdit);
    edgeDrag(0,true,-.02,"configured-note-step-preview",false);
    check("left_stretch_respects_note_edit_step",near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,1.16+1./64.));
    install(snappedData);edgeDrag(0,true,-.02,"alt-left-exact-step-preview");
    check("alt_left_stretch_remains_exact",near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,1.18));
    install(snappedData);edgeDrag(0,false,.02,"alt-right-exact-step-preview");
    check("alt_right_stretch_remains_exact",near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,1.18));
    install(snappedData);(void)capture();const auto edgeBounds=roll.diagnosticHitBounds(0);
    const auto edgeBegin=juce::Point<float>(edgeBounds.getX()+2,edgeBounds.getCentreY());
    const auto edgeEnd=edgeBegin.translated(-8,0);const auto plain=juce::ModifierKeys::leftButtonModifier;
    const auto beforeReturn=model.revisionNumber();
    roll.mouseDown(event(roll,edgeBegin,edgeBegin,plain));roll.mouseDrag(event(roll,edgeEnd,edgeBegin,plain));(void)capture();
    roll.mouseDrag(event(roll,edgeBegin,edgeBegin,plain));(void)capture();roll.mouseUp(event(roll,edgeBegin,edgeBegin,plain));
    check("default_edge_return_to_origin_is_noop",model.revisionNumber()==beforeReturn
        &&near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,1.16));
    install(snappedData);(void)capture();const auto verticalEdge=juce::Point<float>(roll.diagnosticHitBounds(0).getX()+2,
        roll.diagnosticHitBounds(0).getCentreY());const auto verticalEnd=verticalEdge.translated(0,20);
    const auto beforeVertical=model.revisionNumber();roll.mouseDown(event(roll,verticalEdge,verticalEdge,plain));
    roll.mouseDrag(event(roll,verticalEnd,verticalEdge,plain));roll.mouseUp(event(roll,verticalEnd,verticalEdge,plain));
    check("vertical_edge_drag_does_not_snap_original_timing",model.revisionNumber()==beforeVertical);
    // Timeline-region resizing has a separate path from piano-roll edges.
    // Its implicit consonant clock must remain exact beyond the velocity range.
    auto timeline=data;auto& tc=timeline.tracks[0].clips[0];tc.notes[0].attackSpeed=.5;
    tc.notes[0].amplitudeEnvelope={{0,-12,true},{.6,-3,true},{1.2,-9,true}};
    tc.notes[0].vibratoEnabled=true;tc.notes[0].vibratoReferenceDurationSeconds=1.5;tc.notes[0].vibratoTimeOffsetSeconds=.3;
    tc.gainEnvelope={{0,-3},{.6,-9},{1.2,-3}};
    tc.inheritedGainEnvelopes={{{0,-6},{1.2,0}}};
    const auto timelineClock=nativeClipClock(tc);model.replace(timeline);const auto beforeTimeline=model.contentFingerprint();
    model.resizeClip(tc.id,tc.startSeconds,36);const auto timelineLong=model.snapshot().tracks[0].clips[0];
    check("timeline_long_stretch_duration",near(timelineLong.durationSeconds,36)&&near(timelineLong.notes[0].durationSeconds,36));
    bool exactClock=true;for(double t=0;t<=1.2;t+=.007)
        exactClock &= near(nativeSourceTimeAt(nativeClipClock(timelineLong),t*30),nativeSourceTimeAt(timelineClock,t));
    check("timeline_long_stretch_keeps_consonant_source_anchor",exactClock);
    check("timeline_long_stretch_scales_loudness_handles",near(timelineLong.notes[0].amplitudeEnvelope[1].timeSeconds,18)
        &&near(timelineLong.notes[0].amplitudeEnvelope.back().timeSeconds,36));
    check("timeline_long_stretch_scales_clip_gain_layers",near(timelineLong.gainEnvelope[1].timeSeconds,18)
        &&near(timelineLong.inheritedGainEnvelopes[0].back().timeSeconds,36));
    check("timeline_long_stretch_scales_vibrato_clock",near(timelineLong.notes[0].vibratoReferenceDurationSeconds,45)
        &&near(timelineLong.notes[0].vibratoTimeOffsetSeconds,9));
    model.resizeClip(tc.id,tc.startSeconds,1.2);const auto timelineBack=model.snapshot().tracks[0].clips[0];
    check("timeline_stretch_round_trip_restores_source_and_loudness",near(nativeSourceTimeAt(nativeClipClock(timelineBack),.1),.05)
        &&near(timelineBack.notes[0].amplitudeEnvelope[1].timeSeconds,.6)&&near(timelineBack.notes[0].vibratoTimeOffsetSeconds,.3));
    model.undo();model.undo();check("timeline_long_stretch_undo_restores_original",model.contentFingerprint()==beforeTimeline);
    model.replace(split);model.disconnectNativeAudio({"head",middle,tail});model.linkNativeAudio({"head",middle,tail});
    const auto childBefore=model.snapshot().tracks[0].clips[0];
    model.resizeClip(childBefore.id,childBefore.startSeconds,childBefore.durationSeconds*30);const auto childAfter=model.snapshot().tracks[0].clips[0];
    const auto beforeSources=expandedClipParts(childBefore),afterSources=expandedClipParts(childAfter);bool childClocks=beforeSources.size()==afterSources.size();
    for(std::size_t i=0;childClocks&&i<beforeSources.size();++i)for(double t=0;t<=beforeSources[i].durationSeconds;t+=.013)
        childClocks &= near(nativeSourceTimeAt(nativeClipClock(afterSources[i]),t*30),nativeSourceTimeAt(nativeClipClock(beforeSources[i]),t));
    check("timeline_linked_children_long_stretch_preserves_source_clocks",childClocks);
    std::cout<<"native_note_move_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
