#pragma once
#include "../NativeAudioTrim.h"
namespace hachi
{
inline bool runNativeAudioTrimSmoke(const juce::File& folder)
{
    folder.createDirectory(); bool ok=true; int checks=0;
    const auto check=[&](const char* key,bool pass){ok &= pass;++checks;std::cout<<key<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<2.e-6;};
    const auto file=folder.getChildFile("source.wav");juce::WavAudioFormat wav;auto stream=file.createOutputStream();
    if(stream){stream->setPosition(0);stream->truncate();}
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),48000,1,24,{},0));if(!writer)return false;stream.release();
    juce::AudioBuffer<float> buffer(1,96000);
    for(int i=0;i<96000;++i)buffer.setSample(0,i,static_cast<float>(.2*(.7+.3*std::sin(i*.00008))*std::sin(i*2*juce::MathConstants<double>::pi*220/48000)));
    check("source_written",writer->writeFromAudioSampleBuffer(buffer,0,96000));writer.reset();
    ClipData clip;clip.id="audio";clip.sourceFile=file;clip.startSeconds=1;clip.durationSeconds=clip.sourceDurationSeconds=2;
    NoteData note;note.id="n";note.label="trim";note.durationSeconds=2;note.midiNote=note.sourceMidiCenter=57;
    note.sourcePitchMeasured=true;note.utauAutoPitchTransition=false;
    for(int i=0;i<=400;++i)note.contour.push_back({i*.005,0,0,true});bindNativeNoteSource(note,clip);clip.notes={note};
    TrackData track;track.id="native";track.pitchAlgorithm=PitchAlgorithm::world;track.normalizeVolume=false;track.clips={clip};
    ProjectData initial;initial.tracks={track};initial.gridDivision="1/2";initial.noteEditDivision=2;ProjectModel model;model.replace(initial);
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack("native");roll.setFocusedClip("audio");
    roll.setTool(PianoRollComponent::Tool::trim);roll.setPixelsPerSecond(320);roll.setRowHeight(24);roll.setSize(1600,roll.getHeight());roll.diagnosticRefresh();
    const auto event=[](juce::Component& c,juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(
        juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys::leftButtonModifier,0,0,0,0,0,
        &c,&c,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    const auto drag=[&](bool left,float dx){roll.diagnosticRefresh();auto r=roll.diagnosticHitBounds(0);
        juce::Point<float> from(left?r.getX()+.5f:r.getRight()-.5f,r.getCentreY()),to(from.x+dx,from.y);
        roll.mouseDown(event(roll,from,from));roll.mouseDrag(event(roll,to,from));
        // Exercise the actual preview, including the shared source clock.
        juce::Image image(juce::Image::RGB,1100,220,true,juce::SoftwareImageType{});
        {juce::Graphics g(image);g.fillAll(Palette::background);g.setOrigin(0,-static_cast<int>(r.getY())+100);roll.paint(g);}
        auto out=folder.getChildFile(left?"trim-left.png":"trim-right.png").createOutputStream();if(out){out->setPosition(0);out->truncate();juce::PNGImageFormat().writeImageToStream(image,*out);}
        roll.mouseUp(event(roll,to,from));model.dispatchPendingMessages();roll.diagnosticRefresh();};
    drag(true,.25f);auto result=model.snapshot().tracks[0].clips[0];
    check("subpixel_left_trim_ignores_coarse_grid_and_drag_threshold",near(result.startSeconds,1+.25/320)&&near(result.durationSeconds,2-.25/320)
        &&near(result.sourceOffsetSeconds,.25/320)&&near(result.sourceDurationSeconds,result.durationSeconds));
    model.undo();model.dispatchPendingMessages();check("undo_restores_full_audio",near(model.snapshot().tracks[0].clips[0].durationSeconds,2));
    drag(false,-.5f);result=model.snapshot().tracks[0].clips[0];
    check("subpixel_right_trim_ignores_grid",near(result.startSeconds,1)&&near(result.durationSeconds,2-.5/320)&&near(result.sourceDurationSeconds,result.durationSeconds));
    model.undo();model.dispatchPendingMessages();drag(true,123.25f);result=model.snapshot().tracks[0].clips[0];
    check("head_trim_keeps_end_position_and_source_rate",near(result.startSeconds+result.durationSeconds,3)&&near(result.sourceOffsetSeconds,123.25/320)
        &&near(result.sourceDurationSeconds,result.durationSeconds));
    check("source_pitch_line_stops_at_crop_boundaries",near(result.notes[0].contour.front().timeSeconds,0)
        &&near(result.notes[0].contour.back().timeSeconds,result.notes[0].durationSeconds));
    const auto cropped=model.snapshot();juce::String error;ProjectModel reopened;
    check("save_reopen_retains_source_cut",model.save(folder.getChildFile("trim.hjpx"),error)&&reopened.load(folder.getChildFile("trim.hjpx"),error)
        &&near(reopened.snapshot().tracks[0].clips[0].sourceOffsetSeconds,result.sourceOffsetSeconds));
    model.undo();model.redo();check("redo_restores_cut",near(model.snapshot().tracks[0].clips[0].sourceOffsetSeconds,result.sourceOffsetSeconds));
    check("outward_drag_restores_hidden_source",model.trimNativeNoteEdge("n",-.2,true)
        && near(model.snapshot().tracks[0].clips[0].sourceOffsetSeconds,result.sourceOffsetSeconds-.2));
    // Linked syllables: a crop leaves a gap; it must not stretch adjacent pieces.
    auto split=initial;auto& c=split.tracks[0].clips[0];c.sourceTimeMap={{0,0},{.4,.2},{1.4,1.1},{2,2}};c.notes.clear();
    const double edges[]={0,.4,1.4,2};
    for(int i=0;i<3;++i){auto n=note;n.id="s"+juce::String(i);n.startSeconds=edges[i];n.durationSeconds=edges[i+1]-edges[i];
        n.contour={{0,0,0,true},{n.durationSeconds,120,120,true}};n.amplitudeEnvelope={{0,-6},{n.durationSeconds,0}};bindNativeNoteSource(n,c);c.notes.push_back(n);}
    model.replace(split);model.setNotesConnection({"s0","s1","s2"},true);check("middle_trim_succeeds",model.trimNativeNoteEdge("s1",.1234567,true));
    auto expanded=model.snapshot();expandProjectClipParts(expanded,false);bool stable=true;int found=0;
    for(const auto& child:expanded.tracks[0].clips)for(const auto& n:child.notes)
    {
        const auto index=n.id.getLastCharacters(1).getIntValue();++found;
        const double cut=index==1?.1234567:0;
        stable &= near(child.startSeconds+n.startSeconds,1+edges[index]+cut)&&near(n.durationSeconds,edges[index+1]-edges[index]-cut);
        for(double t=.001;t<n.durationSeconds;t+=.013)
            stable &= near(child.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(child),n.startSeconds+t),nativeSourceTimeAt(nativeClipClock(c),edges[index]+cut+t));
    }
    check("all_three_segments_keep_timing_and_nonlinear_source_clock",stable&&found==3);
    check("only_cut_seam_disconnects",model.snapshot().nativeConnections.size()==1&&model.snapshot().nativeConnections[0].leftNoteId=="s1");
    auto vibrato=initial;auto& v=vibrato.tracks[0].clips[0].notes[0];v.vibratoEnabled=true;v.vibratoLengthPercent=100;
    v.vibratoPhasePercent=23;v.vibratoFadeInPercent=40;v.vibratoFadeOutPercent=40;model.replace(vibrato);
    model.trimNativeNoteEdge("n",.37,true);model.trimNativeNoteEdge("n",-.29,false);
    auto vtrim=model.snapshot().tracks[0].clips[0].notes[0];bool sameVibrato=true;
    for(double t=0;t<vtrim.durationSeconds;t+=.003)sameVibrato &= near(vibratoCentsAt(v,t+.37),vibratoCentsAt(vtrim,t));
    check("crop_preserves_vibrato_phase_and_fades",sameVibrato);
    check("vibrato_clock_survives_save",model.save(folder.getChildFile("vibrato-trim.hjpx"),error)&&reopened.load(folder.getChildFile("vibrato-trim.hjpx"),error)
        &&near(vibratoCentsAt(reopened.snapshot().tracks[0].clips[0].notes[0],.321),vibratoCentsAt(v,.691)));
    auto u=initial;u.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;model.replace(u);check("utau_rejects_trim",!model.trimNativeNoteEdge("n",.1,true));
    MainComponent main;main.setSize(1600,900);main.diagnosticProject().replace(initial);main.diagnosticProject().dispatchPendingMessages();main.diagnosticSelectTrack("native");main.diagnosticRefreshControls();
    const auto* trim=main.findChildWithID("icon.trim");const auto* connect=main.findChildWithID("icon.connect");
    check("trim_button_directly_left_of_connect",trim&&connect&&trim->isEnabled()&&trim->getRight()<=connect->getX()&&connect->getX()-trim->getRight()<12);
    main.diagnosticPressTool(PianoRollComponent::Tool::trim);check("toolbar_activates_trim",main.diagnosticTool()==PianoRollComponent::Tool::trim);
    auto screen=main.createComponentSnapshot(main.getLocalBounds());auto image=folder.getChildFile("native-toolbar.png").createOutputStream();if(image){image->setPosition(0);image->truncate();juce::PNGImageFormat().writeImageToStream(screen,*image);}
    main.diagnosticPressTool(PianoRollComponent::Tool::trim);check("second_click_restores_normal_tool",main.diagnosticTool()==PianoRollComponent::Tool::note);
    main.diagnosticPressTool(PianoRollComponent::Tool::trim);main.diagnosticProject().replace(u);main.diagnosticProject().dispatchPendingMessages();main.diagnosticRefreshControls();
    check("switching_to_utau_disables_and_exits_trim",trim&&!trim->isEnabled()&&main.diagnosticTool()==PianoRollComponent::Tool::note);
    // Render uncropped audio and cropped audio on the same absolute timeline.
    AudioEngine engine;engine.prepareToPlay(256,48000);WavExportOptions options;options.sampleRate=48000;options.channels=2;options.bitDepth=32;
    const auto render=[&](const ProjectData& d,const char* name){engine.syncProject(d);for(int i=0;i<1500&&engine.renderProgress();++i)juce::Thread::sleep(10);
        return engine.hasCurrentRenderedAudio()&&engine.exportWav(folder.getChildFile(name),error,track.id,1,3,options);};
    check("original_and_trimmed_audio_render",render(initial,"before.wav")&&render(cropped,"after.wav"));
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(folder.getChildFile("before.wav"))),b(formats.createReaderFor(folder.getChildFile("after.wav")));
    bool identical=a&&b;double difference=0,energy=0;
    if(identical){juce::AudioBuffer<float> x(2,96000),y(2,96000);a->read(&x,0,96000,0,true,true);b->read(&y,0,96000,0,true,true);
        for(int i=24000;i<90000;++i){const auto v=x.getSample(0,i);const auto delta=v-y.getSample(0,i);difference+=delta*delta;energy+=v*v;}
        identical=energy>1&&difference/energy<1.e-4;
        check("cropped_head_is_silent",y.getRMSLevel(0,0,17000)<1.e-5f);}
    check("retained_audio_matches_original_without_restretch",identical);std::cout<<"render_relative_error="<<(difference/std::max(energy,1.e-12))<<std::endl;engine.releaseResources();
    std::cout<<"native_audio_trim_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
