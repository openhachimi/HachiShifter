#pragma once
#include "../OtoWaveformEditorComponent.h"
#include "../MaterialAnnotationSession.h"
#include "../OttoMaterialIO.h"
#include "../ProjectModel.h"
#include "../Theme.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
namespace hachi
{
inline bool runNativeEdgeProfileGuiSmoke(const juce::File& output)
{
    using Scope=MaterialAnnotationSession::EditScope;
    const auto root=output.getChildFile("edge-profile-gui-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";};
    const auto source=root.getChildFile("source.wav"),manifest=root.getChildFile("otto.ini");
    {juce::AudioBuffer<float> pcm(1,48000);pcm.clear();auto stream=source.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(stream?format.createWriterFor(stream.release(),48000,1,16,{},0):nullptr);check("physical_source_fixture",writer&&writer->writeFromAudioSampleBuffer(pcm,0,48000));}
    SampleRegionSetting row;row.name="author";row.materialAudioId="variant";row.materialRegionId="row";row.regionStartSeconds=.1;row.regionEndSeconds=.8;row.alignmentSeconds=.2;row.fixedDurationSeconds=.3;row.overlapSeconds=.04;
    auto other=row;other.name="other";other.materialRegionId="other";other.regionStartSeconds=other.alignmentSeconds=.8;other.regionEndSeconds=1;other.fixedDurationSeconds=0;other.overlapSeconds=0;
    OttoMaterialIO::Document document;document.audio={{row.materialAudioId,source.getFileName(),{row,other}}};check("legacy_profile_absent_fixture",OttoMaterialIO::saveDocument(manifest,document,error));
    const auto sourceHash=juce::SHA256(source).toHexString(),initialHash=juce::SHA256(manifest).toHexString();
    I18n strings;strings.setLanguage(I18n::Language::enUS);int published=0;
    auto editor=OtoWaveformEditorComponent::forMaterial(source,manifest,strings,[&](const auto&){++published;},false,&error,row.materialAudioId);
    check("existing_production_editor_overview",editor&&editor->selectMaterialPronunciation(row.materialRegionId,false));if(!editor)return false;
    check("opening_legacy_row_does_not_create_profile",!editor->diagnosticMaterialRows()[0].nativePronunciationEdges&&juce::SHA256(manifest).toHexString()==initialHash&&!editor->diagnosticEdgeFieldState()["enabled"]);
    editor->diagnosticEdgeFields(true,"160","650",false);
    check("pending_edge_fields_guard_before_notifications",editor->hasUnpublishedMaterialEdits()&&!editor->diagnosticMaterialRows()[0].nativePronunciationEdges&&published==0&&juce::SHA256(manifest).toHexString()==initialHash);
    check("dirty_edge_fields_block_auto_annotation_launch",!editor->requestJapaneseAutoAnnotation());
    editor->diagnosticEdgeFields(true,"nan","650");check("nonfinite_source_ms_refused",!editor->diagnosticCommitEdgeFields(error)&&!editor->diagnosticMaterialRows()[0].nativePronunciationEdges);
    editor->diagnosticEdgeFields(true,"50","650");check("out_of_coverage_source_ms_refused",!editor->diagnosticCommitEdgeFields(error));
    editor->diagnosticEdgeFields(true,"160","650");check("explicit_edge_apply_uses_shared_session",editor->diagnosticCommitEdgeFields(error));
    auto rows=editor->diagnosticMaterialRows();check("anchored_v1_edges_do_not_change_P_C_overlap",rows.size()==2&&rows[0].nativePronunciationEdges&&rows[0].nativePronunciationEdges->entryFromSourceSeconds==.1&&rows[0].nativePronunciationEdges->entryToSourceSeconds==.16&&rows[0].nativePronunciationEdges->exitFromSourceSeconds==.65&&rows[0].nativePronunciationEdges->exitToSourceSeconds==.8&&rows[0].alignmentSeconds==.2&&rows[0].fixedDurationSeconds==.3&&rows[0].overlapSeconds==.04);
    editor->diagnosticEdgeHistory(true);check("edge_apply_same_session_undo",!editor->diagnosticMaterialRows()[0].nativePronunciationEdges);editor->diagnosticEdgeHistory(false);check("edge_apply_same_session_redo",editor->diagnosticMaterialRows()[0].nativePronunciationEdges.has_value());
    editor->diagnosticSave();OttoMaterialIO::Document cold;check("source_save_cold_OTTO_profile",published==1&&OttoMaterialIO::loadDocument(manifest,cold,error)&&cold.audio[0].rows[0].nativePronunciationEdges&&cold.audio[0].rows[0].nativePronunciationEdges->entryToSourceSeconds==.16);
    const auto savedHash=juce::SHA256(manifest).toHexString();row=cold.audio[0].rows[0];
    {
        // GUI198: fade ramps + draggable inner ends on the waveform write the
        // same edge fields (0.1 ms display) through the same validation.
        editor->setSize(1100,760);editor->selectMaterialPronunciation(row.materialRegionId,false);editor->diagnosticEdgeFields(true,"160","650");
        std::cout<<"debug_edge_state="<<juce::JSON::toString(editor->diagnosticEdgeFieldState(),true)<<std::endl;
        const auto handle=editor->diagnosticWaveformEdgeHandle(true);check("waveform_shows_draggable_fade_in_handle",handle.has_value()&&editor->diagnosticWaveformEdgeHandle(false).has_value());
        if(handle)editor->diagnosticDragWaveformEdge(true,handle->translated(25.f,0.f));
        const auto text=editor->diagnosticEdgeEntryText();
        check("dragging_fade_handle_edits_entry_field_in_0_1ms",handle&&text!="160"&&text.getDoubleValue()>160.0&&(!text.containsChar('.')||text.fromLastOccurrenceOf(".",false,false).length()==1));
        check("fade_handle_drag_does_not_publish_until_apply",juce::SHA256(manifest).toHexString()==savedHash);
        editor->diagnosticEdgeFields(true,"160","650");
    }
    editor->diagnosticEdgeFields(false,"half typed disabled","not numeric");check("explicit_disable_ignores_disabled_text",editor->diagnosticCommitEdgeFields(error)&&!editor->diagnosticMaterialRows()[0].nativePronunciationEdges);editor.reset();
    auto reopened=OtoWaveformEditorComponent::forMaterial(source,manifest,strings,[](const auto&){},false,&error,row.materialAudioId);check("closing_unsaved_disable_preserves_source_profile",reopened&&reopened->diagnosticMaterialRows()[0].nativePronunciationEdges&&juce::SHA256(manifest).toHexString()==savedHash);
    // Real native ProjectModel local adapter; saving through the same precise
    // GUI changes only the bound note and never the source OTTO publication.
    ProjectData data;data.bpm=137;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;
    ClipData clip;clip.id="owner";clip.sourceFile=source;clip.durationSeconds=clip.sourceDurationSeconds=1;clip.sourceTimeMap={{0,0},{1,1}};
    NoteData note;note.id="bound-note";note.label="keep musical label";note.materialLabelFollowsAlias=false;note.materialBoundAlias=row.name;note.materialAliasFollowsPrimary=true;note.startSeconds=.1;note.durationSeconds=.7;note.midiNote=67;note.materialAudioId=row.materialAudioId;note.materialRegionId=row.materialRegionId;note.materialSourceStartSeconds=note.nativeSourceStartSeconds=.1;note.materialSourceEndSeconds=note.nativeSourceEndSeconds=.8;note.nativeSegments=SampleSettings::nativeSegmentsFor(row);note.nativePronunciationEdges=row.nativePronunciationEdges;note.consonantSeconds=.3;note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=.1;note.utauOverlapOverrideEnabled=true;note.utauOverlapSeconds=.04;clip.notes={note};track.clips={clip};data.tracks={track};ProjectModel model;model.resetDocument(data);
    auto ticket=std::make_shared<ProjectModel::NoteMaterialLocalBinding>();SampleRegionSetting local;
    check("real_model_note_binding",model.getNoteMaterialLocalBinding(note.id,*ticket,local,error)&&ticket->nativeFullRow);
    check("one_ULP_user_duration_projects_exact_stored_profile_anchors",local.regionStartSeconds==row.regionStartSeconds&&local.regionEndSeconds==row.regionEndSeconds&&local.nativePronunciationEdges==row.nativePronunciationEdges);
    auto derivedData=data;derivedData.tracks[0].clips[0].notes[0].startSeconds=row.regionStartSeconds;derivedData.tracks[0].clips[0].notes[0].durationSeconds=row.regionEndSeconds-row.regionStartSeconds;
    ProjectModel derivedModel;derivedModel.resetDocument(derivedData);ProjectModel::NoteMaterialLocalBinding derivedTicket;SampleRegionSetting derivedRow;
    check("derived_duration_projects_same_stored_profile_anchors",derivedModel.getNoteMaterialLocalBinding(note.id,derivedTicket,derivedRow,error)&&derivedTicket.nativeFullRow&&derivedRow.regionStartSeconds==local.regionStartSeconds&&derivedRow.regionEndSeconds==local.regionEndSeconds&&derivedRow.nativePronunciationEdges==local.nativePronunciationEdges);
    MaterialAnnotationSession::NoteScopeBinding binding;binding.noteId=ticket->noteId;binding.audioId=ticket->audioId;binding.regionId=ticket->regionId;binding.sourceAudio=ticket->sourceFile;binding.sourceFingerprint=ticket->sourceFingerprint;binding.projectRevision=ticket->projectRevision;binding.localRow=local;
    binding.apply=[&model,&log,ticket](const auto& context,const auto& changed,juce::uint64& next,juce::String& problem)
    {if(context.projectRevision!=ticket->projectRevision){problem="stale model ticket";log+="native_model_apply_error="+problem+"\n";return false;}std::uint64_t revision=0;if(!model.applyNoteMaterialLocalRow(*ticket,changed,revision,problem)){log+="native_model_apply_error="+problem+"\n";return false;}ticket->projectRevision=revision;next=revision;return true;};
    // Keep the legitimate independent .1/.7 user-duration case in the actual
    // GUI route: the profile-present projection must stabilize its known anchors.
    log+="projected_local_coverage="+juce::String(local.regionStartSeconds,17)+","+juce::String(local.regionEndSeconds,17)+"\n";
    const bool localEntered=reopened&&reopened->enableNoteScope(binding,Scope::thisNote,error);
    if(!localEntered)log+="native_note_destination_error="+error+"\n";
    check("same_editor_native_note_destination",localEntered&&reopened->diagnosticEdgeControlsEnabled());
    if(reopened){reopened->diagnosticEdgeFields(true,"180","600");const bool committed=reopened->diagnosticCommitEdgeFields(error);if(!committed)log+="native_note_edge_apply_error="+error+"\n";check("native_note_edge_apply",committed);reopened->diagnosticSave();}
    const auto actual=model.snapshot();check("native_note_save_no_source_OTTO_write",actual.tracks[0].clips[0].notes[0].nativePronunciationEdges&&actual.tracks[0].clips[0].notes[0].nativePronunciationEdges->entryToSourceSeconds==.18&&actual.tracks[0].clips[0].notes[0].materialLocalOverride&&juce::SHA256(manifest).toHexString()==savedHash&&actual.bpm==137&&actual.tracks[0].clips[0].notes[0].label==note.label&&actual.tracks[0].clips[0].notes[0].midiNote==note.midiNote);
    // Real canonical classic OTO ticket/model: the native edge controls are
    // disabled and an attempted disabled-field edit never mutates its note.
    const auto classicRoot=root.getChildFile("classic");classicRoot.createDirectory();const auto classicAudio=classicRoot.getChildFile("source.wav"),classicOto=classicRoot.getChildFile("oto.ini");
    check("classic_canonical_fixture",source.copyFileTo(classicAudio)&&classicOto.replaceWithText("source.wav=ka,0,100,-800,80,20\n"));
    ProjectData classicData;TrackData classicTrack;classicTrack.id="utau";classicTrack.trackType=TrackType::utau;classicTrack.compose=true;classicTrack.pitchAlgorithm=PitchAlgorithm::utau;classicTrack.utauMode=UtauMode::classic;classicTrack.voicebankDirectory=classicRoot;
    ClipData classicClip;classicClip.id="classic-clip";classicClip.durationSeconds=1;NoteData classicNote;classicNote.id="classic-note";classicNote.label="ka";classicNote.durationSeconds=.5;classicClip.notes={classicNote};classicTrack.clips={classicClip};classicData.tracks={classicTrack};ProjectModel classicModel;classicModel.resetDocument(classicData);
    const auto classicBefore=classicModel.contentFingerprint();const auto classicHash=juce::SHA256(classicOto).toHexString();const auto sample=backend::UtauRenderer::resolveVoiceSample(classicRoot,"ka",60,100,false,false);
    auto scalar=OtoWaveformEditorComponent::forClassicNote(classicModel,classicNote.id,strings,sample,classicModel.revisionNumber(),error,Scope::thisNote);
    check("classic_scalar_edge_controls_disabled",sample.found&&scalar&&scalar->diagnosticClassicScalarScope()&&!scalar->diagnosticEdgeControlsEnabled());
    if(scalar){scalar->diagnosticEdgeFields(true,"200","700");check("classic_scalar_attempt_does_not_mutate_profile",scalar->diagnosticCommitEdgeFields(error)&&!scalar->diagnosticMaterialRows()[0].nativePronunciationEdges&&classicModel.contentFingerprint()==classicBefore&&juce::SHA256(classicOto).toHexString()==classicHash&&!classicRoot.getChildFile("otto.ini").existsAsFile());}
    struct Window:juce::DocumentWindow{Window():DocumentWindow("Native edge GUI",Palette::panel,DocumentWindow::closeButton){setUsingNativeTitleBar(true);}void closeButtonPressed()override{setVisible(false);}};
    int languageIndex=0;for(const auto language:{I18n::Language::zhCN,I18n::Language::zhTW,I18n::Language::jaJP,I18n::Language::koKR,I18n::Language::enUS})
    {
        I18n localized;localized.setLanguage(language);bool catalog=true;for(const auto* key:{"edge.enabled","edge.entry","edge.exit","edge.summary","edge.separate","edge.sourceMs","edge.absent","edge.invalid"})catalog&=localized.text(key).isNotEmpty()&&localized.text(key)!=key;check("five_language_edge_catalog",catalog);
        auto view=OtoWaveformEditorComponent::forMaterial(source,manifest,localized,[](const auto&){},false,&error,row.materialAudioId);if(!view){check("localized_editor",false);continue;}view->selectMaterialPronunciation(row.materialRegionId,false);Window window;window.setContentNonOwned(view.get(),true);window.centreWithSize(1280,1000);window.setVisible(true);view->setSize(1280,1000);view->resized();
        juce::Image image(juce::Image::ARGB,view->getWidth(),view->getHeight(),true,juce::SoftwareImageType{});{juce::Graphics graphics(image);view->paintEntireComponent(graphics,true);}auto png=root.getChildFile("edge-overview-"+juce::String(languageIndex++)+".png").createOutputStream();juce::PNGImageFormat format;check("actual_GUI_software_PNG_flushed",png&&format.writeImageToStream(image,*png));
        if(language==I18n::Language::enUS){view->selectMaterialPronunciation(row.materialRegionId,true);view->resized();juce::Image detail(juce::Image::ARGB,view->getWidth(),view->getHeight(),true,juce::SoftwareImageType{});{juce::Graphics graphics(detail);view->paintEntireComponent(graphics,true);}auto out=root.getChildFile("edge-detail.png").createOutputStream();check("actual_precise_GUI_software_PNG_flushed",out&&format.writeImageToStream(detail,*out));}
        window.setVisible(false);
    }
    check("all_GUI_actions_leave_source_PCM_unchanged",juce::SHA256(source).toHexString()==sourceHash);
    const auto written=root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"native_edge_profile_gui="<<ok<<"|fixture="<<root.getFullPathName()<<std::endl;if(!ok)std::cerr<<log<<std::endl;return ok&&written;
}
}
