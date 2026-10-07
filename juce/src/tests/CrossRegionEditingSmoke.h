#pragma once
namespace hachi
{
inline bool MainComponent::diagnosticCrossRegionEditing(const juce::File& folder)
{
    folder.createDirectory();stopTimer();bool ok=true;
    const auto check=[&](const char* name,bool pass){ok&=pass;std::cout<<name<<'='<<pass<<std::endl;};
    const auto get=[](const ProjectData& d,const juce::String& id){for(const auto& t:d.tracks)for(const auto& c:t.clips)for(const auto& n:c.notes)if(n.id==id)return n;return NoteData{};};
    const auto make=[](){ProjectData d;TrackData t;t.id="lead";t.compose=true;t.pitchAlgorithm=PitchAlgorithm::utau;
        for(int i=0;i<3;++i){ClipData c;c.id="c"+juce::String(i);c.startSeconds=1+4*i;c.durationSeconds=c.sourceDurationSeconds=3;c.showNormalDisplay=i==1;
            NoteData n;n.id="n"+juce::String(i);n.label="a";n.startSeconds=i==1?.2:.6;n.durationSeconds=.6;n.midiNote=60+i*2;n.utauFlags="g-10B30";n.utauFlagCurveEnabled=true;n.utauFlagCurves={{"g",{{0,-10},{.5,20}}}};n.diffSingerPitchReference={{0,n.midiNote-1},{.5,n.midiNote+.5f}};n.diffSingerPitchOffset={{0,-2},{.5,-1}};n.pitchControlPoints={{0,n.midiNote},{.5,n.midiNote+1}};c.notes.push_back(n);t.clips.push_back(c);}
        d.tracks.push_back(t);return d;};
    const auto event=[](juce::Component& c,juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier),0,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    for(int mode=0;mode<4;++mode)
    {
        auto data=make();data.tracks[0].utauMode=mode==0?UtauMode::classic:mode==1?UtauMode::jie:UtauMode::mou;
        if(mode==3){data.tracks[0].voicebankDirectory=folder.getChildFile("ds-bank");data.tracks[0].voicebankDirectory.createDirectory();data.tracks[0].voicebankDirectory.getChildFile("dsconfig.yaml").replaceWithText("# identity fixture");}
        ProjectModel model;model.resetDocument(data);PianoRollComponent roll(model,strings);roll.setReadsVoicebankInBackground(false);roll.setPixelsPerSecond(120);roll.setRowHeight(24);roll.setFocusedTrack("lead");roll.setFocusedClip("c0");roll.setTool(PianoRollComponent::Tool::note);roll.setSize(1800,3400);roll.diagnosticRefresh();roll.setSelectedNoteIds({"n0","n1"});
        const juce::Point<float> down(roll.diagnosticEdgeX(1.8),roll.diagnosticYForMidi(60)),to=down+juce::Point<float>(30,-48);
        roll.mouseDown(event(roll,down,down));check("mouse_down_retains_cross_region_selection",roll.selectedNoteIds().size()==2);
        roll.mouseDrag(event(roll,to,down));roll.mouseUp(event(roll,to,down));auto moved=model.snapshot();
        check("ui_moves_both_regions_by_same_time_and_pitch",std::abs(get(moved,"n0").startSeconds-.85)<1.e-5&&std::abs(get(moved,"n1").startSeconds-.45)<1.e-5&&get(moved,"n0").midiNote==62&&get(moved,"n1").midiNote==64);
        check("ui_keeps_selection_and_hidden_region_unchanged",roll.selectedNoteIds().size()==2&&get(moved,"n2").startSeconds==get(data,"n2").startSeconds&&get(moved,"n2").midiNote==64);
        check("one_undo_restores_all_regions",model.undo()&&get(model.snapshot(),"n0").midiNote==60&&get(model.snapshot(),"n1").midiNote==62&&!model.canUndo());
        check("redo_restores_group_move",model.redo()&&get(model.snapshot(),"n1").midiNote==64);
        check("flags_and_offsets_survive_group_move",get(moved,"n0").utauFlags=="g-10B30"&&get(moved,"n1").utauFlagCurves.size()==1&&get(moved,"n1").diffSingerPitchOffset[0].targetMidi==-2);
        check("absolute_pitch_and_ds_reference_transpose_together",get(moved,"n1").pitchControlPoints[0].targetMidi==64&&get(moved,"n1").diffSingerPitchReference[0].targetMidi==63);
    }
    auto data=make();ProjectModel model;model.resetDocument(data);check("left_boundary_uses_all_regions",model.moveUtauNotes({"n0","n1"},-3,0)&&std::abs(get(model.snapshot(),"n0").startSeconds-.4)<1.e-8&&get(model.snapshot(),"n1").startSeconds==0);
    data.tracks[0].clips[1].notes[0].midiNote=126;model.resetDocument(data);model.moveUtauNotes({"n0","n1"},0,8);
    check("shared_pitch_clamp_preserves_intervals",get(model.snapshot(),"n0").midiNote==61&&get(model.snapshot(),"n1").midiNote==127);
    data=make();auto obstacle=data.tracks[0].clips[1].notes[0];obstacle.id="obstacle";obstacle.startSeconds=1.5;data.tracks[0].clips[1].notes.push_back(obstacle);model.resetDocument(data);const auto hash=model.contentFingerprint();juce::String error;
    check("collision_rejects_entire_group_with_explanation",!model.moveUtauNotes({"n0","n1"},1,2,&error)&&error.isNotEmpty()&&model.contentFingerprint()==hash&&!model.canUndo());
    data=make();project.resetDocument(data);project.dispatchPendingMessages();stopTimer();focusClip("c0");pianoRoll.diagnosticRefresh();pianoRoll.setSelectedNoteIds({"n0","n1"});refreshSelectionSummary();
    check("selection_summary_reports_notes_and_regions",selectionSummaryLabel.getText().contains("2")&&selectionSummaryLabel.getText().contains(juce::String::fromUTF8("区域")));
    copySelectedNotes(false);check("copy_includes_all_regions_and_absolute_spacing",copiedNotes.size()==2&&std::abs(copiedNotes[1].startSeconds-3.6)<1.e-8&&copiedNotes[1].utauFlags=="g-10B30");
    project.setNotesUtauFlags(pianoRoll.selectedNoteIds(),"B55");check("parameter_edit_includes_all_selected_regions",get(project.snapshot(),"n0").utauFlags=="B55"&&get(project.snapshot(),"n1").utauFlags=="B55"&&get(project.snapshot(),"n2").utauFlags=="g-10B30");project.undo();
    project.transposeNotes(pianoRoll.selectedNoteIds(),3);check("transpose_includes_all_selected_regions",get(project.snapshot(),"n0").midiNote==63&&get(project.snapshot(),"n1").midiNote==65&&get(project.snapshot(),"n2").midiNote==64);project.undo();
    project.removeNotes(pianoRoll.selectedNoteIds());check("delete_includes_only_selected_regions",project.snapshot().tracks[0].clips[0].notes.empty()&&project.snapshot().tracks[0].clips[1].notes.empty()&&project.snapshot().tracks[0].clips[2].notes.size()==1);project.undo();
    project.setClipNormalDisplay("c1",false);project.dispatchPendingMessages();stopTimer();pianoRoll.diagnosticRefresh();check("closing_normal_display_drops_hidden_selection",pianoRoll.selectedNoteIds().size()==1&&pianoRoll.selectedNoteIds()[0]=="n0");
    return ok;
}
}