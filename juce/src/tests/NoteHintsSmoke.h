#pragma once
#include "../Theme.h"
namespace hachi
{
inline bool MainComponent::diagnosticNoteHints(const juce::File& folder)
{
    folder.createDirectory();stopTimer();setSize(1280,800);bool ok=true;
    const auto check=[&](const char* name,bool passed){ok=ok&&passed;std::cout<<name<<'='<<passed<<std::endl;};
    ProjectData data;
    const auto makeClip=[](const juce::String& id,float pitch,double offset)
    {ClipData c;c.id=id;c.startSeconds=1;c.durationSeconds=c.sourceDurationSeconds=4;
        NoteData n;n.id=id+"-n";n.label="a";n.startSeconds=offset;n.durationSeconds=1;n.midiNote=pitch;c.notes={n};return c;};
    for(int i=0;i<4;++i){TrackData t;t.id="track"+juce::String(i);t.name=t.id;t.pitchAlgorithm=PitchAlgorithm::utau;data.tracks.push_back(t);}
    auto guide=makeClip("guide",60,.5);auto second=guide.notes[0];second.id="guide-next";second.startSeconds=2;second.midiNote=62;guide.notes.push_back(second);
    data.tracks[0].clips={guide};data.tracks[1].clips={makeClip("edit",67,.25),makeClip("sibling",64,.5)};
    auto empty=makeClip("empty",60,0);empty.notes.clear();empty.startSeconds=7;data.tracks[1].clips.push_back(empty);
    data.tracks[2].pitchAlgorithm=PitchAlgorithm::world;data.tracks[2].clips={makeClip("other",55,.75)};
    data.tracks[3].accompaniment=true;data.tracks[3].compose=false;data.tracks[3].clips={makeClip("backing",70,.5)};
    project.replace(data);project.dispatchPendingMessages();stopTimer();focusClip("edit");
    const auto menuState=[&](const juce::String& id,bool showing)
    {auto menu=clipContextMenu(id,2);int result=-1;for(juce::PopupMenu::MenuItemIterator it(menu);it.next();)
        if(it.getItem().itemID==3){const auto& item=it.getItem();if(item.text!=strings.text(showing?"clip.stopNoteHints":"clip.startNoteHints"))return -2;result=item.isEnabled?1:0;}return result;};
    check("start_hint_menu_on_note_region",menuState("guide",false)==1);
    check("backing_and_empty_regions_disabled",menuState("backing",false)==0&&menuState("empty",false)==0);
    const auto hash=AudioEngine::diagnosticUtauRenderKey(project.snapshot(),"guide");const auto revision=project.revisionNumber();
    clipContextMenuItemChosen(3,"guide",2);project.dispatchPendingMessages();stopTimer();
    check("menu_enables_only_target_region",project.snapshot().tracks[0].clips[0].showNoteHints
        &&!project.snapshot().tracks[1].clips[0].showNoteHints&&project.revisionNumber()==revision+1);
    check("stop_hint_menu_after_enable",menuState("guide",true)==1);
    check("hint_toggle_does_not_change_render_key",!hash.empty()&&hash==AudioEngine::diagnosticUtauRenderKey(project.snapshot(),"guide"));
    check("hint_toggle_single_undo",project.undo()&&!project.snapshot().tracks[0].clips[0].showNoteHints);
    check("hint_toggle_redo",project.redo()&&project.snapshot().tracks[0].clips[0].showNoteHints);
    const auto guarded=project.revisionNumber();project.setClipNoteHints("backing",true);project.setClipNoteHints("empty",true);
    project.setClipNoteHints("missing",true);project.setClipNoteHints("guide",true);
    check("invalid_and_unchanged_toggles_are_noops",project.revisionNumber()==guarded);
    clipContextMenuItemChosen(3,"guide",2);check("menu_disables_hint",!project.snapshot().tracks[0].clips[0].showNoteHints);
    project.setClipNoteHints("guide",true);project.setClipNoteHints("sibling",true);project.setClipNoteHints("other",true);
    project.dispatchPendingMessages();stopTimer();const auto enabled=project.snapshot();
    // Geometry and pixels from a real roll; ghosts never join its hit list.
    ProjectModel model;model.replace(data);PianoRollComponent roll(model,strings);roll.setPixelsPerSecond(120);roll.setRowHeight(22);
    roll.setFocusedTrack("track1");roll.setFocusedClip("edit");roll.setShowPitchLine(false);roll.setShowWaveforms(false);roll.setShowUtauWaveforms(false);
    roll.setTool(PianoRollComponent::Tool::points);
    const juce::Rectangle<int> area(58,static_cast<int>(roll.diagnosticYForMidi(72))-11,720,440);
    const auto capture=[&]{return roll.createComponentSnapshot(area);};
    const auto baseline=capture();model.replace(enabled);model.dispatchPendingMessages();roll.diagnosticRefresh();const auto shown=capture();
    const auto at=[&](double seconds,float pitch){return juce::Point<int>(static_cast<int>(roll.diagnosticEdgeX(seconds))-area.getX(),static_cast<int>(roll.diagnosticYForMidi(pitch))-area.getY());};
    const auto changed=[&](const juce::Image& a,const juce::Image& b,juce::Point<int> p){return a.getPixelAt(p.x,p.y)!=b.getPixelAt(p.x,p.y);};
    check("other_track_notes_appear_as_hints",changed(shown,baseline,at(1.73,60))&&changed(shown,baseline,at(3.23,62)));
    check("same_track_other_region_hint",changed(shown,baseline,at(1.73,64)));
    check("multiple_tracks_can_show_hints",changed(shown,baseline,at(2.13,55)));
    const auto colourClose=[](juce::Colour a,juce::Colour b)
    {return std::abs(a.getRed()-b.getRed())<=3&&std::abs(a.getGreen()-b.getGreen())<=3
        &&std::abs(a.getBlue()-b.getBlue())<=3&&std::abs(a.getAlpha()-b.getAlpha())<=3;};
    for(const auto& item:std::vector<std::tuple<int,double,float>>{{0,1.73,60},{1,1.73,64},{2,2.13,55}})
    {const auto [index,time,pitch]=item;const auto p=at(time,pitch);
        check("hint_uses_track_colour_at_twenty_percent_opacity",colourClose(shown.getPixelAt(p.x,p.y),
            baseline.getPixelAt(p.x,p.y).overlaidWith(Palette::trackColour(static_cast<std::size_t>(index)).withAlpha(.20f))));}
    check("hints_do_not_add_editable_hits",roll.diagnosticHitCount()==1);
    roll.selectAllNotes();check("select_all_ignores_hints",roll.selectedNoteIds()==std::vector<juce::String>{"edit-n"});roll.clearNoteSelection();
    const auto event=[&](juce::Point<float> p){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    const juce::Point<float> ghost(roll.diagnosticEdgeX(1.73),roll.diagnosticYForMidi(60));const auto beforeClick=model.revisionNumber();
    roll.mouseDown(event(ghost));roll.mouseUp(event(ghost));
    check("click_hint_does_not_select_or_edit_its_note",roll.selectedNoteIds().empty()&&model.revisionNumber()==beforeClick);
    roll.selectAllNotes();check("delete_current_note_only",roll.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey))
        &&model.snapshot().tracks[0].clips[0].notes.size()==2&&model.snapshot().tracks[1].clips[1].notes.size()==1);
    (void)model.undo();model.dispatchPendingMessages();roll.diagnosticRefresh();
    model.setClipNoteHints("edit",true);model.dispatchPendingMessages();roll.diagnosticRefresh();const auto own=capture();
    bool same=true;for(int y=0;y<shown.getHeight();++y)for(int x=0;x<shown.getWidth();++x)if(shown.getPixelAt(x,y)!=own.getPixelAt(x,y))same=false;
    check("active_region_does_not_draw_duplicate_hint",same);
    model.setClipNoteHints("guide",false);model.dispatchPendingMessages();roll.diagnosticRefresh();const auto hidden=capture();
    check("disabling_one_region_removes_only_its_hints",!changed(hidden,baseline,at(1.73,60))&&changed(hidden,baseline,at(1.73,64)));
    model.setClipNoteHints("guide",true);model.moveClip("guide",2);model.dispatchPendingMessages();roll.diagnosticRefresh();const auto moved=capture();
    check("hint_follows_region_move",!changed(moved,baseline,at(1.73,60))&&changed(moved,baseline,at(2.73,60)));
    model.transposeNote("guide-n",1);model.dispatchPendingMessages();roll.diagnosticRefresh();const auto pitched=capture();
    check("hint_follows_note_pitch_edit",!changed(pitched,baseline,at(2.73,60))&&changed(pitched,baseline,at(2.73,61)));
    const auto enabledForView=model.snapshot();
    roll.setPixelsPerSecond(240);roll.setRowHeight(30);
    const auto zoomArea=juce::Rectangle<int>(58,static_cast<int>(roll.diagnosticYForMidi(70))-15,1300,500);
    const auto zoomed=roll.createComponentSnapshot(zoomArea);auto noHints=enabledForView;
    for(auto& t:noHints.tracks)for(auto& c:t.clips)c.showNoteHints=false;
    model.replace(noHints);model.dispatchPendingMessages();roll.diagnosticRefresh();const auto zoomBlank=roll.createComponentSnapshot(zoomArea);
    check("hint_follows_horizontal_and_vertical_zoom",changed(zoomed,zoomBlank,
        {static_cast<int>(roll.diagnosticEdgeX(2.73))-zoomArea.getX(),static_cast<int>(roll.diagnosticYForMidi(61))-zoomArea.getY()}));
    model.replace(enabled);model.dispatchPendingMessages();roll.diagnosticRefresh();roll.setPixelsPerSecond(120);roll.setRowHeight(22);
    roll.setFocusedClip("guide");roll.setSourceEditMode(true);const auto sourceHints=capture();
    model.replace(data);model.dispatchPendingMessages();roll.diagnosticRefresh();const auto sourceBlank=capture();
    same=true;for(int y=0;y<sourceHints.getHeight();++y)for(int x=0;x<sourceHints.getWidth();++x)
        if(sourceHints.getPixelAt(x,y)!=sourceBlank.getPixelAt(x,y))same=false;
    check("source_edit_has_no_project_time_hints",same);
    juce::String error;const auto saved=folder.getChildFile("hints.hjpx");ProjectModel loaded;
    check("hint_state_save_reopen",project.save(saved,error)&&loaded.load(saved,error)
        &&loaded.snapshot().tracks[0].clips[0].showNoteHints&&loaded.snapshot().tracks[1].clips[1].showNoteHints
        &&!loaded.snapshot().tracks[1].clips[0].showNoteHints);
    const auto duplicate=loaded.duplicateClip("guide",6);check("duplicate_inherits_hint_state",duplicate.isNotEmpty()&&loaded.snapshot().tracks[0].clips.back().showNoteHints);
    const auto right=loaded.splitClip("guide",2.7);check("split_both_regions_inherit_hint_state",right.isNotEmpty()
        &&loaded.snapshot().tracks[0].clips[0].showNoteHints&&loaded.snapshot().tracks[0].clips[1].showNoteHints);
    loaded.setClipNoteHints("guide",false);const auto merged=loaded.mergeClips({"guide",right});
    check("merge_preserves_enabled_hint",merged=="guide"&&loaded.snapshot().tracks[0].clips[0].showNoteHints);
    loaded.setClipNoteHints("guide",false);const auto afterSplit=loaded.splitClip("guide",2.7);
    check("disabled_merged_hint_does_not_reappear_on_split",afterSplit.isNotEmpty()
        &&!loaded.snapshot().tracks[0].clips[0].showNoteHints&&!loaded.snapshot().tracks[0].clips[1].showNoteHints);
    juce::MemoryBlock bytes;saved.loadFileAsData(bytes);auto legacy=juce::ValueTree::readFromData(bytes.getData(),bytes.getSize());
    for(auto t:legacy)for(auto c:t)if(c.hasType("Clip"))c.removeProperty("showNoteHints",nullptr);
    const auto oldFile=folder.getChildFile("legacy.hjpx");if(auto out=oldFile.createOutputStream()){out->setPosition(0);out->truncate();legacy.writeToStream(*out);}
    ProjectModel old;check("old_projects_default_hints_off",old.load(oldFile,error)&&!old.snapshot().tracks[0].clips[0].showNoteHints);
    const auto save=[&](const char* name,const juce::Image& image){auto out=folder.getChildFile(name).createOutputStream();check("hint_preview_written",out&&juce::PNGImageFormat().writeImageToStream(image,*out));};
    save("hints-on.png",shown);save("hints-off.png",baseline);save("hints-zoomed.png",zoomed);
    return ok;
}
}