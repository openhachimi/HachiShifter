#pragma once
#include "../NativeVirtualAutoConnect.h"
#include "../NativeVirtualRenderPlan.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualSameFileSkipSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto source=folder.getChildFile("a_b_c.wav");
    {juce::AudioBuffer<float> pcm(1,48000);double phase=0;for(int s=0;s<48000;++s){const auto t=s/48000.;const auto hz=t<.3?180.:t<.6?260.:330.;phase+=juce::MathConstants<double>::twoPi*hz/48000.;pcm.setSample(0,s,float(.1*std::sin(phase)));}auto stream=source.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,48000))return false;}
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"owner-C":"owner-A";clip.sourceFile=source;clip.startSeconds=i?.3:0.;clip.durationSeconds=clip.sourceDurationSeconds=.3;clip.sourceOffsetSeconds=i?.6:0.;clip.sourceTimeMap={{0,0},{.3,.3}};
        NoteData note;note.id=i?"C":"A";note.durationSeconds=.3;note.nativeUnpitched=true;note.midiNote=note.sourceMidiCenter=i?64:57;for(int f=0;f<=60;++f)note.contour.push_back({f*.005,0,0,false});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);const auto original=model.contentFingerprint();
    check("audio_link_a_to_c_from_same_file_uses_virtual_authority",model.linkNativeAudio({"A","C"})&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].sharedPitch&&model.snapshot().tracks[0].clips.size()==2&&model.snapshot().nativeConnections.empty());
    if(model.snapshot().nativeVirtualGroups.empty())return false;
    const auto groupId=model.snapshot().nativeVirtualGroups[0].id;const auto linked=model.snapshot();
    check("skipped_b_is_not_inserted_or_baked_into_a_new_source",linked.tracks[0].clips[0].sourceFile==source&&linked.tracks[0].clips[1].sourceFile==source&&linked.tracks[0].clips[1].sourceOffsetSeconds==.6&&linked.tracks[0].clips[0].parts.empty()&&linked.tracks[0].clips[1].parts.empty());
    NativeVirtualConnectionPlan plan;check("source_gap_is_preserved_while_target_cut_is_joined",resolveNativeVirtualGroup(linked.tracks[0],linked.nativeVirtualGroups[0],plan,error)&&std::abs(plan.members[0].sourceToSeconds-.3)<1.e-8&&std::abs(plan.members[1].sourceFromSeconds-.6)<1.e-8&&std::abs(plan.handovers[0].from-.3)<1.e-8);
    model.undo();check("same_file_virtual_assembly_is_one_undoable_action",model.contentFingerprint()==original);model.redo();
    const auto file=folder.getChildFile("skip-b.hjpx");ProjectModel cold;
    check("same_file_noncontinuous_route_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups.size()==1&&cold.snapshot().nativeVirtualGroups[0].id==groupId&&cold.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"A","C"});
    auto repeated=seed;repeated.tracks[0].clips[1].sourceOffsetSeconds=0.;NativeVirtualConnectionPlan repeatPlan;
    check("repeating_a_source_region_is_virtual_not_a_partial_cut",planNativeVirtualConnection(repeated.tracks[0],{"A","C"},repeatPlan,error));
    auto reversed=seed;reversed.tracks[0].clips[0].sourceOffsetSeconds=.6;reversed.tracks[0].clips[1].sourceOffsetSeconds=0.;
    check("reversed_source_order_with_forward_target_order_is_allowed",planNativeVirtualConnection(reversed.tracks[0],{"A","C"},repeatPlan,error));
    auto warped=seed;warped.tracks[0].clips[1].sourceOffsetSeconds=.1;warped.tracks[0].clips[1].sourceDurationSeconds=.8;warped.tracks[0].clips[1].sourceTimeMap={{0,.5},{.3,.8}};
    check("continuity_is_read_from_the_real_source_clock_not_offset_alone",planNativeVirtualConnection(warped.tracks[0],{"A","C"},repeatPlan,error)&&std::abs(repeatPlan.members[1].sourceFromSeconds-.6)<1.e-8);
    auto continuous=seed;continuous.tracks[0].clips[1].sourceOffsetSeconds=.3;
    ProjectModel partial;partial.resetDocument(continuous);check("genuine_contiguous_source_stays_with_the_existing_partial_authority",partial.linkNativeAudio({"A","C"})&&partial.snapshot().nativeVirtualGroups.empty()&&partial.snapshot().tracks[0].clips.size()==1&&partial.snapshot().tracks[0].clips[0].nativeAudioLinked);
    const auto render=[&](const ProjectData& state,juce::AudioBuffer<float>& buffer)
    {AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(state);const auto until=juce::Time::getMillisecondCounterHiRes()+10000.;while(juce::Time::getMillisecondCounterHiRes()<until){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}if(!engine.activeRenderWarnings().isEmpty()){error=engine.activeRenderWarnings();return false;}buffer.setSize(2,48000);buffer.clear();engine.setPosition(0);engine.play();for(int s=0;s<48000;s+=257)engine.getNextAudioBlock({&buffer,s,std::min(257,48000-s)});engine.stop();return true;};
    juce::AudioBuffer<float> before,after;const bool real=render(seed,before)&&render(linked,after);double maximum=0;if(real)for(int ch=0;ch<2;++ch)for(int s=0;s<48000;++s)maximum=std::max(maximum,double(std::abs(before.getSample(ch,s)-after.getSample(ch,s))));
    check("actual_callback_reads_a_then_c_without_synthesizing_b",real&&maximum<2.e-6&&after.getMagnitude(15000,9000)>.01f);
    auto autoSeed=seed;for(auto& clip:autoSeed.tracks[0].clips){clip.notes[0].nativeUnpitched=false;for(auto& p:clip.notes[0].contour)p.voiced=true;}
    const auto automatic=autoConnectNativeVirtualImport(autoSeed,{true,false});check("opt_in_discovery_also_allows_same_file_source_skips",automatic.created==1&&autoSeed.nativeVirtualGroups.size()==1);
    model.moveNativeNotes({"C"},.15,2,&error);
    check("moving_same_file_noncontinuous_members_does_not_reclassify_or_dissolve",model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].id==groupId&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"A","C"}&&std::abs(model.snapshot().tracks[0].clips[1].sourceOffsetSeconds-.6)<1.e-8);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
