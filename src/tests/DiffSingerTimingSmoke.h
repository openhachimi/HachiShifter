#pragma once
#include "../DiffSingerPhonemeEditor.h"
#include "../AudioEngine.h"
#include "../backend/DiffSingerRenderer.h"
#include <iostream>

namespace hachi
{
inline bool runDiffSingerTimingSmoke(const juce::File& bank, const juce::File& folder)
{
    folder.createDirectory(); bool ok = true;
    const auto check = [&](const char* name, bool result)
    { std::cout << name << '=' << result << std::endl; ok = result && ok; };
    ProjectModel model; ProjectData data; TrackData track; ClipData clip;
    track.id="ds"; track.pitchAlgorithm=PitchAlgorithm::utau; track.voicebankDirectory=bank;
    clip.id="clip"; clip.durationSeconds=2.4;
    for(int i=0;i<2;++i)
    {
        NoteData note; note.id=juce::String(i); note.label=i==0?"ni":"hao";
        note.startSeconds=i==0?.3:1.; note.durationSeconds=i==0?.7:.9; note.midiNote=60+(float)i*2;
        clip.notes.push_back(note);
    }
    track.clips.push_back(clip); data.tracks.push_back(track); model.replace(data);
    check("legacy_DS_migrated_to_mou",model.snapshot().tracks[0].utauMode==UtauMode::mou);
    model.setTrackUtauMode("ds",UtauMode::classic);
    check("DS_stays_in_mou",model.snapshot().tracks[0].utauMode==UtauMode::mou);
    backend::UtauRenderRequest request; request.voicebankDirectory=bank; request.targetDurationSeconds=2.4;
    request.notes=AudioEngine::diagnosticUtauRequestNotes(model.snapshot(),"clip");
    const auto json=backend::DiffSingerRenderer::requestJson(request,"timing");
    const auto result=backend::DiffSingerRenderer::invoke(json);
    check("real_duration_prediction",(bool)result["ok"] && result["phonemes"].size()==6);
    if (!(bool)result["ok"]) {std::cout << result["error"].toString() << std::endl;return false;}
    const auto revision=model.revisionNumber();
    DiffSingerPhonemeEditor editor(result,json["notes"],[&](const auto& values)
    {return model.applyDiffSingerTiming(revision,values);});
    const auto original=editor.phonemes();
    DiffSingerPhonemeEditor scoped(result,json["notes"],{}, {}, "1", {"1"});
    check("selected_note_only_visible",scoped.displayedPhonemes().size()==2
        && !scoped.noteVisible("0") && scoped.noteVisible("1") && !scoped.noteVisible(""));
    check("selected_note_keeps_context",scoped.phonemes().size()==original.size()
        && scoped.phonemes()[3].start==original[3].start);
    check("hidden_boundary_cannot_move",!scoped.moveBoundary(1,original[1].start-.04));
    check("selected_boundary_can_move",scoped.moveBoundary(3,original[3].start-.04));
    check("apply_only_selected_note",scoped.timings().size()==1 && scoped.timings()[0].first=="1"
        && scoped.timings()[0].second.isNotEmpty());
    scoped.resetPrediction(true);
    check("reset_only_selected_note",scoped.timings().size()==1 && scoped.timings()[0].second.isEmpty());
    if (auto stream=folder.getChildFile("DS-single-note.png").createOutputStream())
    {
        stream->setPosition(0);stream->truncate();
        juce::PNGImageFormat().writeImageToStream(scoped.createComponentSnapshot(scoped.getLocalBounds()),*stream);
    }
    DiffSingerPhonemeEditor multi(result,json["notes"],{}, {}, "0", {"0","1"});
    check("multi_selection_only_owned_phonemes",multi.displayedPhonemes().size()==4 && multi.timings().size()==2);
    check("empty_selection_shows_full_track",editor.displayedPhonemes().size()==6 && editor.timings().size()==2);
    DiffSingerPhonemeEditor hiddenEdits(result,json["notes"],{});
    hiddenEdits.moveBoundary(1,original[1].start-.03);
    hiddenEdits.moveBoundary(3,original[3].start-.03);
    ProjectModel scopedModel; scopedModel.replace(model.snapshot());
    const auto both=hiddenEdits.timings();
    scopedModel.applyDiffSingerTiming(scopedModel.revisionNumber(),both);
    scopedModel.applyDiffSingerTiming(scopedModel.revisionNumber(),scoped.timings());
    const auto retained=scopedModel.snapshot().tracks[0].clips[0].notes;
    check("reset_apply_preserves_hidden_note_edit",retained[0].diffSingerTiming==both[0].second
        && retained[1].diffSingerTiming.isEmpty());
    auto slurRequest=request;slurRequest.notes[1].alias="+";
    const auto slurJson=backend::DiffSingerRenderer::requestJson(slurRequest,"timing");
    const auto slurResult=backend::DiffSingerRenderer::invoke(slurJson);
    DiffSingerPhonemeEditor slur(slurResult,slurJson["notes"],{}, {}, "1", {"1"});
    const auto continuation=slur.displayedPhonemes();
    check("single_slur_shows_its_vowel_read_only",(bool)slurResult["ok"] && continuation.size()==1
        && continuation[0].kind=="V" && !continuation[0].editable
        && std::abs(continuation[0].start-1.)<.012 && !slur.noteVisible("0"));
    check("vowel_beat_locked",!editor.moveBoundary(4,original[4].start-.05));
    check("consonant_boundary_moves",editor.moveBoundary(3,original[3].start-.08));
    const auto changed=editor.phonemes();
    check("shared_boundary_negative_onset",changed[3].start==changed[2].end
        && changed[3].start<changed[3].noteStart && changed[4].start==original[4].start);
    editor.keyPressed(juce::KeyPress('Z',juce::ModifierKeys::ctrlModifier,0));
    check("editor_undo",std::abs(editor.phonemes()[3].start-original[3].start)<1e-9);
    editor.keyPressed(juce::KeyPress('Y',juce::ModifierKeys::ctrlModifier,0));
    check("editor_redo",std::abs(editor.phonemes()[3].start-changed[3].start)<1e-9);
    if (auto stream=folder.getChildFile("DS-phoneme-editor.png").createOutputStream())
        juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*stream);
    auto edits=editor.timings();
    check("single_project_undo_step",model.applyDiffSingerTiming(revision,edits)
        && model.revisionNumber()==revision+1);
    check("stale_apply_rejected",!model.applyDiffSingerTiming(revision,edits));
    const auto edited=model.snapshot();
    check("MIDI_preserved",edited.tracks[0].clips[0].notes[1].startSeconds==1.
        && edited.tracks[0].clips[0].notes[1].durationSeconds==.9);
    check("render_hash_invalidated",AudioEngine::utauNoteRenderHash(clip.notes[1])
        !=AudioEngine::utauNoteRenderHash(edited.tracks[0].clips[0].notes[1]));
    juce::String error;
    const auto projectFile=folder.getChildFile("DS-phoneme-edit.hjpx");
    check("save",model.save(projectFile,error)); ProjectModel loaded;
    check("load",loaded.load(projectFile,error));
    check("timing_persisted",loaded.snapshot().tracks[0].clips[0].notes[1].diffSingerTiming
        ==edited.tracks[0].clips[0].notes[1].diffSingerTiming);
    request.notes=AudioEngine::diagnosticUtauRequestNotes(loaded.snapshot(),"clip");
    const auto replay=backend::DiffSingerRenderer::invoke(backend::DiffSingerRenderer::requestJson(request,"timing"));
    check("saved_boundary_reaches_worker",(bool)replay["ok"]
        && std::abs((double)replay["phonemes"][3]["start"]-changed[3].start)<1e-9);
    editor.resetPrediction(true);
    bool empty=true; for(const auto& pair:editor.timings()) empty=empty && pair.second.isEmpty();
    check("reset_all_predictions",empty && std::abs(editor.phonemes()[3].start-original[3].start)<1e-9);
    model.undo();
    check("project_undo",model.snapshot().tracks[0].clips[0].notes[1].diffSingerTiming.isEmpty());
    model.redo();
    check("project_redo",model.snapshot().tracks[0].clips[0].notes[1].diffSingerTiming==edits[1].second);

