#pragma once
#include "../TrackGainEnvelope.h"
namespace hachi
{
inline bool MainComponent::diagnosticTrackGainEnvelope(const juce::File& folder)
{
    folder.createDirectory();stopTimer();setSize(1280,800);bool ok=true;
    const auto check=[&](const char* name,bool pass){ok=ok&&pass;std::cout<<name<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.0e-4;};
    const auto source=folder.getChildFile("source.wav");
    {
        juce::AudioBuffer<float> samples(2,96000);
        for(int c=0;c<2;++c)for(int i=0;i<samples.getNumSamples();++i)
            samples.setSample(c,i,static_cast<float>(.12*std::sin(juce::MathConstants<double>::twoPi*(211+c*73)*i/48000.0)));
        auto out=source.createOutputStream();out->setPosition(0);out->truncate();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(out.release(),48000,2,32,{},0));
        check("source_written",writer&&writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples()));
    }
    ProjectData data;TrackData track;track.id="audio";track.name="Independent region envelopes";
    track.accompaniment=true;track.compose=false;track.volume=.8f;
    ClipData a;a.id="a";a.sourceFile=source;a.startSeconds=.5;a.durationSeconds=1.5;
    a.sourceDurationSeconds=1.2;a.audioStartSeconds=.2;a.audioDurationSeconds=1.2;a.gain=.6f;a.fadeInSeconds=.01;
    ClipData b=a;b.id="b";b.startSeconds=2.5;b.durationSeconds=1;b.audioStartSeconds=.1;
    b.sourceDurationSeconds=.8;b.audioDurationSeconds=.8;b.gain=.35f;
    track.clips={a,b};data.tracks={track};auto other=track;other.id="other";other.name="Other track";other.muted=true;
    other.clips[0].id="c";other.clips[1].id="d";data.tracks.push_back(other);project.replace(data);
    const auto refresh=[&](int height=96)
    {
        project.dispatchPendingMessages();stopTimer();resized();timelineHorizontalZoom=140;
        timeline.setPixelsPerSecond(140);timeline.setRowHeight(static_cast<float>(height));trackList.setRowHeight(static_cast<float>(height));
        (void)timeline.createComponentSnapshot(timeline.getLocalBounds());
    };
    const auto clip=[&](const juce::String& id)
    {for(const auto& t:project.snapshot().tracks)for(const auto& c:t.clips)if(c.id==id)return c;return ClipData{};};
    const auto envelope=[&]{return clip("a").gainEnvelope;};
    const auto event=[&](juce::Point<float> p,juce::Point<float> down,int mods=0)
    {return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|mods),0,0,0,0,0,&timeline,&timeline,
        juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,p!=down);};
    const auto gesture=[&](juce::Point<float> from,juce::Point<float> to,int mods=0)
    {timeline.mouseDown(event(from,from,mods));timeline.mouseDrag(event(to,from,mods));timeline.mouseUp(event(to,from,mods));refresh();};
    const auto preview=[&](const juce::String& name)
    {if(auto out=folder.getChildFile(name+".png").createOutputStream())check("preview_written",juce::PNGImageFormat().writeImageToStream(timeline.createComponentSnapshot(timeline.getLocalBounds()),*out));};
    const juce::Point<float> centre(210,86.5f);refresh();auto revision=project.revisionNumber();
    timeline.mouseDoubleClick(event({320,86.5f},{320,86.5f}));refresh();
    check("empty_gap_does_not_create_points",project.revisionNumber()==revision&&envelope().empty()&&clip("b").gainEnvelope.empty());
    timeline.mouseDoubleClick(event(centre,centre));refresh();
    check("point_and_anchors_are_clip_relative",envelope().size()==3&&near(envelope()[1].timeSeconds,1)
        &&near(envelope()[0].timeSeconds,0)&&near(envelope()[2].timeSeconds,1.5));
    check("other_regions_stay_flat",clip("b").gainEnvelope.empty()&&clip("c").gainEnvelope.empty());
    check("point_addition_undo",project.undo()&&envelope().empty());check("point_addition_redo",project.redo()&&envelope().size()==3);refresh();
    revision=project.revisionNumber();gesture(centre.translated(3,1),centre.translated(3,1));
    check("edge_grab_does_not_jump",revision==project.revisionNumber()&&near(envelope()[1].timeSeconds,1));
    timeline.mouseDown(event(centre,centre));timeline.mouseDrag(event(centre.translated(14,5.1f),centre));
    check("point_drag_preview_defers_commit",project.revisionNumber()==revision&&near(envelope()[1].gainDb,0));
    timeline.mouseUp(event(centre.translated(14,5.1f),centre));refresh();
    check("point_drag_updates_only_region",near(envelope()[1].timeSeconds,1.1)&&near(envelope()[1].gainDb,-12)&&clip("b").gainEnvelope.empty());
    check("point_drag_one_undo",project.revisionNumber()==revision+1&&project.undo()&&near(envelope()[1].gainDb,0));
    check("point_drag_redo",project.redo()&&near(envelope()[1].gainDb,-12));refresh();
    const auto changed=envelope();const juce::Point<float> moved(224,91.6f);revision=project.revisionNumber();
    timeline.mouseDown(event(moved,moved));timeline.mouseDrag(event(moved.translated(20,-20),moved));
    (void)timeline.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));timeline.mouseUp(event(moved.translated(20,-20),moved));refresh();
    check("escape_cancels_point_edit",project.revisionNumber()==revision&&sameTrackGainEnvelope(changed,envelope()));
    gesture(moved,moved.translated(0,-2.55f),juce::ModifierKeys::shiftModifier);check("shift_fine_adjustment",near(envelope()[1].gainDb,-11.4));
    project.setClipGainEnvelope("a",{{0,0},{1,0},{1.5,0}});refresh();
    gesture(centre,{900,86.5f});check("point_stays_inside_region",envelope()[1].timeSeconds<1.5);
    project.setClipGainEnvelope("a",{{0,0},{1,0},{1.5,0}});refresh();gesture(centre,{210,-200});check("upper_limit",near(envelope()[1].gainDb,12));
    gesture({210,61},{210,400});check("lower_limit",near(envelope()[1].gainDb,-60));
    gesture({210,112},{210,106.9f});check("silent_point_can_be_raised",near(envelope()[1].gainDb,-48));
    timeline.setSelectedClips({"a"});(void)timeline.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));refresh();
    check("delete_point_preserves_region",envelope().size()==2&&clip("a").id=="a");
    project.setClipGainEnvelope("a",{{0,0},{.5,-12},{1.5,0}});refresh();
    const auto curveBeforeMove=envelope();timeline.setSelectedClips({"a"});const juce::Point<float> body(180,74);
    timeline.mouseDown(event(body,body));timeline.mouseDrag(event(body.translated(140,0),body));preview("move-preview");
    check("region_drag_preview_preserves_model",near(clip("a").startSeconds,.5));
    timeline.mouseUp(event(body.translated(140,0),body));refresh();preview("moved-region");
    check("region_move_keeps_relative_points",near(clip("a").startSeconds,1.5)&&sameTrackGainEnvelope(curveBeforeMove,envelope()));
    check("region_move_leaves_neighbour_unchanged",near(clip("b").startSeconds,2.5)&&clip("b").gainEnvelope.empty());
    check("region_move_undo",project.undo()&&near(clip("a").startSeconds,.5));refresh();
    project.moveClips({"a","b"},.5);check("multimove_carries_independent_curves",near(clip("a").startSeconds,1)&&near(clip("b").startSeconds,3)&&sameTrackGainEnvelope(curveBeforeMove,envelope()));
    const auto duplicate=project.duplicateClip("a",4,"audio");
    check("duplicate_copies_envelope",duplicate.isNotEmpty()&&sameTrackGainEnvelope(envelope(),clip(duplicate).gainEnvelope));
    project.setClipGainEnvelope(duplicate,{{0,-24}});check("duplicate_edit_is_independent",sameTrackGainEnvelope(curveBeforeMove,envelope()));
    // Audio comparisons use independent dB calculations and both source-reader and rendered paths.
    AudioEngine engine;juce::String error;constexpr int frames=288000;
    const auto render=[&](const ProjectData& input,const juce::String& name,bool all=false)
    {
        engine.syncProject(input);const auto file=folder.getChildFile(name+".wav");
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+60000;
        while(engine.renderProgress()&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(10);
        const auto exported=engine.exportWav(file,error,all?juce::String{}:"audio",0,6,{48000,2,32});check("export_succeeded",exported);
        if(!exported)std::cout<<"export_error="<<error<<std::endl;
        juce::WavAudioFormat wav;auto stream=file.createInputStream();
        std::unique_ptr<juce::AudioFormatReader> reader(exported&&stream?wav.createReaderFor(stream.release(),true):nullptr);
        juce::AudioBuffer<float> result(2,frames);result.clear();
        double end=0;for(const auto& t:input.tracks)for(const auto& c:t.clips)if(!c.muted)end=std::max(end,c.startSeconds+c.durationSeconds);
        check("export_length",reader&&reader->lengthInSamples==static_cast<juce::int64>(std::ceil(std::min(6.0,end)*48000)));
        if(reader)reader->read(&result,0,std::min(frames,static_cast<int>(reader->lengthInSamples)),0,true,true);return result;
    };
    const auto diff=[](const auto& x,const auto& y,float scale=1.0f)
    {float d=0;for(int c=0;c<2;++c)for(int i=0;i<frames;++i)d=std::max(d,std::abs(x.getSample(c,i)*scale-y.getSample(c,i)));return d;};
    const auto baseline=render(data,"baseline");auto shapedData=data;
    shapedData.tracks[0].clips[0].gainEnvelope={{0,0},{.5,-12},{1.5,0}};
    shapedData.tracks[0].clips[1].gainEnvelope={{0,-6},{1,-18}};
    const auto shaped=render(shapedData,"shaped");float gainError=0;
    for(int i=0;i<frames;++i)
    {
        const double t=i/48000.0;double db=0;
        if(t>=.5&&t<2){const auto local=t-.5;db=local<.5?-24*local:-12+12*(local-.5);}
        if(t>=2.5&&t<3.5)db=-6-12*(t-2.5);
        for(int c=0;c<2;++c)gainError=std::max(gainError,std::abs(shaped.getSample(c,i)-baseline.getSample(c,i)*static_cast<float>(std::pow(10.0,db/20.0))));
    }
    check("independent_envelopes_match_audio_including_empty_head",gainError<2.0e-6f&&baseline.getMagnitude(0,frames)>.01f);
    check("mix_equals_track_export",diff(shaped,render(shapedData,"mix",true))<1.0e-6f);
    project.replace(shapedData);project.moveClips({"a"},.5);const auto movedAudio=render(project.snapshot(),"moved");float moveError=0;
    for(int c=0;c<2;++c){for(int i=24000;i<96000;++i)moveError=std::max(moveError,std::abs(shaped.getSample(c,i)-movedAudio.getSample(c,i+24000)));
        for(int i=120000;i<168000;++i)moveError=std::max(moveError,std::abs(shaped.getSample(c,i)-movedAudio.getSample(c,i)));}
    check("audio_moves_with_region_without_changing_neighbour",moveError<2.0e-6f);
    project.replace(shapedData);const auto rightId=project.splitClip("a",1.2);check("split_succeeds",rightId.isNotEmpty());
    check("split_preserves_audio",diff(shaped,render(project.snapshot(),"split"))<2.0e-6f);
    const auto rightBefore=clip(rightId).gainEnvelope;project.setClipGainEnvelope("a",{{0,-30}});
    check("split_halves_edit_independently",sameTrackGainEnvelope(rightBefore,clip(rightId).gainEnvelope));
    project.replace(shapedData);check("trim_succeeds",project.trimClip("a",.9,1.1));const auto trimmed=render(project.snapshot(),"trimmed");float trimError=0;
    for(int c=0;c<2;++c)for(int i=44000;i<95000;++i)trimError=std::max(trimError,std::abs(shaped.getSample(c,i)-trimmed.getSample(c,i)));
    check("trim_preserves_envelope_alignment",trimError<2.0e-6f);
    project.replace(shapedData);check("extension_succeeds",project.trimClip("a",.25,2));
    check("extension_does_not_shift_content_envelope",diff(shaped,render(project.snapshot(),"extended"))<2.0e-6f);
    // Overlapping sources require their individual envelopes, not a lossy combined curve.
    auto overlap=shapedData;overlap.tracks[0].clips[1].startSeconds=1.25;project.replace(overlap);
    const auto overlapAudio=render(project.snapshot(),"overlap");check("merge_succeeds",project.mergeClips({"a","b"})=="a");
    check("merged_region_preserves_overlapping_envelopes",diff(overlapAudio,render(project.snapshot(),"merged"))<2.0e-6f);
    project.setClipGainEnvelope("a",{{0,-6},{1.75,-6}});const auto mergedShaped=render(project.snapshot(),"merged-shaped");
    const auto minus6=static_cast<float>(std::pow(10.0,-6.0/20.0));
    check("merged_parent_envelope_applies_once",diff(overlapAudio,mergedShaped,minus6)<2.0e-6f);
    const auto splitMerged=project.splitClip("a",1.1);check("merged_split_succeeds",splitMerged.isNotEmpty());
    check("merged_split_preserves_all_layers",diff(mergedShaped,render(project.snapshot(),"merged-split"))<2.0e-6f);
    check("remerge_succeeds",project.mergeClips({"a",splitMerged})=="a");
    check("remerge_preserves_inherited_layers",diff(mergedShaped,render(project.snapshot(),"remerged"))<2.0e-6f);
    ProjectModel reopened;const auto saved=folder.getChildFile("regions.hjpx");
    check("save_reopen_all_layers",project.save(saved,error)&&reopened.load(saved,error)
        &&diff(mergedShaped,render(reopened.snapshot(),"reopened"))<2.0e-6f);
    // Build a real revision-047 file and verify transparent load-time migration.
    project.replace(data);const auto legacy=folder.getChildFile("legacy-track.hjpx");check("legacy_fixture_saved",project.save(legacy,error));
    {
        juce::MemoryBlock bytes;legacy.loadFileAsData(bytes);juce::MemoryInputStream in(bytes,false);auto root=juce::ValueTree::readFromStream(in);
        for(auto t:root)if(t.hasType("Track")&&t.getProperty("id").toString()=="audio")
            for(auto p:std::vector<GainEnvelopePoint>{{0,0},{2,-12},{4,0}})
            {juce::ValueTree point("TrackGainPoint");point.setProperty("timeSeconds",p.timeSeconds,nullptr);point.setProperty("gainDb",p.gainDb,nullptr);t.addChild(point,-1,nullptr);}
        if(auto out=legacy.createOutputStream()){out->setPosition(0);out->truncate();root.writeToStream(*out);}
    }
    check("legacy_load_migrates_to_each_region",reopened.load(legacy,error)&&!reopened.snapshot().tracks[0].clips[0].gainEnvelope.empty()
        &&!reopened.snapshot().tracks[0].clips[1].gainEnvelope.empty());
    const auto migrated=render(reopened.snapshot(),"legacy-migrated");float legacyError=0;
    for(int c=0;c<2;++c)for(int i=0;i<frames;++i){const double t=i/48000.0,db=t<2?-6*t:t<4?-12+6*(t-2):0;
        legacyError=std::max(legacyError,std::abs(migrated.getSample(c,i)-baseline.getSample(c,i)*static_cast<float>(std::pow(10.0,db/20.0))));}
    check("legacy_migration_preserves_audio",legacyError<2.0e-6f);
    check("migration_saved_without_double_application",reopened.save(saved,error)&&project.load(saved,error)
        &&diff(migrated,render(project.snapshot(),"migrated-reopened"))<2.0e-6f);
    engine.syncProject(shapedData);engine.prepareToPlay(512,48000);engine.setPosition(2.75);engine.play();
    juce::AudioBuffer<float> live(2,512);engine.getNextAudioBlock({&live,0,512});engine.stop();float liveError=0;
    for(int c=0;c<2;++c)for(int i=0;i<512;++i)liveError=std::max(liveError,std::abs(live.getSample(c,i)-shaped.getSample(c,132000+i)));
    check("live_playback_matches_export",live.getMagnitude(0,512)>.001f&&liveError<2.0e-6f);
    auto native=data;native.tracks[0].accompaniment=false;native.tracks[0].compose=true;native.tracks[0].pitchAlgorithm=PitchAlgorithm::mld5;
    for(auto& c:native.tracks[0].clips){NoteData n;n.id=c.id+"-note";n.startSeconds=c.audioStartSeconds;n.durationSeconds=c.audioDurationSeconds;n.midiNote=60;c.notes={n};}
    const auto nativeBefore=render(native,"native-before");const auto renderKey=AudioEngine::diagnosticUtauRenderKey(native,"a");
    for(auto& c:native.tracks[0].clips)c.gainEnvelope={{0,-6}};
    check("gain_change_reuses_synthesis_cache",renderKey==AudioEngine::diagnosticUtauRenderKey(native,"a"));
    const auto nativeAfter=render(native,"native-after");
    check("rendered_buffer_uses_region_envelope",engine.hasCurrentRenderedAudio()&&nativeBefore.getMagnitude(0,frames)>.001f&&diff(nativeBefore,nativeAfter,minus6)<2.0e-6f);
    project.replace(shapedData);refresh();for(const int height:{48,96,192}){refresh(height);preview("regions-height-"+juce::String(height));}
    return ok;
}
}