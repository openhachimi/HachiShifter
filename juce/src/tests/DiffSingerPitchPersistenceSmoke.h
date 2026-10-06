#pragma once
#include "../MainComponent.h"
#include "../DiffSingerPitchHandles.h"
#include <iostream>
namespace hachi
{
inline bool MainComponent::diagnosticPitchPersistence(const juce::File& original,
    const juce::File& folder, bool reopenOnly)
{
    folder.createDirectory();bool ok=true;juce::Array<juce::var> checks;
    const auto check=[&](const char* name,bool passed) {
        auto* c=new juce::DynamicObject();c->setProperty("name",name);c->setProperty("passed",passed);
        checks.add(c);ok=ok&&passed;std::cout<<name<<'='<<passed<<std::endl;
    };
    const auto same=[](const auto& a,const auto& b) {
        if(a.size()!=b.size())return false;
        for(size_t i=0;i<a.size();++i)
            if(a[i].timeSeconds!=b[i].timeSeconds||a[i].targetMidi!=b[i].targetMidi||a[i].shape!=b[i].shape
                ||a[i].bezierX1!=b[i].bezierX1||a[i].bezierX2!=b[i].bezierX2||a[i].bezierY1!=b[i].bezierY1||a[i].bezierY2!=b[i].bezierY2)return false;
        return true;
    };
    const auto treeFrom=[](const juce::File& file) {
        juce::MemoryBlock bytes;file.loadFileAsData(bytes);return juce::ValueTree::readFromData(bytes.getData(),bytes.getSize());
    };
    juce::String error;ProjectModel source;
    if(!source.load(original,error)||source.snapshot().tracks.empty())return false;
    const auto baseline=source.snapshot();const auto& track=baseline.tracks.front();const auto& clip=track.clips.front();
    const auto first=clip.notes.front();const auto reference=first.diffSingerPitchReference;
    const auto note=[&]{return project.snapshot().tracks.front().clips.front().notes.front();};
    const auto saved=folder.getChildFile("edited-saved-project.hjpx");
    setSize(1400,860);
    const auto refresh=[&] {
        project.dispatchPendingMessages();diagnosticSelectTrack(track.id);pianoRoll.setFocusedClip(clip.id);
        closeEnvelopeLanes();setSourceEditMode(false);viewOptions.pitchLine=true;applyViewOptions();
        pianoRoll.setTool(PianoRollComponent::Tool::points);setToolButton(pointButton);
        pianoRoll.setPixelsPerSecond(420);pianoRoll.setRowHeight(30);pianoRoll.diagnosticRefresh();
        pianoViewport.setViewPosition(std::max(0,(int)pianoRoll.diagnosticEdgeX(clip.startSeconds+first.startSeconds-.35)),
            std::max(0,(int)pianoRoll.diagnosticYForMidi(first.midiNote+6)));
    };
    const auto shot=[&](const char* name) {
        if(auto stream=folder.getChildFile(name).createOutputStream()) {
            stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream);
        }
    };
    const auto finish=[&] {
        auto* report=new juce::DynamicObject();report->setProperty("passed",ok);report->setProperty("checks",checks);
        report->setProperty("reference_points",(int)reference.size());
        folder.getChildFile(reopenOnly?"fresh-process-report.json":"report.json").replaceWithText(juce::JSON::toString(juce::var(report),true));
        return ok;
    };
    if(reopenOnly)
    {
        loadProjectFile(saved);refresh();
        check("fresh_process_keeps_loaded_reference",same(note().diffSingerPitchReference,reference));
        check("fresh_process_keeps_reference_provenance",note().diffSingerPitchReferenceFromSavedPitch);
        check("fresh_process_keeps_manual_curve_separate",!same(note().pitchControlPoints,reference)&&note().pitchControlPoints.size()==2);
        check("fresh_process_displays_reference",!pianoRoll.diagnosticDiffSingerPitchReference(first.id).isEmpty());
        check("fresh_process_can_restore_to_reference",pianoRoll.diffSingerPitchRestoreAvailable());
        shot("DS-reference-after-restart.png");return finish();
    }
    check("legacy_file_has_no_reference_schema",!treeFrom(original).hasProperty("diffSingerPitchReferenceVersion"));
    check("legacy_current_pitch_used_as_explicit_load_reference",!reference.empty()&&same(reference,first.pitchControlPoints)
        &&first.diffSingerPitchReferenceFromSavedPitch);
    auto withoutReference=baseline;for(auto& t:withoutReference.tracks)for(auto& c:t.clips)for(auto& n:c.notes)
        {n.diffSingerPitchReference.clear();n.diffSingerPitchReferenceFromSavedPitch=false;}
    check("legacy_migration_does_not_change_audio",AudioEngine::diagnosticUtauRenderKey(baseline,clip.id)
        ==AudioEngine::diagnosticUtauRenderKey(withoutReference,clip.id));
    check("repair_copy_saved_without_changing_current_pitch",source.save(folder.getChildFile("diff-reference-restored.hjpx"),error)
        &&same(source.snapshot().tracks.front().clips.front().notes.front().pitchControlPoints,first.pitchControlPoints));
    loadProjectFile(original);refresh();
    check("real_open_file_displays_legacy_reference",!pianoRoll.diagnosticDiffSingerPitchReference(first.id).isEmpty()
        &&pianoRoll.diffSingerPitchRestoreAvailable());
    auto manual=pianoRoll.diagnosticPitchAnchors(first.id);int index=0;
    for(size_t i=1;i+1<manual.size();++i)if(!manual[i].diffSingerRestoreSupport){index=(int)i;break;}
    prepareSampledPitchPointEdit(manual,index);manual[(size_t)index].targetMidi+=2;
    project.setNotePitchCurve(first.id,manual,true);refresh();
    check("manual_edit_keeps_loaded_reference",same(note().diffSingerPitchReference,reference)&&!same(note().pitchControlPoints,reference));
    check("real_save_embeds_reference",saveProjectTo(saved)&&treeFrom(saved).hasProperty("diffSingerPitchReferenceVersion"));
    project.clear();loadProjectFile(saved);refresh();
    check("real_reopen_preserves_reference",same(note().diffSingerPitchReference,reference)&&note().diffSingerPitchReferenceFromSavedPitch);
    check("real_reopen_preserves_manual_curve",same(note().pitchControlPoints,manual));
    check("real_reopen_draws_reference",!pianoRoll.diagnosticDiffSingerPitchReference(first.id).isEmpty());
    project.setNotePitchCurve(first.id,{{0,first.midiNote+2},{first.durationSeconds,first.midiNote+1}},true);
    check("overwrite_save",saveProjectTo(saved));project.clear();loadProjectFile(saved);refresh();
    check("overwrite_reopen_keeps_reference_and_shorter_edit",same(note().diffSingerPitchReference,reference)&&note().pitchControlPoints.size()==2);
    shot("DS-reference-after-reopen.png");
    auto newPrediction=reference;for(auto& p:newPrediction)p.targetMidi+=.3f;
    check("new_DS_generation_replaces_loaded_baseline",project.applyDiffSingerPitch(project.revisionNumber(),{{first.id,newPrediction}})
        &&same(note().diffSingerPitchReference,newPrediction)&&!note().diffSingerPitchReferenceFromSavedPitch);
    project.undo();check("undo_recovers_saved_pitch_provenance",same(note().diffSingerPitchReference,reference)&&note().diffSingerPitchReferenceFromSavedPitch);