    // Keep the actual device rate: export reconnects its callback afterwards.
    AudioEngine engine;
    const auto render = [&](const ProjectData& project, const juce::String& name)
    {
        engine.selectEveryUtauNote(project); engine.syncProject(project);
        for(int i=0;i<1200 && !engine.hasCurrentRenderedAudio();++i)
        {if(i>30 && !engine.renderProgress()) break; juce::Thread::sleep(25);}
        return engine.hasCurrentRenderedAudio() && engine.exportWav(folder.getChildFile(name),error);
    };
    check("edited_audio_export",render(model.snapshot(),"edited.wav"));
    model.undo(); check("undo_audio_export",render(model.snapshot(),"original.wav"));
    juce::MemoryBlock originalWav, editedWav;
    folder.getChildFile("original.wav").loadFileAsData(originalWav);
    folder.getChildFile("edited.wav").loadFileAsData(editedWav);
    check("timing_changes_audio",editedWav.getSize()>1000 && editedWav!=originalWav);
    model.redo(); check("redo_audio_export",render(model.snapshot(),"redo.wav"));
    juce::MemoryBlock a,b;
    folder.getChildFile("edited.wav").loadFileAsData(a); folder.getChildFile("redo.wav").loadFileAsData(b);
    check("redo_reuses_correct_audio",a==b);

