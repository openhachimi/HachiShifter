#pragma once
#include "../NativeCoupledSelectionMove.h"
#include "../NativeVirtualGroupIO.h"
#include "../PianoRollComponent.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualCoupledMoveSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    const std::vector<juce::String> ids{"A","B","C"};
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="owner-"+ids[std::size_t(i)];clip.sourceFile=folder.getChildFile(ids[std::size_t(i)]+".wav");clip.startSeconds=i;clip.durationSeconds=clip.sourceDurationSeconds=1.;clip.sourceTimeMap={{0,0},{1,1}};
        {juce::AudioBuffer<float> pcm(1,48000);for(int sample=0;sample<48000;++sample)pcm.setSample(0,sample,float(.1*std::sin(sample*juce::MathConstants<double>::twoPi*(180+i*40)/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,48000))return false;}
        NoteData note;note.id=ids[std::size_t(i)];note.durationSeconds=1.;note.midiNote=note.sourceMidiCenter=60+float(i*2);note.nativeUnpitched=true;note.consonantSeconds=.05;for(int frame=0;frame<=200;++frame)note.contour.push_back({frame*.005,0,0,false});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);const auto group=model.createNativeVirtualConnection(ids,false,error);
    check("virtual_chain_uses_coupled_editing_by_default",group.isNotEmpty()&&model.snapshot().nativeVirtualGroups[0].coupledMovement);if(group.isEmpty())return false;
    const auto original=model.snapshot();const auto fingerprint=model.contentFingerprint();
    const auto spans=[](const ProjectData& data){std::vector<std::pair<double,double>> out;for(const auto& clip:data.tracks[0].clips){const auto& n=clip.notes.front();out.emplace_back(clip.startSeconds+n.startSeconds,clip.startSeconds+n.startSeconds+n.durationSeconds);}return out;};
    const auto adjacent=[&](const ProjectData& data){const auto p=spans(data);return p.size()==3&&std::abs(p[0].second-p[1].first)<1.e-7&&std::abs(p[1].second-p[2].first)<1.e-7;};
    check("moving_middle_note_reuses_shared_boundary_time_warp",model.moveNativeNotes({"B"},.12,3,&error));
    auto moved=model.snapshot();const auto movedSpans=spans(moved);
    log+="moved A="+juce::String(movedSpans[0].first,9)+".."+juce::String(movedSpans[0].second,9)+" B="+juce::String(movedSpans[1].first,9)+".."+juce::String(movedSpans[1].second,9)+" C="+juce::String(movedSpans[2].first,9)+".."+juce::String(movedSpans[2].second,9)+"\n";
    check("actual_neighbor_seams_stay_joined_not_only_saved_ids",adjacent(moved)&&std::abs(movedSpans[1].first-1.12)<1.e-7&&moved.tracks[0].clips[1].notes[0].midiNote==65);
    bool physical=true;for(int i=0;i<3;++i){const auto& before=track.clips[std::size_t(i)];const auto& after=moved.tracks[0].clips[std::size_t(i)];physical&=after.id==before.id&&after.sourceFile==before.sourceFile&&after.sourceOffsetSeconds==before.sourceOffsetSeconds&&after.sourceDurationSeconds==before.sourceDurationSeconds&&after.parts.empty()&&!after.nativeAudioLinked;const auto clock=nativeClipClock(after);physical&=std::abs(clock.front().sourceSeconds)<1.e-7&&std::abs(clock.back().sourceSeconds-1.)<1.e-7;}
    check("independent_owner_identity_and_original_source_ranges_are_preserved",physical&&moved.nativeVirtualGroups.size()==1&&moved.nativeVirtualGroups[0].id==group&&moved.nativeVirtualGroups[0].orderedNoteIds==ids&&moved.nativeConnections.empty());
    model.undo();check("coupled_edit_is_one_exact_undo",model.contentFingerprint()==fingerprint);model.redo();
    const auto saved=folder.getChildFile("coupled-native-virtual.hjpx");ProjectModel cold;check("coupled_clock_projection_survives_cold_reload",model.save(saved,error)&&cold.load(saved,error)&&adjacent(cold.snapshot())&&cold.snapshot().nativeVirtualGroups[0].coupledMovement);
    model.resetDocument(original);check("left_resize_updates_only_the_designated_adjacent_seam",model.resizeNativeNoteEdge("B",.08,true)&&adjacent(model.snapshot())&&std::abs(spans(model.snapshot())[1].first-1.08)<1.e-7);
    model.resetDocument(original);check("right_resize_keeps_the_next_source_joined",model.resizeNativeNoteEdge("B",.08,false)&&adjacent(model.snapshot())&&std::abs(spans(model.snapshot())[1].second-2.08)<1.e-7);
    model.resetDocument(original);check("whole_group_move_remains_rigid",model.moveNativeNotes(ids,.15,0,&error)&&adjacent(model.snapshot())&&std::abs(spans(model.snapshot())[0].first-.15)<1.e-7&&std::abs(spans(model.snapshot())[2].second-3.15)<1.e-7);
    model.resetDocument(original);model.setNativeVirtualConnectionEnabled(group,false,error);check("AB_disabled_relationship_still_keeps_editing_binding",model.moveNativeNotes({"B"},.1,0,&error)&&adjacent(model.snapshot())&&!model.snapshot().nativeVirtualGroups[0].enabled);
    model.resetDocument(original);I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.diagnosticRefresh();
    const auto menu=roll.diagnosticEnabledNoteMenuIds("B");check("context_menu_exposes_explicit_independent_movement_choice",std::find(menu.begin(),menu.end(),PianoRollComponent::virtualMovementItem)!=menu.end());
    roll.applyNoteMenuChoice("B",PianoRollComponent::virtualMovementItem,0,{});const auto modeFP=model.contentFingerprint();
    check("independent_mode_retains_members_and_leaves_other_owners_fixed",!model.snapshot().nativeVirtualGroups[0].coupledMovement&&model.moveNativeNotes({"B"},.12,0,&error)&&spans(model.snapshot())[0]==spans(original)[0]&&spans(model.snapshot())[2]==spans(original)[2]&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==ids);model.undo();check("independent_move_undo_preserves_the_explicit_mode",model.contentFingerprint()==modeFP);
    const auto freeFile=folder.getChildFile("independent.hjpx");ProjectModel freeCold;check("explicit_independent_mode_is_persisted",model.save(freeFile,error)&&freeCold.load(freeFile,error)&&!freeCold.snapshot().nativeVirtualGroups[0].coupledMovement);
    model.resetDocument(original);check("large_crossing_move_retains_exact_path_instead_of_silent_clamp",model.moveNativeNotes({"B"},2.2,0,&error)&&std::abs(spans(model.snapshot())[1].first-3.2)<1.e-7&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==ids);
    auto gap=model.snapshot();const auto unchangedA=spans(gap)[0];model.moveNativeNotes({"B"},.1,0,&error);check("separated_route_does_not_fabricate_support_to_fill_the_gap",spans(model.snapshot())[0]==unchangedA&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==ids);
    auto multi=original;auto da=multi.tracks[0].clips[0].notes.front();da.id="unrelated-da";da.durationSeconds=.2;multi.tracks[0].clips[0].notes.push_back(da);ProjectModel scoped;scoped.resetDocument(multi);const auto safe=scoped.contentFingerprint();
    check("multi_note_owner_is_not_silently_warped_through_unrelated_da",!scoped.moveNativeNotes({"B"},.1,0,&error)&&!error.isEmpty()&&scoped.contentFingerprint()==safe);
    auto schema=nativeVirtualGroupTree(original.nativeVirtualGroups[0]);schema.setProperty("coupledMovement","false",nullptr);NativeVirtualGroup decoded;check("movement_mode_uses_strict_boolean_codec",!nativeVirtualGroupFromTree(schema,decoded,error));
    const auto render=[&](const ProjectData& state,juce::AudioBuffer<float>& pcm)
    {AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(state);const auto until=juce::Time::getMillisecondCounterHiRes()+10000.;while(juce::Time::getMillisecondCounterHiRes()<until){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}if(!engine.activeRenderWarnings().isEmpty()){error=engine.activeRenderWarnings();return false;}pcm.setSize(2,144000);pcm.clear();engine.setPosition(0);engine.play();for(int i=0;i<144000;i+=257)engine.getNextAudioBlock({&pcm,i,std::min(257,144000-i)});engine.stop();return true;};
    juce::AudioBuffer<float> live,reloaded;const bool audio=render(moved,live)&&render(cold.snapshot(),reloaded);bool same=audio;
    for(int channel=0;same&&channel<2;++channel)same=std::equal(live.getReadPointer(channel),live.getReadPointer(channel)+144000,reloaded.getReadPointer(channel));
    bool seamAudible=audio;for(const auto time:{movedSpans[1].first,movedSpans[2].first}){const int from=int(std::round(time*48000.))-240;seamAudible&=live.getRMSLevel(0,from,480)>.0001f;}
    check("actual_warped_source_playback_has_no_silent_seams_and_cold_PCM_is_exact",audio&&seamAudible&&same);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
