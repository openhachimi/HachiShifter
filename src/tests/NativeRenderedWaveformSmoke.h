#pragma once
#include "../SourceWaveformPreview.h"
namespace hachi
{
inline bool MainComponent::diagnosticNativeRenderedWaveform(const juce::File& folder,const juce::File& modelDirectory)
{
    folder.createDirectory();stopTimer();bool ok=true;
    const auto check=[&](const char* name,bool pass){ok=ok&&pass;std::cout<<name<<'='<<pass<<std::endl;};
    const auto save=[&](const char* name,const juce::Image& image)
    {auto stream=folder.getChildFile(juce::String(name)+".png").createOutputStream();if(!stream)return false;
        stream->setPosition(0);stream->truncate();return juce::PNGImageFormat().writeImageToStream(image,*stream);};
    juce::PropertySet settings;const auto defaults=viewOptionsFrom(settings);
    check("real_waveform_is_an_optional_view",!defaults.nativeRenderedWaveform&&defaults.nativeWaveform);
    const auto toggled=afterViewMenuChoice(defaults,5,false);
    check("real_waveform_toggle_does_not_change_source_toggle",toggled.nativeRenderedWaveform&&toggled.nativeWaveform);
    storeViewOptions(settings,toggled);check("real_waveform_option_persists",viewOptionsFrom(settings).nativeRenderedWaveform);
    check("native_menu_has_real_waveform_option",viewMenuItemEnabled(5,false)&&!viewMenuItemEnabled(5,true));
    juce::AudioBuffer<float> precise(2,97);precise.clear();precise.setSample(0,0,-.0004f);
    precise.setSample(1,31,.75f);precise.setSample(0,32,-.2f);precise.setSample(1,96,.33f);
    const auto exact=measureNativeRenderedPeaks(precise,48000);
    check("float_peaks_keep_quiet_samples_and_stereo",exact->minima.size()==4&&exact->minima[0]==-.0004f
        &&exact->maxima[0]==.75f&&exact->minima[1]==-.2f&&exact->maxima[3]==.33f);
    check("silent_buckets_are_exactly_zero",exact->minima[2]==0&&exact->maxima[2]==0);
    const auto source=folder.getChildFile("native-source.wav");
    juce::AudioBuffer<float> buffer(1,48000);
    for(int i=0;i<48000;++i){const auto t=i/48000.0;const auto level=.84*std::exp(-std::pow((t-.22)/.075,2))
        +.76*std::exp(-std::pow((t-.72)/.12,2));buffer.setSample(0,i,static_cast<float>(level*std::sin(2*juce::MathConstants<double>::pi*440*t)));}
    juce::WavAudioFormat format;auto out=source.createOutputStream();if(out){out->setPosition(0);out->truncate();}
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(out.get(),48000,1,24,{},0));
    if(!writer){check("source_written",false);return false;}out.release();writer->writeFromAudioSampleBuffer(buffer,0,48000);writer.reset();
    ClipData clip;clip.id="actual-clip";clip.sourceFile=source;clip.startSeconds=.5;clip.sourceDurationSeconds=1;clip.durationSeconds=1;
    NoteData first;first.id="first";first.label="a";first.midiNote=72;first.sourceMidiCenter=69;first.durationSeconds=.5;
    first.consonantSeconds=0;first.gain=.65f;first.contour={{0,0,0,true},{.5,0,0,true}};first.amplitudeEnvelope={{0,0},{.5,0}};
    auto second=first;second.id="second";second.label="i";second.startSeconds=.5;second.midiNote=69;second.gain=.18f;
    clip.notes={first,second};TrackData track;track.id="native-track";track.name="Native rendered";track.compose=true;
    track.pitchAlgorithm=PitchAlgorithm::world;track.normalizeVolume=false;track.volume=.1f;track.clips={clip};ProjectData data;data.tracks={track};
    ProjectModel project;project.replace(data);PianoRollComponent roll(project,strings);roll.setFocusedTrack(track.id);roll.setFocusedClip(clip.id);
    roll.setPixelsPerSecond(400);roll.setRowHeight(26);roll.setSize(800,roll.getHeight());
    TimelineComponent lane(project);lane.setPixelsPerSecond(400);lane.setRowHeight(110);lane.setShowNativeRenderedWaveforms(true);
    roll.setShowNativeRenderedWaveforms(true);juce::Thread::sleep(120);
    const auto capture=[&]{return roll.createComponentSnapshot({240,static_cast<int>(roll.diagnosticNoteY(0))-55,430,200});};
    const auto blue=[](const juce::Image& image){int count=0;for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x)
        {const auto c=image.getPixelAt(x,y);if(c.getRed()>130&&c.getBlue()>c.getRed()+15&&c.getGreen()>c.getRed()+8&&c.getBlue()>120)++count;}return count;};
    const auto before=capture();check("pending_render_keeps_warm_source_preview",blue(before)==0);
    AudioEngine engine;engine.prepareToPlay(256,48000);engine.syncProject(data);
    const auto wait=[&]{for(int i=0;i<1000&&engine.renderProgress();++i)juce::Thread::sleep(10);engine.refreshNativeWaveforms();};
    wait();auto waveforms=engine.nativeClipWaveforms();
    check("real_native_render_completed",engine.hasCurrentRenderedAudio()&&waveforms&&waveforms->size()==1);
    if(!waveforms||waveforms->size()!=1){std::cout<<engine.activeRenderWarnings()<<std::endl;return false;}
    const auto& rendered=waveforms->front();const auto& peaks=*rendered.peaks;
    check("native_snapshot_uses_current_clip_hash",rendered.audioHash==AudioEngine::nativeClipWaveformHash(clip,track));
    check("real_waveform_uses_decoder_sample_grid",peaks.sampleRate==48000&&std::abs(peaks.sampleCount-48000)<=1);
    const auto raw=measureNativeRenderedPeaks(buffer,48000);
    const auto windowPeak=[](const NativeRenderedPeaks& wave,double from,double to)
    {float peak=0;const auto a=static_cast<int>(from*wave.sampleRate/NativeRenderedPeaks::samplesPerBucket);
        const auto b=std::min(static_cast<int>(wave.maxima.size()),static_cast<int>(std::ceil(to*wave.sampleRate/NativeRenderedPeaks::samplesPerBucket)));
        for(int i=a;i<b;++i)peak=std::max({peak,std::abs(wave.minima[static_cast<std::size_t>(i)]),std::abs(wave.maxima[static_cast<std::size_t>(i)])});return peak;};
    check("real_waveform_contains_note_gain_processing",windowPeak(peaks,.6,.85)<windowPeak(*raw,.6,.85)*.5f
        &&windowPeak(peaks,.1,.3)>.05f);
    // Low track gain keeps the output limiter inactive for this sample comparison.
    WavExportOptions exportOptions;exportOptions.sampleRate=48000;exportOptions.channels=2;exportOptions.bitDepth=32;
    juce::String exportError;const auto exportedFile=folder.getChildFile("actual-render.wav");
    const auto exported=engine.exportWav(exportedFile,exportError,track.id,.5,1.5,exportOptions);
    check("real_native_audio_exported",exported);
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(exportedFile));
    bool audioMatches=exported&&reader&&reader->sampleRate==48000;float largestError=0;
    if(audioMatches)
    {
        juce::AudioBuffer<float> exportedAudio(2,static_cast<int>(reader->lengthInSamples));
        reader->read(&exportedAudio,0,exportedAudio.getNumSamples(),0,true,true);
        for(std::size_t b=100;b+100<peaks.maxima.size();++b)
        {
            float low=0,high=0;
            const auto begin=static_cast<int>(b)*NativeRenderedPeaks::samplesPerBucket;
            const auto end=std::min(begin+NativeRenderedPeaks::samplesPerBucket,exportedAudio.getNumSamples());
            for(int i=begin;i<end;++i){const auto value=exportedAudio.getSample(0,i)/(.1f*std::sqrt(.5f));low=std::min(low,value);high=std::max(high,value);}
            largestError=std::max({largestError,std::abs(low-peaks.minima[b]),std::abs(high-peaks.maxima[b])});
        }
        audioMatches=largestError<.0001f;
    }
    check("cached_waveform_matches_exported_audio_samples",audioMatches);
    std::cout<<"export_peak_error="<<largestError<<std::endl;
    const auto same=engine.nativeClipWaveforms();engine.refreshNativeWaveforms();
    check("unchanged_timer_reuses_snapshot",engine.nativeClipWaveforms()==same);
    roll.setNativeClipWaveforms(waveforms);lane.setNativeClipWaveforms(waveforms);const auto actual=capture();
    check("actual_note_blocks_are_pale_blue",blue(actual)>100);
    check("actual_note_demo_written",save("actual-native-notes",actual));
    check("actual_upper_demo_written",save("actual-native-timeline",lane.createComponentSnapshot({0,0,800,lane.getHeight()})));
    roll.setShowNativeWaveforms(false);check("actual_waveform_can_show_without_source_preview",blue(capture())>100);
    roll.setShowNativeRenderedWaveforms(false);check("actual_waveform_switch_hides_blue_blocks",blue(capture())==0);
    roll.setShowNativeRenderedWaveforms(true);roll.setShowNativeWaveforms(true);
    project.transposeNote(first.id,1);project.dispatchPendingMessages();roll.diagnosticRefresh();
    check("pitch_edit_hides_stale_real_waveform",blue(capture())==0);
    const auto changed=project.snapshot();const auto changedHash=AudioEngine::nativeClipWaveformHash(changed.tracks[0].clips[0],changed.tracks[0]);
    engine.syncProject(changed);engine.refreshNativeWaveforms();const auto pending=engine.nativeClipWaveforms();
    check("old_playback_fallback_not_published_as_new_waveform",!pending||std::all_of(pending->begin(),pending->end(),[&](const auto& wave){return wave.audioHash==changedHash;}));
    wait();roll.setNativeClipWaveforms(engine.nativeClipWaveforms());check("rerender_restores_blue_waveform",blue(capture())>100);
    project.setNoteGain(first.id,.25f);project.dispatchPendingMessages();roll.diagnosticRefresh();check("note_gain_edit_hides_stale_waveform",blue(capture())==0);
    auto moved=changed;moved.tracks[0].clips[0].startSeconds=1.5;engine.syncProject(moved);engine.refreshNativeWaveforms();
    const auto movedWaves=engine.nativeClipWaveforms();check("moving_clip_rebinds_ready_waveform",movedWaves&&movedWaves->size()==1
        &&movedWaves->front().audioHash==AudioEngine::nativeClipWaveformHash(moved.tracks[0].clips[0],moved.tracks[0])&&!engine.renderProgress());
    auto merged=clip;merged.notes[0].clipPartId="a";merged.notes[1].clipPartId="b";
    auto partA=clip;partA.id="a";partA.startSeconds=0;partA.durationSeconds=.5;partA.sourceDurationSeconds=.5;partA.sourceTimeMap.clear();
    auto partB=partA;partB.id="b";partB.startSeconds=.5;partB.sourceOffsetSeconds=.5;merged.parts={partA,partB};
    const auto parts=expandedClipParts(merged);const auto views=expandedClipParts(merged,true);bool matches=parts.size()==2&&views.size()==2;
    for(std::size_t i=0;i<parts.size();++i)matches=matches&&AudioEngine::nativeClipWaveformHash(parts[i],track)==AudioEngine::nativeClipWaveformHash(views[i],track);
    check("merged_parts_share_render_and_view_identity",matches);
    project.replace(data);project.dispatchPendingMessages();roll.diagnosticRefresh();roll.setNativeClipWaveforms(waveforms);
    check("undo_compatible_identity_restores_real_waveform",blue(capture())>100);
    project.replace(data);project.dispatchPendingMessages();
    const auto rightId=project.splitClip(clip.id,clip.startSeconds+.5);project.dispatchPendingMessages();roll.diagnosticRefresh();
    engine.syncProject(project.snapshot());wait();const auto splitWaves=engine.nativeClipWaveforms();
    check("split_regions_publish_separate_real_waveforms",rightId.isNotEmpty()&&splitWaves&&splitWaves->size()==2);
    roll.setNativeClipWaveforms(splitWaves);roll.setFocusedClip(clip.id);
    check("real_waveform_shows_other_native_regions_by_default",blue(roll.createComponentSnapshot({500,static_cast<int>(roll.diagnosticNoteY(1))-40,140,90}))>50);
    check("split_native_waveform_preview_written",save("actual-native-split-regions",capture()));
    roll.setFocusedClip(rightId);check("other_split_region_can_show_real_waveform",blue(roll.createComponentSnapshot({490,static_cast<int>(roll.diagnosticNoteY(1))-40,150,90}))>50);
    if(modelDirectory.isDirectory())
    {
        auto joined=data;auto& t=joined.tracks[0];t.pitchAlgorithm=PitchAlgorithm::nsfHifigan;t.renderOrder=RenderOrder::stretchSpliceThenPitch;
        auto a=clip;a.id="joined-a";a.durationSeconds=.5;a.sourceDurationSeconds=.5;a.notes={first};a.notes[0].connectedToNext=true;a.glideConnectedToNext=true;
        auto b=clip;b.id="joined-b";b.startSeconds=1;b.sourceOffsetSeconds=.5;b.durationSeconds=.5;b.sourceDurationSeconds=.5;
        b.notes={second};b.notes[0].startSeconds=0;b.notes[0].connectedToPrevious=true;b.glideConnectedFromPrevious=true;t.clips={a,b};
        engine.setHifiganModelDirectory(modelDirectory);engine.setInferenceConfiguration(backend::InferenceBackend::cpu,-1);engine.syncProject(joined);
        for(int i=0;i<6000&&engine.renderProgress();++i)juce::Thread::sleep(10);engine.refreshNativeWaveforms();
        const auto joinedWaves=engine.nativeClipWaveforms();
        check("native_nsf_merged_phrase_completed",engine.diagnosticMergedPhraseCount()==1&&engine.hasCurrentRenderedAudio());
        bool allSlices=joinedWaves&&joinedWaves->size()==2;
        if(allSlices)for(const auto& c:t.clips)
        {
            const auto hash=AudioEngine::nativeClipWaveformHash(c,t);
            const auto found=std::find_if(joinedWaves->begin(),joinedWaves->end(),[&](const auto& wave){return wave.audioHash==hash;});
            allSlices=allSlices&&found!=joinedWaves->end();
            if(found!=joinedWaves->end())allSlices=allSlices&&found->peaks->sampleCount==24000;
        }
        check("native_nsf_merged_slices_have_real_peaks",allSlices);
        engine.syncProject(joined);engine.refreshNativeWaveforms();
        const auto cached=engine.nativeClipWaveforms();
        check("cached_merged_phrase_republishes_ready_slices",cached&&cached->size()==2&&!engine.renderProgress());
    }
    // Visual QA: the hollow native range/envelope must remain legible over
    // both source preview and freshly rendered audio, including selection.
    auto appearance=data;appearance.tracks[0].clips[0].notes[0].amplitudeEnvelope={{0,-60},{.06,0},{.38,0},{.5,-60}};
    appearance.tracks[0].clips[0].notes[0].nativeSegments={{"onset","a",NativeSegmentRole::vowel,0,.5}};
    project.replace(appearance);project.dispatchPendingMessages();roll.diagnosticRefresh();
    roll.setShowEnvelope(true);roll.setShowNoteRange(true);roll.setShowPitchLine(true);
    roll.setShowNativeRenderedWaveforms(false);
    check("native_hollow_source_preview_written",save("native-hollow-source",capture()));
    engine.syncProject(appearance);wait();roll.setNativeClipWaveforms(engine.nativeClipWaveforms());
    roll.setShowNativeRenderedWaveforms(true);
    check("native_hollow_rendered_preview_written",save("native-hollow-rendered",capture()));
    roll.setSelectedNoteIds({first.id});
    check("native_selected_outline_preview_written",save("native-hollow-selected",capture()));
    roll.setTool(PianoRollComponent::Tool::amplitude);
    check("native_envelope_tool_preview_written",save("native-hollow-amplitude",capture()));
    // Native movement previews must not reuse the old audio or invalidate
    // the stable cache addresses when the pointer returns to its start.
    roll.setTool(PianoRollComponent::Tool::note);
    const auto blueBefore=blue(capture());const auto revisionBefore=project.revisionNumber();
    check("rendered_preview_available_before_move",blueBefore>100);
    roll.diagnosticBeginMoveDrag(first.id,.1);
    const auto moving=capture();
    // Selected outline anti-aliasing can contain a few pale blue pixels;
    // the rendered waveform occupied hundreds of interior pixels.
    std::cout<<"rendered_blue_before="<<blueBefore<<"; preview_blue="<<blue(moving)<<std::endl;
    check("time_drag_hides_stale_rendered_waveform",blue(moving)<blueBefore/4);
    check("native_timing_preview_written",save("native-timing-move-preview",moving));
    roll.diagnosticBeginMoveDrag(first.id,0);
    check("returning_pointer_recovers_rendered_preview",blue(capture())>100);
    roll.diagnosticReleaseDrag();
    check("preview_restores_stable_waveform_cache",blue(capture())==blueBefore);
    check("returning_drag_to_origin_does_not_create_edit",project.revisionNumber()==revisionBefore);
    const auto clipOther=folder.getChildFile("test.hjpx");juce::String saveError;check("view_option_does_not_modify_project_data",project.save(clipOther,saveError));
    std::cout<<"native_rendered_waveform_ok="<<ok<<std::endl;return ok;
}
}
