#pragma once
#include "../MaterialAnnotationSession.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>
#include <iostream>

namespace hachi
{
inline bool runNativePronunciationEdgeEditSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("native-edge-edit-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;
    const auto check=[&](const char* key,bool pass){ok&=pass;log+=juce::String(key)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<key<<"="<<pass<<"|"<<error<<std::endl;error.clear();};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-9;};
    const auto makeRow=[](double begin,double end,const juce::String& id)
    {
        SampleRegionSetting r;r.name=id;r.materialAudioId="edit-audio";r.materialRegionId=id;r.regionStartSeconds=begin;r.regionEndSeconds=end;
        r.alignmentSeconds=begin+(end-begin)*.2;r.fixedDurationSeconds=(end-begin)*.25;r.overlapSeconds=(end-begin)*.1;
        r.melodynePitchCenterCents=6200;r.melodyneOriginalPitchCenterCents=6000;r.melodynePitchDrift=.73;r.melodyneAmplitude=.61;
        NativePronunciationEdges p;p.entryFromSourceSeconds=begin;p.entryToSourceSeconds=begin+(end-begin)*.1;
        p.exitFromSourceSeconds=begin+(end-begin)*.75;p.exitToSourceSeconds=p.terminationSourceSeconds=end;p.provenance="authored-fixture";
        r.nativePronunciationEdges=p;return r;
    };
    auto row=makeRow(0,2,"a");std::optional<NativePronunciationEdges> result;
    check("affine_maps_all_anchors",affineNativePronunciationEdges(row.nativePronunciationEdges,0,2,1,5,result,error)
        &&result&&near(result->entryToSourceSeconds,1.4)&&near(result->exitFromSourceSeconds,4)&&result->terminationSourceSeconds==5);
    check("clip_keeps_optin_and_zero_windows",clipNativePronunciationEdges(row.nativePronunciationEdges,0,2,.5,1,result,error)
        &&result&&result->entryFromSourceSeconds==.5&&result->entryToSourceSeconds==.5&&result->exitFromSourceSeconds==1&&result->terminationSourceSeconds==1);
    const auto sentinel=result;auto invalid=row.nativePronunciationEdges;invalid->version=99;
    check("bad_old_version_output_unchanged",!affineNativePronunciationEdges(invalid,0,2,0,3,result,error)&&result==sentinel);
    invalid=row.nativePronunciationEdges;invalid->entryToSourceSeconds=std::numeric_limits<double>::quiet_NaN();
    check("bad_old_number_output_unchanged",!clipNativePronunciationEdges(invalid,0,2,0,1,result,error)&&result==sentinel);
    check("clip_cannot_extrapolate",!clipNativePronunciationEdges(row.nativePronunciationEdges,0,2,0,2.5,result,error)&&result==sentinel);
    auto first=makeRow(0,1,"first"),last=makeRow(1,2,"last");
    check("merge_takes_first_entry_last_exit",mergeNativePronunciationEdges({first,last},result,error)&&result
        &&near(result->entryToSourceSeconds,.1)&&near(result->exitFromSourceSeconds,1.75)&&result->terminationSourceSeconds==2);
    first.nativePronunciationEdges.reset();
    check("merge_missing_entry_is_explicit_flat",mergeNativePronunciationEdges({first,last},result,error)&&result&&result->entryToSourceSeconds==0&&near(result->exitFromSourceSeconds,1.75));
    first=makeRow(0,1,"first");last.nativePronunciationEdges.reset();
    check("merge_missing_exit_is_explicit_flat",mergeNativePronunciationEdges({first,last},result,error)&&result&&near(result->entryToSourceSeconds,.1)&&result->exitFromSourceSeconds==2);
    first.nativePronunciationEdges.reset();
    check("all_legacy_merge_remains_absent",mergeNativePronunciationEdges({first,last},result,error)&&!result);
    auto legacyRow=makeRow(0,1,"legacy");legacyRow.nativePronunciationEdges.reset();
    OttoMaterialIO::Document legacyDoc;legacyDoc.audio={{"edit-audio","source.wav",{legacyRow}}};juce::String legacyText,legacyAgain;
    check("legacy_fixture_encodes",OttoMaterialIO::encode(legacyDoc,legacyText,error));
    check("absent_affine_stays_absent",affineNativePronunciationEdges({},0,1,0,1,result,error)&&!result);
    legacyDoc.audio[0].rows[0].nativePronunciationEdges=result;
    check("profile_absent_old_text_unchanged",OttoMaterialIO::encode(legacyDoc,legacyAgain,error)&&legacyAgain==legacyText);
    NativePronunciationEdges defaults;
    check("default_reuses_source_overlap_and_C",defaultNativePronunciationEdges(row,defaults,error)&&near(defaults.entryToSourceSeconds,.2)&&near(defaults.exitFromSourceSeconds,.5));
    auto legacyTiming=row;legacyTiming.overlapSeconds=9;legacyTiming.fixedDurationSeconds=8;
    check("new_default_bounds_long_legacy_overlap_and_C_without_rewriting_raw",defaultNativePronunciationEdges(legacyTiming,defaults,error)
        &&defaults.entryToSourceSeconds==row.regionEndSeconds&&defaults.exitFromSourceSeconds==row.regionEndSeconds
        &&legacyTiming.overlapSeconds==9&&legacyTiming.fixedDurationSeconds==8&&legacyTiming.alignmentSeconds==row.alignmentSeconds);
    legacyTiming.overlapSeconds=-.3;legacyTiming.fixedDurationSeconds=-.2;
    check("new_default_negative_timing_projects_zero_windows_without_rewriting_raw",defaultNativePronunciationEdges(legacyTiming,defaults,error)
        &&defaults.entryToSourceSeconds==row.regionStartSeconds&&defaults.exitFromSourceSeconds==row.regionStartSeconds
        &&legacyTiming.overlapSeconds==-.3&&legacyTiming.fixedDurationSeconds==-.2);
    const auto previousDefault=defaults;legacyTiming.overlapSeconds=std::numeric_limits<double>::quiet_NaN();
    check("new_default_nonfinite_rejects_without_changing_output",!defaultNativePronunciationEdges(legacyTiming,defaults,error)&&defaults==previousDefault);
    auto changed=row;check("explicit_user_edge_edit",editNativePronunciationEdges(changed,true,.3,1.7,error)&&changed.nativePronunciationEdges&&near(changed.nativePronunciationEdges->exitFromSourceSeconds,1.7)
        &&changed.melodynePitchDrift==row.melodynePitchDrift&&changed.alignmentSeconds==row.alignmentSeconds);
    const auto beforeBad=changed.nativePronunciationEdges;
    check("invalid_user_edit_atomic",!editNativePronunciationEdges(changed,true,3,1.7,error)&&changed.nativePronunciationEdges==beforeBad);
    check("explicit_off_removes_only_profile",editNativePronunciationEdges(changed,false,0,0,error)&&!changed.nativePronunciationEdges&&changed.melodyneAmplitude==row.melodyneAmplitude);

