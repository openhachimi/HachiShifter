#pragma once
#include "../SourceWaveformPreview.h"
namespace hachi
{
inline bool MainComponent::diagnosticNativeWaveformPreview(const juce::File& folder)
{
    folder.createDirectory(); stopTimer(); bool ok = true;
    const auto check = [&](const char* name, bool pass)
    { ok = ok && pass; std::cout << name << '=' << pass << std::endl; };
    const auto near = [](double a, double b) { return std::abs(a-b)<1.0e-7; };
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    const auto source = folder.getChildFile("pulses.wav");
    juce::AudioBuffer<float> samples(1, 48000); samples.clear();
    for (int i=0; i<48000; ++i)
    {
        const auto t = i / 48000.0;
        if ((t >= .145 && t <= .155) || (t >= .645 && t <= .655))
            samples.setSample(0, i, .9f * std::sin(static_cast<float>(i)*.13f));
    }
    auto stream = source.createOutputStream(); if(stream){stream->setPosition(0);stream->truncate();} juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),48000,1,16,{},0));
    if (!writer) { check("fixture_written",false); return false; }
    stream.release(); check("fixture_written",writer->writeFromAudioSampleBuffer(samples,0,48000)); writer.reset();
    juce::AudioThumbnailCache cache(4); juce::AudioThumbnail thumb(nativeWaveformSamplesPerPeak,formats,cache);
    thumb.setSource(new juce::FileInputSource(source));
    for (int i=0; i<400 && !thumb.isFullyLoaded(); ++i) juce::Thread::sleep(5);
    check("background_source_peaks_loaded", thumb.isFullyLoaded());
    ClipData clip; clip.id="native"; clip.sourceFile=source; clip.startSeconds=.5;
    clip.sourceDurationSeconds=1; clip.durationSeconds=2;
    clip.sourceTimeMap={{0,0},{1,.25},{2,1}};
    NoteData note;note.id="note";note.label="a";note.midiNote=60;note.durationSeconds=2;
    clip.notes={note};
    const auto map=nativeSourceTimeMap(clip);
    check("manual_map_matches_first_peak",near(nativeSourceTimeAt(map,.6),.15));
    check("manual_map_matches_second_peak",near(nativeSourceTimeAt(map,1+(.65-.25)/.75),.65));
    auto automatic=clip;automatic.sourceTimeMap.clear();automatic.notes[0].consonantSeconds=.2;
    automatic.notes[0].attackSpeed=2;
    const auto attack=nativeSourceTimeMap(automatic);
    check("consonant_speed_uses_renderer_anchors",near(nativeSourceTimeAt(attack,.2),.4));
    const auto picture=[&](ClipData c, int width=400)
    {
        juce::Image image(juce::Image::RGB,width,120,true);juce::Graphics g(image);g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);const auto audio=nativeAudioPreviewClip(c);
        drawNativeSourceWaveform(g,thumb,audio,nativeSourceTimeMap(audio),{0,0,static_cast<float>(width),120});
        return image;
    };
    const auto ink=[](const juce::Image& image,int begin,int end)
    {int count=0;for(int y=0;y<image.getHeight();++y)for(int x=begin;x<end;++x)
        if(image.getPixelAt(x,y)!=juce::Colours::black)++count;return count;};
    const auto save=[&](juce::String name,const juce::Image& image)
    {auto out=folder.getChildFile(name+".png").createOutputStream();if(out){out->setPosition(0);out->truncate();}return out&&juce::PNGImageFormat().writeImageToStream(image,*out);};
    const auto warped=picture(clip);check("warped_image_written",save("warped-source",warped));
    check("warped_peaks_at_target_times",ink(warped,116,125)>100 && ink(warped,303,312)>100);
    check("old_linear_positions_empty",ink(warped,56,65)==0 && ink(warped,256,265)==0);
    int gold=0,red=0;for(int y=0;y<warped.getHeight();++y)for(int x=0;x<warped.getWidth();++x)
    {auto pixel=warped.getPixelAt(x,y);if(pixel.getRed()>200&&pixel.getGreen()>160&&pixel.getBlue()<150)++gold;
        if(pixel.getRed()>80&&pixel.getRed()>pixel.getGreen()*1.8&&pixel.getRed()>pixel.getBlue()*1.3)++red;}
    check("warm_gradient_has_yellow_core_and_red_edge",gold>20&&red>20);
    auto offset=clip;offset.sourceOffsetSeconds=.5;offset.sourceDurationSeconds=.5;offset.durationSeconds=1;offset.sourceTimeMap.clear();offset.notes[0].consonantSeconds=0;
    const auto shifted=picture(offset);check("source_offset_keeps_correct_peak",ink(shifted,115,127)>100 && ink(shifted,310,325)==0);
    auto padded=clip;padded.durationSeconds=3;padded.audioStartSeconds=.4;padded.audioDurationSeconds=2;
    for(auto& n:padded.notes)n.startSeconds+=.4;
    for(auto& point:padded.sourceTimeMap)point.targetSeconds+=.4;
    const auto canonical=nativeAudioPreviewClip(padded);
    check("edge_extension_keeps_empty_audio_padding",near(canonical.startSeconds,.9)&&near(canonical.durationSeconds,2)
        &&near(canonical.notes[0].startSeconds,0)&&near(canonical.sourceTimeMap[1].targetSeconds,1));
    check("edge_extension_does_not_change_stretch",ink(picture(padded),116,125)==ink(warped,116,125));
    const auto compressed=picture(clip,20);check("zoom_out_preserves_peaks",ink(compressed,0,20)>20);
    auto malformed=clip;malformed.sourceTimeMap={{2,1},{1,.25},{1,.2},{0,0},{.5,.1}};
    const auto sorted=nativeSourceTimeMap(malformed);
    check("unordered_duplicate_anchors_normalized",near(nativeSourceTimeAt(sorted,1),.25));
    TrackData track;track.id="track";track.name="Native";track.compose=true;track.pitchAlgorithm=PitchAlgorithm::mld5;
    track.clips={clip};ProjectData data;data.tracks={track};ProjectModel project;project.replace(data);
    PianoRollComponent roll(project,strings);roll.setFocusedTrack(track.id);roll.setFocusedClip(clip.id);
    roll.setPixelsPerSecond(200);roll.setRowHeight(20);roll.setSize(800,roll.getHeight());
    TimelineComponent lane(project);lane.setPixelsPerSecond(200);lane.setRowHeight(110);
    // Component thumbnails are background jobs too; no neural engine is requested.
    juce::Thread::sleep(250);
    const auto capture=[&] {return roll.createComponentSnapshot({0,0,800,roll.getHeight()});};
    const auto difference=[](const juce::Image& a,const juce::Image& b,juce::Rectangle<int> area)
    {int count=0;area=area.getIntersection(a.getBounds());for(int y=area.getY();y<area.getBottom();++y)
        for(int x=area.getX();x<area.getRight();++x)if(a.getPixelAt(x,y)!=b.getPixelAt(x,y))++count;return count;};
    const auto on=capture();roll.setShowNativeWaveforms(false);const auto off=capture();
    check("native_notes_show_waveform_before_synthesis",difference(on,off,on.getBounds())>100);
    check("native_note_peaks_follow_manual_warp",difference(on,off,{274,0,10,on.getHeight()})>20
        &&difference(on,off,{461,0,10,on.getHeight()})>20);
    check("native_note_old_linear_positions_empty",difference(on,off,{214,0,10,on.getHeight()})==0);
    roll.setShowNativeWaveforms(true);check("native_toggle_restores_waveform",difference(capture(),on,on.getBounds())==0);
    check("native_roll_image_written",save("native-roll",on));
    const auto upper=lane.createComponentSnapshot({0,0,800,lane.getHeight()});
    check("upper_image_written",save("native-timeline",upper));
    auto linear=data;linear.tracks[0].clips[0].sourceTimeMap.clear();project.replace(linear);project.dispatchPendingMessages();
    const auto linearUpper=lane.createComponentSnapshot({0,0,800,lane.getHeight()});
    check("upper_preview_uses_native_time_map",difference(upper,linearUpper,{215,55,12,35})>20
        && difference(upper,linearUpper,{400,55,15,35})>20);
    project.replace(data);project.dispatchPendingMessages();roll.diagnosticRefresh();roll.setShowWaveforms(true);
    const auto overview=capture();roll.setShowNativeWaveforms(false);
    check("source_overview_avoids_double_waveform",difference(overview,capture(),overview.getBounds())==0);
    auto scoped=data;auto other=clip;other.id="other";other.startSeconds=3;
    other.notes[0].id="other-note";scoped.tracks[0].clips.push_back(other);
    project.replace(scoped);project.dispatchPendingMessages();roll.diagnosticRefresh();roll.setShowWaveforms(false);
    roll.setShowNativeWaveforms(true);const auto focused=capture();roll.setShowNativeWaveforms(false);const auto focusedOff=capture();
    check("native_regions_share_waveforms_by_default",difference(focused,focusedOff,{650,0,150,focused.getHeight()})>20);
    scoped.tracks[0].clips[1].showNormalDisplay=true;project.replace(scoped);project.dispatchPendingMessages();roll.diagnosticRefresh();
    roll.setShowNativeWaveforms(true);const auto shared=capture();roll.setShowNativeWaveforms(false);
    check("normal_display_region_can_show_native_waveform",difference(shared,capture(),{650,0,150,shared.getHeight()})>20);
    check("native_visibility_does_not_require_normal_display_flag",difference(shared,focused,shared.getBounds())==0);
    auto merged=clip;merged.notes[0].clipPartId="first";
    ClipData firstPart=clip;firstPart.id="first";firstPart.startSeconds=0;firstPart.durationSeconds=1;
    firstPart.sourceDurationSeconds=.25;firstPart.sourceTimeMap={{0,0},{1,.25}};
    ClipData secondPart=clip;secondPart.id="second";secondPart.startSeconds=1;secondPart.durationSeconds=1;
    secondPart.sourceOffsetSeconds=.25;secondPart.sourceDurationSeconds=.75;secondPart.sourceTimeMap={{0,0},{1,.75}};
    merged.parts={firstPart,secondPart};auto secondNote=note;secondNote.id="second-note";
    secondNote.startSeconds=1;secondNote.durationSeconds=1;secondNote.clipPartId="second";
    merged.notes[0].durationSeconds=1;merged.notes.push_back(secondNote);
    const auto parts=expandedClipParts(merged);const auto views=expandedClipParts(merged,true);
    bool aligned=parts.size()==2&&views.size()==2;
    for(std::size_t i=0; i<parts.size();++i)
    {const auto a=nativeAudioPreviewClip(parts[i]),b=nativeAudioPreviewClip(views[i]);
        aligned=aligned&&near(a.startSeconds,b.startSeconds)&&near(a.durationSeconds,b.durationSeconds)
            &&near(a.notes[0].startSeconds,b.notes[0].startSeconds)
            &&near(nativeSourceTimeAt(nativeSourceTimeMap(a),.5),nativeSourceTimeAt(nativeSourceTimeMap(b),.5));}
    check("merged_children_align_in_both_editors",aligned);
    project.replace(data);project.dispatchPendingMessages();roll.diagnosticRefresh();
    auto utau=data;utau.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;project.replace(utau);project.dispatchPendingMessages();roll.diagnosticRefresh();
    roll.setShowWaveforms(false);roll.setShowNativeWaveforms(false);const auto utauOff=capture();roll.setShowNativeWaveforms(true);
    check("utau_does_not_draw_native_preview",difference(utauOff,capture(),utauOff.getBounds())==0);
    const auto fineFile=folder.getChildFile("fine-transients.wav");
    juce::AudioBuffer<float> fine(1,48000);fine.clear();
    for(int i=14400;i<14408;++i)fine.setSample(0,i,.8f);
    for(int i=14544;i<14552;++i)fine.setSample(0,i,.8f);
    auto fineStream=fineFile.createOutputStream();if(fineStream){fineStream->setPosition(0);fineStream->truncate();}std::unique_ptr<juce::AudioFormatWriter> fineWriter(format.createWriterFor(fineStream.get(),48000,1,16,{},0));
    if(fineWriter){fineStream.release();fineWriter->writeFromAudioSampleBuffer(fine,0,48000);fineWriter.reset();}
    juce::AudioThumbnailCache coarseCache(2);
    juce::AudioThumbnail fineThumb(nativeWaveformSamplesPerPeak,formats,cache),coarseThumb(256,formats,coarseCache);
    fineThumb.setSource(new juce::FileInputSource(fineFile));coarseThumb.setSource(new juce::FileInputSource(fineFile));
    for(int i=0;i<400&&(!fineThumb.isFullyLoaded()||!coarseThumb.isFullyLoaded());++i)juce::Thread::sleep(5);
    float fineLow=0,fineHigh=0,coarseLow=0,coarseHigh=0;
    fineThumb.getApproximateMinMax(.3014,.3015,0,fineLow,fineHigh);
    coarseThumb.getApproximateMinMax(.3014,.3015,0,coarseLow,coarseHigh);
    check("fine_cache_resolves_gap_between_short_transients",std::max(std::abs(fineLow),std::abs(fineHigh))<=1.0f/128
        &&coarseHigh>.7f);
    fineThumb.getApproximateMinMax(.3000,.3001,0,fineLow,fineHigh);
    check("fine_cache_preserves_submillisecond_transient",fineHigh>.7f);
    const auto demoFile=folder.getChildFile("note-shapes.wav");
    for(int i=0;i<48000;++i){auto t=i/48000.0;auto strength=.86*std::exp(-std::pow((t-.15)/.038,2))
        +.28*std::exp(-std::pow((t-.64)/.105,2));samples.setSample(0,i,static_cast<float>(strength*std::sin(i*.17)));}
    auto demoStream=demoFile.createOutputStream();if(demoStream){demoStream->setPosition(0);demoStream->truncate();}std::unique_ptr<juce::AudioFormatWriter> demoWriter(format.createWriterFor(demoStream.get(),48000,1,16,{},0));
    if(demoWriter){demoStream.release();demoWriter->writeFromAudioSampleBuffer(samples,0,48000);demoWriter.reset();}
    auto demo=data;auto& demoClip=demo.tracks[0].clips[0];demoClip.sourceFile=demoFile;
    demoClip.notes[0].durationSeconds=1;demoClip.notes[0].midiNote=64;
    auto demoNote=note;demoNote.id="demo-second";demoNote.midiNote=60;demoNote.startSeconds=1;demoNote.durationSeconds=1;
    demoClip.notes.push_back(demoNote);project.replace(demo);project.dispatchPendingMessages();roll.diagnosticRefresh();
    roll.setShowNativeWaveforms(true);juce::Thread::sleep(250);
    const auto demoPicture=roll.createComponentSnapshot({100,static_cast<int>(roll.diagnosticNoteY(0))-60,510,220});
    check("warm_note_demo_written",save("warm-native-notes",demoPicture));
    int demoWarm=0;for(int y=0;y<demoPicture.getHeight();++y)for(int x=0;x<demoPicture.getWidth();++x)
    {auto pixel=demoPicture.getPixelAt(x,y);if(pixel.getRed()>140&&pixel.getRed()>pixel.getBlue()*1.5)++demoWarm;}
    check("warm_demo_contains_real_note_shapes",demoWarm>300);
    juce::PropertySet settings;const auto defaults=viewOptionsFrom(settings);
    check("native_preview_on_utau_rendered_wave_off_by_default",defaults.nativeWaveform&&!defaults.utauWaveform);
    auto toggled=afterViewMenuChoice(defaults,3,false);
    check("native_menu_toggle_is_independent",!toggled.nativeWaveform&&!toggled.utauWaveform);
    storeViewOptions(settings,toggled);check("native_toggle_saved",!viewOptionsFrom(settings).nativeWaveform);
    check("native_menu_item_enabled",viewMenuItemEnabled(3,false));
    std::cout<<"native_waveform_preview_ok="<<ok<<std::endl;return ok;
}
}
