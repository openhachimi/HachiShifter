#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualMoveSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"owner-right":"owner-left";clip.sourceFile=output.getChildFile(i?"right.wav":"left.wav");
        clip.startSeconds=i?1.:0.;clip.durationSeconds=clip.sourceDurationSeconds=i?1.:1.2;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        juce::AudioBuffer<float> pcm(1,juce::roundToInt(clip.durationSeconds*48000));
        for(int s=0;s<pcm.getNumSamples();++s)pcm.setSample(0,s,.1f*std::sin(juce::MathConstants<double>::twoPi*(i?233:173)*s/48000.));
        auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,16,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;writer.reset();
        NoteData note;note.id=i?"right":"left";note.label="-";note.durationSeconds=clip.durationSeconds;note.nativeUnpitched=true;clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);const auto id=model.createNativeVirtualConnection({"left","right"},false,error);check("binding_created_once",id.isNotEmpty());if(id.isEmpty())return false;
    const auto bound=model.snapshot();
    const auto retained=[&](const ProjectData& data)
    {return data.nativeVirtualGroups.size()==1&&data.nativeVirtualGroups[0].id==id&&data.nativeVirtualGroups[0].enabled&&data.nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"left","right"};};
    const auto audible=[&](const ProjectData& data)
    {
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(data);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;
        while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}
        if(!engine.activeRenderWarnings().isEmpty()){error=engine.activeRenderWarnings();return false;}
        juce::AudioBuffer<float> pcm(2,240000);pcm.clear();engine.setPosition(0);engine.play();
        for(int offset=0;offset<pcm.getNumSamples();offset+=257)engine.getNextAudioBlock({&pcm,offset,std::min(257,pcm.getNumSamples()-offset)});
        engine.stop();return pcm.getMagnitude(0,pcm.getNumSamples())>.01f;
    };
    model.moveClip("owner-right",3.);auto moved=model.snapshot();
    check("horizontal_separation_never_dissolves_or_reorders_binding",retained(moved)&&moved.tracks[0].clips[1].startSeconds==3.);
    ProjectData projected;std::vector<NativeVirtualRenderGroup> groups;
    check("retained_gap_resolves_without_new_candidate_rejection",prepareNativeVirtualRender(moved,projected,groups,error)&&groups.size()==1&&groups[0].plan.members[0].noteId=="left"&&groups[0].plan.members[1].noteId=="right");
    if(!groups.empty())
    {
        const auto before=nativeVirtualContributions(groups[0].plan,.5),gap=nativeVirtualContributions(groups[0].plan,2.),after=nativeVirtualContributions(groups[0].plan,3.5);
        check("gap_keeps_both_real_sources_audible_without_inventing_middle_audio",before[0]==1&&before[1]==0&&gap[0]==0&&gap[1]==0&&after[0]==0&&after[1]==1);
    }
    check("normal_audio_callback_does_not_silence_moved_group",audible(moved));
    model.undo();check("move_undo_retains_same_binding",retained(model.snapshot())&&model.snapshot().tracks[0].clips[1].startSeconds==1.);model.redo();check("move_redo_retains_same_binding",retained(model.snapshot())&&model.snapshot().tracks[0].clips[1].startSeconds==3.);
    model.transposeNote("right",7);check("vertical_transpose_keeps_exact_connection_identity",retained(model.snapshot())&&model.snapshot().tracks[0].clips[1].notes[0].midiNote==67);
    check("moved_binding_can_toggle_off_on_without_requalification",model.setNativeVirtualConnectionEnabled(id,false,error)&&model.setNativeVirtualConnectionEnabled(id,true,error)&&retained(model.snapshot()));
    const auto file=output.getChildFile("moved-bound.hjpx");ProjectModel cold;
    check("moved_binding_cold_roundtrip_keeps_original_direction",model.save(file,error)&&cold.load(file,error)&&retained(cold.snapshot()));
    auto reversed=bound;reversed.tracks[0].clips[0].startSeconds=2.;reversed.tracks[0].clips[1].startSeconds=0.;model.resetDocument(reversed);
    check("crossed_positions_do_not_change_topology_order",prepareNativeVirtualRender(model.snapshot(),projected,groups,error)&&retained(model.snapshot())&&groups[0].plan.members[0].noteId=="left"&&groups[0].plan.members[1].noteId=="right");
    check("crossed_positions_still_produce_real_audio",audible(model.snapshot()));
    auto nested=bound;nested.tracks[0].clips[1].startSeconds=.1;model.resetDocument(nested);
    check("nested_support_does_not_disconnect_existing_relation",prepareNativeVirtualRender(model.snapshot(),projected,groups,error)&&retained(model.snapshot()));
    bool partition=!groups.empty();if(partition)for(double t=0;t<1.2;t+=.001){const auto w=nativeVirtualContributions(groups[0].plan,t);partition&=std::abs(w[0]+w[1]-1.)<1.e-8;}
    check("nested_real_support_remains_a_single_contribution_partition",partition);
    NativeVirtualConnectionPlan candidate;check("unrelated_new_gap_still_not_auto_connected",!planNativeVirtualConnection(moved.tracks[0],{"left","right"},candidate,error));
    model.resetDocument(bound);
    const auto nativeMoved=model.moveNativeNotes({"right"},.25,3,&error);
    check("piano_roll_native_move_keeps_same_virtual_binding",nativeMoved&&retained(model.snapshot()));
    check("piano_roll_native_move_remains_renderable",prepareNativeVirtualRender(model.snapshot(),projected,groups,error));
    model.resetDocument(bound);const auto allMoved=model.moveNativeNotes({"left","right"},.5,-2,&error);
    check("whole_group_native_move_keeps_original_order_and_identity",allMoved&&retained(model.snapshot())&&prepareNativeVirtualRender(model.snapshot(),projected,groups,error));
    const auto original=model.snapshot();check("only_explicit_dissolve_removes_connection",model.removeNativeVirtualConnection(id,error)&&model.snapshot().nativeVirtualGroups.empty());model.undo();check("explicit_dissolve_undo_restores_binding",retained(model.snapshot())&&model.snapshot().tracks[0].clips[0].sourceFile==original.tracks[0].clips[0].sourceFile);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