    const auto audio=root.getChildFile("source.wav"),manifest=root.getChildFile("otto.ini");
    juce::AudioBuffer<float> pcm(1,32000);pcm.clear();juce::WavAudioFormat format;
    auto stream=audio.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(stream?format.createWriterFor(stream.release(),8000,1,16,{},0):nullptr);
    check("physical_fixture_written",writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));writer.reset();
    const auto audioSha=juce::SHA256(audio).toHexString();
    ProjectModel untouchedProject;ProjectData score;TrackData scoreTrack;scoreTrack.id="untouched";
    ClipData scoreClip;scoreClip.id="untouched-clip";scoreClip.sourceFile=audio;scoreClip.sourceTimeMap={{0,0},{1,1}};
    NoteData scoreNote;scoreNote.id="untouched-note";scoreNote.midiNote=63;scoreNote.pitchControlPoints={{0,63},{1,65}};scoreNote.amplitudeEnvelope={{0,-3},{1,-7}};
    scoreClip.notes={scoreNote};scoreTrack.clips={scoreClip};score.tracks={scoreTrack};untouchedProject.resetDocument(score);
    const auto musicFingerprint=untouchedProject.contentFingerprint();
    auto successor=makeRow(2,3,"successor");row.connectedNextRegionId=successor.materialRegionId;
    OttoMaterialIO::Document document;document.audio={{"edit-audio","source.wav",{row,successor}}};
    check("seed_saved",OttoMaterialIO::saveDocument(manifest,document,error));
    auto session=MaterialAnnotationSession::open(audio,manifest,error,"edit-audio");check("session_open",session!=nullptr);if(!session)return false;
    const auto original=session->rows();
    check("split_profile_rows_no_strict_codec_rejection",session->split({1},error)&&session->rows().size()==3);
    if(session->rows().size()!=3)return false;
    const auto splitRows=session->rows();const auto rightId=splitRows[1].materialRegionId;
    check("split_left_right_both_profiles_and_successor",splitRows[0].nativePronunciationEdges&&splitRows[1].nativePronunciationEdges
        &&splitRows[0].nativePronunciationEdges->exitToSourceSeconds==1&&splitRows[1].nativePronunciationEdges->entryFromSourceSeconds==1
        &&splitRows[0].connectedNextRegionId==rightId&&splitRows[1].connectedNextRegionId=="successor");
    check("split_undo_redo_exact_profile",session->undo()&&session->rows()[0].nativePronunciationEdges==original[0].nativePronunciationEdges
        &&session->redo()&&session->rows()[1].nativePronunciationEdges==splitRows[1].nativePronunciationEdges);
    check("move_boundary_affine_profiles",session->moveBoundary("a",1.2,error)&&near(session->rows()[0].nativePronunciationEdges->entryToSourceSeconds,.24)
        &&near(session->rows()[1].nativePronunciationEdges->exitFromSourceSeconds,1.6));
    check("merge_session_uses_actual_outer_profiles",session->merge({"a",rightId},error)&&session->rows().size()==2
        &&near(session->rows()[0].nativePronunciationEdges->entryToSourceSeconds,.24)&&near(session->rows()[0].nativePronunciationEdges->exitFromSourceSeconds,1.6)
        &&session->rows()[0].connectedNextRegionId=="successor");
    check("remove_successor_for_independent_resize",session->erase({"successor"},error));
    auto resized=session->rows()[0];
    resized.regionStartSeconds=.25;resized.regionEndSeconds=2.75;resized.alignmentSeconds=.75;resized.fixedDurationSeconds=.625;
    check("setRow_inherited_profile_affine",session->setRow("a",resized,error)
        &&near(session->rows()[0].nativePronunciationEdges->entryToSourceSeconds,.55)&&near(session->rows()[0].nativePronunciationEdges->exitFromSourceSeconds,2.25));
    auto explicitSegments=session->rows()[0];explicitSegments.regionEndSeconds=3;
    const auto segmentMap=[](double value){return .25+(value-.25)*1.1;};
    explicitSegments.alignmentSeconds=segmentMap(explicitSegments.alignmentSeconds);explicitSegments.fixedDurationSeconds*=1.1;
    for(auto& segment:explicitSegments.segments){segment.sourceStartSeconds=segmentMap(segment.sourceStartSeconds);segment.sourceEndSeconds=segmentMap(segment.sourceEndSeconds);segment.alignmentSeconds=segmentMap(segment.alignmentSeconds);segment.confidence=.8f;}
    check("inherited_profile_affine_even_with_explicit_segments",session->setRow("a",explicitSegments,error)&&session->rows()[0].nativePronunciationEdges
        &&near(session->rows()[0].nativePronunciationEdges->entryToSourceSeconds,.58)&&near(session->rows()[0].nativePronunciationEdges->exitFromSourceSeconds,2.45));
    check("explicit_segment_edit_undo",session->undo()&&near(session->rows()[0].regionEndSeconds,2.75));
    auto explicitProfile=session->rows()[0];explicitProfile.regionStartSeconds=.5;explicitProfile.alignmentSeconds=.9;
    explicitProfile.nativePronunciationEdges->entryFromSourceSeconds=.5;explicitProfile.nativePronunciationEdges->entryToSourceSeconds=.8;
    check("explicit_profile_is_not_remapped_twice",session->setRow("a",explicitProfile,error)&&near(session->rows()[0].nativePronunciationEdges->entryToSourceSeconds,.8));
    const auto stable=session->rows()[0];auto rejectedRow=stable;rejectedRow.nativePronunciationEdges->exitFromSourceSeconds=99;
    const auto diskBefore=juce::SHA256(manifest).toHexString();
    check("invalid_row_rejects_draft_and_disk_atomically",!session->setRow("a",rejectedRow,error)&&session->rows()[0].nativePronunciationEdges==stable.nativePronunciationEdges&&juce::SHA256(manifest).toHexString()==diskBefore);
    check("save_and_cold_reopen_profile",session->save(error));auto cold=MaterialAnnotationSession::open(audio,manifest,error,"edit-audio");
    check("cold_profile_exact",cold&&cold->rows()[0].nativePronunciationEdges==session->rows()[0].nativePronunciationEdges);
    check("source_PCM_bytes_unchanged",juce::SHA256(audio).toHexString()==audioSha);
    check("source_session_does_not_mutate_project_tuning_or_clocks",untouchedProject.contentFingerprint()==musicFingerprint);
    return root.getChildFile("native-edge-edit.txt").replaceWithText(log)&&ok;
}
}
