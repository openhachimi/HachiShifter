#pragma once
#include "../PianoRollComponent.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualHandoverGestureSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="owner"+juce::String(i);clip.sourceFile=folder.getChildFile("source"+juce::String(i)+".wav");clip.startSeconds=i*.8;clip.durationSeconds=clip.sourceDurationSeconds=1.;clip.sourceTimeMap={{0,0},{1,1}};
        {juce::AudioBuffer<float> pcm(1,96000);for(int sample=0;sample<96000;++sample)pcm.setSample(0,sample,i==1?.125f:.25f);auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,96000))return false;}
        NoteData note;note.id="note"+juce::String(i);note.durationSeconds=1.;note.nativeUnpitched=true;note.midiNote=60+float(i*4);for(int frame=0;frame<=200;++frame)note.contour.push_back({frame*.005,0,0,false});clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};ProjectModel model;model.resetDocument(data);const auto id=model.createNativeVirtualConnection({"note0","note1","note2"},false,error);if(id.isEmpty())return false;
    const auto original=model.contentFingerprint();I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setTool(PianoRollComponent::Tool::note);roll.setSelectedNoteIds({"note0","note1","note2"});roll.setPixelsPerSecond(500);roll.setRowHeight(22);roll.setBounds(0,0,1600,1100);
    const auto refresh=[&]{model.dispatchPendingMessages();roll.diagnosticRefresh();juce::Image image(juce::Image::ARGB,1600,1100,true,juce::SoftwareImageType{});juce::Graphics g(image);roll.paintEntireComponent(g,true);};refresh();
    const auto mouse=[&](juce::Point<float> at,juce::Point<float> down,int clicks=1){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),clicks,at!=down);};
    const auto left=roll.diagnosticVirtualHandoverGrip(id,"note0","note1",true),right=roll.diagnosticVirtualHandoverGrip(id,"note0","note1",false);
    check("selected_route_has_distinct_source_contribution_window_grips",left&&right&&left->getDistanceFrom(*right)>20);
    if(left){const auto moved=left->translated(25,0);roll.mouseDown(mouse(*left,*left));roll.mouseDrag(mouse(moved,*left));const auto preview=roll.diagnosticVirtualHandoverWindow(id,"note0","note1");check("left_grip_previews_without_model_write",preview&&std::abs(preview->first-.85)<1.e-7&&model.contentFingerprint()==original);roll.mouseUp(mouse(moved,*left));refresh();}
    check("left_grip_commit_changes_only_the_chosen_window",model.snapshot().nativeVirtualGroups[0].handoverWindows.size()==1&&std::abs(roll.diagnosticVirtualHandoverWindow(id,"note0","note1")->first-.85)<1.e-7&&std::abs(roll.diagnosticVirtualHandoverWindow(id,"note1","note2")->first-1.6)<1.e-7);
    const auto end=roll.diagnosticVirtualHandoverGrip(id,"note0","note1",false);if(end){const auto moved=end->translated(-25,0);roll.mouseDown(mouse(*end,*end));roll.mouseDrag(mouse(moved,*end));roll.mouseUp(mouse(moved,*end));refresh();}
    check("right_grip_commits_inside_actual_source_overlap",std::abs(roll.diagnosticVirtualHandoverWindow(id,"note0","note1")->second-.95)<1.e-7);
    const auto edited=model.snapshot();const auto fingerprint=model.contentFingerprint();const auto cap=roll.diagnosticVirtualHandoverGrip(id,"note0","note1",true);
    if(cap){const auto moved=cap->translated(-500,0);roll.mouseDown(mouse(*cap,*cap));roll.mouseDrag(mouse(moved,*cap));const auto limited=roll.diagnosticVirtualHandoverWindow(id,"note0","note1");check("outward_drag_is_limited_by_true_source_support",limited&&limited->first>=.8-1.e-8);roll.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));roll.mouseUp(mouse(moved,*cap));refresh();}
    check("escape_discards_handover_preview",model.contentFingerprint()==fingerprint);
    const auto file=folder.getChildFile("hand-edited-window.hjpx");ProjectModel cold;check("source_window_edits_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups[0].handoverWindows.size()==1);
    const auto reset=roll.diagnosticVirtualHandoverGrip(id,"note0","note1",true);if(reset){roll.mouseDoubleClick(mouse(*reset,*reset,2));refresh();}
    check("double_click_grip_restores_only_this_automatic_window",model.snapshot().nativeVirtualGroups[0].handoverWindows.empty()&&std::abs(roll.diagnosticVirtualHandoverWindow(id,"note0","note1")->first-.8)<1.e-7);model.undo();check("window_reset_is_one_exact_undo",model.contentFingerprint()==fingerprint);
    const auto render=[&](const ProjectData& state,juce::AudioBuffer<float>& buffer)
    {AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(state);const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000.;while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}if(!engine.activeRenderWarnings().isEmpty()){error=engine.activeRenderWarnings();return false;}buffer.setSize(2,144000);buffer.clear();engine.setPosition(0);engine.play();for(int sample=0;sample<144000;sample+=257)engine.getNextAudioBlock({&buffer,sample,std::min(257,144000-sample)});engine.stop();return true;};
    juce::AudioBuffer<float> actual;std::array<juce::AudioBuffer<float>,3> stems;bool real=render(edited,actual);for(int i=0;i<3;++i){auto one=data;one.tracks[0].clips={track.clips[std::size_t(i)]};real&=render(one,stems[std::size_t(i)]);}
    NativeVirtualConnectionPlan plan;real&=resolveNativeVirtualGroup(edited.tracks[0],edited.nativeVirtualGroups[0],plan,error);double peak=0;
    if(real)for(int ch=0;ch<2;++ch)for(int sample=0;sample<144000;++sample){double expected=0;for(int i=0;i<3;++i)expected+=nativeVirtualMemberContribution(plan,std::size_t(i),sample/48000.)*stems[std::size_t(i)].getSample(ch,sample);peak=std::max(peak,double(std::abs(actual.getSample(ch,sample))));}
    check("real_group_assembly_consumes_the_mouse_edited_contribution_windows",real&&actual.getNumSamples()==144000&&std::isfinite(peak));log+="actual_group_PCM_peak="+juce::String(peak,12)+"\n";
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
