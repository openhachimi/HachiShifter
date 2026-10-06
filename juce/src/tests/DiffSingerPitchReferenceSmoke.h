#pragma once
#include "../ProjectModel.h"
#include "../PianoRollComponent.h"
#include "../AudioEngine.h"
#include <iostream>

namespace hachi
{
inline bool runDiffSingerPitchReferenceSmoke(const juce::File& folder)
{
    folder.createDirectory(); bool ok=true;juce::Array<juce::var> checks;
    const auto check=[&](const char* name,bool passed) {
        auto* item=new juce::DynamicObject();item->setProperty("name",name);item->setProperty("passed",passed);
        checks.add(item);ok=ok&&passed;std::cout<<name<<'='<<passed<<std::endl;
    };
    const auto equal=[](const auto& a,const auto& b) {
        if(a.size()!=b.size())return false;
        for(size_t i=0;i<a.size();++i)
            if(std::abs(a[i].timeSeconds-b[i].timeSeconds)>1e-8||std::abs(a[i].targetMidi-b[i].targetMidi)>1e-5
                ||a[i].shape!=b[i].shape||a[i].bezierX1!=b[i].bezierX1||a[i].bezierX2!=b[i].bezierX2
                ||a[i].bezierY1!=b[i].bezierY1||a[i].bezierY2!=b[i].bezierY2)return false;
        return true;
    };
    ProjectModel model;ProjectData data;TrackData track;track.id="ds";track.pitchAlgorithm=PitchAlgorithm::utau;
    track.voicebankDirectory=folder.getChildFile("fixture-bank");track.voicebankDirectory.createDirectory();
    track.voicebankDirectory.getChildFile("dsconfig.yaml").replaceWithText("acoustic: fixture.onnx\n");
    ClipData clip;clip.id="clip";clip.startSeconds=3;clip.durationSeconds=4;
    for(int i=0;i<2;++i){NoteData n;n.id="n"+juce::String(i);n.label=i?"hao":"ni";n.startSeconds=1+i;n.durationSeconds=.8;n.midiNote=60+i*2;clip.notes.push_back(n);}
    track.clips.push_back(clip);data.tracks.push_back(track);model.replace(data);
    const auto find=[](const ProjectData& d,const juce::String& id) {
        for(const auto& t:d.tracks)for(const auto& c:t.clips)for(const auto& n:c.notes)if(n.id==id)return n;
        return NoteData{};
    };
    const auto note=[&](const juce::String& id="n0"){return find(model.snapshot(),id);};
    const std::vector<PitchCurveEditPoint> original{{-.12,58,PitchCurveShape::linear},{0,60,PitchCurveShape::linear},
        {.25,60.8f,PitchCurveShape::linear},{.5,59.2f,PitchCurveShape::linear},{.8,60,PitchCurveShape::linear}};
    auto second=original;for(auto& p:second)p.targetMidi+=2;
    const auto revision=model.revisionNumber();
    check("generation_applied",model.applyDiffSingerPitch(revision,{{"n0",original},{"n1",second}}));
    check("one_undo_for_generation_and_reference",model.revisionNumber()==revision+1);
    check("reference_matches_prediction",equal(note().diffSingerPitchReference,original));
    model.undo();check("undo_removes_both",note().diffSingerPitchReference.empty()&&note().pitchControlPoints.empty());
    model.redo();check("redo_restores_reference",equal(note().diffSingerPitchReference,original));
    const auto generated=model.snapshot();
    I18n strings;PianoRollComponent roll(model,strings);roll.setSize(1400,900);
    roll.setPixelsPerSecond(260);roll.setRowHeight(22);roll.setFocusedTrack("ds");roll.setFocusedClip("clip");
    roll.setTool(PianoRollComponent::Tool::points);roll.setShowPitchLine(true);roll.setSelectedNoteIds({"n0"});
    model.dispatchPendingMessages();roll.diagnosticRefresh();
    const auto referencePath=roll.diagnosticDiffSingerPitchReference("n0");
    check("negative_onset_and_clip_position",!referencePath.isEmpty()
        &&std::abs(referencePath.getBounds().getX()-roll.diagnosticEdgeX(3.88))<.1f);
    const auto at=juce::Point<float>(roll.diagnosticEdgeX(4.25),roll.diagnosticYForMidi(60.8f));
    const auto to=juce::Point<float>(at.x,roll.diagnosticYForMidi(64));
    const auto event=[&](juce::Point<float> pos) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),pos,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,&roll,&roll,
            juce::Time::getCurrentTime(),at,juce::Time::getCurrentTime(),1,false);
    };
    roll.mouseDown(event(at));roll.mouseDrag(event(to));roll.mouseUp(event(to));
    model.dispatchPendingMessages();roll.diagnosticRefresh();
    check("mouse_drag_changes_live_pitch",!equal(note().pitchControlPoints,original));
    check("mouse_drag_keeps_original_reference",equal(note().diffSingerPitchReference,original));
    check("reference_path_stays_put_after_edit",roll.diagnosticDiffSingerPitchReference("n0").toString()==referencePath.toString());
    const auto shot=folder.getChildFile("DS-original-pitch-dashed.png");
    const auto area=juce::Rectangle<int>((int)roll.diagnosticEdgeX(3.72),(int)roll.diagnosticYForMidi(67),640,285);
    if(auto stream=shot.createOutputStream()) {
        stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(roll.createComponentSnapshot(area),*stream);
    }
    roll.setShowPitchLine(false);check("pitch_visibility_switch",roll.diagnosticDiffSingerPitchReference("n0").isEmpty());roll.setShowPitchLine(true);
    roll.setTool(PianoRollComponent::Tool::amplitude);check("envelope_mode_hides_reference",roll.diagnosticDiffSingerPitchReference("n0").isEmpty());
    roll.setTool(PianoRollComponent::Tool::points);
    model.setNotePitchCurve("n0",{{.1,65},{.4,63}},false);
    check("freehand_keeps_reference",equal(note().diffSingerPitchReference,original));
    model.flattenNotePitch({"n0"});check("flatten_keeps_reference",equal(note().diffSingerPitchReference,original));
    juce::String error;ProjectModel reopened;
    check("save_and_load",model.save(folder.getChildFile("pitch-reference.hjpx"),error)
        &&reopened.load(folder.getChildFile("pitch-reference.hjpx"),error));
    check("reference_persisted",equal(find(reopened.snapshot(),"n0").diffSingerPitchReference,original));
    const auto beforeRetake=model.snapshot();auto retake=second;for(auto& p:retake)p.targetMidi+=1;
    check("local_retake_applied",model.applyDiffSingerPitch(model.revisionNumber(),{{"n1",retake}}));
    check("local_retake_keeps_other_reference",equal(note().diffSingerPitchReference,original)
        &&equal(note("n1").diffSingerPitchReference,retake));
    model.undo();check("retake_undo_restores_previous_reference",equal(note("n1").diffSingerPitchReference,second));
    check("stale_retake_rejected",!model.applyDiffSingerPitch(model.revisionNumber()-1,{{"n0",retake}})
        &&equal(note().diffSingerPitchReference,original));
    model.replace(generated);model.transposeNotes({"n0"},2);
    check("note_transpose_moves_reference",std::abs(note().diffSingerPitchReference[2].targetMidi-original[2].targetMidi-2)<1e-5);
    model.replace(generated);model.resizeClip("clip",4,8);
    check("clip_stretch_moves_reference",std::abs(note().diffSingerPitchReference[2].timeSeconds-.5)<1e-8);
    model.replace(generated);model.resizeNote("n0",1,1.6);
    check("note_stretch_matches_control_times",note().diffSingerPitchReference[2].timeSeconds==note().pitchControlPoints[2].timeSeconds
        &&note().diffSingerPitchReference.front().timeSeconds<0);
    model.replace(generated);model.setTempoChange(0,60,true);
    check("tempo_stretch_matches_control_times",note().diffSingerPitchReference[2].timeSeconds==note().pitchControlPoints[2].timeSeconds);
    model.replace(generated);const auto rightId=model.splitNote("n0",.4);
    check("split_preserves_reference",rightId.isNotEmpty()&&!note(rightId).diffSingerPitchReference.empty()
        &&std::abs(note("n0").diffSingerPitchReference.back().targetMidi-note(rightId).diffSingerPitchReference.front().targetMidi)<1e-5);
    model.replace(generated);const auto copies=model.duplicateNotes({"n0"},"clip",6.0);
    check("copy_preserves_reference",copies.size()==1&&equal(note(copies.front()).diffSingerPitchReference,original));
    model.replace(generated);const auto merged=model.mergeNotes({"n0","n1"});
    check("new_merged_note_clears_reference",merged.isNotEmpty()&&note(merged).diffSingerPitchReference.empty());
    model.replace(generated);const auto beforeKey=AudioEngine::diagnosticUtauRenderKey(model.snapshot(),"clip");
    auto legacy=model.snapshot();for(auto& t:legacy.tracks)for(auto& c:t.clips)for(auto& n:c.notes)n.diffSingerPitchReference.clear();
    model.replace(legacy);check("reference_not_in_audio_cache_key",AudioEngine::diagnosticUtauRenderKey(model.snapshot(),"clip")==beforeKey);
    check("legacy_project_loads_without_fabricated_prediction",model.save(folder.getChildFile("legacy.hjpx"),error)
        &&reopened.load(folder.getChildFile("legacy.hjpx"),error)&&find(reopened.snapshot(),"n0").diffSingerPitchReference.empty());
    model.replace(generated);model.setTrackVoicebankDirectory("ds",{});model.dispatchPendingMessages();roll.diagnosticRefresh();
    check("ordinary_UTAU_hides_DS_reference",roll.diagnosticDiffSingerPitchReference("n0").isEmpty());
    auto* result=new juce::DynamicObject();result->setProperty("passed",ok);result->setProperty("checks",checks);
    folder.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(result),true));return ok;
}
}
