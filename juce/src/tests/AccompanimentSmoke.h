#pragma once
#include "../MainComponent.h"
#include "../backend/McpServer.h"
#include <iostream>
namespace hachi
{
inline bool MainComponent::diagnosticAccompaniment(const juce::File& folder)
{
    folder.createDirectory();stopTimer();setSize(1400,860);
    bool ok=true;juce::Array<juce::var> checks;
    const auto check=[&](const juce::String& name,bool passed)
    { auto* row=new juce::DynamicObject();row->setProperty("name",name);row->setProperty("passed",passed);
      checks.add(row);ok=ok && passed;std::cout<<name<<"="<<passed<<std::endl; };
    const auto source=folder.getChildFile("stereo-backing.wav");
    juce::AudioBuffer<float> original(2,48000);
    for(int i=0;i<48000;++i)
    {
        original.setSample(0,i,.2f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*440*i/48000)));
        original.setSample(1,i,.15f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*880*i/48000)));
    }
    {
        auto stream=source.createOutputStream();stream->setPosition(0);stream->truncate();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,2,32,{},0));
        check("source_written",writer && writer->writeFromAudioSampleBuffer(original,0,48000));
    }
    SampleRegionSetting region;region.name="sidecar-note";region.regionEndSeconds=1;
    juce::String error;
    check("source_sidecar_written",SampleSettings::save(source,{region},error));
    ProjectModel model;const auto voice=model.addTrack("voice");model.setTrackPitchAlgorithm(voice,PitchAlgorithm::utau);
    const auto backing=model.addTrack("backing",false,false,true);
    const auto get=[&] { for(const auto& t:model.snapshot().tracks) if(t.id==backing)return t;return TrackData{}; };
    check("independent_track_type",get().accompaniment && !get().compose && !get().referenceOnly);
    ProjectModel inheritance;
    const auto inheritedVoice=inheritance.addTrack("voice");
    inheritance.setTrackPitchAlgorithm(inheritedVoice,PitchAlgorithm::world);
    (void)inheritance.addTrack("backing",false,false,true);
    (void)inheritance.addTrack("new voice");
    check("new_voice_inherits_vocal_engine_across_backing",inheritance.snapshot().tracks.back().pitchAlgorithm==PitchAlgorithm::world);
    const auto originalAlgorithm=get().pitchAlgorithm;
    const auto revision=model.revisionNumber();model.setTrackCompose(backing,true);
    check("compose_cannot_enable_tuning",!get().compose && model.revisionNumber()==revision);
    model.setTrackPitchAlgorithm(backing,PitchAlgorithm::world);model.setPitchAlgorithm(PitchAlgorithm::utau);
    check("individual_and_global_engines_cannot_change_backing",get().pitchAlgorithm==originalAlgorithm);
    model.setTrackReferenceOnly(backing,true);check("not_a_selection_only_reference",!get().referenceOnly);
    const auto clipId=model.addAudioFile(source,1,0,backing);
    check("import_keeps_waveform_without_note_conversion",get().clips.size()==1 && get().clips.front().notes.empty());
    NoteData note;note.id="should-not-analyse";note.durationSeconds=1;note.midiNote=60;
    check("late_analysis_cannot_add_notes",!model.setClipNotesIfEmpty(clipId,{note}));
    ProjectModel regular;(void)regular.addAudioFile(source,1);
    check("ordinary_sidecar_import_still_works",!regular.snapshot().tracks.front().clips.front().notes.empty());
    const auto file=folder.getChildFile("backing.hjpx");check("project_saved",model.save(file,error));
    ProjectModel reopened;check("project_reopened",reopened.load(file,error));
    auto reopenedTrack=reopened.snapshot().tracks.back();
    check("backing_type_and_audio_persist",reopenedTrack.accompaniment && !reopenedTrack.compose
        && !reopenedTrack.referenceOnly && reopenedTrack.clips.front().sourceFile==source);
    ProjectModel undoModel;const auto undoId=undoModel.addTrack("undo backing",false,false,true);
    undoModel.undo();check("track_creation_undo",undoModel.snapshot().tracks.empty());undoModel.redo();
    check("track_creation_redo",undoModel.snapshot().tracks.size()==1 && undoModel.snapshot().tracks.front().id==undoId
        && undoModel.snapshot().tracks.front().accompaniment);

    const auto verifyAudio=[&](const juce::String& name,ProjectData data,int count)
    {
        audio.setAuditionTrack(voice);audio.syncProject(data);
        check(name+"_no_synthesis_jobs",!audio.renderProgress() && audio.activeRenderBackends().isEmpty() && audio.activeRenderWarnings().isEmpty());
        const auto output=folder.getChildFile(name+".wav");
        check(name+"_exports",audio.exportWav(output,error,backing,0,0,{48000,2,32}));
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(output.createInputStream().release(),true));
        check(name+"_correct_length",reader && reader->lengthInSamples==count && reader->numChannels==2);
        if(reader && reader->lengthInSamples==count)
        {
            juce::AudioBuffer<float> actual(2,count);reader->read(&actual,0,count,0,true,true);
            float difference=0;
            for(int c=0;c<2;++c) for(int i=240;i<count-240;++i)
                difference=std::max(difference,std::abs(actual.getSample(c,i)-original.getSample(c,i)));
            check(name+"_preserves_original_samples_and_pitch",difference<1e-6f);
        }
    };
    verifyAudio("backing-original",model.snapshot(),48000);
    auto altered=model.snapshot();auto& raw=altered.tracks.back();raw.compose=true;raw.pitchAlgorithm=PitchAlgorithm::utau;
    raw.utauMode=UtauMode::mou;raw.utauGlobalFlags="Mb100g100";raw.clips.front().notes={note};
    raw.voicebankDirectory=folder.getChildFile("fake-ds");raw.voicebankDirectory.createDirectory();
    raw.voicebankDirectory.getChildFile("dsconfig.yaml").replaceWithText("name: test\n");
    check("backing_never_identified_as_ds_or_flag_track",!trackIsDiffSinger(raw) && !trackTakesFlagCurves(raw));
    verifyAudio("backing-ignores-stale-engine-data",altered,48000);
    auto warningData=altered;warningData.tracks.back().pitchAlgorithm=PitchAlgorithm::world;
    check("backing_has_no_irrelevant_tuning_warning",AudioEngine::renderCapabilityWarnings(warningData).isEmpty());
    raw.clips.front().durationSeconds=.5;
    verifyAudio("backing-trim-does-not-transpose",altered,24000);
    check("separated_export_disabled_for_backing",AudioEngine::componentExportIssue(model.snapshot(),{}).isNotEmpty());

    ProjectModel mcpModel;backend::McpServer mcp(mcpModel,nullptr);
    const auto result=mcp.executeTool("add_track",juce::JSON::parse(R"({"name":"MCP backing","accompaniment":true})"));
    check("mcp_can_create_backing",!static_cast<bool>(result["isError"]) && mcpModel.snapshot().tracks.front().accompaniment
        && !mcpModel.snapshot().tracks.front().compose);
    const auto json=mcp.snapshotJson();const auto jt=json["tracks"][0];
    check("mcp_reports_no_tuning_engine",static_cast<bool>(jt["accompaniment"]) && jt["pitch_algorithm"].toString()=="none");
    auto* args=new juce::DynamicObject();args->setProperty("track_id",mcpModel.snapshot().tracks.front().id);args->setProperty("pitch_algorithm","utau");
    check("mcp_rejects_tuning_on_backing",static_cast<bool>(mcp.executeTool("set_track",juce::var(args))["isError"]));

    project.replace(ProjectData{});project.dispatchPendingMessages();stopTimer();
    trackAreaMenuItemChosen(accompanimentTrackMenuItem);project.dispatchPendingMessages();stopTimer();
    const auto uiId=selectedTrackId;
    check("context_menu_creates_selected_backing",selectedTrackIsAccompaniment() && !uiId.isEmpty());
    const auto menu=diagnosticTrackAreaMenu();check("track_area_menu_contains_backing",std::any_of(menu.begin(),menu.end(),[](const auto& item){return item.id==accompanimentTrackMenuItem;}));
    auto trackMenu=getMenuForIndex(2,{});bool hasEntry=false,locked=false;
    for(juce::PopupMenu::MenuItemIterator it(trackMenu);it.next();)
    { if(it.getItem().itemID==accompanimentTrackMenuItem)hasEntry=true;if(it.getItem().itemID==31)locked=!it.getItem().isEnabled; }
    check("main_track_menu_exposes_backing_and_locks_compose",hasEntry && locked);
    const auto analysisCount=pendingNativeAnalyses;
    addAnalysedAudioFile(source,1,0,uiId);project.dispatchPendingMessages();stopTimer();
    check("ui_import_skips_pitch_analysis",pendingNativeAnalyses==analysisCount && project.snapshot().tracks.front().clips.front().notes.empty());
    scheduleAnalysis(source,project.snapshot().tracks.front().clips.front().id);
    check("explicit_analysis_entry_is_guarded",pendingNativeAnalyses==analysisCount);
    check("tuning_controls_disabled",!pitchAlgorithm.isEnabled() && !stretchAlgorithm.isEnabled() && !renderOrder.isEnabled()
        && !pianoRoll.isEnabled() && pitchAlgorithm.getText()==strings.text("track.originalAudio"));
    resized();if(auto stream=folder.getChildFile("accompaniment.png").createOutputStream())
        juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream);
    const auto normal=project.addTrack("normal");project.dispatchPendingMessages();diagnosticSelectTrack(normal);stopTimer();
    check("switching_to_normal_restores_controls",pitchAlgorithm.isEnabled() && pianoRoll.isEnabled());
    menuItemSelected(accompanimentTrackMenuItem,2);project.dispatchPendingMessages();stopTimer();
    check("main_menu_creates_backing",selectedTrackIsAccompaniment());
    auto* report=new juce::DynamicObject();report->setProperty("passed",ok);report->setProperty("checks",checks);
    folder.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(report),true));return ok;
}
}
