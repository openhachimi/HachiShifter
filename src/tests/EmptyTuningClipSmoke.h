#pragma once
namespace hachi
{
inline bool MainComponent::diagnosticEmptyTuningClip(const juce::File& folder)
{
    folder.createDirectory();stopTimer();setSize(1280,800);bool ok=true;
    const auto check=[&](const char* name,bool pass){ok=ok&&pass;std::cout<<name<<'='<<pass<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.0e-8;};
    ProjectData data;TrackData vocal;vocal.id="voice";vocal.name="Voice";vocal.compose=false;
    vocal.pitchAlgorithm=PitchAlgorithm::utau;vocal.utauMode=UtauMode::mou;vocal.utauGlobalFlags="HF2Mb50";
    ClipData occupied;occupied.id="existing";occupied.startSeconds=.5;occupied.durationSeconds=1.5;vocal.clips={occupied};
    TrackData backing;backing.id="backing";backing.name="Backing";backing.accompaniment=true;backing.compose=false;
    data.tracks={vocal,backing};project.replace(data);project.dispatchPendingMessages();stopTimer();resized();
    timeline.setPixelsPerSecond(140);timeline.setRowHeight(96);trackList.setRowHeight(96);
    (void)timeline.createComponentSnapshot(timeline.getLocalBounds());selectedTrackId="backing";
    const auto state=[&](const std::optional<TimelineComponent::Anchor>& anchor)
    {
        auto menu=trackAreaMenu(anchor);int result=-1;
        for(juce::PopupMenu::MenuItemIterator it(menu);it.next();)if(it.getItem().itemID==emptyTuningClipMenuItem)
            result=it.getItem().isEnabled?1:0;
        return result;
    };
    check("non_backing_menu_enabled",state(TimelineComponent::Anchor{"voice",3.25})==1);
    check("backing_menu_disabled",state(TimelineComponent::Anchor{"backing",3.25})==0);
    check("outside_lanes_has_no_entry",state({})==-1);
    check("occupied_time_is_disabled",state(TimelineComponent::Anchor{"voice",1})==0);
    check("unknown_track_disabled",state(TimelineComponent::Anchor{"missing",3})==0);
    std::optional<TimelineComponent::Anchor> clicked;
    const auto originalMenu=timeline.onEmptyAreaMenu;
    timeline.onEmptyAreaMenu=[&](auto){clicked=timeline.pointerAnchor();};
    const juce::Point<float> at(455,78);
    const auto event=juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,
        juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier),0,0,0,0,0,&timeline,&timeline,
        juce::Time::getCurrentTime(),at,juce::Time::getCurrentTime(),1,false);
    const auto position=audio.position();const auto revision=project.revisionNumber();
    timeline.mouseDown(event);timeline.onEmptyAreaMenu=originalMenu;
    check("right_click_captures_clicked_track_and_time",clicked&&clicked->trackId=="voice"&&near(clicked->seconds,3.25));
    check("opening_menu_does_not_edit_or_seek",project.revisionNumber()==revision&&near(audio.position(),position));
    timeline.mouseExit(event);check("leaving_lane_clears_live_hover",!timeline.pointerAnchor());
    trackAreaMenuItemChosen(emptyTuningClipMenuItem,clicked);project.dispatchPendingMessages();stopTimer();
    const auto after=project.snapshot();const auto id=selectedClipId;
    const auto added=std::find_if(after.tracks[0].clips.begin(),after.tracks[0].clips.end(),[&](const auto& c){return c.id==id;});
    check("created_on_captured_track_not_selected_backing",after.tracks[0].clips.size()==2&&after.tracks[1].clips.empty());
    check("default_ten_seconds_at_click",added!=after.tracks[0].clips.end()&&near(added->startSeconds,3.25)&&near(added->durationSeconds,10));
    check("clip_is_empty_and_ready_for_notes",added!=after.tracks[0].clips.end()&&added->notes.empty()&&added->sourceFile==juce::File{}&&after.tracks[0].compose);
    check("engine_mode_and_flags_preserved",after.tracks[0].pitchAlgorithm==vocal.pitchAlgorithm&&after.tracks[0].utauMode==vocal.utauMode&&after.tracks[0].utauGlobalFlags==vocal.utauGlobalFlags);
    check("new_clip_selected_for_editing",selectedTrackId=="voice"&&timeline.isClipSelected(id));
    check("creation_is_one_undo",project.revisionNumber()==revision+1&&project.undo()&&project.snapshot().tracks[0].clips.size()==1&&!project.snapshot().tracks[0].compose);
    check("redo_restores_clip_and_note_editing",project.redo()&&project.snapshot().tracks[0].clips.size()==2&&project.snapshot().tracks[0].compose);
    project.dispatchPendingMessages();stopTimer();
    const auto beforeRejected=project.revisionNumber();
    trackAreaMenuItemChosen(emptyTuningClipMenuItem,TimelineComponent::Anchor{"backing",5});
    trackAreaMenuItemChosen(emptyTuningClipMenuItem,TimelineComponent::Anchor{"missing",5});
    trackAreaMenuItemChosen(emptyTuningClipMenuItem,clicked);
    trackAreaMenuItemChosen(emptyTuningClipMenuItem,{});
    check("disabled_stale_and_duplicate_actions_do_not_edit",project.revisionNumber()==beforeRejected&&project.snapshot().tracks[1].clips.empty());
    check("invalid_times_rejected",project.addEmptyTuningClip("voice",-1).isEmpty()&&project.addEmptyTuningClip("voice",NAN).isEmpty()
        &&project.addEmptyTuningClip("voice",20,INFINITY).isEmpty()&&project.revisionNumber()==beforeRejected);
    const auto note=project.addNote(id,3.5,.5,60);
    check("new_clip_accepts_notes",note.isNotEmpty());
    juce::String error;ProjectModel reopened;const auto file=folder.getChildFile("empty-region.hjpx");
    check("save_and_reopen",project.save(file,error)&&reopened.load(file,error));
    const auto restored=reopened.snapshot();const auto& saved=restored.tracks[0].clips.back();
    check("saved_clip_retains_position_duration_and_notes",saved.id==id&&near(saved.startSeconds,3.25)&&near(saved.durationSeconds,10)
        &&saved.notes.size()==1&&near(saved.notes.front().startSeconds,.25)&&saved.notes.front().id==note);
    // Creation keeps the selected synthesis algorithm across all native/UTAU modes.
    for(const auto backend:{PitchAlgorithm::utau,PitchAlgorithm::world,PitchAlgorithm::llsm2,PitchAlgorithm::nsfHifigan})
    {
        ProjectModel model;auto source=data;source.tracks[0].pitchAlgorithm=backend;source.tracks[0].clips.clear();model.replace(source);
        const auto created=model.addEmptyTuningClip("voice",0);
        check("native_and_utau_modes_accept_empty_clip",created.isNotEmpty()&&model.snapshot().tracks[0].pitchAlgorithm==backend
            &&model.snapshot().tracks[0].clips.front().notes.empty()&&near(model.snapshot().tracks[0].clips.front().durationSeconds,10));
    }
    for(const auto mode:{UtauMode::classic,UtauMode::jie,UtauMode::mou})
    {
        ProjectModel model;auto source=data;source.tracks[0].utauMode=mode;source.tracks[0].clips.clear();model.replace(source);
        check("utau_submodes_preserved",model.addEmptyTuningClip("voice",0).isNotEmpty()&&model.snapshot().tracks[0].utauMode==mode);
    }
    project.dispatchPendingMessages();stopTimer();resized();
    if(auto out=folder.getChildFile("empty-region.png").createOutputStream())check("preview_written",
        juce::PNGImageFormat().writeImageToStream(timeline.createComponentSnapshot(timeline.getLocalBounds()),*out));
    return ok;
}
}