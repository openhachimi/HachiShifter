#pragma once
#include "../NativeSourceEdgePlan.h"
#include "../OttoMaterialIO.h"
#include "../SampleSettings.h"
#include <iostream>

namespace hachi
{
// Pure planning/metadata tests only. No claim of acoustic or old-PCM equality.
inline bool runNativeSourceEdgePlanSmoke(const juce::File& output)
{
    bool ok=true;juce::String log,error;
    const auto check=[&](const char* key,bool value)
    {
        ok&=value;log+=juce::String(key)+"="+(value?"PASS":"FAIL")+"\n";std::cout<<key<<"="<<value<<std::endl;
        if(!value&&error.isNotEmpty()){log+="error="+error+"\n";std::cout<<"error="<<error<<std::endl;}
    };
    TrackData track;track.id="native-edge-plan";track.trackType=TrackType::hachi;
    ClipData clip;clip.id="whole-recording";clip.sourceFile=output.getChildFile("actual-source.wav");
    clip.startSeconds=10;clip.durationSeconds=clip.sourceDurationSeconds=3;clip.sourceTimeMap={{0,0},{3,3}};
    for(int i=0;i<3;++i)
    {
        NoteData note;note.id=juce::String::charToString(char('a'+i));note.startSeconds=i;note.durationSeconds=1;
        note.materialAudioId="registration";note.materialRegionId=note.id;
        note.materialSourceStartSeconds=note.nativeSourceStartSeconds=i;
        note.materialSourceEndSeconds=note.nativeSourceEndSeconds=i+1;
        if(i<2)note.materialConnectedNextRegionId=juce::String::charToString(char('b'+i));
        NativePronunciationEdges profile;profile.entryFromSourceSeconds=i;profile.entryToSourceSeconds=i+.125;
        profile.exitFromSourceSeconds=i+.75;profile.exitToSourceSeconds=profile.terminationSourceSeconds=i+1;
        profile.provenance="fixture-physical-anchors";note.nativePronunciationEdges=profile;clip.notes.push_back(note);
    }
    track.clips={clip};NativeSourceEdgePlan plan;
    check("whole_abc_one_explicit_component",resolveNativeSourceEdgePlan(track,{},plan,error)&&plan.components.size()==1&&plan.members.size()==3);
    check("whole_only_a_entry_c_exit",plan.members.size()==3&&plan.members[0].exposedEntry&&!plan.members[0].exposedExit
        &&!plan.members[1].exposedEntry&&!plan.members[1].exposedExit&&!plan.members[2].exposedEntry&&plan.members[2].exposedExit
        &&plan.members[0].entryFromTargetSeconds==10&&plan.members[2].terminationTargetSeconds==13);
    check("subset_ab_recomputes_exposed_b_exit",resolveNativeSourceEdgePlan(track,{"a","b"},plan,error)&&plan.components.size()==1
        &&plan.members.back().noteId=="b"&&plan.members.back().exposedExit&&plan.members.back().exitFromTargetSeconds==11.75);
    check("single_b_exposes_both_sides",resolveNativeSourceEdgePlan(track,{"b"},plan,error)&&plan.members.size()==1
        &&plan.members[0].exposedEntry&&plan.members[0].exposedExit&&plan.members[0].entryFromTargetSeconds==11&&plan.members[0].terminationTargetSeconds==12);
    auto unlinked=track;for(auto& n:unlinked.clips[0].notes)n.materialConnectedNextRegionId.clear();
    check("touching_rows_do_not_manufacture_source_links",resolveNativeSourceEdgePlan(unlinked,{},plan,error)&&plan.components.size()==3);
    auto legacy=track;for(auto& n:legacy.clips[0].notes)n.nativePronunciationEdges.reset();
    check("legacy_absent_profile_is_inert",resolveNativeSourceEdgePlan(legacy,{},plan,error)&&plan.members.empty()&&plan.components.empty());
    check("unknown_selected_identity_rejected",!resolveNativeSourceEdgePlan(track,{"missing"},plan,error)&&plan.members.empty());
    auto bad=track;bad.clips[0].notes[0].nativePronunciationEdges->terminationSourceSeconds=1.5;
    check("invalid_termination_rejected_atomically",!resolveNativeSourceEdgePlan(bad,{},plan,error)&&plan.members.empty());
    auto multi=track;multi.clips.clear();
    for(int i=0;i<3;++i)
    {
        auto c=clip;c.id="owner"+juce::String(i);c.startSeconds=10+i;c.sourceOffsetSeconds=i;c.durationSeconds=c.sourceDurationSeconds=1;c.sourceTimeMap={{0,0},{1,1}};
        c.notes={clip.notes[static_cast<std::size_t>(i)]};c.notes[0].startSeconds=0;
        c.sourceFile=output.getChildFile(juce::String(i==0?"so_re":i==1?"e_g":"ga")+".wav");
        if(i==1)c.notes[0].pronunciationKind=PronunciationKind::transition;multi.clips.push_back(std::move(c));
    }
    check("real_transition_different_sources_stays_three_components",resolveNativeSourceEdgePlan(multi,{},plan,error)&&plan.components.size()==3);
    NativePronunciationEdges decoded;const auto value=nativePronunciationEdgesValue(*clip.notes[1].nativePronunciationEdges);
    check("shared_profile_codec_exact_roundtrip",nativePronunciationEdgesFromValue(value,decoded,error)&&decoded==*clip.notes[1].nativePronunciationEdges);
    auto invalidValue=value;invalidValue.getArray()->set(0,2);
    check("unknown_profile_version_rejected",!nativePronunciationEdgesFromValue(invalidValue,decoded,error));
    SampleRegionSetting row;row.name="b";row.materialAudioId="registration";row.materialRegionId="b";row.regionStartSeconds=1;row.regionEndSeconds=2;row.alignmentSeconds=1;
    OttoMaterialIO::Document document;document.audio={{"registration","actual-source.wav",{row}}};juce::String oldText,newText;
    error.clear();check("legacy_otto_encodes_without_edge_property",OttoMaterialIO::encode(document,oldText,error));
    document.audio[0].rows[0].nativePronunciationEdges=clip.notes[1].nativePronunciationEdges;
    OttoMaterialIO::Document restored;
    error.clear();check("optin_otto_exact_profile_roundtrip",OttoMaterialIO::encode(document,newText,error)&&OttoMaterialIO::decode(newText,restored,error)
        &&restored.audio[0].rows[0].nativePronunciationEdges==clip.notes[1].nativePronunciationEdges);
    document.audio[0].rows[0].nativePronunciationEdges.reset();juce::String again;
    error.clear();check("unset_profile_keeps_old_otto_shape",OttoMaterialIO::encode(document,again,error)&&again==oldText);
    ProjectModel model;ProjectData data;data.tracks={track};model.resetDocument(data);
    const auto saved=output.getChildFile("native-edges.hjpx");ProjectModel reopened;
    check("hjpx_optional_profile_exact_roundtrip",model.save(saved,error)&&reopened.load(saved,error)
        &&reopened.snapshot().tracks[0].clips[0].notes[1].nativePronunciationEdges==clip.notes[1].nativePronunciationEdges);
    return output.getChildFile("native-source-edge-plan.txt").replaceWithText(log)&&ok;
}
}
