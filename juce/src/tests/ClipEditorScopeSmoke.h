#pragma once
#include "../FlagCurveDrawing.h"
namespace hachi
{
inline bool MainComponent::diagnosticClipEditorScope(const juce::File& folder)
{
    folder.createDirectory();stopTimer();setSize(1280,800);bool ok=true;
    const auto check=[&](const char* name,bool pass){ok=ok&&pass;std::cout<<name<<'='<<pass<<std::endl;};
    const auto event=[](juce::Component& c,juce::Point<float> at,juce::Point<float> down,int mods=juce::ModifierKeys::leftButtonModifier)
    {return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(mods),0,0,0,0,0,
        &c,&c,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    ProjectData data;TrackData track;track.id="voice";track.compose=true;track.pitchAlgorithm=PitchAlgorithm::utau;
    ClipData clip;clip.id="whole";clip.startSeconds=1;clip.durationSeconds=4;clip.sourceDurationSeconds=4;
    for(int i=0;i<4;++i)
    {
        NoteData n;n.id="n"+juce::String(i);n.startSeconds=.25+i;n.durationSeconds=.5;n.midiNote=60.0f+static_cast<float>(i)*2.0f;n.label="a";
        n.utauFlagCurveEnabled=true;n.utauAutoPitchTransition=false;
        n.pitchControlPoints={{0,n.midiNote},{.5,n.midiNote+.5f}};
        n.diffSingerPitchReference=n.pitchControlPoints;clip.notes.push_back(n);
    }
    track.clips={clip};data.tracks={track};project.replace(data);project.dispatchPendingMessages();stopTimer();
    const auto right=project.splitClip("whole",3);project.dispatchPendingMessages();stopTimer();
    check("split_created_two_regions",right.isNotEmpty()&&project.snapshot().tracks[0].clips.size()==2);
    if(right.isEmpty())return false;
    const auto split=project.snapshot();
    timeline.setPixelsPerSecond(120);timeline.setRowHeight(96);trackList.setRowHeight(96);
    pianoRoll.setPixelsPerSecond(120);pianoRoll.setRowHeight(22);pianoRoll.setTool(PianoRollComponent::Tool::note);
    const auto click=[&](double seconds)
    {
        (void)timeline.createComponentSnapshot(timeline.getLocalBounds());
        const juce::Point<float> at(static_cast<float>(seconds*120),60);
        timeline.mouseDown(event(timeline,at,at));timeline.mouseUp(event(timeline,at,at));
    };
    const auto chosen=[&](std::vector<juce::String> expected)
    {auto actual=pianoRoll.selectedNoteIds();std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;};
    const auto revision=project.revisionNumber();
    click(1.7);
    check("left_click_focuses_only_left_notes",selectedClipId=="whole"&&pianoRoll.diagnosticHitCount()==2
        &&std::abs(pianoRoll.diagnosticHitX(0)-pianoRoll.diagnosticEdgeX(1.25))<.1f);
    pianoRoll.selectAllNotes();check("select_all_is_region_local",chosen({"n0","n1"}));
    click(3.7);
    check("right_click_focuses_only_right_notes",selectedClipId==right&&pianoRoll.diagnosticHitCount()==2
        &&std::abs(pianoRoll.diagnosticHitX(0)-pianoRoll.diagnosticEdgeX(3.25))<.1f);
    check("switching_regions_clears_hidden_selection",pianoRoll.selectedNoteIds().empty()&&selectedNoteId.isEmpty());
    check("switching_display_does_not_edit_project",project.revisionNumber()==revision);
    check("delete_after_switch_cannot_remove_hidden_notes",!pianoRoll.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey))
        &&project.snapshot().tracks[0].clips[0].notes.size()==2);
    pianoRoll.selectAllNotes();check("right_region_select_all",chosen({"n2","n3"}));
    check("delete_visible_notes",pianoRoll.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
    project.dispatchPendingMessages();stopTimer();
    check("delete_preserves_other_region",project.snapshot().tracks[0].clips[0].notes.size()==2
        &&project.snapshot().tracks[0].clips[1].notes.empty());
    check("undo_restores_region_notes",project.undo());project.dispatchPendingMessages();stopTimer();
    check("undo_keeps_current_region_scope",pianoRoll.diagnosticHitCount()==2&&selectedClipId==right);
    const auto merged=project.mergeClips({"whole",right});project.dispatchPendingMessages();stopTimer();focusClip(merged);
    check("merged_region_displays_all_owned_notes",merged.isNotEmpty()&&pianoRoll.diagnosticHitCount()==4);
    check("undo_merge",project.undo());project.dispatchPendingMessages();stopTimer();click(1.7);
    check("scope_restored_after_undo_merge",pianoRoll.diagnosticHitCount()==2&&selectedClipId=="whole");
    juce::String error;ProjectModel loaded;const auto saved=folder.getChildFile("split-regions.hjpx");
    check("split_project_save_reopen",project.save(saved,error)&&loaded.load(saved,error));
    project.replace(loaded.snapshot());project.dispatchPendingMessages();stopTimer();click(3.7);
    check("reopened_region_is_independent",pianoRoll.diagnosticHitCount()==2&&selectedClipId==right);
    // Real timeline clicks must retain native clips and cross-region selection,
    // while a note edit still goes to its owning region.
    auto native=split;native.tracks[0].pitchAlgorithm=PitchAlgorithm::nsfHifigan;
    native.tracks[0].utauMode=UtauMode::classic;
    auto outside=native.tracks[0];outside.id="outside";outside.clips.resize(1);outside.clips[0].id="outside-clip";
    for(auto& n:outside.clips[0].notes)n.id="outside-"+n.id;
    native.tracks.push_back(outside);project.replace(native);project.dispatchPendingMessages();stopTimer();
    click(1.7);check("native_timeline_click_keeps_all_regions",pianoRoll.diagnosticHitCount()==4);
    pianoRoll.selectAllNotes();check("native_select_all_spans_regions",chosen({"n0","n1","n2","n3"}));
    click(3.7);check("native_region_switch_retains_visible_selection",pianoRoll.diagnosticHitCount()==4&&chosen({"n0","n1","n2","n3"}));
    const auto nativeRevision=project.revisionNumber();focusClip("whole");
    check("native_visibility_is_not_a_project_edit",project.revisionNumber()==nativeRevision
        &&!project.snapshot().tracks[0].clips[1].showNormalDisplay);
    pianoRoll.clearNoteSelection();pianoRoll.setTool(PianoRollComponent::Tool::note);
    const auto down=pianoRoll.diagnosticHitBounds(2).getCentre(),to=down+juce::Point<float>(0,-22);
    pianoRoll.mouseDown(event(pianoRoll,down,down));pianoRoll.mouseDrag(event(pianoRoll,to,down));pianoRoll.mouseUp(event(pianoRoll,to,down));
    project.dispatchPendingMessages();stopTimer();
    const auto editedNative=project.snapshot();
    check("native_other_region_note_edit_keeps_owner",editedNative.tracks[0].clips[1].notes[0].id=="n2"
        &&editedNative.tracks[0].clips[1].notes[0].midiNote>64.5f
        &&editedNative.tracks[0].clips[0].notes[0].midiNote==60&&editedNative.tracks[1].clips[0].notes[0].midiNote==60);
    check("native_edit_retains_all_visible_regions",pianoRoll.diagnosticHitCount()==4);
    project.save(folder.getChildFile("native-regions.hjpx"),error);ProjectModel nativeReload;
    check("native_region_save_reopen",nativeReload.load(folder.getChildFile("native-regions.hjpx"),error));
    project.replace(nativeReload.snapshot());project.dispatchPendingMessages();stopTimer();focusClip("whole");
    check("native_reopened_regions_visible_without_flags",pianoRoll.diagnosticHitCount()==4);
    auto nativePreview=folder.getChildFile("native-all-regions.png").createOutputStream();
    check("native_shared_region_preview_written",nativePreview&&juce::PNGImageFormat().writeImageToStream(
        pianoRoll.createComponentSnapshot({0,static_cast<int>(pianoRoll.diagnosticYForMidi(72)),800,380}),*nativePreview));
    auto nativeMenu=clipContextMenu("whole",1.7);bool nativeNormalToggle=false;
    for(juce::PopupMenu::MenuItemIterator it(nativeMenu);it.next();)nativeNormalToggle|=it.getItem().itemID==4;
    check("native_default_visibility_needs_no_normal_display_toggle",!nativeNormalToggle);
    focusClip("outside-clip");check("native_track_switch_excludes_other_tracks",pianoRoll.diagnosticHitCount()==2);
    // Standalone rolls compare the actual painted output with the other
    // region removed, including curve, reference, amplitude and FLAG modes.
    const auto equalPixels=[](const juce::Image& a,const juce::Image& b)
    {if(a.getBounds()!=b.getBounds())return false;for(int y=0;y<a.getHeight();++y)for(int x=0;x<a.getWidth();++x)
        if(a.getPixelAt(x,y)!=b.getPixelAt(x,y))return false;return true;};
    for(int mode=0;mode<5;++mode)
    {
        auto scoped=split;auto& t=scoped.tracks[0];
        if(mode==0)t.pitchAlgorithm=PitchAlgorithm::world;
        else t.utauMode=mode==1?UtauMode::classic:mode==2?UtauMode::jie:UtauMode::mou;
        if(mode==4)
        {t.voicebankDirectory=folder.getChildFile("fixture-bank");t.voicebankDirectory.createDirectory();
            t.voicebankDirectory.getChildFile("dsconfig.yaml").replaceWithText("acoustic: fixture.onnx\n");}
        ProjectModel model;model.replace(scoped);PianoRollComponent roll(model,strings);
        roll.setPixelsPerSecond(120);roll.setRowHeight(22);roll.setFocusedTrack("voice");roll.setFocusedClip("whole");
        roll.setShowPitchLine(true);roll.setShowUtauWaveforms(true);roll.setReadsVoicebankInBackground(false);
        // Rendered note waveforms must not leak across the selected region.
        auto waves=std::make_shared<std::vector<UtauNoteWaveform>>();
        for(const auto& c:t.clips)for(const auto& n:c.notes)
        {UtauNoteWaveform w;w.noteId=n.id;w.audioHash=AudioEngine::utauNoteAudioHash(n);w.durationSeconds=.5;
            w.unshapedMaxima.assign(60,.5f);w.unshapedMinima.assign(60,-.5f);waves->push_back(w);}
        roll.setUtauNoteWaveforms(waves);
        check("engine_specific_region_hit_scope",roll.diagnosticHitCount()==(mode==0?4:2));
        const auto area=juce::Rectangle<int>(58,static_cast<int>(roll.diagnosticYForMidi(70)),670,300);
        const auto hiddenBounds=juce::Rectangle<int>(static_cast<int>(roll.diagnosticEdgeX(3)),area.getY(),260,300);
        for(const auto tool:{PianoRollComponent::Tool::note,PianoRollComponent::Tool::points,
                            PianoRollComponent::Tool::amplitude,PianoRollComponent::Tool::flagCurve})
        {
            roll.setTool(tool);const auto full=roll.createComponentSnapshot(hiddenBounds);
            auto isolated=scoped;isolated.tracks[0].clips.resize(1);model.replace(isolated);model.dispatchPendingMessages();roll.diagnosticRefresh();
            const auto single=roll.createComponentSnapshot(hiddenBounds);
            check(mode==0?"native_other_region_paints_notes_curves_and_waveforms":"hidden_region_does_not_paint_notes_curves_or_waveforms",
                  mode==0?!equalPixels(full,single):equalPixels(full,single));
            model.replace(scoped);model.dispatchPendingMessages();roll.diagnosticRefresh();
        }
        roll.setTool(PianoRollComponent::Tool::points);
        if(mode==4)
        {
            check("ds_reference_visible_only_for_current_region",!roll.diagnosticDiffSingerPitchReference("n0").isEmpty()
                &&roll.diagnosticDiffSingerPitchReference("n2").isEmpty());
            roll.setFocusedClip(right);
            check("ds_reference_switches_with_region",roll.diagnosticDiffSingerPitchReference("n0").isEmpty()
                &&!roll.diagnosticDiffSingerPitchReference("n2").isEmpty());
        }
        roll.setFocusedClip("whole");
        if(mode==1)
        {
            roll.setTool(PianoRollComponent::Tool::flagCurve);roll.setFlagLaneFlag("g");
            roll.setFlagEditMode(PianoRollComponent::FlagEditMode::continuous);
            const juce::Point<float> begin(roll.diagnosticEdgeX(1.5),roll.flagLaneY(10));
            const juce::Point<float> end(roll.diagnosticEdgeX(4.5),roll.flagLaneY(30));
            roll.mouseDown(event(roll,begin,begin));roll.mouseDrag(event(roll,end,begin));roll.mouseUp(event(roll,end,begin));
            const auto edited=model.snapshot();
            check("continuous_flag_edits_visible_region",!flagCurvePointsFor(edited.tracks[0].clips[0].notes[0],"g").empty());
            check("continuous_flag_cannot_edit_hidden_region",flagCurvePointsFor(edited.tracks[0].clips[1].notes[0],"g").empty()
                &&flagCurvePointsFor(edited.tracks[0].clips[1].notes[1],"g").empty());
        }
        roll.setTool(PianoRollComponent::Tool::points);
        for(const auto& id:std::vector<juce::String>{"whole",right})
        {
            roll.setFocusedClip(id);const auto image=roll.createComponentSnapshot(area);
            if(mode==1||mode==4)
            {auto out=folder.getChildFile(juce::String(mode==1?"UTAU-":"DS-")+(id=="whole"?"left":"right")+".png").createOutputStream();
                check("region_preview_written",out&&juce::PNGImageFormat().writeImageToStream(image,*out));}
        }
        roll.setSourceEditMode(true);roll.setFocusedClip("whole");check("source_edit_stays_region_local",roll.diagnosticHitCount()==2);
        if(mode==0){roll.setSourceEditMode(false);roll.selectAllNotes();roll.setSourceEditMode(true);
            check("native_source_edit_prunes_other_region_selection",roll.selectedNoteIds().size()==2);}
    }
    return ok;
}
}