    // A real prediction in a file without the new schema marker always wins.
    auto oldWithPrediction=treeFrom(saved);oldWithPrediction.removeProperty("diffSingerPitchReferenceVersion",nullptr);
    const auto legacyCopy=folder.getChildFile("legacy-with-reference.hjpx");
    if(auto stream=legacyCopy.createOutputStream()) {stream->setPosition(0);stream->truncate();oldWithPrediction.writeToStream(*stream);}
    ProjectModel loaded;check("existing_reference_never_rebuilt_from_manual_pitch",loaded.load(legacyCopy,error)
        &&same(loaded.snapshot().tracks.front().clips.front().notes.front().diffSingerPitchReference,reference));
    auto noReference=withoutReference;for(auto& t:noReference.tracks)for(auto& c:t.clips)for(auto& n:c.notes)
        {n.pitchControlPoints.clear();n.contour.clear();}
    loaded.replace(noReference);loaded.save(folder.getChildFile("midi-only.hjpx"),error);
    check("new_MIDI_only_note_has_no_fabricated_prediction",loaded.load(folder.getChildFile("midi-only.hjpx"),error)
        &&loaded.snapshot().tracks.front().clips.front().notes.front().diffSingerPitchReference.empty());
    auto ordinary=oldWithPrediction;auto ordinaryTrack=ordinary.getChildWithName("Track");
    ordinaryTrack.setProperty("voicebankDirectory","",nullptr);ordinaryTrack.setProperty("voicebankDirectoryRelative","",nullptr);
    for(auto c:ordinaryTrack)for(auto n:c)for(int i=n.getNumChildren()-1;i>=0;--i)
        if(n.getChild(i).hasType("DiffSingerPitchReferencePoint"))n.removeChild(i,nullptr);
    const auto classicFile=folder.getChildFile("ordinary-utau.hjpx");
    if(auto stream=classicFile.createOutputStream()) {stream->setPosition(0);stream->truncate();ordinary.writeToStream(*stream);}
    check("ordinary_UTAU_not_migrated",loaded.load(classicFile,error)
        &&loaded.snapshot().tracks.front().clips.front().notes.front().diffSingerPitchReference.empty());
    return finish();
}
}