    // Reuse the same worker while typing a lyric and undoing/redoing it. The
    // namespace zh/ is shared by both syllables; only b versus zh changes.
    auto lyricData = model.snapshot();
    auto& lyricNotes = lyricData.tracks[0].clips[0].notes;
    lyricNotes.resize(1); lyricNotes[0].label="zhang";
    lyricNotes[0].startSeconds=.5; lyricNotes[0].durationSeconds=.5;
    lyricNotes[0].diffSingerTiming.clear(); model.replace(lyricData);
    for (int step=0; step<4; ++step)
    {
        if (step==1) model.setNoteLabel("0","bang");
        if (step==2) model.undo();
        if (step==3) model.redo();
        const auto bang=step==1 || step==3;
        request.notes=AudioEngine::diagnosticUtauRequestNotes(model.snapshot(),"clip");
        const auto input=backend::DiffSingerRenderer::requestJson(request,"timing");
        const auto predicted=backend::DiffSingerRenderer::invoke(input);
        const auto valid=(bool)predicted["ok"] && predicted["phonemes"].size()==4
            && input["notes"][0]["lyric"].toString()==(bang?"bang":"zhang")
            && predicted["phonemes"][1]["token"].toString()==(bang?"zh/b":"zh/zh")
            && predicted["phonemes"][2]["token"].toString()=="zh/ang";
        const auto name=juce::String("lyric_")+juce::String(step)+(bang?"_bang":"_zhang");
        check(name.toRawUTF8(),valid);
        if (!valid) continue;
        DiffSingerPhonemeEditor preview(predicted,input["notes"],{}, {}, "0");
        if (auto stream=folder.getChildFile(name+".png").createOutputStream())
        {
            stream->setPosition(0); stream->truncate();
            juce::PNGImageFormat().writeImageToStream(preview.createComponentSnapshot(preview.getLocalBounds()),*stream);
        }
    }
    const auto oldKey=AudioEngine::diagnosticUtauRenderKey(model.snapshot(),"clip");
    const auto beforePronunciation=model.snapshot().tracks[0].clips[0].notes[0].label;
    check("pronunciation_and_dictionary_apply",model.applyDiffSingerPronunciation(model.revisionNumber(),"ds",
        {{"0","[zh/zh zh/a en/ng]"}},"en/read = [r eh d]"));
    check("pronunciation_invalidates_audio_cache",oldKey!=AudioEngine::diagnosticUtauRenderKey(model.snapshot(),"clip"));
    check("pronunciation_keeps_display_lyric",model.snapshot().tracks[0].clips[0].notes[0].label==beforePronunciation);
    check("pronunciation_export",render(model.snapshot(),"pronunciation.wav"));
    const auto pronunciationFile=folder.getChildFile("pronunciation-dictionary.hjpx");
    check("pronunciation_save",model.save(pronunciationFile,error));
    ProjectModel reopened;
    check("pronunciation_load",reopened.load(pronunciationFile,error));
    check("pronunciation_dictionary_roundtrip",reopened.snapshot().tracks[0].diffSingerDictionary=="en/read = [r eh d]"
        && reopened.snapshot().tracks[0].clips[0].notes[0].diffSingerPronunciation=="[zh/zh zh/a en/ng]");
    return ok;
}
}
