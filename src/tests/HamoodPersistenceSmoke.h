#pragma once
namespace hachi
{
bool MainComponent::diagnosticHamoodPersistence(const juce::File& folder)
{
    folder.createDirectory();stopTimer();bool ok=true;using namespace hamoodstate;
    const auto check=[&](const char* name,bool pass){ok&=pass;std::cout<<name<<'='<<pass<<std::endl;};
    ProjectData data;TrackData t;t.id="lead";t.name="Lead";t.compose=true;t.pitchAlgorithm=PitchAlgorithm::utau;ClipData clip;clip.id="clip";clip.durationSeconds=4;
    for(int i=0;i<2;++i){NoteData n;n.id="n"+juce::String(i);n.label="a";n.startSeconds=.25+2*i;n.durationSeconds=.5;n.midiNote=60+2*i;clip.notes.push_back(n);}t.clips.push_back(clip);data.tracks.push_back(t);project.resetDocument(data);
    juce::String error;int writes=0;const auto persist=[&](const juce::String& json){++writes;return project.setHamoodState(json,error);};
    {
        HamoodPanel panel(project.snapshot(),"lead",{},[](const auto&,const auto&){return true;},persist);
        check("opening_hamood_does_not_dirty_project",writes==0&&project.snapshot().hamoodState.isEmpty());
        panel.mode.setSelectedId(3,juce::dontSendNotification);panel.startBar.setText("1",false);panel.barCount.setText("1",false);panel.tonic.setSelectedId(4,juce::dontSendNotification);panel.addSection.onClick();
        check("manual_key_is_stored_without_generating_harmony",read(project.snapshot())["settings"]["manual_sections"].size()==1&&project.snapshot().tracks.size()==1);
    }
    HamoodPanel reopened(project.snapshot(),"lead",{},[](const auto&,const auto&){return true;},persist);
    check("closing_and_reopening_retains_manual_keys",reopened.mode.getSelectedId()==3&&reopened.manualSections.size()==1&&reopened.manualSections[0].tonic==3);
    reopened.startBar.setText("2",false);reopened.barCount.setText("1",false);reopened.tonic.setSelectedId(1,juce::dontSendNotification);reopened.addSection.onClick();
    reopened.sectionList.selectRow(0);reopened.sectionConfirmed.setToggleState(false,juce::dontSendNotification);reopened.sectionConfirmed.onClick();
    check("key_confirmation_is_persisted",!(bool)read(project.snapshot())["settings"]["manual_sections"][0]["confirmed"]);
    const auto savedKeys=encode(read(project.snapshot())["settings"]["manual_sections"]);
    reopened.mode.setSelectedId(1,juce::dontSendNotification);reopened.refresh();
    check("automatic_prediction_preserves_manual_key_corrections",encode(read(project.snapshot())["settings"]["manual_sections"])==savedKeys&&read(project.snapshot())["key_predictions"].size()>0);
    auto state=read(project.snapshot());HamoodTimelinePanel timeline(project.snapshot(),state,{},persist);
    timeline.start.setText("0",false);timeline.end.setText("2",false);timeline.name.setText("Cm",false);timeline.writeRow(false);
    check("manual_chord_and_confirmation_persist",read(project.snapshot())["chords"].size()==1&&read(project.snapshot())["chords"][0]["label"].toString()=="Cm"&&(bool)read(project.snapshot())["chords"][0]["confirmed"]);
    timeline.kind.setSelectedId(2,juce::dontSendNotification);timeline.start.setText("0",false);timeline.end.setText("4",false);timeline.name.setText(juce::String::fromUTF8("副歌"),false);timeline.writeRow(false);
    check("section_name_range_confirmation_persist",read(project.snapshot())["sections"].size()==1&&read(project.snapshot())["sections"][0]["label"].toString()==juce::String::fromUTF8("副歌"));
    const auto beforeEdit=project.snapshot().hamoodState;timeline.list.selectRow(0);timeline.name.setText(juce::String::fromUTF8("副歌一"),false);timeline.writeRow(true);
    check("metadata_edits_are_undoable_without_shared_objects",project.snapshot().hamoodState!=beforeEdit&&project.undo()&&project.snapshot().hamoodState==beforeEdit);project.redo();
    state=read(project.snapshot());hamood::Options audio;audio.chords={{0,2,.99,"G",{7,11,2}},{2,4,.7,"F",{5,9,0}}};importChords(project.snapshot(),state,audio.chords,"backing");
    check("reanalysis_keeps_confirmed_chords_and_adds_other_ranges",state["chords"].size()==2&&state["chords"][0]["label"].toString()=="Cm"&&state["chords"][1]["label"].toString()=="F");
    check("saved_context_valid",project.setHamoodState(encode(state),error));
    hamood::Options options;options.trackId="lead";options.wholeTrack=true;options.keyMode="manual";options.tonic=0;attach(project.snapshot(),state,options);const auto plan=hamood::analyse(project.snapshot(),options);
    check("harmony_preview_uses_saved_chords",plan.error.isEmpty()&&plan.audioCoveredNotes==2&&plan.audioAdjustedNotes>0);
    const auto path=folder.getChildFile("hamood-context.hjpx");check("save_project_context",project.save(path,error));ProjectModel loaded;check("reopen_preserves_complete_context",loaded.load(path,error)&&loaded.snapshot().hamoodState==project.snapshot().hamoodState);
    const auto recovery=folder.getChildFile("hamood-recovery.hjpx");check("auto_recovery_also_keeps_context",ProjectModel::saveRecoverySnapshot(project.snapshot(),recovery,path,error)&&loaded.load(recovery,error)&&loaded.snapshot().hamoodState==project.snapshot().hamoodState);
    auto faster=loaded.snapshot();faster.bpm=240;const auto first=read(faster)["chords"][0];check("chord_positions_follow_musical_time_after_tempo_change",std::abs(faster.secondsForQuarterPosition((double)first["end_quarter"])-1)<1.e-9);
    check("context_reuses_saved_settings_for_mcp",defaults(project.snapshot(),object())["key_mode"].toString()=="auto");auto requested=object();put(requested,"key_mode","manual");check("explicit_mcp_options_override_defaults",defaults(project.snapshot(),requested)["key_mode"].toString()=="manual");
    const auto unchanged=project.contentFingerprint();auto malformed=read(project.snapshot());auto badRow=(*malformed["chords"].getArray())[0];put(badRow,"end_quarter",-2);
    check("invalid_context_rejected_atomically",!project.setHamoodState(encode(malformed),error)&&error.isNotEmpty()&&project.contentFingerprint()==unchanged);
    auto configured=project.snapshot();auto configuredState=read(configured);auto configuredSettings=configuredState["settings"];put(configuredSettings,"minimum_chord_score",.75);put(configuredState,"settings",configuredSettings);configured.hamoodState=encode(configuredState);
    HamoodPanel configuredPanel(configured,"lead",{},[](const auto&,const auto&){return true;});check("gui_inherits_mcp_chord_threshold",std::abs(configuredPanel.readOptions().minimumChordScore-.75)<1.e-9);
    check("common_chord_notation",chordPitches("Bbmaj7")==std::vector<int>({10,2,5,9})&&chordPitches("Dm7b5")==std::vector<int>({2,5,8,0})&&chordPitches("C:min")==std::vector<int>({0,3,7})&&chordPitches("G7/B")==std::vector<int>({7,11,2,5})&&chordPitches("wrong").empty());
    HamoodPanel finalPanel(project.snapshot(),"lead",{},[](const auto&,const auto&){return true;});finalPanel.mode.setSelectedId(3,juce::dontSendNotification);finalPanel.refresh(false);
    HamoodTimelinePanel finalTimeline(project.snapshot(),read(project.snapshot()),{},{});
    juce::PNGImageFormat png;
    if(auto out=folder.getChildFile("hamood-persistent-keys.png").createOutputStream())check("key_editor_preview",png.writeImageToStream(finalPanel.createComponentSnapshot(finalPanel.getLocalBounds()),*out));
    if(auto out=folder.getChildFile("hamood-context.png").createOutputStream())check("context_editor_preview",png.writeImageToStream(finalTimeline.createComponentSnapshot(finalTimeline.getLocalBounds()),*out));
    return ok;
}
}