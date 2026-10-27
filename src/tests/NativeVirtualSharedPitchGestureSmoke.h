#pragma once
#include "../PianoRollComponent.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualSharedPitchGestureSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData data;TrackData track;track.id="linked-pitch";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="region"+juce::String(i);clip.startSeconds=1+i*.6;clip.durationSeconds=clip.sourceDurationSeconds=.6;clip.sourceTimeMap={{0,0},{.6,.6}};clip.sourceFile=folder.getChildFile("source"+juce::String(i)+".wav");
        {juce::AudioBuffer<float> pcm(1,48000);for(int s=0;s<48000;++s)pcm.setSample(0,s,float(.1*std::sin(s*juce::MathConstants<double>::twoPi*220/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,48000))return false;}
        NoteData note;note.id="part"+juce::String(i);note.durationSeconds=.6;note.midiNote=60+float(i);note.sourceMidiCenter=57;note.sourcePitchMeasured=true;
        note.pitchControlPoints={{0,note.midiNote},{.25,note.midiNote+1},{.6,note.midiNote}};
        for(int f=0;f<=120;++f){PitchPoint p;p.timeSeconds=f*.005;p.voiced=true;p.hasManualTarget=true;p.manualTargetCents=(evaluatePitchCurve(note.pitchControlPoints,p.timeSeconds)-note.midiNote)*100;note.contour.push_back(p);}
        clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};ProjectModel model;model.resetDocument(data);
    check("new_audio_link_creates_shared_virtual_mode",model.linkNativeAudio({"part0","part1","part2"})&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].sharedPitch&&model.snapshot().tracks[0].clips.size()==3);
    if(model.snapshot().nativeVirtualGroups.empty())return false;
    I18n strings;PianoRollComponent roll(model,strings);roll.setBounds(0,0,1100,900);roll.setPixelsPerSecond(300);roll.setFocusedTrack(track.id);roll.setTool(PianoRollComponent::Tool::points);roll.setShowNativeWaveforms(false);roll.setShowOriginalPitchLine(false);roll.diagnosticRefresh();
    // Paint establishes the REAL virtual-marker hit boxes as in a visible GUI.
    const auto refresh=[&]{model.dispatchPendingMessages();roll.diagnosticRefresh();juce::Image image(juce::Image::ARGB,1100,900,true,juce::SoftwareImageType{});juce::Graphics g(image);roll.paintEntireComponent(g,true);};refresh();
    const auto mouse=[&](juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    const auto original=model.contentFingerprint();const double seam=1.6;
    const auto left=roll.diagnosticPitchLineAt("part0",seam),right=roll.diagnosticPitchLineAt("part1",seam);
    check("virtual_sources_show_one_value_at_the_shared_joint",left&&right&&std::abs(*left-*right)<1.e-4);
    const juce::Point<float> from(float(roll.diagnosticEdgeX(seam)),roll.diagnosticYForMidi(61));const auto to=from.translated(0,-22);
    roll.mouseDown(mouse(from,from));check("actual_shared_joint_can_be_grabbed_with_point_tool",roll.diagnosticDraggingAnchor());
    roll.mouseDrag(mouse(to,from));
    const auto pendingLeft=roll.diagnosticPitchLineAt("part0",seam),pendingRight=roll.diagnosticPitchLineAt("part1",seam);
    check("drag_preview_changes_both_sides_without_an_owner_merge",pendingLeft&&pendingRight&&std::abs(*pendingLeft-*pendingRight)<1.e-4&&std::abs(*pendingRight-61)>.1&&model.snapshot().tracks[0].clips.size()==3);
    roll.mouseUp(mouse(to,from));refresh();
    const auto committedLeft=roll.diagnosticPitchLineAt("part0",seam),committedRight=roll.diagnosticPitchLineAt("part1",seam);
    check("shared_node_drag_really_commits_and_both_sides_stay_equal",model.contentFingerprint()!=original&&committedLeft&&committedRight&&std::abs(*committedLeft-*committedRight)<1.e-4&&std::abs(*committedRight-61)>.1);
    check("node_edit_does_not_create_a_parallel_cross_source_owner_or_edge",model.snapshot().tracks[0].clips.size()==3&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeConnections.empty());
    ProjectData projected;std::vector<NativeVirtualRenderGroup> groups;
    bool consistent=prepareNativeVirtualRender(model.snapshot(),projected,groups,error)&&groups.size()==1&&groups[0].pitch.active&&committedRight;
    if(consistent)consistent=std::abs(evaluatePitchCurve(groups[0].pitch.points,seam)-*committedRight)<.01;
    check("committed_shared_node_is_also_the_render_target",consistent);
    const auto after=model.contentFingerprint();model.undo();check("one_undo_restores_the_shared_node",model.contentFingerprint()==original);model.redo();check("redo_restores_the_committed_shared_node",model.contentFingerprint()==after);
    const auto file=folder.getChildFile("edited-shared.hjpx");ProjectModel cold;
    check("shared_edit_and_mode_survive_cold_project_reload",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups.size()==1&&cold.snapshot().nativeVirtualGroups[0].sharedPitch);
    ProjectData coldProjection;std::vector<NativeVirtualRenderGroup> coldGroups;
    bool restored=prepareNativeVirtualRender(cold.snapshot(),coldProjection,coldGroups,error)&&coldGroups.size()==1&&coldGroups[0].pitch.active&&committedRight;
    if(restored)restored=std::abs(evaluatePitchCurve(coldGroups[0].pitch.points,seam)-*committedRight)<.01;
    check("cold_render_retains_exact_shared_joint_pitch",restored);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
