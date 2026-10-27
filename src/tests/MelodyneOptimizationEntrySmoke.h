#pragma once
#include "../MelodyneImportOptimization.h"
#include "../backend/McpServer.h"
#include <iostream>
namespace hachi
{
inline bool runMelodyneOptimizationEntrySmoke(const juce::File& folder,const juce::File& realMpd={})
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    MelodyneImportOptimizationOptions defaults;juce::var empty(new juce::DynamicObject);
    check("headless_audio_optimization_options_all_default_off",readMelodyneImportOptimizationOptions(empty,defaults,error)&&!defaults.mergeConsonants&&!defaults.virtualConnections&&!defaults.virtualPitch&&defaults.registerTransitions);
    juce::var invalid(new juce::DynamicObject);invalid.getDynamicObject()->setProperty("auto_virtual_connections","false");MelodyneImportOptimizationOptions unchanged;unchanged.mergeConsonants=true;
    check("string_false_is_rejected_not_cast_to_true",!readMelodyneImportOptimizationOptions(invalid,unchanged,error)&&unchanged.mergeConsonants&&!unchanged.virtualConnections);
    juce::var enabled(new juce::DynamicObject);enabled.getDynamicObject()->setProperty("auto_merge_consonants",true);enabled.getDynamicObject()->setProperty("auto_virtual_connections",true);enabled.getDynamicObject()->setProperty("auto_virtual_pitch",true);
    MelodyneImportOptimizationOptions all;check("all_three_audio_options_are_independent_explicit_booleans",readMelodyneImportOptimizationOptions(enabled,all,error)&&all.mergeConsonants&&all.virtualConnections&&all.virtualPitch);
    const auto tools=backend::McpServer::diagnosticTools();bool schemas=true;int found=0;
    if(const auto* list=tools.getArray())for(const auto& tool:*list)if(tool.getProperty("name",{}).toString()=="import_melodyne"||tool.getProperty("name",{}).toString()=="project_open")
    {
        ++found;const auto properties=tool.getProperty("inputSchema",{}).getProperty("properties",{});
        for(const char* name:{"auto_merge_consonants","auto_virtual_connections","auto_virtual_pitch"})schemas&=properties.getProperty(name,{}).getProperty("type",{}).toString()=="boolean";
    }
    check("both_actual_MCP_schemas_advertise_the_same_three_booleans",schemas&&found==2);
    ProjectModel model;const auto before=model.contentFingerprint();backend::McpServer server(model,nullptr,{folder.getFullPathName()});
    for(const char* tool:{"project_open","import_melodyne"})
    {
        juce::var args(new juce::DynamicObject);args.getDynamicObject()->setProperty("path",folder.getChildFile("not-read.mpd").getFullPathName());args.getDynamicObject()->setProperty("auto_merge_consonants","true");
        const auto result=server.executeTool(tool,args);
        check(juce::String(tool)=="project_open"?"project_open_rejects_bad_options_without_changing_the_document":"import_melodyne_rejects_bad_options_without_changing_the_document",bool(result.getProperty("isError",false))&&model.contentFingerprint()==before);
    }
    juce::var badRestore(new juce::DynamicObject);badRestore.getDynamicObject()->setProperty("note_id","unknown");badRestore.getDynamicObject()->setProperty("discard_merged_edits","false");
    check("headless_restore_never_casts_string_false_into_discard_confirmation",bool(server.executeTool("restore_merged_consonant",badRestore).getProperty("isError",false))&&model.contentFingerprint()==before);
    const auto snapshot=server.snapshotJson();check("MCP_snapshot_exposes_virtual_groups_and_archive_identity_without_payload",snapshot.hasProperty("native_virtual_groups")&&snapshot.getProperty("native_virtual_groups",{}).isArray()&&snapshot.hasProperty("consonant_merge_archives"));
    const auto source=folder.getChildFile("source.wav");
    {juce::AudioBuffer<float> pcm(1,48000);pcm.clear();auto stream=source.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,48000))return false;}
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;track.pitchAlgorithm=PitchAlgorithm::world;
    ClipData clip;clip.id="owner";clip.sourceFile=source;clip.durationSeconds=clip.sourceDurationSeconds=.5;clip.sourceTimeMap={{0,0},{.5,.5}};NoteData note;note.id="note";note.durationSeconds=.5;note.midiNote=note.sourceMidiCenter=60;for(int f=0;f<=100;++f)note.contour.push_back({f*.005,0,0,true});clip.notes={note};track.clips={clip};data.tracks={track};
    struct RestoreRoots{juce::StringArray roots;~RestoreRoots(){SampleSettings::setMaterialRoots(roots);}} restore{SampleSettings::materialRoots()};SampleSettings::setMaterialRoots({folder.getFullPathName()});
    juce::StringArray warnings;const auto projected=SampleSettings::convertMelodyneProject(data,warnings,true,false);
    check("project_only_conversion_keeps_native_timing_without_publishing_material_files",projected&&warnings.isEmpty()&&!SampleSettings::annotationFor(source).existsAsFile()&&!data.tracks[0].clips[0].notes[0].nativeSegments.empty());
    if(realMpd.existsAsFile())
    {
        juce::var plain(new juce::DynamicObject);plain.getDynamicObject()->setProperty("path",realMpd.getFullPathName());
        const auto opened=server.executeTool("project_open",plain);
        check("actual_MCP_project_open_imports_MPD_with_all_audio_optimizations_off",!bool(opened.getProperty("isError",false))&&!model.snapshot().tracks.empty()&&model.snapshot().nativeVirtualGroups.empty()&&model.snapshot().consonantMerges.empty());
        juce::var tuned(new juce::DynamicObject);tuned.getDynamicObject()->setProperty("path",realMpd.getFullPathName());tuned.getDynamicObject()->setProperty("recursive_media",false);tuned.getDynamicObject()->setProperty("auto_virtual_connections",true);tuned.getDynamicObject()->setProperty("auto_virtual_pitch",true);tuned.getDynamicObject()->setProperty("auto_merge_consonants",true);
        const auto imported=server.executeTool("import_melodyne",tuned);const auto state=model.snapshot();int proofs=0;bool pitch=true;
        for(const auto& group:state.nativeVirtualGroups){proofs+=int(group.extraSplitTransitions.size())+(group.splitTransition?1:0);pitch&=group.automaticPitch;}
        check("actual_MCP_opt_in_uses_common_multi_bridge_and_pitch_commits",!bool(imported.getProperty("isError",false))&&!state.nativeVirtualGroups.empty()&&proofs==3&&pitch);
    }
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
