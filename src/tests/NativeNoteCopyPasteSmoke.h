#pragma once
#include "../NativeAudioClipboard.h"
namespace hachi
{
inline bool MainComponent::diagnosticNativeNoteCopyPaste(const juce::File& folder)
{
    folder.createDirectory();stopTimer();bool ok=true;int checks=0;
    const auto check=[&](const char* name,bool pass){ok &= pass;++checks;std::cout<<name<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-5;};
    const auto source=folder.getChildFile("edited-source.wav");juce::AudioBuffer<float> samples(1,57600);
    for(int i=0;i<samples.getNumSamples();++i){const auto t=i/48000.;samples.setSample(0,i,
        static_cast<float>((.22+.06*std::sin(t*8))*(std::sin(t*2*juce::MathConstants<double>::pi*220)
            +.2*std::sin(t*2*juce::MathConstants<double>::pi*440))));}
    juce::WavAudioFormat format;auto stream=source.createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),48000,1,24,{},0));
    if(!writer)return false;stream.release();check("source_written",writer->writeFromAudioSampleBuffer(samples,0,57600));writer.reset();
    ClipData clip;clip.id="source";clip.sourceFile=source;clip.startSeconds=2;clip.durationSeconds=clip.sourceDurationSeconds=1.2;
    clip.gain=.7f;clip.gainEnvelope={{0,-2},{.6,0},{1.2,-1}};
    NoteData n;n.id="head";n.label="baip";n.durationSeconds=1.2;n.midiNote=n.sourceMidiCenter=57;n.sourcePitchMeasured=true;
    n.nativeSourceStartSeconds=0;n.nativeSourceEndSeconds=1.2;n.amplitudeEnvelope={{0,-4},{.5,0},{1.2,-3}};
    n.nativeSegments={{"c","baip",NativeSegmentRole::consonant,0,.1},{"v","baip",NativeSegmentRole::vowel,.1,1.2}};
    for(int i=0;i<=240;++i)n.contour.push_back({i*.005,0,0,true});clip.notes={n};
    TrackData track;track.id="native";track.name="native";track.pitchAlgorithm=PitchAlgorithm::world;track.normalizeVolume=false;track.clips={clip};
    TrackData empty=track;empty.id="destination";empty.name="destination";empty.clips.clear();ProjectData data;data.tracks={track,empty};project.replace(data);
    const auto middle=project.splitNote("head",.3);const auto tail=project.splitNote(middle,.6);
    check("three_native_segments_created",middle.isNotEmpty()&&tail.isNotEmpty());
    check("head_moved_and_middle_stretched",project.moveNativeNotes({"head"},-.2,1));
    check("middle_stretch_moves_tail",project.resizeNativeNoteEdge(middle,.5,false));
    check("tail_stretched",project.resizeNativeNoteEdge(tail,.3,false));
    project.flattenNotePitch({middle});project.setNotesConnection({"head",middle,tail},true);
    project.dispatchPendingMessages();focusClip(clip.id);pianoRoll.setSelectedNoteIds({"head",middle,tail});
    const auto adjusted=project.snapshot();const auto original=adjusted.tracks[0].clips[0];
    const auto copyKey=juce::KeyPress('C',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0);
    check("ctrl_c_copies_audio_and_clock",keyPressed(copyKey)&&copiedNativeClips.size()==1&&copiedNativeClips[0].notes.size()==3);
    check("paste_menu_enabled",diagnosticEditMenuItemEnabled(18).value_or(false));
    pasteCopiedNotes(6);project.dispatchPendingMessages();const auto pastedData=project.snapshot();
    check("paste_after_original_audio_end_creates_region",pastedData.tracks[0].clips.size()==2);
    if(pastedData.tracks[0].clips.size()!=2)return false;
    const auto pasted=pastedData.tracks[0].clips.back();
    const auto sameNotes=[&](const ClipData& a,const ClipData& b){
        if(a.notes.size()!=b.notes.size())return false;
        for(std::size_t i=0;i<a.notes.size();++i){const auto& x=a.notes[i];const auto& y=b.notes[i];
            if(!near(x.startSeconds,y.startSeconds)||!near(x.durationSeconds,y.durationSeconds)
                ||!near(x.midiNote,y.midiNote)||!near(x.sourceMidiCenter,y.sourceMidiCenter)
                ||x.label!=y.label||x.nativeSegments.size()!=y.nativeSegments.size()
                ||x.contour.size()!=y.contour.size()||x.amplitudeEnvelope.size()!=y.amplitudeEnvelope.size())return false;
            for(std::size_t j=0;j<x.contour.size();++j)if(!near(x.contour[j].timeSeconds,y.contour[j].timeSeconds)
                ||!near(x.contour[j].relativeCents,y.contour[j].relativeCents)
                ||x.contour[j].hasManualTarget!=y.contour[j].hasManualTarget
                ||!near(x.contour[j].manualTargetCents,y.contour[j].manualTargetCents))return false;
            for(std::size_t j=0;j<x.amplitudeEnvelope.size();++j)if(!near(x.amplitudeEnvelope[j].timeSeconds,y.amplitudeEnvelope[j].timeSeconds)
                ||!near(x.amplitudeEnvelope[j].gainDb,y.amplitudeEnvelope[j].gainDb))return false;
        }return true;};
    check("all_split_timing_pitch_and_envelopes_preserved",sameNotes(original,pasted));
    bool clockMatches=near(original.durationSeconds,pasted.durationSeconds)&&near(original.sourceOffsetSeconds,pasted.sourceOffsetSeconds)
        &&near(original.sourceDurationSeconds,pasted.sourceDurationSeconds);
    for(double t=0;t<original.durationSeconds;t+=.005)clockMatches &= near(nativeSourceTimeAt(nativeClipClock(original),t),nativeSourceTimeAt(nativeClipClock(pasted),t));
    check("full_nonlinear_stretch_clock_preserved",clockMatches);
    check("pasted_note_and_clip_ids_are_new",pasted.id!=original.id&&pasted.notes[0].id!=original.notes[0].id);
    check("pasted_notes_selected",pianoRoll.selectedNoteIds().size()==3&&selectedClipId==pasted.id);
    check("internal_connections_remapped",pastedData.nativeConnections.size()==adjusted.nativeConnections.size()*2);
    const auto beforeTarget=AudioEngine::diagnosticNativeTargetMidi(pastedData,0,0),afterTarget=AudioEngine::diagnosticNativeTargetMidi(pastedData,0,1);
    check("renderer_target_frames_match_original",beforeTarget==afterTarget);
    AudioEngine engine;engine.prepareToPlay(256,48000);engine.syncProject(pastedData);
    for(int i=0;i<1000&&engine.renderProgress();++i)juce::Thread::sleep(10);
    WavExportOptions options;options.sampleRate=48000;options.channels=2;options.bitDepth=32;juce::String error;
    const auto originalWav=folder.getChildFile("original-adjusted.wav"),copyWav=folder.getChildFile("pasted-adjusted.wav");
    const auto exported=engine.hasCurrentRenderedAudio()
        &&engine.exportWav(originalWav,error,"native",original.startSeconds,original.startSeconds+original.durationSeconds,options)
        &&engine.exportWav(copyWav,error,"native",6,6+pasted.durationSeconds,options);
    check("original_and_pasted_audio_render_and_export",exported);
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(originalWav)),b(formats.createReaderFor(copyWav));
    // Export ranges use floor/ceil at different absolute positions; one
    // boundary sample may differ even though the source clock is identical.
    bool soundsSame=a&&b&&std::abs(a->lengthInSamples-b->lengthInSamples)<=1;float maximum=1;
    if(soundsSame){const auto count=static_cast<int>(std::min(a->lengthInSamples,b->lengthInSamples));
        juce::AudioBuffer<float> x(2,count),y(2,count);
        a->read(&x,0,x.getNumSamples(),0,true,true);b->read(&y,0,y.getNumSamples(),0,true,true);maximum=0;
        for(int ch=0;ch<2;++ch)for(int i=0;i<x.getNumSamples();++i)maximum=std::max(maximum,std::abs(x.getSample(ch,i)-y.getSample(ch,i)));
        soundsSame=x.getRMSLevel(0,0,x.getNumSamples())>.002&&maximum<1.e-4f;}
    check("pasted_audio_matches_adjusted_source",soundsSame);std::cout<<"audio_max_difference="<<maximum<<std::endl;engine.releaseResources();
    const auto saved=folder.getChildFile("copied-native.hjpx");check("copied_project_saved",project.save(saved,error));ProjectModel reopened;
    check("copied_project_reopens",reopened.load(saved,error));const auto savedData=reopened.snapshot();
    check("saved_copy_keeps_audio_clock_and_notes",savedData.tracks[0].clips.size()==2
        &&sameNotes(savedData.tracks[0].clips[1],pasted)&&savedData.tracks[0].clips[1].sourceFile==source);
    project.undo();check("one_undo_removes_whole_paste",project.snapshot().tracks[0].clips.size()==1
        &&project.snapshot().nativeConnections.size()==adjusted.nativeConnections.size());
    project.transposeNotes({"head",middle,tail},12);selectedTrackId="destination";pasteCopiedNotes(9);project.dispatchPendingMessages();
    const auto across=project.snapshot();check("paste_to_empty_native_track_works",across.tracks[1].clips.size()==1);
    check("clipboard_is_snapshot_before_source_was_changed",across.tracks[1].clips.size()==1&&sameNotes(across.tracks[1].clips[0],original));
    project.replace(adjusted);project.dispatchPendingMessages();focusClip(clip.id);pianoRoll.setSelectedNoteIds({middle});copySelectedNotes(false);pasteCopiedNotes(7);
    project.dispatchPendingMessages();const auto one=project.snapshot().tracks[0].clips.back();
    check("single_middle_copy_uses_only_middle_audio",one.notes.size()==1&&near(one.notes[0].durationSeconds,original.notes[1].durationSeconds)
        &&near(one.sourceOffsetSeconds,original.notes[1].nativeSourceStartSeconds)&&near(one.notes[0].startSeconds,0));
    bool middleClock=true;for(double t=0;t<one.durationSeconds;t+=.005)middleClock &= near(one.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(one),t),
        original.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(original),original.notes[1].startSeconds+t));
    check("middle_copy_retains_original_source_mapping",middleClock);
    project.replace(adjusted);project.dispatchPendingMessages();focusClip(clip.id);pianoRoll.setSelectedNoteIds({"head",tail});copySelectedNotes(false);
    check("noncontiguous_selection_excludes_middle_audio",copiedNativeClips.size()==2&&copiedNativeClips[0].notes.size()==1&&copiedNativeClips[1].notes.size()==1);
    pasteCopiedNotes(8);project.dispatchPendingMessages();const auto sparse=project.snapshot();
    check("noncontiguous_paste_preserves_gap",sparse.tracks[0].clips.size()==3
        &&near(sparse.tracks[0].clips[2].startSeconds-sparse.tracks[0].clips[1].startSeconds,original.notes[2].startSeconds));
    project.undo();check("one_undo_removes_multiple_audio_regions",project.snapshot().tracks[0].clips.size()==1);
    const auto direct=project.duplicateNotes({"head",middle,tail},clip.id,12);check("model_duplicate_notes_carries_native_audio",direct.size()==3&&project.snapshot().tracks[0].clips.size()==2);
    auto utau=adjusted;utau.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;project.replace(utau);project.dispatchPendingMessages();focusClip(clip.id);
    pianoRoll.setSelectedNoteIds({"head"});copySelectedNotes(false);check("utau_copy_clears_native_clipboard",copiedNativeClips.empty()&&copiedNativeConnections.empty()&&copiedNotes.size()==1);
    // A representative ordinary UTAU fixture for its existing ripple/replace check.
    juce::String ustText="[#VERSION]\nUST Version1.2\n[#SETTING]\nTempo=120\nTracks=1\n";
    for(int i=0;i<40;++i)ustText+="[#"+juce::String(i).paddedLeft('0',4)+"]\nLength=480\nLyric=a\nNoteNum=60\nIntensity=100\n";
    ustText+="[#TRACKEND]\n";const auto ust=folder.getChildFile("utau-paste.ust");
    juce::StringArray warnings;ProjectModel ordinary;
    check("ordinary_utau_fixture_saved",ust.replaceWithText(ustText)&&ordinary.addUstFile(ust,error,warnings)
        &&ordinary.save(folder.getChildFile("utau-paste.hjpx"),error));
    std::cout<<"native_note_copy_paste_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
