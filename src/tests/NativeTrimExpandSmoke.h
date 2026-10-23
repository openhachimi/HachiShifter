#pragma once
#include "../NativeAudioTrim.h"

namespace hachi
{
inline bool runNativeTrimExpandSmoke(const juce::File& folder)
{
    folder.createDirectory();bool ok=true;int checks=0;
    const auto check=[&](const char* name,bool pass){ok &= pass;++checks;std::cout<<name<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<2.e-6;};
    const auto file=folder.getChildFile("two-words.wav");
    juce::WavAudioFormat wav;auto stream=file.createOutputStream();
    if(stream){stream->setPosition(0);stream->truncate();}
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),48000,1,24,{},0));
    if(!writer)return false;stream.release();juce::AudioBuffer<float> samples(1,96000);
    for(int i=0;i<96000;++i)samples.setSample(0,i,static_cast<float>(.2*std::sin(i*2*juce::MathConstants<double>::pi*(i<48000?220:330)/48000)));
    check("source_written",writer->writeFromAudioSampleBuffer(samples,0,96000));writer.reset();
    ClipData clip;clip.id="clip";clip.sourceFile=file;clip.startSeconds=1;clip.durationSeconds=clip.sourceDurationSeconds=2;
    NoteData note;note.id="word";note.label="two words";note.durationSeconds=2;note.midiNote=note.sourceMidiCenter=57;
    note.sourcePitchMeasured=true;note.utauAutoPitchTransition=false;
    for(int i=0;i<=400;++i)note.contour.push_back({i*.005,i<200?0.f:700.f,i<200?0.f:700.f,true});
    bindNativeNoteSource(note,clip);clip.notes={note};
    clip.nativeSourcePitch=nativeSourcePitchReference(clip.notes,[](double t){return t;});clip.nativeSourcePitchComplete=true;
    TrackData track;track.id="native";track.pitchAlgorithm=PitchAlgorithm::world;track.normalizeVolume=false;track.clips={clip};
    ProjectData initial;initial.tracks={track};ProjectModel model;model.replace(initial);
    check("tail_trim",model.trimNativeNoteEdge("word",-1,true)==false && model.trimNativeNoteEdge("word",-1,false));
    auto cropped=model.snapshot();check("only_first_word_remains",near(cropped.tracks[0].clips[0].sourceDurationSeconds,1));
    check("tail_expands_to_source_end",model.trimNativeNoteEdge("word",4,false));
    auto restored=model.snapshot();auto full=restored.tracks[0].clips[0];
    check("extension_clamped_at_source_file_end",near(full.durationSeconds,2)&&near(full.sourceDurationSeconds,2));
    check("second_word_source_pitch_restored",full.notes[0].contour.back().voiced&&near(full.notes[0].contour.back().relativeCents,700));
    check("restored_audio_is_unedited",nativePitchIsUnedited(full.notes[0]));
    check("cannot_extend_past_file_end",!model.trimNativeNoteEdge("word",.2,false));
    model.undo();check("undo_reverts_extension",near(model.snapshot().tracks[0].clips[0].durationSeconds,1));
    model.redo();check("redo_restores_extension",near(model.snapshot().tracks[0].clips[0].durationSeconds,2));
    model.replace(initial);check("head_trim",model.trimNativeNoteEdge("word",.73123456,true));
    juce::String error;ProjectModel reopened;
    check("cropped_project_saved_and_reopened",model.save(folder.getChildFile("crop.hjpx"),error)&&reopened.load(folder.getChildFile("crop.hjpx"),error));
    const auto saved=reopened.snapshot().tracks[0].clips[0];
    check("full_source_reference_saved",saved.nativeSourcePitch&&saved.nativeSourcePitchComplete&&near(saved.nativeSourcePitch->front().sourceSeconds,0)&&near(saved.nativeSourcePitch->back().sourceSeconds,2));
    check("hidden_negative_clock_saved",!saved.nativeTrimClock.empty()&&saved.nativeTrimClock.front().targetSeconds<0);
    check("head_expands_after_reopen",reopened.trimNativeNoteEdge("word",-9,true));
    const auto head=reopened.snapshot().tracks[0].clips[0];
    check("head_expansion_keeps_absolute_end_and_source_rate",near(head.startSeconds,1)&&near(head.durationSeconds,2)&&near(head.sourceOffsetSeconds,0)&&near(head.sourceDurationSeconds,2));
    check("head_source_pitch_restored",head.notes[0].contour.front().voiced&&near(head.notes[0].contour.front().relativeCents,0));
    model.replace(initial);const auto second=model.splitNote("word",1);
    check("split_into_two_words",second.isNotEmpty());model.removeNotes({second});
    check("removed_word_restorable_by_trim",model.trimNativeNoteEdge("word",1,false));
    auto afterDelete=model.snapshot();check("deleted_word_audio_and_pitch_restored",near(afterDelete.tracks[0].clips[0].notes[0].durationSeconds,2)&&near(afterDelete.tracks[0].clips[0].notes[0].contour.back().relativeCents,700));
    model.replace(initial);(void)model.splitNote("word",1);const auto tailRegion=model.splitClip(clip.id,2);
    check("timeline_region_cut_creates_tail",tailRegion.isNotEmpty());model.removeClip(tailRegion);
    check("cut_and_deleted_region_can_be_exposed_again",model.trimNativeNoteEdge("word",1,false));
    const auto restoredRegion=model.snapshot();const auto& region=restoredRegion.tracks[0].clips[0];
    check("cut_region_restores_absolute_source_and_pitch",near(region.startSeconds,1)&&near(region.sourceOffsetSeconds,0)
        &&near(region.sourceDurationSeconds,2)&&near(region.notes[0].contour.back().relativeCents,700));
    // Independent clips remain collision constrained unless overlap is enabled.
    model.replace(cropped);auto blocked=cropped;auto other=clip;other.id="other";other.startSeconds=2.4;other.durationSeconds=other.sourceDurationSeconds=.4;
    other.notes[0].id="other-note";other.notes[0].durationSeconds=.4;blocked.tracks[0].clips.push_back(other);model.replace(blocked);
    check("outward_trim_stops_at_next_audio",model.trimNativeNoteEdge("word",1,false)&&near(model.snapshot().tracks[0].clips[0].durationSeconds,1.4));
    blocked.tracks[0].allowNativeAudioOverlap=true;model.replace(blocked);
    check("overlap_option_allows_extension",model.trimNativeNoteEdge("word",1,false)&&near(model.snapshot().tracks[0].clips[0].durationSeconds,2));
    // Preserve nonlinear source timing after crop, restore and note stretching.
    auto warped=initial;warped.tracks[0].clips[0].sourceTimeMap={{0,0},{.4,.2},{1.4,1.1},{2,2}};model.replace(warped);
    check("nonlinear_tail_crop",model.trimNativeNoteEdge("word",-.9,false));
    check("nonlinear_tail_restore",model.trimNativeNoteEdge("word",.9,false));
    auto w=model.snapshot().tracks[0].clips[0];bool clockStable=true;
    for(double t=0;t<=2;t+=.017)clockStable &= near(nativeSourceTimeAt(nativeClipClock(w),t),nativeSourceTimeAt(nativeClipClock(warped.tracks[0].clips[0]),t));
    check("nonlinear_source_anchors_restored_without_restretch",clockStable);
    model.replace(initial);model.trimNativeNoteEdge("word",.5,true);
    check("cropped_note_can_be_stretched",model.resizeNativeNoteEdge("word",.5,false));
    const auto stretched=model.snapshot().tracks[0].clips[0];
    check("stretched_crop_can_restore_head",model.trimNativeNoteEdge("word",-.5,true));
    const auto extendedStretch=model.snapshot().tracks[0].clips[0];bool unchangedClock=true;
    for(double t=.01;t<stretched.notes[0].durationSeconds;t+=.019)
        unchangedClock &= near(stretched.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(stretched),t),
            extendedStretch.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(extendedStretch),t+.5));
    check("head_restoration_preserves_previously_stretched_audio_clock",unchangedClock);
    // Actual mouse gesture, including unquantized preview and source bounds.
    model.replace(cropped);I18n strings;PianoRollComponent roll(model,strings);
    roll.setFocusedTrack(track.id);roll.setFocusedClip(clip.id);roll.setTool(PianoRollComponent::Tool::trim);
    roll.setPixelsPerSecond(320);roll.setSize(1600,roll.getHeight());roll.diagnosticRefresh();
    const auto bounds=roll.diagnosticHitBounds(0);const auto down=juce::Point<float>(bounds.getRight()-.5f,bounds.getCentreY());
    const auto to=down.translated(100.25f,0);
    const auto event=[&](juce::Point<float> at){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,
        juce::ModifierKeys::leftButtonModifier,0,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    roll.mouseDown(event(down));roll.mouseDrag(event(to));
    check("outward_preview_does_not_commit",near(model.snapshot().tracks[0].clips[0].durationSeconds,1));
    auto preview=roll.createComponentSnapshot(roll.getLocalBounds());auto png=folder.getChildFile("outward-preview.png").createOutputStream();
    if(png){png->setPosition(0);png->truncate();juce::PNGImageFormat().writeImageToStream(preview,*png);}
    roll.mouseUp(event(to));
    check("outward_mouse_trim_has_fractional_precision",near(model.snapshot().tracks[0].clips[0].notes[0].durationSeconds,1+100.25/320));
    auto legacy=cropped;auto& lc=legacy.tracks[0].clips[0];lc.nativeSourcePitch.reset();lc.nativeSourcePitchComplete=false;lc.nativeTrimClock.clear();model.replace(legacy);
    check("legacy_cropped_project_can_extend",model.trimNativeNoteEdge("word",1,false));
    check("missing_hidden_pitch_requests_background_analysis",model.snapshot().tracks[0].clips[0].nativeSourcePitchPending);
    check("background_analysis_applied",model.setNativeTrimSourceAnalysis(file,{note}));
    const auto analysed=model.snapshot().tracks[0].clips[0];
    check("background_restores_hidden_source_pitch",!analysed.nativeSourcePitchPending&&analysed.nativeSourcePitchComplete&&analysed.notes[0].contour.back().voiced&&near(analysed.notes[0].contour.back().relativeCents,700));
    // Actual output: both words restored at their original positions, untouched.
    AudioEngine engine;engine.prepareToPlay(256,48000);WavExportOptions options;options.sampleRate=48000;options.channels=2;options.bitDepth=32;
    const auto render=[&](const ProjectData& project,const char* name){engine.syncProject(project);for(int i=0;i<1500&&engine.renderProgress();++i)juce::Thread::sleep(10);
        return engine.hasCurrentRenderedAudio()&&engine.exportWav(folder.getChildFile(name),error,track.id,1,3,options);};
    check("original_and_restored_pcm_render",render(initial,"original.wav")&&render(restored,"restored.wav")&&render(restoredRegion,"cut-restored.wav"));
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(folder.getChildFile("original.wav"))),b(formats.createReaderFor(folder.getChildFile("restored.wav")));
    double difference=0,energy=0;bool identical=a&&b;
    if(identical){juce::AudioBuffer<float> x(2,96000),y(2,96000);a->read(&x,0,96000,0,true,true);b->read(&y,0,96000,0,true,true);
        for(int i=512;i<95000;++i){const auto v=x.getSample(0,i),d=v-y.getSample(0,i);difference+=d*d;energy+=v*v;}
        identical=energy>1&&difference/energy<1.e-5;check("restored_second_word_is_audible",y.getRMSLevel(0,55000,35000)>.05f);}
    check("restored_pcm_matches_original_without_pitch_or_speed_change",identical);engine.releaseResources();
    std::unique_ptr<juce::AudioFormatReader> c(formats.createReaderFor(folder.getChildFile("cut-restored.wav")));
    bool cutIdentical=a&&c;double cutDifference=0;
    if(cutIdentical){juce::AudioBuffer<float> x(2,96000),y(2,96000);a->read(&x,0,96000,0,true,true);c->read(&y,0,96000,0,true,true);
        for(int i=512;i<95000;++i){const auto d=x.getSample(0,i)-y.getSample(0,i);cutDifference+=d*d;}
        cutIdentical=cutDifference/std::max(energy,1.e-12)<1.e-5;}
    check("cut_deleted_restored_pcm_matches_original",cutIdentical);
    std::cout<<"render_relative_error="<<difference/std::max(energy,1.e-12)<<std::endl;
    std::cout<<"native_trim_expand_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
