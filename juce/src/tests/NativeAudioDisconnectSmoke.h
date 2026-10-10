#pragma once
#include "../NativeAudioDisconnect.h"
namespace hachi
{
inline bool runNativeAudioDisconnectSmoke(const juce::File& folder)
{
    folder.createDirectory();bool ok=true;int checks=0;
    const auto check=[&](const char* key,bool pass){ok &= pass;++checks;std::cout<<key<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-5;};
    const auto source=folder.getChildFile("source.wav");juce::AudioBuffer<float> buffer(1,57600);
    for (int i=0;i<57600;++i)buffer.setSample(0,i,static_cast<float>(.24*std::sin(i*2*juce::MathConstants<double>::pi*220/48000)));
    juce::WavAudioFormat wav;auto stream=source.createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),48000,1,24,{},0));if(!writer)return false;
    stream.release();check("source_written",writer->writeFromAudioSampleBuffer(buffer,0,57600));writer.reset();
    ClipData clip;clip.id="audio";clip.startSeconds=2;clip.durationSeconds=2;clip.sourceDurationSeconds=1.2;clip.sourceFile=source;
    clip.sourceTimeMap={{0,0},{.3,.2},{.8,.4},{1.6,.9},{2,1.2}};clip.gain=.75f;clip.gainEnvelope={{0,-3},{1,0},{2,-2}};
    const double boundaries[]={0,.3,.8,1.6,2};
    for (int i=0;i<4;++i)
    {
        NoteData n;n.id="n"+juce::String(i);n.label="baip";n.startSeconds=boundaries[i];n.durationSeconds=boundaries[i+1]-boundaries[i];
        n.midiNote=n.sourceMidiCenter=57;n.sourcePitchMeasured=true;n.gain=.9f;
        n.amplitudeEnvelope={{0,-1},{n.durationSeconds,0}};n.formantSemitones=.2f;n.tension=.1f;
        n.pitchControlPoints={{0,57},{n.durationSeconds,57}};
        for (int k=0;k<=static_cast<int>(std::round(n.durationSeconds/.005));++k)n.contour.push_back({k*.005,0,0,true});
        bindNativeNoteSource(n,clip);clip.notes.push_back(n);
    }
    TrackData track;track.id="native";track.pitchAlgorithm=PitchAlgorithm::world;track.normalizeVolume=false;track.allowNativeAudioOverlap=true;track.clips={clip};
    ProjectData data;data.tracks={track};ProjectModel model;model.replace(data);model.setNotesConnection({"n0","n1","n2","n3"},true);
    const auto before=model.snapshot();
    const auto locate=[](const ProjectData& d,const juce::String& id)->std::pair<ClipData,NoteData>{
        for(const auto& t:d.tracks)for(const auto& c:t.clips)for(const auto& n:c.notes)if(n.id==id)return {c,n};return {};};
    const auto stable=[&](const ProjectData& a,const ProjectData& b,const juce::String& id){
        const auto x=locate(a,id),y=locate(b,id);
        if(y.second.id!=id||!near(x.first.startSeconds+x.second.startSeconds,y.first.startSeconds+y.second.startSeconds)
            ||!near(x.second.durationSeconds,y.second.durationSeconds)||!near(x.second.midiNote,y.second.midiNote)
            ||x.second.contour.size()!=y.second.contour.size()||x.second.pitchControlPoints.size()!=y.second.pitchControlPoints.size()
            ||!near(x.second.gain,y.second.gain)
            ||!near(x.second.formantSemitones,y.second.formantSemitones)||!near(x.second.tension,y.second.tension))return false;
        for(double t=0;t<=x.second.durationSeconds;t+=.005)
            if(!near(x.first.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(x.first),x.second.startSeconds+t),
                y.first.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(y.first),y.second.startSeconds+t)))return false;
        // Disconnect may materialize the shared group envelope into local
        // endpoints. Compare the heard curve, not its previous storage layout.
        const auto envelopeAt=[&](const ProjectData& d,const auto& placed,double t)
        {
            for(const auto& tr:d.tracks)
            {
                const auto groups=nativeSharedEnvelopes(tr);
                if(const auto p=groups.find(id);p!=groups.end())
                    return nativeEnvelopeDbAt(*p->second.points,placed.first.startSeconds+placed.second.startSeconds+t);
            }
            return nativeEnvelopeDbAt(scaledAmplitudeEnvelope(placed.second.amplitudeEnvelope,placed.second.amplitudeEnvelopeBasePercent),t);
        };
        for(double t=0;t<=x.second.durationSeconds;t+=.005)
            if(!near(envelopeAt(a,x,t),envelopeAt(b,y,t)))return false;
        for(std::size_t i=0;i<x.second.contour.size();++i)
            if(!near(x.second.contour[i].timeSeconds,y.second.contour[i].timeSeconds)
                ||!near(x.second.contour[i].relativeCents,y.second.contour[i].relativeCents))return false;
        return true;
    };
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setFocusedClip(clip.id);
    roll.setTool(PianoRollComponent::Tool::note);roll.setPixelsPerSecond(240);roll.setRowHeight(24);roll.setSize(1400,roll.getHeight());roll.diagnosticRefresh();
    const auto has=[](const auto& list,int id){return std::find(list.begin(),list.end(),id)!=list.end();};
    roll.setSelectedNoteIds({"n0"});check("single_selection_cannot_disconnect",!has(roll.diagnosticEnabledNoteMenuIds("n0"),29));
    const auto event=[](juce::Component& c,juce::Point<float> at,juce::Point<float> down,int mods){
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(mods),0,0,0,0,0,
            &c,&c,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    const auto first=roll.diagnosticHitBounds(0),third=roll.diagnosticHitBounds(2);
    const juce::Point<float> from(first.getX()-12,first.getY()-18),to(third.getRight()-.5f,third.getBottom()+18);
    roll.mouseDown(event(roll,from,from,juce::ModifierKeys::rightButtonModifier));
    roll.mouseDrag(event(roll,to,from,juce::ModifierKeys::rightButtonModifier));
    roll.mouseUp(event(roll,to,from,juce::ModifierKeys::rightButtonModifier));juce::PopupMenu::dismissAllActiveMenus();
    check("right_marquee_selects_three",roll.selectedNoteIds().size()==3);
    check("native_context_offers_disconnect",has(roll.diagnosticEnabledNoteMenuIds("n0"),29));
    roll.applyNoteMenuChoice("n0",29,0,{});model.dispatchPendingMessages();roll.diagnosticRefresh();const auto detached=model.snapshot();
    check("four_independent_audio_regions",detached.tracks[0].clips.size()==4
        &&std::all_of(detached.tracks[0].clips.begin(),detached.tracks[0].clips.end(),[](const auto& c){return c.notes.size()==1;}));
    bool retained=true;for(int i=0;i<4;++i)retained &= stable(before,detached,"n"+juce::String(i));
    check("timing_source_clock_pitch_envelopes_preserved",retained);
    check("connections_and_compatibility_flags_removed",detached.nativeConnections.empty()
        &&std::all_of(detached.tracks[0].clips.begin(),detached.tracks[0].clips.end(),[](const auto& c){
            return !c.glideConnectedFromPrevious&&!c.glideConnectedToNext&&!c.notes[0].connectedToPrevious&&!c.notes[0].connectedToNext;}));
    check("only_context_fragment_selected_after_disconnect",roll.selectedNoteIds().size()==1&&roll.selectedNoteIds()[0]=="n0");
    check("already_disconnected_is_noop",!model.disconnectNativeAudio({"n0","n1","n2"}));
    check("first_segment_moves_independently",model.moveNativeNotes({"n0"},-.2,0));
    auto moved=model.snapshot();check("move_leaves_all_neighbors_untouched",stable(detached,moved,"n1")&&stable(detached,moved,"n2")&&stable(detached,moved,"n3"));
    check("middle_left_stretches_independently",model.resizeNativeNoteEdge("n1",-.15,true));moved=model.snapshot();
    check("middle_left_does_not_move_or_compress_neighbors",stable(detached,moved,"n2")&&stable(detached,moved,"n3"));
    const auto afterLeft=moved;check("middle_right_stretches_independently",model.resizeNativeNoteEdge("n1",.6,false));moved=model.snapshot();
    check("middle_right_does_not_move_neighbors",stable(afterLeft,moved,"n0")&&stable(afterLeft,moved,"n2")&&stable(afterLeft,moved,"n3"));
    check("tail_moves_independently",model.moveNativeNotes({"n2"},.45,0));moved=model.snapshot();
    check("tail_move_does_not_stretch_middle",stable(afterLeft,moved,"n3")&&near(locate(moved,"n1").second.durationSeconds,1.25));
    // Roll back only the independent moves, then the entire disconnect in one step.
    for(int i=0;i<4;++i)model.undo();check("undo_edits_returns_to_detached",model.snapshot().tracks[0].clips.size()==4&&stable(detached,model.snapshot(),"n1"));
    model.undo();check("one_undo_restores_shared_audio_and_connections",model.snapshot().tracks[0].clips.size()==1
        &&model.snapshot().nativeConnections.size()==before.nativeConnections.size());
    check("disconnect_two_fragments",model.disconnectNativeAudio({"n0","n1"}));const auto partial=model.snapshot();
    check("unselected_run_remains_connected",partial.tracks[0].clips.size()==3&&partial.tracks[0].clips.back().notes.size()==2
        &&partial.nativeConnections.size()==1&&partial.nativeConnections[0].leftNoteId=="n2"&&partial.nativeConnections[0].rightNoteId=="n3");
    juce::String error;const auto saved=folder.getChildFile("disconnected.hjpx");ProjectModel reopened;
    check("saved_project_reopens_independent_regions",model.save(saved,error)&&reopened.load(saved,error)
        &&reopened.snapshot().tracks[0].clips.size()==3&&reopened.snapshot().nativeConnections.size()==1);
    check("reopened_segment_moves_independently",reopened.moveNativeNotes({"n0"},.2,0)&&stable(partial,reopened.snapshot(),"n1")&&stable(partial,reopened.snapshot(),"n2"));
    // Independent regions with explicit joins also disconnect, without re-slicing.
    model.replace(detached);model.setNotesConnection({"n0","n1","n2"},true);
    check("cross_region_forced_links_disconnect",model.disconnectNativeAudio({"n0","n1","n2"})
        &&model.snapshot().nativeConnections.empty()&&model.snapshot().tracks[0].clips.size()==4);
    // Merged children retain separate source clocks and parent gain layers.
    auto merged=before;auto parent=merged.tracks[0].clips[0];auto child=parent;child.notes.clear();child.id="child";
    parent.parts={child};for(auto& n:parent.notes)n.clipPartId="child";merged.tracks[0].clips={parent};model.replace(merged);
    check("merged_source_child_disconnects",model.disconnectNativeAudio({"n0","n1","n2","n3"})&&model.snapshot().tracks[0].clips.size()==4);
    const auto mergedResult=model.snapshot();check("merged_source_gain_layers_preserved",near(mergedResult.tracks[0].clips[0].gain,parent.gain*child.gain)
        &&!mergedResult.tracks[0].clips[0].inheritedGainEnvelopes.empty());
    auto utau=before;utau.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;model.replace(utau);model.dispatchPendingMessages();roll.diagnosticRefresh();roll.setSelectedNoteIds({"n0","n1"});
    check("utau_has_no_disconnect_audio_item",!has(roll.diagnosticNoteMenuIds("n0"),29)&&!model.disconnectNativeAudio({"n0","n1"}));
    model.replace(detached);const auto rendered=model.snapshot();AudioEngine engine;engine.prepareToPlay(256,48000);engine.syncProject(rendered);
    for(int i=0;i<1000&&engine.renderProgress();++i)juce::Thread::sleep(10);
    WavExportOptions options;options.sampleRate=48000;options.channels=2;options.bitDepth=32;
    const auto output=folder.getChildFile("disconnected-render.wav");check("detached_audio_renders_and_exports",engine.hasCurrentRenderedAudio()&&engine.exportWav(output,error,track.id,2,4,options));
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output));bool audible=reader!=nullptr;
    if(reader){juce::AudioBuffer<float> sound(2,static_cast<int>(reader->lengthInSamples));reader->read(&sound,0,sound.getNumSamples(),0,true,true);
        for(int i=0;i<4;++i){const int begin=static_cast<int>((boundaries[i]+.05)*48000),length=static_cast<int>((boundaries[i+1]-boundaries[i]-.1)*48000);
            audible &= length>0&&sound.getRMSLevel(0,begin,length)>.01f;}}
    check("every_detached_segment_is_audible",audible);engine.releaseResources();
    std::cout<<"native_audio_disconnect_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
