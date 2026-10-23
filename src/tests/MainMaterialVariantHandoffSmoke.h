#pragma once
#include "../MainComponent.h"
#include "../OttoMaterialIO.h"
#include "../OtoWaveformEditorComponent.h"
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
inline bool MainComponent::diagnosticMaterialVariantHandoffUi(const juce::File& output)
{
    const auto root=output.getChildFile("main-variant-handoff-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    stopTimer();audio.stop();bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";std::cout<<name<<'='<<pass<<std::endl;};
    const auto finish=[&]{const auto written=root.getChildFile("main-variant-handoff-ui.txt").replaceWithText(log);std::cout<<"variant_handoff_fixture="<<root.getFullPathName()<<std::endl;return ok&&written;};
#if !JUCE_WINDOWS
    check("windows_actual_message_pump",false);return finish();
#else
    const auto pump=[&]
    {
        MSG message{};for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i){if(message.message==WM_QUIT){PostQuitMessage(static_cast<int>(message.wParam));return false;}TranslateMessage(&message);DispatchMessageW(&message);}
        project.dispatchPendingMessages();return true;
    };
    const auto wait=[&](const std::function<bool()>& ready,int milliseconds)
    {const auto deadline=juce::Time::getMillisecondCounterHiRes()+milliseconds;while(!ready()&&juce::Time::getMillisecondCounterHiRes()<deadline){if(!pump())return false;juce::Thread::sleep(5);}pump();return ready();};
    const auto source=root.getChildFile("one-physical-audio.wav"),manifest=root.getChildFile("otto.ini");
    juce::AudioBuffer<float> pcm(1,48000);for(int i=0;i<48000;++i)pcm.setSample(0,i,.1f*std::sin(juce::MathConstants<double>::twoPi*220*i/48000.0));
    auto stream=source.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(stream?format.createWriterFor(stream.release(),48000,1,24,{},0):nullptr);
    check("single_physical_source_fixture",writer&&writer->writeFromAudioSampleBuffer(pcm,0,48000));writer.reset();
    SampleRegionSetting a;a.materialAudioId="annotation-A";a.materialRegionId="same-row-id";a.name="A-only";a.regionStartSeconds=a.alignmentSeconds=.1;a.regionEndSeconds=.5;a.role=NativeSegmentRole::vowel;
    auto b=a;b.materialAudioId="annotation-B";b.name="B-only";b.regionStartSeconds=b.alignmentSeconds=.2;b.regionEndSeconds=.8;
    OttoMaterialIO::Document document;document.audio={{a.materialAudioId,source.getFileName(),{a}},{b.materialAudioId,source.getFileName(),{b}}};
    check("two_variants_same_path_and_scoped_row_ids_saved",OttoMaterialIO::saveDocument(manifest,document,error));if(!ok)return finish();
    juce::MemoryBlock sourceBefore,manifestBefore;source.loadFileAsData(sourceBefore);manifest.loadFileAsData(manifestBefore);
    setAssetManagerVisible(true);check("actual_asset_manager_registered",assetManager&&assetManager->diagnosticRegisterFolder(root)>=0);if(!assetManager)return finish();
    check("typed_Main_selection_callback_bound",bool(assetManager->onLoadMaterialSelection));
    ProjectData seed;TrackData track;track.id="variant-host";track.pitchAlgorithm=PitchAlgorithm::world;seed.tracks={track};project.resetDocument(seed);project.dispatchPendingMessages();selectedTrackId=track.id;pianoRoll.setFocusedTrack(track.id);audio.setPosition(0);setSize(1320,850);resized();
    const auto beforeRevision=project.revisionNumber();
    assetManager->onLoadMaterialSelection({source,root,b.materialAudioId,{b.materialRegionId}});
    const auto placed=project.snapshot();
    check("typed_callback_inserts_exact_variant_not_same_path_sibling",placed.tracks.size()==1&&placed.tracks[0].clips.size()==1&&placed.tracks[0].clips[0].notes.size()==1
        &&placed.tracks[0].clips[0].notes[0].materialAudioId==b.materialAudioId&&placed.tracks[0].clips[0].notes[0].label==b.name
        &&placed.tracks[0].clips[0].notes[0].materialSourceStartSeconds==b.regionStartSeconds&&placed.tracks[0].clips[0].notes[0].materialSourceEndSeconds==b.regionEndSeconds);
    check("typed_callback_single_music_undo",project.revisionNumber()==beforeRevision+1&&project.canUndo());
    check("typed_analysis_finishes",wait([&]{return pendingNativeAnalyses==0;},180000));
    if(project.snapshot().tracks[0].clips.empty())return finish();const auto boundNote=project.snapshot().tracks[0].clips[0].notes[0].id;
    openSelectedMaterialEditor(true,boundNote);auto* quickWindow=noteOtoWindows.findVisible<OtoWaveformEditorComponent>();
    auto* quick=quickWindow?dynamic_cast<OtoWaveformEditorComponent*>(quickWindow->getContentComponent()):nullptr;
    const auto quickRows=quick?quick->diagnosticMaterialRows():std::vector<SampleRegionSetting>{};
    check("precise_quick_editor_receives_bound_variant_identity",quickRows.size()==1&&quickRows[0].materialAudioId==b.materialAudioId&&quickRows[0].name==b.name);
    if(quickWindow)quickWindow->setVisible(false);noteOtoWindows.removeClosed();
    openSelectedMaterialEditor(false,boundNote);pump();
    bool exactFull=false;
    for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)
        if(auto* window=dynamic_cast<juce::DialogWindow*>(juce::Desktop::getInstance().getComponent(i));window&&window->isVisible())
            if(auto* editor=dynamic_cast<OtoWaveformEditorComponent*>(window->getContentComponent()))
            {const auto rows=editor->diagnosticMaterialRows();if(rows.size()==1&&rows[0].materialAudioId==b.materialAudioId&&rows[0].name==b.name){exactFull=true;window->setVisible(false);}}
    check("full_manager_editor_receives_exact_registration",exactFull);
    project.resetDocument(seed);project.dispatchPendingMessages();selectedTrackId=track.id;pianoRoll.setFocusedTrack(track.id);resized();
    const auto payload=assetManager->pronunciationDragDescription(source,{a.materialRegionId},a.materialAudioId);
    check("actual_drag_payload_contains_variant",payload["audioVariantId"].toString()==a.materialAudioId&&bool(payload["nativeMaterial"]));
    const auto dropPoint=pianoViewport.getPosition()+juce::Point<int>(58,40);juce::DragAndDropTarget::SourceDetails drop(payload,assetManager.get(),dropPoint);
    check("Main_accepts_real_manager_variant_drag",isInterestedInDragSource(drop));itemDropped(drop);
    const auto dragged=project.snapshot();
    check("actual_drop_preserves_explicit_variant_and_shared_row_id",dragged.tracks[0].clips.size()==1&&dragged.tracks[0].clips[0].notes.size()==1
        &&dragged.tracks[0].clips[0].notes[0].materialAudioId==a.materialAudioId&&dragged.tracks[0].clips[0].notes[0].label==a.name);
    check("drag_analysis_finishes",wait([&]{return pendingNativeAnalyses==0;},180000));
    const auto unchanged=project.contentFingerprint();const auto unchangedRevision=project.revisionNumber();
    const auto closeWarnings=[&]{for(int i=0;i<8;++i){auto* modal=juce::Component::getCurrentlyModalComponent();if(!modal)break;modal->exitModalState(0);pump();}};
    assetManager->onLoadMaterialSelection({source,root,"unknown-registration",{a.materialRegionId}});closeWarnings();
    check("unknown_explicit_variant_does_not_fall_back_or_mutate",project.contentFingerprint()==unchanged&&project.revisionNumber()==unchangedRevision);
    importAssetAudio(source,root,1,2,track.id,{},"unknown-registration");closeWarnings();
    check("generic_audio_branch_unknown_variant_never_falls_back",project.contentFingerprint()==unchanged&&project.revisionNumber()==unchangedRevision);
    importMaterialPronunciations(source,manifest,{"removed-row"},2,track.id,{},a.materialAudioId);closeWarnings();
    check("stale_explicit_region_never_selects_sibling",project.contentFingerprint()==unchanged&&project.revisionNumber()==unchangedRevision);
    juce::MemoryBlock sourceAfter,manifestAfter;source.loadFileAsData(sourceAfter);manifest.loadFileAsData(manifestAfter);
    check("handoff_does_not_rewrite_source_or_annotation_variants",sourceBefore==sourceAfter&&manifestBefore==manifestAfter);
    const auto written=root.getChildFile("main-variant-handoff-ui.txt").replaceWithText(log);std::cout<<"variant_handoff_fixture="<<root.getFullPathName()<<std::endl;return ok&&written;
#endif
}
}
