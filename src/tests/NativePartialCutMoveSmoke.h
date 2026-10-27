#pragma once
#include "../backend/MelodyneImporter.h"
#include "../NativeCoupledSelectionMove.h"
#include "../SampleSettings.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
// Imported partial cuts must behave like Melodyne: moving one side left/right
// or transposing it keeps the seam joined instead of tearing the recording.
inline bool runNativePartialCutMoveSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto source=output.getChildFile("phrase.wav");
    {
        juce::AudioBuffer<float> pcm(1,96000);
        for(int s=0;s<pcm.getNumSamples();++s)pcm.setSample(0,s,.1f*std::sin(juce::MathConstants<double>::twoPi*196*s/48000.));
        auto stream=source.createOutputStream();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));
        if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;
    }
    // One continuous recording split by a partial cut, exactly as an import
    // leaves it: shared source-usage identity plus reciprocal glide flags.
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;
    track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"right-owner":"left-owner";clip.sourceFile=source;
        clip.startSeconds=i?1.4:1.;clip.durationSeconds=clip.sourceDurationSeconds=i?.6:.4;
        clip.sourceOffsetSeconds=i?.4:0.;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        clip.materialSourceUsageGroupId="melodyne-use-1";
        clip.glideConnectedToNext=i==0;clip.glideConnectedFromPrevious=i==1;
        NoteData note;note.id=i?"right":"left";note.label="-";note.durationSeconds=clip.durationSeconds;
        note.midiNote=note.sourceMidiCenter=i?67:62;note.sourcePitchMeasured=true;
        note.connectedToNext=i==0;note.connectedToPrevious=i==1;
        for(int f=0;f<=int(std::lround(note.durationSeconds/.005));++f)note.contour.push_back({f*.005,0,0,true});
        clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};
    const auto seam=[](const ProjectData& data,double& targetGap,double& sourceGap)
    {
        const ClipData *left=nullptr,*right=nullptr;
        for(const auto& clip:data.tracks[0].clips){if(clip.id=="left-owner")left=&clip;if(clip.id=="right-owner")right=&clip;}
        if(!left||!right)return false;
        const auto leftClock=nativeClipClock(*left),rightClock=nativeClipClock(*right);
        const auto& a=left->notes.front();const auto& b=right->notes.front();
        targetGap=(right->startSeconds+b.startSeconds)-(left->startSeconds+a.startSeconds+a.durationSeconds);
        sourceGap=(right->sourceOffsetSeconds+nativeSourceTimeAt(rightClock,b.startSeconds))
            -(left->sourceOffsetSeconds+nativeSourceTimeAt(leftClock,a.startSeconds+a.durationSeconds));
        return true;
    };
    double targetGap=1,sourceGap=1;
    check("imported_partial_cut_starts_joined",seam(seed,targetGap,sourceGap)&&std::abs(targetGap)<1.e-9&&std::abs(sourceGap)<1.e-9);
    const auto midiOf=[](const ProjectData& data,const juce::String& id)
    {
        for(const auto& clip:data.tracks[0].clips)for(const auto& note:clip.notes)if(note.id==id)return note.midiNote;
        return -1.0f;
    };
    struct Case{const char* name;juce::String id;double delta;float semitones;};
    const std::vector<Case> cases{
        {"drag_right_half_later_keeps_seam","right",.25,0},
        {"drag_right_half_earlier_keeps_seam","right",-.18,0},
        {"drag_left_half_later_keeps_seam","left",.2,0},
        {"drag_left_half_earlier_keeps_seam","left",-.3,0},
        {"transpose_one_half_keeps_seam","right",0,5},
        {"drag_and_transpose_together_keeps_seam","left",.15,-3}};
    for(const auto& item:cases)
    {
        ProjectModel model;model.resetDocument(seed);
        const auto before=midiOf(seed,item.id);
        const auto moved=model.moveNativeNotes({item.id},item.delta,item.semitones,&error);
        double gap=1,sgap=1;const auto state=model.snapshot();
        const bool joined=seam(state,gap,sgap)&&std::abs(gap)<1.e-7&&std::abs(sgap)<1.e-7;
        const bool pitched=std::abs(item.semitones)<1.e-6||std::abs(midiOf(state,item.id)-before-item.semitones)<1.e-5;
        const bool owners=state.tracks[0].clips.size()==2;
        check(item.name,moved&&joined&&pitched&&owners);
    }
    ProjectModel model;model.resetDocument(seed);const auto original=model.contentFingerprint();
    model.moveNativeNotes({"right"},.25,0,&error);const auto afterMove=model.contentFingerprint();
    check("coupled_move_is_one_undoable_action",afterMove!=original&&[&]{model.undo();return model.contentFingerprint()==original;}());
    model.redo();
    check("redo_restores_the_coupled_result",model.contentFingerprint()==afterMove);
    // The neighbour must stretch, not lose or invent recording.
    const auto state=model.snapshot();double total=0;
    for(const auto& clip:state.tracks[0].clips)total+=clip.sourceDurationSeconds;
    check("coupled_stretch_keeps_total_source_support",std::abs(total-1.)<1.e-7);
    bool clocksValid=true;
    for(const auto& clip:state.tracks[0].clips)
    {
        const auto clock=nativeClipClock(clip);clocksValid&=clock.size()>=2;
        for(std::size_t i=1;i<clock.size();++i)
            clocksValid&=clock[i].targetSeconds>clock[i-1].targetSeconds&&clock[i].sourceSeconds>clock[i-1].sourceSeconds;
    }
    check("both_owners_keep_strictly_increasing_source_clocks",clocksValid);
    // Unrelated independent notes must NOT be dragged along.
    auto separate=seed;ClipData other;other.id="other-owner";other.sourceFile=output.getChildFile("other.wav");
    source.copyFileTo(other.sourceFile);other.startSeconds=3.;other.durationSeconds=other.sourceDurationSeconds=.4;
    other.sourceTimeMap={{0,0},{.4,.4}};NoteData alone;alone.id="alone";alone.durationSeconds=.4;alone.midiNote=60;
    for(int f=0;f<=80;++f)alone.contour.push_back({f*.005,0,0,true});other.notes={alone};separate.tracks[0].clips.push_back(other);
    ProjectModel independent;independent.resetDocument(separate);
    independent.moveNativeNotes({"right"},.25,0,&error);
    const auto third=independent.snapshot();bool untouched=false;
    for(const auto& clip:third.tracks[0].clips)if(clip.id=="other-owner")
        untouched=std::abs(clip.startSeconds-3.)<1.e-9&&std::abs(clip.durationSeconds-.4)<1.e-9;
    check("unrelated_independent_note_is_not_dragged_along",untouched);
    ProjectModel lone;lone.resetDocument(separate);const auto loneBefore=lone.contentFingerprint();
    lone.moveNativeNotes({"alone"},.2,0,&error);const auto loneAfter=lone.snapshot();
    double loneGap=1,loneSource=1;
    check("moving_an_unconnected_note_keeps_old_independent_behaviour",
        lone.contentFingerprint()!=loneBefore&&seam(loneAfter,loneGap,loneSource)&&std::abs(loneGap)<1.e-7);
    // A cross-source virtual connection binds identity and direction only: its
    // members stay independently movable, exactly as specified.
    auto crossSource=seed;crossSource.tracks[0].clips[1].sourceFile=output.getChildFile("other.wav");
    crossSource.tracks[0].clips[0].materialSourceUsageGroupId.clear();
    crossSource.tracks[0].clips[1].materialSourceUsageGroupId.clear();
    ProjectModel cross;cross.resetDocument(crossSource);
    const auto groupId=cross.createNativeVirtualConnection({"left","right"},false,error);
    const auto startOf=[](const ProjectData& data,const juce::String& clipId)
    {
        for(const auto& c:data.tracks[0].clips)if(c.id==clipId)return c.startSeconds;
        return std::numeric_limits<double>::quiet_NaN();
    };
    const auto beforeLeft=startOf(cross.snapshot(),"left-owner");
    const auto crossMoved=cross.moveNativeNotes({"right"},.3,0,&error);
    const auto crossState=cross.snapshot();
    const auto afterLeft=startOf(crossState,"left-owner"),rightStart=startOf(crossState,"right-owner");
    check("cross_source_group_member_moves_without_dragging_its_partner",
        groupId.isNotEmpty()&&crossMoved&&std::abs(afterLeft-beforeLeft)<1.e-9&&std::abs(rightStart-1.7)<1.e-6);
    check("cross_source_group_keeps_exact_members_and_order_after_moving",
        crossState.nativeVirtualGroups.size()==1&&crossState.nativeVirtualGroups[0].id==groupId
        &&crossState.nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"left","right"});
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
