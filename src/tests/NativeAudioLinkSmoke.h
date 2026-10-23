#pragma once
#include "../NativeAudioLink.h"
#include "../NativeAudioClipboard.h"
namespace hachi
{
inline bool runNativeAudioLinkSmoke(const juce::File& folder)
{
    if(!runNativeAudioDisconnectSmoke(folder.getChildFile("seed")))return false;
    bool ok=true;int checks=0;const auto check=[&](const char* key,bool pass){ok &= pass;++checks;std::cout<<key<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-5;};ProjectModel model;juce::String error;
    if(!model.load(folder.getChildFile("seed/disconnected.hjpx"),error))return false;
    model.disconnectNativeAudio({"n0","n1","n2","n3"});auto initial=model.snapshot();initial.tracks[0].allowNativeAudioOverlap=true;
    const auto secondSource=folder.getChildFile("second-source.wav");initial.tracks[0].clips[1].sourceFile.copyFileTo(secondSource);
    initial.tracks[0].clips[1].sourceFile=secondSource;model.replace(initial);
    const auto locate=[](const ProjectData& d,const juce::String& id)->std::pair<ClipData,NoteData>{
        for(const auto& t:d.tracks)for(const auto& c:t.clips)for(const auto& part:expandedClipParts(c))
            for(const auto& n:part.notes)if(n.id==id)return {part,n};return {};};
    const auto stable=[&](const ProjectData& a,const ProjectData& b,const juce::String& id){
        const auto x=locate(a,id),y=locate(b,id);
        if(y.second.id!=id||x.first.sourceFile!=y.first.sourceFile
            ||!near(x.first.startSeconds+x.second.startSeconds,y.first.startSeconds+y.second.startSeconds)
            ||!near(x.second.durationSeconds,y.second.durationSeconds)||!near(x.second.midiNote,y.second.midiNote)
            ||x.second.contour.size()!=y.second.contour.size()||x.second.amplitudeEnvelope.size()!=y.second.amplitudeEnvelope.size()
            ||!near(x.first.gain,y.first.gain)||!near(x.second.formantSemitones,y.second.formantSemitones))return false;
        for(double t=0;t<=x.second.durationSeconds;t+=.005)
            if(!near(x.first.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(x.first),x.second.startSeconds+t),
                y.first.sourceOffsetSeconds+nativeSourceTimeAt(nativeClipClock(y.first),y.second.startSeconds+t)))return false;
        return true;
    };
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(initial.tracks[0].id);roll.setFocusedClip(initial.tracks[0].clips[0].id);roll.diagnosticRefresh();
    const auto has=[](const auto& list,int id){return std::find(list.begin(),list.end(),id)!=list.end();};
    roll.setSelectedNoteIds({"n0"});check("single_selection_cannot_link",!has(roll.diagnosticEnabledNoteMenuIds("n0"),30));
    roll.setSelectedNoteIds({"n0","n1","n2"});check("multi_source_selection_offers_link",has(roll.diagnosticEnabledNoteMenuIds("n0"),30));
    roll.applyNoteMenuChoice("n0",30,0,{});model.dispatchPendingMessages();roll.diagnosticRefresh();const auto joined=model.snapshot();
    const auto parent=std::find_if(joined.tracks[0].clips.begin(),joined.tracks[0].clips.end(),[](const auto& c){return c.nativeAudioLinked;});
    check("three_sources_linked_without_unselected_neighbor",parent!=joined.tracks[0].clips.end()&&parent->parts.size()==3&&parent->notes.size()==3&&joined.tracks[0].clips.size()==2);
    check("join_keeps_audio_timing_pitch_and_source_clocks",stable(initial,joined,"n0")&&stable(initial,joined,"n1")&&stable(initial,joined,"n2")&&stable(initial,joined,"n3"));
    check("only_context_fragment_selected_after_link",roll.selectedNoteIds().size()==1&&roll.selectedNoteIds()[0]=="n0");
    check("already_linked_is_noop",!model.linkNativeAudio({"n0","n1","n2"}));
    check("linked_head_moves",model.moveNativeNotes({"n0"},-.1,0));auto moved=model.snapshot();
    check("head_move_stretches_middle",near(locate(moved,"n1").second.durationSeconds,locate(joined,"n1").second.durationSeconds+.1));
    check("unselected_material_stays_independent",stable(joined,moved,"n3"));
    check("linked_tail_moves",model.moveNativeNotes({"n2"},.2,0));moved=model.snapshot();
    check("tail_move_stretches_middle",near(locate(moved,"n1").second.durationSeconds,locate(joined,"n1").second.durationSeconds+.3));
    const auto beforeResize=moved;check("linked_middle_right_stretches",model.resizeNativeNoteEdge("n1",.15,false));moved=model.snapshot();
    check("middle_stretch_moves_tail_without_compressing_it",near(locate(moved,"n2").first.startSeconds+locate(moved,"n2").second.startSeconds,
        locate(beforeResize,"n2").first.startSeconds+locate(beforeResize,"n2").second.startSeconds+.15)
        &&near(locate(moved,"n2").second.durationSeconds,locate(beforeResize,"n2").second.durationSeconds));
    check("linked_middle_left_stretches",model.resizeNativeNoteEdge("n1",-.12,true));moved=model.snapshot();
    check("middle_left_moves_head_without_compressing_it",near(locate(moved,"n0").second.durationSeconds,locate(joined,"n0").second.durationSeconds));
    check("unselected_neighbor_still_unchanged",stable(joined,moved,"n3"));
    for(int i=0;i<4;++i)model.undo();check("undo_edits_restores_linked_timing",stable(joined,model.snapshot(),"n0")&&stable(joined,model.snapshot(),"n1"));
    model.undo();check("one_undo_restores_independent_sources",model.snapshot().tracks[0].clips.size()==4&&stable(initial,model.snapshot(),"n1"));
    model.replace(joined);const auto saved=folder.getChildFile("linked.hjpx");ProjectModel reopened;
    check("linked_project_saved_and_reopens",model.save(saved,error)&&reopened.load(saved,error));
    check("reopened_group_retains_linked_editing",!nativeAudioLinkAvailable(reopened.snapshot(),{"n0","n1","n2"})&&reopened.moveNativeNotes({"n0"},-.1,0)
        &&near(locate(reopened.snapshot(),"n1").second.durationSeconds,locate(joined,"n1").second.durationSeconds+.1));
    const auto clipboard=copyNativeAudioNotes(joined,{"n0","n1","n2"});
    check("clipboard_preserves_multi_source_link_group",clipboard.clips.size()==1&&clipboard.clips[0].nativeAudioLinked&&clipboard.clips[0].parts.size()==3);
    const auto ids=model.insertNativeAudioClips(initial.tracks[0].id,clipboard.clips,clipboard.connections,6);
    const auto pasted=model.snapshot();check("pasted_group_keeps_source_ownership",ids.size()==3&&pasted.tracks[0].clips.back().nativeAudioLinked
        &&std::all_of(pasted.tracks[0].clips.back().notes.begin(),pasted.tracks[0].clips.back().notes.end(),[](const auto& n){return n.clipPartId.isNotEmpty();}));
    check("pasted_group_still_edits_as_linked",model.moveNativeNotes({ids.front()},-.1,0)
        &&near(locate(model.snapshot(),ids[1]).second.durationSeconds,locate(pasted,ids[1]).second.durationSeconds+.1));
    model.replace(joined);check("link_can_be_disconnected_again",model.disconnectNativeAudio({"n0","n1","n2"})&&model.snapshot().tracks[0].clips.size()==4);
    const auto detached=model.snapshot();check("disconnected_group_moves_independently",model.moveNativeNotes({"n0"},-.1,0)&&stable(detached,model.snapshot(),"n1"));
    check("detached_sources_can_link_again",model.linkNativeAudio({"n0","n1","n2"}));
    auto utau=initial;utau.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;model.replace(utau);model.dispatchPendingMessages();roll.diagnosticRefresh();roll.setSelectedNoteIds({"n0","n1"});
    check("utau_has_no_link_audio_item",!has(roll.diagnosticNoteMenuIds("n0"),30)&&!model.linkNativeAudio({"n0","n1"}));
    AudioEngine engine;engine.prepareToPlay(256,48000);WavExportOptions options;options.sampleRate=48000;options.channels=2;options.bitDepth=32;
    const auto exportAudio=[&](const ProjectData& d,const juce::File& f){engine.syncProject(d);for(int i=0;i<1000&&engine.renderProgress();++i)juce::Thread::sleep(10);
        return engine.hasCurrentRenderedAudio()&&engine.exportWav(f,error,initial.tracks[0].id,2,4,options);};
    const auto originalWav=folder.getChildFile("before-link.wav"),linkedWav=folder.getChildFile("after-link.wav");
    check("multi_source_audio_renders_before_and_after_link",exportAudio(initial,originalWav)&&exportAudio(joined,linkedWav));
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(originalWav)),b(formats.createReaderFor(linkedWav));
    bool same=a&&b&&a->lengthInSamples==b->lengthInSamples;float maximum=1;
    if(same){const auto count=static_cast<int>(a->lengthInSamples);juce::AudioBuffer<float> x(2,count),y(2,count);
        a->read(&x,0,count,0,true,true);b->read(&y,0,count,0,true,true);maximum=0;
        for(int ch=0;ch<2;++ch)for(int i=0;i<count;++i)maximum=std::max(maximum,std::abs(x.getSample(ch,i)-y.getSample(ch,i)));
        same=maximum<1.e-4f&&x.getRMSLevel(0,0,count)>.01f;}
    check("link_alone_preserves_rendered_audio",same);std::cout<<"audio_max_difference="<<maximum<<std::endl;engine.releaseResources();
    std::cout<<"native_audio_link_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
