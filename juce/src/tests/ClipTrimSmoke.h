#pragma once
namespace hachi
{
inline bool MainComponent::diagnosticClipTrim(const juce::File& folder)
{
    folder.createDirectory(); stopTimer(); setSize(1280, 800);
    bool ok = true;
    const auto check = [&](const char* label, bool passed)
    { ok = ok && passed; std::cout << label << '=' << passed << std::endl; };
    const auto near = [](double a, double b) { return std::abs(a-b) < 1.0e-7; };
    const auto file = folder.getChildFile("source.wav");
    juce::AudioBuffer<float> signal(2, 288000);
    for (int c=0;c<2;++c) for(int i=0;i<signal.getNumSamples();++i)
    { const auto t=i/48000.0; signal.setSample(c,i,static_cast<float>(.2*std::sin(juce::MathConstants<double>::twoPi*((193+c*227)*t+11*t*t)))); }
    {
        auto stream=file.createOutputStream(); if (!stream) return false;
        stream->setPosition(0); stream->truncate(); juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,2,32,{},0));
        check("source_written",writer && writer->writeFromAudioSampleBuffer(signal,0,signal.getNumSamples()));
    }
    const auto get = [&] { return project.snapshot().tracks[0].clips[0]; };
    const auto refresh = [&]
    {
        project.dispatchPendingMessages(); stopTimer(); resized();
        timelineHorizontalZoom=140; timeline.setPixelsPerSecond(140); timelineViewport.setViewPosition(0,0);
        (void) timeline.createComponentSnapshot(timeline.getLocalBounds());
    };
    const auto drag = [&](int edge, double seconds)
    {
        refresh(); const auto clip=get();
        const auto x = edge<0 ? timeline.pixelForSeconds(clip.startSeconds)+3
            : edge>0 ? timeline.pixelForSeconds(clip.startSeconds+clip.durationSeconds)-3
            : timeline.pixelForSeconds(clip.startSeconds+clip.durationSeconds*.5);
        const juce::Point<float> down(static_cast<float>(x), static_cast<float>(timeline.getRulerHeight()+19+(timeline.getRowHeight()-25)/2));
        const auto event = [&](juce::Point<float> p)
        { return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,&timeline,&timeline,
            juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,true); };
        timeline.mouseDown(event(down));
        const auto end=down+juce::Point<float>(static_cast<float>(seconds*140),0);
        timeline.mouseDrag(event(end));
        (void) timeline.createComponentSnapshot(timeline.getLocalBounds());
        timeline.mouseUp(event(end)); refresh();
    };
    const auto render = [&](const juce::String& name)
    {
        audio.syncProject(project.snapshot()); juce::String error;
        const auto output=folder.getChildFile(name+".wav");
        check("export_succeeded",audio.exportWav(output,error,"audio",0,6,{48000,2,32}));
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(output.createInputStream().release(),true));
        juce::AudioBuffer<float> result;
        double endOfClips=0;
        for(const auto& track:project.snapshot().tracks)for(const auto& clip:track.clips)
            endOfClips=std::max(endOfClips,clip.startSeconds+clip.durationSeconds);
        check("export_keeps_outer_clip_length",reader && reader->lengthInSamples == static_cast<juce::int64>(
            std::ceil(std::min(6.0,endOfClips)*48000)));
        // Export stops at the project end. Pad only the comparison buffer so
        // different clip extents can be compared on the same absolute timeline.
        if(reader) { result.setSize(2,288000); result.clear();
            reader->read(&result,0,std::min(288000,static_cast<int>(reader->lengthInSamples)),0,true,true); }
        return result;
    };
    const auto difference = [&](const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,double begin=0,double end=6)
    {
        if(a.getNumSamples()!=288000 || b.getNumSamples()!=288000) return 100.0f;
        float d=0;for(int c=0;c<2;++c)for(int i=static_cast<int>(begin*48000);i<static_cast<int>(end*48000);++i)
            d=std::max(d,std::abs(a.getSample(c,i)-b.getSample(c,i)));return d;
    };
    const auto silent = [&](const juce::AudioBuffer<float>& a,double begin,double end)
    { return a.getNumSamples()==288000 && a.getMagnitude(static_cast<int>(begin*48000),static_cast<int>((end-begin)*48000))<1.0e-7f; };
    ProjectData preview;
    for (const bool backing : {true,false})
    {
        ProjectData data; TrackData track; track.id="audio"; track.name=backing?"Backing":"Raw audio";
        track.accompaniment=backing; track.compose=false;
        ClipData clip; clip.id="clip"; clip.sourceFile=file; clip.startSeconds=1; clip.durationSeconds=2;
        clip.sourceOffsetSeconds=.2; clip.sourceDurationSeconds=backing?2:3; clip.gain=.7f;
        track.clips.push_back(clip);data.tracks.push_back(track);project.replace(data);refresh();
        const juce::String tag=backing?"backing":"raw";
        const auto baseline=render(tag+"-original");
        auto revision=project.revisionNumber();drag(1,1);
        check("right_edge_extends_without_moving_or_stretching",near(get().startSeconds,1)&&near(get().durationSeconds,3)
            &&near(get().audioLength(),2)&&near(get().sourceDurationSeconds,clip.sourceDurationSeconds));
        check("drag_is_one_undo_step",project.revisionNumber()==revision+1);
        drag(-1,-.5);
        check("left_edge_adds_leading_blank",near(get().startSeconds,.5)&&near(get().durationSeconds,3.5)
            &&near(get().audioStartSeconds,.5)&&near(get().audioLength(),2));
        const auto padded=render(tag+"-extended");
        const auto error=difference(baseline,padded);std::cout<<"extension_max_difference="<<error<<std::endl;
        check("extended_audio_unchanged_and_padding_silent",error<1.0e-6f&&silent(padded,.5,1)&&silent(padded,3,4));
        if(backing)preview=project.snapshot();
        drag(0,1);
        check("middle_still_moves_whole_clip",near(get().startSeconds,1.5)&&near(get().audioStartSeconds,.5)&&near(get().durationSeconds,3.5));
        check("undo_move",project.undo()&&near(get().startSeconds,.5));refresh();
        drag(1,-1.5);
        check("right_edge_trims_content",near(get().startSeconds,.5)&&near(get().durationSeconds,2)&&near(get().audioLength(),1.5));
        drag(-1,1);
        check("left_edge_trims_without_shifting_retained_audio",near(get().startSeconds,1.5)&&near(get().durationSeconds,1)
            &&near(get().audioStartSeconds,0)&&near(get().sourceOffsetSeconds,.2+.5*(backing?1:1.5)));
        const auto cropped=render(tag+"-cropped");
        check("cropped_samples_keep_original_timing",difference(baseline,cropped,1.51,2.49)<1.0e-6f
            &&silent(cropped,0,1.5)&&silent(cropped,2.5,6));
        revision=project.revisionNumber();
        check("extend_cropped_clip",project.trimClip("clip",.5,4.5));
        check("extend_records_one_change",project.revisionNumber()==revision+1);
        const auto extendedAgain=render(tag+"-extended-again");
        check("reextension_does_not_restore_cropped_audio",difference(cropped,extendedAgain)<1.0e-6f);
        check("undo_extension",project.undo()&&near(get().startSeconds,1.5)&&near(get().durationSeconds,1));
        check("redo_extension",project.redo()&&near(get().audioStartSeconds,1)&&near(get().durationSeconds,4.5));
        juce::String errorText;const auto projectFile=folder.getChildFile(tag+".hjpx");ProjectModel reopened;
        check("save_reopen_padding",project.save(projectFile,errorText)&&reopened.load(projectFile,errorText)
            &&near(reopened.snapshot().tracks[0].clips[0].audioStartSeconds,1)
            &&near(reopened.snapshot().tracks[0].clips[0].audioLength(),1));
        project.replace(reopened.snapshot());
        check("split_leading_blank",project.splitClip("clip",.75).isNotEmpty());
        check("split_padding_keeps_silence_and_audio",difference(cropped,render(tag+"-split-blank"))<1.0e-6f);
        const auto bodyId=project.snapshot().tracks[0].clips[1].id;
        check("split_padded_audio",project.splitClip(bodyId,2).isNotEmpty());
        check("split_padded_audio_stays_continuous",difference(cropped,render(tag+"-split-audio"))<1.0e-6f);
        project.replace(data);
        check("trim_all_audio_to_empty",project.trimClip("clip",4,1));
        check("empty_clip_is_silent",silent(render(tag+"-empty"),0,6));
        check("empty_extension_stays_empty",project.trimClip("clip",3,3)&&get().audioLength()==0);
    }
    ProjectData score;TrackData voice;voice.id="voice";voice.pitchAlgorithm=PitchAlgorithm::utau;
    ClipData phrase;phrase.id="phrase";phrase.startSeconds=1;phrase.durationSeconds=3;
    NoteData a;a.id="a";a.startSeconds=.2;a.durationSeconds=.8;
    NoteData b;b.id="b";b.startSeconds=1.2;b.durationSeconds=1.8;b.utauFlags="Mb70";
    b.diffSingerPitchReference={{0,60},{1.8,63}};b.utauFlagCurves={{"g",{{0,-20},{1.8,30}}}};
    phrase.notes={a,b};voice.clips={phrase};score.tracks={voice};project.replace(score);
    check("score_left_extension",project.trimClip("phrase",0,4));
    check("notes_keep_absolute_time_duration_flags_and_reference",near(get().notes[0].startSeconds,1.2)
        &&near(get().notes[0].durationSeconds,.8)&&get().notes[1].utauFlags=="Mb70"
        &&near(get().notes[1].diffSingerPitchReference.back().timeSeconds,1.8));
    check("score_right_extension",project.trimClip("phrase",0,5));
    const auto revision=project.revisionNumber();check("score_right_crop",project.trimClip("phrase",0,3));
    check("crossing_note_cropped_not_stretched",get().notes.size()==2&&near(get().notes[1].startSeconds,2.2)
        &&near(get().notes[1].durationSeconds,.8)&&get().notes[1].utauFlags=="Mb70"
        &&project.revisionNumber()==revision+1);
    check("undo_score_crop_once",project.undo()&&near(get().notes[1].durationSeconds,1.8));
    check("redo_score_crop_once",project.redo()&&near(get().notes[1].durationSeconds,.8));
    const auto noOp=project.revisionNumber();
    check("invalid_and_unchanged_resize_have_no_history",!project.trimClip("phrase",0,3)
        &&!project.trimClip("phrase",-1,2)&&!project.trimClip("phrase",0,.001)&&project.revisionNumber()==noOp);
    const auto beforeTempo=get();
    project.setTempoChange(0,60,true);
    check("tempo_sync_preserves_padded_audio_mapping",near(get().audioStartSeconds,beforeTempo.audioStartSeconds*2)
        &&near(get().audioLength(),beforeTempo.audioLength()*2)
        &&near(get().notes[0].startSeconds,beforeTempo.notes[0].startSeconds*2));
    project.replace(preview);refresh();
    if(auto stream=folder.getChildFile("clip-trim.png").createOutputStream())
        check("preview_written",juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream));
    return ok;
}
}
