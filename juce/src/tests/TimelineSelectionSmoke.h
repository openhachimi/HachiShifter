#pragma once
namespace hachi
{
inline bool MainComponent::diagnosticTimelineSelection(const juce::File& folder)
{
    folder.createDirectory(); stopTimer(); setSize(1280,800);
    bool ok=true;
    const auto check=[&](const char* name,bool value) { ok=ok&&value; std::cout<<name<<'='<<value<<std::endl; };
    const auto near=[](double a,double b) { return std::abs(a-b)<1.0e-7; };
    ProjectData data;
    for(int row=0;row<3;++row)
    {
        TrackData track; track.id="track"+juce::String(row); track.name="Track "+juce::String(row+1);
        track.accompaniment=true; track.compose=false;
        if(row<2) for(int c=0;c<2;++c)
        {
            ClipData clip; clip.id=(row==0?"a":"b")+juce::String(c+1);
            clip.sourceFile=folder.getChildFile(clip.id+".wav");
            clip.startSeconds=row==0?1+c*2:1.5+c*3.5;clip.durationSeconds=1;clip.gain=.7f;
            NoteData note;note.id="note-"+clip.id;note.startSeconds=.2;note.durationSeconds=.6;note.utauFlags="Mb70";
            note.diffSingerPitchReference={{0,60},{.6,63}};clip.notes.push_back(note);
            track.clips.push_back(clip);
        }
        data.tracks.push_back(track);
    }
    project.replace(data);
    const auto refresh=[&]
    {
        project.dispatchPendingMessages();stopTimer();resized();
        timelineHorizontalZoom=140;timeline.setPixelsPerSecond(140);timeline.setRowHeight(96);trackList.setRowHeight(96);
        (void)timeline.createComponentSnapshot(timeline.getLocalBounds());
    };
    refresh();timelineViewport.setViewPosition(0,0);
    const auto at=[](double time,float y) { return juce::Point<float>(static_cast<float>(time*140),y); };
    const auto event=[&](juce::Point<float> p,juce::Point<float> down,int mods)
    { return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|mods),0,0,0,0,0,&timeline,&timeline,
        juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,p!=down); };
    const auto gesture=[&](juce::Point<float> start,juce::Point<float> end,int mods=0)
    {
        refresh();timeline.mouseDown(event(start,start,mods));timeline.mouseDrag(event(end,start,mods));
        (void)timeline.createComponentSnapshot(timeline.getLocalBounds());timeline.mouseUp(event(end,start,mods));refresh();
    };
    const auto selected=[&](std::initializer_list<const char*> ids)
    {
        if(timeline.selectedClipIds().size()!=ids.size())return false;
        return std::all_of(ids.begin(),ids.end(),[&](const auto* id){return timeline.isClipSelected(id);});
    };
    const auto clip=[&](const char* id)
    {for(const auto& t:project.snapshot().tracks)for(const auto& c:t.clips)if(c.id==id)return c;return ClipData{};};
    int seeks=0;double sought=-1;
    const auto oldSeek=timeline.onSeek;timeline.onSeek=[&](double time){++seeks;sought=time;};
    const auto revision=project.revisionNumber();
    const auto start=at(.5,28),end=at(4.25,210);
    timeline.mouseDown(event(start,start,0));timeline.mouseDrag(event(end,start,0));
    check("cross_track_marquee_selects_only_intersections",selected({"a1","a2","b1"})&&timeline.selectedTrackIds().size()==2);
    check("marquee_does_not_seek_or_change_project",seeks==0&&project.revisionNumber()==revision);
    if(auto out=folder.getChildFile("marquee.png").createOutputStream())
        check("marquee_preview_written",juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*out));
    timeline.mouseUp(event(end,start,0));refresh();
    check("selection_survives_mouse_release",selected({"a1","a2","b1"})&&seeks==0);
    const auto live=liveMcpStatus();
    check("mcp_reports_clip_and_track_multiselection",live["selected_clip_ids"].size()==3&&live["selected_track_ids"].size()==2);
    gesture(at(3.5,80),at(4.5,80));
    check("drag_selected_body_moves_group",near(clip("a1").startSeconds,2)&&near(clip("a2").startSeconds,4)
        &&near(clip("b1").startSeconds,2.5)&&near(clip("b2").startSeconds,5));
    check("group_move_keeps_content_and_tracks",project.snapshot().tracks[0].clips.size()==2
        &&near(clip("a1").durationSeconds,1)&&near(clip("a1").notes[0].startSeconds,.2)
        &&clip("a1").notes[0].utauFlags=="Mb70"&&near(clip("a1").notes[0].diffSingerPitchReference.back().timeSeconds,.6));
    check("group_move_one_undo_step",project.revisionNumber()==revision+1&&project.undo()&&near(clip("b1").startSeconds,1.5));refresh();
    check("group_move_redo",project.redo()&&near(clip("a2").startSeconds,4));(void)project.undo();refresh();
    gesture(at(3.5,80),at(0,80));
    check("left_limit_preserves_relative_spacing",near(clip("a1").startSeconds,0)&&near(clip("a2").startSeconds,2)
        &&near(clip("b1").startSeconds,.5));(void)project.undo();refresh();
    gesture(at(4,80)-juce::Point<float>(3,0),at(4.5,80)-juce::Point<float>(3,0));
    check("edge_drag_trims_only_pointed_clip",near(clip("a2").durationSeconds,1.5)&&near(clip("a1").durationSeconds,1)
        &&near(clip("b1").durationSeconds,1)&&near(clip("a2").audioLength(),1));(void)project.undo();refresh();
    gesture(at(4.5,112),at(.5,28));
    check("reverse_marquee_same_track_multiple_clips",selected({"a1","a2"})&&timeline.selectedTrackIds().size()==1);
    gesture(at(1.5,80),at(1.5,80),juce::ModifierKeys::ctrlModifier);
    check("ctrl_click_toggles_one_clip",selected({"a2"}));
    gesture(at(5.5,176),at(5.5,176),juce::ModifierKeys::shiftModifier);
    check("shift_click_adds_clip",selected({"a2","b2"}));
    gesture(at(.5,28),at(2.2,112),juce::ModifierKeys::shiftModifier);
    check("shift_marquee_adds_to_selection",selected({"a1","a2","b2"}));
    gesture(at(3.5,80),at(1.2,176),juce::ModifierKeys::altModifier);
    check("alt_marquee_starts_inside_clip_without_moving",selected({"a1","a2","b1"})&&near(clip("a2").startSeconds,3));
    const auto beforeCancel=timeline.selectedClipIds();
    timeline.mouseDown(event(start,start,0));timeline.mouseDrag(event(at(.7,40),start,0));
    (void)timeline.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));timeline.mouseUp(event(at(.7,40),start,0));
    check("escape_cancels_marquee_and_restores_selection",timeline.selectedClipIds()==beforeCancel);
    gesture(at(6.5,80),at(6.5,80));
    check("empty_click_clears_and_seeks_once",selected({})&&seeks==1&&near(sought,6.5));
    gesture(at(6,28+192),at(6.5,112+192));
    check("empty_track_can_be_selected",selected({})&&timeline.selectedTrackIds()==std::vector<juce::String>{"track2"});
    (void)timeline.keyPressed(juce::KeyPress('A',juce::ModifierKeys::ctrlModifier,0));
    check("timeline_select_all_selects_clips",selected({"a1","a2","b1","b2"}));
    const auto oldMenu=timeline.onClipMenu;bool menuCalled=false;
    timeline.onClipMenu=[&](const auto&,double,juce::Point<int>){menuCalled=true;};
    const auto p=at(1.5,80);
    timeline.mouseDown(event(p,p,juce::ModifierKeys::rightButtonModifier));timeline.mouseUp(event(p,p,0));
    check("right_click_preserves_multiselection",menuCalled&&selected({"a1","a2","b1","b2"}));timeline.onClipMenu=oldMenu;
    timelineViewport.setViewPosition(180,0);
    gesture(at(2.8,28),at(4.25,112));
    check("selection_uses_timeline_coordinates_after_scroll",selected({"a2"}));
    timelineViewport.setViewPosition(0,0);
    timeline.setSelectedClips({"a2","b1"});
    pianoRoll.selectAllNotes();
    const auto oldConfirm=preferences->getBoolValue("operation.confirmDestructive",true);
    preferences->setValue("operation.confirmDestructive",false);
    const auto deleteRevision=project.revisionNumber();
    (void)timeline.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));refresh();
    check("delete_removes_selected_clips_only",project.snapshot().tracks[0].clips.size()==1
        &&project.snapshot().tracks[1].clips.size()==1&&clip("a1").notes.size()==1&&clip("b2").notes.size()==1);
    check("deleted_ids_removed_from_selection",timeline.selectedClipIds().empty());
    check("batch_delete_one_undo_step",project.revisionNumber()==deleteRevision+1&&project.undo()
        &&project.snapshot().tracks[0].clips.size()==2&&project.snapshot().tracks[1].clips.size()==2);refresh();
    check("batch_delete_redo",project.redo()&&project.snapshot().tracks[0].clips.size()==1);(void)project.undo();refresh();
    preferences->setValue("operation.confirmDestructive",oldConfirm);
    timeline.setSelectedClips({});
    const auto emptyDeleteRevision=project.revisionNumber();
    (void)timeline.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
    check("empty_timeline_delete_ignores_lower_note_selection",project.revisionNumber()==emptyDeleteRevision);
    project.moveClips({"missing"},1);project.moveClips({"a1"},0);project.removeClips({"missing"});
    check("invalid_or_noop_group_edits_do_not_add_undo",project.revisionNumber()==emptyDeleteRevision);
    timeline.onSeek=oldSeek;
    gesture(start,end);
    if(auto out=folder.getChildFile("selected.png").createOutputStream())
        check("selection_preview_written",juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*out));
    return ok;
}
}
