#pragma once
#include "../ProjectModel.h"
#include "../PianoRollComponent.h"
#include "../I18n.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <algorithm>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef small
#undef small
#endif
#endif

namespace hachi
{
// Interaction only: no Main, AudioEngine, synthesis, source analysis or PCM
// comparison. A zero-PCM file supplies the real physical-owner timing route.
inline bool runNativeNoteClickInteractionSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("native-note-click-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";std::cout<<name<<'='<<pass<<std::endl;};
    const auto finish=[&]{const bool written=root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"native_note_click_interaction_fixture="<<root.getFullPathName()<<std::endl;return ok&&written;};
#if !JUCE_WINDOWS
    check("actual_Windows_timer_message_pump",false);return finish();
#else
    const auto source=root.getChildFile("silent-physical-owner.wav");juce::AudioBuffer<float> zeros(1,48000);zeros.clear();
    auto stream=source.createOutputStream();juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),48000,1,16,{},0):nullptr);
    check("minimal_zero_PCM_physical_timing_fixture",writer&&writer->writeFromAudioSampleBuffer(zeros,0,zeros.getNumSamples()));writer.reset();if(!ok)return finish();
    ProjectData seed;seed.gridDivision="1/16";
    TrackData track;track.id="native-click";track.trackType=TrackType::hachi;track.compose=true;track.pitchAlgorithm=PitchAlgorithm::world;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id="clip-"+juce::String(i);clip.sourceFile=source;clip.startSeconds=i?1.37:.137;
        clip.durationSeconds=clip.sourceDurationSeconds=.8;clip.sourceTimeMap={{0,0},{.8,.8}};
        NoteData note;note.id=i?"B":"A";note.label=i?"b":"a";note.durationSeconds=.8;note.midiNote=note.sourceMidiCenter=i?67.f:60.f;
        note.sourcePitchMeasured=true;note.contour={{0,0,0,true},{.8,0,0,true}};
        note.materialAudioId="registered-physical";note.materialRegionId=i?"row-b":"row-a";
        note.materialSourceStartSeconds=note.nativeSourceStartSeconds=0;note.materialSourceEndSeconds=note.nativeSourceEndSeconds=.8;
        note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=.11;note.consonantSeconds=.2;
        clip.notes={note};track.clips.push_back(std::move(clip));
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);I18n strings;strings.setLanguage(I18n::Language::enUS);
    PianoRollComponent roll(model,strings);roll.diagnosticSetCulling(false);roll.setFocusedTrack(track.id);
    roll.setTool(PianoRollComponent::Tool::note);roll.setPixelsPerSecond(400);roll.setRowHeight(24);roll.setSize(1200,3400);
    roll.setShowWaveforms(false);roll.setShowNativeRenderedWaveforms(false);roll.setReadsVoicebankInBackground(false);roll.diagnosticRefresh();
    int callbacks=0;juce::String clicked;std::vector<juce::String> callbackSelection;
    roll.onRequestMaterialSourceInput=[&](const juce::String& id,const juce::String&)
    {++callbacks;clicked=id;callbackSelection=roll.selectedNoteIds();};
    roll.onRequestMaterialSourceSequence=[&](const std::vector<juce::String>& ids,const juce::String& id)
    {++callbacks;clicked=id;callbackSelection=ids;};
    const auto pump=[&]
    {
        MSG message{};for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i)
        {if(message.message==WM_QUIT){PostQuitMessage(static_cast<int>(message.wParam));return false;}TranslateMessage(&message);DispatchMessageW(&message);}
        model.dispatchPendingMessages();return true;
    };
    const auto pumpFor=[&](int milliseconds)
    {const auto until=juce::Time::getMillisecondCounterHiRes()+milliseconds;do{if(!pump())return false;juce::Thread::sleep(2);}while(juce::Time::getMillisecondCounterHiRes()<until);return pump();};
    const int timeout=juce::MouseEvent::getDoubleClickTimeout();const int settle=timeout+100;
    log+="actual_double_click_timeout_ms="+juce::String(timeout)+"\n";
    constexpr int left=juce::ModifierKeys::leftButtonModifier;
    const auto event=[&](juce::Point<float> at,juce::Point<float> down,int mods,int clicks,juce::Time began,bool dragged=false)
    {return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(mods),0,0,0,0,0,
        &roll,&roll,juce::Time::getCurrentTime(),down,began,clicks,dragged);};
    const auto point=[&](const char* id)
    {const auto data=model.snapshot();for(const auto& t:data.tracks)for(const auto& c:t.clips)for(const auto& n:c.notes)if(n.id==id)
        return juce::Point<float>(float(roll.pixelForSeconds(c.startSeconds+n.startSeconds+.4)),roll.diagnosticYForMidi(n.midiNote));return juce::Point<float>{};};
    const auto click=[&](const char* id,int mods=juce::ModifierKeys::leftButtonModifier)
    {roll.diagnosticRefresh();const auto p=point(id);const auto began=juce::Time::getCurrentTime();roll.mouseDown(event(p,p,mods,1,began));roll.mouseUp(event(p,p,mods,1,began));pump();};
    const auto reset=[&]
    {
        // A fresh down also invalidates any queued work before resetting the
        // fixture; generation/revision guards remain production code's job.
        roll.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));model.resetDocument(seed);model.dispatchPendingMessages();roll.diagnosticRefresh();roll.setSelectedNoteIds({});
        callbacks=0;clicked.clear();callbackSelection.clear();
    };
    const auto selected=[&](std::vector<juce::String> expected)
    {auto actual=roll.selectedNoteIds();std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;};
    // 1. An initial plain short click is selection, not material replacement.
    reset();const auto firstFingerprint=model.contentFingerprint();click("A");pumpFor(settle);
    check("first_single_click_only_selects_no_material_callback",selected({"A"})&&callbacks==0&&model.contentFingerprint()==firstFingerprint&&!model.canUndo());
    // 2. A separate short click on that already selected note opens only after
    // the real system double-click discrimination window, without model edits.
    click("A");check("selected_short_click_is_deferred_not_mouseDown_popup",callbacks==0);pumpFor(settle);
    check("second_selected_short_click_requests_material_without_mutation",callbacks==1&&clicked=="A"&&selected({"A"})&&model.contentFingerprint()==firstFingerprint&&!model.canUndo());
    // 3. Actual Ctrl-add multi-selection survives the later plain short click.
    reset();click("A");click("B",left|juce::ModifierKeys::ctrlModifier);pumpFor(settle);
    check("actual_Ctrl_add_keeps_two_notes_without_opening",selected({"A","B"})&&callbacks==0);
    const auto multiFingerprint=model.contentFingerprint();click("A");pumpFor(settle);std::sort(callbackSelection.begin(),callbackSelection.end());
    check("selected_plain_click_preserves_multi_and_requests_actual_sequence",callbacks==1&&clicked=="A"&&selected({"A","B"})
        &&callbackSelection==std::vector<juce::String>{"A","B"}&&model.contentFingerprint()==multiFingerprint);
    // 4. Fast double click must cancel a queued material action and retain the
    // old real P-to-grid edit. The authority itself supplies the exact oracle.
    reset();roll.setSelectedNoteIds({"A"});ProjectModel control;control.resetDocument(seed);
    const bool expectedAlignment=control.alignNotePreutteranceToGrid("A",.25,error);const auto expectedFingerprint=control.contentFingerprint();
    click("A");const auto p=point("A");const auto began=juce::Time::getCurrentTime();
    roll.mouseDown(event(p,p,left,2,began));roll.mouseDoubleClick(event(p,p,left,2,began));roll.mouseUp(event(p,p,left,2,began));pumpFor(settle);
    check("fast_double_click_retains_old_P_grid_alignment_cancels_material_popup",expectedAlignment&&callbacks==0&&model.contentFingerprint()==expectedFingerprint&&model.canUndo());
    // 5. Holding is never an open-on-down gesture. A long release remains a
    // hold, and an actual move gesture changes placement without a popup.
    reset();roll.setSelectedNoteIds({"A"});const auto held=point("A");const auto holdBegan=juce::Time::getCurrentTime();
    roll.mouseDown(event(held,held,left,1,holdBegan));pumpFor(settle);
    check("held_mouseDown_does_not_open_before_release",callbacks==0);roll.mouseUp(event(held,held,left,1,holdBegan));pumpFor(settle);
    check("long_hold_release_does_not_open_material",callbacks==0&&model.contentFingerprint()==firstFingerprint);
    reset();roll.setSelectedNoteIds({"A"});const auto from=point("A"),to=from.translated(100,0);const auto dragBegan=juce::Time::getCurrentTime();
    roll.mouseDown(event(from,from,left,1,dragBegan));roll.mouseDrag(event(to,from,left,1,dragBegan,true));roll.mouseUp(event(to,from,left,1,dragBegan,true));pumpFor(settle);
    const auto moved=model.snapshot();const auto& movedClip=moved.tracks[0].clips[0];
    check("held_actual_drag_moves_note_not_material_popup",callbacks==0&&model.contentFingerprint()!=firstFingerprint
        &&std::abs(movedClip.startSeconds+movedClip.notes[0].startSeconds-.137)>.001);
    // 6. Both revision and Escape invalidate an otherwise eligible timer.
    reset();roll.setSelectedNoteIds({"A"});click("A");model.setNoteLabel("B","revision-change");const auto changed=model.contentFingerprint();pumpFor(settle);
    check("revision_change_cancels_queued_material_request",callbacks==0&&model.contentFingerprint()==changed);
    reset();roll.setSelectedNoteIds({"A"});click("A");roll.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));pumpFor(settle);
    check("Escape_cancels_queued_material_request_without_edit",callbacks==0&&model.contentFingerprint()==firstFingerprint);
    return finish();
#endif
}
}
