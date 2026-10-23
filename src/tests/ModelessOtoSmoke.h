#pragma once
#include "../VoicebankSettingsComponent.h"
#include "../OtoWaveformEditorComponent.h"
namespace hachi
{
inline bool MainComponent::diagnosticModelessOto(const juce::File& folder)
{
    folder.createDirectory();stopTimer();setSize(1280,850);bool ok=true;
    const auto check=[&](const char* name,bool pass){ok&=pass;std::cout<<name<<'='<<pass<<std::endl;};
    const auto waitRender=[&]{const auto end=juce::Time::getMillisecondCounterHiRes()+15000;
        while(audio.renderProgress()&&juce::Time::getMillisecondCounterHiRes()<end)juce::Thread::sleep(5);
        return audio.hasCurrentRenderedAudio()&&audio.activeRenderWarnings().isEmpty();};
    const auto bank=folder.getChildFile("bank");bank.createDirectory();
    {
        juce::AudioBuffer<float> samples(1,44100);
        for(int i=0;i<samples.getNumSamples();++i)samples.setSample(0,i,.2f*(float)std::sin(juce::MathConstants<double>::twoPi*220*i/44100));
        auto out=bank.getChildFile("a.wav").createOutputStream();juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(out.release(),44100,1,16,{},0));
        check("modeless_audio_fixture",writer&&writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples()));
    }
    bank.getChildFile("oto.ini").replaceWithText("a.wav=a,0,100,-800,50,20\n");
    ProjectData data;TrackData track;track.id="voice";track.name="Modeless OTO";track.compose=true;track.pitchAlgorithm=PitchAlgorithm::utau;track.utauMode=UtauMode::classic;track.voicebankDirectory=bank;
    ClipData clip;clip.id="clip";clip.durationSeconds=clip.sourceDurationSeconds=2;
    NoteData note;note.id="note";note.label="a";note.startSeconds=.2;note.durationSeconds=.6;note.midiNote=60;clip.notes={note};track.clips={clip};data.tracks={track};
    project.resetDocument(data);project.dispatchPendingMessages();stopTimer();focusClip("clip");pianoRoll.setSelectedNoteIds({"note"});selectedNoteId="note";
    pianoRoll.setReadsVoicebankInBackground(false);pianoRoll.setTool(PianoRollComponent::Tool::note);pianoRoll.diagnosticRefresh();
    audio.prepareToPlay(512,44100);audio.selectEveryUtauNote(data);syncAudio(data);
    showVoicebankSettings();auto* list=voicebankWindows.findVisible<VoicebankSettingsComponent>();
    check("bank_window_is_modeless",list&&!list->isCurrentlyModal());if(!list)return false;
    auto* panel=dynamic_cast<VoicebankSettingsComponent*>(list->getContentComponent());
    check("main_transport_and_roll_are_not_blocked",!isCurrentlyBlockedByAnotherModalComponent()&&!playButton.isCurrentlyBlockedByAnotherModalComponent()&&!pianoRoll.isCurrentlyBlockedByAnotherModalComponent());
    const auto listCount=voicebankWindows.size();showVoicebankSettings();check("reopen_reuses_bank_window",voicebankWindows.size()==listCount&&list==voicebankWindows.findVisible<VoicebankSettingsComponent>());
    panel->diagnosticOpenEditor();auto* wave=panel->diagnosticEditorWindow();auto* editor=wave?dynamic_cast<OtoWaveformEditorComponent*>(wave->getContentComponent()):nullptr;
    check("oto_window_is_modeless",wave&&!wave->isCurrentlyModal());if(!editor)return false;
    juce::Component::SafePointer<OtoWaveformEditorComponent> editorLife(editor);
    check("opening_oto_keeps_main_and_list_interactive",!pianoRoll.isCurrentlyBlockedByAnotherModalComponent()&&!panel->isCurrentlyBlockedByAnotherModalComponent());
    panel->diagnosticOpenEditor();check("reopen_reuses_unsaved_oto_window",wave==panel->diagnosticEditorWindow());
    check("project_renders_with_both_tools_open",waitRender());
    // A real piano-roll mouse gesture, while the windows are still visible.
    const auto mouse=[&](juce::Point<float> p,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,&pianoRoll,&pianoRoll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,p!=down);};
    const auto before=project.snapshot().tracks[0].clips[0].notes[0].midiNote;
    const auto point=pianoRoll.diagnosticHitBounds(0).getCentre();const auto moved=point.withY(point.y-48);
    pianoRoll.mouseDown(mouse(point,point));pianoRoll.mouseDrag(mouse(moved,point));pianoRoll.mouseUp(mouse(moved,point));
    project.dispatchPendingMessages();stopTimer();pianoRoll.diagnosticRefresh();
    check("pitch_drag_works_with_both_tools_open",project.snapshot().tracks[0].clips[0].notes[0].midiNote>before&&list->isVisible()&&wave->isVisible());
    check("pitch_edit_rerenders_with_tools_open",waitRender());
    juce::AudioDeviceManager offlineDevice;editor->setPlaybackHost({[&]{return &offlineDevice;},[]{return true;}});
    editor->diagnosticPressPlay();check("raw_preview_attaches",editor->diagnosticPreviewPlaying()&&editor->diagnosticPreviewAttached());
    audio.setPosition(.3);startPreparedPlayback();
    check("project_playback_stops_raw_preview",audio.isPlaying()&&!editor->diagnosticPreviewPlaying()&&!editor->diagnosticPreviewAttached());
    juce::AudioBuffer<float> block(2,512);juce::AudioSourceChannelInfo output(&block,0,512);audio.getNextAudioBlock(output);
    check("rendered_song_produces_audio_with_tools_open",block.getMagnitude(0,512)>.00001f);audio.stop();
    editor->diagnosticDragOffsetTo(10);editor->diagnosticSave();panel->diagnosticCollectClosedEditors();
    check("save_closes_only_oto_not_bank_or_main",editorLife==nullptr&&list->isVisible()&&!isCurrentlyBlockedByAnotherModalComponent());
    check("save_updates_bank_timing",panel->diagnosticSelectedEntry()&&std::abs(panel->diagnosticSelectedEntry()->offsetMs-10)<.001);
    const auto revision=project.revisionNumber();refreshAfterVoicebankChange();project.dispatchPendingMessages();stopTimer();
    check("saved_oto_refreshes_project_render_without_dirtying_project",waitRender()&&project.revisionNumber()==revision);
    // Direct region and per-note entrypoints must not reintroduce blocking.
    showRegionEditorForNote("note");auto* direct=voicebankWindows.findVisible<OtoWaveformEditorComponent>();
    check("direct_bank_oto_is_modeless",direct&&!direct->isCurrentlyModal()&&!pianoRoll.isCurrentlyBlockedByAnotherModalComponent());
    if(direct){direct->closeButtonPressed();voicebankWindows.removeClosed();}
    showNoteOtoEditorForNote("note");auto* single=noteOtoWindows.findVisible<OtoWaveformEditorComponent>();
    check("single_note_oto_is_modeless",single&&!single->isCurrentlyModal()&&!pianoRoll.isCurrentlyBlockedByAnotherModalComponent());
    juce::Component::SafePointer<juce::DialogWindow> singleLife(single);liveMcpDocumentChanged();
    check("document_switch_closes_single_note_tool_but_keeps_bank",singleLife==nullptr&&list->isVisible());
    panel->diagnosticOpenEditor();wave=panel->diagnosticEditorWindow();editor=wave?dynamic_cast<OtoWaveformEditorComponent*>(wave->getContentComponent()):nullptr;
    if(editor)
    {
        editorLife=editor;editor->setPlaybackHost({[&]{return &offlineDevice;},[]{return true;}});editor->diagnosticPressPlay();
        juce::Component::SafePointer<juce::DialogWindow> waveLife(wave);
        list->closeButtonPressed();voicebankWindows.removeClosed();
        check("closing_bank_releases_child_window_and_preview",editorLife==nullptr&&waveLife==nullptr);
    }
    showVoicebankSettings();list=voicebankWindows.findVisible<VoicebankSettingsComponent>();
    check("bank_reopens_after_close",list!=nullptr);
    if(list){static_cast<juce::Component*>(list)->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));voicebankWindows.removeClosed();check("escape_releases_modeless_bank",voicebankWindows.size()==0);}
    stopOtoPreviews();check("closed_preview_pointers_are_pruned",otoPreviews.empty());
    audio.releaseResources();return ok;
}
}
