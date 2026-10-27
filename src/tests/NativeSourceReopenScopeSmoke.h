#pragma once
#include "../MainComponent.h"
#include "../OtoWaveformEditorComponent.h"
#include "../OttoMaterialIO.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
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
// The normal, durable manager factory is exercised AFTER window destruction.
// No ASR retarget callback setter is used in this closure regression.
inline bool runNativeSourceReopenScopeSmoke(MainComponent& main,const juce::File& output)
{
    const auto root=output.getChildFile("source-reopen-scope-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";};
    const auto finish=[&]{std::cout<<"native_source_reopen_scope="<<ok<<"|fixture="<<root.getFullPathName()<<std::endl;if(!ok)std::cerr<<log<<std::endl;return root.getChildFile("verification.txt").replaceWithText(log)&&ok;};
#if !JUCE_WINDOWS
    check("actual_native_message_pump_available",false);return finish();
#else
    const auto pump=[&]{MSG message{};for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i){if(message.message==WM_QUIT){PostQuitMessage(int(message.wParam));return false;}TranslateMessage(&message);DispatchMessageW(&message);}main.diagnosticProject().dispatchPendingMessages();return true;};
    const auto wait=[&](const std::function<bool()>& ready){const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;while(!ready()&&juce::Time::getMillisecondCounterHiRes()<deadline){if(!pump())return false;juce::Thread::sleep(2);}pump();return ready();};
    const auto find=[&](const auto& self,juce::Component* component,const juce::String& id)->juce::Component*{if(!component)return nullptr;if(component->getComponentID()==id)return component;for(int i=0;i<component->getNumChildComponents();++i)if(auto* found=self(self,component->getChildComponent(i),id))return found;return nullptr;};
    const auto source=root.getChildFile("source.wav"),manifest=root.getChildFile("otto.ini");
    {juce::AudioBuffer<float> pcm(1,96000);pcm.clear();auto stream=source.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),48000,1,16,{},0):nullptr);check("physical_source_fixture",writer&&writer->writeFromAudioSampleBuffer(pcm,0,96000));}
    SampleRegionSetting a;a.materialAudioId="old-author-variant";a.materialRegionId="old-row";a.name="old-author";a.regionStartSeconds=a.alignmentSeconds=0;a.regionEndSeconds=1;a.role=NativeSegmentRole::vowel;a.nativeUnpitched=true;
    auto b=a;b.materialAudioId="generated-native-variant";b.materialRegionId="new-row";b.name="generated";b.regionStartSeconds=b.alignmentSeconds=.5;b.regionEndSeconds=1.5;
    OttoMaterialIO::Document document;document.audio={{a.materialAudioId,source.getFileName(),{a}},{b.materialAudioId,source.getFileName(),{b}}};check("two_exact_native_registration_fixture",OttoMaterialIO::saveDocument(manifest,document,error));
    const auto sourceHash=juce::SHA256(source).toHexString();ProjectData data;data.bpm=137;data.assetManagerVisible=true;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;
    ClipData clip;clip.id="owner";clip.sourceFile=source;clip.durationSeconds=clip.sourceDurationSeconds=2;clip.sourceTimeMap={{0,0},{2,2}};
    const auto noteFor=[&](const SampleRegionSetting& row,const juce::String& id){NoteData note;note.id=id;note.label=note.materialBoundAlias=row.name;note.materialLabelFollowsAlias=note.materialAliasFollowsPrimary=true;note.startSeconds=row.regionStartSeconds;note.durationSeconds=row.regionEndSeconds-row.regionStartSeconds;note.materialAudioId=row.materialAudioId;note.materialRegionId=row.materialRegionId;note.materialSourceStartSeconds=note.nativeSourceStartSeconds=row.regionStartSeconds;note.materialSourceEndSeconds=note.nativeSourceEndSeconds=row.regionEndSeconds;note.nativeSegments=SampleSettings::nativeSegmentsFor(row);note.nativeRole=row.role;note.nativeProvenance=row.provenance;note.nativeConfidence=row.confidence;note.nativeUnpitched=true;note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=0;note.utauOverlapOverrideEnabled=true;note.utauOverlapSeconds=0;return note;};
    auto old=noteFor(a,"old-bound"),used=noteFor(b,"new-bound"),bare=noteFor(b,"bare");bare.materialAudioId.clear();bare.materialRegionId.clear();bare.materialBoundAlias.clear();bare.label="bare-keep";clip.notes={old,used,bare};track.clips={clip};data.tracks={track};main.diagnosticProject().resetDocument(data);pump();
    AssetManagerComponent* manager=nullptr;for(int i=0;i<main.getNumChildComponents();++i)if(auto* candidate=dynamic_cast<AssetManagerComponent*>(main.getChildComponent(i)))manager=candidate;
    check("real_Main_manager_callbacks",manager&&bool(manager->onMaterialPreflight)&&bool(manager->onMaterialSaved));if(!manager)return finish();check("real_root_registration",manager->registerMaterialRoot(root,error));
    const auto visibleEditor=[&]()->OtoWaveformEditorComponent*{for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)if(auto* window=dynamic_cast<juce::DialogWindow*>(juce::Desktop::getInstance().getComponent(i));window&&window->isVisible())if(auto* editor=dynamic_cast<OtoWaveformEditorComponent*>(window->getContentComponent());editor&&editor->materialSourceAudio()==source&&editor->materialAudioVariantId()==b.materialAudioId)return editor;return nullptr;};
    manager->editMaterial(source,{},b.materialAudioId);juce::Component::SafePointer<OtoWaveformEditorComponent> destroyed=visibleEditor();check("ordinary_manager_factory_exact_variant",bool(destroyed));if(!destroyed)return finish();
    if(auto* window=destroyed->findParentComponentOfClass<juce::DialogWindow>())window->closeButtonPressed();pump();manager->editMaterial(source,{},b.materialAudioId);
    auto* editor=visibleEditor();check("true_close_reopen_creates_fresh_normal_session",destroyed==nullptr&&editor);if(!editor)return finish();check("reopened_overview_explicit_row",editor->selectMaterialPronunciation(b.materialRegionId,false));
    const auto before=main.diagnosticProject().snapshot();auto foreignBefore=before;std::erase_if(foreignBefore.tracks[0].clips[0].notes,[](const auto& note){return note.id=="new-bound";});ProjectModel foreignModel;foreignModel.resetDocument(foreignBefore);const auto expectedForeign=foreignModel.contentFingerprint();
    auto* alias=dynamic_cast<juce::TextEditor*>(find(find,editor,"material.overview.alias"));auto* save=dynamic_cast<juce::Button*>(find(find,editor,"material.overview.save"));check("ordinary_human_source_controls",alias&&save&&save->isEnabled());if(!alias||!save)return finish();alias->setText("reopened-used-save",true);save->triggerClick();
    const auto coldHasAlias=[&]{OttoMaterialIO::Document cold;juce::String problem;if(!OttoMaterialIO::loadDocument(manifest,cold,problem))return false;for(const auto& section:cold.audio)if(section.id==b.materialAudioId)for(const auto& row:section.rows)if(row.materialRegionId==b.materialRegionId&&row.name=="reopened-used-save")return true;return false;};
    check("actual_reopened_Source_Save_cold_OTTO",wait(coldHasAlias));auto after=main.diagnosticProject().snapshot();const auto selected=std::find_if(after.tracks[0].clips[0].notes.begin(),after.tracks[0].clips[0].notes.end(),[](const auto& note){return note.id=="new-bound";});check("ordinary_factory_Save_updates_used_exact_variant",selected!=after.tracks[0].clips[0].notes.end()&&selected->label=="reopened-used-save");
    std::erase_if(after.tracks[0].clips[0].notes,[](const auto& note){return note.id=="new-bound";});ProjectModel foreignAfter;foreignAfter.resetDocument(after);check("ordinary_factory_Save_preserves_all_foreign_and_bare_data",foreignAfter.contentFingerprint()==expectedForeign);
    check("delete_all_keeps_authoritative_session_audio_ID",editor->diagnosticRemoveMaterialAlias(b.materialRegionId,"reopened-used-save",error)&&editor->diagnosticMaterialRows().empty()&&editor->materialAudioVariantId()==b.materialAudioId);
    save=dynamic_cast<juce::Button*>(find(find,editor,"material.overview.save"));if(save)save->triggerClick();
    const auto coldEmpty=[&]{OttoMaterialIO::Document cold;juce::String problem;if(!OttoMaterialIO::loadDocument(manifest,cold,problem))return false;for(const auto& section:cold.audio)if(section.id==b.materialAudioId)return section.rows.empty();return false;};
    check("actual_empty_rows_Save_retains_exact_empty_registration",save&&wait(coldEmpty));after=main.diagnosticProject().snapshot();std::erase_if(after.tracks[0].clips[0].notes,[](const auto& note){return note.id=="new-bound";});foreignAfter.resetDocument(after);check("empty_rows_callback_scope_never_marks_foreign_or_binds_bare",foreignAfter.contentFingerprint()==expectedForeign);
    for(int i=0;i<4;++i){auto* modal=juce::Component::getCurrentlyModalComponent();if(!modal)break;modal->exitModalState(0);pump();}
    check("source_PCM_never_modified_by_reopened_Save",juce::SHA256(source).toHexString()==sourceHash);return finish();
#endif
}
}
